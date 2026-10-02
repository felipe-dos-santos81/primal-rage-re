#!/usr/bin/env python3
"""Gameplay U7 (two players): is a gp capture (or a port replay's trace) a
two-human match? Plan docs/superpowers/plans/2026-10-01-gameplay-u7-two-players.md,
record 2026-10-01-gameplay-u7-derivations.md §T.1-§T.2. Stdlib only.

  gp_twop.py check --capture DIR    exit 0 when DIR/poll.log is a two-human match
  gp_twop.py check --trace FILE     the same over a port trace.txt (T records)
                                    (a trace is converted only when the file is named
                                    trace.txt; any other name is read as a poll.log
                                    and finds no S records)
  gp_twop.py path  --capture DIR    the mode path with b1f, cred, e0 and e2

The raw (record §T.1): DS_00104B1F (`b1f`) holds one bit per human side.
0x257A4 stores its argument there (1 for LEFT PLAYER ARCADE, 0x250C4) and the
character select ORs in side + 1 when 0x11F28(side) accepts a start
(0x43B65..0x43B71). With b1f == 3 the versus hook 0x430E8 skips 0x41350
(0x43136 `cmp eax,3; je`), the routine that marks a CPU slot (+0x63 = 1,
0x41385). A human slot's command word (DS_001088E0[side], logged e0/e2) is
written only by 0x4F644 from the pads (0x4F6BD/0x4F6DE), by 0x24C96 (0, while
the slot's +0x41 has bit 0x10) and by 0x246D4 (character 1's entrance in mode
5: 0xA000/0x9000, 0x246EE/0x246F9). The CPU's writers, 0x472CA/0x472FF/0x47325
(0x47208) and 0x3B207..0x3B278 (0x3B134), run only for a slot whose +0x63 is
non-zero (0x472B9, 0x3B170). A join must not debit the credit counter: 0x2CA93
skips the debit 0x2CA9C when b1f != 0 (record §T.1.3). The check runs up to the scenario's X record (the
capture runs on to its time limit with no input)."""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gp_session as gs

ENTRANCE_WORDS = (0xA000, 0x9000)     # 0x246EE / 0x246F9 (mode 5 only)
FIGHT_MODE = 6                        # the fight frame (spec §3.9 item 1)


def pad_word(new, held, side):
    """0x4F644: DS_001088E0 = (new >> 24) | (held >> 16 & 0xFF00) for side 0,
    DS_001088E2 = (new >> 8 & 0xFF) | (held & 0xFF00) for side 1."""
    if side == 0:
        return (new >> 24) & 0xFF | (held >> 16) & 0xFF00
    return (new >> 8) & 0xFF | held & 0xFF00


def classify(r, side):
    """Who wrote this S record's command word for `side`: 'pad', 'zero',
    'entrance' or 'other' (a writer a human slot never runs)."""
    e = r['e2'] if side else r['e0']
    if e == pad_word(r['new'], r['held'], side):
        return 'pad'
    if e == 0:
        return 'zero'
    if r['mode'] == 5 and e in ENTRANCE_WORDS:
        return 'entrance'
    return 'other'


def load(path):
    """poll.log lines, or a port trace.txt with its T records read as S records."""
    with open(path) as f:
        lines = f.read().splitlines()
    if os.path.basename(path) == 'trace.txt':
        lines = ['S' + l[1:] for l in lines if l.startswith('T ')]
    return lines


def two_human(lines):
    """The evidence that a log is a two-human match, up to its X record (all
    records when it has none). Returns a dict: join (the first f with b1f == 3),
    end (the X record's f or None), frames (S records from the join to the end),
    counts ({class: n} per side), pressed (fight-mode S records with a non-zero
    pad word, per side), cred_before/cred_join, and fail (the reasons it is not)."""
    xrec = next((r for r in (gs.parse(l) for l in lines) if r and r['kind'] == 'X'), None)
    end = None if xrec is None else xrec['f']
    snap = gs.snapshots(lines)
    fs = sorted(f for f in snap if end is None or f <= end)
    out = dict(join=None, end=end, frames=0, counts=[{}, {}], pressed=[0, 0],
               cred_before=None, cred_join=None, fail=[])
    join = next((f for f in fs if snap[f]['b1f'] == 3), None)
    if join is None:
        out['fail'].append('b1f never reaches 3 (no S record has both sides human)')
        return out
    out['join'] = join
    out['cred_join'] = snap[join]['cred']
    before = [f for f in fs if f < join]
    if before:
        out['cred_before'] = snap[before[-1]]['cred']
        if out['cred_join'] != out['cred_before']:
            out['fail'].append('credit %X -> %X at the join f=%X: P2 joining must not debit (0x2CA93 skips 0x2CA9C when b1f != 0)'
                               % (out['cred_before'], out['cred_join'], join))
    for f in fs:
        if f < join:
            continue
        r = snap[f]
        out['frames'] += 1
        if r['b1f'] != 3 and not any('b1f' in x for x in out['fail']):
            out['fail'].append('b1f=%X at f=%X after the join at f=%X' % (r['b1f'], f, join))
        for side in (0, 1):
            c = classify(r, side)
            out['counts'][side][c] = out['counts'][side].get(c, 0) + 1
            e = r['e2'] if side else r['e0']
            if c == 'other' and not any('side %d' % side in x for x in out['fail']):
                out['fail'].append('side %d command word %04X at f=%X (mode %X) is not a pad word: a CPU wrote it'
                                   % (side, e, f, r['mode']))
            if r['mode'] == FIGHT_MODE and c == 'pad' and e != 0:
                out['pressed'][side] += 1
    for side in (0, 1):
        if out['pressed'][side] == 0:
            out['fail'].append('side %d never pressed a key in the fight (mode %d)' % (side, FIGHT_MODE))
    return out


def mode_path(lines):
    """[(line number, f, mode, b1f, cred, e0, e2)] at the first S record of each
    mode change (a P record may see the change earlier, without these fields)."""
    out, prev = [], None
    for n, l in enumerate(lines, 1):
        r = gs.parse(l)
        if r and r['kind'] == 'S' and r['mode'] != prev:
            out.append((n, r['f'], r['mode'], r['b1f'], r['cred'], r['e0'], r['e2']))
            prev = r['mode']
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('cmd', choices=('check', 'path'))
    src = ap.add_mutually_exclusive_group(required=True)
    src.add_argument('--capture')
    src.add_argument('--trace')
    a = ap.parse_args()
    path = os.path.join(a.capture, 'poll.log') if a.capture else a.trace
    lines = load(path)
    name = os.path.basename(os.path.normpath(a.capture)) if a.capture else path
    if a.cmd == 'path':
        for n, f, m, b1f, cred, e0, e2 in mode_path(lines):
            print('%s:%d f=%X mode=%X b1f=%X cred=%X e0=%04X e2=%04X'
                  % (os.path.basename(path), n, f, m, b1f, cred, e0, e2))
        return 0
    r = two_human(lines)
    if r['join'] is not None:
        print('gp_twop: %s: join f=%X (cred %s -> %X); %d S records from the join to %s; '
              'side 0 %s, side 1 %s; fight presses %d/%d'
              % (name, r['join'], '?' if r['cred_before'] is None else '%X' % r['cred_before'],
                 r['cred_join'], r['frames'], 'the end' if r['end'] is None else 'X f=%X' % r['end'],
                 sorted(r['counts'][0].items()), sorted(r['counts'][1].items()),
                 r['pressed'][0], r['pressed'][1]))
    for x in r['fail']:
        print('gp_twop: %s: FAIL: %s' % (name, x))
    if r['fail']:
        return 1
    print('gp_twop: %s: two-human match: ok' % name)
    return 0


if __name__ == '__main__':
    sys.exit(main())
