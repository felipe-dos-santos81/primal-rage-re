#!/usr/bin/env python3
"""U16 R2: read the `s0_54` set/exit transition of a gp-u24-u16-state poll.log
(plan 2026-10-27-claim-coverage Task 10; record 2026-10-26-fought-match-truth
§F5.4-§F5.6).

The `0x4844C` case 3 reaction sets slot 0's `+0x54` to 2 (`0x48374`'s D500 arm)
and clears it when the gate at `0x48599` (`cmp edx,[eax+0x30]`; `0x4859C jle`)
sees the previous iteration's latched y below `0x1400`. The marker is therefore a
`s0_54` 2 -> 0 pair after the first mode-9 entry, and the deciding input is
`s0_30` (the latched y) at the exit's previous frame, not at the exit itself
(the port's own exit at `0x155C` follows `0x155B`'s `0x1104` while `0x155A`'s
`0x147E` is still above).

`gp-u24-u16-state`'s published capture does not exist; the recovery artifact is
`gp-u24-u16-state.failed` and the poller missed the set frame (`0x1532` is one
of its 347 missed frames), so its log has no 2 -> 0 transition. When no set
record exists the reader falls back to the old U23 capture's marker pair
(`0x1532`/`0x1533`, record §F5.6) to name the presumed exit, but only if that
frame is inside a mode-9 record: a capture that never entered the finisher path
(the first run, `.failed.run1`, whose `0x1533` is mode 6) then reports no marker
instead of a bogus exit.

The values are reported, never asserted: an absent set frame is this diagnostic's
known measurement condition, not a failure. The reader also accepts a port
`trace.txt`'s `T` records, so the same marker compares the replay's side.

  gp_u16_state.py read --capture DIR [--marker-set N] [--marker-exit N]
  gp_u16_state.py path --capture DIR          one line per s0_54 change in mode 9

Stdlib only."""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gp_session as gs

SCENARIO = 'gp-u24-u16-state'
GATE_Y = 0x1400          # 0x48599/0x4859C: the reaction ends while 0x1400 > slot+0x30
# The old U23 capture's marker pair (record §F5.6): its s0_54 2 -> 0 frames. The
# set frame is the gate's own input frame (the previous iteration's latched y).
MARKER_SET, MARKER_EXIT = 0x1532, 0x1533


def rows(lines):
    """{f: record} for the S (poll.log) and T (port trace) records, first per frame."""
    out = {}
    for line in lines:
        r = gs.parse(line)
        if r and r['kind'] in ('S', 'T'):
            out.setdefault(r['f'], r)
    return out


def _state(r):
    """The marker's three fields at one record (None when the log predates them)."""
    if r is None or 's0_54' not in r:
        return None
    return dict(frame=r['f'], s0_54=r.get('s0_54'), s0_30=r.get('s0_30'), s0_42=r.get('s0_42'))


def _fmt_state(s):
    if s is None:
        return 'no record'
    parts = ['f=%X' % s['frame']]
    for n in ('s0_54', 's0_30', 's0_42'):
        if s[n] is None:
            return ' '.join(parts) + ' (%s fields absent)' % n
        parts.append('%s=0x%X' % (n, s[n]))
    return ' '.join(parts)


def _relation(y):
    """The 0x4859C form: `0x1400 > y` exits, so equality does not."""
    return 'below' if y < GATE_Y else 'at' if y == GATE_Y else 'above'


def read(lines, marker=(MARKER_SET, MARKER_EXIT)):
    """The marker as a dict. The transition is discovered after the first mode-9
    entry (the plan's marker script); `marker` only names the exit when the set
    record is missing, and only a mode-9 record there is accepted."""
    snap = rows(lines)
    fs = sorted(snap)
    res = dict(frames=len(fs), has_54=False, has_state=False, mode9=None,
               set_frame=None, exit_frame=None, set_missing=False, anchored=False,
               set_state=None, exit_state=None, gate_frame=None, gate_state=None,
               gate_relation=None, before=None, after=None, marker=tuple(marker),
               marker_set_present=False)
    if not fs:
        return res
    res['has_54'] = 's0_54' in snap[fs[0]]
    res['has_state'] = 's0_30' in snap[fs[0]] and 's0_42' in snap[fs[0]]
    if not res['has_54']:
        return res
    f9 = next((f for f in fs if snap[f]['mode'] == 0x09), None)
    res['mode9'] = f9
    if f9 is None:
        return res

    # The reaction's own set: mode 9 only, so a later screen's +0x54 is not read
    # as the reaction's (record §F5.4 requires it to follow the first mode-9 entry).
    set_f = next((f for f in fs if f > f9 and snap[f]['mode'] == 0x09
                  and snap[f]['s0_54'] == 2), None)
    if set_f is not None:
        res['set_frame'] = set_f
        res['set_state'] = _state(snap[set_f])
        res['exit_frame'] = next((f for f in fs if f > set_f and snap[f]['mode'] == 0x09
                                  and snap[f]['s0_54'] == 0), None)
    else:
        res['set_missing'] = True
        m_set, m_exit = marker
        res['marker_set_present'] = m_set in snap
        if m_exit in snap and m_exit > f9 and snap[m_exit]['mode'] == 0x09:
            res['exit_frame'] = m_exit
            res['anchored'] = True

    ef = res['exit_frame']
    if ef is not None:
        res['exit_state'] = _state(snap[ef])
        res['gate_frame'] = ef - 1
        res['gate_state'] = _state(snap.get(ef - 1))
        if res['gate_state'] is not None and res['gate_state']['s0_30'] is not None:
            res['gate_relation'] = _relation(res['gate_state']['s0_30'])

    base = res['set_frame'] if res['set_frame'] is not None else res['exit_frame']
    if base is not None:
        before = [f for f in fs if f < base]
        if before:
            res['before'] = _state(snap[before[-1]])
    if ef is not None:
        after = [f for f in fs if f > ef]
        if after:
            res['after'] = _state(snap[after[0]])
    return res


def report(res):
    """[(label, detail)] lines for one `read` result."""
    out = []
    if not res['frames']:
        return [('poll.log', 'no S/T records')]
    if not res['has_54']:
        return [('poll.log', 'not a post-fought-match-truth capture: the S records have no s0_54')]
    out.append(('mode 9 entry', 'none' if res['mode9'] is None else 'f=%X' % res['mode9']))
    if res['mode9'] is None:
        return out
    if res['set_missing']:
        m_set = res['marker'][0]
        out.append(('set frame missing',
                    'no mode-9 S record holds s0_54 = 2; the old U23 marker\'s set frame '
                    'f=%X is %s' % (m_set, 'present but not the set' if res['marker_set_present']
                                    else 'not in the log (a missed frame)')))
    elif res['set_frame'] is not None:
        out.append(('set frame', 'f=%X' % res['set_frame']))
        out.append(('set state', _fmt_state(res['set_state'])))
    if res['exit_frame'] is None:
        out.append(('exit frame missing',
                    'no mode-9 S record after the set holds s0_54 = 0'
                    if not res['set_missing'] else
                    "the marker's exit f=%X is not a mode-9 S record after the entry"
                    % res['marker'][1]))
    else:
        label = 'exit frame'
        if res['anchored']:
            label = "exit frame (the old U23 marker's, §F5.6)"
        out.append((label, 'f=%X' % res['exit_frame']))
        out.append(('exit state', _fmt_state(res['exit_state'])))
        gf = res['gate_frame']
        if not res['has_state']:
            out.append(('gate input y(%X)' % gf, 'unreadable: the S records have no s0_30'))
        elif res['gate_state'] is None:
            out.append(('gate input y(%X)' % gf,
                        'absent: no S record at f=%X (the previous iteration was not sampled)' % gf))
        else:
            out.append(('gate input y(%X)' % gf,
                        'present: s0_30=0x%X (%s 0x%X)'
                        % (res['gate_state']['s0_30'], res['gate_relation'], GATE_Y)))
    if res['before'] is not None:
        out.append(('last S before the marker', _fmt_state(res['before'])))
    if res['after'] is not None:
        out.append(('first S after the exit', _fmt_state(res['after'])))
    return out


def path(lines):
    """One line per s0_54 change inside a mode-9 record: f=.. s0_54=.. (diagnostic)."""
    snap = rows(lines)
    fs = sorted(snap)
    out, prev = [], None
    for f in fs:
        if snap[f]['mode'] != 0x09 or 's0_54' not in snap[f]:
            continue
        v = snap[f]['s0_54']
        if v != prev:
            out.append('f=%X s0_54=%02X' % (f, v))
            prev = v
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('cmd', choices=('read', 'path'))
    ap.add_argument('--capture', required=True, help='a capture directory (or a port dump '
                    'directory holding poll.log/trace.txt)')
    ap.add_argument('--marker-set', default=None,
                    help='the s0_54 = 2 frame the incomplete case falls back to (default 0x%X)'
                         % MARKER_SET)
    ap.add_argument('--marker-exit', default=None,
                    help='the s0_54 = 0 exit frame the incomplete case falls back to (default 0x%X)'
                         % MARKER_EXIT)
    a = ap.parse_args()
    src = os.path.join(a.capture, 'poll.log')
    if not os.path.isfile(src):
        src = os.path.join(a.capture, 'trace.txt')
    if not os.path.isfile(src):
        print('gp_u16_state: no poll.log or trace.txt at %s' % a.capture)
        return 2
    with open(src) as f:
        lines = f.read().splitlines()
    if a.cmd == 'path':
        for row in path(lines):
            print(row)
        return 0
    marker = (int(a.marker_set, 0) if a.marker_set not in (None, '') else MARKER_SET,
              int(a.marker_exit, 0) if a.marker_exit not in (None, '') else MARKER_EXIT)
    res = read(lines, marker)
    if not res['frames'] or not res['has_54']:
        for label, detail in report(res):
            print('%s: %s' % (label, detail))
        return 2
    print('gp_u16_state: %s: %s' % (SCENARIO, os.path.basename(src)))
    for label, detail in report(res):
        print('  %s: %s' % (label, detail))
    return 0


if __name__ == '__main__':
    sys.exit(main())
