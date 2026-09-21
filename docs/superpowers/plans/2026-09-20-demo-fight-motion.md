# Demo fight, cycle 1 — motion and render (implementation plan)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Port the attract demo fight's motion and render path — states 6 and 7 of the original's state machine — so the port's demo advances from the front-end into a visible, moving CPU-vs-CPU fight, with a report-only pixel oracle that proves the demo window clean up to the frame the original first lands a hit.

**Architecture:** Three new modules carry the fight: `camera.c` (the fight camera state machine and the 16.16 projection), `fighter.c` (per-fighter think/AI and, in cycle 2, hit resolution), and `fight.c` (the arena frame and the HUD/health path). `flow.c` wires states 6 and 7 into the existing `game_state_step` switch and gains the `DS_00104B15` tail in `game_frame` minus its combat calls. The existing capture and oracle machinery is reused: one `PR_FRONTEND_DUMP` run, a second compare window over it as its own report-only ladder target, and the demo's determinism pinned in the existing `tools/title_pin.py` table.

**Tech Stack:** C99, CMake, SDL3 (host layer only), Python 3 for the oracle tools, DOSBox-X for captures. The original is `data/game/C/PRAGE.EXE` (DOS/4GW bound LE, 32-bit); its decompilation is `port/decomp/prage.c`.

**Spec:** `docs/superpowers/specs/2026-09-20-demo-fight-design.md`. Read it first — it carries the correction this cycle starts from (states 6/7/8 are the attract demo, not the interactive match) and the evidence for the scope boundary.

## Global Constraints

- Fixed profile; register-level fidelity. Original state lives in `mem[]`; pointer globals are `mem[]` offsets consumed as `mem + DSD(...)`. The original's globals keep their `port/src/symbols.h` names.
- SDL appears only in `port/src/host.c` and `port/src/main.c`.
- `data/` is read-only and git-ignored. `tools/` existing files are read-only **except `tools/title_pin.py` and `tools/title_compare.py`**, carved out for this cycle by explicit approval. `tools/title_capture.py` and `tools/smk_capture.py` are **not** modified.
- Comments in `port/src` are only `/* PORT: ... */`, `/* TODO(verify): */`, and address tags. No comments in `port/tests` beyond the existing style.
- Build with **0 warnings**. No new dependencies. **Never ship a fitted constant** — every value is derived from the raw or the capture, or is a declared gap.
- Tests: one `int test_X(void)` per file, declared in `port/tests/test.h` and called from `port/tests/run_tests.c`; only `CHECK(cond,msg)` and `CHECK_EQ_INT(a,b)`. `port/src` is a PUBLIC include dir. `port/CMakeLists.txt` lists sources explicitly.
- `game_init()` may run only once per process; any test that calls it must be env-gated (`PR_*_DUMP`).
- Assertions must be able to fail. A test that pre-sets the value it then checks is a defect, not a style point: seed sentinels that differ from the post-conditions, and prove each new assertion fails under a mutation of the code it tests.
- Capture oracles **skip cleanly** when their capture directory is absent — never fail. `PR_ORACLE_REQUIRED=1` makes an absent capture a failure.
- Oracles that must not move: the title oracle (`54 clean, 55 splice, 2 transition, 0 unexplained` and `54 clean, 57 splice, 0 unexplained`), the attract oracle (`FIRST DIVERGENCE at capture frame 215`), `smk_compare` (`120/120`, `41/41`), the enforced front-end oracle (window `[557..813]`, 257 frames, `0 unexplained`), and `oracle C-vs-Python: 9866 writes byte-exact`.
- On plan-vs-raw conflict, the RAW wins; record the correction and the address.
- `make verify` must pass at the end of every task whose change can move an oracle.
- Never `git add -A`. Commit style: `<area>: <what changed>`.

## The correction this cycle starts from

`port/spec/game_flow.md` and `README.md` call states 6/7/8 "the fight engine". They are the **attract-mode CPU-vs-CPU demo fight and its continue sequence**. The interactive match is a different machine — `DS_00104B00`, a screen *mode*, entered by the `0x257A4` coin divert and flowing `0x10` → `0x13` → `0x1E`. Cycle 1 ports only the demo path plus the state-6 timeout exit; the mode graph, the coin divert, `0x1EEB0`, `0x1F458`, the player screens and the human input mapping are out of scope and are recorded as unowned.

Two boundaries verified while the spec was written, both of which this plan depends on:

- **No state `>= 10` is ever assigned.** Every assignment to `DS_000F0A64` in the image is 0–9 or `DS_000F0A6C`. The `>= 10 → 0x11000` arm is unreachable; `0x11000` is reached only by state 0's fallthrough, which the port already encodes.
- **The demo never ends a round.** `0x27FA8` (the round-end check) has exactly two callers — `0x26254` (mode 4/0xB) and `0x26540` (mode 0x21) — neither on the state-7 path, and `0x49C78` is not a caller. So the KO/win screens stay out of scope and the demo runs out state 6's 900-frame timer.

## File Structure

| file | responsibility |
|---|---|
| `port/src/game/camera.c`, `camera.h` (new) | the fight camera state machine (`0x12D48` dispatcher, `0x12CD4`, `0x1317C`, `0x12DA8`, `0x1282C`) and the projection (`0x17FA0`, `0x17580`, `0x16D58`) |
| `port/src/game/fighter.c`, `fighter.h` (new) | the per-fighter think/AI chain (`0x1975C`, `0x3B464`, `0x3B298`, `0x3B134`, `0x3BDDC`, `0x18C14`, `0x1A978`) — cycle 2 adds combat here |
| `port/src/game/fight.c`, `fight.h` (new) | the arena frame `0x263F4`, the HUD/health path (`0x35658`, `0x1D890`), and state 6/7's bodies (`0x11A8C`) |
| `port/src/game/flow.c` (modify) | `game_state_step` cases 6/7; the `game_frame` `DS_00104B15` tail minus its combat calls |
| `docs/superpowers/plans/2026-09-20-demo-fight-derivations.md` (new) | the raw-byte derivation record (Task 1) |
| `port/tests/test_fight.c` (new) | camera, projection, think mapping and state 6/7 assertions |
| `port/tests/run_tests.c`, `port/tests/test.h`, `port/CMakeLists.txt` (modify) | registration of the new test file |
| `tools/title_pin.py` (modify, approved) | + the demo's determinism pins, same fail-closed patch table |
| `tools/title_compare.py` (modify, approved) | + a demo window, same alignment/splice classification |
| `Makefile` (modify) | a `demo-oracle` target; the dump length raised |
| `port/spec/game_flow.md`, `README.md`, `docs/superpowers/specs/2026-09-20-demo-fight-design.md` (modify) | dispositions, the cycle's gaps, and the corrected 6/7/8 framing |

---

## Amendments

Two changes were made after execution began; the numbering below reflects them.

1. **A new Task 6 was inserted and the old Tasks 6–7 became 7–8.** Task 5's review
   confirmed, and the reviewer independently verified against the raw, that the demo
   advances into state 7 but has **no fighters**: `DS_001077A8`, the live-slot count that
   `0x263F4`, `0x186D0` and `0x33F08` read, is only ever *read* in the port — its writer
   is `0x33CA0` inside `0x33C78`, reached solely through the spawn `0x33EB4` (state 6) /
   `0x357D6` (HUD), which no cycle owned. The cycle's stated bound (clean up to the frame
   the original first lands a hit) is unreachable while the arena frame runs on null
   slots, so the human ruled that the spawn folds into cycle 1 as the new Task 6.
2. **Task 3's "stubs" wording is void.** The human ruled during the pre-flight scan that
   no behaviour-less stub ships: port what the record shows is load-bearing, otherwise a
   `/* PORT: <addr>. <reason> */` skip plus a named gap. Task 3 was executed that way.
3. **A new Task 8 was inserted and the old Task 8 became Task 9.** Task 7's measurement
   showed the demo still frozen after its fix, and systematic debugging (controller-run,
   instrumenting the port and confirming against the raw) found a second, dominant
   omission: `fight_health_sync` (`0x34B6C`) ports only the `rec+0x52 == 6` case of the
   raw's dispatch, while the raw jumps through a 22-entry table —
   `0x34BF4 cmp al,0x15; ja 0x34C08; jmp dword ptr cs:[eax*4 + 0x24B14]`. The demo's
   fighters sit at `rec+0x52 == 0`, so they take the unported default `0x349C8`; the AI
   mapper `fight_command_map` (`0x3B134`) is never called, the command words
   `DS_001088E0`/`E2` stay 0, and the fighters never move or change animation. The human
   ruled that the `+0x52` handlers fold into cycle 1. (The table is at `0x34B14`
   — `PTR_LAB_00034b14`, confirmed in Ghidra; the port's comment naming it is
   correct.) Task 7's report blamed `0x3C88C` and `0x20DF4`; both were refuted by
   disassembly — `0x3C88C`'s subtree is pure slot logic with no aperture or blit
   reference.

---

### Task 1: Derive the fight camera, the think chain and `0x49C78`

This is the cycle's derivation task and it gates every other task. It is also where the spec's open question 2 is answered: **how often the demo's AI consumes the RNG.** That answer decides how many pins Task 9 needs, and whether the demo bound is the first landing hit or something earlier.

**Files:**
- Create: `docs/superpowers/plans/2026-09-20-demo-fight-derivations.md`

**Interfaces:**
- Produces: the record later tasks implement from — for each function below, its arguments, its globals, its arithmetic (with exact widths and signedness), and the concrete input/expected-output pairs its unit test must assert.

**Toolchain (the same the previous cycle's derivation used):** `data/game/C/PRAGE.EXE` is read-only; obj-0 (code) file offset = VA + `0x52E54`, obj-1 (data) = VA + `0x46E54`; disassemble with capstone `CS(CS_ARCH_X86, CS_MODE_32)` over the LE image rebuilt with a replica of `port/src/mem.c`'s `mem_load_le` + `mem_load_le_fixups`. The engine decompilation is `port/decomp/prage.c` (read-only). Mirror the record's shape on `docs/superpowers/plans/2026-09-20-frontend-chain-derivations.md`.

- [ ] **Step 1: Derive the projection `0x17FA0`**

`0x17FA0` (557 B) takes a byte pointer and a record pointer (`&DS_00100B60`/`&DS_00100AF0` for P1, `&DS_00100B61`/`&DS_00100AF4` for P2) and is called six times per demo frame. Record: the exact fixed-point format of the multiply and shift, the rounding, the clamp ranges, and every global it writes. This is the cycle's bellwether — a single rounding difference drifts the whole arena — so record the arithmetic in enough detail to unit-test it byte-for-byte, with at least four worked input/output pairs taken from the raw.

- [ ] **Step 2: Derive the camera chain**

`0x12D48` (93 B, the dispatcher), `0x12DF0` (74 B), `0x12E3C` (409 B), `0x13290` (169 B), `0x1333C` (134 B), `0x12CD4` (93 B), `0x1317C` (122 B), `0x12DA8` (72 B) and `0x1282C` (166 B). These were **deferred as unreachable** by the previous cycle (`port/spec/game_flow.md` records them as unowned); the demo fight is the caller that makes them reachable, so this cycle retires that deferral. Record each function's body, the camera-mode global it dispatches on, and the `DS_000F0AEC`/`DS_000F0AF0`/`DS_000F0AF4` semantics. Note that `0x1324C` (shake decay) is already ported and registered but dormant; the demo may be what makes it live, so record whether any demo path sets `DS_00104AE8` bit 0.

- [ ] **Step 3: Derive the arena frame's remaining callees**

`0x263F4`'s body is already transcribed in the spec; derive what it calls that the port does not have: `0x3C5CC` (23 B), `0x16D58` (73 B), `0x17580` (330 B), `0x1958C` (464 B), `0x19068` (250 B), `0x3CB68` (91 B), `0x35658` (478 B), `0x1282C`, `0x12DA8`. For each: body, globals, and what it contributes to the frame. `0x35658` and the HUD/health path are cycle 2's to make live but cycle 1 must not break them — record what they read so the tail can be added without them.

- [ ] **Step 4: Derive `0x49C78`**

`0x49C78` is 2492 bytes with 12 callees and is the largest single unknown in the slice. Derive it fully enough to port: its phases or loops, every callee it pulls in that the port lacks, and which parts are resolvable. Anything that depends on state the port does not model goes in Step 7 as a named gap — do not guess.

- [ ] **Step 5: Derive the think/AI chain and answer the RNG question**

`0x1975C` (186 B), `0x3B464` (608 B), `0x3B298` (441 B), `0x3B134` (355 B), `0x3BDDC` (401 B), `0x18C14` (1035 B), `0x1A978` (408 B), plus the behaviour helpers the record finds. Record the input→command-word mapping in `0x3B134`/`0x3B298` precisely enough to unit-test over hand-built input masks, and record every RNG call site (`0x5D7DC`) inside the think chain with its range argument and its per-frame or per-decision frequency.

**Answer this explicitly, with the call sites as evidence:** does the demo's AI draw from the RNG every frame, every decision, or rarely? If it draws every frame, say how many draws per frame and in what order, because that is exactly what Task 9 must pin.

- [ ] **Step 6: Record what could not be determined**

Any function whose semantics depend on state the port does not model is listed as a named gap with the evidence — not guessed. In particular: the resource handles behind the HUD/scene (`0x1EA08`'s paged-resource reader `0x2DBC4`/`0x2DB58` is unported, the same class of gap the previous cycle declared).

- [ ] **Step 7: Give each function its unit-test values**

For every function derived in Steps 1–5, record the concrete input and the exact expected output or global transition its unit test must assert — the shape `2026-09-20-frontend-chain-derivations.md` §8 used. A function whose values cannot be pinned is listed in Step 6 instead. Without this the porting tasks have nothing concrete to assert.

- [ ] **Step 8: Commit**

```bash
git add docs/superpowers/plans/2026-09-20-demo-fight-derivations.md
git commit -m "docs: derive the demo fight's camera, think chain and 0x49C78"
```

**Gate for this task:** the record answers the RNG-consumption question with call sites, and every function Task 2–5 names has either unit-test values or a Step 6 gap entry. Nothing downstream starts before this resolves.

---

### Task 2: The fight camera and projection

The demo cannot render without a camera, and the camera is where a single rounding error drifts the whole arena — so it is ported and unit-proven before anything draws.

**Files:**
- Create: `port/src/game/camera.c`, `port/src/game/camera.h`
- Create: `port/tests/test_fight.c`
- Modify: `port/tests/test.h`, `port/tests/run_tests.c`, `port/CMakeLists.txt`

**Interfaces:**
- Consumes: Task 1's record for the `0x17FA0` arithmetic and the `0x12D48` mode table.
- Produces: `void camera_init(void);`, `void camera_project(u8 side, u32 rec);` (the `0x17FA0` projection for one side), `void camera_dispatch(void);` (the `0x12D48` per-frame mode switch), and `void camera_scene_step(void);` (the `0x1282C` + `0x12DA8` pair `0x263F4` calls at the end of a frame).

The record fixes the internal arithmetic; these signatures and the field names are the port's, and later tasks call them exactly as written.

- [ ] **Step 1: Add the test file and its registration**

`port/tests/test_fight.c` with one entry point, plus its declaration in `port/tests/test.h`, its call in `port/tests/run_tests.c` beside the other `test_X()` calls, and `tests/test_fight.c` in `port/CMakeLists.txt`'s explicit source list.

- [ ] **Step 2: Write the failing projection test**

Projection writes the fighter's world position. `0x186D0` reads `DS_001077DC`/`DS_001077E0`/`DS_001077E4` for a slot, so the projection's output lands in that record. Assert the record's four worked pairs — the exact inputs and expected outputs Task 1 Step 1 recorded — and the `side` selection (side 0 uses `&DS_00100B60`/`&DS_00100AF0`, side 1 uses `&DS_00100B61`/`&DS_00100AF4`):

```c
/* 0x17FA0: the record's worked pairs, projected per side. PROJECT_P1_IN /
 * PROJECT_P1_OUT and PROJECT_P2_IN / PROJECT_P2_OUT are Task 1 Step 7's
 * anchors for this function, transcribed verbatim as literals. */
{
    u32 rec0 = DSD(DS_001077B0);
    u32 rec1 = DSD(DS_001077B4);
    DSD(DS_00100AF0) = PROJECT_P1_IN;            /* Task 1 Step 7 anchor */
    camera_project(0, rec0);
    CHECK_EQ_INT((int)DSD(rec0 + 0x1Cu), PROJECT_P1_OUT);
    camera_project(1, rec1);
    CHECK_EQ_INT((int)DSD(rec1 + 0x1Cu), PROJECT_P2_OUT);
}
```

Define those four as literals from Task 1's Step 7 anchors. Do not substitute
an approximation: if an anchor is missing, that is a Task 1 defect to fix there,
not licence to invent a value here.

Seed the records with values that differ between the sides, so a `side` swap fails rather than passing on identical data.

- [ ] **Step 3: Write the failing dispatch test**

`0x12D48` switches on the camera mode and `0x12CD4` compares against `0x100`, stepping by `0x40` when the difference exceeds it and using the target otherwise; `0x1324C` (already ported as `camera_shake_decay`) clears update-table bit 0 when the velocity is negative and the accumulator reaches zero. Assert the mode dispatch and one `0x12CD4` step each way across the `0x100` boundary, with `DS_00104AE8` pre-seeded to a sentinel so the shake clear is observable.

- [ ] **Step 4: Run and watch them fail**

Run: `make build && ./build/run_tests 2>&1 | grep -iE "camera|project"`
Expected: FAIL — `camera_project` and the rest do not exist.

- [ ] **Step 5: Implement the module**

`0x17FA0` first — its arithmetic, widths and rounding exactly as the record pins them. Then `0x17580`, `0x16D58`, the `0x12D48` dispatcher with `0x12DF0`/`0x12E3C`/`0x13290`/`0x1333C`, `0x12CD4`, `0x1317C`, `0x12DA8` and `0x1282C`, plus `camera_init` registering whatever the raw registers (the record says which table and bit; `0x1324C` is already registered by `effects.c`, so do not double-register it).

Comments follow the `port/src` convention: `/* PORT: 0xADDR. ... */` and address tags only.

- [ ] **Step 6: Run and watch them pass**

Run: `make build && ./build/run_tests 2>&1 | grep -iE "camera|project"`
Expected: PASS, and `all checks passed` with no other test moving.

- [ ] **Step 7: Prove the assertions can fail**

Break the projection's rounding deliberately — shift by one less than the record says — rebuild, and confirm the projection assertions fail. Then restore. An assertion that survives a wrong shift is not testing the projection.

- [ ] **Step 8: Full ladder and commit**

Run: `make verify`
Expected: exit 0, 0 warnings, every oracle unmoved (the camera is not yet called from a live path, so nothing should move — if something does, investigate rather than accept).

```bash
git add port/src/game/camera.c port/src/game/camera.h port/tests/test_fight.c port/tests/test.h port/tests/run_tests.c port/CMakeLists.txt
git commit -m "camera: port the fight camera and projection"
```

---

### Task 3: The arena frame and the fighter render

**Files:**
- Create: `port/src/game/fight.c`, `port/src/game/fight.h`
- Modify: `port/src/game/fighter.c`, `port/src/game/fighter.h` (created here)
- Test: `port/tests/test_fight.c`

**Interfaces:**
- Consumes: Task 1's record for `0x263F4`'s callees; Task 2's `camera_project`, `camera_scene_step`.
- Produces: `void fight_arena_frame(void);` (the port's `0x263F4`, in the raw's exact call order) and `void fighter_render(u8 side);` (`0x1958C`/`0x19068`).

- [ ] **Step 1: Write the failing test**

`0x263F4`'s order is the contract: `0x3C5CC`, `0x16D58` twice, the two position latches (`DS_001077E8 = DS_001077E4`, `DS_0010787C = DS_00107878`), the two projections, `0x17580`, the two fighter updates, the two projections again, `0x1975C`, the two projections again, `0x3CB68`, `0x35658` twice, `0x49C78`, `0x1282C`, `0x12DA8`. Assert the observable parts: the latches take the previous frame's values, and the fighter updates leave the records changed. Seed the latches to sentinels that differ from the values the latches copy, so a missing latch fails.

- [ ] **Step 2: Run and watch it fail**

Run: `make build && ./build/run_tests 2>&1 | grep -iE "arena|fighter"`
Expected: FAIL — the functions do not exist.

- [ ] **Step 3: Implement the frame and the fighter render path**

Transcribe `0x263F4` in the raw's order — the six projection calls are not redundant, so do not collapse them. Then `0x1958C` and `0x19068` from the record, reusing `actors.c`'s existing animation and pset machinery rather than adding a parallel one. `0x49C78`, `0x35658` and the HUD path belong to `fight.c` as stubs that the record says are inert until cycle 2, unless Task 1 found them load-bearing for motion — the record's Step 6 entry decides, and if they are load-bearing they are ported here.

- [ ] **Step 4: Run and watch it pass**

Run: `make build && ./build/run_tests 2>&1 | grep -iE "arena|fighter"`
Expected: PASS.

- [ ] **Step 5: Prove the assertions can fail**

Remove one of the position latches, rebuild, confirm the test fails, restore.

- [ ] **Step 6: Full ladder and commit**

Run: `make verify`
Expected: exit 0, 0 warnings, every oracle unmoved.

```bash
git add port/src/game/fight.c port/src/game/fight.h port/src/game/fighter.c port/src/game/fighter.h port/tests/test_fight.c
git commit -m "fight: port the arena frame and the fighter render"
```

---

### Task 4: The think/AI chain

Without this the fighters stand still and the demo never visible-fights, so it lands before the states are wired.

**Files:**
- Modify: `port/src/game/fighter.c`, `port/src/game/fighter.h`
- Test: `port/tests/test_fight.c`

**Interfaces:**
- Consumes: Task 1's record for `0x1975C` and the `0x3B134`/`0x3B298` command mapping; Task 3's `fighter_render`.
- Produces: `void fighter_think(u8 side);` (the port's `0x1975C`) and the command-word accessors the record pins (`0x3B134`/`0x3B298` read the input cursor and write the move command at `DS_001088E0`/`DS_001088E2`).

- [ ] **Step 1: Write the failing mapping test**

The command mapping is pure: an input mask and a fighter state in, a command word out. Assert the record's cases over hand-built masks, using the input bytes the record names, and include at least two cases whose expected command words differ — the recurring defect in this repo has been assertions that pass whatever the code does:

```c
/* 0x3B134/0x3B298: the input cursor and the fighter state map to the command
 * word at DS_001088E0. THINK_MASK_A/B and THINK_WORD_A/B are Task 1 Step 7's
 * anchors for this mapping, transcribed verbatim as literals. */
{
    DSD(DS_001088E0) = 0xFFFF0000u;                /* sentinel != every expected word */
    DSB(DS_001088E4) = THINK_MASK_A;
    fighter_think(0);
    CHECK_EQ_INT((int)(DSW(DS_001088E0)), THINK_WORD_A);
    DSB(DS_001088E4) = THINK_MASK_B;
    fighter_think(0);
    CHECK_EQ_INT((int)(DSW(DS_001088E0)), THINK_WORD_B);
}
```

Define those four from Task 1's Step 7 anchors, and keep the sentinel distinct
from every expected word so the first assertion genuinely depends on the mapping.

- [ ] **Step 2: Write the failing chain test**

`0x1975C` calls `0x3B464` (the think), which reaches `0x3B298`/`0x3B134`. Assert that one call to `fighter_think` advances the fighter's animation state on the record the record names, with the pre-state seeded to a value the advance changes.

- [ ] **Step 3: Run and watch them fail**

Run: `make build && ./build/run_tests 2>&1 | grep -iE "think|command"`
Expected: FAIL — the functions do not exist.

- [ ] **Step 4: Implement the chain**

Transcribe `0x1975C`, `0x3B464`, `0x3B298`, `0x3B134`, `0x3BDDC`, and whichever of `0x18C14`/`0x1A978` and the behaviour helpers Task 1 Step 5 found on the demo path. Reuse `actors.c`'s animation interpreter for the transitions rather than reimplementing them. If the record shows any of these pulling in unported state, that part is a `/* PORT: ... */` skip with a Step 6 gap entry — not a fitted value.

- [ ] **Step 5: Run and watch them pass**

Run: `make build && ./build/run_tests 2>&1 | grep -iE "think|command"`
Expected: PASS.

- [ ] **Step 6: Prove the assertions can fail**

Invert one branch of the command mapping, rebuild, confirm both the mapping and the chain assertions fail, restore.

- [ ] **Step 7: Full ladder and commit**

Run: `make verify`
Expected: exit 0, 0 warnings, every oracle unmoved.

```bash
git add port/src/game/fighter.c port/src/game/fighter.h port/tests/test_fight.c
git commit -m "fighter: port the think and command chain"
```

---

### Task 5: States 6 and 7, the `game_frame` tail and the timer exit

**Files:**
- Modify: `port/src/game/flow.c`
- Test: `port/tests/test_fight.c`

**Interfaces:**
- Consumes: Task 2's `camera_*`, Task 3's `fight_arena_frame`, Task 4's `fighter_think`, and Task 1's record for `0x11A8C`.
- Produces: `game_state_step`'s cases 6 and 7 (staying inline in the switch if the raw inlines them, as case 5 does), and `game_frame`'s `DS_00104B15` tail.

- [ ] **Step 1: Write the failing state-6 test**

State 6's observables, from the raw `0x11A8C`: two RNG character picks through `0x41350` (one per player), `DS_00104B15 = 1`, the HUD spawn `0x1D890`, `DS_001082CC = 3`, `DS_00104B19.byte2` from the register handoff, the `(DS_00104528+1) & 2` branch running `0x1C500`/`0x2F198` three times when the bit is clear, and the four stores `DS_000F0A64 = 7`, `DS_000F0A6A = 900`, `DS_000F0A6C = DS_000F0A72`, `DS_000F0A6F = 0`.

Seed every one of those globals to a sentinel that differs from its post-condition — the previous cycle shipped three assertions that could not fail because the suite arrived with the value already set:

```c
/* 0x11A8C: the demo setup's stores and the HUD arm. */
{
    DSB(DS_00104B15) = 0;
    DSW(DS_001082CC) = 0;
    DSW(DS_000F0A64) = 0;
    DSW(DS_000F0A6A) = 0;
    DSW(DS_000F0A72) = 5;                 /* so the f0a6c store is observable */
    DSW(DS_000F0A6C) = 0;
    DSB(DS_000F0A6F) = 0xFF;
    game_state_step();
    CHECK_EQ_INT((int)DSB(DS_00104B15), 1);
    CHECK_EQ_INT((int)DSW(DS_001082CC), 3);
    CHECK_EQ_INT((int)DSW(DS_000F0A64), 7);
    CHECK_EQ_INT((int)DSW(DS_000F0A6A), 900);
    CHECK_EQ_INT((int)DSW(DS_000F0A6C), 5);
    CHECK_EQ_INT((int)DSB(DS_000F0A6F), 0);
}
```

- [ ] **Step 2: Write the failing state-7 and exit tests**

State 7 runs `fight_arena_frame` then `0x33F08`, then the three shared tails. The exit: when `DS_000F0A6A` reaches zero, `0x11BCC` restores `DS_000F0A64 = DS_000F0A6C`, clears `DS_00104B19.byte2` and `DS_00104B15`, and calls `0x29D60` + `0x2C3FC`. Seed `DS_000F0A6A` to 1 so one call fires the exit, and to 2 so one call does not.

- [ ] **Step 3: Run and watch them fail**

Run: `make build && ./build/run_tests 2>&1 | grep -iE "state 6|state 7|demo"`
Expected: FAIL — cases 6 and 7 are stubs.

- [ ] **Step 4: Implement the states and the tail**

Wire `case 6:` and `case 7:` in `game_state_step`, transcribing the raw's order. `case 7` is `0x263F4`, then `0x33F08`, then the three tails — note the raw tests `DS_000F0A6A - 1` *before* storing, so the exit fires on the frame the pre-decrement value is zero.

Then add `game_frame`'s tail at the end of the function, after `actors_update()`, gated on `DS_00104B15 != 0`, in the raw's order: `0x3BB90`, `0x12D48`, then per live entry of the two player records `0x186D0` + `0x2A690`, then `0x33F08`. **Cycle 1's combat call `0x3BB90` is the exception**: Task 4 of cycle 2 owns it. Until then it is a `/* PORT: 0x3BB90 is cycle 2's; see the spec's cycle split. */` skip, and the tail's other calls land. If the record shows the tail is inert without collision, say so in the report rather than wiring a partial tail silently.

- [ ] **Step 5: Run and watch them pass**

Run: `make build && ./build/run_tests 2>&1 | grep -iE "state 6|state 7|demo"`
Expected: PASS.

- [ ] **Step 6: Prove the assertions can fail**

Revert `case 6` to `break;`, rebuild, confirm the state-6 assertions fail, restore.

- [ ] **Step 7: Full ladder and commit**

Run: `make verify`
Expected: exit 0, 0 warnings, and the front-end gate still `0 unexplained`. The demo is now live in the `PR_FRONTEND_DUMP` run, so the dump may grow — if the existing front-end window moves, stop and investigate before committing.

```bash
git add port/src/game/flow.c port/tests/test_fight.c
git commit -m "flow: port the demo states 6 and 7 and the fight tail"
```

---

### Task 6: Port the fighter spawn (inserted — see Amendments)

This is the task that gives the demo something to render. Tasks 2–5 built a faithful motion and render path, but the arena frame runs on null slots because the spawn belonged to no cycle; until it lands, the demo window cannot be clean past the first frame the original draws a fighter.

**Files:**
- Modify: `port/src/game/flow.c` (wire `0x33EB4` into state 6 at the raw's position)
- Modify: `port/src/game/fighter.c`, `fighter.h`, `port/src/game/fight.c`, `fight.h` (the spawn and whatever it calls)
- Modify: `port/tests/test_fight.c`
- Modify: `docs/superpowers/plans/2026-09-20-demo-fight-derivations.md` — the spawn chain is not yet derived; extend the record before porting

**Interfaces:**
- Consumes: Task 5's state 6 in `flow.c`; Task 3's `fight_slot_clear`/`fight_health_bars` and Task 4's slot/character helpers; `actors.c`'s existing record and pset machinery.
- Produces: whatever the record pins for the spawn, and the live-slot count non-zero for the two slots after state 6.

- [ ] **Step 1: Derive the spawn chain into the record**

`0x33EB4` (state 6's spawn entry), `0x33C78` (571 B — its `0x33CA0 mov [eax+0x877A8],esi` is what populates `DS_001077A8`), and everything it pulls in: `0x494A8`, `0x29BC8`, `0x1CEBC`, `0x1D890`, `0x20DF4`, and the `DS_001082C8` EDX handoff. Extend `docs/superpowers/plans/2026-09-20-demo-fight-derivations.md` with a new section in the record's established shape: each function's body, globals, arithmetic with widths, addresses, and the concrete input/expected-output pairs its unit test must assert (the shape §8 uses). Derive before porting — the earlier tasks paid for guessing at gated call order.

- [ ] **Step 2: Settle the resource question — this is the tripwire**

`0x494A8`/`0x29BC8`/`0x1CEBC` may be the same unported paged-resource class (`0x2DBC4`/`0x2DB58`) that earlier cycles declared as gaps. Establish from the raw whether the spawn can populate the live-slot count and the two slot records without that reader. **If it cannot, stop and report BLOCKED with the evidence** — the human's fallback is to re-scope the cycle-1 bound and move the spawn to cycle 2. Do not fabricate a spawn to make the arena frame look alive.

- [ ] **Step 3: Port and wire it**

Wire `0x33EB4` into state 6 at the raw's position (after the character picks, where the raw calls it), and `0x1D890` where the raw calls it. Reuse `actors.c`'s record and pset machinery rather than adding a parallel allocator.

- [ ] **Step 4: Test the spawn**

Assert that after state 6 runs, the live-slot count the record names is `2` and the slot records carry the characters state 6 picked. Seed each to a sentinel that differs from the post-condition, and prove the assertion fails when the spawn is skipped.

- [ ] **Step 5: Re-measure the arena frame with live slots**

With fighters live, `0x263F4`'s per-side calls and the `game_frame` tail's `0x186D0`/`0x2A690`/`0x33F08` stop being inert. Add an assertion that the slot records change across a frame, so the later tasks measure a moving fight rather than a null one.

- [ ] **Step 6: Full ladder and commit**

Run: `make verify`
Expected: exit 0, 0 warnings, every oracle unmoved, and the front-end window still `257 frames, 0 unexplained`.

```bash
git add port/src/game/fighter.c port/src/game/fighter.h port/src/game/fight.c port/src/game/fight.h port/src/game/flow.c port/tests/test_fight.c docs/superpowers/plans/2026-09-20-demo-fight-derivations.md
git commit -m "fighter: port the demo fight's fighter spawn"
```

**Gate for this task:** the live-slot count is non-zero for two slots after state 6 and the arena frame's per-side calls are live — or a BLOCKED report carrying the resource evidence, which sends the cycle back to the human for the fallback.

---

### Task 7: Open the demo window (report-only)

**Files:**
- Modify: `Makefile`, `tools/title_compare.py` (approved), `port/tests/test_frontend.c`, `port/spec/game_flow.md`

**Interfaces:**
- Consumes: Task 5 — the demo is live in the dump run.
- Produces: `make demo-oracle`, a report-only target over a demo window; the measured divergence frame recorded in the spec.

- [ ] **Step 1: Raise the dump length**

`PR_FRONTEND_DUMP` currently dumps 900 frames from the front-end entry. State 6 arms a 900-frame timer, so the dump must cover the state-6 entry plus the demo. Raise `PR_FRONTEND_DUMP_FRAMES` in the driver (and its Makefile default if it has one) enough to cover it, and record the new value in the report. Do not remove the existing front-end window — it must keep reporting `257 frames, 0 unexplained`.

- [ ] **Step 2: Add the demo window to the comparator**

`tools/title_compare.py` gains a `--demo` mode: the same content alignment and the same clean/splice/transition/unexplained classification, over the region after the front-end window. Reuse the existing classification; do not add a new one. **The title path's behaviour and output format must stay byte-identical** — the title oracles' lines must not move. Report-only for now: it prints the window, the counts and the first unexplained frame, and exits 0.

- [ ] **Step 3: Add the ladder target**

In `Makefile`, beside `frontend-oracle`, add `demo-oracle` running the same dump and invoking `title_compare.py --demo`. It is **not** added to `verify`'s sequence yet — cycle 2 promotes it to a gate. It must skip cleanly when the capture directory is absent, like the other capture oracles.

- [ ] **Step 4: Measure**

Run: `make demo-oracle`
Expected: a demo window with counts, and the first unexplained frame. Record the window bounds (distinct indices and raw numbers), the frame count, and the first unexplained frame's index and raw number.

- [ ] **Step 5: Record what the measurement means**

In `port/spec/game_flow.md`'s states 6/7 section, record the window, the counts, the first unexplained frame, and — explicitly — that cycle 1 expects that frame to be the original's first landing hit, so the bound is a declared consequence of the cycle split, not an accident. If the first unexplained frame is **earlier** than any plausible first hit, say so: that is a defect in the motion layers and belongs back in Tasks 2–5, not in the pin loop.

- [ ] **Step 6: Full ladder and commit**

Run: `make verify`
Expected: exit 0, 0 warnings, every existing oracle unmoved (including the front-end window's `257 frames, 0 unexplained`).

```bash
git add Makefile tools/title_compare.py port/tests/test_frontend.c port/spec/game_flow.md
git commit -m "tests: open the demo window, report-only"
```

---

### Task 8: Port the per-fighter state dispatch (inserted — see Amendments)

This is the task that makes the demo move. Task 7's measurement showed the port's state-7 output frozen, and the controller's root-cause investigation found why: `fight_health_sync` (`0x34B6C`) ports only the `rec+0x52 == 6` case of the raw's dispatch, and the demo's fighters sit at `+0x52 == 0`, so they take the unported default and the AI mapper `fight_command_map` (`0x3B134`) is never called.

**Files:**
- Modify: `port/src/game/fight.c` (`fight_health_sync`'s `+0x52` dispatch) and, for the handlers, `port/src/game/fighter.c` / `fighter.h`
- Modify: `port/tests/test_fight.c`
- Modify: `docs/superpowers/plans/2026-09-20-demo-fight-derivations.md` — the table and the handlers are not yet derived; §7.10 currently covers `0x34B6C` as a single gap

**Interfaces:**
- Consumes: Task 3's `fight_hud_pass`/`fight_stance_pass`/`fight_command_map`, Task 6's spawn, and `actors.c`.
- Produces: the `+0x52` handlers the demo path enters, so `fight_command_map` runs and `DS_001088E0`/`E2` become non-zero.

- [ ] **Step 1: Derive the table and the handlers into the record**

The dispatch is at `0x34BF4`: `mov al,[ecx+0x52]; cmp al,0x15; ja 0x34C08; and eax,0xff; jmp dword ptr cs:[eax*4 + 0x34b14]`. The 22-entry table at `0x34B14` (`PTR_LAB_00034b14`, confirmed in Ghidra) is:

| `+0x52` | handler | calls |
|---|---|---|
| 0, and the `>0x15` default | `0x34C08` | `0x349C8` |
| 1 | `0x34C15` | `0x359E0` |
| 2 | `0x34C26` | `0x35C1C` / `0x35D20` |
| 3 | `0x34C46` | `0x35D7C` |
| 4 | `0x34C53` | `0x35F84` |
| 5 | `0x34C62` | `0x36430` |
| 6 | `0x34C73` | `0x1A978` (already ported as `fight_stance_pass`) |
| 7 | `0x34C80` | `0x399CC` |
| 8 | `0x34C8D` | `0x37464` |
| 12 | `0x34C9A` | — |
| 13 | `0x34CA9` | — |
| 17 | `0x34CB8` | — |
| 18 | `0x34CC7` | — |
| 19 | `0x34D22` | — |
| 20 | `0x34D69` | — |
| 21 | `0x34D78` | — |
| 9, 10, 11, 14, 15, 16 | `0x34D83` | the epilogue — a no-op |

Derive each handler the demo enters into the record in §8's shape (body, globals, arithmetic with widths, addresses, unit-test values). The port's existing comment naming `0x34B14` is correct — leave it.

- [ ] **Step 2: Establish which handlers the demo actually enters — do not port all 22 speculatively**

The measured starting point is `+0x52 == 0` (the default `0x349C8`). Follow what each handler does to `+0x52` to see whether the demo path cycles into other states, and port only the ones it enters. Leave the rest as §7.10 gaps with the evidence.

- [ ] **Step 3: Port the default handler first and prove it moves the demo**

Port `0x349C8` and whatever it needs, then add a test asserting that the command word becomes non-zero for a live fighter — the observable that proves the AI mapper now runs. Seed the command word to a sentinel differing from the post-condition, and prove the assertion fails when the handler is skipped.

- [ ] **Step 4: Port the remaining entered handlers**

One at a time, each with its own test values from Step 1, re-running the measurement between them.

- [ ] **Step 5: Re-measure the demo window**

Run `make demo-oracle` and report the new window, the counts, the first unexplained frame, and whether the port's state-7 frames now vary (previously one image across 899 frames). If the fighters now move and animate, say whether the new first-unexplained frame is a landing hit or a declared gap — the `0x494A8` dust gap (2 RNG per side) remains a candidate.

- [ ] **Step 6: Full ladder and commit**

Run: `make verify`
Expected: exit 0, 0 warnings, every oracle unmoved, and the front-end window still `257 frames, 0 unexplained`.

```bash
git add port/src/game/fight.c port/src/game/fighter.c port/src/game/fighter.h port/tests/test_fight.c docs/superpowers/plans/2026-09-20-demo-fight-derivations.md
git commit -m "fighter: port the per-fighter +0x52 state dispatch"
```

**Gate for this task:** the command words are non-zero during the demo and the port's state-7 frames vary — or a BLOCKED report carrying the evidence.

---

### Task 9: Pin the demo's determinism and record the cycle-1 bound

**Files:**
- Modify: `tools/title_pin.py` (approved), `Makefile`, `port/spec/game_flow.md`, `README.md`, `docs/superpowers/specs/2026-09-20-demo-fight-design.md`

**Interfaces:**
- Consumes: Task 7's window and first-unexplained measurement; Task 1 Step 5's answer on how often the AI draws from the RNG.
- Produces: the pins the demo needs, and the recorded bound that cycle 2 retires.

- [ ] **Step 1: Pin one site at a time**

For each unexplained frame inside the demo window, identify the responsible non-deterministic site (the record's Step 5 RNG call list is the starting point), add ONE entry to `PATCHES` in `tools/title_pin.py` in the existing `(offset, original_bytes, replacement_bytes)` shape with equal lengths, and keep the table's fail-closed byte verification. A pin is a determinism fix — a site where the original reads uninitialised or timing-dependent state — never a value chosen to make a frame match.

- [ ] **Step 2: Re-capture after every pin**

A pin changes the pinned original's own output, so the capture is stale the moment you add one:

```bash
make frontend-capture      # regenerates data/title-captures/frontend
make demo-oracle
```

- [ ] **Step 3: Stop on one of three conditions**

Stop when the demo window reports `0 unexplained`; **or** when the unexplained frames begin at the first landing hit — the declared cycle-1 bound, since no motion-layer pin can make a hit resolve; **or** at eight pins. Record which condition fired. Never add a fitted pin to force a match.

- [ ] **Step 4: Confirm the title window did not move**

The pin table is shared. Run `make title-oracle` and `make attract-oracle`: their lines must be byte-identical to `54 clean, 55 splice, 2 transition, 0 unexplained` / `54 clean, 57 splice, 0 unexplained` and `FIRST DIVERGENCE at capture frame 215`. If they move, the pin leaks into the title's behaviour: re-capture `title`/`title2` with `make title-capture`, confirm the oracles pass against the new captures, and report that reference change explicitly.

- [ ] **Step 5: Record the bound and the cycle's status**

In `port/spec/game_flow.md` and `README.md`: correct the states 6/7/8 framing (they are the attract demo, not the interactive match — the correction the spec opens with), record the demo window's numbers, the pins added with their addresses, the measured divergence frame and that it is the first landing hit, and state that cycle 2 owns collision, damage and the window's closure. In the design spec, mark cycle 1 complete and leave cycle 2's section standing.

- [ ] **Step 6: Full ladder and commit**

Run: `make verify`
Expected: exit 0, 0 warnings, every gate unmoved, and `frontend-oracle` still enforced at `0 unexplained`.

```bash
git add tools/title_pin.py Makefile port/spec/game_flow.md README.md docs/superpowers/specs/2026-09-20-demo-fight-design.md
git commit -m "tests: pin the demo's determinism and record its bound"
```

**Gate for this cycle:** the demo window reports clean from its start to the measured divergence frame, that frame is the first landing hit, the pins are all determinism fixes, every helper has mutation-proof unit tests, and every unported piece is a named gap. Cycle 2 then implements combat and retires the bound.

---

## Self-Review

**Spec coverage.** The spec's cycle 1 is "the derivation, the fight camera and projection, the arena and fighter render, the think/AI chain, the state 6/7 handlers, the 900-frame timer and the `0x11BCC` exit, and the `game_frame` tail minus its combat calls" plus the provisional oracle. Task 1 covers the derivation (including open question 2, the RNG-consumption answer, and open question 3's `0x49C78` pinnability). Task 2 the camera and projection. Task 3 the arena and fighter render. Task 4 the think chain. Task 5 the states, the tail minus `0x3BB90`, and the timer exit. Task 6 (inserted — see Amendments) the fighter spawn, without which the arena frame runs on null slots and the bound below is unreachable. Task 7 the provisional, report-only window. Task 8 (inserted) the per-fighter +0x52 state dispatch, without which the AI mapper never runs and the demo never moves; Task 9 the pins and the recorded bound. The spec's out-of-scope list — the mode graph, the coin divert, `0x1EEB0`, `0x1F458`, `0x1EA08`'s remaining sites, the player screens and the human input mapping — is not implemented by any task and is recorded as unowned in Task 9 Step 5. The camera-chain deferral the previous cycle left is retired by Task 2, which the spec calls out.

**Deliberate deferrals, stated not hidden.** `0x3BB90` is the only combat call in cycle 1's scope and is explicitly skipped in Task 5 Step 4 with a `/* PORT: */` marker and a reason. The HUD/health path (`0x35658`, `0x33F08`, `0x1D890`) is ported only as far as Task 1's record shows it is needed for motion; the rest is cycle 2's. No task invents a value Task 1 has not derived, and no task ports a premise the spec's correction refutes.

**Placeholder scan.** Every step names its file, its command, and either the exact values to assert or the record section that supplies them. The two test snippets that depend on Task 1 (Task 2 Step 2's projection pairs, Task 4 Step 1's command mapping) name their values as constants with the record section that defines them, and each says explicitly that a missing anchor is a Task 1 defect rather than licence to invent a value — inventing one would be the fitted constant this project forbids. No `TBD`, no "similar to Task N", no step that describes an action without its code or command.

**Type consistency.** `camera_init`, `camera_project`, `camera_dispatch` and `camera_scene_step` are declared in Task 2 and used unchanged in Tasks 3 and 5. `fight_arena_frame` and `fighter_render` are declared in Task 3, `fighter_think` in Task 4; Task 5 calls all three. `game_state_step`'s cases 6/7 are the raw's inline form, matching how case 5 is written today. Globals are referenced by their `symbols.h` names throughout (`DS_000F0A64`, `DS_000F0A6A`, `DS_000F0A6C`, `DS_000F0A6F`, `DS_000F0A72`, `DS_00104B15`, `DS_00104B19`, `DS_00104528`, `DS_001082CC`, `DS_00100B60`/`B61`, `DS_00100AF0`/`AF4`, `DS_001077B0`/`B4`, `DS_001077DC`/`E0`/`E4`, `DS_001077E8`, `DS_0010787C`, `DS_001088E0`/`E2`, `DS_001088E4`, `DS_00104AE8`).
