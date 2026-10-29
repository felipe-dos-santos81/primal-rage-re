#!/usr/bin/env python3
"""The side-1 ending cross-check (plan 2026-10-28-state-coverage Task 6,
task-6-report.md): the mode-0x1F frames of gp-u26-side1-ending's replay must be
byte-identical in rendered RGB to gp-u21-armadon-ending's replay. The ending
content is side-independent — the mode-0x1F handler 0x208F8 reads the winner's
character DS_0010782A[DS_00104AD4 * 0x94] = c1 = 4 for side 1, c0 = 4 for side
0 (the 0x208F8 differential row) — so the same ARMADON ending renders on both
sides.

The claim is over the rendered RGB of the mode-0x1F frames only, never the
whole run (whose pre-ending content differs by scenario and would trivially
differ). Each .ipx frame is expanded through its own DAC (gp_compare.expand_ipx)
before the comparison, so an index-byte difference that renders to the same RGB
is not a failure. Measured: the first mode-0x1F frame holds 55 021 differing
index bytes (a state-0 reset transient — that frame's DAC maps all 256 entries
to a single colour, so every differing index renders identically) and zero
differing rendered pixels; the DACs are identical.

The compared frame count is pinned (--expect-count), so an empty or truncated
mode-0x1F window fails instead of trivially passing. The tool prints the count
and the sha256 of the concatenated RGB (informational; byte-identity already
pins it). An absent replay skips (exit 0), matching gp_compare's absent-capture
skip.

Stdlib only; gp_compare is read-only here."""

import argparse
import hashlib
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gp_compare as gc


def mode_1f_indices(frames_path):
    """The port frame indices of the mode-0x1F entries, in dump order. A
    frames.txt line is '<idx> f=<f> tick=<tick> mode=<mode>' (gp_dump_if_new,
    test_game.c)."""
    out = []
    with open(frames_path) as f:
        for line in f:
            p = line.split()
            if len(p) != 4:
                continue
            if int(p[3].split('=', 1)[1], 16) == 0x1F:
                out.append(int(p[0]))
    return out


def load_rgb(dump_dir, indices):
    rgb = []
    for i in indices:
        with open(os.path.join(dump_dir, 'frame_%05d.ipx' % i), 'rb') as f:
            rgb.append(gc.expand_ipx(f.read()))
    return rgb


def compare(side1_dir, p1_dir, expect_count=None, out=print):
    """Compare the mode-0x1F rendered RGB of the side-1 and P1 replays.

    Returns (rc, count) where count is the number of mode-0x1F frames compared
    (None when both replays are absent and the check skips). rc is 0 on skip or
    match, 1 on a count mismatch or a byte difference."""
    if not os.path.isdir(side1_dir) or not os.path.isdir(p1_dir):
        out('gp_side1: no replay at %s and/or %s (skipped)' % (side1_dir, p1_dir))
        return 0, None
    a = mode_1f_indices(os.path.join(side1_dir, 'frames.txt'))
    b = mode_1f_indices(os.path.join(p1_dir, 'frames.txt'))
    if expect_count is not None and (len(a) != expect_count or len(b) != expect_count):
        out('gp_side1: FAIL: mode-0x1F frame count side 1 %d / P1 %d != pinned %d'
            % (len(a), len(b), expect_count))
        return 1, (len(a), len(b))
    if len(a) != len(b):
        out('gp_side1: FAIL: mode-0x1F frame count differs: side 1 %d, P1 %d'
            % (len(a), len(b)))
        return 1, (len(a), len(b))
    ra = load_rgb(side1_dir, a)
    rb = load_rgb(p1_dir, b)
    first = None
    for k, (x, y) in enumerate(zip(ra, rb)):
        if x != y:
            first = k
            break
    count = len(ra)
    if first is not None:
        out('gp_side1: FAIL: mode-0x1F frame %d (side 1 port %d, P1 port %d) '
            'differs in rendered RGB' % (first, a[first], b[first]))
        return 1, count
    concat = hashlib.sha256(b''.join(ra)).hexdigest()
    out('gp_side1: %d mode-0x1F frames compared, all byte-identical in rendered RGB' % count)
    out('gp_side1: concat sha256 %s' % concat)
    return 0, count


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--side1', required=True, help='the gp-u26-side1-ending replay dump')
    ap.add_argument('--p1', required=True, help='the gp-u21-armadon-ending replay dump')
    ap.add_argument('--expect-count', type=int, default=None,
                    help='the pinned mode-0x1F frame count (both sides); an unpinned '
                         'run can pass on an empty window')
    a = ap.parse_args()
    if a.expect_count is None:
        print('gp_side1: FAIL: --expect-count is not pinned')
        return 1
    rc, _ = compare(a.side1, a.p1, a.expect_count, print)
    return rc


if __name__ == '__main__':
    sys.exit(main())
