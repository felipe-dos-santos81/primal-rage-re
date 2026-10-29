# State coverage — design

Status: **Implemented and gated**, 2026-10-28, gated 2026-10-29 (two full
`PR_DIFF_FULLSCAN=1 GATE_STRICT=1 PR_PINS_EXACT=1 make verify` runs plus
`make audio-render`, both green; record
`docs/superpowers/plans/2026-10-28-state-coverage-derivations.md` §D8). Follows
claim-coverage, proof completion, endgame truth, music truth and fought-match
truth. The port is complete against its declared population (`798 1203 66`
raw, `759 759 100` portable) and the differential harness reads `1033/1033`
functions, `14993/14993` mutants, 0 named gaps. What remains is not code
coverage of the game: it is **evidence coverage of the game's states**, plus
one measured port divergence and two model questions (a 13-pixel band and the
OPL core) that the existing oracle families cannot decide. §7's predicted
figures are superseded by the measurements in the record §D1–§D8, itemised in
§11 below.

## 1. Intent and why

Claim coverage asked whether every *claim* has a gate. This phase asks the next
question down: **does every state the game can enter have an exhibit, or a
declared reason it does not?**

Recon for this design measured five things the existing records state less
strongly than the evidence supports:

1. **The mode table has unexhibited content handlers.** The 39 `poll.log`s on
   disk exhibit 35 of the 52 table modes; the never-presented set is
   `01 02 04 0A 0B 0E 18 19 1C 1D 20 21 25 26 2F 32 33` (measured today from
   `data/k11-captures/*/poll.log`). Six are declared no-content entries
   (`01`/`02`/`20` are `game_null_step`, `1C`/`1D`/`26` are bare `break`s at
   `flow.c:7894-7897`), one shares its body with exhibited modes (`04`, the
   `0x26254` fight frame) and one is a byte-for-byte duplicate (`2F` of `2E`,
   `flow.c:7510`). The remaining **content** modes have zero capture frames:
   `0A`/`0B` (the KO-branch drawn match; claim-coverage Task 11 holds the
   scenario and has no capture yet), `0E` (the continue/toll screen,
   `game_mode_0e_step` `0x27A2C`, `flow.c:1967`), `21` (the volleyball
   easter egg, entered at `0x4BD98`, `fight.c:2945`, after the eighth
   **worshipper trample**, `fight.c:3080`), `25` (the both-characters-4
   camera sequence, entered at `0x4E11C`, `fight.c:5575`) and `32`/`33`
   (ENDURANCE's team advance and match end, `flow.c:2124`/`2651`).
   `18`/`19` are effects-gated transitions with no content
   (`flow.c:1250`/`1272`). **Two of these gates are two-human-only**:
   `DS_00104ABC` is `(DS_00104B1F == 3) + 1` (`flow.c:1621`, `flow.c:2405`),
   so the **in-match** `0x4BD98` and `0x4E11C` entries both require `b1f == 3`
   (a two-human match), not a poked 1P win. That makes the in-match `0x21`
   (volleyball) and `0x25` entries the hardest in the set and not part of this
   phase's exhibits. The attract's own volleyball (the same mode-`0x21`
   handler) is outside every oracle's capture window and is not proven
   either way; the census records it, the phase does not take it.
2. **There is no per-driver function-hit instrument.** `fn_resolve`'s miss log
   records misses only (`mem.c:73` returns before the record when the function
   is found), and `PR_SEAM` is NULL outside `build/diffrun` (`mem.c:61`). The
   port therefore cannot say which of its 759 portable functions any oracle
   actually entered; the unexhibited-mode list above had to be reconstructed
   from `poll.log` modes by hand. The span-writer census (proof completion
   §P2-§P4) showed what a measured census buys.
3. **U16 R2 is a known port divergence with a measured shape.** The original's
   `slot+0x30` and the port's agree exactly at `0x14F0..0x14FD`, then diverge:
   the original drops to `0xFFFFFD00` at `0x14FE` and stays **`0x1180` below**
   the port at every sampled frame `0x1501..0x1531` (the port's `0x0D40` vs the
   original's `0xFFFFFBC0` at `0x1501`, the port's `0x1B40` vs `0x09C0` at
   `0x1531`). The port's dwell at its own exit `0x155C` is `0x1180`, the
   original's exit value at `0x1533`; its post-exit curve is the original's
   shifted by `0x29` frames. The split between the record's `+0x1C` and the
   `0x100AB4` anchor term is **not yet measured** — the constant `0x1180`
   difference holds while the anchor index walks, which points at the record,
   but the original's `ab4` is unmeasured. The U23 oracle records the moves
   difference at `0x155B`; the phase measures the split and then fixes it.
4. **The 5120 band's cause is decidable with one read.** The band at
   `gp-u13-ending2` capture 5120 is 4 rows × 320 pixels (1280 pixels, 3840
   bytes), of which exactly 13 pixels differ from port 4317's rows: 8 are
   `00001c` where 4317's index is `0x32` (black in that frame's DAC), 1 is
   black where the index is `0x2E` (`100000`) and 4 are black where it is
   `0x4D` (`00000c`). The capture's neighbour 5119 is clean at port 4315
   (`f=1824`) and 5121 is a `(4317,4318)` splice, so the band sits at the
   `f=1825/f=1826` fade step; the pair `(4316,4317)` is the dump's unique
   match (`tools/gp_compare.py:243-275`). Rows 1..4 are the dump's only unheld
   rows, and no port DAC renders index `0x32` as `00001c`, so no single
   palette explains the 13. The unmeasured surface is the real aperture at
   the S record (the poller reads the memory file: `tools/gp_capture.py:36`'s
   `read_snap`, over the map in `tools/k11_capture.py:6-10`). A 14-byte
   diagnostic read at every S record decides between "not video memory"
   (capture-pipeline artifact) and "the aperture holds it" (a port divergence
   with a nameable writer).
5. **U14's acceptance rule is unsatisfiable on the scenario's own path.**
   `tools/gp_win.py`'s `live_death` requires a game-set `b0c=1` after the KO.
   `b0c`'s only non-zero store is `0x37FF6` in `0x37EA0`'s remains stream
   (`fighter.c:14402`), and every measured non-zero is after the **seventh**
   final KO (U10 `f=0x1602`, U18 `0x15F7`, …); `b0c` is 0 in all 3,541 `S`
   records of the poked match-1 win (`gp-u9-win`) and in all 9,471 of U14's two
   fought KOs. The U14 scenario
   stops at mode `0x12` in match 1. A further attempt cannot pass the rule
   whatever P1 does; the rule, not the capture, needs re-scoping.

The thesis: **a ported handler with no exhibit is a claim the port has not
made.** This phase builds the instrument that measures the remaining
unexhibited set, takes the exhibits that close the content-bearing part of it,
fixes the one divergence whose cause is already measured, and decides the two
model questions with experiments that can come back negative.

## 2. Current state (measured 2026-10-28)

- **Progress:** `798 1203 66` raw, `759 759 100` portable; `ported 798
  (759 portable + 39 runtime) · host-owned 41 · deferred 12 · replaced 37 ·
  runtime 315` (`python3 tools/port_progress.py`).
- **Differential:** `1033/1033 functions VERIFIED; 14993/14993 mutants
  detected; 0 named gaps; 825/845 rows with callees closed (188 have none);
  809/845 closed on full rows; 8 ported functions with no row`. 20 open rows
  (19 addresses, all terminal with per-row reasons), 13 `WEAKER`, 8 `UNROWED`,
  the span census at 385 writer slots (384 reachable + dead `0x57E3C`) plus the
  out-of-table `0x5D110`, and the E2 triage's 388 span-writer rows with the
  supplement `0x5D004`. Nothing
  in this set is closable: every blocker is runtime (`>= 0x5D000`) or
  host-owned in stub mode, the three flush rows rest on `0x6C4CF` (`mov eax,1;
  jmp 0x62003`; its closure reaches only runtime call targets), and the AIL
  rows compare state the game's `mem[]` never holds (`ail.c:1-13`).
- **Captures:** 34/35 present, `gp-u14-win-live` the declared absence. The
  claim-coverage phase's Stage 2 is mid-flight: `tools/gp_draw.py`, the
  `gp-draw` scenario and `DRAW_EXTRA` are merged (`gp_session.py:497-509`), but
  no `gp-draw` capture, Makefile oracle or miss set exists yet; the
  two-human-KO and U16-state diagnostics remain `.failed` artefacts; claim
  coverage Task 12 is skipped (U14 not authorized) and Task 13 (the gate) is
  pending.
- **Mode census:** 35 of 52 modes exhibited; the never-presented content modes
  are `0A 0B 0E 21 25 32 33` (with `04 18 19 2F` and the null/empty entries
  `01 02 1C 1D 20 26` declared).
- **Audio:** 26 code-reachable banks; the register-stream oracle compares 29
  `.dro` traces (millisecond timestamps; the OPL core renders at 49,716 Hz);
  the PCM pin is determinism only (selmo peak
  2,790, sha `530a9430…`). The OPL core is vendored Opal 2.0.3
  (`port/src/platform/audio/opl/opal/opal.h`), the mixer is invented
  (`mixer.h:4-9`). The 43 `.dro` files on disk declare `hardware` dual-OPL2
  (34) or OPL2 (9); every stream uses the second register set (about half its
  writes are `0x100+`; 4,699 bank-1 AM/VIB-enable and 4,589 non-zero bank-1
  waveform writes), yet the only control-register write is `0xBD = 0xC0`, once
  per file, with no `0x101/0x104/0x108/0x1BD`. The stream is therefore equally
  consistent with one OPL3, and the bank-1 AM/VIB/waveform behaviour is a
  topology fork the oracle cannot see.
- **U16:** the dwell series and the divergence are in the fought-match record
  §F5 and were re-measured today against the U23 replay; the gate operand is
  `(s16)DSW(0xBD884 + 2*char)` vs `slot+0x30` at `0x48599` (`fighter.c:16289`).
- **5120:** reproduced on a fresh 4,953-frame dump: capture 5120 binds the pair
  `(4316, 4317)` at the `f=1825/f=1826` fade step; 5119 is clean at port 4315
  (`f=1824`), 5121 is a `(4317,4318)` splice; the 13 pixels are 8/1/4 against
  port 4317's indices `0x32`/`0x2E`/`0x4D` (`tools/gp_compare.py:243-275`).
- **U14:** the rule mismatch in §1.5 is measured from `gp-u14-win-live.failed`
  and `gp-u9-win`; the scenario is unchanged.

## 3. Goals

**G1 — A measured state census.** A `PR_COVERAGE` build records, for every
**portable** function address (the 759; the 39 runtime addresses are declared
runtime), whether a named driver entered it. The counter is a new macro,
**not** a `PR_SEAM` (a seam added to a function the differential harness stubs
changes what catches a mutant; AGENTS.md records that hazard). `make coverage`
runs the four non-scenario drivers, the `--check` run and every present gp/k11
script (`PR_GP_DUMP`/`PR_K11_DUMP` per scenario);
a missing capture skips its tag by name and is reported, so a checkout without
the git-ignored captures still runs the target (the baseline is measured on
the full set). `tools/coverage_census.py` prints the per-driver matrix, the
union, and the split `HIT / NOT-HIT / UNINSTRUMENTED`. The NOT-HIT set is
pinned in `tools/coverage_baseline.txt`, so a new never-hit function fails and
a new hit is an `improved:` re-pin with a recorded reason. **The claim is the
hit set, not the call count**: `fn_resolve` may note an entry a seam already
noted; the set is what the census reports. Every ported function carries a
counter or is a declared `UNINSTRUMENTED` exception; a guard test fails on a
new column-0 header without one.

**G2 — The out-of-scope modes declared, not captured.** `0x21` (the
volleyball easter egg, `0x4BD98` after the eighth worshipper trample) and
`0x25` (the both-characters-4 camera sequence, `0x4E11C`) both require
`DS_00104ABC >= 2`, which is `b1f == 3` only (`flow.c:1621`/`2405`); their
capture is a two-human scenario with eight tramples of a specific effects
entry and a mirror-ARMADON camera condition. The phase derives the exact gates
from the bytes, records why they are not in scope, and leaves them as declared
gaps with the census rows. `0x0E` (the continue/toll screen) is declared for a
different reason: it needs a lost match plus a credit, a scenario this phase
does not take.

**G3 — ENDURANCE's team advance and match end (modes `0x32`/`0x33`).** Capture
the four-KO roster walk (the loser's `DS_00108134 + s*4 + count` bytes), the
`DS_00104AF0`/`DS_00104AF1` counters reaching 4, the mode-`0x33` 600-frame wait
and the results screen; a `gp_endurance.py` milestone check and a
`gp-u25-oracle`. The two counters are not in `SNAP_FIELDS` today
(`WIN_EXTRA` has `w2`/`w3` at `0x104AF2/AF3`, not `0x104AF0/AF1`), so the task
appends them and takes a fresh capture.

**G4 — The side-1 tour and ending (`ad4=1`).** Capture the "RIGHT PLAYER
ARCADE" route (`gp-u8-right-arcade` already exhibits `b1f=2`, `w10d=1`, the
mode-`0x2E`→`0x10`→`0x11`→`0x05`→`0x06` chain) through seven poked wins to mode
`0x1F` with `ad4=1`, and prove the falsifiable claim that the ending content is
side-independent by a frame cross-check against the same character's P1 ending.
The side-1-only screens (mode `0x12`'s pset-`0x74` banner, the side-1
scoreboard/prompts, mode `0x1E` states 7/8) become measurable in the same run.
`b1f=2` keeps `DS_00104ABC` at 1, so the two-human-only modes above stay out.

**G5 — The timeout/health drawn round.** A poked round with both health bytes
equal and the clock top byte at zero (`0x280BA`), a `gp_draw.py --timeout`
mode that accepts `ad4=0xFFFFFFFF`, `clock=0`, `|A-B| <= 3`,
`b1e==adc==3` (and **not** the KO draw's `ad4=2`, `b16=2`). The arm calls
`flow_round_over` without `0x27C48`, so it does not write `DS_00104B16`;
`b16` is round 2's value and must not be pinned as a draw predicate. This
closes the second draw predicate, which no capture reaches: across 729
mode-7 records with both health bytes below `0x78`, the smallest measured
`|A-B|` is 30, against the arm's `<= 3`.

**G6 — The U16 reaction fix.** Append the fields the dump needs (`0x100AB4`,
`rec+0x1C`, the anchor index, `slot+0x57`, `rec+8`/`+0x34`/`+0x36`/`+0x44`),
take the discriminating measurement on the port and (guarded) on the original,
classify the defect — record motion vs the `0x100AB4` anchor term vs call order
— and fix the raw instruction that the measurement names. Acceptance: the
port's dwell matches the original's series `0x1501..0x1531` (the port is
`0x1180` **above** it today), `s0_54` clears at the capture's frame, `r0`
reaches `0x33`, the U23 moves ratchet advances one measured step, and the
`0x13FF/0x1400/0x1401` (+`+0x42` bit 3) boundary test with a mutation proof
passes. If the measurement contradicts the model, the task stops and records
it; no fitted amplitude.

**G7 — Two model questions decided, not fitted.** (a) Read the 13 aperture
bytes (rows 1..4, the measured x list) plus a control row at every S record in
a new diagnostic scenario; PASS-A (not video memory ⇒ capture-pipeline
artifact, band re-derived with that cause), PASS-B (aperture differs ⇒ name the
writer), FAIL (both contradicted ⇒ the band stays a named gap). (b) An OPL
single-voice fidelity test: write one note from the register bytes, render 1 s
at 49,716 Hz, measure the fundamental against
`fnum * 49716 / 2^(20-block)` within a tolerance derived from the window
length and the fnum quantization, with pitch mutations (swap channels, block+1)
that must fail. This claims *pitch*, not timbre; the dual-OPL2/OPL3 topology,
Opal's YMF262 accuracy and the invented mixer stay named gaps.

**G8 — The proof debt declared, not re-litigated.** One record task (no code)
declares the differential frontier terminal with the per-item reasons (20 open /
13 weaker / 8 unrowed; the span census's 385 writer slots (384 reachable + dead
`0x57E3C`) plus the out-of-table `0x5D110`, and the E2 triage's 388 span-writer
rows with the supplement `0x5D004`), answers the storage §S.10 items ((a) terminal
because the exit chain is save-free, (b) the static trace, (c) the two
"nothing else stores it" absolute-operand scans are already implicitly covered
by the rows' memory comparison (the explicit watch is optional and not taken),
(d) the statistics screens already enforced, (e) the 39 consumers audited), and
records U17 and
U14 as declared with the measured reason. After this task, a later phase must
bring new evidence to re-open any of them.

## 4. Non-goals

- **U14 R1.** Not run. The measured rule mismatch (§1.5) makes the current gate
  unsatisfiable on the match-1 path; the phase records the finding and the two
  re-scope options (drop the `b0c` clause and claim "fought KOs, no pokes", or
  move the scenario to the final) for a future, explicitly authorized phase.
- **The two-human KO publish.** Owned by claim-coverage Task 9; this phase
  consumes whatever that task publishes (it feeds the census baseline).
- **The two-human-only modes `0x21`/`0x25` and the continue screen `0x0E`.**
  The gates are `DS_00104ABC >= 2`, i.e. `b1f == 3`; `0x21` needs eight
  worshipper tramples and `0x25` a mirror-ARMADON camera sequence, and `0x0E`
  a lost match with credits. They are declared with their measured gates and
  census rows, not captured.
- **Language reload, the coin countdown beyond the already-captured debit, the
  U17 tick-keyed replay, host/product work (SDL hot-plug, CI, packaging,
  save-path docs).** Each is recorded with its evidence and left out; U17's
  trace ratchet would be empty across the frozen menu window and its screens
  are already covered by `k11-oracle`.
- **Death-stream intermediate state.** The rendered result of every death
  stream is already inside the ending frame ratchets (the mode-`0x0F` spans sit
  below every `MIN_FIRST`), so a `fx` hash would buy an intermediate-state
  claim at the cost of a new capture and a new trace field; it is named, not
  taken.
- **The 20 terminal rows, the 8 `UNROWED`, the 13 `WEAKER`, rendered
  audibility in the full sense, the 5120 band's cause if the experiment is
  inconclusive.** Declared, not fitted.
- **Porting new game code.** The one code change is the U16 offset the
  measurement names, inside an already ported function. A task that proves a
  different defect stops and records it.

## 5. Constraints

- Work from the repo root. `data/` is read-only; captures write only their own
  `data/k11-captures/<scenario>` directories; never touch `gp-u13-ending2`'s
  published artifacts (the 5120 diagnostic is a new scenario directory).
- **No fitted constants.** Every pin is measured and proved to fail one step in
  the tested direction; a value that cannot be pinned is a named gap with its
  evidence.
- **The differential rows stay byte-identical** unless a task's code fix
  changes a ported function; adding `PR_HIT` must not introduce a `PR_SEAM`
  (the harness's mutant-catch sets are sensitive to seams).
- **The coverage build must not change behaviour.** A `--check` run and a
  driver run must produce byte-identical frames/`fn_miss` with `PR_COVERAGE`
  off; with it on, no game state may change (the counter is host bookkeeping,
  never in `mem[]`).
- **Existing pins do not move** except each task's measured re-pins. A pin that
  moves outside its task stops the phase.
- A failed capture is preserved as `.failed.N`; no pins, no `CAPTURE_DIRS`
  entry, no oracle.
- The gate is `PR_DIFF_FULLSCAN=1 GATE_STRICT=1 PR_PINS_EXACT=1 make verify`
  plus `make audio-render`, twice at the phase's close.

## 6. Approaches considered

**A — Exhibit-first.** Take the content captures and pins before building the
instrument. Highest immediate claim count, but it repeats claim-coverage's
failure mode (a capture that reaches the game event can still fail publication
on a poller gap, and the full gate cannot judge it); and the unexhibited set
stays a hand-reconstructed list that the next handler invalidates. Rejected.

**B — Divergence-first.** Fix U16, decide 5120, test the OPL core; no new
captures. Bounded, real bug work, and it removes the only known trace
divergence; but the unexhibited content modes stay unclaimed and the phase
cannot say how many there are. Rejected as too narrow for the current state.

**C — Instrument first, then the measured exhibits and the one divergence
(recommended).** Stage 1 makes the unexhibited set a measured, pinned object
and installs the census the repo has been missing. Stage 2 uses it to take the
two content exhibits with the highest claim value (ENDURANCE end, side-1 tour)
plus the cheap timeout draw, declares the out-of-scope modes, then the U16
fix, then the two model experiments. The census names everything the phase does
not take; the phase does not promise to empty the NOT-HIT set. This mirrors the
shape that worked for claim coverage (offline instrument first) without
repeating its capture-first risk.

## 7. The design

### 7.1 Stage 1 — the instrument (offline)

1. **Baseline and the record.** Re-measure and paste: the counters above, the
   52-mode union and the never-presented list, the seamed/registered split
   (**measured: 631 of 759 portable addresses instrumented** — 610 seamed + 32
   `fn_register`, 11 in both — and the 128 unseamed split into **124 addresses
   carrying a `PR_HIT` line** plus the **4 `UNINSTRUMENTED` exceptions**
   `0x29D60`, `0x2EA30`, `0x2EA64`, `0x500BB`; record §D1.4/§D1.9), the current
   `--check` frames and
   `fn_miss` log, the U16 series against the U23 replay, the 5120 dump and the
   band's 13 offsets, the OPL facts and the U14 rule rows. Write
   `docs/superpowers/plans/2026-10-28-state-coverage-derivations.md` §D1.
2. **The counter.** `PR_HIT(addr)` in `mem.h`, compiled only under
   `PR_COVERAGE`; a 1,300-slot `{addr, hits}` table in `mem.c` with
   `fn_hit_arm/report/clear`; `fn_resolve` notes a hit only when it finds a
   function; the seam macros gain a `PR_HIT` line inside the same `#ifdef` so
   seamed functions are covered without a second edit. `--coverage FILE` in
   `main.c`/`run_tests.c` writes `fn-hit <tag> 0xADDR n=N`. A unit test proves
   arm/note/report/clear and that the report is inert when unarmed; a mutation
   of the note path fails it. Prove byte-identity: a `--check` run and one
   driver run with `PR_COVERAGE` off hold the same frames and `fn_miss` as the
   normal build.
3. **The census.** `tools/coverage_census.py` imports
   `port_progress.ported_set()`/`fn_addresses()`, scans `port/src` for the
   instrumented addresses (the same scan the guard uses), unions the per-tag
   dumps, prints the matrix and the `HIT / NOT-HIT / UNINSTRUMENTED` split, and
   `--check` compares against `tools/coverage_baseline.txt` (new NOT-HIT fails;
   new hit prints `improved:` and needs `--update-baseline --reason`). The
   split is decided by the scan, not by the dump: an instrumented address with
   no dump entry is `NOT-HIT`; an uninstrumented address is `UNINSTRUMENTED`
   only if it is a declared exception; otherwise the guard fails. A guard test
   fails on a new column-0 `/* 0xADDR` header in a `port/src` **`.c`** file
   that has neither a `PR_HIT` nor a declared `UNINSTRUMENTED` entry. `make
   coverage` is a new target outside `verify` (it runs the drivers again;
   measure and record its run time).

### 7.2 Stage 2 — the exhibits and the declaration (capture truth)

4. **The out-of-scope modes declared (`0x21`, `0x25`, `0x0E`).** Derive from
   the bytes the exact gates (`0x4BD98`: the eighth worshipper trample
   `+0x1F > 7`, `DS_00104ABC >= 2`, `DS_00104AD8 <= 0`,
   `fight_13134` false; `0x4E11C`: `0x104ABC >= 2`, both characters 4, the
   camera within `±0x3300`; `0x27A2C`: the continue poll and the credit) and
   record why each is out of scope: `DS_00104ABC = (b1f == 3) + 1`
   (`flow.c:1621`/`2405`) makes `0x21`/`0x25` two-human-only, and `0x21`/`0x25`
   need an eight-trample and a mirror-ARMADON camera sequence respectively. Add
   the census rows and the declared-gap text; no capture.
5. **ENDURANCE advance/end (`0x32`/`0x33`).** Clone `gp-u12-endurance`, keep its
   pins, and add four poked KOs of one side (the U10 `KO_P2`/`DEATH_DONE`
   recipe keyed `after_entry 0x31 k`). **Measured** (`gp-u25-endurance-end`,
   1,360 frames, `gp-u25-oracle` green; record §D5): the four-KO roster walk is
   confirmed (the loser's roster byte `c1` walks `04 → 06 → 01 → 03` as
   predicted), the counter that reaches 4 is **`af1` (`0x104AF1`)** — not
   `af0`, which is the other side's byte — and the stop is mode **`0x17`**
   (not the plan's predicted `0x14`, ledger R6), before the mode-`0x33`
   600-frame wait and the results screen. Append `af0`/`af1`
   (`0x104AF0`/`0x104AF1`) to `SNAP_FIELDS` — they are
   **not** `w2`/`w3` (`0x104AF2`/`0x104AF3`) — and
   take a fresh capture. A `gp_endurance.py check` reads mode/`c1`/`af0`/`af1`/
   `b25`/`w10d`; a `gp-u25-oracle` pins the ratchets. The 0x33 screen and the
   0x31 re-entry are the new pixel claims.
6. **Side-1 tour/ending (`ad4=1`).** Build the "RIGHT PLAYER ARCADE" route
   (`u8` row 1 + `p2.right`/`p2.start`) with seven poked wins for side 1
   (`KO_P1`, `lands_marked(1,c)`, the seven `after_entry 0x0C`/`0x0D`), stop at
   mode `0x1F`/`0x1E`; `gp_win.py`/`tools/gp_side1.py` carry the side parameter
   (`c1`, `s0_5a`, `w2=0`/`w3=2`, `ad4=1`, `t104>>8`, `0xC0|c`), and the check
   asserts the side-1-only rows (mode `0x12`'s pset-`0x74` banner, mode `0x1E`
   states 7/8). **Measured** (`gp-u26-side1-ending`, ARMADON, `gp-u26-oracle`
   green; record §D5): `b1f=2`, `ad4=1`, `c1=4`, and the falsifiable
   cross-check passes — **268 mode-`0x1F` frames byte-identical to `gp-u21`**
   (ARMADON's P1 ending), proving the ending content is side-independent (the
   `0x208F8` differential row's claim); the cross-check is
   `tools/gp_side1.py` with a mutation proof. A difference is a finding, not a
   tolerance. New miss set, oracle and pins.
7. **Timeout/health draw.** Poke `0x10780A = 0x10789E = 0x30` and the clock
   top byte `0x1088F2 = 0` in round 3 after two poked KOs (1-1); stop on
   `0x0B`; `gp_draw.py` gains a `--timeout` mode requiring `ad4=0xFFFFFFFF`,
   `clock=0`, `|A-B| <= 3`, `b1e==adc==3`, and modes `0x0A` and `0x0B`.
   **Measured** (`gp-draw-timeout`, `gp-draw-timeout-oracle` green; record
   §D5/§D7.6): the `clock = 0` predicate is read from the **`W` record's clock
   poke** — the round-3 arm re-reads `+0x5A` after `0x39FF4`, so the health
   values are bound to the clock-poke frame and asserted `< 0x78`, not the
   earliest KO poke. Do **not** pin `b16`: `0x280BA` calls `flow_round_over`
   without `0x27C48`, so `DS_00104B16` keeps round 2's value. A new scenario,
   oracle and pins; the KO draw (`ad4=2`) is asserted *not* to match.
8. **Name-entry letters (taken; 6's side-1 run does not exhibit states 7/8).**
   Clone the U10 script and inject letter keys at mode `0x1E` so the entry
   screen is driven, not just displayed, and extend the pixel window over the
   exit frames the current pin (`5437`) excludes. No port change. **Measured**
   (`gp-u27-name-entry`, record §D5): the exhibit drives entry sub-states `b25
   00 → 04 → 05 → 06 → 07 → 0A`, three letters and the finaliser committing
   "ABD". It is an **exhibit, not a gated oracle**: its replay has a pinned miss
   set (`k_miss_gp_u27_name_entry`), but there is no `gp-u27-oracle` and it is
   **not in `make verify`**.

### 7.3 Stage 3 — the known divergence (code truth)

9. **The discriminating dump.** Append the fields (§3 G6) to the port trace
   (always) and to a guarded original run's S records. The original's
   `slot+0x30` series already exists in `gp-u24-u16-state.failed`'s `S` records
   at ~30 frames from `0x14F0` to `0x153C` (the 0x1504/0x151B/0x151C/0x151F/
   0x1532 gaps are the poller's), so the new original arm only needs the
   *components* (`0x100AB4`, `rec+0x1C`, the anchor index) at a few frames; the
   recovery proved the KO reproduces (75/75 steps, mode 9 at `0x1486`). If the
   poller misses the components, use the DOSBox-X breakpoint at
   `0x18652`/`0x48599` with a frame condition and a hit cap. Classify: record
   motion (`rec+0x1C` differs), the anchor term (`0x100AB4` differs), or call
   order (both agree and `0x186D0` reads a different `+0x1C`).
10. **The fix.** Only the class the measurement names: for motion, the raw
    instruction that stores/applies the offset at `0x14FE` (the stream bytes
    at the dumped cursor are the source), with the port's dwell series
    matching the original's at `0x1501..0x1531`; for the anchor term, the
    pset/sprite write; for call order, `0x186D0`'s order. Add the
    `0x13FF/0x1400/0x1401` (+`+0x42` bit 3) boundary test with a mutation
    proof, re-pin `GP_U23_MOVES_MIN_FIRST` and the trace field the fix moves,
    and prove every other pin unchanged. No fitted offset; if the class is not
    one of the three, the task stops and records. **Measured** (record §D3):
    the class is **record motion** (`ab4` agrees at every frame, `rec+0x1C`
    differs by `0x1180`), and the cause is a **null table base, not a
    mistranscribed instruction** — the reaction stream's opcode `0x21` reads
    `mem[1]`/`mem[3]` (the IVT at linear 0) with no base loaded. The fix is
    the host stand-in `bios_ivt_stand_in()` (`flow.c`, called by `game_init`)
    seeding `mem[1] = 0xCA`/`mem[3] = 0xF0` (the inverted dwell `-3456`/`-4480`
    = `y * 64`); the x bytes `mem[0]`/`mem[2]` are the adjacent record-motion
    defect, recorded not fixed. Only `GP_U23_TRACE_MIN_FIRST`
    `5449 → 6050` and `GP_U23_MOVES_MIN_FIRST` `5467 → 6050` re-pinned; every
    other pin unchanged.

### 7.4 Stage 4 — the two model questions

11. **The 5120 aperture read.** A new scenario `gp-u13-band` (the U13 script
    plus a diagnostic read of the 13 offsets and one control row at every S
    record; the published U13 directory and its sha are untouched). Read the S
    record with `f=0x1826`; apply PASS-A/PASS-B/FAIL (§3 G7) and write the
    result into the record. PASS-B names the writer; PASS-A re-derives the band
    with the pipeline cause; FAIL keeps it a named gap. No pin moves.
    **Measured** (record §D4): **INCONCLUSIVE**, a fourth outcome outside the
    design's three — the DOSBox-X memory file maps the guest's main RAM only,
    not the VGA aperture (`vgamem`), so every one of the 14 bytes (and all
    140,280 across the capture) reads `0xFF` and the read is not a measurement
    of the aperture. The band stays a named gap for that reason; its bounds
    `(5120, 1, 5, 56b900b1…)` and every pin are unchanged.
12. **The OPL single-voice test.** In `test_audio.c`, derive `f` from the
    register bytes (`fnum * 49716 / 2^(20-block)`), write one note
    (`0x20/0x40/0x60/0x80/0xA0/0xB0/0xC0`), render 1 s at 49,716 Hz and measure
    the fundamental over the window (rising zero crossings, error ≤
    `49716/N + fnum quantization`). Pitch mutations (channel swap, block+1)
    must fail; a carrier-TL mutation must move the RMS by its derived factor.
    The claim is pitch fidelity of the core, not audibility; the topology fork
    and Opal accuracy are named. **Measured** (record §D6): target A4 with
    `fnum=580`, `block=4` → `f = 439.9915 Hz`, tolerance `1.7586 Hz`; the core
    renders **439 rising crossings = 439.0000 Hz** (`|0.9915| ≤ 1.7586`); the
    channel-swap and block+1 mutations fail (873 Hz and 879 Hz), and the
    carrier-TL amplitude sanity reads mean-square ratio **0.2501** against the
    derived 0.25.

### 7.5 Stage 5 — the declaration and the gate

13. **The proof debt.** One record task: differential terminal (20/13/8; the
    span census's 385 writer slots — 384 reachable + dead `0x57E3C` — plus the
    out-of-table `0x5D110`, and the E2 triage's 388 span-writer rows with the
    supplement `0x5D004`, each with the measured reason), storage §S.10 (a)
    terminal and (b) static trace recorded, (d)/(e) documentation, U17
    declared, the B2–B8 labels replaced by the census's measured NOT-HIT set,
    and U14's rule mismatch recorded with the two re-scope options. **Measured**
    (record §D7): 20 open rows over 19 addresses (the `TERMINAL_CENSUS`
    terminals), 13 `WEAKER`, 8 `UNROWED`; the B2–B8 labels are replaced by the
    census's measured NOT-HIT set (**102** addresses, §D7.5/§D8.3; Task 3's
    original pin was 110) plus the declared modes. The
    coverage baseline's provenance is a **separate pin**
    (`tools/coverage_baseline.txt`), **not** part of `tools/pin_baseline.sha256`
    (the capture-pin tripwire; record §D7.5). No code.
14. **The gate and the docs.** Two full gates and `audio-render`; every new pin
    proved at ±1; `port_progress.py`, the diff counter, the gate inventory, the
    capture manifest, the pin baseline and `symbols.h` checked; the design
    status, `docs/PROGRESS.md`, README, AGENTS and the affected specs updated
    with the measured lines.

## 8. What the phase will claim, and what it will not

**Will claim:** every portable function is either hit by a named driver or
declared `UNINSTRUMENTED`/`NOT-HIT` against a pinned baseline; ENDURANCE's
advance/end and the side-1 tour's content are reproduced for their captured
windows; the timeout draw is distinct from the KO draw and both `0x0A`/`0x0B`
predicates are exhibited; the U16 reaction's record-motion gap is fixed by the
IVT stand-in (the raw instruction the measurement names); the 5120 band's cause
is **INCONCLUSIVE** (the memory file excludes the VGA aperture — the fourth
outcome, and the band stays a named gap); the OPL core renders a single voice
at the derived pitch within a derived tolerance; the differential and storage
frontiers are terminal with per-item reasons; the two-human-only modes
(`0x21`, `0x25`) and the continue screen (`0x0E`) are declared with their
measured gates.

**Will not claim:** U14's fought win; the two-human KO (claim-coverage owns
it); the `0x21` volleyball, the `0x25` camera sequence or the `0x0E` continue
screen's content; the full pixel content of every unexhibited mode outside the
captured windows; death-stream intermediate state; the full audibility chain;
the 5120 band's cause if the experiment is inconclusive; any row, classification
or universe the proof-completion phase declared.

## 9. Risks and stop rules

- **The census can lie by omission.** A function with no counter is not
  "never hit"; it is `UNINSTRUMENTED`. Never report the two together. The guard
  test makes the omission explicit.
- **A `PR_HIT` that becomes a seam changes mutant detection.** The counter must
  be a separate macro; a task that adds `PR_SEAM` for coverage is reverted.
- **The coverage build may perturb output.** Stop if a `--check` run or a
  driver's frames/`fn_miss` differ from the normal build.
- **Captures fail under host load.** The claim-coverage Task 9 guard and
  preflight apply; a `.failed.N` is an acceptable outcome and moves no pin.
- **The U16 fix may not be the record.** Stop at the classification; do not fit
  an offset or a dwell. If the original's `0x100AB4` differs while the record
  matches, the anchor term is the fix; if both match, re-derive `0x186D0`'s
  call order. If the original's `y(0x1532)` is above `0x1400`, the gate model is
  the suspect and the task records the contradiction.
- **The two-human-only gates cannot be poked open.** `DS_00104ABC` is
  `(b1f == 3) + 1`; a 1P or `b1f=2` scenario cannot reach `0x21`/`0x25` however
  much damage is poked. Do not build one; declare the modes with their gates.
- **The 5120 experiment may not reproduce the frame.** A non-recurrence leaves
  the gap declared; do not widen the band or lower a pin.
- **The OPL test may show the core is wrong.** That is a finding: record the
  measured pitch error and either fix the core or declare the limit; do not
  loosen the tolerance to pass.
- **A new exhibit may expose a port divergence.** The frame ratchet is the
  claim; a divergence is recorded as the first unexplained frame (the repo's
  normal outcome), never smoothed by a wider allowance without a byte-pinned
  recipe and a mutation proof.

## 10. What is left to the builder

- The exact hit-counter layout and the `--coverage` dump's tag names; the
  per-driver list and which present scripts are excluded (U17 by name).
- The ENDURANCE scenario's `time_limit`/`memsize`/stop mode and the exact
  roster bytes (predicted, not fitted: the measurement is the task's), plus the
  `af0`/`af1` field append.
- The side-1 check's field list and whether mode `0x1E` state 8 is reachable
  through `ad4=1` alone or needs `B14`/`ABC` too.
- The timeout draw's poke frame (the arm re-reads `+0x5A` after `0x39FF4`, so
  the same-frame hit can break equality) and the `gp_draw.py` mode's fields;
  the clock field's address (`0x1088F2`, the top byte of `0x1088EF`).
- The U16 append's exact addresses, the second-pointer support `read_snap`
  needs for a slot record, and the breakpoint script's condition/hit cap.
- The 5120 diagnostic's 14 offsets (the 13 plus one control) and the S-record
  field format; the PASS/FAIL thresholds are the two named outcomes, not a
  tolerance.
- The OPL measurement window/tolerance derivation and the mutation set.
- The declaration record's per-item reason table; the U14 re-scope wording.

## 11. Measured outcome (Task 14)

§7's predicted figures are superseded by these measurements (record §D1–§D8);
the gate (`PR_DIFF_FULLSCAN=1 GATE_STRICT=1 PR_PINS_EXACT=1 make verify` plus
`make audio-render`, each run twice) is green.

- **Mode census:** **35 of 52** table modes exhibited; the never-presented set
  is `01 02 04 0A 0B 0E 18 19 1C 1D 20 21 25 26 2F 32 33` (the content-bearing
  ones are `0A 0B 0E 21 25 32 33`, declared in §2/§D2) — as predicted.
- **Seam/registration split:** **631 of 759** portable addresses instrumented
  (610 seamed + 32 `fn_register`, 11 in both); the 128 unseamed split into
  **124 addresses carrying a `PR_HIT` line** (125 `PR_HIT` lines — `0x50188`
  carries two) plus the **4 `UNINSTRUMENTED`** exceptions `0x29D60`, `0x2EA30`,
  `0x2EA64`, `0x500BB` (record §D1.4/§D1.9). The census reads **HIT 653 /
  NOT-HIT 102 / UNINSTRUMENTED 4** (sums to 759; Task 14 re-pinned it from the
  Task 3 baseline's `HIT 645 / NOT-HIT 110` — R17/§D8.3 — the eight `improved:`
  addresses measured as `gp-u25-endurance-end`'s `0x292D4`/`0x29638`/`0x296B8`
  and `gp-draw-timeout`'s `0x19820`/`0x19958`/`0x27ED8`/`0x28BD4`/`0x28C38`).
- **U16 (record §D3):** the class is **record motion**, not the anchor term or
  call order; the cause is the reaction's null table base (the IVT at linear
  0), fixed by `bios_ivt_stand_in()` seeding `mem[1] = 0xCA`/`mem[3] = 0xF0`.
  Only `GP_U23_TRACE_MIN_FIRST` `5449 → 6050` and `GP_U23_MOVES_MIN_FIRST`
  `5467 → 6050` re-pinned.
- **ENDURANCE (record §D5):** the roster counter is **`af1`** (`0x104AF1`)
  walking `0 → 4`; the stop is mode **`0x17`**; the capture
  `gp-u25-endurance-end` is **1,360 frames** and `gp-u25-oracle` is green.
- **Side-1 ending (record §D5):** `gp-u26-side1-ending` (ARMADON) shows
  `b1f=2`, `ad4=1`, `c1=4`, and **268 mode-`0x1F` frames are byte-identical to
  `gp-u21`** (ARMADON's P1 ending) — the side-independence cross-check passes.
- **Timeout draw (record §D5):** `gp-draw-timeout` reads the `clock = 0`
  predicate from the **`W` record's clock poke** (the round-3 arm re-reads
  `+0x5A` after `0x39FF4`, so the health values are bound to that frame and
  asserted `< 0x78`); `b16` is not asserted.
- **Name-entry (record §D5):** `gp-u27-name-entry` is an **exhibit, not a gated
  oracle** — it drives entry sub-states `b25 00 → 04 → 05 → 06 → 07 → 0A`,
  three letters and the finaliser committing "ABD", with a pinned replay miss
  set (`k_miss_gp_u27_name_entry`) but **no `gp-u27-oracle` and no `verify`
  entry**.
- **5120 (record §D4):** **INCONCLUSIVE** — the DOSBox-X memory file excludes
  the VGA aperture, so the 14 diagnostic bytes read `0xFF` and the band stays a
  declared gap.
- **OPL (record §D6):** the core renders 439.9915 Hz (measured 439.0000 Hz)
  within the 1.7586 Hz derived tolerance; the pitch mutations and the TL
  amplitude sanity pass as derived.
- **Proof debt (record §D7):** `make diff-verify` reads `1033/1033 functions
  VERIFIED; 14993/14993 mutants detected; 0 named gaps; 825/845 rows with
  callees closed (188 have none); 809/845 closed on full rows; 8 ported
  functions with no row`; **20 open rows over 19 addresses** (terminal),
  **13 `WEAKER`**, **8 `UNROWED`**.
- **Gate inventory:** **37/38 capture paths present**, `gp-u14-win-live` the
  one declared absence (the phase added `gp-u25-endurance-end`,
  `gp-u26-side1-ending` and `gp-draw-timeout` to `CAPTURE_DIRS`; the design's
  §2 "34/35" is the pre-phase reading).
