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
        for w in self.writes:
            if len(w) != 3 or not (w[0] is None or (isinstance(w[0], int) and 0 <= w[0] < len(self.args))):
                raise ValueError("call 0x%X: write %r needs a base that is None or an index of its %d args"
                                 % (self.addr, w, len(self.args)))


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
    seen = {}              # addr -> (stop kind or None, transfer (kind, direct target or None) or None, indirect call insn or None)
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
        executed.add(addr)     # includes a stub's entry address: a callee address, not one of F's blocks
        prev, state["prev"] = state["prev"], None
        # arrived by a call or a taken jmp/jcc/loop (record E3 §E3.2); a not-taken jcc falling into it is not
        if prev is not None and addr in callset and (prev[1] is None or prev[1] == addr):
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
                transfer = ("call", None)
                tgt = _direct_target(ins)
                if tgt is None:
                    ind = ins
                elif tgt not in allow and tgt not in callset:
                    kind = ("unmodeled", "call 0x%X from 0x%X" % (tgt, addr))
            elif ins.group(capstone.CS_GRP_JUMP) or ins.mnemonic in ("loop", "loope", "loopne"):
                transfer = ("jump", _direct_target(ins))      # the target: only a taken jcc arrives there
            seen[addr] = (kind, transfer, ind)
        if ind is not None and kind is None:          # an indirect call: its target, as of now
            try:
                tgt = _indirect_target(uc, ind)
            except UcError:                           # an operand address nothing maps: E1's stop, not a fault
                tgt = None
                kind = ("unmodeled", "indirect call at 0x%X" % addr)
            if kind is None and tgt not in allow and tgt not in callset:
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


# The registers whose low part a guard may compare (record E3 §E3.7): cmp al/ax/eax all bound eax.
_FAMILY = {r: fam for fam, rs in {
    "eax": ("al", "ax", "eax"), "ebx": ("bl", "bx", "ebx"), "ecx": ("cl", "cx", "ecx"),
    "edx": ("dl", "dx", "edx"), "esi": ("si", "esi"), "edi": ("di", "edi"), "ebp": ("bp", "ebp")}.items()
    for r in rs}


_HIGH8 = {"ah": "eax", "bh": "ebx", "ch": "ecx", "dh": "edx"}


def _keeps_bound(p, idx, width, clean):
    """Whether instruction `p`, between the `ja` and the jump, leaves the index bounded. Returns the new
    `clean` (the index's bits above the compared width are known zero) or None when `p` may widen it.
    Allowed: `and idx32, imm` with imm inside the width and `movzx idx32, r` from the family at or below
    the width (both clean it); a mov/movzx/movsx/lea/xor/nop that writes a register of another family."""
    if p.mnemonic == "and" and len(p.operands) == 2 and p.operands[0].type == cx86.X86_OP_REG \
            and p.reg_name(p.operands[0].reg) == idx and p.operands[1].type == cx86.X86_OP_IMM \
            and (p.operands[1].imm & 0xFFFFFFFF) & ~((1 << width) - 1) == 0:
        return True
    if p.mnemonic == "movzx" and len(p.operands) == 2 and p.operands[0].type == cx86.X86_OP_REG \
            and p.reg_name(p.operands[0].reg) == idx and p.operands[1].type == cx86.X86_OP_REG \
            and _FAMILY.get(p.reg_name(p.operands[1].reg)) == idx and 8 * p.operands[1].size <= width:
        return True
    if p.mnemonic == "nop":
        return clean
    if p.mnemonic in ("mov", "movzx", "movsx", "lea", "xor") and p.operands \
            and p.operands[0].type == cx86.X86_OP_REG:
        dest = p.reg_name(p.operands[0].reg)
        if _FAMILY.get(dest, _HIGH8.get(dest)) not in (None, idx):
            return clean
    return None


def switch_cases(image, ins, prior):
    """The case targets of a bounded switch `jmp dword ptr [R*4 + T]` (record E3 §E3.7), else None.
    Bounded means: among the (up to five) instructions `prior` that precede it on its own straight
    path, a `cmp r, imm` with r in R's family immediately followed by a `ja`, then only instructions
    that keep R bounded (`_keeps_bound`), R's bits above the compared width cleared when that is
    narrower than 32; the table then holds imm + 1 dwords (imm masked to the compared width).
    Stricter than E2's rule (E2 §E2.2), which checks neither the register, the `ja`'s place nor the
    path between the `ja` and the jump."""
    mem = [op for op in ins.operands if op.type == cx86.X86_OP_MEM]
    if ins.mnemonic != "jmp" or not mem or not mem[0].mem.index or mem[0].mem.scale != 4 or mem[0].mem.base:
        return None
    reg = ins.reg_name(mem[0].mem.index)
    idx = _FAMILY.get(reg)
    for k in range(len(prior) - 1, 0, -1):
        c, j = prior[k - 1], prior[k]
        if not (c.mnemonic == "cmp" and j.mnemonic == "ja" and len(c.operands) == 2
                and c.operands[0].type == cx86.X86_OP_REG and c.operands[1].type == cx86.X86_OP_IMM
                and _FAMILY.get(c.reg_name(c.operands[0].reg)) == idx):
            continue
        width = 8 * c.operands[0].size
        clean = width == 32
        for p in prior[k + 1:]:
            clean = _keeps_bound(p, idx, width, clean)
            if clean is None:
                break
        else:
            if not clean:
                continue
            n = (c.operands[1].imm & ((1 << width) - 1)) + 1
            table = mem[0].mem.disp & 0xFFFFFFFF
            if not image.contains(table, 4 * n):
                return None
            return [int.from_bytes(image.bytes_at(table + 4 * m, 4), "little") for m in range(n)]
    return None


def static_scan(image, entry, max_insns=4000, stop=(), switches=False):
    """`stop`: call-set addresses (record E3 §E3.4); a jump to one is a tail call, not followed.
    `switches`: follow a bounded switch's case targets (switch_cases) instead of flagging it."""
    stop = frozenset(stop)
    insns, leaders, indirect, unresolved = {}, {entry}, [], []
    work, truncated = [entry], False
    while work:
        addr = work.pop()
        path = []                     # the instructions of this straight run, for a switch's guard
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
            path = (path + [ins])[-6:]
            if ins.group(capstone.CS_GRP_RET):
                break
            if ins.group(capstone.CS_GRP_JUMP) or m in ("loop", "loope", "loopne"):
                tgt = _direct_target(ins)
                if tgt is None:
                    cases = switch_cases(image, ins, path[:-1]) if switches else None
                    if cases is None:
                        indirect.append(addr)
                    else:
                        for t in cases:
                            if t not in stop:
                                leaders.add(t)
                                work.append(t)
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
