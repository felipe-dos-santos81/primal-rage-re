#!/usr/bin/env python3
"""Guard: no doc repeats a retired "ported-function-as-gap" claim.

Task 8 of the claim-coverage phase reconciled a set of stale spec/status claims
against the code and records (each is a function that was ported and
differentially verified, or a measured count/status superseded by a later
phase). This tool pins the exact stale wording of each retired claim as a
`(file, phrase)` pair and fails when any phrase reappears, so a doc edit cannot
silently regress the claim back to "still a gap".

Each phrase is the exact stale substring the task removed, scoped to the one
file that held it, so a still-true named gap elsewhere is never flagged. The
pairs are the task-8 report's list; a new pair needs that report's measured
reason (the code/record that superseded the wording).

Exit 0 on a clean tree, 1 on any stale phrase.
"""
import argparse
import os
import sys

# (file-relative-to-root, phrase-that-must-not-appear). See the module docstring.
CLAIMS = [
    # audio.md's "the port's audio was never heard on this host" — superseded by
    # the 49716 Hz coreaudio open and the music-truth phase; rendered audibility
    # is the remaining (correct) named gap, not "never heard".
    ("port/spec/audio.md", "never heard on this host"),
    # The audio trace count: 29 `.dro` files across 28 capture directories
    # (gp-pads holds two), not the earlier "27 replayed" figure.
    ("port/spec/audio.md", "27 replayed"),
    ("README.md", "27 replayed"),
    # game_flow.md's ported-but-called-gap lines: each address is now ported with
    # a differential row (see the task-8 report for the function/row per address).
    ("port/spec/game_flow.md", "`0xE8916`-table callers are unported"),
    ("port/spec/game_flow.md", "tails remain named gaps"),
    ("port/spec/game_flow.md", "is still a named gap"),
    ("port/spec/game_flow.md", "unported `0x36870`"),
    ("port/spec/game_flow.md", "`0x4F83C` ships"),
    # RE_GUIDE's "4b gap": the credit countdown is ported (4b-B); the only 4b gap
    # left is the missing coin-countdown capture, not a port gap.
    ("port/RE_GUIDE.md", "the live credit countdown is a 4b gap"),
    # PROGRESS's "the port is complete": that is the portable figure (759/759),
    # not the proof state.
    ("docs/PROGRESS.md", "The port is complete"),
]


def default_root():
    return os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def check(root):
    """Return the list of violations; empty means clean."""
    problems = []
    for rel, phrase in CLAIMS:
        path = os.path.join(root, rel)
        if not os.path.isfile(path):
            problems.append('%s: missing (cannot check %r)' % (rel, phrase))
            continue
        with open(path, 'r', errors='ignore') as f:
            for lineno, line in enumerate(f, 1):
                if phrase in line:
                    problems.append('%s:%d: stale phrase %r'
                                    % (rel, lineno, phrase))
    return problems


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--root', default=None,
                    help='repo root (default: the directory above tools/)')
    args = ap.parse_args(argv)
    root = args.root or default_root()
    problems = check(root)
    for p in problems:
        print('check_docs_claims: %s' % p)
    if problems:
        print('check_docs_claims: %d stale doc claim(s)' % len(problems))
        return 1
    print('check_docs_claims: %d stale-phrase claim(s) absent' % len(CLAIMS))
    return 0


if __name__ == '__main__':
    sys.exit(main())
