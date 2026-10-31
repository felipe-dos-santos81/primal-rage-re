#!/usr/bin/env python3
"""Live truth Task 6 (record 2026-10-29-live-truth-derivations.md §L6) and final
truth Task 5 (record 2026-10-30-final-truth-derivations.md §F5): the mode-0x25
bowling entry, reached by real two-player input. Stdlib only.

  gp_bowling.py check --capture DIR
  gp_bowling.py check --staggered [--scenario gp-bowling-2] --capture DIR

`check` is the live-truth route's checker (three move-0 attempts): both human
sides on character 4 (ARMADON), at least three rising edges into s*_52=9 (the
reaction callback's animation state, about two per move execution and not an
attempt count), non-decreasing DS_00108892 growth to a visible 4 apart from one
-1 immediately after a value >=6 (the 0x4E131/0x4E189 rejection arm), and
fight_mode25_enter's stores at the first mode-0x25 snapshot (0x4E11C).

`check --staggered` is final truth Task 5's five-attempt route (scenario
gp-bowling-2). It adds the fields the live-truth phase did not sample
(slot 0's +0x53 = 0x107803, the signed camera x 0xF0AF0) and the raw facts of
§F5:

* 3..10 rising edges into s*_52=9 per side: the callback ran (>= 3, the
  live-truth minimum) and at most twice per move execution over the route's
  five attempts (both prior captures measured exactly two per attempt per
  side);
* the counter's visible maximum reaches 6 (the fourth attempt's increments over
  the three-attempt route's measured 5), every increase is at most +2 (the two
  0x45158 hooks, one increment each per frame), and every decrease is the
  single -1 of the rejection arm from a value >= 6 at a frame whose s0_53 is 0
  (0x4E131/0x4E189 only run behind the 0x263DE gate);
* the entry snapshot has s0_53 = 0 (the gate's byte), the signed camx within
  [-0x3300, 0x3300] (0x4E13D..0x4E151), and every fight_mode25_enter store
  (b892 = 0 at 0x4E1CB, b1f = 3, b1d not 2/3, c0 = c1 = 4, b25 = 0, b8c4 0 -> 1,
  b8a6 = 0xB4, b8bd = b415 = 0, b8c0 = 1);
* at least one pre-entry s0_53 = 7 sample (the 91-frame reaction window of
  §F5.2 existed; the counter cannot be read by the gate while it is 7);
* no W record (a poke is not this trigger) and real pad input on both sides.

The raw counter can cross 6 in the same iteration that enters the mode and
resets it, so the boundary snapshot need not show the crossing; the entry
stores, not an impossible sampled peak, close the gate claim. The S/T formats
carry no text row, so the decoded string 0x61 ("BOWLING!") and the banner's
pixels are the port decoder's and gp_compare's claims, not something this state
checker invents from the poll log.

The claim is the measured trigger and the mode entry/content inside the captured
window. The later scoreboard is not claimed.
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gp_session as gs

SCENARIO = 'gp-bowling'
STAGGERED_SCENARIO = 'gp-bowling-2'
MODE25 = 0x25
REACTION = 0x22
# The third pair's increments are visible; the crossing increment can be hidden.
VISIBLE_MIN = 4
STATE9_ENTRIES_MIN = 3
FIELDS = ('b892', 'b8c4', 'b8a6', 'b8bd', 'b415', 'b8c0')
ENTRY_VALUES = {
    'b1f': 3, 'c0': 4, 'c1': 4, 'b25': 0,
    'b892': 0,                  # 0x4E1CB resets the armed counter
    'b8c4': 1, 'b8a6': 0xB4, 'b8bd': 0, 'b415': 0, 'b8c0': 1,
}
# Final truth Task 5 (record §F5). The staggered route plays five move-0
# attempts; the three-attempt route's last visible b892 was 5, and a fourth
# attempt's measured +1..+2 puts the clear frame's count at 6..7, so the
# visible maximum must reach 6. An increment is one 0x45158 call per side per
# frame (inc word 0x451C6), so no single frame raises the word by more than 2.
# The two prior captures measured exactly two state-9 rising edges per attempt
# per side (2 for one attempt, 6 for three), so five attempts bound the edges
# at 10; the live-truth minimum of 3 stays as the "the callback ran" floor.
STAGGERED_VISIBLE_MIN = 6
STAGGERED_MAX_STEP = 2
STATE9_ENTRIES_MAX = 10
CAM_LIMIT = 0x3300
STAGGERED_FIELDS = ('s0_53', 'camx')


def _fail(name, out, detail):
    out('gp_bowling: %s: evidence: FAIL: %s' % (name, detail))
    return 1


def _s32(v):
    """A 32-bit value as the raw's signed compares see it (camx, 0x4E143)."""
    return v - 0x100000000 if v & 0x80000000 else v


def _entry_missing(name, out, snaps, fs):
    """The no-mode-0x25 verdict, with the measured residue when the appended
    fields are present (record §F5.4): the count's maximum and the camera."""
    mode6 = [f for f in fs if snaps[f].get('mode') == 6]
    if not mode6:
        return _fail(name, out, 'mode 0x25 never observed')
    peak = max(snaps[f].get('b892', 0) for f in mode6)
    text = 'mode 0x25 never observed (b892 max %d in mode 6)' % peak
    cams = [snaps[f] for f in mode6 if 'camx' in snaps[f]]
    if cams:
        signed = [_s32(c['camx']) for c in cams]
        text += ', camx %+d..%+d (gate [-%d, %d])' % (min(signed), max(signed),
                                                     CAM_LIMIT, CAM_LIMIT)
    else:
        text += ', camx not sampled (pre-Task-5 capture)'
    return _fail(name, out, text)


def evidence(name, lines, out=print):
    """The capture-only trigger and entry check. Returns 0 ok, 1 FAIL."""
    parsed = [r for r in (gs.parse(line) for line in lines) if r]
    if any(r['kind'] == 'W' for r in parsed):
        return _fail(name, out, 'a poke (W record) substituted for the real-input trigger')
    snaps = gs.snapshots(lines)
    fs = sorted(snaps)
    entry_f = next((f for f in fs if snaps[f].get('mode') == MODE25), None)
    if entry_f is None:
        return _entry_missing(name, out, snaps, fs)
    entry = snaps[entry_f]
    missing = [n for n in FIELDS if any(n not in snaps[f] for f in fs)]
    if missing:
        return _fail(name, out, 'the S records lack the appended mode-0x25 fields: %s'
                     % ', '.join(missing))
    before = [f for f in fs if f < entry_f]
    if not before:
        return _fail(name, out, 'mode 0x25 at f=%X has no preceding snapshot' % entry_f)

    previous = {f: fs[i - 1] for i, f in enumerate(fs) if i}
    reactions = {}
    for side, field in enumerate(('r0', 'r1')):
        reactions[field] = next((f for f in before
                                 if snaps[f]['mode'] == 6 and snaps[f].get(field) == REACTION), None)
        if reactions[field] is None:
            return _fail(name, out, '%s never applied reaction 0x22 in the fight before f=%X'
                         % (field, entry_f))
        state = 's%d_52' % side
        entries = [f for f in before if snaps[f]['mode'] == 6
                   and snaps[f].get(field) == REACTION and snaps[f].get(state) == 9
                   and f in previous and snaps[previous[f]].get(state) != 9]
        if len(entries) < STATE9_ENTRIES_MIN:
            return _fail(name, out, '%s/%s shows %d callback-animation state-9 entries, '
                         'want at least three' % (field, state, len(entries)))
    series = [(f, snaps[f]['b892']) for f in before if snaps[f]['mode'] == 6]
    rejections = 0
    for (f0, v0), (f1, v1) in zip(series, series[1:]):
        if v1 >= v0:
            continue
        if v0 >= 6 and v1 == v0 - 1 and rejections == 0:
            rejections += 1
            continue
        return _fail(name, out, 'b892 is not non-decreasing apart from one -1 after >=6: '
                     '%d -> %d at f=%X' % (v0, v1, f1))
    armed = max((v for _, v in series), default=0)
    if armed < VISIBLE_MIN:
        return _fail(name, out, 'b892 reached %d, not >= 4 (only the increment crossing 6 '
                         'can be hidden by entry/reset)' % armed)
    if (not any(snaps[f]['b8c4'] == 0 for f in before)
            or any(snaps[f]['b8c4'] != 0 for f in before)):
        return _fail(name, out, 'b8c4 is not the required 0 -> 1 transition at f=%X' % entry_f)

    pad_sides = {int(r['press'][1]) for r in parsed
                 if r['kind'] == 'I' and r.get('press', '')[:2] in ('p1', 'p2')
                 and r.get('f', entry_f) < entry_f}
    if pad_sides != {1, 2}:
        return _fail(name, out, 'the trigger lacks real pad input on both sides (sides %r)'
                     % sorted(pad_sides))
    if entry['b1d'] in (2, 3):
        return _fail(name, out, 'b1d=%d at f=%X; fight_mode25_enter excludes 2 and 3'
                     % (entry['b1d'], entry_f))
    for field, want in ENTRY_VALUES.items():
        got = entry[field]
        if got != want:
            return _fail(name, out, '%s=%X at the mode-0x25 entry f=%X, want %X (0x4E11C)'
                         % (field, got, entry_f, want))

    out('gp_bowling: %s: evidence: trigger b892=%d (non-decreasing apart from one '
        'rejection; the crossing increment can be hidden), r0=r1=0x22 at f=%X/%X, '
        'at least three state-9 callback-animation entries per side, real pad input on both '
        'sides, no poke ok' % (name, armed, reactions['r0'], reactions['r1']))
    out('gp_bowling: %s: evidence: mode 0x25 at f=%X: b1f=3, b1d not 2/3, '
        'c0=c1=4, b25=0, b8c4 0 -> 1, b8a6=0xB4, b8bd=b415=0, b8c0=1 ok'
        % (name, entry_f))
    return 0


def evidence_staggered(name, lines, out=print):
    """Final truth Task 5's five-attempt route check (record §F5). Returns 0 ok, 1 FAIL."""
    parsed = [r for r in (gs.parse(line) for line in lines) if r]
    if any(r['kind'] == 'W' for r in parsed):
        return _fail(name, out, 'a poke (W record) substituted for the real-input trigger')
    snaps = gs.snapshots(lines)
    fs = sorted(snaps)
    entry_f = next((f for f in fs if snaps[f].get('mode') == MODE25), None)
    if entry_f is None:
        return _entry_missing(name, out, snaps, fs)
    entry = snaps[entry_f]
    missing = [n for n in FIELDS + STAGGERED_FIELDS if any(n not in snaps[f] for f in fs)]
    if missing:
        return _fail(name, out, 'the S records lack the appended mode-0x25 fields: %s'
                     % ', '.join(missing))
    before = [f for f in fs if f < entry_f]
    if not before:
        return _fail(name, out, 'mode 0x25 at f=%X has no preceding snapshot' % entry_f)

    # The reaction callback ran repeatedly on both sides: 3..10 state-9 rising
    # edges per side (the two prior captures measured two per attempt per side).
    previous = {f: fs[i - 1] for i, f in enumerate(fs) if i}
    reactions = {}
    edges = {}
    for side, field in enumerate(('r0', 'r1')):
        reactions[field] = next((f for f in before
                                 if snaps[f]['mode'] == 6 and snaps[f].get(field) == REACTION), None)
        if reactions[field] is None:
            return _fail(name, out, '%s never applied reaction 0x22 in the fight before f=%X'
                         % (field, entry_f))
        state = 's%d_52' % side
        edges[state] = [f for f in before if snaps[f]['mode'] == 6
                        and snaps[f].get(field) == REACTION and snaps[f].get(state) == 9
                        and f in previous and snaps[previous[f]].get(state) != 9]
        if len(edges[state]) < STATE9_ENTRIES_MIN:
            return _fail(name, out, '%s/%s shows %d callback-animation state-9 entries, '
                         'want at least three' % (field, state, len(edges[state])))
        if len(edges[state]) > STATE9_ENTRIES_MAX:
            return _fail(name, out, '%s/%s shows %d callback-animation state-9 entries, '
                         'more than the route\'s %d attempts x 2 (measured per attempt)'
                         % (field, state, len(edges[state]), STATE9_ENTRIES_MAX // 2))

    # The counter: increases <= 2 per frame (the two hooks), decreases only the
    # rejection arm's -1 from >= 6 at a frame whose s0_53 is 0 (the 0x263DE gate).
    series = [(f, snaps[f]['b892']) for f in before if snaps[f]['mode'] == 6]
    rejections = 0
    for (f0, v0), (f1, v1) in zip(series, series[1:]):
        if v1 > v0:
            if v1 - v0 > STAGGERED_MAX_STEP:
                return _fail(name, out, 'b892 rose %d -> %d at f=%X: more than the two '
                             '0x45158 hooks can add in one frame' % (v0, v1, f1))
            continue
        if v1 == v0:
            continue
        if v0 >= 6 and v1 == v0 - 1 and snaps[f1].get('s0_53') == 0:
            rejections += 1
            continue
        return _fail(name, out, 'b892 fell %d -> %d at f=%X without the rejection arm\'s '
                     '>= 6 / s0_53=0 shape' % (v0, v1, f1))
    armed = max((v for _, v in series), default=0)
    if armed < STAGGERED_VISIBLE_MIN:
        return _fail(name, out, 'b892 reached %d, not >= %d: the fourth attempt\'s '
                     'increments never landed (the three-attempt route measured 5)'
                     % (armed, STAGGERED_VISIBLE_MIN))
    if not any(snaps[f].get('s0_53') == 7 for f in before if snaps[f]['mode'] == 6):
        return _fail(name, out, 's0_53 never reads 7 before f=%X: the reaction window the '
                     'counter is counted in never existed' % entry_f)
    if (not any(snaps[f]['b8c4'] == 0 for f in before)
            or any(snaps[f]['b8c4'] != 0 for f in before)):
        return _fail(name, out, 'b8c4 is not the required 0 -> 1 transition at f=%X' % entry_f)
    if entry['s0_53'] != 0:
        return _fail(name, out, 's0_53=%X at the mode-0x25 entry f=%X, want 0 (0x263DE)'
                     % (entry['s0_53'], entry_f))
    cam = _s32(entry['camx'])
    if cam < -CAM_LIMIT or cam > CAM_LIMIT:
        return _fail(name, out, 'camx=%+d at the mode-0x25 entry f=%X, outside [-%d, %d] '
                     '(0x4E13D..0x4E151)' % (cam, entry_f, CAM_LIMIT, CAM_LIMIT))

    pad_sides = {int(r['press'][1]) for r in parsed
                 if r['kind'] == 'I' and r.get('press', '')[:2] in ('p1', 'p2')
                 and r.get('f', entry_f) < entry_f}
    if pad_sides != {1, 2}:
        return _fail(name, out, 'the trigger lacks real pad input on both sides (sides %r)'
                     % sorted(pad_sides))
    if entry['b1d'] in (2, 3):
        return _fail(name, out, 'b1d=%d at f=%X; fight_mode25_enter excludes 2 and 3'
                     % (entry['b1d'], entry_f))
    for field, want in ENTRY_VALUES.items():
        got = entry[field]
        if got != want:
            return _fail(name, out, '%s=%X at the mode-0x25 entry f=%X, want %X (0x4E11C)'
                         % (field, got, entry_f, want))

    out('gp_bowling: %s: staggered: b892 max %d (increases <= 2/frame, %d rejection '
        'decrement(s) from >= 6 with s0_53=0), r0=0x22 at f=%X, r1=0x22 at f=%X, '
        'state-9 edges %d/%d, no poke ok'
        % (name, armed, rejections, reactions['r0'], reactions['r1'],
           len(edges['s0_52']), len(edges['s1_52'])))
    out('gp_bowling: %s: staggered: mode 0x25 at f=%X: s0_53=0, camx=%+d within '
        '[-%d, %d], b1f=3, b1d not 2/3, c0=c1=4, b25=0, b8c4 0 -> 1, b8a6=0xB4, '
        'b8bd=b415=0, b8c0=1 ok'
        % (name, entry_f, cam, CAM_LIMIT, CAM_LIMIT))
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('cmd', choices=('check',))
    ap.add_argument('--capture', required=True)
    ap.add_argument('--staggered', action='store_true',
                    help='final truth Task 5\'s five-attempt route rules (record §F5)')
    ap.add_argument('--scenario', default=None,
                    help='the scenario name to pin (default %s for --staggered)'
                         % STAGGERED_SCENARIO)
    a = ap.parse_args()
    want = a.scenario or (STAGGERED_SCENARIO if a.staggered else None)
    if want is not None and os.path.basename(os.path.normpath(a.capture)) != want:
        print('gp_bowling: capture %s is not the %s capture' % (a.capture, want))
        return 1
    path = os.path.join(a.capture, 'poll.log')
    if not os.path.isfile(path):
        print('gp_bowling: no capture at %s (skipped)' % path)
        return 0
    with open(path) as f:
        lines = f.read().splitlines()
    check = evidence_staggered if a.staggered else evidence
    return check(os.path.basename(os.path.normpath(a.capture)), lines)


if __name__ == '__main__':
    sys.exit(main())
