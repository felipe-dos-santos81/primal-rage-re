# Gameplay Truth II — Implementation Plan

Status: implemented; evidence closeout complete (2026-10-22): four captures are taken and pinned (U15, U12, U13, U16); U14 live win is the R1 named gap (§J6). The U16 finisher (R2) and U13's pixel claim are superseded by the endgame truth phase (`gp-u23-oracle` §E2; `gp-u18-oracle`…`gp-u22-oracle` §E3–§E7; U13 `GP_U13_MIN_FIRST=5904`/`MIN_REACH=4952`, §E9). No existing pin moved.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development
> (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps
> use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Resolve the U10 tail from capture 4113 onward, land the five held replay captures
(ENDURANCE, ending2, win-live, join, twop2) with measured pins, and record the joystick
verdict from the raw — with no byte-exact oracle line or existing pin moved without
recorded provenance.

**Architecture:** Tools-first. Task 1 gives `gp_compare` an uncapped report so Task 2 can
classify the whole U10 tail; Task 3 resolves the first unexplained run (fix, poke artifact,
or named allowance). Tasks 4–8 add the five scenario definitions and their empty, skipping
Makefile blocks without any capture. Tasks 9–12 are session-gated: each capture is taken
only after the user opens a DOSBox-X session, then its pins are measured. Task 13 records
the joystick verdict from the raw. Task 14 runs the full gate and closes the docs.

**Tech Stack:** Python 3 stdlib `unittest` tools (`tools/gp_compare.py`,
`tools/gp_session.py`, `tools/gp_capture.py`, `tools/gp_win.py`, `tools/gp_twop.py`), C11
port, GNU make, DOSBox-X, SDL3.

**Spec:** `docs/superpowers/specs/2026-10-20-gameplay-truth-ii-design.md` — read it first;
the plan argues from it.

## Global Constraints

- The byte-exact oracle lines never move. Every new pin is measured from its capture, with
  provenance in the Makefile comment and the derivation record, and fails at pin + 1.
- Every new assertion is mutation-proven able to fail; every new test binding stays inside
  the 64 MB `mem[]`.
- One capture at a time, each preceded by its short design and the user's capture go-ahead
  (the track-J Part II rule). Nobody types into the DOSBox-X window; `check-input` must be
  clean.
- `data/` is git-ignored and read-only except the harness's own capture directory;
  `data/game/C` must be proven untouched by the capture's staging check.
- The game dir's `CMOS` stays absent or 2040 zero bytes; `make verify` checks it.
- A fresh worktree needs `port/tests/ghidra_data.bin` and
  `port/tests/title_screen_ref.ppm` copied in before `make verify`.
- Commit style `<area>: <what changed>`; stage named files; never `git add -A`.
- Each task runs its owning tool tests and target; the controller runs the full
  `make verify` (~26 minutes) at the close.
- Only Tasks 1–3 can touch `port/src`; Tasks 4–13 touch tools, tests and the Makefile;
  Tasks 9–12 are the session-gated captures.

## Review Focus

1. **A capture with unscripted input or a truncated script.** `check-input` must fail and
   the capture must not be pinned. The capture tasks run it and stop on a failure; the
   existing `tools/tests/test_gp_capture.py` `test_unscripted_input` covers the detector.
2. **A tail pass that stops at the report cap and calls the tail classified.** Task 1's
   lifted-cap test and Task 2's `--max-unexplained 0` run are the proof; the record must
   list every run, not just five.
3. **A capture whose replay ends before the capture (N at the script's end).** The pin's
   provenance must say "how far the port got, not a divergence"; the task's record step
   writes that sentence, and `gp_compare`'s existing "improved: raise N" test covers the
   ratchet direction.
4. **The win-live death animation stopping at the mode-8 scan-out.** Task 11 must measure
   a `SCAN3_ALLOWED` entry and compare the frames, or narrow the claim to the `0x37FF6`
   state and the trace. The `Scan3Allowance` mutation test in
   `tools/tests/test_gp_compare.py` is the pattern.
5. **The ending2 cursor selecting the wrong character.** Task 5 must pin the ending row
   from `DS_0010782A[DS_00104AD4 * 0x94]` (`c0 = 1`, `ad4 = 0`), not assume it; the new
   `tools/tests/test_gp_win.py` row is the test.

---

## Task 1: An uncapped report for the U10 tail

**Files:**
- Modify: `tools/gp_compare.py:469-470` (`frame_claim` signature), `:557-560` (the report
  cap), `:763-782` (`main`'s arguments), `:809-812` (the call)
- Modify: `tools/tests/test_gp_compare.py:43-49` (`run_claim` helper) and add two tests
  (`TestFrames`, `TestCli`)
- Modify: `Makefile:1173-1175` (`gp-report`) and add `GP_REPORT_MAX ?= 5` beside `GP_ARGS`

**Interfaces:**
- Consumes: nothing new.
- Produces: `frame_claim(..., max_unexplained=REPORT_MAX)`; a report-mode run stops after
  `max_unexplained` unexplained frames, and `0` means no cap. `gp_compare.main` gains
  `--max-unexplained N` (default 5, `0` = unlimited). `make gp-report` passes
  `-max-unexplained "$(GP_REPORT_MAX)"` (default 5; `max=0` lifts it).

- [ ] **Step 1: Write the failing test for the parameter**

In `tools/tests/test_gp_compare.py`, extend the helper signature and pass the value:

```python
def run_claim(self, n, report=False, max_start=100, max_unexplained=gc.REPORT_MAX):
    ...
    rc, first, _, _ = gc.frame_claim('t', cap, tc.raw_map(self.cap), port, rows, n, report,
                                     out.append, max_start,
                                     max_unexplained=max_unexplained)
    return rc, first, out
```

Add to `TestFrames`:

```python
def test_the_report_cap_can_be_lifted(self):
    bad = [bytes([9]) * tc.FRAME_BYTES] * 7
    self.write([1, 2], [rgb(1), rgb(2)] + bad)
    for cap, want in ((gc.REPORT_MAX, gc.REPORT_MAX), (0, 7)):
        rc, first, out = self.run_claim(0, report=True, max_unexplained=cap)
        self.assertEqual(sum('UNEXPLAINED capture' in l for l in out), want, out)
    rc, first, out = self.run_claim(0, report=False)      # enforced mode still stops at 1
    self.assertEqual(sum('UNEXPLAINED capture' in l for l in out), 1, out)
```

Add to `TestCli`:

```python
def test_the_report_max_option(self):
    self.dump()                                   # clean frames 0-1 and the logs
    bad = [bytes([9]) * tc.FRAME_BYTES] * 7
    self.write([1, 2], [rgb(1), rgb(2)] + bad)    # frames 0-1 clean, 2-8 unexplained
    rc, out = self.cli('--report', '--scenario', 'gp-x', '--capture', self.cap,
                       '--port', self.port)
    self.assertEqual(rc, 0)
    self.assertEqual(out.count('UNEXPLAINED capture'), gc.REPORT_MAX, out)
    rc, out = self.cli('--report', '--scenario', 'gp-x', '--capture', self.cap,
                       '--port', self.port, '--max-unexplained', '0')
    self.assertEqual(rc, 0)
    self.assertEqual(out.count('UNEXPLAINED capture'), 7, out)
```

- [ ] **Step 2: Run the tests to verify they fail**

Run: `python3 -m unittest tools.tests.test_gp_compare.TestFrames.test_the_report_cap_can_be_lifted tools.tests.test_gp_compare.TestCli.test_the_report_max_option -v`
Expected: FAIL — `frame_claim() got an unexpected keyword argument 'max_unexplained'` and
`gp_compare: unrecognized arguments: --max-unexplained`.

- [ ] **Step 3: Implement the parameter and the option**

In `tools/gp_compare.py`, change the signature to add
`max_unexplained=REPORT_MAX` after `overlay=None`. Change the cap at `:559` to:

```python
if not report or (max_unexplained and len(unexpl) >= max_unexplained):
    break
```

Add to `main()` after `--report`:

```python
ap.add_argument('--max-unexplained', type=int, default=REPORT_MAX,
                help='report mode: stop after this many unexplained frames (0 = no cap; default 5)')
```

Pass `a.max_unexplained` to `frame_claim` as its last keyword argument.

In `Makefile`, add beside `GP_ARGS`:

```make
GP_REPORT_MAX ?= 5
```

and extend the `gp-report` comparison line with `--max-unexplained "$(GP_REPORT_MAX)"`.

- [ ] **Step 4: Run the tests to verify they pass**

Run: `python3 -m unittest tools.tests.test_gp_compare -v`
Expected: PASS, including the two new tests.

- [ ] **Step 5: Commit**

```bash
git add tools/gp_compare.py tools/tests/test_gp_compare.py Makefile
git commit -m "tools: gameplay truth II an uncapped report for the U10 tail"
```

---

## Task 2: Classify the whole U10 tail

**Files:**
- Create: `docs/superpowers/plans/2026-10-20-gameplay-truth-ii-derivations.md` (§W1)
- Reads: `data/k11-captures/gp-u10-ending`, `/tmp/pr_gp_dump/gp-u10-ending`

**Interfaces:**
- Consumes: Task 1's `--max-unexplained 0`.
- Produces: §W1's measured list of every unexplained run from 4113 to 5703, each with its
  nearest port frame, rows, x range and pixel count. Task 3 reads it.

- [ ] **Step 1: Build the replay and the script**

Run: `make gp-replay scenario=gp-u10-ending GP_OPTIONAL=1`
Expected: the driver runs, `PR_GP_DUMP` writes `/tmp/pr_gp_dump/gp-u10-ending`
(`frames.txt`, `trace.txt`, `frame_*.ipx`).

- [ ] **Step 2: Run the uncapped report**

Run:

```bash
python3 tools/gp_compare.py --report --scenario gp-u10-ending \
  --capture data/k11-captures/gp-u10-ending --port /tmp/pr_gp_dump/gp-u10-ending \
  --max-unexplained 0
```

Expected: the classification counts, the window start, and one
`UNEXPLAINED capture ...` line per run. The first must still be
`capture 4113 (raw 7858): nearest port 3444 ... (9732 px)`; the lines after it are new
evidence.

- [ ] **Step 3: Write §W1 in the record**

Create the record with the header `# Gameplay Truth II — derivation record` and a §W1
section containing: the command, the exact counts line, every `UNEXPLAINED` line verbatim,
and one sentence per run naming the class it looks like (A, B or C from the spec) and what
evidence the next task needs. Do not decide the class yet; mark each as "candidate".

- [ ] **Step 4: Commit**

```bash
git add docs/superpowers/plans/2026-10-20-gameplay-truth-ii-derivations.md
git commit -m "docs: gameplay truth II the U10 tail classification"
```

---

## Task 3: Resolve the first unexplained run

**Files:** depend on the branch chosen in Step 2:
- A (fix): the owning `port/src/**` and `port/tests/test_*.c`; `tools/diff_verify.py` if
  the changed function is rowed.
- B (poke artifact): `docs/superpowers/plans/2026-10-20-gameplay-truth-ii-derivations.md`
  and `AGENTS.md` (the named gap).
- C (allowance): `tools/gp_compare.py` (`SCAN3_ALLOWED`/`OVERLAY_ALLOWED`),
  `tools/tests/test_gp_compare.py`, `Makefile` (the U10 pin).

**Interfaces:**
- Consumes: §W1's first run.
- Produces: one of three outcomes, recorded in §W1 with its proving address, capture bytes
  or pin, and the U10 oracle's new (or unchanged) verdict.

- [ ] **Step 1: Re-read the first run against the raw and the capture**

Read the §W1 lines for the first run. Compare the capture frame's rows with the nearest
port frame's (the `diff_box` already names the rows and x range). For a death-animation
stream, read `0x37EA0`, the `0x37DD4`/`0x29C78` streams and the mode-`0xD`/`0xF` difference
(the U9/U10 record §W.16). Write the candidate class (A, B or C) and its evidence into §W1.

- [ ] **Step 2: Apply the class**

**A — a real port divergence.**
1. Add the failing assertion to the owning area file (one of `port/tests/test_fight.c`,
   `test_game.c`, `test_platform.c`, `test_video.c`, `test_audio.c`) that reproduces the
   wrong value from the bytes.
2. Run it; expected FAIL.
3. Fix the owning function in `port/src/**`.
4. Run the owning area and the U10 oracle; expected PASS and a re-measured `GP_ENDING_MIN_FIRST`
   (record the old and new value).
5. If the function has a differential row, add the case/mutant that only the old code
   fails; run `make diff-verify`.

**B — a poke artifact.**
1. Quote the capture's `W` record(s) and the S record showing the poked state, and the
   raw instruction the un-poked path would take instead.
2. Add the named gap to §W1 and the `AGENTS.md` named-gaps paragraph, with the win-live
   capture (Task 11) named as the shared-machinery test.
3. Run the U10 oracle; expected PASS with the pin unchanged.

**C — a capture/scan-out artifact.**
1. Add the measured entry to `SCAN3_ALLOWED['gp-u10-ending']` (or `OVERLAY_ALLOWED`, if it
   is a glyph overlay) with the same tuple shape the existing entries use: the frame, the
   bound port indices, the named bytes and their sha256.
2. Add the mutation test to the matching class in `tools/tests/test_gp_compare.py`: the
   named bytes explain the frame, and mutating one bound frame or the named bytes
   un-explains it.
3. Re-measure `GP_ENDING_MIN_FIRST`; raise it to the new first unexplained, and record the
   old and new value. Run `make gp-ending-oracle`; expected PASS.

- [ ] **Step 3: Run the owning tests and the U10 oracle**

Run: `python3 -m unittest tools.tests.test_gp_compare -v` (C) or the owning
`port/tests` area via `./build/run_tests` (A), then `make gp-ending-oracle`.
Expected: PASS; the record's §W1 has the class, the evidence and the pin consequence.

- [ ] **Step 4: Commit**

```bash
git add <the files the branch touched> docs/superpowers/plans/2026-10-20-gameplay-truth-ii-derivations.md
git commit -m "port: gameplay truth II resolve the U10 tail"   # or tools:/docs:
```

---

## Task 4: `gp-u12-endurance` — the scenario

**Files:**
- Modify: `tools/gp_session.py` (add the scenario, after `gp-u8-endurance`)
- Modify: `tools/tests/test_gp_session.py` (one shape test)
- Modify: `Makefile` (the `GP_U12_*` block and `gp-u12-oracle`, added to `verify`)

**Interfaces:**
- Consumes: `_u8_menu` (`tools/gp_session.py:303-308`) and the `0x44798` team-pass picks.
- Produces: `SCENARIOS['gp-u12-endurance']`, a `dict` with `time_limit`, `steps` and (if the
  picks need it) `extra`; `make gp-u12-oracle` skips without
  `data/k11-captures/gp-u12-endurance`.

- [ ] **Step 1: Write the failing test**

In `tools/tests/test_gp_session.py`, add:

```python
def test_the_endurance_fight_scenario_reaches_the_team_pass(self):
    scn = gs.SCENARIOS['gp-u12-endurance']
    self.assertGreater(scn['time_limit'], 46)
    self.assertTrue(any(s[0] == 'until_mode' and s[1] in (0x10, 0x30) for s in scn['steps']))
    self.assertTrue(any(s[0] in ('after', 'after_mode', 'after_entry') for s in scn['steps']))
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `python3 -m unittest tools.tests.test_gp_session -v`
Expected: FAIL — `KeyError: 'gp-u12-endurance'`.

- [ ] **Step 3: Add the scenario and the Makefile block**

In `tools/gp_session.py`, define `SCENARIOS['gp-u12-endurance']` by extending
`gp-u8-endurance`'s session: keep its boot/menu path and row-5 down presses, then add the
`0x44798` team-pass picks and an `until_mode` for `0x30` (or `0x06` if the fight enters the
ordinary round mode) with a time limit long enough for the fight. Use the existing
`('after', n, ('pad', (...), hold))` and `('until_mode', mode, n)` forms; the exact frames
and picks are measured from a preview run and written as a short comment above the
definition (the raw's `0x44798` picks, record §U8.9).

In `Makefile`, add an empty `GP_U12_*` block beside `GP_U8_*` (sha256, frames, MIN_FIRST,
MAX_START, MIN_REACH) and a `gp-u12-oracle` target that calls `gp-replay` with
`GP_OPTIONAL=1` and `gp_compare` with the pins; add it to `verify` after the U8 oracles.
The empty pins make the target skip without the capture.

- [ ] **Step 4: Run the test and the target**

Run: `python3 -m unittest tools.tests.test_gp_session -v` and `make gp-u12-oracle`.
Expected: PASS; the oracle prints its skip line because the capture is absent.

- [ ] **Step 5: Commit**

```bash
git add tools/gp_session.py tools/tests/test_gp_session.py Makefile
git commit -m "tools: gameplay truth II the ENDURANCE fight scenario"
```

---

## Task 5: `gp-u13-ending2` — a non-character-0 ending

**Files:**
- Modify: `tools/gp_session.py` (clone `gp-u10-ending`)
- Modify: `tools/gp_win.py` (`MILESTONES['gp-u13-ending2']`)
- Modify: `tools/tests/test_gp_win.py` and `tools/tests/test_gp_session.py`
- Modify: `Makefile` (the `GP_U13_*` block and `gp-u13-oracle`, added to `verify`)

**Interfaces:**
- Consumes: `WIN_MENU` (`tools/gp_session.py:361-366`), `WIN_EXTRA`, `KO_P2`, `DEATH_DONE`,
  `lands_marked`; `_START`/`_MATCH1` (`tools/gp_win.py:49-61`).
- Produces: `SCENARIOS['gp-u13-ending2']` with a cursor move before the confirm;
  `MILESTONES['gp-u13-ending2']`, whose ending row is
  `_all(ad4=0, c0=1)` and whose round-1 start row is `_all(c0=1, b1e=1)`.

- [ ] **Step 1: Write the failing tests**

In `tools/tests/test_gp_win.py`, add:

```python
def test_the_second_ending_pins_character_one(self):
    rows = gp_win.MILESTONES['gp-u13-ending2']
    ending = [r for r in rows if r[1] == 0x1F and r[2] == 1]
    self.assertTrue(ending)
    self.assertEqual(ending[0][3](dict(ad4=0, c0=1)), True)
    self.assertEqual(ending[0][3](dict(ad4=0, c0=0)), False)
    start = [r for r in rows if r[1] == 0x06 and r[2] == 1]
    self.assertTrue(start)
    self.assertEqual(start[0][3](dict(c0=1, b1e=1)), True)
    self.assertEqual(start[0][3](dict(c0=0, b1e=1)), False)
```

In `tools/tests/test_gp_session.py`, add a test that the scenario's steps contain a
`p1.right` pad press before the `p1.start` confirm.

- [ ] **Step 2: Run the tests to verify they fail**

Run: `python3 -m unittest tools.tests.test_gp_win tools.tests.test_gp_session -v`
Expected: FAIL — `KeyError: 'gp-u13-ending2'`.

- [ ] **Step 3: Add the scenario and the milestone row**

In `tools/gp_session.py`, clone `gp-u10-ending`'s steps with **three** `p1.right`
presses before the `p1.start` confirm. The cell table is
`0xC8882 = 00 02 03 01 06 04 05` (U5 record §C5.4), so cell 1 is class 2 (TALON) and
class 1 (BLIZZARD) is cell 3. Raw correction (Task 5 report): the plan's original
one-right recipe selected TALON. The first land poke is
`lands_marked(0, 1)` (`0x81` x 7), not u10's `0x80`: `0x41C28` state 1 tallies a
mark only when `(mark & 0x7F) == DS_0010782A[side] | side << 6`
(`0x41D39`/`0x41D5C`). The rest of the pokes and `WIN_EXTRA` are unchanged.

In `tools/gp_win.py`, add a local start tuple and use it:

```python
_START1 = (
    ('character select (mode 0x10)', 0x10, 1, None),
    ('round 1: P1 is character 1 (cursor 3 confirmed, 0x43CAD)', 0x06, 1, _all(c0=1, b1e=1)),
)
MILESTONES['gp-u13-ending2'] = _START1 + (
    ('round 1 KO with the seven lands poked P1\'s', 0x08, 1,
     lambda r: r['s1_5a'] == 0x78 and r['w2'] == 1 and r['w3'] == 0 and r['ad4'] == NO_RESULT
     and lands(r) == [0x80 | r['c0']] * 7),
) + _MATCH1[1:] + <copy gp-u10-ending's rows from the '0x41C28 state 3' row on,
    replacing the ending row's predicate with _all(ad4=0, c0=1) and its label with
    'BLIZZARD's ending'>
```

Do not mutate the shared `_START`; the `c0 = 1` row belongs to this scenario only. The
ending row's predicate is `_all(ad4=0, c0=1)`, and its label names BLIZZARD.

Add the empty `GP_U13_*` block and `gp-u13-oracle` target to the Makefile, in `verify`.

- [ ] **Step 4: Run the tests and the target**

Run: `python3 -m unittest tools.tests.test_gp_win tools.tests.test_gp_session -v` and
`make gp-u13-oracle`.
Expected: PASS; the oracle skips without the capture.

- [ ] **Step 5: Commit**

```bash
git add tools/gp_session.py tools/gp_win.py tools/tests/test_gp_win.py tools/tests/test_gp_session.py Makefile
git commit -m "tools: gameplay truth II the second ending scenario"
```

---

## Task 6: `gp-u14-win-live` — a won match without pokes

**Files:**
- Modify: `tools/gp_session.py` (a scenario with no `poke` actions)
- Modify: `tools/gp_win.py` (`MILESTONES['gp-u14-win-live']`)
- Modify: `tools/tests/test_gp_win.py` and `tools/tests/test_gp_session.py`
- Modify: `Makefile` (the `GP_U14_*` block and `gp-u14-oracle`, added to `verify`)

**Interfaces:**
- Consumes: the `gp-idle-loss` menu path, `WIN_EXTRA`, and `tools/gp_moves.py`'s step
  generator (`steps --char 0 --facing 0 ...`) for the hit landings.
- Produces: `SCENARIOS['gp-u14-win-live']`, whose steps contain no `('poke', ...)`;
  `MILESTONES['gp-u14-win-live'] = _START + _MATCH1`, without the seven-lands row.

- [ ] **Step 1: Write the failing tests**

In `tools/tests/test_gp_session.py`:

```python
def test_the_live_win_has_no_pokes(self):
    scn = gs.SCENARIOS['gp-u14-win-live']
    self.assertFalse(any(s[-1][0] == 'poke' for s in scn['steps'] if isinstance(s[-1], tuple)))
```

In `tools/tests/test_gp_win.py`:

```python
def test_the_live_win_uses_the_unpoked_match_rows(self):
    rows = gp_win.MILESTONES['gp-u14-win-live']
    self.assertEqual([r[:3] for r in rows], [r[:3] for r in gp_win._START + gp_win._MATCH1])
```

- [ ] **Step 2: Run the tests to verify they fail**

Run: `python3 -m unittest tools.tests.test_gp_session tools.tests.test_gp_win -v`
Expected: FAIL — `KeyError: 'gp-u14-win-live'`.

- [ ] **Step 3: Add the scenario and the milestones**

In `tools/gp_session.py`, clone the `gp-idle-loss` menu path, confirm character 0, then
script the hit landings that KO P2 in rounds 1 and 2. Derive the presses with
`python3 tools/gp_moves.py steps --char 0 --facing 0 --moves <the raw's entries>` and
paste the resulting `('after', n, ('pad', (...), hold))` rows. Use a longer `time_limit`
(the capture must cover both rounds and the match win) and `extra=WIN_EXTRA`. Include an
`('until_mode', 0x12, 0)` (the conquered-lands screen) or an end after the match result,
so the capture stops after the win path.

In `tools/gp_win.py`, add:

```python
MILESTONES['gp-u14-win-live'] = _START + _MATCH1
```

Add the empty `GP_U14_*` block and `gp-u14-oracle` target to the Makefile, in `verify`.

- [ ] **Step 4: Run the tests and the target**

Run: `python3 -m unittest tools.tests.test_gp_session tools.tests.test_gp_win -v` and
`make gp-u14-oracle`.
Expected: PASS; the oracle skips without the capture.

- [ ] **Step 5: Commit**

```bash
git add tools/gp_session.py tools/gp_win.py tools/tests/test_gp_win.py tools/tests/test_gp_session.py Makefile
git commit -m "tools: gameplay truth II the live-win scenario"
```

---

## Task 7: `gp-u15-join` — mid-match join

**Files:**
- Modify: `tools/gp_session.py` (a short scenario)
- Modify: `tools/tests/test_gp_session.py`
- Modify: `Makefile` (the `GP_U15_*` block and `gp-u15-oracle`, added to `verify`)

**Interfaces:**
- Consumes: the `gp-idle-loss` menu path, then `('after_mode', 0x06, 60, ('pad',
  ('p2.start',), 5))` for F2.
- Produces: `SCENARIOS['gp-u15-join']`; `gp_twop.two_human` on the capture must report a
  `join` with `b1f == 3` and no credit debit.

- [ ] **Step 1: Write the failing test**

In `tools/tests/test_gp_session.py`:

```python
def test_the_join_scenario_presses_f2_in_the_fight(self):
    scn = gs.SCENARIOS['gp-u15-join']
    self.assertTrue(any(s[0] == 'after_mode' and s[1] == 0x06 and 'p2.start' in s[3][1]
                        for s in scn['steps']))
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `python3 -m unittest tools.tests.test_gp_session -v`
Expected: FAIL — `KeyError: 'gp-u15-join'`.

- [ ] **Step 3: Add the scenario and the Makefile block**

Clone the `gp-idle-loss` steps to mode `0x06`, then add the F2 press and an end 60 frames
later. Do not confirm P2 in the character select; the join is mid-round. Add a short
comment naming `0x28CC8` and `0x2CA93` (the free-credit join). Add the empty `GP_U15_*`
block and `gp-u15-oracle` target; the target runs `python3 tools/gp_twop.py check
--capture data/k11-captures/gp-u15-join` before `gp_compare`.

- [ ] **Step 4: Run the test and the target**

Run: `python3 -m unittest tools.tests.test_gp_session -v` and `make gp-u15-oracle`.
Expected: PASS; the oracle skips without the capture.

- [ ] **Step 5: Commit**

```bash
git add tools/gp_session.py tools/tests/test_gp_session.py Makefile
git commit -m "tools: gameplay truth II the mid-match join scenario"
```

---

## Task 8: `gp-u16-twop2` — extended two-player + finisher

**Files:**
- Modify: `tools/gp_session.py` (clone `gp-twop`, extend it)
- Modify: `tools/tests/test_gp_session.py`
- Modify: `Makefile` (the `GP_U16_*` block and `gp-u16-oracle`, added to `verify`)

**Interfaces:**
- Consumes: `gp-twop`'s start and both-sides steps; `tools/gp_moves.py` for a finisher
  sequence (reactions `0x32`..`0x34` on a dying opponent, `0x37640`/`0x37774`/`0x37898`).
- Produces: `SCENARIOS['gp-u16-twop2']` with a longer time limit and a finisher press
  sequence; `gp_twop.two_human` must still pass.

- [ ] **Step 1: Write the failing test**

In `tools/tests/test_gp_session.py`:

```python
def test_the_extended_twop_scenario_runs_past_the_first_match(self):
    scn = gs.SCENARIOS['gp-u16-twop2']
    self.assertGreater(scn['time_limit'], gs.SCENARIOS['gp-twop']['time_limit'])
    self.assertTrue(any(s[0] == 'until_mode' and s[1] in (0x09, 0x12) for s in scn['steps']))
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `python3 -m unittest tools.tests.test_gp_session -v`
Expected: FAIL — `KeyError: 'gp-u16-twop2'`.

- [ ] **Step 3: Add the scenario and the Makefile block**

Clone `gp-twop`'s steps, keep the two-human join and both confirms, then extend the fight
with a finisher sequence derived from the raw (the `gp_moves.py` steps, a dying P2, and the
finisher's reaction). End after the match result or the conquered-lands screen. Add a
comment naming the finisher entries and the raw reaction table. Add the empty `GP_U16_*`
block and `gp-u16-oracle` target; the target runs `gp_twop.py check` then `gp_compare`.

- [ ] **Step 4: Run the test and the target**

Run: `python3 -m unittest tools.tests.test_gp_session -v` and `make gp-u16-oracle`.
Expected: PASS; the oracle skips without the capture.

- [ ] **Step 5: Commit**

```bash
git add tools/gp_session.py tools/tests/test_gp_session.py Makefile
git commit -m "tools: gameplay truth II the extended two-player scenario"
```

---

## Task 9: Capture `gp-u12-endurance`

**Files:**
- Modify: `Makefile` (`GP_U12_*` values)
- Modify: `docs/superpowers/plans/2026-10-20-gameplay-truth-ii-derivations.md` (§J4)
- Modify: `docs/PROGRESS.md`
- Capture: `data/k11-captures/gp-u12-endurance` (git-ignored)

**Interfaces:**
- Consumes: Task 4's scenario and empty block.
- Produces: a measured `GP_U12_*` block and a passing `make gp-u12-oracle`.

- [ ] **Step 1: STOP — ask the user for a capture session**

Show the user the scenario's steps and the time limit. Do not run `gp-capture` until the
user confirms a DOSBox-X session. Nobody types into the window while it runs.

- [ ] **Step 2: Capture and check the input**

Run: `make gp-capture scenario=gp-u12-endurance`
then `python3 tools/gp_capture.py check-input data/k11-captures/gp-u12-endurance`.
Expected: the capture's checks are all ok and `check-input` exits 0; a `FAIL unscripted
input` stops the task (do not pin the capture; re-take it with the user).

- [ ] **Step 3: Replay and measure**

Run: `make gp-replay scenario=gp-u12-endurance` and
`python3 tools/gp_compare.py --report --scenario gp-u12-endurance --capture
data/k11-captures/gp-u12-endurance --port /tmp/pr_gp_dump/gp-u12-endurance
--max-unexplained 0`.
Record the first unexplained, the trace/moves first difference, the window start, the reach,
the capture's `poll.log` sha256 and the frame count.

- [ ] **Step 4: Pin and prove the pin**

Fill `GP_U12_MIN_FIRST`, `GP_U12_TRACE_MIN_FIRST`, `GP_U12_MAX_START`,
`GP_U12_MIN_REACH`, `GP_U12_CAPTURE_SHA256` and `GP_U12_CAPTURE_FRAMES` with the measured
values and their provenance comments (including "N is the script's end, not a divergence"
if that is what it is). Run `make gp-u12-oracle`; expected PASS. Raise one pin by 1; expected
FAIL; restore it.

- [ ] **Step 5: Record and commit**

Write §J4 with the capture's checks, the measured lines, the end-vs-divergence sentence and
the pin proofs. Add the PROGRESS paragraph. Commit:

```bash
git add Makefile docs/superpowers/plans/2026-10-20-gameplay-truth-ii-derivations.md docs/PROGRESS.md
git commit -m "tools: gameplay truth II the ENDURANCE fight capture"
```

---

## Task 10: Capture `gp-u13-ending2`

**Files:**
- Modify: `Makefile` (`GP_U13_*` values)
- Modify: `docs/superpowers/plans/2026-10-20-gameplay-truth-ii-derivations.md` (§J5)
- Modify: `docs/PROGRESS.md`
- Capture: `data/k11-captures/gp-u13-ending2` (git-ignored)

**Interfaces:**
- Consumes: Task 5's scenario and milestone row.
- Produces: a measured `GP_U13_*` block, a passing `gp_win.py check` with the ending row
  `c0 = 1`, and a passing `make gp-u13-oracle`.

- [ ] **Step 1: STOP — ask the user for a capture session**

Show the user the character-select move and the ending path. Wait for the go-ahead.

- [ ] **Step 2: Capture and check the input**

Run: `make gp-capture scenario=gp-u13-ending2`, then
`python3 tools/gp_capture.py check-input data/k11-captures/gp-u13-ending2`.
Expected: all checks ok; stop on `FAIL unscripted input`.

- [ ] **Step 3: Replay, check the evidence, measure**

Run: `make gp-replay scenario=gp-u13-ending2`,
`python3 tools/gp_win.py check --scenario gp-u13-ending2 --capture
data/k11-captures/gp-u13-ending2`, and the uncapped `gp_compare --report`.
Expected: the evidence check reports the ending row with `c0 = 1` (BLIZZARD) and `ad4 = 0`;
record the milestone count, the first unexplained, the trace/win first difference, the
window start and the reach.

- [ ] **Step 4: Pin and prove the pin**

Fill `GP_U13_*` (sha256, frames, MIN_FIRST, TRACE_MIN_FIRST, MAX_START, MIN_REACH,
MILESTONES, WIN_MIN_FIRST) and run `make gp-u13-oracle`; expected PASS. Raise one pin by 1;
expected FAIL; restore it.

- [ ] **Step 5: Record and commit**

Write §J5 (the ending's evidence, the character index, the pin proofs) and the PROGRESS
paragraph. Commit:

```bash
git add Makefile docs/superpowers/plans/2026-10-20-gameplay-truth-ii-derivations.md docs/PROGRESS.md
git commit -m "tools: gameplay truth II the second ending capture"
```

---

## Task 11: Capture `gp-u14-win-live`

**Files:**
- Modify: `Makefile` (`GP_U14_*` values)
- Modify: `docs/superpowers/plans/2026-10-20-gameplay-truth-ii-derivations.md` (§J6)
- Modify: `docs/PROGRESS.md`
- Capture: `data/k11-captures/gp-u14-win-live` (git-ignored)

**Interfaces:**
- Consumes: Task 6's scenario and milestones; Task 3's W1 class.
- Produces: a measured `GP_U14_*` block, a passing live-win evidence check, and the
  shared-machinery death-animation evidence for W1.

- [ ] **Step 1: STOP — ask the user for a capture session**

Show the user the hit-landing script and the expected two-round win. Wait for the go-ahead.
This is the longest capture; use `memsize=64` only if the original's OOM is observed.

- [ ] **Step 2: Capture and check the input**

Run: `make gp-capture scenario=gp-u14-win-live`, then
`python3 tools/gp_capture.py check-input data/k11-captures/gp-u14-win-live`.
Expected: all checks ok; stop on `FAIL unscripted input`. If the CPU's determinism makes the
frame-keyed script miss, record the attempt and stop — do not fabricate a pin.

- [ ] **Step 3: Replay and measure**

Run: `make gp-replay scenario=gp-u14-win-live`,
`python3 tools/gp_win.py check --scenario gp-u14-win-live --capture
data/k11-captures/gp-u14-win-live`, and the uncapped `gp_compare --report`.
Expected: the milestones reproduce; the game itself sets the death-done byte (`b0c`) after
the fought KO. Record the first unexplained, the trace/win first difference, the window
start, the reach, the sha256 and the frame count.

- [ ] **Step 4: Pin and prove the pin**

Fill `GP_U14_*` and run `make gp-u14-oracle`; expected PASS. Raise one pin by 1; expected
FAIL; restore it. If the death-animation frames stop at a mode-8 scan-out, add the measured
`SCAN3_ALLOWED['gp-u14-win-live']` entry and its mutation test, or narrow the claim to
`0x37FF6` and the trace — say which in the record.

- [ ] **Step 5: Record and commit**

Write §J6 (the live KO, the death-animation evidence, the shared-stream limit from §W1, the
pin proofs) and the PROGRESS paragraph. Commit:

```bash
git add Makefile tools/gp_compare.py tools/tests/test_gp_compare.py docs/superpowers/plans/2026-10-20-gameplay-truth-ii-derivations.md docs/PROGRESS.md
git commit -m "tools: gameplay truth II the live-win capture"
```

---

## Task 12: Capture `gp-u15-join` and `gp-u16-twop2`

**Files:**
- Modify: `Makefile` (`GP_U15_*`, `GP_U16_*` values)
- Modify: `docs/superpowers/plans/2026-10-20-gameplay-truth-ii-derivations.md` (§J7, §J8)
- Modify: `docs/PROGRESS.md`
- Captures: `data/k11-captures/gp-u15-join`, `data/k11-captures/gp-u16-twop2` (git-ignored)

**Interfaces:**
- Consumes: Tasks 7 and 8's scenarios and empty blocks.
- Produces: measured `GP_U15_*`/`GP_U16_*` blocks; `gp_twop.py check` passes on both
  (the join's `b1f == 3` and no credit debit; twop2's two-human counts).

- [ ] **Step 1: STOP — ask the user for the two capture sessions**

Show the two scripts and their time limits. Take them one at a time; wait for the go-ahead
for each.

- [ ] **Step 2: Capture and check the input**

For each scenario, run `make gp-capture scenario=...` and
`python3 tools/gp_capture.py check-input data/k11-captures/<scenario>`.
Expected: all checks ok; stop on `FAIL unscripted input`.

- [ ] **Step 3: Replay, check, measure**

For each, run `make gp-replay scenario=...`,
`python3 tools/gp_twop.py check --capture data/k11-captures/<scenario>`, and the uncapped
`gp_compare --report`. Expected: the join check reports a mid-round join with no debit;
twop2's two-human counts pass; record the first unexplained, trace/moves first difference,
window start, reach, sha256 and frames.

- [ ] **Step 4: Pin and prove the pins**

Fill `GP_U15_*` and `GP_U16_*`; run `make gp-u15-oracle` and `make gp-u16-oracle`; expected
PASS. Raise one pin of each by 1; expected FAIL; restore.

- [ ] **Step 5: Record and commit**

Write §J7 and §J8 with the checks, measurements and pin proofs, and the PROGRESS paragraph.
Commit:

```bash
git add Makefile docs/superpowers/plans/2026-10-20-gameplay-truth-ii-derivations.md docs/PROGRESS.md
git commit -m "tools: gameplay truth II the join and extended two-player captures"
```

---

## Task 13: The joystick verdict

**Files:**
- Modify: `AGENTS.md` (the input/oracle paragraph)
- Modify: `tools/port_classification.txt` only if a new evidence line is needed
- Modify: `docs/superpowers/plans/2026-10-20-gameplay-truth-ii-derivations.md` (§J9)

**Interfaces:**
- Consumes: `0x2D2F0`'s differential row, `0x4F644`'s gate at `0x4F68B`, `0x5004A`'s
  host-owned classification line.
- Produces: a recorded verdict: the original has no gameplay joystick path; an SDL-gamepad
  map is additive; a report-only Alt-J/boot probe is optional and not a `gp-*` fixture.

- [ ] **Step 1: Write the verdict**

Add an `AGENTS.md` paragraph (or extend the U11 input paragraph) with the addresses and the
sentence "no gameplay joystick path: `0x2D2F0` is `xor eax,eax; ret`; `0x4F644` merges the
joystick words only when bit 1 of its result is set, which it never is; `0x5004A` is
host-owned. An SDL-gamepad map is additive, not faithful." Write §J9 in the record with the
same evidence.

- [ ] **Step 2: Verify no code or pin moved**

Run: `make diff-verify` (the two rows are already VERIFIED) and
`python3 -m unittest tools.tests.test_gp_compare tools.tests.test_gp_session`.
Expected: PASS; no pin or oracle line changes.

- [ ] **Step 3: Commit**

```bash
git add AGENTS.md docs/superpowers/plans/2026-10-20-gameplay-truth-ii-derivations.md
git commit -m "docs: gameplay truth II the joystick verdict"
```

---

## Task 14: Closeout

**Files:**
- Modify: `docs/PROGRESS.md`, `AGENTS.md`, `README.md` (only if the ported-function counter
  moved), `docs/superpowers/plans/2026-10-20-gameplay-truth-ii-derivations.md`

**Interfaces:**
- Consumes: every task's pins and record sections.
- Produces: a green `make verify`, the truthful docs, and the phase's final counts.

- [ ] **Step 1: Re-measure every pin**

For each new scenario, run its oracle and its `--report` pass; confirm the pin is the first
unexplained/differing value and that pin + 1 fails. Record any re-measurement in the record.

- [ ] **Step 2: Run the full gate**

Run: `make verify` (about 26 minutes).
Expected: exit 0; every existing oracle line unchanged; the new oracles pass or skip (a
declined capture is named in the record).

- [ ] **Step 3: Update the docs**

Append the phase paragraph to `docs/PROGRESS.md` with the new pins, the U10 tail outcome
and the joystick verdict. Update the `AGENTS.md` Commands list with the new oracle targets.
Update the README title only if `python3 tools/port_progress.py` changed (expected: no new
ported function).

- [ ] **Step 4: Commit**

```bash
git add docs/PROGRESS.md AGENTS.md README.md docs/superpowers/plans/2026-10-20-gameplay-truth-ii-derivations.md docs/superpowers/specs/2026-10-20-gameplay-truth-ii-design.md
git commit -m "docs: gameplay truth II closeout"
```

---

## Execution Notes

- Tasks 1–8 and 13 can run without a capture session. Tasks 9–12 stop at their first step
  until the user opens a DOSBox-X session; that is the only intended pause.
- If a capture cannot be made, record it as a named gap with the user's decision; the phase
  can close with fewer than five captures (design acceptance #2).
- Never write to `data/game/C`. The capture's own staging check and `check-input` are the
  proof; a failure stops the task.
- If a task's measured pin differs from the design's expectation, the capture wins; record
  the correction and the evidence.
