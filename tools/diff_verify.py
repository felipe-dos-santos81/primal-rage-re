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


def le16(v):
    return (v & 0xFFFF).to_bytes(2, "little")


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
DISPATCH = E.Call(0x3B298, ("eax", "edx"), clobbers=("edx",))
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
RELEASE = E.Call(0x2AD40, ("eax", "edx"), clobbers=("edx",))   # void (the seam on release_record)


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
P7_2BDB8_R = E.Call(0x2BDB8, ("eax", "edx"), mode="real")
P7_2BDE8_R = E.Call(0x2BDE8, ("eax",), mode="real")
P7_23960 = E.Call(0x23960, ("eax",))
P7_3A9D8 = E.Call(0x3A9D8, ("eax", "edx"), clobbers=("edx",))
P7_2B150 = E.Call(0x2B150, ("eax",))
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
                 DS_SLOTS + 0x94 + 0x7A: bytes([char]), E3_REC2 + 0x29: b"\x21\x2a",
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
    # 0x224EC (record §P7.3): ctx; 0x22404(ctx[0]); the own slot's +0x57 = 2. Both slots' +0x57
    # are seeded differently, so @side (ctx[3]) shows.
    Spec("fighter_224ec", 0x224EC, [
        Case("s0", {"eax": E3_REC, "edx": 0x1234},
             {**SLOT_PTRS, E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
              DS_SLOTS + 0x57: b"\x57", DS_SLOTS + 0x94 + 0x57: b"\x67"}),
        Case("s1", {"eax": E3_REC2, "edx": 0x5678},
             {**SLOT_PTRS, E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
              DS_SLOTS + 0x57: b"\x57", DS_SLOTS + 0x94 + 0x57: b"\x67"}),
    ], allow_calls=(0x339AC,), calls=(P7_22404,), eax_mask=0,
       mutants=("@mutant", "@side", "@order")),
    # 0x2BDE8 (record §P7.3): +0x20 = +0x24 + (-1.0f, 0x809B8); +0x2B bit 1 clear. s1's +0x2B
    # has only bit 1 set (the clear observable), s0's 0xFF and s2's 0x03.
    Spec("fighter_2bde8", 0x2BDE8, [
        Case("s0", {"eax": E3_REC}, {E3_REC + 0x20: le32(0x20202020), E3_REC + 0x24: le32(0x40400000),
                                     E3_REC + 0x2B: b"\xff\x2c", E3_REC + 0x2A: b"\x2a"}),
        Case("s1", {"eax": E3_REC2}, {E3_REC2 + 0x20: le32(0x20202020), E3_REC2 + 0x24: le32(0xC0000000),
                                      E3_REC2 + 0x2B: b"\x02\x3c", E3_REC2 + 0x2A: b"\x3a"}),
        Case("s2", {"eax": E3_REC}, {E3_REC + 0x20: le32(0x20202020), E3_REC + 0x24: le32(0x3F800000),
                                     E3_REC + 0x2B: b"\x03\x2c", E3_REC + 0x2A: b"\x2a"}),
    ], eax_mask=0, mutants=("@mutant", "@byte")),
    # 0x36114 (record §P7.3): +0x24 = 2.0; the slot's +0x58 + 1; the signed word 0xBDA4C[char]
    # negated unless the record's +0x28 bit 14; 0xBDA5A[char] into +0x36; 0x1883C(side, +0x34's
    # word, +0x36's word); then 0x2BDE8 on the own and the other record (when set).
    Spec("fighter_36114", 0x36114, [
        Case("s0", {"eax": E3_REC, "edx": 0x1234},
             {**SLOT_PTRS, **P6_PTRS, E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
              E3_REC + 0x14: le32(DS_SLOTS), E3_REC + 0x28: b"\x00\x40",
              E3_REC + 0x24: le32(0x24242424), E3_REC + 0x32: b"\x32\x33\x34\x35\x36\x37",
              E3_REC + 0x20: le32(0x20202020), E3_REC + 0x2B: b"\x2b\x2c",
              E3_REC2 + 0x20: le32(0x40404040), E3_REC2 + 0x24: le32(0x44444444),
              E3_REC2 + 0x2B: b"\x4b\x4c", DS_SLOTS + 0x7A: b"\x03", DS_SLOTS + 0x58: b"\x58\x59"}),
        Case("s1", {"eax": E3_REC, "edx": 0x1234},
             {**SLOT_PTRS, **P6_PTRS, E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
              E3_REC + 0x14: le32(DS_SLOTS), E3_REC + 0x28: b"\x00\x00",
              E3_REC + 0x24: le32(0x24242424), E3_REC + 0x32: b"\x32\x33\x34\x35\x36\x37",
              E3_REC + 0x20: le32(0x20202020), E3_REC + 0x2B: b"\x2b\x2c",
              E3_REC2 + 0x20: le32(0x40404040), E3_REC2 + 0x24: le32(0x44444444),
              E3_REC2 + 0x2B: b"\x4b\x4c", DS_SLOTS + 0x7A: b"\x03", DS_SLOTS + 0x58: b"\x58\x59"}),
        Case("s2", {"eax": E3_REC2, "edx": 0x5678},
             {**SLOT_PTRS, **P6_PTRS, E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
              E3_REC2 + 0x14: le32(DS_SLOTS + 0x94), E3_REC2 + 0x28: b"\x00\x40",
              E3_REC2 + 0x24: le32(0x44444444), E3_REC2 + 0x32: b"\x42\x43\x44\x45\x46\x47",
              E3_REC2 + 0x20: le32(0x40404040), E3_REC2 + 0x2B: b"\x4b\x4c",
              E3_REC + 0x20: le32(0x20202020), E3_REC + 0x24: le32(0x24242424),
              E3_REC + 0x2B: b"\x2b\x2c", DS_SLOTS + 0x94 + 0x7A: b"\x02",
              DS_SLOTS + 0x94 + 0x58: b"\x68\x69"}),
        Case("n0", {"eax": E3_REC, "edx": 0x1234},
             {E3_REC + 0x14: le32(0), E3_REC + 0x24: le32(0x24242424), E3_REC + 0x2B: b"\x2b\x2c"}),
        Case("o0", {"eax": E3_REC, "edx": 0x1234},
             {DS_SLOTS - 8: le32(DS_SLOTS) + le32(0), E3_REC + 0x51: b"\x00",
              E3_REC + 0x14: le32(DS_SLOTS), E3_REC + 0x28: b"\x00\x40",
              E3_REC + 0x24: le32(0x24242424), E3_REC + 0x32: b"\x32\x33\x34\x35\x36\x37",
              E3_REC + 0x20: le32(0x20202020), E3_REC + 0x2B: b"\x2b\x2c",
              DS_SLOTS + 0x7A: b"\x03", DS_SLOTS + 0x58: b"\x58\x59"}),
    ], calls=(C1_1883C, P7_2BDE8_R), eax_mask=0,
       mutants=("@neg", "@side", "@anchor", "@char", "@table", "@order", "@second")),
    Spec("fighter_23960", 0x23960, [
        Case("s0", {"eax": E3_REC}, P7_23960_SEED, {0x2AE14: P6_CHILD}),
        Case("s1", {"eax": E3_REC2},
             {**SLOT_PTRS, **P6_PTRS,
              E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
              DS_SLOTS + 0x2C: le32(0xFFFFF000),
              E3_REC + 0x30: le32(0x00008000),
              P6_CHILD + 0x20: le32(0x20202020), P6_CHILD + 0x24: le32(0x40400000),
              0x105B4C: b"\x4c\x4d"}, {0x2AE14: P6_CHILD}),
        Case("o0", {"eax": E3_REC}, {DS_SLOTS - 8: le32(DS_SLOTS) + le32(0), E3_REC + 0x51: b"\x00",
                                     0x105B4C: b"\x4c\x4d"}),
    ], calls=(SPAWN, P7_RNG), eax_mask=0,
       mutants=("@off", "@y", "@desc", "@float", "@x")),
    # 0x23A7C (record §P7.4): the D100 target at 0xE18C4. With the record's +0x14 slot set: a
    # 0xA8388 spawn with a5 = word +0x56 | 0x400; the child's +0x14 = the slot, +0x51 = the side;
    # the record's +0x4B = the child's +0x56; then 0x23960(child).
    Spec("fighter_23a7c", 0x23A7C, [
        Case("s0", {"eax": E3_REC}, {E3_REC + 0x14: le32(E3_SLOT), E3_REC + 0x51: b"\x00",
                                     E3_REC + 0x56: b"\x34\x12", E3_REC + 0x4B: b"\x4b",
                                     E3_SLOT + 0x14: le32(0x14141414), E3_SLOT + 0x51: b"\x51",
                                     P6_CHILD + 0x56: b"\x5a", P6_CHILD + 0x14: le32(0x14141414)},
             {0x2AE14: P6_CHILD}),
        Case("s1", {"eax": E3_REC2}, {E3_REC2 + 0x14: le32(E3_SLOT + 0x94), E3_REC2 + 0x51: b"\x01",
                                      E3_REC2 + 0x56: b"\x78\x56", E3_REC2 + 0x4B: b"\x4b",
                                      E3_SLOT + 0x94 + 0x14: le32(0x24242424),
                                      E3_SLOT + 0x94 + 0x51: b"\x61",
                                      P6_CHILD + 0x56: b"\x5a", P6_CHILD + 0x14: le32(0x24242424)},
             {0x2AE14: P6_CHILD}),
        Case("n0", {"eax": E3_REC}, {E3_REC + 0x14: le32(0), E3_REC + 0x4B: b"\x4b"}),
    ], calls=(SPAWN, P7_23960), eax_mask=0,
       mutants=("@a5", "@slot", "@side", "@mutant", "@order")),
    # 0x3A9D8 (record §P7.5): 0x3A95C's twin (the stream table 0xC9030). EAX = side, EDX = b.
    # 0x33A10 runs on both sides (allow); 0x188AC(ctx[1], the other record's +0x18, 0); the own
    # slot (ctx[3]) 0x10/0x0A/0/0x10 = 0; 0x2BC30(ctx[5], the char stream, 3.0); ctx[3].+0x7E =
    # byte 0xBECF8 + (u8)b.
    Spec("fighter_3a9d8", 0x3A9D8, [
        Case("s0", {"eax": 0, "edx": 0x1234},
             {**SLOT_PTRS, E3_REC2 + 0x18: le32(0x18181818), E3_REC + 0x18: le32(0x28282828),
              DS_SLOTS + 0x10: b"\x20\x20\x20\x20",
              DS_SLOTS + 0x52: b"\x52\x53\x54\x55", DS_SLOTS + 0x94 + 0x52: b"\x62\x63\x64\x65",
              DS_SLOTS + 0x94 + 0x10: b"\x10\x10\x10\x10", DS_SLOTS + 0x7A: b"\x03",
              DS_SLOTS + 0x94 + 0x7A: b"\x02", DS_SLOTS + 0x7E: b"\x7e\x7f",
              DS_SLOTS + 0x94 + 0x7E: b"\x8e\x8f"}),
        Case("s1", {"eax": 1, "edx": 0x0080},
             {**SLOT_PTRS, E3_REC2 + 0x18: le32(0x18181818), E3_REC + 0x18: le32(0x28282828),
              DS_SLOTS + 0x10: b"\x20\x20\x20\x20",
              DS_SLOTS + 0x52: b"\x52\x53\x54\x55", DS_SLOTS + 0x94 + 0x52: b"\x62\x63\x64\x65",
              DS_SLOTS + 0x94 + 0x10: b"\x10\x10\x10\x10", DS_SLOTS + 0x7A: b"\x03",
              DS_SLOTS + 0x94 + 0x7A: b"\x02", DS_SLOTS + 0x7E: b"\x7e\x7f",
              DS_SLOTS + 0x94 + 0x7E: b"\x8e\x8f"}),
    ], allow_calls=(0x33A10,), calls=(ANCHOR, ANIM_BEGIN), eax_mask=0,
       mutants=("@side", "@stream", "@anchor", "@slot", "@frame", "@byte")),
    # 0x48254 (record §P7.5): 0x482E4's twin (the stream 0xED87A and 0x3A9D8). The flag
    # 0x108392[own side] selects the other record on 0xC8F40[other char] at 5.0, else
    # 0x3A9D8(other side, 0xF) and 0x39834(other side, the own slot's +0x5F).
    Spec("fighter_48254", 0x48254, [
        Case("s0", {"eax": E3_REC, "edx": 0x1234},
             {**SLOT_PTRS, E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
              DS_SLOTS + 0x94 + 0x7A: b"\x02", DS_SLOTS + 0x7A: b"\x03",
              DS_SLOTS + 0x5F: b"\x5f", DS_SLOTS + 0x94 + 0x5F: b"\x9f",
              0x108392: b"\x01", 0x108392 + 1: b"\x00"}),
        Case("s1", {"eax": E3_REC2, "edx": 0x5678},
             {**SLOT_PTRS, E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
              DS_SLOTS + 0x94 + 0x7A: b"\x02", DS_SLOTS + 0x7A: b"\x03",
              DS_SLOTS + 0x5F: b"\x5f", DS_SLOTS + 0x94 + 0x5F: b"\x9f",
              0x108392: b"\x00", 0x108392 + 1: b"\x01"}),
        Case("z0", {"eax": E3_REC, "edx": 0x1234},
             {**SLOT_PTRS, E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
              DS_SLOTS + 0x94 + 0x7A: b"\x04", DS_SLOTS + 0x7A: b"\x03",
              DS_SLOTS + 0x5F: b"\x5f", DS_SLOTS + 0x94 + 0x5F: b"\x9f",
              0x108392: b"\x00", 0x108392 + 1: b"\x00"}),
    ], calls=(ANIM_BEGIN, P7_3A9D8, POSE), eax_mask=0,
       mutants=("@byte", "@side", "@stream", "@pose", "@char", "@order")),
    Spec("fighter_23ae0", 0x23AE0,
         [p7_23ae0_case("c%d" % i, E3_REC, i) for i in range(7)]
         + [p7_23ae0_case("c7", E3_REC, 7),
            Case("o0", {"eax": E3_REC}, {DS_SLOTS - 8: le32(DS_SLOTS) + le32(0), E3_REC + 0x51: b"\x00",
                                         0x105B4C: b"\x4c\x4d"})],
         calls=(P7_2BCF4, P7_2A17C), eax_mask=0,
         mutants=("@char", "@val", "@side", "@pal", "@word", "@bit", "@seek", "@order")),
    # 0x29C78 (record §P7.6): every entry of the table 0x29C5C is 0x29CA8, so the body is
    # 0x2A17C(rec, 0, 0x105FEBC) alone. d0's own char 7 exercises the degenerate switch.
    Spec("fighter_29c78", 0x29C78, [
        Case("s0", {"eax": E3_REC, "edx": 0x1234}, {**SLOT_PTRS, E3_REC + 0x51: b"\x00"}),
        Case("s1", {"eax": E3_REC2, "edx": 0x5678}, {**SLOT_PTRS, E3_REC2 + 0x51: b"\x01"}),
        Case("d0", {"eax": E3_REC, "edx": 0x1234}, {**SLOT_PTRS, E3_REC + 0x51: b"\x00",
                                                    DS_SLOTS + 0x7A: b"\x07"}),
    ], calls=(P7_2A17C,), eax_mask=0, mutants=("@pal", "@rec")),
    Spec("fighter_4b03c", 0x4B03C,
         [Case("t%d" % i, {"eax": E3_REC}, {**P7_4B03C_SEED, E3_OUT + 0x48: bytes([0x20 + i])})
          for i in range(6)]
         + [
             Case("t6", {"eax": E3_REC}, {**P7_4B03C_SEED, E3_OUT + 0x48: b"\x26"}),
             Case("t7", {"eax": E3_REC}, {**P7_4B03C_SEED, E3_OUT + 0x48: b"\x1f"}),
             Case("z0", {"eax": E3_REC}, {**P7_4B03C_SEED, E3_OUT + 0x48: b"\x20", 0x10780A: b"\x08"}),
             Case("m0", {"eax": E3_REC}, {**P7_4B03C_SEED, E3_OUT + 0x48: b"\x20", 0x104B00: b"\x22\x00"}),
             Case("m1", {"eax": E3_REC}, {**P7_4B03C_SEED, E3_OUT + 0x48: b"\x20", 0x104B00: b"\x24\x00"}),
             Case("eq0", {"eax": E3_REC}, {**P7_4B03C_SEED, E3_OUT + 0x48: b"\x20", E3_SLOT + 0x21: b"\x00"}),
             Case("o0", {"eax": E3_REC}, {E3_REC + 0x14: le32(0)}),
         ],
         calls=(VOICE, VOICE, P7_2B150, P7_41310, P7_49444), eax_mask=0,
         mutants=("@type", "@val", "@sub", "@mode", "@plus", "@eq", "@dead", "@cam", "@tear", "@order", "@voice")),
]

# ---- track P batch C2: the verification-only callee rows (record 2026-10-04-reverse-c2) ----------
# The ported callees the P/C1 rows stub that still have no row (record §C2.1). Every field a row
# writes carries a sentinel that differs from what it writes and every neighbour byte is seeded (the
# P-track checklist 1-2). A row's callee is stubbed through its seam on BOTH sides (the port's C
# returns at its PR_SEAM), so the callee's own behaviour is its own row's claim while this row's
# call list, its arguments and the memory at each call are compared (E3 §E3.10, §E3.12).
DS_1088C2 = 0x1088C2           # DS_001088C2: the byte 0x13244 sets
DS_1014EC = 0x1014EC           # DS_001014EC: the pset pool base
DS_1014F4 = 0x1014F4           # DS_001014F4: the actor pool base (the port's in_pool check)
DS_A8A98 = 0x000A8A98          # DS_000A8A98: the per-character palette-row pointer table
DS_105B34 = 0x00105B34         # DS_00105B34: the per-side palette index byte table
DS_BE018 = 0x000BE018          # DS_000BE018: the arena wall
DS_104B00 = 0x00104B00         # DS_00104B00: the fight mode word
DS_100C1D = 0x00100C1D         # DS_00100C1D: the once-per-fight dust latch
C2_PSET = 0x10A600             # zero BSS: a pset base for DS_001014EC
C2_POOL = 0x10A2B8             # 0x68-aligned zero BSS: an actor-pool record base
C2_REC = 0x10A2B8
C2_ROW0 = 0x10A6C0             # zero BSS: two palette rows for 0x29C08/0x29BC8
C2_STR = 0x10A700              # zero BSS: animation-stream words for 0x2A408
C2_ROW1 = 0x10A6E0
C2_ENTRY = 0x10A800            # zero BSS: a palette-table entry {handle; refcount; ...}
DS_104529 = 0x00104529         # DS_00104529: the dust descriptor selector bit
FIGHTER_A17D4 = 0x000A17D4     # the dust spawn x word pair
FIGHTER_A17D6 = 0x000A17D6     # the dust spawn y word pair
C2_LATCH = E.Call(0x186D0, ("eax",))

C2_SPECS = [
    # 0x39280 (record §48-C): EAX = side; clear the side slot's +0x5D and +0x43 bit 2. Mask 0: both
    # callers overwrite EAX at once (0x350D0's +0x41 bit-2 arm and its DS_001078F2 block).
    Spec("fighter_state_39280", 0x39280, [
        Case("s0", {"eax": 0}, {DS_SLOTS + 0x42: b"\x42", DS_SLOTS + 0x43: b"\x07", DS_SLOTS + 0x44: b"\x44",
                                DS_SLOTS + 0x5C: b"\x5c", DS_SLOTS + 0x5D: b"\x5d", DS_SLOTS + 0x5E: b"\x5e",
                                DS_SLOTS + 0x94 + 0x42: b"\x52", DS_SLOTS + 0x94 + 0x43: b"\x06",
                                DS_SLOTS + 0x94 + 0x44: b"\x54", DS_SLOTS + 0x94 + 0x5C: b"\x6c",
                                DS_SLOTS + 0x94 + 0x5D: b"\x6d", DS_SLOTS + 0x94 + 0x5E: b"\x6e"}),
        Case("s1", {"eax": 1}, {DS_SLOTS + 0x42: b"\x42", DS_SLOTS + 0x43: b"\x07", DS_SLOTS + 0x44: b"\x44",
                                DS_SLOTS + 0x5C: b"\x5c", DS_SLOTS + 0x5D: b"\x5d", DS_SLOTS + 0x5E: b"\x5e",
                                DS_SLOTS + 0x94 + 0x42: b"\x52", DS_SLOTS + 0x94 + 0x43: b"\x06",
                                DS_SLOTS + 0x94 + 0x44: b"\x54", DS_SLOTS + 0x94 + 0x5C: b"\x6c",
                                DS_SLOTS + 0x94 + 0x5D: b"\x6d", DS_SLOTS + 0x94 + 0x5E: b"\x6e"}),
    ], eax_mask=0, mutants=("@mutant", "@side", "@width")),
    # 0x33864 (record §C1.2): EAX = the 0x10-byte palette-table entry; refcount-- and clear the
    # handle at zero. Mask 0: its only caller 0x2A1C8 stores over EAX at once (0x2A1D1).
    Spec("palette_release", 0x33864, [
        Case("r0", {"eax": C2_ENTRY}, {C2_ENTRY: le32(0x11223344), C2_ENTRY + 4: le32(1), C2_ENTRY + 8: le32(0x88888888)}),
        Case("r1", {"eax": C2_ENTRY}, {C2_ENTRY: le32(0x11223344), C2_ENTRY + 4: le32(2), C2_ENTRY + 8: le32(0x88888888)}),
        Case("r2", {"eax": C2_ENTRY}, {C2_ENTRY: le32(0x11223344), C2_ENTRY + 4: le32(0x100),
                                       C2_ENTRY + 8: le32(0x88888888)}),
    ], eax_mask=0, mutants=("@mutant", "@width", "@off")),
    # 0x13244 (record §48-P): DS_001088C2 = 1; no argument, no callee. Mask 0 (the caller 0x3F34C
    # falls through into the spawn tail and overwrites EAX).
    Spec("fighter_13244", 0x13244, [
        Case("c0", {}, {DS_1088C2: b"\x00"}),
        Case("c1", {}, {DS_1088C2: b"\xa5"}),
    ], eax_mask=0, mutants=("@mutant", "@addr", "@val")),
    # 0x29C08 (record §P4.5): EAX = side, EDX = char; the handle is DSD(DSD(0xA8A98+ch*4) +
    # DSB(0x105B34+side)*4). Mask full: 0x3E160 stores the result in a descriptor's +0x10.
    Spec("fighter_29c08", 0x29C08, [
        Case("c0", {"eax": 0, "edx": 0}, {DS_A8A98: le32(C2_ROW0), DS_A8A98 + 4: le32(C2_ROW1),
                                          DS_105B34: b"\x00\x01",
                                          C2_ROW0: le32(0x11111111), C2_ROW0 + 4: le32(0x22222222),
                                          C2_ROW1: le32(0x33333333), C2_ROW1 + 4: le32(0x44444444)}),
        Case("c1", {"eax": 1, "edx": 0}, {DS_A8A98: le32(C2_ROW0), DS_A8A98 + 4: le32(C2_ROW1),
                                          DS_105B34: b"\x00\x01",
                                          C2_ROW0: le32(0x11111111), C2_ROW0 + 4: le32(0x22222222),
                                          C2_ROW1: le32(0x33333333), C2_ROW1 + 4: le32(0x44444444)}),
        Case("c2", {"eax": 0, "edx": 1}, {DS_A8A98: le32(C2_ROW0), DS_A8A98 + 4: le32(C2_ROW1),
                                          DS_105B34: b"\x00\x01",
                                          C2_ROW0: le32(0x11111111), C2_ROW0 + 4: le32(0x22222222),
                                          C2_ROW1: le32(0x33333333), C2_ROW1 + 4: le32(0x44444444)}),
        Case("c3", {"eax": 0x80, "edx": 0}, {DS_A8A98: le32(C2_ROW0), DS_105B34 + 0x80: b"\xff",
                                             C2_ROW0 + 0x3FC: le32(0xAABBCCDD), C2_ROW0 - 4: le32(0xDEADBEEF)}),
    ], eax_mask=0xFFFFFFFF, mutants=("@side", "@char", "@sext")),
    # 0x2A148 (record §42-A): EAX = rec, DL = flag; rec+0x5F = flag and the pset's +2 word =
    # rec+0x2E | 0x800 when the stored flag is non-zero. Mask 0 (the callers ignore EAX; the raw
    # leaves its 0x800 scratch there). The port's actor_pset checks the pool: DS_001014F4 = C2_REC.
    Spec("actor_pset_flag_5f", 0x2A148, [
        Case("f0", {"eax": C2_REC, "edx": 0}, {DS_1014EC: le32(C2_PSET), DS_1014F4: le32(C2_POOL),
                                               C2_REC + 0x56: b"\x01\x00", C2_REC + 0x2E: b"\x26\x26",
                                               C2_REC + 0x5F: b"\x5f", C2_PSET + 0x22: b"\x02\x02",
                                               C2_PSET + 0x2C: b"\x2c\x2c"}),
        Case("f1", {"eax": C2_REC, "edx": 1}, {DS_1014EC: le32(C2_PSET), DS_1014F4: le32(C2_POOL),
                                               C2_REC + 0x56: b"\x01\x00", C2_REC + 0x2E: b"\x26\x26",
                                               C2_REC + 0x5F: b"\x00", C2_PSET + 0x22: b"\x02\x02",
                                               C2_PSET + 0x2C: b"\x2c\x2c"}),
        Case("f2", {"eax": C2_REC, "edx": 0x100}, {DS_1014EC: le32(C2_PSET), DS_1014F4: le32(C2_POOL),
                                                   C2_REC + 0x56: b"\x01\x00", C2_REC + 0x2E: b"\x26\x26",
                                                   C2_REC + 0x5F: b"\x5f", C2_PSET + 0x22: b"\x02\x02",
                                                   C2_PSET + 0x2C: b"\x2c\x2c"}),
    ], eax_mask=0, mutants=("@mutant", "@word", "@pset")),
    # 0x29BC8 (record §P6.2): EAX = side, EBX = rec, EDX = char; resolve the palette handle and
    # 0x2A17C(rec, 0, handle). The callee is stubbed through its seam on both sides.
    Spec("fighter_29bc8", 0x29BC8, [
        Case("c0", {"eax": 0, "ebx": C2_REC, "edx": 0}, {DS_A8A98: le32(C2_ROW0), DS_A8A98 + 4: le32(C2_ROW1),
                                                         DS_105B34: b"\x00\x01",
                                                         C2_ROW0: le32(0x11111111), C2_ROW1: le32(0x22222222)}),
        Case("c1", {"eax": 1, "ebx": C2_REC, "edx": 1}, {DS_A8A98: le32(C2_ROW0), DS_A8A98 + 4: le32(C2_ROW1),
                                                         DS_105B34: b"\x00\x01",
                                                         C2_ROW0: le32(0x11111111), C2_ROW1: le32(0x22222222)}),
    ], calls=(PALETTE,), eax_mask=0, mutants=("@side", "@rec")),
    # 0x1890C (record §P6.2): EAX = side, EDX = y; rec+0x1C += y - slot+0x30, latched twice. The
    # latch is stubbed on both sides, so the case's slot/record bytes survive.
    Spec("hit_anchor_y", 0x1890C, [
        Case("y0", {"eax": 0, "edx": 0x100}, {**SLOT_PTRS, DS_SLOTS + 0x30: le32(0x50),
                                              E3_REC + 0x1C: le32(0x1000), DS_SLOTS + 0x94 + 0x30: le32(0x60),
                                              E3_REC2 + 0x1C: le32(0x2000)}),
        Case("y1", {"eax": 0, "edx": 0xFFFFFF00}, {**SLOT_PTRS, DS_SLOTS + 0x30: le32(0x50),
                                                   E3_REC + 0x1C: le32(0x1000), DS_SLOTS + 0x94 + 0x30: le32(0x60),
                                                   E3_REC2 + 0x1C: le32(0x2000)}),
        Case("y2", {"eax": 1, "edx": 0x200}, {**SLOT_PTRS, DS_SLOTS + 0x30: le32(0x50),
                                              E3_REC + 0x1C: le32(0x1000), DS_SLOTS + 0x94 + 0x30: le32(0x60),
                                              E3_REC2 + 0x1C: le32(0x2000)}),
    ], calls=(C2_LATCH,), eax_mask=0, mutants=("@mutant", "@side", "@field")),
    # 0x187FC (record §C1.2): latch both slots, return slot0+0x2C - slot1+0x2C. Mask full.
    Spec("ai_distance", 0x187FC, [
        Case("d0", {}, {**SLOT_PTRS, DS_SLOTS + 0x2C: le32(0x1000), DS_SLOTS + 0x94 + 0x2C: le32(0x800)}),
        Case("d1", {}, {**SLOT_PTRS, DS_SLOTS + 0x2C: le32(0x800), DS_SLOTS + 0x94 + 0x2C: le32(0x1000)}),
    ], calls=(C2_LATCH,), eax_mask=0xFFFFFFFF, mutants=("@mutant", "@order")),
]

# 0x18540/0x18350: the latch's own callees (record §48-V.5); stubbed in 0x186D0/0x18714's rows and
# seamed in port/src/game/fighter.c. 0x18540 saves and restores everything; 0x18350 clobbers EDX.
C2_18540 = E.Call(0x18540, ("eax",))
C2_18350 = E.Call(0x18350, ("eax", "edx"), clobbers=("edx",))
C2_189FC = E.Call(0x189FC, ("eax",))
C2_18A4C = E.Call(0x18A4C, ("eax",))
C2_39EFC = E.Call(0x39EFC, ("eax",))
C2_3B8D8 = E.Call(0x3B8D8, ("eax", "edx"), clobbers=("edx",))
C2_3B90C = E.Call(0x3B90C, ("eax", "edx"), clobbers=("edx",))
C2_ACTOR = 0x1014EC              # DS_001014EC: the pset pool base (1A570's actor word)
C2_ANCHOR = 0x100AF0             # DS_00100AF0: the per-side screen anchor
C2_OFFS = 0x100AB0               # DS_00100AB0/AB4: the per-side screen offset pair

C2_SPECS += [
    # 0x186D0 (record §C1.2): EAX = side; with slot+0x42 bit 3 the record's +0x18/+0x1C latch into
    # +0x2C/+0x30, else 0x18540(side) runs, the +0x20 anchor is refreshed (0x18350) and the offset
    # pair is added; +0x41 bit 7 copies +0x2C into +0x34. Mask 0 (every caller ignores EAX).
    Spec("fighter_slot_latch", 0x186D0, [
        Case("l0", {"eax": 0}, {**SLOT_PTRS, DS_SLOTS + 0x20: le32(0x24242424), DS_SLOTS + 0x41: b"\x80",
                                DS_SLOTS + 0x42: b"\x08", DS_SLOTS + 0x2C: le32(0x2C2C2C2C),
                                DS_SLOTS + 0x30: le32(0x30303030), DS_SLOTS + 0x34: le32(0x34343434),
                                E3_REC + 0x18: le32(0x18181818), E3_REC + 0x1C: le32(0x1C1C1C1C),
                                C2_ANCHOR: le32(0x1111) + le32(0x2222), C2_OFFS: le32(0x64) + le32(0x65)}),
        Case("l1", {"eax": 0}, {**SLOT_PTRS, DS_SLOTS + 0x20: le32(0x1111), DS_SLOTS + 0x24: le32(0x24242424),
                                DS_SLOTS + 0x41: b"\x80", DS_SLOTS + 0x42: b"\x00",
                                DS_SLOTS + 0x2C: le32(0x2C2C2C2C), DS_SLOTS + 0x30: le32(0x30303030),
                                DS_SLOTS + 0x34: le32(0x34343434), E3_REC + 0x18: le32(0x18181818),
                                E3_REC + 0x1C: le32(0x1C1C1C1C), C2_ANCHOR: le32(0x1111) + le32(0x2222),
                                C2_OFFS: le32(0x64) + le32(0x65)}),
        Case("l2", {"eax": 0}, {**SLOT_PTRS, DS_SLOTS + 0x20: le32(0x9999), DS_SLOTS + 0x24: le32(0x24242424),
                                DS_SLOTS + 0x41: b"\x00", DS_SLOTS + 0x42: b"\x00",
                                DS_SLOTS + 0x2C: le32(0x2C2C2C2C), DS_SLOTS + 0x30: le32(0x30303030),
                                DS_SLOTS + 0x34: le32(0x34343434), E3_REC + 0x18: le32(0x18181818),
                                E3_REC + 0x1C: le32(0x1C1C1C1C), C2_ANCHOR: le32(0x1111) + le32(0x2222),
                                C2_OFFS: le32(0x64) + le32(0x65)}),
        Case("l3", {"eax": 1}, {**SLOT_PTRS, DS_SLOTS + 0x94 + 0x20: le32(0x24242424),
                                DS_SLOTS + 0x94 + 0x41: b"\x80", DS_SLOTS + 0x94 + 0x42: b"\x08",
                                DS_SLOTS + 0x94 + 0x2C: le32(0x3C3C3C3C), DS_SLOTS + 0x94 + 0x30: le32(0x40404040),
                                DS_SLOTS + 0x94 + 0x34: le32(0x44444444), E3_REC2 + 0x18: le32(0x28282828),
                                E3_REC2 + 0x1C: le32(0x2C2C2C2C), C2_ANCHOR: le32(0x1111) + le32(0x2222),
                                C2_OFFS: le32(0x64) + le32(0x65)}),
    ], calls=(C2_18540, C2_18350), eax_mask=0, mutants=("@mutant", "@side", "@anchor")),
    # 0x18714 (record §C1.2): EAX = side; the record-x: bit 3 set takes rec+0x18, else the latch
    # path returns slot+0x2C - DS_00100AB0[side*8]. Mask full (0x188DC stores the result).
    Spec("hit_record_x", 0x18714, [
        Case("x0", {"eax": 0}, {**SLOT_PTRS, DS_SLOTS + 0x20: le32(0x9999), DS_SLOTS + 0x41: b"\x00",
                                DS_SLOTS + 0x42: b"\x08", DS_SLOTS + 0x2C: le32(0x2C2C2C2C),
                                E3_REC + 0x18: le32(0x18181818), E3_REC + 0x1C: le32(0x1C1C1C1C),
                                C2_ANCHOR: le32(0x1111) + le32(0x2222), C2_OFFS: le32(0x64) + le32(0x65)}),
        Case("x1", {"eax": 0}, {**SLOT_PTRS, DS_SLOTS + 0x20: le32(0x1111), DS_SLOTS + 0x24: le32(0x24242424),
                                DS_SLOTS + 0x41: b"\x00", DS_SLOTS + 0x42: b"\x00",
                                DS_SLOTS + 0x2C: le32(0x2C2C2C2C), E3_REC + 0x18: le32(0x18181818),
                                E3_REC + 0x1C: le32(0x1C1C1C1C), C2_ANCHOR: le32(0x1111) + le32(0x2222),
                                C2_OFFS: le32(0x64) + le32(0x65)}),
        Case("x2", {"eax": 0}, {**SLOT_PTRS, DS_SLOTS + 0x20: le32(0x9999), DS_SLOTS + 0x24: le32(0x24242424),
                                DS_SLOTS + 0x41: b"\x00", DS_SLOTS + 0x42: b"\x00",
                                DS_SLOTS + 0x2C: le32(0x2C2C2C2C), E3_REC + 0x18: le32(0x18181818),
                                E3_REC + 0x1C: le32(0x1C1C1C1C), C2_ANCHOR: le32(0x1111) + le32(0x2222),
                                C2_OFFS: le32(0x64) + le32(0x65)}),
        Case("x3", {"eax": 1}, {**SLOT_PTRS, DS_SLOTS + 0x94 + 0x20: le32(0x9999), DS_SLOTS + 0x94 + 0x42: b"\x08",
                                DS_SLOTS + 0x94 + 0x2C: le32(0x3C3C3C3C), E3_REC2 + 0x18: le32(0x28282828),
                                E3_REC2 + 0x1C: le32(0x2C2C2C2C), C2_ANCHOR: le32(0x1111) + le32(0x2222),
                                C2_OFFS: le32(0x64) + le32(0x65)}),
    ], calls=(C2_18540, C2_18350), eax_mask=0xFFFFFFFF, mutants=("@mutant", "@side", "@anchor")),
    # 0x189FC (record §C1.2): EAX = side; 0x33A10 builds the context (allow), 0x1A570's AL picks the
    # comparison: clear -> slot[side]+0x2C < slot[other]+0x2C, set -> >. Mask 0xFF (`test al,al`).
    Spec("fighter_189fc", 0x189FC, [
        Case("z0", {"eax": 0}, {**SLOT_PTRS, DS_SLOTS + 0x2C: le32(0x100), DS_SLOTS + 0x94 + 0x2C: le32(0x200)},
             {0x1A570: 1}),
        Case("z1", {"eax": 0}, {**SLOT_PTRS, DS_SLOTS + 0x2C: le32(0x200), DS_SLOTS + 0x94 + 0x2C: le32(0x200)},
             {0x1A570: 1}),
        Case("z2", {"eax": 0}, {**SLOT_PTRS, DS_SLOTS + 0x2C: le32(0x300), DS_SLOTS + 0x94 + 0x2C: le32(0x200)},
             {0x1A570: 1}),
        Case("z3", {"eax": 0}, {**SLOT_PTRS, DS_SLOTS + 0x2C: le32(0x300), DS_SLOTS + 0x94 + 0x2C: le32(0x200)},
             {0x1A570: 0}),
        Case("z4", {"eax": 0}, {**SLOT_PTRS, DS_SLOTS + 0x2C: le32(0x100), DS_SLOTS + 0x94 + 0x2C: le32(0x200)},
             {0x1A570: 0}),
        Case("z5", {"eax": 0}, {**SLOT_PTRS, DS_SLOTS + 0x2C: le32(0x300), DS_SLOTS + 0x94 + 0x2C: le32(0x200)},
             {0x1A570: 0x101}),
        Case("z6", {"eax": 1}, {**SLOT_PTRS, DS_SLOTS + 0x2C: le32(0x300), DS_SLOTS + 0x94 + 0x2C: le32(0x100)},
             {0x1A570: 1}),
    ], allow_calls=(0x33A10,), calls=(BIT15,), eax_mask=0xFF, mutants=("@mutant", "@side", "@bit", "@al")),
    # 0x18A4C (record §C1.2): EAX = side; 0x33950's context (allow); 0x1A570 runs real (its own
    # row) so its two calls can disagree; 0x189FC is stubbed with a per-case AL. Mask 0xFF.
    Spec("fighter_18a4c", 0x18A4C, [
        Case("a0", {"eax": 0}, {**SLOT_PTRS, C2_ACTOR: le32(C2_PSET), E3_REC + 0x56: b"\x00\x00",
                                E3_REC2 + 0x56: b"\x01\x00", C2_PSET: b"\x00\x80", C2_PSET + 0x20: b"\x00\x00"},
             {0x189FC: 1}),
        Case("a1", {"eax": 0}, {**SLOT_PTRS, C2_ACTOR: le32(C2_PSET), E3_REC + 0x56: b"\x00\x00",
                                E3_REC2 + 0x56: b"\x01\x00", C2_PSET: b"\x00\x80", C2_PSET + 0x20: b"\x00\x80"},
             {0x189FC: 1}),
        Case("a2", {"eax": 0}, {**SLOT_PTRS, C2_ACTOR: le32(C2_PSET), E3_REC + 0x56: b"\x00\x00",
                                E3_REC2 + 0x56: b"\x01\x00", C2_PSET: b"\x00\x80", C2_PSET + 0x20: b"\x00\x00"},
             {0x189FC: 0}),
        Case("a3", {"eax": 0}, {**SLOT_PTRS, C2_ACTOR: le32(C2_PSET), E3_REC + 0x56: b"\x00\x00",
                                E3_REC2 + 0x56: b"\x01\x00", C2_PSET: b"\x00\x00", C2_PSET + 0x20: b"\x00\x80"},
             {0x189FC: 1}),
        Case("a4", {"eax": 0}, {**SLOT_PTRS, C2_ACTOR: le32(C2_PSET), E3_REC + 0x56: b"\x00\x00",
                                E3_REC2 + 0x56: b"\x01\x00", C2_PSET: b"\x00\x00", C2_PSET + 0x20: b"\x00\x00"},
             {0x189FC: 1}),
        Case("a5", {"eax": 1}, {**SLOT_PTRS, C2_ACTOR: le32(C2_PSET), E3_REC + 0x56: b"\x00\x00",
                                E3_REC2 + 0x56: b"\x01\x00", C2_PSET: b"\x00\x00", C2_PSET + 0x20: b"\x00\x80"},
             {0x189FC: 1}),
    ], allow_calls=(0x33950,), calls=(C2_189FC, E.Call(0x1A570, ("eax",), mode="real")),
       eax_mask=0xFF, mutants=("@mutant", "@side", "@arg")),
    # 0x39EFC (record §P6.2): EAX = side; 0x33A10 allow; 1 only when the side's slot +0x53 == 0x0A,
    # +0x10 == 0x39CC8 and +0x58 == 4. Mask 0xFF (`test al,al` at 0x3B797).
    Spec("fighter_39efc", 0x39EFC, [
        Case("e0", {"eax": 0}, {**SLOT_PTRS, DS_SLOTS + 0x53: b"\x0a", DS_SLOTS + 0x10: le32(0x39CC8),
                                DS_SLOTS + 0x58: b"\x04"}),
        Case("e1", {"eax": 0}, {**SLOT_PTRS, DS_SLOTS + 0x53: b"\x0b", DS_SLOTS + 0x10: le32(0x39CC8),
                                DS_SLOTS + 0x58: b"\x04"}),
        Case("e2", {"eax": 0}, {**SLOT_PTRS, DS_SLOTS + 0x53: b"\x0a", DS_SLOTS + 0x10: le32(0x39CC0),
                                DS_SLOTS + 0x58: b"\x04"}),
        Case("e3", {"eax": 0}, {**SLOT_PTRS, DS_SLOTS + 0x53: b"\x0a", DS_SLOTS + 0x10: le32(0x39CC8),
                                DS_SLOTS + 0x58: b"\x05"}),
        Case("e4", {"eax": 1}, {**SLOT_PTRS, DS_SLOTS + 0x94 + 0x53: b"\x0a", DS_SLOTS + 0x94 + 0x10: le32(0x39CC8),
                                DS_SLOTS + 0x94 + 0x58: b"\x04"}),
    ], allow_calls=(0x33A10,), eax_mask=0xFF, mutants=("@mutant", "@val", "@side")),
    # 0x3B8D8 (record §C1.2): EAX = side, EDX = delta; 1 when slot+0x2C + delta >= wall or <= -wall.
    # Mask 0xFF (`test al,al` at 0x3C2BF/0x3C2EF).
    Spec("fighter_3b8d8", 0x3B8D8, [
        Case("b0", {"eax": 0, "edx": 0x100}, {**SLOT_PTRS, DS_SLOTS + 0x2C: le32(0x1000), DS_BE018: le32(0x2000)}),
        Case("b1", {"eax": 0, "edx": 0x100}, {**SLOT_PTRS, DS_SLOTS + 0x2C: le32(0x1F00), DS_BE018: le32(0x2000)}),
        Case("b2", {"eax": 0, "edx": 0xFFFFE000}, {**SLOT_PTRS, DS_SLOTS + 0x2C: le32(0x1000), DS_BE018: le32(0x2000)}),
        Case("b3", {"eax": 0, "edx": 0xFFFFD000}, {**SLOT_PTRS, DS_SLOTS + 0x2C: le32(0x1000), DS_BE018: le32(0x2000)}),
        Case("b4", {"eax": 1, "edx": 0x100}, {**SLOT_PTRS, DS_SLOTS + 0x94 + 0x2C: le32(0x1F00), DS_BE018: le32(0x2000)}),
        Case("b5", {"eax": 0, "edx": 0}, {**SLOT_PTRS, DS_SLOTS + 0x2C: le32(0), DS_BE018: le32(0x80000000)}),
    ], eax_mask=0xFF, mutants=("@mutant", "@side", "@bound")),
    # 0x3B90C (record §C1.2): EAX = side, EDX = delta; the sum clamped to +/-wall. Mask full
    # (0x188DC stores the result as x).
    Spec("fighter_3b90c", 0x3B90C, [
        Case("c0", {"eax": 0, "edx": 0x100}, {**SLOT_PTRS, DS_SLOTS + 0x2C: le32(0x1000), DS_BE018: le32(0x2000)}),
        Case("c1", {"eax": 0, "edx": 0x100}, {**SLOT_PTRS, DS_SLOTS + 0x2C: le32(0x1F00), DS_BE018: le32(0x2000)}),
        Case("c2", {"eax": 0, "edx": 0xFFFFD000}, {**SLOT_PTRS, DS_SLOTS + 0x2C: le32(0x1000), DS_BE018: le32(0x2000)}),
        Case("c3", {"eax": 0, "edx": 0xFFFFE000}, {**SLOT_PTRS, DS_SLOTS + 0x2C: le32(0x1000), DS_BE018: le32(0x2000)}),
        Case("c4", {"eax": 1, "edx": 0x100}, {**SLOT_PTRS, DS_SLOTS + 0x94 + 0x2C: le32(0x1000), DS_BE018: le32(0x2000)}),
        Case("c5", {"eax": 0, "edx": 0}, {**SLOT_PTRS, DS_SLOTS + 0x2C: le32(0x80000000), DS_BE018: le32(0x80000000)}),
    ], eax_mask=0xFFFFFFFF, mutants=("@mutant", "@side", "@bound")),
    # 0x33A10 (record §E3.6): EAX = the six-dword out buffer, EDX = side; out[0]=1-side, out[1]=side,
    # out[2]=&slot[1-side], out[3]=&slot[side], out[4]=rec_other, out[5]=rec_self. Mask 0.
    Spec("fighter_ctx_swap", 0x33A10, [
        Case("w0", {"eax": E3_OUT, "edx": 0}, {**SLOT_PTRS, E3_OUT: b"\xaa" * 24}),
        Case("w1", {"eax": E3_OUT, "edx": 1}, {**SLOT_PTRS, E3_OUT: b"\xaa" * 24}),
    ], eax_mask=0, mutants=("@mutant", "@slot", "@rec")),
    # 0x1883C (record §C1.2): EAX = side, EDX = a, EBX = b; latch both slots, add (a, b) to the
    # side's +0x2C/+0x30, then rec+0x18/+0x1C from 0x18714/0x18788 (stubbed with a per-case EAX).
    Spec("fighter_1883c", 0x1883C, [
        Case("c0", {"eax": 0, "edx": 0x10, "ebx": 0x20},
             {**SLOT_PTRS, DS_SLOTS + 0x2C: le32(0x100), DS_SLOTS + 0x30: le32(0x200),
              DS_SLOTS + 0x94 + 0x2C: le32(0x300), DS_SLOTS + 0x94 + 0x30: le32(0x400),
              E3_REC + 0x18: le32(0x18181818), E3_REC + 0x1C: le32(0x1C1C1C1C)},
             {0x18714: 0x1111, 0x18788: 0x2222}),
        Case("c1", {"eax": 1, "edx": 0x10, "ebx": 0x20},
             {**SLOT_PTRS, DS_SLOTS + 0x2C: le32(0x100), DS_SLOTS + 0x30: le32(0x200),
              DS_SLOTS + 0x94 + 0x2C: le32(0x300), DS_SLOTS + 0x94 + 0x30: le32(0x400),
              E3_REC2 + 0x18: le32(0x28282828), E3_REC2 + 0x1C: le32(0x2C2C2C2C)},
             {0x18714: 0x3333, 0x18788: 0x4444}),
        Case("c2", {"eax": 0, "edx": 0xFFFFFFF0, "ebx": 0xFFFFFFE0},
             {**SLOT_PTRS, DS_SLOTS + 0x2C: le32(0x100), DS_SLOTS + 0x30: le32(0x200),
              DS_SLOTS + 0x94 + 0x2C: le32(0x300), DS_SLOTS + 0x94 + 0x30: le32(0x400),
              E3_REC + 0x18: le32(0x18181818), E3_REC + 0x1C: le32(0x1C1C1C1C)},
             {0x18714: 0x5555, 0x18788: 0x6666}),
    ], calls=(C2_LATCH, E.Call(0x18714, ("eax",)), E.Call(0x18788, ("eax",))),
       eax_mask=0, mutants=("@mutant", "@side", "@latch", "@arg")),
    # 0x18B04 (record §C1.2): EAX = side; mode 0x104B00 == 0x22 returns; else the +0x29 bit 0x40
    # by the two slots' +0x2C, then rec+0x18 = 0x18714(side) (stubbed). Mask 0.
    Spec("hit_facing_flag", 0x18B04, [
        Case("m0", {"eax": 0}, {DS_104B00: b"\x22\x00", **SLOT_PTRS, DS_SLOTS + 0x2C: le32(0x100),
                               DS_SLOTS + 0x94 + 0x2C: le32(0x200), E3_REC + 0x29: b"\x29",
                               E3_REC + 0x18: le32(0x18181818)}, {0x18714: 0x1111}),
        Case("m1", {"eax": 0}, {DS_104B00: b"\x00\x00", **SLOT_PTRS, DS_SLOTS + 0x2C: le32(0x100),
                               DS_SLOTS + 0x94 + 0x2C: le32(0x200), E3_REC + 0x29: b"\x00",
                               E3_REC + 0x18: le32(0x18181818)}, {0x18714: 0x2222}),
        Case("m2", {"eax": 0}, {DS_104B00: b"\x00\x00", **SLOT_PTRS, DS_SLOTS + 0x2C: le32(0x200),
                               DS_SLOTS + 0x94 + 0x2C: le32(0x200), E3_REC + 0x29: b"\xff",
                               E3_REC + 0x18: le32(0x18181818)}, {0x18714: 0x3333}),
        Case("m3", {"eax": 1}, {DS_104B00: b"\x00\x00", **SLOT_PTRS, DS_SLOTS + 0x2C: le32(0x100),
                               DS_SLOTS + 0x94 + 0x2C: le32(0x200), E3_REC2 + 0x29: b"\x00",
                               E3_REC2 + 0x18: le32(0x28282828)}, {0x18714: 0x4444}),
    ], allow_calls=(0x33950,), calls=(E.Call(0x18714, ("eax",)),), eax_mask=0,
       mutants=("@mutant", "@mode", "@side", "@store")),
    # 0x18B44 (record §P6.2): EAX = slot; once per fight (DS_00100C1D) and not when +0x63 == 1,
    # the two dust spawns (0x2AE14 stub) at the 0xA17D4/0xA17D6-derived x/y, layers 0xFE/0xFF.
    Spec("fighter_18b44", 0x18B44, [
        Case("d0", {"eax": E3_SLOT}, {E3_SLOT + 0x63: b"\x01", DS_100C1D: b"\x00", DS_104529: b"\x02",
                                      FIGHTER_A17D4: le32(0x00110022), FIGHTER_A17D6: le32(0x00330044)}),
        Case("d1", {"eax": E3_SLOT}, {E3_SLOT + 0x63: b"\x00", DS_100C1D: b"\xff", DS_104529: b"\x02",
                                      FIGHTER_A17D4: le32(0x00110022), FIGHTER_A17D6: le32(0x00330044)}),
        Case("d2", {"eax": E3_SLOT}, {E3_SLOT + 0x63: b"\x00", DS_100C1D: b"\x00", DS_104529: b"\x02",
                                      FIGHTER_A17D4: le32(0x00110022), FIGHTER_A17D6: le32(0x00330044)}),
        Case("d3", {"eax": E3_SLOT}, {E3_SLOT + 0x63: b"\x00", DS_100C1D: b"\x00", DS_104529: b"\x01",
                                      FIGHTER_A17D4: le32(0x00110022), FIGHTER_A17D6: le32(0x00330044)}),
    ], calls=(SPAWN,), eax_mask=0, mutants=("@mutant", "@latch", "@layer")),
    # 0x1DDF4 (record §6.7): EAX = side, EDX = table, EBX = idx; |0x187FC| <= DSB(table+char)<<6
    # and |0x1881C| <= DSB(idx+char)<<6 (both stubbed with per-case EAX). Mask 0xFF (`test al,al`).
    Spec("hit_geometry", 0x1DDF4, [
        Case("g0", {"eax": 0, "edx": C2_ROW0, "ebx": C2_ROW1},
             {**SLOT_PTRS, DS_SLOTS + 0x7A: b"\x01", C2_ROW0 + 1: b"\x10", C2_ROW1 + 1: b"\x20"},
             {0x187FC: 0x300, 0x1881C: 0x700}),
        Case("g1", {"eax": 0, "edx": C2_ROW0, "ebx": C2_ROW1},
             {**SLOT_PTRS, DS_SLOTS + 0x7A: b"\x01", C2_ROW0 + 1: b"\x10", C2_ROW1 + 1: b"\x20"},
             {0x187FC: 0x500, 0x1881C: 0x700}),
        Case("g2", {"eax": 0, "edx": C2_ROW0, "ebx": C2_ROW1},
             {**SLOT_PTRS, DS_SLOTS + 0x7A: b"\x01", C2_ROW0 + 1: b"\x10", C2_ROW1 + 1: b"\x20"},
             {0x187FC: 0x300, 0x1881C: 0x900}),
        Case("g3", {"eax": 0, "edx": C2_ROW0, "ebx": C2_ROW1},
             {**SLOT_PTRS, DS_SLOTS + 0x7A: b"\x01", C2_ROW0 + 1: b"\x10", C2_ROW1 + 1: b"\x20"},
             {0x187FC: 0xFFFFFB00, 0x1881C: 0x700}),
        Case("g4", {"eax": 0, "edx": C2_ROW0, "ebx": C2_ROW1},
             {**SLOT_PTRS, DS_SLOTS + 0x7A: b"\x01", C2_ROW0 + 1: b"\x10", C2_ROW1 + 1: b"\x20"},
             {0x187FC: 0x400, 0x1881C: 0x800}),
        Case("g5", {"eax": 1, "edx": C2_ROW0, "ebx": C2_ROW1},
             {**SLOT_PTRS, DS_SLOTS + 0x94 + 0x7A: b"\x03", C2_ROW0 + 3: b"\x40", C2_ROW1 + 3: b"\x10"},
             {0x187FC: 0x1000, 0x1881C: 0x400}),
        Case("g6", {"eax": 0, "edx": C2_ROW0, "ebx": C2_ROW1},
             {**SLOT_PTRS, DS_SLOTS + 0x7A: b"\x01", C2_ROW0 + 1: b"\x10", C2_ROW1 + 1: b"\x20"},
             {0x187FC: 0x300, 0x1881C: 0xFFFFF900}),
    ], calls=(E.Call(0x187FC, ()), E.Call(0x1881C, ())), eax_mask=0xFF,
       mutants=("@mutant", "@side", "@abs", "@table")),
    # 0x39F40 (record §P6.2): EAX = side, EDX/EBX/ECX/s0 = the four pose words; 0x33A10 allow;
    # seeds DS_00107A60/68/70/78[side], the slot's +0x52/53/54/10/58 and the record's +0x24.
    Spec("fighter_pose_start", 0x39F40, [
        Case("p0", {"eax": 0, "edx": 0x11111111, "ebx": 0x22222222, "ecx": 0x33333333, "s0": 0x44444444},
             {**SLOT_PTRS, DS_SLOTS + 0x10: le32(0x10101010), DS_SLOTS + 0x52: b"\x52\x53\x54\x55",
              DS_SLOTS + 0x58: b"\x58", E3_REC + 0x24: le32(0x24242424), E3_REC2 + 0x24: le32(0x34343434),
              0x107A60: bytes(range(0x60, 0x80))}),
        Case("p1", {"eax": 1, "edx": 0x55555555, "ebx": 0x66666666, "ecx": 0x77777777, "s0": 0x88888888},
             {**SLOT_PTRS, DS_SLOTS + 0x94 + 0x10: le32(0x20202020),
              DS_SLOTS + 0x94 + 0x52: b"\x62\x63\x64\x65", DS_SLOTS + 0x94 + 0x58: b"\x68",
              E3_REC + 0x24: le32(0x24242424), E3_REC2 + 0x24: le32(0x34343434),
              0x107A60: bytes(range(0x60, 0x80))}),
    ], allow_calls=(0x33A10,), eax_mask=0, mutants=("@mutant", "@side", "@arg", "@field")),
    # 0x2BCF4 (record §P6.2): EAX = rec, EDX = stream; rec+8 = stream, rec+0x28 &= ~0x14, the pset
    # word = 0x2A408(rec, pset)'s low word (stubbed with a per-case EAX). Mask 0.
    Spec("actors_anim_seek", 0x2BCF4, [
        Case("n0", {"eax": E3_REC, "edx": 0x10A700}, {DS_1014EC: le32(C2_PSET), E3_REC + 8: le32(0x08080808),
                                                      E3_REC + 0x28: b"\xff\xff", E3_REC + 0x56: b"\x01\x00",
                                                      C2_PSET + 0x20: b"\x02\x02", C2_PSET + 0x22: b"\x03\x03"},
             {0x2A408: 0x1234}),
        Case("n1", {"eax": E3_REC, "edx": 0x10A700}, {DS_1014EC: le32(C2_PSET), E3_REC + 8: le32(0x08080808),
                                                      E3_REC + 0x28: b"\x00\x00", E3_REC + 0x56: b"\x00\x00",
                                                      C2_PSET: b"\x02\x02", C2_PSET + 2: b"\x03\x03"},
             {0x2A408: 0x5678}),
    ], calls=(C1_SPRITE_ID,), eax_mask=0, mutants=("@mutant", "@char", "@width")),
    # 0x37D18 (record §P6.2): EAX = slot, EDX = rec; the dispatch state, the 0xC9238[char]
    # animation, 0x39A10(rec, 0x309), the 0xBDAD4[char] voice, the 0x1078DC approach pointer and
    # the other slot's actor +0x59/+0x40. Mask 0.
    Spec("fighter_37d18", 0x37D18, [
        Case("r0", {"eax": DS_SLOTS, "edx": E3_REC}, {**P6_PTRS, **SLOT_PTRS, DS_SLOTS + 0x42: b"\x42",
             DS_SLOTS + 0x7A: b"\x01", E3_REC + 0x51: b"\x00", 0x000C9238 + 4: le32(0x000E1234),
             0x000BDAD4 + 2: b"\x55\x66", DS_SLOTS + 0x94 + 0x40: b"\x40\x41\x42\x43",
             E3_REC2 + 0x59: b"\x59", 0x1078DC: b"\xdc\xdc\xdc\xdc"}),
        Case("r1", {"eax": DS_SLOTS, "edx": E3_REC2}, {DS_SLOTS - 8: le32(DS_SLOTS + 0x94) + le32(0),
             **SLOT_PTRS, DS_SLOTS + 0x94 + 0x42: b"\x52", DS_SLOTS + 0x94 + 0x7A: b"\x03",
             E3_REC2 + 0x51: b"\x01", 0x000C9238 + 12: le32(0x000E5678), 0x000BDAD4 + 6: b"\x77\x88",
             DS_SLOTS + 0x40: b"\x60\x61\x62\x63", E3_REC + 0x59: b"\x69", 0x1078DC: b"\xdc\xdc\xdc\xdc"}),
    ], calls=(ANIM_BEGIN, E.Call(0x39A10, ("eax", "edx"), clobbers=("edx",)), VOICE), eax_mask=0,
       mutants=("@mutant", "@state", "@order")),
    # 0x3AA54 (record §P6.2): EAX = slot; the reaction-0x11 pose seed from the 0xBEC88/0xBECA4/
    # 0xBECC0/0xBECDC[char] tables, slot+0x52 = 0x11, then 0x1A5AC(rec+0x51) (stubbed) negates
    # rec+0x34 when 0. Mask 0 (the callers ignore the returned 0x11).
    Spec("fighter_3aa54", 0x3AA54, [
        Case("a0", {"eax": DS_SLOTS}, {**SLOT_PTRS, DS_SLOTS + 0x7A: b"\x01", E3_REC + 0x24: le32(0x24242424),
             E3_REC + 0x28: b"\x28\x28", E3_REC + 0x34: b"\x34\x34", E3_REC + 0x51: b"\x00",
             E3_REC + 0x43: b"\x43", DS_SLOTS + 0x10: le32(0x10101010), DS_SLOTS + 0x41: b"\x41",
             DS_SLOTS + 0x52: b"\x52\x53\x54\x55", DS_SLOTS + 0x58: b"\x58", 0xBEC88 + 4: b"\x88\x88",
             0xBECA4 + 4: b"\xa4\xa4", 0xBECC0 + 4: b"\xc0\xc0", 0xBECDC + 4: b"\xdc"}, {0x1A5AC: 0}),
        Case("a1", {"eax": DS_SLOTS}, {**SLOT_PTRS, DS_SLOTS + 0x7A: b"\x01", E3_REC + 0x24: le32(0x24242424),
             E3_REC + 0x28: b"\x28\x28", E3_REC + 0x34: b"\x34\x34", E3_REC + 0x51: b"\x00",
             E3_REC + 0x43: b"\x43", DS_SLOTS + 0x10: le32(0x10101010), DS_SLOTS + 0x41: b"\x41",
             DS_SLOTS + 0x52: b"\x52\x53\x54\x55", DS_SLOTS + 0x58: b"\x58", 0xBEC88 + 4: b"\x88\x88",
             0xBECA4 + 4: b"\xa4\xa4", 0xBECC0 + 4: b"\xc0\xc0", 0xBECDC + 4: b"\xdc"}, {0x1A5AC: 1}),
        Case("a2", {"eax": DS_SLOTS}, {**SLOT_PTRS, DS_SLOTS + 0x7A: b"\x02", E3_REC + 0x24: le32(0x24242424),
             E3_REC + 0x28: b"\x28\x28", E3_REC + 0x34: b"\x34\x34", E3_REC + 0x51: b"\x01",
             E3_REC + 0x43: b"\x43", DS_SLOTS + 0x10: le32(0x10101010), DS_SLOTS + 0x41: b"\x41",
             DS_SLOTS + 0x52: b"\x52\x53\x54\x55", DS_SLOTS + 0x58: b"\x58", 0xBEC88 + 8: b"\x99\x99",
             0xBECA4 + 8: b"\xb5\xb5", 0xBECC0 + 8: b"\xd1\xd1", 0xBECDC + 8: b"\xdd"}, {0x1A5AC: 0}),
    ], calls=(E.Call(0x1A5AC, ("eax",)),), eax_mask=0, mutants=("@mutant", "@char", "@field")),
    # 0x2AD40 (record §P4.5): EAX = rec, EDX = pset; the child's dead bit + 0x2B150(child), the
    # parent refcount, 0x249D0/0x249B0, the pset reset and 0x2B150(rec). 0x2EA30 is the interrupt
    # lock the port drops as inert (record §47-C): allowed, and 0xBCD60 = 0 makes it a no-op.
    Spec("release_record", 0x2AD40, [
        Case("r0", {"eax": C2_REC, "edx": C2_PSET}, {DS_1014F4: le32(C2_POOL), C2_REC: le32(0x105B3C) + le32(0x105B3C), C2_REC + 0x2A: b"\x00\x00",
             C2_REC + 0x4F: b"\x00", C2_REC + 0x4B: b"\x01", C2_REC + 0x4A: b"\x02",
             C2_REC + 0x28: b"\x28\x04", C2_REC + 0x68 + 0x2A: b"\xff", C2_PSET + 4: le32(0x04040404),
             C2_PSET + 8: le32(0x08080808), C2_PSET + 0x0C: b"\x0c\x0c\x0e\x0e",
             0x105B3C: le32(0x105B3C), 0xBCD60: b"\x00"}),
        Case("r1", {"eax": C2_REC, "edx": C2_PSET}, {DS_1014F4: le32(C2_POOL), C2_REC + 0x2A: b"\x00\x00",
             C2_REC + 0x4F: b"\x00", C2_REC + 0x4B: b"\x00", C2_REC + 0x4A: b"\x02",
             C2_REC + 0x28: b"\x28\x04", C2_PSET + 4: le32(0x04040404), C2_PSET + 8: le32(0x08080808),
             C2_PSET + 0x0C: b"\x0c\x0c\x0e\x0e", 0x105B3C: le32(0x105B3C), 0xBCD60: b"\x00"}),
        Case("r2", {"eax": C2_REC, "edx": C2_PSET}, {DS_1014F4: le32(C2_POOL), C2_REC + 0x2A: b"\x08\x00",
             C2_REC + 0x4F: b"\x00", C2_REC + 0x4B: b"\x01", C2_REC + 0x4A: b"\x02",
             C2_REC + 0x28: b"\x28\x04", C2_REC + 0x68 + 0x2A: b"\xff", C2_PSET + 4: le32(0x04040404),
             C2_PSET + 8: le32(0x08080808), C2_PSET + 0x0C: b"\x0c\x0c\x0e\x0e",
             0x105B3C: le32(0x105B3C), 0xBCD60: b"\x00"}),
        Case("r3", {"eax": C2_REC, "edx": C2_PSET}, {DS_1014F4: le32(C2_POOL), C2_REC + 0x2A: b"\x00\x00",
             C2_REC + 0x4F: b"\x01", C2_REC + 0x4B: b"\x01", C2_REC + 0x4A: b"\x02",
             C2_REC + 0x28: b"\x28\x04", C2_REC + 0x68 + 0x2A: b"\xff", C2_PSET + 4: le32(0x04040404),
             C2_PSET + 8: le32(0x08080808), C2_PSET + 0x0C: b"\x0c\x0c\x0e\x0e",
             0x105B3C: le32(0x105B3C), 0xBCD60: b"\x00"}),
    ], allow_calls=(0x2EA30,), calls=(E.Call(0x2B150, ("eax",)), E.Call(0x249D0, ("eax",)),
                                     E.Call(0x249B0, ("eax", "edx"))),
       eax_mask=0, mutants=("@mutant", "@field", "@latch")),
    # 0x2A408 (record §P6.2): EAX = rec, EDX = pset; the sprite-id reader: +0x28 bit 8 keeps
    # DSW(rec+8), else the stream word's bit 15/op 0xD00 arms, then the bit-0x8000 flip by +0x28
    # bit 6. 0x29F34 (stub, per-case EAX) supplies the variable. Mask 0xFFFF (the callers store AX).
    Spec("anim_next_sprite_id", 0x2A408, [
        Case("s0", {"eax": E3_REC, "edx": C2_PSET}, {E3_REC + 8: le32(0x00109234), E3_REC + 0x28: b"\x00\x08",
                                                     C2_PSET: b"\x34\x92"}, {0x29F34: 0}),
        Case("s1", {"eax": E3_REC, "edx": C2_PSET}, {E3_REC + 8: le32(C2_STR), E3_REC + 0x28: b"\x00\x40",
                                                     C2_STR: b"\x34\x12", C2_PSET: b"\x34\x92"}, {0x29F34: 0}),
        Case("s2", {"eax": E3_REC, "edx": C2_PSET}, {E3_REC + 8: le32(C2_STR), E3_REC + 0x28: b"\x00\x00",
                                                     C2_STR: b"\x05\x80", C2_PSET: b"\x34\x92"}, {0x29F34: 0}),
        Case("s3", {"eax": E3_REC, "edx": C2_PSET}, {E3_REC + 8: le32(C2_STR), E3_REC + 0x28: b"\x00\x00",
                                                     C2_STR: b"\x45\xcd", C2_STR + 2: b"\x00\x01",
                                                     C2_PSET: b"\x34\x92"}, {0x29F34: 0x10}),
        Case("s4", {"eax": E3_REC, "edx": C2_PSET}, {E3_REC + 8: le32(C2_STR), E3_REC + 0x28: b"\x00\x40",
                                                     C2_STR: b"\x25\xad", C2_STR + 2: le32(C2_ROW0),
                                                     C2_ROW0 + 6: b"\x22\x02", C2_PSET: b"\x34\x92"}, {0x29F34: 3}),
        Case("s5", {"eax": E3_REC, "edx": C2_PSET}, {E3_REC + 8: le32(0x00109234), E3_REC + 0x28: b"\x40\x08",
                                                     C2_PSET: b"\x34\x92"}, {0x29F34: 0}),
    ], calls=(E.Call(0x29F34, ("eax", "edx"), clobbers=("edx",)),), eax_mask=0xFFFF,
       mutants=("@mutant", "@bit8", "@op", "@var", "@clear")),
    # 0x36638 (record §P6.2): EAX = slot, EDX = rec; the +0x54 dispatch: 1/default restart on
    # 0xC9210/0xC91E8[char] with +0x52 = 0x12, 4 on 0xC91E8 with +0x52 = 8/+0x57 = 2, 5 runs
    # 0x38154(rec+0x51); +0x54 == 3 clears +0x43 bit 0x40 and returns. Mask 0xFF.
    Spec("fighter_state_36638", 0x36638, [
        Case("s0", {"eax": DS_SLOTS, "edx": E3_REC}, {**SLOT_PTRS, DS_SLOTS + 0x43: b"\x40",
             DS_SLOTS + 0x54: b"\x03", DS_SLOTS + 0x7A: b"\x01", DS_SLOTS + 0x52: b"\x52",
             DS_SLOTS + 0x53: b"\x53", DS_SLOTS + 0x57: b"\x57", DS_104B00: b"\x25\x00"}),
        Case("s1", {"eax": DS_SLOTS, "edx": E3_REC}, {**SLOT_PTRS, DS_SLOTS + 0x43: b"\x00",
             DS_SLOTS + 0x54: b"\x01", DS_SLOTS + 0x7A: b"\x01", DS_SLOTS + 0x52: b"\x52",
             DS_SLOTS + 0x53: b"\x53", DS_SLOTS + 0x57: b"\x57", DS_104B00: b"\x00\x00"}),
        Case("s2", {"eax": DS_SLOTS, "edx": E3_REC}, {**SLOT_PTRS, DS_SLOTS + 0x40: b"\x40\x41\x42\x43",
             DS_SLOTS + 0x43: b"\x40", DS_SLOTS + 0x54: b"\x01", DS_SLOTS + 0x7A: b"\x01",
             DS_SLOTS + 0x52: b"\x52", DS_SLOTS + 0x53: b"\x53", DS_SLOTS + 0x57: b"\x57",
             0x000C9210 + 4: le32(0x000E1111), DS_104B00: b"\x00\x00"}),
        Case("s3", {"eax": DS_SLOTS, "edx": E3_REC}, {**SLOT_PTRS, DS_SLOTS + 0x40: b"\x40\x41\x42\x43",
             DS_SLOTS + 0x43: b"\x40", DS_SLOTS + 0x54: b"\x04", DS_SLOTS + 0x7A: b"\x01",
             DS_SLOTS + 0x52: b"\x52", DS_SLOTS + 0x53: b"\x53", DS_SLOTS + 0x57: b"\x57",
             0x000C91E8 + 4: le32(0x000E2222), DS_104B00: b"\x00\x00"}),
        Case("s4", {"eax": DS_SLOTS, "edx": E3_REC}, {**SLOT_PTRS, DS_SLOTS + 0x40: b"\x40\x41\x42\x43",
             DS_SLOTS + 0x43: b"\x40", DS_SLOTS + 0x54: b"\x05", DS_SLOTS + 0x7A: b"\x01",
             DS_SLOTS + 0x52: b"\x52", DS_SLOTS + 0x53: b"\x53", DS_SLOTS + 0x57: b"\x57",
             E3_REC + 0x51: b"\x01", DS_104B00: b"\x00\x00"}),
        Case("s5", {"eax": DS_SLOTS, "edx": E3_REC}, {**SLOT_PTRS, DS_SLOTS + 0x40: b"\x40\x41\x42\x43",
             DS_SLOTS + 0x43: b"\x40", DS_SLOTS + 0x54: b"\x00", DS_SLOTS + 0x7A: b"\x01",
             DS_SLOTS + 0x52: b"\x52", DS_SLOTS + 0x53: b"\x53", DS_SLOTS + 0x57: b"\x57",
             0x000C91E8 + 4: le32(0x000E3333), DS_104B00: b"\x00\x00"}),
        Case("s6", {"eax": DS_SLOTS, "edx": E3_REC}, {**SLOT_PTRS, DS_SLOTS + 0x40: b"\x40\x41\x42\x43",
             DS_SLOTS + 0x43: b"\x40", DS_SLOTS + 0x54: b"\x01", DS_SLOTS + 0x7A: b"\x01",
             DS_SLOTS + 0x52: b"\x52", DS_SLOTS + 0x53: b"\x53", DS_SLOTS + 0x57: b"\x57",
             0x000C9210 + 4: le32(0x000E4444), DS_104B00: b"\x25\x00"}),
    ], calls=(ANIM_BEGIN, E.Call(0x38154, ("eax",))), eax_mask=0xFF,
       mutants=("@mutant", "@anim", "@mode", "@side")),
]

# ---- track P batch 8: the remaining entries and the triage (record 2026-10-04-reverse-p8) ---------

# 0x29CFC (record §P8.3): a one-instruction tail alias, `jmp 0x13DF0` (effects_clear). The callee
# is stubbed on both sides through its seam; the port's effects_29cfc records the arrival. Mask 0
# (the caller, 0x424E8's case 2, overwrites EAX at once).
P8_CLEAR = E.Call(0x13DF0)
P8_SPECS = [
    Spec("effects_29cfc", 0x29CFC, [
        Case("c0", {}),
    ], calls=(P8_CLEAR,), eax_mask=0, mutants=("@mutant",)),
    # 0x3A820 (record §P8.2): the 0x3A8E8 pose family's per-frame handler. EAX = slot (dead),
    # EBX = side (`mov edx,ebx` at 0x3A823); the ctx swap (0x33A10, allow) builds ctx[3] = the
    # own slot and ctx[5] = its record. Phase 0 arms +0x58; phase 1 starts the 0xC9058[char]
    # stream at 3.0 (0x40400000), re-anchors the self record and, when B[side] (0x107CFC+side*2)
    # is neither 0 nor 5 and (u8)(+0x90 - 1) > 3, snaps x to A[side] (0x107CF8+side*2); +0x90 =
    # 4 at the end. EDX is a scratch seed the raw never reads (@side catches a port that does).
    Spec("fighter_pose_3a820", 0x3A820, [
        Case("p0", {"eax": E3_SLOT, "edx": 1, "ebx": 0},
             {**SLOT_PTRS, E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
              DS_SLOTS + 0x58: b"\x00", DS_SLOTS + 0x59: b"\x59", DS_SLOTS + 0x7A: b"\x7a",
              DS_SLOTS + 0x7B: b"\x7b", DS_SLOTS + 0x90: b"\x90", DS_SLOTS + 0x91: b"\x91",
              DS_SLOTS + 0x94 + 0x58: b"\x58", DS_SLOTS + 0x94 + 0x7A: b"\x6a",
              DS_SLOTS + 0x94 + 0x90: b"\x70", 0x00107CF8: le32(0x43214321),
              0x00107CFC: le32(0x00030003)}),
        Case("p2", {"eax": E3_SLOT, "edx": 0, "ebx": 1},
             {**SLOT_PTRS, E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
              E3_REC + 0x18: le32(0x18181818), E3_REC + 0x1C: le32(0x1C1C1C1C),
              DS_SLOTS + 0x58: b"\x58", DS_SLOTS + 0x7A: b"\x7a", DS_SLOTS + 0x90: b"\x90",
              DS_SLOTS + 0x94 + 0x58: b"\x02", DS_SLOTS + 0x94 + 0x59: b"\x59",
              DS_SLOTS + 0x94 + 0x7A: b"\x6a", DS_SLOTS + 0x94 + 0x90: b"\x70",
              0x00107CF8: le32(0x43214321), 0x00107CFC: le32(0x00030003)}),
        Case("s0", {"eax": E3_SLOT, "edx": 1, "ebx": 0},
             {**SLOT_PTRS, E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
              E3_REC + 0x18: le32(0x18181818), E3_REC + 0x1C: le32(0x1C1C1C1C),
              DS_SLOTS + 0x58: b"\x01", DS_SLOTS + 0x7A: b"\x00", DS_SLOTS + 0x90: b"\x00",
              DS_SLOTS + 0x94 + 0x58: b"\x58", DS_SLOTS + 0x94 + 0x7A: b"\x6a",
              DS_SLOTS + 0x94 + 0x90: b"\x70", 0x00107CF8: le32(0x00004321),
              0x00107CFC: le32(0x00000003)}),
        Case("s1", {"eax": E3_SLOT, "edx": 0, "ebx": 1},
             {**SLOT_PTRS, E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
              E3_REC2 + 0x18: le32(0x28282828), E3_REC2 + 0x1C: le32(0x2C2C2C2C),
              DS_SLOTS + 0x58: b"\x58", DS_SLOTS + 0x7A: b"\x7a", DS_SLOTS + 0x90: b"\x90",
              DS_SLOTS + 0x94 + 0x58: b"\x01", DS_SLOTS + 0x94 + 0x7A: b"\x01",
              DS_SLOTS + 0x94 + 0x90: b"\x04", DS_SLOTS + 0x94 + 0x91: b"\x91",
              0x00107CFA: le32(0x00004322), 0x00107CFE: le32(0x00000003)}),
        Case("q0", {"eax": E3_SLOT, "edx": 1, "ebx": 0},
             {**SLOT_PTRS, E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
              E3_REC + 0x18: le32(0x18181818), E3_REC + 0x1C: le32(0x1C1C1C1C),
              DS_SLOTS + 0x58: b"\x01", DS_SLOTS + 0x7A: b"\x00", DS_SLOTS + 0x90: b"\x00",
              0x00107CF8: le32(0x00004321), 0x00107CFC: le32(0x00000000)}),
        Case("q5", {"eax": E3_SLOT, "edx": 1, "ebx": 0},
             {**SLOT_PTRS, E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
              E3_REC + 0x18: le32(0x18181818), E3_REC + 0x1C: le32(0x1C1C1C1C),
              DS_SLOTS + 0x58: b"\x01", DS_SLOTS + 0x7A: b"\x00", DS_SLOTS + 0x90: b"\x00",
              0x00107CF8: le32(0x00004321), 0x00107CFC: le32(0x00000005)}),
    ], allow_calls=(0x33A10,), calls=(ANIM_BEGIN, ANCHOR, ANCHORX), eax_mask=0,
       mutants=("@side", "@stream", "@globs", "@end", "@order")),
]

# ---- track P batch C2b: the nine deferred callee rows (record 2026-10-05-reverse-c2b) ------------
#
# The resource fixture every 0x1B544 caller shares: a preloaded INDEX entry whose payload is in the
# image, so the original's 0x1B544 fast path (0x1B569 `test [ecx+0xc],0x1000000`) returns
# payload + (handle & 0x7FFFFF) exactly as the port's res_resolve does, with no EMS call. The entry
# count DS_001014F0 covers the index, so the port's in-range guard passes too.
C2B_RES_PTR, C2B_RES_N = 0x001014E0, 0x001014F0   # DS_001014E0 (table ptr) / DS_001014F0 (count)
C2B_RES_TABLE = 0x10A600          # 0x14-byte entries in the image's zero BSS
C2B_RES_DATA = 0x10A700           # the resolved payload (its first dword is the colour count)

def c2b_res(handle, count):
    """The pokes that make `handle` resolve to a preloaded entry whose payload's [0] = count.

    0x1B544 (and res_resolve) add the handle's low 23 bits to the entry's payload base, so the entry
    stores the base and the per-handle payload sits at base + (handle & 0x7FFFFF)."""
    idx = (handle >> 23) & 0xFF
    entry = C2B_RES_TABLE + idx * 0x14
    data = C2B_RES_DATA + (handle & 0x7FFFFF)
    return {C2B_RES_PTR: le32(C2B_RES_TABLE), C2B_RES_N: le32(0x100),
            entry + 0x0C: le32(0x01000000), entry + 0x10: le32(C2B_RES_DATA), data: le32(count)}

# 0x33754 (record §C2b): EAX = palette handle. Search the 24-entry palette table at 0x107618 for the
# handle (refcount++ on a hit), else take the first free entry, seed it {handle; 1; start; count} and
# append its DAC record at DS_00107798, then reflow the occupied entries after it. EDX is scratch
# (the entry search re-reads ECX). Mask full (0x2A17C stores the entry offset in the pset's +0x18).
# 0x1B544 is allowed against the preloaded entry (E.Call would need a port seam it does not have);
# its fast path and res_resolve agree byte for byte. The full-table fatal 0x3384E is unhit and named.
C2B_PAL_TABLE, C2B_PAL_END = 0x107618, 0x107798
C2B_DIRTY = 0x107500              # a valid dirty-list head (0x107498..0x107618)
C2B_DIRTY_HI = 0x1075F0           # p0's head: its 0x337DA bump (head+0x10) carries into byte 1
C2B_PAL_TAIL = 0x00107798         # DS_00107798: the dirty-list head variable

def c2b_pal_entry(i, handle, start, count):
    """One 16-byte poke per entry: handle, 1, start, count (the case's poke limit is 16)."""
    e = C2B_PAL_TABLE + i * 0x10
    return {e: le32(handle) + le32(1) + le32(start) + le32(count)}

def c2b_pal_run(n, base_handle, start0, length):
    """Pokes for the contiguous occupied entries 0..n-1, as 64-byte pokes (the case limit)."""
    blob = b"".join(le32(base_handle + i) + le32(1) + le32(start0 + i * length) + le32(length)
                    for i in range(n))
    return {C2B_PAL_TABLE + off: blob[off:off + 64] for off in range(0, len(blob), 64)}

def c2b_3b298_pokes(side, edx_arg, char, mask, word, count, cmd, ead, bits, o43):
    """The 0x3B298 fixture: the slot pair, the command word, the +0x100CDE dword, the character byte
    and the anim[2]+2 word for the (char, arg) triple the allowed 0x3AFC4 computes. mask/word are
    the case's 0x1AB5C/0x46460 stub EAX values, named here so the call site reads as one fixture."""
    del mask, word
    ctx0 = 1 - side
    anim2 = 0x000A6728 + ((char << 6) + edx_arg) * 6
    return {**SLOT_PTRS,
            DS_SLOTS + side * 0x94 + 0x43: bytes([o43]),
            DS_SLOTS + side * 0x94 + 0x86: b"\x86\x86",
            DS_SLOTS + (1 - side) * 0x94 + 0x84: b"\x84\x84",
            0x001088E0 + side * 2: cmd.to_bytes(2, "little"),
            0x00100CDE + ctx0 * 2: (ead << 16).to_bytes(4, "little"),
            0x0010782A + ctx0 * 0x94: bytes([char]),
            0x000BEEF2: (count << 16).to_bytes(4, "little"),
            anim2 + 2: bits.to_bytes(2, "little")}

C2B_POOL = 0x10A900   # a 0x68-stride actor pool in the image's zero BSS (0x2BD44's row)

def c2b_3b714(side, char, r24, bits=0, a02=0x22, a03=0x33, efc=0, p1_52=0, p1_54=0, p2_52=0,              rec4_34=0, rec5_34=0, rec5_4b=0, row60=0, p2_8a=0):
    """The 0x3B714 fixture: the winner slot (param_2 = the side's slot), the other slot (param_1),
    the reaction byte r24, the anim[2]+2 word and anim[0]'s +2/+3 bytes, the 0x39EFC state on the
    inverted side, and the 2bd44 row for the other record's +0x4B index."""
    p2 = DS_SLOTS + side * 0x94
    p1 = DS_SLOTS + (1 - side) * 0x94
    efc_slot = DS_SLOTS + (1 - side) * 0x94
    rec4 = E3_REC if side == 0 else E3_REC2
    rec5 = E3_REC2 if side == 0 else E3_REC
    c = (char << 6) + r24
    a0 = 0x000DE114 + c * 11
    a2 = 0x000A6728 + c * 6
    # one 64-byte block per record (its +0x34 word, +0x4B index and +0x51 side byte) and per slot
    # (its +0x52/+0x5F bytes, or the +0x41..+0x65 run), so a case stays inside the 16-poke limit.
    r4 = bytearray(0x40); r4[0:4] = le32(rec4_34); r4[0x51 - 0x34] = side
    r5 = bytearray(0x40); r5[0:4] = le32(rec5_34); r5[0x51 - 0x34] = 1 - side
    if rec5_4b:
        r5[0x4B - 0x34] = rec5_4b
    s2 = bytearray(64); s2[0x52 - 0x40] = p2_52; s2[0x5F - 0x40] = r24
    s1 = bytearray(64)
    s1[0x41 - 0x41] = 0x41; s1[0x52 - 0x41] = p1_52; s1[0x54 - 0x41] = p1_54
    # +0x4E/+0x4F: the 0x3B7D0 `mov word [esi+0x4e], ax` (AX = 0) observes both bytes only if the
    # high byte is seeded nonzero.
    s1[0x4E - 0x41] = 0x4E; s1[0x4F - 0x41] = 0xAA; s1[0x65 - 0x41] = 0x65
    p = {**SLOT_PTRS, rec4 + 0x34: bytes(r4), rec5 + 0x34: bytes(r5),
         p2 + 0x40: bytes(s2), p1 + 0x41: bytes(s1),
         0x0010782A + side * 0x94: bytes([char]),
         a0 + 2: bytes([a02, a03]), a2 + 2: bits.to_bytes(2, "little")}
    if p2_8a:
        p[p2 + 0x8A] = bytes([p2_8a])
    if rec5_4b:
        p[0x001014F4] = le32(C2B_POOL)
        p[C2B_POOL + rec5_4b * 0x68 + 0x60] = bytes([row60])
    if efc:
        p[efc_slot + 0x53] = b"\x0a"
        p[efc_slot + 0x10] = le32(0x39CC8)
        p[efc_slot + 0x58] = b"\x04"
    return p

def c2b_39834(side, b, char, k=0, a0=0x10, a1=0x40, a8=0x08, tbl=100, r=0, ai=0, s5d=0, f2=0,
              s52=0, s54=1, p_104abc=0, p_104b14=0, m2c=0x0101, m28=0x28282828, s43=0x43):
    """The 0x39834 fixture: the side's slot and the inverted side's char byte select the anim triple
    (char<<6 | b); the 0x107D2A+ctx0*2 dword's high word is k (and its low word the +0x107D2C/D2A
    counter); a0/a0+1/a0+8 are the anim[0] bytes; tbl the per-level table entry for k <= 0xB;
    r the 0x39738 stub value; ai the 0x468D8 inputs (slot+0x10 == 0x22BEC, rec+0x24 clear, +0x54
    != 2, or +0x52 == 7); s5d/s52/s54/f2/p_104abc/p_104b14 the tail gates; s43 the +0x43 seed (0x47
    in f1 so the 0x39917 `and cl,0xFB` store is observable). Blocks: the record's
    +0x24..+0x63, the slot's +0x41..+0x80 and the 0x107D20..+0x5F run (the counters and 0x107D28
    plus the per-side k dword), so a case stays inside the 16-poke limit."""
    ctx0 = 1 - side
    slot = DS_SLOTS + side * 0x94
    rec = E3_REC if side == 0 else E3_REC2
    c = (char << 6) + b
    A0 = 0x000DE114 + c * 11
    kd = 0x00107D2A + ctx0 * 2
    rb = bytearray(0x40); rb[0:4] = le32(0); rb[0x51 - 0x24] = side
    if ai == 0:
        rb[0:4] = le32(0x80000000)
    sb = bytearray(64)
    sb[0x43 - 0x41] = s43; sb[0x52 - 0x41] = s52; sb[0x54 - 0x41] = s54; sb[0x5D - 0x41] = s5d
    sb[0x53 - 0x41] = 0x53; sb[0x5E - 0x41] = 0x5E
    tb = bytearray(64)
    tb[0x00107D28 - 0x00107D20:0x00107D28 - 0x00107D20 + 4] = le32(m28)
    tb[kd - 0x00107D20:kd - 0x00107D20 + 4] = (m2c | (k << 16)).to_bytes(4, "little")
    ctr = 0x00107D20 + ctx0 * 2
    tb[ctr - 0x00107D20:ctr - 0x00107D20 + 2] = m2c.to_bytes(2, "little")
    ab = bytearray(11); ab[0] = a0; ab[1] = a1; ab[8] = a8
    p = {**SLOT_PTRS, rec + 0x24: bytes(rb), slot + 0x41: bytes(sb), 0x00107D20: bytes(tb),
         slot + 0x10: le32(0x22BEC if ai else 0x11111111),
         0x0010782A + ctx0 * 0x94: bytes([char]), A0: bytes(ab),
         0x001078F2 + side: bytes([f2]),
         0x00104ABC: le32(p_104abc), 0x00104B14: bytes([p_104b14])}
    if k <= 0xB:
        p[0x000BEBF8 + k * 4] = le32(tbl)
    return p

def c2b_36870(side, mode=0, s54=0, s41=0x41, s42=0x42, s43=0x43, s40=0x40404040, s5d=0x5d,
              r365=0, r366=0, char=0, s28=0x2828):
    """The 0x36870 fixture: rec+0x51 = side, the slot pair, the slot's +0x40 dword and +0x41..+0x7F
    run (one block), its +0x84..+0x90 run, the record's +0x1C..+0x5B run and +0x7A char, the mode
    word and the three dword globals the resets clear. r365/r366 are the 0x365C8/0x36638 stub
    values; char selects the per-character anim pointers DSD(0xC8950/0xC89A0/0xC89F0 + char*4).
    The seeds cover the resets the sweep found writing their own pre-state: the slot's +0x0C..+0x1F
    run (its +0x0C/+0x10/+0x18/+0x1C dword zeros), both slots' +0x66 (the other slot's zero), the
    record's +0x1C dword (its zero) and the record's +0x34/+0x36/+0x44 word zeros (their high bytes
    +0x35/+0x37/+0x45 seeded)."""
    s = DS_SLOTS + side * 0x94
    so = DS_SLOTS + (1 - side) * 0x94
    rec = E3_REC if side == 0 else E3_REC2
    b1 = bytearray(64)
    b1[0:4] = le32(s40); b1[0x41 - 0x40] = s41; b1[0x42 - 0x40] = s42; b1[0x43 - 0x40] = s43
    for off in (0x52, 0x53, 0x54, 0x55, 0x5F, 0x62, 0x65, 0x67, 0x68):
        b1[off - 0x40] = off
    b1[0x54 - 0x40] = s54
    b1[0x5D - 0x40] = s5d
    b1[0x74 - 0x40:0x74 - 0x40 + 2] = (0x7474).to_bytes(2, "little")
    b1[0x7A - 0x40] = char
    b2 = bytearray(17)
    # 0x84FF so the +0x84 word's increment (0x3694D `inc ebx` / 0x3694E store, and the side-1
    # sibling) carries into the high byte; both bytes then differ from the pre-state.
    b2[0x84 - 0x80:0x84 - 0x80 + 2] = (0x84FF).to_bytes(2, "little")
    b2[0x8A - 0x80] = 0x8A; b2[0x90 - 0x80] = 0x90
    rb = bytearray(0x40)
    rb[0:4] = le32(0x1C1C1C1C); rb[0x28 - 0x1C:0x2A - 0x1C] = s28.to_bytes(2, "little")
    for off in (0x34, 0x35, 0x36, 0x37, 0x42, 0x43, 0x44, 0x45, 0x4C, 0x4D):
        rb[off - 0x1C] = off
    rb[0x51 - 0x1C] = side
    p = {**SLOT_PTRS, rec + 0x51: bytes([side]), rec + 0x1C: bytes(rb), s + 0x40: bytes(b1),
         s + 0x80: bytes(b2), rec + 0x7A: bytes([char]),
         s + 0x0C: (le32(0x0C0C0C0C) + le32(0x10101010) + le32(0x14141414) + le32(0x18181818)
                    + le32(0x1C1C1C1C)),
         s + 0x66: b"\x66", so + 0x66: b"\x66",
         0x00104B00: mode.to_bytes(2, "little"),
         0x00100CE0 + (1 - side) * 2: (0xCE01).to_bytes(2, "little"),
         0x00100AF8 + side * 4: le32(0xAF8AF8F8),
         0x000FD148 + side * 4: le32(0xD148D148)}
    return p

C2B_2AE14_POOL = 0x10A900        # the actor pool base DS_001014F4 points at
C2B_2AE14_REC = 0x10A968         # pool + 1*0x68: the record actor_alloc returns
C2B_2AE14_PSET = 0x10AD00        # the pset base DS_001014EC points at
C2B_2AE14_DESC = 0x10AE00        # the descriptor the spawn reads
C2B_2AE14_STR = 0x10AF00         # the stream the animation walk follows
C2B_2AE14_NODE = 0x10AF80        # the render list's free node

def c2b_2ae14(a2=0, a3=0, a4=0, a5=0, frame=0, hdl=0, type21=1, dp8=0, dp0=C2B_2AE14_STR,
              word1=0, word2=0, cb=0x5D812, free=1, word_a=0, word_b=0):
    """The 0x2AE14 fixture: the descriptor (stream dword, type, frame, the word fields and the
    palette handle), the actor pool and pset bases, the returned record (pool+0x68), the render
    free node and list head, the 0xBB9DC type-table entry (type 1 -> 0xBB9E8) and the stream's first
    two words. free=0 empties the render free list. The sentinels cover the sweep's hidden stores:
    pset+0x14 differs from pset+0x08 (the g6 copy source) and the node's +4 differs from the pset
    the insert writes there."""
    rec = C2B_2AE14_REC
    pset = C2B_2AE14_PSET + 0x20
    dp = (le32(dp0) + bytes([type21, frame]) + word_a.to_bytes(2, "little")
          + dp8.to_bytes(2, "little") + word_b.to_bytes(2, "little")
          + (0x0C0C).to_bytes(2, "little") + le32(hdl))
    psb = bytearray(32)
    for i in range(32):
        psb[i] = 0xA5
    psb[0x0E] = 0x2E; psb[0x0F] = 0x2E
    psb[0x14:0x18] = b"\x5a" * 4
    p = {0x001014EC: le32(C2B_2AE14_PSET), 0x001014F4: le32(C2B_2AE14_POOL),
         C2B_2AE14_DESC: dp + b"\xa5" * (0x14 - len(dp)),
         0x000BB9E8: le32(cb), C2B_2AE14_STR: word1.to_bytes(2, "little") + word2.to_bytes(2, "little"),
         pset: bytes(psb), 0x00105B44: le32(0), 0x0010275C: le32(C2B_2AE14_NODE) if free else le32(0),
         0x000F0A78: le32(0x000F0A78),
         C2B_2AE14_NODE: le32(0) + le32(0x5A5A5A5A)}
    # seed the record with sentinels: the spawn overwrites most fields. The +0x38 word is distinct
    # (DSW(rec+0x38) = 0 must change it); the run continues 0xA5 to +0x3F.
    p[rec + 0x08] = b"\xa5" * 48 + b"\x38\x38" + b"\xa5" * 6
    p[rec + 0x40] = b"\xa5" * 40
    return p

C2B_OP_REC = 0x10A980            # the 0x2B2A0 row's record
C2B_OP_STR = 0x10AA00            # its command word and the words that follow
C2B_OP_PSET = 0x10AA80           # DS_001014EC: an 0x20-stride pset base (index 1 -> +0x20)
C2B_OP_DESC = 0x10AB00           # the opcode-0x0C child descriptor

C2B_OP_RNG = 0x1234
C2B_OP_CHILD = 0x10A9E8   # C2B_OP_REC + 0x68

def c2b_op(op, value=0x10, low=None, r8=None, word_extra=b"", r28=0x2828, r18=0x18181818,
           r1c=0x1C1C1C1C, r20=0x20202020, r24=0x24242424, r50=0x50, r2a=0x2A2A,
           r2c=0x2C2C, r2e=0x2E2E, r30=0x3030, r32=0x3232, r34=0x3434, r36=0x3636,
           r38=0x3838, r4e=0x4E, r4f=0x4F4F4F4F, r59=0x59, r61=0x61, p5e6=0, p5e8=0,
           p5d4=C2B_OP_DESC, pset0c=0x0C0C, prefix=False):
    """One 0x2B2A0 case: the command word (op<<8 | low) at C2B_OP_STR, the record's fields, the
    0x105BE4/6/8 words, the stream base 0x105BD4 and the pset's +0x0C word. `value` is the 0x2B8F8
    stub EAX (the operand); `word_extra` are the bytes after the command word (bounded ops).
    `prefix` forces the 0x1F prefix encoding for an op the direct form would short-circuit."""
    rec, str_ = C2B_OP_REC, C2B_OP_STR
    if r8 is None:
        r8 = str_
    if op >= 0x20 or op == 0x1F or prefix:
        # the 0x1F prefix: anim_operand stores the low byte as the opcode and returns the next word.
        data = (((0x1F << 8) | op).to_bytes(2, "little")
                + (value & 0xFFFF).to_bytes(2, "little") + word_extra)
    else:
        low = (value & 0xFF) if low is None else low
        data = (((op << 8) | low).to_bytes(2, "little") + word_extra)
    p = {rec + 8: le32(r8) + b"\xa5" * 60,
         rec + 0x48: b"\xa5" * 28,
         str_: data + b"\xa5" * (8 - len(data)),
         rec + 0x18: le32(r18) + le32(r1c) + le32(r20) + le32(r24),
         rec + 0x28: r28.to_bytes(2, "little") + r2a.to_bytes(2, "little") + r2c.to_bytes(2, "little")
                     + r2e.to_bytes(2, "little") + r30.to_bytes(2, "little") + r32.to_bytes(2, "little")
                     + r34.to_bytes(2, "little") + r36.to_bytes(2, "little") + r38.to_bytes(2, "little"),
         rec + 0x4E: bytes([r4e]) + le32(r4f) + bytes([r50]) + bytes([0x51]),
         rec + 0x59: bytes([r59]) + b"\xa5" * 7 + bytes([r61]),
         0x00105BE4: p5e6.to_bytes(2, "little") + p5e8.to_bytes(2, "little"),
         # 0x105BD8's high byte is seeded 0xAA: the op-0x0C store (0x2B4B1 `mov [0x105BD8],esi`)
         # writes an image pointer whose top byte is 0, so the byte changes only from a nonzero pre.
         0x00105BD4: le32(p5d4) + le32(0xAA0005D8),
         0x001014EC: le32(C2B_OP_PSET),
         C2B_OP_PSET + 0x20 + 0x0C: pset0c.to_bytes(2, "little"),
         0x000EF6DC: le32(0xEF6DC) + le32(0)}
    p[0x00105BE6] = p5e6.to_bytes(2, "little")
    p[0x00105BE8] = p5e8.to_bytes(2, "little")
    return p

C2B_VOICE_BASE = 0x000BBDC8      # the 0x0C-stride voice table

def c2b_voice(vid, vtype, h=0x1111, b=0x22, cur=0, playing=0, cur_hi=None):
    """One 0x2C3FC case: the id's voice record {type, h, b} and the current-song dword; the playing
    stub (0x1CE70) is set where the type-2/3/4 or the case-5 sub-ids test it. `cur_hi` seeds the
    high two bytes of the 0x105D5C dword the type-1 arm writes (0x2C447 `mov [0x105D5C],eax`), so
    the store's bytes 2/3 change instead of reproducing the pre-state."""
    rec = C2B_VOICE_BASE + vid * 0xC
    pokes = {rec: bytes([vtype]) + b"\x00\x00\x00" + le32(h) + bytes([b, 0, 0]),
             0x00105D5C: le32(cur)}
    if cur_hi is not None:
        pokes[0x00105D5E] = cur_hi
    stubs = {0x1CE70: playing} if vtype in (2, 3, 4) or vtype == 5 else {}
    return {"eax": vid}, pokes, stubs

def c2b_voice5(cid, vid, cur, playing=0):
    """A type-5 case: the dispatcher's music_stop/sample_stop sub-id menu."""
    r, p, s = c2b_voice(vid, 5, cur=cur, playing=playing)
    return Case(cid, r, p, s)

C2B_VOICE_CASES = [
    Case("v0", {"eax": 0}, {0x00105D5C: le32(0x30)}),          # id 0: return 0
    Case("v100", {"eax": 0x100}, {C2B_VOICE_BASE: bytes([1]) + b"\x00\x00\x00" + le32(0x9999)
                                 + bytes([9, 0, 0]), 0x00105D5C: le32(0)}),  # id 0x100 -> record 0
    Case("t0", *c2b_voice(1, 0)),                              # type 0: return 1
    Case("t1", *c2b_voice(2, 1, h=0x1234, b=5, cur=0x30,
                          cur_hi=b"\xDE\xAD")),                 # type 1: music request (the DE/AD seed
                                                                # observes the write's high bytes)
    Case("t2a", *c2b_voice(3, 2, h=0x77, playing=1)),           # type 2, playing: return 0
    Case("t2b", *c2b_voice(4, 2, h=0x78, b=3, playing=0)),      # type 2, free: queue
    Case("t3a", *c2b_voice(0x46, 3, h=1, playing=1)),           # id 0x46, playing
    Case("t3b", *c2b_voice(0x46, 3, h=1, playing=0)),
    Case("t3c", *c2b_voice(0x4D, 3, h=2, playing=0)),
    Case("t3d", *c2b_voice(0x5D, 3, h=3, playing=0)),
    Case("t3e", *c2b_voice(0x10, 3, h=4, playing=0)),           # another type-3 id: return 0
    Case("t3f", *c2b_voice(0x50, 3, h=5, playing=0)),           # an id in (0x4D,0x5D): the 0x2C4D2 tail
    Case("t4a", *c2b_voice(5, 4, playing=1)),                   # type 4, the sample playing
    Case("t4b", *c2b_voice(6, 4, playing=0)),                   # type 4, the queue arm
    *[c2b_voice5("t5_%02x_%d" % (vid, i), vid, cur)
      for (vid, curs) in [(0x00, [0]), (0x22, [0x20, 0x10]), (0x2B, [0x2A, 0]),
                          (0x2D, [0x2C, 0]), (0x2F, [0x2E, 0x30, 0]), (0x33, [0x32, 0]),
                          (0x3C, [0x3B, 0]), (0x3F, [0]), (0x41, [0]), (0x43, [0]),
                          (0x4C, [0]), (0x4F, [0]), (0x55, [0x54, 0]), (0x57, [0x56, 0]),
                          (0x5B, [0]), (0xE0, [0xDF, 0]), (0xE2, [0xE1, 0xE3, 0]), (0xF1, [0]),
                          (0x01, [0])]
      for i, cur in enumerate(curs)],
    Case("t6", *c2b_voice(7, 6)),                               # type 6: return 0
    # the unmatched sub-ids in each case-5 binary-search range, so every range tail is hit.
    *[c2b_voice5("t5_tail_%02x" % vid, vid, 0)
      for vid in (0x02, 0x24, 0x2C, 0x2E, 0x34, 0x42, 0x44, 0x50, 0x51, 0x58, 0x5C, 0x80, 0xE4, 0xF2, 0xFF)],
    # the 0x100 remap with record 0's type 5: the sub-id switch still sees the remapped 0.
    Case("v100b", {"eax": 0x100}, {C2B_VOICE_BASE: bytes([5]) + b"\x00\x00\x00" + le32(1)
                                   + bytes([0, 0, 0]), 0x00105D5C: le32(0)}),
]

C2B_OP_CASES = [
    # One case per opcode: the command word is (op<<8 | value) for the 5-bit ops and the 0x1F prefix
    # (0x1F<op>) plus the operand word for op >= 0x20, so the real anim_operand (allowed) selects the
    # opcode. `o0dp` is the 0x1F prefix carrying op 0x0D: the direct 0x0D test at 0x2B2CA runs before
    # anim_operand, but the prefix stores the low byte back (`mov [0x105BE4],cx` at 0x2B932) and the
    # table dispatch at 0x2B2FC then reaches 0x2B52F. The stub EAX values: set_dead/sample are void;
    # 0x5D7DC rng; 0x2AE14 the child record; 0x29DB8/0x2C3FC void.
    *[Case(cid, {"eax": C2B_OP_REC, "edx": 1, "ebx": flag},
           {**c2b_op(op, value=value, word_extra=we, **{k: v for k, v in kw.items()
                                                       if k in ("r28", "r2a", "r18", "r1c", "p5e6",
                                                                "p5e8", "p5d4", "pset0c", "prefix")})},
           ({0x2B150: 0} if op == 0 else {})
           | ({0x5D7DC: C2B_OP_RNG} if op == 0x08 else {})
           | ({0x2AE14: C2B_OP_CHILD} if op == 0x0C else {})
           | ({0x29DB8: 0} if op in (0x0D, 0x0E, 0x0F, 0x17, 0x18, 0x19) else {})
           | ({0x2C3FC: 0} if op == 0x2E else {}))
      for (cid, op, value, flag, we, kw) in [
        ("o00a", 0x00, 0x11, 1, b"", {"p5e6": 0xAA00}),   # the 0x105BE4 write's high byte seeded
        ("o00b", 0x00, 0x12, 0, b"", {}),
        ("o01", 0x01, 0x13, 0, b"", {}),
        ("o02", 0x02, 0x2B, 0, b"", {}),
        ("o03", 0x03, 0x14, 0, b"", {}),
        ("o04a", 0x04, 0x15, 0, b"", {"p5e8": 5}),
        ("o04b", 0x04, 0x15, 0, b"", {"p5e8": 0}),
        ("o05a", 0x05, 0x00, 0, b"", {}),
        ("o05b", 0x05, 0x01, 0, b"", {}),
        ("o06a", 0x06, 0x02, 0, b"\x01\x00", {}),
        ("o06b", 0x06, 0x02, 0, b"\x00\x00", {}),
        ("o07", 0x07, 0x16, 0, b"", {}),
        ("o08", 0x08, 0x64, 0, b"", {}),
        ("o09", 0x09, 0x17, 0, b"", {}),
        ("o0a", 0x0A, 0x7E, 0, b"", {}),
        ("o0b", 0x0B, 0x18, 0, b"", {}),
        ("o0ca", 0x0C, 0x01, 0, b"\x02\x00\x03\x00", {"p5e8": 1}),
        ("o0cb", 0x0C, 0x02, 0, b"\x02\x00\x03\x00", {"p5e8": 0}),
        ("o0d", 0x0D, 0x1A, 0, b"", {}),
        ("o0dp", 0x0D, 7, 0, b"", {"prefix": True}),
        ("o0e", 0x0E, 0x1B, 0, b"", {}),
        ("o0fa", 0x0F, 0x1C, 0, b"", {"p5e6": 0}),
        ("o0fb", 0x0F, 0x1C, 0, b"", {"p5e6": 1}),
        ("o10", 0x10, 0x29D60, 0, b"", {"p5d4": 0x29D60}),
        ("o11", 0x11, 0x29D60, 0, b"", {"p5d4": 0x29D60}),
        ("o12", 0x12, 0x1D, 0, b"", {}),
        ("o13", 0x13, 0x1E, 0, b"", {}),
        ("o14", 0x14, 0x1F, 0, b"", {}),
        ("o15", 0x15, 0x29D60, 0, b"", {"p5d4": 0x29D60}),
        ("o16", 0x16, 0x20, 0, b"", {"r28": 0x2A28}),
        ("o17", 0x17, 0x21, 0, b"", {"p5e8": 3}),
        ("o18a", 0x18, 0x01, 0, b"\x04\x00", {"p5e8": 2}),
        ("o18b", 0x18, 0x01, 0, b"\x01\x00", {"p5e8": 2}),
        ("o19a", 0x19, 0x03, 0, b"\x01\x00", {"p5e8": 2}),
        ("o19b", 0x19, 0x03, 0, b"\x05\x00", {"p5e8": 2}),
        ("o1a", 0x1A, 0x22, 0, b"", {}),
        ("o1b", 0x1B, 0x23, 0, b"", {}),
        ("o1c", 0x1C, 0x24, 0, b"", {}),
        ("o1d", 0x1D, 0x25, 0, b"", {}),
        ("o1e", 0x1E, 0x26, 0, b"", {"r2a": 0x2E2A}),
        ("o1f", 0x1F, 0x02, 0, b"", {"r28": 0x2828}),
        # o20a/o21: value 0x403 (ax*64 = 0x100C0) and rec+0x18/+0x1C =(0xAAFFFF50) so the add
        # carries through all four bytes of the 0x2B7A9/0x2B7C3 stores (0xAAFFFF50+0x100C0 =
        # 0xAB010010).
        ("o20a", 0x20, 0x403, 0, b"", {"r28": 0x2828, "r18": 0xAAFFFF50}),
        ("o20b", 0x20, 0x02, 0, b"", {"r28": 0x6828}),
        ("o21", 0x21, 0x403, 0, b"", {"r1c": 0xAAFFFF50}),
        ("o22", 0x22, 0x07, 0, b"", {}),                  # value<<6 = 0x1C0 carries into +0x32's high byte
        ("o23", 0x23, 0x00, 0, b"", {}),
        ("o24", 0x24, 0x00, 0, b"", {}),
        ("o25", 0x25, 0x04, 0, b"", {}),
        ("o26", 0x26, 0x05, 0, b"", {}),
        ("o27", 0x27, 0x06, 0, b"", {}),
        ("o28a", 0x28, 0x07, 0, b"", {"r28": 0x2828}),
        ("o28b", 0x28, 0x07, 0, b"", {"r28": 0x6828}),
        ("o29", 0x29, 0x08, 0, b"", {}),
        ("o2a", 0x2A, 0x09, 0, b"", {}),
        ("o2b", 0x2B, 0x0A, 0, b"", {}),
        ("o2c", 0x2C, 0x0B, 0, b"", {}),
        ("o2da", 0x2D, 0x20, 0, b"", {"pset0c": 0x10}),
        ("o2db", 0x2D, 0x20, 0, b"", {"pset0c": 0x20}),
        ("o2e", 0x2E, 0x6F, 0, b"", {}),
        ("o2f", 0x2F, 0x00, 0, b"", {}),
      ]]]

C2B_SPECS = [
    Spec("palette_acquire", 0x33754, [
        # p0: empty table; entry 0 is the first free; sentinels on its fields and a seeded dirty area.
        # The 0x800540 handle's byte 1 (0x05) differs from the free entry's 0, so the handle store
        # (0x337BA) changes bytes 0/1/2 (byte 3 is 0 for every valid handle: the inherent limit); the
        # head at 0x1075F0 makes the 0x337DA bump (head+0x10) carry into byte 1.
        Case("p0", {"eax": 0x800540}, {**c2b_res(0x800540, 3), C2B_PAL_TAIL: le32(C2B_DIRTY_HI),
             C2B_PAL_TABLE + 4: b"\xa5\xa5\xa5\xa5", C2B_PAL_TABLE + 8: b"\xa5\xa5\xa5\xa5",
             C2B_PAL_TABLE + 0x0C: b"\xa5\xa5\xa5\xa5", C2B_DIRTY_HI: b"\xa5" * 0x20}),
        # p1: the handle already owns entry 0: refcount 7 -> 8, nothing else moves.
        Case("p1", {"eax": 0x800040}, {**c2b_res(0x800040, 3), C2B_PAL_TAIL: le32(C2B_DIRTY),
             **c2b_pal_entry(0, 0x800040, 0x1111, 0x2222), C2B_DIRTY: b"\xa5" * 0x20}),
        # p2: entry 1 is free and entry 0 is occupied (start 5, len 4): the new entry starts at 9;
        # the reflow skips free entry 1 and 3, moves entry 2 (0xA5A5A520 -> 12; the seeded high
        # bytes make the 0x337FF start store observe all four bytes, and 12 != 0xA5A5A520 fails
        # the break exactly as 12 != 0x20 did), and entry 4 (14) stops it. Entry 1's +4..+0xF
        # carry sentinels (a free entry is found by its zero handle dword only).
        Case("p2", {"eax": 0x800540}, {**c2b_res(0x800540, 3), C2B_PAL_TAIL: le32(C2B_DIRTY),
             **c2b_pal_entry(0, 0x800001, 5, 4), **c2b_pal_entry(2, 0x800002, 0xA5A5A520, 2),
             **c2b_pal_entry(4, 0x800004, 14, 4), C2B_PAL_TABLE + 0x10: b"\x00\x00\x00\x00",
             C2B_PAL_TABLE + 0x14: b"\xa5" * 12,
             C2B_DIRTY: b"\xa5" * 0x40}),
        # p3: entries 0..22 occupied, slot 23 free: the new entry lands on the last slot, start is the
        # accumulated end 0x170 and the reflow loop does not run (the next slot is the table end).
        # Entry 23's +4..+0xF carry sentinels (its zero handle marks it free).
        Case("p3", {"eax": 0x800540}, {**c2b_res(0x800540, 3), C2B_PAL_TAIL: le32(C2B_DIRTY),
             **c2b_pal_run(23, 0x800100, 0, 0x10),
             C2B_PAL_TABLE + 23 * 0x10: b"\x00\x00\x00\x00",
             C2B_PAL_TABLE + 23 * 0x10 + 4: b"\xa5" * 12,
             C2B_DIRTY: b"\xa5" * 0x20}),
        # p4: the handle owns the last slot. The search walks all 24 entries.
        Case("p4", {"eax": 0x800040}, {**c2b_res(0x800040, 3), C2B_PAL_TAIL: le32(C2B_DIRTY),
             **c2b_pal_entry(23, 0x800040, 0x1111, 0x2222), C2B_DIRTY: b"\xa5" * 0x20}),
        # p5: the resolved count is 0.
        Case("p5", {"eax": 0x800540}, {**c2b_res(0x800540, 0), C2B_PAL_TAIL: le32(C2B_DIRTY),
             C2B_PAL_TABLE + 4: b"\xa5\xa5\xa5\xa5", C2B_PAL_TABLE + 8: b"\xa5\xa5\xa5\xa5",
             C2B_PAL_TABLE + 0x0C: b"\xa5\xa5\xa5\xa5", C2B_DIRTY: b"\xa5" * 0x20}),
        # p6: a nonzero low-23 offset: the count comes from payload+0x40 and the record stores the
        # full handle.
        Case("p6", {"eax": 0x800543}, {**c2b_res(0x800543, 5), C2B_PAL_TAIL: le32(C2B_DIRTY),
             C2B_PAL_TABLE + 4: b"\xa5\xa5\xa5\xa5", C2B_PAL_TABLE + 8: b"\xa5\xa5\xa5\xa5",
             C2B_PAL_TABLE + 0x0C: b"\xa5\xa5\xa5\xa5", C2B_DIRTY: b"\xa5" * 0x20}),
    ], allow_calls=(0x1B544,), eax_mask=0xFFFFFFFF,
       unhit_named={0x3384E: "the table-full fatal: the raw calls 0x62003, the port returns 0 "
                           "(raw-over-port deviation, record §C2b.3)"},
       mutants=("@mutant", "@new", "@inc", "@start", "@reflow", "@count")),
    # 0x13C70 (record §C2b): EAX = source_rec, EDX = byte_arg, EBX = palette handle. Pop the free
    # head (0x249D0), fill the record, zero +0x10 if the source's +0xC signed count is positive, copy
    # resolved[1..] into +0x410, then insert at the active head (0x249B0) under the 0x9AF3C lock.
    # 0x249D0/0x249B0 and the host 0x1B544 are allowed: the port's effects.c keeps its own copies of
    # the two list primitives (no seam), and its writes match the raw's byte for byte. The port skips
    # the +0x410 copy when res_resolve fails; the cases resolve. Mask 0: every caller ignores the
    # return (the raw's EAX at return is 0xFC CE0, the port's is the record offset, record §C2b.3).
    Spec("effects_spawn", 0x13C70, [
        # f0: count 3: both loops run, the copies differ from their sentinels, the lock is restored
        # from 0x5A and the active counter 0x9AF3D starts at 0x7E.
        Case("f0", {"eax": 0x10A100, "edx": 3, "ebx": 0x800043},
             {**c2b_res(0x800043, 7), 0x000FCCE8: le32(0x10A200),
              0x10A200: le32(0x000FCCE0) + le32(0x000FCCE8),
              0x10A208: b"\xa5" * 8, 0x10A210: b"\xa5" * 12, 0x10A610: b"\xa5" * 12,
              0x000FCCE0: le32(0x000FCCE0) + le32(0x000FCCE0), 0x10A10C: le32(3),
              0x0009AF3C: b"\x5a", 0x0009AF3D: b"\x7e",
              C2B_RES_DATA + 0x43 + 4: le32(0x11) + le32(0x22) + le32(0x33)}),
        # f1: count -1: the signed test skips both loops; +0x10 and +0x410 keep their sentinels.
        Case("f1", {"eax": 0x10A100, "edx": 0, "ebx": 0x800043},
             {**c2b_res(0x800043, 7), 0x000FCCE8: le32(0x10A200),
              0x10A200: le32(0x000FCCE0) + le32(0x000FCCE8), 0x10A210: b"\xa5" * 12,
              0x10A610: b"\xa5" * 12, 0x000FCCE0: le32(0x000FCCE0), 0x10A10C: le32(0xFFFFFFFF),
              0x0009AF3C: b"\x00", 0x0009AF3D: b"\x00"}),
        # f2: count 0: the same skip path.
        Case("f2", {"eax": 0x10A100, "edx": 0, "ebx": 0x800043},
             {**c2b_res(0x800043, 7), 0x000FCCE8: le32(0x10A200),
              0x10A200: le32(0x000FCCE0) + le32(0x000FCCE8), 0x10A210: b"\xa5" * 12,
              0x10A610: b"\xa5" * 12, 0x000FCCE0: le32(0x000FCCE0), 0x10A10C: le32(0),
              0x0009AF3C: b"\x00", 0x0009AF3D: b"\x00"}),
        # f3: the free list is empty (its sentinel points at itself): the raw returns with rec 0 and
        # EAX = the still-untouched source_rec; the port returns 0. Mask 0 makes that agree.
        Case("f3", {"eax": 0x10A100, "edx": 9, "ebx": 0x800043},
             {0x000FCCE8: le32(0x000FCCE8), 0x000FCCE0: le32(0x000FCCE0), 0x10A10C: le32(3),
              0x0009AF3C: b"\x11"}),
        # f4: count 1: each loop runs exactly once.
        Case("f4", {"eax": 0x10A100, "edx": 0xFF, "ebx": 0x800043},
             {**c2b_res(0x800043, 7), 0x000FCCE8: le32(0x10A200),
              0x10A200: le32(0x000FCCE0) + le32(0x000FCCE8), 0x10A210: b"\xa5" * 8,
              0x10A610: b"\xa5" * 8, 0x000FCCE0: le32(0x000FCCE0), 0x10A10C: le32(1),
              0x0009AF3C: b"\x00", 0x0009AF3D: b"\x00",
              C2B_RES_DATA + 0x43 + 4: le32(0x7777)}),
    ], allow_calls=(0x1B544, 0x249B0, 0x249D0), eax_mask=0,
       mutants=("@mutant", "@count", "@copy", "@lock", "@free")),
    # 0x3B298 (record §C2b): EAX = side, EDX = arg. The command dispatch: ctx swap (0x33A10) and the
    # anim triple (0x3AFC4) are allowed; 0x3B134 (the mapper), 0x1AB5C (the input mask), 0x46460
    # (one ring word) and 0x1A734 (the block hit) are stubbed with per-case values, so the cases
    # select each arm. anim[2]+2 is the word at 0xA6728 + ((char<<6)+arg)*6 + 2. Mask 0xFF (`test
    # al,al` at the caller). 0x43's bits 0x20/0x10 are the block flags; +0x86 copies the other
    # slot's +0x84.
    Spec("fighter_command_dispatch", 0x3B298, [
        # c0: anim[2]+2 has both bits 0 and 1: return 0 before the scan.
        Case("c0", {"eax": 0, "edx": 0}, {**SLOT_PTRS, **c2b_3b298_pokes(0, 0, 0, 0, 0, 0, 0, 0, 3, 0x40)}),
        # c1: no ring hits, mask 0, cmd 0, the +0x100CDE word <= 1: b1 = b2 = 0, return 0.
        Case("c1", {"eax": 0, "edx": 0}, {**SLOT_PTRS, **c2b_3b298_pokes(0, 0, 0, 0, 0, 0, 0, 0, 0, 0x40)},
             {0x1AB5C: 0, 0x46460: 0}),
        # c2: one ring word 0x4000 against mask 0x4000: b2, anim bits 0: set +0x43 bit 0x10, hit.
        Case("c2", {"eax": 0, "edx": 0}, {**SLOT_PTRS, **c2b_3b298_pokes(0, 0, 0, 0x4000, 0x4000, 1, 0, 0, 0, 0x40)},
             {0x1AB5C: 0x4000, 0x46460: 0x4000}),
        # c3: side 1, ring word 1 against mask 1: bit 0x4000 clear and 0x8000 clear: b1, set +0x43
        # bit 0x20, hit.
        Case("c3", {"eax": 1, "edx": 0}, {**SLOT_PTRS, **c2b_3b298_pokes(1, 0, 0, 1, 1, 1, 0, 0, 0, 0x40)},
             {0x1AB5C: 1, 0x46460: 1}),
        # c4: b1 but anim bit 0 set: the b1 arm is skipped, b2 is 0: return 0.
        Case("c4", {"eax": 0, "edx": 0}, {**SLOT_PTRS, **c2b_3b298_pokes(0, 0, 0, 1, 1, 1, 0, 0, 1, 0x40)},
             {0x1AB5C: 1, 0x46460: 1}),
        # c5: b2 but anim bit 1 set: the b2 arm is skipped: return 0.
        Case("c5", {"eax": 0, "edx": 0}, {**SLOT_PTRS, **c2b_3b298_pokes(0, 0, 0, 0x4000, 0x4000, 1, 0, 0, 2, 0x40)},
             {0x1AB5C: 0x4000, 0x46460: 0x4000}),
        # c6: no ring hits; the command word 0x4000 against mask 0x4000: b2 and the b2 arm.
        Case("c6", {"eax": 0, "edx": 0}, {**SLOT_PTRS, **c2b_3b298_pokes(0, 0, 0, 0x4000, 0, 0, 0x4000, 0, 0, 0x40)},
             {0x1AB5C: 0x4000, 0x46460: 0}),
        # c7: the command word 1 against mask 1 (bit 0x8000 clear): b1 and the b1 arm.
        Case("c7", {"eax": 0, "edx": 0}, {**SLOT_PTRS, **c2b_3b298_pokes(0, 0, 0, 1, 0, 0, 1, 0, 0, 0x40)},
             {0x1AB5C: 1, 0x46460: 0}),
        # c8: the forced block: the +0x100CDE word > 1, anim bit 0x80 set and +0x43 bit 0x30 set.
        Case("c8", {"eax": 0, "edx": 0}, {**SLOT_PTRS, **c2b_3b298_pokes(0, 0, 0, 0, 0, 0, 0, 2, 0x80, 0x30)},
             {0x1AB5C: 0, 0x46460: 0}),
        # c9: the forced block's > 1 test fails (word == 1): no force.
        Case("c9", {"eax": 0, "edx": 0}, {**SLOT_PTRS, **c2b_3b298_pokes(0, 0, 0, 0, 0, 0, 0, 1, 0x80, 0x30)},
             {0x1AB5C: 0, 0x46460: 0}),
        # c10: word > 1 but anim bit 0x80 clear: no force.
        Case("c10", {"eax": 0, "edx": 0}, {**SLOT_PTRS, **c2b_3b298_pokes(0, 0, 0, 0, 0, 0, 0, 2, 0, 0x30)},
             {0x1AB5C: 0, 0x46460: 0}),
        # c11: word > 1 and bit 0x80 set but +0x43's 0x30 bits clear: no force.
        Case("c11", {"eax": 0, "edx": 0}, {**SLOT_PTRS, **c2b_3b298_pokes(0, 0, 0, 0, 0, 0, 0, 2, 0x80, 0)},
             {0x1AB5C: 0, 0x46460: 0}),
        # c12: one ring word 0 against mask 0x4000: the loop's filter skips it.
        Case("c12", {"eax": 0, "edx": 0}, {**SLOT_PTRS, **c2b_3b298_pokes(0, 0, 0, 0x4000, 0, 1, 0, 0, 0, 0x40)},
             {0x1AB5C: 0x4000, 0x46460: 0}),
        # c13: one ring word 0x8001 against mask 1: the match has neither block bit, its 0x8000 bit
        # is set: the loop continues.
        Case("c13", {"eax": 0, "edx": 0}, {**SLOT_PTRS, **c2b_3b298_pokes(0, 0, 0, 1, 0x8001, 1, 0, 0, 0, 0x40)},
             {0x1AB5C: 1, 0x46460: 0x8001}),
        # c14: one ring word 1 against mask 0x4000: nonzero, but the mask filter skips it (a mutant
        # that only tests the word for zero would arm b1).
        Case("c14", {"eax": 0, "edx": 0}, {**SLOT_PTRS, **c2b_3b298_pokes(0, 0, 0, 0x4000, 1, 1, 0, 0, 0, 0x40)},
             {0x1AB5C: 0x4000, 0x46460: 1}),
    ], allow_calls=(0x33A10, 0x3AFC4),
       calls=(E.Call(0x3B134, ("eax", "edx", "ebx"), clobbers=("ebx", "edx", "edi", "ebp")),
              E.Call(0x1AB5C, ("eax",), clobbers=("ebp",)),
              E.Call(0x46460, ("eax", "edx"), clobbers=("edx",)),
              E.Call(0x1A734, ("eax",))),
       eax_mask=0xFF, mutants=("@mutant", "@copy", "@early", "@scan", "@b2", "@force", "@arm")),
    # 0x3B714 (record §C2b): EAX = param_1 (the other slot), EDX = param_2 (the winner's slot). The
    # reaction applier. side = DSD(param_2)[0x51]; ctx = 0x33950(side) (allow); the frame flag
    # 0x3C59C, the anim triple 0x3AFC4 (allow) and 0x39EFC (its own row, run real) gate; the clean
    # path runs 0x3B298 (stub), 0x3B080/0x3AE9C/0x2BD44/0x3AAFC/0x3AD98/0x3B6C4 (stubs), the
    # 0x3B6C4 hold copies the own record's +0x34 onto the other, then +0x41 |= 0x80. The
    # local_24 == 0xFF fatal (0x3B75D, the raw's 0x62003) is unhit and named. Mask 0: the caller
    # 0x193B0 ignores the return.
    Spec("fighter_reaction", 0x3B714, [
        # r0: the frame flag is already set: return at once.
        Case("r0", {"eax": DS_SLOTS + 0x94, "edx": DS_SLOTS}, {**c2b_3b714(0, 0, 0x10)},
             {0x3C59C: 1, 0x3B298: 0, 0x3B080: 0, 0x3AE9C: 0, 0x2BD44: 0, 0x3AAFC: 0,
              0x3AD98: 0, 0x3B6C4: 0}),
        # r1: anim[2]+2 bit 0x800: return.
        Case("r1", {"eax": DS_SLOTS + 0x94, "edx": DS_SLOTS}, {**c2b_3b714(0, 0, 0x10, bits=0x800)},
             {0x3C59C: 0, 0x3B298: 0, 0x3B080: 0, 0x3AE9C: 0, 0x2BD44: 0, 0x3AAFC: 0,
              0x3AD98: 0, 0x3B6C4: 0}),
        # r2: 0x39EFC holds and anim bit 0x4000 is clear: return.
        Case("r2", {"eax": DS_SLOTS + 0x94, "edx": DS_SLOTS}, {**c2b_3b714(0, 0, 0x10, efc=1)},
             {0x3C59C: 0, 0x3B298: 0, 0x3B080: 0, 0x3AE9C: 0, 0x2BD44: 0, 0x3AAFC: 0,
              0x3AD98: 0, 0x3B6C4: 0}),
        # r3: 0x39EFC holds but anim bit 0x4000 is set: past the second gate; command dispatch 1:
        # the else arm (0x8A cleared, 0x3AD98), no 0x3B6C4 copy.
        Case("r3", {"eax": DS_SLOTS + 0x94, "edx": DS_SLOTS}, {**c2b_3b714(0, 0, 0x10, bits=0x4000, efc=1,
             p2_8a=0x8a)},
             {0x3C59C: 0, 0x3B298: 1, 0x3B080: 0, 0x3AE9C: 0, 0x2BD44: 0, 0x3AAFC: 0,
              0x3AD98: 0, 0x3B6C4: 0}),
        # r4: the full path: param_1's +0x52 == 4 (the two stores), the 0x3B080 seed (param_1's
        # +0x54 != 2), the 0x3AE9C landing (the winner's +0x52 == 4), dispatch 0, the 2bd44 index
        # zero, 0x3AAFC, and the 0x3B6C4 hold copy.
        Case("r4", {"eax": DS_SLOTS + 0x94, "edx": DS_SLOTS}, {**c2b_3b714(0, 0, 0x10, p1_52=4, p1_54=1,
             p2_52=4, rec4_34=0x1111, rec5_34=0x2222)},
             {0x3C59C: 0, 0x3B298: 0, 0x3B080: 0, 0x3AE9C: 0, 0x2BD44: 0, 0x3AAFC: 0,
              0x3AD98: 0, 0x3B6C4: 1}),
        # r5: the skip arms: param_1's +0x52 != 4, +0x54 == 2 (no 0x3B080), winner's +0x52 != 4
        # (no 0x3AE9C) and dispatch 1 (0x3AD98); no hold copy.
        Case("r5", {"eax": DS_SLOTS + 0x94, "edx": DS_SLOTS}, {**c2b_3b714(0, 0, 0x10, p1_52=0, p1_54=2)},
             {0x3C59C: 0, 0x3B298: 1, 0x3B080: 0, 0x3AE9C: 0, 0x2BD44: 0, 0x3AAFC: 0,
              0x3AD98: 0, 0x3B6C4: 0}),
        # r6: dispatch 0 and the other record's +0x4B nonzero with its pool row's +0x60 set: the
        # 0x2BD44 call, then 0x3AAFC.
        Case("r6", {"eax": DS_SLOTS + 0x94, "edx": DS_SLOTS}, {**c2b_3b714(0, 0, 0x10, rec5_4b=3, row60=1)},
             {0x3C59C: 0, 0x3B298: 0, 0x3B080: 0, 0x3AE9C: 0, 0x2BD44: 0, 0x3AAFC: 0,
              0x3AD98: 0, 0x3B6C4: 0}),
        # r7: the same index but the row's +0x60 clear: no 0x2BD44 call.
        Case("r7", {"eax": DS_SLOTS + 0x94, "edx": DS_SLOTS}, {**c2b_3b714(0, 0, 0x10, rec5_4b=3, row60=0)},
             {0x3C59C: 0, 0x3B298: 0, 0x3B080: 0, 0x3AE9C: 0, 0x2BD44: 0, 0x3AAFC: 0,
              0x3AD98: 0, 0x3B6C4: 0}),
        # r8: side 1 with its own character byte and a different anim record: the side/char/index
        # choices do not fall back on side 0's.
        Case("r8", {"eax": DS_SLOTS, "edx": DS_SLOTS + 0x94}, {**c2b_3b714(1, 1, 0x20, bits=0x4000)},
             {0x3C59C: 0, 0x3B298: 0, 0x3B080: 0, 0x3AE9C: 0, 0x2BD44: 0, 0x3AAFC: 0,
              0x3AD98: 0, 0x3B6C4: 0}),
    ], allow_calls=(0x33950, 0x33A10, 0x3AFC4),
       calls=(E.Call(0x3C59C, ("eax", "edx"), clobbers=("edx",)),
              E.Call(0x3B298, ("eax", "edx"), clobbers=("edx",)),
              E.Call(0x39EFC, ("eax",), mode="real"),
              E.Call(0x3B080, ("eax", "edx", "ebx", "ecx"), clobbers=("ebx", "ecx", "edx")),
              E.Call(0x3AE9C, ("eax", "edx"), clobbers=("edx",)),
              E.Call(0x2BD44, ("eax", "edx"), clobbers=("edx",)),
              E.Call(0x3AAFC, ("eax", "s0"), pop=4, clobbers=("ebx", "ecx", "edx", "edi", "ebp")),
              E.Call(0x3AD98, ("eax", "[edx]", "[edx+4]", "[edx+8]"), clobbers=("edx",)),
              E.Call(0x3B6C4, ("eax",))),
       eax_mask=0, mutants=("@mutant", "@hold", "@swap", "@early", "@efc", "@branch"),
       unhit_named={0x3B75D: "local_24 == 0xFF: the raw calls the 0x62003 fatal; the port returns "
                           "0 (raw-over-port deviation, record §C2b.3)"}),
    # 0x39834 (record §C2b): EAX = side, EDX = the reaction byte b. The winner's pose driver. The
    # ctx swap (0x33A10) and the anim triple (0x3AFC4) are allowed; 0x39738/0x392A0/0x36CE4/0x4F434
    # are stubbed with per-case values (0x39738's EAX is the scaler's r); 0x468D8 and 0x36D98 run
    # real (their own rows) and 0x2C3FC is stubbed. The k source is DSD(0x107D2A + ctx0*2) >> 16 and
    # the +0x107D2C/+0x107D20 word counters are DSW(0x107D2C + ctx0*2)/DSW(0x107D20 + ctx0*2).
    Spec("fighter_39834", 0x39834, [
        # f0: k = 1 (the table arm): ebx = 250*0x40/100 = 0xA0; r = 7 with the 0x468D8 predicate
        # true: 0x36D98 runs; the tail gates are open (4f434). m2c = 0x01F9 so the +0x107D22 word
        # counter (m2c + r = 0x0200) carries into its high byte.
        Case("f0", {"eax": 0, "edx": 5}, {**c2b_39834(0, 5, 3, k=1, a1=0x40, tbl=250, r=7, ai=1,
             s5d=0x10, p_104abc=1, p_104b14=0, m2c=0x01F9)},
             {0x39738: 7, 0x392A0: 0, 0x36CE4: 0, 0x2C3FC: 0, 0x4F434: 0}),
        # f1: k = 0xFF (the divide arm): ebx = 0x40/16 = 4; the predicate false and +0x5D >= 0x44
        # with 0x1078F2+side set: the zero arm (no 0x36CE4). The +0x43 seed has bit 2 set, so the
        # 0x39917 `and cl,0xFB` clear is observable. k's low byte 0xFF makes the +0x107D2E word
        # counter (k + 1 = 0x0100) carry into its high byte.
        Case("f1", {"eax": 0, "edx": 1}, {**c2b_39834(0, 1, 3, k=0xFF, a1=0x40, r=0, ai=0, s5d=0x50,
             f2=1, p_104abc=0, s43=0x47)},
             {0x39738: 0, 0x392A0: 0, 0x36CE4: 0, 0x2C3FC: 0, 0x4F434: 0}),
        # f2: the same but 0x1078F2 clear: 0x36CE4 runs.
        Case("f2", {"eax": 0, "edx": 2}, {**c2b_39834(0, 2, 3, k=0xC, a1=0x40, r=0, ai=0, s5d=0x50,
             f2=0, p_104abc=0)},
             {0x39738: 0, 0x392A0: 0, 0x36CE4: 0, 0x2C3FC: 0, 0x4F434: 0}),
        # f3: the predicate true but r <= 0: the else-if arm (the 0x398C9 jle).
        Case("f3", {"eax": 0, "edx": 3}, {**c2b_39834(0, 3, 3, k=1, r=0, ai=1, s5d=0x50, f2=1)},
             {0x39738: 0, 0x392A0: 0, 0x36CE4: 0, 0x2C3FC: 0, 0x4F434: 0}),
        # f4: the predicate false and +0x5D below 0x44: both arms skipped.
        Case("f4", {"eax": 0, "edx": 4}, {**c2b_39834(0, 4, 3, k=1, r=0, ai=0, s5d=0x43)},
             {0x39738: 0, 0x392A0: 0, 0x36CE4: 0, 0x2C3FC: 0, 0x4F434: 0}),
        # f5: k = 0x13 (the +0x107D2E counter the same dword's high word bumps to 0x14 before
        # 0x39973), so the +0x29A store at 0x107824 + side*0x94 runs; the nested 0x104B14 test
        # blocks 0x4F434.
        Case("f5", {"eax": 0, "edx": 0x10}, {**c2b_39834(0, 0x10, 3, k=0x13, r=0, ai=0, s5d=0x10,
             p_104abc=1, p_104b14=1)},
             {0x39738: 0, 0x392A0: 0, 0x36CE4: 0, 0x2C3FC: 0, 0x4F434: 0}),
        # f6: side 1 with its own character byte (the 0x1078BE byte) and the k dword at
        # 0x107D2A; the predicate true via +0x52 == 7. k = 0xFF (divide) and m2c = 0x01FF make
        # both side-1 words carry into their high bytes (k + 1 = 0x0100, m2c + r = 0x0202).
        Case("f6", {"eax": 1, "edx": 6}, {**c2b_39834(1, 6, 4, k=0xFF, a1=0x40, r=3, ai=1,
             s52=7, s5d=0x10, p_104abc=1, p_104b14=0, m2c=0x01FF)},
             {0x39738: 3, 0x392A0: 0, 0x36CE4: 0, 0x2C3FC: 0, 0x4F434: 0}),
    ], allow_calls=(0x33A10, 0x3AFC4),
       calls=(E.Call(0x39738, ("eax", "edx"), clobbers=("edx",)),
              E.Call(0x392A0, ("eax", "edx", "ebx"), clobbers=("ebx", "edx")),
              E.Call(0x468D8, ("eax",), mode="real"),
              E.Call(0x36D98, ("eax",), mode="real"),
              E.Call(0x36CE4, ("eax",)),
              E.Call(0x2C3FC, ("eax",)),
              E.Call(0x4F434, ())),
       eax_mask=0, mutants=("@mutant", "@ai", "@arm", "@thr", "@tail")),
    # 0x36870 (record §C2b): EAX = rec. The +0x54 machine. All eleven callees are stubbed through
    # their seams: 0x385B0 (the 0x25 mode reset), 0x39280, 0x164E8, 0x39040, 0x37D18, 0x365C8,
    # 0x36BC8, 0x36638, 0x2BC30 (anim-begin, three sites), 0x3C520 (case 2) and 0x379C4 (case 4);
    # 0x365C8/0x36638 return the per-case AL. Mask 0 (the animation-opcode target's return is
    # dropped by 0x2B2A0's 0x10/0x11/0x15 arms).
    Spec("fighter_36870", 0x36870, [
        # f0: mode 0x25: the 0x385B0 reset runs and returns.
        Case("f0", {"eax": E3_REC}, {**c2b_36870(0, mode=0x25)}),
        # f1: +0x54 = 0 with +0x42 bit 5: 0x37D18 and return. The +0x40 byte-0 seed (0xC0) is
        # cleared to 0x40 by the case-0 `& 0x7F7F`, and +0x42 bit 2 (0x26) is cleared by the
        # `& 0xCCF3BFFF`, so both dword bytes change.
        Case("f1", {"eax": E3_REC}, {**c2b_36870(0, s54=0, s40=0x404040C0, s42=0x26, s43=0x43)}),
        # f2: +0x43 bit 2 runs 0x36BC8 after the 0x365C8 hit sets bit 0x40 (the +0x41 bit-2 mask
        # clears +0x43 bit 2 when it runs, so the two are separate cases).
        Case("f2", {"eax": E3_REC}, {**c2b_36870(0, s54=0, s41=0x41, s42=0x42, s43=0x04,
             r365=1, char=1)}, {0x365C8: 1}),
        # f2b: the +0x41 bit-2 arm runs 0x39280 and its +0x40 mask; 0x36638 then returns nonzero.
        Case("f2b", {"eax": E3_REC}, {**c2b_36870(0, s54=0, s41=0x44, s42=0x12, s43=0x43,
             char=1)}, {0x365C8: 0, 0x36638: 1}),
        # f3: +0x42 bit 4 skips the 0x36BC8 test; 0x36638 returns 0 and mode 3 restarts the record
        # (anim-begin) then returns before the side stream.
        Case("f3", {"eax": E3_REC}, {**c2b_36870(0, s54=0, s42=0x52, s43=0x43, mode=3, char=1)}),
        # f4: the same with mode 0: after the restart the side's 0x102900 record starts the 0xE906A
        # stream.
        Case("f4", {"eax": E3_REC}, {**c2b_36870(0, s54=0, s42=0x12, s43=0x43, mode=0, char=1)},
             {0x365C8: 0, 0x36638: 0}),
        # f5: 0x36638 returns nonzero: no restart.
        Case("f5", {"eax": E3_REC}, {**c2b_36870(0, s54=0, s42=0x12, s43=0x43)},
             {0x36638: 1}),
        # f6: +0x54 = 1: the case-1 arm (mask +0x40, 0x365C8, 0x36638 0, +0x52 = 5 and the
        # 0xC89A0 stream).
        Case("f6", {"eax": E3_REC}, {**c2b_36870(0, s54=1, s41=0x41, s42=0x42, s43=0x43, char=1)},
             {0x365C8: 1, 0x36638: 0}),
        # f6b: the case-1 miss: +0x43 bit 0x40 cleared instead.
        Case("f6b", {"eax": E3_REC}, {**c2b_36870(0, s54=1, s41=0x41, s42=0x42, s43=0x43, char=1)},
             {0x365C8: 0, 0x36638: 1}),
        # f7: +0x54 = 2: 0x3C520 with the 0xC89F0 stream.
        Case("f7", {"eax": E3_REC}, {**c2b_36870(0, s54=2, char=1)}),
        # f8: +0x54 = 4: 0x379C4.
        Case("f8", {"eax": E3_REC}, {**c2b_36870(0, s54=4)}),
        # f9: +0x54 = 3: nothing after the shared resets.
        Case("f9", {"eax": E3_REC}, {**c2b_36870(0, s54=3)}),
        # f10: +0x54 = 5: the default arm.
        Case("f10", {"eax": E3_REC}, {**c2b_36870(0, s54=5)}),
        # f11: side 1 (the record's +0x51 byte selects its own slot): the +0x42 bit-5 arm again,
        # with the side-1 siblings of f1's +0x40/+0x42 seeds (0x107884/0x107886).
        Case("f11", {"eax": E3_REC2}, {**c2b_36870(1, s54=0, s40=0x404040C0, s42=0x26, s43=0x43,
             char=2)}),
    ], calls=(E.Call(0x385B0, ("eax",)),
              E.Call(0x39280, ("eax",)),
              E.Call(0x164E8, ("eax",)),
              E.Call(0x39040, ("eax",)),
              E.Call(0x37D18, ("eax", "edx"), clobbers=("edx",)),
              E.Call(0x365C8, ("eax", "edx", "ebx"), clobbers=("ebx", "edx")),
              E.Call(0x36BC8, ("eax", "edx"), clobbers=("edx",)),
              E.Call(0x36638, ("eax", "edx"), clobbers=("edx",)),
              E.Call(0x2BC30, ("eax", "edx", "s0"), pop=4, clobbers=("edx",)),
              E.Call(0x3C520, ("eax", "edx", "s0"), pop=4, clobbers=("edx",)),
              E.Call(0x379C4, ("eax",), clobbers=("ecx", "esi", "edi", "ebp"))),
       eax_mask=0, mutants=("@mutant", "@arm", "@so", "@case", "@mask")),
    # 0x2AE14 (record §C2b): EAX = desc, EDX = a2, ECX = a3, EBX = a4, stack = a5. The spawn: alloc
    # (0x2AC80 stub), the descriptor field copy, the initial animation walk (0x2B2A0 stub, status
    # 0 loops / 2 -> id 0x1E1, else 0x2A408 stub), the pset writes (0x2A820 stub), the mode-1
    # cursor (0x2A620 stub, rec+0x28 bit 0x10), the type-callback indirect at 0x2B0E9 (0x5D812
    # allow for the visible arm; 0x127C0 allow, whose empty-list path is self-contained, for the
    # invisible one) and 0x1C390/0x1C3A0 (allow: the port's render_list_insert performs both). The
    # raw's 0x2AE3C zeroes a5's high word (EDX is cleared at 0x2AE35); the cases keep a5's high word
    # 0, so both sides agree.
    Spec("actor_spawn", 0x2AE14, [
        # g0: the alloc fails: return 0.
        Case("g0", {"eax": C2B_2AE14_DESC, "edx": 2, "ecx": 3, "ebx": 4, "s0": 0},
             {**c2b_2ae14()}, {0x2AC80: 0}),
        # g1: frame 5 (the float -1 arm), rec+0x28 bit 0x800 skips the walk (0x2A408), hdl 0 (no
        # palette), a5 without 0x400 (the +0x4A clear at the end).
        Case("g1", {"eax": C2B_2AE14_DESC, "edx": 0x2222, "ecx": 0x3333, "ebx": 0x4444, "s0": 0},
             {**c2b_2ae14(frame=5, dp8=0x0800, a2=0x2222, a3=0x3333, a4=0x4444)},
             {0x2AC80: C2B_2AE14_REC, 0x2A408: 0x1234, 0x2B2A0: 0}),
        # g2: the walk's first word has bit 15 clear: no 0x2B2A0 call, the id comes from 0x2A408.
        Case("g2", {"eax": C2B_2AE14_DESC, "edx": 2, "ecx": 3, "ebx": 4, "s0": 0},
             {**c2b_2ae14(dp8=0, word1=0x0100)},
             {0x2AC80: C2B_2AE14_REC, 0x2A408: 0x2222, 0x2B2A0: 0}),
        # g3: the first word has bit 15 set and the second clears it: one 0x2B2A0 call (status 0)
        # then the 0x2A408 id.
        Case("g3", {"eax": C2B_2AE14_DESC, "edx": 2, "ecx": 3, "ebx": 4, "s0": 0},
             {**c2b_2ae14(dp8=0, word1=0x8000, word2=0x0100)},
             {0x2AC80: C2B_2AE14_REC, 0x2A408: 0x3333, 0x2B2A0: 0}),
        # g4: the 0x2B2A0 stub returns 2: id 0x1E1, no 0x2A408 call.
        Case("g4", {"eax": C2B_2AE14_DESC, "edx": 2, "ecx": 3, "ebx": 4, "s0": 0},
             {**c2b_2ae14(dp8=0, word1=0x8000)},
             {0x2AC80: C2B_2AE14_REC, 0x2A408: 0x4444, 0x2B2A0: 2}),
        # g5: a palette handle: 0x33754 (stub) fills pset+0x18.
        Case("g5", {"eax": C2B_2AE14_DESC, "edx": 2, "ecx": 3, "ebx": 4, "s0": 0},
             {**c2b_2ae14(dp8=0x0800, hdl=0x800040, word1=0)},
             {0x2AC80: C2B_2AE14_REC, 0x2A408: 0x5555, 0x2B2A0: 0, 0x33754: 0xB0B}),
        # g6: rec+0x28 bit 0x1000 runs the mode-1 cursor after pset_write.
        Case("g6", {"eax": C2B_2AE14_DESC, "edx": 2, "ecx": 3, "ebx": 4, "s0": 0},
             {**c2b_2ae14(dp8=0x1800, word1=0)},
             {0x2AC80: C2B_2AE14_REC, 0x2A408: 0x6666, 0x2B2A0: 0}),
        # g7: the type callback is the 0x127C0 allow (empty list -> AL 0xFF): the record dies.
        Case("g7", {"eax": C2B_2AE14_DESC, "edx": 2, "ecx": 3, "ebx": 4, "s0": 0},
             {**c2b_2ae14(dp8=0x0800, word1=0, cb=0x127C0)},
             {0x2AC80: C2B_2AE14_REC, 0x2A408: 0x7777, 0x2B2A0: 0}),
        # g8: a5 bit 0x400: the parent-index branch (parent = pool + (a5&0x7f)*0x68 = pool), the
        # layer comes from the parent when a3 is 0, and the +0x4A clear is skipped.
        Case("g8", {"eax": C2B_2AE14_DESC, "edx": 0x2222, "ecx": 0, "ebx": 0x4444, "s0": 0x400},
             {**c2b_2ae14(a2=0x2222, a4=0x4444, a5=0x400, dp8=0x0800, word1=0),
              C2B_2AE14_POOL + 0x5A: b"\x5a", C2B_2AE14_POOL + 0x4F: b"\x4f",
              C2B_2AE14_POOL + 0x49: b"\x49", C2B_2AE14_POOL + 0x28: b"\x28\x02"},
             {0x2AC80: C2B_2AE14_REC, 0x2A408: 0x8888, 0x2B2A0: 0}),
        # g10: rec+0x28 bit 0x2000 stores a3 to rec+0x49 instead of rec+0x32.
        Case("g10", {"eax": C2B_2AE14_DESC, "edx": 2, "ecx": 0x33, "ebx": 4, "s0": 0},
             {**c2b_2ae14(a3=0x33, dp8=0x2800, word1=0)},
             {0x2AC80: C2B_2AE14_REC, 0x2A408: 0xAAAA, 0x2B2A0: 0}),
        # g11: the parent branch with a nonzero layer: rec+0x49 takes a3 (0x2AFEE).
        Case("g11", {"eax": C2B_2AE14_DESC, "edx": 0x2222, "ecx": 0x5A, "ebx": 0x4444, "s0": 0x400},
             {**c2b_2ae14(a2=0x2222, a3=0x5A, a4=0x4444, a5=0x400, dp8=0x0800, word1=0),
              C2B_2AE14_POOL + 0x5A: b"\x5a", C2B_2AE14_POOL + 0x4F: b"\x4f",
              C2B_2AE14_POOL + 0x49: b"\x49", C2B_2AE14_POOL + 0x28: b"\x28\x02"},
             {0x2AC80: C2B_2AE14_REC, 0x2A408: 0xBBBB, 0x2B2A0: 0}),
    ], allow_calls=(0x1C390, 0x1C3A0, 0x127C0, 0x5D812),
       calls=(E.Call(0x2AC80, ("eax",)),
              E.Call(0x2B2A0, ("eax", "edx", "ebx"), clobbers=("ebx", "edx")),
              E.Call(0x2A408, ("eax", "edx"), clobbers=("edx",)),
              E.Call(0x33754, ("eax",)),
              E.Call(0x2A820, ("eax", "edx"), clobbers=("edx",)),
              E.Call(0x2A620, ("eax", "edx"), clobbers=("edx",))),
       eax_mask=0xFFFFFFFF, mutants=("@mutant", "@minus", "@pal", "@type"),
       unhit_named={0x2B071: "rec+0x5F is set to 1 at 0x2AF31, so the pset+2 word's zero arm "
                           "cannot be reached (a dead block in the raw)"}),
    # 0x2C3FC (record §C2b): EAX = voice id. The record at 0xBBDC8 + id*0xC ({type, h, b}) selects
    # the arm: 0 no-op, 1 music request, 2 sample queue unless playing, 3 the 0x46/0x4D/0x5D pairs,
    # 4 the unpause pair, 5 the music_stop/sample_stop sub-id menu on DSD(0x105D5C), >= 6 nothing.
    # The audio callees are stubbed through their seams (music_stop and samples_stop_all return
    # through PR_SEAM_RET0, the unpause pair through PR_SEAM0). Mask 0xFF (AL, `mov al,1`).
    Spec("sound_voice", 0x2C3FC, C2B_VOICE_CASES,
       calls=(E.Call(0x1CA14, ("eax", "edx"), clobbers=("edx",)),
              E.Call(0x1CA6C, ()),
              E.Call(0x1CC28, ("eax", "edx"), clobbers=("edx",)),
              E.Call(0x1CD9C, ()),
              E.Call(0x1CE04, ("eax",)),
              E.Call(0x1CE70, ("eax",)),
              E.Call(0x1D238, ()),
              E.Call(0x1D244, ())),
       eax_mask=0xFF, mutants=("@mutant", "@queue", "@case5", "@play")),
    # 0x2B2A0 (record §C2b): EAX = rec, EDX = index, EBX = flag. The animation-opcode dispatcher.
    # 0x2B8F8 (the operand), 0x2B150 (set_dead), 0x5D7DC (rng), 0x2AE14 (the opcode-0x0C child),
    # 0x29DB8 (the variable write) and 0x2C3FC (the opcode-0x2E voice) are stubbed through their
    # seams; 0x2EA64 (a bare `ret`) and the 0x10/0x11/0x15 indirect target 0x29D60 are allowed
    # (the port's fn_resolve has no entry for it, so both sides do nothing). Mask 0xFF.
    Spec("spawn_anim_opcode", 0x2B2A0, C2B_OP_CASES,
       allow_calls=(0x2EA64, 0x29D60, 0x2B8F8, 0x29F34),
       calls=(E.Call(0x2B150, ("eax",)),
              E.Call(0x5D7DC, ("eax",)),
              E.Call(0x2AE14, ("eax", "edx", "ecx", "ebx", "s0"), pop=4,
                     clobbers=("ebx", "ecx", "edx")),
              E.Call(0x29DB8, ("eax", "edx", "ebx"), clobbers=("ebx", "edx")),
              E.Call(0x2C3FC, ("eax",))),
       eax_mask=0xFF, mutants=("@mutant", "@indirect", "@child", "@skip")),
]

# ---- track P batch C3 (record 2026-10-05-reverse-c3): the frontier callee rows ------------------
#
# 0x249B0 (record C3 §C3.2): EAX = `at`, EDX = `rec`. The splice insert: next = [at]; [at] = rec;
# [rec] = next; [rec+4] = at; [next+4] = rec. Straight-line (record §C3.1); every store observed by
# the distinct sentinels. The port's effects.c copy carries the row (the actors.c copy is the same
# body, named in §C3.5).
C3_SPECS = [
    Spec("list_insert_after", 0x249B0, [
        # l0: distinct at/rec/next; rec's links and next's back link carry sentinels.
        Case("l0", {"eax": 0x10A200, "edx": 0x10A240},
             {0x10A200: le32(0x10A280), 0x10A250: le32(0x10A260),
              0x10A240: le32(0xA5A5A5A5), 0x10A244: le32(0xA5A5A5A5),
              0x10A284: le32(0xA5A5A5A5)}),
        # l1: at's next is at itself (a one-element ring), so next == at: the four stores land on
        # four distinct words (0x10A200, 0x10A204, 0x10A240, 0x10A244), none overwritten.
        Case("l1", {"eax": 0x10A200, "edx": 0x10A240},
             {0x10A200: le32(0x10A200), 0x10A204: le32(0xA5A5A5A5),
              0x10A240: le32(0x11111111), 0x10A244: le32(0x22222222)}),
        # l2: next == rec (re-inserting a detached node): [rec] is written twice.
        Case("l2", {"eax": 0x10A200, "edx": 0x10A280},
             {0x10A200: le32(0x10A280), 0x10A280: le32(0x33333333), 0x10A284: le32(0x44444444)}),
    ], eax_mask=0, mutants=("@next", "@skip", "@back", "@head")),
    # 0x249D0 (record C3 §C3.2): EAX = `rec`. next = [rec]; prev = [rec+4]; [next+4] = prev;
    # [prev] = next; [rec+4] = 0; [rec] = 0.
    Spec("list_unlink", 0x249D0, [
        # u0: distinct neighbours; every neighbour field seeded differently from what is written.
        Case("u0", {"eax": 0x10A240},
             {0x10A240: le32(0x10A280), 0x10A244: le32(0x10A260),
              0x10A284: le32(0xA5A5A5A5), 0x10A260: le32(0xA5A5A5A5)}),
        # u1: a self-linked rec: next == prev == rec, so [next+4] and [prev] hit rec's own fields
        # and are then zeroed.
        Case("u1", {"eax": 0x10A240},
             {0x10A240: le32(0x10A240), 0x10A244: le32(0x10A240), 0x10A248: le32(0xDEADBEEF)}),
        # u2: next == prev (a two-element ring): [next+4] and [prev] are the same word.
        Case("u2", {"eax": 0x10A240},
             {0x10A240: le32(0x10A280), 0x10A244: le32(0x10A280),
              0x10A284: le32(0x55555555)}),
    ], eax_mask=0, mutants=("@prev", "@link", "@one", "@swap")),
    # 0x164E8 (record C3 §C3.2): EAX = side. One dword store of 0 at 0xFD148 + side*4. The raw
    # leaves EAX = side; every caller ignores it (mask 0).
    Spec("fighter_164e8", 0x164E8, [
        Case("s0", {"eax": 0}, {0x000FD148: le32(0xA5A5A5A5), 0x000FD144: le32(0x12345678)}),
        Case("s1", {"eax": 1}, {0x000FD14C: le32(0xA5A5A5A5), 0x000FD150: le32(0x12345678)}),
        Case("s2", {"eax": 2}, {0x000FD150: le32(0xA5A5A5A5), 0x000FD14C: le32(0x12345678)}),
    ], eax_mask=0, mutants=("@noside", "@byte", "@side")),
    # 0x1D238 (record C3 §C3.2): clear the music pause byte DS_001028DA. `xor ah,ah` (0x1D238); then
    # `mov byte [0x1028DA],ah` (0x1D23A): one byte store of AH, and DS_001028DB is untouched.
    # Callers ignore EAX (mask 0).
    Spec("snd_music_unpause", 0x1D238, [
        Case("p1", {}, {0x001028DA: b"\x01", 0x001028DB: b"\xA5"}),
        Case("p2", {}, {0x001028DA: b"\xA5", 0x001028DB: b"\x00"}),
        Case("p3", {}, {0x001028DA: b"\x00", 0x001028D9: b"\x5A"}),
    ], eax_mask=0, mutants=("@db", "@one", "@word")),
    # 0x1D244 (record C3 §C3.2): clear the sample pause byte DS_001028DB.
    Spec("snd_sample_unpause", 0x1D244, [
        Case("p1", {}, {0x001028DB: b"\x01", 0x001028DA: b"\xA5"}),
        Case("p2", {}, {0x001028DB: b"\xA5", 0x001028DA: b"\x00"}),
        Case("p3", {}, {0x001028DB: b"\x00", 0x001028DC: b"\x5A"}),
    ], eax_mask=0, mutants=("@da", "@one")),
    # 0x1CA14 (record C3 §C3.2): EAX = song, DL = b. Store b at DS_001028D9 and song at
    # DS_001028D4; when not paused (DS_001028DA != 1) and a sequence handle exists (DS_001028C0 !=
    # 0), DS_001028CC = song and AL = 1, else AL = 0. Mask 0xFF (AL).
    Spec("snd_music_request", 0x1CA14, [
        Case("r0", {"eax": 0x2803E640, "edx": 0x12},
             {0x001028D9: b"\xA5", 0x001028D4: le32(0), 0x001028DA: b"\x00",
              0x001028C0: le32(0x11111111), 0x001028CC: le32(0xA5A5A5A5)}),
        Case("r1", {"eax": 0x2803E640, "edx": 0x12},
             {0x001028D9: b"\xA5", 0x001028D4: le32(0), 0x001028DA: b"\x01",
              0x001028C0: le32(0x11111111), 0x001028CC: le32(0xA5A5A5A5)}),
        Case("r2", {"eax": 0x2803E640, "edx": 0x12},
             {0x001028D9: b"\xA5", 0x001028D4: le32(0), 0x001028DA: b"\x00",
              0x001028C0: le32(0), 0x001028CC: le32(0xA5A5A5A5)}),
        Case("r3", {"eax": 0, "edx": 0x1FF},
             {0x001028D9: b"\x5A", 0x001028D4: le32(0xDEADBEEF), 0x001028DA: b"\x00",
              0x001028C0: le32(1), 0x001028CC: le32(0xA5A5A5A5)}),
    ], eax_mask=0xFF, mutants=("@d9", "@pause", "@seq", "@cc", "@al")),
    # 0x2A620 (record C3 §C3.2): EAX = rec, EDX = pset. The mode-1 shear cursor. rec+0x1C == 0 ->
    # v = pset+0x14; else v = [0xF0AEC] + 0x3BC0 - (rec+0x30 >> 16). v >>= 6 (arithmetic); the
    # unsigned word v < [0x107A4C] -> 0xFF; else rec+0x64 = (u8)v - (u8)[0x107A4C]; rec+0x61's top
    # byte >= 0x80 -> 0x7F.
    Spec("mode1_cursor", 0x2A620, [
        # y0: the pset path, v below the threshold.
        Case("y0", {"eax": 0x10A600, "edx": 0x10A700},
             {0x10A61C: le32(0), 0x10A700 + 0x14: le32(0x1000), 0x107A4C: b"\x50\x00",
              0x10A600 + 0x64: b"\x5A", 0x10A661: le32(0)}),
        # y1: v == the threshold: not below (stores 0, no clamp).
        Case("y1", {"eax": 0x10A600, "edx": 0x10A700},
             {0x10A61C: le32(0), 0x10A700 + 0x14: le32(0x1400), 0x107A4C: b"\x50\x00",
              0x10A600 + 0x64: b"\x5A", 0x10A661: le32(0)}),
        # y2: above the threshold, no clamp.
        Case("y2", {"eax": 0x10A600, "edx": 0x10A700},
             {0x10A61C: le32(0), 0x10A700 + 0x14: le32(0x2000), 0x107A4C: b"\x50\x00",
              0x10A600 + 0x64: b"\x5A", 0x10A661: le32(0)}),
        # y3: the y path and the 0x7F clamp (rec+0x61's top byte 0x80).
        Case("y3", {"eax": 0x10A600, "edx": 0x10A700},
             {0x10A61C: le32(1), 0x10A630: le32(0), 0x000F0AEC: le32(0), 0x107A4C: b"\x10\x00",
              0x10A600 + 0x64: b"\x5A", 0x10A661: le32(0x80000000)}),
        # y4: the y path, the (u8) subtraction with a threshold whose low byte is 0.
        Case("y4", {"eax": 0x10A600, "edx": 0x10A700},
             {0x10A61C: le32(1), 0x10A630: le32(0x10000), 0x000F0AEC: le32(0x1000),
              0x107A4C: b"\x00\x01", 0x10A600 + 0x64: b"\x5A", 0x10A661: le32(0)}),
        # y5: a negative v: the arithmetic shift keeps -1 and the unsigned word compare is high.
        Case("y5", {"eax": 0x10A600, "edx": 0x10A700},
             {0x10A61C: le32(1), 0x10A630: le32(0x3BC10000), 0x000F0AEC: le32(0),
              0x107A4C: b"\x10\x00", 0x10A600 + 0x64: b"\x5A", 0x10A661: le32(0)}),
        # y6: the pset path with the clamp.
        Case("y6", {"eax": 0x10A600, "edx": 0x10A700},
             {0x10A61C: le32(0), 0x10A700 + 0x14: le32(0x2000), 0x107A4C: b"\x50\x00",
              0x10A600 + 0x64: b"\x5A", 0x10A661: le32(0xFF000000)}),
    ], eax_mask=0, mutants=("@pset", "@y", "@cmp", "@sub", "@shl"),
       unhit_named={0x2A66D: "the 0x7F clamp store is dead in both: sar edx,0x18 yields "
                            "[-0x80,0x7F], so cmp edx,0x80 / jl at 0x2A665/0x2A66B always takes "
                            "the jump (the port's >= 0x80 is never true; record C3 §C3.5)"}),
    # 0x3C59C (record C3 §C3.2): EAX = bit (AL), EDX = side. Test-and-set bit (bit & 0x1F) of
    # DSD(0x107D50 + side*4): AL = 1 when already set, else the bit is set and AL = 0. Mask 0xFF.
    Spec("fighter_pass_flag", 0x3C59C, [
        Case("f0", {"eax": 0, "edx": 0}, {0x00107D50: le32(0)}),
        Case("f1", {"eax": 0, "edx": 1}, {0x00107D54: le32(1)}),
        Case("f2", {"eax": 0x1F, "edx": 0}, {0x00107D50: le32(0)}),
        Case("f3", {"eax": 0x20, "edx": 0}, {0x00107D50: le32(1)}),
        Case("f4", {"eax": 3, "edx": 2}, {0x00107D58: le32(8)}),
        Case("f5", {"eax": 5, "edx": 0}, {0x00107D50: le32(0x22)}),
        Case("f6", {"eax": 0, "edx": 0}, {0x00107D50: le32(0x80000000)}),
    ], eax_mask=0xFF, mutants=("@eq", "@set", "@side", "@shift")),
    # 0x46460 (record C3 §C3.2): EAX = side, EDX = index (signed). Word of the 0x28-stride ring at
    # 0x108270, `index` steps behind the position DSD(0x1082D2) >> 16 (wrapping modulo 0x14). Mask
    # 0xFFFF: the raw sets AX alone, so the high half is scratch.
    Spec("fighter_input_read", 0x46460, [
        Case("i0", {"eax": 0, "edx": 0}, {0x001082D2: le32(5 << 16),
             0x0010827A: b"\x05\x10", 0x00108278: b"\x04\x10"}),
        Case("i1", {"eax": 0, "edx": 3}, {0x001082D2: le32(5 << 16),
             0x00108274: b"\x02\x10", 0x00108272: b"\x01\x10"}),
        Case("i2", {"eax": 0, "edx": 5}, {0x001082D2: le32(1 << 16),
             0x00108290: b"\x10\x10", 0x00108268: b"\x77\x77"}),
        Case("i3", {"eax": 0, "edx": 0xFFFFFFFF}, {0x001082D2: le32(4 << 16),
             0x00108278: b"\x04\x10"}),
        Case("i4", {"eax": 1, "edx": 1}, {0x001082D2: le32(2 << 16),
             0x0010829A: b"\x01\x20", 0x00108292: b"\x77\x77"}),
        Case("i5", {"eax": 0, "edx": 0x14}, {0x001082D2: le32(0),
             0x00108270: b"\x00\x10", 0x00108296: b"\x77\x77"}),
    ], eax_mask=0xFFFF, mutants=("@side", "@wrap", "@sign", "@pos")),
    # 0x41310 (record C3 §C3.2): EAX = side, EDX = delta (signed). Add delta to the side's
    # camera-target record +0x3C unless [0x104B00] == 3; a negative delta whose sum < 1 clamps the
    # field to 0. Mask 0.
    Spec("fighter_41310", 0x41310, [
        Case("g0", {"eax": 0, "edx": 25},
             {0x001077A8: le32(0x10A600), 0x001077AC: le32(0x10A680),
              0x00104B00: b"\x03\x00", 0x10A600 + 0x3C: le32(100), 0x10A680 + 0x3C: le32(200)}),
        Case("g1", {"eax": 0, "edx": 25},
             {0x001077A8: le32(0x10A600), 0x001077AC: le32(0x10A680),
              0x00104B00: b"\x00\x00", 0x10A600 + 0x3C: le32(100), 0x10A680 + 0x3C: le32(200)}),
        Case("g2", {"eax": 0, "edx": 0},
             {0x001077A8: le32(0x10A600), 0x001077AC: le32(0x10A680),
              0x00104B00: b"\x00\x00", 0x10A600 + 0x3C: le32(100), 0x10A680 + 0x3C: le32(200)}),
        Case("g3", {"eax": 0, "edx": 0xFFFFFFE7},
             {0x001077A8: le32(0x10A600), 0x001077AC: le32(0x10A680),
              0x00104B00: b"\x00\x00", 0x10A600 + 0x3C: le32(10), 0x10A680 + 0x3C: le32(200)}),
        Case("g4", {"eax": 1, "edx": 0xFFFFFFF6},
             {0x001077A8: le32(0x10A600), 0x001077AC: le32(0x10A680),
              0x00104B00: b"\x00\x00", 0x10A600 + 0x3C: le32(100), 0x10A680 + 0x3C: le32(10)}),
        Case("g5", {"eax": 0, "edx": 0xFFFFFFF7},
             {0x001077A8: le32(0x10A600), 0x001077AC: le32(0x10A680),
              0x00104B00: b"\x00\x00", 0x10A600 + 0x3C: le32(10), 0x10A680 + 0x3C: le32(200)}),
        Case("g6", {"eax": 0, "edx": 1},
             {0x001077A8: le32(0x10A600), 0x001077AC: le32(0x10A680),
              0x00104B00: b"\x00\x00", 0x10A600 + 0x3C: le32(0x7FFFFFFF), 0x10A680 + 0x3C: le32(200)}),
    ], eax_mask=0, mutants=("@mode", "@clamp", "@eq", "@add", "@side")),
    # 0x365C8 (record C3 §C3.2): EAX = slot, EDX = rec, EBX = side. 1 when this slot is behind the
    # other's +0x2C in the facing direction and the other slot's +0x43 bit 0x80 is set. Mask 0xFF.
    Spec("fighter_state_365c8", 0x365C8, [
        # s0: the slot's +0x42 bit 0x10.
        Case("s0", {"eax": 0x10A600, "edx": 0x10A700, "ebx": 0},
             {0x10A642: b"\x10", 0x001077A8: le32(0x10A600), 0x001077AC: le32(0x10A680)}),
        # s1: the other slot's record pointer is null.
        Case("s1", {"eax": 0x10A600, "edx": 0x10A700, "ebx": 0},
             {0x10A642: b"\x00", 0x001077A8: le32(0x10A600), 0x001077AC: le32(0)}),
        # s2: the slot's +0x42 bit 0x08.
        Case("s2", {"eax": 0x10A600, "edx": 0x10A700, "ebx": 0},
             {0x10A642: b"\x08", 0x001077A8: le32(0x10A600), 0x001077AC: le32(0x10A680)}),
        # s3: the other's +0x43 bit 0x80 clear.
        Case("s3", {"eax": 0x10A600, "edx": 0x10A700, "ebx": 0},
             {0x10A642: b"\x00", 0x10A6C3: b"\x00", 0x001077A8: le32(0x10A600), 0x001077AC: le32(0x10A680)}),
        # s4: the other's +0x42 bit 0x08.
        Case("s4", {"eax": 0x10A600, "edx": 0x10A700, "ebx": 0},
             {0x10A642: b"\x00", 0x10A6C3: b"\x80", 0x10A6C2: b"\x08",
              0x001077A8: le32(0x10A600), 0x001077AC: le32(0x10A680)}),
        # s5: bit 0x4000 clear, slot+0x2C <= other+0x2C: 1.
        Case("s5", {"eax": 0x10A600, "edx": 0x10A700, "ebx": 0},
             {0x10A642: b"\x00", 0x10A6C3: b"\x80", 0x10A6C2: b"\x00", 0x10A62C: le32(5),
              0x10A6AC: le32(10), 0x10A728: b"\x00\x00", 0x10A600 + 0x43: b"\x00",
              0x001077A8: le32(0x10A600), 0x001077AC: le32(0x10A680)}),
        # s6: the same with slot+0x2C > other+0x2C: 0.
        Case("s6", {"eax": 0x10A600, "edx": 0x10A700, "ebx": 0},
             {0x10A642: b"\x00", 0x10A6C3: b"\x80", 0x10A6C2: b"\x00", 0x10A62C: le32(10),
              0x10A6AC: le32(5), 0x10A728: b"\x00\x00", 0x001077A8: le32(0x10A600), 0x001077AC: le32(0x10A680)}),
        # s7: bit 0x4000 set, other+0x2C < slot+0x2C: 1.
        Case("s7", {"eax": 0x10A600, "edx": 0x10A700, "ebx": 0},
             {0x10A642: b"\x00", 0x10A6C3: b"\x80", 0x10A6C2: b"\x00", 0x10A62C: le32(10),
              0x10A6AC: le32(5), 0x10A728: b"\x00\x40", 0x001077A8: le32(0x10A600), 0x001077AC: le32(0x10A680)}),
        # s8: bit 0x4000 set, other+0x2C >= slot+0x2C: 0.
        Case("s8", {"eax": 0x10A600, "edx": 0x10A700, "ebx": 0},
             {0x10A642: b"\x00", 0x10A6C3: b"\x80", 0x10A6C2: b"\x00", 0x10A62C: le32(5),
              0x10A6AC: le32(10), 0x10A728: b"\x00\x40", 0x001077A8: le32(0x10A600), 0x001077AC: le32(0x10A680)}),
        # s9: the signed <=: -1 <= 1.
        Case("s9", {"eax": 0x10A600, "edx": 0x10A700, "ebx": 0},
             {0x10A642: b"\x00", 0x10A6C3: b"\x80", 0x10A6C2: b"\x00", 0x10A62C: le32(0xFFFFFFFF),
              0x10A6AC: le32(1), 0x10A728: b"\x00\x00", 0x001077A8: le32(0x10A600), 0x001077AC: le32(0x10A680)}),
        # s10: the signed <: -1 < 1.
        Case("s10", {"eax": 0x10A600, "edx": 0x10A700, "ebx": 0},
             {0x10A642: b"\x00", 0x10A6C3: b"\x80", 0x10A6C2: b"\x00", 0x10A62C: le32(1),
              0x10A6AC: le32(0xFFFFFFFF), 0x10A728: b"\x00\x40",
              0x001077A8: le32(0x10A600), 0x001077AC: le32(0x10A680)}),
        # s11: rec+0x28's low byte 0x40: the bit is 0x4000, so the first arm applies (1).
        Case("s11", {"eax": 0x10A600, "edx": 0x10A700, "ebx": 0},
             {0x10A642: b"\x00", 0x10A6C3: b"\x80", 0x10A6C2: b"\x00", 0x10A62C: le32(5),
              0x10A6AC: le32(10), 0x10A728: b"\x40\x00", 0x001077A8: le32(0x10A600), 0x001077AC: le32(0x10A680)}),
        # s12: the other's +0x42 bit 0x80 set but +0x43 clear: 0.
        Case("s12", {"eax": 0x10A600, "edx": 0x10A700, "ebx": 0},
             {0x10A642: b"\x00", 0x10A6C3: b"\x00", 0x10A6C2: b"\x80", 0x10A62C: le32(5),
              0x10A6AC: le32(10), 0x10A728: b"\x00\x00", 0x001077A8: le32(0x10A600), 0x001077AC: le32(0x10A680)}),
    ], eax_mask=0xFF, mutants=("@bit", "@other", "@signed", "@f43")),
    # 0x1A5AC (record C3 §C3.2): EAX = side. 1 when the side's slot record (ctx[4]) +0x28 has bit
    # 0x4000 clear. 0x33950 runs on both sides (allow). Mask 0xFF (the caller's `test al,al`).
    Spec("fighter_1a5ac", 0x1A5AC, [
        Case("a0", {"eax": 0},
             {0x001077B0: le32(0x10A600), 0x001077B0 + 0x94: le32(0x10A680),
              0x10A628: b"\x00\x00", 0x10A680 + 0x28: b"\x00\x40"}),
        Case("a1", {"eax": 0},
             {0x001077B0: le32(0x10A600), 0x001077B0 + 0x94: le32(0x10A680),
              0x10A628: b"\x00\x40", 0x10A680 + 0x28: b"\x00\x00"}),
        Case("a2", {"eax": 1},
             {0x001077B0: le32(0x10A600), 0x001077B0 + 0x94: le32(0x10A680),
              0x10A628: b"\x00\x40", 0x10A680 + 0x28: b"\x00\x00"}),
        Case("a3", {"eax": 0},
             {0x001077B0: le32(0x10A600), 0x001077B0 + 0x94: le32(0x10A680),
              0x10A628: b"\x00\x80", 0x10A680 + 0x28: b"\x00\x40"}),
        Case("a4", {"eax": 0},
             {0x001077B0: le32(0x10A600), 0x001077B0 + 0x94: le32(0x10A680),
              0x10A628: b"\x40\x00", 0x10A680 + 0x28: b"\x00\x40"}),
        Case("a5", {"eax": 0},
             {0x001077B0: le32(0x10A600), 0x001077B0 + 0x94: le32(0x10A680),
              0x10A628: b"\x00\xC0", 0x10A680 + 0x28: b"\x00\x00"}),
    ], allow_calls=(0x33950,), eax_mask=0xFF, mutants=("@slot", "@side", "@bit")),
    # 0x13DF0 (record C3 §C3.2): walk the active list from the sentinel DS_000FCCE0, tear each node
    # down (0x13420, stubbed: its own row is C3b), then zero the active counter and the lock. The
    # port's zero-sentinel guard is a PORT deviation (record §C3.5): the cases keep the head
    # non-zero.
    Spec("effects_clear", 0x13DF0, [
        # e0: the empty list (the sentinel points at itself): no call, both bytes written.
        Case("e0", {}, {0x000FCCE0: le32(0x000FCCE0), 0x0009AF3C: b"\x5A", 0x0009AF3D: b"\xA5"}),
        # e1: one node.
        Case("e1", {}, {0x000FCCE0: le32(0x10A600), 0x10A600: le32(0x000FCCE0),
                        0x0009AF3C: b"\x5A", 0x0009AF3D: b"\xA5"}),
        # e2: three nodes; the walk order is pinned by the recorded calls.
        Case("e2", {}, {0x000FCCE0: le32(0x10A600), 0x10A600: le32(0x10A620),
                        0x10A620: le32(0x10A640), 0x10A640: le32(0x000FCCE0),
                        0x0009AF3C: b"\x5A", 0x0009AF3D: b"\xA5"}),
    ], calls=(E.Call(0x13420, ("eax",)),), eax_mask=0,
       mutants=("@walk", "@count", "@lock", "@set", "@one")),
    # 0x1881C (record C3 §C3.2): latch slot 0 then slot 1 (0x186D0, a stubbed call: its own row
    # pins it), then return slot0+0x30 - slot1+0x30 at 0x1077E0 and 0x1077E0+0x94. The stub writes
    # nothing (record §C3.5), so the difference is read from the seeded memory; the call list pins
    # the order and the two side arguments. Mask full (the caller reads the signed difference).
    Spec("hit_vert_distance", 0x1881C, [
        Case("v0", {}, {0x001077E0: le32(100), 0x00107874: le32(40)}),
        Case("v1", {}, {0x001077E0: le32(0), 0x00107874: le32(0xFFFFFFFF)}),
        Case("v2", {}, {0x001077E0: le32(0x80000000), 0x00107874: le32(1)}),
        Case("v3", {}, {0x001077E0: le32(0), 0x00107874: le32(0)}),
    ], calls=(E.Call(0x186D0, ("eax",)),), eax_mask=0xFFFFFFFF,
       mutants=("@one", "@order", "@side", "@diff")),
    # 0x36CE4 (record C3 §C3.2): EAX = slot. Set slot+0x43 bit 2; in modes other than 3/0x22
    # restart the side's DS_00102900 record on the 0xE906E stream at 3.0 (0x2BC30, a stubbed call
    # with its own row). Mask 0.
    Spec("fighter_36ce4", 0x36CE4, [
        # c0: mode 3: only the bit is set.
        Case("c0", {"eax": 0x10A600}, {0x10A600: le32(0x10A700), 0x10A643: b"\x00",
             0x00104B00: b"\x03\x00", 0x10A751: b"\x00", 0x00102900: le32(0x10A700),
             0x00102904: le32(0x10A780)}),
        # c1: mode 0x22: only the bit.
        Case("c1", {"eax": 0x10A600}, {0x10A600: le32(0x10A700), 0x10A643: b"\x00",
             0x00104B00: b"\x22\x00", 0x10A751: b"\x00", 0x00102900: le32(0x10A700),
             0x00102904: le32(0x10A780)}),
        # c2: mode 0, side 0: the animation call.
        Case("c2", {"eax": 0x10A600}, {0x10A600: le32(0x10A700), 0x10A643: b"\x00",
             0x00104B00: b"\x00\x00", 0x10A750: b"\x01", 0x10A751: b"\x00",
             0x00102900: le32(0x10A700), 0x00102904: le32(0x10A780)}),
        # c3: mode 0, side 1: the other record; rec+0x50 differs from rec+0x51.
        Case("c3", {"eax": 0x10A600}, {0x10A600: le32(0x10A700), 0x10A643: b"\x00",
             0x00104B00: b"\x00\x00", 0x10A750: b"\x00", 0x10A751: b"\x01",
             0x00102900: le32(0x10A700), 0x00102904: le32(0x10A780)}),
    ], calls=(ANIM_BEGIN,), eax_mask=0,
       mutants=("@bit", "@mode", "@mode22", "@rec", "@stream", "@side")),
    # 0x2AC80 (record C3 §C3.2): EAX = flag. When the free list is not the empty sentinel, pop its
    # head (0x249D0, a real call: its own row proves it) and insert it at the active list's head
    # (0x249B0 real) or, when the flag's 0x400 bit (CH bit 2) is set, at the tail (0x249C0, not in
    # the call set: it has no row, and both bounds read its final bytes). The two 0x2EA30
    # interrupt-lock calls run on the original side only (allow, record §C3.5); with the lock byte
    # zero their net write is zero, so the memory at the recorded calls agrees.
    Spec("actor_alloc", 0x2AC80, [
        Case("a0", {"eax": 0}, {0x000BCD60: b"\x00", 0x00105B3C: le32(0x10A600), 0x10A600: le32(0x00105B3C),
             0x10A604: le32(0x00105B3C), 0x00105B40: le32(0x5A5A5A5A),
             0x00105BCC: le32(0x00105BCC), 0x00105BD0: le32(0x00105BCC)}),
        Case("a1", {"eax": 0x400}, {0x000BCD60: b"\x00", 0x00105B3C: le32(0x10A600), 0x10A600: le32(0x00105B3C),
             0x10A604: le32(0x00105B3C), 0x00105B40: le32(0x5A5A5A5A),
             0x00105BCC: le32(0x00105BCC), 0x00105BD0: le32(0x00105BCC)}),
        Case("a2", {"eax": 0}, {0x000BCD60: b"\x00", 0x00105B3C: le32(0x00105B3C), 0x00105BCC: le32(0x00105BCC),
             0x00105BD0: le32(0x12345678)}),
        Case("a3", {"eax": 0xFFFFFFFF}, {0x000BCD60: b"\x00", 0x00105B3C: le32(0x10A600), 0x10A600: le32(0x00105B3C),
             0x10A604: le32(0x00105B3C), 0x00105B40: le32(0x5A5A5A5A),
             0x00105BCC: le32(0x00105BCC), 0x00105BD0: le32(0x00105BCC)}),
        Case("a4", {"eax": 1}, {0x000BCD60: b"\x00", 0x00105B3C: le32(0x10A600), 0x10A600: le32(0x00105B3C),
             0x10A604: le32(0x00105B3C), 0x00105B40: le32(0x5A5A5A5A),
             0x00105BCC: le32(0x00105BCC), 0x00105BD0: le32(0x00105BCC)}),
    ], allow_calls=(0x2EA30, 0x249C0),
       calls=(E.Call(0x249D0, ("eax",), mode="real"),
              E.Call(0x249B0, ("eax", "edx"), mode="real")),
       eax_mask=0xFFFFFFFF, mutants=("@flag", "@unlink", "@tail", "@empty", "@ret")),
]

# ---- track P batch C3b (record 2026-10-05-reverse-c3b): the frontier rows, part 2 ----------------
#
# 0x29F34 anim_read_var (record §C3b.1): EAX = rec, EDX = op; the low word selects (`xor dh,dh` /
# `and dl,0x7f` at 0x29F38/0x29F3C). o = op & 0x7F: < 0x40 the ring word at 0x105B4C indexed by
# (o + rec+0x51) & 0x3F; 0x40..0x45 the record's own fields (0x40..0x43 and 0x45 sign-extended
# bytes, 0x44 the +0x56 word); 0x46..0x4B / 0x4C..0x51 the same fields on the record at
# [0x1014F4] + rec[0x4A]/rec[0x4B] * 0x68; above 0x51 zero. The ring/record arms zero EAX first
# (0x29F3A/0x29F63/0x29F73 `xor eax,eax`); the pool arms write only AX (`66 0f be`/`66 8b`,
# 0x29FED..0x2A014) on the full 32-bit base built at 0x29FBC..0x29FD8, so a pool arm returns EAX
# with the base's high word above the 16-bit field. The callers read 16 bits — `and eax,0xffff` at
# 0x2A49C; 0x2A480 `add ecx,eax`, whose low word alone survives (0x2A4C4 `mov eax,ecx` /
# 0x2A4D1 `and eax,0xffff`); the `mov ax,cx` returns 0x2A4E4/0x2A4F1; 0x2AAD1 `test ax,ax` — so
# eax_mask=0xFFFF is load-bearing: a full mask compares the base's high word (a8: 0x100033 vs 0x33).
C3B_A_REC = 0x10A800          # C3b zero BSS: 0x29F34's record
C3B_A_ACT = 0x10A900          # its parent/child actor pair (index 0 / 1 at +0x68)
C3B_A_RING = 0x105B4C         # DS_00105B4C: the 0x40-word ring

def c3b_a_pokes(idx=0):
    return {C3B_A_REC + 0x51: bytes([idx]),
            C3B_A_REC + 0x52: b"\x81", C3B_A_REC + 0x53: b"\x7f",
            C3B_A_REC + 0x54: b"\x80", C3B_A_REC + 0x55: b"\xff",
            C3B_A_REC + 0x56: b"\xef\xbe", C3B_A_REC + 0x58: b"\x80",
            C3B_A_REC + 0x4a: b"\x00", C3B_A_REC + 0x4b: b"\x01",
            0x001014F4: le32(C3B_A_ACT),
            C3B_A_RING: b"\x11\x11", C3B_A_RING + 2: b"\x22\x22",
            C3B_A_RING + 0x80: b"\x77\x77"}

# 0x18350 fighter_18350 (record §C3b.1): EAX = side, EDX = anchor. ch = 0x1077B0[side*0x94]+0x7A
# selects the per-character table (0x18334; >6 0xCEB00), p = table + anchor*2 (the u32 wrap the
# port keeps), x/y = sx(p[0])/sx(p[1]), the x negated when 0x1A570 reports the actor's bit 15 set,
# both <<6 into 0x100AB0/AB4[side*8]. Callers ignore EAX (mask 0).
C3B_B_REC = 0x10AB00          # the side's record for 0x1A570
C3B_B_ACT = 0x10AB40          # its actor table ([0x1014EC])
C3B_B_TABLES = {0: 0xCEB00, 1: 0xCF399, 2: 0xCFC32, 3: 0xD033B, 4: 0xD0A44, 5: 0xCEB00,
                6: 0xCF399}

def c3b_b_case(side, ch, anchor, tbl, tblbytes, idx=0, actor=0x0000, actor0=0x0000):
    slot = 0x1077B0 + side * 0x94
    out = 0x100AB0 + side * 8
    p = {slot + 0x7A: bytes([ch]), slot: le32(C3B_B_REC),
         C3B_B_REC + 0x56: (idx & 0xFFFF).to_bytes(2, "little"),
         0x001014EC: le32(C3B_B_ACT), C3B_B_ACT: (actor0 & 0xFFFF).to_bytes(2, "little"),
         C3B_B_ACT + idx * 0x20: (actor & 0xFFFF).to_bytes(2, "little"),
         out: le32(0xA5A5A5A5), out + 4: le32(0x5A5A5A5A)}
    p[tbl + anchor * 2] = tblbytes
    return p

# 0x18540 fighter_18540 (record §C3b.1): EAX = side. slot = 0x1077A8[side]; null returns. ch =
# slot+0x7A selects the per-character camera word (0x18524; ch 0 and >6 the 0xE6DD0 default), then
# 0x18460 runs (allow: effect-free, its 0x18428 dispatch is a bare RET per character). sprite =
# (word at [0x1014EC] + word[DSD(slot)+0x56] * 0x20) & 0x7FFF; anchor = sprite - cam into
# 0x100AF0[side], zeroed when negative or at/over 0xE6DB4[ch]. Callers ignore EAX (mask 0).
C3B_C_SLOT = 0x10AC00
C3B_C_REC = 0x10AC40
C3B_C_ACT = 0x10AC80

def c3b_c_case(side, ch, cam, limit, sprite, idx=0, live=True, extra=None):
    slot = 0x1077B0 + side * 0x94
    p = {0x100AF0 + side * 4: le32(0xA5A5A5A5),
         0xE6DB4 + ch * 4: le32(limit)}
    if live:
        p.update({0x1077A8 + side * 4: le32(slot),
                  slot + 0x7A: bytes([ch]),
                  slot: le32(C3B_C_REC),
                  C3B_C_REC + 0x56: (idx & 0xFFFF).to_bytes(2, "little"),
                  0x001014EC: le32(C3B_C_ACT),
                  C3B_C_ACT: (0x0000 if idx else sprite & 0xFFFF).to_bytes(2, "little"),
                  C3B_C_ACT + idx * 0x20: (sprite & 0xFFFF).to_bytes(2, "little")})
    else:
        p[0x1077A8 + side * 4] = le32(0)
    cams = {1: 0xE39D0, 2: 0xECBD8, 3: 0xD2134, 4: 0xEA604, 5: 0xD3E08, 6: 0xE061C}
    p[cams.get(ch, 0xE6DD0)] = (cam & 0xFFFF).to_bytes(2, "little")
    if extra:
        p.update(extra)
    return p

# 0x18788 hit_record_y (record §C3b.1): EAX = side; slot+0x42 bit 3 takes the record's +0x1C;
# otherwise 0x18540 re-latches, 0x18350 re-runs when the anchor differs from slot+0x20, and the
# result is slot+0x30 - 0x100AB4[side]. Full mask (the only caller 0x1883C reads EAX).
C3B_Y_REC = 0x10AD00

def c3b_y_pokes(side, bit42, anchor=None, slot30=0, ab4=0, cam=0x0100, sprite=0x0140, extra=None):
    slot = 0x1077B0 + side * 0x94
    p = {slot: le32(C3B_Y_REC), slot + 0x42: bytes([bit42]),
         C3B_Y_REC + 0x1C: le32(0x11223344), C3B_Y_REC + 0x18: le32(0x55667788),
         0x100AB4 + side * 8: le32(ab4)}
    if anchor is not None:
        p.update({0x1077A8 + side * 4: le32(slot), slot + 0x7A: b"\x00",
                  C3B_Y_REC + 0x56: b"\x00\x00",
                  0x001014EC: le32(C3B_C_ACT), C3B_C_ACT: (sprite & 0xFFFF).to_bytes(2, "little"),
                  slot + 0x20: le32(anchor), slot + 0x30: le32(slot30),
                  0xE6DD0: (cam & 0xFFFF).to_bytes(2, "little")})
    if extra:
        p.update(extra)
    return p

C3B_D_REC = 0x10AE00

# The voice rows' state (record C3b §C3b.1): the port's DS_ symbols, not in symbols.h's Python side.
DS_001028C0, DS_001028C8 = 0x001028C0, 0x001028C8
DS_001028CC, DS_001028D4, DS_001028D9 = 0x001028CC, 0x001028D4, 0x001028D9
DS_001028DB = 0x001028DB


def c3b_v_case(driver, cur, queued=None, extra=None):
    p = {DS_001028C8: le32(driver)}
    for k in range(4):
        p[0x00102860 + k * 0x18] = le32(0x10C000 + k * 0x18)
        p[0x0010286C + k * 0x18] = le32(cur[k])
        if queued is not None:
            p[0x00102864 + k * 0x18] = le32(queued)
    if extra:
        p.update(extra)
    return p


def c3b_d_case(side, bd, s884, p, x, bit14, slot53=1, slot54=0, mode4b=0, live=True, recnull=False):
    slot = 0x1077B0 + side * 0x94
    p2 = {0x1077A8 + side * 4: le32(slot), slot: le32(0 if recnull else C3B_D_REC),
          slot + 0x7A: b"\x00",
          C3B_D_REC + 0x3C: le32(p), C3B_D_REC + 0x18: le32(x),
          C3B_D_REC + 0x28: (0x4000 if bit14 else 0).to_bytes(2, "little"),
          0x1088BD: bytes([bd]), 0x108884: le32(s884), 0x104B00: le16(mode4b),
          slot + 0x53: bytes([slot53]), slot + 0x54: bytes([slot54]),
          # +0x43 bit 6 clear (distinct nonzero neighbours 0x40..0x42): the 0x38221 OR 0x40
          # must change the byte, or a dropped store at 0x38214..0x38221 passes (C3b sweep).
          slot + 0x40: b"\x40\x41\x42\x00", 0x1078F0 + side: b"\xA5",
          0x10AE4C: b"\xAA\xBB", slot + 0x55: b"\xCC", slot + 0x5F: b"\xDD"}
    if not live:
        p2[0x1077A8 + side * 4] = le32(0)
    return p2

def c3b_q_slots():
    out = bytearray()
    for k in range(4):
        out += le32(0x10C000 + k * 0x18) + le32(0x11111111) + b"\x22" + le32(0x99999999) \
             + le32(0x33333333) + le32(0x44444444)
    return bytes(out)

C3B_Q_T = 0x00102874
C3B_Q_BUF = 0x00102870
C3B_Q_QUEUED = 0x00102864

# 0x13420 effect_teardown (record §C3b.1): EAX = rec. The interrupt lock DS_0009AF3C is set to 1,
# 0x249D0 unlinks rec from the active list, the lock is restored, then the record's type
# (rec+0xC, an unsigned byte: `mov al`/`cmp al,5`/`ja`) dispatches through the table 0x13408:
# type 1 -> palette_record(rec+0x10, DSD(src+8)+DSB(rec+0xF), 1, 0); type 4 -> palette_record(
# 0xFCCF0, DSD(src+8), DSD(src+0xC), 0); types 0/2/3/5 -> palette_record_flagged(DSD(src),
# DSD(src+8), DSD(src+0xC)); anything else (6..0xFF) -> nothing. Then 0x249B0 inserts rec after
# the free-list sentinel DS_000FCCE8. The two palette writers and the two list calls run as
# allows (the effects.c copies carry no seam: C3's actor_alloc mutant route depends on their
# calls staying unrecorded; the final bytes are the comparison). Mask 0.
C3B_E_REC = 0x10A800
C3B_E_SRC = 0x10A900
C3B_E_SENTINEL = 0x000FCCE8


def c3b_e_pokes(typ, extra=None):
    # rec, the sentinel and the two neighbour nodes A/B are interlinked so the unlink and the
    # insert-after both move bytes (a self-linked sentinel would make them idempotent and the
    # @unlink/@tail mutants unobservable).
    p = {C3B_E_REC: le32(0x10AC00) + le32(0x10AC40) + le32(C3B_E_SRC),
         C3B_E_REC + 0xC: bytes([typ, 0x00, 0x00, 0x44]) + b"\x77\x77\x77\x77",
         C3B_E_SRC: le32(0x11111111) + b"\x00" * 4 + le32(0x22222222) + le32(0x33333333),
         C3B_E_SENTINEL: le32(0x10AC00) + le32(0x10AC40),
         0x10AC00: le32(0x10AC40) + le32(C3B_E_SENTINEL),
         0x10AC40: le32(C3B_E_SENTINEL) + le32(0x10AC00),
         0x0009AF3C: b"\x5A", 0x00107798: le32(0x00107498)}
    if extra:
        p.update(extra)
    return p

# 0x49444 actor_type_49444 (record §C3b.1): EAX = rec. node = DSD(rec+0x14); null returns.
# node+0x1C bit 1 (loaded as a word but only AL bit 1 read: `mov ax,[..]; xor ah,ah; and al,2`)
# clears the entry DSD(0x10839C + (signed DSD(node+0x18)>>16)*4); a non-zero node+0x10 is
# retired with 0x2B150 (stubbed: its own row proves it) and zeroed; then 0x249D0 unlinks the
# node and 0x249B0 re-inserts it after the 0x1083C4 sentinel, and rec+0x14 is zeroed. Mask 0.
C3B_F_REC = 0x10A800
C3B_F_NODE = 0x10A900
C3B_F_SENTINEL = 0x001083C4


def c3b_f_pokes(q=0x00030000, flags=2, child=0, rec14=None, extra=None):
    p = {C3B_F_REC + 0x14: le32(C3B_F_NODE if rec14 is None else rec14),
         C3B_F_NODE: le32(C3B_F_SENTINEL), C3B_F_NODE + 4: le32(C3B_F_SENTINEL),
         C3B_F_NODE + 0x10: le32(child), C3B_F_NODE + 0x18: le32(q),
         C3B_F_NODE + 0x1C: le16(flags),
         C3B_F_SENTINEL: le32(C3B_F_SENTINEL), C3B_F_SENTINEL + 4: le32(C3B_F_SENTINEL),
         0x0010839C: le32(0xDEADBEEF), 0x001083A0: le32(0xDEADBEEF),
         0x001083A4: le32(0xDEADBEEF), 0x001083A8: le32(0xDEADBEEF),
         0x00108398: le32(0xDEADBEEF)}
    if extra:
        p.update(extra)
    return p

# 0x2B150 set_dead (record §C3b.1): EAX = rec. rec+0x28 |= 8 (a byte store); when rec+0x2b
# bit 6 is set, the type callback DSD(0xBB9E0 + DSB(rec+0x48)*0xC) is called with EAX = rec
# (an indirect call: the 0x5D812 entries are the `xor eax,eax; ret` stub, the others are the
# registered port callbacks) and rec+0x2b bit 6 is cleared; then pset = DSD(0x1014EC) +
# DSW(rec+0x56)*0x20, and when pset+0x18 is non-zero 0x33864 releases it and it is zeroed;
# finally 0x1C458 finds the render-list node whose +4 is the pset and 0x1C3D0 unlinks it to
# the free head (the port runs both as one render_list_remove). The palette release is
# stubbed; the render pair and the callback targets are allowed. Mask 0.
C3B_S_REC = 0x10A068           # in-pool at DS_001014F4 = 0x10A000, stride 0x68
C3B_S_POOL = 0x10A000
C3B_S_PSET = 0x10A800          # DS_001014EC: the pset table, stride 0x20
C3B_S_NODE = 0x10A900          # the type callback's rec+0x14 node
C3B_S_LAND = 0x000F0A78        # 0x12800's destination sentinel
C3B_S_RNODE = 0x0010153C       # the first render-pool node
C3B_S_RFREE = 0x0010275C       # the render free-list head
C3B_S_RHEAD = 0x00105B44       # the render list head


# 0x29DB8 anim_write_var (record §C3b.1): EAX = rec, EDX = op, EBX = value. The pool record the
# parent/child forms write is DS_001014F4 + DSB(rec+0x4A|0x4B) * 0x68.
C3B_W_REC = 0x10A800
C3B_W_POOL = 0x10A000
C3B_W_RING = 0x00105B4C
C3B_W_BE8 = 0x00105BE8


def c3b_w_pokes(i51=0, i4a=0, i4b=1, be8=0x5A, ring=None, extra=None):
    p = {0x001014F4: le32(C3B_W_POOL),
         C3B_W_REC + 0x4A: bytes([i4a, i4b]),
         C3B_W_REC + 0x51: bytes([i51, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27]),
         C3B_W_BE8: le16(be8),
         C3B_W_POOL + 0x52: b"\xA5" * 8,
         C3B_W_POOL + 0x68 + 0x52: b"\xB5" * 8}
    for i, b in (ring or {}).items():
        p[C3B_W_RING + i * 2] = b
    if extra:
        p.update(extra)
    return p


# 0x2B8F8 anim_operand (record §C3b.1): EAX = rec; the stream pointer DSD(rec+8) walks the
# image words at C3B_O_P, the byte/deref forms index from DSD(rec+0x0C) = C3B_O_BASE.
C3B_O_REC = 0x10A800
C3B_O_P = 0x10B000
C3B_O_BASE = 0x10A900


def c3b_o_pokes(op=0, cw=0, w2=0, w4=0, extra=None):
    p = {C3B_O_REC + 8: le32(C3B_O_P), C3B_O_REC + 0x0C: le32(C3B_O_BASE),
         C3B_O_P: le16(cw), C3B_O_P + 2: le16(w2), C3B_O_P + 4: le16(w4),
         0x00105BE4: le16(op), 0x00105BE6: le16(0), 0x00105BE8: le16(0xA5A5),
         0x00105BD4: le32(0xA5A5A5A5),
         C3B_O_BASE: bytes([0x00, 0xFF, 0xEE, 0x42, 0x7F, 0x5A, 0x00, 0x5A]),
         C3B_O_BASE + 8: le16(0x5A5A), C3B_O_BASE + 0x0A: le16(0x6B6B)}
    if extra:
        p.update(extra)
    return p


# 0x3AFC4 fighter_anim_triple (record §C3b.1): EAX = slot_char, EDX = the frame byte, EBX = the
# 3-dword output. The per-character byte at DS_0010782A + slot_char*0x94 is shifted left 6 and
# added to EDX, giving c; the output is 0xDE114 + c*11, 0xA3528 + c*20 and 0xA6728 + c*6. EDX
# outside [0,0x40) is the raw's 0x62003 fatal (the port zeroes the triple: the named gap).
C3B_T_OUT = 0x10A800
C3B_T_TBL = 0x0010782A


def c3b_t_pokes(sc=0, edx=0, tbl=None, extra=None):
    p = {C3B_T_OUT: b"\xA5" * 12}
    if tbl is not None:
        p[C3B_T_TBL + sc * 0x94] = bytes([tbl])
    if extra:
        p.update(extra)
    return p


def c3b_s_pokes(typ=0, cb=False, pset18=0, idx=0, node=0, rnode=False, extra=None):
    p = {0x001014F4: le32(C3B_S_POOL), 0x001014EC: le32(C3B_S_PSET),
         C3B_S_REC + 0x14: le32(node),
         C3B_S_REC + 0x28: b"\xA5", C3B_S_REC + 0x2A: b"\x00",
         C3B_S_REC + 0x2B: (b"\x40" if cb else b"\x00"),
         C3B_S_REC + 0x48: bytes([typ]), C3B_S_REC + 0x56: le16(idx),
         C3B_S_PSET + idx * 0x20 + 0x18: le32(pset18),
         C3B_S_RHEAD: le32(0), C3B_S_RFREE: le32(0),
         C3B_S_LAND: le32(C3B_S_LAND), C3B_S_LAND + 4: le32(C3B_S_LAND)}
    if rnode:
        p[C3B_S_RHEAD] = le32(C3B_S_RNODE)
        p[C3B_S_RNODE] = le32(0)
        p[C3B_S_RNODE + 4] = le32(C3B_S_PSET + idx * 0x20)
    if node:
        p[C3B_S_NODE] = le32(C3B_S_LAND)
        p[C3B_S_NODE + 4] = le32(C3B_S_LAND)
    if extra:
        p.update(extra)
    return p

def c3b_q_case(driver=1, db=0, size=0x6001, overrides=None):
    slots = c3b_q_slots()
    p = {0x00102860: slots[:0x30], 0x00102890: slots[0x30:],
         DS_001028C8: le32(driver), DS_001028DB: bytes([db]), 0x00101500: le32(0x1000)}
    p.update(c2b_res(0x800001, size))
    p.update(overrides or {})
    return p


C3B_SPECS = [
    Spec("anim_read_var", 0x29F34, [
        Case("a0", {"eax": C3B_A_REC, "edx": 0x00}, c3b_a_pokes(0)),          # ring[0]
        Case("a1", {"eax": C3B_A_REC, "edx": 0x3F}, c3b_a_pokes(1)),          # (0x3F+1)&0x3F = 0
        Case("a2", {"eax": C3B_A_REC, "edx": 0x40}, c3b_a_pokes()),           # sx(+0x52) = -0x7F
        Case("a3", {"eax": C3B_A_REC, "edx": 0x41}, c3b_a_pokes()),           # sx(+0x53) = +0x7F
        Case("a4", {"eax": C3B_A_REC, "edx": 0x42}, c3b_a_pokes()),           # sx(+0x54)
        Case("a5", {"eax": C3B_A_REC, "edx": 0x43}, c3b_a_pokes()),           # sx(+0x55)
        Case("a6", {"eax": C3B_A_REC, "edx": 0x44}, c3b_a_pokes()),           # word +0x56
        Case("a7", {"eax": C3B_A_REC, "edx": 0x45}, c3b_a_pokes()),           # sx(+0x58)
        Case("a8", {"eax": C3B_A_REC, "edx": 0x46},
             {**c3b_a_pokes(), C3B_A_ACT + 0x52: b"\x33"}),                   # parent +0x52
        Case("a9", {"eax": C3B_A_REC, "edx": 0x4B},
             {**c3b_a_pokes(), C3B_A_ACT + 0x58: b"\x42"}),                   # parent +0x58
        Case("a10", {"eax": C3B_A_REC, "edx": 0x4C},
             {**c3b_a_pokes(), C3B_A_ACT + 0x68 + 0x52: b"\x55"}),            # child +0x52
        Case("a11", {"eax": C3B_A_REC, "edx": 0x51},
             {**c3b_a_pokes(), C3B_A_ACT + 0x68 + 0x58: b"\x61"}),            # child +0x58
        Case("a12", {"eax": C3B_A_REC, "edx": 0x52}, c3b_a_pokes()),          # above 0x51: 0
        Case("a13", {"eax": C3B_A_REC, "edx": 0xFF40}, c3b_a_pokes()),        # dx = 0x40
        Case("a14", {"eax": C3B_A_REC, "edx": 0x80}, c3b_a_pokes(0)),         # dl&0x7F = 0
        Case("a15", {"eax": C3B_A_REC, "edx": 0x3F}, c3b_a_pokes(0x80)),      # ring index 0x3F
        Case("a16", {"eax": C3B_A_REC, "edx": 0x49},
             {**c3b_a_pokes(), C3B_A_ACT + 0x55: b"\x66"}),                   # parent +0x55
        Case("a17", {"eax": C3B_A_REC, "edx": 0x4A},
             {**c3b_a_pokes(), C3B_A_ACT + 0x56: b"\x77\x76"}),               # parent word
        Case("a18", {"eax": C3B_A_REC, "edx": 0x48},
             {**c3b_a_pokes(), C3B_A_ACT + 0x54: b"\x88"}),                   # parent +0x54
        Case("a19", {"eax": C3B_A_REC, "edx": 0x47},
             {**c3b_a_pokes(), C3B_A_ACT + 0x53: b"\x99"}),                   # parent +0x53
    ], eax_mask=0xFFFF, mutants=("@ring", "@sx", "@pswap", "@cswap", "@w58", "@a45", "@ret", "@opff")),
    Spec("fighter_18350", 0x18350, [
        Case("b0", {"eax": 0, "edx": 0}, c3b_b_case(0, 0, 0, 0xCEB00, b"\x01\x02")),
        Case("b1", {"eax": 1, "edx": 0}, c3b_b_case(1, 1, 0, 0xCF399, b"\x03\x04", 1, 0x8000)),
        Case("b2", {"eax": 0, "edx": 0}, c3b_b_case(0, 2, 0, 0xCFC32, b"\x05\x06")),
        Case("b3", {"eax": 0, "edx": 1}, c3b_b_case(0, 3, 1, 0xD033B, b"\x7f\x80")),
        Case("b4", {"eax": 0, "edx": 0}, c3b_b_case(0, 4, 0, 0xD0A44, b"\x09\x0a")),
        Case("b5", {"eax": 1, "edx": 0}, c3b_b_case(1, 5, 0, 0xCEB00, b"\x01\x02")),
        Case("b6", {"eax": 0, "edx": 0}, c3b_b_case(0, 6, 0, 0xCF399, b"\x03\x04")),
        Case("b7", {"eax": 0, "edx": 0}, c3b_b_case(0, 7, 0, 0xCEB00, b"\x01\x02")),
        Case("b8", {"eax": 1, "edx": 0}, c3b_b_case(1, 0x20, 0, 0xCEB00, b"\x01\x02")),
        Case("b9", {"eax": 0, "edx": 0}, c3b_b_case(0, 0, 0, 0xCEB00, b"\x01\x02", 0, 0x8000)),
    ], calls=(E.Call(0x1A570, ("eax",), mode="real"),), eax_mask=0,
       mutants=("@tab", "@anchor", "@neg", "@bit", "@side")),
    Spec("fighter_18540", 0x18540, [
        Case("c0", {"eax": 0}, c3b_c_case(0, 0, 0x0100, 0x0FFF, 0x1234)),
        Case("c1", {"eax": 1}, c3b_c_case(1, 1, 0x0200, 0xFFFF, 0x1000)),
        Case("c2", {"eax": 0}, c3b_c_case(0, 2, 0x0300, 0x0FFF, 0x1000)),
        Case("c3", {"eax": 0}, c3b_c_case(0, 3, 0x0400, 0x0FFF, 0x1000)),
        Case("c4", {"eax": 0}, c3b_c_case(0, 4, 0x0500, 0x0FFF, 0x1000)),
        Case("c5", {"eax": 0}, c3b_c_case(0, 5, 0x0600, 0x0FFF, 0x1000)),
        Case("c6", {"eax": 0}, c3b_c_case(0, 6, 0x0700, 0x0FFF, 0x1000)),
        Case("c7", {"eax": 0}, c3b_c_case(0, 7, 0x0800, 0x0FFF, 0x1000)),
        Case("c8", {"eax": 0}, c3b_c_case(0, 0, 0x0200, 0x0FFF, 0x0100)),    # negative: 0
        Case("c9", {"eax": 0}, c3b_c_case(0, 0, 0x0100, 0x2000, 0x2100)),    # == limit: 0
        Case("c10", {"eax": 0}, c3b_c_case(0, 0, 0x0100, 0x2000, 0x20FF)),   # limit-1 kept
        Case("c11", {"eax": 0}, c3b_c_case(0, 0, 0, 0, 0, live=False)),      # slot null: no write
        Case("c12", {"eax": 0}, c3b_c_case(0, 0, 0x0000, 0x0FFF, 0x8010, 1)),  # mask 0x7FFF, *0x20
    ], allow_calls=(0x18460, 0x18428), eax_mask=0,
       mutants=("@default", "@noclamp", "@gt", "@mask", "@idx", "@side")),
    Spec("hit_record_y", 0x18788, [
        Case("y0", {"eax": 0}, c3b_y_pokes(0, 0x08)),
        Case("y1", {"eax": 0}, c3b_y_pokes(0, 0x00, anchor=0x40, slot30=0x00020010,
                                           ab4=0x00010000)),
        Case("y2", {"eax": 0}, c3b_y_pokes(0, 0x00, anchor=0x40, slot30=0x00020000,
                                           ab4=0x00010000,
                                           extra={0xCEB80: b"\x01\x02", 0x1077B0 + 0x20: le32(0x41)})),
        Case("y3", {"eax": 1}, c3b_y_pokes(1, 0x00, anchor=0x40, slot30=0x00030020,
                                           ab4=0x00020000)),
    ], allow_calls=(0x18460, 0x18428),
       calls=(E.Call(0x18540, ("eax",), mode="real"),
              E.Call(0x18350, ("eax", "edx"), mode="real"),
              E.Call(0x1A570, ("eax",), mode="real")),
       eax_mask=0xFFFFFFFF, mutants=("@bit", "@rec", "@call", "@always", "@add")),
    # 0x38154 fighter_38154 (record §C3b.1): EAX = side. slot = DSD(0x1077A8[side]), rec = DSD(slot);
    # either null returns. v: DS_001088BD == 8 && side == 1 gives 0x1500 - rec+0x3C when rec+0x3C is
    # in [0, 0x5D00], otherwise DS_00108884 - 0x1500 - rec+0x18; every other side gives
    # (0x4900 - side*0x200) - rec+0x3C in range, otherwise (0x1F00 - side*0x200) + DS_00108884 -
    # rec+0x18. With rec+0x28 bit 14: |v| > 0x200 and v >= 0 runs 0x35838(slot, rec, 0x1000), flag 0;
    # else slot+0x43 |= 0x40 and, when the zero-extended DS_001088BD >= 8, flag 1, else
    # 0x36638(slot, rec) and flag 0. Without the bit: |v| <= 0x200 gives flag 1, else
    # 0x35838(slot, rec, v < 0 ? 0x2000 : 0x1000) and flag 0. The flag is DS_001078F0[side]; with it
    # set and slot+0x53 != 0, 0x367DC(slot, rec) (allow) runs and slot+0x52 = 9; with it clear,
    # slot+0x53 = 0xC. The 0x36638 and 0x35838 paths are declared real; their own anim calls are the
    # ANIM_BEGIN stub; slot+0x54 = 5 is never used (its 0x36638 arm recurses into 0x38154). Mask 0.
    Spec("fighter_38154", 0x38154, [
        Case("d0", {"eax": 0}, c3b_d_case(0, 8, 0, 0x1000, 0, 0)),        # V3 far v>=0: 0x35838 0x1000
        Case("d1", {"eax": 0}, c3b_d_case(0, 8, 0, 0x1000, 0, 1, 0, 4)),  # bit14 far v>=0: 0x35838
        Case("d2", {"eax": 0}, c3b_d_case(0, 8, 0, 0x5000, 0, 1, 0, 4)),  # bit14 far v<0: 0x35838
        Case("d3", {"eax": 0}, c3b_d_case(0, 8, 0, 0x4800, 0, 1)),        # bit14 near, bd>=8: flag 1
        Case("d4", {"eax": 0}, c3b_d_case(0, 0, 0, 0x4800, 0, 1, 0, 1)),  # bit14 near, bd<8: 0x36638
        Case("d5", {"eax": 1}, c3b_d_case(1, 8, 0, 0x4700, 0, 1)),        # side1 V3, near: flag 1
        Case("d6", {"eax": 0}, c3b_d_case(0, 0, 0, 0x4800, 0, 0, 0)),     # near, slot53 0: no 0x367DC
        Case("d7", {"eax": 0}, c3b_d_case(0, 8, 0x2000, 0x8000, 0x0B00, 0)),  # V2 near: flag 1
        Case("d8", {"eax": 0}, c3b_d_case(0, 0, 0x1000, 0x8000, 0x2000, 1, 0, 4)),  # V4 far: 0x35838
        Case("d9", {"eax": 0}, c3b_d_case(0, 0, 0, 0, 0, 0, live=False)),  # slot null: return
        Case("d10", {"eax": 0}, c3b_d_case(0, 0, 0, 0, 0, 0, recnull=True)),  # rec null: return
        Case("d11", {"eax": 1}, c3b_d_case(1, 8, 0, 0x1000, 0, 0)),       # side1 V1: v = 0x500
        Case("d12", {"eax": 1}, c3b_d_case(1, 8, 0x2000, 0x8000, 0x0B00, 0)),  # side1 V2 near
        Case("d13", {"eax": 1}, c3b_d_case(1, 8, 0, 0x1400, 0, 0)),       # side1 V1 near: flag 1
        Case("d14", {"eax": 0}, c3b_d_case(0, 0, 0, 0x5000, 0, 0)),       # v<0, bit14 clear: abs
        Case("d15", {"eax": 0}, c3b_d_case(0, 9, 0, 0x4800, 0, 1)),       # bd=9 still >= 8: flag 1
        Case("d16", {"eax": 0}, c3b_d_case(0, 0x88, 0, 0x4800, 0, 1, 0, 1)),  # bd 0x88 zero-extended >= 8 -> flag 1; kills the ==8 mutant and a sign-extended read
    ], allow_calls=(0x367DC,),
       calls=(ANIM_BEGIN,
              E.Call(0x36638, ("eax", "edx"), mode="real"),
              E.Call(0x35838, ("eax", "edx", "ebx"), mode="real")),
       eax_mask=0, mutants=("@mode8", "@range", "@abs", "@bit", "@bd", "@flag", "@call", "@dir")),
    # The voice remainder (record §C3b.1): the four DIG functions whose only callees are the AIL
    # runtime calls. The runtime addresses are stubbed, with the port's host wrappers carrying the
    # seams (ail.c AIL_sample_status/AIL_stop_sample/AIL_init_sample/AIL_stop_sequence, flow.c
    # snd_music_playing): every wrapper passes no arguments, so the E.Call entries record none (the
    # original's mem[] handle and the port's host pointer cannot be compared). EAX is AL everywhere
    # (the raw's `mov al,1`/`xor al,al`); the callee clobbers are E.callee_clobbers over the image.
    Spec("snd_music_stop", 0x1CA6C, [
        Case("m0", {}, {DS_001028C0: le32(0), DS_001028D4: le32(0xA5A5A5A5),
                        DS_001028D9: b"\xAA", DS_001028CC: le32(0xBBBBBBBB)}),      # no sequence: 0
        Case("m1", {}, {DS_001028C0: le32(0x10B000), DS_001028D4: le32(0xA5A5A5A5),
                        DS_001028D9: b"\xAA", DS_001028CC: le32(0xBBBBBBBB)}, {0x1CA40: 0}),
        Case("m2", {}, {DS_001028C0: le32(0x10B000), DS_001028D4: le32(0xA5A5A5A5),
                        DS_001028D9: b"\xAA", DS_001028CC: le32(0xBBBBBBBB)}, {0x1CA40: 1}),
    ], calls=(E.Call(0x1CA40, (), eax=0), E.Call(0x5DEAF, (), clobbers=("ebx", "ecx", "edx"))),
       eax_mask=0xFF,
       mutants=("@gate", "@play", "@stop", "@zero", "@al", "@cc")),
    # 0x1CE70 (record §C3b.1): EAX = the resource handle. The +0x0C dword of each 0x18-stride DIG
    # slot is compared; on a match 0x5DD03 (+0x00's AIL handle) status 4 returns 1, any other clears
    # the slot's +0x0C and the scan goes on. AL = 0 without a DIG driver.
    Spec("snd_sample_playing", 0x1CE70, [
        Case("q0", {"eax": 0x1234}, c3b_v_case(0, (0x1234, 0x99999999, 0x99999999, 0x99999999)),
             {0x5DD03: 4}),
        Case("q1", {"eax": 0x1234}, c3b_v_case(1, (0x99999999, 0x99999999, 0x99999999, 0x99999999)),
             {0x5DD03: 4}),
        Case("q2", {"eax": 0x1234}, c3b_v_case(1, (0x1234, 0x99999999, 0x99999999, 0x99999999)),
             {0x5DD03: 4}),
        Case("q3", {"eax": 0x1234}, c3b_v_case(1, (0x99999999, 0x1234, 0x99999999, 0x99999999)),
             {0x5DD03: 0}),
        Case("q4", {"eax": 0x1234}, c3b_v_case(1, (0x99999999, 0x99999999, 0x99999999, 0x1234)),
             {0x5DD03: 4}),
        Case("q5", {"eax": 0x1234}, c3b_v_case(1, (0x1234, 0x99999999, 0x99999999, 0x1234)),
             {0x5DD03: 0}),
        Case("q6", {"eax": 0x1234}, c3b_v_case(1, (0xAB001234, 0x99999999, 0x99999999, 0x99999999)),
             {0x5DD03: 4}),
    ], calls=(E.Call(0x5DD03, (), eax=0),), eax_mask=0xFF,
       mutants=("@driver", "@status", "@clear", "@scan", "@wide", "@al")),
    # 0x1CD9C (record §C3b.1): without a DIG driver AL = 0. Otherwise every slot's +0x04 and +0x0C
    # are cleared and a slot whose 0x5DD03 status is not 2 is stopped/re-inited; AL = 1.
    Spec("snd_samples_stop_all", 0x1CD9C, [
        Case("r0", {}, c3b_v_case(0, (0x99999999, 0x99999999, 0x99999999, 0x99999999),
                                  queued=0x11111111), {0x5DD03: 2}),
        Case("r1", {}, c3b_v_case(1, (0x99999999, 0x99999999, 0x99999999, 0x99999999),
                                  queued=0x11111111), {0x5DD03: 2}),
        Case("r2", {}, c3b_v_case(1, (0x99999999, 0x99999999, 0x99999999, 0x99999999),
                                  queued=0x11111111), {0x5DD03: 0}),
    ], calls=(E.Call(0x5DD03, (), eax=0), E.Call(0x5DC8B, (), clobbers=("ebx", "ecx", "edx")),
              E.Call(0x5DC0F, (), clobbers=("ebx", "ecx", "edx"))),
       eax_mask=0xFF, mutants=("@driver", "@skip2", "@clr4", "@clrc", "@al", "@order")),
    # 0x1CE04 (record §C3b.1): EAX = the handle. The first 0x18-stride slot whose +0x0C is the
    # handle and whose 0x5DD03 status is not 2 is stopped/re-inited, its +0x0C cleared, AL = 1.
    Spec("snd_sample_stop", 0x1CE04, [
        Case("t0", {"eax": 0x1234}, c3b_v_case(0, (0x1234, 0x99999999, 0x99999999, 0x99999999)),
             {0x5DD03: 4}),
        Case("t1", {"eax": 0x1234}, c3b_v_case(1, (0x99999999, 0x99999999, 0x99999999, 0x99999999)),
             {0x5DD03: 4}),
        Case("t2", {"eax": 0x1234}, c3b_v_case(1, (0x1234, 0x99999999, 0x99999999, 0x99999999)),
             {0x5DD03: 2}),
        Case("t3", {"eax": 0x1234}, c3b_v_case(1, (0x1234, 0x99999999, 0x99999999, 0x99999999)),
             {0x5DD03: 0}),
        Case("t4", {"eax": 0x1234}, c3b_v_case(1, (0x99999999, 0x99999999, 0x1234, 0x99999999)),
             {0x5DD03: 4}),
        Case("t5", {"eax": 0x1234}, c3b_v_case(1, (0x1234, 0x1234, 0x99999999, 0x99999999)),
             {0x5DD03: 0}),
        Case("t6", {"eax": 0x1234}, c3b_v_case(1, (0xAB001234, 0x99999999, 0x99999999, 0x99999999)),
             {0x5DD03: 4}),
    ], calls=(E.Call(0x5DD03, (), eax=0), E.Call(0x5DC8B, (), clobbers=("ebx", "ecx", "edx")),
              E.Call(0x5DC0F, (), clobbers=("ebx", "ecx", "edx"))),
       eax_mask=0xFF, mutants=("@driver", "@eq2", "@clear", "@nolimit", "@al", "@calls")),
    # 0x1CC28 snd_sample_queue (record §C3b.1): EAX = handle h, DL = the loop byte. Without a DIG
    # driver, while the sample pause byte DS_001028DB is set, or when the handle does not resolve
    # (0x1B544 allow; the 0x500BB allow reads the same DS_00101500), the queue does nothing. A
    # payload above 0x6000 bytes tries only slot 0: buffer +0x10 set, +0x04 free and 0x5DD03 status
    # not 4 queue there; otherwise the tail. A payload at or below 0x6000 scans slots 3..0 for a
    # free arm (+0x10 set, +0x04 clear, status != 4) and otherwise takes the tail's candidate, the
    # slot with the smallest +0x14 time strictly below the 0x500BB now (slot 0 on a tie). The tail
    # stops and re-inits the handle's AIL sample (0x5DC8B/0x5DC0F), stores h/+0x08/now and AL = 1.
    # The slot fields are seeded by one 0x60-byte poke (queued 0x11111111, loop 0x22, current
    # 0x99999999, buffer 0x33333333, time 0x44444444), overridden per case.
    # 0x249C0 list_insert_before (record §C3b.1): EAX = `at`, EDX = `rec`. prev = [at+4];
    # [at+4] = rec; [rec] = at; [rec+4] = prev; [prev] = rec. Straight-line, no callees; the row
    # binds the effects.c copy (the actors.c copy is the same body, as in C3's list rows). Mask 0.
    Spec("list_insert_before", 0x249C0, [
        Case("v0", {"eax": 0x10A200, "edx": 0x10A240},
             {0x10A204: le32(0x10A260), 0x10A260: le32(0xA5A5A5A5),
              0x10A240: le32(0x11111111), 0x10A244: le32(0x22222222)}),
        Case("v1", {"eax": 0x10A200, "edx": 0x10A240},
             {0x10A204: le32(0x10A200), 0x10A240: le32(0x33333333), 0x10A244: le32(0x44444444)}),
        Case("v2", {"eax": 0x10A200, "edx": 0x10A240},
             {0x10A204: le32(0x10A240), 0x10A240: le32(0x55555555), 0x10A244: le32(0x66666666)}),
    ], eax_mask=0, mutants=("@prev", "@link", "@back", "@head", "@forward")),
    Spec("snd_sample_queue", 0x1CC28, [
        Case("z0", {"eax": 0x800001, "edx": 0x37}, c3b_q_case(driver=0), {0x5DD03: 0}),
        Case("z1", {"eax": 0x800001, "edx": 0x37}, c3b_q_case(db=1), {0x5DD03: 0}),
        # size > 0x6000: slot 0's arm queues (buf set, +0x04 clear, status 0).
        Case("z2", {"eax": 0x800001, "edx": 0x37}, c3b_q_case(overrides={C3B_Q_QUEUED: le32(0)}),
             {0x5DD03: 0}),
        # size > 0x6000: slot 0 has no buffer, the tail queues slot 0.
        Case("z3", {"eax": 0x800001, "edx": 0x37},
             c3b_q_case(overrides={C3B_Q_QUEUED: le32(0), C3B_Q_BUF: le32(0)}), {0x5DD03: 0}),
        # size > 0x6000: slot 0 is occupied (+0x04 set), the tail queues it.
        Case("z4", {"eax": 0x800001, "edx": 0x37}, c3b_q_case(), {0x5DD03: 0}),
        # size > 0x6000: slot 0 plays (status 4), the tail queues it.
        Case("z5", {"eax": 0x800001, "edx": 0x37}, c3b_q_case(overrides={C3B_Q_QUEUED: le32(0)}),
             {0x5DD03: 4}),
        # size <= 0x6000: every arm fails differently; the smallest time is slot 1's.
        Case("z6", {"eax": 0x800001, "edx": 0x37},
             c3b_q_case(size=0x100, overrides={
                 C3B_Q_T: le32(0x400), C3B_Q_T + 0x18: le32(0x100), C3B_Q_T + 0x30: le32(0x200),
                 C3B_Q_T + 0x48: le32(0x300), C3B_Q_QUEUED: le32(0), C3B_Q_BUF + 0x18: le32(0)}),
             {0x5DD03: 4}),
        # size <= 0x6000: slot 2's arm queues before any candidate scan.
        Case("z7", {"eax": 0x800001, "edx": 0x37},
             c3b_q_case(size=0x100, overrides={C3B_Q_BUF + 0x48: le32(0), C3B_Q_QUEUED + 0x30: le32(0)}),
             {0x5DD03: 0}),
        # size <= 0x6000: slots 1 and 0 tie on the smallest time; the strict compare keeps 2.
        Case("z8", {"eax": 0x800001, "edx": 0x37},
             c3b_q_case(size=0x100, overrides={
                 C3B_Q_T: le32(0x100), C3B_Q_T + 0x18: le32(0x200), C3B_Q_T + 0x30: le32(0x100),
                 C3B_Q_T + 0x48: le32(0x200), C3B_Q_QUEUED: le32(0)}), {0x5DD03: 4}),
        # size <= 0x6000: slots 3 and 0 are both free; the 3..0 scan queues slot 3.
        Case("z9", {"eax": 0x800001, "edx": 0x37},
             c3b_q_case(size=0x100, overrides={C3B_Q_QUEUED + 0x48: le32(0), C3B_Q_QUEUED: le32(0)}),
             {0x5DD03: 0}),
    ], allow_calls=(0x500BB, 0x1B544),
       calls=(E.Call(0x5DD03, (), eax=0), E.Call(0x5DC8B, (), clobbers=("ebx", "ecx", "edx")),
              E.Call(0x5DC0F, (), clobbers=("ebx", "ecx", "edx"))),
       eax_mask=0xFF, mutants=("@driver", "@pause", "@size", "@armbuf", "@armq", "@armst",
                               "@order", "@candcmp", "@candmin", "@tailret", "@tailstop",
                               "@tailorder", "@loopbyte")),
    Spec("effect_teardown", 0x13420, [
        Case("e0", {"eax": C3B_E_REC}, c3b_e_pokes(0)),
        Case("e1", {"eax": C3B_E_REC}, c3b_e_pokes(1)),
        Case("e2", {"eax": C3B_E_REC}, c3b_e_pokes(2)),
        Case("e3", {"eax": C3B_E_REC}, c3b_e_pokes(3)),
        Case("e4", {"eax": C3B_E_REC}, c3b_e_pokes(4)),
        Case("e5", {"eax": C3B_E_REC}, c3b_e_pokes(5)),
        Case("e6", {"eax": C3B_E_REC}, c3b_e_pokes(6)),
        Case("e7", {"eax": C3B_E_REC}, c3b_e_pokes(0x80)),
    ], allow_calls=(0x249D0, 0x249B0, 0x33734, 0x33714),
       eax_mask=0, mutants=("@unlink", "@restore", "@type1", "@flag", "@buf", "@tail")),
    Spec("actor_type_49444", 0x49444, [
        Case("a0", {"eax": C3B_F_REC}, c3b_f_pokes(rec14=0)),                 # rec+0x14 null
        Case("a1", {"eax": C3B_F_REC}, c3b_f_pokes()),                        # bit1, idx 3
        Case("a2", {"eax": C3B_F_REC}, c3b_f_pokes(flags=0)),                 # bit1 clear
        Case("a3", {"eax": C3B_F_REC}, c3b_f_pokes(child=0x10AB00)),          # child retired
        Case("a4", {"eax": C3B_F_REC}, c3b_f_pokes(q=0xFFFF0000)),            # idx -1
        Case("a5", {"eax": C3B_F_REC}, c3b_f_pokes(flags=0x22)),              # bit5 too
        Case("a6", {"eax": C3B_F_REC}, c3b_f_pokes(flags=1)),                 # bit0 only
        Case("a7", {"eax": C3B_F_REC}, c3b_f_pokes(q=0x00020000, child=0x10AB00)),  # idx 2, child
    ], calls=(E.Call(0x2B150, ("eax",)),
              E.Call(0x249D0, ("eax",), mode="real"),
              E.Call(0x249B0, ("eax", "edx"), mode="real")),
       eax_mask=0, mutants=("@bit", "@idx", "@sign", "@child", "@link", "@reclr")),
    # 0x2B150 (record §C3b.1). The callback targets 0x5D812 (a stub) and 0x12800 (the port's
    # actor_type_12800; the callback's own 0x249D0/0x249B0 calls are in the call set) are
    # allowed; the render pair 0x1C458+0x1C3D0 is allowed as the raw's two calls (the port's
    # render_list_remove is the same body).
    Spec("set_dead", 0x2B150, [
        Case("s0", {"eax": C3B_S_REC}, c3b_s_pokes()),
        Case("s1", {"eax": C3B_S_REC}, c3b_s_pokes(pset18=0x10AB00)),
        Case("s2", {"eax": C3B_S_REC}, c3b_s_pokes(rnode=True)),
        Case("s3", {"eax": C3B_S_REC}, c3b_s_pokes(typ=0, cb=True)),
        Case("s4", {"eax": C3B_S_REC}, c3b_s_pokes(typ=1, cb=True, node=C3B_S_NODE)),
        Case("s5", {"eax": C3B_S_REC}, c3b_s_pokes(typ=1, cb=True)),
        Case("s6", {"eax": C3B_S_REC}, c3b_s_pokes(pset18=0x10AB00, idx=1)),
        Case("s7", {"eax": C3B_S_REC},
             c3b_s_pokes(typ=1, cb=True, node=C3B_S_NODE, pset18=0x10AB00)),
    ], allow_calls=(0x1C458, 0x1C3D0, 0x5D812, 0x12800),
       calls=(E.Call(0x33864, ("eax",)),
              E.Call(0x249D0, ("eax",), mode="real"),
              E.Call(0x249B0, ("eax", "edx"), mode="real")),
       eax_mask=0, mutants=("@bit", "@gate", "@clear", "@table", "@pset", "@pal", "@render")),
    # 0x29DB8 anim_write_var (record §C3b.1): EAX = rec, EDX = op, EBX = value (0x29DBC
    # `mov eax,ebx`). o = (EDX & 0xFF) & 0x7F: below 0x40 the ring word at 0x105B4C indexed by
    # (o + rec+0x51) & 0x3F takes (u16)value; 0x40..0x45 the record's own +0x52..+0x55/+0x58
    # bytes and the +0x56 word (low byte only), all (u8)value or (u16)value&0xFF; 0x46..0x4B and
    # 0x4C..0x51 the same fields on the pool record named by rec+0x4A / rec+0x4B (the byte forms
    # store DS_00105BE8, not value); above 0x51 nothing. Mask 0.
    Spec("anim_write_var", 0x29DB8, [
        Case("w0", {"eax": C3B_W_REC, "edx": 0, "ebx": 0x1234},
             c3b_w_pokes(i51=0x3F, ring={0: b"\xBB\xBB", 0x3E: b"\xAA\xAA"})),
        Case("w1", {"eax": C3B_W_REC, "edx": 1, "ebx": 0x5678},
             c3b_w_pokes(i51=0x3F, ring={0: b"\xBB\xBB", 0x3F: b"\xAA\xAA"})),
        Case("w2", {"eax": C3B_W_REC, "edx": 2, "ebx": 0x9ABC},
             c3b_w_pokes(i51=0x3E, ring={0: b"\xBB\xBB", 0x3F: b"\xAA\xAA"})),
        Case("w3", {"eax": C3B_W_REC, "edx": 0x40, "ebx": 0x11223344}, c3b_w_pokes()),
        Case("w4", {"eax": C3B_W_REC, "edx": 0x41, "ebx": 0x5566}, c3b_w_pokes()),
        Case("w5", {"eax": C3B_W_REC, "edx": 0x42, "ebx": 0x7788}, c3b_w_pokes()),
        Case("w6", {"eax": C3B_W_REC, "edx": 0x43, "ebx": 0x99AA}, c3b_w_pokes()),
        Case("w7", {"eax": C3B_W_REC, "edx": 0x44, "ebx": 0x1234}, c3b_w_pokes()),
        Case("w8", {"eax": C3B_W_REC, "edx": 0x45, "ebx": 0x5566}, c3b_w_pokes()),
        Case("w9", {"eax": C3B_W_REC, "edx": 0x46, "ebx": 0x99}, c3b_w_pokes(i4a=0, be8=0x5A)),
        Case("w10", {"eax": C3B_W_REC, "edx": 0x4A, "ebx": 0x1234}, c3b_w_pokes(i4a=0)),
        Case("w11", {"eax": C3B_W_REC, "edx": 0x4B, "ebx": 0x7788}, c3b_w_pokes(i4a=0)),
        Case("w12", {"eax": C3B_W_REC, "edx": 0x4C, "ebx": 0x99}, c3b_w_pokes(i4a=0, i4b=1, be8=0x6B)),
        Case("w13", {"eax": C3B_W_REC, "edx": 0x50, "ebx": 0x1234}, c3b_w_pokes(i4a=0, i4b=1)),
        Case("w14", {"eax": C3B_W_REC, "edx": 0x51, "ebx": 0x99}, c3b_w_pokes(i4a=0, i4b=1)),
        Case("w15", {"eax": C3B_W_REC, "edx": 0x52, "ebx": 0x99}, c3b_w_pokes()),
        Case("w16", {"eax": C3B_W_REC, "edx": 0xFF, "ebx": 0x99}, c3b_w_pokes()),
        Case("w17", {"eax": C3B_W_REC, "edx": 0xBF, "ebx": 0x4321},
             c3b_w_pokes(i51=0, ring={0x3F: b"\xAA\xAA"})),
        Case("w18", {"eax": C3B_W_REC, "edx": 0xC0, "ebx": 0xABCD}, c3b_w_pokes()),
        Case("w19", {"eax": C3B_W_REC, "edx": 0x47, "ebx": 0x99}, c3b_w_pokes(i4a=0, be8=0x61)),
        Case("w20", {"eax": C3B_W_REC, "edx": 0x48, "ebx": 0x99}, c3b_w_pokes(i4a=0, be8=0x62)),
        Case("w21", {"eax": C3B_W_REC, "edx": 0x49, "ebx": 0x99}, c3b_w_pokes(i4a=0, be8=0x63)),
    ], eax_mask=0,
       mutants=("@mask", "@ringm", "@radio", "@w44", "@swap", "@bep", "@child", "@high")),
    # 0x2B8F8 anim_operand (record §C3b.1): EAX = rec. cw = DSW(DSD(rec+8)); mode = cw & 0x6000
    # is stored at DS_00105BE6. When the previous op DS_00105BE4 is 0x1F, p advances by 2 and the
    # next word is the operand (cx); when not, cx = cw & 0xFF and the mode-0 arm returns cx
    # zero/sign-extended and stores it at DS_00105BE8. Otherwise 0x29F34 reads the variable
    # (stubbed here: its own row proves it) and the mode selects: 0x2000 the read word; 0x4000
    # the read word with DS_00105BD4 = DSD(rec+8 + 2) and return; otherwise DS_00105BD4 =
    # DSD(rec+0x0C) and the next word's 0xF000: 0x1000 v*2, 0x2000 v*2+1, 0x4000/0x5000/0x6000
    # a word at base + v*2 / v*4 / v*4+2 (DS_00105BD4 = that address), anything else the byte at
    # base + v (sign-extended to 0xFFxx when above 0x7F). EAX is masked to 16 bits on every return.
    Spec("anim_operand", 0x2B8F8, [
        Case("o0", {"eax": C3B_O_REC}, c3b_o_pokes(op=0x1F, cw=0x0001, w2=0x1234)),
        Case("o1", {"eax": C3B_O_REC}, c3b_o_pokes(op=0x1F, cw=0x2002, w2=0x002A),
             stub_eax={0x29F34: 0x7777}),
        Case("o2", {"eax": C3B_O_REC}, c3b_o_pokes(cw=0x0005)),
        Case("o3", {"eax": C3B_O_REC}, c3b_o_pokes(cw=0x0085)),
        Case("o4", {"eax": C3B_O_REC}, c3b_o_pokes(cw=0x2007), stub_eax={0x29F34: 0x1234}),
        Case("o5", {"eax": C3B_O_REC}, c3b_o_pokes(cw=0x4008, w2=0xB100, w4=0x0010),
             stub_eax={0x29F34: 0x9ABC}),
        Case("o6", {"eax": C3B_O_REC}, c3b_o_pokes(cw=0x6009, w4=0x1000),
             stub_eax={0x29F34: 3}),
        Case("o7", {"eax": C3B_O_REC}, c3b_o_pokes(cw=0x600A, w4=0x2000),
             stub_eax={0x29F34: 3}),
        Case("o8", {"eax": C3B_O_REC}, c3b_o_pokes(cw=0x600B, w4=0x4000),
             stub_eax={0x29F34: 2}),
        Case("o9", {"eax": C3B_O_REC}, c3b_o_pokes(cw=0x600C, w4=0x5000),
             stub_eax={0x29F34: 2}),
        Case("o10", {"eax": C3B_O_REC}, c3b_o_pokes(cw=0x600D, w4=0x6000),
             stub_eax={0x29F34: 2}),
        Case("o11", {"eax": C3B_O_REC}, c3b_o_pokes(cw=0x600E, w4=0),
             stub_eax={0x29F34: 2}),
        Case("o12", {"eax": C3B_O_REC}, c3b_o_pokes(cw=0x600F, w4=0),
             stub_eax={0x29F34: 3}),
        Case("o13", {"eax": C3B_O_REC}, c3b_o_pokes(cw=0x6010, w4=0x3000),
             stub_eax={0x29F34: 4}),
        Case("o14", {"eax": C3B_O_REC}, c3b_o_pokes(op=0x1F, cw=0x0003, w2=0xFFFF)),
        Case("o15", {"eax": C3B_O_REC}, c3b_o_pokes(cw=0x6011, w4=0x7000),
             stub_eax={0x29F34: 5}),
    ], calls=(E.Call(0x29F34, ("eax", "edx"), clobbers=("edx",)),),
       eax_mask=0xFFFF,
       mutants=("@op", "@mask", "@adv", "@be8", "@sel", "@scale", "@deref", "@byte")),
    # 0x3AFC4 fighter_anim_triple (record §C3b.1). The 0x3AFCE block is the raw's 0x62003 fatal
    # path (EDX outside [0,0x40)); the port zeroes the triple there, and no case can stop the
    # original on a fatal, so the block is named unhit. Mask 0 (every port caller ignores EAX).
    Spec("fighter_anim_triple", 0x3AFC4, [
        Case("t0", {"eax": 0, "edx": 0, "ebx": C3B_T_OUT}, c3b_t_pokes(sc=0, tbl=0x03)),
        Case("t1", {"eax": 0, "edx": 0x3F, "ebx": C3B_T_OUT}, c3b_t_pokes(sc=0, tbl=0x03)),
        Case("t2", {"eax": 1, "edx": 0x20, "ebx": C3B_T_OUT}, c3b_t_pokes(sc=1, tbl=0x01)),
        Case("t3", {"eax": 1, "edx": 0, "ebx": C3B_T_OUT}, c3b_t_pokes(sc=1, tbl=0x01)),
        Case("t4", {"eax": 2, "edx": 0x11, "ebx": C3B_T_OUT}, c3b_t_pokes(sc=2, tbl=0xFF)),
    ], unhit_named={0x3AFCE: "the raw's 0x62003 fatal for edx outside [0,0x40)"},
       eax_mask=0, mutants=("@idx", "@shift", "@add", "@o0", "@o1", "@o2")),
]

# ---- track P batch C3c (record 2026-10-05-reverse-c3c): the frontier rows, part 3 ----------------
#
# The render-list halves (0x1C390/0x1C3A0/0x1C458/0x1C3D0), the type-0x01 callbacks
# (0x127C0/0x12800) and the new frontier items (0x18428/0x18460/0x367DC/0x1CA40/0x33714/0x33734).
# The four render rows share this scratch: nodes are { next; pset } and the layer is pset+0x0E.
C3C_R_FREE = 0x0010275C         # the render free-list head
C3C_R_LIST = 0x00105B44         # the render list head
C3C_R_HEAD = 0x10AF00           # a scratch list head for the splice/find/unlink rows
C3C_R_N1, C3C_R_N2, C3C_R_N3, C3C_R_N4 = 0x10AF40, 0x10AF80, 0x10AFC0, 0x10B000
C3C_R_P1, C3C_R_P2, C3C_R_P3, C3C_R_P4 = 0x10B040, 0x10B080, 0x10B0A0, 0x10B0B0
C3C_MP_HUB = 0x10AF00           # the type-callback hub scratch


def c3c_r_node(nxt, pset, layer):
    return {nxt[0]: le32(nxt[1]), nxt[0] + 4: le32(pset), pset + 0x0E: le16(layer)}


def c3c_r_list(head, items):
    """Pokes linking `items` = [(node, pset, layer)] from [head]; the last next is 0."""
    p = {head: le32(items[0][0] if items else 0)}
    for i, (n, ps, ly) in enumerate(items):
        p[n] = le32(items[i + 1][0] if i + 1 < len(items) else 0)
        p[n + 4] = le32(ps)
        p[ps + 0x0E] = le16(ly)
    return p


def c3c_b6_case(side, s53, s54, o54, bit15_0, bit15_1):
    """0x3B6C4's slots: the side's +0x53/+0x54, the other's +0x54 and the two actor bit-15 words."""
    rec0, rec1 = 0x10AF00, 0x10AF40
    p = {0x001077B0: le32(rec0), 0x001077B0 + 0x94: le32(rec1),
         rec0 + 0x56: le16(1), rec1 + 0x56: le16(2),
         0x001014EC: le32(0x10B000),
         0x10B020: le16(bit15_0 << 15), 0x10B040: le16(bit15_1 << 15),
         0x001077B0 + side * 0x94 + 0x53: bytes([s53]),
         0x001077B0 + side * 0x94 + 0x54: bytes([s54]),
         0x001077B0 + (1 - side) * 0x94 + 0x54: bytes([o54])}
    return p


def c3c_k_case(side, b60, b62, b43, ch):
    """0x1A734's side slot and its record (EAX = side; ch selects the 0xC8F40/0xC8F90 stream)."""
    slot = 0x001077B0 + side * 0x94
    rec = 0x10AF00 + side * 0x40
    return {slot: le32(rec), slot + 0x60: bytes([b60]), slot + 0x62: bytes([b62]),
            slot + 0x43: bytes([b43]), slot + 0x54: b"\xAA", slot + 0x7A: bytes([ch])}


def c3c_97_case(side, self8c, other8c, k, b, k2=0, s63=0, diff=0x5678, d=1):
    """0x39738's state: the per-side 0x8C words, the slot+0x63 gate, the two k words, the two
    12-entry per-character tables (0xBEC28/0xBEC58; their entries 11 are the raw's >0xB fallbacks
    0xBEC54/0xBEC84) and the difficulty table entry 0xBEBD8[d]. Only the entries a case (or a
    mutant) reads are poked: the harness caps a case at 16 pokes."""
    s = 0x001077B0 + side * 0x94
    o = 0x001077B0 + (1 - side) * 0x94
    kk, k2c = min(k, 11), min(k2, 11)
    p = {s + 0x8C: le16(self8c), o + 0x8C: le16(other8c), s + 0x63: bytes([s63]),
         0x00107D2A + side * 2: le32(k << 16), 0x00107D1E + side * 2: le32(k2 << 16),
         0x001082C8 + side * 4: le32(d), 0x000BEBD8: le32(0x5555),
         0x000BEBD8 + d * 4: le32(diff)}
    if self8c:
        p[0x000BEC58 + kk * 4] = le32(0x2000 + kk)
        if k2c != kk:
            p[0x000BEC58 + k2c * 4] = le32(0x2000 + k2c)
        p[0x000BEC58 + 44] = le32(0x200B)
        p[0x000BEC58 + 48] = le32(0x200C)
    else:
        p[0x000BEC28 + kk * 4] = le32(0x1000 + kk)
        if k2c != kk:
            p[0x000BEC28 + k2c * 4] = le32(0x1000 + k2c)
        p[0x000BEC28 + 44] = le32(0x100B)
    return p


def c3c_46_case(ch, id0, id1=0x0015, lo=0x0010, hi=0x0020):
    """0x18460's state: slot0/slot1 + their records + the ch range pair + the actor words.
    idx0 = 1 and idx1 = 2, so a0 = 1 and a1 = 2 on every call."""
    s0, s1, r0, r1 = 0x10AF00, 0x10AF80, 0x10AF40, 0x10AFC0
    return {0x001077A8: le32(s0), 0x001077AC: le32(s1), s0: le32(r0), s1: le32(r1),
            s0 + 0x7A: bytes([ch & 0xFF]), s1 + 0x7A: b"\x00",
            r0 + 0x56: le16(1), r1 + 0x56: le16(2),
            0x001014EC: le32(0x10B000),
            0x10B000 + 1 * 0x20: le16(id0), 0x10B000 + 2 * 0x20: le16(id1),
            0x000A1774 + ch * 14: le16(lo), 0x000A1776 + ch * 14: le16(hi)}


C3C_SPECS = [
    # 0x1C390 render_pop_free: pop the free-list head [0x10275C]; EAX = the popped node. The raw
    # dereferences the new head unconditionally (no empty-list case; the port's caller guards it).
    Spec("render_pop_free", 0x1C390, [
        Case("p0", {}, {C3C_R_FREE: le32(C3C_R_N1), C3C_R_N1: le32(C3C_R_N2), C3C_R_N2: le32(0)}),
        Case("p1", {}, {C3C_R_FREE: le32(C3C_R_N3), C3C_R_N3: le32(0)}),
        Case("p2", {}, {C3C_R_FREE: le32(C3C_R_N1), C3C_R_N1: le32(C3C_R_N2),
                        C3C_R_N2: le32(C3C_R_N3), C3C_R_N3: le32(0),
                        C3C_R_N1 + 4: le32(0x5A5A5A5A)}),
    ], eax_mask=0xFFFFFFFF, mutants=("@head", "@ret", "@next")),
    # 0x1C3A0 render_splice: EAX = headp, EDX = node; insert node before the first node whose
    # pset+0x0E layer is greater (stable <=). Void.
    Spec("render_splice", 0x1C3A0, [
        Case("s0", {"eax": C3C_R_HEAD, "edx": C3C_R_N1},
             {C3C_R_HEAD: le32(0), C3C_R_N1: le32(0xDEADBEEF), C3C_R_N1 + 4: le32(C3C_R_P1),
              C3C_R_P1 + 0x0E: le16(0x10)}),
        Case("s1", {"eax": C3C_R_HEAD, "edx": C3C_R_N1},
             {**c3c_r_list(C3C_R_HEAD, [(C3C_R_N2, C3C_R_P2, 0x30)]),
              C3C_R_N1: le32(0xDEADBEEF), C3C_R_N1 + 4: le32(C3C_R_P1), C3C_R_P1 + 0x0E: le16(0x10)}),
        Case("s2", {"eax": C3C_R_HEAD, "edx": C3C_R_N1},
             {**c3c_r_list(C3C_R_HEAD, [(C3C_R_N2, C3C_R_P2, 0x10), (C3C_R_N3, C3C_R_P3, 0x30)]),
              C3C_R_N1: le32(0xDEADBEEF), C3C_R_N1 + 4: le32(C3C_R_P1), C3C_R_P1 + 0x0E: le16(0x20)}),
        Case("s3", {"eax": C3C_R_HEAD, "edx": C3C_R_N1},
             {**c3c_r_list(C3C_R_HEAD, [(C3C_R_N2, C3C_R_P2, 0x10), (C3C_R_N3, C3C_R_P3, 0x20)]),
              C3C_R_N1: le32(0xDEADBEEF), C3C_R_N1 + 4: le32(C3C_R_P1), C3C_R_P1 + 0x0E: le16(0x30)}),
        Case("s4", {"eax": C3C_R_HEAD, "edx": C3C_R_N1},
             {**c3c_r_list(C3C_R_HEAD, [(C3C_R_N2, C3C_R_P2, 0x20)]),
              C3C_R_N1: le32(0xDEADBEEF), C3C_R_N1 + 4: le32(C3C_R_P1), C3C_R_P1 + 0x0E: le16(0x20)}),
        Case("s5", {"eax": C3C_R_HEAD, "edx": C3C_R_N1},
             {**c3c_r_list(C3C_R_HEAD, [(C3C_R_N2, C3C_R_P2, 0x20), (C3C_R_N3, C3C_R_P3, 0x20)]),
              C3C_R_N1: le32(0xDEADBEEF), C3C_R_N1 + 4: le32(C3C_R_P1), C3C_R_P1 + 0x0E: le16(0x20)}),
    ], eax_mask=0, mutants=("@lt", "@next", "@head", "@prev", "@first")),
    # 0x1C458 render_find: EAX = headp, EDX = pset_off; EAX = the node whose +4 matches, or 0.
    Spec("render_find", 0x1C458, [
        Case("f0", {"eax": C3C_R_HEAD, "edx": 0x10B040},
             {C3C_R_HEAD: le32(C3C_R_N1), C3C_R_N1: le32(0), C3C_R_N1 + 4: le32(0x10B040)}),
        Case("f1", {"eax": C3C_R_HEAD, "edx": 0x10B080},
             {C3C_R_HEAD: le32(C3C_R_N1), C3C_R_N1: le32(C3C_R_N2), C3C_R_N1 + 4: le32(0x10B040),
              C3C_R_N2: le32(0), C3C_R_N2 + 4: le32(0x10B080)}),
        Case("f2", {"eax": C3C_R_HEAD, "edx": 0x10B0A0},
             {C3C_R_HEAD: le32(C3C_R_N1), C3C_R_N1: le32(C3C_R_N2), C3C_R_N1 + 4: le32(0x10B040),
              C3C_R_N2: le32(0), C3C_R_N2 + 4: le32(0x10B080)}),
        Case("f3", {"eax": C3C_R_HEAD, "edx": 0x10B040}, {C3C_R_HEAD: le32(0)}),
    ], eax_mask=0xFFFFFFFF, mutants=("@cmp", "@skip", "@last", "@null")),
    # 0x1C3D0 render_unlink: EAX = headp, EDX = node; unlink node (no-op when absent) and push it
    # on the free-list head. Void.
    Spec("render_unlink", 0x1C3D0, [
        Case("u0", {"eax": C3C_R_HEAD, "edx": C3C_R_N2},
             {**c3c_r_list(C3C_R_HEAD, [(C3C_R_N1, C3C_R_P1, 0x10), (C3C_R_N2, C3C_R_P2, 0x20),
                                        (C3C_R_N3, C3C_R_P3, 0x30)]),
              C3C_R_N2: le32(C3C_R_N3), C3C_R_FREE: le32(C3C_R_N4), C3C_R_N4: le32(0xF0F0F0F0)}),
        Case("u1", {"eax": C3C_R_HEAD, "edx": C3C_R_N1},
             {**c3c_r_list(C3C_R_HEAD, [(C3C_R_N1, C3C_R_P1, 0x10), (C3C_R_N2, C3C_R_P2, 0x20)]),
              C3C_R_FREE: le32(0), C3C_R_N1: le32(C3C_R_N2)}),
        Case("u2", {"eax": C3C_R_HEAD, "edx": C3C_R_N3},
             {**c3c_r_list(C3C_R_HEAD, [(C3C_R_N1, C3C_R_P1, 0x10), (C3C_R_N2, C3C_R_P2, 0x20),
                                        (C3C_R_N3, C3C_R_P3, 0x30)]),
              C3C_R_FREE: le32(C3C_R_N1), C3C_R_N3: le32(0x01020304)}),
        Case("u3", {"eax": C3C_R_HEAD, "edx": C3C_R_N4},
             {**c3c_r_list(C3C_R_HEAD, [(C3C_R_N1, C3C_R_P1, 0x10), (C3C_R_N2, C3C_R_P2, 0x20)]),
              C3C_R_FREE: le32(C3C_R_N4)}),
        Case("u4", {"eax": C3C_R_HEAD, "edx": C3C_R_N1},
             {C3C_R_HEAD: le32(0), C3C_R_FREE: le32(C3C_R_N1)}),
    ], eax_mask=0, mutants=("@head", "@free", "@chain", "@prev", "@noop")),
    # 0x127C0 actor_type_127C0 (the type-0x01 cb1): EAX = rec, EDX = slot (unread). Pop the 0xF0A78
    # head; empty (the sentinel points to itself) -> EAX 0xFF, else link the node at rec+0x14 and
    # insert it after 0xF0AE0, return 0. The 0x249D0/0x249B0 calls run as real on both sides.
    Spec("actor_type_127C0", 0x127C0, [
        Case("t0", {"eax": C3C_MP_HUB, "edx": 0x10AF40},
             {0x000F0A78: le32(0x000F0A78), 0x000F0A78 + 4: le32(0x000F0A78),
              C3C_MP_HUB + 0x14: le32(0x11111111)}),
        Case("t1", {"eax": C3C_MP_HUB, "edx": 0x10AF40},
             {0x000F0A78: le32(C3C_R_N1), C3C_R_N1: le32(0x000F0A78),
              C3C_R_N1 + 4: le32(0x000F0A78), C3C_R_N1 + 8: le32(0xF0F0F0F0),
              0x000F0AE0: le32(0x000F0AE0), 0x000F0AE0 + 4: le32(0x000F0AE0),
              C3C_MP_HUB + 0x14: le32(0x11111111)}),
        Case("t2", {"eax": C3C_R_N4, "edx": C3C_R_P4},
             {0x000F0A78: le32(C3C_R_N2), C3C_R_N2: le32(0x000F0A78),
              C3C_R_N2 + 4: le32(0x000F0A78), C3C_R_N2 + 8: le32(0xF0F0F0F0),
              0x000F0AE0: le32(C3C_R_N3), C3C_R_N3: le32(0x000F0AE0),
              C3C_R_N3 + 4: le32(0x000F0AE0), 0x000F0AE0 + 4: le32(C3C_R_N3),
              C3C_R_N4 + 0x14: le32(0x11111111)}),
    ], calls=(E.Call(0x249D0, ("eax",), mode="real"),
              E.Call(0x249B0, ("eax", "edx"), mode="real")),
       eax_mask=0xFF, mutants=("@empty", "@ret", "@link", "@rec", "@at", "@unlink")),
    # 0x12800 actor_type_12800 (the type-0x01 cb2): EAX = rec. Return the rec+0x14 node to 0xF0A78
    # and clear rec+0x14; a null node returns with nothing done.
    Spec("actor_type_12800", 0x12800, [
        Case("t0", {"eax": C3C_MP_HUB}, {C3C_MP_HUB + 0x14: le32(0)}),
        Case("t1", {"eax": C3C_MP_HUB},
             {C3C_MP_HUB + 0x14: le32(C3C_R_N1), 0x10AF80: le32(C3C_R_N1),
              C3C_R_N1: le32(0x10AF80), C3C_R_N1 + 4: le32(0x10AF80),
              0x000F0A78: le32(0x000F0A78), 0x000F0A78 + 4: le32(0x000F0A78)}),
        Case("t2", {"eax": C3C_MP_HUB},
             {C3C_MP_HUB + 0x14: le32(C3C_R_N2), 0x10AF40: le32(C3C_R_N2),
              C3C_R_N2: le32(0x10AF40), C3C_R_N2 + 4: le32(0x10AF40),
              0x000F0A78: le32(C3C_R_N3), C3C_R_N3: le32(0x000F0A78),
              C3C_R_N3 + 4: le32(0x000F0A78), 0x000F0A78 + 4: le32(C3C_R_N3)}),
    ], calls=(E.Call(0x249D0, ("eax",), mode="real"),
              E.Call(0x249B0, ("eax", "edx"), mode="real")),
       eax_mask=0, mutants=("@zero", "@clear", "@at", "@unlink", "@node")),
    # 0x18428 fighter_18428: EAX = side. Reads slot[side]+0x7A and (for a character 0..6) jumps
    # through the all-RET table 0x1840C. Effect-free: reads only, so no case can catch a read-only
    # mutation and the row carries no mutants (a named limit, record §C3c).
    Spec("fighter_18428", 0x18428, [
        Case("h0", {"eax": 0}, {0x1077A8: le32(C3C_R_N1), C3C_R_N1 + 0x7A: b"\x03"}),
        Case("h1", {"eax": 1}, {0x1077A8: le32(C3C_R_N1), 0x1077AC: le32(C3C_R_N2),
                                C3C_R_N1 + 0x7A: b"\x00", C3C_R_N2 + 0x7A: b"\x06"}),
        Case("h2", {"eax": 0}, {0x1077A8: le32(C3C_R_N2), C3C_R_N2 + 0x7A: b"\x07"}),
        Case("h3", {"eax": 1}, {0x1077A8: le32(0), 0x1077AC: le32(0)}),
    ], eax_mask=0, mutants=()),
    # 0x18460 fighter_18460: EAX = side. Both slot pointers must be live; the side's character
    # 0..6 reads {lo,hi} = 0xA1774/76 + ch*14, the sprite id is the actor word & 0x7FFF, and
    # 0x18428 runs when the id is outside [lo,hi) and not 0x1E1. Returns 1 when the call is made
    # (the return is port-only; mask 0). 0x18428 runs real: the row compares its call and args.
    Spec("fighter_18460", 0x18460, [
        Case("h0", {"eax": 0}, {0x1077A8: le32(0), 0x1077AC: le32(0x10AF80)}),
        Case("h1", {"eax": 0}, {0x1077A8: le32(0x10AF00), 0x1077AC: le32(0)}),
        Case("h2", {"eax": 0}, c3c_46_case(0, 0x0015)),
        Case("h3", {"eax": 0}, c3c_46_case(0, 0x0030)),
        Case("h4", {"eax": 0}, c3c_46_case(7, 0x0100, lo=0x0001, hi=0x7FFF)),
        Case("h5", {"eax": 0}, c3c_46_case(0, 0x01E1)),
        Case("h6", {"eax": 1}, c3c_46_case(0, 0x0000, id1=0x0060)),
        Case("h7", {"eax": 0}, c3c_46_case(0, 0x0010)),
        Case("h8", {"eax": 0}, c3c_46_case(0, 0x0020)),
        Case("h9", {"eax": 0}, c3c_46_case(0, 0x0005)),
        Case("ha", {"eax": 0}, c3c_46_case(0, 0x8123, lo=0x2000, hi=0x3000)),
    ], calls=(E.Call(0x18428, ("eax", "edx", "ebx", "ecx"), mode="real"),),
       eax_mask=0, mutants=("@null", "@null2", "@lo", "@hi", "@range", "@e1", "@call", "@id")),
    # 0x367DC fighter_state_367dc: EAX = slot, EDX = rec. Restart the record's animation at
    # 0xC8950[slot+0x7A] (float 3.0), clear the slot/record fields, mask slot+0x40, then in modes
    # other than 3/0x22/0x24 a second animation (0x102900[rec+0x51], 0xE906A, 1.0). Void.
    Spec("fighter_state_367dc", 0x367DC, [
        Case("d0", {"eax": C3C_R_N1, "edx": C3C_R_N2},
             {C3C_R_N1 + 0x7A: b"\x02", C3C_R_N2 + 0x4C: b"\xAA", C3C_R_N2 + 0x4D: b"\xBB",
              C3C_R_N1 + 0x52: b"\x01", C3C_R_N1 + 0x53: b"\x02", C3C_R_N1 + 0x5F: b"\x01",
              C3C_R_N1 + 0x55: b"\x02", C3C_R_N1 + 0x54: b"\x03",
              C3C_R_N1 + 0x40: le32(0xFFFFFFFF), 0x104B00: le16(3)}),
        Case("d1", {"eax": C3C_R_N1, "edx": C3C_R_N2},
             {C3C_R_N1 + 0x7A: b"\x02", C3C_R_N2 + 0x4C: b"\xAA", C3C_R_N2 + 0x4D: b"\xBB",
              C3C_R_N1 + 0x52: b"\x01", C3C_R_N1 + 0x53: b"\x02", C3C_R_N1 + 0x5F: b"\x01",
              C3C_R_N1 + 0x55: b"\x02", C3C_R_N1 + 0x54: b"\x03",
              C3C_R_N1 + 0x40: le32(0xFFFFFFFF), 0x104B00: le16(0x22)}),
        Case("d2", {"eax": C3C_R_N1, "edx": C3C_R_N2},
             {C3C_R_N1 + 0x7A: b"\x02", C3C_R_N2 + 0x4C: b"\xAA", C3C_R_N2 + 0x4D: b"\xBB",
              C3C_R_N1 + 0x52: b"\x01", C3C_R_N1 + 0x53: b"\x02", C3C_R_N1 + 0x5F: b"\x01",
              C3C_R_N1 + 0x55: b"\x02", C3C_R_N1 + 0x54: b"\x03",
              C3C_R_N1 + 0x40: le32(0xFFFFFFFF), 0x104B00: le16(0x24)}),
        Case("d3", {"eax": C3C_R_N1, "edx": C3C_R_N2},
             {C3C_R_N1 + 0x7A: b"\x05", C3C_R_N2 + 0x4C: b"\xAA", C3C_R_N2 + 0x4D: b"\xBB",
              C3C_R_N1 + 0x52: b"\x01", C3C_R_N1 + 0x53: b"\x02", C3C_R_N1 + 0x5F: b"\x01",
              C3C_R_N1 + 0x55: b"\x02", C3C_R_N1 + 0x54: b"\x03",
              C3C_R_N1 + 0x40: le32(0xFFFFFFFF), 0x104B00: le16(0),
              C3C_R_N2 + 0x51: b"\x01", 0x102900: le32(0x10B0A0), 0x102904: le32(0x10B0B0)}),
        Case("d4", {"eax": C3C_R_N1, "edx": C3C_R_N2},
             {C3C_R_N1 + 0x7A: b"\x05", C3C_R_N2 + 0x4C: b"\xAA", C3C_R_N2 + 0x4D: b"\xBB",
              C3C_R_N1 + 0x52: b"\x01", C3C_R_N1 + 0x53: b"\x02", C3C_R_N1 + 0x5F: b"\x01",
              C3C_R_N1 + 0x55: b"\x02", C3C_R_N1 + 0x54: b"\x03",
              C3C_R_N1 + 0x40: le32(0xFFFFFFFF), 0x104B00: le16(0x23),
              C3C_R_N2 + 0x51: b"\x00", 0x102900: le32(0x10B0A0), 0x102904: le32(0x10B0B0)}),
    ], calls=(E.Call(0x2BC30, ("eax", "edx", "s0"), pop=4, clobbers=("edx",)),),
       eax_mask=0, mutants=("@mode", "@stream", "@ch", "@clr4c", "@clr53", "@ff", "@mask", "@call")),
    # 0x1CA40 snd_music_playing: no args; AL = 1 when [0x1028C0] (the sequence handle) is non-zero
    # and 0x5DEED (the AIL status, stubbed through the port's seam) returns 4; [0x1028C0] == 0
    # returns 0 without the call. The raw masks the result to AL (`and eax,0xff`).
    Spec("snd_music_playing", 0x1CA40, [
        Case("m0", {}, {0x001028C0: le32(0)}, {0x5DEED: 4}),
        Case("m1", {}, {0x001028C0: le32(0x10B0C0)}, {0x5DEED: 4}),
        Case("m2", {}, {0x001028C0: le32(0x10B0C0)}, {0x5DEED: 3}),
        Case("m3", {}, {0x001028C0: le32(0x10B0C0)}, {0x5DEED: 0x104}),
    ], calls=(E.Call(0x5DEED, (), mode="stub"),),
       eax_mask=0xFF, mutants=("@zero", "@one", "@gate", "@eq3", "@call", "@eax")),
    # 0x33714 palette_record_flagged (EBX = ptr, EAX = first, EDX = count): append the record
    # { ptr; first; count; flag = 1 } at the head [0x107798] and advance it by 0x10. The pre-state
    # seeds every record byte with a distinct nonzero value, and v1's head carries the +0x10 bump
    # into its byte 1 (0x1075F8 -> 0x107608): every store is observable — the flag stores are
    # single-byte writes to head+0x0C, the ptr/first/count dword stores change record bytes, and
    # the head-advance dword store changes bytes 0-1 through the carry (the C3c sweep: the flag
    # byte's own pre was 0, so the flag store at 0x3371F/0x3373F changed nothing and a dropped
    # flag store passed both palette rows).
    Spec("palette_record_flagged", 0x33714, [
        Case("v0", {"eax": 0x00001234, "edx": 0x00005678, "ebx": 0x00009ABC},
             {0x00107798: le32(0x00107498),
              0x00107498: b"\x91\x92\x93\x94\x95\x96\x97\x98\x99\x9A\x9B\x9C\x9D\x9E\x9F\xA0",
              0x001074A8: le32(0x11111111)}),
        Case("v1", {"eax": 0xDEADBEEF, "edx": 0x00000002, "ebx": 0x00107798},
             {0x00107798: le32(0x001075F8),
              0x001075F8: b"\xB1\xB2\xB3\xB4\xB5\xB6\xB7\xB8\xB9\xBA\xBB\xBC\xBD\xBE\xBF\xC0",
              0x00107608: le32(0x44444444)}),
    ], eax_mask=0, mutants=("@flag", "@order", "@adv", "@wide", "@swap")),
    # 0x33734 palette_record (EBX = ptr, EAX = first, EDX = count): the same append with flag 0.
    Spec("palette_record", 0x33734, [
        Case("w0", {"eax": 0x00001234, "edx": 0x00005678, "ebx": 0x00009ABC},
             {0x00107798: le32(0x00107498),
              0x00107498: b"\x91\x92\x93\x94\x95\x96\x97\x98\x99\x9A\x9B\x9C\x9D\x9E\x9F\xA0",
              0x001074A8: le32(0x11111111)}),
        Case("w1", {"eax": 0xDEADBEEF, "edx": 0x00000002, "ebx": 0x00107798},
             {0x00107798: le32(0x001075F8),
              0x001075F8: b"\xB1\xB2\xB3\xB4\xB5\xB6\xB7\xB8\xB9\xBA\xBB\xBC\xBD\xBE\xBF\xC0",
              0x00107608: le32(0x44444444)}),
    ], eax_mask=0, mutants=("@flag", "@order", "@adv", "@wide", "@swap")),
    # 0x2BD44 fighter_2bd44: EAX = param_1 (the fighter record), EDX = param_2 (the 0x1014F4 row).
    # Copy param_2's +0x4B into param_1, re-arm param_2 (clear +0x24, +0x2A bit 3, +0x28 bits 2/4;
    # set +0x29 bit 3; +8 = 0x1E1), load its sprite id through 0x2A408 (stub, per-case EAX) into its
    # actor word, then 0x2B150 (stub). Void.
    Spec("fighter_2bd44", 0x2BD44, [
        Case("m0", {"eax": 0x10AF00, "edx": 0x10B000},
             {0x10AF00 + 0x4B: b"\x11", 0x10B000 + 0x4B: b"\xA7", 0x10B000 + 0x24: le32(0xDEADBEEF),
              0x10B000 + 0x2A: b"\xFF", 0x10B000 + 0x29: b"\x00", 0x10B000 + 0x28: b"\xFF",
              0x10B000 + 8: le32(0x99999999), 0x10B000 + 0x56: le16(3),
              0x001014EC: le32(0x10AF80), 0x10AFF0: le16(0x5A5A)}, {0x2A408: 0xBEEF}),
        Case("m1", {"eax": 0x10AF00, "edx": 0x10B000},
             {0x10AF00 + 0x4B: b"\x00", 0x10B000 + 0x4B: b"\x3C", 0x10B000 + 0x24: le32(0x00000000),
              0x10B000 + 0x2A: b"\x08", 0x10B000 + 0x29: b"\xFF", 0x10B000 + 0x28: b"\x00",
              0x10B000 + 8: le32(0x11111111), 0x10B000 + 0x56: le16(0),
              0x001014EC: le32(0x10AF80), 0x10AF80: le16(0x7FFF)}, {0x2A408: 0x1234}),
    ], calls=(E.Call(0x2A408, ("eax", "edx"), mode="stub", clobbers=("edx",)),
              E.Call(0x2B150, ("eax",), mode="stub")),
       eax_mask=0, mutants=("@src", "@clr24", "@a2a", "@o29", "@a28", "@id", "@idx")),
    # 0x3B6C4 fighter_3b6c4: EAX = side. 1 when the side's slot +0x53 == 8 and +0x54 == 2, the
    # other slot's +0x54 != 2, and both sides' 0x1A570 actor-bit-15 predicates agree. The 0x33950
    # ctx builder and the 0x1A570 calls are the row's; AL only (`mov al,1`/`xor al,al`).
    Spec("fighter_3b6c4", 0x3B6C4, [
        Case("b0", {"eax": 0}, c3c_b6_case(0, 8, 2, 0, 0, 0)),
        Case("b1", {"eax": 0}, c3c_b6_case(0, 7, 2, 0, 0, 0)),
        Case("b2", {"eax": 0}, c3c_b6_case(0, 8, 3, 0, 0, 0)),
        Case("b3", {"eax": 0}, c3c_b6_case(0, 8, 2, 2, 0, 0)),
        Case("b4", {"eax": 0}, c3c_b6_case(0, 8, 2, 0, 1, 0)),
        Case("b5", {"eax": 1}, c3c_b6_case(1, 8, 2, 0, 1, 1)),
    ], allow_calls=(0x33950,),
       calls=(E.Call(0x1A570, ("eax",), mode="real"),),
       eax_mask=0xFF, mutants=("@s53", "@s54", "@o54", "@cmp", "@ret", "@side")),
    # 0x3C520 hit_anim_start_c: EAX = rec, EDX = stream, s0 = frame bits. The ctx (0x339AC) gives
    # the side's slot; 0x2BC30 (stub) starts the animation; 0x188DC/0x1890C (stubs) take the slot's
    # +0x2C/+0x30 as x/y. Void.
    Spec("hit_anim_start_c", 0x3C520, [
        Case("c0", {"eax": 0x10AF00, "edx": 0x00E12345, "s0": 0x3F800000},
             {0x10AF00 + 0x51: b"\x00", 0x001077B0 + 0x2C: le32(0x11111111),
              0x001077B0 + 0x30: le32(0x22222222)}),
        Case("c1", {"eax": 0x10AF40, "edx": 0x00E56789, "s0": 0x40400000},
             {0x10AF40 + 0x51: b"\x01", 0x001077B0 + 0x94 + 0x2C: le32(0x33333333),
              0x001077B0 + 0x94 + 0x30: le32(0x44444444)}),
        Case("c2", {"eax": 0x10AF80, "edx": 0x80000000, "s0": 0x00000000},
             {0x10AF80 + 0x51: b"\x00", 0x001077B0 + 0x2C: le32(0xFFFF8000),
              0x001077B0 + 0x30: le32(0x00008000)}),
    ], allow_calls=(0x339AC,),
       calls=(E.Call(0x2BC30, ("eax", "edx", "s0"), pop=4, clobbers=("edx",)),
              E.Call(0x188DC, ("eax", "edx"), mode="stub", clobbers=("edx",)),
              E.Call(0x1890C, ("eax", "edx"), mode="stub", clobbers=("edx",))),
       eax_mask=0, mutants=("@side", "@xy", "@begin", "@bits", "@anchor", "@stream")),
    # 0x1A734 fighter_block_hit: EAX = side. The ctx (0x33A10) gives the side's slot; +0x61 = 0x0C,
    # lowered to +0x60 when greater and +0x62 set; +0x43 bit 0x20 picks the +0x54 = 0 arm with the
    # 0xC8F40[ch] stream, bit 0x10 the +0x54 = 1 arm with 0xC8F90[ch]; both end in 0x3C480(rec,
    # stream, 3.0). 0x18B04 and 0x3C480 are stubs (the latter pops its stack arg). Void.
    Spec("fighter_block_hit", 0x1A734, [
        Case("k0", {"eax": 0}, c3c_k_case(0, 0x05, 0x01, 0x20, 0x02)),
        Case("k1", {"eax": 0}, c3c_k_case(0, 0x20, 0x01, 0x10, 0x02)),
        Case("k2", {"eax": 0}, c3c_k_case(0, 0x20, 0x01, 0x00, 0x02)),
        Case("k3", {"eax": 1}, c3c_k_case(1, 0x05, 0x01, 0x20, 0x05)),
        Case("k4", {"eax": 0}, c3c_k_case(0, 0x00, 0x00, 0x20, 0x02)),
        Case("k5", {"eax": 0}, c3c_k_case(0, 0x05, 0x01, 0x30, 0x02)),
    ], allow_calls=(0x33A10,),
       calls=(E.Call(0x18B04, ("eax",), mode="stub"),
              E.Call(0x3C480, ("eax", "edx", "s0"), pop=4, clobbers=("edx",))),
       eax_mask=0, mutants=("@c61", "@clamp", "@b62", "@u", "@arm", "@s54", "@call", "@tab")),
    # 0x39738 fighter_39738: EAX = side, EDX = b. The 0x33950 ctx gives the side's and the other's
    # slots. When the side's slot+0x8C is non-zero the per-character base is 0xBEC58[0x107D2A[side]]
    # (0xBEC84 above 0xB) * b / 100, else 0xBEC28[k] * b / 100, or with k > 0xB: b/4 when
    # 0x107D1E[side] > 0x46, else 0xBEC54 * b / 100. Then the other slot's +0x8C cuts 15% and a
    # non-zero side slot+0x63 adds 0xBEBD8[0x1082C8[side]] * v / 100. Full EAX (the dword result).
    Spec("fighter_39738", 0x39738, [
        Case("r0", {"eax": 0, "edx": 100}, c3c_97_case(0, 1, 0, 5, 100)),
        Case("r1", {"eax": 0, "edx": 100}, c3c_97_case(0, 1, 0, 11, 100)),
        Case("r2", {"eax": 0, "edx": 100}, c3c_97_case(0, 1, 0, 12, 100)),
        Case("r3", {"eax": 0, "edx": 200}, c3c_97_case(0, 0, 0, 5, 200)),
        Case("r4", {"eax": 0, "edx": 200}, c3c_97_case(0, 0, 0, 12, 200, k2=0x46)),
        Case("r5", {"eax": 0, "edx": 0xFFFFFF9B}, c3c_97_case(0, 0, 0, 12, 0xFFFFFF9B, k2=0x47)),
        Case("r6", {"eax": 1, "edx": 0xFFFFFF9C}, c3c_97_case(1, 1, 1, 12, 0xFFFFFF9C, k2=0x47, s63=1, d=2)),
        Case("r7", {"eax": 1, "edx": 101}, c3c_97_case(1, 0, 1, 3, 101, s63=1, d=2)),
    ], allow_calls=(0x33950,), eax_mask=0xFFFFFFFF,
       mutants=("@chan", "@k", "@tab", "@k2", "@div", "@cut", "@diff", "@idx")),
]

# ---- track P batch C3d (record 2026-10-05-reverse-c3d): the type-family rows, part 4 ------

C3D_REC0, C3D_REC1 = 0x10AF00, 0x10AF40   # the two scratch records (C3c's convention)
C3D_ACTOR = 0x10B000                      # the actor pool DS_001014EC points at


def c3d_slot_pokes():
    """The 0x1077A8 pointer pair and both slots' record pointers, one poke each (capped at 64
    bytes). The case helpers add per-slot field pokes, contiguous where the fields are."""
    return {0x001077A8: le32(C3D_REC0) + le32(C3D_REC1) + le32(C3D_REC0),
            0x001077B0 + 0x94: le32(C3D_REC1)}


def c3d_4f_case(sel, v_sel, v_opp, dx):
    """0x4F434: the selector byte, the two slot +0x5A bytes and the dx pair. The counter dword at
    0x1082C8 + opp*4 is 3 and 0x1082C0 + opp*4 is 3 - dx, so dx is exact while the +-1 nudge stays
    inside the 5-cap of the 0xC9408[3] clamp."""
    opp = 1 - sel
    return {0x0010810D: bytes([sel]),
            0x001077B0 + sel * 0x94 + 0x5A: bytes([v_sel & 0xFF]),
            0x001077B0 + opp * 0x94 + 0x5A: bytes([v_opp & 0xFF]),
            0x001082C8 + sel * 4: le32(0x10),
            0x001082C0 + sel * 4: le32(0),
            0x001082C8 + opp * 4: le32(3),
            0x001082C0 + opp * 4: le32(3 - dx),
            0x0010452C: b"\x03", 0x001082D0: le32(0)}


def c3d_85_case(side, s54):
    """0x385B0: the side's slot +0x54 and every field the body writes, seeded with sentinels."""
    p = c3d_slot_pokes()
    so = 0x001077B0 + side * 0x94
    rec = C3D_REC0 + side * 0x40
    p[so + 0x0C] = le32(0x33333333) + le32(0x44444444) + le32(0x55555555) + le32(0x66666666) \
        + le32(0x77777777)                    # +0x0C..+0x1F: every dword the body zeroes is
                                              # sentinelled; +0x14 is the surviving neighbour
                                              # (the Task 3 sweep: +0x1C's pre was the image's 0,
                                              # so the 0x38693 store changed nothing)
    p[so + 0x40] = le32(0xFFFFFFFF)
    sb = bytearray(0x17)                      # slot +0x52 .. +0x68
    sb[0x00] = 0x11; sb[0x01] = 0x22; sb[0x02] = s54; sb[0x03] = 0x55
    sb[0x0D] = 0x11; sb[0x10] = 0x7F; sb[0x13] = 0x88; sb[0x15] = 0x77; sb[0x16] = 0xAA
    p[so + 0x52] = bytes(sb)
    p[so + 0x74] = le32(0x99999999) + b"\x00\x00\x03"
    p[so + 0x84] = le32(0x11111111) + b"\x00\x00\x7F"
    rb = bytearray(0x26)                      # rec+0x28 .. rec+0x4D
    rb[0x00] = 0xFF                           # +0x28
    rb[0x0C:0x0E] = le16(0xDDDD)              # +0x34
    rb[0x0E:0x10] = le16(0xEEEE)              # +0x36
    rb[0x1A] = 0x55; rb[0x1B] = 0xCC          # +0x42, +0x43
    rb[0x1C:0x20] = le32(0xBBBBBBBB)          # +0x44
    rb[0x24] = 0x99; rb[0x25] = 0x88          # +0x4C, +0x4D
    p[rec + 0x28] = bytes(rb)
    p[rec + 0x1C] = le32(0x0F0F0F0F)
    p[rec + 0x51] = bytes([side, 0, 0, 0, 0]) + le16(1)
    p[0x001014EC] = le32(C3D_ACTOR)
    p[C3D_ACTOR] = le16(0x8000)
    p[0x00100AF8 + side * 4] = le32(0xA5A5A5A5)
    return p


def c3d_6b_case(side, s43, mode, d2c_other):
    """0x36BC8: the side slot +0x43, the mode word and the other side's 0x107D2C word."""
    p = c3d_slot_pokes()
    so = 0x001077B0 + side * 0x94
    rec = C3D_REC0 + side * 0x40
    p[so + 0x43] = bytes([s43])
    p[so + 0x52] = bytes([0x33, 0x44, 0x66, 0x77])
    p[so + 0x5D] = b"\x77"
    p[so + 0x74] = le32(0xAAAA5555) + b"\x00\x00\x03"
    p[rec + 0x18] = le32(0x11223344)
    p[rec + 0x51] = bytes([side, 0, 0, 0, 0]) + le16(1)
    p[0x001014EC] = le32(C3D_ACTOR)
    p[C3D_ACTOR] = le16(0x8000)
    p[0x00107D2C + (1 - side) * 2] = le16(d2c_other)
    p[0x00104B00] = le16(mode)
    return p


def c3d_79_case(fe, s57, b41, e8):
    """0x379C4: the 0x1078FE gate, the slot's +0x57/+0x41, and the 0x1078E8 callback pointer."""
    p = c3d_slot_pokes()
    p[0x001077B0 + 0x57] = bytes([s57])
    p[0x001077B0 + 0x41] = bytes([b41])
    p[0x001077B0 + 0x7A] = b"\x03"
    p[C3D_REC0 + 0x51] = b"\x00\x00\x00\x00\x00" + le16(1)
    p[0x001078FE] = bytes([fe])
    p[0x001078E8] = le32(e8)
    return p


def c3d_ae_case(side, s53, w34, w36, s40, k44, bit15):
    """0x3AE9C: the side's slot +0x53/+0x40/+0x7A, the record's +0x34/+0x36/+0x44 word (the k the
    raw reads is the word at +0x44, the dword at +0x42 shifted) and the side's actor bit 15."""
    p = c3d_slot_pokes()
    so = 0x001077B0 + side * 0x94
    rec = C3D_REC0 + side * 0x40
    p[so + 0x53] = bytes([s53])
    p[so + 0x40] = bytes([s40])
    p[so + 0x7A] = b"\x03"
    p[rec + 0x34] = le16(w34) + le16(w36)
    p[rec + 0x44] = le16(k44)
    p[rec + 0x51] = bytes([side, 0, 0, 0, 0]) + le16(1)
    p[0x001014EC] = le32(C3D_ACTOR)
    p[C3D_ACTOR + 1 * 0x20] = le16(bit15 << 15)
    return p


def c3d_b0_case(side, d2, bit15_other, x):
    """0x3B080: the 0xBEDF2 gate, the other side's actor bit 15 (0x1A570), the side's +0x2C x and
    the 0xBE018/0xBEDEE window 0x3B038 reads."""
    p = c3d_slot_pokes()
    so = 0x001077B0 + side * 0x94
    oo = 0x001077B0 + (1 - side) * 0x94
    rec = C3D_REC0 + side * 0x40
    orec = C3D_REC0 + (1 - side) * 0x40
    p[so + 0x34] = b"\x11" + b"\x00" * 0x0E + b"\x22"
    p[oo + 0x34] = b"\x33" + b"\x00" * 0x0E + b"\x44"
    p[so + 0x2C] = le32(x)
    p[rec + 0x51] = bytes([side, 0, 0, 0, 0]) + le16(1)
    p[orec + 0x56] = le16(2)
    p[0x001014EC] = le32(C3D_ACTOR)
    p[C3D_ACTOR + (1 + (1 - side)) * 0x20] = le16(bit15_other << 15)
    p[0x000BEDF2] = bytes([d2])
    p[0x000BE018] = le32(0x100)
    p[0x000BEDEE] = le32(0x50 << 16)
    return p


def c3d_ad_case(side, key, d5a, dplus):
    """0x3AD98: EAX = side, EDX = 0x10B100 (the triple); anim[0] -> 0x10B200 (its +4/+5/+9
    bytes), anim[2] -> 0x10B280 (the first word selects the stream). The side record's +0x30 high
    word is 5 and 0x100AD8[other] is 2."""
    p = c3d_slot_pokes()
    so = 0x001077B0 + side * 0x94
    rec = C3D_REC0 + side * 0x40
    p[0x0010A000] = le32(0x10A400) + le32(0x10A420) + le32(0x10A440)
    p[0x0010A400 + 4] = bytes([dplus, 5, 0, 0, 0, 9])
    p[0x0010A440] = le16(key)
    p[so + 0x5A] = bytes([d5a])
    p[so + 0x41] = b"\x11"
    p[so + 0x7A] = b"\x03"
    p[rec + 0x30] = le32(0x00050000)
    p[0x00100AD8 + (1 - side) * 4] = le32(2)
    p[0x000F0AEC] = le32(0x1000)
    return p


def c3d_im_case(side, s53, s54, o5f, o64, orec34, self2c, other2c, ring, word):
    """0x1AB5C: the side slot's +0x53/+0x54/+0x43/+0x2C, the other's +0x5F/+0x64/+8 and its
    record's +0x34, the command word 0x1088E0[side] and the ring stub's word (the spec's
    stub_eax)."""
    p = c3d_slot_pokes()
    so = 0x001077B0 + side * 0x94
    oo = 0x001077B0 + (1 - side) * 0x94
    orec = C3D_REC0 + (1 - side) * 0x40
    p[so + 0x53] = bytes([s53])
    p[so + 0x54] = bytes([s54])
    p[so + 0x43] = b"\xFF"
    p[so + 0x2C] = le32(self2c)
    p[oo + 0x2C] = le32(other2c)
    p[oo + 0x5F] = bytes([o5f])
    p[oo + 0x64] = bytes([o64])
    p[oo + 0x08] = le32(orec if o5f == 0xFF else 0)
    p[orec + 0x34] = le16(orec34)
    wb = bytearray(4)
    wb[side * 2:side * 2 + 2] = le16(word)
    p[0x001088E0] = bytes(wb)
    return p


def c3d_cm_case(side, s63, s53, s54, self2c, other2c, o5f, o64, orec34, thr, animbits):
    """0x3B134: the side slot's +0x63/+0x53/+0x54/+0x2C, the other's +0x5F/+0x64/+8 and its
    record's +0x34, the rng threshold word 0xBEDF4 and the anim[2]+2 byte at 0xA672A (char 0,
    edx_arg 0; the 0x3AFC4 allow computes the same triple on both sides)."""
    p = c3d_slot_pokes()
    so = 0x001077B0 + side * 0x94
    oo = 0x001077B0 + (1 - side) * 0x94
    orec = C3D_REC0 + (1 - side) * 0x40
    p[so + 0x63] = bytes([s63])
    p[so + 0x53] = bytes([s53])
    p[so + 0x54] = bytes([s54])
    p[so + 0x2C] = le32(self2c)
    p[oo + 0x2C] = le32(other2c)
    p[oo + 0x5F] = bytes([o5f])
    p[oo + 0x64] = bytes([o64])
    p[oo + 0x08] = le32(orec)
    p[orec + 0x34] = le16(orec34)
    p[0x0010452C] = b"\x00"
    p[0x001082C8 + side * 4] = le32(0)
    p[0x000BEDF4] = le16(thr)
    p[0x000A672A] = bytes([animbits])
    return p


def c3d_28_case(rec28, rec2c=0, rec40=0x1000, rec2a=0, bdc=0, be0=0, s49=0x09, s59=5):
    """0x2A820: EAX = rec (0x10AF00), EDX = pset (0x10AA00); the pools at 0x10A800/0x10A900 with
    one 0x68-byte parent record and its 0x20-byte pset; DS_00105BDC/BE0 seeded for the window."""
    rb = bytearray(0x5A)                      # rec 0x10AF00 .. +0x59
    rb[0x18:0x1C] = le32(0x11111111); rb[0x1C:0x20] = le32(0x22222222)
    rb[0x28:0x2A] = le16(rec28); rb[0x2A:0x2C] = le16(rec2a)
    rb[0x2C:0x2E] = le16(rec2c)
    rb[0x32:0x36] = le32(0x00030000); rb[0x34:0x38] = le32(0x00020000)
    rb[0x40:0x42] = le16(rec40)
    rb[0x49] = s49; rb[0x4A] = 0x01; rb[0x4B] = 0x02; rb[0x59] = s59
    pb = bytearray(0x38)                      # parent record 0x10A800 .. +0x57
    pb[0x18:0x1C] = le32(0x33333333); pb[0x1C:0x20] = le32(0x44444444)
    pb[0x2C:0x2E] = le16(0x1234); pb[0x30:0x34] = le32(0x00010000)
    pb[0x56:0x58] = le16(0)
    pp = bytearray(0x10)                      # parent pset 0x10A900 .. +0x0F
    pp[4:8] = le32(0x100); pp[8:12] = le32(0x200); pp[0x0E:0x10] = le16(7)
    qb = bytearray(0x10)                      # the caller's pset 0x10AA00 .. +0x0F
    qb[0:4] = le32(0xAAAA5555); qb[4:8] = le32(0xBBBBBBBB)
    qb[8:12] = le32(0xCCCCCCCC); qb[0x0C:0x0E] = le16(0xDDDD)
    return {0x001014EC: le32(0x10A900), 0x001014F4: le32(0x10A800),
            0x00105BDC: le32(bdc), 0x00105BE0: le32(be0),
            0x10AF00: bytes(rb[:0x30]), 0x10AF30: bytes(rb[0x30:0x50]),
            0x10AF50: bytes(rb[0x50:]),
            0x10A800: bytes(pb), 0x10A900: bytes(pp), 0x10AA00: bytes(qb),
            0x10A8D0: b"\x00" * 0x30 + le32(0x00090000), 0x10A8D0 + 0x56: le16(3)}


C3D_SPECS = [
    # 0x4F434 fighter_4f434: the AI difficulty nudge. No args; the 0x46534 callee runs on both
    # sides (allow, unrecorded), so the accumulator's byte diff is the observation.
    Spec("fighter_4f434", 0x4F434, [
        Case("f0", {}, c3d_4f_case(0, 0x3C, 0x00, 0)),    # scaled 50: -1 when dx == 0
        Case("f1", {}, c3d_4f_case(0, 0x3C, 0x00, 1)),    # dx != 0 -> return
        Case("f2", {}, c3d_4f_case(0, 0x10, 0x00, 0)),    # scaled 13 -> return
        Case("f3", {}, c3d_4f_case(0, 0x00, 0x30, 0)),    # scaled -40: +1 when dx == 0
        Case("f4", {}, c3d_4f_case(0, 0x00, 0x30, 2)),    # dx != 0 -> return
        Case("f5", {}, c3d_4f_case(0, 0x00, 0x60, 0)),    # scaled -80: +1 when dx <= 1
        Case("f6", {}, c3d_4f_case(0, 0x00, 0x60, 3)),    # dx > 1 -> return
        Case("f7", {}, c3d_4f_case(1, 0x3C, 0x00, 0)),    # the other selector
        Case("f8", {}, c3d_4f_case(0, 0x3B, 0x00, 0)),    # scaled 49 -> return (the threshold)
    ], allow_calls=(0x46534,), eax_mask=0,
       mutants=("@sel", "@scale", "@thr", "@dx", "@delta", "@call")),
    # 0x385B0 fighter_385b0: the mode-0x25 slot reset. EAX = rec; 0x164E8, the 0xC8950 animation
    # and 0x38154 are stubs; the +0x54 == 5 arm runs 0x38154, every other value the animation.
    Spec("fighter_385b0", 0x385B0, [
        Case("b0", {"eax": C3D_REC0}, c3d_85_case(0, 0)),
        Case("b1", {"eax": C3D_REC0}, c3d_85_case(0, 1)),
        Case("b2", {"eax": C3D_REC0}, c3d_85_case(0, 2)),
        Case("b3", {"eax": C3D_REC0}, c3d_85_case(0, 5)),
        Case("b4", {"eax": C3D_REC1}, c3d_85_case(1, 2)),
    ], calls=(E.Call(0x164E8, ("eax",), mode="stub"), ANIM_BEGIN,
              E.Call(0x38154, ("eax",), mode="stub")),
       eax_mask=0, mutants=("@af8", "@arm", "@five", "@anim", "@tail", "@call")),
    # 0x3AE9C fighter_3ae9c: EAX = side, DL = param_2. EAX mask 0 (void). The 0x1A570 predicate
    # runs real (its row exists); 0x33950 is an allow.
    Spec("fighter_3ae9c", 0x3AE9C, [
        Case("e0", {"eax": 0, "edx": 0}, c3d_ae_case(0, 7, 0, 0, 0, 0, 0)),
        Case("e1", {"eax": 0, "edx": 0}, c3d_ae_case(0, 2, 0, 1, 0, 0, 0)),      # g=1, sign 0 -> flip
        Case("e2", {"eax": 0, "edx": 1}, c3d_ae_case(0, 2, 0, 0, 0x80, 0, 1)),   # bit set, +0x44=0x28
        Case("e3", {"eax": 0, "edx": 0}, c3d_ae_case(0, 2, 0, 0, 0x80, 0, 1)),   # +0x44=0x3C
        Case("e4", {"eax": 0, "edx": 1}, c3d_ae_case(0, 2, 5, 0, 0, 0x40, 1)),   # k>0x1E
        Case("e5", {"eax": 0, "edx": 0}, c3d_ae_case(0, 2, 0xFFFF, 0, 0, 0x10, 1)),  # k<0x1A
        Case("e6", {"eax": 0, "edx": 0}, c3d_ae_case(0, 2, 0, 0, 0, 0x1C, 1)),   # k in range
        Case("e7", {"eax": 0, "edx": 0}, c3d_ae_case(0, 2, 0xFF9C, 0, 0, 0, 0)),  # sign -1 multiply
        Case("e8", {"eax": 1, "edx": 1}, c3d_ae_case(1, 2, 0, 0, 0x80, 0, 0)),    # the other side
    ], allow_calls=(0x33950,), calls=(E.Call(0x1A570, ("eax",), mode="real"),),
       eax_mask=0, mutants=("@s53", "@g", "@sign", "@tab", "@u44", "@k", "@bit")),
    # 0x3B080 fighter_3b080: EAX = side, EDX = param_2, EBX = param_3, ECX = param_4. The 0x1A570
    # predicate runs real, 0x3C148 is a stub; 0x3B038 is an allow (leaf).
    Spec("fighter_3b080", 0x3B080, [
        Case("b0", {"eax": 0, "edx": 3, "ebx": 0x11, "ecx": 1}, c3d_b0_case(0, 1, 0, 0x64)),
        Case("b1", {"eax": 0, "edx": 3, "ebx": 0x11, "ecx": 0}, c3d_b0_case(0, 0, 0, 0x64)),
        Case("b2", {"eax": 0, "edx": 3, "ebx": 0x11, "ecx": 0}, c3d_b0_case(0, 0, 1, 0x200)),
        Case("b3", {"eax": 0, "edx": 3, "ebx": 0x11, "ecx": 1}, c3d_b0_case(0, 0, 0, 0x200)),
        Case("b4", {"eax": 1, "edx": 4, "ebx": 0x22, "ecx": 1}, c3d_b0_case(1, 0, 0, 0x200)),
    ], allow_calls=(0x3B038,), calls=(E.Call(0x1A570, ("eax",), mode="real"),
                                      E.Call(0x3C148, ("eax",), mode="stub")),
       eax_mask=0, mutants=("@gate", "@neg", "@mirror", "@p4", "@c148", "@o43")),
    # 0x36BC8 fighter_state_36bc8: EAX = slot, EDX = rec. 0x38BC8 is a stub, 0x38BB0 an allow
    # (leaf), the 0x2BC30 animation and 0x188AC anchor stubs. Returns 7.
    Spec("fighter_state_36bc8", 0x36BC8, [
        Case("c0", {"eax": 0x001077B0, "edx": C3D_REC0}, c3d_6b_case(0, 4, 0, 0)),
        Case("c1", {"eax": 0x001077B0, "edx": C3D_REC0}, c3d_6b_case(0, 4, 0, 2)),
        Case("c2", {"eax": 0x001077B0, "edx": C3D_REC0}, c3d_6b_case(0, 0, 0, 2)),
        Case("c3", {"eax": 0x001077B0, "edx": C3D_REC0}, c3d_6b_case(0, 0, 3, 2)),
        Case("c4", {"eax": 0x001077B0, "edx": C3D_REC0}, c3d_6b_case(0, 0, 0x22, 2)),
        Case("c5", {"eax": 0x001077B0 + 0x94, "edx": C3D_REC1}, c3d_6b_case(1, 4, 0, 2)),
    ], allow_calls=(0x38BB0,), calls=(E.Call(0x38BC8, ("eax",), mode="stub"), ANIM_BEGIN, ANCHOR),
       eax_mask=0, mutants=("@other", "@cond", "@anim", "@s5d", "@bit", "@tail")),
    # 0x379C4 fighter_379c4: EAX = slot. 0x37178 and the 0xC8950/0x1078E4/0xC9260 animations are
    # in the call set; the 0x1078E8 callback runs as an allow (0x45D14 returns 1, the named 0x5D812
    # returns 0 through the port's NULL-resolve guard, so both sides take the same arm).
    Spec("fighter_379c4", 0x379C4, [
        Case("k0", {"eax": 0x001077B0}, c3d_79_case(0, 2, 0, 0)),
        Case("k1", {"eax": 0x001077B0}, c3d_79_case(0, 3, 0, 0)),
        Case("k2", {"eax": 0x001077B0}, c3d_79_case(1, 0, 0, 0)),
        Case("k3", {"eax": 0x001077B0}, c3d_79_case(1, 0, 2, 0)),
        Case("k4", {"eax": 0x001077B0}, c3d_79_case(1, 0, 2, 0x45D14)),
        Case("k5", {"eax": 0x001077B0}, c3d_79_case(1, 0, 2, 0x5D812)),
    ], allow_calls=(0x45D14, 0x5D812), calls=(E.Call(0x37178, ("eax",), mode="stub", clobbers=("esi", "edi", "ebp")), ANIM_BEGIN),
       eax_mask=0, mutants=("@fe", "@s57", "@b41", "@e8", "@cb", "@fc", "@tab")),
    # 0x3AD98 fighter_3ad98: EAX = side, EDX = the 3-dword animation triple. The voice, the
    # 0x2AE14 effect spawn and the 0x2BC30 animation are stubs; 0x392A0 is a stub (its own row).
    # The spawn's stub EAX is the new actor (0x10B400), so its +0x59/stream writes are compared.
    Spec("fighter_3ad98", 0x3AD98, [
        Case("a0", {"eax": 0, "edx": 0x10A000}, c3d_ad_case(0, 1, 0x10, 0x20),
             {0x2AE14: 0x10A480}),
        Case("a1", {"eax": 0, "edx": 0x10A000}, c3d_ad_case(0, 2, 0x10, 0x20),
             {0x2AE14: 0x10A480}),
        Case("a2", {"eax": 0, "edx": 0x10A000}, c3d_ad_case(0, 3, 0x10, 0x20),
             {0x2AE14: 0x10A480}),
        Case("a3", {"eax": 0, "edx": 0x10A000}, c3d_ad_case(0, 0, 0x10, 0x20), {}),
        Case("a4", {"eax": 0, "edx": 0x10A000}, c3d_ad_case(0, 0, 0x70, 0x20), {}),
        Case("a5", {"eax": 1, "edx": 0x10A000}, c3d_ad_case(1, 1, 0x10, 0x20),
             {0x2AE14: 0x10A480}),
        Case("a6", {"eax": 0, "edx": 0x10A000}, c3d_ad_case(0, 4, 0x10, 0x20), {}),
        Case("a7", {"eax": 0, "edx": 0x10A000}, c3d_ad_case(0, 0, 0x77, 0x20), {}),
    ], allow_calls=(0x33A10,),
       calls=(VOICE, SPAWN, ANIM_BEGIN,
              E.Call(0x392A0, ("eax", "edx", "ebx"), mode="stub", clobbers=("ebx", "edx"))),
       eax_mask=0, mutants=("@voice", "@off", "@tab", "@zero", "@call", "@clamp", "@d", "@bit")),
    # 0x1AB5C fighter_input_mask: EAX = side; the seven 0x46460 ring reads and the command word are
    # ORed; a non-ok state clears +0x43 bit 4 and returns 0, else the facing base and the +0x54
    # store/0x1A7CC arm. The ring stub returns the case's 16-bit word on every read; 0x1AB10 and
    # 0x33A10 are allows.
    Spec("fighter_input_mask", 0x1AB5C, [
        Case("i0", {"eax": 0}, c3d_im_case(0, 0, 0, 0x00, 0x00, 0, 5, 3, 0x1000, 0x0000), {0x46460: 0x1000}),
        Case("i1", {"eax": 0}, c3d_im_case(0, 2, 0, 0x00, 0x00, 0, 5, 3, 0x1000, 0x0000), {0x46460: 0x1000}),
        Case("i2", {"eax": 0}, c3d_im_case(0, 0, 1, 0x00, 0x00, 0, 3, 5, 0x2000, 0x0000), {0x46460: 0x2000}),
        Case("i3", {"eax": 0}, c3d_im_case(0, 1, 1, 0x00, 0x00, 0, 4, 4, 0x3000, 0x0000), {0x46460: 0x3000}),
        Case("i4", {"eax": 0}, c3d_im_case(0, 0, 0, 0xFF, 0x00, 0xFFFF, 4, 4, 0x2000, 0x0000), {0x46460: 0x2000}),
        Case("i5", {"eax": 0}, c3d_im_case(0, 0, 0, 0xFF, 0x00, 0x0000, 4, 4, 0x1000, 0x0000), {0x46460: 0x1000}),
        Case("i6", {"eax": 0}, c3d_im_case(0, 1, 0, 0x00, 0x00, 0, 5, 3, 0x4000, 0x0000), {0x46460: 0x4000}),
        Case("i7", {"eax": 0}, c3d_im_case(0, 0, 0, 0x00, 0x00, 0, 5, 3, 0x0000, 0x0000), {0x46460: 0x0000}),
        Case("i8", {"eax": 1}, c3d_im_case(1, 0, 0, 0x00, 0x00, 0, 5, 3, 0x8000, 0x1000), {0x46460: 0x8000}),
    ], allow_calls=(0x33A10, 0x1AB10),
       calls=(E.Call(0x46460, ("eax", "edx"), mode="stub", clobbers=("edx",)),
              E.Call(0x18B04, ("eax",), mode="stub"),
              E.Call(0x1A7CC, ("eax",), mode="stub", clobbers=("esi", "edi", "ebp"))),
       eax_mask=0xFFFFFFFF, mutants=("@loop", "@word", "@ok", "@face", "@eq", "@bl", "@arm",
                                     "@s54", "@call")),
    # 0x3B134 fight_command_map: EAX = side, EDX = edx_arg, EBX = override. 0x33A10/0x3AFC4/
    # 0x1AB10 are allows; the rng, the readiness gate and the consumer are stubs.
    Spec("fight_command_map", 0x3B134, [
        Case("m0", {"eax": 0, "edx": 0, "ecx": 0}, c3d_cm_case(0, 0, 0, 0, 5, 3, 0x00, 0x00, 0, 50, 0), {0x5D7DC: 100}),
        Case("m1", {"eax": 0, "edx": 0, "ecx": 0}, c3d_cm_case(0, 1, 2, 0, 5, 3, 0x00, 0x00, 0, 50, 0), {0x5D7DC: 100}),
        Case("m2", {"eax": 0, "edx": 0, "ecx": 0}, c3d_cm_case(0, 1, 0, 0, 5, 3, 0x00, 0x00, 0, 50, 0), {0x5D7DC: 100}),
        Case("m3", {"eax": 0, "edx": 0, "ecx": 0}, c3d_cm_case(0, 1, 0, 0, 5, 3, 0x00, 0x00, 0xFFFF, 50, 0), {0x5D7DC: 10}),
        Case("m4", {"eax": 0, "edx": 0, "ecx": 0}, c3d_cm_case(0, 1, 0, 0, 5, 3, 0x00, 0x00, 0x0000, 50, 0), {0x5D7DC: 10}),
        Case("m5", {"eax": 0, "edx": 0, "ecx": 0}, c3d_cm_case(0, 1, 0, 0, 5, 3, 0xFF, 0xFF, 0, 50, 0), {0x5D7DC: 10}),
        Case("m6", {"eax": 0, "edx": 0, "ecx": 0}, c3d_cm_case(0, 1, 0, 0, 5, 3, 0xFF, 0xFF, 0, 50, 1), {0x5D7DC: 10}),
        Case("m7", {"eax": 0, "edx": 0, "ecx": 0}, c3d_cm_case(0, 1, 0, 1, 5, 3, 0xFF, 0xFF, 0, 50, 3), {0x5D7DC: 10, 0x3BDB0: 1, 0x3BDDC: 1}),
        Case("m8", {"eax": 0, "edx": 0, "ecx": 0}, c3d_cm_case(0, 1, 0, 2, 5, 3, 0xFF, 0xFF, 0, 50, 3), {0x5D7DC: 10}),
        Case("m9", {"eax": 1, "edx": 0, "ecx": 0}, c3d_cm_case(1, 1, 0, 0, 5, 3, 0x00, 0x00, 0xFFFF, 50, 0), {0x5D7DC: 10}),
        Case("m10", {"eax": 0, "edx": 0, "ecx": 0}, c3d_cm_case(0, 1, 0, 0, 3, 5, 0x00, 0x00, 0x0000, 50, 0), {0x5D7DC: 10}),
        Case("m11", {"eax": 0, "edx": 0, "ebx": 1}, c3d_cm_case(0, 1, 0, 0, 5, 3, 0x00, 0x00, 0xFFFF, 50, 0), {0x5D7DC: 100}),
    ], allow_calls=(0x33A10, 0x3AFC4, 0x1AB10),
       calls=(E.Call(0x5D7DC, ("eax",), mode="stub"),
              E.Call(0x3BDB0, ("eax",), mode="stub"),
              E.Call(0x3BDDC, ("eax",), mode="stub", clobbers=("ebp",))),
       eax_mask=0, mutants=("@a", "@b", "@r", "@ov", "@bs", "@os", "@b1", "@rd", "@c")),
    # 0x2A820 pset_write: EAX = rec, EDX = pset. The 0x2A690 free-record writer and the 0x2B150
    # (set_dead) dead path are stubs; the parent-relative arm and the visibility test run inline.
    Spec("pset_write", 0x2A820, [
        Case("p0", {"eax": 0x10AF00, "edx": 0x10AA00}, c3d_28_case(0x0000)),
        Case("p1", {"eax": 0x10AF00, "edx": 0x10AA00}, c3d_28_case(0x0400)),
        Case("p2", {"eax": 0x10AF00, "edx": 0x10AA00}, c3d_28_case(0x2000)),
        Case("p3", {"eax": 0x10AF00, "edx": 0x10AA00}, c3d_28_case(0x2400, rec2c=0x1234)),
        # ^ the child arm's `rec+0x2C = parent+0x2C` copy (0x2A8A3/0x2A8A7): the real parent
        # (index 1, 0x10A868) is the image's zeros, so rec2c must be the sentinel or the
        # store changes nothing (the Task 3 sweep)
        Case("p4", {"eax": 0x10AF00, "edx": 0x10AA00}, c3d_28_case(0x0100, rec2c=5, bdc=0x100, be0=0x200)),
        Case("p5", {"eax": 0x10AF00, "edx": 0x10AA00},
             c3d_28_case(0x0000, rec2c=5, rec40=0x1000, bdc=0x5000, be0=0x2000)),
        Case("p6", {"eax": 0x10AF00, "edx": 0x10AA00},
             c3d_28_case(0x0080, rec2c=0, rec2a=0x0800, bdc=0x10000, be0=0x10000)),
        Case("p7", {"eax": 0x10AF00, "edx": 0x10AA00},
             c3d_28_case(0x0400, bdc=0x10000, be0=0x10000)),
        Case("p8", {"eax": 0x10AF00, "edx": 0x10AA00}, c3d_28_case(0x2000, s49=0xFF, s59=0x7F)),
        Case("p9", {"eax": 0x10AF00, "edx": 0x10AA00}, c3d_28_case(0x0440)),
        Case("p10", {"eax": 0x10AF00, "edx": 0x10AA00}, c3d_28_case(0x0400, s59=0xFF)),
    ], calls=(E.Call(0x2A690, ("eax",), mode="stub"),
              E.Call(0x2B150, ("eax",), mode="stub")),
       unhit_named={0x2A9CE: "the child layer is an 8-bit add (0x2A9B5/0x2A9B8), so its 0xFF clamp cannot fire"},
       eax_mask=0, mutants=("@pool", "@x", "@lay", "@vis", "@ext", "@dead", "@call")),
]

# ---- track P batch C3e (record 2026-10-05-reverse-c3e): the last type rows, part 5 -------------


def c3e_90_case(side, rnd, q=0x20, divisor=2, s81=0x30, cc=8, cb=0x20, s7b=0x11, s63=1,
                a=0x23, d1c=0x1111, r3=0x30):
    """0x39040: the per-side gate word 0x107D2C (the high half of the raw's 0x107D2A dword read),
    the 0x107D20 counter, the slot's +0x3C/+0x81/+0x7B/+0x63, the 0xC9520 divisor, the
    0x1088CB/CC/BF/A8 band bytes, the 0x4F944 target globals and the 0x40-byte table at
    0x107A80 + side*0x40."""
    so = 0x001077B0 + side * 0x94
    d = bytearray([0x7E]) * 0x1C                       # 0x107D18 .. 0x107D33
    d[0x107D18 - 0x107D18 + side * 2:0x107D1A - 0x107D18 + side * 2] = le16(0x3333)
    d[0x107D1C - 0x107D18 + side * 2:0x107D1E - 0x107D18 + side * 2] = le16(d1c)
    d[0x107D20 - 0x107D18 + side * 2:0x107D22 - 0x107D18 + side * 2] = le16(a)
    d[0x107D24 - 0x107D18 + side * 2:0x107D26 - 0x107D18 + side * 2] = le16(0x2222)
    d[0x107D2A - 0x107D18 + side * 2:0x107D2C - 0x107D18 + side * 2] = le16(0x5A5A)
    d[0x107D2C - 0x107D18 + side * 2:0x107D2E - 0x107D18 + side * 2] = le16(rnd)
    b = bytearray([0x7E]) * (0x1088CE - 0x1088A8)      # 0x1088A8 .. 0x1088CD
    b[0x1088A8 - 0x1088A8 + side] = r3
    b[0x1088B6 - 0x1088A8 + side] = 0x77
    b[0x1088BF - 0x1088A8] = 0x66
    b[0x1088CB - 0x1088A8] = cb
    b[0x1088CC - 0x1088A8] = cc
    e = bytearray(10)                                  # 0x1088E8 .. 0x1088F1
    e[0:2] = le16(0xAAAA)
    e[2:4] = le16(0xBBBB)
    e[8] = 0x99
    return {
        0x00107D18: bytes(d),
        0x00107A80 + side * 0x40: bytes([0xEE]) * 0x40,
        0x001088A8: bytes(b),
        0x001088E8: bytes(e),
        0x00104AE9: b"\x01",
        0x000C9520: le32(divisor),
        so + 0x3C: le32(q),
        so + 0x7B: bytes([s7b]),
        so + 0x63: bytes([s63]),
        so + 0x81: bytes([s81]),
    }


def c3e_92_base(side, s53=0, s54=0, s41=0x10, s5a=0x10, s5d=0x10, o63=0):
    """0x392A0: the side's slot +0x53/+0x54/+0x41/+0x5A/+0x5D, its record's +0x51, the other
    slot's +0x63 and the mode-2 `so` record's +0x5A (DSD(0x1077A8 + other*4) is REC1 for side 0
    and REC0 for side 1, both seeded by c3d_slot_pokes)."""
    p = c3d_slot_pokes()
    so = 0x001077B0 + side * 0x94
    rec = C3D_REC0 + side * 0x40
    p[so + 0x53] = bytes([s53])
    p[so + 0x54] = bytes([s54])
    p[so + 0x41] = bytes([s41])
    p[so + 0x5A] = bytes([s5a])
    p[so + 0x5D] = bytes([s5d])
    p[rec + 0x51] = bytes([side])
    p[0x001077B0 + (1 - side) * 0x94 + 0x63] = bytes([o63])
    p[C3D_REC0 + (1 - side) * 0x40 + 0x5A] = b"\x22"
    return p


def c3e_aa_base(side, s5f=0, s53=0, s54=0, s52=0, s42=0, s90=0, o5f=0, o42=0, o6c=0,
                ch0=0, ch1=0):
    """0x3AAFC: the side's slot +0x5F/+0x53/+0x54/+0x52/+0x42/+0x90/+0x7A, the other slot's
    +0x5F/+0x42/+0x6C and both slots' +0x7A character bytes (the anim-triple index)."""
    p = c3d_slot_pokes()
    so = 0x001077B0 + side * 0x94
    oo = 0x001077B0 + (1 - side) * 0x94
    rec = C3D_REC0 + side * 0x40
    a = bytearray([0x7E]) * 0x3C                       # so+0x40 .. so+0x7B
    a[0x42 - 0x40] = s42
    a[0x52 - 0x40] = s52
    a[0x53 - 0x40] = s53
    a[0x54 - 0x40] = s54
    a[0x5F - 0x40] = s5f
    a[0x7A - 0x40] = ch1 if side else ch0
    c = bytearray([0x7D]) * 0x3C                       # oo+0x40 .. oo+0x7B
    c[0x42 - 0x40] = o42
    c[0x5F - 0x40] = o5f
    c[0x6C - 0x40:0x6E - 0x40] = le16(o6c)
    c[0x7A - 0x40] = ch0 if side else ch1
    p[rec + 0x51] = bytes([side])
    p[so + 0x40] = bytes(a)
    p[so + 0x90] = bytes([s90])
    p[oo + 0x40] = bytes(c)
    return p


def c3e_4f_case():
    """0x4F944: the four globals its clamp/arming body writes, seeded off their written values."""
    return {0x001088F0: b"\x55", 0x001088E8: le16(0xAAAA), 0x001088EA: le16(0xBBBB),
            0x00104AE9: b"\x01"}


def c3e_33_case(out, rec, rec51):
    """0x33A68: the out buffer, rec+0x51 and the two slot record pointers through 0x1077B0."""
    return {rec + 0x51: bytes([rec51]), out: b"\xAA" * 24,
            0x001077B0: le32(0x10A200), 0x001077B0 + 0x94: le32(0x10A240),
            0x10A200: le32(0x10A280), 0x10A240: le32(0x10A2C0)}


def c3e_6e_case(side, s5b, live=True):
    """0x36E78: the +0x5B/else arms' slot fields, the other record's +0x43/+0x5D through
    0x1077A8 and the 0x1078FF character's slot record for the 0x29BC8 stub."""
    p = c3d_slot_pokes()
    so = 0x001077B0 + side * 0x94
    orec = 0x10A300
    p[so + 0x5B] = bytes([s5b])
    p[so + 0x41] = b"\x11"
    p[so + 0x5A] = b"\x77"
    p[so + 0x5D] = b"\x44"
    p[so + 0x40] = le32(0x404040C0)
    p[0x001077A8 + (1 - side) * 4] = le32(orec if live else 0)
    p[orec + 0x43] = b"\x44"
    p[orec + 0x5D] = b"\x66"
    p[0x001078FF] = b"\x02"
    p[0x001077B0 + 2 * 0x94] = le32(0x10A400)
    p[0x001077B0 + 2 * 0x94 + 0x7A] = b"\x03"
    p[0x00104AE9] = b"\xFF"       # The +0xFE mask store (0x36F05, b1 only): the image pre is 0,
                                  # so the bit-0 clear changed nothing (the Task 3 sweep);
                                  # 0xFF leaves 0xFE, distinguishing the store from a drop.
    return p


def c3e_8d_case(side, s10, r24, s54, s52):
    """0x468D8: the side slot's +0x10/+0x54/+0x52 and its record's +0x24."""
    p = c3d_slot_pokes()
    so = 0x001077B0 + side * 0x94
    rec = C3D_REC0 + side * 0x40
    p[so + 0x10] = le32(s10)
    p[so + 0x54] = bytes([s54])
    p[so + 0x52] = bytes([s52])
    p[rec + 0x24] = le32(r24)
    p[rec + 0x51] = bytes([side])
    return p


def c3e_cf_case(field, desc, base=None, extra=None):
    """0x2D974: the descriptor table 0x2D300[field] and (optionally) the packed config bytes
    0x105DE1.. and the trailing byte 0x105DAF+trail."""
    p = {0x0002D300 + field * 4: le32(desc)}
    if base is not None:
        p[0x00105DE1] = base
    trail = desc & 0x3F
    if trail:
        p[0x00105DAF + trail] = b"\x3E"
    if extra:
        p.update(extra)
    return p


def c3e_90b_case(desc, free, base):
    """0x46190: the 0x29 descriptor, the packed config bytes and the free-play byte."""
    return {0x0002D300 + 0x29 * 4: le32(desc), 0x00105D60: bytes([free]),
            0x00105DE1: base}


def c3e_36d_case(side, s54, s52, s43=0x11, mode=0, rec34=0x11223344):
    """0x36D20: the slot's +0x54/+0x52/+0x43, the mode word, the record's +0x34 and the side's
    0x102900 animation record."""
    p = c3d_slot_pokes()
    so = 0x001077B0 + side * 0x94
    rec = C3D_REC0 + side * 0x40
    p[so + 0x54] = bytes([s54])
    p[so + 0x52] = bytes([s52])
    p[so + 0x43] = bytes([s43])
    p[rec + 0x34] = le32(rec34)
    p[rec + 0x51] = bytes([side])
    p[0x00104B00] = le16(mode)
    p[0x00102900 + side * 4] = le32(0x10A600 + side * 0x40)
    return p


def c3e_pose_case(side, edx, ga, gb, s52=0x33, s2c=0x1122, rec24=0x99999999):
    """The four 0x3A5xx/0x3A6xx/0x3A8xx pose setters: the self slot's +0x52 band (the raw reads
    the caller's BX from it), +0x2C (the latched word), +0x58/+0x7E, the record's +0x24 and the
    setter's glob pair."""
    p = c3d_slot_pokes()
    so = 0x001077B0 + side * 0x94
    rec = C3D_REC0 + side * 0x40
    p[so + 0x10] = le32(0xA5A5A5A5)
    p[so + 0x2C] = le16(s2c)
    p[so + 0x52] = bytes([s52, 0x44, 0x55, 0x66])
    p[so + 0x58] = b"\x88"
    p[so + 0x7E] = b"\x77"
    p[rec + 0x24] = le32(rec24)
    p[rec + 0x51] = bytes([side])
    p[0x000BECF8] = b"\x0A"
    p[ga + side * 2] = le16(0xAAAA)
    p[gb + side * 2] = le16(0xBBBB)
    return p


def c3e_0fc_case(side, frame, char=0, stream=0x1234, key=1, b5b3a=0,
                 layer=0, f0aec=0, f100ad8=0, s28=0, other2c=0x11223344):
    """0x3A0FC: the triple frame char/frame, the effect stream at anim[1]+8 and the key word at
    anim[2], the 0x105B3A gate, the 0xF0AEC/0x100AD8 offset, the layer from the other record's
    +0x30 and the other slot's +0x2C."""
    p = c3d_slot_pokes()
    so = 0x001077B0 + side * 0x94
    oo = 0x001077B0 + (1 - side) * 0x94
    recs = C3D_REC0 + side * 0x40
    reco = C3D_REC0 + (1 - side) * 0x40
    c = (char << 6) + frame
    p[so + 0x7A] = bytes([char])
    p[so + 0x5F] = bytes([frame])
    p[so + 0x52] = b"\x20"
    p[recs + 0x51] = bytes([side])
    p[reco + 0x51] = bytes([1 - side])
    p[recs + 0x28] = le16(s28)
    p[reco + 0x30] = le32(layer << 16)
    p[oo + 0x2C] = le32(other2c)
    p[0x000A3528 + 20 * c + 8] = le32(stream)
    p[0x000A6728 + 6 * c] = le16(key)
    p[0x00105B3A] = bytes([b5b3a])
    p[0x000F0AEC] = le32(f0aec)
    p[0x00100AD8 + side * 4] = le32(f100ad8)
    return p


C3E_SPECS = [
    # 0x39040 fighter_39040: the per-side combo pass. EAX = side. The gate is the 0x107D2C word
    # (the high half of the raw's 0x107D2A dword read); 0x38D90/0x38FEC/0x41310/rng (0x5D7DC)/
    # voice (0x2C3FC) are stubs, 0x4F944 is a leaf allow (its writes are the observation).
    Spec("fighter_39040", 0x39040, [
        Case("g0", {"eax": 0}, c3e_90_case(0, 1)),                    # the gate: (s16)word <= 1
        Case("g1", {"eax": 0}, c3e_90_case(0, 2, q=0x10)),            # q <= lim, s63 != 0
        Case("g2", {"eax": 0}, c3e_90_case(0, 2, q=0x30, s81=0x20, cc=0x10)),  # q > lim, no clamp
        Case("g3", {"eax": 0}, c3e_90_case(0, 2, q=0x40, s81=0x20, cc=0x10)),  # q >= cap: clamp
        Case("g4", {"eax": 0}, c3e_90_case(0, 2, s63=0, a=0x50)),     # round == 2: 0x3E8
        Case("g5", {"eax": 0}, c3e_90_case(0, 3, s63=0, a=0x23),
             {0x5D7DC: 0}),                                           # delta 0x7D0; voice 0xCD
        Case("g6", {"eax": 0}, c3e_90_case(0, 4, s63=0, a=0x23), {0x5D7DC: 0}),   # 0x4F944 runs
        Case("g7", {"eax": 0}, c3e_90_case(0, 5, s63=0, a=0x41), {0x5D7DC: 0}),   # +BF = 1
        Case("g8", {"eax": 0}, c3e_90_case(0, 5, s63=0, a=0x41), {0x5D7DC: 1}),   # +BF = 2
        Case("g9", {"eax": 0}, c3e_90_case(0, 0x10, s63=0, a=0x10, r3=0x50),
             {0x5D7DC: 1}),                                           # 0x4E20; voice 0xDA
        Case("g10", {"eax": 0}, c3e_90_case(0, 4, s63=0, a=0x10, r3=0x50),
             {0x5D7DC: 0}),                                           # voice 0xDB
        Case("g11", {"eax": 0}, c3e_90_case(0, 4, s63=0, a=0x10, r3=0x30),
             {0x5D7DC: 1}),                                           # voice 0xCE
        Case("g12", {"eax": 0}, c3e_90_case(0, 4, s63=0, a=0x10, r3=0x30),
             {0x5D7DC: 2}),                                           # voice 0xCF
        Case("g13", {"eax": 1}, c3e_90_case(1, 4, s63=0, a=0x10, r3=0x30, q=0x42, s81=0x21, cc=0x11),
             {0x5D7DC: 2}),                                           # the other side
        Case("g14", {"eax": 0}, c3e_90_case(0, 3, s63=0, a=0x30), {0x5D7DC: 0}),  # a > 0x23: tail
        Case("g15", {"eax": 0}, c3e_90_case(0, 4, s63=0, a=0x41), {0x5D7DC: 0}),  # round < 5: tail
        Case("g16", {"eax": 0}, c3e_90_case(0, 0xC, s63=0, a=0x23), {0x5D7DC: 0}),  # the 0xC edge
    ], allow_calls=(0x4F944,),
       calls=(E.Call(0x38D90, ("eax",), mode="stub"),
              E.Call(0x38FEC, ("eax",), mode="stub"),
              E.Call(0x41310, ("eax", "edx"), mode="stub"),
              E.Call(0x5D7DC, ("eax",), mode="stub"),
              VOICE),
       eax_mask=0,
       mutants=("@gate", "@lim", "@b4", "@delta", "@a23", "@a41", "@id", "@rng2", "@voice",
                "@clear", "@inc")),
    # 0x392A0 fighter_392a0: EAX = slot, EDX = v, EBX = w. 0x33A68 is a leaf allow (its ctx out
    # buffer is the caller's); 0x46190 (the per-case predicate, stub EAX 0/1), 0x41310, 0x36E78
    # and rng are stubs.
    Spec("fighter_392a0", 0x392A0, [
        Case("m0", {"eax": 0x001077B0, "edx": 0x55, "ebx": 0x40},
             {**c3e_92_base(0, s5a=0x10, s5d=0x10), 0x00100CE2: le16(1),
              0x000BEBCA: le16(0x64), 0x00104B1D: b"\x00"}),           # mode 0: 5A += A
        Case("m1", {"eax": 0x001077B0, "edx": 0x40, "ebx": 0x40},
             {**c3e_92_base(0, s5a=0x70, s5d=0x10), 0x00100CE2: le16(1),
              0x00104B1D: b"\x00", 0x00105B36: b"\x00", 0x00104B00: le16(0)}),  # 5A clamp to 0x78
        Case("m2", {"eax": 0x001077B0, "edx": 1, "ebx": 0x40},
             {**c3e_92_base(0, s5a=0x10), 0x00100CE2: le16(1), 0x00104B1D: b"\x02",
              0x00104B00: le16(3), C3D_REC1 + 0x5A: b"\x40"}),         # mode 2: so 5A -= A
        Case("m3", {"eax": 0x001077B0, "edx": 0x40, "ebx": 0x40},
             {**c3e_92_base(0, s5a=0x10), 0x00100CE2: le16(1), 0x00104B1D: b"\x02",
              0x00104B00: le16(3), C3D_REC1 + 0x5A: b"\x05"}),         # mode 2: 5A += A - y
        Case("m4", {"eax": 0x001077B0, "edx": 0x40, "ebx": 0x40},
             {**c3e_92_base(0, s5a=0x70), 0x00100CE2: le16(1), 0x00104B1D: b"\x02",
              0x00104B00: le16(3), C3D_REC1 + 0x5A: b"\x05"}),         # mode 2 x > 0x78
        Case("m5", {"eax": 0x001077B0, "edx": 1, "ebx": 0x40},
             {**c3e_92_base(0, s5a=0x10), 0x00100CE2: le16(1), 0x00104B1D: b"\x02",
              0x001077A8 + 4: le32(0), 0x00104B00: le16(3)}),          # so == 0: early return
        Case("m6", {"eax": 0x001077B0, "edx": 1, "ebx": 0x40},
             {**c3e_92_base(0, s5a=0x10), 0x00100CE2: le16(1), 0x00104B1D: b"\x02",
              0x00105B38: b"\x01", 0x00104B00: le16(3)}),              # 5B38 != 0: L5E7
        Case("m7", {"eax": 0x001077B0, "edx": 0x20, "ebx": 0x40},
             {**c3e_92_base(0, s5a=0x70, s5d=0x10), 0x00100CE2: le16(1), 0x00104B1D: b"\x04",
              0x00104B1F: b"\x03", 0x00107468: le32(50), 0x00104B00: le16(3)}),  # mode 4 E scale
        Case("m8", {"eax": 0x001077B0, "edx": 1, "ebx": 0x40},
             {**c3e_92_base(0, s5a=0x10, s5d=0x10), 0x00100CE2: le16(1), 0x00104B1D: b"\x04",
              0x00104B1F: b"\x00", 0x00104B00: le16(3)}),              # mode 4: E = A
        Case("m9", {"eax": 0x001077B0, "edx": 1, "ebx": 0x40},
             {**c3e_92_base(0, s5a=0x78, s5d=0x10, o63=1), 0x00100CE2: le16(1),
              0x00107813: b"\x55", 0x001078A7: b"\xAA", 0x00104B00: le16(0)},
             {0x46190: 1}),                                            # 46190 zeroing
        Case("m10", {"eax": 0x001077B0, "edx": 1, "ebx": 0x60},
             {**c3e_92_base(0, s53=1, s5a=0x10, s5d=0x10), 0x00100CE2: le16(1),
              0x000BEBCA: le16(0x64), 0x00104B1D: b"\x00"}),           # +0x53 == 1: B clamped
        Case("m11", {"eax": 0x001077B0, "edx": 0x20, "ebx": 0x40},
             {**c3e_92_base(0, s41=0x11, s5a=0x10, s5d=0x10), 0x00100CE2: le16(1),
              0x000BEBCA: le16(0x64), 0x00104B1D: b"\x00"}),           # +0x41 bit 1: A halved
        Case("m12", {"eax": 0x001077B0, "edx": 1, "ebx": 0x40},
             {**c3e_92_base(0, s5a=0x10, s5d=0x10), 0x00100CE2: le16(5),
              0x000BEBCA + 10: le16(0x100), 0x000BEBCA + 12: le16(0x20),
              0x00104B1D: b"\x00"}),                          # the k table arm
        Case("m13", {"eax": 0x001077B0, "edx": 1, "ebx": 0x40},
             {**c3e_92_base(0, s5a=0x10, s5d=0x10), 0x00100CE2: le16(7),
              0x00104B1D: b"\x00"}),                                   # k >= 6: 40 arm
        Case("m14", {"eax": 0x001077B0, "edx": 1, "ebx": 0x40},
             {**c3e_92_base(0, s5a=0x10, s5d=0x10), 0x00100CE2: le16(0xFF9C),
              0x00104B1D: b"\x00"}),                                   # the word < 0: 40 arm
        Case("m15", {"eax": 0x001077B0, "edx": 1, "ebx": 0x40},
             {**c3e_92_base(0, s5a=0x10, s5d=0x40), 0x00100CE2: le16(1),
              0x00104B1D: b"\x00"}),                                   # 5D clamp to 0x44
        Case("m16", {"eax": 0x001077B0, "edx": 1, "ebx": 0x40},
             {**c3e_92_base(0, s5a=0x10, s5d=0x10), 0x00100CE2: le16(1),
              0x00104B1D: b"\x00"}),                                   # 5D + B <= 0x44
        Case("m17", {"eax": 0x001077B0 + 0x94, "edx": 1, "ebx": 0x40},
             {**c3e_92_base(1, s5a=0x10, s5d=0x10), 0x00100CE0: le16(1),
              0x00104B1D: b"\x00"}),                                   # the other side
    ], allow_calls=(0x33A68,),
       calls=(E.Call(0x46190, (), mode="stub"),
              E.Call(0x41310, ("eax", "edx"), mode="stub"),
              E.Call(0x36E78, ("eax",), mode="stub"),
              E.Call(0x5D7DC, ("eax",), mode="stub")),
       eax_mask=0,
       mutants=("@a", "@b", "@clamp", "@half", "@k", "@t", "@zero", "@mode2", "@so", "@clamp44",
                "@tail")),
    # 0x3AAFC fighter_reaction_apply: EAX = slot, the reaction is the stack argument s0. 0x33A10,
    # 0x3AFC4 and 0x3A280 are leaf allows; 0x18B04, 0x39F40, 0x39834, rng, voice, 0x3A0FC,
    # 0x3AA54, 0x36D20 and the four pose setters are stubs (each has its own row or is in
    # progress).
    Spec("fighter_reaction_apply", 0x3AAFC, [
        # The anim word addresses: the first/third triple (ctx[0] = 1-side) at 0xA672A+6*c1 with
        # c1 = (char[1-side] << 6) + reaction; the second (ctx[1] = side, frame DSB(self+0x5F))
        # at 0xA672A+6*c2 with c2 = (char[side] << 6) + DSB(self+0x5F).
        Case("a0", {"eax": 0x001077B0, "s0": 1},
             {**c3e_aa_base(0), 0x000A672A: le16(0x44), 0x000A6730: le16(0)}),  # uvar2 bit 6
        Case("a1", {"eax": 0x001077B0, "s0": 1},
             {**c3e_aa_base(0, s5f=0xFF), 0x000A672A: le16(0x44),
              0x000A6730: le16(0)}),                                   # 5F == 0xFF: uvar2 0
        Case("a2", {"eax": 0x001077B0, "s0": 1},
             {**c3e_aa_base(0, s5f=0xFF, s53=6, ch0=1), 0x000A672A: le16(0x44),
              0x000A6730: le16(0)}),                                   # +0x53 == 6, char == 1
        Case("a3", {"eax": 0x001077B0, "s0": 1},
             {**c3e_aa_base(0, s5f=0xFF, s54=2), 0x000A672A: le16(0x44),
              0x000A6730: le16(0)}),                                   # +0x54 == 2
        Case("a4", {"eax": 0x001077B0, "s0": 1},
             {**c3e_aa_base(0, s5f=0xFF), 0x000A672A: le16(0x44),
              0x000A6730: le16(0x100)},
             {0x3AA54: 0x42}),                                        # ecx bit 8: 0x3AA54
        Case("a5", {"eax": 0x001077B0, "s0": 1},
             {**c3e_aa_base(0, s5f=0xFF), 0x000A672A: le16(0x44),
              0x000A6730: le16(0x2000)},
             {0x36D20: 0x55}),                                        # ecx bit 0x2000: 0x36D20
        Case("a6", {"eax": 0x001077B0, "s0": 1},
             {**c3e_aa_base(0, s5f=0xFF, s54=1), 0x000A672A: le16(0x44),
              0x000A6730: le16(0), 0x000DE125: b"\x03"}),             # st == 1: 0x3A8E8
        Case("a7", {"eax": 0x001077B0, "s0": 1},
             {**c3e_aa_base(0, s5f=0xFF, s54=3), 0x000A672A: le16(0x44),
              0x000A6730: le16(0), 0x000DE125: b"\x03"}),             # st == 3: return
        Case("a8", {"eax": 0x001077B0, "s0": 1},
             {**c3e_aa_base(0, s5f=0xFF), 0x000A672A: le16(0x44),
              0x000A6730: le16(0x04), 0x000DE125: b"\x03"}),          # ecx bit 4: 0x3A650
        Case("a9", {"eax": 0x001077B0, "s0": 1},
             {**c3e_aa_base(0, s5f=0xFF), 0x000A672A: le16(0x44),
              0x000A6730: le16(0x08), 0x000DE125: b"\x03"}),          # ecx bit 8: 0x3A79C
        Case("a10", {"eax": 0x001077B0, "s0": 1},
             {**c3e_aa_base(0, s5f=0xFF), 0x000A672A: le16(0x44),
              0x000A6730: le16(0), 0x000DE125: b"\x03"}),             # default: 0x3A504
        Case("a11", {"eax": 0x001077B0, "s0": 1},
             {**c3e_aa_base(0, s5f=0xFF, o5f=0x10), 0x000A672A: le16(0x44),
              0x000A6730: le16(0), 0x000DE125: b"\x03",
              0x000BE008: le16(0x1234)}),                             # the 0x3A280 voice arm
        Case("a12", {"eax": 0x001077B0 + 0x94, "s0": 2},
             {**c3e_aa_base(1, s5f=0), 0x000A672A: le16(0),
              0x000A6736: le16(0x200)}),                               # side 1, c1 = 2
    ], allow_calls=(0x33A10, 0x3AFC4, 0x3A280),
       calls=(E.Call(0x18B04, ("eax",), mode="stub"),
              P6_39F40,
              POSE,
              E.Call(0x2C3FC, ("eax",), mode="stub", eax=1),
              E.Call(0x3A0FC, ("eax",), mode="stub", clobbers=("ebp",)),
              E.Call(0x3AA54, ("eax",), mode="stub"),
              E.Call(0x36D20, ("eax",), mode="stub"),
              E.Call(0x3A504, ("eax", "edx"), mode="stub", clobbers=("edx",)),
              E.Call(0x3A650, ("eax", "edx"), mode="stub", clobbers=("edx",)),
              E.Call(0x3A79C, ("eax", "edx"), mode="stub", clobbers=("edx",)),
              E.Call(0x3A8E8, ("eax", "edx"), mode="stub", clobbers=("edx",))),
       eax_mask=0,
       mutants=("@u2", "@s5f", "@s6", "@s54", "@e200", "@e100", "@e2000", "@st", "@s42", "@inc",
                "@voice", "@call0fc")),
    # 0x3A280 fighter_3a280: EAX = code (the callers read AL): 1 for 0x20..0x3F and, on the raw's
    # full EAX, 0x10..0x17. The 0x100010/0x100020 cases pin the raw's full-width compares.
    Spec("fighter_3a280", 0x3A280, [
        Case("r%d" % n, {"eax": v})
        for n, v in enumerate((0x0F, 0x10, 0x17, 0x18, 0x1F, 0x20, 0x3F, 0x40, 0x100010, 0x100020))
    ], eax_mask=0xFF, mutants=("@lo", "@cast")),
    # 0x4F944 fighter_4f944: EAX = v. The clamp is signed (0x2000000A > 0x14 but negative values
    # are kept); the byte, the two words and the 0x104AE9 bit are the observation. No calls.
    Spec("fighter_4f944", 0x4F944, [
        Case("v%d" % n, {"eax": v}, c3e_4f_case())
        for n, v in enumerate((0, 5, 0x14, 0x15, 0xFFFFFFFF, 0x80000000, 0x12345678))
    ], eax_mask=0, mutants=("@cmp", "@f0", "@e8", "@ae9")),
    # 0x33A68 fighter_ctx_rec_swap: EAX = the six-dword out buffer, EDX = rec; the side is
    # rec+0x51 (masked) and the slot records come from 0x1077B0.
    Spec("fighter_ctx_rec_swap", 0x33A68, [
        Case("s0", {"eax": 0x10A000, "edx": 0x10A100}, c3e_33_case(0x10A000, 0x10A100, 0)),
        Case("s1", {"eax": 0x10A000, "edx": 0x10A100}, c3e_33_case(0x10A000, 0x10A100, 1)),
        Case("s2", {"eax": 0x10A000, "edx": 0x10A100}, c3e_33_case(0x10A000, 0x10A100, 2)),
        Case("s3", {"eax": 0x10A030, "edx": 0x10A100}, c3e_33_case(0x10A030, 0x10A100, 1)),
    ], eax_mask=0, mutants=("@side", "@stride", "@rec")),
    # 0x36E78 fighter_36e78: EAX = slot. The +0x5B != 0 arm (clear it, +0x5A = 0x78 - b, +0x41
    # bit 3), else the +0x40 mask, +0x5D = 0, the other record's reset and the 0x29BC8 palette
    # call (stub).
    Spec("fighter_36e78", 0x36E78, [
        Case("b0", {"eax": 0x001077B0}, c3e_6e_case(0, 0x30)),
        Case("b1", {"eax": 0x001077B0}, c3e_6e_case(0, 0x00)),
        Case("b2", {"eax": 0x001077B0}, c3e_6e_case(0, 0x00, live=False)),
        Case("b3", {"eax": 0x001077B0 + 0x94}, c3e_6e_case(1, 0x30)),
        Case("b4", {"eax": 0x001077B0 + 0x94}, c3e_6e_case(1, 0x00)),
    ], calls=(P6_29BC8,), eax_mask=0, mutants=("@f5b", "@f40", "@other", "@pal")),
    # 0x2CAA8 config_not_free_play: `sete al` on DSB(0x105D60) (AL is what the caller reads).
    Spec("config_not_free_play", 0x2CAA8, [
        Case("f0", {}, {0x00105D60: b"\x00"}),
        Case("f1", {}, {0x00105D60: b"\x01"}),
        Case("f2", {}, {0x00105D60: b"\xFF"}),
    ], eax_mask=0xFF, mutants=("@eq", "@byte", "@zero")),
    # 0x468D8 ai_pred_468d8: EAX = side; 1 when the slot's +0x10 == 0x22BEC, the record's +0x24
    # low 31 bits clear and +0x54 != 2, else +0x52 == 7. 0x33A10 runs on both sides.
    Spec("ai_pred_468d8", 0x468D8, [
        Case("p0", {"eax": 0}, c3e_8d_case(0, 0x22BEC, 0, 2, 0)),
        Case("p1", {"eax": 0}, c3e_8d_case(0, 0x22BEC, 0, 1, 0)),
        Case("p2", {"eax": 0}, c3e_8d_case(0, 0x22BEC, 1, 0, 0)),
        Case("p3", {"eax": 0}, c3e_8d_case(0, 0x22BEC, 0, 2, 7)),
        Case("p4", {"eax": 0}, c3e_8d_case(0, 0x1234, 0, 0, 7)),
        Case("p5", {"eax": 0}, c3e_8d_case(0, 0x1234, 0, 0, 0)),
        Case("p6", {"eax": 1}, c3e_8d_case(1, 0x22BEC, 0, 1, 0)),
    ], allow_calls=(0x33A10,), eax_mask=0xFF,
       mutants=("@hand", "@low", "@byte", "@side")),
    # 0x2D974 config_field_get: EAX = field; 0xFFFFFFFF above 0x3E, else the packed nibble/byte
    # walk of 0x2D300[field]'s descriptor over 0x105DE1 and the 0x105DAF trailer.
    Spec("config_field_get", 0x2D974, [
        Case("c0", {"eax": 6}, c3e_cf_case(6, 0x0000, b"\xAB\x3C")),
        Case("c1", {"eax": 7}, c3e_cf_case(7, 0x4000, b"\xAB\xCD")),
        Case("c2", {"eax": 8}, c3e_cf_case(8, 0x8040, b"\xAB\xAB\xCD")),
        Case("c3", {"eax": 9}, c3e_cf_case(9, 0xC000, b"\xAB\xAB\xCD")),
        Case("c4", {"eax": 0x0A}, c3e_cf_case(0x0A, 0x20000, b"\x06\x05\x04\x03\x21")),
        Case("c5", {"eax": 0x0B}, c3e_cf_case(0x0B, 0x0005, b"\x0C\x00")),
        Case("c6", {"eax": 0x3F}, c3e_cf_case(0x3F, 0)),
        Case("c7", {"eax": 0x3E}, c3e_cf_case(0x3E, 0x0040, b"\x5A")),
        Case("c8", {"eax": 0x0C}, c3e_cf_case(0x0C, 0x14040, b"\x5A\x11\x22\x33")),
    ], eax_mask=0xFFFFFFFF, mutants=("@odd", "@width", "@trail", "@field", "@hi")),
    # 0x46190 fighter_46190: DIP field 0x29 bit 0x800 and free play; 0x2D974/0x2CAA8 run real
    # (each has its own row). Mask 0xFF (AL).
    Spec("fighter_46190", 0x46190, [
        Case("q0", {}, c3e_90b_case(0x3C000, 0, b"\x00\x08")),
        Case("q1", {}, c3e_90b_case(0x3C000, 0, b"\x00\x07")),
        Case("q2", {}, c3e_90b_case(0x3C000, 1, b"\x00\x08")),
        Case("q3", {}, c3e_90b_case(0x3C000, 0xFF, b"\x00\x08")),
        Case("q4", {}, c3e_90b_case(0x0000, 0, b"\x80")),
    ], calls=(E.Call(0x2D974, ("eax",), mode="real"),
              E.Call(0x2CAA8, (), mode="real")),
       eax_mask=0xFF, mutants=("@bit", "@notfree", "@field")),
    # 0x36D20 fighter_36d20: EAX = slot. 0x468D8 (stub, per-case EAX) gates; +0x54 == 2 runs the
    # inlined 0x36CE4 body (the port factors it into a call, unrecorded), else rec+0x34 = 0 and
    # 0x36BC8 (stub); the anim runs through 0x2BC30.
    Spec("fighter_36d20", 0x36D20, [
        Case("d0", {"eax": 0x001077B0}, c3e_36d_case(0, 0, 0x33), {0x468D8: 1}),
        Case("d1", {"eax": 0x001077B0}, c3e_36d_case(0, 2, 0x33, mode=3), {0x468D8: 0}),
        Case("d2", {"eax": 0x001077B0}, c3e_36d_case(0, 2, 0x33, mode=0), {0x468D8: 0}),
        Case("d3", {"eax": 0x001077B0}, c3e_36d_case(0, 2, 0x33, mode=0x22), {0x468D8: 0}),
        Case("d4", {"eax": 0x001077B0}, c3e_36d_case(0, 0, 0x33), {0x468D8: 0}),
        Case("d5", {"eax": 0x001077B0 + 0x94}, c3e_36d_case(1, 1, 0x44, mode=0), {0x468D8: 0}),
    ], calls=(E.Call(0x468D8, ("eax",), mode="stub"),
              E.Call(0x36BC8, ("eax", "edx"), mode="stub", clobbers=("edx",)),
              ANIM_BEGIN),
       eax_mask=0xFF, mutants=("@pred", "@s54", "@anim", "@rec", "@ret")),
    # The four 0x3A5xx/0x3A6xx/0x3A8xx pose setters: EAX = side, EDX = the frame delta; each
    # shares the 0x3A50E..0x3A56B body with its own callback and glob pair. 0x33A10 runs on both
    # sides (allow).
    Spec("fighter_pose_3a504", 0x3A504, [
        Case("p0", {"eax": 0, "edx": 0x10, "ebx": 0x33}, c3e_pose_case(0, 0x10, 0x107D14, 0x107D10)),
        Case("p1", {"eax": 1, "edx": 0x80, "ebx": 0x33}, c3e_pose_case(1, 0x80, 0x107D14, 0x107D10)),
    ], allow_calls=(0x33A10,), eax_mask=0, mutants=("@s52", "@cb", "@glob")),
    Spec("fighter_pose_3a650", 0x3A650, [
        Case("p0", {"eax": 0, "edx": 0x10, "ebx": 0x33}, c3e_pose_case(0, 0x10, 0x107D0C, 0x107D00)),
        Case("p1", {"eax": 1, "edx": 0x80, "ebx": 0x33}, c3e_pose_case(1, 0x80, 0x107D0C, 0x107D00)),
    ], allow_calls=(0x33A10,), eax_mask=0, mutants=("@s52", "@cb", "@glob")),
    Spec("fighter_pose_3a79c", 0x3A79C, [
        Case("p0", {"eax": 0, "edx": 0x10, "ebx": 0x33}, c3e_pose_case(0, 0x10, 0x107D08, 0x107D04)),
        Case("p1", {"eax": 1, "edx": 0x80, "ebx": 0x33}, c3e_pose_case(1, 0x80, 0x107D08, 0x107D04)),
    ], allow_calls=(0x33A10,), eax_mask=0, mutants=("@s52", "@cb", "@glob")),
    Spec("fighter_pose_3a8e8", 0x3A8E8, [
        Case("p0", {"eax": 0, "edx": 0x10, "ebx": 0x33}, c3e_pose_case(0, 0x10, 0x107CF8, 0x107CFC)),
        Case("p1", {"eax": 1, "edx": 0x80, "ebx": 0x33}, c3e_pose_case(1, 0x80, 0x107CF8, 0x107CFC)),
    ], allow_calls=(0x33A10,), eax_mask=0, mutants=("@s52", "@cb", "@glob")),
    # 0x3A0FC fighter_3a0fc: EAX = side. 0x33950/0x3AFC4 are allows; the two effect spawns
    # (0x2AE14, per-case stub EAX) and their 0x2BC30 animates are stubs. The 0x105B3A jump table
    # (entries 0..2 spawn, 3 skips) and the per-side stream tables are the branches.
    Spec("fighter_3a0fc", 0x3A0FC, [
        Case("f0", {"eax": 0}, c3e_0fc_case(0, 5, stream=0x1234, key=1, f0aec=0x1000,
             f100ad8=2, layer=3, other2c=0x55667788), {0x2AE14: 0x10A480}),
        Case("f1", {"eax": 1}, c3e_0fc_case(1, 0, stream=0x2222, key=1, f0aec=0x2000,
             f100ad8=1, layer=5, other2c=0x99AABBCC), {0x2AE14: 0x10A480}),
        Case("f2", {"eax": 0}, c3e_0fc_case(0, 5, stream=0, key=2, f0aec=0x1000), {0x2AE14: 0x10A480}),
        Case("f3", {"eax": 0}, c3e_0fc_case(0, 5, stream=0x1234, key=3, b5b3a=4), {0x2AE14: 0x10A480}),
        Case("f4", {"eax": 0}, c3e_0fc_case(0, 5, stream=0x1234, key=0, s28=0x4000), {}),
        Case("f5", {"eax": 1}, c3e_0fc_case(1, 2, stream=0x3333, key=2, s28=0x4000,
             f0aec=0x3000), {0x2AE14: 0x10A480}),
        Case("f6", {"eax": 1}, c3e_0fc_case(1, 2, stream=0x3333, key=3), {0x2AE14: 0x10A480}),
        Case("f7", {"eax": 0}, c3e_0fc_case(0, 3, stream=0x4444, key=9, b5b3a=3), {}),
        Case("f8", {"eax": 1}, c3e_0fc_case(1, 2, stream=0, key=4, b5b3a=4), {}),
    ], allow_calls=(0x33950, 0x3AFC4), calls=(SPAWN, ANIM_BEGIN), eax_mask=0,
       mutants=("@frame", "@face", "@off", "@layer", "@stream", "@key", "@s59")),
]

# ---- track P batch C3f (record 2026-10-05-reverse-c3f): the tail's tail --------------------------
#
# List B/C and the text-tree leaves first; the 0x38D90/0x38FEC tree follows (the batch's prototype
# order is the record's §C3f dependency order).


def c3f_38b_case():
    """0x38BB0/0x38BC8: the three 0x40-byte 0x107A80 tables and the two 0x107D24 words (each poke at
    most 0x40 bytes: the driver's per-poke cap). The 0x107D24 word for both sides is seeded
    different from the 0 the 0x38BCC store writes (side 1's image byte is 0), with the 0x107D28
    neighbour nonzero so a too-wide store changes it (the Task 3 sweep)."""
    return {0x00107A40: bytes([0x5A]) * 0x40, 0x00107A80: bytes([0x5B]) * 0x40,
            0x00107AC0: bytes([0x5C]) * 0x40,
            0x00107D24: le16(0x1234) + le16(0x5678) + le16(0x9ABC)}


def c3f_b038_case(side, base, k, x):
    """0x3B038: the 0xBE018 base, the 0xBEDEE window word (high 16) and the side slot's +0x2C."""
    return {0x000BE018: le32(base), 0x000BEDEE: le32(k << 16),
            0x001077B0 + side * 0x94 + 0x2C: le32(x)}


def c3f_46534_case(side, v, floor, capidx=0, cap=0x40):
    """0x46534: the per-side accumulator 0x1082C8 (seed pre-delta), the 0x1082D0 floor, the
    0x10452C index byte and the 0xC9408 cap table entry."""
    return {0x001082C8 + side * 4: le32(v), 0x001082D0: le32(floor),
            0x0010452C: bytes([capidx]), 0x000C9408 + capidx: bytes([cap])}


def c3f_ready_case(side, s53, s54):
    """0x3BDB0: the side slot's +0x53/+0x54."""
    p = c3d_slot_pokes()
    so = 0x001077B0 + side * 0x94
    p[so + 0x53] = bytes([s53])
    p[so + 0x54] = bytes([s54])
    return p


def c3f_scan_case(side, pos, values):
    """0x4649C: the side's 0x14-word input ring at 0x108270 and the 0x1082D2 read position
    (the raw reads the dword's high 16 as the word index). `values` maps ring index -> word."""
    p = {0x00108270 + side * 0x28: le16(0x1111) * 0x14, 0x001082D2: le32(pos << 16)}
    for i, v in values.items():
        p[0x00108270 + side * 0x28 + i * 2] = le16(v)
    return p


def c3f_ok_case(side, s54, s53):
    """0x1AB10: the side slot's +0x54/+0x53."""
    p = c3d_slot_pokes()
    so = 0x001077B0 + side * 0x94
    p[so + 0x54] = bytes([s54])
    p[so + 0x53] = bytes([s53])
    return p


def c3f_lock_case(flags, ln, base=0x0010A000):
    """0x1E75C: the handle at 0x10AA00 (base +8, len +0xC, bits +0x15) with its neighbours."""
    return {0x0010AA00: b"\x11" * 8 + le32(base) + le32(ln) + b"\x22\x33\x44\x55\x66"
            + bytes([flags]) + b"\x99\x77"}


def c3f_unlock_case(flags, clock=0x11223344):
    """0x1E808: the handle at 0x10AA00 and the DPMI clock shadow 0x101500. `flags` sits at +0x15,
    the byte the clear reads; +0x14 is 0x02 (bit 1 set) so the @off mutant's wrong-offset clear is
    visible even when flags' bit 1 is already clear."""
    return {0x00101500: le32(clock),
            0x0010AA00: b"\x11" * 8 + le32(0x0010A000) + le32(0x40) + le32(0x77777777)
            + b"\x02" + bytes([flags]) + b"\x99\x77"}


def c3f_anim_case(side, s54, s43, ch, rec=C3D_REC1):
    """0x1A6AC: the slot's +0x54/+0x43/+0x7A, the stream tables 0xC8F40/0xC8F90 and the rec
    argument's +0x51 (fighter_ctx_rec_swap's side)."""
    p = c3d_slot_pokes()
    so = 0x001077B0 + side * 0x94
    p[so + 0x54] = bytes([s54])
    p[so + 0x43] = bytes([s43])
    p[so + 0x7A] = bytes([ch])
    p[rec + 0x51] = bytes([side])
    p[0x000C8F40 + ch * 4] = le32(0x11111111)
    p[0x000C8F90 + ch * 4] = le32(0x22222222)
    return p


def c3f_chain_case(side, s5f, s53, s63, s7c=0, s55=0, ch=0):
    """0x3CF38: the side slot's +0x5F/+0x53/+0x63/+0x7C/+0x55/+0x7A."""
    p = c3d_slot_pokes()
    so = 0x001077B0 + side * 0x94
    p[so + 0x5F] = bytes([s5f])
    p[so + 0x53] = bytes([s53])
    p[so + 0x63] = bytes([s63])
    p[so + 0x7C] = bytes([s7c])
    p[so + 0x55] = bytes([s55])
    p[so + 0x7A] = bytes([ch])
    return p


def c3f_str_case(groups, outlen=0x100, base=0x0010B000, handle=0x0010AA00):
    """0x474E4: the localisation table at `base`. `groups` is a list of lists of plaintext
    entries; group i>0 sits at an 8-byte block (its chain link at +4) and every entry is a length
    byte followed by its bytes XORed with that length. The handle names base; the out buffer at
    0x10A200 is seeded 0x5A so a dropped or short write is visible."""
    buf = bytearray(8)                                  # base+0..+7 (group 0's entries at +8)
    offs = []
    for gi, g in enumerate(groups):
        if gi > 0:
            offs.append(len(buf))
            buf.extend(b"\x00" * 8)
        for e in g:
            buf.append(len(e) & 0xFF)
            buf.extend(bytes(b ^ (len(e) & 0xFF) for b in e))
    for i in range(1, len(offs)):
        buf[offs[i] + 4:offs[i] + 8] = le32(offs[i + 1] if i + 1 < len(offs) else 0)
    if offs:
        buf[4:8] = le32(offs[0])
    return {0x001082DC: le32(handle),
            handle: b"\x00" * 8 + le32(base) + le32(0x400) + b"\x00\x00\x00\x00\x00"
            + b"\x00\x00\x00",
            base: bytes(buf), 0x0010A200: bytes([0x5A]) * 0x40}


C3F_SPECS = [
    # 0x38BB0 fighter_38bb0: EAX = side; clear 0x107A80 + side*0x40 .. +0x3F. EAX is dead.
    Spec("fighter_38bb0", 0x38BB0, [
        Case("b0", {"eax": 0}, c3f_38b_case()),
        Case("b1", {"eax": 1}, c3f_38b_case()),
    ], eax_mask=0, mutants=("@stride", "@len", "@off")),
    # 0x38BC8 fighter_38bc8: EAX = side; 0x38BB0 plus DSW(0x107D24 + side*2) = 0.
    Spec("fighter_38bc8", 0x38BC8, [
        Case("b0", {"eax": 0}, c3f_38b_case()),
        Case("b1", {"eax": 1}, c3f_38b_case()),
    ], allow_calls=(0x38BB0,), eax_mask=0, mutants=("@word", "@off", "@clear")),
    # 0x3B038 fighter_3b038: EAX = side; 1 when slot+0x2C is in the window about 0xBE018.
    Spec("fighter_3b038", 0x3B038, [
        Case("w0", {"eax": 0}, c3f_b038_case(0, 0, 0xFF00, 0xFFFFFF00)),   # arm 2 boundary
        Case("w1", {"eax": 0}, c3f_b038_case(0, 0, 0xFF00, 0xFFFFFF01)),   # arm 2 only
        Case("w2", {"eax": 0}, c3f_b038_case(0, 0, 0xFF00, 0x00000000)),   # both miss
        Case("w3", {"eax": 0}, c3f_b038_case(0, 0, 0xFF00, 0x00000100)),   # arm 1 boundary
        Case("w4", {"eax": 0}, c3f_b038_case(0, 0, 0xFF00, 0x00000101)),   # arm 1
        Case("w5", {"eax": 0}, c3f_b038_case(0, 0x1000, 0x40, 0x0FBF)),    # positive k: miss
        Case("w6", {"eax": 1}, c3f_b038_case(1, 0, 0xFF00, 0x00000000)),   # side 1: both miss
        Case("w7", {"eax": 1}, c3f_b038_case(1, 0, 0xFF00, 0x00000100)),   # side 1: arm 1
    ], eax_mask=0xFF, mutants=("@s1", "@s2", "@sum")),
    # 0x46534 fighter_46534: EAX = side, EDX = delta; add to 0x1082C8[side], clamp to the
    # 0xC9408 cap down, to 0 up, then to the 0x1082D0 floor. EAX is dead.
    Spec("fighter_46534", 0x46534, [
        Case("v0", {"eax": 0, "edx": 5}, c3f_46534_case(0, 0x10, 0)),      # 0x15
        Case("v1", {"eax": 0, "edx": 0x30}, c3f_46534_case(0, 0x10, 0)),   # cap 0x40: 0x40
        Case("v2", {"eax": 0, "edx": 0xFFFFFFF6}, c3f_46534_case(0, 0x10, 0)),  # -10 -> 6
        Case("v3", {"eax": 0, "edx": 0xFFFFFFEF}, c3f_46534_case(0, 0x10, 0)),  # -17 -> 0
        Case("v4", {"eax": 0, "edx": 5}, c3f_46534_case(0, 0x10, 0x30)),   # floor 0x30
        Case("v5", {"eax": 0, "edx": 0x20}, c3f_46534_case(0, 0x3E, 0)),   # cap 0x40: 0x40
        Case("v6", {"eax": 0, "edx": 2}, c3f_46534_case(0, 0x10, 0, capidx=3, cap=0x20)),
        Case("v7", {"eax": 1, "edx": 5}, c3f_46534_case(1, 0x10, 0)),      # the other side
        Case("v8", {"eax": 0, "edx": 0}, c3f_46534_case(0, 0x10, 0x50)),   # floor > cap
        Case("v9", {"eax": 0, "edx": 0xFFFFFFE0}, c3f_46534_case(0, 0x10, 0)),  # negative
    ], eax_mask=0, mutants=("@cap", "@sign", "@floor", "@order")),
    # 0x3BDB0 fight_attack_ready: EAX = side; AL = 1 when slot+0x53 == 0 and +0x54 != 2.
    Spec("fight_attack_ready", 0x3BDB0, [
        Case("r0", {"eax": 0}, c3f_ready_case(0, 0, 0)),
        Case("r1", {"eax": 0}, c3f_ready_case(0, 0, 2)),
        Case("r2", {"eax": 0}, c3f_ready_case(0, 1, 0)),
        Case("r3", {"eax": 0}, c3f_ready_case(0, 0, 3)),
        Case("r4", {"eax": 0}, c3f_ready_case(0, 0x80, 1)),
        Case("r5", {"eax": 1}, c3f_ready_case(1, 0, 2)),
        Case("r6", {"eax": 1}, c3f_ready_case(1, 0, 0)),
    ], allow_calls=(0x33950,), eax_mask=0xFF, mutants=("@s53", "@s54", "@side")),
    # 0x4649C fighter_input_scan: EAX = side, EDX = n1, EBX = n2, ECX = mask; skip n1 ring
    # entries, scan n2 from there, 1 on the first word whose low 16 bits overlap the mask.
    Spec("fighter_input_scan", 0x4649C, [
        Case("s0", {"eax": 0, "edx": 0, "ebx": 0, "ecx": 0xFFFF}, c3f_scan_case(0, 3, {})),
        Case("s1", {"eax": 0, "edx": 0, "ebx": 1, "ecx": 0x0001},
             c3f_scan_case(0, 3, {3: 0x0001})),                            # the first word hits
        Case("s2", {"eax": 0, "edx": 0, "ebx": 1, "ecx": 0x0002},
             c3f_scan_case(0, 3, {3: 0x0001})),                            # the mask misses
        Case("s3", {"eax": 0, "edx": 2, "ebx": 2, "ecx": 0x0004},
             c3f_scan_case(0, 3, {3: 0x0008, 2: 0x0004})),                 # hit after the skip
        Case("s4", {"eax": 0, "edx": 2, "ebx": 2, "ecx": 0x0010},
             c3f_scan_case(0, 3, {3: 0x0008, 2: 0x0004})),                 # miss
        Case("s5", {"eax": 0, "edx": 2, "ebx": 1, "ecx": 0x0004},
             c3f_scan_case(0, 0, {0x12: 0x0004, 0x13: 0x0000})),           # skip wraps 0 -> 0x13
        Case("s6", {"eax": 0, "edx": 0, "ebx": 2, "ecx": 0x0008},
             c3f_scan_case(0, 0, {0x13: 0x0008})),                         # scan wraps 0 -> 0x13
        Case("s7", {"eax": 0, "edx": 1, "ebx": 3, "ecx": 0x0100},
             c3f_scan_case(0, 1, {0: 0x0100})),                            # second word hits
        Case("s8", {"eax": 1, "edx": 0, "ebx": 2, "ecx": 0x0001},
             c3f_scan_case(1, 2, {2: 0x0001})),
        Case("s9", {"eax": 1, "edx": 0, "ebx": 1, "ecx": 0x8000},
             c3f_scan_case(1, 0x13, {0x13: 0x8000})),
    ], eax_mask=0xFF, mutants=("@n1", "@n2", "@mask", "@wrap")),
    # 0x1AB10 fighter_state_ok: EAX = side; 1 when the side slot's +0x54 and +0x53 are both <= 1
    # (unsigned, `test al,al`/`jbe`). 0x33A10 is an allow (leaf).
    Spec("fighter_state_ok", 0x1AB10, [
        Case("o0", {"eax": 0}, c3f_ok_case(0, 0, 0)),
        Case("o1", {"eax": 0}, c3f_ok_case(0, 1, 1)),
        Case("o2", {"eax": 0}, c3f_ok_case(0, 2, 0)),
        Case("o3", {"eax": 0}, c3f_ok_case(0, 0, 2)),
        Case("o4", {"eax": 0}, c3f_ok_case(0, 0x80, 0)),
        Case("o5", {"eax": 0}, c3f_ok_case(0, 0, 0xFF)),
        Case("o6", {"eax": 1}, c3f_ok_case(1, 0, 0)),
        Case("o7", {"eax": 1}, c3f_ok_case(1, 2, 0)),
    ], allow_calls=(0x33A10,), eax_mask=0xFF, mutants=("@s54", "@s53", "@sign")),
    # 0x1E75C string_lock: EAX = the handle; returns its +8 base when +0x15 bit 0 is clear and
    # +0xC != 0 (setting bit 1), else 0.
    Spec("string_lock", 0x1E75C, [
        Case("l0", {"eax": 0x10AA00}, c3f_lock_case(0x00, 0x40)),
        Case("l1", {"eax": 0x10AA00}, c3f_lock_case(0x01, 0x40)),
        Case("l2", {"eax": 0x10AA00}, c3f_lock_case(0x00, 0)),
        Case("l3", {"eax": 0x10AA00}, c3f_lock_case(0xFE, 0x40)),
        Case("l4", {"eax": 0x10AA00}, c3f_lock_case(0x80, 0x40)),
    ], eax_mask=0xFFFFFFFF, mutants=("@bit", "@len", "@base", "@or")),
    # 0x1E808 string_unlock: EAX = the handle; clears +0x15 bit 1, then the 0x500BB clock read
    # stored at +0x10 (the raw `mov eax,[0x101500]; ret` body is an allow).
    Spec("string_unlock", 0x1E808, [
        Case("u0", {"eax": 0x10AA00}, c3f_unlock_case(0xFF)),
        Case("u1", {"eax": 0x10AA00}, c3f_unlock_case(0x00)),
        Case("u2", {"eax": 0x10AA00}, c3f_unlock_case(0x02, clock=0xDEADBEEF)),
    ], allow_calls=(0x500BB,), eax_mask=0, mutants=("@and", "@off", "@store", "@clock")),
    # 0x474E4 string_decode: EAX = id, EDX = out, EBX = outlen. The handle at 0x1082DC names the
    # table base; 0x1E75C/0x1E808 are stubs, so each case chooses the base through the lock stub.
    Spec("string_decode", 0x474E4, [
        Case("n0", {"eax": 0, "edx": 0x10A200, "ebx": 0x100},
             c3f_str_case([[b"HELLO"]]), {0x1E75C: 0x0010B000}),          # len 5, ret 6
        Case("n1", {"eax": 3, "edx": 0x10A200, "ebx": 0x100},
             c3f_str_case([[b"aa", b"bbb", b"c", b"ABCD"]]), {0x1E75C: 0x0010B000}),
        Case("n2", {"eax": 0x40, "edx": 0x10A200, "ebx": 0x100},
             c3f_str_case([[b"first"], [b"WORLD"]]), {0x1E75C: 0x0010B000}),
        Case("n3", {"eax": 0x41, "edx": 0x10A200, "ebx": 0x100},
             c3f_str_case([[b"first"], [b"aa", b"WORLD"]]), {0x1E75C: 0x0010B000}),
        Case("n4", {"eax": 0, "edx": 0x10A200, "ebx": 4},
             c3f_str_case([[b"HELLO"]]), {0x1E75C: 0x0010B000}),          # truncated: ret 4
        Case("n4b", {"eax": 0, "edx": 0x10A200, "ebx": 5},
             c3f_str_case([[b"HELLO"]]), {0x1E75C: 0x0010B000}),          # len == outlen
        Case("n5", {"eax": 0, "edx": 0x10A200, "ebx": 0x100},
             c3f_str_case([[b""]]), {0x1E75C: 0x0010B000}),               # len 0: ret 0
        Case("n6", {"eax": 0x80, "edx": 0x10A200, "ebx": 0x100},
             c3f_str_case([[b"g0"], [b"g1"], [b"THIRD"]]), {0x1E75C: 0x0010B000}),
    ], calls=(E.Call(0x1E75C, ("eax",), mode="stub"), E.Call(0x1E808, ("eax",), mode="stub")),
       eax_mask=0xFFFFFFFF, mutants=("@xor", "@trunc", "@term", "@skip", "@off", "@ret", "@link")),
    # 0x1C500 game_string_get: EAX = id; calls 0x474E4(EDX = 0x102760, EBX = 0x100) and zeroes the
    # buffer's first byte when the return is 0; returns the buffer address (EAX always 0x102760).
    Spec("game_string_get", 0x1C500, [
        Case("c0", {"eax": 0}, c3f_str_case([]), {0x474E4: 6}),
        Case("c1", {"eax": 7}, c3f_str_case([]), {0x474E4: 0}),
        Case("c2", {"eax": 0x40}, c3f_str_case([]), {0x474E4: 0x100}),
    ], calls=(E.Call(0x474E4, ("eax", "edx", "ebx"), mode="stub",
                     clobbers=("ebx", "edx"), writes=((1, 0, b"HELLO\x00"),)),),
       eax_mask=0xFFFFFFFF, mutants=("@zero", "@id", "@ret", "@call")),
    # 0x1A6AC fighter_block_anim: EAX = slot, EDX = rec. 0x33A68 runs on both sides (allow, the
    # port's ctx builder); 0x18B04 (the facing flag) and 0x3C480 (the animation start) are stubs.
    Spec("fighter_block_anim", 0x1A6AC, [
        Case("a0", {"eax": 0x001077B0, "edx": C3D_REC1}, c3f_anim_case(0, 0, 0x00, 3)),
        Case("a1", {"eax": 0x001077B0, "edx": C3D_REC1}, c3f_anim_case(0, 0, 0x20, 3)),
        Case("a2", {"eax": 0x001077B0, "edx": C3D_REC1}, c3f_anim_case(0, 1, 0x00, 3)),
        Case("a3", {"eax": 0x001077B0, "edx": C3D_REC1}, c3f_anim_case(0, 1, 0x10, 3)),
        Case("a4", {"eax": 0x001077B0, "edx": C3D_REC1}, c3f_anim_case(0, 2, 0x00, 3)),
        Case("a5", {"eax": 0x001077B0, "edx": C3D_REC1}, c3f_anim_case(0, 0xFF, 0x00, 3)),
        Case("a6", {"eax": 0x001077B0, "edx": C3D_REC1}, c3f_anim_case(0, 0x80, 0x00, 3)),
        Case("a7", {"eax": 0x001077B0 + 0x94, "edx": C3D_REC1}, c3f_anim_case(1, 0, 0x00, 5)),
        Case("a8", {"eax": 0x001077B0 + 0x94, "edx": C3D_REC1}, c3f_anim_case(1, 1, 0x10, 5)),
        Case("a9", {"eax": 0x001077B0, "edx": C3D_REC1}, c3f_anim_case(0, 0, 0x40, 3)),
        Case("a10", {"eax": 0x001077B0, "edx": C3D_REC1}, c3f_anim_case(0, 0, 0x80, 3)),
    ], allow_calls=(0x33A68,),
       calls=(E.Call(0x18B04, ("eax",), mode="stub"),
              E.Call(0x3C480, ("eax", "edx", "s0"), mode="stub", pop=4, clobbers=("edx",))),
       eax_mask=0, mutants=("@s54", "@bit", "@mask", "@stream", "@call", "@rec")),
    # 0x3CF38 hit_chain_resolve: EAX = side. 0x3CD44 (scan), 0x3CE58 (drive) and 0x3C6A8 (seed)
    # are stubs; 0x32BAC is a one-byte RET the port's hit_sound no-ops (allow).
    Spec("hit_chain_resolve", 0x3CF38, [
        Case("h0", {"eax": 0}, c3f_chain_case(0, 0, 0, 0), {0x3CD44: 0xFFFFFFFF}),  # scan -1
        Case("h1", {"eax": 0}, c3f_chain_case(0, 0, 1, 0), {0x3CD44: 5, 0x3CE58: 0},),
        Case("h2", {"eax": 0}, c3f_chain_case(0, 0, 0, 0), {0x3CD44: 5, 0x3CE58: 0},),
        Case("h3", {"eax": 0}, c3f_chain_case(0, 0, 0, 0), {0x3CD44: 5, 0x3CE58: 1},),
        Case("h4", {"eax": 0}, c3f_chain_case(0, 0, 0, 1), {0x3CD44: 5, 0x3CE58: 1},),
        Case("h5", {"eax": 0}, c3f_chain_case(0, 0x10, 0, 0), {0x3CD44: 0x1C, 0x3CE58: 1},),
        Case("h6", {"eax": 0}, c3f_chain_case(0, 0x10, 0, 0), {0x3CD44: 0x1B, 0x3CE58: 1},),
        Case("h7", {"eax": 0}, c3f_chain_case(0, 0x17, 0, 0), {0x3CD44: 0x1C, 0x3CE58: 1},),
        Case("h8", {"eax": 0}, c3f_chain_case(0, 0x18, 0, 0), {0x3CD44: 0x1C, 0x3CE58: 1},),
        Case("h9", {"eax": 1}, c3f_chain_case(1, 0, 0, 0, s7c=7, s55=9, ch=2),
             {0x3CD44: 3, 0x3CE58: 1},),
        Case("h10", {"eax": 1}, c3f_chain_case(1, 0, 0, 0),
             {0x3CD44: 3, 0x3CE58: 0},),
    ],
       calls=(E.Call(0x3CD44, ("eax",), mode="stub"),
              E.Call(0x3CE58, ("eax", "edx"), mode="stub", clobbers=("edx", "edi", "ebp")),
              E.Call(0x3C6A8, ("eax", "edx", "ebx"), mode="stub", clobbers=("edx",)),
              E.Call(0x32BAC, ("eax",), mode="stub")),
       eax_mask=0xFF, mutants=("@guard", "@drive", "@seed", "@sound", "@s5f", "@s55", "@inc",
                               "@ret")),
]


# ---- track P batch C3g (record 2026-10-05-reverse-c3g): the closing frontier ---------------------
#
# Wave 1: 0x1A7CC, 0x2A690, 0x37178, 0x38D90, 0x38FEC, 0x3BDDC, 0x3C6A8, 0x3CD44, 0x3CE58. The C3f
# fixtures (C3D_REC0/1, c3d_slot_pokes, c3f_str_case) and seams are reused; every poke stays inside
# the image.

C3G_POOL, C3G_REC, C3G_PSET = 0x0010A000, 0x0010A068, 0x0010A200


def c3g_seed_case(side, i, value, desc=0x000C619C):
    """0x3C6A8: the three (side, i) words and the frame's stun entry at desc+0xC+value*0x14."""
    off = side * 0x40 + i * 2
    return {0x00107D58 + off: le16(0xFFFF), 0x00107E58 + off: le16(0xAAAA),
            0x00107DD8 + off: le16(0xBBBB),
            desc + 0x0C + value * 0x14: le16(0x1234)}


def c3g_scan_case(side, armed):
    """0x3CD44: the side's 0x20 phase words, 8 at the indices in `armed` (0 elsewhere)."""
    p = {0x00107D58 + side * 0x40: b"\x00" * 0x40}
    for i in armed:
        p[0x00107D58 + side * 0x40 + i * 2] = le16(8)
    return p


def c3g_fec_case(side, ch, n, rec=0x000C0000):
    """0x38FEC: the slot's +0x7A char, the 0xBEBB8+ch*2 count word, 0xBEB90+ch*4 record base."""
    p = c3d_slot_pokes()
    p[0x001077B0 + side * 0x94 + 0x7A] = bytes([ch])
    p[0x000BEBB8 + ch * 2] = le16(n)
    p[0x000BEB90 + ch * 4] = le32(rec)
    return p


def c3g_drive_case(side, i, ch=0, reaction=0x10, mode=0x20, fa=0, s5f=0, s53=0, s56=0):
    """0x3CE58: the slot's +0x7A/+0x5F/+0x53/+0x56, the mode word 0x104B00, the flash gate
    0x1078FA and the (ch, i) reaction word at 0xC619C+(ch*0x20+i)*8+4."""
    p = c3d_slot_pokes()
    so = 0x001077B0 + side * 0x94
    p[so + 0x7A] = bytes([ch])
    p[so + 0x5F] = bytes([s5f])
    p[so + 0x53] = bytes([s53])
    p[so + 0x56] = bytes([s56])
    p[0x00104B00] = le16(mode)
    p[0x001078FA] = bytes([fa])
    p[0x000C619C + (ch * 0x20 + i) * 8 + 4] = le16(reaction)
    return p


def c3g_combo_str_case():
    """0x38D90: a localisation table with id 0xE5 = group 3, entry 0x25 (0xE4 = entry 0x24),
    reachable through the chain base+4 -> group1 -> group2 -> group3, and the 0x102760 out buffer
    seeded 0x5A. The entries between are zero-length so the table fits in <= 0x40-byte pokes."""
    base = 0x0010B000
    g3 = [b""] * 36 + [b"XX", b"BBBB"]                 # entry 36 (0xE4) and entry 37 (0xE5)
    groups = [[b"g0"], [b"g1"], [b"g2"], g3]
    buf, offs = bytearray(8), []
    for gi, g in enumerate(groups):
        if gi > 0:
            offs.append(len(buf))
            buf.extend(b"\x00" * 8)
        for e in g:
            buf.append(len(e) & 0xFF)
            buf.extend(bytes(c ^ (len(e) & 0xFF) for c in e))
    buf[4:8] = le32(offs[0])
    for i in range(len(offs) - 1):
        buf[offs[i] + 4:offs[i] + 8] = le32(offs[i + 1])
    p = {0x001082DC: le32(0x0010AA00),
         0x0010AA00: b"\x00" * 8 + le32(base) + le32(0x400) + b"\x00" * 15,
         0x00102760: bytes([0x5A]) * 0x40}
    for off in range(0, len(buf), 0x40):
        p[base + off] = bytes(buf[off:off + 0x40])
    return p


def c3g_90_case(side, s63):
    """0x38D90: the combo string table, the side slot's +0x63 gate and the two number sources
    (0x107D2C/0x107D20)."""
    p = c3g_combo_str_case()
    p[0x001077B0 + side * 0x94 + 0x63] = bytes([s63])
    p[0x00107D2C + side * 2] = le16(0x1234)
    p[0x00107D20 + side * 2] = le16(0x0056)
    return p


def c3g_start_case(side, s5f=3, count=3, tribyte=5, pct=50, s43=0xFF, s61=0x11, s62=0, s60=0x55):
    """0x1A7CC: the side's slot +0x43/+0x60..+0x62, the other slot's +0x5F/+0x64/+0x7A, the
    0x100CE0 + other*2 count word, the 0xA2C4A pct table (broad 0x0001, then the count+1 entry) and
    the tri[0]+0xA byte of the (other char = 2, s5f) triple."""
    other = 1 - side
    p = c3d_slot_pokes()
    so = 0x001077B0 + side * 0x94
    oo = 0x001077B0 + other * 0x94
    p[so + 0x43] = bytes([s43])
    p[so + 0x60] = bytes([s60, s61, s62])
    p[oo + 0x5F] = bytes([s5f & 0xFF])
    p[oo + 0x64] = bytes([0])
    p[oo + 0x7A] = bytes([2])
    p[0x00100CE0 + other * 2] = le16(count)
    p[0x000A2C4A] = le32(0x00640000) + le32(0x00010001) * 15   # 0xA2C4A..0xA2C89: k=0 100, else 1
    k = (count + 1) & 0xFFFF
    p[0x000A2C4C + k * 2] = le16(pct)
    if s5f < 0x40:
        c = ((2 << 6) + s5f) * 11
        p[0x000DE114 + c + 0x0A] = bytes([tribyte & 0xFF])
    return p


def c3g_pset_case(rec, s28, x18=0x12345678, y30=0x00010000, y1c=7, s34=0, s38=0, s64=0,
                  s61=0x02000000, s44=0, s32=0, s59=0, idx=0, d44=0, daf0=0x00020000,
                  daec=0x00010000):
    """0x2A690: the pool bases, the record's fields (one composed buffer, so overlaps cannot drop a
    field), the two globals and the pset (C3G_PSET + idx*0x20) with sentinels."""
    buf = bytearray(0x68)
    def w32(o, v): buf[o:o + 4] = le32(v)
    def w16(o, v): buf[o:o + 2] = le16(v)
    w16(0x28, s28)
    w32(0x18, x18)
    w32(0x30, y30)          # +0x30..+0x33
    w32(0x1C, y1c)
    w16(0x32, s32)          # inside the y30 dword: written after it
    w16(0x34, s34)
    w16(0x38, s38)
    w32(0x3C, 0x5A5A5A5A)
    w32(0x44, s44)
    buf[0x59] = s59
    w16(0x56, idx)
    w16(0x2C, 0x00AB)
    w32(0x61, s61)          # +0x61..+0x64
    buf[0x64] = s64
    p = {0x001014F4: le32(C3G_POOL), 0x001014EC: le32(C3G_PSET),
         0x00107A44: le32(d44), 0x000F0AF0: le32(daf0), 0x000F0AEC: le32(daec),
         rec: bytes(buf[:0x40]), rec + 0x40: bytes(buf[0x40:])}
    ps = bytearray(0x20)
    ps[4:8] = le32(0x11111111)
    ps[8:12] = le32(0x22222222)
    ps[0x0C:0x0E] = le16(0x7777)
    ps[0x0E:0x10] = le16(0x8888)
    ps[0x10:0x14] = le32(0x33333333)
    ps[0x14:0x18] = le32(0x44444444)
    p[C3G_PSET + idx * 0x20] = bytes(ps)
    p[0x00105BE0] = le32(0x66666666)
    p[0x00105BDC] = le32(0x55555555)
    p[0x00107900] = le32(0x00001234) + le32(0x00005678)
    return p


def c3g_attack_case(side, s40=0, cmd=0x8000, ch=2, s54=0, chain=0, scan=0):
    """0x3BDDC: the slot's +0x40/+0x53/+0x54/+0x7A, the command word 0x1088E0+side*2, the
    0x107803+side*0x94 chain gate byte, the record pointers, the animation table at 0xC8B30 and the
    two attack rows at 0xBEF28/0xBEF64."""
    p = c3d_slot_pokes()
    so = 0x001077B0 + side * 0x94
    p[so + 0x40] = bytes([s40])
    p[so + 0x54] = bytes([s54])
    p[so + 0x7A] = bytes([ch])
    p[so + 0x52] = bytes([0x11])
    p[so + 0x4E] = le16(0x2222)
    p[0x001088E0 + side * 2] = le16(cmd)
    p[0x00107803 + side * 0x94] = bytes([0 if chain is None else chain])
    rec = C3D_REC0 + (side & 1) * 0x40
    p[rec + 0x34] = le16(0x3333)
    p[rec + 0x43] = bytes([0x44])
    p[rec + 0x42] = bytes([0x55])
    p[0x00107D40 + side * 4] = le32(0x66666666)
    p[0x001078F8 + side] = bytes([0x77])
    p[0x000C8B30 + ch * 4] = le32(0x0000ABCD)
    p[0x000BEF28 + ch * 6] = le16(0x0101) + le16(0x0202) + le16(0x0303)
    p[0x000BEF64 + ch * 6] = le16(0x0404) + le16(0x0505) + le16(0x0606)
    p[0x00107D2C + side * 2] = le16(0)
    return p


def c3g_37178_case(rec_s, rec_o, side=None, s51=None, x18_s=0x1000, x18_o=0x1200, s28_s=0,
                   s28_o=0, ch_s=1, ch_o=1, word=0x0100, tbl=0x0010A300, actor_base=0x0010A400,
                   act_s=0x8000, act_o=0x8000, idx_s=0, idx_o=1):
    """0x37178: the two records (X = the slot's [slot], poked to rec_s), +0x51 (the side byte),
    +0x18, +0x28, +0x7A, +0x56 (the actor index), the 0x1078DC base table and the 0x1014EC actor
    words (bit 15 selects)."""
    if side is not None:
        s51 = side
    p = c3d_slot_pokes()
    p[0x001077B0] = le32(rec_s)
    p[0x001077B0 + 0x94] = le32(rec_o)
    p[rec_s + 0x51] = bytes([0 if s51 is None else s51])
    p[rec_o + 0x51] = bytes([1 - (0 if s51 is None else s51)])
    p[rec_s + 0x18] = le32(x18_s)
    p[rec_o + 0x18] = le32(x18_o)
    p[rec_s + 0x28] = le16(s28_s)
    p[rec_o + 0x28] = le16(s28_o)
    sd = 0 if s51 is None else (s51 & 1)
    p[0x001077B0 + sd * 0x94 + 0x7A] = bytes([ch_s])          # the slot's char, not the record's
    p[0x001077B0 + (1 - sd) * 0x94 + 0x7A] = bytes([ch_o])
    p[rec_s + 0x56] = le16(idx_s)
    p[rec_o + 0x56] = le16(idx_o)
    # The slot fields the body writes/reads (the Task 5 sweep: these were poked at rec_s/rec_o,
    # the records, so the slot +0x40 dword AND wrote its own pre-state 0 and was invisible). The
    # +0x40 sentinel has bit 0x400000 set, which the AND clears, and bit 6 clear, which the OR
    # sets; +0x43 has bit 6 clear for its OR 0x40; the other slot's +0x42 has bit 2 clear for its
    # OR 4.
    p[0x001077B0 + sd * 0x94 + 0x40] = le32(0x00400011)
    p[0x001077B0 + sd * 0x94 + 0x52] = bytes([0x11])
    p[0x001077B0 + sd * 0x94 + 0x54] = bytes([0x22])
    p[0x001077B0 + sd * 0x94 + 0x43] = bytes([0x15])
    p[0x001077B0 + (1 - sd) * 0x94 + 0x42] = bytes([0x73])
    p[0x001078FE] = bytes([0x88])
    p[0x001078DC] = le32(tbl)
    p[tbl + ch_s * 14 + ch_o * 2] = le16(word)
    p[tbl + 2 + ch_o * 2] = le16(0x0080)        # the @base mutant's wrong-stride read
    p[0x001014EC] = le32(actor_base)
    p[actor_base + idx_s * 0x20] = le16(act_s)
    p[actor_base + idx_o * 0x20] = le16(act_o)
    return p


def c3g_stance_case(side, i, ch, st, e, d, s7b=0):
    """0x3CCEC: the side slot's +0x54 (stance) and +0x7A (char; +0x7B is the @char mutant's
    wrong byte, seeded with `s7b`, whose own entry gets zero flags), and the 0xC619C entry
    (char*0x20+i)*8: six sentinel bytes then d (+6) and e (+7). The swapped-index entry the
    @entry mutant reads is seeded 0xF1/0xF2 so a wrong index changes the verdict."""
    p = c3d_slot_pokes()
    so = 0x001077B0 + side * 0x94
    p[so + 0x54] = bytes([st])
    p[so + 0x7A] = bytes([ch, s7b])
    entry = ch * 0x20 + i
    p[0x000C619C + entry * 8] = b"\x41\x42\x43\x44\x45\x46"
    p[0x000C619C + entry * 8 + 6] = bytes([d])
    p[0x000C619C + entry * 8 + 7] = bytes([e])
    entry2 = ch + i * 0x20
    if entry2 != entry:
        p[0x000C619C + entry2 * 8 + 6] = b"\xF1"
        p[0x000C619C + entry2 * 8 + 7] = b"\xF2"
    entry3 = s7b * 0x20 + i
    if entry3 != entry:
        p[0x000C619C + entry3 * 8 + 6] = b"\x00"
        p[0x000C619C + entry3 * 8 + 7] = b"\x00"
    return p


def c3g_desc_case(side, i, s63, sel=None, ch=0, ptr=0x0010A000, cpu=0x11111111, pl=0x22222222):
    """0x3C600: the side slot's +0x63 (nonzero forces sel 2) and +0x7A (char; +0x7B feeds the
    @char mutant), the 0x101514 pointer's mode word (+0x2D4/+0x2D6; the unused one is 0xAA so a
    wrong-side read hits the default), and the 0xC6B9C/0xC619C entries at (char*0x20+i)*8: the real
    one, the swapped-index one (@entry), the char+1 one (@char) and the next entry dword."""
    p = c3d_slot_pokes()
    so = 0x001077B0 + side * 0x94
    p[so + 0x63] = bytes([s63])
    p[so + 0x7A] = bytes([ch, ch + 1])
    p[0x00101514] = le32(ptr)
    p[ptr + 0x2D4] = le16(0x00AA)
    p[ptr + 0x2D6] = le16(0x00AA)
    if sel is not None:
        p[ptr + 0x2D4 + side * 2] = le16(sel)
    entry = ch * 0x20 + i
    p[0x000C6B9C + entry * 8] = le32(cpu)
    p[0x000C619C + entry * 8] = le32(pl)
    entry2 = ch + i * 0x20
    if entry2 != entry:
        p[0x000C6B9C + entry2 * 8] = le32(0x33333333)
        p[0x000C619C + entry2 * 8] = le32(0x44444444)
    entry3 = (ch + 1) * 0x20 + i
    if entry3 != entry:
        p[0x000C6B9C + entry3 * 8] = le32(0x55555555)
        p[0x000C619C + entry3 * 8] = le32(0x66666666)
    p[0x000C6B9C + (entry + 1) * 8] = le32(0x77777777)
    p[0x000C619C + (entry + 1) * 8] = le32(0x88888888)
    return p


def c3g_react_case(side, st, cmd, ch=0, tbl2=0x000A7B44, tbl2w=0x000A7ACC, idx_tbl=0x000A7A70):
    """0x3CBC4/0x3CC58: the side slot's +0x54 (stance) and +0x7A (char), the command word
    0x1088E0+side*2, the per-char table pointer DSD(`tbl2`+ch*4) (0xA7B44 for A, 0xA7ACC for B;
    the other table is seeded differently for the @table mutant) and the 0xA7A70 idx byte."""
    p = c3d_slot_pokes()
    so = 0x001077B0 + side * 0x94
    p[so + 0x54] = bytes([st])
    p[so + 0x7A] = bytes([ch])
    p[0x001088E0 + side * 2] = le16(cmd)
    p[tbl2 + ch * 4] = le32(0x0010A300)
    other = tbl2w if tbl2 != tbl2w else 0x000A7B44
    if other != tbl2:
        p[other + ch * 4] = le32(0x0010A400)
    p[idx_tbl + ch] = bytes([0x44])
    return p


def c3g_react_b_case(side, st, cmd, ch=0):
    return c3g_react_case(side, st, cmd, ch=ch, tbl2=0x000A7ACC, tbl2w=0x000A7B44)


def c3g_allow_case(side, ch):
    """0x4CE70: both slots' +0x7A chars (the unread side's is 0x0B > 6 so the @side mutant is
    visible) and +0x7B (ch^1, so the @char mutant reads a different list)."""
    p = c3d_slot_pokes()
    for s in (0, 1):
        so = 0x001077B0 + s * 0x94
        c = ch if s == side else 0x0B
        p[so + 0x7A] = bytes([c, c ^ 1])
    return p


def c3g_gate_case(side, s56):
    """0x3CE24: the side slot's +0x56 (the signed byte the raw's `sar 0x18` of the +0x53 dword
    reads), with +0x55 and +0x57 seeded different from it."""
    p = c3d_slot_pokes()
    so = 0x001077B0 + side * 0x94
    p[so + 0x55] = b"\x7E"
    p[so + 0x56] = bytes([s56])
    p[so + 0x57] = b"\x7F"
    return p


C3G_38ED0_REC = 0x0010A600           # 0x38ED0's 0x44-byte combo record scratch
# The six string ids the pair pokes use (0x38ED0's +4*j/+0xC+4*j dwords). `c3f_str_case` seeds a
# real table so the two 0x1C500 decodes differ per id and the @pair mutant is visible in the
# decoded buffer at each 0x2F4BC call (the recorded call args alone cannot see a string swap).
C3G_38ED0_PAIR = ((0, 1), (2, 3), (4, 5))
C3G_38ED0_STRINGS = [[b"A", b"BB", b"CCC", b"DDDD", b"EEEEE", b"FFFFFF"]]


def c3g_38ed0_case(side, ids, needs=(), thr=(0, 0, 0), hit=5, hit_other=5, s63=0, s62=0,
                   pair=C3G_38ED0_PAIR, table=(), table_other=(), rec=C3G_38ED0_REC):
    """0x38ED0: the record's +0x1B id list (`ids`, the exact bytes), the +0x2F needs and the three
    +0x18 threshold bytes; the +4*j/+0xC+4*j string id pairs; the slot's +0x62/+0x63; the side's
    0x107A80 need table (`table` as (id, byte) pairs) and both 0x107D2C hit words. The record's
    spare bytes are 0xA5 sentinels (so a shifted id read is visible); +0x2E is 0x00 for the @need
    mutant. The string table makes every pair id decode to distinct text."""
    p = c3d_slot_pokes()
    p.update(c3f_str_case(C3G_38ED0_STRINGS))
    so = 0x001077B0 + side * 0x94
    p[0x001077B0] = le32(rec)
    p[so + 0x62] = bytes([s62])
    p[so + 0x63] = bytes([s63])
    lo = bytearray(b"\xA5" * 0x18)
    for j, (r6, r7) in enumerate(pair):
        lo[j * 4:j * 4 + 4] = le32(r6)
        lo[0xC + j * 4:0xC + j * 4 + 4] = le32(r7)
    hi = bytearray(b"\xA5" * 0x2C)          # rec+0x18 .. rec+0x43
    hi[0x16] = 0x00                         # the byte before the needs (@need's wrong offset)
    for j in range(3):
        hi[j] = thr[j] & 0xFF
    for i, b in enumerate(ids):
        hi[3 + i] = b & 0xFF
    for i, b in enumerate(needs):
        hi[0x17 + i] = b & 0xFF
    p[rec] = bytes(lo)
    p[rec + 0x18] = bytes(hi)
    p[0x00107D2C + side * 2] = le16(hit)
    p[0x00107D2C + (1 - side) * 2] = le16(hit_other)
    for i, b in table:
        p[0x00107A80 + side * 0x40 + i] = bytes([b])
    for i, b in table_other:
        p[0x00107A80 + (1 - side) * 0x40 + i] = bytes([b])
    return p


C3G_APPLY_REC0 = 0x0010A600          # 0x34E2C's slot 0 record
C3G_APPLY_REC1 = 0x0010A680          # 0x34E2C's slot 1 record
C3G_APPLY_STREAM = 0x0010A800        # the pointer the triple's [anim[1]+4] points to
C3G_APPLY_CB = 0x00029D60            # the allowed bare `ret` the callback arm calls (fn_resolve skips)
C3G_APPLY_C = 0x12                   # char 0, reaction 0x12: the triple entry every case uses


def c3g_apply_case(side, reaction, ch=0, s54=0, s52=0, s53=0, s6a=0, s41=0, s84=0, s5f=0,
                   fa=0, cb=0, stream=0, bx=0, voice=0, s62=0, st5a=0, st5c=0, d24=0):
    """0x34E2C: both slots' records, the side slot's +0x41/+0x52/+0x53/+0x54/+0x5F/+0x62/+0x6A/+0x7A/
    +0x84, the 0x1078F8[side] sentinel, the 0x1078FA gate, the (ch, reaction) triple's three entries
    (0xDE114's +7 voice byte, 0xA3528's +0 callback and +4 stream pointer over the stream dword at
    C3G_APPLY_STREAM, 0xA6728's +2 word) and the stance timer's 0x100B5A/0x100B5C seeds. The
    records' +0x20/+0x24/+0x59/+0x62/+0x63 carry sentinels."""
    p = c3d_slot_pokes()
    s = 0x001077B0 + side * 0x94
    p[0x001077B0] = le32(C3G_APPLY_REC0)
    p[0x001077B0 + 0x94] = le32(C3G_APPLY_REC1)
    p[s + 0x41] = bytes([s41])
    p[s + 0x52] = bytes([s52])
    p[s + 0x53] = bytes([s53])
    p[s + 0x54] = bytes([s54])
    p[s + 0x5F] = bytes([s5f])
    p[s + 0x62] = bytes([s62])
    p[s + 0x6A] = le16(s6a)
    p[s + 0x7A] = bytes([ch])
    # +0x84 and the +0x86..+0x8B neighbours: the +0x88 word store (raw 0x34E9E) writes 0, so
    # its pre-state must not be 0 (the Task 5 sweep); the nonzero neighbours catch a too-wide
    # store at +0x88.
    p[s + 0x84] = le16(s84) + b"\xA5\xA5" + le16(0x1234) + b"\x5A\x5A"
    p[0x001078F8] = b"\xA5\xA5"
    p[0x001078FA] = bytes([fa])
    rec = C3G_APPLY_REC0 if side == 0 else C3G_APPLY_REC1
    for r in (C3G_APPLY_REC0, C3G_APPLY_REC1):
        p[r + 0x20] = le32(0x20202020)
        p[r + 0x24] = le32(0x24242424)
        p[r + 0x59] = b"\x59"
        p[r + 0x62] = b"\x62"
        p[r + 0x63] = b"\x63"
    p[rec + 0x24] = le32(d24)
    c = ch * 0x40 + reaction
    a0, a1, a2 = 0x000DE114 + c * 11, 0x000A3528 + c * 20, 0x000A6728 + c * 6
    p[a0 + 7] = bytes([voice])
    p[a1] = le32(cb)
    p[a1 + 4] = le32(C3G_APPLY_STREAM if stream else 0)
    p[C3G_APPLY_STREAM] = le32(stream)
    p[a2] = le16(0x0000)                # the @bx mutant's wrong word (bit 12 clear)
    p[a2 + 2] = le16(bx)
    p[0x00100B5A + side] = bytes([st5a])
    p[0x00100B5C + side] = bytes([st5c])
    p[0x00100B5E] = b"\xA5\xA5"         # both sides' clear word, so @stance's side is visible
    return p


# ---- waves 3+4: the text tree's roots, 0x3CD94 and 0x1922C (record §C3g.5, plan Task 4) --------
# The scratch addresses (all inside the data object): the glyph row's col/row pair (the raw's EDX/
# EBX point at them), the text rows' EBX string, 0x2EF24/0x2EFD4's EDX dest, a released cell's
# record and the pset array 0x1014EC points at, the actor pool/free record and the render node the
# allowed 0x2AE14's real body needs (its alloc/palette/render paths are poked valid so no case
# reads outside the image).
C3G_T_COLROW = 0x0010A200
C3G_T_STR = 0x0010A210
C3G_T_DEST = 0x0010A280
C3G_T_PSET = 0x0010A380          # DS_001014EC (the pset array; below the pool)
C3G_T_POOL = 0x0010A400          # DS_001014F4 and the free-list head's record (index 0)
C3G_T_CELL = 0x0010A468          # the grid records (pool index 1; in_pool for actor_pset)
C3G_T_NODE = 0x0010A700          # the render free node (0x10275C)


def c3g_tstr_case(text, extra=None):
    """The text rows' EBX string at C3G_T_STR (NUL-terminated)."""
    p = {C3G_T_STR: bytes(text) + b"\x00"}
    if extra:
        p.update(extra)
    return p


def c3g_tw_case(text, mode, cls=(), wid=(), nbr=()):
    """0x2F0F0: the string, the class bytes at 0xBD390+c (`nbr` the +1 neighbours the @coff mutant
    reads) and the width tables' +2 bytes at table + cls*4 + 2."""
    p = {C3G_T_STR: bytes(text) + b"\x00"}
    for c, v in cls:
        p[0x000BD390 + c] = bytes([v])
    for c, v in nbr:
        p[0x000BD391 + c] = bytes([v])
    for table, k, v in wid:
        p[table + k * 4 + 2] = bytes([v])
    return p


def c3g_cell_case(cells, cur=None, pset=C3G_T_PSET):
    """0x2F280/0x2F314: the 0x1014EC pset pointer, the pool base 0x1014F4, the cursor when `cur`,
    and per (idx, pset index) a record at C3G_T_CELL+k*0x68 stored into the grid word
    0x105F38+idx with +0x56 the pset index."""
    p = {0x001014EC: le32(pset), 0x001014F4: le32(C3G_T_POOL)}
    if cur is not None:
        p[0x00105F34] = le32(cur)
    for k, (idx, pi) in enumerate(cells):
        rec = C3G_T_CELL + k * 0x68
        p[0x00105F38 + idx] = le32(rec)
        p[rec + 0x56] = le16(pi)
    return p


def c3g_imm_case(side, i, r, ch, c, lo=0, hi=0, ch2=None):
    """0x3CD94: the slot's +0x5F reaction byte and +0x7A char (its +0x7B neighbour `ch2` feeds the
    @char mutant), the (ch*0x20+i)*8 entry's +4 word (the signed candidate) and the two dwords of
    0xA182C + ch*0x200 + r*8."""
    p = c3d_slot_pokes()
    so = 0x001077B0 + side * 0x94
    p[so + 0x5F] = bytes([r])
    p[so + 0x7A] = bytes([ch]) if ch2 is None else bytes([ch, ch2])
    entry = ch * 0x20 + i
    p[0x000C619C + entry * 8 + 4] = le16(c & 0xFFFF)
    entry2 = ch + i * 0x20
    if entry2 != entry:
        p[0x000C619C + entry2 * 8 + 4] = le16(0)
    p[0x000A182C + ch * 0x200 + r * 8] = le32(lo)
    p[0x000A182C + ch * 0x200 + r * 8 + 4] = le32(hi)
    return p


def c3g_timer_case(side, b5a, b5c, x24, x20=0x5A5A5A5A):
    """0x1922C: the slot's record pointer (C3D_REC0/1), DS_00100B5A/B5C/B5E[side] and the record's
    +0x24/+0x20. B5E is seeded 0x99 so the unconditional clear is visible."""
    p = c3d_slot_pokes()
    p[0x001077B0 + side * 0x94] = le32(C3D_REC0 + side * 0x40)
    p[0x00100B5A + side] = bytes([b5a])
    p[0x00100B5C + side] = bytes([b5c])
    p[0x00100B5E + side] = b"\x99"
    p[C3D_REC0 + side * 0x40 + 0x24] = le32(x24)
    p[C3D_REC0 + side * 0x40 + 0x20] = le32(x20)
    return p


def c3g_glyph_case(ch, col, row, mode, vertical=0, cell=0, free=True, pset_idx=0):
    """0x2F5A0: the col/row pair at C3G_T_COLROW (the raw's EDX/EBX scratch), the grid cell at
    row*0xac + col*4 (`cell` a record pointer at C3G_T_CELL), the actor pool 0x1014F4 and pset
    0x1014EC, the 0x105B3C free ring (one record at C3G_T_POOL; `free` False is the self-sentinel
    empty list, so the allowed 0x2AE14's alloc fails) and the render free node 0x10275C."""
    p = {C3G_T_COLROW: le32(col), C3G_T_COLROW + 4: le32(row)}
    if cell:
        p[0x00105F38 + (row & 0xFFFF) * 0xac + (col & 0xFFFF) * 4] = le32(cell)
        p[cell + 0x56] = le16(pset_idx)
    p[0x001014F4] = le32(C3G_T_POOL)
    p[0x001014EC] = le32(C3G_T_PSET)
    p[0x00105B3C] = le32(C3G_T_POOL if free else 0x00105B3C)
    if free:
        p[C3G_T_POOL] = le32(0x00105B3C)
        p[C3G_T_POOL + 4] = le32(0x00105B3C)
    p[0x00105BCC] = le32(0x00105BCC)
    p[0x00105BD0] = le32(0x00105BCC)
    p[0x0010275C] = le32(C3G_T_NODE)
    p[0x00105B44] = le32(0)
    return p


C3G_SPECS = [
    # 0x3C6A8 hit_slot_seed: EAX = side, EDX = value, EBX = i. 0x3C600 (desc) is a stub.
    Spec("hit_slot_seed", 0x3C6A8, [
        Case("g0", {"eax": 0, "edx": 2, "ebx": 3}, c3g_seed_case(0, 3, 2), {0x3C600: 0x000C619C}),
        Case("g1", {"eax": 1, "edx": 0, "ebx": 0}, c3g_seed_case(1, 0, 0), {0x3C600: 0x000C619C}),
        Case("g2", {"eax": 0, "edx": 4, "ebx": 0x1F}, c3g_seed_case(0, 0x1F, 4), {0x3C600: 0x000C619C}),
        Case("g3", {"eax": 1, "edx": 1, "ebx": 5}, c3g_seed_case(1, 5, 1, desc=0x000C719C),
             {0x3C600: 0x000C719C}),
    ], calls=(E.Call(0x3C600, ("eax", "edx"), mode="stub"),),
       eax_mask=0, mutants=("@phase", "@clear", "@off", "@field", "@scale", "@bytes")),
    # 0x3CD44 hit_scan: EAX = side. 0x3CCEC (stance) is a stub (AL tested); -1 when none armed.
    Spec("hit_scan", 0x3CD44, [
        Case("t0", {"eax": 0}, c3g_scan_case(0, [0]), {0x3CCEC: 1}),
        Case("t1", {"eax": 0}, c3g_scan_case(0, [2, 5]), {0x3CCEC: 1}),
        Case("t2", {"eax": 0}, c3g_scan_case(0, [3]), {0x3CCEC: 0}),
        Case("t3", {"eax": 0}, c3g_scan_case(0, []), {0x3CCEC: 1}),
        Case("t4", {"eax": 0}, c3g_scan_case(0, [0x1F]), {0x3CCEC: 1}),
        Case("t5", {"eax": 1}, c3g_scan_case(1, [0x1F]), {0x3CCEC: 1}),
        Case("t6", {"eax": 1}, c3g_scan_case(1, []), {0x3CCEC: 1}),
        Case("t7", {"eax": 0}, c3g_scan_case(0, [2, 5]), {0x3CCEC: 0x100}),
    ], calls=(E.Call(0x3CCEC, ("eax", "edx"), mode="stub", clobbers=("edx",)),),
       eax_mask=0xFFFFFFFF, mutants=("@bound", "@phase", "@side", "@stride", "@al", "@miss")),
    # 0x38FEC fighter_38fec: EAX = side; walk the char's combo records until 0x38ED0 (stub) says 1.
    Spec("fighter_38fec", 0x38FEC, [
        Case("f0", {"eax": 0}, c3g_fec_case(0, 2, 0)),
        Case("f1", {"eax": 0}, c3g_fec_case(0, 2, 2), {0x38ED0: 1}),
        Case("f2", {"eax": 0}, c3g_fec_case(0, 3, 2), {0x38ED0: 0}),
        Case("f3", {"eax": 1}, c3g_fec_case(1, 5, 1), {0x38ED0: 1}),
        Case("f4", {"eax": 0}, c3g_fec_case(0, 2, 0xFFFF)),
        Case("f5", {"eax": 0}, c3g_fec_case(0, 0, 2), {0x38ED0: 1}),
    ], allow_calls=(0x33950,),
       calls=(E.Call(0x38ED0, ("eax", "edx"), mode="stub", clobbers=("edx",)),),
       eax_mask=0, mutants=("@ch", "@n", "@table", "@stride", "@call")),
    # 0x3CE58 hit_reaction_drive: EAX = side, EDX = i. 0x3CE24 (gate) and 0x4CE70 (allow) are
    # stubs whose AL the raw tests; 0x3CBC4/0x3CC58 return the reaction word (movsx, 0x3CF14);
    # 0x34D8C runs real (the 0x1078FA gate); 0x34E2C (apply) is a stub.
    Spec("hit_reaction_drive", 0x3CE58, [
        Case("d0", {"eax": 0, "edx": 0}, c3g_drive_case(0, 0), {0x3CE24: 0}),
        Case("d1", {"eax": 0, "edx": 0x20}, c3g_drive_case(0, 5), {0x3CE24: 1}),
        Case("d2", {"eax": 0, "edx": 5}, c3g_drive_case(0, 5, reaction=0x10, mode=0x21),
             {0x3CE24: 1, 0x4CE70: 0}),
        Case("d3", {"eax": 0, "edx": 5}, c3g_drive_case(0, 5, reaction=0x10, mode=0x22),
             {0x3CE24: 1, 0x4CE70: 1}),
        Case("d4", {"eax": 0, "edx": 5}, c3g_drive_case(0, 5, reaction=0x10),
             {0x3CE24: 1, 0x3CBC4: 0x1234}),
        Case("d5", {"eax": 0, "edx": 5}, c3g_drive_case(0, 5, reaction=0x11),
             {0x3CE24: 1, 0x3CC58: 0x5678}),
        Case("d6", {"eax": 0, "edx": 5}, c3g_drive_case(0, 5, reaction=0x12),
             {0x3CE24: 1}),
        Case("d7", {"eax": 0, "edx": 5}, c3g_drive_case(0, 5, reaction=0x0F),
             {0x3CE24: 1}),
        Case("d8", {"eax": 0, "edx": 5}, c3g_drive_case(0, 5, reaction=0x10),
             {0x3CE24: 1, 0x3CBC4: 0x8000}),
        Case("d9", {"eax": 0, "edx": 5}, c3g_drive_case(0, 5, reaction=0x12, fa=2),
             {0x3CE24: 1}),
        Case("d10", {"eax": 0, "edx": 5}, c3g_drive_case(0, 5, reaction=0x12), {0x3CE24: 0x100}),
        Case("d11", {"eax": 0, "edx": 5}, c3g_drive_case(0, 5, reaction=0x12, mode=0x21),
             {0x3CE24: 1, 0x4CE70: 0x100}),
    ], allow_calls=(0x34D8C,),
       calls=(E.Call(0x3CE24, ("eax", "edx"), mode="stub", clobbers=("edx",)),
              E.Call(0x4CE70, ("eax", "edx"), mode="stub"),
              E.Call(0x3CBC4, ("eax",), mode="stub"),
              E.Call(0x3CC58, ("eax",), mode="stub"),
              E.Call(0x34E2C, ("eax", "edx"), mode="stub", clobbers=("edx", "edi", "ebp"))),
       eax_mask=0xFF, mutants=("@gate", "@gateal", "@bound", "@mode", "@allow", "@flash",
                               "@sel", "@hi", "@store", "@apply", "@ret")),
    # 0x38D90 fighter_38d90: EAX = side. The whole string path runs real (0x1C500 and its tree);
    # the four text calls are stubs, the 0x1C500 string feeds the two row-6/7 holds.
    Spec("fighter_38d90", 0x38D90, [
        Case("s0", {"eax": 0}, c3g_90_case(0, 0)),
        Case("s1", {"eax": 1}, c3g_90_case(1, 0)),
        Case("s2", {"eax": 0}, c3g_90_case(0, 1)),
    ], allow_calls=(0x1C500, 0x474E4, 0x1E75C, 0x1E808, 0x500BB),
       calls=(E.Call(0x38C5C, ("eax",), mode="stub"),
              E.Call(0x2F4BC, ("eax", "edx", "ebx", "ecx"), mode="stub",
                     clobbers=("ebx", "ecx", "edx")),
              E.Call(0x2F4D0, ("eax", "edx", "ebx", "ecx", "s0", "s1"), mode="stub", pop=8,
                     clobbers=("ebx", "ecx", "edx")),
              E.Call(0x2F20C, ("eax", "edx", "ebx", "ecx"), mode="stub",
                     clobbers=("ebx", "ecx", "edx"))),
       eax_mask=0, mutants=("@c5c", "@row", "@id", "@mode", "@col", "@pctrow", "@pct")),
    # 0x1A7CC fighter_block_start: EAX = side. 0x33A10/0x3AFC4 are allows; 0x18B04 and 0x1A6AC
    # are stubs. The 0x1A842 arm is dead (the +0x5F <= 0x3F test at 0x1A810 precedes it).
    Spec("fighter_block_start", 0x1A7CC, [
        Case("b0", {"eax": 0}, c3g_start_case(0, s5f=3, count=3, tribyte=5, pct=50)),
        Case("b1", {"eax": 0}, c3g_start_case(0, s5f=0x40, count=1, pct=40)),
        Case("b2", {"eax": 0}, c3g_start_case(0, s5f=0x3F, count=0, tribyte=0x7F, pct=30)),
        Case("b3", {"eax": 0}, c3g_start_case(0, s5f=1, count=6, pct=10)),
        Case("b4", {"eax": 0}, c3g_start_case(0, s5f=1, count=0x7FFF, pct=10)),
        Case("b5", {"eax": 1}, c3g_start_case(1, s5f=2, count=2, tribyte=9, pct=25)),
        Case("b6", {"eax": 0}, c3g_start_case(0, s5f=4, count=2, tribyte=0x80, pct=50)),
    ], allow_calls=(0x33A10, 0x3AFC4),
       calls=(E.Call(0x18B04, ("eax",), mode="stub"),
              E.Call(0x1A6AC, ("eax", "edx"), mode="stub", clobbers=("edx",))),
       eax_mask=0,
       unhit_named={0x1A842: "dead: the +0x5F <= 0x3F arm of 0x1A810 was just taken (re-tested at 0x1A82D), so the 0x1A830 jge cannot fall here",
                    0x1A854: "dead: the same arm as 0x1A842 (its second call site)"},
       mutants=("@and", "@s61", "@s60", "@s62", "@bound", "@tri", "@k",
                "@div", "@tbl", "@s52", "@s53")),
    # 0x2A690 actor_pset_point: EAX = rec. 0x2A620 (cursor) is a stub; the pset is
    # 0x1014EC + word[rec+0x56]*0x20 and the record must be in the 0x1014F4 pool (the port's
    # actor_pset guard).
    Spec("actor_pset_point", 0x2A690, [
        Case("q0", {"eax": C3G_REC}, c3g_pset_case(C3G_REC, 0x0001)),
        Case("q1", {"eax": C3G_REC}, c3g_pset_case(C3G_REC, 0x1001, s38=1, s64=0x7F, s61=0x02000000,
                                                   s44=0x00020000)),
        Case("q2", {"eax": C3G_REC}, c3g_pset_case(C3G_REC, 0x1000, s64=0xFF, s34=0)),
        Case("q3", {"eax": C3G_REC}, c3g_pset_case(C3G_REC, 0x1000, s64=0xFF, s34=5, s32=200,
                                                   d44=0xFF9C0000)),
        Case("q4", {"eax": C3G_REC}, c3g_pset_case(C3G_REC, 0x0000, s32=1, y30=0xFF000000, s59=0x7F)),
        Case("q5", {"eax": C3G_REC}, c3g_pset_case(C3G_REC, 0x0000, s32=0xFFFF, s59=0x80)),
        Case("q6", {"eax": C3G_REC}, c3g_pset_case(C3G_REC, 0x1040, s32=0xFFFF, s59=0x7F)),
        Case("q7", {"eax": C3G_REC}, c3g_pset_case(C3G_REC, 0x1000, s38=1, s64=0x00, s61=0x03000000,
                                                   s44=0x00040000, idx=1)),
    ], calls=(E.Call(0x2A620, ("eax", "edx"), mode="stub", clobbers=("edx",)),),
       # @cls and a lower layer-B clamp are equivalent mutations (record §C3g.3): layer B is
       # s8(s59)+0xF0 >= 0x70, and at s32 == 0 the two layer arms compute the same value.
       eax_mask=0, mutants=("@bit", "@x0", "@x2", "@gte", "@ramp", "@x44", "@div", "@ymask",
                            "@rec3c", "@bdc", "@cursor")),
    # 0x3BDDC fighter_attack_consume: EAX = side. 0x33950 runs real; 0x3CF38 and 0x4649C are
    # stubs; 0x3C480 is the HIT_A stub (hold 1.0 pushed).
    Spec("fighter_attack_consume", 0x3BDDC, [
        Case("a0", {"eax": 0}, c3g_attack_case(0, s40=0x80)),
        Case("a1", {"eax": 0}, c3g_attack_case(0, cmd=0x7FFF)),
        Case("a2", {"eax": 0}, c3g_attack_case(0, cmd=0x8000, chain=None), {0x3CF38: 1}),
        Case("a3", {"eax": 0}, c3g_attack_case(0, cmd=0x9000, chain=None),
             {0x3CF38: 0, 0x4649C: 5}),
        Case("a4", {"eax": 0}, c3g_attack_case(0, cmd=0x9000, chain=1), {0x4649C: 5}),
        Case("a5", {"eax": 0}, c3g_attack_case(0, cmd=0xA000, chain=None),
             {0x3CF38: 0, 0x4649C: 0}),
        Case("a6", {"eax": 0}, c3g_attack_case(0, cmd=0x8000, chain=None),
             {0x3CF38: 0, 0x4649C: 0}),
        Case("a7", {"eax": 1}, c3g_attack_case(1, cmd=0x9000, chain=None, ch=5),
             {0x3CF38: 0, 0x4649C: 3}),
    ], allow_calls=(0x33950,),
       calls=(E.Call(0x3CF38, ("eax",), mode="stub"),  # preserves EDI/EBP: §C3g.2
              E.Call(0x4649C, ("eax", "edx", "ebx", "ecx"), mode="stub",
                     clobbers=("ebx", "edx")),
              E.Call(0x3C480, ("eax", "edx", "s0"), mode="stub", pop=4, clobbers=("edx",))),
       eax_mask=0xFF, mutants=("@o40", "@cmd", "@clr", "@s5f", "@gate", "@chain", "@scan",
                               "@base", "@row", "@anim", "@state", "@dir", "@ret")),
    # 0x37178 fighter_37178: EAX = slot. 0x1A570 runs real; 0x36870 (ANIM54), 0x35838 (DIRS) and
    # 0x36638 are stubs. The base word is at 0x1078DC + ch_s*14 + ch_o*2; the facing/bits come
    # from the two records' +0x28.
    Spec("fighter_37178", 0x37178, [
        Case("r0", {"eax": 0x001077B0}, c3g_37178_case(0x0010AD00, 0x0010AE00, s51=0, word=0xFFFF)),
        Case("r1", {"eax": 0x001077B0}, c3g_37178_case(0x0010AD00, 0x0010AE00, s51=0, word=0x0100,
                                                       x18_s=0x1000, x18_o=0x1200, act_o=0x8000,
                                                       act_s=0)),
        Case("r2", {"eax": 0x001077B0}, c3g_37178_case(0x0010AD00, 0x0010AE00, s51=0, word=0x0100,
                                                       x18_s=0x2000, x18_o=0x1000, s28_s=0x4000,
                                                       s28_o=0, act_o=0, act_s=0x8000)),
        Case("r3", {"eax": 0x001077B0}, c3g_37178_case(0x0010AD00, 0x0010AE00, s51=0, word=0x0100,
                                                       x18_s=0x0F00, x18_o=0x1000, s28_s=0x4000,
                                                       s28_o=0, act_o=0, act_s=0x8000)),
        Case("r4", {"eax": 0x001077B0}, c3g_37178_case(0x0010AD00, 0x0010AE00, s51=0, word=0x0100,
                                                       x18_s=0x2000, x18_o=0x0F00, s28_s=0x4000,
                                                       s28_o=0, act_o=0x8000, act_s=0)),
        Case("r5", {"eax": 0x001077B0}, c3g_37178_case(0x0010AD00, 0x0010AE00, s51=0, word=0x0100,
                                                       x18_s=0x0800, x18_o=0x1000, s28_s=0x4000,
                                                       s28_o=0, act_o=0x8000, act_s=0x8000)),
        Case("r6", {"eax": 0x001077B0}, c3g_37178_case(0x0010AD00, 0x0010AE00, s51=0, word=0x0100,
                                                       x18_s=0x1000, x18_o=0x1050)),
        Case("r7", {"eax": 0x001077B0}, c3g_37178_case(0x0010AD00, 0x0010AE00, s51=0, word=0x0100,
                                                       x18_s=0x2000, x18_o=0x1000, act_o=0x8000,
                                                       act_s=0)),
        Case("r8", {"eax": 0x001077B0}, c3g_37178_case(0x0010AD00, 0x0010AE00, s51=0, word=0x0100,
                                                       x18_s=0x0F00, x18_o=0x1000, act_o=0x8000,
                                                       act_s=0)),
        Case("r9", {"eax": 0x001077B0}, c3g_37178_case(0x0010AD00, 0x0010AE00, s51=0, word=0x0100,
                                                       x18_s=0x2000, x18_o=0x0F00, act_o=0,
                                                       act_s=0x8000)),
        Case("r10", {"eax": 0x001077B0}, c3g_37178_case(0x0010AD00, 0x0010AE00, s51=0, word=0x0100,
                                                        x18_s=0x0F00, x18_o=0x2000, act_o=0,
                                                        act_s=0x8000)),
        Case("r11", {"eax": 0x001077B0}, c3g_37178_case(0x0010AD00, 0x0010AE00, s51=0, word=0x0100,
                                                        x18_s=0x3000, x18_o=0x1000, act_o=0,
                                                        act_s=0x8000)),
        Case("r12", {"eax": 0x001077B0}, c3g_37178_case(0x0010AD00, 0x0010AE00, s51=0, word=0x0500,
                                                        x18_s=0x1000, x18_o=0x1000, act_o=0x8000,
                                                        act_s=0)),
        Case("r13", {"eax": 0x001077B0}, c3g_37178_case(0x0010AD00, 0x0010AE00, s51=0, word=0x0100,
                                                        x18_s=0x1500, x18_o=0x1200, act_o=0,
                                                        act_s=0)),
    ], allow_calls=(0x1A570,),
       calls=(E.Call(0x36870, ("eax",), mode="stub", clobbers=("esi", "edi", "ebp")),
              E.Call(0x35838, ("eax", "edx", "ebx"), mode="stub", clobbers=("ebx", "edx")),
              E.Call(0x36638, ("eax", "edx"), mode="stub", clobbers=("edx",))),
        eax_mask=0, mutants=("@base", "@x", "@facing", "@bound", "@s52", "@fe", "@s18", "@call",
                             "@dirs", "@s57", "@s43", "@s54")),
    # ---- wave 2: the 0x3CF38 tree's predicates -----------------------------------------------
    # 0x3CCEC hit_stance_ok: EAX = side, EDX = i. The 0xC619C entry's +7 (e) selects stance 2,
    # +6 (d) stances 0/1.
    Spec("hit_stance_ok", 0x3CCEC, [
        Case("k0", {"eax": 0, "edx": 3}, c3g_stance_case(0, 3, 0, 2, 1, 0)),
        Case("k1", {"eax": 0, "edx": 3}, c3g_stance_case(0, 3, 0, 2, 0, 1)),
        Case("k2", {"eax": 1, "edx": 0}, c3g_stance_case(1, 0, 5, 1, 0, 1)),
        Case("k3", {"eax": 0, "edx": 0x1F}, c3g_stance_case(0, 0x1F, 6, 2, 1, 1, s7b=5)),
        Case("k4", {"eax": 0, "edx": 5}, c3g_stance_case(0, 5, 2, 3, 1, 0)),
        Case("k5", {"eax": 0, "edx": 7}, c3g_stance_case(0, 7, 3, 0, 0, 1)),
        Case("k6", {"eax": 1, "edx": 1}, c3g_stance_case(1, 1, 1, 1, 1, 0)),
        Case("k7", {"eax": 0, "edx": 0}, c3g_stance_case(0, 0, 0, 2, 0, 0)),
        Case("k8", {"eax": 0, "edx": 3}, c3g_stance_case(0, 3, 0, 2, 0, 0)),
    ], eax_mask=0xFFFFFFFF, mutants=("@e", "@st2", "@d", "@lo", "@entry", "@char", "@ret")),
    # 0x3CE24 hit_gate: EAX = side, EDX = i. 0x3CD94 (immunity) is a stub whose AL the raw tests
    # (0x3CE4F); the early reject's AL is 0 with the sign byte's high bits in EAX, so the mask is AL.
    Spec("hit_gate", 0x3CE24, [
        Case("g0", {"eax": 0, "edx": 0}, c3g_gate_case(0, 5), {0x3CD94: 1}),
        Case("g1", {"eax": 0, "edx": 0}, c3g_gate_case(0, 6), {0x3CD94: 1}),
        Case("g2", {"eax": 0, "edx": 0}, c3g_gate_case(0, 5), {0x3CD94: 0}),
        Case("g3", {"eax": 0, "edx": 3}, c3g_gate_case(0, 0x80), {0x3CD94: 1}),
        Case("g4", {"eax": 1, "edx": 7}, c3g_gate_case(1, 0xFF), {0x3CD94: 1}),
        Case("g5", {"eax": 0, "edx": 0}, c3g_gate_case(0, 5), {0x3CD94: 0x100}),
        Case("g6", {"eax": 0, "edx": 0x1F}, c3g_gate_case(0, 0), {0x3CD94: 0x100}),
        Case("g7", {"eax": 1, "edx": 2}, c3g_gate_case(1, 6), {0x3CD94: 1}),
        Case("g8", {"eax": 0, "edx": 4}, c3g_gate_case(0, 0x7F), {0x3CD94: 1}),
        Case("g9", {"eax": 0, "edx": 5}, c3g_gate_case(0, 5), {0x3CD94: 0x37}),
    ], calls=(E.Call(0x3CD94, ("eax", "edx"), mode="stub", clobbers=("edx",)),),
       eax_mask=0xFF, mutants=("@bound", "@signed", "@off", "@ret", "@call", "@al")),
    # 0x3C600 hit_frame_desc: EAX = side, EDX = i, ECX the caller's (the raw's default and its
    # sel>6 arm return ECX; the port returns 0, so every default case holds ECX 0 — named limit).
    Spec("hit_frame_desc", 0x3C600, [
        Case("d0", {"eax": 0, "edx": 3}, c3g_desc_case(0, 3, 1, ch=0)),
        Case("d1", {"eax": 0, "edx": 0}, c3g_desc_case(0, 0, 0, sel=0, ch=1)),
        Case("d2", {"eax": 0, "edx": 0x1F}, c3g_desc_case(0, 0x1F, 0, sel=4, ch=2)),
        Case("d3", {"eax": 0, "edx": 2}, c3g_desc_case(0, 2, 0, sel=6, ch=3)),
        Case("d4", {"eax": 1, "edx": 5}, c3g_desc_case(1, 5, 0, sel=2, ch=4)),
        Case("d5", {"eax": 0, "edx": 1}, c3g_desc_case(0, 1, 0, sel=7, ch=5)),
        Case("d6", {"eax": 1, "edx": 1}, c3g_desc_case(1, 1, 0, sel=1, ch=5)),
        Case("d7", {"eax": 0, "edx": 1}, c3g_desc_case(0, 1, 0, sel=0xFFFF, ch=6)),
    ], eax_mask=0xFFFFFFFF,
       mutants=("@s63", "@side", "@gt", "@table", "@entry", "@char", "@def", "@mask")),
    # 0x3CBC4/0x3CC58 hit_reaction_a/b: EAX = side. 0x33950 is allowed (the port reads the char
    # directly); 0x1DDF4 (C1_GEOM, its own row) is stubbed.
    Spec("hit_reaction_a", 0x3CBC4, [
        Case("a0", {"eax": 0}, c3g_react_case(0, 2, 0x0000)),
        Case("a1", {"eax": 0}, c3g_react_case(0, 0, 0x4000)),
        Case("a2", {"eax": 0}, c3g_react_case(0, 1, 0x0000), {0x1DDF4: 1}),
        Case("a3", {"eax": 0}, c3g_react_case(0, 1, 0x0000), {0x1DDF4: 0}),
        Case("a4", {"eax": 3}, c3g_react_case(0, 3, 0x0001), {0x1DDF4: 1}),
        Case("a5", {"eax": 1}, c3g_react_case(1, 2, 0x0000)),
        Case("a6", {"eax": 1}, c3g_react_case(1, 0, 0x4000)),
        Case("a7", {"eax": 0}, c3g_react_case(0, 2, 0x4000)),
    ], allow_calls=(0x33950,), calls=(C1_GEOM,),
       eax_mask=0xFFFFFFFF, mutants=("@st", "@bit", "@side", "@table", "@idx", "@ret", "@geom")),
    Spec("hit_reaction_b", 0x3CC58, [
        Case("b0", {"eax": 0}, c3g_react_b_case(0, 2, 0x0000)),
        Case("b1", {"eax": 0}, c3g_react_b_case(0, 0, 0x4000)),
        Case("b2", {"eax": 0}, c3g_react_b_case(0, 1, 0x0000), {0x1DDF4: 1}),
        Case("b3", {"eax": 0}, c3g_react_b_case(0, 1, 0x0000), {0x1DDF4: 0}),
        Case("b4", {"eax": 3}, c3g_react_b_case(0, 3, 0x0001), {0x1DDF4: 1}),
        Case("b5", {"eax": 1}, c3g_react_b_case(1, 2, 0x0000)),
        Case("b6", {"eax": 1}, c3g_react_b_case(1, 0, 0x4000)),
    ], allow_calls=(0x33950,), calls=(C1_GEOM,),
       eax_mask=0xFFFFFFFF, mutants=("@st", "@bit", "@side", "@table", "@idx", "@ret", "@geom")),
    # 0x4CE70 hit_reaction_allow: EAX = side, EDX = reaction; the per-char blocked-id list
    # (0x4CE54's jump table over the +0x7A char), 1 unless `reaction` is in it.
    Spec("hit_reaction_allow", 0x4CE70, [
        Case("r0", {"eax": 0, "edx": 0x28}, c3g_allow_case(0, 0)),
        Case("r1", {"eax": 0, "edx": 0x29}, c3g_allow_case(0, 0)),
        Case("r2", {"eax": 0, "edx": 0x2A}, c3g_allow_case(0, 0)),
        Case("r3", {"eax": 0, "edx": 0x2C}, c3g_allow_case(0, 0)),
        Case("r4", {"eax": 0, "edx": 0x2B}, c3g_allow_case(0, 0)),
        Case("r5", {"eax": 0, "edx": 0x24}, c3g_allow_case(0, 1)),
        Case("r6", {"eax": 0, "edx": 0x23}, c3g_allow_case(0, 3)),
        Case("r7", {"eax": 1, "edx": 0x26}, c3g_allow_case(1, 2)),
        Case("r8", {"eax": 1, "edx": 0x27}, c3g_allow_case(1, 2)),
        Case("r9", {"eax": 0, "edx": 0x24}, c3g_allow_case(0, 4)),
        Case("r10", {"eax": 0, "edx": 0x21}, c3g_allow_case(0, 5)),
        Case("r11", {"eax": 0, "edx": 0x24}, c3g_allow_case(0, 6)),
        Case("r12", {"eax": 0, "edx": 0x28}, c3g_allow_case(0, 7)),
        Case("r13", {"eax": 0, "edx": 0x28}, c3g_allow_case(0, 0xFF)),
        Case("r14", {"eax": 1, "edx": 0x22}, c3g_allow_case(1, 1)),
        Case("r15", {"eax": 0, "edx": 0x26}, c3g_allow_case(0, 4)),
        Case("r16", {"eax": 0, "edx": 0x22}, c3g_allow_case(0, 5)),
    ], eax_mask=0xFFFFFFFF,
       mutants=("@side", "@char", "@off", "@shift", "@d0", "@d2", "@inv", "@z")),
    # 0x38C5C fighter_38c5c: EAX = side. Four text calls, no reads: the row is the recorded call
    # list (0x2F280 x3 stubs and 0x2F314 x1 stub; 0x2F314 ignores the raw's ECX).
    Spec("fighter_38c5c", 0x38C5C, [
        Case("s0", {"eax": 0}, {}),
        Case("s1", {"eax": 1}, {}),
    ], calls=(E.Call(0x2F280, ("eax", "edx", "ebx", "ecx"), mode="stub", clobbers=("ebx", "ecx", "edx")),
              E.Call(0x2F314, ("eax", "edx", "ebx"), mode="stub", clobbers=("ebx", "ecx", "edx"))),
       eax_mask=0,
       mutants=("@c", "@two", "@r9", "@r10", "@s2", "@s6", "@s3", "@mode", "@col4", "@n4")),
    # 0x2F4BC text_cursor_hold: EAX = col, EDX = row, EBX = string, ECX = mode. 0x2F198 is a stub
    # (its by-value string dwords) that also writes 0x105F34, so the save/restore is observable.
    Spec("text_cursor_hold", 0x2F4BC, [
        Case("h0", {"eax": 3, "edx": 4, "ebx": 0x0010A200, "ecx": 0x1000},
             {0x00105F34: le32(0x11223344), 0x0010A200: b"\x11\x22\x33\x44\x55\x66\x77\x88"}),
        Case("h1", {"eax": 0xFFFFFFFF, "edx": 0xFFFFFFFF, "ebx": 0x0010A210, "ecx": 0x2000},
             {0x00105F34: le32(0xAABBCCDD), 0x0010A210: b"\x91\x82\x73\x64\x55\x46\x37\x28"}),
    ], calls=(E.Call(0x2F198, ("eax", "edx", "[ebx]", "[ebx+4]", "[ebx+8]", "[ebx+12]", "ecx"),
                     mode="stub", clobbers=("ebx", "ecx", "edx"),
                     writes=((None, 0x00105F34, b"\x5A\x5A\x5A\x5A"),)),),
       eax_mask=0, mutants=("@save", "@args", "@str", "@mode")),
    # 0x2F4D0 text_number_draw: EAX = col, EDX = row, EBX = value, ECX = width, [esp+4] = pad,
    # [esp+8] = mode; `ret 8`. 0x2EFD4 and 0x2F198 are stubs (by-value buffers; the 0x2F198 stub
    # writes 0x105F34 so the cursor save/restore is observable).
    Spec("text_number_draw", 0x2F4D0, [
        Case("n0", {"eax": 5, "edx": 6, "ebx": 0xFFFFFFD6, "ecx": 3, "s0": 0, "s1": 0x1000},
             {0x00105F34: le32(0x11223344)}),
        Case("n1", {"eax": 0, "edx": 0, "ebx": 12345, "ecx": 5, "s0": 2, "s1": 0x2000},
             {0x00105F34: le32(0xAABBCCDD)}),
    ], calls=(E.Call(0x2EFD4, ("eax", "[edx]", "[edx+4]", "[edx+8]", "[edx+12]", "ebx", "ecx"),
                     mode="stub", clobbers=("ebx", "ecx", "edx")),
              E.Call(0x2F198, ("eax", "edx", "[ebx]", "[ebx+4]", "[ebx+8]", "[ebx+12]", "ecx"),
                     mode="stub", clobbers=("ebx", "ecx", "edx"),
                     writes=((None, 0x00105F34, b"\x5A\x5A\x5A\x5A"),))),
       eax_mask=0, mutants=("@val", "@pad", "@mode", "@save", "@fmt", "@col", "@row")),
    # 0x2F20C text_vertical_set: EAX = col, EDX = row, EBX = string, ECX = mode. 0x2F0F0 (the
    # centring width) and 0x2F830 (vertical = 1) are stubs; the cursor 0x105F34 takes {row,
    # col + extent}, reloaded sign-extended when row == -1.
    Spec("text_vertical_set", 0x2F20C, [
        Case("v0", {"eax": 5, "edx": 6, "ebx": 0x0010A200, "ecx": 0x1000},
             {0x00105F34: le32(0x00010002)}, {0x2F830: 7}),
        Case("v1", {"eax": 0xFFFFFFFF, "edx": 4, "ebx": 0x0010A200, "ecx": 0x2000},
             {0x00105F34: le32(0x00010002)}, {0x2F0F0: 0x21, 0x2F830: 3}),
        Case("v2", {"eax": 0xFFFFFFFF, "edx": 4, "ebx": 0x0010A200, "ecx": 0x2000},
             {0x00105F34: le32(0x00010002)}, {0x2F0F0: 0x40, 0x2F830: 2}),
        Case("v3", {"eax": 7, "edx": 0xFFFFFFFF, "ebx": 0x0010A200, "ecx": 0x2000},
             {0x00105F34: le32(0x00040003)}, {0x2F830: 1}),
        Case("v4", {"eax": 0xFFFFFFFF, "edx": 0xFFFFFFFF, "ebx": 0x0010A200, "ecx": 0x2000},
             {0x00105F34: le32(0x00060005)}, {0x2F830: 0}),
        Case("v5", {"eax": 0, "edx": 0xFFFFFFFF, "ebx": 0x0010A200, "ecx": 0x2000},
             {0x00105F34: le32(0x0000FFF6)}, {0x2F830: 1}),
    ], calls=(E.Call(0x2F0F0, ("eax", "edx"), mode="stub", clobbers=("edx",)),
              E.Call(0x2F830, ("eax", "edx", "ecx", "ebx", "s0"), mode="stub", pop=4,
                     clobbers=("ebx", "ecx", "edx"))),
       # The raw's EAX at the ret is 0x2F830's return (its `add esi,eax` leaves EAX); the callers
       # read nothing, so the mask is 0.
       eax_mask=0,
       mutants=("@reload", "@neg", "@wargs", "@skip", "@vert", "@mode", "@ext", "@row", "@sext")),
    # 0x38ED0 fighter_38ed0: EAX = side, EDX = the 0x44-byte combo record. 0x33950 (ctx), 0x1C500
    # (the string, with its 0x474E4/0x1E75C/0x1E808/0x500BB tree) run real; 0x2F4BC is a stub. The
    # +0x2F need bytes compare unsigned; the +0x18 thresholds are ZERO-extended bytes against the
    # signed 0x107D2C word (the raw's `xor edx,edx; mov dl,..; cmp dx,cx; jg` at 0x38F06..0x38F15);
    # every return is AL (`xor al,al` / `mov al,1`), so the mask is 0xFF.
    Spec("fighter_38ed0", 0x38ED0, [
        Case("e0", {"eax": 0, "edx": C3G_38ED0_REC},
             c3g_38ed0_case(0, [0xFF], thr=(2, 3, 4), hit=5, hit_other=0, s63=0)),
        Case("e1", {"eax": 0, "edx": C3G_38ED0_REC},
             c3g_38ed0_case(0, [3, 0xFF], needs=[1], thr=(9, 2, 3), hit=5, s63=1,
                            table=[(3, 0x05)], table_other=[(3, 0x00)])),
        Case("e2", {"eax": 0, "edx": C3G_38ED0_REC},
             c3g_38ed0_case(0, [3, 0xFF], needs=[9], thr=(2, 3, 4), hit=5, s63=0,
                            table=[(3, 0x05)])),
        Case("e3", {"eax": 0, "edx": C3G_38ED0_REC},
             c3g_38ed0_case(0, [0xFF], thr=(9, 8, 7), hit=5, s63=1)),
        Case("e4", {"eax": 0, "edx": C3G_38ED0_REC},
             c3g_38ed0_case(0, [0xFF], thr=(9, 8, 7), hit=8, s63=1)),
        Case("e5", {"eax": 0, "edx": C3G_38ED0_REC},
             c3g_38ed0_case(0, [1] * 0x14, needs=[0] * 0x14, hit=5, s63=1, table=[(1, 0x01)])),
        Case("e6", {"eax": 0, "edx": C3G_38ED0_REC},
             c3g_38ed0_case(0, [1] * 19 + [0xFF], needs=[0] * 19, thr=(0, 9, 9), hit=5, s63=1,
                            table=[(1, 0x01)])),
        Case("e7", {"eax": 1, "edx": C3G_38ED0_REC},
             c3g_38ed0_case(1, [0xFF], thr=(0x80, 0x40, 0x20), hit=0x64, hit_other=0x30, s63=0)),
    ], allow_calls=(0x33950, 0x1C500, 0x474E4, 0x1E75C, 0x1E808, 0x500BB),
       calls=(E.Call(0x2F4BC, ("eax", "edx", "ebx", "ecx"), mode="stub",
                     clobbers=("ebx", "ecx", "edx")),),
       eax_mask=0xFF,
       mutants=("@bound", "@id", "@term", "@side", "@need", "@signed", "@s63", "@text", "@pair",
                "@hit")),
    # 0x34E2C hit_reaction_apply: EAX = side, EDX = reaction. 0x18B04/0x3C4CC/0x3C520/0x2C3FC are
    # stubs; 0x3AFC4 (the triple) runs real, 0x1922C (Task 4's row) and the bare-ret 0x29D60
    # callback run real through fn_resolve (the port skips the unregistered 0x29D60, the original's
    # allowed `ret` does nothing). Mask 0: a void function.
    Spec("hit_reaction_apply", 0x34E2C, [
        Case("h0", {"eax": 0, "edx": 0xFF}, c3g_apply_case(0, 0xFF, s54=2, fa=2, cb=C3G_APPLY_CB)),
        Case("h1", {"eax": 0, "edx": 0x12},
             c3g_apply_case(0, 0x12, s54=0, s84=0xFFFF)),
        Case("h2", {"eax": 0, "edx": 0x12},
             c3g_apply_case(0, 0x12, s54=2, fa=2, s62=0x62)),
        Case("h3", {"eax": 0, "edx": 0x12},
             c3g_apply_case(0, 0x12, s54=0, s52=0, s6a=5, stream=0x0010, bx=0x1000, voice=3)),
        Case("h4", {"eax": 0, "edx": 0x12},
             c3g_apply_case(0, 0x12, s54=2, s52=4, stream=0x0020, bx=0, voice=4)),
        Case("h5", {"eax": 0, "edx": 0x12},
             c3g_apply_case(0, 0x12, s54=0, cb=C3G_APPLY_CB)),
        Case("h6", {"eax": 0, "edx": 0x12},
             c3g_apply_case(0, 0x12, s54=0, stream=0x0030, bx=0x1000, voice=5, cb=C3G_APPLY_CB)),
        Case("h7", {"eax": 1, "edx": 0x12}, c3g_apply_case(1, 0x12, s54=0, fa=2)),
        Case("h8", {"eax": 0, "edx": 0x12},
             c3g_apply_case(0, 0x12, s54=0, st5a=5, st5c=0xFE, d24=0)),
    ], allow_calls=(0x33950, 0x1922C, 0x3AFC4, 0x29D60),
       calls=(E.Call(0x18B04, ("eax",), mode="stub"),
              E.Call(0x3C4CC, ("eax", "edx", "s0"), mode="stub", pop=4, clobbers=("edx",)),
              E.Call(0x3C520, ("eax", "edx", "s0"), mode="stub", pop=4, clobbers=("edx",)),
              E.Call(0x2C3FC, ("eax",), mode="stub")),
       eax_mask=0,
       mutants=("@ff", "@f88", "@gate", "@pair", "@facing", "@stance", "@s63", "@cnt", "@hi",
                "@anim", "@stream", "@bx", "@start", "@s52", "@cb", "@cbstore")),
    # ---- waves 3+4: the text tree's roots, 0x3CD94 and 0x1922C ---------------------------------
    # 0x2F0F0 text_width: EAX = string, EDX = mode; no callees (the strlen path is inline). Mode &
    # 3 in {0,1} is strlen; 2/3 sum 1 when the signed class table's entry width byte is 8, else 2.
    # Mask 0xFFFFFFFF: the centring callers use the return as a length.
    Spec("text_width", 0x2F0F0, [
        Case("w0", {"eax": C3G_T_STR, "edx": 0}, c3g_tw_case(b"ABCDE", 0)),
        Case("w1", {"eax": C3G_T_STR, "edx": 1}, c3g_tw_case(b"", 1)),
        Case("w2", {"eax": C3G_T_STR, "edx": 2},
             c3g_tw_case(b"AQ", 2, cls=[(0x41, 2), (0x51, 0xFF)], nbr=[(0x41, 0x11)],
                         wid=[(0xBD048, 2, 8)])),
        Case("w3", {"eax": C3G_T_STR, "edx": 2},
             c3g_tw_case(b"AR", 2, cls=[(0x41, 3), (0x52, 4)], nbr=[(0x41, 0x33)],
                         wid=[(0xBD048, 3, 8), (0xBD048, 4, 8)])),
        Case("w3b", {"eax": C3G_T_STR, "edx": 2},
             c3g_tw_case(b"RS", 2, cls=[(0x52, 4), (0x53, 5)],
                         wid=[(0xBD048, 4, 7), (0xBD048, 5, 7)])),
        Case("w4", {"eax": C3G_T_STR, "edx": 3},
             c3g_tw_case(b"AQ", 3, cls=[(0x41, 2), (0x51, 0xFF)], nbr=[(0x41, 0x22)],
                         wid=[(0xBD1EC, 2, 8)])),
        Case("w5", {"eax": C3G_T_STR, "edx": 0x1002},
             c3g_tw_case(b"AQ", 0x1002, cls=[(0x41, 2), (0x51, 0xFF)], nbr=[(0x41, 0x44)],
                         wid=[(0xBD048, 2, 8)])),
        Case("w6", {"eax": C3G_T_STR, "edx": 4}, c3g_tw_case(b"XYZ", 4)),
        Case("w7", {"eax": C3G_T_STR, "edx": 2},
             c3g_tw_case(b"", 2, cls=[(0x41, 2)], wid=[(0xBD048, 2, 8)])),
        Case("w8", {"eax": C3G_T_STR, "edx": 2},
             c3g_tw_case(b"\x80\x41", 2, cls=[(0x80, 5), (0x41, 2)], nbr=[(0x80, 0x55)],
                         wid=[(0xBD048, 5, 7), (0xBD048, 2, 8)])),
    ], eax_mask=0xFFFFFFFF, mutants=("@mask", "@table", "@cls", "@coff", "@wid", "@len")),
    # 0x2F198 text_cursor_set: EAX = col (-1 centres), EDX = row (-1 reloads the cursor), EBX =
    # string, ECX = mode. 0x2F0F0 (the centring width) and 0x2F830 (the render) are stubs; the
    # cursor at 0x105F34 takes {row, col + extent}. A void function.
    Spec("text_cursor_set", 0x2F198, [
        Case("t0", {"eax": 7, "edx": 4, "ebx": C3G_T_STR, "ecx": 0x1000},
             c3g_tstr_case(b"AB"), {0x2F830: 3}),
        Case("t1", {"eax": 9, "edx": 0xFFFFFFFF, "ebx": C3G_T_STR, "ecx": 0x1000},
             c3g_tstr_case(b"AB", {0x00105F34: le32(0x00060005)}), {0x2F830: 2}),
        Case("t2", {"eax": 9, "edx": 0xFFFFFFFF, "ebx": C3G_T_STR, "ecx": 0x2000},
             c3g_tstr_case(b"AB", {0x00105F34: le32(0xFFFBFFFE)}), {0x2F830: 1}),
        Case("t3", {"eax": 0xFFFFFFFF, "edx": 3, "ebx": C3G_T_STR, "ecx": 0x1000},
             c3g_tstr_case(b"AB"), {0x2F0F0: 0x20, 0x2F830: 4}),
        Case("t4", {"eax": 0xFFFFFFFF, "edx": 3, "ebx": C3G_T_STR, "ecx": 0x1000},
             c3g_tstr_case(b"AB"), {0x2F0F0: 0x30, 0x2F830: 2}),
        Case("t5", {"eax": 0xFFFFFFFF, "edx": 3, "ebx": C3G_T_STR, "ecx": 0x1000},
             c3g_tstr_case(b"AB"), {0x2F0F0: 0x2B, 0x2F830: 1}),
    ], calls=(E.Call(0x2F0F0, ("eax", "edx"), mode="stub", clobbers=("edx",)),
              E.Call(0x2F830, ("eax", "edx", "ecx", "ebx", "s0"), mode="stub", pop=4,
                     clobbers=("ebx", "ecx", "edx"))),
       eax_mask=0, mutants=("@reload", "@sext", "@wargs", "@neg", "@mode", "@row", "@ext")),
    # 0x2F280 text_cells_release: EAX = col (-1 centres), EDX = row, EBX = string, ECX = mode.
    # 0x2F0F0's return is the count (a stub); each non-empty grid word is released through 0x2AD40
    # (stub) and zeroed. A void function.
    Spec("text_cells_release", 0x2F280, [
        Case("c0", {"eax": 2, "edx": 1, "ebx": C3G_T_STR, "ecx": 0x1000},
             c3g_tstr_case(b"AB"), {0x2F0F0: 4}),
        Case("c0b", {"eax": 2, "edx": 1, "ebx": C3G_T_STR, "ecx": 0x1000},
             c3g_cell_case([(1 * 0xac + 2 * 4, 0), (1 * 0xac + 5 * 4, 3)], cur=0), {0x2F0F0: 4}),
        Case("c1", {"eax": 0xFFFFFFFF, "edx": 1, "ebx": C3G_T_STR, "ecx": 0x1000},
             c3g_cell_case([(1 * 0xac + 20 * 4, 1)], cur=0x5A5A5A5A), {0x2F0F0: 0x0C}),
        Case("c2", {"eax": 0xFFFFFFFF, "edx": 1, "ebx": C3G_T_STR, "ecx": 0x1000},
             c3g_cell_case([(1 * 0xac, 2), (1 * 0xac + 0x2A * 4, 1)]), {0x2F0F0: 0x30}),
        Case("c3", {"eax": 0x2A, "edx": 1, "ebx": C3G_T_STR, "ecx": 0x1000},
             c3g_cell_case([(1 * 0xac + 0x2A * 4, 0), (2 * 0xac, 1), (2 * 0xac + 4, 2),
                            (2 * 0xac + 8, 3)]),
             {0x2F0F0: 4}),
        Case("c4", {"eax": 0x2B, "edx": 1, "ebx": C3G_T_STR, "ecx": 0x1000},
             c3g_cell_case([(2 * 0xac, 2)]), {0x2F0F0: 2}),
        Case("c5", {"eax": 5, "edx": 2, "ebx": C3G_T_STR, "ecx": 0x1000},
             c3g_cell_case([(2 * 0xac + 5 * 4, 0)]), {0x2F0F0: 0}),
        Case("c6", {"eax": 0xFFFFFFFF, "edx": 2, "ebx": C3G_T_STR, "ecx": 0x1000},
             c3g_cell_case([(2 * 0xac + 0x15 * 4, 0)]), {0x2F0F0: 0}),
    ], calls=(E.Call(0x2F0F0, ("eax", "edx"), mode="stub", clobbers=("edx",)),
              E.Call(0x2AD40, ("eax", "edx"), mode="stub", clobbers=("edx",))),
       eax_mask=0,
       mutants=("@neg", "@force", "@cmp", "@stride", "@rowbase", "@zero", "@rel", "@pset",
                "@rec0", "@wm")),
    # 0x2F314 text_cells_release_vertical: EAX = col, EDX = row, EBX = string (the count is
    # strlen; ECX is ignored). The walk goes one row per cell and stops after a row above 0x1E.
    Spec("text_cells_release_vertical", 0x2F314, [
        Case("v0", {"eax": 2, "edx": 1, "ebx": C3G_T_STR},
             {**c3g_cell_case([(1 * 0xac + 2 * 4, 0), (2 * 0xac + 2 * 4, 1),
                               (3 * 0xac + 2 * 4, 2)]),
              **c3g_tstr_case(b"AB")}),
        Case("v1", {"eax": 3, "edx": 0x1E, "ebx": C3G_T_STR},
             {**c3g_cell_case([(0x1E * 0xac + 3 * 4, 2), (0x1F * 0xac + 3 * 4, 0),
                               (0x20 * 0xac + 3 * 4, 1)]),
              **c3g_tstr_case(b"ABC")}),
        Case("v2", {"eax": 3, "edx": 0x1F, "ebx": C3G_T_STR},
             {**c3g_cell_case([(0x1F * 0xac + 3 * 4, 1)]), **c3g_tstr_case(b"AB")}),
        Case("v3", {"eax": 5, "edx": 2, "ebx": C3G_T_STR},
             {**c3g_cell_case([(2 * 0xac + 5 * 4, 3)]), **c3g_tstr_case(b"A")}),
        Case("v4", {"eax": 5, "edx": 2, "ebx": C3G_T_STR},
             {**c3g_cell_case([]), **c3g_tstr_case(b"")}),
        Case("v5", {"eax": 2, "edx": 1, "ebx": C3G_T_STR},
             {**c3g_cell_case([(1 * 0xac + 2 * 4, 0)]), **c3g_tstr_case(b"ABCD")}),
    ], calls=(E.Call(0x2AD40, ("eax", "edx"), mode="stub", clobbers=("edx",)),),
       eax_mask=0, mutants=("@len", "@stride", "@stop", "@zero", "@rel", "@col", "@inc")),
    # 0x2F830 text_render: EAX = string, EDX = mode, EBX = col, ECX = row, [esp+4] = vertical.
    # 0x2F280 (the all-spaces clear), 0x2F0F0 (the width) and 0x2F5A0 (the glyph; its col/row
    # pointers are dereferenced by the call record) are stubs. The string is truncated in place
    # (0x2A/0x1E), so the case's pokes see the writes. Mask 0xFFFFFFFF: the count is returned.
    Spec("text_render", 0x2F830, [
        Case("r0", {"eax": C3G_T_STR, "edx": 0x1000, "ebx": 5, "ecx": 4, "s0": 0},
             c3g_tstr_case(b"")),
        Case("r1", {"eax": C3G_T_STR, "edx": 0x1000, "ebx": 5, "ecx": 4, "s0": 0},
             c3g_tstr_case(b"   ")),
        Case("r2", {"eax": C3G_T_STR, "edx": 0x1000, "ebx": 5, "ecx": 4, "s0": 0},
             c3g_tstr_case(b"AB"), {0x2F0F0: 4}),
        Case("r2b", {"eax": C3G_T_STR, "edx": 0x1000, "ebx": 5, "ecx": 4, "s0": 0},
             c3g_tstr_case(b"  A"), {0x2F0F0: 3}),
        Case("r3", {"eax": C3G_T_STR, "edx": 0x1000, "ebx": 0, "ecx": 0, "s0": 0},
             c3g_tstr_case(b"C" * 0x30), {0x2F0F0: 0x30}),
        Case("r4", {"eax": C3G_T_STR, "edx": 0x1000, "ebx": 0x20, "ecx": 0, "s0": 0},
             c3g_tstr_case(b"C" * 0x10), {0x2F0F0: 0x10}),
        Case("r5", {"eax": C3G_T_STR, "edx": 0x1000, "ebx": 5, "ecx": 0x18, "s0": 1},
             c3g_tstr_case(b"C" * 0x10), {0x2F0F0: 0x10}),
        Case("r6", {"eax": C3G_T_STR, "edx": 0x1000, "ebx": 5, "ecx": 4, "s0": 1},
             c3g_tstr_case(b"C" * 0x28), {0x2F0F0: 0x28}),
        Case("r7", {"eax": C3G_T_STR, "edx": 0x1000, "ebx": 5, "ecx": 4, "s0": 0},
             c3g_tstr_case(b"AB"), {0x2F0F0: 4, 0x2F5A0: 1}),
        Case("r8", {"eax": C3G_T_STR, "edx": 0x1000, "ebx": 5, "ecx": 4, "s0": 0},
             c3g_tstr_case(b"AB"), {0x2F0F0: 4, 0x2F5A0: 0x100}),
        Case("r9", {"eax": C3G_T_STR, "edx": 0x1000, "ebx": 0, "ecx": 0, "s0": 0},
             c3g_tstr_case(b"C" * 0x2B), {0x2F0F0: 0x2A}),
    ], calls=(E.Call(0x2F280, ("eax", "edx", "ebx", "ecx"), mode="stub",
                     clobbers=("ebx", "ecx", "edx")),
              E.Call(0x2F0F0, ("eax", "edx"), mode="stub", clobbers=("edx",)),
              E.Call(0x2F5A0, ("eax", "[edx]", "[ebx]", "ecx", "s0"), mode="stub", pop=4,
                     clobbers=("ebx", "ecx", "edx"))),
       eax_mask=0xFFFFFFFF,
       mutants=("@empty", "@spaces", "@rel", "@wargs", "@t30", "@t1e", "@emit", "@stop", "@cnt",
                "@adv")),
    # 0x2EF24 text_number_core: EAX = value, EDX = dest. 0x65546 (the libc sprintf wrapper) is a
    # stub that writes the case's digits; the return is the strlen the raw computes itself.
    Spec("text_number_core", 0x2EF24, [
        Case("n0", {"eax": 42, "edx": C3G_T_DEST},
             {C3G_T_DEST: b"\xA5" * 8}),
        Case("n1", {"eax": 42, "edx": C3G_T_DEST + 0x20},
             {C3G_T_DEST + 0x20: b"\xA5" * 8}),
        Case("n2", {"eax": 42, "edx": C3G_T_DEST + 0x40},
             {C3G_T_DEST + 0x40: b"\xA5" * 8}),
    ], calls=(E.Call(0x65546, ("s0", "s1", "s2"), mode="stub",
                     writes=((0, 0, b"42\x00"),)),),
       eax_mask=0xFFFFFFFF, mutants=("@val", "@dest", "@call", "@len")),
    # 0x2EFD4 text_number_format: EAX = value, EDX = dest, EBX = width, ECX = pad. 0x2EF24 (the
    # core; its buffer is the raw's zero stack, by value) and 0x61A70 (the libc memset wrapper,
    # EAX/EDX/EBX = dest/fill/len) are stubs; the digits copy and the terminator stay observable.
    Spec("text_number_format", 0x2EFD4, [
        Case("f0", {"eax": 42, "edx": C3G_T_DEST, "ebx": 5, "ecx": 0},
             {C3G_T_DEST: b"\xA5" * 8}, {0x2EF24: 2}),
        Case("f1", {"eax": 42, "edx": C3G_T_DEST, "ebx": 5, "ecx": 1},
             {C3G_T_DEST: b"\xA5" * 8}, {0x2EF24: 2}),
        Case("f2", {"eax": 42, "edx": C3G_T_DEST, "ebx": 5, "ecx": 2},
             {C3G_T_DEST: b"\xA5" * 8}, {0x2EF24: 2}),
        Case("f3", {"eax": 42, "edx": C3G_T_DEST, "ebx": 5, "ecx": 3},
             {C3G_T_DEST: b"\xA5" * 8}, {0x2EF24: 2}),
        Case("f4", {"eax": 42, "edx": C3G_T_DEST, "ebx": 5, "ecx": 4},
             {C3G_T_DEST: b"\xA5" * 8}, {0x2EF24: 2}),
        Case("f5", {"eax": 42, "edx": C3G_T_DEST, "ebx": 2, "ecx": 0},
             {C3G_T_DEST: b"\xA5" * 8}, {0x2EF24: 5}),
        Case("f6", {"eax": 42, "edx": C3G_T_DEST, "ebx": 3, "ecx": 1},
             {C3G_T_DEST: b"\xA5" * 8}, {0x2EF24: 3}),
        Case("f7", {"eax": 42, "edx": C3G_T_DEST, "ebx": 4, "ecx": 0},
             {C3G_T_DEST: b"\xA5" * 8}, {0x2EF24: 0}),
        Case("f8", {"eax": 42, "edx": C3G_T_DEST, "ebx": 2, "ecx": 3},
             {C3G_T_DEST: b"\xA5" * 8}, {0x2EF24: 0x10}),
        Case("f9", {"eax": 42, "edx": C3G_T_DEST, "ebx": 0, "ecx": 2},
             {C3G_T_DEST: b"\xA5" * 8}, {0x2EF24: 2}),
        Case("f10", {"eax": 42, "edx": C3G_T_DEST, "ebx": 1, "ecx": 3},
             {C3G_T_DEST: b"\xA5" * 8}, {0x2EF24: 0x14}),
    ], calls=(E.Call(0x2EF24, ("eax", "[edx]", "[edx+4]", "[edx+8]", "[edx+12]"), mode="stub"),
              E.Call(0x61A70, ("eax", "edx", "ebx"), mode="stub", clobbers=("edx",))),
       eax_mask=0xFFFFFFFF,
       mutants=("@gap", "@copy", "@pad", "@fill", "@end", "@ret", "@n", "@m2")),
    # 0x2F5A0 text_glyph_emit: EAX = char, EDX = &col, EBX = &row, ECX = mode, [esp+4] =
    # vertical. 0x2AD40 is a stub; 0x2AE14 runs real (allowed) with its exercised tree:
    # 0x2AC80/0x249D0/0x249B0 (the list splice), 0x2EA30 (the inert lock), 0x2A408 (the literal
    # sprite id), 0x5D812 (the type table's bare ret), 0x1C390/0x1C3A0 (the render merge), and
    # 0x33754 (palette acquire) and 0x2A820 (pset write) are stubs. Mask 0xFF (AL = failure).
    Spec("text_glyph_emit", 0x2F5A0, [
        Case("g0", {"eax": 0x41, "edx": C3G_T_COLROW, "ebx": C3G_T_COLROW + 4, "ecx": 0x0000,
                    "s0": 0},
             c3g_glyph_case(0x41, 2, 3, 0x0000), {0x33754: 0x11111111}),
        Case("g1", {"eax": 0x41, "edx": C3G_T_COLROW, "ebx": C3G_T_COLROW + 4, "ecx": 0x1000,
                    "s0": 0},
             c3g_glyph_case(0x41, 2, 3, 0x1000), {0x33754: 0x22222222}),
        Case("g2", {"eax": 0x41, "edx": C3G_T_COLROW, "ebx": C3G_T_COLROW + 4, "ecx": 0x2000,
                    "s0": 0},
             c3g_glyph_case(0x41, 2, 3, 0x2000), {0x33754: 0x33333333}),
        Case("g3", {"eax": 0x41, "edx": C3G_T_COLROW, "ebx": C3G_T_COLROW + 4, "ecx": 0x3000,
                    "s0": 0},
             c3g_glyph_case(0x41, 2, 3, 0x3000), {0x33754: 0x44444444}),
        Case("g4", {"eax": 0x41, "edx": C3G_T_COLROW, "ebx": C3G_T_COLROW + 4, "ecx": 0x4000,
                    "s0": 0},
             c3g_glyph_case(0x41, 2, 3, 0x4000), {0x33754: 0x55555555}),
        Case("g5", {"eax": 0x41, "edx": C3G_T_COLROW, "ebx": C3G_T_COLROW + 4, "ecx": 0x5000,
                    "s0": 0},
             c3g_glyph_case(0x41, 2, 3, 0x5000), {0x33754: 0x66666666}),
        Case("g6", {"eax": 0x41, "edx": C3G_T_COLROW, "ebx": C3G_T_COLROW + 4, "ecx": 0x6000,
                    "s0": 0},
             c3g_glyph_case(0x41, 2, 3, 0x6000), {0x33754: 0x77777777}),
        Case("g7", {"eax": 0x41, "edx": C3G_T_COLROW, "ebx": C3G_T_COLROW + 4, "ecx": 0x8000,
                    "s0": 0},
             c3g_glyph_case(0x41, 2, 3, 0x8000), {0x33754: 0x88888888}),
        Case("g8", {"eax": 0x41, "edx": C3G_T_COLROW, "ebx": C3G_T_COLROW + 4, "ecx": 0x8800,
                    "s0": 0},
             c3g_glyph_case(0x41, 2, 3, 0x8800), {0x33754: 0x99999999}),
        Case("g9", {"eax": 0x41, "edx": C3G_T_COLROW, "ebx": C3G_T_COLROW + 4, "ecx": 0x9000,
                    "s0": 0},
             c3g_glyph_case(0x41, 2, 3, 0x9000), {0x33754: 0xAAAAAAAA}),
        Case("g10", {"eax": 0x41, "edx": C3G_T_COLROW, "ebx": C3G_T_COLROW + 4, "ecx": 0x9800,
                     "s0": 0},
             c3g_glyph_case(0x41, 2, 3, 0x9800), {0x33754: 0xBBBBBBBB}),
        Case("g11", {"eax": 0x41, "edx": C3G_T_COLROW, "ebx": C3G_T_COLROW + 4, "ecx": 0xA000,
                     "s0": 0},
             c3g_glyph_case(0x41, 2, 3, 0xA000), {0x33754: 0xCCCCCCCC}),
        Case("g12", {"eax": 0x41, "edx": C3G_T_COLROW, "ebx": C3G_T_COLROW + 4, "ecx": 0xA800,
                     "s0": 0},
             c3g_glyph_case(0x41, 2, 3, 0xA800), {0x33754: 0xDDDDDDDD}),
        Case("g13", {"eax": 0x41, "edx": C3G_T_COLROW, "ebx": C3G_T_COLROW + 4, "ecx": 0xB000,
                     "s0": 0},
             c3g_glyph_case(0x41, 2, 3, 0xB000), {0x33754: 0xEEEEEEEE}),
        Case("g14", {"eax": 0x41, "edx": C3G_T_COLROW, "ebx": C3G_T_COLROW + 4, "ecx": 0xB800,
                     "s0": 0},
             c3g_glyph_case(0x41, 2, 3, 0xB800), {0x33754: 0x01020304}),
        Case("g15", {"eax": 0x41, "edx": C3G_T_COLROW, "ebx": C3G_T_COLROW + 4, "ecx": 0xC000,
                     "s0": 0},
             c3g_glyph_case(0x41, 2, 3, 0xC000), {0x33754: 0x05060708}),
        Case("g16", {"eax": 0x41, "edx": C3G_T_COLROW, "ebx": C3G_T_COLROW + 4, "ecx": 0xC800,
                     "s0": 0},
             c3g_glyph_case(0x41, 2, 3, 0xC800), {0x33754: 0x090A0B0C}),
        Case("g17", {"eax": 0x41, "edx": C3G_T_COLROW, "ebx": C3G_T_COLROW + 4, "ecx": 0xD000,
                     "s0": 0},
             c3g_glyph_case(0x41, 2, 3, 0xD000), {0x33754: 0x0D0E0F10}),
        Case("g18", {"eax": 0x41, "edx": C3G_T_COLROW, "ebx": C3G_T_COLROW + 4, "ecx": 0xE000,
                     "s0": 0},
             c3g_glyph_case(0x41, 2, 3, 0xE000), {0x33754: 0x11121314}),
        Case("g19", {"eax": 0x41, "edx": C3G_T_COLROW, "ebx": C3G_T_COLROW + 4, "ecx": 0xF000,
                     "s0": 0},
             c3g_glyph_case(0x41, 2, 3, 0xF000), {0x33754: 0x15161718}),
        Case("g20", {"eax": 0x20, "edx": C3G_T_COLROW, "ebx": C3G_T_COLROW + 4, "ecx": 0x1000,
                     "s0": 0},
             c3g_glyph_case(0x20, 2, 3, 0x1000), {0x33754: 0x191A1B1C}),
        Case("g21", {"eax": 0x41, "edx": C3G_T_COLROW, "ebx": C3G_T_COLROW + 4, "ecx": 2, "s0": 0},
             {**c3g_glyph_case(0x41, 2, 3, 2),
              0x000BD390 + 0x41: b"\xFF"}),
        Case("g22", {"eax": 0x41, "edx": C3G_T_COLROW, "ebx": C3G_T_COLROW + 4, "ecx": 3, "s0": 0},
             {**c3g_glyph_case(0x41, 2, 3, 3),
              0x000BD390 + 0x41: b"\x00", 0x000BD1EC: b"\x44\x3F\x02\x02",
              0x000BD048: b"\x99\x3E\x03\x03"},
             {0x33754: 0x1D1E1F20}),
        Case("g23", {"eax": 0x41, "edx": C3G_T_COLROW, "ebx": C3G_T_COLROW + 4, "ecx": 0x1000,
                     "s0": 0},
             c3g_glyph_case(0x41, 2, 3, 0x1000, cell=C3G_T_CELL, pset_idx=3),
             {0x33754: 0x21222324}),
        Case("g24", {"eax": 0x41, "edx": C3G_T_COLROW, "ebx": C3G_T_COLROW + 4, "ecx": 0x1000,
                     "s0": 0},
             c3g_glyph_case(0x41, 2, 3, 0x1000, free=False), {0x33754: 0x25262728}),
        Case("g25", {"eax": 0x41, "edx": C3G_T_COLROW, "ebx": C3G_T_COLROW + 4, "ecx": 0x1000,
                     "s0": 1},
             c3g_glyph_case(0x41, 2, 3, 0x1000, vertical=1), {0x33754: 0x292A2B2C}),
        Case("g26", {"eax": 0x141, "edx": C3G_T_COLROW, "ebx": C3G_T_COLROW + 4, "ecx": 0x1000,
                     "s0": 0},
             c3g_glyph_case(0x41, 2, 3, 0x1000), {0x33754: 0x2D2E2F30}),
        Case("g27", {"eax": 0x41, "edx": C3G_T_COLROW, "ebx": C3G_T_COLROW + 4, "ecx": 6, "s0": 0},
             {**c3g_glyph_case(0x41, 2, 3, 6),
              0x000BD390 + 0x41: b"\x00", 0x000BD048: b"\x77\x3F\x10\x10",
              0x000BD1EC: b"\x99\x3E\x03\x03"},
             {0x33754: 0x31323334}),
        Case("g28", {"eax": 0x41, "edx": C3G_T_COLROW, "ebx": C3G_T_COLROW + 4, "ecx": 0x3002,
                     "s0": 0},
             {**c3g_glyph_case(0x41, 2, 3, 0x3002),
              0x000BD390 + 0x41: b"\x00", 0x000BD048: b"\x77\x3F\x10\x10"},
             {0x33754: 0x35363738}),
        Case("g29", {"eax": 0x41, "edx": C3G_T_COLROW, "ebx": C3G_T_COLROW + 4, "ecx": 0x3002,
                     "s0": 1},
             {**c3g_glyph_case(0x41, 2, 3, 0x3002, vertical=1),
              0x000BD390 + 0x41: b"\x00", 0x000BD048: b"\x77\x3F\x10\x10"},
             {0x33754: 0x393A3B3C}),
    ], allow_calls=(0x2AE14, 0x2AC80, 0x249D0, 0x249B0, 0x2EA30, 0x2A408, 0x5D812,
                    0x1C390, 0x1C3A0),
       calls=(E.Call(0x2AD40, ("eax", "edx"), mode="stub", clobbers=("edx",)),
              E.Call(0x33754, ("eax",), mode="stub"),
              E.Call(0x2A820, ("eax", "edx"), mode="stub", clobbers=("edx",))),
       eax_mask=0xFF,
       unhit_named={0x2F647: "dead: `and eax,0xf000` at 0x2F5BB limits the mhi switch to multiples of 0x1000, so no value falls in (0xB000,0xC000)",
                    0x2F660: "dead: the same mask, no value falls in (0x9000,0xA000)",
                    0x2F684: "dead: the same mask, no value falls in (0x3000,0x4000)",
                    0x2F6D8: "dead: ESI was masked to 0xFF at 0x2F5AC, so the 0x2EB5..0x2EC5 arm cannot be entered",
                    0x2F6E0: "dead: the same 0x2EB5..0x2EC5 arm's body (reached only through 0x2F6D8)"},
       mutants=("@cls", "@tab", "@pal0", "@pal1", "@pal2", "@pal3", "@pal4", "@class", "@rel",
                "@sp", "@desc", "@spawn", "@fail", "@store", "@adv", "@ch")),
    # 0x3CD94 hit_immunity: EAX = side, EDX = i. No callees. A fresh reaction 0xFF is immune; a
    # reaction 0..0x3F tests the per-character mask's bit for the candidate reaction (0xC619C's
    # +4 word); the two dwords at 0xA182C + ch*0x200 + r*8 split at 0x20. Mask 0xFF (AL tested).
    Spec("hit_immunity", 0x3CD94, [
        Case("i0", {"eax": 0, "edx": 0}, c3g_imm_case(0, 0, 0xFF, 1, 5)),
        Case("i1", {"eax": 0, "edx": 0}, c3g_imm_case(0, 0, 0x40, 1, 0, lo=1)),
        Case("i2", {"eax": 0, "edx": 0}, c3g_imm_case(0, 0, 0x80, 1, 5)),
        Case("i3", {"eax": 0, "edx": 0}, c3g_imm_case(0, 0, 5, 1, 3, lo=0x00000008)),
        Case("i4", {"eax": 0, "edx": 0}, c3g_imm_case(0, 0, 5, 1, 3, lo=0xFFFFFFF7)),
        Case("i5", {"eax": 0, "edx": 0}, c3g_imm_case(0, 0, 5, 1, 0x23, hi=0x00000008)),
        Case("i6", {"eax": 0, "edx": 0}, c3g_imm_case(0, 0, 5, 1, 0x23, hi=0xFFFFFFF7)),
        Case("i7", {"eax": 5, "edx": 2}, c3g_imm_case(5, 2, 5, 5, 3, lo=0x00000008)),
        Case("i8", {"eax": 1, "edx": 2}, c3g_imm_case(1, 2, 5, 6, 3, lo=0x00000008, ch2=0)),
        Case("i9", {"eax": 0, "edx": 9}, c3g_imm_case(0, 9, 5, 1, 0x1F, lo=0x80000000)),
        Case("i10", {"eax": 0, "edx": 0}, c3g_imm_case(0, 0, 5, 1, 0xFFFF, lo=0x80000000)),
        Case("i11", {"eax": 0, "edx": 0}, c3g_imm_case(0, 0, 5, 1, 0, lo=0x00000001)),
    ], eax_mask=0xFF,
       mutants=("@ff", "@bound", "@cmp", "@char", "@entry", "@rmul", "@hi", "@inv")),
    # 0x1922C hit_stance_timer: EAX = side. 0x33950 runs real (allowed; the port reads the record
    # directly). B5A > 0 (signed) and rec+0x24's magnitude zero seed the float from B5C and clamp
    # outside [1.0, 0x8058C]; then B5A (taken arm only) and B5E clear. A void function.
    Spec("hit_stance_timer", 0x1922C, [
        Case("s0", {"eax": 0}, c3g_timer_case(0, 5, 100, 0)),
        Case("s1", {"eax": 0}, c3g_timer_case(0, 5, 5, 0)),
        Case("s2", {"eax": 0}, c3g_timer_case(0, 5, 0, 0)),
        Case("s3", {"eax": 0}, c3g_timer_case(0, 5, 0xFB, 0)),
        Case("s4", {"eax": 0}, c3g_timer_case(0, 5, 10, 0)),
        Case("s5", {"eax": 0}, c3g_timer_case(0, 5, 1, 0)),
        Case("s6", {"eax": 0}, c3g_timer_case(0, 0, 7, 0)),
        Case("s7", {"eax": 0}, c3g_timer_case(0, 0x80, 7, 0)),
        Case("s8", {"eax": 0}, c3g_timer_case(0, 5, 7, 0x80000000)),
        Case("s9", {"eax": 1}, c3g_timer_case(1, 5, 5, 0)),
        Case("s10", {"eax": 0}, c3g_timer_case(0, 5, 7, 0x3F800000)),
        Case("s11", {"eax": 1}, c3g_timer_case(1, 1, 0xFB, 0x3F800000)),
        Case("s12", {"eax": 0}, c3g_timer_case(0, 5, 11, 0)),
    ], allow_calls=(0x33950,), eax_mask=0,
       mutants=("@clear", "@sign", "@mag", "@seed", "@clamp", "@limit", "@val", "@eclear", "@side")),
]


# ---- track D1 (record 2026-10-11-reverse-d1): rows for the ported functions the callee scan never
# reached. Each batch's block goes between this list and SPECS and ends with `D1_SPECS += D1x_SPECS`.
D1_SPECS = []


# ---- track D1 batch d1a (record 2026-10-11-reverse-d1a): game/fighter.c, first half --------------
#
# Every index a case passes (side, state, character) keeps base + index * size inside the image for
# the binding and for each mutant (the brief's mem[] rule); a mutant's wrong read is seeded with the
# opposite answer, never left to whatever the image holds.

D1A_SLOT = 0x001077B0            # DS_001077B0: the two 0x94-byte fighter slots
D1A_ROW, D1A_ROW2 = 0x0010A500, 0x0010A900   # 0x18950: the pair row and the wrong-index row (zero BSS)
D1A_HOOK = 0x0003E484            # a registered slot +0x18 hook (fighter_3e484, actors.c fn_register)


def d1a_gate_case(side, pose_o, st_o, word_s, word_o, pose_s=0x00011111, st_s=0x00, nxt=0):
    """0x14590: the other slot's +0x10 dword and +0x52/+0x53 bytes, the own slot's (what @slot
    reads), the two 0x107D2C words and the word after them (the high half of @wide's side-1 read)."""
    other = 1 - side
    so, ss = D1A_SLOT + other * 0x94, D1A_SLOT + side * 0x94
    words = [0, 0]
    words[side], words[other] = word_s, word_o
    return {so + 0x10: le32(pose_o), so + 0x52: bytes([st_o, 0x77]),
            ss + 0x10: le32(pose_s), ss + 0x52: bytes([st_s, 0x66]),
            0x00107D2C: le16(words[0]) + le16(words[1]) + le16(nxt)}


def d1a_latch_case(bit3, o41, anchor_same):
    """0x186C4: per side the +0x42 bit 3, the +0x41 bit 7 and whether the +0x20 anchor already
    equals DS_00100AF0[side]; the records' +0x18/+0x1C, the offset pairs and the three latched
    dwords are slot-distinct sentinels."""
    p = dict(SLOT_PTRS)
    p[C2_ANCHOR] = le32(0x1111) + le32(0x2222)
    p[C2_OFFS] = le32(0x64) + le32(0x65) + le32(0x74) + le32(0x75)
    for side in (0, 1):
        so = DS_SLOTS + side * 0x94
        rec = (E3_REC, E3_REC2)[side]
        anchor = (0x1111, 0x2222)[side]
        p[so + 0x20] = le32(anchor if anchor_same[side] else 0x24242424 + side) + le32(0x99999999)
        p[so + 0x2C] = le32(0x2C2C2C2C + side) + le32(0x30303030 + side) + le32(0x34343434 + side)
        p[so + 0x41] = bytes([o41[side], 0x08 if bit3[side] else 0x00])
        p[rec + 0x18] = le32(0x18181818 + side * 0x01010101) + le32(0x1C1C1C1C + side * 0x01010101)
    return p


def d1a_conn_case(p1, p2, st1, st2, ch1, ch2, want):
    """0x18950: both slots' +0x5F and +0x7A, the pair-table dword of (ch1, ch2) pointing at D1A_ROW
    and the tested dword pair at D1A_ROW + st1*8 holding exactly the answer `want` (the tested bit
    alone, or every bit but it). Every wrong read a mutant makes holds the opposite answer: the
    swapped pair index and the sign-extended character index point at D1A_ROW2 (all bits = not
    want), and D1A_ROW's other dword pairs (the swapped state roles, st1 sign-extended) are filled
    with not want."""
    s1, s2 = D1A_SLOT + p1 * 0x94, D1A_SLOT + p2 * 0x94
    bit = 1 << (st2 & 0x1F)
    hi = st2 >= 0x20
    yes, no = (bit, 0) if want else (~bit & 0xFFFFFFFF, 0xFFFFFFFF)
    other = le32(0 if want else 0xFFFFFFFF)
    p = {s1 + 0x5F: bytes([st1]), s1 + 0x7A: bytes([ch1]),
         0x000A1290 + (ch2 + ch1 * 10) * 4: le32(D1A_ROW)}
    if p2 != p1:
        p[s2 + 0x5F] = bytes([st2])
        p[s2 + 0x7A] = bytes([ch2])
    for a in {(ch1 + ch2 * 10) * 4, (ch2 + ((ch1 ^ 0x80) - 0x80) * 10) * 4} - {(ch2 + ch1 * 10) * 4}:
        p[0x000A1290 + a] = le32(D1A_ROW2)
    for a in {st2 * 8, ((st1 ^ 0x80) - 0x80) * 8} - {st1 * 8}:
        p[D1A_ROW + a] = other * 2
    p[D1A_ROW2 + st1 * 8] = other * 2
    p[D1A_ROW2 + st2 * 8] = other * 2
    p[D1A_ROW2 + ((st1 ^ 0x80) - 0x80) * 8] = other * 2
    p[D1A_ROW + st1 * 8] = le32(no if hi else yes) + le32(yes if hi else no)
    return p


def d1a_hook_case(side, hook_s, hook_o):
    """0x19020: the side slot's +0x18 hook and the other's (what @side reads), and both
    DS_00100AF8 dwords seeded 0x5A5A5A5A (neither 0 nor 1)."""
    return {**SLOT_PTRS, D1A_SLOT + side * 0x94 + 0x18: le32(hook_s),
            D1A_SLOT + (1 - side) * 0x94 + 0x18: le32(hook_o),
            0x00100AF8: le32(0x5A5A5A5A) * 2}


def d1a_passb_case(st, b58, b5a, b5c, b5e, x24=(0, 0), s52=(0, 0), flags=0):
    """0x19068: per side the slot's +0x52, +0x55..+0x57 (seeded 0x65 0x66 0x67) and +0x5F, the latched DS_00100B58,
    the timers B5A/B5C/B5E, the record's +0x20 (seeded) and +0x24; and the 0x107EE0 flag word."""
    p = c3d_slot_pokes()
    p[0x00100B58] = bytes([b58[0], b58[1], b5a[0], b5a[1], b5c[0], b5c[1], b5e[0], b5e[1]])
    p[0x00107EE0] = le32(flags)
    for side in (0, 1):
        so = D1A_SLOT + side * 0x94
        rec = (C3D_REC0, C3D_REC1)[side]
        p[so + 0x52] = bytes([s52[side]])
        p[so + 0x55] = bytes([0x65, 0x66, 0x67])
        p[so + 0x5F] = bytes([st[side]])
        p[rec + 0x20] = le32(0x5A5A5A50 + side) + le32(x24[side])
    return p


def d1a_f32(x):
    """The IEEE single bits of x, as the dword the case pokes."""
    import struct
    return struct.unpack("<I", struct.pack("<f", x))[0]


def d1a_seed_case(side, x20, x24, st=0x21):
    """0x19164: both slots' record pointers and +0x5F (the other side's differs), the side
    record's +0x20/+0x24 dwords as given and the other record's as 9.0/9.0 (what @rec reads), and
    the eight B58..B5F bytes seeded 0x91..0x98."""
    p = c3d_slot_pokes()
    recs = (C3D_REC0, C3D_REC1)
    p[0x00100B58] = bytes(range(0x91, 0x99))
    p[recs[side] + 0x20] = le32(x20) + le32(x24)
    p[recs[1 - side] + 0x20] = le32(0x41100000) * 2
    p[D1A_SLOT + side * 0x94 + 0x5F] = bytes([st])
    p[D1A_SLOT + (1 - side) * 0x94 + 0x5F] = bytes([st ^ 0x55])
    return p


def d1a_list_case(c0, r0, c1, r1, ae8=0x03):
    """0x19820: the 0x88 bytes 0x100C20..0x100CA7 filled with the in-image pointer 0x10A040 (a
    dropped sentinel store then splices through scratch, inside mem[]), DS_00100CA8/CA9 seeded,
    the two slots' +8 record pointers (0x10A100/0x10A180; the +0 pointers go to records whose +0x48
    is the other answer), +0x7A characters, the two +0x48 bytes and DS_00104AE8."""
    fill = le32(0x0010A040)
    p = {0x00100C20: fill * 16, 0x00100C60: fill * 16, 0x00100CA0: fill * 2 + b"\x11\x77",
         0x00104AE8: bytes([ae8])}
    for side, ch, r48 in ((0, c0, r0), (1, c1, r1)):
        so = D1A_SLOT + side * 0x94
        p[so] = le32(0x0010A200 + side * 0x80) + le32(0x55555555) + le32(0x0010A100 + side * 0x80)
        p[so + 0x7A] = bytes([ch])
        p[0x0010A100 + side * 0x80 + 0x48] = bytes([r48])
        p[0x0010A200 + side * 0x80 + 0x48] = bytes([4 if r48 == 5 else 5])
    return p


def d1a_facing_case(side, f28, cmd):
    """0x1A5D4/0x1A640: the side record's +0x28 word and the side's command word; the other
    side's are the same words with the tested bits flipped (0x4000; 0x3000)."""
    p = c3d_slot_pokes()
    recs = (C3D_REC0, C3D_REC1)
    p[recs[side] + 0x28] = le16(f28)
    p[recs[1 - side] + 0x28] = le16(f28 ^ 0x4000)
    words = [0, 0]
    words[side], words[1 - side] = cmd, cmd ^ 0x3000
    p[0x001088E0] = le16(words[0]) + le16(words[1])
    return p


D1A_END0, D1A_END1 = 0x000C8F68, 0x000C8FB8   # 0x1A8F4: the two per-character stream tables


def d1a_end_case(side, s54, ch, own_rec=None):
    """0x1A8F4: the record's +0x51 (the side), the side slot's record pointer (C3D_REC0/1, or
    `own_rec` to make it differ from the EDX record), +0x43/+0x52/+0x53/+0x54/+0x60..+0x62 and
    +0x7A; the other slot the same fields with other bytes (and the other +0x54 arm); the two
    table dwords of `ch` and of ch sign-extended, each a distinct sentinel."""
    p = c3d_slot_pokes()
    rec = (C3D_REC0, C3D_REC1)[side]
    p[rec + 0x51] = bytes([side])
    if own_rec is not None:
        p[D1A_SLOT + side * 0x94] = le32(own_rec)
    for k, s in ((0, side), (1, 1 - side)):
        so = D1A_SLOT + s * 0x94
        p[so + 0x43] = bytes([0xFF - k])
        p[so + 0x52] = bytes([0x11 + k, 0x22 + k, s54 if k == 0 else (0 if s54 == 1 else 1)])
        p[so + 0x60] = bytes([0x33 + k, 0x44 + k, 0x55 + k])
        p[so + 0x7A] = bytes([ch])
    sx = (ch ^ 0x80) - 0x80
    for i, c in enumerate(sorted({ch, sx})):
        p[D1A_END0 + c * 4] = le32(0x000E1000 + i * 0x100)
        p[D1A_END1 + c * 4] = le32(0x000E2000 + i * 0x100)
    return p


def d1a_spawn21_case(side, f28, z30, x=0x00123456, y=0x00ABCDEF):
    """0x21F30: the slot's record (C3D_REC0/1) with +0x18 x, +0x1C y, +0x28 and +0x30."""
    rec = (C3D_REC0, C3D_REC1)[side]
    p = c3d_slot_pokes()
    p[rec + 0x18] = le32(x) + le32(y)
    p[rec + 0x28] = le16(f28)
    p[rec + 0x30] = le32(z30)
    return p


D1A_CTX, D1A_PSET = 0x0010A000, 0x00108000   # 0x22B28: the context buffer and the pset pool base
D1A_SA, D1A_SB, D1A_RC = 0x0010A100, 0x0010A200, 0x0010A300   # its ctx[2], ctx[3] and ctx[5]


def d1a_freeze_case(side, idx, s53, src=0x00C0FFEE):
    """0x22B28: a free-form context (ctx[0] = 1 - side, ctx[1] = side, ctx[2]/[3]/[5] three scratch
    blocks, ctx[4] unused), the side slot's record (C3D_REC0/1, not ctx[5]) with the pset word
    +0x56 (ctx[5]'s +0x56 names another entry), the pset entries' +0x18 dwords (the word's, the low
    byte's and ctx[5]'s), and every byte the function writes seeded with its neighbours."""
    p = c3d_slot_pokes()
    rec = (C3D_REC0, C3D_REC1)[side]
    p[D1A_CTX] = le32(1 - side) + le32(side) + le32(D1A_SA) + le32(D1A_SB) + le32(0x0010A400) + le32(D1A_RC)
    p[0x001014EC] = le32(D1A_PSET)
    p[rec + 0x56] = le16(idx)
    p[D1A_RC + 0x56] = le16(5)
    p[D1A_PSET + 5 * 0x20 + 0x18] = le32(0x0BADBAD5)
    if idx & 0xFF != idx:
        p[D1A_PSET + (idx & 0xFF) * 0x20 + 0x18] = le32(0x0BADBAD1)
    p[D1A_PSET + idx * 0x20 + 0x18] = le32(src)
    p[D1A_SB + 0x14] = le32(0x11111111)
    p[D1A_SB + 0x43] = b"\xFF"
    p[D1A_SB + 0x53] = bytes([0x0A if s53 != 0x0A else 0x0B])
    p[D1A_SB + 0x57] = b"\x56\x55\x5A"
    p[D1A_SA + 0x53] = bytes([s53])
    p[D1A_RC + 0x20] = le32(0x20202020) + le32(0x3F800000) + le32(0x28282828)
    p[0x0010474C] = le16(0x7777) * 2
    p[0x0010476C] = b"\x99\x99"
    return p


def d1a_proj_case(side, x18, x3c, z30):
    """0x24754: both records' +0x18/+0x3C/+0x30 (the other record's are the ones read; the own
    record's differ), the side slot's +8 seeded, and the spawned actor block at 0x10A300 with the
    written fields and their neighbours seeded."""
    p = c3d_slot_pokes()
    ro = (C3D_REC0, C3D_REC1)[1 - side]
    rs = (C3D_REC0, C3D_REC1)[side]
    p[ro + 0x18] = le32(x18)
    p[ro + 0x3C] = le32(x3c)
    p[ro + 0x30] = le32(z30)
    p[rs + 0x18] = le32(0x00000700)
    p[rs + 0x3C] = le32(0x00000033)
    p[rs + 0x30] = le32(0x00090000)
    p[D1A_SLOT + side * 0x94 + 0x08] = le32(0x08080808)
    p[0x0010A300 + 0x14] = le32(0x14141414)
    p[0x0010A300 + 0x34] = le16(0x3434) + le16(0x3636) + le16(0x3838)
    p[0x0010A300 + 0x58] = b"\x58\x59\x5A"
    return p


D1A_HROW = 0x0010A600            # 0x29C20: the character's handle row


def d1a_handle_case(ch, side, idx):
    """0x29C20: the row pointer of `ch` (and, at the unscaled index @row reads, a row of other
    handles), DS_001078FF = side, DS_00105B34[side] = idx (the other entries hold another index),
    and the row's handles: [0], [1], [idx] and [idx sign-extended], all distinct."""
    p = {0x000A8A98 + ch * 4: le32(D1A_HROW), 0x001078FF: bytes([side])}
    if ch:
        p[0x000A8A98 + ch] = le32(D1A_HROW + 0x400)
    other = 0 if idx else 1
    for s in sorted({0, 1, side, (side ^ 0x80) - 0x80}):
        p[0x00105B34 + s] = bytes([idx if s == side else other])
    for i in sorted({0, 1, idx, (idx ^ 0x80) - 0x80}):
        p[D1A_HROW + i * 4] = le32(0x11110000 + (i & 0xFFFF))
        p[D1A_HROW + 0x400 + i * 4] = le32(0x22220000 + (i & 0xFFFF))
    return p


D1A_SNAP, D1A_SNAP2 = 0x0010A000, 0x0010A100   # 0x33ACC: the two destinations
D1A_SREC = (0x0010A200, 0x0010A280)            # ... and the two slots' records


def d1a_snap_case():
    """0x33ACC: both slots' 0x94 bytes (the record pointer, then slot-distinct bytes) and both
    records' 0x68 bytes distinct, the two destinations filled with 0xEE through four bytes past
    their ends (0x98 and 0x6C bytes), and four distinct bytes after each source."""
    p = {}
    for side in (0, 1):
        slot = bytes((0x40 + side * 0x80 + i) & 0xFF for i in range(0x94))
        slot = le32(D1A_SREC[side]) + slot[4:]
        so = D1A_SLOT + side * 0x94
        for off in range(0, 0x94, 0x40):
            p[so + off] = slot[off:off + 0x40]
        rec = bytes((0x10 + side * 0x60 + i * 3) & 0xFF for i in range(0x6C))
        p[D1A_SREC[side]] = rec[:0x40]
        p[D1A_SREC[side] + 0x40] = rec[0x40:]
    p[D1A_SLOT + 2 * 0x94] = b"\xA1\xA2\xA3\xA4"
    p[D1A_SNAP] = b"\xEE" * 0x40
    p[D1A_SNAP + 0x40] = b"\xEE" * 0x40
    p[D1A_SNAP + 0x80] = b"\xEE" * 0x18
    p[D1A_SNAP2] = b"\xEE" * 0x40
    p[D1A_SNAP2 + 0x40] = b"\xEE" * 0x2C
    return p


def d1a_stun_case(side, timer, s, actors, low=0x7777, ae9=0xFF):
    """0x34038: the side slot's +0x8A/+0x8C words (the other side's timer is the opposite: 1 or
    2), DS_001078FF = s, the two DS_001077A0 actor dwords, both slots' record pointers and +0x7A
    characters (slot-distinct), DS_00104AE9 and the dwords around DS_001077A0."""
    p = c3d_slot_pokes()
    p[0x0010779C] = le32(0x9C9C9C9C) + le32(actors[0]) + le32(actors[1])
    p[0x001078FF] = bytes([s])
    p[0x00104AE9] = bytes([ae9])
    for k in (0, 1):
        so = D1A_SLOT + k * 0x94
        p[so + 0x7A] = bytes([3 + k])
        p[so + 0x8A] = le16(low) + le16(timer if k == side else (2 if timer == 1 else 1))
    return p


# 0x34038's two stubs flip DS_001078FF, so a port that keeps the side in a local (the raw re-reads
# the byte at 0x34073 and 0x340A2) calls 0x29BC8 on, or clears the actor of, the wrong side.
D1A_DEAD = E.Call(0x2B150, ("eax",), writes=((None, 0x001078FF, b"\x01"),))
D1A_PAL = E.Call(0x29BC8, ("eax", "ebx", "edx"), clobbers=("ebx", "edx"),
                 writes=((None, 0x001078FF, b"\x00"),))


def d1a_ground_case(side, ch, thr, y, r36, r38=0):
    """0x34DDC: the side slot's +0x7A and +0x30, its record's +0x36 word (and +0x38, the high
    half of @w36's dword), the 0xBD870 threshold word of `ch` (of ch sign-extended: the opposite
    order against y); the other slot holds the opposite verdict."""
    p = c3d_slot_pokes()
    so, oo = D1A_SLOT + side * 0x94, D1A_SLOT + (1 - side) * 0x94
    rs, ro = (C3D_REC0, C3D_REC1)[side], (C3D_REC0, C3D_REC1)[1 - side]
    verdict = not ((((thr ^ 0x8000) - 0x8000) > ((y ^ 0x80000000) - 0x80000000)) and (r36 & 0x8000))
    p[so + 0x7A] = bytes([ch])
    p[so + 0x30] = le32(y)
    p[rs + 0x36] = le16(r36) + le16(r38)
    p[0x000BD870 + ch * 2] = le16(thr)
    sx = (ch ^ 0x80) - 0x80
    if sx != ch:
        p[0x000BD870 + sx * 2] = le16(0x8000 if verdict is False else 0x7FFF)
    p[oo + 0x7A] = bytes([ch])
    p[oo + 0x30] = le32(0x80000000 if verdict else 0x7FFFFFFF)
    p[ro + 0x36] = le16(0x8000 if verdict else 0) + le16(0)
    return p


def d1a_cb14_case(side, hook_s, hook_o, busy, ch=2, idx=1):
    """0x35050: both slots' +0x10/+0x14/+0x18 dwords (the +0x14 hook, or 0), their records' +0x51
    side bytes and +0x7A characters, DS_0009AF3D (non-zero makes 0x29D04 return 0) and the palette
    handle row 0x29D04 reads for the side."""
    p = c3d_slot_pokes()
    for k, hook in ((side, hook_s), (1 - side, hook_o)):
        so = D1A_SLOT + k * 0x94
        p[so + 0x10] = le32(0x10101010 + k) + le32(hook) + le32(0x18181818 + k)
        p[so + 0x7A] = bytes([ch + k])
        p[(C3D_REC0, C3D_REC1)[k] + 0x51] = bytes([k])
        p[0x000A8A98 + (ch + k) * 4] = le32(D1A_HROW + k * 0x100)
        p[D1A_HROW + k * 0x100] = le32(0x33330000 + k) + le32(0x44440000 + k)
    p[0x00105B34] = bytes([idx, 1 - idx])
    p[0x0009AF3D] = bytes([busy])
    return p


# 0x349C8's consume stub zeroes both command words: the raw loaded DX before the call (0x34A93) and
# masks that copy after it (0x34AA4), so a port that re-reads the word after 0x3BDDC sees 0.
D1A_CONSUME = E.Call(0x3BDDC, ("eax",), clobbers=("ebp",), writes=((None, 0x001088E0, b"\x00" * 4),))


def d1a_default_case(side, s42, cmd=0, s53=0, rec51=None, slot=True, other=True, o40=0x08200000,
                     ch=2):
    """0x349C8: the DS_001077A8 slot-pointer pair (0 = no slot), the side slot's record pointer,
    +0x42, +0x43 (0xB5: bit 6 clear, the rest mixed), +0x52..+0x54 and +0x7A, the record's +0x51
    (default the side; the table entry a 0x80 names is poked too), the other slot's +0x40 dword and
    record pointer, both command words (the other's is the complement) and the 0xC8978 stream of
    `ch`."""
    so, oo = D1A_SLOT + side * 0x94, D1A_SLOT + (1 - side) * 0x94
    rs, ro = (C3D_REC0, C3D_REC1)[side], (C3D_REC0, C3D_REC1)[1 - side]
    ptrs = [0, 0]
    ptrs[side] = so if slot else 0
    ptrs[1 - side] = oo if other else 0
    words = [0, 0]
    words[side], words[1 - side] = cmd, cmd ^ 0xFFFF
    r51 = side if rec51 is None else rec51
    p = {0x001077A8: le32(ptrs[0]) + le32(ptrs[1]), so: le32(rs), oo: le32(ro),
         so + 0x42: bytes([s42, 0xB5]), so + 0x52: bytes([0x11, s53, 0x33]), so + 0x7A: bytes([ch]),
         rs + 0x51: bytes([r51]), oo + 0x40: le32(o40), 0x001088E0: le16(words[0]) + le16(words[1]),
         0x000C8978 + ch * 4: le32(0x000E3000 + ch)}
    if r51 > 1:
        p[0x001077A8 + (r51 ^ 1) * 4] = le32(oo)
    return p


D1A_FSLOT, D1A_FREC = 0x0010A000, 0x0010A100   # 0x35C1C/0x35D20: a scratch slot and its record
D1A_SEEK_A, D1A_SEEK_B = 0x0010A300, 0x0010A500   # 0x35C1C: the two frame-word tables of the char


def d1a_frame_case(ch, mx, frame, step, s43, x29=0x51, x28=0x52):
    """0x35C1C: the slot's +0x43/+0x7A, the record's +0x28/+0x29 (bits 4 and 8 clear) with the
    words around them, +0x4F..+0x52 (the dword the raw shifts) and +0x58, the 0xBDA3E word of `ch`
    (max in its low byte; the byte the unscaled index reads is max ^ 0x55) and the two frame-word
    tables: A[i] = 0x1100 + i, B[i] = 0x2200 + i for the 16 frames around 0 (i = -8..7)."""
    p = {D1A_FSLOT + 0x43: bytes([s43]), D1A_FSLOT + 0x7A: bytes([ch]),
         D1A_FREC + 0x26: le16(0x2626) + bytes([x28, x29]) + le16(0x2A2A),
         D1A_FREC + 0x4F: bytes([0x4F, 0x50, 0x51, frame, 0x53]), D1A_FREC + 0x58: bytes([step]),
         0x000BDA3E + ch * 2: bytes([mx, 0xEE]),
         0x000C8A68 + ch * 4: le32(D1A_SEEK_A), 0x000C8AE0 + ch * 4: le32(D1A_SEEK_B),
         D1A_SEEK_A - 16: b"".join(le16(0x1100 + (i & 0xFF)) for i in range(-8, 8)),
         D1A_SEEK_B - 16: b"".join(le16(0x2200 + (i & 0xFF)) for i in range(-8, 8))}
    if ch:
        p[0x000BDA3E + ch] = bytes([mx ^ 0x55])
    return p


def d1a_land_case(ch, s43, s53, s54, fe=0x77):
    """0x35D20: the slot's +0x43, +0x52..+0x54 and +0x7A, DS_001078FE with its neighbours, and the
    two stream tables' dwords of `ch`."""
    return {D1A_FSLOT + 0x42: bytes([0x42, s43, 0x44]), D1A_FSLOT + 0x51: bytes([0x51, 0x11, s53, s54, 0x55]),
            D1A_FSLOT + 0x7A: bytes([ch]), 0x001078FD: bytes([0xFD, fe, 0xFF]),
            0x000C8A90 + ch * 4: le32(0x000E4000 + ch), 0x000C8B08 + ch * 4: le32(0x000E5000 + ch)}


D1A_TSLOT = 0x0010A200           # 0x35D7C: what the DS_001077A8 entry points at (not the slot)


def d1a_arm_case(side, table):
    """0x35D7C: both slots' +0x52..+0x55 seeded slot-distinct, the DS_001077A8 pointer pair
    (`table`: 0, or D1A_TSLOT, a scratch block seeded the same way) and that block's bytes."""
    p = {0x001077A8: le32(table if side == 0 else 0x0010A280) + le32(table if side == 1 else 0x0010A280),
         D1A_TSLOT + 0x52: bytes([0x92, 0x93, 0x94, 0x95]), 0x0010A280 + 0x52: bytes([0xA2, 0xA3, 0xA4, 0xA5])}
    for k in (0, 1):
        p[D1A_SLOT + k * 0x94 + 0x52] = bytes([0x52 + k * 0x10, 0x53 + k * 0x10, 0x54 + k * 0x10, 0x55 + k * 0x10])
    return p


D1A_SPECS = [
    # 0x14590 fighter_14590: EAX = side. 0x33950 runs real (allowed; it fills the stack ctx). AL only:
    # `mov al,1` 0x145BE / `xor al,al` 0x145C5, and all six callers test AL (0x14669, 0x14707,
    # 0x14872, 0x149D5, 0x14A73, 0x14BEC: `test al,al`).
    Spec("fighter_14590", 0x14590, [
        Case("g0", {"eax": 0}, d1a_gate_case(0, 0x00039CC8, 0x00, 1, 0)),
        Case("g1", {"eax": 1}, d1a_gate_case(1, 0x12349CC8, 0x11, 0x7FFF, 0)),
        Case("g2", {"eax": 0}, d1a_gate_case(0, 0x12349CC8, 0x10, 5, 5, pose_s=0x00039CC8, st_s=0x11)),
        Case("g3", {"eax": 0}, d1a_gate_case(0, 0x00039CC8, 0x00, 0, 5)),
        Case("g4", {"eax": 0}, d1a_gate_case(0, 0x00039CC8, 0x00, 0x8000, 0)),
        Case("g5", {"eax": 1}, d1a_gate_case(1, 0x00039CC8, 0x11, 0xFFFF, 1)),
        Case("g6", {"eax": 1}, d1a_gate_case(1, 0x00039CC8, 0x00, 1, 0xFFFF)),
        Case("g7", {"eax": 1}, d1a_gate_case(1, 0x00039CC8, 0x00, 0, 0, nxt=5)),
    ], allow_calls=(0x33950,), eax_mask=0xFF,
       mutants=("@slot", "@word", "@sign", "@pose", "@and", "@sw", "@wide")),
    # 0x186C4 fighter_slot_latch_both: `xor eax,eax; call 0x186d0; mov eax,1` falling into 0x186D0,
    # so 0x186D0 cannot be a stub (the second pass is not a call): it runs real on both sides and its
    # blocks are this row's (the scan follows the fall-through). 0x18540/0x18350 are its stubs.
    # Mask 0: the two callers return at once (0x3582E..0x35835, 0x385AA..0x385AE) and no call site
    # of theirs (0x35658, 0x384F8) reads EAX before writing it (record §1).
    Spec("fighter_slot_latch_both", 0x186C4, [
        Case("b0", {}, d1a_latch_case((True, True), (0x80, 0x00), (False, False))),
        Case("b1", {}, d1a_latch_case((False, False), (0x00, 0x80), (False, False))),
        Case("b2", {}, d1a_latch_case((False, True), (0x80, 0x80), (True, False))),
        Case("b3", {}, d1a_latch_case((True, False), (0x00, 0x00), (False, True))),
    ], allow_calls=(0x186D0,), calls=(C2_18540, C2_18350), eax_mask=0,
       mutants=("@one0", "@one1", "@order", "@twice")),
    # 0x18950 fighter_connect_query: EAX = param_1, EDX = param_2 (slot indices). AL only: `mov al,1`
    # 0x189C5/0x189DC, `xor al,al` 0x189E2; the callers read AL (0x19658 `mov bl,al`, 0x19696 and
    # 0x196A7 `test al,al`).
    Spec("fighter_connect_query", 0x18950, [
        Case("c0", {"eax": 0, "edx": 1}, d1a_conn_case(0, 1, 2, 3, 1, 2, True)),
        Case("c1", {"eax": 0, "edx": 1}, d1a_conn_case(0, 1, 2, 3, 1, 2, False)),
        Case("c2", {"eax": 0, "edx": 1}, d1a_conn_case(0, 1, 4, 0x20, 3, 0, True)),
        Case("c3", {"eax": 0, "edx": 1}, d1a_conn_case(0, 1, 4, 0x1F, 3, 0, True)),
        Case("c4", {"eax": 1, "edx": 0}, d1a_conn_case(1, 0, 6, 0x3F, 0, 6, False)),
        Case("c5", {"eax": 1, "edx": 0}, d1a_conn_case(1, 0, 0x80, 5, 0, 6, True)),
        Case("c6", {"eax": 0, "edx": 1}, d1a_conn_case(0, 1, 1, 7, 0x80, 2, True)),
        Case("c7", {"eax": 0, "edx": 1}, d1a_conn_case(0, 1, 9, 0x40, 2, 1, True)),
        Case("c8", {"eax": 1, "edx": 0}, d1a_conn_case(1, 0, 3, 0x21, 5, 4, False)),
    ], eax_mask=0xFF, mutants=("@swap", "@state", "@hi", "@bound", "@ssext", "@csext")),
    # 0x19020 fighter_19020: EAX = side. The hook is the registered 0x3E484 (run real on both sides,
    # with 0x33950/0x18BD4), whose 0x18C14 call is the stub whose EAX comes back; 0x19048 tests the
    # whole EAX. Mask 0: the one caller overwrites EAX (0x195CC `mov eax,[esp+8]`, on both arms).
    Spec("fighter_19020", 0x19020, [
        Case("h0", {"eax": 0}, d1a_hook_case(0, 0, D1A_HOOK)),
        Case("h1", {"eax": 0}, d1a_hook_case(0, D1A_HOOK, 0), {0x18C14: 0}),
        Case("h2", {"eax": 1}, d1a_hook_case(1, D1A_HOOK, 0), {0x18C14: 1}),
        Case("h3", {"eax": 0}, d1a_hook_case(0, D1A_HOOK, 0), {0x18C14: 0x100}),
        Case("h4", {"eax": 1}, d1a_hook_case(1, D1A_HOOK, D1A_HOOK), {0x18C14: 0}),
    ], allow_calls=(D1A_HOOK, 0x33950, 0x18BD4), calls=(CHECKS,), eax_mask=0,
       mutants=("@al", "@inv", "@side", "@idx", "@byte", "@arg")),
    # 0x19068 fighter_pass_b: AL = arg. 0x3C570 and 0x1922C (with its 0x33950) run real; 0x3CF38 is
    # a stub whose EAX the raw overwrites (0x190F6 `mov eax,[edx+0x100b57]`). Mask 0: every call site
    # writes EAX before reading it (record §1).
    Spec("fighter_pass_b", 0x19068, [
        Case("p0", {"eax": 1}, d1a_passb_case((1, 1), (1, 1), (5, 5), (7, 7), (3, 3), s52=(0x13, 0))),
        Case("p1", {"eax": 1}, d1a_passb_case((1, 1), (1, 1), (5, 5), (7, 7), (3, 3), s52=(0, 0x13))),
        Case("p2", {"eax": 0}, d1a_passb_case((1, 1), (1, 1), (5, 5), (7, 7), (3, 3), flags=0x20)),
        Case("p3", {"eax": 0}, d1a_passb_case((2, 1), (1, 1), (0, 5), (7, 7), (9, 3), flags=0x10),
             {0x3CF38: 1}),
        Case("p4", {"eax": 1}, d1a_passb_case((0xFF, 4), (0xFF, 4), (0, 1), (7, 0xFB), (9, 1),
                                              flags=0x20)),
        Case("p5", {"eax": 0x100}, d1a_passb_case((3, 3), (3, 3), (1, 1), (7, 9), (0x81, 0),
                                                  x24=(0x80000000, 0x3F800000))),
        Case("p6", {"eax": 1}, d1a_passb_case((3, 3), (3, 3), (0, 0x80), (7, 9), (1, 2))),
        Case("p7", {"eax": 1}, d1a_passb_case((3, 0), (3, 0), (2, 0x7F), (7, 9), (0x7F, 0))),
        Case("p8", {"eax": 1}, d1a_passb_case((3, 5), (5, 3), (4, 0), (0x10, 9), (9, 9))),
    ], allow_calls=(0x3C570, 0x1922C, 0x33950), calls=(E.Call(0x3CF38, ("eax",), mode="stub"),),
       eax_mask=0,
       mutants=("@s52a", "@s52b", "@al", "@flag", "@ff", "@b58", "@sign", "@s56", "@order", "@call",
                "@clr", "@b5a", "@one", "@mag", "@seed", "@x20", "@a", "@e")),
    # 0x19164 fighter_19164: EAX = side. x87: `fld 5.0; fcomp [rec+0x20]; jbe` 0x1917C..0x19188,
    # 0x61A4C (frndint under RC = chop, runtime, run real) then `fistp`, the low byte stored.
    # n0..n5 leave the domain a C cast shares: NaN at +0x20 is kept by the unordered `jbe` and, like
    # a truncation outside the dword, stored as the integer indefinite's low byte 0 (record §3: the
    # port's correction). Mask 0: the one caller overwrites EAX (0x19481 `mov eax,[esp+8]`).
    Spec("fighter_19164", 0x19164, [
        Case("s0", {"eax": 0}, d1a_seed_case(0, d1a_f32(3.0), d1a_f32(0.0))),
        Case("s1", {"eax": 0}, d1a_seed_case(0, d1a_f32(5.0), d1a_f32(16.99))),
        Case("s2", {"eax": 1}, d1a_seed_case(1, d1a_f32(5.9), d1a_f32(17.0))),
        Case("s3", {"eax": 0}, d1a_seed_case(0, d1a_f32(200.7), d1a_f32(-3.0))),
        Case("s4", {"eax": 1}, d1a_seed_case(1, d1a_f32(300.0), d1a_f32(0.5))),
        Case("s5", {"eax": 0}, d1a_seed_case(0, 0x80000000, d1a_f32(1.0))),
        Case("s6", {"eax": 1}, d1a_seed_case(1, d1a_f32(4.999), d1a_f32(16.5))),
        Case("s7", {"eax": 0}, d1a_seed_case(0, d1a_f32(17.5), 0x80000000)),
        Case("n0", {"eax": 0}, d1a_seed_case(0, 0x7FC00000, d1a_f32(1.0))),
        Case("n1", {"eax": 1}, d1a_seed_case(1, d1a_f32(3e9), d1a_f32(16.0))),
        Case("n2", {"eax": 0}, d1a_seed_case(0, d1a_f32(6.0), 0x7FC00000)),
        Case("n3", {"eax": 0}, d1a_seed_case(0, 0x7F800000, d1a_f32(1.0))),
        Case("n4", {"eax": 1}, d1a_seed_case(1, d1a_f32(-3e9), d1a_f32(1.0))),
        Case("n5", {"eax": 0}, d1a_seed_case(0, d1a_f32(2147483520.0), d1a_f32(1.0))),
    ], allow_calls=(0x61A4C,), eax_mask=0,
       mutants=("@rec", "@floor", "@round", "@b5a", "@lo", "@hi", "@x24", "@b58", "@src", "@nan",
                "@ovf")),
    # 0x19820 fighter_19820: no argument. 0x249C0 (the list splice, a leaf) runs real. Mask 0: a
    # void initialiser; its one caller writes AX at once (0x27F3F `mov ah,0x1e`, 0x27F4D `mov al,
    # [0x10780a]`) and never reads the upper half (record §1 names the untraced returns above).
    Spec("fighter_19820", 0x19820, [
        Case("k0", {}, d1a_list_case(5, 5, 7, 5)),
        Case("k1", {}, d1a_list_case(6, 5, 0, 0)),
        Case("k2", {}, d1a_list_case(6, 4, 6, 5, ae8=0xFB)),
        Case("k3", {}, d1a_list_case(6, 0x85, 5, 5)),
        Case("k4", {}, d1a_list_case(6, 5, 6, 5)),
    ], allow_calls=(0x249C0,), eax_mask=0,
       mutants=("@s28", "@cnt", "@head", "@ca8", "@r48", "@side1", "@c6", "@both", "@zero", "@or")),
    # 0x1A5D4 ai_facing_cmd: EAX = side. The whole EAX is the result (0x2000, 0x1000 or 0) and all
    # six callers test it whole (`test eax,eax` at 0x234C2, 0x35A32, 0x35AB4, 0x4681C, 0x4686C,
    # 0x4874B).
    Spec("ai_facing_cmd", 0x1A5D4, [
        Case("f0", {"eax": 0}, d1a_facing_case(0, 0x0000, 0x2000)),
        Case("f1", {"eax": 0}, d1a_facing_case(0, 0xBFFF, 0xDFFF)),
        Case("f2", {"eax": 1}, d1a_facing_case(1, 0x4000, 0x1000)),
        Case("f3", {"eax": 1}, d1a_facing_case(1, 0xFFFF, 0xEFFF)),
        Case("f4", {"eax": 0}, d1a_facing_case(0, 0x8000, 0xFFFF)),
        Case("f5", {"eax": 1}, d1a_facing_case(1, 0x4000, 0xFFFF)),
    ], eax_mask=0xFFFFFFFF, mutants=("@side", "@cmd", "@swap", "@bit", "@ret")),
    # 0x1A640 fighter_1a640: the mirror (0x1000 with the bit clear, 0x2000 with it set); the five
    # callers test the whole EAX (0x1AA98, 0x35A51, 0x364D7, 0x365A3, 0x4875E).
    Spec("fighter_1a640", 0x1A640, [
        Case("f0", {"eax": 0}, d1a_facing_case(0, 0x0000, 0x1000)),
        Case("f1", {"eax": 0}, d1a_facing_case(0, 0xBFFF, 0xEFFF)),
        Case("f2", {"eax": 1}, d1a_facing_case(1, 0x4000, 0x2000)),
        Case("f3", {"eax": 1}, d1a_facing_case(1, 0xFFFF, 0xDFFF)),
        Case("f4", {"eax": 0}, d1a_facing_case(0, 0x8000, 0xFFFF)),
        Case("f5", {"eax": 1}, d1a_facing_case(1, 0x4000, 0xFFFF)),
    ], eax_mask=0xFFFFFFFF, mutants=("@side", "@cmd", "@swap", "@bit", "@ret")),
    # 0x1A8F4 fighter_block_end: EDX = rec (EAX is the ctx buffer at once, 0x1A8F7). 0x33A68 runs
    # real; 0x2BC30 is the ANIM_BEGIN stub (3.0 pushed). Mask 0: the caller returns at once
    # (0x1AB09..0x1AB0F), its own caller too (0x34C7A..0x34C7F), and the dispatcher's one call site
    # overwrites EAX (0x357FC; record §1).
    Spec("fighter_block_end", 0x1A8F4, [
        Case("e0", {"eax": 0x11111111, "edx": C3D_REC0}, d1a_end_case(0, 0, 2)),
        Case("e1", {"eax": 0x11111111, "edx": C3D_REC1}, d1a_end_case(1, 1, 3)),
        Case("e2", {"eax": 0x11111111, "edx": C3D_REC0}, d1a_end_case(0, 2, 0)),
        Case("e3", {"eax": 0x11111111, "edx": C3D_REC1}, d1a_end_case(1, 0xFF, 6)),
        Case("e4", {"eax": 0x11111111, "edx": C3D_REC0}, d1a_end_case(0, 1, 0x80, own_rec=0x0010A300)),
    ], allow_calls=(0x33A68,), calls=(ANIM_BEGIN,), eax_mask=0,
       mutants=("@slot", "@s54", "@tab", "@csext", "@rec", "@frame", "@and", "@s53", "@s62",
                "@s60", "@order", "@s52")),
    # 0x1CEBC fighter_spawn_audio_gate: no argument. AL only (`mov al,1` 0x1CECE, `xor al,al`
    # 0x1CED1); the one caller tests AL (0x33E4D `test al,al`).
    Spec("fighter_spawn_audio_gate", 0x1CEBC, [
        Case("a0", {"eax": 0x12345600}, {0x001028C8: le32(0), 0x001028DB: b"\x00\x77"}),
        Case("a1", {"eax": 0x12345600}, {0x001028C8: le32(0x1234), 0x001028DB: b"\x00\x77"}),
        Case("a2", {"eax": 0x12345600}, {0x001028C8: le32(0x1234), 0x001028DB: b"\x01\x00"}),
        Case("a3", {"eax": 0x12345600}, {0x001028C8: le32(0x00010000), 0x001028DB: b"\x00\x00"}),
        Case("a4", {"eax": 0x12345600}, {0x001028C8: le32(0), 0x001028DB: b"\x80\x00"}),
    ], eax_mask=0xFF, mutants=("@handle", "@pause", "@w", "@pw")),
    # 0x21F30 fighter_21f30: EAX = slot. Four stubs: the voice 0x6C, the 0xBB308 spawn (x, +0x30
    # SAR 16, y, a5 = 0x4000 with +0x28 bit 14), 0x13244 and the voice 0x72. Mask 0: each of the
    # three callers overwrites EAX at once (0x214DE, 0x21DC3, 0x220AB `mov eax,[..]`).
    Spec("fighter_21f30", 0x21F30, [
        Case("v0", {"eax": D1A_SLOT}, d1a_spawn21_case(0, 0x4000, 0x00078000), {0x2C3FC: 0, 0x2AE14: 0}),
        Case("v1", {"eax": D1A_SLOT}, d1a_spawn21_case(0, 0xBFFF, 0xFFF80000),
             {0x2C3FC: 1, 0x2AE14: 0x0010A300}),
        Case("v2", {"eax": D1A_SLOT + 0x94}, d1a_spawn21_case(1, 0xFFFF, 0x7FFF0000, x=0xFFFFFF00, y=5),
             {0x2C3FC: 1, 0x2AE14: 0x0010A300}),
    ], calls=(VOICE, SPAWN, P6_13244), eax_mask=0,
       mutants=("@v1", "@voice", "@a5", "@xy", "@shift", "@no13244", "@order")),
    # 0x22B28 fighter_22b28: EAX = the six-dword context (ECX keeps it; every field is re-read
    # from it after each call). 0x13C70, 0x3C16C, 0x3C148 and 0x2C3FC are stubs. Mask 0: both
    # callers return (0x22D8A, 0x2365A) and their one call site each overwrites EAX (0x22E64,
    # 0x3B65E; record §1).
    Spec("fighter_22b28", 0x22B28, [
        Case("z0", {"eax": D1A_CTX}, d1a_freeze_case(0, 3, 0x0A)),
        Case("z1", {"eax": D1A_CTX}, d1a_freeze_case(1, 0x0102, 0x0B)),
        Case("z2", {"eax": D1A_CTX}, d1a_freeze_case(1, 0, 0x0A, src=0x12345678), {0x2C3FC: 0}),
        Case("z3", {"eax": D1A_CTX}, d1a_freeze_case(0, 7, 0x00)),
    ], calls=(P6_13C70, CLEAR36, CLEAR34, VOICE), eax_mask=0,
       mutants=("@rec", "@idxb", "@one", "@cb", "@tickb", "@order", "@x24", "@s58", "@s43",
                "@slot2", "@six", "@voice")),
    # 0x24754 fighter_24754: EAX = side. 0x33950 runs real; 0x1A570 is a stub whose AL the raw
    # tests (0x2476C `test al,al`), 0x2AE14 a stub whose EAX is the new actor (stored at slot+8 and
    # written through), 0x2C3FC the voice. Mask 0: the one caller overwrites EAX (0x24917 `lea eax,
    # [esi*8]`).
    Spec("fighter_24754", 0x24754, [
        Case("j0", {"eax": 0}, d1a_proj_case(0, 0x00012000, 0x00000400, 0x00070000),
             {0x1A570: 1, 0x2AE14: 0x0010A300}),
        Case("j1", {"eax": 1}, d1a_proj_case(1, 0x00012000, 0x00000400, 0xFFF90000),
             {0x1A570: 0, 0x2AE14: 0x0010A300}),
        Case("j2", {"eax": 0}, d1a_proj_case(0, 0xFFFF0000, 0x00000100, 0x00030000),
             {0x1A570: 0x100, 0x2AE14: 0x0010A300}),
        Case("j3", {"eax": 1}, d1a_proj_case(1, 0x00000010, 0xFFFFFF00, 0x00010000),
             {0x1A570: 0xFFFFFF01, 0x2AE14: 0x0010A300}),
    ], allow_calls=(0x33950,), calls=(BIT15, SPAWN, VOICE), eax_mask=0,
       # A dword store of the speed at +0x34 is an equivalent mutation: the +0x36 word store at
       # 0x247EA follows and rewrites its upper half.
       mutants=("@al", "@rec", "@x", "@speed", "@shift", "@layer", "@slot8", "@e14", "@e59",
                "@e36", "@voice")),
    # 0x29C20 fighter_29c20: EAX = char, DL = flag. The whole EAX is the handle and the one caller
    # keeps it (0x346BA `mov ebx,eax`).
    Spec("fighter_29c20", 0x29C20, [
        Case("t0", {"eax": 0, "edx": 1}, d1a_handle_case(0, 0, 0)),
        Case("t1", {"eax": 3, "edx": 1}, d1a_handle_case(3, 1, 1)),
        Case("t2", {"eax": 3, "edx": 0}, d1a_handle_case(3, 1, 0)),
        Case("t3", {"eax": 2, "edx": 0}, d1a_handle_case(2, 0, 1)),
        Case("t4", {"eax": 2, "edx": 0x100}, d1a_handle_case(2, 0, 2)),
        Case("t5", {"eax": 1, "edx": 0xFFFFFF01}, d1a_handle_case(1, 1, 0x80)),
        Case("t6", {"eax": 6, "edx": 1}, d1a_handle_case(6, 0x80, 1)),
        Case("t7", {"eax": 5, "edx": 0}, d1a_handle_case(5, 1, 0x80)),
    ], eax_mask=0xFFFFFFFF, mutants=("@dl", "@inv", "@side", "@isext", "@row", "@ssext")),
    # 0x33ACC fighter_33acc: EAX = side, EDX = the slot copy, EBX = the record copy (`rep movsd`
    # 0x25 and 0x1A dwords). Mask 0: the three callers overwrite EAX (0x22D32.., 0x2360F.., 0x27277).
    Spec("fighter_33acc", 0x33ACC, [
        Case("m0", {"eax": 0, "edx": D1A_SNAP, "ebx": D1A_SNAP2}, d1a_snap_case()),
        Case("m1", {"eax": 1, "edx": D1A_SNAP, "ebx": D1A_SNAP2}, d1a_snap_case()),
    ], eax_mask=0, mutants=("@len1", "@len2", "@over", "@swap", "@side", "@rec", "@over2")),
    # 0x34038 fighter_34038: EAX = side (the timer's slot). 0x2B150 and 0x29BC8 are stubs that
    # each flip DS_001078FF (D1A_DEAD, D1A_PAL), which the raw re-reads after each call. Mask 0:
    # three callers overwrite EAX at once (0x357F3, 0x3855B, 0x3A093 `mov eax,r`); the fourth
    # returns (0x370E7..0x370EC) into the dispatcher, whose one call site overwrites it (0x357FC).
    Spec("fighter_34038", 0x34038, [
        Case("u0", {"eax": 0}, d1a_stun_case(0, 1, 0, (0x0010A500, 0x0010A540))),
        Case("u1", {"eax": 0}, d1a_stun_case(0, 1, 1, (0x0010A500, 0))),
        Case("u2", {"eax": 1}, d1a_stun_case(1, 1, 1, (0x0010A500, 0x0010A540), ae9=0x01)),
        Case("u3", {"eax": 1}, d1a_stun_case(1, 2, 0, (0x0010A500, 0x0010A540))),
        Case("u4", {"eax": 0}, d1a_stun_case(0, 0, 0, (0x0010A500, 0x0010A540), low=1)),
        Case("u5", {"eax": 0}, d1a_stun_case(0, 0xFFFF, 1, (0x0010A500, 0x0010A540))),
        Case("u6", {"eax": 1}, d1a_stun_case(1, 1, 0, (0, 0x0010A540), low=0)),
    ], calls=(D1A_DEAD, D1A_PAL), eax_mask=0,
       mutants=("@side", "@cache", "@cache2", "@timer", "@nz", "@clr", "@ae9", "@order", "@rec")),
    # 0x34978 fighter_slots_reset: no argument. 0x654C7 (the runtime dword fill, ECX = 2 dwords)
    # runs real. Mask 0: a void reset; neither caller reads EAX before its next call or return
    # (0x20E47 and 0x20EEE `call 0x2c074`, which never touches EAX; record §1 names what is above).
    Spec("fighter_slots_reset", 0x34978, [
        Case("r0", {}, {0x001077A4: le32(0xA4A4A4A4) + le32(0xA8A8A8A8) + le32(0xACACACAC)
                        + le32(0x0010AF00), 0x001078F4: bytes([0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9,
                                                               0xFA, 0xFB])}),
        Case("r1", {}, {0x001077A4: le32(1) + le32(0x0010A000) + le32(0x0010A100) + le32(0x0010A200),
                        0x001078F4: bytes([1, 1, 1, 1, 1, 1, 2, 1])}),
    ], allow_calls=(0x654C7,), eax_mask=0, mutants=("@n1", "@n3", "@f6b", "@f6", "@fa", "@faw")),
    # 0x34DDC fighter_34ddc: EAX = side. AL only (`xor al,al` 0x34E16, `mov al,1` 0x34E1B); the one
    # caller tests AL (0x35293 `test al,al`).
    Spec("fighter_34ddc", 0x34DDC, [
        Case("d0", {"eax": 0}, d1a_ground_case(0, 2, 0x0100, 0x000000FF, 0x8000)),
        Case("d1", {"eax": 0}, d1a_ground_case(0, 2, 0x0100, 0x00000100, 0x8000)),
        Case("d2", {"eax": 1}, d1a_ground_case(1, 0, 0x0100, 0x00000080, 0x0000, r38=0x8000)),
        Case("d3", {"eax": 1}, d1a_ground_case(1, 5, 0x8000, 0xFFFF0000, 0xFFFF)),
        Case("d4", {"eax": 0}, d1a_ground_case(0, 6, 0x8000, 0xFFFF9000, 0xFFFF)),
        Case("d5", {"eax": 0}, d1a_ground_case(0, 0x80, 0x0100, 0x00000010, 0x8001)),
        Case("d6", {"eax": 1}, d1a_ground_case(1, 3, 0x0100, 0xFFFFFFF0, 0x8000)),
        Case("d7", {"eax": 0}, d1a_ground_case(0, 1, 0x0100, 0x00000001, 0x7FFF)),
    ], eax_mask=0xFF, mutants=("@thr", "@cmp", "@ucmp", "@r36", "@side", "@csext", "@w36")),
    # 0x34E20 fighter_34e20: EAX = the reaction. `cmp eax,0x17; jbe` on the whole EAX, AL returned;
    # the one caller tests AL (0x354A8 `test al,al`).
    Spec("fighter_34e20", 0x34E20, [
        Case("e%d" % i, {"eax": v}) for i, v in enumerate((0, 0x17, 0x18, 0xFFFFFFFF, 0x80000000, 0x117))
    ], eax_mask=0xFF, mutants=("@bound", "@signed", "@byte")),
    # 0x35050 fighter_35050: EAX = side. The callback is the registered 0x29D04 (run real; it
    # returns 0 with DS_0009AF3D set, else calls the 0x2A17C stub and returns 1). Mask 0: both
    # callers overwrite EAX at once (0x22C65, 0x39CDE `mov eax,[esp+0xc]`).
    Spec("fighter_35050", 0x35050, [
        Case("q0", {"eax": 0}, d1a_cb14_case(0, 0, 0x00029D04, 0)),
        Case("q1", {"eax": 0}, d1a_cb14_case(0, 0x00029D04, 0, 1)),
        Case("q2", {"eax": 1}, d1a_cb14_case(1, 0x00029D04, 0, 0)),
        Case("q3", {"eax": 0}, d1a_cb14_case(0, 0x00029D04, 0x00029D04, 0, idx=0)),
    ], allow_calls=(0x00029D04,), calls=(PALETTE,), eax_mask=0,
       mutants=("@side", "@arg", "@keep", "@inv", "@field")),
    # 0x349C8 fighter_state_default: EAX = side. Seven stubs; 0x365C8, 0x36638 and 0x3BDDC return
    # the AL the raw tests (0x34A43, 0x34A5A, 0x34AA7 `test al,al`). Mask 0: the one caller returns
    # at once (0x34C0F..0x34C14) and the dispatcher's one call site overwrites EAX (0x357FC).
    Spec("fighter_state_default", 0x349C8, [
        Case("w0", {"eax": 0}, d1a_default_case(0, 0x40, slot=False)),
        Case("w1", {"eax": 1}, d1a_default_case(1, 0x40)),
        Case("w2", {"eax": 0}, d1a_default_case(0, 0xA0, o40=0x7FFFFFFF)),
        Case("w3", {"eax": 0}, d1a_default_case(0, 0x80, other=False)),
        Case("w4", {"eax": 1}, d1a_default_case(1, 0x80, o40=0x08000000)),
        Case("w5", {"eax": 1}, d1a_default_case(1, 0x80, rec51=0x80, o40=0x08200000)),
        Case("w6", {"eax": 0}, d1a_default_case(0, 0x80, rec51=1, o40=0x08200000)),
        Case("w7", {"eax": 0}, d1a_default_case(0, 0x00, cmd=0x5000), {0x365C8: 1, 0x36638: 1}),
        Case("w8", {"eax": 1}, d1a_default_case(1, 0x3F, cmd=0x4500),
             {0x365C8: 0x100, 0x36638: 0x100}),
        Case("w9", {"eax": 0}, d1a_default_case(0, 0x00, cmd=0x41FF, ch=5),
             {0x365C8: 0, 0x36638: 0, 0x3BDDC: 0x100}),
        Case("w10", {"eax": 1}, d1a_default_case(1, 0x00, cmd=0x4000),
             {0x365C8: 1, 0x36638: 0, 0x3BDDC: 1}),
        Case("w11", {"eax": 0}, d1a_default_case(0, 0x00, cmd=0x1000, s53=0),
             {0x365C8: 0, 0x36638: 0x100, 0x3BDDC: 0}),
        Case("w12", {"eax": 1}, d1a_default_case(1, 0x00, cmd=0x14FF, s53=1),
             {0x365C8: 0, 0x36638: 0, 0x3BDDC: 0}),
        Case("w13", {"eax": 0}, d1a_default_case(0, 0x00, cmd=0xB0AB, s53=1),
             {0x365C8: 0, 0x36638: 0, 0x3BDDC: 0}),
        Case("w14", {"eax": 0}, d1a_default_case(0, 0x00, cmd=0x8CFF, s53=0),
             {0x365C8: 0, 0x36638: 0, 0x3BDDC: 0}),
        Case("w15", {"eax": 1}, d1a_default_case(1, 0x00, cmd=0x1300, s53=0),
             {0x365C8: 0, 0x36638: 0, 0x3BDDC: 0}),
    ], calls=(E.Call(0x37178, ("eax",), clobbers=("esi", "edi", "ebp")), P6_37D18,
              E.Call(0x365C8, ("eax", "edx", "ebx"), clobbers=("ebx", "edx")), C1_36638,
              D1A_CONSUME, ANIM_BEGIN, DIRS), eax_mask=0,
       mutants=("@b6", "@o51", "@mask", "@d18", "@s53", "@al365", "@al366", "@gate", "@al3b",
                "@reread", "@frame", "@s52", "@s54", "@s53z", "@dir")),
    # 0x35C1C fighter_state_35c1c: EAX = slot, EDX = rec. The whole EAX is the clamp flag (`mov eax,
    # ecx` 0x35CAE) and three callers test it whole (`test eax,eax` 0x34C2F, 0x35C05, 0x35D0F).
    # 0x2BCF4 (the seek) is a stub.
    Spec("fighter_state_35c1c", 0x35C1C, [
        Case("x0", {"eax": D1A_FSLOT, "edx": D1A_FREC}, d1a_frame_case(2, 6, 2, 1, 0x00)),
        Case("x1", {"eax": D1A_FSLOT, "edx": D1A_FREC}, d1a_frame_case(2, 6, 5, 1, 0x02)),
        Case("x2", {"eax": D1A_FSLOT, "edx": D1A_FREC}, d1a_frame_case(3, 6, 0, 0xFF, 0xFD)),
        Case("x3", {"eax": D1A_FSLOT, "edx": D1A_FREC}, d1a_frame_case(0, 7, 0x7F, 1, 0xFF)),
        Case("x4", {"eax": D1A_FSLOT, "edx": D1A_FREC}, d1a_frame_case(5, 0x80, 3, 0, 0x00)),
        Case("x5", {"eax": D1A_FSLOT, "edx": D1A_FREC}, d1a_frame_case(1, 6, 4, 1, 0x02, x29=0xFF, x28=0xFF)),
        Case("x6", {"eax": D1A_FSLOT, "edx": D1A_FREC}, d1a_frame_case(6, 0, 0, 0xFF, 0x00)),
        Case("x7", {"eax": D1A_FSLOT, "edx": D1A_FREC}, d1a_frame_case(4, 6, 6, 1, 0x00)),
    ], calls=(P6_2BCF4,), eax_mask=0xFFFFFFFF,
       mutants=("@max", "@step", "@neg", "@cmp", "@ucmp", "@r", "@b29", "@tab", "@idx", "@b28",
                "@order", "@seekb")),
    # 0x35D20 fighter_state_35d20: EAX = slot, EDX = rec. 0x2BC30 is the ANIM_BEGIN stub (1.0
    # pushed). Mask 0: the three callers return at once (0x34C40..0x34C45, 0x35C12..0x35C19,
    # 0x35D1C..0x35D1E; record §1 names what is above them).
    Spec("fighter_state_35d20", 0x35D20, [
        Case("y0", {"eax": D1A_FSLOT, "edx": D1A_FREC}, d1a_land_case(2, 0xFF, 0x05, 0x04)),
        Case("y1", {"eax": D1A_FSLOT, "edx": D1A_FREC}, d1a_land_case(3, 0xFD, 0x0D, 0x03)),
        Case("y2", {"eax": D1A_FSLOT, "edx": D1A_FREC}, d1a_land_case(0, 0x03, 0x04, 0x0D, fe=0x01)),
        Case("y3", {"eax": D1A_FSLOT, "edx": D1A_FREC}, d1a_land_case(6, 0x00, 0x0D, 0x04, fe=0x00)),
    ], calls=(ANIM_BEGIN,), eax_mask=0,
       mutants=("@tab", "@rec", "@frame", "@s52", "@and", "@s54", "@order", "@keep")),
    # 0x35D7C fighter_state_35d7c: EDX = side (EAX is overwritten at 0x35D80). The slot is computed
    # from the side (0x35D8E..0x35DA6), not read from DS_001077A8, and there is no null test: the
    # cases point the table entry elsewhere, or at nothing. 0x3CF38 is a stub whose AL the raw
    # tests (0x35DED). Mask 0: the caller returns at once (0x34C4D..0x34C52) and the dispatcher's
    # one call site overwrites EAX (0x357FC).
    Spec("fighter_state_35d7c", 0x35D7C, [
        Case("a0", {"eax": D1A_TSLOT, "edx": 0}, d1a_arm_case(0, D1A_TSLOT), {0x3CF38: 0}),
        Case("a1", {"eax": D1A_TSLOT, "edx": 1}, d1a_arm_case(1, D1A_TSLOT), {0x3CF38: 1}),
        Case("a2", {"eax": D1A_TSLOT, "edx": 0}, d1a_arm_case(0, 0), {0x3CF38: 0x100}),
        Case("a3", {"eax": D1A_TSLOT, "edx": 1}, d1a_arm_case(1, 0), {0x3CF38: 0xFFFFFF00}),
        Case("a4", {"eax": D1A_TSLOT, "edx": 0}, d1a_arm_case(0, D1A_SLOT), {0x3CF38: 0x80}),
    ], calls=(E.Call(0x3CF38, ("eax",), mode="stub"),), eax_mask=0,
       mutants=("@table", "@c54", "@c53", "@al", "@s53", "@order", "@arg")),
]
D1_SPECS += D1A_SPECS


# ---- D1 d1b (record 2026-10-11-reverse-d1b): game/fighter.c, second half ------------------------
D1B_SLOT, D1B_REC = 0x10A200, 0x10A300      # zero BSS of the image: a slot and its record


def d1b_36300_case(st, ch=2, w36=0x1234, r1c=0, s5c=5, w34=0, thr=None, step=None):
    """0x36300: the slot's +0x41/+0x52/+0x58/+0x5C/+0x7A with their neighbours, the record's
    +0x1C/+0x34/+0x36, and (when given) this character's 0xBDA76 word and 0xBDA84 byte. +0x7B
    holds another character (5) and +0x5D another decrement, for the index mutants."""
    p = {D1B_SLOT + 0x40: bytes([0x40, 0x41, 0x42]),
         D1B_SLOT + 0x51: bytes([0x51, 0x52, 0x53]),
         D1B_SLOT + 0x57: bytes([0x57, st, 0x59]),
         D1B_SLOT + 0x5B: bytes([0x5B, s5c, 0x77]),
         D1B_SLOT + 0x79: bytes([0x79, ch, 5]),
         D1B_REC + 0x1C: le32(r1c),
         D1B_REC + 0x30: b"\x30\x31\x32\x33" + le16(w34) + le16(w36) + b"\x38\x39\x3a\x3b"}
    if thr is not None:
        p[0x000BDA76 + ch * 2] = le16(thr)
    if step is not None:
        p[0x000BDA84 + ch] = bytes([step])
    return p


D1B_REC2, D1B_REC3 = 0x10A400, 0x10A480     # the side slot's record and the other side's


def d1b_36710_case(side, flag, ch=2, w36=0, r1c=0, oflag=3):
    """0x36710: the EAX slot (D1B_SLOT) is NOT the side's slot, and the EDX record (D1B_REC) is
    not the side slot's record (D1B_REC2), so each operand's source is told apart. The side slot
    holds the flag and the character; the EAX slot holds another flag (3: no arm) and character
    (5), the other side's slot another flag and character (6)."""
    ss, os_ = DS_SLOTS + side * 0x94, DS_SLOTS + (1 - side) * 0x94
    return {ss: le32(D1B_REC2), ss + 0x51: bytes([0x61, 0x62, 0x63]),
            ss + 0x57: bytes([0x67, flag, 0x69]), ss + 0x79: bytes([0x79, ch, 0x7B]),
            os_: le32(D1B_REC3), os_ + 0x51: bytes([0x71, 0x72, 0x73]),
            os_ + 0x57: bytes([0x77, oflag, 0x79]), os_ + 0x79: bytes([0x79, 6, 0x7B]),
            D1B_SLOT + 0x51: bytes([0x51, 0x52, 0x53]), D1B_SLOT + 0x57: bytes([0x57, 3, 0x59]),
            D1B_SLOT + 0x79: bytes([0x79, 5, 0x7B]),
            D1B_REC + 0x1C: le32(r1c), D1B_REC + 0x34: b"\x34\x35" + le16(w36) + b"\x38\x39",
            D1B_REC + 0x42: bytes([0x42, 0x43, 0x44]), D1B_REC + 0x50: bytes([0x50, side, 0x52]),
            D1B_REC2 + 0x42: bytes([0x62, 0x63, 0x64]), D1B_REC3 + 0x42: bytes([0x72, 0x73, 0x74])}


D1B_CHILD = 0x10A500                        # zero BSS: the 0x2AE14 stub's returned actor(s)
D1B_C5C = E.Call(0x38C5C, ("eax",))
D1B_HOLD = E.Call(0x2F4BC, ("eax", "edx", "ebx", "ecx"), clobbers=("ebx", "ecx", "edx"))


def d1b_380c4_case(side, ch, w28, w56):
    """0x380C4: the side slot's record pointer and +0x7A (its +0x7B holds character 5), the
    record's words +0x28 and +0x56 with their neighbours (+0x54 is the @w56off word), the other
    side's slot and record with other values, and the two stub actors' +0x58..+0x5A."""
    ss, os_ = DS_SLOTS + side * 0x94, DS_SLOTS + (1 - side) * 0x94
    return {ss: le32(D1B_REC), ss + 0x79: bytes([0x79, ch, 5]),
            os_: le32(D1B_REC2), os_ + 0x79: bytes([0x79, ch ^ 1, 3]),
            D1B_REC + 0x18: le32(0x00181818),
            D1B_REC + 0x26: b"\x26\x27" + le16(w28) + b"\x2a\x2b",
            D1B_REC + 0x54: le16(0x5455) + le16(w56) + b"\x58\x59",
            D1B_REC2 + 0x26: b"\x26\x27" + le16(w28 ^ 0x4000) + b"\x2a\x2b",
            D1B_REC2 + 0x54: le16(0x6465) + le16(w56 ^ 0x00FF) + b"\x68\x69",
            D1B_CHILD + 0x58: bytes([0x58, 0x59, 0x5A]),
            D1B_CHILD + 0x80 + 0x58: bytes([0x68, 0x69, 0x6A])}


def d1b_38d24_case(t0, t1):
    """0x38D24: C3g's string table (id 0xE5 decodes to "BBBB", 0xE4 to "XX") and the two timer
    words 0x107D18/0x107D1A between two sentinel words."""
    p = c3g_combo_str_case()
    p[0x00107D16] = le16(0x1616) + le16(t0) + le16(t1) + le16(0x1C1C)
    return p


def d1b_anim2(ch, b):
    """The third pointer 0x3AFC4 returns for (character, b): 0xA6728 + ((ch << 6) + b) * 6."""
    return 0x000A6728 + ((ch << 6) + b) * 6


def d1b_gate_slots(side, b5f, b64, ch, och):
    """The side slot's +0x5F/+0x64/+0x7A with neighbours; the other slot holds them swapped and
    another character."""
    ss, os_ = DS_SLOTS + side * 0x94, DS_SLOTS + (1 - side) * 0x94
    return {ss + 0x5E: bytes([0x5E, b5f, 0x60]), ss + 0x63: bytes([0x63, b64, 0x65]),
            ss + 0x79: bytes([0x79, ch, 0x7B]),
            os_ + 0x5E: bytes([0x5E, b64, 0x60]), os_ + 0x63: bytes([0x63, b5f, 0x65]),
            os_ + 0x79: bytes([0x79, och, 0x7B])}


def d1b_gate_words(side, cnt, hits, ocnt=0x0055):
    """0x107D22..0x107D31: the per-side counter words 0x107D24 and hit words 0x107D2C between
    sentinels. The other side's hit word is on the other side of the `< 1` test."""
    ohits = 0 if 0 < hits < 0x8000 else 1
    pair = lambda a, b: le16(a) + le16(b) if side == 0 else le16(b) + le16(a)
    return {0x00107D22: le16(0x2222) + pair(cnt, ocnt) + le16(0x2828) + le16(0x2A2A)
            + pair(hits, ohits) + le16(0x3030)}


def d1b_3962c_case(side, w5f, w64, cnt, hits, b5f=3, b64=9, ch=2, och=4):
    """0x3962C: the two selectable descriptor entries of the side's character carry the words
    w5f and w64 at +2 (their +0 words have bit 4 clear, for @wordoff); the other character's
    entries carry them with bit 4 flipped."""
    p = d1b_gate_slots(side, b5f, b64, ch, och)
    p.update(d1b_gate_words(side, cnt, hits))
    p[d1b_anim2(ch, b5f)] = le16(0xA0A0) + le16(w5f) + le16(0x4444)
    p[d1b_anim2(ch, b64)] = le16(0xA1A1) + le16(w64) + le16(0x4545)
    p[d1b_anim2(och, b5f)] = le16(0xA2A2) + le16(w5f ^ 0x10) + le16(0x4646)
    p[d1b_anim2(och, b64)] = le16(0xA3A3) + le16(w64 ^ 0x10) + le16(0x4747)
    return p


def d1b_396ac_case(side, b5f, pre, thr, hits, b64=0x11, thr64=0x7000, pre64=0x30, ch=2, och=4):
    """0x396AC: the 0x107A80[side*0x40 + b] counter (between two sentinels) and the descriptor
    entry's +4 word for the +0x5F byte, and another counter/threshold for the +0x64 byte; the
    other side's counter at the same b is 0x20 and the other character's threshold 0x7000."""
    p = d1b_gate_slots(side, b5f, b64, ch, och)
    p.update(d1b_gate_words(side, 0x0024, hits))
    for b, t, c in ((b5f, thr, pre), (b64, thr64, pre64)):
        p[0x00107A80 + side * 0x40 + b - 1] = bytes([0x70, c, 0x72])
        p[0x00107A80 + (1 - side) * 0x40 + b - 1] = bytes([0x60, 0x20, 0x62])
        if b < 0x40:
            p[d1b_anim2(ch, b)] = le16(0xA0A0) + le16(0x2222) + le16(t)
            p[d1b_anim2(och, b)] = le16(0xA2A2) + le16(0x2323) + le16(0x7000)
    return p


def d1b_399cc_case(side, s, s5d=0, slot5d=1, mode=0, ch=2):
    """0x399CC: `s` is the 0x1077A8[side] pointer (D1B_SLOT: a slot that is not the side's; the
    side's own slot; or 0, with D1B_SLOT in the other entry). Both real slots, D1B_SLOT and the
    three records carry sentinels at every byte the function or 0x367DC writes."""
    ss, os_ = DS_SLOTS + side * 0x94, DS_SLOTS + (1 - side) * 0x94
    alt = D1B_SLOT if s == 0 else 0
    if s == ss:
        slot5d = s5d
    p = {0x001077A8: le32(s) + le32(alt) if side == 0 else le32(alt) + le32(s),
         ss: le32(D1B_REC2), ss + 0x40: b"\x40\x41\x42\x43", ss + 0x52: b"\x52\x53\x54\x55",
         ss + 0x5C: bytes([0x5C, slot5d, 0x5E, 0x5F]), ss + 0x74: b"\x74\x75\x76\x77",
         ss + 0x79: bytes([0x79, 4, 0x7B]),
         os_: le32(D1B_REC3), os_ + 0x40: b"\x60\x61\x62\x63", os_ + 0x52: b"\x62\x63\x64\x65",
         os_ + 0x5C: b"\x6c\x00\x6e\x6f", os_ + 0x74: b"\x64\x65\x66\x67",
         os_ + 0x79: bytes([0x79, 6, 0x7B]),
         D1B_SLOT: le32(D1B_REC), D1B_SLOT + 0x40: b"\x50\x51\x52\x53",
         D1B_SLOT + 0x52: b"\x72\x73\x74\x75", D1B_SLOT + 0x5C: bytes([0x7C, s5d, 0x7E, 0x7F]),
         D1B_SLOT + 0x74: b"\x84\x85\x86\x87", D1B_SLOT + 0x79: bytes([0x79, ch, 0x7B]),
         D1B_REC + 0x4B: b"\x4b\x4c\x4d\x4e", D1B_REC + 0x50: bytes([0x50, side, 0x52]),
         D1B_REC2 + 0x4B: b"\x5b\x5c\x5d\x5e", D1B_REC2 + 0x50: bytes([0x50, 1 - side, 0x52]),
         D1B_REC3 + 0x4B: b"\x6b\x6c\x6d\x6e", D1B_REC3 + 0x50: bytes([0x50, side, 0x52]),
         0x00102900: le32(0x0010A580) + le32(0x0010A5C0), DS_104B00: le16(mode)}
    return p


D1B_3A2A0_CALLS = (
    E.Call(0x3B298, ("eax", "edx"), clobbers=("edx",)), VOICE,
    E.Call(0x3A0FC, ("eax",), clobbers=("ebp",)), E.Call(0x18B04, ("eax",)),
    E.Call(0x39F40, ("eax", "edx", "ebx", "ecx", "s0"), pop=4, clobbers=("ebx", "edx")),
    E.Call(0x39834, ("eax", "edx"), clobbers=("edx", "ebp")),
    E.Call(0x3B080, ("eax", "edx", "ebx", "ecx"), clobbers=("ebx", "ecx", "edx")),
    E.Call(0x3AD98, ("eax", "[edx]", "[edx+4]", "[edx+8]"), clobbers=("edx",)))
D1B_POSE = {"edx": 0x0D0D0001, "ebx": 0x0B0B0002, "ecx": 0x0C0C0003, "s0": 0x50500004}


def d1b_3a2a0_case(side, b, o54=0, ch=2, och=4, ob=9):
    """0x3A2A0: both slots' +0x18, +0x41/+0x42, +0x54, +0x5F, +0x7A and +0x8A with neighbours
    (the two sides hold different values everywhere), the 0xDE114 triple entry's bytes +2/+3 of
    the side's character and of the other's, and distinct 0xBE008 voice words."""
    ss, os_ = DS_SLOTS + side * 0x94, DS_SLOTS + (1 - side) * 0x94
    c, oc = ((ch << 6) + b) * 11, ((och << 6) + b) * 11
    return {ss + 0x16: b"\x16\x17\x18\x19\x1a\x1b\x1c\x1d", ss + 0x40: b"\x40\x41\xff\x43",
            ss + 0x53: bytes([0x53, 2 if o54 != 2 else 0, 0x55]), ss + 0x5E: bytes([0x5E, b, 0x60]),
            ss + 0x79: bytes([0x79, ch, 0x7B]), ss + 0x89: b"\x89\x8a\x8b",
            os_ + 0x16: b"\x26\x27\x28\x29\x2a\x2b\x2c\x2d", os_ + 0x40: b"\x50\x51\xef\x53",
            os_ + 0x53: bytes([0x63, o54, 0x65]), os_ + 0x5E: bytes([0x6E, ob, 0x70]),
            os_ + 0x79: bytes([0x79, och, 0x7B]), os_ + 0x89: b"\x99\x9a\x9b",
            0x000DE114 + c: b"\xd0\xd1\x22\x33\xd4", 0x000DE114 + oc: b"\xe0\xe1\x44\x55\xe4",
            0x000BE008 + ch * 2: le16(0x0111), 0x000BE008 + och * 2: le16(0x0222)}


def d1b_3b938_case(st, ch, step=None, stw_hi=0x49):
    """0x3B938: the slot (D1B_SLOT) with its +0x08 projectile (D1B_REC), +0x64 and +0x7A (its
    +0x7B holds character 5), the projectile's +0x48 state (+0x49 non-zero: the state is a
    byte) and words +0x34/+0x36, and distinct table entries for the two characters."""
    p = {D1B_SLOT + 0x04: le32(0x04040404) + le32(D1B_REC) + le32(0x0C0C0C0C),
         D1B_SLOT + 0x63: bytes([0x63, 0x64, 0x65]), D1B_SLOT + 0x79: bytes([0x79, ch, 5]),
         D1B_REC + 0x32: b"\x32\x33\x34\x35\x36\x37\x38\x39",
         D1B_REC + 0x47: bytes([0x47, st, stw_hi, 0x4A]),
         0x000BDFC8 + ch * 4: le32(0x000E0000 + ch * 0x100), 0x000BDFC8 + 5 * 4: le32(0x000E5555),
         0x000BDFFA + ch * 2: le16(0x0100 + ch), 0x000BDFFA + 5 * 2: le16(0x0155)}
    if step is not None:
        p[0x000BDFF0 + ch] = bytes([step])
    return p


D1B_1883C_X = E.Call(0x1883C, ("eax", "edx", "ebx"), clobbers=("ebx", "edx"),
                     writes=((None, DS_SLOTS + 0x2C, le32(0x00000500)),))
D1B_3B8D8 = E.Call(0x3B8D8, ("eax", "edx"), clobbers=("edx",))


def d1b_3b9d8_case(side, x, ox, s42=0x10, o43=0x01, s54=0, w34=0x1234, o4c=0x0123):
    """0x3B9D8: both slots' +0x2C x, +0x40..+0x43, +0x4A..+0x4D and +0x54, the two records'
    +0x34 words (opposite signs) and DS_00107D30 with its neighbours. The other slot's +0x43
    holds bit 0 or bit 1, never both, and the side's holds the other one. The 0x1883C stub
    writes side 0's +0x2C = 0x500 (the real one moves the side), so the x test the +0x54 == 2
    tail repeats reads a changed x."""
    ss, os_ = DS_SLOTS + side * 0x94, DS_SLOTS + (1 - side) * 0x94
    return {ss: le32(D1B_REC), os_: le32(D1B_REC2),
            ss + 0x2C: le32(x), os_ + 0x2C: le32(ox),
            ss + 0x40: bytes([0x40, 0x41, s42, o43 ^ 3]), os_ + 0x40: bytes([0x50, 0x51, 0x52, o43]),
            ss + 0x4A: le16(0x5A5B) + le16(o4c ^ 0xFFFF), os_ + 0x4A: le16(0x4A4B) + le16(o4c),
            ss + 0x53: bytes([0x53, s54, 0x55]), os_ + 0x53: bytes([0x63, 0 if s54 == 2 else 2, 0x65]),
            D1B_REC + 0x32: b"\x32\x33" + le16(w34) + b"\x36\x37",
            D1B_REC2 + 0x32: b"\x42\x43" + le16((-w34) & 0xFFFF) + b"\x46\x47",
            0x00107D2F: b"\x2f\x30\x31"}


def d1b_x_case(side, x, ox, w34=0x3434):
    """0x3C1C8: the two slots' record pointers and +0x2C, the two records' +0x34 words."""
    ss, os_ = DS_SLOTS + side * 0x94, DS_SLOTS + (1 - side) * 0x94
    return {ss: le32(D1B_REC), os_: le32(D1B_REC2), ss + 0x2C: le32(x), os_ + 0x2C: le32(ox),
            D1B_REC + 0x32: b"\x32\x33" + le16(w34) + b"\x36\x37",
            D1B_REC2 + 0x32: b"\x42\x43\x44\x45\x46\x47"}


def d1b_3f6f4_case(side, w88, ow88=0x0777):
    """0x3F6F4: the side slot's words +0x86/+0x88 (and the other slot's)."""
    ss, os_ = DS_SLOTS + side * 0x94, DS_SLOTS + (1 - side) * 0x94
    return {ss + 0x86: le16(0x0666) + le16(w88) + le16(0x8A8A),
            os_ + 0x86: le16(0x0555) + le16(ow88) + le16(0x9A9A)}


def d1b_3d0c0_case(side):
    """0x3D0C0: the side slot's +0x51..+0x55 and +0x5B..+0x5D and its record's +0x32..+0x37 and
    +0x41..+0x46, every byte a sentinel; the other slot and record the same with other bytes."""
    ss, os_ = DS_SLOTS + side * 0x94, DS_SLOTS + (1 - side) * 0x94
    return {ss: le32(D1B_REC), os_: le32(D1B_REC2),
            ss + 0x51: b"\x51\x52\x53\x54\x55", ss + 0x5B: b"\x5b\x5c\x5d",
            os_ + 0x51: b"\x61\x62\x63\x64\x65", os_ + 0x5B: b"\x6b\x6c\x6d",
            D1B_REC + 0x32: b"\x32\x33\x34\x35\x36\x37", D1B_REC + 0x41: b"\x41\x42\x43\x44\x45\x46",
            D1B_REC2 + 0x32: b"\x72\x73\x74\x75\x76\x77", D1B_REC2 + 0x41: b"\x81\x82\x83\x84\x85\x86"}


def d1b_spawn_case(w28, y30):
    """0x3F308/0x408E8: the slot (D1B_SLOT) and its record's +0x18, +0x1C, word +0x28 and dword
    +0x30; DS_00108094 with its neighbours and DS_001088C2."""
    return {D1B_SLOT: le32(D1B_REC), D1B_REC + 0x18: le32(0x00181818) + le32(0x001C1C1C),
            D1B_REC + 0x26: b"\x26\x27" + le16(w28) + b"\x2a\x2b",
            D1B_REC + 0x2C: le32(0x2C2C2C2C) + le32(y30) + le32(0x34343434),
            0x00108090: le32(0x90909090) + le32(0x94949494) + le32(0x98989898)}


D1B_3F720_CALLS = (E.Call(0x187FC, ()), E.Call(0x189FC, ("eax",)),
                   E.Call(0x3C190, ("eax", "edx"), clobbers=("edx",)))


def d1b_46698_case(side, s40, x, ox, w34):
    """0x46698: the side slot's +0x40 (bit 6 is the complement of bit 7) and +0x2C, the other
    slot's (its +0x40 holds the complement), and the two records' +0x34 words (opposite signs)."""
    ss, os_ = DS_SLOTS + side * 0x94, DS_SLOTS + (1 - side) * 0x94
    return {ss: le32(D1B_REC), os_: le32(D1B_REC2), ss + 0x2C: le32(x), os_ + 0x2C: le32(ox),
            ss + 0x3F: bytes([0x3F, s40, 0x41]), os_ + 0x3F: bytes([0x4F, s40 ^ 0xC0, 0x51]),
            D1B_REC + 0x32: b"\x32\x33" + le16(w34) + b"\x36\x37",
            D1B_REC2 + 0x32: b"\x42\x43" + le16((-w34) & 0xFFFF if w34 else 0x8000) + b"\x46\x47"}


def d1b_state_case(side, s5f, s54, s53, s52, o=(0x7F, 9, 9, 9)):
    """The slot-state predicates: the side slot's +0x52/+0x53/+0x54 and +0x5F with neighbours;
    the other slot holds `o` (+0x5F, +0x54, +0x53, +0x52)."""
    ss, os_ = DS_SLOTS + side * 0x94, DS_SLOTS + (1 - side) * 0x94
    return {ss + 0x51: bytes([0x51, s52, s53, s54, 0x55]), ss + 0x5E: bytes([0x5E, s5f, 0x60]),
            os_ + 0x51: bytes([0x61, o[3], o[2], o[1], 0x65]), os_ + 0x5E: bytes([0x6E, o[0], 0x70])}


def d1b_46898_case(side, s10, r24, s54, o=(0, 1, 9)):
    """0x46898: the side slot's +0x10 and +0x54 and its record's +0x24; the other side holds `o`."""
    ss, os_ = DS_SLOTS + side * 0x94, DS_SLOTS + (1 - side) * 0x94
    return {ss: le32(D1B_REC), os_: le32(D1B_REC2),
            ss + 0x0C: le32(0x0C0C0C0C) + le32(s10) + le32(0x14141414),
            os_ + 0x0C: le32(0x1C1C1C1C) + le32(o[0]) + le32(0x24242424),
            ss + 0x53: bytes([0x53, s54, 0x55]), os_ + 0x53: bytes([0x63, o[2], 0x65]),
            D1B_REC + 0x20: le32(0x20202020) + le32(r24) + le32(0x28282828),
            D1B_REC2 + 0x20: le32(0x30303030) + le32(o[1]) + le32(0x38383838)}


def d1b_4fb20_case(x1, y1, x2, y2, w):
    """0x4FB20: the two latched points 0xD3388/0xD338C and 0xD3390/0xD3394, the four output
    dwords 0xD3398..0xD33A7 (sentinels) and the width word 0xD33A8 with its neighbour."""
    return {0x000D3388: le32(x1) + le32(y1) + le32(x2) + le32(y2),
            0x000D3398: le32(0x98989898) + le32(0x9C9C9C9C) + le32(0xA0A0A0A0) + le32(0xA4A4A4A4),
            0x000D33A8: le16(w) + le16(0xAAAA)}


D1B_AI_P1, D1B_AI_P2 = 0x10A600, 0x10AB00   # zero BSS: the two per-pair tables 0x46BBC indexes


def d1b_46bbc_case(side, sc, oc, ident, xc=5):
    """0x46BBC: the side's and the other's (1 - side) character bytes 0x10782A, another character
    (`xc`) at the slot `side ^ 1` when that is a different slot (side 2), the two pair-table
    entries pointing at scratch tables whose `ident` entries hold sentinels, and the side's AI
    block fields +0x14, +0x2C, +0x34 (and the other block's) with neighbours."""
    idx = sc * 10 + oc
    b, ob = 0x001081F0 + side * 0x40, 0x001081F0 + (1 - side) * 0x40
    p = {0x0010782A + side * 0x94: bytes([sc]), 0x0010782A + (1 - side) * 0x94: bytes([oc]),
         0x00095CA8 + idx * 4: le32(D1B_AI_P1), 0x00095E38 + idx * 4: le32(D1B_AI_P2),
         D1B_AI_P1 + ident * 4: le32(0x11000000 + ident), D1B_AI_P2 + ident * 4: le32(0x22000000 + ident),
         D1B_AI_P1 + (ident & 0xFF) * 4 + (0x200 if ident < 0x100 else 0): le32(0x33333333),
         b + 0x10: le32(0x10101010) + le32(0x14141414) + le32(0x18181818),
         b + 0x2B: b"\x2b\x2c\x2d", b + 0x30: le32(0x30303030) + le32(0x34343434) + le32(0x38383838),
         ob + 0x10: le32(0x50505050) + le32(0x54545454) + le32(0x58585858),
         ob + 0x2B: b"\x6b\x6c\x6d", ob + 0x30: le32(0x70707070) + le32(0x74747474) + le32(0x78787878)}
    if (side ^ 1) != ((1 - side) & 0xFFFFFFFF):
        p[0x0010782A + (side ^ 1) * 0x94] = bytes([xc])
    return p


D1B_BAND_T = 0x10A600                       # zero BSS: the 0x14-entry band table b+0x14 points at
D1B_BAND_IDS = (0x8C238, 0x83E70, 0x91F0C, 0x94B8C, 0x8F0E4, 0x893E0, 0x86A34)
D1B_BAND_AUX = (0x8A4A4, 0x81E28, 0x90158, 0x92FA0, 0x8D300, 0x87B20, 0x84F7C)


def d1b_46dd4_case(side, ch, gap=False, och=None):
    """0x46DD4: the side's character (the other side holds another), the AI block's +0x14 band
    table pointer and +0x18/+0x2C..+0x2F/+0x38 sentinels (both sides), a 0x14-entry band table
    {id 0x10+i, sub 0x40+i, lo 10*i, hi 10*i+9} whose last entry ends at 0xFF (with `gap`, entry
    3 starts at 35), and the character's two id-indexed tables for ids 0x10..0x23."""
    och = (ch + 1) % 7 if och is None else och
    b, ob = 0x001081F0 + side * 0x40, 0x001081F0 + (1 - side) * 0x40
    tbl = bytearray()
    for i in range(0x14):
        lo, hi = 10 * i, (10 * i + 9 if i < 0x13 else 0xFF)
        if gap and i == 3:
            lo = 35
        tbl += bytes([0x10 + i, 0x40 + i, lo, hi])
    p = {0x0010782A + side * 0x94: bytes([ch]), 0x0010782A + (1 - side) * 0x94: bytes([och]),
         b + 0x14: le32(D1B_BAND_T) + le32(0x0010A700), b + 0x2C: b"\x2c\x2d\x2e\x2f\x30",
         b + 0x38: le32(0x38383838),
         ob + 0x14: le32(0x0010A700) + le32(0x0010A700), ob + 0x2C: b"\x6c\x6d\x6e\x6f\x70",
         ob + 0x38: le32(0x78787878),
         D1B_BAND_T: bytes(tbl[:0x28]), D1B_BAND_T + 0x28: bytes(tbl[0x28:])}
    for base, tag in ((D1B_BAND_IDS[ch], 0xA1000000), (D1B_BAND_AUX[ch], 0xA2000000)):
        vals = b"".join(le32(tag + i) for i in range(0x10, 0x24))
        p[base + 0x40] = vals[:0x28]
        p[base + 0x68] = vals[0x28:]
    return p


D1B_SPECS = [
    # 0x36300 fighter_state_36300: EAX = slot, EDX = rec; the +0x52 == 13 handler. slot+0x41 gets
    # bit 7, then slot+0x58 picks an arm through the table 0x362F0 (0 and above 3: nothing; 1: the
    # threshold test, then the 0x2BC30 restart or the decrement; 2: the decrement; 3: the rec+0x34
    # decay). Mask 0: its one caller 0x34CAD pops and returns (0x34CB2..0x34CB7) into 0x34B6C's
    # one caller, which reloads EAX at once (`mov eax,esi` 0x35801).
    Spec("fighter_state_36300", 0x36300, [
        Case("d0", {"eax": D1B_SLOT, "edx": D1B_REC}, d1b_36300_case(0)),
        Case("d4", {"eax": D1B_SLOT, "edx": D1B_REC}, d1b_36300_case(4)),
        Case("d5", {"eax": D1B_SLOT, "edx": D1B_REC}, d1b_36300_case(5)),
        Case("dff", {"eax": D1B_SLOT, "edx": D1B_REC}, d1b_36300_case(0xFF, w34=0x0040)),
        Case("a1", {"eax": D1B_SLOT, "edx": D1B_REC}, d1b_36300_case(1, w36=0, r1c=0x50)),
        Case("a2", {"eax": D1B_SLOT, "edx": D1B_REC}, d1b_36300_case(1, w36=0x8000, r1c=0x801)),
        Case("a3", {"eax": D1B_SLOT, "edx": D1B_REC}, d1b_36300_case(1, w36=0xFFFF, r1c=0x800)),
        Case("a4", {"eax": D1B_SLOT, "edx": D1B_REC},
             d1b_36300_case(1, w36=0xFFFF, r1c=0x100, thr=0x9000)),
        Case("a5", {"eax": D1B_SLOT, "edx": D1B_REC},
             d1b_36300_case(1, w36=0xFFFF, r1c=0x80000000)),
        Case("a6", {"eax": D1B_SLOT, "edx": D1B_REC},
             d1b_36300_case(1, ch=6, w36=0xFFFF, r1c=0, step=0x90)),
        Case("b1", {"eax": D1B_SLOT, "edx": D1B_REC}, d1b_36300_case(2, w36=3, s5c=0xFF)),
        Case("b2", {"eax": D1B_SLOT, "edx": D1B_REC}, d1b_36300_case(2, w36=0x8000, s5c=1)),
        Case("c1", {"eax": D1B_SLOT, "edx": D1B_REC}, d1b_36300_case(3, w34=0xFFF0)),
        Case("c2", {"eax": D1B_SLOT, "edx": D1B_REC}, d1b_36300_case(3, w34=0x0010)),
        Case("c3", {"eax": D1B_SLOT, "edx": D1B_REC}, d1b_36300_case(3, w34=0x0011)),
        Case("c4", {"eax": D1B_SLOT, "edx": D1B_REC}, d1b_36300_case(3, w34=0xFFEF)),
        Case("c5", {"eax": D1B_SLOT, "edx": D1B_REC}, d1b_36300_case(3, w34=0x8000)),
        Case("c6", {"eax": D1B_SLOT, "edx": D1B_REC}, d1b_36300_case(3, w34=0x7FFF)),
    ], calls=(ANIM_BEGIN,), eax_mask=0,
       mutants=("@or", "@mask", "@case0", "@sign36", "@thrsext", "@thridx", "@thrcmp", "@uns",
                "@inc", "@order", "@stepsext", "@char", "@stream", "@frame", "@arg0", "@sub2",
                "@subsext", "@suboff", "@subwide", "@abs", "@le", "@no52", "@zero", "@updown",
                "@step")),
    # 0x36710 fighter_state_36710: EAX = slot, EDX = rec; the +0x52 == 17 handler. The flag
    # (+0x58), the character (+0x7A) and the record handed to 0x3C480 are those of the slot of
    # rec+0x51's side (`mov ebx,0x1077b0; add ebx,eax` 0x3675A..0x36762; `mov eax,[ebx]` 0x3676E;
    # `mov dl,[ebx+0x7a]` 0x36793); only the +0x43 store goes to EDX and the +0x52 store to EAX
    # (0x367C3, 0x367CB). rec+0x51 stays in 0..2: 0x80 would index a slot past the image's end.
    # The voice stub's EAX is not read (the function returns after the call, 0x367D4). Mask 0:
    # 0x34CBC pops and returns (0x34CC1..0x34CC6) into 0x34B6C's caller, `mov eax,esi` 0x35801.
    Spec("fighter_state_36710", 0x36710, [
        Case("f0", {"eax": D1B_SLOT, "edx": D1B_REC}, d1b_36710_case(0, 0)),
        Case("f1", {"eax": D1B_SLOT, "edx": D1B_REC}, d1b_36710_case(1, 0)),
        Case("f2", {"eax": D1B_SLOT, "edx": D1B_REC}, d1b_36710_case(2, 0)),
        Case("g0", {"eax": D1B_SLOT, "edx": D1B_REC}, d1b_36710_case(0, 1)),
        Case("g1", {"eax": D1B_SLOT, "edx": D1B_REC}, d1b_36710_case(1, 1, ch=4)),
        Case("h0", {"eax": D1B_SLOT, "edx": D1B_REC}, d1b_36710_case(0, 2)),
        Case("h1", {"eax": D1B_SLOT, "edx": D1B_REC}, d1b_36710_case(1, 2), {0x2C3FC: 0}),
        Case("h2", {"eax": D1B_SLOT, "edx": D1B_REC}, d1b_36710_case(0, 2, w36=0x0100)),
        Case("h3", {"eax": D1B_SLOT, "edx": D1B_REC}, d1b_36710_case(0, 2, r1c=0x00010000)),
        Case("h4", {"eax": D1B_SLOT, "edx": D1B_REC}, d1b_36710_case(0, 3, oflag=2)),
        Case("h5", {"eax": D1B_SLOT, "edx": D1B_REC}, d1b_36710_case(1, 0xFF, oflag=0)),
    ], calls=(HIT_A, VOICE), eax_mask=0,
       mutants=("@side", "@side01", "@slotarg", "@set1", "@char", "@rec", "@stream", "@frame",
                "@order", "@set2", "@w36", "@r1c", "@no43", "@rec43", "@s52", "@vorder", "@src43",
                "@novoice", "@voice")),
    # 0x380C4 fighter_380c4: EAX = side. The side's record restarts on 0xBDDC4[char] at 1.0
    # (0x2BC30), then 0xBDDE0[char] is spawned at (0, 0, 0) with a5 = (word rec+0x56 | 0x400)
    # | (word rec+0x28 & 0x4000), zero-extended (`and edx,0xffff` 0x38112), and the spawn's +0x59
    # = 1 (0x3814C; the raw does not test the spawn for 0, and a 0 stub EAX would write outside
    # the image, so it is not exercised). The 0x2BC30 stub's EAX is not read (`mov ax,[ebx+0x28]`
    # 0x380F6); the spawn stub's EAX is the store's base and varies. Mask 0: `xor eax,eax` 0x48B7A.
    Spec("fighter_380c4", 0x380C4, [
        Case("k0", {"eax": 0}, d1b_380c4_case(0, 2, 0x4000, 0x1234), {0x2AE14: D1B_CHILD}),
        Case("k1", {"eax": 1}, d1b_380c4_case(1, 4, 0xBFFF, 0x8123), {0x2AE14: D1B_CHILD + 0x80}),
        Case("k2", {"eax": 0}, d1b_380c4_case(0, 0, 0xFFBF, 0x0400), {0x2AE14: D1B_CHILD + 0x80}),
        Case("k3", {"eax": 1}, d1b_380c4_case(1, 6, 0x0000, 0xFBFF), {0x2AE14: D1B_CHILD}),
    ], calls=(ANIM_BEGIN, SPAWN), eax_mask=0,
       mutants=("@side", "@char", "@stream", "@frame", "@rec", "@flipbit", "@noflip", "@or400",
                "@w56off", "@sext", "@desc", "@a2", "@order", "@no59", "@off59")),
    # 0x38D24 fighter_38d24: EAX = side. The word 0x107D18 + side*2 counts down; the step that
    # reaches 0 clears the combo text (0x38C5C) and redraws string 0xE5 on rows 6 and 7. The
    # string path (0x1C500 and its tree) runs on both sides, as in C3g's 0x38D90 row; the decoded
    # buffer 0x102760 is part of the memory at each 0x2F4BC call. Neither stub's EAX is read
    # (`mov eax,0xe5` 0x38D4C/0x38D6C). Mask 0: `mov eax,esi` 0x357FA, `mov eax,ecx` 0x38562.
    Spec("fighter_38d24", 0x38D24, [
        Case("t0", {"eax": 0}, d1b_38d24_case(0, 1)),
        Case("t1", {"eax": 0}, d1b_38d24_case(1, 5)),
        Case("t2", {"eax": 0}, d1b_38d24_case(2, 0)),
        Case("t3", {"eax": 1}, d1b_38d24_case(7, 1)),
        Case("t4", {"eax": 1}, d1b_38d24_case(1, 0)),
        Case("t5", {"eax": 0}, d1b_38d24_case(0x0100, 1)),
        Case("t6", {"eax": 1}, d1b_38d24_case(0, 0x0101)),
        Case("t7", {"eax": 0}, d1b_38d24_case(0x8000, 0)),
    ], allow_calls=(0x1C500, 0x474E4, 0x1E75C, 0x1E808, 0x500BB), calls=(D1B_C5C, D1B_HOLD),
       eax_mask=0,
       mutants=("@side", "@width", "@nodec", "@always", "@c5c", "@c5cside", "@col", "@mode",
                "@one", "@row", "@id", "@order")),
    # 0x39278 fighter_39278: `inc eax; mov [0x1088ec],ax; ret`. Mask 0: 0x19367 `mov eax,2`;
    # the returns through 0x192DC/0x193B0 and 0x1975C end in `xor eax,eax` (0x262F3, 0x2635B,
    # 0x26482, 0x264EA, 0x265DF, 0x26647, 0x2747D, 0x2882C, 0x29AE5), `mov eax,esp` 0x19774 or
    # 0x3CB68's `mov eax,[0x107edc]` 0x3CB79, with no read of EAX on any path (record §1).
    Spec("fighter_39278", 0x39278, [
        Case("v0", {"eax": 2}, {0x001088EA: b"\xea\xea\xec\xec\xee\xee"}),
        Case("v1", {"eax": 0xFFFF}, {0x001088EA: b"\xea\xea\xec\xec\xee\xee"}),
        Case("v2", {"eax": 0x00012345}, {0x001088EA: b"\xea\xea\xec\xec\xee\xee"}),
        Case("v3", {"eax": 0x01FF}, {0x001088EA: b"\xea\xea\xec\xec\xee\xee"}),
    ], eax_mask=0, mutants=("@noinc", "@byte", "@wide", "@off")),
    # 0x3962C fighter_3962c: EAX = side, DL = selector (`mov bl,dl` 0x39630, `test bl,bl`
    # 0x3963B: q5 and q8 set EDX's upper bytes). 0x33950 and 0x3AFC4 run on both sides; the +0x5F
    # and +0x64 bytes stay below 0x40 (above it 0x3AFC4 takes its 0x62003 error path, which the
    # port does not model). Mask 0xFF: `mov al,1` 0x3969C / `xor al,al` 0x396A3, and both callers
    # `test al,al` (0x193E9, 0x197A0) before reloading EAX.
    Spec("fighter_3962c", 0x3962C, [
        Case("q0", {"eax": 0, "edx": 0}, d1b_3962c_case(0, 0xFFEF, 0x0010, 5, 1)),
        Case("q1", {"eax": 0, "edx": 0}, d1b_3962c_case(0, 0x0010, 0xFFEF, 0, 1)),
        Case("q2", {"eax": 0, "edx": 0}, d1b_3962c_case(0, 0x0010, 0xFFEF, 1, 1)),
        Case("q3", {"eax": 1, "edx": 0}, d1b_3962c_case(1, 0x0010, 0xFFEF, 1, 0)),
        Case("q4", {"eax": 1, "edx": 1}, d1b_3962c_case(1, 0xFFEF, 0x0010, 5, 0x7FFF)),
        Case("q5", {"eax": 0, "edx": 0x100}, d1b_3962c_case(0, 0x0010, 0xFFEF, 1, 1)),
        Case("q6", {"eax": 0, "edx": 0}, d1b_3962c_case(0, 0x0010, 0xFFEF, 0x7FFF, 1)),
        Case("q7", {"eax": 0, "edx": 0}, d1b_3962c_case(0, 0x0010, 0xFFEF, 0xFFFF, 1)),
        Case("q8", {"eax": 1, "edx": 0x201}, d1b_3962c_case(1, 0xFFEF, 0x0010, 1, 2)),
        Case("q9", {"eax": 0, "edx": 0}, d1b_3962c_case(0, 0x0010, 0xFFEF, 1, 0x8000)),
    ], allow_calls=(0x33950, 0x3AFC4), eax_mask=0xFF,
       mutants=("@dl", "@sel", "@triside", "@side", "@early", "@bit", "@wordoff", "@noinc",
                "@preinc", "@cmp1", "@uns24", "@side2c", "@cmp2c", "@uns2c")),
    # 0x396AC fighter_396ac: EAX = side, DL = selector (`mov bl,dl` 0x396B1, `test bl,bl`
    # 0x396BC). The byte must be below 0x40 (0x396DA); then the 0x107A80[side*0x40 + b] counter
    # is bumped and compared, zero-extended (`xor eax,eax; mov al,..` 0x3970A/0x3970F), against
    # the descriptor's +4 word, signed (`sar ebx,0x10` 0x3970C). Mask 0xFF: `mov al,1` 0x3972B /
    # `xor al,al` 0x3972F; all four callers `test al,al` (0x14838, 0x14BB4, 0x1941E, 0x197C4).
    Spec("fighter_396ac", 0x396AC, [
        Case("r0", {"eax": 0, "edx": 0}, d1b_396ac_case(0, 3, 5, 5, 1)),
        Case("r1", {"eax": 0, "edx": 0}, d1b_396ac_case(0, 3, 4, 5, 1)),
        Case("r2", {"eax": 1, "edx": 0}, d1b_396ac_case(1, 0x3F, 9, 3, 0)),
        Case("r3", {"eax": 0, "edx": 0}, d1b_396ac_case(0, 0x40, 9, 3, 1)),
        Case("r4", {"eax": 0, "edx": 0}, d1b_396ac_case(0, 0xFF, 9, 3, 1)),
        Case("r5", {"eax": 1, "edx": 1},
             d1b_396ac_case(1, 0x50, 9, 3, 3, b64=0x11, thr64=2, pre64=7)),
        Case("r6", {"eax": 0, "edx": 0x100}, d1b_396ac_case(0, 3, 5, 5, 1, b64=0x50)),
        Case("r7", {"eax": 0, "edx": 0}, d1b_396ac_case(0, 3, 0xFF, 5, 1)),
        Case("r8", {"eax": 0, "edx": 0}, d1b_396ac_case(0, 3, 1, 0xFFFF, 1)),
        Case("r9", {"eax": 0, "edx": 0}, d1b_396ac_case(0, 3, 5, 5, 0x8000)),
        Case("r10", {"eax": 1, "edx": 0}, d1b_396ac_case(1, 7, 0x7F, 0x7F, 1)),
    ], allow_calls=(0x33950, 0x3AFC4), eax_mask=0xFF,
       mutants=("@dl", "@sel", "@bound", "@triside", "@idxside", "@wrap", "@noinc", "@preinc",
                "@thrzext", "@thrword", "@vcmp", "@side2c", "@cmp2c", "@uns2c", "@vsext")),
    # 0x399CC fighter_state_399cc: EAX = side; the +0x52 == 7 handler. The 0x33A10 context's own
    # slot gets +0x41 bit 2 and the word +0x74 = 0; then s = [0x1077A8 + side*4] (0x399EE), and
    # with s live and s+0x5D clear, 0x367DC(s, [s]) (`mov edx,[eax]` 0x399FC, 0x39A02). 0x33A10
    # and 0x367DC run on both sides; 0x367DC's 0x2BC30 calls are the stubs that show its
    # arguments. Mask 0: 0x34C82 pops and returns into 0x34B6C's caller, `mov eax,esi` 0x35801.
    Spec("fighter_state_399cc", 0x399CC, [
        Case("p0", {"eax": 0}, d1b_399cc_case(0, D1B_SLOT)),
        Case("p1", {"eax": 1}, d1b_399cc_case(1, D1B_SLOT, ch=3)),
        Case("p2", {"eax": 0}, d1b_399cc_case(0, 0)),
        Case("p3", {"eax": 0}, d1b_399cc_case(0, D1B_SLOT, s5d=1, slot5d=0)),
        Case("p4", {"eax": 1}, d1b_399cc_case(1, D1B_SLOT, s5d=0x80, slot5d=0)),
        Case("p5", {"eax": 0}, d1b_399cc_case(0, DS_SLOTS, mode=3)),
        Case("p6", {"eax": 1}, d1b_399cc_case(1, 0)),
    ], allow_calls=(0x33A10, 0x367DC), calls=(ANIM_BEGIN,), eax_mask=0,
       mutants=("@side", "@or", "@orbit", "@w74wide", "@sidx", "@nonull", "@no5d", "@5dslot",
                "@slot", "@rec", "@order", "@w74")),
    # 0x39AC8 fighter_39ac8: AX = a, DX = n. q = (a * 128) / (n * n), IDIV (truncating), on the
    # sign-extended words; then q unless q + 0.5 (the double at 0x80BFA, which the port inlines
    # and no case pokes) is not below (s16)q + 1, where it is q + 1: that is, q + 1 exactly when
    # q does not fit a word's sign extension from below. n = 0 is a #DE in the raw and is not
    # exercised. Full mask: `mov eax,edx` (and `inc eax`) at 0x39B03/0x39B0B is the whole result.
    Spec("fighter_39ac8", 0x39AC8, [
        Case("g0", {"eax": 0x0100, "edx": 4}),
        Case("g1", {"eax": 0xFF00, "edx": 4}),
        Case("g2", {"eax": 7, "edx": 3}),
        Case("g3", {"eax": 0xFFF9, "edx": 3}),
        Case("g4", {"eax": 7, "edx": 0xFFFD}),
        Case("g5", {"eax": 0x7FFF, "edx": 1}),
        Case("g6", {"eax": 0x0100, "edx": 1}),
        Case("g7", {"eax": 0x8000, "edx": 1}),
        Case("g8", {"eax": 0x12340100, "edx": 0x56780004}),
        Case("g9", {"eax": 0, "edx": 5}),
        Case("g10", {"eax": 0xFFFF, "edx": 11}),
        Case("g11", {"eax": 0x7FFF, "edx": 0x0100}),
        Case("g12", {"eax": 0x7FFF, "edx": 0x8000}),
        Case("g13", {"eax": 0x00FF, "edx": 1}),
    ], mutants=("@wide", "@nzext", "@sq", "@shift", "@floor", "@noround", "@always", "@s16")),
    # 0x39B14 fighter_39b14: `imul eax,edx; ret`. Full mask (the product's low 32 bits).
    Spec("fighter_39b14", 0x39B14, [
        Case("m0", {"eax": 3, "edx": 5}),
        Case("m1", {"eax": 0xFFFF, "edx": 0xFFFF}),
        Case("m2", {"eax": 0x00010000, "edx": 0x00010000}),
        Case("m3", {"eax": 0x12345678, "edx": 0x9ABCDEF0}),
        Case("m4", {"eax": 0xFFFFFFFE, "edx": 3}),
    ], mutants=("@add", "@word", "@trunc", "@hi")),
    # 0x3A2A0 fighter_3a2a0: EAX = side, EDX/EBX/ECX and the stack word (`ret 4`) the pose words.
    # 0x33950, 0x3AFC4 and 0x3A280 run on both sides; the other eight callees are stubs. The
    # 0x3B298 stub's AL picks the arm (`test al,al` 0x3A2F7: n1 returns 0x100); no other stub's
    # EAX is read (each call is followed by a load of EAX). The +0x5F byte stays below 0x40
    # (0x3AFC4's error path). 0x39F40's stack word is EDX as 0x3A335 loaded it, which holds
    # across 0x3A0FC and 0x18B04 (neither clobbers EDX; `push edx` 0x3A349). Full mask: EAX =
    # ESI = 0 (0x3A375/0x3A37A) or 1 (0x3A38B/0x3A3E2).
    Spec("fighter_3a2a0", 0x3A2A0, [
        Case("n0", {"eax": 0, **D1B_POSE}, d1b_3a2a0_case(0, 0x05), {0x3B298: 0}),
        Case("n1", {"eax": 1, **D1B_POSE}, d1b_3a2a0_case(1, 0x10), {0x3B298: 0x100}),
        Case("n2", {"eax": 0, **D1B_POSE}, d1b_3a2a0_case(0, 0x3F, ch=6, och=1),
             {0x3B298: 0, 0x2C3FC: 0}),
        Case("n3", {"eax": 1, **D1B_POSE}, d1b_3a2a0_case(1, 0x18), {0x3B298: 0}),
        Case("h0", {"eax": 0, **D1B_POSE}, d1b_3a2a0_case(0, 0x05), {0x3B298: 1}),
        Case("h1", {"eax": 1, **D1B_POSE}, d1b_3a2a0_case(1, 0x07, o54=2), {0x3B298: 0xFF}),
        Case("h2", {"eax": 0, **D1B_POSE}, d1b_3a2a0_case(0, 0x21, o54=3), {0x3B298: 0x80}),
    ], allow_calls=(0x33950, 0x3AFC4, 0x3A280), calls=D1B_3A2A0_CALLS,
       mutants=("@triside", "@dside", "@db", "@al", "@order", "@c8a", "@c18", "@c18w",
                "@voiceidx", "@novoice", "@a0fc", "@facing", "@pswap", "@pword", "@pecx",
                "@p834", "@or41", "@no41", "@ret", "@s54", "@b080", "@b080c", "@and42a",
                "@and42b", "@bit42", "@ad98", "@ret1")),
    # 0x3B938 fighter_3b938: EAX = the thrower's slot. Its +0x08 projectile restarts on 0xE1898
    # at 2.0 when the projectile's state byte +0x48 is 4, else on 0xBDFC8[char] at the float of
    # the byte 0xBDFF0[char] (zero-extended, `fild word` 0x3B96B); then slot+0x64 = 0xFF, the
    # projectile's +0x48 and words +0x34/+0x36 clear, slot+0x08 = 0 and the voice 0xBDFFA[char].
    # Neither stub's EAX is read. Mask 0: `mov eax,[0x10784c]` 0x17C97, `mov eax,esi` 0x197FD.
    Spec("fighter_3b938", 0x3B938, [
        Case("s0", {"eax": D1B_SLOT}, d1b_3b938_case(4, 2)),
        Case("s1", {"eax": D1B_SLOT}, d1b_3b938_case(0, 3, step=0x90)),
        Case("s2", {"eax": D1B_SLOT}, d1b_3b938_case(5, 1), {0x2C3FC: 0}),
        Case("s3", {"eax": D1B_SLOT}, d1b_3b938_case(4, 6, stw_hi=0)),
        Case("s4", {"eax": D1B_SLOT}, d1b_3b938_case(0xFF, 0, step=7)),
    ], calls=(ANIM_BEGIN, VOICE), eax_mask=0,
       mutants=("@char", "@stw", "@stream4", "@frame4", "@stepsext", "@arg0", "@frame", "@s64",
                "@p48", "@p34", "@p36", "@vidx", "@s08", "@vorder", "@early", "@novoice")),
    # 0x3B9D8 fighter_3b9d8: EAX = side, CX = pen (sign-extended at 0x3BA39/0x3BA6E: u4's EDX
    # has upper bits). 0x33950 runs on both sides. The 0x3B8D8 stub's AL picks the pushed side
    # (`test al,al` 0x3BA53/0x3BA8A: u2 returns 0x100); the 0x1883C stub's EAX is not read and
    # its write moves side 0's x, which the tail's repeated x test (0x3BAB9) then reads. A zero
    # +0x34 word makes the tail's store unobservable, so the `> 0` / `< 0` boundaries are not
    # separated from `>= 0` / `<= 0`. Mask 0: `xor eax,eax` 0x3BB48/0x3BB64, `mov eax,1` 0x3BB58.
    Spec("fighter_3b9d8", 0x3B9D8, [
        Case("e0", {"eax": 0, "edx": 0x11}, d1b_3b9d8_case(0, 0x900, 0x700, s42=0x20)),
        Case("e1", {"eax": 0, "edx": 0x11}, d1b_3b9d8_case(0, 0x900, 0x700, o43=0x02, o4c=0x8123)),
        Case("e2", {"eax": 1, "edx": 0x11}, d1b_3b9d8_case(1, 0x900, 0x700, o43=0x02, o4c=0x0123)),
        Case("u0", {"eax": 0, "edx": 0x0011}, d1b_3b9d8_case(0, 0x700, 0x700), {0x3B8D8: 0}),
        Case("u1", {"eax": 0, "edx": 0xFFEF}, d1b_3b9d8_case(0, 0x900, 0x700), {0x3B8D8: 1}),
        Case("u2", {"eax": 1, "edx": 0x0011}, d1b_3b9d8_case(1, 0x300, 0x700), {0x3B8D8: 0x100}),
        Case("u3", {"eax": 1, "edx": 0x0011}, d1b_3b9d8_case(1, 0x300, 0x700), {0x3B8D8: 1}),
        Case("u4", {"eax": 0, "edx": 0x00018010}, d1b_3b9d8_case(0, 0x900, 0x700), {0x3B8D8: 0}),
        Case("u5", {"eax": 1, "edx": 0x0020}, d1b_3b9d8_case(1, 0xFFFFF000, 0x700),
             {0x3B8D8: 0}),
        Case("z0", {"eax": 0, "edx": 0x0010}, d1b_3b9d8_case(0, 0x900, 0x700, s54=2, w34=5),
             {0x3B8D8: 0}),
        Case("z1", {"eax": 0, "edx": 0x0010}, d1b_3b9d8_case(0, 0x900, 0x700, s54=2, w34=0xFFFB),
             {0x3B8D8: 0}),
        Case("z2", {"eax": 1, "edx": 0x0010}, d1b_3b9d8_case(1, 0x600, 0x700, s54=2, w34=0xFFFB),
             {0x3B8D8: 0}),
        Case("z3", {"eax": 1, "edx": 0x0010}, d1b_3b9d8_case(1, 0x600, 0x700, s54=2, w34=5),
             {0x3B8D8: 1}),
        Case("z4", {"eax": 1, "edx": 0x0010}, d1b_3b9d8_case(1, 0x500, 0x700, s54=2, w34=0x7FFF),
             {0x3B8D8: 0}),
    ], allow_calls=(0x33950,), calls=(D1B_1883C_X, D1B_3B8D8), eax_mask=0,
       mutants=("@pen16", "@nobit5", "@bit5", "@or41a", "@or41b", "@b43side", "@b43bit",
                "@w4czext", "@w4coff", "@d30", "@d30order", "@xge", "@xuns", "@floor", "@bside",
                "@bval", "@al", "@wallside", "@wallval", "@freeside", "@s54", "@cache", "@w34rec",
                "@w34swap", "@w34wide")),
    # 0x3C1C8 fighter_3c1c8: EAX = side, DX = v. The side's record takes the word +0x34 = -v
    # while its slot's +0x2C is below the other's (signed, `jge` 0x3C1E5), else v. 0x33950 runs
    # on both sides. Mask 0: `mov eax,[esp+0x10]` 0x45415.
    Spec("fighter_3c1c8", 0x3C1C8, [
        Case("x0", {"eax": 0, "edx": 0x0005}, d1b_x_case(0, 0x100, 0x200)),
        Case("x1", {"eax": 0, "edx": 0x0005}, d1b_x_case(0, 0x200, 0x200)),
        Case("x2", {"eax": 1, "edx": 0x00012345}, d1b_x_case(1, 0x300, 0x200)),
        Case("x3", {"eax": 1, "edx": 0x8000}, d1b_x_case(1, 0xFFFFFF00, 0x200)),
        Case("x4", {"eax": 0, "edx": 0x0007}, d1b_x_case(0, 0x100, 0xFFFFFF00)),
    ], allow_calls=(0x33950,), eax_mask=0,
       mutants=("@side", "@cmp", "@uns", "@swap", "@rec", "@wide")),
    # 0x3F6F4 fighter_3f6f4: EAX = side, EDX = n. n minus the side slot's word +0x88, sign-
    # extended (`sar eax,0x10` of the dword at +0x86, 0x3F70D), or 0 when that is negative
    # (`xor eax,ebx` on equal registers, 0x3F718). 0x33950 runs on both sides. Full mask: its
    # callers move EAX to EDX (0x233EA, 0x2343E, 0x457D0, 0x45813, 0x4881A, 0x4886D) or compare
    # it (`cmp eax,1` 0x3FA42).
    Spec("fighter_3f6f4", 0x3F6F4, [
        Case("y0", {"eax": 0, "edx": 10}, d1b_3f6f4_case(0, 5)),
        Case("y1", {"eax": 0, "edx": 5}, d1b_3f6f4_case(0, 5)),
        Case("y2", {"eax": 1, "edx": 4}, d1b_3f6f4_case(1, 5)),
        Case("y3", {"eax": 1, "edx": 0}, d1b_3f6f4_case(1, 0x8000)),
        Case("y4", {"eax": 0, "edx": 0x7FFFFFFF}, d1b_3f6f4_case(0, 0xFFFF)),
        Case("y5", {"eax": 0, "edx": 0x80000005}, d1b_3f6f4_case(0, 3)),
    ], allow_calls=(0x33950,),
       mutants=("@zext", "@off", "@side", "@noclamp", "@abs", "@add")),
    # 0x3C6E8 fighter_cmd_facing: EAX = facing (`test ebx,ebx` 0x3C6F0, the whole register: c9),
    # EDX = the input bitfield. Each of the bits 0x10000/0x40000/0x20000/0x80000 ORs one of
    # 0x10/0x20/0x1000/0x2000 into the input, in the order facing selects, and the result is the
    # low word, zero-extended (`and edx,0xffff; mov eax,edx` 0x3C748). Full mask.
    Spec("fighter_cmd_facing", 0x3C6E8, [
        Case("c0", {"eax": 0, "edx": 0x00010000}),
        Case("c1", {"eax": 0, "edx": 0x00020000}),
        Case("c2", {"eax": 0, "edx": 0x00040000}),
        Case("c3", {"eax": 0, "edx": 0x00080000}),
        Case("c4", {"eax": 1, "edx": 0x00010000}),
        Case("c5", {"eax": 1, "edx": 0x00020000}),
        Case("c6", {"eax": 1, "edx": 0x00040000}),
        Case("c7", {"eax": 1, "edx": 0x00080000}),
        Case("c8", {"eax": 0, "edx": 0xFFF08A45}),
        Case("c9", {"eax": 0x100, "edx": 0x00010401}),
        Case("c10", {"eax": 0, "edx": 0x0000CDEF}),
    ], mutants=("@fswap", "@pairs", "@no1", "@no4", "@no2", "@no8", "@trunc", "@low", "@fbyte")),
    # 0x3D0C0 fighter_3d0c0: EAX = side. Four slot bytes and five record fields, no callee.
    # Mask 0: `mov eax,[esp+0xc]` 0x3FE09.
    Spec("fighter_3d0c0", 0x3D0C0, [
        Case("i0", {"eax": 0}, d1b_3d0c0_case(0)),
        Case("i1", {"eax": 1}, d1b_3d0c0_case(1)),
    ], eax_mask=0,
       mutants=("@s52", "@s53", "@s54", "@s5c", "@r44", "@r43", "@r42", "@r36", "@r34", "@side",
                "@v52", "@v53", "@r44b")),
    # 0x3F308 fighter_3f308: EAX = slot. Voice 0x6C; the slot's record spawns 0xBB308 at its
    # +0x18, +0x30 >> 16 (signed, `sar ecx,0x10` 0x3F33C) and +0x1C with a5 = its word +0x28 &
    # 0x4000; 0x13244; voice 0x72. No stub's EAX is read (the spawn's is dropped). Mask 0:
    # `mov eax,[ecx]` 0x3E72E, `mov eax,[esp]` 0x3F1A3 and 0x3F46F.
    Spec("fighter_3f308", 0x3F308, [
        Case("w0", {"eax": D1B_SLOT}, d1b_spawn_case(0x4000, 0xFFF01234)),
        Case("w1", {"eax": D1B_SLOT}, d1b_spawn_case(0xBFFF, 0x00301234), {0x2C3FC: 0}),
        Case("w2", {"eax": D1B_SLOT}, d1b_spawn_case(0xC000, 0x7FFF0000)),
    ], calls=(VOICE, SPAWN, E.Call(0x13244, ())), eax_mask=0,
       mutants=("@nov1", "@v1", "@noflag", "@flag", "@desc", "@a2", "@a3zext", "@a4", "@nov2",
                "@v2", "@order", "@no13244")),
    # 0x3F720 fighter_3f720: EAX = side, EDX = n (below 1 is 1, signed: `cmp ebx,1; jge`
    # 0x3F72F). The speed is |0x187FC()| / n (IDIV; 0x187FC is called twice, once for the sign
    # and once for the value) or 100 when AL of 0x189FC(the other side) is 0 (`test al,al`
    # 0x3F766: d5 returns 0x100); 0x3C190(side, speed). 0x33950 runs on both sides. The 0x3C190
    # stub's EAX is the function's EAX at return and is not compared. Mask 0: every direct
    # caller reloads EAX (0x146B7, 0x14A23, 0x233F4, 0x23448, 0x3FBE9, 0x457DA, 0x4581D, 0x48824,
    # 0x48877); 0x3FA80's return leaves 0x3F9C8, a slot +0x0C callback (`mov dword [ebx+0xc],
    # 0x3f9c8` 0x3FBC1), whose two `call [ecx+0xc]` sites drop EAX (`xor eax,eax` 0x35434;
    # 0x384D9 returns through 0x38434 to `mov eax,esi` 0x38570).
    Spec("fighter_3f720", 0x3F720, [
        Case("d0", {"eax": 0, "edx": 4}, {}, {0x187FC: 100, 0x189FC: 1, 0x3C190: 0x11}),
        Case("d1", {"eax": 1, "edx": 3}, {}, {0x187FC: 0xFFFFFF9C, 0x189FC: 1, 0x3C190: 0x22}),
        Case("d2", {"eax": 0, "edx": 0}, {}, {0x187FC: 77, 0x189FC: 0xFF}),
        Case("d3", {"eax": 0, "edx": 0xFFFFFFFB}, {}, {0x187FC: 0xFFFFFFF9, 0x189FC: 1}),
        Case("d4", {"eax": 1, "edx": 2}, {}, {0x187FC: 50, 0x189FC: 0}),
        Case("d5", {"eax": 0, "edx": 2}, {}, {0x187FC: 50, 0x189FC: 0x100}),
        Case("d6", {"eax": 0, "edx": 1}, {}, {0x187FC: 0, 0x189FC: 1}),
        Case("d7", {"eax": 1, "edx": 7}, {}, {0x187FC: 0x80000000, 0x189FC: 1}),
        Case("d8", {"eax": 0, "edx": 2}, {}, {0x187FC: 0xFFFFFFF9, 0x189FC: 1}),
    ], allow_calls=(0x33950,), calls=D1B_3F720_CALLS, eax_mask=0,
       mutants=("@nclamp", "@nuns", "@once", "@noabs", "@udiv", "@al", "@bside", "@v100",
                "@cside", "@order")),
    # 0x408E8 fighter_408e8: EAX = slot. As 0x3F308 with the descriptor 0xC77B0 and, in place of
    # 0x13244, the spawn's EAX stored to DS_00108094 (0x4092C; the stub's EAX varies). Mask 0:
    # `mov eax,[esp+0xc]` 0x40A27.
    Spec("fighter_408e8", 0x408E8, [
        Case("w0", {"eax": D1B_SLOT}, d1b_spawn_case(0x4000, 0xFFF01234), {0x2AE14: 0x0010A968}),
        Case("w1", {"eax": D1B_SLOT}, d1b_spawn_case(0xBFFF, 0x00301234),
             {0x2AE14: 0, 0x2C3FC: 0}),
        Case("w2", {"eax": D1B_SLOT}, d1b_spawn_case(0xC000, 0x7FFF0000), {0x2AE14: 0x12345678}),
    ], calls=(VOICE, SPAWN), eax_mask=0,
       mutants=("@nov1", "@v1", "@noflag", "@flag", "@desc", "@a2", "@a3zext", "@a4", "@nov2",
                "@v2", "@store", "@storew", "@vorder")),
    # 0x46698 ai_pred_46698: EAX = side. 0 unless the side slot's +0x40 bit 7 is set; then, with
    # the other's +0x2C above the side's (signed, `jle` 0x466C4), 1 when the side record's word
    # +0x34 >= 0, else 1 when it is <= 0. 0x33950 runs on both sides. Mask 0xFF: `mov al,1`
    # 0x466EA / `xor al,al`, and `test al,al` at 0x46A0C.
    Spec("ai_pred_46698", 0x46698, [
        Case("a0", {"eax": 0}, d1b_46698_case(0, 0x40, 0x100, 0x200, 5)),
        Case("a1", {"eax": 0}, d1b_46698_case(0, 0x80, 0x100, 0x200, 0)),
        Case("a2", {"eax": 0}, d1b_46698_case(0, 0x80, 0x100, 0x200, 0xFFFF)),
        Case("a3", {"eax": 1}, d1b_46698_case(1, 0xBF, 0x100, 0x200, 1)),
        Case("a4", {"eax": 0}, d1b_46698_case(0, 0x80, 0x200, 0x200, 0)),
        Case("a5", {"eax": 0}, d1b_46698_case(0, 0x80, 0x200, 0x200, 1)),
        Case("a6", {"eax": 1}, d1b_46698_case(1, 0x80, 0x300, 0x200, 0xFFFF)),
        Case("a7", {"eax": 0}, d1b_46698_case(0, 0x80, 0xFFFFFF00, 0x200, 0x7FFF)),
        Case("a8", {"eax": 1}, d1b_46698_case(1, 0x80, 0x200, 0xFFFFFF00, 0x8000)),
    ], allow_calls=(0x33950,), eax_mask=0xFF,
       mutants=("@bit", "@nobit", "@xcmp", "@xuns", "@swap", "@rec", "@ge", "@le", "@side")),
    # 0x466F4 ai_pred_466f4: EAX = side. 1 when the slot's +0x5F is 0xFF and +0x54, +0x53 and
    # +0x52 are 0. Mask 0xFF: `mov al,1` 0x46738 / `xor al,al`; `test al,al` 0x46A25.
    Spec("ai_pred_466f4", 0x466F4, [
        Case("b0", {"eax": 0}, d1b_state_case(0, 0xFF, 0, 0, 0)),
        Case("b1", {"eax": 0}, d1b_state_case(0, 0xFE, 0, 0, 0)),
        Case("b2", {"eax": 0}, d1b_state_case(0, 0xFF, 1, 0, 0)),
        Case("b3", {"eax": 0}, d1b_state_case(0, 0xFF, 0, 1, 0)),
        Case("b4", {"eax": 1}, d1b_state_case(1, 0xFF, 0, 0, 1)),
        Case("b5", {"eax": 1}, d1b_state_case(1, 0xFF, 0, 0, 0)),
        Case("b6", {"eax": 0}, d1b_state_case(0, 0x7F, 9, 9, 9, o=(0xFF, 0, 0, 0))),
    ], eax_mask=0xFF, mutants=("@side", "@no5f", "@no54", "@no53", "@no52")),
    # 0x4673C ai_pred_4673c: EAX = side. +0x5F == 0xFF, +0x54 == 1, +0x53 == 0 and +0x52 5 or
    # 0x15. Mask 0xFF: `mov al,1` 0x4678C / `xor al,al`; `test al,al` 0x46A45.
    Spec("ai_pred_4673c", 0x4673C, [
        Case("b0", {"eax": 0}, d1b_state_case(0, 0xFF, 1, 0, 5)),
        Case("b1", {"eax": 1}, d1b_state_case(1, 0xFF, 1, 0, 0x15)),
        Case("b2", {"eax": 0}, d1b_state_case(0, 0xFF, 1, 0, 4)),
        Case("b3", {"eax": 0}, d1b_state_case(0, 0x00, 1, 0, 5)),
        Case("b4", {"eax": 0}, d1b_state_case(0, 0xFF, 0, 0, 5)),
        Case("b5", {"eax": 1}, d1b_state_case(1, 0xFF, 2, 0, 0x15)),
        Case("b6", {"eax": 0}, d1b_state_case(0, 0xFF, 1, 1, 5)),
        Case("b7", {"eax": 1}, d1b_state_case(1, 0x7F, 9, 9, 9, o=(0xFF, 1, 0, 5))),
    ], eax_mask=0xFF, mutants=("@side", "@no5f", "@no54", "@no53", "@no52", "@no5", "@no15")),
    # 0x46794 ai_pred_46794: EAX = side. +0x5F == 0xFF, +0x54 == 2, +0x53 == 0, +0x52 == 4.
    # Mask 0xFF: `mov al,1` 0x467D8 / `xor al,al`; `test al,al` 0x46A65.
    Spec("ai_pred_46794", 0x46794, [
        Case("b0", {"eax": 0}, d1b_state_case(0, 0xFF, 2, 0, 4)),
        Case("b1", {"eax": 1}, d1b_state_case(1, 0xFF, 2, 0, 4)),
        Case("b2", {"eax": 0}, d1b_state_case(0, 0xFF, 2, 0, 5)),
        Case("b3", {"eax": 0}, d1b_state_case(0, 0x80, 2, 0, 4)),
        Case("b4", {"eax": 0}, d1b_state_case(0, 0xFF, 1, 0, 4)),
        Case("b5", {"eax": 1}, d1b_state_case(1, 0xFF, 2, 2, 4)),
        Case("b6", {"eax": 0}, d1b_state_case(0, 0x7F, 9, 9, 9, o=(0xFF, 2, 0, 4))),
    ], eax_mask=0xFF, mutants=("@side", "@no5f", "@no54", "@no53", "@no52")),
    # 0x46898 ai_pred_46898: EAX = side. 1 when the side slot's +0x10 is 0x22BEC, its record's
    # +0x24 has no bit below bit 31 (`test dword [eax+0x24],0x7fffffff` 0x468B6) and the slot's
    # +0x54 is 2. 0x33A10 runs on both sides. Mask 0xFF: `mov al,1` 0x468C9 / `xor al,al`
    # 0x468D0; `test al,al` 0x46B44.
    Spec("ai_pred_46898", 0x46898, [
        Case("e0", {"eax": 0}, d1b_46898_case(0, 0x22BEC, 0x80000000, 2)),
        Case("e1", {"eax": 1}, d1b_46898_case(1, 0x22BEC, 0, 2)),
        Case("e2", {"eax": 0}, d1b_46898_case(0, 0x22BED, 0, 2)),
        Case("e3", {"eax": 0}, d1b_46898_case(0, 0x22BEC, 1, 2)),
        Case("e4", {"eax": 1}, d1b_46898_case(1, 0x22BEC, 0, 1)),
        Case("e5", {"eax": 0}, d1b_46898_case(0, 0, 1, 9, o=(0x22BEC, 0, 2))),
    ], allow_calls=(0x33A10,), eax_mask=0xFF,
       mutants=("@side", "@no10", "@no24", "@mask", "@no54", "@s54")),
    # 0x46958 ai_ready: EAX = side. 1 when the slot's +0x53 is 7 or 8 and its +0x5F, zero-
    # extended (`and eax,0xff` 0x4698E), is below 0x40. Mask 0xFF: `mov al,1` 0x4699C / `xor
    # al,al` 0x469A1; `test al,al` 0x469DC.
    Spec("ai_ready", 0x46958, [
        Case("r0", {"eax": 0}, d1b_state_case(0, 0x00, 0x54, 7, 0x52)),
        Case("r1", {"eax": 1}, d1b_state_case(1, 0x3F, 0x54, 8, 0x52)),
        Case("r2", {"eax": 0}, d1b_state_case(0, 0x40, 0x54, 8, 0x52)),
        Case("r3", {"eax": 0}, d1b_state_case(0, 0xFF, 0x54, 7, 0x52)),
        Case("r4", {"eax": 1}, d1b_state_case(1, 0x00, 0x54, 6, 0x52)),
        Case("r5", {"eax": 0}, d1b_state_case(0, 0x00, 0x54, 9, 0x52)),
        Case("r6", {"eax": 1}, d1b_state_case(1, 0x7F, 0x54, 9, 0x52, o=(0x10, 9, 7, 9))),
    ], eax_mask=0xFF, mutants=("@side", "@no8", "@no7", "@bound", "@s8")),
    # 0x4FB20 fighter_4fb20: no argument. The two axis distances of the latched points are
    # stored signed and absolute; 0 when either exceeds the zero-extended width word 0xD33A8
    # (`jg`: equal passes), else max + (min >> 2) + ((min >> 2) >> 1), returned when the width is
    # above it (`jg` 0x4FB8B), else 0. The y pair is not written when the x test fails. The y
    # test's boundary is not observable (|dy| == width makes the estimate >= width, so 0 either
    # way, after the same stores). Full mask.
    Spec("fighter_4fb20", 0x4FB20, [
        Case("o0", {}, d1b_4fb20_case(100, 50, 40, 30, 200)),
        Case("o1", {}, d1b_4fb20_case(40, 30, 100, 50, 200)),
        Case("o2", {}, d1b_4fb20_case(60, 0, 40, 60, 200)),
        Case("o3", {}, d1b_4fb20_case(301, 5, 100, 0, 200)),
        Case("o4", {}, d1b_4fb20_case(300, 5, 100, 5, 200)),
        Case("o5", {}, d1b_4fb20_case(10, 0, 0, 201, 200)),
        Case("o6", {}, d1b_4fb20_case(10, 200, 0, 0, 200)),
        Case("o7", {}, d1b_4fb20_case(100, 7, 0, 0, 102)),
        Case("o8", {}, d1b_4fb20_case(100, 7, 0, 0, 101)),
        Case("o9", {}, d1b_4fb20_case(0x9000, 0, 0, 0x100, 0xFFFF)),
        Case("o10", {}, d1b_4fb20_case(0x80000000, 5, 0, 0, 0x7000)),
        Case("o11", {}, d1b_4fb20_case(0, 0, 0, 0x80000000, 0x7000)),
    ], mutants=("@wsext", "@sdx", "@noabsx", "@sadx", "@early", "@gex", "@sdy", "@noabsy",
                "@sady", "@max", "@shift", "@noq2", "@ret")),
    # 0x46BBC ai_weight: EAX = side, EDX = the weight id. The pair index is the side's character
    # * 10 + the character of side 1 - side (`mov edx,1; sub edx,ebx` 0x46BE4/0x46BE9: j3 is
    # side 2, where 1 - side and side ^ 1 are different slots); the id's low byte goes to the
    # side's AI block +0x2C and the id-indexed dwords of the two pair tables 0x95CA8/0x95E38 to
    # +0x14/+0x34, the id whole as the index (`mov eax,[esp]` 0x46C1C: j2). Mask 0: every call
    # site (all in 0x46C78) reaches `mov eax,[ebx+0x108218]` 0x46DA4 with no read of EAX before it.
    Spec("ai_weight", 0x46BBC, [
        Case("j0", {"eax": 0, "edx": 0x05}, d1b_46bbc_case(0, 2, 4, 0x05)),
        Case("j1", {"eax": 1, "edx": 0x8A}, d1b_46bbc_case(1, 6, 1, 0x8A)),
        Case("j2", {"eax": 0, "edx": 0x105}, d1b_46bbc_case(0, 3, 0, 0x105)),
        Case("j3", {"eax": 2, "edx": 0x07}, d1b_46bbc_case(2, 1, 3, 0x07)),
    ], eax_mask=0,
       mutants=("@oside", "@idx", "@bside", "@idb", "@idw", "@s2c", "@s14", "@s34", "@p12")),
    # 0x46DD4 ai_band: EAX = side. The character (0..6, the switch 0x46DB8; above 6 the raw
    # leaves EBP/EDI as its caller had them, which the port cannot reproduce: not exercised)
    # picks the id-indexed tables; the band is |0x187FC()| / 64 (the `sbb` idiom 0x46E79..
    # 0x46E83 truncates toward zero) clamped to 0xFF, signed (`jle` 0x46E9C: k9's 0x80000000
    # stays negative and matches no band), searched in the 0x14-entry table b+0x14 (first entry
    # with lo <= band <= hi, bytes zero-extended; entry 0 when none). 0x187FC is called twice.
    # Mask 0: its one caller 0x46FEC goes on to `lea eax,[edx*4]` 0x46FF1 with no read of EAX.
    Spec("ai_band", 0x46DD4, [
        Case("k0", {"eax": 0}, d1b_46dd4_case(0, 0), {0x187FC: 320}),
        Case("k1", {"eax": 1}, d1b_46dd4_case(1, 1), {0x187FC: 0xFFFFF9C0}),
        Case("k2", {"eax": 0}, d1b_46dd4_case(0, 2), {0x187FC: 64 * 199 + 63}),
        Case("k3", {"eax": 1}, d1b_46dd4_case(1, 3), {0x187FC: 64 * 300}),
        Case("k4", {"eax": 0}, d1b_46dd4_case(0, 4), {0x187FC: 64 * 10}),
        Case("k5", {"eax": 1}, d1b_46dd4_case(1, 5), {0x187FC: 64 * 9 + 63}),
        Case("k6", {"eax": 0}, d1b_46dd4_case(0, 6), {0x187FC: 64 * 19}),
        Case("k7", {"eax": 0}, d1b_46dd4_case(0, 2, gap=True), {0x187FC: 64 * 32}),
        Case("k8", {"eax": 1}, d1b_46dd4_case(1, 4, gap=True), {0x187FC: 64 * 35}),
        Case("k9", {"eax": 0}, d1b_46dd4_case(0, 5), {0x187FC: 0x80000000}),
        Case("k10", {"eax": 1}, d1b_46dd4_case(1, 0), {0x187FC: 64 * 189 + 1}),
    ], calls=(E.Call(0x187FC, ()),), eax_mask=0,
       mutants=("@side", "@bside", "@once", "@noabs", "@uclamp", "@clamp", "@lo", "@hi", "@bsext",
                "@n14", "@fall", "@s2d", "@s2ftab", "@tabswap", "@s18", "@s38", "@tbl14")),
]
D1_SPECS += D1B_SPECS


# ---- track D1 batch d1d (record 2026-10-11-reverse-d1d): game/flow.c second half, flow.h ---------
#
# Every poke stays inside the image; no index sentinel leaves mem[] (brief, "The port side must stay
# inside mem[]").

def d1d_chunks(base, buf):
    """`buf` at `base` as pokes of at most 0x40 bytes (diffrun's poke limit)."""
    return {base + o: bytes(buf[o:o + 0x40]) for o in range(0, len(buf), 0x40)}


D1D_LIST, D1D_LIST_END = 0x00107608, 0x00107798     # 0x33904's table: `mov eax,0x107608`, `cmp eax,0x107798`


def d1d_list_case(live):
    """0x33904: the 0x10-stride table with every entry's +0 = 0x11111111 and +0xC = 0x33333333 (so
    a read at the wrong offset or stride finds a non-zero dword), +8 = 0, and +4 = live[addr] (0
    elsewhere). The entry AT the end bound (0x107798) is seeded live: only `jae` keeps it out."""
    n = (D1D_LIST_END + 0x10 - D1D_LIST) // 0x10
    assert all((a - D1D_LIST) % 0x10 == 0 and D1D_LIST <= a < D1D_LIST_END for a in live), live
    buf = bytearray()
    for i in range(n):
        a = D1D_LIST + i * 0x10
        v = 0x44444444 if a == D1D_LIST_END else live.get(a, 0)
        buf += le32(0x11111111) + le32(v) + le32(0) + le32(0x33333333)
    return d1d_chunks(D1D_LIST, buf)


def d1d_edge_case(a3c, a48, a4a, a4c, a4e, a3a=0x7B7B):
    """0x389C4: the gate/high word 0x107A3C (with its low neighbour 0x107A3A), the numerator word
    0x107A48, the pair 0x107A4A/0x107A4C, the source 0x107A4E; 0x107A38 seeded 0xEEEE."""
    return {0x00107A38: le16(0xEEEE), 0x00107A3A: le16(a3a), 0x00107A3C: le16(a3c),
            0x00107A48: le16(a48), 0x00107A4A: le16(a4a), 0x00107A4C: le16(a4c),
            0x00107A4E: le16(a4e)}


def d1d_fill_case(stride, divisor, count, a4c, a42, a50):
    """0x38A38: the dword 0xF0AF0, the divisor word 0x107A40 (never 0: the raw's `idiv ecx` at
    0x38A59 traps), the count word 0x107A52, the edge word 0x107A4C, the tail's words
    0x107A42/0x107A50. Every word the function writes is seeded: the table 0x107900 (count + 2
    entries of 0xA5A5), 0x107A3E, 0x107A46 (and its low neighbour 0x107A44), 0x107A3A."""
    p = {0x000F0AF0: le32(stride), 0x00107A40: le16(divisor), 0x00107A52: le16(count),
         0x00107A4C: le16(a4c), 0x00107A42: le16(a42), 0x00107A50: le16(a50),
         0x00107A3E: le16(0xEEEE), 0x00107A44: le16(0x4444) + le16(0x6666),
         0x00107A3A: le16(0xAAAA) + le16(0x3C3C), 0x00107A38: le16(0x3838)}
    p.update(d1d_chunks(0x00107900, le16(0xA5A5) * (count + 2)))
    return p


D1D_ROWS = 0x00107A1C          # 0x38B18's seven-dword table; 0x107A18 is the dword before it


def d1d_row_case(used):
    """0x38B18: the table with the slots in `used` taken (distinct non-zero dwords), the dword
    before it (0x107A18, the last of the 0x107A00 array) and the word after it (0x107A38) seeded."""
    buf = le32(0x18181818) + b"".join(le32(0x00C0FFE0 + k if k in used else 0) for k in range(7)) \
        + le16(0x3838)
    return {D1D_ROWS - 4: buf}


D1D_PREC, D1D_PREC2 = 0x0010A300, 0x0010A400       # the portrait's record and the wrong-index one


def d1d_portrait_case(i, flags, ch, s29, flash=0x0123, reveal=0x0456):
    """0x414B4/0x41528: slot i's class byte 0x108106[i] (the seven bytes seeded distinct), its
    character 0xC8664[i] = ch, the record pointer 0x1080C0[ch] (and a different record at [i]), the
    two stream words 0xC85D8[ch]/0xC85E6[ch], the reveal palette 0xC8648[ch] and the flash palette
    0xC8610[col] for the raw's col, each a sentinel no image dword holds; the record's +0x28..+0x2A."""
    cls, bit6 = flags & 0xF, flags & 0x40
    col = cls + 7 if bit6 else cls
    seven = bytearray(0x60 + k for k in range(7))
    seven[i] = flags
    return {0x00108106: bytes(seven), 0x000C8664 + i: bytes([ch]),
            0x001080C0 + i * 4: le32(D1D_PREC2), 0x001080C0 + ch * 4: le32(D1D_PREC),
            0x000C85D8 + ch * 2: le16(flash), 0x000C85E6 + ch * 2: le16(reveal),
            0x000C8648 + ch * 4: le32(0x00AB4800 + ch), 0x000C8610 + col * 4: le32(0x00AB1000 + col),
            D1D_PREC + 0x28: bytes([0x28, s29, 0x2A]), D1D_PREC2 + 0x28: bytes([0x28, s29, 0x2A])}


D1D_STR_BASE, D1D_STR_HANDLE, D1D_STR_OUT = 0x0010B000, 0x0010AA00, 0x00102760
D1D_STR_ALLOW = (0x1C500, 0x474E4, 0x1E75C, 0x1E808, 0x500BB)     # 0x1C500 and its tree, run real (C3g)
D1D_CURSOR = E.Call(0x2F198, ("eax", "edx", "[ebx]", "[ebx+4]", "[ebx+8]", "[ebx+12]", "ecx"),
                    mode="stub", clobbers=("ebx", "ecx", "edx"))
D1D_RELEASE = E.Call(0x2F280, ("eax", "edx", "ebx", "ecx"), mode="stub", clobbers=("ebx", "ecx", "edx"))


def d1d_str_pokes(strings):
    """The localisation table 0x1C500 reads (c3f_str_case's layout): id = group * 0x40 + entry,
    group g > 0 an 8-byte block whose +4 links the next, an entry a length byte and its bytes XORed
    with it. `strings` maps ids to plaintext; every other entry up to the highest id of a used
    group is empty. The 0x102760 out buffer is seeded 0x5A."""
    top = max(strings) >> 6
    groups = [[] for _ in range(top + 1)]
    for sid in sorted(strings):
        g = groups[sid >> 6]
        g.extend([b""] * ((sid & 0x3F) - len(g)))
        g.append(strings[sid])
    buf, offs = bytearray(8), []
    for gi, g in enumerate(groups):
        if gi > 0:
            offs.append(len(buf))
            buf.extend(b"\x00" * 8)
        for e in g:
            buf.append(len(e))
            buf.extend(bytes(c ^ len(e) for c in e))
    if offs:
        buf[4:8] = le32(offs[0])
    for i in range(len(offs) - 1):
        buf[offs[i] + 4:offs[i] + 8] = le32(offs[i + 1])
    assert D1D_STR_BASE + len(buf) <= 0x0010B0D0, len(buf)
    p = {0x001082DC: le32(D1D_STR_HANDLE),
         D1D_STR_HANDLE: b"\x00" * 8 + le32(D1D_STR_BASE) + le32(0x400) + b"\x00" * 15,
         D1D_STR_OUT: bytes([0x5A]) * 0x40}
    p.update(d1d_chunks(D1D_STR_BASE, buf))
    return p


def d1d_advance_case(flag, r, field, x=0x2A00):
    """0x4160C: the sprite flag 0x104529 (its neighbour 0x104528 seeded), the prompt word 0xC8718,
    strings 0xDC/0xDD, the result 0x104AD4 = r and both sides' +0x82 bytes (0x107832 + side *
    0x94; the other side's must survive), and every byte the function writes seeded with its
    neighbours: 0x108105..0x10810D, 0x108110..0x108112, 0x104B22..0x104B25, 0x104AFE."""
    p = d1d_str_pokes({0xDC: b"PRESS-START-DC-0", 0xDD: b"ERASE-THESE-DD-1"})
    p.update({0x00104528: bytes([0x5C, flag]), 0x000C8718: le16(x), 0x00104AD4: le32(r),
              0x00107831 + r * 0x94: bytes([0x31, field, 0x33]),
              0x00107831 + (r ^ 1) * 0x94: b"\x31\x10\x33",
              0x00108105: bytes(range(0x71, 0x7A)), 0x00108110: b"\x76\x77\x78",
              0x00104B22: b"\x92\x93\x94\x95",
              0x00104AFC: le16(0xFCFC) + le16(0xFFFF) + le16(0xB0B0)})
    return p


def d1d_gameover_case(flag, x=0x2A00):
    """0x417C4: the sprite flag, the prompt word, strings 0x1E/0x1F/0x20 and the four stores'
    targets seeded with their neighbours (the hook 0x104AE4, the words 0x1088EE, 0x104AFE,
    0x104B00)."""
    p = d1d_str_pokes({0x1E: b"GAME-OVER-LINE-1", 0x1F: b"NO-CONTINUES-L-2", 0x20: b"THIRD-ROW-LINE-3"})
    p.update({0x00104528: bytes([0x5C, flag]), 0x000C8718: le16(x),
              0x00104AE0: le32(0xE0E0E0E0) + le32(0xE4E4E4E4) + le32(0xE8E8E8E8),
              0x001088EC: le16(0xECEC) + le16(0xEEEE) + le16(0xF0F0),
              0x00104AFC: le16(0xFCFC) + le16(0xFFFF) + le16(0xB0B0) + le16(0xB2B2)})
    return p


def d1d_latch_case(c0, c4, c8, cc):
    """0x46504/0x4651C/0x46594: the four dwords 0x1082C0..0x1082CC, with 0x1082BC and 0x1082D0
    (the neighbours) seeded."""
    return {0x001082BC: le32(0xBCBCBCBC) + le32(c0) + le32(c4) + le32(c8) + le32(cc) + le32(0xD0D0D0D0)}


def d1d_init_case(flag, b, v93f8, v9388, alt=0x11):
    """0x46594: the gate byte 0x108173, the difficulty byte 0x10452C = b (0x10452D seeded
    b ^ 0x07), the bytes 0xC93F8[b] and 0xC9388[7b]; 0xC9388[8b] and 0xC9388[b] hold `alt`."""
    p = d1d_latch_case(0xC0C0C0C0, 0xC4C4C4C4, 0xC8C8C8C8, 0xCCCCCCCC)
    p.update({0x00108172: bytes([0x72, flag, 0x74]), 0x0010452C: bytes([b, b ^ 0x07]),
              0x000C93F8 + b: bytes([v93f8]), 0x001082D4: le32(0xD4D4D4D4)})
    if b:
        p[0x000C9388 + b * 8] = bytes([alt])
        p[0x000C9388 + b] = bytes([alt ^ 0xFF])
    p[0x000C9388 + b * 7] = bytes([v9388])
    return p


def d1d_column_case(col, b, count, v):
    """0x4660C: the continue count 0x104B11 (neighbours seeded), the difficulty byte 0x10452C, the
    byte 0xC9388[7b + col] = v (0xC9388[8b + col] and 0xC9388[7b] hold other values), 0x1082D0 and
    the dword after it seeded."""
    p = {0x00104B10: bytes([0x10, count, 0x12]), 0x0010452C: bytes([b]),
         0x001082D0: le32(0xD0D0D0D0) + le32(0xD4D4D4D4)}
    if b:
        p[0x000C9388 + b * 8 + col] = bytes([v ^ 0x55])
    if col:
        p[0x000C9388 + b * 7] = bytes([v ^ 0xAA])
    p[0x000C9388 + b * 7 + col] = bytes([v])
    return p


D1D_ORIGIN = {0x00107A36: le16(0x3636) + le16(0xFFFF) + le16(0xFFFF) + le16(0x3C3C)}
D1D_WIPE = {0x001088F4: b"\xF4\x7F\xF6", 0x00104AFA: le16(0xFFFF) + le16(0xCCCC),
            0x00104B00: le16(0x1111) + le16(0x2222)}


def d1d_surface_case(a, b):
    """0x51F45: the two buffer dwords 0x1014E4/0x1014E8, the two it binds (0xE87A0/0xE87A4, their
    neighbours seeded) and the 200-dword table 0x1088F8 seeded 0xA5 with a dword on each side."""
    p = {0x001014E4: le32(a) + le32(b),
         0x000E879C: le32(0x9C9C9C9C) + le32(0xA0A0A0A0) + le32(0xA4A4A4A4) + le32(0xA8A8A8A8)}
    p.update(d1d_chunks(0x001088F4, b"\xA5" * (4 + 800 + 4)))
    return p


def d1d_buttons_case(held, masks=None):
    """0x4F778: the pressed bits 0x1088E4 (0x1088D8, the held bits, seeded with their complement);
    the two masks 0xC9898 are the image's (0x0F000000, 0x00000F00) unless `masks` pokes them."""
    p = {0x001088E4: le32(held), 0x001088D8: le32(held ^ 0xFFFFFFFF)}
    if masks:
        p[0x000C9898] = le32(masks[0]) + le32(masks[1])
    return p


def d1d_match_end_case(r, s63, f2, other=1):
    """0x4F728: the result 0x104AD4 = r, that side's +0x63 byte (0x107813 + r * 0x94) = s63 with
    its +0x62 neighbour and the other side's +0x63 = `other`, the signed byte 0x1088F2 = f2. For
    r = -1 (never indexed by the raw) the byte one slot below the table is seeded 0."""
    p = {0x00104AD4: le32(r), 0x001088F1: bytes([0xF1, f2, 0xF3])}
    if r == 0xFFFFFFFF:
        p[0x00107813 - 0x94] = b"\x00"
        return p
    p[0x00107812 + r * 0x94] = bytes([s63 ^ 1, s63])
    p[0x00107812 + (r ^ 1) * 0x94] = bytes([other ^ 1, other])
    return p


D1D_HOOK_NOP = E.Call(0x5D812, (), mode="real")     # `xor eax,eax; ret`: a DS_00104AE4 value the port does not register


def d1d_hook_case(gate, hook):
    """0x4F6E8/0x4F704: the gate byte 0x9AF3D between two 0xFF neighbours, the hook dword
    0x104AE4, and the mode words 0x104AFA/0x104AFE/0x104B00 seeded distinct."""
    return {0x0009AF3C: bytes([0xFF, gate, 0xFF]), 0x00104AE4: le32(hook),
            0x00104AFA: le16(0x2222) + le16(0x3333) + le16(0x4444) + le16(0x1111) + le16(0xB2B2),
            0x001088F5: b"\x7F"}


D1D_S0, D1D_S1, D1D_SPAWNED = 0x0010A300, 0x0010A400, 0x0010A500   # the two slot records, the stub's record


def d1d_fighters_case(r, c0, c1):
    """0x42724: the characters 0x10782A/0x1078BE, the result 0x104AD4, the x words the arms read
    (the high word of the dword at T + c * 2, each a distinct signed value; 0xC834A seeded for
    both characters), the two slot records with different +0x56 words and seeded +4 children, the
    record the 0x2AE14 stub returns (its +0x56 word and +0x58..+0x5A) and the two latches. The
    descriptor tables 0xBB830/0xBB880/0xBB8D0 are the image's (distinct per index)."""
    assert c0 != c1 and abs(c0 - c1) > 1 and max(c0, c1) < 7
    p = {0x0010782A: bytes([c0]), 0x001078BE: bytes([c1]), 0x00104AD4: le32(r),
         0x000C8312 + c0 * 2 + 2: le16(0x8112), 0x000C8320 + c1 * 2 + 2: le16(0x0120),
         0x000C832E + c0 * 2 + 2: le16(0x012E), 0x000C833C + c1 * 2 + 2: le16(0xF13C),
         0x000C834A + c0 * 2 + 2: le16(0x014A), 0x000C834A + c1 * 2 + 2: le16(0x824A),
         0x001077B0: le32(D1D_S0) + le32(0x04040404), 0x00107844: le32(D1D_S1) + le32(0x48484848),
         D1D_S0 + 0x56: le16(0x0012) + bytes([0x58, 0x59, 0x5A]),
         D1D_S1 + 0x56: le16(0xF8FF) + bytes([0x58, 0x59, 0x5A]),
         D1D_SPAWNED + 0x56: le16(0xFB01) + bytes([0x58, 0x59, 0x5A]),
         0x001080E8: le32(0xE8E8E8E8) + le32(0xECECECEC) + le32(0xF0F0F0F0) + le32(0xF4F4F4F4)}
    return p


def d1d_drop_rec(rec, s5b, w36, d1c):
    return {rec + 0x18: le32(0x18181818) + le32(d1c) + le32(0x20202020),
            rec + 0x34: le16(0x1234) + le16(w36) + le16(0x3838),
            rec + 0x5A: bytes([0x5A, s5b, 0x5C])}


def d1d_drop_case(rec0, rec1, b25=0x7F):
    """0x42BCC: the two record pointers 0x1080B4/0x1080B8 (the dword after them seeded), each
    record's +0x5B (with neighbours), +0x34 dword (low word 0x1234, high word = the +0x36 word)
    and +0x1C dword (with neighbours), and the state byte 0x104B25 with its neighbours."""
    p = {0x001080B4: le32(D1D_S0) + le32(D1D_S1) + le32(0xBCBCBCBC),
         0x00104B24: bytes([0x24, b25, 0x26])}
    p.update(d1d_drop_rec(D1D_S0, *rec0))
    p.update(d1d_drop_rec(D1D_S1, *rec1))
    return p


D1D_SPECS = [
    # 0x33904 frontend_list_next: EAX = node (0 = the head). Full mask: all 15 call sites read EAX
    # whole (`mov ebx,eax`/`mov ecx,eax`/`test eax,eax`, 0x12030 .. 0x425B2).
    Spec("frontend_list_next", 0x33904, [
        Case("n0", {"eax": 0}, d1d_list_case({})),
        Case("n1", {"eax": 0}, d1d_list_case({0x00107608: 0x55555555, 0x00107618: 1})),
        Case("n2", {"eax": 0}, d1d_list_case({0x00107788: 0x00000100})),
        Case("n3", {"eax": 0x00107618}, d1d_list_case({0x00107618: 7, 0x00107648: 0x00010000})),
        Case("n4", {"eax": 0x00107788}, d1d_list_case({0x00107788: 9})),
        Case("n5", {"eax": 0x00107708}, d1d_list_case({0x00107718: 0x80000000})),
        Case("n6", {"eax": 0x00107778}, d1d_list_case({})),
    ], eax_mask=0xFFFFFFFF,
       mutants=("@head", "@bound", "@gt", "@off", "@width", "@stride", "@zero")),
    # 0x389C4 render_scroll_edge: no argument. The gate is the signed word 0x107A3C (`jle`); the
    # > 0 arm's x = zx(0x107A48) - (dword 0x107A3A >> 16), /64 truncating; `ja` is unsigned.
    Spec("render_scroll_edge", 0x389C4, [
        Case("e0", {}, d1d_edge_case(0x0000, 0x1234, 0x0005, 0x9999, 0x5555)),
        Case("e1", {}, d1d_edge_case(0x8000, 0xFFC0, 0x0005, 0x9999, 0x5555)),
        Case("e2", {}, d1d_edge_case(0x0001, 0x0100, 0x0005, 0x0009, 0x5555)),
        Case("e3", {}, d1d_edge_case(0x0100, 0x003F, 0x0009, 0x0005, 0x5555)),
        Case("e4", {}, d1d_edge_case(0x0001, 0xFFFF, 0x8000, 0x0001, 0x5555)),
        Case("e5", {}, d1d_edge_case(0x0002, 0x0050, 0x0001, 0x8000, 0x5555)),
    ], eax_mask=0,
       mutants=("@sign", "@floor", "@cmp", "@zext", "@src", "@hi", "@store")),
    # 0x38A38 render_scroll_fill: no argument (EDI's upper half at entry is the caller's: `mov
    # di,[0x107a4c]` at 0x38A6C keeps it, and only DI is read at 0x38A91; f1/f5 seed it).
    Spec("render_scroll_fill", 0x38A38, [
        Case("f0", {}, d1d_fill_case(0x00001234, 3, 0, 0x0000, 2, 0x0100)),
        Case("f1", {"edi": 0xABCD0000}, d1d_fill_case(0x00001234, 3, 3, 0x0000, 2, 0x0100)),
        Case("f2", {}, d1d_fill_case(0x00001234, 5, 2, 0x00EF, 1, 0x0040)),
        Case("f3", {}, d1d_fill_case(0x00001234, 5, 1, 0x00F0, 1, 0x0040)),
        Case("f4", {}, d1d_fill_case(0xFFFF8765, 7, 4, 0x0000, 3, 0x0021)),
        Case("f5", {"edi": 0x7FFF0000}, d1d_fill_case(0x00004321, 9, 2, 0xFFFF, 1, 0x0040)),
        Case("f6", {}, d1d_fill_case(0x00000321, 0x8001, 2, 0x0010, 0xFFFF, 0xFFFF)),
        Case("f7", {}, d1d_fill_case(0xFFFF8765, 7, 1, 0x0000, 3, 0x0021)),
    ], eax_mask=0,
       mutants=("@div", "@floor256", "@half", "@idx", "@edge", "@edge16", "@floor32", "@step",
                "@a42", "@a46", "@a50", "@last", "@tail256", "@tail32")),
    # 0x38B18 frontend_spawn_row: EAX = desc, EDX = a2, EBX = a3; 0x2AE14 is a stub whose EAX (the
    # record) varies per case. p3 is the full table: EDI stays -1 (0x38B1F) and the store lands on
    # 0x107A18 (0x38B62).
    Spec("frontend_spawn_row", 0x38B18, [
        Case("p0", {"eax": 0x000C87BC, "edx": 0, "ebx": 0}, d1d_row_case(()), {0x2AE14: 0x0010A300}),
        Case("p1", {"eax": 0x000C87BC, "edx": 0x2A, "ebx": 0}, d1d_row_case((0,)), {0x2AE14: 0x0010A400}),
        Case("p2", {"eax": 0x0009AC1C, "edx": 0x20000001, "ebx": 0x1E}, d1d_row_case(range(6)),
             {0x2AE14: 0x0010A468}),
        Case("p3", {"eax": 0x0009AC1C, "edx": 0x2A, "ebx": 0x1E}, d1d_row_case(range(7)),
             {0x2AE14: 0x0010A500}),
        Case("p4", {"eax": 0x0009AD84, "edx": 5, "ebx": 0x80000003}, d1d_row_case((0, 1, 3)),
             {0x2AE14: 0x0010A568}),
        Case("p5", {"eax": 0x0009AD84, "edx": 1, "ebx": 2}, d1d_row_case(range(1, 7)),
             {0x2AE14: 0x0010A600}),
    ], calls=(SPAWN,), eax_mask=0,
       mutants=("@a2", "@a3", "@layer", "@swap", "@bound", "@full", "@first", "@flags", "@store")),
    # 0x414B4 frontend_portrait_flash: EAX = the portrait slot. 0x2BCF4 and 0x2A17C are stubs.
    Spec("frontend_portrait_flash", 0x414B4, [
        Case("a0", {"eax": 0}, d1d_portrait_case(0, 0x03, 2, 0x21)),
        Case("a1", {"eax": 1}, d1d_portrait_case(1, 0x45, 0, 0xF7, flash=0x8123)),
        Case("a2", {"eax": 6}, d1d_portrait_case(6, 0x83, 5, 0x00, flash=0xFFFF)),
        Case("a3", {"eax": 3}, d1d_portrait_case(3, 0x16, 0x80, 0x08)),
        Case("a4", {"eax": 2}, d1d_portrait_case(2, 0x46, 4, 0x10)),
    ], calls=(P6_2BCF4, PALETTE), eax_mask=0,
       mutants=("@cls", "@bit", "@plus", "@char", "@sext", "@table", "@or", "@order", "@early",
                "@word", "@flagidx", "@stream")),
    # 0x41528 frontend_portrait_reveal: EAX = the portrait slot; no class split.
    Spec("frontend_portrait_reveal", 0x41528, [
        Case("b0", {"eax": 0}, d1d_portrait_case(0, 0x03, 3, 0x21)),
        Case("b1", {"eax": 4}, d1d_portrait_case(4, 0x45, 0x80, 0xF7, reveal=0x8001)),
        Case("b2", {"eax": 6}, d1d_portrait_case(6, 0x83, 1, 0x08)),
    ], calls=(P6_2BCF4, PALETTE), eax_mask=0,
       mutants=("@char", "@sext", "@table", "@or", "@order", "@early", "@word", "@stream")),
    # 0x4160C frontend_mode12_advance: no argument. Sprite arm: 0x2AE14 (stub). Text arm: 0x1C500
    # runs real (the decoded string is in the memory at each text call), 0x2F280/0x2F198 are stubs.
    # 0x65490 (the 7-byte fill) and its 0x654C7 run real.
    Spec("frontend_mode12_advance", 0x4160C, [
        Case("m0", {}, d1d_advance_case(0x02, 0, 0x41)),
        Case("m1", {}, d1d_advance_case(0xFD, 1, 0xFF)),
        Case("m2", {}, d1d_advance_case(0xFF, 1, 0x7F, x=0x8123)),
        Case("m3", {}, d1d_advance_case(0x01, 0, 0x00)),
    ], allow_calls=D1D_STR_ALLOW + (0x65490, 0x654C7), calls=(SPAWN, D1D_RELEASE, D1D_CURSOR), eax_mask=0,
       mutants=("@bit", "@desc", "@x", "@layer", "@z", "@order", "@id", "@mode", "@row", "@fill",
                "@b111", "@side", "@b23", "@afe", "@inc", "@b25")),
    # 0x417C4 flow_no_continue_screen: no argument. The same two arms (three text rows), then the
    # hook 0x26978, mode 0x17 and the 0xB4 countdown.
    Spec("flow_no_continue_screen", 0x417C4, [
        Case("o0", {}, d1d_gameover_case(0x02)),
        Case("o1", {}, d1d_gameover_case(0xFD)),
        Case("o2", {}, d1d_gameover_case(0xFF, x=0x8123)),
        Case("o3", {}, d1d_gameover_case(0x01)),
    ], allow_calls=D1D_STR_ALLOW, calls=(SPAWN, D1D_CURSOR), eax_mask=0,
       mutants=("@bit", "@desc", "@x", "@z", "@ids", "@third", "@mode", "@row", "@hook", "@ee",
                "@b00", "@afe")),
    # 0x42724 flow_challenge_fighters: no argument. Every 0x2AE14 and both 0x29BC8 are stubs; the
    # 0x2AE14 stub's record is the same for a case's every spawn, so t3/t4 (no fighter spawn, the two
    # slot records kept) are what separate slot 0 from slot 1 in the 0x29BC8 calls and the a5 words.
    Spec("flow_challenge_fighters", 0x42724, [
        Case("t0", {}, d1d_fighters_case(0, 2, 5), {0x2AE14: D1D_SPAWNED}),
        Case("t1", {}, d1d_fighters_case(1, 2, 5), {0x2AE14: D1D_SPAWNED}),
        Case("t2", {}, d1d_fighters_case(2, 4, 1), {0x2AE14: D1D_SPAWNED}),
        Case("t3", {}, d1d_fighters_case(3, 6, 0), {0x2AE14: D1D_SPAWNED}),
        Case("t4", {}, d1d_fighters_case(0xFFFFFFFF, 0, 3), {0x2AE14: D1D_SPAWNED}),
        Case("t5", {}, d1d_fighters_case(1, 5, 2), {0x2AE14: D1D_SPAWNED}),
    ], calls=(SPAWN, P6_29BC8), eax_mask=0,
       mutants=("@c1a3", "@childc", "@or", "@fe", "@order", "@latch", "@shr", "@r0", "@child4",
                "@kept", "@r1", "@side")),
    # 0x42BCC flow_challenge_drop: no argument. A record is (+0x5B, the +0x36 word, the +0x1C dword).
    Spec("flow_challenge_drop", 0x42BCC, [
        Case("d0", {}, d1d_drop_case((1, 0x0010, 5), (1, 0x0020, 6))),
        Case("d1", {}, d1d_drop_case((0, 0x0000, 0xFFFFFF00), (0, 0x00C0, 0xFFFFFF00))),
        Case("d2", {}, d1d_drop_case((0x80, 0x0010, 5), (0, 0x0080, 0))),
        Case("d3", {}, d1d_drop_case((0, 0x0083, 0), (1, 0x0020, 6))),
        Case("d4", {}, d1d_drop_case((0, 0xFFC0, 5), (0, 0xFF01, 0x00000200))),
        Case("d5", {}, d1d_drop_case((0, 0x7FF0, 0x00010000), (0, 0x0080, 0xFFFFFF40))),
        Case("d6", {}, d1d_drop_case((1, 0x0010, 5), (0xFF, 0x0020, 6), b25=0xFF)),
        Case("d7", {}, d1d_drop_case((0, 0x0040, 0x80000000), (0, 0x0000, 0xFFFFFFC1))),
    ], eax_mask=0,
       unhit_named={0x42C5D: "dead: 0x42C51 `jge` repeats 0x42C4F's `jge 0x42c7e` on the same flags "
                             "(`test si,si` at 0x42C4C, nothing between), so it is never taken"},
       mutants=("@both", "@b25", "@add", "@nosum", "@clr1c", "@div", "@floor", "@noneg", "@thr",
                "@zero36", "@v", "@skip", "@one", "@done")),
    # 0x46504 flow_1082c8_latch / 0x4651C flow_1082c8_restore: no argument, one block each.
    Spec("flow_1082c8_latch", 0x46504, [
        Case("l0", {}, d1d_latch_case(0xAAAAAAAA, 0xBBBBBBBB, 0x11223344, 0x55667788)),
        Case("l1", {}, d1d_latch_case(0x00000001, 0x00000002, 0x00000000, 0xFFFFFFFF)),
    ], eax_mask=0, mutants=("@rev", "@swap", "@one", "@width")),
    Spec("flow_1082c8_restore", 0x4651C, [
        Case("u0", {}, d1d_latch_case(0x11223344, 0x55667788, 0xAAAAAAAA, 0xBBBBBBBB)),
        Case("u1", {}, d1d_latch_case(0x00000000, 0xFFFFFFFF, 0x00000001, 0x00000002)),
    ], eax_mask=0, mutants=("@rev", "@swap", "@one", "@width")),
    # 0x46594 flow_1082c8_init: no argument. The gate byte 0x108173 selects 7/4 or the byte
    # 0xC93F8[b]; then 0x1082D0 = the byte 0xC9388[7b] and the latch.
    Spec("flow_1082c8_init", 0x46594, [
        Case("i0", {}, d1d_init_case(0x01, 0, 0x33, 0x44)),
        Case("i1", {}, d1d_init_case(0x80, 3, 0x33, 0x90)),
        Case("i2", {}, d1d_init_case(0x00, 0, 0x05, 0x09)),
        Case("i3", {}, d1d_init_case(0x00, 3, 0x85, 0xF0)),
        Case("i4", {}, d1d_init_case(0x00, 0xFF, 0x7F, 0x01)),
    ], eax_mask=0, mutants=("@seven", "@tbl", "@stride", "@sext", "@d0", "@latch", "@idx")),
    # 0x4660C flow_1082d0_from_column: EAX = the column.
    Spec("flow_1082d0_from_column", 0x4660C, [
        Case("k0", {"eax": 0}, d1d_column_case(0, 0, 0, 0x09)),
        Case("k1", {"eax": 4}, d1d_column_case(4, 2, 1, 0x80)),
        Case("k2", {"eax": 3}, d1d_column_case(3, 1, 2, 0x01)),
        Case("k3", {"eax": 6}, d1d_column_case(6, 5, 5, 0x02)),
        Case("k4", {"eax": 1}, d1d_column_case(1, 3, 0xFF, 0xFF)),
        Case("k5", {"eax": 2}, d1d_column_case(2, 4, 3, 0x90)),
        Case("k6", {"eax": 5}, d1d_column_case(5, 0, 2, 0x00)),
    ], eax_mask=0, mutants=("@gt", "@minus", "@clamp", "@sext", "@stride", "@col", "@width")),
    # 0x4F1D0 frontend_origin_zero: the two words; EAX returns unchanged (full mask, pass-through).
    Spec("frontend_origin_zero", 0x4F1D0, [
        Case("z0", {"eax": 0x12345678}, D1D_ORIGIN),
        Case("z1", {"eax": 0}, D1D_ORIGIN),
    ], eax_mask=0xFFFFFFFF, mutants=("@a", "@b", "@width", "@eax")),
    # 0x4F6E8 frontend_mode_18_step: gated on the byte 0x9AF3D; the hook is the dword 0x104AE4.
    # h2's hook is 0x5D812 (recorded on both sides: the port's fn_resolve reports the unregistered
    # target through the seam); h3's is the registered 0x28D68, run real with its 0x4F980, whose
    # store to 0x104AFA the raw reads back after the call.
    Spec("frontend_mode_18_step", 0x4F6E8, [
        Case("h0", {}, d1d_hook_case(0x01, 0x5D812)),
        Case("h1", {}, d1d_hook_case(0x80, 0x28D68)),
        Case("h2", {}, d1d_hook_case(0x00, 0x5D812)),
        Case("h3", {}, d1d_hook_case(0x00, 0x28D68)),
    ], allow_calls=(0x28D68, 0x4F980), calls=(D1D_HOOK_NOP,), eax_mask=0,
       mutants=("@gate", "@sign", "@width", "@nohook", "@early", "@nostore", "@src")),
    # 0x4F704 frontend_mode_19_step: the same gate and hook call, no mode store.
    Spec("frontend_mode_19_step", 0x4F704, [
        Case("j0", {}, d1d_hook_case(0x01, 0x5D812)),
        Case("j1", {}, d1d_hook_case(0x80, 0x28D68)),
        Case("j2", {}, d1d_hook_case(0x00, 0x5D812)),
        Case("j3", {}, d1d_hook_case(0x00, 0x28D68)),
    ], allow_calls=(0x28D68, 0x4F980), calls=(D1D_HOOK_NOP,), eax_mask=0,
       mutants=("@gate", "@sign", "@width", "@nohook", "@store")),
    # 0x4F714 sound_voice_stage: EAX = the stage; the id word 0xC9888[stage], zero-extended, into
    # 0x2C3FC by a tail jump (a stub: its EAX is the row's).
    Spec("sound_voice_stage", 0x4F714, [
        Case("s0", {"eax": 0}, {}, {0x2C3FC: 1}),
        Case("s1", {"eax": 1}, {}, {0x2C3FC: 0}),
        Case("s2", {"eax": 7}, {}, {0x2C3FC: 0x100}),
        Case("s3", {"eax": 3}, {0x000C9888 + 6: le16(0x8123)}, {0x2C3FC: 0xABCD0001}),
        Case("s4", {"eax": 5}, {0x000C9888 + 10: le16(0x0100)}, {0x2C3FC: 1}),
    ], calls=(VOICE,), eax_mask=0xFFFFFFFF, mutants=("@stride", "@byte", "@sext", "@ret", "@id")),
    # 0x4F728 sound_voice_match_end: no argument; two voices (0xDF or 0x23, then 0x22).
    Spec("sound_voice_match_end", 0x4F728, [
        Case("v0", {}, d1d_match_end_case(0, 0, 0x01)),
        Case("v1", {}, d1d_match_end_case(1, 0, 0x7F), {0x2C3FC: 0}),
        Case("v2", {}, d1d_match_end_case(0, 1, 0x01, other=0)),
        Case("v3", {}, d1d_match_end_case(0, 0, 0x00)),
        Case("v4", {}, d1d_match_end_case(1, 0, 0x80)),
        Case("v5", {}, d1d_match_end_case(3, 0, 0x01)),
        Case("v6", {}, d1d_match_end_case(0xFFFFFFFF, 0, 0x01)),
        Case("v7", {}, d1d_match_end_case(2, 0, 0x02)),
        Case("v8", {}, d1d_match_end_case(1, 0x80, 0x01, other=0)),
    ], calls=(VOICE,), eax_mask=0,
       mutants=("@m1", "@r3", "@s63", "@side", "@sign", "@ge", "@ids", "@one", "@order", "@second")),
    # 0x4F778 frontend_buttons_pressed: EAX = n. AL only (mask 0xFF): b1/b5 leave AL = 0 under
    # non-zero upper bytes of EAX.
    Spec("frontend_buttons_pressed", 0x4F778, [
        Case("b0", {"eax": 0}, d1d_buttons_case(0x01000000)),
        Case("b1", {"eax": 0}, d1d_buttons_case(0x00000100)),
        Case("b2", {"eax": 1}, d1d_buttons_case(0x00000800)),
        Case("b3", {"eax": 1}, d1d_buttons_case(0x08000000)),
        Case("b4", {"eax": 0}, d1d_buttons_case(0x00000000)),
        Case("b5", {"eax": 1}, d1d_buttons_case(0xFFFFF0FF)),
        Case("b6", {"eax": 1}, d1d_buttons_case(0x80000000, masks=(0x00000001, 0x80000001))),
        Case("b7", {"eax": 0}, d1d_buttons_case(0x00000004, masks=(0x00000F0F, 0x000000F0))),
    ], eax_mask=0xFF, mutants=("@stride", "@other", "@src", "@all", "@low")),
    # 0x4F980 frontend_wipe_arm: EAX = the return mode (AX stored); EAX returns unchanged.
    Spec("frontend_wipe_arm", 0x4F980, [
        Case("w0", {"eax": 0x00000010}, D1D_WIPE),
        Case("w1", {"eax": 0xABCD1234}, D1D_WIPE),
    ], eax_mask=0xFFFFFFFF, mutants=("@f5", "@mode", "@width", "@byte", "@b00", "@eax")),
    # 0x50188 swap_buffers: the two dwords swap; EAX returns unchanged (push/pop).
    Spec("swap_buffers", 0x50188, [
        Case("x0", {"eax": 0x12345678}, {0x000E879C: le32(0x9C9C9C9C) + le32(0x11223344)
                                         + le32(0x55667788) + le32(0xA8A8A8A8)}),
        Case("x1", {"eax": 0}, {0x000E879C: le32(0x9C9C9C9C) + le32(0x00010000)
                                + le32(0x00020000) + le32(0xA8A8A8A8)}),
    ], eax_mask=0xFFFFFFFF, mutants=("@a", "@b", "@width", "@eax")),
    # 0x51F45 surface_setup: the two buffer dwords and the 200-entry pitch table; pushal/popal.
    Spec("surface_setup", 0x51F45, [
        Case("y0", {"eax": 0x12345678}, d1d_surface_case(0x00200000, 0x00300000)),
        Case("y1", {"eax": 0}, d1d_surface_case(0x00300000, 0x00200000)),
    ], eax_mask=0xFFFFFFFF, mutants=("@swap", "@a4", "@count", "@pitch", "@start", "@eax")),
]
D1_SPECS += D1D_SPECS

# ---- track D1 batch d1g (record 2026-10-11-reverse-d1g): the game/actors.c queue ----------------
# Every poke and every address a binding or mutant derives from one stays inside the image.

D1G_PSET, D1G_REC = 0x0010A200, 0x0010A468          # zero BSS: a pset array and an actor record
D1G_SLOT0, D1G_SLOT1 = 0x0010A600, 0x0010A700       # two slot structures (their +0 = a record)
D1G_R0, D1G_R1 = 0x0010A800, 0x0010A900             # the two slots' records


def d1g_psw_case(w28, idx=1, base=D1G_PSET, rec=D1G_REC):
    """0x10D70: rec+0x26..+0x2B (the +0x28 word between sentinels), the +0x54/+0x56 words, both
    pool bases (0x1014F4 0x400 above the pset base) and the pset slot's first eight bytes."""
    return {0x001014EC: le32(base), 0x001014F4: le32(base + 0x400),
            rec + 0x26: b"\x26\x27" + le16(w28) + b"\x2a\x2b",
            rec + 0x54: le16(3) + le16(idx),
            base + idx * 0x20: b"\x5a\x5b\x5c\x5d\x5e\x5f\x60\x61"}


def d1g_lnk_case(k, pool, k2=1, rec=D1G_REC):
    """0x2BD20: rec+0x4A = k (rec+0x4B = k2, another index), both pool bases, and the named
    record's +0x4A..+0x4C seeded with distinct bytes."""
    return {0x001014F4: le32(pool), 0x001014EC: le32(pool + 0x10),
            rec + 0x49: bytes([0x49, k, k2, 0x4C]),
            pool + k * 0x68 + 0x4A: b"\xa4\xb4\xc4"}


def d1g_o53_case(side, ptrs, extra=None, rec=D1G_REC):
    """0x37B54: rec+0x51 = side (rec+0x52 = side ^ 1, rec+0x50 a sentinel), the four dwords at
    0x1077A8 = `ptrs`, the two slots' record pointers and +0x52..+0x54 of slots and records."""
    p = {rec + 0x50: bytes([0x50, side, side ^ 1, 0x53]),
         0x001077A8: b"".join(le32(v) for v in ptrs),
         D1G_SLOT0: le32(D1G_R0), D1G_SLOT1: le32(D1G_R1)}
    for i, a in enumerate((D1G_SLOT0, D1G_SLOT1, D1G_R0, D1G_R1)):
        p[a + 0x52] = bytes([0x52 + 0x10 * i, 0x53 + 0x10 * i, 0x54 + 0x10 * i])
    p.update(extra or {})
    return p


def d1g_cur_case(fill):
    """0x38B70: the three cursors between two sentinel dwords, and 0x1079FC..0x107A3B (a dword
    below 0x107A00, the two 7-dword arrays, a dword above 0x107A1C + 28) all non-zero."""
    return {0x000BDFB8: bytes((fill + i) & 0xFF or 1 for i in range(20)),
            0x001079FC: bytes((fill + 0x40 + i) & 0xFF or 1 for i in range(0x40))}


def d1g_lists_case(inuse, free, first, end, seed, extra=None):
    """0x28E98/0x48C4C: every node byte of [first, end) non-zero (so the nodes' +8..+0xF must
    survive), and the two sentinels' {next; prev} pointing into the scratch BSS (a mutant that
    skips a self-link then follows them inside the image)."""
    p = {}
    data = bytes(((seed + i) & 0x7F) | 0x80 for i in range(end - first))
    for off in range(0, len(data), 0x40):
        p[first + off] = data[off:off + 0x40]
    p[inuse] = le32(0x0010A030) + le32(0x0010A010)
    p[free] = le32(0x0010A040) + le32(0x0010A020)
    p.update(extra or {})
    return p


D1G_POOL, D1G_PREC = 0x0010A000, 0x0010A068         # the actor pool and its record 1 (in_pool)


def d1g_m1_case(w28, pset4=0x5000, pset8=0x100, y30=0, f0aec=0, f0af0=0x1000, thr=0xEA, ramp=(),
                s64=2, rec=D1G_PREC):
    """0x2BE5C: a pool record (pset index 1, every byte seeded), its pset, the two camera
    globals, 0x2A620's threshold word 0x107A4C and the 0x107900 ramp entries in `ramp`."""
    buf = bytearray(0x80 | (i & 0x7F) for i in range(0x68))
    def w32(o, v): buf[o:o + 4] = le32(v)
    def w16(o, v): buf[o:o + 2] = le16(v)
    w32(0x18, 0x18181818)
    w32(0x1C, 0x1C1C1C1C)
    w16(0x28, w28)
    w32(0x30, y30)
    w32(0x44, 0x46460044)                # the +0x46 word the ramp entry replaces
    w16(0x56, 1)
    w32(0x61, 0x00616263)
    buf[0x64] = s64
    ps = bytearray(0xA0 | (i & 0x1F) for i in range(0x20))
    ps[4:8] = le32(pset4)
    ps[8:12] = le32(pset8)
    p = {0x001014F4: le32(D1G_POOL), 0x001014EC: le32(D1G_PSET),
         rec: bytes(buf[:0x40]), rec + 0x40: bytes(buf[0x40:]),
         D1G_PSET + 0x20: bytes(ps),
         0x000F0AEC: le32(f0aec), 0x000F0AF0: le32(f0af0), 0x00107A4C: le16(thr)}
    for i, v in ramp:
        p[0x00107900 + i * 2] = le16(v)
    return p


def d1g_aid_case(k, pw28=0, idx=1, pool=D1G_POOL, base=D1G_PSET, rec=D1G_REC):
    """0x2A39C: rec+0x26..+0x2B (the +0x28 byte 0xFF), rec+0x4A = k, the parent pool record's
    +0x28 word between sentinels, both bases and the pset slot's first eight bytes."""
    p = {0x001014EC: le32(base), 0x001014F4: le32(pool),
         rec + 0x26: b"\x26\x27\xff\x29\x2a\x2b",
         rec + 0x49: bytes([0x49, k, 0x4B]),
         base + idx * 0x20: b"\x5a\x5b\x5c\x5d\x5e\x5f\x60\x61"}
    if k:
        p[pool + k * 0x68 + 0x26] = b"\x26\x27" + le16(pw28) + b"\x2a\x2b"
    return p


def d1g_mot_case(w2a=0x8000, x=0x1000, y=0x2000, w32=0, w34=0, w36=0, w38=0, b42=0, b43=0,
                 w44=0, w28=0, rec=D1G_REC):
    """0x2A4FC: the record's +0x18..+0x47 as one buffer (unnamed bytes are sentinels)."""
    buf = bytearray(0x80 | (i & 0x7F) for i in range(0x18, 0x48))
    def put(o, b): buf[o - 0x18:o - 0x18 + len(b)] = b
    put(0x18, le32(x)); put(0x1C, le32(y)); put(0x28, le16(w28)); put(0x2A, le16(w2a))
    put(0x32, le16(w32)); put(0x34, le16(w34)); put(0x36, le16(w36)); put(0x38, le16(w38))
    put(0x42, bytes([b42, b43])); put(0x44, le16(w44))
    return {rec + 0x18: bytes(buf)}


def d1g_deb_case(seed, ca8, mode, ca9=0, t48=0x27, sp=D1G_R1):
    """0x19958: the rng seed, the two gate bytes 0x100CA8/0x100CA9, the stage word 0x104AFC, the
    camera x 0xF0AF0 and the record the SPAWN stub returns (+0x30..+0x4B seeded)."""
    return {DS_RNG: le32(seed), 0x00100CA8: bytes([ca8, ca9]), 0x00104AFC: le16(mode),
            0x000F0AF0: le32(0x00012340),
            sp + 0x30: bytes(0x80 | i for i in range(0x30, 0x48)) + bytes([t48, 0x28, 0x4A, 0x4B])}


def d1g_prj_case(slot, w28=0, fw28=0, side=0, y30=0x00030000, rec=D1G_REC, frec=D1G_R0,
                 sp=D1G_R1):
    """0x2372C: rec+0x14 = slot and its +0x28 word; the slot's record `frec` (+0x18 x, +0x1C y,
    +0x28, +0x30, +0x51) and +8; the spawned record's +0x14, +0x2E, +0x34 and +0x4E seeded."""
    return {rec + 0x14: le32(slot), rec + 0x26: b"\x26\x27" + le16(w28) + b"\x2a\x2b",
            rec + 0x50: bytes([0x50, 0 if side else 1, 0x52]),
            D1G_SLOT0: le32(frec) + le32(0x04040404) + le32(0x08080808) + le32(0x0C0C0C0C),
            frec + 0x18: le32(0x00005000) + le32(0x00000100),
            frec + 0x26: b"\x26\x27" + le16(fw28) + b"\x2a\x2b",
            frec + 0x30: le32(y30), frec + 0x50: bytes([0x50, side, 0x52]),
            sp + 0x14: le32(0x14141414), sp + 0x2C: b"\x2c\x2d\x2e\x2f\x30\x31",
            sp + 0x32: b"\x32\x33\x34\x35\x36\x37", sp + 0x4D: b"\x4d\x4e\x4f"}


def d1g_fin_case(side, ptrs, b14, ch=3, extra=None, rec=D1G_REC):
    """0x370F0: rec+0x51 = side (+0x53 a sentinel), the four dwords at 0x1077A8, the gate byte
    0x104B14, 0xF0AFE, and both slot structures' +0x42, +0x52..+0x54 and +0x7A (slot 0 = `ch`,
    slot 1 = ch ^ 1)."""
    p = {rec + 0x50: bytes([0x50, side, 0x52, 0x53, 0x54]),
         0x001077A8: b"".join(le32(v) for v in ptrs),
         0x00104B13: bytes([0x13, b14, 0x15]), 0x000F0AFD: b"\xfd\xfe\xff"}
    for i, a in enumerate((D1G_SLOT0, D1G_SLOT1)):
        p[a] = le32((D1G_R0, D1G_R1)[i])
        p[a + 0x41] = bytes([0x41, 0x80 + i, 0x43])
        p[a + 0x51] = bytes([0x51, 0x52 + 0x10 * i, 0x53 + 0x10 * i, 0x54 + 0x10 * i, 0x55])
        p[a + 0x79] = bytes([0x79, ch ^ i, (ch ^ i) + 1])
    p.update(extra or {})
    return p


D1G_ARG_SLOT = 0x0010A500                           # the EAX slot of the setup rows


def d1g_setup_case(side, ptrs, ch=0, extra=None, rec=D1G_REC, slot=D1G_ARG_SLOT):
    """0x3FF08/0x40BBC: rec+0x51 = side (+0x53 a sentinel), the four dwords at 0x1077A8, the
    table slots' +0x7A (slot 0 = ch, slot 1 = ch ^ 5; +0x7B = 0 or 5, another class) and the EAX
    slot's +0x0C..+0x1F, +0x52..+0x57 seeded."""
    p = {rec + 0x50: bytes([0x50, side, 0x52, 0x53, 0x54]),
         0x001077A8: b"".join(le32(v) for v in ptrs),
         slot: le32(D1G_R1) + le32(0x04040404) + le32(0x08080808) + le32(0x0C0C0C0C)
               + le32(0x10101010) + le32(0x14141414) + le32(0x18181818) + le32(0x1C1C1C1C)
               + le32(0x20202020),
         slot + 0x51: bytes([0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58])}
    for i, a in enumerate((D1G_SLOT0, D1G_SLOT1)):
        c = ch ^ (5 * i)
        p[a] = le32((D1G_R0, D1G_R1)[i])
        p[a + 0x79] = bytes([0x79, c & 0xFF, 5 if c == 0 else 0, 0x7C])
    p.update(extra or {})
    return p


def d1g_hook_case(side, s61, slot_fill=0x50):
    """0x3FD30: both slots' record pointers (C3D_REC0/1), the side's record +0x60..+0x62 and its
    slot's +0x52..+0x57 and +0x89..+0x8B."""
    p = c3d_slot_pokes()
    so = 0x001077B0 + side * 0x94
    rec = C3D_REC0 if side == 0 else C3D_REC1
    p[rec + 0x60] = bytes([s61 ^ 0xFF if s61 else 0x60, s61, 0x62])
    p[so + 0x52] = bytes(slot_fill + i for i in range(2, 8))
    p[so + 0x89] = b"\x89\x8a\x8b"
    return p


# The text wrappers' stubs, as the C3g text rows declare them: 0x2F198 by value (the string's first
# 16 bytes) and writing the cursor 0x105F34 (so a save/restore, or its absence, is observable),
# 0x2EFD4 by value, 0x2F5A0 with the two cells dereferenced.
D1G_TXT198 = E.Call(0x2F198, ("eax", "edx", "[ebx]", "[ebx+4]", "[ebx+8]", "[ebx+12]", "ecx"),
                    mode="stub", clobbers=("ebx", "ecx", "edx"),
                    writes=((None, 0x00105F34, b"\x5A\x5A\x5A\x5A"),))
D1G_TXTFMT = E.Call(0x2EFD4, ("eax", "[edx]", "[edx+4]", "[edx+8]", "[edx+12]", "ebx", "ecx"),
                    mode="stub", clobbers=("ebx", "ecx", "edx"))
D1G_GLYPH = E.Call(0x2F5A0, ("eax", "[edx]", "[ebx]", "ecx", "s0"), mode="stub", pop=4,
                   clobbers=("ebx", "ecx", "edx"))
D1G_STR_A, D1G_STR_B = 0x0010A200, 0x0010A210       # two scratch strings (0x10A000 is zero)
D1G_STRS = {D1G_STR_A: b"\x11\x22\x33\x44\x55\x66\x77\x88\x99\xaa\xbb\xcc\xdd\xee\xff\x01",
            D1G_STR_B: b"\x91\x82\x73\x64\x55\x46\x37\x28\x19\x0a\xfb\xec\xdd\xce\xbf\x02"}

def d1g_rl_case(fill):
    """0x1C350: the dword pair below the node pool, the first and a middle window of nodes, the
    last nodes through the free head 0x10275C and a pair above it, and the list head 0x105B44 (the
    raw leaves it alone), all non-zero."""
    def run(n, k): return bytes(((fill + k + i) & 0x7F) | 0x80 for i in range(n))
    return {0x00101534: run(8, 0) + run(0x38, 8),                # 0x101534..0x101573
            0x00101D3C: run(0x40, 0x20),                         # nodes 256..263
            0x00102724: run(0x40, 0x40),                         # 0x102724..0x102763
            0x00105B40: run(12, 0x60)}                           # 0x105B40..0x105B4B


def d1g_cb_case(side, st, w=0x5A5A, ch=3, extra=None, rec=D1G_REC):
    """0x45B50: both slots' record pointers (C3D_REC0/1), the EAX slot's +0x52..+0x57 (state
    `st`), the two slots' +0x7A (the other side's = ch, the own = ch ^ 1), the countdown word
    0x1081EC between sentinels, 0xF0AFE, and +0x24 of the EDX record and both slot records."""
    p = c3d_slot_pokes()
    own, oth = 0x001077B0 + side * 0x94, 0x001077B0 + (1 - side) * 0x94
    p[own + 0x52] = bytes([0x52, 0x53, 0x54, 0x55, 0x56, st, 0x58])
    p[own + 0x79] = bytes([0x79, ch ^ 1, 0x7B])
    p[oth + 0x79] = bytes([0x79, ch, 0x7B])
    p[0x001081EA] = b"\xea\xeb" + le16(w) + b"\xee\xef"
    p[0x000F0AFD] = b"\xfd\xfe\xff"
    p[rec + 0x24] = le32(0x24242424)
    p[C3D_REC0 + 0x24] = le32(0x11111111)
    p[C3D_REC1 + 0x24] = le32(0x22222222)
    p.update(extra or {})
    return p


D1G_CURSOR = E.Call(0x2A620, ("eax", "edx"), mode="real")   # mode1_cursor: its own row (C3)

D1G_SPECS = [
    # 0x10D70 actor_pset_word_set: EAX = the word, [esp+4] = rec (`ret 4`). Mask 0: 0x4FA7E and
    # 0x4FB19 `xor eax,eax`; the other five sites return through 0x1D2F0/0x1D464/0x1DA84, whose
    # callers load EAX before reading it (record d1g §1).
    Spec("actor_pset_word_set", 0x10D70, [
        Case("w0", {"eax": 0xABCDFFFF, "s0": D1G_REC}, d1g_psw_case(0xBFFF)),
        Case("w1", {"eax": 0x00001234, "s0": D1G_REC}, d1g_psw_case(0x4004, idx=2)),
        Case("w2", {"eax": 0x00008001, "s0": D1G_REC}, d1g_psw_case(0x0040, idx=0)),
        Case("w3", {"eax": 0x00010203, "s0": D1G_REC}, d1g_psw_case(0x4000, idx=0x101,
                                                                     base=0x00108000)),
    ], eax_mask=0, mutants=("@base", "@idxw", "@scale", "@clrhi", "@clear", "@bit", "@keep",
                            "@width")),
    # 0x2BD20 actors_link_held: EAX = rec, EDX = v; `xor eax,eax` at 0x2BD40, so EAX = 0 whole.
    Spec("actors_link_held", 0x2BD20, [
        Case("l0", {"eax": D1G_REC, "edx": 0x1234}, d1g_lnk_case(1, 0x0010A000, k2=2)),
        Case("l1", {"eax": D1G_REC, "edx": 0x00FF}, d1g_lnk_case(0, 0x0010A000)),
        Case("l2", {"eax": D1G_REC, "edx": 0x0077}, d1g_lnk_case(0x80, 0x00107000)),
    ], mutants=("@sext", "@index", "@base", "@stride", "@field", "@width", "@ret")),
    # 0x37B54 fighter_37b54: EAX = rec. Mask 0: the 0xD000 dispatcher overwrites EAX (0x2B575),
    # 0x4904F is followed by `mov eax,0xf0` (0x4905B), and 0x45B43/0x45FD3 return through the slot
    # +0x0C callbacks 0x4505C/0x45B50, whose caller 0x35431 is followed by `xor eax,eax` (0x35434).
    Spec("fighter_37b54", 0x37B54, [
        Case("o0", {"eax": D1G_REC}, d1g_o53_case(0, (D1G_SLOT0, D1G_SLOT1, 0, 0))),
        Case("o1", {"eax": D1G_REC}, d1g_o53_case(1, (D1G_SLOT0, D1G_SLOT1, 0, 0))),
        Case("o2", {"eax": D1G_REC}, d1g_o53_case(0x80, (D1G_SLOT1, D1G_SLOT1, 0, 0),
                                                  {0x001077A8 + 0x81 * 4: le32(D1G_SLOT0)})),
        Case("o3", {"eax": D1G_REC}, d1g_o53_case(0, (D1G_SLOT0, 0, D1G_SLOT1, D1G_SLOT1))),
        Case("o4", {"eax": D1G_REC}, d1g_o53_case(2, (D1G_SLOT0, 0, 0, D1G_SLOT1))),
    ], eax_mask=0, mutants=("@field", "@xor", "@not", "@sext", "@base", "@null", "@deref",
                            "@off", "@val")),
    # 0x38B70 actor_cursor_reset: no argument. 0x654C7 (the runtime dword fill, ECX = 7 dwords)
    # runs on the original side; the port's mem_fill has no seam. Mask 0: both callers (0x20C3D,
    # 0x2BBDA) call 0x2F920 next, which loads EAX (`mov eax,0x105f38` 0x2F927) before any read.
    Spec("actor_cursor_reset", 0x38B70, [
        Case("z0", {}, d1g_cur_case(0x11)),
        Case("z1", {"eax": 0x12345678, "edx": 0x9ABCDEF0, "ecx": 0x55}, d1g_cur_case(0xC0)),
    ], allow_calls=(0x654C7,), eax_mask=0,
       mutants=("@bc", "@c0", "@c4", "@rows", "@bytes", "@over", "@latch", "@short")),
    # 0x28E98 actor_type_0a19_list_init: no argument; 0x249C0 (list_insert_before, its own C3b
    # row) runs on both sides. Mask 0: both callers (0x20E3D, 0x20EE4) call 0x34978 next, which
    # loads EAX (`mov eax,0x1077a8` 0x3497F) before reading it.
    Spec("actor_type_0a19_list_init", 0x28E98, [
        Case("i0", {}, d1g_lists_case(0x00104880, 0x00104888, 0x00104780, 0x00104880, 0x11)),
        Case("i1", {"eax": 0x5A5A5A5A}, d1g_lists_case(0x00104880, 0x00104888, 0x00104780,
                                                       0x00104880, 0x47)),
    ], allow_calls=(0x249C0,), eax_mask=0,
       mutants=("@inuse", "@self", "@first", "@count", "@list", "@order")),
    # 0x48C4C actor_type_2d_list_init: no argument; 0x249C0 runs on both sides, the voice 0xEF
    # (0x2C3FC) is the VOICE stub. Mask 0: its one caller 0x48DBF returns through the slot +0x0C
    # callback 0x48D94 (`xor eax,eax` 0x35434 after 0x35431).
    Spec("actor_type_2d_list_init", 0x48C4C, [
        Case("j0", {}, d1g_lists_case(0x00108368, 0x001082E0, 0x001082E8, 0x00108368, 0x21,
                                      {0x00108395: b"\x95\x96\x97\x98\x99"})),
        Case("j1", {"eax": 0x5A5A5A5A}, d1g_lists_case(0x00108368, 0x001082E0, 0x001082E8,
                                                       0x00108368, 0x63,
                                                       {0x00108395: b"\x15\x16\x17\x18\x19"}),
             {0x2C3FC: 0}),
    ], allow_calls=(0x249C0,), calls=(VOICE,), eax_mask=0,
       mutants=("@inuse", "@self", "@first", "@count", "@list", "@order", "@b98", "@b97", "@b96",
                "@b95", "@novoice", "@voice", "@early")),
    # 0x2BE5C actor_mode1_pset: EAX = rec (a pool record: the port's actor_pset guard, a PORT
    # deviation the raw does not have). 0x2A620 runs real and is recorded: it reads the rec+0x1C
    # and pset+0x14 this function has just stored and writes the rec+0x64 the ramp index is.
    # Mask 0: the three callers load EAX next (0x28FBA, 0x29072, 0x37FD4).
    Spec("actor_mode1_pset", 0x2BE5C, [
        Case("m0", {"eax": D1G_PREC}, d1g_m1_case(0xEFFF, y30=0x00030000, f0aec=0x40)),
        Case("m1", {"eax": D1G_PREC}, d1g_m1_case(0x3000, ramp=((5, 0x0123), (2, 0x0222)))),
        Case("m2", {"eax": D1G_PREC}, d1g_m1_case(0x1000, thr=0x100,
                                                  ramp=((-1, 0x0040), (0xFF, 0x0999), (2, 0x0222)))),
        Case("m3", {"eax": D1G_PREC}, d1g_m1_case(0x1000, pset8=0x3BC0, thr=0xE8,
                                                  ramp=((7, 0x8001), (2, 0x0222)))),
        Case("m4", {"eax": D1G_PREC}, d1g_m1_case(0x0010, y30=0xFFFF8000, f0aec=0x100,
                                                  f0af0=0xFFFFF000)),
    ], calls=(D1G_CURSOR,), eax_mask=0,
       mutants=("@y", "@sar", "@bit", "@x14", "@cursor", "@early", "@ramp", "@x46", "@x2", "@zext",
                "@else", "@clr", "@clrlo")),
    # 0x2A39C anim_id_path: EAX = rec, DX = the pset index (`mov cx,dx` 0x2A3A2; a4's EDX has
    # high bits: the correction this row found, `@idxw` is the port before it). 0x2A408 is the
    # C1_SPRITE_ID stub (EDX clobbered; `mov edx,eax` 0x2A3C1 reloads it). Mask 0: after 0x2A272
    # AL is reloaded (0x2A277) and AX at 0x2A289 before the only store of it, and 0x2A1FC's
    # callers load EAX (0x2A38E, 0x35818); after 0x2ABE6 `mov ax,[ecx+0x2a]` .. `and eax,0xffff`
    # (0x2ABEB..0x2ABF4).
    Spec("anim_id_path", 0x2A39C, [
        Case("a0", {"eax": D1G_REC, "edx": 1}, d1g_aid_case(0), {0x2A408: 0x8123}),
        Case("a1", {"eax": D1G_REC, "edx": 1}, d1g_aid_case(2, 0x4000), {0x2A408: 0x0123}),
        Case("a2", {"eax": D1G_REC, "edx": 2}, d1g_aid_case(2, 0xBFFF, idx=2), {0x2A408: 0xFFFF8123}),
        Case("a3", {"eax": D1G_REC, "edx": 1}, d1g_aid_case(0x80, 0x4040, pool=0x00107000),
             {0x2A408: 0x0001}),
        Case("a4", {"eax": D1G_REC, "edx": 0x00030001}, d1g_aid_case(0), {0x2A408: 0x7777}),
        Case("a5", {"eax": D1G_REC, "edx": 0x101}, d1g_aid_case(0, idx=0x101, base=0x00108000),
             {0x2A408: 0x0456}),
    ], calls=(C1_SPRITE_ID,), eax_mask=0,
       mutants=("@clear", "@order", "@idxw", "@idxb", "@arg", "@nopar", "@k", "@pbase", "@pbit", "@keep",
                "@width")),
    # 0x2A4FC motion_step: EAX = rec. 0x2B150 (set_dead) is the P7_2B150 stub. Mask 0: after
    # 0x2A2E2 the caller 0x2A1FC returns to callers that load EAX (0x2A38E, 0x35818); after
    # 0x38572 `mov eax,ecx` (0x38582, 0x3859B) or 0x186C4's `xor eax,eax`.
    Spec("motion_step", 0x2A4FC, [
        Case("s0", {"eax": D1G_REC}, d1g_mot_case(w2a=0x7FFF, w34=0x100, w36=5, w44=3)),
        Case("s1", {"eax": D1G_REC}, d1g_mot_case(w32=0xFFF0, w38=0x20, w34=0x100, w36=5, b42=5)),
        Case("s2", {"eax": D1G_REC}, d1g_mot_case(y=0, w34=0xFF00, w36=0xFFFF, b42=5, b43=3, w44=4,
                                                  w28=0x20)),
        Case("s3", {"eax": D1G_REC}, d1g_mot_case(w34=0, w36=5, b42=9, b43=7, w44=4, w28=0xFF5F)),
        Case("s4", {"eax": D1G_REC}, d1g_mot_case(y=0x10, w34=0x10, w36=0xFFFF, b43=3, w44=2)),
        Case("s5", {"eax": D1G_REC}, d1g_mot_case(y=1, w34=2, w36=0xFFFF, b42=1, b43=3, w44=3,
                                                  w28=0x80)),
        Case("s6", {"eax": D1G_REC}, d1g_mot_case(y=0xFFFFFFFB, w36=0xFFFE, w44=6, w28=0x08)),
        Case("s7", {"eax": D1G_REC}, d1g_mot_case(w34=1, b42=0x80)),
        Case("s8", {"eax": D1G_REC}, d1g_mot_case(y=0xFFFFFFFF, w34=0x90, w36=0, b43=0x80, w44=1)),
        Case("s9", {"eax": D1G_REC}, d1g_mot_case(w34=0xFFE0, b43=0x30)),
    ], calls=(P7_2B150,), eax_mask=0,
       mutants=("@gate", "@x", "@y", "@frac", "@b42", "@zero", "@b43", "@abs", "@fr", "@dir",
                "@stop", "@clr43", "@g20", "@ge", "@y0", "@sub", "@dead", "@nodead", "@v36",
                "@g44", "@land")),
    # 0x19958 debris_spawn: no argument. 0x5D7DC runs real and recorded (P6_RNG), 0x2AE14 is the
    # SPAWN stub (its EAX the spawned record, 0 in v4). Mask 0: 0x19B90 falls from the call
    # (0x19BB3) into the walker 0x19BB8, which loads EAX (`mov eax,[ebx+8]` 0x19BCC) or returns to
    # the update dispatcher (0x24CEF), whose next read of EAX follows `mov eax,edx` (0x24CED) or
    # the `int 0x16` at 0x24D0E (AH = 1 loaded at 0x24D08; AX is the BIOS's answer).
    Spec("debris_spawn", 0x19958, [
        Case("v0", {}, d1g_deb_case(0x12345678, 0xFF, 4)),
        Case("v1", {}, d1g_deb_case(0x12345678, 0, 4), {0x2AE14: D1G_R1}),
        Case("v2", {}, d1g_deb_case(0x22222222, 1, 5, ca9=0, t48=0x28), {0x2AE14: D1G_R1}),
        Case("v3", {}, d1g_deb_case(0x55555555, 0, 5, ca9=1, t48=0x27), {0x2AE14: D1G_R1}),
        Case("v4", {}, d1g_deb_case(0x33333333, 0, 0), {0x2AE14: 0}),
        Case("v5", {}, d1g_deb_case(0x11111111, 0, 6, t48=0x28), {0x2AE14: D1G_R1}),
        Case("v6", {}, d1g_deb_case(0x77777777, 1, 3, ca9=1, t48=0x28), {0x2AE14: D1G_R1}),
        Case("v7", {}, d1g_deb_case(0x0BADF00D, 0, 5, ca9=0, t48=0x29), {0x2AE14: D1G_R1}),
    ], calls=(P6_RNG, SPAWN), eax_mask=0,
       mutants=("@gate", "@x", "@xr", "@w0", "@d4", "@ca9", "@redraw", "@w0r", "@rng2", "@args",
                "@s0", "@null", "@v4", "@mode5", "@t28", "@g8")),
    # 0x2372C: the 0xD100 target as fn_resolve reaches it (EAX = rec; EDX is overwritten at
    # 0x2374D/0x2375E before any read). 0x2AE14 is the SPAWN stub. Mask 0: the dispatcher
    # overwrites EAX (`mov eax,ecx` 0x2B59A) and the direct call 0x2463E is followed by
    # `mov eax,0x40`/`mov eax,0x80` (0x2464C/0x24653).
    Spec("anim_2372c", 0x2372C, [
        Case("n0", {"eax": D1G_REC, "edx": 0x1234}, d1g_prj_case(0, w28=0x4000)),
        Case("n1", {"eax": D1G_REC, "edx": 0x1234}, d1g_prj_case(D1G_SLOT0, 0xBFFF, 0xBFFF, 0),
             {0x2AE14: D1G_R1}),
        Case("n2", {"eax": D1G_REC, "edx": 0}, d1g_prj_case(D1G_SLOT0, 0x4000, 0x4000, 1),
             {0x2AE14: D1G_R1}),
        Case("n3", {"eax": D1G_REC, "edx": 0}, d1g_prj_case(D1G_SLOT0, 0x4000, 0x0040, 0x80,
                                                             y30=0xFFFF8000), {0x2AE14: D1G_R1}),
        Case("n4", {"eax": D1G_REC, "edx": 0}, d1g_prj_case(D1G_SLOT0, 0x0040, 0x4000, 0),
             {0x2AE14: D1G_R1}),
    ], calls=(SPAWN,), eax_mask=0,
       mutants=("@sign", "@a5", "@off", "@y", "@sar", "@args", "@s8", "@v34", "@p14", "@side",
                "@p2e", "@p4e", "@desc")),
    # 0x370F0: the 0xD000/0xD100 target as fn_resolve reaches it (EAX = rec; DL is loaded at
    # 0x37105 before any read of EDX). 0x2BC30 is the ANIM_BEGIN stub. Mask 0: the dispatcher
    # overwrites EAX; 0x48BC3 and 0x48F36 return through the slot +0x0C callbacks 0x48AAC and
    # 0x48D94 (`xor eax,eax` 0x35434 after 0x35431).
    Spec("anim_370f0", 0x370F0, [
        Case("f0", {"eax": D1G_REC}, d1g_fin_case(0, (0, D1G_SLOT1, 0, 0), 1)),
        Case("f1", {"eax": D1G_REC}, d1g_fin_case(0, (D1G_SLOT0, 0, 0, 0), 1)),
        Case("f2", {"eax": D1G_REC}, d1g_fin_case(0, (D1G_SLOT0, D1G_SLOT1, 0, 0), 1, ch=3)),
        Case("f3", {"eax": D1G_REC}, d1g_fin_case(1, (D1G_SLOT0, D1G_SLOT1, 0, 0), 0, ch=2)),
        Case("f4", {"eax": D1G_REC},
             d1g_fin_case(0x80, (0, 0, 0, 0), 0xFF, ch=0x80,
                          extra={0x001077A8 + 0x80 * 4: le32(D1G_SLOT0) + le32(D1G_SLOT1),
                                 0x000BDC2C + 0x80 * 4: le32(0x000E1234) + le32(0x000E5678),
                                 0x000BDC2C - 0x80 * 4: le32(0x000E9ABC)})),
        Case("f5", {"eax": D1G_REC}, d1g_fin_case(2, (0, 0, D1G_SLOT1, D1G_SLOT0), 0)),
    ], calls=(ANIM_BEGIN,), eax_mask=0,
       mutants=("@swap", "@s54", "@or4", "@s52", "@gate", "@char", "@sextc", "@frame", "@rec",
                "@o42", "@r53", "@afe", "@early")),
    # 0x3FF08 fighter_3ff08: EAX = slot, EDX = rec; every exit sets AL (`xor al,al` 0x3FF23 and
    # 0x3FF3A, `mov al,1` 0x3FF85) and the rest of EAX is scratch (the side * 4 at 0x3FF2D, or
    # 0x3C4CC's): mask 0xFF. Its one caller (0x40026) loads AL itself (`mov al,1` 0x4002B).
    # 0x3C4CC is the HIT_B stub.
    Spec("fighter_3ff08", 0x3FF08, [
        Case("g0", {"eax": D1G_ARG_SLOT, "edx": D1G_REC}, d1g_setup_case(0, (D1G_SLOT0, 0, 0, 0))),
        Case("g1", {"eax": D1G_ARG_SLOT, "edx": D1G_REC}, d1g_setup_case(0, (0, D1G_SLOT1, 0, 0))),
        Case("g2", {"eax": D1G_ARG_SLOT, "edx": D1G_REC},
             d1g_setup_case(0, (D1G_SLOT0, D1G_SLOT1, 0, 0), ch=0)),
        Case("g3", {"eax": D1G_ARG_SLOT, "edx": D1G_REC},
             d1g_setup_case(1, (D1G_SLOT0, D1G_SLOT1, 0, 0), ch=0)),
        Case("g4", {"eax": D1G_ARG_SLOT, "edx": D1G_REC},
             d1g_setup_case(0, (D1G_SLOT0, D1G_SLOT1, 0, 0), ch=3)),
        Case("g5", {"eax": D1G_ARG_SLOT, "edx": D1G_REC},
             d1g_setup_case(0x80, (0, 0, 0, 0), ch=0x85,
                            extra={0x001077A8 + 0x80 * 4: le32(D1G_SLOT0) + le32(D1G_SLOT1)})),
    ], calls=(HIT_B,), eax_mask=0xFF,
       mutants=("@other", "@retn", "@xor", "@chf", "@late", "@hrec", "@ch0", "@frame", "@ch5",
                "@r53", "@s52", "@s57", "@cb0c", "@cb18", "@cb1c", "@ret")),
    # 0x40BBC fighter_40bbc: the 0x1078E8-shaped callback (the dword at 0xBDB14; 0x379E8, then
    # `test eax,eax` on the whole EAX). EAX = slot, EDX = rec. Mask 0xFF: the main path ends
    # `mov al,1` (0x40C25) over 0x3C4CC's or the char's scratch, non-zero whatever the rest; the
    # null path is `xor al,al` over side * 4 (0x40BD1), zero for every side below 0x40 (h4, side
    # 0x40, is the named limit: the raw's EAX is 0x100 there, the port returns 0; record d1g §4).
    # 0x339AC builds an unread context on the stack (allowed; the port builds none). 0x3C4CC is
    # the HIT_B stub.
    Spec("fighter_40bbc", 0x40BBC, [
        Case("h0", {"eax": D1G_ARG_SLOT, "edx": D1G_REC}, d1g_setup_case(0, (0, D1G_SLOT1, 0, 0))),
        Case("h1", {"eax": D1G_ARG_SLOT, "edx": D1G_REC},
             d1g_setup_case(0, (D1G_SLOT0, D1G_SLOT1, 0, 0), ch=0)),
        Case("h2", {"eax": D1G_ARG_SLOT, "edx": D1G_REC},
             d1g_setup_case(1, (D1G_SLOT0, D1G_SLOT1, 0, 0), ch=0)),
        Case("h3", {"eax": D1G_ARG_SLOT, "edx": D1G_REC},
             d1g_setup_case(0, (D1G_SLOT0, D1G_SLOT1, 0, 0), ch=2)),
        Case("h4", {"eax": D1G_ARG_SLOT, "edx": D1G_REC},
             d1g_setup_case(0x40, (D1G_SLOT0, D1G_SLOT1, 0, 0))),
    ], allow_calls=(0x339AC,), calls=(HIT_B,), eax_mask=0xFF,
       mutants=("@retn", "@xor", "@chf", "@late", "@hrec", "@ch0", "@frame", "@ch5", "@s54",
                "@s52", "@s57", "@cb0c", "@cb18", "@cb1c", "@ret")),
    # 0x3FD30 fighter_3fd30: the slot +0x18 hook (0x1903F; `test eax,eax` 0x19048: the whole EAX,
    # mask 0xFFFFFFFF). EAX = side. 0x33950 and 0x18BD4 run on both sides; 0x18C14 is the CHECKS
    # stub (its EAX returned as is, varied per case; k4's 0x100 has AL = 0); 0x36870 is ANIM54.
    Spec("fighter_3fd30", 0x3FD30, [
        Case("k0", {"eax": 0}, d1g_hook_case(0, 0)),
        Case("k1", {"eax": 0}, d1g_hook_case(0, 5), {0x18C14: 1}),
        Case("k2", {"eax": 1}, d1g_hook_case(1, 0x80), {0x18C14: 0}),
        Case("k3", {"eax": 0}, d1g_hook_case(0, 1), {0x18C14: 2}),
        Case("k4", {"eax": 1}, d1g_hook_case(1, 7), {0x18C14: 0x100}),
        Case("k5", {"eax": 1}, d1g_hook_case(1, 9), {0x18C14: 0x101}),
    ], allow_calls=(0x33950, 0x18BD4), calls=(CHECKS, ANIM54),
       mutants=("@gate", "@f5", "@f1", "@f8", "@f4", "@fe", "@f7", "@boxes", "@al", "@s8a",
                "@s52", "@s53", "@r61", "@anim", "@late", "@s57", "@ret")),
    # The six text wrappers below return whatever their last callee left in EAX (none writes it
    # after the call); the port's are void, as 0x2F198's own row is (C3g). Mask 0 rests on the
    # caller scan of record d1g §1 (tool-assisted, its limits named there).
    # 0x2F41C text_cursor_next_line: EAX = string, EDX = mode: 0x2F198(0, -1, string, mode).
    Spec("text_cursor_next_line", 0x2F41C, [
        Case("t0", {"eax": D1G_STR_A, "edx": 0x5000}, {**D1G_STRS, 0x00105F34: le32(0x11223344)}),
        Case("t1", {"eax": D1G_STR_B, "edx": 0x1002, "ebx": 0x77, "ecx": 0x88},
             {**D1G_STRS, 0x00105F34: le32(0xAABBCCDD)}),
    ], calls=(D1G_TXT198,), eax_mask=0, mutants=("@col", "@row", "@str", "@mode")),
    # 0x2F510 text_cursor_hold_font2: EAX = col, EDX = row, EBX = string, ECX = mode (`or cl,2`
    # 0x2F511); the cursor dword is saved (0x2F514) and restored (0x2F51F) around 0x2F198.
    Spec("text_cursor_hold_font2", 0x2F510, [
        Case("u0", {"eax": 3, "edx": 4, "ebx": D1G_STR_A, "ecx": 0x1000},
             {**D1G_STRS, 0x00105F34: le32(0x11223344)}),
        Case("u1", {"eax": 0xFFFFFFFF, "edx": 0xFFFFFFFF, "ebx": D1G_STR_B, "ecx": 0x2003},
             {**D1G_STRS, 0x00105F34: le32(0xAABBCCDD)}),
        Case("u2", {"eax": 7, "edx": 9, "ebx": D1G_STR_B, "ecx": 0x4001},
             {**D1G_STRS, 0x00105F34: le32(0x01020304)}),
    ], calls=(D1G_TXT198,), eax_mask=0, mutants=("@or", "@or1", "@args", "@str", "@save")),
    # 0x2F434 text_number_set: EAX = col, EDX = row, EBX = value, ECX = width, [esp+4] = pad,
    # [esp+8] = mode; `ret 8`. 0x2EFD4 then 0x2F198, the cursor not held (`@save` holds it).
    Spec("text_number_set", 0x2F434, [
        Case("p0", {"eax": 5, "edx": 6, "ebx": 0xFFFFFFD6, "ecx": 3, "s0": 0, "s1": 0x1000},
             {0x00105F34: le32(0x11223344)}),
        Case("p1", {"eax": 0, "edx": 9, "ebx": 12345, "ecx": 5, "s0": 2, "s1": 0x2002},
             {0x00105F34: le32(0xAABBCCDD)}),
    ], calls=(D1G_TXTFMT, D1G_TXT198), eax_mask=0,
       mutants=("@val", "@pad", "@mode", "@save", "@fmt", "@col", "@row")),
    # 0x2F528 text_number_draw_font2: the same registers and stack; the mode ORed with 2 (`or
    # ebp,2` 0x2F54A) and the cursor held (0x2F53A/0x2F544, 0x2F55F/0x2F563).
    Spec("text_number_draw_font2", 0x2F528, [
        Case("q0", {"eax": 5, "edx": 6, "ebx": 0xFFFFFFD6, "ecx": 3, "s0": 0, "s1": 0x1000},
             {0x00105F34: le32(0x11223344)}),
        Case("q1", {"eax": 0, "edx": 9, "ebx": 12345, "ecx": 5, "s0": 2, "s1": 0x2001},
             {0x00105F34: le32(0xAABBCCDD)}),
    ], calls=(D1G_TXTFMT, D1G_TXT198), eax_mask=0,
       mutants=("@val", "@pad", "@mode", "@save", "@fmt", "@col", "@row")),
    # 0x2F174 text_glyph_at: EAX = col, EBX = ch, EDX = row, ECX = mode: 0x2F5A0(ch, &col, &row,
    # mode, 0) on stack copies of the two cells (0x2F182/0x2F18A), the advance discarded.
    Spec("text_glyph_at", 0x2F174, [
        Case("y0", {"eax": 5, "ebx": 0x41, "edx": 6, "ecx": 0x2000}, {}, {0x2F5A0: 0}),
        Case("y1", {"eax": 0x2A, "ebx": 0x80, "edx": 0x13, "ecx": 0x1002}, {}, {0x2F5A0: 1}),
    ], calls=(D1G_GLYPH,), eax_mask=0, mutants=("@swap", "@ch", "@mode", "@vert")),
    # 0x2F388 text_cells_release_count: EAX = col (negative centres), EDX = row, EBX = the cell
    # count. 0x2AD40 is the RELEASE stub (records in the pool: the port's actor_pset guard, a
    # PORT deviation, record d1g §4).
    Spec("text_cells_release_count", 0x2F388, [
        Case("e0", {"eax": 2, "edx": 1, "ebx": 4},
             c3g_cell_case([(1 * 0xac + 2 * 4, 0), (1 * 0xac + 5 * 4, 3)])),
        Case("e1", {"eax": 0xFFFFFFFF, "edx": 1, "ebx": 0x0C},
             c3g_cell_case([(1 * 0xac + 15 * 4, 1), (1 * 0xac + 26 * 4, 2), (1 * 0xac + 27 * 4, 3)])),
        Case("e2", {"eax": 0xFFFFFFFE, "edx": 1, "ebx": 0x30},
             c3g_cell_case([(1 * 0xac, 2), (1 * 0xac + 0x29 * 4, 1), (1 * 0xac + 0x2A * 4, 3)])),
        Case("e3", {"eax": 0x2A, "edx": 1, "ebx": 4},
             c3g_cell_case([(1 * 0xac + 0x2A * 4, 0), (2 * 0xac, 1), (2 * 0xac + 4, 2),
                            (2 * 0xac + 8, 3)])),
        Case("e4", {"eax": 0x2B, "edx": 1, "ebx": 2}, c3g_cell_case([(2 * 0xac, 2)])),
        Case("e5", {"eax": 5, "edx": 2, "ebx": 0}, c3g_cell_case([(2 * 0xac + 5 * 4, 0)])),
        Case("e6", {"eax": 5, "edx": 2, "ebx": 0xFFFFFFFF}, c3g_cell_case([(2 * 0xac + 5 * 4, 0)])),
        Case("e7", {"eax": 0xFFFFFFFF, "edx": 0, "ebx": 0x2B},
             c3g_cell_case([(0, 1), (0x2A * 4, 2), (0x2B * 4, 3)])),
    ], calls=(RELEASE,), eax_mask=0,
       mutants=("@neg", "@force", "@cmp", "@stride", "@rowbase", "@zero", "@rel", "@pset", "@rec0",
                "@centre", "@count")),
    # 0x1C350 render_list_init: no argument. The list head 0x105B44 is seeded and must survive:
    # the correction this row found (`@head` is the port before it). Mask 0: its one caller
    # (0x2BBD5) calls 0x38B70 next, which loads EAX (`mov eax,0x107a1c` 0x38B79) before any read.
    Spec("render_list_init", 0x1C350, [
        Case("x0", {}, d1g_rl_case(0x11)),
        Case("x1", {"eax": 0x5A5A5A5A, "ebx": 0x12345678, "edx": 0x9ABCDEF0}, d1g_rl_case(0x57)),
    ], eax_mask=0, mutants=("@count", "@stride", "@ptr4", "@free", "@last", "@l58", "@head")),
    # 0x45B50 fighter_45b50: the slot +0x0C callback (0x35431, then `xor eax,eax` 0x35434: mask
    # 0). EAX = slot, EDX = rec, EBX = side. 0x33950 runs on both sides; 0x2BC30, 0x2A17C and
    # 0x2C3FC are the ANIM_BEGIN, PALETTE and VOICE stubs (EBX, the handle loaded at 0x45B94,
    # survives 0x2BC30: its clobber set is EDX).
    Spec("fighter_45b50", 0x45B50, [
        Case("c0", {"eax": 0x001077B0, "edx": D1G_REC, "ebx": 0}, d1g_cb_case(0, 0)),
        Case("c1", {"eax": 0x001077B0, "edx": D1G_REC, "ebx": 0}, d1g_cb_case(0, 1)),
        Case("c2", {"eax": 0x001077B0 + 0x94, "edx": D1G_REC, "ebx": 1},
             d1g_cb_case(1, 1, ch=0x80, extra={0x0009B01C + 0x80 * 4: le32(0x000E1234),
                                              0x0009B01C - 0x80 * 4: le32(0x000E5678)})),
        Case("c3", {"eax": 0x001077B0, "edx": D1G_REC, "ebx": 0}, d1g_cb_case(0, 2, w=5)),
        Case("c4", {"eax": 0x001077B0, "edx": D1G_REC, "ebx": 0}, d1g_cb_case(0, 2, w=1)),
        Case("c5", {"eax": 0x001077B0 + 0x94, "edx": D1G_REC, "ebx": 1},
             d1g_cb_case(1, 2, w=0x8001)),
        Case("c6", {"eax": 0x001077B0, "edx": D1G_REC, "ebx": 0}, d1g_cb_case(0, 3, w=1)),
        Case("c7", {"eax": 0x001077B0, "edx": D1G_REC, "ebx": 0}, d1g_cb_case(0, 2, w=0)),
    ], allow_calls=(0x33950,), calls=(ANIM_BEGIN, PALETTE, VOICE), eax_mask=0,
       mutants=("@st1", "@rec", "@char", "@sextc", "@frame", "@pal", "@palrec", "@v50", "@w",
                "@inc", "@dec", "@order", "@jg", "@ge", "@r24", "@o24", "@vd2", "@s53", "@s52",
                "@afe")),
]
D1_SPECS += D1G_SPECS

# ---- track D1 batch d1h (record 2026-10-11-reverse-d1h): attract.c, camera.c, effects.c ----------

D1H_RNG = E.Call(0x5D7DC, ("eax",), mode="stub")
D1H_RNG_REAL = E.Call(0x5D7DC, ("eax",), mode="real")
D1H_INSERT_BEFORE = E.Call(0x249C0, ("eax", "edx"), mode="real")
D1H_REC = 0x0010A000           # zero BSS of the image: a record a stubbed spawn returns


def d1h_fill(addr, n, byte):
    """`n` bytes of `byte` from `addr`, as pokes of at most 0x40 bytes."""
    return {addr + o: bytes([byte]) * min(0x40, n - o) for o in range(0, n, 0x40)}


def d1h_dust_case(tick, seed, cam):
    """0x1282C: the 0xEF6DC tick word, the rng seed, the camera x and the two records' +0x30..+0x3B."""
    return {0x000EF6DC: le16(tick), DS_RNG: le32(seed), 0x000F0AF0: le32(cam),
            D1H_REC + 0x30: bytes(range(0x30, 0x3C)), D1H_REC + 0xB0: bytes(range(0xB0, 0xBC))}


def d1h_effects_case(lock, fill):
    """0x13ADC: the lock byte, the two sentinels and each record's first 12 bytes (its two links
    and the dword after them)."""
    p = {0x0009AF3C: bytes([lock, 0x11, 0x22, 0x33]), 0x000FCCE0: bytes([fill]) * 0x10}
    for rec in range(0x000F0B00, 0x000FCCE0, 0x814):
        p[rec] = bytes([fill]) * 12
    return p


def d1h_vt_case(c1, c2):
    """0x10F28: the two countdown words 0xF0A60/0xF0A62 with sentinel words on both sides."""
    return {0x000F0A5C: le32(0xA5A6A7A8) + le16(c1) + le16(c2) + le32(0x5A5B5C5D)}


def d1h_cam(y=0xE1E2E3E4, x=0xC1C2C3C4, shake=0xA1A2, vel=0xB1B2, step=0x0400, mode=0x5A, idx=0x6B):
    """The camera block 0xF0AEC..0xF0AFF as one poke: y, x, the shake offset and velocity words,
    a sentinel dword, the step word, the mode byte and the mode-0 index byte."""
    return {0x000F0AEC: le32(y) + le32(x) + le16(shake) + le16(vel) + le32(0xD1D2D3D4) + le16(step)
            + bytes([mode, idx])}


D1H_TRACK = {0: 0x0010A000, 1: 0x0010A080, 0x80: 0x0010A100}


def d1h_track_case(idx, cam, target):
    """0x12DF0: the three table entries the index byte can select (0, 1 and 0x80: 0x1079A8, where a
    sign-extended index would read 0x1075A8), each record's +0x30..+0x3B, the selected +0x34."""
    p = d1h_cam(x=cam, idx=idx)
    for i, rec in D1H_TRACK.items():
        p[0x001077A8 + i * 4] = le32(rec)
        p[rec + 0x30] = le32(0x30303030) + le32(target if i == idx else 0x44440000 + i) + le32(0x38380000 + i)
    return p


def d1h_shake_case(vel, off, flag):
    """0x1324C: the shake words in the camera block and the 0x104AE8 flag dword."""
    p = d1h_cam(shake=off, vel=vel)
    p[0x00104AE8] = bytes([flag, 0xE9, 0xEA, 0xEB])
    return p


def d1h_center_case(cam, p0, p1, y, gate, idx=None, third=None):
    """0x13290/0x1333C: the camera block (mode byte 0x5A), the two slots' record pointers, each
    record's +0x14..+0x1F, the gate byte 0x1078FE and, for 0x1333C, the index byte 0x10810D (a
    third slot pointer at 0x1077B0 + 2*0x94 when `third` is given)."""
    p = d1h_cam(y=y, x=cam)
    p[0x001077B0] = le32(0x0010A000)
    p[0x00107844] = le32(0x0010A080)
    p[0x0010A014] = le32(0x14141414) + le32(p0) + le32(0x1C1C1C1C)
    p[0x0010A094] = le32(0x15151515) + le32(p1) + le32(0x1D1D1D1D)
    p[0x001078FE] = bytes([gate])
    if idx is not None:
        p[0x0010810D] = bytes([idx])
        p[0x001077B4] = le32(0x0010A080)      # where an index scaled by 4 would look for side 1
    if third is not None:
        p[0x001078D8] = le32(0x0010A100)
        p[0x0010A114] = le32(0x16161616) + le32(third) + le32(0x1E1E1E1E)
    return p


def d1h_hold_case(mid, cam, s0, s1):
    """0x12FD8: the centre 0x108884, the camera block, and per slot (x, latch) relative to the
    centre: +0x2C..+0x3B as one poke (+0x2C and +0x30 sentinels, +0x34 = x, +0x38 = latch), the
    record pointer and the record's +0x14..+0x1F."""
    p = d1h_cam(x=cam)
    p[0x00108884] = le32(mid)
    for i, (x, latch) in enumerate((s0, s1)):
        slot, rec = 0x001077B0 + i * 0x94, 0x0010A000 + i * 0x80
        p[slot] = le32(rec)
        p[slot + 0x2C] = le32(0x2C2C2C20 + i) + le32(0x30303030) + le32(mid + x) + le32(mid + latch)
        p[rec + 0x14] = le32(0x14141410 + i) + le32(0x18181810 + i) + le32(0x1C1C1C10 + i)
    return p


def d1h_char_case(side, ch):
    """0x17EEC: the side's slot +0x78..+0x7B with the character at +0x7A; the other slot holds a
    different in-range character."""
    other = (ch + 3) % 7
    return {0x001077B0 + side * 0x94 + 0x78: bytes([0x78, 0x79, ch, 0x7B]),
            0x001077B0 + (1 - side) * 0x94 + 0x78: bytes([0x78, 0x79, other, 0x7B])}


D1H_BASE_SEED = {0x00100A6C: le32(0x6C6C6C6C) + le32(0x70707070) + le32(0x74747474) + le32(0x78787878),
                 0x00100A94: le32(0x94949494) + le32(0x98989898) + le32(0x9C9C9C9C) + le32(0xA0A0A0A0)}


def d1h_last_case(side, f24, f20, other=(9.0, 9.0), const=None):
    """0x164C0: the two slots' record pointers and each record's +0x1C..+0x2B (+0x20 and +0x24 the
    two floats, sentinels around them); `const` overrides the -1.0f at 0x80580."""
    p = {}
    for i in (0, 1):
        rec = 0x0010A000 + i * 0x80
        a, b = (f24, f20) if i == side else other
        p[0x001077B0 + i * 0x94] = le32(rec)
        p[rec + 0x1C] = le32(0x1C1C1C1C) + f32(b) + f32(a) + le32(0x28282828)
    if const is not None:
        p[0x00080580] = f32(const)
    return p


# 0x16734: (character, sprite id) for every id the raw's compare chains name and each range's two
# outer neighbours; characters 7 and 0xFF take the `ja` to the miss.
D1H_CODE_IDS = (
    [(0, u) for u in (0xF97, 0xF98, 0xF9B, 0xF9C, 0xF9D, 0xF9E, 0xF9F, 0xFA0, 0xFA1, 0xFA2, 0xFA3)]
    + [(1, u) for u in range(0x134E, 0x135A)]
    + [(2, u) for u in (0xC47, 0xC48, 0xC49, 0xC4A, 0xC50, 0xC51)]
    + [(3, u) for u in (0x1744, 0x1745, 0x1746, 0x1747, 0x1748, 0x174E, 0x174F, 0x1750, 0x1751, 0x1752, 0x1753)]
    + [(4, u) for u in range(0x2023, 0x202D)]
    + [(5, u) for u in (0xF97, 0xF98, 0xF9B, 0xF9C, 0xF9F, 0xFA0, 0xFA1, 0xFA2, 0xFA3)]
    + [(6, u) for u in range(0x134E, 0x135A)]
    + [(7, 0xF9F), (0xFF, 0x134F)])
D1H_POOL = 0x0010A800


def d1h_code_case(side, ch, uid, bit15):
    """0x16734: both slots' record pointers and +0x78..+0x7B (the character at +0x7A, another valid
    one at +0x7B), each record's +0x54..+0x57 (the actor index at +0x56, another at +0x54), the
    pool pointer and four actors: the side's (index 4) holds the id, the others 0x7777."""
    p = {0x001014EC: le32(D1H_POOL)}
    for i in (0, 1):
        slot, rec = 0x001077B0 + i * 0x94, 0x0010A000 + i * 0x80
        mine = i == side
        p[slot] = le32(rec)
        p[slot + 0x78] = bytes([0x78, 0x79, ch if mine else (ch + 2) % 7, (ch + 1) % 7])
        p[rec + 0x54] = le16(6) + le16(4 if mine else 8)
    for idx in (2, 4, 6, 8):
        w = (uid | (0x8000 if bit15 else 0)) if idx == 4 else 0x7777
        p[D1H_POOL + idx * 0x20] = le16(w) + le16(0xA5A5)
    return p


D1H_BUF = 0x0010A040


def d1h_rect_case(a, b, alias=False):
    """0x14080: (regs, pokes). a at D1H_BUF, b at +0x20, out at +0x40 (0xEE-seeded) or, when
    `alias`, out == a as the raw caller passes it; a sentinel dword after each rectangle."""
    pokes = {D1H_BUF: b"".join(le32(v) for v in a) + le32(0xA1A1A1A1),
             D1H_BUF + 0x20: b"".join(le32(v) for v in b) + le32(0xB1B1B1B1),
             D1H_BUF + 0x40: b"\xee" * 0x14}
    return {"eax": D1H_BUF if alias else D1H_BUF + 0x40, "edx": D1H_BUF, "ebx": D1H_BUF + 0x20}, pokes


def d1h_buf_case(seed):
    """0x60 bytes at D1H_BUF - 0x10, every byte different from its neighbours and from zero."""
    return {D1H_BUF - 0x10 + o: bytes(((i * 37 + seed * 11) & 0xFF) | 1 for i in range(o, o + 0x30))
            for o in (0, 0x30)}


def d1h_row_case(row, seed):
    """0x1638C: 0x40 bytes of both row tables at `row` (different patterns) and the 0xEE-seeded
    destination."""
    return {0x000FD160 + row * 0x26: bytes((i * 7 + seed * 13 + 1) & 0xFF for i in range(0x40)),
            0x000FEDE0 + row * 0x26: bytes((i * 5 + seed * 17 + 0x80) & 0xFF for i in range(0x40)),
            D1H_BUF: b"\xee" * 0x40}


D1H_VIS_SEED = {D1H_BUF: b"".join(le32(0xEEEEEE00 + i) for i in range(10))}


def d1h_vis_regs(p1, p2, p3):
    """0x181D0: the three values and the five out pointers (p4 = +0, p5 = +8, p6 = +0x10,
    p7 = +0x18, p8 = +0x20 of D1H_BUF, a sentinel dword between each)."""
    return {"eax": p1 & 0xFFFFFFFF, "edx": p2 & 0xFFFFFFFF, "ebx": p3 & 0xFFFFFFFF, "ecx": D1H_BUF,
            "s0": D1H_BUF + 8, "s1": D1H_BUF + 0x10, "s2": D1H_BUF + 0x18, "s3": D1H_BUF + 0x20}


# 0x15FD4/0x1617C: (bit, rows, arg5).
D1H_SHIFT_ARGS = ((0, 5, 5), (3, 5, 5), (8, 5, 5), (13, 5, 5), (40, 5, 5), (41, 5, 5), (0xFFFFFFFF, 5, 5),
                  (0, 0, 0), (3, 5, 0), (3, 5, 6), (3, 5, 0xFFFFFFFB), (3, 1, 1), (7, 0x25, 0x25),
                  (0x128, 0x25, 0x25), (0x127, 0x25, 0x25), (16, 2, 2), (9, 2, 2), (0, 1, 1), (1, 3, 3),
                  (20, 4, 4))
D1H_DST = 0x0010A0C0


def d1h_shift_regs(bit, rows, a5):
    return {"eax": bit, "edx": D1H_BUF, "ebx": rows, "ecx": D1H_DST, "s0": a5}


def d1h_shift_case(seed):
    """The source buffer (d1h_buf_case, 0x10A030..0x10A08F) and the destination's 0x60 bytes from
    0x10A090, every byte with its top bit set and different from its neighbours."""
    p = d1h_buf_case(seed)
    for o in (0, 0x30):
        p[0x0010A090 + o] = bytes(((i * 29 + seed * 5 + 7) & 0xFF) | 0x80 for i in range(o, o + 0x30))
    return p


D1H_SPECS = [
    # 0x10F28 attract_voice_tick: no register argument. 0x2C3FC (voice) and 0x5D7DC (rng) are stubs;
    # the rng stub's EAX is the pick (`test eax,eax` 0x10F7B) and both reloads (`add eax,..; mov
    # word,ax`), so it varies per case: 0, 1, 0x100 (AL = 0, EAX != 0) and 0x12345 (the word store).
    Spec("attract_voice_tick", 0x10F28, [
        Case("a0", {"eax": 0x11223344}, d1h_vt_case(5, 7), {0x5D7DC: 1}),
        Case("a1", {}, d1h_vt_case(1, 1), {0x5D7DC: 0}),
        Case("a2", {}, d1h_vt_case(1, 1), {0x5D7DC: 1, 0x2C3FC: 0}),
        Case("a3", {}, d1h_vt_case(0x8000, 0), {0x5D7DC: 0x100}),
        Case("a4", {}, d1h_vt_case(0, 2), {0x5D7DC: 0x12345}),
        Case("a5", {}, d1h_vt_case(0x8001, 0x8001), {0x5D7DC: 0}),
    ], calls=(VOICE, D1H_RNG), eax_mask=0,
       mutants=("@signed", "@zero", "@novoice", "@reload", "@early", "@al", "@pick", "@store")),
    # 0x12750 camera_dust_list_init: no argument, no read of prior state; 0x249C0 runs on both sides
    # and is recorded (its eight (0xF0A78, node) calls and the memory at each). The cases differ only
    # in the garbage the lists hold before.
    Spec("camera_dust_list_init", 0x12750, [
        Case("l0", {"eax": 0x55667788}, d1h_fill(0x000F0A70, 0x80, 0xA5)),
        Case("l1", {}, d1h_fill(0x000F0A70, 0x80, 0x00)),
    ], calls=(D1H_INSERT_BEFORE,), eax_mask=0,
       mutants=("@bound", "@stride", "@after", "@sentinel", "@inuse", "@late")),
    # 0x1282C camera_dust_spawn: no argument. The rng runs on both sides (three draws from the
    # case's seed); 0x2AE14 is a stub returning the record whose +0x34 word the raw stores.
    Spec("camera_dust_spawn", 0x1282C, [
        Case("p0", {}, d1h_dust_case(0xFFC0, 2, 0x12345678), {0x2AE14: D1H_REC}),       # draw 4
        Case("p1", {}, d1h_dust_case(0x0000, 4, 0xFFFFF000), {0x2AE14: D1H_REC + 0x80}),  # draw 0
        Case("p2", {}, d1h_dust_case(0x0040, 0xD, 0x00008000), {0x2AE14: D1H_REC}),     # draw 4
        Case("p3", {}, d1h_dust_case(0x0000, 8, 0x7FFFF000), {0x2AE14: D1H_REC}),       # draw 0
        Case("p4", {}, d1h_dust_case(0x0000, 7, 0), {0x2AE14: D1H_REC}),                # draw 1
        Case("p5", {}, d1h_dust_case(0x0000, 3, 0), {0x2AE14: D1H_REC}),                # draw 2
        Case("p6", {}, d1h_dust_case(0x0000, 1, 0), {0x2AE14: D1H_REC}),                # draw 6
        Case("p7", {}, d1h_dust_case(0x0001, 2, 0), {0x2AE14: D1H_REC}),                # gate
        Case("p8", {}, d1h_dust_case(0x0020, 2, 0), {0x2AE14: D1H_REC}),                # gate, bit 5
    ], calls=(SPAWN, D1H_RNG_REAL), eax_mask=0,
       mutants=("@gate", "@draw", "@arm", "@flag", "@order", "@cam", "@r1", "@a4", "@desc",
                "@width", "@field")),
    # 0x13ADC effects_init: no argument. The lock byte 0x9AF3C is 1 during the build (seen in the
    # memory at each recorded 0x249C0 call) and 0 after; the image holds 1 there, so e0 pokes 0.
    Spec("effects_init", 0x13ADC, [
        Case("e0", {"eax": 0x99887766}, d1h_effects_case(0x00, 0xA5)),
        Case("e1", {}, d1h_effects_case(0x01, 0x3C)),
    ], calls=(D1H_INSERT_BEFORE,), eax_mask=0,
       mutants=("@bound", "@stride", "@after", "@sentinel", "@active", "@unlock", "@flag")),
    # 0x12C70 camera_step_seed: `mov word [0xf0afc],0x400`. The mode byte 0xF0AFE and the index byte
    # 0xF0AFF sit in the same dword and must survive.
    Spec("camera_step_seed", 0x12C70, [
        Case("s0", {"eax": 0x12345678}, d1h_cam(step=0xFFFF)),
        Case("s1", {}, d1h_cam(step=0x0000, mode=0x00, idx=0x00)),
    ], eax_mask=0, mutants=("@width", "@byte", "@value")),
    # 0x12C7C camera_x_commit: EAX = the target x. The step is the zero-extended word 0xF0AFC
    # (`xor edx,edx; mov dx,..`): 0x8000 is +32768. Snap on |diff| <= step (`jg` 0x12CA1), reseeding
    # the step to 0x400; else step toward the target and keep the step.
    Spec("camera_x_commit", 0x12C7C, [
        Case("c0", {"eax": 0x00001100}, d1h_cam(x=0x00001000, step=0x0500)),
        Case("c1", {"eax": 0x00003000}, d1h_cam(x=0x00001000, step=0x0300)),
        Case("c2", {"eax": 0xFFFFF000}, d1h_cam(x=0x00001000, step=0x0300)),
        Case("c3", {"eax": 0x00000300}, d1h_cam(x=0, step=0x0300)),
        Case("c4", {"eax": 0x00000301}, d1h_cam(x=0, step=0x0300)),
        Case("c5", {"eax": 0x00007000}, d1h_cam(x=0, step=0x8000)),
        Case("c6", {"eax": 0xFFFFFD00}, d1h_cam(x=0, step=0x0300)),
        Case("c7", {"eax": 0xFFFFFCFF}, d1h_cam(x=0, step=0x0300)),
        Case("c8", {"eax": 0x12345678}, d1h_cam(x=0x12345678, step=0x0000)),
    ], eax_mask=0, mutants=("@sext", "@ge", "@noseed", "@dir", "@abs", "@reseed", "@width")),
    # 0x12DF0 camera_mode_track_player: no argument. rec = [0x1077A8 + byte[0xF0AFF]*4] (zero-
    # extended: `xor eax,eax; mov al,..`), target = [rec+0x34], dead zone 0x1800 (`jl` 0x12E1D).
    Spec("camera_mode_track_player", 0x12DF0, [
        Case("k0", {}, d1h_track_case(0, 0x00010000, 0x000117FF)),
        Case("k1", {}, d1h_track_case(0, 0x00010000, 0x00011800)),
        Case("k2", {}, d1h_track_case(0, 0x00010000, 0x00011801)),
        Case("k3", {}, d1h_track_case(1, 0x00010000, 0x0000E7FF)),
        Case("k4", {}, d1h_track_case(1, 0xFFFF0000, 0xFFFF4000)),
        Case("k5", {}, d1h_track_case(0x80, 0x00010000, 0x00013000)),
        Case("k6", {}, d1h_track_case(0, 0x00010000, 0x0000E800)),
    ], eax_mask=0, mutants=("@sext", "@table", "@field", "@zone", "@dir", "@snap")),
    # 0x1324C camera_shake_decay: no argument (update-table entry 0, `call [eax+0xa8644]` 0x24CEF).
    # The clear arm needs vel < 0 (`jge` 0x13261) and off + vel <= 0 as a word (`jg` 0x13266).
    Spec("camera_shake_decay", 0x1324C, [
        Case("y0", {}, d1h_shake_case(0x0100, 0x0010, 0xFF)),
        Case("y1", {}, d1h_shake_case(0xFFE0, 0x0010, 0xFF)),
        Case("y2", {}, d1h_shake_case(0xFFE0, 0x0020, 0x01)),
        Case("y3", {}, d1h_shake_case(0xFFE0, 0x0021, 0xFF)),
        Case("y4", {}, d1h_shake_case(0x0000, 0xFFFB, 0xFF)),
        Case("y5", {}, d1h_shake_case(0x7FFF, 0x0001, 0xFF)),
        Case("y6", {}, d1h_shake_case(0x8000, 0x8000, 0x03)),
        Case("y7", {}, d1h_shake_case(0x0010, 0xFFE0, 0xFF)),
    ], eax_mask=0, mutants=("@sum", "@vel", "@flag", "@keepvel", "@keepoff", "@dec", "@early", "@swap")),
    # 0x13290 camera_mode_center_two: no argument. Target = the midpoint of the two records' +0x18;
    # |diff| > 0x100 steps by 0x40 unless |cam| == 0x5D00 (settle); else settle when the camera y
    # 0xF0AEC is 0. Settling writes mode 4 only when the byte 0x1078FE is non-zero.
    Spec("camera_mode_center_two", 0x13290, [
        Case("t0", {}, d1h_center_case(0x1000, 0x1100, 0x0F00, y=0, gate=1)),
        Case("t1", {}, d1h_center_case(0x1000, 0x0F00, 0x1100, y=5, gate=1)),
        Case("t2", {}, d1h_center_case(0x5D00, 0x1000, 0x1000, y=0, gate=0)),
        Case("t3", {}, d1h_center_case(0xFFFFA300, 0x1000, 0x0800, y=7, gate=0x80)),
        Case("t4", {}, d1h_center_case(0x1000, 0x3000, 0x2000, y=0, gate=1)),
        Case("t5", {}, d1h_center_case(0x1000, 0xFFFFF000, 0xFFFFE000, y=0, gate=1)),
        Case("t6", {}, d1h_center_case(0x1000, 0x1100, 0x1100, y=0, gate=1)),
        Case("t7", {}, d1h_center_case(0x1000, 0x1101, 0x1101, y=0, gate=1)),
        Case("t8", {}, d1h_center_case(0x1000, 0x1003, 0x1000, y=0, gate=0)),
        Case("t9", {}, d1h_center_case(0x5D00, 0x5D80, 0x5D40, y=0, gate=1)),
        Case("t10", {}, d1h_center_case(0x1000, 0x0F00, 0x0F03, y=0x10000, gate=1)),
        # the midpoint, not either record: p0 is at the camera (settle if it were the target)
        Case("t11", {}, d1h_center_case(0x1000, 0x1000, 0x3000, y=0, gate=1)),
        Case("t12", {}, d1h_center_case(0x1000, 0x3000, 0x1000, y=0, gate=1)),
    ], eax_mask=0, mutants=("@field", "@slot", "@mid", "@abs", "@ge", "@edge", "@dir", "@step",
                            "@y", "@gate", "@mode")),
    # 0x1333C camera_mode_center_one: no argument. Target = [[0x1077B0 + byte[0x10810D]*0x94]+0x18];
    # as mode 2 but the settle is not gated on 0x1078FE.
    Spec("camera_mode_center_one", 0x1333C, [
        Case("o0", {}, d1h_center_case(0x1000, 0x1080, 0x7777, y=0, gate=0, idx=0)),
        Case("o1", {}, d1h_center_case(0x1000, 0x7777, 0x0F80, y=5, gate=1, idx=1)),
        Case("o2", {}, d1h_center_case(0x5D00, 0x1000, 0x7777, y=0, gate=0, idx=0)),
        Case("o3", {}, d1h_center_case(0xFFFFA300, 0x7777, 0x1000, y=7, gate=0, idx=1)),
        Case("o4", {}, d1h_center_case(0x1000, 0x3000, 0x0000, y=0, gate=0, idx=0)),
        Case("o5", {}, d1h_center_case(0x1000, 0x2000, 0xFFFFF000, y=0, gate=0, idx=1)),
        Case("o6", {}, d1h_center_case(0x1000, 0x1100, 0x7777, y=0, gate=0, idx=0)),
        Case("o7", {}, d1h_center_case(0x1000, 0x1101, 0x7777, y=0, gate=0, idx=0)),
        Case("o8", {}, d1h_center_case(0x5D00, 0x5D80, 0x7777, y=0, gate=1, idx=0)),
        Case("o9", {}, d1h_center_case(0x1000, 0x7777, 0x7777, y=0, gate=0, idx=2, third=0x1040)),
    ], eax_mask=0, mutants=("@field", "@idx", "@abs", "@ge", "@edge", "@dir", "@step", "@y",
                            "@gate", "@mode")),
    # 0x12FD8 camera_pair_hold: AL = track (`test al,al` 0x12FE1). 0x18714 is a stub: its EAX goes to
    # the pulled slot's record +0x18 (0x1308E, 0x130EC), its argument is the pulled side. The left
    # slot a is slot 1 when slot0.x >= slot1.x (`setge` 0x12FF4). M = the centre 0x108884.
    Spec("camera_pair_hold", 0x12FD8, [
        Case("h0", {"eax": 0}, d1h_hold_case(0x10000, 0x7777, (0, 0), (0, 0))),
        Case("h1", {"eax": 0x100}, d1h_hold_case(0x10000, 0x7777, (0, 0), (0, 0))),
        # both far bands pull (L: x < latch, R: x > latch); the camera is clamped high
        Case("h2", {"eax": 1}, d1h_hold_case(0x10000, 0x12000, (-0x3000, -0x2F00), (0x3000, 0x2F00)),
             {0x18714: 0x11111111}),
        # slot 1 is the left one; far bands with x == latch: no pull; the camera is clamped low
        Case("h3", {"eax": 0xFF01}, d1h_hold_case(-0x10000, -0x12000, (0x3000, 0x3000), (-0x3000, -0x3000)),
             {0x18714: 0x22222222}),
        # between the bands: nothing; the camera is inside the clamp
        Case("h4", {"eax": 1}, d1h_hold_case(0x10000, 0x10000, (-0x1000, -0x2000), (0x1000, 0x2000))),
        # both inner bands pull (L: x > latch, R: x < latch); the camera exactly at the high clamp
        Case("h5", {"eax": 1}, d1h_hold_case(-0x10000, -0x10000 + 0x1500, (-0x100, -0x200), (0x100, 0x200)),
             {0x18714: 0x33333333}),
        # inner bands with x == latch: no pull; the camera exactly at the low clamp
        Case("h6", {"eax": 1}, d1h_hold_case(0x10000, 0x10000 - 0x1500, (-0x100, -0x100), (0x100, 0x100))),
        # x exactly at the far band edges (M - 0x2E80, M + 0x2E80): the middle arm, no pull
        Case("h7", {"eax": 1}, d1h_hold_case(0x10000, 0x10000, (-0x2E80, -0x2000), (0x2E80, 0x2000))),
        # x exactly at the inner band edges (M - 0xA80, M + 0xA80): the inner arm, both pull
        Case("h8", {"eax": 1}, d1h_hold_case(0x10000, 0x10000, (-0xA80, -0x1000), (0xA80, 0x1000)),
             {0x18714: 0x44444444}),
        # equal x: slot 1 is the left one (setge); it alone pulls
        Case("h9", {"eax": 1}, d1h_hold_case(0x10000, 0x10000, (0, 0x7000), (0, -0x100)),
             {0x18714: 0x55555555}),
        # slot 1 left, both pull
        Case("h10", {"eax": 1}, d1h_hold_case(0x10000, 0x10000, (0x3000, 0x2F00), (-0x3000, -0x2F00)),
             {0x18714: 0x66666666}),
        # one past each edge: the far bands (x = M -+ 0x2E81) and no pull by the latch
        Case("h11", {"eax": 1}, d1h_hold_case(0x10000, 0x10000 + 0x1501, (-0x2E81, -0x2E82), (0x2E81, 0x2E82))),
        Case("h12", {"eax": 1}, d1h_hold_case(0x10000, 0x10000 - 0x1501, (-0xA81, -0x2000), (0xA81, 0x2000))),
    ], calls=(C1_RECORD_X,), eax_mask=0,
       mutants=("@al", "@ge", "@band", "@inner", "@latch", "@x34", "@x2c", "@rec", "@side",
                "@clamp", "@lo", "@rinner")),
    # 0x2C9B8 config_voice_gate: EAX = v, signed (`jge` 0x2C9BA). The voice stub's EAX is
    # overwritten (`mov eax,0x10000` 0x2C9C3); both exits load the whole EAX, so the mask is full.
    Spec("config_voice_gate", 0x2C9B8, [
        Case("g0", {"eax": 0}, {}, {0x2C3FC: 0x77}),
        Case("g1", {"eax": 1}, {}, {0x2C3FC: 0x77}),
        Case("g2", {"eax": 0x7FFFFFFF}, {}, {0x2C3FC: 0x77}),
        Case("g3", {"eax": 0x80000000}, {}, {0x2C3FC: 0x77}),
        Case("g4", {"eax": 0xFFFFFFFF}, {}, {0x2C3FC: 0}),
    ], calls=(VOICE,), eax_mask=0xFFFFFFFF,
       mutants=("@gt", "@never", "@novoice", "@id", "@ret", "@val")),
    # 0x17EEC camera_char_const: EAX = side; the character is the slot's +0x7A byte (`cmp al,6; ja`
    # 0x17F00, unsigned), the jump table 0x17ED0 picks one of seven words (the image's own values,
    # all different: 0x0EE4, 0x12A2, 0x0BD4, 0x16B5, 0x1F9E, 0x2F19, 0x32D7). Every exit is
    # `xor eax,eax; mov ax,..`: full mask.
    Spec("camera_char_const", 0x17EEC, [
        Case("n%d_%X" % (side, ch), {"eax": side}, d1h_char_case(side, ch))
        for side, ch in ((0, 0), (1, 1), (0, 2), (1, 3), (0, 4), (1, 5), (0, 6), (1, 7), (0, 0x80),
                         (1, 0xFF), (1, 0), (0, 1))
    ], eax_mask=0xFFFFFFFF, mutants=("@side", "@index", "@bound", "@default", "@width")),
    # 0x16D58 camera_screen_base: AX = side, DX = character, both words (`test ax,ax; jl`, `movsx`).
    Spec("camera_screen_base", 0x16D58, [
        Case("b0", {"eax": 0, "edx": 0}, D1H_BASE_SEED),
        Case("b1", {"eax": 1, "edx": 9}, D1H_BASE_SEED),
        Case("b2", {"eax": 0, "edx": 0xA}, D1H_BASE_SEED),
        Case("b3", {"eax": 2, "edx": 3}, D1H_BASE_SEED),
        Case("b4", {"eax": 0xFFFF, "edx": 1}, D1H_BASE_SEED),
        Case("b5", {"eax": 1, "edx": 0xFFFF}, D1H_BASE_SEED),
        Case("b6", {"eax": 0x00010000, "edx": 0x00010005}, D1H_BASE_SEED),
        Case("b7", {"eax": 0x8000, "edx": 2}, D1H_BASE_SEED),
        Case("b8", {"eax": 1, "edx": 0x8003}, D1H_BASE_SEED),
        Case("b9", {"eax": 0xFFFF0001, "edx": 0xFFFF0007}, D1H_BASE_SEED),
    ], eax_mask=0, mutants=("@side16", "@char16", "@bound", "@sbound", "@mul", "@swap", "@stride",
                            "@second")),
    # 0x164C0 camera_frame_last: EAX = side. `fld [rec+0x24]; fadd [0x80580]; fcomp [rec+0x20];
    # sete al`: AL only (both callers `test al,al`, 0x165E6 and 0x16C53). The sum is held at the
    # x87's 53-bit precision, so f3/f4/f5 (sums a float cannot hold: 1e-10 - 1, 2^24 + 1, 2^24 + 3)
    # separate it from a float sum; `sete` reads ZF, which an unordered compare sets too (f9, f10:
    # a NaN on either side gives 1).
    Spec("camera_frame_last", 0x164C0, [
        Case("f0", {"eax": 0}, d1h_last_case(0, 3.0, 2.0)),
        Case("f1", {"eax": 1}, d1h_last_case(1, 3.0, 3.0)),
        Case("f2", {"eax": 0}, d1h_last_case(0, 1.0, 0.0, other=(5.0, 4.0))),
        Case("f3", {"eax": 0}, d1h_last_case(0, 1e-10, -1.0)),
        Case("f4", {"eax": 1}, d1h_last_case(1, 16777218.0, 16777216.0)),
        Case("f5", {"eax": 0}, d1h_last_case(0, 16777220.0, 16777220.0)),
        Case("f6", {"eax": 1}, d1h_last_case(1, 2.0, 3.0)),
        Case("f7", {"eax": 0}, d1h_last_case(0, 7.0, 6.0, const=1.0)),
        Case("f8", {"eax": 0}, d1h_last_case(0, 5.0, 6.0, const=1.0)),
        Case("f9", {"eax": 0}, d1h_last_case(0, float("nan"), 2.0)),
        Case("f10", {"eax": 1}, d1h_last_case(1, 3.0, float("nan"))),
    ], eax_mask=0xFF, mutants=("@side", "@swap", "@const", "@float", "@ge", "@nan")),
    # 0x16734 camera_sprite_code: EAX = side. 0x33A10 (the context) runs on both sides; the actor's
    # sprite id is word[[0x1014EC] + word[rec+0x56]*0x20] & 0x7FFF (`and ah,0x7f` 0x1675D), the
    # character the slot's +0x7A (`cmp dl,6; ja` 0x1676C, jump table 0x16718). One case per id of
    # every chain and per range edge; the sides and bit 15 alternate. Every exit loads the whole
    # EAX (the caller compares it with -1, 0x16B0F): full mask.
    Spec("camera_sprite_code", 0x16734, [
        Case("i%X_%04X" % (ch, uid), {"eax": n & 1}, d1h_code_case(n & 1, ch, uid, bit15=bool(uid & 1)))
        for n, (ch, uid) in enumerate(D1H_CODE_IDS)
    ], allow_calls=(0x33A10,), eax_mask=0xFFFFFFFF,
       unhit_named={0x1695D: "dead: reached only by the `jne` at 0x1694F, so ZF is clear and the `jne` at 0x1695B is taken",
                    0x1696D: "dead: the same flags reach the `jne` at 0x16967, always taken"},
       mutants=("@side", "@field", "@stride", "@mask", "@ch", "@bound", "@range", "@code", "@miss")),
    # 0x14080 camera_rect_clip: EAX = out, EDX = a, EBX = b (four signed dwords each); EAX = 1 when
    # the intersection is non-empty (`mov eax,1` / `xor eax,eax`: full mask). r0 is the raw caller's
    # shape, out == a (0x1424E/0x14256).
    Spec("camera_rect_clip", 0x14080, [
        Case("r0", *d1h_rect_case((10, 20, 100, 200), (50, 60, 150, 250), alias=True)),
        Case("r1", *d1h_rect_case((50, 60, 150, 250), (10, 20, 100, 200))),
        Case("r2", *d1h_rect_case((0, 0, 10, 10), (20, 0, 30, 10))),
        Case("r3", *d1h_rect_case((0, 0, 10, 10), (0, 20, 10, 30))),
        Case("r4", *d1h_rect_case((0, 0, 10, 10), (10, 0, 20, 10))),
        Case("r5", *d1h_rect_case((0, 0, 10, 10), (0, 10, 10, 20), alias=True)),
        Case("r6", *d1h_rect_case((-10, -10, 10, 10), (5, 5, 20, 20))),
        Case("r7", *d1h_rect_case((5, 5, 20, 20), (-10, -10, 10, -1))),
        Case("r8", *d1h_rect_case((3, 4, 5, 6), (3, 4, 5, 6), alias=True)),
        Case("r9", *d1h_rect_case((0, 0, 10, 10), (11, 0, 20, 10))),
    ], eax_mask=0xFFFFFFFF, mutants=("@unsigned", "@min", "@top", "@strict", "@y", "@input")),
    # 0x15EC0 camera_bitrev: EAX = len (signed: `sar`, `jle` 0x15EDD, `idiv` 0x15F0D), EDX = buf.
    # The table 0xA1432 is the image's.
    Spec("camera_bitrev", 0x15EC0, [
        Case("v%d" % i, {"eax": n, "edx": D1H_BUF}, d1h_buf_case(i))
        for i, n in enumerate((0, 1, 2, 5, 8, 0x25, 0x26, 0xFFFFFFFF, 0xFFFFFFFE, 0xFFFFFFFD))
    ], eax_mask=0, mutants=("@off", "@half", "@table", "@noswap", "@mid")),
    # 0x1638C camera_sprite_row: EAX = side (zero or not, `test eax,eax; jne` 0x163A5), EDX = dst,
    # EBX = count, ECX = row; the copy is `shr ecx,2; rep movsd; and cl,3; rep movsb` (the F2 prefix).
    Spec("camera_sprite_row", 0x1638C, [
        Case("w%d" % i, {"eax": side, "edx": D1H_BUF, "ebx": n, "ecx": row}, d1h_row_case(row, i))
        for i, (side, row, n) in enumerate(((0, 0, 0x25), (1, 3, 0x26), (2, 1, 5), (0, 2, 0), (0, 5, 1),
                                            (1, 0, 3), (0, 1, 4), (0xFFFFFFFF, 2, 7), (0, 191, 0x26)))
    ], eax_mask=0, mutants=("@side", "@base", "@row", "@stride", "@count")),
    # 0x15F48 camera_bitplane_pixel: EAX = bit (signed), EDX = rows (`shl edx,3`), EBX = dst, CL =
    # value. 0x61A70 (the libc memset wrapper) and its body 0x65490/0x654C7 run on the original side;
    # the mask byte is 0xA1420[bit % 8].
    Spec("camera_bitplane_pixel", 0x15F48, [
        Case("x%d" % i, {"eax": bit, "edx": rows, "ebx": D1H_BUF, "ecx": ecx}, d1h_buf_case(i))
        for i, (bit, rows, ecx) in enumerate((
            (0, 0x25, 0xFF), (8, 0x25, 0xFF), (13, 0x25, 0xFF), (7, 0x25, 0x00), (0x128, 0x25, 0xFF),
            (0x129, 0x25, 0xFF), (0xFFFFFFF8, 0x25, 0xFF), (0x127, 0x25, 0x123456A5), (16, 2, 0x11),
            (17, 2, 0x11), (9, 0x20000001, 0x77), (8, 0x20000001, 0x77), (0x23, 5, 0xC3)))
    ], allow_calls=(0x61A70, 0x65490, 0x654C7), eax_mask=0,
       mutants=("@value", "@rows", "@bound", "@mask", "@fill", "@idx", "@aligned")),
    # 0x181D0 camera_sync_visible: EAX = p1 (the item's extent), EDX = p2 (the container's), EBX = p3
    # (the item's start, signed), ECX = p4 and the four stack dwords p5..p8 are out pointers
    # (`ret 0x10`). AL only (`mov al,N` / `xor al,al`; every caller `and eax,0xff`).
    Spec("camera_sync_visible", 0x181D0, [
        Case("z%d" % i, d1h_vis_regs(p1, p2, p3), D1H_VIS_SEED)
        for i, (p1, p2, p3) in enumerate((
            (0, 5, 0), (5, 0, 0), (-3, 5, 0), (5, -1, 0), (5, 9, -5), (5, 9, -4), (10, 5, -2),
            (10, 20, 30), (10, 8, 3), (10, 20, 3), (10, 8, 8), (4, 4, 0), (1, 1, 0), (10, 13, 3),
            (10, 8, -2), (10, 20, -2), (5, 9, -6), (10, 9, 8)))
    ], eax_mask=0xFF, mutants=("@p57", "@p68", "@p1", "@code", "@neg", "@gt", "@rest", "@vis", "@ret")),
    # 0x15FD4 camera_bitplane_shift: EAX = bit, EDX = src, EBX = rows, ECX = dst, one stack dword
    # (arg5, `ret 4`), AL = 1 when done. bit == 0x128 leaves the bit index [esp+0x14] unset in the raw
    # (0x1603B..0x1604A); with rows = 0x25 the byte shift 0x25 then zeroes every dst byte whatever it
    # held, so that one case is defined; rows > 0x25 at bit 0x128 is not cased (named in the record).
    # Both raw callers shift in place (EDX = ECX = 0x100BD3, 0x173DA/0x173E4 and 0x17A53/0x17A5D) with
    # a byte bit count (`xor eax,eax; mov al,..`): the p cases are that shape, dst == src.
    Spec("camera_bitplane_shift", 0x15FD4, [
        Case("j%d" % i, d1h_shift_regs(bit, rows, a5), d1h_shift_case(i))
        for i, (bit, rows, a5) in enumerate(D1H_SHIFT_ARGS)
    ] + [
        Case("p%d" % i, {**d1h_shift_regs(bit, rows, rows), "ecx": D1H_BUF}, d1h_shift_case(i + 0x20))
        for i, (bit, rows) in enumerate(((0, 5), (3, 5), (8, 5), (13, 5), (0x23, 5), (0xFF, 0x25), (7, 1)))
    ], eax_mask=0xFF, mutants=("@bound", "@arg5", "@mask", "@shift", "@edge", "@tail", "@zero", "@dir")),
    # 0x1617C camera_bitplane_merge: the left twin, same registers; bit == 0x128 sets the bit index to
    # 0 explicitly (0x161E2/0x161E8), so rows = 0x26 at bit 0x128 is cased here.
    Spec("camera_bitplane_merge", 0x1617C, [
        Case("m%d" % i, d1h_shift_regs(bit, rows, a5), d1h_shift_case(i))
        for i, (bit, rows, a5) in enumerate(D1H_SHIFT_ARGS + ((0x128, 0x26, 0x26),))
    ], eax_mask=0xFF, mutants=("@bound", "@arg5", "@mask", "@shift", "@edge", "@tail", "@zero", "@dir")),
]
D1_SPECS += D1H_SPECS


# ---- track D1 batch d1i (record 2026-10-11-reverse-d1i): config.c, menu.c, nameentry.c, svcmenu.c,
# gfx.c, input.c, render.c. Every poke stays inside the image; the scratch strings are in its zero BSS.

D1I_WORD, D1I_WORD2, D1I_STR = 0x0010A000, 0x0010A020, 0x0010A040


def d1i_nfw_case(word, text, tol=2):
    """0x13F68: the word and the name (both NUL-terminated), the tolerance byte 0xFD0F0 with its
    neighbour 0xFD0F1 (a word read differs), and sentinels in the scratch globals 0xFD0F2 (word),
    0xFD0F4 (dword) and 0xFD0F8 (dword: another word in mem[], so a dropped store restarts on it)."""
    return {D1I_WORD: word + b"\x00", D1I_WORD2: b"zz\x00", D1I_STR: text + b"\x00",
            0x000FD0F0: bytes([tol, 0x01]) + le16(0xA55A) + le32(0x0010A0C0) + le32(D1I_WORD2)
                        + b"\x77" * 4}


D1I_NFW = {"eax": D1I_WORD, "edx": D1I_STR}

D1I_KREC, D1I_BIOS, D1I_BIOS2 = 0x0010A000, 0x0010A400, 0x0010A800
D1I_KMIRROR, D1I_KBIOS_PTR, D1I_KDEFAULTS = 0x001014AC, 0x00101514, 0x000A2C62


def d1i_krec(seed):
    """A 0x28-byte key-config record whose bytes are all distinct (so a wrong offset shows)."""
    return bytes((seed + i * 5) & 0xFF for i in range(0x28))


def d1i_keys_case(seed, bios=D1I_BIOS, rec=D1I_KREC, alias=None):
    """0x1AE28/0x1AF64: the record at `rec`, the BIOS pointer 0x101514 and sentinels in the two
    destinations (the BIOS record's +0x2D0..+0x2EF and the 0x28-byte mirror at 0x1014AC). `alias`
    = a pointer: the BIOS record is placed so that its scan bytes +0x2DE..+0x2E1 ARE the pointer
    0x101514, and the first four key words carry `alias` in their scan bytes, so the pointer the raw
    re-reads (0x1AE8D, 0x1AFA1) is no longer the one it started with. Their ASCII bytes carry
    `alias` + 0x100, so a mutant that stores the wrong byte still leaves a pointer inside mem[]."""
    r = bytearray(d1i_krec(seed))
    p = {D1I_KMIRROR: bytes(0xC0 ^ i for i in range(0x28))}
    if alias is None:
        p[bios + 0x2D0] = bytes(0xE0 + i for i in range(0x20))
    else:
        bios = D1I_KBIOS_PTR - 0x2DE
        for i, b in enumerate(le32(alias)):
            r[3 + 2 * i] = b
            r[2 + 2 * i] = le32(alias + 0x100)[i]     # the ASCII bytes: a pointer in mem[] too
        p[bios + 0x2D0] = bytes(0xE0 + i for i in range(0x0E))
        p[alias + 0x2E0] = bytes(0xD0 + i for i in range(0x10))
        p[alias + 0x3E0] = bytes(0xB0 + i for i in range(0x10))
    p[rec] = bytes(r) + b"\x3c\x3d"
    p[D1I_KBIOS_PTR] = le32(bios)
    return p


D1I_R_LIST = 0x00105B44        # the render list head, the EAX of both callers of 0x1C3FC


def d1i_sort_case(layers):
    """0x1C3FC: a render list of len(layers) nodes ({next, pset}, 8 bytes each, at 0x10A000) whose
    psets (0x10A100 + i*0x10) carry the layer word at +0xE and a decoy word at +0xC in descending
    order (a wrong offset reverses the list)."""
    p = {D1I_R_LIST: le32(0x0010A000 if layers else 0)}
    for i, ly in enumerate(layers):
        n, ps = 0x0010A000 + i * 8, 0x0010A100 + i * 0x10
        p[n] = le32(n + 8 if i + 1 < len(layers) else 0) + le32(ps)
        p[ps + 0x0C] = le16(0x100 - i) + le16(ly)
    return p


def d1i_key_case(wr, rd, cls=None):
    """0x20860: the ring's write index 0x1044BC and read index 0x1044C8, the 17 ring dwords
    0x104458.. seeded, the flag byte 0x1044D8 with its three neighbours; `cls` = {ascii: class
    byte} pokes the C runtime's table at 0x81C84 + (u8)(c + 1)."""
    p = {0x001044BC: le32(wr), 0x001044C8: le32(rd),
         0x00104458: bytes(0xA0 + i for i in range(0x40)), 0x00104498: bytes(0xE0 + i for i in range(4)),
         0x001044D8: b"\x5a\x5b\x5c\x5d"}
    for c, v in (cls or {}).items():
        p[0x00081C84 + ((c + 1) & 0xFF)] = bytes([v])
    return p


D1I_CREDITS, D1I_FREEPLAY, D1I_NODEBIT = 0x00105C00, 0x00105D60, 0x00104B1F
D1I_FIELD29 = 0x00105E14       # config field 0x29 as 0x2D974 reads it: the image's descriptor
#                                0x2D3A4 = 0x0001D980 is 8 nibbles ending at byte 0x105DE1 + 54


def d1i_credit_case(credits, freeplay=0, nodebit=0):
    """The credit dword 0x105C00 with its four neighbours seeded, and the free-play and no-debit
    bytes each followed by three non-zero bytes (a dword read of either differs)."""
    return {D1I_CREDITS: le32(credits) + b"\x5a\x5b\x5c\x5d",
            D1I_FREEPLAY: bytes([freeplay]) + b"\xff\xff\xff",
            D1I_NODEBIT: bytes([nodebit]) + b"\xee\xee\xee"}


D1I_MENU, D1I_MENU_ENT, D1I_MENU_STR = 0x0010A000, 0x0010A100, 0x0010A300


def d1i_menu_case(records):
    """0x2CCD0: a menu-descriptor table at D1I_MENU. `records` = [(presence, shift, count, stars)]:
    each record gets max(count, 4) (4 when count is negative) entries of stride 8 ({char *, a
    second valid pointer}), entry i's string starting with '*' when i is in `stars`. The table
    ends at the first zero presence (the BSS after it is zero)."""
    p = {D1I_MENU_STR: b"*on\x00", D1I_MENU_STR + 8: b"off\x00", D1I_MENU_STR + 0x10: b"+x\x00"}
    ent = D1I_MENU_ENT
    for k, (presence, shift, count, stars) in enumerate(records):
        n = count if 0 < count < 0x20 else 4
        p[D1I_MENU + k * 0x14] = (le32(presence) + le32(shift) + le32(count) + le32(D1I_MENU_STR + 8)
                                  + le32(ent))
        p[ent] = b"".join(le32(D1I_MENU_STR + (0 if i in stars else 8)) + le32(D1I_MENU_STR + 0x10)
                          for i in range(n))
        ent += n * 8
    return p


D1I_HS_DESC, D1I_HS_BASE, D1I_HS_OUT = 0x0002D3FC, 0x0002D478, 0x0010A000


def d1i_hs_case(descs=((10, 4, 8), (1, 4, 0x18), (1, 3, 2)), bases=(0x00105E34, 0x00105EAC, 0x00105EC8)):
    """0x2DB58: the three 8-byte descriptors 0x2D3FC (count word, the unread word +2 seeded, the
    value and name byte counts), the three block bases 0x2D478 + 8*t (the dword +4 between them
    seeded) and the two out dwords with sentinels. The defaults are the image's own values."""
    p = {D1I_HS_OUT: le32(0x5A5A5A5A) + le32(0x6B6B6B6B) + le32(0x7C7C7C7C)}
    for t, (count, vb, nb) in enumerate(descs):
        p[D1I_HS_DESC + t * 8] = le16(count) + le16(0x0101 * (t + 1)) + le16(vb) + le16(nb)
        p[D1I_HS_BASE + t * 8] = le32(bases[t]) + le32(0x00D0D000 + t)
    return p


D1I_STR_HANDLE, D1I_STR_TABLE, D1I_STR_OUT = 0x0010AA00, 0x0010AC00, 0x00102760


def d1i_str_case(groups):
    """The localisation table 0x1C500 decodes (c3g_combo_str_case's form): `groups` is a list of
    lists of byte strings, id = group * 0x40 + entry; each entry is a length byte and the bytes
    XOR that length; a group's header dword +4 is the next group's offset. The 0x102760 output
    buffer is seeded 0x5A."""
    buf, offs = bytearray(8), []
    for gi, g in enumerate(groups):
        if gi > 0:
            offs.append(len(buf))
            buf.extend(b"\x00" * 8)
        for e in g:
            buf.append(len(e) & 0xFF)
            buf.extend(bytes(c ^ (len(e) & 0xFF) for c in e))
    if offs:
        buf[4:8] = le32(offs[0])
    for i in range(len(offs) - 1):
        buf[offs[i] + 4:offs[i] + 8] = le32(offs[i + 1])
    p = {0x001082DC: le32(D1I_STR_HANDLE),
         D1I_STR_HANDLE: b"\x00" * 8 + le32(D1I_STR_TABLE) + le32(0x400) + b"\x00" * 15,
         D1I_STR_OUT: bytes([0x5A]) * 0x40}
    for off in range(0, len(buf), 0x40):
        p[D1I_STR_TABLE + off] = bytes(buf[off:off + 0x40])
    return p


D1I_ENT = 0x0010A000
D1I_ENT_STRINGS = [[b"zero", b"?hid", b"shown", b">x"]]      # ids 0..3


def d1i_find_case(ids, stride):
    """0x2FE40: entries of `stride` bytes at D1I_ENT whose first dword is the string id (the rest
    seeded), and the string table with ids 1 = "?hid", 2 = "shown", 3 = ">x"."""
    p = d1i_str_case(D1I_ENT_STRINGS)
    for i, sid in enumerate(ids):
        p[D1I_ENT + i * stride] = le32(sid) + b"\x77" * (stride - 4)
    return p


def d1i_proj_case():
    """0x4F228: the two flag bytes 0x107A54/55 with two neighbours and the projection words
    0x107A38/3A with a word on each side."""
    return {0x00107A54: b"\x5a\x5b\x5c\x5d", 0x00107A36: bytes(0xA0 + i for i in range(8))}


def d1i_track_case(v, add):
    """0x38990: the scroll dword 0xF0AEC, the two stored words 0x107A3C/0x107A4A with their
    neighbours, and the added word 0x107A4E."""
    return {0x000F0AEC: le32(v), 0x00107A3C: le16(0x5A5A) + le16(0x6B6B),
            0x00107A48: le16(0x7C7C) + le16(0x8D8D) + le16(0x9E9E) + le16(add)}


def d1i_pal_case(seed):
    """0x336C0: the 0x180-byte dirty list 0x107498 and the 0x180-byte ownership table 0x107618,
    both seeded non-zero, a stale list head 0x107798 and a non-zero 0xBD470."""
    p = {0x00107798: le32(0x00107600), 0x000BD470: le32(0x11223344)}
    for k in range(6):
        p[0x00107498 + k * 0x40] = bytes(((seed + k * 0x40 + i) & 0x7F) | 0x01 for i in range(0x40))
        p[0x00107618 + k * 0x40] = bytes(((seed * 3 + k * 0x40 + i) & 0x7F) | 0x80 for i in range(0x40))
    return p


def d1i_pump_case(k8, k9, prev, level, latch, rpt, cnt, first=0x0030, nxt=0x0008):
    """0x500C4: the two key bytes at [0x101514]+0x2D8/9 (a byte on each side seeded), then the
    previous sample 0xE1C30, the level 0xE1C34, the latch 0xE1C38, the repeat mask 0xE1C3C and
    the repeat words 0xE1C40 (counter), 0xE1C42 (first), 0xE1C44 (next) and a neighbour."""
    return {0x00101514: le32(D1I_BIOS), D1I_BIOS + 0x2D7: bytes([0x77, k8, k9, 0x66]),
            0x000E1C30: le32(prev) + le32(level) + le32(latch) + le32(rpt) + le16(cnt) + le16(first)
                        + le16(nxt) + le16(0x9A9A)}


D1I_PT_ACC = 0x0010746C         # the four tick accumulators; [3] = 0x107478 is 0x32B00's snapshot
D1I_PT_AUDIT = ("calls the deferred audit layer (tools/port_classification.txt: 2DAE4/2E934 deferred "
                "record-§48-V), which the port declares a no-op; its add always changes audit bytes")


def d1i_pt_case(acc=(0x11111111, 0x22222222, 0x33333333, 0x44444444), clock=0x00012345):
    """0x32A3C/0x32B00/0x32B4C: the four accumulators 0x10746C.., then the run clock's state in
    the one form where 0x32970 changes no byte: last tick 0x10747C == the ISR tick 0x105D88 (no
    elapsed time), 0x107480 seeded, 0x107484 below 0x3840, 0x107488.. seeded, mode mask
    0x107494 = 0."""
    return {D1I_PT_ACC: b"".join(le32(v) for v in acc) + le32(clock) + le32(0x55555555) + le32(0x00000100)
                        + le32(0x00000200) + le32(0x00000300) + le32(0x00000400) + le32(0),
            0x00105D88: le32(clock)}


def d1i_pack_case(seed, rec=D1I_KREC):
    """0x1AEE0: the mirror (distinct bytes, the unread odd ones included) and a seeded destination."""
    return {D1I_KMIRROR: bytes((seed + i * 9) & 0xFF for i in range(0x28)),
            rec: b"\x5a" * 0x2A}

D1I_SPECS = [
    # 0x13F68 name_filter_word: EAX = word, EDX = str. It calls itself at 0x1403E (allowed: the
    # port recurses in C). AL only: 0x13F2F `or ah,al`; the recursive site reloads EAX (0x14043).
    Spec("name_filter_word", 0x13F68, [
        Case("n0", D1I_NFW, d1i_nfw_case(b"ab", b"")),
        Case("n1", D1I_NFW, d1i_nfw_case(b"ab", b"xyz")),
        Case("n2", D1I_NFW, d1i_nfw_case(b"ab", b"xxab")),
        Case("n3", D1I_NFW, d1i_nfw_case(b"ab", b"aaabbb")),
        Case("n4", D1I_NFW, d1i_nfw_case(b"aab", b"ab")),
        Case("n5", D1I_NFW, d1i_nfw_case(b"a b", b"ab")),
        Case("n6", D1I_NFW, d1i_nfw_case(b"ab", b"a  b")),
        Case("n7", D1I_NFW, d1i_nfw_case(b"ab", b"axb")),
        Case("n8", D1I_NFW, d1i_nfw_case(b"ab", b"axb", tol=0)),
        Case("n9", D1I_NFW, d1i_nfw_case(b"ab", b"axxxb")),
        Case("n10", D1I_NFW, d1i_nfw_case(b"ab", b"a\x00ab")),
        Case("n11", D1I_NFW, d1i_nfw_case(b"a", b"xa")),
        Case("n12", D1I_NFW, d1i_nfw_case(b"ab", b"abab")),
        Case("n13", D1I_NFW, d1i_nfw_case(b"abb", b"abb")),
    ], allow_calls=(0x13F68,), eax_mask=0xFF,
       mutants=("@tolw", "@nodec", "@restart", "@norec", "@ret", "@strrep", "@wordrep", "@wspace",
                "@sspace", "@star", "@word", "@save", "@bxb", "@nul", "@rep2")),
    # 0x1AE28 config_keys_apply: EAX = the record. k2 is the aliasing case (the raw re-reads the
    # BIOS pointer at 0x1AE8D after the first loop's stores). No caller reads EAX (diff_runner.c).
    Spec("config_keys_apply", 0x1AE28, [
        Case("k0", {"eax": D1I_KREC}, d1i_keys_case(0x11)),
        Case("k1", {"eax": D1I_KREC + 0x40}, d1i_keys_case(0x91, bios=D1I_BIOS2, rec=D1I_KREC + 0x40)),
        Case("k2", {"eax": D1I_KREC}, d1i_keys_case(0x23, alias=D1I_BIOS2)),
    ], eax_mask=0,
       mutants=("@devw", "@sext", "@dev", "@swap", "@bioslo", "@count", "@keys2", "@w24", "@w26",
                "@mirror", "@reread")),
    # 0x1AE20: `mov eax,0xa2c62` and the fall into 0x1AE28. EAX at entry is a decoy record; d1
    # pokes the default record itself, so its bytes are read, not known.
    Spec("config_keys_apply_defaults", 0x1AE20, [
        Case("d0", {"eax": D1I_KREC}, d1i_keys_case(0x11)),
        Case("d1", {"eax": D1I_KREC}, {**d1i_keys_case(0x11, bios=D1I_BIOS2),
                                       D1I_KDEFAULTS: d1i_krec(0x47)}),
    ], eax_mask=0, mutants=("@rec", "@eax")),
    # 0x1AEE0 config_keys_pack: EAX = the destination record.
    Spec("config_keys_pack", 0x1AEE0, [
        Case("p0", {"eax": D1I_KREC}, d1i_pack_case(0x21)),
        Case("p1", {"eax": D1I_KREC + 0x40}, d1i_pack_case(0x8D, rec=D1I_KREC + 0x40)),
    ], eax_mask=0, mutants=("@devw", "@dev2", "@order", "@count", "@keys2", "@w24w", "@w26b")),
    # 0x1AF64 config_keys_load: EAX = the record. l2 is the aliasing case (0x1AFA1 re-reads).
    Spec("config_keys_load", 0x1AF64, [
        Case("l0", {"eax": D1I_KREC}, d1i_keys_case(0x11)),
        Case("l1", {"eax": D1I_KREC + 0x40}, d1i_keys_case(0x91, bios=D1I_BIOS2, rec=D1I_KREC + 0x40)),
        Case("l2", {"eax": D1I_KREC}, d1i_keys_case(0x23, alias=D1I_BIOS2)),
    ], eax_mask=0, mutants=("@devw", "@dev2", "@lo", "@count", "@reread", "@keys2")),
    # 0x1C3FC render_list_sort: EAX = the list head pointer (0x105B44 at both call sites; the
    # port hard-wires it). 0x1C3A0 (render_splice, its own row) runs real on both sides. The raw
    # returns only with EAX == 0.
    Spec("render_list_sort", 0x1C3FC, [
        Case("s0", {"eax": D1I_R_LIST}, d1i_sort_case([])),
        Case("s1", {"eax": D1I_R_LIST}, d1i_sort_case([5])),
        Case("s2", {"eax": D1I_R_LIST}, d1i_sort_case([1, 2, 3])),
        Case("s3", {"eax": D1I_R_LIST}, d1i_sort_case([3, 2, 1])),
        Case("s4", {"eax": D1I_R_LIST}, d1i_sort_case([2, 1, 2, 1])),
        Case("s5", {"eax": D1I_R_LIST}, d1i_sort_case([0x8000, 1, 0xFFFF, 0x7FFF])),
        Case("s6", {"eax": D1I_R_LIST}, d1i_sort_case([5, 1, 3, 2, 4, 0])),
        Case("s7", {"eax": D1I_R_LIST}, d1i_sort_case([0x100, 0x0FF, 0x1FE])),
    ], allow_calls=(0x1C3A0,), eax_mask=0xFFFFFFFF,
       mutants=("@signed", "@unlink", "@prev", "@byte", "@off", "@ret")),
    # 0x20860 nameentry_key: EAX = the key's ascii byte. 0x653ED (the runtime's toupper) runs
    # real in the original; the port inlines it. No caller reads EAX (diff_runner.c).
    Spec("nameentry_key", 0x20860, [
        Case("q0", {"eax": 0x61}, d1i_key_case(3, 4)),
        Case("q1", {"eax": 0x08}, d1i_key_case(0, 5)),
        Case("q2", {"eax": 0x0D}, d1i_key_case(1, 5)),
        Case("q3", {"eax": 0x20}, d1i_key_case(2, 5)),
        Case("q4", {"eax": 0x61}, d1i_key_case(4, 0)),
        Case("q5", {"eax": 0x5A}, d1i_key_case(5, 0)),
        Case("q6", {"eax": 0x31}, d1i_key_case(6, 0)),
        Case("q7", {"eax": 0x6D}, d1i_key_case(0xF, 5)),
        Case("q8", {"eax": 0x108}, d1i_key_case(7, 0)),
        Case("q9", {"eax": 0x161}, d1i_key_case(8, 0)),
        Case("q10", {"eax": 0x7B}, d1i_key_case(9, 0, {0x7B: 0x40})),
        Case("q11", {"eax": 0x60}, d1i_key_case(10, 0, {0x60: 0x80})),
        Case("q12", {"eax": 0xFF}, d1i_key_case(11, 0, {0xFF: 0x80})),
        Case("q13", {"eax": 0x62}, d1i_key_case(0x25, 0)),
        Case("q14", {"eax": 0x61}, d1i_key_case(0xF, 0)),
        Case("q15", {"eax": 0x7A}, d1i_key_case(12, 0)),
    ], allow_calls=(0x653ED,), eax_mask=0,
       mutants=("@full", "@bs", "@enter", "@space", "@idx", "@class", "@upper", "@base", "@mask",
                "@wstore", "@fullflag", "@flagw", "@up8", "@byte")),
    # 0x2BF00 config_set_credit_row_init: `mov byte [0x105c05],0x1d`. EAX is not touched and its
    # one caller reloads it (0x20CD1).
    Spec("config_set_credit_row_init", 0x2BF00, [
        Case("r0", {"eax": 0x12345678}, {0x00105C04: b"\x5a\x5b\x5c\x5d"}),
    ], eax_mask=0, mutants=("@val", "@width", "@addr")),
    # 0x2C304 config_credits_init: 0x2D974(0x29) runs real on both sides and is recorded (its own
    # row); field 0x29 is the dword D1I_FIELD29. EAX = the stored dword (diff_runner.c).
    Spec("config_credits_init", 0x2C304, [
        Case("i0", {}, {**d1i_credit_case(0xA5A5A5A5), D1I_FIELD29: le32(0x00050000)}),
        Case("i1", {}, {**d1i_credit_case(0xA5A5A5A5), D1I_FIELD29: le32(0xFFFFFFFF)}),
        Case("i2", {}, {**d1i_credit_case(0xA5A5A5A5), D1I_FIELD29: le32(0x12345678)}),
        Case("i3", {}, {**d1i_credit_case(0xA5A5A5A5), D1I_FIELD29: le32(0x00000000)}),
        Case("i4", {}, {**d1i_credit_case(0xA5A5A5A5), D1I_FIELD29: le32(0xFFF0FFFF)}),
    ], calls=(E.Call(0x2D974, ("eax",), mode="real"),), eax_mask=0xFFFFFFFF,
       mutants=("@field", "@mask", "@shift", "@inc", "@width")),
    # 0x2CA2C config_has_credit: `or eax,edx; setne al; and eax,0xff` (the whole register).
    Spec("config_has_credit", 0x2CA2C, [
        Case("h0", {}, d1i_credit_case(0, 0)),
        Case("h1", {}, d1i_credit_case(1, 0)),
        Case("h2", {}, d1i_credit_case(0, 1)),
        Case("h3", {}, d1i_credit_case(0x00010000, 0)),
        Case("h4", {}, d1i_credit_case(0x00000300, 0x80)),
    ], eax_mask=0xFFFFFFFF, mutants=("@fpw", "@crb", "@and", "@val")),
    # 0x2CA48 config_credit_take: free play, the zero-credit guard (its `je` lands on 0x2CA78),
    # the suppressed debit. `mov eax,1` / `xor eax,eax`.
    Spec("config_credit_take", 0x2CA48, [
        Case("t0", {}, d1i_credit_case(0, 1)),
        Case("t1", {}, d1i_credit_case(0, 0)),
        Case("t2", {}, d1i_credit_case(3, 0, 0)),
        Case("t3", {}, d1i_credit_case(3, 0, 1)),
        Case("t4", {}, d1i_credit_case(0x00000100, 0, 0)),
        Case("t5", {}, d1i_credit_case(0x80000000, 0, 0)),
        Case("t6", {}, d1i_credit_case(0x00010000, 0x80, 0)),
    ], eax_mask=0xFFFFFFFF, mutants=("@fpw", "@crb", "@nodebit", "@decb", "@order", "@ndw")),
    # 0x2CA78 config_credit_zero and 0x2D2F0 input_joystick_device: `xor eax,eax; ret`.
    Spec("config_credit_zero", 0x2CA78, [
        Case("z0", {"eax": 0x12345678}),
        Case("z1", {"eax": 0x000000FF}),
    ], eax_mask=0xFFFFFFFF, mutants=("@keep", "@al")),
    Spec("input_joystick_device", 0x2D2F0, [
        Case("j0", {"eax": 0xFFFFFF00}),
        Case("j1", {"eax": 0x00000001}),
    ], eax_mask=0xFFFFFFFF, mutants=("@keep", "@al")),
    # 0x2CCD0 config_menu_default_bits: EAX = the table. b6 has a negative count (the raw's
    # `jge` at 0x2CD11 is signed and its index is `movsx ebx,dx`, 0x2CD0B).
    Spec("config_menu_default_bits", 0x2CCD0, [
        Case("b0", {"eax": D1I_MENU}, d1i_menu_case([])),
        Case("b1", {"eax": D1I_MENU}, d1i_menu_case([(1, 4, 3, {2})])),
        Case("b2", {"eax": D1I_MENU}, d1i_menu_case([(1, 0, 2, {1}), (1, 8, 4, {3})])),
        Case("b3", {"eax": D1I_MENU}, d1i_menu_case([(1, 2, 2, set()), (1, 0, 3, {1, 2}), (1, 12, 2, {0})])),
        Case("b4", {"eax": D1I_MENU}, d1i_menu_case([(0x01000000, 0x24, 3, {1}), (1, 0x1F, 2, {1})])),
        Case("b5", {"eax": D1I_MENU}, d1i_menu_case([(1, 3, 0, {0}), (1, 5, 1, {0}), (1, 6, 3, {1})])),
        Case("b6", {"eax": D1I_MENU}, d1i_menu_case([(1, 4, 0xFFFFFFFF, {1}), (1, 1, 2, {1})])),
        Case("b7", {"eax": D1I_MENU}, d1i_menu_case([(1, 4, 0x80000000, {1}), (1, 9, 3, {2})])),
    ], eax_mask=0xFFFFFFFF,
       mutants=("@presb", "@shift", "@unsigned", "@all", "@star", "@idx1", "@estride", "@stride")),
    # 0x2DB58 hiscore_locate: EAX = rec, EDX = table, EBX = left_out, ECX = size_out.
    Spec("hiscore_locate", 0x2DB58, [
        Case("l0", {"eax": 0, "edx": 0, "ebx": D1I_HS_OUT, "ecx": D1I_HS_OUT + 4}, d1i_hs_case()),
        Case("l1", {"eax": 9, "edx": 0, "ebx": D1I_HS_OUT, "ecx": D1I_HS_OUT + 4}, d1i_hs_case()),
        Case("l2", {"eax": 10, "edx": 0, "ebx": D1I_HS_OUT, "ecx": D1I_HS_OUT + 4}, d1i_hs_case()),
        Case("l3", {"eax": 0, "edx": 1, "ebx": D1I_HS_OUT, "ecx": 0}, d1i_hs_case()),
        Case("l4", {"eax": 0, "edx": 2, "ebx": 0, "ecx": D1I_HS_OUT + 4}, d1i_hs_case()),
        Case("l5", {"eax": 0, "edx": 3, "ebx": D1I_HS_OUT, "ecx": D1I_HS_OUT + 4}, d1i_hs_case()),
        Case("l6", {"eax": 0, "edx": 0xFFFFFFFF, "ebx": D1I_HS_OUT, "ecx": D1I_HS_OUT + 4}, d1i_hs_case()),
        Case("l7", {"eax": 0xFFFFFFFF, "edx": 1, "ebx": D1I_HS_OUT, "ecx": D1I_HS_OUT + 4}, d1i_hs_case()),
        Case("l8", {"eax": 5, "edx": 1, "ebx": 0, "ecx": 0},
             d1i_hs_case(descs=((3, 1, 2), (7, 5, 0x21), (2, 9, 4)),
                         bases=(0x0010A100, 0x0010A200, 0x0010A300))),
        Case("l9", {"eax": 6, "edx": 1, "ebx": D1I_HS_OUT + 4, "ecx": D1I_HS_OUT},
             d1i_hs_case(descs=((3, 1, 2), (7, 5, 0x21), (2, 9, 4)),
                         bases=(0x0010A100, 0x0010A200, 0x0010A300))),
        Case("l10", {"eax": 1, "edx": 2, "ebx": D1I_HS_OUT, "ecx": D1I_HS_OUT + 4},
             d1i_hs_case(descs=((3, 1, 2), (7, 5, 0x21), (2, 0xFFFF, 0xFFFF)),
                         bases=(0x0010A100, 0x0010A200, 0x0010A300))),
    ], eax_mask=0xFFFFFFFF,
       mutants=("@bound", "@tsigned", "@rsigned", "@size", "@left", "@base", "@swap", "@countw")),
    # 0x2F464 text_number_cont: EAX = value, EDX = width, EBX = pad, ECX = mode. 0x2EFD4 and
    # 0x2F198 are stubs, as in text_number_draw's row: the 0x14-byte stack buffer goes by value
    # (zero in both: the emulator's fresh stack, the port's zeroed array), so the calls and their
    # arguments are compared but not that the two calls share one buffer (record §4). The stub
    # EAX of 0x2EFD4 varies; 0x2F464 does not read it (0x2F47E `xor eax,eax`).
    Spec("text_number_cont", 0x2F464, [
        Case("c0", {"eax": 12345, "edx": 5, "ebx": 0, "ecx": 0xF000}, {0x00105F34: le32(0x11223344)},
             {0x2EFD4: 5}),
        Case("c1", {"eax": 0xFFFFFFD6, "edx": 3, "ebx": 4, "ecx": 0x1000}, {0x00105F34: le32(0xAABBCCDD)},
             {0x2EFD4: 0x100}),
        Case("c2", {"eax": 7, "edx": 2, "ebx": 1, "ecx": 2}, {0x00105F34: le32(0x01020304)},
             {0x2EFD4: 0}),
    ], calls=(E.Call(0x2EFD4, ("eax", "[edx]", "[edx+4]", "[edx+8]", "[edx+12]", "ebx", "ecx"),
                     mode="stub", clobbers=("ebx", "ecx", "edx")),
              E.Call(0x2F198, ("eax", "edx", "[ebx]", "[ebx+4]", "[ebx+8]", "[ebx+12]", "ecx"),
                     mode="stub", clobbers=("ebx", "ecx", "edx"),
                     writes=((None, 0x00105F34, b"\x5A\x5A\x5A\x5A"),))),
       eax_mask=0, mutants=("@val", "@swap", "@fmt", "@col", "@row", "@mode")),
    # 0x2FE40 menu_entry_find: EAX = base, EDX = stride, EBX = idx. 0x1C500 and its tree run real
    # (the string's first byte decides); ids: 1 = "?hid", 2 = "shown", 3 = ">x".
    Spec("menu_entry_find", 0x2FE40, [
        Case("e0", {"eax": D1I_ENT, "edx": 0x10, "ebx": 0xFFFFFFFF}, d1i_find_case([2], 0x10)),
        Case("e1", {"eax": D1I_ENT, "edx": 0x10, "ebx": 0}, d1i_find_case([0], 0x10)),
        Case("e2", {"eax": D1I_ENT, "edx": 0x10, "ebx": 0}, d1i_find_case([2], 0x10)),
        Case("e3", {"eax": D1I_ENT, "edx": 0x10, "ebx": 0}, d1i_find_case([1], 0x10)),
        Case("e4", {"eax": D1I_ENT, "edx": 0x10, "ebx": 2}, d1i_find_case([2, 0x100, 2], 0x10)),
        Case("e5", {"eax": D1I_ENT, "edx": 0x10, "ebx": 2}, d1i_find_case([2, 0, 2], 0x10)),
        Case("e6", {"eax": D1I_ENT, "edx": 0x10, "ebx": 2}, d1i_find_case([2, 2, 0], 0x10)),
        Case("e7", {"eax": D1I_ENT, "edx": 0x24, "ebx": 1}, d1i_find_case([2, 1], 0x24)),
        Case("e8", {"eax": D1I_ENT, "edx": 0x10, "ebx": 0x80000000}, d1i_find_case([2], 0x10)),
        Case("e9", {"eax": D1I_ENT, "edx": 8, "ebx": 3}, d1i_find_case([2, 2, 2, 3, 2], 8)),
        Case("e10", {"eax": D1I_ENT, "edx": 0x10, "ebx": 1}, d1i_find_case([0, 2], 0x10)),
    ], allow_calls=(0x1C500, 0x474E4, 0x1E75C, 0x1E808, 0x500BB), eax_mask=0xFFFFFFFF,
       mutants=("@neg", "@hole", "@holeb", "@stride", "@q", "@qchar", "@empty", "@off")),
    # 0x4F228 render_projection_reset: AL = the byte for 0x107A55 (AH is zeroed, 0x4F22A).
    Spec("render_projection_reset", 0x4F228, [
        Case("v0", {"eax": 0x12345678}, d1i_proj_case()),
        Case("v1", {"eax": 0x0000FF80}, d1i_proj_case()),
    ], eax_mask=0, mutants=("@ah", "@w55", "@al", "@a3a", "@a38")),
    # 0x38990 render_scroll_track: the masked low word and the truncating /64 of the dword.
    Spec("render_scroll_track", 0x38990, [
        Case("k0", {}, d1i_track_case(0x00001234, 0x0010)),
        Case("k1", {}, d1i_track_case(0xFFFFFFC1, 0x0000)),
        Case("k2", {}, d1i_track_case(0xFFFFFFC0, 0xFFFF)),
        Case("k3", {}, d1i_track_case(0xFFFFFF81, 0x8000)),
        Case("k4", {}, d1i_track_case(0x7FFFFFFF, 0x0001)),
        Case("k5", {}, d1i_track_case(0x80000000, 0x0001)),
        Case("k6", {}, d1i_track_case(0x0001003F, 0x0100)),
        Case("k7", {}, d1i_track_case(0x0000FFC0, 0x0000)),
    ], eax_mask=0, mutants=("@mask", "@nomask", "@lo", "@shift", "@add", "@wide")),
    # 0x336C0 palette_list_init: 0x61A70 (the runtime memset, with 0x65490/0x654C7) and 0x33734
    # (palette_record) run real on both sides. EAX = the stored head (diff_runner.c).
    Spec("palette_list_init", 0x336C0, [
        Case("p0", {}, d1i_pal_case(0x11)),
        Case("p1", {}, d1i_pal_case(0x52)),
    ], allow_calls=(0x61A70, 0x65490, 0x654C7, 0x33734), eax_mask=0xFFFFFFFF,
       mutants=("@fill", "@len", "@head", "@ff", "@count", "@bd", "@enq", "@args")),
    # 0x500C4 input_pump: the one-frame debounce and the repeat counter.
    Spec("input_pump", 0x500C4, [
        Case("u0", {}, d1i_pump_case(0x01, 0x02, 0x01000200, 0x01000200, 0xFFFFFFFF, 0x01000000, 5)),
        Case("u1", {}, d1i_pump_case(0x01, 0x02, 0x01000200, 0x01000200, 0xFFFFFFFF, 0x01000000, 1)),
        Case("u2", {}, d1i_pump_case(0x01, 0x02, 0x00000000, 0x01000000, 0xFFFFFFFF, 0x01000000, 5)),
        Case("u3", {}, d1i_pump_case(0x01, 0x02, 0x01000200, 0x01000200, 0xFFFFFFFF, 0x80000000, 5)),
        Case("u4", {}, d1i_pump_case(0x02, 0x03, 0x01000000, 0x01000000, 0xFFFFFFFF, 0x00000000, 5)),
        Case("u5", {}, d1i_pump_case(0x01, 0x02, 0x01000200, 0x01000200, 0x12345678, 0x00000200, 0)),
        Case("u6", {}, d1i_pump_case(0x01, 0x02, 0x01000200, 0x01000200, 0x12345678, 0x00000200, 0x0100)),
        Case("u7", {}, d1i_pump_case(0xFF, 0x80, 0xFF008000, 0xFF008000, 0x0F0F8F0F, 0xFF000000, 1)),
        Case("u8", {}, d1i_pump_case(0x01, 0x02, 0x01000200, 0x01000200, 0x12345678, 0x00000200, 0x0201)),
    ], eax_mask=0xFFFFFFFF,
       mutants=("@k", "@prev", "@debounce", "@latch", "@rpt1", "@rpt2", "@decb", "@next", "@clr", "@ret")),
    # 0x50146 input_repeat_set: EAX = mask, DX = first, BX = next.
    Spec("input_repeat_set", 0x50146, [
        Case("w0", {"eax": 0x11223344, "edx": 0xAAAA5678, "ebx": 0xBBBB9ABC},
             {0x000E1C3C: le32(0x5A5A5A5A) + le16(0x1111) + le16(0x2222) + le16(0x3333) + le16(0x4444)}),
        Case("w1", {"eax": 0, "edx": 0x00000001, "ebx": 0xFFFF0000},
             {0x000E1C3C: le32(0x5A5A5A5A) + le16(0x1111) + le16(0x2222) + le16(0x3333) + le16(0x4444)}),
    ], eax_mask=0, mutants=("@mask", "@first", "@count", "@next", "@wide")),
    # 0x50161 input_select_bits: EAX = mask.
    Spec("input_select_bits", 0x50161, [
        Case("x0", {"eax": 0}, {0x000E1C34: le32(0x12345678) + le32(0x0F0F0F0F)}),
        Case("x1", {"eax": 0xFF00FF00}, {0x000E1C34: le32(0x12345678) + le32(0x0F0F0F0F)}),
        Case("x2", {"eax": 0x00FF00FF}, {0x000E1C34: le32(0x12345678) + le32(0x0F0F0F0F)}),
        Case("x3", {"eax": 0xFFFFFFFF}, {0x000E1C34: le32(0xA5A5A5A5) + le32(0xA5A5A5A5)}),
        Case("x4", {"eax": 0x80000001}, {0x000E1C34: le32(0xFFFFFFFF) + le32(0x00000000)}),
    ], eax_mask=0xFFFFFFFF, mutants=("@xor", "@latch", "@keep", "@inv")),
    # 0x51F72 gfx_fill_screen: EAX = addr, EDX = value; 0xFA00 bytes inside the image's data.
    Spec("gfx_fill_screen", 0x51F72, [
        Case("g0", {"eax": 0x000F1000, "edx": 0xA1B2C3D4, "ecx": 0x11223344}),
        Case("g1", {"eax": 0x000F0003, "edx": 0x00000011, "ecx": 0xFFFFFF00}),
    ], eax_mask=0xFFFFFFFF, mutants=("@count", "@row", "@step", "@byte")),
    # 0x32B00 config_play_time_snap / 0x32B4C config_play_time_snap_b: EAX = idx, EDX = arm. Only
    # the arm != 0 store is exercised; the arm == 0 block posts through 0x2E934.
    Spec("config_play_time_snap", 0x32B00, [
        Case("a0", {"eax": 0, "edx": 1}, d1i_pt_case()),
        Case("a1", {"eax": 1, "edx": 0x100}, d1i_pt_case()),
        Case("a2", {"eax": 2, "edx": 0x80000000}, d1i_pt_case()),
        Case("a3", {"eax": 5, "edx": 1}, d1i_pt_case()),
    ], unhit_named={0x32B17: "the arm == 0 block " + D1I_PT_AUDIT},
       eax_mask=0xFFFFFFFF, mutants=("@idx", "@dst", "@wide", "@arm")),
    Spec("config_play_time_snap_b", 0x32B4C, [
        Case("b0", {"eax": 0, "edx": 1}, d1i_pt_case()),
        Case("b1", {"eax": 1, "edx": 0x100}, d1i_pt_case()),
        Case("b2", {"eax": 2, "edx": 0x80000000}, d1i_pt_case()),
        Case("b3", {"eax": 3, "edx": 1}, d1i_pt_case()),
    ], unhit_named={0x32B63: "the arm == 0 block " + D1I_PT_AUDIT},
       eax_mask=0xFFFFFFFF, mutants=("@idx", "@dst", "@wide", "@arm")),
    # 0x32A3C config_play_time_close: EAX = mode, EDX = flag. The run clock 0x32970 (host-owned;
    # the port omits it) runs real in the original from the state where it changes no byte
    # (d1i_pt_case); only mode & 3 == 0 is exercised, every other block adds through 0x2DAE4.
    Spec("config_play_time_close", 0x32A3C, [
        Case("c0", {"eax": 0, "edx": 0}, d1i_pt_case()),
        Case("c1", {"eax": 4, "edx": 1}, d1i_pt_case()),
        Case("c2", {"eax": 0xFFFFFFFC, "edx": 0}, d1i_pt_case(acc=(0x01020304, 5, 6, 7), clock=0)),
    ], allow_calls=(0x32970,),
       unhit_named={a: "mode & 3 != 0: " + D1I_PT_AUDIT
                    for a in (0x32A65, 0x32A6A, 0x32A7B, 0x32A91, 0x32AB4, 0x32AC4, 0x32AC8, 0x32AD4,
                              0x32ADE, 0x32AF5)},
       eax_mask=0xFFFFFFFF, mutants=("@idx", "@drop", "@wide")),
]
D1_SPECS += D1I_SPECS


# ---- track D1 batch d1c (record 2026-10-11-reverse-d1c): game/flow.c's first half ----------------
D1C_FX_RECS = (0x000F0B00, 0x000F1314, 0x000F1B28)   # the effect pool's first three records (stride 0x814)
D1C_FX_FREE = 0x000F233C                              # the fourth: the one record on the free list
D1C_FX_SRC = 0x0010A100                               # the source records, 0x20 apart (zero BSS)
D1C_FX_PAL = 0x00107498                               # the palette dirty list's base (0x33734's head)


def d1c_dw(*vals):
    return b"".join(le32(v) for v in vals)


def d1c_fx_rec(t, state, n=0, reload=7, body=b"", tgt=b"", first=0x20, count=2):
    """One active effect record: +0xC type, +0xD reload, +0xE state, +0xF n, `body` from +0x10,
    `tgt` from +0x410, and its source record's +8 (palette first) and +0xC (count)."""
    return (t, state, n, reload, bytes(body), bytes(tgt), first, count)


def d1c_fx_case(recs, lock=0, counter=0x40):
    """0x134C0: the active list 0xFCCE0 -> recs -> 0xFCCE0, one record on the free list 0xFCCE8, the
    lock 0x9AF3C, the active counter 0x9AF3D and the dirty-list head on a 0xA5-seeded list. The
    bytes of a record past `body`/`tgt` are 0x5A/0x6B, so a loop one entry too long changes them."""
    p = {0x0009AF3C: bytes([lock]), 0x0009AF3D: bytes([counter]),
         0x00107798: le32(D1C_FX_PAL), D1C_FX_PAL: b"\xa5" * 0x40,
         0x000FCCE8: le32(D1C_FX_FREE) * 2, D1C_FX_FREE: le32(0x000FCCE8) * 2}
    chain = [0x000FCCE0] + list(D1C_FX_RECS[:len(recs)]) + [0x000FCCE0]
    p[0x000FCCE0] = le32(chain[1]) + le32(chain[-2])
    for i, (t, state, n, reload, body, tgt, first, count) in enumerate(recs):
        src = D1C_FX_SRC + i * 0x20
        head = le32(chain[i + 2]) + le32(chain[i]) + le32(src) + bytes([t, reload, state, n])
        p[chain[i + 1]] = (head + body).ljust(0x40, b"\x5a")
        p[chain[i + 1] + 0x410] = tgt.ljust(0x40, b"\x6b")
        p[src] = le32(0x00800040 + i) + le32(0x5A5A5A5A) + le32(first) + le32(count)
    return p


# 0x1C6D4: the nine dwords the tree accepts, and values around and between them (one per
# rejecting block: below 0x80995C, between the constants, above 0x8099CC, a low-16 alias).
D1C_KNOWN = (0x0080995C, 0x0080997C, 0x00809984, 0x0080998C, 0x00809994, 0x0080999C, 0x008099A4,
             0x008099AC, 0x008099CC)
D1C_UNKNOWN = (0x0080995B, 0x00809960, 0x00809980, 0x00809988, 0x00809990, 0x00809998, 0x008099A8,
               0x008099B0, 0x008099D0, 0x0090995C, 0, 0xFFFFFFFF)


def d1c_pause_case(d8=0x11, d9=0x22, da=0x33, db=0x44, seq=0x0010B000, song=0x00801234):
    """0x1D1B0/0x1D220: the four pause bytes 0x1028D8..DB (prompt, song byte, music, sample), the
    sequence handle 0x1028C0, the current song 0x1028D4 and the pending song 0x1028CC."""
    return {0x001028D8: bytes([d8, d9, da, db]), 0x001028DC: b"\x55",
            0x001028C0: le32(seq), 0x001028D4: le32(song), 0x001028CC: le32(0xBBBBBBBB)}


def d1c_audit_case(f27, f26):
    """0x1E988: fields 0x27 and 0x26 as full 32-bit values: descriptor 0x1C000 is 8 nibbles ending
    at byte 4 of 0x105DE1, 0x1C200 the next 8 (0x2D974's walk, most significant byte last)."""
    return {0x0002D300 + 0x27 * 4: le32(0x0001C000), 0x0002D300 + 0x26 * 4: le32(0x0001C200),
            0x00105DE1: le32(f27) + le32(f26)}


def d1c_chal_case(f0, f1):
    """0x29CBC: the two side bytes 0x105B34/35 and the two adjacent 7-dword tables, every entry
    distinct."""
    return {0x00105B34: bytes([f0, f1]),
            0x000A8AC0: d1c_dw(*[0x00C00000 + k for k in range(7)]),
            0x000A8ADC: d1c_dw(*[0x00D00000 + k for k in range(7)])}


def d1c_marks_case(af2, af3, a=(0, 0, 0, 0), b=(0, 0, 0, 0)):
    """0x256F4: the two bound bytes 0x104AF2/F3, the two 4-dword handle arrays 0x104A88/0x104A98
    and the dword after them (0x104AA8, non-zero so a walk past the second array stops spawning)."""
    return {0x00104A88: d1c_dw(*a), 0x00104A98: d1c_dw(*b), 0x00104AA8: le32(0x5A5A5A5A),
            0x00104AF2: bytes([af2, af3])}


def d1c_marks_wide():
    """0x256F4 with both bounds 0x80: [0x104A88, 0x104C98) filled with 0x80 (the bound bytes
    included), then one zero dword per array: 0x104A88[100] and 0x104A98[120]."""
    p = {0x00104A88 + k: b"\x80" * min(0x40, 0x210 - k) for k in range(0, 0x210, 0x40)}
    p[0x00104A88 + 100 * 4] = le32(0)
    p[0x00104A98 + 120 * 4] = le32(0)
    return p


def d1c_res_case(adc, b1e, af2, af3, w0=0x0010, w1=0x0020):
    """0x27BA4: the dword 0x104ADC, the byte 0x104B1E, the two round-win bytes, the two words
    0x108860 with their neighbours and a sentinel in the result dword."""
    return {0x00104ADC: le32(adc), 0x00104B1E: bytes([b1e]), 0x00104AF2: bytes([af2, af3]),
            0x0010885E: le16(0x0055) + le16(w0) + le16(w1) + le16(0x0030),
            0x00104AD4: le32(0x5A5A5A5A)}


D1C_EDX = {"edx": 0xABCD1234}


def d1c_stage_case(mode, b1f=0, cb=b"\x7f\x7f\x7f", tbl=None, lst=b"\x00" * 7, fr0=3, fr1=2,
                   ad4=0, afc=0x5A5A):
    """0x25848: the mode byte 0x104B17, 0x104B1F and the three signed bytes 0x108169.., the
    0xA87C4 entries in `tbl` (index -> byte), the seven list bytes 0x108106 (an eighth seeded),
    the two frame sources 0x10782A/0x1078BE, 0x104AD4 and the stage word with its neighbour."""
    p = {0x00104B17: bytes([mode]), 0x00104B1F: bytes([b1f]), 0x00108169: cb,
         0x00108106: lst + b"\x99", 0x0010782A: bytes([fr0]), 0x001078BE: bytes([fr1]),
         0x00104AD4: le32(ad4), 0x00104AFC: le16(afc) + le16(0x7777)}
    for k, v in (tbl or {}).items():
        p[0x000A87C4 + k] = bytes([v])
    return p


def d1c_bonus_case(b529, fa, fb):
    """0x25FDC/0x2604C: the option byte 0x104529, the two handle dwords 0x104AB0/B4, the bytes
    0x104B0C..0x104B10 (the two timers and their neighbours) and 0x104AE8..0x104AEB (the two arm
    bytes and their neighbours)."""
    return {0x00104529: bytes([b529]), 0x00104AB0: le32(0x11111111) + le32(0x22222222),
            0x00104B0C: b"\x61\x62\x63\x64\x65", 0x00104AE8: bytes([0x10, fa, fb, 0x20])}


def d1c_pose_case(ad8, aa8, a, b):
    """0x28C38: the frame count 0x104AD8, the divisor 0x104AA8 and the two +0x5A bytes."""
    return {0x00104AD8: le32(ad8), 0x00104AA8: le32(aa8), 0x0010780A: bytes([a]),
            0x0010789E: bytes([b])}


def d1c_vol_case(sfx=0x21, mus=0x22, seq=0x0010B000):
    """0x1CAB8/0x1CED4: the SFX and music volumes 0xA2CB4/0xA2CB8 with the bytes around them and
    the sequence handle 0x1028C0."""
    return {0x000A2CB0: le32(0x5A5A5A5A) + le32(sfx) + le32(mus) + le32(0x6B6B6B6B),
            0x001028C0: le32(seq)}


# 0x474E4 as the two prompt rows stub it: the stub writes the string into its out buffer (EDX)
# and 0x1C500, real on both sides, returns that buffer. 0x2F280's arguments are its seam's.
D1C_STR = E.Call(0x474E4, ("eax", "edx", "ebx"), mode="stub", eax=8, clobbers=("ebx", "edx"),
                 writes=((1, 0, b"INSERT \x00"),))
D1C_RELEASE = E.Call(0x2F280, ("eax", "edx", "ebx", "ecx"), clobbers=("ebx", "ecx", "edx"))


def d1c_prompt_case(b529, s0=0, s1=0, c06=0x12, c07=0x0C):
    """0x2C088: the option byte, the two prompt sprites 0x105BF0/F4 (and the length dword after
    them), the last drawn col/row bytes 0x105C06/07 with their neighbours and the string buffer."""
    return {0x00104529: bytes([b529]), 0x00105BF0: le32(s0) + le32(s1) + le32(0x5A5A5A5A),
            0x00105C04: bytes([0x61, 0x62, c06, c07, 0x63]), 0x00102760: b"\x5a" * 0x10}


def d1c_coin_case(phase, mid=b"1\x00", bf8=0x5A5A5A5A):
    """0x2C1D4: the phase word 0xEF6DC, the middle string 0x809C4, the length dword 0x105BF8,
    the col/row bytes 0x105C06/07 with their neighbours and the string buffer."""
    return {0x000EF6DC: le16(phase), 0x000809C4: mid, 0x00105BF8: le32(bf8),
            0x00105C04: b"\x61\x62\x63\x64\x65", 0x00102760: b"\x5a" * 0x10}


def d1c_ovl_case(attract=0, free=0, credits=0, phase=0, c04=0x61, row=0x21):
    """0x2BF08: the attract byte 0x9AD58, the free-play byte 0x105D60, the credits dword, the
    phase word, the blink byte 0x105C04 and the row byte 0x105C05 with a neighbour, and the
    string buffer."""
    return {0x0009AD58: bytes([attract]), 0x00105D60: bytes([free]), 0x00105C00: le32(credits),
            0x000EF6DC: le16(phase), 0x00105C04: bytes([c04, row, 0x63]),
            0x00102760: b"\x5a" * 0x10}


def d1c_buf_case(once=0, c0=0x0010B000, c4=0x0010B010, c8=1, d0=0, slots=(0, 0, 0, 0)):
    """0x1D0BC: the once byte 0xA2CB0 (and the enable byte after it), the handles 0x1028C0 (sequence),
    C4 (MDI), C8 (DIG), CC (pending song) and the MIDI buffer D0, and each slot's +0x10 buffer
    dword between two sentinels."""
    p = {0x000A2CB0: bytes([once, 0x6C]),
         0x001028C0: le32(c0) + le32(c4) + le32(c8) + le32(0xBBBBBBBB) + le32(d0)}
    for i, v in enumerate(slots):
        p[0x0010286C + i * 0x18] = le32(0x5A5A5A5A) + le32(v) + le32(0x6B6B6B6B)
    return p


D1C_SPECS = [
    # 0x134C0 effects_step: no register argument. The walk of the active list 0xFCCE0 under the
    # lock 0x9AF3C; per record the +0xE countdown, the type dispatch (jump table 0x134A8, six
    # entries, resolved by hand below) and the seven bodies. 0x33734 (the dirty-list append) and
    # the two list primitives 0x249D0/0x249B0 run real on both sides (leaves; effects.c keeps its
    # own unseamed copies of the primitives, as the 0x13C70 row states). Mask 0: the one caller
    # (0x25639) reloads EAX at 0x2563E (`mov eax,[0x10150c]`).
    Spec("effects_step", 0x134C0, [
        Case("k0", {}, d1c_fx_case([d1c_fx_rec(4, 1, body=d1c_dw(0x00101010, 0))], lock=1)),
        Case("k1", {}, d1c_fx_case([])),
        Case("z0", {}, d1c_fx_case([d1c_fx_rec(0, 0, n=2, body=d1c_dw(3, 0x11, 0x22, 0x33))])),
        Case("z1", {}, d1c_fx_case([d1c_fx_rec(0, 5, n=2, body=d1c_dw(3, 0x11111111, 0x22222222,
                                                                      0x33333333))])),
        Case("a0", {}, d1c_fx_case([d1c_fx_rec(1, 2, n=0x90, body=d1c_dw(0x5A5A5A5A, 0x00102030))])),
        # a1: b 0xFE+5 wraps to 0x03; g 0xF8+0x110 = 0x208 spills 0x02 into r (the 0xffff mask at
        # 0x13573); r 0xF0-0x10 = 0xE0; the deltas are the words at +0x1A/+0x1C/+0x1E (the dwords
        # +0x18/+0x1A/+0x1C `sar 16`); +0xF = 0x90 is added zero-extended.
        Case("a1", {}, d1c_fx_case([d1c_fx_rec(1, 1, n=0x90, body=d1c_dw(0x5A5A5A5A, 0x7FF0F8FE)
                                               + le16(0x7777) + le16(0x0005) + le16(0x0110)
                                               + le16(0xFFF0))])),
        Case("a2", {}, d1c_fx_case([d1c_fx_rec(1, 1, n=1, body=d1c_dw(0x5A5A5A5A, 0x00100020)
                                               + le16(0x0101) + le16(0xFFD0) + le16(0xFFFF)
                                               + le16(0x0000))])),
        Case("s0", {}, d1c_fx_case([d1c_fx_rec(2, 1, n=1, body=d1c_dw(0x111111FE, 0x22222222,
                                                                      0x33333333))])),
        # s1: the descending rotate overwrites the offset byte (+0x10) with the old +0x14's low
        # byte 5 before 0x135D6 reads it: the palette first is src+8 - 5, not + 0x10.
        Case("s1", {}, d1c_fx_case([d1c_fx_rec(2, 1, n=2, body=d1c_dw(0x111111F0, 0x22222205,
                                                                      0x33333333))])),
        Case("s2", {}, d1c_fx_case([d1c_fx_rec(2, 1, n=4, body=d1c_dw(3, 0xA1, 0xA2, 0xA3, 0xA4,
                                                                      0xA5))])),
        Case("s3", {}, d1c_fx_case([d1c_fx_rec(2, 1, n=0, body=d1c_dw(0x1111117F, 0x22222222))])),
        # s4: n = 3 descending: the third store lands on the record's own +0xC dword (type,
        # reload, state, n), and 0x135E7 reads the new n = 2 for the palette count.
        Case("s4", {}, d1c_fx_case([d1c_fx_rec(2, 1, n=3, body=d1c_dw(0x444444FF, 0x02010905,
                                                                      0x66666666))])),
        Case("l0", {}, d1c_fx_case([d1c_fx_rec(3, 1, n=9, count=3, body=d1c_dw(1, 2, 3, 4))])),
        # l1: entry 0 equal (skipped); 1 clamps b/g/r at 0xF9/0xFA/0xFF; 2 differs in the top
        # byte only (rewritten, top byte cleared); 3 has g exactly at the target (0x40+8 = 0x48).
        Case("l1", {}, d1c_fx_case([d1c_fx_rec(3, 1, count=4,
                                               body=d1c_dw(0x00101010, 0x00F8F8F8, 0xAA303030,
                                                           0x00404040),
                                               tgt=d1c_dw(0x00101010, 0x00FFFAF9, 0xBB303030,
                                                          0x00804844))])),
        Case("l2", {}, d1c_fx_case([d1c_fx_rec(3, 1, count=2, body=d1c_dw(0x00111111, 0x00222222),
                                               tgt=d1c_dw(0x00111111, 0x00222222)),
                                    d1c_fx_rec(4, 1, count=2, body=d1c_dw(0x00101010, 0))])),
        Case("l3", {}, d1c_fx_case([d1c_fx_rec(3, 1, count=0, body=d1c_dw(1), tgt=d1c_dw(2))])),
        Case("l4", {}, d1c_fx_case([d1c_fx_rec(3, 1, count=0xFFFFFFFF, body=d1c_dw(1),
                                               tgt=d1c_dw(2))])),
        Case("d0", {}, d1c_fx_case([d1c_fx_rec(4, 1, count=3, body=d1c_dw(0x7F050310, 0,
                                                                          0x00FFFF07))])),
        Case("d1", {}, d1c_fx_case([d1c_fx_rec(4, 1, count=2, body=d1c_dw(0, 0))])),
        Case("p0", {}, d1c_fx_case([d1c_fx_rec(5, 1, n=3, body=b"\xf0\x01\x5a\x5a"
                                               + d1c_dw(0x00101010, 0, 0x00050607))])),
        Case("p1", {}, d1c_fx_case([d1c_fx_rec(5, 1, n=2, body=b"\x04\x03\x5a\x5a" + d1c_dw(0, 0))])),
        Case("p2", {}, d1c_fx_case([d1c_fx_rec(5, 1, n=3, body=b"\x04\x00\x5a\x5a"
                                               + d1c_dw(0x00101010, 0x00202020, 0x00303030),
                                               tgt=d1c_dw(0x6B6B6B6B, 0x00181410, 0x00202020,
                                                          0x00323C40))])),
        Case("p3", {}, d1c_fx_case([d1c_fx_rec(5, 1, n=2, body=b"\x04\x00\x5a\x5a"
                                               + d1c_dw(0x00101010, 0x00202020),
                                               tgt=d1c_dw(0x6B6B6B6B, 0x00101010, 0x00202020))])),
        Case("p4", {}, d1c_fx_case([d1c_fx_rec(5, 1, n=0, body=b"\x04\x00\x5a\x5a")])),
        Case("q0", {}, d1c_fx_case([d1c_fx_rec(6, 1, n=1, count=2, body=d1c_dw(1, 2, 3))])),
        # q1: entry 0 floors b exactly at the target (0x40-8 = 0x38), g at 0x3C, r stays 0x38;
        # 1 equal; 2 differs in the top byte only.
        Case("q1", {}, d1c_fx_case([d1c_fx_rec(6, 1, count=3,
                                               body=d1c_dw(0x00404040, 0x00101010, 0xCC202020),
                                               tgt=d1c_dw(0x00303C38, 0x00101010, 0xDD202020))])),
        Case("q2", {}, d1c_fx_case([d1c_fx_rec(6, 1, count=1, body=d1c_dw(0x00123456),
                                               tgt=d1c_dw(0x00123456))])),
        Case("t0", {}, d1c_fx_case([d1c_fx_rec(0x87, 1, count=1, body=d1c_dw(0x00404040),
                                               tgt=d1c_dw(0))])),
        Case("m0", {}, d1c_fx_case([d1c_fx_rec(1, 1, n=2, body=d1c_dw(0x5A5A5A5A, 0x00010203)
                                               + le16(0) + le16(1) + le16(2) + le16(3)),
                                    d1c_fx_rec(4, 1, count=1, body=d1c_dw(0)),
                                    d1c_fx_rec(2, 1, n=2, body=d1c_dw(1, 0xB1, 0xB2, 0xB3))])),
    ], allow_calls=(0x33734, 0x249B0, 0x249D0), eax_mask=0,
       mutants=("@lock", "@type0", "@dec", "@reload", "@default", "@t1mask", "@t1delta",
                "@t1first", "@t1sext", "@rotdir", "@rotsub", "@offcache", "@ncache", "@t3flag",
                "@t6flag", "@ptr", "@palcount", "@nopal", "@eq", "@step", "@clamp", "@top",
                "@floor", "@count", "@t4zero", "@t6to", "@t5flag", "@t5dec", "@t5base", "@t5tgt",
                "@t5zext", "@t5remove", "@remove", "@free", "@stop")),
    # 0x1C6D4 frontend_resource_known: EAX = rec; the dword at rec against the nine constants of
    # the cmp/jb/jbe tree. Mask 0xFF: `mov al,1`/`xor al,al`, and the three callers test AL
    # (0x1203D, 0x126F1, 0x42599 `test al,al`).
    Spec("frontend_resource_known", 0x1C6D4,
         [Case("y%d" % i, {"eax": 0x0010A000}, {0x0010A000: le32(v), 0x0010A004: le32(0x0080995C)})
          for i, v in enumerate(D1C_KNOWN)]
         + [Case("n%d" % i, {"eax": 0x0010A000}, {0x0010A000: le32(v), 0x0010A004: le32(0x0080995C)})
            for i, v in enumerate(D1C_UNKNOWN)],
         eax_mask=0xFF,
         mutants=("@deref", "@low16", "@range", "@next") + tuple("@c%d" % i for i in range(9))),
    # 0x1D220 sound_sample_pause_toggle: no argument. The byte 0x1028DB ^= 1; when it is now 1
    # (zero-extended, `cmp eax,1`) the tail jump 0x1D231 reaches 0x1CD9C (stub). Mask 0: the one
    # caller (0x24DC9) jumps to 0x24D08, which loads AH and calls int 16h (AH = 1 reads no AL).
    Spec("sound_sample_pause_toggle", 0x1D220, [
        Case("t0", {}, d1c_pause_case(db=0)),
        Case("t1", {}, d1c_pause_case(db=1)),
        Case("t2", {}, d1c_pause_case(db=3)),
        Case("t3", {}, d1c_pause_case(db=2)),
    ], calls=(E.Call(0x1CD9C, ()),), eax_mask=0,
       mutants=("@set", "@nz", "@nocall", "@order", "@byte")),
    # 0x1D1B0 sound_music_pause_toggle: no argument. Paused (0x1028DA == 1): the byte clears and,
    # with a song byte, a song and a sequence, the song is pending again (0x1028CC). Otherwise a
    # sequence that plays (0x1CA40, AL tested at 0x1D200) is stopped (0x5DEAF) and the byte is
    # set. Mask 0: record §1 (0x1D261, 0x1D281, 0x24DC4).
    Spec("sound_music_pause_toggle", 0x1D1B0, [
        Case("r0", {}, d1c_pause_case(da=1, d9=0)),
        Case("r1", {}, d1c_pause_case(da=1, d9=1, song=0)),
        Case("r2", {}, d1c_pause_case(da=1, d9=0x80, seq=0)),
        Case("r3", {}, d1c_pause_case(da=1, d9=1)),
        Case("r4", {}, d1c_pause_case(da=0, seq=0)),
        Case("r5", {}, d1c_pause_case(da=0), {0x1CA40: 0}),
        Case("r6", {}, d1c_pause_case(da=0), {0x1CA40: 1}),
        Case("r7", {}, d1c_pause_case(da=2), {0x1CA40: 0x100}),
        Case("r8", {}, d1c_pause_case(da=0x81), {0x1CA40: 0x101}),
    ], calls=(E.Call(0x1CA40, (), eax=0), E.Call(0x5DEAF, (), clobbers=("ebx", "ecx", "edx"))),
       eax_mask=0,
       mutants=("@clear", "@d9", "@song", "@seq", "@cc", "@eq", "@gate", "@al", "@nostop", "@set",
                "@order")),
    # 0x1E988 hiscore_audit_reset_due: no argument. Config field 0x27 >= 2000 and field 0x26 >=
    # 200, both signed (`jl`); 0x2D974 runs real on both sides and is recorded (its own row). The
    # two descriptors are poked to full 32-bit fields so a negative value exists. Mask 0xFF: the
    # one caller masks it (0x1E884 `and eax,0xff`).
    Spec("hiscore_audit_reset_due", 0x1E988, [
        Case("h0", {}, d1c_audit_case(1999, 500)),
        Case("h1", {}, d1c_audit_case(2000, 199)),
        Case("h2", {}, d1c_audit_case(2000, 200)),
        Case("h3", {}, d1c_audit_case(0x80000000, 500)),
        Case("h4", {}, d1c_audit_case(5000, 0xFFFFFFFF)),
        Case("h5", {}, d1c_audit_case(0x7FFFFFFF, 0x7FFFFFFF)),
    ], calls=(E.Call(0x2D974, ("eax",), mode="real"),), eax_mask=0xFF,
       mutants=("@t27", "@t26", "@u27", "@u26", "@swap", "@one")),
    # 0x29B70 game_null_step and 0x32968 game_init_null: a bare `ret` each; the rows assert that
    # no byte changes. Mask 0: neither writes EAX.
    Spec("game_null_step", 0x29B70, [Case("z0", {"eax": 0x1234})], eax_mask=0, mutants=("@mutant",)),
    Spec("game_init_null", 0x32968, [Case("z0", {"eax": 0x1234})], eax_mask=0, mutants=("@mutant",)),
    # 0x29CBC flow_challenge_handle: EAX = side, EDX = the character. The dword 0xA8ADC[ch] when
    # the byte 0x105B34[side] is non-zero, else 0xA8AC0[ch]. Full mask: the four callers store
    # EAX (0x42984, 0x42A1E, 0x42AB8, 0x42AEE `mov [0xc83c4],eax`).
    Spec("flow_challenge_handle", 0x29CBC, [
        Case("c0", {"eax": 0, "edx": 0}, d1c_chal_case(0, 0)),
        Case("c1", {"eax": 0, "edx": 3}, d1c_chal_case(1, 0)),
        Case("c2", {"eax": 1, "edx": 6}, d1c_chal_case(1, 0)),
        Case("c3", {"eax": 1, "edx": 2}, d1c_chal_case(0, 0x80)),
        Case("c4", {"eax": 0, "edx": 5}, d1c_chal_case(2, 0)),
    ], eax_mask=0xFFFFFFFF, mutants=("@side", "@tbl", "@scale", "@char", "@one")),
    # 0x2C06C config_set_credit_row: the byte 0x105C05 = AL. Mask 0: EAX is not written.
    Spec("config_set_credit_row", 0x2C06C, [
        Case("w0", {"eax": 0x1D}, {0x00105C04: b"\x61\x62\x63\x64"}),
        Case("w1", {"eax": 0x12345601}, {0x00105C04: b"\x61\x62\x63\x64"}),
        Case("w2", {"eax": 0xFFFFFF00}, {0x00105C04: b"\x61\x62\x63\x64"}),
    ], eax_mask=0, mutants=("@width", "@off")),
    # 0x2C074 flow_105bf0_clear: the dwords 0x105BF4 and 0x105BF0 = 0. Mask 0: EAX is not written.
    Spec("flow_105bf0_clear", 0x2C074, [
        Case("x0", {"eax": 0x1234, "edx": 0x5678},
             {0x00105BEC: b"\x71\x72\x73\x74" + b"\x81\x82\x83\x84" + b"\x91\x92\x93\x94"
                          + b"\xa1\xa2\xa3\xa4"}),
    ], eax_mask=0, mutants=("@f0", "@f4", "@width")),
    # 0x2C390 flow_list_105c0c_init: the two sentinels 0x105C0C and 0x105C14 self-linked, then the
    # sixteen 0x14-byte nodes 0x105C1C..0x105D48 appended to the 0x105C14 list through 0x249C0
    # (real on both sides: effects.c's unseamed copy). Mask 0: EAX = 0x105C14 at return (0x2C3C2)
    # and both callers call 0x12750 next (0x20E33, 0x20EDA), which loads its registers itself.
    Spec("flow_list_105c0c_init", 0x2C390, [
        Case("i0", {}, {0x00105C0C + k: b"\xa5" * 0x40 for k in range(0, 0x180, 0x40)}),
        Case("i1", {}, {0x00105C0C + k: bytes((0x11 + j + k) & 0xFF for j in range(0x40))
                        for k in range(0, 0x180, 0x40)}),
    ], allow_calls=(0x249C0,), eax_mask=0,
       mutants=("@count", "@head", "@stride", "@sent", "@first")),
    # 0x256F4 flow_win_markers_spawn: no argument. For k below the byte 0x104AF2 (zero-extended,
    # re-read each pass, not capped at the four slots) a zero 0x104A88[k] takes the spawn 0xBB68C at
    # x = 0x1700 + k*0x380; then for k below 0x104AF3 a zero 0x104A98[k] at 0x3940 - k*0x380.
    # 0x2AE14 is a stub whose EAX is the stored handle. Mask 0: record §1.
    Spec("flow_win_markers_spawn", 0x256F4, [
        Case("w0", {}, d1c_marks_case(0, 0), {0x2AE14: 0x0010C100}),
        Case("w1", {}, d1c_marks_case(2, 0, a=(0, 0x0010C0A1, 0, 0)), {0x2AE14: 0x0010C101}),
        Case("w2", {}, d1c_marks_case(3, 2, a=(0x0010C0A0, 0, 0, 0), b=(0, 0x0010C0B1, 0, 0)),
             {0x2AE14: 0x0010C102}),
        Case("w3", {}, d1c_marks_case(0, 3, b=(0x0010C0B0, 0, 0, 0x0010C0B3)), {0x2AE14: 0x0010C103}),
        # w4: bound 5 walks past the four slots: k = 4 is 0x104A98[0], which the second loop then
        # finds non-zero.
        Case("w4", {}, d1c_marks_case(5, 1, b=(0, 0x0010C0B1, 0x0010C0B2, 0x0010C0B3)),
             {0x2AE14: 0x0010C104}),
        # w5: both bounds 0x80 (zero-extended: 128 passes each) over a 0x80-filled range with one
        # zero dword per array (k = 100 and k = 120, whose x is negative).
        Case("w5", {}, d1c_marks_wide(), {0x2AE14: 0x0010C105}),
    ], calls=(SPAWN,), eax_mask=0,
       mutants=("@swap", "@bound", "@sext", "@cap", "@skip", "@step", "@base", "@a3", "@a4", "@a5",
                "@store", "@desc")),
    # 0x27BA4 flow_match_result_set: no argument, no callee. The result -1/0/1/2 into the dword
    # 0x104AD4, and for 0 or 1 the word 0x108860[r] (zero-extended, below 0xC8) gains 4.
    # Mask 0: record §1.
    Spec("flow_match_result_set", 0x27BA4, [
        Case("e0", {}, d1c_res_case(2, 2, 3, 3)),
        Case("e1", {}, d1c_res_case(2, 2, 1, 3)),
        Case("e2", {}, d1c_res_case(2, 2, 3, 1)),
        Case("e3", {}, d1c_res_case(3, 1, 3, 1)),
        Case("e4", {}, d1c_res_case(2, 3, 3, 1)),
        Case("e5", {}, d1c_res_case(5, 2, 5, 1)),
        Case("e6", {}, d1c_res_case(5, 2, 4, 1)),
        Case("e7", {}, d1c_res_case(5, 2, 1, 5)),
        Case("e8", {}, d1c_res_case(5, 2, 1, 4)),
        Case("e9", {}, d1c_res_case(0x80000000, 2, 5, 1)),
        Case("e10", {}, d1c_res_case(0x80, 0x80, 1, 3)),
        Case("e11", {}, d1c_res_case(2, 2, 3, 1, w0=0xC8)),
        Case("e12", {}, d1c_res_case(2, 2, 3, 1, w0=0xC7)),
        Case("e13", {}, d1c_res_case(2, 2, 3, 1, w0=0xFFFF)),
        Case("e14", {}, d1c_res_case(0, 0, 1, 3)),
        Case("e15", {}, d1c_res_case(2, 2, 0x80, 1)),
    ], eax_mask=0,
       mutants=("@eqfirst", "@two", "@bsign", "@ugt", "@d", "@ge0", "@ge1", "@inc", "@cap",
                "@wsext", "@widx", "@r2", "@store")),
    # 0x25848 flow_stage_pick: no argument. The stage word 0x104AFC on the byte 0x104B17; 0x5D7DC
    # is a stub (its EAX per case). EDX enters dirty: 0x25895 clears DH only and the word store
    # takes DX. The k = -2 arm (0x104AD4 = -1) reads a stack byte below the frame (0x259A8), which
    # the port does not model: case f10 has no entry that could match it, so both exit unchanged.
    # Mask 0: record §1.
    Spec("flow_stage_pick", 0x25848, [
        Case("g0", D1C_EDX, d1c_stage_case(0, b1f=3), {0x5D7DC: 0x12340005}),
        Case("g1", D1C_EDX, d1c_stage_case(0, b1f=0, cb=b"\x02\x7f\x7f", tbl={2: 3}), {0x5D7DC: 1}),
        Case("g2", D1C_EDX, d1c_stage_case(0, b1f=1, cb=b"\x7f\xff\x7f", tbl={-1: 6}), {0x5D7DC: 5}),
        Case("g3", D1C_EDX, d1c_stage_case(0, b1f=2, cb=b"\x7f\x7f\x01", tbl={1: 1}), {0x5D7DC: 5}),
        Case("g4", D1C_EDX, d1c_stage_case(0, b1f=2, cb=b"\x7f\x7f\x01", tbl={1: 1}), {0x5D7DC: 4}),
        Case("g5", D1C_EDX, d1c_stage_case(0, b1f=0, cb=b"\x00\x7f\x7f", tbl={0: 0xF0}),
             {0x5D7DC: 0xFFFF0020}),
        Case("g6", D1C_EDX, d1c_stage_case(2, b1f=3)),
        Case("f0", D1C_EDX, d1c_stage_case(1, lst=b"\x00\x05\x00\x85\x01\x00\x03"), {0x5D7DC: 1}),
        Case("f1", D1C_EDX, d1c_stage_case(1, lst=b"\x00\x05\x00\x85\x01\x00\x03"), {0x5D7DC: 0}),
        Case("f2", D1C_EDX, d1c_stage_case(1, lst=b"\x00\x05\x00\x85\x01\x00\x03"), {0x5D7DC: 2}),
        Case("f3", D1C_EDX, d1c_stage_case(1, lst=b"\x05\x41\x00\x85\x01\x06\x03", fr0=5, fr1=1),
             {0x5D7DC: 3}),
        Case("f4", D1C_EDX, d1c_stage_case(1, lst=b"\x83\x42\xc2\x05\x83\x42\x83")),
        Case("f5", D1C_EDX, d1c_stage_case(1, lst=b"\x83\x42\xc2\x03\x83\x42\x83", ad4=2, afc=3)),
        Case("f6", D1C_EDX, d1c_stage_case(1, lst=b"\x83\x42\xc2\x03\x83\x42\x83", ad4=2, afc=6)),
        Case("f7", D1C_EDX, d1c_stage_case(1, lst=b"\x83\x42\xc2\x03\x83\x42\x83", ad4=2,
                                           afc=0xFFFF)),
        Case("f8", D1C_EDX, d1c_stage_case(1, lst=b"\x42\xc2\x83\x03\x83\x42\x83", ad4=0)),
        Case("f9", D1C_EDX, d1c_stage_case(1, lst=b"\x42\xc2\x83\x03\x83\x42\x83", ad4=1)),
        Case("f10", D1C_EDX, d1c_stage_case(1, lst=b"\x42\xc2\x83\x03\x83\x42\x83",
                                            ad4=0xFFFFFFFF)),
        Case("f11", D1C_EDX, d1c_stage_case(0xFF, lst=b"\x00\x05\x00\x85\x01\x00\x03"),
             {0x5D7DC: 1}),
        Case("f12", D1C_EDX, d1c_stage_case(1, lst=b"\x83\x03\x83\x03\x83\x03\x83", ad4=0)),
    ], calls=(E.Call(0x5D7DC, ("eax",)),), eax_mask=0,
       mutants=("@rng7", "@rng6", "@csext", "@cidx", "@plus", "@mod", "@ge", "@mode2", "@or40",
                "@mask", "@n", "@pick", "@wrap", "@k", "@step", "@word")),
    # 0x25FDC flow_round_bonus_a and 0x2604C flow_round_bonus_b: EAX = the side. The card spawn
    # (0x104529 bit 1 picks the descriptor and a4) into 0x104AB4/0x104AB0, the timer byte
    # 0x104B0D/0x104B0F cleared, the arm bit 0x104AE9 |= 0x80 / 0x104AEA |= 1, then
    # 0x41310(side, 0x4E20). Both callees are stubs. Mask 0: record §1.
    Spec("flow_round_bonus_a", 0x25FDC, [
        Case("a0", {"eax": 0}, d1c_bonus_case(0x02, 0x01, 0x40), {0x2AE14: 0x0010C200}),
        Case("a1", {"eax": 1}, d1c_bonus_case(0xFD, 0x7F, 0xFE), {0x2AE14: 0x0010C201}),
        Case("a2", {"eax": 1}, d1c_bonus_case(0xFF, 0x80, 0x01), {0x2AE14: 0}),
    ], calls=(SPAWN, P7_41310), eax_mask=0,
       mutants=("@bit", "@desc", "@a2", "@a3", "@a4", "@a5", "@slot", "@clr", "@or", "@side",
                "@val", "@order")),
    Spec("flow_round_bonus_b", 0x2604C, [
        Case("b0", {"eax": 0}, d1c_bonus_case(0x02, 0x01, 0x40), {0x2AE14: 0x0010C210}),
        Case("b1", {"eax": 1}, d1c_bonus_case(0xFD, 0x7F, 0xFE), {0x2AE14: 0x0010C211}),
        Case("b2", {"eax": 1}, d1c_bonus_case(0xFF, 0x80, 0x01), {0x2AE14: 0}),
    ], calls=(SPAWN, P7_41310), eax_mask=0,
       mutants=("@bit", "@desc", "@a2", "@a3", "@a4", "@a5", "@slot", "@clr", "@or", "@side",
                "@val", "@order")),
    # 0x28C38 flow_winner_pose_step: no argument. The signed dword 0x104AD8 is incremented and
    # taken modulo 0x104AA8 (`idiv`); on a zero remainder each side whose +0x5A byte is below
    # 0x77 (zero-extended) gets 0x392A0(slot, 1, 0), a stub. A zero divisor and INT_MIN / -1
    # fault in the raw (#DE) and have no case (record §4). Mask 0: record §1.
    Spec("flow_winner_pose_step", 0x28C38, [
        Case("v0", {}, d1c_pose_case(5, 3, 0x76, 0x76)),
        Case("v1", {}, d1c_pose_case(4, 3, 0x76, 0x76)),
        Case("v2", {}, d1c_pose_case(0xFFFFFFF9, 3, 0x77, 0x00)),
        Case("v3", {}, d1c_pose_case(0xFFFFFFFA, 3, 0x00, 0x00)),
        Case("v4", {}, d1c_pose_case(5, 0xFFFFFFFD, 0x80, 0x77)),
        Case("v5", {}, d1c_pose_case(0x7FFFFFFE, 1, 0x00, 0x80)),
        Case("v6", {}, d1c_pose_case(0xFFFFFFFF, 7, 0x76, 0x78)),
    ], calls=(E.Call(0x392A0, ("eax", "edx", "ebx"), clobbers=("ebx", "edx")),), eax_mask=0,
       mutants=("@inc", "@pre", "@unsigned", "@le", "@sext", "@slot", "@args", "@byte2",
                "@early")),
    # 0x1CAB8 sound_music_volume: EAX = the volume. Unchanged from 0xA2CB8 nothing happens; else
    # it is stored and, with a sequence handle whose 0x5DEED status is 4 (the whole EAX, 0x1CAE1),
    # 0x5DECA takes it over 500 ms. The AIL thunks are stubs; 0x5DECA's volume and time are its
    # second and third stack arguments (the handle is a host pointer in the port). Mask 0: record §1.
    Spec("sound_music_volume", 0x1CAB8, [
        Case("u0", {"eax": 0x40}, d1c_vol_case(mus=0x40), {0x5DEED: 4}),
        Case("u1", {"eax": 0x50}, d1c_vol_case(mus=0x40, seq=0), {0x5DEED: 4}),
        Case("u2", {"eax": 0x50}, d1c_vol_case(mus=0x40), {0x5DEED: 4}),
        Case("u3", {"eax": 0x50}, d1c_vol_case(mus=0x40), {0x5DEED: 3}),
        Case("u4", {"eax": 0x50}, d1c_vol_case(mus=0x40), {0x5DEED: 0x104}),
        Case("u5", {"eax": 0xFFFFFFFF}, d1c_vol_case(mus=0), {0x5DEED: 4}),
    ], calls=(E.Call(0x5DEED, (), eax=0),
              E.Call(0x5DECA, ("s1", "s2"), clobbers=("ebx", "ecx", "edx"))), eax_mask=0,
       mutants=("@same", "@store", "@seq", "@status", "@ms", "@vol", "@order", "@sfx")),
    # 0x1CED4 sound_sfx_volume: EAX = the volume. Unchanged from 0xA2CB4 nothing happens; else it
    # is stored and each of the four slots whose 0x5DD03 status is 4 gets it through 0x5DCC5 (its
    # second stack argument). One stub EAX serves the four status calls of a case. Mask 0: §1.
    Spec("sound_sfx_volume", 0x1CED4, [
        Case("x0", {"eax": 0x30}, d1c_vol_case(sfx=0x30), {0x5DD03: 4}),
        Case("x1", {"eax": 0x55}, d1c_vol_case(sfx=0x30), {0x5DD03: 4}),
        Case("x2", {"eax": 0x55}, d1c_vol_case(sfx=0x30), {0x5DD03: 0}),
        Case("x3", {"eax": 0x55}, d1c_vol_case(sfx=0x30), {0x5DD03: 0x104}),
        Case("x4", {"eax": 0xFFFFFF80}, d1c_vol_case(sfx=0x30), {0x5DD03: 4}),
    ], calls=(E.Call(0x5DD03, (), eax=0),
              E.Call(0x5DCC5, ("s1",), clobbers=("ebx", "ecx", "edx"))), eax_mask=0,
       mutants=("@same", "@store", "@status", "@count", "@vol", "@mus")),
    # 0x2C088 prompt_press_start_clear: EAX = col, EDX = row, EBX = side. With 0x104529 bit 1 the
    # side's sprite 0x105BF0[side], when set, is marked dead (0x2B150) and its slot zeroed, and
    # string 0x48's cells are released at 0x105C06/0x105C07 (zero-extended); else at col/row. Mode
    # 0x1000. 0x1C500 runs real over a stubbed 0x474E4 (which writes the string); 0x2B150 and
    # 0x2F280 are stubs. Mask 0: record §1.
    Spec("prompt_press_start_clear", 0x2C088, [
        Case("p0", {"eax": 5, "edx": 9, "ebx": 0}, d1c_prompt_case(0x02, s0=0x0010A300)),
        Case("p1", {"eax": 5, "edx": 9, "ebx": 1}, d1c_prompt_case(0x02, s0=0x0010A300, c06=0x92,
                                                                  c07=0x8C)),
        Case("p2", {"eax": 5, "edx": 9, "ebx": 1}, d1c_prompt_case(0xFF, s0=0x0010A300,
                                                                  s1=0x0010A340)),
        Case("p3", {"eax": 0xFFFFFFFF, "edx": 0x0C, "ebx": 0}, d1c_prompt_case(0xFD, s0=0x0010A300)),
        Case("p4", {"eax": 7, "edx": 0x0B, "ebx": 1}, d1c_prompt_case(0x00, s1=0x0010A340),
             {0x474E4: 0}),
    ], allow_calls=(0x1C500,), calls=(E.Call(0x2B150, ("eax",)), D1C_STR, D1C_RELEASE), eax_mask=0,
       mutants=("@bit", "@dead", "@zero", "@side", "@pos", "@sext", "@swap", "@id", "@mode",
                "@order")),
    # 0x2C1D4 prompt_insert_coin_blink: EAX = col, EDX = row. On the phase word 0xEF6DC & 0x1F:
    # 0 builds string 0x49 + the image string 0x809C4 + string 0x4B in a 0x14-byte stack buffer
    # (unbounded copies), draws it through 0x2F198 (mode 0x3000) and stores the col byte, the
    # length and the row byte; 0x18 releases 0x105BF8 cells at col/row through 0x2F388
    # (0x2C1AD, the block before the entry); else nothing. 0x2F198's seam takes the string's
    # first 16 bytes by value, so the two cases build 15 and 19 characters (n1 pokes a five-
    # character middle string, which fills the 0x14 bytes exactly): a shorter string
    # leaves stack bytes after its NUL in that window (not comparable), a longer one overruns
    # the raw's buffer into its saved col (the port bounds the copy: record §4). Mask 0: §1.
    Spec("prompt_insert_coin_blink", 0x2C1D4, [
        Case("n0", {"eax": 0x0E, "edx": 0x0C}, d1c_coin_case(0xFFE0)),
        Case("n1", {"eax": 0x12345678, "edx": 0x9ABCDEF0}, d1c_coin_case(0x0100, mid=b"7abcd\x00")),
        Case("n2", {"eax": 0x0E, "edx": 0x0C}, d1c_coin_case(0x0038)),
        Case("n3", {"eax": 0x0E, "edx": 0x0C}, d1c_coin_case(0x0005)),
        Case("n4", {"eax": 0xFFFFFFFF, "edx": 3}, d1c_coin_case(0xFF18, bf8=0x21)),
    ], allow_calls=(0x1C500,),
       calls=(D1C_STR,
              E.Call(0x2F198, ("eax", "edx", "[ebx]", "[ebx+4]", "[ebx+8]", "[ebx+12]", "ecx"),
                     mode="stub", clobbers=("ebx", "ecx", "edx")),
              E.Call(0x2F388, ("eax", "edx", "ebx"), clobbers=("ebx", "edx"))), eax_mask=0,
       mutants=("@phase", "@p18", "@order", "@mid", "@mode", "@c06", "@c07", "@swap", "@len",
                "@cnt", "@relpos")),
    # 0x2BF08 game_overlay_step: no argument. Off in the attract (0x9AD58). Free play (0x2CAA8
    # returns 0; real on both sides, the port tests 0x105D60 inline): string 0x45 held (phase
    # 0xEF6DC & 0x20) or released at col -1. No credits: on a phase & 0x1F with the byte 0x105C04
    # clear nothing is drawn; else string 0x47 held or released at col 0xD. The row is the byte
    # 0x105C05 (zero-extended), the mode 0 and 0x105C04 = 0 at each drawing exit. 0x1C500 runs
    # real over the stubbed 0x474E4. The credits arm 0x2BF87 formats through the runtime's
    # sprintf 0x65546, which the harness cannot run (record §4): no case enters it.
    # Mask 0: record §1.
    Spec("game_overlay_step", 0x2BF08, [
        Case("o0", {}, d1c_ovl_case(attract=1, free=1, phase=0x20)),
        Case("o1", {}, d1c_ovl_case(free=1, phase=0x0020)),
        Case("o2", {}, d1c_ovl_case(free=1, phase=0xFFDF, row=0x91)),
        Case("o3", {}, d1c_ovl_case(free=0x80, phase=0x0020, credits=3)),
        Case("o4", {}, d1c_ovl_case(phase=0x0021, c04=0)),
        Case("o5", {}, d1c_ovl_case(phase=0x0021, c04=1)),
        Case("o6", {}, d1c_ovl_case(phase=0x0100, c04=0, row=0x91)),
        Case("o7", {}, d1c_ovl_case(phase=0x0020, c04=0)),
        Case("o8", {}, d1c_ovl_case(phase=0x0001, c04=5)),
    ], allow_calls=(0x1C500, 0x2CAA8),
       calls=(D1C_STR,
              E.Call(0x2F4BC, ("eax", "edx", "ebx", "ecx"), clobbers=("ebx", "ecx", "edx")),
              D1C_RELEASE), eax_mask=0,
       unhit_named={0x2BF87: "the credits arm: 0x2BFA7 calls the runtime's sprintf 0x65546, whose "
                             "0x6C8EB core leaves the flat model under the harness (it writes "
                             "0xFF30.. and stops on an int3 at 0x10000); the port formats with "
                             "snprintf and has no seam there (record §4)"},
       mutants=("@attract", "@free", "@id45", "@id47", "@p20", "@hold", "@col", "@colf", "@gate",
                "@c04", "@clr", "@sext", "@order")),
    # 0x1D0BC sound_buffers_alloc: no argument. Once (the byte 0xA2CB0): with a sequence and an
    # MDI handle and no MIDI buffer, 0x5100 zeroed bytes into 0x1028D0; with a DIG driver, slot
    # i's +0x10 = 0x8C00 (i = 0) or 0x6000 bytes while the slot is empty. The allocator 0x1C308
    # is a stub (EDX = the size is compared; its EAX = 0x41 tag has no C counterpart) whose EAX
    # is the block for every call of a case, inside the image so the runtime's memset (0x61A70
    # and its 0x65490/0x654C7, real on the raw side against the port's libc memset) is diffed. The two
    # failure arms end in the runtime's printf 0x62734, which the harness cannot run and the
    # port does not call (record §4): no case enters them. Mask 0xFF: `mov al,dl` / `xor al,al`.
    Spec("sound_buffers_alloc", 0x1D0BC, [
        Case("q0", {}, d1c_buf_case(once=1)),
        Case("q1", {}, d1c_buf_case()),
        Case("q2", {}, d1c_buf_case(c4=0, c8=0)),
        Case("q3", {}, d1c_buf_case(c0=0, slots=(0, 0, 0x000C8000, 0))),
        Case("q4", {}, d1c_buf_case(d0=0x000C8000, slots=(0, 0x000C8000, 0, 0))),
        Case("q5", {}, d1c_buf_case(c4=0, slots=(0, 0, 0, 0x000C8000))),
    ], allow_calls=(0x61A70, 0x65490, 0x654C7),
       calls=(E.Call(0x1C308, ("edx",), eax=0x000C0000, clobbers=("edx",)),), eax_mask=0xFF,
       unhit_named={0x1D10E: "the MIDI buffer's failure arm: it ends in the runtime's printf "
                             "0x62734 (0x1D12A), which the harness cannot run and the port does "
                             "not call (record §4)",
                    0x1D183: "the no-slot-0-buffer arm: the same printf (0x1D18D), then "
                             "0x1028C8 = 0 (record §4)"},
       mutants=("@once", "@c4", "@c0", "@d0", "@size", "@zero", "@len", "@c8", "@first", "@rest",
                "@stride", "@four", "@stop", "@set", "@ret")),
]
D1_SPECS += D1C_SPECS


# ---- track D1 batch d1e (record 2026-10-11-reverse-d1e): game/fight.c, first half ----------------
#
# Every poke stays inside the image; the scratch records sit in its zero BSS (0x10A000..0x10B0CF).

D1E_REC0, D1E_REC1 = 0x0010A300, 0x0010A400       # the two fighter records (E3_REC/E3_REC2)


def d1e_x_case(x0, x1):
    """0x13134: the two slots' record pointers and each record's +0x14..+0x1F: the x at +0x18
    between two neighbours that would give the other verdict if read instead."""
    n0 = 0x3301 if not (x0 > 0x3300 and x1 > 0x3300) else 0
    return {**SLOT_PTRS,
            D1E_REC0 + 0x14: le32(n0) + le32(x0) + le32(n0),
            D1E_REC1 + 0x14: le32(n0) + le32(x1) + le32(n0)}


def d1e_rate_case(side, ch, v, o_ch=1, o_v=0x40):
    """0x1D4D0: the side slot's +0x7A/+0x7B and +0x5C/+0x5D, the other slot's seeded apart, and
    the 0xA769C rows made distinct (the image's seven rows are all 02 04 08 0C, which cannot
    prove the row index): row c = (c*0x10+1 .. c*0x10+4), the half-stride row at ch*2 included."""
    so, oo = DS_SLOTS + side * 0x94, DS_SLOTS + (1 - side) * 0x94
    p = {**SLOT_PTRS,
         so + 0x7A: bytes([ch, (ch + 3) & 0x7F]), so + 0x5C: bytes([(v + 0x20) & 0xFF, v]),
         oo + 0x7A: bytes([o_ch, 5]), oo + 0x5C: bytes([0x11, o_v]),
         0x000A769C: bytes((c * 0x10 + k + 1) & 0xFF for c in range(8) for k in range(4))}
    for c in {ch, o_ch, (ch + 3) & 0x7F}:
        if c >= 8:
            p[0x000A769C + c * 4] = bytes([0xE1, 0xE2, 0xE3, 0xE4])
            p[0x000A769C + c * 2] = bytes([0xD1, 0xD2, 0xD3, 0xD4])
    return p


def d1e_bits_case(side, w, other=None):
    """0x1D74C: the four words at 0x1088E0 (the side's, the other side's with the opposite
    verdict, and the two a side*4 index would reach)."""
    o = (0x0000 if (w & 0x30) else 0x0030) if other is None else other
    words = [0, 0, o, o]
    words[side], words[1 - side] = w, o
    return {0x001088E0: b"".join(le16(x) for x in words)}


# marker and HUD records: never dereferenced (0x2B150/0x2BC30 are stubs), every byte non-zero so a
# narrow store shows, and below MEM_SIZE
D1E_MK0, D1E_MK1, D1E_MK2 = 0x01A50511, 0x02A64522, 0x03A78533


def d1e_marker_case(r0, r1, r2=D1E_MK2, far=None):
    """0x1D810/0x1D7B8/0x1D838: the marker slots DS_001028E0[0..1] between two seeded
    neighbours (0x1028DC before, 0x1028E8 after: where a side*8 index, or side 2, lands); `far`
    seeds the slot of side 0x100 (0x102CE0), for the full-register side test."""
    p = {0x001028DC: le32(0xDCDCDCDC) + le32(r0) + le32(r1) + le32(r2) + le32(0xECECECEC)}
    if far is not None:
        p[0x00102CDC] = le32(0xCDCDCDCD) + le32(far) + le32(0xCECECECE)
    return p


def d1e_pulse_case(w0, w1, b0=0x10, b1=0x21):
    """0x1DA08: the two countdown words at 0x102908 between seeded neighbours (0x102906 before,
    0x10290C after), the two DS_001028F8 records between theirs, and each slot's +0x59..+0x5B
    (the +0x5A byte the reload reads, its neighbours seeded apart)."""
    return {0x00102906: le16(0x0607) + le16(w0) + le16(w1) + le16(0x0C0D),
            0x001028F4: le32(0xF4F4F4F4) + le32(D1E_MK0) + le32(D1E_MK1) + le32(0x00F0F0F0),
            DS_SLOTS + 0x59: bytes([0x7F, b0, 0x02]),
            DS_SLOTS + 0x94 + 0x59: bytes([0x7E, b1, 0x04])}


# a pset table base and a decoy for DS_001014F4 (0x10 out of phase, so no entry overlaps): the
# largest index used, 0x202, ends at 0x10804C, inside the image
D1E_PSET, D1E_POOL = 0x00104000, 0x00105010


def d1e_dust_case(side, f, other=None):
    """0x29CDC: DS_00105B34[side] between seeded neighbours, the other side's flag opposite."""
    o = (0 if f else 1) if other is None else other
    b = [0x33, 0, 0, 0x36]
    b[1 + side], b[2 - side] = f, o
    return {0x00105B33: bytes(b)}


def d1e_term_pokes(rec, idx, val, hi=0x0001):
    """One fighter record for 0x2BE00: +0x54 a decoy index, +0x56 the pset index word, +0x58 a
    small non-zero word (a dword read of +0x56 stays inside mem[]), and the pset entry's
    +0/+4/+8 (val at +4), with the entries a byte-wide, half-stride or decoy index reaches."""
    p = {rec + 0x54: le16(idx ^ 3) + le16(idx) + le16(hi),
         D1E_PSET + idx * 0x20: le32(0x11110000 | idx) + le32(val) + le32(0x88880000 | idx)}
    for other in {idx & 0xFF, idx ^ 3}:
        if other != idx:
            p[D1E_PSET + other * 0x20] = le32(0x22220000 | other) + le32(0x44440000 | other) \
                + le32(0x66660000 | other)
    if idx * 0x10 % 0x20:           # the half-stride entry falls between two entries
        p[D1E_PSET + idx * 0x10] = le32(0x33330000 | idx) + le32(0x55550000 | idx) + le32(0x77770000 | idx)
    p[D1E_POOL + idx * 0x20] = le32(0x99990000 | idx) + le32(0xAAAA0000 | idx) + le32(0xBBBB0000 | idx)
    return p


def d1e_term_case(*recs):
    """0x2BE00/0x2BE1C: the pset base DS_001014EC (and the pool pointer DS_001014F4, a decoy),
    then each (rec, idx, val)."""
    p = {0x001014EC: le32(D1E_PSET), 0x001014F4: le32(D1E_POOL)}
    for rec, idx, val in recs:
        p.update(d1e_term_pokes(rec, idx, val))
    return p


def d1e_x4c_case(x44):
    """0x2BE4C: the record's +0x40..+0x4B, the dword at +0x44 between two decoys."""
    return {D1E_REC0 + 0x40: le32(0x00070000) + le32(x44) + le32(0x00090000)}


D1E_CROWD = 0x0010A800                          # a crowd table in the zero BSS


def d1e_crowd_case(scene, entries, n=None, stride=0xC):
    """0x2C320: the scene's count word (0xBBD98) and table pointer (0xBBDA8), both between
    decoys a wrong index scale reaches, the DS_00105C08 sentinel, and the 12-byte records
    (a2 dword, a4 word, a3 word, a5 low word, descriptor index byte, a5 high byte)."""
    n = len(entries) if n is None else n
    p = {0x00105C08: le32(0x5C5C5C5C)}
    # the count and pointer rows of every scene 0..7: only `scene` holds this case's values. The
    # scenes used are even, and each decoy pointer is an in-image address with a low word <= 0x20:
    # a mutant that takes the count from scene*4 (it lands in the pointer row from scene 4 up) or
    # the pointer from scene*2 then walks a short in-image table.
    counts = [0 if n else 1] * 8
    ptrs = [0x00100004 + 4 * k for k in range(8)]
    counts[scene] = n
    ptrs[scene] = D1E_CROWD
    p[0x000BBD98] = b"".join(le16(c) for c in counts)
    p[0x000BBDA8] = b"".join(le32(x) for x in ptrs)
    buf = b""
    for a2, a4, a3, lo, idx, hi in entries:
        buf += le32(a2) + le16(a4) + le16(a3) + le16(lo) + bytes([idx, hi])
    buf += le32(0x0E0E0E0E) + le16(0x0E0E) + le16(0x0E0E) + le16(0x0E0E) + bytes([0, 0x0E])   # one past the end
    for off in range(0, len(buf), 0x40):
        p[D1E_CROWD + off] = buf[off:off + 0x40]
    return p


def d1e_reset_case(side):
    """0x33C18: both slots' fields seeded non-zero with their neighbours (+0x3B..+0x40,
    +0x5A..+0x5C, +0x7E..+0x83), and the 0x10885E..0x108865 words (the two sides' 0x108860
    words between decoys)."""
    p = {0x0010885E: le16(0x5E5E) + le16(0x6A6A) + le16(0x6B6B) + le16(0x6C6C)}
    for k in (0, 1):
        so = DS_SLOTS + k * 0x94
        t = 0x10 * (k + 1)
        p[so + 0x3B] = bytes([t + 1]) + le32(0x3C3C3C3C + k) + bytes([t + 2])
        p[so + 0x5A] = bytes([t + 3, t + 4, t + 5])
        p[so + 0x7E] = bytes([t + 6, t + 7, t + 8, t + 9, t + 10, t + 11])
    return p


D1E_HB_CONST = {1: 0x000E39D0, 2: 0x000ECBD8, 3: 0x000D2134, 4: 0x000EA604, 5: 0x000D3E08,
                6: 0x000E061C}                  # 0x33EEC's seven targets; 0 and > 6 read 0xE6DD0
D1E_HB_K = {0: 0x0EE4, 1: 0x12A2, 2: 0x0BD4, 3: 0x16B5, 4: 0x1F9E, 5: 0x2F19, 6: 0x32D7}   # the image's words


def d1e_hb_side(side, ch, s, bit15=0, f41=0, bar=0x1234, p29=0x00, idx_f=None, idx_s=None,
                const=None, raw=None, bar_s=None):
    """One side of 0x33F08: the slot record (0x1077A8[side]) with its fighter, its bar actor
    and the bar's word table. The fighter's pset word is the character constant + s (| bit 15),
    or `raw`; `const` pokes the character's constant word; `bar_s` is the bar table index that
    holds `bar` (default s, when it is in 0..0x4B0)."""
    rec = 0x0010A300 + side * 0x100
    fighter = 0x0010A500 + side * 0x80
    actor = 0x0010A600 + side * 0x80
    bars = 0x00108800 + side * 0xC00
    idx_f = 3 + side if idx_f is None else idx_f
    idx_s = 9 + side if idx_s is None else idx_s
    k = D1E_HB_K[ch if ch <= 6 else 0] if const is None else const
    aw = (((k + s) & 0x7FFF) | (0x8000 if bit15 else 0)) if raw is None else raw
    p = {rec: le32(fighter) + le32(actor), rec + 0x20: le32(bars + 0x10) + le32(bars),
         rec + 0x41: bytes([f41]), rec + 0x7A: bytes([ch, (ch + 1) % 7]),
         fighter + 0x54: le16(idx_f + 0x10) + le16(idx_f),
         actor + 8: le32(0x5A5A5A5A) + le32(0x6B6B6B6B), actor + 0x28: bytes([0x28, p29, 0x2A]),
         actor + 0x54: le16(0x0077) + le16(idx_s),
         D1E_PSET + idx_f * 0x20: le16(aw) + le16(0xF00F),
         D1E_PSET + (idx_f + 0x10) * 0x20: le16(aw ^ 0x8040) + le16(0xF11F),
         D1E_PSET + idx_s * 0x20: le16(0x7777) + le16(0x8888)}
    if const is not None:
        p[D1E_HB_CONST.get(ch, 0x000E6DD0)] = le16(const)
    bar_s = (s if 0 <= s <= 0x4B0 else None) if bar_s is None else bar_s
    if bar_s is not None:
        if bar_s:
            p[bars + bar_s * 2 - 2] = le16(0x0B0B) + le16(bar) + le16(0x0D0D)
        else:
            p[bars] = le16(bar) + le16(0x0D0D)
        p[bars + 0x10 + bar_s * 2] = le16(bar ^ 0x0101)
    return p


def d1e_hb_case(s0=None, s1=None):
    """0x33F08: the pset table base, the two slot pointers (0 = that side has no slot) and each
    side's fixture."""
    p = {0x001014EC: le32(D1E_PSET),
         0x001077A8: le32(0x0010A300 if s0 else 0) + le32(0x0010A400 if s1 else 0)}
    for side, kw in ((0, s0), (1, s1)):
        if kw:
            p.update(d1e_hb_side(side, **kw))
    return p


def d1e_gate_case(b1d, b14, s63, s42, s52, s54):
    """0x36E2C: the two mode bytes (0x104B1D, 0x104B14) and the slot's +0x42, +0x52, +0x54 and
    +0x63, each between neighbours; +0x53 and +0x55 hold a value with the opposite verdict."""
    ok52 = s52 in (0, 1, 5, 6, 7)
    return {0x00104B13: bytes([0x13, b14, 0x15]), 0x00104B1C: bytes([0x1C, b1d, 0x1E]),
            D1E_REC0 + 0x41: bytes([0xEF, s42, 0x00]),
            D1E_REC0 + 0x51: bytes([0x02, s52, 2 if ok52 else 0, s54, 9 if s54 <= 1 else 0]),
            D1E_REC0 + 0x62: bytes([0x00 if s63 else 0x62, s63, 0x00 if s63 else 0x64])}


def d1e_scratch_case():
    """0x38BEC: the 0x107D16..0x107D31 words and the 0x107A7F..0x107B00 bytes, all non-zero
    (both sides' fields, and the bytes before and after each range)."""
    p = {0x00107D16: b"".join(le16(0x8000 | (a & 0xFFF)) for a in range(0x107D16, 0x107D32, 2))}
    buf = bytes((i % 0xFD) + 1 for i in range(0x82))
    for off in range(0, len(buf), 0x40):
        p[0x00107A7F + off] = buf[off:off + 0x40]
    return p


D1E_E0 = 0x000C8602        # a 0x413C8 handle whose +0x56 word is 0xC85BC[3]'s descriptor + 0x100
# 0x2AE14 as 0x413C8 sees it: each stubbed spawn writes the word 0xABCD at its descriptor + 0x100,
# so the D1E_E0 record's +0x56 changes when the i = 3 spawn returns (the raw reloads it each pass)
D1E_SPAWN_W = dataclasses.replace(SPAWN, writes=((0, 0x100, b"\xcd\xab"),))


def d1e_413_case(handle, w0):
    """0x413C8: DS_001080F4 and the seven DS_001080C0 handles between seeded neighbours, and the
    first handle's +0x54/+0x56 words."""
    return {0x001080BC: le32(0xBCBCBCBC) + b"".join(le32(0xEEEEEEE0 + i) for i in range(7))
            + le32(0xDCDCDCDC),
            0x001080F0: le32(0xF0F0F0F0) + le32(0xF4F4F4F4) + le32(0xF8F8F8F8),
            handle + 0x54: le16(w0 ^ 0x0101) + le16(w0)}


# 0x2AE14 as 0x43964 sees it: each stubbed spawn also writes a marker over DS_00108154[0], so
# after the second spawn the side-0 entry handle (the marker) and panel handle (the stub's EAX)
# differ, and which of the two 0x2BCF4/0x2A17C take is observable (one stub has one EAX)
D1E_SPAWN_A = dataclasses.replace(SPAWN, writes=((None, 0x00108154, le32(0x01A50511)),))


def d1e_marks_case(marks, count, before=0x55, after=0x5D):
    """0x4246C/0x4248C: the seven stage bytes 0x108106..0x10810C between two neighbours and the
    count byte 0x108111 between two more."""
    assert len(marks) == 7
    return {0x00108105: bytes([before] + list(marks) + [after]),
            0x00108110: bytes([0x10, count, 0x12])}


def d1e_entry_case(side, ch, o_ch, a2=None, a4=None, pw=None):
    """0x43964: the two character bytes 0x108166[side] between neighbours, the four handle
    slots 0x108154../0x10815C.. with sentinels, and the 0xC88A6 words made distinct (the
    image's are 0x540 for characters 0..3 and 0xEC0 for 4..6). `a2`/`a4`/`pw` poke the
    character's 0xC8898/0xC88A6/0xC88F8 word."""
    chars = [0, 0]
    chars[side], chars[1 - side] = ch, o_ch
    words = [0x0540 + 0x111 * i for i in range(7)]
    if a4 is not None:
        words[ch] = a4
    p = {0x00108150: le32(0x50505050) + le32(0xAAAA5454) + le32(0xAAAA5858) + le32(0xBBBB5C5C)
         + le32(0xBBBB6060),
         0x00108164: bytes([0x64, 0x65] + chars + [0x68]),
         0x000C88A6: b"".join(le16(w) for w in words)}
    if a2 is not None:
        p[0x000C8898 + ch * 2] = le16(a2)
    if pw is not None:
        p[0x000C88F8 + ch * 2] = le16(pw)
    return p


D1E_SPECS = [
    # 0x13134 fight_13134: no argument; EAX = 1/0 (`mov eax,1` 0x13150/0x13172, `xor eax,eax`
    # 0x13178), read whole by its caller (`test eax,eax` at 0x4BDBF).
    Spec("fight_13134", 0x13134, [
        Case("x0", {}, d1e_x_case(-0x3301, -0x3301)),
        Case("x1", {}, d1e_x_case(-0x3301, -0x3300)),
        Case("x2", {}, d1e_x_case(-0x3300, -0x3301)),
        Case("x3", {}, d1e_x_case(0x3301, 0x3301)),
        Case("x4", {}, d1e_x_case(0x3301, 0x3300)),
        Case("x5", {}, d1e_x_case(0x3300, 0x3301)),
        Case("x6", {}, d1e_x_case(-0x3301, 0x3301)),
        Case("x7", {}, d1e_x_case(0x3301, -0x3301)),
        Case("x8", {}, d1e_x_case(-0x80000000, -0x80000000)),
        Case("x9", {}, d1e_x_case(0x7FFFFFFF, 0x7FFFFFFF)),
        Case("x10", {}, d1e_x_case(-1, -1)),
        Case("x11", {}, d1e_x_case(0, 0)),
    ], eax_mask=0xFFFFFFFF, mutants=("@lt", "@gt", "@unsigned", "@field", "@rec1", "@rec0", "@or")),
    # 0x1D4D0 fight_hud_rate: EAX = side; 0x33A10 (allow) gives the side's slot (out[3]). The
    # result is `and eax,0xff` (0x1D511/0x1D526/0x1D536), read whole (`mov edi,eax` at 0x1D568).
    Spec("fight_hud_rate", 0x1D4D0, [
        Case("r0", {"eax": 0}, d1e_rate_case(0, 2, 0x3E)),
        Case("r1", {"eax": 0}, d1e_rate_case(0, 2, 0x3D)),
        Case("r2", {"eax": 0}, d1e_rate_case(0, 3, 0x30)),
        Case("r3", {"eax": 0}, d1e_rate_case(0, 3, 0x2F)),
        Case("r4", {"eax": 1}, d1e_rate_case(1, 4, 0x23, o_ch=2, o_v=0x10)),
        Case("r5", {"eax": 1}, d1e_rate_case(1, 4, 0x22, o_ch=2, o_v=0x40)),
        Case("r6", {"eax": 0}, d1e_rate_case(0, 0, 0x00)),
        Case("r7", {"eax": 0}, d1e_rate_case(0, 6, 0x80)),
        Case("r8", {"eax": 1}, d1e_rate_case(1, 6, 0xFF, o_ch=0, o_v=0)),
        Case("r9", {"eax": 0}, d1e_rate_case(0, 0x80, 0x30)),
    ], allow_calls=(0x33A10,), eax_mask=0xFFFFFFFF,
       mutants=("@side", "@ch", "@chsext", "@scale", "@field", "@sext", "@b0", "@b1", "@b2", "@col")),
    # 0x1D74C fight_cmd_bits_30: EAX = side; `and eax,0xffff; setne al` leaves EAX = 0/1 (the
    # caller tests AL, 0x1D604; the whole register is compared).
    Spec("fight_cmd_bits_30", 0x1D74C, [
        Case("b0", {"eax": 0}, d1e_bits_case(0, 0x0000)),
        Case("b1", {"eax": 0}, d1e_bits_case(0, 0x0010)),
        Case("b2", {"eax": 0}, d1e_bits_case(0, 0x0020)),
        Case("b3", {"eax": 1}, d1e_bits_case(1, 0x0030)),
        Case("b4", {"eax": 1}, d1e_bits_case(1, 0xFFCF)),
        Case("b5", {"eax": 0}, d1e_bits_case(0, 0x3000)),
        Case("b6", {"eax": 1}, d1e_bits_case(1, 0x0010, other=0x0020)),
    ], eax_mask=0xFFFFFFFF, mutants=("@side", "@scale", "@b4", "@b5", "@word", "@ret")),
    # 0x1D810 fight_select_marker_release: EAX = side. 0x2B150 (stub, no register clobbered) is
    # reached with the slot still holding the record (the zero store is after it, 0x1D82D). Void:
    # EAX at return is the side or 0x2B150's; every caller rewrites EAX before reading it
    # (0x4376C/0x444FC `mov eax,1`; 0x20EF8 at 0x20F05, 0x43818 at 0x4381B).
    Spec("fight_select_marker_release", 0x1D810, [
        Case("k0", {"eax": 0}, d1e_marker_case(D1E_MK0, D1E_MK1)),
        Case("k1", {"eax": 1}, d1e_marker_case(D1E_MK0, D1E_MK1)),
        Case("k2", {"eax": 0}, d1e_marker_case(0, D1E_MK1)),
        Case("k3", {"eax": 1}, d1e_marker_case(D1E_MK0, 0)),
    ], calls=(P7_2B150,), eax_mask=0,
       mutants=("@side", "@scale", "@nozero", "@width", "@order", "@nocall", "@arg", "@always")),
    # 0x1D7B8 fight_select_marker_spawn: EAX = side, EDX = class, EBX = y (never written: it is
    # 0x2AE14's a4). The release (0x2B150 stub) and its zero store precede the spawn (SPAWN stub,
    # its EAX stored at DS_001028E0[side]); `test esi,esi; jne` picks a2 (any non-zero side,
    # the whole register: case m5).
    # Void: 0x43A89 reloads EAX; at 0x44035/0x4432D only AX is rewritten before those functions
    # return (the upper word flows to the callers of 0x43EA0/0x4418C: open, their rows).
    Spec("fight_select_marker_spawn", 0x1D7B8, [
        Case("m0", {"eax": 0, "edx": 0, "ebx": 0x1800}, d1e_marker_case(0, D1E_MK1),
             {0x2AE14: 0x02B6C7D8}),
        Case("m1", {"eax": 0, "edx": 1, "ebx": 0x1800}, d1e_marker_case(D1E_MK0, D1E_MK1),
             {0x2AE14: 0x02B6C7D9, 0x2B150: 0x5A5A5A5A}),
        Case("m2", {"eax": 1, "edx": 6, "ebx": 0x0980}, d1e_marker_case(D1E_MK0, 0),
             {0x2AE14: 0x03B7C8D9}),
        Case("m3", {"eax": 1, "edx": 3, "ebx": 0xFFFF1234}, d1e_marker_case(D1E_MK0, D1E_MK1),
             {0x2AE14: 0}),
        Case("m4", {"eax": 2, "edx": 2, "ebx": 0x0001}, d1e_marker_case(D1E_MK0, D1E_MK1, 0),
             {0x2AE14: 0x01B8C9DA}),
        Case("m5", {"eax": 0x100, "edx": 4, "ebx": 0x0002},
             d1e_marker_case(D1E_MK0, D1E_MK1, far=D1E_MK2), {0x2AE14: 0x02B6C7D8}),
    ], calls=(P7_2B150, SPAWN), eax_mask=0,
       mutants=("@slot", "@nocall", "@nozero", "@scale", "@a2", "@a3", "@y", "@a5", "@store",
                "@width", "@eq1", "@side8", "@table")),
    # 0x1D838 fight_hud_badge_spawn: as 0x1D7B8 with the 0xA7760 descriptor table. Void: 0x2777E
    # rewrites EAX; 0x25F4E/0x298F8 rewrite AL only and 0x1D9DA/0x1DDC4/0x1DBFF never touch it
    # before their function returns or loops (EAX flows on: open, those callers' rows).
    Spec("fight_hud_badge_spawn", 0x1D838, [
        Case("h0", {"eax": 0, "edx": 0, "ebx": 0x0980}, d1e_marker_case(0, D1E_MK1),
             {0x2AE14: 0x02B6C7D8}),
        Case("h1", {"eax": 0, "edx": 1, "ebx": 0x0980}, d1e_marker_case(D1E_MK0, D1E_MK1),
             {0x2AE14: 0x02B6C7D9, 0x2B150: 0x5A5A5A5A}),
        Case("h2", {"eax": 1, "edx": 6, "ebx": 0x1800}, d1e_marker_case(D1E_MK0, 0),
             {0x2AE14: 0x03B7C8D9}),
        Case("h3", {"eax": 1, "edx": 3, "ebx": 0xFFFF1234}, d1e_marker_case(D1E_MK0, D1E_MK1),
             {0x2AE14: 0}),
        Case("h4", {"eax": 2, "edx": 2, "ebx": 0x0001}, d1e_marker_case(D1E_MK0, D1E_MK1, 0),
             {0x2AE14: 0x01B8C9DA}),
        Case("h5", {"eax": 0x100, "edx": 4, "ebx": 0x0002},
             d1e_marker_case(D1E_MK0, D1E_MK1, far=D1E_MK2), {0x2AE14: 0x02B6C7D8}),
    ], calls=(P7_2B150, SPAWN), eax_mask=0,
       mutants=("@slot", "@nocall", "@nozero", "@scale", "@a2", "@a3", "@y", "@a5", "@store",
                "@width", "@eq1", "@side8", "@table")),
    # 0x1DA08 fight_hud_pulse: no argument. Per side the word 0x102908[side] is decremented and,
    # when it is not above 0 (signed), the DS_001028F8[side] record begins 0xE9050 at 4.0
    # (ANIM_BEGIN stub; EDX is reloaded after it, 0x1DA3D) and the word becomes (0x78 - the
    # slot's +0x5A byte) >> 1, raised to 0xC. Void: 0x27FA8 (0x27FAC), 0x272DC (0x272E6) and
    # 0x29970 (0x29972) rewrite EAX; at 0x2686F only AX is rewritten (`mov ax,[0xef6dc]`).
    Spec("fight_hud_pulse", 0x1DA08, [
        Case("p0", {}, d1e_pulse_case(2, 1)),
        Case("p1", {}, d1e_pulse_case(1, 2, b0=0x60)),
        Case("p2", {}, d1e_pulse_case(1, 1, b0=0x62, b1=0x5E)),
        Case("p3", {}, d1e_pulse_case(0, 0x8001, b0=0xF8, b1=0x79)),
        Case("p4", {}, d1e_pulse_case(0x8000, 0x7FFF, b0=0x00, b1=0x11)),
        Case("p5", {}, d1e_pulse_case(3, 0x0100)),
        Case("p6", {}, d1e_pulse_case(1, 0, b0=0x78, b1=0x80)),
    ], calls=(ANIM_BEGIN,), eax_mask=0,
       mutants=("@gt", "@u", "@rec", "@stream", "@frame", "@sext", "@w16", "@clamp", "@order",
                "@byte", "@side", "@nostore", "@decb", "@relb", "@one", "@k78", "@norel", "@noclamp")),
    # 0x29CDC fight_dust_value: EAX = side, EDX = the character (`shl edx,2`, never masked).
    # `test bl,bl` on DS_00105B34[side]: zero picks 0xA8AF8, non-zero 0xA8B14 (the image's two
    # tables differ in every row). The dword is returned whole (stored at +0x10 by each caller).
    Spec("fight_dust_value", 0x29CDC, [
        Case("d0", {"eax": 0, "edx": 0}, d1e_dust_case(0, 0)),
        Case("d1", {"eax": 0, "edx": 3}, d1e_dust_case(0, 1)),
        Case("d2", {"eax": 1, "edx": 6}, d1e_dust_case(1, 0)),
        Case("d3", {"eax": 1, "edx": 1}, d1e_dust_case(1, 1)),
        Case("d4", {"eax": 0, "edx": 5}, d1e_dust_case(0, 0x80, other=0x80)),
        Case("d5", {"eax": 1, "edx": 2}, d1e_dust_case(1, 2, other=0)),
    ], eax_mask=0xFFFFFFFF, mutants=("@side", "@scale", "@one", "@inv", "@always0")),
    # 0x2BE00 fight_2be00: EAX = the record; the pset entry of its +0x56 word (`and
    # edx,0xffff`), the dword at +4. Unhit input, named: an index >= 0x8000 would tell zero-
    # from sign-extension, but index * 0x20 >= 0x100000 leaves the image from any base in it.
    Spec("fight_2be00", 0x2BE00, [
        Case("t0", {"eax": D1E_REC0}, d1e_term_case((D1E_REC0, 0, 0x12345678))),
        Case("t1", {"eax": D1E_REC0}, d1e_term_case((D1E_REC0, 1, 0xFFFF8001))),
        Case("t2", {"eax": D1E_REC1}, d1e_term_case((D1E_REC0, 1, 0x0BAD0BAD),
                                                    (D1E_REC1, 0x0102, 0x80000000))),
        Case("t3", {"eax": D1E_REC0}, d1e_term_case((D1E_REC0, 0x7F, 0x7FFFFFFF))),
    ], eax_mask=0xFFFFFFFF, mutants=("@byte", "@dword", "@field", "@off", "@scale", "@base")),
    # 0x2BE1C fight_2be1c: EAX = record a, EDX = record b; a's 0x2BE00 term minus b's
    # (`sub eax,[edx+4]`), whole.
    Spec("fight_2be1c", 0x2BE1C, [
        Case("e0", {"eax": D1E_REC0, "edx": D1E_REC1},
             d1e_term_case((D1E_REC0, 2, 0x00005000), (D1E_REC1, 5, 0x00001234))),
        Case("e1", {"eax": D1E_REC1, "edx": D1E_REC0},
             d1e_term_case((D1E_REC0, 2, 0x00005000), (D1E_REC1, 5, 0x00001234))),
        Case("e2", {"eax": D1E_REC0, "edx": D1E_REC1},
             d1e_term_case((D1E_REC0, 0x0102, 0x80000000), (D1E_REC1, 0x0201, 0x7FFFFFFF))),
        Case("e3", {"eax": D1E_REC0, "edx": D1E_REC0},
             d1e_term_case((D1E_REC0, 7, 0x00000777), (D1E_REC1, 9, 0x00000999))),
    ], eax_mask=0xFFFFFFFF, mutants=("@same", "@swap", "@add", "@abyte", "@bbyte", "@adword",
                                     "@bdword", "@aoff", "@boff")),
    # 0x2BE4C fight_2be4c: EAX = the record, EDX = the value; ([rec+0x44] sar 0x10) * 2 + value
    # - 0x2A00, whole.
    Spec("fight_2be4c", 0x2BE4C, [
        Case("v0", {"eax": D1E_REC0, "edx": 0x2A00}, d1e_x4c_case(0x00010000)),
        Case("v1", {"eax": D1E_REC0, "edx": 0x1234}, d1e_x4c_case(0xFFFF0000)),
        Case("v2", {"eax": D1E_REC0, "edx": 0xFFFFA880}, d1e_x4c_case(0x00018000)),
        Case("v3", {"eax": D1E_REC0, "edx": 0}, d1e_x4c_case(0x8000FFFF)),
        Case("v4", {"eax": D1E_REC0, "edx": 0x7FFFFFFF}, d1e_x4c_case(0x7FFF0001)),
    ], eax_mask=0xFFFFFFFF, mutants=("@shr", "@sh15", "@const", "@sub", "@field", "@nodbl")),
    # 0x2C320 fight_scene_crowd: EAX = the scene. `dec edi; cmp di,-1` runs the count word's
    # records (a count >= 0x8000 is not exercised: 32768 recorded spawns); each spawns
    # 0x2AE14 (SPAWN stub) with a3/a4 `sar 0x10` of the dwords at +4/+2 and a5 = (byte +0xB
    # << 16) + word +8. Void: its caller 0x412D8 goes on to `call [edi*4+0xc7f58]` (open).
    Spec("fight_scene_crowd", 0x2C320, [
        Case("c0", {"eax": 0}, d1e_crowd_case(0, [], n=0)),
        Case("c1", {"eax": 2}, d1e_crowd_case(2, [(0x12345678, 0xFFFF, 0x8000, 0xFFFF, 0, 0x80)]),
             {0x2AE14: 0x01B6C7D8}),
        Case("c2", {"eax": 4}, d1e_crowd_case(4, [(0x00000200, 0x0010, 0x00E0, 0x0001, 3, 0x00),
                                                  (0x00004200, 0x7FFF, 0xFF20, 0x8000, 0x80, 0x01),
                                                  (0xFFFFFFFF, 0x8001, 0x0001, 0x0100, 9, 0x7F)])),
        Case("c3", {"eax": 6}, d1e_crowd_case(6, [(0x00010000, 0x0000, 0x0000, 0x0000, 6, 0xFF),
                                                  (0x00020000, 0x0001, 0x0002, 0x0003, 1, 0x04)]),
             {0x2AE14: 0}),
    ], calls=(SPAWN,), eax_mask=0,
       mutants=("@nscale", "@tscale", "@store", "@stride", "@late", "@one", "@hisext", "@a5hi",
                "@a5w", "@idxsext", "@idx", "@a3zext", "@a4zext", "@swap", "@a2")),
    # 0x33C18 fight_char_reset: EAX = side; slot +0x7F/+0x80/+0x82/+0x5B bytes and the +0x3C
    # dword cleared, the word 0x108860[side] = 100. Void (EAX = the slot address at return):
    # 0x257EB/0x25AA1/0x27176/0x27722/0x28E17/0x29893/0x43E4E/0x44496 rewrite EAX; 0x292DC
    # rewrites AH only, 0x257F7 (0x46594) and 0x41359 branch before any access (open).
    Spec("fight_char_reset", 0x33C18, [
        Case("z0", {"eax": 0}, d1e_reset_case(0)),
        Case("z1", {"eax": 1}, d1e_reset_case(1)),
    ], eax_mask=0,
       mutants=("@side", "@stride", "@s7f", "@s80", "@s3c", "@s82", "@s5b", "@w", "@wscale",
                "@val", "@wbyte", "@s3cw", "@s81")),
    # 0x33F08 fight_health_bars: no argument. Per side with a slot record at 0x1077A8[side]:
    # the fighter's pset word less the character constant (0x33EEC's seven targets), in 16
    # bits (`test dx,dx` 0x33F98, `movsx ecx,dx` 0x33F9D); the bar's +8 from the slot's +0x24
    # table or 0x1E1; +0x29 bit 6 from the pset word's bit 15; then 0x2A408 (C1_SPRITE_ID stub)
    # into the bar's pset word. The stub models 0x2A408 as leaving DS_001014EC alone (the raw
    # reloads EDI from it at 0x34015). Void: 0x2545C rewrites AX only; 0x11C49/0x25545/0x2558B/
    # 0x255C5 return and 0x10DB0/0x425EF do not touch EAX at once (open).
    Spec("fight_health_bars", 0x33F08, [
        Case("a0", {}, d1e_hb_case(dict(ch=1, s=0, bar=0x8001), dict(ch=2, s=0x4B0, bit15=1, bar=0x7FFE)),
             {0x2A408: 0x1234ABCD}),
        Case("a1", {}, d1e_hb_case(dict(ch=3, s=-1, p29=0xFF), dict(ch=4, s=0x4B1, bit15=1, p29=0x40)),
             {0x2A408: 0xFFFF0001}),
        Case("a2", {}, d1e_hb_case(dict(ch=5, s=0x20, f41=0x20, bit15=1, p29=0xBF),
                                   dict(ch=6, s=5, f41=0xDF, p29=0xFF)),
             {0x2A408: 0x00008000}),
        Case("a3", {}, d1e_hb_case(None, dict(ch=0, s=7, bar=0x0102)), {0x2A408: 0x0000BEEF}),
        Case("a4", {}, d1e_hb_case(dict(ch=7, s=0x100, bit15=1, bar=0xFFFF), None),
             {0x2A408: 0x00000042}),
        Case("a5", {}, d1e_hb_case(dict(ch=0xFF, s=1, bar=0x0001), dict(ch=1, s=0x4AF, bar=0x4AF0)),
             {0x2A408: 0x00010002}),
        Case("a6", {}, d1e_hb_case(None, None)),
        Case("a7", {}, d1e_hb_case(dict(ch=6, s=0, const=0xFB60, raw=0x0010, bar=0x0B60, bar_s=0x4B0),
                                   dict(ch=2, s=0, const=0x8000, raw=0x7FFF, bit15=0, bar=0x0C70)),
             {0x2A408: 0x00000007}),
    ], calls=(C1_SPRITE_ID,), eax_mask=0,
       mutants=("@one", "@idxf", "@ch", "@mask", "@flag", "@lo", "@hi", "@sext", "@e1", "@pset8",
                "@inv", "@bit", "@nobit", "@spset", "@word", "@nocall", "@args", "@bar", "@no8", "@nospr",
                "@wide")),
    # 0x36E2C fight_position_gate: EAX = the slot. AL only (`mov al,1` 0x36E6D, `xor al,al`
    # 0x36E71; the upper bytes are the slot's), and its one caller tests AL (0x34B93).
    Spec("fight_position_gate", 0x36E2C, [
        Case("g0", {"eax": D1E_REC0}, d1e_gate_case(3, 1, 0, 0x10, 0, 0)),
        Case("g1", {"eax": D1E_REC0}, d1e_gate_case(2, 0, 0, 0x10, 5, 1)),
        Case("g2", {"eax": D1E_REC0}, d1e_gate_case(2, 1, 0, 0x10, 0, 0)),
        Case("g3", {"eax": D1E_REC0}, d1e_gate_case(2, 1, 1, 0x10, 7, 0)),
        Case("g4", {"eax": D1E_REC0}, d1e_gate_case(3, 0, 1, 0xEF, 0, 0)),
        Case("g5", {"eax": D1E_REC0}, d1e_gate_case(3, 0, 1, 0xFF, 6, 1)),
        Case("g6", {"eax": D1E_REC0}, d1e_gate_case(3, 0, 1, 0x10, 1, 0)),
        Case("g7", {"eax": D1E_REC0}, d1e_gate_case(3, 0, 1, 0x10, 2, 0)),
        Case("g8", {"eax": D1E_REC0}, d1e_gate_case(3, 0, 1, 0x10, 5, 2)),
        Case("g9", {"eax": D1E_REC0}, d1e_gate_case(3, 0, 1, 0x10, 0, 0x80)),
        Case("g10", {"eax": D1E_REC0}, d1e_gate_case(3, 0, 1, 0x10, 8, 0)),
        Case("g11", {"eax": D1E_REC0}, d1e_gate_case(3, 0, 1, 0x10, 3, 0)),
        Case("g12", {"eax": D1E_REC0}, d1e_gate_case(3, 0, 1, 0x10, 4, 0)),
        Case("g13", {"eax": D1E_REC0}, d1e_gate_case(0, 0x80, 0x80, 0x30, 7, 1)),
        Case("g14", {"eax": D1E_REC0}, d1e_gate_case(3, 0, 1, 0x20, 0, 0)),
    ], eax_mask=0xFF,
       mutants=("@b1d", "@b14", "@s63", "@bit", "@field", "@f54", "@st0", "@st5", "@st7", "@st6",
                "@st1", "@st2", "@lt", "@signed")),
    # 0x38BEC fight_side_scratch_clear: EAX = side; five words [side] and the 0x40 bytes at
    # 0x107A80 + side * 0x40 cleared (`inc eax` before each `[eax+0x107a7f]` store). Void (EAX
    # = side * 0x40 + 0x40 at return): its one caller rewrites AH only (0x20F22; open).
    Spec("fight_side_scratch_clear", 0x38BEC, [
        Case("y0", {"eax": 0}, d1e_scratch_case()),
        Case("y1", {"eax": 1}, d1e_scratch_case()),
    ], eax_mask=0,
       mutants=("@d2c", "@d20", "@d18", "@d1c", "@d24", "@side", "@wscale", "@n3f", "@n41",
                "@base", "@bscale", "@wbyte")),
    # 0x3C5CC fight_slot_clear: no argument; DS_00107EE0, DS_00107D50 and DS_00107D54 = 0.
    # Void: each of its 12 callers rewrites EAX before reading it (`xor eax,eax` or `lea eax`).
    Spec("fight_slot_clear", 0x3C5CC, [
        Case("f0", {}, {0x00107EDC: le32(0xDCDCDCDC) + le32(0xE0E1E2E3) + le32(0xE4E4E4E4),
                        0x00107D4C: le32(0x4C4C4C4C) + le32(0x50515253) + le32(0x54555657)
                        + le32(0x58585858)}),
    ], eax_mask=0, mutants=("@ee0", "@d50", "@d54", "@word")),
    # 0x413C8 fight_413c8: no argument. Eight 0x2AE14 spawns (D1E_SPAWN_W stub: one EAX for
    # all eight, so the handles are equal and the store order shows in the memory at each
    # call). a5 of the seven is the first handle's +0x56 word | 0x400, re-read each pass: with
    # the handle D1E_E0 the i = 3 spawn's stub write changes that word. Void: its one caller
    # rewrites AH only (0x4145D; open).
    Spec("fight_413c8", 0x413C8, [
        Case("q0", {}, d1e_413_case(D1E_E0, 0x1234), {0x2AE14: D1E_E0}),
        Case("q1", {}, d1e_413_case(0x0010A6C1, 0xFBFF), {0x2AE14: 0x0010A6C1}),
        Case("q2", {}, d1e_413_case(D1E_E0, 0x0400), {0x2AE14: D1E_E0}),
    ], calls=(D1E_SPAWN_W,), eax_mask=0,
       mutants=("@xy", "@a3", "@late", "@count", "@once", "@field", "@or", "@a3b", "@slot",
                "@nostore", "@nof4")),
    # 0x4246C fight_stage_marks_clear: no argument. 0x65490 (the runtime byte fill, allowed
    # with its dword helper 0x654C7: their own bytes run in the original, the port fills the
    # seven bytes itself) zeroes the stage bytes; `xor ah,ah` then clears 0x108111. Void:
    # 0x25B4C rewrites EAX, 0x28B46 rewrites AL only (open).
    Spec("fight_stage_marks_clear", 0x4246C, [
        Case("m0", {}, d1e_marks_case([0x81, 0x02, 0x43, 0x04, 0xC5, 0x06, 0x47], 0x07)),
        Case("m1", {}, d1e_marks_case([0xFF] * 7, 0xFF, before=0xFF, after=0xFF)),
    ], allow_calls=(0x65490, 0x654C7), eax_mask=0,
       mutants=("@n6", "@n8", "@base", "@nocount", "@cnt")),
    # 0x4248C fight_stage_mark_drop: EAX = the class (AL), EDX = the side (DL, `shl dl,6`).
    # Each stage byte whose low 7 bits equal the key becomes 0 and the count byte 0x108111 drops
    # by one. Void: 0x43E5D/0x444A5 rewrite AL only (open).
    Spec("fight_stage_mark_drop", 0x4248C, [
        Case("s0", {"eax": 5, "edx": 1},
             d1e_marks_case([0x45, 0xC5, 0x05, 0x44, 0x45, 0x46, 0x85], 7, before=0x45, after=0x45)),
        Case("s1", {"eax": 3, "edx": 0},
             d1e_marks_case([0x03, 0x43, 0x83, 0x00, 0x03, 0x7F, 0x03], 0, before=0x03, after=0x03)),
        Case("s2", {"eax": 0, "edx": 0},
             d1e_marks_case([0x00, 0x80, 0x01, 0x40, 0x00, 0xFF, 0x80], 0x10, before=0x80, after=0x00)),
        Case("s3", {"eax": 0x105, "edx": 0x101},
             d1e_marks_case([0x05, 0x45, 0x01, 0x45, 0xC5, 0x41, 0x04], 0x80, before=0x45, after=0x45)),
        Case("s4", {"eax": 5, "edx": 2},
             d1e_marks_case([0x85, 0x05, 0x45, 0x85, 0x05, 0x45, 0x85], 3, before=0x85, after=0x85)),
        Case("s5", {"eax": 5, "edx": 4},
             d1e_marks_case([0x85, 0x05, 0x45, 0x04, 0x06, 0xC5, 0x05], 3, before=0x05, after=0x05)),
        Case("s6", {"eax": 0x85, "edx": 0},
             d1e_marks_case([0x85, 0x05, 0x45, 0x85, 0x05, 0x45, 0x85], 9, before=0x85, after=0x05)),
        Case("s7", {"eax": 6, "edx": 1},
             d1e_marks_case([0x01, 0x02, 0x03, 0x04, 0x05, 0x07, 0x46], 1, before=0x46, after=0x46)),
    ], eax_mask=0,
       mutants=("@mask", "@shift", "@n6", "@n8", "@nodec", "@inc", "@keep7", "@nowrite", "@key7",
                "@from1", "@cnt", "@argw")),
    # 0x43964 fight_char_entry_spawn: EAX = side; the character is the signed byte
    # 0x108166[side] (`sar esi,0x18`). Two spawns (D1E_SPAWN_A stub), then 0x2BCF4 and
    # 0x2A17C (stubs) on the second handle, re-read from DS_0010815C[side]. Void: each caller
    # rewrites EAX (`mov eax,edx` at 0x4379B/0x43B5E/0x4451C).
    Spec("fight_char_entry_spawn", 0x43964, [
        Case("n0", {"eax": 0}, d1e_entry_case(0, 1, 4), {0x2AE14: 0x02B6C7D8}),
        Case("n1", {"eax": 1}, d1e_entry_case(1, 4, 1), {0x2AE14: 0x03B7C8D9}),
        Case("n2", {"eax": 0}, d1e_entry_case(0, 0xFF, 2), {0x2AE14: 0x01B8C9DA}),
        Case("n3", {"eax": 1}, d1e_entry_case(1, 6, 0, a2=0x9000, a4=0x8540, pw=0x8014),
             {0x2AE14: 0x02B6C7D8}),
        Case("n4", {"eax": 0}, d1e_entry_case(0, 0, 0), {0x2AE14: 0}),
    ], calls=(D1E_SPAWN_A, P6_2BCF4, PALETTE), eax_mask=0,
       mutants=("@zext", "@chside", "@swap", "@wsext", "@side", "@a3", "@slota", "@desc", "@actor",
                "@order", "@id", "@psext", "@pal", "@noa", "@nob")),
]
D1_SPECS += D1E_SPECS


# ---- track D1 batch d1f (record 2026-10-11-reverse-d1f): game/fight.c, second half ---------------
#
# Scratch records in the image's zero BSS (0x10A000..): every poke stays inside the image.

D1F_ENT, D1F_ACT = 0x0010A000, 0x0010A100    # an effects entry / list node and its actor record
D1F_REC0, D1F_REC1 = 0x0010A200, 0x0010A240  # the two fighter records the slots point at
D1F_SLOTS = 0x001077B0
# 0x4AF04's seven stream ranges, read from the bytes: the `cmp r,imm` pair of each case block of
# the jump table at 0x4AEE8 (ch -> block 0x4AF39, 0x4AF7F, 0x4AFC1, 0x4AFE0, 0x4AFFF, 0x4AF5C, 0x4AFA2)
D1F_HOLD = ((0x000E7B02, 0x000E7B50), (0x000E4744, 0x000E47FC), (0x000ED7AE, 0x000ED80C),
            (0x000D2DEC, 0x000D2E06), (0x000EB38A, 0x000EB3E4), (0x000D4A3C, 0x000D4A8A),
            (0x000E137A, 0x000E1432))


def d1f_btn_case(w0, w1, w2):
    """0x43D0C: the two sides' command words at 0x1088E0 and the word after them (the @width
    mutant's side-1 read)."""
    return {0x001088E0: le16(w0) + le16(w1) + le16(w2)}


def d1f_clr_case(b, d):
    """0x46670: the byte the loop clears at 0x1081DE + o and the dword pair at 0x1081B0 + o for
    o = 0x40 and 0x80, each inside seeded neighbours, and the same fields at o = 0 and 0xC0 (the
    @first / @bound mutants' extra iterations)."""
    p = {}
    for o in (0, 0x40, 0x80, 0xC0):
        p[0x001081DD + o] = bytes([b ^ 0xFF, b, b ^ 0x0F, b ^ 0xF0, b ^ 0x33])
        p[0x001081AC + o] = le32(d ^ 0x11111111) + le32(d) + le32(d + o + 1) + le32(d ^ 0x22222222)
    return p


def d1f_arr_case(ax, ex, s34, s32=0, x1c=None):
    """0x4A7D4 / 0x4A808: the entry's actor pointer (+8) and x (+0x14) between seeded neighbours,
    the actor's x (+0x18; +0x1C holds another value), and the +0x32 dword whose high word is the
    step (+0x34); +0x36 is seeded."""
    return {D1F_ENT + 0x08: le32(D1F_ACT) + le32(0x0BADF00D) + le32(0x0BADF11D) + le32(ex)
            + le32(0x0BADF22D),
            D1F_ACT + 0x14: le32(0x0BADF33D) + le32(ax) + le32((ax + 0x100) if x1c is None else x1c),
            D1F_ACT + 0x30: le16(0x5A5A) + le16(s32) + le16(s34) + le16(0x7B7B)}


def d1f_hold_case(side, ch, cur, other=(7, 0)):
    """0x4AF04: the side slot's +0x7A character and its record's +8 stream cursor (between seeded
    +4 and +0xC); the other slot holds `other` = (ch, cur), chosen per case to answer differently."""
    p = {}
    for s, (c, v) in ((side, (ch, cur)), (1 - side, other)):
        rec = (D1F_REC0, D1F_REC1)[s]
        p[D1F_SLOTS + s * 0x94] = le32(rec)
        p[D1F_SLOTS + s * 0x94 + 0x79] = bytes([0x66, c, 0x77])
        p[rec + 4] = le32(0x0BAD0004) + le32(v) + le32(0x0BAD000C)
    return p


def d1f_hold_cases():
    """Per character: its range's two bounds and the value just outside each."""
    out = []
    for c, (lo, hi) in enumerate(D1F_HOLD):
        out += [Case("h%da" % c, {"eax": 0}, d1f_hold_case(0, c, lo - 1)),
                Case("h%db" % c, {"eax": 0}, d1f_hold_case(0, c, lo, other=(c, 0))),
                Case("h%dc" % c, {"eax": 0}, d1f_hold_case(0, c, hi, other=(c, 0))),
                Case("h%dd" % c, {"eax": 0}, d1f_hold_case(0, c, hi + 1))]
    out += [Case("h7", {"eax": 0}, d1f_hold_case(0, 7, 0, other=(0, 0))),
            Case("hff", {"eax": 0}, d1f_hold_case(0, 0xFF, 0, other=(0, 0))),
            Case("s1a", {"eax": 1}, d1f_hold_case(1, 3, D1F_HOLD[3][0], other=(3, 0))),
            Case("s1b", {"eax": 1}, d1f_hold_case(1, 3, D1F_HOLD[3][1] + 1)),
            Case("e100", {"eax": 0x100}, d1f_hold_case(0, 2, D1F_HOLD[2][0] + 4, other=(2, 0))),
            Case("e101", {"eax": 0x101}, d1f_hold_case(1, 4, D1F_HOLD[4][1] + 4))]
    return out


def d1f_5b_case(b):
    """0x4DBB4: the slot's +0x5B byte between seeded +0x5A and +0x5C."""
    return {D1F_ENT + 0x5A: bytes([0x11, b, 0x22])}


D1F_SPECS = [
    # 0x43D0C fight_char_button: EAX = side. The caller reads all of EAX (0x43DA2 `mov edx,eax`).
    Spec("fight_char_button", 0x43D0C, [
        Case("c0", {"eax": 0}, d1f_btn_case(0x02FF, 0x0400, 0x0800)),
        Case("c1", {"eax": 0}, d1f_btn_case(0x04FF, 0x0200, 0x0800)),
        Case("c2", {"eax": 0}, d1f_btn_case(0x08FF, 0x0200, 0x0400)),
        Case("c3", {"eax": 0}, d1f_btn_case(0xF1FF, 0x0200, 0x0400)),
        Case("c4", {"eax": 0}, d1f_btn_case(0x0E00, 0x0000, 0x0000)),
        Case("c5", {"eax": 0}, d1f_btn_case(0x0C00, 0x0200, 0x0200)),
        Case("c6", {"eax": 1}, d1f_btn_case(0x0200, 0x0800, 0x0400)),
        Case("c7", {"eax": 1}, d1f_btn_case(0x0E00, 0x0000, 0x0200)),
    ], eax_mask=0xFFFFFFFF, mutants=("@side", "@width", "@lo", "@order", "@b200", "@b800")),
    # 0x46670 fight_1081f0_clear: no argument. Its EAX (0x80) dies unread: 0x20F91 `xor ah,ah`,
    # then 0x20EF8's callers (record §1).
    Spec("fight_1081f0_clear", 0x46670, [
        Case("z0", {}, d1f_clr_case(0x5A, 0x12345678)),
        Case("z1", {}, d1f_clr_case(0xFF, 0xFFFFFF00)),
    ], eax_mask=0, mutants=("@first", "@bound", "@byte", "@d0", "@d4", "@width")),
    # 0x496AC fight_dust_clamp: EAX = y, signed (`jl` 0x496B2, `jg` 0x496C0). Every caller stores
    # AX; the value fits 16 bits on every arm, so the full EAX is compared.
    Spec("fight_dust_clamp", 0x496AC, [
        Case("k0", {"eax": 0xB00}), Case("k1", {"eax": 0xAFF}), Case("k2", {"eax": 0xAFE}),
        Case("k3", {"eax": 0xB01}), Case("k4", {"eax": 0x400}), Case("k5", {"eax": 0x401}),
        Case("k6", {"eax": 0x7FF}), Case("k7", {"eax": 0xFFFFFFFF}), Case("k8", {"eax": 0x80000000}),
        Case("k9", {"eax": 0x7FFFFFFF}), Case("k10", {"eax": 0}),
    ], eax_mask=0xFFFFFFFF, mutants=("@unsigned", "@lo", "@hi", "@half")),
    # 0x4A7D4 fight_4a7d4: EAX = entry. The caller tests EAX (0x4A19B `test eax,eax`).
    Spec("fight_4a7d4", 0x4A7D4, [
        Case("a0", {"eax": D1F_ENT}, d1f_arr_case(0x1000, 0x0FF0, 8)),
        Case("a1", {"eax": D1F_ENT}, d1f_arr_case(0x1000, 0x0FEF, 8)),
        Case("a2", {"eax": D1F_ENT}, d1f_arr_case(0x0FF0, 0x1000, 8)),
        Case("a3", {"eax": D1F_ENT}, d1f_arr_case(0x0FEF, 0x1000, 8)),
        Case("a4", {"eax": D1F_ENT}, d1f_arr_case(0x1000, 0x0FF0, 0xFFF8)),
        Case("a5", {"eax": D1F_ENT}, d1f_arr_case(0x80000000, 0, 0)),
        Case("a6", {"eax": D1F_ENT}, d1f_arr_case(0x00011000, 0x1000, 0x8000)),
        Case("a7", {"eax": D1F_ENT}, d1f_arr_case(0x00011001, 0x1000, 0x8000)),
        Case("a8", {"eax": D1F_ENT}, d1f_arr_case(0x1000, 0x0FEF, 8, s32=0x7FFF)),
    ], eax_mask=0xFFFFFFFF, mutants=("@absd", "@absv", "@dbl", "@lt", "@word", "@x")),
    # 0x4A808 fight_4a808: EAX = node. The caller tests EAX (0x4E750 `test eax,eax`).
    Spec("fight_4a808", 0x4A808, [
        Case("n0", {"eax": D1F_ENT}, d1f_arr_case(0x1000, 0x0FF0, 8)),
        Case("n1", {"eax": D1F_ENT}, d1f_arr_case(0x1000, 0x0FEF, 8)),
        Case("n2", {"eax": D1F_ENT}, d1f_arr_case(0x0FF0, 0x1000, 8)),
        Case("n3", {"eax": D1F_ENT}, d1f_arr_case(0x0FEF, 0x1000, 8)),
        Case("n4", {"eax": D1F_ENT}, d1f_arr_case(0x0FEF, 0x1000, 0xFFF8)),
        Case("n5", {"eax": D1F_ENT}, d1f_arr_case(0x1000, 0x0FEF, 0xFFF8)),
        Case("n6", {"eax": D1F_ENT}, d1f_arr_case(0x1000, 0x0FFF, 0)),
        Case("n7", {"eax": D1F_ENT}, d1f_arr_case(0x0FFF, 0x1000, 0)),
        Case("n8", {"eax": D1F_ENT}, d1f_arr_case(0xFFFFF000, 0x1000, 8)),
        Case("n9", {"eax": D1F_ENT}, d1f_arr_case(0x0FF4, 0x1000, 8)),
    ], eax_mask=0xFFFFFFFF, mutants=("@absd", "@dbl", "@lt", "@dir", "@sign0", "@unsigned")),
    # 0x4AF04 fight_4af04: EAX = side, read as AL (0x4AF09 `mov dl,al`). The caller tests AL
    # (0x4A0A7 `test al,al`): mask 0xFF. The seven table targets are each hit on both arms.
    Spec("fight_4af04", 0x4AF04, d1f_hold_cases(),
         eax_mask=0xFF, mutants=("@side", "@al", "@ch6", "@order", "@lo", "@hi", "@signed", "@cur")),
    # 0x4DBB4 fight_slot_5b_add: EAX = slot, EDX = amount. Its EAX (the sum) dies unread: 0x12BB8's
    # callers load 1 (0x12AA8..), and the render walk reloads EAX after 0x4F5C8 returns (0x25621).
    Spec("fight_slot_5b_add", 0x4DBB4, [
        Case("m0", {"eax": D1F_ENT, "edx": 1}, d1f_5b_case(0x00)),
        Case("m1", {"eax": D1F_ENT, "edx": 50}, d1f_5b_case(0x10)),
        Case("m2", {"eax": D1F_ENT, "edx": 100}, d1f_5b_case(0x00)),
        Case("m3", {"eax": D1F_ENT, "edx": 100}, d1f_5b_case(0x01)),
        Case("m4", {"eax": D1F_ENT, "edx": 0xFFFFFFFB}, d1f_5b_case(0x03)),
        Case("m5", {"eax": D1F_ENT, "edx": 0}, d1f_5b_case(0xFF)),
        Case("m6", {"eax": D1F_ENT, "edx": 0xFFFFFFFF}, d1f_5b_case(0x10)),
        Case("m7", {"eax": D1F_ENT, "edx": 0x02222223}, d1f_5b_case(0x05)),
    ], eax_mask=0, mutants=("@sext", "@scale", "@floor", "@field", "@unsigned", "@capv")),
]
# The effects entries (0x49C78's helpers). 0x2BC30 is the ANIM_BEGIN stub throughout: the call list
# carries the actor, the stream and the pushed hold, the call memory the order of the stores.
D1F_ENT2, D1F_ACT2 = 0x0010A080, 0x0010A180
D1F_RNG = E.Call(0x5D7DC, ("eax",), mode="stub")
D1F_LIST = 0x0010884C


def d1f_ent_case(e1c=0xF3, e1e=0x55, f29=0xFF, w=(0x1111, 0x2222, 0x3333), b48=0x22, b55=0x77,
                 y1c=0x1000, y32=0x4444, ent=D1F_ENT, act=D1F_ACT, nxt=None, x14=0x0BAD0014):
    """One effects entry and its actor, every field the rows read or write inside seeded
    neighbours: the entry's +0 (next, when `nxt`), +8 (the actor), +0x14, +0x18..+0x1F (the +0x1A
    word, the +0x1C flags and the +0x1E type between +0x1D and +0x1F); the actor's +0x18/+0x1C,
    +0x28..+0x2B, +0x30..+0x3B (the +0x32 y word and the +0x34/+0x36/+0x38 words), +0x47..+0x49
    and +0x54..+0x56."""
    p = {ent + 0x08: le32(act) + le32(0x0BAD000C) + le32(0x0BAD0010) + le32(x14)
         + le16(0xA1A1) + le16(0x9999) + bytes([e1c, 0x1D, e1e, 0x1F]),
         act + 0x18: le32(0x00005555) + le32(y1c),
         act + 0x28: bytes([0x28, f29, 0x2A, 0x2B]),
         act + 0x30: le16(0x3030) + le16(y32) + le16(w[0]) + le16(w[1]) + le16(w[2]) + le16(0x3A3A),
         act + 0x47: bytes([0x47, b48, 0x49]),
         act + 0x54: bytes([0x54, b55, 0x56])}
    if nxt is not None:
        p[ent] = le32(nxt) + le32(0x0BAD0004)
    return p


def d1f_zero_case(table, index, **kw):
    """The same entry with the row's stream table entry `index` poked to 0 (the not-set arm)."""
    p = d1f_ent_case(**kw)
    p[table + index * 4] = le32(0)
    return p


def d1f_3f0_cases(table):
    """0x4B3F0 / 0x4B430: flag 0, 1, 0x100 (EBX is tested whole, 0x4B3FA / 0x4B43A `test ebx,ebx`)
    and 0x80000000, the +0x55 byte seeded 0x77, 0 and 1, and one zero stream."""
    e = {"eax": D1F_ENT}
    return [Case("p0", dict(e, edx=0, ebx=0), d1f_ent_case()),
            Case("p1", dict(e, edx=5, ebx=1), d1f_ent_case()),
            Case("p2", dict(e, edx=11, ebx=0x100), d1f_ent_case()),
            Case("p3", dict(e, edx=3, ebx=0x80000000), d1f_ent_case(b55=0)),
            Case("p4", dict(e, edx=2, ebx=1), d1f_zero_case(table, 2)),
            Case("p5", dict(e, edx=7, ebx=0), d1f_ent_case(b55=1, e1e=0x08))]


def d1f_face_case(nodes, cx):
    """0x4E27C: the 0x10884C list (head and its +8, which only the @empty mutant reads), the
    signed 0xF0AF0, one entry and actor per (b48, f29) of `nodes`, and a sentinel at the
    0xC95D4 + 0xFFE0 * 4 entry a +0x48 byte of 0 selects (the index is the low word of
    byte - 0x20, 0x4E2DA `mov dx,cx`)."""
    ents = ((D1F_ENT, D1F_ACT), (D1F_ENT2, D1F_ACT2))
    p = {0x000F0AF0: le32(cx), 0x000C95D4 + 0xFFE0 * 4: le32(0x000EE777),
         D1F_LIST: le32(ents[0][0] if nodes else D1F_LIST) + le32(0x0BAD0004) + le32(D1F_ACT2)}
    for i, (b48, f29) in enumerate(nodes):
        ent, act = ents[i]
        p.update(d1f_ent_case(ent=ent, act=act, b48=b48, f29=f29,
                              nxt=ents[i + 1][0] if i + 1 < len(nodes) else D1F_LIST))
    if not nodes:
        p.update(d1f_ent_case(ent=D1F_ENT2, act=D1F_ACT2))
    return p


def d1f_cb_case(a, b, **kw):
    """0x4CB18: the entry and actor, and the two fighters' +0x34 dwords (0x1077E4, 0x107878)."""
    p = d1f_ent_case(**kw)
    p[0x001077E4] = le32(a)
    p[0x00107878] = le32(b)
    return p


D1F_FACE_ANIM = dataclasses.replace(ANIM_BEGIN, writes=((None, D1F_ENT, le32(D1F_LIST)),))

D1F_SPECS += [
    # 0x4AC38 fight_4ac38: EAX = entry, EDX = index. EAX dies unread at each of its nine call sites
    # (record §1): mask 0.
    Spec("fight_4ac38", 0x4AC38, [
        Case("s0", {"eax": D1F_ENT, "edx": 0}, d1f_ent_case(f29=0xFF)),
        Case("s1", {"eax": D1F_ENT, "edx": 2}, d1f_ent_case(f29=0x00)),
        Case("s2", {"eax": D1F_ENT, "edx": 5}, d1f_ent_case(f29=0x40, e1e=0x08)),
    ], calls=(ANIM_BEGIN,), eax_mask=0,
       mutants=("@and", "@or", "@w34", "@w36", "@w38", "@type", "@tbl", "@hold", "@idx", "@order",
                "@width")),
    # 0x4B3F0 fight_4b3f0: EAX = entry, EDX = index, EBX = flag. Callers test EAX (0x4AB1F, 0x4AB74,
    # 0x4D27F `test eax,eax`).
    Spec("fight_4b3f0", 0x4B3F0, d1f_3f0_cases(0x000C955C), calls=(ANIM_BEGIN,),
         eax_mask=0xFFFFFFFF,
         mutants=("@zero", "@flag", "@fl8", "@f55", "@tbl", "@type", "@hold", "@order", "@ret")),
    # 0x4B430 fight_4b430: the same over 0xC958C. Callers test EAX (0x4A154, 0x4AB03, 0x4AB9E).
    Spec("fight_4b430", 0x4B430, d1f_3f0_cases(0x000C958C), calls=(ANIM_BEGIN,),
         eax_mask=0xFFFFFFFF,
         mutants=("@zero", "@flag", "@fl8", "@f55", "@tbl", "@type", "@hold", "@order", "@ret")),
    # 0x4BD4C fight_4bd4c: EAX = entry, EDX = index. Callers test EAX (0x49D44, 0x4AB39, 0x4D23B,
    # 0x4D3CA). The type store follows the call (0x4BD8E).
    Spec("fight_4bd4c", 0x4BD4C, [
        Case("q0", {"eax": D1F_ENT, "edx": 0}, d1f_ent_case()),
        Case("q1", {"eax": D1F_ENT, "edx": 6}, d1f_ent_case(w=(0xFFFF, 0x8000, 0x0001))),
        Case("q2", {"eax": D1F_ENT, "edx": 4}, d1f_zero_case(0x000C9754, 4)),
        Case("q3", {"eax": D1F_ENT, "edx": 3}, d1f_ent_case(e1e=0x00)),
    ], calls=(ANIM_BEGIN,), eax_mask=0xFFFFFFFF,
       mutants=("@zero", "@w38", "@w34", "@w36", "@tbl", "@hold", "@order", "@type", "@ret")),
    # 0x4D108 fight_4d108: EAX = entry, EDX = index. The rng draw is a stub whose whole EAX the raw
    # tests (0x4D115 `test eax,eax`); the caller tests AL (0x4D24C `test al,al`): mask 0xFF.
    Spec("fight_4d108", 0x4D108, [
        Case("r0", {"eax": D1F_ENT, "edx": 0}, d1f_ent_case(), {0x5D7DC: 0}),
        Case("r1", {"eax": D1F_ENT, "edx": 5}, d1f_ent_case(y32=0xFFF0, e1e=0x08), {0x5D7DC: 0}),
        Case("r2", {"eax": D1F_ENT, "edx": 1}, d1f_ent_case(), {0x5D7DC: 1}),
        Case("r3", {"eax": D1F_ENT, "edx": 1}, d1f_ent_case(), {0x5D7DC: 0x100}),
        Case("r4", {"eax": D1F_ENT, "edx": 1}, d1f_ent_case(), {0x5D7DC: 0x3B}),
    ], calls=(D1F_RNG, ANIM_BEGIN), eax_mask=0xFF,
       mutants=("@rng", "@al", "@tbl", "@hold", "@y", "@w1a", "@w38", "@type", "@order", "@ret")),
    # 0x4E27C fight_mode25_face: no argument. EAX dies unread (0x4E350's paths reload it, record
    # §1): mask 0. The 0x2BC30 stub rewrites the first entry's next pointer to the list head, a
    # harness write: the raw read the next entry before the call (0x4E28E), so it still reaches
    # the second entry.
    Spec("fight_mode25_face", 0x4E27C, [
        Case("f0", {}, d1f_face_case((), 0x00020000)),
        Case("f1", {}, d1f_face_case(((0x22, 0x01),), 0x00020000)),
        Case("f2", {}, d1f_face_case(((0x20, 0xFF),), 0)),
        Case("f3", {}, d1f_face_case(((0x00, 0xFF),), 0x80000000)),
        Case("f4", {}, d1f_face_case(((0x25, 0x00), (0x21, 0xBF)), 1)),
        Case("f5", {}, d1f_face_case(((0x23, 0x40), (0x24, 0x7F)), 0xFFFFFFFF)),
    ], calls=(D1F_FACE_ANIM,), eax_mask=0,
       mutants=("@gt", "@arm", "@w34", "@flip", "@idx", "@b48", "@tbl", "@hold", "@next", "@empty")),
    # 0x4CB18 fight_4cb18: EAX = entry, EDX = index, EBX = flag, ECX = side. 0x2C3FC, 0x2BC30 and
    # 0x1A570 are stubs; the raw tests 0x1A570's AL (0x4CB92 `test al,al`), and 0x8000 is what
    # 0x1A570 itself leaves in EAX when the bit is set (0x1A59C `and ah,0x80`, 0x1A5A4 `sete al`).
    # EAX dies unread in 0x4B470's and 0x4C60C's callers (record §1): mask 0.
    Spec("fight_4cb18", 0x4CB18, [
        Case("t0", {"eax": D1F_ENT, "edx": 0, "ebx": 0, "ecx": 0},
             d1f_cb_case(0x5000, 0x3000, b48=0x22), {0x1A570: 0}),
        Case("t1", {"eax": D1F_ENT, "edx": 5, "ebx": 1, "ecx": 1},
             d1f_cb_case(0x3000, 0x5000, b48=0x23), {0x1A570: 1}),
        Case("t2", {"eax": D1F_ENT, "edx": 11, "ebx": 0, "ecx": 0},
             d1f_cb_case(0x7FFF, 0x4000, b48=0x10, y1c=0x5000), {0x1A570: 0x8000}),
        Case("t3", {"eax": D1F_ENT, "edx": 2, "ebx": 0x100, "ecx": 1},
             d1f_cb_case(0x4000, 0x7F00, b48=0xFF, e1c=0x80), {0x1A570: 1}),
        Case("t4", {"eax": D1F_ENT, "edx": 7, "ebx": 0, "ecx": 1},
             d1f_cb_case(0x4100, 0x4000, b48=0x22), {0x1A570: 1}),
        Case("t5", {"eax": D1F_ENT, "edx": 9, "ebx": 1, "ecx": 0},
             d1f_cb_case(0x4000, 0x2000, b48=0x24), {0x1A570: 0}),
        Case("t6", {"eax": D1F_ENT, "edx": 1, "ebx": 0, "ecx": 0},
             d1f_cb_case(0x80000000, 0, b48=0x20, y1c=0xFFFFF000), {0x1A570: 0}),
    ], calls=(VOICE, ANIM_BEGIN, BIT15), eax_mask=0,
       mutants=("@sext", "@thr", "@voice", "@tbl", "@hold", "@absd", "@clamp", "@h", "@side", "@al",
                "@neg", "@fl8", "@div", "@w36", "@div16", "@type", "@order", "@bit7")),
]

# The character-select portrait and team-tag rows (0x43B24's and 0x4434C's helpers).
D1F_PANEL0, D1F_PANEL1 = 0x0010A300, 0x0010A340    # DS_0010815C[side]: passed on, never read here
D1F_PSET, D1F_POOL, D1F_NODE = 0x0010A380, 0x0010A400, 0x0010A700
D1F_TAGS = 0x00108114                                 # DS_00108114[side * 4 + slot]


def d1f_port_case(cur, gate=(0, 0), e28=(0x00, 0xFB), entries=(D1F_ENT, D1F_ENT2)):
    """0x43EA0 / 0x4418C: the two entry pointers DS_00108154 and panel pointers DS_0010815C, the
    two cursors DS_00108166, the two bytes DS_00108170, and per entry the +8 dword, the +0x18 and
    +0x1C dwords (seeded with every byte set, so a 16-bit store shows) and the +0x28 byte between
    +0x27 and +0x29."""
    p = {0x00108154: le32(entries[0]) + le32(entries[1]) + le32(D1F_PANEL0) + le32(D1F_PANEL1),
         0x00108165: bytes([0x65, cur[0], cur[1], 0x68]),
         0x0010816F: bytes([0x6F, gate[0], gate[1], 0x72])}
    for i, e in enumerate((D1F_ENT, D1F_ENT2)):
        p[e + 0x08] = le32(0x0BAD0008 + i)
        p[e + 0x18] = le32(0xFFFFFFFF) + le32(0xEEEEEEEE)
        p[e + 0x27] = bytes([0x27, e28[i], 0x29])
    return p


def d1f_drop_case(side, slot, rec, idx=3, pset=D1F_PSET):
    """0x44054: the eight tag slots (each a different record pointer; `rec` in the addressed one,
    0 for the empty arm), the record's +0x54/+0x56 words and the pset base DS_001014EC."""
    slots = [0x0010A200 + 0x10 * k for k in range(8)]
    slots[side * 4 + slot] = rec
    p = {D1F_TAGS: b"".join(le32(v) for v in slots), 0x001014EC: le32(pset)}
    if rec:
        p[rec + 0x54] = le16(0x0007) + le16(idx) + le16(0x0009)
    return p


def d1f_tag_case(side, slot, cur, held=0, free=True):
    """0x4408C: the eight tag slots (0 but for `held` in the addressed one and a sentinel in each
    neighbour), the two cursors, and what the allowed 0x2AE14 needs: the actor pool 0x1014F4 and
    pset 0x1014EC, the 0x105B3C free ring (one record at D1F_POOL; `free` False is the
    self-sentinel empty ring, so the alloc fails), the active-list sentinel 0x105BCC and the render
    free node 0x10275C."""
    slots = [0x0BAD0100 + k for k in range(8)]
    slots[side * 4 + slot] = held
    p = {D1F_TAGS: b"".join(le32(v) for v in slots),
         0x00108165: bytes([0x65, cur[0], cur[1], 0x68]),
         0x001014F4: le32(D1F_POOL), 0x001014EC: le32(D1F_PSET),
         0x00105B3C: le32(D1F_POOL if free else 0x00105B3C),
         0x00105BCC: le32(0x00105BCC), 0x00105BD0: le32(0x00105BCC),
         0x0010275C: le32(D1F_NODE), 0x00105B44: le32(0),
         D1F_POOL + 0x08: bytes(range(0x48, 0x48 + 0x38)), D1F_POOL + 0x40: bytes(range(0x90, 0x90 + 0x28)),
         D1F_PSET: bytes(range(0xC0, 0xC0 + 0x20))}
    p[D1F_POOL] = le32(0x00105B3C) + le32(0x00105B3C) if free else le32(0x0BAD0000) + le32(0x0BAD0004)
    return p


D1F_PORT_MUTANTS = ("@eq", "@hl", "@oth", "@obit", "@ne", "@zext", "@x", "@w18", "@bit", "@seek",
                    "@pal", "@voice", "@order", "@panel")

D1F_SPECS += [
    # 0x43EA0 fight_char_portrait: EAX = side. 0x2BCF4, 0x2A17C and 0x2C3FC are stubs. EAX dies
    # unread (0x43CA6 `mov eax,edx`): mask 0.
    Spec("fight_char_portrait", 0x43EA0, [
        Case("v0", {"eax": 0}, d1f_port_case((2, 2), gate=(0x77, 1))),
        Case("v1", {"eax": 0}, d1f_port_case((2, 3), gate=(0, 1), e28=(0xFB, 0x00))),
        Case("v2", {"eax": 0}, d1f_port_case((4, 4), gate=(1, 0))),
        Case("v3", {"eax": 1}, d1f_port_case((5, 5), gate=(1, 0), e28=(0xFB, 0x00))),
        Case("v4", {"eax": 1}, d1f_port_case((1, 6), gate=(1, 0x77))),
        Case("v5", {"eax": 0}, d1f_port_case((6, 6), gate=(0, 2))),
        Case("v6", {"eax": 0}, d1f_port_case((0xFF, 0), gate=(1, 0))),
        Case("v7", {"eax": 1}, d1f_port_case((3, 0xFE), gate=(1, 0), e28=(0x04, 0x04))),
    ], calls=(P6_2BCF4, PALETTE, VOICE), eax_mask=0,
       mutants=("@gate", "@one") + D1F_PORT_MUTANTS),
    # 0x4418C fight_char_team_portrait: EAX = side. The same without the 0x108170 test, and nothing
    # when the side's entry is 0 (0x4419F). EAX dies unread (0x448ED `mov eax,edx`): mask 0.
    Spec("fight_char_team_portrait", 0x4418C, [
        Case("w0", {"eax": 0}, d1f_port_case((2, 2))),
        Case("w1", {"eax": 0}, d1f_port_case((2, 3), e28=(0xFB, 0x00))),
        Case("w2", {"eax": 1}, d1f_port_case((5, 5), e28=(0xFB, 0x00))),
        Case("w3", {"eax": 1}, d1f_port_case((1, 6))),
        Case("w4", {"eax": 0}, d1f_port_case((4, 4), entries=(0, D1F_ENT2))),
        Case("w5", {"eax": 1}, d1f_port_case((4, 0), entries=(D1F_ENT, 0))),
        Case("w6", {"eax": 0}, d1f_port_case((0xFF, 0))),
        Case("w7", {"eax": 1}, d1f_port_case((3, 0xFE), e28=(0x04, 0x04))),
    ], calls=(P6_2BCF4, PALETTE, VOICE), eax_mask=0,
       mutants=D1F_PORT_MUTANTS + ("@null",)),
    # 0x44054 fight_char_team_tag_drop: EAX = side, EDX = slot. 0x2AD40 is the RELEASE stub; the
    # slot is zeroed after it (0x44083, ECX = 0 across the call). EAX dies unread (0x4445A `lea
    # eax,[ecx*8]`): mask 0.
    Spec("fight_char_team_tag_drop", 0x44054, [
        Case("d0", {"eax": 0, "edx": 0}, d1f_drop_case(0, 0, D1F_ENT)),
        Case("d1", {"eax": 1, "edx": 3}, d1f_drop_case(1, 3, D1F_ENT, idx=0xFFFF)),
        Case("d2", {"eax": 0, "edx": 2}, d1f_drop_case(0, 2, 0)),
        Case("d3", {"eax": 1, "edx": 0}, d1f_drop_case(1, 0, D1F_ENT2, idx=0, pset=0x0010A3A0)),
        Case("d4", {"eax": 0, "edx": 3}, d1f_drop_case(0, 3, D1F_ENT, idx=0x8001)),
    ], calls=(RELEASE,), eax_mask=0,
       mutants=("@stride", "@swap", "@w56", "@sext", "@pset", "@keep", "@order", "@nocall")),
    # 0x4408C fight_char_team_tag_add: EAX = side, EDX = slot. The descriptor is on the stack, so
    # 0x2AE14 is not stubbed (record E3 §E3.8): it runs on both sides (allowed) with its exercised
    # tree (0x2AC80, 0x249D0, 0x249B0, 0x2EA30, 0x2A408, 0x5D812, 0x1C390, 0x1C3A0), and the record
    # and pset it fills carry the descriptor. 0x33754 (the palette handle, +0x10) and 0x2A820 are
    # stubs. EAX dies unread (0x44414 `xor eax,eax`): mask 0.
    Spec("fight_char_team_tag_add", 0x4408C, [
        Case("g0", {"eax": 0, "edx": 0}, d1f_tag_case(0, 0, (2, 5)), {0x33754: 0x11111111}),
        Case("g1", {"eax": 1, "edx": 3}, d1f_tag_case(1, 3, (2, 5)), {0x33754: 0x22222222}),
        Case("g2", {"eax": 0, "edx": 1}, d1f_tag_case(0, 1, (2, 5), held=0x0010A200)),
        Case("g3", {"eax": 1, "edx": 2}, d1f_tag_case(1, 2, (2, 5), free=False)),
        Case("g4", {"eax": 0, "edx": 2}, d1f_tag_case(0, 2, (0xFF, 3)), {0x33754: 0x33333333}),
        Case("g5", {"eax": 1, "edx": 1}, d1f_tag_case(1, 1, (0, 6)), {0x33754: 0x44444444}),
    ], allow_calls=(0x2AE14, 0x2AC80, 0x249D0, 0x249B0, 0x2EA30, 0x2A408, 0x5D812, 0x1C390, 0x1C3A0),
       calls=(E.Call(0x33754, ("eax",), mode="stub"),
              E.Call(0x2A820, ("eax", "edx"), mode="stub", clobbers=("edx",))),
       eax_mask=0,
       mutants=("@stride", "@gate", "@spr", "@w6", "@w8", "@wa", "@wc", "@pal", "@zext", "@xoff",
                "@xy", "@layer", "@slot")),
]

# 0x49388, 0x49300, 0x4A634, 0x4E99C.


def d1f_pick_case(mode, r0, r1, r2=0x0BAD):
    """0x49388: the mode word 0x104B00 (its high byte set where the case wants a word that is not
    3 with a low byte of 3) and the two sides' range words at 0x108860, with the word after them
    (the @width mutant's side-1 read)."""
    return {0x00104B00: le16(mode), 0x00108860: le16(r0) + le16(r1) + le16(r2)}


D1F_SPECS += [
    # 0x49388 fight_dust_pick: EAX = side. The draw is a stub, so each threshold is hit on both
    # sides of its boundary; the raw's compares are signed (`jge` at 0x493B3, 0x493BF, 0x493CB,
    # 0x493D7, 0x493E3). Callers index with all of EAX (0x495B9 `[eax*4+0xc9524]`).
    Spec("fight_dust_pick", 0x49388, [
        Case("d0", {"eax": 0}, d1f_pick_case(3, 0x11, 0x22), {0x5D7DC: 0x00}),
        Case("d1", {"eax": 0}, d1f_pick_case(0x12, 0x50, 0x60), {0x5D7DC: 0x1D}),
        Case("d2", {"eax": 1}, d1f_pick_case(0x12, 0x50, 0x60), {0x5D7DC: 0x1E}),
        Case("d3", {"eax": 0}, d1f_pick_case(0x0103, 0x50, 0x60), {0x5D7DC: 0x31}),
        Case("d4", {"eax": 1}, d1f_pick_case(0, 0x50, 0xFFFF), {0x5D7DC: 0x32}),
        Case("d5", {"eax": 0}, d1f_pick_case(3, 0x50, 0x60), {0x5D7DC: 0x45}),
        Case("d6", {"eax": 0}, d1f_pick_case(0x12, 0x50, 0x60), {0x5D7DC: 0x46}),
        Case("d7", {"eax": 0}, d1f_pick_case(0x12, 0x50, 0x60), {0x5D7DC: 0x54}),
        Case("d8", {"eax": 0}, d1f_pick_case(0x12, 0x50, 0x60), {0x5D7DC: 0x55}),
        Case("d9", {"eax": 0}, d1f_pick_case(0x12, 0x50, 0x60), {0x5D7DC: 0x5E}),
        Case("d10", {"eax": 0}, d1f_pick_case(0x12, 0x50, 0x60), {0x5D7DC: 0x5F}),
        Case("d11", {"eax": 0}, d1f_pick_case(0x12, 0x50, 0x60), {0x5D7DC: 0x80000000}),
    ], calls=(D1F_RNG,), eax_mask=0xFFFFFFFF,
       mutants=("@side", "@mode8", "@r64", "@width", "@unsigned", "@t1e", "@t32", "@t46", "@t55",
                "@t5f")),
]


def d1f_li_case(mode, fill):
    """0x49300: the mode word 0x104AFC, the node area 0x1083C4..0x10884B and the 0x10884C
    sentinel pair filled with `fill`-derived bytes (so every link store shows), 0x108880, and the
    0x1088CB/CC pair between seeded neighbours."""
    p = {0x00104AFC: le16(mode), 0x00108880: le32(0x0BAD8880),
         0x001088CA: bytes([0xCA, 0x77, 0x88, 0xCD])}
    a = 0x001083C4
    while a < 0x00108854:
        n = min(0x40, 0x00108854 - a)
        p[a] = bytes(((a + k) * 7 + fill) & 0xFF or 0x5A for k in range(n))
        a += n
    return p


def d1f_fr_case(seed, s42, r):
    """0x4A634: the rng state 0xEF6D8 (0x5D7DC runs), each slot's +0x42 byte between +0x41 and
    +0x43, the two reaction bytes 0x1088A8, and the 0x10889E and 0x1088B2 pairs between seeded
    neighbours."""
    p = {0x000EF6D8: le32(seed),
         0x001088A7: bytes([0xA7, r[0], r[1], 0xAA]),
         0x0010889D: bytes([0x9D, 0x11, 0x22, 0xA0]),
         0x001088B1: bytes([0xB1, 0x33, 0x44, 0xB4])}
    for s in (0, 1):
        p[D1F_SLOTS + s * 0x94 + 0x41] = bytes([0x41, s42[s], 0x43])
    return p


def d1f_rc_case(bd, ch, pairs, b9=0x77):
    """0x4E99C: the round counter 0x1088BD, the character 0x1088BC (the index of the 5-byte rows
    at 0x108888), the flag 0x1088B9, each inside seeded neighbours, and the ten row bytes
    0x108888..0x108891 (`pairs` = the four bytes +0..+3 of the addressed row; the other row holds
    different bytes)."""
    rows = bytearray([0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9])
    rows[(ch & 1) * 5:(ch & 1) * 5 + 4] = bytes(pairs)
    return {0x00108888: bytes(rows),
            0x001088B8: bytes([0xB8, b9, 0xBA, 0xBB, ch, bd, 0xBE])}


D1F_SPECS += [
    # 0x49300 fight_list_init: no argument. 0x249C0 has no seam and runs on both sides (allowed;
    # its own row is list_insert_before). EAX dies unread (0x28E98 and 0x34978 load it before any
    # read, record §1): mask 0.
    Spec("fight_list_init", 0x49300, [
        Case("i0", {}, d1f_li_case(7, 0x11)),
        Case("i1", {}, d1f_li_case(6, 0x55)),
        Case("i2", {}, d1f_li_case(0x0107, 0xA3)),
    ], allow_calls=(0x249C0,), eax_mask=0,
       mutants=("@stride", "@first", "@bound", "@head", "@v1500", "@self", "@mode", "@word",
                "@swap")),
    # 0x4A634 fight_4a634: no argument. 0x5D7DC runs real (recorded, P6_RNG): a case's stub EAX is one
    # value, and the 0xCA/0xDB arm needs the two rng(2) draws to differ, so the draws come from
    # the 0xEF6D8 seed. 0x2C3FC is the VOICE stub. EAX dies unread (0x4A596 `xor eax,eax`): mask 0.
    Spec("fight_4a634", 0x4A634, [
        Case("e0", {}, d1f_fr_case(0x04, (0xFF, 0x01), (0x20, 0x20))),
        Case("e1", {}, d1f_fr_case(0x03, (0x02, 0x00), (0x3F, 0x30))),
        Case("e2", {}, d1f_fr_case(0x01, (0x00, 0x02), (0x00, 0x21))),
        Case("e3", {}, d1f_fr_case(0x03, (0x02, 0x00), (0x10, 0x00))),
        Case("e4", {}, d1f_fr_case(0x01, (0x02, 0x00), (0x17, 0x00))),
        Case("e5", {}, d1f_fr_case(0x06, (0x02, 0x02), (0x12, 0x2A))),
        Case("e6", {}, d1f_fr_case(0x15, (0x03, 0x03), (0x25, 0x14))),
        Case("e7", {}, d1f_fr_case(0x01, (0x02, 0x02), (0x1F, 0x40))),
        Case("e8", {}, d1f_fr_case(0x01, (0x02, 0x02), (0x0F, 0x18))),
    ], calls=(P6_RNG, VOICE), eax_mask=0,
       mutants=("@bit", "@r", "@lo20", "@hi3f", "@lo10", "@hi17", "@r3", "@v1", "@second", "@pair",
                "@clr", "@b9e", "@bb2", "@off", "@gate")),
    # 0x4E99C fight_4e99c: EAX = the count (0x4E9A3 `mov ebx,eax`; the caller's 0x4E91C `xor eax,eax`,
    # `mov ax,si`), tested as a word (`test bx,bx` 0x4E9D5). The incoming EBX is not read. 0x2C3FC and
    # 0x34D8C are stubs. EAX dies unread (0x4EBC1 `mov eax,0x62` in 0x4EBB8, the next call): mask 0.
    Spec("fight_4e99c", 0x4E99C, [
        Case("y0", {"eax": 0}, d1f_rc_case(0x00, 0, (0x11, 0x22, 0x33, 0x44))),
        Case("y1", {"eax": 0}, d1f_rc_case(0x01, 0, (0x03, 0x22, 0x33, 0x44))),
        Case("y2", {"eax": 3}, d1f_rc_case(0x00, 1, (0x11, 0x22, 0x33, 0x44))),
        Case("y3", {"eax": 3}, d1f_rc_case(0x01, 0, (0x07, 0x22, 0x33, 0x44))),
        Case("y4", {"eax": 3}, d1f_rc_case(0x01, 1, (0x02, 0x22, 0x33, 0x44))),
        Case("y5", {"eax": 0}, d1f_rc_case(0x02, 1, (0x11, 0x22, 0x33, 0x44))),
        Case("y6", {"eax": 0}, d1f_rc_case(0x04, 0, (0x11, 0x22, 0x33, 0x44))),
        Case("y7", {"eax": 0}, d1f_rc_case(0x05, 1, (0x11, 0x22, 0x04, 0x44))),
        Case("y8", {"eax": 4}, d1f_rc_case(0x06, 0, (0x11, 0x22, 0x33, 0x44))),
        Case("y9", {"eax": 4}, d1f_rc_case(0x07, 0, (0x11, 0x22, 0x06, 0x44))),
        Case("y10", {"eax": 4}, d1f_rc_case(0x07, 1, (0x11, 0x22, 0x01, 0x44))),
        Case("y11", {"eax": 0}, d1f_rc_case(0xFF, 0, (0x04, 0x22, 0x33, 0x44))),
        Case("y12", {"eax": 0x00010000}, d1f_rc_case(0x00, 0, (0x11, 0x22, 0x33, 0x44))),
        Case("y13", {"eax": 0x00000100}, d1f_rc_case(0x00, 0, (0x11, 0x22, 0x33, 0x44))),
        Case("y14", {"eax": 0}, d1f_rc_case(0x03, 0x80, (0x11, 0x22, 0x33, 0x44))),
    ], calls=(VOICE, FLASH), eax_mask=0,
       mutants=("@inc", "@save", "@five", "@par", "@bx8", "@bx32", "@idx", "@hi", "@ten", "@v0",
                "@b9", "@voice", "@tail", "@tog", "@flash", "@order", "@sext")),
]

D1_SPECS += D1F_SPECS

# ---- track D wave 2, batch d2a (record 2026-10-11-reverse-d2a): game/fighter.c --------------------
#
# Every index a case passes (side, state, character) keeps base + index * size inside the image for
# the binding and for each mutant (the brief's mem[] rule); what a mutant's wrong read finds is
# seeded with the opposite answer, never left to whatever the image holds.

D2A_SLOT = 0x001077B0            # DS_001077B0: the two 0x94-byte fighter slots
D2A_R0, D2A_R1, D2A_RX = 0x0010A200, 0x0010A300, 0x0010A400   # zero BSS: the slots' records, and a third
D2A_HIT_C = E.Call(0x3C520, ("eax", "edx", "s0"), pop=4, clobbers=("edx",))
D2A_STEP_146F0 = (0x000FD108, 0x000FD11C, 0x0009AFDA)   # the per-side dword, the per-side byte, the constants
D2A_STEP_14A5C = (0x000FD100, 0x000FD11A, 0x0009AFEA)


def d2a_step_case(tw, side, gate, cmd, fd, flag, x_o, dx, y, w, s42=0xFF, xrec=False):
    """0x146F0/0x14A5C. `gate` makes 0x14590(side) 1 ("pose": the other slot's +0x10 is 0x39CC8;
    "st": its +0x52 is 0x11) or 0 ("no": neither; "le": the side's 0x107D2C word is negative), and
    0x14590(1 - side) the opposite. The side's command word, per-side dword and per-side byte are as
    given; the other side's are chosen so that reading them flips the add/subtract choice (the
    word: bit 13 alone or bit 12 alone, so no OR), the OR'd bit 13 (the dword) or the arm (the
    byte). The constants are the dx, y and w words (0x9AFDC/DE/E0 or the twin's), the word before
    them seeded; the side slot's +0x41..+0x43 and +0x52..+0x58, its record's +0x34..+0x39 and
    +0x42..+0x47 and the bytes 0x1078F7..FA are seeded. `xrec`: EDX is a third record whose +0x51
    names the side, so ctx[4] is not EDX."""
    fd_a, flag_a, k = tw
    other = 1 - side
    so, oo = D2A_SLOT + side * 0x94, D2A_SLOT + other * 0x94
    rs, ro = (D2A_R0, D2A_R1)[side], (D2A_R0, D2A_R1)[other]
    on = gate in ("pose", "st")
    sub = bool((cmd if cmd & 0x3000 else cmd | fd) & 0x2000)
    words, fds, flags, gws = [0, 0], [0, 0], [0, 0], [5, 5]
    words[side], words[other] = cmd, (0x1000 if sub else 0x2000)
    fds[side], fds[other] = fd, fd ^ 0x2000
    flags[side], flags[other] = flag, (0 if flag else 1)
    if gate == "le":
        gws[side] = 0x8000
    p = {so: le32(rs), oo: le32(ro),
         so + 0x10: le32(0x12345678 if on else 0x00039CC8),
         oo + 0x10: le32(0x00039CC8 if gate in ("pose", "le") else 0x12349CC8),
         so + 0x2C: le32(0x00777000), oo + 0x2C: le32(x_o),
         so + 0x41: bytes([0x41, s42, 0x43]), oo + 0x41: bytes([0x61, 0xFF, 0x63]),
         so + 0x52: bytes([0x33, 0x53, 0x7F, 0x55, 0x56, 0x77, 0x58]),
         oo + 0x52: bytes([0x11 if gate == "st" else 0x44, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68]),
         0x00107D2C: le16(gws[0]) + le16(gws[1]),
         0x001088E0: le16(words[0]) + le16(words[1]),
         fd_a: le32(fds[0]) + le32(fds[1]), flag_a: bytes(flags),
         k - 2: le16(0xAAAA) + le16(0xBBBB) + le16(dx) + le16(y) + le16(w),
         rs + 0x34: le16(0x3434) + le16(0x3636) + le16(0x3838),
         rs + 0x42: le16(0x4242) + le16(0x4444) + le16(0x4646),
         rs + 0x51: bytes([side]), ro + 0x51: bytes([other]),
         0x001078F7: bytes([0xF7, 0x08, 0x09, 0xFA])}
    if xrec:
        p[D2A_RX + 0x34] = le16(0x7434) + le16(0x7636) + le16(0x7838)
        p[D2A_RX + 0x42] = le16(0x7242) + le16(0x7444) + le16(0x7646)
        p[D2A_RX + 0x51] = bytes([side])
    return p


def d2a_step_cases(tw):
    """The six cases both twins take: every block, both sides, both arms, the OR taken and not."""
    rec = (D2A_R0, D2A_R1)
    return [
        Case("c0", {"eax": 0x11111111, "edx": rec[0]},
             d2a_step_case(tw, 0, "pose", 0x0000, 0x2000, 1, 0x00050000, 0x0011, 0x0080, 0x0019)),
        Case("c1", {"eax": 0x11111111, "edx": rec[1]},
             d2a_step_case(tw, 1, "no", 0x1000, 0x2000, 0, 0x00012345, 0x0011, 0x0080, 0x0019)),
        Case("c2", {"eax": 0x11111111, "edx": rec[0]},
             d2a_step_case(tw, 0, "st", 0x2000, 0x0000, 0, 0x00000005, 0xFFF0, 0x0080, 0x0019, s42=0x04)),
        Case("c3", {"eax": 0x11111111, "edx": D2A_RX},
             d2a_step_case(tw, 1, "le", 0x00FF, 0x1000, 0x80, 0xFFFFFF00, 0xFFF0, 0xFF80, 0x7001, xrec=True)),
        Case("c4", {"eax": 0x11111111, "edx": rec[1]},
             d2a_step_case(tw, 1, "pose", 0x3000, 0x0000, 1, 0x00040000, 0x8000, 0x7FFF, 0xFFFF)),
        Case("c5", {"eax": 0x11111111, "edx": rec[0]},
             d2a_step_case(tw, 0, "no", 0x0000, 0x0000, 0, 0x00060000, 0x7FFF, 0x0000, 0x0001, s42=0xFB)),
    ]


D2A_STEP_CALLS = (CLEAR34, D2A_HIT_C, HIT_A, P6_1890C, ANCHORX, C1_FACING, VOICE)
D2A_STEP_MUTANTS = ("@gate", "@oside", "@cmd", "@m30", "@fd", "@flag", "@clr", "@ysh", "@w36", "@w44",
                    "@rec", "@f8", "@sign", "@x2c", "@dxsh", "@and", "@s57", "@early", "@s54", "@a",
                    "@order", "@hold", "@voice", "@wide", "@xw")

D2A_39278 = E.Call(0x39278, ("eax",), mode="real")   # recorded and run (the seam on fighter_39278)


def d2a_hold_case(side, st, o53, w, d=None, s54=(0x33, 0x44)):
    """0x192DC: the side slot's +0x5F (the other slot's is on the other side of 0x20) and the other
    slot's +0x53 (the own slot's is 1 only when that one is not), both slots' +0x54 (0x107804 and
    0x107898, which the dead compares at 0x1932A/0x19385 read), the side's 0x100CE0 word (the other
    side's is on the other side of 2) and the two 0x107D2C words (`d`, or the opposite of the
    0x100CE0 verdict: what @tab reads), with the word before each pair and the bytes around
    DS_001088EC seeded."""
    so, oo = D2A_SLOT + side * 0x94, D2A_SLOT + (1 - side) * 0x94
    low = ((w ^ 0x8000) - 0x8000) < 2
    ws, ds = [0, 0], [0, 0]
    ws[side], ws[1 - side] = w, (2 if low else 1)
    ds[side] = (2 if low else 1) if d is None else d
    ds[1 - side] = 2 if ds[side] < 2 else 1
    return {so + 0x53: bytes([0 if o53 == 1 else 1, s54[side]]), oo + 0x53: bytes([o53, s54[1 - side]]),
            so + 0x5F: bytes([st]), oo + 0x5F: bytes([0x20 if st < 0x20 else 0x1F]),
            0x00100CDE: le16(0xDEDE) + le16(ws[0]) + le16(ws[1]),
            0x00107D2A: le16(0x2A2A) + le16(ds[0]) + le16(ds[1]),
            0x001088EA: b"\xea\xea\xec\xec\xee\xee"}


D2A_PSET = 0x00108000            # the pset pool DS_001014EC points at (d1a's base: entry 0x102 stays in the image)
D2A_SNAP_SLOT, D2A_SNAP_REC = 0x00104530, 0x00104658   # 0x235C4: 0x33ACC's two destinations, per side


def d2a_snap_case(side, o53, idx=3):
    """0x235C4: both slots' 0x94 bytes (the record pointer, then slot-distinct bytes; the other
    slot's +0x53 is `o53`, what 0x22B28 compares with 0x0A) and both records' 0x68 bytes (the
    side's +0x56 word names pset entry `idx`, the other's entry 5), the two snapshot areas
    0x104530..0x104727 filled with 0xEE, the pset entries' +0x18 dwords, and 0x22B28's tick words
    and flag bytes seeded."""
    recs = (D2A_R0, D2A_R1)
    p = {}
    for k in (0, 1):
        slot = bytearray((0x40 + k * 0x80 + i) & 0xFF for i in range(0x94))
        slot[0:4] = le32(recs[k])
        if k != side:
            slot[0x53] = o53
        so = D2A_SLOT + k * 0x94
        for off in range(0, 0x94, 0x40):
            p[so + off] = bytes(slot[off:off + 0x40])
        rec = bytearray((0x10 + k * 0x60 + i * 3) & 0xFF for i in range(0x68))
        rec[0x56:0x58] = le16(idx if k == side else 5)
        p[recs[k]] = bytes(rec[:0x40])
        p[recs[k] + 0x40] = bytes(rec[0x40:])
    for off in range(0, 0x1F8, 0x40):
        p[D2A_SNAP_SLOT + off] = b"\xEE" * min(0x40, 0x1F8 - off)
    p[0x001014EC] = le32(D2A_PSET)
    p[D2A_PSET + idx * 0x20 + 0x18] = le32(0x00C0FFEE)
    p[D2A_PSET + 5 * 0x20 + 0x18] = le32(0x0BADBAD5)
    p[0x0010474C] = le16(0x7777) * 2
    p[0x0010476C] = b"\x99\x99"
    return p


D2A_FS, D2A_FR = 0x0010A000, 0x0010A100          # 0x35B7C: a scratch slot and its record
D2A_SEEK_A, D2A_SEEK_B = 0x0010A300, 0x0010A500  # ... and 0x35C1C's two frame-word tables


def d2a_enter_case(ch, word, frame, s53, s43=0x02, s54=0x04):
    """0x35B7C: the slot's +0x42..+0x44, +0x51..+0x55 and +0x7A; the record's +0x28/+0x29 with the
    words around them, +0x4F..+0x53 (the frame at +0x52, inside the dword the raw shifts) and
    +0x57..+0x59 (the step, seeded 0x5A); the eight words at 0xBDA3E (distinct bytes, the word of
    `ch` as given); and what the two real callees read: 0x35C1C's frame-word tables (A[i] = 0x1100
    + i, B[i] = 0x2200 + i for i = -8..23), 0x35D20's two stream dwords and DS_001078FE."""
    tab = bytearray(0x30 + 7 * j for j in range(16))
    tab[ch * 2:ch * 2 + 2] = le16(word)
    return {D2A_FS + 0x42: bytes([0x42, s43, 0x44]), D2A_FS + 0x51: bytes([0x51, 0x11, s53, s54, 0x55]),
            D2A_FS + 0x7A: bytes([ch]),
            D2A_FR + 0x26: le16(0x2626) + b"\x52\x51" + le16(0x2A2A),
            D2A_FR + 0x4F: bytes([0x4F, 0x50, 0x51, frame, 0x53]), D2A_FR + 0x57: bytes([0x57, 0x5A, 0x59]),
            0x000BDA3E: bytes(tab),
            0x000C8A68 + ch * 4: le32(D2A_SEEK_A), 0x000C8AE0 + ch * 4: le32(D2A_SEEK_B),
            D2A_SEEK_A - 16: b"".join(le16(0x1100 + (i & 0xFF)) for i in range(-8, 24)),
            D2A_SEEK_B - 16: b"".join(le16(0x2200 + (i & 0xFF)) for i in range(-8, 24)),
            0x000C8A90 + ch * 4: le32(0x000E4000 + ch), 0x000C8B08 + ch * 4: le32(0x000E5000 + ch),
            0x001078FD: bytes([0xFD, 0x77, 0xFF])}


D2A_365C8 = E.Call(0x365C8, ("eax", "edx", "ebx"), clobbers=("ebx", "edx"))
D2A_CONSUME = E.Call(0x3BDDC, ("eax",), clobbers=("ebp",))
D2A_H_TABS = (0x000C89C8, 0x000C8F90, 0x000C8FB8)   # 0x36430/0x364FC: the three per-character stream tables


def d2a_handler_case(side, s43, cmd, ch=2, f28=0, own43=None):
    """0x36430/0x364FC: EAX is the side's slot, or (with `own43`) the scratch slot D2A_FS while the
    side's own slot carries +0x43 = own43 (what the raw's bit-2 test reads). The slot's
    +0x42..+0x44, +0x51..+0x55 and +0x7A; both records' +0x28 word (0x1A640's facing bit); the
    side's command word (the other side's has its four high bits flipped); and the three stream
    dwords of `ch` and of ch sign-extended, all distinct."""
    so, oo = D2A_SLOT + side * 0x94, D2A_SLOT + (1 - side) * 0x94
    slot = so if own43 is None else D2A_FS
    words = [0, 0]
    words[side], words[1 - side] = cmd, cmd ^ 0xF000
    p = {so: le32((D2A_R0, D2A_R1)[side]), oo: le32((D2A_R0, D2A_R1)[1 - side]),
         D2A_R0 + 0x28: le16(f28), D2A_R1 + 0x28: le16(f28),
         slot + 0x42: bytes([0x42, s43, 0x44]), slot + 0x51: bytes([0x51, 0x11, 0x53, 0x7F, 0x55]),
         slot + 0x7A: bytes([ch]), 0x001088E0: le16(words[0]) + le16(words[1])}
    if own43 is not None:
        p[so + 0x42] = bytes([0x62, own43, 0x64])
    for i, c in enumerate(sorted({ch, (ch ^ 0x80) - 0x80})):
        for j, t in enumerate(D2A_H_TABS):
            p[t + c * 4] = le32(0x000E1000 + j * 0x1000 + i * 0x100 + (c & 0xFF))
    return p


def d2a_handler_cases():
    """The eleven cases both twins take (EAX = slot, EDX = rec, EBX = side)."""
    def regs(side, scratch=False):
        return {"eax": D2A_FS if scratch else D2A_SLOT + side * 0x94, "edx": (D2A_R0, D2A_R1)[side], "ebx": side}
    return [
        Case("k0", regs(0), d2a_handler_case(0, 0x00, 0x0000), {0x365C8: 1, 0x36638: 1}),
        Case("k1", regs(1), d2a_handler_case(1, 0xFF, 0x0000), {0x365C8: 0, 0x36638: 0}),
        Case("k2", regs(0), d2a_handler_case(0, 0xFB, 0x80FF), {0x365C8: 0x100, 0x36638: 0x100, 0x3BDDC: 1}),
        Case("k3", regs(1), d2a_handler_case(1, 0x04, 0x8000), {0x365C8: 0xFFFFFF01, 0x36638: 0}),
        Case("k4", regs(0), d2a_handler_case(0, 0x00, 0x5000), {0x365C8: 0, 0x36638: 0}),
        Case("k5", regs(1), d2a_handler_case(1, 0x00, 0xD000, f28=0x4000), {0x365C8: 0, 0x36638: 0}),
        Case("k6", regs(1, True), d2a_handler_case(1, 0x04, 0x8000, own43=0xFB), {0x365C8: 0, 0x36638: 0}),
        Case("k7", regs(0, True), d2a_handler_case(0, 0xFB, 0x8000, own43=0x04), {0x365C8: 0, 0x36638: 0}),
        Case("k8", regs(0), d2a_handler_case(0, 0x40, 0x0000, ch=0x80), {0x365C8: 0, 0x36638: 0}),
        Case("k9", regs(1), d2a_handler_case(1, 0x00, 0x6000, ch=0x80, f28=0x4000), {0x365C8: 0, 0x36638: 0}),
        Case("k10", regs(0), d2a_handler_case(0, 0x00, 0x4000, ch=0x80), {0x365C8: 0, 0x36638: 0}),
    ]


D2A_HANDLER_CALLS = (D2A_365C8, C1_36638, D2A_CONSUME, ANIM_BEGIN)
D2A_HANDLER_MUTANTS = ("@al365", "@al366", "@slot43", "@or", "@cmd", "@prio", "@b2", "@cside", "@tab",
                       "@hold", "@s52a", "@s54", "@early", "@inv", "@s52b", "@late", "@csext", "@order",
                       "@no366", "@side1a", "@holdb", "@tabb", "@al1a")

D2A_BED10, D2A_BED60, D2A_BD882 = 0x000BED10, 0x000BED60, 0x000BD882   # 0x39B30's three per-character tables


def d2a_launch_case(side, ch, s54, y, cnt, n, h, m, a, den, ground, npad=0, och=1):
    """0x39B30: both slots' record pointers, +0x30, +0x40..+0x42, +0x54, +0x67..+0x69, +0x74..+0x77
    and +0x7A (the other slot: another character, the other +0x54 arm, a y on the other side of the
    ground); the side record's +0x34..+0x39, +0x42..+0x47 and +0x62..+0x64 (the other record's
    seeded differently); the four per-side words n, h, m, a at 0x107A60/68/70/78 (the other side's
    are other values; `npad` is the word above n, the high half of the dword the raw's fild reads);
    and for `ch`, the other character and ch sign-extended the divisor dword, the stream dword and
    the ground word (0xBD884 + ch*2, with the word below it)."""
    other = 1 - side
    so, oo = D2A_SLOT + side * 0x94, D2A_SLOT + other * 0x94
    rs, ro = (D2A_R0, D2A_R1)[side], (D2A_R0, D2A_R1)[other]
    gs = (ground ^ 0x8000) - 0x8000
    ys = (y ^ 0x80000000) - 0x80000000
    quad = [[0, 0], [0, 0], [0, 0], [0, 0]]
    for i, (v, o) in enumerate(((n, 0x0005), (h, 0x0123), (m, 0x0009), (a, 0x0321))):
        quad[i][side], quad[i][other] = v, o
    pad = [0x5A5A, 0x5A5A]
    pad[side] = npad
    p = {so: le32(rs), oo: le32(ro),
         so + 0x30: le32(y), oo + 0x30: le32(0x7FFFFFFF if gs > ys else 0x80000000),
         so + 0x40: bytes([0x40, 0x41, 0x42]), oo + 0x40: bytes([0x60, 0x61, 0x62]),
         so + 0x54: bytes([s54]), oo + 0x54: bytes([0 if s54 == 2 else 2]),
         so + 0x67: bytes([0x67, cnt, 0x69]), oo + 0x67: bytes([0x57, 0x7F, 0x59]),
         so + 0x74: le16(0x7474) + le16(0x7676), oo + 0x74: le16(0x6464) + le16(0x6666),
         so + 0x7A: bytes([ch]), oo + 0x7A: bytes([och]),
         rs + 0x34: le16(0x3434) + le16(0x3636) + le16(0x3838), ro + 0x34: le16(0x2434) + le16(0x2636) + le16(0x2838),
         rs + 0x42: le16(0x4242) + le16(0x4444) + le16(0x4646), ro + 0x42: le16(0x2242) + le16(0x2444) + le16(0x2646),
         rs + 0x62: bytes([0x62, 0x7F, 0x64]), ro + 0x62: bytes([0x52, 0x6F, 0x54]),
         0x00107A5E: le16(0x5E5E) + le16(quad[0][0]) + le16(pad[0]) + le16(quad[0][1]) + le16(pad[1])
                     + le16(quad[1][0]) + le16(0x6A6A) + le16(quad[1][1]) + le16(0x6E6E)
                     + le16(quad[2][0]) + le16(0x7272) + le16(quad[2][1]) + le16(0x7676)
                     + le16(quad[3][0]) + le16(0x7A7A) + le16(quad[3][1]) + le16(0x7E7E)}
    chars = [(ch, den, 0x000E6000, ground)]
    if och != ch:
        chars.append((och, den + 3, 0x000E7000, (ground + 0x40) & 0xFFFF))
    sx = (ch ^ 0x80) - 0x80
    if sx != ch:
        chars.append((sx, den + 5, 0x000E8000, (ground + 0x80) & 0xFFFF))
    for c, d, st, gr in chars:
        p[D2A_BED10 + c * 4] = le32(d)
        p[D2A_BED60 + c * 4] = le32(st + (c & 0xFF))
        p[D2A_BD882 + c * 2] = le16(0x8282) + le16(gr)
    return p


D2A_ROWS = 0x00106000            # 0x39FF4: the 0x68-byte rows DS_001014F4 points at (row 0x80 stays in the image)
D2A_2BD44 = E.Call(0x2BD44, ("eax", "edx"), clobbers=("edx",))


def d2a_freeze_side(side, child, flag, f40, t, x):
    """0x39FF4, one side: the slot's nine dwords +0x00..+0x23 (the record pointer, then
    slot-distinct sentinels), +0x40, +0x5C..+0x5E, +0x7A and +0x8A..+0x8F (the timer word `t`
    between two seeded words); the record's +0x18 (x) through +0x27 (+0x24 = 0, what the real
    0x1922C seeds), +0x29..+0x2B and +0x4A..+0x4C (the child index); and the child's row: +0x60
    = `flag` between two seeded bytes (+0x61 the opposite, what @flag reads), the opposite flag
    at the 0x60 stride (@row) and at the sign-extended index (@csext)."""
    so, rec = D2A_SLOT + side * 0x94, (D2A_R0, D2A_R1)[side]
    p = {so: le32(rec) + b"".join(le32(v * 0x01010101 + side) for v in (4, 8, 0x0C, 0x10, 0x14, 0x18, 0x1C, 0x20)),
         so + 0x40: le32(f40), so + 0x5C: bytes([0x5C, 0x7F, 0x5E]), so + 0x7A: bytes([3 + side]),
         so + 0x8A: le16(0x8A8A) + le16(t) + le16(0x8E8E),
         rec + 0x18: le32(x) + le32(0x1C1C1C1C) + le32(0x20202020) + le32(0),
         rec + 0x29: bytes([0x29, 0xFF, 0x2B]), rec + 0x4A: bytes([0x4A, child, 0x4C])}
    if child:
        no = 0 if flag else 1
        p[D2A_ROWS + child * 0x68 + 0x5F] = bytes([0x5F, flag, 0 if flag else 0x61])
        p[D2A_ROWS + child * 0x60 + 0x60] = bytes([no])
        if child >= 0x80:
            p[D2A_ROWS + (child - 0x100) * 0x68 + 0x5F] = bytes([0x5F, no, 0x61])
    return p


def d2a_freeze_case(cam, s, actors, side0, side1):
    """0x39FF4: the two sides (d2a_freeze_side's arguments after the side), the camera x
    DS_000F0AF0, the row base, row 0's flag (what @child0 reads), and what the two real callees
    read: DS_001078FF, the two DS_001077A0 actor dwords and DS_00104AE9 (0x34038), the eight bytes
    DS_00100B58..5F (0x1922C)."""
    p = {0x000F0AF0: le32(cam), 0x001014F4: le32(D2A_ROWS), D2A_ROWS + 0x5F: bytes([0x5F, 0x01, 0x00]),
         0x001078FF: bytes([s]), 0x0010779C: le32(0x9C9C9C9C) + le32(actors[0]) + le32(actors[1]),
         0x00104AE9: b"\xFF", 0x00100B58: bytes([0x58, 0x59, 0x01, 0xFF, 0x05, 0x07, 0x5E, 0x5F])}
    p.update(d2a_freeze_side(0, *side0))
    p.update(d2a_freeze_side(1, *side1))
    return p


# 0x3BAEC: the real 0x1883C re-latches the slots; this stub clears both slots' +0x41 instead, so the
# bit 15 the raw saved before the pushes (0x3BB0A..0x3BB26) and ORs back after them is observable.
D2A_1883C_CLR = E.Call(0x1883C, ("eax", "edx", "ebx"), clobbers=("ebx", "edx"),
                       writes=((None, D2A_SLOT + 0x41, b"\x00"), (None, D2A_SLOT + 0x94 + 0x41, b"\x00")))


def d2a_push_case(f40, x, s54=(0, 0), w34=(0x1234, 0xEDCC), w4c=(0x0123, 0x8123)):
    """0x3BAEC: per slot the +0x40 dword (+0x41 bit 7 is the kept bit, +0x42 bit 2 the gate and bit
    5 0x3B9D8's own early return, +0x43 bit 1 the order), +0x2C x, +0x4A..+0x4D and +0x53..+0x55;
    the records' +0x32..+0x37; and DS_00107D30 with its neighbours (what the real 0x3B9D8 reads
    and writes)."""
    p = {0x00107D2F: b"\x2f\x30\x31"}
    for k in (0, 1):
        so, rec = D2A_SLOT + k * 0x94, (D2A_R0, D2A_R1)[k]
        p[so] = le32(rec)
        p[so + 0x2C] = le32(x[k])
        p[so + 0x40] = le32(f40[k])
        p[so + 0x4A] = le16(0x4A4B + k) + le16(w4c[k])
        p[so + 0x53] = bytes([0x53, s54[k], 0x55])
        p[rec + 0x32] = le16(0x3233) + le16(w34[k]) + le16(0x3637)
    return p


D2A_DESC = 0x0010A600            # 0x3C758/0x3C800: the frame descriptor the 0x3C600 stub returns (0x14-byte entries)
D2A_OUT, D2A_OUT2 = 0x0010A720, 0x0010A740   # the two stack dwords the raw's callers pass by address
D2A_DESC_CALL = E.Call(0x3C600, ("eax", "edx"))


def d2a_frame_pokes(side, i, phase, entry, cmd):
    """The phase word of (side, i) at 0x107D58 + side*0x40 + i*2 between two seeded words, the
    0x14-byte descriptor entry of `phase` (sign-extended: `sar 16`) at D2A_DESC, and the side's
    command word (the other side's is the complement)."""
    off = side * 0x40 + i * 2
    ps = (phase ^ 0x8000) - 0x8000
    words = [0, 0]
    words[side], words[1 - side] = cmd, cmd ^ 0xFFFF
    return {0x00107D56 + off: le16(0x5656) + le16(phase) + le16(0x5A5A),
            D2A_DESC + ps * 0x14: entry, 0x001088E0: le16(words[0]) + le16(words[1])}


def d2a_connect_case(side, i, phase, m, cmd, out):
    """0x3C758: the entry's three input dwords `m` (+0x0C..+0x13 seeded) and the dword the stack
    argument points at, between two seeded dwords."""
    p = d2a_frame_pokes(side, i, phase, le32(m[0]) + le32(m[1]) + le32(m[2]) + le32(0x0C0C0C0C) + le32(0x10101010),
                        cmd)
    p[D2A_OUT - 4] = le32(0x1C1C1C1C) + le32(out) + le32(0x24242424)
    return p


def d2a_displace_case(side, i, phase, m4, disp, cmd, m2=0x08090A0B):
    """0x3C800: the entry's +0x10 input dword `m4` and the word at +0x0C (`disp`: the high half of
    the dword at +0x0A the raw shifts), the rest seeded; both stack dwords seeded with their
    neighbours."""
    p = d2a_frame_pokes(side, i, phase, le32(0x00010203) + le32(0x04050607) + le32(m2) + le16(disp) + le16(0x0E0F)
                        + le32(m4), cmd)
    p[D2A_OUT - 4] = le32(0x1C1C1C1C) + le32(0x20202020) + le32(0x24242424)
    p[D2A_OUT2 - 4] = le32(0x3C3C3C3C) + le32(0x5A5A5A5A) + le32(0x44444444)
    return p


def d2a_pred_case(side, st, f28, cmd):
    """0x467DC/0x4682C: the side slot's +0x52..+0x55 (`st` = the +0x52, +0x53, +0x54 bytes; +0x55
    seeded 0x55) and the other slot's (0x02 0x00 0x00 when the side's pass the three tests, else
    0x01 0x00 0x00), both records' +0x28 word (0x1A5D4's facing bit) and the side's command word
    (the other side's has bits 12 and 13 flipped, which flips 0x1A5D4's verdict)."""
    so, oo = D2A_SLOT + side * 0x94, D2A_SLOT + (1 - side) * 0x94
    words = [0, 0]
    words[side], words[1 - side] = cmd, cmd ^ 0x3000
    return {so: le32((D2A_R0, D2A_R1)[side]), oo: le32((D2A_R0, D2A_R1)[1 - side]),
            so + 0x52: bytes(st) + b"\x55", oo + 0x52: bytes([2 if tuple(st) == (1, 0, 0) else 1, 0, 0, 0x55]),
            D2A_R0 + 0x28: le16(f28), D2A_R1 + 0x28: le16(f28),
            0x001088E0: le16(words[0]) + le16(words[1])}


def d2a_pred_cases():
    """The ten cases both twins take."""
    return [
        Case("e0", {"eax": 0}, d2a_pred_case(0, (1, 0, 0), 0x0000, 0x2000)),
        Case("e1", {"eax": 1}, d2a_pred_case(1, (1, 0, 0), 0x4000, 0x2000)),
        Case("e2", {"eax": 0}, d2a_pred_case(0, (1, 0, 1), 0x0000, 0x2000)),
        Case("e3", {"eax": 1}, d2a_pred_case(1, (1, 0x80, 0), 0x4000, 0x2000)),
        Case("e4", {"eax": 0}, d2a_pred_case(0, (2, 0, 0), 0x0000, 0x0000)),
        Case("e5", {"eax": 1}, d2a_pred_case(1, (0, 0, 0), 0x0000, 0x2000)),
        Case("e6", {"eax": 0}, d2a_pred_case(0, (1, 0, 0), 0x4000, 0x1000)),
        Case("e7", {"eax": 1}, d2a_pred_case(1, (1, 0, 0), 0x0000, 0xDFFF)),
        Case("e8", {"eax": 1}, d2a_pred_case(1, (1, 0, 1), 0x0000, 0x0000)),
        Case("e9", {"eax": 0}, d2a_pred_case(0, (1, 1, 0), 0x0000, 0x2000)),
    ]


D2A_PRED_MUTANTS = ("@slot", "@s54", "@s53", "@s52", "@wide", "@fside", "@al", "@inv")

D2A_CFG = 0x0010A000             # 0x461DC: what DS_00101514 points at (the +0x2D4/+0x2D6 selector words)


def d2a_ring_case(sel, s63, w, pos):
    """0x461DC: the two selector words (between two seeded words), both slots' +0x62..+0x64, the
    two command words, the ring index word DS_001082D4 between two seeded words, and both
    0x14-word rings 0x108270/0x108298 with the word before the first and the word after the second
    seeded (index -1 lands on 0x10826E and 0x108296)."""
    return {0x00101514: le32(D2A_CFG), D2A_CFG + 0x2D2: le16(0xD2D2) + le16(sel[0]) + le16(sel[1]) + le16(0xD8D8),
            D2A_SLOT + 0x62: bytes([0x62, s63[0], 0x64]), D2A_SLOT + 0x94 + 0x62: bytes([0x72, s63[1], 0x74]),
            0x001088DE: le16(0xDEDE) + le16(w[0]) + le16(w[1]) + le16(0xE4E4),
            0x001082D2: le16(0xD2D2) + le16(pos) + le16(0xD6D6),
            0x0010826E: le16(0x6E6E) + bytes(0x80 + i for i in range(0x28)),
            0x00108298: bytes(0xC0 + i for i in range(0x28)) + le16(0xC0C0)}


D2A_AI_P1, D2A_AI_P2 = 0x0010A600, 0x0010AB00   # 0x46C78: the two id-indexed tables the real 0x46BBC reads


def d2a_state_case(side, state, s5f, s8a=0, s64=0xFF, sc=2, oc=4):
    """0x46C78: the side's AI block (+0x10..+0x1B seeded, +0x24..+0x3B with the state dword at
    +0x28) and the other block's (another state: what @bside reads); the other slot's +0x5F, +0x64
    and +0x8A as given and the own slot's different (what @slot reads); and what the real 0x46BBC
    reads: both characters, the pair-table entries, and the two tables' dwords for every id a path
    or a mutant can pick (distinct sentinels)."""
    b, ob = 0x001081F0 + side * 0x40, 0x001081F0 + (1 - side) * 0x40
    so, oo = D2A_SLOT + side * 0x94, D2A_SLOT + (1 - side) * 0x94
    idx = sc * 10 + oc
    p = {so + 0x7A: bytes([sc]), oo + 0x7A: bytes([oc]),
         oo + 0x5F: bytes([s5f]), oo + 0x64: bytes([s64]), oo + 0x8A: bytes([s8a]),
         so + 0x5F: bytes([s5f ^ 0x15]), so + 0x64: bytes([0x33 if s64 == 0xFF else 0xFF]),
         so + 0x8A: bytes([0 if s8a else 1]),
         0x00095CA8 + idx * 4: le32(D2A_AI_P1), 0x00095E38 + idx * 4: le32(D2A_AI_P2),
         0x00095CA8 + (oc * 10 + sc) * 4: le32(D2A_AI_P2), 0x00095E38 + (oc * 10 + sc) * 4: le32(D2A_AI_P1),
         b + 0x10: le32(0x10101010) + le32(0x14141414) + le32(0x18181818),
         b + 0x24: le32(0x24242424) + le32(state) + b"\x2c\x2d\x2e\x2f" + le32(0x30303030) + le32(0x34343434)
                   + le32(0x38383838),
         ob + 0x10: le32(0x50505050) + le32(0x54545454) + le32(0x58585858),
         ob + 0x24: le32(0x64646464) + le32(2 if state != 2 else 3) + b"\x6c\x6d\x6e\x6f" + le32(0x70707070)
                    + le32(0x74747474) + le32(0x78787878)}
    ids = {s5f, s5f ^ 0x15, s5f + 0x40, (s5f + 0x40) & 0xFF, (s5f ^ 0x15) + 0x40, 0x33, 0xFF, s64}
    if s5f >= 0x80:
        ids.add(s5f - 0x100)
    for i in sorted(ids - set(range(0x81, 0x8B))):
        p[D2A_AI_P1 + i * 4] = le32(0x11000000 + (i & 0xFFFF))
        p[D2A_AI_P2 + i * 4] = le32(0x22000000 + (i & 0xFFFF))
    p[D2A_AI_P1 + 0x81 * 4] = b"".join(le32(0x11000000 + i) for i in range(0x81, 0x8B))
    p[D2A_AI_P2 + 0x81 * 4] = b"".join(le32(0x22000000 + i) for i in range(0x81, 0x8B))
    return p


def d2a_wall_case(side, x, f40, s53, o54, w34, thr=0x0040, w36=0x3636, wall=0x00000800):
    """0x354F0: the wall DS_000BE018, the threshold word 0xBDBEA (the word below it seeded), the
    side slot's x, +0x40/+0x41 and +0x53/+0x54, its record's +0x32..+0x37 (`w34` between a seeded
    word and `w36`), and the other slot's and record's same fields holding the other answer: an x
    inside the walls, the other +0x40 bit 6, the other +0x53 verdict, a +0x54 on the other side
    of 2 (the own slot's +0x54 is what @o54 reads) and a +0x34 of the other sign."""
    so, oo = D2A_SLOT + side * 0x94, D2A_SLOT + (1 - side) * 0x94
    rs, ro = (D2A_R0, D2A_R1)[side], (D2A_R0, D2A_R1)[1 - side]
    ws = (w34 ^ 0x8000) - 0x8000
    return {0x000BE018: le32(wall), 0x000BDBE8: le16(0x7FFF if thr >= 0x8000 else 0x8000) + le16(thr),
            so: le32(rs), oo: le32(ro),
            so + 0x2C: le32(x), oo + 0x2C: le32(0x00000010),
            so + 0x40: bytes([f40, 0x41]), oo + 0x40: bytes([f40 ^ 0x40, 0x61]),
            so + 0x53: bytes([s53, 2 if o54 != 2 else 0]),
            oo + 0x53: bytes([0x0B if s53 == 0x0A else 0x0A, o54]),
            rs + 0x32: le16(0x3232) + le16(w34) + le16(w36),
            ro + 0x32: le16(0x2232) + le16(0xFFFB if ws >= 0 else 0x0005) + le16(0x2636)}


def d2a_st20_case(side, cmd, cnt=0x0005, ch=2, scratch=False):
    """0x35E6C: both slots' record pointers, +0x40..+0x42, +0x51..+0x55, +0x7A, +0x82..+0x87 (the
    +0x84 counter between two seeded words) and +0x90..+0x93; both records' +0x18 and +0x51; the
    side's command word (the other side's has bits 8..11 complemented); the 0xC8B58 stream dword
    of `ch`, of the other slot's character and of ch sign-extended. `scratch`: EAX is D2A_FS and
    EDX is D2A_RX (+0x51 = side, another +0x18), both seeded like a slot and a record."""
    recs = (D2A_R0, D2A_R1)
    words = [0, 0]
    words[side], words[1 - side] = cmd, cmd ^ 0x0F00
    p = {0x001088E0: le16(words[0]) + le16(words[1])}
    blocks = [(D2A_SLOT, recs[0], 0), (D2A_SLOT + 0x94, recs[1], 1)]
    if scratch:
        blocks.append((D2A_FS, D2A_RX, 2))
    for so, rec, k in blocks:
        c = ch if k == side else (ch + 1 + k) & 0x7F
        p[so] = le32(rec)
        p[so + 0x40] = bytes([0x40 + k, 0x41 + k, 0x42 + k])
        p[so + 0x51] = bytes([0x51, 0x11 + k, 0x33 + k, 0x7F - k, 0x55])
        p[so + 0x7A] = bytes([c])
        p[so + 0x82] = le16(0x8282) + le16((cnt + k * 0x100) & 0xFFFF) + le16(0x8686)
        p[so + 0x90] = le16(0x9090) + le16(0x9292 + k)
        p[0x000C8B58 + c * 4] = le32(0x000E9000 + c)
    for k, rec in enumerate(recs):
        p[rec + 0x18] = le32(0x00181800 + k)
        p[rec + 0x51] = bytes([k])
    if scratch:
        p[D2A_RX + 0x18] = le32(0x00181877)
        p[D2A_RX + 0x51] = bytes([side])
    if ch >= 0x80:
        p[0x000C8B58 + (ch - 0x100) * 4] = le32(0x000EA000 + ch)
    return p


D2A_SPECS = [
    # 0x146F0 fighter_146f0: EDX = rec (EAX is overwritten unread, 0x146F8). 0x339AC, 0x14590 and
    # its 0x33950 run real (allowed): 0x14590 is no stub of any row, so the `test al,al` at 0x14707
    # is not separated from a whole-EAX test (its port returns 0 or 1; record §4). Seven stubs.
    # Mask 0: the one caller overwrites EAX at once (0x146E0 `mov eax,[esp+8]`).
    Spec("fighter_146f0", 0x146F0, d2a_step_cases(D2A_STEP_146F0),
         allow_calls=(0x339AC, 0x14590, 0x33950), calls=D2A_STEP_CALLS, eax_mask=0,
         mutants=D2A_STEP_MUTANTS + ("@twin",)),
    # 0x14A5C fighter_14a5c: 0x146F0's twin over 0xFD100/0xFD11A/0x9AFEA and the 0xD2FCC/0xD2F8A
    # streams; its 0x3C480 arm first stores ctx[2]'s +0x54 = 0 and +0x52 = 9 (0x14B0A, 0x14B12).
    # Mask 0: the one caller overwrites EAX at once (0x14A4C `mov eax,[esp+8]`).
    Spec("fighter_14a5c", 0x14A5C, d2a_step_cases(D2A_STEP_14A5C),
         allow_calls=(0x339AC, 0x14590, 0x33950), calls=D2A_STEP_CALLS, eax_mask=0,
         mutants=D2A_STEP_MUTANTS + ("@s54z", "@s52", "@late")),
    # 0x192DC fighter_192dc: EAX = side. 0x33950 runs real (allowed); 0x39278 runs real and is
    # recorded (D2A_39278), since one call and two leave the same DS_001088EC word. The compares at
    # 0x19307..0x19343 and 0x19377..0x1939E are dead: every path from them reaches the one call at
    # 0x193A0 (h5..h12 walk them). Mask 0: the caller returns at once (0x19585..0x1958A, in 0x193B0)
    # and that return ends in `xor eax,eax` or a load of EAX on every path (record d1b §1, 0x39278).
    Spec("fighter_192dc", 0x192DC, [
        Case("h0", {"eax": 0}, d2a_hold_case(0, 0x1F, 1, 0x0001)),
        Case("h1", {"eax": 1}, d2a_hold_case(1, 0x00, 1, 0x0002)),
        Case("h2", {"eax": 0}, d2a_hold_case(0, 0x1F, 1, 0xFFFF)),
        Case("h3", {"eax": 1}, d2a_hold_case(1, 0x20, 1, 0x0001)),
        Case("h4", {"eax": 0}, d2a_hold_case(0, 0x80, 1, 0x0000)),
        Case("h5", {"eax": 1}, d2a_hold_case(1, 0x05, 0, 0x0001, d=1, s54=(2, 2))),
        Case("h6", {"eax": 0}, d2a_hold_case(0, 0x05, 2, 0x0001, d=1, s54=(2, 3))),
        Case("h7", {"eax": 0}, d2a_hold_case(0, 0x05, 0, 0x0001, d=1, s54=(1, 1))),
        Case("h8", {"eax": 1}, d2a_hold_case(1, 0x05, 0, 0x0001, d=2)),
        Case("h9", {"eax": 0}, d2a_hold_case(0, 0x30, 0, 0x0001, d=1, s54=(2, 2))),
        Case("h10", {"eax": 1}, d2a_hold_case(1, 0x30, 0, 0x0001, d=1, s54=(2, 5))),
        Case("h11", {"eax": 0}, d2a_hold_case(0, 0x30, 0, 0x0001, d=1, s54=(3, 3))),
        Case("h12", {"eax": 1}, d2a_hold_case(1, 0x30, 0xFF, 0x0001, d=2)),
    ], allow_calls=(0x33950,), calls=(D2A_39278,), eax_mask=0,
       mutants=("@sslot", "@sext", "@st", "@b20", "@o53", "@tab", "@word", "@usign", "@bound", "@arg",
                "@once", "@v53")),
    # 0x235C4 fighter_235c4: EAX = side (EDX is pushed and overwritten, 0x235C9). 0x33A10 (the
    # stack ctx), 0x33ACC (the two `rep movsd` copies) and 0x22B28 (the stack ctx is its argument)
    # run real; 0x39834 and 0x22B28's four callees are stubs. Mask 0: the one caller overwrites EAX
    # (0x3B663 `jmp 0x3b6ac`, 0x3B6AC `mov eax,[esp+0xc]`).
    Spec("fighter_235c4", 0x235C4, [
        Case("s0", {"eax": 0, "edx": 0x22222222}, d2a_snap_case(0, 0x0A)),
        Case("s1", {"eax": 1, "edx": 0x22222222}, d2a_snap_case(1, 0x0B)),
        Case("s2", {"eax": 1, "edx": 0x22222222}, d2a_snap_case(1, 0x0A, idx=0x0102), {0x2C3FC: 0}),
        Case("s3", {"eax": 0, "edx": 0x22222222}, d2a_snap_case(0, 0x00, idx=7)),
    ], allow_calls=(0x33A10, 0x33ACC, 0x22B28), calls=(POSE, P6_13C70, CLEAR36, CLEAR34, VOICE),
       eax_mask=0,
       mutants=("@side", "@dst", "@scale", "@nosnap", "@react", "@pside", "@s52", "@s53", "@cb",
                "@x18", "@x1c", "@slot", "@late", "@order", "@no22", "@ctx")),
    # 0x35B7C fighter_state_35b7c: EAX = slot, EDX = rec. 0x35C1C and 0x35D20 run real (neither is a
    # stub of any row), so the `test eax,eax` at 0x35C05 is not separated from an AL test (0x35C1C
    # returns 0 or 1; record §4); their 0x2BCF4 and 0x2BC30 are the stubs. n7 is the 16-bit
    # difference: the 0xBDA3E word 0x8000 against frame 0x7F (0x35BBC `sub edi,eax`, 0x35BC5 `cwd`,
    # 0x35BC7 `idiv si`; record §3). Mask 0: three sites return through 0x359E0 and one through
    # 0x37464 into the dispatcher, whose one call site overwrites EAX (0x35801 `mov eax,esi`);
    # 0x36F7E `mov eax,[ebx]` / 0x37019 `xor eax,eax`; 0x383F6.. returns to 0x38569 `mov eax,ecx`.
    Spec("fighter_state_35b7c", 0x35B7C, [
        Case("n0", {"eax": D2A_FS, "edx": D2A_FR}, d2a_enter_case(2, 0x0006, 0x01, 0x05)),
        Case("n1", {"eax": D2A_FS, "edx": D2A_FR}, d2a_enter_case(3, 0x0006, 0x03, 0x0D, s43=0x00)),
        Case("n2", {"eax": D2A_FS, "edx": D2A_FR}, d2a_enter_case(0, 0x0006, 0x05, 0x0D, s43=0xFF, s54=0x03)),
        Case("n3", {"eax": D2A_FS, "edx": D2A_FR}, d2a_enter_case(6, 0x0006, 0xFA, 0x07)),
        Case("n4", {"eax": D2A_FS, "edx": D2A_FR}, d2a_enter_case(1, 0x000C, 0x03, 0x05)),
        Case("n5", {"eax": D2A_FS, "edx": D2A_FR}, d2a_enter_case(5, 0xFFFA, 0xFD, 0x05, s43=0x00)),
        Case("n6", {"eax": D2A_FS, "edx": D2A_FR}, d2a_enter_case(4, 0xFFF9, 0xFC, 0x05)),
        Case("n7", {"eax": D2A_FS, "edx": D2A_FR}, d2a_enter_case(2, 0x8000, 0x7F, 0x05)),
        Case("n8", {"eax": D2A_FS, "edx": D2A_FR}, d2a_enter_case(3, 0x0106, 0x04, 0x05)),
    ], allow_calls=(0x35C1C, 0x35D20), calls=(P6_2BCF4, ANIM_BEGIN), eax_mask=0,
       mutants=("@s52", "@s53a", "@s53k", "@idx", "@maxb", "@maxz", "@floor", "@fz", "@cmp", "@neg",
                "@f2z", "@w32", "@gate", "@r", "@div")),
    # 0x36430 fighter_state_36430: EAX = slot, EDX = rec, EBX = side. 0x1A640 runs real (no stub of
    # any row; `test eax,eax` 0x364D7 on its 0/0x1000/0x2000); 0x365C8 and 0x36638 are stubs whose
    # AL the raw tests (0x3643E, 0x36455: k2 returns 0x100 from both), 0x3BDDC a stub whose result
    # is unread, 0x2BC30 the anim-begin. k6/k7 pass a scratch slot in EAX: the bit-2 test at 0x36499
    # reads the side's own slot (0x3648B..0x36497 scale ESI). Mask 0: the caller returns at once
    # (0x34C6D..0x34C72, the dispatcher) and its one call site overwrites EAX (0x35801).
    Spec("fighter_state_36430", 0x36430, d2a_handler_cases(), allow_calls=(0x1A640,),
         calls=D2A_HANDLER_CALLS, eax_mask=0, mutants=D2A_HANDLER_MUTANTS),
    # 0x364FC fighter_state_364fc: the twin; the 0x4000 arm runs on 0x1A640 == 0 (0x365A5 `jne`),
    # with +0x52 = 5 and the 0xC8FB8 stream at 2.0. Mask 0: 0x34D83..0x34D88, then 0x35801.
    Spec("fighter_state_364fc", 0x364FC, d2a_handler_cases(), allow_calls=(0x1A640,),
         calls=D2A_HANDLER_CALLS, eax_mask=0, mutants=D2A_HANDLER_MUTANTS),
    # 0x39B30 fighter_39b30: EAX = side. 0x33A10 (the stack ctx), 0x39AC8 and 0x39B14 run real; six
    # stubs (0x3C148, 0x3C16C, 0x3C520, 0x3C480, 0x1890C, 0x3C190). The hold is x87: `fild
    # [ch*4+0xbed10]; fild [side*4+0x107a60]; fdivrp; fstp dword` 0x39B64..0x39B7A, the dividend the
    # whole dword (q1: the word above n is 3). The divisor dword is never 0 here (0/0 is the x87's
    # negative default NaN, the host's is not; record §4), nor n (0x39AC8's #DE) nor (s16)(n + m)
    # (0x39C6D `idiv ebx`). Mask 0: the one caller overwrites EAX (0x39D10 `mov eax,[esp+0xc]`).
    Spec("fighter_39b30", 0x39B30, [
        Case("q0", {"eax": 0}, d2a_launch_case(0, 2, 2, 0x00000080, 0x00, 0x0010, 0x0040, 0x0010, 0x0100, 5, 0x0100)),
        Case("q1", {"eax": 1}, d2a_launch_case(1, 3, 0, 0x00000100, 0x02, 0x0008, 0xFFC0, 0x0004, 0x0100, 7, 0x0100,
                                               npad=3)),
        Case("q2", {"eax": 0}, d2a_launch_case(0, 0, 2, 0x80000000, 0xFF, 0xFFF0, 0x7FFF, 0x0011, 0xFF00, 7, 0x8000,
                                               npad=0xFFFF)),
        Case("q3", {"eax": 1}, d2a_launch_case(1, 6, 1, 0x7FFFFFFF, 0x03, 0x7FFF, 0x7FFF, 0x0002, 0x7FFF, 4, 0x0100)),
        Case("q4", {"eax": 0}, d2a_launch_case(0, 0x80, 0x82, 0x000000FF, 0x01, 0x0010, 0x8000, 0x0010, 0x0200, 6,
                                               0x0100)),
        Case("q5", {"eax": 1}, d2a_launch_case(1, 5, 2, 0xFFFFFF00, 0x80, 0x0003, 0x0001, 0x0001, 0x0001, 8, 0x0100,
                                               och=0)),
    ], allow_calls=(0x33A10, 0x39AC8, 0x39B14), calls=(CLEAR34, CLEAR36, D2A_HIT_C, HIT_A, P6_1890C, SPEED),
       eax_mask=0,
       mutants=("@clr", "@clr36", "@corder", "@nword", "@hinv", "@chslot", "@csext", "@s54", "@arm", "@noy",
                "@gz", "@gcmp", "@gy", "@gu", "@an", "@g44", "@w36", "@vm", "@hz", "@den32", "@q32", "@hsh",
                "@r63", "@or41", "@inc", "@b3", "@cs", "@w74", "@side", "@rec", "@early")),
    # 0x39FF4 fighter_39ff4: no argument; side 0 then side 1 (EDI, ESI = side * 0x94). 0x34038 (its
    # 0x2B150 and 0x29BC8 are stubs) and 0x1922C (with its 0x33950) run real; 0x2BD44 and 0x39F40
    # are stubs. Mask 0: 0x28003 `call 0x27ed8`, whose first use of EAX is a write (0x27EE6 `mov
    # eax,edx`), and 0x28069 `xor eax,eax`.
    Spec("fighter_39ff4", 0x39FF4, [
        Case("f0", {}, d2a_freeze_case(0x00010000, 0, (0x0010A500, 0x0010A540),
                                       (3, 1, 0xFFFFFFFF, 0x0005, 0x0000FC00), (0, 0, 0x00000000, 0x0000, 0x00011234))),
        Case("f1", {}, d2a_freeze_case(0x00020000, 1, (0x0010A500, 0),
                                       (2, 0, 0x04081000, 0x8000, 0x0001E800), (0x80, 1, 0x12345678, 0x0001, 0x00022000))),
        Case("f2", {}, d2a_freeze_case(0x80000000, 1, (0, 0x0010A540),
                                       (1, 0x80, 0xFBF7EFFF, 0x7FFF, 0x00000000), (0, 0, 0x04080000, 0xFFFF, 0x7FFFE7FF))),
        Case("f3", {}, d2a_freeze_case(0x00030000, 0, (0x0010A500, 0x0010A540),
                                       (0x80, 0, 0x00001000, 0x0100, 0x00030000), (5, 1, 0x80000000, 0x0002, 0x0002FFC1))),
    ], allow_calls=(0x34038, 0x1922C, 0x33950), calls=(D2A_2BD44, P7_2B150, P6_29BC8, P6_39F40), eax_mask=0,
       mutants=("@child0", "@csext", "@row", "@flag", "@bd", "@c4b", "@f2a", "@x0c", "@x18", "@x1c", "@s5d",
                "@mask1", "@mask2", "@or41", "@t", "@tb", "@t1", "@tbyte", "@no34", "@side34", "@no19",
                "@order19", "@abs", "@k", "@div", "@noneg", "@q32", "@p64", "@p14", "@one", "@rev")),
    # 0x3BAEC fighter_3baec: AX = pen (`movsx ebx,ax` 0x3BB37/0x3BB4C). 0x3B9D8 (with its 0x33950)
    # runs real: it is no stub of any row, and its own row truncates the pen again, so a pen passed
    # unextended is not separated here (record §4). Its 0x3B8D8 is a stub; its 0x1883C is the stub
    # that clears both +0x41 bytes (D2A_1883C_CLR). Mask 0: 0x3BC61 `mov al,1` and a return from
    # 0x3BB90, whose sites are 0x2550E `mov eax,1` and 0x25422 `call 0x12d48` (record §1).
    Spec("fighter_3baec", 0x3BAEC, [
        Case("g0", {"eax": 0x0011}, d2a_push_case((0x00048000, 0x0000FF00), (0x900, 0x700))),
        Case("g1", {"eax": 0x0011}, d2a_push_case((0x00008000, 0x00048000), (0x900, 0x700))),
        Case("g2", {"eax": 0x0011}, d2a_push_case((0x0200FF00, 0x01000000), (0x900, 0x700)), {0x3B8D8: 0}),
        Case("g3", {"eax": 0x0011}, d2a_push_case((0x02008000, 0x02008000), (0x700, 0x900))),
        Case("g4", {"eax": 0x0020}, d2a_push_case((0x00007F00, 0x0200FF00), (0x700, 0x900)), {0x3B8D8: 1}),
        Case("g5", {"eax": 0xFFEF}, d2a_push_case((0x01008000, 0x00000000), (0x900, 0x700), s54=(2, 2)),
             {0x3B8D8: 1}),
        Case("g6", {"eax": 0x00018010}, d2a_push_case((0x00000000, 0x01008100), (0x300, 0x700)), {0x3B8D8: 0x100}),
        Case("g7", {"eax": 0x0011}, d2a_push_case((0x00200000, 0x00208000), (0x900, 0x700))),
    ], allow_calls=(0x3B9D8, 0x33950), calls=(D1B_3B8D8, D2A_1883C_CLR), eax_mask=0,
       mutants=("@gate0", "@gate1", "@gbit", "@order", "@rorder", "@o1", "@o1inv", "@keep0", "@keep1",
                "@kswap", "@klate", "@kmask", "@one", "@same")),
    # 0x3C758 hit_connect: EAX = side, EDX = facing, EBX = i, the one stack argument (`ret 4`) the
    # address of the caller's accumulator dword (`mov edx,[ecx]; or edx,eax; mov [ecx],edx; cmp
    # ebx,edx` 0x3C7D4..0x3C7DA: v7 sets its upper half). The phase is sign-extended (`sar ebx,
    # 0x10` 0x3C774: v8 is -1). 0x3C600 is a stub returning D2A_DESC; 0x3C6E8 runs real (v9: its
    # facing argument picks the mapped bit). Full mask: 0x3C956/0x3C9D3 `test eax,eax`, 0x3CAD7
    # `cmp eax,3`.
    Spec("hit_connect", 0x3C758, [
        Case("v0", {"eax": 0, "edx": 0, "ebx": 3, "ecx": 0x0C0C0C0C, "s0": D2A_OUT},
             d2a_connect_case(0, 3, 0, (0x0001, 0x0002, 0x0040), 0x0040, 0), {0x3C600: D2A_DESC}),
        Case("v1", {"eax": 1, "edx": 0, "ebx": 5, "ecx": 0x0C0C0C0C, "s0": D2A_OUT},
             d2a_connect_case(1, 5, 2, (0x0001, 0x0002, 0x0040), 0x0002, 0), {0x3C600: D2A_DESC}),
        Case("v2", {"eax": 0, "edx": 0, "ebx": 0, "ecx": 0x0C0C0C0C, "s0": D2A_OUT},
             d2a_connect_case(0, 0, 1, (0x0001, 0x0002, 0x0040), 0x0043, 0), {0x3C600: D2A_DESC}),
        Case("v3", {"eax": 1, "edx": 0, "ebx": 0x1F, "ecx": 0x0C0C0C0C, "s0": D2A_OUT},
             d2a_connect_case(1, 0x1F, 7, (0x0030, 0x0100, 0x0200), 0x0C30, 0), {0x3C600: D2A_DESC}),
        Case("v4", {"eax": 0, "edx": 0, "ebx": 4, "ecx": 0x0C0C0C0C, "s0": D2A_OUT},
             d2a_connect_case(0, 4, 3, (0x0030, 0x0100, 0x0200), 0x0010, 0), {0x3C600: D2A_DESC}),
        Case("v5", {"eax": 1, "edx": 0, "ebx": 4, "ecx": 0x0C0C0C0C, "s0": D2A_OUT},
             d2a_connect_case(1, 4, 3, (0x0030, 0x0100, 0x0200), 0x0010, 0x0020), {0x3C600: D2A_DESC}),
        Case("v6", {"eax": 0, "edx": 0, "ebx": 4, "ecx": 0x0C0C0C0C, "s0": D2A_OUT},
             d2a_connect_case(0, 4, 3, (0x0030, 0x0100, 0x0200), 0x0C00, 0), {0x3C600: D2A_DESC}),
        Case("v7", {"eax": 1, "edx": 0, "ebx": 4, "ecx": 0x0C0C0C0C, "s0": D2A_OUT},
             d2a_connect_case(1, 4, 3, (0x0030, 0x0100, 0x0200), 0x0010, 0x00010020), {0x3C600: D2A_DESC}),
        Case("v8", {"eax": 0, "edx": 0, "ebx": 6, "ecx": 0x0C0C0C0C, "s0": D2A_OUT},
             d2a_connect_case(0, 6, 0xFFFF, (0x0001, 0x0002, 0x0040), 0x0040, 0), {0x3C600: D2A_DESC}),
        Case("v9", {"eax": 1, "edx": 1, "ebx": 2, "ecx": 0x0C0C0C0C, "s0": D2A_OUT},
             d2a_connect_case(1, 2, 4, (0x0001, 0x0002, 0x00010000), 0x0020, 0), {0x3C600: D2A_DESC}),
    ], allow_calls=(0x3C6E8,), calls=(D2A_DESC_CALL,), eax_mask=0xFFFFFFFF,
       mutants=("@di", "@off", "@pz", "@stride", "@f", "@m01", "@m12", "@cmd", "@order", "@ow", "@nostore",
                "@noacc", "@eq", "@r3")),
    # 0x3C800 hit_displace: EAX = side, EDX = facing, EBX = i, two stack arguments (`ret 8`): the
    # second ([esp+0x10] after the two pushes) is the address the displacement goes to, a dword
    # (`mov eax,[ecx+eax*4+0xa]; sar eax,0x10; mov [edx],eax` 0x3C85E..0x3C869: w1 is negative); the
    # first is unread. AL only: `mov al,1` 0x3C86B / `xor al,al` 0x3C872, and the one caller tests
    # AL (0x3CA99 `test al,al`).
    Spec("hit_displace", 0x3C800, [
        Case("w0", {"eax": 0, "edx": 0, "ebx": 2, "s0": D2A_OUT, "s1": D2A_OUT2},
             d2a_displace_case(0, 2, 3, 0x0040, 0x0123, 0x0040), {0x3C600: D2A_DESC}),
        Case("w1", {"eax": 1, "edx": 0, "ebx": 5, "s0": D2A_OUT, "s1": D2A_OUT2},
             d2a_displace_case(1, 5, 2, 0x0300, 0x8001, 0x0100), {0x3C600: D2A_DESC}),
        Case("w2", {"eax": 0, "edx": 0, "ebx": 0, "s0": D2A_OUT, "s1": D2A_OUT2},
             d2a_displace_case(0, 0, 0, 0x0040, 0x0123, 0xFFBF), {0x3C600: D2A_DESC}),
        Case("w3", {"eax": 1, "edx": 0, "ebx": 6, "s0": D2A_OUT, "s1": D2A_OUT2},
             d2a_displace_case(1, 6, 0xFFFF, 0x0040, 0x0456, 0x0040), {0x3C600: D2A_DESC}),
        Case("w4", {"eax": 0, "edx": 1, "ebx": 2, "s0": D2A_OUT, "s1": D2A_OUT2},
             d2a_displace_case(0, 2, 4, 0x00010000, 0x7FFF, 0x0020), {0x3C600: D2A_DESC}),
        Case("w5", {"eax": 1, "edx": 0, "ebx": 0x1F, "s0": D2A_OUT, "s1": D2A_OUT2},
             d2a_displace_case(1, 0x1F, 7, 0x0040, 0x0123, 0x0000, m2=0x00400040), {0x3C600: D2A_DESC}),
    ], allow_calls=(0x3C6E8,), calls=(D2A_DESC_CALL,), eax_mask=0xFF,
       mutants=("@di", "@pz", "@stride", "@f", "@cmd", "@off10", "@dlo", "@dz", "@dw", "@nostore", "@ret",
                "@s0", "@off")),
    # 0x467DC ai_pred_467dc: EAX = side. +0x54 == 0, +0x53 == 0, +0x52 == 1 (0x467F2, 0x467FD,
    # 0x4680B), then 0x1A5D4 (run real; `test eax,eax` 0x4681C on its 0/0x1000/0x2000, whose AL is
    # always 0: @al) must be zero. AL only: `mov al,cl` 0x46825 / `xor al,al`; the one caller tests
    # AL (0x46A85 `test al,al`).
    Spec("ai_pred_467dc", 0x467DC, d2a_pred_cases(), allow_calls=(0x1A5D4,), eax_mask=0xFF,
         mutants=D2A_PRED_MUTANTS),
    # 0x4682C ai_pred_4682c: the twin; 0x1A5D4 must be non-zero (0x4686E `jne`). AL only; 0x46AA5
    # `test al,al`.
    Spec("ai_pred_4682c", 0x4682C, d2a_pred_cases(), allow_calls=(0x1A5D4,), eax_mask=0xFF,
         mutants=D2A_PRED_MUTANTS),
    # 0x461DC fighter_input_ring_update: no argument; side 0 then side 1, then the ring store. 0x1A570
    # is a stub whose AL the raw compares with 1 (0x46238 `and eax,0xff`, 0x4623D `cmp eax,1`);
    # 0x3C6E8 runs real and is an identity on the zero-extended word it is given, so neither its
    # call nor the AL compare is observable (record §4). The ring index is the signed word (0x46420
    # `mov eax,[0x1082d2]`, 0x46425 `sar eax,0x10`: r4 is -2). Mask 0: both arms after the call
    # reach `call 0x38990` (0x24CC3/0x24CC8), whose first uses of EAX are writes (0x38991, 0x3899F).
    Spec("fighter_input_ring_update", 0x461DC, [
        Case("r0", {}, d2a_ring_case((4, 4), (0, 0), (0x0001, 0x0111), 0x0005), {0x1A570: 1}),
        Case("r1", {}, d2a_ring_case((4, 4), (0, 0), (0x1001, 0x0121), 0x0013), {0x1A570: 0}),
        Case("r2", {}, d2a_ring_case((4, 4), (0, 0), (0x2001, 0x0004), 0x0012), {0x1A570: 0x101}),
        Case("r3", {}, d2a_ring_case((4, 4), (0, 0), (0x0414, 0x1004), 0xFFFF), {0x1A570: 2}),
        Case("r4", {}, d2a_ring_case((4, 4), (0, 0), (0x1030, 0x0424), 0xFFFE), {0x1A570: 0x100}),
        Case("r5", {}, d2a_ring_case((4, 4), (0, 0), (0x2004, 0x0015), 0x0000), {0x1A570: 1}),
        Case("r6", {}, d2a_ring_case((4, 5), (1, 0), (0x0111, 0x0424), 0x0001), {0x1A570: 1}),
        Case("r7", {}, d2a_ring_case((0x0104, 4), (0, 0x80), (0x0111, 0x0111), 0x0002), {0x1A570: 1}),
        Case("r8", {}, d2a_ring_case((4, 0), (0, 0), (0x0111, 0x0111), 0x0003), {0x1A570: 0}),
    ], allow_calls=(0x3C6E8,), calls=(BIT15,), eax_mask=0, unhit_named={
        0x463FB: "the fourth toggle tests the low byte's bit 5 again (0x463EC `mov ax,[esp+ebx]`, 0x463F2 `and "
                 "al,0x20`), which 0x463E8 has just cleared whenever it was set"},
       mutants=("@s63", "@s63slot", "@selside", "@selb", "@t4", "@b101", "@b202", "@c2", "@c100", "@e404",
                "@e808", "@f8", "@f400", "@flag", "@t1", "@t2", "@t3", "@prio", "@pz", "@wrap", "@d4",
                "@swap", "@m1010")),
    # 0x46C78 ai_state_setup: EAX = side. The state dword (the side's AI block +0x28) picks one of
    # the 14 targets of the table 0x46C40; 0x33950 and 0x46BBC run real (0x46BBC is no stub of any
    # row: its three stores carry the id). State 0xA and every state above 0xD reach 0x46D97, the
    # raw's 0x62003 fatal, which the port skips: named unhit. Mask 0: the one caller overwrites EAX
    # (0x46FE8 `mov eax,edx`).
    Spec("ai_state_setup", 0x46C78, [
        Case("t0", {"eax": 0}, d2a_state_case(0, 0, 0x05)),
        Case("t0b", {"eax": 1}, d2a_state_case(1, 0, 0x80)),
        Case("t1", {"eax": 1}, d2a_state_case(1, 1, 0x7F)),
        Case("t1b", {"eax": 0}, d2a_state_case(0, 1, 0x7F, s8a=0x80)),
        Case("t1c", {"eax": 1}, d2a_state_case(1, 1, 0xFF)),
        Case("t2", {"eax": 0}, d2a_state_case(0, 2, 0x05)),
        Case("t3", {"eax": 1}, d2a_state_case(1, 3, 0x05)),
        Case("t4", {"eax": 0}, d2a_state_case(0, 4, 0x05)),
        Case("t5", {"eax": 1}, d2a_state_case(1, 5, 0x05)),
        Case("t6", {"eax": 0}, d2a_state_case(0, 6, 0x05)),
        Case("t7", {"eax": 1}, d2a_state_case(1, 7, 0x05)),
        Case("t8", {"eax": 0}, d2a_state_case(0, 8, 0x05)),
        Case("t9", {"eax": 1}, d2a_state_case(1, 9, 0x05)),
        Case("tb", {"eax": 1}, d2a_state_case(1, 0xB, 0x05, s64=0x23)),
        Case("tbf", {"eax": 0}, d2a_state_case(0, 0xB, 0x05, s64=0xFF)),
        Case("tb0", {"eax": 0}, d2a_state_case(0, 0xB, 0x05, s64=0x00)),
        Case("tc", {"eax": 0}, d2a_state_case(0, 0xC, 0x05)),
        Case("td", {"eax": 1}, d2a_state_case(1, 0xD, 0x05)),
    ], allow_calls=(0x33950, 0x46BBC), eax_mask=0, unhit_named={
        0x46D97: "state 0xA or above 0xD: the raw calls the 0x62003 fatal; the port skips the weight store"},
       mutants=("@slot", "@bside", "@s8a", "@s40", "@wrap", "@s64", "@nostate", "@stbyte", "@sext", "@id2",
                "@wside")),
    # 0x354F0 fighter_wall_clamp: EAX = side. Five stubs. 0x187FC is called twice on the push path
    # (0x3558E then 0x35597 or 0x355A0; 0x3560C then 0x35615 or 0x3561E): once for the sign, once
    # for the value. Its EAX is the distance (w4, w8: negative), compared with the signed word
    # 0xBDBEA (`sar edx,0x10` 0x355AB/0x35629: w6 is 0x8000). Mask 0: 0x35821 `mov eax,[esp]`, and
    # 0x35829 `call 0x186c4`, whose first instruction is `xor eax,eax`.
    Spec("fighter_wall_clamp", 0x354F0, [
        Case("w0", {"eax": 0}, d2a_wall_case(0, 0x00000900, 0x41, 0x0A, 0, 0x0005)),
        Case("w1", {"eax": 0}, d2a_wall_case(0, 0x00000900, 0x80, 0x0A, 0, 0x0005), {0x187FC: 0x00000010}),
        Case("w2", {"eax": 1}, d2a_wall_case(1, 0x00000801, 0x00, 0x0B, 0, 0x0000)),
        Case("w3", {"eax": 0}, d2a_wall_case(0, 0x7FFFFF00, 0xBF, 0x0A, 2, 0x8000)),
        Case("w4", {"eax": 1}, d2a_wall_case(1, 0x00000900, 0x00, 0x0A, 1, 0x0001, w36=0xFFFF),
             {0x187FC: 0xFFFFFFF0}),
        Case("w5", {"eax": 0}, d2a_wall_case(0, 0x00000900, 0x00, 0x0A, 0, 0x0000, w36=0x0001),
             {0x187FC: 0x00000040}),
        Case("w6", {"eax": 1}, d2a_wall_case(1, 0x00000900, 0x00, 0x0A, 0, 0x7FFF, thr=0x8000),
             {0x187FC: 0x00000005}),
        Case("w7", {"eax": 0}, d2a_wall_case(0, 0x00000800, 0x00, 0x0A, 0, 0x0005), {0x187FC: 0x00000010}),
        Case("w8", {"eax": 1}, d2a_wall_case(1, 0xFFFFF800, 0x00, 0x0A, 0, 0xFFFB), {0x187FC: 0x00000010}),
        Case("w9", {"eax": 0}, d2a_wall_case(0, 0xFFFFF700, 0x00, 0x0A, 0, 0xFFFB), {0x187FC: 0x00000010}),
        Case("w10", {"eax": 1}, d2a_wall_case(1, 0xFFFFF7FF, 0x00, 0x0A, 3, 0x0005), {0x187FC: 0xFFFFFFC1}),
        Case("w11", {"eax": 0}, d2a_wall_case(0, 0x80000100, 0x00, 0x09, 0, 0x0000, w36=0x8000)),
        Case("w12", {"eax": 1}, d2a_wall_case(1, 0xFFFFF700, 0x00, 0x0A, 2, 0x8000)),
        Case("w13", {"eax": 0}, d2a_wall_case(0, 0xFFFFF700, 0x00, 0x0A, 0, 0x0000), {0x187FC: 0x0000003F}),
        Case("w14", {"eax": 0}, d2a_wall_case(0, 0x00000900, 0x00, 0x0A, 0, 0x0005), {0x187FC: 0xFFFFFF00}),
        Case("w15", {"eax": 1}, d2a_wall_case(1, 0xFFFFF700, 0x00, 0x0A, 0, 0xFFFB), {0x187FC: 0xFFFFFFC0}),
    ], calls=(ANCHORX, C1_AI_DIST, C1_1883C, CLEAR34), eax_mask=0,
       mutants=("@b40", "@x", "@cmp", "@ov", "@a53", "@o54", "@v54", "@one", "@abs", "@thrlo", "@thrz",
                "@tcmp", "@pside", "@pneg", "@rec", "@w34", "@c34", "@c148", "@lret", "@lanchor", "@early")),
    # 0x35E6C fighter_state_35e6c: EAX = slot, EDX = rec. The EAX slot only takes the +0x41 bit
    # (0x35ED0 `or byte [ebx+0x41],0x80`); the side is the record's +0x51 and everything else runs
    # on that side's own slot and its record (0x35F2B `mov eax,[esp+8]`, 0x35F58 and 0x35F6D `mov
    # ..,[esp+0x10]`): b5/b6 pass a scratch slot and a third record. 0x3BDDC is a stub whose AL the
    # raw tests (0x35F1A: b3 returns 0x100). Five stubs. Mask 0: the caller returns at once
    # (0x34D72..0x34D77, the dispatcher) and its one call site overwrites EAX (0x35801).
    Spec("fighter_state_35e6c", 0x35E6C, [
        Case("b0", {"eax": D2A_SLOT, "edx": D2A_R0}, d2a_st20_case(0, 0x0000), {0x3BDDC: 1}),
        Case("b1", {"eax": D2A_SLOT + 0x94, "edx": D2A_R1}, d2a_st20_case(1, 0x00FF), {0x3BDDC: 0}),
        Case("b2", {"eax": D2A_SLOT, "edx": D2A_R0}, d2a_st20_case(0, 0x0500, cnt=0x00FF)),
        Case("b3", {"eax": D2A_SLOT + 0x94, "edx": D2A_R1}, d2a_st20_case(1, 0x0300), {0x3BDDC: 0x100}),
        Case("b4", {"eax": D2A_SLOT, "edx": D2A_R0}, d2a_st20_case(0, 0x0C00), {0x3BDDC: 0xFFFFFF01}),
        Case("b5", {"eax": D2A_FS, "edx": D2A_RX}, d2a_st20_case(1, 0x0A00, scratch=True)),
        Case("b6", {"eax": D2A_FS, "edx": D2A_RX}, d2a_st20_case(0, 0x0800, scratch=True), {0x3BDDC: 1}),
        Case("b7", {"eax": D2A_SLOT, "edx": D2A_R0}, d2a_st20_case(0, 0x0F00, cnt=0xFFFF, ch=0x80)),
    ], calls=(VOICE, D2A_CONSUME, C1_FACING, HIT_A, ANCHOR), eax_mask=0,
       mutants=("@al", "@slot", "@rec", "@x18", "@or41", "@voice", "@cmd", "@b3", "@bc", "@band", "@cside",
                "@fside", "@nofacing", "@s54", "@s52", "@s53", "@inc", "@incb", "@w92", "@csext", "@hold",
                "@aside", "@order")),
]
D1_SPECS += D2A_SPECS

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
] + E3_SPECS + P1_SPECS + P1_ANIM_SPECS + P2_SPECS + P3_SPECS + P6_SPECS + P45_SPECS + C1_SPECS + C2_SPECS + C2B_SPECS + C3_SPECS + C3B_SPECS + P7_SPECS + P8_SPECS + C3C_SPECS + C3D_SPECS + C3E_SPECS + C3F_SPECS + C3G_SPECS + D1_SPECS


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
