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

    def __post_init__(self):
        ids = [c.id for c in self.cases]
        dup = sorted({i for i in ids if ids.count(i) > 1})
        if dup:
            raise ValueError("spec %s: duplicate case id %s" % (self.name, ", ".join(dup)))


@dataclass
class PortResult:
    eax: int = 0
    mask: int = 0xFFFFFFFF
    writes: dict = field(default_factory=dict)
    error: str = ""


def cases_text(spec, port_name):
    out = []
    for c in spec.cases:
        out.append("case %s" % c.id)
        out.append("fn %s" % port_name)
        for r, v in c.regs.items():
            out.append("reg %s 0x%X" % (r, v))
        for a, b in c.pokes.items():
            out.append("poke 0x%X %s" % (a, bytes(b).hex()))
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
    diffs: int = 0                                   # eax and byte differences (not port errors)


def verify_spec(spec, image, port_results, port_name=None):
    """Compare every case of `spec` against `port_results` (case id -> PortResult)."""
    sent, got = [c.id for c in spec.cases], set(port_results)
    if got != set(sent):
        raise ValueError("%s: the port's results do not match the cases sent: missing %s, extra %s"
                         % (port_name or spec.name, sorted(set(sent) - got), sorted(got - set(sent))))
    info = E.static_scan(image, spec.entry)
    executed, outside, problems, blocked = set(), set(), [], []
    port_errors, diffs = [], 0
    for c in spec.cases:
        orig = E.run_original(image, spec.entry, c.regs, c.pokes, spec.allow_calls)
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
            if not d.startswith("port: "):
                diffs += 1
    hit, unhit = E.coverage(info, executed)
    res = SpecResult(port_name or spec.name, spec.entry, "VERIFIED", len(spec.cases), len(hit),
                     len(info.leaders), problems, [a for a in unhit if a not in spec.unhit_named],
                     sorted(outside), port_errors, diffs)
    if problems:
        res.verdict = "MISMATCH"
    elif blocked:
        res.verdict, res.problems = "NOT_EXERCISABLE", blocked
    elif res.unhit or info.indirect or info.truncated:
        res.verdict = "PARTIAL"
        if info.indirect:
            res.problems.append("indirect jmp/call at %s: targets unknown" % ",".join(hex(a) for a in info.indirect))
        if info.truncated:
            res.problems.append("static scan truncated")
    return res


# ---- the E1 specs: four ported functions, each cased to cover every block ----------------------

DS_RNG = 0xEF6D8
DS_FLAGS = 0x107EE0
DS_CREDITS = 0x105C00
DS_FREEPLAY = 0x105D60
DS_NODEBIT = 0x104B1F

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
]


# ---- driver -------------------------------------------------------------------------------------

def verify_all(diffrun, exe, image_path, specs, only=None, mutants=False):
    """One SpecResult per function (or per mutant binding when `mutants`)."""
    results = []
    for spec in specs:
        if only and spec.name != only:
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
        return False, "no eax or byte difference on any case (verdict %s)" % r.verdict
    return True, ""


def table_row(r):
    note = "; reads outside the image: " + ", ".join("0x%X+%d" % o for o in r.outside) if r.outside else ""
    return "| %s | 0x%05X | %d | %d/%d | %s%s |" % (r.name, r.entry, r.ncases, r.hit, r.total, r.verdict, note)


TABLE_HEAD = ("| function | original | cases | blocks hit/total | verdict |\n"
              "|---|---|---|---|---|")


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
    bad = [r for r in real if r.verdict != "VERIFIED"]
    missed = [(r, why) for r in mutants for ok, why in [mutant_detection(r)] if not ok]

    rows = [TABLE_HEAD] + [table_row(r) for r in real + mutants]
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
    print("diff-verify: %d/%d functions VERIFIED%s. Claim: equivalence on the exercised blocks and "
          "inputs only." % (len(real) - len(bad), len(real),
                            "; %d/%d mutants detected" % (len(mutants) - len(missed), len(mutants))
                            if args.self_check else ""))
    return 1 if bad or missed else 0


if __name__ == "__main__":
    sys.exit(main())
