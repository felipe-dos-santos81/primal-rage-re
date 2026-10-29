#!/usr/bin/env python3
"""Gameplay claim-coverage Task 11 (the drawn match 0x0A/0x0B): is a gp capture a drawn
match? Plan docs/superpowers/plans/2026-10-27-claim-coverage.md, record
task-11-draw-report.md. Stdlib only.

  gp_draw.py check --capture DIR    exit 0 when DIR/poll.log shows the KO-branch drawn
                                    match: AD4 == 2 (0x27BA4's draw result), B1E == ADC
                                    (the deciding round) and modes 0x0A (flow_round_over's
                                    round-over) then 0x0B (the winner pose) reached. Exit 1
                                    on the first failure.
  gp_draw.py check --timeout --capture DIR
                                    exit 0 when DIR/poll.log shows the timeout/health drawn
                                    round (flow_round_end_check 0x27FA8's arm at 0x280BA):
                                    AD4 == 0xFFFFFFFF (the 1-1 match is undecided), B1E ==
                                    ADC == 3, the clock top byte 0x1088F2 poked to 0 (the
                                    timeout) and the two +0x5A health bytes poked equal
                                    (within 3), then modes 0x0A and 0x0B. The two draw
                                    predicates are disjoint: the KO draw's S record has
                                    AD4 = 2, the timeout's AD4 = 0xFFFFFFFF, so each log
                                    fails the other's check.

The KO-branch draw (flow_round_end_check 0x27FA8): when either +0x5A byte is 0x78,
0x27C48 names the round winner (DS_00104B16 = 2 for A == B) and 0x27BA4 stores the match
result DS_00104AD4. AD4 = 2 only when B1E == ADC and the round-win counters w2/w3 are
level (0x27BA4). flow_round_end_check then calls flow_round_over (0x27ED8) -> mode 0xA,
and mode 0xA advances to 0xB once both +0x54 bytes clear (0x28C0E..0x28C1E). The check
reads the S records: the draw's record (ad4 = 2) must carry b1e == adc and w2 == w3, and
modes 0x0A and 0x0B must appear in that order. Narrow, like the other gp oracles. The
existing gp_twop.py (whose check rejects ad4 == 2) is not this checker.

The timeout/health draw (flow_round_end_check's 0x280BA arm): when both +0x5A bytes are
below 0x78 and the clock top byte 0x1088F2 (the signed dword DS_001088EF's +3 byte) is at
most 0, the arm re-reads the two +0x5A bytes after 0x39FF4 and, with B1E == ADC and
|A - B| <= 3, calls flow_round_over -> mode 0xA -> mode 0xB. It calls flow_round_over
without 0x27C48, so AD4 stays 0xFFFFFFFF (the 1-1 match is undecided) and DS_00104B16
keeps round 2's value (never a draw predicate). flow_round_over resets the clock to 0x1E
and both +0x5A bytes to 0 in the same iteration the draw fires, so no S record carries the
poked clock 0 or the poked health bytes; the timeout check reads them from the W records
(the poke) instead."""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gp_session as gs

# The fields the draw check reads: adc (DS_00104ADC, the best-of rounds, 0x25A14..0x25A28),
# ad4/w2/w3/b1e (gp_session.WIN_EXTRA, record §W.2). mode and s0_5a/s1_5a are SNAP_FIELDS'.
DRAW_FIELDS = ('adc', 'ad4', 'w2', 'w3', 'b1e')

# The timeout/health draw's fields: adc/ad4/b1e (DRAW_FIELDS' subset it reads; the arm's
# 0x28069 test is B1E == ADC, not the level w2/w3 the KO draw needs), plus s0_5a/s1_5a and
# mode (SNAP_FIELDS'). The clock top byte 0x1088F2 is read from the W records, not the S
# records (flow_round_over resets it to 0x1E in the draw's own iteration, record §D7).
TIMEOUT_FIELDS = ('adc', 'ad4', 'b1e')
NO_RESULT = 0xFFFFFFFF            # DS_00104AD4 = -1: 0x27BA4's undecided match (0x25A0E)
CLOCK_DS = 0x1088F2               # the round timer's top byte (the timeout poke writes 0)
HEALTH_DS = (0x10780A, 0x10789E)  # the two +0x5A health bytes (the arm reads them within 3)


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


def evidence_timeout(name, lines, out=print):
    """The timeout/health drawn round (0x280BA). Returns 0 ok, 1 FAIL. The S records
    name the draw (ad4 = 0xFFFFFFFF, b1e = adc = 3, |s0_5a - s1_5a| <= 3, modes 0x0A then
    0x0B); the W records name the poke that triggered it (0x1088F2 = 0, the health bytes
    within 3), because flow_round_over clears both in the draw's own iteration."""
    snaps = gs.snapshots(lines)
    ws = [r for r in (gs.parse(l) for l in lines) if r and r['kind'] == 'W']
    if not snaps or any(n not in next(iter(snaps.values())) for n in TIMEOUT_FIELDS):
        out('gp_draw: %s: timeout: FAIL: the capture records no TIMEOUT fields '
            '(adc/ad4/b1e)' % name)
        return 1
    fs = sorted(snaps)
    draw = next((f for f in fs if snaps[f]['ad4'] == NO_RESULT and snaps[f]['mode'] == 0x0A), None)
    if draw is None:
        out("gp_draw: %s: timeout: FAIL: no S record has ad4 = 0xFFFFFFFF and mode 0x0A "
            "(0x280BA's timeout draw)" % name)
        return 1
    r = snaps[draw]
    if r['b1e'] != r['adc'] or r['b1e'] != 3:
        out('gp_draw: %s: timeout: FAIL: the draw at f=%X has b1e = %d, adc = %d, not '
            'b1e = adc = 3 (0x280BA draws only on the deciding round)'
            % (name, draw, r['b1e'], r['adc']))
        return 1
    if abs(r['s0_5a'] - r['s1_5a']) > 3:
        out('gp_draw: %s: timeout: FAIL: the draw at f=%X has |s0_5a - s1_5a| = %d > 3 '
            '(0x280BA requires the +0x5A bytes within 3)'
            % (name, draw, abs(r['s0_5a'] - r['s1_5a'])))
        return 1
    a = next((f for f in fs if snaps[f]['mode'] == 0x0A), None)
    b = next((f for f in fs if snaps[f]['mode'] == 0x0B), None)
    if b is None:
        out("gp_draw: %s: timeout: FAIL: mode 0x0B (the winner pose, 0x28C38) never observed" % name)
        return 1
    if b < a:
        out('gp_draw: %s: timeout: FAIL: mode 0x0B at f=%X precedes mode 0x0A at f=%X '
            '(0x28BD4 advances 0xA -> 0xB)' % (name, b, a))
        return 1
    clock = next((w for w in ws if w['addr'] == CLOCK_DS and w['now'] == '00'), None)
    if clock is None:
        out('gp_draw: %s: timeout: FAIL: no W record wrote 0x1088F2 = 0 (the timeout '
            'clock poke)' % name)
        return 1
    ha = next((w for w in ws if w['addr'] == HEALTH_DS[0]), None)
    hb = next((w for w in ws if w['addr'] == HEALTH_DS[1]), None)
    if ha is None or hb is None or abs(int(ha['now'], 16) - int(hb['now'], 16)) > 3:
        out('gp_draw: %s: timeout: FAIL: the +0x5A pokes (0x10780A/0x10789E) are missing '
            'or more than 3 apart (0x280BA requires them within 3)' % name)
        return 1
    out('gp_draw: %s: timeout: the drawn round: ad4 = 0xFFFFFFFF, b1e = adc = 3, '
        '|A - B| <= 3, clock 0x1088F2 poked 0, mode 0x0A at f=%X, mode 0x0B at f=%X ok'
        % (name, a, b))
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('cmd', choices=('check',))
    ap.add_argument('--capture', required=True)
    ap.add_argument('--timeout', action='store_true')
    a = ap.parse_args()
    path = os.path.join(a.capture, 'poll.log')
    if not os.path.isfile(path):
        print('gp_draw: %s: no capture at %s (skipped)' % (a.capture, path))
        return 0
    with open(path) as f:
        lines = f.read().splitlines()
    fn = evidence_timeout if a.timeout else evidence
    return fn(os.path.basename(os.path.normpath(a.capture)), lines)


if __name__ == '__main__':
    sys.exit(main())
