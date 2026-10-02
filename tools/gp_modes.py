#!/usr/bin/env python3
"""Gameplay U8: the evidence that a capture reached its START MENU row or the
attract start (plan docs/superpowers/plans/2026-10-01-gameplay-u8-other-modes.md,
record docs/superpowers/plans/2026-10-01-gameplay-u8-derivations.md §U8.1-§U8.4).

ROWS is derived from the raw: the START MENU table 0xBCCCC (items 0xBCCDC..0xBCD3C,
+8 the setter), the setters 0x2CBC4..0x2CC6A (`mov edx,M; xor ah,ah | mov ah,N;
mov [0x104B00],dx; mov [0x104B1D],ah`), the mode switch 0x24EEC..0x24F01
(`jmp [eax*4+0x24B8C]`), the game-start handlers 0x24F09..0x25199 (0x2CA7C(1)
only in modes 0x2D/0x2E at 0x250BA/0x25117; then 0x257A4(mask), which stores
DS_00104B1F at 0x257E0 and arms the wipe 0x4F980 to mode 0x10 at 0x25816) and
the attract start 0x11D04 (0x11D13..0x11D41: 0x11F28(side) spends one credit
at 0x11F47, then 0x257A4(side mask)).

  gp_modes.py check --scenario NAME --capture DIR   the checks; exit 1 on a FAIL
  gp_modes.py path --capture DIR                     the mode path, one line per change
Stdlib only."""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gp_session as gs

START_ENTRY = 0xBCCDC            # START MENU's item list (0x2FFC4 init: DS_0010741C = table + 0x10)
GAME_START_MODES = range(0x28, 0x30)   # jump-table entries 0x24F09..0x2512B
WIPE_MODE = 0x1A                 # 0x4F980 stores mode 0x1A (0x4F989..0x4F994)
CHARSEL_MODE = 0x10              # 0x257A4: 0x2580B mov eax,0x10; 0x25816 call 0x4F980

# scenario -> the raw's expectation. mode: the setter's mode (None: the attract
# start); b1d: the setter's DS_00104B1D; b1f: 0x257A4's argument; spend: the
# credits 0x2CA7C/0x11F28 take; reach: the mode the scenario ends in; stay: the
# scenario must end still in `reach` (ENDURANCE's team select has no time-out).
ROWS = {
    'gp-idle-loss': dict(row=0, mode=0x2D, b1d=0, b1f=1, spend=1, reach=6, stay=False),   # U4's, the reference
    'gp-u8-right-arcade': dict(row=1, mode=0x2E, b1d=0, b1f=2, spend=1, reach=6, stay=False),
    'gp-u8-left-training': dict(row=2, mode=0x28, b1d=1, b1f=3, spend=0, reach=6, stay=False),
    'gp-u8-right-training': dict(row=3, mode=0x29, b1d=1, b1f=3, spend=0, reach=6, stay=False),
    'gp-u8-tug-of-war': dict(row=4, mode=0x2A, b1d=2, b1f=3, spend=0, reach=6, stay=False),
    'gp-u8-endurance': dict(row=5, mode=0x2B, b1d=3, b1f=3, spend=0, reach=0x10, stay=True),
    'gp-u8-handicap': dict(row=6, mode=0x2C, b1d=4, b1f=3, spend=0, reach=6, stay=False),
    'gp-u8-attract-start': dict(row=None, mode=None, b1d=0, b1f=1, spend=1, reach=6, stay=False),
}


def check(name, lines):
    """[(label, ok, detail)] for ROWS[name] over a poll.log (record §U8.5)."""
    want = ROWS[name]
    recs = [r for r in (gs.parse(l) for l in lines) if r]
    base = next((r['base'] for r in recs if r['kind'] == 'B'), None)
    ms = [(i, r) for i, r in enumerate(recs) if r['kind'] in ('S', 'P') and r.get('mode') is not None]
    out = []
    if want['mode'] is None:
        out.append(('no mode 0x27 (no Enter)', all(r['mode'] != 0x27 for _, r in ms), ''))
        snap = gs.snapshots(lines)
        arm = next((f for f in sorted(snap) if gs.raw_to_kb(snap[f]['raw']) & 0x0100), None)
        ok = arm is not None and snap[arm]['mode'] == 3 and snap.get(arm - 1, {}).get('mode') == 3
        out.append(('the P1 start bit reached the bitmap in mode 3', ok,
                    'f=%s' % (None if arm is None else '%X' % arm)))
        sel = next((i for i, r in ms if arm is not None and r['f'] >= arm and r['mode'] != 3), None)
        ok = sel is not None and recs[sel]['mode'] == WIPE_MODE
        out.append(('the first mode after 3 is the wipe 0x1A', ok,
                    '' if sel is None else 'f=%X mode=%X' % (recs[sel]['f'], recs[sel]['mode'])))
    else:
        sel = next((i for i, r in ms if r['mode'] == want['mode']), None)
        out.append(('the row mode 0x%X appears' % want['mode'], sel is not None,
                    '' if sel is None else 'f=%X (%s)' % (recs[sel]['f'], recs[sel]['kind'])))
        other = sorted({r['mode'] for _, r in ms if r['mode'] in GAME_START_MODES and r['mode'] != want['mode']})
        out.append(('no other game-start mode', not other, ' '.join('%X' % m for m in other)))
        off = None if base is None else base - gs.DATA_BASE_VA
        opened = sel is not None and off is not None and any(
            r['kind'] == 'S' and r['mode'] == 0x27 and r['ent'] - off == START_ENTRY for i, r in ms if i < sel)
        out.append(('START MENU open (ent 0xBCCDC) before the select', opened, ''))
    before = [r for i, r in ms if sel is not None and i < sel and r['kind'] == 'S']
    after = next((r for i, r in ms if sel is not None and i > sel and r['kind'] == 'S'
                  and r['mode'] == WIPE_MODE), None)
    ok = after is not None and after['b1d'] == want['b1d'] and after['b1f'] == want['b1f']
    out.append(('the divert: b1d=%X b1f=%X' % (want['b1d'], want['b1f']), ok,
                '' if after is None else 'f=%X b1d=%X b1f=%X' % (after['f'], after['b1d'], after['b1f'])))
    ok = after is not None and bool(before) and before[-1]['cred'] - want['spend'] == after['cred']
    out.append(('credits spent %d' % want['spend'], ok,
                '' if not (after and before) else '%X -> %X' % (before[-1]['cred'], after['cred'])))
    ch = next((r for i, r in ms if sel is not None and i > sel and r['mode'] == CHARSEL_MODE), None)
    out.append(('the character select (mode 0x10) follows', ch is not None, '' if ch is None else 'f=%X' % ch['f']))
    reached = next((r for i, r in ms if sel is not None and i > sel and r['mode'] == want['reach']), None)
    out.append(('mode 0x%X reached' % want['reach'], reached is not None,
                '' if reached is None else 'f=%X' % reached['f']))
    xrec = next((r for r in recs if r['kind'] == 'X'), None)
    out.append(('the scenario end (X record)', xrec is not None, '' if xrec is None else 'f=%X' % xrec['f']))
    if want['stay']:
        last = [r for i, r in ms if reached is not None and r['kind'] == 'S'
                and xrec is not None and r['f'] <= xrec['f']]
        ok = bool(last) and all(r['mode'] == want['reach'] for r in last if r['f'] >= reached['f'])
        out.append(('still in mode 0x%X at the end' % want['reach'], ok,
                    '' if not last else 'last S f=%X mode=%X' % (last[-1]['f'], last[-1]['mode'])))
    return out


def path(lines):
    """One line per mode change of the S/P records (poll.log:<n> kind f mode), and
    the first S record of each new mode with cred, b1d, b1f, e0, e2 and both
    slots' +0x5A."""
    out, prev, prev_s = [], None, None
    for n, l in enumerate(lines, 1):
        r = gs.parse(l)
        if not r or r['kind'] not in ('S', 'P'):
            continue
        if r['kind'] == 'S' and r['mode'] != prev_s:
            out.append('poll.log:%d S f=%X mode=%X cred=%X b1d=%X b1f=%X e0=%X e2=%X s0_5a=%X s1_5a=%X' % (
                n, r['f'], r['mode'], r['cred'], r['b1d'], r['b1f'], r['e0'], r['e2'], r['s0_5a'], r['s1_5a']))
            prev = prev_s = r['mode']
        elif r['kind'] == 'P' and r['mode'] != prev:
            out.append('poll.log:%d P f=%X mode=%X' % (n, r['f'], r['mode']))
            prev = r['mode']
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('cmd', choices=('check', 'path'))
    ap.add_argument('--scenario')
    ap.add_argument('--capture', required=True)
    a = ap.parse_args()
    with open(os.path.join(a.capture, 'poll.log')) as f:
        lines = f.read().splitlines()
    if a.cmd == 'path':
        print('\n'.join(path(lines)))
        return 0
    res = check(a.scenario, lines)
    for label, ok, detail in res:
        print('gp_modes: %s: %s: %s%s' % (a.scenario, label, 'ok' if ok else 'FAIL', ' (%s)' % detail if detail else ''))
    return 0 if all(ok for _, ok, _ in res) else 1


if __name__ == '__main__':
    sys.exit(main())
