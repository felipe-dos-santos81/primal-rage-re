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
frame at each end of the window (0 and 95) may be unexhibited, and only when its
neighbour (1 / 94) is exhibited. Clean (b=0) samples of a port frame must be
byte-identical across the two captures. The splice byte and transition row are
derived from the data, never arguments; no threshold, mask, crop, frame-skip or
per-frame allowance. If clause B is still insufficient (two transition rows, or a
byte from a third frame), the frame is reported and the oracle fails; the model
is not extended.

Stdlib only (bytes/slices); no third-party imports.
"""
import argparse
import hashlib
import os
import sys

FRAME_W, FRAME_H = 320, 200
ROW = FRAME_W * 3
FRAME_BYTES = FRAME_H * ROW
CHUNK = 8192

# The determinism proof compares clean (b=0) samples that BOTH captures hold.
# That set must not be empty or the proof is vacuous. The measured runs share 36
# clean samples (53 clean each, 36 in common), but the shared count is set by the
# capture sampling phase, not by the port, so a static floor above 1 would
# false-negative a legitimate pair. The honest bound is at least one.
MIN_SHARED_CLEAN = 1


def load(path):
    with open(path, 'rb') as f:
        return f.read()


def load_frames(d, what):
    """Every frame must be exactly FRAME_BYTES; a short/long frame fails with
    its index and the two lengths rather than comparing a truncated window."""
    frames, i = [], 0
    while True:
        path = os.path.join(d, 'frame_%04d.raw' % i)
        if not os.path.exists(path):
            break
        data = load(path)
        if len(data) != FRAME_BYTES:
            print("title_compare: %s frame %d is %d bytes, expected %d"
                  % (what, i, len(data), FRAME_BYTES))
            return None
        frames.append(data)
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


def first_diff(a, b):
    """First byte index where a and b differ, or min(len) if equal."""
    n = min(len(a), len(b))
    i = 0
    while i < n:
        j = min(n, i + CHUNK)
        if a[i:j] != b[i:j]:
            k = i
            while a[k] == b[k]:
                k += 1
            return k
        i = j
    return n


def common_suffix(a, b):
    """Length of the common trailing run of a and b."""
    n = min(len(a), len(b))
    i = 0
    while i < n:
        j = min(n, i + CHUNK)
        if a[n - j:n - i] != b[n - j:n - i]:
            k = 0
            while a[n - 1 - k] == b[n - 1 - k]:
                k += 1
            return k
        i = j
    return n


def row_common(ch, ph):
    k = 0
    while k < FRAME_H and ch[k] == ph[k]:
        k += 1
    p = k
    k = 0
    while k < FRAME_H and ch[FRAME_H - 1 - k] == ph[FRAME_H - 1 - k]:
        k += 1
    return p, k


def best_splice(c, port, pref_r, suff_r, n):
    """Exact best (mismatch_bytes, N, b) over the closest candidate frames."""
    cands = [N for N in range(n - 1) if pref_r[N] + suff_r[N + 1] >= FRAME_H - 1]
    if not cands:
        cands = list(range(n - 1))
    best = None
    for N in cands:
        a, b = port[N], port[N + 1]
        db = sum(1 for x, y in zip(c, b) if x != y)
        cur = 0
        best_v, best_b = db, 0
        for i in range(FRAME_BYTES):
            ci = c[i]
            if ci != a[i]:
                cur += 1
            if ci != b[i]:
                cur -= 1
            v = cur + db
            if v < best_v:
                best_v, best_b = v, i + 1
        if best is None or best_v < best[0]:
            best = (best_v, N, best_b)
    return best


def explain(c, ch, port, port_rows, n):
    """(kind, data): clean=M; splice=[(N,lo,hi)]; transition=[(N,r,fromN,fromN1)];
    unexplained=(None; the best splice is computed lazily for the report)."""
    pref_r, suff_r = [0] * n, [0] * n
    for M, ph in enumerate(port_rows):
        pref_r[M], suff_r[M] = row_common(ch, ph)
    for M in range(n):
        if pref_r[M] == FRAME_H and c == port[M]:
            return 'clean', M
    splices = []
    for N in range(n - 1):
        if pref_r[N] + suff_r[N + 1] < FRAME_H:
            continue
        pb = first_diff(c, port[N])
        sb = common_suffix(c, port[N + 1])
        if pb + sb >= FRAME_BYTES:
            splices.append((N, FRAME_BYTES - sb, pb))
    if splices:
        return 'splice', splices
    for N in range(n - 1):
        if pref_r[N] + suff_r[N + 1] != FRAME_H - 1:
            continue
        r = pref_r[N]
        row = c[r * ROW:(r + 1) * ROW]
        ra = port[N][r * ROW:(r + 1) * ROW]
        rb = port[N + 1][r * ROW:(r + 1) * ROW]
        only_n = only_n1 = 0
        for x, y, z in zip(row, ra, rb):
            if x == y:
                if x != z:
                    only_n += 1
            elif x == z:
                only_n1 += 1
            else:
                break
        else:
            if c[:r * ROW] == port[N][:r * ROW] and \
               c[(r + 1) * ROW:] == port[N + 1][(r + 1) * ROW:]:
                return 'transition', [(N, r, only_n, only_n1)]
    return 'unexplained', (pref_r, suff_r)


def first_diff_row_byte(c, a, b, sp):
    ref = a[:sp] + b[sp:]
    d = first_diff(c, ref)
    if d >= FRAME_BYTES:
        return None, None
    return d // ROW, d


def bands(c, a, b):
    """Byte ranges/counts where c matches only a (old) and only b (new)."""
    only_a = [i for i in range(FRAME_BYTES) if c[i] == a[i] and c[i] != b[i]]
    only_b = [i for i in range(FRAME_BYTES) if c[i] == b[i] and c[i] != a[i]]
    return ((only_a[0], only_a[-1], len(only_a)) if only_a else None,
            (only_b[0], only_b[-1], len(only_b)) if only_b else None)


def check_capture(capture, port, port_rows, n, name, verbose):
    frames = load_frames(capture, name)
    if frames is None:
        return 1, None
    if not frames:
        print("title_compare: %s is empty" % name)
        return 1, None
    raws = raw_map(capture) or list(range(len(frames)))
    kinds = [explain(frames[j], row_hashes(frames[j]), port, port_rows, n)
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
            for (N, r, only_n, only_n1) in data:
                s.add(N)
                s.add(N + 1)
        exh.append(s)

    idx = [j for j, s in enumerate(exh) if s]
    a, b = idx[0], idx[-1]
    window = range(a, b + 1)
    clean_j = [j for j in window if kinds[j][0] == 'clean']
    splice_j = [j for j in window if kinds[j][0] == 'splice']
    trans_j = [j for j in window if kinds[j][0] == 'transition']
    unexpl_j = [j for j in window if kinds[j][0] == 'unexplained']
    covered = set()
    for j in window:
        covered |= exh[j]
    missing = sorted(set(range(n)) - covered)
    bad_missing = [M for M in missing if M not in (0, n - 1)]
    # Spec: at most one frame at each end of the window may be unexhibited, and
    # only when its neighbour (1 / 94) is exhibited.
    endpoint_ok = True
    for M in (0, n - 1):
        if M in missing and (1 if M == 0 else n - 2) not in covered:
            endpoint_ok = False

    splices = sorted({(N, lo) for j in splice_j for (N, lo, hi) in kinds[j][1]},
                     key=lambda x: (x[1], x[0]))
    trans = sorted({(N, r, on, on1) for j in trans_j
                    for (N, r, on, on1) in kinds[j][1]})
    print("title_compare: %s: window distinct [%d..%d] (raw %s..%s)"
          % (name, a, b, raws[a], raws[b]))
    print("title_compare: %s: %d frames in window: %d clean, %d splice, "
          "%d transition, %d unexplained"
          % (name, b - a + 1, len(clean_j), len(splice_j), len(trans_j),
             len(unexpl_j)))
    print("title_compare: %s: splice bytes [%d frames, %d distinct]: %s"
          % (name, len(splice_j), len(splices),
             ['port%d@%d' % (N, lo) for (N, lo) in splices] if verbose else
             [lo for (N, lo) in splices]))
    print("title_compare: %s: transition rows (N, row, from_N, from_N+1) "
          "[%d frames]: %s"
          % (name, len(trans_j),
             ['port%d@row%d(%d/%d)' % t for t in trans] if trans else 'none'))
    print("title_compare: %s: port frames exhibited %d/%d; missing %s; "
          "endpoints %s" % (name, len(covered), n, missing,
                            'OK' if endpoint_ok else 'BAD'))

    bad = len(unexpl_j) + len(bad_missing) + (0 if endpoint_ok else 1)
    for j in unexpl_j:
        pref_r, suff_r = kinds[j][1]
        m, N, b = best_splice(frames[j], port, pref_r, suff_r, n)
        r, i = first_diff_row_byte(frames[j], port[N], port[N + 1], b)
        print("title_compare: %s: UNEXPLAINED captured frame %d (raw %s): best "
              "byte splice port%d[0..%d) ++ port%d[%d..%d) still differs at %d "
              "byte(s) (first row %s byte %s)"
              % (name, j, raws[j], N, b, N + 1, b, FRAME_BYTES, m, r, i))
        oa, ob = bands(frames[j], port[N], port[N + 1])
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
        data = load(path)
        if len(data) != FRAME_BYTES:
            print("title_compare: port frame %d is %d bytes, expected %d"
                  % (i, len(data), FRAME_BYTES))
            return 1
        port.append(data)
    port_rows = [row_hashes(p) for p in port]

    bad = 0
    results = []
    for k, capture in enumerate(a.capture):
        if not os.path.isdir(capture):
            print("title_compare: capture %d at %s absent, not compared"
                  % (k + 1, capture))
            continue
        rc, res = check_capture(capture, port, port_rows, n,
                                'capture %d' % (k + 1), a.verbose)
        bad += rc
        if res:
            results.append(res)

    if len(results) < 2:
        if required:
            print("title_compare: determinism proof INCOMPLETE: only %d "
                  "independent capture(s); two are required with "
                  "PR_ORACLE_REQUIRED=1" % len(results))
            bad += 1
        else:
            print("title_compare: determinism proof skipped: only %d capture(s)"
                  % len(results))
    else:
        agree = disagree = 0
        for M in range(n):
            c1 = [j for j in results[0]['clean']
                  if results[0]['kinds'][j][1] == M]
            c2 = [j for j in results[1]['clean']
                  if results[1]['kinds'][j][1] == M]
            if c1 and c2:
                if results[0]['frames'][c1[0]] == results[1]['frames'][c2[0]]:
                    agree += 1
                else:
                    disagree += 1
        print("title_compare: determinism: clean samples of %d port frame(s) "
              "agree, %d disagree" % (agree, disagree))
        if disagree:
            bad += disagree
        if agree + disagree < MIN_SHARED_CLEAN:
            print("title_compare: determinism proof VACUOUS: no port frame has a "
                  "clean sample in both captures, so the clean-sample agreement "
                  "cannot be established (need >= %d)" % MIN_SHARED_CLEAN)
            bad += 1

    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
