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
        Case("p0", {"eax": E3_SLOT, "edx": 0xDEAD, "ebx": 0},
             {**SLOT_PTRS, E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
              DS_SLOTS + 0x58: b"\x00", DS_SLOTS + 0x59: b"\x59", DS_SLOTS + 0x7A: b"\x7a",
              DS_SLOTS + 0x7B: b"\x7b", DS_SLOTS + 0x90: b"\x90", DS_SLOTS + 0x91: b"\x91",
              DS_SLOTS + 0x94 + 0x58: b"\x58", DS_SLOTS + 0x94 + 0x7A: b"\x6a",
              DS_SLOTS + 0x94 + 0x90: b"\x70", 0x00107CF8: le32(0x43214321),
              0x00107CFC: le32(0x00030003)}),
        Case("p2", {"eax": E3_SLOT, "edx": 0xDEAD, "ebx": 1},
             {**SLOT_PTRS, E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
              E3_REC + 0x18: le32(0x18181818), E3_REC + 0x1C: le32(0x1C1C1C1C),
              DS_SLOTS + 0x58: b"\x58", DS_SLOTS + 0x7A: b"\x7a", DS_SLOTS + 0x90: b"\x90",
              DS_SLOTS + 0x94 + 0x58: b"\x02", DS_SLOTS + 0x94 + 0x59: b"\x59",
              DS_SLOTS + 0x94 + 0x7A: b"\x6a", DS_SLOTS + 0x94 + 0x90: b"\x70",
              0x00107CF8: le32(0x43214321), 0x00107CFC: le32(0x00030003)}),
        Case("s0", {"eax": E3_SLOT, "edx": 0xDEAD, "ebx": 0},
             {**SLOT_PTRS, E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
              E3_REC + 0x18: le32(0x18181818), E3_REC + 0x1C: le32(0x1C1C1C1C),
              DS_SLOTS + 0x58: b"\x01", DS_SLOTS + 0x7A: b"\x00", DS_SLOTS + 0x90: b"\x00",
              DS_SLOTS + 0x94 + 0x58: b"\x58", DS_SLOTS + 0x94 + 0x7A: b"\x6a",
              DS_SLOTS + 0x94 + 0x90: b"\x70", 0x00107CF8: le32(0x00004321),
              0x00107CFC: le32(0x00000003)}),
        Case("s1", {"eax": E3_SLOT, "edx": 0xDEAD, "ebx": 1},
             {**SLOT_PTRS, E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
              E3_REC2 + 0x18: le32(0x28282828), E3_REC2 + 0x1C: le32(0x2C2C2C2C),
              DS_SLOTS + 0x58: b"\x58", DS_SLOTS + 0x7A: b"\x7a", DS_SLOTS + 0x90: b"\x90",
              DS_SLOTS + 0x94 + 0x58: b"\x01", DS_SLOTS + 0x94 + 0x7A: b"\x01",
              DS_SLOTS + 0x94 + 0x90: b"\x04", DS_SLOTS + 0x94 + 0x91: b"\x91",
              0x00107CFA: le32(0x00004322), 0x00107CFE: le32(0x00000003)}),
        Case("q0", {"eax": E3_SLOT, "edx": 0xDEAD, "ebx": 0},
             {**SLOT_PTRS, E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
              E3_REC + 0x18: le32(0x18181818), E3_REC + 0x1C: le32(0x1C1C1C1C),
              DS_SLOTS + 0x58: b"\x01", DS_SLOTS + 0x7A: b"\x00", DS_SLOTS + 0x90: b"\x00",
              0x00107CF8: le32(0x00004321), 0x00107CFC: le32(0x00000000)}),
        Case("q5", {"eax": E3_SLOT, "edx": 0xDEAD, "ebx": 0},
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
    p[so + 0x0C] = le32(0x33333333) + le32(0x44444444) + le32(0x55555555) + le32(0x66666666)
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
    # 0x3B134 fight_command_map: EAX = side, EDX = edx_arg, ECX = override. 0x33A10/0x3AFC4/
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
        Case("p3", {"eax": 0x10AF00, "edx": 0x10AA00}, c3d_28_case(0x2400)),
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
] + E3_SPECS + P1_SPECS + P1_ANIM_SPECS + P2_SPECS + P3_SPECS + P6_SPECS + P45_SPECS + C1_SPECS + C2_SPECS + C2B_SPECS + C3_SPECS + C3B_SPECS + P7_SPECS + P8_SPECS + C3C_SPECS + C3D_SPECS


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
