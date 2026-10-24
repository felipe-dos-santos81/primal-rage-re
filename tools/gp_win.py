#!/usr/bin/env python3
"""Gameplay U9/U10 (plan docs/superpowers/plans/2026-10-02-gameplay-u9-u10-win-and-endings.md,
record docs/superpowers/plans/2026-10-02-gameplay-u9-u10-derivations.md §W.9): the win path
and the ending, reached under memory pokes (gp_session's ('poke', ...) steps).
gp-u14-win-live (gameplay truth II U14, spec W5) uses the same start and match-1 rows
without pokes: its match is fought, and the capture (its Task 11) measures the hits.

  check  evidence: the capture's S records show the scenario's raw-derived path, every
         MILESTONES row in order (each found in the k-th entry into its mode, from the
         first frame in mode 0x27 on), and every DEATH_DONE poke found the death-done
         byte still 0 (the game had not ended a death animation itself). For
         gp-u14-win-live also the live win's own proof (spec W5): an S record with
         b0c = 1 at or after the round-2 KO row and no DEATH_DONE poke — the game
         itself ended a KO'd fighter's death animation (0x37EA0 -> 0x37FF6). Exit 1 on
         the first failure.
  path   the port's T records (trace.txt) reach the same milestones at the same frame,
         judged over the frames the capture snapshotted (as gp_compare's trace claim);
         the leading count reproduced must be >= --min-milestones N (a ratchet). With
         --win-min-first F, also gp_compare's trace claim over gp_session.WIN_FIELDS
         ('win'): the first differing f must be >= F.

Narrow, like the other gp oracles: a milestone is judged on the WIN fields, mode and the
slot bytes it names, nothing else; the frames are gp_compare's claim. Stdlib only.
Usage: gp_win.py check --scenario S --capture DIR [--capture-sha256 H]
       gp_win.py path --scenario S --capture DIR --port DIR --min-milestones N
                      [--win-min-first F] [--capture-sha256 H]"""
import argparse
import hashlib
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gp_compare as gc
import gp_session as gs

NO_RESULT = 0xFFFFFFFF          # DS_00104AD4 = -1: 0x27BA4's undecided match (0x25A0E, 0x27C0C)


def lands(r):
    """The seven land marks DS_00108106..0x10810C from the m106/m10a fields."""
    return list(r['m106'].to_bytes(4, 'little') + r['m10a'].to_bytes(4, 'little'))[:7]


def land(r, i):
    return lands(r)[i] if 0 <= i < 7 else None


def _all(**want):
    return lambda r: all(r[n] == v for n, v in want.items())


# The mode-0x1F ending row's label: the winner's character slot
# DS_0010782A[DS_00104AD4 * 0x94] (0x20900..0x2091A, the load at 0x20914..0x2091A)
# selects the ending (record §W.6, §E1.1).
_ENDING_LABEL = ("%s's ending (mode 0x1F, 0x208F8): the winner's character "
                 "DS_0010782A[DS_00104AD4 * 0x94] (0x20900..0x2091A) is character c0=%d")


# One row per milestone: (label, mode, k, predicate). The frame of a row is the first record
# of the k-th entry into `mode` whose predicate holds. Raw sources: record §W.3-§W.6.
_START = (
    ('character select (mode 0x10)', 0x10, 1, None),
    ('round 1: P1 is character 0 (cursor 0 confirmed, 0x43CAD)', 0x06, 1, _all(c0=0, b1e=1)),
)


def start_char(char, cell):
    """The character-select and round-1 rows for a winner of character `char` confirmed on
    character-select `cell`: 0xC8882 = 00 02 03 01 06 04 05 maps the cursor cell to the
    class at 0x43D6E/0x43D74 (record §E1.1), so cell 1 is class 2 TALON and cell 3 class 1
    BLIZZARD. The label names the character and the cell; the predicate is
    _all(c0=char, b1e=1), exactly _START's."""
    return (
        ('character select (mode 0x10)', 0x10, 1, None),
        ('round 1: P1 is character %d (cursor %d confirmed, 0x43CAD)' % (char, cell),
         0x06, 1, _all(c0=char, b1e=1)),
    )


_MATCH1 = (
    ('round 1 KO: 0x27C48 counts P1, 0x27BA4 leaves the match open, 0x27FA8 -> mode 8',
     0x08, 1, _all(s1_5a=0x78, w2=1, w3=0, ad4=NO_RESULT)),
    ('round 2 (mode 6, round index 2)', 0x06, 2, _all(b1e=2, w2=1)),
    ('round 2 KO: P1 wins the match (0x27BA4 result 0) -> mode 9', 0x09, 1,
     _all(s1_5a=0x78, w2=2, ad4=0)),
    ('the conquered-lands screen (mode 0x12, 0x4142C), the won land marked by 0x286BC '
     '(0x80 | side 0 | character)', 0x12, 1, lambda r: land(r, r['afc']) == 0x80 | r['c0']),
)
MILESTONES = {
    'gp-u9-win': _START + _MATCH1 + (
        ('0x41C28 state 3 counts one land for P1', 0x12, 1, lambda r: r['t104'] & 0xFF == 1),
        ('match 2, round 1: a new, unmarked land (0x25848 picks the stage, 0x41760)', 0x06, 3,
         lambda r: r['b1e'] == 1 and r['w2'] == 0 and land(r, r['afc']) == 0),
    ),
    'gp-u10-ending': _START + (
        ('round 1 KO with the seven lands poked P1\'s', 0x08, 1,
         lambda r: r['s1_5a'] == 0x78 and r['w2'] == 1 and r['w3'] == 0 and r['ad4'] == NO_RESULT
         and lands(r) == [0x80 | r['c0']] * 7),
    ) + _MATCH1[1:] + (
        ('0x41C28 state 3 counts the seventh land', 0x12, 1, lambda r: r['t104'] & 0xFF == 7),
        ('WORLD DOMINATION: 0x4160C clears the marks, +0x82 = 1', 0x12, 1,
         lambda r: r['c82'] == 1 and lands(r) == [0] * 7),
        ('YOU MUST REPLENISH YOUR HEALTH: mode 0x23 (0x417C4, 0x26978)', 0x23, 1, None),
        ('the health bonus (mode 0x22)', 0x22, 1, None),
        ('mode 0x24 (0x26F58)', 0x24, 1, None),
        ('the final: mode 0xC, stage 7, flag DS_00104B14 (0x26F58, 0x25C88)', 0x0C, 1,
         _all(afc=7, b14=1, b21=0)),
    ) + tuple(row for k in range(1, 8) for row in (
        ('final opponent %d KO\'d -> mode 0xD (0x272DC)' % k, 0x0D, k, _all(s1_5a=0x78, b21=k - 1, b0c=0)),
        ('final opponent %d replaced (0x274FC, count %d)' % (k, k), 0x0C, k + 1, _all(b21=k)),
    )[:2 if k < 7 else 1]) + (
        ('YOU ARE MASTER OF THE NEW URTH: mode 0xF, count 7 (0x274FC)', 0x0F, 1, _all(b21=7)),
        (_ENDING_LABEL % ('SAURON', 0), 0x1F, 1, _all(ad4=0, c0=0)),
        ('the ending, second part (mode 0x1F, state 3)', 0x1F, 2, None),
        ('the high-score entry (mode 0x1E)', 0x1E, 1, None),
        ('back in mode 3', 0x03, 1, None),
    ),
}


def _ending_tail(c0, name):
    """MILESTONES['gp-u10-ending']'s rows from the seventh land ('0x41C28 state 3') on, with
    the mode-0x1F ending row's predicate and label for a winner of character `c0`."""
    rows = MILESTONES['gp-u10-ending']
    first = next(i for i, r in enumerate(rows) if r[0].startswith('0x41C28 state 3'))
    return tuple((_ENDING_LABEL % (name, c0), 0x1F, 1, _all(ad4=0, c0=c0))
                 if (r[1], r[2]) == (0x1F, 1) else r for r in rows[first:])


# The second ending (gameplay truth II U13): P1 is character 1 (BLIZZARD), cell 3 of the
# character select (gp_session.WIN_MENU1: 0xC8882 = 00 02 03 01 06 04 05, so cell 1 is
# class 2 TALON and cell 3 is class 1). _START is shared by U9/U10 and keeps character 0.
_START1 = (
    ('character select (mode 0x10)', 0x10, 1, None),
    ('round 1: P1 is character 1 (cursor 3 confirmed, 0x43CAD)', 0x06, 1, _all(c0=1, b1e=1)),
)
MILESTONES['gp-u13-ending2'] = _START1 + (
    ('round 1 KO with the seven lands poked P1\'s', 0x08, 1,
     lambda r: r['s1_5a'] == 0x78 and r['w2'] == 1 and r['w3'] == 0 and r['ad4'] == NO_RESULT
     and lands(r) == [0x80 | r['c0']] * 7),
) + _MATCH1[1:] + _ending_tail(1, 'BLIZZARD')

# Endgame truth Task 3 (record §E1.1): TALON is character 2, character-select cell 1
# (0xC8882 = 00 02 03 01 06 04 05 -> class 2), whose ending title id is 0xE0 (TALON'S
# DREAM, a 0xA80E4 fact and the compared frames' content, not a WIN/S field). The seven
# land marks are 0x80 | side 0 | 2 = 0x82 (0x41C28 state 1 compares
# (mark & 0x7F) == DS_0010782A[side] | side << 6, 0x41D39/0x41D5C); SAURON's 0x80 bytes
# would count no land for P1 and never reach the seventh-land row. The rest is
# gp-u10-ending's from its second poke on (_ending_tail's seventh-land row on), and
# time_limit 240, WIN_EXTRA and memsize 64 are its (record §W.13).
MILESTONES['gp-u18-talon-ending'] = start_char(2, 1) + (
    ('round 1 KO with the seven lands poked P1\'s', 0x08, 1,
     lambda r: r['s1_5a'] == 0x78 and r['w2'] == 1 and r['w3'] == 0
     and r['ad4'] == NO_RESULT and lands(r) == [0x80 | r['c0']] * 7),
) + _MATCH1[1:] + _ending_tail(2, 'TALON')

# Endgame truth Task 4 (record §E1.1): VERTIGO is character 3, character-select cell 2
# (0xC8882 = 00 02 03 01 06 04 05 -> class 3), whose ending title id is 0xE1 (THE PALACE
# OF VERTIGO, a 0xA80E4 fact and the compared frames' content, not a WIN/S field). The
# seven land marks are 0x80 | side 0 | 3 = 0x83 (0x41C28 state 1 compares
# (mark & 0x7F) == DS_0010782A[side] | side << 6, 0x41D39/0x41D5C); another character's
# bytes would count no land for P1 and never reach the seventh-land row. The rest is
# gp-u10-ending's from its second poke on (_ending_tail's seventh-land row on), and
# time_limit 240, WIN_EXTRA and memsize 64 are its (record §W.13).
MILESTONES['gp-u19-vertigo-ending'] = start_char(3, 2) + (
    ('round 1 KO with the seven lands poked P1\'s', 0x08, 1,
     lambda r: r['s1_5a'] == 0x78 and r['w2'] == 1 and r['w3'] == 0
     and r['ad4'] == NO_RESULT and lands(r) == [0x80 | r['c0']] * 7),
) + _MATCH1[1:] + _ending_tail(3, 'VERTIGO')

# Endgame truth Task 5 (record §E1.1): CHAOS is character 6, character-select cell 4
# (0xC8882 = 00 02 03 01 06 04 05 -> class 6), whose ending title id is 0xE4 (CHAOS'
# REDEMPTION, a 0xA80E4 fact and the compared frames' content, not a WIN/S field). The
# seven land marks are 0x80 | side 0 | 6 = 0x86 (0x41C28 state 1 compares
# (mark & 0x7F) == DS_0010782A[side] | side << 6, 0x41D39/0x41D5C); another character's
# bytes would count no land for P1 and never reach the seventh-land row. The rest is
# gp-u10-ending's from its second poke on (_ending_tail's seventh-land row on), and
# time_limit 240, WIN_EXTRA and memsize 64 are its (record §W.13).
MILESTONES['gp-u20-chaos-ending'] = start_char(6, 4) + (
    ('round 1 KO with the seven lands poked P1\'s', 0x08, 1,
     lambda r: r['s1_5a'] == 0x78 and r['w2'] == 1 and r['w3'] == 0
     and r['ad4'] == NO_RESULT and lands(r) == [0x80 | r['c0']] * 7),
) + _MATCH1[1:] + _ending_tail(6, 'CHAOS')

# Endgame truth Task 6 (record §E1.1): ARMADON is character 4, character-select cell 5
# (0xC8882 = 00 02 03 01 06 04 05 -> class 4), whose ending title id is 0xE2 (THE
# MEDITATION OF ARMADON, a 0xA80E4 fact and the compared frames' content, not a WIN/S
# field). The seven land marks are 0x80 | side 0 | 4 = 0x84 (0x41C28 state 1 compares
# (mark & 0x7F) == DS_0010782A[side] | side << 6, 0x41D39/0x41D5C); another character's
# bytes would count no land for P1 and never reach the seventh-land row. The rest is
# gp-u10-ending's from its second poke on (_ending_tail's seventh-land row on), and
# time_limit 240, WIN_EXTRA and memsize 64 are its (record §W.13).
MILESTONES['gp-u21-armadon-ending'] = start_char(4, 5) + (
    ('round 1 KO with the seven lands poked P1\'s', 0x08, 1,
     lambda r: r['s1_5a'] == 0x78 and r['w2'] == 1 and r['w3'] == 0
     and r['ad4'] == NO_RESULT and lands(r) == [0x80 | r['c0']] * 7),
) + _MATCH1[1:] + _ending_tail(4, 'ARMADON')

# Endgame truth Task 7 (record §E1.1): DIABLO is character 5, character-select cell 6
# (0xC8882 = 00 02 03 01 06 04 05 -> class 5), whose ending title id is 0xE3 (DIABLO'S
# HELL, a 0xA80E4 fact and the compared frames' content, not a WIN/S field). The seven
# land marks are 0x80 | side 0 | 5 = 0x85 (0x41C28 state 1 compares
# (mark & 0x7F) == DS_0010782A[side] | side << 6, 0x41D39/0x41D5C); another character's
# bytes would count no land for P1 and never reach the seventh-land row. The rest is
# gp-u10-ending's from its second poke on (_ending_tail's seventh-land row on), and
# time_limit 240, WIN_EXTRA and memsize 64 are its (record §W.13).
MILESTONES['gp-u22-diablo-ending'] = start_char(5, 6) + (
    ('round 1 KO with the seven lands poked P1\'s', 0x08, 1,
     lambda r: r['s1_5a'] == 0x78 and r['w2'] == 1 and r['w3'] == 0
     and r['ad4'] == NO_RESULT and lands(r) == [0x80 | r['c0']] * 7),
) + _MATCH1[1:] + _ending_tail(5, 'DIABLO')

# Gameplay truth II U14 (plan 2026-10-20-gameplay-truth-ii.md Task 6; spec W5): a match won
# by fought hits (gp_session's gp-u14-win-live, no pokes). The rows are the shared start and
# match-1 rows unchanged: the KO the game reaches is the same mode-8 entry with P2's +0x5A at
# 0x78 (0x27C48/0x27FA8), and the won land is marked by 0x286BC as in U9. The seven-lands
# row is U10's poke of round 1, so it does not apply. The rows say nothing about which
# opponent is fought (c1) or how the hits landed: that is the capture's, Task 11's.
MILESTONES['gp-u14-win-live'] = _START + _MATCH1


def entries(recs, start):
    """{(mode, k): [records of the k-th entry into mode]} over the records (by f) from
    `start` on; an entry begins at a record whose mode differs from the previous one's."""
    out, count, prev, cur = {}, {}, None, None
    for f in sorted(recs):
        if f < start:
            continue
        r = recs[f]
        if r['mode'] != prev:
            k = count.get(r['mode'], 0) + 1
            count[r['mode']] = k
            cur = out.setdefault((r['mode'], k), [])
            prev = r['mode']
        cur.append(r)
    return out


def start_frame(recs):
    return next((f for f in sorted(recs) if recs[f]['mode'] == 0x27), None)


def milestone_frames(name, recs, start):
    """[f or None] per MILESTONES[name] row."""
    ent = entries(recs, start)
    out = []
    for _, mode, k, pred in MILESTONES[name]:
        rows = ent.get((mode, k), [])
        out.append(next((r['f'] for r in rows if pred is None or pred(r)), None))
    return out


def snaps_from(lines):
    return gs.snapshots(lines)


def trace_from(lines):
    out = {}
    for l in lines:
        r = gs.parse(l)
        if r and r['kind'] == 'T':
            out.setdefault(r['f'], r)
    return out


def death_done_set(lines, snaps):
    """The frames of the DEATH_DONE pokes (W records at DS_00104B0C) that did not stand for
    the game's own death-done byte: the byte already set (in the W record or in the S record
    of the spin they were written in), the spin not in mode 0xD, or the KO count (b21)
    moved since the mode-0xD entry began: the game set the byte itself, mode 0xD
    (0x27562..0x2758A) consumed it and the poke landed after, finding it clear."""
    addr = gs.DEATH_DONE[1][0][0]
    fs = sorted(snaps)
    out = []
    for r in (gs.parse(l) for l in lines):
        if not (r and r['kind'] == 'W' and r['addr'] == addr):
            continue
        s = snaps.get(r['f'])
        if s is None or s['b0c'] != 0 or r['was'] != '00' or s['mode'] != 0x0D:
            out.append(r['f'])
            continue
        first = s
        for f in reversed([f for f in fs if f < r['f']]):
            if snaps[f]['mode'] != 0x0D:
                break
            first = snaps[f]
        if first['b21'] != s['b21']:
            out.append(r['f'])
    return out


# Spec W5's live-win proof (gameplay truth II U14): a fought match must show the game's own
# death-animation end. 0x37EA0 (the remains streams' 0xD100 target) sets DS_00104B0C = 1 at
# 0x37FF6 when a KO'd fighter's death animation ends; the poked wins replaced that write.
# The zero stores are mode 0x24's timer branch (0x2703D), mode 0x17's setup (0x271E0's
# 0x27200), and the final's consumers (0x27582, 0x29744); so a set byte at the match-end screen
# proves a game death animation ended by then, not that round 2's specifically did: the
# frames and the WIN trace carry which animation ran (record §W.5, §W.9).
LIVE_WIN = frozenset({'gp-u14-win-live'})


def live_death(name, frames, lines, snaps, out=print):
    """The live win's death-done rule (LIVE_WIN): 0 ok, 1 FAIL. `frames` is
    milestone_frames(name, ...). Fails when a W record wrote DS_00104B0C (the DEATH_DONE
    poke) or when no S record at or after the round-2 KO row (mode 9, k = 1) has b0c = 1."""
    addr = gs.DEATH_DONE[1][0][0]
    poked = next((r['f'] for r in (gs.parse(l) for l in lines)
                  if r and r['kind'] == 'W' and r['addr'] == addr), None)
    if poked is not None:
        out("gp_win: %s: evidence: FAIL: a DEATH_DONE poke at f=%X stands for the game's own "
            'death-done byte (0x37FF6), which a fought win must reach itself' % (name, poked))
        return 1
    ko = next((f for (_, mode, k, _), f in zip(MILESTONES[name], frames) if (mode, k) == (0x09, 1)),
              None)
    if ko is None:
        out('gp_win: %s: evidence: FAIL: no round-2 KO row (mode 9, k 1) to anchor the '
            'death-done byte on' % name)
        return 1
    got = next((f for f in sorted(snaps) if f >= ko and snaps[f]['b0c'] == 1), None)
    if got is None:
        out("gp_win: %s: evidence: FAIL: no S record with b0c = 1 at or after the round-2 KO "
            "(mode 9, f=%X): the game never ended a KO'd fighter's death animation "
            '(0x37EA0 -> 0x37FF6), the write the U9/U10 DEATH_DONE poke replaced' % (name, ko))
        return 1
    out("gp_win: %s: evidence: the game ended a KO'd fighter's death animation itself "
        '(b0c = 1 at f=%X, no DEATH_DONE poke)' % (name, got))
    return 0


def evidence(name, lines, out=print):
    """The check. Returns 0 ok, 1 FAIL."""
    snaps = snaps_from(lines)
    if not snaps or any(n not in next(iter(snaps.values())) for n in gs.WIN_FIELDS):
        out('gp_win: %s: evidence: FAIL: the capture records no WIN fields' % name)
        return 1
    start = start_frame(snaps)
    if start is None:
        out('gp_win: %s: evidence: FAIL: mode 0x27 never observed' % name)
        return 1
    prev = start
    frames = milestone_frames(name, snaps, start)
    for (label, mode, k, _), f in zip(MILESTONES[name], frames):
        if f is None or f < prev:
            out('gp_win: %s: evidence: FAIL: %s (mode 0x%X entry %d)%s'
                % (name, label, mode, k, '' if f is None else ' at f=%X, before the previous milestone' % f))
            return 1
        out('gp_win: %s: evidence: %s at f=%X ok' % (name, label, f))
        prev = f
    bad = death_done_set(lines, snaps)
    if bad:
        out('gp_win: %s: evidence: FAIL: the death-done poke at f=%X did not find a clear byte in the mode-0xD entry' % (name, bad[0]))
        return 1
    if name in LIVE_WIN and live_death(name, frames, lines, snaps, out):
        return 1
    out('gp_win: %s: evidence: %d/%d milestones ok' % (name, len(MILESTONES[name]), len(MILESTONES[name])))
    return 0


def reproduced(name, cap_lines, port_lines):
    """(count, rows): the leading milestones the port reaches at the capture's frame, over
    the frames the capture snapshotted; rows = [(label, capture f, port f)]."""
    snaps, port = snaps_from(cap_lines), trace_from(port_lines)
    start = start_frame(snaps)
    if start is None:
        return 0, [(row[0], None, None) for row in MILESTONES[name]]
    shared = {f: port[f] for f in port if f in snaps}
    cf = milestone_frames(name, snaps, start)
    pf = milestone_frames(name, shared, start)
    rows = [(row[0], c, p) for row, c, p in zip(MILESTONES[name], cf, pf)]
    n = 0
    for _, c, p in rows:
        if c is None or c != p:
            break
        n += 1
    return n, rows


def sha_ok(name, cap_dir, sha, out):
    if sha is None:
        return True
    with open(os.path.join(cap_dir, 'poll.log'), 'rb') as f:
        got = hashlib.sha256(f.read()).hexdigest()
    if sha == '' or got != sha:
        out('gp_win: %s: capture: FAIL: poll.log sha256 %s != the pinned %s: re-measure, then re-pin'
            % (name, got, sha or '(unpinned)'))
        return False
    return True


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('cmd', choices=('check', 'path'))
    ap.add_argument('--scenario', required=True, choices=sorted(MILESTONES))
    ap.add_argument('--capture', required=True)
    ap.add_argument('--port')
    ap.add_argument('--min-milestones')
    ap.add_argument('--win-min-first')
    ap.add_argument('--capture-sha256')
    a = ap.parse_args()
    name = a.scenario
    if not os.path.isfile(os.path.join(a.capture, 'poll.log')):
        print('gp_win: %s: no capture at %s (skipped)' % (name, a.capture))
        return 0
    if not sha_ok(name, a.capture, a.capture_sha256, print):
        return 1
    with open(os.path.join(a.capture, 'poll.log')) as f:
        cl = f.read().splitlines()
    if a.cmd == 'check':
        return evidence(name, cl)
    if a.port is None or not os.path.isfile(os.path.join(a.port, 'trace.txt')):
        print('gp_win: %s: path: FAIL: no port trace at %s' % (name, a.port))
        return 1
    with open(os.path.join(a.port, 'trace.txt')) as f:
        pl = f.read().splitlines()
    return path(name, cl, pl, None if a.min_milestones in (None, '') else int(a.min_milestones),
                a.win_min_first)


def path(name, cl, pl, min_milestones, win_min_first=None, out=print):
    """The path command on the two logs' lines: 0 ok, 1 FAIL."""
    if start_frame(snaps_from(cl)) is None:
        out('gp_win: %s: path: FAIL: mode 0x27 never observed in the capture' % name)
        return 1
    n, rows = reproduced(name, cl, pl)
    for label, c, p in rows:
        out('gp_win: %s: path: %-70s capture %s port %s' % (name, label, '-' if c is None else '%X' % c,
                                                          '-' if p is None else '%X' % p))
    rc = gc.ratchet(name, 'path', None if n == len(rows) else n, len(rows), min_milestones, out,
                    'not reproduced')
    if win_min_first is not None:
        rc2, _ = gc.trace_claim(name, cl, pl, None if win_min_first == '' else int(win_min_first),
                                out, False, gs.WIN_FIELDS, 'win')
        rc = rc or rc2
    return rc


if __name__ == '__main__':
    sys.exit(main())
