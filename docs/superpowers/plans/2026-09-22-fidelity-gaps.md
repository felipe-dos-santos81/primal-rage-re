# Closing the small fidelity gaps Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Close the in-scope named fidelity gaps — the five unported `+0x52` handlers, `0x349C8`'s bit-6/7 deep callees, the loader flush scope, and the attract/scene palette drivers — moving no enforced oracle claim, and record every gap as closed or re-scoped with evidence.

**Architecture:** Derivation-first. Task 1 produces the authoritative record (`docs/superpowers/plans/2026-09-22-fidelity-gaps-derivations.md`); Tasks 2–5 each port one gap from it — one C function per original, address-tagged — gated by a raw-derived unit assertion with a mutation proof and the full ladder; Task 6 records the outcome and corrects the stale documentation.

**Tech Stack:** C over the flat `mem[]` (SDL3 port), CMake, the repo's `CHECK`/`CHECK_EQ_INT` suite, Ghidra (project `rage`, `/PRAGE.EXE`) for the raw, DOSBox-X for live RAM.

**Spec:** `docs/superpowers/specs/2026-09-22-fidelity-gaps-design.md`. Read it first — it is the contract.

## Global Constraints

- **No enforced oracle claim moves.** The gate is `make verify` exit 0 with: title `54 clean, 55 splice, 2 transition, 0 unexplained` and `54 clean, 57 splice, 0 unexplained`; attract `FIRST DIVERGENCE at capture frame 215`; front-end `[560..830]`/271 `0 unexplained`; smk `120/120` + `41/41`; `oracle C-vs-Python: 9866 writes byte-exact`; `symbols.h` regenerating byte-identically.
- **If a correct fix moves an enforced claim:** the task **halts** and reports the old value, the new value, and the raw evidence that the new one is more faithful. If justified, the claim is updated **in the same commit** with the reason recorded. The default is no move.
- **Never ship a fitted constant.** Every value is derived from the raw bytes or a capture, with the address that proves it. A value that cannot be pinned is a **named gap with its evidence**.
- **On any plan-vs-raw conflict the raw wins** — record the correction and its address.
- **One C function per original function**, header comment `/* 0xADDR — spec section */`. Deliberate deviations `/* PORT: ... */`; doubts `/* TODO(verify): ... */`. No other comment styles in `port/src`.
- Code addresses stored in data go through `fn_origin()` / `fn_resolve()`. SDL and file/asset I/O live only in `port/src/host.c` and `main.c`. **Never shadow original state in a long-lived C global.** `port/src/symbols.h` is generated — never hand-edit it.
- **0 warnings**, no new dependency, `data/` read-only. Never `git add -A`. Commit style: `<area>: <what changed>`.
- **Tests:** one `int test_X(void)` per file, registered once in `TEST_CASES` (`port/tests/test.h`); only `CHECK`/`CHECK_EQ_INT`; **assertions must be able to fail** — seed sentinels that differ from the post-condition and prove the new assertion fails under a mutation of the code it tests; `game_init()` may run only once per process (env-gate any test that calls it). Assertions live in the **existing per-area files** — no new `test_gaps.c`.
- **The size gate:** a group whose genuinely-new closure is **≥ ~4 KB or ≥ ~20 new functions** becomes a follow-on cycle — measured in Task 1, re-scoped **before** porting.
- **The branch:** `fidelity-gaps`, in-place (no worktree — a worktree lacks the git-ignored `data/`, so the oracles would skip silently). Run from the repo root.
- **Commit incrementally** — one coherent step per commit as soon as it builds and the suite passes. Sessions die; a death must cost at most the current step.

---

### Task 1: The derivation record

**Files:**
- Create: `docs/superpowers/plans/2026-09-22-fidelity-gaps-derivations.md`

**Interfaces:**
- Consumes: the cycle-3 record (`docs/superpowers/plans/2026-09-22-combat-fidelity-derivations.md`) for the address model, the closure-size method (§10.4), and the shape to follow.
- Produces: the record's sections — `§0` addressing/corrections, `§1` the five handlers, `§2` the deep callees, `§3` the loader flush scope, `§4` the attract/scene palette drivers, `§5` the 169-tick hold, `§6` the size-gate verdicts, `§7` the named gaps, `§8` the unit-test values, `§9` the provenance. **Tasks 2–5 implement from `§1`–`§4`; their exact addresses, values and test values come from here, not from this plan.**

- [ ] **Step 1: Derive the five unported `+0x52` handlers (§1)**

For each of `0x359E0` (state 1), `0x35C1C` + `0x35D20` (state 2), `0x37464` (state 8), `0x33B00` (state 19), `0x35E6C` (state 20): the full body (`ghidra_decompile_function`), its dispatch entry in `fight_health_sync` (`0x34B6C`, the jump table `0x34BF4`), its callees, its written fields (`+0x52`/`+0x53`/`+0x54`/`+0x5x`), and the port's current dispatch handling. Record the porting plan and the unit-test values (a seeded slot → the raw's post-state). A value that cannot be pinned is a named gap with its evidence.

- [ ] **Step 2: Derive the deep callees (§2)**

`0x36870`'s `0x385B0` and `0x37D18`'s `0x39A10`: the full bodies, the callers that reach them (`0x37178`'s arms, `0x36870`'s case structure), the callees they need, and their closure size. Record the porting plan and the test values.

- [ ] **Step 3: Re-derive the loader flush scope's porting plan (§3)**

From the cycle-3 record §4: the exact change (`platform/res.c`'s `res_load_present`, `platform/gfx.c`'s `gfx_flush_palette`/`palette_record`, `game/flow.c`'s ordering), what the raw shows faithful, and the assertion that proves the scope (which palettes survive the loader flush and which reach the gate's `0x25672` flush). State the measured oracle-neutrality (§4.3's 1381/1381) as context, not as the goal.

- [ ] **Step 4: Derive the attract/scene palette drivers (§4)**

`0x4F7F4` and `0x4F83C` (and `0x33874` if it is genuinely unported): the full bodies, the callers (`attract_scene_tick` `0x292AC`, ported at `attract.c:19`), the `DS_00104AD0` bit-0 protocol, the `DS_001088F1` counter, the `0x33874` enqueue into the `DS_00107798` list. **Resolve `0x4F83C`'s shape** — a distinct code-pointer target or a tail of `0x4F7F4` — and record which. Record the porting plan and the test values.

- [ ] **Step 5: Investigate the 169-tick static hold (§5)**

The port holds the front-end's last frame byte-static for 169 ticks (dump 312–480, loop 901–1069) before state 7 begins; the capture does not. Establish, from the port's frame trace and the raw: is this the load/stall model's expected behaviour (`RES_READ_BYTES_PER_TICK`, the master-loop gate) or a real divergence? Record the evidence and the verdict; if it is a divergence, name it as a gap with its owner.

- [ ] **Step 6: The attract-215 question (§4, the measurement note)**

Establish whether the attract oracle's first divergence at capture frame 215 is palette-related (what the capture shows at 215 vs the port; whether the `0x4F7F4`/`0x4F83C` animation is on that path). This decides what the drivers' port is measured against — it does not change the claim-move policy.

- [ ] **Step 7: The size-gate verdicts (§6) and the named gaps (§7)**

Apply the gate to each group: a group whose genuinely-new closure is **≥ ~4 KB or ≥ ~20 new functions** becomes a follow-on cycle. Use the cycle-3 method (§10.4) and report both the naive and the true-new figures (the "named in a comment" caveat). Then list the named gaps and the provenance (§9).

- [ ] **Step 8: Self-review and commit**

Check: every value cites its address; no porting code in the record; the size-gate verdicts are mechanical (the numbers, not a judgement); the porting plans are concrete enough for Tasks 2–5 to implement without re-deriving. Then:

```bash
git add docs/superpowers/plans/2026-09-22-fidelity-gaps-derivations.md
git commit -m "docs: derive the small-fidelity-gaps cycle"
```

**Gate for this task:** the record exists with `§0`–`§9`, every value address-backed, the three investigations resolved, and a size-gate verdict per group. **No porting code.**

---

### Task 2: The five unported `+0x52` handlers

**Files:**
- Modify: `port/src/game/fighter.c` (the handlers), `port/src/game/fight.c` (the dispatch, if the wiring lives there), `port/src/game/fighter.h` (declarations)
- Test: `port/tests/test_fight.c`

**Interfaces:**
- Consumes: record `§1` — each handler's address, body, written fields, and test values.
- Produces: `fighter_state_<addr>()` functions (the repo's naming) wired into the `+0x52` dispatch; the test's seeded assertions.

- [ ] **Step 1: Port the handlers from record §1**

For each of the five: one C function per original, header comment `/* 0xADDR — spec section */`, the body faithful to the record's decompilation, the written fields exactly as the raw writes them. Wire each into the dispatch's case (`fight_health_sync`). A field the raw writes that the record could not pin stays a `/* TODO(verify): ... */` with the evidence — never a fitted value.

- [ ] **Step 2: Write the failing assertions**

In `port/tests/test_fight.c`, for each handler: seed the slot's pre-state (a sentinel differing from the post-condition — never an unseeded BSS-zero), call the handler, assert the post-state the record pins. Run `cmake --build build && ./build/run_tests` — expect the new assertions to **fail** before the port is complete (if they pass, the assertion cannot distinguish the write from the seed — fix the seed).

- [ ] **Step 3: Make them pass and prove they can fail**

Build, run `./build/run_tests` → all pass. Then **mutate** the code each assertion tests (perturb the field the assertion reads) and show the exact failure line; revert. Record the mutation output in the task report.

- [ ] **Step 4: The ladder**

```bash
cmake --build build && ./build/run_tests && make verify
```

Expected: `all checks passed`; `make verify` exit 0 with every enforced claim unmoved; 0 warnings. If an enforced claim moved, **halt and report** per the Global Constraints.

- [ ] **Step 5: Commit**

```bash
git add port/src/game/fighter.c port/src/game/fighter.h port/src/game/fight.c port/tests/test_fight.c
git commit -m "fighter: port the five remaining +0x52 handlers"
```

**Gate for this task:** the five handlers faithful and dispatched; each assertion mutation-proven; the ladder green.

---

### Task 3: `0x349C8`'s bit-6/7 deep callees

**Files:**
- Modify: `port/src/game/fighter.c` / `port/src/game/fight.c` (wherever the callers live), `port/src/game/fighter.h`
- Test: `port/tests/test_fight.c`

**Interfaces:**
- Consumes: record `§2` — `0x385B0` and `0x39A10`'s bodies, callers, and test values.
- Produces: the ported callees wired into their call sites (the `0x37178` arms / `0x36870` structure), with the record's field writes.

- [ ] **Step 1: Port from record §2** — one C function per original, address-tagged, wired at the call site the record names.
- [ ] **Step 2: Write the failing assertions** — seed the pre-state (a sentinel differing from the post-condition), call, assert the record's post-state; run to see them fail.
- [ ] **Step 3: Make them pass and prove they can fail** — the mutation proof, as in Task 2.
- [ ] **Step 4: The ladder** — `./build/run_tests` + `make verify`; halt and report if a claim moved.
- [ ] **Step 5: Commit** — `git commit -m "fight: port the 0x349C8 deep callees"`.

**Gate for this task:** both callees faithful and wired; each assertion mutation-proven; the ladder green.

---

### Task 4: The loader flush scope

**Files:**
- Modify: `port/src/platform/res.c`, `port/src/platform/gfx.c`, `port/src/game/flow.c`
- Test: `port/tests/test_frontend.c`

**Interfaces:**
- Consumes: record `§3` — the exact change and the surviving-palette assertion.
- Produces: the loader flush scoped as the raw scopes it (the arena's records survive to the gate's flush), with the assertion proving the scope.

- [ ] **Step 1: Apply the scoped change from record §3** — exactly as far as the record shows the raw faithful, no further.
- [ ] **Step 2: Write the failing assertion** — drive the loader path and assert which palette records survive the flush (the record's values); run to see it fail against the unscoped port.
- [ ] **Step 3: Make it pass and prove it can fail** — the mutation proof (revert the scope → the assertion fails).
- [ ] **Step 4: The ladder** — `./build/run_tests` + `make verify`. The change is measured oracle-neutral; confirm that, and halt if any claim moves.
- [ ] **Step 5: Commit** — `git commit -m "res: scope the loader's palette flush to the raw's"`.

**Gate for this task:** the flush scoped faithfully; the assertion mutation-proven; the ladder green and the oracle-neutrality re-confirmed.

---

### Task 5: The attract/scene palette drivers

**Files:**
- Modify: `port/src/game/attract.c` (the drivers, and `attract_scene_tick`'s wiring), `port/src/game/attract.h` if needed
- Test: `port/tests/test_attract.c`

**Interfaces:**
- Consumes: record `§4` — the bodies, the `DS_00104AD0` bit-0 protocol, the `DS_001088F1` counter, `0x33874`'s enqueue, and `0x4F83C`'s resolved shape.
- Produces: the drivers ported (a distinct named C function for `0x4F83C` per the spec) and wired into `attract_scene_tick`; the assertions.

- [ ] **Step 1: Port the drivers from record §4** — `0x4F7F4`, `0x4F83C` (as the record resolves its shape — a distinct named function with its address tag), and `0x33874` if unported; wire them into the ported `attract_scene_tick` (`0x292AC`).
- [ ] **Step 2: Write the failing assertions** — seed the scene and the counter, tick the driver, assert the `DS_00104AD0` bit and the `0x33874` enqueue the record pins; run to see them fail.
- [ ] **Step 3: Make them pass and prove they can fail** — the mutation proof.
- [ ] **Step 4: The ladder** — `./build/run_tests` + `make verify`, including the attract oracle. **This is the gap most likely to touch the attract claim** (record `§4`'s 215 measurement): if `FIRST DIVERGENCE at capture frame 215` moves, halt and report per the Global Constraints.
- [ ] **Step 5: Commit** — `git commit -m "attract: port the scene palette drivers"`.

**Gate for this task:** the drivers faithful and wired; each assertion mutation-proven; the ladder green (or the claim-move policy invoked and recorded).

---

### Task 6: The record, the Gate, and the stale documentation

**Files:**
- Modify: `docs/superpowers/plans/2026-09-22-fidelity-gaps-derivations.md` (the outcome), `README.md`, `port/spec/game_flow.md`, `docs/superpowers/specs/2026-09-22-fidelity-gaps-design.md` (the Outcome)

**Interfaces:**
- Consumes: every task's outcome and the record's `§7` named gaps.
- Produces: the updated gap inventory — each gap closed or re-scoped with evidence — and the corrected stale claims.

- [ ] **Step 1: Update the gap inventory** — mark each in-scope gap closed (with its assertion and the ladder's result) or re-scoped with its evidence; carry forward the out-of-scope items (the freeze, the oracle's `res is None`, the `0x13xxx` callers, 831/832, the interactive match, the audio gaps) with their owners.
- [ ] **Step 2: Correct the three stale claims, each with its evidence** — `README.md:198-199` contradicting `:331` (the `0x13xxx` render path); the camera-chain deferral framing (`game_flow.md:327-341`, `README.md:283-284`) now that `port/src/game/camera.c` ports that chain; and `0x13B3C` described as deferred when it is dead (zero callers in the image).
- [ ] **Step 3: Write the Outcome** into `README.md`, `port/spec/game_flow.md` and the spec — the four gaps' outcomes, the hold's verdict, the size-gate results, and any claim the policy moved.
- [ ] **Step 4: The full ladder**

```bash
make verify
```

Expected: exit 0, 0 warnings, every enforced claim unmoved (or moved and recorded per the policy).

- [ ] **Step 5: Commit**

```bash
git add README.md port/spec/game_flow.md docs/superpowers/plans/2026-09-22-fidelity-gaps-derivations.md docs/superpowers/specs/2026-09-22-fidelity-gaps-design.md
git commit -m "docs: record the small-fidelity-gaps outcome"
```

**Gate for this task:** every gap closed or re-scoped with evidence; the three stale claims corrected; the Outcome written; the ladder green.

---

## Self-Review

**Spec coverage.** The spec's four in-scope gaps map to Tasks 2 (the five handlers), 3 (the deep callees), 4 (the loader flush scope), 5 (the attract palettes). Its Out list is Task 6 Step 1's carried-forward inventory (the freeze, the oracle, the `0x13xxx` callers, 831/832, the match, the audio). Its three Task 1 investigations are Task 1 Steps 5 (the hold), 4 (`0x4F83C`'s shape) and 6 (the attract-215 question). Its size gate is Task 1 Step 7. Its claim-move policy is every task's ladder step. Its representation rule for `0x4F83C` is Task 5 Step 1. Its Workflow is the Global Constraints' branch line. Its Verification is each task's assertion-and-mutation step plus the ladder.

**Placeholder scan.** No "TBD"/"implement later"/"similar to Task N". Tasks 2–5's code is not reproduced because it is derived in Task 1 — the record is the authoritative value source, and each step names the record section it implements from. Every step has its command.

**Type consistency.** The handler names (`fighter_state_<addr>`) follow the repo's convention, as do the test-file homes (`test_fight.c`, `test_frontend.c`, `test_attract.c`). `fight_health_sync` (`0x34B6C`) and `attract_scene_tick` (`0x292AC`) are the names cycle 3 and the port already use. `RES_READ_BYTES_PER_TICK` is the existing constant named in Task 1 Step 5.

**Right-sizing.** Task 1 is one deliverable with its own gate (the record, no porting). Tasks 2–5 each end at an independently verifiable deliverable (a gap ported, asserted, mutation-proven, ladder-green) — a reviewer can reject one without touching its neighbour. Task 6 is the record and the docs, which no porting task can own.
