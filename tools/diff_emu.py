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


def deref_arg(a):
    """(reg, offset) for a Call argument `[reg]` or `[reg+N]` (N decimal): the dword at that address when
    the callee is reached, for a pointer argument with no mem[] offset to compare (a caller's stack buffer;
    record 2026-10-02-reverse-p2 §P2.7). None for any other argument."""
    if not (a.startswith("[") and a.endswith("]")):
        return None
    reg, plus, off = a[1:-1].partition("+")
    if reg not in REGS or (plus and not (off.isascii() and off.isdigit())):
        return None
    return reg, int(off or "0")
# The dwords above the return address at entry: the stack arguments a `ret N` function pops
# (record E3 §E3.5). A case sets them as regs["s0"]..regs["s3"]; a call record reads them the same way.
STACK_ARGS = ("s0", "s1", "s2", "s3")
# The value a stub leaves in each register its callee does not preserve (Call.clobbers, record E3
# §E3.12): an original caller that reads one after the call computes with this value, which the
# port's C (it cannot read a callee's registers) does not reproduce, so the row turns MISMATCH.
CLOBBER_POISON = 0xC10BBE2D

# Instructions the harness cannot model faithfully: the function becomes NOT_EXERCISABLE with the
# instruction named (spec §5.2), never a guessed result.
UNMODELED_MNEMONICS = frozenset({
    "int", "int3", "into", "in", "out", "ins", "insb", "insw", "insd", "outs", "outsb", "outsw",
    "outsd", "hlt", "cli", "sti", "syscall", "sysenter", "iret", "iretd",
})

# Runtime routines that load a segment register (track D2 batch d2e, record 2026-10-11-reverse-d2e
# §2.2): the WATCOM strlen 0x65624 runs `push es; mov eax,ds; mov es,eax ... pop es`. Unicorn starts
# with no descriptor tables, and a segment load there recomputes the stack-size flag from an empty
# SS cache: every later push/pop goes through a 16-bit SS:SP (the `ret` at 0x6563C pops address 0).
# A run whose allow-list or real call set names one of these gets the flat 32-bit segments DOS/4GW
# gives the game (one code and one data descriptor over the whole address space). Opt-in by
# address, so no other row's emulator state changes.
FLAT_SEGMENT_CALLEES = frozenset({0x65624})
FLAT_GDT = STACK_LOW          # the two descriptors, at the bottom of the private stack mapping
# D3 d3a (record 2026-10-12-reverse-d3a §2.2): the runtime's formatter core 0x6C8EB, which sprintf 0x65546
# reaches, saves and loads ES (0x6C8ED `push es`, 0x6C94E `les edx,[ecx-8]`, 0x6CA21 `mov es,edx`, 0x6CB68
# `pop es`). A row that allows it runs under the same flat descriptors.
FLAT_SEGMENT_CALLEES |= {0x6C8EB}
# D5 d5d (record 2026-10-12-reverse-d5d §2): the runtime memmove 0x653A1 loads ES from DS on both of its
# copy arms (0x653BB `mov edx,ds`, 0x653BD `push es`, 0x653BE `mov es,edx`; the same at 0x653D5..0x653DA) and
# pops it back. Without the flat descriptors its `ret` pops through a 16-bit stack (the run stopped `int3 at
# 0x10000`). No row before this batch names 0x653A1.
FLAT_SEGMENT_CALLEES |= {0x653A1}


def flat_segments(mu):
    """Load a three-entry GDT (null, flat code 0x08, flat data 0x10) and point CS at the code
    descriptor and SS/DS/ES at the data descriptor."""
    def desc(access):
        return bytes([0xFF, 0xFF, 0, 0, 0, access, 0xCF, 0])      # base 0, limit 0xFFFFF pages, 32-bit
    gdt = bytes(8) + desc(0x9B) + desc(0x93)
    mu.mem_write(FLAT_GDT, gdt)
    mu.reg_write(ux.UC_X86_REG_GDTR, (0, FLAT_GDT, len(gdt) - 1, 0))
    for r in (ux.UC_X86_REG_SS, ux.UC_X86_REG_DS, ux.UC_X86_REG_ES):
        mu.reg_write(r, 0x10)
    mu.reg_write(ux.UC_X86_REG_CS, 0x08)


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
    value), EAX becomes `eax`, every register in `clobbers` (the ones the callee does not preserve,
    derived from its bytes: `callee_clobbers`) becomes CLOBBER_POISON, and the callee returns,
    popping `pop` bytes of stack arguments (its own `ret N`). mode "real": the bytes run (the callee
    is proven by its own check)."""
    addr: int
    args: tuple = ()
    mode: str = "stub"
    eax: int = 0
    writes: tuple = ()
    pop: int = 0
    clobbers: tuple = ()

    def __post_init__(self):
        if self.mode not in ("stub", "real"):
            raise ValueError("call 0x%X: mode %r is neither stub nor real" % (self.addr, self.mode))
        bad = [a for a in self.args if a not in REGS + STACK_ARGS and deref_arg(a) is None]
        if bad:
            raise ValueError("call 0x%X: unknown argument %s" % (self.addr, ", ".join(bad)))
        if self.mode == "real" and (self.writes or self.eax or self.pop or self.clobbers):
            raise ValueError("call 0x%X: a real call declares no effect" % self.addr)
        bad = [r for r in self.clobbers if r not in REGS or r == "eax"]
        if bad:
            raise ValueError("call 0x%X: clobbers %s: a register other than eax (eax is the stub's `eax`)"
                             % (self.addr, ", ".join(bad)))
        for w in self.writes:
            if len(w) != 3 or not (w[0] is None or (isinstance(w[0], int) and 0 <= w[0] < len(self.args))):
                raise ValueError("call 0x%X: write %r needs a base that is None or an index of its %d args"
                                 % (self.addr, w, len(self.args)))
            if w[0] is not None and deref_arg(self.args[w[0]]) is not None:
                raise ValueError("call 0x%X: write %r is based on %s, a dereferenced value, not a pointer"
                                 % (self.addr, w, self.args[w[0]]))


APERTURE = (0xA0000, 0xAFA00)     # the 320x200 bytes the game writes at the literal 0xA0000 (gfx.c, the aperture rule)
# The bits a device keeps of a byte written to its port: the VGA DAC data register 0x3C9 holds six bits
# per channel (mode 13h, the game's mode; the port's gfx_dac comment), so a write is recorded as what the
# DAC latched. Every other port is recorded as written.
DEVICE_BITS = {0x3C9: 0x3F}


@dataclass(frozen=True)
class Machine:
    """The machine model a case opts into (track H, record 2026-10-14-reverse-h §H.1). With none, an
    `int`, `in` or `out` stops the run as `unmodeled`, as before. Each part is opt-in on its own:

    keys: the BIOS key queue, AX words in order, or None (int 0x16 stays unmodeled). `int 0x16` with
      AH = 0 removes the first word into AX (an empty queue stops the run: the BIOS would wait); AH = 1
      sets ZF = 1 when the queue is empty, else ZF = 0 and AX = the first word, which stays queued. AH
      only (the game's six sites issue 0 and 1; any other AH stops the run). EAX bits 16+, the other
      registers and the other flags are kept: every site reads AX and, after AH = 1, ZF alone.
      OrigResult.keys_left = the words left.
    ins: ((port, (byte, ...)), ...): `in al,dx` / `in al,imm8` from a listed port returns its bytes in
      order; past the last, or from a port not listed, the run stops (`unmodeled`).
    outs: the ports whose `out dx,al` / `out imm8,al` are recorded, in order, in OrigResult.outs as
      (port, AL & DEVICE_BITS); an `out` to another port stops the run.
    aperture: the bytes changed in APERTURE are the screen's, not mem[]'s: they leave OrigResult.writes
      and the call memory for OrigResult.aperture. A case seeds the screen by poking that range.
    Only the byte forms of in/out are modelled; the word and dword forms stay unmodeled."""
    keys: tuple = None
    ins: tuple = ()
    outs: tuple = ()
    aperture: bool = False

    def __post_init__(self):
        if self.keys is not None and not all(0 <= k <= 0xFFFF for k in self.keys):
            raise ValueError("machine: a key word is not 16 bits: %r" % (self.keys,))
        for port, vals in self.ins:
            if not 0 <= port <= 0xFFFF or not vals or not all(0 <= v <= 0xFF for v in vals):
                raise ValueError("machine: in from 0x%X needs one or more byte values" % port)
        if not all(0 <= p <= 0xFFFF for p in self.outs):
            raise ValueError("machine: an out port is not 16 bits: %r" % (self.outs,))


class _Devices:
    """One run's machine state; step() models one int/in/out (EIP not yet moved) and returns None, or
    the (outcome, detail) the run stops with."""

    def __init__(self, m):
        self.m, self.outs = m, []
        self.keys = None if m.keys is None else list(m.keys)
        self.ins = {p: list(v) for p, v in m.ins}

    def step(self, uc, ins, addr):
        op = ins.bytes[0]
        stop = ("unmodeled", "%s at 0x%X" % (ins.mnemonic, addr))
        if op == 0xCD:                                    # int imm8
            if ins.bytes[1] != 0x16 or self.keys is None:
                return stop
            eax = uc.reg_read(ux.UC_X86_REG_EAX)
            ah = (eax >> 8) & 0xFF
            if ah == 0 and not self.keys:
                return ("unmodeled", "int 0x16 AH=0 at 0x%X: the key queue is empty" % addr)
            if ah not in (0, 1):
                return ("unmodeled", "int 0x16 AH=0x%X at 0x%X" % (ah, addr))
            if ah == 1:
                fl = uc.reg_read(ux.UC_X86_REG_EFLAGS)
                uc.reg_write(ux.UC_X86_REG_EFLAGS, fl & ~0x40 if self.keys else fl | 0x40)
            if self.keys:
                uc.reg_write(ux.UC_X86_REG_EAX, (eax & 0xFFFF0000) | (self.keys.pop(0) if ah == 0 else self.keys[0]))
        elif op in (0xE4, 0xEC):                          # in al,imm8 / in al,dx
            port = ins.bytes[1] if op == 0xE4 else uc.reg_read(ux.UC_X86_REG_EDX) & 0xFFFF
            if not self.ins.get(port):
                return stop if port not in self.ins else (
                    "unmodeled", "in from 0x%X at 0x%X: its values are spent" % (port, addr))
            eax = uc.reg_read(ux.UC_X86_REG_EAX)
            uc.reg_write(ux.UC_X86_REG_EAX, (eax & ~0xFF) | self.ins[port].pop(0))
        elif op in (0xE6, 0xEE):                          # out imm8,al / out dx,al
            port = ins.bytes[1] if op == 0xE6 else uc.reg_read(ux.UC_X86_REG_EDX) & 0xFFFF
            if port not in self.m.outs:
                return stop
            self.outs.append((port, uc.reg_read(ux.UC_X86_REG_EAX) & DEVICE_BITS.get(port, 0xFF)))
        else:
            return stop
        uc.reg_write(ux.UC_X86_REG_EIP, addr + ins.size)
        return None


@dataclass
class OrigResult:
    outcome: str                 # ok | fault | unmodeled | timeout
    detail: str = ""
    regs: dict = field(default_factory=dict)       # final value of each REGS entry
    writes: dict = field(default_factory=dict)     # addr -> final byte, only bytes that changed, private stack excluded
    executed: set = field(default_factory=set)     # addresses of executed instructions
    outside: list = field(default_factory=list)    # (addr, size) read or written outside image and stack
    calls: list = field(default_factory=list)      # (addr, args tuple) per arrival at a call-set address, in order
    call_mem: list = field(default_factory=list)   # per arrival: {addr: byte} changed since the start, private stack excluded
    outs: list = field(default_factory=list)       # Machine.outs: (port, value) per recorded `out`, in order
    keys_left: int = None                          # Machine.keys: the words left in the queue (None: no key model)
    aperture: dict = field(default_factory=dict)   # Machine.aperture: addr -> final byte of each changed screen byte


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
                 calls=(), machine=None):
    """Run the original function at `entry` once from `image` with `regs` and `pokes` applied.

    regs: {"eax": v, ...}; pokes: {addr: bytes}, applied inside the image before the run (the
    port side applies them to the same bytes, so both sides start from identical memory).
    allow_calls: call targets the emulator may enter, unrecorded; any other call stops the run as
    `unmodeled` (spec §5.2: the closure the port also runs).
    calls: Call entries (record E3 §E3.4): their targets may be called, directly or indirectly,
    and every arrival is recorded in `OrigResult.calls`. With no calls and no allow-list entry for
    it, an indirect call stops the run as E1's "indirect call at" (E1 §E.3).
    machine: a Machine (track H §H.1), or None: the int/in/out instructions stop the run as before.
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
    # The game's x87 control word, not unicorn's reset 0x0000 (24-bit precision, every exception unmasked):
    # the init-table entry at 0xF09F0 (routine dword at 0xF09F2 = 0x6B98F) calls 0x6B954 -> 0x72A66, whose
    # `fldcw` at 0x72A83 loads the word at 0xF09B4 = 0x127F (53-bit precision, round to nearest, every
    # exception masked); 0x6B70D loads the same word (record 2026-10-03-reverse-p3 §P3.6).
    mu.reg_write(ux.UC_X86_REG_FPCW, 0x127F)
    if FLAT_SEGMENT_CALLEES & (set(allow_calls) | {c.addr for c in calls if c.mode == "real"}):
        flat_segments(mu)

    state = {"stop": None, "prev": None}
    executed = set()
    before = {}            # addr -> byte before the run's first write to it
    outside = set()
    seen = {}              # addr -> (stop kind or None, transfer (kind, direct target or None) or None, indirect call insn or None)
    allow = frozenset(allow_calls)
    callset = {c.addr: c for c in calls}
    recorded, recorded_mem = [], []
    dev = _Devices(machine) if machine is not None else None
    screen = (lambda a: APERTURE[0] <= a < APERTURE[1]) if machine is not None and machine.aperture else (lambda a: False)

    def arg(uc, esp, a):
        if a in STACK_ARGS:
            return int.from_bytes(uc.mem_read(esp + 4 + 4 * STACK_ARGS.index(a), 4), "little")
        d = deref_arg(a)
        if d is not None:
            return int.from_bytes(uc.mem_read((uc.reg_read(names[d[0]]) + d[1]) & 0xFFFFFFFF, 4), "little")
        return uc.reg_read(names[a])

    def changed(uc):
        """{addr: byte} of every byte that differs from the case's start (pokes applied), private
        stack excluded: what the port's seam hook reports from mem[] against its copy (§E3.12)."""
        out = {}
        for a, old in before.items():
            if not STACK_LOW <= a < STACK_END and not screen(a):
                new = uc.mem_read(a, 1)[0]
                if new != old:
                    out[a] = new
        return out

    def arrive(uc, addr, c):
        """Record the arrival at a call-set address with the memory changed so far; for a stub,
        apply its effect and return."""
        esp = uc.reg_read(ux.UC_X86_REG_ESP)
        vals = tuple(arg(uc, esp, a) for a in c.args)
        recorded.append((addr, vals))
        recorded_mem.append(changed(uc))
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
        for r in c.clobbers:
            uc.reg_write(names[r], CLOBBER_POISON)
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
            elif dev is not None and ins.mnemonic in ("int", "in", "out"):
                kind = ("machine", ins)
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
        if kind is not None and kind[0] == "machine":
            kind = dev.step(uc, kind[1], addr)
            if kind is None:
                return
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
    writes, aperture = {}, {}
    for a, old in before.items():
        if STACK_LOW <= a < STACK_END:
            continue
        new = mu.mem_read(a, 1)[0]
        if new != old:
            (aperture if screen(a) else writes)[a] = new
    return OrigResult(outcome, detail, final_regs, writes, executed, sorted(outside), recorded, recorded_mem,
                      dev.outs if dev else [], None if dev is None or dev.keys is None else len(dev.keys), aperture)


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


def static_scan(image, entry, max_insns=4000, stop=(), switches=False, resolved=None):
    """`stop`: call-set addresses (record E3 §E3.4); a jump to one is a tail call, not followed.
    `switches`: follow a bounded switch's case targets (switch_cases) instead of flagging it.
    `resolved`: {jmp address: (guard, table, n)}, jump tables resolved by hand (RESOLVED_JUMPS): the
    jump's n case targets are read from the table and followed."""
    stop = frozenset(stop)
    resolved = resolved or {}
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
                    if cases is None and addr in resolved:
                        cases = resolved_cases(image, resolved[addr])
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


# ---- the registers a callee does not preserve (record E3 §E3.12), for Call.clobbers -------------

# Jump tables `switch_cases` does not follow (an index moved to another register, or pre-scaled into a
# base, record §E3.7), resolved by hand for the trees of the seamed callees (record §E3.5, §E3.12):
# jmp address -> (the guard `cmp` that bounds the index, the table, its entry count = the cmp's imm + 1).
# The case targets are always read from the table in the image; a test re-checks each guard and table
# from the bytes (test_diff_verify.py, test_each_resolved_jump_table_matches_the_bytes).
RESOLVED_JUMPS = {
    # 0x18350: cmp al,6; ja; and eax,0xff; lea esi,[eax*4]; ...; jmp cs:[esi+0x18334]
    0x18384: (0x18366, 0x18334, 7),
    # 0x29DB8 and 0x29F34: cmp dx,5; ja; xor r,r; mov r16,dx; jmp cs:[r*4+T]
    0x29DFE: (0x29DF3, 0x29D70, 6),
    0x29E62: (0x29E57, 0x29D88, 6),
    0x29EDF: (0x29ED4, 0x29DA0, 6),
    0x29F78: (0x29F6D, 0x29EEC, 6),
    0x29FE5: (0x29FDA, 0x29F04, 6),
    0x2A056: (0x2A04B, 0x29F1C, 6),
    # 0x22638 (record 2026-10-02-reverse-p2 §P2.9): cmp al,7; ja; and eax,0xff; lea edx,[eax*4]; mov eax,[esp];
    # add eax,eax; jmp cs:[edx+0x22618]
    0x227BC: (0x227A3, 0x22618, 8),
    # 0x36300 (record 2026-10-11-reverse-d1b §4): cmp dl,3; ja; xor ecx,ecx; mov cl,dl; jmp cs:[ecx*4+0x362f0]
    0x3631D: (0x36310, 0x362F0, 4),
    # 0x134C0 (record 2026-10-11-reverse-d1c §2): cmp al,5; ja; and eax,0xff; lea esi,[edi+0x14];
    # lea edx,[eax*4]; lea eax,[edi+0x10]; jmp cs:[edx+0x134a8]
    0x1353A: (0x13520, 0x134A8, 6),
    # 0x361C8 (record 2026-10-11-reverse-d2a §4): cmp dl,3; ja; xor ecx,ecx; mov cl,dl; jmp cs:[ecx*4+0x361b8]
    0x361E3: (0x361D6, 0x361B8, 4),
    # 0x4E350 (record 2026-10-11-reverse-d2b §3): both tables index by EDI = 4 * the counter [esp+8], kept in
    # step with it (0x4E36A `xor edi,edi`, 0x4E588 `add edi,4`): cmp eax,9; ja; ...; jmp cs:[edi+0x4e300]
    0x4E426: (0x4E409, 0x4E300, 10),
    # cmp dword [esp+8],9; ja; jmp cs:[edi+0x4e328]
    0x4E4B9: (0x4E4B2, 0x4E328, 10),
    # 0x1FFD0 (record 2026-10-11-reverse-d2c §2): cmp al,8; ja; mov bx,[0x1044c4]; and eax,0xff; shl ebx,9;
    # jmp cs:[eax*4+0x1ffac]
    0x2002E: (0x20017, 0x1FFAC, 9),
    # 0x319B0 (record 2026-10-11-reverse-d2c §2.2): cmp dword [esp+8],3; ja; mov eax,[esp+4]; jmp cs:[eax+0x319a0].
    # The index [esp+4] is 4 * the compared counter [esp+8]: both start 0 (0x319BE, 0x319CB) and step
    # together (0x31A4C add edx,4 / 0x31A4F inc ebx, stored at 0x31A50/0x31A54).
    0x319E0: (0x319D5, 0x319A0, 4),
    # 0x31E28 (record 2026-10-11-reverse-d2d §4): cmp edi,6; ja; lea eax,[edi*4]; mov edx,[esp]; lea edi,[ebp-8];
    # sub edx,2; jmp cs:[eax+0x31e0c]
    0x31EBC: (0x31EA7, 0x31E0C, 7),
    # 0x4505C (record 2026-10-11-reverse-d2d §4; d1g §6 left it for this): cmp dl,4; ja; xor eax,eax; mov al,dl;
    # jmp cs:[eax*4+0x45048]
    0x4507A: (0x45071, 0x45048, 5),
}

# 0x41C28 (track D2 batch d2e, record 2026-10-11-reverse-d2e §2.2): cmp al,8; ja; mov edx,[0x104ad4];
# lea ecx,[edx*8]; add ecx,edx; shl ecx,2; and eax,0xff; add ecx,edx; lea ebx,[eax*4]; shl ecx,2;
# jmp cs:[ebx+0x41c04] (the side's slot offset is built in ECX between the guard and the jump)
RESOLVED_JUMPS[0x41C61] = (0x41C36, 0x41C04, 9)
# 0x208F8 (track D5 batch d5a, record 2026-10-12-reverse-d5a §4): cmp dl,4; ja; xor eax,eax; mov al,dl;
# jmp cs:[eax*4+0x208e4] (the guard compares DL, the index is the EAX the next two instructions build from it)
RESOLVED_JUMPS[0x20931] = (0x20924, 0x208E4, 5)

# 0x4C60C (track D5 batch d5d, record 2026-10-12-reverse-d5d §2): the character byte is saved at [esp]
# (0x4C678 `mov [esp],al`) and read back for the index: cmp al,6; ja; xor eax,eax; mov al,[esp]; shl eax,2;
# jmp cs:[eax+0x4c5f0]
RESOLVED_JUMPS[0x4C687] = (0x4C67B, 0x4C5F0, 7)


def resolved_cases(image, entry):
    """The case targets of a RESOLVED_JUMPS entry, read from the image."""
    _, table, n = entry
    if not image.contains(table, 4 * n):
        return None
    return [int.from_bytes(image.bytes_at(table + 4 * m, 4), "little") for m in range(n)]


def _saved_and_written(image, f, resolved):
    """(saved, written, direct callees) of the function at `f`. saved: the registers its entry
    pushes (the leading `push reg` run) that the pops directly before EVERY `ret` restore (`add
    esp, imm` may sit between them; `leave` restores ebp); nothing when a jump lands inside such a
    pop sequence (it would skip a restore). written: every register family an instruction of its
    static scan writes (capstone's regs_access; push and call excluded); every register when the
    scan is unsure: an indirect call, an indirect jump neither a bounded switch nor in `resolved`,
    a direct target outside the image, or a truncated scan."""
    info = static_scan(image, f, switches=True, resolved=resolved)
    every = set(REGS)
    if info.truncated or info.unresolved:
        return set(), every, set()
    ins = {a: _decode(image.bytes_at(a, 15), a) for a in info.insns}
    entry, a = [], f
    while a in ins and ins[a].mnemonic == "push" and ins[a].operands[0].type == cx86.X86_OP_REG:
        entry.append(ins[a].reg_name(ins[a].operands[0].reg))
        a += ins[a].size
    order = sorted(ins)
    restored, landed = None, False
    targets = set(info.leaders)                   # every jump target (and fall-through) of the scan
    for i, x in enumerate(order):
        if not ins[x].group(capstone.CS_GRP_RET):
            continue
        pops, seq, j = set(), [], i - 1
        while j >= 0 and order[j] + ins[order[j]].size == order[j + 1]:
            p = ins[order[j]]
            if p.mnemonic == "pop" and p.operands[0].type == cx86.X86_OP_REG:
                pops.add(p.reg_name(p.operands[0].reg))
            elif p.mnemonic == "leave":
                pops.add("ebp")
            elif not (p.mnemonic == "add" and p.op_str.startswith("esp,")):
                break
            seq.append(order[j])
            j -= 1
        # a jump into the restore sequence below its first instruction, or onto the ret after it,
        # skips a restore: the function then saves nothing
        if seq and any(a in targets for a in sorted(seq)[1:] + [x]):
            landed = True
        restored = pops if restored is None else restored & pops
    saved = set() if landed else set(entry) & (restored or set())
    written, callees = set(), set()
    for x in info.indirect:
        if ins[x].mnemonic != "call":             # an indirect jump nothing resolved: its targets are unknown
            written |= every
    for x in order:
        p = ins[x]
        if p.mnemonic == "call":
            t = _direct_target(p)
            if t is None:
                written |= every
            else:
                callees.add(t)
            continue
        if p.mnemonic == "push":
            continue
        for r in p.regs_access()[1]:
            fam = _FAMILY.get(p.reg_name(r), _HIGH8.get(p.reg_name(r)))
            if fam in every:
                written.add(fam)
    return saved, written, callees


def callee_clobbers(image, addr, resolved=None):
    """The registers other than EAX that the function at `addr` may change on some path, its whole
    direct-call tree included (a least fixpoint, so recursion is handled): written minus saved,
    per function, plus what its callees clobber that it does not save. A callee outside the image
    clobbers everything. Over-approximating is the safe direction: a poisoned register a caller
    really relies on turns its row MISMATCH (fail-closed), never VERIFIED. `resolved` defaults to
    RESOLVED_JUMPS; {} gives the result with no hand-resolved jump table."""
    resolved = RESOLVED_JUMPS if resolved is None else resolved
    local, work = {}, [addr]
    while work:
        f = work.pop()
        if f in local:
            continue
        local[f] = _saved_and_written(image, f, resolved) if image.contains(f) else (set(), set(REGS), set())
        work.extend(local[f][2])
    clob = {f: set() for f in local}
    moved = True
    while moved:
        moved = False
        for f, (saved, written, callees) in local.items():
            c = set(written)
            for g in callees:
                c |= clob[g]
            c -= saved
            if c != clob[f]:
                clob[f], moved = c, True
    result = tuple(r for r in REGS if r != "eax" and r in clob[addr])
    # Hand corrections where the transitive scan over-approximates the Watcom callee-saved registers
    # (record §C2b.3): the callers of 0x2B150, 0x2BD44 and 0x3B298 keep ESI/EDI/EBP live across the
    # call (0x2B30D reads ESI before 0x2B150 and 0x2B314 uses it after; 0x3B877/0x3B8C8 read ESI
    # after 0x2BD44; the EDI read before the 0x3B834 call into 0x3AE9C is `mov eax,edi` at 0x3B82E),
    # so those callees preserve them.
    return _CALLEE_CLOBBER_FIXES.get(addr, result)

# C3g: 0x3CF38's body pushes and pops ESI only (0x3CF3B/0x3CFD1, 0x3CFFE) and never writes
# EDI or EBP, so it preserves them; the scan above over-approximates through the indirect
# reaction callback. The raw caller 0x3BDDC depends on DI across the call (set at 0x3BDFD,
# read at 0x3BF14), which the poison would break. 0x2AD40 pushes only ebx/ecx/esi and clobbers
# edx; its raw callers read EDI (0x2F280 at 0x2F2ED) and EBP (0x2F314 at 0x2F37A) across the
# call, so the caller-observed set is edx.
_CALLEE_CLOBBER_FIXES = {0x2B150: (), 0x2BD44: ("edx",), 0x3B298: ("edx",), 0x3CF38: (),
                         0x2AD40: ("edx",)}
# D3 d3a (record 2026-10-12-reverse-d3a §2): 0x2C060 is `call 0x2caa8; jmp 0x2ca2c`. 0x2CAA8 writes EAX
# only (0x2CAAF `sete al`, 0x2CAB2 `and eax,0xff`) and the tail-jump target saves EDX around its one use
# (0x2CA2C `push edx` ... 0x2CA44 `pop edx`; `ret`), which the scan cannot credit to 0x2C060 (the push is
# not at its entry). The raw caller 0x11F28 reads EDX across the call (0x11F29 `mov edx,eax`, 0x11F39
# `test [edx*4+0x9acbc],eax`).
_CALLEE_CLOBBER_FIXES[0x2C060] = ()
# D3 d3c: 0x29D04 pushes EBX, ECX, EDX (0x29D04..0x29D06) and pops all three before each of its two
# `ret`s (0x29D12..0x29D14; 0x29D5B..0x29D5D), but the second run is followed by `mov eax,eax`
# (0x29D5E) before the `ret` at 0x29D60, which the pops-directly-before-every-ret rule rejects. Its
# raw caller 0x350D0 keeps the side in EBX across the +0x14 hook call (0x3514C; read at 0x35227).
_CALLEE_CLOBBER_FIXES[0x29D04] = ()
# D3 d3c: 0x46C78 saves EBX and EDX and its own instructions write only EAX, EBX and EDX; its callees
# 0x33950 and 0x46BBC clobber EDX alone. The scan's (ecx, esi, edi, ebp) comes only from its direct
# target 0x62003, the runtime's fatal exit (>= 0x5D000, taken to clobber everything), which does not
# return. Its raw caller 0x46F4C loads EBP before the call (0x46F82..0x46FD9; call 0x46FE3) and reads
# it after (0x47084, 0x470C4).
_CALLEE_CLOBBER_FIXES[0x46C78] = ()
# D3 d3d (record 2026-10-12-reverse-d3d section 4): 0x36870's own body pushes and pops EBX, ECX and
# EDX and writes no other register; the scan inherits esi/edi/ebp from 0x379C4, whose indirect hook
# call (`call [0x1078e8]` 0x379E8) makes it assume every register written. The raw caller 0x47B04
# keeps EDI live across the call (set at 0x47B0B, read at 0x47BCE and 0x47BEF: `mov byte
# [edi+0x57],3`), so 0x36870 preserves EDI; ESI and EBP stay in the set (no caller shown to read them).
_CALLEE_CLOBBER_FIXES[0x36870] = ("esi", "ebp")
# D5 d5b (record 2026-10-12-reverse-d5b section 1): 0x51E5C tests two node fields first (0x51E5C, 0x51E65:
# no register written; both `je` go to the `ret` at 0x51ED4), then pushes EBX, ECX, EDX, ESI, EDI, EBP
# (0x51E6E..0x51E73) and pops all six (0x51ECE..0x51ED3) around its body and its indirect renderer call
# (`call [edx*4+0x80c8c]` 0x51EBF), which is what makes the scan assume every register written; the pushes
# are not at the entry, so the scan credits none. The raw caller 0x14328 keeps EBP (the camera) and EDI
# (the clip record) live across the call (set 0x1432F/0x14331, read 0x1439C and 0x144E4 for the next node).
_CALLEE_CLOBBER_FIXES[0x51E5C] = ()
# D5 d5f (record 2026-10-12-reverse-d5f section 4): 0x3962C saves EBX (0x3962C `push ebx`; `pop ebx`
# 0x396A1, 0x396A8) and its own instructions write only EAX, EBX and EDX; 0x396AC saves EBX and ECX
# (0x396AC/0x396AD; `pop ecx; pop ebx` 0x39734) and writes only EAX, EBX, ECX and EDX. Their callees are
# 0x33950 (EDX alone) and 0x3AFC4, which saves ECX and writes EAX, ECX and EDX: the scan's
# (esi, edi, ebp) comes only from 0x3AFC4's direct target 0x62003, the runtime's fatal exit (>= 0x5D000,
# taken to clobber everything), which does not return (as 0x46C78 above). The raw caller 0x1975C keeps
# ESI = 2 live across both calls (0x1976D `mov esi,2`; calls 0x1979B, 0x197BF; read at 0x197FD
# `mov eax,esi` and at the second pass's 0x19780 `cmp esi,[eax*4+0x100ad0]`).
_CALLEE_CLOBBER_FIXES[0x3962C] = ("edx",)
_CALLEE_CLOBBER_FIXES[0x396AC] = ("edx",)
# D5 d5f (record section 4): 0x49444 pushes EBX, ECX, EDX and ESI (0x49444..0x49447) and pops them
# before its one `ret` (0x494A0..0x494A3); its own instructions write only EAX, EBX, ECX, EDX and ESI.
# Its callees 0x249B0 and 0x249D0 clobber nothing and 0x2B150 preserves ESI/EDI/EBP (the entry above):
# the scan's (edi, ebp) is 0x2B150's uncorrected set, which the fixpoint uses for an inner callee. The
# raw caller 0x4E67C keeps the next list node in EDI across the call (0x4E730 `mov edi,[ebx]`; call
# 0x4E8EE; 0x4E8F3 `mov ebx,edi`).
_CALLEE_CLOBBER_FIXES[0x49444] = ()
# D5 d5g (record 2026-10-12-reverse-d5g section 2): 0x2EA74 is `xor eax,eax; mov eax,eax` with no `ret`: it
# falls into 0x2EA78, whose own leading `push ebx; push ecx; push edx; push esi` (0x2EA78..0x2EA7B) the
# scan cannot credit to the entry two instructions above it, so it reports every register. Its raw
# callers keep all six live across the call: 0x2FFC4 loads EBX before it (0x2FFE7 `mov ebx,0xfffffffe`)
# and stores it after (0x30017), and reads EDI (0x30086 `mov eax,[edi+8]`), ESI (0x30094 `add edi,esi`)
# and EBP (0x300BB `mov eax,ebp`) after the call at 0x2FFF1; 0x2FA40 loads EDX, EBX and ECX before the
# call at 0x2FA61 (0x2FA4E..0x2FA58) and stores them after (0x2FA72, 0x2FA76, 0x2FA7D).
_CALLEE_CLOBBER_FIXES[0x2EA74] = ()


# ---- decoding helpers for the static tools (E2 tools/entry_triage.py); additive, used by nothing above --

def decode_at(image, addr):
    """The one instruction at `addr` (a capstone instruction with details), or None if undecodable."""
    return _decode(image.bytes_at(addr, 15), addr)


def disasm_range(image, start, end):
    """Linear decode of [start, end): the capstone instructions, stopping at the first undecodable byte."""
    return list(_md.disasm(image.bytes_at(start, end - start), start))

# D5 d5h (record 2026-10-12-reverse-d5h §2): 0x11D04's state switch. cmp ax,9; ja; and eax,0xffff;
# lea edx,[eax*4]; mov ax,[0xf0a6a]; dec eax; jmp cs:[edx+0x11cdc] (the index is pre-scaled into EDX,
# and EAX is reloaded with the timer word between the guard and the jump)
RESOLVED_JUMPS[0x11D69] = (0x11D50, 0x11CDC, 10)

# D6 d6a (record 2026-10-13-reverse-d6a section 6): 0x51ED8 is 0x51E5C's shape. It tests two node fields
# first (0x51ED8, 0x51EE1: no register written; both `je` go to the `ret` at 0x51F44), then pushes EBX,
# ECX, EDX, ESI, EDI, EBP (0x51EEA..0x51EEF) and pops all six (0x51F3E..0x51F43) around its body and its
# indirect renderer call (`call [edx*4+0x80c8c]` 0x51F35); the pushes are not at the entry, so the scan
# credits none. The raw caller 0x1C5E8 keeps ECX (the column's address, 0x1C5ED) and ESI (the width byte,
# 0x1C5FF) live across the call at 0x1C63D (read at 0x1C642 `cmp esi,0x10` and 0x1C647 `add [ecx],0xf`).
_CALLEE_CLOBBER_FIXES[0x51ED8] = ()
# D6 d6a (record 2026-10-13-reverse-d6a section 6): 0x31A78's key switch, 0x319B0's form. cmp edi,3; ja;
# mov eax,[esp+0x10]; jmp cs:[eax+0x31a68]. EDI is the counter [esp+0x18] (loaded at 0x31AB2) and the index
# [esp+0x10] is 4 * it: both start 0 (0x31A96, 0x31AA4) and step together (0x31B65 add eax,4 / 0x31B68
# inc edx, stored at 0x31B69/0x31B6D).
RESOLVED_JUMPS[0x31AD6] = (0x31AC9, 0x31A68, 4)
# D6 d6a (record 2026-10-13-reverse-d6a section 6): 0x1AE20 is `mov eax,0xa2c62; lea eax,[eax]` with no
# `ret`: it falls into 0x1AE28, whose leading `push ebx; push ecx; push edx; push esi; push edi`
# (0x1AE28..0x1AE2C; popped 0x1AED8..0x1AEDC before the `ret`) the scan cannot credit to the entry two
# instructions above it (0x2EA74's case). The raw caller 0x2CADC loads ECX and EBX before the call at
# 0x2CB50 (0x2CB3A `mov ecx,0xb000`, 0x2CB46 `mov ebx,0x809f0`) for the 0x2F280 call at 0x2CB69.
_CALLEE_CLOBBER_FIXES[0x1AE20] = ()
# D6 d6a (record 2026-10-13-reverse-d6a section 6): 0x2CADC pushes and pops EBX, ECX and EDX (0x2CADC..
# 0x2CADE, 0x2CB6E..0x2CB70) and its own instructions write no other register; the scan's (esi, edi, ebp)
# is its callees' (0x2EA78 and the text routines). Its raw caller 0x2D6F8 keeps ESI (the first read's
# result, 0x2D72E) and EBP (the defaults flag, 0x2D82D) live across the call at 0x2D886 (read at 0x2D910
# `mov eax,ebp`, 0x2D917 and 0x2D91E `test esi,esi`), so 0x2CADC preserves them; EDI stays in the set (no
# caller shown to read it).
_CALLEE_CLOBBER_FIXES[0x2CADC] = ("edi",)
# D6 d6a (record 2026-10-13-reverse-d6a section 6): the runtime memcpy 0x655FF loads ES from DS around its
# copy (0x65608 `push es`, 0x65609 `mov eax,ds`, 0x6560B `mov es,eax`, 0x6561D `pop es`), as the memmove
# 0x653A1 does: without the flat descriptors its `ret` pops through a 16-bit stack (the run stopped `int3
# at 0x10000`, measured). No row before this batch names 0x655FF.
FLAT_SEGMENT_CALLEES |= {0x655FF}
