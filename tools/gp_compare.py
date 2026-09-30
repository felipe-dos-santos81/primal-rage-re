#!/usr/bin/env python3
"""The gameplay oracle (spec 2026-09-30-gameplay-ground-truth-design.md §4.3,
record §G.13..). Two claims, each a ratchet on its first unexplained item:

  frames  the capture's distinct frames (data/k11-captures/<scenario>/
          frame_%05d.raw.gz, gp_capture) against the port's displayed frames
          (<port>/frame_%05d.ipx, test_gp_replay) with title_compare.explain's
          model: clean, a byte splice of adjacent port frames (the 70.09 Hz
          capture against the 60.05 Hz game), or one transition row. All-black
          capture frames are the documented capture artefact and are skipped.
          The first unexplained capture frame must be >= --min-first N.
  trace   the capture's S records (poll.log) against the port's T records
          (trace.txt) by the frame counter f, over gp_session.TRACE_FIELDS;
          the first differing f must be >= --trace-min-first F. tick is
          reported apart (host-timed, spec §7 Q6).

The search for a frame's explanation tries the port frames [p - BACK, p +
AHEAD) around the last explained index p first, then the whole dump, so the
result is the unwindowed classification (the window is a search order, a
harness value). Enforced runs stop at the first unexplained frame; --report
goes on (up to REPORT_MAX) and always exits 0. An absent capture skips (exit
0); a present capture with an unpinned N fails. Stdlib only; title_compare and
gp_session are read-only here."""
import argparse
import collections
import gzip
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gp_session as gs
import title_compare as tc

BACK, AHEAD = 2, 64          # harness: the search order (spec §4.3), not a game value
REPORT_MAX = 5
IPX_BYTES = 64000 + 768


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


def frame_claim(name, cap, raws, port, rows, min_first, report, out=print):
    """Returns (rc, first_unexplained or None, next_after_last_classified)."""
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
        return 1, None, 0
    p, counts, black, unexpl = 0, collections.Counter(), 0, []
    j = start
    while j < len(cap):
        c = cap[j]
        if not any(c):
            black += 1
            j += 1
            continue
        kind, data = classify(c, port, rows, p)
        counts[kind] += 1
        if kind == 'unexplained':
            unexpl.append(j)
            if not report or len(unexpl) >= REPORT_MAX:
                break
        else:
            ex = exhibited(kind, data)
            if ex:
                p = max(p, max(ex))              # the port only moves forward in time
        j += 1
    out('gp_compare: %s: frames: window from capture %d (raw %d); %d classified: %d clean, %d splice, '
        '%d transition, %d unexplained, %d all-black'
        % (name, start, raws[start], sum(counts.values()), counts['clean'], counts['splice'],
           counts['transition'], counts['unexplained'], black))
    for k, u in enumerate(unexpl):
        cu = cap[u]
        m = nearest(cu, rows)
        box = diff_box(cu, port[m])
        out('gp_compare: %s: frames: %s capture %d (raw %d): nearest port %d, rows %d..%d, x %d..%d (%d px)'
            % ((name, 'FIRST UNEXPLAINED' if k == 0 else 'UNEXPLAINED', u, raws[u], m) + box))
    first = unexpl[0] if unexpl else None
    return ratchet(name, 'frames', first, len(cap), min_first, out), first, j


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


def ratchet(name, what, first, end, n, out=print):
    """first unexplained (None: none up to `end`) against the pinned N."""
    if n is None:
        out('gp_compare: %s: %s: FAIL: the ratchet N is not pinned (U4 Task 5)' % (name, what))
        return 1
    if first is None:
        if n > end:
            out('gp_compare: %s: %s: FAIL: N %d > end %d: N is unreachable' % (name, what, n, end))
            return 1
        out('gp_compare: %s: %s: 0 unexplained through %d; ratchet N %d ok' % (name, what, end - 1, n))
        return 0
    if first < n:
        out('gp_compare: %s: %s: FAIL: first unexplained %d < ratchet N %d' % (name, what, first, n))
        return 1
    out('gp_compare: %s: %s: first unexplained %d, ratchet N %d ok%s'
        % (name, what, first, n, '' if first == n else ' (improved: raise N)'))
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


def trace_claim(name, cap_lines, port_lines, min_first, out=print):
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
        for n in gs.TRACE_FIELDS:
            if cap[f][n] != port[f][n]:
                first, field = f, n
                break
        if first is not None:
            break
    out('gp_compare: %s: trace: %d frames compared up to the first difference (f %s..), %d without a capture snapshot; first tick '
        'difference %s (reported, not ratcheted)'
        % (name, compared, fs and '%X' % fs[0], skipped, 'none' if tick_first is None else 'f=%X' % tick_first))
    if first is not None:
        out('gp_compare: %s: trace: first difference f=%X (%d) in %s: capture %X, port %X'
            % (name, first, first, field, cap[first][field], port[first][field]))
    base = None
    for l in cap_lines:
        r = gs.parse(l)
        if r and r['kind'] == 'B':
            base = r['base']
            break
    norm = normalised(cap, port, fs, base)
    out('gp_compare: %s: trace: normalised (reported, not ratcheted): %s'
        % (name, '; '.join('%s %d of %d differ%s' % (n, d, c, '' if d == 0 else ' (first f=%X)' % f0)
                           for n, (c, d, f0) in sorted(norm.items()))))
    if compared == 0:
        out('gp_compare: %s: trace: FAIL: no port T record has a capture snapshot (nothing compared)' % name)
        return 1, None
    end = (fs[-1] + 1) if fs else 0
    return ratchet(name, 'trace', first, end, min_first, out), first
