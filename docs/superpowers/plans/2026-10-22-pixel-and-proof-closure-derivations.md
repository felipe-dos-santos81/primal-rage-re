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

`P7C_ENT_CASES` is eight cases, chosen so each row's four control-flow arms of the placement and
both blink arms are all exercised. The width makes the arms case-specific: `l0` (left success) and
`l1` (the same on side 1/sel 1), `r0` (right success), `r1` (the 0x5000 rows' right arm fails into
the `left = 1` back-edge; at 0x40CB0's 0x3000 it is a right success), `f0` (the 0x5000 left test's
`jl` fails into the right arm; at 0x3000 the left arm succeeds), `g0` (at 0x40CB0's 0x3000 the left
test fails into the right arm; at 0x5000 the left arm also fails and the right succeeds), `m0`
(sel 1 with side 0 for the mask/select split), `p0` (a poked bound `0x4000` for the back-edge). The
records `0x10A800`/`0x10A900` are zero BSS of the image; the record blob sets `+0x14` (the slot),
`+0x18` (x), `+0x1C`, `+0x28` (REC has bit 14, REC2 not, so `0x2372C`'s two speed/a5 arms both run),
`+0x30`, `+0x3C` and `+0x51` (the side), every other byte its own sentinel. The slots' `+0x40` dword
is `0x80008000` (so the blink store flips its `+0x41` byte 0x80 -> 0x81 and the `| 0x81000` shows);
`+0x52..+0x64` carry 0x52..0x64. `0x10810D` is the case's `sel`, `0xBD898` is poked to `0x5A5A`
(the spawn's ECX argument) and the character's blink byte (`0x104B03` here) to the case's `blink`.
Mask 0: the three dispatchers read none of EAX — `0x25F27 -> 0x25F2E` reloads the entering side
from `0x104B12`, `0x27732 -> 0x27739` reads the selected side `0x10810D`, and `0x2989C -> 0x298A3`
loads BL from `0x104B09`. `0x1A570` is the `BIT15` stub with the case's AL (the entrance's only
such call passes `DSB(0x10810D)`); `0x33C78` is the existing `D4C_33C78` stub (EBX = 0 and the
stack a5 0x4000/0, its own row proving the body); `0x2BC30`/`0x39A10`/`0x2C3FC` are
`ANIM_BEGIN`/`TIMER`/`VOICE`; `0x2372C` is the port's unseamed projectile target, so it runs on both
sides (allow) and the `0x2AE14` it calls is the `SPAWN` stub returning `P7C_CHILD` (0x10AA00), whose
`+0x14/+0x2E/+0x4E/+0x34/+0x36/+0x59` the body writes are seeded.

Measured, `--function fighter_24568 --self-check`:

```text
| fighter_24568 | 0x24568 | 8 | 11/11 | VERIFIED | 1A570 stub unverified, 2372C allow unverified, 2AE14 stub unverified, 2BC30 stub unverified, 2C3FC stub unverified, 33C78 stub unverified, 39A10 stub unverified |
| fighter_24568@a5 | 0x24568 | 8 | 11/11 | MISMATCH | ... |
| fighter_24568@stream | 0x24568 | 8 | 11/11 | MISMATCH | ... |
| fighter_24568@no64 | 0x24568 | 8 | 11/11 | MISMATCH | ... |
```

The original's six recorded calls and the placement, per case (the `--self-check` evidence):

```text
l0  1A570(0x0) 33C78(0x0,0xFFFFD000,0x5A5A,0x0,0x4000) 2BC30(0x10A800,0xE453A,0x40800000)
    39A10(0x10A800,0x309) 2AE14(0xBB3D0,0x2C00,0x30,0x1C1491,0x4000) 2C3FC(0xB4)  (16 bytes)
l1  1A570(0x1) 33C78(0x1,0xFFFFC800,0x5A5A,0x0,0x4000) ... 2AE14(...,0xC00,0x30,0x1C14D1,0x0)
    (18 bytes: 0x2372C's side-1 +0x2E/+0x4E arm)
r0  33C78(0x0,0x6000,0x5A5A,0x0,0x0)      r1  ... 0xFFFFE000 ... 0x4000   (the back-edge)
f0  1A570(0x0) ... 33C78(0x0,0x2000,...)  (the left arm's `jl` to the right arm)
g0  33C78(0x0,0x0,0x5A5A,0x0,0x0)         (left 0xFFFF6000 < -0x7C00; right 0x0 succeeds)
m0  1A570(0x1) ... 33C78(0x0,0x6000,...)  sel 1, side 0: the mask is 0x40, the store happens
p0  bound 0x4000: 33C78(0x1,0xFFFFD000,...,0x4000)
```

The three mutants are caught by the call arguments and one store: `@a5` (`0x33C78`'s stack word 0
against the original's 0x4000; l0, l1, r1 and p0 — the four cases whose original a5 is 0x4000),
`@stream` (`0x2BC30`'s EDX 0xE790A against 0xE453A; all eight), `@no64` (`+0x64` left at the seed
0x64 instead of 0x2A; all eight). No case reads EAX.

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
| fighter_40cb0 | 0x40CB0 | 8 | 11/11 | VERIFIED | 1A570 stub unverified, 2BC30 stub unverified, 2C3FC stub unverified, 33C78 stub unverified, 39A10 stub unverified |
| fighter_40cb0@y | 0x40CB0 | 8 | 11/11 | MISMATCH | ... |
| fighter_40cb0@place | 0x40CB0 | 8 | 11/11 | MISMATCH | ... |
| fighter_40cb0@stream | 0x40CB0 | 8 | 11/11 | MISMATCH | ... |
```

The placement per case (width 0x3000): `l0` `0xFFFFF000` and `l1` `0xFFFFE800` (left success);
`r0` `0x4000` and `r1` `0x6000` (right success — 0x6000 is inside the 0x7C00 bound, so 0x40CB0
takes no `jg` back-edge on `r1`); `f0` `0xFFFFA000` (x0 `0xFFFFD000`: the left arm succeeds,
`0xFFFFA000 >= -0x7C00`); `g0` `0xFFFFE000` (x0 `0xFFFFB000` is below the left floor `0xFFFFB400`:
the left test's `jl` at `0x40CF5` falls into `0x40CFE` and the right arm succeeds); `m0` `0x4000`
(sel 1/side 0 stores the blink bit with mask 0x40); `p0` (bound 0x4000) the `jg` back-edge to
`0xFFFFF000` (right `0x5000 > 0x4000`, left `0xFFFFF000 >= -0x4000`). So `0x40CB0`'s left-test
`jl` edge (`0x40CF5 -> 0x40CFE`) is `g0`'s alone and its `jg` back-edge (`0x40D2B`) `p0`'s alone.
`@y` changes the spawn's EBX (0x4C00 -> 0; all eight), `@place` the width (0x3000 -> 0x5000; all
eight), `@stream` the anim stream (all eight).

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
| fighter_24804 | 0x24804 | 8 | 11/11 | VERIFIED | 1A570 stub unverified, 24754 allow unverified, 2AE14 stub unverified, 2BC30 stub unverified, 2C3FC stub unverified, 33950 allow unverified, 33C78 stub unverified, 39A10 stub unverified |
| fighter_24804@24754 | 0x24804 | 8 | 11/11 | MISMATCH | ... |
| fighter_24804@a5 | 0x24804 | 8 | 11/11 | MISMATCH | ... |
| fighter_24804@no64 | 0x24804 | 8 | 11/11 | MISMATCH | ... |
```

`@24754` calls it on `side ^ 1` and is caught by the second `0x1A570` argument, the `0x2AE14` x and
the target-slot store (`0x1077B8`/`0x10784C`) on all eight cases (104 diffs); `@a5` on l0/l1/r1/p0
and `@no64` on all eight, as on `0x24568`. The placement is `0x24568`'s (0x5000): `g0`'s x0
`0xFFFFB000` fails the left test (`0xFFFF6000 < -0x7C00`) and the right arm succeeds at `0x0`. The
base row's call list for `l0` is `1A570(0x0) 33C78(0x0,0xFFFFD000,...) 2BC30(0x10A800,0xE1174,...)
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
count, blocks or verdict. The callee column flips 1130 callee-status entries over 425 unique rows
(`24568 stub` x372, `24804 stub` x333, `40CB0 stub` x425; mutants included, the mode stays `stub`).

**The measured closed/full figures are `804/840` and `798/840`, 7 above the plan's `797/791`**: the
plan's baseline `789/783` is four below the measured baseline `793/787` (Task 5's and Task 6's rows
that already closed themselves, §P5.4/§P6.5) and each of the three new rows also closes itself (+3).
The plan counted them as callee-less, but each records VERIFIED callees (`0x24568`: six stubs and
the allow-mode `0x2372C`; `0x40CB0`: five stubs; `0x24804`: six stubs and the allow-mode `0x33950`/
`0x24754`). Hence `with_callees` 837 -> 840, `closed` 793 + 8 + 3 = 804, `full` 787 + 8 + 3 = 798
and `840 - 804 = 36 = 44 - 8` open. The full *increase* equals the closed one (+11) because none of
the eleven is in `WEAKER`; the absolute `closed - full = 6` is the same six closed `WEAKER` rows as
on the baseline.

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
`0`; `@a5` caught on `l0`/`l1`/`r1`/`p0` only (both `0x24568` and `0x24804`; the other four cases'
original a5 is already 0); `@no64`, `@y`, `@place`, `@stream` and `@24754` on all eight cases;
`@stream` on `0x2BC30`'s EDX, `@24754` on the second `0x1A570` argument and the `0x2AE14` x.

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

## §P8 The menu, loop and memset entries: `0x2CB74`, `0x4F5C8` and `0x61A70`

Task 8 of the plan: three rows, no `port/src` change. The three were recorded stubs in 36 rows'
callee columns (§P7.6) and kept four rows open: `0x2CB74` (`menu_step`, `menu_run`), `0x4F5C8`
(`game_loop`) and `0x61A70` (`text_number_format`). The port implements the first two
(`svc_start_menu` at `svcmenu.c:78` with `PR_SEAM_RET(0x2CB74u, entry)`, `flow_bonus_count_step` at
`flow.c:2745` with `PR_SEAM0(0x4F5C8u)`); the third is the runtime memset the port serves from
`mem_fill` (`mem.c:131`) behind `host_memset`'s `PR_SEAM(0x61A70u, …)` (`actors.c:4335`), so its row
is **binding-only**: a `port/src` column-0 header or `fn_register` would put `0x61A70` in the
closure rule's `ported` set and stop it excusing the 28 rows that name it as an allow-mode callee
(`§J1`). Everything below was measured at base commit `965c62db` in the worktree
`pixel-and-proof-closure`, with the plan's driver command:

```bash
python3 tools/diff_verify.py --diffrun build/diffrun --exe data/game/C/PRAGE.EXE \
  --image /tmp/pr_diff_image.bin --table /tmp/pr_diff_table_p8.md --self-check
```

### §P8.1 The baseline line

```text
diff-verify: 1027/1027 functions VERIFIED; 14978/14978 mutants detected; 0 named gaps; 804/840 rows with callees closed (187 have none); 798/840 closed on full rows; 8 ported functions with no row. ...
```

(the baseline table is `/tmp/pr_diff_table_base_p8.md`, 16005 entries = 1027 real + 14978 mutants;
the 36 open rows are `840 - 804`, Task 7's measured figure).

### §P8.2 `0x2CB74` — the START MENU step

The raw bytes at file offset `0x7F9C8` (VA + `0x52E54`) are 0x1D bytes, `53 51 52 BB 00 F0 00 00 BA
10 00 00 00 B8 CC CC 03 00 31 C9 E8 37 34 00 00 5A 59 5B C3`: `push ebx; push ecx; push edx;
mov ebx,0xF000; mov edx,0x10; mov eax,0x3CCCC; xor ecx,ecx; call 0x2FFC4; pop edx; pop ecx;
pop ebx; ret`. The `0x3CCCC` there is pre-fixup (`mem_load_le_fixups` adds the data object's
`0x80000` base), so the runtime table argument is `0xBCCCC`, the START MENU table `svcmenu.c`
calls `SVC_TABLE_START`. The function is one block (the call is the row's stop).

```python
P8C_MENU_STEP = E.Call(0x2FFC4, ("eax", "edx", "ecx"), clobbers=("ebx", "ecx", "edx"))
Spec("svc_2cb74", 0x2CB74, [
    Case("m0", {"eax": 0x00C0FFEE}, {}, {0x2FFC4: 5}),
], calls=(P8C_MENU_STEP,), mutants=("@entry",)),
```

The entry EAX is dead: `0x2CB81` reloads EAX with the table (0xBCCCC at runtime), so the case seeds it
`0x00C0FFEE` and the recorded first argument still has to be `0xBCCCC`. The return is `menu_step`'s own
(`0x2CB8D`): `0x2FFC4` invokes the selected item's callback with `call dword ptr [edx+8]`
(`0x304A2`), tests the result with full-register compares (`cmp eax,-0xA` `0x304AA`, `cmp eax,-5`
`0x304B3`) and returns it on those arms (`0x305E9`/`0x304DA`), and its own callers test the whole of
it too (`0x251E9`'s main-menu step, `test eax,eax; je; cmp eax,-5` at `0x251F3`), so mask full; the
stub's EAX 5 makes the return observable. `0x2FFC4` is a recorded stub
(the `menu_step` row proves its body), so the port's `menu_step(SVC_TABLE_START, 0x10, 0)` seam
returns it. The `EBX = 0xF000` the raw sets is not read by `0x2FFC4` (the `menu_step` row's i4).

Measured, `--function svc_2cb74 --self-check`:

```text
| svc_2cb74 | 0x2CB74 | 1 | 1/1 | VERIFIED | 2FFC4 stub unverified |
| svc_2cb74@entry | 0x2CB74 | 1 | 1/1 | MISMATCH | 2FFC4 stub unverified |
```

`@entry` is caught on `m0` by the call argument alone: `call #0: original 0x2FFC4(0xBCCCC, 0x10,
0x0), port 0x2FFC4(0xC0FFEE, 0x10, 0x0)` (1 diff).

### §P8.3 `0x4F5C8` — the render walk's bit-4 entry

The raw bytes at file offset `0xA241C` are 0x7A bytes, `53 51 52 56 31 D2 66 8B 15 F4 4A 08 00 8B 1D
D0 88 08 00 89 D0 C1 FA 1F F7 FB 85 D2 75 57 8B 0D ...`, the render table's bit-4 entry
(`DS_000A86C4[4]` = the dword at `0xA86D4`, its only reference; Ghidra has no function here) reached
by `0x255CC`'s per-bit walk while `DS_00104AEC` bit 4 is set (which `0x26D40` sets). Its four blocks
(leaders `0x4F5C8`, `0x4F5E6`, `0x4F5F0`, `0x4F63D`):

```text
4F5C8  push ebx/ecx/edx/esi; xor edx,edx; mov dx,[0x104AF4]; mov ebx,[0x1088D0];
       mov eax,edx; sar edx,0x1f; idiv ebx; test edx,edx; jne 0x4F63D
4F5E6  mov ecx,[0x104AC4]; test ecx,ecx; je 0x4F63D
4F5F0  push 0x4000; lea esi,[ecx-1]; mov eax,0x13; mov ecx,2; push edx (0); mov ebx,esi;
       mov edx,1; mov [0x104AC4],esi; call 0x2F528; xor edx,edx; mov dl,[0x104B1A];
       lea eax,[edx*8]; add eax,edx; shl eax,2; add eax,edx; shl eax,2; mov edx,1;
       add eax,0x1077B0; call 0x4DBB4
4F63D  pop esi/edx/ecx/ebx; ret
```

The tick is a **word** the raw zero-extends (`xor edx,edx` before `mov dx`) before a signed `idiv`;
the divisor is the signed dword `0x1088D0`; the count `0x104AC4` is stored as `n = count - 1`
(0x4F60A) **before** the draw (0x4F610); the slot is `0x1077B0 + DSB(0x104B1A) * 0x94` and the
`0x4DBB4(slot, 1)` add writes its `+0x5B` byte. Port comment and body: `flow.c:2730-2757`.

```python
P8C_TICK, P8C_DIV, P8C_COUNT, P8C_SIDE = 0x00104AF4, 0x001088D0, 0x00104AC4, 0x00104B1A
P8C_SLOT0, P8C_SLOT1 = 0x001077B0, 0x00107844          # +0x5B: 0x10780B / 0x10789F
def p8c_loop_case(word, div, count, side): ...         # the four globals between seeded neighbours,
                                                       # both slots' +0x5A..+0x5C
P8C_LOOP_CASES = [r0, c0, b0, w0, b1]
Spec("flow_4f5c8", 0x4F5C8, P8C_LOOP_CASES,
     allow_calls=(0x4DBB4,), calls=(D2C_NUMDRAW2,), eax_mask=0, mutants=("@zext",)),
```

The five cases and what the original does (measured from `spec_original`):

| case | word | divisor | count | side | arm | original calls and writes |
|---|---|---|---|---|---|---|
| `r0` | 5 | 3 | 7 | 0 | 5 % 3 = 2: the remainder return | none |
| `c0` | 6 | 3 | 0 | 0 | the zero-count return | none |
| `b0` | 6 | 3 | 3 | 0 | the body | `2F528(0x13,1,0x2,0x2,0x0,0x4000)`; 0x104AC4 3→2, 0x10780B 0x20→0x21 |
| `w0` | 0xFFFF | 3 | 1 | 0 | the zero-extension: 65535 % 3 = 0 | `2F528(0x13,1,0x0,0x2,0x0,0x4000)`; 0x104AC4 1→0, 0x10780B 0x20→0x21 |
| `b1` | 6 | 3 | 2 | 1 | the body on side 1 | `2F528(0x13,1,0x1,0x2,0x0,0x4000)`; 0x104AC4 2→1, 0x10789F 0x40→0x41 |

At the `0x2F528` arrival the original has already stored the new count: its recorded memory there is
`{0x104AC4: n}` on `b0`/`w0`/`b1` (0x2/0x0/0x1) and empty on `r0`/`c0`, which is what pins the store
before the draw (`0x4F60A` vs `0x4F610`).

The `r0`/`c0` arms leave EAX at the idiv quotient (1 and 2) and the body arms at `0x4DBB4`'s sum
(0x21/0x41), none of which a caller reads (the walk reloads EAX at `0x25621`), so mask 0. `0x2F528`
(`text_number_draw_font2`) is the existing `D2C_NUMDRAW2` stub (its own row proves the draw, and the
call's recorded memory pins the count store's order); `0x4DBB4` (`fight_slot_5b_add`) has **no**
`PR_SEAM`, so it runs on both sides (allow) and its own row proves the body, its `+0x5B` store being
the compared byte. No case uses a zero divisor: the raw's `idiv` would fault (`0x4F5E0`), and the
port's zero return is the documented deviation (`flow.c`'s PORT comment, as `0x4F4E8`'s).

Measured, `--function flow_4f5c8 --self-check`:

```text
| flow_4f5c8 | 0x4F5C8 | 5 | 4/4 | VERIFIED | 2F528 stub unverified, 4DBB4 allow unverified |
| flow_4f5c8@zext | 0x4F5C8 | 5 | 4/4 | MISMATCH | 2F528 stub unverified, 4DBB4 allow unverified |
```

`@zext` (a copy of the body whose tick read is `(s32)(s16)DSW(0x104AF4)`) is caught on `w0` alone,
three ways: `call #0: original 0x2F528(0x13,0x1,0x0,0x2,0x0,0x4000), port none`,
`byte 0x104AC4: original 0x00, port unchanged`, `byte 0x10780B: original 0x21, port unchanged`
(-1 % 3 = -1 returns). The other four cases read the same word sign- or zero-extended, so they
cannot catch it — the case is load-bearing.

### §P8.4 `0x61A70` — the runtime memset, binding only

The raw bytes at file offset `0xB48C4` are `50 51 88 D6 C1 E2 08 88 F2 C1 E2 08 88 F2 89 D9 E8 0B
3A 00 00 59 58 C3`: `push eax; push ecx; mov dh,dl; shl edx,8; mov dl,dh; shl edx,8; mov dl,dh;
mov ecx,ebx; call 0x65490; pop ecx; pop eax; ret`. EAX = dest, EDX = fill (the three shifts
broadcast DL through the whole register), EBX = len; `0x65490` (and its `0x654C7` body) fills the
bytes, and the popped EAX returns dest. The row is a binding over the port's `mem_fill`, which is the
same byte fill:

```c
static void b_p8c_61a70(const u32 *r, u32 *eax)
{ mem_fill(r[R_EAX], (u8)r[R_EDX], r[R_EBX]); *eax = r[R_EAX]; }
```

No `port/src` header or `fn_register`: `mem_fill` is a `port/src` function but the address `0x61A70`
stays unregistered, so `NonPortable.ported` does not gain it and the closure rule keeps excusing the
28 rows whose `61A70 allow` callee names it (two of the 28 are open on other blockers; 26 currently
close on that excuse).

```python
Spec("runtime_61a70", 0x61A70, [
    Case("z0", {"eax": P8C_BUF, "edx": 0x0000005A, "ebx": 0}, d1d_chunks(P8C_BUF, b"\xA5" * 8)),
    Case("a0", {"eax": P8C_BUF, "edx": 0x0000005A, "ebx": 0x23}, d1d_chunks(P8C_BUF, b"\xA5" * 0x40)),
    Case("b0", {"eax": P8C_BUF + 1, "edx": 0x12345678, "ebx": 7}, d1d_chunks(P8C_BUF, b"\xA5" * 0x10)),
    Case("c0", {"eax": P8C_BUF, "edx": 0x00000000, "ebx": 0x100}, d1d_chunks(P8C_BUF, b"\xFF" * 0x120)),
], allow_calls=(0x65490, 0x654C7), mutants=("@fill",)),
```

`P8C_BUF` is `0x10AC00` in the image's zero BSS. The four cases, measured: `z0` len 0 (the callee's
`or ecx,ecx; je 0x654C0`: no writes, EAX = dest 0x10AC00), `a0` len 0x23 from an aligned start
(35 bytes 0x10AC00..0x10AC22), `b0` len 7 from an unaligned start with `EDX = 0x12345678` (DL 0x78,
DH 0x56: the write is 0x78, not 0x56) (7 bytes 0x10AC01..0x10AC07), `c0` len 0x100 (256 bytes
0x10AC00..0x10ACFF, the 0x20-byte unrolled path). Each returns dest in EAX.

Measured, `--function runtime_61a70 --self-check`:

```text
| runtime_61a70 | 0x61A70 | 4 | 1/1 | VERIFIED | 65490 allow unverified, 654C7 allow unverified |
| runtime_61a70@fill | 0x61A70 | 4 | 1/1 | MISMATCH | 65490 allow unverified, 654C7 allow unverified |
```

`@fill` fills with `(u8)(DL + 1)` and is caught on the three non-empty cases (298 byte diffs, first
`a0: byte 0x10AC00: original 0x5A, port 0x5B`). The row's own callees are the allow-mode runtime
pair `0x65490`/`0x654C7` (both `>= 0x5D000`, neither in the port tree), which the closure rule
excuses, so the row closes and is full.

### §P8.5 The final line, the four closed rows, and the arithmetic

```text
diff-verify: 1030/1030 functions VERIFIED; 14981/14981 mutants detected; 0 named gaps; 811/843 rows with callees closed (187 have none); 804/843 closed on full rows; 8 ported functions with no row. ...
```

Functions `1027 -> 1030` and mutants `14978 -> 14981` are the plan's `+3`/`+3`. The four rows the
three stubs kept open (each now reads `VERIFIED` on the callee; no other field moved):

```text
menu_step           2CB74 stub unverified -> VERIFIED
menu_run            2CB74 stub unverified -> VERIFIED
game_loop           4F5C8 stub unverified -> VERIFIED   (stays in WEAKER: closed, not full)
text_number_format  61A70 stub unverified -> VERIFIED
```

The measured `811/843` closed and `804/843` full are the Task 7 line's `804/840`/`798/840` plus:
`with_callees` +3 (each new row records VERIFIED callees), closed +4 (the four rows above) +3 (the
three new rows close themselves) = `804 + 7 = 811`, full +3 (the three of the four that are not
`WEAKER`) +3 = `798 + 6 = 804`, and open `843 - 811 = 32 = 36 - 4`. `game_loop` is the one new
closed row that is not full, so the full increase (+6) is one below the closed one (+7); the absolute
`closed - full = 7` is the previous six closed `WEAKER` rows plus `game_loop`. The plan's `797 -> 801`
closed and `791 -> 794` full count only the four unblocked rows (its three new rows as callee-less),
so the measured *increases* are `+3` above the plan's (`+7` vs `+4`, `+6` vs `+3`, one per new row);
the baseline is seven above the plan's 797/791 (the Task 5-7 measured corrections,
§P5.4/§P6.5/§P7.5), so the absolute final is **ten** above the plan's 801/794
(`811 - 801 = 804 - 794 = 10`).

Table size `16005 -> 16011` entries = the six new lines (3 real + 3 mutants); of the 1027 shared real
rows **none** changed its entry, case count, blocks or verdict. The callee-status column flips 563
entries over 563 rows (`61A70 allow/stub -> VERIFIED` ×412, `2CB74 stub -> VERIFIED` ×117,
`4F5C8 stub -> VERIFIED` ×34; mutants included, the mode is unchanged). The `61A70` flips reach every
row that names it as an allow callee (28 real rows and their mutants) as well as
`text_number_format`, which stubs it.

### §P8.6 The 32-row blocker census

Measured from the final table with the §J1 rule (`closed_rows`/`non_portable`), the same script that
reproduces §P7.6's 36 rows on the baseline table. Every blocker is a recorded stub; the three this
task removed carried 4 of the 71 baseline incidences (`0x2CB74` ×2, `0x4F5C8` ×1, `0x61A70` ×1), and
the rest is §P7.6's table unchanged:

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
| `0x6C4CF` | stub | 2 | `config_file_load`, `menu_fatal_error` |
| `0x1C308`, `0x25FAC`, `0x26194`, `0x3D0BC`, `0x4FBBB`, `0x4FC05`, `0x4FCFD`, `0x4FDF5`, `0x5D86A`, `0x5DC2A`, `0x5DC4D`, `0x5DC70`, `0x5DCA6`, `0x5DCE4`, `0x65546` | stub | 1 each | (15 rows: `sound_buffers_alloc`, `game_frame_head` ×2, `fighter_state_3531c`, `input_sampler` ×4, `sound_shutdown`, `sound_sample_start` ×5, `text_number_core`) |

24 distinct blockers, 67 incidences over 32 rows. The 8 ported functions with no row (`UNROWED`)
are unmoved.

### §P8.7 The harness pins, the full-scan check and `port_progress`

The harness suite (`python3 -m unittest tools.tests.test_diff_verify`) is **not run here** by
instruction (its exhaustive pins are Task 11's). Task 11 needs, for the three new specs: masks
`0xFFFFFFFF` (`svc_2cb74`, `runtime_61a70`) and `0` (`flow_4f5c8`); `@entry` caught on `m0` by the
`0x2FFC4` first argument; `@zext` on `w0` only (three diffs); `@fill` on `a0`/`b0`/`c0` (298 diffs).
`PR_DIFF_FULLSCAN=1 --function <row> --self-check` is green on all three:
`1/1 functions VERIFIED; 1/1 mutants detected` each. None of the three prints a
`reads outside the image` note or an unhit block (`m0` 1/1, `flow_4f5c8` 4/4, `runtime_61a70` 1/1).

`python3 tools/port_progress.py` is unmoved, as required (no `port/src` header, no `fn_register`):

```text
799 1203 66
759 759 100 (portable: excludes 53 host-owned/deferred and runtime >= 5D000)
```

### §P8.8 Files and commit

`port/tests/diff_runner.c` (the `p8c` binding block: `b_p8c_2cb74` with `m_p8c_2cb74_entry`,
`b_p8c_4f5c8` with `m_p8c_4f5c8_zext`, `b_p8c_61a70` with `m_p8c_61a70_fill`, and their six
`k_bindings` entries), `tools/diff_verify.py` (`P8C_SPECS`, `P8C_MENU_STEP`,
`P8C_TICK`/`P8C_DIV`/`P8C_COUNT`/`P8C_SIDE`, `P8C_SLOT0`/`P8C_SLOT1`, `P8C_BUF`, `p8c_loop_case`,
`P8C_LOOP_CASES`, and the place of `P8C_SPECS` in the `SPECS` concatenation — the variable is
`P8C_SPECS` because `P8_SPECS` already names track P batch 8), this record. No `port/src` header or
`fn_register`, no `data/`, no Makefile, no `tools/tests/` change. Commit
`diff: row the menu, loop and memset entries`. The task report is the git-ignored ledger.

## §P9 The synthetic `+0x10` hook and the real `0x3A6D4`

Task 9 of the plan: the `0x3D0BC` spec artifact is corrected and the real slot `+0x10` hook
`0x3A6D4` gets a row. The hook was already ported (`fighter_pose_3a6d4`, `fighter.c:5704`) and
`fn_register`ed (`actors.c:301`), so no new port function or header: one `PR_SEAM` line is the
existing repo rule for a call-set callee, and its absence was the only port change (§P9.4).
Everything below was measured at base commit `7bf21ad8` in the worktree `pixel-and-proof-closure`,
with the plan's driver command:

```bash
python3 tools/diff_verify.py --diffrun build/diffrun --exe data/game/C/PRAGE.EXE \
  --image /tmp/pr_diff_image.bin --table /tmp/pr_diff_table_p9.md --self-check
```

### §P9.1 The baseline line

```text
diff-verify: 1030/1030 functions VERIFIED; 14981/14981 mutants detected; 0 named gaps; 811/843 rows with callees closed (187 have none); 804/843 closed on full rows; 8 ported functions with no row. ...
```

Task 8's §P8.5 line, 32 open rows (`843 - 811`), with the baseline table at
`/tmp/pr_diff_table_p8_fix.md` (16011 entries).

### §P9.2 The raw correction: `0x3D0BC` is not a function

The bytes at `0x3D0BC` (file offset `0x8FF10` = VA + `0x52E54`) are `00 00 00 00`. The
surrounding bytes are the previous function's epilogue (`0x3D004`, the decomp index's size 0xB7,
ends at `0x3D0BB`: its last instruction is the `ret` at `0x3D0BA`) and the five-byte alignment pad
before the next entry:

```text
3D0B4  30 c0               xor al,al
3D0B6  5e                  pop esi
3D0B7  5a                  pop edx
3D0B8  59                  pop ecx
3D0B9  5b                  pop ebx
3D0BA  c3                  ret
3D0BB  00 00 00 00 00      the five pad bytes that align 0x3D0C0
3D0C0  52 89 c2 c1 e0 03   push edx; mov edx,eax; shl eax,3; add eax,edx; ...
```

`0x3D0BC` is bytes 2..5 of that pad (`00 00 00 00`); the real entry is `0x3D0C0`, which the decomp
index lists (`port/decomp/prage.functions.csv`: `0003d0c0`, size 75) and which already has its own
row (`fighter_3d0c0`, 2 cases, `VERIFIED`, 1/1 blocks). The index has no entry at `0x3D0BC`, and
`tools/port_classification.txt`, `symbols.h` and `port/src` name it nowhere: it is not a function
and no row can exist for it.

The slot `+0x10` hooks the pose setters store are the real functions this neighbourhood was
confused with:

| setter | store | hook |
|---|---|---|
| `0x3A79C` (the `0x107D08`/`0x107D04` family) | `0x3A7D2  c7 40 10 d4 a6 03 00` (`mov dword [eax+0x10],0x3a6d4`) | `0x3A6D4` |
| `0x3A650` (the `0x107D0C`/`0x107D00` family) | `0x3A686  c7 40 10 88 a5 03 00` (`mov dword [eax+0x10],0x3a588`) | `0x3A588` |

`0x3531C` case 10 is the only **call** through the dword (`cmp dword [ecx+0x10],0; je 0x354E5;
mov eax,ecx; call dword [ecx+0x10]` 0x354DA..0x354E2; it is `port/src`'s only
`fn_resolve(DSD(slot + 0x10u))` — the other sites compare the value), and its registers there are
EAX = ECX = slot and EBX = side (`0x35325 mov ebx,eax`, rewritten on no path to the call). The old
`d15` case poked `0x3D0BC` into the scratch slot's `+0x10`; the original's stub at that address
recorded the arrival, and the port's `fn_resolve_from` reports an *unregistered* address to the
seam with no arguments (`mem.c:66..72`), so `fn_resolve(0x3D0BC)` returning NULL still produced a
matching `(0x3D0BC, ())` arrival on both sides — the case compared the address alone (the old
comment's "the port reports the address alone"), never a call, and so could not notice that the
address was not a function.

### §P9.3 The row: `0x3A6D4`, twelve blocks

The raw at `0x3A6D4..0x3A798` (measured from the fixup-applied image; the image is
`mem[0x10000..0x10B0D0)`, `build/diffrun --exe data/game/C/PRAGE.EXE --image-out`):

```text
3A6D4  83 ec 18            sub esp,0x18
3A6D7  89 da               mov edx,ebx              ; side
3A6D9  89 e0               mov eax,esp
3A6DB  e8 30 93 ff ff      call 0x33A10             ; the ctx swap
3A6E0  8b 44 24 0c         mov eax,[esp+0xc]        ; ctx[3] = the side's own slot
3A6E4  8a 40 58            mov al,[eax+0x58]
3A6E7  3c 01               cmp al,1
3A6E9  72 06               jb 0x3A6F1               ; phase 0
3A6EB  76 18               jbe 0x3A705              ; phase 1
3A6ED  83 c4 18            add esp,0x18             ; phase > 1
3A6F0  c3                  ret
3A6F1  84 c0               test al,al
3A6F3  0f 85 9c 00 00 00   jne 0x3A795              ; dead: al < 1 unsigned means al is 0
3A6F9  8b 44 24 0c         mov eax,[esp+0xc]
3A6FD  c6 40 58 01         mov byte [eax+0x58],1
3A701  83 c4 18            add esp,0x18
3A704  c3                  ret
3A705  8b 44 24 0c         mov eax,[esp+0xc]
3A709  68 00 00 40 40      push 0x40400000          ; 3.0f
3A70E  8a 40 7a            mov al,[eax+0x7a]        ; the character
3A711  25 ff 00 00 00      and eax,0xff
3A716  8b 14 85 08 90 0c 00 mov edx,[eax*4+0xC9008] ; the stream table
3A71D  8b 44 24 18         mov eax,[esp+0x18]       ; ctx[5] = the record
3A721  e8 0a 15 ff ff      call 0x2BC30             ; RET 4, pops the push
3A726  8b 54 24 14         mov edx,[esp+0x14]       ; ctx[5]
3A72A  31 db               xor ebx,ebx
3A72C  8b 44 24 04         mov eax,[esp+4]          ; ctx[1] = the side
3A730  8b 52 18            mov edx,[edx+0x18]
3A733  e8 74 e1 fd ff      call 0x188AC             ; anchor set (x, y = 0)
3A738  8b 44 24 0c         mov eax,[esp+0xc]
3A73C  c6 40 58 02         mov byte [eax+0x58],2
3A740  8b 54 24 04         mov edx,[esp+4]          ; the side
3A744  8b 1c 55 06 7d 10 00 mov ebx,[edx*2+0x107D06] ; high word = 0x107D08 + side*2 = A
3A74B  8b 14 55 02 7d 10 00 mov edx,[edx*2+0x107D02] ; high word = 0x107D04 + side*2 = B
3A752  89 e1               mov ecx,esp
3A754  c1 fa 10            sar edx,0x10
3A757  c1 fb 10            sar ebx,0x10
3A75A  85 d2               test edx,edx
3A75C  74 2c               je 0x3A78A               ; B == 0
3A75E  83 fa 05            cmp edx,5
3A761  74 27               je 0x3A78A               ; B == 5
3A763  8b 44 24 0c         mov eax,[esp+0xc]
3A767  8a 80 90 00 00 00   mov al,[eax+0x90]        ; the old gate
3A76D  fe c8               dec al
3A76F  3c 03               cmp al,3
3A771  77 0d               ja 0x3A780
3A773  25 ff 00 00 00      and eax,0xff
3A778  2e ff 24 85 c4 a6 03 00  jmp cs:[eax*4+0x3A6C4]   ; four entries, all 0x3A78A
3A780  89 da               mov edx,ebx              ; A
3A782  8b 41 04            mov eax,[ecx+4]          ; ctx[1] = the side
3A785  e8 52 e1 fd ff      call 0x188DC             ; snap x to A
3A78A  8b 44 24 0c         mov eax,[esp+0xc]
3A78E  c6 80 90 00 00 00 03 mov byte [eax+0x90],3
3A795  83 c4 18            add esp,0x18
3A798  c3                  ret
```

The switch table at `0x3A6C4` is `8a a7 02 00` four times in the file and `8a a7 03 00` four times
in the image (the fixups add the code object's `0x10000`): every entry is `0x3A78A`, so the four
gate values `1..4` skip the snap. The gate test is `(u8)(old +0x90 - 1) > 3`: the snap runs when
the old `+0x90` is `0` or `5..0xFF`. `E.static_scan` resolves the switch through `switch_cases`
(the `cmp al,3; ja` guard with `and eax,0xff` keeping the bound), so the entry's twelve leaders are
`0x3A6D4, 0x3A6EB, 0x3A6ED, 0x3A6F1, 0x3A6F9, 0x3A705, 0x3A75E, 0x3A763, 0x3A773, 0x3A780,
0x3A78A, 0x3A795` and no `RESOLVED_JUMPS` entry is needed.

The row (`P9C_SPECS` in `tools/diff_verify.py`) runs `0x33A10` on both sides (allow; its own row
proves it) and stubs `0x188AC` (`ANCHOR`), `0x2BC30` (`ANIM_BEGIN`) and `0x188DC` (`ANCHORX`) —
each has its own row — so it compares the recorded arguments and the two slot bytes. Mask 0: the
only caller is `0x3531C` case 10, whose return value no caller reads (the parent's mask-0 note).

```python
Spec("fighter_3a6d4", 0x3A6D4, [ ... six cases ... ],
     allow_calls=(0x33A10,), calls=(ANCHOR, ANIM_BEGIN, ANCHORX), eax_mask=0,
     mutants=("@a16", "@b5"))
```

The six cases and what the original does (measured from `spec_original`):

| case | side | phase | char | A | B | gate | the original's calls and writes |
|---|---|---|---|---|---|---|---|
| `p0` | 0 | 0 | 3 | 0x7FFF | 7 | 0 | none; `0x107808` (`+0x58`) 0 → 1 |
| `p1` | 1 | 1 | 5 | 0x1000 | 0 | 2 | `2BC30(0x10A400, 0xE1234, 0x40400000)`, `188AC(1, 0x28282828, 0)`; `0x10789C` 1 → 2, `0x1078D4` 2 → 3 |
| `p2` | 0 | 1 | 3 | 0x2000 | 5 | 0 | `2BC30(0x10A300, 0xE1234, 0x40400000)`, `188AC(0, 0x18181818, 0)`; `0x107808` 1 → 2, `0x107840` 0 → 3 |
| `p3` | 1 | 1 | 5 | 0x3000 | 0x8000 | 1 | as `p1` with the gate 1 (index 0 ≤ 3): the switch arm, A unused, no snap |
| `p4` | 0 | 1 | 3 | 0x8001 | 7 | 0 | as `p2` plus `188DC(0, 0xFFFF8001)` — the sign-extended A |
| `p5` | 1 | 2 | 5 | 0x4000 | 0 | 0 | none, no writes (the `0x3A6ED` return) |

`p1` and `p3` are the first `je` (B == 0) and the switch arm; `p2` is the second `je` (B == 5);
`p4` is the snap arm; `p0` and `p5` are the two early returns. Every seed differs from what the
function writes (`+0x58` and `+0x90` are seeded 0/1/2 and 0/1/2), the other slot's three bytes are
seeded with different values, and the other side's A/B words (0x6666/0x7777) and the 0xC9008
neighbour entry (0xE5678) make a wrong side or character a different recorded argument; a wrong 3.0
scale shows in the recorded `s0` instead, because the scale is the raw's immediate `0x40400000`
(0x3A709), no table word. The fixture's slot array is `DS_001077B0` itself — `0x1077B0` holds side
0's record pointer (E3_REC) and `0x107844` side 1's (E3_REC2); `0x1077A8`, the slot-pointer pair the
parent reads, is a different table and the hook never touches it.

Measured, `--function fighter_3a6d4 --self-check`:

```text
| fighter_3a6d4 | 0x3A6D4 | 6 | 12/12 | VERIFIED | 188AC stub unverified, 188DC stub unverified, 2BC30 stub unverified, 33A10 allow unverified |
| fighter_3a6d4@a16 | 0x3A6D4 | 6 | 12/12 | MISMATCH | ... |
| fighter_3a6d4@b5 | 0x3A6D4 | 6 | 12/12 | MISMATCH | ... |
```

`@a16` reads A zero-extended (`(s32)(u32)DSW(0x107D08 + side*2)` where the raw's `sar ebx,0x10`
sign-extends) and is caught on `p4` alone, by the recorded call:
`p4: call #2: original 0x188DC(0x0, 0xFFFF8001), port 0x188DC(0x0, 0x8001)` (1 diff). `@b5` drops
the B == 5 exemption (`0x3A75E/0x3A761`) and is caught on `p2` alone (B = 5, gate 0):
`p2: call #2: original none, port 0x188DC(0x0, 0x2000)` (1 diff). The other four cases are the
ones that leave each mutation unobservable (B = 0, the closed gate, the early returns), so both
cases that carry them are load-bearing.

### §P9.4 The `d15` case and the one-line port seam

`D3C_CB10` becomes `0x0003A6D4` and its stub `D3C_CB10_CALL` becomes
`E.Call(0x3A6D4, ("eax", "ebx"), clobbers=("ebx", "ecx", "edx"))` — the original's registers at
0x354E2 in the port's C argument order `(slot, side)`, and `E.callee_clobbers` over `0x3A6D4`'s
bytes gives exactly `('ebx', 'ecx', 'edx')`. Nothing else in the parent changed; its 28 cases,
25/25 blocks and 36 mutants are unchanged.

The first run showed the port side still needed its seam. Without `PR_SEAM`, the port's
`fn_resolve(0x3A6D4)` resolves the registered `fighter_pose_3a6d4` and runs its body inside the
parent's case, which
- records no call (so `d15: call #0: original 0x3A6D4(0x10A000, 0x1), port none`), and
- stores the phase-0 byte on the *side's own* slot (`d15: byte 0x10789C: original unchanged,
  port 0x01`) — the hook re-derives its slot from `DS_001077B0` through `0x33A10` and ignores the
  pointer the parent passes, so the parent's scratch-slot case is exactly where an unseamed hook
  shows.

`AGENTS.md`'s rule ("a ported function that a differentially verified function stubs or records
opens with `PR_SEAM`") is the fix: one line at the top of `fighter_pose_3a6d4`,
`PR_SEAM(0x3A6D4u, slot, side);`, arguments in the C signature's order, the `E.Call` naming the
original's registers in the same order (EAX = slot, EBX = side). It is inert outside `build/diffrun`
(`pr_seam` is NULL), so `run_tests` and the windowed run are unaffected. With it, the parent reads
`VERIFIED` again (25/25, 36/36 mutants detected) and both sides record
`0x3A6D4(0x10A000, 0x1)` on `d15`.

### §P9.5 The final line, the two closed rows, and the arithmetic

```text
diff-verify: 1031/1031 functions VERIFIED; 14983/14983 mutants detected; 0 named gaps; 813/844 rows with callees closed (187 have none); 806/844 closed on full rows; 8 ported functions with no row. ...
```

Functions `1030 -> 1031` and mutants `14981 -> 14983` are the plan's `+1`/`+2`. The two rows that
the correction closed (each reads `VERIFIED` on the callee; no verdict, case count or block count
moved anywhere):

```text
fighter_state_3531c  3D0BC stub unverified -> VERIFIED   (the d15 case's hook, now 3A6D4)
fighter_3a6d4         the new row (188AC, 188DC, 2BC30 stubs and 33A10 allow, all VERIFIED)
```

`with_callees` `843 -> 844` (the new row has four), closed `811 -> 813` (the new row plus the
parent), full `804 -> 806` (both), and open `32 -> 31` (`844 - 813`). The plan's `801 -> 802`
closed and `794 -> 795` full count only the parent (its new row as callee-less); as with §P5.4,
§P6.5, §P7.5 and §P8.5 the measured *increases* are one higher (`+2` vs `+1`) because a new row
with callees closes itself, and the absolute figure is twelve above the plan's 801/794.

Table size `16011 -> 16014` entries = the three new lines (1 real + 2 mutants); of the 1030 shared
real rows **none** changed its verdict, case count or blocks, and the callee-status column flips
exactly 37 entries over 37 rows (`3D0BC stub unverified -> 3A6D4 stub VERIFIED`: the parent and its
36 mutants). The 1031 real rows are 1030 shared + the new one; the 14983 mutants are 14981 shared +
2.

### §P9.6 The 31-row blocker census

Measured from the final table with the §J1 rule (`closed_rows`/`non_portable`), the same script
that reproduces §P8.6's 32 rows on the baseline table. The only change is the parent's row, whose
`0x3D0BC` blocker is gone (the blocker that was never a function); 23 distinct blockers, 66
incidences over 31 rows:

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
| `0x6C4CF` | stub | 2 | `config_file_load`, `menu_fatal_error` |
| `0x1C308`, `0x25FAC`, `0x26194`, `0x4FBBB`, `0x4FC05`, `0x4FCFD`, `0x4FDF5`, `0x5D86A`, `0x5DC2A`, `0x5DC4D`, `0x5DC70`, `0x5DCA6`, `0x5DCE4`, `0x65546` | stub | 1 each | (14 rows: `sound_buffers_alloc`, `game_frame_head` ×2, `input_sampler` ×4, `sound_shutdown`, `sound_sample_start` ×5, `text_number_core`) |

The 8 ported functions with no row (`UNROWED`) are unmoved: `0x3A6D4` was never in that set — the
set's eight are `symbols.h` `FN_` functions, while `0x3A6D4` is one of the E2-class entries
`port/src` implements and `fn_register`s that the decomp index never lists (like `0x3A43C` and
`0x3A588` beside it), which is the class Task 10's frontier counter is about.

### §P9.7 The harness pins, the full-scan check and `port_progress`

The harness suite (`python3 -m unittest tools.tests.test_diff_verify`) is **not run here** by
instruction (Task 11). This task disturbs **five** exhaustive pins, not the four Task 5 named — the
fifth is the stub-clobber dictionary, which a task disturbs whenever it adds or moves a stub
address (this task **moves** one, `0x3D0BC -> 0x3A6D4`; Task 8 did neither):

1. `test_the_eax_mask_is_stated_by_the_spec_per_function` — add `fighter_3a6d4` (and its mutants
   inherit the spec name only for the mask dict's `s.name` keys; only the real spec is listed).
2. `test_every_ported_function_agrees_with_the_original_on_every_block` — add `fighter_3a6d4` to
   the real-name list (a `P9C_MASKS`-style dict or a literal).
3. `test_every_mutant_is_reported_as_a_mismatch` — add `fighter_3a6d4@a16`, `fighter_3a6d4@b5`.
4. `test_each_stub_declares_the_registers_its_callee_clobbers` — **replace** the pinned dict's
   stale `0x3D0BC: ("edx",)` (`tools/tests/test_diff_verify.py:31989`) with
   `0x3A6D4: ("ebx", "ecx", "edx")`. The test asserts dict equality, so adding the new key alone
   leaves it red on the stale `0x3D0BC`. Both entries are what `E.callee_clobbers` derives from the
   bytes at their addresses (the old one from the padding read as code), and the live re-derivation
   passes.
5. The pinned counter string — §P9.5's line.

For Task 11's per-mutant "what alone catches it" maps: `@a16` → `{"call #2"}` on `p4`, `@b5` →
`{"call #2"}` on `p2` (measured above; no `P9C_KINDS` exists yet).

`PR_DIFF_FULLSCAN=1 --function fighter_3a6d4 --self-check` and
`PR_DIFF_FULLSCAN=1 --function fighter_state_3531c --self-check` are green
(`1/1 functions VERIFIED; 2/2` and `36/36 mutants detected`); neither prints a
`reads outside the image` note or an unhit block (`p0..p5` 12/12, `d0..r15` 25/25).

`python3 tools/port_progress.py` is unmoved, as required (the hook's header and `fn_register`
already existed; the seam is not a header):

```text
799 1203 66
759 759 100 (portable: excludes 53 host-owned/deferred and runtime >= 5D000)
```

`./build/run_tests` — the suite without the oracle env vars — passes ("all checks passed"): the
seam is inert outside `build/diffrun`.

### §P9.8 Files and commit

`tools/diff_verify.py` (`D3C_CB10`/`D3C_CB10_CALL`, the parent's comment, `P9C_SLOT`,
`p9c_3a6d4_case`, `P9C_SPECS` and its place in the `SPECS` concatenation — the variable is
`P9C_SPECS` because `P9_SPECS` would read as the track-P batch-9 name), `port/tests/diff_runner.c`
(the `p9c` binding block: `b_p9c_3a6d4`, the `p9c_3a6d4_core` mutant core with
`m_p9c_3a6d4_a16`/`m_p9c_3a6d4_b5`, and their three `k_bindings` entries),
`port/src/game/fighter.c` (one line: the `PR_SEAM(0x3A6D4u, slot, side)` the callee rule
requires), this record. No new `port/src` header or `fn_register`, no `data/`, no Makefile, no
`tools/tests/` change. Commit `fix: correct the 0x3D0BC spec to the real 0x3A6D4 hook`. The task
report is the git-ignored ledger.

## §P10 The unrowed figure measures the frontier

### §P10.1 What the literal could not say

The counter's clause read `8 ported functions with no row`, and the 8 was the literal set
`UNROWED` (track J j1, record §J1:26). A literal cannot move when a new `FN_` function is ported
without a row: `diffrun` would keep printing 8 while the frontier grew. This task computes the
frontier from the repo's own tables and prints the declared total, the declared portable 8, and
the scan-visible subset. The closure rule is untouched.

### §P10.2 The computation

`tools/diff_verify.py` gains two functions next to `non_portable()`:

- `fn_addresses()` reads `port/src/symbols.h` for `^#define FN_([0-9A-F]{8}) `, the parse
  `tools/port_progress.py` uses. The file has 1206 `#define FN_` lines; 1203 name a function by
  its 8-hex-digit address (`FN_0001B120`). The other three are switch-case label rows in the
  decomp index — `caseD_0`/`caseD_40`/`caseD_3` at 0x6FE85/0x7068E/0x70D1B — which the generator
  emits as `FN_CASED_0`/`FN_CASED_40`/`FN_CASED_3`; they are not function entries, and a loose
  `FN_` prefix would count them. 1203 is also `port_progress.py`'s denominator and AGENTS.md's
  real-function count.
- `ported_unrowed()` intersects `fn_addresses()` with `NonPortable.ported` (the strict
  `fn_register`/column-0 `/* 0xADDR` scan the closure rule already uses), subtracts every `SPECS`
  entry, and splits at `RUNTIME_BASE` (0x5D000). It returns a frozen `PortedUnrowed` with
  `scan_portable` (the scan-visible portable half), `runtime`, `declared_portable`
  (`scan_portable | UNROWED_HEADER_SHAPE` — the true portable figure) and the two totals
  (`total` = declared + runtime, `scan_total` = scan-visible + runtime). `specs` and `outside` are
  parameters so the guard test passes the snapshots it already read; the counter calls it with
  neither.

Measured on this tree (d642ba86, 1031 rows over 905 distinct entries): `FN_` 1203, `ported` 1262
addresses (all classes), `FN_ ∩ ported` 784, of which **42 have no row**; with the three
header-shape exceptions the declared portable figure is **8** (5 + 3) and the total **45** (8 + 37).
The scan's 42 are the 5 portable + 37 runtime; the three exceptions are `UNROWED_HEADER_SHAPE`,
outside the strict scan's `ported` set. The `42 = 5 + 37` stays as the scan-visible sub-figure.

### §P10.3 The two declared sets, and the three header-shape exceptions

`UNROWED` stays the 8 (no row changed in this task). The computation explains its shape:

| address | in the registry scan? | where port/src names it |
|---|---|---|
| 0x1B3AC | yes | `port/src/platform/res.c:57` (`/* 0x1B3AC's presentation head`) |
| 0x1BEC4 | yes | `port/src/game/flow.h:19` (`/* 0x1BEC4's init chain ...`) |
| 0x1C930 | yes | `port/src/game/flow.c:6170` |
| 0x1CF40 | yes | `port/src/game/flow.c:6051` |
| 0x47370 | yes | `port/src/game/flow.c:295` |
| 0x1B120 | **no** | `port/src/platform/res.c:180`, `res.h:1` (mid-comment) |
| 0x20C10 | **no** | `port/src/game/flow.c:6835` (`game_init`; mid-comment at 2, 121, 6896) |
| 0x4FBA2 | **no** | `port/src/game/flow.c:110` (`int10h_query`; `/* PORT: 0x4FBA2 ...`) |

The three are `UNROWED_HEADER_SHAPE`. Their implementations are real — `0x1B120` is the INDEX
walk in `res_load_index`, `0x20C10` is `game_init`, `0x4FBA2` is `int10h_query` — but each
mention is inside a comment that does not start with `/* 0xADDR`, so the strict scan misses it.
That is a named limit of the measure: it reports the registry (`fn_register`/column-0 header),
not a semantic "implements" relation, and a port added in that shape is invisible to it. The
exceptions are declared so the difference between the scan's 5 and the 8 is arithmetic, not
silence. (`0x1B3AC`, the fifth, is matched through the possessive `0x1B3AC's` at `res.c:57`:
the scan's `\b` accepts the apostrophe, and the comment heads that function's presentation half.)

The runtime set is named too, `RUNTIME_UNROWED` (37 addresses): the `FN_` functions port/src
implements at/above 0x5D000 with no row — WATCOM libc / DOS-4GW code the port serves from the
host libc, so a row of its own is not a porting target. It is a declaration, not a count: the
guard test compares it as a set.

### §P10.4 Plan-vs-raw: the 12 new rows do not move the frontier

The plan's Step 1 says the new rows reduce the computed total by the rows that were in it, and
the task brief names 10 of the 12 as `FN_` addresses. The raw says otherwise. Measured at the
plan's base 2cd2ab0d (1019 rows, 893 entries) the scan-visible frontier is 42 addresses (5
portable + 37 runtime) and the declared frontier is 45 (8 + 37, the base tree's `UNROWED` being
the same eight); measured at d642ba86 (1031 rows, 905 entries, after the 12 rows of Tasks 5-9)
both are the **same sets** (set equality checked: no address added or removed). None of the 12
can move them:

- ten of the 12 — `0x28D68`, `0x3E484`, `0x29D04`, `0x3E4C4`, `0x24568`, `0x24804`, `0x40CB0`,
  `0x2CB74`, `0x4F5C8`, `0x3A6D4` — are not `FN_` addresses at all: they are E2-class entries the
  decomp index never lists as functions, so symbols.h has no define for them;
- the two that are `FN_` functions, `0x5D812` and `0x61A70`, are binding-only rows
  (`port/tests/diff_runner.c`), so they were never in `NonPortable.ported` and never in the 42;
  their rows remove nothing either.

The brief's parenthetical is inverted (the ten are the non-`FN_` entries; the two are the runtime
bindings), and the plan's "reduces the computed total" is true in general but had no effect here.
The scan-visible frontier is 42 and the declared frontier 45, before and after; what the guard
test now catches is the next `FN_` port without a row.

Method: `git archive 2cd2ab0d tools port/src tools/port_classification.txt
port/decomp/prage.functions.csv | tar -x -C "$tmp"`, load that tree's `tools/diff_verify.py`
with its `ROOT` on the extracted tree, and compute
`(V.fn_addresses() & V.non_portable().ported) - {s.entry for s in V.SPECS}`; the same expression
on this tree gives a frozenset equal to it (`==`).

### §P10.5 The counter line

Before (d642ba86), from §P9.5:

```text
...; 8 ported functions with no row. The closure rule (record J §J1): ...
```

After (`python3 tools/diff_verify.py --diffrun build/diffrun --exe data/game/C/PRAGE.EXE
--image /tmp/opencode/p10fix.bin --self-check`, exit 0):

```text
diff-verify: 1031/1031 functions VERIFIED; 14983/14983 mutants detected; 0 named gaps; 813/844 rows with callees closed (187 have none); 806/844 closed on full rows; 8 ported functions with no row (45 FN_ ported-unrowed: 8 portable [5 registry-visible + 3 header-shape exceptions] + 37 runtime; the scan-visible subset is 42 = 5 + 37). The closure rule (record J §J1): an allow-mode callee outside the portable set - runtime >= 0x5D000, host-owned/deferred in tools/port_classification.txt, or an address the decomp index never lists as a function - does not keep a row open, provided the port tree does not implement the address in any class (no fn_register, no column-0 `/* 0xADDR` header), while a recorded stub or real callee and a ported function without a row do. Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
```

The old 8 is still the first figure and the closure rule's wording is unchanged. The new clause
is the only counter change: `45 = 8 + 37` is the declared figure, `8 = 5 + 3` its portable half,
and `42 = 5 + 37` the scan-visible subset. The **45** is what Task 11 pins. The four other pinned
harness values are Task 11's too.

### §P10.6 The guard test and its mutation proof

`tools/tests/test_diff_verify.py` (`CallParseTests.test_the_unrowed_figure_measures_the_frontier`)
asserts, using the `outside` and `SPECS` snapshots it already read:

1. `UNROWED_HEADER_SHAPE` is a subset of the `FN_` addresses and of `UNROWED`, every one is below
   `RUNTIME_BASE`, none has a row, and `scan_portable` is disjoint from the set (an exception that
   became scan-visible would otherwise be counted in both halves while the union stayed 8);
2. the declared portable set equals `UNROWED` (8) and the runtime set equals `RUNTIME_UNROWED`
   (37) — set equality in both cases, so a new port (added) and a stale declaration (removed)
   both fail with the address named;
3. the declared figures are `45 = 8 + 37`, the scan-visible subset `42 = 5 + 37`, and
   `UNROWED_HEADER_SHAPE` is 3;
4. `ported_unrowed()` returns exactly the independently computed scan/runtime sets and the
   declared portable set, so a wrong split boundary or rowed set inside the function cannot pass.

Mutation proofs, each run alone and reverted:

- a fake column-0 header `/* 0x10024 ... */` in `port/src/mem.c` (an `FN_` function with no row)
  failed with `FN_ ported-unrowed below 0x5D000 moved: new with no row: 0x10024 (port one or
  declare it in diff_verify.UNROWED); declared but rowed or unported now: none (shrink UNROWED)`;
- a fake header for `0x5F758` (an `FN_` runtime address with no row) failed with `FN_
  ported-unrowed at/above 0x5D000 moved: new with no row: 0x5F758 (port one or declare it in
  diff_verify.RUNTIME_UNROWED); declared but rowed or unported now: none (shrink the set)`;
- an in-process spec row for `0x1B120` (a declared exception) failed with `a header-shape
  exception gained a row (shrink UNROWED_HEADER_SHAPE and UNROWED): 0x1B120`;
- an in-process spec row for `0x1B3AC` (a declared portable address) failed with `declared but
  rowed or unported now: 0x1B3AC (shrink UNROWED)`;
- a fake column-0 header for `0x1B120` (making a declared exception scan-visible) failed the
  disjointness assertion with `an exception became scan-visible (0x1B120): the printed
  arithmetic would read 6 + 3, not 5 + 3`. Without that guard the union would still be the 8 (the
  address is in both halves), so only the arithmetic guard catches the overlap.

`port/src/mem.c` was byte-restored after each file-level mutation (`git hash-object` equals
`HEAD:port/src/mem.c`). The spec-row mutations appended a `V.Spec` to `V.SPECS` in the test
process, so no file changed.

### §P10.7 Verification

- `python3 -m unittest tools.tests.test_diff_verify -k test_the_unrowed_figure_measures_the_frontier`
  — 1 test, OK.
- the 17 classes that do not run the real build (`ParseTests` ... `H2DeclaredTests`) — 95 tests,
  OK.
- the direct driver line above — exit 0, `1031/1031` functions and `14983/14983` mutants; the
  closed/full counts are unmoved (`813/844`, `806/844`; 31 open) and the frontier clause reads
  `45 ... 8 ... 5 ... 3 ... 37 ... 42`.
- `python3 tools/port_progress.py` — `799 1203 66` / `759 759 100`, unmoved (no `port/src`
  change).
- The pinned counter unittest (`test_the_self_check_counts_functions_mutants_gaps_and_closed_rows`)
  is expected red until Task 11 re-pins the string; it was not run to completion here (it re-runs
  the whole driver). No other `tools.tests` case moved.

### §P10.8 Files and commit

`tools/diff_verify.py` (`UNROWED_HEADER_SHAPE`, `RUNTIME_UNROWED`, `fn_addresses()`, the
`PortedUnrowed` return and `ported_unrowed()`, the counter clause), `tools/tests/test_diff_verify.py`
(the one guard test), this record. No `port/src` change, no row, no `data/`, no Makefile, no
pinned harness value. Commit `diff: measure the ported-unrowed frontier` (6c49fcce); the §P10.9
fix is the follow-up commit.

### §P10.9 Fix: the declared total is 45, and the halves are disjoint

Review of the first implementation found two defects in the measure, both corrected here (no
counter pin, no row, no port code touched):

- **The printed total undercounted.** `ported_unrowed()` returned only the scan-visible `(5, 37)`
  and the counter printed `42 computed FN_ ported-unrowed: 5 portable + 37 runtime`, although the
  three header-shape exceptions are genuine `FN_` ported-unrowed addresses: the true portable set
  is 8 and the true total 45. The function now returns a `PortedUnrowed` whose
  `declared_portable` is `scan_portable | UNROWED_HEADER_SHAPE` (8) and whose `total` is 45;
  `scan_portable` (5) and `scan_total` (42) stay as the scan-visible subset. The counter prints
  `45 FN_ ported-unrowed: 8 portable [5 registry-visible + 3 header-shape exceptions] + 37
  runtime; the scan-visible subset is 42 = 5 + 37` (§P10.5 has the whole line). This is the
  figure Task 11 pins.
- **The union permitted overlap.** `portable | UNROWED_HEADER_SHAPE == UNROWED` alone would pass
  if an exception became scan-visible: it appears in both halves, the union stays 8, and the
  printed `5 + 3` becomes `6 + 3`. The test now asserts `scan_portable` is disjoint from the
  exception set, and that all three exceptions are below `RUNTIME_BASE`. Mutation: a fake
  column-0 header for `0x1B120` fails with `an exception became scan-visible (0x1B120): the
  printed arithmetic would read 6 + 3, not 5 + 3`.
- The test now passes the `outside`/`SPECS` snapshots it already read
  (`ported_unrowed(specs=V.SPECS, outside=outside)`) instead of re-scanning, and asserts the
  declared figures `(8, 8, 5, 37, 45, 42, 3)`.
- `ported_unrowed()`'s docstring no longer says `portable` is "the figure UNROWED describes (plus
  UNROWED_HEADER_SHAPE)": it names `scan_portable` as the subset and `declared_portable` as the
  true portable figure.

Verification after the fix: the guard test alone passes; the five mutation proofs in §P10.6 fail
as quoted; the 17 classes that do not run the real build (95 tests) pass; the direct driver's
line is the §P10.5 one (`1031/1031`, `14983/14983`, `813/844`, `806/844`, 31 open);
`port_progress.py` unchanged. Fix commit `diff: count the declared 45 in the unrowed figure`.

## §P11 The phase gate, the final counter and the handoff

Task 12 of the plan, and the phase's last section. The plan calls this record section
§P6; the record's §P6 is the fighter callbacks (`0x3E484`, `0x29D04`, `0x3E4C4`),
because Track A's four tasks took §P1–§P4 and the differential tasks then took §P5–§P10
one apiece (the record keeps the measured section number, not the plan's). Everything
below was measured on the phase tree at `4a9b46cc` (branch `pixel-and-proof-closure`,
base `2cd2ab0d`); it touches this record and `docs/PROGRESS.md` and no `port/`, `tools/`,
`data/` or Makefile file.

### §P11.1 The full gate

```bash
PR_DIFF_FULLSCAN=1 make verify        # rc 0
```

`PR_DIFF_FULLSCAN=1` runs each `diffrun` binding's outside-the-image scan over the whole
63 MB instead of the write-fault pages — the form AGENTS.md asks for before a push, and
the one this phase's review focus #4 names (every new binding and mutant inside `mem[]`).
One uninterrupted invocation, about 58 minutes wall for the shell call, with `EXIT=0`. Every
stage it reaches passes:

| stage | result line (verbatim) |
|---|---|
| the game dir's `CMOS` | `cmos=zero` |
| headless `--check 820` | `fn-miss check distinct=2 dropped=0` |
| tests (oracle-required) | `all checks passed` |
| restart driver | `test_restart_drive: restart after 1198 menu iterations (stamp 3E, clock 4EF)`; `all checks passed` |
| smacker | `smk_compare: 120/120 frames match`; `smk_compare: 41/41 frames match` |
| title | both captures `111 frames in window` with `0 unexplained`; `port frames exhibited 95/96; missing [0]; endpoints OK`; `determinism: clean samples of 54 port frame(s) agree, 0 disagree` |
| front-end | `1325 frames in window: 517 clean, 801 splice, 3 transition, 2 unexplained`; `2 unexplained captured frame(s) allowed by name: [(832, 3671), (833, 3740)]; no other unexplained frame in the window.`; `port frames exhibited 1145/1382` (its missing-frame list elided), `endpoints OK` |
| demo-fight | `0 unexplained in the fight window`; `fully explained; the window claim is now exact` |
| attract cycle-2 | `1732 frames in region: 1078 clean, 630 splice, 17 transition, 1 unexplained, 6 all-black` (3545 allowed by name); `0 unexplained in the region` |
| attract prefix | both captures `215/216 capture frames explained; port attract frames exhibited 173/690`; `expected divergence at capture frame 215` |
| K11 walk | `81 frames in window: 47 clean, 2 splice, 0 transition, 2 unexplained, 30 all-black` (138, 151 allowed by name); `settled screens exhibited 38/38`; `non-black port frames exhibited 48/48`; `0 unexplained in the window` |
| K11 menuesc | `283 frames in window: 184 clean, 92 splice, 3 transition, 0 unexplained, 4 all-black`; `2/2`; `184/184`; `END 388 must be >= 388: ok` |
| save-reload | `the replay wrote 2040 bytes identical to the capture's CMOS (sha256 dad3b1f6…)`; `the port holds the capture's five ranges at f=01EB and every later dumped frame (3 compared)` |
| gp-pads replay | `all checks passed` |
| differential | `Ran 217 tests in 565.452s` / `OK` (the harness suite), then the counter line in §P11.2 |
| E2 triage | `Ran 44 tests in 6.739s` / `OK`; `579 candidates (575 by U0's rule)`; `targets 233 unported, 262 ported; supplement 131 (3 unported, 0 stale); untrusted entries 30`; `voice sites outside Ghidra 134: 0 in unported code, 115 in ported code, 19 nowhere`; the committed table is byte-identical (`--check` exits 0) |
| k11/gp tool tests | `Ran 295 tests in 2.912s` / `OK` |
| title_compare tests | `Ran 12 tests in 0.097s` / `OK` |
| asset extractor | `Ran 33 tests in 1.024s` / `OK` (oracle-required) |
| `symbols.h` | `1304 globals, 1206 functions -> port/src/symbols.h`; regenerates byte-identically (`git diff --quiet` passes) |

The gate's tail, verbatim (the last line is the wrapper's `echo "EXIT=$?"`, not make's):

```text
== symbols.h must regenerate byte-identically ==
python3 tools/gen_symbols.py port/decomp port/src/symbols.h
1304 globals, 1206 functions -> port/src/symbols.h
  dropped 11 globals, 1 functions outside the LE objects
all checks passed
EXIT=0
```

The gameplay oracles, each at its pin with no `improved` suffix and no FAIL (`first
unexplained` | trace | the scenario's other ratchets):

| scenario | frames | trace | other |
|---|---:|---:|---|
| `gp-idle-loss` | 7559 | 8320 | — |
| `gp-u5-charsel` | 516 | 1513 | — |
| `gp-u6-moves-b` | 2139 | 3248 | moves 3248 |
| `gp-keys-fight` | 1136 | 2164 | effects 11; `gp_win` evidence ok |
| `gp-twop` | 612 | 1506 | moves 1506 |
| `gp-u8-right-arcade` | 1072 | 2274 | — |
| `gp-u8-left-training` | 1076 | 2338 | — |
| `gp-u8-right-training` | 1098 | 2402 | — |
| `gp-u8-tug-of-war` | 1107 | 2466 | — |
| `gp-u8-handicap` | 1022 | 2274 | — |
| `gp-u8-endurance` | 278 | 1174 | — |
| `gp-u8-attract-start` | 1087 | 2018 | — |
| `gp-u12-endurance` | 732 | 1931 | — |
| `gp-u13-ending2` | 5891 | 10014 | reach 4939, win 10014, milestones 30/30 |
| `gp-u14-win-live` | — | — | `no capture … (skipped)`, rc 0 by design (§J6) |
| `gp-u15-join` | 805 | 2158 | — |
| `gp-u16-twop2` | 8069 | 8637 | moves 8637 |
| `gp-u17-device2` | — | — | evidence ok; the frame/trace ratchets are empty by name (§H6.9) |
| `gp-u9-win` | 2452 | 3503 | win 3503, path 8 |
| `gp-u10-ending` | 5437 | 9954 | win 9954, path 30 |

Every capture identity line matches its pin (`… matches the pin`; U17's evidence block says
`the capture identity matches the pin`), U13's `poll.log sha256 e581916f..2b69c6, 5904
frames` among them, so no capture file changed.

### §P11.2 The counter, the census and the unrowed measure

The gate's differential line is Task 11's pin, byte for byte:

```text
diff-verify: 1031/1031 functions VERIFIED; 14983/14983 mutants detected; 0 named gaps; 813/844 rows with callees closed (187 have none); 806/844 closed on full rows; 8 ported functions with no row (45 FN_ ported-unrowed: 8 portable [5 registry-visible + 3 header-shape exceptions] + 37 runtime; the scan-visible subset is 42 = 5 + 37). The closure rule (record J §J1): an allow-mode callee outside the portable set - runtime >= 0x5D000, host-owned/deferred in tools/port_classification.txt, or an address the decomp index never lists as a function - does not keep a row open, provided the port tree does not implement the address in any class (no fn_register, no column-0 `/* 0xADDR` header), while a recorded stub or real callee and a ported function without a row do. Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
```

The arithmetic, each term measured (§P5–§P9 and Task 11's report carry the derivations):
`833 + 11 = 844` rows with callees (11 of the 12 rows have callees; only `runtime_5d812`
has none — the `0x61A70` row's two callees, `0x65490`/`0x654C7`, are allow-mode),
`1019 + 12 = 1031` functions, `14956 + 27 = 14983` mutants, `774 + 11 + 28 = 813` closed,
`768 + 11 + 27 = 806` full, `59 - 28 = 31` open. The twelve rows of this phase:

| row | entry | cases | mutants | plan task |
|---|---|---:|---:|---|
| `runtime_5d812` | `0x5D812` | 2 | 1 | 5 |
| `frontend_28d68` | `0x28D68` | 1 | 3 | 5 |
| `fighter_3e484` | `0x3E484` | 3 | 4 | 6 |
| `fighter_29d04` | `0x29D04` | 3 | 3 | 6 |
| `fighter_3e4c4` | `0x3E4C4` | 2 | 2 | 6 |
| `fighter_24568` | `0x24568` | 8 | 3 | 7 |
| `fighter_24804` | `0x24804` | 8 | 3 | 7 |
| `fighter_40cb0` | `0x40CB0` | 8 | 3 | 7 |
| `svc_2cb74` | `0x2CB74` | 1 | 1 | 8 |
| `flow_4f5c8` | `0x4F5C8` | 5 | 1 | 8 |
| `runtime_61a70` | `0x61A70` | 4 | 1 | 8 |
| `fighter_3a6d4` | `0x3A6D4` | 6 | 2 | 9 |

51 cases and 27 mutants together. Two of the twelve (`0x5D812`, `0x61A70`) are
binding-only rows: the port registers no address for them (the plan forbids a header or
`fn_register`), so `port_progress.py` cannot move — and it did not (§P11.6).

**The open-row census.** Re-measured from the gate's own table (`--table`, the same
`closed_rows`/`non_portable` rule as §P9.6): 1031 rows, 844 with callees, 813 closed, 806
closed on full rows, **31 open**, blocked by **23 distinct `(address, mode)` pairs, 66
incidences**. Every blocker is a recorded `stub`; by class they are 16 runtime
(≥ `0x5D000`), 5 host-owned (`0x1C308`, `0x4FBBB`, `0x4FC05`, `0x4FCFD`, `0x4FDF5`) and 2
addresses the decomp index never lists as functions (`0x25FAC`, `0x26194`). The block is
by design: a recorded stub's declaration is exactly what a row of its own would verify,
so the row stays open, and none of the 23 is in the portable set (no row can exist) — the
closure rule's two-sided condition (§J1).

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
| `0x6C4CF` | stub | 2 | `config_file_load`, `menu_fatal_error` |
| `0x1C308`, `0x25FAC`, `0x26194`, `0x4FBBB`, `0x4FC05`, `0x4FCFD`, `0x4FDF5`, `0x5D86A`, `0x5DC2A`, `0x5DC4D`, `0x5DC70`, `0x5DCA6`, `0x5DCE4`, `0x65546` | stub | 1 each | (14 rows: `sound_buffers_alloc`, `game_frame_head` ×2, `input_sampler` ×4, `sound_shutdown`, `sound_sample_start` ×5, `text_number_core`) |

The 31 open rows, in table order: `snd_music_stop`, `snd_sample_playing`,
`snd_samples_stop_all`, `snd_sample_stop`, `snd_sample_queue`, `snd_music_playing`,
`text_number_core`, `input_sampler`, `sound_music_pause_toggle`, `sound_music_volume`,
`sound_sfx_volume`, `sound_buffers_alloc`, `attract_config_volumes_unscaled`,
`attract_config_volumes`, `sound_sample_start`, `fight_char_screen_open`,
`fight_char_screen_open_both`, the ten `0x24C5C` rows (`game_frame_keys`,
`game_frame_keys2`, `game_frame_head`, `game_frame_fight`, `game_frame_tails`,
`game_frame_m25`, `game_frame_match`, `game_frame_front`, `game_frame_start`,
`game_frame_rr`), `game_quit_prompt`, `config_file_load`, `menu_fatal_error` and
`sound_shutdown`. No row blocks on a portable-set function by accident: every blocker is
outside the portable set, and the only ported functions with no row at all are the
declared unrowed set below.

**The unrowed measure** (Task 10, corrected in §P10.9): the counter prints `8 ported
functions with no row` (still `UNROWED`'s literal) followed by the computed `45 FN_
ported-unrowed: 8 portable [5 registry-visible + 3 header-shape exceptions] + 37 runtime;
the scan-visible subset is 42 = 5 + 37`. The three header-shape exceptions are `0x1B120`,
`0x20C10` and `0x4FBA2` (implemented, but named only inside a comment that does not start
with `/* 0xADDR`, so the strict scan cannot see them); the guard test
`test_the_unrowed_figure_measures_the_frontier` fails on a new `FN_` port without a row,
on a stale declaration, and on an exception that became scan-visible (§P10.6).

**The weaker rows** (a row that runs the original under a code poke, leaves a reachable
block out, or declares a terminal difference; it can still be closed). Twelve functions
carry one: `0x1AFE8` (`config_file_load`, the l6 short-read exit difference), `0x1B084`
(`config_file_save`, the exit block a memory-backed file cannot reach), `0x1B544`
(`res_resolve`, the resident arm only), `0x1BE30` (`game_shutdown`, `int 0x10` poked),
`0x1C740` (`movie_play`, the keyboard poll poked), `0x24C5C` (`game_frame_*`, 62/161
blocks on `game_frame_keys`), `0x255CC` (`game_loop`, 18/20 blocks, two named unhits
`0x25680`/`0x256D4` and its RNG a recorded stub), `0x2CADC`, `0x2EA30`, `0x2EA68`,
`0x2EA74`, `0x2EA78`. Seven of their rows are closed (so `806 = 813 - 7`); three of the
other five entries are among the 31 open rows above (`0x1AFE8`, `0x24C5C`, `0x2EA68`);
`0x1B544` and `0x2EA30` have callee-less rows, so they never enter the closed/full
arithmetic. On the function scale the docs keep, 12 of the 751 portable rowed functions
are weaker and 739 are full.

### §P11.3 The final U13 pins and their failing direction

| pin | value | provenance |
|---|---:|---|
| `GP_U13_MIN_FIRST` | **5891** | the first unexplained capture frame, raw 14361; 5891..5903 are the post-script tail (§P1.6) |
| `GP_U13_MIN_REACH` | **4939** | the enforced comparison's window reach at that frame (§P4.1) |
| `GP_U13_TRACE_MIN_FIRST` | 10014 | 0 differing through 10013 |
| `GP_U13_WIN_MIN_FIRST` | 10014 | 0 differing through 10013 |
| `GP_U13_MAX_START` | 96 | the run prints `window from capture 96` |
| `GP_U13_MILESTONES` | 30 | 30/30, the BLIZZARD row's `ad4=0, c0=1` included |
| `GP_U13_CAPTURE_SHA256` | `e581916f…2b69c6` | the capture identity line matches |
| `GP_U13_CAPTURE_FRAMES` | 5904 | matches |

The gate's enforced run at these pins prints `first unexplained 5891, ratchet N 5891 ok`
(no `improved` suffix) and `trace: 0 differing through 10013; ratchet N 10014 ok`,
`path: 0 not reproduced through 29; ratchet N 30 ok`, `win: 0 differing through 10013;
ratchet N 10014 ok`, and the four allowance lines (§P4.4). Each pin fails one step above
(§P4.3, measured on this tree; the pins are Makefile assignments, so the override must be
on the command line — the environment form runs at the pin and passes, §P4.1):

```text
$ make gp-u13-oracle GP_U13_MIN_FIRST=5892
gp_compare: gp-u13-ending2: frames: FAIL: first unexplained 5891 < ratchet N 5892
make: *** [Makefile:1282: gp-u13-oracle] Error 1

$ make gp-u13-oracle GP_U13_MIN_REACH=4940
gp_compare: gp-u13-ending2: frames: FAIL: window reach port 4939 < pinned reach 4940 (the port exhibited fewer port frames)
make: *** [Makefile:1282: gp-u13-oracle] Error 1
```

The three in-replay allowances that moved the first unexplained from 4248 to 5891 are
§P2's and §P3's: capture 4248 (raw 7887) and 4320 (raw 8019) as three-present scan-outs,
5120 (raw 8839) as a four-row band, each a named per-frame byte recipe bound to the
port's own frames and mutation-tested (a changed port byte un-explains the frame). The
window's claim is as narrow as the other gp scenarios': no content-bearing capture frame
through 5890 is unexplained and the window reaches its end; port-frame order,
under-rendering inside the window, and the 5891..5903 tail are not claimed.

### §P11.4 The raw correction: `0x3D0BC` is padding, not a function

The plan's Task 9 carried a correction: `0x3D0BC` — the address `fighter_state_3531c`'s
old `d15` case poked into its scratch slot's `+0x10` hook — is four zero bytes of the
five-byte alignment pad after `0x3D004`'s `ret` (`0x3D0BA`), not a function (§P9.2). The
decomp index has no entry there; the real entry is `0x3D0C0`, which already had a row
(`fighter_3d0c0`). The slot `+0x10` hooks the pose setters store are `0x3A6D4`
(`0x3A7D2`) and `0x3A588` (`0x3A686`), and the only call through the dword is
`0x354DA..0x354E2` in `0x3531C` case 10. The old case compared the address alone (the
port reports an unregistered address with no arguments), so it could not notice; the row
`fighter_3a6d4` (6 cases, 2 mutants, 3 recorded calls) runs the real function, and the
stub-clobber pin moved from `0x3D0BC: ("edx",)` to `0x3A6D4: ("ebx", "ecx", "edx")`,
re-derived from the bytes (`E.callee_clobbers`).

### §P11.5 The named limits

The phase closes the two offline weaknesses it set out to close (U13's in-replay pixels
through capture 5890, and the differential's rowable frontier) and leaves these limits,
each with its evidence:

- **U13's post-script tail.** 5891..5903 (raw 14361..14417) is the 13-frame tail after the
  scenario's `X` record (f=10013); the port's dump ends at frame 4939 (f=0x271B) and its
  trace at f=0x271D, so no frame exists for those states and the pin stops there (§P1.6).
- **The 5120 band's cause.** Capture 5120 is explained by name (§P3.3) with a byte-pinned,
  mutation-tested recipe (§P3.4), but the state that produced its four-row band is not
  isolated: no dumped DAC, no dumped present and no whole-frame offset reproduces it, and
  the band's bounds are capture-pinned, not machine-forced (`tools/gp_compare.py:106`) —
  the same limit as U11's 81/92. The allowance binds the band's bytes, not a cause.
- **U14's fought live win.** `gp-u14-win-live` has no capture and all `GP_U14_*` pins stay
  empty (`make gp-u14-oracle` skips, rc 0); the two permitted attempts failed the session
  checks (§J6). The poked `DEATH_DONE` wins of U9/U10 are not a substitute and are not
  claimed for U14.
- **U16's finisher.** The three finisher entries were attempted but no `r0`/`r1` reaction
  in `0x32..0x34` appears; `make gp-u16-oracle` passes `--finisher
  --allow-missing-reaction` (`Makefile:1435`), so the oracle does not claim a reaction
  (R2, §J8).
- **The other endings.** `gp-u13-ending2` covers character 1's BLIZZARD ending; character
  0's is the U10 path under `DEATH_DONE` pokes (`gp-u10-ending`, first unexplained 5437,
  the replay's post-script tail) and what the pokes replaced — the KO by hits and the six
  earlier won matches — is not claimed (§W.9). No other character's ending has a capture.
- **The runtime/host blockers.** All 31 open rows block on recorded stubs outside the
  portable set — 16 runtime addresses, 5 host-owned routines (the AIL/presence family),
  2 addresses the decomp index never lists. None can have a row, so those rows keep their
  place in the counter by design (§J1, §P11.2).
- **The weaker rows.** The 12 ways above (pokes, unhit blocks, terminal differences) are
  declared, not proved away; the `0x24C5C` rows' partial block coverage (62/161 on
  `game_frame_keys`) and `game_loop`'s two named unhits are the largest, and the counter's
  headline does not distinguish a weaker row from a full one — only its `closed on full
  rows` figure does.
- **`flow_4f5c8`'s zero divisor.** No case uses divisor 0: the raw's `idiv` would fault at
  `0x4F5E0`, and the port returns 0 instead — a documented deviation (`flow.c`'s PORT
  comment, the same form as `0x4F4E8`'s), so that path is unverified on both sides by
  construction.
- **The oracles' claim width.** Each gameplay oracle proves only that no content-bearing
  capture frame in its pinned window is unexplained and that the window reaches its end
  (plus the state-field ratchets); port-frame order, under-rendering inside a window and
  full interior coverage are not claimed for any of them (AGENTS.md's gp-oracle note).

### §P11.6 The `port_progress` lines, the PROGRESS paragraph and the commit

`python3 tools/port_progress.py` is unmoved, as required (no `port/src` file changed by
this task; the phase added no new `FN_` header and no `fn_register`, only seams such as
`0x3A6D4`'s):

```text
799 1203 66
759 759 100 (portable: excludes 53 host-owned/deferred and runtime >= 5D000)
```

`docs/PROGRESS.md`'s phase paragraph:

> **Pixel and proof closure, Tasks 5-12: the differential last mile, the unrowed measure
> and the phase gate (plan `2026-10-22-pixel-and-proof-closure.md`, record §P5-§P11).**
> The phase's differential half rowed twelve more functions — the frontend stub pair
> `0x5D812`/`0x28D68`, the fighter callbacks `0x3E484`/`0x29D04`/`0x3E4C4`, the actor
> entrances `0x24568`/`0x24804`/`0x40CB0`, the menu, loop and memset entries
> `0x2CB74`/`0x4F5C8`/`0x61A70` and the real `+0x10` hook `0x3A6D4` — in 51 cases and 27
> mutants, so `make diff-verify` reads `1031/1031 functions VERIFIED; 14983/14983 mutants
> detected; 0 named gaps; 813/844 rows with callees closed (187 have none); 806/844
> closed on full rows; 8 ported functions with no row (45 FN_ ported-unrowed: 8 portable
> [5 registry-visible + 3 header-shape exceptions] + 37 runtime; the scan-visible subset
> is 42 = 5 + 37)`. The open rows fall 59 -> 31 (23 blockers, 66 incidences; record
> §P11.2), all on recorded stubs outside the portable set (16 runtime, 5 host-owned, 2
> not in the decomp index), and the unrowed figure now measures the frontier instead of
> restating a hand-maintained set (the guard test fails on a new `FN_` port without a
> row). The `0x3D0BC` spec was corrected: it is four zero bytes of alignment padding, not
> a function, and the real hook is `0x3A6D4`. Six pins in
> `tools/tests/test_diff_verify.py` were re-measured from the driver's own line — the
> counter string, the EAX-mask dict, the real-spec-name list, the mutant-name list, the
> stub-clobber dict, and Task 10's frontier guard test (already green) — and no row,
> closure rule or port code changed with them. The pixel half re-pinned U13: with the
> named per-frame recipes for capture 4248, 4320 and 5120, `gp-u13-ending2`'s first
> unexplained capture frame is 5891 (the post-script tail 5891..5903) and the enforced
> reach is port 4939 (`GP_U13_MIN_FIRST` 4248 -> 5891, `GP_U13_MIN_REACH` 3549 -> 4939,
> each proved to fail one step above; trace/WIN stay 10014, start 96, milestones 30/30,
> the capture sha and 5904 frames unchanged). `PR_DIFF_FULLSCAN=1 make verify` is green on
> the phase tip, `port_progress.py` is unmoved (`799 1203 66` / `759 759 100`) and no pin
> moved outside the U13 pair and the six re-measured harness pins. **Remaining
> (capture-gated or by design):** U14's fought live win
> (R1, no capture; all `GP_U14_*` pins empty), U16's finisher (R2), the other endings
> beyond character 1's BLIZZARD and character 0's poked U10 path, U13's 5891..5903 tail,
> the 31 open rows' runtime/host stubs (no row can exist), the 12 weaker rows and
> `flow_4f5c8`'s documented zero-divisor deviation.

Files: this record and `docs/PROGRESS.md` only. No `data/` file changed (the gate's
`cmos=zero` and every capture identity line) and no capture was taken. Commit
`docs: close the pixel and proof phase` (a commit cannot cite its own hash; the task
report carries it).
