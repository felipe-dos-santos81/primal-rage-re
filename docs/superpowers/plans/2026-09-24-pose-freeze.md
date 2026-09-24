# The pose/freeze subsystem Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Port the freeze's remaining layer — the unfreeze half (`0x140E4` + `0x170A0`) and the pose-entry half (`0x193B0` → `0x3B714` → `0x3AAFC` → the pose family) — so the demo's pose state runs and the demo oracle leaves its `res is None` fallback with the port's fight frames byte-matching the capture.

**Architecture:** Derivation-first. Task 1 produces the authoritative record; Tasks 2–4 each port one unit from it — one C function per original, address-tagged — gated by a raw-derived unit assertion with a mutation proof and the full ladder; Task 5 measures the observable and records the outcome.

**Tech Stack:** C over the flat `mem[]` (SDL3 port), CMake, the repo's `CHECK`/`CHECK_EQ_INT` suite, Ghidra (project `rage`, `/PRAGE.EXE`) for the raw.

**Spec:** `docs/superpowers/specs/2026-09-24-pose-freeze-design.md`. Read it first — it is the contract.

## Global Constraints

- **No enforced oracle claim moves.** The gate is `make verify` exit 0 with: title `54 clean, 55 splice, 2 transition, 0 unexplained` and `54 clean, 57 splice, 0 unexplained`; attract `FIRST DIVERGENCE at capture frame 215`; front-end `[560..830]`/271 `0 unexplained`; smk `120/120` + `41/41`; `oracle C-vs-Python: 9866 writes byte-exact`; `symbols.h` regenerating byte-identically.
- **If a correct fix moves an enforced claim:** the task **halts** and reports the old value, the new value, and the raw evidence that the new one is more faithful. If justified, the claim is updated **in the same commit** with the reason recorded. The default is no move.
- **Never ship a fitted constant.** Every value is derived from the raw bytes or a capture, with the address that proves it. A value that cannot be pinned is a **named gap with its evidence**.
- **On any plan-vs-raw conflict the raw wins** — record the correction and its address.
- **One C function per original function**, header comment `/* 0xADDR — spec section */`. Deliberate deviations `/* PORT: ... */`; doubts `/* TODO(verify): ... */`. No other comment styles in `port/src`.
- Code addresses stored in data go through `fn_origin()` / `fn_resolve()`. SDL and file/asset I/O live only in `port/src/host.c` and `main.c`. **Never shadow original state in a long-lived C global.** `port/src/symbols.h` is generated — never hand-edit it.
- **0 warnings**, no new dependency, `data/` read-only. Never `git add -A`. Commit style: `<area>: <what changed>`.
- **Tests:** one `int test_X(void)` per file, registered once in `TEST_CASES` (`port/tests/test.h`); only `CHECK`/`CHECK_EQ_INT`; **assertions must be able to fail** — seed sentinels that differ from the post-condition and prove the new assertion fails under a mutation of the code it tests; `game_init()` may run only once per process (env-gate any test that calls it). Assertions live in the **existing per-area files** — no new `test_gaps.c`.
- **The size gate:** a group whose genuinely-new closure is **≥ ~4 KB or ≥ ~20 new functions** becomes a follow-on cycle — measured in Task 1, re-scoped **before** porting. The union is already 68 f / 13 131 B, so the gate's purpose is to catch a *further* expansion (the winner gate, or a half that grows past the record's measurement).
- **The branch:** `pose-freeze`, in-place (no worktree — a worktree lacks the git-ignored `data/`, so the oracles would skip silently). Run from the repo root.
- **Commit incrementally** — one coherent step per commit as soon as it builds and the suite passes. Sessions die; a death must cost at most the current step.

---

### Task 1: The derivation record

**Files:**
- Create: `docs/superpowers/plans/2026-09-24-pose-freeze-derivations.md`

**Interfaces:**
- Consumes: the cycle-3 record (`docs/superpowers/plans/2026-09-22-combat-fidelity-derivations.md`) §10 for the freeze's owner chain and the closure method, and the cycle-3/fidelity-gaps records for the shape to follow.
- Produces: the record's sections — `§0` addressing/corrections, `§1` the unfreeze half, `§2` the pose-entry half, `§3` the winner gate, `§4` the `0x19020` no-op, `§5` the size-gate verdicts, `§6` the measurement plan, `§7` the named gaps, `§8` the unit-test values, `§9` the provenance. **Tasks 2–4 implement from `§1`–`§4`; their exact addresses, values and test values come from here, not from this plan.**

- [ ] **Step 1: Derive the unfreeze half (§1)**

For `0x140E4` (388 B, the box-overlap bool) and `0x170A0` (1 248 B, the `AF8` writer): the full body (`ghidra_decompile_function`), each callee (`0x14080`, `0x17EEC`, `0x1B544`, `0x15C30`, `0x181D0`, `0x16DA4`, `0x15F48`, `0x15FD4`, `0x61A70`, …) with its size and ported status, the written fields (`AF8`, `AFC`, `B54`, the `B00`/`B08`/`B0C`/`B10`/`B14`/`B20`/`B24`/`B28`/`B2C`/`B30`/`B34`/`B38`/`B40` screen-sync state), the caller chain (`0x26254` → `0x17580` at `0x17698`/`0x176AC`/`0x176BF`), and the port's current stub (`camera.c:373`). Record the porting plan and the unit-test values (a seeded `DS_00100B54`/`B60`/`B61` → the raw's post-`camera_decay` `AF8`/`AFC`). A value that cannot be pinned is a named gap with its evidence.

- [ ] **Step 2: Derive the pose-entry half (§2)**

For `0x193B0` (475 B, the winner's per-frame body), `0x3B714` (449 B), `0x3AAFC` (667 B, the reaction applier), and the pose family (`0x3A504`/`0x3A650`/`0x3A79C`/`0x3A8E8`, each 116 B): the full bodies, the callees they need (`0x33950`, `0x34D8C`, `0x1922C`, `0x3962C`, `0x396AC`, `0x18B44`, `0x39A10`, `0x19164`, `0x3C148`, `0x3C16C`, `0x192DC`, …) with ported status, the written fields (`slot+0x52`, `slot+0x54`, `slot+0x8A`, the record `+0x18`/`+0x1C`/`+0x43`/`+0x14`), the caller chain (`0x1958C`'s tail at `0x1974D`), and the port's current stub (`fighter.c:364-368`). Record the porting plan and the test values (a seeded winner flag + slot → the raw's pose state `+0x52 = 0x10`/`+0x54 = 0`, or whichever variant the raw selects).

- [ ] **Step 3: Resolve the winner gate (§3)**

`0x18950` (152 B, 0 callees): the full body, the table it reads (`PTR_DAT_000a1290`, indexed by the character bytes at `slot+0x2A`/`slot+0x0F`), and the demo's actual return for `(0,1)` and `(1,0)`. Establish from the raw whether the port's take-both-as-0 assumption at `fighter.c:328` changes which side's `AF8`/`AFC` survives — i.e. whether it gates the demo's winner. Record the verdict and, if in-scope, the porting plan and test values; if not, name it a gap with its evidence.

- [ ] **Step 4: Confirm the `0x19020` no-op (§4)**

From the raw, confirm `slot+0x18 == 0` on the demo's frames (`0x19032 CMP dword ptr [EAX+0x1077C8],0x0; 0x19039 JZ 0x19062`; `0x3FFDC`'s zero code xrefs), so `0x19020` writes nothing and the current stub (`fighter.c:299`) stays a named gap. Record the addresses.

- [ ] **Step 5: The size-gate verdicts (§5)**

Apply the gate to each group (the unfreeze half, the pose-entry half, the winner gate) using the cycle-3 method (record §10.4: `prage.calls.csv`/`prage.functions.csv`; "new" = not the `0x6xxxx` runtime, not the RNG, not the five stubs, and not named anywhere in `port/src` except `symbols.h`). Report both the naive and the true-new figures (the "named in a comment" caveat). A group over the gate becomes a follow-on cycle.

- [ ] **Step 6: The measurement plan (§6)**

State exactly how Task 5 establishes the observable: the `make demo-oracle` invocation, the port dump path (`/tmp/pr_frontend_dump/run1`), the 482/834 byte-diff method, the oracle's `res is None` fallback exit (`tools/title_compare.py:484`), and the record of the residual first-unexplained frame.

- [ ] **Step 7: The named gaps (§7), the unit-test values (§8), the provenance (§9)**

The gap inventory — each closed, re-scoped or carried with its owner (the residual divergence if any; `0x19020`; `0x38154`; the `0x3Fxxx` script; 831/832; the `0x3Fxxx`-vs-gate concern). The unit-test substitution table for Tasks 2–4. The provenance (the Ghidra calls per section).

- [ ] **Step 8: Self-review and commit**

Check: every value cites its address; no porting code in the record; the size-gate verdicts are mechanical (the numbers, not a judgement); the porting plans are concrete enough for Tasks 2–4 to implement without re-deriving. Then:

```bash
git add docs/superpowers/plans/2026-09-24-pose-freeze-derivations.md
git commit -m "docs: derive the pose/freeze cycle"
```

**Gate for this task:** the record exists with `§0`–`§9`, every value address-backed, the winner gate resolved, the `0x19020` no-op confirmed, and a size-gate verdict per group. **No porting code.**

---

### Task 2: The unfreeze half

**Files:**
- Modify: `port/src/game/camera.c` (the ported functions, and the `camera_decay` stub at `:373`), `port/src/game/camera.h` if declarations move
- Test: `port/tests/test_fight.c` (`check_decay`, `:319`)

**Interfaces:**
- Consumes: record `§1` — `0x140E4`/`0x170A0`'s bodies, callees, written fields, and test values.
- Produces: `0x140E4` and `0x170A0` as address-tagged C functions (the repo's naming), wired into `camera_decay`; the test's seeded assertions.

- [ ] **Step 1: Port the unfreeze half from record §1**

For `0x140E4` and `0x170A0` and each new callee: one C function per original, header comment `/* 0xADDR — spec section */`, the body faithful to the record's decompilation, the written fields exactly as the raw writes them. Wire the calls into `camera_decay` at `camera.c:373` in the raw's order (`0x140E4` bool gate → `0x170A0(0)`/`0x170A0(1)`). A field the raw writes that the record could not pin stays a `/* TODO(verify): ... */` with the evidence — never a fitted value.

- [ ] **Step 2: Write the failing assertions**

In `port/tests/test_fight.c`'s `check_decay` (or a sibling `check_*` it calls): seed `DS_00100B54`/`DS_00100B60`/`DS_00100B61` and the screen-sync state so the raw's guards pass, call `camera_decay`, assert the raw's post-state (`AF8`/`AFC` per record `§1`). Seed sentinels that differ from the post-condition — never an unseeded BSS-zero. Run `cmake --build build && ./build/run_tests` — expect the new assertions to **fail** before the port is complete (if they pass, the seed cannot distinguish the write — fix it). **Note:** `check_decay`'s existing `AF8 == 0`/`AFC == 0` assertions (`test_fight.c:336-337`) may legitimately move once the tail runs; if so, update them with the raw evidence in the same commit (a behaviour change, not a consolidation).

- [ ] **Step 3: Make them pass and prove they can fail**

Build, run `./build/run_tests` → all pass. Then **mutate** the code each assertion tests (perturb the field the assertion reads) and show the exact failure line; revert. Record the mutation output in the task report.

- [ ] **Step 4: The ladder**

```bash
cmake --build build && ./build/run_tests && make verify
```

Expected: `all checks passed`; `make verify` exit 0 with every enforced claim unmoved; 0 warnings. If an enforced claim moved, **halt and report** per the Global Constraints.

- [ ] **Step 5: Commit**

```bash
git add port/src/game/camera.c port/src/game/camera.h port/tests/test_fight.c
git commit -m "camera: port the unfreeze half (0x140E4/0x170A0)"
```

**Gate for this task:** the unfreeze half faithful and wired; each assertion mutation-proven; the ladder green.

---

### Task 3: The pose-entry chain

**Files:**
- Modify: `port/src/game/fighter.c` (the ported functions, and the tail stub at `:364-368`), `port/src/game/fighter.h` if declarations are needed
- Test: `port/tests/test_fight.c`

**Interfaces:**
- Consumes: record `§2` — `0x193B0`/`0x3B714`/`0x3AAFC`/the pose family's bodies, callees, and test values.
- Produces: the pose-entry functions as address-tagged C functions, wired into `fighter_pass_a`'s tail at `fighter.c:364`; the test's seeded assertions.

- [ ] **Step 1: Port the pose-entry chain from record §2**

For `0x193B0`, `0x3B714`, `0x3AAFC`, the pose family, and each new callee: one C function per original, address-tagged, faithful to the record. Wire `0x193B0(0)`/`0x193B0(1)` into the tail at `fighter.c:364-368` (the raw's `0x1974D` call). An unpinned field stays a `/* TODO(verify): ... */`.

- [ ] **Step 2: Write the failing assertions**

In `port/tests/test_fight.c`: seed the winner flag (`DS_00100AF8`/`AFC` non-zero, `DSB(0x10783A)`/`0x1078CE` non-zero) and the slot's pre-state, call `fighter_pass_a`, assert the raw's pose state (`slot+0x52`/`+0x54`/`+0x8A` per record `§2`). Seed a sentinel differing from the post-condition; run to see it fail.

- [ ] **Step 3: Make them pass and prove they can fail** — the mutation proof, as in Task 2.

- [ ] **Step 4: The ladder** — `./build/run_tests` + `make verify`; halt and report if an enforced claim moved.

- [ ] **Step 5: Commit** — `git commit -m "fighter: port the pose-entry chain (0x193B0/0x3B714/0x3AAFC)"`.

**Gate for this task:** the pose-entry chain faithful and wired; each assertion mutation-proven; the ladder green.

---

### Task 4: The winner gate

**Files:**
- Modify: `port/src/game/fighter.c` (the `0x18950` stub at `:328`), `port/src/game/fighter.h` if needed
- Test: `port/tests/test_fight.c`

**Interfaces:**
- Consumes: record `§3` — `0x18950`'s body, table, and the demo's return values; **if the record finds it does not gate the demo, this task is dropped** and the stub stays a named gap.
- Produces: `0x18950` as an address-tagged C function, wired at `fighter.c:328`; the test's seeded assertion.

- [ ] **Step 1: Port `0x18950` from record §3** — one C function per original, address-tagged, wired at the two call sites (`0x19653`/`0x19661`), replacing the take-both-as-0 assumption. Port any other stub the record names as gating the winner.
- [ ] **Step 2: Write the failing assertion** — seed the character table and the two slots, call `0x18950`, assert the record's return; run to see it fail against the stub.
- [ ] **Step 3: Make it pass and prove it can fail** — the mutation proof.
- [ ] **Step 4: The ladder** — `./build/run_tests` + `make verify`; halt and report if an enforced claim moved.
- [ ] **Step 5: Commit** — `git commit -m "fighter: port the 0x18950 reachability query"`.

**Gate for this task:** `0x18950` faithful and wired; the assertion mutation-proven; the ladder green. (Or, per record §3, the task is dropped and the gap carried.)

---

### Task 5: The demo measurement, the record and the docs

**Files:**
- Modify: `docs/superpowers/plans/2026-09-24-pose-freeze-derivations.md` (the outcome), `README.md`, `port/spec/game_flow.md`, `docs/superpowers/specs/2026-09-24-pose-freeze-design.md` (the Outcome)

**Interfaces:**
- Consumes: every task's outcome and the record's `§7` named gaps.
- Produces: the measured observable, the updated gap inventory, and the corrected stale claims.

- [ ] **Step 1: Measure the observable**

```bash
make demo-oracle
```

Expected: the demo oracle leaves the `res is None` fallback (`tools/title_compare.py:484`) and reports a real clean/splice/unexplained breakdown. Then the byte-diff witness (port 482 ↔ capture 834; before: 18 294 B, entirely the two fighters) and the residual first-unexplained frame. Record both in the task report.

- [ ] **Step 2: Update the gap inventory** — mark each in-scope gap closed (with its assertion and the ladder's result) or re-scoped with its evidence; carry forward the out-of-scope items (`0x19020`, `0x38154`, the `0x3Fxxx` script, 831/832, the interactive match, the audio gaps) with their owners. If the observable did not move as far as the freeze, name the residual divergence with its owner.

- [ ] **Step 3: Write the Outcome** into `README.md`, `port/spec/game_flow.md` and the spec — the two halves' outcomes, the winner-gate verdict, the measured observable, the size-gate results, and any claim the policy moved.

- [ ] **Step 4: The full ladder**

```bash
make verify
```

Expected: exit 0, 0 warnings, every enforced claim unmoved (or moved and recorded per the policy).

- [ ] **Step 5: Commit**

```bash
git add README.md port/spec/game_flow.md docs/superpowers/plans/2026-09-24-pose-freeze-derivations.md docs/superpowers/specs/2026-09-24-pose-freeze-design.md
git commit -m "docs: record the pose/freeze outcome"
```

**Gate for this task:** the observable measured and recorded; every gap closed or re-scoped with evidence; the Outcome written; the ladder green.

---

## Self-Review

**Spec coverage.** The spec's two halves map to Tasks 2 (unfreeze) and 3 (pose-entry); its winner-gate clause to Task 4 (conditional on record §3); its three Task-1 resolutions to Task 1 Steps 3 (`0x18950`), 4 (`0x19020`) and 6 (the measurement plan); its acceptance point 2 to Task 5 Step 1; its size gate to Task 1 Step 5; its claim-move policy to every task's ladder step; its Verification to each task's assertion-and-mutation step plus the ladder; its Workflow to the Global Constraints' branch line.

**Placeholder scan.** No "TBD"/"implement later"/"similar to Task N". Tasks 2–4's code is not reproduced because it is derived in Task 1 — the record is the authoritative value source, and each step names the record section it implements from. Every step has its command.

**Type consistency.** The test-file homes match the repo's existing ones (`test_fight.c` already tests `camera_decay` at `:319`, `fighter_pass_a` and the winner flags). `camera_decay` (`0x17580`), `fighter_pass_a` (`0x1958C`), `DS_00100AF8`/`AFC`/`B54` and `DSB(0x10783A)`/`0x1078CE` are the names the port and the records already use.

**Right-sizing.** Task 1 is one deliverable with its own gate (the record, no porting). Tasks 2–4 each end at an independently verifiable deliverable (a half or stub ported, asserted, mutation-proven, ladder-green) — a reviewer can reject one without touching its neighbour. Task 5 is the measurement and the docs, which no porting task can own.
