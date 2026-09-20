#!/usr/bin/env python3
"""Attract-prefix pixel oracle: the post-logo attract frames vs the port run.

The capture (data/title-captures/title) is the pinned original's whole post-logo
run. window.txt records `# mode=post-logo`; `twi5_last`/`twg_last` are the two
boot logos' last-presented raw indices, so the emitted frame_0000.raw is the
first game frame after the second logo (title_capture.py collapses consecutive
identical DOSBox-X frames to one frame per distinct game frame). The same
capture's later frames are the title window the Task 10 oracle compares.

The port dump is one RGB24 frame per presented state-0 frame from the
continuous PR_ATTRACT_DUMP run (`<port>/attract/frame_%04d.raw`), plus the
`<port>/title/` frames from the same run's post-attract title window.

The attract window is derived, not hardcoded: the first capture frame exhibited
by the port's title frames begins the title window, so every capture frame
before it is the attract prefix. With the existing title alignment that is
capture frames 0..215.

Because the capture is collapsed and the port dumps every presented tick, the
port run is denser than the capture. The oracle therefore requires every capture
frame in the attract window to be explained by some port frame (clean, splice or
transition) and reports the FIRST capture frame it cannot explain; unwitnessed
port frames are a statistic, not a failure. With `--expect-first N` the oracle
becomes a gate: the first divergence must be exactly capture frame N (the known
boundary of the matched prefix), so any regression inside frames 0..N-1 fails.
The comparison reuses tools/title_compare.py's helpers by import (that file is
read-only).

Stdlib only. Read-only; writes nothing.
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import title_compare as tc


def exhibited(kind, data):
    """The port frame indices a capture explanation exhibits (empty if none)."""
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
        for (N, r, only_n, only_n1) in data:
            s.add(N)
            s.add(N + 1)
    return s


def title_window_start(frames, port_title, port_title_rows):
    """First capture index exhibited by the port's title frames, or None."""
    n = len(port_title)
    for j in range(len(frames)):
        f = frames[j]
        kind, data = tc.explain(f, tc.row_hashes(f), port_title, port_title_rows, n)
        if exhibited(kind, data):
            return j
    return None


def check_capture(capture, port_attract, port_attract_rows,
                  port_title, port_title_rows, expect_first=None):
    frames = tc.load_frames(capture, os.path.basename(capture))
    if frames is None:
        return 1, None
    if not frames:
        print("attract_compare: %s is empty" % capture)
        return 1, None
    raws = tc.raw_map(capture) or list(range(len(frames)))

    ts = title_window_start(frames, port_title, port_title_rows)
    if ts is None:
        print("attract_compare: %s: no capture frame exhibits the port's title "
              "frames, so the attract boundary cannot be derived" % capture)
        return 1, None
    if ts == 0:
        print("attract_compare: %s: the title window starts at capture 0; there "
              "is no attract prefix to compare" % capture)
        return 1, None

    first = None
    explained = 0
    port_exh = set()
    for j in range(ts):
        f = frames[j]
        kind, data = tc.explain(f, tc.row_hashes(f), port_attract,
                                port_attract_rows, len(port_attract))
        if kind == 'unexplained':
            first = j
            break
        explained += 1
        port_exh |= exhibited(kind, data)

    print("attract_compare: %s: attract window [0..%d]; title window starts at "
          "capture %d (raw %d)" % (capture, ts - 1, ts, raws[ts]))
    print("attract_compare: %s: %d/%d capture frames explained; port attract "
          "frames exhibited %d/%d"
          % (capture, explained, ts, len(port_exh), len(port_attract)))
    if first is not None:
        f = frames[first]
        print("attract_compare: %s: FIRST DIVERGENCE at capture frame %d "
              "(raw %d)" % (capture, first, raws[first]))
        pref_r, suff_r = tc.explain(f, tc.row_hashes(f), port_attract,
                                    port_attract_rows, len(port_attract))[1]
        n = len(port_attract)
        m, N, b = tc.best_splice(f, port_attract, pref_r, suff_r, n)
        r, i = tc.first_diff_row_byte(f, port_attract[N], port_attract[N + 1], b)
        print("attract_compare: %s:   best byte splice port%d[0..%d) ++ "
              "port%d[%d..%d) still differs at %d byte(s) (first row %s byte %s)"
              % (capture, N, b, N + 1, b, tc.FRAME_BYTES, m, r, i))
        if expect_first is None:
            return 1, (first, raws[first])
        if first == expect_first:
            print("attract_compare: %s: expected divergence at capture frame %d"
                  % (capture, expect_first))
            return 0, (first, raws[first])
        print("attract_compare: %s: EXPECTED first divergence at capture frame "
              "%d, got %d" % (capture, expect_first, first))
        return 1, (first, raws[first])
    if expect_first is not None:
        print("attract_compare: %s: EXPECTED first divergence at capture frame "
              "%d, but the whole attract prefix was explained"
              % (capture, expect_first))
        return 1, (None, None)
    return 0, (None, None)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--capture', action='append', required=True)
    ap.add_argument('--port', required=True,
                    help='PR_ATTRACT_DUMP root; attract/ and title/ live here')
    ap.add_argument('--expect-first', type=int, default=None,
                    help='capture frame index the FIRST divergence must land '
                         'on; turns the report into a gate (0 = pass)')
    ap.add_argument('--verbose', action='store_true')
    a = ap.parse_args()
    required = os.environ.get('PR_ORACLE_REQUIRED') == '1'

    primary = a.capture[0]
    if not os.path.isdir(primary):
        print("attract_compare: no capture at %s (%s)"
              % (primary, 'FAIL (required)' if required else 'skipped'))
        return 1 if required else 0
    attract_dir = os.path.join(a.port, 'attract')
    title_dir = os.path.join(a.port, 'title')
    if not os.path.isdir(attract_dir):
        print("attract_compare: no attract dump at %s" % attract_dir)
        return 1
    if not os.path.isdir(title_dir):
        print("attract_compare: no title dump at %s (needed to derive the "
              "attract boundary)" % title_dir)
        return 1

    port_attract = tc.load_frames(attract_dir, 'port attract')
    port_title = tc.load_frames(title_dir, 'port title')
    if port_attract is None or port_title is None:
        return 1
    if not port_attract or not port_title:
        print("attract_compare: port dumps are empty")
        return 1
    port_attract_rows = [tc.row_hashes(p) for p in port_attract]
    port_title_rows = [tc.row_hashes(p) for p in port_title]

    bad = 0
    for k, capture in enumerate(a.capture):
        if not os.path.isdir(capture):
            print("attract_compare: capture %d at %s absent, not compared"
                  % (k + 1, capture))
            continue
        rc, _ = check_capture(capture, port_attract, port_attract_rows,
                              port_title, port_title_rows, a.expect_first)
        bad += rc
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
