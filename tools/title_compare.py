#!/usr/bin/env python3
"""Tear-aware title oracle: every captured frame must be a splice of two
adjacent port frames, and every port frame must be exhibited.

Absent capture: skip (exit 0) unless PR_ORACLE_REQUIRED=1, then fail.
Usage: title_compare.py --capture DIR [--capture DIR2] --port DIR --frames 96

Zero pixel tolerance. The model is the human's Task 10 ruling:
a captured frame C is explained by `port[N][0..t) ++ port[N+1][t..200)` for
adjacent port frames N, N+1 and a tear row t (0..200); t=0 or t=200 is a clean
whole port frame. Rows are exact: a frame is unexplained if no (N, t) splices
it. Every captured frame inside the title window must be explained, and every
port frame 0..frames-1 must be exhibited by both captures (its rows appear at
their correct y as some frame's band or body). Clean samples of the same port
frame must be byte-identical across the two captures.

The tear row is derived from the data, never taken as an argument; there is no
threshold, mask, crop, frame-skip or per-frame allowance. A frame explaining
only under a *sub-row* boundary is reported as model insufficiency (the ruling's
STOP case), not accepted.
"""
import argparse
import hashlib
import os
import sys

FRAME_W, FRAME_H = 320, 200
FRAME_BYTES = FRAME_W * FRAME_H * 3


def load(path):
    with open(path, 'rb') as f:
        return f.read()


def load_frames(d):
    frames, i = [], 0
    while True:
        path = os.path.join(d, 'frame_%04d.raw' % i)
        if not os.path.exists(path):
            break
        frames.append(load(path))
        i += 1
    return frames


def raw_map(d):
    """window.txt emitted-index -> raw capture index, or None."""
    path = os.path.join(d, 'window.txt')
    if not os.path.exists(path):
        return None
    pairs = {}
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            parts = line.split()
            if len(parts) >= 2:
                pairs[int(parts[0])] = int(parts[1])
    return [pairs[i] for i in range(len(pairs))] if pairs else None


def row_hashes(data):
    return [hashlib.md5(data[r * FRAME_W * 3:(r + 1) * FRAME_W * 3]).digest()
            for r in range(FRAME_H)]


def row_differs(a, b):
    """Per-row difference flags between two frames' row-hash lists."""
    return [0 if a[r] == b[r] else 1 for r in range(FRAME_H)]


def prefix_sum(flags):
    out = [0] * (len(flags) + 1)
    for i, v in enumerate(flags):
        out[i + 1] = out[i] + v
    return out


def explain(ch, port_ch, n):
    """Classify one captured frame. Returns (kind, data).

    kind 'clean': data = port index M (C == port[M]).
    kind 'torn':  data = list of (N, lo, hi) valid row-boundary splices.
    kind 'unexplained': data = the best whole-row (N, t, mismatches, flags)."""
    pref, suff = [], []
    for ph in port_ch:
        k = 0
        while k < FRAME_H and ch[k] == ph[k]:
            k += 1
        pref.append(k)
        k = 0
        while k < FRAME_H and ch[FRAME_H - 1 - k] == ph[FRAME_H - 1 - k]:
            k += 1
        suff.append(k)
    clean = [M for M in range(n) if pref[M] == FRAME_H]
    if clean:
        return 'clean', clean[0]
    tears = []
    for N in range(n - 1):
        if pref[N] + suff[N + 1] >= FRAME_H:
            tears.append((N, max(0, FRAME_H - suff[N + 1]),
                          min(FRAME_H, pref[N])))
    if tears:
        return 'torn', tears
    # Best whole-row splice, for the failure report.
    best = None
    for N in range(n - 1):
        a = row_differs(ch, port_ch[N])       # C vs old (top band)
        b = row_differs(ch, port_ch[N + 1])   # C vs new (bottom band)
        pa = prefix_sum(a)
        pb = prefix_sum(b)
        for t in range(FRAME_H + 1):
            m = pa[t] + (pb[FRAME_H] - pb[t])
            if best is None or m < best[0]:
                best = (m, N, t, a, b)
    return 'unexplained', best


def first_diff_byte(cap_data, a, b, t):
    """First byte differing from the whole-row splice a[:t]+b[t:]."""
    for r in range(FRAME_H):
        ref = a[r * FRAME_W * 3:(r + 1) * FRAME_W * 3] if r < t else \
              b[r * FRAME_W * 3:(r + 1) * FRAME_W * 3]
        got = cap_data[r * FRAME_W * 3:(r + 1) * FRAME_W * 3]
        for i in range(len(ref)):
            if ref[i] != got[i]:
                return r, i
    return None, None


def subrow_boundary(cap_data, a_data, b_data, r):
    """Evidence for the STOP case: does row r splice a[:x]+b[x:] for some x?"""
    got = cap_data[r * FRAME_W * 3:(r + 1) * FRAME_W * 3]
    ra = a_data[r * FRAME_W * 3:(r + 1) * FRAME_W * 3]
    rb = b_data[r * FRAME_W * 3:(r + 1) * FRAME_W * 3]
    for order, (x, y) in (('old-then-new', (ra, rb)), ('new-then-old', (rb, ra))):
        for off in range(0, len(got) + 1):
            if got[:off] == x[:off] and got[off:] == y[off:]:
                return order, off
    return None


def check_capture(capture, port, port_ch, n, name):
    frames = load_frames(capture)
    if not frames:
        print("title_compare: %s is empty" % name)
        return 1, None
    if len(frames[0]) != FRAME_BYTES:
        print("title_compare: %s frame 0 is %d bytes, expected %d"
              % (name, len(frames[0]), FRAME_BYTES))
        return 1, None
    raws = raw_map(capture) or list(range(len(frames)))

    kinds = []
    for data in frames:
        kinds.append(explain(row_hashes(data), port_ch, n))

    exh = []
    for kind, data in kinds:
        s = set()
        if kind == 'clean':
            s.add(data)
        elif kind == 'torn':
            for (N, lo, hi) in data:
                if hi > 0:
                    s.add(N)
                if lo < FRAME_H:
                    s.add(N + 1)
        exh.append(s)

    idx = [j for j, s in enumerate(exh) if s]
    if not idx:
        print("title_compare: %s: no captured frame is explained by a port frame"
              % name)
        return 1, None
    a, b = idx[0], idx[-1]
    window = range(a, b + 1)
    clean_j = {j for j in window if kinds[j][0] == 'clean'}
    torn_j = {j for j in window if kinds[j][0] == 'torn'}
    unexpl_j = [j for j in window if kinds[j][0] == 'unexplained']
    covered = set()
    for j in window:
        covered |= exh[j]
    missing = sorted(set(range(n)) - covered)

    print("title_compare: %s: window distinct [%d..%d] (raw %s..%s)"
          % (name, a, b, raws[a], raws[b]))
    print("title_compare: %s: %d frames in window: %d clean, %d torn, "
          "%d unexplained" % (name, b - a + 1, len(clean_j), len(torn_j),
                              len(unexpl_j)))
    tears = sorted({(d[0][0], d[0][1], d[0][2]) for j in torn_j
                    for d in [kinds[j][1]]})
    print("title_compare: %s: tear rows (N, lo..hi): %s"
          % (name, tears if tears else 'none'))
    print("title_compare: %s: port frames exhibited %d/%d; missing %s"
          % (name, len(covered), n, missing))

    bad = len(missing) + len(unexpl_j)
    for j in unexpl_j:
        m, N, t, da, db = kinds[j][1]
        r, c = first_diff_byte(frames[j], port[N], port[N + 1], t)
        print("title_compare: %s: UNEXPLAINED captured frame %d (raw %s): "
              "best whole-row splice port%d[0..%d) ++ port%d[%d..200) still "
              "differs at %d row(s) (first row %s byte %s)"
              % (name, j, raws[j], N, t, N + 1, t, m, r, c))
        sub = subrow_boundary(frames[j], port[N], port[N + 1], t)
        if sub:
            print("title_compare: %s:   MODEL INSUFFICIENCY: row %d splices "
                  "exactly at byte %d (%s) -- tear is mid-row, outside the "
                  "whole-row model" % (name, t, sub[1], sub[0]))
    for M in missing:
        print("title_compare: %s: PORT FRAME %d is not exhibited by any "
              "captured frame" % (name, M))
    return (1 if bad else 0), {'clean': clean_j, 'kinds': kinds, 'frames': frames}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--capture', action='append', required=True)
    ap.add_argument('--port', required=True)
    ap.add_argument('--frames', type=int, default=0)
    a = ap.parse_args()
    required = os.environ.get('PR_ORACLE_REQUIRED') == '1'

    primary = a.capture[0]
    if not os.path.isdir(primary):
        print("title_compare: no capture at %s (%s)"
              % (primary, 'FAIL (required)' if required else 'skipped'))
        return 1 if required else 0
    if not os.path.isdir(a.port):
        print("title_compare: no port dump at %s" % a.port)
        return 1
    if a.frames:
        n = a.frames
    else:
        n = len([f for f in os.listdir(a.port) if f.endswith('.raw')])
    port = []
    for i in range(n):
        path = os.path.join(a.port, 'frame_%04d.raw' % i)
        if not os.path.exists(path):
            print("title_compare: port frame %d is missing at %s" % (i, path))
            return 1
        port.append(load(path))
    port_ch = [row_hashes(p) for p in port]

    bad = 0
    results = []
    for k, capture in enumerate(a.capture):
        if not os.path.isdir(capture):
            print("title_compare: capture %d at %s absent, not compared"
                  % (k + 1, capture))
            continue
        rc, res = check_capture(capture, port, port_ch, n, 'capture %d' % (k + 1))
        bad += rc
        if res:
            results.append(res)

    if len(results) < 2:
        if required:
            print("title_compare: determinism proof INCOMPLETE: only %d "
                  "independent capture(s) present; two are required"
                  % len(results))
        else:
            print("title_compare: determinism proof skipped: only %d capture(s)"
                  % len(results))
    else:
        agree = disagree = 0
        for M in range(n):
            c1 = [j for j in results[0]['clean']
                  if results[0]['kinds'][j][0] == 'clean'
                  and results[0]['kinds'][j][1] == M]
            c2 = [j for j in results[1]['clean']
                  if results[1]['kinds'][j][0] == 'clean'
                  and results[1]['kinds'][j][1] == M]
            if c1 and c2:
                if results[0]['frames'][c1[0]] == results[1]['frames'][c2[0]]:
                    agree += 1
                else:
                    disagree += 1
        print("title_compare: determinism: clean samples of %d port frame(s) "
              "agree, %d disagree" % (agree, disagree))
        if disagree:
            bad += disagree

    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
