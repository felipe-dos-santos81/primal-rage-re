#!/usr/bin/env python3
"""The K11 service-menu oracle (plan 2026-09-30-named-gaps-a-k11-harness.md).
It compares a DOSBox-X capture of the pinned original (tools/k11_capture.py,
data/k11-captures/<scenario>) with the port's PR_K11_DUMP frames
(test_k11_oracle). The comparison is byte-exact and uses title_compare's model:
clean, a byte splice of adjacent port frames, or one transition row.

The window runs from the first capture frame that exhibits a port frame at or
before the first settled screen (screens.txt `key 0 settled N`) to the last
capture frame that exhibits the final settled screen (`end settled N`). Each
claim is a failure when broken:
  1. the window is not empty;
  2. every capture frame in it is explained, except all-black frames (a
     documented capture artefact, port/spec/game_flow.md) and the frames
     K11_ALLOWED_UNEXPLAINED names with their record reference;
  3. every settled screen of screens.txt is exhibited by a capture frame in
     the window, so a port that renders less than the original fails.
--report prints the same, plus the first unexplained frames' difference boxes,
and always exits 0 (the evidence scenarios). An absent capture skips (exit 0)
unless PR_ORACLE_REQUIRED=1. Stdlib only; title_compare is read-only here."""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import title_compare as tc

# scenario -> {capture index: 'record §A.x: reason'}. Only plan Task 10 adds rows.
K11_ALLOWED_UNEXPLAINED = {}
REPORT_MAX = 5


def exhibited(kind, data):
    s = set()
    if kind == 'clean':
        s.add(data)
    elif kind == 'splice':
        for (N, lo, hi) in data:
            if hi > 0:
                s.add(N)
            if lo < tc.FRAME_BYTES:
                s.add(N + 1)
    elif kind == 'transition':
        for (N, _r, _a, _b) in data:
            s.update((N, N + 1))
    return s


def read_screens(path):
    keys, end = [], None
    with open(path) as f:
        for line in f:
            parts = line.split()
            if len(parts) == 4 and parts[0] == 'key' and parts[2] == 'settled':
                keys.append(int(parts[3]))
            elif len(parts) == 3 and parts[0] == 'end' and parts[1] == 'settled':
                end = int(parts[2])
    return keys, end


def diff_box(c, p):
    rows = [r for r in range(tc.FRAME_H) if c[r * tc.ROW:(r + 1) * tc.ROW] != p[r * tc.ROW:(r + 1) * tc.ROW]]
    if not rows:
        return None
    xs = [x for r in rows for x in range(tc.FRAME_W)
          if c[r * tc.ROW + 3 * x:r * tc.ROW + 3 * x + 3] != p[r * tc.ROW + 3 * x:r * tc.ROW + 3 * x + 3]]
    return rows[0], rows[-1], min(xs), max(xs), len(xs)


def nearest(ch, port_rows):
    best, best_m = -1, 0
    for m, ph in enumerate(port_rows):
        a, b = tc.row_common(ch, ph)
        if min(a + b, tc.FRAME_H) > best:
            best, best_m = min(a + b, tc.FRAME_H), m
    return best_m


def compare(name, frames, raws, port, port_rows, keys, end_settled, report):
    n = len(port)
    cache = {}

    def ex(j):
        if j not in cache:
            cache[j] = tc.explain(frames[j], tc.row_hashes(frames[j]), port, port_rows, n)
        return cache[j]

    first = keys[0] if keys else end_settled
    start = next((j for j in range(len(frames)) if any(frames[j])
                  and any(m <= first for m in exhibited(*ex(j)))), None)
    end = next((j for j in range(len(frames) - 1, -1, -1) if any(frames[j])
                and end_settled in exhibited(*ex(j))), None)
    if start is None or end is None or end < start:
        print("k11_compare: %s: window empty (start %s, end %s): the capture never exhibits the "
              "port's first or final settled screen" % (name, start, end))
        if not report or start is None:
            return 1
        end = len(frames) - 1
    allowed = K11_ALLOWED_UNEXPLAINED.get(name, {})
    counts = {'clean': 0, 'splice': 0, 'transition': 0, 'unexplained': 0}
    unexpl, black, exh = [], [], set()
    for j in range(start, end + 1):
        if not any(frames[j]):
            black.append(j)
            continue
        kind, data = ex(j)
        counts[kind] += 1
        if kind == 'unexplained':
            unexpl.append(j)
        else:
            exh |= exhibited(kind, data)
    wanted = sorted(set(keys + ([end_settled] if end_settled is not None else [])))
    missing = [m for m in wanted if m not in exh]
    bad = [j for j in unexpl if j not in allowed]
    print('k11_compare: %s: window distinct [%d..%d] (raw %d..%d)' % (name, start, end, raws[start], raws[end]))
    print('k11_compare: %s: %d frames in window: %d clean, %d splice, %d transition, %d unexplained, %d all-black'
          % (name, end - start + 1, counts['clean'], counts['splice'], counts['transition'],
             counts['unexplained'], len(black)))
    print('k11_compare: %s: settled screens exhibited %d/%d; missing %s'
          % (name, len(wanted) - len(missing), len(wanted), missing))
    if allowed and unexpl:
        print('k11_compare: %s: allowed by name: %s' % (name, sorted(j for j in unexpl if j in allowed)))
    if not bad:
        print('k11_compare: %s: 0 unexplained in the window' % name)
    for k, j in enumerate(bad[:REPORT_MAX if report else 1]):
        ch = tc.row_hashes(frames[j])
        m = nearest(ch, port_rows)
        box = diff_box(frames[j], port[m])
        label = 'FIRST UNEXPLAINED' if k == 0 else 'UNEXPLAINED'
        print('k11_compare: %s: %s capture %d (raw %d): nearest port %d, differs in rows %d..%d, x %d..%d (%d px)'
              % ((name, label, j, raws[j], m) + box))
    return 0 if not bad and not missing else 1


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--capture', required=True)
    ap.add_argument('--port', required=True)
    ap.add_argument('--scenario')
    ap.add_argument('--report', action='store_true')
    a = ap.parse_args()
    name = a.scenario or os.path.basename(os.path.normpath(a.capture))
    required = os.environ.get('PR_ORACLE_REQUIRED') == '1'
    if not os.path.isdir(a.capture):
        print('k11_compare: no capture at %s (%s)' % (a.capture, 'FAIL (required)' if required else 'skipped'))
        return 1 if required and not a.report else 0
    screens = os.path.join(a.port, 'screens.txt')
    if not os.path.isfile(screens):
        print('k11_compare: no port dump at %s (screens.txt missing)' % a.port)
        return 0 if a.report else 1
    frames = tc.load_frames(a.capture, name)
    port = tc.load_frames(a.port, 'port')
    if not frames or not port:
        print('k11_compare: %s: empty or malformed frames (capture %s, port %s)'
              % (name, None if frames is None else len(frames), None if port is None else len(port)))
        return 0 if a.report else 1
    raws = tc.raw_map(a.capture) or list(range(len(frames)))
    keys, end_settled = read_screens(screens)
    rc = compare(name, frames, raws, port, [tc.row_hashes(p) for p in port], keys, end_settled, a.report)
    return 0 if a.report else rc


if __name__ == '__main__':
    sys.exit(main())
