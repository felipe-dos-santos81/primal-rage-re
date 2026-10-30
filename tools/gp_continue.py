#!/usr/bin/env python3
"""Live truth Task 5 (record 2026-10-29-live-truth-derivations.md §L5): the
continue screen 0x0E, its PRESS START prompt/countdown and its expiry; final
truth Task 4 (design §3.6, record §F4): the taken-continue branch (0x2791C).
Stdlib only.

  check  evidence: the capture opens mode 0x0E on the incumbent side (w10d=0)
         after the labelled 0x10780A=0x78 KO poke, keeps cred=4 and b1f=0 (no
         continue taken), counts 15 down to 0, takes the credit-ready PRESS
         START prompt arm at cnt=13 (the raw gate is cnt < 0x0E), and expires
         to mode 0x15 with b25=0.
  check --taken  evidence: the same screen is opened, its countdown reaches the
         prompt arm (cnt < 0x0E), and the screen ends in the 0x2791C take:
         mode 0x0C, cred 4 -> 3 (0x42F60's own 0x2CA9C debit, not a poke),
         b1f 0 -> 1 (0x27A40), the incumbent's +0x5A 0x78 -> 0 (0x1D764) and
         the restarted round's mode-0x0C records with the count frozen. The W
         rows must be the route's existing harness writes only: no credit
         write, no poke at or after the screen opens, and the labelled
         incumbent poke before it. The prompt/countdown cells' pixels are the
         frame oracle's claim, not this check's.
  path   the port's T records reproduce those milestones at the capture's
         frames. Only fields gp_trace_line carries are compared: mode, w10d,
         cnt, cred, b1f and b25. The prompt's pixels are gp_compare's claim.

The KO is a harness poke. Neither a fought KO nor (for the plain check) a taken
continue is claimed; the `--taken` check's credit spend is the game's own.
Usage: gp_continue.py check --scenario gp-u28-continue --capture DIR
                          [--capture-sha256 H]
       gp_continue.py check --taken --scenario gp-u29-continue-taken
                          --capture DIR [--capture-sha256 H]
       gp_continue.py path  --scenario gp-u28-continue --capture DIR --port DIR
                          [--capture-sha256 H]
       gp_continue.py path  --taken --scenario gp-u29-continue-taken
                          --capture DIR --port DIR [--capture-sha256 H]"""
import argparse
import hashlib
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gp_session as gs

SCENARIO = 'gp-u28-continue'
TAKEN_SCENARIO = 'gp-u29-continue-taken'
KO_DS = 0x10780A
CREDIT_DS = 0x105C00
PATH_FIELDS = ('mode', 'w10d', 'cnt', 'cred', 'b1f', 'b25')
# The route's existing harness writes (record §L5, plan Task 4 ruling R-P10):
# the game-1 win's lands byte and P2 KO, the second P2 KO, and the incumbent KO
# that opens the screen. A capture may carry only these; the incumbent one is
# the screen's labelled opener (and is required before the screen opens).
ROUTE_W = (0x108106, 0x10789E, 0x10780A)
# How many mode-0x0C records the taken capture must hold after the take for the
# round restart to be in the window (measured: 0x5F; a floor, not the pin).
TAKEN_MIN_ROUND = 0x20


def snaps_from(lines):
    return gs.snapshots(lines)


def trace_from(lines):
    out = {}
    for line in lines:
        r = gs.parse(line)
        if r and r['kind'] == 'T':
            out.setdefault(r['f'], r)
    return out


def _continue_run(recs):
    fs = sorted(recs)
    first = next((f for f in fs if recs[f]['mode'] == 0x0E), None)
    if first is None:
        return None, []
    run = []
    for f in fs:
        if f < first:
            continue
        if recs[f]['mode'] != 0x0E:
            break
        run.append(recs[f])
    return first, run


def _take_record(recs, run):
    """The first record after the mode-0x0E run: the take (mode 0x0C) when the
    screen was taken, the expiry (mode 0x15) or anything else when it was not."""
    if not run:
        return None
    last = run[-1]['f']
    return next((recs[f] for f in sorted(recs) if f > last), None)


def milestones(recs):
    """The first mode-0x0E record, each new countdown value, then its expiry."""
    first, run = _continue_run(recs)
    if first is None:
        return []
    rows, previous = [], None
    for r in run:
        if not rows or r['cnt'] != previous:
            rows.append(r)
        previous = r['cnt']
    expiry = next((recs[f] for f in sorted(recs)
                   if f > first and recs[f]['mode'] == 0x15), None)
    if expiry is not None:
        rows.append(expiry)
    return rows


def taken_milestones(recs):
    """The first mode-0x0E record, each new countdown value, then the take."""
    first, run = _continue_run(recs)
    if first is None:
        return []
    rows, previous = [], None
    for r in run:
        if not rows or r['cnt'] != previous:
            rows.append(r)
        previous = r['cnt']
    take = _take_record(recs, run)
    if take is not None:
        rows.append(take)
    return rows


def evidence(name, lines, out=print, taken=False):
    """The capture-only check. Returns 0 ok, 1 FAIL."""
    if taken:
        return taken_evidence(name, lines, out)
    recs = snaps_from(lines)
    first, run = _continue_run(recs)
    if first is None:
        out('gp_continue: %s: evidence: FAIL: mode 0x0E never observed' % name)
        return 1
    ws = [r for r in (gs.parse(l) for l in lines) if r and r['kind'] == 'W']
    poke = next((w for w in ws if w['addr'] == KO_DS and w['now'].lower() == '78'
                 and w['f'] < first), None)
    if poke is None:
        out('gp_continue: %s: evidence: FAIL: no labelled KO poke wrote '
            '0x10780A=0x78 before mode 0x0E' % name)
        return 1
    for r in run:
        if r['w10d'] != 0:
            out('gp_continue: %s: evidence: FAIL: f=%X has w10d=%d, not the '
                'incumbent side 0 (0x272DC opens 0x0E on the KO of w10d)'
                % (name, r['f'], r['w10d']))
            return 1
        if r['cred'] != 4:
            out('gp_continue: %s: evidence: FAIL: f=%X has cred=%d, not the '
                'unchanged post-start credit count 4' % (name, r['f'], r['cred']))
            return 1
        if r['b1f'] != 0:
            out('gp_continue: %s: evidence: FAIL: f=%X has b1f=%d: the continue '
                'was taken, which is not this claim' % (name, r['f'], r['b1f']))
            return 1
    counts = [r['cnt'] for r in milestones(recs) if r['mode'] == 0x0E]
    if counts != list(range(0x0F, -1, -1)):
        out('gp_continue: %s: evidence: FAIL: countdown %r is not the complete '
            'descending 15->0 sequence (0x278C2, 0x27ACD..0x27AED)'
            % (name, counts))
        return 1
    prompt = next(r for r in run if r['cnt'] == 0x0D)
    expiry = next((recs[f] for f in sorted(recs)
                   if f > first and recs[f]['mode'] == 0x15), None)
    if expiry is None:
        out('gp_continue: %s: evidence: FAIL: the countdown expiry (mode 0x15) '
            'was not captured' % name)
        return 1
    if expiry['b25'] != 0:
        out('gp_continue: %s: evidence: FAIL: the expiry at f=%X has b25=%d, '
            'not 0 (0x27B1C)' % (name, expiry['f'], expiry['b25']))
        return 1
    if expiry['cred'] != 4:
        out('gp_continue: %s: evidence: FAIL: the expiry at f=%X has cred=%d, '
            'not 4' % (name, expiry['f'], expiry['cred']))
        return 1
    out('gp_continue: %s: evidence: labelled KO poke at f=%X; incumbent w10d=0, '
        'cred=4 unchanged, continue not taken ok' % (name, poke['f']))
    out('gp_continue: %s: evidence: countdown 15->0 (16/16 values), mode 0x0E '
        'at f=%X..%X ok' % (name, run[0]['f'], run[-1]['f']))
    out('gp_continue: %s: evidence: PRESS START prompt path at f=%X '
        '(cnt=13 < 14, cred=4 selects 0x2C0F4, not INSERT COIN) ok'
        % (name, prompt['f']))
    out('gp_continue: %s: evidence: expiry at f=%X: mode 0x15, b25=0 ok'
        % (name, expiry['f']))
    return 0


def taken_evidence(name, lines, out=print):
    """The taken-continue check (0x0E -> 0x2791C -> mode 0x0C). Returns 0 ok, 1 FAIL."""
    recs = snaps_from(lines)
    first, run = _continue_run(recs)
    if first is None:
        out('gp_continue: %s: evidence: FAIL: mode 0x0E never observed' % name)
        return 1
    take = _take_record(recs, run)
    if take is None:
        out('gp_continue: %s: evidence: FAIL: the continue screen never ended: '
            'no record after f=%X' % (name, run[-1]['f']))
        return 1
    if take['mode'] != 0x0C:
        out('gp_continue: %s: evidence: FAIL: f=%X has mode %02X after the 0x0E '
            'screen, not the 0x2791C take\'s mode 0x0C (0x27A15): the screen '
            'expired or was left, not taken'
            % (name, take['f'], take['mode']))
        return 1
    ws = [r for r in (gs.parse(l) for l in lines) if r and r['kind'] == 'W']
    credit = next((w for w in ws if CREDIT_DS <= w['addr'] < CREDIT_DS + 4), None)
    if credit is not None:
        out('gp_continue: %s: evidence: FAIL: a credit write at f=%X (0x%08X): '
            'the spend must be 0x42F60\'s own 0x2CA9C debit, not a poke'
            % (name, credit['f'], credit['addr']))
        return 1
    incumbent = [w for w in ws if w['addr'] == KO_DS and w['now'].lower() == '78'
                 and w['f'] < first]
    if len(incumbent) != 1:
        out('gp_continue: %s: evidence: FAIL: expected exactly one labelled '
            'incumbent KO poke (0x10780A=0x78) before mode 0x0E opens, found %d'
            % (name, len(incumbent)))
        return 1
    late = next((w for w in ws if w['f'] >= first), None)
    if late is not None:
        out('gp_continue: %s: evidence: FAIL: a poke at f=%X (0x%08X) at or '
            'after the continue screen opened: the take must not be poke-induced'
            % (name, late['f'], late['addr']))
        return 1
    unknown = next((w for w in ws if w['addr'] not in ROUTE_W), None)
    if unknown is not None:
        out('gp_continue: %s: evidence: FAIL: a poke at f=%X (0x%08X) is not one '
            'of the route\'s existing harness writes (0x108106/0x10789E/0x10780A)'
            % (name, unknown['f'], unknown['addr']))
        return 1
    for r in run + [take]:
        if r['w10d'] != 0:
            out('gp_continue: %s: evidence: FAIL: f=%X has w10d=%d, not the '
                'incumbent side 0 (0x272DC opens 0x0E on the KO of w10d)'
                % (name, r['f'], r['w10d']))
            return 1
    for r in run:
        if r['s0_5a'] != 0x78:
            out('gp_continue: %s: evidence: FAIL: the incumbent +0x5A at f=%X is '
                '%02X, not the labelled KO 0x78' % (name, r['f'], r['s0_5a']))
            return 1
        if r['cred'] != 4:
            out('gp_continue: %s: evidence: FAIL: f=%X has cred=%d, not the '
                'pre-take 4' % (name, r['f'], r['cred']))
            return 1
        if r['b1f'] != 0:
            out('gp_continue: %s: evidence: FAIL: f=%X has b1f=%d, not the '
                'screen-opening 0 (0x278BC)' % (name, r['f'], r['b1f']))
            return 1
    counts = [r['cnt'] for r in milestones(recs) if r['mode'] == 0x0E]
    want = list(range(0x0F, take['cnt'] - 1, -1))
    if counts != want:
        out('gp_continue: %s: evidence: FAIL: countdown %r is not %r (0x278C2 '
            'starts 0x0F; 0x27ACD..0x27AED decrements every 64 frames)'
            % (name, counts, want))
        return 1
    if not (0 <= take['cnt'] < 0x0E):
        out('gp_continue: %s: evidence: FAIL: the take at f=%X holds cnt=%02X; '
            'the PRESS START prompt path is only below 0x0E (0x27B59)'
            % (name, take['f'], take['cnt']))
        return 1
    if run[-1]['f'] >= take['f'] or run[-1]['cnt'] != take['cnt']:
        out('gp_continue: %s: evidence: FAIL: the prompt arm (cnt=%02X at f=%X) '
            'does not precede the take at f=%X'
            % (name, run[-1]['cnt'], run[-1]['f'], take['f']))
        return 1
    if take['cred'] != 3:
        out('gp_continue: %s: evidence: FAIL: the take at f=%X has cred=%d, not '
            '3: 0x42F60 must spend one credit (0x2CA9C)' % (name, take['f'], take['cred']))
        return 1
    if take['b1f'] != 1:
        out('gp_continue: %s: evidence: FAIL: the take at f=%X has b1f=%d, not '
            '(w10d != 0) + 1 = 1 (0x27A40)' % (name, take['f'], take['b1f']))
        return 1
    if take['s0_5a'] != 0:
        out('gp_continue: %s: evidence: FAIL: the take at f=%X left the '
            'incumbent +0x5A = %02X: 0x1D764 (0x279B6) must clear it to 0 for '
            'the round to restart' % (name, take['f'], take['s0_5a']))
        return 1
    post = [recs[f] for f in sorted(recs) if f > take['f']]
    if len(post) < TAKEN_MIN_ROUND:
        out('gp_continue: %s: evidence: FAIL: only %d record(s) after the take; '
            'the restarted round needs at least %d in the window'
            % (name, len(post), TAKEN_MIN_ROUND))
        return 1
    for r in post:
        if r['mode'] != 0x0C:
            out('gp_continue: %s: evidence: FAIL: f=%X has mode %02X after the '
                'take, not the restarted round\'s 0x0C' % (name, r['f'], r['mode']))
            return 1
        if r['cnt'] != take['cnt']:
            out('gp_continue: %s: evidence: FAIL: f=%X has cnt=%02X after the '
                'take, not the frozen %02X: the 0x0E countdown did not stop'
                % (name, r['f'], r['cnt'], take['cnt']))
            return 1
        if r['cred'] != 3:
            out('gp_continue: %s: evidence: FAIL: f=%X has cred=%d after the '
                'take, not the spent 3' % (name, r['f'], r['cred']))
            return 1
        if r['b1f'] != 1:
            out('gp_continue: %s: evidence: FAIL: f=%X has b1f=%d after the '
                'take, not 1' % (name, r['f'], r['b1f']))
            return 1
    out('gp_continue: %s: evidence: labelled incumbent KO poke at f=%X, no '
        'credit write and no poke at/after the screen; W rows are the route\'s '
        'existing harness writes ok' % (name, incumbent[0]['f']))
    out('gp_continue: %s: evidence: take at f=%X: mode 0x0E -> 0x0C, incumbent '
        'w10d=0, +0x5A 0x78 -> 0, cred 4 -> 3, b1f 0 -> 1 ok'
        % (name, take['f']))
    out('gp_continue: %s: evidence: prompt arm cnt=%02X at f=%X (below 0x0E); '
        'countdown %s ok'
        % (name, take['cnt'], run[-1]['f'],
           ' -> '.join('%02X' % c for c in counts)))
    out('gp_continue: %s: evidence: %d mode-0x0C records after the take, cnt '
        'frozen at %02X: the round restarted ok'
        % (name, len(post), take['cnt']))
    return 0


def _difference(c, p):
    if p is None:
        return 'missing port record'
    wrong = ['%s=%s (want %s)' % (n, p.get(n, 'missing'), c.get(n))
             for n in PATH_FIELDS if p.get(n) != c.get(n)]
    return ', '.join(wrong)


def path(name, cl, pl, out=print, taken=False):
    """The port path claim over trace-carried fields only. Returns 0 ok, 1 FAIL."""
    cap, port = snaps_from(cl), trace_from(pl)
    rows = taken_milestones(cap) if taken else milestones(cap)
    if taken:
        if not rows or rows[-1]['mode'] != 0x0C:
            out('gp_continue: %s: path: FAIL: the capture does not hold the '
                'take (mode 0x0C after the 0x0E screen)' % name)
            return 1
    elif len(rows) != 17:
        out('gp_continue: %s: path: FAIL: the capture does not hold the 16 '
            'countdown milestones plus the expiry' % name)
        return 1
    count = 0
    for c in rows:
        p = port.get(c['f'])
        detail = _difference(c, p)
        ok = not detail
        count += ok
        out('gp_continue: %s: path: f=%X mode=%02X cnt=%02X: %s%s'
            % (name, c['f'], c['mode'], c['cnt'], 'ok' if ok else 'FAIL',
               '' if ok else ': ' + detail))
    out('gp_continue: %s: path: %d/%d milestones reproduced' % (name, count, len(rows)))
    return 0 if count == len(rows) else 1


def sha_ok(name, cap_dir, sha, out):
    if sha is None:
        return True
    with open(os.path.join(cap_dir, 'poll.log'), 'rb') as f:
        got = hashlib.sha256(f.read()).hexdigest()
    if sha == '' or got != sha:
        out('gp_continue: %s: capture: FAIL: poll.log sha256 %s != the pinned %s: '
            're-measure, then re-pin' % (name, got, sha or '(unpinned)'))
        return False
    return True


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('cmd', choices=('check', 'path'))
    ap.add_argument('--scenario', required=True, choices=(SCENARIO, TAKEN_SCENARIO))
    ap.add_argument('--capture', required=True)
    ap.add_argument('--port')
    ap.add_argument('--taken', action='store_true',
                    help='the taken-continue branch (%s)' % TAKEN_SCENARIO)
    ap.add_argument('--capture-sha256')
    a = ap.parse_args()
    if a.taken != (a.scenario == TAKEN_SCENARIO):
        print('gp_continue: %s: --taken selects the %s scenario only'
              % (a.scenario, TAKEN_SCENARIO))
        return 1
    poll = os.path.join(a.capture, 'poll.log')
    if not os.path.isfile(poll):
        print('gp_continue: %s: no capture at %s (skipped)' % (a.scenario, poll))
        return 0
    if not sha_ok(a.scenario, a.capture, a.capture_sha256, print):
        return 1
    with open(poll) as f:
        cl = f.read().splitlines()
    if a.cmd == 'check':
        return evidence(a.scenario, cl, taken=a.taken)
    trace = None if a.port is None else os.path.join(a.port, 'trace.txt')
    if trace is None or not os.path.isfile(trace):
        print('gp_continue: %s: path: FAIL: no port trace at %s' % (a.scenario, a.port))
        return 1
    with open(trace) as f:
        pl = f.read().splitlines()
    return path(a.scenario, cl, pl, taken=a.taken)


if __name__ == '__main__':
    sys.exit(main())
