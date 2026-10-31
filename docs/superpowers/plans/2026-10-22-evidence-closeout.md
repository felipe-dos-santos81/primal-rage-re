# Evidence Closeout and Truth Pass — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Take Gameplay Truth II's five held captures (`gp-u12-endurance`, `gp-u13-ending2`, `gp-u14-win-live`, `gp-u15-join`, `gp-u16-twop2`), pin and prove each oracle, add the evidence checks their claims need, and make the durable docs and load-bearing comments match the ported tree.

**Architecture:** Tools and truth pass first (Tasks 1–4), then the five captures lowest-risk first (Tasks 5–9), then a closeout (Task 10) that re-measures every pin and runs the full gate. Each capture task measures its replay's `fn_resolve` miss set, pins the Makefile block with provenance, proves every pin fails at +1, and records a §J section. No new ported functions, no differential rows, no packaging/CI.

**Tech Stack:** DOSBox-X capture harness (`tools/gp_capture.py`, `tools/gp_session.py`), Python judge tools (`gp_compare.py`, `gp_win.py`, `gp_twop.py`, `gp_modes.py`), the Makefile `GP_U12_*`…`GP_U16_*` blocks, the C `PR_GP_DUMP` driver, and the SDL3 port's `make verify` gate.

**Spec:** `docs/superpowers/specs/2026-10-22-evidence-closeout-design.md`

## Global Constraints

- **Session-gated captures.** The user has approved the session; nobody may type in the DOSBox-X window while `make gp-capture` runs. Use `DISPLAY=:1` (the host display where the earlier captures ran). `data/` is read-only except the capture's own `data/k11-captures/<scenario>` write.
- **`check-input` clean.** A `FAIL unscripted input` stops that capture: do not pin it; re-take it only with the user's go-ahead.
- **Pins are measured and fail at +1.** Every `GP_U1x_*` value comes from the capture's report with its provenance in the Makefile comment and the record. No fitted or copied value. A pin that passes at +1 is wrong.
- **Narrow claims.** The oracles claim only that no content-bearing capture frame in the exhibited window is unexplained and that the scripted end was reached. No port-frame order, no interior coverage, no claim past the capture's end.
- **No new porting.** If a replay needs an unported target to run, that scenario is a named gap. Pinning a `fn_resolve` miss with its address/caller evidence is not porting.
- **Allowances are measured per scenario.** `SCAN3_ALLOWED` / `OVERLAY_ALLOWED` entries are byte-checked and mutation-tested; a U10 entry cannot be copied to U13.
- **Existing pins do not move.** The byte-exact oracle lines, every existing capture pin, `port_progress` (`799 1203 66` / `759 759 100`) and `diff-verify` (`1019/1019`, `14956/14956`, 0 named gaps, `774/833`, 8 unrowed) stay unchanged. `make verify` is green at the closeout.
- **One failure does not stop the rest.** A failed capture leaves its scenario as a named gap with the evidence; the other tasks proceed.

## Review Focus

1. **A capture that ends on its time limit instead of its script** — the oracle must fail on the capture's end/identity, never pin a time-limit run. Tested in every capture task's step «run the oracle» and the closeout's re-measure.
2. **A pin that passes at N + 1** — every capture task raises each pin by one and expects FAIL; a passing +1 is a review rejection.
3. **Unscripted input** — `check-input` in each capture task; a non-clean input stops the task.
4. **A sibling capture's pin or allowance copied over** — U13 clones U10 and U14 shares U9's machinery; Tasks 7 and 9 measure their own values, and the record names any copied shape.
5. **A replay miss pinned without evidence, or a new ported function** — each capture task's miss-set step records the exact `fn-miss` addr/ctx; an unported target stops the claim.
6. **A docs claim contradicted by the tree** — Task 1's greps must return no stale "no-op"/"unported"/"byte-wise" hits; Task 2's command list must match `make help-all`.
7. **A claim the evidence check cannot prove** (U16's finisher, U12's 0x31) — Tasks 3 and 4 add the capture-side checks before the captures; a capture without the state is a named gap, not a green pin.

---

### Task 1: Truth pass — the spec's storage, config and clock claims

**Files:**
- Modify: `port/spec/game_flow.md` (`:663-666`, `:794-796`, `:889-892`, `:912-913`, `:935-936`, `:957-958`, `:969-970`, `:984-986`)
- Modify: `port/src/platform/sprite.h:105-107`
- Modify: `port/src/host.h:129-131`

**Interfaces:**
- Consumes: the ported functions in `port/src/game/config.c` (`config_run_clock` 0x32970, `audit_add` 0x2DAE4, `audit_add_if_bit0` 0x32B94, `config_file_load` 0x1AFE8, `config_file_save` 0x1B084, `config_image_store` 0x2EA68) and `sprite.c:116-129`.
- Produces: no code change; the spec and comments state what the tree does.

- [ ] **Step 1: Record the stale hits (the failing check)**

Run:
```bash
grep -n "declared a no-op\|No storage I/O\|deferred in the port\|is unported (4b carve-out)\|both no-ops here\|deferred with the audit layer" port/spec/game_flow.md
grep -n "added byte-wise" port/src/platform/sprite.h
grep -n "through host_kb_bit" port/src/host.h
```
Expected: the six game_flow hits, the sprite.h hit and the host.h hit.

- [ ] **Step 2: Correct `game_flow.md`**

Each bullet names the stale claim and the tree's truth; keep the surrounding addresses and record citations.

- `:663-666` — "the deferred audit `0x32B94` and run-clock `0x32970` (both no-ops here)" → they are ported and called: `audit_add_if_bit0` (`config.c`), `config_run_clock` (`config.c`), as the same sentence's `config_play_time_close` already says.
- `:794-796` — "`0x2DAE4` (deferred, spec §7)" → `audit_add` (`config.c`), ported.
- `:889-892` — "With the storage layer … declared a no-op, validate always takes the defaults path" → the storage layer is ported: `config_file_load`/`config_file_save` through the host save calls, `config_image_store` (the `0x100CE4..0x1014DB` image) and the kind-0 counters/high-score tables. `config_validate` reads the loaded image; the defaults path is for a missing/short image, as on a fresh machine. `make save-reload-oracle` pins the write and the load.
- `:912-913` — "No storage I/O … and the deferred module taps `0x1AE20`/`0x2EA78`" → remove; both taps are ported in `config.c`, and the save/load path is the one above.
- `:935-936` — "`host_key_bits()` (through `host_kb_bit`, the key-config block …)" → `host_input_fill_keyboard()` writes the BIOS key-state table from the host's own key state (`host_key_bits()`/`host_key_scans`); `host_kb_bit` resolves a key-config binding when the block exists. The sampler `0x1BBAC` remains the sole writer of `+0x2D8/+0x2D9`.
- `:957-958` — "`0x1B084` (deferred in the port, record §50-C)" → `config_file_save`, called on the ESC/soft-restart path and by the OPTIONS MENU's leave.
- `:969-970` — "Alt-J the joystick calibration `0x5004A` (host-owned … not ported)" → the Alt-J path calls `host_pad_center()` (`flow.c:7430-7433`); the six PIT/game-port routines stay host-owned.
- `:984-986` — "`0x32970` is unported (4b carve-out)" → `config_run_clock`, ported and wired at its call sites.

- [ ] **Step 3: Correct the two comments**

- `port/src/platform/sprite.h:105-107`: replace "with the bank offset added byte-wise to every pixel" with the dword-carry truth: the bank is added as a dword over each four-pixel group; the carry chains within the group and restarts every four pixels; a run's tail is a carried word (record §W1; `sprite.c:116-129`).
- `port/src/host.h:129-131`: replace "the SDL keys pressed, through `host_kb_bit`" with the fill's real source (`host_key_bits`/`host_key_scans`), keeping `host_kb_bit` as the key-config binding helper.

- [ ] **Step 4: Verify the stale claims are gone**

Run the three greps from Step 1. Expected: no output from each. Run `git diff --check`. Expected: exit 0.

- [ ] **Step 5: Commit**

```bash
git add port/spec/game_flow.md port/src/platform/sprite.h port/src/host.h
git commit -m "docs: evidence closeout the spec's storage and clock truth"
```

---

### Task 2: Truth pass — commands, fixtures, guide and phase status

**Files:**
- Modify: `AGENTS.md:8-42`
- Modify: `Makefile:85-100`
- Modify: `README.md:141-160`
- Modify: `port/RE_GUIDE.md:232-265`
- Modify: `docs/superpowers/specs/2026-10-20-gameplay-truth-ii-design.md:3-7`
- Modify: `docs/superpowers/plans/2026-10-20-gameplay-truth-ii.md:1-12`
- Modify: `docs/superpowers/plans/2026-10-21-host-input-truth-derivations.md` (`:1524`, the §H2.5 citation)
- Modify: `docs/superpowers/specs/2026-10-21-host-input-truth-design.md:71`

**Interfaces:**
- Consumes: `make help-all` (the authoritative target list) and the Makefile's `deps`/`verify` checks.
- Produces: the command list, the fixture/tool prerequisites and the phase status agree with the tree.

- [ ] **Step 1: Record the current gaps (the failing check)**

Run:
```bash
make deps
make help-all | grep -E "gp-u1[2-6]-oracle"
grep -n "title_screen_ref.ppm" AGENTS.md
```
Expected: `make deps` prints no `ghidra_data.bin`/unicorn/capstone/CMOS line; `make help-all` lists the five targets (they exist) while `AGENTS.md` lists none of them; the fixture line names the unused `title_screen_ref.ppm`.

- [ ] **Step 2: Update AGENTS and the dependency check**

- Add `gp-u12-oracle` … `gp-u16-oracle` to the Commands block after `gp-modes-oracle`, one line each, naming the capture and its skip. Keep `gp-u17-oracle` where it is.
- Replace the fixture paragraph (`AGENTS.md:40-42`) with: a fresh worktree needs `port/tests/ghidra_data.bin` copied in (git-ignored; `make re-oracle` regenerates it). Drop `title_screen_ref.ppm` — no test reads it (only `.gitignore`, `make clean`, and old plans name it).
- `make deps`: add warnings for `port/tests/ghidra_data.bin`, `python3 -c "import unicorn"`/`"import capstone"`, and `data/game/C/CMOS` being absent or 2040 zero bytes (the `make verify` precondition). Keep the existing checks.

- [ ] **Step 3: Update README and RE_GUIDE**

- README "Verify": after the capture-oracle sentence, add the prerequisites: `port/tests/ghidra_data.bin`, `unicorn`/`capstone` (`tools/requirements-diff.txt`), and the absent-or-all-zero `data/game/C/CMOS`; the capture-based oracles skip without their git-ignored captures.
- `RE_GUIDE.md` "Next steps": mark item 3 done or drop it; replace items 2/5's stale "remain" list with the real remaining list: the five held captures, the differential audit (the 59 open rows/8 unrowed), host/product work (audio audibility, CI/packaging, pad hot-plug, save-path docs), and the still-open tick-vector item.

- [ ] **Step 4: Make the phase status truthful**

- GT2 design line 3: `Status: DRAFT` → `Status: implemented; capture Tasks 9–12 open (evidence closeout plan 2026-10-22).` Task 10 replaces the second clause with the measured result.
- GT2 plan: add the same status line under the header.
- Host-input record `:1524`: "26 the six keyboard and two F readers" → "26 (the six keyboard readers — two of them the F readers)".
- Host-input record §H2.5: cite §H2.3/§H2.6 for the 26 split (the record's §H2.5 line as the source is wrong).
- Host-input design `:71`: "game_loop writes both bytes" → the sampler writes `+0x2D8/+0x2D9`; `game_loop` runs the fills and the pump.

- [ ] **Step 5: Verify**

Run:
```bash
make deps
grep -c "gp-u1[2-6]-oracle" AGENTS.md
grep -n "title_screen_ref.ppm" AGENTS.md
grep -n "Status: DRAFT" docs/superpowers/specs/2026-10-20-gameplay-truth-ii-design.md
git diff --check
```
Expected: `make deps` names the fixture, unicorn/capstone and CMOS; the AGENTS count is 5; the last two greps are empty; `git diff --check` is silent.

- [ ] **Step 6: Commit**

```bash
git add AGENTS.md Makefile README.md port/RE_GUIDE.md \
  docs/superpowers/specs/2026-10-20-gameplay-truth-ii-design.md \
  docs/superpowers/plans/2026-10-20-gameplay-truth-ii.md \
  docs/superpowers/plans/2026-10-21-host-input-truth-derivations.md \
  docs/superpowers/specs/2026-10-21-host-input-truth-design.md
git commit -m "docs: evidence closeout the command, fixture and phase truth"
```

---

### Task 3: U12 evidence check and scenario shape

**Files:**
- Modify: `tools/gp_modes.py` (`ROWS`, `check`)
- Modify: `tools/tests/test_gp_modes.py`
- Modify: `tools/tests/test_gp_session.py:541-555`

**Interfaces:**
- Consumes: `gp_modes.check(name, lines) -> [(label, ok, detail)]` and `gp_modes.ROWS`.
- Produces: `ROWS['gp-u12-endurance']` with a `report` tuple; `check` adds a non-failing "mode 0x%X reached (report)" row for each `report` mode.

- [ ] **Step 1: Write the failing tests**

In `tools/tests/test_gp_modes.py`, add (the file imports `gp_session as gs` and `gp_modes as gm`):
```python
def test_u12_row_reaches_the_fight_and_reports_0x31(self):
    self.assertEqual(gm.ROWS['gp-u12-endurance']['reach'], 0x30)
    lines = ...  # the existing synthetic poll.log builder, with modes 0x27, 0x2B, 0x10, 0x30 and an X record
    rows = dict((label, ok) for label, ok, _ in gm.check('gp-u12-endurance', lines))
    self.assertTrue(rows['the row mode 0x2B appears'])
    self.assertTrue(rows['mode 0x30 reached'])
    self.assertTrue(rows['the scenario end (X record)'])
    self.assertIn('mode 0x31 reached (report)', rows)   # the report row exists
```
Add a negative case: the same lines without any `mode=0x30` record make `rows['mode 0x30 reached']` false.

In `tools/tests/test_gp_session.py`, extend the U12 test:
```python
self.assertEqual(scn['time_limit'], 70)
self.assertEqual(steps, gs.SCENARIOS['gp-u8-endurance']['steps'][:-1]
                 + gs.ENDURANCE_PICKS + (('until_mode', 0x30, 300),))
```
This pins the eight-pick block and the exact last step, so the u8 prefix + D3 cut cannot pass.

- [ ] **Step 2: Run the tests to verify they fail**

Run: `python3 -m unittest tools.tests.test_gp_modes tools.tests.test_gp_session -v`
Expected: FAIL (no `gp-u12-endurance` row / the `report` key).

- [ ] **Step 3: Implement the row and the report line**

In `gp_modes.py`:
```python
'gp-u12-endurance': dict(row=5, mode=0x2B, b1d=3, b1f=3, spend=0,
                         reach=0x30, stay=False, report=(0x31,)),
```
After the `want['reach']` check, add:
```python
for m in want.get('report', ()):
    r2 = next((r for i, r in ms if sel is not None and i > sel and r['mode'] == m), None)
    out.append(('mode 0x%X reached (report)' % m, True,
                'not reached' if r2 is None else 'f=%X' % r2['f']))
```
The row is a report, not a failure: 0x30 is the required fight entry, 0x31 is named in the record as present or absent.

- [ ] **Step 4: Run the tests to verify they pass**

Run the same command. Expected: PASS.

- [ ] **Step 5: Mutation-prove the new assertion**

Change the expected `reach` in the test to 0x31 (or remove the `('until_mode', 0x30, 300)` step from the expected tuple) and rerun. Expected: FAIL. Restore.

- [ ] **Step 6: Commit**

```bash
git add tools/gp_modes.py tools/tests/test_gp_modes.py tools/tests/test_gp_session.py
git commit -m "tools: evidence closeout the U12 evidence check"
```

---

### Task 4: U16 finisher check and judge-rule corrections

**Files:**
- Modify: `tools/gp_twop.py` (`two_human`/new `finisher`, `main`)
- Modify: `tools/tests/test_gp_twop.py`
- Modify: `tools/tests/test_gp_win.py`
- Modify: `tools/tests/test_gp_session.py` (the U14 second anchor press's hold)
- Modify: `tools/gp_win.py` (the `0x37FF2` comments/messages at `:14`, `:208`, `:225`, `:237`)
- Modify: `tools/gp_session.py:398,408` (the same citation)
- Modify: `docs/superpowers/specs/2026-10-20-gameplay-truth-ii-design.md` and `.md` plan (the `0x37FF2` citations)
- Modify: `port/src/game/fighter.c:14400` (the `0x37FF2` comment)

**Interfaces:**
- Consumes: `gp_session.parse`'s S-record fields (`r0`/`r1`; record gameplay-u6 §U6.11) and `gp_twop.two_human`.
- Produces: `gp_twop.finisher(lines) -> dict(mode9, mode12, reactions, fail)`; `gp_twop.py check --finisher` runs the two-human check plus the finisher check.

- [ ] **Step 1: Write the failing tests**

In `tools/tests/test_gp_twop.py`, add (the file imports `gp_session as gs` and `gp_twop as gt`):
```python
def test_finisher_requires_a_reaction_and_the_end(self):
    lines = ...  # the existing synthetic two-human poll.log, with an S record r0=0x32 and modes 0x09/0x12
    self.assertEqual(gt.finisher(lines)['fail'], [])
    self.assertTrue(gt.finisher(lines)['reactions'])   # at least one of 0x32..0x34
    no_react = [l for l in lines if 'r0=32' not in l and 'r1=32' not in l]  # fixture-specific
    self.assertTrue(gt.finisher(no_react)['fail'])
```
and a mode-0x12-absent case that must fail. Use the test file's existing poll.log builder; do not invent a new format.

In `tools/tests/test_gp_win.py`, add (the file imports `gp_win as gw`):
```python
def test_u13_ending_is_blizzard(self):
    rows = gw.MILESTONES['gp-u13-ending2']
    label, mode, k, pred = next(r for r in rows if r[1] == 0x1F)
    self.assertIn('BLIZZARD', label)
    self.assertIn('c0=1', label)
```
The existing `_ending_tail(1, 'BLIZZARD')` supplies the label; the test makes a swapped label fail.

- [ ] **Step 2: Run the tests to verify they fail**

Run: `python3 -m unittest tools.tests.test_gp_twop tools.tests.test_gp_win -v`
Expected: FAIL (`finisher` missing / the label test's import or assertion).

- [ ] **Step 3: Implement `finisher` and wire `--finisher`**

`finisher(lines)`:
- parse S records with `gs.parse`, collect `r0`/`r1` values in `{0x32, 0x33, 0x34}` into `reactions`;
- `mode9 = any(r['mode'] == 0x09)`, `mode12 = any(r['mode'] == 0x12)`;
- `fail` names each missing item ("no finisher reaction 0x32..0x34 in any S record", "mode 0x09 not reached", "mode 0x12 not reached").
`main`: add `--finisher` (mutually compatible with the existing two-human check; not with `--join`), and when set run `two_human(lines)` then `finisher(lines)`, print both and exit 1 if either fails.

U13's capture-frame count is already enforced: `gp-u13-oracle` passes `--capture-frames $(GP_U13_CAPTURE_FRAMES)` to `gp_compare` (Makefile:1257), exactly as U10's target does. Do not add a redundant `gp_win.py` frames argument.

- [ ] **Step 4: Close the U14 evidence-test minors**

The GT2 ledger's deferred minors (`.superpowers/sdd/2026-10-20-gameplay-truth-ii/progress.md:93-95`): the zero-writer list omits `0x2703D`; the negative fixture clears `b0c` at `0x85E`, which no raw writer does; the second anchor press's hold is unpinned. Correct the list (the record's writers: `0x2703D`, `0x27200`, `0x27582`, `0x29744`, `0x37FF6`), make the negative case clear `b0c` at a frame a raw writer actually does, and pin the second anchor's hold in the U14 scenario test (`test_gp_session.py`).

- [ ] **Step 5: Sweep the death-done citation**

Replace `0x37FF2` with `0x37FF6` in the named comments/messages only. The record's §W.5 already cites `0x37FF6`; `0x37FF2` in the sweep is the check instruction, `0x37FF6` the write. Verify:
```bash
grep -rn "0x37FF2" tools/gp_win.py tools/gp_session.py \
  docs/superpowers/specs/2026-10-20-gameplay-truth-ii-design.md \
  docs/superpowers/plans/2026-10-20-gameplay-truth-ii.md port/src/game/fighter.c
```
Expected: no hits (the record keeps its `0x37FF6`).

- [ ] **Step 6: Run the tests to verify they pass**

Run the same unittest command. Expected: PASS.

- [ ] **Step 7: Mutation-prove the three assertions**

Remove the reaction from the finisher fixture → the test fails. Change the label to `'SAURON'` in the expected string → the U13 test fails. Change the second anchor's hold from 4 to 5 in the expected U14 step → the scenario test fails. Restore all three.

- [ ] **Step 8: Commit**

```bash
git add tools/gp_twop.py tools/tests/test_gp_twop.py tools/tests/test_gp_win.py \
  tools/tests/test_gp_session.py tools/gp_win.py tools/gp_session.py \
  docs/superpowers/specs/2026-10-20-gameplay-truth-ii-design.md \
  docs/superpowers/plans/2026-10-20-gameplay-truth-ii.md port/src/game/fighter.c
git commit -m "tools: evidence closeout the finisher and ending checks"
```

---

### Task 5: Capture `gp-u15-join`

**Files:**
- Modify: `Makefile` (`GP_U15_*` values; the provenance comment)
- Modify: `port/tests/test_platform.c` (the `k_miss_gp_u15_join` array and its `k_gp_sets` row)
- Modify: `docs/superpowers/plans/2026-10-20-gameplay-truth-ii-derivations.md` (§J7)
- Modify: `docs/PROGRESS.md`
- Capture: `data/k11-captures/gp-u15-join` (git-ignored)

**Interfaces:**
- Consumes: the scenario at `gp_session.SCENARIOS['gp-u15-join']`, the `gp-u15-oracle` block and `gp_twop.py check --join`.
- Produces: a measured `GP_U15_*` block, a passing `make gp-u15-oracle`, and §J7.

- [ ] **Step 1: Pre-flight**

Run:
```bash
ls -d data/k11-captures/gp-u15-join && echo "UNEXPECTED: capture exists" || echo "ok: no capture"
git status --porcelain -uall
```
Expected: no capture; the worktree clean apart from the plan/design (untracked). Do not overwrite an existing capture.

- [ ] **Step 2: Capture**

Run: `DISPLAY=:1 make gp-capture scenario=gp-u15-join`
Expected: the session's checks all ok (`steps fired`, `end frame reached`, `no unscripted input`, `stopped at the end`). A `FAIL unscripted input` or an end-frame failure stops the task — do not pin.

- [ ] **Step 3: Check the input**

Run: `python3 tools/gp_capture.py check-input data/k11-captures/gp-u15-join`
Expected: `check=ok no unscripted input`.

- [ ] **Step 4: Replay and pin the miss set**

Run: `make gp-replay scenario=gp-u15-join`
Expected: the `PR_GP_DUMP` driver fails on an unexpected `fn_resolve` miss before the replay's output. Copy each `fn-miss ... unexpected 0xADDR from CALLER` line into a new array in `port/tests/test_platform.c`:
```c
static const fnm_pair k_miss_gp_u15_join[] = {
    { 0x5D812, "frontend_mode_1b_step" },   /* the base pair, from the failed run */
    { 0x29D60, "frontend_mode_1b_step" },
    /* the scenario's own measured rows, addr/ctx copied verbatim */
};
```
and add `{ "gp-u15-join", 0, k_miss_gp_u15_join, FNM_N(k_miss_gp_u15_join) }` to `k_gp_sets`. Rebuild and rerun `make gp-replay scenario=gp-u15-join` until it runs. If a miss is a genuinely unported target the replay must execute, stop: the scenario is a named gap (no new porting).

- [ ] **Step 5: Measure**

Run:
```bash
python3 tools/gp_compare.py --report --scenario gp-u15-join \
  --capture data/k11-captures/gp-u15-join --port /tmp/pr_gp_dump/gp-u15-join --max-unexplained 0
python3 tools/gp_twop.py check --join --capture data/k11-captures/gp-u15-join
```
Expected: the join evidence (mode 6 → 0x17, `b1f` 1 → 3, joining side's start mask newly pressed, no credit debit). Record the first unexplained, the trace first difference, the window start, the reach, the `poll.log` sha256 and the frame count. Verify the `--join` no-before guard at `tools/gp_twop.py:121` is covered by a `test_gp_twop.py` case; add one if it is not.

- [ ] **Step 6: Pin and prove**

Fill `GP_U15_MIN_FIRST`, `GP_U15_TRACE_MIN_FIRST`, `GP_U15_MAX_START`, `GP_U15_MIN_REACH`, `GP_U15_CAPTURE_SHA256`, `GP_U15_CAPTURE_FRAMES` from the report, with the measured report lines and the end-vs-divergence sentence in the comment. Run `make gp-u15-oracle`; expected PASS. Raise each pin by 1, rerun, expect FAIL, restore.

- [ ] **Step 7: Record and commit**

Write §J7 (the capture's checks, the miss set, the measured lines, the join evidence, the pin proofs) and a `docs/PROGRESS.md` paragraph. Commit:
```bash
git add Makefile port/tests/test_platform.c \
  docs/superpowers/plans/2026-10-20-gameplay-truth-ii-derivations.md docs/PROGRESS.md
git commit -m "tools: gameplay truth II the join capture"
```

---

### Task 6: Capture `gp-u12-endurance`

**Files:**
- Modify: `Makefile` (`GP_U12_*`; the `gp-u12-oracle` recipe gains the `gp_modes` check)
- Modify: `port/tests/test_platform.c` (`k_miss_gp_u12_endurance`, `k_gp_sets`)
- Modify: `docs/superpowers/plans/2026-10-20-gameplay-truth-ii-derivations.md` (§J4)
- Modify: `docs/PROGRESS.md`
- Capture: `data/k11-captures/gp-u12-endurance` (git-ignored)

**Interfaces:**
- Consumes: Task 3's `gp-u12-endurance` row and `check`; the `gp-u12-oracle` block.
- Produces: a measured `GP_U12_*` block, the evidence check wired into the oracle, and §J4.

- [ ] **Step 1: Pre-flight** — as Task 5 Step 1 for `gp-u12-endurance`.

- [ ] **Step 2: Capture** — `DISPLAY=:1 make gp-capture scenario=gp-u12-endurance`; the checks all ok; a failure stops.

- [ ] **Step 3: Check the input** — `python3 tools/gp_capture.py check-input data/k11-captures/gp-u12-endurance`; expected clean.

- [ ] **Step 4: Replay and pin the miss set** — as Task 5 Step 4, with `k_miss_gp_u12_endurance` and the `gp-u12-endurance` exact-name row. The base pair plus the scenario's own measured rows.

- [ ] **Step 5: Measure**

```bash
python3 tools/gp_compare.py --report --scenario gp-u12-endurance \
  --capture data/k11-captures/gp-u12-endurance --port /tmp/pr_gp_dump/gp-u12-endurance --max-unexplained 0
python3 tools/gp_modes.py check --scenario gp-u12-endurance --capture data/k11-captures/gp-u12-endurance
```
Expected: the mode path shows the ENDURANCE row 0x2B, the team pass, mode 0x30, and the X record; the 0x31 report says reached or not. If the first unexplained is the fight's mode-0x30 scan-out, pin `GP_U12_MIN_FIRST` there and state the narrow limit in the record; do not add an allowance unless the artifact is byte-pinned and mutation-tested.

- [ ] **Step 6: Wire the evidence check and pin**

Add to `gp-u12-oracle` before the compare:
```make
@$(PYTHON) -m unittest tools.tests.test_gp_modes
@$(PYTHON) tools/gp_modes.py check --scenario gp-u12-endurance --capture $(K11_CAPTURES)/gp-u12-endurance
```
Fill `GP_U12_MIN_FIRST`, `GP_U12_TRACE_MIN_FIRST`, `GP_U12_MAX_START`, `GP_U12_MIN_REACH`, `GP_U12_CAPTURE_SHA256`, `GP_U12_CAPTURE_FRAMES`. Run `make gp-u12-oracle`; expected PASS. Each pin +1 → FAIL; restore.

- [ ] **Step 7: Record and commit**

Write §J4 (the checks, the mode path, the 0x31 report, the measured lines and pin proofs) and the PROGRESS paragraph. Commit:
```bash
git add Makefile port/tests/test_platform.c \
  docs/superpowers/plans/2026-10-20-gameplay-truth-ii-derivations.md docs/PROGRESS.md
git commit -m "tools: gameplay truth II the ENDURANCE fight capture"
```

---

### Task 7: Capture `gp-u13-ending2`

**Files:**
- Modify: `Makefile` (`GP_U13_*`; add a measured `SCAN3_ALLOWED`/`OVERLAY_ALLOWED` entry in `tools/gp_compare.py` only if needed)
- Modify: `port/tests/test_platform.c` (`k_miss_gp_u13_ending2`, `k_gp_sets`)
- Modify: `tools/tests/test_gp_compare.py` (the allowance mutation test, if an entry is added)
- Modify: `docs/superpowers/plans/2026-10-20-gameplay-truth-ii-derivations.md` (§J5)
- Modify: `docs/PROGRESS.md`
- Capture: `data/k11-captures/gp-u13-ending2`

**Interfaces:**
- Consumes: Task 4's BLIZZARD label test; `gp_win.MILESTONES['gp-u13-ending2']`; the scenario's `memsize=64`.
- Produces: a measured `GP_U13_*` block, a passing `make gp-u13-oracle`, and §J5.

- [ ] **Step 1: Pre-flight** — no `data/k11-captures/gp-u13-ending2`; clean tree.

- [ ] **Step 2: Capture** — `DISPLAY=:1 make gp-capture scenario=gp-u13-ending2` (240 s; `memsize=64` comes from the scenario). The end-frame and input checks must pass.

- [ ] **Step 3: Check the input** — `python3 tools/gp_capture.py check-input data/k11-captures/gp-u13-ending2`.

- [ ] **Step 4: Replay and pin the miss set** — as Task 5 Step 4, with `k_miss_gp_u13_ending2`.

- [ ] **Step 5: Measure and check the evidence**

```bash
python3 tools/gp_win.py check --scenario gp-u13-ending2 --capture data/k11-captures/gp-u13-ending2
python3 tools/gp_compare.py --report --scenario gp-u13-ending2 \
  --capture data/k11-captures/gp-u13-ending2 --port /tmp/pr_gp_dump/gp-u13-ending2 --max-unexplained 0
```
Expected: the ending row `c0 = 1` (BLIZZARD), `ad4 = 0`; record the milestone count, first unexplained, trace/win first differences, window start and reach.

- [ ] **Step 6: Measure the allowances**

If the mode-8 entry's scan-out (the U10 331 class) or the loader's `- LOADING -` strip (the U10 2062 class) is the first unexplained, add a `SCAN3_ALLOWED['gp-u13-ending2']` / `OVERLAY_ALLOWED['gp-u13-ending2']` entry with the capture's own measured frame, rows, adjacent port pair and sha256 — never U10's values. Add the mutation test in `test_gp_compare.py` (change the pinned sha/pair → FAIL). If the artifact cannot be pinned, keep the claim's end before it and say so in the record.

- [ ] **Step 7: Pin and prove** — fill `GP_U13_MIN_FIRST`, `GP_U13_TRACE_MIN_FIRST`, `GP_U13_MAX_START`, `GP_U13_MIN_REACH`, `GP_U13_CAPTURE_SHA256`, `GP_U13_CAPTURE_FRAMES`, `GP_U13_MILESTONES`, `GP_U13_WIN_MIN_FIRST`. `make gp-u13-oracle` green; each pin +1 → FAIL; restore.

- [ ] **Step 8: Record and commit** — §J5 and PROGRESS; commit:
```bash
git add Makefile tools/gp_compare.py tools/tests/test_gp_compare.py port/tests/test_platform.c \
  docs/superpowers/plans/2026-10-20-gameplay-truth-ii-derivations.md docs/PROGRESS.md
git commit -m "tools: gameplay truth II the second ending capture"
```

---

### Task 8: Capture `gp-u16-twop2`

**Files:**
- Modify: `Makefile` (`GP_U16_*`; add `GP_U16_MOVES_MIN_FIRST` and `--moves-min-first` if the report has moves; add `--finisher` to the check)
- Modify: `port/tests/test_platform.c` (`k_miss_gp_u16_twop2`, `k_gp_sets`)
- Modify: `docs/superpowers/plans/2026-10-20-gameplay-truth-ii-derivations.md` (§J8)
- Modify: `docs/PROGRESS.md`
- Capture: `data/k11-captures/gp-u16-twop2`

**Interfaces:**
- Consumes: Task 4's `gp_twop.py check --finisher`; the scenario's finisher steps.
- Produces: a measured `GP_U16_*` block, a passing `make gp-u16-oracle` (two-human + finisher + frames), and §J8.

- [ ] **Step 1: Pre-flight** — no `data/k11-captures/gp-u16-twop2`.

- [ ] **Step 2: Capture** — `DISPLAY=:1 make gp-capture scenario=gp-u16-twop2` (240 s). The checks must pass.

- [ ] **Step 3: Check the input** — `python3 tools/gp_capture.py check-input data/k11-captures/gp-u16-twop2`.

- [ ] **Step 4: Replay and pin the miss set** — as Task 5 Step 4, with `k_miss_gp_u16_twop2`.

- [ ] **Step 5: Measure and check the evidence**

```bash
python3 tools/gp_twop.py check --finisher --capture data/k11-captures/gp-u16-twop2
python3 tools/gp_compare.py --report --scenario gp-u16-twop2 \
  --capture data/k11-captures/gp-u16-twop2 --port /tmp/pr_gp_dump/gp-u16-twop2 --max-unexplained 0
```
Expected: two-human counts, mode 0x09 and 0x12 reached, and a finisher reaction (`r0`/`r1` in 0x32..0x34). If the finisher never appears, do not pin it: record the attempt and the absent reactions, keep the enforced target to the two-human + frames claim, and name the finisher gap. One narrower retry is allowed; a fabricated pin is not.

- [ ] **Step 6: Wire the check and pin**

Add `--finisher` to the `gp-u16-oracle` call only if the capture proved the finisher (Step 5); if it did not, keep the two-human check and name the gap. If the report prints a moves first difference, add `GP_U16_MOVES_MIN_FIRST` and `--moves-min-first "$(GP_U16_MOVES_MIN_FIRST)"`; if the S records have no move fields, leave it empty and say so. Fill `GP_U16_MIN_FIRST`, `GP_U16_TRACE_MIN_FIRST`, `GP_U16_MAX_START`, `GP_U16_MIN_REACH`, `GP_U16_CAPTURE_SHA256`, `GP_U16_CAPTURE_FRAMES`. `make gp-u16-oracle` green; each pin +1 → FAIL; restore.

- [ ] **Step 7: Record and commit** — §J8 and PROGRESS; commit:
```bash
git add Makefile port/tests/test_platform.c \
  docs/superpowers/plans/2026-10-20-gameplay-truth-ii-derivations.md docs/PROGRESS.md
git commit -m "tools: gameplay truth II the extended two-player capture"
```

---

### Task 9: Capture `gp-u14-win-live`

**Files:**
- Modify: `Makefile` (`GP_U14_*`)
- Modify: `port/tests/test_platform.c` (`k_miss_gp_u14_win_live`, `k_gp_sets`)
- Modify: `tools/gp_compare.py` / `tools/tests/test_gp_compare.py` (the measured allowance, if needed)
- Modify: `docs/superpowers/plans/2026-10-20-gameplay-truth-ii-derivations.md` (§J6)
- Modify: `docs/PROGRESS.md`
- Capture: `data/k11-captures/gp-u14-win-live`

**Interfaces:**
- Consumes: `gp_win.py check` (the fought KO and the game's own `b0c`), the scenario's punch script.
- Produces: a measured `GP_U14_*` block, a passing `make gp-u14-oracle`, and §J6.

- [ ] **Step 1: Pre-flight** — no `data/k11-captures/gp-u14-win-live`; clean tree.

- [ ] **Step 2: Capture** — `DISPLAY=:1 make gp-capture scenario=gp-u14-win-live` (200 s). The checks must pass. If the original prints the OOM and the capture fails, add `memsize=64` to the U14 scenario dict (as U10 does), re-take once, and record it as a harness value — never a game value.

- [ ] **Step 3: Check the input** — `python3 tools/gp_capture.py check-input data/k11-captures/gp-u14-win-live`.

- [ ] **Step 4: Replay and pin the miss set** — as Task 5 Step 4, with `k_miss_gp_u14_win_live`.

- [ ] **Step 5: Measure and check the evidence**

```bash
python3 tools/gp_win.py check --scenario gp-u14-win-live --capture data/k11-captures/gp-u14-win-live
python3 tools/gp_compare.py --report --scenario gp-u14-win-live \
  --capture data/k11-captures/gp-u14-win-live --port /tmp/pr_gp_dump/gp-u14-win-live --max-unexplained 0
```
Expected: a fought KO, `b0c = 1` set by the game (not a DEATH_DONE poke), and the shared machinery rows. If the KO never lands, record the attempt and the evidence; the scenario is a named gap (no weakened claim, no fabricated pin). One narrower retry is allowed.

- [ ] **Step 6: Measure the allowance and pin**

If the death-animation frames hit the mode-8 three-frame scan-out, add `SCAN3_ALLOWED['gp-u14-win-live']` with the measured frame/rows/pair/sha and its mutation test; otherwise the evidence is the `0x37FF6` transition and the trace, and the record says so. Fill `GP_U14_MIN_FIRST`, `GP_U14_TRACE_MIN_FIRST`, `GP_U14_MAX_START`, `GP_U14_MIN_REACH`, `GP_U14_CAPTURE_SHA256`, `GP_U14_CAPTURE_FRAMES`, `GP_U14_MILESTONES`, `GP_U14_WIN_MIN_FIRST`. `make gp-u14-oracle` green; each pin +1 → FAIL; restore.

- [ ] **Step 7: Record and commit** — §J6 (the live KO, the shared-code limit, the `0x37FF6` evidence, the pin proofs) and PROGRESS; commit:
```bash
git add Makefile tools/gp_compare.py tools/tests/test_gp_compare.py port/tests/test_platform.c \
  docs/superpowers/plans/2026-10-20-gameplay-truth-ii-derivations.md docs/PROGRESS.md
git commit -m "tools: gameplay truth II the live-win capture"
```

---

### Task 10: Closeout

**Files:**
- Modify: `docs/superpowers/specs/2026-10-20-gameplay-truth-ii-design.md` (the status line)
- Modify: `docs/superpowers/plans/2026-10-20-gameplay-truth-ii.md` (the status line)
- Modify: `docs/superpowers/plans/2026-10-20-gameplay-truth-ii-derivations.md` (a short §J index if one is useful)
- Modify: `docs/PROGRESS.md`
- Add: `docs/superpowers/specs/2026-10-22-evidence-closeout-design.md`
- Add: `docs/superpowers/plans/2026-10-22-evidence-closeout.md`

**Interfaces:**
- Consumes: every capture task's pins, record section and oracle.
- Produces: a green full gate and the phase's truthful status.

- [ ] **Step 1: Re-measure every new pin**

For each of the five, run its oracle and its `--report` pass; confirm the pin is the first unexplained/differing value and that +1 fails. Record any re-measurement in the record.

- [ ] **Step 2: Finalize the status**

GT2 design and plan: replace "capture Tasks 9–12 open" with the measured truth — all five taken and pinned, or name exactly which remain open and why. The record gets a short index of §J4–§J8 (the capture sections) if it does not already read cleanly.

- [ ] **Step 3: Run the full gate**

Run: `make verify` (about 26 minutes).
Expected: exit 0; every existing oracle line and pin unchanged; the five new oracles pass (or their named gap is recorded); `diff-verify` `1019/1019`, `14956/14956`, 0 named gaps, `774/833` (186 have none), 8 unrowed; `port_progress` `799 1203 66` / `759 759 100`.

- [ ] **Step 4: Append the phase paragraph**

`docs/PROGRESS.md`: the five captures, their pins, the named limits (which claims stop where), the truth-pass items and the fact that no ported function or existing pin moved.

- [ ] **Step 5: Commit**

```bash
git add docs/PROGRESS.md \
  docs/superpowers/specs/2026-10-20-gameplay-truth-ii-design.md \
  docs/superpowers/plans/2026-10-20-gameplay-truth-ii.md \
  docs/superpowers/plans/2026-10-20-gameplay-truth-ii-derivations.md \
  docs/superpowers/specs/2026-10-22-evidence-closeout-design.md \
  docs/superpowers/plans/2026-10-22-evidence-closeout.md
git commit -m "docs: evidence closeout"
```

---

## Execution Notes

- Tasks 1–4 need no capture session; they can run while the session is closed.
- Tasks 5–9 are the session: run them in the order written (u15, u12, u13, u16, u14). The controller runs `make gp-capture`; nobody types in the window. If a capture fails its own checks, stop that task, record it, and continue with the next.
- The `fn_resolve` miss sets are measured, not guessed. A miss row without an address and caller from the actual run is a fabricated pin.
- A capture that cannot be made is a named gap; the phase closes with it named, never with a fitted pin.
- Never write to `data/game/C`. The capture's staging check and `check-input` are the proof.
- If a measured value differs from the design's expectation, the capture wins; record the correction and the evidence.
