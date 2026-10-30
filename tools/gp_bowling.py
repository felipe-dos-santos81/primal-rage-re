#!/usr/bin/env python3
"""Live truth Task 6 (record 2026-10-29-live-truth-derivations.md §L6):
the mode-0x25 bowling entry, reached by real two-player input. Stdlib only.

  gp_bowling.py check --capture DIR

The checker requires both human sides on character 4 (ARMADON), three reaction-
0x22 entries per side, monotone DS_00108892 growth to a visible 4, and
fight_mode25_enter's stores at the first mode-0x25 snapshot (0x4E11C). The raw
counter reaches 6 in the same iteration that enters the mode and resets it, so
the boundary snapshot cannot show that peak; the entry stores, not an impossible
sampled 6, close the gate claim. Any W record rejects the run: a poke is not this
trigger. The S/T formats carry no text row, so the decoded string 0x61
("BOWLING!") and the banner's pixels are the port decoder's and gp_compare's
claims, not something this state checker invents from the poll log.

The claim is the measured trigger and the mode entry/content inside the captured
window. The later scoreboard is not claimed.
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gp_session as gs

SCENARIO = 'gp-bowling'
MODE25 = 0x25
REACTION = 0x22
VISIBLE_MIN = 4               # the third pair's +2 is reset in the entry iteration
REACTION_ENTRIES = 3
FIELDS = ('b892', 'b8c4', 'b8a6', 'b8bd', 'b415', 'b8c0')
ENTRY_VALUES = {
    'b1f': 3, 'c0': 4, 'c1': 4, 'b25': 0,
    'b892': 0,                  # 0x4E1CB resets the armed counter
    'b8c4': 1, 'b8a6': 0xB4, 'b8bd': 0, 'b415': 0, 'b8c0': 1,
}


def _fail(name, out, detail):
    out('gp_bowling: %s: evidence: FAIL: %s' % (name, detail))
    return 1


def evidence(name, lines, out=print):
    """The capture-only trigger and entry check. Returns 0 ok, 1 FAIL."""
    parsed = [r for r in (gs.parse(line) for line in lines) if r]
    if any(r['kind'] == 'W' for r in parsed):
        return _fail(name, out, 'a poke (W record) substituted for the real-input trigger')
    snaps = gs.snapshots(lines)
    fs = sorted(snaps)
    entry_f = next((f for f in fs if snaps[f].get('mode') == MODE25), None)
    if entry_f is None:
        return _fail(name, out, 'mode 0x25 never observed')
    entry = snaps[entry_f]
    missing = [n for n in FIELDS if any(n not in snaps[f] for f in fs)]
    if missing:
        return _fail(name, out, 'the S records lack the appended mode-0x25 fields: %s'
                     % ', '.join(missing))
    before = [f for f in fs if f < entry_f]
    if not before:
        return _fail(name, out, 'mode 0x25 at f=%X has no preceding snapshot' % entry_f)

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
                   and f - 1 in snaps and snaps[f - 1].get(state) != 9]
        if len(entries) < REACTION_ENTRIES:
            return _fail(name, out, '%s/%s shows %d reaction-0x22 entries, not three '
                         '(each attempt must re-enter the move)' % (field, state, len(entries)))
    series = [(f, snaps[f]['b892']) for f in before if snaps[f]['mode'] == 6]
    for (f0, v0), (f1, v1) in zip(series, series[1:]):
        if v1 < v0:
            return _fail(name, out, 'b892 is not monotone: %d -> %d at f=%X' % (v0, v1, f1))
    armed = max((v for _, v in series), default=0)
    if armed < VISIBLE_MIN:
        return _fail(name, out, 'b892 reached %d, not >= 4 (the entry/reset hides the '
                         'third pair\'s increment to 6)' % armed)
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

    out('gp_bowling: %s: evidence: trigger b892=%d (monotone; the peak 6 is not '
        'sampled), r0=r1=0x22 at f=%X/%X, three reaction entries per side, '
        'real pad input on both sides, no poke ok'
        % (name, armed, reactions['r0'], reactions['r1']))
    out('gp_bowling: %s: evidence: mode 0x25 at f=%X: b1f=3, b1d not 2/3, '
        'c0=c1=4, b25=0, b8c4 0 -> 1, b8a6=0xB4, b8bd=b415=0, b8c0=1 ok'
        % (name, entry_f))
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('cmd', choices=('check',))
    ap.add_argument('--capture', required=True)
    a = ap.parse_args()
    path = os.path.join(a.capture, 'poll.log')
    if not os.path.isfile(path):
        print('gp_bowling: no capture at %s (skipped)' % path)
        return 0
    with open(path) as f:
        lines = f.read().splitlines()
    return evidence(os.path.basename(os.path.normpath(a.capture)), lines)


if __name__ == '__main__':
    sys.exit(main())
