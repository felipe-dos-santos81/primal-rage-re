# Final Truth — execution plan

**Design:** `docs/superpowers/specs/2026-10-30-final-truth-design.md`.
**Status:** ready for execution; not started. Date: 2026-10-30.
**Base:** `main` at `9a149100` (tree `6815bdef`; the Live Truth merge).
**Worktree/branch:** `final-truth`; merge to `main` at the end; keep only
`main` locally and remotely unless the operator asks otherwise.

This plan closes the last real behavioural gaps and turns every remaining
named gap into either a gate-enforced claim or a terminal declaration with a
measured blocker. It does not add a portable function. It uses the
subagent-driven-development workflow: one implementer per task, one reviewer
per task, captures serialized, one final whole-branch review + one fix wave.

## 0. Preconditions and ground rules

### 0.1 Gate-0 (the Live Truth Task 13 debt)

The Live Truth Task 13 sequence (two full
`PR_DIFF_FULLSCAN=1 GATE_STRICT=1 PR_PINS_EXACT=1 make verify` runs plus
`make audio-render`) is still the outstanding gate. It is running detached
from the previous phase on the identical tree (`6815bdef`); the logs are
`/tmp/opencode/lt-gate1-final.log`, `lt-gate2-final.log`, `lt-audio1.log`,
`lt-audio2.log`, and the completion marker is `/tmp/opencode/lt-final-status`.

- If it finishes green: record both gate exit codes, the audio-render hashes
  (selmo `530a9430…` / peak 2790, title `515b7c94…` / 4090 bytes) and the
  measured counters; Gate-0 is done.
- If it fails: diagnose on `main`, fix, and re-run the full sequence; no new
  task starts on a red baseline.
- If it was killed by the environment: restart the same sequence in a
  detached session and let it finish. Do not claim the gate from a partial
  log.

### 0.2 Worktree setup

```bash
git worktree add -b final-truth ../primal-rage-final-truth main
cd ../primal-rage-final-truth
# a fresh worktree does not bring the git-ignored test fixture:
cp ../primal-rage-reverse/port/tests/ghidra_data.bin port/tests/ghidra_data.bin
```

`make re-oracle` regenerates the fixture if the copy is absent, but copying
is faster. Run all binaries from the repo root; `PR_GAME_DIR` defaults to
`data/game/C`.

**Ghidra / docs tools.** The checked-in `port/decomp/prage.c`,
`prage.calls.csv` and `prage.functions.csv` are the Ghidra 12.2 output for
the project; the design investigation used them and the raw LE image. The
live Ghidra MCP instance is currently open on the `kindle` project, not
`primal-rage`; a task that needs a fresh xref must open the
`data/primal-rage.gpr` project (or the `_tools/ghidra_proj/prage` headless
project) and record the program/project used in its report. Context7 is not
needed for the design; use it only if the audio/OPL task hits a specific
library-API question, and cite the doc in the task report.

### 0.3 Capture discipline

- One capture at a time; host load ≤ the core count; no other heavy work
  while `make gp-capture` runs; the capture preflight refuses a starved run.
- A capture that fails a session check is preserved as `.failed` and adds no
  replay, oracle, pin, miss set, manifest entry or census improvement.
- A failed route is not retried in this phase unless the task says so; the
  terminal declaration is the fallback.

### 0.4 Pin discipline

- Every new pin is measured; every new pin fails one step in the failing
  direction; every existing pin is unchanged or re-measured with a reason.
- `tools/pin_baseline.sha256` is re-pinned only with
  `python3 tools/capture_manifest.py --update-pins --reason "<measured
  reason>"`; the reason names the measured source.
- `tools/coverage_baseline.txt` is re-pinned only if the task's new driver
  actually changes HIT/NOT-HIT; the reason names the captures and addresses.
- No fitted constant, tolerance or cause. An underived cause is a terminal
  gap, not a number.

### 0.5 Agent budget

Up to 14 agents across the phase: one implementer per task, one reviewer per
task, one final whole-branch reviewer, one fix-wave implementer, one scoped
re-reviewer, and the controller. Captures are serialized; agents may read
in parallel but must not run captures or builds concurrently with a capture.

## Task 0 — Gate-0 and baseline (controller)

**Goal:** the Live Truth gate is green on the base tree, and the phase starts
from a measured baseline.

**Steps**
1. Check `/tmp/opencode/lt-final-status`, the four logs, and
   `pgrep -af 'make verify'`.
2. If running, wait; if green, record; if red, diagnose and fix.
3. Record in the phase ledger: gate exit codes, `make gate-check` (41/42,
   declared U14), `make diff-verify` (`1033/1033`, `14993/14993`, 0 named
   gaps, `825/845`, `809/845`, 8 unrowed), `port_progress.py`
   (`798 1203 66` / `759 759 100`), `capture_manifest --check-pins`
   (`cadd4ec6…`), `audio-render` hashes.
4. Create the ledger `.superpowers/sdd/2026-10-30-final-truth/progress.md`
   and the investigation summary from the design.

**Deliverable:** a ledger entry with the measured baseline; no code change.

**Abort:** a red gate that cannot be fixed stops the phase; do not start
Task 1 on a red base.

## Task 1 — `0x2CADC` boot-defaults fidelity (conditional)

**Goal:** the port draws the defaults message and models the wait/release,
or the omission is a terminal deviation with the measured frame shift.

**Why:** `config_set_defaults` (`0x2CADC`) makes ten raw calls; the port makes
five and omits `0x2F198` (draws the defaults message, stores the cursor words
`0x105F34/36`), `0x2EA78(0xB4)` (the 180-tick wait that presents frames) and
`0x2F280` (releases the cells). The `PORT` note says the omission is to keep
the frame-indexed oracles stable. The row is one of the 13 WEAKER rows and
the only non-error-path behavioural one.

**Derivation first**
1. Read `port/decomp/prage.c` `FUN_0002cadc` (ten calls), `0x2F198`,
   `0x2EA78`, `0x2F280`, and the caller of `0x2CADC`; pin the three
   arguments (string pointer, row/col, mode) from the raw bytes, not from a
   guess. Identify the message string through the game's own
   `game_string_get`/resource table, not a fitted literal.
2. Measure the wait: does `0x2EA78(0xB4)` present frames before the loop's
   `f` counter? Read `0x2EA78` and the loop entry; count the raw frames it
   presents and compare with the capture's boot frames.
3. Decide: can the port draw the message and model the wait without shifting
   the frame-indexed oracles (the wait is before `game_loop`)? If the raw's
   presents are outside the compared windows, implement them; if they shift
   the windows, the task must re-pin or abort.

**Implementation (only if the derivation reconciles)**
1. `config_set_defaults` calls the three ported functions
   (`text_draw`/`0x2F198`, `config_screen_wait`/`0x2EA78`, `text_cells_release`/
   `0x2F280`) with the derived arguments; add `PR_SEAM` only where the
   differential row needs it.
2. Remove the three nop pokes from the `0x2CADC` row in
   `tools/diff_verify.py` (`D6A_DEFAULTS`/`stub`); add the callee seams; the
   row becomes full if the callees are ported.
3. Re-measure every affected oracle (title/attract/front-end/K11/gp windows)
   by re-running the driver; re-pin only the measured values; record the
   reason. If the re-pin is broader than the phase can verify, revert the
   implementation and take the terminal branch.

**Terminal branch (if the wait cannot be modelled)**
- Revert the code; record the measured frame shift and the raw call trace in
  the derivations; the `PORT` deviation becomes a terminal claim with its
  reason; the WEAKER row stays 13. No half-fix.

**Tests/evidence**
- A unit test for the message draw (the string and cursor words) and the
  release; the `0x2CADC` row without nops; the oracle re-pins; a mutation
  proof for each new pin.
- The differential counter string and `WEAKER_CENSUS` updated if the row
  becomes full (13 → 12); record the new string.

**Abort/fallback:** the terminal branch above; no second implementation
attempt.

## Task 2 — the 5120 band

**Goal:** decide the 13-pixel band's cause and either fix it or declare its
class; no fitted cause.

**Step 1 — holder search (no capture)**
1. In `tools/gp_band.py`/`gp_compare.py`, search the existing U13 replay dump
   for any port frame that holds the capture-5120 13-pixel values exactly
   (per-pixel, per-row segment, and the control). The existing `band_pair`
   test is row-range-wide; this is the per-pixel version.
2. If a holder exists, the band is a splice/order artifact: adjust the
   splice model or the allowance and record the holder frame; no capture.
3. If no holder, proceed to Step 2.

**Step 2 — DAC-injection diagnostic (one capture)**
1. Re-derive the fade palette source from the raw: `0x29B74` → `0x13D4C` →
   `0x134C0`; pin the 24 active palette records `0xF0B00+0x814n` and the
   `0x32/0x2E/0x4D/0x65` sources. Use the design investigation's candidate
   only after re-deriving it.
2. Add the S fields: back buffers `0xE87A0`/`0xE87A4`, the 13 band bytes,
   `eff0/eff1/efffree/effn` (`0xFCCE0/4/E8`, `0x9AF3D`); read `0xE87A0` for
   the just-presented frame at the S record (the swap runs after the copy).
3. Add `gp-u13-band-dac`: U13 steps + `WIN_EXTRA`, stop `after_entry 0x15,
   k=1, n=84`, time 240, memsize 64; the 56 W writes at f=0x1825 (24 records
   × 4 sentinel groups) with the derived sentinels; the control pixel must
   turn the sentinel colour.
4. One capture. Outcomes:
   - sentinel at the 8 pixels + back buffer `0x32` → PASS-A (capture/pipeline
     present model); keep the band allowance and declare the class.
   - no sentinel + back buffer `0x32` → the present-layer class (undumped
     present/aperture residue); declare terminal with the S record.
   - back buffer Q at any band offset → PASS-B (content divergence); name the
     renderer from Q and fix it, then re-pin the band.
   - control not yellow, any race, or no S at f=0x1826 → INCONCLUSIVE; keep
     `(5120,1,5,56b900b1…)` and the declared gap; no second capture.

**Tests/evidence**
- The holder-search result (no capture); the diagnostic's S record and W
  rows; the sentinel/back-buffer outcome; the new pins or the terminal
  reason; the band allowance unchanged unless a fix is measured.

**Abort/fallback:** INCONCLUSIVE → terminal; no second capture without a new
derivation.

## Task 3 — U14 rule + two-human fought match win

**Goal:** correct the unsatisfiable `b0c` rule, publish a two-human fought
match win (no pokes), and declare U14's arcade fought win (R1) terminal.

**Step 1 — the rule**
1. Correct the docs: `0x37FF6` is the final's `0x0C→0x0D` chain
   (`0x274FC`/`0x27380`), not the ending path. Record the correction in the
   derivations and the design.
2. Split `live_death` into:
   - the match rule: no `W` on `DS_00104B0C`, a live `0<+0x5A<0x78` to
     `0x78`, no KO/death poke, no timeout, the round/match result names the
     winner; **no `b0c` clause**;
   - the final-only rule: `b0c=1` is required only on the final path
     (0x0C→0x0D→0x0F).
3. Tests: the `.failed` artifact's fought KO satisfies the KO/result clauses
   but not a win; a synthetic final row requires `b0c`; the match rule passes
   a two-human `w2/w3` win. The tests must fail before the split.

**Step 2 — the capture**
1. Scenario `gp-u14-twop-win`: clone `gp-twop-ko` through round 2; keep
   `b1f=3`, no pokes, `stop_mode=0x12`, `WIN_EXTRA`; stop after the match
   result. One capture.
2. Checker `gp_twop.py check --win` (or a new `--match-win`): two-human,
   no KO/death poke, both rounds' live-to-`0x78`, the round result and the
   match result (`w2=2`/`w3=0` or the measured pairing), no timeout, an X
   record.
3. If it passes: `gp-u14-twop-win-oracle` in `make verify` and
   `CAPTURE_DIRS`, with frame/trace/moves/min-reach/max-start/max-missing
   pins, capture sha + frame count, a `k_miss_gp_u14_twop_win` miss set, and
   the manifest/pin baseline. The claim is a **two-human fought match win**,
   not an arcade win.
4. If it fails any session check, any `W` on a KO/death field, no live
   `0x78` in both rounds, or no X: preserve `.failed`, no oracle/pins; keep
   U14 R1 declared.

**Step 3 — terminal R1**
- Record the measured damage race (round 1 AI 10 hits/120 vs script 2/25;
  round 2 AI 9/120 vs script 8/92), the three historical runs with no won
  arcade round, the `b0c` final chain, and the Sauron reaction data; declare
  the arcade fought win terminal. Do not call the two-human win an arcade
  win.

**Abort/fallback:** the failed-capture branch; no second capture.

## Task 4 — the taken-continue branch (`0x0E` → `0x2791C`)

**Goal:** exhibit and gate the taken-continue path, the one branch the
continue capture deliberately did not reach.

**Steps**
1. Scenario `gp-u29-continue-taken`: clone `gp-u28-continue`'s route (U10's
   route through match 1 + the incumbent poke) and add a `p1.start` at the
   prompt (count < 14; the measured prompt arm is `cnt < 0x0E`, `0x27B59`).
   Stop at mode `0x0C`/`0x06` after the take. No new poke; boot `cred=4`.
2. One capture. The session must reach the prompt, the start, the credit
   spend and the restart.
3. Checker `gp_continue.py check --taken`: `w10d=0`, `cred 4→3`, `b1f=1`,
   mode `0x0C`, the prompt/countdown cells cleared, the round restart;
   reject a `W` credit write or a poke.
4. `gp-u29-continue-taken-oracle` in `make verify`/`CAPTURE_DIRS` with the
   standard pins, capture sha + frames, miss set, manifest/baseline.
5. Coverage: if the capture driver is added to the coverage run, re-pin
   `0x2791C` NOT-HIT→HIT (98→97) with the measured hit dump; if the coverage
   driver is not extended, the register says the branch is gate-enforced but
   not a census improvement.

**Abort/fallback:** no credit/start/mode 0x0C or any session failure →
`.failed`, no pins; the branch stays declared.

## Task 5 — 0x25 bowling: derivation + one staggered capture

**Goal:** settle the count/camera residue with one derived, staggered route;
publish the mode or declare it terminal.

**Step 1 — the derivation (no capture)**
1. Record the `+0x53` machine: `0x455A0` writes `+0x53=7` and `+0x52=9`;
   `0x263DE` checks slot 0's `+0x53==0`; `0x3531C` case 7 → `0x367DC` when
   char 4, `+0x5F∈{0x21,0x22}`, `+0x88>90`; `0x367DC` clears `+0x52/+0x53`
   and sets `+0x5F=0xFF`. The clear is at the ~91st reaction frame, so
   `b892>=6` must hold at that frame. The one-sided route is impossible
   (the increment reads the other slot's `+0x5F==0x22`; the hook is installed
   only by that slot's own callback).
2. State the residue: the corrected capture's last sample is 5 (the first
   attempt reached 2); the count is one short; the camera `0xF0AF0` is the
   only unmeasured gate. A 4–5 attempt
   staggered route (150-frame spacing) is the new route.

**Step 2 — the capture (one)**
1. Add `s0_53` (`0x107803`) and `camx` (`0xF0AF0`) to `SNAP_FIELDS` and the
   port T record (append-only).
2. Scenario `gp-bowling-2`: the `gp-bowling-fix` mirror-ARMADON route with
   4–5 attempts, 150 frames apart; stop on mode 0x25 or after the fifth
   attempt; no poke.
3. Checker `gp_bowling.py check --staggered`: callback edges per side (the
   plan's "3–5" estimate is superseded by the measurement: both prior captures
   show exactly two per attempt per side, so the staggered checker enforces
   `3..10` for the route's five attempts — Final truth Task 5, record §F5.7),
   visible counter growth to ≥6 or the permitted same-iteration 6→5, the
   `s0_53` at the entry check (the plan's 0 is the `0x263DE` gate's read; the
   entry's own `0x38154` write stores 0 or `0x0C`, and the measured capture
   holds `0x0C` — record §F5.5), `camx` within ±0x3300, the entry stores
   (`b892=0`, `b1f=3`, `b1d∉{2,3}`, `c0=c1=4`, `b25=0`, `b8c4 0→1`,
   `b8a6=0xB4`, `b8bd=b415=0`, `b8c0=1`), no `W`.
4. One capture. If it opens 0x25: `gp-bowling-oracle`, pins, capture path,
   `k_miss_gp_bowling`, census `0x4E11C` HIT if the coverage driver runs.
   If not: `.failed`, no oracle/pins; terminal declaration with the measured
   count and camera (or INCONCLUSIVE if the camera was not sampled).

**Abort/fallback:** no 0x25 after five attempts → terminal with the
`.failed`; no second capture.

## Task 6 — 0x21 volleyball: static derivation + conditional capture

**Goal:** decide whether the eight-trample chain is structurally unreachable;
if a window exists, one two-human capture; otherwise terminal.

**Step 1 — the derivation (no capture)**
1. Derive the `+0x1C` re-hit question: the writers of the actor's `+0x1C`
   (the grab arms `0x4B8E8`/`0x4DA19` and the stream interpreter), the case-6
   sum test (`0x49F47`), and whether any animation stream restores `+0x1C`
   after a counted trample. The candidate blocker: `0x4B470` never writes
   `+0x1C`; the case-6 landing at `+0x36==0` leaves bit 7 off; only the
   type-8 release tramples without counting → count can only reach 1 per
   spawn.
2. If the count cannot exceed 1: terminal declaration with the derivation,
   the `0x4B470`/`0x49F47` addresses, and the census rows; no capture.
3. If a window exists: state the exact sequence and the stage RNG; proceed to
   Step 2.

**Step 2 — the conditional capture (one)**
1. Add S fields: `0x104AFC` (stage word), `0x104ABC`, `0x104AD8`, and the
   first effects entry's `+0x1F` (and `+0x1C`).
2. Scenario `gp-u21-volley`: the `gp-twop` join, stage selection until the
   stage word is 0 (within the one capture; if the match's stage is not 0,
   abort), the derived trample sequence; stop on mode 0x21.
3. If it opens: `gp-u21-oracle`, pins, capture path, miss set, census
   `0x4BD98` HIT if the coverage driver runs. If not: `.failed`, no pins;
   terminal with the derivation and the attempted route.
4. No poked 1P; a poked two-human needs an explicit operator reversal, not
   this phase.

**Abort/fallback:** stage not 0, `b1d` 2/3, no `+0x1F>7`, or any session
failure → `.failed`; terminal.

**Measured outcome (Final truth Task 6, record §F6; fix round 1 corrects the
flight arithmetic and the evidence).** The Step-1 derivation refutes the
premise's blocker: the count is **not** limited to 1 (the tumble's flight is 71
motion steps — the same-pass case-6 decrement makes the first step add `0x230`,
so `h(n) = 568n - 8n²` — the latch returns on the 35 passes with `vy < 0`, and
`0x4B470` re-launches on every hit), so the chain is mechanically reachable —
but no frame-exact route is derivable from the checked-in bytes (the strike
frames are the fighters' attack-box data; the two-human stage is the design's
1-in-7 `rng(7)` draw at `0x2587C`, measured in `gp-twop`; the crowd lie is
measured in `gp-idle-loss` and the demo, and all four measured `0x4B470` calls
are first hits on distinct entries). With one capture and `.failed` publishing
nothing, the phase spent none; `0x21` stays declared and the mode's pass
`0x4BF18` and tail `0x4BD98` stay `NOT-HIT`. A port-side frame-keyed route
search is the new task that would enable the capture.

## Task 7 — audio truth

**Goal:** re-derive the one unproven audio exclusion, close the bank residue
with a no-capture table, settle the `0xE4` bank with an existing artifact,
and declare rendered audibility terminal.

**7a — the carrier TL re-derivation**
1. Read `audio.md` §F.5 and the raw `0x68C8B`/`0x688E0` latch; compare with
   the port's latch (`port/src/platform/audio/sequencer.c:206-216,283-291,
   437-446`).
2. Add a unit test that feeds the raw's CC7/volume arrival sequence and
   asserts the port's TL latch value; if it differs, fix the latch; if it
   matches, narrow/remove the exclusion with the measured reason. No fitted
   threshold.

**7b — the no-capture bank residue table**
1. In `tools/audio_compare.py`, add a `TRACE_RESIDUE` table for the 10
   requested-unclaimed banks: bank → first-diff tick and cause class
   (`voice-rotation` vs `tail-lag`), measured from the existing 27 reports.
   Pin it in its own `PIN-BASELINE` region; the region guard and the
   mutation test (move one first-diff tick one earlier) fail.
2. The 4 never-requested (superseded by the measured outcome, record §F7.2:
   the pair is `grave+0x0A8C` + `grave+0x05E4`, requested-but-unattributable;
   `grave+0x08` is the claimed 1,200-tick section; `sound+0x7714` is gated by
   the shared `+0x42`/`+0x43` pose behind three reaction callbacks, and
   `konsd+0x1CCB0` by the `0x24508` stream). Record the evidence.
3. Replay the existing `gp-u24-u16-state-t9-r5` artifact with `PR_AUDIO_DUMP`
   (no new capture); if the `0xE4` bank fires, add the section/pin; otherwise
   declare it.

**7c — rendered audibility terminal**
1. Add the terminal declaration: Opal 2.0.3 vs DOSBox-X's OPL, 49,716 vs
   48,000 Hz, the invented mixer, the 0.8 % span and the label-shift flake;
   exact PCM equality is not a game claim. The game-owned claim remains the
   register stream (74 prefixes/29,656 ticks).
2. Optional, only if cheap: the narrow **register-to-render** test — a WAV
   sink at `host_audio_submit` and an exact comparison of the game path's
   OPL stem to `opl_render` of `PR_AUDIO_DUMP`; call it determinism, never
   audibility.

**Abort/fallback:** if the TL re-derivation needs a raw store the port does
not have, record the gap; do not fit a threshold. No new audio capture.

## Task 8 — the terminal claim register

**Goal:** one authoritative table; every remaining gap has a measured status;
docs agree.

**Steps**
1. Create `docs/superpowers/specs/2026-10-30-final-claims.md` with columns:
   gap | status (closed/gated/terminal) | evidence (address/file:line or
   capture/frame) | terminal reason | residual claim.
2. Include: U14 R1; 0x21; 0x25; `0x0E` taken-continue; 5120; rendered
   audibility; the 12/10/2/2 audio banks (record §F7.2: 12 claimed + 10
   residue + 2 requested-but-unattributable + 2 never requested); U17; U16
   R2/finisher; the differential
   20/13/8; the coverage 4/98; storage; movie audio; the 53/37/315
   classification; the oracle window/model limits; the `0x2CADC` deviation
   if aborted.
3. Update `AGENTS.md`, `docs/PROGRESS.md`, `README.md`, `port/RE_GUIDE.md`
   to point at the register; the README title percentage stays 66% (the raw
   figure; the portable figure is 100%). Also reconcile the pre-existing
   `0x21` passages that Task 6 refuted: `docs/superpowers/specs/2026-10-29-live-truth-design.md`
   §2.6 and `docs/superpowers/plans/2026-10-29-live-truth-derivations.md` §L9.3
   (both carry superseded notes now — the counter/landing blocker is refuted,
   the stage is a 1-in-7 retry, not a blocker), so the register never repeats
   the refuted blockers.
4. Add `tools/tests/test_final_claims.py` (or extend
   `test_check_docs_claims.py`): every "named gap"/"declared"/"INCONCLUSIVE"
   row in AGENTS is in the register, and every terminal register row names an
   address/file:line or a `.failed`/capture; a missing or stale row fails.
   Add the test module to the Makefile `verify` unittest list; mutation-proof
   the test.
5. Fold U16 R2 (the measured `y(0x1531)=0x0EAE < 0x1400`, strict) into the
   register; the finisher gap itself stays declared.

**Deliverable:** the register + docs + test; no new claim beyond Tasks 1–7.

## Task 9 — final gate and merge

**Steps**
1. `make gate-check` (the expected count: 41/42 plus the new captures if
   published; U14's absence stays if R1 is terminal).
2. `make diff-verify` (`PR_DIFF_FULLSCAN=1`), the counter string; if Task 1
   removed the `0x2CADC` WEAKER row, update `WEAKER_CENSUS` and the string
   (13 → 12); every other count unchanged.
3. `make entry-triage` (no new portable function; unchanged).
4. `symbols.h` idempotence (`python3 tools/gen_symbols.py` + diff).
5. `python3 tools/port_progress.py` (`798 1203 66` / `759 759 100`).
6. `python3 tools/capture_manifest.py --check-pins`; the new digest and
   reason; `PR_PINS_EXACT=1` on every new oracle.
7. The coverage census if the new drivers are in it; re-pin only the
   measured improvements; otherwise leave the baseline and say so.
8. Two full `PR_DIFF_FULLSCAN=1 GATE_STRICT=1 PR_PINS_EXACT=1 make verify`
   runs, sequential, standalone, on the final tree; both exit 0.
9. `make audio-render` twice; byte-identical; the selmo peak/sha and title
   sha; record.
10. Final whole-branch review (one reviewer, the whole diff), one fix wave
    if needed, one scoped re-review.
11. Merge `final-truth` into `main` with a merge commit; push `main`; delete
    the branch locally and remotely; keep only `main` (unless the operator
    asks otherwise). Record the merge commit and the tree.

**Abort:** any red step is fixed and the two full gates re-run; no pin is
weakened to pass.

## Task 99 — Appendix: task/agent table

| Task | Implementer | Reviewer | Capture? | Port code? |
|---|---|---|---|---|
| 0 Gate-0 | controller | — | no | no |
| 1 `0x2CADC` | agent 1 | agent 2 | no | conditional |
| 2 5120 | agent 3 | agent 4 | one diagnostic | conditional |
| 3 U14 | agent 5 | agent 6 | one | no |
| 4 `0x0E` taken | agent 7 | agent 8 | one | no |
| 5 0x25 | agent 9 | agent 10 | one | no (test fields only) |
| 6 0x21 | agent 11 | agent 12 | conditional | no (test fields only) |
| 7 audio | agent 13 | agent 14 | no | conditional (TL) |
| 8 register | agent 2 or 4 | final reviewer | no | no |
| 9 gate | controller | final reviewer | no | no |

The final whole-branch review gets the package from
`superpowers:subagent-driven-development` and reads the ledger's parked
minors. One fix wave, one scoped re-review.

## 12. Definition of done

1. Gate-0 and the final two gates + audio-render are green on the final tree;
   the exit codes and hashes are recorded.
2. The terminal claim register is the single source of truth; every remaining
   gap is closed or terminal with a measured blocker; the docs test passes.
3. Every new pin is mutation-proven; every existing pin is unchanged or
   re-measured with a reason; no fitted constant.
4. The only port-code changes are the conditional `0x2CADC` fix and the
   conditional audio TL fix; no new portable function; `759/759` portable.
5. Every failed capture is `.failed` with no claim; every published capture
   has an oracle, pins, a miss set and a manifest entry.
6. `main` is the only branch locally and remotely; the merge commit and the
   tree are recorded.
