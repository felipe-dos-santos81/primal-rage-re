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
pairs are the task-8 report's list plus later measured reconciliations (the
README audio/row counters); a new pair needs its measured reason (the
code/record that superseded the wording).

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
    # Final truth Task 8's register reconciliation: the stale status/count
    # wording removed from the authoritative files. Each is scoped to the one
    # file that held it; the historical phase paragraphs keep their own text
    # with a superseded note.
    ("AGENTS.md", "U14 R1 stays declared"),
    ("AGENTS.md", "42 explicit capture paths"),
    ("AGENTS.md", "HIT 657 / NOT-HIT 98"),
    ("README.md", "13 `WEAKER`"),
    ("README.md", "42 explicit capture paths"),
    ("README.md", "0x25` bowling route stays declared"),
    ("port/RE_GUIDE.md", "13 rows are `WEAKER`"),
    ("port/RE_GUIDE.md", "5120 band is **INCONCLUSIVE**"),
    # The audio prefix total moved with Driver Truth (74 sections / 29,656 ticks
    # -> 86 / 44,489; register R09/R29 and AGENTS.md); README's summary must
    # carry the measured value. The row count moved with Driver Truth's R35/R36
    # (34 -> 36 rows; register §2) and Completion truth's R37/R38 (36 -> 38).
    # The capture inventory and the two-human win moved with Completion truth
    # Task 4 (45/44/45 -> 47/46/47; R03 closed on the promoted capture), and the
    # title states 66% ported / 100% analysed (design §1).
    ("README.md", "74 diff-allowed prefixes"),
    ("README.md", "It has 34 rows"),
    ("README.md", "It has 36 rows"),
    ("README.md", "45 explicit capture paths"),
    ("README.md", "44/45 present"),
    ("README.md", "Reverse Engineering 66%"),
    ("README.md", "the two-human 2-0 win is game evidence only"),
    ("README.md", "U23 moves ratchet records"),
    # Completion truth's final fix wave: the P2 is not idle, R16's evidence is
    # the t9-r5 capture (not the 110/133 latency reading), and the register has
    # one declared row (R39), not zero.
    ("README.md", "It has 38 rows"),
    ("README.md", "No row is `declared`"),
    ("README.md", "110/133"),
    ("AGENTS.md", "no row is `declared`"),
    ("AGENTS.md", "110/133"),
    ("AGENTS.md", "P2 idle after the join"),
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
