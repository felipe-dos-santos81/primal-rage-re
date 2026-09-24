# The demo arena backdrop Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Close the demo's state-7 arena render gap so port frame 482 byte-matches capture 834 (0 / 192 000 RGB bytes).

**Architecture:** Derivation-first. Task 1 produces the authoritative record — it pins the missing horizon layer by runtime differential (a temporary, reverted port dump vs the original's live RAM under dosbox-x) plus Ghidra, and applies the size gate; Task 2 ports each piece from it — one C function per original, address-tagged — gated by a raw-derived unit assertion with a mutation proof and the full ladder; Task 3 re-measures the byte-exact and records the outcome.

**Tech Stack:** C over the flat `mem[]` (SDL3 port), CMake, the repo's `CHECK`/`CHECK_EQ_INT` suite, Ghidra (project `rage`, `/PRAGE.EXE`) and dosbox-x for the raw.

**Spec:** `docs/superpowers/specs/2026-09-24-arena-backdrop-design.md`. Read it first — it is the contract.

## Global Constraints

- **The cycle's own gate:** port frame 482 ↔ capture 834 byte-exact (0 / 192 000 RGB bytes, 0 px). Reported before and after. Before: **18 294 B (9.5 %) / 6 194 px**.
- **No enforced oracle claim moves.** The gate is `make verify` exit 0 with: title `54 clean, 55 splice, 2 transition, 0 unexplained` and `54 clean, 57 splice, 0 unexplained`; attract `FIRST DIVERGENCE at capture frame 215`; front-end `[560..830]`/271 `0 unexplained`; smk `120/120` + `41/41`; `oracle C-vs-Python: 9866 writes byte-exact`; `symbols.h` regenerating byte-identically.
- **If a correct fix moves an enforced claim:** the task **halts** and reports the old value, the new value, and the raw evidence that the new one is more faithful. If justified, the claim is updated **in the same commit** with the reason recorded. The default is no move.
- **Never ship a fitted constant.** Every value is derived from the raw bytes or a capture, with the address that proves it. A value that cannot be pinned is a **named gap with its evidence**.
- **On any plan-vs-raw conflict the raw wins** — record the correction and its address.
- **One C function per original function**, header comment `/* 0xADDR — spec section */`. Deliberate deviations `/* PORT: ... */`; doubts `/* TODO(verify): ... */`. No other comment styles in `port/src`.
- Code addresses stored in data go through `fn_origin()` / `fn_resolve()`. SDL and file/asset I/O live only in `port/src/host.c` and `main.c`. **Never shadow original state in a long-lived C global.** `port/src/symbols.h` is generated — never hand-edit it.
- **0 warnings**, no new dependency, `data/` read-only. Never `git add -A`. Commit style: `<area>: <what changed>`.
- **Tests:** one `int test_X(void)` per file, registered once in `TEST_CASES` (`port/tests/test.h`); only `CHECK`/`CHECK_EQ_INT`; **assertions must be able to fail** — seed sentinels that differ from the post-condition and prove the new assertion fails under a mutation of the code it tests; `game_init()` may run only once per process (env-gate any test that calls it). Assertions live in the **existing per-area files** (`test_fight.c`, `test_render.c`) — no new test file.
- **The size gate:** a group whose genuinely-new closure is **≥ ~4 KB or ≥ ~20 new functions** becomes a follow-on cycle — measured in Task 1, re-scoped **before** porting. If it triggers, the cycle stops after Task 1 and names the follow-on; the derivation is still the cycle's deliverable.
- **The branch:** `arena-backdrop`, in-place (no worktree — a worktree lacks the git-ignored `data/`, so the oracles would skip silently). Run from the repo root.
- **Commit incrementally** — one coherent step per commit as soon as it builds and the suite passes. Sessions die; a death must cost at most the current step.

---

### Task 1: The derivation record

**Files:**
- Create: `docs/superpowers/plans/2026-09-24-arena-backdrop-derivations.md`
- Temporarily modify (then revert): `port/src/platform/render.c` (a `getenv`-gated render-list/actor dump)

**Interfaces:**
- Consumes: the cycle-4 record (`docs/superpowers/plans/2026-09-24-pose-freeze-derivations.md`) §11.4 for the residual and its owner; the combat-fidelity record (`2026-09-22-combat-fidelity-derivations.md`) §1.2 for the arena-backdrop handle `0x0A838B44`; the demo-fight-closure record (`2026-09-21-demo-fight-closure-derivations.md`) §9.4/§9.5 for the props and the presentation.
- Produces: the record's sections — `§0` addressing/corrections (including the "never renders the backdrop" correction), `§1` the runtime differential, `§2` the missing layer's owner chain, `§3` the porting plan, `§4` the size-gate verdicts, `§5` the measurement plan, `§6` the named gaps, `§7` the unit-test values, `§8` the provenance. **Task 2 implements from `§2`–`§3`; its exact addresses, values and test values come from here, not from this plan.**

- [ ] **Step 1: The runtime differential (§1)**

Instrument the port with a temporary, `getenv`-gated dump of `render_list`'s nodes (the layer, the pset, the sprite id, the palette, px/py, x/y, W/H) and the scene-actor spawns, run the demo dump, and capture the arena's first frames. The baseline measured during the design: at frame 482 the list carries layer 2 = sky (sprite id 11232, 549×213 at y = 0) and layer 1 = sea (id 11233, 975×64 at y = 137), scene index 0, and the two scene-actor descriptors are `0xBDE50`/`0xBDEF0`. Compare against the original's live RAM under dosbox-x (the same render-list pool `DS_0010153C`, head `DS_00105B44`, at the same state) so the absent or wrong entry is isolated **by measurement**. Revert the instrumentation before the task's commit; record the dump and the comparison in the record.

- [ ] **Step 2: The missing layer's owner chain (§2)**

From the differential, derive the missing layer's owner: the actor/pset that should carry it, the animation stream that selects its sprite (`actor_spawn`'s `desc[0]` → `rec+0x08`), the frame field, and the draw path (`render_list`'s layer 1/2 handling; the projection words `DS_00107A38`/`3A`/`3E`/`48`/`4E`). Use `ghidra_decompile_function` / `ghidra_disassemble_function` and the port's existing code; record each function's body, written fields, and ported status. If the cause is a data/sprite divergence rather than a missing draw, record that and the owner (the animation/resolve path) — the fix lands where the invariant belongs.

- [ ] **Step 3: The porting plan (§3)**

For each piece: the raw addresses, the port's home file, the exact wiring site (the raw's call site), and the unit-test values (a seeded state → the raw's post-state). A value that cannot be pinned is a named gap with its evidence.

- [ ] **Step 4: The size-gate verdicts (§4)**

Apply the gate (≥ ~4 KB or ≥ ~20 new functions) to the closure, using the cycle-3 method (record §10.4: `prage.calls.csv`/`prage.functions.csv`; "new" = not the `0x6xxxx` runtime, not the RNG, not the five stubs, and not named anywhere in `port/src` except `symbols.h`). Report both the naive and the true-new figures. **If the gate triggers, the cycle stops after Task 1**: record the verdict and name the follow-on — do not proceed to Task 2.

- [ ] **Step 5: The measurement plan (§5)**

State exactly how Task 3 establishes the byte-exact: the `make demo-oracle` invocation, the port dump path (`/tmp/pr_frontend_dump/run1`), the 482/834 per-pixel compare:

```bash
python3 -c "a=open('/tmp/pr_frontend_dump/run1/frame_0482.raw','rb').read(); b=open('data/title-captures/frontend/frame_0834.raw','rb').read(); d=[i for i in range(len(a)) if a[i]!=b[i]]; print(len(d),'bytes',len({i//3 for i in d}),'px')"
```

and the oracle's fallback exit (`tools/title_compare.py:484`).

- [ ] **Step 6: The named gaps (§6), the unit-test values (§7), the provenance (§8)**

The gap inventory — each closed, re-scoped or carried with its owner (the residual if any; the state-9 hold's animation; the interactive match; the audio gaps; 831/832). The unit-test substitution table for Task 2. The provenance (the Ghidra calls and the dosbox-x runs per section).

- [ ] **Step 7: Self-review and commit**

Check: every value cites its address; no porting code in the record; the size-gate verdicts are mechanical (the numbers, not a judgement); the porting plan is concrete enough for Task 2 to implement without re-deriving; the instrumentation is reverted (`git status --short` clean except the record). Then:

```bash
git add docs/superpowers/plans/2026-09-24-arena-backdrop-derivations.md
git commit -m "docs: derive the arena-backdrop cycle"
```

**Gate for this task:** the record exists with `§0`–`§8`, every value address-backed, the differential measured (not assumed), a size-gate verdict, and the instrumentation reverted. **No porting code.**

---

### Task 2: The port

**Files:**
- Modify: the record's `§3` home file(s) (`port/src/platform/render.c`, `port/src/game/actors.c`, or wherever the record's seam lands)
- Test: `port/tests/test_fight.c` and/or `port/tests/test_render.c` (the record's `§7` values)

**Interfaces:**
- Consumes: record `§2`–`§3` — the missing layer's owner chain, the porting plan, and the test values.
- Produces: the ported function(s) as address-tagged C functions, wired at the raw's call site; the test's seeded assertions.

**Note:** the exact file paths, function names and test values are Task 1's `§3`/`§7` output — this task is written before the derivation, so they are deliberately not guessed here. Task 1 Step 7's self-review checks the porting plan is concrete enough that they are determined. If the record names more than one independently-verifiable piece, split this into one task per piece (each with its own assertion, mutation proof, ladder and commit) — a reviewer must be able to reject one without touching its neighbour.

- [ ] **Step 1: Port from record §3**

For each piece and each new callee: one C function per original, header comment `/* 0xADDR — spec section */`, the body faithful to the record's decompilation, the written fields exactly as the raw writes them, wired at the raw's call site. A field the raw writes that the record could not pin stays a `/* TODO(verify): ... */` with the evidence — never a fitted value.

- [ ] **Step 2: Write the failing assertions**

In the existing per-area file: seed the record's `§7` pre-state (sentinels that differ from the post-condition — never an unseeded BSS-zero), call the ported function, assert the raw's post-state. Run `cmake --build build && ./build/run_tests` — expect the new assertions to **fail** before the port is complete (if they pass, the seed cannot distinguish the write — fix it).

- [ ] **Step 3: Make them pass and prove they can fail**

Build, run `./build/run_tests` → all pass. Then **mutate** the code each assertion tests and show the exact failure line; revert. Record the mutation output in the task report.

- [ ] **Step 4: The byte-exact check**

Re-run the 482/834 per-pixel compare from Task 1's `§5`. The diff must fall (ideally to 0). Record the new number. If it did not move, the port is not the missing layer — report and re-derive (do not ship a no-op).

- [ ] **Step 5: The ladder**

```bash
cmake --build build && ./build/run_tests && make verify
```

Expected: `all checks passed`; `make verify` exit 0 with every enforced claim unmoved; 0 warnings. If an enforced claim moved, **halt and report** per the Global Constraints.

- [ ] **Step 6: Commit**

```bash
git add port/src/... port/tests/...
git commit -m "<area>: port the arena backdrop layer (0xADDR)"
```

**Gate for this task:** the piece faithful and wired; each assertion mutation-proven; the 482/834 diff measured (and reduced); the ladder green.

---

### Task 3: The outcome and the docs

**Files:**
- Modify: `docs/superpowers/plans/2026-09-24-arena-backdrop-derivations.md` (the outcome), `README.md`, `port/spec/game_flow.md`, `docs/superpowers/specs/2026-09-24-arena-backdrop-design.md` (the Outcome)

**Interfaces:**
- Consumes: every task's outcome and the record's `§6` named gaps.
- Produces: the measured byte-exact, the updated gap inventory, and the corrected stale claims.

- [ ] **Step 1: Measure the byte-exact**

```bash
make demo-oracle
```

Then the 482/834 per-pixel compare (before: 18 294 B / 6 194 px). Record the after number and the oracle's report (whether it leaves the fallback — a consequence, not a gate). Record both in the task report.

- [ ] **Step 2: Update the gap inventory** — mark each in-scope gap closed (with its assertion and the ladder's result) or re-scoped with its evidence; carry forward the out-of-scope items (the state-9 hold, the interactive match, the audio gaps, 831/832) with their owners. If the byte-exact is not reached, name the residual divergence with its owner.

- [ ] **Step 3: Write the Outcome** into `README.md`, `port/spec/game_flow.md` and the spec — the layer's outcome, the measured byte-exact, the size-gate result, and any claim the policy moved.

- [ ] **Step 4: The full ladder**

```bash
make verify
```

Expected: exit 0, 0 warnings, every enforced claim unmoved (or moved and recorded per the policy).

- [ ] **Step 5: Commit**

```bash
git add README.md port/spec/game_flow.md docs/superpowers/plans/2026-09-24-arena-backdrop-derivations.md docs/superpowers/specs/2026-09-24-arena-backdrop-design.md
git commit -m "docs: record the arena-backdrop outcome"
```

**Gate for this task:** the byte-exact measured and recorded; every gap closed or re-scoped with evidence; the Outcome written; the ladder green.

---

## Self-Review

**Spec coverage.** The spec's Goal and Acceptance 1 (byte-exact at 482/834) map to Task 2 Step 4 and Task 3 Step 1; its Evidence section to Task 1 Steps 1–2 (and the correction to `§0`); its scope and size gate to Task 1 Step 4; its architecture to the three tasks; its Verification to each task's assertion-and-mutation step plus the ladder; its Workflow to the Global Constraints' branch line; its Risks to Task 1's "if the gate triggers, stop" and Task 2 Step 4's "if the diff did not move, re-derive".

**Placeholder scan.** No "TBD"/"implement later"/"similar to Task N". Task 2's code is not reproduced because it is derived in Task 1 — the record is the authoritative value source, and each step names the record section it implements from. Every step has its command.

**Type consistency.** The known names are the ones the port and the records already use: `render_list` (`0x14328`), `render_scroll_setup` (`0x38730`), `render_scroll_scene_a`/`b` (`0x387F4`/`0x38890`), `render_scroll_fill` (`0x38A38`), `actor_spawn` (`0x2AE14`), `fight_scene_props` (`0x412A0`/`0x2C320`), the render-list pool `DS_0010153C`/head `DS_00105B44`, the projection words `DS_00107A38`/`3A`/`3E`/`48`/`4E`, the scene tables `0xBDE1C`/`0xBDE2C`/`0xBDE0C`/`0xBDDFC` and the descriptor tables `0xBDF7C`/`0xBDF9C`. The test homes match the repo's existing ones.

**Right-sizing.** Task 1 is one deliverable with its own gate (the record, no porting) and can legitimately end the cycle (the size gate). Task 2 is the port — split per independently-verifiable piece when the record names more than one. Task 3 is the measurement and the docs, which no porting task can own.
