#!/usr/bin/env python3
"""State coverage Task 5 (plan docs/superpowers/plans/2026-10-28-state-coverage.md
§G3, record docs/superpowers/plans/2026-10-28-state-coverage-derivations.md §D5):
ENDURANCE's four-KO roster walk and match end (modes `0x32`/`0x33` and the
results screen). Stdlib only.

  check  evidence: the capture's S records show the raw-derived path — mode 0x30
         resets the loss counters (af0 = af1 = 0), mode 0x31's four entries walk
         the loser's roster c1 = 04 -> 06 -> 01 -> 03 with af1 = 0 -> 1 -> 2 -> 3,
         the fourth KO ends the match (mode 0x33, af1 = 4), and the results
         screen (mode 0x17, entry 2) clears b25 and names side 0 the winner
         (w10d = 0). The loser is side 1 (P2: KO_P2 pokes its +0x5A
         DS_0010789E), so the counter that reaches 4 is af1 (DS_00104AF1), NOT
         af0. The counters are the bytes DS_00104AF0/AF1 written by
         game_mode_32_step (0x296B8); they are not WIN_EXTRA's round wins
         w2/w3 (DS_00104AF2/AF3).
  path   the port's T records reach the same milestones at the same frame, judged
         over the frames the capture snapshotted; the leading count reproduced
         must be >= --min-milestones N (a ratchet).

Narrow, like the other gp oracles: a milestone is judged on the mode and the
af0/af1/c1/b25/w10d bytes it names, nothing else; the frames are gp_compare's
claim. The roster byte DS_00108134[s*4 + count] the loser (side 1) reads is a
prediction from ENDURANCE_PICKS' teams ([0,2,3,1] / [4,6,1,3]); the capture
measures it, and a divergence is a FAIL, never a fitted value.
Usage: gp_endurance.py check --scenario S --capture DIR [--capture-sha256 H]
       gp_endurance.py path --scenario S --capture DIR --port DIR --min-milestones N
                            [--capture-sha256 H]"""
import argparse
import hashlib
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gp_compare as gc
import gp_session as gs


def _all(**want):
    return lambda r: all(r[n] == v for n, v in want.items())


# The milestones of the ENDURANCE match end, read on the S records (record §D5):
# mode 0x30's sub-state 3 resets DS_00104AF0/AF1 to 0 (game_mode_30_step 0x29328)
# and enters mode 0x31; each KO runs flow_round_over_check (0x29970) -> mode 0x32
# (DS_00104B09 = the KO'd side), and game_mode_32_step (0x296B8) increments
# DS_00104AF0[s] and replaces the loser with DS_00108134[s*4 + count], re-entering
# mode 0x31, or, at count 4, ends the match: mode 0x33 (the 0x258 = 600-frame
# wait) and then mode 0x17's results screen (the hook 0x25AE8 = game_hook_25ae8,
# game_mode_33_step 0x29638), which sets b25 = 0 and names the winner side
# DS_0010810D (w10d). With P2 KO'd, side 1 is the loser, so its counter af1
# reaches 4 and c1 walks side 1's roster [4,6,1,3]; the winner is side 0 (w10d = 0).
MILESTONES = {
    'gp-u25-endurance-end': (
        ('the fight prep resets the loss counters (af0=0, af1=0)', 0x30, 1,
         _all(af0=0, af1=0)),
        ('mode 0x31 entry 1: P2 is the roster\'s first character (c1=4)', 0x31, 1,
         _all(c1=4)),
        ('KO 1 advances the roster: mode 0x31 entry 2 is c1=6, af1=1', 0x31, 2,
         _all(c1=6, af1=1)),
        ('KO 2: mode 0x31 entry 3 is c1=1, af1=2', 0x31, 3, _all(c1=1, af1=2)),
        ('KO 3: mode 0x31 entry 4 is c1=3, af1=3', 0x31, 4, _all(c1=3, af1=3)),
        ('KO 4: af1=4 ends the match -> mode 0x33 (the 600-frame wait)', 0x33, 1,
         _all(af1=4)),
        ('the results screen: mode 0x17 after the wait, b25=0, the winner side 0 (w10d=0)',
         0x17, 2, _all(b25=0, w10d=0)),
    ),
}


def entries(recs, start):
    """{(mode, k): [records of the k-th entry into mode]} over the records (by f)
    from `start` on; an entry begins at a record whose mode differs from the
    previous one's (as gp_win.entries)."""
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


def evidence(name, lines, out=print):
    """The check. Returns 0 ok, 1 FAIL."""
    snaps = snaps_from(lines)
    if not snaps or any(n not in next(iter(snaps.values())) for n in ('af0', 'af1')):
        out('gp_endurance: %s: evidence: FAIL: the capture records no af0/af1 fields' % name)
        return 1
    start = start_frame(snaps)
    if start is None:
        out('gp_endurance: %s: evidence: FAIL: mode 0x27 never observed' % name)
        return 1
    prev = start
    frames = milestone_frames(name, snaps, start)
    for (label, mode, k, _), f in zip(MILESTONES[name], frames):
        if f is None or f < prev:
            out('gp_endurance: %s: evidence: FAIL: %s (mode 0x%X entry %d)%s'
                % (name, label, mode, k,
                   '' if f is None else ' at f=%X, before the previous milestone' % f))
            return 1
        out('gp_endurance: %s: evidence: %s at f=%X ok' % (name, label, f))
        prev = f
    out('gp_endurance: %s: evidence: %d/%d milestones ok'
        % (name, len(MILESTONES[name]), len(MILESTONES[name])))
    return 0


def reproduced(name, cap_lines, port_lines):
    """(count, rows): the leading milestones the port reaches at the capture's
    frame, over the frames the capture snapshotted; rows = [(label, capture f,
    port f)]."""
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
        out('gp_endurance: %s: capture: FAIL: poll.log sha256 %s != the pinned %s: '
            're-measure, then re-pin' % (name, got, sha or '(unpinned)'))
        return False
    return True


def path(name, cl, pl, min_milestones, win_min_first=None, out=print):
    """The path command on the two logs' lines: 0 ok, 1 FAIL. `win_min_first` is
    accepted for gp_win.py's call shape but unused: the ENDURANCE claim has no
    separate win-fields trace (af0/af1 are SNAP_FIELDS, covered by the milestone
    ratchet and gp_compare's trace claim)."""
    if start_frame(snaps_from(cl)) is None:
        out('gp_endurance: %s: path: FAIL: mode 0x27 never observed in the capture' % name)
        return 1
    n, rows = reproduced(name, cl, pl)
    for label, c, p in rows:
        out('gp_endurance: %s: path: %-70s capture %s port %s'
            % (name, label, '-' if c is None else '%X' % c, '-' if p is None else '%X' % p))
    return gc.ratchet(name, 'path', None if n == len(rows) else n, len(rows),
                      min_milestones, out, 'not reproduced')


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('cmd', choices=('check', 'path'))
    ap.add_argument('--scenario', required=True, choices=sorted(MILESTONES))
    ap.add_argument('--capture', required=True)
    ap.add_argument('--port')
    ap.add_argument('--min-milestones')
    ap.add_argument('--capture-sha256')
    a = ap.parse_args()
    name = a.scenario
    if not os.path.isfile(os.path.join(a.capture, 'poll.log')):
        print('gp_endurance: %s: no capture at %s (skipped)' % (name, a.capture))
        return 0
    if not sha_ok(name, a.capture, a.capture_sha256, print):
        return 1
    with open(os.path.join(a.capture, 'poll.log')) as f:
        cl = f.read().splitlines()
    if a.cmd == 'check':
        return evidence(name, cl)
    if a.port is None or not os.path.isfile(os.path.join(a.port, 'trace.txt')):
        print('gp_endurance: %s: path: FAIL: no port trace at %s' % (name, a.port))
        return 1
    with open(os.path.join(a.port, 'trace.txt')) as f:
        pl = f.read().splitlines()
    return path(name, cl, pl, None if a.min_milestones in (None, '') else int(a.min_milestones))


if __name__ == '__main__':
    sys.exit(main())
