# Gameplay U11 — In-Match Keys Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

## Decisions needed from the user

**Decided by the user on 2026-10-01:** all recommendations accepted — D1 the `gp-keys-fight` capture (about 70 MB, 90 s) approved; D2 the windowed host's key binding is fixed in U11.

1. **D1 — Take the `gp-keys-fight` capture (Tasks 5–6).** One DOSBox-X run of 90 s wall, written only by `make gp-capture` to the git-ignored `data/k11-captures/gp-keys-fight/`; estimated **about 70 MB** (gp-idle-loss's frames up to the same point, raw ≤ 4034, are 45.2 MB; its post-restart movie tail is 22.0 MB; poll.log about 1.5 MB). **Recommendation: yes.** Cost of "no": U11 ships the tools, the port seam and a `gp-keys-oracle` that skips; no in-match key has capture evidence and the spec's G exit criterion ("U11 captured and ratcheted") stays open. Cost of "yes": 70 MB of disk and one 90 s run.
2. **D2 — Fix the windowed host's key binding in U11 (Task 7).** `host.c`'s `k_input_bind` maps SDL keys to the wrong bitmap bits (Up presses P2's b3, '5' P2's start; record §K.7). **Recommendation: yes**, as Task 7: a pure `host_kb_bit(scan)` with the raw default binding, tested headless. Cost of "no": the windowed port keeps unplayable controls until the Closeout's O10, which then needs this same work; cost of "yes": `port/src/host.c`/`host.h` change (no oracle can move: every driver uses the override seam).

Without D1, run Tasks 0–4, 7 (if D2) and 8; the record then says the capture was not taken.

## Re-baseline (2026-10-01, main 8eaf25a)

Planned on `e9271df`; U5 (`b09b9e6`) and U6a (`8eaf25a`) merged since. Every changed code block was re-run on a scratch copy of `main` `8eaf25a` (`git archive`): the plan's patches, extracted from this file, apply with `git apply` in the order Tasks 1, 3, 4, 7, 6 and in the order 1, 3, 4, 6, 7, and the result is byte-identical to the scratch tree every check below ran on. Not run: any DOSBox-X capture (Tasks 5–6 still need D1's run).

| # | task / step | old → new | source |
|---|---|---|---|
| 1 | Where to run | base `e9271df` → `8eaf25a` | `git log` |
| 2 | Task 0 Step 1 | gp-idle-loss `ratchet N 203 ok` / `first differing 2088, ratchet N 2088 ok` → `frames: first unexplained 2064, ratchet N 2064 ok` / `trace: 0 differing through 8319; ratchet N 8320 ok`; adds the `gp-u5-charsel` lines (516, 1513) and `diff-verify: 6/6 … 7/7`; tool-test `Ran 104` → `Ran 107` | Makefile `GP_IDLE_LOSS_*`, record gameplay-u6 §U6.21; `make gp-oracle gp-charsel-oracle` and the tool-test line run on `8eaf25a` |
| 3 | Task 1 Step 3 | the scenario hunk `@@ -125,6` (context: after `SCENARIOS['gp-idle-loss-run2']`) → `@@ -147,6` (context: the end of U5's `SCENARIOS['gp-u5-charsel']` block) | U5 inserted its block at the old spot (`git apply --check` failed there); the new spot is also clear of U6b's insertion after `gp-idle-loss-run2` |
| 4 | Task 1 Steps 4, 6 | `Ran 38 tests` → `Ran 41 tests` | U5 added 3 tests to `test_gp_session`; measured; S1–S4 re-run, each fails as listed |
| 5 | Task 2 Steps 4, 6 | `Ran 77 tests` → `Ran 80 tests` | measured; M1–M9 re-run, each fails as listed |
| 6 | Task 3 Files, patch, Steps 2 and 5 | `test_game.c` hunks +33 lines (`12256/12266/12324/12374/12651/12662` → `12289/12299/12357/12407/12684/12695`); Step 2 `test_game.c:12378` → `:12411`; P1's lines now concrete (`12710`, `12422`) | U5's ticks C/D in `check_idle_tick_37a58` (`test_game.c:4759`); Steps 2, 4, 5 re-run: `1849 != 1850` before, `1 restart(s) landed`, `PR_RESTART` passes, `11 of 11`, `ratchet N 11 ok`, §K.8's table unchanged, P1–P3 as listed |
| 7 | Task 4 patch, Files, Steps 4–5 | `.PHONY`'s last line now ends `diff-verify gp-charsel-oracle` (U5): `gp-keys-oracle` appended after it; the recipe after `gp-charsel-oracle`'s (was after `gp-oracle`'s); `verify` calls it after `gp-charsel-oracle`; `Ran 117` → `Ran 120` (107 + 13) | Makefile on `8eaf25a`; Step 3's skip lines and the help line re-run, unchanged |
| 8 | Task 6 Interfaces, Step 1, Step 2 patch and mutation | `fnm_known(…, idle_loss, keys_fight)` → `fnm_known(…, idle_loss, charsel, keys_fight)` (U5's `charsel` is the 5th parameter, `keys_fight` the 6th; signature, driver call and the `--check` call `0, 0, 0, 0` all updated); table after `k_miss_gp_charsel[]`; rows `0x29D60`, `0x5D812` (frontend) only: the `0x3A588 fighter_state_3531c` row is dropped; mutation "delete `0x3A588` → `5 != 4`" → "delete `0x29D60` → `test_platform.c:204: 4 != 3`, `unexpected 0x29D60 from frontend_mode_1b_step`"; a miss of `0x23208`/`0x3A588`/`0x3640C`/`0x37DCC` is now a regression | U6a registered `0x3A588` (and `0x23208 0x3640C 0x37DCC`), record gameplay-u6 §U6.21; the dry run re-run on `8eaf25a` (cut and as `gp-keys-fight`): `distinct=4`, `all checks passed`; the mutation run measured |
| 9 | Task 6 Step 6 | "frame claim stops at the character select, O1; trace's first difference at or before `f = 0x828`, O2" → measured values; on `8eaf25a` the gp-idle-loss path holds to capture 2064 / `f = 0x207F`, so an earlier first difference is a finding | record gameplay-u6 §U6.21 |
| 10 | Task 7 Step 1 patch | test hunk `@@ -2614` → `@@ -2611` | U5's `k_miss` edits above `test_host`; Steps 1–3 re-run: the build error, `all checks passed`, `256 != 1`, `1 != 0` ×3 |
| 11 | Gates (Tasks 3, 4, 6, 7; Execution notes) | full `make verify` after each → the light gate below; the full gate at Task 0, Task 8 and before merge | the user's speed-up ruling (2026-10-01) |
| 12 | Record §K.6, §K.8, §K.10 item 2 | re-baseline notes: line `12411`, the miss set without `0x3A588`, the gp-idle-loss lines `2064`/`8320`, the frame claim now past round 1 | the runs above |

The full `make verify` on the scratch tree with every patch applied (Tasks 1–4, 6 with the two measured rows, 7) exited 0 with every gate line as expected (Execution notes).

**Warnings for the implementer.**

- **`GP_IDLE_LOSS_TRACE_MIN_FIRST = 8320` is an exact pin** (no traced difference through `f = 0x207F`, the replay's end; gp_compare fails an N above the end as unreachable). U11 changes neither the gp-idle-loss script nor its end, and its `T`-line fields are outside gp_compare's `TRACE_FIELDS`: with every patch applied the `gp_compare: gp-idle-loss` and `gp-u5-charsel` lines are byte-identical to `8eaf25a`'s (the replays only add `0 restart(s) landed`). A change that alters that replay's script or end must re-measure and re-pin 8320, and must never lower `GP_IDLE_LOSS_MIN_FIRST` 2064 below what it measures.
- **Round 1 of the gp-idle-loss path is a CPU KO at the capture's `f = 0xC71`** (record §U6.21; divergence 2b, §U6.8 `f = 0x871`, does not occur on this base). U11's events run from about `0x7FF` to `0x86D` (dry run 2047–2157), inside round 1.
- **No function is ported** (`host_kb_bit` and the seam carry `PORT:` headers): `port_progress.py` stays `771 1203 64` / `731 731 100`, and E2's triage table needs no regeneration.
- **Merge order with U6b (parallel worktree): U6b's Task 1 (the snapshot bytes) first, U11 second.** Shared edits: `tools/gp_session.py` — no textual overlap (U6b: `SNAP_FIELDS`' tail, `MOVE_FIELDS` after `TRACE_FIELDS`, its scenario after `gp-idle-loss-run2`; U11: `KEYS.update` after `PAD`, `format_s`'s optional `fields`, its scenario after U5's block; U6b's `gp_moves` calls `format_s` with five arguments, which the default keeps); `tools/gp_capture.py` — U11 only; `tools/gp_compare.py` — U6b only. **The conflict is `port/tests/test_game.c` `gp_trace_line`**: both append fields at the same two lines, and U6b's `test_the_port_t_line_writes_every_snap_field_in_order` (`tools/tests/test_gp_session.py`) requires the `T` names to equal `SNAP_FIELDS`, which U11's `lat spz mpz` (in `KEYS_EXTRA`, not `SNAP_FIELDS`) break. Resolution when U11 merges second: the `T` format ends `ent=%08X r0=%02X r1=%02X c0=%02X c1=%02X s0_43=%02X lat=%08X spz=%02X mpz=%02X\n` with U6b's five arguments before U11's three, and U6b's test expects `[n for n, _, _ in gs.SNAP_FIELDS + gs.KEYS_EXTRA]` (the `T` line is `SNAP_FIELDS` then `KEYS_EXTRA`, the order `gp-keys-fight`'s `S` lines have); U11 stages `tools/tests/test_gp_session.py` in the merge commit and re-runs that test and U6b's Task 1 Step 5 mutation. If U11 is ready first the mirror applies: U6b inserts its five fields between `ent` and `lat` and writes its test with `+ gs.KEYS_EXTRA`. Also: `Makefile` (the tool-test line: keep `test_gp_moves` and `test_gp_keys`; `.PHONY` and `verify`: keep both targets) and `test_platform.c` (`fnm_known`: `charsel` 5th, then each unit's flag in merge order, every call site updated). U6b's bytes in a `gp-keys-fight` capture taken after U6b's Task 1 are harmless (`gp_keys` reads fields by name; U6b's tools tolerate their absence in an earlier one), so neither order forces a re-capture or a re-pin.

**The light gate** (Tasks 3, 4, 6, 7; drop `gp-keys-oracle` in Task 3, which runs before Task 4 adds it):

```bash
T=u11_<t>
PR_GAME_DIR=data/game/C ./build/run_tests 2>&1 | tail -1
PR_RESTART=1 PR_GAME_DIR=data/game/C ./build/run_tests 2>&1 | tail -1
make gp-oracle gp-charsel-oracle gp-keys-oracle GP_DUMP=/tmp/pr_${T}_gp > /tmp/pr_${T}_gp.log 2>&1; echo gp-exit=$?
grep '^gp_compare: gp-idle-loss' /tmp/pr_${T}_gp.log | diff - $S/gp_idle_loss_base.txt && echo GP-IDLE-LOSS-EQUAL
grep -E 'landed|ratchet N|^gp_keys:' /tmp/pr_${T}_gp.log
make diff-verify DIFF_IMAGE=/tmp/pr_${T}_diffimg DIFF_TABLE=/tmp/pr_${T}_diff.md 2>&1 | grep '^diff-verify:'
PR_ORACLE_REQUIRED=1 python3 -m unittest tools.tests.test_k11_fields tools.tests.test_k11_session tools.tests.test_k11_capture \
  tools.tests.test_k11_compare tools.tests.test_gp_session tools.tests.test_gp_capture tools.tests.test_gp_compare tools.tests.test_gp_keys 2>&1 | grep '^Ran'
python3 tools/port_progress.py
```

Expected: `all checks passed` twice, `gp-exit=0`, `GP-IDLE-LOSS-EQUAL`, the ratchets `2064`, `8320`, `516`, `1513` ok, `diff-verify: 6/6 functions VERIFIED…`, `Ran 120 tests`, `771 1203 64`, `731 731 100`.

**Goal:** Derive every key the game acts on during a match from the raw, capture the original doing each one in round 1 of the `gp-idle-loss` path (Enter, the pause, ESC's ABANDON CONQUEST? with N and with Y, Alt-Q's QUIT TO DOS? with N, Alt-S and Alt-M twice, F1, F2's join), check the capture shows each raw-derived effect, replay it in the port, and ratchet the number of events the port reproduces in `make verify`.

**Architecture:** One scenario, `gp-keys-fight`, in `tools/gp_session.py`, with three extra snapshot fields (the key-loop latch and the two pause bytes) logged by `tools/gp_capture.py` for this scenario only and by the port driver's `T` line always. A new stdlib tool, `tools/gp_keys.py`, judges each event twice from the same frames: `evidence` on the capture, `effects` on the port's trace (a ratchet). The port gains one test seam (`game_restart_landings()`) so the replay driver accepts the iteration the ABANDON restart abandons; `make gp-keys-oracle` joins `make verify` and skips without the capture.

**Tech Stack:** Python 3 stdlib (`unittest`), C11 (`CHECK`/`CHECK_EQ_INT`), DOSBox-X 2026.08.31, the U1–U4 gameplay tools, `make`.

**Spec:** `docs/superpowers/specs/2026-09-30-reverse-completion-design.md` §4 (track G, U11), §6 (G exit criterion), §7 (named gaps); `docs/superpowers/specs/2026-09-30-gameplay-ground-truth-design.md` §2 (U11 out of U1–U4 scope), §3.1–§3.2, §4.1–§4.3, §5, §7 Q2/Q4/Q5.

**Derivation record:** `docs/superpowers/plans/2026-10-01-gameplay-u11-derivations.md` (§K.0–§K.10 written by the planner; Tasks append §K.11 tool log, §K.12 capture/replay/pins, §K.13 host binding, §K.14 closure). Read §K.2–§K.8 before Task 1.

---

## Global Constraints

- AGENTS.md, evidence discipline: "**Never ship a fitted constant.** Every value is derived from the raw bytes or a capture, with the address that proves it. A value that cannot be pinned is a **named gap with its evidence** — never a plausible-looking number." and "**On any plan-vs-raw conflict the raw wins.** Record the correction and the address, in the derivation record and the report."
- Gameplay spec §5: "Harness values (`HOLD_FRAMES`, the 150-frame gaps, `time_limit`, …) are named as harness values with their source, never presented as game values." This plan's harness values: the 10- and 20-frame gaps, `time_limit = 90`, `LOOKAHEAD = 8` (record §K.6).
- `make verify` is THE gate: the 45 oracle lines (`grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|== demo-fight)' <log>`) must equal `.superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt`; the `make audio-render` WAV must be byte-identical to `.superpowers/sdd/2026-09-29-k7-k12/scratch/before-t2.wav`; `python3 tools/port_progress.py` prints `771 1203 64` and `731 731 100` (U11 ports no function; `host_kb_bit` and the seam carry `PORT:` headers, not `/* 0xADDR` ones). The `gp_compare: gp-idle-loss` lines must equal Task 0's.
- AGENTS.md, tests: "Only `CHECK(cond,msg)` and `CHECK_EQ_INT(a,b)`." "**`game_init()` may run only once per process**" (the replay is the env-gated `PR_GP_DUMP` driver). "**Assertions must be able to fail.** Seed sentinels that differ from the post-conditions, prove a new assertion fails under a mutation of the code it tests, and never assert an unseeded BSS-zero." Python tests are `unittest` in `tools/tests/`, importing with `sys.path.insert(0, ROOT/tools)`.
- AGENTS.md, porting: "Mark deliberate deviations `/* PORT: ... */`… No other comment styles in `port/src`." "SDL and file/asset I/O live **only** in `port/src/host.c` and `main.c`." "**`port/src/symbols.h` is generated**… Never hand-edit it."
- AGENTS.md: "Every env-gated driver and `--check` run arms `fn_resolve`'s miss log and must record exactly the pinned known-set… A port that makes a driver reach a new unregistered code pointer fails it: register the target or pin the miss with its evidence."
- AGENTS.md: "Captures are git-ignored. `data/` is git-ignored and read-only — never write to it." Only `make gp-capture` writes, to `data/k11-captures/gp-keys-fight/` (`gp_capture.guard_gp`). Gp captures skip in `make verify` when absent, even under `PR_ORACLE_REQUIRED` (gameplay spec §4.3).
- AGENTS.md: "Commit style: `<area>: <what changed>`. **Never `git add -A`** — stage named files." Trailer: `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. Do not stage the `data` and `.superpowers` symlinks.
- Parallel units (common brief): U5–U8 and U11 merge separately; every shared-file edit below is additive and listed under **Shared-file touch points**. U5 and U6a are merged (main `8eaf25a`); U6b runs in parallel with U11 (see the Re-baseline's merge order).

## Review Focus

1. **An event that passes without happening.** The latch keeps its value between keys in a fight (record §K.3), so a rule that only reads the latch after the key would pass on a stale value. Every latch rule requires the value before the key to differ (`test_an_unchanged_latch_cannot_show_an_event`), the scenario's order makes every event change it (record §K.6), and the restart rule requires the abandoned iteration to have no record (`test_a_restart_must_abandon_its_iteration`).
2. **An answer consumed after its opener's frame** (a blocking reader waiting with `f` frozen, spec §7 Q5). Each event's words must share one consumption frame (`test_an_answer_read_a_frame_later_fails`); the capture's own `port script v2` check rejects a word whose `S(c)` disagrees (record §G.4).
3. **The restart breaks the replay's one-line-per-frame check, or a seam that hides real skips.** The seam counts landings only; `test_restart_drive` asserts it counts its one landing; the dry run (Task 3) fails without it (`1849 != 1850`) and both checks fail when the increment is deleted.
4. **A ratchet pinned where it cannot fail, or on another capture.** `gp_keys.py effects` fails unpinned, above the event count, and on another `poll.log` (`test_the_ratchet_fails_below_n_and_unpinned`, `test_another_capture_fails_the_pin`); Task 6 proves the pinned N fails under a port mutation (the pause's `config_screen_wait` deleted).
5. **Reading green as "the keys are right".** The claim is the eleven state changes of record §K.6 at the capture's frames, nothing more; the pause/prompt frames, QUIT TO DOS's yes, Alt-J, a pause left open and the physical keyboard are named gaps (record §K.10), restated in Task 8.

## Where to run

Branch `u11-keys` in `.worktrees/u11-keys`, off `main` (`8eaf25a` after the re-baseline; planned on `e9271df`). One-time setup, from the main checkout:

```bash
cd /Users/felipe.dos.santos/code/mine/primal-rage-reverse
git worktree add .worktrees/u11-keys -b u11-keys main
cd .worktrees/u11-keys
ln -s ../../data data
ln -s ../../.superpowers .superpowers
cp ../../port/tests/ghidra_data.bin ../../port/tests/title_screen_ref.ppm port/tests/
ls -d data/game/C data/k11-captures/gp-idle-loss data/k11-captures/gp-pads .superpowers/sdd port/tests/ghidra_data.bin port/tests/title_screen_ref.ppm
```

Every later command runs from `.worktrees/u11-keys`. Scratch: `S=/tmp/pr_u11` (`mkdir -p $S`). Ledger: `.superpowers/sdd/2026-10-01-gameplay-u11-in-match-keys/progress.md`. The gate (used by several tasks; `<t>` is the task tag):

```bash
T=u11_<t>
make verify SMK_DUMP=/tmp/pr_${T}_smk TITLE_DUMP=/tmp/pr_${T}_title ATTRACT_DUMP=/tmp/pr_${T}_att \
  FRONTEND_DUMP=/tmp/pr_${T}_fe TITLE_PIN_DIR=/tmp/pr_${T}_pin AUDIO_WAV=/tmp/pr_${T}.wav \
  K11_DUMP=/tmp/pr_${T}_k11 GP_DUMP=/tmp/pr_${T}_gp DIFF_IMAGE=/tmp/pr_${T}_diffimg \
  DIFF_TABLE=/tmp/pr_${T}_diff.md > /tmp/pr_${T}_verify.log 2>&1; echo verify-exit=$?
grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|== demo-fight)' /tmp/pr_${T}_verify.log \
  | diff - .superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt && echo ORACLES-EQUAL
make audio-render AUDIO_WAV=/tmp/pr_${T}.wav > /dev/null 2>&1; cmp /tmp/pr_${T}.wav .superpowers/sdd/2026-09-29-k7-k12/scratch/before-t2.wav && echo WAV-IDENTICAL
grep '^gp_compare: gp-idle-loss' /tmp/pr_${T}_verify.log | diff - $S/gp_idle_loss_base.txt && echo GP-IDLE-LOSS-EQUAL
grep -E '^gp_keys:|^Ran [0-9]+ tests' /tmp/pr_${T}_verify.log
python3 tools/port_progress.py
```

## Shared-file touch points

All additive; each is the whole edit to that file.

| File | Region | What is added | Merge note |
|---|---|---|---|
| `tools/gp_session.py` | after the `PAD` dict | `KEYS.update({...6 keys...})` | none expected |
| `tools/gp_session.py` | `format_s` | optional `fields=SNAP_FIELDS` parameter (default keeps every existing caller) | a unit that also edits `format_s` keeps both |
| `tools/gp_session.py` | after U5's `SCENARIOS['gp-u5-charsel']` block (the last before `def expand`) | `KEYS_EXTRA`, `SCENARIOS['gp-keys-fight']` (key `extra`) | U6b inserts its scenario after `SCENARIOS['gp-idle-loss-run2']`, above U5's block: no textual overlap |
| `tools/gp_capture.py` | `read_snap`, `Poller.__init__`, the `v2` read and `format_s` call, `main`'s `Poller(...)` | the optional `fields` path (`SNAP_FIELDS + scn.get('extra', ())`) | backward compatible: a scenario without `extra` logs exactly as before |
| `port/tests/test_game.c` | `gp_trace_line` (12287–12304) | three fields appended to every `T` line: `lat spz mpz` | **conflicts with U6b** (five fields appended at the same lines, and U6b's `test_the_port_t_line_writes_every_snap_field_in_order` requires the `T` names to equal `SNAP_FIELDS`): resolve as the Re-baseline's merge order says |
| `port/tests/test_game.c` | `test_gp_replay` (the `gp_trace_lines` check, 12409–12411) and `test_restart_drive` (12684–12698) | the landing count; one `CHECK_EQ_INT` | — |
| `port/src/game/flow.c`, `flow.h` | after `s_restart_point`; `game_loop`'s landing branch | `game_restart_landings()` seam (`PORT:`) | — |
| `port/tests/test_platform.c` | after `k_miss_gp_charsel[]`; `fnm_known`; `test_fn_misslog_driver`; the `--check` call | `k_miss_gp_keys_fight[]` and a `keys_fight` flag, `fnm_known`'s 6th parameter after U5's `charsel` (Task 6) | U6b (`moves`), U7 (`twop`) and U8 (a name-keyed table) edit the same spots: keep every flag and table, each a further parameter in merge order; if U8's name-keyed table lands first, register `gp-keys-fight` there instead of the flag. `0x3A588` is already registered (U6a, record gameplay-u6 §U6.21), so the table has no such row |
| `port/tests/test_platform.c` | top of `test_host` | the `host_kb_bit` checks (Task 7, D2) | — |
| `port/src/host.c`, `host.h` | `k_input_bind` → `k_bios_pad` + `host_kb_bit`; `host_key_bits`'s loop | Task 7 (D2) | no sibling plan edits `host.c` |
| `Makefile` | `.PHONY` (its last line now ends `diff-verify gp-charsel-oracle`); after the `gp-charsel-oracle` recipe; `verify` (after `gp-charsel-oracle`; the tool-test line) | `GP_KEYS_*`, `gp-keys-oracle`, `tools.tests.test_gp_keys` | U6b/U7/U8 add their own targets beside it; the tool-test line (U6b appends `tools.tests.test_gp_moves`): keep every added module |
| `docs/PROGRESS.md` | end | one paragraph (Task 8) | append-only |

New files: `tools/gp_keys.py`, `tools/tests/test_gp_keys.py`, the record (exists).

---

### Task 0: Baseline

**Files:** none changed. Writes `$S/gp_idle_loss_base.txt` and the ledger.

**Interfaces:** Produces the baseline the gate compares against.

- [ ] **Step 1: Build and run the baseline gate**

Run the setup of "Where to run", then `cmake -S port -B build && cmake --build build 2>&1 | tail -2` and the gate with `<t>` = `t0`, except that the `GP-IDLE-LOSS-EQUAL` line is replaced by:

```bash
mkdir -p $S; grep '^gp_compare: gp-idle-loss' /tmp/pr_u11_t0_verify.log > $S/gp_idle_loss_base.txt; cat $S/gp_idle_loss_base.txt
```

Expected: `verify-exit=0`, `ORACLES-EQUAL`, `WAV-IDENTICAL`, the eight `gp_compare: gp-idle-loss` lines ending `frames: first unexplained 2064, ratchet N 2064 ok` and `trace: 0 differing through 8319; ratchet N 8320 ok` (Makefile `GP_IDLE_LOSS_MIN_FIRST`/`GP_IDLE_LOSS_TRACE_MIN_FIRST`, record gameplay-u6 §U6.21), the `gp_compare: gp-u5-charsel` lines ending `ratchet N 516 ok` and `0 differing through 1512; ratchet N 1513 ok`, `diff-verify: 6/6 functions VERIFIED; 7/7 mutants detected`, the tool-test line `Ran 107 tests`, `771 1203 64`, `731 731 100`. If a capture is absent from `data/`, its oracle prints its skip line: record which.

- [ ] **Step 2: Ledger**

Create `.superpowers/sdd/2026-10-01-gameplay-u11-in-match-keys/progress.md` with the Step 1 results (verify-exit, the four checks, the tool-test count). No commit.

---

### Task 1: The in-match keys, the extra snapshot fields and the scenario

**Files:**
- Create: `tools/tests/test_gp_keys.py` (Task 2 replaces it with the full file)
- Modify: `tools/gp_session.py` (after `PAD`; `format_s`; after U5's `SCENARIOS['gp-u5-charsel']` block)
- Modify: `tools/gp_capture.py` (`read_snap`, `Poller`, `main`)
- Modify: record §K.11

**Interfaces:**
- Consumes: `gp_session.Schedule`, `SNAP_FIELDS`, `KEYS`, `PAD`, `format_s`, `parse`; `gp_capture.read_snap`, `Poller`.
- Produces: `KEYS['space'|'y'|'n'|'alt-q'|'alt-s'|'alt-m']`; `gp_session.KEYS_EXTRA`; `SCENARIOS['gp-keys-fight']` (with key `extra`); `format_s(ms, vals, kb, head, tail, fields=SNAP_FIELDS)`; `gp_capture.read_snap(mm, base, fields=gs.SNAP_FIELDS)`; `Poller(..., fields=gs.SNAP_FIELDS)`.

- [ ] **Step 1: Write the failing test** — create `tools/tests/test_gp_keys.py`:

```python
# tools/tests/test_gp_keys.py — U11 in-match keys (record 2026-10-01-gameplay-u11 §K)
import os
import sys
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import gp_capture as gc
import gp_session as gs

FIELDS = gs.SNAP_FIELDS + gs.SCENARIOS['gp-keys-fight']['extra']
STEPS = gs.SCENARIOS['gp-keys-fight']['steps']
BASE = 0x266000


class TestKeys(unittest.TestCase):
    def test_keys_are_bios_make_words(self):
        # scan << 8 | ascii; an Alt-letter has ascii 0 (the 0x24D96 arms)
        self.assertEqual([gs.KEYS[k] for k in ('space', 'y', 'n', 'alt-q', 'alt-s', 'alt-m')],
                         [(0x39, 0x3920), (0x15, 0x1579), (0x31, 0x316E),
                          (0x10, 0x1000), (0x1F, 0x1F00), (0x32, 0x3200)])
        self.assertEqual(gs.KEYS['enter'], (0x1C, 0x1C0D))         # unchanged
        self.assertEqual(gs.KEYS['esc'], (0x01, 0x011B))

    def test_an_answer_fires_with_its_opener(self):
        s = gs.Schedule(STEPS)
        s.due_boot(gs.ENTER_WAIT)
        s.on_mode(0x141, 0x27)
        s.due(0x141 + 149)
        s.due(0x141 + 299)
        s.on_mode(0x7F5, 0x06)
        self.assertEqual([x[0] for x in s.due(0x7F5 + 9)], [3])
        self.assertEqual([x[0] for x in s.due(0x7F5 + 19)], [4, 5])      # space, space in one spin
        self.assertEqual([x[0] for x in s.due(0x7F5 + 29)], [6])
        self.assertEqual([x[0] for x in s.due(0x7F5 + 39)], [7, 8])      # esc, n
        self.assertEqual(s.total, 18)

    def test_extra_fields_are_read_and_logged(self):
        m = bytearray(0x400000)
        o = BASE + 0x105F30 - gs.DATA_BASE_VA
        m[o:o + 4] = (0x1B).to_bytes(4, 'little')
        m[BASE + 0x1028DA - gs.DATA_BASE_VA] = 1
        v = gc.read_snap(m, BASE, FIELDS)
        self.assertEqual((v['lat'], v['mpz'], v['spz']), (0x1B, 1, 0))
        self.assertNotIn('lat', gc.read_snap(m, BASE))                    # the default is unchanged
        rec = gs.parse(gs.format_s(0, dict(v, f=7), 0, 0x1E, 0x1E, FIELDS))
        self.assertEqual((rec['lat'], rec['mpz']), (0x1B, 1))
        self.assertNotIn('lat', gs.parse(gs.format_s(0, v, 0, 0x1E, 0x1E)))


if __name__ == '__main__':
    unittest.main()
```

- [ ] **Step 2: Run to verify it fails**

Run: `PYTHONDONTWRITEBYTECODE=1 python3 -m unittest tools.tests.test_gp_keys 2>&1 | tail -3`
Expected: `KeyError: 'gp-keys-fight'` (an import-time error of the module).

- [ ] **Step 3: Implement** — apply (from the worktree root):

```bash
git apply <<'PATCH'
--- a/tools/gp_session.py
+++ b/tools/gp_session.py
@@ -59,6 +59,16 @@
     'p2.start': (0x3C, 0x3C00, 0x0001),
 }
 
+# U11 (record 2026-10-01-gameplay-u11 §K.2): the keys the int 16h key loop
+# 0x24D08..0x24EE7 and its blocking readers (the prompt 0x24A64, the pause
+# 0x24E54) act on in a match. Space, y and n are the standard set-1 make words
+# (scan << 8 | ascii); an Alt-letter is its scan with ascii 0, the form the
+# extended-key arms 0x24D96..0x24DB8 test. The key-state side writes the
+# letter's own scan: nothing reads Alt's scan 0x38 (record §K.2), and S (0x1F),
+# M (0x32) and N (0x31) are also P1's up, b3 and b2 in the default binding.
+KEYS.update({'space': (0x39, 0x3920), 'y': (0x15, 0x1579), 'n': (0x31, 0x316E),
+             'alt-q': (0x10, 0x1000), 'alt-s': (0x1F, 0x1F00), 'alt-m': (0x32, 0x3200)})
+
 _DEC = ('ms', 'rc', 'step', 'late', 'ring')
 _TEXT = ('reason', 'press', 'release')
 
@@ -68,8 +78,8 @@
     return ((raw >> 24) & 0xFF) << 8 | (raw >> 8) & 0xFF
 
 
-def format_s(ms, vals, kb, head, tail):
-    body = ' '.join('%s=%0*X' % (n, 2 * sz, vals[n]) for n, _, sz in SNAP_FIELDS)
+def format_s(ms, vals, kb, head, tail, fields=SNAP_FIELDS):
+    body = ' '.join('%s=%0*X' % (n, 2 * sz, vals[n]) for n, _, sz in fields)
     return 'S ms=%d %s kb=%04X head=%04X tail=%04X' % (ms, body, kb, head, tail)
 
 
@@ -147,6 +157,42 @@
     ('until_mode', 0x06, 0),                          # the round's first frame
 ))
 
+# U11 (plan 2026-10-01-gameplay-u11-in-match-keys.md, record §K.6): the
+# in-match keys, pressed in round 1 (mode 6) of the gp-idle-loss path, then
+# ABANDON CONQUEST's yes in the join's mode 0x17. The order makes every event
+# change the latch DS_00105F30 (record §K.6). A prompt's or the pause's answer
+# is queued in its opener's spin (`after` 0), so the blocking reader (0x24A64,
+# 0x24E54) finds it at once and f never has to advance inside the blocking
+# loop (spec §7 Q5, record §K.5). Harness values, not game values: the
+# 10-frame gaps exceed the 1-4 iteration BIOS latency (record §G.7.3) plus
+# HOLD_FRAMES; the 20 frames after F2 keep the ESC inside the join's 0x78-frame
+# mode 0x17 (0x28E75); 90 s covers mode 6 at 55.1 s in gp-idle-loss (§G.18)
+# plus the restart's boot movies (14.7 s in gp-pads, §G.7.3). KEYS_EXTRA are
+# the S fields this scenario adds: the latch (0x24D4D), the sample and music
+# pause bytes (0x1D220, 0x1D1B0).
+KEYS_EXTRA = (('lat', 0x105F30, 4), ('spz', 0x1028DB, 1), ('mpz', 0x1028DA, 1))
+SCENARIOS['gp-keys-fight'] = dict(time_limit=90, extra=KEYS_EXTRA, steps=(
+    ('boot', ENTER_WAIT, ('key', 'enter')),         # 0: mode 3 -> 0x27
+    ('after_mode', 0x27, 150, ('key', 'enter')),    # 1: START MENU
+    ('after', 150, ('key', 'enter')),               # 2: LEFT PLAYER ARCADE (mode 0x2D)
+    ('after_mode', 0x06, 10, ('key', 'enter')),     # 3: Enter in a fight: latched only (0x24ECF)
+    ('after', 10, ('key', 'space')),                # 4: - PAUSED - (0x24E19) ...
+    ('after', 0, ('key', 'space')),                 # 5: ... ended by the next space (0x24E67)
+    ('after', 10, ('key', 'alt-s')),                # 6: samples paused (0x24DC9 0x1D220)
+    ('after', 10, ('key', 'esc')),                  # 7: ABANDON CONQUEST? Y/N (0x24EC5) ...
+    ('after', 0, ('key', 'n')),                     # 8: ... no (0x24AB5)
+    ('after', 10, ('key', 'alt-m')),                # 9: music paused (0x24DBF 0x1D1B0)
+    ('after', 10, ('key', 'alt-q')),                # 10: QUIT TO DOS? Y/N (0x24DDF) ...
+    ('after', 0, ('key', 'n')),                     # 11: ... no
+    ('after', 10, ('key', 'alt-s')),                # 12: samples back
+    ('after', 10, ('key', 'alt-m')),                # 13: music back
+    ('after', 10, ('pad', ('p1.start',), 3)),       # 14: F1 = P1 b0 (side 0 already in: 0x28CD7)
+    ('after', 10, ('pad', ('p2.start',), 3)),       # 15: F2 = P2 joins (0x2525F 0x28CC8, 0x25269 0x28DA4)
+    ('after', 20, ('key', 'esc')),                  # 16: ABANDON CONQUEST? Y/N in mode 0x17 ...
+    ('after', 0, ('key', 'y')),                     # 17: ... yes: the 0x24AB0 soft restart
+    ('until_mode', 0x03, 0),                        # 18: the restart's mode 3 (0x20CE6 0x10E80)
+))
+
 
 def expand(action):
     """An action -> [(name, scan, bios_word, hold_frames)]."""
--- a/tools/gp_capture.py
+++ b/tools/gp_capture.py
@@ -27,9 +27,9 @@
 import title_capture as tcap
 
 
-def read_snap(mm, base):
+def read_snap(mm, base, fields=gs.SNAP_FIELDS):
     out = {}
-    for name, ds, size in gs.SNAP_FIELDS:
+    for name, ds, size in fields:
         o = base + ds - gs.DATA_BASE_VA
         out[name] = int.from_bytes(mm[o:o + size], 'little')
     return out
@@ -189,8 +189,9 @@
 
 
 class Poller(threading.Thread):
-    def __init__(self, mem_path, log_path, steps, stop, pad_bios=True):
+    def __init__(self, mem_path, log_path, steps, stop, pad_bios=True, fields=gs.SNAP_FIELDS):
         super().__init__(daemon=True)
+        self.fields = fields          # SNAP_FIELDS plus a scenario's `extra` (U11 record §K.3)
         self.mem_path, self.log_path, self.stop, self.pad_bios = mem_path, log_path, stop, pad_bios
         self.sched = gs.Schedule(steps)
         self.end_seen = False
@@ -240,12 +241,12 @@
                     for name, scan, word, hold in gs.expand(act):
                         inj.press(step, v['f'], name, scan, word, hold, 0, ms)
                 if spinning(v) and v['f'] != last_f:
-                    v2 = read_snap(mm, base)
+                    v2 = read_snap(mm, base, self.fields)
                     kb = mm[ptr + 0x2D8] << 8 | mm[ptr + 0x2D9]
                     bh, bt = kc.u16(mm, gs.BDA_HEAD), kc.u16(mm, gs.BDA_TAIL)
                     v3 = read_snap(mm, base)
                     if accept(v, v2, v3):
-                        log.write(gs.format_s(ms, v2, kb, bh, bt) + '\n')
+                        log.write(gs.format_s(ms, v2, kb, bh, bt, self.fields) + '\n')
                         f = v2['f']
                         self.sched.on_mode(f, v2['mode'])
                         inj.release_due(f, ms)
@@ -287,7 +288,8 @@
         print('gp_capture: %s' % shlex.join(cmd))
         stop = threading.Event()
         poll = Poller(os.path.join(root, 'guest.mem'), os.path.join(root, 'poll.log'),
-                      scn['steps'], stop, pad_bios=not a.no_pad_bios)
+                      scn['steps'], stop, pad_bios=not a.no_pad_bios,
+                      fields=gs.SNAP_FIELDS + scn.get('extra', ()))
         poll.start()
         t = time.monotonic()
         r = subprocess.run(cmd)
PATCH
```

- [ ] **Step 4: Run the tests**

Run: `PYTHONDONTWRITEBYTECODE=1 python3 -m unittest tools.tests.test_gp_keys tools.tests.test_gp_session tools.tests.test_gp_capture 2>&1 | tail -3`
Expected: `Ran 41 tests` … `OK` (3 new + 38 existing; U5 added three to `test_gp_session`).

- [ ] **Step 5: Mutation proofs** (each applied alone, run as Step 4, then restored with `git checkout -p` or by re-applying; record each output line in §K.11):

| # | mutation | expected |
|---|---|---|
| S1 | `gp_capture.read_snap`: `for name, ds, size in fields:` → `in gs.SNAP_FIELDS:` | `ERROR: test_extra_fields_are_read_and_logged` |
| S2 | `gp_session.format_s`: `for n, _, sz in fields)` → `in SNAP_FIELDS)` | `ERROR: test_extra_fields_are_read_and_logged` |
| S3 | `'alt-s': (0x1F, 0x1F00)` → `(0x1F, 0x1F73)` | `FAIL: test_keys_are_bios_make_words` |
| S4 | step 8 `('after', 0, ('key', 'n'))` → `('after', 1, …)` | `FAIL: test_an_answer_fires_with_its_opener` |

- [ ] **Step 6: Record** — append to the record:

```markdown
## §K.11 Tool log (Tasks 1–4)

### Task 1: keys, extra fields, scenario
- `gp_session`: six keys (record §K.2), `format_s(..., fields)`, `KEYS_EXTRA` (record §K.3), `SCENARIOS['gp-keys-fight']` (record §K.6). `gp_capture`: `read_snap(..., fields)`, `Poller(fields=...)`, `main` passes `SNAP_FIELDS + extra`.
- Tests: before `KeyError: 'gp-keys-fight'`; after `Ran 41 tests … OK`. Mutations S1–S4: <the four measured lines>.
```

- [ ] **Step 7: Commit**

```bash
git add tools/gp_session.py tools/gp_capture.py tools/tests/test_gp_keys.py docs/superpowers/plans/2026-10-01-gameplay-u11-derivations.md
git commit -m "$(cat <<'EOF'
tools: gp-keys-fight scenario, in-match BIOS keys and per-scenario snapshot fields (record §K.11)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 2: `tools/gp_keys.py` — the evidence check and the effects ratchet

**Files:**
- Create: `tools/gp_keys.py`
- Modify: `tools/tests/test_gp_keys.py` (replace with the full file)
- Modify: record §K.11

**Interfaces:**
- Consumes: `gp_session.parse`, `raw_to_kb`, `format_s`, `SCENARIOS['gp-keys-fight']`; the capture's `poll.log` (`S`, `I`, `H`); the port's `trace.txt` (`T`).
- Produces: `gp_keys.EVENTS`, `records(lines, kind)`, `frames(cap_lines)`, `judge(rule, c, F, rec, boot_cred)`, `judge_all(cap_lines, rec) -> [(label, c, F, reasons)]`; the CLI `gp_keys.py {evidence|effects} --capture DIR [--port DIR] [--min-effects N] [--capture-sha256 HEX]` (exit 0 ok or skipped, 1 on a failure); output lines `gp_keys: <scenario>: <cmd>: <label> f=<c> [F=<F>] ok|FAIL: …`, `gp_keys: …: evidence: <k> of 11 events ok|FAIL`, `gp_keys: …: effects: first not reproduced <k>, ratchet N <n> ok`.

- [ ] **Step 1: Write the failing tests** — replace `tools/tests/test_gp_keys.py` with:

```python
# tools/tests/test_gp_keys.py — U11 in-match keys (record 2026-10-01-gameplay-u11 §K)
import io
import os
import sys
import tempfile
import unittest
from contextlib import redirect_stdout

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import gp_capture as gc
import gp_keys as gk
import gp_session as gs

FIELDS = gs.SNAP_FIELDS + gs.SCENARIOS['gp-keys-fight']['extra']
STEPS = gs.SCENARIOS['gp-keys-fight']['steps']
BASE = 0x266000


def _word(step):
    act = STEPS[step][-1]
    return gs.KEYS[act[1]][1] if act[0] == 'key' else gs.PAD[act[1][0]][1]


def _fight(mut=None):
    """A synthetic gp-keys-fight poll.log that does what record §K.6 derives:
    boot in mode 3 with 5 credits, the three menu Enters, round 1 (mode 6, 4
    credits, side 0 in) from f=0x200, then each event of gk.EVENTS 10 frames
    apart from f=0x20A, every word consumed at its event's frame c. `mut`
    (label -> {field: value}) overrides the state an event leaves at its judged
    frame; 'split' consumes the event's second word one frame later."""
    mut = mut or {}
    st = {n: 0 for n, _, _ in FIELDS}
    st.update(mode=0x03, cred=5, t508=1, t50c=2, rng=gk.RESTART_SEED)
    change = {0x101: dict(mode=0x27), 0x103: dict(mode=0x2D, cred=4, b1f=1), 0x200: dict(mode=0x06, rng=0x1234)}
    press, heads, skip = [], [], set()
    for s, f in ((0, 0x101), (1, 0x102), (2, 0x103)):
        press.append((f - 1, s, 0x1C0D))
        heads.append(f)
    f, pz = 0x20A, {'spz': 0, 'mpz': 0}
    for label, step, n, rule in gk.EVENTS:
        for k in range(n):
            press.append((f - 1, step + k, _word(step + k)))
            heads.append(f + (1 if k and mut.get(label) == 'split' else 0))
        kind = rule[0]
        at = f
        if kind == 'latched':
            change[f] = dict(lat=rule[1])
        elif kind == 'cleared':
            change[f] = dict(lat=0)
        elif kind == 'toggled':
            pz[rule[2]] ^= 1
            change[f] = {'lat': rule[1], rule[2]: pz[rule[2]]}
        elif kind == 'b0':
            change[f] = dict(raw=0x01000000, lat=rule[1])
            change[f + 1] = dict(new=0x01000000, held=0x01000000, e0=0x0101)
            change[f + 2] = dict(new=0, e0=0x0100)
            change[f + 3] = dict(raw=0)
            change[f + 4] = dict(held=0, e0=0)
            at = f + 1
        elif kind == 'join':
            change[f] = dict(raw=0x100, lat=rule[1])
            change[f + 1] = dict(new=0x100, held=0x100, mode=gk.JOIN_MODE, b1f=3)
            change[f + 2] = dict(new=0)
            change[f + 3] = dict(raw=0)
            at = f + 1
        else:
            skip.add(f)                              # 0x24AB0 abandons iteration c
            change[f + 1] = dict(mode=0x03, rng=gk.RESTART_SEED, cred=5, b1f=0)
            at = f + 1
        if isinstance(mut.get(label), dict):
            change.setdefault(at, {}).update(mut[label])
        f += 10
    last = f + 4
    out = []
    for g in range(0x100, last):
        while press and press[0][0] == g:
            pf, s, w = press.pop(0)
            out.append('I ms=0 f=%04X step=%d press=k scan=%02X lin=00010000 old=FF bios=%04X ring=1 late=0'
                       % (pf, s, w >> 8, w))
        for h in [h for h in heads if h == g]:
            out.append('H ms=0 f=%04X head=0020' % h)
        heads = [h for h in heads if h != g]
        st.update(change.get(g, {}))
        if g in skip or 0x104 <= g < 0x1F0:
            continue                                 # the restart's iteration; the menus and the select
        out.append(gs.format_s(0, dict(st, f=g), gs.raw_to_kb(st['raw']), 0x1E, 0x1E, FIELDS))
    out.append('X ms=0 f=%04X step=18 end' % (last - 1))
    return out


def _port(lines):
    """The port's trace for a capture: the same records as T lines."""
    return ['T ' + l[2:] for l in lines if l.startswith('S ')]


class TestKeys(unittest.TestCase):
    def test_keys_are_bios_make_words(self):
        # scan << 8 | ascii; an Alt-letter has ascii 0 (the 0x24D96 arms)
        self.assertEqual([gs.KEYS[k] for k in ('space', 'y', 'n', 'alt-q', 'alt-s', 'alt-m')],
                         [(0x39, 0x3920), (0x15, 0x1579), (0x31, 0x316E),
                          (0x10, 0x1000), (0x1F, 0x1F00), (0x32, 0x3200)])
        self.assertEqual(gs.KEYS['enter'], (0x1C, 0x1C0D))         # unchanged
        self.assertEqual(gs.KEYS['esc'], (0x01, 0x011B))

    def test_an_answer_fires_with_its_opener(self):
        s = gs.Schedule(STEPS)
        s.due_boot(gs.ENTER_WAIT)
        s.on_mode(0x141, 0x27)
        s.due(0x141 + 149)
        s.due(0x141 + 299)
        s.on_mode(0x7F5, 0x06)
        self.assertEqual([x[0] for x in s.due(0x7F5 + 9)], [3])
        self.assertEqual([x[0] for x in s.due(0x7F5 + 19)], [4, 5])      # space, space in one spin
        self.assertEqual([x[0] for x in s.due(0x7F5 + 29)], [6])
        self.assertEqual([x[0] for x in s.due(0x7F5 + 39)], [7, 8])      # esc, n
        self.assertEqual(s.total, 18)

    def test_extra_fields_are_read_and_logged(self):
        m = bytearray(0x400000)
        o = BASE + 0x105F30 - gs.DATA_BASE_VA
        m[o:o + 4] = (0x1B).to_bytes(4, 'little')
        m[BASE + 0x1028DA - gs.DATA_BASE_VA] = 1
        v = gc.read_snap(m, BASE, FIELDS)
        self.assertEqual((v['lat'], v['mpz'], v['spz']), (0x1B, 1, 0))
        self.assertNotIn('lat', gc.read_snap(m, BASE))                    # the default is unchanged
        rec = gs.parse(gs.format_s(0, dict(v, f=7), 0, 0x1E, 0x1E, FIELDS))
        self.assertEqual((rec['lat'], rec['mpz']), (0x1B, 1))
        self.assertNotIn('lat', gs.parse(gs.format_s(0, v, 0, 0x1E, 0x1E)))


class TestEvidence(unittest.TestCase):
    def test_the_raw_effects_pass(self):
        L = _fight()
        rows = gk.judge_all(L, gk.records(L, 'S'))
        self.assertEqual([r[0] for r in rows], [e[0] for e in gk.EVENTS])
        self.assertEqual([r[3] for r in rows], [[]] * len(gk.EVENTS))

    def test_each_rule_can_fail(self):
        cases = {'enter': {'lat': 0}, 'pause': {'lat': 0x20}, 'alt-s on': {'spz': 0},
                 'esc-n': {'mpz': 1}, 'alt-m on': {'lat': 0}, 'altq-n': {'lat': 0x6E},
                 'alt-s off': {'mpz': 0}, 'alt-m off': {'mpz': 1}, 'f1': {'e0': 0x0100},
                 'f2': {'cred': 3}, 'esc-y': {'rng': 0x1234}}
        self.assertEqual(sorted(cases), sorted(e[0] for e in gk.EVENTS))
        for label, m in cases.items():
            L = _fight({label: m})
            bad = [r[0] for r in gk.judge_all(L, gk.records(L, 'S')) if r[3]]
            self.assertIn(label, bad, label)

    def test_an_unchanged_latch_cannot_show_an_event(self):
        L = _fight({'pause': {'lat': 0x0D}})          # the pause left the Enter's latch
        bad = [r[0] for r in gk.judge_all(L, gk.records(L, 'S')) if r[3]]
        self.assertEqual(bad, ['pause'])
        L = _fight()
        rec = gk.records(L, 'S')
        c = [r for r in gk.judge_all(L, rec) if r[0] == 'enter'][0][1]
        rec[c - 1] = dict(rec[c - 1], lat=0x0D)
        self.assertIn('already', ' '.join(gk.judge(('latched', 0x0D), c, None, rec, 5)))

    def test_a_restart_must_abandon_its_iteration(self):
        L = _fight()
        rec = gk.records(L, 'S')
        c = [r for r in gk.judge_all(L, rec) if r[0] == 'esc-y'][0][1]
        self.assertEqual(gk.judge(('restart',), c, None, rec, 5), [])
        rec[c] = dict(rec[c - 1], f=c)
        self.assertIn('abandons', ' '.join(gk.judge(('restart',), c, None, rec, 5)))

    def test_an_answer_read_a_frame_later_fails(self):
        L = _fight({'esc-n': 'split'})
        bad = [(r[0], r[3]) for r in gk.judge_all(L, gk.records(L, 'S')) if r[3]]
        self.assertEqual([b[0] for b in bad], ['esc-n'])
        self.assertIn('want one', bad[0][1][0])


class TestEffects(unittest.TestCase):
    def _run(self, cap, port, n, sha=None, cmd='effects'):
        with tempfile.TemporaryDirectory() as d:
            os.makedirs(os.path.join(d, 'cap'))
            os.makedirs(os.path.join(d, 'port'))
            if cap is not None:
                with open(os.path.join(d, 'cap', 'poll.log'), 'w') as f:
                    f.write('\n'.join(cap) + '\n')
            with open(os.path.join(d, 'port', 'trace.txt'), 'w') as f:
                f.write('\n'.join(port) + '\n')
            argv = ['gp_keys.py', cmd, '--capture', os.path.join(d, 'cap'), '--port', os.path.join(d, 'port')]
            argv += [] if n is None else ['--min-effects', str(n)]
            argv += [] if sha is None else ['--capture-sha256', sha]
            out, old = io.StringIO(), sys.argv
            sys.argv = argv
            try:
                with redirect_stdout(out):
                    rc = gk.main()
            finally:
                sys.argv = old
            return rc, out.getvalue()

    def test_a_faithful_port_reproduces_every_event(self):
        L = _fight()
        rc, out = self._run(L, _port(L), len(gk.EVENTS))
        self.assertEqual(rc, 0, out)
        self.assertIn('first not reproduced 11, ratchet N 11 ok', out)

    def test_the_ratchet_fails_below_n_and_unpinned(self):
        L = _fight()
        bad = _port(_fight({'alt-s off': {'spz': 1}}))                 # the port misses event 6
        rc, out = self._run(L, bad, 7)
        self.assertEqual(rc, 1)
        self.assertIn('first not reproduced 6 < ratchet N 7', out)
        self.assertEqual(self._run(L, bad, 6)[0], 0)
        self.assertEqual(self._run(L, _port(L), None)[0], 1)            # unpinned
        self.assertEqual(self._run(L, _port(L), 12)[0], 1)              # N past the events

    def test_another_capture_fails_the_pin(self):
        L = _fight()
        rc, out = self._run(L, _port(L), 11, sha='0' * 64)
        self.assertEqual(rc, 1)
        self.assertIn('re-pin', out)
        rc, out = self._run(L, _port(L), 11, sha='')
        self.assertEqual(rc, 1)
        self.assertIn('not pinned', out)
        good = gk.hashlib.sha256(('\n'.join(L) + '\n').encode()).hexdigest()
        self.assertEqual(self._run(L, _port(L), 11, sha=good)[0], 0)

    def test_evidence_cli(self):
        L = _fight()
        self.assertEqual(self._run(L, [], None, cmd='evidence')[0], 0)
        rc, out = self._run(_fight({'f2': {'b1f': 1}}), [], None, cmd='evidence')
        self.assertEqual(rc, 1)
        self.assertIn('10 of 11 events FAIL', out)

    def test_an_absent_capture_skips(self):
        rc, out = self._run(None, [], 11)
        self.assertEqual(rc, 0)
        self.assertIn('skipped', out)


if __name__ == '__main__':
    unittest.main()
```

- [ ] **Step 2: Run to verify it fails**

Run: `PYTHONDONTWRITEBYTECODE=1 python3 -m unittest tools.tests.test_gp_keys 2>&1 | tail -4`
Expected: `ModuleNotFoundError: No module named 'gp_keys'`.

- [ ] **Step 3: Implement** — create `tools/gp_keys.py`:

```python
#!/usr/bin/env python3
"""U11 in-match keys (plan docs/superpowers/plans/2026-10-01-gameplay-u11-in-match-keys.md,
record docs/superpowers/plans/2026-10-01-gameplay-u11-derivations.md §K.6).
Two checks of the gp-keys-fight capture, one row per key event:

  evidence  the capture's S records (poll.log) show each event's raw-derived
            effect: the original did what the raw says;
  effects   the port's T records (trace.txt of the PR_GP_DUMP replay) show the
            same effect at the same frames: the port did what the original did.
            A ratchet: the events before the first one the port does not
            reproduce must number at least --min-effects N.

An event is one or more scenario steps whose BIOS words are consumed in one
iteration c (a prompt's or the pause's answer is queued in its opener's spin).
c pairs each I press with its H record FIFO, as gp_session.port_script does.
A pad event's frame F is the first S record after its press whose raw word
carries its kb bit; the level, and so DS_001088E4, follows at F + 1 (0x500C4,
record gameplay-ground-truth §G.1.1). Both sides are judged at the capture's
c and F (the port script queues each word at c and applies the bits at F).
An absent capture skips (exit 0); a present capture with another poll.log
than --capture-sha256 pins, an empty (unpinned) --capture-sha256 or an
unpinned N fails. Stdlib only."""
import argparse
import hashlib
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gp_session as gs

SCENARIO = 'gp-keys-fight'
RESTART_SEED = 0xABCD   # 0x20C53 mov ebx,0xABCD; 0x20C62 mov [0xEF6D8],ebx (the restart tail)
JOIN_MODE = 0x17        # 0x28E3A mov ecx,0x17; 0x28E66 mov [0x104B00],cx (0x28DA4)
P1_B0_NEW = 0x01000000  # 0x9ACBC[0]: side 0's start mask, F1's and U's bit (record §G.7.2)
LOOKAHEAD = 8           # harness: frames searched past F + 1 for a state (snapshot gaps, spec §3.7)

# (label, first scenario step, steps in the event, rule); record §K.6 derives each rule.
EVENTS = (
    ('enter', 3, 1, ('latched', 0x0D)),
    ('pause', 4, 2, ('cleared',)),
    ('alt-s on', 6, 1, ('toggled', 0x1F, 'spz', 'mpz')),
    ('esc-n', 7, 2, ('cleared',)),
    ('alt-m on', 9, 1, ('toggled', 0x32, 'mpz', 'spz')),
    ('altq-n', 10, 2, ('cleared',)),
    ('alt-s off', 12, 1, ('toggled', 0x1F, 'spz', 'mpz')),
    ('alt-m off', 13, 1, ('toggled', 0x32, 'mpz', 'spz')),
    ('f1', 14, 1, ('b0', 0x3B, 0x0100)),
    ('f2', 15, 1, ('join', 0x3C, 0x0001)),
    ('esc-y', 16, 2, ('restart',)),
)


def records(lines, kind):
    """f -> the first record of `kind` ('S' capture, 'T' port) at that f."""
    out = {}
    for line in lines:
        r = gs.parse(line)
        if r and r['kind'] == kind:
            out.setdefault(r['f'], r)
    return out


def frames(cap_lines):
    """step -> (press f, [consumption frame of each BIOS word]) from the capture."""
    recs = [r for r in (gs.parse(l) for l in cap_lines) if r]
    presses = [r for r in recs if r['kind'] == 'I' and 'press' in r and r.get('bios') is not None]
    heads = [r for r in recs if r['kind'] == 'H']
    out = {}
    for k, p in enumerate(presses):
        c = heads[k]['f'] if k < len(heads) else None
        out.setdefault(p['step'], (p['f'], []))[1].append(c)
    return out


def _first(rec, lo, hi):
    """The record with the smallest f in [lo, hi], or None."""
    for f in range(lo, hi + 1):
        if f in rec:
            return rec[f]
    return None


def _pad_frame(cap, press_f, bit):
    """F: the first capture S record after the press whose raw carries `bit`."""
    for f in range(press_f + 1, press_f + 1 + LOOKAHEAD):
        r = cap.get(f)
        if r is not None and gs.raw_to_kb(r['raw']) & bit:
            return f
    return None


def _latch(rec, c, want):
    """[] when the latch DS_00105F30 changes to `want` in iteration c."""
    prev, now = rec.get(c - 1), rec.get(c)
    if prev is None or now is None:
        return ['no record at f=%X or f=%X' % (c - 1, c)]
    why = []
    if prev['lat'] == want:
        why.append('lat already %X at f=%X: the event cannot show' % (want, c - 1))
    if now['lat'] != want:
        why.append('lat %X at f=%X, want %X' % (now['lat'], c, want))
    return why


def judge(rule, c, F, rec, boot_cred):
    """[] when `rec` shows the rule's effect at c (and F), else the reasons."""
    kind = rule[0]
    if kind == 'restart':
        prev = rec.get(c - 1)
        if prev is None:
            return ['no record at f=%X' % (c - 1)]
        why = []
        if prev['mode'] in (0x03, 0x27):
            why.append('mode %X before the key (0x24EB7 needs another mode)' % prev['mode'])
        if prev['cred'] == boot_cred:
            why.append('cred %X before the key equals the boot value: nothing to restore' % prev['cred'])
        if c in rec:
            why.append('a record at f=%X: 0x24AB0 abandons that iteration before its spin' % c)
        after = next((rec[f] for f in sorted(rec) if f > c), None)
        if after is None:
            return why + ['no record after f=%X' % c]
        if (after['mode'], after['rng'], after['cred']) != (0x03, RESTART_SEED, boot_cred):
            why.append('mode/rng/cred %X/%X/%X at f=%X, want 3/%X/%X'
                       % (after['mode'], after['rng'], after['cred'], after['f'], RESTART_SEED, boot_cred))
        return why
    want = 0 if kind == 'cleared' else rule[1]
    why = _latch(rec, c, want)
    if why and why[0].startswith('no record'):
        return why
    prev, now = rec[c - 1], rec[c]
    if kind in ('latched', 'cleared', 'toggled'):
        if prev['mode'] in (0x03, 0x27):
            why.append('mode %X before the key (the arms need a fight)' % prev['mode'])
        if now['mode'] != prev['mode']:
            why.append('mode %X -> %X' % (prev['mode'], now['mode']))
        if kind == 'toggled':
            fld, other = rule[2], rule[3]
            if prev[fld] not in (0, 1) or now[fld] != prev[fld] ^ 1:
                why.append('%s %X -> %X, want a flip' % (fld, prev[fld], now[fld]))
            if now[other] != prev[other]:
                why.append('%s %X -> %X, want unchanged' % (other, prev[other], now[other]))
        elif kind == 'cleared':
            for fld in ('spz', 'mpz'):
                if now[fld] != prev[fld]:
                    why.append('%s %X -> %X, want unchanged (0x1D250/0x1D270 pair)' % (fld, prev[fld], now[fld]))
        return why
    if F is None:
        return why + ['the pad bit never reached raw']
    before = rec.get(F - 1)
    if before is None:
        return why + ['no record at f=%X' % (F - 1)]
    if kind == 'b0':
        edge = rec.get(F + 1)
        if edge is None:
            return why + ['no record at f=%X' % (F + 1)]
        if not edge['new'] & P1_B0_NEW or edge['e0'] & 0x0101 != 0x0101:
            why.append('new %08X e0 %04X at f=%X, want P1 b0 newly pressed' % (edge['new'], edge['e0'], F + 1))
        if edge['mode'] != before['mode'] or edge['b1f'] != before['b1f']:
            why.append('mode/b1f %X/%X -> %X/%X, want no join' % (before['mode'], before['b1f'],
                                                                 edge['mode'], edge['b1f']))
        return why
    after = _first(rec, F + 1, F + 1 + LOOKAHEAD)
    if after is None:
        return why + ['no record in f=%X..%X' % (F + 1, F + 1 + LOOKAHEAD)]
    if before['b1f'] & 2:
        why.append('side 1 already in (b1f %X)' % before['b1f'])
    if after['mode'] != JOIN_MODE or after['b1f'] != before['b1f'] | 2:
        why.append('mode/b1f %X/%X at f=%X, want %X/%X' % (after['mode'], after['b1f'], after['f'],
                                                          JOIN_MODE, before['b1f'] | 2))
    if after['cred'] != before['cred']:
        why.append('cred %X -> %X, want unchanged (0x2CA93)' % (before['cred'], after['cred']))
    return why


def judge_all(cap_lines, rec):
    """[(label, c, F, reasons)] for every event, judged on `rec` at the capture's frames."""
    fr = frames(cap_lines)
    cap = records(cap_lines, 'S')
    boot_cred = rec[min(rec)]['cred'] if rec else None
    out = []
    for label, step, n, rule in EVENTS:
        got = [fr.get(s) for s in range(step, step + n)]
        if any(g is None for g in got):
            out.append((label, None, None, ['step %d never pressed' % step]))
            continue
        cs = [c for _, cl in got for c in cl]
        c = cs[0]
        if c is None or any(x != c for x in cs):
            out.append((label, c, None, ['words consumed in frames %s, want one' % cs]))
            continue
        F = _pad_frame(cap, got[0][0], rule[2]) if rule[0] in ('b0', 'join') else None
        out.append((label, c, F, judge(rule, c, F, rec, boot_cred)))
    return out


def _print(name, what, rows):
    for label, c, F, why in rows:
        at = 'f=%s' % ('-' if c is None else '%X' % c) + ('' if F is None else ' F=%X' % F)
        print('gp_keys: %s: %s: %-9s %s %s' % (name, what, label, at, 'ok' if not why else 'FAIL: ' + '; '.join(why)))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('cmd', choices=('evidence', 'effects'))
    ap.add_argument('--scenario', default=SCENARIO)
    ap.add_argument('--capture', required=True)
    ap.add_argument('--port')
    ap.add_argument('--min-effects', default=None)
    ap.add_argument('--capture-sha256', default=None,
                    help="the pinned sha256 of the capture's poll.log; with it another capture fails")
    a = ap.parse_args()
    log = os.path.join(a.capture, 'poll.log')
    if not os.path.isfile(log):
        print('gp_keys: %s: no capture at %s, skipped' % (a.scenario, a.capture))
        return 0
    with open(log, 'rb') as f:
        blob = f.read()
    if a.capture_sha256 is not None:
        sha = hashlib.sha256(blob).hexdigest()
        if a.capture_sha256 == '':
            print('gp_keys: %s: FAIL: --capture-sha256 is not pinned (this poll.log: %s)' % (a.scenario, sha))
            return 1
        if sha != a.capture_sha256:
            print('gp_keys: %s: FAIL: poll.log sha256 %s, pinned %s (re-measure and re-pin)'
                  % (a.scenario, sha, a.capture_sha256))
            return 1
    cap_lines = blob.decode().splitlines()
    if a.cmd == 'evidence':
        rows = judge_all(cap_lines, records(cap_lines, 'S'))
        _print(a.scenario, 'evidence', rows)
        bad = sum(1 for r in rows if r[3])
        print('gp_keys: %s: evidence: %d of %d events %s' % (a.scenario, len(rows) - bad, len(rows),
                                                         'ok' if not bad else 'FAIL'))
        return 1 if bad else 0
    trace = os.path.join(a.port or '', 'trace.txt')
    if not os.path.isfile(trace):
        print('gp_keys: %s: effects: FAIL: no port trace at %s' % (a.scenario, trace))
        return 1
    with open(trace) as f:
        rows = judge_all(cap_lines, records(f.read().splitlines(), 'T'))
    _print(a.scenario, 'effects', rows)
    first = next((k for k, r in enumerate(rows) if r[3]), len(rows))
    if a.min_effects in (None, ''):
        print('gp_keys: %s: effects: FAIL: --min-effects is not pinned (first not reproduced: %d)' % (a.scenario, first))
        return 1
    n = int(a.min_effects)
    if n > len(rows):
        print('gp_keys: %s: effects: FAIL: ratchet N %d > %d events' % (a.scenario, n, len(rows)))
        return 1
    if first < n:
        print('gp_keys: %s: effects: FAIL: first not reproduced %d < ratchet N %d' % (a.scenario, first, n))
        return 1
    print('gp_keys: %s: effects: first not reproduced %d, ratchet N %d ok%s'
          % (a.scenario, first, n, ' (improved: raise N)' if first > n else ''))
    return 0


if __name__ == '__main__':
    sys.exit(main())
```

- [ ] **Step 4: Run the tests**

Run: `PYTHONDONTWRITEBYTECODE=1 python3 -m unittest tools.tests.test_gp_keys tools.tests.test_gp_session tools.tests.test_gp_capture tools.tests.test_gp_compare 2>&1 | tail -3`
Expected: `Ran 80 tests` … `OK` (13 in `test_gp_keys`).

- [ ] **Step 5: Mutation proofs** (each alone in `tools/gp_keys.py`, run `python3 -m unittest tools.tests.test_gp_keys`, restore; record each line):

| # | mutation | expected |
|---|---|---|
| M1 | `_latch`: `if prev['lat'] == want:` → `if False:` | `FAIL: test_an_unchanged_latch_cannot_show_an_event` |
| M2 | restart: `if c in rec:` → `if False:` | `FAIL: test_a_restart_must_abandon_its_iteration` |
| M3 | `if first < n:` → `if first < n - 1:` | `FAIL: test_the_ratchet_fails_below_n_and_unpinned` |
| M4 | `if sha != a.capture_sha256:` → `if False:` | `FAIL: test_another_capture_fails_the_pin` |
| M5 | cleared: `for fld in ('spz', 'mpz'):` → `for fld in ():` | `FAIL: test_each_rule_can_fail` |
| M6 | b0: `if not edge['new'] & P1_B0_NEW or edge['e0'] & 0x0101 != 0x0101:` → `if False:` | `FAIL: test_each_rule_can_fail` |
| M7 | join: `if after['cred'] != before['cred']:` → `if False:` | `FAIL: test_each_rule_can_fail` |
| M8 | toggled: `if now[other] != prev[other]:` → `if False:` | `FAIL: test_each_rule_can_fail` |
| M9 | `if a.capture_sha256 == '':` → `if False:` | `FAIL: test_another_capture_fails_the_pin` |

- [ ] **Step 6: Record** — append to §K.11 a `### Task 2: gp_keys.py` paragraph: the CLI, the rules as implemented (record §K.6's table), before `ModuleNotFoundError`, after `Ran 80 tests … OK`, and M1–M9's measured lines.

- [ ] **Step 7: Commit**

```bash
git add tools/gp_keys.py tools/tests/test_gp_keys.py docs/superpowers/plans/2026-10-01-gameplay-u11-derivations.md
git commit -m "$(cat <<'EOF'
tools: gp_keys.py, the in-match key evidence check and effects ratchet (record §K.11)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 3: The port side — the trace fields and the restart landing count

**Files:**
- Modify: `port/src/game/flow.c` (after `s_restart_point`, ~line 124; `game_loop`'s landing branch, ~6860), `port/src/game/flow.h` (after `game_restart_arm`)
- Modify: `port/tests/test_game.c` (`gp_trace_line` 12287–12304; `test_gp_replay` 12324–12412; `test_restart_drive` 12658–12698)
- Modify: record §K.11

**Interfaces:**
- Consumes: `game_restart_arm`, `game_loop`, the `PR_GP_DUMP` driver, `test_restart_drive`.
- Produces: `u32 game_restart_landings(void)` (`flow.h`); `T` lines ending `… ent=%08X lat=%08X spz=%02X mpz=%02X`; the driver's line `test_gp_replay: <n> restart(s) landed`.

- [ ] **Step 1: The dry-run scripts** — save in `$S` (scratch, not committed; record §K.8):

`$S/mk_dry.py`:

```python
# U11 dry run (record §K.8): the gp-idle-loss script's three Enters, then the
# eleven events of record §K.6 at hand-picked frames 10 apart from 2047 (mode 6
# is first seen at 0x7F5 = 2037 in gp-idle-loss), pads held 3, end 2170.
# Usage: python3 mk_dry.py IDLE_SCRIPT OUT_SCRIPT SCENARIO_NAME
import sys
ev = []
def key(f, s, a): ev.append((f, 0, 'key %d %02X %02X' % (f, s, a)))
def bits(f, kb): ev.append((f, 1, 'bits %d %04X' % (f, kb)))
key(2047, 0x1C, 0x0D)                                                    # enter
key(2057, 0x39, 0x20); key(2057, 0x39, 0x20)                             # pause
key(2067, 0x1F, 0); bits(2066, 0x8000); bits(2069, 0)                    # alt-s on (S = P1 up)
key(2077, 0x01, 0x1B); key(2077, 0x31, 0x6E); bits(2076, 0x0400); bits(2079, 0)   # esc-n (N = P1 b2)
key(2087, 0x32, 0); bits(2086, 0x0800); bits(2089, 0)                    # alt-m on (M = P1 b3)
key(2097, 0x10, 0); key(2097, 0x31, 0x6E); bits(2096, 0x0400); bits(2099, 0)      # altq-n
key(2107, 0x1F, 0); bits(2106, 0x8000); bits(2109, 0)                    # alt-s off
key(2117, 0x32, 0); bits(2116, 0x0800); bits(2119, 0)                    # alt-m off
bits(2126, 0x0100); bits(2129, 0); key(2127, 0x3B, 0)                    # f1
bits(2136, 0x0001); bits(2139, 0); key(2137, 0x3C, 0)                    # f2
key(2157, 0x01, 0x1B); key(2157, 0x15, 0x79)                             # esc-y
ev.sort(key=lambda e: (e[0], e[1]))
head = [l for l in open(sys.argv[1]).read().splitlines() if not l.startswith('end ')]
head[0] = '# gp port script v2: scenario %s' % sys.argv[3]
open(sys.argv[2], 'w').write('\n'.join(head + [e[2] for e in ev] + ['end 2170']) + '\n')
```

`$S/mk_drycap.py`:

```python
# U11 dry run (record §K.8): a poll.log from the port's own trace (T records
# as S), with I/H records at the dry script's frames, so tools/gp_keys.py can
# judge the port alone. Usage: python3 mk_drycap.py TRACE OUT_POLL_LOG
import sys
ev = {0: (320, 321), 1: (473, 474), 2: (621, 622), 3: (2046, 2047), 4: (2056, 2057), 5: (2056, 2057),
      6: (2066, 2067), 7: (2076, 2077), 8: (2076, 2077), 9: (2086, 2087), 10: (2096, 2097), 11: (2096, 2097),
      12: (2106, 2107), 13: (2116, 2117), 14: (2125, 2127), 15: (2135, 2137), 16: (2156, 2157), 17: (2156, 2157)}
recs = [(pf, 0, 'I ms=0 f=%04X step=%d press=k scan=00 lin=0 old=FF bios=0000 ring=1 late=0' % (pf, s))
        for s, (pf, c) in sorted(ev.items())]
recs += [(c, 1, 'H ms=0 f=%04X head=0020' % c) for s, (pf, c) in sorted(ev.items())]
recs += [(int(l.split()[1][2:], 16), 2, 'S ms=0 ' + l[2:].rstrip()) for l in open(sys.argv[1]) if l.startswith('T ')]
recs.sort(key=lambda x: (x[0], x[1]))
open(sys.argv[2], 'w').write('\n'.join(t for _, _, t in recs) + '\n')
```

- [ ] **Step 2: Run the restart replay on the unmodified port to see it fail**

```bash
cmake --build build 2>&1 | tail -1
python3 tools/gp_session.py port-script --scenario gp-idle-loss --capture data/k11-captures/gp-idle-loss --out $S/idle.script --end 2200
python3 $S/mk_dry.py $S/idle.script $S/dry.script 'gp-idle-loss (cut at 2170)'
rm -rf $S/dry0; PR_GP_DUMP=$S/dry0 PR_GP_SCRIPT=$S/dry.script PR_GAME_DIR=data/game/C ./build/run_tests 2>&1 | grep -E 'FAIL|passed|FAILURES'
```

Expected: `FAIL …/port/tests/test_game.c:12411: 1849 != 1850` and `FAILURES: 1` (re-measured on `8eaf25a`; the miss log then holds only the four harmless pairs, `distinct=4`): the ESC–Y restart at 2157 abandons an iteration (record §K.6), so the driver has one `T` line fewer than frames. (The scenario name `gp-idle-loss (cut at 2170)` lets the miss check accept a subset of the gp-idle-loss set; Task 6 pins gp-keys-fight's own.)

- [ ] **Step 3: Implement** — apply:

```bash
git apply <<'PATCH'
--- a/port/src/game/flow.c
+++ b/port/src/game/flow.c
@@ -122,6 +122,17 @@
  * §B.1), which only 0x65431 reads. game_loop() arms it. */
 static jmp_buf *s_restart_point;
 
+/* PORT: test seam, no raw counterpart: the restarts landed in game_loop()
+ * since boot. A landing abandons its iteration, so the frame counter rises by
+ * two in that step (record named-gaps-b §B.6b); the gp replay driver counts
+ * the landings to allow exactly that (record 2026-10-01-gameplay-u11 §K.6). */
+static u32 s_restart_landings;
+
+u32 game_restart_landings(void)
+{
+    return s_restart_landings;
+}
+
 jmp_buf *game_restart_arm(jmp_buf *jb)
 {
     jmp_buf *prev = s_restart_point;
@@ -6858,6 +6869,7 @@
     jmp_buf restart;
     jmp_buf *const prev = game_restart_arm(&restart);
     if (setjmp(restart) != 0) {
+        s_restart_landings++;          /* PORT: test seam (game_restart_landings) */
         /* 0x20C24: 0x65431 resumes after 0x20C1F's setjmp, whose result is
          * discarded (0x20C24 mov eax,-1), re-runs the tail and re-enters
          * 0x255CC at 0x20DE8; the iteration that jumped is abandoned (record
--- a/port/src/game/flow.h
+++ b/port/src/game/flow.h
@@ -39,6 +39,10 @@
  * 0x1044F4 (0x653FC), which only 0x65431 reads. */
 jmp_buf *game_restart_arm(jmp_buf *jb);
 
+/* PORT: test seam, no raw counterpart: how many 0x65431 restarts have landed
+ * at game_loop()'s restart point since boot. */
+u32 game_restart_landings(void);
+
 /* Tells game_main() which directory holds the INDEX-listed resources
  * (data/game/C). Must be called before game_main(). */
 void game_set_game_dir(const char *dir);
--- a/port/tests/test_game.c
+++ b/port/tests/test_game.c
@@ -12289,7 +12289,8 @@
     fprintf(gp_trace,
             "T f=%04X mode=%04X st=%04X tick=%08X t508=%08X t50c=%08X raw=%08X pad=%08X new=%08X held=%08X "
             "e0=%04X e2=%04X rng=%08X cred=%08X fp=%02X b1d=%02X b1f=%02X b25=%02X w10d=%02X cnt=%02X "
-            "s0_52=%02X s0_54=%02X s0_5a=%02X s1_52=%02X s1_54=%02X s1_5a=%02X ent=%08X\n",
+            "s0_52=%02X s0_54=%02X s0_5a=%02X s1_52=%02X s1_54=%02X s1_5a=%02X ent=%08X "
+            "lat=%08X spz=%02X mpz=%02X\n",
             (unsigned)DSW(DS_000EF6DC), (unsigned)DSW(DS_00104B00), (unsigned)DSW(DS_000F0A64),
             (unsigned)DSD(DS_00101500), (unsigned)DSD(DS_00101508), (unsigned)DSD(DS_0010150C),
             (unsigned)DSD(DS_000E1C30), (unsigned)DSD(DS_000E1C34), (unsigned)DSD(DS_001088E4),
@@ -12299,7 +12300,11 @@
             (unsigned)DSB(GP_DS_0010810D), (unsigned)DSB(DS_00108110),
             (unsigned)DSB(DS_00107802), (unsigned)DSB(DS_00107804), (unsigned)DSB(DS_0010780A),
             (unsigned)DSB(DS_00107896), (unsigned)DSB(DS_00107898), (unsigned)DSB(DS_0010789E),
-            (unsigned)DSD(DS_0010741C));
+            (unsigned)DSD(DS_0010741C),
+            /* U11 (record 2026-10-01-gameplay-u11 §K.3): the key-loop latch
+             * (0x24D4D) and the sample / music pause bytes (0x1D220, 0x1D1B0),
+             * gp_session.KEYS_EXTRA's names. */
+            (unsigned)DSD(DS_00105F30), (unsigned)DSB(DS_001028DB), (unsigned)DSB(DS_001028DA));
     gp_trace_lines++;
 }
 
@@ -12357,6 +12362,7 @@
     static u32 mode_before, mode_after, frame_after, state_after;
     mode_before = 0xFFFFu; mode_after = 0xFFFFu; frame_after = 0xFFFFFu; state_after = 0xFFFFFu;
     gp_first_f = 0xFFFFFu;
+    const u32 landings0 = game_restart_landings();
     const u32 limit = gp_end + GP_LOOP_SLACK;
     if (setjmp(gp_end_jb) == 0)
     for (gp_iters = 0; gp_iters < limit && !gp_done && !gp_failed; gp_iters++) {
@@ -12407,8 +12413,13 @@
      * loader screen). */
     CHECK_EQ_INT((int)gp_first_f, (int)gp_enter_frame);
     /* One T line per f from enter_frame to end: each armed iteration raises
-     * the counter by exactly one (0x24CDB), so a skipped or repeated f fails. */
-    CHECK_EQ_INT((int)gp_trace_lines, (int)(gp_end - gp_enter_frame + 1u));
+     * the counter by exactly one (0x24CDB), so a skipped or repeated f fails,
+     * except an iteration a 0x65431 restart abandons: its step raises the
+     * counter twice (record named-gaps-b §B.6b), so each landing accounts for
+     * one f without a T line (record 2026-10-01-gameplay-u11 §K.6). */
+    const u32 landed = game_restart_landings() - landings0;
+    printf("test_gp_replay: %u restart(s) landed\n", (unsigned)landed);
+    CHECK_EQ_INT((int)(gp_trace_lines + landed), (int)(gp_end - gp_enter_frame + 1u));
     return g_failures - before;
 }
 
@@ -12684,6 +12695,7 @@
     game_loop_step();                                  /* the menu's init */
     CHECK_EQ_INT((int)DSB(DS_00107414), 1);            /* 0x300C2 */
     const u32 t0 = DSD(DS_00105F2C);
+    const u32 l0 = game_restart_landings();
     u32 t_prev = 0u, t_last = 0u;
     u16 f_last = 0u;
     int n = 0;
@@ -12695,6 +12707,7 @@
         n++;
     }
     CHECK(n < RD_GUARD, "the idle timeout restarts within the guard");
+    CHECK_EQ_INT((int)(game_restart_landings() - l0), 1);   /* the seam counts the one landing */
     CHECK(t_last - t0 > 0x4B0u, "0x2EB9F: over 0x4B0 ticks on the restart iteration");
     CHECK(t_prev - t0 <= 0x4B0u, "0x2EB9F: not over on the iteration before");
     CHECK_EQ_INT((int)DSW(DS_00104B00), 3);            /* 0x10EA1 */
PATCH
cmake --build build 2>&1 | grep -E 'error|warning'; echo built
```

- [ ] **Step 4: Run to see it pass**

```bash
rm -rf $S/dry1; PR_GP_DUMP=$S/dry1 PR_GP_SCRIPT=$S/dry.script PR_GAME_DIR=data/game/C ./build/run_tests 2>&1 | grep -E 'FAIL|passed|landed'
PR_RESTART=1 PR_GAME_DIR=data/game/C ./build/run_tests 2>&1 | tail -1
mkdir -p $S/drycap; python3 $S/mk_drycap.py $S/dry1/trace.txt $S/drycap/poll.log
python3 tools/gp_keys.py evidence --capture $S/drycap | tail -1
python3 tools/gp_keys.py effects --capture $S/drycap --port $S/dry1 --min-effects 11 | tail -1
python3 - <<'EOF'
import sys; sys.path.insert(0, 'tools'); import gp_session as gs
T = {r['f']: r for r in (gs.parse(l) for l in open('/tmp/pr_u11/dry1/trace.txt')) if r and r['kind'] == 'T'}
for f in (2047, 2057, 2067, 2077, 2087, 2097, 2107, 2117, 2127, 2137, 2157, 2158):
    r = T.get(f)
    print(f, '-' if r is None else 'mode=%X lat=%X spz=%X mpz=%X b1f=%X cred=%X rng=%08X new=%08X e0=%04X'
          % (r['mode'], r['lat'], r['spz'], r['mpz'], r['b1f'], r['cred'], r['rng'], r['new'], r['e0']))
EOF
```

Expected: `test_gp_replay: 1 restart(s) landed`, `all checks passed`; `PR_RESTART`: `all checks passed`; `evidence: 11 of 11 events ok`; `effects: first not reproduced 11, ratchet N 11 ok`; the table of record §K.8 (lat `D 0 1F 0 32 0 1F 32 3B 3C`, 2157 `-`, 2158 `mode=3 … cred=5 rng=0000ABCD`). These judge the port against itself (the dry run proves the port's model and the tool on real port output, not the original).

- [ ] **Step 5: Mutation proofs** (each alone, rebuild, rerun Step 4's first two commands or the judge; restore):

| # | mutation | expected |
|---|---|---|
| P1 | `flow.c`: delete `s_restart_landings++;` | `PR_RESTART`: `FAIL …test_game.c:12710: 0 != 1`; replay: `0 restart(s) landed`, `FAIL …test_game.c:12422: 1849 != 1850` (lines measured on `8eaf25a` with the patch applied) |
| P2 | `test_game.c` `gp_trace_line`: swap `DS_001028DB` and `DS_001028DA` in the last argument line | `effects: … FAIL: first not reproduced 2 < ratchet N 11` |
| P3 | `flow.c` `game_key_loop`: delete the pause's `config_screen_wait(-1);` (`0x24E46/0x24E4B`) | `effects: pause f=809 FAIL: lat 20 at f=809, want 0`, `first not reproduced 1 < ratchet N 11` |

- [ ] **Step 6: Gate** — the light gate (Re-baseline) with `<t>` = `t3`. Expected: `all checks passed` twice, `gp-exit=0`, `GP-IDLE-LOSS-EQUAL` (the replays now print `0 restart(s) landed`), the `gp_compare: gp-u5-charsel` ratchets `516`/`1513` ok, the two `gp_keys: … skipped` lines only from Task 4 on, the diff-verify line, `771 1203 64`, `731 731 100`.

- [ ] **Step 7: Record** — append `### Task 3: the port side` to §K.11: the seam, the `T` fields, Step 2's failure, Step 4's lines, P1–P3, the gate lines.

- [ ] **Step 8: Commit**

```bash
git add port/src/game/flow.c port/src/game/flow.h port/tests/test_game.c docs/superpowers/plans/2026-10-01-gameplay-u11-derivations.md
git commit -m "$(cat <<'EOF'
port: gp replay traces the key latch and pause bytes and counts restart landings (record §K.11)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 4: `make gp-keys-oracle` in `make verify`

**Files:**
- Modify: `Makefile` (`.PHONY`; after the `gp-charsel-oracle` recipe; `verify`)
- Modify: record §K.11

**Interfaces:**
- Consumes: `make gp-replay`, `tools/gp_keys.py`.
- Produces: `make gp-keys-oracle`; `GP_KEYS_MIN_EFFECTS`, `GP_KEYS_CAPTURE_SHA256` (empty until Task 6); `verify` runs it after `gp-charsel-oracle` and adds `tools.tests.test_gp_keys` to the tool-test line.

- [ ] **Step 1: See it missing**

Run: `make gp-keys-oracle 2>&1 | tail -1`
Expected: `make: *** No rule to make target` … `gp-keys-oracle` … `Stop.` (the quoting differs between make versions).

- [ ] **Step 2: Implement** — apply:

```bash
git apply <<'PATCH'
--- a/Makefile
+++ b/Makefile
@@ -53,7 +53,7 @@
         re-info re-gra re-render re-symbols re-cluster re-extract re-extract-test \
         re-decompile re-analyze re-oracle re-original title-pin title-capture \
         title-oracle attract-oracle frontend-capture frontend-oracle demo-oracle demo-fight-oracle \
-        attract2-oracle attract2-compare k11-capture k11-oracle k11-report gp-capture gp-replay gp-oracle gp-report diff-verify gp-charsel-oracle
+        attract2-oracle attract2-compare k11-capture k11-oracle k11-report gp-capture gp-replay gp-oracle gp-report diff-verify gp-charsel-oracle gp-keys-oracle
 
 # ── Environment ──────────────────────────────────────────────────────────────
 
@@ -491,6 +491,25 @@
 		--trace-min-first "$(GP_CHARSEL_TRACE_MIN_FIRST)" --max-start "$(GP_CHARSEL_MAX_START)" \
 		--capture-sha256 "$(GP_CHARSEL_CAPTURE_SHA256)" --capture-frames "$(GP_CHARSEL_CAPTURE_FRAMES)"
 
+# U11 in-match keys (plan 2026-10-01-gameplay-u11-in-match-keys.md, record
+# 2026-10-01-gameplay-u11-derivations.md §K.6/§K.12): tools/gp_keys.py judges
+# each key event of data/k11-captures/gp-keys-fight twice: `evidence` (the
+# capture shows the raw-derived effect) and `effects` (the port's PR_GP_DUMP
+# replay shows it at the same frames; a ratchet: the events before the first
+# the port does not reproduce must number >= GP_KEYS_MIN_EFFECTS). Skips without
+# the capture; with it, an unpinned value or another poll.log fails. Narrow: an
+# event is judged on the latch, the pause bytes, the pad words, mode, b1f, cred
+# and rng only; the pause/prompt frames are not compared (record §K.10).
+GP_KEYS_MIN_EFFECTS =
+GP_KEYS_CAPTURE_SHA256 =
+gp-keys-oracle: build ## In-match keys: evidence + effects ratchet on gp-keys-fight (skips without data/k11-captures/gp-keys-fight)
+	@echo "== in-match keys oracle: gp-keys-fight (evidence, effects ratchet) =="
+	@$(MAKE) --no-print-directory gp-replay scenario=gp-keys-fight GP_OPTIONAL=1
+	@$(PYTHON) tools/gp_keys.py evidence --capture $(K11_CAPTURES)/gp-keys-fight \
+		--capture-sha256 "$(GP_KEYS_CAPTURE_SHA256)"
+	@$(PYTHON) tools/gp_keys.py effects --capture $(K11_CAPTURES)/gp-keys-fight --port $(GP_DUMP)/gp-keys-fight \
+		--min-effects "$(GP_KEYS_MIN_EFFECTS)" --capture-sha256 "$(GP_KEYS_CAPTURE_SHA256)"
+
 gp-report: build ## Report-only gameplay comparison (scenario=gp-…): counts and first differences, no ratchet, exit 0
 	@$(MAKE) --no-print-directory gp-replay scenario=$(scenario) GP_OPTIONAL=1
 	@$(PYTHON) tools/gp_compare.py --report --scenario $(scenario) --capture $(K11_CAPTURES)/$(scenario) --port $(GP_DUMP)/$(scenario)
@@ -544,9 +563,10 @@
 	@echo "== gameplay oracle (frame and trace ratchets; skips without its capture; record §G.16) =="
 	@$(MAKE) --no-print-directory gp-oracle
 	@$(MAKE) --no-print-directory gp-charsel-oracle
+	@$(MAKE) --no-print-directory gp-keys-oracle
 	@$(MAKE) --no-print-directory diff-verify
 	@echo "== k11 and gp tool unit tests =="
-	PR_ORACLE_REQUIRED=1 $(PYTHON) -m unittest tools.tests.test_k11_fields tools.tests.test_k11_session tools.tests.test_k11_capture tools.tests.test_k11_compare tools.tests.test_gp_session tools.tests.test_gp_capture tools.tests.test_gp_compare
+	PR_ORACLE_REQUIRED=1 $(PYTHON) -m unittest tools.tests.test_k11_fields tools.tests.test_k11_session tools.tests.test_k11_capture tools.tests.test_k11_compare tools.tests.test_gp_session tools.tests.test_gp_capture tools.tests.test_gp_compare tools.tests.test_gp_keys
 	@echo "== title_compare unit tests (splice3, record §47-A) =="
 	$(PYTHON) -m unittest tools.tests.test_title_compare
 	@echo "== gra_extract oracle tests (real assets required) =="
PATCH
```

- [ ] **Step 3: Run it without the capture**

Run: `make gp-keys-oracle 2>&1 | grep -E '==|gp-replay:|gp_keys:'; make help | grep gp-keys-oracle`
Expected:

```
== in-match keys oracle: gp-keys-fight (evidence, effects ratchet) ==
gp-replay: no capture at data/k11-captures/gp-keys-fight
gp_keys: gp-keys-fight: no capture at data/k11-captures/gp-keys-fight, skipped
gp_keys: gp-keys-fight: no capture at data/k11-captures/gp-keys-fight, skipped
```

and the help line `gp-keys-oracle  In-match keys: evidence + effects ratchet on gp-keys-fight (skips without data/k11-captures/gp-keys-fight)`. (The failing paths with a present capture — unpinned N, unpinned or wrong sha — are the unit tests of Task 2 and Task 6 Step 6.)

- [ ] **Step 4: Gate** — the light gate, `<t>` = `t4`. Expected as Task 3, plus the two `gp_keys: … skipped` lines and the tool-test line `Ran 120 tests` (107 + 13).

- [ ] **Step 5: Record and commit** — append `### Task 4: make gp-keys-oracle` to §K.11 (the skip lines, `Ran 120 tests`), then:

```bash
git add Makefile docs/superpowers/plans/2026-10-01-gameplay-u11-derivations.md
git commit -m "$(cat <<'EOF'
make: gp-keys-oracle (in-match keys evidence and effects ratchet) in make verify (record §K.11)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 5 (D1): Capture `gp-keys-fight` and check the raw evidence

**Files:**
- Writes (git-ignored): `data/k11-captures/gp-keys-fight/` (only through `make gp-capture`)
- Modify: record §K.12

**Interfaces:**
- Consumes: `SCENARIOS['gp-keys-fight']`, `make gp-capture`, `gp_keys.py evidence`.
- Produces: the capture; its `poll.log` sha256 and frame count (for Task 6).

- [ ] **Step 1: Capture**

```bash
make gp-capture scenario=gp-keys-fight 2>&1 | tee $S/cap.txt | tail -14
du -sh data/k11-captures/gp-keys-fight; cat data/k11-captures/gp-keys-fight/session.txt
shasum -a 256 data/k11-captures/gp-keys-fight/poll.log; ls data/k11-captures/gp-keys-fight/frame_*.raw.gz | wc -l
```

Expected: every `check=ok` (`base`, `steps fired 18/18`, `end frame reached`, `mode 0x27 after the Enter`, `snapshots kb == raw`, `frames written`, `port script v2`), `wall_s` under 90, the size near D1's 70 MB. Branches (harness, not game values): `end frame reached` fails → rerun once with `GP_ARGS="--time-limit 120"` and record both runs; `port script v2` fails (a word consumed after its opener's frame, spec §7 Q5, or a key consumed in a snapshot gap) → rerun once; if it fails again, stop the task and report the failing `ScriptError` with the `I`/`H`/`S` lines around it (record §K.5's assumption is then wrong for that event and the controller decides).

- [ ] **Step 2: The path and the event frames**

```bash
python3 - <<'EOF'
import sys; sys.path.insert(0, 'tools'); import gp_session as gs
L = open('data/k11-captures/gp-keys-fight/poll.log').read().splitlines()
prev = None
for n, l in enumerate(L, 1):
    r = gs.parse(l)
    if not r:
        continue
    if r['kind'] in ('S', 'P') and r.get('mode') != prev:
        print('poll.log:%d %s f=%X mode=%X' % (n, r['kind'], r['f'], r['mode'])); prev = r['mode']
    if r['kind'] in ('I', 'H', 'X', 'E') and 'release' not in r:
        print('poll.log:%d %s' % (n, l[:120]))
EOF
```

Expected: the `gp-idle-loss` path to mode 6 (record §G.18: `0x27`, `0x2D`, `0x1A`, `0x1B`, `0x10`, …, 5, 6 at about `f = 0x7F5`), the presses of steps 3–17 in mode 6 then `0x17` at 10-frame steps, each answer's `H` in its opener's frame, mode `0x17` after F2, then mode 3 after the ESC–Y (a `P` record), the `X` record and `E`. Record the table with `poll.log:<n>` citations. A different path before mode 6 (the pick, the round start) is a correction of the expectation: record it; if mode 6 never came, stop and report.

- [ ] **Step 3: The raw evidence**

Run: `python3 tools/gp_keys.py evidence --capture data/k11-captures/gp-keys-fight`
Expected: eleven `ok` rows and `evidence: 11 of 11 events ok`. **A `FAIL` row is a plan-vs-raw conflict, and the raw wins:** read the row's reasons, the `S` records at `c − 1`, `c`, `F`, `F + 1`, and re-read the raw at the addresses record §K.2–§K.4 give for that rule. If the raw shows the rule was derived wrongly, correct the rule in `tools/gp_keys.py` and its test in a separate commit, with the corrected derivation and the address in §K.12 ("Correction (raw wins)"); if the raw agrees with the rule and the capture does not, the event is a named gap with both pieces of evidence and the scenario is not changed to fit. Never edit a rule to match the capture without a raw derivation.

- [ ] **Step 4: Record and commit** — add `## §K.12 The capture, the replay and the pins` with `### Capture (Task 5)`: `session.txt`, size, sha256, frame count, the Step 2 table, the Step 3 rows (each with `c`, `F` and the observed values), any correction. Then:

```bash
git add docs/superpowers/plans/2026-10-01-gameplay-u11-derivations.md
git commit -m "$(cat <<'EOF'
docs: gp-keys-fight capture: the in-match keys' raw evidence (record §K.12)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 6 (D1): Replay, the miss set, and the pinned ratchet

**Files:**
- Modify: `port/tests/test_platform.c` (after `k_miss_gp_charsel[]`; `fnm_known`; `test_fn_misslog_driver`; the `--check` call)
- Modify: `Makefile` (`GP_KEYS_MIN_EFFECTS`, `GP_KEYS_CAPTURE_SHA256` and their provenance comment)
- Modify: record §K.12

**Interfaces:**
- Consumes: the Task 5 capture; `make gp-replay`; `gp_keys.py effects`.
- Produces: `k_miss_gp_keys_fight[]`; `fnm_known(u32 addr, const char *ctx, int frontend, int idle_loss, int charsel, int keys_fight)` (U5's `charsel` stays 5th); the pinned `GP_KEYS_MIN_EFFECTS` (= the measured K) and `GP_KEYS_CAPTURE_SHA256`.

- [ ] **Step 1: Measure the replay's miss set** (no table yet)

```bash
python3 tools/gp_session.py port-script --scenario gp-keys-fight --capture data/k11-captures/gp-keys-fight --out $S/keys.script
rm -rf $S/keys; PR_GP_DUMP=$S/keys PR_GP_SCRIPT=$S/keys.script PR_GAME_DIR=data/game/C ./build/run_tests 2>&1 | grep -E 'fn-miss|FAIL|landed|passed' | tee $S/keys_miss.txt
```

Expected: `test_gp_replay: 1 restart(s) landed`; the driver's own checks pass; the only `FAIL` lines are the miss log's (`unexpected 0x… from …` and the count line), because `gp-keys-fight` has no pinned set yet. The re-baselined dry run on `8eaf25a` (record §K.8) recorded, beyond the base pair (`0x5D812 actor_spawn`, `0x5D812 set_dead`): `0x29D60 frontend_mode_1b_step`, `0x5D812 frontend_mode_1b_step` (`distinct=4`; the planner's `0x3A588 fighter_state_3531c` row is gone: U6a registered it, record gameplay-u6 §U6.21). The table of Step 2 holds **exactly the measured pairs**: if the measurement differs, change the rows to it, and classify every pair from record §G.24 (`0x29D60` a bare `ret`; `0x5D812` the runtime's `xor eax,eax; ret` stub). A miss of `0x23208`, `0x3A588`, `0x3640C` or `0x37DCC` (ported and registered by U6a) is a regression, not a row: stop and report it. A pair outside §G.24 is disassembled (`build/diffrun --exe data/game/C/PRAGE.EXE --image-out $S/image.bin` and capstone) and named in §K.12 before it is pinned, never registered here. If a driver check other than the miss log fails (a stall, `missed` steps, the `T`-line count), stop and report it with `$S/keys/gp.log`.

- [ ] **Step 2: Pin the set** — apply (rows as measured in Step 1):

```bash
git apply <<'PATCH'
--- a/port/tests/test_platform.c
+++ b/port/tests/test_platform.c
@@ -131,6 +131,14 @@
     { 0x5D812u, "frontend_mode_1b_step" },
 };
 
+/* gp-keys-fight (record 2026-10-01-gameplay-u11 §K.12): the gp-idle-loss path
+ * to round 1, the in-match keys, the join and the 0x24AB0 restart, measured
+ * on its full replay; each pair is one of §G.24's classified misses. */
+static const fnm_pair k_miss_gp_keys_fight[] = {
+    { 0x29D60u, "frontend_mode_1b_step" },
+    { 0x5D812u, "frontend_mode_1b_step" },
+};
+
 /* The scenario named by the first line of PR_GP_SCRIPT ("# gp port script v2:
  * scenario <name>[ (cut at N)]"), and whether the script was cut (--end): a
  * cut replay ends before some misses, so it may record a subset. */
@@ -165,11 +173,13 @@
 
 #define FNM_N(t) (sizeof (t) / sizeof (t)[0])
 
-static int fnm_known(u32 addr, const char *ctx, int frontend, int idle_loss, int charsel)
+static int fnm_known(u32 addr, const char *ctx, int frontend, int idle_loss, int charsel,
+                     int keys_fight)
 {
     if (fnm_in(k_miss_known, FNM_N(k_miss_known), addr, ctx)) return 1;
     if (frontend && fnm_in(k_miss_frontend, FNM_N(k_miss_frontend), addr, ctx)) return 1;
     if (charsel && fnm_in(k_miss_gp_charsel, FNM_N(k_miss_gp_charsel), addr, ctx)) return 1;
+    if (keys_fight && fnm_in(k_miss_gp_keys_fight, FNM_N(k_miss_gp_keys_fight), addr, ctx)) return 1;
     return idle_loss && fnm_in(k_miss_gp_idle_loss, FNM_N(k_miss_gp_idle_loss), addr, ctx);
 }
 
@@ -177,22 +187,24 @@
 {
     int before = g_failures;
     int frontend = strcmp(env, "PR_FRONTEND_DUMP") == 0;
-    int idle_loss = 0, charsel = 0, cut = 0;
+    int idle_loss = 0, charsel = 0, keys_fight = 0, cut = 0;
     if (strcmp(env, "PR_GP_DUMP") == 0) {
         char sc[64];
         fnm_gp_scenario(sc, sizeof sc, &cut);
         idle_loss = strncmp(sc, "gp-idle-loss", 12) == 0;
         charsel = strcmp(sc, "gp-u5-charsel") == 0;
+        keys_fight = strcmp(sc, "gp-keys-fight") == 0;
     }
     u32 want = (u32)FNM_N(k_miss_known) +
                (frontend ? (u32)FNM_N(k_miss_frontend) : 0u) +
                (idle_loss ? (u32)FNM_N(k_miss_gp_idle_loss) : 0u) +
-               (charsel ? (u32)FNM_N(k_miss_gp_charsel) : 0u);
+               (charsel ? (u32)FNM_N(k_miss_gp_charsel) : 0u) +
+               (keys_fight ? (u32)FNM_N(k_miss_gp_keys_fight) : 0u);
     CHECK_EQ_INT(fn_misslog_dropped(), 0);
     if (cut) CHECK(fn_misslog_count() <= want, "a cut gp replay records no more than the pinned set");
     else CHECK_EQ_INT(fn_misslog_count(), want);
     for (u32 i = 0; i < fn_misslog_count(); i++)
-        if (!fnm_known(fn_misslog_addr(i), fn_misslog_ctx(i), frontend, idle_loss, charsel)) {
+        if (!fnm_known(fn_misslog_addr(i), fn_misslog_ctx(i), frontend, idle_loss, charsel, keys_fight)) {
             printf("fn-miss %s: unexpected 0x%05X from %s\n", env,
                    (unsigned)fn_misslog_addr(i), fn_misslog_ctx(i));
             CHECK(0, "the driver's miss log holds only its pinned known-set");
@@ -274,7 +286,7 @@
                 continue;
             }
             n++;
-            if (!fnm_known((u32)addr, ctx, 0, 0, 0)) {
+            if (!fnm_known((u32)addr, ctx, 0, 0, 0, 0)) {
                 printf("fn_miss.txt: unexpected 0x%05X from %s\n", addr, ctx);
                 CHECK(0, "the --check miss log holds only the pinned known-set");
             }
PATCH
cmake --build build 2>&1 | grep -E 'error|warning'
make gp-replay scenario=gp-keys-fight GP_DUMP=$S/gpd 2>&1 | grep -E 'landed|FAIL|passed'
```

Expected: `test_gp_replay: 1 restart(s) landed`, `all checks passed`. Mutation (restore after): delete the `0x29D60` row of `k_miss_gp_keys_fight` → `FAIL …test_platform.c:204: 4 != 3` and `fn-miss PR_GP_DUMP: unexpected 0x29D60 from frontend_mode_1b_step` (re-baseline dry run on `8eaf25a`, script named `gp-keys-fight`; with other measured rows, the counts differ accordingly).

- [ ] **Step 3: Measure the effects**

```bash
python3 tools/gp_keys.py effects --capture data/k11-captures/gp-keys-fight --port $S/gpd/gp-keys-fight --min-effects 0 | tee $S/effects.txt
```

Expected: eleven rows and `first not reproduced K, ratchet N 0 ok (improved: raise N)`. The dry run (record §K.8) predicts K = 11. For each `FAIL` row, record the capture's and the port's values at `c − 1`, `c` (and `F ± 1`) side by side in §K.12 and name the divergence (the first unexplained event, with the raw address of the rule); do not fix it in this task: a fix is a follow-up with its own test.

- [ ] **Step 4: Pin** — in the Makefile replace the two empty lines

```
GP_KEYS_MIN_EFFECTS =
GP_KEYS_CAPTURE_SHA256 =
```

with (K from Step 3, the sha256 from Task 5 Step 1):

```
# MIN_EFFECTS: the port reproduces the first K of the 11 events of record §K.6 on
# data/k11-captures/gp-keys-fight (Task 6, record §K.12: <the effects line>); raise it when
# gp_keys prints "improved: raise N". CAPTURE_SHA256: that capture's poll.log; another capture
# fails until it is re-measured (Task 5 Step 1) and both values re-pinned.
GP_KEYS_MIN_EFFECTS = K
GP_KEYS_CAPTURE_SHA256 = <sha256>
```

Run: `make gp-keys-oracle GP_DUMP=$S/gpd 2>&1 | grep -E '^gp_keys:' | tail -2`
Expected: `evidence: 11 of 11 events ok`, `effects: first not reproduced K, ratchet N K ok`.

- [ ] **Step 5: Mutation proofs of the pins** (each alone, restore):

| # | mutation | expected |
|---|---|---|
| R1 | `make gp-keys-oracle GP_KEYS_MIN_EFFECTS=$((K+1))` | `FAIL: first not reproduced K < ratchet N K+1` (K = 11: `FAIL: ratchet N 12 > 11 events`) |
| R2 | `make gp-keys-oracle GP_KEYS_CAPTURE_SHA256=0000000000000000000000000000000000000000000000000000000000000000` | `FAIL: poll.log sha256 …, pinned 0000… (re-measure and re-pin)` |
| R3 | `make gp-keys-oracle GP_KEYS_MIN_EFFECTS=` | `FAIL: --min-effects is not pinned (first not reproduced K)` |
| R4 | (K ≥ 2) `flow.c` `game_key_loop`: delete the pause's `config_screen_wait(-1);`, rebuild, `make gp-keys-oracle` | `pause … FAIL: lat 20 at f=…, want 0`, `FAIL: first not reproduced 1 < ratchet N K` |

- [ ] **Step 6: The report-only comparison** — `make gp-report scenario=gp-keys-fight GP_DUMP=$S/gpd 2>&1 | grep '^gp_compare' | tee $S/report.txt`. Record the frame and trace lines in §K.12 (not ratcheted here). The values are measured, not predicted: on `8eaf25a` the same path in `gp-idle-loss` has no unexplained frame before capture 2064 (mode 8, `f = 0xC71`) and no traced difference through `f = 0x207F` (record gameplay-u6 §U6.21), so a first unexplained frame or trace difference before the first event's `c` is a finding: name it in §K.12 with the capture's and the port's values.

- [ ] **Step 7: Gate** — the light gate, `<t>` = `t6`. Expected as Task 4, plus the two `gp_keys` ok lines of Step 4.

- [ ] **Step 8: Record and commit** — `### Replay and pins (Task 6)` in §K.12: Step 1's lines and each pair's classification, K and the eleven rows, R1–R4, the report lines, the gate. Then:

```bash
git add port/tests/test_platform.c Makefile docs/superpowers/plans/2026-10-01-gameplay-u11-derivations.md
git commit -m "$(cat <<'EOF'
gp: gp-keys-fight replay miss set and the pinned in-match keys ratchet (record §K.12)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 7 (D2): The windowed host's key binding follows the raw

**Files:**
- Modify: `port/src/host.c` (`k_input_bind` 55–68 → `k_bios_pad`, `host_kb_bit`; `host_key_bits`'s loop)
- Modify: `port/src/host.h` (`host_key_bits`'s comment; `host_kb_bit`)
- Modify: `port/tests/test_platform.c` (top of `test_host`)
- Modify: record §K.13

**Interfaces:**
- Consumes: `k_bios_letter` (host.c).
- Produces: `u16 host_kb_bit(u8 scan)` — a set-1 scan's kb bit under the default binding, 0 for any other scan; `host_key_bits()` built from it.

- [ ] **Step 1: Write the failing test** — apply the test half:

```bash
git apply <<'PATCH'
--- a/port/tests/test_platform.c
+++ b/port/tests/test_platform.c
@@ -2611,6 +2611,19 @@
 {
     int before = g_failures;
 
+    /* The default binding (record gameplay-ground-truth §G.1.2; every bit
+     * captured in gp-pads, §G.7.2): a set-1 scan's kb bit. */
+    {
+        static const u8 scans[18] = { 0x1F, 0x2D, 0x2C, 0x2E, 0x16, 0x17, 0x31, 0x32, 0x3B,
+                                      0x48, 0x50, 0x4B, 0x4D, 0x47, 0x49, 0x4F, 0x51, 0x3C };
+        static const u16 bits[18] = { 0x8000, 0x4000, 0x2000, 0x1000, 0x0100, 0x0200, 0x0400, 0x0800, 0x0100,
+                                      0x0080, 0x0040, 0x0020, 0x0010, 0x0001, 0x0002, 0x0004, 0x0008, 0x0001 };
+        for (int i = 0; i < 18; i++) CHECK_EQ_INT(host_kb_bit(scans[i]), bits[i]);
+        CHECK_EQ_INT(host_kb_bit(0x10), 0);     /* Q: Alt-Q's letter is no pad key */
+        CHECK_EQ_INT(host_kb_bit(0x38), 0);     /* Alt: read by nothing (record u11 §K.2) */
+        CHECK_EQ_INT(host_kb_bit(0x06), 0);     /* '5': the stale "coin" binding */
+    }
+
     /* host_pump()/host_present_rgb()/host_shutdown() before host_init(): the
      * suite runs headless with no window, so all three must be safe no-ops. */
     host_shutdown();
PATCH
cmake --build build 2>&1 | grep -E 'error' | head -3
```

Expected: the build fails on `host_kb_bit` (`call to undeclared function 'host_kb_bit'`, or the link error `_host_kb_bit` undefined).

- [ ] **Step 2: Implement** — apply:

```bash
git apply <<'PATCH'
--- a/port/src/host.c
+++ b/port/src/host.c
@@ -52,21 +52,50 @@
     0x15, 0x2C
 };
 
-/* PORT: the host key binding. Bit 0 is the coin input 0x11F28 debits against;
- * the rest are the held inputs the mode transitions and the menus read. */
-static const SDL_Scancode k_input_bind[16] = {
-    SDL_SCANCODE_5,      /* 0: coin */
-    SDL_SCANCODE_1,      /* 1: player 1 start */
-    SDL_SCANCODE_2,      /* 2: player 2 start */
-    SDL_SCANCODE_UP,     /* 3 */
-    SDL_SCANCODE_DOWN,   /* 4 */
-    SDL_SCANCODE_LEFT,   /* 5 */
-    SDL_SCANCODE_RIGHT,  /* 6 */
-    SDL_SCANCODE_SPACE,  /* 7 */
-    SDL_SCANCODE_A, SDL_SCANCODE_S, SDL_SCANCODE_D, SDL_SCANCODE_F,
-    SDL_SCANCODE_G, SDL_SCANCODE_H, SDL_SCANCODE_J, SDL_SCANCODE_K,
+/* PORT: the host stands in for the IRQ1 key-state table and the ISR sampler
+ * 0x1BBAC (host-owned, record §50-C) with the game's default binding: the
+ * scans +0x2DE..+0x2ED hold, the high bytes of the config words at 0x122C62
+ * (record gameplay-ground-truth §G.1.2; every bit captured in gp-pads,
+ * §G.7.2). The letters map through k_bios_letter; the other pad keys here. */
+typedef struct { SDL_Scancode sdl; u8 scan; } host_scan_pair;
+static const host_scan_pair k_bios_pad[] = {
+    { SDL_SCANCODE_UP, 0x48 },    { SDL_SCANCODE_DOWN, 0x50 },
+    { SDL_SCANCODE_LEFT, 0x4B },  { SDL_SCANCODE_RIGHT, 0x4D },
+    { SDL_SCANCODE_HOME, 0x47 },  { SDL_SCANCODE_PAGEUP, 0x49 },
+    { SDL_SCANCODE_END, 0x4F },   { SDL_SCANCODE_PAGEDOWN, 0x51 },
+    { SDL_SCANCODE_F1, 0x3B },    { SDL_SCANCODE_F2, 0x3C },
 };
 
+/* PORT: 0x1BBAC's device-0 combine for one key, under the default binding:
+ * +0x2D8 = 0x1B610 | 0x1B730 | 0x1B850 (P1: S X Z C give 0x80 0x40 0x20 0x10,
+ * U I N M give 1 2 4 8, F1 (+0x28F) gives 1) and +0x2D9 the same for P2 (the
+ * arrows, Home PgUp End PgDn, F2 at +0x290), as the kb word (+0x2D8 << 8) |
+ * +0x2D9. Any other scan is no pad key. */
+u16 host_kb_bit(u8 scan)
+{
+    switch (scan) {
+    case 0x1F: return 0x8000u;   /* S */
+    case 0x2D: return 0x4000u;   /* X */
+    case 0x2C: return 0x2000u;   /* Z */
+    case 0x2E: return 0x1000u;   /* C */
+    case 0x16: return 0x0100u;   /* U */
+    case 0x3B: return 0x0100u;   /* F1 */
+    case 0x17: return 0x0200u;   /* I */
+    case 0x31: return 0x0400u;   /* N */
+    case 0x32: return 0x0800u;   /* M */
+    case 0x48: return 0x0080u;   /* Up */
+    case 0x50: return 0x0040u;   /* Down */
+    case 0x4B: return 0x0020u;   /* Left */
+    case 0x4D: return 0x0010u;   /* Right */
+    case 0x47: return 0x0001u;   /* Home */
+    case 0x3C: return 0x0001u;   /* F2 */
+    case 0x49: return 0x0002u;   /* PgUp */
+    case 0x4F: return 0x0004u;   /* End */
+    case 0x51: return 0x0008u;   /* PgDn */
+    default: return 0u;
+    }
+}
+
 static int g_key_bits_override_on;
 static u16 g_key_bits_override;
 
@@ -83,8 +112,10 @@
     const bool *st = SDL_GetKeyboardState(NULL);
     if (st == NULL) return 0u;
     u16 bits = 0u;
-    for (int i = 0; i < 16; i++)
-        if (st[k_input_bind[i]]) bits |= (u16)(1u << i);
+    for (int i = 0; i < 26; i++)
+        if (st[SDL_SCANCODE_A + i]) bits |= host_kb_bit(k_bios_letter[i]);
+    for (size_t i = 0; i < sizeof k_bios_pad / sizeof k_bios_pad[0]; i++)
+        if (st[k_bios_pad[i].sdl]) bits |= host_kb_bit(k_bios_pad[i].scan);
     return bits;
 }
 
--- a/port/src/host.h
+++ b/port/src/host.h
@@ -107,12 +107,16 @@
 const char *host_audio_error(void);
 
 /* PORT: the 16 input bits the game's bitfield carries, packed as the two key
- * bytes at DAT_00101514 + 0x2d8/0x2d9. Which physical key drives which bit is a
- * port choice: the original's mapping lives in a hardware keyboard handler and
- * BIOS scancode space SDL does not have. The binding table lives in host.c and
- * is the single place to change it. */
+ * bytes at DAT_00101514 + 0x2d8/0x2d9 (kb word (+0x2D8 << 8) | +0x2D9): the
+ * SDL keys pressed, through host_kb_bit. */
 u16 host_key_bits(void);
 
+/* PORT: one set-1 scan's kb bit under the game's default binding (record
+ * gameplay-ground-truth §G.1.2): P1 S X Z C / U I N M / F1, P2 the arrows /
+ * Home PgUp End PgDn / F2; 0 for any other scan. Stands in for the host-owned
+ * ISR sampler 0x1BBAC's device-0 path. */
+u16 host_kb_bit(u8 scan);
+
 /* PORT: test seam, no raw counterpart. With `on` set, host_key_bits() returns
  * `bits` instead of the SDL keyboard state; the K11 oracle driver holds the key
  * bitmap a capture's poll log recorded through it (named-gaps A record §A.3).
PATCH
cmake --build build 2>&1 | grep -E 'error|warning'; echo built
PR_GAME_DIR=data/game/C ./build/run_tests 2>&1 | tail -1
grep -n k_input_bind port/src/*.c port/src/*/*.c port/src/*/*/*.c
```

Expected: `all checks passed`; no `k_input_bind` left.

- [ ] **Step 3: Mutation proof** — in `host_kb_bit`, `case 0x3C: return 0x0001u;   /* F2 */` → `return 0x0100u;`, rebuild, run: `FAIL …test_platform.c:<line>: 256 != 1`, `FAILURES: 1`. Restore. A second one: `default: return 0u;` → `return 1u;` → the three zero checks fail (`1 != 0` ×3).

- [ ] **Step 4: Gate** — the light gate, `<t>` = `t7`. Expected as Task 4 (Task 6's lines too when D1 ran): no oracle line can move (every driver uses `host_set_key_bits_override`).

- [ ] **Step 5: Record and commit** — `## §K.13 The host binding (Task 7)`: the raw truth (record §K.7), the new table, the mutation lines, the gate; the named gaps that stay (the SDL → set-1 table is untested headless; configured bindings; `translate_key`'s missing F1/F2/Home/PgUp/End/PgDn words). Then:

```bash
git add port/src/host.c port/src/host.h port/tests/test_platform.c docs/superpowers/plans/2026-10-01-gameplay-u11-derivations.md
git commit -m "$(cat <<'EOF'
host: key bitmap follows the game's default binding (P1 S X Z C U I N M F1, P2 arrows Home PgUp End PgDn F2) (record §K.13)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
EOF
)"
```

---

### Task 8: Closure

**Files:**
- Modify: record §K.14
- Modify: `docs/PROGRESS.md` (append one paragraph)

**Interfaces:** none.

- [ ] **Step 1: Final gate** — `<t>` = `t8`; all lines as Task 6/7. `git diff --stat main -- port/src/symbols.h` → empty.

- [ ] **Step 2: Record §K.14** — what U11 delivered; the claim (record §K.10, verbatim, with K and the capture sha); spec §7 Q5 answered (no tick-keyed step; record §K.5); Q2's in-match part (F1 = P1 b0 in a fight, F2 = P2's join, from the capture); the coin question (no coin key, record §K.4); the stale host claim (fixed in Task 7, or still open for the Closeout's O10 if D2 was "no"); every named gap of §K.10 restated; the merge notes of "Shared-file touch points".

- [ ] **Step 3: PROGRESS paragraph** — append to `docs/PROGRESS.md`:

```markdown
**Gameplay U11 — in-match keys (2026-10-01).** The keys the game acts on in a match are the int 16h key loop's (`0x24CFE..0x24EE7`: Enter latches only; space pauses until the next space; ESC asks ABANDON CONQUEST? Y/N, whose yes is the `0x24AB0` soft restart; Alt-Q asks QUIT TO DOS? Y/N; Alt-S/Alt-M toggle the sample/music pause; Alt-J calibrates the joystick, host-owned) and the pad bits the timer ISR samples (F1/F2 are the start bits; F2 mid-match joins side 1 for free, `0x2CA93`); there is no coin key (no instruction adds a credit). Capture `gp-keys-fight` (D1: <taken, sha256 …, K of 11 reproduced | not taken>) and `make gp-keys-oracle` (evidence + effects ratchet `GP_KEYS_MIN_EFFECTS`, skips without the capture). The replay driver counts restart landings (`game_restart_landings`, `PORT:` seam). <Task 7: the windowed host's bitmap now follows the default binding | not done>. Named gaps: the pause/prompt frames, a pause left open, QUIT TO DOS's yes, Alt-J, the physical keyboard. Record `docs/superpowers/plans/2026-10-01-gameplay-u11-derivations.md`.
```

(Fill the two `<…|…>` choices from what ran; no other text changes.)

- [ ] **Step 4: Commit**

```bash
git add docs/superpowers/plans/2026-10-01-gameplay-u11-derivations.md docs/PROGRESS.md
git commit -m "$(cat <<'EOF'
docs: U11 in-match keys closure, named gaps and PROGRESS (record §K.14)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
EOF
)"
```

---

## Execution notes

- **What the planner ran (record §K.0, §K.8):** every code block above. The patches were produced from a scratch tree and applied with `git apply`, in this plan's order (Tasks 1, 2, 3, 4, 7, 6), to a copy of `main` `e9271df`; the result equals the scratch tree byte for byte. Measured there: Task 1 `Ran 38 tests OK` after `KeyError: 'gp-keys-fight'`; Task 2 `Ran 77 tests OK` after `ModuleNotFoundError`, M1–M9 each failing as listed; Task 3 `1849 != 1850` before, `1 restart(s) landed` / `all checks passed` / `11 of 11` / `ratchet N 11 ok` after, P1–P3 as listed; Task 4 the skip lines and `Ran 117 tests`; Task 7 `all checks passed`, the F2 mutation `256 != 1`; Task 6 Step 2's table on the dry run (`all checks passed`; dropping `0x3A588` → `5 != 4`); `make gp-oracle` with every change applied: `ratchet N 203 ok`, `ratchet N 2088 ok`, `0 restart(s) landed`; the `gp-pads` replay `all checks passed`; `port_progress` `771 1203 64` / `731 731 100`. Not run by the planner: `make verify` end to end, any DOSBox-X capture.
- **What the re-baseline ran (main `8eaf25a`, see "Re-baseline"):** every code block of Tasks 1–4, 6 (with the two measured rows) and 7, extracted from this file and applied with `git apply` to a `git archive` of `8eaf25a`, and the mutation tables S1–S4, M1–M9, P1–P3, Task 6's row deletion and Task 7's two mutations. With everything applied, the full `make verify` (gate variables pointed at scratch): `verify-exit=0`, `ORACLES-EQUAL`, `WAV-IDENTICAL` (`make audio-render`), `ratchet N 2064 ok`, `ratchet N 8320 ok`, `ratchet N 516 ok`, `ratchet N 1513 ok`, `0 restart(s) landed` on every replay, the two `gp_keys: … skipped` lines, `diff-verify: 6/6 functions VERIFIED; 7/7 mutants detected`, the tool-test line `Ran 120 tests`, `771 1203 64` / `731 731 100`, `symbols.h` regenerated identically. The light gate's commands were run once on the same tree with the same results. Not run: any DOSBox-X capture.
- **Order:** 0 → 1 → 2 → 3 → 4 → 7 (if D2) → 5 → 6 (if D1) → 8. Tasks 5–6 need D1; Task 7 needs D2; Task 7 may run before or after 5–6 (its patch applies either way).
- **Model tiers:** Task 0 haiku; Tasks 1, 2, 4, 7, 8 sonnet; Task 3 sonnet (C, restart semantics); Tasks 5 and 6 opus (a live capture, raw-vs-capture triage, measured pins).
- **Gate (speed-up ruling, 2026-10-01):** the full gate of "Where to run" at Task 0, Task 8 and before merge; after Tasks 3, 4, 6 and 7 the light gate (Re-baseline); after Tasks 1 and 2 the Python suites. A red gate stops the run.
