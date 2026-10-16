#!/usr/bin/env python3
"""The gameplay oracle (spec 2026-09-30-gameplay-ground-truth-design.md §4.3,
record §G.13..). Two claims, each a ratchet on its first unexplained item:

  frames  the capture's distinct frames (data/k11-captures/<scenario>/
          frame_%05d.raw.gz, gp_capture) against the port's displayed frames
          (<port>/frame_%05d.ipx, test_gp_replay) with title_compare.explain's
          model: clean, a byte splice of adjacent port frames (the 70.09 Hz
          capture against the 60.05 Hz game), or one transition row. All-black
          capture frames are the documented capture artefact and are skipped.
          Frames named in BANDS_ALLOWED for the scenario are explained ONLY by
          their per-frame byte recipe (a top/bottom byte-bind to one unique
          adjacent port pair plus a pinned band hash; record §G5) — never a
          class, and a named frame whose bytes are not the recipe fails.
          Frames named in SCAN3_ALLOWED are explained ONLY by their per-frame
          three-segment recipe: a pinned top-row hash that at least one EARLIER
          dump frame must hold, plus one unique adjacent port pair (B, B+1)
          byte-binding the rows below the top (record §G6). Frames named in
          OVERLAY_ALLOWED are explained ONLY by their per-frame glyph-overlay
          recipe: one port frame's pixels on the mask where a second port frame
          differs from a third (a pinned glyph fingerprint), and the third's
          (the screen's) off it (record §G7). None of the three tables is
          ever an extension of the two-frame model.
          The first unexplained capture frame must be >= --min-first N, and the
          window start (the first capture frame that shows the port's first frame)
          must be <= --max-start and < N, so a regressed port cannot slide the window
          forward past the frame that set N. Not claimed: the order of the port's
          frames, and that every port frame appears (a coverage count is reported).
  trace   the capture's S records (poll.log) against the port's T records
          (trace.txt) by the frame counter f, over gp_session.TRACE_FIELDS;
          the first differing f must be >= --trace-min-first F. tick is
          reported apart (host-timed, spec §7 Q6).

The search for a frame's explanation tries the port frames [p - BACK, p +
AHEAD) around the last explained index p first, then the whole dump, so the
result is the unwindowed classification (the window is a search order, a
harness value). Enforced runs stop at the first unexplained frame; --report
goes on (up to REPORT_MAX) and always exits 0. An absent capture skips (exit
0); a present capture with an unpinned N or start fails. Stdlib only; title_compare and
gp_session are read-only here."""
import argparse
import collections
import gzip
import hashlib
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gp_session as gs
import title_compare as tc

BACK, AHEAD = 2, 64          # harness: the search order (spec §4.3), not a game value
REPORT_MAX = 5
IPX_BYTES = 64000 + 768

# Per-scenario named per-frame allowances for the frames claim, patterned on
# title_compare.ATTRACT2_SPLICE3_ALLOWED (record §G5): only the named capture
# frames, each only with its own byte recipe, never a class and never an
# extension of the two-frame model. An entry is (capture frame j, top_row, band_hi,
# band_sha256): the frame is explained ONLY if
#   1. sha256 of its bytes [top_row*ROW, band_hi*ROW) equals band_sha256, and
#   2. EXACTLY ONE adjacent pair (P, P+1) in the CURRENT port dump holds: the
#      capture's rows [0, top_row) equal port[P]'s rows and the capture's rows
#      [band_hi, FRAME_H) equal port[P+1]'s.
# The bound rows are byte-verified against the port's dump, so a port that renders
# either bound frame differently un-explains the capture frame (Review Focus #1;
# BandAllowanceTests). The band itself has no port binding and is capture-pinned
# bytes: that is the allowance's named limit — the band is a present state the
# driver never dumped (one frame per f; record §K.10 item 2), and its cause stays
# the named gap §K.12 records. A named frame the two-frame model explains now is
# reported as a stale name; a named frame that is not the named bytes FAILS.
BANDS_ALLOWED = {
    'gp-keys-fight': (
        # Capture 835 (raw 3934): the altq-n (QUIT TO DOS?, c = 0x833) scan-out
        # catch-up frame. Rows 0..80 are byte-equal to the port's f = 0x832 frame,
        # rows 92..199 to f = 0x833, and rows 81..91 hold an 11-row band no port
        # frame holds (2442 of its bytes equal neither bound frame; §G5 re-measured
        # at 5f874678, which is also where N/MAX_START/TRACE were pinned). The pair
        # (592, 593) is the dump's unique match (measured: 1 hit over all 789
        # frames).
        (835, 81, 92, 'd9c0ac62f7e0c0d4082fba8d195eae4f767c56a7887bdf7346450511f998cad8'),
    ),
}

# Per-scenario named THREE-frame scan-out allowances for the frames claim
# (record §G6): the mode-8 entry's catch-up burst (the gate at 0x25643 fails for
# iterations whose compute overruns an ISR tick, their presents are skipped, and
# the re-opened presents land inside ONE 70.09 Hz capture scan — see §G6 for the
# isolation and the rejection of the loader re-sync hypothesis there). Like
# BANDS_ALLOWED this is never a class and never an extension of the two-frame
# model: only the named capture frames, each only with its own byte recipe. An
# entry is (capture frame j, r1, r2, top_sha256): the frame is explained ONLY if
#   1. sha256 of its bytes [0, r1*ROW) equals top_sha256 (the still-held top rows
#      of the old screen, the segment no *unique* port frame answers for),
#   2. EXACTLY ONE adjacent pair (B, B+1) in the CURRENT port dump holds: the
#      capture's rows [r1, r2) equal port[B]'s rows and its rows [r2, FRAME_H)
#      equal port[B+1]'s (the middle range is vacuous when r1 == r2 — the U9
#      shape, where only the bottom rows are pair-bound), and
#   3. at least one port frame A with A < B holds the capture's rows [0, r1):
#      the still-scanned old screen IS one of the port's own earlier frames.
# The pair and the top are byte-verified against the port's dump, so a port that
# renders any bound segment differently un-explains the capture frame (Review
# Focus #1; Scan3AllowanceTests). The top segment is not uniquely bound: several
# frames can share those rows (§G6's table names the candidates) — the named
# hash plus the existence of an earlier holder is the allowance's named limit,
# and the old screen's whole frames are claimed by the capture frames below the
# ratchet anyway. A named frame the two-frame model explains now is reported as
# a stale name; a named frame that is not the named bytes FAILS.
SCAN3_ALLOWED = {
    'gp-idle-loss': (
        # Capture 2064 (raw 5190): the round-1 mode-8 entry catch-up (§G6).
        # Rows 0..2 are port 1665/1666's (f=0xC70/0xC71, identical there, the
        # screen held across raws 5187..5189), rows 3..97 are port 1669's
        # (f=0xC74) exclusively and rows 98..199 are port 1670's (f=0xC75)
        # exclusively; the pair (1669, 1670) is the 6301-frame dump's unique
        # match; f=0xC72/0xC73 were never blitted (their presents the gate
        # skipped — §G6).
        (2064, 3, 98, '7bdf2c561c38a264337bc74abd093172ac99ceebb8c818635e189530cf1c58dc'),
    ),
    'gp-u9-win': (
        # Capture 346 (raw 2830): the same mode-8 entry under U9's pokes, but
        # the shape is the unpoked one (§G6/§W.12). Rows 0..4 are port 218's
        # (f=0x48A, the first mode-8 frame, held across raws 2827..2829) and
        # rows 5..199 are port 221's (f=0x48D) exclusively — no middle band;
        # the vacuous-middle pair (220, 221) is the 1977-frame dump's unique
        # match; ports 219/220 (f=0x48B/0x48C) were never blitted.
        (346, 5, 5, '703828af123edf1a8c3bade8d42bdd56a68576cd87e13de7339bdd203dc8f0b6'),
    ),
    'gp-u10-ending': (
        # Capture 331 (raw 2831): the same mode-8 entry (§G6/§W.14). Rows 0..1
        # are port 216's (f=0x483, the first mode-8 frame, held across raws
        # 2828..2830; the dump's holders of those two rows before port 218 are
        # {216, 243, 1127}), rows 2..63 are port 218's (f=0x485) exclusively
        # and rows 64..199 are port 219's (f=0x486) exclusively; the pair
        # (218, 219) is the 4664-frame dump's unique match; port 217 (f=0x484)
        # was never blitted.
        (331, 2, 64, '2d6fb6ad829baae61c65a844fb44f05cb51393322afdce4ab8117960e1306ddd'),
    ),
}


# Per-scenario named per-frame glyph-overlay allowances (record §G7): the one
# capture frame whose loader presentation landed on a screen the replay only
# presents later. The loader blits its `- LOADING -` text (string 489,
# 0x1B3EA/0x1B3EF) straight into the aperture, over whatever the screen holds,
# and the read that follows stalls the loop. At the mode-0x12 entry the first
# read of s16snd2 (index 6) is a *music* request (voice 0x32 case 1, song handle
# 0x03021BFA): the original's sequencer reads the song through the resource
# layer (0x1B544, draw arm) in the entry's window, so the text lands over the
# entry's screen; the port's music path is host-owned (platform/audio/ never
# resolves), so the replay's first s16snd2 resolve is the *sample* queue of
# voice 0x3A (0x03022554, also index 6) at f=0xC0E and the same text lands one
# screen later. The capture frame is therefore `- LOADING -` over the
# mode-0x12 screen — a composition of two frames the port itself dumped (the
# presentation frame and the screen), which the two-frame model cannot express.
# Like BANDS_ALLOWED/SCAN3_ALLOWED this is never a class and never an extension
# of the two-frame model: only the named capture frames, each only with its own
# byte recipe. An entry is (capture j, T1, T2, S, glyph_sha): the frame is
# explained ONLY if
#   1. sha256 of T1's bytes on the mask {i: T1[i] != T2[i]} equals glyph_sha
#      (T1 = the port's `- LOADING` frame with the glyphs visible; T2 = the same
#      indices under the stale black palette, so the mask is exactly the
#      glyphs' pixels and the mask's shape is part of the fingerprint), and
#   2. every pixel of the capture equals T1's on the mask and S's (the screen
#      frame) off it — the whole frame, not a box.
# So a mutation of T1's overlay bytes, of the mask's shape (via T2) or of any
# S pixel un-explains the capture frame (OverlayAllowanceTests). Named limits
# (as SCAN3_ALLOWED's top segment): T2 is only the mask's indicator — its bytes
# inside the mask bind nothing beyond the `!=` relation, and T1's bytes off the
# mask are not claimed; the glyph sha binds T1's byte sequence at the sorted
# mask indices, not the index set itself, so two different index sets carrying
# identical T1 bytes could coincide. A named frame the two-frame model explains
# now is reported as a stale name; a named frame that is not its named bytes
# FAILS.
OVERLAY_ALLOWED = {
    'gp-u9-win': (
        # Capture 2209 (raw 4953) at the mode-0x17->0x12 entry: the loader's
        # text (port 1776's 166 glyph px, rows 192..197 x 0..85) over the
        # mode-0x12 screen (port 1777, the frame at f=0xBA1 = the entry).
        # Port 1775 is the same index image under the stale black palette (the
        # mask's source). The capture shows the presentation one screen later
        # than the replay's own (the replay's is over the black screen,
        # capture 2208 = port 1776); the port's own later presentation of the
        # same text is dump frame 1805 (f=0xC0E). §G6's "held by no frame"
        # stands for the whole strip; this names the composition.
        (2209, 1776, 1775, 1777,
         'de1c1bb2b51d72c402f12b25450fdcdbc64d90ff7f22f0f506e335a994b31ced'),
    ),
    'gp-u10-ending': (
        # Capture 2062 (raw 4954): the same presentation at U10's entry
        # (port 1685 over 1686, mask source 1684 — the palette pair is swapped
        # against U9's). The glyph fingerprint is the same bytes (the same
        # string at the same place).
        (2062, 1685, 1684, 1686,
         'de1c1bb2b51d72c402f12b25450fdcdbc64d90ff7f22f0f506e335a994b31ced'),
    ),
}


def band_pair(c, port, top_row, band_hi):
    """The unique adjacent port pair P with c's rows [0, top_row) == port[P]'s and
    c's rows [band_hi, FRAME_H) == port[P+1]'s; None if there is not exactly one."""
    R = tc.ROW
    if len(port) < 2:
        return None
    lo, hi = c[:top_row * R], c[band_hi * R:]
    hits = []
    prev = port[0]
    for m in range(1, len(port)):
        cur = port[m]
        if prev[:top_row * R] == lo and cur[band_hi * R:] == hi:
            hits.append(m - 1)
        prev = cur
    return hits[0] if len(hits) == 1 else None


def band_explain(c, port, entry):
    """(P, None) when capture frame c is byte-identical to the named entry's recipe
    against this port dump; (None, why) otherwise."""
    _j, top_row, band_hi, band_sha = entry
    got = hashlib.sha256(c[top_row * tc.ROW:band_hi * tc.ROW]).hexdigest()
    if got != band_sha:
        return None, 'its band rows %d..%d are %s.., not the named %s..' % (
            top_row, band_hi - 1, got[:12], band_sha[:12])
    P = band_pair(c, port, top_row, band_hi)
    if P is None:
        return None, 'no unique adjacent port pair binds its rows [0,%d)/[%d,200)' % (
            top_row, band_hi)
    return P, None


def scan3_bind(c, port, r1, r2):
    """The unique adjacent port pair (B, B+1) with c's rows [r1, r2) == port[B]'s
    and c's rows [r2, FRAME_H) == port[B+1]'s; None if there is not exactly one.
    The [r1, r2) test is vacuous when r1 == r2 (the U9 two-segment shape), so the
    bind then asks for exactly one pair whose B+1 holds the whole [r2, FRAME_H)."""
    R = tc.ROW
    if len(port) < 2:
        return None
    mid, bot = c[r1 * R:r2 * R], c[r2 * R:]
    hits = []
    for m in range(1, len(port)):
        if port[m - 1][r1 * R:r2 * R] == mid and port[m][r2 * R:] == bot:
            hits.append(m - 1)
    return hits[0] if len(hits) == 1 else None


def scan3_top(c, port, B, r1):
    """The port frames A < B that hold c's rows [0, r1) (the still-scanned old
    screen): the earlier holders, empty when none — the top segment is not
    uniquely bound (§G6's named limit), only required to exist below B."""
    R = tc.ROW
    lo = c[:r1 * R]
    return [a for a in range(B) if port[a][:r1 * R] == lo]


def scan3_explain(c, port, entry):
    """(exhibits, None) when capture frame c is byte-identical to the named
    three-frame recipe against this port dump; (None, why) otherwise. exhibits is
    the earlier top holders plus the bound pair (B, B+1); B is dropped when the
    middle range is vacuous (r1 == r2) so a frame with no bound pixel is never
    exhibited."""
    _j, r1, r2, top_sha = entry
    got = hashlib.sha256(c[:r1 * tc.ROW]).hexdigest()
    if got != top_sha:
        return None, 'its top rows 0..%d are %s.., not the named %s..' % (
            r1 - 1, got[:12], top_sha[:12])
    B = scan3_bind(c, port, r1, r2)
    if B is None:
        return None, 'no unique adjacent port pair binds its rows [%d,%d) and [%d,200)' % (
            r1, r2, r2)
    tops = scan3_top(c, port, B, r1)
    if not tops:
        return None, 'no port frame before %d holds its named top rows' % B
    ex = sorted(set(tops) | ({B, B + 1} if r1 < r2 else {B + 1}))
    return ex, None


def overlay_mask(t1, t2):
    """The pixel indices i where T1 differs from T2 (§G7: the loader's glyph
    pixels, the presentation frame against the same indices under the stale
    black palette)."""
    return [i for i in range(tc.FRAME_W * tc.FRAME_H)
            if t1[3 * i:3 * i + 3] != t2[3 * i:3 * i + 3]]


def overlay_explain(c, port, entry):
    """(info, None) when capture frame c is byte-identical to the named entry's
    recipe against this port dump; (None, why) otherwise. info is the mask's
    size; the caller reports the frames. Every pixel is checked: T1's off the
    mask and T2's values bind nothing (T2 is the mask's indicator only)."""
    _j, t1_i, t2_i, s_i, glyph_sha = entry
    t1, t2, s = port[t1_i], port[t2_i], port[s_i]
    mask = overlay_mask(t1, t2)
    got = hashlib.sha256(b''.join(t1[3 * i:3 * i + 3] for i in mask)).hexdigest()
    if got != glyph_sha:
        return None, 'its glyph pixels are %s.., not the named %s..' % (got[:12], glyph_sha[:12])
    mset = set(mask)
    for i in range(tc.FRAME_W * tc.FRAME_H):
        if i in mset:
            want, src = t1, t1_i
        else:
            want, src = s, s_i
        if c[3 * i:3 * i + 3] != want[3 * i:3 * i + 3]:
            return None, ('its pixel row %d x %d is %s, not port %d\'s %s'
                          % (i // tc.FRAME_W, i % tc.FRAME_W, c[3 * i:3 * i + 3].hex(),
                             src, want[3 * i:3 * i + 3].hex()))
    return len(mask), None


def expand_ipx(data):
    """64 000 indices + a 768-byte DAC -> RGB24, as fe_write_frame (rgb = dac[idx])."""
    if len(data) != IPX_BYTES:
        raise ValueError('ipx frame is %d bytes, expected %d' % (len(data), IPX_BYTES))
    idx, dac = data[:64000], data[64000:]
    rgb = bytearray(tc.FRAME_BYTES)
    for c in range(3):
        rgb[c::3] = idx.translate(bytes(dac[3 * i + c] for i in range(256)))
    return bytes(rgb)


def _paths(d, pattern):
    out, i = [], 0
    while os.path.exists(os.path.join(d, pattern % i)):
        out.append(os.path.join(d, pattern % i))
        i += 1
    return out


def load_capture_frame(path):
    with gzip.open(path) as f:
        data = f.read()
    if len(data) != tc.FRAME_BYTES:
        raise ValueError('%s is %d bytes, expected %d' % (path, len(data), tc.FRAME_BYTES))
    return data


def load_port_frame(path):
    with open(path, 'rb') as f:
        return expand_ipx(f.read())


class Lazy:
    """A read-only frame sequence loaded on demand with a small LRU cache;
    title_compare.explain indexes it like a list."""

    def __init__(self, paths, loader, cache=160):
        self.paths, self.loader, self.cache = paths, loader, cache
        self.lru = collections.OrderedDict()

    def __len__(self):
        return len(self.paths)

    def __getitem__(self, i):
        if i < 0 or i >= len(self.paths):
            raise IndexError(i)
        if i in self.lru:
            self.lru.move_to_end(i)
            return self.lru[i]
        data = self.loader(self.paths[i])
        self.lru[i] = data
        if len(self.lru) > self.cache:
            self.lru.popitem(last=False)
        return data


class View:
    def __init__(self, seq, lo, hi):
        self.seq, self.lo, self.hi = seq, lo, hi

    def __len__(self):
        return self.hi - self.lo

    def __getitem__(self, i):
        return self.seq[self.lo + i]


def shift(kind, data, lo):
    if kind == 'clean':
        return kind, data + lo
    if kind == 'splice':
        return kind, [(N + lo, a, b) for N, a, b in data]
    if kind == 'transition':
        return kind, [(N + lo, r, a, b) for N, r, a, b in data]
    return kind, data


def exhibited(kind, data):
    s = set()
    if kind == 'clean':
        s.add(data)
    elif kind == 'splice':
        for N, lo, hi in data:
            if hi > 0:
                s.add(N)
            if lo < tc.FRAME_BYTES:
                s.add(N + 1)
    elif kind == 'transition':
        for N, _r, _a, _b in data:
            s.update((N, N + 1))
    elif kind == 'band':
        s.update((data, data + 1))     # the named band splice binds port P above and P+1 below
    elif kind == 'scan3':
        s.update(data)                 # the named scan-out binds the earlier top holder(s) and the pair (B, B+1)
    elif kind == 'overlay':
        s.update(data)                 # the named overlay binds T1's mask bytes and S's off-mask bytes
    return s


def classify(c, port, rows, p):
    """(kind, data) for capture frame c, the window around p first."""
    ch = tc.row_hashes(c)
    n = len(port)
    lo, hi = max(0, p - BACK), min(n, p + AHEAD)
    if hi - lo >= 1:
        kind, data = tc.explain(c, ch, View(port, lo, hi), rows[lo:hi], hi - lo)
        if kind != 'unexplained':
            return shift(kind, data, lo)
    return tc.explain(c, ch, port, rows, n)


BLACK_ROW = hashlib.md5(bytes(tc.ROW)).digest()


def start_check(name, start, raws, max_start, min_first, out=print):
    """The window START is ratcheted too (record §I item 8): the start is where the
    capture first shows the port's first frame, so a port that regressed to an earlier
    screen that recurs later in the capture would slide it forward, past the frame that
    set N, and go green. It must not be later than the pinned start, and it must lie
    below N (a window at or beyond N claims nothing)."""
    rc = 0
    if max_start is None:
        out('gp_compare: %s: frames: FAIL: the window-start pin is not set (GP_IDLE_LOSS_MAX_START)' % name)
        rc = 1
    elif start > max_start:
        out('gp_compare: %s: frames: FAIL: window starts at capture %d (raw %d) > pinned start %d'
            % (name, start, raws[start], max_start))
        rc = 1
    elif start < max_start:
        out('gp_compare: %s: frames: window start %d < pinned start %d (improved: lower the pin)'
            % (name, start, max_start))
    if min_first is not None and start >= min_first:
        out('gp_compare: %s: frames: FAIL: window start %d >= ratchet N %d: the window claims nothing below N'
            % (name, start, min_first))
        rc = 1
    return rc


def reach_check(name, reach, min_reach, out=print):
    """The window REACH is ratcheted (sub-project A §A1): `reach` is the highest port frame
    any classified capture frame exhibits. Both the window and the coverage count are
    computed up to it, so a port that renders fewer screens stops at a lower reach and
    passes; it must be at least the pinned reach. A larger reach is an improvement (raise
    the pin). `min_reach is None` fails (a gp scenario without the pin is unguarded)."""
    rc = 0
    if min_reach is None:
        out('gp_compare: %s: frames: FAIL: the window-reach pin is not set (GP_*_MIN_REACH)' % name)
        rc = 1
    elif reach < min_reach:
        out('gp_compare: %s: frames: FAIL: window reach port %d < pinned reach %d '
            '(the port exhibited fewer port frames)' % (name, reach, min_reach))
        rc = 1
    elif reach > min_reach:
        out('gp_compare: %s: frames: window reach port %d > pinned reach %d '
            '(improved: raise the pin)' % (name, reach, min_reach))
    return rc


def frame_claim(name, cap, raws, port, rows, min_first, report, out=print, max_start=None,
                bands=None, scan3=None, overlay=None):
    """Returns (rc, first_unexplained or None, next_after_last_classified, reach). In report
    mode no ratchet verdict is printed and no pin is checked (rc stays 1 only for
    "window empty"; main() turns every report-mode rc into 0). `bands`, `scan3`
    and `overlay` are the scenario's named per-frame allowances (BANDS_ALLOWED,
    SCAN3_ALLOWED, OVERLAY_ALLOWED); a named frame that is not its named bytes
    prints its FAIL in both modes and fails the enforced run. `reach` is `p`, the
    highest exhibited port frame, the window's end (sub-project A §A1)."""
    bands = {} if bands is None else {b[0]: b for b in bands}
    scan3 = {} if scan3 is None else {s[0]: s for s in scan3}
    overlay = {} if overlay is None else {o[0]: o for o in overlay}
    start = None
    head = min(2, len(port))
    for j in range(len(cap)):
        c = cap[j]
        if not any(c):
            continue
        kind, data = tc.explain(c, tc.row_hashes(c), View(port, 0, head), rows[:head], head)
        if 0 in exhibited(kind, data):          # exact: exhibiting port 0 needs only frames 0 and 1
            start = j
            break
    if start is None:
        out("gp_compare: %s: frames: window empty: no capture frame exhibits the port's first frame" % name)
        return 1, None, 0, 0
    p, counts, black, unexpl, seen = 0, collections.Counter(), 0, [], set()
    allow_bad = 0
    j = start
    while j < len(cap):
        c = cap[j]
        if not any(c):
            black += 1
            j += 1
            continue
        kind, data = classify(c, port, rows, p)
        if j in bands:
            if kind != 'unexplained':
                out('gp_compare: %s: frames: note: capture %d (raw %d) is named in the '
                    'allowance but %s now; retire the name' % (name, j, raws[j], kind))
            else:
                P, why = band_explain(c, port, bands[j])
                if P is None:
                    out('gp_compare: %s: frames: FAIL: allowed capture %d (raw %d) is not '
                        'its named band splice: %s' % (name, j, raws[j], why))
                    allow_bad = 1
                else:
                    out('gp_compare: %s: frames: captured frame %d (raw %d) allowed by name '
                        'as a band splice: port %d/%d rows 0..%d/%d..199, band rows %d..%d '
                        'the named bytes'
                        % (name, j, raws[j], P, P + 1, bands[j][1] - 1, bands[j][2],
                           bands[j][1], bands[j][2] - 1))
                    kind, data = 'band', P
        if kind == 'unexplained' and j in scan3:
            ex, why = scan3_explain(c, port, scan3[j])
            if ex is None:
                out('gp_compare: %s: frames: FAIL: allowed capture %d (raw %d) is not '
                    'its named three-frame scan-out: %s' % (name, j, raws[j], why))
                allow_bad = 1
            else:
                _j, r1, r2, _sha = scan3[j]
                B = scan3_bind(c, port, r1, r2)
                tops = ' '.join(str(a) for a in ex if a < B)
                mid = '' if r1 == r2 else 'port %d rows %d..%d, ' % (B, r1, r2 - 1)
                out('gp_compare: %s: frames: captured frame %d (raw %d) allowed by name '
                    'as a three-frame scan-out: top rows 0..%d held by port %s, %s'
                    'port %d rows %d..199 the named bytes'
                    % (name, j, raws[j], r1 - 1, tops, mid, B + 1, r2))
                kind, data = 'scan3', ex
        elif j in scan3 and kind != 'unexplained':
            out('gp_compare: %s: frames: note: capture %d (raw %d) is named in the '
                'scan-out allowance but %s now; retire the name' % (name, j, raws[j], kind))
        if kind == 'unexplained' and j in overlay:
            info, why = overlay_explain(c, port, overlay[j])
            if info is None:
                out('gp_compare: %s: frames: FAIL: allowed capture %d (raw %d) is not '
                    'its named glyph overlay: %s' % (name, j, raws[j], why))
                allow_bad = 1
            else:
                _j, t1_i, t2_i, s_i, _sha = overlay[j]
                out('gp_compare: %s: frames: captured frame %d (raw %d) allowed by name '
                    'as a glyph overlay: %d px of port %d over port %d (the mask from '
                    'port %d, the named glyph bytes)'
                    % (name, j, raws[j], info, t1_i, s_i, t2_i))
                kind, data = 'overlay', (t1_i, s_i)
        elif j in overlay and kind != 'unexplained':
            out('gp_compare: %s: frames: note: capture %d (raw %d) is named in the '
                'overlay allowance but %s now; retire the name' % (name, j, raws[j], kind))
        counts[kind] += 1
        if kind == 'unexplained':
            unexpl.append(j)
            if not report or len(unexpl) >= REPORT_MAX:
                break
        else:
            ex = exhibited(kind, data)
            seen |= ex
            if ex:
                p = max(p, max(ex))              # the port only moves forward in time
        j += 1
    out('gp_compare: %s: frames: window from capture %d (raw %d); %d classified: %d clean, %d splice, '
        '%d transition, %d band-allowed, %d scan3-allowed, %d overlay-allowed, %d unexplained, %d all-black'
        % (name, start, raws[start], sum(counts.values()), counts['clean'], counts['splice'],
           counts['transition'], counts['band'], counts['scan3'], counts['overlay'],
           counts['unexplained'], black))
    for k, u in enumerate(unexpl):
        cu = cap[u]
        m = nearest(cu, rows)
        box = diff_box(cu, port[m])
        out('gp_compare: %s: frames: %s capture %d (raw %d): nearest port %d, rows %d..%d, x %d..%d (%d px)'
            % ((name, 'FIRST UNEXPLAINED' if k == 0 else 'UNEXPLAINED', u, raws[u], m) + box))
    first = unexpl[0] if unexpl else None
    # Coverage, reported and NOT ratcheted (record §I item 9, a named gap): the port's
    # non-black frames up to the last one the capture exhibits that no classified capture
    # frame exhibits. Order is not claimed either.
    missing = [m for m in range(min(p + 1, len(rows))) if m not in seen and any(h != BLACK_ROW for h in rows[m])]
    out('gp_compare: %s: frames: coverage (reported, not ratcheted): %d non-black port frame(s) up to port %d '
        'not exhibited by any classified capture frame%s'
        % (name, len(missing), p, '' if not missing else ': %s%s' % (missing[:20], ' ...' if len(missing) > 20 else '')))
    if report:
        if first is None:
            out('gp_compare: %s: frames: 0 unexplained through %d' % (name, len(cap) - 1))
        if allow_bad:
            out('gp_compare: %s: frames: FAIL: an allowance named its frame but not its bytes' % name)
        return 0, first, j, p
    rc = max(ratchet(name, 'frames', first, len(cap), min_first, out),
             start_check(name, start, raws, max_start, min_first, out), allow_bad)
    return rc, first, j, p


def nearest(c, rows):
    ch = tc.row_hashes(c)
    best, best_m = -1, 0
    for m, ph in enumerate(rows):
        a, b = tc.row_common(ch, ph)
        if min(a + b, tc.FRAME_H) > best:
            best, best_m = min(a + b, tc.FRAME_H), m
    return best_m


def diff_box(c, q):
    R = tc.ROW
    rs = [r for r in range(tc.FRAME_H) if c[r * R:(r + 1) * R] != q[r * R:(r + 1) * R]]
    if not rs:
        return 0, 0, 0, 0, 0
    xs = [x for r in rs for x in range(tc.FRAME_W) if c[r * R + 3 * x:r * R + 3 * x + 3] != q[r * R + 3 * x:r * R + 3 * x + 3]]
    return rs[0], rs[-1], min(xs), max(xs), len(xs)


def ratchet(name, what, first, end, n, out=print, noun='unexplained'):
    """first unexplained (None: none up to `end`) against the pinned N."""
    if n is None:
        out('gp_compare: %s: %s: FAIL: the ratchet N is not pinned (U4 Task 5)' % (name, what))
        return 1
    if first is None:
        if n > end:
            out('gp_compare: %s: %s: FAIL: N %d > end %d: N is unreachable' % (name, what, n, end))
            return 1
        out('gp_compare: %s: %s: 0 %s through %d; ratchet N %d ok%s'
            % (name, what, noun, end - 1, n, '' if n == end else ' (every item is explained: N = %d is the exact pin)' % end))
        return 0
    if first < n:
        out('gp_compare: %s: %s: FAIL: first %s %d < ratchet N %d' % (name, what, noun, first, n))
        return 1
    out('gp_compare: %s: %s: first %s %d, ratchet N %d ok%s'
        % (name, what, noun, first, n, '' if first == n else ' (improved: raise N)'))
    return 0


DATA_BASE_PORT = 0x80000    # the port's data object base (mem.h DATA_BASE), record §H item 2


def normalised(cap, port, fs, base):
    """Fields that differ between the capture and the port by construction (record
    §H items 1-2), compared after the conversion: t508 (the capture reads it in the
    spin, the port after the releasing tick: port = capture + 1) and ent (a pointer:
    capture linear address - the capture's data base + the port's). Returns
    {field: (compared, differing, first differing f)}; reported, never ratcheted."""
    out = {'t508': [0, 0, None], 'ent': [0, 0, None]}
    for f in fs:
        if f not in cap:
            continue
        c, p = cap[f], port[f]
        want = {'t508': (c['t508'] + 1) & 0xFFFFFFFF}
        if base is not None:
            want['ent'] = ((c['ent'] - base + DATA_BASE_PORT) & 0xFFFFFFFF) if c['ent'] else 0
        for n, w in want.items():
            out[n][0] += 1
            if p[n] != w:
                out[n][1] += 1
                if out[n][2] is None:
                    out[n][2] = f
    return {n: tuple(v) for n, v in out.items()}


def trace_claim(name, cap_lines, port_lines, min_first, out=print, report=False, fields=None, what='trace'):
    """The trace claim over `fields` (gp_session.TRACE_FIELDS by default). With
    what='moves' (gp_session.MOVE_FIELDS, record gameplay-u6 §U6.11) the same
    comparison is labelled 'moves' and the tick and normalised lines are left to
    the trace claim."""
    fields = gs.TRACE_FIELDS if fields is None else fields
    cap = gs.snapshots(cap_lines)
    port = {}
    for l in port_lines:
        r = gs.parse(l)
        if r and r['kind'] == 'T':
            port.setdefault(r['f'], r)
    fs = sorted(port)
    compared = skipped = 0
    first = field = tick_first = None
    for f in fs:
        if f not in cap:
            skipped += 1
            continue
        compared += 1
        if tick_first is None and cap[f]['tick'] != port[f]['tick']:
            tick_first = f
        for n in fields:
            if cap[f][n] != port[f][n]:
                first, field = f, n
                break
        if first is not None:
            break
    if what != 'trace':
        out('gp_compare: %s: %s: %d frames compared%s (f %s..) over %s, %d without a capture snapshot'
            % (name, what, compared, '' if first is None else ' up to the first difference',
               ('%X' % fs[0]) if fs else '-', ' '.join(fields), skipped))
    else:
        out('gp_compare: %s: trace: %d frames compared%s (f %s..), %d without a capture snapshot; first tick '
            'difference %s (reported, not ratcheted)'
            % (name, compared, '' if first is None else ' up to the first difference', ('%X' % fs[0]) if fs else '-', skipped,
               'none' if tick_first is None else 'f=%X' % tick_first))
    if first is not None:
        out('gp_compare: %s: %s: first difference f=%X (%d) in %s: capture %X, port %X'
            % (name, what, first, first, field, cap[first][field], port[first][field]))
    if what != 'trace':
        if compared == 0:
            out('gp_compare: %s: %s: FAIL: no port T record has a capture snapshot (nothing compared)' % (name, what))
            return 1, None
        end = (fs[-1] + 1) if fs else 0
        if report:
            if first is None:
                out('gp_compare: %s: %s: 0 differing through %d' % (name, what, end - 1))
            return 0, first
        return ratchet(name, what, first, end, min_first, out, 'differing'), first
    base = None
    for l in cap_lines:
        r = gs.parse(l)
        if r and r['kind'] == 'B':
            base = r['base']
            break
    norm = normalised(cap, port, fs, base)
    out('gp_compare: %s: trace: normalised (reported, not ratcheted): %s%s'
        % (name, '; '.join('%s %d of %d differ%s' % (n, d, c, '' if d == 0 else ' (first f=%X)' % f0)
                           for n, (c, d, f0) in sorted(norm.items()) if n != 'ent' or base is not None),
           '' if base is not None else '; ent not compared (the capture has no B record)'))
    if compared == 0:
        out('gp_compare: %s: trace: FAIL: no port T record has a capture snapshot (nothing compared)' % name)
        return 1, None
    end = (fs[-1] + 1) if fs else 0
    if report:
        if first is None:
            out('gp_compare: %s: trace: 0 differing through %d' % (name, end - 1))
        return 0, first
    return ratchet(name, 'trace', first, end, min_first, out, 'differing'), first


def capture_identity(name, cap_dir, n_frames, sha, frames, out):
    """The pins (N, F, the window start) are measured on one capture: the pick time-out
    steps in 64-frame units of the absolute frame counter (record §G.24), so a re-capture
    can move the whole timeline. Given --capture-sha256, a capture whose poll.log hashes
    differently (or whose frame count differs) fails, never skips. Returns True when it matches
    or no pin was asked for."""
    if sha is None:
        return True
    if sha == '':
        out('gp_compare: %s: capture: FAIL: the capture identity (poll.log sha256) is not pinned' % name)
        return False
    with open(os.path.join(cap_dir, 'poll.log'), 'rb') as f:
        got = hashlib.sha256(f.read()).hexdigest()
    want_frames = _int_or_none(frames)
    if got != sha or (want_frames is not None and n_frames != want_frames):
        out('gp_compare: %s: capture: FAIL: poll.log sha256 %s (%d frames) != the pinned %s (%s frames): '
            'a re-capture invalidates the pinned N, F and window start; re-measure them from the new '
            'capture (record §G.24) before pinning' % (name, got, n_frames, sha,
                                                       '?' if want_frames is None else want_frames))
        return False
    out('gp_compare: %s: capture: poll.log sha256 %s..%s, %d frames: matches the pin'
        % (name, got[:8], got[-6:], n_frames))
    return True


def _int_or_none(s):
    return None if s in (None, '') else int(s, 0)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--scenario', required=True)
    ap.add_argument('--capture', required=True)
    ap.add_argument('--port', required=True)
    ap.add_argument('--min-first', default=None)
    ap.add_argument('--trace-min-first', default=None)
    ap.add_argument('--max-start', default=None)
    ap.add_argument('--min-reach', default=None,
                    help='the pinned highest port frame any classified capture frame exhibits '
                         '(sub-project A §A1): a port that renders fewer screens fails')
    ap.add_argument('--capture-sha256', default=None,
                    help="the pinned sha256 of the capture's poll.log; with it a different capture fails")
    ap.add_argument('--capture-frames', default=None,
                    help="the pinned number of frame_*.raw.gz in the capture (given with --capture-sha256)")
    ap.add_argument('--moves-min-first', default=None,
                    help='the moves claim over gp_session.MOVE_FIELDS (record gameplay-u6 §U6.11); '
                         'report mode runs it whenever the capture records those fields')
    ap.add_argument('--report', action='store_true')
    a = ap.parse_args()
    name = a.scenario
    if a.report:
        print('gp_compare: %s: report only: no ratchet applied, exit 0' % name)
    if not os.path.isdir(a.capture):
        print('gp_compare: no capture at %s (skipped)' % a.capture)
        return 0
    if not os.path.isfile(os.path.join(a.port, 'trace.txt')):
        print('gp_compare: no port dump at %s (trace.txt missing)' % a.port)
        return 0 if a.report else 1
    if not os.path.isfile(os.path.join(a.capture, 'poll.log')):
        print('gp_compare: %s: capture %s has no poll.log' % (name, a.capture))
        return 0 if a.report else 1
    cpaths = _paths(a.capture, 'frame_%05d.raw.gz')
    ppaths = _paths(a.port, 'frame_%05d.ipx')
    if not a.report and not capture_identity(name, a.capture, len(cpaths), a.capture_sha256,
                                             a.capture_frames, print):
        return 1
    if not cpaths or not ppaths:
        print('gp_compare: %s: no frames (capture %d, port %d)' % (name, len(cpaths), len(ppaths)))
        return 0 if a.report else 1
    cap = Lazy(cpaths, load_capture_frame)
    port = Lazy(ppaths, load_port_frame)
    rows = [tc.row_hashes(port[m]) for m in range(len(port))]
    raws = tc.raw_map(a.capture) or list(range(len(cap)))
    n_frames = None if a.report else _int_or_none(a.min_first)
    n_trace = None if a.report else _int_or_none(a.trace_min_first)
    rc1, _, _, reach = frame_claim(name, cap, raws, port, rows, n_frames, a.report, print,
                                   None if a.report else _int_or_none(a.max_start),
                                   BANDS_ALLOWED.get(name, ()), SCAN3_ALLOWED.get(name, ()),
                                   OVERLAY_ALLOWED.get(name, ()))
    rc4 = 0 if a.report else reach_check(name, reach, _int_or_none(a.min_reach), print)
    with open(os.path.join(a.capture, 'poll.log')) as f:
        cl = f.read().splitlines()
    with open(os.path.join(a.port, 'trace.txt')) as f:
        pl = f.read().splitlines()
    rc2, _ = trace_claim(name, cl, pl, n_trace, print, a.report)
    rc3 = 0
    snaps = gs.snapshots(cl)
    has_moves = bool(snaps) and all(n in next(iter(snaps.values())) for n in gs.MOVE_FIELDS)
    if a.moves_min_first is not None and not has_moves:
        print('gp_compare: %s: moves: FAIL: the capture records no move fields (nothing compared)' % name)
        rc3 = 1
    elif a.moves_min_first is not None or (a.report and has_moves):
        rc3, _ = trace_claim(name, cl, pl, None if a.report else _int_or_none(a.moves_min_first),
                             print, a.report, gs.MOVE_FIELDS, 'moves')
    return 0 if a.report else (1 if rc1 or rc2 or rc3 or rc4 else 0)


if __name__ == '__main__':
    sys.exit(main())
