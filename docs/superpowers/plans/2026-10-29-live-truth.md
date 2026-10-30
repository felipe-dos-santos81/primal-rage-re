# Live Truth — implementation plan

> **For agentic workers:** this plan is executed task-by-task under
> subagent-driven development. Each task has a report in the git-ignored ledger
> and is reviewed before the next one merges. Do not batch tasks.

**Goal:** close the last reachable live/unexhibited content claims — the
KO-branch drawn match, the two-human KO, the continue screen and the `0x25`
easter egg — promote the name-entry exhibit to a gated oracle, fold the
already-measured U16 R2 gate value into the record, and record U14's
fought-KO evidence without claiming the fought win.

**Architecture:** no new portable function and no port-code change. The work is
captures, oracles, miss sets, pin baselines, one measurement record and the
docs that make the claims honest. Captures run serially on the DOSBox-X host;
offline tasks run in parallel worktrees and are cherry-picked.

**Tech stack:** C port + Python oracle tooling; DOSBox-X 2026.08.31 for
captures; CMake/`make verify`; `tools/gp_*.py`, `tools/capture_manifest.py`,
`tools/coverage_census.py`.

**Baseline:** `main` = `0b458148` (`Merge branch 'state-coverage'`). Portable
`759/759`; differential `1033/1033 functions VERIFIED; 14993/14993 mutants
detected; 0 named gaps; 825/845 rows with callees closed (188 have none);
809/845 closed on full rows; 8 ported functions with no row`; captures
`37/38 present, 1 missing` (the declared `gp-u14-win-live`); coverage census
`HIT 653 / NOT-HIT 102 / UNINSTRUMENTED 4`.

**Design:** `docs/superpowers/specs/2026-10-29-live-truth-design.md`.

**Record:** create `docs/superpowers/plans/2026-10-29-live-truth-derivations.md`;
the authoritative raw-byte record for every task. `docs/PROGRESS.md` gets one
paragraph per landed task.

**Ledger:** `.superpowers/sdd/2026-10-29-live-truth/progress.md` plus
`task-<n>-report.md` (git-ignored; the recovery map).

**Branch:** create `live-truth` from `main` when execution is authorized;
offline tasks use `lt-<n>` worktrees and are cherry-picked. Never commit on
`main`.

---

## Global constraints

- **Evidence discipline.** No fitted constant; every pin is measured from the
  capture and proved to fail one step in the claimed direction. A value that
  cannot be pinned is a named gap with its evidence.
- **Raw wins.** The state-coverage Task 4 prose called `0x25` a camera; the raw
  entry draws string `0x61` and the family's score strings (the recon decoded
  it as the bowling banner — the task re-reads the decoder before naming it).
  The `0x0E` trigger needs the KO on `DS_0010810D`'s side. The U16 gate is
  strict (`y < 0x1400`, not `<=`). Record each correction in the record.
- **No port code.** If a capture proves a port divergence, record the first
  unexplained frame and stop; do not patch or widen an allowance.
- **No pin moves without a reason.** Every existing pin, capture claim and
  counter stays unless a task measures why it moved.
- **Captures are serial.** One DOSBox-X run at a time; the host load must be
  ≤ the core count (`capture_manifest.py` preflight). A failed run is
  `.failed.N`; it does not publish and does not move a pin.
- **Oracle window.** Add an oracle only when the port's replay reaches the
  claimed frames; otherwise publish as game evidence with the gap named.
- **Tests.** Only `CHECK`/`CHECK_EQ_INT` in `port/tests`; a new assertion is
  mutation-proved. Python tests use the existing `unittest` style.
- **Commits.** Stage named files; `<area>: <what changed>`. Commit only when
  the task is green and the operator has authorized execution.

---

### Task 1: Baseline and the derivation record

**Files:**
- Create: `docs/superpowers/plans/2026-10-29-live-truth-derivations.md`
- Create: `.superpowers/sdd/2026-10-29-live-truth/progress.md` (git-ignored)
- Read: `AGENTS.md`, `docs/PROGRESS.md` tail,
  `docs/superpowers/specs/2026-10-29-live-truth-design.md`

**Interfaces:**
- Consumes: `main` at `0b458148`, the state-coverage ledger in
  `.superpowers/sdd/2026-10-28-state-coverage/`.
- Produces: the record's §L1 baseline and the phase ledger; no code, capture or
  pin.

- [ ] **Step 1: Measure the baseline.** Run and paste the exact lines:
  `python3 tools/port_progress.py`, `make gate-check`,
  `python3 tools/capture_manifest.py --check-pins`,
  `python3 tools/coverage_census.py --check-instrumented`, `git status
  --short --branch`, and the capture inventory `ls data/k11-captures`.
  (The differential counter is re-measured in Task 12; cite the baseline line
  above.)
- [ ] **Step 2: Write §L1.** Record the counters, the capture inventory, the
  four recon corrections (U16 measured; `0x25` bowling not camera; `0x0E` side
  rule; U14 `b0c` unsatisfiable), and the phase's no-port-code rule. State
  that the design is proposed and nothing has been built.
- [ ] **Step 3: Create the ledger.** Copy the task list and the rulings R1–R5
  below into `.superpowers/sdd/2026-10-29-live-truth/progress.md`.
- [ ] **Step 4: Commit (when authorized).** `docs: plan the live truth phase`.

**Rulings:**
- **R1:** `gp-draw` and `gp-draw-timeout` are distinct claims; the KO draw is
  the one that exhibits the KO-branch `0x0B`.
- **R2:** the two-human KO is the game's own damage path; no KO pokes; a
  failed capture publishes nothing.
- **R3:** U16 R2 is a record task; the t9-r5 set frame is the measurement.
- **R4:** `0x21` stays declared; no poked 1P exhibit without an operator
  reversal of state-coverage design §9.
- **R5:** U14's fought win stays open; the `.failed` artifact is game evidence
  only and adds no oracle/pin.

**Acceptance:** §L1 exists with measured lines; the ledger exists; no repo file
outside the record/ledger changed.

---

### Task 2: `gp-draw` — the KO-branch drawn match (`0x0A`/`0x0B`)

**Files:**
- Modify: `tools/gp_session.py` (only if the measured stop/step needs it; the
  `gp-draw` scenario exists)
- Modify: `Makefile` (`CAPTURE_DIRS`, `.PHONY`, the `GP_DRAW_*` /
  `GP_DRAW_MAX_MISSING` pin block, `gp-draw-oracle`, the `verify` dependency)
- Modify: `port/tests/test_platform.c` (`k_miss_gp_draw`, the `k_gp_sets` row)
- Modify: `tools/capture_manifest.json` (regenerate), `tools/pin_baseline.sha256`
  (re-pin with a measured reason)
- Modify: `tools/audio_compare.py` + its test if the capture's `.dro` is added
  to the inventory (otherwise name it out)
- Modify: `docs/superpowers/plans/2026-10-29-live-truth-derivations.md`
- Read: `tools/gp_session.py:572-602` (`SCENARIOS['gp-draw']`, `DRAW_EXTRA`,
  `KO_P2`/`KO_P1`/`KO_BOTH`), `tools/gp_draw.py:62-98`,
  `Makefile:1304-1334` (the `gp-draw-timeout` model), `flow.c` `0x27BA4` /
  `0x27ED8` / `0x28BD4` / `0x28C38`.

**Interfaces:**
- Consumes: the existing scenario and checker; `gp-draw-timeout`'s oracle
  shape.
- Produces: a published `gp-draw` and `gp-draw-oracle`, or a preserved
  `.failed.N` with no pins.

- [ ] **Step 1: Capture.** Run `make gp-capture scenario=gp-draw` on an idle
  host. Accept only if all of the following hold:
  `python3 tools/gp_draw.py check --capture data/k11-captures/gp-draw` exits 0;
  the poll.log has a row with `ad4=2`, `b1e==adc`, `w2==w3`, then mode `0x0A`
  and later `0x0B`; `gp_capture.py`'s session checks all pass (B, steps, X,
  mode-0x27 after Enter, kb==raw, frames, script v2, no unscripted input,
  pokes, 0 skipped BIOS words, stopped at the end). A failure is
  `data/k11-captures/gp-draw.failed.N`; stop.
- [ ] **Step 2: Replay and measure.** `make gp-replay scenario=gp-draw`;
  `python3 tools/gp_compare.py --scenario gp-draw --capture
  data/k11-captures/gp-draw --port /tmp/pr_gp_dump/gp-draw --report`. Record
  the first unexplained frame, the trace/moves first difference, the window
  start, the reach, and the max-missing. Confirm the port dump reaches both
  `0x0A` and `0x0B`; if not, see the window rule.
- [ ] **Step 3: Miss set.** Run the replay with the miss log; add
  `k_miss_gp_draw` (the base pair `0x29D60`/`0x5D812` plus any measured
  additions) and the `k_gp_sets` row. **Use prefix 0** for the scenario name:
  `gp-draw` is a prefix of `gp-draw-timeout`, and a prefix-1 row would trip
  the one-match CHECK.
- [ ] **Step 4: Wire the oracle.** Add `GP_DRAW_MIN_FIRST`,
  `GP_DRAW_TRACE_MIN_FIRST`, `GP_DRAW_MOVES_MIN_FIRST`, `GP_DRAW_MAX_START`,
  `GP_DRAW_MIN_REACH`, `GP_DRAW_MAX_MISSING`, `GP_DRAW_CAPTURE_SHA256` and
  `GP_DRAW_CAPTURE_FRAMES` in a `PIN-BASELINE` block; clone
  `gp-draw-timeout-oracle` as `gp-draw-oracle` (the `check`, the replay, the
  compare); add it to `CAPTURE_DIRS`, `.PHONY` and the `verify` list after
  `gp-draw-timeout-oracle`. The oracle's `N`/`MIN_REACH` must cover the
  `0x0A`/`0x0B` frames.
- [ ] **Step 5: Audio decision.** If `prage_000.dro` exists, either add it to
  `tools/audio_compare.py`'s inventory with a measured `TRACE_PINS` minimum
  and the expected-file list, or name it out of the inventory in the record.
  Do not invent a section pin.
- [ ] **Step 6: Prove and record.** Move each pin one step in the failing
  direction (raise `MIN_FIRST`/`TRACE`/`MOVES`, lower `MAX_START`/`MIN_REACH`,
  raise `MAX_MISSING`, change the sha/frames), run `make gp-draw-oracle`,
  confirm the failure, restore. Record the measured values and the
  failing-direction proof. Retire the "`0x0B` unexhibited" sentence.
- [ ] **Step 7: Commit (when authorized).** `gameplay: capture the KO drawn
  match`.

**Acceptance:** the oracle is green; every pin is ±1-proved; both modes are in
the compared window; `make verify`'s capture count rises by one (the declared
U14 gap stays); no existing pin moved; no port code.

---

### Task 3: `gp-twop-ko` — the two-human KO

**Files:**
- Modify: `tools/gp_session.py` (only if the measured stop/step needs it; the
  `gp-twop-ko` scenario exists)
- Modify: `Makefile` (`CAPTURE_DIRS`, `.PHONY`, `GP_TWOP_KO_*`,
  `gp-twop-ko-oracle`, `verify`)
- Modify: `port/tests/test_platform.c` (`k_miss_gp_twop_ko`, `k_gp_sets`)
- Modify: `tools/capture_manifest.json`, `tools/pin_baseline.sha256`
- Modify: `tools/audio_compare.py` + test if the `.dro` joins the inventory
- Modify: the record
- Read: `tools/gp_session.py`'s `SCENARIOS['gp-twop-ko']`,
  `tools/gp_twop.py:260-318` (`check --ko`), `tools/gp_capture.py:89-140`
  (the guard), `data/k11-captures/gp-twop-ko.failed/session.txt`,
  `docs/superpowers/plans/2026-10-26-fought-match-truth-derivations.md`
  §F4.3–§F4.7.

**Interfaces:**
- Consumes: the already-merged pin guard/preflight; the surviving failed
  artifact's measured KO (`f=0B91`, `s1_5a=78`, `w3=00`, `ad4=FFFFFFFF`,
  `b0c=00`).
- Produces: a published `gp-twop-ko` + oracle, or a `.failed.N` and no pins.

- [ ] **Step 1: Preflight and one guarded run.** Confirm the host load is ≤
  cores and the capture root is writable. Run `make gp-capture
  scenario=gp-twop-ko` once. Accept only if all session checks pass, including
  **`check=ok port script v2`** (the failed run's only failure was `key 16
  (p1.b2) consumption at f=596 unpinned`), `check-input` is clean, and
  `python3 tools/gp_twop.py check --ko --capture
  data/k11-captures/gp-twop-ko` exits 0. A failure is `.failed.N`; stop.
- [ ] **Step 2: Replay and measure.** `make gp-replay scenario=gp-twop-ko`;
  confirm the port dump reaches the capture's KO frame and the result screen;
  measure the first unexplained, trace/moves, start, reach, max-missing. If
  the replay stops before the KO, publish game evidence only (no oracle, no
  pins) and record why.
- [ ] **Step 3: Miss set and oracle.** Add `k_miss_gp_twop_ko` (the base pair
  plus the measured additions) and the `k_gp_sets` row; add the eight
  `GP_TWOP_KO_*` pins (`MIN_FIRST`, `TRACE`, `MOVES`, `MAX_START`,
  `MIN_REACH`, `MAX_MISSING`, `SHA256`, `FRAMES`); clone `gp-twop-oracle` as
  `gp-twop-ko-oracle`, with `check --ko` before the replay; add it to
  `CAPTURE_DIRS`, `.PHONY` and `verify`. **The oracle's `N`/`MIN_REACH` must
  cover the KO record**; the claim is the KO window, not the result naming.
- [ ] **Step 4: Audio.** Add the `.dro` with a measured minimum or name it
  out; do not fabricate a section.
- [ ] **Step 5: Prove and record.** ±1 every pin; run the oracle; record the
  measured values, the one-shot outcome, and (if failed) the exact session
  check. Update the claim-coverage/fought-match narrative.
- [ ] **Step 6: Commit (when authorized).** `gameplay: publish the two-human
  KO` or `docs: preserve the two-human KO attempt`.

**Acceptance:** a green oracle with the KO inside the window, or a preserved
failure with no pins; no KO poke in the scenario; the game-dealt `0x78` from a
live value is recorded.

---

### Task 4: U16 R2 — record the measured gate operand

**Files:**
- Modify: `docs/superpowers/plans/2026-10-27-claim-coverage-derivations.md`
  (append §C5)
- Modify: `docs/superpowers/plans/2026-10-29-live-truth-derivations.md` (§L4)
- Modify: `AGENTS.md`, `docs/PROGRESS.md`, `port/RE_GUIDE.md` where the
  "unmeasured" sentence lives
- Read: `tools/gp_u16_state.py`, `data/k11-captures/gp-u24-u16-state-t9-r5/
  poll.log`, `port/src/game/fighter.c` around `0x48599`, the state-coverage
  record §D3.4.

**Interfaces:**
- Consumes: the t9-r5 set/exit pair (measured today:
  `f=1531 s0_54=2 s0_30=0xEAE`, exit `f=1532 s0_30=0x1180`).
- Produces: the record's §C5 with the strict-inequality correction; no capture,
  no port change, no pin.

- [ ] **Step 1: Re-run the reader.** `python3 tools/gp_u16_state.py read
  --capture data/k11-captures/gp-u24-u16-state-t9-r5`; paste the set/exit
  lines. Confirm `pad_bios=0` and all 11 session checks in `session.txt`.
- [ ] **Step 2: Cross-check the gate.** Quote `0x48599 cmp edx,[eax+0x30]`,
  `0x4859C jle` and the `0xBD888` threshold `0x1400`; state that the clear is
  `y < 0x1400` (equality does not clear) and that the gate reads the previous
  iteration's latched y.
- [ ] **Step 3: Write §C5.** Record the measured `y(0x1531)=0x0EAE`; the
  pre-fix port value `0x202E`; the `+0x1180` record-motion class; the IVT
  stand-in fix; the verdict that the original's clear is the gate, and that
  the `.failed` artifact's missing set frame is not a measurement. State that
  no port change or pin follows.
- [ ] **Step 4: Update the prose.** Replace "the deciding gate input is
  unmeasured" with the measured value and the `.failed` caveat; note the
  claim-coverage plan's `<= 0x1400` boundary correction to `<`.
- [ ] **Step 5: Commit (when authorized).** `docs: record the measured U16
  gate operand`.

**Acceptance:** the record has the exact rows and the strict inequality; the
U23 pins and every existing pin are untouched.

---

### Task 5: `gp-u28-continue` — the continue screen (`0x0E`)

**Files:**
- Modify: `tools/gp_session.py` (new `SCENARIOS['gp-u28-continue']`; reuse
  `KO_P1`; `DRAW_EXTRA`/`WIN_EXTRA` fields; `SNAP_FIELDS` already has `w10d`,
  `cnt`, `b25`, `cred`)
- Create: `tools/gp_continue.py`, `tools/tests/test_gp_continue.py`
- Modify: `Makefile`, `port/tests/test_platform.c`, `tools/capture_manifest.json`,
  `tools/pin_baseline.sha256`, `tools/audio_compare.py` if the `.dro` joins
- Modify: the record
- Read: `flow.c:1789` (`flow_continue_open`), `:1813-1827` (the side rule),
  `:2000-2003` (the 64-frame countdown), `:2016-2021` (the prompt),
  `:1898` (`flow_continue_take`); `gp-u10-ending`'s mode-`0x0C` route;
  `flow.c:615-627` (`config_credit_ready`), `config.c:675-679`.

**Interfaces:**
- Consumes: the boot credit `5` (4 after one start), no credit write.
- Produces: a published `gp-u28-continue` + `gp-continue-oracle`, or a
  `.failed.N`.

- [ ] **Step 1: Derive the route.** Clone `gp-u10-ending`'s LEFT PLAYER ARCADE
  route to mode `0xC`; at the round entry poke `0x10780A=0x78` (`KO_P1`, the
  `DS_0010810D` side); stop on mode `0x0E` with a stop delay that shows the
  PRESS START/INSERT COIN prompt and the countdown, or on mode `0x15` for the
  expiry. Use one capture to `0x15` if the replay covers the countdown;
  otherwise stop at `0x0E` 200 frames in.
- [ ] **Step 2: Write the checker.** `gp_continue.py check` reads `mode=0x0E`,
  `w10d=0`, `cnt=0x0F` then a descending `cnt`, `cred` unchanged at 4, the
  prompt path, and the expiry's `mode=0x15`/`b25=0`. Add a `path` mode only
  for the fields the port trace carries. The claim is the screen/countdown,
  **not** the KO and **not** the taken round.
- [ ] **Step 3: Capture and accept.** `make gp-capture scenario=gp-u28-continue`;
  accept on all session checks and the checker; else `.failed.N`.
- [ ] **Step 4: Replay, miss set, oracle.** `make gp-replay`, add
  `k_miss_gp_u28_continue` and the `k_gp_sets` row, measure the pins, add
  `gp-u28-continue-oracle` (clone `gp-draw-timeout-oracle`), `CAPTURE_DIRS`,
  `.PHONY`, `verify`. The oracle's window must cover the prompt+countdown (or
  the expiry if that is the capture's stop).
- [ ] **Step 5: Audio, ±1, record.** Add the `.dro` with a measured minimum or
  name it out; prove every pin ±1; record the side-rule correction and the
  measured countdown. The census rows `0x27A2C`/`0x278B0`/`0x2791C` should
  improve; the re-pin is Task 12.
- [ ] **Step 6: Commit (when authorized).** `gameplay: capture the continue
  screen`.

**Acceptance:** the oracle is green; the capture shows `0x0E` and the
countdown (or the `0x15` expiry); the KO is a labelled poke; no existing pin
moved.

---

### Task 6: `gp-bowling` — the `0x25` easter egg

**Files:**
- Modify: `tools/gp_session.py` (new `SCENARIOS['gp-bowling']`; append the
  mode-`0x25` fields `0x108892`, `0x1088C4`, `0x1088A6`, `0x1088BD`,
  `0x104B15`, `0x1088C0`; `SNAP_FIELDS` is append-only)
- Create: `tools/gp_bowling.py`, `tools/tests/test_gp_bowling.py`
- Modify: `Makefile`, `port/tests/test_platform.c`, `tools/capture_manifest.json`,
  `tools/pin_baseline.sha256`, `tools/audio_compare.py` if the `.dro` joins
- Modify: the record
- Read: `fight.c:5578-5624` (`fight_mode25_enter`), `flow.c:3231`,
  `fight.c:9823-9845` (`fighter_45158`), `fight.c:10004-10032` (the reaction
  `0x22` callback), `tools/gp_session.py:250-263` (`gp-twop`),
  `tools/gp_moves.py` (`--char 4 --moves 0`).

**Interfaces:**
- Consumes: the two-human join (`b1f==3`), both characters 4, the reaction
  `0x22` move on both sides, `DS_00108892>=6`.
- Produces: a published `gp-bowling` + `gp-bowling-oracle`, or a `.failed.N`.

- [ ] **Step 1: Confirm the string and the gates.** Decode string `0x61` with
  the game's decoder (or a port test) before naming the mode; record the exact
  gates and the move-0 sequence. If the decoded text is not the bowling
  banner, use the measured text and correct the design.
- [ ] **Step 2: Build the scenario.** `gp-twop`'s join and char select: P1 five
  `p1.right` presses (cursor 0 → cell 5, class 4); P2's default cell 5 (class
  4); `p1.b0`/`p2.b0`; at mode `0x06` both perform move 0
  (`gp_moves.py steps --char 4 --facing 0 --moves 0` for P1, the facing-1
  mirror for P2); stop on mode `0x25` (`until_mode`), with enough tail to
  record the banner.
- [ ] **Step 3: Write the checker.** `gp_bowling.py check` reads `mode=0x25`,
  `b1f=3`, `b1d∉{2,3}`, `c0=c1=4`, `b25=0`, the appended
  `0x108892>=6`, `0x1088C4` 0→1, `0x1088A6=0xB4`, `0x1088BD=0`,
  `0x104B15=0`, `0x1088C0=1`, and the string row if the trace carries it.
- [ ] **Step 4: Capture and accept.** `make gp-capture scenario=gp-bowling`;
  accept on all session checks and the checker; else `.failed.N`.
- [ ] **Step 5: Replay, miss set, oracle.** `make gp-replay`, add
  `k_miss_gp_bowling`, measure the pins, add `gp-bowling-oracle` (clone
  `gp-twop-oracle`), `CAPTURE_DIRS`, `.PHONY`, `verify`. The window covers the
  mode-`0x25` entry and the banner; the scoreboard beyond the captured frames
  is not claimed.
- [ ] **Step 6: Audio, ±1, record.** Add the `.dro` or name it out; prove the
  pins ±1; record the measured trigger; the census rows `0x4E11C` and the
  family should improve (Task 12 re-pins).
- [ ] **Step 7: Commit (when authorized).** `gameplay: capture the 0x25
  bowling mode`.

**Acceptance:** the oracle is green; the trigger is real input (no poke); the
mode is no longer NOT-HIT; the state-coverage "camera" label is corrected.

---

### Task 7: `gp-u27` — the name-entry oracle

**Files:**
- Create: `tools/gp_u27.py`, `tools/tests/test_gp_u27.py`
- Modify: `tools/gp_compare.py` (`SCAN3_ALLOWED['gp-u27-name-entry']`),
  `tools/tests/test_gp_compare.py`
- Modify: `Makefile` (`GP_U27_*`, `gp-u27-oracle`, `CAPTURE_DIRS`, `.PHONY`,
  `verify`), `tools/capture_manifest.json`, `tools/pin_baseline.sha256`
- Modify: the record
- Read: `data/k11-captures/gp-u27-name-entry/poll.log`,
  `port/tests/test_platform.c` (`k_miss_gp_u27_name_entry`),
  `Makefile:1210-1225` (`gp-u25-oracle` model).

**Interfaces:**
- Consumes: the measured replay (`trace 0 differing through 9401`, `moves 0
  differing through 9401`, 4,991 dumped frames) and the one named frame
  (capture 334, mode-8 entry).
- Produces: a gated `gp-u27-oracle` in `verify`.

- [ ] **Step 1: Add the allowance.** Add
  `SCAN3_ALLOWED['gp-u27-name-entry'] = (334, 3, 63, '3f1839ec…')` with the
  measured row strip and the unique adjacent pair; add a mutation test that
  fails when one bound byte is changed. The recipe binds port indices
  219/221/222; a boot/attract frame-count change must fail loudly.
- [ ] **Step 2: Write the checker.** `gp_u27.py check` reads `b25`
  `00@0x217E → 04@0x217F → 05@0x2183 → 06@0x22CE → 07@0x23FC → 0A@0x23FD`;
  `ncnt 1@0x2193, 2@0x21B0, 3@0x21CE`; the name bytes `0x41/0x4241/0x444241/
  0x20444241`; `sc0=0x005217A0`; mode 3 at `0x24B9`. A `path` mode may assert
  only the port's `b25` (the other fields are not in `gp_trace_line`). Add the
  unit tests.
- [ ] **Step 3: Pins and oracle.** Add `GP_U27_MIN_FIRST 5871`,
  `GP_U27_MAX_START 81`, `GP_U27_MIN_REACH 4990`, `GP_U27_MAX_MISSING 38`,
  `GP_U27_TRACE_MIN_FIRST 9402`, `GP_U27_MOVES_MIN_FIRST 9402`, the sha
  `5e2c5824…` and `GP_U27_CAPTURE_FRAMES 5885` (re-measure all). Add the
  `gp-u27-oracle` target (clone `gp-u25-oracle`) to `CAPTURE_DIRS`, `.PHONY`
  and `verify`. The capture already exists; no new run.
- [ ] **Step 4: Prove and record.** ±1 every pin; the `SCAN3` mutation; record
  that all 303 content-bearing capture frames 5311..5616 are clean/splice
  explained. The earlier 258 scene-total figure and its mode split are
  withdrawn as not re-derivable.
- [ ] **Step 5: Commit (when authorized).** `gameplay: gate the name-entry
  oracle`.

**Acceptance:** `gp-u27-oracle` green in `verify`; the finaliser covered; the
capture 334 allowance mutation-proved; no existing pin moved.

---

### Task 8: U14 R1 — the game-evidence-only KO report

**Files:**
- Modify: `tools/gp_twop.py` (`check --ko --arcade`), `tools/tests/test_gp_twop.py`
- Modify: the record, `AGENTS.md`, `docs/PROGRESS.md`
- Read: `tools/gp_twop.py:260-318`, `tools/gp_win.py:377-405`,
  `docs/superpowers/plans/2026-10-23-endgame-truth-derivations.md` §E8,
  the fought-match record §F2/§F4.6, `data/k11-captures/gp-u14-win-live.failed`.

**Interfaces:**
- Consumes: the `.failed` artifact's `S` rows (`ko f=898`, `w3=2`, `b0c` never
  1, no `X`/`W`).
- Produces: a report-only predicate and record; no oracle, no pin, no capture.

- [ ] **Step 1: Add `--arcade`.** Replace the `two_human` condition in
  `gp_twop.ko()` with `b1f==1`; keep the WIN fields, the no-KO-poke rule, the
  live-to-`0x78` rule, the no-timeout rule and the result naming. Write the
  RED case: a two-human log fails `--arcade`; a log with a KO poke fails.
- [ ] **Step 2: Run it read-only.** `python3 tools/gp_twop.py check --ko
  --arcade --capture data/k11-captures/gp-u14-win-live.failed`; record
  `ko f=898; round result f=898; match result f=F9F`, `s0_5a=78`, `w3=2`,
  `b0c=0`, no `X`/`W`, and the four session failures (end frame, port
  script, unscripted input, stopped at the end).
- [ ] **Step 3: Record the decision.** The claim is game evidence only: the
  game's own damage path KO'd a fighter in a played arcade round, with no KO
  poke; the artifact is not a capture and adds no pins. Reject option (b)
  with the §E1.3 evidence (the `b0c=1` final appears with all seven final KOs
  poked).
- [ ] **Step 4: Keep the gap.** `GP_U14_*` stays empty; `GATE_NAMED_GAPS`
  keeps `gp-u14-win-live`; no capture is run. Update the U14 narrative.
- [ ] **Step 5: Commit (when authorized).** `docs: record the fought-KO
  evidence`.

**Acceptance:** the predicate and tests; the report; the win still declared;
no capture, oracle or pin.

---

### Task 9: `0x21` — the volleyball declaration

**Files:**
- Modify: the record (§L9), `AGENTS.md`, `docs/PROGRESS.md`, the design's
  `0x21` paragraph
- Read: `fight.c:2940-2977` (`fight_4bd98`), `fight.c:3080-3091` (the
  `fight_4b470` tail), `fight.c:1436-1500`/`3113` (the trample prelude),
  `flow.c:1632`/`2416` (`DS_00104ABC`), the state-coverage record §D2.

**Interfaces:**
- Consumes: the measured gate chain and the unmeasured eight-trample chain.
- Produces: the declaration; no capture, code or pin.

- [ ] **Step 1: Record the gate.** Quote the chain and the `+0x1F>7` tail, the
  two spawns, the `0x1E` timer, and the two-human gate `DS_00104ABC>=2`.
- [ ] **Step 2: Record the blockers.** The `b1d` byte is a mode byte, not a
  player count; the stage RNG is 1-in-7; the trample counter resets on
  landing; no capture has `+0x1F>=2`; a poke can force `+0x1F=8`, but the
  state-coverage design §9 stop rule says "Do not build one; declare the modes
  with their gates".
- [ ] **Step 3: State the decision.** Keep `0x21` declared; a later poked
  exhibit needs an explicit operator reversal of §9; a real two-human chain
  needs a measured sequence this phase does not have.
- [ ] **Step 4: Commit (when authorized).** `docs: re-declare the 0x21
  volleyball mode`.

**Acceptance:** the declaration cites the addresses; no new claim; the census
rows stay `NOT-HIT`.

---

### Task 10: The 5120 band — the DAC-injection experiment (optional)

**Files:**
- Modify: `tools/gp_session.py` (the `gp-u13-band-dac`/`gp-u13-band-fb`
  scenarios), `tools/gp_band.py`, `tools/tests/test_gp_band.py`
- Modify: the record; no `Makefile` oracle unless the experiment becomes a
  gate
- Read: the state-coverage record §D4, the endgame record §E9,
  `tools/gp_session.py`'s `gp-u13-band`, `port/src/platform/gfx.c` (the
  present/compose), `port/src/platform/sprite.c` (`0x51ED8`),
  `flow.c`'s mode-`0x15` ending handler.

**Interfaces:**
- Consumes: the pinned 13 offsets `(5120, 1, 5, 56b900b1…)` and the dumped
  DAC.
- Produces: one of four measured outcomes (pipeline, content divergence,
  older-present sample, invalid injection) or a declared instrument gap.

- [ ] **Step 1: Locate the fade palette source.** Derive the mode-`0x15`
  fade's palette-record call and the index-`0x32` source; pin the address.
  If it cannot be pinned from the raw bytes, stop and declare the instrument
  gap.
- [ ] **Step 2: Build E1.** A `gp-u13-ending2` clone with a poke at f=1825 of
  the index-`0x32` palette entry to an RGB unique in the port's f=1826 DAC;
  the dumped DAC's colour is the control. Add E2: poke `DS_000E87A4`'s rows
  1..4 with a unique index at f=1825.
- [ ] **Step 3: Capture.** One run each, with the injection disabled as the
  control; keep `gp-u13-band` untouched; do not add these diagnostics to
  `CAPTURE_DIRS` unless they become gates.
- [ ] **Step 4: Decide.** E1: the 8 band pixels turn the injected colour while
  the other 93 index-`0x32` positions stay black ⇒ index `0x32` ⇒
  DAC/compose timing (pipeline, no port fix); they stay `00001c` while the
  injection is visible elsewhere ⇒ content divergence; nothing changes ⇒ the
  injection missed the flush (invalid, not a result). E2: visible ⇒ current
  present; invisible ⇒ older present. Record the exact bytes.
- [ ] **Step 5: Record.** Move `BANDS_ALLOWED`'s 5120 entry only if E2 says
  "current present"; otherwise leave the bounds and the declared gap. No
  fitted cause.
- [ ] **Step 6: Commit (when authorized).** `gameplay: test the 5120 band's
  source` or `docs: declare the 5120 instrument gap`.

**Acceptance:** a falsifiable outcome with the control, or a declared gap; no
claim beyond the measured bytes.

**Execution outcome:** Task 10 was not taken because the fade palette source
was not funded. No injection or capture ran. The 5120 band stays INCONCLUSIVE,
with no fitted cause and unchanged bounds and pins (record §L10).

---

### Task 11: Audio — the diff-allowed prefix claim (optional)

**Files:**
- Modify: `tools/audio_compare.py`, `tools/tests/test_audio_compare.py`
- Modify: `Makefile` only if a per-section pin is added to the oracle
- Modify: the record, `AGENTS.md`
- Read: `tools/audio_compare.py:775-815` (the derived-k anchor),
  `tools/tests/test_audio_compare.py:528-566`, the music-truth record §M4.

**Interfaces:**
- Consumes: the already-computed `al.claimed` matched prefix of each
  diff-allowed section; the 27 reports.
- Produces: a per-section prefix pin, or the named residual; no new capture,
  no port change.

- [ ] **Step 1: Claim the prefixes.** For each diff-allowed section, pin the
  matched prefix length with a per-section minimum; a capture differing at
  tick 0 claims 0; a prefix one shorter fails. Add the nulls: a shifted copy
  and a shuffled capture still fail (the existing rotation nulls).
- [ ] **Step 2: Re-measure the bank inventory.** Correct the stale `16/5`
  sentence to the measured 12 claimed / 10 requested-unclaimed / 4
  never-requested (re-measure from the reports); name the 4 as needing new
  captures.
- [ ] **Step 3: Record.** The claim is the matched prefix, not the whole
  section; the unmatched remainder stays named. No audibility claim.
- [ ] **Step 4: Commit (when authorized).** `audio: claim the matched
  section prefixes`.

**Acceptance:** a prefix pin that fails one tick short; the 4 never-requested
banks named; no new capture; no audibility claim.

---

### Task 12: Docs, counters and the coverage re-pin

**Files:**
- Modify: `AGENTS.md`, `README.md` (title only if the percentage moves — it
  should not), `docs/PROGRESS.md`, `port/RE_GUIDE.md`, the design, the plan,
  the record
- Modify: `tools/coverage_baseline.txt` (`--update-baseline --reason` with the
  measured improved set), `tools/capture_manifest.json`,
  `tools/pin_baseline.sha256`
- Read: the landed task reports; `tools/port_progress.py`,
  `tools/coverage_census.py`, `tools/capture_manifest.py`.

**Interfaces:**
- Consumes: the new captures/oracles and the measured pins.
- Produces: docs and counters that match the measured state; no port code.

**Execution split:** Task 12a is docs-only. Task 12b re-pinned the measured census in
`tools/coverage_baseline.txt` (`tools/capture_manifest.json` and
`tools/pin_baseline.sha256` are its other measured owners); Task 12a did not modify them or run the census.
The current inventory is 42 explicit capture paths, 41/42 present with the U14
gap declared.

- [ ] **Step 1: Update the narrative.** Add the new oracles/commands, the
  capture inventory, and the corrected named gaps: `0x0B` exhibited (KO draw),
  `0x0E` exhibited, `0x25` declared after Task 6's structurally blocked
  attempts, `gp-u27` gated, U16 R2 measured, U14 game-evidence-only, `0x21`
  declared, 5120 outcome, audio prefix. Keep the U14 fought win declared.
- [ ] **Step 2: Re-measure the counters.** `python3 tools/port_progress.py`
  (expect `798 1203 66` / `759 759 100`); `make diff-verify` (expect the
  baseline line); `make entry-triage` (expect unchanged); regenerate
  `symbols.h` and confirm byte-identical; `capture_manifest --check-pins`.
- [ ] **Step 3: Re-pin the census.** `make coverage` (or the documented
  `coverage_census.py --update-baseline --reason "…"`) with the measured
  `improved:` set from the new captures; if no new rows are HIT, keep the
  baseline and record why. The census is a separate pin, not
  `pin_baseline.sha256`.
- [ ] **Step 4: Re-pin the tripwire.** `capture_manifest.py --update-pins
  --reason "…"` after the Makefile/pin changes; confirm the digest and the
  measured reason.
- [ ] **Step 5: Commit (when authorized).** `docs: close the live truth
  phase`.

**Acceptance:** every doc claim matches the measured pins; the counters are
re-measured, not copied; the README title is unchanged (no port code); the
U14 gap stays declared.

---

### Task 13: The full gate

**Files:** none (logs only).
**Interfaces:** consumes every landed task; produces the phase gate evidence.

- [ ] **Step 1: First full gate.** `PR_DIFF_FULLSCAN=1 GATE_STRICT=1
  PR_PINS_EXACT=1 make verify`, sequential, log to
  `/tmp/opencode/live-truth-gate-1.log`; exit 0. Confirm the capture count
  (the measured `CAPTURE_DIRS` inventory; the U14 gap declared), the diff
  counter, the new oracles ran, `entry-triage` and `symbols.h`.
- [ ] **Step 2: Second full gate.** The same command, log 2, exit 0; compare
  the pin lines with log 1 (identical).
- [ ] **Step 3: Audio and pins.** `make audio-render` twice; the selmo peak
  `2790` and sha `530a94301f4be46f9a5c666c94516f8a1f7fca6f136d3be0562faed802c47cef`
  unchanged (determinism, not audibility). Re-prove every new pin one step in
  the failing direction and restore; the gate log lists each.
- [ ] **Step 4: Close.** Record the gate, the counters and the named gaps in
  the record; update the design status to **Implemented and gated**; the
  final `docs/PROGRESS.md` paragraph. Commit (when authorized).

**Acceptance:** two green full gates; `audio-render` green twice; every new
pin ±1-proved; no existing pin weakens; the U14 gap remains declared.

---

## Parallelization and review

- **Task 1 first.** It fixes the baseline and the rulings.
- **Serial capture lane:** Tasks 2, 3, 5, 6, 10. One DOSBox-X run at a time;
  each task rebases on the phase branch before the next capture. The two-human
  KO (Task 3) is the highest-risk run: schedule it first on a quiet host.
- **Offline lane:** Tasks 4, 8, 9 and 11 may run in parallel worktrees after
  Task 1; Task 7 touches the Makefile and runs after the capture tasks'
  Makefile blocks (or in a worktree with a recorded rebase).
- **Docs/gate:** Task 12 after every landed task; Task 13 last.
- **Review:** each task gets a standards + spec review; the phase gets a
  whole-branch review before the gate. A failure branch is preserved, never
  hidden.
- **Maximum concurrent agents:** up to 14 may be dispatched (one capture
  lane plus up to 13 offline workers on disjoint files). The DOSBox-X host and
  the shared `gp_session.py`/`Makefile` serialize the capture tasks, so 6
  concurrent is the practical default; more agents would contend, not speed
  the captures.

## Named gaps after the phase

- U14's fought win (R1) — unless a new explicit go-ahead names a capture.
- The `0x21` volleyball content — unless the operator reverses state-coverage
  design §9 for a poked exhibit.
- The `0x25` bowling content — Task 6's symmetric route is structurally
  blocked; two `.failed` artifacts, no oracle or pins.
- The 5120 band's cause — Task 10 was not taken; the instrument gap stays
  declared and the band INCONCLUSIVE.
- Rendered audibility; the 4 never-requested audio banks; the terminal
  differential rows; the declared storage/config items.
