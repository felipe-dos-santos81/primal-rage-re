#!/usr/bin/env python3
"""The state-coverage census (plan 2026-10-28-state-coverage, Task 3).

Consumes Task 2's per-tag dumps (`coverage/<tag>.hits`, one
`fn-hit <tag> 0xADDR n=N` line per recorded address), unions them, and classifies
every *portable* function address as HIT / NOT-HIT / UNINSTRUMENTED. The split is
decided by the instrumentation scan (a seam, `PR_HIT` or `fn_register` in a
`port/src/*.c` body), not by the dump:

  * an instrumented address with a dump entry is HIT;
  * an instrumented address with no dump entry is NOT-HIT;
  * an uninstrumented address with a declared exception is UNINSTRUMENTED;
  * an uninstrumented address with neither is a guard failure (the census cannot
    classify it, and refuses to run).

The four UNINSTRUMENTED exceptions are the Task 2 report's no-body ports
(§D1.4/§D1.9): 0x29D60 (bare `ret`), 0x2EA30 (inert interrupt-lock counter),
0x2EA64 (bare `ret`), 0x500BB (inlined DPMI clock read). A `.c` function with a
column-0 `/* 0xADDR` header must carry a counter in its body (R2), which
`--check-instrumented` enforces on the `.c` files alone (`.h` files hold
declaration comments, not bodies).

The NOT-HIT set is pinned in `tools/coverage_baseline.txt` (one `0xADDR CLASS`
line per address, human-diffable). `--check` fails (exit 1) on a new NOT-HIT and
prints `improved:` / exits 2 on a new hit, unless `--update-baseline --reason`
re-pins it. A missing capture's tag is reported and excluded, never counted as a
NOT-HIT.
"""
import argparse
import glob
import io
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import port_progress as pp  # noqa: E402  (the shared strict registry)

RUNTIME_BASE = pp.RUNTIME_BASE
PORT_SRC = os.path.join(ROOT, 'port', 'src')
DEFAULT_BASELINE = os.path.join(ROOT, 'tools', 'coverage_baseline.txt')
DEFAULT_HITS_DIR = 'coverage'

# The four portable addresses Task 2 could not instrument: no C function body
# exists, so there is no `/* 0xADDR` header and no place to put a `PR_HIT`
# (Task 1 §D1.4, Task 2 report, record §D1.9).
UNINSTRUMENTED = frozenset({0x29D60, 0x2EA30, 0x2EA64, 0x500BB})

# Column-0 `/* 0xADDR` header — the same pattern port_progress.ported_set() and
# the strict registry use, so the guard and the credit scan never disagree.
_COL0 = re.compile(r'^/\* 0x([0-9A-Fa-f]{4,6})\b', re.M)
# A seam invocation's first argument is the original address (R2: PR_SEAM,
# PR_SEAM0, PR_SEAM_RET, PR_SEAM_RET0 all count).
_SEAM = re.compile(r'PR_SEAM(?:_RET)?0?\(\s*(0x[0-9A-Fa-f]+)')
_HIT = re.compile(r'PR_HIT\(\s*(0x[0-9A-Fa-f]+)')
_REG = re.compile(r'fn_register\(\s*0x([0-9A-Fa-f]+)')
_HITS_LINE = re.compile(r'\b0x([0-9A-Fa-f]{4,8})\b')


def port_c_files(root=PORT_SRC):
    """The port/src `*.c` files as [(path, text)] (the `.h` files are
    deliberately excluded — R2's declaration-comment rule)."""
    out = []
    for f in sorted(glob.glob(os.path.join(root, '**', '*.c'), recursive=True)):
        with open(f, errors='ignore') as fh:
            out.append((f, fh.read()))
    return out


def column0_headers(text):
    """The column-0 `/* 0xADDR` header addresses in one file's text."""
    return {int(m, 16) for m in _COL0.findall(text)}


def instrumented_addrs(text):
    """The addresses one `.c` file instruments: a seam, a PR_HIT or an
    fn_register. A seamed function is instrumented (R2) — the seam macro's own
    PR_HIT fires in a coverage build, so no separate PR_HIT line is needed."""
    out = {int(m, 16) for m in _SEAM.findall(text)}
    out |= {int(m, 16) for m in _HIT.findall(text)}
    out |= {int(m, 16) for m in _REG.findall(text)}
    return out


def instrumented_c(files):
    """The union of every `.c` file's instrumented addresses."""
    out = set()
    for _, text in files:
        out |= instrumented_addrs(text)
    return out


def portable_addrs():
    """The census universe: the 759 portable addresses (a portable FN_ address
    the strict registry credits). The runtime-credited addresses are declared
    runtime and are not part of the census (design G1)."""
    fns = pp.fn_addresses()
    ported = pp.ported_set()
    return frozenset(a for a in fns if a < RUNTIME_BASE and a in ported)


def classify(hit_addrs, instrumented, portable, exceptions=UNINSTRUMENTED):
    """{addr: 'HIT'|'NOT-HIT'|'UNINSTRUMENTED'} plus the guard failures.

    The split is decided by `instrumented` (the scan), not by `hit_addrs` (the
    dump). A portable address that is neither instrumented nor a declared
    exception is a guard failure — it is returned, not silently classified."""
    classes = {}
    failures = set()
    for a in sorted(portable):
        if a in instrumented:
            classes[a] = 'HIT' if a in hit_addrs else 'NOT-HIT'
        elif a in exceptions:
            classes[a] = 'UNINSTRUMENTED'
        else:
            failures.add(a)
    return classes, failures


def check_instrumented(files, exceptions=UNINSTRUMENTED, portable=None):
    """The `.c`-header guard: every column-0 `/* 0xADDR` header in a `.c` file
    whose address is a *portable* function must be instrumented (seam / PR_HIT /
    fn_register) or a declared exception. Returns the failing [(addr, path)]
    pairs (empty = pass).

    Scoped to `portable` (default: the census universe `portable_addrs()`): the
    runtime region's headers (>= 0x5D000, the declared-runtime replacement
    layer) and the many data-address comments (`/* 0x8xxxx */`) are not function
    bodies the census counts, so they are not guard targets."""
    if portable is None:
        portable = portable_addrs()
    instrumented = instrumented_c(files)
    failing = []
    for path, text in files:
        for a in sorted(column0_headers(text) & portable):
            if a not in instrumented and a not in exceptions:
                failing.append((a, path))
    return failing


def read_hits_file(path):
    """The addresses in one `coverage/<tag>.hits` dump."""
    addrs = set()
    with open(path) as f:
        for line in f:
            m = _HITS_LINE.search(line)
            if m:
                addrs.add(int(m.group(1), 16))
    return addrs


def parse_tags(s):
    """The expected tags from a `--tags` argument: space- and/or comma-separated
    (the Makefile passes a space-separated list; a manual call may use commas)."""
    return [t for t in re.split(r'[,\s]+', s) if t]


def read_hits_dir(hits_dir):
    """{tag: set-of-addrs} for every `*.hits` file in `hits_dir` (the file's
    basename without the extension is the tag)."""
    out = {}
    if not os.path.isdir(hits_dir):
        return out
    for name in sorted(os.listdir(hits_dir)):
        if name.endswith('.hits'):
            out[name[:-len('.hits')]] = read_hits_file(os.path.join(hits_dir, name))
    return out


def split_counts(classes):
    out = {'HIT': 0, 'NOT-HIT': 0, 'UNINSTRUMENTED': 0}
    for c in classes.values():
        out[c] += 1
    return out


def fmt_addrs(addrs):
    return ' '.join('0x%05X' % a for a in sorted(addrs))


def report(dumps, tags, classes, failures, out):
    """The stable sorted report: the tag counts, the split, the per-driver
    matrix (present tags), the missing captures, and the union by class."""
    present = set(dumps)
    expected = set(tags)
    missing = sorted(expected - present)
    unexpected = sorted(present - expected)
    out.write('coverage: tags present=%d missing=%d total=%d\n'
              % (len(present), len(missing), len(expected)))
    for tag in sorted(present):
        out.write('driver %s %s\n' % (tag, fmt_addrs(dumps[tag])))
    for tag in missing:
        out.write('missing %s\n' % tag)
    for tag in unexpected:
        out.write('unexpected %s\n' % tag)
    counts = split_counts(classes)
    out.write('coverage: split HIT=%d NOT-HIT=%d UNINSTRUMENTED=%d\n'
              % (counts['HIT'], counts['NOT-HIT'], counts['UNINSTRUMENTED']))
    out.write('hit %s\n' % fmt_addrs(sorted(a for a, c in classes.items() if c == 'HIT')))
    out.write('not-hit %s\n' % fmt_addrs(sorted(a for a, c in classes.items() if c == 'NOT-HIT')))
    out.write('uninstrumented %s\n' % fmt_addrs(sorted(a for a, c in classes.items() if c == 'UNINSTRUMENTED')))
    if failures:
        out.write('guard-failure %s\n' % fmt_addrs(failures))


def write_baseline(path, classes, reason):
    with open(path, 'w') as f:
        f.write('# reason: %s\n' % reason)
        for a in sorted(classes):
            f.write('0x%05X %s\n' % (a, classes[a]))


# The three classes the baseline can carry (the classification's only outputs).
_BASELINE_CLASSES = frozenset({'HIT', 'NOT-HIT', 'UNINSTRUMENTED'})


def read_baseline(path):
    """{addr: class} from the baseline. Blank lines and `#` comments are skipped;
    any other line must be exactly `0xADDR CLASS` (CLASS one of HIT / NOT-HIT /
    UNINSTRUMENTED), else a ValueError names the offending line — baseline
    corruption must not be silently dropped."""
    out = {}
    with open(path) as f:
        for lineno, line in enumerate(f, 1):
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            parts = line.split()
            if len(parts) != 2 or parts[1] not in _BASELINE_CLASSES \
                    or not parts[0].lower().startswith('0x'):
                raise ValueError('%s:%d: unparseable baseline line %r '
                                 '(expected "0xADDR CLASS")'
                                 % (path, lineno, line))
            try:
                addr = int(parts[0], 16)
            except ValueError:
                raise ValueError('%s:%d: bad address %r'
                                 % (path, lineno, parts[0]))
            out[addr] = parts[1]
    return out


def compare_baseline(classes, baseline):
    """(new_not_hit, new_hit): the addresses whose class moved from the pinned
    baseline. A NOT-HIT that was HIT/UNINSTRUMENTED/absent is a regression; a
    HIT that was NOT-HIT/UNINSTRUMENTED/absent is an improvement."""
    new_nothit = sorted(a for a, c in classes.items()
                        if c == 'NOT-HIT' and baseline.get(a) != 'NOT-HIT')
    new_hit = sorted(a for a, c in classes.items()
                     if c == 'HIT' and baseline.get(a) not in (None, 'HIT'))
    return new_nothit, new_hit


def run_census(dumps, tags, hit_addrs, instrumented, portable,
               baseline_path, check, update, reason,
               exceptions=UNINSTRUMENTED):
    """The census's decision logic, injectable for tests: returns
    (exit_code, report_text). exit 0 = clean (also an unpinned first run — the
    baseline is absent, so `make coverage`'s plan-Step-5 run-twice flow
    succeeds), 1 = guard failure or a new NOT-HIT, 2 = a new hit needs a
    re-pin. `--check` is strict (1/2) only once a baseline exists."""
    classes, failures = classify(hit_addrs, instrumented, portable, exceptions)
    buf = io.StringIO()
    report(dumps, tags, classes, failures, buf)

    if failures:
        buf.write('coverage: guard failure — %d portable address(es) are '
                  'neither instrumented nor declared UNINSTRUMENTED: %s\n'
                  % (len(failures), fmt_addrs(failures)))
        return 1, buf.getvalue()

    if update:
        if not reason:
            return 2, 'coverage: --update-baseline requires --reason\n'
        write_baseline(baseline_path, classes, reason)
        buf.write('coverage: baseline written (%s)\n' % baseline_path)
        return 0, buf.getvalue()

    if not check:
        return 0, buf.getvalue()

    if not os.path.exists(baseline_path):
        buf.write('unpinned: no baseline at %s (run --update-baseline --reason '
                  'to pin it)\n' % baseline_path)
        return 0, buf.getvalue()

    baseline = None
    try:
        baseline = read_baseline(baseline_path)
    except ValueError as e:
        buf.write('coverage: %s\n' % e)
        return 1, buf.getvalue()
    new_nothit, new_hit = compare_baseline(classes, baseline)
    if new_nothit:
        buf.write('coverage: new NOT-HIT: %s\n' % fmt_addrs(new_nothit))
        return 1, buf.getvalue()
    if new_hit:
        buf.write('improved: %s\n' % fmt_addrs(new_hit))
        return 2, buf.getvalue()
    counts = split_counts(classes)
    buf.write('coverage: no change (HIT=%d NOT-HIT=%d UNINSTRUMENTED=%d)\n'
              % (counts['HIT'], counts['NOT-HIT'], counts['UNINSTRUMENTED']))
    return 0, buf.getvalue()


def main(argv=None):
    ap = argparse.ArgumentParser(description='the state-coverage census')
    ap.add_argument('--hits-dir', default=DEFAULT_HITS_DIR)
    ap.add_argument('--baseline', default=DEFAULT_BASELINE)
    ap.add_argument('--tags', default='', help='comma-separated expected tags '
                    '(the full driver set; an expected tag with no dump is a '
                    'missing capture and is excluded, not NOT-HIT)')
    ap.add_argument('--check', action='store_true')
    ap.add_argument('--update-baseline', action='store_true')
    ap.add_argument('--reason', default='')
    ap.add_argument('--check-instrumented', action='store_true')
    args = ap.parse_args(argv)

    if args.check_instrumented:
        failing = check_instrumented(port_c_files())
        for a, path in failing:
            print('uninstrumented-header 0x%05X %s' % (a, path))
        if failing:
            print('check-instrumented: %d uninstrumented .c header(s)'
                  % len(failing))
            return 1
        print('check-instrumented: every .c column-0 header is instrumented')
        return 0

    tags = parse_tags(args.tags) if args.tags else []
    dumps = read_hits_dir(args.hits_dir)
    if tags:
        want = set(tags)
        dumps = {t: s for t, s in dumps.items() if t in want}
    hit_addrs = set()
    for s in dumps.values():
        hit_addrs |= s
    code, text = run_census(dumps, tags, hit_addrs, instrumented_c(port_c_files()),
                            portable_addrs(), args.baseline, args.check,
                            args.update_baseline, args.reason)
    sys.stdout.write(text)
    return code


if __name__ == '__main__':
    sys.exit(main())
