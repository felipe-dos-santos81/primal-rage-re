# Gameplay U8 — Other Modes (the START MENU rows and the attract start) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Capture the pinned original walking from mode 3 into every START MENU row other than LEFT PLAYER ARCADE (rows 1–6) and into a match through the attract start (P1's F1 in mode 3), one short capture each, ending 300 frames into the row's fight (ENDURANCE: 300 frames into its team select); prove each capture reached its row from the raw's expectations before pinning anything; replay each in the port; pin a frame and a trace ratchet per scenario in `make verify` (skip when the capture is absent).

**Architecture:** A new evidence checker `tools/gp_modes.py` holds the raw-derived expectation per scenario (the setter's mode, `DS_00104B1D`, the divert mask `DS_00104B1F`, the credits spent, the mode reached). Seven `gp-u8-*` scenarios in `tools/gp_session.py` drive the walk; the attract start needs a backward-compatible "pad arm" (no Enter) in `gp_session.port_script`, `gp_capture.run_checks` and the `PR_GP_DUMP` driver. New targets `gp-modes-oracle`/`gp-modes-one` loop over the pinned scenarios with `GP_MODES_<ID>_*` values, reusing `gp-replay` and `gp_compare.py` unchanged; per-scenario `fn_resolve` miss sets go into a scenario table in `test_platform.c`.

**Tech Stack:** Python 3 stdlib (`unittest`), the U1–U4 gameplay tools, DOSBox-X 2026.08.31, the port's C test driver, GNU make.

**Spec:** `docs/superpowers/specs/2026-09-30-reverse-completion-design.md` §4 (track G, U8), §6 (exit criteria G), §7; `docs/superpowers/specs/2026-09-30-gameplay-ground-truth-design.md` §2, §3.2–§3.4, §3.6, §4.1–§4.3, §5, §7 Q4/Q5/Q7. **Derivation record:** `docs/superpowers/plans/2026-10-01-gameplay-u8-derivations.md` (§U8.0–§U8.9 the planner's evidence; each task appends its section from §U8.10 on). U4's record `docs/superpowers/plans/2026-09-30-gameplay-ground-truth-derivations.md` §G.7, §G.16–§G.24 is the precedent for every capture step.

---

## Decisions needed from the user

**Decided by the user on 2026-10-01:** all recommendations accepted — D1 storage for the seven captures (about 475 MB total) approved; D2 the attract start (P1's F1 in mode 3) is included, with the backward-compatible pad-arm extension; D3 the ENDURANCE capture ends in its team select and the endurance fight is a named gap handed to U5.

1. **D1 — Storage for seven captures (gates Tasks 6a–6g).** Record §U8.6 estimates (from `gp-idle-loss`'s per-segment sizes; a capture's frames run to DOSBox-X's time limit, not to the scenario end): rows 1–4 and 6 ≈ 75 MB each, ENDURANCE ≈ 24 MB, the attract start ≈ 73 MB: **≈ 475 MB** in the git-ignored `data/k11-captures/gp-u8-*` (U4's idle run alone is 376 MB). A port dump per replay (52–89 MB) lives in `/tmp` only while it is compared. *Recommendation: yes.* Cost of the wrong answer: refusing leaves U8 with tools and no oracle (the spec's G exit criterion for U8 is unmet); accepting costs disk only.
2. **D2 — The attract start (Tasks 4, 5, 6g).** It is the spec's "second way into a match" (gameplay spec §3.4), and the raw shows it skips the config decode `0x2D974(0x29)` that every START MENU row runs (record §U8.3), so its fight can differ from LEFT PLAYER ARCADE's. It needs a backward-compatible "pad arm" in three shared files (`tools/gp_session.py`, `tools/gp_capture.py`, `port/tests/test_game.c`). *Recommendation: include.* Cost: if deferred, drop Tasks 4, 5 and 6g and the `gp-u8-attract-start` row of `ROWS`; the second way into a match stays unverified (a named gap). If included and another unit changes the same driver lines first, the merge needs one manual resolution (Shared-file touch points).
3. **D3 — ENDURANCE stops in its team select.** The team select `0x44798` has no time-out (record §U8.4: its only exit `0x4482A` needs both sides to pick four classes), so an idle walk never reaches the endurance fight. *Recommendation: end the scenario 300 frames into the team select and name the endurance fight as a gap for U5 (the character-select unit, which owns pick inputs).* Cost of the alternative (script eight picks here): deriving the `DS_001088E0/E2` command-word bits the team select reads, which U5 is planned to derive; a wrong pick script yields a capture that stalls in the select (the check below catches it, the capture is wasted).

## Re-baseline (2026-10-01, main 8eaf25a)

Written against `e9271df`; re-based onto `8eaf25a` (U5 `b09b9e6` and U6a merged). The user-approved decisions above are unchanged. Changes (task/step: old → new, source):

1. **Task 3 Step 4 (the reference run and its mutation):** `GP_MODES_REF_MIN_FIRST=203`/`TRACE_MIN_FIRST=2088`, mutation `MIN_FIRST=204` → measured first (a `gp-replay … --end 2325` plus `gp_compare --report` block), then `1070`/`2326` (on `8eaf25a`) with two mutations, `MIN_FIRST=1071` (`frames: FAIL: first unexplained 1070 < ratchet N 1071`) and `TRACE_MIN_FIRST=2327` (`trace: FAIL: N 2327 > end 2326: N is unreachable`). Why: after U5's divergence-1 fix and U6a's ports the cut replay has no divergence, so its first unexplained frame is capture 1070, the frame after the port's last (port 817, `f = 0x915`), and the trace has no difference through 2325; by `gp_compare.ratchet` (`tools/gp_compare.py:258`) N fails iff N > first and, with no difference, F fails iff F > end, so N = 204 passed (`ratchet N 204 ok (improved: raise N)`, rc 0 — run on `8eaf25a`) and only N = j + 1 and F = end + 1 can fail. All values run on `8eaf25a` (record §U8.7 "re-baseline").
2. **Task 6 Step 4 (the miss-set table):** the anchors `int idle_loss = 0, cut = 0;` and `if (!fnm_known(…, frontend, idle_loss)) {` → `int idle_loss = 0, charsel = 0, cut = 0;` and `…, frontend, idle_loss, charsel)) {` (`port/tests/test_platform.c:180`, `:195`), stated as "whatever flags and parameters the base has" (U6b `moves`, U11 `keys_fight`, U7 `twop` may come first). Re-verified in a scratch build.
3. **Task 6 Step 3 (miss classification):** `0x23208`, `0x3A588`, `0x3640C` listed as unported → ported (and `0x37DCC`) by U6a (`port/src/game/actors.c` `fn_register`s, record gameplay-u6 §U6.21); `k_miss_gp_idle_loss` now holds only `0x29D60` and `0x5D812 frontend_mode_1b_step` (`test_platform.c:120–123`). The previews were re-run on `8eaf25a` (record §U8.3): row 1 newly misses `0x15510` (`anim_indirect`).
4. **Task 6 Step 5 (triage):** "U4's divergence 1 (capture 203) or 2 (`0x23208`)" → the end of the port's replay, `gp-idle-loss`'s remaining gap (capture 2064, the round-1 KO scan-out, record §U6.21), or new; divergences 1, 2 and 2b do not occur on this base. Plus the exact-pin rule for `F`.
5. **Task 0 Step 2–3 and the gate:** expected oracle lines → `gp-idle-loss` 2064/8320 (exact), `gp-u5-charsel` 516/1513, `diff-verify: 6/6 functions VERIFIED; 7/7 mutants detected` (run on `8eaf25a`), recorded as the base's own lines; the full `make verify` gate runs at Tasks 0, 7 and before merge, a per-task gate (own tests + `gp-modes-oracle` + `diff-verify`) elsewhere (the user's speed-up ruling); `port_progress.py` compared against Task 0's lines (`771 1203 64` / `731 731 100` on `8eaf25a`; U6b/U7 may change them).
6. **Line anchors:** `gp_session.py` Task 2 insertion "after line 126 `gp-idle-loss-run2`" → before `def expand` (U5's `gp-u5-charsel` block now sits there, lines 140–148); `port_script` 216 → 238; `test_game.c` 12159/12164–12196/12363–12364 → 12192/12197–12229/12396–12397; `test_platform.c` `FNM_N` 161 → 166, driver 170–200 → 176–201; Makefile `gp-report` 462 → 494–496; the `verify` line goes after `gp-charsel-oracle` (line 546), U5's line after `gp-oracle`. Every replaced text in Tasks 4–5 was checked to exist verbatim on `8eaf25a`.
7. **Task 5 Step 3:** the long pad-arm preview on `8eaf25a` misses only `0x29D60` and `0x5D812 frontend_mode_1b_step` (no longer `0x23208`, `0x3A588`); `3 != 2` and `3 != 39` unchanged.
8. **Task 7 Step 1 and record §U8.9:** ENDURANCE's fight "(D3, U5)" → owner unassigned: U5 merged and hands the team pass and the ENDURANCE fight back to U8 (record gameplay-u5 §C5.19). D3 itself (stop in the team select) is unchanged.
9. **Execution notes:** "run Task 6 after U5 when possible" → U5/U6a merged; U8 runs after U6b and U7 (dependencies listed).

**Warnings for the implementer.**
- `GP_IDLE_LOSS_TRACE_MIN_FIRST = 8320` is an **exact** pin (no traced difference through `f = 0x207F`; `gp_compare` fails 8321 as unreachable). U8 does not change the `gp-idle-loss` scenario or its replay; if anything in U8 does change that replay's script or end, re-measure and re-pin it, and never set the frame N (2064) below what the run measures. The same holds for every U8 `TRACE_MIN_FIRST` pinned as the replay end + 1 (Task 6 Step 5).
- Every value Task 6 pins (N, F, window start, miss set) and Task 3 Step 4's `REF_N`/`REF_F` are measured on the base U8 actually runs on, after U6b (snapshot bytes: `SNAP_FIELDS` and the `T` line gain `r0 r1 c0 c1 s0_43`; `TRACE_FIELDS` unchanged; U6b's moves claim in `gp_compare.py` is opt-in, so `gp-modes-one` needs no new argument) and U7; the `8eaf25a` numbers here are references only.
- `fnm_known`'s parameter list is the base's: U8 adds no parameter, only the scenario-table clause beside the call.
- Not resolved here: the ENDURANCE/team-pass owner (item 8); the cause of the new row-1 miss `0x15510` (a preview, classified in Task 6a Step 3 if the capture's replay shows it).

## Global Constraints

- AGENTS.md: "**Never ship a fitted constant.** Every value is derived from the raw bytes or a capture, with the address that proves it. A value that cannot be pinned is a **named gap with its evidence** — never a plausible-looking number." and "**On any plan-vs-raw conflict the raw wins.** Record the correction and the address, in the derivation record and the report."
- Gameplay spec §5: "Harness values (`HOLD_FRAMES`, the 150-frame gaps, `time_limit`, the `[p − 2, p + 64)` search order, `GP_LOOP_SLACK`) are named as harness values with their source, never presented as game values." U8's harness values: the 60-frame gaps, the 4-frame hold, the 300-frame tail, each `time_limit` and its 6 s margin (record §U8.6).
- AGENTS.md: "The byte-exact oracle lines are the regression gate and must not move; run `make verify` after any change that can affect rendering, timing or RNG." Common brief: the 45 oracle lines (`grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|== demo-fight)' <log>`) must equal `.superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt`; the `make audio-render` WAV must be byte-identical to `before-t2.wav` (in the same directory); `python3 tools/port_progress.py` prints the same two lines as at Task 0 (U8 ports no function; on main `8eaf25a` they are `771 1203 64` and `731 731 100`, and U6b/U7 merge before U8 and may change them: Task 0 records the base's lines and every later gate compares against those).
- AGENTS.md: "Captures are git-ignored. `data/` is git-ignored and read-only — never write to it." U8 writes only `data/k11-captures/gp-u8-*` through `make gp-capture` (`gp_capture.guard_gp`). Gameplay spec §4.3 / brief: a gp capture skips in `make verify` when absent, even under `PR_ORACLE_REQUIRED`.
- AGENTS.md: "Only `CHECK(cond,msg)` and `CHECK_EQ_INT(a,b)`." "**Assertions must be able to fail.** Seed sentinels that differ from the post-conditions, prove a new assertion fails under a mutation of the code it tests, and never assert an unseeded BSS-zero." Python tests are `unittest` in `tools/tests/` with `sys.path.insert(0, ROOT/tools)`.
- AGENTS.md: "Every env-gated driver and `--check` run arms `fn_resolve`'s miss log and must record exactly the pinned known-set … A port that makes a driver reach a new unregistered code pointer fails it: register the target or pin the miss with its evidence."
- AGENTS.md: "**`game_init()` may run only once per process**"; the `PR_GP_DUMP` driver runs alone (`TEST_DRIVERS`).
- AGENTS.md: "Commit style: `<area>: <what changed>`. **Never `git add -A`**." Trailer: `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Common brief (parallel units): Makefile variables prefixed `GP_MODES_`; scenarios named `gp-u8-…`; one new line in `verify`; `docs/PROGRESS.md` append-only; a shared tool change is backward compatible, minimal, tested, and listed under Shared-file touch points.
- The claim is narrow (AGENTS.md, gameplay oracle): "no content-bearing capture frame from the window start up to N is unexplained and the traced fields agree below F … It cannot detect a port that under-renders, and **the order of the port's frames and that every port frame appears are not claimed**." Every U8 ratchet inherits it.

## Review Focus

1. **A capture that reached another row, or none.** A single mis-sampled press moves the cursor one row too few or too many. Pinned by `tools/gp_modes.py check` (Task 1; `test_a_wrong_row_fails`, `test_divert_and_credits_are_checked`), which `gp-modes-one` runs before every comparison and Task 6 Step 2 runs before any pin.
2. **Row expectations mis-transcribed from the raw.** Pinned by `TestRowsFromTheRaw.test_setters_and_divert_arguments` (Task 1), which reads the setters' and handlers' bytes from `PRAGE.EXE`; mutation M1 proves it fails.
3. **The pad arm breaking the Enter arm** (U4's and `gp-pads`' replays must not change). Pinned by `test_an_enter_scenario_still_needs_mode_0x27` (Task 4), the unchanged `test_gp_session`/`test_gp_capture` suites, the `gp-pads` replay's `all checks passed` and the driver mutation (Task 5 Step 4).
4. **A ratchet that cannot fail or belongs to another capture.** Task 6 Step 7 raises each N and F by one and damages a port frame (each must FAIL), and the `GP_MODES_<ID>_CAPTURE_SHA256` pin makes a re-capture fail rather than pass on stale values.
5. **A miss set keyed to the wrong scenario or too wide.** The scenario table matches the exact scenario name; Task 6a Step 4 proves that dropping one pinned pair fails the driver and that a scenario without a row may miss only the base pair.

## Where to run

Worktree `.worktrees/gameplay-u8`, branch `gameplay-u8` off `main`. One-time setup from the main checkout `/Users/felipe.dos.santos/code/mine/primal-rage-reverse`:

```bash
git worktree add .worktrees/gameplay-u8 -b gameplay-u8 main
cd .worktrees/gameplay-u8
ln -s ../../data data
ln -s ../../.superpowers .superpowers
cp ../../port/tests/ghidra_data.bin ../../port/tests/title_screen_ref.ppm port/tests/
mkdir -p /tmp/gameplay-u8
```

Every command below runs from the worktree root with `S=/tmp/gameplay-u8`. The pinned exe directory is `TITLE_PIN_DIR=/tmp/pr_u8_pin`. Every `make verify` uses the parallel-safe overrides:

```bash
V="SMK_DUMP=/tmp/pr_u8_smk TITLE_DUMP=/tmp/pr_u8_title ATTRACT_DUMP=/tmp/pr_u8_att FRONTEND_DUMP=/tmp/pr_u8_fe TITLE_PIN_DIR=/tmp/pr_u8_pin AUDIO_WAV=/tmp/pr_u8.wav K11_DUMP=/tmp/pr_u8_k11 GP_DUMP=/tmp/pr_u8_gp DIFF_IMAGE=/tmp/pr_u8_diffimg DIFF_TABLE=/tmp/pr_u8_diff.md"
```

**The gate** (the full gate: Task 0, Task 7 and before merge, per the user's 2026-10-01 speed-up ruling; `<n>` the task): 

```bash
make verify $V > $S/t<n>_verify.txt 2>&1; echo "verify-exit=$?"
grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|== demo-fight)' $S/t<n>_verify.txt | diff - .superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt && echo ORACLES-EQUAL
make audio-render AUDIO_WAV=/tmp/pr_u8.wav > /dev/null && cmp /tmp/pr_u8.wav .superpowers/sdd/2026-09-29-k7-k12/scratch/before-t2.wav && echo WAV-IDENTICAL
python3 tools/port_progress.py
```

Expected: `verify-exit=0`, `ORACLES-EQUAL`, `WAV-IDENTICAL`, and the two `port_progress.py` lines Task 0 recorded (`771 1203 64` / `731 731 100` on `8eaf25a`).

**The per-task gate** (Tasks 1–6g, the same ruling: the task's own tests + its oracle + diff-verify): the task's test commands as written in its steps; `python3 -m unittest tools.tests.test_gp_modes tools.tests.test_gp_session tools.tests.test_gp_capture tools.tests.test_gp_compare` (from Task 1 on); `make gp-modes-oracle GP_DUMP=$S/gpo` (from Task 3 on; Tasks 5 and 6a–6g also `make gp-replay scenario=gp-pads GP_OPTIONAL=1 GP_DUMP=$S/gpr` and `make gp-oracle GP_DUMP=$S/gpi`, the Enter regression); `make diff-verify DIFF_IMAGE=/tmp/pr_u8_diffimg DIFF_TABLE=/tmp/pr_u8_diff.md` (on `8eaf25a`: `diff-verify: 6/6 functions VERIFIED; 7/7 mutants detected`; U6b raises both, so expect Task 0's line); `python3 tools/port_progress.py` equal to Task 0's. Each must exit 0. Where a step below says "gate (`<n>` = …)" for Tasks 1–6g, it means this per-task gate.

## Shared-file touch points

All additive; each region is named so the controller can sequence merges with U5, U6, U7, U11.

| file | region | what U8 adds |
|---|---|---|
| `tools/gp_session.py` | immediately before `def expand(action):`, after the last `SCENARIOS[…]` block (on `8eaf25a`: U5's `SCENARIOS['gp-u5-charsel'] = …`, lines 140–148; `def expand` at line 151) | `_u8_menu()` and six `SCENARIOS['gp-u8-…']` entries (Task 2); `SCENARIOS['gp-u8-attract-start']` (Task 4) |
| `tools/gp_session.py` | before `def port_script` (line 238 on `8eaf25a`); inside `port_script` (the `p27` lookup, the `--end` guard, the header lines) | `pad_arm()`; `port_script` takes the arm from `pad_arm` when the scenario has `arm='pad'` and writes `arm pad`; unchanged output for every other scenario (Task 4) |
| `tools/gp_capture.py` | `run_checks` (lines 144–153) | the arm label/predicate for `arm='pad'` (`mode left 3 after the pad arm`); unchanged for Enter scenarios (Task 4) |
| `port/tests/test_game.c` | the gp driver statics (line 12192 on `8eaf25a`), `gp_parse` (12197–12229), the arm checks in `test_gp_replay` (12396–12397) | `gp_arm_pad`, the `arm pad` line, the pad-arm parse and `mode_after` expectation (Task 5) |
| `port/tests/test_platform.c` | after `#define FNM_N` (line 166 on `8eaf25a`); `test_fn_misslog_driver` (176–201) | `fnm_scen`, `k_miss_gp_scen[]` keyed by exact scenario name, one `k_miss_gp_u8_*[]` per scenario (Task 6a introduces it; 6b–6g add rows). U8 leaves `fnm_known` and its parameter list untouched and adds its own test beside the call. On `8eaf25a` `fnm_known` has five parameters (`addr, ctx, frontend, idle_loss, charsel`: U5 merged `k_miss_gp_charsel` and the `charsel` flag; U5 and U6a removed `0x3640C`, `0x23208` and `0x3A588` from `k_miss_gp_idle_loss`, which now holds only `0x29D60` and `0x5D812 frontend_mode_1b_step`). U6b (`moves`), U11 (`keys_fight`) and U7 (`twop`, in the driver) may add flags before U8 runs: keep every unit's tables and flags when merging. Because the driver's miss count is exact, a later merge that ports a function a U8 replay misses fails that U8 scenario until its set is re-measured (Task 6 Steps 3–4) — never hand-edit the numbers. |
| `Makefile` | after the `gp-report` recipe (lines 494–496 on `8eaf25a`); in `verify` after the last gameplay-oracle line (on `8eaf25a`: `@$(MAKE) --no-print-directory gp-charsel-oracle`, line 546) | `GP_MODES_*` variables, its own `.PHONY: gp-modes-oracle gp-modes-one` line (the shared `.PHONY` list is not edited), the two targets (Task 3); one `verify` line (Task 3); the pinned `GP_MODES_<ID>_*` and `GP_MODES_SCENARIOS` entries (Tasks 6a–6g). Other units add their own `verify` line at the same place: a one-line merge conflict to resolve by keeping both. |
| `docs/PROGRESS.md` | end of file | one paragraph (Task 7) |
| `AGENTS.md` | the gameplay-oracle paragraph | one sentence naming `make gp-modes-oracle` (Task 7) |
| new files | — | `tools/gp_modes.py`, `tools/tests/test_gp_modes.py` |

---

### Task 0: Worktree, baseline and record header

**Files:**
- Modify: `docs/superpowers/plans/2026-10-01-gameplay-u8-derivations.md` (append §U8.10)

- [ ] **Step 1: Setup** — run the "Where to run" block; `make build`.
- [ ] **Step 2: Baseline gate** — run the full gate with `<n>` = 0. Expected: `verify-exit=0`, `ORACLES-EQUAL`, `WAV-IDENTICAL`, the two `port_progress.py` lines (`771 1203 64`, `731 731 100` on `8eaf25a`); the `gp_compare: gp-idle-loss` and `gp_compare: gp-u5-charsel` ratchet lines `ok` (the captures are in the main checkout's `data/`). On `8eaf25a` they read `first unexplained 2064, ratchet N 2064 ok`, `0 differing through 8319; ratchet N 8320 ok` (Makefile `GP_IDLE_LOSS_*`, record gameplay-u6 §U6.21), `first unexplained 516, ratchet N 516 ok`, `0 differing through 1512; ratchet N 1513 ok` (`GP_CHARSEL_*`, record gameplay-u5 §C5.18), and `diff-verify: 6/6 functions VERIFIED; 7/7 mutants detected`; U6b and U7 merge before U8 and may move every one of these, so the base's own lines are what Step 3 records and later gates compare against.
- [ ] **Step 3: Record** — append `## §U8.10 Baseline (Task 0)` with the commit (`git rev-parse HEAD`), the four gate results, the `gp_compare` ratchet lines, the `diff-verify:` line, `fnm_known`'s parameter list on this base (`grep -n 'static int fnm_known' port/tests/test_platform.c`) and `grep -c . $S/t0_verify.txt`.
- [ ] **Step 4: Commit**

```bash
git add docs/superpowers/plans/2026-10-01-gameplay-u8-derivations.md
git commit -m "$(cat <<'EOF'
docs: gameplay U8 baseline (record §U8.10)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
EOF
)"
```

(The plan and the record are on `main` since `8eaf25a`'s history; branch from a `main` that holds this re-baselined version.)

---

### Task 1: The evidence checker `tools/gp_modes.py`

**Files:**
- Create: `tools/gp_modes.py`
- Create: `tools/tests/test_gp_modes.py`
- Modify: record (append §U8.11)

**Interfaces:**
- Produces: `gp_modes.ROWS` (scenario → `dict(row, mode, b1d, b1f, spend, reach, stay)`), `gp_modes.check(name, lines) -> [(label, ok, detail)]`, `gp_modes.path(lines) -> [str]`, CLI `gp_modes.py check --scenario NAME --capture DIR` (exit 1 on any FAIL) and `gp_modes.py path --capture DIR`; constants `START_ENTRY = 0xBCCDC`, `GAME_START_MODES`, `WIPE_MODE = 0x1A`, `CHARSEL_MODE = 0x10`.
- Consumes: `gp_session.parse`, `snapshots`, `raw_to_kb`, `format_s`, `SNAP_FIELDS`, `DATA_BASE_VA`.

- [ ] **Step 1: Write the failing test** — create `tools/tests/test_gp_modes.py`:

```python
# tools/tests/test_gp_modes.py
import os, struct, sys, unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import gp_session as gs
import gp_modes as gm

EXE = os.path.join(ROOT, 'data', 'game', 'C', 'PRAGE.EXE')
IDLE = os.path.join(ROOT, 'data', 'k11-captures', 'gp-idle-loss')
CODE_FILE_OFF = 0x52E54          # obj-0 file offset = VA + 0x52E54 (AGENTS.md); immediates need no fixup


def _s(f, mode, cred=5, b1d=0, b1f=0, ent=0, raw=0, st=0):
    vals = {n: 0 for n, _, _ in gs.SNAP_FIELDS}
    vals.update(f=f, mode=mode, cred=cred, b1d=b1d, b1f=b1f, ent=ent, raw=raw, st=st, t508=1, t50c=2)
    return gs.format_s(0, vals, gs.raw_to_kb(raw), 0x1E, 0x30)


OFF = 0x266000 - 0x80000        # a capture's base minus DATA_BASE_VA (the B record below)


def _row_log(mode, b1d, b1f, spend, reach=6, stay=False):
    """A minimal poll.log of a START MENU row: MAIN MENU, START MENU open, the
    row's mode (P only, as gp-idle-loss's 0x2D at poll.log:631), the divert, the
    character select, `reach`, the X record."""
    L = ['B ms=0 base=00266000 ptr=0000FE20']
    L += [_s(f, 3) for f in range(0x100, 0x110)]
    L += [_s(f, 0x27, ent=0xBCBEC + OFF) for f in range(0x110, 0x120)]
    L += [_s(f, 0x27, ent=gm.START_ENTRY + OFF) for f in range(0x120, 0x130)]
    L.append('P ms=1 f=0130 mode=%04X st=0000 tick=00000000' % mode)
    L += [_s(f, 0x1A, cred=5 - spend, b1d=b1d, b1f=b1f) for f in range(0x131, 0x140)]
    L += [_s(f, 0x10, cred=5 - spend, b1d=b1d, b1f=b1f) for f in range(0x140, 0x150)]
    if not stay:
        L += [_s(f, reach, cred=5 - spend, b1d=b1d, b1f=b1f) for f in range(0x150, 0x160)]
    L.append('X ms=2 f=015F step=4 end')
    return L


def _attract_log():
    L = ['B ms=0 base=00266000 ptr=0000FE20']
    L += [_s(f, 3) for f in range(0x100, 0x110)]
    L += [_s(0x110, 3, raw=0x01000000), _s(0x111, 3, raw=0x01000000)]
    L.append('P ms=1 f=0112 mode=001A st=0000 tick=00000000')
    L += [_s(f, 0x1A, cred=4, b1f=1) for f in range(0x112, 0x120)]
    L += [_s(f, 0x10, cred=4, b1f=1) for f in range(0x120, 0x130)]
    L += [_s(f, 6, cred=4, b1f=1) for f in range(0x130, 0x140)]
    L.append('X ms=2 f=013F step=1 end')
    return L


def _failed(res):
    return [label for label, ok, _ in res if not ok]


class TestRowsFromTheRaw(unittest.TestCase):
    @unittest.skipUnless(os.path.isfile(EXE), 'needs data/game/C/PRAGE.EXE')
    def test_setters_and_divert_arguments(self):
        with open(EXE, 'rb') as f:
            img = f.read()

        def at(va, n):
            return img[va + CODE_FILE_OFF:va + CODE_FILE_OFF + n]
        # 0x2CBC4 + 0x18 k: push edx; mov edx,M (ba M 0 0 0); xor ah,ah (30 e4) | mov ah,N (b4 N)
        setter = {}
        for k in range(7):
            b = at(0x2CBC4 + 0x18 * k, 8)
            self.assertEqual((b[0], b[1]), (0x52, 0xBA))
            setter[b[2]] = 0 if b[6:8] == b'\x30\xe4' else b[7]
        # the divert argument per mode: the `mov <reg>, N` that feeds 0x257A4 (record §U8.2)
        divert = {0x28: (0x24F51, 0xB8), 0x29: (0x24FAF, 0xB8), 0x2A: (0x25002, 0xBB), 0x2B: (0x25187, 0xBA),
                  0x2C: (0x2505C, 0xB8), 0x2D: (0x250BF, 0xB8), 0x2E: (0x2511C, 0xB8)}
        spend = set()
        for va in range(0x24F09, 0x2519E):          # the handlers 0x28..0x2F: calls to 0x2CA7C
            b = at(va, 5)
            if b[0] == 0xE8 and va + 5 + struct.unpack('<i', b[1:])[0] == 0x2CA7C:
                spend.add(va)
        self.assertEqual(spend, {0x250BA, 0x25117, 0x25173})   # modes 0x2D, 0x2E and 0x2F
        for name, w in gm.ROWS.items():
            if w['mode'] is None:
                continue
            self.assertEqual(setter[w['mode']], w['b1d'], name)
            va, op = divert[w['mode']]
            self.assertEqual(at(va, 2), bytes((op, w['b1f'])), name)
            self.assertEqual(w['spend'], int(w['mode'] in (0x2D, 0x2E)), name)


class TestCheck(unittest.TestCase):
    def test_each_row_passes_on_its_own_log(self):
        for name, w in gm.ROWS.items():
            if w['mode'] is None:
                continue
            L = _row_log(w['mode'], w['b1d'], w['b1f'], w['spend'], w['reach'], w['stay'])
            self.assertEqual(_failed(gm.check(name, L)), [], name)

    def test_a_wrong_row_fails(self):
        L = _row_log(0x29, 1, 3, 0)                  # RIGHT PLAYER TRAINING's log
        self.assertIn('the row mode 0x28 appears', _failed(gm.check('gp-u8-left-training', L)))
        L = _row_log(0x2A, 2, 3, 0)
        bad = _failed(gm.check('gp-u8-handicap', L))
        self.assertIn('no other game-start mode', bad)

    def test_divert_and_credits_are_checked(self):
        self.assertIn('the divert: b1d=2 b1f=3', _failed(gm.check('gp-u8-tug-of-war', _row_log(0x2A, 2, 1, 0))))
        self.assertIn('credits spent 1', _failed(gm.check('gp-u8-right-arcade', _row_log(0x2E, 0, 2, 0))))
        self.assertIn('credits spent 0', _failed(gm.check('gp-u8-handicap', _row_log(0x2C, 4, 3, 1))))

    def test_start_menu_must_be_open(self):
        L = [l.replace('ent=%08X' % (gm.START_ENTRY + OFF), 'ent=%08X' % (0xBCBEC + OFF)) for l in _row_log(0x2E, 0, 2, 1)]
        self.assertIn('START MENU open (ent 0xBCCDC) before the select', _failed(gm.check('gp-u8-right-arcade', L)))

    def test_reach_and_end(self):
        L = [l for l in _row_log(0x2A, 2, 3, 0) if not (l.startswith('S ') and gs.parse(l)['mode'] == 6)]
        self.assertIn('mode 0x6 reached', _failed(gm.check('gp-u8-tug-of-war', L)))
        L = [l for l in _row_log(0x2A, 2, 3, 0) if not l.startswith('X ')]
        self.assertIn('the scenario end (X record)', _failed(gm.check('gp-u8-tug-of-war', L)))

    def test_endurance_must_stay_in_the_team_select(self):
        L = _row_log(0x2B, 3, 3, 0, reach=0x10, stay=True)
        self.assertEqual(_failed(gm.check('gp-u8-endurance', L)), [])
        L2 = L[:-1] + [_s(0x15E, 0x1A, b1d=3, b1f=3), 'X ms=2 f=015F step=4 end']
        self.assertIn('still in mode 0x10 at the end', _failed(gm.check('gp-u8-endurance', L2)))

    def test_attract_start(self):
        self.assertEqual(_failed(gm.check('gp-u8-attract-start', _attract_log())), [])
        L = [l.replace('raw=01000000', 'raw=00000000') for l in _attract_log()]
        self.assertIn('the P1 start bit reached the bitmap in mode 3', _failed(gm.check('gp-u8-attract-start', L)))
        L = _attract_log()
        L.insert(5, _s(0x104, 0x27))
        self.assertIn('no mode 0x27 (no Enter)', _failed(gm.check('gp-u8-attract-start', L)))

    @unittest.skipUnless(os.path.isfile(os.path.join(IDLE, 'poll.log')), 'needs data/k11-captures/gp-idle-loss')
    def test_the_u4_capture_is_row_0(self):
        with open(os.path.join(IDLE, 'poll.log')) as f:
            L = f.read().splitlines()
        self.assertEqual(_failed(gm.check('gp-idle-loss', L)), [])
        self.assertIn('the row mode 0x2E appears', _failed(gm.check('gp-u8-right-arcade', L)))


if __name__ == '__main__':
    unittest.main()
```

- [ ] **Step 2: Run it to see it fail**

Run: `python3 -m unittest tools.tests.test_gp_modes -v`
Expected: `ModuleNotFoundError: No module named 'gp_modes'`.

- [ ] **Step 3: Implement** — create `tools/gp_modes.py`:

```python
#!/usr/bin/env python3
"""Gameplay U8: the evidence that a capture reached its START MENU row or the
attract start (plan docs/superpowers/plans/2026-10-01-gameplay-u8-other-modes.md,
record docs/superpowers/plans/2026-10-01-gameplay-u8-derivations.md §U8.1-§U8.4).

ROWS is derived from the raw: the START MENU table 0xBCCCC (items 0xBCCDC..0xBCD3C,
+8 the setter), the setters 0x2CBC4..0x2CC6A (`mov edx,M; xor ah,ah | mov ah,N;
mov [0x104B00],dx; mov [0x104B1D],ah`), the mode switch 0x24EEC..0x24F01
(`jmp [eax*4+0x24B8C]`), the game-start handlers 0x24F09..0x25199 (0x2CA7C(1)
only in modes 0x2D/0x2E at 0x250BA/0x25117; then 0x257A4(mask), which stores
DS_00104B1F at 0x257E0 and arms the wipe 0x4F980 to mode 0x10 at 0x25816) and
the attract start 0x11D04 (0x11D13..0x11D41: 0x11F28(side) spends one credit
at 0x11F47, then 0x257A4(side mask)).

  gp_modes.py check --scenario NAME --capture DIR   the checks; exit 1 on a FAIL
  gp_modes.py path --capture DIR                     the mode path, one line per change
Stdlib only."""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gp_session as gs

START_ENTRY = 0xBCCDC            # START MENU's item list (0x2FFC4 init: DS_0010741C = table + 0x10)
GAME_START_MODES = range(0x28, 0x30)   # jump-table entries 0x24F09..0x2512B
WIPE_MODE = 0x1A                 # 0x4F980 stores mode 0x1A (0x4F989..0x4F994)
CHARSEL_MODE = 0x10              # 0x257A4: 0x2580B mov eax,0x10; 0x25816 call 0x4F980

# scenario -> the raw's expectation. mode: the setter's mode (None: the attract
# start); b1d: the setter's DS_00104B1D; b1f: 0x257A4's argument; spend: the
# credits 0x2CA7C/0x11F28 take; reach: the mode the scenario ends in; stay: the
# scenario must end still in `reach` (ENDURANCE's team select has no time-out).
ROWS = {
    'gp-idle-loss': dict(row=0, mode=0x2D, b1d=0, b1f=1, spend=1, reach=6, stay=False),   # U4's, the reference
    'gp-u8-right-arcade': dict(row=1, mode=0x2E, b1d=0, b1f=2, spend=1, reach=6, stay=False),
    'gp-u8-left-training': dict(row=2, mode=0x28, b1d=1, b1f=3, spend=0, reach=6, stay=False),
    'gp-u8-right-training': dict(row=3, mode=0x29, b1d=1, b1f=3, spend=0, reach=6, stay=False),
    'gp-u8-tug-of-war': dict(row=4, mode=0x2A, b1d=2, b1f=3, spend=0, reach=6, stay=False),
    'gp-u8-endurance': dict(row=5, mode=0x2B, b1d=3, b1f=3, spend=0, reach=0x10, stay=True),
    'gp-u8-handicap': dict(row=6, mode=0x2C, b1d=4, b1f=3, spend=0, reach=6, stay=False),
    'gp-u8-attract-start': dict(row=None, mode=None, b1d=0, b1f=1, spend=1, reach=6, stay=False),
}


def check(name, lines):
    """[(label, ok, detail)] for ROWS[name] over a poll.log (record §U8.5)."""
    want = ROWS[name]
    recs = [r for r in (gs.parse(l) for l in lines) if r]
    base = next((r['base'] for r in recs if r['kind'] == 'B'), None)
    ms = [(i, r) for i, r in enumerate(recs) if r['kind'] in ('S', 'P') and r.get('mode') is not None]
    out = []
    if want['mode'] is None:
        out.append(('no mode 0x27 (no Enter)', all(r['mode'] != 0x27 for _, r in ms), ''))
        snap = gs.snapshots(lines)
        arm = next((f for f in sorted(snap) if gs.raw_to_kb(snap[f]['raw']) & 0x0100), None)
        ok = arm is not None and snap[arm]['mode'] == 3 and snap.get(arm - 1, {}).get('mode') == 3
        out.append(('the P1 start bit reached the bitmap in mode 3', ok,
                    'f=%s' % (None if arm is None else '%X' % arm)))
        sel = next((i for i, r in ms if arm is not None and r['f'] >= arm and r['mode'] != 3), None)
        ok = sel is not None and recs[sel]['mode'] == WIPE_MODE
        out.append(('the first mode after 3 is the wipe 0x1A', ok,
                    '' if sel is None else 'f=%X mode=%X' % (recs[sel]['f'], recs[sel]['mode'])))
    else:
        sel = next((i for i, r in ms if r['mode'] == want['mode']), None)
        out.append(('the row mode 0x%X appears' % want['mode'], sel is not None,
                    '' if sel is None else 'f=%X (%s)' % (recs[sel]['f'], recs[sel]['kind'])))
        other = sorted({r['mode'] for _, r in ms if r['mode'] in GAME_START_MODES and r['mode'] != want['mode']})
        out.append(('no other game-start mode', not other, ' '.join('%X' % m for m in other)))
        off = None if base is None else base - gs.DATA_BASE_VA
        opened = sel is not None and off is not None and any(
            r['kind'] == 'S' and r['mode'] == 0x27 and r['ent'] - off == START_ENTRY for i, r in ms if i < sel)
        out.append(('START MENU open (ent 0xBCCDC) before the select', opened, ''))
    before = [r for i, r in ms if sel is not None and i < sel and r['kind'] == 'S']
    after = next((r for i, r in ms if sel is not None and i > sel and r['kind'] == 'S'
                  and r['mode'] == WIPE_MODE), None)
    ok = after is not None and after['b1d'] == want['b1d'] and after['b1f'] == want['b1f']
    out.append(('the divert: b1d=%X b1f=%X' % (want['b1d'], want['b1f']), ok,
                '' if after is None else 'f=%X b1d=%X b1f=%X' % (after['f'], after['b1d'], after['b1f'])))
    ok = after is not None and bool(before) and before[-1]['cred'] - want['spend'] == after['cred']
    out.append(('credits spent %d' % want['spend'], ok,
                '' if not (after and before) else '%X -> %X' % (before[-1]['cred'], after['cred'])))
    ch = next((r for i, r in ms if sel is not None and i > sel and r['mode'] == CHARSEL_MODE), None)
    out.append(('the character select (mode 0x10) follows', ch is not None, '' if ch is None else 'f=%X' % ch['f']))
    reached = next((r for i, r in ms if sel is not None and i > sel and r['mode'] == want['reach']), None)
    out.append(('mode 0x%X reached' % want['reach'], reached is not None,
                '' if reached is None else 'f=%X' % reached['f']))
    xrec = next((r for r in recs if r['kind'] == 'X'), None)
    out.append(('the scenario end (X record)', xrec is not None, '' if xrec is None else 'f=%X' % xrec['f']))
    if want['stay']:
        last = [r for i, r in ms if reached is not None and r['kind'] == 'S'
                and xrec is not None and r['f'] <= xrec['f']]
        ok = bool(last) and all(r['mode'] == want['reach'] for r in last if r['f'] >= reached['f'])
        out.append(('still in mode 0x%X at the end' % want['reach'], ok,
                    '' if not last else 'last S f=%X mode=%X' % (last[-1]['f'], last[-1]['mode'])))
    return out


def path(lines):
    """One line per mode change of the S/P records (poll.log:<n> kind f mode), and
    the first S record of each new mode with cred, b1d, b1f, e0, e2 and both
    slots' +0x5A."""
    out, prev, prev_s = [], None, None
    for n, l in enumerate(lines, 1):
        r = gs.parse(l)
        if not r or r['kind'] not in ('S', 'P'):
            continue
        if r['kind'] == 'S' and r['mode'] != prev_s:
            out.append('poll.log:%d S f=%X mode=%X cred=%X b1d=%X b1f=%X e0=%X e2=%X s0_5a=%X s1_5a=%X' % (
                n, r['f'], r['mode'], r['cred'], r['b1d'], r['b1f'], r['e0'], r['e2'], r['s0_5a'], r['s1_5a']))
            prev = prev_s = r['mode']
        elif r['kind'] == 'P' and r['mode'] != prev:
            out.append('poll.log:%d P f=%X mode=%X' % (n, r['f'], r['mode']))
            prev = r['mode']
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('cmd', choices=('check', 'path'))
    ap.add_argument('--scenario')
    ap.add_argument('--capture', required=True)
    a = ap.parse_args()
    with open(os.path.join(a.capture, 'poll.log')) as f:
        lines = f.read().splitlines()
    if a.cmd == 'path':
        print('\n'.join(path(lines)))
        return 0
    res = check(a.scenario, lines)
    for label, ok, detail in res:
        print('gp_modes: %s: %s: %s%s' % (a.scenario, label, 'ok' if ok else 'FAIL', ' (%s)' % detail if detail else ''))
    return 0 if all(ok for _, ok, _ in res) else 1


if __name__ == '__main__':
    sys.exit(main())
```

- [ ] **Step 4: Run the tests**

Run: `python3 -m unittest tools.tests.test_gp_modes -v`
Expected: `Ran 9 tests … OK` (with `data/` present none is skipped; `test_setters_and_divert_arguments` reads `PRAGE.EXE`, `test_the_u4_capture_is_row_0` reads `gp-idle-loss/poll.log`).

Then the CLI on the U4 capture (row 0, the reference):

```bash
python3 tools/gp_modes.py check --scenario gp-idle-loss --capture data/k11-captures/gp-idle-loss; echo rc=$?
python3 tools/gp_modes.py check --scenario gp-u8-right-arcade --capture data/k11-captures/gp-idle-loss; echo rc=$?
```

Expected (planner run): eight `ok` lines — `the row mode 0x2D appears: ok (f=26E (P))`, `no other game-start mode: ok`, `START MENU open (ent 0xBCCDC) before the select: ok`, `the divert: b1d=0 b1f=1: ok (f=26F b1d=0 b1f=1)`, `credits spent 1: ok (5 -> 4)`, `the character select (mode 0x10) follows: ok (f=293)`, `mode 0x6 reached: ok (f=7F5)`, `the scenario end (X record): ok (f=207F)` — and `rc=0`; then seven `FAIL` lines (only the `X record` line `ok`) and `rc=1`.

- [ ] **Step 5: Mutation proofs** (each applied alone, the suite run, then restored; planner-verified):
  - M1: `ROWS['gp-u8-tug-of-war']` `b1f=3` → `b1f=1`: `FAIL: test_setters_and_divert_arguments` and `FAIL: test_divert_and_credits_are_checked`.
  - M2: the credits line `ok = after is not None and bool(before) and before[-1]['cred'] - want['spend'] == after['cred']` → `ok = after is not None`: `FAIL: test_divert_and_credits_are_checked`.
  - M3: `if want['stay']:` → `if False:`: `FAIL: test_endurance_must_stay_in_the_team_select`.
  - M4: `r['ent'] - off == START_ENTRY` → `True`: `FAIL: test_start_menu_must_be_open`.
  Record the four outputs in §U8.11.

- [ ] **Step 6: Record and commit** — §U8.11: the test count, the two CLI outputs, the mutation lines. Run the gate (`<n>` = 1).

```bash
git add tools/gp_modes.py tools/tests/test_gp_modes.py docs/superpowers/plans/2026-10-01-gameplay-u8-derivations.md
git commit -m "$(cat <<'EOF'
tools: gp_modes, the evidence that a capture reached its START MENU row (record §U8.11)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 2: The six START MENU row scenarios

**Files:**
- Modify: `tools/gp_session.py` (immediately before `def expand(action):`; on `8eaf25a` after U5's `SCENARIOS['gp-u5-charsel']` block, which ends at line 148)
- Modify: `tools/tests/test_gp_modes.py` (append `TestScenarios` before the `if __name__` block)
- Modify: record (append §U8.12)

**Interfaces:**
- Produces: `gp_session._u8_menu(moves, reach, time_limit) -> dict(time_limit, steps)`; `SCENARIOS['gp-u8-right-arcade' | 'gp-u8-left-training' | 'gp-u8-right-training' | 'gp-u8-tug-of-war' | 'gp-u8-endurance' | 'gp-u8-handicap']`, which `gp_capture.py --scenario` accepts (its `choices` are the `gp-` keys).
- Consumes: `ENTER_WAIT`, `Schedule`, `gp_modes.ROWS`.

- [ ] **Step 1: Write the failing test** — insert before `if __name__ == '__main__':` in `tools/tests/test_gp_modes.py`:

```python
class TestScenarios(unittest.TestCase):
    def test_every_row_has_a_scenario_and_the_walk_is_derived(self):
        # START MENU opens on row 0 (spec §3.3); row k is k p1.down, row 6 one p1.up (the wrap)
        for name, w in gm.ROWS.items():
            if name == 'gp-idle-loss' or w['mode'] is None:
                continue
            steps = gs.SCENARIOS[name]['steps']
            acts = [st[-1] for st in steps if isinstance(st[-1], tuple)]
            want = [('pad', ('p1.up',), 4)] if w['row'] == 6 else [('pad', ('p1.down',), 4)] * w['row']
            self.assertEqual([a for a in acts if a[0] == 'pad'], want, name)
            self.assertEqual([a for a in acts if a[0] == 'key'], [('key', 'enter')] * 3, name)
            self.assertEqual(steps[-1], ('until_mode', w['reach'], 300), name)

    def test_row_schedule_fires_the_walk(self):
        s = gs.Schedule(gs.SCENARIOS['gp-u8-tug-of-war']['steps'])
        self.assertEqual(s.due_boot(gs.ENTER_WAIT), [(0, ('key', 'enter'))])
        s.on_mode(0x141, 0x27)
        self.assertEqual(s.due(0x141 + 149), [(1, ('key', 'enter'))])
        got = [s.due(0x141 + 149 + 60 * k) for k in range(1, 6)]
        self.assertEqual(got, [[(k + 1, ('pad', ('p1.down',), 4))] for k in range(1, 5)] + [[(6, ('key', 'enter'))]])
        s.on_mode(0x303, 0x2A)
        s.on_mode(0x328, 0x10)
        self.assertIsNone(s.end_frame)
        s.on_mode(0x875, 6)
        self.assertEqual((s.end_frame, s.fired, s.total), (0x875 + 300, 7, 7))
```

- [ ] **Step 2: Run it to see it fail**

Run: `python3 -m unittest tools.tests.test_gp_modes -v`
Expected: `KeyError: 'gp-u8-right-arcade'` (and `'gp-u8-tug-of-war'`).

- [ ] **Step 3: Implement** — insert immediately before `def expand(action):`, after the last `SCENARIOS[…]` block (on `8eaf25a` that is U5's `SCENARIOS['gp-u5-charsel'] = dict(…)`, lines 140–148; U6b/U7/U11 may add theirs first), with two blank lines on each side:

```python
# Gameplay U8 (plan 2026-10-01-gameplay-u8-other-modes.md, record
# 2026-10-01-gameplay-u8-derivations.md §U8.3): START MENU rows 1..6 and the
# attract start. The walk: the mode-3 Enter (MAIN MENU on "Start"), the Enter
# that opens START MENU on row 0 (0x2CB74 -> 0x2FFC4(0xBCCCC)), one p1.down per
# row (menu_step 0x3055E: the level edge of 0x40004000) or, for row 6, one p1.up
# (0x30511..0x3052F: row -1 wraps to the count, 7), then the Enter that runs the
# row's setter (0x3046F..0x304A2). Harness values: U4's 150-frame gap; 60-frame
# gaps and a 4-frame hold (one level edge: the hold is under the 0x1E-frame
# repeat delay that 0x251C6..0x251DA re-arms every mode-0x27 frame); the end 300
# frames into the reach mode; time_limit = U4's wall timeline (§U8.6) + 6 s.
def _u8_menu(moves, reach, time_limit):
    steps = (('boot', ENTER_WAIT, ('key', 'enter')),
             ('after_mode', 0x27, 150, ('key', 'enter')))
    steps += tuple(('after', 60, ('pad', (m,), 4)) for m in moves)
    steps += (('after', 60, ('key', 'enter')), ('until_mode', reach, 300))
    return dict(time_limit=time_limit, steps=steps)


SCENARIOS['gp-u8-right-arcade'] = _u8_menu(('p1.down',), 6, 66)          # row 1, mode 0x2E
SCENARIOS['gp-u8-left-training'] = _u8_menu(('p1.down',) * 2, 6, 67)     # row 2, mode 0x28
SCENARIOS['gp-u8-right-training'] = _u8_menu(('p1.down',) * 3, 6, 68)    # row 3, mode 0x29
SCENARIOS['gp-u8-tug-of-war'] = _u8_menu(('p1.down',) * 4, 6, 69)        # row 4, mode 0x2A
# Row 5, mode 0x2B: the team select 0x44798 has no time-out (§U8.4), so the
# scenario ends 300 frames into mode 0x10.
SCENARIOS['gp-u8-endurance'] = _u8_menu(('p1.down',) * 5, 0x10, 46)
SCENARIOS['gp-u8-handicap'] = _u8_menu(('p1.up',), 6, 66)                # row 6, mode 0x2C
```

The time limits are record §U8.6's `65 + k` (k moves; ENDURANCE 46): harness values.

- [ ] **Step 4: Run the tests**

Run: `python3 -m unittest tools.tests.test_gp_modes tools.tests.test_gp_session tools.tests.test_gp_capture -v`
Expected: all `OK` (`test_gp_modes` 11 tests; the U1–U4 suites unchanged).

- [ ] **Step 5: Mutation proof** — `SCENARIOS['gp-u8-handicap'] = _u8_menu(('p1.up',), 6, 66)` → `_u8_menu(('p1.down',) * 6, 6, 66)`: `FAIL: test_every_row_has_a_scenario_and_the_walk_is_derived` (planner-verified). Restore.

- [ ] **Step 6: Record, gate, commit** — §U8.12: the scenario table (name, row, moves, reach, time limit) with record §U8.3/§U8.6 as the source; gate (`<n>` = 2).

```bash
git add tools/gp_session.py tools/tests/test_gp_modes.py docs/superpowers/plans/2026-10-01-gameplay-u8-derivations.md
git commit -m "$(cat <<'EOF'
tools: gp-u8 scenarios for START MENU rows 1-6 (record §U8.12)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 3: `make gp-modes-oracle` in `make verify`, proved on the U4 capture

**Files:**
- Modify: `Makefile` (after the `gp-report` recipe, lines 494–496 on `8eaf25a`; `verify`, after the last gameplay-oracle line, line 546 on `8eaf25a`)
- Modify: record (append §U8.13)

**Interfaces:**
- Produces: `make gp-modes-oracle` (runs `tools.tests.test_gp_modes`, then `gp-modes-one` for each `<ID>:<scenario>` of `GP_MODES_SCENARIOS`); `make gp-modes-one GP_MODES_ID=<ID> scenario=<sc>`; variables `GP_MODES_SCENARIOS` (empty until Task 6a), `GP_MODES_KEEP`, and per scenario `GP_MODES_<ID>_MIN_FIRST`, `_TRACE_MIN_FIRST`, `_MAX_START`, `_CAPTURE_SHA256`, `_CAPTURE_FRAMES`, optional `_END`.
- Consumes: `gp-replay` (with `GP_OPTIONAL=1`, `GP_SCRIPT_ARGS`), `tools/gp_compare.py` (unchanged), `tools/gp_modes.py`.

- [ ] **Step 1: The failing run**

Run: `make gp-modes-oracle; echo "exit=$?"`
Expected: `make: *** No rule to make target 'gp-modes-oracle'.  Stop.` and `exit=2`.

- [ ] **Step 2: Implement** — insert after the `gp-report` recipe's last line (`@$(PYTHON) tools/gp_compare.py --report …`) and its blank line:

```make
# Gameplay U8 (plan 2026-10-01-gameplay-u8-other-modes.md, record 2026-10-01-gameplay-u8-
# derivations.md): the other START MENU rows and the attract start, one capture each. For every
# <ID>:<scenario> in GP_MODES_SCENARIOS whose data/k11-captures/<scenario> exists: the evidence
# check that the capture reached its row (tools/gp_modes.py), the port replay (cut at
# GP_MODES_<ID>_END when set) and both ratchets with the capture identity pin; an absent capture
# skips (exit 0), even under PR_ORACLE_REQUIRED (spec §4.3). A scenario is listed only once its
# values are pinned (record §U8.16-§U8.22), each value's provenance in its comment. The port dump
# is removed after its comparison unless GP_MODES_KEEP=1 (record §U8.6: 52-89 MB of /tmp each).
GP_MODES_SCENARIOS =
GP_MODES_KEEP ?=
.PHONY: gp-modes-oracle gp-modes-one
gp-modes-oracle: build ## Gameplay U8 oracle: the other START MENU rows and the attract start (each skips without its capture)
	@echo "== gameplay U8: other modes (frame and trace ratchets; each skips without its capture) =="
	@$(PYTHON) -m unittest tools.tests.test_gp_modes
	@for p in $(GP_MODES_SCENARIOS); do \
		$(MAKE) --no-print-directory gp-modes-one GP_MODES_ID=$${p%%:*} scenario=$${p#*:} || exit 1; \
	done

gp-modes-one: build
	@if [ -d $(K11_CAPTURES)/$(scenario) ]; then \
		$(PYTHON) tools/gp_modes.py check --scenario $(scenario) --capture $(K11_CAPTURES)/$(scenario) && \
		$(MAKE) --no-print-directory gp-replay scenario=$(scenario) GP_OPTIONAL=1 \
			GP_SCRIPT_ARGS="$(if $(GP_MODES_$(GP_MODES_ID)_END),--end $(GP_MODES_$(GP_MODES_ID)_END))" && \
		$(PYTHON) tools/gp_compare.py --scenario $(scenario) --capture $(K11_CAPTURES)/$(scenario) \
			--port $(GP_DUMP)/$(scenario) --min-first "$(GP_MODES_$(GP_MODES_ID)_MIN_FIRST)" \
			--trace-min-first "$(GP_MODES_$(GP_MODES_ID)_TRACE_MIN_FIRST)" \
			--max-start "$(GP_MODES_$(GP_MODES_ID)_MAX_START)" \
			--capture-sha256 "$(GP_MODES_$(GP_MODES_ID)_CAPTURE_SHA256)" \
			--capture-frames "$(GP_MODES_$(GP_MODES_ID)_CAPTURE_FRAMES)"; \
		rc=$$?; [ -n "$(GP_MODES_KEEP)" ] || rm -rf $(GP_DUMP)/$(scenario); exit $$rc; \
	else \
		echo "gp-modes-oracle: no capture at $(K11_CAPTURES)/$(scenario), skipped"; \
	fi
```

and in `verify`, after the last gameplay-oracle line (on `8eaf25a`: `@$(MAKE) --no-print-directory gp-charsel-oracle`, which U5 added after `gp-oracle`; keep any line U6b/U7/U11 added there):

```make
	@$(MAKE) --no-print-directory gp-modes-oracle
```

- [ ] **Step 3: Empty list and skip** (planner-verified)

```bash
make gp-modes-oracle GP_DUMP=$S/gpo 2>&1 | tail -3; echo "exit=$?"
PR_ORACLE_REQUIRED=1 make gp-modes-oracle GP_DUMP=$S/gpo GP_MODES_SCENARIOS="RA:gp-u8-right-arcade" 2>&1 | tail -1; echo "exit=$?"
```

Expected: the unit tests `OK`, `exit=0`; then `gp-modes-oracle: no capture at data/k11-captures/gp-u8-right-arcade, skipped`, `exit=0`.

- [ ] **Step 4: The whole path on real data** — `gp-idle-loss` as a reference row, cut where a row scenario ends (`f = 0x7F5 + 300 = 2325`), with the cut replay's own pins (the Makefile's `GP_IDLE_LOSS_*` 2064/8320 belong to the full replay to `f = 0x207F` and cannot be used on a cut: an N past the cut's end is unreachable). Nothing of this is committed; it proves the target before any U8 capture exists. The values `REF_N = 1070` and `REF_F = 2326` were measured on `8eaf25a` (re-baseline, record §U8.7); U6b and U7 merge before U8, so first measure them on this base — the replay as `gp-modes-one` runs it, then the report:

```bash
rm -rf $S/gpm; make gp-replay scenario=gp-idle-loss GP_OPTIONAL=1 GP_SCRIPT_ARGS="--end 2325" GP_DUMP=$S/gpm 2>&1 | grep -E 'fn-miss PR_GP_DUMP distinct|all checks|FAIL'
python3 tools/gp_compare.py --report --scenario gp-idle-loss --capture data/k11-captures/gp-idle-loss --port $S/gpm/gp-idle-loss \
  | grep -E 'window|FIRST UNEXPLAINED|first difference|differing through'; tail -1 $S/gpm/gp-idle-loss/frames.txt; rm -rf $S/gpm
```

`REF_N` = the `FIRST UNEXPLAINED capture <j>` value; `REF_F` = the `0 differing through <e>` value + 1 (the exact pin, `gp_compare.ratchet`: with no difference, any N ≤ end passes and N = end + 1 fails as unreachable), or, if a `first difference f=<hex> (<F>)` line is printed, that `F`. On `8eaf25a` (re-baseline run): `fn-miss PR_GP_DUMP distinct=4 dropped=0`, `all checks passed`; `window from capture 90 (raw 1744)`; `FIRST UNEXPLAINED capture 1070 (raw 4182): nearest port 817, rows 121..199`; `0 differing through 2325`; `00817 f=0915 tick=00000973 mode=0006`. Port 817 (`f = 0x915` = 2325) is the replay's last frame, so capture 1070 is how far the cut replay got, not a divergence. If `j` is below 1070 or a trace difference appears, the base has a divergence before `f = 0x915` that `make gp-oracle` must also show: stop and report it. Then, with `REF_N`/`REF_F` as measured:

```bash
REF_N=1070; REF_F=2326     # as measured above
make gp-modes-oracle GP_DUMP=$S/gpo GP_MODES_KEEP=1 GP_MODES_SCENARIOS="REF:gp-idle-loss" \
  GP_MODES_REF_END=2325 GP_MODES_REF_MIN_FIRST=$REF_N GP_MODES_REF_TRACE_MIN_FIRST=$REF_F GP_MODES_REF_MAX_START=90 \
  GP_MODES_REF_CAPTURE_SHA256=773e264731ea83a23623c6ec6cc5547165628d8adcf96f5a7c99c1c2b88c8447 \
  GP_MODES_REF_CAPTURE_FRAMES=8173 > $S/t3_ref.txt 2>&1; echo "exit=$?"
grep -E 'gp_modes|gp_compare|all checks|FAIL' $S/t3_ref.txt; du -sh $S/gpo/gp-idle-loss
make gp-modes-oracle GP_DUMP=$S/gpo GP_MODES_SCENARIOS="REF:gp-idle-loss" \
  GP_MODES_REF_END=2325 GP_MODES_REF_MIN_FIRST=$((REF_N + 1)) GP_MODES_REF_TRACE_MIN_FIRST=$REF_F GP_MODES_REF_MAX_START=90 \
  GP_MODES_REF_CAPTURE_SHA256=773e264731ea83a23623c6ec6cc5547165628d8adcf96f5a7c99c1c2b88c8447 \
  GP_MODES_REF_CAPTURE_FRAMES=8173 > $S/t3_ref_fail.txt 2>&1; echo "exit=$?"; grep FAIL $S/t3_ref_fail.txt
make gp-modes-oracle GP_DUMP=$S/gpo GP_MODES_SCENARIOS="REF:gp-idle-loss" \
  GP_MODES_REF_END=2325 GP_MODES_REF_MIN_FIRST=$REF_N GP_MODES_REF_TRACE_MIN_FIRST=$((REF_F + 1)) GP_MODES_REF_MAX_START=90 \
  GP_MODES_REF_CAPTURE_SHA256=773e264731ea83a23623c6ec6cc5547165628d8adcf96f5a7c99c1c2b88c8447 \
  GP_MODES_REF_CAPTURE_FRAMES=8173 > $S/t3_ref_fail2.txt 2>&1; echo "exit=$?"; grep FAIL $S/t3_ref_fail2.txt; ls $S/gpo
```

Expected (planner run on `e9271df` 42 s per run; the values re-measured on `8eaf25a`): `exit=0`; the eight `gp_modes: gp-idle-loss: … ok` lines; `all checks passed`; `gp_compare: gp-idle-loss: capture: poll.log sha256 773e2647..8c8447, 8173 frames: matches the pin`; `frames: window from capture 90 (raw 1744)`; `FIRST UNEXPLAINED capture 1070 (raw 4182)`; `first unexplained 1070, ratchet N 1070 ok`; `trace: 0 differing through 2325; ratchet N 2326 ok`; dump ≈ 52 MB. The second run: `frames: FAIL: first unexplained 1070 < ratchet N 1071`, `exit=2`; the third: `trace: FAIL: N 2327 > end 2326: N is unreachable`, `exit=2`; and `ls $S/gpo` shows only `gp-idle-loss.script` (the dump is removed without `GP_MODES_KEEP`). Both mutations sit exactly one above the measured values, so each can fail only if the ratchet compares against the measured first frame and the replay's end. The planner's original mutation (`MIN_FIRST=204` over `203`) passes trivially on any base holding U5: there the cut replay's first unexplained frame is 1070, and `gp_compare.ratchet` prints `ratchet N 204 ok (improved: raise N)`, exit 0 (re-baseline run on `8eaf25a`).

- [ ] **Step 5: Record, gate, commit** — §U8.13: the three outputs. Gate (`<n>` = 3): additionally `grep -E '^== gameplay U8' $S/t3_verify.txt` shows the banner and the unit tests ran.

```bash
git add Makefile docs/superpowers/plans/2026-10-01-gameplay-u8-derivations.md
git commit -m "$(cat <<'EOF'
build: gp-modes-oracle in make verify, proved on gp-idle-loss (record §U8.13)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 4 (D2): The pad arm and the attract-start scenario (tools)

**Files:**
- Modify: `tools/gp_session.py` (the attract-start scenario after the Task 2 block; `pad_arm` before `def port_script`; three edits inside `port_script`)
- Modify: `tools/gp_capture.py` (`run_checks`, lines 144–153)
- Modify: `tools/tests/test_gp_modes.py` (append `_arm_log` and `TestPadArm` before `if __name__`)
- Modify: record (append §U8.14)

**Interfaces:**
- Produces: `SCENARIOS['gp-u8-attract-start']` with the key `arm='pad'`; `gp_session.pad_arm(snap, keys) -> dict(f, st, mode)`; a port script v2 with the extra line `arm pad` before `enter_frame` (only for `arm='pad'`; `enter_frame` = the first `S` whose bitmap is non-zero, `enter_state` its `st`); `gp_capture.run_checks` label `mode left 3 after the pad arm` for `arm='pad'`.
- Consumes: the record §U8.3 attract-start facts (mode 3 at the arm frame, the wipe from the next).

- [ ] **Step 1: Write the failing test** — insert before `if __name__ == '__main__':` in `tools/tests/test_gp_modes.py`:

```python
def _arm_log():
    """The attract start: mode 3, F1 pressed (boot step, its BIOS word 3B00),
    sampled from f 0x110 (raw bit 24), the word consumed at 0x112, the wipe."""
    L = ['B ms=0 base=00266000 ptr=0000FE20']
    L += [_s(f, 3, st=4) for f in range(0x100, 0x110)]
    L.append('I ms=1 f=010F step=0 press=p1.start scan=3B lin=0001008F old=FF bios=3B00 ring=1 late=0')
    L += [_s(0x110, 3, st=4, raw=0x01000000), _s(0x111, 3, st=4, raw=0x01000000)]
    L.append('H ms=2 f=0112 head=0020')
    L.append('P ms=2 f=0112 mode=001A st=0004 tick=00000000')
    L += [_s(0x112, 0x1A, cred=4, b1f=1, raw=0x01000000), _s(0x113, 0x1A, cred=4, b1f=1, raw=0x01000000)]
    L = [l.replace('head=001E', 'head=0020') if l.startswith('S ') and gs.parse(l)['f'] >= 0x112 else l for l in L]
    L.append('I ms=3 f=0113 step=0 release=p1.start lin=0001008F')
    L += [_s(f, 0x1A, cred=4, b1f=1).replace('head=001E', 'head=0020') for f in range(0x114, 0x118)]
    L.append('X ms=4 f=0117 step=1 end')
    return L


class TestPadArm(unittest.TestCase):
    def test_the_attract_start_scenario(self):
        sc = gs.SCENARIOS['gp-u8-attract-start']
        self.assertEqual(sc.get('arm'), 'pad')
        self.assertEqual(sc['steps'], (('boot', gs.ENTER_WAIT, ('pad', ('p1.start',), 4)), ('until_mode', 6, 300)))
        self.assertEqual(gm.ROWS['gp-u8-attract-start']['reach'], 6)

    def test_the_script_arms_on_the_press(self):
        text = gs.port_script('gp-u8-attract-start', _arm_log())
        self.assertEqual(text.splitlines()[1:], ['arm pad', 'enter_frame 272', 'enter_state 0004',
                                                 'bits 272 0100', 'key 274 3B 00', 'bits 276 0000', 'end 279'])

    def test_the_arm_must_be_pinned_in_mode_3(self):
        L = [l for l in _arm_log() if not (l.startswith('S ') and gs.parse(l)['f'] == 0x10F)]
        with self.assertRaises(gs.ScriptError):
            gs.port_script('gp-u8-attract-start', L)
        L = [l.replace('raw=01000000', 'raw=00000000') for l in _arm_log()]
        with self.assertRaises(gs.ScriptError):
            gs.port_script('gp-u8-attract-start', L)
        L = [l.replace('mode=0003', 'mode=0027') if l.startswith('S ') and gs.parse(l)['f'] in (0x10F, 0x110) else l
             for l in _arm_log()]                       # 0x11D04 runs in mode 3 only (0x25238)
        with self.assertRaises(gs.ScriptError):
            gs.port_script('gp-u8-attract-start', L)

    def test_an_enter_scenario_still_needs_mode_0x27(self):
        with self.assertRaises(gs.ScriptError):
            gs.port_script('gp-u8-right-arcade', _arm_log())

    def test_the_capture_check_follows_the_arm(self):
        import gp_capture as gc
        s = gs.Schedule(())
        res = dict((n.split(' (')[0], ok) for n, ok in gc.run_checks('gp-u8-attract-start', _arm_log(), s, 3, 3))
        self.assertTrue(res['mode left 3 after the pad arm'])
        self.assertTrue(res['port script v2'])
        bad = [l for l in _arm_log() if not (l.startswith('P ') or (l.startswith('S ') and gs.parse(l)['mode'] == 0x1A))]
        res = dict((n.split(' (')[0], ok) for n, ok in gc.run_checks('gp-u8-attract-start', bad, s, 3, 3))
        self.assertFalse(res['mode left 3 after the pad arm'])
```

- [ ] **Step 2: Run it to see it fail**

Run: `python3 -m unittest tools.tests.test_gp_modes -v`
Expected: `KeyError: 'gp-u8-attract-start'` in `test_the_attract_start_scenario`; `test_the_script_arms_on_the_press` and `test_the_capture_check_follows_the_arm` fail (`mode 0x27 never observed` / `KeyError: 'mode left 3 after the pad arm'`).

- [ ] **Step 3: Implement**

(a) `tools/gp_session.py`, after the Task 2 block's last scenario line:

```python
# The attract start (spec §3.4; 0x11D04 -> 0x11F28(0)): P1's start (F1) in mode
# 3 at the K11 boot wait, held 4; no Enter, so the port script's arm is the
# press (arm='pad', port_script's pad_arm).
SCENARIOS['gp-u8-attract-start'] = dict(time_limit=61, arm='pad', steps=(
    ('boot', ENTER_WAIT, ('pad', ('p1.start',), 4)),
    ('until_mode', 6, 300),
))
```

(b) `tools/gp_session.py`, before `def port_script(name, lines, end=None):`:

```python
def pad_arm(snap, keys):
    """The arm of a scenario with arm='pad' (the attract start, record gameplay-u8
    §U8.3): no Enter, so the arm frame is the first S record whose bitmap is
    non-zero, pinned by S(f - 1) in mode 3 (0x11D04 runs in mode 3 only, 0x25238);
    every key is consumed at or after it."""
    f = next((f for f in sorted(snap) if raw_to_kb(snap[f]['raw']) != 0), None)
    if f is None:
        raise ScriptError('the pad arm never reached the bitmap')
    if f - 1 not in snap or snap[f - 1]['mode'] != 3 or snap[f]['mode'] != 3:
        raise ScriptError('the pad arm at f=%X is not pinned in mode 3' % f)
    if keys and keys[0][0] < f:
        raise ScriptError('a key (f=%X) is consumed before the pad arm (f=%X)' % (keys[0][0], f))
    return dict(f=f, st=snap[f]['st'], mode=snap[f]['mode'])
```

(c) In `port_script`, replace

```python
    p27 = next((r for r in recs if r['kind'] in ('S', 'P') and r.get('mode') == 0x27), None)
    if p27 is None:
        raise ScriptError('mode 0x27 never observed')
    if not keys or keys[0][0] != p27['f']:
        raise ScriptError('the first key (f=%s) is not the frame mode 0x27 appears (f=%X)'
                          % (keys[0][0] if keys else None, p27['f']))
```

with

```python
    pad = SCENARIOS.get(name, {}).get('arm', 'enter') == 'pad'
    if pad:
        p27 = pad_arm(snap, keys)
    else:
        p27 = next((r for r in recs if r['kind'] in ('S', 'P') and r.get('mode') == 0x27), None)
        if p27 is None:
            raise ScriptError('mode 0x27 never observed')
        if not keys or keys[0][0] != p27['f']:
            raise ScriptError('the first key (f=%s) is not the frame mode 0x27 appears (f=%X)'
                              % (keys[0][0] if keys else None, p27['f']))
```

then replace

```python
        if end < keys[0][0]:
            raise ScriptError('--end %d is before the Enter (f=%d): the script would not start' % (end, keys[0][0]))
```

with

```python
        if end < p27['f']:
            raise ScriptError('--end %d is before the %s (f=%d): the script would not start'
                              % (end, 'pad arm' if pad else 'Enter', p27['f']))
```

(for an Enter scenario `p27['f']` equals `keys[0][0]`, so the message is unchanged), and replace

```python
    out = ['# gp port script v2: scenario %s%s' % (name, '' if end is None else ' (cut at %d)' % end),
           'enter_frame %d' % p27['f'],
           'enter_state %04X' % p27['st']]
```

with

```python
    out = ['# gp port script v2: scenario %s%s' % (name, '' if end is None else ' (cut at %d)' % end)]
    out += ['arm pad'] if pad else []
    out += ['enter_frame %d' % p27['f'], 'enter_state %04X' % p27['st']]
```

(d) `tools/gp_capture.py` `run_checks`: replace

```python
    enter = next((i for i, x in enumerate(recs) if x['kind'] == 'I' and x.get('press') == 'enter'), None)
    first27 = next((i for i, x in enumerate(recs) if x['kind'] in ('S', 'P') and x.get('mode') == 0x27), None)
```

with

```python
    if gs.SCENARIOS.get(name, {}).get('arm', 'enter') == 'pad':      # record gameplay-u8 §U8.3
        arm_label = 'mode left 3 after the pad arm'
        enter = next((i for i, x in enumerate(recs) if x['kind'] == 'I' and 'press' in x), None)
        first27 = next((i for i, x in enumerate(recs) if enter is not None and i > enter
                        and x['kind'] in ('S', 'P') and x.get('mode') not in (None, 3)), None)
    else:
        arm_label = 'mode 0x27 after the Enter'
        enter = next((i for i, x in enumerate(recs) if x['kind'] == 'I' and x.get('press') == 'enter'), None)
        first27 = next((i for i, x in enumerate(recs) if x['kind'] in ('S', 'P') and x.get('mode') == 0x27), None)
```

and in its returned list `('mode 0x27 after the Enter', ordered),` → `(arm_label, ordered),`.

- [ ] **Step 4: Run the tests**

Run: `python3 -m unittest tools.tests.test_gp_modes tools.tests.test_gp_session tools.tests.test_gp_capture tools.tests.test_gp_compare -v`
Expected: all `OK` (`test_gp_modes` 16 tests; the U1–U4 suites unchanged — the Enter path is byte-identical for their scenarios). Also `python3 tools/gp_session.py port-script --scenario gp-pads --capture data/k11-captures/gp-pads --out $S/pads.script && head -3 $S/pads.script`: `enter_frame 321`, `enter_state 0000` (record §G.7.3), no `arm pad` line.

- [ ] **Step 5: Mutation proofs** (planner-verified):
  - M5: `pad = SCENARIOS.get(name, {}).get('arm', 'enter') == 'pad'` → `pad = False`: `ERROR: test_the_script_arms_on_the_press`, `FAIL: test_the_capture_check_follows_the_arm`.
  - M6: in `run_checks`, `if gs.SCENARIOS.get(name, {}).get('arm', 'enter') == 'pad':` → `if False:`: `ERROR: test_the_capture_check_follows_the_arm`.
  - M7: in `pad_arm`, `if f - 1 not in snap or snap[f - 1]['mode'] != 3 or snap[f]['mode'] != 3:` → `if False:`: `FAIL: test_the_arm_must_be_pinned_in_mode_3`.

- [ ] **Step 6: Record, gate, commit** — §U8.14: the tests, the `gp-pads` script head, the mutation lines; gate (`<n>` = 4).

```bash
git add tools/gp_session.py tools/gp_capture.py tools/tests/test_gp_modes.py docs/superpowers/plans/2026-10-01-gameplay-u8-derivations.md
git commit -m "$(cat <<'EOF'
tools: the pad arm (no Enter) and the gp-u8-attract-start scenario (record §U8.14)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 5 (D2): The `PR_GP_DUMP` driver accepts the pad arm

**Files:**
- Modify: `port/tests/test_game.c` (gp driver: statics at line 12192, `gp_parse` 12197–12229, the arm checks in `test_gp_replay` 12396–12397, all on `8eaf25a`; the K11 driver's look-alike `mode_before`/`mode_after` checks at 12162–12163 differ in whitespace and are not touched)
- Modify: record (append §U8.15)

**Interfaces:**
- Consumes: a v2 script with `arm pad` (Task 4): its first step is `bits <enter_frame> <kb != 0>`.
- Produces: the driver's arm checks for a pad arm: `mode_before == 3`, `mode_after == 3` (the press reaches the level one iteration later, record §U8.3), `frame_after == enter_frame`, `state_after == enter_state`; every Enter script behaves as before.

- [ ] **Step 1: The failing run** — a hand-built pad-arm script (the attract at `f = 0x140` is in `st = 0` in mode 3: `gp-idle-loss/poll.log`'s `S f=0140 mode=0003 st=0000`):

```bash
printf '# gp port script v2: scenario gp-u8-attract-start\narm pad\nenter_frame 320\nenter_state 0000\nbits 320 0100\nkey 322 3B 00\nbits 324 0000\nend 400\n' > $S/arm.script
make build > /dev/null
PR_GP_DUMP=$S/arm PR_GP_SCRIPT=$S/arm.script PR_GAME_DIR=data/game/C ./build/run_tests 2>&1 | grep -E 'FAIL|all checks' | head -3
```

Expected: `FAIL …: PR_GP_SCRIPT names a parsable gp port script v2` (derived from `gp_parse`: `arm pad` matches none of its line forms, so `ok = 0`).

- [ ] **Step 2: Implement** (planner-verified edits):

(a) after `static int gp_armed, gp_done, gp_failed;`:

```c
static int gp_arm_pad;              /* `arm pad` (record gameplay-u8 §U8.3): the arm is a pad press in mode 3 */
```

(b) in `gp_parse`, after `gp_n = gp_nkeys = 0u;` add `gp_arm_pad = 0;`, and after `if (line[0] == '#' || line[0] == '\n') continue;` add

```c
        if (strncmp(line, "arm pad", 7) == 0) { gp_arm_pad = 1; continue; }
```

(c) replace `gp_parse`'s return statement

```c
    return ok && have_frame && have_state && gp_n > 0u && gp_step[gp_n - 1u].op == 'e'
        && gp_step[0].op == 'k' && gp_step[0].f == gp_enter_frame
        && gp_step[0].a == 0x1Cu && gp_step[0].b == 0x0Du;   /* the mode-3 Enter (0x24ECF) */
```

with

```c
    if (!(ok && have_frame && have_state && gp_n > 0u && gp_step[gp_n - 1u].op == 'e')) return 0;
    if (gp_arm_pad)                                  /* the pad press's bits at the arm frame (0x11D04) */
        return gp_step[0].op == 'b' && gp_step[0].f == gp_enter_frame && gp_step[0].a != 0u;
    return gp_step[0].op == 'k' && gp_step[0].f == gp_enter_frame
        && gp_step[0].a == 0x1Cu && gp_step[0].b == 0x0Du;   /* the mode-3 Enter (0x24ECF) */
```

(d) in `test_gp_replay`, replace

```c
    CHECK_EQ_INT((int)mode_before, 3);               /* 0x24ECF: the Enter arm needs mode 3 */
    CHECK_EQ_INT((int)mode_after, 0x27);             /* 0x24EE0 */
```

with

```c
    CHECK_EQ_INT((int)mode_before, 3);               /* 0x24ECF / 0x25238: either arm needs mode 3 */
    /* 0x24EE0 stores 0x27 in the Enter's iteration; a pad arm leaves mode 3 one
     * iteration later, when the level (0x500C4) reaches 0x4F644 and 0x11D04. */
    CHECK_EQ_INT((int)mode_after, gp_arm_pad ? 3 : 0x27);
```

- [ ] **Step 3: Run it** — rebuild, rerun Step 1's command, then the long preview and the Enter regression:

```bash
make build > /dev/null
PR_GP_DUMP=$S/arm PR_GP_SCRIPT=$S/arm.script PR_GAME_DIR=data/game/C ./build/run_tests 2>&1 | grep -E 'FAIL|all checks|fn-miss PR_GP_DUMP' 
sed 's/^end 400/end 2600/' $S/arm.script > $S/arm_long.script; rm -rf $S/arm_long
PR_GP_DUMP=$S/arm_long PR_GP_SCRIPT=$S/arm_long.script PR_GAME_DIR=data/game/C ./build/run_tests > $S/arm_long.txt 2>&1
awk '{print $2, $3, $15, $18}' $S/arm_long/trace.txt | awk '{m=$2; if (m!=p) print; p=m}' | head -5
make gp-replay scenario=gp-pads GP_DUMP=$S/gpr 2>&1 | tail -1
```

Expected: the driver's own checks pass; the only `FAIL` lines are the miss-log ones (`test_platform.c`: `unexpected 0x29D60 from frontend_mode_1b_step` …), because `gp-u8-attract-start` has no pinned set until Task 6g (to `f = 400` the miss log holds the base pair and `0x29D60`, so the count line reads `3 != 2`). The long preview (planner run, and the re-baseline run on `8eaf25a`): `f=0140 mode=0003 cred=00000005 b1f=00`, `f=0141 mode=001A cred=00000004 b1f=01`, `f=0153 mode=001B`, `f=0165 mode=0010`; on `8eaf25a` it reaches mode 6 at `f=06B5` and misses, beyond the base pair, only `0x29D60` and `0x5D812` (both `frontend_mode_1b_step`; `0x23208` and `0x3A588`, which the `e9271df` preview missed, are ported by U6a); `gp-pads`: `all checks passed`. (Step 1's unmodified driver also prints the miss-count line `test_platform.c:193: 0 != 2`: the parse fails before `game_init`.)

- [ ] **Step 4: Mutation proof** — `CHECK_EQ_INT((int)mode_after, gp_arm_pad ? 3 : 0x27);` → `CHECK_EQ_INT((int)mode_after, 0x27);`, rebuild, rerun `$S/arm.script`: `FAIL …test_game.c:<line>: 3 != 39` (planner run; re-baseline run on `8eaf25a` with `$S/arm_long.script`: `test_game.c:12404: 3 != 39`). Restore, rebuild.

- [ ] **Step 5: Record, gate, commit** — §U8.15: the outputs; gate (`<n>` = 5; the `gp-pads` and `gp-idle-loss` replays inside `make verify` are the Enter regression).

```bash
git add port/tests/test_game.c docs/superpowers/plans/2026-10-01-gameplay-u8-derivations.md
git commit -m "$(cat <<'EOF'
tests: the PR_GP_DUMP driver accepts a pad arm (the attract start; record §U8.15)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 6 (D1): Capture, check, replay and pin one scenario — run as 6a … 6g

Each sub-task is one scenario, one review and one commit, in this order (6g only with D2):

| sub-task | `ID` | `SC` | row | expected path (record §U8.2–§U8.4) | `time_limit` | record |
|---|---|---|---|---|---|---|
| 6a | `RA` | `gp-u8-right-arcade` | 1 | `0x27`, `0x2E`, `0x1A` (`b1d 0`, `b1f 2`, cred 5 → 4), `0x1B`, `0x10`, …, 6 | 66 | §U8.16 |
| 6b | `LT` | `gp-u8-left-training` | 2 | `0x27`, `0x28`, `0x1A` (1, 3, cred 5), `0x1B`, `0x10`, …, 6 | 67 | §U8.17 |
| 6c | `RT` | `gp-u8-right-training` | 3 | `0x27`, `0x29`, `0x1A` (1, 3, 5), `0x1B`, `0x10`, …, 6 | 68 | §U8.18 |
| 6d | `TW` | `gp-u8-tug-of-war` | 4 | `0x27`, `0x2A`, `0x1A` (2, 3, 5), `0x1B`, `0x10`, …, 6 | 69 | §U8.19 |
| 6e | `HC` | `gp-u8-handicap` | 6 | `0x27`, `0x2C`, `0x1A` (4, 3, 5), `0x1B`, `0x10`, …, 6 | 66 | §U8.20 |
| 6f | `EN` | `gp-u8-endurance` | 5 | `0x27`, `0x2B`, `0x1A` (3, 3, 5), `0x1B`, `0x10` to the end | 46 | §U8.21 |
| 6g | `AS` | `gp-u8-attract-start` | — | 3 (F1 at F), `0x1A` from F + 1 (0, 1, 5 → 4), `0x1B`, `0x10`, …, 6 | 61 | §U8.22 |

**Files (each sub-task):**
- Writes (git-ignored): `data/k11-captures/<SC>/`
- Modify: `port/tests/test_platform.c` (6a: the scenario table and its first row; 6b–6g: one row each)
- Modify: `Makefile` (the pinned `GP_MODES_<ID>_*`, `GP_MODES_SCENARIOS += <ID>:<SC>`)
- Modify: record (the sub-task's section)

**Interfaces:**
- Consumes: Tasks 1–5; `make gp-capture`; `tools/gp_compare.py --report`.
- Produces: the capture; its pinned miss set; `GP_MODES_<ID>_MIN_FIRST`, `_TRACE_MIN_FIRST`, `_MAX_START`, `_CAPTURE_SHA256`, `_CAPTURE_FRAMES` (and `_END` only in the stall branch), each measured in Step 5 and pinned in Step 6 of the same sub-task.

Set the sub-task's values first, e.g. for 6a: `ID=RA; SC=gp-u8-right-arcade; LIMIT=66`.

- [ ] **Step 1: Capture**

```bash
make gp-capture scenario=$SC TITLE_PIN_DIR=/tmp/pr_u8_pin 2>&1 | tee $S/cap_$SC.txt | tail -12
du -sh data/k11-captures/$SC; cat data/k11-captures/$SC/session.txt
```

Expected: every `CHECK … ok` (6g: `mode left 3 after the pad arm: ok`), `steps fired n/n`, `end frame reached: ok`, the snapshot line, `wall` ≈ `LIMIT`. Branch — only `end frame reached` FAILs (the run went to `data/k11-captures/$SC.failed`): rerun once with `GP_ARGS="--time-limit $((LIMIT + 15))"` (U4 §G.18's rule, a harness value) and record both runs; if it fails again, or any other CHECK fails, stop the sub-task: record `session.txt` and `python3 tools/gp_modes.py path --capture data/k11-captures/$SC.failed`, leave the scenario unlisted, and report.

- [ ] **Step 2: The evidence that the capture reached its row**

```bash
python3 tools/gp_modes.py check --scenario $SC --capture data/k11-captures/$SC; echo rc=$?
python3 tools/gp_modes.py path --capture data/k11-captures/$SC | tee $S/path_$SC.txt
```

Expected: every line `ok`, `rc=0`, and the path of the table above. A FAIL means the capture did not reach the row: do not pin; record the failing line and the path, then decide from the evidence (a stimulus landing in a snapshot gap, §3.7, is rerun once; anything else is a correction of the record's expectation, recorded with its `poll.log:<n>`, and reported). A path that differs past `0x10` (another pre-fight sequence) is recorded as a correction, not a failure, as long as `check` is all `ok`.

- [ ] **Step 3: The replay and the miss set**

```bash
make build > /dev/null; mkdir -p $S/gp; rm -rf $S/gp/$SC
python3 tools/gp_session.py port-script --scenario $SC --capture data/k11-captures/$SC --out $S/gp/$SC.script
PR_GP_DUMP=$S/gp/$SC PR_GP_SCRIPT=$S/gp/$SC.script PR_GAME_DIR=data/game/C ./build/run_tests 2>&1 \
  | tee $S/replay_$SC.txt | grep -E 'fn-miss|FAIL|all checks|stalled|fault'
```

Expected: no driver FAIL other than the miss log's (`unexpected 0x… from …` and the count line): the scenario has no pinned set yet. Each `fn-miss PR_GP_DUMP 0x<addr> <ctx> hits=<n>` beyond the base pair (`0x5D812 actor_spawn`, `0x5D812 set_dead`) is a pair to pin; classify each from the raw in the record (the known ones, record §G.24: `0x29D60` a bare `ret`; `0x5D812` the runtime's `xor eax,eax; ret` stub; anything else: disassemble the target with `img.py` (record §U8.A) and name it — an unported callback is named with its table and owner, never registered here). `0x23208`, `0x3A588`, `0x3640C` and `0x37DCC` are ported on `8eaf25a` (U6a, record gameplay-u6 §U6.21) and U6b ports further callbacks before U8 runs: a miss of a registered address cannot occur, so a replay listing one is running a stale build. Re-baseline previews on `8eaf25a` (record §U8.3, hand-built scripts to `f = 2900`): rows 2, 3, 4 and 6 and the attract start miss only `0x29D60` and `0x5D812 frontend_mode_1b_step` beyond the base pair, row 5 (ENDURANCE, in the team select to the end) only `0x29D60`; row 1 also `0x14EF8` and `0x14F50` (`hit_reaction_apply`, 4 hits each) and `0x15510` (`anim_indirect`, 1 hit; not in the `e9271df` preview, cause not isolated). The pinned set is what Step 3 prints on the merged base, never these previews.

**Branch — stall or fault** (`stalled at f=…`, `fault …`, or `the gp script ran to its end frame` FAIL): as U4 Task 4: record the last `T` line of `$S/gp/$SC/trace.txt`; `F` = its `f`; regenerate the script with `--end F` and continue with it; Step 6 then also pins `GP_MODES_<ID>_END = <F>` with the evidence.

- [ ] **Step 4: Pin the miss set** — in `port/tests/test_platform.c`:

**6a only** — after `#define FNM_N(t) (sizeof (t) / sizeof (t)[0])` add the table with the right-arcade row (one `{ 0x<addr>u, "<ctx>" },` line per pair Step 3 printed beyond the base pair, in the printed order, each with its classification in the comment above):

```c
/* Gameplay U8 (record 2026-10-01-gameplay-u8 §U8.16): one pinned set per U8
 * scenario (an exact name), on top of the base pair, each pair measured on its
 * capture's replay and classified from the raw in the record. */
static const fnm_pair k_miss_gp_u8_right_arcade[] = {
    /* the pairs of $S/replay_gp-u8-right-arcade.txt, e.g. { 0x29D60u, "frontend_mode_1b_step" }, */
};
typedef struct { const char *name; const fnm_pair *t; u32 n; } fnm_scen;
static const fnm_scen k_miss_gp_scen[] = {
    { "gp-u8-right-arcade", k_miss_gp_u8_right_arcade, (u32)FNM_N(k_miss_gp_u8_right_arcade) },
};

static const fnm_scen *fnm_scen_find(const char *name)
{
    for (size_t i = 0; i < FNM_N(k_miss_gp_scen); i++)
        if (strcmp(k_miss_gp_scen[i].name, name) == 0) return &k_miss_gp_scen[i];
    return NULL;
}
```

and in `test_fn_misslog_driver` (planner-verified with an attract-start row; re-verified on `8eaf25a`, see below): after the flags' declaration line (on `8eaf25a`: `int idle_loss = 0, charsel = 0, cut = 0;`; U6b/U7/U11 may have added flags to it) add `const fnm_scen *scen = NULL;`; after `idle_loss = strncmp(sc, "gp-idle-loss", 12) == 0;` (and any flag lines that follow it, on `8eaf25a` `charsel = strcmp(sc, "gp-u5-charsel") == 0;`) add `scen = fnm_scen_find(sc);`; extend `want`'s last term (on `8eaf25a`: `(charsel ? (u32)FNM_N(k_miss_gp_charsel) : 0u);`) with `+ (scen != NULL ? scen->n : 0u)`; and append the scenario-table clause to the loop's `if (!fnm_known(...)) {` condition, keeping `fnm_known`'s argument list as the base has it (five arguments on `8eaf25a`; U6b, U11 may add one each). On `8eaf25a` that is: replace `if (!fnm_known(fn_misslog_addr(i), fn_misslog_ctx(i), frontend, idle_loss, charsel)) {` with

```c
        if (!fnm_known(fn_misslog_addr(i), fn_misslog_ctx(i), frontend, idle_loss, charsel)
            && !(scen != NULL && fnm_in(scen->t, scen->n, fn_misslog_addr(i), fn_misslog_ctx(i)))) {
```

(Re-baseline check on `8eaf25a`, a scratch copy of `port/` with Task 5's edits, these edits and a `gp-u8-attract-start` row of `0x29D60`/`0x5D812 frontend_mode_1b_step`: the `end 2600` pad-arm replay `distinct=4`, `all checks passed`; with the `0x5D812` row deleted `test_platform.c:214: 4 != 3` and `unexpected 0x5D812 from frontend_mode_1b_step`; the unit suite `all checks passed`.)

**6b–6g** — add `static const fnm_pair k_miss_gp_u8_<name>[] = { … };` above `fnm_scen` and one `{ "<SC>", k_miss_gp_u8_<name>, (u32)FNM_N(k_miss_gp_u8_<name>) },` row to `k_miss_gp_scen[]`. A scenario whose replay misses nothing beyond the base pair gets no row (C has no empty arrays; "a scenario with no entry here may miss only the base pair").

Rebuild and rerun Step 3's driver command. Expected: `all checks passed`.

Mutation (6a, and once more in any later sub-task whose set is non-empty): delete one pair from the new array, rebuild, rerun: `fn-miss PR_GP_DUMP: unexpected 0x… from …` and the count line `FAIL … <count> != <count − 1>`. Restore, rebuild.

- [ ] **Step 5: Measure the claims**

```bash
python3 tools/gp_compare.py --report --scenario $SC --capture data/k11-captures/$SC --port $S/gp/$SC | tee $S/report_$SC.txt | grep -E 'window|FIRST UNEXPLAINED|first difference|coverage'
shasum -a 256 data/k11-captures/$SC/poll.log; ls data/k11-captures/$SC/frame_*.raw.gz | wc -l
```

Record verbatim: the window start `w` (`window from capture <w>`), the first unexplained capture frame `j` (with its raw and nearest port frame), the first differing trace `f` (decimal `F`, field, values), the first tick difference, the sha256 and the frame count. Triage the first unexplained frame and the first trace difference exactly as U4 Task 4 Steps 2–3 (the PNG pair from `gp_compare.load_capture_frame`/`load_port_frame`; the `poll.log` and `trace.txt` lines at `F`; whether it is the end of the port's replay (the capture frame after the port's last frame, nearest port = the last `.ipx`: how far the port got, not a divergence — as `GP_CHARSEL_MIN_FIRST = 516` and Task 3 Step 4's 1070), `gp-idle-loss`'s remaining frame gap (the three-frame scan-out at the round-1 KO, capture 2064 at `f = 0xC71`..`0xC75`, a named gap in record gameplay-u6 §U6.21 and the Makefile's `GP_IDLE_LOSS_MIN_FIRST` comment), or new. U4's divergences 1 (the pick countdown, §G.20; fixed by U5's `0x37B03` operand fix, record gameplay-u5 §C5.12) and 2 (`0x23208`; ported by U6a) and §U6.8's divergence 2b (`f = 0x871`, absent on `8eaf25a`, §U6.21) are not expected on this base. A new divergence is named with its evidence and an owner (U6 moves, U7 two players, U11 in-match keys, or this unit's named gaps); none is fixed here. When the trace never differs, `F` = the port's last `T` `f` + 1 (U4's rule): that is the exact pin (`gp_compare.ratchet` fails `F + 1` as "N is unreachable"), so a later change that shortens the replay's end (a re-capture, a new `--end`) fails it and must re-measure and re-pin it from the new run, never set a value below what it measures. U8 takes one capture per scenario, so `F` has no run-to-run bound; U4's two runs agreed at every `f` from `0x625` (§G.19) — record that as the determinism evidence and its limit.

- [ ] **Step 6: Pin** — in the Makefile, before `GP_MODES_SCENARIOS =`, one block per scenario:

```make
# <SC> (record §U8.<n>): measured at <commit> on the capture below. MIN_FIRST: first unexplained
# capture frame <j> (raw <r>), <one-line cause>; TRACE_MIN_FIRST: first differing f=<hex> (<F>) in
# <field>, <cause>; MAX_START: the window starts at capture frame <w>. Raise N/F when they improve.
GP_MODES_<ID>_MIN_FIRST = <j>
GP_MODES_<ID>_TRACE_MIN_FIRST = <F>
GP_MODES_<ID>_MAX_START = <w>
GP_MODES_<ID>_CAPTURE_SHA256 = <poll.log sha256>
GP_MODES_<ID>_CAPTURE_FRAMES = <frame count>
```

and append `<ID>:<SC>` to `GP_MODES_SCENARIOS` (space-separated, in sub-task order).

- [ ] **Step 7: Green, then each pin fails**

```bash
make gp-modes-oracle GP_DUMP=$S/gpo > $S/o_$ID.txt 2>&1; echo "exit=$?"; grep -E "gp_modes: $SC|gp_compare: $SC" $S/o_$ID.txt | grep -E 'ratchet|FAIL|matches'
make gp-modes-oracle GP_DUMP=$S/gpo GP_MODES_${ID}_MIN_FIRST=$((<j> + 1)) > /dev/null 2>&1; echo "exit=$?"
make gp-modes-oracle GP_DUMP=$S/gpo GP_MODES_${ID}_TRACE_MIN_FIRST=$((<F> + 1)) > /dev/null 2>&1; echo "exit=$?"
make gp-modes-oracle GP_DUMP=$S/gpo GP_MODES_${ID}_CAPTURE_SHA256=0000 > /dev/null 2>&1; echo "exit=$?"
```

Expected: `exit=0` with `ratchet N <j> ok`, `ratchet N <F> ok`, `matches the pin`; then three `exit=2`. The damaged-frame proof as U4 Task 5 Step 3: `make gp-modes-one GP_MODES_ID=$ID scenario=$SC GP_DUMP=$S/gpo GP_MODES_KEEP=1`, copy `$S/gpo/$SC` to `$S/dmg`, flip byte 32000 of a port frame below the one nearest `j`, and `python3 tools/gp_compare.py --scenario $SC --capture data/k11-captures/$SC --port $S/dmg --min-first <j> --trace-min-first <F>` → a `FIRST UNEXPLAINED` below `<j>`, `rc=1`.

- [ ] **Step 8: Record, gate, commit** — the sub-task's section: Step 1's `session.txt` and size (the D1 estimate against the measured MB), Step 2's check and path, Step 3's miss lines and their classification, Step 5's lines and triage, Step 7's results. Gate (`<n>` = 6a … 6g).

```bash
git add Makefile port/tests/test_platform.c docs/superpowers/plans/2026-10-01-gameplay-u8-derivations.md
git commit -m "$(cat <<'EOF'
build: pin the <SC> frame and trace ratchets (record §U8.<n>)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
EOF
)"
```

(Stage `port/tests/test_platform.c` only when the sub-task changed it.)

---

### Task 7: U8 closure

**Files:**
- Modify: record (append §U8.23 closure and named gaps); `docs/PROGRESS.md` (append); `AGENTS.md` (one sentence)

- [ ] **Step 1: Record** — §U8.23: the table of scenarios (capture MB, frames, `w`, `j`, `F`, the first divergence and its owner), the D1 estimate against the measured total, the answers to spec Q5 (record §U8.8) and the credits question (§U8.2), the narrow claim (AGENTS.md's gameplay-oracle text with U8's values), and the named gaps: every row of §U8.9, every mode a capture reached past its N and F as "not covered", ENDURANCE's fight and the team pass `0x44798` (D3; owner unassigned: D3 named U5, but U5 merged without them and its record gameplay-u5 §C5.19 hands both to U8 — record that circular hand-off as is and leave the owner to the controller), the attract start's config-decode difference if its trace shows it, and any scenario left unlisted with its evidence.
- [ ] **Step 2: PROGRESS and AGENTS** — append a `docs/PROGRESS.md` paragraph "Gameplay U8: the other START MENU rows and the attract start …" (scenarios, sizes, pins, first divergences, named gaps). In `AGENTS.md`'s gameplay-oracle paragraph add: "`make gp-modes-oracle` (in `make verify`) applies the same two ratchets to the U8 captures `data/k11-captures/gp-u8-*` (the other START MENU rows and the attract start), each with `GP_MODES_<ID>_*` values and provenance in the Makefile and an evidence check (`tools/gp_modes.py`) that the capture reached its row; each skips without its capture."
- [ ] **Step 3: Final gate and commit** — gate (`<n>` = 7); `grep -E 'gp_compare: gp-u8' $S/t7_verify.txt` shows every pinned scenario's two `ratchet … ok` lines.

```bash
git add docs/superpowers/plans/2026-10-01-gameplay-u8-derivations.md docs/PROGRESS.md AGENTS.md
git commit -m "$(cat <<'EOF'
docs: gameplay U8 closure: the other modes, their ratchets and named gaps (record §U8.23)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
EOF
)"
```

---

## Execution notes

- **Order:** 0 → 1 → 2 → 3 (no decision needed) → 4 → 5 (D2) → D1 → 6a 6b 6c 6d 6e 6f (6g after 4–5) → 7. U5 and U6a are merged (`8eaf25a`) and U8 runs after U6b and U7, so every Task 6 value (N, F, window start, miss set) is measured on that merged base in its own sub-task, never taken from the previews. 6a must precede the others (it lands the miss-set table). If another unit has already merged a per-scenario miss-set table into `test_platform.c`, 6a adds its row to that table instead of creating `k_miss_gp_scen` (on `8eaf25a` none has: U5 used a `charsel` flag; U6b, U7 and U11 plan flags too).
- **Dependencies on units that merge before U8** (each re-checked at Task 0): U6b appends five bytes to `SNAP_FIELDS` and the port's `T` line (`r0 r1 c0 c1 s0_43`, its Task 1) — Task 1's `_s` helper builds every field from `gs.SNAP_FIELDS`, `TRACE_FIELDS` is unchanged, and U6b's moves claim in `gp_compare.py` runs only with `--moves-min-first` (or in `--report` when the capture has the fields), so `gp-modes-one` is unaffected and a `--report` in Task 6 Step 5 may print an extra, unratcheted moves line; U6b/U11 add `fnm_known` parameters and U7 a driver flag (Task 6 Step 4's anchors say how to keep them); U6b ports further callbacks, which can only shrink a replay's miss set.
- **Model tiers:** Tasks 0–3 and 7: a standard model (mechanical, code given). Tasks 4–5: standard, with the reviewer checking backward compatibility (Review Focus 3). Tasks 6a–6g: the strongest model available (captures, triage and classification of misses and divergences need judgement under the evidence rules).
- **Time:** each capture is its `time_limit` (46–69 s) plus encoding; each replay ≈ 40–50 s; `make verify` grows by ≈ 42 s per pinned scenario (record §U8.6–§U8.7).
- **Gate:** Tasks 0 and 7 (and the pre-merge check) end with the full gate block (verify-exit 0, ORACLES-EQUAL, WAV-IDENTICAL, the `port_progress.py` lines of Task 0); Tasks 1–6g with the per-task gate ("Where to run"); a sub-task that cannot pin (a FAIL in Step 1 or 2) still commits its record section and leaves its scenario unlisted.
- **What the re-baseline ran on `8eaf25a`** (record §U8.3, §U8.7 "re-baseline"): Tasks 1, 2 and 4's code blocks extracted from this plan into a scratch copy of `tools/` (`test_gp_modes` 9, then 16 tests `OK`; with `test_gp_session`, `test_gp_capture`, `test_gp_compare` 83 tests `OK`; the Task 1 CLI's eight `ok` lines and seven `FAIL` lines; the `gp-pads` and the cut `gp-idle-loss` port scripts byte-identical before and after Task 4); Task 5's and Task 6a's C edits in a scratch copy of `port/` (the Step 1 parse failure, `3 != 2`, the long preview, `gp-pads` `all checks passed`, the `3 != 39` and dropped-pair mutations, the unit suite `all checks passed`); Task 3 Step 4's measurement block, its three ratchet values and both mutations through `gp_compare.py` directly (the Makefile targets do not exist before Task 3; the replay half is `make gp-replay … GP_SCRIPT_ARGS="--end 2325"`, which was run); `make diff-verify`; the six row previews.
- **What the planner ran** (record §U8.0–§U8.7): every code block in Tasks 1–5 and the Task 6a miss-set mechanism, in a scratch copy of `e9271df`: the 16 `test_gp_modes` tests, the U1–U4 tool suites, the seven mutations, the empty/skip/reference runs of `gp-modes-oracle`, the pad-arm driver run, its mutation (`3 != 39`), the `gp-pads` replay (`all checks passed`), and the full `run_tests` unit suite (`all checks passed`).
