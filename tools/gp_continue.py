#!/usr/bin/env python3
"""Live truth Task 5 (record 2026-10-29-live-truth-derivations.md §L5):
the continue screen 0x0E, its PRESS START prompt/countdown and its expiry.
Stdlib only.

  check  evidence: the capture opens mode 0x0E on the incumbent side (w10d=0)
         after the labelled 0x10780A=0x78 KO poke, keeps cred=4 and b1f=0 (no
         continue taken), counts 15 down to 0, takes the credit-ready PRESS
         START prompt arm at cnt=13 (the raw gate is cnt < 0x0E), and expires
         to mode 0x15 with b25=0.
  path   the port's T records reproduce those milestones at the capture's
         frames. Only fields gp_trace_line carries are compared: mode, w10d,
         cnt, cred, b1f and b25. The prompt's pixels are gp_compare's claim.

The KO is a harness poke. Neither a fought KO nor a taken continue is claimed.
Usage: gp_continue.py check --scenario gp-u28-continue --capture DIR
                          [--capture-sha256 H]
       gp_continue.py path  --scenario gp-u28-continue --capture DIR --port DIR
                          [--capture-sha256 H]"""
import argparse
import hashlib
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gp_session as gs

SCENARIO = 'gp-u28-continue'
KO_DS = 0x10780A
PATH_FIELDS = ('mode', 'w10d', 'cnt', 'cred', 'b1f', 'b25')


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


def evidence(name, lines, out=print):
    """The capture-only check. Returns 0 ok, 1 FAIL."""
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


def _difference(c, p):
    if p is None:
        return 'missing port record'
    wrong = ['%s=%s (want %s)' % (n, p.get(n, 'missing'), c.get(n))
             for n in PATH_FIELDS if p.get(n) != c.get(n)]
    return ', '.join(wrong)


def path(name, cl, pl, out=print):
    """The port path claim over trace-carried fields only. Returns 0 ok, 1 FAIL."""
    cap, port = snaps_from(cl), trace_from(pl)
    rows = milestones(cap)
    if len(rows) != 17:
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
    out('gp_continue: %s: path: %d/17 milestones reproduced' % (name, count))
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
    ap.add_argument('--scenario', required=True, choices=(SCENARIO,))
    ap.add_argument('--capture', required=True)
    ap.add_argument('--port')
    ap.add_argument('--capture-sha256')
    a = ap.parse_args()
    poll = os.path.join(a.capture, 'poll.log')
    if not os.path.isfile(poll):
        print('gp_continue: %s: no capture at %s (skipped)' % (a.scenario, poll))
        return 0
    if not sha_ok(a.scenario, a.capture, a.capture_sha256, print):
        return 1
    with open(poll) as f:
        cl = f.read().splitlines()
    if a.cmd == 'check':
        return evidence(a.scenario, cl)
    trace = None if a.port is None else os.path.join(a.port, 'trace.txt')
    if trace is None or not os.path.isfile(trace):
        print('gp_continue: %s: path: FAIL: no port trace at %s' % (a.scenario, a.port))
        return 1
    with open(trace) as f:
        pl = f.read().splitlines()
    return path(a.scenario, cl, pl)


if __name__ == '__main__':
    sys.exit(main())
