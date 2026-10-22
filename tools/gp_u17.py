#!/usr/bin/env python3
"""Host input truth U17: the device-2 selection and the no-pad clear (plan
2026-10-21-host-input-truth Task 6; record docs/superpowers/plans/
2026-10-21-host-input-truth-derivations.md §H6).

The scenario gp-u17-device2 (gp_session) walks the K11 service menu to MODIFY
CONTROLS (0x31F24), steps P1's device from KEYBOARD (0) to JOYSTICK 2 with one
p1.down -- the 0x3206F/0x320ED arm adds 2 -- and leaves with Esc, whose
0x32345 config_keys_apply writes the record's device into BIOS+0x2D4 (0x1AE3B).
DOSBox-X has no game port, so the ISR sampler's presence block (0x1BC4F..
0x1BCE6) probes 0x4FBBB(3) (P1's axes 0/1), gets 0 and clears the word on its
next tick (0x1BC6F). The capture's S records carry dev1/dev2 (+0x2D4/+0x2D6
behind DS_00101514, gp_session's POINTER_FIELDS), so the clear is observable:
the word reads 2 (the selection) and is 0 in every record after the leave.

  gp_u17.py check --capture DIR [--clear-frame N]   the checks; exit 1 on a FAIL
  gp_u17.py path --capture DIR                      one line per dev1/dev2 change

The clear frame is the first S record after the last injected key whose dev1 is
0. --clear-frame pins it exactly (a re-capture invalidates the pin; record
§G.24). Requiring the selection to be SEEN is the non-vacuous form: the
no-pad clear alone is also what a capture that never entered MODIFY CONTROLS
would show. A capture whose poll timing missed the 2 (one game tick wide) has
no dated selection; --unseen-selection-ok accepts it and then pins only the end
state, a named gap to record.
Stdlib only."""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gp_session as gs

SCENARIO = 'gp-u17-device2'
DEVICE2 = 2                      # MODIFY CONTROLS' JOYSTICK 2 (0x3206F/0x320ED)


def check(lines, clear_frame=None, unseen_ok=False):
    """[(label, ok, detail)] over a poll.log (record §H6)."""
    recs = [r for r in (gs.parse(l) for l in lines) if r]
    snaps = gs.snapshots(lines)
    out = []

    has = bool(snaps) and all(n in next(iter(snaps.values())) for n in gs.DEV_FIELDS)
    out.append(('the S records carry dev1/dev2', has,
                '' if has else 'not a gp-u17 capture: the poll.log predates the fields'))
    if not has:
        return out

    # The scenario's own last two actions: the device step, then the leave.
    steps = [s for s in gs.SCENARIOS[SCENARIO]['steps']
             if s[0] != 'until_mode' and s[-1] != ('end',)]
    want = [gs.expand(s[-1])[0][0] for s in steps[-2:]]
    presses = [r for r in recs if r['kind'] == 'I' and 'press' in r]
    got = [r['press'] for r in presses[-2:]]
    out.append(('the scenario ends with the device step and the leave (0x32345)', got == want,
                'want %s, got %s' % (want, got)))

    # The service menu keeps mode 0x27 from the Enter to the end (the walk's own
    # record: no mode change after it).
    fs = sorted(snaps)
    first27 = next((f for f in fs if snaps[f]['mode'] == 0x27), None)
    stayed = first27 is not None and all(snaps[f]['mode'] == 0x27 for f in fs if f >= first27)
    out.append(('mode stays 0x27 from the menu Enter to the end', stayed,
                'first 0x27 %s' % (None if first27 is None else hex(first27))))

    # P2's word is never selected.
    dev2 = [(f, snaps[f]['dev2']) for f in fs if snaps[f]['dev2'] != 0]
    out.append(('dev2 stays 0 (P2 never selects a device)', not dev2,
                '' if not dev2 else 'first f=%X dev2=%X' % dev2[0]))

    # The selection: dev1 reads 2 (KEYBOARD -> JOYSTICK 2), in one run.
    sel = [(f, snaps[f]['dev1']) for f in fs if snaps[f]['dev1'] != 0]
    sel_ok = bool(sel) and all(v == DEVICE2 for _, v in sel)
    if not unseen_ok:
        out.append(('the selection is seen: dev1 = 2 (one p1.down from KEYBOARD)', sel_ok,
                    'frames %s' % (['%X' % f for f, _ in sel] or 'none: the poll never sampled it')))
    elif not sel_ok:
        out.append(('the selection was not sampled: the clear is pinned by the end state only', True,
                    'unseen (accepted by --unseen-selection-ok)'))

    # The leave: the last injected key's frame (an action fires in the spin of
    # F - 1, so the menu consumes it in iteration F = f + 1).
    leave_f = presses[-1].get('f') if presses else None
    if leave_f is None:
        out.append(('the leave key is in the capture', False, 'no I record'))
        return out
    after = [f for f in fs if f > leave_f]
    clear = next((f for f in after if snaps[f]['dev1'] == 0), None)
    out.append(('the clear is observed after the leave', clear is not None,
                'leave f=%X, first dev1 = 0 at %s' % (leave_f, 'none' if clear is None else 'f=%X' % clear)))
    if clear is None:
        return out
    sticky = [f for f in after if f > clear and snaps[f]['dev1'] != 0]
    out.append(('no record after the clear holds a device word', not sticky,
                '' if not sticky else 'first sticky f=%X' % sticky[0]))
    if sel_ok:
        last_sel = sel[-1][0]
        out.append(('the clear follows the selection', clear > last_sel,
                    'last dev1 = 2 at f=%X, clear at f=%X' % (last_sel, clear)))

    # The pin.
    if clear_frame is not None:
        out.append(('the clear frame is the pinned one', clear == clear_frame,
                    'measured f=%X, pinned f=%X' % (clear, clear_frame)))

    out.append(('clear frame', True, 'f=%X (%d)' % (clear, clear)))
    return out


def path(lines):
    """One line per dev1/dev2 change: f=.. dev1=.. dev2=.. (record §H6)."""
    snaps = gs.snapshots(lines)
    rows, prev = [], None
    for f in sorted(snaps):
        v = (snaps[f]['dev1'], snaps[f]['dev2'])
        if v != prev:
            rows.append('f=%X dev1=%X dev2=%X' % (f, v[0], v[1]))
            prev = v
    return rows


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('cmd', choices=('check', 'path'))
    ap.add_argument('--capture', required=True)
    ap.add_argument('--clear-frame', default=None,
                    help='the pinned clear frame (decimal or 0x hex); exact')
    ap.add_argument('--unseen-selection-ok', action='store_true',
                    help='accept a capture whose poll never sampled dev1 = 2 (a named gap: the '
                         'clear is then pinned by the end state only)')
    a = ap.parse_args()
    poll = os.path.join(a.capture, 'poll.log')
    if not os.path.isfile(poll):
        print('gp_u17: no poll.log at %s' % a.capture)
        return 2
    with open(poll) as f:
        lines = f.read().splitlines()
    if a.cmd == 'path':
        for row in path(lines):
            print(row)
        return 0
    pin = None if a.clear_frame in (None, '') else int(a.clear_frame, 0)
    rows = check(lines, pin, a.unseen_selection_ok)
    ok = all(r[1] for r in rows)
    print('check=%s %s' % ('ok' if ok else 'FAIL', SCENARIO))
    for label, good, detail in rows:
        print('  %-4s %s%s' % ('ok' if good else 'FAIL', label, '' if not detail else ': ' + detail))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
