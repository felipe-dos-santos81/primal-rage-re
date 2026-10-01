#!/usr/bin/env python3
"""The gameplay oracle (spec 2026-09-30-gameplay-ground-truth-design.md §4.3,
record §G.13..). Two claims, each a ratchet on its first unexplained item:

  frames  the capture's distinct frames (data/k11-captures/<scenario>/
          frame_%05d.raw.gz, gp_capture) against the port's displayed frames
          (<port>/frame_%05d.ipx, test_gp_replay) with title_compare.explain's
          model: clean, a byte splice of adjacent port frames (the 70.09 Hz
          capture against the 60.05 Hz game), or one transition row. All-black
          capture frames are the documented capture artefact and are skipped.
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


def frame_claim(name, cap, raws, port, rows, min_first, report, out=print, max_start=None):
    """Returns (rc, first_unexplained or None, next_after_last_classified). In report
    mode no ratchet verdict is printed and no pin is checked (rc stays 1 only for
    "window empty"; main() turns every report-mode rc into 0)."""
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
    p, counts, black, unexpl, seen = 0, collections.Counter(), 0, [], set()
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
            seen |= ex
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
        return 0, first, j
    rc = max(ratchet(name, 'frames', first, len(cap), min_first, out), start_check(name, start, raws, max_start, min_first, out))
    return rc, first, j


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


def trace_claim(name, cap_lines, port_lines, min_first, out=print, report=False):
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
    out('gp_compare: %s: trace: %d frames compared%s (f %s..), %d without a capture snapshot; first tick '
        'difference %s (reported, not ratcheted)'
        % (name, compared, '' if first is None else ' up to the first difference', ('%X' % fs[0]) if fs else '-', skipped,
           'none' if tick_first is None else 'f=%X' % tick_first))
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
    if not cpaths or not ppaths:
        print('gp_compare: %s: no frames (capture %d, port %d)' % (name, len(cpaths), len(ppaths)))
        return 0 if a.report else 1
    cap = Lazy(cpaths, load_capture_frame)
    port = Lazy(ppaths, load_port_frame)
    rows = [tc.row_hashes(port[m]) for m in range(len(port))]
    raws = tc.raw_map(a.capture) or list(range(len(cap)))
    n_frames = None if a.report else _int_or_none(a.min_first)
    n_trace = None if a.report else _int_or_none(a.trace_min_first)
    rc1, _, _ = frame_claim(name, cap, raws, port, rows, n_frames, a.report, print,
                            None if a.report else _int_or_none(a.max_start))
    with open(os.path.join(a.capture, 'poll.log')) as f:
        cl = f.read().splitlines()
    with open(os.path.join(a.port, 'trace.txt')) as f:
        pl = f.read().splitlines()
    rc2, _ = trace_claim(name, cl, pl, n_trace, print, a.report)
    return 0 if a.report else (1 if rc1 or rc2 else 0)


if __name__ == '__main__':
    sys.exit(main())
