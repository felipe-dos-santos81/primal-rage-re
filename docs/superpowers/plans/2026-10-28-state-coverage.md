# State coverage — plan

**Goal:** Make the port's remaining evidence gap measurable, then close the
content-bearing part of it: a per-driver function-hit census with a pinned
baseline; exhibits for ENDURANCE's advance/end (`0x32`/`0x33`) and the side-1
tour/ending (`ad4=1`); the timeout/health drawn round; the declared
two-human-only modes (`0x21`, `0x25`) and the continue screen (`0x0E`); the U16
reaction divergence fixed from the raw; and two model questions (the 5120 band
and the OPL core's pitch) decided by experiments that can come back negative.

**Architecture:** Stage 1 is offline and builds the instrument (`PR_COVERAGE`
counter, census tool, baseline). Stage 2 takes the two content exhibits and the
cheap timeout draw, each with its evidence check and oracle, and declares the
modes whose gates need `b1f == 3`. Stage 3 is the one code change: the U16
reaction, first measured, then fixed. Stage 4 runs the two cause experiments.
Stage 5 declares the debt that stays and runs the gate. Every stage is
independently green: a failed capture or an inconclusive experiment stops its
task, not the phase.

**Tech Stack:** C (port), Python 3 stdlib (tools), CMake/Make, DOSBox-X
captures, the existing `gp_*` harness and `gp_compare.py` ratchets.

**Spec:** `docs/superpowers/specs/2026-10-28-state-coverage-design.md`

**Precondition:** the claim-coverage phase is closed (`docs/superpowers/plans/2026-10-27-claim-coverage.md`
Task 13). This plan consumes whatever it published: `gp-draw` if its capture
passed, the two-human-KO and U16-state outcomes as recorded, and the gate's
derived capture inventory. If a claim-coverage task is still open, do not start
Stage 1's baseline; record the open task instead.

---

## Stage 1 — the instrument

### Task 1: Baseline and the derivation record

**Files:**
- Create: `docs/superpowers/plans/2026-10-28-state-coverage-derivations.md`
  (§D1).
- Read: `docs/superpowers/specs/2026-10-28-state-coverage-design.md`,
  `docs/superpowers/plans/2026-10-27-claim-coverage-derivations.md` §C1,
  `port/src/mem.c`, `port/src/mem.h`, `port/src/main.c`,
  `port/tests/run_tests.c`, `port/tests/test_platform.c`,
  `tools/port_progress.py`, `tools/gp_compare.py`.

**Interfaces:**
- Consumes: the closed claim-coverage tip.
- Produces: the measured numbers every later task cites.

- [ ] **Step 1: Record the counters.** Paste `python3 tools/port_progress.py`
  (`798 1203 66`, `759 759 100`, the class breakdown), `make diff-verify`'s
  counter, `make gate-check`'s `present/missing` line and `make entry-triage`'s
  status.
- [ ] **Step 2: Measure the mode census.** Scan the 39 `poll.log`s (or however
  many the closed tip has) for the `P` mode transitions; record the union, the
  never-presented list and the per-mode record counts. The design's measured
  reading is 35 presented of 52 with the never set `01 02 04 0A 0B 0E 18 19 1C
  1D 20 21 25 26 2F 32 33`; re-measure, do not copy.
- [ ] **Step 3: Measure the seam/registration split.** Count the portable
  addresses (759) that open with `PR_SEAM`/`PR_SEAM0`/`PR_SEAM_RET`/`PR_SEAM_RET0`
  or are reached through `fn_register`/`fn_resolve`, versus those with only a
  column-0 header. The design's recon reads 631/759 instrumented and 128 not
  (the majority in `game/`); re-measure and list the 128 addresses.
- [ ] **Step 4: Record the three open measurements.** (a) the U16 series
  (`PR_GP_DUMP` trace vs the `.failed` poll.log): the frames `0x14F0..0x14FD`
  agree exactly, the original drops at `0x14FE`, and the original is exactly
  `0x1180` **below** the port at every sampled frame `0x1501..0x1531` (port
  `0x0D40` vs original `0xFFFFFBC0` at `0x1501`; port `0x1B40` vs `0x09C0` at
  `0x152C..0x1531`), with the port's exit at `0x155C` and the original's at
  `0x1533`; (b) the 5120 dump (`make gp-replay scenario=gp-u13-ending2
  GP_SCRIPT_ARGS=--end-to-last-s`) with the 13 offsets, the `(4316,4317)`
  bind, 5119 clean at 4315 and 5121 the `(4317,4318)` splice; (c) the OPL
  facts (Opal 2.0.3, 49,716 Hz, the 1:1 music path, the dual-OPL2/OPL3 write
  evidence).
- [ ] **Step 5: Record the U14 rule rows.** From `gp-u14-win-live.failed` and
  `gp-u9-win`: `b0c = 0` in every S record; the two fights' damage rows; the
  only non-zero `b0c` writes on the ending path. State the rule mismatch and
  the two re-scope options (§D5).
- [ ] **Step 6: Write §D1 and commit (when authorized).**
  `docs: measure the state coverage baseline`.

**Acceptance:** §D1 holds every number with its command and file:line; the
mode list and the seam split are this task's own measurement; no value is
copied from the design without a re-measurement.

---

### Task 2: The `PR_COVERAGE` hit counter

**Files:**
- Modify: `port/src/mem.h` (the macro), `port/src/mem.c` (the table and the
  funnel notes), `port/src/main.c` and `port/tests/run_tests.c` (the dump),
  `port/CMakeLists.txt` (the option), `port/tests/test_platform.c` (the test),
  `port/tests/test.h` (registration), and the 128 `port/src` `.c` files with no
  seam (one `PR_HIT` line each; Task 1's list).
- Read: `port/src/mem.h:89-125` (the seam macros), `port/src/mem.c:15-95`
  (`fn_register`/`fn_resolve`/`fn_resolve_from`), `port/src/main.c:42-48`,
  `port/tests/run_tests.c:39-46`.

**Interfaces:**
- Consumes: nothing.
- Produces: `fn_hit_arm(int)`, `fn_hit_report(const char *tag)`,
  `fn_hit_clear(void)`; `PR_HIT(0xADDR)`; the `PR_COVERAGE` CMake option; the
  `PR_HIT_TAG` env var.

- [ ] **Step 1: Write the failing test.** In `test_platform.c`, arm, note one
  address, report to a temp file and assert the line
  `fn-hit <tag> 0xADDR n=1`; clear and assert the report is empty; note the
  same address twice and assert `n=2`; assert an unarmed report writes nothing.
  Prove it fails with the note path mutated (drop the increment).
- [ ] **Step 2: Implement the counter.** `#ifdef PR_COVERAGE` in `mem.h`: a
  `PR_HIT(addr)` that increments a 1,300-slot `{u32 addr; u32 hits;}` table
  (the same headroom as `FN_TABLE_MAX`); a no-op otherwise. Place the macro
  **inside** the four seam macros but **outside** the `if (pr_seam)` guard so
  it fires in a normal (seam-less) coverage build, and at the top of the 128
  unseamed functions (one line each). In `fn_resolve`, note a hit only when the
  function is found; leave `fn_resolve_from`'s miss path untouched.
  **PORT:** the table is host bookkeeping, never in `mem[]`; the macro must not
  become a seam.
- [ ] **Step 3: The dump.** `PR_HIT_TAG` names the file; `main.c` and
  `run_tests.c` write `coverage/<tag>.hits` beside the miss log when
  `PR_COVERAGE` is on and the env var is set. No output when unset.
- [ ] **Step 4: Prove byte-identity.** Build without `PR_COVERAGE`; run
  `./build/prageport --game-dir data/game/C --check 820` (the verify frame
  count, so the run is ~200 MB, not `8000`'s ~2 GB) and one driver; hash the
  frames and `frames/fn_miss.txt`; repeat with the option on and the dump
  unset; the hashes must be equal. Paste both into the record.
- [ ] **Step 5: Commit (when authorized).**
  `port: add the coverage hit counter`.

**Acceptance:** the unit test fails with the note path mutated and passes
otherwise; `PR_COVERAGE` off is byte-identical; with it on, no game state
changes; no new `PR_SEAM` exists.

---

### Task 3: The census tool, the baseline and the guard

**Files:**
- Create: `tools/coverage_census.py`, `tools/coverage_baseline.txt`,
  `tools/tests/test_coverage_census.py`.
- Modify: `Makefile` (`coverage` target, the unittest list), `.gitignore`
  (`coverage/`), `tools/port_progress.py` only if a shared helper is missing.
- Read: `tools/port_progress.py` (`ported_set`, `fn_addresses`),
  `tools/diff_verify.py`'s `NonPortable.ported` (the strict rule).

**Interfaces:**
- Consumes: Task 2's dumps.
- Produces: `python3 tools/coverage_census.py [--check|--update-baseline]`;
  `make coverage`.

- [ ] **Step 1: Write the failing tool test.** A synthetic hits directory
  with two tags and three addresses must print the per-driver matrix and the
  `HIT / NOT-HIT / UNINSTRUMENTED` split. The split is decided by the
  instrumentation scan, not by the dump: an **instrumented** address with no
  dump entry is `NOT-HIT`; an address with no counter and a declared exception
  is `UNINSTRUMENTED`; an address with neither fails the guard. A missing
  capture's tag is reported and excluded from the comparison, not counted as a
  NOT-HIT. `--check` with
  a new NOT-HIT exits 1; a new hit prints `improved:` and exits 2 unless
  `--update-baseline --reason` is given.
- [ ] **Step 2: Implement the tool.** Union the per-tag dumps; classify each
  portable address; print a stable sorted report; write the baseline as one
  `addr class` line per address (or a sha over the sorted set — the builder
  picks; the baseline must be human-diffable).
- [ ] **Step 3: Add `make coverage`.** Run the four non-scenario drivers
  (`PR_ATTRACT_DUMP`, `PR_TITLE_DUMP`, `PR_FRONTEND_DUMP`, `PR_RESTART`), the
  `--check` run, every present `gp-*` script (`PR_GP_DUMP`) and the three k11
  scenarios (`PR_K11_DUMP`) with
  `PR_HIT_TAG` set; exclude `gp-u17-device2` by name (no replay). A missing
  capture skips its tag by name and is reported, so the target also runs on a
  checkout without the git-ignored captures; the baseline is measured on the
  full set. Outside `verify`; measure and record its run time.
- [ ] **Step 4: The guard.** `--check-instrumented` parses the **`.c`** files
  under `port/src` for column-0 `/* 0xADDR` headers (the `.h` files also carry
  declaration comments that are not function bodies, so they are excluded) and
  fails on a `.c` one without a `PR_HIT` in its first lines or a declared
  exception. Register the test in the Makefile's unittest list.
- [ ] **Step 5: Measure and pin.** Run `make coverage` twice; the hit sets
  must be identical; pin the baseline with a `--reason` that names the run.
- [ ] **Step 6: Commit (when authorized).**
  `tools: add the coverage census`.

**Acceptance:** the census runs twice with identical output; a new NOT-HIT
fails and a new hit is an `improved:` re-pin; every portable address is
`HIT`, `NOT-HIT` or a declared `UNINSTRUMENTED`, and the guard fails on a new
uninstrumented header.

---

## Stage 2 — the exhibits and the declaration

### Task 4: The out-of-scope modes declared (`0x21`, `0x25`, `0x0E`)

**Files:**
- Modify: `docs/superpowers/plans/2026-10-28-state-coverage-derivations.md`
  (§D2), `docs/PROGRESS.md`, `AGENTS.md`, `port/RE_GUIDE.md`.
- Read: `port/src/game/fight.c:2917-2960` (`0x4BD98`, `fight_13134`,
  `fight_4b470`'s eighth-trample arm at `:3076-3085`),
  `port/src/game/fight.c:5554-5600` (`0x4E11C`),
  `port/src/game/flow.c:1621`/`2405` (`DS_00104ABC = (b1f == 3) + 1`),
  `port/src/game/flow.c:1967-2000` (`game_mode_0e_step`, the continue poll),
  `port/src/game/flow.c:5864-5900` (`game_mode_25_step`),
  the design §1.1, §3 G2.

**Interfaces:**
- Consumes: the census's NOT-HIT rows for these handlers.
- Produces: a measured declaration with the gates and the reason; no capture.

- [ ] **Step 1: Derive the gates from the bytes.** `0x21`: the caller's
  `+0x1F > 7` (the eighth worshipper trample), the surrounding gates
  (`DS_00104B1D` not 2/3, `DS_00104AFC == 0`, `DS_001088C1 == 0`,
  `(DS_001088EF >> 24) > 1`, `DS_001088C5 == 0`, `DS_00108864 == 0`),
  `DS_00104AD8 <= 0`, `DS_00104ABC >= 2`, `fight_13134` false, the two actor
  spawns and the `0x1E` timer. `0x25`: `0x104ABC >= 2`, both characters 4,
  `DS_001078FA == 2`, `DS_00108892 >= 6`, slot 0's `+0x53 == 0`, the camera
  within `±0x3300`, `DS_00104AD8 <= 0`. `0x0E`: the continue poll, the
  `DS_00108110` countdown (starts `0x0A`, decremented every 64 master ticks or
  on a pressed button) and the credit (`config_credit_ready`). Record every
  address.
- [ ] **Step 2: Record why they are out of scope.** `DS_00104ABC` is
  `(b1f == 3) + 1`, so `0x21`/`0x25` need a two-human match; `0x21` also needs
  eight tramples of a specific effects entry and `0x25` a mirror-ARMADON bowling
  trigger; `0x0E` needs a lost match plus a credit. A poked 1P scenario cannot
  open either gate — do not build one.
- [ ] **Step 3: Declare in the docs.** Add the three modes to the named-gap
  list with the measured gates and the census rows; note that `0x0B` (the
  drawn winner pose) is covered by claim-coverage Task 11's `gp-draw` if that
  capture published.
- [ ] **Step 4: Commit (when authorized).**
  `docs: declare the out-of-scope modes`.

**Acceptance:** the declaration holds the gate addresses and the
`DS_00104ABC` derivation; no capture, pin or code change; the census's NOT-HIT
set loses no address (the modes stay declared, not hit).

---

### Task 5: ENDURANCE's advance and match end (modes `0x32`/`0x33`)

**Files:**
- Create: `tools/gp_endurance.py`, `tools/tests/test_gp_endurance.py`.
- Modify: `tools/gp_session.py` (`SNAP_FIELDS` plus `af0`/`af1` at
  `0x104AF0`/`0x104AF1`, `SCENARIOS['gp-u25-endurance-end']`),
  `tools/tests/test_gp_session.py` (the field append parses and round-trips),
  `Makefile` (`gp-u25-oracle`, `GP_U25_*`, `CAPTURE_DIRS` plus one),
  `port/tests/test_platform.c` (the miss set), `tools/audio_compare.py` as the
  capture requires, `tools/capture_manifest.json`.
- Read: `port/src/game/flow.c:2124-2210` (`0x296B8`: the loser's roster byte
  `DS_00108134 + s*4 + count`, `DS_00104AF0[s]`, mode `0x33`/`0x258`),
  `port/src/game/flow.c:2651-2690` (`0x29638`: the 600-frame wait),
  `docs/superpowers/plans/2026-10-20-gameplay-truth-ii-derivations.md` §J4,
  `tools/gp_session.py`'s `gp-u12-endurance`.

**Interfaces:**
- Consumes: U12's rosters (`c0 = [0,2,3,1]`, `c1 = [4,6,1,3]`) and its pins.
- Produces: a published `gp-u25-endurance-end`, the milestone check and the
  oracle.

- [ ] **Step 1: Write the check's test.** A synthetic log with `c1`
  `04→06→01→03`, `af0 = 4` (the side's byte; `af1` is the other side's),
  mode `0x33` and `b25 = 0` passes; one with `af0 = 3` or the mode `0x31`
  re-entry missing fails. The fields are the **bytes** `0x104AF0`/`0x104AF1`
  (`af0`/`af1`), **not** `w2`/`w3` (`0x104AF2`/`0x104AF3`).
- [ ] **Step 2: Build the scenario.** Append `af0`/`af1` to `SNAP_FIELDS`
  (a capture-time field: the next capture records it; older logs keep parsing).
  Clone `gp-u12-endurance`; keep its steps and pins; add four poked KOs of one
  side (`KO_P2`/`DEATH_DONE` keyed `after_entry 0x31 k`), then
  `until_mode 0x14` (or the measured successor). The loser's roster byte is
  **predicted** to walk `c1 04→06→01→03` from `ENDURANCE_PICKS`' teams
  (`[0,2,3,1]`/`[4,6,1,3]`); the capture measures it. `time_limit` from the
  predicted ~2,700-frame end, a harness value.
- [ ] **Step 3: Capture and check.** One run; accept only on the session
  checks, the S-record rows (mode, `c1`, `af0`, `af1`, `b25`, `w10d`) and
  `check-input`. A failure is preserved; no pins.
- [ ] **Step 4: Replay and pin.** `gp-replay`, the miss set, `gp_compare`'s
  ratchets, each proved at ±1. Add the oracle to `verify`; update the manifest
  and the pin baseline.
- [ ] **Step 5: Commit (when authorized).**
  `gameplay: capture the endurance team end`.

**Acceptance:** the capture walks the loser's roster to `af0 = 4`, enters mode
`0x33` and reaches the results screen; every milestone row is measured; no
U12 pin moves.

---

### Task 6: The side-1 tour and ending (`ad4=1`)

**Files:**
- Create: `tools/gp_side1.py` (the evidence check) or extend `gp_win.py` with
  a `side` parameter, `tools/tests/test_gp_win.py` additions.
- Modify: `tools/gp_session.py` (`SCENARIOS['gp-u26-side1-ending']`),
  `tools/gp_win.py` (`_ending_tail(side)`, `_MATCH1` side rows),
  `Makefile` (`gp-u26-oracle`, `GP_U26_*`, `CAPTURE_DIRS` plus one),
  `port/tests/test_platform.c` (the miss set), `tools/capture_manifest.json`,
  `tools/audio_compare.py` as the capture requires.
- Read: `port/src/game/flow.c:5505-5520` (`0x208F8`'s character load),
  `port/src/game/flow.c:3050` (`DS_001077B0 + ad4*0x94`), the mode-`0x2E`
  row (`flow.c:7499-7507`), `tools/gp_session.py`'s `gp-u8-right-arcade`,
  `docs/superpowers/plans/2026-10-23-endgame-truth-derivations.md` §E1-§E7.

**Interfaces:**
- Consumes: `gp-u8-right-arcade`'s measured chain (`b1f=2`, `w10d=1`,
  `c0=3`, `c1=4`) and U10's seven-poke recipe.
- Produces: a published `gp-u26-side1-ending`, the side-aware check and the
  oracle.

- [ ] **Step 1: Write the check's test.** A synthetic log with `b1f=2`,
  `ad4=1`, `c1=char`, `w2=0`, `w3=2`, the `0xC0|c` land marks and the
  mode-`0x1F` row passes; the same log with `ad4=0` fails. Prove the existing
  P1 checks still pass on `gp-u18-talon-ending`.
- [ ] **Step 2: Build the scenario.** The `u8` row-1 menu prefix
  (`p1.down`), `p2.right`/`p2.start` at mode `0x10`, then U10's body with
  `KO_P1` and `lands_marked(1,c)`, the seven `after_entry 0x0C`/`0x0D` rows,
  `memsize=64`, and `time_limit=240` (U18's harness value; its capture took
  ~205 s).
- [ ] **Step 3: Capture and check.** One run; accept on the session checks,
  `gp_win.py check --side 1`'s milestones (including the mode-`0x1E` states
  7/8 if reachable) and `check-input`. A failure is preserved; no pins.
  `b1f=2` keeps `DS_00104ABC` at 1, so `0x21`/`0x25` are out of this route by
  construction.
- [ ] **Step 4: The falsifiable cross-check.** Compare the mode-`0x1F` frames
  with the same character's P1 ending (`gp-u10`/`gp-u13`/`gp-u18`…`gp-u22`); the
  expected result
  is byte-identical content (the handler is side-independent by the `0x208F8`
  differential row). A difference is the task's finding; do not allow it by
  name without a byte-pinned recipe.
- [ ] **Step 5: Replay and pin.** `gp-replay`, the miss set, `gp_compare`'s
  ratchets, each proved at ±1. Add the oracle to `verify`; update the manifest
  and the pin baseline.
- [ ] **Step 6: Commit (when authorized).**
  `gameplay: capture the side-1 ending`.

**Acceptance:** the capture shows `b1f=2`, `ad4=1`, `c1=char`, the side-1-only
screens and the mode-`0x1F` content; the cross-check is recorded with its
outcome; no P1 ending pin moves.

---

### Task 7: The timeout/health drawn round

**Files:**
- Modify: `tools/gp_draw.py` (`--timeout`, the `clock` field),
  `tools/tests/test_gp_draw.py`, `tools/gp_session.py`
  (`SNAP_FIELDS` plus `clock` at `0x1088F2`; `SCENARIOS['gp-draw-timeout']`),
  `tools/tests/test_gp_session.py` (the field parses),
  `Makefile` (`gp-draw-timeout-oracle`, `GP_DRAW_TIMEOUT_*`, `CAPTURE_DIRS`
  plus one), `port/tests/test_platform.c` (the miss set),
  `tools/capture_manifest.json`.
- Read: `port/src/game/flow.c:3092-3121` (`0x27FA8`'s timeout arm at
  `0x280BA`), the clock at `0x1088F2` (the top byte of `0x1088EF`),
  `docs/superpowers/specs/2026-10-28-state-coverage-design.md` §1.1, §3 G5.

**Interfaces:**
- Consumes: `gp-draw`'s scenario helper and the `DRAW_EXTRA` fields.
- Produces: a published `gp-draw-timeout`, the `--timeout` check and the
  oracle.

- [ ] **Step 1: Write the check's test.** A synthetic log with `ad4 =
  0xFFFFFFFF`, `clock = 0`, `b1e = adc = 3`, `|s0_5a - s1_5a| <= 3`,
  modes `0x0A` then `0x0B` passes; the `gp-draw` KO log fails it, and the
  timeout log fails `gp_draw.py`'s default check (the two predicates are
  disjoint). **Do not** assert `b16`: `0x280BA` calls `flow_round_over`
  without `0x27C48`, so `DS_00104B16` keeps round 2's value.
- [ ] **Step 2: Build the scenario.** Two poked KOs (1-1) then a third round
  with `0x10780A = 0x10789E = 0x30` and `0x1088F2 = 0` in one `poke`;
  `stop_mode=0x0B`, `stop_delay=60`. The check must read the S record (the arm
  re-reads `+0x5A` after `0x39FF4`, so a same-frame hit can break equality).
- [ ] **Step 3: Capture and check.** One run; accept only if the S records
  show the timeout row before mode `0x0A` and the session checks pass. A
  failure is preserved; no pins.
- [ ] **Step 4: Replay and pin.** `gp-replay`, the miss set, `gp_compare`'s
  ratchets, each proved at ±1. Add the oracle to `verify`; update the manifest
  and the pin baseline.
- [ ] **Step 5: Commit (when authorized).**
  `gameplay: capture the timeout draw`.

**Acceptance:** the two draw predicates are disjoint and both exhibited; the
timeout capture's rows are the measured ones; no `gp-draw` pin moves.

---

### Task 8: Name-entry letters (optional)

**Files:**
- Modify: `tools/gp_session.py` (a letter-keyed variant of the U10 script),
  `Makefile` (`GP_ENDING_*` pins only if the same capture is used),
  `tools/gp_win.py` if a milestone reads the entry state.

**Interfaces:**
- Consumes: Task 6's side-1 run if it already exhibits states 7/8.
- Produces: either the letter-driven entry rows and a longer pixel window, or
  a recorded drop.

- [ ] **Step 1: Decide from Task 6.** If Task 6's capture already drives the
  entry screen, drop this task and cite its rows. Otherwise clone U10 and
  inject letter keys at mode `0x1E` (the BIOS words through `key` steps).
- [ ] **Step 2: Capture and check.** One run; accept on the session checks
  and the S records (the entry states, the letter cells, the finaliser).
- [ ] **Step 3: Pin or declare.** Extend the existing oracle's pixel window
  only if the new frames are explained; otherwise record the frames and leave
  the pin where it is. No port change.

**Acceptance:** either the entry interaction is exhibited and pinned, or the
task is dropped with Task 6's rows as the reason.

---

## Stage 3 — the known divergence

### Task 9: The U16 discriminating dump

**Files:**
- Modify: `tools/gp_session.py` (`SNAP_FIELDS` plus `ab4`, `rec_y`, `anchor`,
  `s0_57`, `rec8`, `rec34`, `rec36`, `rec44`), and `read_snap`/`format_s` so a
  field can point through a **slot record** (`0x1077B0`/`0x107844`), not only
  the fixed DS offsets and the key-config block pointer at `[DS_00101514]`;
  `tools/tests/test_gp_session.py` (the pointer field and the new
  `SNAP_FIELDS` round-trip);
  `port/src/game/fighter.c` if a read-only dump hook is needed (a
  `PR_GP_DUMP`-gated print); `Makefile` if a new driver env var is needed.
- Read: `docs/superpowers/plans/2026-10-26-fought-match-truth-derivations.md`
  §F5, `port/src/game/fighter.c:16280-16300` (`0x4844C`), `port/src/game/fighter.c:252-274`
  (`0x186D0`), `port/src/game/fighter.c:221-247` (the anchor table).

**Interfaces:**
- Consumes: the U23 replay and the `.failed` U16 poll.log (its S records
  already hold the original's `s0_30` across `0x14F0..0x153C`; the
  `0x1504/0x151B/0x151C/0x151F/0x1532` gaps are the poller's).
- Produces: the measured class (record offset / anchor term / call order) and
  the exact addresses the fix will touch.

- [ ] **Step 1: Port arm.** Append the fields to the trace and run the U23
  replay; record `ab4`, `rec_y`, `s0_30` per frame at `0x1501..0x1531`. The
  measured port series is `0x0D40` at `0x1501` rising to `0x1B40` at
  `0x152C..0x1531`; the original's is `0xFFFFFBC0` to `0x09C0` — the port is
  `0x1180` **above** it, and the pre-`0x14FE` frames agree exactly.
- [ ] **Step 2: Original arm.** The existing `.failed` S records give the
  original's `s0_30`; the new fields only need the *components* at a few
  frames. Rerun `gp-u24-u16-state` with the claim-coverage Task 9 guard, or use
  the DOSBox-X breakpoint at `0x18652`/`0x48599` with a frame condition and a
  hit cap. Reject the run if the components at `0x1501` and `0x152C..0x1531`
  are missing.
- [ ] **Step 3: Classify.** `rec_y` differs with `ab4` equal ⇒ the record
  offset; `ab4` differs ⇒ the anchor term; both agree ⇒ call order. Write §D3
  with the two series and the class.

**Acceptance:** the class is measured on both sides at both boundary frames;
no port behaviour changes; no pin moves.

---

### Task 10: The U16 fix

**Files:**
- Modify: `port/src/game/fighter.c` (the instruction the class names),
  `port/tests/test_fight.c` (the boundary test),
  `port/tests/test.h` (registration if new), `Makefile` (`GP_U23_*` re-pins),
  `docs/superpowers/plans/2026-10-28-state-coverage-derivations.md` (§D3).
- Read: Task 9's §D3, the raw stream bytes at the dumped cursor,
  `docs/superpowers/plans/2026-10-26-fought-match-truth-derivations.md` §F5.

**Interfaces:**
- Consumes: Task 9's class.
- Produces: the corrected offset and the re-pinned U23 ratchet.

- [ ] **Step 1: Write the failing test.** Seed the `0x13FF/0x1400/0x1401`
  (+`+0x42` bit 3) boundary and the dwell; assert the port's dwell matches the
  original's series. Prove the test fails before the fix.
- [ ] **Step 2: Implement the raw instruction.** For the record offset: the
  instruction that applies it at `0x14FE` (the stream bytes are the source),
  not a fitted value. For the anchor term, the pset/sprite write; for call
  order, `0x186D0`'s order. Add a `/* PORT: */` note only if a host stand-in is
  unavoidable.
- [ ] **Step 3: Prove.** The boundary test passes and fails under the mutant;
  `s0_54` clears at the capture's frame; `r0` reaches `0x33`; the U23 moves
  ratchet advances one measured step; every other pin is unchanged.
- [ ] **Step 4: Commit (when authorized).**
  `fighter: apply the measured reaction offset`.

**Acceptance:** the dwell series matches the original's; the boundary test and
its mutation prove it; only `GP_U23_MOVES_MIN_FIRST` (and the trace field the
fix moves) is re-pinned, each at ±1.

---

## Stage 4 — the model questions

### Task 11: The 5120 aperture read

**Files:**
- Modify: `tools/gp_session.py` (`SCENARIOS['gp-u13-band']`, the diagnostic
  read), `tools/gp_capture.py` if the read needs a new field, `tools/gp_compare.py`
  only if the analysis is a tool, `tools/tests/test_gp_compare.py` for the
  analysis test.
- Create: the analysis script/section that applies PASS-A/PASS-B/FAIL.
- Read: `tools/gp_compare.py:275` (the band pins), `:551`/`:568`
  (`band_pair`/`band_explain`), the record §E9, the design §1.4.

**Interfaces:**
- Consumes: U13's script and the poller's memory-file read.
- Produces: the 14 guest bytes per S record and a decided cause.

- [ ] **Step 1: Build the scenario.** Clone U13's script; add the read of the
  13 offsets (`0xA0000 + 320*row + x`, rows 1..4, the measured x list: row 1
  `23, 24, 295, 296`; row 2 `22, 297`; row 3 `21, 74, 298`; row 4
  `27, 28, 291, 292`) plus one control (`row 1, x=100`) to every S record; a
  new directory, `memsize=64`, the same time limit.
- [ ] **Step 2: Capture.** One run; the published U13 directory is untouched.
  The band's frame is the S record at `f=0x1826` (the fade step the pair
  `(4316,4317)` binds); if the capture's frame count shifts, key on `f`, not
  the frame index.
- [ ] **Step 3: Analyse the `f=0x1826` record.** PASS-A/PASS-B/FAIL per the
  design; write the result and the 14 values into §D4. No pin moves; the
  band's bounds stay as they are. The measured context to cite: 5119 is clean
  at port 4315 (`f=1824`), 5121 is a `(4317,4318)` splice, the 13 pixels are
  8/1/4 against port 4317's indices `0x32`/`0x2E`/`0x4D`.
- [ ] **Step 4: Commit (when authorized).**
  `gameplay: decide the 5120 band's cause`.

**Acceptance:** one of the three outcomes is recorded with the 14 measured
bytes; a non-recurrence leaves the gap declared; no band or pin is widened.

---

### Task 12: The OPL single-voice pitch test

**Files:**
- Modify: `port/tests/test_audio.c` (a new `test_opl` case or an extension of
  it), `port/tests/test.h` only if a new case is registered.
- Read: `port/src/platform/audio/opl/opl.c`, `opal/opal.h`, the record
  §M1-§M6, the design §2's audio bullet.

**Interfaces:**
- Consumes: `opl_reset/opl_write/opl_render`.
- Produces: a measured fundamental and a mutation-proved tolerance.

- [ ] **Step 1: Derive the pitch.** From `fnum`/`block` in the register bytes,
  `f = fnum * 49716 / 2^(20-block)`; choose `fnum`/`block` that land on a
  target note; state the fnum quantization error.
- [ ] **Step 2: Measure.** Render 1 s; count rising zero crossings; compare
  with `f` within `49716/N + quantization` (derived from the window, not
  fitted). Prove the test fails under a channel swap and under `block+1`.
- [ ] **Step 3: The amplitude sanity.** A carrier-TL mutation must move the
  RMS by its derived factor; record the factor's derivation.
- [ ] **Step 4: Record the limits.** The claim is pitch, not timbre; the
  dual-OPL2/OPL3 topology fork and Opal's unmeasured YMF262 accuracy are named
  gaps in §D6.
- [ ] **Step 5: Commit (when authorized).**
  `audio: test the OPL core's rendered pitch`.

**Acceptance:** the test passes on the core and fails under both pitch
mutations; the tolerance is derived and printed; the named limits are
recorded.

---

## Stage 5 — the declaration and the gate

### Task 13: The proof-debt declaration

**Files:**
- Modify: `docs/superpowers/plans/2026-10-28-state-coverage-derivations.md`
  (§D7), `docs/PROGRESS.md`, `AGENTS.md`, `port/RE_GUIDE.md`,
  `docs/superpowers/plans/2026-10-24-proof-completion-derivations.md` only if
  a citation is wrong (read-only otherwise).
- Read: `tools/diff_verify.py` (`TERMINAL_CENSUS`, `WEAKER`, `UNROWED`),
  `tools/tests/test_diff_verify.py`, `tools/port_classification.txt`, the
  storage record §S.8/§S.10, the U17 record §H6.9.

**Interfaces:**
- Consumes: Tasks 1-12's measurements.
- Produces: one declaration with per-item reasons; no code.

- [ ] **Step 1: The differential table.** 20 open / 13 `WEAKER` / 8 `UNROWED`
  / the span census's 385 writer slots (384 reachable + dead `0x57E3C`) plus
  the out-of-table `0x5D110`, and the E2 triage's 388 span-writer rows with the
  supplement `0x5D004` (`0x81110[127]`; proof-completion §P4), each with the
  measured blocker and the reason it is terminal. Note the one-off
  `sprite_blit_aperture` pool mismatch (three reruns green, not reproduced).
- [ ] **Step 2: The storage table.** (a) terminal: the exit chain is save-free
  (`0x2EA68` = `mov eax,1; jmp 0x62003`; `0x6C4CF`'s closure reaches only
  runtime call targets, so no boot loads slot 13..35); (b) the static trace;
  (c) the two "nothing else stores it" absolute-operand scans are already
  implicitly covered by the rows' memory comparison (the explicit watch is
  optional and not taken); (d) the statistics screens are already enforced;
  (e) the 39 consumers are bounded and audited.
- [ ] **Step 3: U17 and U14.** U17: the wall-paced walk has no tick-keyed
  replay and its screens are already covered; U14: the rule mismatch and the
  two re-scope options. Both declared, neither run.
- [ ] **Step 4: Replace the B2-B8 labels.** The labels in
  `docs/superpowers/specs/2026-10-27-claim-coverage-design.md` are undefined
  in the repo; cite the census's measured NOT-HIT set and the declared modes
  (`0x21`, `0x25`, `0x0E`) instead, and record the census baseline's
  provenance in `tools/pin_baseline.sha256` if it is a pin.
- [ ] **Step 5: Commit (when authorized).**
  `docs: declare the proof debt terminal`.

**Acceptance:** every declared item has a measured reason and an address; no
row, class or universe is widened; the docs agree with `make diff-verify`'s
printed line.

---

### Task 14: The gate and the docs

**Files:**
- Modify: `docs/superpowers/specs/2026-10-28-state-coverage-design.md`
  (status), `docs/superpowers/plans/2026-10-28-state-coverage-derivations.md`
  (§D8), `docs/PROGRESS.md`, `README.md`, `AGENTS.md`, `port/RE_GUIDE.md`,
  `port/spec/*.md` as the tasks require.

**Interfaces:**
- Consumes: every task's result.
- Produces: the phase's closing record.

- [ ] **Step 1: Run the gate twice.** `PR_DIFF_FULLSCAN=1 GATE_STRICT=1
  PR_PINS_EXACT=1 make verify` and `make audio-render`, each twice; paste the
  key lines.
- [ ] **Step 2: Verify the counters.** `port_progress.py`, `make diff-verify`,
  `make entry-triage`, `symbols.h` idempotence, the gate inventory, the
  capture manifest, the coverage baseline and the pin baseline. No
  `improved:`/`re-pin` line without a recorded measurement.
- [ ] **Step 3: Correct the design.** Mark it **Implemented and gated** (or
  **Partially implemented** with the named failures) and replace every
  predicted number with the measured one.
- [ ] **Step 4: Update the status docs.** `docs/PROGRESS.md` gets a paragraph
  per task; README/AGENTS/RE_GUIDE say the same measured things. List the
  named gaps that remain: the declared two-human-only modes (`0x21`, `0x25`)
  and the continue screen (`0x0E`), the `UNINSTRUMENTED` addresses, U14's rule
  mismatch, the two-human KO if claim-coverage left it open, the 5120 band's
  cause if the experiment was inconclusive, rendered audibility, the
  proof-debt table, and the host/product items.
- [ ] **Step 5: Commit (when authorized).**
  `docs: close the state coverage phase`.

**Acceptance:** two green gates under load; `audio-render` twice with the same
peak/sha; every new pin proved at ±1; no unrelated pin moved; the README's
title percentage and the class breakdown are the measured ones.

---

## Verification checklist (the phase's claims)

- [ ] `make coverage` runs twice with identical hit sets; a new NOT-HIT fails;
  a new hit is an `improved:` re-pin; every portable address is `HIT`,
  `NOT-HIT` or a declared `UNINSTRUMENTED`.
- [ ] The `PR_COVERAGE` build's frames and `fn_miss` are byte-identical to the
  normal build; no `PR_SEAM` was added for coverage.
- [ ] The two-human-only modes (`0x21`, `0x25`) and the continue screen
  (`0x0E`) are declared with their measured gates (`DS_00104ABC >= 2` is
  `b1f == 3`); no 1P scenario was attempted for them.
- [ ] ENDURANCE's roster walk reaches `af0 = 4`, mode `0x33` and the results
  screen; U12's pins are unchanged.
- [ ] The side-1 tour shows `b1f=2`, `ad4=1`, `c1=char` and the mode-`0x1F`
  content; the P1-ending cross-check is recorded.
- [ ] The timeout draw's rows (`ad4=0xFFFFFFFF`, `clock=0`, `b1e=adc=3`,
  `|A-B|<=3`, modes `0x0A`/`0x0B`) are exhibited and disjoint from the KO draw;
  `b16` is not asserted.
- [ ] The U16 dwell matches the original's series `0x1501..0x1531` (the port
  was `0x1180` above it); `s0_54` clears at the capture's frame; the boundary
  test and its mutation pass; only the U23 pins moved.
- [ ] The 5120 band's cause is PASS-A, PASS-B or FAIL, with the 14 measured
  bytes.
- [ ] The OPL pitch test passes and fails under both pitch mutations; the
  tolerance is derived; the named limits are recorded.
- [ ] The proof-debt declaration has a measured reason per item; no row,
  class or universe was widened.
- [ ] The full gate is green twice; `audio-render` is byte-identical twice; no
  unrelated pin moved.
