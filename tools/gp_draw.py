#!/usr/bin/env python3
"""Gameplay claim-coverage Task 11 (the drawn match 0x0A/0x0B): is a gp capture a drawn
match? Plan docs/superpowers/plans/2026-10-27-claim-coverage.md, record
task-11-draw-report.md. Stdlib only.

  gp_draw.py check --capture DIR    exit 0 when DIR/poll.log shows the KO-branch drawn
                                    match: AD4 == 2 (0x27BA4's draw result), B1E == ADC
                                    (the deciding round) and modes 0x0A (flow_round_over's
                                    round-over) then 0x0B (the winner pose) reached. Exit 1
                                    on the first failure.

The KO-branch draw (flow_round_end_check 0x27FA8): when either +0x5A byte is 0x78,
0x27C48 names the round winner (DS_00104B16 = 2 for A == B) and 0x27BA4 stores the match
result DS_00104AD4. AD4 = 2 only when B1E == ADC and the round-win counters w2/w3 are
level (0x27BA4). flow_round_end_check then calls flow_round_over (0x27ED8) -> mode 0xA,
and mode 0xA advances to 0xB once both +0x54 bytes clear (0x28C0E..0x28C1E). The check
reads the S records: the draw's record (ad4 = 2) must carry b1e == adc and w2 == w3, and
modes 0x0A and 0x0B must appear in that order. Narrow, like the other gp oracles. The
existing gp_twop.py (whose check rejects ad4 == 2) is not this checker."""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gp_session as gs

# The fields the draw check reads: adc (DS_00104ADC, the best-of rounds, 0x25A14..0x25A28),
# ad4/w2/w3/b1e (gp_session.WIN_EXTRA, record §W.2). mode and s0_5a/s1_5a are SNAP_FIELDS'.
DRAW_FIELDS = ('adc', 'ad4', 'w2', 'w3', 'b1e')


def evidence(name, lines, out=print):
    """The check. Returns 0 ok, 1 FAIL."""
    snaps = gs.snapshots(lines)
    if not snaps or any(n not in next(iter(snaps.values())) for n in DRAW_FIELDS):
        out('gp_draw: %s: evidence: FAIL: the capture records no DRAW fields '
            '(adc/ad4/w2/w3/b1e)' % name)
        return 1
    fs = sorted(snaps)
    draw = next((f for f in fs if snaps[f]['ad4'] == 2), None)
    if draw is None:
        out("gp_draw: %s: evidence: FAIL: no S record has ad4 = 2 (0x27BA4's draw result)" % name)
        return 1
    r = snaps[draw]
    if r['b1e'] != r['adc']:
        out('gp_draw: %s: evidence: FAIL: the draw at f=%X has b1e = %d, not adc = %d '
            '(0x27BA4 draws only on the deciding round)' % (name, draw, r['b1e'], r['adc']))
        return 1
    if r['w2'] != r['w3']:
        out('gp_draw: %s: evidence: FAIL: the draw at f=%X has w2 = %d != w3 = %d '
            '(0x27BA4 draws only when the round-win counters are level)'
            % (name, draw, r['w2'], r['w3']))
        return 1
    a = next((f for f in fs if snaps[f]['mode'] == 0x0A), None)
    if a is None:
        out("gp_draw: %s: evidence: FAIL: mode 0x0A (flow_round_over's round-over) never observed" % name)
        return 1
    b = next((f for f in fs if snaps[f]['mode'] == 0x0B), None)
    if b is None:
        out("gp_draw: %s: evidence: FAIL: mode 0x0B (the winner pose, 0x28C38) never observed" % name)
        return 1
    if b < a:
        out('gp_draw: %s: evidence: FAIL: mode 0x0B at f=%X precedes mode 0x0A at f=%X '
            '(0x28BD4 advances 0xA -> 0xB)' % (name, b, a))
        return 1
    out('gp_draw: %s: evidence: the drawn match: ad4 = 2, b1e = adc = %d, w2 = w3 = %d, '
        'mode 0x0A at f=%X, mode 0x0B at f=%X ok' % (name, r['b1e'], r['w2'], a, b))
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('cmd', choices=('check',))
    ap.add_argument('--capture', required=True)
    a = ap.parse_args()
    path = os.path.join(a.capture, 'poll.log')
    if not os.path.isfile(path):
        print('gp_draw: %s: no capture at %s (skipped)' % (a.capture, path))
        return 0
    with open(path) as f:
        lines = f.read().splitlines()
    return evidence(os.path.basename(os.path.normpath(a.capture)), lines)


if __name__ == '__main__':
    sys.exit(main())
