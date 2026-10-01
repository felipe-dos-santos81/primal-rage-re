#!/usr/bin/env python3
"""Differential-emulation harness, original side (spec 2026-09-30-reverse-completion-design §5).

Runs the ORIGINAL x86 bytes of one function from a flat image snapshot and reports what it did
(final registers, every byte it changed, which instructions it executed). The port side
(port/tests/diff_runner.c) runs the C function from the same snapshot; tools/diff_verify.py
compares the two. Narrow claim, repeated in every record: equivalence on the exercised blocks
and inputs only.

Flat model, confirmed in E1 (record §E.1): PRAGE.EXE is flat 32-bit, operands are plain linear
addresses (rng_next 0x5D7DC reads `mov eax,[0xef6d8]`), so the emulator maps a 64 MB flat space
with the image at its own addresses and no segment base.
"""
from dataclasses import dataclass, field

try:  # the harness skips cleanly (spec §5.5) when capstone is not installed
    import capstone
    from capstone import x86 as cx86
except ImportError:  # pragma: no cover - exercised only on a host without capstone
    capstone = None

try:  # ... or when unicorn is not installed
    import unicorn
    from unicorn import (Uc, UcError, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE,
                         UC_HOOK_MEM_READ, UC_HOOK_MEM_WRITE, UC_PROT_ALL)
    from unicorn import x86_const as ux
except ImportError:  # pragma: no cover - exercised only on a host without unicorn
    unicorn = None

MEM_SIZE = 0x4000000          # = port/src/mem.h MEM_SIZE
IMAGE_BASE = 0x10000          # = CODE_BASE; the dump starts here
# The private emulator stack lives OUTSIDE [0, MEM_SIZE) so that every write the original makes
# inside the port's mem[] range is diffed; only this range is excluded from the diff (the port
# has no stack of its own to compare).
STACK_LOW = 0x7FF00000
STACK_END = 0x80000000
STACK_TOP = 0x7FFF0000
SENTINEL = 0x7FFFF000         # the return address pushed for the function; emulation ends here
DEFAULT_MAX_INSNS = 200_000
REGS = ("eax", "ebx", "ecx", "edx", "esi", "edi", "ebp")
# The dwords above the return address at entry: the stack arguments a `ret N` function pops
# (record E3 §E3.5). A case sets them as regs["s0"]..regs["s3"]; a call record reads them the same way.
STACK_ARGS = ("s0", "s1", "s2", "s3")

# Instructions the harness cannot model faithfully: the function becomes NOT_EXERCISABLE with the
# instruction named (spec §5.2), never a guessed result.
UNMODELED_MNEMONICS = frozenset({
    "int", "int3", "into", "in", "out", "ins", "insb", "insw", "insd", "outs", "outsb", "outsw",
    "outsd", "hlt", "cli", "sti", "syscall", "sysenter", "iret", "iretd",
})


def available():
    return unicorn is not None and capstone is not None


class Image:
    """The flat snapshot: raw bytes of the linear range [base, base+len)."""

    def __init__(self, data, base=IMAGE_BASE):
        self.data = bytes(data)
        self.base = base

    @classmethod
    def load(cls, path):
        with open(path, "rb") as f:
            return cls(f.read())

    @property
    def end(self):
        return self.base + len(self.data)

    def contains(self, addr, n=1):
        return self.base <= addr and addr + n <= self.end

    def bytes_at(self, addr, n):
        off = addr - self.base
        return self.data[off:off + n]


@dataclass(frozen=True)
class Call:
    """A callee in the call set (spec §5.2, record E3 §E3.4). Each arrival at `addr` by a `call` or
    a `jmp` is recorded as (addr, the values of `args`), `args` naming registers or stack slots in
    the order the port's C signature passes them. mode "stub": the callee's bytes do not run; its
    `writes` are applied ((base, offset, bytes): base None is absolute, an int is that argument's
    value), EAX becomes `eax`, and the callee returns, popping `pop` bytes of stack arguments (its
    own `ret N`). mode "real": the bytes run (the callee is proven by its own check)."""
    addr: int
    args: tuple = ()
    mode: str = "stub"
    eax: int = 0
    writes: tuple = ()
    pop: int = 0

    def __post_init__(self):
        if self.mode not in ("stub", "real"):
            raise ValueError("call 0x%X: mode %r is neither stub nor real" % (self.addr, self.mode))
        bad = [a for a in self.args if a not in REGS + STACK_ARGS]
        if bad:
            raise ValueError("call 0x%X: unknown argument %s" % (self.addr, ", ".join(bad)))
        if self.mode == "real" and (self.writes or self.eax or self.pop):
            raise ValueError("call 0x%X: a real call declares no effect" % self.addr)


@dataclass
class OrigResult:
    outcome: str                 # ok | fault | unmodeled | timeout
    detail: str = ""
    regs: dict = field(default_factory=dict)       # final value of each REGS entry
    writes: dict = field(default_factory=dict)     # addr -> final byte, only bytes that changed, private stack excluded
    executed: set = field(default_factory=set)     # addresses of executed instructions
    outside: list = field(default_factory=list)    # (addr, size) read or written outside image and stack
    calls: list = field(default_factory=list)      # (addr, args tuple) per arrival at a call-set address, in order


if capstone is not None:
    _md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    _md.detail = True

if unicorn is not None:   # capstone's register names -> unicorn's ids, for an indirect call's operand
    _UC_REG = {n: getattr(ux, "UC_X86_REG_" + n.upper())
               for n in ("eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "esp")}


def _decode(data, addr):
    for ins in _md.disasm(bytes(data), addr, 1):
        return ins
    return None


def _direct_target(ins):
    """The immediate target of a direct jmp/jcc/call, else None."""
    if ins.operands and ins.operands[0].type == cx86.X86_OP_IMM:
        return ins.operands[0].imm & 0xFFFFFFFF
    return None


def _indirect_target(uc, ins):
    """The target of an indirect call, read from the registers and memory as they are now."""
    op = ins.operands[0]
    if op.type == cx86.X86_OP_REG:
        return uc.reg_read(_UC_REG[ins.reg_name(op.reg)])
    m = op.mem
    ea = m.disp
    if m.base:
        ea += uc.reg_read(_UC_REG[ins.reg_name(m.base)])
    if m.index:
        ea += uc.reg_read(_UC_REG[ins.reg_name(m.index)]) * m.scale
    return int.from_bytes(uc.mem_read(ea & 0xFFFFFFFF, 4), "little")


def run_original(image, entry, regs=None, pokes=None, allow_calls=(), max_insns=DEFAULT_MAX_INSNS,
                 calls=()):
    """Run the original function at `entry` once from `image` with `regs` and `pokes` applied.

    regs: {"eax": v, ...}; pokes: {addr: bytes}, applied inside the image before the run (the
    port side applies them to the same bytes, so both sides start from identical memory).
    allow_calls: call targets the emulator may enter, unrecorded; any other call stops the run as
    `unmodeled` (spec §5.2: the closure the port also runs).
    calls: Call entries (record E3 §E3.4): their targets may be called, directly or indirectly,
    and every arrival is recorded in `OrigResult.calls`. With no calls and no allow-list entry for
    it, an indirect call stops the run as E1's "indirect call at" (E1 §E.3).
    """
    if not available():
        raise RuntimeError("unicorn or capstone is not installed (pip install -r tools/requirements-diff.txt)")
    mu = Uc(UC_ARCH_X86, UC_MODE_32)
    mu.mem_map(0, MEM_SIZE, UC_PROT_ALL)
    mu.mem_map(STACK_LOW, STACK_END - STACK_LOW, UC_PROT_ALL)
    mu.mem_write(image.base, image.data)
    for addr, data in (pokes or {}).items():
        if not image.contains(addr, len(data)):
            raise ValueError("poke 0x%X+%d is outside the image" % (addr, len(data)))
        mu.mem_write(addr, bytes(data))

    names = {"eax": ux.UC_X86_REG_EAX, "ebx": ux.UC_X86_REG_EBX, "ecx": ux.UC_X86_REG_ECX,
             "edx": ux.UC_X86_REG_EDX, "esi": ux.UC_X86_REG_ESI, "edi": ux.UC_X86_REG_EDI,
             "ebp": ux.UC_X86_REG_EBP}
    for r in REGS:
        mu.reg_write(names[r], 0)
    esp = STACK_TOP - 4
    for r, v in (regs or {}).items():
        if r in STACK_ARGS:
            mu.mem_write(esp + 4 + 4 * STACK_ARGS.index(r), (v & 0xFFFFFFFF).to_bytes(4, "little"))
        else:
            mu.reg_write(names[r], v & 0xFFFFFFFF)
    mu.mem_write(esp, SENTINEL.to_bytes(4, "little"))
    mu.reg_write(ux.UC_X86_REG_ESP, esp)

    state = {"stop": None, "prev": None}
    executed = set()
    before = {}            # addr -> byte before the run's first write to it
    outside = set()
    seen = {}              # addr -> (stop kind or None, transfer "call"/"jmp"/None, indirect call insn or None)
    allow = frozenset(allow_calls)
    callset = {c.addr: c for c in calls}
    recorded = []

    def arg(uc, esp, a):
        if a in STACK_ARGS:
            return int.from_bytes(uc.mem_read(esp + 4 + 4 * STACK_ARGS.index(a), 4), "little")
        return uc.reg_read(names[a])

    def arrive(uc, addr, c):
        """Record the arrival at a call-set address; for a stub, apply its effect and return."""
        esp = uc.reg_read(ux.UC_X86_REG_ESP)
        vals = tuple(arg(uc, esp, a) for a in c.args)
        recorded.append((addr, vals))
        if c.mode == "real":
            return
        for base, off, data in c.writes:
            at = (off if base is None else vals[base] + off) & 0xFFFFFFFF
            if not image.contains(at, len(data)):
                raise ValueError("stub 0x%X writes 0x%X+%d, outside the image" % (addr, at, len(data)))
            for i, b in enumerate(uc.mem_read(at, len(data))):
                before.setdefault(at + i, b)
            uc.mem_write(at, bytes(data))
        uc.reg_write(ux.UC_X86_REG_EAX, c.eax & 0xFFFFFFFF)
        ret = int.from_bytes(uc.mem_read(esp, 4), "little")
        uc.reg_write(ux.UC_X86_REG_ESP, esp + 4 + c.pop)
        uc.reg_write(ux.UC_X86_REG_EIP, ret)

    def in_scope(addr):
        return image.contains(addr) or STACK_LOW <= addr < STACK_END

    def on_code(uc, addr, size, _):
        executed.add(addr)
        prev, state["prev"] = state["prev"], None
        if prev is not None and addr in callset:      # arrived by a call or a jmp (record E3 §E3.2)
            try:
                arrive(uc, addr, callset[addr])
            except ValueError as e:
                state["stop"] = ("unmodeled", str(e))
                uc.emu_stop()
                return
            if callset[addr].mode == "stub":
                return
        if addr in seen:
            kind, transfer, ind = seen[addr]
        else:
            ins = _decode(uc.mem_read(addr, 15), addr)
            kind, transfer, ind = None, None, None
            if ins is None:
                kind = ("unmodeled", "undecodable bytes at 0x%X" % addr)
            elif ins.mnemonic in UNMODELED_MNEMONICS:
                kind = ("unmodeled", "%s at 0x%X" % (ins.mnemonic, addr))
            elif ins.mnemonic == "call":
                transfer = "call"
                tgt = _direct_target(ins)
                if tgt is None:
                    ind = ins
                elif tgt not in allow and tgt not in callset:
                    kind = ("unmodeled", "call 0x%X from 0x%X" % (tgt, addr))
            elif ins.mnemonic == "jmp":
                transfer = "jmp"
            seen[addr] = (kind, transfer, ind)
        if ind is not None and kind is None:          # an indirect call: its target, as of now
            tgt = _indirect_target(uc, ind)
            if tgt not in allow and tgt not in callset:
                kind = ("unmodeled", "indirect call at 0x%X" % addr if not callset and not allow
                        else "indirect call to 0x%X at 0x%X" % (tgt, addr))
        if kind is not None and state["stop"] is None:
            state["stop"] = kind
            uc.emu_stop()
            return
        state["prev"] = transfer

    def on_write(uc, access, addr, size, value, _):
        for a in range(addr, addr + size):
            if a not in before:
                before[a] = uc.mem_read(a, 1)[0]
        if not in_scope(addr):
            outside.add((addr, size))

    def on_read(uc, access, addr, size, value, _):
        if not in_scope(addr):
            outside.add((addr, size))

    mu.hook_add(UC_HOOK_CODE, on_code)
    mu.hook_add(UC_HOOK_MEM_WRITE, on_write)
    mu.hook_add(UC_HOOK_MEM_READ, on_read)

    outcome, detail = "ok", ""
    try:
        mu.emu_start(entry, SENTINEL, count=max_insns)
        if state["stop"] is not None:
            outcome, detail = state["stop"]
        elif mu.reg_read(ux.UC_X86_REG_EIP) != SENTINEL:
            outcome, detail = "timeout", "no return within %d instructions" % max_insns
    except UcError as e:
        outcome, detail = "fault", str(e)

    final_regs = {r: mu.reg_read(names[r]) for r in REGS}
    writes = {}
    for a, old in before.items():
        if STACK_LOW <= a < STACK_END:
            continue
        new = mu.mem_read(a, 1)[0]
        if new != old:
            writes[a] = new
    return OrigResult(outcome, detail, final_regs, writes, executed, sorted(outside), recorded)


# ---- static scan: the function's basic blocks, for the coverage claim (spec §5.3) -------------

@dataclass
class StaticInfo:
    leaders: list                # sorted block-leader addresses
    insns: dict                  # addr -> size, every instruction reached by recursive descent
    indirect: list               # addrs of indirect jmp/call: their targets are unknown (a jump table)
    unresolved: list             # direct targets outside the image
    truncated: bool              # the scan did not cover all reachable bytes (instruction budget or undecodable bytes)


def static_scan(image, entry, max_insns=4000, stop=()):
    """`stop`: call-set addresses (record E3 §E3.4); a jump to one is a tail call, not followed."""
    stop = frozenset(stop)
    insns, leaders, indirect, unresolved = {}, {entry}, [], []
    work, truncated = [entry], False
    while work:
        addr = work.pop()
        while True:
            if addr in insns:
                break
            if not image.contains(addr):
                unresolved.append(addr)
                break
            if len(insns) >= max_insns:
                truncated = True
                work.clear()
                break
            ins = _decode(image.bytes_at(addr, 15), addr)
            if ins is None:
                truncated = True
                break
            insns[addr] = ins.size
            nxt = addr + ins.size
            m = ins.mnemonic
            if ins.group(capstone.CS_GRP_RET):
                break
            if ins.group(capstone.CS_GRP_JUMP) or m in ("loop", "loope", "loopne"):
                tgt = _direct_target(ins)
                if tgt is None:
                    indirect.append(addr)
                    break
                if tgt not in stop:
                    leaders.add(tgt)
                    work.append(tgt)
                if m == "jmp":
                    break
                leaders.add(nxt)
                addr = nxt
                continue
            if ins.group(capstone.CS_GRP_CALL) and _direct_target(ins) is None:
                indirect.append(addr)
            addr = nxt
    return StaticInfo(sorted(a for a in leaders if a in insns), insns, sorted(set(indirect)),
                      sorted(set(unresolved)), truncated)


def coverage(info, executed):
    """(hit, unhit) block leaders: a block is hit when its leader instruction executed."""
    hit = [a for a in info.leaders if a in executed]
    unhit = [a for a in info.leaders if a not in executed]
    return hit, unhit


# ---- decoding helpers for the static tools (E2 tools/entry_triage.py); additive, used by nothing above --

def decode_at(image, addr):
    """The one instruction at `addr` (a capstone instruction with details), or None if undecodable."""
    return _decode(image.bytes_at(addr, 15), addr)


def disasm_range(image, start, end):
    """Linear decode of [start, end): the capstone instructions, stopping at the first undecodable byte."""
    return list(_md.disasm(image.bytes_at(start, end - start), start))
