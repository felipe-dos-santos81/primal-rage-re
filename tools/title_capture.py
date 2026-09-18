#!/usr/bin/env python3
"""Capture the pinned original's post-logo run as 320x200 RGB24 frames.

Runs the Task 1 pinned copy in DOSBox-X, records the whole boot (two Smacker
logos, then FUN_00011000's boot sub-machine, then the title), collapses
consecutive identical capture frames to one frame per distinct game frame, and
writes every distinct frame from just after the second logo to the end of the
run as frame_%04d.raw (192000 bytes each). window.txt records, per written
frame, the raw capture index it came from.

The game's logic runs at 60 Hz while mode 13h is captured at ~70.09 Hz, so a raw
capture index is not a game-frame index; the collapse is what makes a
frame-for-frame comparison possible.

This task does not locate the title window. The title begins at the first
DS_000F0A66 == 0x600 frame, which only the port's own frame_0000.raw identifies,
so Task 10 locates it by content-aligning its PR_TITLE_DUMP against this
capture. --port-anchor DIR performs that alignment here and emits one capture
frame per port frame; with no --port-anchor the whole post-logo region is
emitted so the alignment has something to find.

--verify-reproducible is a diagnostic, not a gate: it captures twice, reports
both frame counts, how many distinct frames are shared, and the raw index of the
first divergence, and exits 0 either way. A divergence inside the title composite
is a finding for Task 1.

Reuses smk_capture.run_dosbox/read_avi_frames/align (2b settled the invocation).
Usage: title_capture.py --out data/title-captures/title [--port-anchor DIR]
                        [--verify-reproducible]"""
import argparse
import hashlib
import os
import shutil
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import smk_capture as sc

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_GAME_DIR = os.path.join(REPO_ROOT, 'data', 'game', 'C')
DEFAULT_CD = os.path.join(REPO_ROOT, 'data', 'game', 'CD', 'RAGECD.ISO')

# Task 1's pinned output; the controller fixed this staging root. data/game/C's
# 44 MB is symlinked, never copied, so run_dosbox's dirname(game_dir)/CD
# derivation lands on the real CD image without modifying smk_capture.py.
PIN_DIR = '/tmp/pr_title_pin'
PIN_EXE = os.path.join(PIN_DIR, 'PRAGE.EXE')
FRAME_BYTES = sc.FRAME_W * sc.FRAME_H * 3


def stage(root, exe_src, game_dir):
    """root/C = symlinks to game_dir plus a copy of exe_src; root/CD = ISO link."""
    cdir = os.path.join(root, 'C')
    os.makedirs(cdir, exist_ok=True)
    for name in os.listdir(game_dir):
        if name.upper() == 'PRAGE.EXE':
            continue
        dst = os.path.join(cdir, name)
        if not os.path.lexists(dst):
            os.symlink(os.path.abspath(os.path.join(game_dir, name)), dst)
    shutil.copyfile(exe_src, os.path.join(cdir, 'PRAGE.EXE'))
    cd = os.path.join(root, 'CD')
    os.makedirs(cd, exist_ok=True)
    iso = os.path.join(cd, 'RAGECD.ISO')
    if not os.path.lexists(iso):
        os.symlink(os.path.abspath(DEFAULT_CD), iso)
    return cdir


def mkrun(root, time_limit, game_dir):
    """Run DOSBox-X once; return the AVI paths and the measured host fps."""
    avi = os.path.join(root, 'avi')
    os.makedirs(avi, exist_ok=True)
    game = stage(root, PIN_EXE, game_dir)
    names = sc.run_dosbox(game, avi, time_limit)
    paths = [os.path.join(avi, n) for n in names]
    fps = sc.ffprobe_fps(paths[0]) if paths else 0.0
    print('title_capture: %d AVI(s), %.4f fps' % (len(paths), fps))
    return paths, fps


def raw_hashes(paths):
    return [hashlib.md5(d).digest() for d in sc.read_avi_frames(paths)]


def collapse_from(cap, start):
    """Distinct consecutive frames from raw `start`: (md5 list, raw index list)."""
    hs, idx, prev = [], [], None
    for i in range(start, len(cap)):
        h = cap[i]
        if h != prev:
            hs.append(h)
            idx.append(i)
            prev = h
    return hs, idx


def frames_at(paths, indices):
    want = set(indices)
    pos = {ix: k for k, ix in enumerate(indices)}
    out = [None] * len(indices)
    for i, data in enumerate(sc.read_avi_frames(paths)):
        if i in want:
            out[pos[i]] = data
    return out


def logos_last_presented(cap, game_dir):
    """twi5's and twg's last-presented raw capture indices."""
    out = {}
    for name in sc.MOVIES:
        movie = None
        for f in os.listdir(game_dir):
            if f.upper() == sc.MOVIES[name]:
                movie = os.path.join(game_dir, f)
        if movie is None:
            out[name] = None
            continue
        refs = [hashlib.md5(d).digest() for d in sc.reference_frames(movie, None)]
        matched = [j for j in sc.align(refs, cap) if j is not None]
        out[name] = matched[-1] if matched else None
    return out


def post_logo_start(cap, game_dir):
    """First raw index after the second logo's last held frame."""
    logs = logos_last_presented(cap, game_dir)
    twg = logs.get('twg')
    if twg is None:
        sys.exit('title_capture: could not align twg against the capture')
    end = twg
    while end < len(cap) and cap[end] == cap[twg]:
        end += 1
    return end, logs


def window_from_port(cap, port_dir):
    """Align the port's frames to capture indices: one output frame per port."""
    port = []
    i = 0
    while True:
        path = os.path.join(port_dir, 'frame_%04d.raw' % i)
        if not os.path.exists(path):
            break
        data = open(path, 'rb').read()
        if len(data) != FRAME_BYTES:
            sys.exit('title_capture: %s is %d bytes, expected %d'
                     % (path, len(data), FRAME_BYTES))
        port.append(data)
        i += 1
    if not port:
        sys.exit('title_capture: no frame_%04d.raw in %s' % (i, port_dir))
    ph = [hashlib.md5(d).digest() for d in port]
    mapping = sc.align(ph, cap)
    missing = [i for i, j in enumerate(mapping) if j is None]
    if missing:
        sys.exit('title_capture: port frame %d not found in the capture; the '
                 'capture is too short or the port dump is off' % missing[0])
    return mapping


def write_window(out_dir, raw_indices, frames, header):
    os.makedirs(out_dir, exist_ok=True)
    for old in os.listdir(out_dir):
        if old.endswith('.raw'):
            os.remove(os.path.join(out_dir, old))
    for i, data in enumerate(frames):
        if data is None:
            sys.exit('title_capture: internal error: missing frame %d' % i)
        with open(os.path.join(out_dir, 'frame_%04d.raw' % i), 'wb') as f:
            f.write(data)
    with open(os.path.join(out_dir, 'window.txt'), 'w') as f:
        for line in header:
            f.write('# %s\n' % line)
        for i, raw in enumerate(raw_indices):
            f.write('%04d %d\n' % (i, raw))


def longest_common_run(a, b):
    prev = [0] * (len(b) + 1)
    best = (0, 0, 0)
    for i in range(1, len(a) + 1):
        cur = [0] * (len(b) + 1)
        for j in range(1, len(b) + 1):
            if a[i - 1] == b[j - 1]:
                cur[j] = prev[j - 1] + 1
                if cur[j] > best[0]:
                    best = (cur[j], i - cur[j], j - cur[j])
        prev = cur
    return best


def diagnose(seqs, logs, starts):
    """Report the two runs' counts, shared frames, and first divergence."""
    (h1, i1), (h2, i2) = seqs
    print('title_capture: diagnostic (not a gate)')
    print('title_capture: run1 %d distinct frames, run2 %d distinct frames'
          % (len(h1), len(h2)))
    shared = len(set(h1) & set(h2))
    print('title_capture: shared distinct frames: %d (%d vs %d)'
          % (shared, len(h1), len(h2)))
    n = min(len(h1), len(h2))
    first = None
    for k in range(n):
        if h1[k] != h2[k]:
            first = k
            break
    if first is None and len(h1) == len(h2):
        print('title_capture: distinct sequences identical over %d frames' % n)
    else:
        k = first if first is not None else n
        r1 = i1[k] if k < len(i1) else None
        r2 = i2[k] if k < len(i2) else None
        print('title_capture: first divergence at distinct index %d '
              '(run1 raw %s, run2 raw %s)' % (k, r1, r2))
        twg = logs.get('twg')
        for r in (r1, r2):
            if r is None:
                continue
            region = 'boot logos' if (twg is not None and r <= twg) else \
                     'post-logo run (boot sub-machine + title; Task 10 pins the boundary)'
            print('title_capture:   raw %d is in the %s' % (r, region))
    if h1 and h2:
        L, a0, b0 = longest_common_run(h1, h2)
        print('title_capture: longest common distinct run: %d frames '
              '(run1 raw %d..%d, run2 raw %d..%d)'
              % (L, i1[a0], i1[a0 + L - 1], i2[b0], i2[b0 + L - 1]))


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--out', required=True,
                    help='output directory for frame_%04d.raw and window.txt')
    ap.add_argument('--game-dir', default=DEFAULT_GAME_DIR)
    ap.add_argument('--time-limit', type=int, default=45,
                    help='DOSBox-X -time-limit seconds (default 45)')
    ap.add_argument('--anchor', type=int,
                    help='manual raw capture index to start the emitted region '
                         '(default: the frame after the second logo)')
    ap.add_argument('--port-anchor',
                    help="directory of the port's frame_%04d.raw dump; one "
                         'capture frame is emitted per port frame (Task 10)')
    ap.add_argument('--verify-reproducible', action='store_true',
                    help='capture twice and report the divergence picture '
                         '(diagnostic, not a gate)')
    ap.add_argument('--keep-avi', action='store_true')
    a = ap.parse_args()

    if not os.path.isfile(PIN_EXE):
        sys.exit('title_capture: pinned copy missing at %s (run make title-pin)'
                 % PIN_EXE)

    def run(fn):
        root = tempfile.mkdtemp(prefix='titlecap-')
        try:
            paths, fps = mkrun(root, a.time_limit, a.game_dir)
            return fn(paths, fps)
        finally:
            if not a.keep_avi:
                shutil.rmtree(root, ignore_errors=True)

    def emit_or_seq(dest, paths, emit):
        cap = raw_hashes(paths)
        start, logs = post_logo_start(cap, a.game_dir)
        print('title_capture: logo last-presented raw indices: %s' % logs)
        if a.port_anchor:
            mapping = window_from_port(cap, a.port_anchor)
            idx = list(mapping)
            if emit and dest:
                frames = [None] * len(idx)
                pos = {ix: k for k, ix in enumerate(idx)}
                for i, data in enumerate(sc.read_avi_frames(paths)):
                    if i in pos:
                        frames[pos[i]] = data
                write_window(dest, idx, frames,
                             ['mode=port-anchor',
                              'raw_window=%d..%d' % (min(idx), max(idx)),
                              'twi5_last=%s twg_last=%s'
                              % (logs.get('twi5'), logs.get('twg'))])
            print('title_capture: port-aligned %d frames, raw %d..%d'
                  % (len(idx), min(idx), max(idx)))
            return idx, None, logs
        if a.anchor is not None:
            start = a.anchor
        print('title_capture: emitting post-logo run from raw %d to %d'
              % (start, len(cap) - 1))
        hs, idx = collapse_from(cap, start)
        if emit and dest:
            frames = frames_at(paths, idx)
            write_window(dest, idx, frames,
                         ['mode=%s' % ('anchor' if a.anchor is not None
                                       else 'post-logo'),
                          'raw_window=%d..%d' % (idx[0], idx[-1]),
                          'twi5_last=%s twg_last=%s'
                          % (logs.get('twi5'), logs.get('twg'))])
        return idx, hs, logs

    if a.verify_reproducible:
        r1 = run(lambda p, f: emit_or_seq(None, p, False))
        r2 = run(lambda p, f: emit_or_seq(None, p, False))
        diagnose(((r1[1], r1[0]), (r2[1], r2[0])), r1[2], (r1[0], r2[0]))
        return 0

    idx, hs, logs = run(lambda p, f: emit_or_seq(a.out, p, True))
    if not a.port_anchor:
        print('title_capture: wrote %d frames to %s (raw %d..%d)'
              % (len(idx), a.out, idx[0], idx[-1]))
    else:
        print('title_capture: wrote %d frames to %s' % (len(idx), a.out))
    return 0


if __name__ == '__main__':
    sys.exit(main())
