# Claim Coverage Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development
> (recommended) or superpowers:executing-plans to implement this plan task-by-task.
> Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the verification gate deterministic, make every completion count
and named gap honest, close the bounded differential and span-writer debt, and
take the three captures that turn the last undeclared behaviours into measured
claims — without fitting a constant, weakening a pin, or claiming U14 without an
explicit go-ahead.

**Architecture:** Stage 1 is offline: the sequencer's tick clock is moved onto
the modelled ISR tick (`DS_00101500`) in headless mode, the capture/pin gate
gains a manifest and preflight, the classification and E2 tables get their real
classes, three differential rows are closed, and the audio oracle gains a
declared channel-rotation anchor. A mid-phase gate must pass twice before Stage
2. Stage 2 uses the hardened capture harness for the two-human KO, the U16 gate
operand and the drawn match; U14 is conditional.

**Tech Stack:** Python 3 (diff harness, capture harness, audio/compare tools,
Ghidra/capstone), the C/SDL3 port, GNU Make, the raw `PRAGE.EXE`, the existing
captures. No new dependency.

**Spec:** `docs/superpowers/specs/2026-10-27-claim-coverage-design.md`

## Global Constraints

- Work from the repo root. `data/` is read-only; never write to `data/game/`.
  Captures write only `data/k11-captures/<scenario>` and preserve a failed run
  as `.failed.N`.
- **No fitted constants.** Every pin is measured from a tool's output and proved
  to fail one step in the tested direction. A value that cannot be pinned is a
  named gap with its evidence.
- **The byte-exact oracles are authoritative.** `PR_ORACLE_REQUIRED=1` stays on
  for `run_tests`; `make verify`'s `--check` step stays before the unit suite
  (`test_fn_misslog` reads `frames/fn_miss.txt`).
- **Existing title/attract/front-end/endgame pins must not move** except this
  phase's own re-measured audio pins. A title or attract regression stops the
  task.
- **A capture that fails a session check is not published.** No pin, no
  `CAPTURE_DIRS` entry, no oracle.
- **U14's next attempt runs only if the design's approval explicitly authorizes
  it.** The plan records the two possible forms; it does not choose one itself.
- **The final gate is both** `PR_DIFF_FULLSCAN=1 GATE_STRICT=1 PR_PINS_EXACT=1
  make verify` **and** `make audio-render`, each twice.
- Commit style `<area>: <what>`; stage named files, never `git add -A`; commit
  only when authorized.

## Review Focus

1. **One green run is not determinism.** The audio fix is proved by two
   consecutive full gates under load and by a byte-identical `PR_AUDIO_DUMP`
   (labels included), not by one retry.
2. **A pin that moved silently.** Every re-pinned audio minimum is shown at
   ±1; a pin that drops without a measurement is a failure.
3. **A count fitted to a target.** The classification and E2 counts are
   whatever the bytes say. A reclassification without an evidence line is a
   failure; so is an E2 universe widened without classifying the 71.
4. **A row closed by weakening it.** The three differential closures must keep
   their mutants and their byte comparisons; deleting a case or a declared
   byte is not a closure.
5. **A rotation anchor that passes by searching.** The null test (a wrong trace
   or shuffled capture must fail) is mandatory; the mutation test (one dropped
   write) is mandatory.
6. **A capture accepted with a failed session check.** `port script v2`,
   `unscripted input`, `end frame reached` and `stopped at the end` all pass
   before publication.
7. **A doc claim left stale.** The reconciliation task's list is checked against
   the code/record, not just reworded.
8. **U14 run without a go-ahead.** The task is skipped unless the approval names
   it.

---

### Task 1: Baseline and the derivation record

**Files:**
- Create: `docs/superpowers/plans/2026-10-27-claim-coverage-derivations.md`
- Read: `tools/port_progress.py`, `tools/diff_verify.py` (`TERMINAL_CENSUS`,
  `WEAKER`, `UNROWED`), `tools/entry_triage.py`, `tools/audio_compare.py`,
  `Makefile` (`gate-check`, `verify`, `audio-oracle`), the fought-match record
  §F6.3, the music-truth record §M4-§M6.

**Interfaces:**
- Produces: §C1 — the measured baseline and every artefact the later tasks cite.

- [ ] **Step 1: Record the counters.** Run and paste:
  ```bash
  python3 tools/port_progress.py
  python3 tools/port_progress.py --unported
  make gate-check
  make diff-verify
  ```
  Record the `799 1203 66` / `759 759 100` line, the diff counter line, the
  20 open rows, the 15 `WEAKER` entries and the 8 `UNROWED` addresses. The
  three lists are `TERMINAL_CENSUS`, `WEAKER` and `UNROWED` in
  `tools/diff_verify.py`; extract them with a one-liner over those module sets
  rather than retyping AGENTS.md.
- [ ] **Step 2: Reproduce the audio flake.** Run `make audio-oracle` twice under
  load (a `make -j` or a host load generator); record the `W`/`S` label
  differences and the failing trace/claim exactly as the fought-match record
  §F6.3 did. A clean pair is a baseline too; say so.
- [ ] **Step 3: Record the classification audit.** Paste the 55-entry
  `tools/port_classification.txt` summary (41 host + 12 deferred, 2 inert), the
  `0x62003` false-positive comment (`port/src/game/fighter.c:1175`), the ten
  inline credits besides the three `UNROWED_HEADER_SHAPE` addresses
  (`0x1B120`, `0x20C10`, `0x4FBA2`), and the caller-closure reading of the 351
  runtime rows (exclusive-ancestor: 37 movie-only / 19,261 B, 56 AIL-only /
  10,824 B, 258 remainder; the 43/76 reading does not reproduce — record the
  rule).
- [ ] **Step 4: Record the E2 and audio measurements.** Paste the 227/158
  split, the `FILLERS` line (`tools/entry_triage.py:40`), the measured
  admission (157, all `span-writer`; universe 579 → 736, U0 575 → 732; the
  158th writer `0x5D004` is above `RUNTIME_BASE`) and the audio rotation
  measurement (the five matched / five requested-only / sixteen unattributed
  banks; the `documented_excluded` second-bank line and the record's
  `0x145=0x20` at gp-u23's selmo tick 25).
- [ ] **Step 5: Record the capture artefacts.** `data/k11-captures/gp-u14-win-live.failed*`,
  `gp-twop-ko.failed*`, `gp-u24-u16-state.failed*`: frames, S records, the
  failing check, and what they still prove. Record the absent U14 target and
  the empty `GP_U14_*` block.
- [ ] **Step 6: Write §C1 and commit (when authorized).**
  `docs: baseline the claim coverage phase`.

---

### Task 2: Deterministic sequencer clock

**Files:**
- Modify: `port/src/game/flow.c` (`game_audio_service`, `s_last_host_tick`),
  `port/tests/test_audio.c` (the PCM pin), `tools/audio_compare.py`
  (`TRACE_PINS`), `Makefile` (`verify`, `audio-oracle` if needed).
- Create/modify: `docs/superpowers/plans/2026-10-27-claim-coverage-derivations.md`
  (§C2).

**Interfaces:**
- Consumes: §C1's flake reproduction.
- Produces: a deterministic headless sequencer clock; re-measured
  `TRACE_PINS` and `audio-render` pin; `audio-render` in `verify`.

- [ ] **Step 1: Write the failing determinism case.** Add a Makefile/tools check
  (two `make audio-one scenario=...` runs + `cmp` of the `PR_AUDIO_DUMP`
  bytes/labels), not a `run_tests` case: a driver cannot spawn itself.
  Expected: FAIL under load before the fix.
- [ ] **Step 2: Select the clock.** In `game_audio_service`, use
  `host_audio_rate() == 0 ? DSD(DS_00101500) : host_tick_count()` for the
  sequencer's elapsed ticks (the same source the mixer already uses at
  `flow.c:6321-6345`). Keep the device path wall-clock. Use two deltas and a
  new `s_last_seq_tick` initialized beside `s_last_isr_tick` (`flow.c:6134`);
  `s_last_isr_tick` is the mixer's and is consumed at 6332. Do not change
  `GAME_AUDIO_TICKS_PER_HOST_TICK` (2, the measured 120 Hz sequencer / 60 Hz
  tick).
- [ ] **Step 3: Re-measure, do not predict.** Run `make audio-oracle` twice.
  Re-pin `TRACE_PINS` only from the measured output; prove every moved pin at
  ±1. **The `audio-render` peak/sha must not move** (the WAV blocks pace the
  sequencer from rendered audio, not `game_audio_service`); if it moves, stop
  and investigate. The tool has no `--pin` flag; use the manual `N`/`N+1`
  procedure.
- [ ] **Step 4: Add `audio-render` to `verify`.** Put it after `audio-oracle`
  (or after `diff-verify`), with the same build dependency; override
  `AUDIO_WAV`/`AUDIO_SONG_WAV` for a parallel lane; record the run time.
- [ ] **Step 5: Confirm the title/attract path.** The 9866-write title oracle,
  the symmetric comparison and the announcer sample are unchanged. If a
  title/attract pin moves, stop.
- [ ] **Step 6: Commit (when authorized).** `audio: make the sequencer clock
  deterministic`.

---

### Task 3: Capture manifest and gate hardening

**Files:**
- Create: `tools/capture_manifest.json`, `tools/capture_manifest.py`,
  `tools/pin_baseline.sha256`, `tools/tests/test_capture_manifest.py`;
  modify `Makefile` (`gate-check`, the pin-region marker comments),
  `tools/gp_capture.py` (preflight), `tools/tests/test_gp_capture.py`.
- Read: `Makefile` (`CAPTURE_DIRS`, `GATE_NAMED_GAPS`, `GP_*` pins),
  `tools/gp_capture.py` (the poller/publish path).

**Interfaces:**
- Consumes: the 35 `CAPTURE_DIRS` paths and the existing pins.
- Produces: a manifest check (present captures: `poll.log` sha256, frame
  count, `.dro` inventory; absent declared gaps: skipped), a clean-host
  preflight, and a pin-baseline tripwire.

- [ ] **Step 1: Generate the manifest from the present captures.** Two families:
  the 30 gameplay/menu captures have `poll.log` sha256, the frame count, the
  `.dro` files where present and the first/last `S` frame; the five frame-dump captures
  (`data/smk-captures/{twi5,twg}`, `data/title-captures/{title,title2,frontend}`)
  have no `poll.log`/`.dro` and are pinned by frame count plus a content hash of
  the frame set (or an explicit exclusion by name citing the existing oracle
  pins). For an absent path, record `"declared": true` and the reason. Do not
  invent a value for an absent capture.
- [ ] **Step 2: Add `--check`.** It fails if a present capture's identity
  differs, if an expected `.dro` is missing, or if a declared gap is present
  (the same stale rule as `gate-check`). It **skips** a declared absence: the
  captures are git-ignored, so a fresh checkout must still pass.
- [ ] **Step 3: Add the preflight.** Split it: `--preflight-verify` checks
  `data/game/C/PRAGE.EXE`, the `INDEX`, `port/tests/ghidra_data.bin` and the
  CMOS 2040-zero rule; capstone/unicorn stay skip-if-absent (the documented
  `diff-verify` behaviour). `--preflight-capture` (called by `gp-capture` only)
  checks `dosbox-x`, `ffmpeg`, the writable capture root, `memsize` and the load
  average. A failed preflight writes no capture.
- [ ] **Step 4: Add the pin-baseline tripwire.** Wrap the pin-defining regions
  of `Makefile` and the Python pin tables in marker comments (`TRACE_PINS`,
  `FRONTEND_*`, `ATTRACT_*`, `GP_*`, `K11_*`/`K11_OPEN_END`,
  `DEMO_FIGHT_MIN_FIRST`, `TITLE_*`, `SMK`, and `gp_compare`'s
  `SCAN3_ALLOWED`/`OVERLAY_ALLOWED`/`BANDS_ALLOWED`);
  `tools/pin_baseline.sha256` hashes the marked text. `gate-check` recomputes
  and fails on a mismatch; an `--update` requires the record's measured reason.
  Replace the literal `35` in the Makefile (and `AGENTS.md` if it repeats it)
  with a derived count.
- [ ] **Step 5: Wire `gate-check`.** Run `capture_manifest.py --check` and the
  pin-baseline check after the inventory. Prove a failed capture hash, a stale
  declared gap and a mutated pin block each exit 1.
- [ ] **Step 6: Test.** `test_capture_manifest.py` builds a temp capture dir
  with a synthetic `poll.log` and asserts the hash/count check, the declared
  absence, the stale case, the missing `.dro` case, the frame-dump family and
  the pin-baseline mismatch. Mutation: change one byte in `poll.log` → fail;
  change one pin digit → fail. Add the test module to the Makefile's explicit
  unittest list (`Makefile:2232`).
- [ ] **Step 7: Commit (when authorized).** `tools: gate the capture identity`.

---

### Task 4: Classification truth

**Files:**
- Modify: `tools/port_classification.txt`, `tools/port_progress.py`,
  `README.md`, `AGENTS.md`, `docs/PROGRESS.md`,
  `port/src/game/fighter.c:1175` (the `0x62003` comment),
  `port/src/game/actors.c` / `port/src/game/flow.c` (the `0x2EA64`/`0x500BB`
  credits), `tools/tests/test_port_progress.py` (new).
- Read: `port/decomp/prage.functions.csv`, `port/decomp/prage.calls.csv`,
  `port/src/symbols.h`, the proof-completion record §P8.

**Interfaces:**
- Consumes: §C1's audit.
- Produces: one declared class per `FN_`; a corrected `0x62003` count; a
  counting-rule test.

- [ ] **Step 1: Write the failing count test.** Assert `0x62003` is not credited
  by the `fighter.c:1175` comment, and that the class map covers every `FN_`
  exactly once. Expected: FAIL on the current tree.
- [ ] **Step 2: Derive the `replaced` class.** Measure the movie-only and
  AIL-only closures from `prage.calls.csv` with a **recorded rule** (the
  exclusive-ancestor reading is 37 movie-only / 19,261 B and 56 AIL-only /
  10,824 B; the 43/76 reading does not reproduce). Add one evidence line per
  address and the closure rule in the file header. Update the header comment to
  define `replaced` and say the portable denominator is unaffected (all are
  `>= 0x5D000`). Do not ship a split without the rule.
- [ ] **Step 3: Fix `0x62003`.** Change `fighter.c:1175` to a `/* PORT: 0x62003
  … */` note so the `/* 0xADDR` credit rule does not match it. Re-run
  `port_progress.py`: the raw count drops by the measured number (`0x62003`
  alone makes it 798); the portable figure is unchanged. Add the regression
  assertion.
- [ ] **Step 4: Resolve the inline credits.** Enumerate the ten inline credits
  besides the three `UNROWED_HEADER_SHAPE` addresses (`0x1B120`, `0x20C10`,
  `0x4FBA2`). For each, either add a real `/* 0xADDR — … */` header (and a real
  function for `0x2EA64`/`0x500BB` if the site is a call), or add the address to
  `PORT_PROGRESS_EXCEPTIONS` with its evidence. Do not leave an undeclared
  inline credit.
- [ ] **Step 5: Update the tool and the narrative.** `port_progress.py` prints
  the class breakdown (ported / host-owned / deferred / replaced / runtime) and
  stops printing "excludes 53" after the `replaced` class lands. README
  title/lines, `AGENTS.md`'s raw/portable paragraph and `docs/PROGRESS.md`:
  "759/759 addresses implemented or declared; the measured movie-only and
  AIL-only closures are replaced; 0x62003 is not a port". Keep the raw
  percentage at the measured value.
- [ ] **Step 6: Add the test to the gate.** Add `test_port_progress` to the
  Makefile's explicit unittest list (`Makefile:2232`).
- [ ] **Step 7: Commit (when authorized).** `docs: classify the runtime
  replacement layer`.

---

### Task 5: E2 writer triage

**Files:**
- Modify: `tools/entry_triage.py` (`FILLERS`), the committed E2 table
  (`docs/superpowers/plans/2026-10-01-reverse-e2-triage.md`), the `--expect`
  counts in the Makefile/CI, `tools/tests/test_entry_triage.py` if present.
- Read: the proof-completion record §P2-§P4, `tools/tests/test_diff_verify.py`
  (`SpanCensusTests`), `tools/diff_verify.py` (`D5B_*`).

**Interfaces:**
- Consumes: §C1's measured 157-admission result.
- Produces: 384 of 385 span-writer slots with an E2 row, and `0x5D004` named
  by its `RUNTIME_BASE` rule.

- [ ] **Step 1: Write the failing triage case.** Assert a known untriaged
  writer (one of the 157 `ret` + `87 db` slots) is admitted after the filler
  change; expected FAIL.
- [ ] **Step 2: Add `87 db` to `FILLERS`.** Regenerate the table with the
  command in `AGENTS.md` (`diffrun --image-out` + `entry_triage.py --image
  --live --out`). The measured result is **157 admissions, all
  `span-writer`**: universe 579 → 736, U0 575 → 732.
- [ ] **Step 3: Name the residual.** The 158th writer (`0x5D004`) is above
  `RUNTIME_BASE`, so it is not an E2 candidate; give it a declared exception or
  a separate row. There is no 71-address non-writer set to classify; if the
  measured run disagrees, record the actual split and do not force this one.
- [ ] **Step 4: Re-pin and prove.** Update `--expect` to the measured counts
  (736 / 732); prove the gate fails on the old count and passes on the new. The
  span census (417/392/385/386) is unchanged. If the table cannot be
  regenerated cleanly, revert the filler and keep 158 as a named residual.
- [ ] **Step 5: Commit (when authorized).** `tools: triage the span writers`.

---

### Task 6: Differential micro-closures

**Files:**
- Modify: `port/src/platform/sprite.c` (`sprite_blit_aperture`'s node
  write-backs), `port/src/game/flow.c` (`game_shutdown`'s guards and the
  `0x4FB98`/`0x1ADE4` calls), `tools/diff_verify.py` (`WEAKER`, the `0x51ED8`
  declared bytes, the `0x1BE30` row, the `0x255CC` unhits),
  `tools/diff_emu.py` (`E.Machine` `int 0x10`/`int 0x31`),
  `tools/tests/test_diff_verify.py` (the `WEAKER` counts and the counter
  string), `port/tests/diff_runner.c` (`n1_51ed8_store`/`n1_51ed8_run`),
  `port/tests/test_platform.c` only if the port tests need the seam.
- Read: the proof-completion record §P8.3, the track-N record §N.1/§N.2, the
  track-G record §G4, the raw bytes at `0x51F2F`, `0x5D21F`, `0x5D262`,
  `0x57FCE`, `0x1BE44`/`0x1BE61`/`0x1BE87`, `0x25680`/`0x256D4`.

**Interfaces:**
- Consumes: the raw addresses above.
- Produces: `807 → 810` full rows and `WEAKER 15 → 12` if all three close;
  otherwise the measured `809`/`13` with the `0x255CC` `0x256D4` reason.

- [ ] **Step 1: `0x51ED8`.** In `sprite_blit_aperture` (**not** the shared
  `sprite_blit_at`, which the already-full `0x51E5C` row uses), after a **live**
  renderer entry only (types 1/2/9, not the `ret` types `0x0A`/5), write the
  renderers' scratch back: `n->rows = 0` (the `dec` loop runs to zero) and
  `n->stride = 0x140 - width` or `0x140 + width` (mirrored type 9), exactly as
  `0x5D262`/`0x57FCE`/`0x57F8B` do. The current port stores `rows - clip_b`
  and leaves `n->stride` at the seed. Delete `h2_scratch`'s declared entries
  and the `WEAKER` entry; run the row: it must be full.
- [ ] **Step 2: `0x1BE30`.** Implement the three raw guards and the `0x4FB98`
  call, and put `0x1ADE4` in the row's `allow_calls` as a host-owned callee
  (vector 9 / `[0x101514]+0x14,+0x16`; vector `0x23` / `0x10151A/1C`; vector
  `0x1B` / `0x101518/1E`). **Do not use `PR_SEAM` for the restores**: a stubbed
  seam returns from `game_shutdown`, and a recorded stub is not excused, so the
  row would stay open. Add the `Machine` `int 0x10`/`int 0x31` answer if the
  row needs it. Remove the code poke / the `WEAKER` entry and add the case
  with the segment words set.
- [ ] **Step 3: `0x255CC`.** `0x25680` is reachable with
  `Machine(aperture=True)` and a seeded back buffer (`0xE87A4`); add that case.
  `0x256D4` waits on the absent ISR tick `0x101508` and loops forever unless an
  ISR/tick model is added; if it is not modelled, leave it named and accept
  `809`. Do not delete a case to claim a closure.
- [ ] **Step 4: Measure.** `make diff-verify` must print the new counter and
  `0` named gaps. Update `tools/tests/test_diff_verify.py`'s pinned
  `WEAKER` counts (15 → the measured 12 or 13), the closed rows' `WEAKER`
  entries, and the counter string. Prove each closed row's mutant still fails
  (re-run one mutant per row) and that `WEAKER` shrank by the measured number.
- [ ] **Step 5: Commit (when authorized).** `diff: close the bounded rows`.

---

### Task 7: Audio coverage

**Files:**
- Modify: `tools/audio_compare.py` (`documented_excluded`, the anchor
  derivation, `TRACE_PINS`, `AUDIO_END` handling), `tools/tests/test_audio_compare.py`,
  `Makefile` (`AUDIO_END`, `audio-oracle`), `docs/superpowers/plans/2026-10-27-claim-coverage-derivations.md`
  (§C3).

**Interfaces:**
- Consumes: §C1's rotation measurement.
- Produces: the second-bank carrier-TL fix, the declared rotation anchor, the
  re-pinned traces, and the claimed sections/banks.

- [ ] **Step 1: Write the failing second-bank case.** Use the record's measured
  write `0x145=0x20` at `gp-u23-u16-finisher`'s selmo tick 25 (gp-u5's selmo is
  per-tick equal with and without the exclusion, so it is not the case). A
  first-bank write on the same channel must still be excluded only for the
  declared channels. Expected: FAIL on the current `attr in (1, 4)` rule.
- [ ] **Step 2: Fix the exclusion.** Derive the predicate from the measured
  attr (bank-1 channel 11 for `0x145`; `11 % 9 = 2`, so the guessed
  `attr % 9 in (1, 4)` does not cover it) and cover the second bank's
  `0x143..0x155` carrier-TL writes. Record the exact register/attr pair the
  capture proves.
- [ ] **Step 3: Add the rotation anchor.** Derive the constant channel rotation
  `k` from the sequencer's voice-cursor state (the driver state that outlives a
  song), not by searching; verify the first `ANCHOR_TICKS` groups, then compare
  the full section. The null test must include a **rotated copy of the correct
  trace** and a shuffled capture; a one-write drop must fail the mutation test.
  If `k` cannot be derived, leave the section unclaimed rather than search it.
- [ ] **Step 4: Raise the `AUDIO_END` cap.** Set one cap above the maximum `X`
  of the 29 traces; the `capture-end` command still returns each capture's own
  `X` (`min(cap, X)`). Budget the extra replay time (the gate is ~26 min and
  `audio-render` runs `run_tests` again). Record the sections/banks that now
  anchor and the ones that still do not (citys, himal, sound `+0x5B88`, sound
  `+0x3E64` at least).
- [ ] **Step 5: Prove and commit.** `make audio-oracle` twice, all 29/29
  inventory, the null/mutation tests, and the re-pinned minima at ±1. Update
  `tools/pin_baseline.sha256` with the measured reason. `audio: anchor the match
  sections`.

---

### Task 8: Spec/claim reconciliation

**Files:**
- Modify: `port/spec/audio.md` (`:13`, `:24`, `:817`), `port/spec/game_flow.md`
  (`:372`, `:486`, `:1831`, `:2022`, `:2063`, `:3011`), `port/RE_GUIDE.md`
  (`:222`), `docs/PROGRESS.md` (`:11`), the `0x4F83C` wording and the
  `gp-u6-moves` audio-inventory line.
- Create: `tools/check_docs_claims.py`, `tools/tests/test_check_docs_claims.py`
  (a pinned stale-phrase list; the test seeds a line and expects a failure).

**Interfaces:**
- Consumes: the docs sweep in §C1.
- Produces: no doc states a ported function as a gap; the check runs in
  `verify`.

- [ ] **Step 1: Fix each line against the code/record.** For every stale claim,
  cite the port function/row that supersedes it. `audio.md`'s "never heard" is
  superseded by the 49716 Hz host open and the music-truth phase; the
  `0x17580`/`0x4AAD0`/`0x36870`/`0x4F83C` lines have ported implementations;
  `RE_GUIDE`'s "4b gap" is only the missing coin capture; `PROGRESS`'s "port is
  complete" is the portable figure, not the proof state.
- [ ] **Step 2: Add the guard.** `check_docs_claims.py` holds a short list of
  (file, phrase) that must not appear (e.g. "never heard", "still a named
  gap" for the four ported addresses). The test seeds one and expects failure.
  Run it in `verify` after the tool suites; add `test_check_docs_claims` to the
  Makefile's explicit unittest list (`Makefile:2232`).
- [ ] **Step 3: Commit (when authorized).** `docs: reconcile the stale gap
  claims`.

---

### Mid-phase gate

- [ ] `PR_DIFF_FULLSCAN=1 GATE_STRICT=1 PR_PINS_EXACT=1 make verify` twice,
  green under load.
- [ ] `make audio-render` twice, same peak/sha.
- [ ] `make gate-check` reads `34/35` with the declared U14 gap; the manifest
  matches.
- [ ] No title/attract/front-end/endgame pin moved except the phase's audio
  re-pins; each is proved at ±1.
- [ ] If the gate cannot be made deterministic, **stop** and record the
  residual; do not start Stage 2.

---

### Task 9: Two-human KO

**Files:**
- Modify: `tools/gp_capture.py` (pin guard, counted skip, fast-fail),
  `tools/gp_session.py` only if the scenario needs a measured stop,
  `tools/gp_twop.py` (no change unless `check --ko` needs a field),
  `Makefile` (`gp-twop-ko-oracle`, `GP_TWOP_KO_*`, `CAPTURE_DIRS` plus one),
  `port/tests/test_platform.c` (the miss set),
  `tools/audio_compare.py` (the `.dro` inventory if present),
  `tools/tests/test_gp_capture.py`.
- Read: the fought-match record §F3-§F4, `gp-twop-ko` (`tools/gp_session.py:831-870`),
  `gp_twop.py check --ko`.

**Interfaces:**
- Consumes: Task 3's preflight and manifest.
- Produces: a published `gp-twop-ko`, its oracle and pins, or a preserved
  `.failed.N`.

- [ ] **Step 1: Write the failing guard case.** A synthetic poll log with a
  missed `S(c-1)` must fail fast (a returned error, not a 78-second run). The
  guard queues no BIOS word until `S(f)` and `S(f-1)` are accepted; a
  `Schedule` backlog is capped below the 16-entry ring and counted as a skip.
- [ ] **Step 2: Implement the guard and the preflight.** `gp-capture` calls the
  Task 3 preflight, refuses a load average above the host's cores, and
  SIGTERMs on a gap. The skip counter is reported in the run summary;
  `steps_ok` accepts only the declared `skip_on_mode` skips.
- [ ] **Step 3: Run the capture.** One guarded run of `gp-twop-ko`. Accept only
  if `gp_twop.py check --ko` passes, the session checks (`port script v2`,
  `unscripted input`, `end frame reached`, `stopped at the end`) pass, and
  `check-input` is clean. Otherwise preserve `.failed.N` and stop.
- [ ] **Step 4: Replay and pin.** `gp-replay`, the miss set, `gp_compare`, the
  eight `GP_TWOP_KO_*` pins (`MIN_FIRST`, `TRACE`, `MOVES`, `MAX_START`,
  `MAX_MISSING`, `MIN_REACH`, `SHA256`, `FRAMES`), each measured and proved at
  ±1. **The oracle's `N`/`MIN_REACH` must cover the KO record.** If the port's
  replay stops before it, publish the capture as game evidence only with the
  replay gap named, or do not publish — and add no oracle pins. If the window
  covers the KO, add the oracle to `verify`; the gate count is the measured
  `CAPTURE_DIRS` inventory after the task. Update
  `tools/pin_baseline.sha256` with the measured reason.
- [ ] **Step 5: Commit (when authorized).** `gameplay: publish the two-human
  KO`.

---

### Task 10: U16 R2 (measurement only)

**Files:**
- Create: `tools/gp_u16_state.py` (the transition reader), maybe
  `tools/gp_debug_capture.py` (the breakpoint path).
- Modify: `tools/gp_capture.py` only if the guarded retry needs a flag;
  `docs/superpowers/plans/2026-10-27-claim-coverage-derivations.md` (§C4).
- Read: the fought-match record §F5, `gp-u24-u16-state`,
  `port/src/game/fighter.c:16258` (the `0x4844C` function; the case-3 arm is
  around `:16283`), `0x48599`.

**Interfaces:**
- Consumes: the measured bound (the plateau `0x09C0`, the port's `+0x4EE`
  delta, the exit `0x1180`).
- Produces: the measured `y(0x1532)` and its `0x1400` relation, or a recorded
  inability to measure it. **No port-code fix in this phase.**

- [ ] **Step 1: Write the transition reader's test.** A synthetic log with
  `s0_54` 2 then 0 within one iteration reads the set/exit pair and the
  `s0_30` values; a log with only the exit reports "set frame missing".
- [ ] **Step 2: Guarded retry.** Run `gp-u24-u16-state` with the Task 9 guard.
  Read the `s0_54=2` set frame and the exit frame. If the poller still misses
  it, go to Step 3.
- [ ] **Step 3: Debugger path.** `BPX 0x48599` (the `cmp edx,[eax+0x30]`) with
  a **frame/record condition and a hit cap**: the breakpoint is in the reaction
  arm, so without the condition it fires for every slot and frame. Script a
  dump of `edx`, `[eax+0x30]`, `s0_42`/`0xEF6DC`, then auto-continue. Record the
  DOSBox-X command lines.
- [ ] **Step 4: Decide and record.** If `y(0x1532) <= 0x1400`, the original
  exits on the `0x4859C` gate as observed; the port's reaction amplitude/anchor
  is wrong at `0x48374`/`0xED8FE`/`0xCFC32[0x1E7]` (its y stays at `0x202E` and
  holds the reaction 0x29 frames longer). Record the exact raw evidence for a
  follow-up, **do not patch it here**. If `y > 0x1400`, the original's exit
  contradicts the gate model; record that contradiction (the gate/rule is the
  suspect, not the amplitude). Either way, no fitted fix and no port/src
  change.
- [ ] **Step 5: Commit (when authorized).** `gameplay: measure the U16 gate
  operand`.

---

### Task 11: Drawn match `0x0A`/`0x0B`

**Files:**
- Modify: `tools/gp_session.py` (the new scenario, cloned from `gp-u9-win`),
  `tools/gp_win.py`/`gp_compare`/`gp_draw.py` as needed, `Makefile`
  (`gp-draw-oracle`, `GP_DRAW_*`, `CAPTURE_DIRS`), `port/tests/test_platform.c`
  (the miss set), `tools/audio_compare.py` if the capture has a `.dro`.
- Read: `port/src/game/flow.c:3040-3065` (`flow_round_over` `0x27ED8`, which
  sets mode `0x0A`), `:4191` (`game_mode_0a_step` `0x28BD4`), `:3146`
  (`flow_winner_pose_step` `0x28C38`, the `0x0B` pose tick), `gp-u9-win`'s
  `WIN_MENU`/`KO_POKES`, the U9/U10 record §W.9.

**Interfaces:**
- Consumes: the `gp-u9-win` scenario and its poke helpers.
- Produces: a capture that reaches the KO branch `B1E==ADC && AD4==2`, the
  round-over `0x0A` and the `0x0B` winner pose; an oracle and pins. The
  timeout/health drawn round (`0x280BA`) is **not** this claim.

- [ ] **Step 1: Derive the poke sequence.** From `gp-u9-win`'s KO pokes, build a
  round 1 P1 win and a round 2 P2 win (one KO each way), so the match is 1-1
  and `AD4==2`. `adc`/`b1e`/`ad4` are not all in `SNAP_FIELDS`; add a
  `DRAW_EXTRA` field set (or the measured equivalent) so the check can read
  `B1E==ADC` — the existing `gp_twop.py` rejects `ad4==2`. Record the exact
  addresses/values and the round entry frames.
- [ ] **Step 2: Build the scenario.** Clone the menu path and the two rounds;
  stop on the `0x0A`/`0x0B` end (`stop_mode`, `stop_delay`). No pokes other than
  the two KOs.
- [ ] **Step 3: Capture and check.** One run; accept only if the session checks
  pass and the S records show `AD4==2`, `B1E==ADC`, mode `0x0A`, mode `0x0B`.
  A `gp_draw.py check` (or the measured equivalent) reads the new fields; the
  existing `gp_twop.py` is not the checker. A failure is preserved; no pins.
- [ ] **Step 4: Replay and pin.** `gp-replay`, the miss set, `gp_compare` and
  the `GP_DRAW_*` pins; add the oracle to `verify` **only if the port's replay
  reaches `0x0A`/`0x0B`** so the window covers them; otherwise publish as game
  evidence with the replay gap named. If the capture has a `.dro`, add it to
  `audio_compare.py`'s `GP_TRACES`/`EXPECTED_FILES` and the Makefile's
  `AUDIO_TRACES`; otherwise name it out of the inventory. Update
  `tools/pin_baseline.sha256` with the measured reason. The claim is the
  KO-branch round-over/winner-pose content, not the poked KOs.
- [ ] **Step 5: Commit (when authorized).** `gameplay: capture the drawn
  match`.

---

### Task 12: U14 R1 (conditional)

**Files:**
- Modify: `tools/gp_session.py` (only if the approval names a new scenario),
  `Makefile` (`GP_U14_*` if published), `port/tests/test_platform.c` (miss
  set), `tools/gp_win.py` (no change unless the acceptance rule needs a field).
- Read: the endgame record §E8, the fought-match record §F2, `gp-u14-win-live`.

**Interfaces:**
- Consumes: the Task 9 guard and the Task 3 preflight.
- Produces: a published `gp-u14-win-live`, or a preserved `.failed.N`, or a
  skipped task.

- [ ] **Step 1: Check the authorization.** If the design approval does not name
  the form (the 1P 160-attempt script or the two-human substitute), skip this
  task and record "not authorized" in §C5. Do not run.
- [ ] **Step 2: Run the authorized form.** Use the guard/preflight; the 1P form
  is `gp-u14-win-live` unchanged (it has not won in three attempts: one tool
  timeout, two 0-2 losses); the two-human form is a new scenario with `b1f=3`
  and an idle P2, clearly labelled as a different claim.
- [ ] **Step 3: Accept or preserve.** Accept only on the session checks plus
  `gp_win.py check`'s `live_death` rule (`b0c=1` after the KO, no
  `DEATH_DONE`). A loss is an outcome, not a harness failure; preserve it and
  keep the gap.
- [ ] **Step 4: Pin and commit (when authorized).** The nine `GP_U14_*` pins
  (eight plus `MAX_MISSING`), the oracle, the miss set, the audio trace; the
  gate count is the measured `CAPTURE_DIRS` inventory after this task. Update
  `tools/pin_baseline.sha256` with the measured reason. `gameplay: publish the
  fought win` or `docs: record the U14 outcome`.

---

### Task 13: Final gate and docs

**Files:**
- Modify: `docs/superpowers/specs/2026-10-27-claim-coverage-design.md` (status),
  `docs/superpowers/plans/2026-10-27-claim-coverage-derivations.md` (§C6),
  `docs/PROGRESS.md`, `README.md`, `AGENTS.md`, `port/RE_GUIDE.md`,
  `port/spec/*.md` as the tasks require.

**Interfaces:**
- Consumes: every task's measured result.
- Produces: the phase's closing record.

- [ ] **Step 1: Run the gate twice.** `PR_DIFF_FULLSCAN=1 GATE_STRICT=1
  PR_PINS_EXACT=1 make verify` and `make audio-render`, each twice; paste the
  logs' key lines.
- [ ] **Step 2: Verify the counters.** `port_progress.py` (the measured raw /
  portable lines and class breakdown), `make diff-verify` (the measured
  counter), `make entry-triage`, `symbols.h` idempotence, the gate inventory,
  the capture manifest and `tools/pin_baseline.sha256`. No `improved:`/`re-pin`
  line without a recorded measurement.
- [ ] **Step 3: Correct the design's predictions.** Mark the design
  **Implemented and gated** (or **Partially implemented** with the named
  failures) and replace every predicted number with the measured one.
- [ ] **Step 4: Update the status docs.** `docs/PROGRESS.md` gets a paragraph
  per task; README/AGENTS/RE_GUIDE say the same measured things. The named
  gaps that remain are listed explicitly: U14 (if not authorized), the 5120
  band, audibility, the 20 terminal rows, the 8 `UNROWED`, the remaining
  `WEAKER`, the language reload/coin countdown/storage/U17/B2-B8.
- [ ] **Step 5: Commit (when authorized).** `docs: close the claim coverage
  phase`.

---

## Verification checklist (the phase's claims)

- [ ] Two consecutive full gates green under load; `audio-render` twice with the
  same peak/sha.
- [ ] The `PR_AUDIO_DUMP` labels are byte-identical across two runs of the same
  replay.
- [ ] Every `FN_` has one class; `0x62003` is not credited; the README/AGENTS
  narrative is true; the count is measured.
- [ ] 384 of the 385 span-writer slots have an E2 row, and `0x5D004` is named
  with its `RUNTIME_BASE` rule (or the table is unchanged with the measured
  residual).
- [ ] `diff-verify` reads `810/845` full (or the measured `809` with the
  `0x255CC` `0x256D4` reason), `WEAKER` is the measured set, `0` named gaps.
- [ ] The audio oracle's claimed sections/banks pass the derived-`k` anchor and
  the null (rotated copy + shuffled capture) and mutation tests; the unclaimed
  ones are named.
- [ ] The two-human KO is published with an oracle whose window covers the KO,
  or as game evidence with the replay gap named, or `.failed.N` and no pin.
- [ ] The U16 gate operand is measured, or the measurement failure is recorded;
  no fitted fix and no `port/src` change from this task.
- [ ] The KO-branch drawn match is captured and oracled with the window covering
  `0x0A`/`0x0B`, or game evidence with the gap named, or `.failed` and no pin.
  The timeout/health drawn round is not claimed.
- [ ] U14 is published only if authorized; otherwise the gap is declared.
- [ ] The capture manifest and pin baseline match; the `CAPTURE_DIRS` count is
  derived, not a literal.
- [ ] No title/attract/front-end/endgame pin moved except the phase's own
  measured `TRACE_PINS`; the `audio-render` peak/sha did not move.
