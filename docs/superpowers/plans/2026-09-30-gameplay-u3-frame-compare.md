# Gameplay U3 — Frame-by-Frame Comparison and Ratchet Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `tools/gp_compare.py`, the gameplay oracle: a frame claim (title_compare's clean / byte-splice / transition-row model, the demo-fight answer to the 70.09 Hz capture against the 60.05 Hz game) and a trace claim (the capture's per-frame `S` records against the port's `T` records by the frame counter), each a ratchet on its first unexplained item, wired into `make verify` through `make gp-oracle`.

**Architecture:** The capture's gzip RGB frames (U1) and the port's `.ipx` frames (U2, expanded through their DAC) are read lazily with an LRU cache; row hashes of every port frame are computed once. Each capture frame is explained by `title_compare.explain` on the port frames `[p − 2, p + 64)` around the last explained index `p`, then on the whole dump if that fails, so the classification equals the unwindowed one. The trace claim walks the port's `T` lines and compares `gp_session.TRACE_FIELDS` with the capture's `S` record of the same `f`. The pinned values live in the Makefile, like `DEMO_FIGHT_MIN_FIRST`; U4 pins them.

**Tech Stack:** Python 3 stdlib (`gzip`, `collections`, `unittest`), `tools/title_compare.py` and `tools/gp_session.py` read-only, `make`.

**Spec:** `docs/superpowers/specs/2026-09-30-gameplay-ground-truth-design.md` §4.3 (this unit), §4.1–§4.2 (the formats), §5.

**Derivation record:** `docs/superpowers/plans/2026-09-30-gameplay-ground-truth-derivations.md`, sections `§G.13..§G.16`.

**Where to run.** Main checkout, branch `gameplay-ground-truth`, after U1 and U2. `S=/tmp/gameplay-u3`. Every command assumes `cd /Users/felipe.dos.santos/code/mine/primal-rage-reverse`.

**Planner's check.** The code blocks below were run as written in a scratch copy of `tools/`: `Ran 10 tests … OK`, and an end-to-end self-comparison (U2's smoke dump expanded into a fake capture, its `trace.txt` rewritten as `S` records) printed `139 clean … 7 all-black`, `0 unexplained through 145`, `601 frames compared`, `rc 0`, in 0.2 s.

---

## Global Constraints

- Spec §5: "Raw wins; never a fitted constant; a value that cannot be pinned is a named gap with its evidence. Harness values (…, the `[p − 2, p + 64)` search order, …) are named as harness values with their source, never presented as game values."
- Spec §5: "`make verify` is the gate after every task; the oracle lines equal the U1 Task 0 baseline; the enforced front-end oracle, the demo-fight and attract cycle-2 ratchets and the K11 oracles stay green."
- Spec §5: "Tests: Python `unittest` under `tools/tests` (stdlib) … every assertion can fail and each new test is shown failing under a named mutation."
- Spec §4.3: "A capture frame is explained by `title_compare.explain` exactly … All-black capture frames are the documented capture artefact and are skipped." No new tolerance, mask, crop or allowance; `title_compare.py` is not edited.
- Spec §4.3: "a missing capture skips (exit 0), like the K11 and front-end oracles (enforced without `PR_ORACLE_REQUIRED`; an unpinned N with a present capture fails)."
- AGENTS.md: "The front-end oracle's claim is narrow … Do not read a green oracle as "the frame is correct"." The gameplay oracle's claim is stated the same way in the record (Task 5).
- AGENTS.md: "Commit style: `<area>: <what changed>`. **Never `git add -A`**." Trailer `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`.

## Review Focus

1. **The window changing the answer.** A frame explained only by a port frame far from `p` must still be explained. `test_a_match_beyond_the_window_is_found` (Task 2), mutation: drop the full-dump fallback.
2. **A vacuous start.** The window must begin where the capture first shows the port's first frame, not at capture frame 0 (which is the title before the Enter). `test_window_start_is_the_port_first_frame`.
3. **A ratchet that cannot fail.** An unpinned `N` with a capture present, and a first unexplained frame below `N`, must each exit 1. `test_unpinned_n_fails`, `test_first_unexplained_against_the_ratchet`.
4. **The host-timed tick ratcheted.** `tick` must be reported, never fail the trace claim (spec §7 Q6). `test_tick_is_reported_not_ratcheted`.
5. **A frame the capture never snapshotted read as equal.** Missing `S` records are skipped and counted, never compared against a default. `test_first_difference_skips_unsnapshotted_frames`.

---

### Task 1: Loaders: gzip capture frames, `.ipx` port frames, the lazy sequence

**Files:**
- Create: `tools/gp_compare.py`
- Create: `tools/tests/test_gp_compare.py`
- Modify: record §G.13

**Interfaces:**
- Consumes: `title_compare.FRAME_BYTES`, `row_hashes`; the U1 capture layout (`frame_%05d.raw.gz`, `window.txt`); the U2 dump layout (`frame_%05d.ipx`)
- Produces: `expand_ipx(bytes) -> bytes`, `_paths(dir, pattern) -> list[str]`, `load_capture_frame(path) -> bytes`, `load_port_frame(path) -> bytes`, `class Lazy(paths, loader, cache=160)` (`len`, `[i]`), `class View(seq, lo, hi)`; the test fixtures `ipx(k)`, `rgb(k)`, `Dirs` (with `write(port_ks, cap_frames)` and `run_claim(n, report=False)`)

- [ ] **Step 1: Write the failing tests**

```python
# tools/tests/test_gp_compare.py
import gzip, io, os, shutil, sys, tempfile, unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import gp_compare as gc
import gp_session as gs
import title_compare as tc

DAC = bytes(i for i in range(256) for _ in range(3))   # grey: index i -> (i, i, i)


def ipx(k):
    """A port frame whose every index is k, except row k (index k+1): distinct rows."""
    idx = bytearray([k]) * 64000
    idx[320 * k:320 * (k + 1)] = bytes([k + 1]) * 320
    return bytes(idx) + DAC


def rgb(k):
    return gc.expand_ipx(ipx(k))


class Dirs(unittest.TestCase):
    def setUp(self):
        self.d = tempfile.mkdtemp(prefix='gpcmp-test-')
        self.addCleanup(shutil.rmtree, self.d, True)
        self.cap = os.path.join(self.d, 'cap')
        self.port = os.path.join(self.d, 'port')
        os.makedirs(self.cap)
        os.makedirs(self.port)

    def write(self, port_ks, cap_frames):
        for i, k in enumerate(port_ks):
            with open(os.path.join(self.port, 'frame_%05d.ipx' % i), 'wb') as f:
                f.write(ipx(k))
        for j, c in enumerate(cap_frames):
            with gzip.open(os.path.join(self.cap, 'frame_%05d.raw.gz' % j), 'wb') as f:
                f.write(c)
        with open(os.path.join(self.cap, 'window.txt'), 'w') as f:
            f.write(''.join('%05d %d\n' % (j, 100 + j) for j in range(len(cap_frames))))

    def run_claim(self, n, report=False):
        cap = gc.Lazy(gc._paths(self.cap, 'frame_%05d.raw.gz'), gc.load_capture_frame)
        port = gc.Lazy(gc._paths(self.port, 'frame_%05d.ipx'), gc.load_port_frame)
        rows = [tc.row_hashes(port[m]) for m in range(len(port))]
        out = []
        rc, first, _ = gc.frame_claim('t', cap, tc.raw_map(self.cap), port, rows, n, report, out.append)
        return rc, first, out


class TestExpand(unittest.TestCase):
    def test_expand_is_dac_of_index(self):
        data = bytearray(ipx(3))
        data[64000 + 3 * 7:64000 + 3 * 7 + 3] = b'\x01\x02\x03'
        data[0] = 7
        out = gc.expand_ipx(bytes(data))
        self.assertEqual(out[0:3], b'\x01\x02\x03')
        self.assertEqual(out[3:6], bytes([3, 3, 3]))
        self.assertEqual(len(out), tc.FRAME_BYTES)

    def test_expand_rejects_a_short_frame(self):
        with self.assertRaises(ValueError):
            gc.expand_ipx(b'\x00' * 100)


if __name__ == '__main__':
    unittest.main()
```

- [ ] **Step 2: Run to verify it fails**

Run: `python3 -m unittest tools.tests.test_gp_compare -v`
Expected: `ModuleNotFoundError: No module named 'gp_compare'`.

- [ ] **Step 3: Implement**

```python
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
```

- [ ] **Step 4: Run the tests**

Run: `python3 -m unittest tools.tests.test_gp_compare -v`
Expected: `Ran 2 tests … OK`.

- [ ] **Step 5: Mutation proof**

In `expand_ipx` use `dac[3 * i + 0]` for every channel: expected `FAIL: test_expand_is_dac_of_index`. Restore; `OK`. Record in §G.13 with the U2 Task 2 Step 4 cross-check (the same `translate` expansion).

- [ ] **Step 6: Commit**

```bash
git add tools/gp_compare.py tools/tests/test_gp_compare.py docs/superpowers/plans/2026-09-30-gameplay-ground-truth-derivations.md
git commit -m "$(cat <<'EOF'
tools: gp_compare loaders for gzip capture and .ipx port frames (record §G.13)

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 2: The frame claim and its ratchet

**Files:**
- Modify: `tools/gp_compare.py` (append), `tools/tests/test_gp_compare.py` (append `TestFrames` before the `__main__` guard)
- Modify: record §G.14

**Interfaces:**
- Consumes: Task 1; `title_compare.explain`, `row_hashes`, `row_common`, `FRAME_H`, `FRAME_W`, `ROW`, `FRAME_BYTES`
- Produces: `shift`, `exhibited(kind, data) -> set`, `classify(c, port, rows, p) -> (kind, data)`, `frame_claim(name, cap, raws, port, rows, min_first, report, out=print) -> (rc, first|None, next_j)`, `nearest`, `diff_box`, `ratchet(name, what, first, end, n, out=print) -> rc`; constants `BACK = 2`, `AHEAD = 64`, `REPORT_MAX = 5`

- [ ] **Step 1: Write the failing tests**

```python
class TestFrames(Dirs):
    def test_clean_and_splice_are_explained(self):
        b = 5000
        self.write([1, 2, 3], [rgb(1), rgb(1)[:b] + rgb(2)[b:], rgb(2), rgb(3)])
        rc, first, out = self.run_claim(4)
        self.assertEqual((rc, first), (0, None), out)

    def test_first_unexplained_against_the_ratchet(self):
        bad = bytes([9]) * tc.FRAME_BYTES
        self.write([1, 2, 3], [rgb(1), rgb(2), bad, rgb(3)])
        self.assertEqual(self.run_claim(2)[:2], (0, 2))
        rc, first, out = self.run_claim(3)
        self.assertEqual((rc, first), (1, 2))
        self.assertTrue(any('FAIL: first unexplained 2 < ratchet N 3' in l for l in out), out)

    def test_black_frames_are_skipped(self):
        self.write([1, 2], [rgb(1), bytes(tc.FRAME_BYTES), rgb(2)])
        self.assertEqual(self.run_claim(3)[:2], (0, None))

    def test_window_start_is_the_port_first_frame(self):
        self.write([1, 2], [rgb(5), rgb(6), rgb(1), rgb(2)])
        rc, first, out = self.run_claim(4)
        self.assertEqual((rc, first), (0, None), out)
        self.assertTrue(out[0].startswith('gp_compare: t: frames: window from capture 2 (raw 102)'), out)

    def test_a_match_beyond_the_window_is_found(self):
        ks = list(range(1, 80))
        self.write(ks, [rgb(1), rgb(79)])
        self.assertEqual(self.run_claim(2)[:2], (0, None))

    def test_unpinned_n_fails(self):
        self.write([1], [rgb(1)])
        self.assertEqual(self.run_claim(None)[0], 1)
```

- [ ] **Step 2: Run to verify it fails**

Run: `python3 -m unittest tools.tests.test_gp_compare -v`
Expected: `AttributeError: module 'gp_compare' has no attribute 'frame_claim'` (6 errors).

- [ ] **Step 3: Implement**

```python
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
```

Notes: `exhibited` is `k11_compare.exhibited`'s rule (a splice with `hi > 0` shows `N`, with `lo < FRAME_BYTES` shows `N + 1`). The start search runs `explain` on port frames 0 and 1 only, which is exact for "exhibits port frame 0" (a clean 0, a splice or transition with `N = 0`) and keeps the pre-Enter title frames from each running a full-dump search. `p` only moves forward (`max`), because the port's frames are in time order; a full-dump match on an earlier identical screen (a menu drawn twice) does not pull the window back.

- [ ] **Step 4: Run the tests**

Run: `python3 -m unittest tools.tests.test_gp_compare -v`
Expected: `Ran 8 tests … OK`.

- [ ] **Step 5: Mutation proofs**

(a) In `classify` replace the last line with `return 'unexplained', None` (no full-dump fallback): expected `FAIL: test_a_match_beyond_the_window_is_found`. (b) Remove the all-black `continue` branch in the main loop of `frame_claim`: expected `FAIL: test_black_frames_are_skipped`. (c) In `ratchet` change `first < n` to `first < n - 1`: expected `FAIL: test_first_unexplained_against_the_ratchet`. Restore each; `OK`. Record in §G.14.

- [ ] **Step 6: Commit**

```bash
git add tools/gp_compare.py tools/tests/test_gp_compare.py docs/superpowers/plans/2026-09-30-gameplay-ground-truth-derivations.md
git commit -m "$(cat <<'EOF'
tools: gp_compare frame claim (explain model, windowed search, ratchet) (record §G.14)

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 3: The trace claim

**Files:**
- Modify: `tools/gp_compare.py` (append), `tools/tests/test_gp_compare.py` (append before the `__main__` guard)
- Modify: record §G.15

**Interfaces:**
- Consumes: `gp_session.snapshots`, `parse`, `format_s`, `SNAP_FIELDS`, `TRACE_FIELDS`; Task 2's `ratchet`
- Produces: `trace_claim(name, cap_lines, port_lines, min_first, out=print) -> (rc, first|None)`

- [ ] **Step 1: Write the failing tests**

```python
def _t(f, **kw):
    vals = {n: 0 for n, _, _ in gs.SNAP_FIELDS}
    vals.update(f=f, **kw)
    return 'T ' + gs.format_s(0, vals, 0, 0, 0)[2:]


def _S(f, **kw):
    vals = {n: 0 for n, _, _ in gs.SNAP_FIELDS}
    vals.update(f=f, **kw)
    return gs.format_s(0, vals, 0, 0, 0)


class TestTrace(unittest.TestCase):
    def test_first_difference_skips_unsnapshotted_frames(self):
        port = [_t(f, rng=f) for f in range(10, 20)]
        cap = [_S(f, rng=f if f != 17 else 0) for f in range(10, 20) if f != 12]
        out = []
        rc, first = gc.trace_claim('t', cap, port, 17, out.append)
        self.assertEqual((rc, first), (0, 17), out)
        self.assertIn('1 without a capture snapshot', out[0])
        rc, _ = gc.trace_claim('t', cap, port, 18, out.append)
        self.assertEqual(rc, 1)

    def test_tick_is_reported_not_ratcheted(self):
        port = [_t(f, tick=f) for f in range(3)]
        cap = [_S(f, tick=0) for f in range(3)]
        out = []
        rc, first = gc.trace_claim('t', cap, port, 3, out.append)
        self.assertEqual((rc, first), (0, None), out)
        self.assertIn('first tick difference f=1', out[0])
```

(`_t` builds a port `T` line from `format_s`'s field text, the same names and widths U2 Task 2 writes.)

- [ ] **Step 2: Run to verify it fails**

Run: `python3 -m unittest tools.tests.test_gp_compare -v`
Expected: `AttributeError: … 'trace_claim'` (2 errors).

- [ ] **Step 3: Implement**

```python
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
    out('gp_compare: %s: trace: %d frames compared (f %s..), %d without a capture snapshot; first tick '
        'difference %s (reported, not ratcheted)'
        % (name, compared, fs and '%X' % fs[0], skipped, 'none' if tick_first is None else 'f=%X' % tick_first))
    if first is not None:
        out('gp_compare: %s: trace: first difference f=%X (%d) in %s: capture %X, port %X'
            % (name, first, first, field, cap[first][field], port[first][field]))
    end = (fs[-1] + 1) if fs else 0
    return ratchet(name, 'trace', first, end, min_first, out), first
```

- [ ] **Step 4: Run the tests**

Run: `python3 -m unittest tools.tests.test_gp_compare -v`
Expected: `Ran 10 tests … OK`.

- [ ] **Step 5: Mutation proof**

Compare `tick` with the trace fields (`for n in gs.TRACE_FIELDS + ('tick',):`): expected `FAIL: test_tick_is_reported_not_ratcheted`. Restore; `OK`. Record in §G.15.

- [ ] **Step 6: Commit**

```bash
git add tools/gp_compare.py tools/tests/test_gp_compare.py docs/superpowers/plans/2026-09-30-gameplay-ground-truth-derivations.md
git commit -m "$(cat <<'EOF'
tools: gp_compare trace claim by frame counter, tick reported apart (record §G.15)

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 4: The CLI, `make gp-oracle`/`gp-report`, `make verify`

**Files:**
- Modify: `tools/gp_compare.py` (append the CLI)
- Modify: `Makefile` (ratchet variables and two targets after `gp-replay`; `.PHONY`; `verify`)
- Modify: record §G.16

**Interfaces:**
- Consumes: U2's `make gp-replay scenario=…` and `$(GP_DUMP)`
- Produces: `gp_compare.py --scenario N --capture DIR --port DIR [--min-first N] [--trace-min-first F] [--report]`; `make gp-oracle` (in `make verify`); `make gp-report scenario=…`; Makefile variables `GP_IDLE_LOSS_MIN_FIRST`, `GP_IDLE_LOSS_TRACE_MIN_FIRST` (empty until U4 Task 5)

- [ ] **Step 1: The CLI**

```python
def _int_or_none(s):
    return None if s in (None, '') else int(s, 0)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--scenario', required=True)
    ap.add_argument('--capture', required=True)
    ap.add_argument('--port', required=True)
    ap.add_argument('--min-first', default=None)
    ap.add_argument('--trace-min-first', default=None)
    ap.add_argument('--report', action='store_true')
    a = ap.parse_args()
    name = a.scenario
    if not os.path.isdir(a.capture):
        print('gp_compare: no capture at %s (skipped)' % a.capture)
        return 0
    if not os.path.isfile(os.path.join(a.port, 'trace.txt')):
        print('gp_compare: no port dump at %s (trace.txt missing)' % a.port)
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
    rc1, _, _ = frame_claim(name, cap, raws, port, rows, n_frames if not a.report else 0, a.report)
    with open(os.path.join(a.capture, 'poll.log')) as f:
        cl = f.read().splitlines()
    with open(os.path.join(a.port, 'trace.txt')) as f:
        pl = f.read().splitlines()
    rc2, _ = trace_claim(name, cl, pl, n_trace if not a.report else 0)
    return 0 if a.report else (1 if rc1 or rc2 else 0)


if __name__ == '__main__':
    sys.exit(main())
```

- [ ] **Step 2: Self-comparison through the CLI (scratch, not committed)**

Build a fake capture from U2's smoke dump (`/tmp/gameplay-u2/smoke`; rerun U2 Task 2 Step 3 if it is gone) and compare the dump with itself:

```bash
S=/tmp/gameplay-u3; mkdir -p $S
python3 - <<'EOF'
import gzip, os, sys
sys.path.insert(0, 'tools')
import gp_compare as gc
port, cap = '/tmp/gameplay-u2/smoke', '/tmp/gameplay-u3/gp-fake'
os.makedirs(cap, exist_ok=True)
ps = gc._paths(port, 'frame_%05d.ipx')
for i, p in enumerate(ps):
    with gzip.open(os.path.join(cap, 'frame_%05d.raw.gz' % i), 'wb', compresslevel=1) as f:
        f.write(gc.load_port_frame(p))
open(os.path.join(cap, 'window.txt'), 'w').write(''.join('%05d %d\n' % (i, i) for i in range(len(ps))))
with open(os.path.join(port, 'trace.txt')) as fi, open(os.path.join(cap, 'poll.log'), 'w') as fo:
    fo.writelines('S ms=0 ' + l[2:] for l in fi)
print(len(ps))
EOF
python3 tools/gp_compare.py --scenario gp-fake --capture $S/gp-fake --port /tmp/gameplay-u2/smoke --min-first 145 --trace-min-first 901; echo "rc=$?"
python3 tools/gp_compare.py --scenario gp-fake --capture $S/gp-fake --port /tmp/gameplay-u2/smoke; echo "rc=$?"
python3 tools/gp_compare.py --scenario gp-fake --capture $S/nothing --port /tmp/gameplay-u2/smoke; echo "rc=$?"
```

Expected: `145`; the first run `0 unexplained through 144; ratchet N 145 ok`, `601 frames compared (f 12C..)`, `0 unexplained through 900; ratchet N 901 ok`, `rc=0`; the second `FAIL: the ratchet N is not pinned` twice, `rc=1`; the third `no capture at … (skipped)`, `rc=0`. Record the lines in §G.16.

- [ ] **Step 3: The Makefile**

After `gp-replay`:

```make
# Gameplay oracle (spec 2026-09-30-gameplay-ground-truth-design.md §4.3):
# the capture against the port's replay, a frame ratchet (title_compare's
# explain model) and a trace ratchet (S against T by the frame counter).
# Enforced like the K11 oracle: skips without the capture, fails on a broken
# claim or an unpinned N once the capture exists. U4 Task 5 pins both values
# from the measured first unexplained capture frame / first differing f.
GP_IDLE_LOSS_MIN_FIRST =
GP_IDLE_LOSS_TRACE_MIN_FIRST =
gp-oracle: build ## Gameplay oracle: gp-idle-loss frame and trace ratchets (skips without data/k11-captures/gp-idle-loss)
	@echo "== gameplay oracle: gp-idle-loss (frame and trace ratchets) =="
	@$(MAKE) --no-print-directory gp-replay scenario=gp-idle-loss
	@$(PYTHON) tools/gp_compare.py --scenario gp-idle-loss --capture $(K11_CAPTURES)/gp-idle-loss \
		--port $(GP_DUMP)/gp-idle-loss --min-first "$(GP_IDLE_LOSS_MIN_FIRST)" \
		--trace-min-first "$(GP_IDLE_LOSS_TRACE_MIN_FIRST)"

gp-report: build ## Report-only gameplay comparison (scenario=gp-…)
	@$(MAKE) --no-print-directory gp-replay scenario=$(scenario)
	@$(PYTHON) tools/gp_compare.py --report --scenario $(scenario) --capture $(K11_CAPTURES)/$(scenario) --port $(GP_DUMP)/$(scenario)
```

Add both to `.PHONY`. In `verify`, after the `k11-oracle` line:

```make
	@echo "== gameplay oracle (frame and trace ratchets; skips without its capture) =="
	@$(MAKE) --no-print-directory gp-oracle
```

and append `tools.tests.test_gp_compare` to the tool-test `unittest` line.

- [ ] **Step 4: The report on U1's gp-pads**

```bash
make gp-report scenario=gp-pads 2>&1 | grep gp_compare
```

Expected: a `frames:` line with the classification counts and, unless the port matches the whole MAIN MENU pad walk, a `FIRST UNEXPLAINED` line with its box; a `trace:` line. Report only; record the lines verbatim in §G.16 as the first port-against-capture view of play input (it is evidence, not a gate).

- [ ] **Step 5: The gate**

```bash
S=/tmp/gameplay-u3
make verify > "$S/t4_verify.txt" 2>&1; echo "verify-exit=$?"
grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|k11_compare)' "$S/t4_verify.txt" | diff - /tmp/gameplay-u1/or_base.txt && echo ORACLES-EQUAL
grep -E 'gp-replay|gp_compare' "$S/t4_verify.txt"
```

Expected: `verify-exit=0`; `ORACLES-EQUAL`; `gp-replay: no capture at data/k11-captures/gp-idle-loss` and `gp_compare: no capture at data/k11-captures/gp-idle-loss (skipped)` (U4 has not captured yet). Record in §G.16.

- [ ] **Step 6: Commit**

```bash
git add tools/gp_compare.py Makefile docs/superpowers/plans/2026-09-30-gameplay-ground-truth-derivations.md
git commit -m "$(cat <<'EOF'
build: make gp-oracle (frame and trace ratchets) in make verify, gp-report (record §G.16)

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 5: U3 closure

**Files:**
- Modify: record (append to §G.16 a "claim" paragraph); `docs/PROGRESS.md`; `AGENTS.md` (the `make` list and the oracle paragraph)

- [ ] **Step 1: The claim, stated narrowly**

Append to §G.16: "The frame claim proves that no content-bearing capture frame from the window start up to the first unexplained one is unexplained by the port's frames under title_compare's model; it does not prove the frames after it, nor that the port draws what the capture draws where both are black. The trace claim proves the listed fields equal at every snapshotted `f` below its first difference; unsnapshotted frames (spec §3.7) are not compared, and `tick` is not claimed."

- [ ] **Step 2: AGENTS.md and PROGRESS**

In `AGENTS.md`'s command block add `make gp-capture scenario=gp-idle-loss` and `make gp-oracle` / `make gp-report scenario=…` with one-line comments, and in the evidence paragraph one sentence: "The gameplay oracle (`make gp-oracle`, in `make verify`) ratchets the first unexplained capture frame and the first differing trace frame of `data/k11-captures/gp-idle-loss`; its N values and provenance are in the Makefile; it skips without the capture." Append a `docs/PROGRESS.md` paragraph "Gameplay U3 …".

- [ ] **Step 3: Commit**

```bash
git add AGENTS.md docs/PROGRESS.md docs/superpowers/plans/2026-09-30-gameplay-ground-truth-derivations.md
git commit -m "$(cat <<'EOF'
docs: gameplay U3 closure: the gameplay oracle and its narrow claim (record §G.16)

Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>
EOF
)"
```

---

## Self-Review

- Spec §4.3 coverage: the explain model and the 70/60 Hz splice (Task 2, reusing `title_compare.explain`), the windowed search with a full fallback (Task 2), all-black skipping (Task 2), the frame ratchet (Task 2), the trace ratchet with tick apart (Task 3), `make gp-oracle` in `make verify` skipping without the capture and failing on an unpinned N (Task 4), the narrow claim (Task 5). The values are pinned by U4.
- Names: `expand_ipx`, `_paths`, `load_capture_frame`, `load_port_frame`, `Lazy`, `View`, `shift`, `exhibited`, `classify`, `frame_claim`, `nearest`, `diff_box`, `ratchet`, `trace_claim`, `main`, `BACK`, `AHEAD`, `REPORT_MAX`, `IPX_BYTES`, `GP_IDLE_LOSS_MIN_FIRST`, `GP_IDLE_LOSS_TRACE_MIN_FIRST`, `gp-oracle`, `gp-report` — each defined once.
- In `--report` mode both claims run with `N = 0`, print their counts and first items, and the exit is 0.
