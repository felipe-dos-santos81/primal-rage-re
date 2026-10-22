#!/usr/bin/env python3
"""Host input truth U17: the device-2 selection and the no-pad clear (plan
2026-10-21-host-input-truth Task 6; record docs/superpowers/plans/
2026-10-21-host-input-truth-derivations.md §H6).

The scenario gp-u17-device2 (gp_session) walks the K11 service menu to MODIFY
CONTROLS (0x31F24), steps P1's device from KEYBOARD (0) to JOYSTICK 2 with one
p1.down and leaves with Esc, whose 0x32345 config_keys_apply writes the record's
device into BIOS+0x2D4 (0x1AE3B). DOSBox-X has no game port, so the ISR
sampler's presence block (0x1BC4F..0x1BCE6) probes 0x4FBBB(3) (P1's axes 0/1),
gets 0 and clears the word on its next tick: P1's device is the only one
selected, so the P1-only arm runs and the clear is 0x1BCBF (dev2 == 0), not the
both-player arm's 0x1BC6F. The capture's S records carry dev1/dev2
(+0x2D4/+0x2D6 behind DS_00101514, gp_session's POINTER_FIELDS), so the clear is
observable: the word reads 2 (the selection) and is 0 in every record after the
leave.

**One press is one step (+2).** MODIFY CONTROLS polls the *held* level every
iteration (0x31FC4 `xor eax,eax` -> 0x2EDE0(0, 1) -> 0x50161(0) returns the
level, 0x50161's own arm for mask 0), and its Down arm (0x320CF ->
0x320ED `dev + 2`; 0x3206F is the Up arm, `dev - 2`) is gated by
`until = [0x101500] + 0x0C` (0x320DB..0x320E5, masked at 0x31FE7): a second step
needs 12 ISR ticks. One menu iteration is `0x2EA78(1)` = 3 ISR ticks (three
waits on the frame word 0xEF6DE) and the harness holds the pad 5 capture frames
= 5/70.09 s = 71 ms = 4.3 ISR ticks (60.05 Hz), so the hold spans at least one
poll and fewer than the 12 ticks a second step needs: exactly one +2. The
arrow-latch path (0x2EBF0) maps only the literal arrow scans 0x48/0x50/0x4B/0x4D
through the config record; the pad's queued word carries its own scan (0x2D), so
this scenario's step is the level's. --device names the expected value (default
2, that derivation) for a capture whose measurement disagrees.

  gp_u17.py check --capture DIR [--clear-frame N] [--device N]
  gp_u17.py path --capture DIR                      one line per dev1/dev2 change

The clear frame is the first S record after the last injected key whose dev1 is
0. --clear-frame pins it exactly (a re-capture invalidates the pin; record
§G.24). Requiring the selection to be SEEN is the non-vacuous form: the
no-pad clear alone is also what a capture that never entered MODIFY CONTROLS
would show. A capture whose poll timing missed the 2 (one game tick wide) has
no dated selection; --unseen-selection-ok accepts it and then pins only the end
state, a named gap to record.

The scenario is WALL-paced (record §H6.9): a service-menu sub-loop presents
frames without the master loop's spin, so the poll has no S record between the
device write and the OPTIONS MENU leave and the selection's write cannot be
observed. The tool says so and pins the end state; the frame/trace replay and
its ratchets wait for a tick-keyed driver path, so the capture identity
(--capture-sha256, --capture-frames) is pinned here instead.
Stdlib only."""
import argparse
import hashlib
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gp_session as gs

SCENARIO = 'gp-u17-device2'
DEVICE2 = 2                      # the derived one-step selection (see the module docstring)


def check(lines, clear_frame=None, unseen_ok=False, device=DEVICE2):
    """[(label, ok, detail)] over a poll.log (record §H6). `device` is the value
    the one-step selection must read (2 unless a measurement says otherwise)."""
    recs = [r for r in (gs.parse(l) for l in lines) if r]
    snaps = gs.snapshots(lines)
    out = []

    has = bool(snaps) and all(n in next(iter(snaps.values())) for n in gs.DEV_FIELDS)
    out.append(('the S records carry dev1/dev2', has,
                '' if has else 'not a gp-u17 capture: the poll.log predates the fields'))
    if not has:
        return out

    # The scenario's own last three actions: the device step, the MODIFY CONTROLS
    # leave and the OPTIONS MENU leave (which resumes the master loop, so the
    # cleared word is in an S record).
    steps = [s for s in gs.SCENARIOS[SCENARIO]['steps']
             if s[0] != 'until_mode' and s[-1] != ('end',)]
    want = [gs.expand(s[-1])[0][0] for s in steps[-3:]]
    presses = [r for r in recs if r['kind'] == 'I' and 'press' in r]
    got = [r['press'] for r in presses[-3:]]
    out.append(('the scenario ends with the device step and the two leaves', got == want,
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

    # The selection: dev1 reads `device` (the derived one +2 step), in one run of
    # that single value. A wall-paced scenario cannot show it: the service menu's
    # sub-loops run without the master loop's spin, so no S record exists between
    # the device write and the OPTIONS MENU leave -- the word is pinned by its end
    # state and (later) the frame ratchet instead, a named gap (record §H6.9).
    sel = [(f, snaps[f]['dev1']) for f in fs if snaps[f]['dev1'] != 0]
    sel_ok = bool(sel) and all(v == device for _, v in sel)
    if gs.SCENARIOS[SCENARIO].get('wall') and not sel:
        out.append(('the selection is not sampled (wall-paced menu: no S record while one is open)',
                    True, 'pinned by the end state; the write is unobservable in the poll'))
    elif not unseen_ok:
        out.append(('the selection is seen: dev1 = %d (one Down from KEYBOARD)' % device, sel_ok,
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
                    'last dev1 = %d at f=%X, clear at f=%X' % (device, last_sel, clear)))

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
    ap.add_argument('--device', default=str(DEVICE2),
                    help='the device value the one-step selection must read (default 2; the '
                         'derivation is one +2 step, see the module docstring -- pass the '
                         'measured value if a capture disagrees, and record why)')
    ap.add_argument('--unseen-selection-ok', action='store_true',
                    help='accept a capture whose poll never sampled the selection (a named gap: '
                         'the clear is then pinned by the end state only)')
    ap.add_argument('--capture-sha256', default=None,
                    help="the pinned sha256 of the capture's poll.log; with it a different capture "
                         'fails (this scenario has no frame/trace replay yet, so the identity is '
                         'pinned here, record §H6.9)')
    ap.add_argument('--capture-frames', default=None,
                    help='the pinned number of frame_*.raw.gz in the capture')
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
    rows = check(lines, pin, a.unseen_selection_ok, int(a.device, 0))
    ok = all(r[1] for r in rows)
    # The capture identity (record §H6.9): this scenario has no frame/trace replay
    # yet, so the pinned sha256 and frame count live here instead of gp_compare's.
    if a.capture_sha256 not in (None, ''):
        got = hashlib.sha256(open(poll, 'rb').read()).hexdigest()
        rows.append(('the capture identity matches the pin', got == a.capture_sha256,
                     'poll.log sha256 %s..%s, pinned %s' % (got[:8], got[-6:], a.capture_sha256[:8] + '..')))
        ok = ok and rows[-1][1]
    if a.capture_frames not in (None, ''):
        n = len([p for p in os.listdir(a.capture) if p.startswith('frame_') and p.endswith('.raw.gz')])
        want = int(a.capture_frames, 0)
        rows.append(('the frame count matches the pin', n == want, '%d frames, pinned %d' % (n, want)))
        ok = ok and rows[-1][1]
    print('check=%s %s' % ('ok' if ok else 'FAIL', SCENARIO))
    for label, good, detail in rows:
        print('  %-4s %s%s' % ('ok' if good else 'FAIL', label, '' if not detail else ': ' + detail))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
