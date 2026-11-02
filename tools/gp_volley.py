#!/usr/bin/env python3
"""Volleyball truth Task 4 (plan 2026-11-01-volleyball-truth, design §5, record
2026-11-01-volleyball-truth-derivations §2): the mode-0x21 volleyball entry,
reached by real two-human input with no poke. Stdlib only.

  gp_volley.py check --capture DIR [--trace PORT/trace.txt]

The capture-side claims (the appended S fields; record §2.4):

* the mode entry: the first S record with mode 0x21, at which
  `b1f=3` (a two-human match), `afc=0` (stage 0), `abc=2`
  (`(b1f == 3) + 1`, the 0x4BDB4 gate), a live ball entry (`ball != 0`,
  0x4BDD3), the 0x1E match clock (0x4BDF4), both scores 0 (0x4BE19/0x4BE21),
  C1 = 1 and C5 = 1 (0x4BDE1/0x4BDEE);
* the clock: 0x1E -> 0 inside the mode, non-increasing, one unit at a time
  (the post-loop's 0x4C429 decrement; the mode's 1,717-frame timeout);
* a point: at least one score becomes non-zero while the mode runs (the port's
  own run scores the side-1 column, but the checker does not pin which side);
* the return: the first mode-6 S record after the mode has clock 0, ball 0,
  C5 0 and both scores 0 (the 0x4CC0C screen's hold, then 0x4CD98's clears);
* no W record: the trigger is real input, not a harness poke;
* real pad input on both sides before the entry.

The `+0x1F` count is on the effects entry, not in the S fields. With `--trace`
(the port replay's trace.txt) the checker also requires the port's V line at the
capture's entry frame to show the ball (type 6) at entry `0x1083F0` and count 8
-- the measured `0x4B716` increments 1..8 and the 0x4B579 `> 7` gate -- and the
entry/return T/Vg stores (mode 0x21, clk 0x1E->0, abc 2, scores 0-0, C1/C5).
The count is otherwise reported as not carried.

The claim boundary (task 4 brief): the mode's court pixels (gp_compare's frame
ratchet), a real point and the return to mode 6. The harness inputs are real;
no `DS_00104B1F`/`DS_00104ABC` poke exists. The mode's later content past the
return is not claimed.
"""
import argparse
import hashlib
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gp_session as gs

SCENARIO = 'gp-u30-volleyball'
MODE21 = 0x21
MODE6 = 0x06
CLOCK_ENTRY = 0x1E
BALL_ENTRY = 0x1083F0
FIELDS = ('ball', 'abc', 's0', 's1', 'clk', 'b8c1', 'b8c5')
ENTRY_VALUES = {'b1f': 3, 'afc': 0, 'abc': 2, 'clk': CLOCK_ENTRY,
                'b8c1': 1, 'b8c5': 1, 's0': 0, 's1': 0}
BALL_COUNT = 8


def _fail(name, out, detail):
    out('gp_volley: %s: evidence: FAIL: %s' % (name, detail))
    return 1


def sha_ok(name, cap_dir, sha, out=print):
    """The pinned capture identity: an empty or different poll.log sha256 fails."""
    if sha is None:
        return True
    with open(os.path.join(cap_dir, 'poll.log'), 'rb') as f:
        got = hashlib.sha256(f.read()).hexdigest()
    if sha == '' or got != sha:
        out('gp_volley: %s: capture: FAIL: poll.log sha256 %s != the pinned %s: '
            're-measure, then re-pin' % (name, got, sha or '(unpinned)'))
        return False
    return True


def _missing(name, out, snaps, fs):
    """The no-mode-0x21 verdict, with the measured residue when the appended
    fields are present."""
    mode6 = [f for f in fs if snaps[f].get('mode') == MODE6]
    if not mode6:
        return _fail(name, out, 'mode 0x21 never observed (no mode-6 snapshot either)')
    peak = max(snaps[f].get('clk', 0) for f in mode6)
    text = 'mode 0x21 never observed (clk max %d in mode 6)' % peak
    balls = {snaps[f].get('ball', 0) for f in mode6}
    text += ', ball %s' % ('live at some mode-6 frame' if any(balls) else 'never live')
    return _fail(name, out, text)


def _trace_get(records, f, out, what):
    """The one record at frame f, or None with a FAIL message."""
    r = records.get(f)
    if r is None:
        _fail('port', out, 'the port trace has no record at f=%X: the port replay '
              'does not hold the capture\'s %s frame' % (f, what))
    return r


def trace_count(path, entry_f, out=print):
    """The port trace's +0x1F count at the capture's entry frame: the `V` line
    of the measured ball entry `BALL_ENTRY` (record §2.4), type 6 and the
    counted hit 8 (the 0x4B579 `> 7` gate). Returns 0 ok, 1 FAIL."""
    found = None
    with open(path) as f:
        for line in f:
            if not line.startswith('V f='):
                continue
            r = gs.parse(line)
            if r.get('f') != entry_f:
                continue
            if r.get('e') == BALL_ENTRY and r.get('ty') == 6:
                found = r
                break
    if found is None:
        return _fail('port', out, 'the trace has no type-6 V line for the measured ball '
                     'entry %08X at the capture entry f=%X: the port replay does not '
                     'hold that entry' % (BALL_ENTRY, entry_f))
    if found['n'] != BALL_COUNT:
        return _fail('port', out, 'the trace entry %08X at f=%X has +0x1F=%d, want %d '
                     '(the 0x4B579 > 7 gate)' % (BALL_ENTRY, entry_f, found['n'],
                                                 BALL_COUNT))
    out('gp_volley: port: trace: the type-6 entry %08X at f=%X carries +0x1F=%d ok'
        % (BALL_ENTRY, entry_f, found['n']))
    return 0


def trace_state(path, entry_f, return_f, out=print):
    """The port trace's `T`/`Vg` entry and return stores (record §2.4). The T
    line carries the appended S fields; the Vg line is the observer's
    `DS_001088B6`/`ball`/`abc`/scores/clock/C1/C5. Returns 0 ok, 1 FAIL."""
    t, vg = {}, {}
    with open(path) as f:
        for line in f:
            r = gs.parse(line)
            if r is None:
                continue
            if r['kind'] == 'T':
                t[r['f']] = r
            elif r['kind'] == 'Vg':
                vg[r['f']] = r
    te = _trace_get(t, entry_f, out, 'entry')
    if te is None:
        return 1
    for name, want in (('mode', MODE21), ('b1f', 3), ('afc', 0), ('abc', 2),
                       ('clk', CLOCK_ENTRY), ('s0', 0), ('s1', 0),
                       ('b8c1', 1), ('b8c5', 1)):
        if te.get(name) != want:
            return _fail('port', out, 'the port T entry f=%X has %s=%r, want %X'
                         % (entry_f, name, te.get(name), want))
    if not te.get('ball'):
        return _fail('port', out, 'the port T entry f=%X has ball=0: the 0x4BDD3 store '
                     'is missing' % entry_f)
    ve = _trace_get(vg, entry_f, out, 'entry')
    if ve is None:
        return 1
    if ve.get('ball') != BALL_ENTRY:
        return _fail('port', out, 'the port Vg entry f=%X has ball=%r, want the measured '
                     'entry %08X' % (entry_f, ve.get('ball'), BALL_ENTRY))
    for name, want in (('abc', 2), ('clk', CLOCK_ENTRY), ('s0', 0), ('s1', 0),
                       ('c1', 1), ('c5', 1)):
        if ve.get(name) != want:
            return _fail('port', out, 'the port Vg entry f=%X has %s=%r, want %X'
                         % (entry_f, name, ve.get(name), want))
    tr = _trace_get(t, return_f, out, 'return')
    if tr is None:
        return 1
    for name, want in (('mode', MODE6), ('b1f', 3), ('afc', 0), ('abc', 2),
                       ('ball', 0), ('clk', 0), ('s0', 0), ('s1', 0),
                       ('b8c1', 1), ('b8c5', 0)):
        if tr.get(name) != want:
            return _fail('port', out, 'the port T return f=%X has %s=%r, want %X'
                         % (return_f, name, tr.get(name), want))
    vr = _trace_get(vg, return_f, out, 'return')
    if vr is None:
        return 1
    for name, want in (('ball', 0), ('abc', 2), ('clk', 0), ('s0', 0), ('s1', 0),
                       ('c1', 1), ('c5', 0)):
        if vr.get(name) != want:
            return _fail('port', out, 'the port Vg return f=%X has %s=%r, want %X'
                         % (return_f, name, vr.get(name), want))
    out('gp_volley: port: trace: the T/Vg entry f=%X (mode 0x21, b1f=3, afc=0, ball=%08X, '
        'abc=2, clk=%X, scores 0-0, C1=C5=1) and return f=%X (mode 6, clk=0, ball=0, '
        'abc=2, scores 0-0, C1=1, C5=0) ok'
        % (entry_f, BALL_ENTRY, CLOCK_ENTRY, return_f))
    return 0


def evidence(name, lines, out=print, trace=None):
    """The capture-only entry/point/return check. Returns 0 ok, 1 FAIL."""
    parsed = [r for r in (gs.parse(line) for line in lines) if r]
    if any(r['kind'] == 'W' for r in parsed):
        return _fail(name, out, 'a poke (W record) substituted for the real-input trigger')
    snaps = gs.snapshots(lines)
    fs = sorted(snaps)
    entry_f = next((f for f in fs if snaps[f].get('mode') == MODE21), None)
    if entry_f is None:
        return _missing(name, out, snaps, fs)
    entry = snaps[entry_f]
    missing = [n for n in FIELDS if any(n not in snaps[f] for f in fs)]
    if missing:
        return _fail(name, out, 'the S records lack the appended mode-0x21 fields: %s'
                     % ', '.join(missing))
    if 'afc' not in entry:
        return _fail(name, out, 'the S records lack afc (the scenario needs extra=WIN_EXTRA)')

    # The entry stores (record §2.4).
    for field, want in ENTRY_VALUES.items():
        got = entry[field]
        if got != want:
            return _fail(name, out, '%s=%X at the mode-0x21 entry f=%X, want %X '
                         '(0x4BD98/0x4BE19)' % (field, got, entry_f, want))
    if entry['ball'] == 0:
        return _fail(name, out, 'ball=0 at the mode-0x21 entry f=%X: the 0x4BDD3 store '
                     'is missing' % entry_f)

    after = [f for f in fs if f > entry_f]
    inside = [f for f in fs if f >= entry_f and snaps[f].get('mode') == MODE21]
    return_f = next((f for f in after if snaps[f].get('mode') == MODE6), None)
    if return_f is None:
        return _fail(name, out, 'mode 0x21 at f=%X never returns to mode 6 in the capture'
                     % entry_f)
    if not inside:
        return _fail(name, out, 'no mode-0x21 S record at or after f=%X' % entry_f)

    # The clock: 0x1E -> 0, no increase, one unit at a time.
    series = [(f, snaps[f]['clk']) for f in inside]
    for (f0, v0), (f1, v1) in zip(series, series[1:]):
        if v1 > v0:
            return _fail(name, out, 'clk rose %d -> %d at f=%X inside the mode' % (v0, v1, f1))
        if v0 - v1 > 1:
            return _fail(name, out, 'clk fell %d -> %d at f=%X: more than one unit per '
                         'snapshot (0x4C429)' % (v0, v1, f1))
    if series[-1][1] != 0:
        return _fail(name, out, 'the last mode-0x21 snapshot f=%X has clk=%d, want 0 (the '
                     '0x4C429 timeout end)' % (series[-1][0], series[-1][1]))
    if series[0][1] != CLOCK_ENTRY:
        return _fail(name, out, 'the first mode-0x21 snapshot f=%X has clk=%d, want %d'
                     % (series[0][0], series[0][1], CLOCK_ENTRY))

    # A point inside the mode; the return's clears.
    points = [(f, snaps[f]['s0'], snaps[f]['s1']) for f in inside]
    scored = [p for p in points if p[1] or p[2]]
    if not scored:
        return _fail(name, out, 'neither score left 0 inside mode 0x21 f=%X..%X: no point'
                     % (entry_f, inside[-1]))
    peak = (max(p[1] for p in points), max(p[2] for p in points))
    if peak[0] > 3 or peak[1] > 3:
        return _fail(name, out, 'a score reached %r: past the 3-point bound (the '
                     '0x4C321/0x4C8E8 `>= 3` arm calls 0x4CC0C and ends the mode)' % (peak,))
    back = snaps[return_f]
    for field, want in (('clk', 0), ('ball', 0), ('b8c5', 0), ('s0', 0), ('s1', 0)):
        if back.get(field) != want:
            return _fail(name, out, '%s=%X at the return f=%X, want %X (0x4CD98\'s clears '
                         'and the clock\'s 0)' % (field, back.get(field, -1), return_f, want))
    if back.get('b1f') != 3 or back.get('afc') != 0:
        return _fail(name, out, 'b1f/afc=%X/%X at the return f=%X: the two-human stage-0 '
                     'match did not survive the mode'
                     % (back.get('b1f', -1), back.get('afc', -1), return_f))

    pad_sides = {int(r['press'][1]) for r in parsed
                 if r['kind'] == 'I' and r.get('press', '')[:2] in ('p1', 'p2')
                 and r.get('f', entry_f) < entry_f}
    if pad_sides != {1, 2}:
        return _fail(name, out, 'the trigger lacks real pad input on both sides (sides %r)'
                     % sorted(pad_sides))

    out('gp_volley: %s: evidence: mode 0x21 at f=%X: b1f=3, afc=0 (stage 0), abc=2, '
        'ball=%08X, clk=%X, scores 0-0, C1=1, C5=1, no poke, pad input on both sides ok'
        % (name, entry_f, entry['ball'], entry['clk']))
    out('gp_volley: %s: evidence: the clock %X -> 0 over %d mode-0x21 snapshots; the point '
        '(scores peak %d-%d) and the return to mode 6 at f=%X (clk=0, ball=0, C5=0, '
        'scores 0-0) ok' % (name, CLOCK_ENTRY, len(inside), peak[0], peak[1], return_f))
    if trace is not None:
        rc = trace_count(trace, entry_f, out)
        if rc == 0:
            rc = trace_state(trace, entry_f, return_f, out)
        if rc != 0:
            return rc
    else:
        out('gp_volley: %s: the +0x1F count is not in the S fields; pass --trace for the '
            'port\'s V line (the entry count 8) and the T/Vg entry/return stores' % name)
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('cmd', choices=('check',))
    ap.add_argument('--capture', required=True)
    ap.add_argument('--scenario', default=SCENARIO)
    ap.add_argument('--trace', default=None,
                    help='the port replay trace.txt (the entry/return stores and the +0x1F count)')
    ap.add_argument('--capture-sha256', default=None,
                    help='the pinned poll.log sha256 (a re-capture fails)')
    a = ap.parse_args()
    if os.path.basename(os.path.normpath(a.capture)) != a.scenario:
        print('gp_volley: capture %s is not the %s capture' % (a.capture, a.scenario))
        return 1
    path = os.path.join(a.capture, 'poll.log')
    if not os.path.isfile(path):
        print('gp_volley: no capture at %s (skipped)' % path)
        return 0
    if not sha_ok(os.path.basename(os.path.normpath(a.capture)), a.capture,
                  a.capture_sha256):
        return 1
    if a.trace is not None and not os.path.isfile(a.trace):
        print('gp_volley: no port trace at %s' % a.trace)
        return 1
    with open(path) as f:
        lines = f.read().splitlines()
    return evidence(os.path.basename(os.path.normpath(a.capture)), lines, print, a.trace)


if __name__ == '__main__':
    sys.exit(main())
