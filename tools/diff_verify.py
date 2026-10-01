#!/usr/bin/env python3
"""Differential verification driver (spec 2026-09-30-reverse-completion-design §5).

For each function in SPECS: run the original bytes (tools/diff_emu.py) and the port's C function
(build/diffrun, port/tests/diff_runner.c) from the same image with the same registers and pokes,
and compare the output register, every changed byte, and the block coverage of the original.

Narrow claim, in every verdict: VERIFIED means equivalence on the exercised blocks and inputs
only. It does not mean the function is correct for states the cases never produce.
"""
import argparse
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


@dataclass
class PortResult:
    eax: int = 0
    mask: int = 0xFFFFFFFF
    writes: dict = field(default_factory=dict)
    error: str = ""
    calls: list = field(default_factory=list)      # (addr, args tuple), in the order the seam saw them


def cases_text(spec, port_name):
    out = []
    for c in spec.cases:
        out.append("case %s" % c.id)
        out.append("fn %s" % port_name)
        for r, v in c.regs.items():
            out.append("reg %s 0x%X" % (r, v))
        for a, b in c.pokes.items():
            out.append("poke 0x%X %s" % (a, bytes(b).hex()))
        for k in spec.calls:
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
    for a in sorted(set(orig.writes) | set(port.writes)):
        if orig.writes.get(a) != port.writes.get(a):
            o, p = orig.writes.get(a), port.writes.get(a)
            out.append("byte 0x%X: original %s, port %s" % (
                a, "unchanged" if o is None else "0x%02X" % o, "unchanged" if p is None else "0x%02X" % p))
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
        orig = E.run_original(image, spec.entry, c.regs, c.pokes, spec.allow_calls, calls=spec.calls)
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
        orig = E.run_original(image, spec.entry, c.regs, c.pokes, spec.allow_calls, calls=spec.calls)
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
]


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
    ap.add_argument("--function", help="only this function")
    ap.add_argument("--self-check", action="store_true",
                    help="also require every mutant binding to be detected (an eax or byte difference, "
                         "and no port error)")
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
    closed = [r for r in funcs if r.verdict == "VERIFIED"
              and all(verdicts.get(a) == "VERIFIED" for a, _ in r.callees)]

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
    print("diff-verify: %d/%d functions VERIFIED%s; %d named gaps; %d/%d with every callee VERIFIED. Claim: "
          "equivalence on the exercised blocks and inputs only, each function with its callees stubbed or "
          "run as stated." % (len([r for r in funcs if r.verdict == "VERIFIED"]), len(funcs),
                              "; %d/%d mutants detected" % (len(mutants) - len(missed), len(mutants))
                              if args.self_check else "", len(gaps), len(closed), len(funcs)))
    return 1 if bad or missed else 0


if __name__ == "__main__":
    sys.exit(main())
