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
    info = E.static_scan(image, spec.entry, stop=[k.addr for k in spec.calls], switches=True,
                         resolved=E.RESOLVED_JUMPS)
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
    info = E.static_scan(image, spec.entry, stop=[k.addr for k in spec.calls], switches=True,
                         resolved=E.RESOLVED_JUMPS)
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


# 0x2A17C (actor_pset_palette): EAX = rec, EDX = word, EBX = handle; a plain `ret`; it saves ECX and ESI and
# clobbers EDX (E.callee_clobbers; record §P1.4).
PALETTE = E.Call(0x2A17C, ("eax", "edx", "ebx"), clobbers=("edx",))
# 14 pokes (diffrun takes 16 per case): the two slots' characters (5 and 3), +0x42 and +4, sentinels on
# everything the callbacks store; +0x52..+0x57 as one poke. Slot 1's +0x42 is 0x20, bit 2 clear, so case 1's
# `or byte [ctx[3]+0x42],4` (0x155CD, 0x157E5) shows (final review I1; @no42). The two slots' +4 records
# differ (final review I2): slot 0's is P1_OWN4, slot 1's E3_OUT, so case 3's copy to the other slot's +4
# record (0x15647, 0x158ED) cannot pass as a copy to its own (@own4); both words +0x2C carry a sentinel, in
# one poke (E3_OUT + 0x2C .. P1_OWN4 + 0x2D).
P1_OWN4 = E3_OUT + 0x20
P1_SEED_CB = {DS_SLOTS + 0x7A: b"\x05", DS_SLOTS + 0x94 + 0x7A: b"\x03", DS_SLOTS + 0x42: b"\x42",
              DS_SLOTS + 0x94 + 0x42: b"\x20", DS_SLOTS + 4: le32(P1_OWN4), DS_SLOTS + 0x94 + 4: le32(E3_OUT),
              E3_OUT + 0x2C: b"\xcc\xcc" + bytes(P1_OWN4 - E3_OUT - 2) + b"\xdd\xdd",
              0xF0AFE: b"\xfe", 0x1078FC: b"\xfc",
              E3_REC2 + 0x29: b"\x29", E3_REC2 + 0x34: b"\x34\x34"}


def p1_cb(name, entry, rows, calls, stub_eax=None, mutants=("@mutant",)):
    """A +0x0C callback's cases: (index, the slot's +0x57, side, extra pokes)."""
    return Spec(name, entry, [
        Case("c%d" % i, {"eax": E3_SLOT, "edx": E3_REC, "ebx": side},
             {**SLOT_PTRS, **P1_SEED_CB, E3_SLOT + 0x52: bytes([0x52, 0x53, 0x54, 0x55, 0x56, st]), **extra},
             (stub_eax or {}).get(i, {}))
        for i, st, side, extra in rows
    ], allow_calls=(0x33950,), calls=calls, eax_mask=0, mutants=mutants)


def p1_23d38(cid, st, x, ox, w28):
    # the held record's +0x29 has bit 6 clear when the word +0x28's bit 14 is set (case 3 sets it, 0x23E67)
    # and set when it is clear (case 3 clears it, `and byte [..+0x29],0xbf` 0x23E70): either store shows
    # (final review I1; @noand, case gA)
    b29 = b"\x29" if w28 & 0x4000 else b"\x69"
    return Case(cid, {"eax": E3_SLOT, "edx": E3_REC, "ebx": 0},
                {E3_SLOT + 0x57: bytes([st]), E3_SLOT + 8: le32(E3_REC2), 0xF0AF0: le32(0x10000),
                 E3_REC + 0x18: le32(x), E3_REC + 0x1C: le32(0x1C1C), E3_REC + 0x28: le32(w28)[:2],
                 E3_REC2 + 0x18: le32(ox), E3_REC2 + 0x1C: le32(0x2C2C), E3_REC2 + 0x29: b29,
                 E3_REC2 + 0x2C: b"\xcc\xcc", E3_REC2 + 0x34: b"\x34\x34", E3_SLOT + 0x52: b"\x52\x53",
                 0xF0AFE: b"\xfe", 0x1078FC: b"\xfc"})


# 0x188AC (hit_anchor_set): EAX = side, EDX = x, EBX = y, a plain `ret`, clobbers EDX; 0x38034: EAX = side,
# a plain `ret`, saves EBX, ECX, EDX (record §P1.4).
ANCHOR = E.Call(0x188AC, ("eax", "edx", "ebx"), clobbers=("edx",))
F38034 = E.Call(0x38034, ("eax",))


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
    # 0x15584 and 0x1579C (record §P1.8): 0x33950 runs on both sides (allow); with side 0 the other slot is
    # slot 1 (character 3), with side 1 slot 0 (character 5); the side's own slot (ctx[2]: slot 0 for side 0,
    # character 5) has the other character, so the mutant's wrong index shows. ctx[3]+4 points at E3_OUT (side
    # 0; P1_OWN4 for side 1), the record whose word +0x2C case 3 copies to. 0x15584's c9 and 0x1579C's c7: the word 0x440 is exactly 0x400
    # after the subtract, where `jg` (0x15638, 0x15850) does not jump: only they tell `jg` from `jge` (@ge).
    p1_cb("fighter_15584", 0x15584, [(0, 0, 0, {}), (1, 1, 0, {}), (2, 2, 1, {}),
                                      (3, 3, 0, {E3_REC2 + 0x2C: b"\x00\x05"}),
                                      (4, 3, 0, {E3_REC2 + 0x2C: b"\x20\x04"}),
                                      (5, 3, 0, {E3_REC2 + 0x2C: b"\x10\x00"}),
                                      (6, 4, 0, {}), (7, 5, 0, {}), (8, 6, 0, {}),
                                      (9, 3, 0, {E3_REC2 + 0x2C: b"\x40\x04"})],
          calls=(ANIM_BEGIN, VOICE), stub_eax={1: {0x2C3FC: 0}}, mutants=("@mutant", "@ge", "@no42", "@own4")),
    p1_cb("fighter_1579c", 0x1579C, [(0, 0, 0, {}), (1, 1, 0, {}), (2, 2, 1, {}),
                                      (3, 3, 0, {E3_REC2 + 0x2C: b"\x00\x05"}),
                                      (4, 3, 0, {E3_REC2 + 0x2C: b"\x20\x04", E3_REC2 + 0x28: b"\x00\x40"}),
                                      (5, 3, 1, {E3_REC + 0x28: b"\xff\xbf\x00\x00\x20\x04"}),
                                      (6, 4, 0, {}), (7, 3, 0, {E3_REC2 + 0x2C: b"\x40\x04"})],
          calls=(ANIM_BEGIN, PALETTE, VOICE), mutants=("@mutant", "@ge", "@no42")),
    # 0x23D38 (record §P1.8): DS_000F0AF0 = 0x10000; the record at slot+8 is E3_REC2. Case 0's boundaries
    # and signedness (Task 4 review): gE/gF put x exactly at the bound (0x13000 with bit 14 of the word +0x28
    # set, 0xD000 with it clear; `jge` 0x23D79 and `jle` 0x23DA5 return, @ge does not); gG/gH a negative x
    # (signed compares; @unsigned flips both); gJ the word 0xBFFF (bit 14 clear, every other bit set: `and
    # dh,0x40` 0x23D60, not a test of the whole word, @bit). Case 1: gI puts F0AF0 - x at 0x80000000, which
    # `neg` (0x23DC1) leaves negative so `jg` passes (@unsigned returns); gK at 0x80000001 (|d| 0x7FFFFFFF;
    # without the `neg`, @noneg, it would pass).
    Spec("fighter_23d38", 0x23D38, [
        p1_23d38(cid, st, x, ox, w28) for cid, st, x, ox, w28 in (
            ("g0", 0, 0x12000, 0, 0x4000), ("g1", 0, 0x14000, 0, 0x4000), ("g2", 0, 0xE000, 0, 0),
            ("g3", 0, 0xC000, 0, 0), ("g4", 1, 0xDFFF, 0, 0), ("g5", 1, 0x12000, 0, 0),
            ("g6", 2, 0x5000, 0x6001, 0), ("g7", 2, 0x7000, 0x6000, 0), ("g8", 3, 0x5000, 0x5B01, 0x4000),
            ("g9", 3, 0x5000, 0x4500, 0x4000), ("gA", 3, 0x5000, 0x5000, 0), ("gB", 4, 0, 0, 0),
            ("gC", 5, 0, 0, 0), ("gD", 6, 0, 0, 0),
            ("gE", 0, 0x13000, 0, 0x4000), ("gF", 0, 0xD000, 0, 0), ("gG", 0, 0xFFFFF000, 0, 0x4000),
            ("gH", 0, 0xFFFFF000, 0, 0), ("gI", 1, 0x80010000, 0, 0), ("gJ", 0, 0xC000, 0, 0xBFFF),
            ("gK", 1, 0x8000FFFF, 0, 0))
    ], calls=(ANIM_BEGIN, VOICE), eax_mask=0, mutants=("@mutant", "@ge", "@unsigned", "@bit", "@noneg", "@noand")),
    # 0x38034 (record §P1.9): side 0 is character 2, side 1 character 4; the spawn stub's EAX is the record
    # whose +0x59 0x38034 sets, a different one per case.
    Spec("fighter_38034", 0x38034, [
        Case("s%d" % side, {"eax": side},
             {DS_SLOTS + side * 0x94: le32(E3_REC), DS_SLOTS + side * 0x94 + 0x7A: bytes([2 + 2 * side]),
              E3_REC + 0x28: le32(w28)[:2], E3_REC + 0x56: b"\x23\x01", sp + 0x59: b"\x59"},
             {0x2AE14: sp})
        for side, w28, sp in ((0, 0x4000, E3_REC2), (1, 0xBFFF, E3_OUT))
    ], calls=(ANIM_BEGIN, SPAWN), eax_mask=0),
    # 0x23B68 (record §P1.9): +0x57 0 and 2 do nothing; 1 with rec+0x1C set only divides; d = 3 and -3 pin
    # the truncating signed division. The slot's +4 record is E3_OUT. q5's dword +0x30 0xFFFD8000 (negative,
    # low half set): `sar ebx,0x10` (0x23B87) gives -3 where a signed / 0x10000 gives -2.
    Spec("fighter_23b68", 0x23B68, [
        Case(cid, {"eax": E3_SLOT, "edx": E3_REC, "ebx": 0},
             {E3_SLOT + 0x52: bytes([0x52, 0x53, 0x54, 0x55, 0x56, st]), E3_SLOT + 4: le32(E3_OUT),
              E3_REC + 0x18: le32(0x12345), E3_REC + 0x1C: le32(y), E3_REC + 0x2C: b"\xcc\xcc",
              E3_REC + 0x30: le32(w30), E3_REC + 0x38: b"\x38\x38", E3_OUT + 0x2C: b"\xcc\xcc",
              0xF0AFE: b"\xfe", 0x1078FC: b"\xfc"})
        for cid, st, y, w30 in (("q0", 0, 0, 0x30000), ("q2", 2, 0, 0x30000), ("q1", 1, 0x77, 0x30000),
                                ("q3", 1, 0, 0x30000), ("q4", 1, 0, 0xFFFD0000), ("q5", 1, 0, 0xFFFD8000))
    ], calls=(ANIM_BEGIN, SPAWN, VOICE), eax_mask=0),
    # 0x401D4 (record §P1.9): 0x33950 runs on both sides; EDX (rec) is E3_OUT, not the side's record E3_REC
    # (ctx[4]), so a port that confuses them differs. Side 0's slot has character 1 (threshold 0x1400). t4's
    # rec word +0x36 is the sentinel 0x3636 (case 1 never reads it), so the store 0x4027E shows (@no36).
    Spec("fighter_401d4", 0x401D4, [
        Case(cid, {"eax": E3_SLOT, "edx": E3_OUT, "ebx": 0},
             {**SLOT_PTRS, E3_SLOT + 0x52: bytes([0x52, 0x53, 0x54, 0x55, 0x56, st]),
              DS_SLOTS + 0x7A: b"\x01", DS_SLOTS + 0x30: le32(s30), DS_SLOTS + 0x54: b"\x54\x55\x56\x57",
              E3_REC + 0x36: le32(r36)[:2], E3_OUT + 0x18: le32(0x5678), E3_OUT + 0x30: le32(0x00070000),
              E3_OUT + 0x34: le32(o36 << 16 | 0x3434), E3_OUT + 0x44: b"\x44\x44",
              0x1078FD: b"\x01", 0x105B3A: bytes([b3a]), 0x1078FC: b"\xfc"})
        for cid, st, s30, r36, o36, b3a in (
            ("t0", 0, 0, 0, 0x0001, 0), ("t1", 0, 0, 0, 0x8000, 0),
            ("t2", 1, 0x1400, 0x8000, 0, 0), ("t3", 1, 0x13FF, 0x7FFF, 0, 0), ("t4", 1, 0x13FF, 0x8000, 0x3636, 0),
            ("t5", 2, 0, 0, 0, 0), ("t6", 3, 0, 0, 0, 1), ("t7", 3, 0, 0, 0, 2), ("t8", 4, 0, 0, 0, 0))
    ], allow_calls=(0x33950,), calls=(ANCHOR, ANIM_BEGIN, F38034, VOICE, SPAWN), eax_mask=0,
         mutants=("@mutant", "@no36")),
]

# ---- final review I4 (record 2026-10-02-reverse-p1 §P1.12): the 0xD100 targets of the finisher streams --
# Each runs as the animation dispatcher's opcode 0x11 calls it (0x2B57F..0x2B594): EAX = rec, EDX = the operand
# word (zero-extended), ECX = 0. Mask 0: the dispatcher overwrites EAX (`mov eax,ecx` 0x2B59A).
P1_ANIM_SEED = {E3_SLOT + 0x54: b"\x54\x55\x56\x57", 0xF0AFE: b"\xfe\xfe",
                E3_REC + 0x34: b"\x34\x34\x36\x36\x38\x38", E3_REC + 0x44: b"\x44\x44"}


def p1_23ca4(cid, owner, side, others, stage, x):
    """0x23CA4: owner = rec+0x14; side = rec+0x51; others = the four DS_001077A8 dwords (the slot pointers
    0x23CBB indexes by (side ^ 1) & 0xFF); stage = DS_00104AFC (0xA83CC: 0x2600, 0, 0x6000); x = rec+0x18."""
    return Case(cid, {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
                {**P1_ANIM_SEED, E3_REC + 0x14: le32(owner), E3_REC + 0x18: le32(x), E3_REC + 0x51: bytes([side]),
                 DS_SLOTS - 8: b"".join(le32(v) for v in others), DS_STAGE: le32(stage)[:2]})


def p1_23868(cid, owner, arg, w28, side, x, y, z, sp):
    """0x23868: EDX = the operand (0, 1, 2 in the streams: the words 0xC0, 0x180, 0x20 at 0xA8364); the
    SPAWN stub returns `sp` for both spawns (one EAX per stub per case: §P1.12's limit), seeded with
    sentinels on every field the function stores or reads there."""
    return Case(cid, {"eax": E3_REC, "edx": arg, "ecx": 0},
                {E3_REC + 0x14: le32(owner), E3_REC + 0x18: le32(x) + le32(y),
                 E3_REC + 0x28: le32(w28)[:2] + b"\x00" * 6 + le32(z),
                 E3_REC + 0x4B: b"\x4b" + bytes(5) + bytes([side]) + bytes(4) + b"\x23\x01",
                 E3_SLOT + 8: le32(0x08080808), sp + 0x14: le32(0x14141414), sp + 0x34: b"\x34\x34",
                 sp + 0x56: b"\x5a\x00\x00\x59\x00\x00\x00\x00\x00\x00\x60"},
                {0x2AE14: sp})


P1_ANIM_SPECS = [
    # 0x156D4: no owner; the owner's +0x57 0x57 -> 0x58; 0xFF wraps to 0 (a byte `inc`).
    Spec("fighter_156d4", 0x156D4, [
        Case("s0", {"eax": E3_REC, "edx": 0, "ecx": 0}, {E3_REC + 0x14: le32(0), E3_REC + 0x57: b"\x57"}),
        Case("s1", {"eax": E3_REC, "edx": 0, "ecx": 0},
             {E3_REC + 0x14: le32(E3_SLOT), E3_SLOT + 0x57: b"\x57", E3_REC + 0x57: b"\x57"}),
        Case("s2", {"eax": E3_REC2, "edx": 0, "ecx": 0},
             {E3_REC2 + 0x14: le32(E3_SLOT), E3_SLOT + 0x57: b"\xff", E3_REC2 + 0x57: b"\x57"}),
    ], eax_mask=0),
    # 0x23CA4: k0 no owner; k1 the other side's slot pointer 0 (side 0 reads DS_001077AC); k2 side 1 (reads
    # DS_001077A8), stage 0, d = 0x2600 - 0x1000 = 0x1600 (/0x48 = 0x4E); k3 side 0, stage 2, d = 0x6000 -
    # 0x7000 = -0x1000 (-56.9: `idiv` truncates to -56, 0xFFC8; unsigned or floor division differs); k4 side 2:
    # (2 ^ 1) & 0xFF = 3 reads DS_001077B4 (a logical not would read [0]); k5 side 1, stage 5, x 0x80003100
    # (d = 0x80000000, the most negative: -0x1C71C71 / 0xE38F).
    Spec("fighter_23ca4", 0x23CA4, [
        p1_23ca4("k0", 0, 1, (E3_SLOT, E3_SLOT, 0, 0), 0, 0x1000),
        p1_23ca4("k1", E3_SLOT, 0, (E3_SLOT, 0, 0, 0), 0, 0x1000),
        p1_23ca4("k2", E3_SLOT, 1, (E3_REC2, 0, 0, 0), 0, 0x1000),
        p1_23ca4("k3", E3_SLOT, 0, (0, E3_REC2, 0, 0), 2, 0x7000),
        p1_23ca4("k4", E3_SLOT, 2, (0, 0, 0, E3_REC2), 2, 0x5000),
        p1_23ca4("k5", E3_SLOT, 1, (E3_REC2, 0, 0, 0), 5, 0x80003100),
    ], eax_mask=0),
    # 0x23868: n0 no owner; n1 bit 14 clear (-w, x - 0xC00), operand 0, side 0 (no palette); n2 bit 14 set,
    # operand 1, side 1 (both palettes); n3 the word 0xBFFF (bit 14 clear, every other bit set: `and ah,0x40`
    # 0x23882), operand 2, side 1; n4 bit 14 set with a negative x and z (`sar` 0x238E4), side 0.
    Spec("fighter_23868", 0x23868, [
        p1_23868("n0", 0, 0, 0, 0, 0x5000, 0x100, 0x30000, E3_OUT),
        p1_23868("n1", E3_SLOT, 0, 0, 0, 0x5000, 0x100, 0x30000, E3_OUT),
        p1_23868("n2", E3_SLOT, 1, 0x4000, 1, 0x7000, 0x200, 0x50000, E3_REC2),
        p1_23868("n3", E3_SLOT, 2, 0xBFFF, 1, 0x6000, 0x300, 0x70000, E3_OUT),
        p1_23868("n4", E3_SLOT, 2, 0x4000, 0, 0xFFFFF000, 0, 0xFFFD8000, E3_REC2),
    ], calls=(SPAWN, PALETTE), eax_mask=0),
    Spec("fighter_3f174", 0x3F174, [
        Case("t%d" % i, {"eax": rec, "edx": 0, "ecx": 0},
             {rec + 0x34: b"\x34\x34\x36\x36", rec + 0x44: b"\x44\x44"})
        for i, rec in enumerate((E3_REC, E3_REC2))
    ], eax_mask=0),
]

# ---- track P batch 2: the move callbacks 0x14EF8..0x3DCEC and the callbacks they store (record
# 2026-10-02-reverse-p2) --------------------------------------------------------------------------------
# A move callback runs as 0x34E2C calls it at 0x35045: EAX = slot, EDX = rec, EBX = side. Mask 0: 0x34E2C
# returns the callback's EAX to 0x352CD (whose 0x350D0 returns to 0x3531C, whose only caller 0x35803 loads
# `mov eax,ebx`) and to 0x3CF2E (`mov al,1` at 0x3CF33; 0x3CE58's two callers read AL alone, `mov dl,al`
# at 0x3CF84/0x3D03A): no caller reads it (record §P2.2). Every slot field the function writes carries a
# sentinel that differs from what it writes; +0x5F (copied to +0x64) takes 0x22 and 0x80.
P2_SEED = {E3_SLOT + 0x0C: le32(0x0C0C0C0C), E3_SLOT + 0x18: le32(0x18181818), E3_SLOT + 0x1C: le32(0x1C1C1C1C),
           E3_SLOT + 0x52: b"\x52\x53\x54\x55\x56\x57", E3_SLOT + 0x5F: b"\x22", E3_SLOT + 0x64: b"\x64"}


def p2_guarded(name, entry, voice, mutants=("@mutant",), extra=None, more=()):
    """A guard-shaped move callback (record §P2.3): `cmp dword [slot+8],0; je` else AL = 0 and nothing
    written. g0: slot+8 = 0x01000000 (non-zero in its high byte only, so a byte test runs the body); g1:
    the body, +0x5F 0x22; g2: the body, +0x5F 0x80, side 1, the voice stub's AL = 0 (`mov al,1` overwrites
    it). `extra(i)` adds pokes per case; `more` is further body cases as (id, ebx), numbered from 3 for `extra`."""
    ex = extra or (lambda i: {})
    return Spec(name, entry, [
        Case("g0", {"eax": E3_SLOT, "edx": E3_REC, "ebx": 0}, {**P2_SEED, E3_SLOT + 8: le32(0x01000000), **ex(0)}),
        Case("g1", {"eax": E3_SLOT, "edx": E3_REC, "ebx": 0}, {**P2_SEED, E3_SLOT + 8: le32(0), **ex(1)}),
        Case("g2", {"eax": E3_SLOT, "edx": E3_REC, "ebx": 1},
             {**P2_SEED, E3_SLOT + 8: le32(0), E3_SLOT + 0x5F: b"\x80", **ex(2)}, {0x2C3FC: 0} if voice else {}),
    ] + [Case(cid, {"eax": E3_SLOT, "edx": E3_REC, "ebx": ebx}, {**P2_SEED, E3_SLOT + 8: le32(0), **ex(3 + k)})
         for k, (cid, ebx) in enumerate(more)], calls=(HIT_B, VOICE) if voice else (HIT_B,), eax_mask=0, mutants=mutants)


# 0x3D10C writes the word DS_001080AC[rec+0x51] (0x3D170) and ignores EBX (plan P2 Task 2 review): rec+0x51 is
# 0 in g0/g1 (side 0), 1 in g2 (side 1), 1 in g3 with side 0 (a port indexing by side writes the wrong word:
# only g3 tells), and 0x80 in g4 with side 0x80 (the `movzx` at 0x3D113 reaches 0x1081AC; a `movsx` reaches
# 0x107FAC: only g4 tells). The words around are seeded with sentinels.
P2_3D10C_R51 = {2: 1, 3: 1, 4: 0x80}


def p2_3d10c_extra(i):
    return {E3_REC + 0x51: bytes([P2_3D10C_R51.get(i, 0)]), 0x1080AC: b"\xac\xac\xae\xae",
            0x1081AC: b"\xb1\xb1", 0x107FAC: b"\xaf\xaf"}


P2_SPECS = [
    p2_guarded("fighter_237d0", 0x237D0, True, ("@mutant", "@guard")),
    p2_guarded("fighter_2381c", 0x2381C, True),
    p2_guarded("fighter_3dadc", 0x3DADC, True),
    p2_guarded("fighter_3db34", 0x3DB34, True),
    p2_guarded("fighter_3d10c", 0x3D10C, True, ("@mutant", "@side", "@sext"), extra=p2_3d10c_extra,
               more=(("g3", 0), ("g4", 0x80))),
    p2_guarded("fighter_22a00", 0x22A00, False),
    # 0x229FC is the `ret` that ends 0x229E8 (0x229FC: c3), the +0x0C callback 0x22A00 stores: nothing at all.
    Spec("fighter_229fc", 0x229FC, [
        Case("r0", {"eax": E3_SLOT, "edx": E3_REC, "ebx": 0}, {E3_SLOT + 0x57: b"\x57"}),
    ], eax_mask=0),
    # character 3's reactions 0x20 and 0x21: the two callbacks gp-u8-right-arcade reaches (record §P2.4)
    p2_guarded("fighter_14ef8", 0x14EF8, True),
    p2_guarded("fighter_14f50", 0x14F50, True),
]


# The 0xD000 targets (opcode 0x10, mode 0x4000) of the streams 0x14EF8 and 0x14F50 start (record §P2.4): the
# animation dispatcher calls them at 0x2B56D with EAX = rec (`mov eax,esi` 0x2B56B; none of the three reads EDX
# or ECX) and overwrites EAX after (`xor ecx,ecx; mov eax,ecx` 0x2B573/0x2B575): mask 0. The SPAWN stub's EAX
# is the spawned record, a different one per case, seeded with sentinels on every field written there.
def p2_14fa8(cid, side, sp):
    return Case(cid, {"eax": E3_REC, "edx": 0x1234, "ecx": 0x5678},
                {E3_REC + 0x4B: b"\x4b", E3_REC + 0x51: bytes([side]), E3_REC + 0x56: b"\x23\x01",
                 sp + 0x2E: b"\xfe\xff", sp + 0x4E: b"\x4e", sp + 0x56: b"\x07", sp + 0x59: b"\x59",
                 sp + 0x60: b"\x60"}, {0x2AE14: sp})


def p2_held(cid, owner, side, al, w28, x, z, w30, sp):
    """0x14FF8/0x150AC: owner = rec+0x14; side = rec+0x51 (0x1A570's argument and the +0x2E/+0x4E arm); al =
    the 0x1A570 stub's AL; w28 = the word rec+0x28 (bit 14: the spawn flag); x, z = rec+0x18/+0x1C; w30 =
    rec+0x30 (`sar 0x10`: the y argument)."""
    return Case(cid, {"eax": E3_REC, "edx": 0x1234, "ecx": 0x5678},
                {E3_REC + 0x14: le32(owner), E3_REC + 0x18: le32(x) + le32(z), E3_REC + 0x28: le32(w28)[:2],
                 E3_REC + 0x30: le32(w30), E3_REC + 0x51: bytes([side]), E3_SLOT + 8: le32(0x08080808),
                 sp + 0x14: le32(0x14141414), sp + 0x2E: b"\xfe\xff", sp + 0x34: b"\x34\x34",
                 sp + 0x4E: b"\x4e", sp + 0x59: b"\x59"},
                {0x2AE14: sp, 0x1A570: al})


P2_HELD_ROWS = (("h0", 0, 0, 0, 0, 0x5000, 0x30000, 0x70000, E3_OUT),
                ("h1", E3_SLOT, 0, 0, 0, 0x5000, 0x30000, 0x70000, E3_OUT),
                ("h2", E3_SLOT, 1, 1, 0x4000, 0xFFFFF000, 0xFFFD8000, 0xFFFD0000, E3_REC2),
                ("h3", E3_SLOT, 1, 0, 0xBFFF, 0x6000, 0x50000, 0x30000, E3_OUT),
                ("h4", E3_SLOT, 0, 1, 0x4000, 0x7000, 0x10000, 0x20000, E3_REC2))
P2_SPECS += [
    Spec("fighter_14fa8", 0x14FA8, [p2_14fa8("f0", 0, E3_OUT), p2_14fa8("f1", 1, E3_REC2)],
         calls=(SPAWN,), eax_mask=0),
    Spec("fighter_14ff8", 0x14FF8, [p2_held(*r) for r in P2_HELD_ROWS], calls=(BIT15, SPAWN), eax_mask=0),
    Spec("fighter_150ac", 0x150AC, [p2_held(*r) for r in P2_HELD_ROWS], calls=(BIT15, SPAWN), eax_mask=0),
]

# The unconditional move callbacks (record §P2.5): no guard, the record started, the slot 9/8/0 or 9/8/1.
# 0x3DCEC reads its stream from the dword 0xC8CD4 (0xD40F2 in the image): u1 pokes another value there, so a
# port that hard-codes the image's value differs. 0x21114 indexes DS_001077A8 by rec+0x51 (0x2111C) and
# switches on that slot's +0x7A: 1 and 6 start a stream (`jbe`/`je`), 0, 2..5 and above 6 do not (`jb`, the
# `jmp` at 0x21140); w0 a zero pointer (nothing stored); w5 side 2 reads 0x1077B0 (a `xor edx,edx; mov
# dl,..` index: no `& 1`).
P2_SEED_UNC = {E3_SLOT + 0x52: b"\x52\x53\x54"}


def p2_21114(cid, side, ptrs, char):
    return Case(cid, {"eax": E3_SLOT, "edx": E3_REC, "ebx": 0},
                {**P2_SEED_UNC, E3_REC + 0x51: bytes([side]), DS_SLOTS - 8: b"".join(le32(v) for v in ptrs),
                 E3_REC2 + 0x7A: bytes([char])})


P2_SPECS += [
    Spec("fighter_15478", 0x15478, [
        Case("u0", {"eax": E3_SLOT, "edx": E3_REC, "ebx": 0}, P2_SEED_UNC),
        Case("u1", {"eax": E3_SLOT, "edx": E3_REC2, "ebx": 1}, {E3_SLOT + 0x52: b"\x09\x09\x09"}),
    ], calls=(HIT_B,), eax_mask=0),
    Spec("fighter_3dcec", 0x3DCEC, [
        Case("u0", {"eax": E3_SLOT, "edx": E3_REC, "ebx": 0}, P2_SEED_UNC),
        Case("u1", {"eax": E3_SLOT, "edx": E3_REC, "ebx": 1}, {**P2_SEED_UNC, 0xC8CD4: le32(0x000E1234)}),
    ], calls=(HIT_B,), eax_mask=0),
    Spec("fighter_21114", 0x21114, [
        p2_21114("w0", 0, (0, E3_REC2, 0, 0), 1),
        p2_21114("w1", 0, (E3_REC2, 0, 0, 0), 1),
        p2_21114("w2", 1, (0, E3_REC2, 0, 0), 6),
        p2_21114("w3", 1, (0, E3_REC2, 0, 0), 0),
        p2_21114("w4", 0, (E3_REC2, 0, 0, 0), 2),
        p2_21114("w5", 2, (0, 0, E3_REC2, 0), 7),
        p2_21114("w6", 0, (E3_REC2, 0, 0, 0), 5),
    ], calls=(HIT_B,), eax_mask=0, mutants=("@mutant", "@side")),
]

# 0x34D8C (hit_flash_pair): EAX = side, a plain `ret`; it pushes EBX and EDX and pops both (record §P2.6).
FLASH = E.Call(0x34D8C, ("eax",))


# The context-built move callbacks (record §P2.6) read EBX (side) alone: 0x33950(side) runs on both sides
# (allow) and they store into ctx[2], the side's own slot DS_SLOTS + side * 0x94, not the EAX slot. Both slots'
# records are SLOT_PTRS; the two slots' characters differ (5 and 3), so a port that reads the other slot's
# character differs; every field written carries a sentinel (+0x0C..+0x1F, +0x41/+0x42, +0x52..+0x57).
def p2_ctx_case(cid, side, extra=None):
    own = DS_SLOTS + side * 0x94
    return Case(cid, {"eax": E3_SLOT, "edx": E3_OUT, "ebx": side},
                {**SLOT_PTRS, DS_SLOTS + 0x7A: b"\x05", DS_SLOTS + 0x94 + 0x7A: b"\x03",
                 own + 0x0C: b"".join(le32(v * 0x01010101) for v in (0x0C, 0x10, 0x14, 0x18, 0x1C)),
                 own + 0x41: b"\x41\x42", own + 0x52: b"\x52\x53\x54\x55\x56\x57", **(extra or {})})


# 0x22938: the other slot's (ctx[3]) +0x42 bit 4 refuses (t0, 0x10 alone; t1/t2 0xEF, every other bit); the
# per-side words 0x104754/0x104758 and the floats 0x104738 (both sides' in one poke each) carry sentinels.
P2_22938_SEED = {0x104754: b"\x54\x47\x56\x47\x58\x47\x5a\x47", 0x104738: b"\x38\x47\x00\x00\x3c\x47\x00\x00"}
P2_SPECS += [
    Spec("fighter_21374", 0x21374, [p2_ctx_case("s0", 0), p2_ctx_case("s1", 1)],
         allow_calls=(0x33950,), calls=(HIT_B,), eax_mask=0),
    Spec("fighter_22938", 0x22938, [
        p2_ctx_case("t0", 0, {**P2_22938_SEED, DS_SLOTS + 0x94 + 0x42: b"\x10"}),
        p2_ctx_case("t1", 0, {**P2_22938_SEED, DS_SLOTS + 0x94 + 0x42: b"\xef"}),
        p2_ctx_case("t2", 1, {**P2_22938_SEED, DS_SLOTS + 0x42: b"\xef"}),
    ], allow_calls=(0x33950,), calls=(HIT_B, FLASH), eax_mask=0, mutants=("@mutant", "@order")),
]

# 0x18C14 (fighter_18c14): EAX = side, EDX = the 16 flag bytes (a buffer on the caller's stack: compared by value,
# the four dwords at EDX, record §P2.7), EBX/ECX = the two box tables; a plain `ret`; it clobbers EBX, EDX and
# EBP (E.callee_clobbers). 0x18BD4 fills the flags (16 bytes of 2 at EAX, a leaf) and runs on both sides.
CHECKS = E.Call(0x18C14, ("eax", "[edx]", "[edx+4]", "[edx+8]", "[edx+12]", "ebx", "ecx"),
                clobbers=("ebx", "edx", "ebp"))


# The slot +0x18 hooks (record §P2.7) run as 0x19020 calls them at 0x1903F: EAX = side; 0x19048 tests the whole
# EAX (`test eax,eax`): mask 0xFFFFFFFF, and the 0x18C14 stub's EAX (returned as is) varies per case. The side's
# slot (ctx[2]) is DS_SLOTS + side * 0x94; 0x2116C bounds its word +0x88 by the words 0xA81AC (1) and 0xA81AE
# (3), signed (`jl`/`jle`); k4 pokes the bounds to -16 and 16 with the word -1, where an unsigned compare
# returns 1 without the call.
def p2_2116c(cid, side, w88, extra=None, stub=None):
    return Case(cid, {"eax": side}, {**SLOT_PTRS, DS_SLOTS + side * 0x94 + 0x88: le32(w88)[:2], **(extra or {})},
                {0x18C14: stub} if stub is not None else {})


# 0x22510 bounds the side's word 0x104758 (the high half of the dword at 0x104756 + 2 * side, `sar 0x10`) by
# 0xD..0x14 (`jg`/`jge`); both sides' words in one poke.
def p2_22510(cid, side, w, stub=None):
    words = [0x5858, 0x5A5A]
    words[side] = w
    return Case(cid, {"eax": side}, {**SLOT_PTRS, 0x104758: le32(words[0])[:2] + le32(words[1])[:2]},
                {0x18C14: stub} if stub is not None else {})


P2_SPECS += [
    Spec("fighter_2116c", 0x2116C, [
        p2_2116c("k0", 0, 4), p2_2116c("k1", 0, 3, stub=0), p2_2116c("k2", 1, 1, stub=0x12345678),
        p2_2116c("k3", 1, 0), p2_2116c("k4", 0, 0xFFFF, {0xA81AC: b"\xf0\xff\x10\x00"}, stub=7),
    ], allow_calls=(0x33950, 0x18BD4), calls=(CHECKS,), mutants=("@mutant", "@unsigned", "@eax")),
    Spec("fighter_22510", 0x22510, [
        p2_22510("j0", 0, 0x15), p2_22510("j1", 0, 0x14, 0), p2_22510("j2", 1, 0xD, 0x9ABCDEF0),
        p2_22510("j3", 1, 0xC), p2_22510("j4", 0, 0xFFFF),
    ], allow_calls=(0x33950, 0x18BD4), calls=(CHECKS,), mutants=("@mutant", "@ge")),
]

# The callees the slot +0x1C callbacks stub (record §P2.8), args from their bytes, clobbers from
# E.callee_clobbers: 0x18AF8 takes nothing (`xor eax,eax; call 0x18b04`); 0x39834 EAX = side, EDX = a byte;
# 0x39A10 EAX = rec, EDX = the word (`movsx ebx,dx`); 0x3C208 EAX = side, EDX = the distance; 0x3C358 EAX = side
# (it pushes EDX and loads it from EAX before any read); 0x22404 EAX = side. All plain `ret`.
# FACING over-declares: 0x18AF8 preserves EBX/ECX/EDX (0x18B04 pushes them, the epilogue at 0x18AEF pops them).
# That is the safe direction here, but a future 0x3C208 row that stubs FACING would fail falsely, because
# 0x3C24D tests EDX after it: declare no clobbers there.
FACING = E.Call(0x18AF8, (), clobbers=("ebx", "ecx", "edx"))
POSE = E.Call(0x39834, ("eax", "edx"), clobbers=("edx", "ebp"))
TIMER = E.Call(0x39A10, ("eax", "edx"), clobbers=("edx",))
PLACE = E.Call(0x3C208, ("eax", "edx"), clobbers=("edx",))
HOLD = E.Call(0x3C358, ("eax",))
ARM404 = E.Call(0x22404, ("eax",))


# The slot +0x1C callbacks (record §P2.8) run as 0x193B0 calls them at 0x19505: EAX = side; 0x19508 loads
# `mov eax,[esp+8]`: mask 0. The slots' bytes +0x52..+0x5F carry different sentinels (slot 0 0x52.., slot 1
# 0xD2..: +0x5F is 0x211F0's byte for 0x39834, so a read of the other slot's differs); the characters 5 (slot 0)
# and 3 (slot 1) differ. `dist` pokes the word 0xA82D8 + 2 * 3 (the 0xA82D6 dword's high half for character 3,
# `sar 0x10`) negative, where a zero-extended read differs; `zext` pokes 0xA81B0 + 2 * 3 (0x211F0's word for
# character 3, `xor edx,edx; mov dx`) to 0xF000, where a sign-extended read differs.
def p2_1c(cid, side, dist=None, zext=None):
    pokes = {**SLOT_PTRS, DS_SLOTS + 0x7A: b"\x05", DS_SLOTS + 0x94 + 0x7A: b"\x03",
             DS_SLOTS + 0x52: bytes(range(0x52, 0x60)), DS_SLOTS + 0x94 + 0x52: bytes(range(0xD2, 0xE0)),
             DS_SLOTS + 0x10: le32(0x10101010), DS_SLOTS + 0x94 + 0x10: le32(0x10101010)}
    if dist is not None:
        pokes[0xA82D8 + 2 * 3] = le32(dist)[:2]
    if zext is not None:
        pokes[0xA81B0 + 2 * 3] = le32(zext)[:2]
    return Case(cid, {"eax": side}, pokes)


P2_SPECS += [
    Spec("fighter_22404", 0x22404, [p2_1c("a0", 0), p2_1c("a1", 1), p2_1c("a2", 0, 0xF000)],
         allow_calls=(0x33950,), calls=(ANIM_BEGIN, HIT_A, PLACE), eax_mask=0, mutants=("@mutant", "@signed")),
    Spec("fighter_211f0", 0x211F0, [p2_1c("b0", 0), p2_1c("b1", 1), p2_1c("b2", 0, None, 0xF000)],
         allow_calls=(0x33950,), calls=(FLASH, HIT_B, HIT_A, FACING, PLACE, POSE, HOLD, TIMER, VOICE), eax_mask=0,
         mutants=("@mutant", "@order", "@zext", "@slot")),
    Spec("fighter_22588", 0x22588, [p2_1c("d0", 0), p2_1c("d1", 1), p2_1c("d2", 0, 0xF000)],
         allow_calls=(0x33950,), calls=(FACING, FLASH, ARM404, HOLD, PLACE, TIMER, VOICE), eax_mask=0,
         mutants=("@mutant", "@order")),
]

# 0x36870 (fighter_36870): EAX = rec, a plain `ret`; it clobbers ESI, EDI and EBP (E.callee_clobbers).
ANIM54 = E.Call(0x36870, ("eax",), clobbers=("esi", "edi", "ebp"))


# The slot +0x0C callbacks 0x21374 and 0x22938 store (record §P2.9) run as 0x3531C case 7 calls them (0x35431:
# EAX = slot, EDX = rec, EBX = side; `xor eax,eax` at 0x35434): mask 0. Both read EBX alone for the context.
# 0x212CC: the own slot's +0x57 (state), word +0x88 (against 0xA81AE's 3, signed) and +0x8A; in state 1 the
# pointer DS_001077A8[rec+0x51] (EDX's record, E3_OUT here: not ctx[4]) and its +0x7A (1, 6 or another).
def p2_212cc(cid, st, w88=0x0100, ridx=0, ptrs=(E3_REC2, 0), char=1):
    return Case(cid, {"eax": E3_SLOT, "edx": E3_OUT, "ebx": 0},
                {**SLOT_PTRS, DS_SLOTS - 8: b"".join(le32(v) for v in ptrs), E3_REC2 + 0x7A: bytes([char]),
                 E3_OUT + 0x51: bytes([ridx]), DS_SLOTS + 0x52: bytes([0x52, 0x53, 0x54, 0x55, 0x56, st]),
                 DS_SLOTS + 0x88: le32(w88)[:2] + b"\x8a"})


def f32(x):
    import struct
    return struct.pack("<f", x)


# 0x22638: cmd = the two sides' command words DS_001088E0 (own, other); lat/cnt = the side's words 0x104754 and
# 0x104758 (the other side's carry sentinels); fl = the side's float 0x104738; o53/o5d/o63 = the other slot's
# +0x53/+0x5D/+0x63 (its character 3 for side 0, whose own slot is character 5; for side 1 the other is slot 0,
# character 5: the words 0xA82EC[3] = 4 and 0xA8300[3] = 0x78, 0xA8300[5] = 0x6E); st = the own +0x57.
def p2_22638(cid, side, st, cmd=(0, 0), lat=0, cnt=0x200, fl=2.0, o53=0x0A, o5d=7, o63=0, stub=None):
    own, oth = DS_SLOTS + side * 0x94, DS_SLOTS + (1 - side) * 0x94
    words = [cmd[0], cmd[1]] if side == 0 else [cmd[1], cmd[0]]
    lw, cw = [0x5454, 0x5656], [0x5858, 0x5A5A]
    lw[side], cw[side] = lat, cnt
    fls = [f32(1.5), f32(2.5)]
    fls[side] = f32(fl)
    oth_bytes = bytearray(range(0x53, 0x64))
    oth_bytes[0] = o53
    oth_bytes[0x5D - 0x53] = o5d
    oth_bytes[0x63 - 0x53] = o63
    return Case(cid, {"eax": E3_SLOT, "edx": E3_OUT, "ebx": side},
                {**SLOT_PTRS, DS_SLOTS + 0x7A: b"\x05", DS_SLOTS + 0x94 + 0x7A: b"\x03",
                 0x1088E0: le32(words[0])[:2] + le32(words[1])[:2],
                 0x104754: b"".join(le32(v)[:2] for v in lw + cw), 0x104738: fls[0] + fls[1],
                 own + 0x52: bytes([0x52, 0x53, 0x54, 0x55, 0x56, st]), own + 0x8A: b"\x8a",
                 oth + 0x53: bytes(oth_bytes)}, {} if stub is None else stub)


P2_SPECS += [
    # m2: the word +0x88 = -1 against 3: signed `jge` returns; unsigned it would step +0x57. m3/m4/m5: state 1
    # with the pointer's character 1, 6 and 2; m6 a zero pointer; m9 rec+0x51 = 1 reads DS_001077AC.
    Spec("fighter_212cc", 0x212CC, [
        p2_212cc("m0", 0, 0x0004), p2_212cc("m1", 0, 0x0003), p2_212cc("m2", 0, 0xFFFF),
        p2_212cc("m3", 1, char=1), p2_212cc("m4", 1, char=6), p2_212cc("m5", 1, char=2),
        p2_212cc("m6", 1, ptrs=(0, E3_REC2)), p2_212cc("m7", 2), p2_212cc("m8", 0xFF),
        p2_212cc("m9", 1, ridx=1, ptrs=(0, E3_REC2), char=6),
    ], allow_calls=(0x33950,), calls=(HIT_B,), eax_mask=0, mutants=("@mutant", "@signed", "@side")),
    # p0..p15 (record §P2.9): the frame count's step and the two bounds, the +0x5D drain (the other side's stick
    # or its +0x63) and floor, the float's -0.7/+0.1 with the 1.0 and 3.0 clamps, the latch, and each state.
    # p14: the count 0x8000 + 1 is negative (signed: 0x78 > it, +0x5D floored to 1); p15: +0x5D = 0x80 is 128
    # against 4 (a signed byte would read -128 and zero it). pG: the count 0x71 lies between the own character 5's
    # 0xA8300 word (0x6E) and the other's character 3 (0x78): only the other slot's floors +0x5D (0) to 1.
    Spec("fighter_22638", 0x22638, [
        p2_22638("p0", 0, 0, cnt=0x14, o5d=0, fl=2.0),
        p2_22638("p1", 0, 0, cmd=(1, 0x10), cnt=0x13, o5d=9, fl=1.5),
        p2_22638("p2", 0, 1, cmd=(0x0C, 0), o5d=3, o63=1, fl=2.95),
        p2_22638("p3", 0, 2, o5d=0),
        p2_22638("p4", 0, 2, o53=0x09),
        p2_22638("p5", 0, 2, cmd=(1, 0), fl=2.0),
        p2_22638("p6", 0, 2, cmd=(4, 0)),
        p2_22638("p7", 0, 2, cmd=(2, 0)),
        p2_22638("p8", 0, 2, cmd=(8, 0)),
        p2_22638("p9", 0, 2),
        p2_22638("pA", 0, 2, lat=6),
        p2_22638("pB", 0, 3),
        p2_22638("pC", 0, 8),
        p2_22638("pD", 1, 2, cmd=(1, 0), fl=1.2, stub={0x2C3FC: 0}),
        p2_22638("pE", 0, 0, cnt=0x8000, o5d=0),
        p2_22638("pF", 1, 3, cmd=(0, 0x20), o5d=0x80),
        p2_22638("pG", 0, 3, cnt=0x70, o5d=0),
    ], allow_calls=(0x33950,), calls=(ANIM_BEGIN, ANIM54, VOICE), eax_mask=0,
       mutants=("@mutant", "@signed", "@byte5d", "@char")),
]

# ---- track P batch 3: the move callbacks 0x475EC..0x489A0 and the callbacks they store (record
# 2026-10-03-reverse-p3) --------------------------------------------------------------------------------
# Every member is character 2's: a move callback (the move-table dwords 0xA4004..0xA42AC) or a callback one of them
# stores. A move callback runs as 0x34E2C calls it at 0x35045 (EAX = slot, EDX = rec, EBX = side), mask 0 (record
# 2026-10-02-reverse-p2 §P2.2).
# 0x35838 (fighter_state_35838): EAX = slot, EDX = rec, EBX = the direction bits (`mov edx,ebx` at 0x3583D); a
# plain `ret`; it clobbers EBX and EDX (E.callee_clobbers).
DIRS = E.Call(0x35838, ("eax", "edx", "ebx"), clobbers=("ebx", "edx"))


# 0x475EC/0x47608 (record §P3.3) write the EDX record's +0x43 and word +0x34, the word negated when 0x1A570, whose AL
# they test, returns 1. 0x1A570's argument is EBX = side (`mov eax,ebx`), not rec+0x51: every case has rec+0x51 = 1,
# and s0/s2 run side 0. The stub's AL varies per case.
P3_REC_SEED = {E3_REC + 0x34: b"\x34\x34", E3_REC + 0x43: b"\x43", E3_REC + 0x51: b"\x01"}


def p3_speed(name, entry, mutants=("@mutant",)):
    return Spec(name, entry, [
        Case("s0", {"eax": E3_SLOT, "edx": E3_REC, "ebx": 0}, P3_REC_SEED, {0x1A570: 0}),
        Case("s1", {"eax": E3_SLOT, "edx": E3_REC, "ebx": 1}, P3_REC_SEED, {0x1A570: 1}),
        Case("s2", {"eax": E3_SLOT, "edx": E3_REC, "ebx": 0}, P3_REC_SEED, {0x1A570: 1}),
    ], calls=(BIT15,), eax_mask=0, mutants=mutants)


# 0x48964/0x489A0 (record §P3.3): the slot's +0x41 bit 6 refuses (q0: 0x40 alone; q1/q2 0xBF, every other bit);
# else the bit is set and 0x35838(slot, rec, 0x2000 or 0x1000 by 0x1A570(rec+0x51)'s AL). EBX is not read (`mov
# bl,[ecx+0x41]` overwrites it): q2 runs side 0 with rec+0x51 = 1, q3 with rec+0x51 = 0x80 (`xor eax,eax; mov
# al,[edx+0x51]` zero-extends the byte: only a sign-extending read passes 0xFFFFFF80 to 0x1A570).
def p3_dirs(name, entry, mutants):
    return Spec(name, entry, [
        Case("q0", {"eax": E3_SLOT, "edx": E3_REC, "ebx": 0}, {E3_SLOT + 0x41: b"\x40", E3_REC + 0x51: b"\x00"}),
        Case("q1", {"eax": E3_SLOT, "edx": E3_REC, "ebx": 0}, {E3_SLOT + 0x41: b"\xbf", E3_REC + 0x51: b"\x00"},
             {0x1A570: 0}),
        Case("q2", {"eax": E3_SLOT, "edx": E3_REC, "ebx": 0}, {E3_SLOT + 0x41: b"\xbf", E3_REC + 0x51: b"\x01"},
             {0x1A570: 1}),
        Case("q3", {"eax": E3_SLOT, "edx": E3_REC, "ebx": 0}, {E3_SLOT + 0x41: b"\xbf", E3_REC + 0x51: b"\x80"},
             {0x1A570: 1}),
    ], calls=(BIT15, DIRS), eax_mask=0, mutants=mutants)


P3_SPECS = [
    p3_speed("fighter_475ec", 0x475EC, ("@mutant", "@side")),
    p3_speed("fighter_47608", 0x47608),
    # 0x47624: 0x2BC30(rec, 0xED79A, 4.0), then the slot 9/8/0 (each seeded otherwise).
    Spec("fighter_47624", 0x47624, [
        Case("n0", {"eax": E3_SLOT, "edx": E3_REC, "ebx": 0}, {E3_SLOT + 0x52: b"\x52\x53\x54"}),
        Case("n1", {"eax": E3_SLOT, "edx": E3_REC2, "ebx": 1}, {E3_SLOT + 0x52: b"\x52\x53\x54"}),
    ], calls=(ANIM_BEGIN,), eax_mask=0),
    # both with the same four mutants: the directions swapped (@mutant on 0x48964, @swap on 0x489A0), the +0x41 bit
    # set after the 0x1A570 call (@late, @mutant), 0x1A570 on EBX (@side), the byte sign-extended (@sext)
    p3_dirs("fighter_48964", 0x48964, ("@mutant", "@late", "@side", "@sext")),
    p3_dirs("fighter_489a0", 0x489A0, ("@mutant", "@swap", "@side", "@sext")),
]

# The callees 0x47688 stubs (record §P3.4), args from their bytes, clobbers from E.callee_clobbers: 0x3B298 EAX =
# side, EDX = a byte (`mov ecx,edx; ...; mov edx,eax`; it returns AL); 0x39FB0 EAX = slot (pushes EBX/ECX/EDX);
# 0x3A95C EAX = side, EDX = a byte. All plain `ret`.
DISPATCH = E.Call(0x3B298, ("eax", "edx"), clobbers=("edx", "edi", "ebp"))
PIVOT = E.Call(0x39FB0, ("eax",))
STANCE = E.Call(0x3A95C, ("eax", "edx"), clobbers=("edx",))


# 0x47720 (record §P3.4) builds its context from the EDX record (0x339AC: ctx[0] = rec+0x51, allowed) and arms that
# side's slot: the EAX slot and EBX are not read (EBX = the other side in every case, EDX = E3_OUT, whose +0x51 names
# the side), and the started record is ctx[4], the slot's own (SLOT_PTRS), not EDX's.
def p3_47720(cid, side):
    own = DS_SLOTS + side * 0x94
    return Case(cid, {"eax": E3_SLOT, "edx": E3_OUT, "ebx": 1 - side},
                {**SLOT_PTRS, E3_OUT + 0x51: bytes([side]), own + 0x0C: le32(0x0C0C0C0C), own + 0x18: le32(0x18181818),
                 own + 0x1C: le32(0x1C1C1C1C), own + 0x52: b"\x52\x53"})


# 0x476FC (the +0x0C callback 0x47720 stores; 0x3531C case 7, (slot, rec, side), EAX unread): the EDX record's
# +0x63, zero-extended (`and edx,0xff`), against 5 (`jl`): c0 4 (nothing), c1 5, c2 0x80 (a signed byte would
# refuse). The slot's own +0x63 is seeded 0 so a read of it differs.
def p3_476fc(cid, b63):
    return Case(cid, {"eax": E3_SLOT, "edx": E3_REC, "ebx": 0},
                {E3_SLOT + 0x0C: le32(0x0C0C0C0C), E3_SLOT + 0x18: le32(0x18181818), E3_SLOT + 0x1C: le32(0x1C1C1C1C),
                 E3_SLOT + 0x63: b"\x00", E3_REC + 0x63: bytes([b63])})


# The +0x18 hooks 0x47648 and 0x477A8 (identical bodies; 0x19020, fn(side), EAX tested whole): flags 1 and 8 = 0,
# 0 = 1, and EBX = ECX = 0 for 0x18C14's two box tables (`xor ecx,ecx` before 0x33950 and `xor ebx,ebx` before
# 0x18BD4, which both keep them). The stub's EAX is returned.
def p3_hook0(cid, side, stub):
    return Case(cid, {"eax": side}, SLOT_PTRS, {0x18C14: stub})


# 0x47688 (the +0x1C callback 0x47720 stores; 0x193B0, fn(side)): the slots' bytes +0x52..+0x5F carry different
# sentinels (slot 0 0x52.., slot 1 0xD2..: the own +0x5F is the byte 0x3B298 and 0x39834 take), the other slot's
# +0x54 selects 0x39FB0 (2) or 0x3A95C (h5: 0x66, the unit run's value; the others 0x54/0xD4); the signed word
# 0xBEDD8 (10 in the image, `sar 0x10` of the dword 0xBEDD6) is poked negative in h4, where a zero-extended read
# differs. 0x3B298's AL alone is tested (`test al,al`): h3's stub EAX 0x100 has AL 0 (plan P3 Task 3 review).
def p3_47688(cid, side, al, o54=None, timer=None):
    pokes = {**SLOT_PTRS, DS_SLOTS + 0x52: bytes(range(0x52, 0x60)), DS_SLOTS + 0x94 + 0x52: bytes(range(0xD2, 0xE0))}
    if o54 is not None:
        pokes[DS_SLOTS + (1 - side) * 0x94 + 0x54] = bytes([o54])
    if timer is not None:
        pokes[0xBEDD8] = le32(timer)[:2]
    return Case(cid, {"eax": side}, pokes, {0x3B298: al})


P3_SPECS += [
    Spec("fighter_47720", 0x47720, [p3_47720("e0", 0), p3_47720("e1", 1)],
         allow_calls=(0x339AC,), calls=(HIT_B,), eax_mask=0, mutants=("@mutant", "@ebx")),
    Spec("fighter_476fc", 0x476FC, [p3_476fc("c0", 4), p3_476fc("c1", 5), p3_476fc("c2", 0x80)],
         eax_mask=0, mutants=("@mutant", "@sext")),
    Spec("fighter_47648", 0x47648, [p3_hook0("k0", 0, 0), p3_hook0("k1", 1, 0x12345678)],
         allow_calls=(0x33950, 0x18BD4), calls=(CHECKS,), mutants=("@mutant",)),
    Spec("fighter_47688", 0x47688, [
        p3_47688("h0", 0, 1), p3_47688("h1", 0, 0, o54=2), p3_47688("h2", 1, 0), p3_47688("h3", 1, 0x100, o54=2),
        p3_47688("h4", 0, 0, timer=0xFFF0), p3_47688("h5", 1, 0, o54=0x66),
    ], allow_calls=(0x33950,), calls=(DISPATCH, POSE, PIVOT, STANCE, TIMER), eax_mask=0,
       mutants=("@mutant", "@pivot", "@zext", "@eax")),
]

# The callees the 0x47874 family stubs (record §P3.5): 0x3C190 EAX = side, EDX = the speed (`mov ebx,eax` before
# 0x1A570, EDX read after); 0x3B714 EAX = the other slot, EDX = the own slot (`mov esi,eax; mov ebp,edx`). Both
# plain `ret`, both clobber EDX.
SPEED = E.Call(0x3C190, ("eax", "edx"), clobbers=("edx",))
REACT = E.Call(0x3B714, ("eax", "edx"), clobbers=("edx",))
P3_SLOT_CBS = {E3_SLOT + 0x0C: b"".join(le32(v * 0x01010101) for v in (0x0C, 0x10, 0x14, 0x18, 0x1C)),
               E3_SLOT + 0x52: b"\x52\x53\x54"}


# 0x47874 (record §P3.5): the EDX record on 0xED974 at 2.0, the EAX slot 9/7/0 with four callbacks (+0x0C 0x47830,
# +0x18 0x477A8, +0x1C 0x477E8, +0x14 0x47798), then 0x3C190(rec+0x51, 0x80) and the voice 0x4B. EBX is not read:
# every case has EBX = 1 - rec+0x51 (v2's 0x80 is read zero-extended, `xor eax,eax; mov al,[esi+0x51]`).
def p3_47874(cid, r51, voice_al=1):
    return Case(cid, {"eax": E3_SLOT, "edx": E3_REC, "ebx": (1 - r51) & 0xFFFFFFFF},
                {**P3_SLOT_CBS, E3_REC + 0x51: bytes([r51])}, {0x2C3FC: voice_al})


# 0x47830 (the +0x0C callback; 0x3531C case 7): the command word DS_001088E0[rec+0x51] (the whole byte index); with
# both bits 0x100 and 0x800 set (`xor dl,dl; and dh,9; cmp edx,0x900`) nothing, else the record on 0xED9A4 at 2.0
# and the slot's +0x0C/+0x14 = 0. EBX is 1 - rec+0x51, the other word a sentinel that takes the other branch. z5's
# rec+0x51 = 0x80 (read zero-extended: its word is at +0x100, the two near ones the other word).
def p3_47830(cid, r51, own, other):
    words = [own, other] if r51 == 0 else [other, own]
    seed = le32(words[0])[:2] + le32(words[1])[:2]
    if r51 > 1:
        seed, far = le32(other)[:2] * 2, {0x1088E0 + 2 * r51: le32(own)[:2]}
    else:
        far = {}
    return Case(cid, {"eax": E3_SLOT, "edx": E3_REC, "ebx": (1 - r51) & 0xFFFFFFFF},
                {**P3_SLOT_CBS, E3_REC + 0x51: bytes([r51]), 0x1088E0: seed, **far})


# 0x477E8 (the +0x1C callback; 0x193B0, fn(side)): 0x3B714(the other slot, the own slot), the own record on 0xED9A4
# at 2.0, the own slot's +0x0C/+0x14 = 0 (both slots' +0x0C..+0x1F seeded).
def p3_477e8(cid, side):
    return Case(cid, {"eax": side},
                {**SLOT_PTRS, DS_SLOTS + 0x0C: b"".join(le32(v * 0x01010101) for v in (0x0C, 0x10, 0x14, 0x18, 0x1C)),
                 DS_SLOTS + 0x94 + 0x0C: b"".join(le32(v * 0x01010101) for v in (0x8C, 0x90, 0x94, 0x98, 0x9C))})


P3_SPECS += [
    Spec("fighter_47874", 0x47874, [p3_47874("v0", 0), p3_47874("v1", 1, 0), p3_47874("v2", 0x80)],
         calls=(HIT_B, SPEED, VOICE), eax_mask=0, mutants=("@mutant", "@side", "@sext", "@early")),
    Spec("fighter_47830", 0x47830, [
        p3_47830("z0", 0, 0x0900, 0), p3_47830("z1", 0, 0x0100, 0x0900), p3_47830("z2", 1, 0x0800, 0x0900),
        p3_47830("z3", 1, 0xF6FF, 0x0900), p3_47830("z4", 0, 0xFFFF, 0), p3_47830("z5", 0x80, 0x0900, 0x0100),
    ], calls=(ANIM_BEGIN,), eax_mask=0, mutants=("@mutant", "@side", "@order", "@sext")),
    # 0x47798 (the +0x14 callback; 0x1952F/0x3514C/0x350B8, fn(slot) with EAX = EDX = the slot, the whole EAX
    # tested): the voice 0x4C, then EAX = 1 (`mov eax,1` over the voice's EAX: w1's stub returns 0).
    Spec("fighter_47798", 0x47798, [
        Case("w0", {"eax": E3_SLOT, "edx": E3_SLOT}),
        Case("w1", {"eax": E3_SLOT, "edx": E3_SLOT}, {}, {0x2C3FC: 0}),
    ], calls=(VOICE,), mutants=("@mutant", "@eax")),
    Spec("fighter_477a8", 0x477A8, [p3_hook0("k0", 0, 0), p3_hook0("k1", 1, 0x12345678)],
         allow_calls=(0x33950, 0x18BD4), calls=(CHECKS,), mutants=("@mutant",)),
    Spec("fighter_477e8", 0x477E8, [p3_477e8("y0", 0), p3_477e8("y1", 1)],
         allow_calls=(0x33950,), calls=(REACT, ANIM_BEGIN), eax_mask=0, mutants=("@mutant", "@order")),
]

# 0x47FCC (record §P3.6) reads EBX alone (`mov edx,ebx`; the context 0x33950(side)): the side's dword 0x108370 = 0,
# then the own record on 0xC8950[the own slot's character] at 2.0 and the own slot armed (P2's p2_ctx_case seeds:
# characters 5 and 3, sentinels on +0x0C..+0x1F, +0x41/+0x42, +0x52..+0x57); both sides' dwords 0x108370 seeded.
P3_108370_SEED = {0x108370: le32(0x70707070) + le32(0x74747474)}


# 0x47D24 (the +0x1C callback 0x47FCC stores; 0x193B0, fn(side)): the side's float 0x108378 is 0x2BC30's frame
# (pushed as a dword), then 3.0; the side's byte 0x108394 = 0; the signed word 0xC947E[the other slot's character]
# (`sar 0x10` of the dword 0xC947C + 2c) is 0x3C208's distance, poked negative for character 3 in b2. Both slots'
# bytes +0x42..+0x5F carry different sentinels (the own +0x5F is 0x39834's byte; +0x42 takes bit 2).
def p3_47d24(cid, side, dist=None):
    pokes = {**SLOT_PTRS, DS_SLOTS + 0x7A: b"\x05", DS_SLOTS + 0x94 + 0x7A: b"\x03",
             DS_SLOTS + 0x42: bytes(range(0x42, 0x60)), DS_SLOTS + 0x94 + 0x42: bytes(range(0xC2, 0xE0)),
             0x108378: f32(1.5) + f32(2.5), 0x108394: b"\x94\x95"}
    if dist is not None:
        pokes[0xC947E + 2 * 3] = le32(dist)[:2]
    return Case(cid, {"eax": side}, pokes)


# 0x47E9C (the +0x0C callback 0x47FCC stores; 0x3531C case 7, EAX unread). Its state byte is the EAX slot's +0x57
# (`mov ecx,eax` at 0x47EA0, `mov al,[ecx+0x57]` at 0x47EC9: E3_SLOT here, `st`), its stores go to ctx[2] (the
# side's slot, +0x57 seeded 0x57). Each frame the side's dword 0x108370 + 1 (signed, above 0x3C sets the byte
# 0x108394); 0: the slot's word +0x88 (`sar 0x10` of the dword +0x86) above 3 sets +0x57 = 1; 1: the own record on
# 0xEDA40 at 2.0, +0x57 = 2, +0x8A = 0; 3: the side's float 0x108378 takes -0.1 on the command's bit 0, else +0.1
# on bit 1, then below 1.1 (the double 0x80C74) becomes 1.1f, above 5.0f becomes 5.0f; 2 and above 3 nothing.
def p3_47e9c(cid, side, st, cnt=0x10, w88=0, cmd=0, fl=2.0):
    own = DS_SLOTS + side * 0x94
    cnts, words, fls = [0x70707070, 0x74747474], [0x5A5A, 0xA5A5], [f32(1.5), f32(2.5)]
    cnts[side], words[side], fls[side] = cnt, cmd, f32(fl)
    return Case(cid, {"eax": E3_SLOT, "edx": E3_OUT, "ebx": side},
                {**SLOT_PTRS, E3_SLOT + 0x57: bytes([st]), own + 0x57: b"\x57", own + 0x86: b"\x86\x86" + le32(w88)[:2],
                 own + 0x8A: b"\x8a", 0x108370: le32(cnts[0]) + le32(cnts[1]), 0x108378: fls[0] + fls[1],
                 0x108394: b"\x94\x95", 0x1088E0: le32(words[0])[:2] + le32(words[1])[:2]})


P3_SPECS += [
    Spec("fighter_47fcc", 0x47FCC, [p2_ctx_case("s0", 0, P3_108370_SEED), p2_ctx_case("s1", 1, P3_108370_SEED)],
         allow_calls=(0x33950,), calls=(HIT_B,), eax_mask=0, mutants=("@mutant", "@order")),
    # 0x47CB0 (the +0x18 hook; 0x19020, fn(side), the whole EAX): flags 1, 8, 4, 0xD, 0xE, 7 = 0 and 5 = 1; the own
    # slot's signed word +0x88 in 1..3 (`jg`/`jge` against immediates) calls 0x18C14(side, the flags, 0xC946A,
    # 0xC9474), else 1. k4: the word -1 (signed: below 1).
    Spec("fighter_47cb0", 0x47CB0, [
        p2_2116c("k0", 0, 4), p2_2116c("k1", 0, 3, stub=0), p2_2116c("k2", 1, 1, stub=0x12345678),
        p2_2116c("k3", 1, 0), p2_2116c("k4", 0, 0xFFFF),
    ], allow_calls=(0x33950, 0x18BD4), calls=(CHECKS,), mutants=("@mutant", "@ge", "@lo")),
    Spec("fighter_47d24", 0x47D24, [p3_47d24("b0", 0), p3_47d24("b1", 1), p3_47d24("b2", 0, 0xF000)],
         allow_calls=(0x33950,), calls=(FLASH, ANIM_BEGIN, HIT_A, PLACE, FACING, POSE, HOLD, TIMER, VOICE),
         eax_mask=0, mutants=("@mutant", "@order", "@signed", "@frame")),
    # e0..eF: the count's bound (0x3B + 1 stays, 0x3C + 1 sets; 0x7FFFFFFF + 1 is negative), each state, the word
    # +0x88 (3, 4, -1), the float's two steps (bit 0 first, then bit 1; 0xFFFC has neither), and both clamps.
    Spec("fighter_47e9c", 0x47E9C, [
        p3_47e9c("e0", 0, 0, cnt=0x3B, w88=3),
        p3_47e9c("e1", 0, 0, cnt=0x3C, w88=4),
        p3_47e9c("e2", 1, 1, cnt=0),
        p3_47e9c("e3", 0, 2),
        p3_47e9c("e4", 0, 4),
        p3_47e9c("e5", 0, 3, cmd=1, fl=2.0),
        p3_47e9c("e6", 1, 3, cmd=2, fl=2.0),
        p3_47e9c("e7", 0, 3, cmd=3, fl=2.0),
        p3_47e9c("e8", 0, 3, cmd=0xFFFC, fl=2.0),
        p3_47e9c("e9", 0, 3, cmd=1, fl=1.15),
        p3_47e9c("eA", 1, 3, cmd=2, fl=4.95),
        p3_47e9c("eB", 0, 3, fl=1.0),
        p3_47e9c("eC", 0, 3, fl=6.0),
        p3_47e9c("eD", 0, 2, cnt=0x7FFFFFFF),
        p3_47e9c("eE", 1, 0, w88=0xFFFF),
        p3_47e9c("eF", 0, 3, cmd=0x0101, fl=3.0),
    ], allow_calls=(0x33950,), calls=(ANIM_BEGIN,), eax_mask=0, mutants=("@mutant", "@slot", "@signed", "@order")),
]

# The callees the 0x48608 family stubs (record §P3.7), args from their bytes, clobbers from E.callee_clobbers:
# 0x48170 EAX = side (it saves every register it writes); 0x3C148 EAX = side; 0x468D8 EAX = side (it returns
# AL); 0x36D98 EAX = slot; 0x188DC EAX = side, EDX = x (clobbers EDX). All plain `ret`.
ARM170 = E.Call(0x48170, ("eax",))
CLEAR34 = E.Call(0x3C148, ("eax",))
PRED = E.Call(0x468D8, ("eax",))
RESET = E.Call(0x36D98, ("eax",))
ANCHORX = E.Call(0x188DC, ("eax", "edx"), clobbers=("edx",))


# 0x48608 (record §P3.7): the EDX record on 0xED834 at 2.0, 0x3C190(rec+0x51, 0x78), the record's +0x42 = 0x1E, the
# EAX slot 9/7/0 with +0x57 = 0 and three callbacks (+0x0C 0x4844C, +0x18 0x48054, +0x1C 0x480B4), and the word
# 0x10838C[rec+0x51] = 0 (EBX = rec+0x51, loaded before both calls, which keep it). EBX at entry is not read
# (1 - rec+0x51 in every case).
def p3_48608(cid, r51):
    # r2's rec+0x51 = 0x80 is read zero-extended (`xor eax,eax; mov al,[edx+0x51]`; a sign-extending read passes
    # 0xFFFFFF80 to 0x3C190 and indexes the word elsewhere): its word is at 0x10838C + 0x100, sentinel 0x9090
    far = {0x10838C + 2 * r51: b"\x90\x90"} if r51 > 1 else {}
    return Case(cid, {"eax": E3_SLOT, "edx": E3_REC, "ebx": (1 - r51) & 0xFFFFFFFF},
                {**P3_SLOT_CBS, E3_SLOT + 0x55: b"\x55\x56\x57", E3_REC + 0x42: b"\x42", E3_REC + 0x51: bytes([r51]),
                 0x10838C: b"\x8c\x8c\x8e\x8e", **far})


# 0x48054 (the +0x18 hook; 0x19020, fn(side), the whole EAX): flags 1, 8, 4, 0xD = 0, 5 and 9 = 1; 0x18C14(side,
# the flags, EBX = 0xC9492, ECX = 0xC949C: both loaded before 0x33950/0x18BD4, which keep them); with the own slot's
# +0x57 non-zero the result is replaced by 1.
def p3_48054(cid, side, st, stub):
    # the other slot's +0x57 is the opposite (0x55 when the own is clear, 0 when set): a read of either slot
    # alone, or of the other, changes the result in some case
    return Case(cid, {"eax": side},
                {**SLOT_PTRS, DS_SLOTS + side * 0x94 + 0x57: bytes([st]),
                 DS_SLOTS + (1 - side) * 0x94 + 0x57: b"\x00" if st else b"\x55"}, {0x18C14: stub})


# 0x480B4 (the +0x1C callback; 0x193B0, fn(side)): 0x3B298(the other side, the own +0x5F) (its AL unread),
# 0x39A10(each record, 0x309), 0x48170(side), 0x3C208(the other side, the signed word 0xC94A6[the other slot's
# character], `sar 0x10` of the dword 0xC94A4 + 2c), poked negative for character 3 in x2.
def p3_480b4(cid, side, al, d3=0x0123, d5=0x0456):
    # the words of both characters (slot 0 is 5, slot 1 is 3) differ, so the other slot's character is told
    # from the own and from a fixed one on both sides; x2/x3 poke the used word negative
    pokes = {**SLOT_PTRS, DS_SLOTS + 0x7A: b"\x05", DS_SLOTS + 0x94 + 0x7A: b"\x03",
             DS_SLOTS + 0x52: bytes(range(0x52, 0x60)), DS_SLOTS + 0x94 + 0x52: bytes(range(0xD2, 0xE0)),
             0xC94A6 + 2 * 3: le32(d3)[:2], 0xC94A6 + 2 * 5: le32(d5)[:2]}
    return Case(cid, {"eax": side}, pokes, {0x3B298: al})


# 0x48170 (called by 0x480B4 at 0x480F6, EAX = side; 0x480FB reloads EAX): the side's word 0x108388 = 0, 0x3C148 on
# both sides, the own record on 0xED850 at 3.0 (0x3C480), the own slot's +0x57 = 2, 0x468D8(the other side) and on
# its AL 0x36D98(the other slot), the other record on 0xC90F8[its character] at 3.0, 0x188DC(the other side, the
# other slot's +0x2C read before that call), then the other slot 0x10/0xA/0 with the +0x10 handler 0x4811C (0x3531C
# case 10), +0x58 = 0, and the side's byte 0x108392 = (the other slot's +0x43 & 0x30) != 0. Both slots' +0x10..+0x13,
# +0x2C and +0x43 differ, the per-side words and bytes carry sentinels. g4's stub EAX 0x100 (AL 0, upper bits set):
# 0x481F0 tests AL only (`test al,al`), so no 0x36D98 call follows (plan P3 Task 6 review).
def p3_48170(cid, side, al, o43):
    oth = DS_SLOTS + (1 - side) * 0x94
    return Case(cid, {"eax": side},
                {**SLOT_PTRS, DS_SLOTS + 0x7A: b"\x05", DS_SLOTS + 0x94 + 0x7A: b"\x03",
                 DS_SLOTS + 0x10: le32(0x10101010), DS_SLOTS + 0x94 + 0x10: le32(0x90909090),
                 DS_SLOTS + 0x2C: le32(0x2C2C2C2C), DS_SLOTS + 0x94 + 0x2C: le32(0xACACACAC),
                 DS_SLOTS + 0x43: b"\x43", DS_SLOTS + 0x94 + 0x43: b"\xc3", oth + 0x43: bytes([o43]),
                 DS_SLOTS + 0x52: bytes(range(0x52, 0x59)), DS_SLOTS + 0x94 + 0x52: bytes(range(0xD2, 0xD9)),
                 0x108388: b"\x88\x88\x8a\x8a", 0x108392: b"\x92\x93"}, {0x468D8: al})


# 0x4811C (the +0x10 handler 0x48170 stores; 0x3531C case 10 at 0x354E2: EAX = slot, EDX = the slot's record loaded
# at 0x35396, EBX = side; the case's `ret` leaves EAX unread): by the slot's +0x58: 0 nothing; 1 the word
# 0x108380[side] = 0 and +0x58 = 2; 2 the word + 1 and, above 0xF (signed: `sar 0x10` of the dword 0x10837E + 2 *
# side), +0x54 = 0 and 0x36870(rec); above 2 nothing. i5: the word 0x7FFF + 1 is negative.
def p3_4811c(cid, side, st, w):
    words = [0x8080, 0x8282]
    words[side] = w
    return Case(cid, {"eax": E3_SLOT, "edx": E3_REC, "ebx": side},
                {E3_SLOT + 0x54: b"\x54", E3_SLOT + 0x58: bytes([st]),
                 0x108380: le32(words[0])[:2] + le32(words[1])[:2]})


P3_SPECS += [
    Spec("fighter_48608", 0x48608, [p3_48608("r0", 0), p3_48608("r1", 1), p3_48608("r2", 0x80)],
         calls=(HIT_B, SPEED), eax_mask=0, mutants=("@mutant", "@order", "@side")),
    Spec("fighter_48054", 0x48054, [p3_48054("n0", 0, 0, 0), p3_48054("n1", 1, 0, 0x12345678),
                                    p3_48054("n2", 0, 3, 0),
                                    p3_48054("n3", 1, 3, 0x0F0F0F0F)],
         allow_calls=(0x33950, 0x18BD4), calls=(CHECKS,), mutants=("@mutant", "@eax")),
    Spec("fighter_480b4", 0x480B4, [p3_480b4("x0", 0, 0), p3_480b4("x1", 1, 1), p3_480b4("x2", 0, 0, 0xF000),
                                    p3_480b4("x3", 1, 1, d5=0xF456)],
         allow_calls=(0x33950,), calls=(DISPATCH, TIMER, ARM170, PLACE), eax_mask=0,
         mutants=("@mutant", "@signed", "@char")),
    Spec("fighter_48170", 0x48170, [p3_48170("g0", 0, 0, 0x10), p3_48170("g1", 1, 1, 0x20), p3_48170("g2", 0, 0, 0xCF),
                                    p3_48170("g3", 0, 1, 0x30), p3_48170("g4", 1, 0x100, 0x10)],
         allow_calls=(0x33950,), calls=(CLEAR34, HIT_A, PRED, RESET, ANCHORX), eax_mask=0,
         mutants=("@mutant", "@order", "@reset", "@al")),
    Spec("fighter_4811c", 0x4811C, [
        p3_4811c("i0", 0, 0, 0x0F), p3_4811c("i1", 1, 1, 0x0F), p3_4811c("i2", 0, 2, 0x0F), p3_4811c("i3", 1, 2, 0x0E),
        p3_4811c("i4", 0, 3, 0x0F), p3_4811c("i5", 0, 2, 0x7FFF), p3_4811c("i6", 1, 2, 0x0F),
    ], calls=(ANIM54,), eax_mask=0, mutants=("@mutant", "@signed", "@order")),
]

# 0x3C16C (fighter_3c16c): EAX = side; it saves EDX, the one register it writes (record §P3.8).
CLEAR36 = E.Call(0x3C16C, ("eax",))


# 0x4844C (the +0x0C callback 0x48608 stores; 0x3531C case 7, EAX unread; record §P3.8). EBX = side, the context
# 0x33950(side). Each frame the side's words 0x10838C and 0x108388 + 1. By the own slot's +0x57 (table 0x48438):
# 0: the own record's word +0x34 in absolute value above 0x15E clears its +0x42; once the count 0x10838C (signed)
# exceeds 0x1E, +0x54 = 0 and 0x36870(the own record); 2: the count 0x108388 against the five (key, voice) words of
# 0xC94CE (the other slot's +0x43 & 0x30) or 0xC94BA, a voice on the equal key; 3: the own slot's word +0x74 = 0, the
# record's +0x28 bit 5, the side's word 0x108384 = the slot's word +0x2C; with the signed word 0xBD884[the own
# character] above the slot's dword +0x30 (signed): +0x54 = 0, 0x3C148(side), 0x3C16C(side), 0x188AC(side, the
# record's +0x18, 0), the record on 0xC8B58[the own character] at 3.0, 0x188DC(side, that word 0x108384, signed) and
# +0x57 = 4; 1, 4 and above nothing. The other side's words carry sentinels; the slot's +0x2C/+0x30 and the record's
# +0x18/+0x28/+0x34/+0x42 are seeded per case.
def p3_4844c(cid, side, st, w34=0x100, cnt=(0x10, 0x10), o43=0, x2c=0x2C2C2C2C, x30=0x30303030, stub=None, bd=None, tbl=None):
    # diffrun takes 16 pokes per case, 64 bytes each: each slot is three buffers (+0, +0x2C..+0x43, +0x54..+0x7A)
    # and each record one (+0x18..+0x43)
    w84, w88, w8c = [0x8484, 0x8686], [0x8888, 0x8A8A], [0x8C8C, 0x8E8E]
    w88[side], w8c[side] = cnt
    pokes = {0x108384: b"".join(le32(v)[:2] for v in w84 + w88 + w8c)}
    for k, (rec, ch) in enumerate(((E3_REC, 5), (E3_REC2, 3))):
        own = k == side
        mid, hi, r = bytearray(0x18), bytearray(0x28), bytearray(0x2C)
        mid[0:8] = le32(x2c) + le32(x30) if own else bytes(8)     # the other slot's +0x2C/+0x30 stay zero
        # the own slot's +0x43 is the opposite of the voice table's key (the other slot's +0x43 & 0x30 is the one read)
        mid[0x17] = o43 if not own else (0x00 if o43 & 0x30 else 0x30)
        hi[0:4] = bytes([0x54, 0x55, 0x56, st]) if own else bytes([0x64, 0x65, 0x66, 0x67])
        # the neighbours of the bytes and words it reads and writes are live state in play (+0x58..+0x5A beside the
        # byte +0x57, +0x76/+0x77 beside the word +0x74, +0x7B beside the character +0x7A, record +0x29 beside +0x28):
        # a width mutant (DSW/DSD for the byte, DSD for the word) must see them
        hi[4:7] = bytes([0x58, 0x59, 0x5A]) if own else bytes([0x68, 0x69, 0x6A])
        hi[0x20:0x22] = b"\x74\x74" if own else b"\x75\x75"
        hi[0x22:0x24] = b"\x76\x77" if own else b"\x78\x79"
        hi[0x26] = ch
        hi[0x27] = 0x7B if own else 0x7C
        r[0:4] = le32(0x18181818 if own else 0x19191919)
        r[0x10:0x12] = b"\x08\x91" if own else b"\x04\x92"
        r[0x1C:0x1E] = le32(w34)[:2] if own else b"\x00\x02"
        r[0x2A] = 0x42 if own else 0x43
        r[0x2B] = 0x4B if own else 0x4C
        pokes[DS_SLOTS + k * 0x94] = le32(rec)
        pokes[DS_SLOTS + k * 0x94 + 0x2C] = bytes(mid)
        pokes[DS_SLOTS + k * 0x94 + 0x54] = bytes(hi)
        pokes[rec + 0x18] = bytes(r)
    pokes.update(tbl or {})
    if bd is not None:
        pokes[0xBD884 + 2 * (5 if side == 0 else 3)] = le32(bd)[:2]
    return Case(cid, {"eax": E3_SLOT, "edx": E3_OUT, "ebx": side}, pokes, {} if stub is None else stub)


P3_SPECS += [
    # a0..aR: state 0's two bounds (|+0x34| 0x100/0x15F/-0x15F/-0x15E; the count 0x1D/0x1E/0x7FFF + 1), state 2's
    # two tables, all ten (key, voice) pairs (aE..aK with a5..a8; 6 matches none) and the table byte's bits outside the
    # mask, state 3's bound (0x1800 against 0x1800, 0x17FF and -1) with a negative word +0x2C (aA), the negative table
    # word 0xBD884 (aO), the dword +0x30's high half (aP), the negative key (aQ) and the duplicated key (aR), and the
    # states that do nothing (a4, aC, aD, the byte 0x82/0x83: aM, aN).
    Spec("fighter_4844c", 0x4844C, [
        p3_4844c("a0", 0, 0, w34=0x100, cnt=(0x10, 0x1D)),
        p3_4844c("a1", 0, 0, w34=0x15F, cnt=(0x10, 0x1E)),
        p3_4844c("a2", 1, 0, w34=0xFEA1, cnt=(0x10, 0x10)),
        p3_4844c("a3", 0, 0, w34=0xFEA2, cnt=(0x10, 0x7FFF)),
        p3_4844c("a4", 0, 1),
        p3_4844c("a5", 0, 2, cnt=(1, 0), o43=0x10),
        p3_4844c("a6", 1, 2, cnt=(0x18, 0), o43=0x00),
        p3_4844c("a7", 0, 2, cnt=(0x37, 0), o43=0x20, stub={0x2C3FC: 0}),
        p3_4844c("a8", 0, 2, cnt=(5, 0), o43=0x10),
        p3_4844c("a9", 0, 3, x30=0x1800),
        p3_4844c("aA", 0, 3, x2c=0x1234F000, x30=0x17FF),
        p3_4844c("aB", 1, 3, x30=0xFFFFFFFF),
        p3_4844c("aC", 0, 4),
        p3_4844c("aD", 1, 5),
        # every (key, voice) pair of both tables (CE: 2, 4, 25, 0x33, 0x38; BA: the same keys), the byte +0x43 & 0x30
        # on bits outside the mask (0xCF, 0x0F: BA) and both bits (0x30: CE), side 1's own count
        p3_4844c("aE", 0, 2, cnt=(3, 0), o43=0x10),
        p3_4844c("aF", 1, 2, cnt=(0x32, 0), o43=0xCF),
        p3_4844c("aG", 0, 2, cnt=(0x18, 0), o43=0x30),
        p3_4844c("aH", 1, 2, cnt=(0x32, 0), o43=0x20),
        p3_4844c("aI", 0, 2, cnt=(1, 0), o43=0xCF),
        p3_4844c("aJ", 1, 2, cnt=(3, 0), o43=0xCF),
        p3_4844c("aK", 0, 2, cnt=(0x37, 0), o43=0x0F),
        # state 0 on side 1 reaching its end (0x36870 on the other record); the state byte's high bits (0x82, 0x83)
        p3_4844c("aL", 1, 0, w34=0x100, cnt=(0x10, 0x1E)),
        p3_4844c("aM", 0, 0x82, cnt=(1, 0), o43=0x10),
        p3_4844c("aN", 0, 0x83, x30=0x17FF),
        # state 3's bound: the word 0xBD884[char] negative (0xF000 against -1: not above), and the dword +0x30
        # with its high half set (0x117FF: the low word alone would end the move)
        p3_4844c("aO", 0, 3, x30=0xFFFFFFFF, bd=0xF000),
        p3_4844c("aP", 0, 3, x30=0x000117FF),
        # the key compared by `movsx` (the table's first key 0xFFFE = -2 against the count 0xFFFD + 1) and the scan going
        # on after a match (the other table's second key made 2 as well: two voices)
        p3_4844c("aQ", 0, 2, cnt=(0xFFFD, 0), o43=0x10, tbl={0xC94CE: b"\xFE\xFF\x7B\x00"}),
        p3_4844c("aR", 1, 2, cnt=(1, 0), o43=0x00, tbl={0xC94BE: b"\x02\x00\x6A\x00"}),
    ], allow_calls=(0x33950,), calls=(ANIM54, VOICE, CLEAR34, CLEAR36, ANCHOR, ANIM_BEGIN, ANCHORX), eax_mask=0,
       mutants=("@mutant", "@signed", "@abs", "@order", "@bound", "@zext", "@width")),
]


# ---- track P batch 6: animation targets C (record 2026-10-03-reverse-p6) -----
# Every member is an animation target (EAX = rec, EDX = the stream opcode's operand; only 0x2BDA0
# reads the operand) except 0x24220, the slot +0x10 handler 0x24338 stores (EAX = slot, EDX = its
# record, EBX = side). The callees the batch stubs, args in the port's C order and clobbers from
# E.callee_clobbers (record §P6.2); 0x39F40 is the only `ret 4`.
P6_39280 = E.Call(0x39280, ("eax",))
P6_39F40 = E.Call(0x39F40, ("eax", "edx", "ebx", "ecx", "s0"), pop=4, clobbers=("ebx", "edx"))
P6_13244 = E.Call(0x13244, ())
P6_2A148 = E.Call(0x2A148, ("eax", "edx"), clobbers=("edx",))
P6_2BCF4 = E.Call(0x2BCF4, ("eax", "edx"), clobbers=("edx",))
P6_1890C = E.Call(0x1890C, ("eax", "edx"), clobbers=("edx",))
P6_RNG   = E.Call(0x5D7DC, ("eax",), mode="real")
P6_29BC8 = E.Call(0x29BC8, ("eax", "ebx", "edx"), clobbers=("ebx", "edx"))
P6_37D18 = E.Call(0x37D18, ("eax", "edx"), clobbers=("edx",))
P6_13C70 = E.Call(0x13C70, ("eax", "edx", "ebx"), clobbers=("ebx", "edx"))
P6_3AA54 = E.Call(0x3AA54, ("eax",))
P6_29C08 = E.Call(0x29C08, ("eax", "edx"), clobbers=("edx",))

# The slot-pointer table 0x1077A8 the functions that do not build the ctx read (it normally holds the
# slot array addresses; record §P6.2).
P6_PTRS = {DS_SLOTS - 8: le32(DS_SLOTS) + le32(DS_SLOTS + 0x94)}
DS_104AE9 = 0x104AE9           # DS_00104AE9: 0x40148 clears bit 2
DS_104738 = 0x104738           # DS_00104738: [side] float, the 0x22494 frame
DS_104748 = 0x104748           # DS_00104748: the record pointer 0x2400C's +0x29 and seek use
DS_108394 = 0x108394           # DS_00108394: [side] byte, 0x47E30's branch
DS_108378 = 0x108378           # DS_00108378: [side] float, 0x47E30's frame
P6_CHILD = 0x10A900            # zero BSS of the image: the 0x22A40 spawn's child
P6_PSET = 0x10A800             # zero BSS of the image: the 0x45C98 pset base


def _p6_fill(size, patches, fill=0xA5):
    """A `size`-byte buffer of `fill` with `patches` (offset -> bytes): a record
    region with sentinels in the gaps, so a too-wide read or write shows against
    the fill (record §P6.2)."""
    buf = bytearray([fill]) * size
    for off, data in patches.items():
        buf[off:off + len(data)] = data
    return bytes(buf)


# The 0x24220 slot fixture (record §P6.10): state 0; the slot's +4 and +8 records
# (the state-0 decrement's target and the @plus4 mutant's, both inside the image);
# the anchor x (distinct from the record's +0x2C, so @anchor shows); +0x41 bit 5
# clear with other bits set (so the |= is observable); char 3 (the 0xA8510 stream
# index). P6_SLOT_SEED_EXTRA flips the countdown cases to state 1.
P6_SLOT_SEED = {E3_SLOT + 4: le32(E3_REC2), E3_SLOT + 8: le32(E3_OUT),
                E3_SLOT + 0x2C: le32(0x00001234), E3_SLOT + 0x41: b"\x41",
                E3_SLOT + 0x58: b"\x00", E3_SLOT + 0x7A: b"\x03"}
P6_SLOT_SEED_EXTRA = {E3_SLOT + 0x58: b"\x01"}

P6_SPECS = [
    Spec("anim_2bda0", 0x2BDA0, [
        Case("r0", {"eax": E3_REC, "edx": 1}, {DS_RNG: le32(0), E3_REC + 0x53: b"\x53"}),
        Case("r1", {"eax": E3_REC, "edx": 0xFFFF}, {DS_RNG: le32(0), E3_REC + 0x53: b"\x53"}),
        Case("r2", {"eax": E3_REC, "edx": 0x12340001}, {DS_RNG: le32(0), E3_REC + 0x53: b"\x53"}),
        Case("r3", {"eax": E3_REC, "edx": 0}, {DS_RNG: le32(0x12345678), E3_REC + 0x53: b"\x53"}),
    ], calls=(P6_RNG,), eax_mask=0, mutants=("@mutant", "@mask")),
    Spec("fighter_241f4", 0x241F4, [
        Case("s0", {"eax": E3_REC, "edx": 0x1234}, {**SLOT_PTRS, E3_REC + 0x51: b"\x00",
                                                     E3_REC2 + 0x51: b"\x01"}),
        Case("s1", {"eax": E3_REC2, "edx": 0x5678}, {**SLOT_PTRS, E3_REC + 0x51: b"\x00",
                                                      E3_REC2 + 0x51: b"\x01"}),
    ], allow_calls=(0x339AC,), calls=(STANCE, VOICE), eax_mask=0,
       mutants=("@mutant", "@side", "@voice")),
    Spec("fighter_47e04", 0x47E04, [
        Case("s0", {"eax": E3_REC, "edx": 0x1234}, {**SLOT_PTRS, E3_REC + 0x51: b"\x00",
                                                     E3_REC2 + 0x51: b"\x01"}),
        Case("s1", {"eax": E3_REC2, "edx": 0x5678}, {**SLOT_PTRS, E3_REC + 0x51: b"\x00",
                                                      E3_REC2 + 0x51: b"\x01"}),
    ], allow_calls=(0x339AC,), calls=(VOICE, STANCE), eax_mask=0,
       mutants=("@mutant", "@side", "@order")),
    Spec("fighter_40148", 0x40148, [
        Case("o0", {"eax": E3_REC, "edx": 0x55}, {DS_SLOTS - 8: le32(DS_SLOTS) + le32(0),
                                                   E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
                                                   DS_104AE9: b"\xff\x5a\x5b"}),
        Case("o1", {"eax": E3_REC, "edx": 0x55}, {**SLOT_PTRS, **P6_PTRS, E3_REC + 0x51: b"\x00",
                                                   E3_REC2 + 0x51: b"\x01", DS_104AE9: b"\xff\x5a\x5b"}),
        Case("o2", {"eax": E3_REC2, "edx": 0x55}, {**SLOT_PTRS, **P6_PTRS, E3_REC + 0x51: b"\x00",
                                                    E3_REC2 + 0x51: b"\x01", DS_104AE9: b"\xff\x5a\x5b"}),
    ], calls=(P6_37D18,), eax_mask=0, mutants=("@mutant", "@side", "@bit")),
    Spec("fighter_40170", 0x40170, [
        Case("p0", {"eax": E3_REC, "edx": 0x55}, {DS_SLOTS - 8: le32(DS_SLOTS) + le32(0),
                                                   E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
                                                   E3_REC + 0x28: b"\x00\x40"}),
        Case("p1", {"eax": E3_REC, "edx": 0x55}, {**SLOT_PTRS, **P6_PTRS, E3_REC + 0x51: b"\x00",
                                                   E3_REC2 + 0x51: b"\x01", DS_SLOTS + 0x94 + 0x7A: b"\x03",
                                                   E3_REC + 0x28: b"\x00\x40", E3_REC2 + 0x29: b"\x29",
                                                   0xC759C + 6: b"\x34\x12", 0xC759C + 8: b"\x78\x56"}),
        Case("p2", {"eax": E3_REC, "edx": 0x55}, {**SLOT_PTRS, **P6_PTRS, E3_REC + 0x51: b"\x00",
                                                   E3_REC2 + 0x51: b"\x01", DS_SLOTS + 0x94 + 0x7A: b"\x04",
                                                   E3_REC + 0x28: b"\x00\x00", E3_REC2 + 0x29: b"\x00",
                                                   0xC759C + 8: b"\x78\x56"}),
        Case("p3", {"eax": E3_REC2, "edx": 0x55}, {**SLOT_PTRS, **P6_PTRS, E3_REC + 0x51: b"\x00",
                                                    E3_REC2 + 0x51: b"\x01", DS_SLOTS + 0x7A: b"\x02",
                                                    E3_REC2 + 0x28: b"\x00\x40", E3_REC + 0x29: b"\x29",
                                                    0xC759C + 4: b"\xCD\xAB"}),
    ], calls=(FLASH, PLACE), eax_mask=0, mutants=("@mutant", "@side", "@set", "@char")),
    Spec("fighter_22494", 0x22494, [
        Case("s0", {"eax": E3_REC, "edx": 0x1234}, {**SLOT_PTRS, **P6_PTRS, E3_REC + 0x51: b"\x00",
                                                     E3_REC2 + 0x51: b"\x01",
                                                     DS_104738: le32(0x40400000), DS_104738 + 4: le32(0x40A00000),
                                                     DS_SLOTS + 0x5F: b"\x5f", DS_SLOTS + 0x94 + 0x5F: b"\x9f"}),
        Case("s1", {"eax": E3_REC2, "edx": 0x5678}, {**SLOT_PTRS, **P6_PTRS, E3_REC + 0x51: b"\x00",
                                                      E3_REC2 + 0x51: b"\x01",
                                                      DS_104738: le32(0x40400000), DS_104738 + 4: le32(0x40A00000),
                                                      DS_SLOTS + 0x5F: b"\x5f", DS_SLOTS + 0x94 + 0x5F: b"\x9f"}),
    ], allow_calls=(0x339AC,), calls=(ANIM_BEGIN, STANCE, POSE, VOICE), eax_mask=0,
       mutants=("@mutant", "@side", "@frame", "@pose")),
    Spec("fighter_2400c", 0x2400C, [
        Case("s0", {"eax": E3_REC, "edx": 0x1234}, {**SLOT_PTRS, **P6_PTRS, E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
                                                     DS_SLOTS + 0x7A: b"\x02", DS_SLOTS + 0x94 + 0x7A: b"\x03",
                                                     E3_REC2 + 0x1C: le32(0x1000),
                                                     0xA83FA + 6: b"\xf0\xff", 0xA83FA + 4: b"\x11\x11",
                                                     0xA8408 + 12: le32(0x000ED111), 0xA8408 + 8: le32(0x000ED222),
                                                     DS_104748: le32(E3_OUT), E3_OUT + 0x29: b"\x29"}),
        Case("s1", {"eax": E3_REC, "edx": 0x1234}, {**SLOT_PTRS, **P6_PTRS, E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
                                                     DS_SLOTS + 0x7A: b"\x02", DS_SLOTS + 0x94 + 0x7A: b"\x04",
                                                     E3_REC2 + 0x1C: le32(0x1000),
                                                     0xA83FA + 8: b"\x34\x12", 0xA83FA + 6: b"\x11\x11",
                                                     0xA8408 + 16: le32(0x000ED333), 0xA8408 + 12: le32(0x000ED222),
                                                     DS_104748: le32(E3_OUT), E3_OUT + 0x29: b"\x29"}),
        Case("s2", {"eax": E3_REC2, "edx": 0x1234}, {**SLOT_PTRS, **P6_PTRS, E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
                                                      DS_SLOTS + 0x7A: b"\x02", DS_SLOTS + 0x94 + 0x7A: b"\x03",
                                                      E3_REC + 0x1C: le32(0x2000),
                                                      0xA83FA + 6: b"\xf0\xff", 0xA83FA + 4: b"\x11\x11",
                                                      0xA8408 + 12: le32(0x000ED111), 0xA8408 + 8: le32(0x000ED222),
                                                      DS_104748: le32(E3_OUT), E3_OUT + 0x29: b"\x29"}),
    ], calls=(ANIM_BEGIN, P6_2BCF4, VOICE), eax_mask=0,
       mutants=("@mutant", "@word", "@char", "@seek", "@other")),
    Spec("fighter_482e4", 0x482E4, [
        Case("s0", {"eax": E3_REC, "edx": 0x1234}, {**SLOT_PTRS, **P6_PTRS, E3_REC + 0x51: b"\x00",
                                                     E3_REC2 + 0x51: b"\x01",
                                                     0x108392: b"\x00\x5a",
                                                     DS_SLOTS + 0x5F: b"\x5f", DS_SLOTS + 0x94 + 0x5F: b"\x9f",
                                                     DS_SLOTS + 0x94 + 0x7A: b"\x03", DS_SLOTS + 0x7A: b"\x02",
                                                     0xC8F40 + 12: le32(0x000ED111), 0xC8F40 + 8: le32(0x000ED222)}),
        Case("s1", {"eax": E3_REC, "edx": 0x1234}, {**SLOT_PTRS, **P6_PTRS, E3_REC + 0x51: b"\x00",
                                                     E3_REC2 + 0x51: b"\x01",
                                                     0x108392: b"\x01\x00",
                                                     DS_SLOTS + 0x5F: b"\x5f", DS_SLOTS + 0x94 + 0x5F: b"\x9f",
                                                     DS_SLOTS + 0x94 + 0x7A: b"\x03", DS_SLOTS + 0x7A: b"\x02",
                                                     0xC8F40 + 12: le32(0x000ED111), 0xC8F40 + 8: le32(0x000ED222)}),
        Case("s2", {"eax": E3_REC2, "edx": 0x1234}, {**SLOT_PTRS, **P6_PTRS, E3_REC + 0x51: b"\x00",
                                                      E3_REC2 + 0x51: b"\x01",
                                                      0x108392: b"\x00\x00",
                                                      DS_SLOTS + 0x5F: b"\x5f", DS_SLOTS + 0x94 + 0x5F: b"\x9f",
                                                      DS_SLOTS + 0x94 + 0x7A: b"\x03", DS_SLOTS + 0x7A: b"\x02",
                                                      0xC8F40 + 12: le32(0x000ED111), 0xC8F40 + 8: le32(0x000ED222)}),
        Case("s3", {"eax": E3_REC2, "edx": 0x1234}, {**SLOT_PTRS, **P6_PTRS, E3_REC + 0x51: b"\x00",
                                                      E3_REC2 + 0x51: b"\x01",
                                                      0x108392: b"\x00\x01",
                                                      DS_SLOTS + 0x5F: b"\x5f", DS_SLOTS + 0x94 + 0x5F: b"\x9f",
                                                      DS_SLOTS + 0x94 + 0x7A: b"\x03", DS_SLOTS + 0x7A: b"\x02",
                                                      0xC8F40 + 12: le32(0x000ED111), 0xC8F40 + 8: le32(0x000ED222)}),
    ], calls=(ANIM_BEGIN, STANCE, POSE), eax_mask=0,
       mutants=("@mutant", "@side", "@byte", "@pose")),
    Spec("fighter_22a40", 0x22A40, [
        Case("s0", {"eax": E3_REC, "edx": 0x1234}, {E3_REC + 0x14: le32(0), E3_REC + 0x51: b"\x00",
                                                     E3_REC + 0x18: le32(0x1111), E3_REC + 0x1C: le32(0x2222),
                                                     E3_REC + 0x28: b"\x00\x00", E3_REC + 0x30: le32(0x33330000)}),
        Case("s1", {"eax": E3_REC, "edx": 0x1234}, {E3_REC + 0x14: le32(E3_SLOT), E3_REC + 0x51: b"\x00",
                                                     E3_REC + 0x18: le32(0x1111), E3_REC + 0x1C: le32(0x2222),
                                                     E3_REC + 0x28: b"\x00\x40", E3_REC + 0x30: le32(0x33330000),
                                                     E3_SLOT + 8: le32(0x8888), P6_CHILD: b"\x99" * 0x20},
             {0x2AE14: P6_CHILD}),
        Case("s2", {"eax": E3_REC, "edx": 0x1234}, {E3_REC + 0x14: le32(E3_SLOT), E3_REC + 0x51: b"\x01",
                                                     E3_REC + 0x18: le32(0x1111), E3_REC + 0x1C: le32(0x2222),
                                                     E3_REC + 0x28: b"\x00\x00", E3_REC + 0x30: le32(0x33330000),
                                                     E3_SLOT + 8: le32(0x8888), P6_CHILD: b"\x99" * 0x20},
             {0x2AE14: P6_CHILD}),
        Case("s3", {"eax": E3_REC2, "edx": 0x1234}, {E3_REC2 + 0x14: le32(E3_SLOT), E3_REC2 + 0x51: b"\x01",
                                                      E3_REC2 + 0x18: le32(0x1111), E3_REC2 + 0x1C: le32(0x2222),
                                                      E3_REC2 + 0x28: b"\x00\x40", E3_REC2 + 0x30: le32(0x33330000),
                                                      E3_SLOT + 8: le32(0x8888), P6_CHILD: b"\x99" * 0x20},
             {0x2AE14: P6_CHILD}),
    ], calls=(SPAWN, VOICE, P6_13244), eax_mask=0,
       mutants=("@mutant", "@side", "@a5", "@order")),
    Spec("fighter_47e30", 0x47E30, [
        Case("s0", {"eax": E3_REC, "edx": 0x1234}, {**SLOT_PTRS, E3_REC + 0x51: b"\x00",
                                                     E3_REC2 + 0x51: b"\x01",
                                                     DS_108394: b"\x00\x5a",
                                                     DS_108378: le32(0x40400000), DS_108378 + 4: le32(0x40A00000)}),
        Case("s1", {"eax": E3_REC, "edx": 0x1234}, {**SLOT_PTRS, E3_REC + 0x51: b"\x00",
                                                     E3_REC2 + 0x51: b"\x01",
                                                     DS_108394: b"\x01\x00",
                                                     DS_108378: le32(0x40400000), DS_108378 + 4: le32(0x40A00000)}),
        Case("s2", {"eax": E3_REC2, "edx": 0x1234}, {**SLOT_PTRS, E3_REC + 0x51: b"\x00",
                                                      E3_REC2 + 0x51: b"\x01",
                                                      DS_108394: b"\x00\x01",
                                                      DS_108378: le32(0x40400000), DS_108378 + 4: le32(0x40A00000)}),
    ], allow_calls=(0x339AC,), calls=(ANIM_BEGIN, P6_3AA54), eax_mask=0,
       mutants=("@mutant", "@flag", "@side")),
    Spec("fighter_24338", 0x24338, [
        Case("s0", {"eax": E3_REC, "edx": 0x1234}, {**SLOT_PTRS, E3_REC + 0x51: b"\x00",
                                                     E3_REC2 + 0x51: b"\x01",
                                                     DS_SLOTS + 0x7A: b"\x02", DS_SLOTS + 0x94 + 0x7A: b"\x03",
                                                     0xBED60 + 8: le32(0x000ED222) + le32(0x000ED111),
                                                     0xBD884 + 4: b"\x11\x11\x00\x20",
                                                     0xBE008 + 4: b"\x66\x00\x77\x00",
                                                     E3_REC2 + 0x10: _p6_fill(0x40, {0x00: le32(0x10101010), 0x19: b"\x29", 0x26: b"\x36\x36"}),
                                                     E3_REC2 + 0x50: b"\xa5\xa5\xa5\xa5\xa5\xa5\xa5\xa5\x58",
                                                     0xF0AFE: b"\xfe\xff"}),
        Case("s1", {"eax": E3_REC2, "edx": 0x1234}, {**SLOT_PTRS, E3_REC + 0x51: b"\x00",
                                                      E3_REC2 + 0x51: b"\x01",
                                                      DS_SLOTS + 0x7A: b"\x02", DS_SLOTS + 0x94 + 0x7A: b"\x04",
                                                      0xBED60 + 8: le32(0x000ED222) + le32(0x000ED333),
                                                      0xBD884 + 6: b"\x11\x11\x34\x12",
                                                      0xBE008 + 6: b"\x66\x00\x55\x00",
                                                      E3_REC + 0x10: _p6_fill(0x40, {0x00: le32(0x10101010), 0x19: b"\x29", 0x26: b"\x36\x36"}),
                                                      E3_REC + 0x50: b"\xa5\xa5\xa5\xa5\xa5\xa5\xa5\xa5\x58",
                                                      0xF0AFE: b"\xfe\xff"}),
    ], allow_calls=(0x339AC,), calls=(ANIM_BEGIN, P6_1890C, VOICE), eax_mask=0,
       mutants=("@mutant", "@other", "@slot", "@af", "@voice")),
    Spec("fighter_3e160", 0x3E160, [
        Case("s0", {"eax": E3_REC, "edx": 0x1234}, {E3_REC + 0x51: b"\x00", DS_SLOTS + 0x7A: b"\x02",
                                                     E3_REC + 0x56: b"\x00\x11",
                                                     P6_CHILD + 0x56: b"\x77", P6_CHILD + 0x60: b"\x00",
                                                     0xBB420 + 0x10: le32(0x10101010),
                                                     0xBB420 + 0x14: le32(0x14141414)},
             {0x2AE14: P6_CHILD}),
        Case("s1", {"eax": E3_REC2, "edx": 0x1234}, {E3_REC2 + 0x51: b"\x01", DS_SLOTS + 0x94 + 0x7A: b"\x03",
                                                      E3_REC2 + 0x56: b"\x00\x22",
                                                     P6_CHILD + 0x56: b"\x77", P6_CHILD + 0x60: b"\x00",
                                                     0xBB434 + 0x10: le32(0x20202020),
                                                     0xBB434 + 0x14: le32(0x24242424)},
             {0x2AE14: P6_CHILD}),
    ], calls=(P6_29C08, SPAWN), eax_mask=0,
       mutants=("@mutant", "@side", "@a5", "@child")),
    Spec("fighter_23f10", 0x23F10, [
        Case("s0", {"eax": E3_REC, "edx": 0x1234}, {DS_SLOTS - 8: le32(DS_SLOTS) + le32(0),
                                                     E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
                                                     DS_SLOTS + 0x94 + 0x7A: b"\x03", DS_SLOTS + 0x7A: b"\x02"}),
        Case("s1", {"eax": E3_REC, "edx": 0x1234}, {**SLOT_PTRS, **P6_PTRS,
                                                     E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
                                                     DS_SLOTS + 0x94 + 0x7A: b"\x03", DS_SLOTS + 0x7A: b"\x02",
                                                     E3_REC2 + 0x18: _p6_fill(0x40, {0x09: b"\x41", 0x04: le32(0x1000),
                                                                                    0x00: le32(0x1111),
                                                                                    0x18: le32(0x22220000),
                                                                                    0x3E: b"\x00\x11"}),
                                                     0xA83EC + 4: b"\x11\x11\xf0\xff",
                                                     0xA8408 + 8: le32(0x000ED222) + le32(0x000ED111),
                                                     P6_CHILD: _p6_fill(0x40, {}, 0x99),
                                                     P6_CHILD + 0x40: _p6_fill(0x20, {0x16: b"\x00\x22"}, 0x99),
                                                     0x104748: le32(0xDEADBEEF)},
             {0x2AE14: P6_CHILD}),
        Case("s2", {"eax": E3_REC2, "edx": 0x1234}, {**SLOT_PTRS, **P6_PTRS,
                                                      E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
                                                      DS_SLOTS + 0x94 + 0x7A: b"\x03", DS_SLOTS + 0x7A: b"\x02",
                                                      E3_REC + 0x18: _p6_fill(0x40, {0x09: b"\x41", 0x04: le32(0x1000),
                                                                                     0x00: le32(0x1111),
                                                                                     0x18: le32(0x22220000),
                                                                                     0x3E: b"\x00\x11"}),
                                                      0xA83EC + 4: b"\x34\x12\x11\x11",
                                                      0xA8408 + 8: le32(0x000ED222) + le32(0x000ED333),
                                                      P6_CHILD: _p6_fill(0x40, {}, 0x99),
                                                      P6_CHILD + 0x40: _p6_fill(0x20, {0x16: b"\x00\x22"}, 0x99),
                                                      0x104748: le32(0xDEADBEEF)},
             {0x2AE14: P6_CHILD}),
    ], calls=(P6_2A148, ANIM_BEGIN, SPAWN, VOICE), eax_mask=0,
       mutants=("@mutant", "@word", "@flag", "@x14", "@a5", "@other41")),
    Spec("fighter_45c98", 0x45C98, [
        Case("s0", {"eax": E3_REC, "edx": 0x1234}, {E3_REC + 0x14: le32(0), E3_REC + 0x51: b"\x00",
                                                     E3_REC2 + 0x51: b"\x01"}),
        Case("s1", {"eax": E3_REC, "edx": 0x1234}, {DS_SLOTS: le32(E3_REC), DS_SLOTS + 0x94: le32(E3_REC2),
                                                     DS_SLOTS - 8: le32(DS_SLOTS) + le32(0),
                                                     E3_REC + 0x14: le32(E3_SLOT), E3_REC + 0x51: b"\x01",
                                                     E3_SLOT: le32(E3_REC), E3_REC + 0x56: b"\x02\x00",
                                                     DS_PSET_BASE: le32(P6_PSET),
                                                     P6_PSET + 2 * 0x20 + 0x18: le32(0x0010A800)}),
        Case("s2", {"eax": E3_REC, "edx": 0x1234}, {DS_SLOTS: le32(E3_REC2), DS_SLOTS + 0x94: le32(E3_REC),
                                                     DS_SLOTS - 8: le32(DS_SLOTS) + le32(DS_SLOTS + 0x94),
                                                     E3_REC + 0x14: le32(E3_SLOT), E3_REC + 0x51: b"\x00",
                                                     E3_SLOT: le32(E3_OUT),
                                                     E3_OUT + 0x51: b"\x01\xa5\xa5\xa5\xa5\x02\x00",
                                                     DS_SLOTS + 0x7A: b"\x02", E3_REC2 + 0x56: b"\x02\x00",
                                                     0xC90F8 + 8: le32(0x000ED111) + le32(0x000ED222),
                                                     0xC75AA + 4: b"\x66\x00\x77\x00",
                                                     DS_PSET_BASE: le32(P6_PSET),
                                                     P6_PSET + 2 * 0x20 + 0x18: le32(0x0010A800)}),
        Case("s3", {"eax": E3_REC2, "edx": 0x1234}, {DS_SLOTS: le32(E3_REC2), DS_SLOTS + 0x94: le32(E3_REC),
                                                      DS_SLOTS - 8: le32(DS_SLOTS) + le32(DS_SLOTS + 0x94),
                                                      E3_REC2 + 0x14: le32(E3_SLOT), E3_REC2 + 0x51: b"\x01",
                                                      E3_SLOT: le32(E3_OUT),
                                                      E3_OUT + 0x51: b"\x00\xa5\xa5\xa5\xa5\x02\x00",
                                                      DS_SLOTS + 0x94 + 0x7A: b"\x03", E3_REC + 0x56: b"\x02\x00",
                                                      0xC90F8 + 8: le32(0x000ED222) + le32(0x000ED333),
                                                      0xC75AA + 4: b"\x66\x00\x77\x00",
                                                      DS_PSET_BASE: le32(P6_PSET),
                                                      P6_PSET + 2 * 0x20 + 0x18: le32(0x0010A800)}),
    ], calls=(ANIM_BEGIN, P6_13C70, VOICE), eax_mask=0,
       mutants=("@mutant", "@stream", "@fx", "@side", "@voice")),
    Spec("fighter_37dd4", 0x37DD4, [
        Case("s0", {"eax": E3_REC, "edx": 0x1234}, {E3_REC + 0x51: b"\x00", DS_SLOTS + 0x7A: b"\x03",
                                                     DS_PSET_BASE: le32(P6_PSET)}),
        Case("s1", {"eax": E3_REC2, "edx": 0x1234}, {E3_REC2 + 0x51: b"\x01", DS_SLOTS + 0x94 + 0x7A: b"\x05",
                                                      DS_PSET_BASE: le32(P6_PSET)}),
        Case("s2", {"eax": E3_REC, "edx": 0x1234}, {E3_REC + 0x51: b"\x02", DS_SLOTS + 0x7A: b"\x03",
                                                     DS_PSET_BASE: le32(P6_PSET)}),
    ], calls=(PALETTE, P6_29BC8), eax_mask=0, mutants=("@mutant", "@word", "@side")),
    Spec("fighter_22338", 0x22338, [
        Case("s0", {"eax": E3_REC, "edx": 0x1234}, {**SLOT_PTRS, E3_REC + 0x51: b"\x00",
                                                     E3_REC2 + 0x51: b"\x01",
                                                     DS_SLOTS + 0x7A: b"\x02", DS_SLOTS + 0x94 + 0x7A: b"\x03",
                                                     DS_SLOTS + 0x57: b"\x00", DS_SLOTS + 0x74: b"\x74\x74",
                                                     DS_SLOTS + 0x94 + 0x74: b"\x84\x84",
                                                     0xBE008 + 6: b"\x77\x00", 0xBE008 + 4: b"\x66\x00"}),
        Case("s1", {"eax": E3_REC, "edx": 0x1234}, {**SLOT_PTRS, E3_REC + 0x51: b"\x00",
                                                     E3_REC2 + 0x51: b"\x01",
                                                     DS_SLOTS + 0x7A: b"\x02", DS_SLOTS + 0x94 + 0x7A: b"\x03",
                                                     DS_SLOTS + 0x57: b"\x06", DS_SLOTS + 0x74: b"\x74\x74",
                                                     DS_SLOTS + 0x94 + 0x74: b"\x84\x84",
                                                     0xBE008 + 6: b"\x77\x00", 0xBE008 + 4: b"\x66\x00"}),
        Case("s2", {"eax": E3_REC, "edx": 0x1234}, {**SLOT_PTRS, E3_REC + 0x51: b"\x00",
                                                     E3_REC2 + 0x51: b"\x01",
                                                     DS_SLOTS + 0x7A: b"\x02", DS_SLOTS + 0x94 + 0x7A: b"\x03",
                                                     DS_SLOTS + 0x57: b"\x07", DS_SLOTS + 0x74: b"\x74\x74",
                                                     DS_SLOTS + 0x94 + 0x74: b"\x84\x84",
                                                     0xBE008 + 6: b"\x77\x00", 0xBE008 + 4: b"\x66\x00"}),
        Case("s3", {"eax": E3_REC, "edx": 0x1234}, {**SLOT_PTRS, E3_REC + 0x51: b"\x00",
                                                     E3_REC2 + 0x51: b"\x01",
                                                     DS_SLOTS + 0x7A: b"\x02", DS_SLOTS + 0x94 + 0x7A: b"\x03",
                                                     DS_SLOTS + 0x57: b"\x08", DS_SLOTS + 0x74: b"\x74\x74",
                                                     DS_SLOTS + 0x94 + 0x74: b"\x84\x84",
                                                     0xBE008 + 6: b"\x77\x00", 0xBE008 + 4: b"\x66\x00"}),
        Case("s4", {"eax": E3_REC2, "edx": 0x1234}, {**SLOT_PTRS, E3_REC + 0x51: b"\x00",
                                                      E3_REC2 + 0x51: b"\x01",
                                                      DS_SLOTS + 0x7A: b"\x02", DS_SLOTS + 0x94 + 0x7A: b"\x03",
                                                      DS_SLOTS + 0x94 + 0x57: b"\x06",
                                                      DS_SLOTS + 0x74: b"\x74\x74",
                                                      DS_SLOTS + 0x94 + 0x74: b"\x84\x84",
                                                      0xBE008 + 6: b"\x77\x00", 0xBE008 + 4: b"\x66\x00"}),
    ], allow_calls=(0x339AC,), calls=(ANIM_BEGIN, POSE, VOICE, P6_39280, P6_39F40), eax_mask=0,
       mutants=("@mutant", "@state", "@args", "@side", "@word", "@state3")),
    Spec("fighter_48374", 0x48374, [
        Case("s0", {"eax": E3_REC, "edx": 0x1234}, {**SLOT_PTRS, E3_REC + 0x51: b"\x00",
                                                     E3_REC2 + 0x51: b"\x01",
                                                     0x108392: b"\x00\x5a",
                                                     DS_SLOTS + 0x7A: b"\x02", DS_SLOTS + 0x94 + 0x7A: b"\x03",
                                                     DS_SLOTS + 0x5F: b"\x5f", DS_SLOTS + 0x57: b"\x57",
                                                     DS_SLOTS + 0x54: b"\x54", DS_SLOTS + 0x58: b"\x58",
                                                     E3_REC + 0x36: b"\x36\x36", E3_REC + 0x44: b"\x44\x44",
                                                     DS_SLOTS + 0x94 + 0x58: b"\x98",
                                                     0xC8F40 + 12: le32(0x000ED111), 0xC8F40 + 8: le32(0x000ED222)}),
        Case("s1", {"eax": E3_REC, "edx": 0x1234}, {**SLOT_PTRS, E3_REC + 0x51: b"\x00",
                                                     E3_REC2 + 0x51: b"\x01",
                                                     0x108392: b"\x01\x00",
                                                     DS_SLOTS + 0x7A: b"\x02", DS_SLOTS + 0x94 + 0x7A: b"\x03",
                                                     DS_SLOTS + 0x5F: b"\x5f", DS_SLOTS + 0x57: b"\x57",
                                                     DS_SLOTS + 0x54: b"\x54", DS_SLOTS + 0x58: b"\x58",
                                                     E3_REC + 0x36: b"\x36\x36", E3_REC + 0x44: b"\x44\x44",
                                                     DS_SLOTS + 0x94 + 0x58: b"\x98",
                                                     0xC8F40 + 12: le32(0x000ED111), 0xC8F40 + 8: le32(0x000ED222)}),
        Case("s2", {"eax": E3_REC2, "edx": 0x1234}, {**SLOT_PTRS, E3_REC + 0x51: b"\x00",
                                                      E3_REC2 + 0x51: b"\x01",
                                                      0x108392: b"\x00\x00",
                                                      DS_SLOTS + 0x7A: b"\x02", DS_SLOTS + 0x94 + 0x7A: b"\x03",
                                                      DS_SLOTS + 0x5F: b"\x5f", DS_SLOTS + 0x57: b"\x57",
                                                      DS_SLOTS + 0x54: b"\x54", DS_SLOTS + 0x58: b"\x58",
                                                      E3_REC2 + 0x36: b"\x36\x36", E3_REC2 + 0x44: b"\x44\x44",
                                                      DS_SLOTS + 0x94 + 0x58: b"\x98",
                                                      0xC8F40 + 12: le32(0x000ED111), 0xC8F40 + 8: le32(0x000ED222)}),
        Case("s3", {"eax": E3_REC2, "edx": 0x1234}, {**SLOT_PTRS, E3_REC + 0x51: b"\x00",
                                                      E3_REC2 + 0x51: b"\x01",
                                                      0x108392: b"\x00\x01",
                                                      DS_SLOTS + 0x7A: b"\x02", DS_SLOTS + 0x94 + 0x7A: b"\x03",
                                                      DS_SLOTS + 0x5F: b"\x5f", DS_SLOTS + 0x57: b"\x57",
                                                      DS_SLOTS + 0x54: b"\x54", DS_SLOTS + 0x58: b"\x58",
                                                      E3_REC2 + 0x36: b"\x36\x36", E3_REC2 + 0x44: b"\x44\x44",
                                                      DS_SLOTS + 0x94 + 0x58: b"\x98",
                                                      0xC8F40 + 12: le32(0x000ED111), 0xC8F40 + 8: le32(0x000ED222)}),
    ], allow_calls=(0x339AC,), calls=(SPEED, ANIM_BEGIN, POSE, P6_39F40), eax_mask=0,
       mutants=("@mutant", "@speed", "@stream", "@frame", "@side", "@pose")),
    Spec("fighter_24220", 0x24220, [
        Case("m0", {"eax": E3_SLOT, "edx": E3_REC, "ebx": 0}, {**P6_SLOT_SEED, E3_REC + 0x1C: le32(0x63FF)}),
        Case("m1", {"eax": E3_SLOT, "edx": E3_REC, "ebx": 0}, {**P6_SLOT_SEED, E3_REC + 0x1C: le32(0x6400)}),
        Case("m2", {"eax": E3_SLOT, "edx": E3_REC, "ebx": 0}, {**P6_SLOT_SEED, 0x104768: b"\x02\x00", E3_REC + 0x1C: le32(0),
                     **P6_SLOT_SEED_EXTRA, P6_CHILD: b"\x99" * 0x20}, {0x2AE14: P6_CHILD}),
        Case("m3", {"eax": E3_SLOT, "edx": E3_REC, "ebx": 0}, {**P6_SLOT_SEED, 0x104768: b"\x01\x00", E3_REC + 0x1C: le32(5),
                     **P6_SLOT_SEED_EXTRA, P6_CHILD: b"\x99" * 0x20}, {0x2AE14: P6_CHILD}),
        Case("m4", {"eax": E3_SLOT, "edx": E3_REC, "ebx": 0}, {**P6_SLOT_SEED, E3_SLOT + 0x58: b"\x02", E3_REC + 0x1C: le32(0),
                     P6_CHILD: b"\x99" * 0x20}, {0x2AE14: P6_CHILD}),
        Case("m5", {"eax": E3_SLOT, "edx": E3_REC, "ebx": 0}, {**P6_SLOT_SEED, E3_SLOT + 0x58: b"\x03", E3_REC + 0x1C: le32(0)}),
        Case("m6", {"eax": E3_SLOT, "edx": E3_REC, "ebx": 1}, {**P6_SLOT_SEED, E3_REC + 0x1C: le32(0x6400)}),
    ], calls=(ANCHORX, ANIM_BEGIN, SPAWN, VOICE), eax_mask=0,
       mutants=("@mutant", "@st", "@bound", "@anchor", "@desc", "@voice", "@plus4", "@decr")),
]


# ---- track P batches 4 and 5: the animation targets A and B (record 2026-10-03-reverse-p4-p5) ------
# A stream target runs as 0x2B2A0's opcodes 0x10 (0xD000), 0x11 (0xD100) and 0x15 (0xD500) call it
# through DS_00105BD4 with EAX = rec; none of the 33 reads the operand or ECX, and the dispatcher
# overwrites EAX after (`xor ecx,ecx; mov eax,ecx`): mask 0. Every field a function writes carries a
# sentinel that differs from what it writes; the neighbour bytes are seeded too. P45_SLOT3 is a fake
# slot pointer for the 0x80/0x81 index cases (a port that masks the index with &1 reads the other
# slot); P45_PSET is a pset base for DS_001014EC.
P45_SLOT3 = 0x10A700
P45_PSET = 0x10A800
P45_CHARS = {DS_SLOTS + 0x7A: b"\x05", DS_SLOTS + 0x94 + 0x7A: b"\x03"}
# The DS_001077A8 index table (two slot pointers, the raw reads it zero-extended) and a fake third
# entry at index 0x81 (0x1077A8 + 0x204): a port that masks the index with &1 reads entry 1.
P45_IDX = {0x1077A8: le32(DS_SLOTS) + le32(DS_SLOTS + 0x94)}
P45_PTRS = {**SLOT_PTRS, **P45_IDX}
P45_FAKE = {0x1079AC: le32(P45_SLOT3), P45_SLOT3: le32(E3_REC)}
CALL29C08 = E.Call(0x29C08, ("eax", "edx"), clobbers=("edx",))       # returns the palette handle
RELEASE = E.Call(0x2AD40, ("eax", "edx"), clobbers=("edx", "edi", "ebp"))   # void (the seam on release_record)


# 0x18BC8: the byte 0x100C1D = 0.
P45_SPECS = [
    Spec("fighter_18bc8", 0x18BC8, [
        Case("c0", {"eax": E3_REC, "edx": 0x1234, "ecx": 0}, {0x100C1D: b"\x5a"}),
    ], eax_mask=0, mutants=("@mutant",)),
    # 0x21084: n0 no held record; n1 held = E3_SLOT (its low byte 0, so a byte test of +0x14 skips).
    Spec("fighter_21084", 0x21084, [
        Case("n0", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(0), E3_REC + 0x1C: le32(0x1C1C1C1C), E3_REC + 0x34: b"\x34\x34\x36\x36"}),
        Case("n1", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(E3_SLOT), E3_REC + 0x1C: le32(0x1C1C1C1C),
              E3_REC + 0x34: b"\x34\x34\x36\x36", E3_SLOT + 0x54: b"\x54\x55", E3_SLOT + 0x57: b"\x57"}),
    ], eax_mask=0, mutants=("@mutant", "@byte14")),
    # 0x400E0: e1 clears bit 2 of 0xFF; e2 has only bit 2 set.
    Spec("fighter_400e0", 0x400E0, [
        Case("e0", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(0), E3_SLOT + 0x42: b"\x42\x43"}),
        Case("e1", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(E3_SLOT), E3_SLOT + 0x42: b"\xff\x43"}),
        Case("e2", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(E3_SLOT), E3_SLOT + 0x42: b"\x04\x43"}),
    ], eax_mask=0, mutants=("@mutant",)),

    Spec("fighter_21044", 0x21044, [
        Case("h0", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(0), E3_REC + 0x34: b"\x34\x34\x36\x36", E3_REC + 0x42: b"\x42\x43\x44\x45"}),
        Case("h1", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(E3_SLOT), E3_REC + 0x34: b"\x34\x34\x36\x36",
              E3_REC + 0x42: b"\x42\x43\x44\x45", E3_SLOT + 0x57: b"\x57"}, {0x1A570: 0}),
        Case("h2", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(E3_SLOT), E3_REC + 0x34: b"\x34\x34\x36\x36",
              E3_REC + 0x42: b"\x42\x43\x44\x45", E3_SLOT + 0x57: b"\x57"}, {0x1A570: 1}),
    ], calls=(BIT15,), eax_mask=0, mutants=("@mutant", "@neg")),

    Spec("fighter_1549c", 0x1549C, [
        Case("s0", {"eax": E3_OUT, "edx": 0x1234, "ecx": 0},
             {**P45_PTRS, **P45_CHARS, E3_OUT + 0x51: b"\x00", 0x9B01C + 12: le32(0xBBBBBBBB),
              0x9B01C + 20: le32(0xAAAAAAAA)}),
        Case("s1", {"eax": E3_OUT, "edx": 0x1234, "ecx": 0},
             {**P45_PTRS, **P45_CHARS, E3_OUT + 0x51: b"\x01", 0x9B01C + 12: le32(0xBBBBBBBB),
              0x9B01C + 20: le32(0xAAAAAAAA)}),
        Case("s2", {"eax": E3_OUT, "edx": 0x1234, "ecx": 0},
             {**P45_PTRS, **P45_CHARS, **P45_FAKE, E3_OUT + 0x51: b"\x80",
              0x9B01C + 0x204: le32(0xCCCCCCCC)}),
    ], calls=(ANIM_BEGIN, PALETTE, VOICE), eax_mask=0, mutants=("@mutant", "@side")),

    Spec("fighter_154e8", 0x154E8, [
        Case("s0", {"eax": E3_OUT, "edx": 0x1234, "ecx": 0},
             {**P45_PTRS, 0x1077AC: le32(0), E3_OUT + 0x51: b"\x00"}),
        Case("s1", {"eax": E3_OUT, "edx": 0x1234, "ecx": 0},
             {**P45_PTRS, E3_OUT + 0x51: b"\x00", E3_REC2 + 0x24: le32(0x24242424)}),
        Case("s2", {"eax": E3_OUT, "edx": 0x1234, "ecx": 0},
             {**P45_PTRS, E3_OUT + 0x51: b"\x01", E3_REC + 0x24: le32(0x24242424)}),
        Case("s3", {"eax": E3_OUT, "edx": 0x1234, "ecx": 0},
             {**P45_PTRS, 0x1077B4: le32(P45_SLOT3), P45_SLOT3: le32(E3_REC),
              E3_OUT + 0x51: b"\x02", E3_REC + 0x24: le32(0x24242424)}),
    ], calls=(VOICE,), eax_mask=0, mutants=("@mutant", "@side")),

    Spec("fighter_229e8", 0x229E8, [
        Case("s0", {"eax": E3_REC, "edx": 0x1234, "ecx": 0}),
        Case("s1", {"eax": E3_REC2, "edx": 0x1234, "ecx": 0}),
    ], calls=(ANIM_BEGIN,), eax_mask=0, mutants=("@mutant",)),

    Spec("fighter_243f8", 0x243F8, [
        Case("z%d" % side, {"eax": E3_OUT, "edx": 0x1234, "ecx": 0},
             {**P45_PTRS, **P45_CHARS, E3_OUT + 0x51: bytes([side]),
              DS_SLOTS + 0x94 * (1 - side) + 0x52: b"\x52\x53\x54\x55\x56\x57",
              DS_SLOTS + 0x94 * (1 - side) + 0x10: le32(0x10101010),
              DS_SLOTS + 0x94 * side + 0x52: b"\x92\x93\x94\x95\x96\x97",
              DS_SLOTS + 0x94 * side + 0x10: le32(0x20202020),
              0xC90F8 + 20: le32(0xAAAAAAAA), 0xC90F8 + 12: le32(0xBBBBBBBB)})
        for side in (0, 1)
    ], allow_calls=(0x33950,), calls=(ANIM_BEGIN,), eax_mask=0, mutants=("@mutant", "@slot")),

]


P45_SPECS += [
Spec("fighter_15510", 0x15510, [
        Case("s0", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {**P45_CHARS, E3_REC + 0x51: b"\x00", E3_REC + 0x56: b"\x23\x01", E3_REC + 0x4B: b"\x4b",
              0x9B08C: le32(0x9B8C8C8C), E3_OUT + 0x56: b"\x07\x00", E3_OUT + 0x59: b"\x59"},
             {0x29C08: 0x11111111, 0x2AE14: E3_OUT}),
        Case("s1", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {**P45_CHARS, E3_REC + 0x51: b"\x01", E3_REC + 0x56: b"\x23\x01", E3_REC + 0x4B: b"\x4b",
              0x9B08C: le32(0x9B8C8C8C), E3_REC2 + 0x56: b"\x07\x00", E3_REC2 + 0x59: b"\x59"},
             {0x29C08: 0x22222222, 0x2AE14: E3_REC2}),
        Case("s2", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {**P45_CHARS, 0x107952: b"\x77", E3_REC + 0x51: b"\x02", E3_REC + 0x56: b"\x23\x01",
              E3_REC + 0x4B: b"\x4b", 0x9B08C: le32(0x9B8C8C8C), E3_OUT + 0x56: b"\x07\x00",
              E3_OUT + 0x59: b"\x59"},
             {0x29C08: 0x33333333, 0x2AE14: E3_OUT}),
    ], calls=(CALL29C08, SPAWN), eax_mask=0, mutants=("@mutant", "@side", "@pal", "@a5")),

Spec("fighter_241a8", 0x241A8, [
        Case("n0", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {**P45_PTRS, 0x1077A8: le32(0), **P45_CHARS}),
        Case("n1", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {**P45_PTRS, **P45_CHARS, 0xA84E0 + 20: le32(0x000A1111), E3_REC + 0x56: b"\x23\x01",
              E3_REC + 0x4B: b"\x4b", E3_OUT + 0x56: b"\x07\x00"},
             {0x2AE14: E3_OUT}),
        Case("n2", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {**P45_PTRS, **P45_CHARS, 0xA84E0 + 12: le32(0x000A2222), E3_REC + 0x56: b"\x23\x01",
              E3_REC + 0x4B: b"\x4b", E3_REC2 + 0x56: b"\x07\x00"},
             {0x2AE14: E3_REC2}),
        Case("n3", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {**P45_PTRS, **P45_CHARS, 0x1079A8: le32(P45_SLOT3), 0x1075A8: le32(E3_REC2),
              P45_SLOT3 + 0x7A: b"\x81", 0xA84E0 + 0x204: le32(0x000A3333), E3_REC + 0x51: b"\x80",
              E3_REC + 0x56: b"\x23\x01",
              E3_REC + 0x4B: b"\x4b", E3_OUT + 0x56: b"\x07\x00"},
             {0x2AE14: E3_OUT}),
    ], calls=(SPAWN,), eax_mask=0, mutants=("@mutant", "@sext")),

Spec("fighter_40358", 0x40358, [
        Case("h0", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(0), E3_REC + 0x56: b"\x23\x01", E3_REC + 0x4B: b"\x4b"}),
        Case("h1", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(E3_SLOT), E3_REC + 0x51: b"\x01", E3_REC + 0x56: b"\x23\x01",
              E3_REC + 0x4B: b"\x4b", E3_OUT + 0x14: le32(0x14141414), E3_OUT + 0x51: b"\x51",
              E3_OUT + 0x56: b"\x07\x00"},
             {0x2AE14: E3_OUT}),
    ], calls=(SPAWN,), eax_mask=0, mutants=("@mutant",)),

Spec("fighter_45c54", 0x45C54, [
        Case("h0", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(0), E3_REC + 0x56: b"\x23\x01"}),
        Case("h1", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(E3_SLOT), E3_REC + 0x56: b"\x23\x01", E3_OUT + 0x14: le32(0x14141414),
              E3_OUT + 0x59: b"\x59"},
             {0x2AE14: E3_OUT}),
    ], calls=(SPAWN, VOICE), eax_mask=0, mutants=("@mutant",)),

Spec("fighter_489dc", 0x489DC, [
        Case("h0", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(0), E3_REC + 0x56: b"\x23\x01"}),
        Case("h1", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(E3_SLOT), E3_SLOT: le32(E3_OUT), E3_OUT + 0x4B: b"\x4b",
              E3_REC + 0x56: b"\x23\x01", E3_OUT + 0x56: b"\x07\x00"}),
        Case("h2", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(E3_SLOT), E3_SLOT: le32(E3_OUT), E3_OUT + 0x4B: b"\x00",
              E3_REC + 0x56: b"\x23\x01", E3_OUT + 0x56: b"\x07\x00"},
             {0x2AE14: E3_OUT}),
    ], calls=(SPAWN,), eax_mask=0, mutants=("@mutant",)),

Spec("fighter_400ec", 0x400EC, [
        Case("s0", {"eax": E3_OUT, "edx": 0x1234, "ecx": 0},
             {**P45_PTRS, **P45_CHARS, E3_OUT + 0x51: b"\x00", 0xC90F8 + 20: le32(0xAAAAAAAA),
              0xC90F8 + 12: le32(0xBBBBBBBB), 0xC75AA + 10: b"\x60\x00", 0xC75AA + 6: b"\x61\x00",
              0x104AE9: b"\xa9\xaa"}),
        Case("s1", {"eax": E3_OUT, "edx": 0x1234, "ecx": 0},
             {**P45_PTRS, **P45_CHARS, E3_OUT + 0x51: b"\x01", 0xC90F8 + 20: le32(0xAAAAAAAA),
              0xC90F8 + 12: le32(0xBBBBBBBB), 0xC75AA + 10: b"\x60\x00", 0xC75AA + 6: b"\x61\x00",
              0x104AE9: b"\xa9\xaa"}),
        Case("s2", {"eax": E3_OUT, "edx": 0x1234, "ecx": 0},
             {**P45_PTRS, **P45_CHARS, **P45_FAKE, E3_OUT + 0x51: b"\x80",
              0xC90F8 + 0x204: le32(0xCCCCCCCC), 0xC75AA + 0x102: b"\x62\x00", 0x104AE9: b"\xa9\xaa"}),
    ], calls=(ANIM_BEGIN, VOICE), eax_mask=0, mutants=("@mutant", "@side")),
]


# The seven 0xD500 side-record targets 0x3427C..0x345BC: s0 the own slot 0's record (E3_REC), s1
# slot 1's (E3_REC2), s2 side 0x80 (the slot at DS_001077B0 + 0x80 * 0x94 = 0x10C1B0, whose record
# is zero: a `&1` port reads E3_REC). The voice (when the function has one) runs before the stream,
# except 0x3438C (anim_begin 0x34403, voice 0x3440D).
def p45_own_anim(name, entry, stream, frame, voice, mutants=("@mutant", "@side")):
    return Spec(name, entry, [
        Case("s0", {"eax": E3_OUT, "edx": 0x1234, "ecx": 0}, {**SLOT_PTRS, E3_OUT + 0x51: b"\x00"}),
        Case("s1", {"eax": E3_OUT, "edx": 0x1234, "ecx": 0}, {**SLOT_PTRS, E3_OUT + 0x51: b"\x01"}),
        Case("s2", {"eax": E3_OUT, "edx": 0x1234, "ecx": 0}, {**SLOT_PTRS, E3_OUT + 0x51: b"\x80"}),
    ], calls=(ANIM_BEGIN, VOICE) if voice else (ANIM_BEGIN,), eax_mask=0, mutants=mutants)

P45_SPECS += [
p45_own_anim("fighter_3427c", 0x3427C, 0xE76B2, 0x40400000, 0x8C, ("@mutant", "@side", "@voice")),

p45_own_anim("fighter_34308", 0x34308, 0xE436A, 0x40400000, None),

p45_own_anim("fighter_3438c", 0x3438C, 0xED354, 0x3F800000, 0xA6),

p45_own_anim("fighter_34418", 0x34418, 0xEAF66, 0x40400000, 0x84),

p45_own_anim("fighter_344a4", 0x344A4, 0xD461C, 0x40400000, 0x86),

p45_own_anim("fighter_34530", 0x34530, 0xD299C, 0x40400000, 0x9C),

p45_own_anim("fighter_345bc", 0x345BC, 0xE0F62, 0x40400000, 0x9B),
]


def p45_156e0(cid, r51, al, b6, b7, slot_rec, stream, extra=None):
    return Case(cid, {"eax": E3_OUT, "edx": 0x1234, "ecx": 0},
                {**P45_PTRS, **P45_CHARS, **P45_FAKE, E3_OUT + 0x51: bytes([r51]),
                 E3_OUT + 0x18: le32(0x18181818) + le32(0x1C1C1C1C),
                 E3_OUT + 0x56: b"\x23\x01", E3_OUT + 0x4B: b"\x4b",
                 0xC9783: bytes([0x11, 0x22, 0x33, b6, b7]), stream: le32(0x5A5A5A5A),
                 0xF0AFE: b"\xfe\xff",
                 slot_rec + 0x18: le32(0x22222222) + le32(0x33333333),
                 slot_rec + 0x56: b"\x07\x00", **(extra or {})}, {0x1A570: al})


P45_SPECS += [
    Spec("fighter_156e0", 0x156E0, [
        p45_156e0("s0", 0x00, 1, 0x80, 0x01, E3_REC2, 0x9B038 + 12),
        p45_156e0("s1", 0x01, 0, 0x02, 0xFF, E3_REC, 0x9B038 + 20),
        p45_156e0("s2", 0x80, 1, 0x04, 0x03, E3_REC, 0x9B038 + 0x204),
    ], calls=(BIT15, ANIM_BEGIN, VOICE), eax_mask=0, mutants=("@mutant", "@side", "@signed", "@plus")),

    Spec("fighter_22ab8", 0x22AB8, [
        Case("h0", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(0), E3_SLOT: le32(E3_OUT), E3_OUT + 0x51: b"\x00"}),
        Case("h1", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(E3_SLOT), E3_SLOT: le32(E3_OUT), E3_OUT + 0x51: b"\x00",
              E3_REC + 0x18: le32(0x18181818), E3_REC + 0x1C: le32(0x1C1C1C1C),
              E3_REC + 0x34: b"\x34\x34\x36\x36", E3_SLOT + 8: le32(0x08080808),
              0x104730: le32(0x000A0000), 0x104734: le32(0x000A0001)}, {0x1A570: 0}),
        Case("h2", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(E3_SLOT), E3_SLOT: le32(E3_OUT), E3_OUT + 0x51: b"\x01",
              E3_REC + 0x18: le32(0x18181818), E3_REC + 0x1C: le32(0x1C1C1C1C),
              E3_REC + 0x34: b"\x34\x34\x36\x36", E3_SLOT + 8: le32(0x08080808),
              0x104730: le32(0x000A0000), 0x104734: le32(0x000A0001)}, {0x1A570: 1}),
        Case("h3", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(E3_SLOT), E3_SLOT: le32(E3_OUT), E3_OUT + 0x51: b"\x80",
              E3_REC + 0x18: le32(0x18181818), E3_REC + 0x1C: le32(0x1C1C1C1C),
              E3_REC + 0x34: b"\x34\x34\x36\x36", E3_SLOT + 8: le32(0x08080808),
              0x104930: le32(0x000A0002), 0x104530: le32(0x000A0003)}, {0x1A570: 0}),
    ], calls=(BIT15,), eax_mask=0, mutants=("@mutant", "@side", "@sext", "@add", "@table")),

    Spec("fighter_37b70", 0x37B70, [
        Case("r0", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(0), E3_SLOT + 0x7A: b"\x00"}),
        Case("r1", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(E3_SLOT), E3_REC + 0x28: b"\x00\x00", E3_REC + 0x56: b"\x23\x01",
              E3_SLOT + 0x7A: b"\x00", 0xBDC48: le32(0), 0xBDC64: b"\x00\x01"}),
        Case("r2", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(E3_SLOT), E3_REC + 0x28: b"\x00\x00", E3_REC + 0x56: b"\x23\x01",
              E3_SLOT + 0x7A: b"\x00", 0xBDC48: le32(0x000A0000), 0xBDC64: b"\x00\x01",
              E3_OUT + 0x56: b"\x07\x00", E3_OUT + 0x59: b"\x59"}, {0x2AE14: E3_OUT}),
        Case("r3", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(E3_SLOT), E3_REC + 0x28: b"\x00\x40", E3_REC + 0x56: b"\x23\x01",
              E3_SLOT + 0x7A: b"\x05", 0xBDC48 + 20: le32(0x000A0005), 0xBDC64 + 10: b"\x00\x01",
              E3_OUT + 0x56: b"\x07\x00", E3_OUT + 0x59: b"\x59"}, {0x2AE14: E3_OUT}),
        Case("r4", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(E3_SLOT), E3_SLOT: le32(E3_OUT), E3_REC + 0x28: b"\x00\x00",
              E3_REC + 0x56: b"\x23\x01", E3_SLOT + 0x7A: b"\x01", 0xBDC48 + 4: le32(0x000A0001),
              E3_OUT + 0x4B: b"\x4b", E3_OUT + 0x56: b"\x07\x00", E3_OUT + 0x59: b"\x59"},
             {0x2AE14: E3_OUT}),
        Case("r5", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(E3_SLOT), E3_SLOT: le32(E3_OUT), E3_REC + 0x28: b"\x00\x40",
              E3_REC + 0x56: b"\x23\x01", E3_SLOT + 0x7A: b"\x06", 0xBDC48 + 24: le32(0x000A0006),
              E3_OUT + 0x4B: b"\x4b", E3_OUT + 0x56: b"\x07\x00", E3_OUT + 0x59: b"\x59"},
             {0x2AE14: E3_OUT}),
        Case("r6", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(E3_SLOT), E3_SLOT: le32(E3_OUT), E3_REC + 0x28: b"\x00\x00",
              E3_REC + 0x56: b"\x23\x01", E3_SLOT + 0x7A: b"\x80", 0xBDC48 + 0x200: le32(0x000A0080),
              0xBDA48: le32(0x000AFF80), E3_OUT + 0x4B: b"\x4b", E3_OUT + 0x56: b"\x07\x00",
              E3_OUT + 0x59: b"\x59"}, {0x2AE14: E3_OUT}),
    ], calls=(SPAWN,), eax_mask=0, mutants=("@mutant", "@path", "@neg", "@hrec", "@sext")),
]


def p45_3db8c(name, entry, w14, w0, mutants=("@mutant", "@a2", "@w34", "@a4", "@side")):
    return Spec(name, entry, [
        Case("h0", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(0), E3_REC + 0x56: b"\x23\x01"}),
        Case("h1", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(E3_SLOT), E3_REC + 0x28: b"\x00\x40", E3_REC + 0x51: b"\x01",
              E3_REC + 0x18: le32(0x00001000), E3_REC + 0x1C: le32(0x00002000),
              E3_REC + 0x30: le32(0x00030000), E3_REC + 0x56: b"\x23\x01",
              E3_SLOT + 8: le32(0x08080808), E3_OUT + 0x14: le32(0x14141414), E3_OUT + 0x2E: b"\xfe\xff",
              E3_OUT + 0x34: b"\x34\x34\x36\x36", E3_OUT + 0x4E: b"\x4e", E3_OUT + 0x56: b"\x07\x00"},
             {0x2AE14: E3_OUT}),
        Case("h2", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(E3_SLOT), E3_REC + 0x28: b"\x00\x00", E3_REC + 0x51: b"\x00",
              E3_REC + 0x18: le32(0x00001000), E3_REC + 0x1C: le32(0x00002000),
              E3_REC + 0x30: le32(0x00030000), E3_REC + 0x56: b"\x23\x01",
              E3_SLOT + 8: le32(0x08080808), E3_OUT + 0x14: le32(0x14141414), E3_OUT + 0x2E: b"\xfe\xff",
              E3_OUT + 0x34: b"\x34\x34\x36\x36", E3_OUT + 0x4E: b"\x4e", E3_OUT + 0x56: b"\x07\x00"},
             {0x2AE14: E3_OUT}),
        Case("h3", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(E3_SLOT), E3_REC + 0x28: b"\x00\x00", E3_REC + 0x51: b"\x01",
              E3_REC + 0x18: le32(0xFFFFFFF0), E3_REC + 0x1C: le32(0x12345678),
              E3_REC + 0x30: le32(0xFFFD8000), E3_REC + 0x56: b"\x23\x01",
              E3_SLOT + 8: le32(0x08080808), E3_REC2 + 0x14: le32(0x14141414),
              E3_REC2 + 0x2E: b"\x34\x12", E3_REC2 + 0x34: b"\x34\x34\x36\x36",
              E3_REC2 + 0x4E: b"\x4e", E3_REC2 + 0x56: b"\x07\x00"}, {0x2AE14: E3_REC2}),
    ], calls=(SPAWN,), eax_mask=0, mutants=mutants)

P45_SPECS += [

Spec("fighter_3d328", 0x3D328, [
        Case("h0", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(0), E3_REC + 0x56: b"\x23\x01"}),
        Case("h1", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(E3_SLOT), E3_REC + 0x28: b"\x00\x00", E3_REC + 0x51: b"\x00",
              E3_REC + 0x56: b"\x23\x01", E3_REC + 0x4B: b"\x4b", E3_OUT + 0x14: le32(0x14141414),
              E3_OUT + 0x2E: b"\xfe\xff", E3_OUT + 0x4E: b"\x4e", E3_OUT + 0x56: b"\x07\x00",
              E3_OUT + 0x60: b"\x60"}, {0x2AE14: E3_OUT}),
        Case("h2", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(E3_SLOT), E3_REC + 0x28: b"\x00\x40", E3_REC + 0x51: b"\x01",
              E3_REC + 0x56: b"\x23\x01", E3_REC + 0x4B: b"\x4b", E3_REC2 + 0x14: le32(0x14141414),
              E3_REC2 + 0x2E: b"\x34\x12", E3_REC2 + 0x4E: b"\x4e", E3_REC2 + 0x56: b"\x07\x00",
              E3_REC2 + 0x60: b"\x60"}, {0x2AE14: E3_REC2}),
        Case("h3", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(E3_SLOT), E3_REC + 0x28: b"\x00\x00", E3_REC + 0x51: b"\x01",
              E3_REC + 0x56: b"\x23\x01", E3_REC + 0x4B: b"\x4b", E3_REC2 + 0x14: le32(0x14141414),
              E3_REC2 + 0x2E: b"\x34\x12", E3_REC2 + 0x4E: b"\x4e", E3_REC2 + 0x56: b"\x07\x00",
              E3_REC2 + 0x60: b"\x60"}, {0x2AE14: E3_REC2}),
    ], calls=(SPAWN, VOICE), eax_mask=0, mutants=("@mutant", "@a2", "@a4", "@side", "@voice")),

Spec("fighter_3da50", 0x3DA50, [
        Case("h0", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(0), E3_REC + 0x56: b"\x23\x01"}),
        Case("h1", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(E3_SLOT), E3_REC + 0x28: b"\x00\x00", E3_REC + 0x51: b"\x00",
              E3_REC + 0x56: b"\x23\x01", E3_REC + 0x4B: b"\x4b", E3_OUT + 0x14: le32(0x14141414),
              E3_OUT + 0x2E: b"\xfe\xff", E3_OUT + 0x4E: b"\x4e", E3_OUT + 0x56: b"\x07\x00",
              E3_OUT + 0x59: b"\x59", E3_OUT + 0x60: b"\x60"}, {0x2AE14: E3_OUT}),
        Case("h2", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(E3_SLOT), E3_REC + 0x28: b"\x00\x40", E3_REC + 0x51: b"\x01",
              E3_REC + 0x56: b"\x23\x01", E3_REC + 0x4B: b"\x4b", E3_REC2 + 0x14: le32(0x14141414),
              E3_REC2 + 0x2E: b"\x34\x12", E3_REC2 + 0x4E: b"\x4e", E3_REC2 + 0x56: b"\x07\x00",
              E3_REC2 + 0x59: b"\x59", E3_REC2 + 0x60: b"\x60"}, {0x2AE14: E3_REC2}),
        Case("h3", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(E3_SLOT), E3_REC + 0x28: b"\x00\x00", E3_REC + 0x51: b"\x01",
              E3_REC + 0x56: b"\x23\x01", E3_REC + 0x4B: b"\x4b", E3_REC2 + 0x14: le32(0x14141414),
              E3_REC2 + 0x2E: b"\x34\x12", E3_REC2 + 0x4E: b"\x4e", E3_REC2 + 0x56: b"\x07\x00",
              E3_REC2 + 0x59: b"\x59", E3_REC2 + 0x60: b"\x60"}, {0x2AE14: E3_REC2}),
    ], calls=(SPAWN, VOICE), eax_mask=0, mutants=("@mutant", "@a2", "@a4", "@side")),

p45_3db8c("fighter_3db8c", 0x3DB8C, b"\xe0\x00", b"\x20\xff"),

p45_3db8c("fighter_3dc3c", 0x3DC3C, b"\xa0\x01", b"\x60\xfe", ("@mutant", "@a2", "@w34", "@side")),

Spec("fighter_403a0", 0x403A0, [
        Case("h0", {"eax": E3_OUT, "edx": 0x1234, "ecx": 0},
             {E3_OUT + 0x14: le32(0), E3_OUT + 0x56: b"\x23\x01"}),
        Case("h1", {"eax": E3_OUT, "edx": 0x1234, "ecx": 0},
             {**P45_PTRS, 0x1077A8: le32(0), **P45_CHARS,
              E3_OUT + 0x14: le32(E3_SLOT), E3_OUT + 0x51: b"\x00", E3_OUT + 0x56: b"\x23\x01"}),
        Case("h2", {"eax": E3_OUT, "edx": 0x1234, "ecx": 0},
             {**P45_PTRS, **P45_CHARS, E3_OUT + 0x14: le32(E3_SLOT), E3_OUT + 0x51: b"\x00",
              E3_OUT + 0x59: b"\x59", E3_OUT + 0x56: b"\x23\x01", E3_REC2 + 0x28: b"\x00\x00",
              E3_REC2 + 0x56: b"\x23\x01", 0xC7780 + 6: b"\x00\x01", 0xC778C + 8: b"\x00\x02",
              E3_OUT + 0x14: le32(0x14141414), E3_OUT + 0x51: b"\x51", E3_OUT + 0x59: b"\x59"},
             {0x2AE14: E3_OUT}),
        Case("h3", {"eax": E3_OUT, "edx": 0x1234, "ecx": 0},
             {**P45_PTRS, **P45_CHARS, E3_OUT + 0x14: le32(E3_SLOT), E3_OUT + 0x51: b"\x01",
              E3_OUT + 0x59: b"\x59", E3_OUT + 0x56: b"\x23\x01", E3_REC + 0x28: b"\x00\x40",
              E3_REC + 0x56: b"\x23\x01", 0xC7780 + 10: b"\x00\x01", 0xC778C + 12: b"\x00\x80",
              E3_REC2 + 0x14: le32(0x14141414), E3_REC2 + 0x51: b"\x51"},
             {0x2AE14: E3_REC2}),
        Case("h4", {"eax": E3_OUT, "edx": 0x1234, "ecx": 0},
             {**P45_PTRS, **P45_CHARS, E3_OUT + 0x14: le32(E3_SLOT), E3_OUT + 0x51: b"\x00",
              E3_OUT + 0x59: b"\x00", E3_OUT + 0x56: b"\x23\x01", E3_REC2 + 0x28: b"\x00\x00",
              E3_REC2 + 0x56: b"\x23\x01", 0xC7780 + 6: b"\x00\x01", 0xC778C + 8: b"\x00\x02",
              E3_OUT + 0x14: le32(0x14141414), E3_OUT + 0x51: b"\x51", E3_OUT + 0x59: b"\x59"},
             {0x2AE14: E3_OUT}),
    ], calls=(SPAWN,), eax_mask=0, mutants=("@mutant", "@side", "@signed", "@neg", "@a5", "@r59")),

Spec("fighter_40fbc", 0x40FBC, [
        Case("s0", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x51: b"\x00", E3_REC + 0x56: b"\x23\x01", E3_REC + 0x20: le32(0x20202020),
              E3_REC + 0x24: le32(0x24242424), E3_OUT + 0x20: le32(0x30303030),
              E3_OUT + 0x24: le32(0x34343434), E3_OUT + 0x59: b"\x59"}, {0x1A570: 1, 0x2AE14: E3_OUT}),
        Case("s1", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x51: b"\x01", E3_REC + 0x56: b"\x23\x01", E3_REC + 0x20: le32(0x20202020),
              E3_REC + 0x24: le32(0x24242424), E3_REC2 + 0x20: le32(0x30303030),
              E3_REC2 + 0x24: le32(0x34343434), E3_REC2 + 0x59: b"\x59"}, {0x1A570: 0, 0x2AE14: E3_REC2}),
        Case("s2", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x51: b"\x01", E3_REC + 0x56: b"\x23\x01", E3_REC + 0x20: le32(0x20202020),
              E3_REC + 0x24: le32(0x24242424), E3_REC2 + 0x20: le32(0x30303030),
              E3_REC2 + 0x24: le32(0x34343434), E3_REC2 + 0x59: b"\x59"}, {0x1A570: 1, 0x2AE14: E3_REC2}),
    ], calls=(BIT15, SPAWN, PALETTE), eax_mask=0, mutants=("@mutant", "@pal", "@a2")),

Spec("fighter_48a20", 0x48A20, [
        Case("w0", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {0x1014F4: le32(E3_OUT), E3_REC + 0x18: le32(0x18181818), E3_REC + 0x1C: le32(0x1C1C1C1C),
              E3_REC + 0x28: b"\x00\x00", E3_REC + 0x30: le32(0x00020000), E3_REC + 0x4B: b"\x00",
              E3_REC + 0x56: b"\x23\x01", E3_OUT + 0x56: b"\x07\x00"},
             {0x2AE14: E3_OUT}),
        Case("w1", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {0x1014F4: le32(E3_OUT), E3_REC + 0x18: le32(0x18181818), E3_REC + 0x1C: le32(0x1C1C1C1C),
              E3_REC + 0x28: b"\x00\x40", E3_REC + 0x30: le32(0xFFFD8000), E3_REC + 0x4B: b"\x01",
              E3_REC + 0x56: b"\x23\x01", E3_OUT + 0x68 + 0x4B: b"\x00", E3_OUT + 0x56: b"\x07\x00"},
             {0x2AE14: E3_OUT}),
        Case("w2", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {0x1014F4: le32(E3_OUT), E3_REC + 0x18: le32(0x18181818), E3_REC + 0x1C: le32(0x1C1C1C1C),
              E3_REC + 0x28: b"\x00\x00", E3_REC + 0x30: le32(0x00020000), E3_REC + 0x4B: b"\x01",
              E3_REC + 0x56: b"\x23\x01", E3_OUT + 0x68 + 0x4B: b"\x02", E3_OUT + 0xD0 + 0x4B: b"\x00",
              E3_OUT + 0x56: b"\x07\x00"}, {0x2AE14: E3_OUT}),
    ], calls=(SPAWN,), eax_mask=0, mutants=("@mutant", "@walk", "@a2")),
]

def p45_24508(cid, side):
    # §P5.5: the 0x339AC context's other slot (ctx[3]); the own slot (ctx[2]) is
    # seeded too, so the @side mutant's wrong-slot stores show as bytes. The two
    # slots' characters differ (5 and 3), so the @stream mutant's index reads the
    # other 0xA85F8 entry (two distinct sentinels) and the recorded 0x2BC30 call
    # differs.
    own = DS_SLOTS + side * 0x94
    oth = DS_SLOTS + (1 - side) * 0x94
    seeds = {}
    for s in (own, oth):
        seeds[s + 0x52] = b"\x52\x53"
        seeds[s + 0x10] = le32(0x10101010)
        seeds[s + 0x58] = b"\x58"
    return Case(cid, {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
                {**P45_PTRS, **P45_CHARS, E3_REC + 0x51: bytes([side]), **seeds,
                 0xA85F8 + 12: le32(0x5B5B5B5B), 0xA85F8 + 20: le32(0x5A5A5A5A)})


P45_SPECS += [
Spec("fighter_24508", 0x24508, [p45_24508("s0", 0), p45_24508("s1", 1)],
         allow_calls=(0x339AC,), calls=(ANIM_BEGIN, VOICE), eax_mask=0,
         mutants=("@mutant", "@side", "@stream")),
    # 0x24454: t1's record +0x24 = 0x80000000 passes the 0x7FFFFFFF mask (a whole-dword test
    # refuses); t6's release pset is 0x1014EC + 0x1234 * 0x20; t5/t7 the state-1 guard.
    Spec("fighter_24454", 0x24454, [
        Case("t0", {"eax": E3_SLOT, "edx": E3_REC, "ecx": 0},
             {E3_SLOT + 0x58: b"\x02", E3_SLOT + 0x7A: b"\x05", E3_REC + 0x24: le32(0),
              E3_REC + 0x56: b"\x23\x01"}),
        Case("t1", {"eax": E3_SLOT, "edx": E3_REC, "ecx": 0},
             {E3_SLOT + 0x58: b"\x00", E3_SLOT + 0x7A: b"\x05", E3_REC + 0x18: le32(0x18181818),
              E3_REC + 0x1C: le32(0x1C1C1C1C), E3_REC + 0x24: le32(0x80000000),
              E3_REC + 0x28: b"\x00\x40", E3_REC + 0x30: le32(0xFFFD8000), E3_REC + 0x56: b"\x23\x01",
              0xA85DC + 20: le32(0x000A85DC), E3_OUT + 0x36: b"\x36\x36", E3_OUT + 0x56: b"\x07\x00",
              0x104740: le32(0x47474747)}, {0x2AE14: E3_OUT}),
        Case("t2", {"eax": E3_SLOT, "edx": E3_REC, "ecx": 0},
             {E3_SLOT + 0x58: b"\x00", E3_SLOT + 0x7A: b"\x05", E3_REC + 0x24: le32(1),
              E3_REC + 0x56: b"\x23\x01", 0x104740: le32(0x47474747)}),
        Case("t3", {"eax": E3_SLOT, "edx": E3_REC, "ecx": 0},
             {E3_SLOT + 0x58: b"\x00", E3_SLOT + 0x7A: b"\x05", E3_REC + 0x18: le32(0x18181818),
              E3_REC + 0x1C: le32(0x1C1C1C1C), E3_REC + 0x24: le32(0), E3_REC + 0x28: b"\x00\x00",
              E3_REC + 0x30: le32(0x00030000), E3_REC + 0x56: b"\x23\x01",
              0xA85DC + 20: le32(0x000A85DC), E3_OUT + 0x36: b"\x36\x36", E3_OUT + 0x56: b"\x07\x00",
              0x104740: le32(0x47474747)}, {0x2AE14: E3_OUT}),
        Case("t4", {"eax": E3_SLOT, "edx": E3_REC, "ecx": 0},
             {E3_SLOT + 0x58: b"\x01", E3_REC + 0x51: b"\x00", E3_REC + 0x24: le32(0),
              0x104740: le32(E3_OUT), E3_OUT + 0x1C: le32(0x00002FFF), E3_OUT + 0x56: b"\x34\x12"}),
        Case("t5", {"eax": E3_SLOT, "edx": E3_REC, "ecx": 0},
             {E3_SLOT + 0x58: b"\x01", E3_SLOT + 0x52: b"\x52\x53", E3_REC + 0x51: b"\x00",
              0x104740: le32(E3_OUT), E3_OUT + 0x1C: le32(0x00003000), E3_OUT + 0x56: b"\x34\x12",
              0x1014EC: le32(P45_PSET), 0x1077AC: le32(0)}),
        Case("t6", {"eax": E3_SLOT, "edx": E3_REC, "ecx": 0},
             {E3_SLOT + 0x58: b"\x01", E3_SLOT + 0x52: b"\x52\x53", E3_REC + 0x51: b"\x00",
              0x104740: le32(E3_OUT), E3_OUT + 0x1C: le32(0x00003000), E3_OUT + 0x56: b"\x34\x12",
              0x1014EC: le32(P45_PSET), **P45_PTRS, **P45_CHARS, 0xE5100: le32(0xE5100)}),
        Case("t7", {"eax": E3_SLOT, "edx": E3_REC, "ecx": 0},
             {E3_SLOT + 0x58: b"\x01", E3_SLOT + 0x52: b"\x52\x53", E3_REC + 0x51: b"\x01",
              0x104740: le32(E3_REC2), E3_REC2 + 0x1C: le32(0x00003001), E3_REC2 + 0x56: b"\x34\x12",
              0x1014EC: le32(P45_PSET), **P45_PTRS, **P45_CHARS}),
        Case("t8", {"eax": E3_SLOT, "edx": E3_REC, "ecx": 0},
             {E3_SLOT + 0x58: b"\x01", E3_SLOT + 0x52: b"\x52\x53", E3_REC + 0x51: b"\x02",
              0x104740: le32(E3_REC2), E3_REC2 + 0x1C: le32(0x00003001), E3_REC2 + 0x56: b"\x34\x12",
              0x1014EC: le32(P45_PSET), **P45_PTRS, 0x1077B4: le32(P45_SLOT3), **P45_CHARS}),
    ], calls=(SPAWN, RELEASE, ANIM_BEGIN), eax_mask=0,
         mutants=("@mutant", "@guard", "@st1", "@pset", "@release", "@side", "@desc", "@a5")),
]
# ---- track P batch C1: the callee rows (record 2026-10-03-reverse-c1) -----------------------------
# The ported callees the P1-P3 rows stub, each with its own row so the dependent rows close (P3
# record §P3.10). Every field a row writes carries a sentinel that differs from what it writes and
# every neighbour byte is seeded (the P-track checklist); the two records E3_REC/E3_REC2 and the
# two slots DS_SLOTS/DS_SLOTS+0x94 are the E3 fixtures.

# Both records seeded slot-distinct: the low record A (0x32..) and B (0x62..); every byte of
# +0x32..+0x39, +0x41..+0x47 and +0x0C/+0x1C/+0x2C/+0x34/+0x42/+0x43/+0x59/+0x74 sentinels.
def c1_rec(rec, base):
    return {rec + 0x32: bytes(range(base, base + 8)),           # +0x32..+0x39
            rec + 0x41: bytes(range(base + 0x0F, base + 0x16))}  # +0x41..+0x47


C1_A = c1_rec(E3_REC, 0x32)
C1_B = c1_rec(E3_REC2, 0x62)
C1_PTRS = dict(SLOT_PTRS)


def c1_slots(seed0, seed1):
    return {DS_SLOTS + 0x42: seed0[0:3], DS_SLOTS + 0x52: seed0[3:6], DS_SLOTS + 0x5C: seed0[6:9],
            DS_SLOTS + 0x94 + 0x42: seed1[0:3], DS_SLOTS + 0x94 + 0x52: seed1[3:6],
            DS_SLOTS + 0x94 + 0x5C: seed1[6:9]}


C1_SLOT_SEED0 = b"\x42\xff\x44\x52\x53\x54\x5c\x5d\x5e"       # +0x42..+0x44, +0x52..+0x54, +0x5c..+0x5e
C1_SLOT_SEED1 = b"\x62\xef\x64\x72\x73\x74\x7c\x7d\x7e"
C1_PSET = 0x10A800                # the pset base 0x1014EC points at (32-byte entries)
C1_STR = 0x10A700                 # the animation-stream words the 0x2BC30 cases walk
C1_18 = {E3_REC + 0x16: b"\x16\x17\x18\x19\x1a\x1b", E3_REC + 0x1C: b"\x1c\x1c\x1c\x1c",
         E3_REC2 + 0x16: b"\x26\x27\x28\x29\x2a\x2b", E3_REC2 + 0x1C: b"\x2c\x2c\x2c\x2c"}

# The callee declarations C1's rows stub (record §C1.2). Clobbers are E.callee_clobbers over the
# image (re-derived by test_each_stub_declares_the_registers_its_callee_clobbers).
C1_SLOT_LATCH = E.Call(0x186D0, ("eax",))
C1_RECORD_X = E.Call(0x18714, ("eax",))
C1_FACING = E.Call(0x18B04, ("eax",))
C1_FACING0 = E.Call(0x18AF8, (), mode="real")
C1_PAL_REL = E.Call(0x33864, ("eax",))
C1_PAL_ACQ = E.Call(0x33754, ("eax",))
C1_ANIM_OPCODE = E.Call(0x2B2A0, ("eax", "edx", "ebx"), clobbers=("ebx", "edx"))
C1_SPRITE_ID = E.Call(0x2A408, ("eax", "edx"), clobbers=("edx",))
C1_POSE = E.Call(0x39F40, ("eax", "edx", "ebx", "ecx", "s0"), pop=4, clobbers=("ebx", "edx"))
C1_36638 = E.Call(0x36638, ("eax", "edx"), clobbers=("edx",))
C1_AI_DIST = E.Call(0x187FC, ())
C1_1883C = E.Call(0x1883C, ("eax", "edx", "ebx"), clobbers=("ebx", "edx"))
C1_3B8D8 = E.Call(0x3B8D8, ("eax", "edx"), clobbers=("edx",))
C1_3B90C = E.Call(0x3B90C, ("eax", "edx"), clobbers=("edx",))
C1_GEOM = E.Call(0x1DDF4, ("eax", "edx", "ebx"), clobbers=("ebx", "edx"))
C1_18B44 = E.Call(0x18B44, ("eax",))
C1_189FC = E.Call(0x189FC, ("eax",))
C1_18A4C = E.Call(0x18A4C, ("eax",))
C1_39EFC = E.Call(0x39EFC, ("eax",))
C1_FLAGS = 0x10A600                # the 16 flag bytes the 0x18C14 cases poke
C1_18C14_SEED = {
    DS_SLOTS + 0x42: b"\x40\x41\x42\x43", DS_SLOTS + 0x52: b"\x52\x53\x54\x55",
    DS_SLOTS + 0x5F: b"\x5f\x60\x61\x62\x63", DS_SLOTS + 0x8A: b"\x8a",
    DS_SLOTS + 0x94 + 0x42: b"\x50\x51\x52\x53", DS_SLOTS + 0x94 + 0x52: b"\x62\x63\x64\x65",
    DS_SLOTS + 0x94 + 0x5F: b"\x6f\x70\x71\x72\x73", DS_SLOTS + 0x94 + 0x74: b"\x74\x75\x76\x77",
    E3_REC + 0x61: b"\x61", 0x100AF8: b"\x00\x00\x00\x00",
}


def c1_18c14(cid, flag, val, hit, stubs=None, extra=None):
    flags = [2] * 16
    flags[flag] = val
    pokes = {**C1_PTRS, **C1_18C14_SEED, C1_FLAGS: bytes(flags)}
    pokes.update(extra or {})
    return Case(cid, {"eax": 0, "edx": C1_FLAGS, "ebx": 0, "ecx": 0}, pokes, stubs or {})


def c1_18c14_cases():
    out = []
    for flag in range(16):
        for val in (0, 1):
            for hit in (False, True):
                cid = "g%X%s%s" % (flag, val, "h" if hit else "n")
                stubs, extra = {}, {}
                if flag == 0:
                    le = (val == 1) if hit else (val == 0)
                    extra[0x100AF8] = le32(0 if le else 1)
                elif flag == 1:
                    if val == 0:
                        extra[DS_SLOTS + 0x94 + 0x74] = b"\x01\x00" if hit else b"\x00\x00"
                        extra[DS_SLOTS + 0x94 + 0x76] = b"\x00\x00" if hit else b"\x01\x00"
                    else:
                        extra[DS_SLOTS + 0x94 + 0x74] = b"\x01\x00" if hit else b"\x01\x00"
                        extra[DS_SLOTS + 0x94 + 0x76] = b"\x00\x00" if hit else b"\x02\x00"
                elif flag == 0xF:
                    extra[DS_SLOTS + 0x43] = b"\x04" if hit == (val == 0) else b"\x00"
                elif flag in (2, 3, 6, 4):
                    v = {2: (0, 1), 3: (1, 0), 6: (7, 0), 4: (2, 0)}[flag][0 if hit == (val == 0) else 1]
                    extra[DS_SLOTS + 0x94 + 0x54] = bytes([v])
                elif flag in (5, 9, 0xA, 0xD, 0xE):
                    addr = {5: 0x1DDF4, 9: 0x189FC, 0xA: 0x18A4C, 0xD: 0x39EFC, 0xE: 0x3B298}[flag]
                    stubs[addr] = 1 if hit == (val == 0) else 0
                elif flag == 7:
                    extra[DS_SLOTS + 0x94 + 0x62] = b"\x01" if hit == (val == 0) else b"\x00"
                elif flag == 8:
                    extra[DS_SLOTS + 0x94 + 0x42] = b"\x08" if hit == (val == 0) else b"\x00"
                elif flag == 0xB:
                    extra[E3_REC + 0x61] = b"\x01" if hit == (val == 0) else b"\x00"
                elif flag == 0xC:
                    extra[DS_SLOTS + 0x94 + 0x53] = b"\x0a" if hit == (val == 0) else b"\x0b"
                out.append(c1_18c14(cid, flag, val, hit, stubs, extra))
    return out



C1_2BC30_SEED = {
    E3_REC + 0x08: le32(0x08080808), E3_REC + 0x0C: b"\x0c" * 8, E3_REC + 0x20: b"\x20" * 8,
    E3_REC + 0x28: b"\xff\xff\xff\xff", E3_REC + 0x50: b"\x50\x51\x52\x53",
    E3_REC + 0x54: b"\x54\x55\x00\x00\x58\x59", E3_REC + 0x5F: b"\x5f\x60\x61\x62",
    0x1014EC: le32(C1_PSET), C1_PSET: b"\x00\x00\x02\x02",
}
C1_35838_SEED = {
    **C1_PTRS, DS_SLOTS + 0x40: b"\x40\x41\x42\x43\x44\x45\x46\x47", DS_SLOTS + 0x4C: b"\x4c\x4d\x4e\x4f\x50\x51\x52\x53",
    DS_SLOTS + 0x7A: b"\x01", 0x104B00: b"\x11\x00",
}
C1_468D8_SEED = {
    **C1_PTRS, DS_SLOTS + 0x10: le32(0), DS_SLOTS + 0x52: b"\x52", DS_SLOTS + 0x54: b"\x54",
    E3_REC + 0x24: le32(0x24242424),
    DS_SLOTS + 0x94 + 0x10: le32(0), DS_SLOTS + 0x94 + 0x52: b"\x62", DS_SLOTS + 0x94 + 0x54: b"\x64",
    E3_REC2 + 0x24: le32(0x34343434),
}


C1_SPECS = [
    # 0x3C148/0x3C16C (record §48-C): clear the side's record fields; EAX = side, mask 0.
    Spec("fighter_3c148", 0x3C148, [
        Case("c0", {"eax": 0}, {**C1_PTRS, **C1_A, **C1_B}),
        Case("c1", {"eax": 1}, {**C1_PTRS, **C1_A, **C1_B}),
    ], eax_mask=0, mutants=("@mutant", "@side", "@width")),
    Spec("fighter_3c16c", 0x3C16C, [
        Case("c0", {"eax": 0}, {**C1_PTRS, **C1_A, **C1_B}),
        Case("c1", {"eax": 1}, {**C1_PTRS, **C1_A, **C1_B}),
    ], eax_mask=0, mutants=("@mutant", "@side", "@width")),
    # 0x39A10: EAX = rec, EDX = value; the side is rec+0x51, zero-extended (`and eax,0xff` at
    # 0x39A14); the store is the word +0x74 of the side's slot (0x107824 = 0x1077B0 + 0x74).
    Spec("fighter_39a10", 0x39A10, [
        Case("t0", {"eax": E3_REC, "edx": 0x12345678},
             {**C1_PTRS, E3_REC + 0x51: b"\x00", DS_SLOTS + 0x72: b"\x72\x73\x74\x75\x76\x77",
              DS_SLOTS + 0x94 + 0x72: b"\x82\x83\x84\x85\x86\x87"}),
        Case("t1", {"eax": E3_REC2, "edx": 0xFFFF},
             {**C1_PTRS, E3_REC2 + 0x51: b"\x01", DS_SLOTS + 0x72: b"\x72\x73\x74\x75\x76\x77",
              DS_SLOTS + 0x94 + 0x72: b"\x82\x83\x84\x85\x86\x87"}),
    ], eax_mask=0, mutants=("@mutant", "@side")),
    # 0x36D98: EAX = the slot (DS_001077B0 + side*0x94); the side is its record's +0x51.
    Spec("fighter_36d98", 0x36D98, [
        Case("r0", {"eax": DS_SLOTS},
             {**C1_PTRS, E3_REC + 0x51: b"\x00", **c1_slots(C1_SLOT_SEED0, C1_SLOT_SEED1),
              0x1078F2: b"\x11\x22"}),
        Case("r1", {"eax": DS_SLOTS + 0x94},
             {**C1_PTRS, E3_REC2 + 0x51: b"\x01", **c1_slots(C1_SLOT_SEED0, C1_SLOT_SEED1),
              0x1078F2: b"\x11\x22"}),
    ], eax_mask=0, mutants=("@mutant", "@side", "@and")),
    # 0x18BD4: EAX = the 16 flag bytes; fill them with 2 (`mov byte [eax+7]` last, 0x18C0B).
    Spec("fighter_18bd4", 0x18BD4, [
        Case("b0", {"eax": E3_OUT}, {E3_OUT: b"\xaa" * 16 + b"\xbb"}),
        Case("b1", {"eax": E3_SLOT + 0x60}, {E3_SLOT + 0x60: b"\xcc" * 16 + b"\xdd"}),
    ], eax_mask=0, mutants=("@mutant", "@val", "@off")),
    # 0x34D8C: the +0x59 palette-flash pair, only when byte 0x1078FA == 2 (`xor eax,eax; mov
    # al,[0x1078fa]; cmp eax,2` 0x34D90..0x34D9A). EAX = side.
    Spec("fighter_34d8c", 0x34D8C, [
        Case("f0", {"eax": 0}, {**C1_PTRS, **C1_A, **C1_B, 0x1078FA: b"\x01"}),
        Case("f1", {"eax": 0}, {**C1_PTRS, **C1_A, **C1_B, 0x1078FA: b"\x02"}),
        Case("f2", {"eax": 1}, {**C1_PTRS, **C1_A, **C1_B, 0x1078FA: b"\x02"}),
    ], eax_mask=0, mutants=("@mutant", "@side")),
    # 0x3C358: EAX = side; 0x33950 runs on both sides (allow); every field seeded on both slots
    # and both records (the +0x0C and +0x1C dwords included).
    Spec("fighter_3c358", 0x3C358, [
        Case("s0", {"eax": 0},
             {**C1_PTRS, **c1_slots(C1_SLOT_SEED0, C1_SLOT_SEED1),
              DS_SLOTS + 0x0C: le32(0x0C0C0C0C), DS_SLOTS + 0x94 + 0x0C: le32(0x1C1C1C1C),
              **C1_A, **C1_B, E3_REC + 0x1C: le32(0x1C1C1C1C), E3_REC2 + 0x1C: le32(0x2C2C2C2C)}),
        Case("s1", {"eax": 1},
             {**C1_PTRS, **c1_slots(C1_SLOT_SEED0, C1_SLOT_SEED1),
              DS_SLOTS + 0x0C: le32(0x0C0C0C0C), DS_SLOTS + 0x94 + 0x0C: le32(0x1C1C1C1C),
              **C1_A, **C1_B, E3_REC + 0x1C: le32(0x1C1C1C1C), E3_REC2 + 0x1C: le32(0x2C2C2C2C)}),
    ], allow_calls=(0x33950,), eax_mask=0, mutants=("@mutant", "@side", "@42")),
    # 0x3C190: EAX = side, EDX = v; 0x1A570(side)'s AL negates v; the word +0x34 of the side's
    # slot takes the low 16 bits.
    Spec("fighter_3c190", 0x3C190, [
        Case("v0", {"eax": 0, "edx": 0x1234}, {**C1_PTRS, **C1_A, **C1_B}, {0x1A570: 0}),
        Case("v1", {"eax": 0, "edx": 0x1234}, {**C1_PTRS, **C1_A, **C1_B}, {0x1A570: 1}),
        Case("v2", {"eax": 1, "edx": 0xFFFF8000}, {**C1_PTRS, **C1_A, **C1_B}, {0x1A570: 1}),
    ], calls=(BIT15,), eax_mask=0, mutants=("@mutant", "@arg", "@width")),
    # 0x3C480: EAX = rec, EDX = stream, s0 = frame bits; 0x339AC runs on both sides (allow), the
    # anchor pair and 0x2BC30 are stubbed (the caller reads none of their effects).
    Spec("fighter_3c480", 0x3C480, [
        Case("a0", {"eax": E3_REC, "edx": 0xE1234, "s0": 0x40000000},
             {**C1_PTRS, E3_REC + 0x51: b"\x00", E3_REC + 0x18: le32(0x18181818),
              DS_SLOTS + 0x2C: le32(0x2C2C2C2C), DS_SLOTS + 0x94 + 0x2C: le32(0x3C3C3C3C),
              E3_REC2 + 0x18: le32(0x28282828)}),
        Case("a1", {"eax": E3_REC2, "edx": 0xE5678, "s0": 0x3F800000},
             {**C1_PTRS, E3_REC2 + 0x51: b"\x01", E3_REC + 0x18: le32(0x18181818),
              DS_SLOTS + 0x2C: le32(0x2C2C2C2C), DS_SLOTS + 0x94 + 0x2C: le32(0x3C3C3C3C),
              E3_REC2 + 0x18: le32(0x28282828)}),
    ], allow_calls=(0x339AC,), calls=(ANCHOR, ANIM_BEGIN, ANCHORX), eax_mask=0,
       mutants=("@mutant", "@order", "@side")),
    # 0x188AC (P1's ANCHOR): EAX = side, EDX = x, EBX = y; the record's +0x18/+0x1C
    # take x/y, then 0x186D0(side) re-latches the slot.
    Spec("fighter_188ac", 0x188AC, [
        Case("a0", {"eax": 0, "edx": 0x11223344, "ebx": 0x55667788},
             {**C1_PTRS, E3_REC + 0x16: b"\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f",
              E3_REC2 + 0x16: b"\x66\x67\x68\x69\x6a\x6b\x6c\x6d\x6e\x6f"}),
        Case("a1", {"eax": 1, "edx": 0x99AABBCC, "ebx": 0xDDEEFF00},
             {**C1_PTRS, E3_REC + 0x16: b"\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f",
              E3_REC2 + 0x16: b"\x66\x67\x68\x69\x6a\x6b\x6c\x6d\x6e\x6f"}),
    ], calls=(C1_SLOT_LATCH,), eax_mask=0, mutants=("@mutant", "@side", "@latch")),
    # 0x188DC (P3's ANCHORX): EAX = side, EDX = x; the slot's +0x2C takes x, then
    # the record's +0x18 takes 0x18714(side)'s result.
    Spec("fighter_188dc", 0x188DC, [
        Case("d0", {"eax": 0, "edx": 0x11223344},
             {**C1_PTRS, **C1_18, DS_SLOTS + 0x2A: b"\x2a\x2b\x2c\x2c\x2e\x2f",
              DS_SLOTS + 0x94 + 0x2A: b"\x3a\x3b\x3c\x3c\x3e\x3f"}, {0x18714: 0x11111111}),
        Case("d1", {"eax": 1, "edx": 0x99AABBCC},
             {**C1_PTRS, **C1_18, DS_SLOTS + 0x2A: b"\x2a\x2b\x2c\x2c\x2e\x2f",
              DS_SLOTS + 0x94 + 0x2A: b"\x3a\x3b\x3c\x3c\x3e\x3f"}, {0x18714: 0x22222222}),
    ], calls=(C1_RECORD_X,), eax_mask=0, mutants=("@mutant", "@side", "@arg", "@eax")),
    # 0x18AF8: `xor eax,eax; call 0x18B04` (recorded, stubbed) then `mov eax,1` falls
    # through into 0x18B04's body with EAX = 1 (no second call). The body runs on both
    # sides: 0x33950 allow, then 0x18714(side) into the record's +0x18 and the +0x29
    # bit 0x40 by the two slots' +0x2C comparison (side 1 only: the fall-through's EAX).
    Spec("fighter_18af8", 0x18AF8, [
        Case("f0", {}, {**C1_PTRS, E3_REC2 + 0x29: b"\x29", E3_REC2 + 0x16: b"\x26\x27\x28\x29\x2a\x2b",
                        DS_SLOTS + 0x2C: le32(0x22222222), DS_SLOTS + 0x94 + 0x2C: le32(0x11111111)},
             {0x18B04: 0, 0x18714: 0x33333333}),
        # f1 takes the `and 0xBF` arm (equal +0x2C values), so its +0x29 seed has bit 6 set:
        # the clear is observable (f0's `<` arm keeps the `or 0x40` and its seed bit 6 clear).
        Case("f1", {}, {**C1_PTRS, E3_REC2 + 0x29: b"\x69", E3_REC2 + 0x16: b"\x26\x27\x28\x29\x2a\x2b",
                        DS_SLOTS + 0x2C: le32(0x11111111), DS_SLOTS + 0x94 + 0x2C: le32(0x11111111)},
             {0x18B04: 1, 0x18714: 0x44444444}),
    ], allow_calls=(0x33950,), calls=(C1_FACING, C1_RECORD_X), eax_mask=0,
       mutants=("@mutant", "@once", "@le")),
    # 0x2A17C (P1's PALETTE): EAX = rec, EDX = word, EBX = handle; pset+2 takes the word
    # with 0x800 when rec+0x5F is set; a zero handle returns; else the old pset+0x18 is
    # released (0x33864) and 0x33754(handle) is stored.
    Spec("fighter_2a17c", 0x2A17C, [
        Case("p0", {"eax": E3_REC, "edx": 0x1234, "ebx": 0},
             {**C1_PTRS, 0x1014EC: le32(C1_PSET), 0x1014F4: le32(E3_REC), E3_REC + 0x56: b"\x00\x00",
              E3_REC + 0x5F: b"\x00", C1_PSET + 0x02: b"\x02\x02", C1_PSET + 0x18: le32(0)}),
        Case("p1", {"eax": E3_REC, "edx": 0x5678, "ebx": 0x77},
             {**C1_PTRS, 0x1014EC: le32(C1_PSET), 0x1014F4: le32(E3_REC), E3_REC + 0x56: b"\x00\x00",
              E3_REC + 0x5F: b"\x01", C1_PSET + 0x02: b"\x02\x02", C1_PSET + 0x18: le32(0)},
             {0x33754: 0xAABBCCDD}),
        Case("p2", {"eax": E3_REC, "edx": 0x9ABC, "ebx": 0x88},
             {**C1_PTRS, 0x1014EC: le32(C1_PSET), 0x1014F4: le32(E3_REC), E3_REC + 0x56: b"\x00\x00",
              E3_REC + 0x5F: b"\x00", C1_PSET + 0x02: b"\x02\x02", C1_PSET + 0x18: le32(0x1234)},
             {0x33754: 0x11223344}),
    ], calls=(C1_PAL_REL, C1_PAL_ACQ), eax_mask=0, mutants=("@mutant", "@order", "@arg", "@early"),
       # 0x2A1AC's EBX==0 return runs before 0x2A1E6's test, so the 0x2A1F5 store is dead
       unhit_named={0x2A1F5: "dead: EBX == 0 already returned at 0x2A1AC"}),
    # 0x2BC30 (E3's ANIM_BEGIN): EAX = rec, EDX = stream, s0 = frame bits; the stream
    # opcode walk calls 0x2B2A0 (stub EAX 0 continues, 1 stops, 2 stops after +2) and
    # the pset word takes 0x2A408's low word.
    Spec("fighter_2bc30", 0x2BC30, [
        Case("n0", {"eax": E3_REC, "edx": C1_STR, "s0": 0x40000000},
             {**C1_2BC30_SEED, C1_STR: b"\x00\x00\x00\x00"}, {0x2A408: 0x1234}),
        Case("n1", {"eax": E3_REC, "edx": C1_STR, "s0": 0x40000000},
             {**C1_2BC30_SEED, C1_STR: b"\x00\x80\x00\x00"}, {0x2A408: 0x2345, 0x2B2A0: 1}),
        Case("n2", {"eax": E3_REC, "edx": C1_STR, "s0": 0x40000000},
             {**C1_2BC30_SEED, C1_STR: b"\x00\x80\x00\x00"}, {0x2A408: 0x3456, 0x2B2A0: 2}),
        Case("n3", {"eax": E3_REC, "edx": C1_STR, "s0": 0x40000000},
             {**C1_2BC30_SEED, C1_STR: b"\x00\x80\x00\x80\x00\x00\x00\x00"},
             {0x2A408: 0x4567, 0x2B2A0: 0}),
    ], calls=(C1_ANIM_OPCODE, C1_SPRITE_ID), eax_mask=0, mutants=("@mutant", "@order", "@frame"),
       # 0x2BC61 `xor eax,eax` makes the 0x2BC66 test always take the jump: the fild block is dead
       unhit_named={0x2BC6D: "dead: EAX is 0 at 0x2BC66 (`xor eax,eax` at 0x2BC61)"}),
    # 0x39FB0: EAX = slot; the side is its record's +0x51; 0x33A10 runs on both sides
    # (allow); 0x18B04(ctx[1]) then the 0x39F40 pose with (ctx[1], -0x50, 0x64, 0xF, 0x14).
    Spec("fighter_39fb0", 0x39FB0, [
        Case("g0", {"eax": DS_SLOTS}, {**C1_PTRS, E3_REC + 0x51: b"\x00"}),
        Case("g1", {"eax": DS_SLOTS + 0x94}, {**C1_PTRS, E3_REC2 + 0x51: b"\x01"}),
    ], allow_calls=(0x33A10,), calls=(C1_FACING, C1_POSE), eax_mask=0,
       mutants=("@mutant", "@side", "@order")),
    # 0x3A95C: EAX = side, EDX = b; 0x33A10 runs on both sides (allow); 0x188AC(ctx[1],
    # ctx[5].+0x18, 0); the own slot (ctx[3]) 0x10/0x0A/0/0x10=0; 0x2BC30(ctx[5], the char
    # stream, 3.0); ctx[3].+0x7E = byte[0xBECF8] + b.
    Spec("fighter_3a95c", 0x3A95C, [
        Case("c0", {"eax": 0, "edx": 0x21},
             {**C1_PTRS, E3_REC2 + 0x18: le32(0x18181818), E3_REC + 0x18: le32(0x28282828),
              DS_SLOTS + 0x10: b"\x20\x20\x20\x20",
              DS_SLOTS + 0x52: b"\x52\x53\x54\x55", DS_SLOTS + 0x94 + 0x52: b"\x62\x63\x64\x65",
              DS_SLOTS + 0x94 + 0x10: b"\x10\x10\x10\x10", DS_SLOTS + 0x94 + 0x7A: b"\x00",
              DS_SLOTS + 0x94 + 0x7E: b"\x7e\x7f"}),
        Case("c1", {"eax": 1, "edx": 0x42},
             {**C1_PTRS, E3_REC2 + 0x18: le32(0x18181818), E3_REC + 0x18: le32(0x28282828),
              DS_SLOTS + 0x52: b"\x52\x53\x54\x55", DS_SLOTS + 0x94 + 0x52: b"\x62\x63\x64\x65",
              DS_SLOTS + 0x10: b"\x20\x20\x20\x20", DS_SLOTS + 0x94 + 0x10: b"\x10\x10\x10\x10",
              DS_SLOTS + 0x7A: b"\x01",
              DS_SLOTS + 0x7E: b"\x6e\x6f"}),
    ], allow_calls=(0x33A10,), calls=(ANCHOR, ANIM_BEGIN), eax_mask=0,
       mutants=("@mutant", "@side", "@arg")),
    # 0x35838 (P3's DIRS): EAX = slot, EDX = rec, EBX = dirbits; the rec+0x28 bit
    # 0x4000 gate and dirbits 0x2000/0x1000 select the two animation streams (or
    # 0x36638 when mode 0x104B00 == 0x22).
    Spec("fighter_35838", 0x35838, [
        Case("s0", {"eax": DS_SLOTS, "edx": E3_REC, "ebx": 0},
             {**C1_35838_SEED, E3_REC + 0x28: b"\x00\x00"}),
        Case("s1", {"eax": DS_SLOTS, "edx": E3_REC, "ebx": 0x2000},
             {**C1_35838_SEED, E3_REC + 0x28: b"\x00\x00"}),
        Case("s2", {"eax": DS_SLOTS, "edx": E3_REC, "ebx": 0},
             {**C1_35838_SEED, E3_REC + 0x28: b"\x00\x00", 0x104B00: b"\x22\x00"}),
        Case("s3", {"eax": DS_SLOTS, "edx": E3_REC, "ebx": 0x1000},
             {**C1_35838_SEED, E3_REC + 0x28: b"\x00\x40"}),
        Case("s4", {"eax": DS_SLOTS, "edx": E3_REC, "ebx": 0},
             {**C1_35838_SEED, E3_REC + 0x28: b"\x00\x40", 0x104B00: b"\x22\x00"}),
        Case("s5", {"eax": DS_SLOTS, "edx": E3_REC, "ebx": 0},
             {**C1_35838_SEED, E3_REC + 0x28: b"\x00\x40"}),
    ], calls=(ANIM_BEGIN, C1_36638), eax_mask=0, mutants=("@mutant", "@side", "@order")),
    # 0x468D8 (P3's PRED): EAX = side; 0x33A10 runs on both sides (allow); 1 when the
    # side slot's +0x10 handler is 0x22BEC, its record's +0x24 low 31 bits are clear and
    # +0x54 is not 2; else 1 when +0x52 is 7.
    Spec("fighter_468d8", 0x468D8, [
        Case("h0", {"eax": 0}, {**C1_468D8_SEED, DS_SLOTS + 0x10: le32(0x22BEC), E3_REC + 0x24: le32(0)}),
        Case("h1", {"eax": 0}, {**C1_468D8_SEED, DS_SLOTS + 0x10: le32(0x1234), DS_SLOTS + 0x52: b"\x07"}),
        Case("h2", {"eax": 0}, {**C1_468D8_SEED, DS_SLOTS + 0x10: le32(0x22BEC),
                                E3_REC + 0x24: le32(0x80000000)}),
        Case("h3", {"eax": 0}, {**C1_468D8_SEED, DS_SLOTS + 0x10: le32(0x22BEC), E3_REC + 0x24: le32(0),
                                DS_SLOTS + 0x54: b"\x02"}),
        Case("h4", {"eax": 0}, {**C1_468D8_SEED, DS_SLOTS + 0x10: le32(0x22BEC), E3_REC + 0x24: le32(0),
                                DS_SLOTS + 0x52: b"\x07"}),
        Case("h5", {"eax": 0}, {**C1_468D8_SEED, DS_SLOTS + 0x10: le32(0x1234)}),
        Case("h6", {"eax": 1}, {**C1_468D8_SEED, DS_SLOTS + 0x94 + 0x10: le32(0x22BEC),
                                E3_REC2 + 0x24: le32(0)}),
    ], allow_calls=(0x33A10,), eax_mask=0xFF, mutants=("@mutant", "@eq", "@side")),
    # 0x3C208 (P2's PLACE): EAX = side, EDX = dist; both slots latch, 0x18AF8 sets the
    # facing flags, both records clear +0x34/+0x43/+0x42; then 0x187FC's |d| against
    # |dist|: farther, 0x1883C(other, ±gap, 0) by 0x1A570; closer, 0x3B8D8(other, ±gap)
    # then 0x1883C or 0x3B90C and the two anchors (slot[other]+0x2C ± want).
    Spec("fighter_3c208", 0x3C208, [
        Case("c0", {"eax": 0, "edx": 0x100},
             {**C1_PTRS, **C1_A, **C1_B, DS_SLOTS + 0x2C: le32(0x1234), DS_SLOTS + 0x94 + 0x2C: le32(0x5678)},
             {0x187FC: 0x200, 0x1A570: 0}),
        Case("c1", {"eax": 0, "edx": 0x100},
             {**C1_PTRS, **C1_A, **C1_B, DS_SLOTS + 0x2C: le32(0x1234), DS_SLOTS + 0x94 + 0x2C: le32(0x5678)},
             {0x187FC: 0x200, 0x1A570: 1}),
        Case("c2", {"eax": 0, "edx": 0xFFFFFF00},
             {**C1_PTRS, **C1_A, **C1_B, DS_SLOTS + 0x2C: le32(0x1234), DS_SLOTS + 0x94 + 0x2C: le32(0x5678)},
             {0x187FC: 0x200, 0x1A570: 0}),
        Case("c3", {"eax": 0, "edx": 0x100},
             {**C1_PTRS, **C1_A, **C1_B, DS_SLOTS + 0x2C: le32(0x1234), DS_SLOTS + 0x94 + 0x2C: le32(0x5678)},
             {0x187FC: 0x50, 0x1A570: 0, 0x3B8D8: 0}),
        Case("c4", {"eax": 0, "edx": 0x100},
             {**C1_PTRS, **C1_A, **C1_B, DS_SLOTS + 0x2C: le32(0x1234), DS_SLOTS + 0x94 + 0x2C: le32(0x5678)},
             {0x187FC: 0x50, 0x1A570: 0, 0x3B8D8: 1, 0x3B90C: 0x777}),
        Case("c5", {"eax": 0, "edx": 0x100},
             {**C1_PTRS, **C1_A, **C1_B, DS_SLOTS + 0x2C: le32(0x1234), DS_SLOTS + 0x94 + 0x2C: le32(0x5678)},
             {0x187FC: 0x50, 0x1A570: 1, 0x3B8D8: 0}),
        Case("c6", {"eax": 0, "edx": 0x100},
             {**C1_PTRS, **C1_A, **C1_B, DS_SLOTS + 0x2C: le32(0x1234), DS_SLOTS + 0x94 + 0x2C: le32(0x5678)},
             {0x187FC: 0x50, 0x1A570: 1, 0x3B8D8: 1, 0x3B90C: 0x888}),
        Case("c7", {"eax": 0, "edx": 0x100},
             {**C1_PTRS, **C1_A, **C1_B, DS_SLOTS + 0x2C: le32(0x1234), DS_SLOTS + 0x94 + 0x2C: le32(0x5678)},
             {0x187FC: 0xFFFFFFF0, 0x1A570: 0, 0x3B8D8: 0}),
    ], allow_calls=(0x33950,),
       calls=(C1_SLOT_LATCH, C1_FACING0, C1_FACING, C1_RECORD_X, C1_AI_DIST, BIT15, C1_1883C, C1_3B8D8, C1_3B90C, ANCHORX),
       eax_mask=0, mutants=("@mutant", "@abs", "@arg", "@early")),
    # 0x18C14 (P2's CHECKS): EAX = side, EDX = the 16 flag bytes (poked; the binding reads
    # mem[EDX]); 0x33950 allow; each flag 2 skips, 0/1 tests (flag 0 rewrites to 4/3); flags
    # 7/0xD/0xE store ctx[2]+0x8A and 7/0xD call 0x18B44; returns 0 only when every check
    # passes. The 64 cases: every flag, value 0/1, condition holding and not.
    Spec("fighter_18c14", 0x18C14, c1_18c14_cases(),
       allow_calls=(0x33950,), calls=(C1_GEOM, C1_18B44, C1_189FC, C1_18A4C, C1_39EFC, DISPATCH),
       eax_mask=0xFF, mutants=("@mutant", "@store", "@live")),
]
# ---- track P batch 7: the remaining unported direct callees and the targets outside E2 (record
# 2026-10-04-reverse-p7) --------------------------------------------------------------------------------
# The callees the batch stubs, args in the port's C order and clobbers from E.callee_clobbers.
P7_2BDB8 = E.Call(0x2BDB8, ("eax", "edx"), clobbers=("edx",))
P7_2BDB8_R = E.Call(0x2BDB8, ("eax", "edx"), mode="real")
P7_2BDE8_R = E.Call(0x2BDE8, ("eax",), mode="real")
P7_23960 = E.Call(0x23960, ("eax",))
P7_3A9D8 = E.Call(0x3A9D8, ("eax", "edx"), clobbers=("edx",))
P7_2B150 = E.Call(0x2B150, ("eax",), clobbers=("esi", "edi", "ebp"))
P7_41310 = E.Call(0x41310, ("eax", "edx"))
P7_49444 = E.Call(0x49444, ("eax",), clobbers=("edi", "ebp"))
P7_RNG   = E.Call(0x5D7DC, ("eax",), mode="real")
P7_22404 = E.Call(0x22404, ("eax",))
P7_2BCF4 = E.Call(0x2BCF4, ("eax", "edx"), clobbers=("edx",))
P7_2A17C = E.Call(0x2A17C, ("eax", "edx", "ebx"), clobbers=("edx",))

# 0x2BDB8 (record §P7.2): the record's +0x2B bit 1, the byte 0x105BEE = the argument, 0x105BEC =
# the argument - 1 and +0x24 = (float)(u8)the argument. b1 (0x100) separates the low byte from the
# whole argument for all three stores; b2 (0x1234FF) pins 255.0f.
P7_2BDB8_SEED = {E3_REC + 0x24: le32(0x24242424), E3_REC + 0x2B: b"\x2b\x2c",
                 0x105BEE: b"\xee\xef", 0x105BEC: b"\xec\xed"}
P7_2BDB8_SEED2 = {E3_REC2 + 0x24: le32(0x34343434), E3_REC2 + 0x2B: b"\x3b\x3c",
                  0x105BEE: b"\xee\xef", 0x105BEC: b"\xec\xed"}


# 0x213F4/0x3E424 (record §P7.2): the other side's slot by rec+0x51 ^ 1; its record on
# 0xC9148/0xC9120[its char] at 3.0; its +0x58 = 0; 0x3E424's +0x41 |= 0x80; its +0x52 = 0xC; then
# 0x2BDB8(3) on the own and the other record. The records and the +0x2B/0x24 stores carry sentinels.
def p7_213f4_case(cid, rec, orec, oside, char):
    oslot = DS_SLOTS + oside * 0x94
    return Case(cid, {"eax": rec, "edx": 0x1234},
                {**SLOT_PTRS, **P6_PTRS,
                 rec + 0x51: bytes([oside ^ 1]), orec + 0x51: bytes([oside]),
                 oslot + 0x7A: bytes([char]), oslot + 0x58: b"\x58\x59",
                 oslot + 0x52: b"\x52\x53", oslot + 0x41: b"\x41\x42",
                 rec + 0x24: le32(0x24242424), rec + 0x2B: b"\x2b\x2c",
                 orec + 0x24: le32(0x34343434), orec + 0x2B: b"\x3b\x3c",
                 0x105BEE: b"\xee\xef", 0x105BEC: b"\xec\xed"})


# 0x23960 (record §P7.4): 0x105B4C = 0; with the other side's slot set, four 0xA839C spawns at its
# +0x2C + 0x600/-0x600/+0x1000/-0x1000, y = the other record's +0x30 >> 16; each child's +0x24 +=
# (float)rng_next(6) and +0x20 the same. The spawn stub returns P6_CHILD.
P7_23960_SEED = {**SLOT_PTRS, **P6_PTRS,
                 E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
                 DS_SLOTS + 0x94 + 0x2C: le32(0x00001000),
                 E3_REC2 + 0x30: le32(0x12345678),
                 P6_CHILD + 0x20: le32(0x20202020), P6_CHILD + 0x24: le32(0x40400000),
                 0x105B4C: b"\x4c\x4d"}

# 0x23AE0 (record §P7.6): the other slot's record's +0x29 bit 3, +0x2E = 0x64, +0x4E = 1; the
# char's word from 0x23AC4 (0x46B6..0x46BA, default 0x46B9) sought on it; the palette handle
# 0x105FEBC; the word 0x105B4C = 1.
def p7_23ae0_case(cid, rec, char):
    return Case(cid, {"eax": rec, "edx": 0x1234},
                {**SLOT_PTRS, **P6_PTRS,
                 rec + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
                 DS_SLOTS + 0x94 + 0x7A: bytes([char]), E3_REC2 + 0x29: b"\x29\x2a",
                 E3_REC2 + 0x2E: b"\x2e\x2f", E3_REC2 + 0x4E: b"\x4e\x4f",
                 0x105B4C: b"\x4c\x4d"})


# 0x4B03C (record §P7.7): the byte (slot+8's record +0x48) - 0x20 selects 0x10/0xE/0x15/0xC/0xA
# (0xD default) and the slot's +0x5A loses it to 0; voice 0xD6, voice 0xCE, the record dead,
# 0x41310 on the slot's +0x21/+0x20 (mode 0x22/0x24 excluded), the 0x1088A4/0x1088A2 counters,
# then 0x49444.
P7_4B03C_SEED = {E3_REC + 0x14: le32(E3_SLOT), E3_SLOT + 8: le32(E3_OUT),
                 E3_SLOT + 0x20: b"\x00", E3_SLOT + 0x21: b"\x01",
                 0x10780A: b"\x5a", 0x10780A + 0x94: b"\x5b",
                 0x10889E + 1: b"\x9e", 0x1088A4: b"\xa4", 0x1088A2 + 1: b"\xa2",
                 0x104B00: b"\x11\x00", E3_OUT + 0x48: b"\x20"}

P7_SPECS = [
    Spec("fighter_2bdb8", 0x2BDB8, [
        Case("b0", {"eax": E3_REC, "edx": 0x42}, P7_2BDB8_SEED),
        Case("b1", {"eax": E3_REC, "edx": 0x100}, P7_2BDB8_SEED),
        Case("b2", {"eax": E3_REC2, "edx": 0x1234FF}, P7_2BDB8_SEED2),
    ], eax_mask=0, mutants=("@mutant", "@and", "@dec", "@width")),
    # 0x213F0 (record §P7.2): a bare `ret`; the row asserts no byte changes.
    Spec("fighter_213f0", 0x213F0, [
        Case("r0", {"eax": E3_REC, "edx": 0x1234}, {E3_REC + 0x2B: b"\x28"}),
    ], eax_mask=0, mutants=("@mutant",)),
    Spec("fighter_213f4", 0x213F4, [
        p7_213f4_case("s0", E3_REC, E3_REC2, 0, 3),
        p7_213f4_case("s1", E3_REC2, E3_REC, 1, 2),
        Case("o0", {"eax": E3_REC, "edx": 0x1234},
             {DS_SLOTS - 8: le32(DS_SLOTS) + le32(0), E3_REC + 0x51: b"\x00",
              E3_REC + 0x24: le32(0x24242424), E3_REC + 0x2B: b"\x2b\x2c",
              0x105BEE: b"\xee\xef", 0x105BEC: b"\xec\xed"}),
    ], calls=(ANIM_BEGIN, P7_2BDB8_R), eax_mask=0,
       mutants=("@mutant", "@stream", "@side", "@order")),
    Spec("fighter_3e424", 0x3E424, [
        p7_213f4_case("s0", E3_REC, E3_REC2, 0, 3),
        p7_213f4_case("s1", E3_REC2, E3_REC, 1, 2),
        Case("o0", {"eax": E3_REC, "edx": 0x1234},
             {DS_SLOTS - 8: le32(DS_SLOTS) + le32(0), E3_REC + 0x51: b"\x00",
              E3_REC + 0x24: le32(0x24242424), E3_REC + 0x2B: b"\x2b\x2c",
              0x105BEE: b"\xee\xef", 0x105BEC: b"\xec\xed"}),
    ], calls=(ANIM_BEGIN, P7_2BDB8_R), eax_mask=0,
       mutants=("@mutant", "@bit", "@side", "@order")),
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
] + E3_SPECS + P1_SPECS + P1_ANIM_SPECS + P2_SPECS + P3_SPECS + P6_SPECS + P45_SPECS + C1_SPECS + P7_SPECS


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
