# Pixel and Proof Closure — derivation record

Plan `docs/superpowers/plans/2026-10-22-pixel-and-proof-closure.md`, spec
`docs/superpowers/specs/2026-10-22-pixel-and-proof-closure-design.md` (§A,
U13 pixel truth).

**Task 1 is evidence only.** It reproduces the U13 report, maps the three
in-replay unexplained capture frames to the raw capture, to `poll.log` and to
the port's dump, and records the comparison model's negative results. It adds
no allowance, no fix and no pin: which of the spec's two outcomes (a real port
divergence or a measured comparison-model artefact) holds is Task 2's decision,
and every measured value below is the input to it.

Everything here was measured at base commit `2cd2ab0d` in the worktree
`pixel-and-proof-closure`. The capture is consumed read-only
(`data/` is read-only); no file under `data/`, `port/`, `tools/` or the
Makefile is touched.

## §P1 U13's in-replay unexplained frames: reproduction and mapping

### §P1.1 Commands, results and capture identity

```bash
make build
GP_WIN_KEEP=1 make gp-u13-oracle
python3 tools/gp_compare.py --report --scenario gp-u13-ending2 \
  --capture data/k11-captures/gp-u13-ending2 \
  --port /tmp/pr_gp_dump/gp-u13-ending2 --max-unexplained 0
```

`make build` exited 0. `GP_WIN_KEEP=1 make gp-u13-oracle` exited 0 (PASS) and
kept `/tmp/pr_gp_dump/gp-u13-ending2/` (4940 `.ipx` frames, `frames.txt`,
`trace.txt`; without `GP_WIN_KEEP` the Makefile removes the directory). The
verdict lines:

```text
gp_compare: gp-u13-ending2: frames: window from capture 96 (raw 1739); 4139 classified: 2569 clean, 1565 splice, 3 transition, 0 band-allowed, 0 scan3-allowed, 1 overlay-allowed, 1 unexplained, 14 all-black
gp_compare: gp-u13-ending2: frames: FIRST UNEXPLAINED capture 4248 (raw 7887): nearest port 3551, rows 0..96, x 0..319 (22923 px)
gp_compare: gp-u13-ending2: frames: coverage (reported, not ratcheted): 39 non-black port frame(s) up to port 3549 not exhibited by any classified capture frame: [9, 31, 123, 164, 165, 267, 1128, 1129, 1443, 1516, 1716, 1720, 1724, 1728, 1732, 1736, 1740, 1744, 1748, 1752] ...
gp_compare: gp-u13-ending2: frames: first unexplained 4248, ratchet N 4248 ok
gp_compare: gp-u13-ending2: trace: 9667 frames compared (f 130..), 43 without a capture snapshot; first tick difference f=131 (reported, not ratcheted)
gp_compare: gp-u13-ending2: trace: normalised (reported, not ratcheted): ent 0 of 9667 differ; t508 7857 of 9667 differ (first f=273)
gp_compare: gp-u13-ending2: trace: 0 differing through 10013; ratchet N 10014 ok
gp_compare: gp-u13-ending2: win: 0 differing through 10013; ratchet N 10014 ok
gp_compare: gp-u13-ending2: path: 0 not reproduced through 29; ratchet N 30 ok
gp_win: gp-u13-ending2: evidence: 30/30 milestones ok
```

The uncapped report exited 0 (report mode applies no ratchet). Its unexplained
list is exactly the expected set — `4248`, `4320`, `5120`, then `5891..5903`:

```text
gp_compare: gp-u13-ending2: frames: window from capture 96 (raw 1739); 5786 classified: 3492 clean, 2270 splice, 7 transition, 0 band-allowed, 0 scan3-allowed, 1 overlay-allowed, 16 unexplained, 22 all-black
gp_compare: gp-u13-ending2: frames: FIRST UNEXPLAINED capture 4248 (raw 7887): nearest port 3551, rows 0..96, x 0..319 (22923 px)
gp_compare: gp-u13-ending2: frames: UNEXPLAINED capture 4320 (raw 8019): nearest port 3615, rows 4..107, x 42..280 (273 px)
gp_compare: gp-u13-ending2: frames: UNEXPLAINED capture 5120 (raw 8839): nearest port 4317, rows 1..4, x 21..298 (13 px)
gp_compare: gp-u13-ending2: frames: UNEXPLAINED capture 5891 (raw 14361): nearest port 0, rows 0..199, x 0..319 (55510 px)
...
gp_compare: gp-u13-ending2: frames: UNEXPLAINED capture 5903 (raw 14417): nearest port 0, rows 0..199, x 0..319 (32421 px)
gp_compare: gp-u13-ending2: frames: coverage (reported, not ratcheted): 72 non-black port frame(s) up to port 4939 not exhibited by any classified capture frame: ...
gp_compare: gp-u13-ending2: trace: 0 differing through 10013
gp_compare: gp-u13-ending2: moves: 0 differing through 10013
```

Capture identity (`data/k11-captures/gp-u13-ending2/`): `poll.log` sha256
`e581916fa0a8e23a2ea52519aa0b358e4bc63084ce9defa0e1f733f3d12b69c6`, 5904
`frame_*.raw.gz`; `session.txt` gives `avis=[...] fps=70.0866`,
`raw_window=1388..14417 avi_frames=14422`, `time_limit=240 wall_s=206.7`,
`stop_at_end=1 tail=60 signal_f=2754`, and every `check=ok` line (base, steps
23/23, end frame, mode 0x27 after the Enter, snapshots, frames 5904/5904, port
script v2, no unscripted input, pokes 17/17, stopped at the end).

The port dump is 4940 frames (`frame_00000.ipx`..`frame_04939.ipx`); the last
`frames.txt` line is `04939 f=271B tick=00002807 mode=0003`, and `trace.txt`
ends at `T f=271D`. The dump's `frames.txt` is `%05u f=%04X tick=%08X mode=%04X`,
so a dumped frame's `f` is the game frame the port presented, not a capture
index.

### §P1.2 Mapping method and its measured ambiguity

For a capture frame `j`:

1. `raw = window.txt[j]` (`window.txt` maps the capture index to the absolute
   AVI frame number; `raw_window=1388..14417`). The three raws are
   `04248 7887`, `04320 8019`, `05120 8839`; the tail's are
   `05891 14361` .. `05903 14417`.
2. `t_raw = raw / 70.0866` s — the capture rate `session.txt` records. The AVI
   frame counter and the poll's `ms` are both DOSBox-X start-relative; the U10
   record used the same mapping (`2026-10-20-gameplay-truth-ii-derivations.md`,
   Run 2: "capture frame 0's raw 1391 at 19.85 s against the first `S` record's
   `ms=19999`").
3. The **nearest preceding `S` record** is the last `S` line whose `ms` is
   `<= 1000 * t_raw`; its `f`, `mode`, `st`, `r0`, `r1`, `s0_5a`, `s1_5a` and
   `b0c` are recorded. The next `S` record is recorded too, because the `S`
   series has sampling gaps: an `S` is logged only for a consistent
   end-of-iteration spin snapshot, and the poller logs a `P` line (a read
   outside a spin) for the iteration in between (`tools/gp_session.py`'s
   poll.log v2 header), so the capture instant can sit inside one.

The mapping has a measured clock lead, not a fitted constant. Taking every
sampled capture frame that `title_compare.explain` classifies **clean**, using
its matched port frame's `f` and that `f`'s first `S` record (|delta| <= 600 ms
to drop held-screen matches), `delta = S.ms - t_raw` is:

| raw range | n | median delta |
|---|---:|---:|
| 1900..4000 | 140 | +217.8 ms |
| 4000..7000 | 271 | +243.9 ms |
| 7000..9000 | 178 | +259.1 ms |
| 9000..11000 | 32 | +269.3 ms |
| 11000..12600 | 9 | +281.4 ms |
| 12600..13600 | 4 | +282.4 ms |

So the poll's `ms` runs ~0.22..0.28 s ahead of `raw/70.0866` over the capture's
range. At the end the `X` record's `ms=205231` against capture 5891's
`raw/70.0866 = 204903.6 ms` is +327 ms (the tail has no clean match; 5891 is
the first non-black frame at the script's end, §P1.6). The consequence for the
three frames is stated in the table and repeated here because it is the honest
limit of the mapping:

- **4248** and **4320** each sit inside a long `S` gap (328 ms and 511 ms). An
  `S` line is an end-of-iteration snapshot taken in the loop's spin (`S ...
  a consistent end-of-iteration snapshot (the master loop's spin state)`,
  `tools/gp_session.py` header), so an iteration that runs outside the spin
  writes a `P` line (read outside a spin) and no `S` until its own spin. The
  two frames' `t_raw` is 58 ms / 243 ms after the preceding `S`, and adding the
  measured ~+259 ms lead puts the instant 12 ms / 8 ms before the following `S`
  line. Both bracketing records are in the table. The bytes are the exact side:
  the capture is byte-equal to port frames labelled `152B`, `152B`, `152C`
  (4248) against the capture's `152A`→`152B` bracket, and `1567`, `1568`
  (4320) against `1566`→`1567`. Those are the port's own present labels — the
  following record's `f` and, for the later presented screens, one beyond it —
  so the wall-clock `S` `f` is a bracket, and no single capture `f` is asserted
  beyond the two records.
- **5120** has only a 15 ms gap, so the raw-clock nearest preceding `S` is
  `f=1815` (line 6228), but the +269 ms lead puts the instant at 126.384 s,
  which is 1 ms after line 6244 (`ms=126383`, `f=1825`); line 6243
  (`ms=126360`, `f=1824`) is the record before it. Every recorded field except
  `f` is identical across those records (`mode=0015 st=0000 r0=FF r1=05
  s0_5a=00 s1_5a=78 b0c=00`), and the nearest port frame is labelled `f=1826`,
  again 1..2 above the bracketing `f` — so the calibrated reading is the
  consistent one, and the raw-clock `f` is not presented as the game frame.

No game frame is invented anywhere below: the table names the two `S` records,
the calibration lead, and the port frame's own `f`.

### §P1.3 The frame table

The two bracketing `S` records and the fields the brief asks for (`f`, `mode`,
`st`, `r0`/`r1`, `s0_5a`/`s1_5a`, `b0c`):

| capture | raw (`window.txt`) | `t_raw` ms | lead (median) | preceding `S` (line, ms) | `f` | `mode` | `st` | `r0` | `r1` | `s0_5a` | `s1_5a` | `b0c` | next `S` (line, ms, `f`, `mode`) |
|---|---:|---:|---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---|
| 4248 | 7887 | 112532.2 | +259 | 5462, 112474 | `152A` | `000D` | `0000` | `FF` | `26` | `00` | `78` | `00` | 5465, 112803, `152B`, `000C` |
| 4320 | 8019 | 114415.6 | +259 | 5534, 114172 | `1566` | `000D` | `0000` | `FF` | `26` | `00` | `78` | `00` | 5537, 114683, `1567`, `000C` |
| 5120 | 8839 | 126115.4 | +269 | 6228, 126110 | `1815` | `0015` | `0000` | `FF` | `05` | `00` | `78` | `00` | 6229, 126125, `1816`, `0015` |

For 5120 the lead-corrected nearest preceding `S` is line 6244
(`ms=126383`, `f=1825`), with line 6243 (`ms=126360`, `f=1824`) the record
before it; its fields are identical to the 6228 row above except `f`. For 4248
and 4320 the lead-corrected nearest preceding `S` is still the listed line
(their `t_raw + lead` is 12/8 ms before the next `S`), which is the bracket the
table records.

The port side, `gp_compare.nearest` (row-hash maximum; content-based, not
temporal) and `gp_compare.diff_box` against that frame:

| capture | nearest port | `frames.txt` | diff rows | x | px | differing bytes |
|---|---:|---|---:|---:|---:|---:|
| 4248 | 3551 | `03551 f=152C tick=000015CB mode=000C` | 0..96 | 0..319 | 22923 | 66153 |
| 4320 | 3615 | `03615 f=1568 tick=00001641 mode=000C` | 4..107 | 42..280 | 273 | 786 |
| 5120 | 4317 | `04317 f=1826 tick=000018FF mode=0015` | 1..4 | 21..298 | 13 | 13 |

Each is a single-frame anomaly: its neighbours are clean or splices —

| capture | classification | nearest port (`f`) |
|---|---|---|
| 4246 | clean | 3548 (`152B`) |
| 4247 | clean | 3549 (`152B`) |
| **4248** | **unexplained** | 3551 (`152C`) |
| 4249 | splice | 3551 (`152C`) |
| 4250 | clean | 3552 (`152D`) |
| 4319 | clean | 3613 (`1567`) |
| **4320** | **unexplained** | 3615 (`1568`) |
| 4321 | splice | 3616 (`1569`) |
| 5119 | clean | 4315 (`1824`) |
| **5120** | **unexplained** | 4317 (`1826`) |
| 5121 | splice | 4318 (`1827`) |

### §P1.4 The model tests: clean, splice, transition, DAC

`title_compare.explain` over the whole dump returns `unexplained` for all
three. The individual tests, as measured:

| capture | clean | splice: best adjacent pair | transition: best prefix+suffix rows | DAC constant offset | explained? |
|---|---|---|---|---|---:|
| 4248 | no | no — pair N=3550 (3550,3551), 24005 differing bytes | no — 103 rows at N=3550 (needs 199) | no — 2810/22923 px have no offset in ±64; largest numeric offset +1 covers 1393/22923 (6.1 %) | **no** |
| 4320 | no | no — pair N=3614 (3614,3615), 347 differing bytes | no — 96 rows at N=3614 | no — 55/273 px have no offset; largest −7 covers 15/273 (5.5 %) | **no** |
| 5120 | no | no — pair N=4316 (4316,4317), 13 differing bytes | no — 196 rows at N=4316 | no — −29 covers 8/13; −64 (the search boundary) covers 5/13 | **no** |

The DAC-index test is the U10 precedent's test (record
`2026-10-20-gameplay-truth-ii-derivations.md`, Run 1: "every differing pixel of
the run maps to `DAC[port_index + 1]`"). For every pixel that differs from the
nearest port frame, take the port frame's index `p` at that pixel and ask
whether the capture's RGB equals `port_dac[(p + k) & 0xFF]` for one constant
`k` over the frame (`k` searched in −64..+64). A palette shift would show one
`k` covering all differing pixels; none does. All differing capture pixels'
RGB values are present in the port frame's 256-entry DAC (0 absent), so this is
not a missing palette entry either — the pixels are different *content*, not a
different index mapping of the same content.

**Negative result for all three.** None of clean, a byte splice of an adjacent
port pair, one transition row, or a constant DAC-index offset explains 4248,
4320 or 5120. This record does not call any of them an artefact on the size of
the mismatch; the negative result is the result.

### §P1.5 What the bytes measure: the segment structure

The row runs below come from comparing each capture row (960 bytes) with every
port frame's row: for each row, the set of port frame indices whose row is
byte-equal to the capture's. Maximal runs of rows with an identical holder set:

**4248** — a three-segment composition:

```text
rows   0.. 45  held only by port 3549   (f=152B tick=000015C7 mode=000D)
rows  46.. 96  held only by port 3550   (f=152B tick=000015CA mode=000C)
rows  97..199  held only by port 3551   (f=152C tick=000015CB mode=000C)
```

- split `r1=46`, `r2=97`: exactly one adjacent pair `(3550, 3551)` binds rows
  46..96 and 97..199; the only holder of rows 0..45 is port 3549 (an earlier
  frame, and also the clean match of capture 4247);
- `scan3_explain` with the measured top hash returns `([3549, 3550, 3551],
  None)`, i.e. this frame satisfies the existing SCAN3 recipe's shape exactly.
  The measured top hash is
  `fc1044a6a9e5bb6cee142ce02811c4a4823585747351089bd1514a76685fc6e6`
  (sha256 of rows 0..45). **This is a measured candidate for Task 2, not an
  allowance added here**: `SCAN3_ALLOWED` has no `gp-u13-ending2` entry, so
  the classifier reports 4248 unexplained;
- `band_pair(top_row=46, band_hi=97)` is `None`, so the BANDS shape does not
  apply;
- the three segments are the mode-`0x0D` frame and the two following
  mode-`0x0C` frames of the `S` bracket `f=152A mode=000D` → `f=152B
  mode=000C`: the capture frame is byte-equal to three consecutively presented
  port frames, split 46/51/103 rows.

**4320** — a composition with no single-holder top:

```text
rows   0.. 87  held by no dumped frame (0 holders)
rows  88..107  held only by port 3614   (f=1567 tick=00001640 mode=000C)
rows 108..189  held only by port 3615   (f=1568 tick=00001641 mode=000C)
rows 190..199  held by ports 3614 and 3615
```

- split `r1=88`, `r2=108`: the adjacent pair is `(3614, 3615)` (unique), but
  the SCAN3 shape fails its top condition — **no** port frame at all holds rows
  0..87 (`scan3_explain` reports "no port frame before 3614 holds its named top
  rows"), and `band_pair(88, 108)` is `None`;
- all 273 differing pixels' bytes are held by at least one dumped frame at that
  coordinate (0 unheld); 59 of them are held *only* by port 3614. The frame is
  a composition of several dumped frames (the pixels are byte-equal to earlier
  frames' rows/pixels), not content absent from the dump, and the two-segment
  part is rows 88..107 / 108..199.

**5120** — a 13-pixel difference:

```text
rows   0..  0  byte-equal to ports 4316, 4317, 4318
rows   1..  4  not byte-equal to any dumped frame (13 px against port 4317)
rows   5..199  byte-equal to port 4317 (many rows also equal to 4316/4318)
```

- the 13 differing pixels (x 21..298) are:
  `00001c` where the port has `000000` at (1,23), (1,24), (1,295), (1,296),
  (2,22), (2,297), (3,21), (3,298) — **held by no dumped frame** (0 holders);
  `000000` where the port has `100000` at (3,74) (717 holders);
  `000000` where the port has `00000c` at (4,27), (4,28), (4,291), (4,292)
  (573..659 holders). All 13 capture RGB values are in the port DAC;
- split `r1=5`, `r2=115`: the adjacent pair `(4316)` binds the rows below, but
  no port frame holds rows 0..4 (`scan3_explain` reports no earlier top
  holder), and `band_pair(5, 115)` is `None`. Eight pixels of a colour that no
  dumped frame carries at those coordinates are the frame's unbound part.

### §P1.6 The post-script tail 5891..5903 is outside the frame claim

The report's remaining 13 unexplained frames are one run:

| capture | raw | nearest port | rows | x | px |
|---|---:|---:|---:|---:|---:|
| 5891 | 14361 | 0 | 0..199 | 0..319 | 55510 |
| 5892 | 14365 | 0 | 0..199 | 0..319 | 49244 |
| 5893 | 14370 | 0 | 0..199 | 0..319 | 49246 |
| 5894 | 14375 | 0 | 0..199 | 0..319 | 45010 |
| 5895 | 14379 | 0 | 0..199 | 0..319 | 45008 |
| 5896 | 14384 | 0 | 0..199 | 0..319 | 41008 |
| 5897 | 14389 | 0 | 0..199 | 0..319 | 38648 |
| 5898 | 14394 | 0 | 0..199 | 0..319 | 38289 |
| 5899 | 14398 | 0 | 0..199 | 0..319 | 36516 |
| 5900 | 14403 | 0 | 0..199 | 0..319 | 35899 |
| 5901 | 14408 | 0 | 0..199 | 0..319 | 33835 |
| 5902 | 14412 | 0 | 0..199 | 0..319 | 33300 |
| 5903 | 14417 | 0 | 0..199 | 0..319 | 32421 |

- the scenario's end step is `X ms=205231 f=271D step=24 end` (`poll.log` line
  10067); the poll continues `S f=271E` (line 10068, `ms=205246`) through
  `f=2754` (line 10122, `ms=206145`) and ends `E ms=206380 reason=end rc=0`
  (line 10123), matching `session.txt`'s `stop_at_end=1 tail=60
  signal_f=2754`;
- the port's dump ends at frame 4939 (`f=271B`) and its trace at `f=271D`; the
  replay has no frame for the post-`271D` states;
- capture 5891 (raw 14361) is 327 ms before the `X` by `t_raw` (204903.6 vs
  205231); the measured lead grows from +217.8 ms (raw 1900..4000) to +282.4 ms
  (raw 12600..13600), so the AVI frame at the `X` is around 5891. It is also the
  first non-black frame after the black pair 5889/5890. The run is the
  capture's post-script output: **recorded, not forced, and outside the frame
  claim**. The `nearest port 0` in this run is the row-hash maximum against the
  port's boot frame (the dump has no later frame to match), not a temporal
  match.

### §P1.7 No pin moved

This task changes no pin and no tool. The Makefile's U13 values are unchanged
and remain the ones the run above produced:

```text
GP_U13_MIN_FIRST = 4248
GP_U13_TRACE_MIN_FIRST = 10014
GP_U13_MAX_START = 96
GP_U13_MIN_REACH = 3549
GP_U13_CAPTURE_SHA256 = e581916fa0a8e23a2ea52519aa0b358e4bc63084ce9defa0e1f733f3d12b69c6
GP_U13_CAPTURE_FRAMES = 5904
GP_U13_MILESTONES = 30
GP_U13_WIN_MIN_FIRST = 10014
```

The only tracked file this task adds is this record (the task report is the
git-ignored ledger).

### §P1.8 Reproduction

Run from the repo root after `GP_WIN_KEEP=1 make gp-u13-oracle`. Save the block
as `/tmp/u13_task1.py` and run `python3 /tmp/u13_task1.py 4248 4320 5120` (no
arguments is the same list). It uses the repo's own loaders
(`tools/gp_compare.py`'s `load_capture_frame`, `load_port_frame`, `nearest`,
`diff_box`, `band_pair`, `scan3_explain` and `tools/title_compare.py`'s
`explain`, `row_hashes`, `row_common`, `best_splice`, `raw_map`), and writes
nothing:

```python
#!/usr/bin/env python3
import hashlib, os, sys, collections
ROOT = os.getcwd()                  # run from the repo root
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import gp_compare as gc
import title_compare as tc

CAP = os.path.join(ROOT, 'data/k11-captures/gp-u13-ending2')
PORT = '/tmp/pr_gp_dump/gp-u13-ending2'
FPS = 70.0866                      # session.txt
N = len(gc._paths(PORT, 'frame_%05d.ipx'))
SEQ = gc.Lazy(gc._paths(PORT, 'frame_%05d.ipx'), gc.load_port_frame)
ROWS = [tc.row_hashes(SEQ[m]) for m in range(N)]
RAWS = tc.raw_map(CAP)


def fmap():
    out = {}
    for line in open(os.path.join(PORT, 'frames.txt')):
        p = line.split()
        if len(p) >= 2:
            out[int(p[0])] = p[1]
    return out


def snapshots():
    out = []
    for ln, line in enumerate(open(os.path.join(CAP, 'poll.log')), 1):
        if line.startswith('S '):
            d = {'line': ln}
            d.update(kv.split('=', 1) for kv in line.split()[1:])
            out.append(d)
    return out


def frame(j, fm):
    c = gc.load_capture_frame(os.path.join(CAP, 'frame_%05d.raw.gz' % j))
    ch = tc.row_hashes(c)
    kind, _ = tc.explain(c, ch, SEQ, ROWS, N)
    m = gc.nearest(c, ROWS)
    box = gc.diff_box(c, SEQ[m])
    print('\ncapture %d raw %d t_raw %.1f ms -> explain %s' % (
        j, RAWS[j], RAWS[j] / FPS * 1000, kind))
    print('  nearest port %d (%s): rows %d..%d x %d..%d, %d px, %d bytes differ' % (
        m, fm[m], box[0], box[1], box[2], box[3], box[4],
        sum(1 for a, b in zip(c, SEQ[m]) if a != b)))
    runs = []
    for r in range(tc.FRAME_H):
        hold = tuple(mm for mm in range(N) if ROWS[mm][r] == ch[r])
        if runs and runs[-1][2] == hold and runs[-1][1] == r - 1:
            runs[-1] = (runs[-1][0], r, hold)
        else:
            runs.append((r, r, hold))
    for lo, hi, hold in runs:
        print('    row run %3d..%3d  %s' % (
            lo, hi, list(hold[:8]) + (['...'] if len(hold) > 8 else [])))
    r1, r2 = {4248: (46, 97), 4320: (88, 108), 5120: (5, 115)}[j]
    pair = [mm - 1 for mm in range(1, N)
            if all(ROWS[mm - 1][r] == ch[r] for r in range(r1, r2)) and
            all(ROWS[mm][r] == ch[r] for r in range(r2, tc.FRAME_H))]
    top = [a for a in range(N) if all(ROWS[a][r] == ch[r] for r in range(r1))]
    band = [mm - 1 for mm in range(1, N)
            if all(ROWS[mm - 1][r] == ch[r] for r in range(0, r1)) and
            all(ROWS[mm][r] == ch[r] for r in range(r2, tc.FRAME_H))]
    print('  split r1=%d r2=%d: pair %s; holders of rows 0..%d: %s; band_pair %s' % (
        r1, r2, pair, r1 - 1, top[:8] + (['...'] if len(top) > 8 else []), band or 'none'))
    top_sha = hashlib.sha256(c[:r1 * tc.ROW]).hexdigest()
    print('  top sha256 %s; scan3_explain %r' % (
        top_sha, gc.scan3_explain(c, SEQ, (j, r1, r2, top_sha))))
    data = open(os.path.join(PORT, 'frame_%05d.ipx' % m), 'rb').read()
    idx, dac = data[:64000], data[64000:]
    off = collections.Counter()
    for i in range(tc.FRAME_W * tc.FRAME_H):
        a = c[3 * i:3 * i + 3]
        if a == SEQ[m][3 * i:3 * i + 3]:
            continue
        p = idx[i]
        off[next((k for k in range(-64, 65)
                  if dac[3 * ((p + k) & 0xFF):3 * ((p + k) & 0xFF) + 3] == a), None)] += 1
    print('  DAC offsets (n=%d): %s' % (sum(off.values()), ', '.join(
        ('%+d' % k if k is not None else 'none') + ':%d' % v for k, v in off.most_common(8))))


def neighbours(j, fm):
    for jj in (j - 1, j + 1):
        c = gc.load_capture_frame(os.path.join(CAP, 'frame_%05d.raw.gz' % jj))
        kind, _ = tc.explain(c, tc.row_hashes(c), SEQ, ROWS, N)
        m = gc.nearest(c, ROWS)
        print('  neighbour %d: %s, nearest port %d (%s)' % (jj, kind, m, fm[m]))


def calibration(fm):
    sms = {}
    for s in snapshots():
        sms.setdefault(int(s['f'], 16), int(s['ms']))
    ds = []
    for j in range(96, 5890, 5):
        c = gc.load_capture_frame(os.path.join(CAP, 'frame_%05d.raw.gz' % j))
        if not any(c):
            continue
        kind, d = tc.explain(c, tc.row_hashes(c), SEQ, ROWS, N)
        if kind != 'clean' or fm.get(d) is None:
            continue
        f = int(fm[d].split('=')[1], 16)
        if f in sms:
            delta = sms[f] - RAWS[j] / FPS * 1000.0
            if abs(delta) <= 600:
                ds.append((RAWS[j], delta))
    for lo, hi in ((1900, 4000), (4000, 7000), (7000, 9000), (9000, 11000),
                   (11000, 12600), (12600, 13600)):
        s = sorted(d for r, d in ds if lo <= r < hi)
        if s:
            print('clock lead raw %5d..%5d: n=%3d median %+.1f ms' % (
                lo, hi, len(s), s[len(s) // 2]))


if __name__ == '__main__':
    fm = fmap()
    for j in [int(x) for x in sys.argv[1:]] or [4248, 4320, 5120]:
        frame(j, fm)
        neighbours(j, fm)
    calibration(fm)
```

Its output is the source of the row runs, the pair/band/scan3 results, the DAC
histograms and the clock-lead table above. (The `idx, dac` line in the
reproduction is kept literal to the `.ipx` layout — 64000 indices then 768 DAC
bytes; `gp_compare.expand_ipx` is the model it reproduces.)
