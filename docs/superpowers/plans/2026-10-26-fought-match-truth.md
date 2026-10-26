# Fought-Match Truth Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Turn the last poked/partial fight claims into measured claims: one fought U14 win (single authorized run), one hand-dealt two-human KO with its derived evidence rule, and the original's deciding U16 state — or declare each a scoped named gap with its evidence.

**Architecture:** The phase is capture-first. A small harness change (two fighter-slot state fields and a stop-on-mode scheduler) lets the new captures stop on the event they are about, without leftover scripted input. Then: the U14 script is extended to the measured 160-attempt form and run once; a new two-human scenario lands a real KO and a new `gp_twop.py check --ko` predicate judges it; a diagnostic replay of the U23 scenario records the original's `slot+0x30`/`+0x42` at the divergence. A port fix is taken only where the raw bytes prove the cause; nothing is fitted.

**Tech Stack:** Python 3 stdlib (capture/predicate/compare tooling), C (port + tests), CMake, DOSBox-X capture harness, Ghidra-verified raw addresses.

**Spec:** `docs/superpowers/specs/2026-10-25-fought-match-truth-design.md`

## Global Constraints

- Work from the repo root. `data/game/` is read-only; captures write only `data/k11-captures/<scenario>/`; never type in the DOSBox-X window while a capture runs.
- `make title-pin` before any capture. Capture commands use a shell timeout of at least 600 s.
- One U14 attempt under this phase's go-ahead. A failed run is recorded and its pins stay empty; no second script tweak.
- No pokes in the new fight captures. U23's existing labelled `0x10789E=0x77` fallback is existing evidence, not a model.
- Every pin is measured from the tool's own output and proved to fail one step in the tested direction. Existing pins must not move except the phase's own.
- `SNAP_FIELDS` additions are appended, so older `poll.log` lines simply lack them. A gate-inventoried capture is added to `CAPTURE_DIRS` and the gate's literal count is bumped in all three places; a diagnostic capture is not gate-inventoried.
- New scenarios go in `STOP_AT_END`. A scenario a `PR_GP_DUMP` replay runs needs its `fn_resolve` miss set pinned in `port/tests/test_platform.c`.
- Any port change is raw-derived, one C function per original, with `/* 0xADDR — section */` and a `PORT:` note for a deliberate deviation. No fitted constants; on a plan-vs-raw conflict the raw wins and the correction is recorded.
- Commit style `<area>: <what>`; stage named files, never `git add -A`; commit only when authorized.
- The final gate is `PR_DIFF_FULLSCAN=1 GATE_STRICT=1 PR_PINS_EXACT=1 make verify`, with every capture present except a declared failed U14.

## Review Focus

1. **A capture that passes the harness checks but is not a KO.** `check-input` only proves the scripted input; it cannot see that the round timed out. Task 3's predicate and Task 4's evidence check must reject a mode-7 round even when every S record is clean.
2. **U14 loses the CPU fight.** The measured run had P1 KO'd in both rounds. Task 2 must record that outcome and leave the pins empty; it must not pin a loss or call it a win.
3. **The U16 diagnostic shows the port's y is right.** The y/anchor hypothesis is unproven. Task 5 must record the refutation and stop; it must not adjust the gate or the y to make the reaction appear.
4. **Appending fields breaks old captures or the port trace.** Old `poll.log` lines lack the new fields, and the port trace must carry them for the comparison. Task 1 tests the parser against a literal old line and the new `format_s` widths.
5. **A new capture is not inventoried or its miss set is not pinned.** Task 4 must bump all three gate literals and add the `k_gp_sets` entry; Task 5 must pin the diagnostic scenario's miss set even though the capture is not gate-inventoried.

---

### Task 1: Fighter-slot state fields and stop-on-mode

**Files:**
- Modify: `tools/gp_session.py` (`SNAP_FIELDS`, `Schedule`)
- Modify: `tools/gp_capture.py` (`Poller`, `run`)
- Modify: `port/tests/test_game.c` (`gp_trace_line`)
- Modify: `tools/tests/test_gp_session.py`
- Modify: `tools/tests/test_gp_capture.py`
- Create: `docs/superpowers/plans/2026-10-26-fought-match-truth-derivations.md` (§F1)

**Interfaces:**
- Produces: `SNAP_FIELDS` names `s0_30`, `s0_42`, `s1_30`, `s1_42` (widths 4, 2, 4, 2); `Schedule(steps, stop_mode=None, stop_delay=0, skip_on_mode=None)`, where each mode argument is an int or a tuple of ints; scenario keys `stop_mode`, `stop_delay` (frames) and `skip_on_mode`.
- Consumes: the existing `SNAP_FIELDS`, `Schedule`, `Poller`, `format_s`, `parse`, `snapshots`.

- [ ] **Step 1: Write the failing field tests**

In `tools/tests/test_gp_session.py`, add:

```python
def test_the_state_fields_are_appended_with_their_addresses_and_widths(self):
    self.assertEqual(gs.SNAP_FIELDS[-4:],
                     (('s0_30', 0x1077E0, 4), ('s0_42', 0x1077F2, 2),
                      ('s1_30', 0x107874, 4), ('s1_42', 0x107886, 2)))
    vals = {n: 0 for n, _, _ in gs.SNAP_FIELDS}
    vals.update({'f': 0x1234, 's0_30': 0x202E, 's0_42': 0x0820,
                 's1_30': 0x13FF, 's1_42': 0x0008})
    line = gs.format_s(1, vals, 0, 0, 0)
    self.assertIn('s0_30=0000202E', line)
    self.assertIn('s0_42=0820', line)
    self.assertIn('s1_30=000013FF', line)
    self.assertIn('s1_42=0008', line)

def test_an_old_s_line_without_the_state_fields_still_parses(self):
    old = ('S ms=1 f=1234 mode=0006 st=0000 tick=00000000 t508=00000000 '
           't50c=00000000 raw=00000000 pad=00000000 new=00000000 held=00000000 '
           'e0=0000 e2=0000 rng=00000000 cred=00000000 fp=00 b1d=00 b1f=03 b25=00 '
           'w10d=00 cnt=00 s0_52=00 s0_54=00 s0_5a=00 s1_52=00 s1_54=00 s1_5a=00 '
           'ent=00000000 r0=00 r1=00 c0=00 c1=00 s0_43=00 dev1=0000 dev2=0000 '
           'kb=0000 head=001E tail=003E')
    r = gs.parse(old)
    self.assertEqual(r['kind'], 'S')
    self.assertNotIn('s0_30', r)

def test_stop_on_mode_arms_the_end_frame_and_counts_as_done(self):
    steps = (('after', 10, ('pad', ('p1.b0',), 4)),
             ('after', 10, ('pad', ('p1.b1',), 4)))
    s = gs.Schedule(steps, stop_mode=0x08, stop_delay=30)
    s.on_mode(100, 0x06)
    s.on_mode(200, 0x08)
    self.assertEqual(s.end_frame, 230)
    self.assertFalse(s.ended(229))
    self.assertTrue(s.ended(230))
    self.assertTrue(s.stopped_on_mode)
    self.assertTrue(s.steps_ok)                 # the unfinished steps are intentional

def test_skip_on_mode_drops_the_round_and_resumes_at_the_next_anchor(self):
    steps = (('after', 10, ('pad', ('p1.b0',), 4)),
             ('after', 10, ('pad', ('p1.b1',), 4)),
             ('after_entry', 0x06, 2, 10, ('pad', ('p1.b0',), 4)),
             ('after', 10, ('pad', ('p1.b1',), 4)))
    s = gs.Schedule(steps, skip_on_mode=0x08)
    s.on_mode(100, 0x06)
    s.on_mode(200, 0x08)
    self.assertEqual(s.i, 2)                    # the next after_entry stays armed
    self.assertEqual(s.skipped, 2)              # both remaining round steps dropped
    self.assertFalse(s.steps_ok)                # the anchor and its tail still to fire
```

The two existing append tests must move with the append: update
`test_the_u6_fields_are_appended` to `gs.SNAP_FIELDS[-11:-6]` and
`test_the_device_fields_are_appended_and_pointer_relative` to
`gs.SNAP_FIELDS[-6:-4]` (the four new fields follow `dev1`/`dev2`).

- [ ] **Step 2: Run the tests to verify they fail**

Run: `python3 -m unittest tools.tests.test_gp_session -k state_fields -k old_s_line -k stop_on_mode -k skip_on_mode -v`
Expected: FAIL — `s0_30` is absent / `Schedule` does not accept `stop_mode` or `skip_on_mode`.

- [ ] **Step 3: Append the fields**

In `tools/gp_session.py`, append after the `dev1`/`dev2` entries:

```python
# Plan 2026-10-26-fought-match-truth Task 1 (§F1): the fighter slots' latched
# y (+0x30, a dword), and the +0x42 word the 0x36F10 gate sets 0x820 in.
# Appended, so every older poll.log's S lines lack them.
('s0_30', 0x1077E0, 4), ('s0_42', 0x1077F2, 2),
('s1_30', 0x107874, 4), ('s1_42', 0x107886, 2),
```

The addresses are slot base `0x1077B0` + `side * 0x94` + the offset. The widths are the raw access widths (`DSD` for +0x30, the word `[slot+0x42]`).

- [ ] **Step 4: Add stop-on-mode, skip-on-mode and `steps_ok` to `Schedule`**

Change the constructor and `on_mode`:

```python
def __init__(self, steps, stop_mode=None, stop_delay=0, skip_on_mode=None):
    ...
    self.stop_modes = () if stop_mode is None else (
        tuple(stop_mode) if isinstance(stop_mode, (tuple, list)) else (stop_mode,))
    self.skip_modes = () if skip_on_mode is None else (
        tuple(skip_on_mode) if isinstance(skip_on_mode, (tuple, list)) else (skip_on_mode,))
    self.stop_delay = stop_delay
    self.skipped = 0
    self.stopped_on_mode = False

def on_mode(self, f, mode):
    if mode in self.skip_modes:
        j = next((k for k in range(self.i, len(self.steps))
                  if self.steps[k][0] == 'after_entry'), None)
        end = len(self.steps) if j is None else j
        for k in range(self.i, end):
            if self.steps[k][0] != 'until_mode' and self.steps[k][-1] != ('end',):
                self.skipped += 1
        self.i = end
    if mode in self.stop_modes and self.end_frame is None:
        self.stopped_on_mode = True
        self.end_frame = f + self.stop_delay
    ...
```

The int-or-tuple normalization lets U14 pass `(0x08, 0x09)` and the
two-human capture pass `(0x08, 0x09)` while a single-mode test passes `0x08`.

`steps_ok` is the harness's completion rule:

```python
@property
def steps_ok(self):
    """All scripted steps either fired or were deliberately skipped, or a
    stop_on_mode ended the capture. A skip_on_mode drops one round's queued
    steps and resumes at the next after_entry; a stop_on_mode ends the whole
    capture."""
    return self.fired + self.skipped == self.total or self.stopped_on_mode
```

`skip_on_mode` fires on every entry into that mode; after one skip `self.i` is
at the next anchor (or past the end), so a second call skips nothing. A
scenario with only one block and a `stop_on_mode` uses that instead.

- [ ] **Step 5: Pass the scenario's stop mode through `Poller` and accept `steps_ok`**

In `tools/gp_capture.py`:

- Add `stop_mode=None, stop_delay=0, skip_on_mode=None` to `Poller.__init__`,
  construct `gs.Schedule(steps, stop_mode, stop_delay, skip_on_mode)`.
- At the `Poller(...)` construction in `main` (around line 544), pass
  `stop_mode=scn.get('stop_mode')`, `stop_delay=scn.get('stop_delay', 0)`,
  `skip_on_mode=scn.get('skip_on_mode')`. `Poller.run` has no scenario
  argument; do not try to read `scn` there.
- In `run_checks` (line 234), replace the condition
  `sched.fired == sched.total` with `sched.steps_ok` and print
  `'steps fired %d/%d (%d skipped)'` % `(sched.fired, sched.total, sched.skipped)`.
- In `Poller.run`, wrap the `for step, act, late in fire(self.sched, f):`
  loop in `if self.sched.end_frame is None:`. A stop-on-mode capture must not
  inject the remaining queued input; a skip-on-mode capture has already
  advanced `i` past the dropped steps.

- [ ] **Step 6: Add the four fields to the port trace**

In `port/tests/test_game.c`, append to `gp_trace_line` after `dev2`:

```c
"s0_30=%08X s0_42=%04X s1_30=%08X s1_42=%04X "
```

and the arguments:

```c
(unsigned)DSD(DS_001077B0u + 0x30u),
(unsigned)DSW(DS_001077B0u + 0x42u),
(unsigned)DSD(DS_001077B0u + 0x94u + 0x30u),
(unsigned)DSW(DS_001077B0u + 0x94u + 0x42u),
```

Keep the line's existing fields and order; the new fields are appended.

- [ ] **Step 7: Add the poller construction test**

In `tools/tests/test_gp_capture.py`:

```python
def test_the_poller_passes_the_stop_modes_to_the_schedule(self):
    p = gc.Poller('/tmp/mem', '/tmp/log', (), None,
                  stop_mode=(0x08, 0x09), stop_delay=240, skip_on_mode=(0x08, 0x09))
    self.assertEqual((p.sched.stop_modes, p.sched.stop_delay, p.sched.skip_modes),
                     ((0x08, 0x09), 240, (0x08, 0x09)))

def test_steps_ok_accepts_a_skipped_round(self):
    s = gs.Schedule((('after', 1, ('pad', ('p1.b0',), 4)),), skip_on_mode=0x08)
    s.on_mode(10, 0x08)
    self.assertTrue(s.steps_ok)                 # the one step is deliberately skipped
```

- [ ] **Step 7: Update the stop-scenario gate test**

`tools/tests/test_gp_capture.py`'s `test_opt_in_scenarios` requires every
`STOP_AT_END ∩ SCENARIOS` scenario's last step to be `('end',)` or an
`until_mode`. The two new fight scenarios take their end from `stop_mode`, so
add the clause:

```python
self.assertTrue(last[-1] == ('end',) or last[0] == 'until_mode'
                or gs.SCENARIOS[name].get('stop_mode') is not None, name)
```

The U14 and two-human scenarios added in Tasks 2 and 4 are the positive cases;
the existing scenarios still pass.

- [ ] **Step 8: Run the tests to verify they pass**

Run: `python3 -m unittest tools.tests.test_gp_session tools.tests.test_gp_capture -v`
Expected: PASS, including the two updated append tests
(`[-11:-6]` for the u6 fields, `[-6:-4]` for the device fields) and the new
address/width assertions.

- [ ] **Step 9: Build and record §F1**

Run: `make build`
Expected: exit 0.

Write §F1 in `docs/superpowers/plans/2026-10-26-fought-match-truth-derivations.md`: the four addresses and widths, the old-line parse result, the stop-on-mode test output, and the `gp_trace_line` field order.

- [ ] **Step 10: Commit**

```bash
git add tools/gp_session.py tools/gp_capture.py tools/tests/test_gp_session.py \
        tools/tests/test_gp_capture.py port/tests/test_game.c \
        docs/superpowers/plans/2026-10-26-fought-match-truth-derivations.md
git commit -m "tools: capture fighter slot state and stop on a mode"
```

---

### Task 2: U14's measured script and the one authorized run

**Files:**
- Modify: `tools/gp_session.py` (`U14_*`, `_u16_round`, `SCENARIOS['gp-u14-win-live']`)
- Modify: `tools/tests/test_gp_session.py` (`TestLiveWin`)
- Modify: `Makefile` (the U14 comment and, only on success, `GP_U14_*`)
- Modify: `port/tests/test_platform.c` (`k_miss_gp_u14_win_live`, only on success)
- Modify: `docs/superpowers/plans/2026-10-26-fought-match-truth-derivations.md` (§F2)

**Interfaces:**
- Consumes: `_u16_round(attempts, first_gap, anchor=(mode, k))`, `stop_mode`/`skip_on_mode` from Task 1, `WIN_EXTRA`, `WIN_MENU`.
- Produces: `SCENARIOS['gp-u14-win-live']` with `time_limit=260`, `stop_mode=0x12`, `skip_on_mode=(0x08, 0x09)`, two `_u16_round(160, 10, anchor=...)` blocks; a throwaway `gp-harness-smoke` scenario; updated `TestLiveWin`; either the nine `GP_U14_*` pins (and an empty `GATE_NAMED_GAPS`) or a recorded failure.

- [ ] **Step 1: Update `TestLiveWin` first**

Replace the body's attempt-count assertions with:

```python
def test_the_live_win_has_no_pokes(self):
    scn = gs.SCENARIOS['gp-u14-win-live']
    self.assertFalse(any(s[-1][0] == 'poke' for s in scn['steps'] if isinstance(s[-1], tuple)))
    self.assertEqual(scn['extra'], gs.WIN_EXTRA)
    self.assertEqual(scn['time_limit'], 260)
    self.assertEqual((scn['stop_mode'], scn['stop_delay']), (0x12, 0))
    self.assertEqual(scn['skip_on_mode'], (0x08, 0x09))

def test_each_round_has_160_attempts_and_a_walk_every_eight(self):
    steps = gs.SCENARIOS['gp-u14-win-live']['steps']
    anchors = [st for st in steps if st[0] == 'after_entry']
    self.assertEqual([(a[1], a[2], a[3]) for a in anchors], [(0x06, 1, 10), (0x06, 2, 10)])
    for n, a in enumerate(anchors):
        i = steps.index(a)
        j = next((k for k in range(i + 1, len(steps))
                  if steps[k][0] == 'after_entry'), len(steps))
        block = steps[i + 1:j]
        attempts = [a] + [st for st in block
                          if st[-1][1] in (gs.U14_PUNCH, gs.U14_KICK)]
        walks = [st for st in block if st[-1][1] == ('p1.right',)]
        self.assertEqual(len(attempts), 160)
        self.assertEqual([st[-1][1] for st in attempts[:17]],
                         [gs.U14_PUNCH, gs.U14_KICK] * 8 + [gs.U14_PUNCH])
        self.assertTrue(all(st[0] == 'after' and st[-1][2] == 4 for st in block
                            if st[-1][1] in (gs.U14_PUNCH, gs.U14_KICK)))
        self.assertEqual(len(walks), 19)          # at k = 8, 16, ..., 152
        self.assertTrue(all(st[-1][2] == 12 for st in walks))   # the hold, not the gap
    self.assertIn('gp-u14-win-live', gs.STOP_AT_END)
```

The exact slice bounds may be adjusted to the builder's output; the assertions must prove 160 attempts per round and a walk every eight.

- [ ] **Step 2: Run the test to verify it fails**

Run: `python3 -m unittest tools.tests.test_gp_session.TestLiveWin -v`
Expected: FAIL — `time_limit` is 200 and the rounds are 32 attempts.

- [ ] **Step 3: Move the builder and rewrite the scenario**

Move `U16_GAP`, `U16_WALK`, `U16_GROUP` and `_u16_round` above `SCENARIOS['gp-u14-win-live']` (they already depend only on `U14_PUNCH`/`U14_KICK`, defined first). Replace the U14 constants and scenario with:

```python
U14_PUNCH = ('p1.b0', 'p1.b1')
U14_KICK = ('p1.b2', 'p1.b3')
U14_GAP = 16

SCENARIOS['gp-u14-win-live'] = dict(
    time_limit=260, extra=WIN_EXTRA,
    stop_mode=0x12, stop_delay=0, skip_on_mode=(0x08, 0x09),
    steps=WIN_MENU
    + _u16_round(160, 10, anchor=(0x06, 1))
    + _u16_round(160, 10, anchor=(0x06, 2)),
)
```

Remove `U14_ATTEMPTS`/`U14_PRESSES` if nothing else uses them. Keep the comment's measured hit-rate provenance and add the 160/8/260 source (record §E8/§J6 and this plan).

- [ ] **Step 4: Run the tests to verify they pass**

Run: `python3 -m unittest tools.tests.test_gp_session -v`
Expected: PASS.

- [ ] **Step 5: Update the Makefile provenance comment**

In the `gp-u14-oracle` block, replace the 32-attempt wording with the measured 160-attempt/16-gap/12-walk/260 s pattern and cite this plan. Add `GP_U14_MAX_MISSING =` to the pin block and `--max-missing "$(GP_U14_MAX_MISSING)"` to the `gp_compare` call: `coverage_check` fails closed when the pin is absent, which is invisible only while the target skips without the capture. Do not fill the pins yet.

- [ ] **Step 6: Add the throwaway harness smoke**

Add to `tools/gp_session.py`:

```python
SCENARIOS['gp-harness-smoke'] = dict(
    time_limit=30, stop_mode=0x27, stop_delay=10, steps=(
    ('boot', ENTER_WAIT, ('key', 'enter')),
    ('after_mode', 0x27, 30, ('pad', ('p1.start',), 4)),
))
```

This exercises the real stop-on-mode path in a capture: the mode 0x27 entry
arms the stop 10 frames later and the `after_mode` press must not fire. It does
not prove skip-on-mode or the 240-frame window (the unit tests do); it is a
DOSBox-X/title-pin/poller preflight. Add `'gp-harness-smoke'` to `STOP_AT_END`;
do not add it to `CAPTURE_DIRS` and do not put it in the audio traces. Add one
shape test to `tools/tests/test_gp_session.py` (the scenario exists, is in
`STOP_AT_END`, has no pokes, and its last step is a pad step).

- [ ] **Step 7: Build and prepare the capture**

Run: `make build`
Expected: exit 0.

Run: `make title-pin`
Expected: the pinned copy at `/tmp/pr_title_pin/PRAGE.EXE`.

Confirm `data/game/C/CMOS` is absent or 2040 zero bytes and
`data/k11-captures/gp-u14-win-live` is absent.

Run the smoke: `DISPLAY=:1 timeout 300 make gp-capture scenario=gp-harness-smoke`
Expected: every `check=` line passes. This is a harness validation, not the
U14 attempt; it does not run `gp-replay` and writes a throwaway capture
outside the gate inventory.

- [ ] **Step 8: Run the one authorized capture**

Run: `DISPLAY=:1 timeout 600 make gp-capture scenario=gp-u14-win-live`
Expected: every `check=` line passes, including
`steps fired ... (... skipped)`, and the capture publishes to
`data/k11-captures/gp-u14-win-live`.

- [ ] **Step 9: Judge the capture**

Run: `python3 tools/gp_win.py check --scenario gp-u14-win-live --capture data/k11-captures/gp-u14-win-live`
Expected on success: 6/6 milestones and `live_death`'s `b0c = 1` line, exit 0.

On failure: record the first failing check, the mode-8/9 rows (`s0_5a`, `s1_5a`, `w2`, `w3`, `ad4`), whether `b0c` became 1, and the preserved `.failed` directory. Leave all `GP_U14_*` pins empty and keep `GATE_NAMED_GAPS`; stop this task. Do not run a second capture.

- [ ] **Step 10: On success, measure, pin and clear the named gap**

Run each, recording the exact output in §F2:

```bash
make gp-replay scenario=gp-u14-win-live
```

The first replay is expected to fail the unpinned miss-set check (and only
that check); from its log add `k_miss_gp_u14_win_live` to
`port/tests/test_platform.c` and the `k_gp_sets` row
`{ "gp-u14-win-live", 0, k_miss_gp_u14_win_live, FNM_N(k_miss_gp_u14_win_live) }`
(expected the `gp-idle-loss` pair, including `0x29D60`; the measurement
decides) and re-run; expected exit 0.

```bash
python3 tools/gp_compare.py --report --scenario gp-u14-win-live \
  --capture data/k11-captures/gp-u14-win-live --port /tmp/pr_gp_dump/gp-u14-win-live \
  --max-unexplained 0
python3 tools/gp_win.py path --scenario gp-u14-win-live \
  --capture data/k11-captures/gp-u14-win-live --port /tmp/pr_gp_dump/gp-u14-win-live \
  --min-milestones 0 --win-min-first 0
sha256sum data/k11-captures/gp-u14-win-live/poll.log
ls data/k11-captures/gp-u14-win-live/frame_*.raw.gz | wc -l
```

Fill the nine `GP_U14_*` variables (including `MAX_MISSING` from the report's
missing count) from the tool output, not from arithmetic. Prove each pin fails
one step (N+1, start−1, reach−1, missing+1, one-byte sha change) and restore
it. Set `GATE_NAMED_GAPS` to empty (a declared absence that is present fails
gate-check). Run `make gp-u14-oracle`; expected exit 0 with the evidence, the
frame/trace/coverage ratchets and the win path.

- [ ] **Step 11: Record §F2 and commit**

§F2 records the script change, the smoke, the capture command, the `check=` lines, the pass/fail outcome, the measured pins (or the failure rows), and the one-step proofs.

```bash
git add tools/gp_session.py tools/tests/test_gp_session.py Makefile \
        port/tests/test_platform.c \
        docs/superpowers/plans/2026-10-26-fought-match-truth-derivations.md
git commit -m "gameplay: extend the fought-win capture"
```

If the capture failed, the commit is the script/test/comment change only; the pins stay empty and `GATE_NAMED_GAPS` keeps `gp-u14-win-live`.

---

### Task 3: `gp_twop.py check --ko`

**Files:**
- Modify: `tools/gp_twop.py`
- Modify: `tools/tests/test_gp_twop.py`
- Modify: `docs/superpowers/plans/2026-10-26-fought-match-truth-derivations.md` (§F3)

**Interfaces:**
- Produces: `ko(lines, winner=0) -> dict(ko=, round=, result=, death=, fail=[])`; CLI `check --ko [--winner N]`, mutually exclusive with `--join`/`--finisher`.
- Consumes: `gs.parse`, `gs.snapshots`, `two_human`, `WIN_FIELDS` names (`ad4`, `w2`, `w3`, `b0c`).

- [ ] **Step 1: Write the failing predicate tests**

In `tools/tests/test_gp_twop.py`, add cases built from literal S lines (the module's existing `_rec` style):

```python
def _ko_log(mode=0x08, ad4=0xFFFFFFFF, b0c=1, poke=None, jump=False, winner_ko=False):
    """A two-human log with a hand-dealt KO of side 1, built on _s()."""
    L = [_s(f, mode=0x10, b1f=1) for f in range(0x2F0, 0x300)]
    L += [_s(f, mode=0x10) for f in range(0x300, 0x310)]
    L += [_s(0x400, mode=0x06, b1f=3, new=0x01000100,
             s1_5a=0x00 if jump else 0x10)]
    if poke is not None:
        L.append('W ms=0 f=0408 step=1 addr=%08X len=1 was=10 now=78 late=0 race=0' % poke)
    if jump:
        L.append(_s(0x410, mode=0x06, b1f=3, new=0x01000100, s1_5a=0x78, w2=1))
    else:
        L.append(_s(0x410, mode=0x06, b1f=3, new=0x01000100, s1_5a=0x78, w2=1,
                    s0_5a=0x78 if winner_ko else 0))
    L += [_s(0x420, mode=mode, b1f=3, w2=1, ad4=ad4, b0c=b0c)]
    L += ['X ms=0 f=0430 step=1 end']
    return L

def test_a_hand_dealt_ko_passes_and_reports_death_done(self):
    r = gt.ko(_ko_log(), winner=0)
    self.assertEqual(r['fail'], [])
    self.assertEqual(r['ko'], 0x410)
    self.assertEqual(r['death'], 0x420)

def test_a_time_out_fails(self):
    lines = [_s(0x400, mode=0x06, b1f=3, new=0x01000100, s1_5a=0x10),
             _s(0x410, mode=0x07, b1f=3, s1_5a=0x10)]
    self.assertIn('time-out', ' '.join(gt.ko(lines)['fail']))

def test_a_poked_ko_fails(self):
    self.assertIn('poke', ' '.join(gt.ko(_ko_log(poke=0x10789E))['fail']))

def test_a_jump_from_zero_to_ko_fails(self):
    self.assertTrue(gt.ko(_ko_log(jump=True))['fail'])

def test_the_winner_must_not_be_ko_d(self):
    self.assertTrue(gt.ko(_ko_log(winner_ko=True), winner=0)['fail'])

def test_a_draw_or_wrong_side_result_fails(self):
    self.assertTrue(gt.ko(_ko_log(mode=0x09, ad4=2, b0c=0))['fail'])
```

The `_s` helper fills every `SNAP_FIELDS` name; extend it to take `s0_5a`/`s1_5a`/`w2`/`w3`/`ad4`/`b0c` (or add a `_win_s` helper using `gs.SNAP_FIELDS + gs.WIN_EXTRA`). The exact literal S line shape is the one in `test_gp_win.py`.

- [ ] **Step 2: Run the tests to verify they fail**

Run: `python3 -m unittest tools.tests.test_gp_twop -k ko -v`
Expected: FAIL — `gp_twop` has no `ko`.

- [ ] **Step 3: Implement `ko`**

Add to `tools/gp_twop.py`:

```python
KO_POKES = (0x10780A, 0x10789E, 0x104AF2, 0x104AF3, 0x104AD4,
            0x104B0C, 0x108106, 0x10810A, 0x10810C)

def ko(lines, winner=0):
    """The hand-dealt KO evidence (plan 2026-10-26-fought-match-truth §F3).
    Returns dict(ko, round, result, death, fail). All conditions are required:
    two_human passes; no W wrote a KO/death field; the loser's +0x5A reaches
    0x78 after a prior nonzero value in the same mode-6 entry; that entry has
    no mode-7 record; the winner's +0x5A stays below 0x78; the round result
    names the winner (mode 8, ad4=NO_RESULT = 0xFFFFFFFF) or the match result does (mode 9,
    ad4=winner); b0c is reported, not required."""
```

Implementation rules:
- Every S record must carry the `WIN_FIELDS`; otherwise fail "no WIN fields".
- Any `W` at `KO_POKES` fails (a poke replaced the game's own write).
- `human = two_human(lines)`; any fail is a fail.
- Find the first mode-6 entry whose S records include the loser's `s*_5a == 0x78`; require a strictly earlier `0 < s*_5a < 0x78` in that entry, no mode-7 record in it, and the winner's `< 0x78` at the KO.
- At the KO, `w` must name the winner (winner 0: `w2 >= 1`, `w3 == 0`; winner 1: `w3 >= 1`, `w2 == 0`), and `ad4` must be `NO_RESULT` (0xFFFFFFFF, mode 8) or `winner` (mode 9). A `2` draw fails.
- If mode 9 is present, its first record must have `ad4 == winner`, `w[winner] >= 1`, `w[loser] == 0`.
- `death` = the first S at/after the KO with `b0c == 1`; report it, do not require it.

- [ ] **Step 4: Wire the CLI**

Add `--ko` to the mutually exclusive check group with `--join`/`--finisher`; add `--winner` (default 0). In `main`, run `ko`, print the KO/round/result/death lines and every failure, exit 1 on any failure.

- [ ] **Step 5: Run the tests to verify they pass**

Run: `python3 -m unittest tools.tests.test_gp_twop -v`
Expected: PASS.

- [ ] **Step 6: Prove the predicate rejects the existing captures**

Run each; expected exit 1:

```bash
python3 tools/gp_twop.py check --ko --capture data/k11-captures/gp-u16-twop2   # no WIN fields
python3 tools/gp_twop.py check --ko --capture data/k11-captures/gp-twop        # no WIN fields
python3 tools/gp_twop.py check --ko --capture data/k11-captures/gp-u23-u16-finisher  # no WIN fields
python3 tools/gp_twop.py check --ko --capture data/k11-captures/gp-u9-win      # poked KO
```

`gp-twop`, `gp-u16-twop2` and `gp-u23` do not log `WIN_EXTRA`, so "no WIN
fields" is the honest first failure: without `s*_5a`/`ad4`/`w2`/`w3`/`b0c`
the capture cannot prove a KO. The time-out and poked-damage arms are
exercised by the synthetic unit cases; `gp-u9` has the fields and exercises
the poke arm on a real capture. Record the exact reason lines in §F3. A
predicate that passes any of these is wrong.

- [ ] **Step 7: Commit**

```bash
git add tools/gp_twop.py tools/tests/test_gp_twop.py \
        docs/superpowers/plans/2026-10-26-fought-match-truth-derivations.md
git commit -m "tools: check a hand-dealt two-human KO"
```

---

### Task 4: The two-human KO capture and oracle

**Files:**
- Modify: `tools/gp_session.py` (`SCENARIOS['gp-twop-ko']`, `STOP_AT_END`)
- Modify: `tools/tests/test_gp_session.py`
- Modify: `Makefile` (`CAPTURE_DIRS`, gate count, `GP_TWOP_KO_*`, `gp-twop-ko-oracle`, `verify`, `.PHONY`) — only after the capture passes
- Modify: `port/tests/test_platform.c` (`k_gp_sets`)
- Modify: `docs/superpowers/plans/2026-10-26-fought-match-truth-derivations.md` (§F4)

**Interfaces:**
- Consumes: `gp_twop.ko`, `_u16_round`, `stop_mode`, `WIN_EXTRA`, `steps_before_end(SCENARIOS['gp-twop']['steps'])`.
- Produces: `SCENARIOS['gp-twop-ko']`; a capture at `data/k11-captures/gp-twop-ko`; `gp-twop-ko-oracle` with measured pins; the `k_miss_gp_twop_ko` miss set.

- [ ] **Step 1: Write the failing scenario test**

In `tools/tests/test_gp_session.py`:

```python
def test_the_two_human_ko_scenario_is_one_round_with_the_walk_in_press(self):
    scn = gs.SCENARIOS['gp-twop-ko']
    self.assertIn('gp-twop-ko', gs.STOP_AT_END)
    self.assertEqual((scn['time_limit'], scn['extra'], scn['stop_mode'], scn['stop_delay']),
                     (240, gs.WIN_EXTRA, (0x08, 0x09), 240))
    self.assertFalse(any(s[-1][0] == 'poke' for s in scn['steps'] if isinstance(s[-1], tuple)))
    steps = scn['steps']
    self.assertEqual(steps[:9], gs.steps_before_end(gs.SCENARIOS['gp-twop']['steps'])[:9])
    block = steps[9:]
    attempts = [st for st in block if st[-1][1] in (gs.U14_PUNCH, gs.U14_KICK)]
    walks = [st for st in block if st[-1][1] == ('p1.right',)]
    self.assertEqual(len(attempts), 140)
    self.assertEqual(len(walks), 17)                 # at k = 8, 16, ..., 136
    self.assertEqual(block[0], ('after', 30, ('pad', gs.U14_PUNCH, 4)))
    self.assertTrue(all(st[0] == 'after' and st[-1][2] == 4 for st in attempts))
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `python3 -m unittest tools.tests.test_gp_session -k two_human_ko -v`
Expected: FAIL — no `gp-twop-ko`.

- [ ] **Step 3: Add the scenario**

```python
SCENARIOS['gp-twop-ko'] = dict(
    time_limit=240, extra=WIN_EXTRA, stop_mode=(0x08, 0x09), stop_delay=240,
    steps=steps_before_end(SCENARIOS['gp-twop']['steps'])[:9]
    + _u16_round(140, 30),
)
```

Add `'gp-twop-ko'` to `STOP_AT_END`. The comment records: 140 attempts
(17 walks of 12 frames every 8 attempts) is about 2,600 nominal frames, 1.6×
the measured ~88-attempt KO; the round timer is the backstop; the kept
walk-in step's `p2.left` is the second-side fight press the two-human rule
needs; the 240-frame result window is sized from U10's observed death stream
(not a two-human measurement), and `b0c` is reported if the capture shows it;
no pokes.

- [ ] **Step 4: Run the tests to verify they pass**

Run: `python3 -m unittest tools.tests.test_gp_session -k two_human_ko -v`
Expected: PASS.

- [ ] **Step 5: Capture**

Run: `make title-pin` (if not already), then
`DISPLAY=:1 timeout 600 make gp-capture scenario=gp-twop-ko`.
Expected: every `check=` line passes, including the result window, and the
capture publishes.

- [ ] **Step 6: Judge the capture**

Run: `python3 tools/gp_twop.py check --ko --capture data/k11-captures/gp-twop-ko`
Expected on success: a KO frame, the round result naming P1, and the
`b0c` report line.

On failure (mode 7 / no `0x78`): record the measured round rows, keep the
capture as `data/k11-captures/gp-twop-ko.failed` (or remove it), do not add
`gp-twop-ko` to `CAPTURE_DIRS`/`verify`, and stop the task. The scenario and
predicate code stay; the claim is not made.

- [ ] **Step 7: On success, replay and measure**

Run each, recording the exact output in §F4:

```bash
make gp-replay scenario=gp-twop-ko
```
The first replay is expected to fail the unpinned miss-set check (and only
that check); it writes the miss log and the dump. Step 8 pins the set and
re-runs.

```bash
python3 tools/gp_compare.py --report --scenario gp-twop-ko \
  --capture data/k11-captures/gp-twop-ko --port /tmp/pr_gp_dump/gp-twop-ko \
  --max-unexplained 0
sha256sum data/k11-captures/gp-twop-ko/poll.log
ls data/k11-captures/gp-twop-ko/frame_*.raw.gz | wc -l
```

The report prints the first unexplained frame, window start, reach, missing
count and moves first difference. Fill `GP_TWOP_KO_MIN_FIRST`,
`_TRACE_MIN_FIRST`, `_MAX_START`, `_MIN_REACH`, `_MAX_MISSING`,
`_MOVES_MIN_FIRST`, `_CAPTURE_SHA256`, `_CAPTURE_FRAMES` from that output, not
from arithmetic. Prove each fails one step and restore it.

- [ ] **Step 8: Pin the replay's miss set**

From the replay log, add `k_miss_gp_twop_ko` to `port/tests/test_platform.c`'s
scenario table with the measured set (expected `0x29D60`/`0x5D812` from
`frontend_mode_1b_step`, the shared §G.24 pair; the measurement decides). Run
`make gp-replay scenario=gp-twop-ko` again; it must pass with exactly that
set.

- [ ] **Step 9: Add the capture to the gate**

Add `$(K11_CAPTURES)/gp-twop-ko` to `CAPTURE_DIRS`; change the gate's literal
`35` to `36` in its help text, echo and `-ne` check. Add the empty
`GP_TWOP_KO_*` variable block, the `gp-twop-ko-oracle` target (evidence check,
replay, `gp_compare` with `--min-first/--trace-min-first/--max-start/--min-reach/
--max-missing` and the moves pin), add it to `.PHONY` and to the `verify`
recipe after `gp-twop-oracle`. The oracle skips only without the capture
directory.

Run: `make gp-twop-ko-oracle`
Expected: exit 0, `gp_twop: gp-twop-ko: evidence: ... ok`, and the measured
ratchet lines.

- [ ] **Step 10: Record §F4 and commit**

```bash
git add tools/gp_session.py tools/tests/test_gp_session.py Makefile \
        port/tests/test_platform.c \
        docs/superpowers/plans/2026-10-26-fought-match-truth-derivations.md
git commit -m "gameplay: capture a hand-dealt two-human KO"
```

On the failure branch, commit only the scenario/test/predicate code and record
the failed capture; do not stage the Makefile oracle block or the gate count.

---

### Task 5: The U16 deciding state

**Files:**
- Modify: `tools/gp_session.py` (`SCENARIOS['gp-u24-u16-state']`, `STOP_AT_END`)
- Modify: `tools/tests/test_gp_session.py`
- Modify: `port/tests/test_platform.c` (`k_gp_sets`)
- Modify (only if a raw-derived fix is proved): `port/src/game/fighter.c`,
  `port/tests/test_fight.c`, `port/tests/diff_runner.c` if the row changes
- Modify: `Makefile` (the U23 comment/pins on improvement only)
- Modify: `docs/superpowers/plans/2026-10-26-fought-match-truth-derivations.md` (§F5)

**Interfaces:**
- Consumes: `SNAP_FIELDS` `s0_30`/`s0_42` (Task 1), `stop_mode`, `SCENARIOS['gp-u23-u16-finisher']`, the port trace's new fields.
- Produces: `SCENARIOS['gp-u24-u16-state']`; the measured original/port state at the case-3 exit (marker-located; f=0x1533 in the current capture); either a raw-derived fix with the boundary test or a recorded refutation.

- [ ] **Step 1: Write the failing scenario test**

```python
def test_the_u16_state_capture_clones_u23_and_stops_on_the_lands(self):
    scn = gs.SCENARIOS['gp-u24-u16-state']
    self.assertEqual(scn['steps'], gs.SCENARIOS['gp-u23-u16-finisher']['steps'])
    self.assertEqual((scn['stop_mode'], scn['stop_delay']), (0x12, 0))
    self.assertIn('gp-u24-u16-state', gs.STOP_AT_END)
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `python3 -m unittest tools.tests.test_gp_session -k u16_state -v`
Expected: FAIL — no `gp-u24-u16-state`.

- [ ] **Step 3: Add the diagnostic scenario**

```python
SCENARIOS['gp-u24-u16-state'] = dict(
    SCENARIOS['gp-u23-u16-finisher'], stop_mode=0x12, stop_delay=0)
```

Add `'gp-u24-u16-state'` to `STOP_AT_END`. Do not add it to `CAPTURE_DIRS`;
it is a diagnostic capture, like `gp-idle-loss-run2`.

- [ ] **Step 4: Run the tests**

Run: `python3 -m unittest tools.tests.test_gp_session -k u16_state -v`
Expected: PASS.

- [ ] **Step 5: Capture and replay**

Run: `make title-pin` (if needed), then
`DISPLAY=:1 timeout 600 make gp-capture scenario=gp-u24-u16-state`.
Expected: every `check=` line passes.

Run: `make gp-replay scenario=gp-u24-u16-state`
The first run is expected to FAIL the unpinned miss-set check (and only that
check); it writes the miss log and `/tmp/pr_gp_dump/gp-u24-u16-state/trace.txt`.
From the miss log, add `k_miss_gp_u24_u16_state` to
`port/tests/test_platform.c` with the measured set (expected the U23 pair
`0x29D60`/`0x5D812`); re-run the replay; expected exit 0 with exactly that
set.

- [ ] **Step 6: Compare the deciding fields**

Run this one-off (record the output in §F5). It anchors on the first mode-9
entry and the following `s0_54` 2→0 transition, not on the old capture's
literal frame:

```bash
python3 - <<'PY'
import sys
sys.path.insert(0, 'tools')
import gp_session as gs
cap = gs.snapshots(open('data/k11-captures/gp-u24-u16-state/poll.log'))
port = {}
for line in open('/tmp/pr_gp_dump/gp-u24-u16-state/trace.txt'):
    r = gs.parse(line)
    if r and r['kind'] in ('S', 'T'):
        port.setdefault(r['f'], r)

def marker(rows):
    f9 = next(f for f in sorted(rows) if rows[f].get('mode') == 0x09)
    f_set = next(f for f in sorted(rows) if f > f9 and rows[f].get('s0_54') == 2)
    f_clear = next(f for f in sorted(rows) if f > f_set and rows[f].get('s0_54') == 0)
    return f9, f_set, f_clear

cf9, cset, cclear = marker(cap)
pf9, pset, pclear = marker(port)
print('capture mode9/set/clear %04X %04X %04X' % (cf9, cset, cclear))
print('port    mode9/set/clear %04X %04X %04X' % (pf9, pset, pclear))
for label, f, rows in (('capture', cclear, cap), ('port', pclear, port)):
    r = rows[f]
    print('%s at +0: s0_30=%08X s0_42=%04X s1_30=%08X s1_42=%04X'
          % (label, r.get('s0_30', 0), r.get('s0_42', 0),
             r.get('s1_30', 0), r.get('s1_42', 0)))
first = next((f for f in sorted(set(cap) & set(port))
              if f > cset and cap[f].get('s0_30') != port[f].get('s0_30')), None)
print('first s0_30 difference after the set:', 'none' if first is None else '%04X' % first)
PY
```

The capture's `mode9/set/clear` values may differ from the old capture's
0x1486/0x1532/0x1533; the marker is what aligns them. The port's values may
differ from 0x155C for the same reason. Compare the state at each side's
`clear` and the first `s0_30` difference after the set.

- [ ] **Step 7: Decide from the bytes**

If the original's `s0_30` at the case-3 exit is below 0x1400 and differs from
the port's 0x202E: use the first-difference frame and the raw chain
(`fighter_slot_latch` 0x186D0, `fighter_48374` 0x48374, `fighter_4844c`
0x4844C case 3, `fighter_1890c` 0x1890C) to find the source. Fix only a
source the raw bytes prove. Add the boundary test in `port/tests/test_fight.c`:
with `slot+0x30 = 0x13FF` case 3 ends (`+0x54=0`, `+0x57=4`, the 0xC8B58
stream, `x = 0x108384[side]`), with `0x1400`/`0x1401` it does not; and with
`slot+0x42` bit 3 set `fighter_slot_latch`'s else arm gives
`slot+0x30 = rec+0x1C`. Prove the test fails under a mutation of the boundary
or the latch arm. Re-run `make gp-u23-oracle`; if the replay now reaches
`r0=0x33`, move the moves/trace pins and prove one step.

If the original's `s0_30` matches the port's: the y/anchor hypothesis is
refuted. Record the measured values and the first divergent field among the
six; leave R2 a named gap. Do not change the gate or the y.

- [ ] **Step 8: Record §F5 and commit**

§F5 records the capture, the compared values, the first difference or the
refutation, the decision, and (if taken) the fix's raw instruction, test and
one-step proofs.

```bash
git add tools/gp_session.py tools/tests/test_gp_session.py \
        port/tests/test_platform.c docs/superpowers/plans/2026-10-26-fought-match-truth-derivations.md
# plus the port fix/test files only if taken
git commit -m "gameplay: measure the U16 finisher state"
```

---

### Task 6: Docs, counters and the phase gate

**Files:**
- Modify: `README.md`
- Modify: `AGENTS.md`
- Modify: `docs/PROGRESS.md`
- Modify: `port/RE_GUIDE.md` (only if a claim/oracle line changes)
- Modify: `docs/superpowers/specs/2026-10-25-fought-match-truth-design.md` (status)
- Modify: `docs/superpowers/plans/2026-10-26-fought-match-truth-derivations.md` (§F6)
- Modify: `Makefile` comments (the phase's measured provenance)

**Interfaces:**
- Consumes: Tasks 1-5's measured values.
- Produces: the docs truth pass and the gate evidence.

- [ ] **Step 1: Correct the known stale lines**

- `AGENTS.md`: U13 `MIN_REACH` 4952 → 4951; the U14 attribution ("the two
  permitted attempts were spent in gameplay truth II; endgame Task 8
  declined"); the U14 pin count is nine on success (`MAX_MISSING` joins the
  eight) and the captured claim replaces the absent-capture sentence; the
  5120 wording ("a byte-pinned 13-pixel allowance; its cause
  is unisolated", not "the last unexplained pixel"); the music-truth sentence
  already in the file stays (register stream verified; audibility a model gap);
  the capture inventory matches the Makefile (35, or 36 if `gp-twop-ko` was
  wired; U14 absent only if it failed) and the command list gains
  `make gp-twop-ko-oracle` only when that oracle is wired.
- `docs/PROGRESS.md`: append the phase narrative with the measured U14
  outcome, the two-human KO (or its failure), and the U16 decision.
- The new captures write `.dro` files. `tools/audio_compare.py`'s explicit
  29-trace inventory does not include them; record that they are unattributed
  (no audio claim) and do not add them without a bank-attribution task. The
  audio oracle's 29/29 summary is unchanged.
- `README.md`: update the named-gaps paragraph only for claims the phase
  actually changed; keep the title percentage at 66% (no new `FN_`).
- The design's status: `Implemented and gated` if both the U14 and two-human
  arms produced claims; otherwise `Partially implemented` with the open named
  gaps named. Its §7.5 gate-count sentence becomes 3 required + 2
  compare-if-present + 30 skip = 35 before the phase's capture, 36 after.

- [ ] **Step 2: Re-measure the counters**

Run:

```bash
python3 tools/port_progress.py
```

Expected: `799 1203 66` and `759 759 100` (no new function).

Run `make entry-triage` and `git diff --stat` to confirm the E2 table and
`symbols.h` are unchanged; if a ported function was added by a Task 5 fix,
regenerate the E2 table in the same commit.

- [ ] **Step 3: Run the full gate**

Run:

```bash
PR_DIFF_FULLSCAN=1 GATE_STRICT=1 PR_PINS_EXACT=1 make verify
```

Expected: exit 0. Check the log line by line:
- `gate: captures ... present, ... missing` with no missing captures, or only
  the declared failed U14;
- every new oracle ran (not skipped);
- `diff-verify: 1033/1033 functions VERIFIED; 14993/14993 mutants detected;
  0 named gaps; ...`
- `entry-triage` matches a fresh run;
- `symbols.h` regenerates byte-identically;
- the last line `all checks passed`.

If a pin improved unexpectedly, stop and record it; `PR_PINS_EXACT=1` fails an
improvement.

- [ ] **Step 4: Optional audio regression**

Run `make audio-render` if the gate budget allows; expected peak 2790 and
sha256 `530a94301f4be46f9a5c666c94516f8a1f7fca6f136d3be0562faed802c47cef`.
No audio file is changed by this phase.

- [ ] **Step 5: Record §F6 and commit**

§F6 records the gate log path, the counter line, the oracle lines, the
one-step proofs, and every named gap left open.

```bash
git add README.md AGENTS.md docs/PROGRESS.md port/RE_GUIDE.md \
        docs/superpowers/specs/2026-10-25-fought-match-truth-design.md \
        docs/superpowers/plans/2026-10-26-fought-match-truth-derivations.md Makefile
git commit -m "docs: close the fought-match truth phase"
```

---

## Phase acceptance summary

1. U14: one run; pass → nine measured pins + `live_death` green; fail →
   recorded, pins empty, R1 named. No second run.
2. Two-human: a hand-dealt KO with the derived `check --ko` passing, the round
   result fields measured, `b0c` claimed only if observed, no pokes; the
   frame/trace/moves pins measured and one-step proved. A time-out is a failed
   task.
3. U16: the original's `s0_30`/`s0_42` at the case-3 exit measured by the
   `s0_54` 2→0 marker; a raw-derived fix only if the bytes prove one, with the
   boundary test and the U23 oracle re-run; otherwise R2 stays a named gap
   with the refutation/retention.
4. 5120 stays a named gap with its measured facts; no pixel fix.
5. `make verify` exits 0; only the phase's own pins move; counters unchanged
   unless a raw-derived fix adds a row; docs current.
6. The derivation record has every measurement, poke, allowance and named gap.
