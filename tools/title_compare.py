#!/usr/bin/env python3
"""Byte-splice + transition-row title oracle. Zero pixel tolerance.

Absent capture: skip (exit 0) unless PR_ORACLE_REQUIRED=1, then fail.
Usage: title_compare.py --capture DIR [--capture DIR2] --port DIR --frames 96

An approved model (Task 10 errata, commit 6bee3c5): a captured frame is valid iff

  A. it is exactly `port[N][0..b) ++ port[N+1][b..192000)` for adjacent port
     frames N, N+1 and a derived splice byte b (b=0/192000 = clean); or
  B. it is byte-exact everywhere outside exactly one transition row r, and every
     byte of row r equals the byte at the same offset in port[N] or port[N+1]
     for the adjacent pair the rest of the frame identifies. No third source.

Coverage: every port frame 1..94 must be exhibited by both captures; at most one
endpoint (0 or 95) may be unexhibited, only when its neighbour (1 / 94) is
exhibited exactly. Clean (b=0) samples of a port frame must be byte-identical
across the two captures. The splice byte and transition row are derived from the
data, never arguments; no threshold, mask, crop, frame-skip or per-frame
allowance. If clause B is still insufficient (two transition rows, or a byte from
a third frame), the frame is reported and the oracle fails; the model is not
extended.
"""
import argparse
import hashlib
import os
import sys

import numpy as np

FRAME_W, FRAME_H = 320, 200
ROW = FRAME_W * 3
FRAME_BYTES = FRAME_H * ROW


def load(path):
    with open(path, 'rb') as f:
        return f.read()


def to_array(data):
    return np.frombuffer(data, dtype=np.uint8)


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
    return [hashlib.md5(data[r * ROW:(r + 1) * ROW]).digest()
            for r in range(FRAME_H)]


def byte_prefix(c, p):
    d = np.flatnonzero(c != p)
    return int(d[0]) if d.size else FRAME_BYTES


def byte_suffix(c, p):
    d = np.flatnonzero(c[::-1] != p[::-1])
    return int(d[0]) if d.size else FRAME_BYTES


def row_common(ch, ph):
    k = 0
    while k < FRAME_H and ch[k] == ph[k]:
        k += 1
    p = k
    k = 0
    while k < FRAME_H and ch[FRAME_H - 1 - k] == ph[FRAME_H - 1 - k]:
        k += 1
    return p, k


def explain(c_arr, ch, port_arr, port_rows, n):
    """(kind, data):
    clean       data = M
    splice      data = [(N, lo, hi)]
    transition  data = [(N, r, only_N, only_N1)]
    unexplained data = (mismatches, N, b)"""
    pref_r, suff_r = [0] * n, [0] * n
    for M in range(n):
        pref_r[M], suff_r[M] = row_common(ch, port_rows[M])
    for M in range(n):
        if pref_r[M] == FRAME_H and c_arr.tobytes() == port_arr[M].tobytes():
            return 'clean', M
    splices = []
    for N in range(n - 1):
        if pref_r[N] + suff_r[N + 1] < FRAME_H:
            continue
        pb = byte_prefix(c_arr, port_arr[N])
        sb = byte_suffix(c_arr, port_arr[N + 1])
        if pb + sb >= FRAME_BYTES:
            splices.append((N, FRAME_BYTES - sb, pb))
    if splices:
        return 'splice', splices
    trans = []
    for N in range(n - 1):
        if pref_r[N] + suff_r[N + 1] != FRAME_H - 1:
            continue
        r = pref_r[N]
        a = c_arr[r * ROW:(r + 1) * ROW]
        pa = port_arr[N][r * ROW:(r + 1) * ROW]
        pb = port_arr[N + 1][r * ROW:(r + 1) * ROW]
        if not bool(np.all((a == pa) | (a == pb))):
            continue
        if c_arr[:r * ROW].tobytes() != port_arr[N][:r * ROW].tobytes():
            continue
        if c_arr[(r + 1) * ROW:].tobytes() != \
           port_arr[N + 1][(r + 1) * ROW:].tobytes():
            continue
        only_n = int(((a == pa) & (a != pb)).sum())
        only_n1 = int(((a == pb) & (a != pa)).sum())
        trans.append((N, r, only_n, only_n1))
    if trans:
        return 'transition', trans
    best = None
    for N in range(n - 1):
        da = (c_arr != port_arr[N]).astype(np.int64)
        db = (c_arr != port_arr[N + 1]).astype(np.int64)
        pa = np.concatenate([[0], np.cumsum(da)])
        pb = np.concatenate([[0], np.cumsum(db)])
        vals = pa - pb + pb[-1]
        b = int(np.argmin(vals))
        m = int(vals[b])
        if best is None or m < best[0]:
            best = (m, N, b)
    return 'unexplained', best


def first_diff_row_byte(c_arr, a, b, sp):
    ref = np.concatenate([a[:sp], b[sp:]])
    d = np.flatnonzero(c_arr != ref)
    if d.size == 0:
        return None, None
    i = int(d[0])
    return i // ROW, i


def bands(c_arr, a, b):
    ma = c_arr == a
    mb = c_arr == b
    only_a = np.flatnonzero(ma & ~mb)
    only_b = np.flatnonzero(mb & ~ma)
    return ((int(only_a[0]), int(only_a[-1]), int(only_a.size))
            if only_a.size else None,
            (int(only_b[0]), int(only_b[-1]), int(only_b.size))
            if only_b.size else None)


def check_capture(capture, port_arr, port_rows, n, name, verbose):
    frames = load_frames(capture)
    if not frames:
        print("title_compare: %s is empty" % name)
        return 1, None
    if len(frames[0]) != FRAME_BYTES:
        print("title_compare: %s frame 0 is %d bytes, expected %d"
              % (name, len(frames[0]), FRAME_BYTES))
        return 1, None
    raws = raw_map(capture) or list(range(len(frames)))
    arrs = [to_array(f) for f in frames]
    kinds = [explain(arrs[j], row_hashes(frames[j]), port_arr, port_rows, n)
             for j in range(len(frames))]

    exh = []
    for kind, data in kinds:
        s = set()
        if kind == 'clean':
            s.add(data)
        elif kind == 'splice':
            for (N, lo, hi) in data:
                if hi > 0:
                    s.add(N)
                if lo < FRAME_BYTES:
                    s.add(N + 1)
        elif kind == 'transition':
            for (N, r, a, b) in data:
                s.add(N)
                s.add(N + 1)
        exh.append(s)

    idx = [j for j, s in enumerate(exh) if s]
    a, b = idx[0], idx[-1]
    window = range(a, b + 1)
    clean_j = {j for j in window if kinds[j][0] == 'clean'}
    splice_j = {j for j in window if kinds[j][0] == 'splice'}
    trans_j = {j for j in window if kinds[j][0] == 'transition'}
    unexpl_j = [j for j in window if kinds[j][0] == 'unexplained']
    covered = set()
    for j in window:
        covered |= exh[j]
    missing = sorted(set(range(n)) - covered)
    allowed = {0, n - 1}
    bad_missing = [M for M in missing if M not in allowed]
    endpoint_ok = True
    for M in (0, n - 1):
        if M in missing and (1 if M == 0 else n - 2) not in covered:
            endpoint_ok = False

    print("title_compare: %s: window distinct [%d..%d] (raw %s..%s)"
          % (name, a, b, raws[a], raws[b]))
    print("title_compare: %s: %d frames in window: %d clean, %d splice, "
          "%d transition, %d unexplained"
          % (name, b - a + 1, len(clean_j), len(splice_j), len(trans_j),
             len(unexpl_j)))
    spl = sorted({d[0][0]: d[0] for j in splice_j for d in [kinds[j][1]]}.values(),
                 key=lambda x: x[1])
    print("title_compare: %s: splice bytes (N, b) [%d frames]: %s"
          % (name, len(splice_j),
             ['port%d@%d' % (N, lo) for (N, lo, hi) in spl] if verbose else
             sorted({lo for (N, lo, hi) in spl})))
    trs = sorted({d[0]: d[0] for j in trans_j for d in [kinds[j][1]]}.values())
    print("title_compare: %s: transition rows (N, r, from_N, from_N+1) "
          "[%d frames]: %s"
          % (name, len(trans_j),
             ['port%d@row%d(%d/%d)' % t for t in trs] if trs else 'none'))
    print("title_compare: %s: port frames exhibited %d/%d; missing %s; "
          "endpoints %s" % (name, len(covered), n, missing,
                            'OK' if endpoint_ok else 'BAD'))

    bad = len(unexpl_j) + len(bad_missing) + (0 if endpoint_ok else 1)
    for j in unexpl_j:
        m, N, sp = kinds[j][1]
        r, i = first_diff_row_byte(arrs[j], port_arr[N], port_arr[N + 1], sp)
        print("title_compare: %s: UNEXPLAINED captured frame %d (raw %s): best "
              "byte splice port%d[0..%d) ++ port%d[%d..%d) still differs at %d "
              "byte(s) (first row %s byte %s)"
              % (name, j, raws[j], N, sp, N + 1, sp, FRAME_BYTES, m, r, i))
        oa, ob = bands(arrs[j], port_arr[N], port_arr[N + 1])
        print("title_compare: %s:   bytes only port%d: %s; only port%d: %s"
              % (name, N, oa, N + 1, ob))
    for M in bad_missing:
        print("title_compare: %s: PORT FRAME %d is not exhibited by any "
              "captured frame" % (name, M))
    return (1 if bad else 0), {'clean': clean_j, 'kinds': kinds, 'frames': frames}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--capture', action='append', required=True)
    ap.add_argument('--port', required=True)
    ap.add_argument('--frames', type=int, default=0)
    ap.add_argument('--verbose', action='store_true')
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
    port_arr = [to_array(p) for p in port]
    port_rows = [row_hashes(p) for p in port]

    bad = 0
    results = []
    for k, capture in enumerate(a.capture):
        if not os.path.isdir(capture):
            print("title_compare: capture %d at %s absent, not compared"
                  % (k + 1, capture))
            continue
        rc, res = check_capture(capture, port_arr, port_rows, n,
                                'capture %d' % (k + 1), a.verbose)
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
