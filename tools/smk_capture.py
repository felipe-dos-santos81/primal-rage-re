#!/usr/bin/env python3
"""Capture the original Primal Rage Smacker logos from DOSBox-X into a
pixel-exact per-frame oracle, and measure their pacing.

Mechanism (Task 2, option 1 of the plan's ladder): DOSBox-X's built-in
``DX-CAPTURE /V`` shell command starts a lossless AVI+ZMBV video capture, runs
``PRAGE.EXE -f``, and finalises the AVI when the emulator shuts down at
``-time-limit``.  Backed by DOSBox-X's own log line ``USING AVI+ZMBV`` and by
decoding the result with ffmpeg.  ZMBV is lossless, so the decoded RGB is the
VGA DAC output the original presented.  DOSBox-X opens a new AVI on each video
mode change (here text-mode boot in ``*_000.avi``, the 320x200 game in
``*_001.avi``); every ``*.avi`` in the capture directory is decoded and
concatenated in filename order, so the text-mode frames are simply never
referenced.

Limits of the mechanism: it needs a real DOSBox-X run (a windowless, scripted
one: ``-nogui -nomenu``), it captures the whole boot (not just the logos), and
it can only observe the DAC output (RGB), never the 8-bit index buffer.

Frame-index alignment (Task 2, option 3 of the plan's ladder): the original is
VBlank-gated, so it presents each Smacker frame for a whole number of ~70 Hz
VGA frames and the AVI captures at that same rate; a movie frame therefore
appears as a run of identical AVI frames.  Each reference frame is matched
against the AVI sequence monotonically (a held frame maps back to the same AVI
index), which also handles a frame the original holds for several Smacker
frames.  The reference decode is *alignment only*: the pixels written to the
oracle are always the original's captured pixels, never the reference's.  A
reference frame that cannot be found in the interior aborts the tool rather
than emitting a misaligned oracle; trailing frames the original does not
present are dropped.

Output (all git-ignored):

    <out>/<movie>/frame_%04d.raw   320x200 RGB24 (192000 bytes per frame)
    <out>/<movie>/palette.txt      256 `RRGGBB` lines (container palette, frame 0)
    <out>/pacing.txt               measured duration / fps / us-per-frame

RGB rather than palette indices because the capture is RGB and the movies'
palettes contain duplicate colours, so an RGB->index reduction would be
ambiguous; RGB comparison is also stricter (it exercises the decoder's palette
as well as its indices, the design's "pixel-exact (palette included)").
"""
import argparse
import hashlib
import os
import shutil
import struct
import subprocess
import sys
import tempfile

FRAME_W, FRAME_H = 320, 200
FRAME_BYTES = FRAME_W * FRAME_H * 3

# Container palette expansion table (6-bit -> 8-bit), from the format reference.
SMK_PAL = bytes([
    0x00, 0x04, 0x08, 0x0C, 0x10, 0x14, 0x18, 0x1C, 0x20, 0x24, 0x28, 0x2C, 0x30, 0x34, 0x38, 0x3C,
    0x41, 0x45, 0x49, 0x4D, 0x51, 0x55, 0x59, 0x5D, 0x61, 0x65, 0x69, 0x6D, 0x71, 0x75, 0x79, 0x7D,
    0x82, 0x86, 0x8A, 0x8E, 0x92, 0x96, 0x9A, 0x9E, 0xA2, 0xA6, 0xAA, 0xAE, 0xB2, 0xB6, 0xBA, 0xBE,
    0xC3, 0xC7, 0xCB, 0xCF, 0xD3, 0xD7, 0xDB, 0xDF, 0xE3, 0xE7, 0xEB, 0xEF, 0xF3, 0xF7, 0xFB, 0xFF])

MOVIES = {'twi5': 'TWI5.SMK', 'twg': 'TWG.SMK'}


def u32(b, o):
    return struct.unpack_from('<I', b, o)[0]


def read_container(path):
    """Parse enough of an SMK2 container for the palette and frame count."""
    b = open(path, 'rb').read()
    magic = b[:4]
    w, h, frames, pts = struct.unpack_from('<IIII', b, 4)
    if magic not in (b'SMK2', b'SMK4'):
        raise ValueError('not an SMK file: %s' % path)
    treesize = u32(b, 0x34)
    tbl = 0x68
    sizes = [u32(b, tbl + 4 * i) for i in range(frames)]
    foff = tbl + 4 * frames
    fflags = b[foff:foff + frames]
    data = foff + frames + treesize
    return dict(b=b, frames=frames, pts=pts, sizes=sizes, fflags=fflags, data=data)


def parse_palette(payload):
    """Decode one Smacker palette-update payload into 256 RGB triplets."""
    pal = bytearray(768)
    i = 0
    entries = 0
    n = len(payload)
    while entries < 256:
        t = payload[i]
        i += 1
        if t & 0x80:
            entries += (t & 0x7f) + 1
        elif t & 0x40:
            cnt = (t & 0x3f) + 1
            off = payload[i]
            i += 1
            if off + cnt > 256:
                raise ValueError('palette copy out of range')
            pal[entries * 3:entries * 3 + cnt * 3] = pal[off * 3:off * 3 + cnt * 3]
            entries += cnt
        else:
            r = SMK_PAL[t & 0x3f]
            g = SMK_PAL[payload[i] & 0x3f]
            i += 1
            bl = SMK_PAL[payload[i] & 0x3f]
            i += 1
            pal[entries * 3] = r
            pal[entries * 3 + 1] = g
            pal[entries * 3 + 2] = bl
            entries += 1
        if i > n:
            raise ValueError('palette payload overrun')
    return bytes(pal)


def movie_frame0_palette(container):
    """Palette in effect for frame 0 (the movies' first frame updates it)."""
    sz = container['sizes'][0] & ~3
    payload = container['b'][container['data']:container['data'] + sz]
    if container['fflags'][0] & 1:
        return parse_palette(payload)
    return bytes(768)


def which(tool):
    p = shutil.which(tool)
    if not p:
        sys.exit('smk_capture: required tool not found on PATH: %s' % tool)
    return p


def run_dosbox(game_dir, avi_dir, time_limit):
    """Capture the whole boot to AVI(s) with DOSBox-X's DX-CAPTURE /V."""
    dosbox = which('dosbox-x')
    cmd = [dosbox, '-defaultconf', '-fastlaunch', '-nopromptfolder', '-nogui',
           '-nomenu', '-time-limit', str(time_limit),
           '-set', 'sdl fullscreen=false',
           '-set', 'dosbox captures=%s' % avi_dir,
           '-c', 'MOUNT C "%s"' % game_dir]
    iso = os.path.join(os.path.dirname(game_dir.rstrip('/')), 'CD', 'RAGECD.ISO')
    if os.path.isfile(iso):
        cmd += ['-c', 'IMGMOUNT D "%s" -t iso' % iso]
    else:
        print('smk_capture: no CD image at %s (mounting C only)' % iso)
    cmd += ['-c', 'C:', '-c', 'DX-CAPTURE /V PRAGE.EXE -f', '-c', 'EXIT']
    print('smk_capture: %s' % ' '.join(cmd))
    r = subprocess.run(cmd)
    if r.returncode != 0:
        sys.exit('smk_capture: dosbox-x exited %d' % r.returncode)
    avis = sorted(f for f in os.listdir(avi_dir) if f.lower().endswith('.avi'))
    if not avis:
        sys.exit('smk_capture: dosbox-x produced no AVI in %s' % avi_dir)
    return avis


def ffprobe_fps(path):
    out = subprocess.check_output(
        ['ffprobe', '-v', 'error', '-select_streams', 'v:0',
         '-show_entries', 'stream=avg_frame_rate',
         '-of', 'default=noprint_wrappers=1:nokey=1', path], text=True).strip()
    num, den = out.split('/')
    return float(num) / float(den)


def read_avi_frames(paths):
    """Yield 320x200 RGB24 frames from every AVI, in order (ffmpeg pipe)."""
    ffmpeg = which('ffmpeg')
    for path in paths:
        p = subprocess.Popen(
            [ffmpeg, '-v', 'error', '-i', path,
             '-vf', 'scale=%d:%d:flags=neighbor' % (FRAME_W, FRAME_H),
             '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-'],
            stdout=subprocess.PIPE)
        try:
            while True:
                data = p.stdout.read(FRAME_BYTES)
                if len(data) < FRAME_BYTES:
                    break
                yield data
        finally:
            p.stdout.close()
            p.wait()


def reference_frames(movie_file, ref_dir):
    """RGB reference frames used only to align the capture to movie indices."""
    if ref_dir:
        paths = sorted(f for f in os.listdir(ref_dir) if f.endswith('.raw'))
        if not paths:
            sys.exit('smk_capture: no *.raw reference frames in %s' % ref_dir)
        for f in paths:
            data = open(os.path.join(ref_dir, f), 'rb').read()
            if len(data) != FRAME_BYTES:
                sys.exit('smk_capture: %s is %d bytes, expected RGB24 %d'
                         % (f, len(data), FRAME_BYTES))
            yield data
        return
    ffmpeg = which('ffmpeg')
    p = subprocess.Popen(
        [ffmpeg, '-v', 'error', '-i', movie_file, '-f', 'rawvideo',
         '-pix_fmt', 'rgb24', '-'], stdout=subprocess.PIPE)
    try:
        while True:
            data = p.stdout.read(FRAME_BYTES)
            if len(data) < FRAME_BYTES:
                break
            yield data
    finally:
        p.stdout.close()
        p.wait()


def align(ref_hashes, cap_hashes):
    """Monotonic content match: ref frame i -> capture index j (or None).

    `cap_hashes[j]` equals `ref_hashes[i]`; when the original holds a frame for
    several movie frames, both map to the first index of that hold.
    """
    by_hash = {}
    for j, h in enumerate(cap_hashes):
        by_hash.setdefault(h, []).append(j)
    out = []
    prev = 0
    for h in ref_hashes:
        idxs = by_hash.get(h)
        j = None
        if idxs:
            for k in idxs:
                if k >= prev:
                    j = k
                    break
        out.append(j)
        if j is not None:
            prev = j
    return out


def capture_movie(name, out_dir, game_dir, ref_dir, cap_hashes, frame_bytes):
    movie_file = None
    for f in os.listdir(game_dir):
        if f.upper() == MOVIES[name]:
            movie_file = os.path.join(game_dir, f)
    if movie_file is None:
        sys.exit('smk_capture: %s not found in %s' % (MOVIES[name], game_dir))

    refs = [hashlib.md5(d).digest() for d in
            reference_frames(movie_file, ref_dir)]
    mapping = align(refs, cap_hashes)

    unmatched = [i for i, j in enumerate(mapping) if j is None]
    trailing = 0
    if unmatched:
        # Only a suffix of undisplayed frames is tolerated (the original's
        # player drops the last frame or two); an interior gap is fatal.
        while trailing < len(mapping) and mapping[len(mapping) - 1 - trailing] is None:
            trailing += 1
        n_missing = len(unmatched)
        if n_missing != trailing:
            sys.exit('smk_capture: %s: %d reference frames unmatched (first %s) '
                     '- capture and reference disagree; refusing to emit a '
                     'misaligned oracle' % (name, n_missing, unmatched[0]))
        mapping = mapping[:len(mapping) - trailing]
    if not mapping:
        sys.exit('smk_capture: %s: no frames aligned' % name)

    md = os.path.join(out_dir, name)
    os.makedirs(md, exist_ok=True)
    for old in os.listdir(md):
        if old.endswith('.raw'):
            os.remove(os.path.join(md, old))

    for i, j in enumerate(mapping):
        data = frame_bytes[refs[i]]
        with open(os.path.join(md, 'frame_%04d.raw' % i), 'wb') as f:
            f.write(data)

    container = read_container(movie_file)
    pal = movie_frame0_palette(container)
    with open(os.path.join(md, 'palette.txt'), 'w') as f:
        for c in range(256):
            f.write('%02X%02X%02X\n' % (pal[c * 3], pal[c * 3 + 1], pal[c * 3 + 2]))

    first, last = mapping[0], mapping[-1]
    end = last + 1
    while end < len(cap_hashes) and cap_hashes[end] == cap_hashes[last]:
        end += 1
    return dict(name=name, file=os.path.basename(movie_file),
                header_frames=container['frames'],
                displayed=len(mapping), first=first, last=last, end=end,
                trailing=trailing)


def self_test():
    """Runnable check for the alignment and palette logic (stdlib only)."""
    a = b'A' * 4
    b = b'B' * 4
    cap = [a, b, b, b, a, a]
    ref = [a, b]                      # frame 1 held -> both map to index 1
    assert align([hashlib.md5(x).digest() for x in ref],
                 [hashlib.md5(x).digest() for x in cap]) == [0, 1]
    ref2 = [a, b, a]                  # after frame 1, frame 2 maps to its own hold
    assert align([hashlib.md5(x).digest() for x in ref2],
                 [hashlib.md5(x).digest() for x in cap]) == [0, 1, 4]
    ref3 = [a, b, b, b, b, b]         # two held frames -> same capture index
    assert align([hashlib.md5(x).digest() for x in ref3],
                 [hashlib.md5(x).digest() for x in cap]) == [0, 1, 1, 1, 1, 1]
    # palette: one new entry (R from the control nibble, G/B following),
    # then two skips summing to the remaining 255 entries.
    payload = bytes([0x00, 5, 7, 0xFF, 0x80 | 126])
    pal = parse_palette(payload)
    assert (pal[0], pal[1], pal[2]) == (SMK_PAL[0], SMK_PAL[5], SMK_PAL[7])
    assert all(pal[k] == 0 for k in range(3, 768))
    print('smk_capture: self-test passed')


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--game-dir', default='data/game/C')
    ap.add_argument('--out', default='data/smk-captures')
    ap.add_argument('--movies', nargs='+', default=['twi5', 'twg'],
                    choices=sorted(MOVIES))
    ap.add_argument('--avi', help='reuse AVIs already in this directory '
                                  'instead of running DOSBox-X')
    ap.add_argument('--keep-avi', action='store_true',
                    help='do not delete the temporary AVI directory')
    ap.add_argument('--reference-dir',
                    help='align against RGB .raw frames here (e.g. the port '
                         'dump) instead of an ffmpeg decode of the movie')
    ap.add_argument('--time-limit', type=int, default=30,
                    help='DOSBox-X -time-limit seconds (default 30)')
    ap.add_argument('--self-test', action='store_true')
    a = ap.parse_args()

    if a.self_test:
        self_test()
        return 0

    for m in a.movies:
        if m not in MOVIES:
            sys.exit('smk_capture: unknown movie %r' % m)

    avi_dir = a.avi
    tmp = None
    if not avi_dir:
        tmp = tempfile.mkdtemp(prefix='smkcap-')
        avi_dir = os.path.join(tmp, 'avi')
        os.makedirs(avi_dir)
        avi_names = run_dosbox(a.game_dir, avi_dir, a.time_limit)
    else:
        avi_names = sorted(f for f in os.listdir(avi_dir) if f.lower().endswith('.avi'))
        if not avi_names:
            sys.exit('smk_capture: no AVI in %s' % avi_dir)

    avi_paths = [os.path.join(avi_dir, f) for f in avi_names]
    host_fps = ffprobe_fps(avi_paths[0])
    print('smk_capture: %d AVI(s), host frame rate %.4f fps'
          % (len(avi_paths), host_fps))

    # Reference hashes first, so only frames we need are held in memory.
    ref_hashes = {}
    for m in a.movies:
        movie_file = None
        for f in os.listdir(a.game_dir):
            if f.upper() == MOVIES[m]:
                movie_file = os.path.join(a.game_dir, f)
        if movie_file is None:
            sys.exit('smk_capture: %s not found in %s' % (MOVIES[m], a.game_dir))
        ref_hashes[m] = [hashlib.md5(d).digest() for d in
                         reference_frames(movie_file, a.reference_dir)]
    wanted = set()
    for hs in ref_hashes.values():
        wanted.update(hs)

    cap_hashes = []
    frame_bytes = {}
    for data in read_avi_frames(avi_paths):
        h = hashlib.md5(data).digest()
        cap_hashes.append(h)
        if h in wanted and h not in frame_bytes:
            frame_bytes[h] = data
    print('smk_capture: %d captured frames, %d reference frames'
          % (len(cap_hashes), sum(len(v) for v in ref_hashes.values())))

    os.makedirs(a.out, exist_ok=True)
    host_period = 1.0 / host_fps
    results = []
    for m in a.movies:
        results.append(capture_movie(m, a.out, a.game_dir,
                                     a.reference_dir, cap_hashes, frame_bytes))

    with open(os.path.join(a.out, 'pacing.txt'), 'w') as f:
        f.write('# measured from the DOSBox-X AVI capture (VBlank-gated original)\n')
        f.write('# host capture: %.6f fps (%.3f us/frame)\n'
                % (host_fps, host_period * 1e6))
        for r in results:
            dur = (r['end'] - r['first']) * host_period
            fps = r['header_frames'] / dur
            f.write('%s: duration=%.3fs frames=%d displayed=%d fps=%.3f '
                    'us_per_frame=%.1f\n'
                    % (r['name'], dur, r['header_frames'], r['displayed'],
                       fps, dur / r['header_frames'] * 1e6))
            print('smk_capture: %s: %d frames written, %.3fs, %.3f fps, '
                  '%.1f us/frame' % (r['name'], r['displayed'], dur, fps,
                                     dur / r['header_frames'] * 1e6))

    if tmp and not a.keep_avi:
        shutil.rmtree(tmp, ignore_errors=True)
    else:
        print('smk_capture: AVIs kept in %s' % avi_dir)
    return 0


if __name__ == '__main__':
    sys.exit(main())
