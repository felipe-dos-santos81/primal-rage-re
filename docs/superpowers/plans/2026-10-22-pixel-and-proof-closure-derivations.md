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
| 5120 | no | no — pair N=4316 (4316,4317), 13 differing bytes | no — 196 rows at N=4316 | **yes — the intersection is {−29, +14, +29}, each covering 13/13** |

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

So the frame model (clean, splice, transition) explains none of the three, and
the DAC-index test is negative for 4248 and 4320 but **positive for 5120**.
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
  named top rows"), and `band_pair(88, 108)` is `None`;
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
differ: rows 4..17 hold 120 of them (1, 6, 6, 9, 11, 5, 1, 9, 18, 25, 29 by
row) and rows 88..107 hold 153 (4, 7, 4, 4, 3, 4, 8, 5, 5, 4, 2, 8, 16, 8, 6,
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
(1280 bytes, sha256 `56b900b1…`). The unheld rows are *only* 1..4, which is why
the band is 1..4 and not wider.

The 13 differing pixels, with port 4317's own index and DAC entry:

| pixel | capture | port RGB | port index | DAC entry | count of that index in the frame |
|---|---|---|---:|---|---:|
| (1,23) (1,24) (1,295) (1,296) (2,22) (2,297) (3,21) (3,298) | `00001c` | `000000` | `0x32` | `000000` | 101 (93 black in the capture) |
| (3,74) | `000000` | `100000` | `0x2E` | `100000` | 96 (95 match) |
| (4,27) (4,28) (4,291) (4,292) | `000000` | `00000c` | `0x4D` | `00000c` | 54 (50 match) |

**Correction to §P1.4/§P1.9's DAC reading (raw wins).** The intersection of the
per-pixel offset sets over the *differing* pixels is `{-29, +14, +29}`, but that
is a DAC **alias**, not a palette shift: `00001c` occurs at five entries
(0x15, 0x40, 0x42, 0x43, 0x4F) and black at two hundred. The discriminating
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
