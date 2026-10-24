# Endgame Truth Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Capture and pin the remaining Primal Rage endgame — the U16 finisher reaction, the five missing character endings (TALON, VERTIGO, CHAOS, ARMADON, DIABLO), and (conditional) the U14 fought live win — then claim the U13 post-script tail from its existing capture, correct the 5120 band's comment, and document the INT 8 tick vector. The game-set `b0c` is already pinned by U10 and is not re-captured.

**Architecture:** Capture-driven, on the existing `gp_session`/`gp_capture`/`gp_win`/`gp_twop`/`gp_compare` harness. Each ending clones the U13 scenario with its character-select cell, land mark and character name, reusing the U10 steps and the `_ending_tail` milestone helper. The U16 finisher reuses the two-human start with a revised KO round and the existing `gp_twop.finisher` check. The U14 retake revises the no-poke fight script and uses the existing `live_death` rule. The U13 tail is an existing-capture replay extension gated by an opt-in `port-script` flag; the tick vector is documentation only. No portable C code is expected.

**Tech Stack:** Python 3 capture/compare tools, DOSBox-X, the C/SDL3 port, the committed Ghidra decompilation (`port/decomp/`) and the raw fixup-applied bytes, GNU Make oracles.

**Spec:** `docs/superpowers/specs/2026-10-23-endgame-truth-design.md`

## Global Constraints

- Work from the repo root. `make`, `run_tests` and the capture tools resolve `data/game/C` and `data/k11-captures/` relative to it.
- `data/` is read-only. Captures are written only by `make gp-capture scenario=gp-…` to `data/k11-captures/<scenario>`; never write a capture by hand. The capture is accepted only after the session's own `check=ok` lines and `check-input` pass.
- Every new scenario is added to `gp_session.STOP_AT_END`. `gp_capture` stops `STOP_TAIL` frames after the `X` and runs the stopped-at-end check only for that set; without it the capture runs to `time_limit` and the post-script tail moves the pins.
- Every new pin is measured from the capture/report: `MIN_FIRST`, `TRACE_MIN_FIRST` (`MOVES_MIN_FIRST` for the U16 path), `MAX_START`, `MIN_REACH`, `CAPTURE_SHA256`, `CAPTURE_FRAMES`; the ending captures also `MILESTONES` and `WIN_MIN_FIRST`. Each pin must fail when moved one step in the tested direction; `GP_WIN_KEEP=1` keeps the dump for measurement.
- Every per-frame allowance binds exact bytes and has a mutation test. No allowance is added without a measured unexplained frame and a byte recipe. No fitted constant; a cause that cannot be named stays a named gap and the first unexplained frame records it.
- Poked captures: the WIN_MENU path, the seven land marks and the `0x10789E=0x78` KO are harness pokes. The record and the oracle comment name every poke; the claim covers only the game-produced content (the mode-0x1F ending and the finisher reaction). The U14 retake has no pokes.
- No C change is expected. If a capture exposes a port divergence, the fix needs the raw instruction, a `PORT:` note, a differential row or existing row case, and the full gate. A divergence that cannot be fixed with evidence stays a named gap.
- The full gate is `PR_DIFF_FULLSCAN=1 make verify`. The new oracle targets are added to `verify`; each skips only when its capture directory is absent. `PR_ORACLE_REQUIRED=1` is set by `make verify`.
- `python3 tools/port_progress.py` must stay `799 1203 66` / `759 759 100`; the differential counter must stay `1031/1031 functions VERIFIED; 14983/14983 mutants detected; 0 named gaps; 813/844 rows with callees closed (187 none); 806/844 full; 8 ported with no row; 31 open`.
- Docs updated: `docs/PROGRESS.md`, `README.md`, `AGENTS.md`, `port/RE_GUIDE.md`, `port/spec/game_flow.md` where relevant, and the derivation record. Commit style `<area>: <what>`; stage named files, never `git add -A`; commit only when the user has authorized the phase's commits.

## Review Focus

The spec implies these failure modes; each gets a test in the task named.

1. **A poked reply passed off as the fought win.** The U14 claim requires no pokes, the game-set `b0c=1` after `(0x09,1)` and no `DEATH_DONE` write. Task 8's tests must fail when the scenario contains a poke, when `live_death` sees a `DEATH_DONE` W, or when `b0c` is missing after the KO.
2. **A wrong character, cell or land mapping.** A wrong cell silently plays the previous character's ending; a wrong land mark never reaches the seventh-land row. Tasks 3–7's tests must assert the scenario's cell walk, the confirmed `c0=char`, the land marks `0x82`..`0x86` on the mode-0x08 row, and the seventh-land `t104` count. The `c0` is the S field; the cell is the scenario's step; the ending title is the record's `0xA80E4` evidence, not a WIN field.
3. **A loader overlay or scan splice mistaken for a port divergence.** A new ending's `- LOADING -` strip or scan-out is not a render defect. Tasks 3–7 may add only byte-pinned `OVERLAY_ALLOWED`/`SCAN3_ALLOWED`/`BANDS_ALLOWED` recipes with mutation tests; otherwise the first unexplained frame stays.
4. **A shared script guard changed for the U13 tail.** The opt-in `--end-to-last-s` must leave every other scenario's generated script byte-identical. Task 9's test compares the default output for a synthetic scenario whose `X` is its last `S`.
5. **An oracle that skips silently.** A missing or replaced capture must not pass as green. Every new target pins `poll.log`'s sha256 and the frame count, the new scenarios are in `STOP_AT_END`, and `make verify` runs the targets only with the phase's captures present. Task 11's gate output is checked for each new oracle line, not just exit 0.

---

### Task 1: Derive the endgame evidence from the raw bytes (evidence only)

**Files:**
- Create: `docs/superpowers/plans/2026-10-23-endgame-truth-derivations.md`
- Read: `port/decomp/prage.c`, `port/decomp/prage.functions.csv`, `tools/gp_session.py`, `tools/gp_win.py`, `tools/gp_twop.py`, `data/k11-captures/gp-u13-ending2` (read-only)

**Interfaces:**
- Produces: record §E1 with the confirmed character/cell/ending table and exact titles, the U16 gate/entries/reactions and the S fields that carry them, the U10 `b0c` evidence, the U14 KO/script facts, the U13 tail S-record range, the 5120 8/1/4 evidence and the INT 8 bytes. Later tasks cite §E1's addresses; the record wins over the spec and the agents' reports.

- [ ] **Step 1: Confirm the seven-character ending map.** Read the mode-0x1F path at `0x208F8` and the character id read at `0x20900..0x20914`; confirm the tables `0xA8090/0xA80AC` (state-1 actors), `0xA80C8` (state-3 actor), `0xA7EBC` (state-3 stream), `0xA818C` (state-4 actor), `0xA80E4` (state-4 title string `0xDE..0xE4`); confirm `0xC8882 = 00 02 03 01 06 04 05` and the exact titles: THE FEAST OF SAURON, BLIZZARD'S SOLITUDE, TALON'S DREAM, THE PALACE OF VERTIGO, THE MEDITATION OF ARMADON, DIABLO'S HELL, CHAOS' REDEMPTION. Record the instruction/table addresses. The `gp_win.py` mode-0x1F label takes the character name, not the title.

- [ ] **Step 2: Confirm the U16 finisher path and fields.** Read `0x3766E test byte [ebx+0x42],0x20`, `0x37678 test byte [ebx+0x43],8`, the `0x36F10` arm A (`0x36FC8 or edx,0x820`, `0x36FD4 mov word [esi+0x42],dx`), the `0x36E78` bit `0x10` path from `0x392A0`'s KO at `0x3961E`, and the mode-9 `0x258` hold (`0x28012`). Record the three entries/reactions/callbacks. Confirm the S fields: `s0_43` exists, no `+0x42` field does, and the existing `gp_twop.finisher` requires at least one reaction unless `--allow-missing-reaction` (`tools/gp_twop.py:173-190`).

- [ ] **Step 3: Confirm the `b0c` evidence is U10's.** Read U10's `poll.log`: its first `b0c=1` at f=0x1602 in mode 0x0F, 190 frames after the seventh poked final KO; `GP_ENDING_WIN_MIN_FIRST=9954` (`Makefile:1139/1212`). Read the U10 steps (`tools/gp_session.py:445-451`): the `DEATH_DONE` pokes replace the mode-0xD wait, and the seventh opponent's `after_entry 0x0D,7,10` consumer is the last one. Confirm the final sets `b14=1` and `0x36F10`'s arm A is gated on `b14==0`, so the `0x36F10` write is not the final's `b0c` path. Record that a new b0c capture is not needed and that U14's `live_death` anchors on `(0x09,1)`, rejects any `DEATH_DONE` write and requires `b0c=1` after the KO.

- [ ] **Step 4: Confirm U14 and U13.** Read the U14 scenario's `U14_ATTEMPTS`/`U14_PUNCH`/`U14_KICK`/`WIN_MENU`/`time_limit` and the failed retry's S records: round 1 P1 KO'd (`s0_5a=0x78`, P2 `s1_5a=0x00`); round 2 P1 KO'd (`s0_5a=0x78`), P2 `s1_5a=0x38`, `ad4=1` (P2 won), mode 3 at `0x1D68`. Read the U13 capture's `X` f=10013 (0x271D), last `S` f=2754 (10068), and the `gp_session.port_script` `end > last` guard (`tools/gp_session.py:970-977`).

- [ ] **Step 5: Confirm the 5120 and tick evidence.** Re-measure the 13 pixels (rows 1..4, x 21..298; 8 are `00001c` where 4317's index is `0x32`, 1 is black where it is `0x2E` (`100000`), 4 are black where it is `0x4D` (`00000c`); the one-capture-frame occurrence; the empty static-index intersections; the corner-ornament candidate `0x01047345`). Confirm the INT 8 bytes at `0x66519..0x6652C` (`b8 08 00 00 00 ba 7e 62 05 00 66 8c cb b4 25 1e 66 8e db cd 21`), the `0x0005627E` immediate, the handler `0x6627E`, the PIT routine `0x663A4`/`0x663AC`/`0x663D4`, 19,887, 59.998 Hz, and `0x2D62C`'s counter role (`0x1BDF4` at `0x1BE28`).

- [ ] **Step 6: Write record §E1.** Put the evidence tables and the exact addresses in `docs/superpowers/plans/2026-10-23-endgame-truth-derivations.md`. No code and no capture. Mark each value measured or inferred; the raw bytes win over the spec.

- [ ] **Step 7: Review and commit.** A reviewer checks that every value later tasks use has an address in §E1 and that no value is a fitted number. Commit (when authorized): `docs: derive the endgame evidence`.

---

### Task 2: Capture and pin the U16 finisher

**Files:**
- Modify: `tools/gp_session.py` (the `U16_FINISHER`/`SCENARIOS['gp-u16-twop2']` area and `STOP_AT_END`)
- Modify: `tools/tests/test_gp_session.py`
- Modify: `tools/gp_compare.py` (only if the new capture requires a measured allowance)
- Modify: `Makefile` (the `GP_U23_*` pins, `gp-u23-oracle`, `verify`)
- Modify: `docs/PROGRESS.md`, `docs/superpowers/plans/2026-10-23-endgame-truth-derivations.md`
- Create (by capture, git-ignored): `data/k11-captures/gp-u23-u16-finisher/`

**Interfaces:**
- Consumes: `U16_FINISHER`, `steps_before_end(SCENARIOS['gp-twop']['steps'])`, `_u16_round(attempts, first_gap, anchor)`, the existing `gp_twop.py check --finisher` (which requires a reaction unless `--allow-missing-reaction`).
- Produces: `SCENARIOS['gp-u23-u16-finisher']` in `STOP_AT_END`, a capture whose S records carry at least one of `r0/r1` in `0x32..0x34`, `GP_U23_*` pins, `gp-u23-oracle`. The old `gp-u16-twop2` scenario and pins are unchanged; `gp_twop.py` is not modified.

- [ ] **Step 1: Write the failing test.** In `tools/tests/test_gp_session.py` add a test that `SCENARIOS['gp-u23-u16-finisher']` exists in `gs.STOP_AT_END`, contains `U16_FINISHER`'s three entry/reaction groups, and has a KO round with more than 32 attempts. Do not add a `gp_twop` test: the existing `test_gp_twop.py` already covers `finisher()`'s reaction requirement and the CLI. Run:
  ```bash
  python3 -m unittest tools.tests.test_gp_session
  ```
  Expected: FAIL (the scenario/STOP_AT_END entry is absent).

- [ ] **Step 2: Add the scenario.** In `tools/gp_session.py`:
  ```python
  SCENARIOS['gp-u23-u16-finisher'] = dict(time_limit=300, steps=(
      steps_before_end(SCENARIOS['gp-twop']['steps']) +
      _u16_round(32, 30) +
      _u16_round(160, 10, anchor=(0x06, 2)) +
      (('after_entry', 0x09, 1, 10, U16_FINISHER[0][-1]),) + U16_FINISHER[1:] +
      (('until_mode', 0x12, 0),)
  ))
  ```
  Add `'gp-u23-u16-finisher'` to `STOP_AT_END`. The 160 attempts and the `p1.right` every 8 are harness values aimed at a real KO; `time_limit=300` is the backstop. If the live KO cannot be landed in the capture budget, the fallback is `after_entry 0x06,2` with `0x10789E=0x77` and one real `1A/1B` hit (the damage poke alone is not the claim; label the fallback in the record and the oracle comment). The claim is at least one of the three reactions; the record lists which appeared.

- [ ] **Step 3: Confirm no tool change.** The existing `gp_twop.py finisher` already fails when no `r0`/`r1` is in `0x32..0x34` and `allow_missing_reaction` is false. The new target simply invokes `gp_twop.py check --finisher` without `--allow-missing-reaction`; leave `gp_twop.py` unchanged.

- [ ] **Step 4: Run the unit tests.** Run the Step 1 command; expected PASS. Do not run the capture yet.

- [ ] **Step 5: Capture.** From the repo root:
  ```bash
  make gp-capture scenario=gp-u23-u16-finisher
  ```
  Run it with the shell timeout disabled or in the background; the capture is up to ~300 s plus checks. Do not type in the DOSBox-X window. The session must print every `check=ok` line, including steps, end frame, pokes 0/0 (or the labeled fallback), snapshots, `check-input` clean, and stopped-at-end.

- [ ] **Step 6: Measure and pin.** Add the `gp-u23-oracle` target from Step 7 first (or run `gp-replay` + `gp_compare.py --report` directly), then run it with the dump retained:
  ```bash
  GP_WIN_KEEP=1 make gp-u23-oracle
  ```
  Record `gp_twop.py check --finisher` (no allowance), the uncapped `gp_compare.py --report` first unexplained frame and window start, the trace/moves first difference, and the replay reach. If the report has an unexplained loader overlay or scan-out, add only the measured byte-pinned allowance to `OVERLAY_ALLOWED`/`SCAN3_ALLOWED`/`BANDS_ALLOWED` with its mutation test; otherwise leave the first unexplained as the pin. Fill `GP_U23_MIN_FIRST`, `TRACE_MIN_FIRST`, `MOVES_MIN_FIRST`, `MAX_START`, `MIN_REACH`, `CAPTURE_SHA256`, `CAPTURE_FRAMES`. Prove each pin fails one step above (e.g. `MIN_FIRST`+1, `MAX_START`-1, a one-byte sha change).

- [ ] **Step 7: Add the Makefile target.** Add `gp-u23-oracle` next to `gp-u16-oracle`, with the same shape: `test_gp_twop`, `gp_twop.py check --finisher` (no `--allow-missing-reaction`), `gp-replay scenario=gp-u23-u16-finisher GP_OPTIONAL=1`, `gp_compare.py --scenario gp-u23-u16-finisher --min-first … --trace-min-first … --moves-min-first … --max-start … --min-reach … --capture-sha256 … --capture-frames …`, and the `GP_WIN_KEEP` dump cleanup. Add `@$(MAKE) --no-print-directory gp-u23-oracle` to `verify` after `gp-u17-oracle`. Document the pins' provenance in the Makefile comment.

- [ ] **Step 8: Confirm the claim and commit.** Confirm at least one of `0x32/0x33/0x34` is in the S records and the mode 9/0x12 entries are present. The raw `+0x42`/`+0x43` gate bits are decomp evidence, not S fields; do not claim them as captured fields. Confirm the old `gp-u16-oracle` output and pins are unchanged. Update `docs/PROGRESS.md` and §E2 of the record. Commit (when authorized): `gameplay: capture the U16 finisher`.

---

### Task 3: TALON's ending (character 2, cell 1)

**Files:**
- Modify: `tools/gp_session.py` (the `ending_menu(cell)` helper, the scenario, `STOP_AT_END`)
- Modify: `tools/gp_win.py` (`start_char(char, cell)`, `MILESTONES['gp-u18-talon-ending']`)
- Modify: `tools/gp_compare.py` (only if the new capture requires a measured allowance)
- Modify: `tools/tests/test_gp_session.py`, `tools/tests/test_gp_win.py`, `tools/tests/test_gp_compare.py` (only for an allowance)
- Modify: `Makefile` (`GP_U18_*`, `gp-u18-oracle`, `verify`)
- Modify: `docs/PROGRESS.md`, the record
- Create (by capture): `data/k11-captures/gp-u18-talon-ending/`

**Interfaces:**
- Consumes: `WIN_MENU`, `WIN_EXTRA`, `lands_marked(side, char)`, `SCENARIOS['gp-u10-ending']['steps']`, `_START`, `_MATCH1`, `_ending_tail(c0, name)` (the second argument is the character name), `_all`, `NO_RESULT`, `STOP_AT_END`.
- Produces: `ending_menu(cell)` (the `WIN_MENU` prefix plus `cell` right presses and a confirm), `start_char(char, cell)` (the character-select and round-1 rows with `c0=char, b1e=1`), `SCENARIOS['gp-u18-talon-ending']` in `STOP_AT_END`, `MILESTONES['gp-u18-talon-ending']`, `GP_U18_*`, `gp-u18-oracle`. Tasks 4–7 reuse the two helpers.

- [ ] **Step 1: Write the failing tests.** In `test_gp_session.py`, assert `ending_menu(1)` has one `p1.right` after the `mode 0x10` wait, a `p1.start`, `ending_menu(0) == WIN_MENU`, and `ending_menu(3) == WIN_MENU1` (the U13 steps); assert `SCENARIOS['gp-u18-talon-ending']` is in `gs.STOP_AT_END` and pokes `lands_marked(0, 2)` at `0x108106` and `0x78` at `0x10789E`, and clones the U10 steps after its first poke. In `test_gp_win.py`, assert `start_char(2, 1)`'s label names character 2/cursor 1 and its predicate accepts `c0=2,b1e=1` and rejects `c0=1`; assert the `MILESTONES['gp-u18-talon-ending']` mode-0x1F row's predicate accepts `ad4=0,c0=2` and its label is `TALON's ending` (the character name, not the title); assert the mode-0x08 row expects the `0x82` land marks and the seventh-land row checks `t104 & 0xFF == 7`; add `gp-u18-talon-ending` to the sorted scenario list and the 30-row tuple in `test_rows_cover_all_scenarios_in_order`. Do not assert `start_char(1, 3) == _START1` by tuple equality: the predicates are lambdas and compare by identity; test the row's label/cell and the predicate's behavior instead. Run:
  ```bash
  python3 -m unittest tools.tests.test_gp_session tools.tests.test_gp_win
  ```
  Expected: FAIL (helpers/scenario absent).

- [ ] **Step 2: Add the helpers.** In `tools/gp_session.py`:
  ```python
  def ending_menu(cell):
      """WIN_MENU's prefix, then `cell` right presses (the first at mode 0x10 + 60,
      the rest 30 apart, hold 4) and the confirm. cell 0 is WIN_MENU's own confirm."""
      if cell == 0:
          return WIN_MENU
      steps = [('after_mode', 0x10, 60, ('pad', ('p1.right',), 4))]
      steps += [('after', 30, ('pad', ('p1.right',), 4)) for _ in range(cell - 1)]
      steps.append(('after', 30, ('pad', ('p1.start',), 6)))
      return WIN_MENU[:-1] + tuple(steps)
  ```
  and in `tools/gp_win.py`:
  ```python
  def start_char(char, cell):
      return (
          ('character select (mode 0x10)', 0x10, 1, None),
          ('round 1: P1 is character %d (cursor %d confirmed, 0x43CAD)' % (char, cell),
           0x06, 1, _all(c0=char, b1e=1)),
      )
  ```
  `ending_menu(3)` must equal `WIN_MENU1`; add that equality to the test. Leave `WIN_MENU1`/`SCENARIOS['gp-u13-ending2']` unchanged.

- [ ] **Step 3: Add the TALON scenario and milestones.** In `tools/gp_session.py`:
  ```python
  SCENARIOS['gp-u18-talon-ending'] = dict(
      time_limit=240, extra=WIN_EXTRA, memsize=64,
      steps=ending_menu(1) + (
          ('after_entry', 0x06, 1, 10,
           ('poke', ((0x108106, lands_marked(0, 2)), (0x10789E, b'\x78')))),
      ) + SCENARIOS['gp-u10-ending']['steps'][len(WIN_MENU) + 1:])
  ```
  Add `'gp-u18-talon-ending'` to `STOP_AT_END`. In `tools/gp_win.py`:
  ```python
  MILESTONES['gp-u18-talon-ending'] = start_char(2, 1) + (
      ('round 1 KO with the seven lands poked P1\'s', 0x08, 1,
       lambda r: r['s1_5a'] == 0x78 and r['w2'] == 1 and r['w3'] == 0
       and r['ad4'] == NO_RESULT and lands(r) == [0x80 | r['c0']] * 7),
  ) + _MATCH1[1:] + _ending_tail(2, 'TALON')
  ```
  The land mark is `0x80 | 2 = 0x82`; the record's ending title is TALON'S DREAM (a `0xA80E4` evidence/compared-frames fact, not the milestone label). Run the Step 1 tests; expected PASS.

- [ ] **Step 4: Capture.** `make gp-capture scenario=gp-u18-talon-ending` (shell timeout disabled/background; ~207 s plus checks; no typing). The session must print all `check=ok` lines, including stopped-at-end, and `check-input` clean; the capture's `poll.log` is the identity.

- [ ] **Step 5: Measure the report and allowances.** Add the `gp-u18-oracle` target from Step 6 first (or run `gp-replay` + `gp_compare.py --report` directly), then run it with the dump retained:
  ```bash
  GP_WIN_KEEP=1 make gp-u18-oracle
  ```
  Run the uncapped report and record the first unexplained frame, the window start, the trace/win first difference and the replay reach. If the report has an unexplained terminal loader overlay or scan-out, add only the measured allowance to `SCAN3_ALLOWED`/`BANDS_ALLOWED`/`OVERLAY_ALLOWED` with its exact bytes, and add the mutation test in `test_gp_compare.py`. Do not add an allowance for a frame with a possible port divergence.

- [ ] **Step 6: Pin and wire the oracle.** Fill `GP_U18_MIN_FIRST`, `TRACE_MIN_FIRST`, `MAX_START`, `MIN_REACH`, `CAPTURE_SHA256`, `CAPTURE_FRAMES`, `MILESTONES`, `WIN_MIN_FIRST`. Add `gp-u18-oracle` with the same shape as `gp-u13-oracle`: `test_gp_win`, `gp_win.py check`, `gp-replay`, `gp_compare.py`, `gp_win.py path --min-milestones … --win-min-first …`, the capture sha. Add `@$(MAKE) --no-print-directory gp-u18-oracle` to `verify` after `gp-u23-oracle`. Prove each pin fails one step above.

- [ ] **Step 7: Confirm the claim and commit.** Confirm `gp_win.py check` reaches the mode-0x1F row with `ad4=0,c0=2` and the `TALON's ending` label; the `MILESTONES` count; the capture's `poll.log` sha and frame count; the record's title TALON'S DREAM; the mode-0x1F frames lie inside the compared window. The 4248/4320/5120 recipes and the U13 pins are untouched. Update `docs/PROGRESS.md` and the record §E3. Commit (when authorized): `gameplay: capture TALON's ending`.

---

### Task 4: VERTIGO's ending (character 3, cell 2)

**Files:**
- Modify: `tools/gp_session.py` (the scenario, `STOP_AT_END`), `tools/gp_win.py` (`MILESTONES`)
- Modify: `tools/tests/test_gp_session.py`, `tools/tests/test_gp_win.py`
- Modify: `Makefile` (`GP_U19_*`, `gp-u19-oracle`, `verify`)
- Modify: `docs/PROGRESS.md`, the record
- Create (by capture): `data/k11-captures/gp-u19-vertigo-ending/`

**Interfaces:**
- Consumes: `ending_menu(cell)`, `start_char(char, cell)`, `_ending_tail`, `WIN_EXTRA`, `lands_marked`, `STOP_AT_END`.
- Produces: `SCENARIOS['gp-u19-vertigo-ending']`, `MILESTONES['gp-u19-vertigo-ending']`, `GP_U19_*`, `gp-u19-oracle`.

- [ ] **Step 1: Write the failing test.** Assert the scenario is in `STOP_AT_END`, uses `ending_menu(2)`, pokes `lands_marked(0, 3)` (`0x83`) at `0x108106`, and the milestone's mode-0x1F row accepts `ad4=0,c0=3` with the `VERTIGO's ending` label; the record's title is THE PALACE OF VERTIGO. Add `gp-u19-vertigo-ending` to the `test_gp_win` scenario list and the 30-row tuple. Run the two test modules; expected FAIL.
- [ ] **Step 2: Add the scenario/milestones.** Build the scenario with Task 3's `dict(time_limit=240, extra=WIN_EXTRA, memsize=64, steps=...)` shape: `ending_menu(2)`, `lands_marked(0, 3)`, `0x10789E=0x78`, the U10 steps, `start_char(3, 2)`, the same seventh-land row as Task 3, `_MATCH1[1:]`, `_ending_tail(3, 'VERTIGO')`; add the scenario to `STOP_AT_END`. Run the tests; expected PASS.
- [ ] **Step 3: Capture.** `make gp-capture scenario=gp-u19-vertigo-ending`; all session checks, stopped-at-end, and `check-input` clean.
- [ ] **Step 4: Measure, pin, wire.** Follow Task 3 Steps 5–6 with `GP_U19_*` and `gp-u19-oracle`; add any measured allowance with its test; add the target to `verify`.
- [ ] **Step 5: Confirm and commit.** `gp_win.py check` reaches `ad4=0,c0=3` and the `VERTIGO's ending` label; the record's title THE PALACE OF VERTIGO; the capture identity is pinned; the existing pins are untouched. Update the record/PROGRESS; commit `gameplay: capture VERTIGO's ending`.

---

### Task 5: CHAOS' ending (character 6, cell 4)

**Files:**
- Modify: `tools/gp_session.py`, `tools/gp_win.py`, `tools/tests/test_gp_session.py`, `tools/tests/test_gp_win.py`, `Makefile`, `docs/PROGRESS.md`, the record
- Create (by capture): `data/k11-captures/gp-u20-chaos-ending/`

**Interfaces:**
- Consumes: the Task 3 helpers; produces `SCENARIOS['gp-u20-chaos-ending']`, `MILESTONES['gp-u20-chaos-ending']`, `GP_U20_*`, `gp-u20-oracle`.

- [ ] **Step 1: Write the failing test.** `ending_menu(4)`, `lands_marked(0, 6)` (`0x86`), the mode-0x1F row `ad4=0,c0=6`, the `CHAOS's ending` label, the record's title CHAOS' REDEMPTION; in `STOP_AT_END`; add to the `test_gp_win` list/tuple. Run the tests; expected FAIL.
- [ ] **Step 2: Add the scenario/milestones.** Build the scenario with Task 3's `dict(time_limit=240, extra=WIN_EXTRA, memsize=64, steps=...)` shape: `ending_menu(4)`, `lands_marked(0, 6)`, `0x10789E=0x78`, the U10 steps, `start_char(6, 4)`, `_ending_tail(6, 'CHAOS')`; add the scenario to `STOP_AT_END`. Run the tests; expected PASS.
- [ ] **Step 3: Capture.** `make gp-capture scenario=gp-u20-chaos-ending`; all checks, stopped-at-end, and `check-input` clean.
- [ ] **Step 4: Measure, pin, wire.** As Task 3 Steps 5–6 with `GP_U20_*`/`gp-u20-oracle`; add to `verify`.
- [ ] **Step 5: Confirm and commit.** `gp_win.py check` reaches `ad4=0,c0=6` and the `CHAOS's ending` label; the record's title CHAOS' REDEMPTION; capture identity pinned; update the record/PROGRESS; commit `gameplay: capture CHAOS' ending`.

---

### Task 6: ARMADON's ending (character 4, cell 5)

**Files:**
- Modify: `tools/gp_session.py`, `tools/gp_win.py`, `tools/tests/test_gp_session.py`, `tools/tests/test_gp_win.py`, `Makefile`, `docs/PROGRESS.md`, the record
- Create (by capture): `data/k11-captures/gp-u21-armadon-ending/`

**Interfaces:**
- Consumes: the Task 3 helpers; produces `SCENARIOS['gp-u21-armadon-ending']`, `MILESTONES['gp-u21-armadon-ending']`, `GP_U21_*`, `gp-u21-oracle`.

- [ ] **Step 1: Write the failing test.** `ending_menu(5)`, `lands_marked(0, 4)` (`0x84`), the mode-0x1F row `ad4=0,c0=4`, the `ARMADON's ending` label, the record's title THE MEDITATION OF ARMADON; in `STOP_AT_END`; add to the `test_gp_win` list/tuple. Run the tests; expected FAIL.
- [ ] **Step 2: Add the scenario/milestones.** Build the scenario with Task 3's `dict(time_limit=240, extra=WIN_EXTRA, memsize=64, steps=...)` shape: `ending_menu(5)`, `lands_marked(0, 4)`, `0x10789E=0x78`, the U10 steps, `start_char(4, 5)`, `_ending_tail(4, 'ARMADON')`; add the scenario to `STOP_AT_END`. Run the tests; expected PASS.
- [ ] **Step 3: Capture.** `make gp-capture scenario=gp-u21-armadon-ending`; all checks, stopped-at-end, and `check-input` clean.
- [ ] **Step 4: Measure, pin, wire.** As Task 3 Steps 5–6 with `GP_U21_*`/`gp-u21-oracle`; add to `verify`.
- [ ] **Step 5: Confirm and commit.** `gp_win.py check` reaches `ad4=0,c0=4` and the `ARMADON's ending` label; the record's title THE MEDITATION OF ARMADON; capture identity pinned; update the record/PROGRESS; commit `gameplay: capture ARMADON's ending`.

---

### Task 7: DIABLO's ending (character 5, cell 6)

**Files:**
- Modify: `tools/gp_session.py`, `tools/gp_win.py`, `tools/tests/test_gp_session.py`, `tools/tests/test_gp_win.py`, `Makefile`, `docs/PROGRESS.md`, the record
- Create (by capture): `data/k11-captures/gp-u22-diablo-ending/`

**Interfaces:**
- Consumes: the Task 3 helpers; produces `SCENARIOS['gp-u22-diablo-ending']`, `MILESTONES['gp-u22-diablo-ending']`, `GP_U22_*`, `gp-u22-oracle`.

- [ ] **Step 1: Write the failing test.** `ending_menu(6)`, `lands_marked(0, 5)` (`0x85`), the mode-0x1F row `ad4=0,c0=5`, the `DIABLO's ending` label, the record's title DIABLO'S HELL; in `STOP_AT_END`; add to the `test_gp_win` list/tuple. Run the tests; expected FAIL.
- [ ] **Step 2: Add the scenario/milestones.** Build the scenario with Task 3's `dict(time_limit=240, extra=WIN_EXTRA, memsize=64, steps=...)` shape: `ending_menu(6)`, `lands_marked(0, 5)`, `0x10789E=0x78`, the U10 steps, `start_char(5, 6)`, `_ending_tail(5, 'DIABLO')`; add the scenario to `STOP_AT_END`. Run the tests; expected PASS.
- [ ] **Step 3: Capture.** `make gp-capture scenario=gp-u22-diablo-ending`; all checks, stopped-at-end, and `check-input` clean.
- [ ] **Step 4: Measure, pin, wire.** As Task 3 Steps 5–6 with `GP_U22_*`/`gp-u22-oracle`; add to `verify`.
- [ ] **Step 5: Confirm and commit.** `gp_win.py check` reaches `ad4=0,c0=5` and the `DIABLO's ending` label; the record's title DIABLO'S HELL; capture identity pinned; update the record/PROGRESS; commit `gameplay: capture DIABLO's ending`.

---

### Task 8: U14 fought live win retake (conditional)

**Status: skipped (declined)** — Step 1's "if declined" branch is the outcome
(record §E8); Steps 2–5 were not executed.

**Files:**
- Modify: `tools/gp_session.py` (`U14_ATTEMPTS`, the `gp-u14-win-live` steps, `time_limit`)
- Modify: `Makefile` (`GP_U14_*` pins; the target already exists)
- Modify: `tools/tests/test_gp_session.py`, `tools/tests/test_gp_win.py`
- Modify: `docs/PROGRESS.md`, the record
- Create (by capture, only if authorized): `data/k11-captures/gp-u14-win-live/`

**Interfaces:**
- Consumes: `U14_PUNCH`, `U14_KICK`, `WIN_MENU`, `_START`, `_MATCH1`, `LIVE_WIN`, `live_death` (already anchors on `(0x09,1)`, rejects any `DEATH_DONE` write and requires `b0c=1` after the KO).
- Produces: the revised `gp-u14-win-live` scenario and the `GP_U14_*` pins, or a recorded named gap. No pokes.

- [x] **Step 1: Ask for the new go-ahead.** The phase record says the two permitted attempts are spent. Ask the user explicitly whether a third attempt is authorized. If declined, write the named-gap paragraph in the record (R1 remains; no capture, all `GP_U14_*` empty) and mark this task skipped; do not run the capture. **Declined: done** — the go-ahead was not given, so the named-gap paragraph is record §E8 and no capture was run.

- [ ] **Step 2: Write the failing test (if authorized).** N/A (declined). In `test_gp_session.py`, assert `U14_ATTEMPTS` is 160, the scenario has a `p1.right` re-approach every 8 attempts, `time_limit=260`, and no `poke` step; in `test_gp_win.py`, assert `gp-u14-win-live` is in `LIVE_WIN` and `live_death` returns 1 on a `DEATH_DONE` W or no `b0c=1` after the KO, and 0 on `b0c=1` with no poke. Run the tests; expected FAIL for the attempt/re-approach assertions; the `live_death` test may already pass.

- [ ] **Step 3: Revise the scenario (if authorized).** N/A (declined). Change `U14_ATTEMPTS` and add the re-approach (the `_u16_round` pattern with `1A/1B`); keep the `WIN_MENU` path and the no-poke rule; `time_limit=260`. Run the tests; expected PASS.

- [ ] **Step 4: Capture and pin (if authorized).** N/A (declined). `make gp-capture scenario=gp-u14-win-live`; all checks, stopped-at-end, and `check-input` clean. Measure with `GP_WIN_KEEP=1 make gp-u14-oracle`; `gp_win.live_death` must pass (game-set `b0c=1`, no `DEATH_DONE`); fill the `GP_U14_*` pins and prove each one step above; adjust the Makefile comment provenance.

- [ ] **Step 5: Confirm and commit.** N/A (declined; the named-gap-only commit is `docs: record the U14 R1 named gap`). If captured: the fought KO by hits, the game-set `b0c`, the land, the end; update the record/PROGRESS; commit `gameplay: capture the fought live win`. If declined/failed: commit the named-gap record only.

---

### Task 9: U13 tail and 5120 wording

**Files:**
- Modify: `tools/gp_session.py` (`port_script` opt-in flag; the `port-script` CLI)
- Modify: `Makefile` (`gp-u13-oracle`'s `GP_SCRIPT_ARGS`; `GP_U13_*` pins)
- Modify: `tools/gp_compare.py` (`BANDS_ALLOWED` sentence; any new tail allowance only if measured)
- Modify: `tools/tests/test_gp_session.py`, `tools/tests/test_gp_compare.py`
- Modify: `docs/PROGRESS.md`, the record

**Interfaces:**
- Consumes: `port_script(name, lines, end=None)`, the `X` record, the `S` records, `gp_compare.py`, `test_gp_compare.py`'s `BandAllowance` fixtures, `GP_SCRIPT_ARGS`.
- Produces: an `--end-to-last-s` opt-in that lets the script end at the last `S`; the U13 tail replay and measured pins (or a named divergence); the corrected 5120 comment/test.

- [ ] **Step 1: Write the failing tests.** In `test_gp_session.py`, add a synthetic log whose last `S` is after its `X` (e.g. X f=0x130, S f=0x160); assert `port_script` with `end_to_last_s=True` accepts `--end 0x160`, the default still rejects it, and a synthetic scenario whose `X` is its last `S` produces a byte-identical script with and without the flag. The real U13 values are X f=0x271D=10013 and last S f=0x2754=10068; the poll.log `f` values are hex, `--end` and the script are decimal. In `test_gp_compare.py`, add a synthetic `BandAllowance` fixture carrying the 5120 shape: 13 pixels in rows 1..4, x 21..298, with the 8/1/4 split (8 at port index `0x32`, 1 at `0x2E`, 4 at `0x4D`); assert the frame is explained and a mutation of one bound byte makes it unexplained. Do not read the git-ignored capture in the unit test; a real-capture test may skip without it. Run the tests; expected FAIL.

- [ ] **Step 2: Add the flag.** In `tools/gp_session.py`, add `end_to_last_s=False` to `port_script`; when true, set `last = max(xrec['f'], max(r['f'] for r in recs if r['kind'] == 'S'))` before the `end` clamp; the default path is unchanged. Add `--end-to-last-s` to the `port-script` CLI (`gp_session.main`, whose choices are `port-script`/`trace-diff`), not a `replay`/`script` subcommand. Add `GP_SCRIPT_ARGS=--end-to-last-s` to the `gp-u13-oracle`'s `gp-replay` invocation; the `gp-replay` target already appends `$(GP_SCRIPT_ARGS)` to the `port-script` call. No new make variable is needed.

- [ ] **Step 3: Replay and measure the tail.** `GP_WIN_KEEP=1 make gp-u13-oracle` with the flag. The port script's `end` is 10068; the dump grows past 4939. Run the uncapped `gp_compare.py --report`. If the tail matches: set `GP_U13_MIN_FIRST=5904`, `GP_U13_MIN_REACH` to the new last exhibited port frame (about 5000), and measure `TRACE_MIN_FIRST`/`WIN_MIN_FIRST` (about 10069); prove each pin one step above. If the tail diverges: keep `MIN_FIRST=5891` and record the first unexplained frame and the port-side cause as a named divergence; do not move the pins. The 4248/4320/5120 allowances and the other U13 pins stay unchanged.

- [ ] **Step 4: Correct the 5120 comment and test.** Keep the existing 8/1/4 table (8 at port index `0x32`, 1 at `0x2E`, 4 at `0x4D`) and the capture-pinned bounds. Replace the sentence "the port's content at the 13 pixels is corroborated on both sides" with "the neighbours 5119/5122 corroborate the port's rendered colours at its own indices; they do not reproduce the capture's 13 pixels." Add the synthetic test from Step 1; a real-capture test, if added, skips without the capture and dump. No code/cause claim.

- [ ] **Step 5: Run and commit.** Run the two test modules and the `gp-u13-oracle`; update the record/PROGRESS; commit `gameplay: claim the U13 tail and correct the 5120 wording`.

---

### Task 10: The tick-vector documentation

**Files:**
- Modify: `port/RE_GUIDE.md` (item 4 and the "Remaining work" list)
- Modify: `port/spec/game_flow.md` (the tick section)
- Optional: a static raw-byte test in `tools/tests/` if the implementer chooses (no port counterpart)

**Interfaces:**
- Consumes: the raw bytes at `0x66519..0x6652C`, `0x6627E`, `0x663A4`/`0x663AC`/`0x663D4`, `0x1CFFF`, `0x2D62C`, `0x1BDF4`, `0x1BE28`.
- Produces: the documentation pin; no port code.

- [ ] **Step 1: Re-read the bytes.** Confirm the AIL timer install (`0x1CFED`/`0x1CFF2`/`0x1CFFF`/`0x1D008`), the INT 8 set-vector bytes (`0x66519 b8 08 00 00 00`, `0x6651E ba 7e 62 05 00`, `0x66523 66 8c cb`, `0x66526 b4 25`, `0x66528 1e`, `0x66529 66 8e db`, `0x6652C cd 21`), the `0x0005627E` immediate → linear `0x6627E` (CS base 0x10000), the handler `0x6627E`, the PIT routine `0x663A4` with the mode command `b0 36 e6 43` at `0x663AC..0x663AF`, the divisor formula `0x663D4` (`param*10000/0x20BC`), 19,887 for the 16,666 µs AIL period (`0x1CFFF` sets 0x3C) and 59.998 Hz, and `0x2D62C`'s counter role (`0x1BDF4` at `0x1BE28`). Record the addresses in §E6.

- [ ] **Step 2: Update the docs.** In `port/RE_GUIDE.md`, mark item 4 done with the INT 8/handler/PIT addresses and the `0x2D62C` distinction; correct the "Remaining work" tick bullet. In `port/spec/game_flow.md`, replace the "vector unread" wording with the measured install and the port's `game_isr_ticks`/`config.c` model. No fitted rate: 59.998 Hz is from 16,666·10,000/8,380.

- [ ] **Step 3: Optional static test.** If added, assert the raw bytes of `0x66519..0x6652C` (including `ba 7e 62 05 00`), the `0x663AC` mode command, and the `0x663D4` formula/divisor 19,887; skip without the game data. Run it; expected PASS.

- [ ] **Step 4: Commit.** Commit (when authorized): `docs: pin the INT 8 tick vector`.

---

### Task 11: The phase gate and docs

**Files:**
- Modify: `Makefile` (`verify`; the new oracle targets are already added by Tasks 2–9)
- Modify: `docs/PROGRESS.md`, `README.md`, `AGENTS.md`, `port/RE_GUIDE.md`
- Modify: `docs/superpowers/plans/2026-10-23-endgame-truth-derivations.md`, the spec's status

**Interfaces:**
- Consumes: every task's pins/targets/docs.
- Produces: the phase tip with `PR_DIFF_FULLSCAN=1 make verify` green, the docs/counters current, and the final review.

- [ ] **Step 1: Wire the new targets into `verify`.** Add `gp-u18-oracle`…`gp-u23-oracle` after `gp-u17-oracle` (the U16 target `gp-u23-oracle` is added by Task 2; the endings by Tasks 3–7). Each target skips only without its capture. The `test_gp_session`/`test_gp_compare` tests are in the `verify` unit list (`Makefile:1614`); the `test_gp_win`/`test_gp_twop` are run by the oracle targets. Do not claim the unit list runs `test_gp_win`/`test_gp_twop`.

- [ ] **Step 2: Update the docs and counters.** `docs/PROGRESS.md`: the phase paragraph, the new captures/pins, the named gaps (U14 if conditional, the 5120 cause, the differential frontier). `README.md`: the oracle list and the current state; the title percentage stays 66% unless a port fix adds an `FN_`. `AGENTS.md`: the new oracles, the five endings/U16 status, the U13 tail, the tick item done, the named gaps, and that the game-set `b0c` is U10's (`GP_ENDING_WIN_MIN_FIRST=9954`). `port/RE_GUIDE.md`: the same. The spec status: implemented/gated. The record: the final counter and the named limits.

- [ ] **Step 3: Run the full gate.** From the repo root:
  ```bash
  PR_DIFF_FULLSCAN=1 make verify
  ```
  Expect exit 0 (~35–70 min with the new oracles). Check the output line by line: each new oracle runs (not skips), each existing oracle pin is unchanged, `diff-verify` reads `1031/1031`, `14983/14983`, 0 named gaps, `813/844`, `806/844`, 8 unrowed, 31 open; `entry-triage` passes; `symbols.h` is unchanged; `port_progress.py` reads `799 1203 66` / `759 759 100`.

- [ ] **Step 4: The final whole-branch review.** Run the SDD review: the whole-branch review, one fix wave if needed, the scoped re-review. Park only non-load-bearing minors with rulings. Do not merge or push without the user's instruction.

- [ ] **Step 5: Commit the phase docs.** Commit (when authorized): `docs: close the endgame truth phase`.

---

## Self-Review

**Spec coverage.** G1 → Task 2; G2 → Tasks 3–7; G3 → Task 8; G4 → Task 9; G5 → Task 10; G6 → Task 11. The spec's §7 measurements → Task 1; the acceptance/§9 → Tasks 2–11; the named gaps/§10 → Task 11's docs. The withdrawn b0c task has no task, matching the spec's §7.3/NG4.

**Type consistency.** The scenario names are exactly `gp-u18-talon-ending`, `gp-u19-vertigo-ending`, `gp-u20-chaos-ending`, `gp-u21-armadon-ending`, `gp-u22-diablo-ending`, `gp-u23-u16-finisher`; the pins are `GP_U18_*`..`GP_U23_*`; the helpers are `ending_menu(cell)`/`start_char(char, cell)`; the `_ending_tail` second argument is the character name; the U16 check is the existing `gp_twop.py check --finisher`; the U14 rule is the existing `live_death`/`LIVE_WIN` with `(0x09,1)`. The new scenarios are in `STOP_AT_END`; the `test_gp_win` sorted list and 30-row tuple include the five endings; no `gp-u24`/b0c scenario exists.

**Proportion.** The five ending tasks share the Task 3 helpers and capture/oracle recipe; the plan does not repeat the bodies. The exact values live in the spec/record; the plan carries the decisions and the measurement gates, not the program.

**Open risk.** The captures are the schedule. Task 2's live KO and Task 8's retake may fail; the plan's fallbacks (the labeled poked damage, the named gap) keep the phase honest. The 5120 cause stays a named gap; the U13 tail may reveal a real divergence, which Task 9 records rather than fits.
