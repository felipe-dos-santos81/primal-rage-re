#!/usr/bin/env python3
"""Differential verification driver (spec 2026-09-30-reverse-completion-design §5).

For each function in SPECS: run the original bytes (tools/diff_emu.py) and the port's C function
(build/diffrun, port/tests/diff_runner.c) from the same image with the same registers and pokes,
and compare the output register, every changed byte, and the block coverage of the original.

Narrow claim, in every verdict: VERIFIED means equivalence on the exercised blocks and inputs
only. It does not mean the function is correct for states the cases never produce.
"""
import argparse
import dataclasses
import os
import subprocess
import sys
import tempfile
from dataclasses import dataclass, field

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import diff_emu as E


def le32(v):
    return (v & 0xFFFFFFFF).to_bytes(4, "little")


@dataclass
class Case:
    id: str
    regs: dict
    pokes: dict = field(default_factory=dict)      # addr -> bytes
    stub_eax: dict = field(default_factory=dict)   # callee addr -> this case's stub EAX (record E3 §E3.12)


@dataclass
class Spec:
    name: str                  # the diffrun binding name
    entry: int                 # the original's entry address
    cases: list
    unhit_named: dict = field(default_factory=dict)   # block leader -> reason it is not exercised
    allow_calls: tuple = ()
    mutants: tuple = ("@mutant",)    # diffrun binding suffixes that must be reported as MISMATCH
    eax_mask: int = 0xFFFFFFFF       # the part of the original's EAX the callers read (full = conservative)
    calls: tuple = ()                # E.Call entries: the callees stubbed or run on both sides, recorded (record E3 §E3.4)
    gap: str = ""                    # a named gap (spec §5.2): the blocking instruction every case must stop on

    def __post_init__(self):
        ids = [c.id for c in self.cases]
        dup = sorted({i for i in ids if ids.count(i) > 1})
        if dup:
            raise ValueError("spec %s: duplicate case id %s" % (self.name, ", ".join(dup)))
        both = sorted(set(self.allow_calls) & {c.addr for c in self.calls})
        if both:
            raise ValueError("spec %s: 0x%X is both allowed and in the call set" % (self.name, both[0]))
        if self.gap and not self.cases:
            raise ValueError("spec %s: a named gap with no cases holds vacuously" % self.name)
        stubs = {k.addr for k in self.calls if k.mode == "stub"}
        for c in self.cases:
            bad = sorted(set(c.stub_eax) - stubs)
            if bad:
                raise ValueError("spec %s case %s: stub_eax names 0x%X, not a stub of the call set"
                                 % (self.name, c.id, bad[0]))


def case_calls(spec, case):
    """The spec's call set with this case's stub EAX values applied: a stub's EAX can vary per case,
    so a port cannot agree by hard-coding the constant a caller reads (record E3 §E3.12)."""
    return tuple(dataclasses.replace(k, eax=case.stub_eax[k.addr]) if k.addr in case.stub_eax else k
                 for k in spec.calls)


@dataclass
class PortResult:
    eax: int = 0
    mask: int = 0xFFFFFFFF
    writes: dict = field(default_factory=dict)
    error: str = ""
    calls: list = field(default_factory=list)      # (addr, args tuple), in the order the seam saw them
    call_mem: list = field(default_factory=list)   # per call: {addr: byte} mem[] changed since the start, at that call


def cases_text(spec, port_name):
    out = []
    for c in spec.cases:
        out.append("case %s" % c.id)
        out.append("fn %s" % port_name)
        for r, v in c.regs.items():
            out.append("reg %s 0x%X" % (r, v))
        for a, b in c.pokes.items():
            out.append("poke 0x%X %s" % (a, bytes(b).hex()))
        for k in case_calls(spec, c):
            out.append("stub 0x%X %s 0x%X" % (k.addr, k.mode, k.eax & 0xFFFFFFFF))
            for base, off, data in k.writes:
                out.append("swrite %s 0x%X %s" % ("abs" if base is None else "arg%d" % base, off, bytes(data).hex()))
        out.append("end")
    return "\n".join(out) + "\n"


def parse_port_output(text):
    """Parse diffrun's output. Strict: a case is complete only with exactly one `ret` or `error`
    line and a closing `end`; anything else (truncation, a repeated line, a duplicate case id, an
    unknown line) is a ValueError, never a default result that could agree with the original."""
    res, cur, got, ended = {}, None, False, False
    for line in text.splitlines():
        t = line.split()
        if not t:
            continue
        if t[0] == "case":
            if len(t) != 2:
                raise ValueError("malformed case line: %r" % line)
            if cur is not None:
                raise ValueError("diffrun output: case %s opened inside an open case" % t[1])
            if t[1] in res:
                raise ValueError("diffrun output: duplicate case id %s" % t[1])
            cur = res[t[1]] = PortResult()
            got, ended = False, False
        elif cur is None:
            raise ValueError("diffrun output outside a case: %r" % line)
        elif t[0] == "ret" and len(t) == 5 and t[1] == "eax" and t[3] == "mask":
            if got:
                raise ValueError("diffrun output: a second result line in one case: %r" % line)
            cur.eax, cur.mask, got = int(t[2], 16), int(t[4], 16), True
        elif t[0] == "c" and len(t) >= 2:
            if got:
                raise ValueError("diffrun output: a call line after the result line: %r" % line)
            cur.calls.append((int(t[1], 16), tuple(int(x, 16) for x in t[2:])))
            cur.call_mem.append({})
        elif t[0] == "m" and len(t) == 3:
            # a byte of mem[] that differs from the case's start at the last call (record E3 §E3.12)
            if got or not cur.calls:
                raise ValueError("diffrun output: a memory line outside a call: %r" % line)
            a = int(t[1], 16)
            if a in cur.call_mem[-1]:
                raise ValueError("diffrun output: byte 0x%X reported twice at one call" % a)
            cur.call_mem[-1][a] = int(t[2], 16)
        elif t[0] == "w" and len(t) == 3:
            a = int(t[1], 16)
            if a in cur.writes:
                raise ValueError("diffrun output: byte 0x%X written twice in one case" % a)
            cur.writes[a] = int(t[2], 16)
        elif t[0] == "error":
            if got:
                raise ValueError("diffrun output: a second result line in one case: %r" % line)
            cur.error, got = " ".join(t[1:]) or "error", True
        elif t[0] == "end" and len(t) == 1:
            if not got:
                raise ValueError("diffrun output: a case ended with no ret or error line")
            cur = None
        else:
            raise ValueError("diffrun output: unrecognised line %r" % line)
    if cur is not None:
        raise ValueError("diffrun output ended inside an open case (truncated?)")
    return res


def run_port(diffrun, exe, image_path, text, timeout=60):
    with tempfile.NamedTemporaryFile("w", suffix=".cases", delete=False) as f:
        f.write(text)
        path = f.name
    try:
        p = subprocess.run([diffrun, "--exe", exe, "--image-out", image_path, "--cases", path],
                           capture_output=True, text=True, timeout=timeout)
    except subprocess.TimeoutExpired:
        raise RuntimeError("diffrun timed out after %ds (a port function did not return)" % timeout)
    finally:
        os.unlink(path)
    if p.returncode != 0:
        raise RuntimeError("diffrun failed (%d): %s" % (p.returncode, p.stderr.strip()))
    return parse_port_output(p.stdout)


def _call_text(c):
    if c is None:
        return "none"
    return "0x%X(%s)" % (c[0], ", ".join("0x%X" % v for v in c[1]))


def _byte_text(v):
    return "unchanged" if v is None else "0x%02X" % v


def compare(orig, port, mask=0xFFFFFFFF):
    """The discrepancies between one original run and one port run (empty = they agree).

    `mask` is the Spec's: the part of the original's EAX that callers read. The port only reports
    the mask its binding uses; a binding that differs from the Spec is itself a discrepancy, so a
    port cannot narrow (or zero) the comparison on its own."""
    out = []
    if port.error:
        out.append("port: " + port.error)
    if port.mask != mask:
        out.append("port reports eax mask 0x%X, the spec states 0x%X" % (port.mask, mask))
    if (orig.regs["eax"] & mask) != port.eax:
        out.append("eax (mask 0x%X): original 0x%X, port 0x%X" % (mask, orig.regs["eax"] & mask, port.eax))
    oc, pc = list(orig.calls), list(port.calls)
    for i in range(max(len(oc), len(pc))):
        o = oc[i] if i < len(oc) else None
        p = pc[i] if i < len(pc) else None
        if o != p:
            out.append("call #%d: original %s, port %s" % (i, _call_text(o), _call_text(p)))
        if o is not None and p is not None:
            # the memory at the call (record E3 §E3.12): a store moved across a stubbed call changes
            # what the callee would read, though the bytes at return may agree
            om = orig.call_mem[i] if i < len(orig.call_mem) else {}
            pm = port.call_mem[i] if i < len(port.call_mem) else {}
            if om != pm:
                a = min(x for x in set(om) | set(pm) if om.get(x) != pm.get(x))
                out.append("call #%d memory: %d bytes changed so far in the original, %d in the port; first "
                           "difference at 0x%X: original %s, port %s"
                           % (i, len(om), len(pm), a, _byte_text(om.get(a)), _byte_text(pm.get(a))))
    for a in sorted(set(orig.writes) | set(port.writes)):
        if orig.writes.get(a) != port.writes.get(a):
            out.append("byte 0x%X: original %s, port %s" % (
                a, _byte_text(orig.writes.get(a)), _byte_text(port.writes.get(a))))
    return out


@dataclass
class SpecResult:
    name: str
    entry: int
    verdict: str                 # VERIFIED | PARTIAL | MISMATCH | NOT_EXERCISABLE
    ncases: int = 0
    hit: int = 0
    total: int = 0
    problems: list = field(default_factory=list)
    unhit: list = field(default_factory=list)       # unhit leaders with no stated reason
    outside: list = field(default_factory=list)
    port_errors: list = field(default_factory=list)  # "<case>: <text>" for each case the port refused
    diffs: int = 0                                   # eax, call and byte differences (not port errors)
    callees: list = field(default_factory=list)      # (addr, how) for every allowed or call-set callee
    gap: str = ""                                    # the named gap's blocking instruction, when the spec is one


def verify_gap(spec, image):
    """A named gap (spec §5.2): every case must stop on exactly the instruction the spec names. The
    port is not run. A case that runs through, or stops elsewhere, is a MISMATCH: the gap is stale
    or misnamed, and the function becomes a verification target again."""
    info = E.static_scan(image, spec.entry, stop=[k.addr for k in spec.calls], switches=True)
    res = SpecResult(spec.name, spec.entry, "NAMED_GAP", len(spec.cases), 0, len(info.leaders), gap=spec.gap)
    executed = set()
    for c in spec.cases:
        orig = E.run_original(image, spec.entry, c.regs, c.pokes, spec.allow_calls, calls=case_calls(spec, c))
        executed |= orig.executed
        if (orig.outcome, orig.detail) != ("unmodeled", spec.gap):
            res.verdict = "MISMATCH"
            res.problems.append("%s: the gap names '%s', the original %s (%s)" % (
                c.id, spec.gap, orig.outcome, orig.detail or "returned"))
    res.hit = len(E.coverage(info, executed)[0])
    return res


def verify_spec(spec, image, port_results, port_name=None):
    """Compare every case of `spec` against `port_results` (case id -> PortResult)."""
    sent, got = [c.id for c in spec.cases], set(port_results)
    if got != set(sent):
        raise ValueError("%s: the port's results do not match the cases sent: missing %s, extra %s"
                         % (port_name or spec.name, sorted(set(sent) - got), sorted(got - set(sent))))
    info = E.static_scan(image, spec.entry, stop=[k.addr for k in spec.calls], switches=True)
    executed, outside, problems, blocked = set(), set(), [], []
    port_errors, diffs = [], 0
    for c in spec.cases:
        orig = E.run_original(image, spec.entry, c.regs, c.pokes, spec.allow_calls, calls=case_calls(spec, c))
        if orig.outcome != "ok":
            blocked.append("%s: %s (%s)" % (c.id, orig.outcome, orig.detail))
            continue
        executed |= orig.executed
        outside |= set(orig.outside)
        port = port_results[c.id]
        if port.error:
            port_errors.append("%s: %s" % (c.id, port.error))
        for d in compare(orig, port, spec.eax_mask):
            problems.append("%s: %s" % (c.id, d))
            if d.startswith(("eax ", "byte ", "call #")):   # the strings compare() emits for a real difference
                diffs += 1
    hit, unhit = E.coverage(info, executed)
    res = SpecResult(port_name or spec.name, spec.entry, "VERIFIED", len(spec.cases), len(hit),
                     len(info.leaders), problems, [a for a in unhit if a not in spec.unhit_named],
                     sorted(outside), port_errors, diffs)
    res.callees = sorted([(a, "allow") for a in spec.allow_calls] + [(k.addr, k.mode) for k in spec.calls])
    # An indirect call hides no block of the function (it returns to the next instruction, which the
    # scan follows), and a run that reached one resolved its target into the allow-list or the call
    # set, or stopped (NOT_EXERCISABLE): only an indirect jump that is not a bounded switch keeps a
    # function PARTIAL (record E3 §E3.7).
    jumps = [a for a in info.indirect if E.decode_at(image, a).mnemonic != "call"]
    if problems:
        res.verdict = "MISMATCH"
    elif blocked:
        res.verdict, res.problems = "NOT_EXERCISABLE", blocked
    elif res.unhit or jumps or info.truncated:
        res.verdict = "PARTIAL"
        if jumps:
            res.problems.append("indirect jmp/call at %s: targets unknown" % ",".join(hex(a) for a in jumps))
        if info.truncated:
            res.problems.append("static scan truncated")
    return res


# ---- the E1 specs: four ported functions, each cased to cover every block ----------------------

DS_RNG = 0xEF6D8
DS_FLAGS = 0x107EE0
DS_CREDITS = 0x105C00
DS_FREEPLAY = 0x105D60
DS_NODEBIT = 0x104B1F
U6_REC, U6_OWNER = 0x10A000, 0x10A100   # inside the image's zero BSS (record gameplay-u6 §U6.6)
DS_1078FC = 0x1078FC         # the byte 0x37DCC stores (record gameplay-u6 §U6.5)

# ---- E3: the call stubs' worked batch (record 2026-10-01-reverse-e3 §E3.6) ----------------------
E3_SLOT, E3_REC, E3_REC2, E3_OUT = 0x10A200, 0x10A300, 0x10A400, 0x10A500   # zero BSS of the image
DS_SLOTS = 0x1077B0            # DS_001077B0: the two 0x94-byte fighter slots; +0 holds the slot's record
# clobbers: the registers each callee does not preserve, E.callee_clobbers over the image (record
# §E3.5's table; a real-image test re-derives them)
VOICE = E.Call(0x2C3FC, ("eax",), eax=1)
ANIM_BEGIN = E.Call(0x2BC30, ("eax", "edx", "s0"), pop=4, clobbers=("edx",))
HIT_B = E.Call(0x3C4CC, ("eax", "edx", "s0"), pop=4, clobbers=("edx",))
HIT_A = E.Call(0x3C480, ("eax", "edx", "s0"), pop=4, clobbers=("edx",))
SPAWN = E.Call(0x2AE14, ("eax", "edx", "ecx", "ebx", "s0"), pop=4, clobbers=("ebx", "ecx", "edx"))
SLOT_PTRS = {DS_SLOTS: le32(E3_REC), DS_SLOTS + 0x94: le32(E3_REC2)}
OUT_SEED = {E3_OUT: b"\xaa" * 24}

E3_SPECS = [
    Spec("fighter_23130", 0x23130, [
        Case("v0", {"eax": E3_SLOT, "edx": E3_REC, "ebx": 0},
             {E3_SLOT + 0x52: b"\x7f\x7f\x7f", E3_SLOT + 0x0C: le32(0xFFFFFFFF)}),
        # the voice stub returns AL = 0 here: 0x23130 ignores it (`mov al,1` at 0x23170)
        Case("v1", {"eax": E3_SLOT, "edx": E3_REC, "ebx": 1},
             {E3_SLOT + 0x52: b"\x7f\x7f\x7f", E3_SLOT + 0x0C: le32(0xFFFFFFFF)}, {0x2C3FC: 0}),
    ], allow_calls=(0x33950,), calls=(HIT_B, VOICE), eax_mask=0xFF,
       mutants=("@voice", "@novoice", "@reorder")),
    Spec("fighter_45878", 0x45878, [
        Case("b0", {"eax": E3_SLOT, "edx": E3_REC},
             {E3_REC + 0x53: b"\x00", E3_REC + 0x59: b"\x00", E3_SLOT + 0x52: b"\x7f\x7f\x7f",
              E3_SLOT + 0x57: b"\x7f", E3_SLOT + 0x0C: le32(0xFFFFFFFF), E3_SLOT + 0x18: le32(0xFFFFFFFF),
              E3_SLOT + 0x1C: le32(0xFFFFFFFF), E3_SLOT + 0x88: b"\xff\xff", E3_SLOT + 0x42: b"\x00"}),
        Case("b1", {"eax": E3_SLOT, "edx": E3_REC},
             {E3_SLOT + 0x42: b"\xfb", E3_SLOT + 0x88: b"\x01\x00"}),
    ], calls=(ANIM_BEGIN,), eax_mask=0),
    Spec("anim_10fa8", 0x10FA8, [
        Case("s0", {}),
        Case("s1", {"eax": E3_REC, "edx": 0x55}),
    ], calls=(SPAWN,), eax_mask=0),
    Spec("anim_3e4e4", 0x3E4E4, [
        Case("e0", {"eax": E3_REC}, {E3_REC + 0x14: le32(0), E3_REC + 0x36: b"\xff\xff", E3_REC + 0x44: b"\xff\xff"}),
        Case("e1", {"eax": E3_REC}, {E3_REC + 0x14: le32(E3_SLOT), E3_REC + 0x36: b"\xff\xff",
                                     E3_REC + 0x44: b"\xff\xff", E3_SLOT + 0x57: b"\x7f"}),
    ], calls=(ANIM_BEGIN,), eax_mask=0),
    Spec("fighter_ctx_same", 0x33950, [
        Case("x0", {"eax": E3_OUT, "edx": 0}, {**OUT_SEED, **SLOT_PTRS}),
        Case("x1", {"eax": E3_OUT, "edx": 1}, {**OUT_SEED, **SLOT_PTRS}),
    ], eax_mask=0),
    Spec("hit_anim_ctx", 0x339AC, [
        Case("y0", {"eax": E3_OUT, "edx": E3_REC}, {**OUT_SEED, **SLOT_PTRS, E3_REC + 0x50: b"\x01\x00"}),
        Case("y1", {"eax": E3_OUT, "edx": E3_REC}, {**OUT_SEED, **SLOT_PTRS, E3_REC + 0x50: b"\x00\x01"}),
    ], eax_mask=0),
    Spec("hit_anim_start_b", 0x3C4CC, [
        Case("h%X" % st, {"eax": E3_REC, "edx": 0xE4872, "s0": 0x40400000},
             {E3_REC + 0x51: bytes([side]), DS_SLOTS + side * 0x94: le32(E3_REC2),
              DS_SLOTS + side * 0x94 + 0x52: bytes([st])})
        # every state byte 0..0xFF (side = its parity), so the dispatch set {0,1,2,5,0xE,0x15} ->
        # 0x2BC30, everything else -> 0x3C480 is pinned value by value, not only its two blocks
        for st, side in [(s, s & 1) for s in range(0x100)]
    ], allow_calls=(0x339AC,), calls=(ANIM_BEGIN, HIT_A), eax_mask=0, mutants=("@mutant", "@set")),
    Spec("host_1b890", 0x1B890, [Case("g0", {})], mutants=(), gap="in at 0x1B899"),
]

# ---- track P batch 1: the finisher entries and their +0x0C callbacks (record 2026-10-02-reverse-p1) --
# A finisher entry runs as 0x379C4 calls it at 0x379E8: EAX = slot, EDX = rec. Mask 0xFF: the raw sets AL = 1
# over its last callee's EAX and 0x379EE's `test eax,eax` is the only read (record §P1.4). Every field the
# function writes is seeded with a sentinel; +0x42, the one it reads (`or ah,8`), takes 0x21 and 0xFF; the
# voice stub returns AL = 1 and 0 (the function overwrites AL with `mov al,1`).
P1_SEED = {E3_SLOT + 0x52: b"\x52\x53\x54", E3_SLOT + 0x57: b"\x57", E3_SLOT + 0x0C: le32(0x0C0C0C0C),
           E3_SLOT + 0x14: le32(0x14141414), E3_SLOT + 0x18: le32(0x18181818), E3_SLOT + 0x1C: le32(0x1C1C1C1C)}


def p1_finisher(name, entry, voice):
    calls = (ANIM_BEGIN, VOICE) if voice else (ANIM_BEGIN,)
    return Spec(name, entry, [
        Case("f0", {"eax": E3_SLOT, "edx": E3_REC}, {**P1_SEED, E3_SLOT + 0x42: b"\x21"}),
        Case("f1", {"eax": E3_SLOT, "edx": E3_REC}, {**P1_SEED, E3_SLOT + 0x42: b"\xff"},
             {0x2C3FC: 0} if voice else {}),
    ], calls=calls, eax_mask=0xFF)


# 0x1A570: AL = 1 when the actor word of the side's record has bit 15 clear (record §P1.4). Its stub EAX is
# the C predicate's 0 or 1 (the AL the 69 callers read).
BIT15 = E.Call(0x1A570, ("eax",))
P1_PSET = 0x10A600            # zero BSS of the image: a pset base for DS_001014EC
DS_PSET_BASE = 0x1014EC
DS_STAGE = 0x104AFC           # DS_00104AFC, the word 0x23BF8 indexes 0xA83C4/0xA83CC by


def p1_bit15(cid, side, idx, word):
    return Case(cid, {"eax": side}, {DS_SLOTS + side * 0x94: le32(E3_REC), E3_REC + 0x56: le32(idx)[:2],
                                     DS_PSET_BASE: le32(P1_PSET), P1_PSET + idx * 0x20: le32(word)[:2]})


def p1_23bf8(cid, stage, al, x, side, extra=None):
    pokes = {**P1_SEED, E3_SLOT + 0x42: bytes([0x21 + side]), DS_STAGE: le32(stage)[:2],
             E3_REC + 0x51: bytes([side]), E3_REC + 0x18: le32(x)}
    pokes.update(extra or {})
    return Case(cid, {"eax": E3_SLOT, "edx": E3_REC}, pokes, {} if al is None else {0x1A570: al})


P1_SPECS = [
    p1_finisher("fighter_1567c", 0x1567C, True),
    p1_finisher("fighter_15908", 0x15908, True),
    p1_finisher("fighter_23ec0", 0x23EC0, True),
    p1_finisher("fighter_45d14", 0x45D14, False),
    Spec("fighter_actor_bit15_clear", 0x1A570, [
        p1_bit15("b0", 0, 3, 0x7FFF), p1_bit15("b1", 1, 5, 0x8000), p1_bit15("b2", 0, 7, 0xFFFF),
        p1_bit15("b3", 1, 2, 0x0000),
    ], eax_mask=0xFF),
    # 0x23BF8 (record §P1.6). The image's own stage tables: flag bytes 01 00 01 00 00 01 at 0xA83C4,
    # thresholds 0x2600 (stage 0), 0x6000 (2), 0x3100 (5) at 0xA83CC. e1 pokes a flag byte zero for the
    # stage word 0x105 so the early return's EAX is 0x100 (e2: 0x205, image byte 0x12, EAX 0x200); a0/a1/a6 put x
    # below, at and above the threshold with AL set, a7/a3/a2 with AL clear; a4/a5 pin the signed compares.
    Spec("fighter_23bf8", 0x23BF8, [
        p1_23bf8("e0", 1, None, 0, 0),
        p1_23bf8("e1", 0x105, None, 0, 1, {0xA83C4 + 0x105: b"\x00"}),
        p1_23bf8("a0", 0, 1, 0x2000, 0),
        p1_23bf8("a1", 0, 1, 0x2600, 1),
        p1_23bf8("a2", 2, 0, 0x6001, 0),
        p1_23bf8("a3", 2, 0, 0x6000, 1),
        p1_23bf8("a4", 5, 1, 0xFFFFF000, 0),
        p1_23bf8("a5", 5, 0, 0xFFFFF000, 1),
        p1_23bf8("a6", 0, 1, 0x2601, 0),
        p1_23bf8("a7", 2, 0, 0x5FFF, 1),
        p1_23bf8("e2", 0x205, None, 0, 0, {0xA83C4 + 0x205: b"\x00"}),
    ], calls=(BIT15, ANIM_BEGIN), mutants=("@mutant", "@zero", "@ne")),
    # 0x402FC: 0x339AC runs on both sides (allow, record E3 §E3.6); its side is rec+0x51. +0x42, which it
    # never writes, carries a sentinel.
    Spec("fighter_402fc", 0x402FC, [
        Case("z%d" % side, {"eax": E3_SLOT, "edx": E3_REC},
             {**P1_SEED, **SLOT_PTRS, E3_REC + 0x51: bytes([side]), 0x1080A0: b"\xa0\xa0\xa2\xa2",
              E3_SLOT + 0x42: b"\x42"},
             {0x3C4CC: 0x1234} if side else {})
        for side in (0, 1)
    ], allow_calls=(0x339AC,), calls=(HIT_B,), eax_mask=0xFF),
]

SPECS = [
    Spec("rng_next", 0x5D7DC, [
        Case("r1", {"eax": 0x1234}, {DS_RNG: le32(0x12345678)}),
        Case("r2", {"eax": 0xFFFF}, {DS_RNG: le32(0xFFFFFFFF)}),
        Case("r3", {"eax": 0x10000}, {DS_RNG: le32(1)}),       # range & 0xFFFF == 0
        Case("r4", {"eax": 0}, {DS_RNG: le32(0xABCD)}),
    ]),
    Spec("fighter_slot_flag", 0x3C570, [
        Case("f0", {"eax": 0}, {DS_FLAGS: le32(0)}),           # sets bit 0, returns 0
        Case("f5", {"eax": 5}, {DS_FLAGS: le32(0x20)}),        # already set: returns 1, no write
        Case("f9", {"eax": 9}, {DS_FLAGS: le32(0x205)}),       # AL-only return: EAX bits 8+ are scratch
        Case("f31", {"eax": 31}, {DS_FLAGS: le32(0)}),
        Case("f33", {"eax": 33}, {DS_FLAGS: le32(2)}),         # x86 masks the count to 5 bits
    # AL only: the original sets AL (`mov al,1` 0x3C586, `xor al,al` 0x3C596) and leaves EAX bits
    # 8+ as scratch. All five direct callers (0x1650B, 0x16534, 0x16B72, 0x16B9B, 0x19091, found
    # by scanning the image for `call rel32` to 0x3C570; no `jmp` tail call or pointer to it)
    # follow the call with `test al,al`; record E1 §E.5.
    ], eax_mask=0xFF),
    Spec("config_credit_spend", 0x2CA7C, [
        Case("c1", {"eax": 5}, {DS_FREEPLAY: b"\x01", DS_CREDITS: le32(0)}),
        Case("c2", {"eax": 5}, {DS_FREEPLAY: b"\x00", DS_CREDITS: le32(3), DS_NODEBIT: b"\x00"}),
        Case("c3", {"eax": 5}, {DS_FREEPLAY: b"\x00", DS_CREDITS: le32(5), DS_NODEBIT: b"\x00"}),
        Case("c4", {"eax": 3}, {DS_FREEPLAY: b"\x00", DS_CREDITS: le32(10), DS_NODEBIT: b"\x01"}),
        Case("c5", {"eax": 0x80000001}, {DS_FREEPLAY: b"\x00", DS_CREDITS: le32(1), DS_NODEBIT: b"\x00"}),
    ], mutants=("@mutant", "@signed")),
    Spec("config_codeword_len", 0x2D4B4, [
        Case("w%d" % n, {"eax": n}) for n in (0, 1, 0x26, 0xFF, 0x1000)
    ]),
    # Record gameplay-u6 §U6.6. Mask 0: the three 0x2B2A0 call sites reload EAX at once (0x2B575,
    # 0x2B59A, 0x2B5F0), so the comparison is the changed bytes alone.
    Spec("fighter_3640c", 0x3640C, [
        Case("k0", {"eax": U6_REC, "edx": 0x11223344},
             {U6_REC + 0x52: b"\x7f", U6_REC + 0x24: le32(0xDEADBEEF), U6_REC + 0x4D: b"\x99",
              U6_REC + 0x14: le32(0)}),
        Case("k1", {"eax": U6_REC, "edx": 0x11223344},
             {U6_REC + 0x52: b"\x7f", U6_REC + 0x24: le32(0xDEADBEEF), U6_REC + 0x4D: b"\x99",
              U6_REC + 0x14: le32(U6_OWNER), U6_OWNER + 0x52: b"\x33"}),
    ], eax_mask=0),
    Spec("fighter_37dcc", 0x37DCC, [
        Case("d0", {}, {DS_1078FC: b"\x00"}),
        Case("d1", {"eax": U6_REC}, {DS_1078FC: b"\x5a"}),
    ], eax_mask=0),
] + E3_SPECS + P1_SPECS


# ---- driver -------------------------------------------------------------------------------------

def verify_all(diffrun, exe, image_path, specs, only=None, mutants=False):
    """One SpecResult per function (or per mutant binding when `mutants`)."""
    results = []
    for spec in specs:
        if only and spec.name != only:
            continue
        if spec.gap:
            if not mutants:
                results.append(verify_gap(spec, E.Image.load(image_path)))
            continue
        names = [spec.name + s for s in spec.mutants] if mutants else [spec.name]
        for name in names:
            port = run_port(diffrun, exe, image_path, cases_text(spec, name))
            results.append(verify_spec(spec, E.Image.load(image_path), port, name))
    return results


def mutant_detection(r):
    """(detected, reason). A mutant is detected only when the port ran every case without an error
    and at least one case differs in EAX or in a changed byte. An unknown binding makes every case
    a port error, which `compare` also reports as a problem (MISMATCH): that is a mutant that is
    missing, not one that was caught."""
    if r.port_errors:
        return False, "port error: " + r.port_errors[0]
    if not r.diffs:
        return False, "no eax or byte difference, and no call difference, on any case (verdict %s)" % r.verdict
    return True, ""


def closed_rows(funcs, verdicts):
    """(closed, with_callees): the rows that have callees, and those of them that are VERIFIED with
    every callee VERIFIED by its own row (one level deep, record §E3.6). Rows with no callee are
    counted apart, so they cannot make the figure look closer than it is (record §E3.4)."""
    with_callees = [r for r in funcs if r.callees]
    closed = [r for r in with_callees if r.verdict == "VERIFIED"
              and all(verdicts.get(a) == "VERIFIED" for a, _ in r.callees)]
    return closed, with_callees


def table_row(r, verdicts=None):
    """One row; `verdicts` (entry -> verdict of the real rows) marks each callee with its own check."""
    note = "; reads outside the image: " + ", ".join("0x%X+%d" % o for o in r.outside) if r.outside else ""
    if r.gap:
        note += " (%s)" % r.gap
    callees = ", ".join("%05X %s %s" % (a, how, (verdicts or {}).get(a, "unverified"))
                        for a, how in r.callees) or "-"
    return "| %s | 0x%05X | %d | %d/%d | %s%s | %s |" % (r.name, r.entry, r.ncases, r.hit, r.total, r.verdict,
                                                       note, callees)


TABLE_HEAD = ("| function | original | cases | blocks hit/total | verdict | callees (each by its own check) |\n"
              "|---|---|---|---|---|---|")


def skip(why):
    """Skip like the other oracles (spec §5.5): exit 0, unless PR_ORACLE_REQUIRED=1."""
    print("diff-verify: skipped: " + why)
    return 1 if os.environ.get("PR_ORACLE_REQUIRED") == "1" else 0


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--diffrun", default="build/diffrun")
    ap.add_argument("--exe", default="data/game/C/PRAGE.EXE")
    ap.add_argument("--image", default=os.path.join(tempfile.gettempdir(), "pr_diff_image.bin"),
                    help="where diffrun dumps the image both sides run from")
    ap.add_argument("--function", help="only this function (its callees' rows are not run, so the callee "
                                       "column reads 'unverified' for each; record E3 §E3.8)")
    ap.add_argument("--self-check", action="store_true",
                    help="also require every mutant binding to be detected (an eax, byte, call-list or "
                         "call-memory difference, and no port error)")
    ap.add_argument("--table", help="write the verification table (markdown) to this file")
    args = ap.parse_args(argv)

    if args.function and args.function not in [s.name for s in SPECS]:
        print("diff-verify: --function %s names no spec; known: %s"
              % (args.function, ", ".join(s.name for s in SPECS)))
        return 2
    if not E.available():
        return skip("unicorn or capstone is not installed (pip install -r tools/requirements-diff.txt)")
    if not os.path.exists(args.exe):
        return skip("%s is absent" % args.exe)
    if not os.path.exists(args.diffrun):
        print("diff-verify: %s is not built (make build)" % args.diffrun)
        return 2

    try:
        real = verify_all(args.diffrun, args.exe, args.image, SPECS, args.function)
        mutants = (verify_all(args.diffrun, args.exe, args.image, SPECS, args.function, mutants=True)
                   if args.self_check else [])
    except (RuntimeError, ValueError) as e:
        print("diff-verify: error: %s" % e)
        return 2
    gaps = [r for r in real if r.gap]
    funcs = [r for r in real if not r.gap]
    bad = [r for r in funcs if r.verdict != "VERIFIED"] + [r for r in gaps if r.verdict != "NAMED_GAP"]
    missed = [(r, why) for r in mutants for ok, why in [mutant_detection(r)] if not ok]
    verdicts = {r.entry: r.verdict for r in real}
    closed, with_callees = closed_rows(funcs, verdicts)

    rows = [TABLE_HEAD] + [table_row(r, verdicts) for r in real + mutants]
    print("\n".join(rows))
    for r in real:
        for p in r.problems:
            print("  %s: %s" % (r.name, p))
        for a in r.unhit:
            print("  %s: block 0x%X not exercised" % (r.name, a))
    for r, why in missed:
        print("  %s: NOT DETECTED (%s)" % (r.name, why))
    if args.table:
        with open(args.table, "w") as f:
            f.write("\n".join(rows) + "\n")
    print("diff-verify: %d/%d functions VERIFIED%s; %d named gaps; %d/%d rows with callees closed (%d have "
          "none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees "
          "stubbed or run as stated." % (len([r for r in funcs if r.verdict == "VERIFIED"]), len(funcs),
                                         "; %d/%d mutants detected" % (len(mutants) - len(missed), len(mutants))
                                         if args.self_check else "", len(gaps), len(closed), len(with_callees),
                                         len(funcs) - len(with_callees)))
    return 1 if bad or missed else 0


if __name__ == "__main__":
    sys.exit(main())
