#!/usr/bin/env python3
"""Gameplay U7 (two players): is a gp capture (or a port replay's trace) a
two-human match? Plan docs/superpowers/plans/2026-10-01-gameplay-u7-two-players.md,
record 2026-10-01-gameplay-u7-derivations.md §T.1-§T.2. Stdlib only.

  gp_twop.py check --capture DIR    exit 0 when DIR/poll.log is a two-human match
  gp_twop.py check --join --capture DIR
                                    the mid-round join (0x28CC8, gp-u15-join): the
                                    capture ends inside the join's mode-0x17 window
                                    (0x28E75), so the check is the join's evidence
                                    (mode 6 -> 0x17, the joining side's start mask,
                                    b1f, no debit), not the two-human fight claim
  gp_twop.py check --finisher --capture DIR
                                    the two-human check plus the U16 finisher reaction
                                    and mode-0x09/mode-0x12 evidence
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

ENTRANCE_WORDS = (0xA000, 0x9000)      # 0x246EE / 0x246F9 (mode 5 only)
FIGHT_MODE = 6                         # the fight frame (spec §3.9 item 1)
JOIN_MODE = 0x17                       # 0x28DA4's DS_00104B00 store (0x28E3A/0x28E66)
START_MASK = (0x01000000, 0x00000100)  # DS_0009ACBC: side 0 F1/U, side 1 F2/Home level bits


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


def two_human(lines, join_only=False):
    """The evidence that a log is a two-human match, up to its X record (all
    records when it has none). Returns a dict: join (the first f with b1f == 3),
    end (the X record's f or None), frames (S records from the join to the end),
    counts ({class: n} per side), pressed (fight-mode S records with a non-zero
    pad word, per side), cred_before/cred_join, join_mode/before_mode/b1f_before/
    b1f_join, and fail (the reasons it is not).

    join_only checks the mid-round join instead of the two-human fight (the
    gp-u15-join capture): the S record before the join is the fight (mode 6) with one
    side human, the join's own record is the join window mode 0x17 (0x28DA4 stores it,
    0x28E3A/0x28E66), the joining side's start mask DS_0009ACBC[side] is newly pressed
    in DS_001088E4, and b1f stays 3 to the end; the credit check is shared (0x2CA93).
    The fight-press and command-word claims are skipped: the capture ends inside the
    join's 0x78-frame 0x17 window (0x28E75), where no fight frame runs and the CPU
    command block still writes the joining side's word (gp-keys-fight f=0x85A..0x85C:
    e2 = 0 while the pad holds 0x100)."""
    xrec = next((r for r in (gs.parse(l) for l in lines) if r and r['kind'] == 'X'), None)
    end = None if xrec is None else xrec['f']
    snap = gs.snapshots(lines)
    fs = sorted(f for f in snap if end is None or f <= end)
    out = dict(join=None, end=end, frames=0, counts=[{}, {}], pressed=[0, 0],
               cred_before=None, cred_join=None, join_mode=None, before_mode=None,
               b1f_before=None, b1f_join=None, fail=[])
    join = next((f for f in fs if snap[f]['b1f'] == 3), None)
    if join is None:
        out['fail'].append('b1f never reaches 3 (no S record has both sides human)')
        return out
    out['join'] = join
    out['cred_join'] = snap[join]['cred']
    out['join_mode'] = snap[join]['mode']
    out['b1f_join'] = snap[join]['b1f']
    before = [f for f in fs if f < join]
    if before:
        prev = snap[before[-1]]
        out['cred_before'] = prev['cred']
        out['before_mode'] = prev['mode']
        out['b1f_before'] = prev['b1f']
        if out['cred_join'] != out['cred_before']:
            out['fail'].append('credit %X -> %X at the join f=%X: P2 joining must not debit (0x2CA93 skips 0x2CA9C when b1f != 0)'
                               % (out['cred_before'], out['cred_join'], join))
    if join_only:
        if out['join_mode'] != JOIN_MODE:
            out['fail'].append('the join at f=%X is in mode %X, not the join window %X (0x28DA4 stores it)'
                               % (join, out['join_mode'], JOIN_MODE))
        if not before:
            out['fail'].append('the join at f=%X has no S record before it' % join)
        elif out['before_mode'] != FIGHT_MODE:
            out['fail'].append('the join at f=%X is not mid-round: the record before it is mode %X (want the fight mode %X)'
                               % (join, out['before_mode'], FIGHT_MODE))
        else:
            newly = out['b1f_before'] ^ out['b1f_join']
            side = {1: 0, 2: 1}.get(newly)
            if side is None:
                out['fail'].append('b1f %X -> %X at the join f=%X: not one side joining'
                                   % (out['b1f_before'], out['b1f_join'], join))
            elif (snap[join]['new'] & START_MASK[side]) == 0:
                out['fail'].append('the joining side %d start mask %08X is not newly pressed at f=%X (new %08X)'
                                   % (side, START_MASK[side], join, snap[join]['new']))
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
            if not join_only and c == 'other' and not any('side %d' % side in x for x in out['fail']):
                out['fail'].append('side %d command word %04X at f=%X (mode %X) is not a pad word: a CPU wrote it'
                                   % (side, e, f, r['mode']))
            if r['mode'] == FIGHT_MODE and c == 'pad' and e != 0:
                out['pressed'][side] += 1
    if not join_only:
        for side in (0, 1):
            if out['pressed'][side] == 0:
                out['fail'].append('side %d never pressed a key in the fight (mode %d)' % (side, FIGHT_MODE))
    return out


def finisher(lines):
    """The U16 finisher evidence: a two-human log reached modes 0x09 and 0x12 and
    contains a finisher reaction (0x32..0x34) in either S record move field."""
    records = [gs.parse(line) for line in lines]
    snapshots = [r for r in records if r and r['kind'] == 'S']
    mode9 = any(r['mode'] == 0x09 for r in snapshots)
    mode12 = any(r['mode'] == 0x12 for r in snapshots)
    reactions = [r[field] for r in snapshots for field in ('r0', 'r1')
                 if r.get(field) in (0x32, 0x33, 0x34)]
    fail = []
    if not reactions:
        fail.append('no finisher reaction 0x32..0x34 in any S record')
    if not mode9:
        fail.append('mode 0x09 not reached')
    if not mode12:
        fail.append('mode 0x12 not reached')
    return dict(mode9=mode9, mode12=mode12, reactions=reactions, fail=fail)


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
    check = ap.add_mutually_exclusive_group()
    check.add_argument('--join', action='store_true',
                       help="check the mid-round join only (0x28CC8/0x28DA4); the capture ends inside the join's mode-0x17 window")
    check.add_argument('--finisher', action='store_true',
                       help='check the U16 finisher reactions and match-end modes, as well as the two-human match')
    a = ap.parse_args()
    path = os.path.join(a.capture, 'poll.log') if a.capture else a.trace
    lines = load(path)
    name = os.path.basename(os.path.normpath(a.capture)) if a.capture else path
    if a.cmd == 'path':
        for n, f, m, b1f, cred, e0, e2 in mode_path(lines):
            print('%s:%d f=%X mode=%X b1f=%X cred=%X e0=%04X e2=%04X'
                  % (os.path.basename(path), n, f, m, b1f, cred, e0, e2))
        return 0
    if a.finisher:
        human = two_human(lines)
        if human['join'] is not None:
            print('gp_twop: %s: join f=%X (cred %s -> %X); %d S records from the join to %s; '
                  'side 0 %s, side 1 %s; fight presses %d/%d'
                  % (name, human['join'], '?' if human['cred_before'] is None else '%X' % human['cred_before'],
                     human['cred_join'], human['frames'],
                     'the end' if human['end'] is None else 'X f=%X' % human['end'],
                     sorted(human['counts'][0].items()), sorted(human['counts'][1].items()),
                     human['pressed'][0], human['pressed'][1]))
        for x in human['fail']:
            print('gp_twop: %s: FAIL: %s' % (name, x))
        print('gp_twop: %s: two-human match: %s'
              % (name, 'FAIL' if human['fail'] else 'ok'))

        result = finisher(lines)
        print('gp_twop: %s: finisher: %s; mode 0x09 %s, mode 0x12 %s, reactions %s'
              % (name, 'FAIL' if result['fail'] else 'ok',
                 'reached' if result['mode9'] else 'missing',
                 'reached' if result['mode12'] else 'missing',
                 ['%02X' % r for r in result['reactions']]))
        for x in result['fail']:
            print('gp_twop: %s: FAIL: %s' % (name, x))
        return 1 if human['fail'] or result['fail'] else 0

    r = two_human(lines, join_only=a.join)
    if r['join'] is not None:
        print('gp_twop: %s: join f=%X (cred %s -> %X); %d S records from the join to %s; '
              'side 0 %s, side 1 %s; fight presses %d/%d'
              % (name, r['join'], '?' if r['cred_before'] is None else '%X' % r['cred_before'],
                 r['cred_join'], r['frames'], 'the end' if r['end'] is None else 'X f=%X' % r['end'],
                 sorted(r['counts'][0].items()), sorted(r['counts'][1].items()),
                 r['pressed'][0], r['pressed'][1]))
        if a.join:
            print('gp_twop: %s: mid-round join: mode %X -> %X, b1f %X -> %X'
                  % (name, r['before_mode'], r['join_mode'], r['b1f_before'], r['b1f_join']))
    for x in r['fail']:
        print('gp_twop: %s: FAIL: %s' % (name, x))
    if r['fail']:
        return 1
    print('gp_twop: %s: %s: ok' % (name, 'mid-round join' if a.join else 'two-human match'))
    return 0


if __name__ == '__main__':
    sys.exit(main())
