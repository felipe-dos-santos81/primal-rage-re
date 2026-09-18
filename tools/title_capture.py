#!/usr/bin/env python3
"""Capture the pinned original's title window as 320x200 RGB24 frames.

Runs the Task 1 pinned copy in DOSBox-X, records the whole boot (two Smacker
logos, then FUN_00011000's boot sub-machine, then the title), and writes one
frame per **distinct game frame** over the title window.

The game's logic runs at 60 Hz while mode 13h is captured at ~70.09 Hz, so a raw
capture index is not a game-frame index: consecutive identical capture frames are
holds of one game frame and are collapsed. window.txt records, per written frame,
the raw capture index it came from.

The window start is pinned by content, never guessed. Without --port-anchor the
tool locates the RNG-sensitive block: a pinned run is diffed against an unpinned
one in *distinct-frame* space (raw-space diffing is dominated by sampling
jitter), and the longest run of pinned frames whose content is absent from the
unpinned run is the title window. With --port-anchor it content-matches the
port's Task 10 dump and emits one capture frame per port frame.

--verify-reproducible captures twice and requires the distinct-frame sequences
over the window to be byte-identical; a mismatch voids the pin.
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


def mkrun(root, exe_src, game_dir, time_limit):
    """Run DOSBox-X once; return the AVI paths and the measured host fps."""
    avi = os.path.join(root, 'avi')
    os.makedirs(avi, exist_ok=True)
    game = stage(root, exe_src, game_dir)
    names = sc.run_dosbox(game, avi, time_limit)
    paths = [os.path.join(avi, n) for n in names]
    fps = sc.ffprobe_fps(paths[0]) if paths else 0.0
    print('title_capture: %d AVI(s), %.4f fps' % (len(paths), fps))
    return paths, fps


def collapse(paths):
    """Distinct consecutive frames: (md5 list, raw index list)."""
    hs, idx, prev = [], [], None
    for i, data in enumerate(sc.read_avi_frames(paths)):
        h = hashlib.md5(data).digest()
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


def raw_hashes(paths):
    return [hashlib.md5(d).digest() for d in sc.read_avi_frames(paths)]


def logos_last_presented(paths, game_dir):
    """twi5's and twg's last-presented raw capture indices (anchor evidence)."""
    cap = raw_hashes(paths)
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


def contiguous(items):
    runs = []
    if not items:
        return runs
    s = prev = items[0]
    for x in items[1:]:
        if x == prev + 1:
            prev = x
        else:
            runs.append((s, prev))
            s = prev = x
    runs.append((s, prev))
    return runs


def locate(pinned_h, pinned_idx, unpinned_h, last_logo):
    """Longest run of pinned distinct frames absent from the unpinned run.

    The title state is the only RNG consumer whose whole window differs from an
    unpinned run, so after the logos its block is the longest such run; the
    scattered single hits from boot-timer jitter are shorter. Returns
    (candidates, chosen), each candidate (distinct_lo, distinct_hi, raw_lo,
    raw_hi)."""
    m = sc.align(pinned_h, unpinned_h)
    unmatched = [i for i, j in enumerate(m) if j is None]
    cands = []
    for a, b in contiguous(unmatched):
        if pinned_idx[b] <= last_logo:
            continue
        cands.append((a, b, pinned_idx[a], pinned_idx[b]))
    chosen = max(cands, key=lambda c: c[1] - c[0]) if cands else None
    return cands, chosen


def window_from_port(cap_paths, port_dir):
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
    cap = raw_hashes(cap_paths)
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


def reproducibility(a, b):
    """Equal up to leading/trailing extras, using smk_capture.align."""
    if a == b:
        return True, 'identical'
    for x, y, label in ((a, b, 'run1'), (b, a, 'run2')):
        m = sc.align(x, y)
        if all(j is not None for j in m):
            return True, ('%s contained in the other (%d vs %d frames)'
                          % (label, len(x), len(y)))
    for i in range(min(len(a), len(b))):
        if a[i] != b[i]:
            return False, ('first differing distinct frame %d (%d vs %d frames)'
                           % (i, len(a), len(b)))
    return False, ('%d vs %d frames' % (len(a), len(b)))


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--out', required=True,
                    help='output directory for frame_%04d.raw and window.txt')
    ap.add_argument('--game-dir', default=DEFAULT_GAME_DIR)
    ap.add_argument('--time-limit', type=int, default=45,
                    help='DOSBox-X -time-limit seconds (default 45)')
    ap.add_argument('--anchor', type=int,
                    help='manual raw capture index for frame 0 (overrides the '
                         'content locator; window is the block containing it)')
    ap.add_argument('--port-anchor',
                    help="directory of the port's frame_%04d.raw dump; one "
                         'capture frame is emitted per port frame')
    ap.add_argument('--verify-reproducible', action='store_true',
                    help='capture twice and require the distinct-frame sequences '
                         'over the window to be byte-identical (the pin gate)')
    ap.add_argument('--keep-avi', action='store_true')
    a = ap.parse_args()

    if not os.path.isfile(PIN_EXE):
        sys.exit('title_capture: pinned copy missing at %s (run make title-pin)'
                 % PIN_EXE)

    def pinned(fn):
        root = tempfile.mkdtemp(prefix='titlecap-')
        try:
            paths, fps = mkrun(root, PIN_EXE, a.game_dir, a.time_limit)
            return fn(paths, fps)
        finally:
            if not a.keep_avi:
                shutil.rmtree(root, ignore_errors=True)

    def unpinned_distinct():
        root = tempfile.mkdtemp(prefix='titleorig-')
        try:
            paths, _ = mkrun(root, os.path.join(a.game_dir, 'PRAGE.EXE'),
                             a.game_dir, a.time_limit)
            hs, _ = collapse(paths)
            print('title_capture: unpinned run %d distinct frames' % len(hs))
            return hs
        finally:
            shutil.rmtree(root, ignore_errors=True)

    def do(dest, paths, unpinned_h, emit):
        cap_h, cap_i = collapse(paths)
        print('title_capture: pinned run %d distinct frames' % len(cap_h))
        logs = logos_last_presented(paths, a.game_dir)
        print('title_capture: logo last-presented raw indices: %s' % logs)
        last_logo = max([v for v in logs.values() if v is not None] or [0])
        if a.port_anchor:
            mapping = window_from_port(paths, a.port_anchor)
            frames = [None] * len(mapping)
            pos = {ix: k for k, ix in enumerate(mapping)}
            for i, data in enumerate(sc.read_avi_frames(paths)):
                if i in pos:
                    frames[pos[i]] = data
            print('title_capture: port-aligned window raw %d..%d, %d frames'
                  % (min(mapping), max(mapping), len(mapping)))
            if emit and dest:
                write_window(dest, list(mapping), frames,
                             ['mode=port-anchor',
                              'raw_window=%d..%d' % (min(mapping), max(mapping)),
                              'twi5_last=%s twg_last=%s'
                              % (logs.get('twi5'), logs.get('twg'))])
            return frames
        cands, chosen = locate(cap_h, cap_i, unpinned_h, last_logo)
        for (a0, b0, r0, r1) in cands:
            print('title_capture: RNG-sensitive block distinct[%d..%d] raw %d..%d'
                  ' (%d frames)' % (a0, b0, r0, r1, b0 - a0 + 1))
        if a.anchor is not None:
            hit = [c for c in cands if c[2] <= a.anchor <= c[3]]
            if hit:
                a0, b0, r0, r1 = hit[0]
            elif chosen:
                a0, b0, r0, r1 = chosen
                print('title_capture: --anchor %d not in a block; using %d..%d'
                      % (a.anchor, r0, r1))
            else:
                print('title_capture: --anchor given but no block; nothing written')
                return None
        elif chosen:
            a0, b0, r0, r1 = chosen
        else:
            print('title_capture: no RNG-sensitive block found; nothing written')
            return None
        print('title_capture: window raw %d..%d, %d distinct frames'
              % (r0, r1, b0 - a0 + 1))
        frames = frames_at(paths, list(range(r0, r1 + 1)))
        if emit and dest:
            write_window(dest, list(range(r0, r1 + 1)), frames,
                         ['mode=%s' % ('anchor' if a.anchor is not None else 'content'),
                          'raw_window=%d..%d' % (r0, r1),
                          'twi5_last=%s twg_last=%s'
                          % (logs.get('twi5'), logs.get('twg'))])
        return frames

    if a.verify_reproducible:
        print('== title_capture: reproducibility gate (distinct-frame windows) ==')
        unpinned_h = unpinned_distinct()
        w1 = pinned(lambda p, f: do(None, p, unpinned_h, False))
        w2 = pinned(lambda p, f: do(None, p, unpinned_h, False))
        if w1 is None or w2 is None:
            sys.exit('title_capture: window not identified in a run; gate aborted')
        h1 = [hashlib.md5(x).digest() for x in w1]
        h2 = [hashlib.md5(x).digest() for x in w2]
        ok, why = reproducibility(h1, h2)
        print('title_capture: run1 %d distinct frames, run2 %d distinct frames'
              % (len(h1), len(h2)))
        if not ok:
            sys.exit('title_capture: NOT reproducible: %s' % why)
        print('title_capture: reproducible: %s' % why)
        return 0

    unpinned_h = unpinned_distinct()
    frames = pinned(lambda p, f: do(a.out, p, unpinned_h, True))
    if frames is None:
        return 1
    print('title_capture: wrote %d frames to %s' % (len(frames), a.out))
    return 0


if __name__ == '__main__':
    sys.exit(main())
