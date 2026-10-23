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
`trace.txt`, `gp.log`; without `GP_WIN_KEEP` the Makefile removes the
directory). The verdict lines:

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
`raw/70.0866 = 204903.6 ms` is +327 ms (the tail has no clean match; 5890 is
the last capture frame clean against the dump and 5891 the first post-script
one, §P1.6). The consequence for the three frames is stated in the table and
repeated here because it is the honest limit of the mapping:

- **4248** and **4320** each sit inside a long `S` gap (329 ms and 511 ms). An
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
  (`ms=126360`, `f=1824`) is the record before it. The seven fields the brief
  asks for are identical across those records (`mode=0015 st=0000 r0=FF r1=05
  s0_5a=00 s1_5a=78 b0c=00`); `f` and the timing fields (`rng`, `t508`, `t50c`,
  `tick`) differ. The nearest port frame is labelled `f=1826`, again 1..2 above
  the bracketing `f` — so the calibrated reading is the consistent one, and the
  raw-clock `f` is not presented as the game frame.

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
before it. The seven fields the brief asks for (`mode`, `st`, `r0`, `r1`,
`s0_5a`, `s1_5a`, `b0c`) are identical to the 6228 row above; `f`, `rng`,
`t508`, `t50c` and `tick` differ (the first version's "every field except `f`"
claim was false: `rng` alone differs between 6228 and 6243/6244). For 4248
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
three, so clean, a byte splice of an adjacent port pair and one transition row
are negative for all three. The individual tests, as measured:

| capture | clean | splice: best adjacent pair | transition: best prefix+suffix rows | DAC-index: constant offset on the differing pixels |
|---|---|---|---|---:|
| 4248 | no | no — pair N=3550 (3550,3551), 24005 differing bytes | no — 103 rows at N=3550 (needs 199) | no — the intersection over all 22923 differing pixels is empty; 2810 px have no offset in ±64; the largest per-`k` coverage is +1 at 1393/22923 (6.1 %) |
| 4320 | no | no — pair N=3614 (3614,3615), 347 differing bytes | no — 96 rows at N=3614 | no — the intersection is empty; 55 px have no offset in ±64; the largest per-`k` coverage is −7 at 15/273 (5.5 %) |
| 5120 | no | no — pair N=4316 (4316,4317), 13 differing bytes | no — 196 rows at N=4316 | **yes — the intersection is {−29, +14, +29}, each covering 13/13** — **superseded by §P3.3**: the intersection is a DAC alias (both colours recur at several entries); frame-wide, `k = 0` has 13 mismatching pixels |

The DAC-index test is the U10 precedent's test (record
`2026-10-20-gameplay-truth-ii-derivations.md`, Run 1: "every differing pixel of
the run maps to `DAC[port_index + 1]`"). The measure is the **intersection** of
the per-pixel offset sets, not the smallest matching offset: for every pixel
that differs from the nearest port frame, take the port frame's index `p` at
that pixel and compute `K = {k in −64..+64 : port_dac[(p + k) & 0xFF] ==
capture RGB}`; the frame's constant offsets are `∩K` over all differing
pixels. (The first version of this record tabulated the smallest matching `k`
per pixel, which is not coverage: for 5120, the five black pixels whose
smallest match is −64 also match −29, +14 and +29.)

- **4248**: `∩K` is empty (2810 of the 22923 differing pixels have `K = ∅`);
  the best single `k` is +1 at 1393/22923 (6.1 %). Negative.
- **4320**: `∩K` is empty (55 of 273 pixels have `K = ∅`); the best single `k`
  is −7 at 15/273 (5.5 %). Negative.
- **5120**: `∩K = {−29, +14, +29}`, each covering 13/13. The 8 `00001c`
  pixels have `K = {−29, 14, 16, 17, 29}`; the 5 black pixels' `K` sets are
  large (black is common in the DAC) and contain −29, +14 and +29.
  **Positive.** This is the U10 4113 evidence signature: that record's Run 1
  read "every differing pixel of the run maps to `DAC[port_index + 1]`" and
  its Task 3 identified the cause as the RLE span writers' dword bank add (a
  real port divergence). This record measures the signature for 5120; it does
  not name the writer or decide that 5120 is the same defect — Task 2 does.
  **Superseded by §P3.3** (2026-10-23 review): the `{−29, +14, +29}`
  intersection is a **DAC alias** — `00001c` occurs at five entries (0x15,
  0x40, 0x42, 0x43, 0x4F) and black at 207 — so it does not distinguish a
  palette shift from content. Frame-wide the same test picks `k = 0` (13
  mismatching pixels of 64000) over every other offset (best non-zero 11282),
  so the 4113 signature is **not** present and 5120 is not a palette shift.

So the frame model (clean, splice, transition) explains none of the three, and
the DAC-index test is negative for 4248 and 4320 but **positive for 5120** as
measured over the differing pixels only; §P3.3 supersedes that 5120 verdict
(frame-wide `k = 0` is the only fit, and the intersection is an alias).
The first version's sentences "A palette shift would show one `k` covering all
differing pixels; none does", "the pixels are different *content*, not a
different index mapping of the same content" and "Negative result for all
three" were false for 5120; §P1.9 records the correction.

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

**4320** — the maximal runs (27 runs):

```text
rows   0..  2  n=59
rows   3..  3  n=22
rows   4..  4  n=12
rows   5..  5  n=22
rows   6..  8  n=7    [3589, 3590, 3591, 3611, 3612, 3613, 3633]
rows   9..  9  n=6    [3589, 3590, 3591, 3611, 3612, 3613]
rows  10.. 10  n=11
rows  11.. 11  n=1    [3613]
rows  12.. 13  n=7
rows  14.. 14  n=1    [3613]
rows  15.. 17  n=4    [3613, 3692, 3693, 3694]
rows  18.. 32  n=8    [3614, 3615, 3616, 3695, 3696, 3697, 3749, 3750]
rows  33.. 41  n=58
rows  42.. 52  n=59
rows  53.. 65  n=21
rows  66.. 68  n=59
rows  69.. 69  n=58
rows  70.. 70  n=56
rows  71.. 71  n=52
rows  72.. 72  n=50
rows  73.. 78  n=21
rows  79.. 82  n=42
rows  83.. 86  n=41
rows  87.. 87  n=21
rows  88..107  n=1    [3614]
rows 108..189  n=1    [3615]
rows 190..199  n=2    [3614, 3615]
```

(`n` is the number of port frames holding the row; sets with more than 8
holders are elided and printed in full by §P1.8's script.)

- split `r1=88`, `r2=108`: the adjacent pair is `(3614, 3615)` (unique), but
  the SCAN3 shape fails its top condition — **no single port frame holds all of
  rows 0..87** (`scan3_explain` reports "no port frame before 3614 holds its
  named top rows"), and `band_pair(88, 108)` is `None`. **§P3.2 corrects this
  split to `18/108`, which the bytes force** (3614 does not hold rows 14..17
  and 3613 does not hold row 18, so `r1 = 18`; 3615 does not hold rows 88..107
  and 3614 does not hold rows 108..189, so `r2 = 108`): the maximal runs of
  identical holder sets are not the segment boundaries here, because the static
  background gives rows 18..87 many different holder sets;
- the 273 differing pixels' holder counts against the whole dump: 59 are held
  only by port 3614, 16 by 2..10 frames, 54 by 11..100, 144 by more than 100,
  and **0 by no frame**. No single dumped frame holds the frame's 200 rows (the
  top 88 rows have per-row holders but no whole-block holder), and splice/SCAN3
  fail; whether this is a port divergence or a comparison artefact is Task 2's
  decision.

**5120** — the maximal runs (20 runs):

```text
rows   0..  0  n=3    [4316, 4317, 4318]
rows   1..  4  n=0    []
rows   5..114  n=3    [4316, 4317, 4318]
rows 115..115  n=120  [1, 3, 5, 7, 23, 24, 25, 26, 27, 28, ...]
rows 116..138  n=3    [4316, 4317, 4318]
rows 139..140  n=2    [4317, 4318]
rows 141..141  n=2    [4316, 4317]
rows 142..144  n=1    [4317]
rows 145..145  n=2    [4316, 4317]
rows 146..152  n=1    [4317]
rows 153..156  n=2    [4316, 4317]
rows 157..157  n=1    [4317]
rows 158..158  n=2    [4316, 4317]
rows 159..168  n=1    [4317]
rows 169..169  n=2    [4316, 4317]
rows 170..171  n=1    [4317]
rows 172..172  n=2    [4317, 4318]
rows 173..193  n=1    [4317]
rows 194..194  n=2    [4316, 4317]
rows 195..199  n=1    [4317]
```

- the split is derived from the runs, not chosen: `r1 = 5` is the first row
  after the run 1..4 that no frame holds, and `r2 = 115` is the end of the
  5..114 run plus one (the next holder-set boundary). At `(5, 115)` the unique
  adjacent pair is **`(4316, 4317)`** (`scan3_explain`'s `B = 4316`); the SCAN3
  top condition fails — no port frame at all holds rows 0..4 — and
  `band_pair(5, 115)` is `None`;
- the 13 differing pixels (x 21..298) are:
  `00001c` where the port has `000000` at (1,23), (1,24), (1,295), (1,296),
  (2,22), (2,297), (3,21), (3,298) — **held by no dumped frame** (0 holders);
  `000000` where the port has `100000` at (3,74) (717 holders);
  `000000` where the port has `00000c` at (4,27), (4,28), (4,291), (4,292)
  (573..659 holders). The DAC-index test (§P1.4) maps all 13 to the constant
  offsets {−29, +14, +29}; the 8 unheld `00001c` pixels are the ones the port
  draws black.

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
  (raw 12600..13600), so the AVI frame at the `X` is around 5891. The content
  boundary is exact: 5889 is all-zero; 5890 is not black (166 pixels at rows
  7..12, x 130..196, colour `41b2d3`) and is **clean at port 4939**, the dump's
  last frame; 5891 is the first frame after the port's replay ends — so the
  run starts at the script's end, not at a black pair. The run is the
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
arguments is the same list). It prints, per frame: the class, the nearest port
frame and diff box, the maximal row runs, the split's pair/`band_pair`/
`scan3_explain` results, the best adjacent-pair splice and best prefix+suffix
row counts, the exact DAC offset intersection and per-`k` coverage, the holder
counts for the differing pixels, and the neighbours' classes; then the
clock-lead table. It uses the repo's own loaders
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
    # the maximal row runs (identical holder set per row)
    runs = []
    for r in range(tc.FRAME_H):
        hold = tuple(mm for mm in range(N) if ROWS[mm][r] == ch[r])
        if runs and runs[-1][2] == hold and runs[-1][1] == r - 1:
            runs[-1] = (runs[-1][0], r, hold)
        else:
            runs.append((r, r, hold))
    for lo, hi, hold in runs:
        print('    row run %3d..%3d n=%-3d %s' % (
            lo, hi, len(hold), list(hold[:10]) + (['...'] if len(hold) > 10 else [])))
    # the measured three-segment candidate split (derived from the runs)
    r1, r2 = {4248: (46, 97), 4320: (88, 108), 5120: (5, 115)}[j]
    pair = [mm - 1 for mm in range(1, N)
            if all(ROWS[mm - 1][r] == ch[r] for r in range(r1, r2)) and
            all(ROWS[mm][r] == ch[r] for r in range(r2, tc.FRAME_H))]
    top = [a for a in range(N) if all(ROWS[a][r] == ch[r] for r in range(r1))]
    band = [mm - 1 for mm in range(1, N)
            if all(ROWS[mm - 1][r] == ch[r] for r in range(0, r1)) and
            all(ROWS[mm][r] == ch[r] for r in range(r2, tc.FRAME_H))]
    print('  split r1=%d r2=%d: B hits %s -> pairs %s; holders of rows 0..%d: %d %s; band_pair %s' % (
        r1, r2, pair, [(b, b + 1) for b in pair], r1 - 1, len(top),
        top[:8] + (['...'] if len(top) > 8 else []), band or 'none'))
    top_sha = hashlib.sha256(c[:r1 * tc.ROW]).hexdigest()
    print('  top sha256 %s; scan3_explain %r' % (
        top_sha, gc.scan3_explain(c, SEQ, (j, r1, r2, top_sha))))
    # the best adjacent-pair splice and the transition precondition
    pref = [0] * N
    suff = [0] * N
    for mm in range(N):
        pref[mm], suff[mm] = tc.row_common(ch, ROWS[mm])
    bs = tc.best_splice(c, SEQ, pref, suff, N)
    bt = max(((pref[mm] + suff[mm + 1], mm) for mm in range(N - 1)))
    print('  best splice %d differing bytes at N=%d (pair %d/%d); best prefix+suffix %d rows at N=%d' % (
        bs[0], bs[1], bs[1], bs[1] + 1, bt[0], bt[1]))
    # the exact DAC-index test: per-pixel offset sets, their intersection and coverage
    data = open(os.path.join(PORT, 'frame_%05d.ipx' % m), 'rb').read()
    idx, dac = data[:64000], data[64000:]
    pix, inter, cover = [], set(range(-64, 65)), collections.Counter()
    for i in range(tc.FRAME_W * tc.FRAME_H):
        a = c[3 * i:3 * i + 3]
        if a == SEQ[m][3 * i:3 * i + 3]:
            continue
        p = idx[i]
        ks = frozenset(k for k in range(-64, 65)
                       if dac[3 * ((p + k) & 0xFF):3 * ((p + k) & 0xFF) + 3] == a)
        pix.append((i, p, a, ks))
        inter &= ks
        for k in ks:
            cover[k] += 1
    best = sorted(cover.items(), key=lambda kv: -kv[1])[:4]
    print('  DAC: %d differing px; intersection %s; per-k coverage %s' % (
        len(pix), sorted(inter) if inter else 'EMPTY',
        ['%+d:%d/%d' % (k, v, len(pix)) for k, v in best]))
    print('  DAC pixels with no offset: %d' % sum(1 for _i, _p, _a, ks in pix if not ks))
    for i, p, a, ks in (pix[:13] if j == 5120 else []):
        print('    (row %d, x %d) capture %s port %s -> %s' % (
            i // 320, i % 320, a.hex(), SEQ[m][3 * i:3 * i + 3].hex(), sorted(ks)))
    # the holder-count buckets for the differing pixels (small frames only)
    if len(pix) <= 300:
        hs = [[] for _ in pix]
        for mm in range(N):
            d = SEQ[mm]
            for k, (i, _p, _a, _ks) in enumerate(pix):
                if d[3 * i:3 * i + 3] == c[3 * i:3 * i + 3]:
                    hs[k].append(mm)
        buckets = collections.Counter(
            '0' if not h else '1' if len(h) == 1 else '2..10' if len(h) <= 10
            else '11..100' if len(h) <= 100 else '>100' for h in hs)
        print('  differing-pixel holder counts: %s' % dict(buckets))
        print('  single-holder pixels: %s' % dict(collections.Counter(
            h[0] for h in hs if len(h) == 1)))


def neighbours(j, fm):
    for jj in (j - 2, j - 1, j + 1, j + 2):
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

Its output is the source of the row runs, the pair/band/scan3 results, the
splice/transition counts, the DAC intersection and coverage, the holder-count
buckets and the clock-lead table above. (The `idx, dac` line in the
reproduction is kept literal to the `.ipx` layout — 64000 indices then 768 DAC
bytes; `gp_compare.expand_ipx` is the model it reproduces.)

### §P1.9 Correction (review, 2026-10-22)

The first version of §P1 had the following evidence errors; the corrected
values above are those the §P1.8 script prints, and no pin, code, tool or
Makefile changed.

1. **5120's DAC test is positive.** The first version tabulated the
   *smallest matching* offset per pixel and read `−29: 8/13, −64: 5/13` as
   coverage. The correct measure is the intersection of the per-pixel offset
   sets: `{−29, +14, +29}`, each covering 13/13. The 8 `00001c` pixels match
   {−29, 14, 16, 17, 29}; the 5 black pixels also match −29, +14 and +29. This
   is the U10 4113 evidence signature; the "different content, not a different
   index mapping" and "Negative result for all three" sentences were false for
   5120. 4248's and 4320's negatives stand (their intersections are empty;
   2810/22923 and 55/273 pixels have no offset in ±64).
   **Superseded by §P3.3 (2026-10-23 review): the "positive" is a DAC alias,
   not a signature.** `00001c` occurs at five DAC entries and black at 207, so
   an offset that "covers" 13 pixels proves nothing; measured frame-wide (all
   64000 pixels) `k = 0` mismatches 13 and every non-zero offset mismatches
   11282 or more, so 5120 is not a palette shift and the 4113 class is absent.
2. **The 4320 row block was an intersection, not the runs.** "Rows 0..87 held
   by no dumped frame" was the whole-block holder check; per-row, rows 0..2 are
   held by 59 frames, etc. §P1.5 now lists the 27 maximal runs and says "no
   single frame holds all of rows 0..87".
3. **The 5120 row block was not the maximal runs.** §P1.5 now lists the 20
   runs, derives the split as `r1 = 5` (after the no-holder run 1..4) and
   `r2 = 115` (the end of the 5..114 run plus one), and names the pair
   `(4316, 4317)` (the first version's `(4316)` was the `B` index, not the
   pair).
4. **The 4320 composition sentence overclaimed.** The measured facts are the
   row runs, the 273 differing pixels' holder counts (59 held only by 3614, 16
   by 2..10, 54 by 11..100, 144 by >100, 0 unheld) and the failing splice/SCAN3
   tests; "a composition of several dumped frames … not content absent from the
   dump" is removed. The divergence-vs-artefact decision is Task 2's.
5. **Small corrections.** 5890 is not black (166 pixels at rows 7..12, x
   130..196, colour `41b2d3`); only 5889 is all-zero, and 5890 is clean at port
   4939. The 5120 `S` records differ in `rng`, `t508`, `t50c` and `tick` as
   well as `f`; only the seven brief fields are identical. 4248's `S` gap is
   329 ms (112803−112474); the 328 ms figure is the `S`→`P` span
   (112802−112474). The neighbour rows 4246/4250 are now printed by the script
   (it checks `j−2 .. j+2`), and the splice/transition counts are computed by
   it. The dump contents list includes `gp.log`.

## §P2 4248: class C (a measured three-segment scan-out)

### §P2.1 The decision

**C — a capture/comparison-model artefact, not a port divergence.** Capture
4248 (raw 7887) is byte-equal, row for row, to three *consecutive port
presents*: rows 0..45 to port 3549, rows 46..96 to port 3550 and rows 97..199
to port 3551. No pixel of the capture frame is unaccounted for by the port's
own output, and the oldest screen is at the top. That is the two-frame splice
class (the repo's model of a 70.09 Hz capture against the 60.05 Hz game) one
present wider: the capture scan raced three presents at the ending's
mode-`0x0D` → `0x0C` transition.

The measured tests, re-run against the kept 4940-frame dump
(`GP_WIN_KEEP=1 make gp-u13-oracle`; the `§P1.8` loaders):

| test | result |
|---|---|
| every row byte-equal to its segment frame | **yes** — 0 rows differ from {3549 for 0..45, 3550 for 46..96, 3551 for 97..199} |
| differing bytes held by no port frame | **none** |
| clean (whole frame) | no |
| best adjacent-pair splice | no — 24005 differing bytes at N=3550 (pair 3550/3551); best prefix+suffix 103 of 199 rows at N=3550 |
| transition (prefix + one row + suffix) | no |
| DAC-index constant offset | no — `∩K` EMPTY over all 22923 differing px; 2810 px have no offset in ±64; best single `k` is +1 at 1393/22923 (6.1 %) |
| a poke replacement (class B) | no — the nearest `W` is `W ms=112474 f=152A step=14 addr=00104B0C len=1 was=00 now=01`, the scenario's one-byte step write (the same address at f=1502, 1516, 152A, 153E, …), replayed by the port at `W.f+1`; the three screens are the port's own |
| port state at the frame | trace 0 differing through 10013; win 0 differing; path 30/30 (the `f=152B` milestone is "final opponent 3 replaced (0x274FC, count 3)") |

The DAC test is §P1.4's measure re-run to the stated values (intersection
empty, 2810/22923 pixels with no offset, +1 at 1393/22923); it rules out the
§W1 palette-bank dword-carry signature, whose fingerprint is one constant
offset covering every differing pixel. The byte-ownership row is the decisive
one: a real divergence (class A) would put bytes on the capture's screen that
none of the port's own frames holds; 4248 holds none. Class B is out because
the frame's bytes are the port's own presents and the only nearby `W` is
replayed on both sides.

The neighbours make the shape visible:

| capture | raw | class | nearest port (`frames.txt`) |
|---|---:|---|---|
| 4246 | 7865 | clean | 3548 (`f=152B tick=000015B4 mode=000D`) |
| 4247 | 7886 | clean | 3549 (`f=152B tick=000015C7 mode=000D`) |
| **4248** | **7887** | **unexplained → the recipe** | 3551 (`f=152C tick=000015CB mode=000C`) |
| 4249 | 7888 | splice of (3551, 3552) at byte 179520 (row 187) | 3551 |
| 4250 | 7889 | clean | 3552 (`f=152D tick=000015CC mode=000C`) |
| 4251 | 7890 | clean | 3553 (`f=152E tick=000015CD mode=000C`) |
| 4252 | 7891 | splice of (3553, 3554) at bytes 15243..16182 | 3554 (`f=152F tick=000015CE mode=000C`) |

So the capture's raws 7865..7891 walk the port's presents in order — 4246
clean at 3548, 4247 clean at 3549, 4248 = 3549/3550/3551, 4249 =
3551/3552, 4250 clean at 3552, 4251 clean at 3553, 4252 = 3553/3554 — so
each AVI frame is a scan over the presents that landed during it, and 4248 is
the one that spans two changes. The port logs 3549 and 3550 under the same game
frame `f=152B` (and 3548 before them, also `f=152B`, tick `000015B4`); the
poll's records bracket them at `S ms=112474 f=152A mode=000D`,
`P ms=112802 f=152B`, `S ms=112803 f=152B mode=000C`,
`S ms=112806 f=152C mode=000C`. The capture's top-to-bottom order matches the
port's presentation order. The recipe binds only that order and the three
screens' bytes; it does not claim the presents' timing or the AVI frame's scan
rate (§P2.3).

### §P2.2 The recipe (exact)

`tools/gp_compare.py`'s `SCAN3_ALLOWED['gp-u13-ending2']` is

```python
((4248, 46, 97,
  'fc1044a6a9e5bb6cee142ce02811c4a4823585747351089bd1514a76685fc6e6'),)
```

and `scan3_explain(capture 4248, dump, entry)` returns `([3549, 3550, 3551],
None)`.

| segment | rows | bytes | bound by | port holders |
|---|---:|---:|---|---|
| top | 0..45 | 44160 | sha256 `fc1044a6…` and `scan3_top`: at least one port frame `A < B` holds it | 3549 only (measured) |
| middle | 46..96 | 48960 | the unique adjacent pair `(B, B+1) = (3550, 3551)`: rows 46..96 = port 3550's | 3550 only |
| bottom | 97..199 | 98880 | the same pair: rows 97..199 = port 3551's | 3551 only |

Measured details:

- the maximal runs of identical holder sets are exactly the three segments
  (rows 0..45 `[3549]`, 46..96 `[3550]`, 97..199 `[3551]`); the split is
  derived from the runs, not chosen;
- `scan3_bind(c, dump, 46, 97)` is `3550` — one hit over the 4940-frame dump,
  so the pair is unique;
- `scan3_top(c, dump, 3550, 46)` is `[3549]` — one earlier holder. Port 3548
  (also `f=152B`, tick `000015B4`, mode `000D`, the clean match of capture
  4246) differs from 3549 in 152 rows (13335 px), so the top segment is not a
  repeat of 3548;
- `band_pair(c, dump, 46, 97)` is `None` (the BANDS shape does not apply: no
  single adjacent pair holds rows 0..45 and 97..199);
- `frames.txt` for the three bound frames: `03549 f=152B tick=000015C7
  mode=000D`, `03550 f=152B tick=000015CA mode=000C`, `03551 f=152C
  tick=000015CB mode=000C`; the capture frame's nearest port by row hash is
  3551 with diff rows 0..96, x 0..319 (22923 px).

### §P2.3 The named limits

Stated exactly as the existing SCAN3 entries' and §G6's limit:

- **The top is not uniquely bound.** The recipe binds rows 0..45 by their
  sha256 *and* by the existence of one earlier holder `A < B`
  (`scan3_top`), not by its identity. Measured here, 3549 is the only holder,
  but the recipe would also accept a port that renders those 44160 bytes in
  another earlier frame; the top's 46 rows are capture-pinned bytes with no
  port-side uniqueness claim. (The whole screen the top belongs to is claimed
  by capture 4247, clean at 3549, as the other entries' old screens are claimed
  by their own captures.)
- **The row splits 46/97 are capture-pinned.** The recipe binds them as the
  boundaries where the capture's bytes change holder; it says nothing about
  the presents' timing, the scan-out's row rate, or why three presents landed
  inside one capture frame. A port that presents the same three screens in the
  same order at a different cadence passes the recipe (the same limit §G6
  records).
- **The middle and bottom are bound byte-for-byte to the current dump**:
  mutating port 3550's rows 46..96 or port 3551's rows 97..199 (or adding a
  second pair that binds them) un-explains the frame. The middle range is not
  vacuous (`r1 < r2`), so both `B` and `B+1` are exhibited.
- **The recipe is for capture 4248 only** and claims nothing about 4320, 5120
  or the post-script tail — they are Task 3's and §P1.6's.

### §P2.4 The test

`tools/tests/test_gp_compare.py`'s
`Scan3Allowance.test_u13_scan3_recipe_is_pinned_and_mutations_fail`:

1. pins the table tuple exactly (capture 4248, r1 46, r2 97, top sha
   `fc1044a6…`), so a silent re-measurement of the claim fails the test;
2. runs the fixture with the class's own entry `(1, 2, 4, sha(rgb(1)[:2 rows]))`
   and asserts it is explained (the table tuple above stays pinned separately);
3. re-runs it with `(1, 2, 4, '0'*64)` — a different top — and asserts the
   recipe refuses with `its top rows 0..1 are …` and the frame is unexplained
   (rc 1, first 1);
4. re-runs it with `(1, 2, 3, self.sha)` — a split the unique pair does not
   bind (`r2` moved) — and asserts `no unique adjacent port pair …` and the
   same un-explanation.

The fixture's own three-segment mutation (`test_a_mutated_bound_pair_frame_
unexplains_it_again`, port 1's bytes changed) and the top-holder mutation
(`test_a_mutated_top_holder_is_named`) already prove the port-side and top-side
sensitivity of the same code path; they run with the fixture entry, so this
test adds the pinned U13 tuple and the tuple mutations without duplicating
them.

### §P2.5 Commands, output, and no pin moved

```bash
GP_WIN_KEEP=1 make gp-u13-oracle
python3 tools/gp_compare.py --report --scenario gp-u13-ending2 \
  --capture data/k11-captures/gp-u13-ending2 \
  --port /tmp/pr_gp_dump/gp-u13-ending2 --max-unexplained 0
python3 -m unittest tools.tests.test_gp_compare
```

`make gp-u13-oracle` exited 0 (PASS) at the unchanged pins; the new allowance
line and the verdicts:

```text
gp_compare: gp-u13-ending2: frames: captured frame 4248 (raw 7887) allowed by name as a three-frame scan-out: top rows 0..45 held by port 3549, port 3550 rows 46..96, port 3551 rows 97..199 the named bytes
gp_compare: gp-u13-ending2: frames: window from capture 96 (raw 1739); 4211 classified: 2600 clean, 1605 splice, 3 transition, 0 band-allowed, 1 scan3-allowed, 1 overlay-allowed, 1 unexplained, 14 all-black
gp_compare: gp-u13-ending2: frames: FIRST UNEXPLAINED capture 4320 (raw 8019): nearest port 3615, rows 4..107, x 42..280 (273 px)
gp_compare: gp-u13-ending2: frames: first unexplained 4320, ratchet N 4248 ok (improved: raise N)
gp_compare: gp-u13-ending2: frames: window reach port 3613 > pinned reach 3549 (improved: raise the pin)
gp_compare: gp-u13-ending2: trace: 0 differing through 10013; ratchet N 10014 ok
gp_compare: gp-u13-ending2: path: 0 not reproduced through 29; ratchet N 30 ok
gp_compare: gp-u13-ending2: win: 0 differing through 10013; ratchet N 10014 ok
gp_win: gp-u13-ending2: evidence: 30/30 milestones ok
```

The uncapped report exited 0 with 4248 explained and 4320 as the first
unexplained; 5120 is still listed (Task 3):

```text
gp_compare: gp-u13-ending2: frames: captured frame 4248 (raw 7887) allowed by name as a three-frame scan-out: ...
gp_compare: gp-u13-ending2: frames: window from capture 96 (raw 1739); 5786 classified: 3492 clean, 2270 splice, 7 transition, 0 band-allowed, 1 scan3-allowed, 1 overlay-allowed, 15 unexplained, 22 all-black
gp_compare: gp-u13-ending2: frames: FIRST UNEXPLAINED capture 4320 (raw 8019): nearest port 3615, rows 4..107, x 42..280 (273 px)
gp_compare: gp-u13-ending2: frames: UNEXPLAINED capture 5120 (raw 8839): nearest port 4317, rows 1..4, x 21..298 (13 px)
gp_compare: gp-u13-ending2: frames: UNEXPLAINED capture 5891 (raw 14361): nearest port 0, rows 0..199, x 0..319 (55510 px)
... 5892..5903 ...
gp_compare: gp-u13-ending2: trace: 0 differing through 10013
gp_compare: gp-u13-ending2: moves: 0 differing through 10013
```

(Before this task the same command printed 16 unexplained with `FIRST
UNEXPLAINED capture 4248 … 22923 px` and `0 scan3-allowed`; now 15, first 4320,
`1 scan3-allowed`.)

`python3 -m unittest tools.tests.test_gp_compare` ran 64 tests: `OK`.

**No pin moved.** `GP_U13_MIN_FIRST` stays 4248 (the oracle now says
"improved: raise N"; Task 4 re-pins after 4320/5120), `GP_U13_MAX_START` stays
96, `GP_U13_TRACE_MIN_FIRST`/`GP_U13_WIN_MIN_FIRST` stay 10014,
`GP_U13_MIN_REACH` stays 3549 (reported 3613; Task 4 re-pins), and the capture
identity pins (`e581916f…2b69c6`, 5904 frames) are unchanged. The Makefile is
not touched.

### §P2.6 Files and commit

`tools/gp_compare.py` (the new `SCAN3_ALLOWED` entry and its comment),
`tools/tests/test_gp_compare.py` (the new test), this record. No `port/` code,
no `data/`, no Makefile, no `docs/PROGRESS.md` (Task 4 owns the U13 line). The
task report is the git-ignored ledger.

## §P3 4320 and 5120: 4248's class, and a four-row top band

### §P3.1 The report after Task 2 (Step 1)

```bash
GP_WIN_KEEP=1 make gp-u13-oracle
python3 tools/gp_compare.py --report --scenario gp-u13-ending2 \
  --capture data/k11-captures/gp-u13-ending2 \
  --port /tmp/pr_gp_dump/gp-u13-ending2 --max-unexplained 0
```

At Task 2's tree the report was 15 unexplained: `4320`, `5120` and
`5891..5903` (the tail), with 4248 explained by name. Both frames are single
anomalies whose neighbours are clean or splices (§P1.3's table), the trace and
WIN fields agree through 10013 and the path is 30/30, so neither is a state
difference. They are classified below from the bytes, not from the state.

### §P3.2 4320 — class C, the same three-present scan-out as 4248, split forced

Capture 4320 (raw 8019) sits at the ending's *second* mode-`0x0D` → `0x0C`
transition: `S ms=114172 f=1566 mode=000D` (poll line 5534) →
`S ms=114683 f=1567 mode=000C` (line 5537). The nearest port by row hash is
3615 (`f=1568 tick=00001641 mode=000C`), 273 differing pixels in rows 4..107,
x 42..280.

The capture's rows are byte-equal to three *consecutive* port presents:

| segment | rows | bytes | held by | `frames.txt` |
|---|---|---:|---|---|
| top | 0..17 | 17280 | port 3613 **only** before 3614 | `03613 f=1567 tick=0000163C mode=000D` |
| middle | 18..107 | 86400 | port 3614 (the whole range is 3614's alone) | `03614 f=1567 tick=00001640 mode=000C` |
| bottom | 108..199 | 88320 | port 3615 (the whole range is 3615's alone) | `03615 f=1568 tick=00001641 mode=000C` |

Row-holder evidence (per-row holders over 3600..3700; the full dump's
`scan3_bind`/`scan3_top` agree):

```text
rows  0.. 2  48 holders incl. 3613          (the old and new screens agree)
rows  3, 5   22 holders incl. 3613, 3614    (both agree)
row   4      12 holders incl. 3613, not 3614
rows  6.. 8  {3611, 3612, 3613, 3633}       (3614 NOT a holder: old content)
row   9      {3611, 3612, 3613}
rows 10..11  {3606..3613} / {3613}
rows 12..13  7 holders incl. 3613, 3614     (both agree)
rows 14..17  {3613, 3692, 3693, 3694}       (3613 only among the three presents)
rows 18..32  {3614, 3615, 3616, 3695..3697} (3613 NOT a holder: new content)
rows 33..87  8..59 holders incl. 3614, 3615
rows 88..107 {3614} only
rows 108..189 {3615} only
rows 190..199 {3614, 3615}
```

**The split is forced by the bytes, not chosen.** For `(B, B+1) = (3614,
3615)`: 3614 does not hold rows 14..17, so `r1 >= 18`; 3613 does not hold row
18, so no single frame holds rows 0..18 and `r1 <= 18` — hence `r1 = 18`.
3615 does not hold rows 88..107, so `r2 >= 108`; 3614 does not hold rows
108..189, so `r2 <= 108` — hence `r2 = 108`. `scan3_bind(c, 18, 108)` is 3614,
one hit over the 4940-frame dump, and `scan3_top(c, 3614, 18)` is `[3613]`, one
earlier holder (also unique here, unlike 4248's). `scan3_explain` returns
`([3613, 3614, 3615], None)`.

The 273 differing pixels are exactly the two bands where the three presents
differ: rows 4..17 hold 120 of them (1, 6, 6, 9, 11, 5, 1, 9, 18, 25, 29 for
rows 4, 6, 7, 8, 9, 10, 11, 14, 15, 16, 17 — rows 5, 12 and 13 hold none) and
rows 88..107 hold 153 (4, 7, 4, 4, 3, 4, 8, 5, 5, 4, 2, 8, 16, 8, 6,
11, 9, 16, 12, 17); no differing pixel lies outside those two bands, and none
of them is unheld (§P1.5's holder buckets: 0 unheld). The DAC-index test is
negative as §P1.4 measured (`∩K` empty; 55 of 273 pixels have no offset in
±64), so this is content, not a palette shift.

The neighbours walk the presents in order, so the capture scan raced three
presents across this transition as it did at 4248: 4319 clean at 3613, **4320 =
3613/3614/3615**, 4321 a splice of (3615, 3616) at bytes 129600..129711 (row
135), 4322 clean at 3616.

**Correction to §P1.5's derivation method (raw wins).** §P1.5 derived 4320's
split as `(88, 108)` from the *maximal runs of identical holder sets* and then
reported that the top condition fails. The runs are not the segments here: the
background is static, so rows 18..87 recur with many different holder sets
(6, 47, 8, 58, 59 holders, §P1.5's list) and the run that ends at 87 is not the
present's boundary. The segment boundary is where the *present's bytes* change
(rows 14..17 vs 18), which is what the forced-split inequalities above use.
`(88, 108)` remains a valid *pair* binding (the middle is a sub-range of 3614's
rows) but there is no single top holder; `(18, 108)` is the only split with one.

### §P3.3 5120 — class C, a four-row band at the top of the raster

Capture 5120 (raw 8839) is inside the ending's mode-`0x15` fade: `S
ms=126110 f=1815 mode=0015` (line 6228) / `S ms=126125 f=1816` (line 6229) on
the raw clock, and `S ms=126360 f=1824` (line 6243) / `S ms=126383 f=1825`
(line 6244) after §P1.2's +269 ms lead — the fade's step. Its neighbours:
5119 clean at port 4315 (`f=1824 tick=000018FD mode=0015`), 5121 a splice of
(4317, 4318) at bytes 142614..143174, 5122 clean at 4318. The
nearest is 4317 (`f=1826 tick=000018FF mode=0015`), 13 differing pixels in
rows 1..4, x 21..298.

Row-holder evidence over the whole 4940-frame dump:

```text
rows with NO holder (measured over all frames): 1, 2, 3, 4  (and no other row)
row 0           held by 4316, 4317, 4318 (identical there)
rows 5..114     held by exactly 4316, 4317, 4318
rows 116..138   held by exactly 4316, 4317, 4318
row 115         a blank row, 120 holders incl. 4317
rows 139..199   every holder set contains 4317; rows 142..144, 146..152, 157,
                159..168, 170..171, 173..193, 195..199 are 4317's alone
```

so the unique adjacent pair is **(4316, 4317)**: `band_pair(c, 1, 5)` = 4316
(one hit over the dump, i.e. no other frame holds all 195 rows 5..199) and the
band is the unheld run's ends, rows 1..4
(1280 pixels, 3840 bytes; sha256 `56b900b1…`). The unheld rows are *only* 1..4,
which is why the band is 1..4 and not wider. The bounds are still
**capture-pinned, not machine-forced**: the shape itself also accepts wider
bands (`band_pair` returns 4316 for `(0, 6)`, `(0, 7)`, … as well), so the
`1/5` choice follows the unheld run's ends exactly as the U11 entry's `81/92`
does — a named limit, not an enforced property.

The 13 differing pixels, with port 4317's own index and DAC entry:

| pixel | capture | port RGB | port index | DAC entry | count of that index in the frame |
|---|---|---|---:|---|---:|
| (1,23) (1,24) (1,295) (1,296) (2,22) (2,297) (3,21) (3,298) | `00001c` | `000000` | `0x32` | `000000` | 101 (93 black in the capture) |
| (3,74) | `000000` | `100000` | `0x2E` | `100000` | 96 (95 match) |
| (4,27) (4,28) (4,291) (4,292) | `000000` | `00000c` | `0x4D` | `00000c` | 54 (50 match) |

**Correction to §P1.4/§P1.9's DAC reading (raw wins).** The intersection of the
per-pixel offset sets over the *differing* pixels is `{-29, +14, +29}`, but that
is a DAC **alias**, not a palette shift: `00001c` occurs at five entries
(0x15, 0x40, 0x42, 0x43, 0x4F) and black at 207. The discriminating
measure is frame-wide, over all 64000 pixels:

| offset `k` | differing pixels of 64000 |
|---|---:|
| **0** | **13** |
| +3 | 12651 |
| +1 | 13451 |
| −1 | 13504 |
| +23 | 14445 |
| −29 (covers the 13) | 19159 |
| +14 (covers the 13) | 17685 |
| +29 (covers the 13) | 23552 |

(the table is the ±64 domain of §P1.4's test; over **all 256** offsets the best
non-zero is `k = +109` at 11282, then +81 at 11372, +82 at 11497 and +80/+90 at
11594, so the conclusion does not depend on the domain.)

`k = 0` wins by three orders of magnitude, so the frame is **not** a
palette-index shift of port 4317's index buffer; the three covering offsets do
not survive the frame. §P1.9's "positive" verdict was the alias, and the U10
4113 signature it was read as is **not** present here (4113's offset was unique
and frame-wide).

The per-index check of the whole frame finds exactly three indices with outlier
pixels (0x2E 1 of 96, 0x32 8 of 101, 0x4D 4 of 54); every other index of the
frame matches its DAC entry. And the band is not a palette state either: no
dumped frame's DAC renders index 0x32 as `00001c` (0 of 4940), 93 other pixels
with index 0x32 are black in this very capture frame, and rotating 4317's whole
DAC by +29 (the alias that fits the 13) breaks 156/174/170/189/192 pixels of
rows 0..4.

What the band is *not* is a content difference the port draws: the port's
content at the 13 pixels is corroborated on both sides by the neighbours' own
trajectories — capture 5119 (clean at 4315) shows exactly
`DAC_4315[0x32]/[0x2E]/[0x4D] = 000000/180000/000014` at those pixels and
capture 5122 (clean at 4318) shows `000000/100000/00000c`, while 5120's band
shows `00001c`/black/black. So rows 1..4 were captured in a state no present
holds, sitting between two presents the capture matches exactly.

**The named gap.** The *cause* of the four-row state is not isolated by the
available artifacts: it is either a sub-raster transient of the original's top
four rows (the palette entries 0x2E/0x32/0x4D are the three the band's pixels
use; the fade steps between 4315 and 4316) or a present state the replay never
dumped (§K.10 item 2's class). No dumped DAC, no dumped present and no
whole-frame offset reproduces it, so the record names the gap instead of
choosing a mechanism. The allowance below therefore binds the band's bytes,
not a cause.

### §P3.4 The recipes (exact)

`tools/gp_compare.py`:

```python
BANDS_ALLOWED['gp-u13-ending2'] = (
    (5120, 1, 5,
     '56b900b1562b3abdf600bb678e31abed485327c292ea26eb022fe7daf0e32250'),)
SCAN3_ALLOWED['gp-u13-ending2'] = (
    (4248, 46, 97,
     'fc1044a6a9e5bb6cee142ce02811c4a4823585747351089bd1514a76685fc6e6'),
    (4320, 18, 108,
     'c8fa5b9305b132f09d1853594b40429750636f08da70c5e154dc5945ce4b8cb4'))
```

| capture | class | top bound by | pair | bottom |
|---|---|---|---|---|
| 4320 | three-frame scan-out | rows 0..17 sha `c8fa5b93…` **and** the unique earlier holder 3613 (`scan3_top`) | (3614, 3615), `scan3_bind` one hit | rows 108..199 = 3615, byte-verified |
| 5120 | band splice | rows 0..0 = port 4316, byte-verified | (4316, 4317), `band_pair` one hit | rows 5..199 = port 4317, byte-verified |

No class is extended and no byte is fitted: 4320 uses the existing
`SCAN3_ALLOWED` shape (a pinned top hash plus a unique adjacent pair, with the
earlier-holder condition), 5120 uses the existing `BANDS_ALLOWED` shape (a
pinned band hash plus a unique adjacent pair). Both entries are per-frame and
per-bytes.

### §P3.5 The tests and the mutation proofs

`tools/tests/test_gp_compare.py`:

- `Scan3Allowance.test_u13_scan3_recipe_is_pinned_and_mutations_fail` now pins
  **both** U13 tuples (4248 and 4320), with the split-forcing argument in its
  comment; the fixture sub-tests already prove the mechanism (a changed top
  hash, a changed split and a mutated bound frame each un-explain the fixture).
- `BandAllowance.test_the_table_names_the_measured_u13_frame` is new: it pins
  the 5120 tuple exactly (frame 5120, top_row 1, band_hi 5, band sha
  `56b900b1…`), so a silent re-measurement fails. The class's existing
  fixture sub-tests prove the mechanism (a mutated band byte, a mutated bound
  frame and an ambiguous pair each un-explain the fixture).

`python3 -m unittest tools.tests.test_gp_compare` ran 65 tests (Task 2's tree:
64): `OK`.

Mutation proof on the real dump (in memory, no file writes; the same calls the
claim makes):

```text
4320 unmutated: ([3613, 3614, 3615], None)
4320 with port 3614 row 50 mutated:   no unique adjacent port pair binds its rows [18,108) and [108,200)
4320 with port 3615 row 150 mutated:  no unique adjacent port pair binds its rows [18,108) and [108,200)
4320 with port 3613 row 6 mutated:    no port frame before 3614 holds its named top rows
4320 with the capture top rows mutated: its top rows 0..17 are ed49ff290d46.., not the named c8fa5b9305b1..
5120 unmutated: (4316, None)
5120 with port 4317 row 50 mutated:   no unique adjacent port pair binds its rows [0,1)/[5,200)
5120 with port 4316 row 0 mutated:    no unique adjacent port pair binds its rows [0,1)/[5,200)
5120 with a capture band byte mutated: its band rows 1..4 are 9bf67ed38238.., not the named 56b900b1562b..
```

### §P3.6 Commands, output, and no pin moved

```bash
GP_WIN_KEEP=1 make gp-u13-oracle            # rc 0
python3 -m unittest tools.tests.test_gp_compare
python3 tools/gp_compare.py --report --scenario gp-u13-ending2 \
  --capture data/k11-captures/gp-u13-ending2 \
  --port /tmp/pr_gp_dump/gp-u13-ending2 --max-unexplained 0
```

The oracle's frame lines (the enforced run, `rc=0`):

```text
gp_compare: gp-u13-ending2: frames: captured frame 4248 (raw 7887) allowed by name as a three-frame scan-out: top rows 0..45 held by port 3549, port 3550 rows 46..96, port 3551 rows 97..199 the named bytes
gp_compare: gp-u13-ending2: frames: captured frame 4320 (raw 8019) allowed by name as a three-frame scan-out: top rows 0..17 held by port 3613, port 3614 rows 18..107, port 3615 rows 108..199 the named bytes
gp_compare: gp-u13-ending2: frames: captured frame 5120 (raw 8839) allowed by name as a band splice: port 4316/4317 rows 0..0/5..199, band rows 1..4 the named bytes
gp_compare: gp-u13-ending2: frames: window from capture 96 (raw 1739); 5774 classified: 3492 clean, 2270 splice, 7 transition, 1 band-allowed, 2 scan3-allowed, 1 overlay-allowed, 1 unexplained, 22 all-black
gp_compare: gp-u13-ending2: frames: FIRST UNEXPLAINED capture 5891 (raw 14361): nearest port 0, rows 0..199, x 0..319 (55510 px)
gp_compare: gp-u13-ending2: frames: first unexplained 5891, ratchet N 4248 ok (improved: raise N)
gp_compare: gp-u13-ending2: frames: window reach port 4939 > pinned reach 3549 (improved: raise the pin)
gp_compare: gp-u13-ending2: trace: 0 differing through 10013; ratchet N 10014 ok
gp_compare: gp-u13-ending2: win: 0 differing through 10013; ratchet N 10014 ok
gp_win: gp-u13-ending2: evidence: 30/30 milestones ok
```

The uncapped report (`--max-unexplained 0`, exit 0) now lists **13**
unexplained — `5891..5903` only, the post-script tail §P1.6 records — and
first-unexplained 5891:

```text
gp_compare: gp-u13-ending2: frames: window from capture 96 (raw 1739); 5786 classified: 3492 clean, 2270 splice, 7 transition, 1 band-allowed, 2 scan3-allowed, 1 overlay-allowed, 13 unexplained, 22 all-black
gp_compare: gp-u13-ending2: frames: FIRST UNEXPLAINED capture 5891 (raw 14361): nearest port 0, rows 0..199, x 0..319 (55510 px)
...
gp_compare: gp-u13-ending2: frames: UNEXPLAINED capture 5903 (raw 14417): nearest port 0, rows 0..199, x 0..319 (32421 px)
gp_compare: gp-u13-ending2: trace: 0 differing through 10013
gp_compare: gp-u13-ending2: moves: 0 differing through 10013
```

(Before this task the same command printed 15 unexplained with `FIRST
UNEXPLAINED capture 4320 … 273 px` and `UNEXPLAINED capture 5120 … 13 px`; now
13, first 5891, `1 band-allowed`, `2 scan3-allowed`.) 5891..5903 are not
claimed: they start after the replay's end (§P1.6).

**No pin moved.** The Makefile's U13 values are untouched and the enforced run
passes at them:

```text
GP_U13_MIN_FIRST = 4248          (reported 5891; "improved: raise N" — Task 4 re-pins)
GP_U13_TRACE_MIN_FIRST = 10014   (0 differing through 10013)
GP_U13_MAX_START = 96            (window starts at capture 96 exactly)
GP_U13_MIN_REACH = 3549          (reported 4939; "improved: raise the pin")
GP_U13_CAPTURE_SHA256 = e581916fa0a8e23a2ea52519aa0b358e4bc63084ce9defa0e1f733f3d12b69c6
GP_U13_CAPTURE_FRAMES = 5904
GP_U13_MILESTONES = 30           (30/30)
GP_U13_WIN_MIN_FIRST = 10014     (0 differing through 10013)
```

### §P3.7 Files and commit

`tools/gp_compare.py` (the `BANDS_ALLOWED['gp-u13-ending2']` entry and the
second `SCAN3_ALLOWED['gp-u13-ending2']` tuple, each with its comment),
`tools/tests/test_gp_compare.py` (the extended pin and the new band test), this
record. No `port/` code, no `data/`, no Makefile. The task report is the
git-ignored ledger.

## §P4 The U13 re-pin: `MIN_FIRST` 5891, `MIN_REACH` 4939, enforced

Task 4 of the plan, armed by Tasks 1–3: after the named recipes, 4248, 4320 and
5120 are explained (§P2, §P3) and the first unexplained capture frame is 5891.
This task changes the Makefile pins and comment, this record and a short
`docs/PROGRESS.md` line; no `port/` code, no `tools/` change, no capture.

### §P4.1 Step 1 — the enforced reach

```bash
GP_WIN_KEEP=1 make gp-u13-oracle        # at the OLD pins (MIN_FIRST 4248, MIN_REACH 3549)
```

The enforced compare stops at the first unexplained, so its reach is computed
over the capture frames it classified before that frame. Its lines (rc 0; the
allowance lines are §P3.6's and unchanged):

```text
gp_compare: gp-u13-ending2: capture: poll.log sha256 e581916f..2b69c6, 5904 frames: matches the pin
gp_compare: gp-u13-ending2: frames: window from capture 96 (raw 1739); 5774 classified: 3492 clean, 2270 splice, 7 transition, 1 band-allowed, 2 scan3-allowed, 1 overlay-allowed, 1 unexplained, 22 all-black
gp_compare: gp-u13-ending2: frames: FIRST UNEXPLAINED capture 5891 (raw 14361): nearest port 0, rows 0..199, x 0..319 (55510 px)
gp_compare: gp-u13-ending2: frames: first unexplained 5891, ratchet N 4248 ok (improved: raise N)
gp_compare: gp-u13-ending2: frames: window reach port 4939 > pinned reach 3549 (improved: raise the pin)
gp_compare: gp-u13-ending2: trace: 0 differing through 10013; ratchet N 10014 ok
gp_win: gp-u13-ending2: evidence: 30/30 milestones ok
gp_compare: gp-u13-ending2: path: 0 not reproduced through 29; ratchet N 30 ok
gp_compare: gp-u13-ending2: win: 0 differing through 10013; ratchet N 10014 ok
```

The enforced reach is **4939**: what the enforced comparison printed, i.e. the
`p` after capture 5890, the highest port frame any classified capture frame in
its window exhibits. The brief warns against the report-only reach; here the two
coincide, and the reason is structural rather than a shortcut: the unexplained
run 5891..5903 is the capture's *last* 13 frames (5904 frames, indices 0..5903),
so report mode has no later classified frame that could raise `p`, and §P3.6's
report had already printed 4939. The pin is set to the enforced number the run
printed.

**The override form (measured).** `GP_WIN_KEEP=1 make …` sets a make variable,
which is how the target selects whether to keep the dump. A pin is different:
the Makefile assigns `GP_U13_MIN_FIRST = 4248` (not `?=`), so a makefile
assignment beats a variable that comes from the **environment**. Measured:

```bash
GP_U13_MIN_FIRST=5892 make -n gp-u13-oracle | grep -o -- '--min-first "[^"]*"'
# --min-first "5891"
make -n gp-u13-oracle GP_U13_MIN_FIRST=5892 | grep -o -- '--min-first "[^"]*"'
# --min-first "5892"
```

`GP_U13_MIN_FIRST=5892 make gp-u13-oracle` (the brief's literal environment
form) therefore ran at the real pin and **passed** (`first unexplained 5891,
ratchet N 5891 ok`, rc 0) — it proves nothing about the pin. The
failing-direction proofs below pass the pin on the **command line**, the form
GNU make lets override the makefile. The reach proof is the same.

### §P4.2 The pin table

| pin | old | new | provenance |
|---|---:|---:|---|
| `GP_U13_MIN_FIRST` | 4248 | **5891** | the first unexplained capture frame (raw 14361); 5891..5903 are the post-script tail (§P1.6) and are not claimed |
| `GP_U13_MIN_REACH` | 3549 | **4939** | the enforced comparison's port reach at that first unexplained (§P4.1) |
| `GP_U13_TRACE_MIN_FIRST` | 10014 | 10014 | 0 differing through 10013; the traced fields end there |
| `GP_U13_WIN_MIN_FIRST` | 10014 | 10014 | 0 differing through 10013; unchanged |
| `GP_U13_MAX_START` | 96 | 96 | the window starts at capture 96 exactly (the run prints `window from capture 96`) |
| `GP_U13_MILESTONES` | 30 | 30 | 30/30 (BLIZZARD `ad4=0, c0=1` included) |
| `GP_U13_CAPTURE_SHA256` | `e581916f…2b69c6` | unchanged | the capture identity line matches the pin |
| `GP_U13_CAPTURE_FRAMES` | 5904 | 5904 | matches the pin |

The capture identity (poll.log sha256
`e581916fa0a8e23a2ea52519aa0b358e4bc63084ce9defa0e1f733f3d12b69c6`, 5904
frames) is unchanged from the capture and from §J5: this task opens no capture.

### §P4.3 The failing direction

**(a) `MIN_FIRST` one above the measured value** — `make gp-u13-oracle
GP_U13_MIN_FIRST=5892`, rc 2 from make (the recipe's compare is rc 1). The
comparison's lines, verbatim (the four allowance lines it also printed are
§P4.4's and unchanged, and its `gp_win.py check` evidence lines are the
scenario's fixed 30-milestone block; the failing `gp_compare` short-circuits
the recipe's `&&` chain, so no `gp_win.py path` or `win` line is printed):

```text
gp_compare: gp-u13-ending2: frames: window from capture 96 (raw 1739); 5774 classified: 3492 clean, 2270 splice, 7 transition, 1 band-allowed, 2 scan3-allowed, 1 overlay-allowed, 1 unexplained, 22 all-black
gp_compare: gp-u13-ending2: frames: FIRST UNEXPLAINED capture 5891 (raw 14361): nearest port 0, rows 0..199, x 0..319 (55510 px)
gp_compare: gp-u13-ending2: frames: coverage (reported, not ratcheted): 69 non-black port frame(s) up to port 4939 not exhibited by any classified capture frame: [9, 31, 123, 164, 165, 267, 1128, 1129, 1443, 1516, 1716, 1720, 1724, 1728, 1732, 1736, 1740, 1744, 1748, 1752] ...
gp_compare: gp-u13-ending2: frames: FAIL: first unexplained 5891 < ratchet N 5892
gp_compare: gp-u13-ending2: trace: 9667 frames compared (f 130..), 43 without a capture snapshot; first tick difference f=131 (reported, not ratcheted)
gp_compare: gp-u13-ending2: trace: normalised (reported, not ratcheted): ent 0 of 9667 differ; t508 7857 of 9667 differ (first f=273)
gp_compare: gp-u13-ending2: trace: 0 differing through 10013; ratchet N 10014 ok
make: *** [Makefile:1282: gp-u13-oracle] Error 1
```

**(b) `MIN_REACH` one above the measured value** — `make gp-u13-oracle
GP_U13_MIN_REACH=4940`, rc 2 from make. The comparison lines, verbatim (the
four allowance lines, the coverage line and the two reported trace lines it
printed are elided; the four allowance lines are §P4.4's, unchanged):

```text
gp_compare: gp-u13-ending2: frames: window from capture 96 (raw 1739); 5774 classified: 3492 clean, 2270 splice, 7 transition, 1 band-allowed, 2 scan3-allowed, 1 overlay-allowed, 1 unexplained, 22 all-black
gp_compare: gp-u13-ending2: frames: FIRST UNEXPLAINED capture 5891 (raw 14361): nearest port 0, rows 0..199, x 0..319 (55510 px)
gp_compare: gp-u13-ending2: frames: first unexplained 5891, ratchet N 5891 ok
gp_compare: gp-u13-ending2: frames: FAIL: window reach port 4939 < pinned reach 4940 (the port exhibited fewer port frames)
gp_compare: gp-u13-ending2: trace: 0 differing through 10013; ratchet N 10014 ok
make: *** [Makefile:1282: gp-u13-oracle] Error 1
```

Both runs left the pins in the Makefile at 5891/4939 (the overrides are command
line only) and both removed the dump (no `GP_WIN_KEEP`), the cleanup path §P4.5
confirms.

### §P4.4 The enforced run at the new pins

```bash
GP_WIN_KEEP=1 make gp-u13-oracle          # rc 0
```

The run's allowance and ratchet lines (the 30 path lines between them are
§P3.6's, same capture/port pairs; the coverage line is elided):

```text
gp_compare: gp-u13-ending2: capture: poll.log sha256 e581916f..2b69c6, 5904 frames: matches the pin
gp_compare: gp-u13-ending2: frames: captured frame 2188 (raw 5047) allowed by name as a glyph overlay: 166 px of port 1799 over port 1800 (the mask from port 1798, the named glyph bytes)
gp_compare: gp-u13-ending2: frames: captured frame 4248 (raw 7887) allowed by name as a three-frame scan-out: top rows 0..45 held by port 3549, port 3550 rows 46..96, port 3551 rows 97..199 the named bytes
gp_compare: gp-u13-ending2: frames: captured frame 4320 (raw 8019) allowed by name as a three-frame scan-out: top rows 0..17 held by port 3613, port 3614 rows 18..107, port 3615 rows 108..199 the named bytes
gp_compare: gp-u13-ending2: frames: captured frame 5120 (raw 8839) allowed by name as a band splice: port 4316/4317 rows 0..0/5..199, band rows 1..4 the named bytes
gp_compare: gp-u13-ending2: frames: window from capture 96 (raw 1739); 5774 classified: 3492 clean, 2270 splice, 7 transition, 1 band-allowed, 2 scan3-allowed, 1 overlay-allowed, 1 unexplained, 22 all-black
gp_compare: gp-u13-ending2: frames: FIRST UNEXPLAINED capture 5891 (raw 14361): nearest port 0, rows 0..199, x 0..319 (55510 px)
gp_compare: gp-u13-ending2: frames: first unexplained 5891, ratchet N 5891 ok
gp_compare: gp-u13-ending2: trace: 0 differing through 10013; ratchet N 10014 ok
gp_win: gp-u13-ending2: evidence: 30/30 milestones ok
gp_compare: gp-u13-ending2: path: 0 not reproduced through 29; ratchet N 30 ok
gp_compare: gp-u13-ending2: win: 0 differing through 10013; ratchet N 10014 ok
```

`first unexplained 5891, ratchet N 5891 ok` (no `improved` suffix): the pin is
exact. The frame ratchet prints no window-reach line on equality —
`reach_check` speaks only for `<` (FAIL) and `>` (`improved: raise the pin`) —
so the reach pin's exactness is shown by §P4.3(b): 4940 fails against 4939.
The run's miss set is unchanged (`fn-miss PR_GP_DUMP distinct=4 dropped=0`:
`0x5D812` actor_spawn/set_dead, `0x29D60`, `0x5D812` frontend_mode_1b_step).

The claim stays as narrow as the other gp scenarios' (AGENTS.md's gp-oracle
note): no content-bearing capture frame in the window is unexplained and the
window reaches its end, but port-frame order, under-rendering *inside* the
window, and the 5891..5903 post-script tail are not claimed.

### §P4.5 The cleanup path

```bash
make gp-u13-oracle                        # without GP_WIN_KEEP: rc 0
```

The same lines as §P4.4, rc 0, and the target's trailing
`[ -n "$(GP_WIN_KEEP)" ] || rm -rf $(GP_DUMP)/gp-u13-ending2` runs:

```text
$ ls -d /tmp/pr_gp_dump/gp-u13-ending2
ls: cannot access '/tmp/pr_gp_dump/gp-u13-ending2': No such file or directory
```

### §P4.6 The Makefile comment

The U13 block's measurement paragraph now reads (addresses and numbers as
measured above): the window starts at capture 96 (raw 1739); every
content-bearing capture frame through 5890 is explained by name — 2188 by the
loader's glyph overlay, 4248 (raw 7887) and 4320 (raw 8019) as three-present
scan-outs, 5120 (raw 8839) as a four-row band (record §P1, §P2, §P3); the first
unexplained is 5891 (raw 14361) and 5891..5903 are the post-script tail (the
replay's dump ends at f=0x271B and its trace at f=0x271D, X f=10013; §P1.6) and
are not claimed, so `MIN_FIRST = 5891` and 5892 fails; the enforced
comparison's window reach is port 4939, so `MIN_REACH = 4939` and 4940 fails;
trace and win fields agree through f=10013, so their end pins are 10014; 30/30
milestones; the capture has 5904 frames with this poll.log sha256 (unchanged
since the capture). The old `4248`/`port 3549`/`report mode` provenance is
gone.

### §P4.7 Files and commit

`Makefile` (`GP_U13_MIN_FIRST` 4248 → 5891, `GP_U13_MIN_REACH` 3549 → 4939 and
the comment), this record, `docs/PROGRESS.md` (one short U13 line; Task 12 owns
the phase paragraph). No `port/` code, no `tools/` change, no `data/`, no
capture. The task report is the git-ignored ledger.

## §P5 The frontend stub pair: `0x5D812` and `0x28D68`

Task 5 of the plan, and the differential track's baseline: two rows, no `port/src` change. `0x5D812`
is a binding-only row (the port does not register the address; the plan forbids a header or
`fn_register` for it) and `0x28D68` is the port's `frontend_char_screen_hook` (`flow.c:976`).
Together they close the eight rows whose recorded callees included them. Everything below was
measured at base commit `04e3f7ed` in the worktree `pixel-and-proof-closure`, with the driver
command the plan fixes:

```bash
python3 tools/diff_verify.py --diffrun build/diffrun --exe data/game/C/PRAGE.EXE \
  --image /tmp/pr_diff_image.bin --table /tmp/pr_diff_table.md --self-check
```

### §P5.1 The baseline line

```text
diff-verify: 1019/1019 functions VERIFIED; 14956/14956 mutants detected; 0 named gaps; 774/833 rows with callees closed (186 have none); 768/833 closed on full rows; 8 ported functions with no row. ...
```

(The sentence continues with the fixed closure-rule explanation, unchanged in every run; the table
is `/tmp/pr_diff_table_baseline.md`, 15975 rows = 1019 real + 14956 mutants.) All 1019 real rows are
`VERIFIED` (30 carry a `reads outside the image` note); the 59 open rows are exactly 833 − 774,
and the baseline census contains the §J1 blockers plus the two this task removes: `0x5D812 real`
(8 rows: `attract_scene_tick`, `frontend_mode_15_step` … `frontend_mode_1b_step`) and `0x28D68 allow`
(6 rows: `frontend_mode_16_step` … `frontend_mode_1b_step`).

### §P5.2 `0x5D812` — `xor eax,eax; ret`, binding only

The raw file bytes at offset `0xB0666` are `33 C0 C3`; the image the harness runs holds the same
bytes at `0x5D812` (no fixup record touches them), and `0x5D815` (`push ebx`) is the next function.
The port registers nothing at the address: `fn_resolve` reports the miss to the harness's seam, and
every caller elides the call (`actors.c:943/3062/3893`, `flow.c:1060`), which is why the row is a
`port/tests/diff_runner.c` binding and not a C function.

The spec (`tools/diff_verify.py`, `P5_SPECS`):

```python
Spec("runtime_5d812", 0x5D812, [
    Case("r0", {"eax": 0x00000000}),
    Case("r1", {"eax": 0x00C0FFEE}),
], eax_mask=0xFFFFFFFF, mutants=("@ret",))
```

`r0` is the zero boundary; `r1` is the case that makes the `xor` observable. The mutant
`runtime_5d812@ret` is the `ret` without the `xor` (EAX passed through), and the measured
discrimination is one case, one kind:

```text
runtime_5d812@ret MISMATCH 1 / 1
    r1: eax (mask 0xFFFFFFFF): original 0x0, port 0xC0FFEE
```

This is why the case set is two cases and not the plan's single `{"eax": 0}`: with EAX = 0 at entry
a pass-through binding is indistinguishable from the original, and the row could not fail for the
one implementable error (a forgotten clear). The binding (`b_p5_5d812`) is `*eax = 0u` and touches
no memory; the mutant (`m_p5_5d812_ret`) is `*eax = r[R_EAX]`.

Measured, `--function runtime_5d812 --self-check`:

```text
| runtime_5d812 | 0x5D812 | 2 | 1/1 | VERIFIED | - |
| runtime_5d812@ret | 0x5D812 | 2 | 1/1 | MISMATCH | - |
```

then the full driver with only this row added:

```text
diff-verify: 1020/1020 functions VERIFIED; 14957/14957 mutants detected; 0 named gaps; 776/833 rows with callees closed (187 have none); 770/833 closed on full rows; 8 ported functions with no row. ...
```

Two rows closed — `774 → 776` closed, `768 → 770` full, and one row without callees (`186 → 187`)
because the new row records none. The two are the ones whose only open blocker was `0x5D812`:

```text
| attract_scene_tick   | 0x292AC | 6 | 6/6 | VERIFIED | 4F1D0 stub VERIFIED, 4F7F4 stub VERIFIED, 5D812 real unverified |
| frontend_mode_15_step| 0x4F24C | 10 | 9/9 | VERIFIED | 4F790 stub VERIFIED, 5D812 real unverified |
```

Their final rows read `5D812 real VERIFIED` and no other callee changed. The six
`frontend_mode_16_step` … `1b_step` rows still name `28D68 allow unverified` and wait for §P5.3.

### §P5.3 `0x28D68` — `frontend_char_screen_hook`

The function's bytes at `0x28D68` are `52 BA 38 37 04 00 B8 10 00 00 00 89 15 E4 4A 10 00 E8 02 6C
02 00 5A C3` (`push edx; mov edx,0x43738; mov eax,0x10; mov [0x104AE4],edx; call 0x4F980; pop edx;
ret`, EDX preserved by the push/pop, EAX left at 0x10 by the non-clobbering callee). The file bytes at
offset `0x7BBBC` are pre-fixup (`0x33738`/`0x84AE4`); `mem_load_le_fixups` rewrites both to
`0x43738`/`0x104AE4`, which is what the harness runs. The port body is the same two effects
(`DSD(DS_00104AE4) = FN_00043738; frontend_wipe_arm(0x10u);`).

The spec:

```python
Spec("frontend_28d68", 0x28D68, [
    Case("h0", {"eax": 0}, {0x00104AE4: le32(0x0005D812) + le32(0xE8E8E8E8)}),
], calls=(E.Call(0x4F980, ("eax",)),), eax_mask=0,
   mutants=("@hook", "@wipearg", "@nowipe")),
```

The case seeds the hook dword with the *other* hook value, `0x0005D812` (not the post-condition),
and `0x104AE8` with a neighbour sentinel so a store wider than the raw's dword would show. `0x4F980`
is a stub in the call set: the port's `frontend_wipe_arm` opens with `PR_SEAM(0x4F980, ret_mode)`, so
the seam records the call and its argument on both sides while the arm's own stores are skipped on
both sides. The row therefore claims: the hook store, the call, and the arm's mode word as the call's
EAX argument; the arm's three stores are `frontend_wipe_arm`'s own row (d1d), not this one. Mask 0:
the hook's callers reach it through `fn_resolve` as a `void (*)(void)` and none reads EAX.

Measured mutant discrimination (all four rows `MISMATCH`, 1/1 block, one case each):

```text
frontend_28d68@hook    h0: call #0 memory: ... first difference at 0x104AE4: original 0x38, port 0x39
                       h0: byte 0x104AE4: original 0x38, port 0x39
frontend_28d68@wipearg h0: call #0: original 0x4F980(0x10), port 0x4F980(0x11)
frontend_28d68@nowipe  h0: call #0: original 0x4F980(0x10), port none
```

The `@hook` byte is the value's low byte; the call-memory difference proves the store is before the
call on both sides, which the raw's order requires. Measured, `--function frontend_28d68
--self-check`:

```text
| frontend_28d68 | 0x28D68 | 1 | 1/1 | VERIFIED | 4F980 stub unverified |
| frontend_28d68@hook | 0x28D68 | 1 | 1/1 | MISMATCH | 4F980 stub unverified |
| frontend_28d68@wipearg | 0x28D68 | 1 | 1/1 | MISMATCH | 4F980 stub unverified |
| frontend_28d68@nowipe | 0x28D68 | 1 | 1/1 | MISMATCH | 4F980 stub unverified |
```

(the callee column reads `unverified` by design in a `--function` run: the callee's own row is not
run; the full run below reads `4F980 stub VERIFIED`.)

### §P5.4 The final line and the eight closed rows

```text
diff-verify: 1021/1021 functions VERIFIED; 14960/14960 mutants detected; 0 named gaps; 783/834 rows with callees closed (187 have none); 777/834 closed on full rows; 8 ported functions with no row. ...
```

The six remaining rows close with the one added above (`frontend_mode_16_step` … `1b_step`; before
`28D68 allow unverified, ... 5D812 real unverified`, after `28D68 allow VERIFIED, ... 5D812 real
VERIFIED`), and no previously open row stays open on either address:

```text
frontend_mode_16_step 0x4F2B0: 28D68 allow VERIFIED, 4F790 stub VERIFIED, 4F980 allow VERIFIED, 5D812 real VERIFIED
frontend_mode_17_step 0x4F318: 28D68 allow VERIFIED, 4F790 stub VERIFIED, 4F980 allow VERIFIED, 5D812 real VERIFIED
frontend_mode_18_step 0x4F6E8: 28D68 allow VERIFIED, 4F980 allow VERIFIED, 5D812 real VERIFIED
frontend_mode_19_step 0x4F704: 28D68 allow VERIFIED, 4F980 allow VERIFIED, 5D812 real VERIFIED
frontend_mode_1a_step 0x4F9A0: 28D68 allow VERIFIED, 4F980 allow VERIFIED, 4F9E4 stub VERIFIED, 5D812 real VERIFIED
frontend_mode_1b_step 0x4F9C8: 28D68 allow VERIFIED, 4F980 allow VERIFIED, 4FA88 stub VERIFIED, 5D812 real VERIFIED
```

Together with §P5.2's two, that is the plan's 8 rows and its open count, `59 → 51`. **The measured
closed/full denominators are one higher than the plan's `782/833`/`776/833`**, and the difference is
not a fitted value: the plan counted the two new rows as callee-less, but `frontend_28d68`'s row
records its call to `0x4F980`, whose own row is VERIFIED, so the new row is itself a closed row with
a callee. Hence `with_callees` 833 → 834 and `closed` 774 + 8 + 1 = 783, `full` 768 + 8 + 1 = 777;
`834 − 783 = 51`, the same 51 open rows as `833 − 782`. The mutant count is exactly the plan's `+4`
(14957 → 14960) and functions `1019 → 1021`.

No other row moved: comparing the baseline and final tables row by row, 15975 → 15981 entries (the
six new ones only), no real row's entry/cases/blocks/verdict changed, and every existing row whose
callee column said `5D812 … unverified` now says `VERIFIED` (the callee-status column is the only
difference on those rows).

### §P5.5 The 51-row blocker census

Measured from the final table with the §J1 rule (`closed_rows`/`non_portable`). A row can name more
than one blocker, so the counts sum above 51; every blocker is a recorded stub except the three
allow-mode E2-class handlers the port implements (`0x29D04`, `0x3E484`, `0x3E4C4`), whose own rows
can exist and so are not excused:

| blocker | mode | rows |
|---|---|---|
| `0x65431` | stub | 11 (`game_frame_*` ×10, `game_quit_prompt`) |
| `0x5DD03` | stub | 10 (`attract_config_volumes` ×2, `fight_char_screen_open` ×2, `snd_sample_*` ×4, `sound_sfx_volume`, `sound_shutdown`) |
| `0x40CB0` | stub | 8 (`game_mode_05_step` ×2, `game_mode_0d_step*` ×3, `game_mode_32_step*` ×3) |
| `0x24568` | stub | 7 (`game_mode_05_step` ×2, `game_mode_0d_step*` ×2, `game_mode_32_step*` ×3) |
| `0x5DEED` | stub | 7 (`attract_config_volumes` ×2, `fight_char_screen_open` ×2, `snd_music_playing`, `sound_music_volume`, `sound_shutdown`) |
| `0x24804` | stub | 6 (`game_mode_05_step` ×2, `game_mode_0d_step*` ×2, `game_mode_32_step*` ×2) |
| `0x5DCC5` | stub | 6 (`attract_config_volumes` ×2, `fight_char_screen_open` ×2, `sound_sample_start`, `sound_sfx_volume`) |
| `0x5DECA` | stub | 5 (`attract_config_volumes` ×2, `fight_char_screen_open` ×2, `sound_music_volume`) |
| `0x5DC0F` | stub | 4 (`snd_sample_queue`, `snd_sample_stop`, `snd_samples_stop_all`, `sound_sample_start`) |
| `0x5DC8B` | stub | 4 (`snd_sample_queue`, `snd_sample_stop`, `snd_samples_stop_all`, `sound_shutdown`) |
| `0x29D04` | allow / stub | 4 (`fighter_35050`, `fighter_think_side` allow; `fighter_state_350d0`, `fighter_winner_body` stub) |
| `0x3E484` | allow | 3 (`fighter_19020`, `fighter_pass_a`, `fighter_pass_a_al`) |
| `0x5DEAF` | stub | 3 (`snd_music_stop`, `sound_music_pause_toggle`, `sound_shutdown`) |
| `0x2CB74` | stub | 2 (`menu_run`, `menu_step`) |
| `0x6C4CF` | stub | 2 (`config_file_load`, `menu_fatal_error`) |
| `0x3E4C4` | allow | 1 (`fighter_winner_body`) |
| `0x1C308`, `0x25FAC`, `0x26194`, `0x3D0BC`, `0x4F5C8`, `0x4FBBB`, `0x4FC05`, `0x4FCFD`, `0x4FDF5`, `0x5D86A`, `0x5DC2A`, `0x5DC4D`, `0x5DC70`, `0x5DCA6`, `0x5DCE4`, `0x61A70`, `0x65546` | stub | 1 each |

Against the §J1 paragraph in `AGENTS.md` this is the same census minus `0x5D812 real` (8 rows) and
`0x28D68 allow` (6 rows), i.e. the 14 blocker-row incidences those two addresses carried.

### §P5.6 The harness pins, the full-scan check and `port_progress`

The direct driver is the acceptance and it is green (exit 0, both runs). The harness suite
(`python3 -m unittest tools.tests.test_diff_verify`, 133 tests) now fails **four** pins, all because
they enumerate the complete spec set and this task adds two specs by plan:

- `test_the_self_check_counts_functions_mutants_gaps_and_closed_rows` — the pinned counter string (the
  one the plan says Task 11 updates);
- `test_the_eax_mask_is_stated_by_the_spec_per_function` — the `{name: eax_mask}` equality;
- `test_every_ported_function_agrees_with_the_original_on_every_block` — the sorted real-spec name list;
- `test_every_mutant_is_reported_as_a_mismatch` — the sorted mutant name list.

The plan's text (line 199) names only the counter string; the other three are the same kind of
exhaustive pin and need the two names (and the four mutants) added in Task 11 as well. The measured
values Task 11 needs: `runtime_5d812` mask `0xFFFFFFFF`, `frontend_28d68` mask `0`; mutants
`@ret` (`eax`, r1 only), `@hook` (`byte` + `call #0 memory`, h0), `@wipearg` (`call #0`, h0),
`@nowipe` (`call #0`, h0). `tools/tests/test_diff_verify.py` is untouched here by instruction.

`PR_DIFF_FULLSCAN=1` on both rows (the plan's risk 4: no new binding or mutant writes outside
`mem[]`) is green: `1/1 functions VERIFIED; 1/1 mutants detected` (0x5D812) and `1/1 functions
VERIFIED; 3/3 mutants detected` (0x28D68) — the only store on either side is the 0x28D68 row's four
bytes at `0x104AE4`, and the 0x5D812 row writes nothing.

`python3 tools/port_progress.py` is unmoved, as required (no `port/src` header, no `fn_register`):

```text
799 1203 66
759 759 100 (portable: excludes 53 host-owned/deferred and runtime >= 5D000)
```

### §P5.7 Files and commit

`port/tests/diff_runner.c` (the `p5` binding block: `b_p5_5d812`, `m_p5_5d812_ret`,
`b_p5_28d68`, `m_p5_28d68_hook`, `m_p5_28d68_wipearg`, `m_p5_28d68_nowipe`, and their six
`k_bindings` entries), `tools/diff_verify.py` (`P5_SPECS` and its place in the `SPECS`
concatenation), this record. No `port/src` header or `fn_register`, no `data/`, no Makefile,
no `tools/tests/` change. Commit `diff: row the frontend stub and 0x28D68`. The task report is the
git-ignored ledger.

## §P6 The fighter callback entries: `0x3E484`, `0x29D04` and `0x3E4C4`

Task 6 of the plan: three rows, no `port/src` change. The three addresses are the E2-class handler
entries `port/src/game/actors.c` `fn_register`s (`0x3E4C4` at 294, `0x3E484` at 362, `0x29D04` at
457) and the decomp index never lists; the closure rule (`NonPortable.excused`, record J §J1) cannot
excuse them because the port tree implements them (the port-code check keeps an `e2-field` address
blocking when `port/src` has a body for it), so the seven rows that name them stay open until each
has a row of its own. Everything below was measured at base commit
`c5f3812f` in the worktree `pixel-and-proof-closure`, with the plan's driver command:

```bash
python3 tools/diff_verify.py --diffrun build/diffrun --exe data/game/C/PRAGE.EXE \
  --image /tmp/pr_diff_image.bin --table /tmp/pr_diff_table_p6.md --self-check
```

### §P6.1 The baseline line

```text
diff-verify: 1021/1021 functions VERIFIED; 14960/14960 mutants detected; 0 named gaps; 783/834 rows with callees closed (187 have none); 777/834 closed on full rows; 8 ported functions with no row. ...
```

(15981 table entries = 1021 real + 14960 mutants; the baseline table is
`/tmp/pr_diff_table_baseline.md`.) The 51 open rows are `834 − 783`; the seven this task closes name
the three addresses, exactly the §P5.5 census rows: `0x3E484 allow` ×3 (`fighter_19020`,
`fighter_pass_a`, `fighter_pass_a_al`), `0x29D04 allow` ×2 (`fighter_35050`, `fighter_think_side`),
`0x29D04 stub` ×2 (`fighter_state_350d0`, `fighter_winner_body`) and `0x3E4C4 allow` ×1
(`fighter_winner_body`).

### §P6.2 `0x3E484` — the slot +0x18 hook

The raw bytes at file offset `0x912D8` (VA + `0x52E54`) are
`53 51 52 83 EC 28 89 C2 89 E0 31 C9 E8 BB 54 FF FF 8D 44 24 18 31 DB E8 34 A7 FD FF 30 D2 B4 01 88
54 24 19 88 54 24 20 8D 54 24 18 88 64 24 18 8B 04 24 E8 58 A7 FD FF 83 C4 28 5A 59 5B C3`:
`push ebx; push ecx; push edx; sub esp,0x28; mov edx,eax; mov eax,esp; xor ecx,ecx; call 0x33950;
lea eax,[esp+0x18]; xor ebx,ebx; call 0x18BD4; xor dl,dl; mov ah,1; mov [esp+0x19],dl; mov
[esp+0x20],dl; lea edx,[esp+0x18]; mov [esp+0x18],ah; mov eax,[esp]; call 0x18C14; add esp,0x28;
pop edx; pop ecx; pop ebx; ret`. No fixup record touches it (the image's bytes at `0x3E484` are the
file's). EAX = side; 0x33950 fills the six-dword context, 0x18BD4 fills 16 flag bytes with 2, then
flag 0 = 1, flags 1 and 8 = 0 and the two box tables are 0 (`xor ecx,ecx`/`xor ebx,ebx`), and
0x18C14's result is returned. The port's `fighter_3e484` (`fighter.c:8331`) is that body.

The spec (`P6C_SPECS`):

```python
Spec("fighter_3e484", 0x3E484, [
    Case("k0", {"eax": 0}, SLOT_PTRS),
    Case("k1", {"eax": 1}, SLOT_PTRS, {0x18C14: 1}),
    Case("k2", {"eax": 0}, SLOT_PTRS, {0x18C14: 0x100}),
], allow_calls=(0x33950, 0x18BD4), calls=(CHECKS,),
   mutants=("@flag0", "@flag1", "@flag8", "@ret")),
```

Mask full: the hook's one caller 0x19020 tests the whole EAX (`test eax,eax` 0x19048). 0x33950 and
0x18BD4 are allow-mode and run on both sides; 0x18C14 is the existing `CHECKS` stub (its EAX is
returned as is, and the four flag dwords are compared by value, §P2.7). k0's stub EAX is 0 and
k1/k2's are 1 and 0x100, so the side 1 case and the non-zero case are both present. The binding is
`b_p6_3e484`; the four mutants restate the port's body with one change each (`@ret` makes the call
and returns 0).

Measured, `--function fighter_3e484 --self-check`:

```text
| fighter_3e484 | 0x3E484 | 3 | 1/1 | VERIFIED | 18BD4 allow unverified, 18C14 stub unverified, 33950 allow unverified |
| fighter_3e484@flag0 | 0x3E484 | 3 | 1/1 | MISMATCH | ... |
| fighter_3e484@flag1 | 0x3E484 | 3 | 1/1 | MISMATCH | ... |
| fighter_3e484@flag8 | 0x3E484 | 3 | 1/1 | MISMATCH | ... |
| fighter_3e484@ret | 0x3E484 | 3 | 1/1 | MISMATCH | ... |
```

(the callee column reads `unverified` in a `--function` run by design: the callee's own row is not
run.) The mutant discrimination (each one case kind, all three cases for the flags):

```text
fighter_3e484@flag0  k0..k2: call #0: original 0x18C14(0x0, 0x2020001, ...), port ... 0x2020002 ...
fighter_3e484@flag1  k0..k2: call #0: original 0x18C14(0x0, 0x2020001, ...), port ... 0x2020201 ...
fighter_3e484@flag8  k0..k2: call #0: original ... 0x2020200, ...), port ... 0x2020202, ...)
fighter_3e484@ret    k1: eax (mask 0xFFFFFFFF): original 0x1, port 0x0
                     k2: eax (mask 0xFFFFFFFF): original 0x100, port 0x0
```

The four flag dwords are the values `0x18BD4` leaves after the three stores: `0x02020001,
0x02020202, 0x02020200, 0x02020202`. `@ret` is caught only by k1/k2 — with k0's stub EAX 0 a
constant return would agree by accident, which is why the case set is not k0 alone. The mutants are
caught by the call arguments (`call #0`), which `mutant_detection` counts as a difference.

### §P6.3 `0x29D04` — the slot +0x14 callback

The raw bytes at file offset `0x7CB58` are `53 51 52 80 3D 3D AF 01 00 00 74 06 31 C0 5A 59 5B C3 8B
18 8A 5B 51 81 E3 FF 00 00 00 31 C9 89 DA 8A 48 7A 8D 04 DD 00 00 00 00 01 D8 C1 E0 02 31 DA 01 D8 8B
0C 8D 98 8A 02 00 8A 93 34 5B 08 00 8B 04 85 B0 77 08 00 8B 1C 91 31 D2 E8 26 04 00 00 B8 01 00 00 00
5A 59 5B 8B C0 C3`: `push ebx; push ecx; push edx; cmp byte [0x9AF3D],0; je 0x29D16; xor eax,eax;
pop edx; pop ecx; pop ebx; ret`, then `mov ebx,[eax]; mov bl,[ebx+0x51]; and ebx,0xff; xor ecx,ecx;
mov edx,ebx; mov cl,[eax+0x7a]; lea eax,[ebx*8]; add eax,ebx; shl eax,2; xor edx,ebx; add eax,ebx;
mov ecx,[ecx*4+0xA8A98]; mov dl,[ebx+0x105B34]; mov eax,[eax*4+0x1077B0]; mov ebx,[ecx+edx*4]; xor
edx,edx; call 0x2A17C; mov eax,1; pop edx; pop ecx; pop ebx; mov eax,eax; ret`. The four absolute
operands are pre-fixup in the file (`0x1AF3D`, `0x28A98`, `0x85B34`, `0x877B0`); `mem_load_le_fixups`
writes `+0x80000`, the values `0x9AF3D`, `0xA8A98`, `0x105B34`, `0x1077B0` the harness runs. EAX =
the slot (the callers pass `0x1077B0 + side*0x94`): `DSD(slot)`'s `+0x51` is the side (zero-extended),
`slot + 0x7A` the character, `0xA8A98[character]` the handle row, `0x105B34[side]` its index,
`0x1077B0[side]` the record 0x2A17C is called on with EDX = 0 and EBX = the handle. The port's
`fighter_29d04` (`fighter.c:6588`) is that body, including the gate.

The spec:

```python
Spec("fighter_29d04", 0x29D04, [
    Case("q0", {"eax": DS_SLOTS}, p6c_29d04_pokes(0, gate=0xFF)),
    Case("q1", {"eax": DS_SLOTS}, p6c_29d04_pokes(0)),
    Case("q2", {"eax": DS_SLOTS + 0x94}, p6c_29d04_pokes(1, ch=3)),
], calls=(PALETTE,), mutants=("@gate", "@side", "@handle")),
```

`p6c_29d04_pokes` seeds the argument slot's record through `SLOT_PTRS` (so `DSD(slot)` is `0x10A300`
for side 0 and `0x10A400` for side 1), the record's `+0x51`, the slot's `+0x7A` character, the
character's row pointer at `0xA8A98 + ch*4` (the next character's points at `P6C_HANDLE2`, whose two
dwords are the first row's XOR `0x5A5A5A5A`), the row's two dwords (`0x33330000`/`0x44440000`), the
`0x105B34` index pair `(1, 0)` and the gate byte. q0's gate is `0xFF` (not 1: a signed or `== 1`
test differs), q1 is side 0 (handle `0x44440000`), q2 side 1 with character 3 (handle `0x33330000`).
Mask full: the three call sites test the whole EAX (`0x350BB`, `0x3514F`, `0x1953A`). 0x2A17C is the
existing `PALETTE` stub, recorded with its three arguments and its own row proving the body. The
binding is `b_p6_29d04`; `@gate` drops the `0x9AF3D` test, `@side` uses `1 - side` for both the
handle index and the palette record, `@handle` reads the row's other dword.

Measured, `--function fighter_29d04 --self-check`:

```text
| fighter_29d04 | 0x29D04 | 3 | 3/3 | VERIFIED | 2A17C stub unverified |
| fighter_29d04@gate | 0x29D04 | 3 | 3/3 | MISMATCH | 2A17C stub unverified |
| fighter_29d04@side | 0x29D04 | 3 | 3/3 | MISMATCH | 2A17C stub unverified |
| fighter_29d04@handle | 0x29D04 | 3 | 3/3 | MISMATCH | 2A17C stub unverified |
```

```text
fighter_29d04@gate    q0: eax (mask 0xFFFFFFFF): original 0x0, port 0x1
                      q0: call #0: original none, port 0x2A17C(0x10A300, 0x0, 0x44440000)
fighter_29d04@side    q1: call #0: original 0x2A17C(0x10A300, 0x0, 0x44440000), port 0x2A17C(0x10A400, 0x0, 0x33330000)
                      q2: call #0: original 0x2A17C(0x10A400, 0x0, 0x33330000), port 0x2A17C(0x10A300, 0x0, 0x44440000)
fighter_29d04@handle  q1: call #0: original ... (0x10A300, 0x0, 0x44440000), port ... (0x10A300, 0x0, 0x33330000)
                      q2: call #0: original ... (0x10A400, 0x0, 0x33330000), port ... (0x10A400, 0x0, 0x44440000)
```

Every one of the three arms of the original is exercised (3/3 blocks: the gate's return, the lookup
and the return 1).

### §P6.4 `0x3E4C4` — the slot +0x1C callback

The raw bytes at file offset `0x91318` are `52 83 EC 18 89 C2 89 E0 E8 7F 54 FF FF 8B 54 24 08 8B 44
24 0C E8 36 D2 FF FF 83 C4 18 5A C3`: `push edx; sub esp,0x18; mov edx,eax; mov eax,esp; call
0x33950; mov edx,[esp+8]; mov eax,[esp+0xc]; call 0x3B714; add esp,0x18; pop edx; ret` (no fixup
touches it). 0x33950's context is `[out0..out5]`; `[esp+8]` is `ctx[2]` (the side's slot) and
`[esp+0xc]` is `ctx[3]` (the other slot), so 0x3B714 is called `(ctx[3], ctx[2])`. The port's
`fighter_3e4c4` (`fighter.c:4686`) is that body.

The spec:

```python
Spec("fighter_3e4c4", 0x3E4C4, [
    Case("c0", {"eax": 0}, SLOT_PTRS),
    Case("c1", {"eax": 1}, SLOT_PTRS),
], allow_calls=(0x33950,), calls=(REACT,), eax_mask=0, mutants=("@swap", "@arg")),
```

Mask 0: the one caller 0x193B0 overwrites EAX at once (`mov eax,[esp+8]` 0x19508), so the row is the
recorded call and its two arguments. 0x33950 runs on both sides (allowed); 0x3B714 is the existing
`REACT` stub (its own row proves the applier). c0 calls `0x3B714(0x107844, 0x1077B0)` and c1
`(0x1077B0, 0x107844)` — the two slot addresses, swapped with the side. The binding is
`b_p6_3e4c4`; `@swap` inverts the pair, `@arg` passes the side where the side's slot belongs.

Measured, `--function fighter_3e4c4 --self-check`:

```text
| fighter_3e4c4 | 0x3E4C4 | 2 | 1/1 | VERIFIED | 33950 allow unverified, 3B714 stub unverified |
| fighter_3e4c4@swap | 0x3E4C4 | 2 | 1/1 | MISMATCH | 33950 allow unverified, 3B714 stub unverified |
| fighter_3e4c4@arg | 0x3E4C4 | 2 | 1/1 | MISMATCH | 33950 allow unverified, 3B714 stub unverified |
```

```text
fighter_3e4c4@swap  c0: call #0: original 0x3B714(0x107844, 0x1077B0), port 0x3B714(0x1077B0, 0x107844)
                    c1: call #0: original 0x3B714(0x1077B0, 0x107844), port 0x3B714(0x107844, 0x1077B0)
fighter_3e4c4@arg   c0: call #0: original 0x3B714(0x107844, 0x1077B0), port 0x3B714(0x107844, 0x0)
                    c1: call #0: original 0x3B714(0x1077B0, 0x107844), port 0x3B714(0x1077B0, 0x1)
```

### §P6.5 The final line and the seven closed rows

```text
diff-verify: 1024/1024 functions VERIFIED; 14969/14969 mutants detected; 0 named gaps; 793/837 rows with callees closed (187 have none); 787/837 closed on full rows; 8 ported functions with no row. ...
```

Functions `1021 → 1024` and mutants `14960 → 14969` are exactly the plan's `+3` and `+9`. The seven
rows the task names close:

```text
fighter_19020       before: 3E484 allow unverified                      after: 3E484 allow VERIFIED
fighter_pass_a      before: 3E484 allow unverified                      after: 3E484 allow VERIFIED
fighter_pass_a_al   before: 3E484 allow unverified                      after: 3E484 allow VERIFIED
fighter_35050       before: 29D04 allow unverified                      after: 29D04 allow VERIFIED
fighter_think_side  before: 29D04 allow unverified                      after: 29D04 allow VERIFIED
fighter_state_350d0 before: 29D04 stub unverified                       after: 29D04 stub VERIFIED
fighter_winner_body before: 29D04 stub unverified, 3E4C4 allow unverified
                    after:  29D04 stub VERIFIED, 3E4C4 allow VERIFIED
```

No other real row moved: comparing the baseline and final tables row by row, 15981 → 15993 entries
(the 12 new ones: 3 real + 9 mutants), and of the 218 lines whose callee column changed **none**
changed any other field (entry, cases, blocks, verdict). The callee statuses flip `3E484 allow` ×57,
`29D04 allow` ×51, `29D04 stub` ×110 and `3E4C4 allow` ×53 lines (mutants included; the rows that
carry these callees).

**The measured closed/full denominators are one higher per new row than the plan's `789/833` and
`783/833`**, exactly as Task 5's §P5.4 warned for its own batch: the plan counted the three new rows
as callee-less, but each records at least one callee whose own row is VERIFIED (0x18C14, 0x2A17C,
0x3B714) and is therefore itself a closed row with a callee. Hence `with_callees` 834 → 837,
`closed` 783 + 7 + 3 = 793 and `full` 777 + 7 + 3 = 787; `837 − 793 = 44`, the plan's open count, the
same 44 open rows as `833 − 789`. The `full` figure equals `closed` because none of the ten is in
`WEAKER`.

### §P6.6 The 44-row blocker census

Measured from the final table with the §J1 rule (`closed_rows`/`non_portable`); the same script
reproduces §P5.5's 51 rows on the baseline table. A row can name more than one blocker, so the counts
sum above 44; every blocker is a recorded stub (no allow-mode address is left), and the three this
task removed (`0x29D04`, `0x3E484`, `0x3E4C4`) carried 8 of the 51 rows' incidences:

| blocker | mode | rows |
|---|---|---|
| `0x65431` | stub | 11 (`game_frame_*` ×10, `game_quit_prompt`) |
| `0x5DD03` | stub | 10 (`attract_config_volumes` ×2, `fight_char_screen_open` ×2, `snd_sample_*` ×4, `sound_sfx_volume`, `sound_shutdown`) |
| `0x40CB0` | stub | 8 (`game_mode_05_step` ×2, `game_mode_0d_step*` ×3, `game_mode_32_step*` ×3) |
| `0x24568` | stub | 7 (`game_mode_05_step` ×2, `game_mode_0d_step*` ×2, `game_mode_32_step*` ×3) |
| `0x5DEED` | stub | 7 (`attract_config_volumes` ×2, `fight_char_screen_open` ×2, `snd_music_playing`, `sound_music_volume`, `sound_shutdown`) |
| `0x24804` | stub | 6 (`game_mode_05_step` ×2, `game_mode_0d_step*` ×2, `game_mode_32_step*` ×2) |
| `0x5DCC5` | stub | 6 (`attract_config_volumes` ×2, `fight_char_screen_open` ×2, `sound_sample_start`, `sound_sfx_volume`) |
| `0x5DECA` | stub | 5 (`attract_config_volumes` ×2, `fight_char_screen_open` ×2, `sound_music_volume`) |
| `0x5DC0F` | stub | 4 (`snd_sample_queue`, `snd_sample_stop`, `snd_samples_stop_all`, `sound_sample_start`) |
| `0x5DC8B` | stub | 4 (`snd_sample_queue`, `snd_sample_stop`, `snd_samples_stop_all`, `sound_shutdown`) |
| `0x5DEAF` | stub | 3 (`snd_music_stop`, `sound_music_pause_toggle`, `sound_shutdown`) |
| `0x2CB74` | stub | 2 (`menu_run`, `menu_step`) |
| `0x6C4CF` | stub | 2 (`config_file_load`, `menu_fatal_error`) |
| `0x1C308`, `0x25FAC`, `0x26194`, `0x3D0BC`, `0x4F5C8`, `0x4FBBB`, `0x4FC05`, `0x4FCFD`, `0x4FDF5`, `0x5D86A`, `0x5DC2A`, `0x5DC4D`, `0x5DC70`, `0x5DCA6`, `0x5DCE4`, `0x61A70`, `0x65546` | stub | 1 each |

Against §P5.5 this is the same table with the four `0x29D04`, three `0x3E484` and one `0x3E4C4`
incidences gone and nothing else changed. The 8 ported functions with no row (`UNROWED`) are
unmoved.

### §P6.7 The harness pins, the full-scan check and `port_progress`

The harness suite (`python3 -m unittest tools.tests.test_diff_verify`) is **not run here** by
instruction (its exhaustive pins are Task 11's, and §P5.6 already recorded the four that the plan's
new specs move). Task 11 needs, for the same four pins: `fighter_3e484` mask `0xFFFFFFFF`,
`fighter_29d04` mask `0xFFFFFFFF`, `fighter_3e4c4` mask `0`; mutants `@flag0`/`@flag1`/`@flag8`
(`call #0`, every case), `@ret` (`eax`, k1/k2), `@gate` (`eax` + `call #0`, q0), `@side` and
`@handle` (`call #0`, q1/q2), `@swap` and `@arg` (`call #0`, c0/c1).

`PR_DIFF_FULLSCAN=1` on each row (the plan's risk 4: no new binding or mutant writes outside
`mem[]`) is green: `1/1 functions VERIFIED; 4/4 mutants detected` (0x3E484), `1/1; 3/3` (0x29D04)
and `1/1; 2/2` (0x3E4C4). The rows' only writes are the pokes' seeded fields, read back by both
sides; no `reads outside the image` note is printed on any of the three.

`python3 tools/port_progress.py` is unmoved, as required (no `port/src` header, no `fn_register`):

```text
799 1203 66
759 759 100 (portable: excludes 53 host-owned/deferred and runtime >= 5D000)
```

### §P6.8 Files and commit

`port/tests/diff_runner.c` (the `p6` binding block: `p6_3e484_core`, `p6_29d04_core`,
`b_p6_3e484`/`m_p6_3e484_{flag0,flag1,flag8,ret}`, `b_p6_29d04`/`m_p6_29d04_{gate,side,handle}`,
`b_p6_3e4c4`/`m_p6_3e4c4_{swap,arg}` and their twelve `k_bindings` entries), `tools/diff_verify.py`
(`P6C_SPECS`, `p6c_29d04_pokes`, `P6C_HANDLE`/`P6C_HANDLE2`, and the place of `P6C_SPECS` in the
`SPECS` concatenation — the variable is `P6C_SPECS` because `P6_SPECS` already names track P batch
6), this record. No `port/src` header or `fn_register`, no `data/`, no Makefile, no `tools/tests/`
change. Commit `diff: row the fighter callback entries`. The task report is the git-ignored ledger.

## §P7 The actor entrance entries: `0x24568`, `0x40CB0` and `0x24804`

Task 7 of the plan: three rows, no `port/src` change. The three addresses are entries 1, 0 and 6 of
the per-character entrance table `0xA8628` (`0xA862C`, `0xA8628`, `0xA8640`); the port implements all
three (`fighter_24568` at `fighter.c:7070`, `fighter_40cb0` at 7162, `fighter_24804` at 7630) and
`actors.c` `fn_register`s them (`0x24568` at 584, `0x40CB0` at 594, `0x24804` at 599). The eight
`game_mode_05`/`game_mode_0d`/`game_mode_32` rows stub them by address and so stay open until each has
a row of its own; every callee of all three already has a row, so the three new rows also close
themselves. Everything below was measured at base commit `453f90ed` in the worktree
`pixel-and-proof-closure`, with the plan's driver command:

```bash
python3 tools/diff_verify.py --diffrun build/diffrun --exe data/game/C/PRAGE.EXE \
  --image /tmp/pr_diff_image.bin --table /tmp/pr_diff_table_p7.md --self-check
```

### §P7.1 The baseline line

```text
diff-verify: 1024/1024 functions VERIFIED; 14969/14969 mutants detected; 0 named gaps; 793/837 rows with callees closed (187 have none); 787/837 closed on full rows; 8 ported functions with no row. ...
```

(the baseline table is `/tmp/pr_diff_table_base.md`, 15993 entries = 1024 real + 14969 mutants; the
44 open rows are `837 - 793`, Task 6's measured figure).

### §P7.2 `0x24568` — entry 1, character 1's entrance

The raw bytes at file offset `0x773BC` (VA + `0x52E54`) are 0x16B bytes: `53 51 52 56 89 C6 31 C0 A0
0D 81 08 00 E8 F6 5F FF FF 84 C0 74 38 ...` — `push ebx; push ecx; push edx; push esi; mov esi,eax;
xor eax,eax; mov al,[0x10810D]; call 0x1A570; test al,al; je 0x245B6`, the 0x5000 placement loop
(`0x2457E..0x245E5`, signed compares against `-DS_000BE018` and `DS_000BE018`), the spawn
`0x245E7..0x245FD` (`and edx,0xffff; xor ecx,ecx; xor ebx,ebx; mov cx,[0xBD898]; push edx; mov
edx,eax; mov eax,esi; call 0x33C78`), `mov edx,0xE453A`/`push 0x40800000`/`call 0x2BC30`,
`mov edx,0x309`/`call 0x39A10`, `call 0x2372C`, the `0x10810D` 0x40/0x80 mask over `0x104B03` and the
`+0x41` bit, `mov eax,0xB4`/`call 0x2C3FC`, then `+0x52`/`+0x53` = 9/3, `+0x64` = 0x2A, `+0x40 |=
0x81000`, `+0x63` = 1 and the four pops. The data operands are pre-fixup in the file (`0x8810D`,
`0x3E018`, `0x3D898`, `0x6453A`, `0x877B0`, `0x84B03`); `mem_load_le_fixups` makes them the values
above. The port's `fighter_24568` is that body, including the placement loop (its comment at
`fighter.c:7053` states the same addresses).

The spec (`P7C_SPECS`) shares one case builder with the other two entrances:

```python
def p7c_record(slot, x, w28, side, tag):   # one record, +0x14..+0x53, sentinel neighbours
def p7c_slot_pokes(slot, rec, d8):         # +0, +8, +0x40 = 0x80008000, +0x52..+0x64
def p7c_ent_case(cid, ch, side, sel, x0, ox0, al, blink, bound=None, spawn=False)

Spec("fighter_24568", 0x24568, p7c_cases(1, True),
     allow_calls=(0x2372C,), calls=(BIT15, D4C_33C78, ANIM_BEGIN, TIMER, SPAWN, VOICE),
     eax_mask=0, mutants=("@a5", "@stream", "@no64")),
```

`P7C_ENT_CASES` is seven cases, chosen so the four control-flow arms of the placement and both blink
arms are all exercised (`l0` left success, `l1` the same on side 1/sel 1, `r0` right success, `r1` the
right arm failing into the `left = 1` back-edge, `f0` the left arm failing into the right arm, `m0`
sel 1 with side 0 for the mask/select split, `p0` a poked bound `0x4000` for the back-edge). The
records `0x10A800`/`0x10A900` are zero BSS of the image; the record blob sets `+0x14` (the slot),
`+0x18` (x), `+0x1C`, `+0x28` (REC has bit 14, REC2 not, so `0x2372C`'s two speed/a5 arms both run),
`+0x30`, `+0x3C` and `+0x51` (the side), every other byte its own sentinel. The slots' `+0x40` dword
is `0x80008000` (so the blink store flips its `+0x41` byte 0x80 -> 0x81 and the `| 0x81000` shows);
`+0x52..+0x64` carry 0x52..0x64. `0x10810D` is the case's `sel`, `0xBD898` is poked to `0x5A5A`
(the spawn's ECX argument) and the character's blink byte (`0x104B03` here) to the case's `blink`.
Mask 0: the three dispatchers (`0x25F27`, `0x27732`, `0x2989C`) reload the side from a byte and read
none of EAX. `0x1A570` is the `BIT15` stub with the case's AL (the entrance's only such call passes
`DSB(0x10810D)`); `0x33C78` is the existing `D4C_33C78` stub (EBX = 0 and the stack a5
0x4000/0, its own row proving the body); `0x2BC30`/`0x39A10`/`0x2C3FC` are `ANIM_BEGIN`/`TIMER`/
`VOICE`; `0x2372C` is the port's unseamed projectile target, so it runs on both sides (allow) and the
`0x2AE14` it calls is the `SPAWN` stub returning `P7C_CHILD` (0x10AA00), whose `+0x14/+0x2E/+0x4E/
+0x34/+0x36/+0x59` the body writes are seeded.

Measured, `--function fighter_24568 --self-check`:

```text
| fighter_24568 | 0x24568 | 7 | 11/11 | VERIFIED | 1A570 stub unverified, 2372C allow unverified, 2AE14 stub unverified, 2BC30 stub unverified, 2C3FC stub unverified, 33C78 stub unverified, 39A10 stub unverified |
| fighter_24568@a5 | 0x24568 | 7 | 11/11 | MISMATCH | ... |
| fighter_24568@stream | 0x24568 | 7 | 11/11 | MISMATCH | ... |
| fighter_24568@no64 | 0x24568 | 7 | 11/11 | MISMATCH | ... |
```

The original's six recorded calls and the placement, per case (the `--self-check` evidence):

```text
l0  1A570(0x0) 33C78(0x0,0xFFFFD000,0x5A5A,0x0,0x4000) 2BC30(0x10A800,0xE453A,0x40800000)
    39A10(0x10A800,0x309) 2AE14(0xBB3D0,0x2C00,0x30,0x1C1491,0x4000) 2C3FC(0xB4)  (16 bytes)
l1  1A570(0x1) 33C78(0x1,0xFFFFC800,0x5A5A,0x0,0x4000) ... 2AE14(...,0xC00,0x30,0x1C14D1,0x0)
    (18 bytes: 0x2372C's side-1 +0x2E/+0x4E arm)
r0  33C78(0x0,0x6000,0x5A5A,0x0,0x0)      r1  ... 0xFFFFE000 ... 0x4000   (the back-edge)
f0  1A570(0x0) ... 33C78(0x0,0x2000,...)  (the left arm's `jl` to the right arm)
m0  1A570(0x1) ... 33C78(0x0,0x6000,...)  sel 1, side 0: the mask is 0x40, the store happens
p0  bound 0x4000: 33C78(0x1,0xFFFFD000,...,0x4000)
```

The three mutants are caught by the call arguments and one store: `@a5` (`0x33C78`'s stack word 0
against the original's 0x4000; l0, l1, r1 and p0 — the four cases whose left arm passes a5),
`@stream` (`0x2BC30`'s EDX 0xE790A against 0xE453A; all seven), `@no64` (`+0x64` left at the seed 0x64
instead of 0x2A; all seven). No case reads EAX.

### §P7.3 `0x40CB0` — entry 0, character 0's entrance

The raw bytes at file offset `0x93B04` are 0x163 bytes, the same shape with 0x3000 (both arms) and
`mov ebx,0x4C00` at `0x40D2F` before the spawn (so EBX = y survives to `0x33C78`), the anim stream
`0xE790A`, the blink byte `0x104B02`, state 9/3/2, `+0x63` = 1, `mov eax,0x8C`/`call 0x2C3FC`; there is
no `0x2372C` and no `+0x64`. The port's `fighter_40cb0` is that body.

```python
Spec("fighter_40cb0", 0x40CB0, p7c_cases(0, False),
     calls=(BIT15, D4C_33C78, ANIM_BEGIN, TIMER, VOICE), eax_mask=0,
     mutants=("@y", "@place", "@stream")),
```

No allow call and no `0x2AE14`: every callee is seamed. Measured, `--function fighter_40cb0
--self-check`:

```text
| fighter_40cb0 | 0x40CB0 | 7 | 11/11 | VERIFIED | 1A570 stub unverified, 2BC30 stub unverified, 2C3FC stub unverified, 33C78 stub unverified, 39A10 stub unverified |
| fighter_40cb0@y | 0x40CB0 | 7 | 11/11 | MISMATCH | ... |
| fighter_40cb0@place | 0x40CB0 | 7 | 11/11 | MISMATCH | ... |
| fighter_40cb0@stream | 0x40CB0 | 7 | 11/11 | MISMATCH | ... |
```

The placement per case: `l0` `0xFFFFF000`, `l1` `0xFFFFE800`, `r0` `0x4000`, `r1` `0x6000` after the
right arm fails (`0x8000 > 0x7C00`) and the back-edge computes `0x2000`; `f0` starts at `0xFFFFA000`
(left `0xFFFF7000 < -0x7C00`) and takes the right arm to `0xFFFFD000`; `m0` sel 1/side 0 stores the
blink bit with mask 0x40; `p0` (bound 0x4000) back-edges to `0xFFFFF000`. `@y` changes the spawn's
EBX (0x4C00 -> 0; all seven), `@place` the width (0x3000 -> 0x5000; all seven), `@stream` the anim
stream (all seven).

### §P7.4 `0x24804` — entry 6, character 6's entrance

The raw bytes at file offset `0x77658` are 0x15D bytes of `0x24568`'s shape on stream `0xE1174`,
blink byte `0x104B08` and `+0x64` = 0x20, with `mov eax,esi`/`call 0x24754` at `0x24910..0x24912`
where `0x24568` calls `0x2372C`. The port's `fighter_24804` is that body.

```python
Spec("fighter_24804", 0x24804, p7c_cases(6, True),
     allow_calls=(0x33950, 0x24754),
     calls=(BIT15, D4C_33C78, ANIM_BEGIN, TIMER, SPAWN, VOICE), eax_mask=0,
     mutants=("@24754", "@a5", "@no64")),
```

`0x24754` is the port's second unseamed callee, so it runs on both sides (allow), as does its
`0x33950`; its `0x1A570`, `0x2AE14` and `0x2C3FC` are the row's stubs. The row therefore records both
`0x1A570` calls (the entrance's with `sel`, `0x24754`'s with `side`) and `0x24754`'s spawn with the
other slot's record x (`ctx[5] + 0x18 - ctx[5] + 0x3C` when AL, else `0x5400 - +0x3C + +0x18`), its
a3 (`+0x30 sar 16`), y = 0x1A00 and a5 = 0. Measured, `--function fighter_24804 --self-check`:

```text
| fighter_24804 | 0x24804 | 7 | 11/11 | VERIFIED | 1A570 stub unverified, 24754 allow unverified, 2AE14 stub unverified, 2BC30 stub unverified, 2C3FC stub unverified, 33950 allow unverified, 33C78 stub unverified, 39A10 stub unverified |
| fighter_24804@24754 | 0x24804 | 7 | 11/11 | MISMATCH | ... |
| fighter_24804@a5 | 0x24804 | 7 | 11/11 | MISMATCH | ... |
| fighter_24804@no64 | 0x24804 | 7 | 11/11 | MISMATCH | ... |
```

`@24754` calls it on `side ^ 1` and is caught by the second `0x1A570` argument, the `0x2AE14` x and
the target-slot store (`0x1077B8`/`0x10784C`); `@a5` and `@no64` as on `0x24568`. The base row's
call list for `l0` is `1A570(0x0) 33C78(0x0,0xFFFFD000,...) 2BC30(0x10A800,0xE1174,...)
39A10(0x10A800,0x309) 1A570(0x0) 2AE14(0xA8614,0x10D4FFAF,0x30,0x1A00,0x0) 2C3FC(0xA8)` (19 bytes
changed).

The brief names the streams' `0xD500` targets (`0x246D4` for `0x24568`, `0x40C34` for `0x40CB0`);
both are data-referenced only (the dword after the `0xD500` word in `0xE453A`/`0xE790A`; no code
calls them), so no case can execute them. What the rows pin is the stream every entrance starts through
`0x2BC30`: the `@stream` mutant is the wrong stream and the call argument catches it. `0x246D4` and
`0x40C34` stay rowless animation targets (registered in `actors.c`, no row names them), as before.

### §P7.5 The final line, the eight closed rows, and the arithmetic

```text
diff-verify: 1027/1027 functions VERIFIED; 14978/14978 mutants detected; 0 named gaps; 804/840 rows with callees closed (187 have none); 798/840 closed on full rows; 8 ported functions with no row. ...
```

Functions `1024 -> 1027` and mutants `14969 -> 14978` are the plan's `+3`/`+9`. The eight rows the
family stubs close (they name the three addresses as recorded stubs; nothing else in them changed):

```text
game_mode_05_step       before: 24568/24804/40CB0 stub unverified   after: all three VERIFIED
game_mode_05_step_reread before: 24568/24804/40CB0 stub unverified  after: all three VERIFIED
game_mode_32_step       before: 24568/24804/40CB0 stub unverified   after: all three VERIFIED
game_mode_32_step_end   before: 24568/40CB0 stub unverified         after: 24568/40CB0 VERIFIED
game_mode_32_step_rr    before: 24568/24804/40CB0 stub unverified   after: all three VERIFIED
game_mode_0d_step       before: 24568/24804/40CB0 stub unverified   after: all three VERIFIED
game_mode_0d_step_end   before: 40CB0 stub unverified               after: 40CB0 VERIFIED
game_mode_0d_step_rr    before: 24568/24804/40CB0 stub unverified   after: all three VERIFIED
```

No other real row moved: comparing the baseline and final tables row by row, 15993 -> 16005 entries
(the 12 new ones: 3 real + 9 mutants) and **none** of the 1024 shared real rows changed entry, case
count, blocks or verdict. The callee column flips `24568 stub` x372, `24804 stub` x333 and `40CB0
stub` x425 lines (mutants included; the mode stays `stub`).

**The measured closed/full figures are `804/840` and `798/840`, one higher per new row than the
plan's `797/791`**, the same correction Task 5's §P5.4 and Task 6's §P6.5 recorded: the plan counted
the three new rows as callee-less, but each records VERIFIED callees (`0x24568`: six stubs and the
allow-mode `0x2372C`; `0x40CB0`: five stubs; `0x24804`: six stubs and the allow-mode `0x33950`/
`0x24754`) and is therefore itself a closed row with callees. Hence
`with_callees` 837 -> 840, `closed` 793 + 8 + 3 = 804, `full` 787 + 8 + 3 = 798 and
`840 - 804 = 36 = 44 - 8` open. The `full` figure equals `closed` because none of the eleven is in
`WEAKER`.

### §P7.6 The 36-row blocker census

Measured from the final table with the §J1 rule (`closed_rows`/`non_portable`), the same script that
reproduces §P6.6's 44 rows on the baseline table. Every blocker is a recorded stub (no allow-mode
address is left); the three this task removed carried 21 of the 92 incidences (8 + 7 + 6), and the
rest is §P6.6's table unchanged:

| blocker | mode | rows | names |
|---|---|---|---|
| `0x65431` | stub | 11 | `game_frame_keys`, `game_frame_keys2`, `game_frame_head`, `game_frame_fight`, `game_frame_tails`, `game_frame_m25`, `game_frame_match`, `game_frame_front`, `game_frame_start`, `game_frame_rr`, `game_quit_prompt` |
| `0x5DD03` | stub | 10 | `snd_sample_playing`, `snd_samples_stop_all`, `snd_sample_stop`, `snd_sample_queue`, `sound_sfx_volume`, `attract_config_volumes_unscaled`, `attract_config_volumes`, `fight_char_screen_open`, `fight_char_screen_open_both`, `sound_shutdown` |
| `0x5DEED` | stub | 7 | `snd_music_playing`, `sound_music_volume`, `attract_config_volumes_unscaled`, `attract_config_volumes`, `fight_char_screen_open`, `fight_char_screen_open_both`, `sound_shutdown` |
| `0x5DCC5` | stub | 6 | `sound_sfx_volume`, `attract_config_volumes_unscaled`, `attract_config_volumes`, `sound_sample_start`, `fight_char_screen_open`, `fight_char_screen_open_both` |
| `0x5DECA` | stub | 5 | `sound_music_volume`, `attract_config_volumes_unscaled`, `attract_config_volumes`, `fight_char_screen_open`, `fight_char_screen_open_both` |
| `0x5DC0F` | stub | 4 | `snd_samples_stop_all`, `snd_sample_stop`, `snd_sample_queue`, `sound_sample_start` |
| `0x5DC8B` | stub | 4 | `snd_samples_stop_all`, `snd_sample_stop`, `snd_sample_queue`, `sound_shutdown` |
| `0x5DEAF` | stub | 3 | `snd_music_stop`, `sound_music_pause_toggle`, `sound_shutdown` |
| `0x2CB74` | stub | 2 | `menu_step`, `menu_run` |
| `0x6C4CF` | stub | 2 | `config_file_load`, `menu_fatal_error` |
| `0x1C308`, `0x25FAC`, `0x26194`, `0x3D0BC`, `0x4F5C8`, `0x4FBBB`, `0x4FC05`, `0x4FCFD`, `0x4FDF5`, `0x5D86A`, `0x5DC2A`, `0x5DC4D`, `0x5DC70`, `0x5DCA6`, `0x5DCE4`, `0x61A70`, `0x65546` | stub | 1 each | (17 rows: `sound_buffers_alloc`, `game_frame_head` x2, `fighter_state_3531c`, `game_loop`, `input_sampler` x4, `sound_shutdown`, `sound_sample_start` x5, `text_number_format`, `text_number_core`) |

27 distinct blockers, 71 incidences over 36 rows. The 8 ported functions with no row (`UNROWED`)
are unmoved.

### §P7.7 The harness pins, the full-scan check and `port_progress`

The harness suite (`python3 -m unittest tools.tests.test_diff_verify`) is **not run here** by
instruction (its exhaustive pins are Task 11's). Task 11 needs, for the three new specs: masks all
`0`; `@a5`/`@no64` (and `@y`, `@place`) caught as call-argument/byte differences on all seven cases;
`@stream` on `0x2BC30`'s EDX on all seven; `@24754` on the second `0x1A570` argument and the
`0x2AE14` x.

`PR_DIFF_FULLSCAN=1 --function <row> --self-check` is green on all three:
`1/1 functions VERIFIED; 3/3 mutants detected` for `fighter_24568`, `fighter_40cb0` and
`fighter_24804`. The rows' only writes are the pokes' seeded fields (the two slots, the two records,
`0x10810D`, `0xBD898`, the blink byte, the optional bound, the child) and the bodies' stores to them;
no `reads outside the image` note is printed on any of the three.

`python3 tools/port_progress.py` is unmoved, as required (no `port/src` header, no `fn_register`):

```text
799 1203 66
759 759 100 (portable: excludes 53 host-owned/deferred and runtime >= 5D000)
```

### §P7.8 Files and commit

`port/tests/diff_runner.c` (the `p7c` binding block: `b_p7c_24568` with `m_p7c_24568_{a5,stream,no64}`,
`b_p7c_40cb0` with `m_p7c_40cb0_{y,place,stream}`, `b_p7c_24804` with `m_p7c_24804_{24754,a5,no64}`
and their twelve `k_bindings` entries), `tools/diff_verify.py` (`P7C_SPECS`, `P7C_REC`/`P7C_REC2`/
`P7C_CHILD`, `P7C_BLINK`,
`P7C_LAYER`/`P7C_BOUND`, `p7c_record`, `p7c_slot_pokes`, `p7c_ent_case`, `p7c_cases`, and the place of
`P7C_SPECS` in the `SPECS` concatenation — the variable is `P7C_SPECS` because `P7_SPECS` already
names track P batch 7), this record. No `port/src` header or `fn_register`, no `data/`, no Makefile,
no `tools/tests/` change. Commit `diff: row the actor entrance entries`. The task report is the
git-ignored ledger.
