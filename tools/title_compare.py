#!/usr/bin/env python3
"""Pixel-exact comparison of the port's title frames against the capture(s).
Absent capture: skip (exit 0) unless PR_ORACLE_REQUIRED=1, then fail.
Usage: title_compare.py --capture DIR [--capture DIR2] --port DIR --frames 96

Each capture is a sequence of frame_%04d.raw (320x200 RGB24), one file per
distinct game frame, with window.txt mapping the emitted index to the raw DOSBox
capture index. The window is located by content-aligning the port's own frames
into the capture (monotonic, like title_capture.py's --port-anchor), so an
anchored capture and the raw post-logo capture both compare index-for-index.
Zero-byte tolerance: no threshold, mask or frame skip. A mismatch is reported
with the frame index, the raw capture index it came from, and the first differing
byte offset. With two captures, the port is compared against both and any
disagreement between the two captures inside the aligned window is a pin finding
with the raw indices of the first divergence (that is the determinism proof the
port-free byte-identity gate could not give)."""
import argparse
import hashlib
import os
import sys

FRAME_BYTES = 320 * 200 * 3


def load(path):
    with open(path, 'rb') as f:
        return f.read()


def raw_map(capture):
    """window.txt emitted-index -> raw capture index, or None if absent."""
    path = os.path.join(capture, 'window.txt')
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
    if not pairs:
        return None
    return [pairs[i] for i in range(len(pairs))]


def load_capture(capture):
    frames = []
    i = 0
    while True:
        path = os.path.join(capture, 'frame_%04d.raw' % i)
        if not os.path.exists(path):
            break
        frames.append(load(path))
        i += 1
    return frames, raw_map(capture)


def digest(data):
    return hashlib.md5(data).digest()


def align(port, frames):
    """Monotonic content match, exactly title_capture.py's --port-anchor rule:
    port[i] -> the first capture index >= the previous one whose bytes match, or
    None. A held game frame maps repeated port frames to the same capture index."""
    by = {}
    for j, f in enumerate(frames):
        by.setdefault(digest(f), []).append(j)
    out = []
    prev = 0
    for f in port:
        h = digest(f)
        j = None
        for k in by.get(h, ()):
            if k >= prev:
                j = k
                break
        out.append(j)
        if j is not None:
            prev = j
    return out


def first_diff(a, b):
    n = min(len(a), len(b))
    for i in range(n):
        if a[i] != b[i]:
            return i
    return None if len(a) == len(b) else n


def compare(port, capture, name, n):
    """Compare the port's n frames against capture's aligned window.
    Returns (bad, window, raws); window/raws are per window frame, or ([], [])
    when the capture cannot be aligned. A mismatch counts as one bad frame."""
    frames, raw = load_capture(capture)
    if not frames:
        print("title_compare: %s is empty" % name)
        return 1, [], []
    if len(frames[0]) != FRAME_BYTES:
        print("title_compare: %s frame 0 is %d bytes, expected %d"
              % (name, len(frames[0]), FRAME_BYTES))
        return 1, [], []
    mapping = align(port, frames)
    missing = [i for i, j in enumerate(mapping) if j is None]
    if missing:
        print("title_compare: %s: port frame %d not found in the capture "
              "(the capture is too short or the port diverged)"
              % (name, missing[0]))
        return 1, [], []
    window = [frames[j] for j in mapping]
    raws = [(raw[j] if raw is not None and j < len(raw) else j) for j in mapping]
    bad = 0
    for i in range(n):
        c, p = window[i], port[i]
        if c != p:
            d = first_diff(c, p)
            print("frame %d (raw %s): MISMATCH len %d/%d first diff at %s"
                  % (i, raws[i], len(c), len(p), d))
            bad += 1
    print("title_compare: %s: %d/%d frames match" % (name, n - bad, n))
    return bad, window, raws


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--capture', action='append', required=True,
                    help='capture directory; repeat for the second capture')
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

    bad = 0
    windows = []
    for k, capture in enumerate(a.capture):
        if not os.path.isdir(capture):
            print("title_compare: capture %d at %s absent, not compared"
                  % (k + 1, capture))
            continue
        b, window, raws = compare(port, capture, 'capture %d' % (k + 1), n)
        bad += b
        if window:
            windows.append((window, raws))

    if len(windows) < 2:
        if required:
            print("title_compare: determinism proof INCOMPLETE: only %d "
                  "independent capture(s) present; two are required"
                  % len(windows))
        else:
            print("title_compare: determinism proof skipped: only %d capture(s)"
                  % len(windows))
    else:
        w1, r1 = windows[0]
        w2, r2 = windows[1]
        divergence = None
        for i in range(n):
            if w1[i] != w2[i]:
                divergence = i
                break
        if divergence is None:
            print("title_compare: determinism: the two captures match each "
                  "other and the port over the %d-frame window" % n)
        else:
            print("title_compare: PIN FINDING: the two captures disagree at "
                  "window frame %d (raw %s vs raw %s), first byte %s"
                  % (divergence, r1[divergence], r2[divergence],
                     first_diff(w1[divergence], w2[divergence])))

    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
