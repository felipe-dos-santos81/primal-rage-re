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
# bl,[ecx+0x41]` overwrites it): q2 runs side 0 with rec+0x51 = 1.
def p3_dirs(name, entry, mutants):
    return Spec(name, entry, [
        Case("q0", {"eax": E3_SLOT, "edx": E3_REC, "ebx": 0}, {E3_SLOT + 0x41: b"\x40", E3_REC + 0x51: b"\x00"}),
        Case("q1", {"eax": E3_SLOT, "edx": E3_REC, "ebx": 0}, {E3_SLOT + 0x41: b"\xbf", E3_REC + 0x51: b"\x00"},
             {0x1A570: 0}),
        Case("q2", {"eax": E3_SLOT, "edx": E3_REC, "ebx": 0}, {E3_SLOT + 0x41: b"\xbf", E3_REC + 0x51: b"\x01"},
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
    p3_dirs("fighter_48964", 0x48964, ("@mutant", "@side")),
    p3_dirs("fighter_489a0", 0x489A0, ("@mutant",)),
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
# +0x54 selects 0x39FB0 (2) or 0x3A95C; the signed word 0xBEDD8 (10 in the image, `sar 0x10` of the dword
# 0xBEDD6) is poked negative in h4, where a zero-extended read differs.
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
        p3_47688("h4", 0, 0, timer=0xFFF0),
    ], allow_calls=(0x33950,), calls=(DISPATCH, POSE, PIVOT, STANCE, TIMER), eax_mask=0,
       mutants=("@mutant", "@pivot", "@zext")),
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
] + E3_SPECS + P1_SPECS + P1_ANIM_SPECS + P2_SPECS + P3_SPECS


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
