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
     the window, so a port that renders less than the original fails;
  4. no non-black capture frame follows the window's end, so the END is the
     capture's last content frame and a port that stops reacting to its last
     keys fails. The START still comes from the port's own dump.
--report prints the same, plus the first unexplained frames' difference boxes,
and always exits 0 (the evidence scenarios). An absent capture skips (exit 0)
unless --required is passed. The environment's PR_ORACLE_REQUIRED is ignored:
the K11 oracle is enforced like the front-end one and skips without its
capture, so a PR_ORACLE_REQUIRED=1 inherited from the caller must not turn a
missing git-ignored capture into a failure. Stdlib only; title_compare is
read-only here."""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import title_compare as tc

# scenario -> {capture index: 'record §A.x: reason'}. Only plan Task 10 adds rows.
# The walk's two rows are one shape: a glyph screen presented mid-draw, its new
# glyphs' pixels black (their palette not yet in the presented DAC); every other
# pixel equals one of the two port frames around it. Owner: the presented DAC
# state (fidelity-gaps §7.11), as title_compare's front-end frame 833.
K11_ALLOWED_UNEXPLAINED = {
    'walk': {
        138: 'record §A.5: MODIFY CONTROLS mid-draw, port 47/48 plus 940 black glyph px (rows 91..158)',
        151: 'record §A.5: TEST CONTROLS markers mid-draw, port 58/59 plus 88 black glyph px (rows 120..152)',
    },
}
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


def canonical(port):
    """Each port frame's first byte-identical frame: explain() names only the
    first of identical port frames, so a settled screen equal to an earlier
    one (the MAIN MENU drawn again after the walk) is credited through it
    (record §A.5)."""
    first, out = {}, []
    for m, data in enumerate(port):
        out.append(first.setdefault(data, m))
    return out


def compare(name, frames, raws, port, port_rows, keys, end_settled, report):
    n = len(port)
    cache = {}
    canon = canonical(port)

    def ex(j):
        if j not in cache:
            cache[j] = tc.explain(frames[j], tc.row_hashes(frames[j]), port, port_rows, n)
        return cache[j]

    first = keys[0] if keys else end_settled
    start = next((j for j in range(len(frames)) if any(frames[j])
                  and any(m <= first for m in exhibited(*ex(j)))), None)
    end = next((j for j in range(len(frames) - 1, -1, -1) if any(frames[j])
                and canon[end_settled] in {canon[m] for m in exhibited(*ex(j))}), None)
    if start is None or end is None or end < start:
        print("k11_compare: %s: window empty (start %s, end %s): the capture never exhibits the "
              "port's first or final settled screen" % (name, start, end))
        if not report or start is None:
            return 1
        end = len(frames) - 1
    # The END comes from the port's final screen, so a port that stops
    # reacting before the capture does would end the window early: every
    # non-black capture frame after it is a failure (review 1, record §A.5).
    past = next((j for j in range(end + 1, len(frames)) if any(frames[j])), None)
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
    exh = {canon[m] for m in exh}
    missing = [m for m in wanted if canon[m] not in exh]
    bad = [j for j in unexpl if j not in allowed]
    print('k11_compare: %s: window distinct [%d..%d] (raw %d..%d)' % (name, start, end, raws[start], raws[end]))
    print('k11_compare: %s: %d frames in window: %d clean, %d splice, %d transition, %d unexplained, %d all-black'
          % (name, end - start + 1, counts['clean'], counts['splice'], counts['transition'],
             counts['unexplained'], len(black)))
    print('k11_compare: %s: settled screens exhibited %d/%d; missing %s'
          % (name, len(wanted) - len(missing), len(wanted), missing))
    if allowed and unexpl:
        print('k11_compare: %s: allowed by name: %s' % (name, sorted(j for j in unexpl if j in allowed)))
    if past is not None:
        print("k11_compare: %s: capture continues past the port's final screen at %d (raw %d)"
              % (name, past, raws[past]))
    if not bad:
        print('k11_compare: %s: 0 unexplained in the window' % name)
    for k, j in enumerate(bad[:REPORT_MAX if report else 1]):
        ch = tc.row_hashes(frames[j])
        m = nearest(ch, port_rows)
        box = diff_box(frames[j], port[m])
        label = 'FIRST UNEXPLAINED' if k == 0 else 'UNEXPLAINED'
        print('k11_compare: %s: %s capture %d (raw %d): nearest port %d, differs in rows %d..%d, x %d..%d (%d px)'
              % ((name, label, j, raws[j], m) + box))
    return 0 if not bad and not missing and past is None else 1


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--capture', required=True)
    ap.add_argument('--port', required=True)
    ap.add_argument('--scenario')
    ap.add_argument('--report', action='store_true')
    ap.add_argument('--required', action='store_true',
                    help='fail (exit 1) when the capture is absent; PR_ORACLE_REQUIRED is not read')
    a = ap.parse_args()
    name = a.scenario or os.path.basename(os.path.normpath(a.capture))
    required = a.required
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
    if end_settled is None or any(m >= len(port) for m in keys + [end_settled]):
        print('k11_compare: %s: screens.txt names a frame the dump does not hold (%d frames)'
              % (name, len(port)))
        return 0 if a.report else 1
    rc = compare(name, frames, raws, port, [tc.row_hashes(p) for p in port], keys, end_settled, a.report)
    return 0 if a.report else rc


if __name__ == '__main__':
    sys.exit(main())
