#!/usr/bin/env python3
"""Live truth Task 7 (record 2026-10-29-live-truth-derivations.md §L7):
the high-score name-entry path of gp-u27-name-entry. Stdlib only.

  check  evidence: the capture's S records show mode 0x1E's six measured
         sub-state records, the three letter counts, the name bytes A/BA/DBA,
         the finaliser's space-padded "ABD ", the score and the return to mode 3.
  path   the port's T records reproduce only the six b25 values at the capture's
         frames. ncnt/cell0/name are not in gp_trace_line, so path never asserts
         them; the capture-only check owns those claims and gp_compare owns the
         rendered finaliser claim.

Usage: gp_u27.py check --scenario gp-u27-name-entry --capture DIR [--capture-sha256 H]
       gp_u27.py path  --scenario gp-u27-name-entry --capture DIR --port DIR
                        [--capture-sha256 H]"""
import argparse
import hashlib
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gp_session as gs


SCENARIO = 'gp-u27-name-entry'
SCORE = 0x005217A0
SUBSTATES = (
    (0x217E, 0x00),
    (0x217F, 0x04),
    (0x2183, 0x05),
    (0x22CE, 0x06),
    (0x23FC, 0x07),
    (0x23FD, 0x0A),
)
LETTER_COUNTS = ((0x2193, 1), (0x21B0, 2), (0x21CE, 3))
NAME_BYTES = (
    (0x220D, 0x00000041),
    (0x2226, 0x00004241),
    (0x2240, 0x00444241),
    (0x2243, 0x20444241),
)
END_FRAME = 0x24B9


def snaps_from(lines):
    return gs.snapshots(lines)


def trace_from(lines):
    out = {}
    for line in lines:
        r = gs.parse(line)
        if r and r['kind'] == 'T':
            out.setdefault(r['f'], r)
    return out


def _first(recs, field, value):
    return next((f for f in sorted(recs) if recs[f].get(field) == value), None)


def _row(recs, f, expected):
    r = recs.get(f)
    if r is None:
        return False, 'missing record'
    wrong = ['%s=%s (want %s)' % (n, r.get(n, 'missing'), v)
             for n, v in expected.items() if r.get(n) != v]
    return not wrong, ', '.join(wrong)


def evidence(name, lines, out=print):
    """The capture-only check. Returns 0 ok, 1 FAIL."""
    recs = snaps_from(lines)
    if not recs:
        out('gp_u27: %s: evidence: FAIL: no S records' % name)
        return 1
    failures = 0

    for f, value in SUBSTATES:
        ok, detail = _row(recs, f, {'mode': 0x1E, 'b25': value, 'sc0': SCORE})
        failures += not ok
        out('gp_u27: %s: evidence: sub-state f=%X b25=%02X: %s%s'
            % (name, f, value, 'ok' if ok else 'FAIL', '' if ok else ': ' + detail))
    out('gp_u27: %s: evidence: %d/6 sub-states ok'
        % (name, sum(_row(recs, f, {'mode': 0x1E, 'b25': v, 'sc0': SCORE})[0]
                     for f, v in SUBSTATES)))

    count_ok = 0
    for f, value in LETTER_COUNTS:
        ok, detail = _row(recs, f, {'mode': 0x1E, 'ncnt': value, 'sc0': SCORE})
        first = _first(recs, 'ncnt', value)
        ok = ok and first == f
        count_ok += ok
        failures += not ok
        out('gp_u27: %s: evidence: letter count f=%X ncnt=%d: %s%s'
            % (name, f, value, 'ok' if ok else 'FAIL',
               '' if ok else ': ' + (detail or 'first occurrence is %s, want f=%X' % (first, f))))
    out('gp_u27: %s: evidence: %d/3 letter counts ok' % (name, count_ok))

    name_ok = 0
    for f, value in NAME_BYTES:
        ok, detail = _row(recs, f, {'name': value, 'sc0': SCORE})
        first = _first(recs, 'name', value)
        ok = ok and first == f
        name_ok += ok
        failures += not ok
        label = 'finaliser "ABD" (space-padded)' if f == NAME_BYTES[-1][0] else 'name bytes'
        out('gp_u27: %s: evidence: %s f=%X name=%08X: %s%s'
            % (name, label, f, value, 'ok' if ok else 'FAIL',
               '' if ok else ': ' + (detail or 'first occurrence is %s, want f=%X' % (first, f))))
    out('gp_u27: %s: evidence: %d/4 name-byte stages ok' % (name, name_ok))

    ok, detail = _row(recs, END_FRAME, {'mode': 3, 'b25': 0, 'name': NAME_BYTES[-1][1],
                                       'sc0': SCORE})
    failures += not ok
    out('gp_u27: %s: evidence: return to mode 3 at f=%X: %s%s'
        % (name, END_FRAME, 'ok' if ok else 'FAIL', '' if ok else ': ' + detail))
    out('gp_u27: %s: evidence: %s' % (name, 'ok' if not failures else
                                      'FAIL: %d check(s) failed' % failures))
    return 1 if failures else 0


def _b25(snap):
    return '--' if 'b25' not in snap else '%02X' % snap['b25']


def path(name, cl, pl, out=print):
    """The port path claim, deliberately b25-only. Returns 0 ok, 1 FAIL."""
    cap, port = snaps_from(cl), trace_from(pl)
    count = 0
    for f, value in SUBSTATES:
        c, p = cap.get(f), port.get(f)
        ok = (c is not None and c.get('b25') == value and
              p is not None and p.get('b25') == value)
        count += ok
        out('gp_u27: %s: path: f=%X b25=%02X capture %s port %s: %s'
            % (name, f, value,
               'missing' if c is None else _b25(c),
               'missing' if p is None else _b25(p),
               'ok' if ok else 'FAIL'))
    out('gp_u27: %s: path: %d/6 sub-states reproduced' % (name, count))
    return 0 if count == len(SUBSTATES) else 1


def sha_ok(name, cap_dir, sha, out):
    if sha is None:
        return True
    with open(os.path.join(cap_dir, 'poll.log'), 'rb') as f:
        got = hashlib.sha256(f.read()).hexdigest()
    if sha == '' or got != sha:
        out('gp_u27: %s: capture: FAIL: poll.log sha256 %s != the pinned %s: '
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
    if not os.path.isfile(os.path.join(a.capture, 'poll.log')):
        print('gp_u27: %s: no capture at %s (skipped)' % (a.scenario, a.capture))
        return 0
    if not sha_ok(a.scenario, a.capture, a.capture_sha256, print):
        return 1
    with open(os.path.join(a.capture, 'poll.log')) as f:
        cl = f.read().splitlines()
    if a.cmd == 'check':
        return evidence(a.scenario, cl)
    if a.port is None or not os.path.isfile(os.path.join(a.port, 'trace.txt')):
        print('gp_u27: %s: path: FAIL: no port trace at %s' % (a.scenario, a.port))
        return 1
    with open(os.path.join(a.port, 'trace.txt')) as f:
        pl = f.read().splitlines()
    return path(a.scenario, cl, pl)


if __name__ == '__main__':
    sys.exit(main())
