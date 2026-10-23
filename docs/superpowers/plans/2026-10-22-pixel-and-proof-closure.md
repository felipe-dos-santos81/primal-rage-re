# Pixel and Proof Closure Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Close U13's in-replay pixel gap and the rowable differential frontier without a capture session or new portable code, and make the two evidence counters report what they measure.

**Architecture:** Two independent offline tracks. Track A classifies the three in-replay U13 capture frames (4248, 4320, 5120) against the existing replay, then fixes a real renderer divergence or adds a byte-pinned, mutation-tested allowance and re-pins the oracle. Track B adds 12 differential rows for game code whose callees are already VERIFIED, corrects one spec address that is not a function, and re-pins the counter. The tracks share only the final gate and the documentation/counter update.

**Tech Stack:** C port (`port/src`, `port/tests`), the existing Python differential harness (`tools/diff_verify.py`, `port/tests/diff_runner.c`), the existing gameplay oracle (`tools/gp_compare.py`, `tools/gp_win.py`, `tools/gp_session.py`), and read-only captures in `data/k11-captures/`. No SDL, no DOSBox-X, no new capture.

**Spec:** `docs/superpowers/specs/2026-10-22-pixel-and-proof-closure-design.md`

## Global Constraints

- No `make gp-capture`/`k11-capture`/DOSBox-X session; `data/` is read-only. The only capture used is the existing `data/k11-captures/gp-u13-ending2`.
- No new `port/src` column-0 header and no new `fn_register`. `0x5D812` and `0x61A70` are differential bindings only. `port_progress.py` must stay `799 1203 66` / `759 759 100`.
- No closure-rule change. A row closes only on VERIFIED callees or the existing allow-mode excuse; a stub/real callee keeps it open.
- No fitted constants. Every U13 allowance is a named per-frame byte recipe with a mutation test; every differential row's cases/mutants are derived from the raw bytes.
- Every existing byte-exact oracle pin must not move, except U13's `GP_U13_MIN_FIRST` (and `GP_U13_MIN_REACH` if the enforced port reach changes) when the first genuine unexplained frame moves. `GP_U13_TRACE_MIN_FIRST`/`GP_U13_WIN_MIN_FIRST` stay 10014; the capture sha/frames stay unchanged.
- Commit only if the operator authorizes execution. Stage named files; never `git add -A`. The commit steps below are conditional on that authorization.
- The live Ghidra `primal-rage` project was not available during planning; each new row/fix re-checks its address against the raw EXE bytes and `port/decomp/` first.

## Review Focus

These are the failure modes the spec implies but no task's test automatically covers. Each is pinned by the owning task's test.

1. **A U13 allowance that explains the current port's wrong bytes.** The allowance test must bind the capture's bytes and a port-side mutation; changing the port frame's bytes must un-explain the capture frame (Task 2/3).
2. **A differential row that closes on a stub or `real` callee.** The per-row self-check and the final counter must show every callee VERIFIED or allow-excused (Tasks 5-9, 11).
3. **A counter that moves without a new row.** The pinned string is updated only from the measured `diffrun` line; the row count and mutant count are recorded (Task 11).
4. **A `0x5D812`/`0x61A70` binding writing outside `mem[]`.** Every new binding and mutant stays within the image; `PR_DIFF_FULLSCAN=1` runs the full scans (Tasks 5, 8, 12).
5. **The `0x3D0BC` correction changing an existing row's verdict.** The `fighter_state_3531c` row and the new `0x3A6D4` row both pass; no other row's verdict changes (Task 9).

---

## Track A — U13 pixel truth

### Task 1: Reproduce and map the in-replay unexplained frames

**Files:**
- Create: `docs/superpowers/plans/2026-10-22-pixel-and-proof-closure-derivations.md`
- Read: `tools/gp_compare.py`, `tools/gp_session.py`, `tools/title_compare.py`, `data/k11-captures/gp-u13-ending2/{window.txt,poll.log}`

**Interfaces:**
- Produces: derivation record §P1 with a table of capture frame, raw frame, game `f`, `mode`, nearest port frame, differing rows/px, and the first splice/clean/DAC test result.

- [ ] **Step 1: Build and keep the U13 replay dump**

Run:
```bash
make build
GP_WIN_KEEP=1 make gp-u13-oracle
```
Expected: `gp-u13-oracle` PASS; `/tmp/pr_gp_dump/gp-u13-ending2/` is kept (the Makefile deletes it without `GP_WIN_KEEP`). Record the exact final lines.

- [ ] **Step 2: Reproduce the report**

Run:
```bash
python3 tools/gp_compare.py --report --scenario gp-u13-ending2 \
  --capture data/k11-captures/gp-u13-ending2 \
  --port /tmp/pr_gp_dump/gp-u13-ending2 --max-unexplained 0
```
Expected: exactly the record §J5 output, including `FIRST UNEXPLAINED capture 4248 (raw 7887): nearest port 3551, rows 0..96, x 0..319 (22923 px)` and `other unexplained capture frames 4320, 5120, and 5891..5903`.

- [ ] **Step 3: Map each in-replay frame**

For 4248, 4320 and 5120: read `data/k11-captures/gp-u13-ending2/window.txt` for the raw frame, then find the nearest preceding `S` record in `poll.log` and record its `f`, `mode`, `st`, `r0/r1`, `s0_5a/s1_5a`, and `b0c`. Mark 5891..5903 as the post-script tail (after the replay's X record) and out of the frame claim.

- [ ] **Step 4: Test the comparison model**

For each frame, compare the capture bytes to the nearest port frame and to the adjacent pair; run the `title_compare.explain` model (clean/splice/transition) and the DAC-index test. Record whether any of them explains the frame. Do not call it an artefact because it is large; record the negative result.

- [ ] **Step 5: Write record §P1**

Create the derivation record with the phase title, the exact commands, the frame table, and the negative/positive classification for 4248/4320/5120. State that no pin has moved yet.

- [ ] **Step 6: Commit (if authorized)**

```bash
git add docs/superpowers/plans/2026-10-22-pixel-and-proof-closure-derivations.md
git commit -m "docs: map the U13 in-replay unexplained frames"
```

### Task 2: Classify and close capture 4248

**Files:**
- Modify: `port/tests/test_video.c` or the `render_list_blit` unit/differential tests, if the frame is a real divergence
- Modify: `tools/gp_compare.py` and `tools/tests/test_gp_compare.py`, if the frame is a measured artefact
- Modify: `docs/superpowers/plans/2026-10-22-pixel-and-proof-closure-derivations.md`

**Interfaces:**
- Consumes: Task 1's §P1 table and the nearest port frames.
- Produces: a classification of 4248 as **A** (real port divergence), **B** (poke replacement), or **C** (capture/scan-out/comparison artefact); a failing-first test and a fix, or a named byte-pinned allowance; record §P2.

- [ ] **Step 1: Trace the renderer path**

Use the frame's `mode`/`f` and the port dump to identify the drawn object and the writer. The U10 precedent is the RLE span writers: the original adds the palette bank as a dword (`edx = [esi] + ebx`, `ebx = (bank - 1)` replicated), so a byte sum above 0xFF carries into the next pixel of the four-pixel group; the port once added byte-wise. Read `docs/superpowers/plans/2026-10-20-gameplay-truth-ii-derivations.md` §W1 and the current `render_list_blit` family before assuming the same cause.

- [ ] **Step 2: Prove a real divergence (branch A)**

If the capture's bytes differ from the port's on a path the state agrees on, add a failing unit test with the exact source bytes/seed and the exact expected pixels in `port/tests/test_video.c` (or the owning game test), and register it once in `port/tests/test.h`'s `TEST_CASES`. Run:
```bash
PR_ORACLE_REQUIRED=1 ./build/run_tests
```
Expected: the new test FAILS on the current port. Fix the owning writer (bank carry, run-tail word rule, table selection, or the specific path the bytes name). Re-run the test; expected PASS. If the fix changes a differentially rowed function, update that row's cases/mutants and run its `--function` self-check.

- [ ] **Step 3: Prove a measured artefact (branch C)**

If the bytes are a composition the two-frame model cannot express, add the frame to the matching table in `tools/gp_compare.py`:
- `BANDS_ALLOWED` if the unexplained region is a band with a pinned top-row hash and a unique adjacent port pair;
- `SCAN3_ALLOWED` if it is the three-segment catch-up;
- `OVERLAY_ALLOWED` only for a glyph overlay (the 166-px loader strip), not for a 22,923-px region.

The entry must bind the exact capture bytes and the exact port frames. Add a `tools/tests/test_gp_compare.py` test that pins the tuple and shows that changing the band hash, the screen pair or the port frame un-explains the fixture.

- [ ] **Step 4: Re-run the report**

Run the Task 1 Step 2 command. Expected: 4248 is no longer in the unexplained list (or is explained with the named allowance). Record the new first unexplained (likely 4320) and the exact output.

- [ ] **Step 5: Write record §P2**

Record the A/B/C decision, the raw addresses, the failing-first test or the allowance recipe, and the test output. If the outcome is C, state the named limit of the recipe (which bytes the recipe does not bind) exactly as the existing SCAN3/OVERLAY comments do.

- [ ] **Step 6: Commit (if authorized)**

Stage only the files changed by this task; use `fix:` for a real divergence and `test:`/`tools:` for an allowance.

### Task 3: Classify and close 4320 and 5120

**Files:**
- Same as Task 2, plus the record

**Interfaces:**
- Consumes: Task 2's classification and the re-run report.
- Produces: 4320 and 5120 explained or fixed; the next unexplained is 5891 or later; record §P3.

- [ ] **Step 1: Re-run the report after Task 2**

Run the Task 1 Step 2 command. Record the first unexplained and the remaining list. If 4320 is a different cause, treat it separately; do not reuse a Task 2 allowance unless the bytes match its recipe.

- [ ] **Step 2: Classify and close 4320**

Repeat Task 2's Step 1-4 for 4320. If it is the same writer/class as 4248, reuse the fix and add a case; if it is an artefact, add a separate named entry with its own bytes.

- [ ] **Step 3: Classify and close 5120**

Repeat for 5120. Record whether 4320/5120 share 4248's cause.

- [ ] **Step 4: Confirm the in-replay set is empty**

Run the report. Expected: the first unexplained is 5891 or later (the post-script tail), and 4248/4320/5120 are absent or explained by named recipes. Do not claim 5891..5903.

- [ ] **Step 5: Write record §P3 and commit (if authorized)**

Record the final classification table and the exact report output; commit the named files.

### Task 4: Re-pin U13 and enforce the new claim

**Files:**
- Modify: `Makefile` (`GP_U13_MIN_FIRST`, `GP_U13_MIN_REACH`, the U13 comment)
- Modify: `docs/superpowers/plans/2026-10-22-pixel-and-proof-closure-derivations.md`
- Modify: `docs/PROGRESS.md` (the phase paragraph is Task 12; Task 4 may add the U13 line)

**Interfaces:**
- Consumes: Task 3's first unexplained and the enforced port reach.
- Produces: the new `GP_U13_*` pins and the failing-direction proof; record §P4.

- [ ] **Step 1: Re-measure the enforced reach**

Run:
```bash
GP_WIN_KEEP=1 make gp-u13-oracle
```
The enforced compare stops at the first unexplained; record the port reach it prints. Do not use the report-only reach (it continues past the first unexplained). Set `GP_U13_MIN_REACH` to the enforced reach.

- [ ] **Step 2: Set `GP_U13_MIN_FIRST`**

Set it to the first unexplained after Task 3 (the post-script 5891 or later), not to a number that merely makes the oracle pass. If the frame is explained by an allowance, the next unexplained is the pin.

- [ ] **Step 3: Prove the pin fails in the raising direction**

Run with `GP_U13_MIN_FIRST=<N+1>` (a Makefile variable override) and confirm the oracle FAILS with `first unexplained ... < ratchet N`; restore N. Run with `GP_U13_MIN_REACH=<reach+1>` and confirm it fails; restore. Record both outputs.

- [ ] **Step 4: Enforce and record**

Run `make gp-u13-oracle`. Expected: PASS at the new pins; the trace and WIN pins remain 10014; the capture sha/frames unchanged. Update the Makefile comment (the old 4248/3549 provenance is historical) and record §P4 with the new pin table and failing-direction proofs.

- [ ] **Step 5: Commit (if authorized)**

```bash
git add Makefile docs/superpowers/plans/2026-10-22-pixel-and-proof-closure-derivations.md docs/PROGRESS.md
git commit -m "oracle: close the U13 in-replay pixel gap"
```

---

## Track B — Differential last mile

Run the differential driver directly after each row task; the `tools.tests.test_diff_verify` counter pin is updated once in Task 11, so `make diff-verify`'s unittest is expected to fail on that stale string until then. The per-task command is:
```bash
python3 tools/diff_verify.py --diffrun build/diffrun --exe data/game/C/PRAGE.EXE \
  --image /tmp/pr_diff_image.bin --table /tmp/pr_diff_table.md --self-check
```
Use `--function <spec name>` for a single row; its callee column reads `unverified` by design (the callees' rows are not run), so the full run is the acceptance.

### Task 5: Baseline and the frontend stub pair (`0x5D812`, `0x28D68`)

**Files:**
- Modify: `port/tests/diff_runner.c` (bindings/mutants)
- Modify: `tools/diff_verify.py` (two `Spec`s)
- Modify: `docs/superpowers/plans/2026-10-22-pixel-and-proof-closure-derivations.md`

**Interfaces:**
- Produces: VERIFIED rows for `0x5D812` and `0x28D68`; the 8 rows blocked by `0x5D812` (7 `frontend_mode_15..1b` + `attract_scene_tick`) and the 6 blocked by `0x28D68` close together. Expected closed `774 → 782`, full `768 → 776`, open `59 → 51`; functions `1019 → 1021`; mutants `+4` (re-measure).

- [ ] **Step 1: Record the baseline**

Run the driver command above. Record the exact final line (`1019/1019 … 774/833 … 768/833 … 8`). Do not edit the pinned string yet.

- [ ] **Step 2: Row `0x5D812` (binding only)**

`0x5D812` is `xor eax,eax; ret` (raw bytes at file offset `0xB0666` are `33 c0 c3`). The port models it inline; do **not** add a `port/src` header or `fn_register`. Add a binding in `port/tests/diff_runner.c` that returns EAX = 0 (and preserves the callee-saved registers the raw preserves), register it in `k_bindings`, and add its mutant. Add `Spec("runtime_5d812", 0x5D812, [Case("r0", {"eax": 0}, …)], mutants=("@ret",))` to `tools/diff_verify.py`. Run with `--function runtime_5d812` and then the full driver. Expected: VERIFIED; the two rows that only `0x5D812` blocks close; the other six wait for `0x28D68`.

- [ ] **Step 3: Row `0x28D68`**

`frontend_char_screen_hook` (`port/src/game/flow.c:976`) stores `0x43738` in `DS_00104AE4` and calls `frontend_wipe_arm(0x10)` (`0x4F980`). Add a `Spec` with:
- a case seeding `DS_00104AE4` with a sentinel (not the post-condition), `eax=0`;
- `E.Call(0x4F980, ("eax",))` (the ret_mode 0x10) using the existing `frontend_wipe_arm` seam;
- the post-condition `DS_00104AE4 == 0x43738`;
- mutants `@hook`, `@wipearg`, `@nowipe`.
Run the full driver. Expected: `782/833` closed, `776/833` full, 51 open; the 6 frontend rows close.

- [ ] **Step 4: Record and commit (if authorized)**

Record §P5 with the exact final line and the closed-row names. `git add port/tests/diff_runner.c tools/diff_verify.py docs/superpowers/plans/2026-10-22-pixel-and-proof-closure-derivations.md`; commit `diff: row the frontend stub and 0x28D68`.

### Task 6: The fighter callbacks (`0x3E484`, `0x29D04`, `0x3E4C4`)

**Files:**
- Modify: `port/tests/diff_runner.c`, `tools/diff_verify.py`, the record

**Interfaces:**
- Produces: VERIFIED rows for `0x3E484`, `0x29D04`, `0x3E4C4`; closes `fighter_19020`, `fighter_pass_a`, `fighter_pass_a_al`, `fighter_35050`, `fighter_think_side`, `fighter_state_350d0`, `winner_body` (7 rows). Expected closed `782 → 789`, full `776 → 783`, open `51 → 44`; functions `+3`; mutants `+9` (re-measure).

- [ ] **Step 1: Row `0x3E484`**

The port function is `fighter_3e484` (`port/src/game/fighter.c:8331`). Its callees: `0x33950` and `0x18BD4` (allow-mode, no seam), `0x18C14` (its own row). Use the existing `CHECKS = E.Call(0x18C14, ("eax", "[edx]", "[edx+4]", "[edx+8]", "[edx+12]", "ebx", "ecx"))` pattern from `tools/diff_verify.py`; add 2-3 cases varying the `0x18C14` stub EAX and the side; mutants `@flag0`, `@flag1`, `@flag8`, `@ret`. Expected: 3 rows close.

- [ ] **Step 2: Row `0x29D04`**

The port function is `fighter_29d04` (`port/src/game/fighter.c:6588`). Cases: `DS_0009AF3D != 0` returns 0; a seeded handle table takes the lookup and calls `0x2A17C(slot, 0, handle)`, returning 1. Record the `0x2A17C` call with its existing row's mode. Add 3 mutants. Expected: `fighter_35050`, `fighter_think_side`, `fighter_state_350d0` close.

- [ ] **Step 3: Row `0x3E4C4`**

The port function is `fighter_3e4c4` (`port/src/game/fighter.c:4686`). Two cases (side 0/1), record `0x3B714(eax, edx)`, 2 mutants. Expected: `winner_body` closes (with `0x29D04`).

- [ ] **Step 4: Verify, record, commit**

Run the full driver. Expected: `789/833` closed, `783/833` full, 44 open. Record §P6; commit `diff: row the fighter callback entries`.

### Task 7: The actor entrances (`0x24568`, `0x24804`, `0x40CB0`)

**Files:**
- Modify: `port/tests/diff_runner.c`, `tools/diff_verify.py`, the record

**Interfaces:**
- Produces: VERIFIED rows for `0x24568`, `0x24804`, `0x40CB0`; closes the `game_mode_05`/`game_mode_0d`/`game_mode_32` rows (8). Expected closed `789 → 797`, full `783 → 791`, open `44 → 36`; functions `+3`; mutants `+9` (re-measure).

- [ ] **Step 1: Row `0x24568`**

The port function is `fighter_24568` (`port/src/game/fighter.c:7053`; `PR_SEAM(0x24568u, side)`). It is entry 1 of the per-character table `0xA8628`; 11 blocks; all callees are rowed. Read its decomp and add cases for the side/entrance stream, the `0x246D4` target and the placement; add 3 mutants. Run `--function fighter_24568` (callee column unverified), then the full driver.

- [ ] **Step 2: Row `0x40CB0`**

The port function is `fighter_40cb0` (`port/src/game/fighter.c:7162`; `PR_SEAM(0x40CB0u, side)`), entry 0 of `0xA8628`; 11 blocks. Add the cases for the `0x40C34` target and the `0x3000`/`0x5000` placement; 3 mutants.

- [ ] **Step 3: Row `0x24804`**

The port function is `fighter_24804` (`port/src/game/fighter.c:7624`; `PR_SEAM(0x24804u, side)`), entry 6; 11 blocks; the `0x24754` call. Add the cases; 3 mutants.

- [ ] **Step 4: Verify, record, commit**

Run the full driver. Expected: `797/833` closed, `791/833` full, 36 open; the 8 actor-entry rows close. Record §P7; commit `diff: row the actor entrance entries`.

### Task 8: The menu/loop/memset rows (`0x2CB74`, `0x4F5C8`, `0x61A70`)

**Files:**
- Modify: `port/tests/diff_runner.c`, `tools/diff_verify.py`, the record

**Interfaces:**
- Produces: VERIFIED rows for `0x2CB74` (+2: `menu_step`, `menu_run`), `0x4F5C8` (+1: `game_loop`, which stays weaker), `0x61A70` (+1: `text_number_format`). Expected closed `797 → 801`, full `791 → 794`, open `36 → 32`; functions `+3`; mutants `+3` (re-measure).

- [ ] **Step 1: Row `0x2CB74`**

The port function is `svc_start_menu` (`port/src/game/svcmenu.c:78`; `PR_SEAM_RET(0x2CB74u, entry)`); 1 block; calls `0x2FFC4`. Add a case seeding the entry and recording the call; 1 mutant. Expected +2.

- [ ] **Step 2: Row `0x4F5C8`**

The port function is `flow_bonus_count_step` (`port/src/game/flow.c:2730`; `PR_SEAM0(0x4F5C8u)`); 4 blocks; 2 rowed callees. Add cases; 1 mutant. Expected `game_loop` closes but remains in `WEAKER`, so +1 closed, +0 full.

- [ ] **Step 3: Row `0x61A70` (binding only)**

The runtime `memset` is served by `mem_fill` (`port/src/...` has the `PR_SEAM(0x61A70u, …)` at `actors.c:4335`). Do **not** add a `port/src` header or `fn_register` (that would open 28 allow rows until the row is VERIFIED). Add a binding in `diff_runner.c` that calls `mem_fill` with the case's dest/fill/len and returns the raw's EAX; add 1 mutant. Add the `Spec("runtime_61a70", 0x61A70, …)`. Expected `text_number_format` closes; +1.

- [ ] **Step 4: Verify, record, commit**

Run the full driver. Expected: `801/833` closed, `794/833` full, 32 open. Record §P8; commit `diff: row the menu, loop and memset entries`.

### Task 9: Correct `0x3D0BC` and row the real `0x3A6D4` hook

**Files:**
- Modify: `tools/diff_verify.py` (the `fighter_state_3531c` `d15` case; the new `0x3A6D4` `Spec`)
- Modify: `port/tests/diff_runner.c` (binding/mutants)
- Modify: `docs/superpowers/plans/2026-10-22-pixel-and-proof-closure-derivations.md`

**Interfaces:**
- Produces: the `0x3D0BC` spec artifact corrected; a VERIFIED row for `0x3A6D4` (12 blocks; callees `0x188AC`, `0x188DC`, `0x2BC30`, `0x33A10` all rowed); `fighter_state_3531c` closes. Expected closed `801 → 802`, full `794 → 795`, open `32 → 31`; functions `+1`; mutants `+2` (re-measure).

- [ ] **Step 1: Prove the raw correction**

Read the raw bytes: `0x3D0BC` is `00 00 00 00`; the real entry at that neighbourhood is `0x3D0C0` (`52 89 c2 …`), which already has a row. The slot+0x10 hooks the setters store are `0x3A6D4` (`port/src/game/fighter.c:5692`; stored by `0x3A79C` at `0x3A7D2`) and `0x3A588` (`0x3A686`). Record the addresses and the `00 00 00 00` bytes.

- [ ] **Step 2: Fix the `d15` case**

In `tools/diff_verify.py`, the `fighter_state_3531c` spec's `d15` case pokes `0x3D0BC`; change it to the real `0x3A6D4` hook (or remove the synthetic case and let the new row close the parent). Do not invent a row for `0x3D0BC`; no such function exists.

- [ ] **Step 3: Row `0x3A6D4`**

Add a `Spec("fighter_3a6d4", 0x3A6D4, …)` with cases for phase 0, phase 1 (the `0xC9008` stream, the B/A globs and the snap), and phase > 1; add 2 mutants. The port function is already `fn_register`ed and carries the header; no new port code. Run `--function fighter_3a6d4`, then the full driver. Expected: `802/833` closed, `795/833` full, 31 open; `fighter_state_3531c` closes.

- [ ] **Step 4: Record and commit**

Record §P9 with the raw correction, the row, and the final line. Commit `fix: correct the 0x3D0BC spec to the real 0x3A6D4 hook`.

### Task 10: Make the unrowed counter measure the frontier

**Files:**
- Modify: `tools/diff_verify.py` (the counter/`UNROWED` computation)
- Modify: `tools/tests/test_diff_verify.py` (the new test; the pinned string is Task 11)
- Modify: `docs/superpowers/plans/2026-10-22-pixel-and-proof-closure-derivations.md`

**Interfaces:**
- Produces: a computed `ported_unrowed()` report with the portable set (the current 8) and the total `FN_` ported-unrowed population; a test that fails when a newly ported `FN_` function has no row and is not in the declared set. No closed-row change.

- [ ] **Step 1: Compute the frontier**

Add a function that computes the `FN_` addresses in `port/src/symbols.h` that the port tree implements (the same `fn_register`/column-0 header scan as `non_portable()`), minus the `SPECS` entries; split at `RUNTIME_BASE` (`0x5D000`) into the portable set and the runtime set. Compare the portable set to `UNROWED`; report both. The pre-phase figures are 8 in `UNROWED` and 42 computed `FN_` ported-unrowed addresses; the two sets overlap only partially (three `UNROWED` addresses have non-column-0 headers and are outside the scan's `ported` set), so report them separately and list the exceptions. The new rows reduce the computed total by the rows that were in it; re-measure.

- [ ] **Step 2: Add the guard test**

Add a `tools/tests/test_diff_verify.py` test that:
- asserts the computed portable set plus the named three-address header-shape exception equals `UNROWED`;
- asserts every `FN_` ported address below `0x5D000` is rowed or in `UNROWED`/the exception set;
- asserts every `FN_` ported address at/above `0x5D000` is rowed or in a named `RUNTIME_UNROWED`/declared set;
- fails with a readable message when a new `FN_` function is ported without a row.
The test must be able to fail: temporarily add a fake ported address to the computed set (or remove a row from a fixture) and see the assertion fail; revert.

- [ ] **Step 3: Update the printed figure**

Print both the portable and total figures in the counter line, e.g. `8 portable (N FN_ total)`. Keep the existing 8 figure visible; do not hide the runtime set. Record the new line.

- [ ] **Step 4: Run, record, commit**

Run `python3 -m unittest tools.tests.test_diff_verify` (the counter pin will still be stale; the new test alone should pass) and the full driver. Record §P10. Commit `diff: measure the ported-unrowed frontier`.

### Task 11: Re-pin the counter and update the documentation truth

**Files:**
- Modify: `tools/tests/test_diff_verify.py` (the pinned counter string)
- Modify: `README.md`, `AGENTS.md`, `docs/PROGRESS.md`
- Modify: `docs/superpowers/specs/2026-10-22-evidence-closeout-design.md` (status)
- Modify: `Makefile` (the U14 comment's `0x37FF2` → `0x37FF6`)
- Modify: `docs/superpowers/plans/2026-10-22-pixel-and-proof-closure-derivations.md`

**Interfaces:**
- Consumes: Task 5-10's measured `diffrun` output and the new unrowed figure.
- Produces: the exact pinned counter string, updated docs, and a passing `make diff-verify`.

- [ ] **Step 1: Re-measure**

Run the full driver command. Record the exact final line: functions, mutants, named gaps, rows with callees closed, full rows, and the new unrowed figure. Expected: at least `802/833` closed, `795/833` full, 31 open; the functions/mutants measured (not hand-set).

- [ ] **Step 2: Pin the string**

Update the `self.assertIn("diff-verify: ...")` string in `tools/tests/test_diff_verify.py` to the exact measured output, including the new unrowed wording. Do not change the closure-rule text.

- [ ] **Step 3: Update the docs**

- `README.md`: the differential paragraph and the portable/raw figures; confirm `python3 tools/port_progress.py` still prints `799 1203 66` / `759 759 100` (no new headers). The title stays 66%.
- `AGENTS.md`: the differential counter paragraph, the `59` open rows → `31`, the blocker table (remove the 12 rowed blockers; keep `0x65431`, the runtime/AIL, the host-owned rows, `0x65546`, `0x6C4CF`, `0x1C308`), the `774/833`/`768/833` figures, and the `8`/total unrowed text.
- `docs/PROGRESS.md`: append the phase paragraph (Task 12 can finish it); update the "Current state" counter lines if they are still the 2026-10-15 ones.
- `docs/superpowers/specs/2026-10-22-evidence-closeout-design.md`: change `Status: approved` to the implemented/closed status the phase narrative uses (one line; do not rewrite the design).
- `Makefile`: the U14 comment cites `0x37EA0 -> 0x37FF2`; the raw/`gp_win.py` use `0x37FF6`. Correct the comment only; the U14 pins stay empty.

- [ ] **Step 4: Verify and commit**

Run:
```bash
python3 -m unittest tools.tests.test_diff_verify
make diff-verify
```
Expected: both pass; the counter line matches the pinned string. Commit `docs: re-pin the differential counter and the phase truth`.

### Task 12: Full gate, record, and handoff

**Files:**
- Modify: `docs/superpowers/plans/2026-10-22-pixel-and-proof-closure-derivations.md`
- Modify: `docs/PROGRESS.md`

**Interfaces:**
- Consumes: all tasks.
- Produces: the phase record §P6, the PROGRESS paragraph, and a green full gate.

- [ ] **Step 1: Run the full gate**

Run:
```bash
PR_DIFF_FULLSCAN=1 make verify
```
Expected: all oracle lines pass, the differential counter is the Task 11 pin, `port_progress.py` is `799 1203 66` / `759 759 100`, `symbols.h` regenerates byte-identically, and `make entry-triage` passes. No `data/` file changes and no new capture. (The full scan is several times slower than the ~26-minute normal gate; run it once, at the end.)

- [ ] **Step 2: Write record §P6 and the PROGRESS paragraph**

Record the final U13 pins (with the failing-direction proofs), the differential counter and its row/mutant/open-row census, the 31 remaining open rows by blocker, the unrowed metric, the raw `0x3D0BC` correction, and the named limits (U14, U16 finisher, other endings, the post-script U13 tail, the runtime/host rows). Append the `docs/PROGRESS.md` paragraph: what changed, what did not move, and what remains.

- [ ] **Step 3: Final verification**

Re-run `make verify` if the full-scan run was clean but any doc/test changed after it; otherwise record the exact command and result. Confirm `git status` has only the intended files (plus the pre-existing untracked `.claude/settings.local.json`); do not commit or push unless the operator asks.

---

## Execution notes

- **Parallelism:** Track A's Tasks 1-4 are sequential (same capture). Track B's Tasks 5-9 are independent and can run on up to four agents; Task 10 follows them; Task 11 is the single serialized counter/doc update; Task 12 is the final gate. Do not let two agents edit `tools/diff_verify.py`, `port/tests/diff_runner.c` or `tools/tests/test_diff_verify.py` concurrently without a merge/rebase step; the safest split is one agent for Tasks 5-6, one for 7, one for 8-9, and one for Track A, with Task 10-12 serialized.
- **Row names:** keep the `Spec` names stable across tasks (`runtime_5d812`, `frontend_28d68`, `fighter_3e484`, `fighter_29d04`, `fighter_3e4c4`, `fighter_24568`, `fighter_24804`, `fighter_40cb0`, `svc_2cb74`, `flow_4f5c8`, `runtime_61a70`, `fighter_3a6d4`); the `--function` filter and the record use them.
- **If a row reveals a real port defect:** fix it with a failing-first test, keep the row's cases/mutants, and re-run the differential; a green row is not permission to stub the defect.
- **If the U13 4248 cannot be bound by a recipe:** keep it as a named gap and do not move the pin; the phase still delivers the differential half. Do not invent a 22,923-px class.
