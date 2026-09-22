# Combat/Render Fidelity (Cycle 3) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Close the attract demo fight's three residual fidelity gaps — the palette acquisition order, the twelve unported `0x34B14` combat handlers, and the fighter animation poses — so the fight runs to its 900-frame timer exit and the arena renders in the DAC banks the raw assigns.

**Architecture:** A derivation-first cycle in the shape of cycles 1 and 2. Task 1 produces the authoritative derivation record; Tasks 2–4 port from it in the order render → behaviour → poses; Task 5 measures the Gate and records the outcome honestly. No new modules unless the derivation proves one exists; each piece extends its existing owner.

**Tech Stack:** C (SDL3), CMake, Ghidra 12.2_DEV headless + Ghidra MCP (project `rage`, program `/PRAGE.EXE`), dosbox-x for runtime ground truth, Python 3 (`capstone`, `Pillow`) for RE tooling.

**Spec:** `docs/superpowers/specs/2026-09-22-combat-fidelity-design.md`. Read it first.

## Global Constraints

- **Run the binaries from the repo root.** Tests default to the relative `data/game/C` (`PR_GAME_DIR` overrides it).
- **Address model:** Ghidra address == linear address == object base + offset. Code object `0x10000`–`0x73B14`; data object `0x80000`–`0x10B0CF`. Raw-file disassembly is **pre-fixup** — use Ghidra for any data address. A global Ghidra calls `DAT_0008xxxx` is at DS offset `0xxxx` (subtract `0x80000`).
- **One C function per original function**, header comment `/* 0xADDR — spec section */`.
- Comments in `port/src` are only `/* PORT: ... */`, `/* TODO(verify): */`, and address tags. **No new comment styles in `port/tests`.**
- **All original state lives in `mem[]`** at its original linear address, accessed as `DSB/DSW/DSD(addr)`. Never shadow original state in a long-lived C global.
- SDL and file/asset I/O live **only** in `port/src/host.c` and `main.c`.
- Code addresses stored in data go through `fn_origin()` / `fn_resolve()`.
- **Never ship a fitted constant.** Every value is derived from the raw bytes or a capture, with the address that proves it. A value that cannot be pinned is a **named gap with its evidence**.
- **On any plan-vs-raw conflict the raw wins.** Record the correction and its address, in the record and the report.
- **Assertions must be able to fail.** Seed sentinels that differ from the post-conditions; prove each new assertion fails under a mutation of the code it tests; never assert an unseeded BSS-zero.
- **`game_init()` may run only once per process.** Any test calling it must be env-gated, and `run_tests.c` must run that driver alone.
- One `int test_X(void)` per file, declared in `port/tests/test.h`, called from `run_tests.c`, and the `.c` added to `port/CMakeLists.txt`'s **explicit** source list (no globbing). Only `CHECK(cond,msg)` and `CHECK_EQ_INT(a,b)`.
- **0 warnings.** No new dependencies. `data/` is read-only. **Never `git add -A`.** Commit style: `<area>: <what changed>`.
- **The oracle's invariant is its CLAIM, not its window indices.** Claims that must not move: title `54 clean, 55 splice, 2 transition, 0 unexplained` and `54 clean, 57 splice, 0 unexplained`; attract `FIRST DIVERGENCE at capture frame 215`; front-end `0 unexplained`; smk `120/120` + `41/41`; the C-vs-Python byte-exact claim (`9866 writes`).
- `make demo-oracle` is **report-only** (not in `verify`). `make verify` is the ladder.
- **Commit incrementally** — one coherent step as soon as it builds and its tests pass. Implementer sessions die; a death must cost at most the current step.

---

### Task 1: The derivation record

Produce `docs/superpowers/plans/2026-09-22-combat-fidelity-derivations.md`, the authoritative value source for Tasks 2–4. **No porting code in this task.** Every value cites its address; a value that cannot be pinned is a named gap with its evidence.

**Files:**
- Create: `docs/superpowers/plans/2026-09-22-combat-fidelity-derivations.md`

**Interfaces:**
- Consumes: `docs/superpowers/plans/2026-09-21-demo-fight-closure-derivations.md` §9.5/§9.6 (the palette/DAC gap) and `docs/superpowers/plans/2026-09-20-demo-fight-derivations.md` §7.10/§11.3 (the `0x34B14` table); the cycle-2 reports for the measured residuals.
- Produces: §1 the palette acquisition sequence; §2 the twelve `0x34B14` handlers and their size; §3 the pose selector; §4 the loader flush scope's separability; §5 the RNG question; §6 the named gaps; §7 the unit-test values.

- [ ] **Step 1: Derive the palette acquisition sequence**

Read `palette_acquire` (`0x33754`) and its **call sites** in Ghidra: from `0x2BAF4` (the scene-2 reset), `0x2C06C`, the actor spawn path (`0x2A1EA`/`0x2A1EF`), the fight's pset sync, and `0x110D8`..`0x11129` (the attract). Record the original's **order** of `palette_acquire` calls and the DAC range each assigns.

Then read the port's order: `rg -n "palette_acquire" port/src` (`attract.c:168-175`, `render.c:285-288`, `actors.c:1186`). Record the port's order and the range each assigns.

State the **first call at which the sequences diverge** and the range `0x1BB9FD58` receives in each. The port's `palette_acquire` body (`actors.c:251-285`) is a faithful port of the search/reflow (`start = prev.start + prev.len`, the reflow loop) — confirm that and do not re-derive it; the divergence is in the **sequence**, so name the owner (the caller, not `palette_acquire`).

- [ ] **Step 2: Derive the twelve `0x34B14` handlers and their size**

For each entry of the 22-entry table at `0x34B14` that the cycle-1 record §11.3 marks **gap**, record the handler's body, its callees, and its `+0x52`/`+0x53`/`+0x54`/`+0x57` writes:

| `+0x52` | entry | handler |
|---|---|---|
| 1 | `0x34C15` | `0x359E0` |
| 2 | `0x34C26` | `0x35C1C`/`0x35D20` |
| 4 | `0x34C53` | `0x35F84` |
| 5 | `0x34C62` | `0x36430` |
| 7 | `0x34C80` | `0x399CC` |
| 8 | `0x34C8D` | `0x37464` |
| 12 | `0x34C9A` | `0x361C8` |
| 13 | `0x34CA9` | `0x36300` |
| 17 | `0x34CB8` | `0x36710` |
| 19 | `0x34D22` | `0x33B00` per side |
| 20 | `0x34D69` | `0x35E6C` |
| 21 | `0x34D78` | `0x364FC` |

Plus `0x349C8`'s sub-handlers `0x37178` and `0x37D18`.

Then answer the Gate's question: **which of them returns `+0x52` from 9 to 4?** (Task 6b measured s1 stalling on `+0x52 = 9`, `+0x53 = 8` at loop 1400 where the original is at `+0x52 = 4`.) Trace the original's `+0x52` trajectory around that frame in a live-RAM dump if the static reading is ambiguous.

**Measure the closure size** — the count of new functions and bytes, as cycle 2's Task 1 did for the hit chain. If it is materially larger than a cycle, **stop and report the size** — a re-scope conversation with the human, not a silent overrun.

- [ ] **Step 3: Derive the pose selector**

At capture 834 the raptor's silhouette has IoU 0.522 and no port frame matches over 900. Establish what selects the raptor's sprite in the original at that frame: which of `+0x52`/`+0x53`/`+0x54`/`+0x57`, which animation stream, and which `rec+0x52` value. Compare with the port's values at the same dumped frame.

State whether the divergence is **a single derivable state** (a field, a tick, or a stream) or a subsystem. If it cannot be pinned, say so — it becomes a named gap, never a fitted value.

- [ ] **Step 4: Derive the loader flush scope's separability**

Candidate (a) is confirmed: the original's loader flush drains only the `0x33734` initial and `0x80997C` font palettes, and the arena's records survive to the gate's `0x25672` flush; the port's loader flush drains the arena's records too. Establish whether scoping it is **separable** from the 831/832 held-frame gap (which needs the un-derivable post-read ISR ticks), and whether fixing it alone moves `make demo-oracle` at all. Report the measurement, not a prediction.

- [ ] **Step 5: Answer the RNG question**

Determine whether the `0x34B14` tail consumes RNG (`0x5D7DC`). If it does, the port's stream alignment changes and a determinism answer is needed before it can be ported — record it, and flag it to the human rather than pinning it.

- [ ] **Step 6: Write the record and commit**

Write the record with sections §0 (address model and corrections), §1–§5 above, §6 the named gaps, §7 the unit-test values, and §8 the provenance. Correct any plan premise it refutes, with the address. Then:

```bash
git add docs/superpowers/plans/2026-09-22-combat-fidelity-derivations.md
git commit -m "docs: derive the combat/render fidelity cycle"
```

**Gate for this task:** the record is the authoritative value source for Tasks 2–4, every value cites an address, and the handler tail's size is measured (or the size case is reported).

---

### Task 2: The driver's palette-variant seed and the loader flush scope

**Task 1's finding (the raw wins): the palette engine is faithful.** The original's and the port's `DS_00107618` tables are identical in order and `start`/`len` for all 11 entries; no `palette_acquire` call diverges. The T-rex's colour difference is the **front-end dump driver's `DS_0010816A` seed** (`port/tests/test_frontend.c:475-476`, `= 0xFF`), which makes the port's palette variant 0 where the original's (BSS 0) is 1 — so the port acquires `0x1BB9FD58` where the original acquires `0x1BB9FCD8`. **Fix the driver's seed, not the engine** (record §1.5/§7.1). The human ruled: fix it and record the reference change it causes — the oracle's **claims** are the invariant, not its window indices.

**Files:**
- Modify: `port/tests/test_frontend.c` (the `DS_0010816A` seed at `:475-476`)
- Modify: `port/src/platform/res.c` / `port/src/game/actors.c` (the loader flush scope, per record §4 — apply only what the record shows faithful)
- Test: `port/tests/test_frontend.c`

**Interfaces:**
- Consumes: record §1 (the variant mechanism — `0x41350`, `DS_00105B34[0]`, the `0xA8A28`/`0xA8AF8` handle tables) and §4 (the loader flush scope's separability).
- Produces: the port's character variant and handles matching the original; the arena's byte-diff reduced.

- [ ] **Step 1: Write the failing test**

Assert the driver's seed and its consequence: with `DS_0010816A` at the original's value, the port's variant and T-rex handle match the original's. Seed a sentinel that differs from the post-condition:

```c
/* Record §1: the original's DS_0010816A is BSS 0, giving variant 1 → 0x1BB9FCD8. */
CHECK_EQ_INT((int)DSB(DS_0010816A + 1u), 0);
/* after the acquire, the T-rex's table entry holds the original's handle */
CHECK_EQ_INT((int)DSD(<the T-rex's DS_00107618 entry>), 0x1BB9FCD8);
```

Substitute the record's table entry. The sentinel is the value seeded before the call, chosen to differ from the post-condition.

- [ ] **Step 2: Run it to verify it fails**

Run: `./build/run_tests`
Expected: FAIL — the driver seeds `0xFF`, so the variant is 0 and the handle is `0x1BB9FD58`.

- [ ] **Step 3: Implement**

Set the driver's seed to the original's value (BSS 0). Apply the loader flush scope only as far as record §4 shows it faithful — it is oracle-neutral (1382/1382 byte-identical frames), so a change there is for fidelity, not for the oracle.

- [ ] **Step 4: Run it to verify it passes**

Run: `./build/run_tests`
Expected: PASS, output pristine.

- [ ] **Step 5: Prove the assertion can fail**

Restore the `0xFF` seed and confirm the named assertion fails. Restore, and report the mutation with its command and output.

- [ ] **Step 6: Re-measure the arena and the oracle**

Record the arena's byte-diff against capture 834 (before: 42667 bytes / 22.2% / 15067 px), the T-rex region (before: 6363 px), and the first-unexplained frame (832). Expected: the diff drops to about 30536 bytes / 15.9% / 10602 px and the T-rex region to about 1898 px (record §1.5). Because the port's dump changes, the front-end oracle's **derived window indices may move** and the reference may need one re-capture: run `make frontend-oracle`, and if it fails on the window, re-capture and record which indices moved and why. The **claims** must not move. If the diff does not move, return to the record — do not tune to match.

- [ ] **Step 7: Full ladder and commit**

Run: `make verify`
Expected: exit 0, 0 warnings, every oracle claim unmoved.

```bash
git add port/tests/test_frontend.c
git commit -m "tests: seed the driver's palette variant to the original's"
```

**Gate for this task:** the port's palette variant and T-rex handle match the original's, and the arena's byte-diff drops.

---

### Task 3: The `0x34B14` handlers

**Files:**
- Modify: `port/src/game/fighter.c` (the `+0x52` dispatch and the state handlers)
- Modify: `port/src/game/fight.c` if the record places a handler in the health/HUD spine
- Test: `port/tests/test_fight.c`

**Interfaces:**
- Consumes: record §2 (each handler's body, callees and writes; the size; the `+0x52 = 9 → 4` answer).
- Produces: the fight progressing past loop 1400 to the timer exit.

- [ ] **Step 1: Size gate**

Read record §2's measured closure. If it is materially larger than a cycle, **stop and report the size** — a re-scope conversation with the human. Otherwise say which case you are in before implementing.

- [ ] **Step 2: Write the failing test**

For each handler, assert its raw's observable effect at the dispatch point, with seeded sentinels. Example shape (substitute the record's addresses and values):

```c
/* Record §2: +0x52 == <n> dispatches to <handler>, which writes <value>. */
DSB(slot + 0x52u) = <n>;
<seed the inputs the handler reads, e.g. slot+0x53/0x54/0x57 and the command word>
<call the dispatch>
CHECK_EQ_INT((int)DSB(slot + 0x52u), <the record's post-value>);
```

- [ ] **Step 3: Run it to verify it fails**

Run: `./build/run_tests`
Expected: FAIL — the handler is unported.

- [ ] **Step 4: Implement**

Port each handler, one C function per original function with its address tag, into the owner the record names. Where a handler consumes RNG, apply record §5's determinism answer.

- [ ] **Step 5: Run it to verify it passes**

Run: `./build/run_tests`
Expected: PASS, output pristine.

- [ ] **Step 6: Prove each assertion can fail**

Mutate each handler (drop its store, or change its value) and confirm the named assertion fails. Restore, and report the mutations.

- [ ] **Step 7: Measure the Gate's first claim**

Add the test or env-gated probe the spec's Verification names, asserting the fight reaches the 900-frame timer exit (loop 1969). Record the last state-change frame (before: 1400) and the timer exit.

- [ ] **Step 8: Full ladder and commit**

Run: `make verify`
Expected: exit 0, 0 warnings, every oracle claim unmoved.

```bash
git add port/src/game/fighter.c port/src/game/fight.c port/tests/test_fight.c
git commit -m "fight: port the +0x52 state handlers"
```

**Gate for this task:** the fight reaches the timer exit without stalling, and every handler is asserted with a mutation proof.

---

### Task 4: The fighter animation poses

**Files:**
- Modify: the owner the record names (candidates: `port/src/game/actors.c`, `port/src/game/fighter.c`)
- Test: `port/tests/test_actors.c` or `port/tests/test_anim.c`

**Interfaces:**
- Consumes: record §3 (the pose selector and the port's divergence).
- Produces: the raptor's silhouette matching capture 834 (or the divergence named as a gap).

- [ ] **Step 1: Write the failing test**

Assert the selector's value at the record's frame, with a seeded sentinel — the field/stream/tick the record names, set up so it can fail:

```c
/* Record §3: <the selector> at the original's capture-834 frame. */
<seed the pre-state>
<run the tick>
CHECK_EQ_INT((int)<the selector>, <the record's value>);
```

- [ ] **Step 2: Run it to verify it fails**

Run: `./build/run_tests`
Expected: FAIL — the port's state differs.

- [ ] **Step 3: Implement**

Port the divergence the record names. If the record found no single derivable divergence, **do not invent one** — implement nothing, write the named gap into the record, and report it.

- [ ] **Step 4: Run it to verify it passes**

Run: `./build/run_tests`
Expected: PASS, output pristine.

- [ ] **Step 5: Prove the assertion can fail**

Mutate the fix and confirm the named assertion fails. Restore, and report the mutation.

- [ ] **Step 6: Re-measure the oracle**

```bash
make demo-oracle
```

Record the arena's byte-diff (Task 2's value → now) and the raptor's silhouette IoU (before: 0.522). Expected: the IoU rises and the diff drops to the palette-and-pose-excluded level.

- [ ] **Step 7: Full ladder and commit**

Run: `make verify`
Expected: exit 0, 0 warnings, every oracle claim unmoved.

```bash
git add <the files the record names>
git commit -m "actors: fix the fighters' animation pose state"
```

**Gate for this task:** the pose selector matches the record, and the arena's diff and the raptor's IoU improve — or the divergence is a named gap with its evidence.

---

### Task 5: The Gate and the record

**Files:**
- Modify: `port/spec/game_flow.md`, `README.md`, `docs/superpowers/specs/2026-09-22-combat-fidelity-design.md`

**Interfaces:**
- Consumes: every task's measurement.
- Produces: the recorded outcome and the Gate assessed honestly.

- [ ] **Step 1: Measure the Gate's two claims**

Claim 1: the fight reaches the timer exit — record the last state-change frame and the exit frame. Claim 2: `0x1BB9FD58`'s DAC range and the arena's byte-diff, with the pose residual named.

- [ ] **Step 2: Assess the Gate honestly**

If both claims hold, say so with the numbers. **If either does not, do not declare it met and do not fit a pin or a value to force it:** report the residual as a named gap with its evidence, and state which task left it. Cycle 2's failure mode was a record that nearly read as met.

- [ ] **Step 3: Record the cycle's status**

In `port/spec/game_flow.md` and `README.md`: the Gate's two claims with their numbers, the palette's DAC range, the handlers ported, the pose outcome, the arena's before/after byte-diff, and that 831/832 remains a named gap and the interactive match remains unowned. In the design spec, mark the cycle's Outcome and leave the interactive match's section standing.

- [ ] **Step 4: Confirm every oracle claim is unmoved**

Run: `make title-oracle`, `make attract-oracle`, `make smk-oracle`, `make frontend-oracle`.
Expected: claims byte-identical to the Global Constraints' list. Report any index that moved, with its old and new value.

- [ ] **Step 5: Full ladder and commit**

Run: `make verify`
Expected: exit 0, 0 warnings, every gate unmoved.

```bash
git add port/spec/game_flow.md README.md docs/superpowers/specs/2026-09-22-combat-fidelity-design.md
git commit -m "docs: record the combat/render fidelity outcome"
```

**Gate for this cycle:** the fight reaches the timer exit and the arena's character palette matches the raw's DAC range — or the residual is a named gap with its evidence and the Gate is recorded as unmet.

---

## Self-Review

**Spec coverage.** The spec's Goal (the fight reaches the timer exit; the palette matches the raw) is Task 3 Step 7 and Task 2 Step 1, assessed in Task 5 Step 2. Its Scope "In" maps to tasks: the palette acquisition order and the loader flush scope → Task 2; the twelve `0x34B14` handlers plus `0x349C8`'s sub-handlers → Task 3; the fighter animation/think state → Task 4. Its "Out" — the 831/832 held-frame presentation and the interactive match — appears as an explicit exclusion in Task 2 Step 6, Task 4 Step 3 and Task 5 Step 3, with 831/832 recorded as a named gap rather than silently dropped. Its Architecture ("no new modules unless the derivation proves one") is Task 1's §1–§3 owner statements. Its Verification's two claims are Task 3 Step 7 and Task 2 Step 1; its oracle-claims rule is every task's ladder step and Task 5 Step 4. Its Risks map to Task 1 Step 2 (the handler tail's size, with an explicit stop), Task 1 Step 3 and Task 4 Step 3 (the poses may be un-derivable), Task 1 Step 1 (the palette fix's owner), and Task 5 Step 2 (the Gate may be unmet).

**Placeholder scan.** No `TBD`, `TODO`, or "similar to Task N". The `<...>` substitutions in Tasks 2–4's test skeletons are deliberate: the ported function's real name, the table entry, and the values come from the Task 1 record, exactly as cycles 1 and 2's plans did — every step names the record section that supplies them and states that a missing anchor is a Task 1 defect rather than licence to invent one. No step describes an action without its command.

**Type consistency.** `palette_acquire`, `palette_record`, `gfx_flush_palette`, `fight_health_sync`, `fighter_think`, `actors_update`, `game_state_step` are the existing names and are used unchanged. New functions are named for the original they port and introduced by the task that first needs them; later tasks reference them by the name their own `Interfaces` block gives. Globals are referenced by their `symbols.h` names; a name the generator does not emit gets a local `#define` with the raw address.

**Right-sizing.** Task 1 is one derivation deliverable with its own gate. Tasks 2, 3 and 4 each end at an independently verifiable measurement — the palette's DAC range and the arena's diff, the fight's progression, the raptor's IoU — which is why the palette order and the loader flush scope are one task (they share the enqueue order) and the twelve handlers are one task (they share the dispatch and the stall). Task 5 is the cycle's record and its honest Gate assessment, which no porting task can own.
