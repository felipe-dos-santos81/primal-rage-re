#!/usr/bin/env python3
"""Capture the original's title screen, pinned, as 320x200 RGB24 frames.

Runs PRAGE.EXE in DOSBox-X from the /tmp pinned copy (Task 1), records the whole
run, and indexes the frames so frame_0000.raw is the first title frame
(DS_000F0A66 == 0x600 as the port dumps it). --verify-reproducible runs the
capture twice and requires byte-identical frames; a mismatch voids the pin.

Reuses smk_capture.read_avi_frames/align rather than re-implementing the
DOSBox-X invocation and AVI path (2b already settled both).
Usage: title_capture.py --out data/title-captures/title [--verify-reproducible]"""
import argparse
import hashlib
import os
import shutil
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import smk_capture as sc
import title_pin

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_GAME_DIR = os.path.join(REPO_ROOT, 'data', 'game', 'C')
DEFAULT_CD = os.path.join(REPO_ROOT, 'data', 'game', 'CD', 'RAGECD.ISO')

# Task 1's output and the staging root the controller fixed: the game drive is
# /tmp/pr_title_pin/C (data/game/C's 44 MB is never copied, only symlinked), so
# smk_capture.run_dosbox's dirname(game_dir)/CD/RAGECD.ISO derivation lands on
# the real CD image symlink without touching smk_capture.py.
PIN_DIR = '/tmp/pr_title_pin'
PIN_EXE = os.path.join(PIN_DIR, 'PRAGE.EXE')
FRAME_BYTES = sc.FRAME_W * sc.FRAME_H * 3


def stage(game_dir):
    """Build PIN_DIR/C (symlinks + the pinned EXE) and PIN_DIR/CD/RAGECD.ISO."""
    cdir = os.path.join(PIN_DIR, 'C')
    os.makedirs(cdir, exist_ok=True)
    for name in os.listdir(game_dir):
        if name.upper() == 'PRAGE.EXE':
            continue
        dst = os.path.join(cdir, name)
        if not os.path.lexists(dst):
            os.symlink(os.path.abspath(os.path.join(game_dir, name)), dst)
    shutil.copyfile(PIN_EXE, os.path.join(cdir, 'PRAGE.EXE'))
    cd = os.path.join(PIN_DIR, 'CD')
    os.makedirs(cd, exist_ok=True)
    iso = os.path.join(cd, 'RAGECD.ISO')
    if not os.path.lexists(iso):
        os.symlink(os.path.abspath(DEFAULT_CD), iso)
    return cdir


def read_port_frames(port_dir):
    """The port's Task 10 dump: frame_%04d.raw, 320x200x3, in index order."""
    out = []
    i = 0
    while True:
        path = os.path.join(port_dir, 'frame_%04d.raw' % i)
        if not os.path.exists(path):
            break
        data = open(path, 'rb').read()
        if len(data) != FRAME_BYTES:
            sys.exit('title_capture: %s is %d bytes, expected %d'
                     % (path, len(data), FRAME_BYTES))
        out.append(data)
        i += 1
    if not out:
        sys.exit('title_capture: no frame_%04d.raw in %s' % (i, port_dir))
    return out


def cap_hashes(paths):
    return [hashlib.md5(d).digest() for d in sc.read_avi_frames(paths)]


def auto_anchor(paths, cap, game_dir):
    """First frame after the second boot logo's last presented frame.

    smk_capture.align maps each reference movie frame to the first captured
    index of its VBlank hold; the frame after that hold is where the title
    window opens (the boot logos are twi5 then twg, in MOVIES order).
    """
    name = list(sc.MOVIES)[-1]
    movie_file = None
    for f in os.listdir(game_dir):
        if f.upper() == sc.MOVIES[name]:
            movie_file = os.path.join(game_dir, f)
    if movie_file is None:
        sys.exit('title_capture: %s not found in %s' % (sc.MOVIES[name], game_dir))
    refs = [hashlib.md5(d).digest() for d in sc.reference_frames(movie_file, None)]
    mapping = sc.align(refs, cap)
    matched = [j for j in mapping if j is not None]
    if not matched:
        sys.exit('title_capture: could not align %s against the capture' % name)
    last = matched[-1]
    end = last
    while end < len(cap) and cap[end] == cap[last]:
        end += 1
    print('title_capture: %s last presented %d, title candidate frame %d'
          % (name, last, end))
    return end, mapping


def write_frames(frames, out_dir):
    os.makedirs(out_dir, exist_ok=True)
    for old in os.listdir(out_dir):
        if old.startswith('frame_') and old.endswith('.raw'):
            os.remove(os.path.join(out_dir, old))
    for i, data in enumerate(frames):
        with open(os.path.join(out_dir, 'frame_%04d.raw' % i), 'wb') as f:
            f.write(data)


def render(paths, anchor, port_dir, game_dir):
    """Return the frame bytes to write, one per output frame."""
    cap = cap_hashes(paths)
    if port_dir:
        # Full content alignment (not just the start index): the game logic runs
        # at 60 Hz while the capture records at ~70 Hz, so one output frame per
        # port frame is the only contract title_compare's frame-for-frame read
        # can honour. smk_capture.align is the same monotonic matcher 2b uses.
        port = read_port_frames(port_dir)
        ph = [hashlib.md5(d).digest() for d in port]
        mapping = sc.align(ph, cap)
        missing = [i for i, j in enumerate(mapping) if j is None]
        if missing:
            sys.exit('title_capture: port frame %d not found in the capture; the '
                     'anchor is off or the capture is too short' % missing[0])
        want = {j: i for i, j in enumerate(mapping)}
        out = [None] * len(mapping)
        for idx, data in enumerate(sc.read_avi_frames(paths)):
            i = want.get(idx)
            if i is not None:
                out[i] = data
        if any(x is None for x in out):
            sys.exit('title_capture: internal error assembling aligned frames')
        return out
    if anchor is None:
        anchor = auto_anchor(paths, cap, game_dir)[0]
    h0 = cap[anchor]
    out_idx = []
    seen_other = False
    for idx in range(anchor, len(cap)):
        h = cap[idx]
        if idx > anchor and h == h0 and seen_other:
            print('title_capture: frame %d repeats frame %d, stopping (loop)'
                  % (idx, anchor))
            break
        if h != h0:
            seen_other = True
        out_idx.append(idx)
    wanted = set(out_idx)
    out = [None] * len(out_idx)
    pos = {idx: k for k, idx in enumerate(out_idx)}
    for idx, data in enumerate(sc.read_avi_frames(paths)):
        if idx in wanted:
            out[pos[idx]] = data
    return out


def one_capture(args, avi_dir):
    game = stage(args.game_dir)
    names = sc.run_dosbox(game, avi_dir, args.time_limit)
    paths = [os.path.join(avi_dir, n) for n in names]
    fps = sc.ffprobe_fps(paths[0])
    total = sum(1 for _ in sc.read_avi_frames(paths))
    print('title_capture: %d AVI(s), %.4f fps, %d frames'
          % (len(paths), fps, total))
    return render(paths, args.anchor, args.port_anchor, args.game_dir), fps


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--out', required=True,
                    help='output directory for frame_%04d.raw')
    ap.add_argument('--src', help='PRAGE.EXE to patch with title_pin.patch() '
                                  'into %s first' % PIN_EXE)
    ap.add_argument('--game-dir', default=DEFAULT_GAME_DIR)
    ap.add_argument('--time-limit', type=int, default=45,
                    help='DOSBox-X -time-limit seconds (default 45)')
    ap.add_argument('--anchor', type=int,
                    help='explicit capture frame index for frame_0000')
    ap.add_argument('--port-anchor',
                    help='directory of the port\'s frame_%04d.raw dump; each port '
                         'frame is content-matched to the capture')
    ap.add_argument('--verify-reproducible', action='store_true',
                    help='capture twice into temp dirs and require byte-identical '
                         'frames (the pin gate)')
    ap.add_argument('--keep-avi', action='store_true')
    a = ap.parse_args()

    if a.src:
        title_pin.patch(a.src, PIN_EXE)
    if not os.path.isfile(PIN_EXE):
        sys.exit('title_capture: pinned copy missing at %s (run make title-pin)'
                 % PIN_EXE)

    def run(dest=None):
        avi = tempfile.mkdtemp(prefix='titlecap-')
        try:
            frames, fps = one_capture(a, avi)
            if dest is not None:
                write_frames(frames, dest)
            return frames
        finally:
            if a.keep_avi:
                print('title_capture: AVIs kept in %s' % avi)
            else:
                shutil.rmtree(avi, ignore_errors=True)

    if a.verify_reproducible:
        print('== title_capture: reproducibility gate (two captures) ==')
        first = run()
        second = run()
        n = min(len(first), len(second))
        for i in range(n):
            if first[i] != second[i]:
                sys.exit('title_capture: NOT reproducible: %d vs %d frames, '
                         'first differing frame %d' % (len(first), len(second), i))
        if len(first) != len(second):
            sys.exit('title_capture: NOT reproducible: %d vs %d frames'
                     % (len(first), len(second)))
        print('title_capture: reproducible: %d frames byte-identical' % n)
        return 0

    frames = run(a.out)
    print('title_capture: wrote %d frames to %s' % (len(frames), a.out))
    return 0


if __name__ == '__main__':
    sys.exit(main())
