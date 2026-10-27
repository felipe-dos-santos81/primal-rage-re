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
  gp_twop.py check --finisher --allow-missing-reaction --capture DIR
                                     enforce the two-human and end-mode evidence while
                                     reporting, but allowing, the R2 reaction gap
  gp_twop.py check --ko --capture DIR [--winner N]
                                     the hand-dealt KO evidence (plan
                                     2026-10-26-fought-match-truth §F3): a two-human match
                                     whose loser's +0x5A reached 0x78 from a live value in
                                     a mode-6 entry that no poke wrote and that did not
                                     time out, and whose result names side N (default 0)
  gp_twop.py check --trace FILE     the same over a port trace.txt (its T records
                                    are read as S records; any file whose records
                                    are T lines is a trace)
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

# The hand-dealt KO (record 2026-10-26-fought-match-truth-derivations.md §F3): the
# KO value 0x78 of a fighter's +0x5A damage byte (0x27FA8's round-end test), the
# result modes (7 is the time-up result), and DS_00104AD4's undecided match.
KO_VALUE = 0x78
TIMEOUT_MODE = 0x07             # the round's time-up result (mode 7)
ROUND_MODE = 0x08               # the round result (mode 8, 0x27FA8)
MATCH_MODE = 0x09               # the match result (mode 9, 0x28024..0x2803B)
NO_RESULT = 0xFFFFFFFF          # DS_00104AD4 = -1 (0x25A0E; the -1 store at 0x27C3D): 0x27BA4's undecided match
# The WIN fields a poked win writes: the two +0x5A damage bytes (s0_5a/s1_5a), the
# round wins (w2/w3), the match result (ad4), the death-done byte (b0c) and the
# seven land marks (m106/m10a; gp_session.lands_marked writes them at 0x108106 with
# len 7). A W record that covers one of these replaced the game's own write
# (record §W.3, §W.5; gp_session.KO_P2, DEATH_DONE).
KO_POKES = (0x10780A, 0x10789E, 0x104AF2, 0x104AF3, 0x104AD4,
            0x104B0C, 0x108106, 0x10810A, 0x10810C)


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
    """poll.log lines, or a port trace.txt with its T records read as S records
    (any file whose records are T lines is read as a trace, so --trace works on
    a renamed dump too; a poll.log has no T records)."""
    with open(path) as f:
        lines = f.read().splitlines()
    if any(l.startswith('T ') for l in lines):
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
    in DS_001088E4 (or, when the harness missed the frames where it rose, held in the
    join's record after a record before it that did not hold it and a genuine sampling
    gap: start_edge 'held'),
    and b1f stays 3 to the end; the credit check is shared (0x2CA93).
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
               b1f_before=None, b1f_join=None, start_edge=None, fail=[])
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
            elif snap[join]['new'] & START_MASK[side]:
                out['start_edge'] = 'new'
            elif (join - before[-1] > 1 and snap[join]['held'] & START_MASK[side]
                  and not (snap[before[-1]]['held'] & START_MASK[side])):
                out['start_edge'] = 'held'
            else:
                out['fail'].append('the joining side %d start mask %08X is not newly pressed at f=%X '
                                   '(new %08X, held %08X; the record before it held %08X)'
                                   % (side, START_MASK[side], join, snap[join]['new'], snap[join]['held'],
                                      snap[before[-1]]['held']))
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


def _entries(recs):
    """[(mode, [records])] in f order, one per run of equal mode: the k-th entry
    into a mode is its own run, as gp_win.entries' (mode, k) pairs count them."""
    out = []
    for r in recs:
        if out and out[-1][0] == r['mode']:
            out[-1][1].append(r)
        else:
            out.append((r['mode'], [r]))
    return out


def _poked_field(r):
    """The first KO_POKES address this W record's bytes cover, or None."""
    return next((a for a in KO_POKES if r['addr'] <= a < r['addr'] + r['len']), None)


def _hx(v):
    """A frame as hex, or '-' when the log does not carry it."""
    return '-' if v is None else '%X' % v


def _named_side(res):
    """The side a result's cumulative round counters (w2/w3, 0x27C48) name, or
    None when they are level: (side, field, value)."""
    if res['w2'] > res['w3']:
        return 0, 'w2', res['w2']
    if res['w3'] > res['w2']:
        return 1, 'w3', res['w3']
    return None


def _result_fail(label, res, winner):
    """A result's counter failure line, naming the side the counters favour
    (not the field: "names side 1 (w3 = 1 > w2 = 0)", record §F3.1)."""
    named = _named_side(res)
    if named is None:
        what = 'names no side (w2 = %d, w3 = %d)' % (res['w2'], res['w3'])
    else:
        side, field, value = named
        other = 'w3' if field == 'w2' else 'w2'
        what = 'names side %d (%s = %d > %s = %d)' % (side, field, value, other, res[other])
    return 'the %s at f=%X %s, not winner side %d' % (label, res['f'], what, winner)


def _result_names(res, wfield, lfield, rmode, prev):
    """Whether a result's cumulative w2/w3 counters name the winner. A match
    result (mode 9) needs w[winner] > w[loser]; a round result (mode 8) may
    instead show the winner's counter rising at that result, because a split
    match's counters can be level (1-1) at its round-2 result (0x27C48
    increments the round's winner). `prev` is the last result record before
    `res`, or None."""
    wv = res[wfield]
    if wv < 1:
        return False
    if wv > res[lfield]:
        return True
    # A split match's counters are level at its round-2 result; that round
    # result names the side whose counter rose there. A result whose counters
    # favour the loser names the loser, not the claimed winner.
    return rmode == ROUND_MODE and wv == res[lfield] and (
        prev is None or wv > prev[wfield])


def ko(lines, winner=0):
    """The hand-dealt KO evidence (plan 2026-10-26-fought-match-truth §F3).

    Returns dict(ko, round, result, death, fail): the frames of the KO (the
    loser's s*_5a reaching 0x78), of the round result (mode 8), of the match
    result (mode 9) and of the first death-done record (b0c = 1), plus the
    reasons the log is not a hand-dealt KO of side `winner`.

    All the conditions are required: the log is a two-human match (two_human's
    fail); no W record wrote a KO/death field (KO_POKES: a poke replaced the
    game's own write); the loser's +0x5A reaches 0x78 from an earlier
    0 < value < 0x78 in the mode-6 entry the KO ends (not a jump the damage
    path cannot make) — the KO's own record is the round or match result when
    the KO and the mode change share an iteration, as every measured capture
    records it (record §F3.2); that round did not time out (no mode-7 record at
    its result); the winner's +0x5A stays below 0x78 at the KO; and the result
    names the winner — the round result (mode 8, ad4 = NO_RESULT = 0xFFFFFFFF)
    or the match result (mode 9, ad4 = winner), never the draw 2. w2/w3 are
    cumulative match wins (0x27C48 increments the round's winner): a result
    names the winner when w[winner] >= 1 and w[winner] > w[loser], or — at a
    round result, where a split match's counters can be level — when the
    winner's counter rose at that result; a mode-9 entry at or after the KO
    must carry ad4 = winner and w[winner] > w[loser]. b0c is reported, not
    required: the first 0 -> 1 transition at or after the KO is the death-done
    frame (b0c is not cleared between rounds, so a stale 1 is not this KO's),
    and a hand-dealt KO leaves the death animation to run (record §W.5)."""
    out = dict(ko=None, round=None, result=None, death=None, fail=[])
    if winner not in (0, 1):
        out['fail'].append('side %r cannot win: the slots are 0 and 1' % (winner,))
        return out
    loser = 1 - winner
    wfield, lfield = ('w2', 'w3') if winner == 0 else ('w3', 'w2')
    w5a, l5a = 's%d_5a' % winner, 's%d_5a' % loser
    parsed = [r for r in (gs.parse(line) for line in lines) if r]
    x = next((r['f'] for r in parsed if r['kind'] == 'X'), None)
    snaps = gs.snapshots(lines)
    recs = sorted((r for f, r in snaps.items() if x is None or f <= x), key=lambda r: r['f'])
    if not recs:
        out['fail'].append('no S records: nothing shows a KO')
        return out
    missing = [n for n in gs.WIN_FIELDS if any(n not in r for r in recs)]
    if missing:
        out['fail'].append('no WIN fields: %s absent from some S records (the capture must log '
                           'gp_session.WIN_EXTRA; without the damage/result bytes a KO cannot be shown)'
                           % ', '.join(missing))
        return out
    for r in parsed:
        hit = _poked_field(r) if r['kind'] == 'W' else None
        if hit is not None:
            out['fail'].append('poke: the W record at f=%X covers %08X, a KO/death field: the round '
                               "must be the game's own write, not a poked one" % (r['f'], hit))
            break
    out['fail'] += two_human(lines)['fail']

    entries = _entries(recs)
    ko_i, ko_r = None, None
    for i, (mode, rows) in enumerate(entries):
        hit = next((r for r in rows if r[l5a] == KO_VALUE), None)
        if hit is not None:
            ko_i, ko_r = i, hit
            break
    if ko_r is None:
        tout = next((r['f'] for r in recs if r['mode'] == TIMEOUT_MODE), None)
        if tout is not None:
            out['fail'].append('time-out: mode %X at f=%X and no record has side %d %s = %X '
                               "(0x27FA8's KO value): the round ended on the timer"
                               % (TIMEOUT_MODE, tout, loser, l5a, KO_VALUE))
        else:
            out['fail'].append("no record has side %d %s = %X (0x27FA8's KO value)"
                               % (loser, l5a, KO_VALUE))
        return out
    out['ko'] = ko_r['f']
    mode = entries[ko_i][0]
    # The round's mode-6 entry: the KO and the mode change share the iteration
    # that deals it, so an end-of-iteration snapshot shows the result mode, not
    # mode 6 (record §F3.2: gp-u14-win-live's fought KO is the mode-6 record
    # f=0x894 at s0_5a=0x71 and the mode-8 record f=0x898 at 0x78; gp-u9-win's
    # poked one is mode 6 f=0x489 at 0 and mode 8 f=0x48D at 0x78). The KO
    # record is itself in the entry when the value was seen in the fight, else
    # the entry is the run just before the result record.
    if mode == FIGHT_MODE:
        round_rows = entries[ko_i][1]
    elif mode in (TIMEOUT_MODE, ROUND_MODE, MATCH_MODE):
        round_rows = entries[ko_i - 1][1] if ko_i and entries[ko_i - 1][0] == FIGHT_MODE else []
        if not round_rows:
            out['fail'].append('the %X at f=%X does not follow a mode-%d entry: the round it ended '
                               'is not the fight' % (KO_VALUE, ko_r['f'], FIGHT_MODE))
    else:
        out['fail'].append('side %d %s first reads %X at f=%X in mode %X, not the fight (mode %d) '
                           'or its result' % (loser, l5a, KO_VALUE, ko_r['f'], mode, FIGHT_MODE))
        round_rows = []
    if round_rows and next((r for r in round_rows
                            if r['f'] < ko_r['f'] and 0 < r[l5a] < KO_VALUE), None) is None:
        out['fail'].append('side %d %s jumps to %X at f=%X with no earlier 0 < value < %X in the '
                           'mode-%d entry the KO ends: the damage path adds to a live value'
                           % (loser, l5a, KO_VALUE, ko_r['f'], KO_VALUE, FIGHT_MODE))
    if ko_r[w5a] >= KO_VALUE:
        out['fail'].append("the winner side %d %s = %X at the KO f=%X: the KO'd side is the winner"
                           % (winner, w5a, ko_r[w5a], ko_r['f']))

    # The round's result record: the KO record itself when the KO first showed on
    # the result, else the run of records after the KO entry. A mode-7 record
    # cannot sit inside a mode-6 run (runs are maximal), so the no-time-out rule
    # reads on the KO's own result run: it is the time-up screen when it is mode 7.
    res = None
    if mode in (ROUND_MODE, MATCH_MODE):
        res = ko_r
    elif mode == TIMEOUT_MODE:
        out['fail'].append('time-out: the KO value at f=%X is in the time-up result (mode %X): the '
                           'round ended on the timer' % (ko_r['f'], TIMEOUT_MODE))
    else:
        nxt_mode, nxt_rows = entries[ko_i + 1] if ko_i + 1 < len(entries) else (None, [])
        if nxt_mode == TIMEOUT_MODE:
            out['fail'].append('time-out: mode %X at f=%X after the KO at f=%X: the round ended on '
                               'the timer, not on the KO' % (TIMEOUT_MODE, nxt_rows[0]['f'], ko_r['f']))
        elif not nxt_rows:
            out['fail'].append('no result after the KO at f=%X: neither the round result (mode %X) '
                               'nor the match result (mode %X) was reached'
                               % (ko_r['f'], ROUND_MODE, MATCH_MODE))
        elif nxt_mode not in (ROUND_MODE, MATCH_MODE):
            out['fail'].append('the record after the KO at f=%X is mode %X, not the round (%X) or '
                               'match (%X) result' % (ko_r['f'], nxt_mode, ROUND_MODE, MATCH_MODE))
        else:
            res = nxt_rows[0]
    if res is not None:
        rmode = res['mode']
        want = NO_RESULT if rmode == ROUND_MODE else winner
        out['round' if rmode == ROUND_MODE else 'result'] = res['f']
        label = 'round result' if rmode == ROUND_MODE else 'match result'
        if res['ad4'] != want:
            out['fail'].append('the %s at f=%X has ad4 = %X, not %X: it does not name side %d%s'
                               % (label, res['f'], res['ad4'], want, winner,
                                  ' (2 is the draw)' if res['ad4'] == 2 else ''))
        # w2/w3 are cumulative (0x27C48 increments the round's winner), so a
        # later round's result can be level after a split (1-1); the round
        # result's rule is then that the winner's counter rose there, not that
        # it exceeds the loser's (review fix: the old w[loser] == 0 rule
        # rejected a legitimate 2-1 match and a round-2 KO after a split).
        prev = next((r for r in reversed(recs) if r['f'] < res['f']
                     and r['mode'] in (ROUND_MODE, MATCH_MODE)), None)
        if not _result_names(res, wfield, lfield, rmode, prev):
            out['fail'].append(_result_fail(label, res, winner))

    if out['result'] is None:
        m9 = next((rows9[0] for mode, rows9 in entries
                   if mode == MATCH_MODE and rows9 and rows9[0]['f'] >= ko_r['f']), None)
        if m9 is not None:
            out['result'] = m9['f']
            if m9['ad4'] != winner:
                out['fail'].append('the match result at f=%X has ad4 = %X, not winner side %d%s'
                                   % (m9['f'], m9['ad4'], winner,
                                      ' (2 is the draw)' if m9['ad4'] == 2 else ''))
            if not (m9[wfield] >= 1 and m9[wfield] > m9[lfield]):
                out['fail'].append(_result_fail('match result', m9, winner))

    # b0c is not cleared between rounds (spec §7.2), so a previous round's 1
    # would otherwise be reported as this KO's: only a 0 -> 1 transition at or
    # after the KO is the death-done frame (report-only; review fix).
    death, before = None, None
    for r in recs:
        if r['f'] < ko_r['f']:
            before = r['b0c']
            continue
        if r['b0c'] == 1 and before == 0:
            death = r['f']
            break
        before = r['b0c']
    out['death'] = death
    return out


def finisher(lines, allow_missing_reaction=False):
    """The U16 finisher evidence: a two-human log reached modes 0x09 and 0x12 and
    contains a finisher reaction (0x32..0x34) in either S record move field. When
    allow_missing_reaction is true, only that reaction requirement is waived."""
    records = [gs.parse(line) for line in lines]
    snapshots = [r for r in records if r and r['kind'] == 'S']
    mode9 = any(r['mode'] == 0x09 for r in snapshots)
    mode12 = any(r['mode'] == 0x12 for r in snapshots)
    reactions = [r[field] for r in snapshots for field in ('r0', 'r1')
                 if r.get(field) in (0x32, 0x33, 0x34)]
    fail = []
    if not reactions and not allow_missing_reaction:
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


def report_human(name, human):
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
    check.add_argument('--ko', action='store_true',
                       help="check the hand-dealt KO of one side and the result that names it (--winner)")
    ap.add_argument('--allow-missing-reaction', action='store_true',
                    help='with --finisher, allow no sampled finisher reaction while still checking end modes')
    ap.add_argument('--winner', type=int, choices=(0, 1),
                    help='with --ko, the winning side (default 0)')
    a = ap.parse_args()
    if a.allow_missing_reaction and not a.finisher:
        ap.error('--allow-missing-reaction requires --finisher')
    if a.winner is not None and not a.ko:
        ap.error('--winner requires --ko')
    path = os.path.join(a.capture, 'poll.log') if a.capture else a.trace
    lines = load(path)
    name = os.path.basename(os.path.normpath(a.capture)) if a.capture else path
    if a.cmd == 'path':
        for n, f, m, b1f, cred, e0, e2 in mode_path(lines):
            print('%s:%d f=%X mode=%X b1f=%X cred=%X e0=%04X e2=%04X'
                  % (os.path.basename(path), n, f, m, b1f, cred, e0, e2))
        return 0
    if a.ko:
        w = 0 if a.winner is None else a.winner
        k = ko(lines, winner=w)
        print('gp_twop: %s: hand-dealt KO of side %d: ko f=%s; round result f=%s; match result f=%s; '
              'death-done f=%s' % (name, w, _hx(k['ko']), _hx(k['round']), _hx(k['result']),
                                   _hx(k['death'])))
        for x in k['fail']:
            print('gp_twop: %s: FAIL: %s' % (name, x))
        print('gp_twop: %s: hand-dealt KO: %s' % (name, 'FAIL' if k['fail'] else 'ok'))
        return 1 if k['fail'] else 0
    if a.finisher:
        human = two_human(lines)
        report_human(name, human)
        print('gp_twop: %s: two-human match: %s'
              % (name, 'FAIL' if human['fail'] else 'ok'))

        result = finisher(lines, allow_missing_reaction=a.allow_missing_reaction)
        print('gp_twop: %s: finisher: %s; mode 0x09 %s, mode 0x12 %s, reactions %s'
              % (name, 'FAIL' if result['fail'] else 'ok',
                 'reached' if result['mode9'] else 'missing',
                 'reached' if result['mode12'] else 'missing',
                 ['%02X' % r for r in result['reactions']]))
        for x in result['fail']:
            print('gp_twop: %s: FAIL: %s' % (name, x))
        return 1 if human['fail'] or result['fail'] else 0

    r = two_human(lines, join_only=a.join)
    report_human(name, r)
    if r['join'] is not None:
        if a.join:
            print('gp_twop: %s: mid-round join: mode %X -> %X, b1f %X -> %X, start press seen as %s'
                  % (name, r['before_mode'], r['join_mode'], r['b1f_before'], r['b1f_join'],
                     {'new': 'the newly-pressed bit', 'held': 'the held-edge across missed frames',
                      None: 'neither'}[r['start_edge']]))
    if r['fail']:
        return 1
    print('gp_twop: %s: %s: ok' % (name, 'mid-round join' if a.join else 'two-human match'))
    return 0


if __name__ == '__main__':
    sys.exit(main())
