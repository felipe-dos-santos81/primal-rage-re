#!/usr/bin/env python3
"""U11 in-match keys (plan docs/superpowers/plans/2026-10-01-gameplay-u11-in-match-keys.md,
record docs/superpowers/plans/2026-10-01-gameplay-u11-derivations.md §K.6).
Two checks of the gp-keys-fight capture, one row per key event:

  evidence  the capture's S records (poll.log) show each event's raw-derived
            effect: the original did what the raw says;
  effects   the port's T records (trace.txt of the PR_GP_DUMP replay) show the
            same effect at the same frames: the port did what the original did.
            A ratchet: the events before the first one the port does not
            reproduce must number at least --min-effects N.

An event is one or more scenario steps whose BIOS words are consumed in one
iteration c (a prompt's or the pause's answer is queued in its opener's spin).
c pairs each I press with its H record FIFO, as gp_session.port_script does.
A pad event's frame F is the first S record after its press whose raw word
carries its kb bit; the level, and so DS_001088E4, follows at F + 1 (0x500C4,
record gameplay-ground-truth §G.1.1). Both sides are judged at the capture's
c and F (the port script queues each word at c and applies the bits at F).
An absent capture skips (exit 0); a present capture with another poll.log
than --capture-sha256 pins, an empty (unpinned) --capture-sha256 or an
unpinned N fails. Stdlib only."""
import argparse
import hashlib
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gp_session as gs

SCENARIO = 'gp-keys-fight'
RESTART_SEED = 0xABCD   # 0x20C53 mov ebx,0xABCD; 0x20C62 mov [0xEF6D8],ebx (the restart tail)
JOIN_MODE = 0x17        # 0x28E3A mov ecx,0x17; 0x28E66 mov [0x104B00],cx (0x28DA4)
P1_B0_NEW = 0x01000000  # 0x9ACBC[0]: side 0's start mask, F1's and U's bit (record §G.7.2)
LOOKAHEAD = 8           # harness: frames searched past F + 1 for a state (snapshot gaps, spec §3.7)

# (label, first scenario step, steps in the event, rule); record §K.6 derives each rule.
EVENTS = (
    ('enter', 3, 1, ('latched', 0x0D)),
    ('pause', 4, 2, ('cleared',)),
    ('alt-s on', 6, 1, ('toggled', 0x1F, 'spz', 'mpz')),
    ('esc-n', 7, 2, ('cleared',)),
    ('alt-m on', 9, 1, ('toggled', 0x32, 'mpz', 'spz')),
    ('altq-n', 10, 2, ('cleared',)),
    ('alt-s off', 12, 1, ('toggled', 0x1F, 'spz', 'mpz')),
    ('alt-m off', 13, 1, ('toggled', 0x32, 'mpz', 'spz')),
    ('f1', 14, 1, ('b0', 0x3B, 0x0100)),
    ('f2', 15, 1, ('join', 0x3C, 0x0001)),
    ('esc-y', 16, 2, ('restart',)),
)


def records(lines, kind):
    """f -> the first record of `kind` ('S' capture, 'T' port) at that f."""
    out = {}
    for line in lines:
        r = gs.parse(line)
        if r and r['kind'] == kind:
            out.setdefault(r['f'], r)
    return out


def frames(cap_lines):
    """step -> (press f, [consumption frame of each BIOS word]) from the capture."""
    recs = [r for r in (gs.parse(l) for l in cap_lines) if r]
    presses = [r for r in recs if r['kind'] == 'I' and 'press' in r and r.get('bios') is not None]
    heads = [r for r in recs if r['kind'] == 'H']
    out = {}
    for k, p in enumerate(presses):
        c = heads[k]['f'] if k < len(heads) else None
        out.setdefault(p['step'], (p['f'], []))[1].append(c)
    return out


def _first(rec, lo, hi):
    """The record with the smallest f in [lo, hi], or None."""
    for f in range(lo, hi + 1):
        if f in rec:
            return rec[f]
    return None


def _pad_frame(cap, press_f, bit):
    """F: the first capture S record after the press whose raw carries `bit`."""
    for f in range(press_f + 1, press_f + 1 + LOOKAHEAD):
        r = cap.get(f)
        if r is not None and gs.raw_to_kb(r['raw']) & bit:
            return f
    return None


def _latch(rec, c, want):
    """[] when the latch DS_00105F30 changes to `want` in iteration c."""
    prev, now = rec.get(c - 1), rec.get(c)
    if prev is None or now is None:
        return ['no record at f=%X or f=%X' % (c - 1, c)]
    why = []
    if prev['lat'] == want:
        why.append('lat already %X at f=%X: the event cannot show' % (want, c - 1))
    if now['lat'] != want:
        why.append('lat %X at f=%X, want %X' % (now['lat'], c, want))
    return why


def judge(rule, c, F, rec, boot_cred):
    """[] when `rec` shows the rule's effect at c (and F), else the reasons."""
    kind = rule[0]
    if kind == 'restart':
        prev = rec.get(c - 1)
        if prev is None:
            return ['no record at f=%X' % (c - 1)]
        why = []
        if prev['mode'] in (0x03, 0x27):
            why.append('mode %X before the key (0x24EB7 needs another mode)' % prev['mode'])
        if prev['cred'] == boot_cred:
            why.append('cred %X before the key equals the boot value: nothing to restore' % prev['cred'])
        if c in rec:
            why.append('a record at f=%X: 0x24AB0 abandons that iteration before its spin' % c)
        after = next((rec[f] for f in sorted(rec) if f > c), None)
        if after is None:
            return why + ['no record after f=%X' % c]
        if (after['mode'], after['rng'], after['cred']) != (0x03, RESTART_SEED, boot_cred):
            why.append('mode/rng/cred %X/%X/%X at f=%X, want 3/%X/%X'
                       % (after['mode'], after['rng'], after['cred'], after['f'], RESTART_SEED, boot_cred))
        return why
    want = 0 if kind == 'cleared' else rule[1]
    why = _latch(rec, c, want)
    if why and why[0].startswith('no record'):
        return why
    prev, now = rec[c - 1], rec[c]
    if kind in ('latched', 'cleared', 'toggled'):
        if prev['mode'] in (0x03, 0x27):
            why.append('mode %X before the key (the arms need a fight)' % prev['mode'])
        if now['mode'] != prev['mode']:
            why.append('mode %X -> %X' % (prev['mode'], now['mode']))
        if kind == 'toggled':
            fld, other = rule[2], rule[3]
            if prev[fld] not in (0, 1) or now[fld] != prev[fld] ^ 1:
                why.append('%s %X -> %X, want a flip' % (fld, prev[fld], now[fld]))
            if now[other] != prev[other]:
                why.append('%s %X -> %X, want unchanged' % (other, prev[other], now[other]))
        elif kind == 'cleared':
            for fld in ('spz', 'mpz'):
                if now[fld] != prev[fld]:
                    why.append('%s %X -> %X, want unchanged (0x1D250/0x1D270 pair)' % (fld, prev[fld], now[fld]))
        return why
    if F is None:
        return why + ['the pad bit never reached raw']
    before = rec.get(F - 1)
    if before is None:
        return why + ['no record at f=%X' % (F - 1)]
    if kind == 'b0':
        edge = rec.get(F + 1)
        if edge is None:
            return why + ['no record at f=%X' % (F + 1)]
        if not edge['new'] & P1_B0_NEW or edge['e0'] & 0x0101 != 0x0101:
            why.append('new %08X e0 %04X at f=%X, want P1 b0 newly pressed' % (edge['new'], edge['e0'], F + 1))
        if edge['mode'] != before['mode'] or edge['b1f'] != before['b1f']:
            why.append('mode/b1f %X/%X -> %X/%X, want no join' % (before['mode'], before['b1f'],
                                                                 edge['mode'], edge['b1f']))
        return why
    after = _first(rec, F + 1, F + 1 + LOOKAHEAD)
    if after is None:
        return why + ['no record in f=%X..%X' % (F + 1, F + 1 + LOOKAHEAD)]
    if before['b1f'] & 2:
        why.append('side 1 already in (b1f %X)' % before['b1f'])
    if after['mode'] != JOIN_MODE or after['b1f'] != before['b1f'] | 2:
        why.append('mode/b1f %X/%X at f=%X, want %X/%X' % (after['mode'], after['b1f'], after['f'],
                                                          JOIN_MODE, before['b1f'] | 2))
    if after['cred'] != before['cred']:
        why.append('cred %X -> %X, want unchanged (0x2CA93)' % (before['cred'], after['cred']))
    return why


def judge_all(cap_lines, rec):
    """[(label, c, F, reasons)] for every event, judged on `rec` at the capture's frames."""
    fr = frames(cap_lines)
    cap = records(cap_lines, 'S')
    # The restart's cred expectation is the CAPTURE's first record: the port's own
    # trace must not supply the value it is judged against.
    boot_cred = cap[min(cap)]['cred'] if cap else None
    out = []
    for label, step, n, rule in EVENTS:
        got = [fr.get(s) for s in range(step, step + n)]
        if any(g is None for g in got):
            out.append((label, None, None, ['step %d never pressed' % step]))
            continue
        cs = [c for _, cl in got for c in cl]
        c = cs[0]
        if c is None or any(x != c for x in cs):
            out.append((label, c, None, ['words consumed in frames %s, want one' % cs]))
            continue
        F = _pad_frame(cap, got[0][0], rule[2]) if rule[0] in ('b0', 'join') else None
        out.append((label, c, F, judge(rule, c, F, rec, boot_cred)))
    return out


def _print(name, what, rows):
    for label, c, F, why in rows:
        at = 'f=%s' % ('-' if c is None else '%X' % c) + ('' if F is None else ' F=%X' % F)
        print('gp_keys: %s: %s: %-9s %s %s' % (name, what, label, at, 'ok' if not why else 'FAIL: ' + '; '.join(why)))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('cmd', choices=('evidence', 'effects'))
    ap.add_argument('--scenario', default=SCENARIO)
    ap.add_argument('--capture', required=True)
    ap.add_argument('--port')
    ap.add_argument('--min-effects', default=None)
    ap.add_argument('--capture-sha256', default=None,
                    help="the pinned sha256 of the capture's poll.log; with it another capture fails")
    a = ap.parse_args()
    log = os.path.join(a.capture, 'poll.log')
    if not os.path.isfile(log):
        print('gp_keys: %s: no capture at %s, skipped' % (a.scenario, a.capture))
        return 0
    with open(log, 'rb') as f:
        blob = f.read()
    if a.capture_sha256 is not None:
        sha = hashlib.sha256(blob).hexdigest()
        if a.capture_sha256 == '':
            print('gp_keys: %s: FAIL: --capture-sha256 is not pinned (this poll.log: %s)' % (a.scenario, sha))
            return 1
        if sha != a.capture_sha256:
            print('gp_keys: %s: FAIL: poll.log sha256 %s, pinned %s (re-measure and re-pin)'
                  % (a.scenario, sha, a.capture_sha256))
            return 1
    cap_lines = blob.decode().splitlines()
    if a.cmd == 'evidence':
        rows = judge_all(cap_lines, records(cap_lines, 'S'))
        _print(a.scenario, 'evidence', rows)
        bad = sum(1 for r in rows if r[3])
        print('gp_keys: %s: evidence: %d of %d events %s' % (a.scenario, len(rows) - bad, len(rows),
                                                         'ok' if not bad else 'FAIL'))
        return 1 if bad else 0
    trace = os.path.join(a.port or '', 'trace.txt')
    if not os.path.isfile(trace):
        print('gp_keys: %s: effects: FAIL: no port trace at %s' % (a.scenario, trace))
        return 1
    with open(trace) as f:
        rows = judge_all(cap_lines, records(f.read().splitlines(), 'T'))
    _print(a.scenario, 'effects', rows)
    first = next((k for k, r in enumerate(rows) if r[3]), len(rows))
    if a.min_effects in (None, ''):
        print('gp_keys: %s: effects: FAIL: --min-effects is not pinned (first not reproduced: %d)' % (a.scenario, first))
        return 1
    n = int(a.min_effects)
    if n > len(rows):
        print('gp_keys: %s: effects: FAIL: ratchet N %d > %d events' % (a.scenario, n, len(rows)))
        return 1
    if first < n:
        print('gp_keys: %s: effects: FAIL: first not reproduced %d < ratchet N %d' % (a.scenario, first, n))
        return 1
    print('gp_keys: %s: effects: first not reproduced %d, ratchet N %d ok%s'
          % (a.scenario, first, n, ' (improved: raise N)' if first > n else ''))
    return 0


if __name__ == '__main__':
    sys.exit(main())
