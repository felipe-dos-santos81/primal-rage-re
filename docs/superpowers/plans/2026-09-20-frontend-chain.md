# Front-End Chain (States 3-5) and Effect Render Path — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Port states 3, 4 and 5 and the effect render path so the port advances from the
select carousel through the front-end into the match-start handoff, with the effects
those states spawn actually drawing, verified against a capture of the original.

**Architecture:** The three states join `port/src/game/flow.c` as phase machines beside
the existing title and select handlers, transcribing the raw's phase counters
(`DS_000F0A6F` for state 3, `DS_0009AD98` for state 4) rather than restructuring them.
The effect render layer joins `port/src/game/effects.{c,h}` on the existing
spawn → step → palette dirty list → `gfx_dac` chain, and its call sites register into the
process tables `game_frame` already runs. Verification is front-loaded: the capture,
behaviour pins and oracle come before any state code, with a declared fallback.

**Tech Stack:** C (C99) against SDL3, CMake, Python 3 standard library for the
capture/compare tooling, DOSBox-X for the reference run.

**Spec:** `docs/superpowers/specs/2026-09-20-frontend-chain-design.md`

## Global Constraints

- Fixed profile; register-level fidelity. Original state lives in `mem[]`; pointer globals are `mem[]` offsets consumed as `mem + DSD(...)`.
- SDL appears only in `port/src/host.c` and `port/src/main.c`.
- `data/` is read-only and git-ignored. `tools/` existing files are read-only **except `tools/title_pin.py` and `tools/title_compare.py`**, carved out this cycle by explicit approval. `tools/title_capture.py` and `tools/smk_capture.py` are **not** modified — the longer capture needs only a new Makefile target.
- Comments in `port/src` are only `/* PORT: ... */`, `/* TODO(verify): */`, and address tags. No comments in `port/tests` beyond the existing style.
- Build with **0 warnings**. No new dependencies. **Never ship a fitted constant** — every value is derived from the raw or the capture, or is a declared gap.
- Tests: one `int test_X(void)` per file, declared in `port/tests/test.h` and called from `port/tests/run_tests.c`; only `CHECK(cond,msg)` and `CHECK_EQ_INT(a,b)`. `port/src` is a PUBLIC include dir. `port/CMakeLists.txt` lists sources explicitly.
- `game_init()` may run only once per process; any test that calls it must be env-gated (`PR_*_DUMP`).
- Capture oracles **skip cleanly** when their capture directory is absent — never fail.
- Oracles that must not move: the title oracle, the attract oracle, `smk_compare`, and `oracle C-vs-Python: 9866 writes byte-exact`. The capture oracle's first difference is currently `C write 430`.
- `make verify` must pass at the end of every task whose change can move an oracle.
- On plan-vs-raw conflict, the RAW wins; record the correction.
- Never `git add -A`. Commit style: `<area>: <what changed>`.

## File Structure

| file | responsibility |
|---|---|
| `tools/title_pin.py` (modify, approved) | + the front-end behaviour pins, same fail-closed patch table |
| `tools/title_compare.py` (modify, approved) | + a front-end window, same alignment/splice classification |
| `Makefile` (modify) | `frontend-capture`, `frontend-oracle` targets; `verify` gains the oracle |
| `port/src/game/flow.c` (modify) | `game_state_step` cases 3/4/5; the three state handlers |
| `port/src/game/effects.{c,h}` (modify) | the camera/scene layer and the effect draw |
| `port/src/game/actors.c` (modify, only if the wiring needs an actor helper) | `0x29B74`/`0x41578` if they belong with the actor list |
| `port/tests/test_frontend.c` (modify) | frame emission for states 3/4; determinism log extension |
| `port/tests/run_tests.c`, `port/tests/test.h` (modify) | registration if a new test file is added |
| `docs/superpowers/plans/2026-09-20-frontend-chain-derivations.md` (new) | the raw-byte derivation record (Task 2) |
| `port/spec/game_flow.md` (modify) | dispositions and the declared gaps |

---

### Task 1: Extend the capture, the pins and the front-end oracle

This is the de-risking task: it must come before any state code. It has an explicit
**stopping condition** and fallback.

**Files:**
- Modify: `Makefile`, `tools/title_pin.py`, `tools/title_compare.py`, `port/tests/test_frontend.c`, `port/tests/run_tests.c`
- Read (do not modify): `tools/title_capture.py`, `tools/smk_capture.py`

**Interfaces:**
- Produces: `make frontend-capture` (writes `data/title-captures/frontend/`), `make frontend-oracle`, and a port frame dump for states 3/4 that the oracle consumes.
- Consumes: the existing `PR_FRONTEND_DUMP` gate and `tools/title_capture.py --time-limit`.

- [ ] **Step 1: Add the longer capture target**

In `Makefile`, first add the dump directory beside the existing two (line 15-16):
`FRONTEND_DUMP = /tmp/pr_frontend_dump`. Then, beside `title-capture`:

```makefile
frontend-capture: title-pin ## Capture the pinned original's front-end region (Task 1 front-end chain)
	$(PYTHON) tools/title_capture.py --out $(TITLE_CAPTURES)/frontend --time-limit 120
```

Run: `make frontend-capture`
Expected: `wrote 3xxx frames to data/title-captures/frontend` (the spike produced 3708 at
120 s; exact count varies). No tool edit is needed — `title_capture.py` already takes
`--time-limit` and `--out`.

- [ ] **Step 2: Make the port's front-end dump emit frames**

`test_frontend.c`'s `PR_FRONTEND_DUMP` driver currently writes only `select.log`
(hashes). The oracle needs pixel data. Extend the driver so that, once the state machine
reaches state 3, it writes each frame as `frame_%04d.raw` (192000 bytes, 320x200 RGB24)
alongside the existing log, using the same buffer the title dump writes. Follow the
`test_title_window` / `PR_TITLE_DUMP` pattern in `port/tests/test_title.c` for the frame
write; do not add a new dump mechanism.

Run: `PR_FRONTEND_DUMP=/tmp/pr_fe PR_GAME_DIR=data/game/C ./build/run_tests 2>&1 | grep -i frontend`
Expected: the driver reports the frame it reaches state 3 on, and `/tmp/pr_fe` contains
`.raw` frames.

- [ ] **Step 3: Drive the pin loop to convergence**

The port's state-3 frames will not match the capture yet — the front-end's RNG draws are
unpinned, exactly as the title's were. Iterate:

1. Run the compare (Step 4) and find the **first** capture frame the port cannot explain.
2. Identify the RNG/behaviour site responsible (the title precedent is
   `tools/title_pin.py`'s four sites: three entry draws and the anim opcode-8 handler).
3. Add a new entry to `PATCHES` in `tools/title_pin.py`, following the existing
   `(offset, original_bytes, replacement_bytes)` shape with equal lengths. The table
   verifies every original byte before writing and fails closed — keep that property.
4. Re-run `make title-pin && make frontend-oracle` and repeat.

**Stopping condition:** stop when the front-end window reports `0 unexplained`, or when
**eight** pins have been added without reaching it. Record each pin with the address and
the raw draw it replaces.

- [ ] **Step 4: Add the compare window and the ladder target**

Extend `tools/title_compare.py` with a front-end mode that takes the port's frame
directory and the `frontend` capture and applies the existing content-alignment plus
clean/splice/unexplained classification over the states 3/4 region. Do not change the
title window's behaviour or its output format — the title oracle's line must stay
byte-identical.

In `Makefile`, add beside `title-oracle`:

```makefile
frontend-oracle: build ## Pixel-exact front-end oracle (states 3/4; skips without data/title-captures/frontend)
	@echo "== front-end oracle (pixel-exact, states 3/4) =="
	@if [ -d $(TITLE_CAPTURES)/frontend ]; then \
		rm -rf $(FRONTEND_DUMP); \
		PR_FRONTEND_DUMP=$(FRONTEND_DUMP) PR_GAME_DIR=$(GAME_DIR) ./$(BUILD_DIR)/run_tests; \
	else \
		echo "frontend-oracle: no capture at $(TITLE_CAPTURES)/frontend, frames not compared"; \
	fi
	@$(PYTHON) tools/title_compare.py --frontend --capture $(TITLE_CAPTURES)/frontend \
		--port $(FRONTEND_DUMP)
```

Add `frontend-oracle` to the `verify` target's sequence, after `title-oracle`.

- [ ] **Step 5: Implement the fallback if the pin does not converge**

If Step 3 hit its stopping condition without `0 unexplained`, do **not** keep grinding and
do **not** add a fitted pin. Instead:

- Record in `port/spec/game_flow.md` (in the states section) that states 3/4 carry **no
  pixel oracle**, naming the first unexplainable frame and the pins attempted.
- Extend the `PR_FRONTEND_DUMP` determinism log through states 3/4 so two runs must
  produce byte-identical frame hashes, and add that check to `run_tests`.
- Keep the `frontend-oracle` target in place: it skips or reports, and stays the tool
  that a later cycle picks up.

- [ ] **Step 6: Run the ladder and commit**

Run: `make verify`
Expected: exit 0, `all checks passed`, 0 warnings, the title/attract/smacker oracles and
`oracle C-vs-Python: 9866 writes byte-exact` unchanged.

```bash
git add Makefile tools/title_pin.py tools/title_compare.py port/tests/test_frontend.c port/tests/run_tests.c port/spec/game_flow.md
git commit -m "tests: capture the front-end region and pin it"
```

**Gate for this task:** `make frontend-capture` produces a capture reaching states 3/4;
the port's front-end frames are emitted; either the window reports `0 unexplained`, or
the fallback is implemented and recorded. Nothing in Task 2+ starts before this resolves.

---

### Task 2: Derive the uncharacterized functions

**Files:**
- Create: `docs/superpowers/plans/2026-09-20-frontend-chain-derivations.md`

**Interfaces:**
- Produces: a record the later tasks implement from — the exact semantics of `0x2F4BC`,
  `0x12658`, `0x1EA08`, `0x29B74`, `0x41578` and the camera/scene functions
  `0x1317C`/`0x1324C`/`0x13290`/`0x1333C`, each with addresses and cited bytes.

- [ ] **Step 1: Re-derive `0x2F4BC`**

`0x2F4BC` is called 8, 12 and 13 times in state 4's phases 0/1/2. Disassemble it
(`capstone`, `Cs(CS_ARCH_X86, CS_MODE_16)` over the bytes at that image-relative offset;
`data/game/C/PRAGE.EXE` is read-only, and obj-0 file offset = VA + `0x52E54`) and record:
its arguments at each call site, what it allocates or spawns, and why the count differs
per phase (8/12/13).

- [ ] **Step 2: Re-derive `0x12658`**

Record what it spawns (`0x2AE14` twice with different arguments), the field copy
(`+0x4b`), and which `DS_000F0A58` record it binds.

- [ ] **Step 3: Re-derive `0x1EA08`**

Called only by state 5. Record its full body and every global it writes.

- [ ] **Step 4: Re-derive the two effect call sites**

`0x29B74` and `0x41578` both walk the front-end list via `0x33904` and call `0x13D4C`
for records matching `0x3E688` (and `0x88874B0` in `0x41578`). Record: which process table
each registers into (`DS_000A8644`/`DS_00104AE8` update vs `PTR_FUN_000A86C4`/
`DS_00104AEC` render), the exact list predicate, and the `+0x0` handle meanings.

- [ ] **Step 5: Re-derive the camera/scene functions**

`0x1317C`, `0x1324C`, `0x13290`, `0x1333C` and their `DS_000F0AEC`/`DS_000F0AF0`/
`DS_000F0AF4` state. Record each function's body, what the three globals hold across
frames, and which one draws — this is the missing half of the effect render path.

- [ ] **Step 6: Record what could not be determined**

Any function whose semantics depend on state the port does not model is listed as a named
gap with the evidence — not guessed.

- [ ] **Step 7: Give each helper its unit-test values**

For every function derived in Steps 1-5, record the concrete input and the exact expected
output or global transition that its porting task's unit test must assert — the same shape
`2026-09-20-midi-controllers-derivations.md` used for its anchors. A helper whose values
cannot be pinned is listed in Step 6 instead. This is what satisfies the spec's "unit
tests for every newly derived helper"; without it the porting tasks have nothing concrete
to assert.

- [ ] **Step 8: Commit**

```bash
git add docs/superpowers/plans/2026-09-20-frontend-chain-derivations.md
git commit -m "docs: derive the front-end chain's uncharacterized functions"
```

---

### Task 3: Port state 3 (`0x12484`)

**Files:**
- Modify: `port/src/game/flow.c` (`game_state_step` case 3)
- Test: `port/tests/test_frontend.c`

**Interfaces:**
- Consumes: Task 1's oracle frames, Task 2's record, the existing `game_state_select`, `frontend_list_next` (`0x33904`), `effects_spawn` (`0x13C70`), `actor_spawn` (`0x2AE14`).
- Produces: `static void game_state_3(void);`

- [ ] **Step 1: Write the failing test**

In `test_frontend.c`, reuse the existing "seed exactly one live entry" block that the
state-2 driver already uses (it writes the record at `DS_00107608` with stride `0x10`) and
give that record the handle `0x3E688` in its first dword, so state 3's list walk matches
it. Then assert both phases (raw `0x12484`):

```c
/* Phase 0: the list walk spawns a type-3 effect for the matching record, the
 * DS_00104528 bit selects the actor branch, and DS_000F0A6F advances. */
{
    seed_frontend_list(0x3E688u);              /* existing seeding block, handle set */
    DSB(DS_00104B1D) = 0;
    DSW(DS_000F0A64) = 3;
    DSW(DS_000F0A6A) = 1;
    DSB(DS_000F0A6F) = 0;
    DSB(DS_00104528 + 1u) |= 2u;               /* take the DS_000F0A40 branch */
    DSD(DS_000F0A40) = 0;
    game_state_step();
    CHECK_EQ_INT(DSB(DS_000F0A6F), 1);
    CHECK(DSD(DS_000F0A40) != 0u, "phase 0 spawned through 0x2AE14");
    CHECK(effects_active() != 0, "phase 0 spawned a type-3 effect");
}

/* Phase 1 terminate: |[cam+0x1C] - 0x1E00| == 0 <= b, so it hands to state 9 with
 * DS_000F0A6C = 6. */
{
    u32 cam = DSD(DS_000F0A58);
    DSD(cam + 0x1Cu) = 0x1E00u;
    DSW(cam + 0x36u) = 0;                      /* high word of [cam+0x34], so b = 0 */
    DSB(DS_000F0A6F) = 1;
    game_state_step();
    CHECK_EQ_INT(DSW(DS_000F0A64), 9);
    CHECK_EQ_INT(DSW(DS_000F0A6C), 6);
    CHECK_EQ_INT(DSW(DS_000F0A6A), 0xF0);
    CHECK_EQ_INT(DSD(cam + 0x1Cu), 0x1E00);
}
```

If the seeding block cannot carry a settable handle, add one parameter to it rather than
duplicating the block — it is shared with the state-2 driver.

- [ ] **Step 2: Run it and watch it fail**

Run: `make build && ./build/run_tests 2>&1 | grep -i "state 3"`
Expected: FAIL — case 3 is still a no-op stub.

- [ ] **Step 3: Implement the handler**

Replace case 3's stub with `game_state_3();` and add the handler beside
`game_state_title`. The raw (`0x12484`, decompiler `:1501-1573`) has exactly **two**
phases on `DS_000F0A6F`, in this order:

**Phase 0** (`DS_000F0A6F == 0`):
1. `0x2BAF4()`
2. `0x38B18()` — four times in a row (decompiler `:1510-1516`)
3. The list walk: `p = 0x33904(); while (p != 0) { if (DSD(p) == 0x3E688) 0x13C70(); p = 0x33904(); }`
4. If `(DSB(DS_00104528 + 1) & 2) == 0`: `0x1C500(); 0x2F198(); 0x1C500(); 0x2F198();`
   else: `DSD(DS_000F0A40) = 0x2AE14(0);` (the decompiler's `_DAT_000f0a40` overlap)
5. `0x12658()`
6. `DSB(DS_000F0A6F) += 1`, then `return` — phase 0 returns and never falls through.

**Phase 1** (`DS_000F0A6F == 1`), with `cam = DSD(DS_000F0A58)` captured at entry:
1. `DSW(DS_00107A38) = DSW(DS_00107A44) >> 6`
2. `v = 0x5A00 - DSD(cam + 0x1C)`, take `|v|` (the raw's `if (v < 0) v = -v`), store the
   low 16 bits to `DSW(DS_00107A44)`
3. `a = |DSD(cam + 0x1C) - 0x1E00|`
4. `b = DSW(cam + 0x36) < 0 ? -(DSD(cam + 0x34) >> 16) : (DSD(cam + 0x34) >> 16)`
5. If `a <= b`: `DSD(cam + 0x24) = 0x40C00000; DSW(DS_000F0A6C) = 6;
   DSD(cam + 0x1C) = 0x1E00; DSW(DS_000F0A64) = 9; DSW(cam + 0x36) = 0;
   DSW(DS_000F0A6A) = 0xF0; DSW(DS_000F0A72) = 0;`
6. `return` (phase 1 stays in phase 1 until the terminate condition fires).

Declare `static void game_state_3(void);` beside `game_state_title` and call it from
`game_state_step`'s case 3. The body is the two phase blocks above, transcribed literally.

Keep the raw's order and its `DS_000F0A58` record arithmetic exactly. Use the port's
existing names for `0x33904`, `0x13C70`, `0x2AE14`; any helper with no port name yet is
added by Task 2's record — do not invent one here.

- [ ] **Step 4: Run the tests**

Run: `make verify`
Expected: the new assertions pass; the front-end oracle advances (or, under the Task 1
fallback, the determinism log extends) and no other oracle moves.

- [ ] **Step 5: Commit**

```bash
git add port/src/game/flow.c port/tests/test_frontend.c
git commit -m "frontend: port state 3"
```

---

### Task 4: Port state 4 (`0x11578`)

**Files:**
- Modify: `port/src/game/flow.c` (`game_state_step` case 4)
- Test: `port/tests/test_frontend.c`

**Interfaces:**
- Consumes: Task 2's record and `0x2F4BC` semantics; the existing `0x2C3FC`, `0x4F1E4`, `0x2BAF4`, `0x2C06C`, `0x2AE14`.
- Produces: `static void game_state_4(void);`

- [ ] **Step 1: Write the failing test**

Assert the phase machine's observable state (raw, decompiler `:1027-1099`): phase 0 sets
`DS_0009AD98 = 4` and `DS_000F0A76 = 0xB4` with `DS_000F0A74 = 1`; phase 4 with a
non-zero timer decrements it; phase 4 at zero jumps to the continuation phase; phase 3
sets `DS_000F0A6C = 0`, `DS_000F0A64 = 9`, `DS_000F0A6A = 1`, `DS_0009AD98 = 0`.

- [ ] **Step 2: Run it and watch it fail**

Run: `make build && ./build/run_tests 2>&1 | grep -i "state 4"`
Expected: FAIL — case 4 is a stub.

- [ ] **Step 3: Implement the handler**

The raw (`0x11578`, decompiler `:1027-1099`) switches on `DS_0009AD98`. Transcribe all
five cases in this exact order — the call sequences and counts are the raw's:

**case 0:** `0x2C3FC`, `0x4F1E4`, `0x2BAF4`, `0x2C06C`, `0x2AE14(0)`, then `0x2F4BC`
**eight** times; `DS_000F0A76 = 0xB4`; `DS_000F0A74 = 1`; `DS_0009AD98 = 4`; return.

**case 1:** `0x2C3FC`, `0x4F1E4`, `0x2BAF4`, `0x2AE14(0)`, then `0x2F4BC` **twelve**
times; `DS_0009AD98 = 4`; `DS_000F0A76 = 0xB4`; `DS_000F0A74 = 2`; return.

**case 2:** `0x2C3FC`, `0x4F1E4`, `0x2BAF4`, `0x2AE14(0)`, then `0x2F4BC` **thirteen**
times; `DS_000F0A76 = 0xB4`; `DS_000F0A74 = 3`; `DS_0009AD98 = 4`; return.

**case 3:** `DS_000F0A6C = 0`; `DS_000F0A64 = 9`; `DS_000F0A6A = 1`; `DS_0009AD98 = 0`;
return.

**case 4:** `old = DS_000F0A76; DS_000F0A76 = old - 1; if (old == 0) DS_0009AD98 =
DS_000F0A74;` then return. (The raw tests the value *before* the decrement — `bVar2 =
DAT_000f0a76 == 0` is read before the store — so the continuation fires on the frame the
counter is already zero, not after it wraps.)

Declare `static void game_state_4(void);` beside `game_state_title` and call it from
`game_state_step`'s case 4. The body is the five cases above, transcribed literally — the
call counts and the `0x2C06C` asymmetry are the raw's, not a loop bound to invent.

Note `0x2C06C` appears in **case 0 only** — cases 1 and 2 do not call it.

- [ ] **Step 4: Run the tests**

Run: `make verify`
Expected: assertions pass; the oracle advances past state 3; no other oracle moves.

- [ ] **Step 5: Commit**

```bash
git add port/src/game/flow.c port/tests/test_frontend.c
git commit -m "frontend: port state 4"
```

---

### Task 5: Port state 5 (match start)

**Files:**
- Modify: `port/src/game/flow.c` (`game_state_step` case 5)
- Test: `port/tests/test_frontend.c`

**Interfaces:**
- Consumes: Task 2's record for `0x1EA08`; the existing `0x2C3FC`, `0x2C06C`, `0x32970`.
- Produces: the case-5 body (no new function needed — the raw inlines it).

- [ ] **Step 1: Write the failing test**

Assert: after case 5 runs, `DS_000F0A72 == 0`, `DS_000F0A6C == 6`, `DS_000F0A64 == 9`, and
the three tails ran.

- [ ] **Step 2: Run it and watch it fail**

Run: `make build && ./build/run_tests 2>&1 | grep -i "state 5"`
Expected: FAIL — case 5 is a stub.

- [ ] **Step 3: Implement the case**

The raw (`0x11D04` case 5, decompiler `:1227-1236`) is inline, in this order:

1. `0x2C3FC()`
2. `0x1EA08()`
3. `0x2C06C()`
4. `0x32970()` (the port already calls this via `host.c`)
5. `DSD(DS_000F0A72) = 0`
6. `DSW(DS_000F0A6C) = 6`
7. `DSW(DS_000F0A64) = 9`
8. `DSW(DS_000F0A6A) = <CX>` and `DSB(DS_000F0A6F) = <DH>`
9. The three tails run in `game_state_step`'s shared tail — do **not** duplicate them here.

**Ambiguity to resolve before writing this case, not after:** steps 8's two values arrive
as the decompiler's `extraout_CX`/`extraout_DH`, i.e. register values left by one of the
calls in steps 1-4 — the decompiler could not name them. Task 2's record must establish
which call produces them and what they are. If Task 2 cannot pin it, this is a named gap:
implement steps 1-7, leave `DS_000F0A6A`/`DS_000F0A6F` as the raw sets them from whatever
the port's corresponding call returns, and record it. Do **not** substitute a literal.

Use the port's real names for the calls; `0x1EA08` and `0x2C3FC` come from Task 2's record
and the existing announcer helper respectively.

- [ ] **Step 4: Run the tests**

Run: `make verify`
Expected: assertions pass; `DS_000F0A64` reaches 9 and the subsequent countdown lands on
state 6, which remains the declared stub; no other oracle moves.

- [ ] **Step 5: Commit**

```bash
git add port/src/game/flow.c port/tests/test_frontend.c
git commit -m "frontend: port state 5 (match start)"
```

---

### Task 6: Effect render path

**Files:**
- Modify: `port/src/game/effects.{c,h}`
- Test: `port/tests/test_effects.c`

**Interfaces:**
- Consumes: Task 2's record of `0x1317C`/`0x1324C`/`0x13290`/`0x1333C` and the three globals.
- Produces: the render entry the process tables and the frame loop call.

- [ ] **Step 1: Write the failing test**

Unit-test the camera/scene functions against the record's derived values — for each, a
known input state and the exact expected output/global transition. Include one test that
proves a spawned effect now changes the presented palette (not just the dirty list), which
is the gap this task closes.

- [ ] **Step 2: Run it and watch it fail**

Run: `make build && ./build/run_tests 2>&1 | grep -iE "camera|scene|render"`
Expected: FAIL — the functions do not exist.

- [ ] **Step 3: Implement the layer**

Add the camera/scene functions and their `mem[]` globals to `effects.c`, following the
existing style (raw order, raw arithmetic, `/* PORT: 0x... */` tags). Wire the draw so it
runs in the render pass the way the raw does, not from `effects_step`.

- [ ] **Step 4: Run the tests**

Run: `make verify`
Expected: the new tests pass; the front-end oracle improves (state 3's spawned effects
now draw); no other oracle moves.

- [ ] **Step 5: Commit**

```bash
git add port/src/game/effects.c port/src/game/effects.h port/tests/test_effects.c
git commit -m "effects: port the camera/scene render path"
```

---

### Task 7: Wire the effect call sites and close the docs

**Files:**
- Modify: `port/src/game/effects.c` or `port/src/game/actors.c` (wherever the list walk belongs), `port/src/game/flow.c`, `port/spec/game_flow.md`, `README.md`
- Test: `port/tests/test_frontend.c`

**Interfaces:**
- Consumes: Task 2's record of `0x29B74`/`0x41578` and the existing `run_process_table`.
- Produces: the two entries registered into the update and render process tables.

- [ ] **Step 1: Write the failing test**

Assert that after the registration runs, the update table's mask includes the `0x29B74`
entry and the render table's includes `0x41578`, and that a table run with each enabled
walks the list and spawns for a matching record.

- [ ] **Step 2: Run it and watch it fail**

Run: `make build && ./build/run_tests 2>&1 | grep -iE "process table|0x29B74|0x41578"`
Expected: FAIL — neither site is registered.

- [ ] **Step 3: Implement the wiring**

Register both functions into the tables `game_frame` already runs, matching the raw's
registration sites and masks. Do not call them ad hoc from the state handlers.

- [ ] **Step 4: Update the docs**

In `port/spec/game_flow.md`: move states 3/4/5 out of the stub list, record each state's
ported phases, and state the effect render path's status. Record the pixel-oracle outcome
(`0 unexplained`) or the declared fallback from Task 1. Note that states 6/7/8 and 9's
semantics remain the next cycle. Update `README.md`'s status paragraph.

- [ ] **Step 5: Full ladder**

Run: `make verify`
Expected: exit 0, 0 warnings, `all checks passed`, `symbols.h` byte-identical, the
C-vs-Python gate at 9866 byte-exact, and the title/attract/smacker oracles unchanged.

- [ ] **Step 6: Commit**

```bash
git add port/src/game/flow.c port/src/game/effects.c port/src/game/actors.c port/spec/game_flow.md README.md port/tests/test_frontend.c
git commit -m "frontend: wire the effect call sites and record the chain"
```

(Stage whichever of `effects.c`/`actors.c` actually received the list walk; do not stage a
file the task did not touch.)

---

## Self-Review

**Spec coverage.** Scope §4: states 3/4/5 → Tasks 3/4/5; the render path → Task 6; the
call sites → Task 7; the capture/pin/oracle → Task 1. Architecture §5: handlers in
`flow.c` (Tasks 3-5), render in `effects.c` (Task 6), process-table wiring (Task 7), the
tools carve-out (Task 1). Interfaces §6: the new derivations → Task 2 Steps 1-5, with each
helper's unit-test values → Task 2 Step 7; the already-ported functions are named in each
task's Interfaces block. Data flow §7: transcribed in Tasks 3/4/5 Step 3. Sequencing §8 →
task order. Verification §9: Task 1 (primary oracle + declared fallback), Tasks 3-6
(helper and camera/scene unit tests), Task 7 Step 5 (non-regression). Risks §10 → Task 1's
stopping condition, Task 5's ambiguity note and Task 2 Step 6. Open questions §11 →
resolved in Task 1.

**Deferral to Task 2, stated not hidden.** Tasks 3-6 name the helpers they consume but
the raw semantics of `0x2F4BC`, `0x12658`, `0x1EA08`, `0x29B74`, `0x41578` and the
camera/scene functions are produced by Task 2 — the same shape the previous cycle used for
its derivation task. No task invents a value Task 2 has not derived.

**Type consistency.** `game_state_3`/`game_state_4` are declared once (Tasks 3, 4);
`frontend_list_next` (`0x33904`), `effects_spawn` (`0x13C70`), `actor_spawn` (`0x2AE14`),
`config_set_credit_row` (`0x2C06C`) and the three tails are the port's existing names and
are used identically in every task. The globals (`DS_000F0A64/6A/6C/6F/72/74/76`,
`DS_0009AD98`, `DS_00107A38/44`, `DS_000F0A58`) are referenced by their `symbols.h`
names throughout.
