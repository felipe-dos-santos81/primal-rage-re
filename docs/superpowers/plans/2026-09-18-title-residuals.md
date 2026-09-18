# Title-Path Residuals (4a-iii) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Close the two title-path residuals the title oracle can reach — port `0x13C70` with its effect-list slice, and make the oracle cover the true un-pinned original by removing the `0x2BF08` inert pin — so the title oracle's coverage claim becomes true.

**Architecture:** A new `port/src/game/effects.{c,h}` owns the `0x13xxx` effect list (sentinels, lock, active count, spawn `0x13C70`, clear `0x13DF0`) over the two intrusive list primitives. `0x2BF08` is implemented in `flow.c`'s existing `0x11D04` tail using already-ported text functions. `tools/title_pin.py` loses its fifth site and the capture is re-derived.

**Tech Stack:** C99 (fixed profile), CMake, python3 (capstone for disassembly), DOSBox-X capture, the existing `make verify` ladder.

## Global Constraints

- Base is `main` at `769b272`. Spec: `docs/superpowers/specs/2026-09-18-title-residuals-design.md`.
- Spec rule (Decision 5): every ported item either moves the title oracle and is proven by it, or is recorded as an explicit coverage gap with a unit-level proof. No silent unproven code.
- LE code-object mapping: `file_offset = va + 0x52E54` (obj0).
- `mem[]` holds original state at original offsets; pointer globals are `mem + DS_*(...)`. Never write `mem[0xA0000]`.
- SDL only in `port/src/host.c` and `port/src/main.c`. No new dependencies. Build 0 warnings.
- `data/` is read-only; nothing under it may be modified.
- Comments in `port/src` only `/* PORT: ... */`, `/* TODO(verify): ... */`, and address tags. No prose comments elsewhere.
- Tests: one `int test_X(void)` per file, declared in `port/tests/test.h`, called from `port/tests/run_tests.c`; only `CHECK(cond,msg)` and `CHECK_EQ_INT(a,b)`. New test sources added explicitly to `port/CMakeLists.txt`.
- `port/src/symbols.h` must regenerate byte-identically: `python3 tools/gen_symbols.py port/decomp port/src/symbols.h`.
- `title_pin.py` fails closed: equal-length replacement bytes, refuses to write over the source or under `data/`.
- Git: stage explicit paths only, never `git add -A`. Commit style `<area>: <what changed>`.
- The ladder: `make verify` (build + headless `--check 60` + `PR_ORACLE_REQUIRED=1` run_tests + `make smk-oracle` + `make title-oracle` + GRA oracle + `symbols.h` idempotence).
- `data/title-captures/` is git-ignored; captures are never committed.

---

## Errata — re-scope after Task 1 (the un-pinned gate tripped WIDER)

Task 1 measured **total** divergence between the un-pinned original and the port:
`title` 587 and `title2` 590 captured frames, 100% unexplained, 0/96 port frames
exhibited, no window; deterministic (both captures agree); pinned-backup control
reproduces the known-green window. Near-miss margins of 0.26%–4.7% of bytes
varying per frame.

Consequences, per the spec's Decision 4:

* Task 5's premise — "port `0x2BF08` only if drift lands at frames 32/64/96" — is
  **falsified**. The 4a-ii hypothesis that `0x2BF08` reaches the aperture only
  with an active message is wrong at the aperture level: it composites a small
  time-varying overlay on effectively **every** title frame.
* The original Tasks 2–6 were **suspended** by this errata; Task 2's diagnosis
  has now re-specified them. Current schedule: Task 1 and Task 1b (done),
  Task 2 (done, the diagnosis), **Task 3** (overlay port + seeded inputs),
  **Task 4** (effect register bindings), **Task 5** (list primitives + effects
  module), **Task 6** (wire), **Task 7** (port `0x134C0` step/age — added after
  Task 6's review found the count never drains), **Task 8** (falsifiability
  record + ladder). The old conditional Task 5 is obsolete (see its note).

The replacement schedule follows. Tasks 1's commit (`33a74e9`) and the un-pinned
captures are retained.

### Task 1b: make the title oracle report total non-alignment instead of crashing

`tools/title_compare.py:241` does `a, b = idx[0], idx[-1]` where `idx` lists the
frames exhibiting any port frame. With zero exhibited frames `idx` is empty and
it raises `IndexError`, so the oracle cannot report the very condition Task 1
produced. This is load-bearing for any future re-capture that diverges.

**Files:**
- Modify: `tools/title_compare.py` (the window derivation in `check_capture`)
- Test: `tools/tests/test_title_compare.py` (create if absent)

**Interfaces:**
- Produces: `check_capture`-level behaviour that reports `0 exhibited` and a
  non-zero exit when no captured frame exhibits any port frame, instead of
  raising.

- [ ] **Step 1: Write the failing test**

Follow the existing test style under `tools/tests/`. Add a case with a synthetic
capture whose every frame is unexplained and assert the checker reports zero
exhibited frames and fails cleanly (non-zero), not with a traceback.

- [ ] **Step 2: Run it and confirm it fails with `IndexError`**

Run the new test. Expected: FAIL with the `IndexError` traceback reproduction.

- [ ] **Step 3: Add the empty-window branch**

When no frame exhibits any port frame, print the per-capture counts (all
unexplained) and return a failing status, without indexing `idx`. Keep every
existing path and threshold unchanged.

- [ ] **Step 4: Run the test and the oracle**

Run the new test (PASS), then `make title-oracle` against the current un-pinned
captures. Expected: it now reports `0 exhibited` and exits non-zero instead of
crashing — the same measurement Task 1 collected, now from the oracle itself.

- [ ] **Step 5: Commit**

```bash
git add tools/title_compare.py tools/tests/test_title_compare.py
git commit -m "title: report total non-alignment instead of crashing the oracle"
```

### Task 2: diagnose `0x2BF08`'s per-frame overlay

Research task. Produces a written diagnosis, not code. It answers, with
disassembly evidence: what `0x2BF08` writes to the aperture on a frame where
`(DS_000EF6DC & 0x1F) != 0` (the frames Task 1 shows are affected) — the
message/font overlay, its source record, which already-ported functions it
composes through, and the size of a faithful port.

**Files:**
- Create: `docs/superpowers/plans/2026-09-18-bf08-overlay-diagnosis.md`

- [ ] **Step 1: Trace the aperture path**

From `prage.c:16666` (`0x2BF08`), follow each branch, especially the
`FUN_0002F280` / `FUN_0002F198` / `FUN_0002F4BC` and `FUN_0001C500` calls, and
determine which of them builds or moves display nodes that reach the composite.
State which are called on a `(DS_000EF6DC & 0x1F) != 0` frame versus a `== 0`
frame, and reconcile that with Task 1's finding that every frame differs.

- [ ] **Step 2: Identify the overlay content**

Determine what is drawn: a message id from `DS_00105C00`, a cursor, a blinking
prompt, or text already in the grid. Disassemble the callees as needed
(`file_offset = va + 0x52E54`).

- [ ] **Step 3: Estimate the port**

State the functions and records a faithful port needs, the ones already ported,
and the new work — so the cycle can be re-specified with a real size.

- [ ] **Step 4: Recommend**

One of: port the overlay now; keep the pin and document the carve-out; or a
smaller intermediate. Give the evidence for the recommendation.

- [ ] **Step 5: Commit**

```bash
git add docs/superpowers/plans/2026-09-18-bf08-overlay-diagnosis.md
git commit -m "docs: diagnose 0x2BF08's per-frame title overlay"
```

Tasks 3+ are re-specified once Task 2's diagnosis lands.

---

### Task 1: Remove the `0x2BF08` inert pin and re-derive the oracle reference

This is the probe-first step (spec Decision 4). It produces the reference everything else is judged against, and a **gate**: if the port's drift after un-pinning is wider than frames 32/64/96, stop and re-scope — do not start Task 3.

**Files:**
- Modify: `tools/title_pin.py:30-36` (the `PATCHES` table and its docstring)
- Modify: `tools/tests/test_title_pin.py` (expected site count and the inert-site assertion)
- Regenerate (git-ignored): `data/title-captures/title`, `data/title-captures/title2`

**Interfaces:**
- Consumes: nothing.
- Produces: an un-pinned oracle reference under `data/title-captures/`, and a recorded drift measurement that Task 5 depends on. `PATCHES` keeps exactly four entries, so a later reader knows `0x2BF08` is no longer inerted.

- [ ] **Step 1: Read the current pin table and its consumers**

Run: `sed -n '25,45p' tools/title_pin.py` and `cat tools/tests/test_title_pin.py`

Confirm the fifth entry is `(0x7ED5C, bytes.fromhex("53"), bytes.fromhex("c3"))` and find every assertion that counts or names the sites.

- [ ] **Step 2: Remove the fifth site**

Delete exactly that one tuple from `PATCHES`. Leave the four behaviour pins (the three RNG immediates and the opcode-8 pin) untouched — they are what keeps the capture deterministic.

Update the `title_pin.py` docstring: the fifth "scope decision" paragraph is gone, and the remaining four sites are the behaviour pins and the opcode-8 pin.

- [ ] **Step 3: Update the pin test**

In `tools/tests/test_title_pin.py`, change the expected site count from 5 to 4 and remove the assertion that `0x7ED5C` becomes `c3`. Keep the fail-closed and refusal assertions.

- [ ] **Step 4: Verify the pin tool still fails closed**

Run: `python3 -m pytest tools/tests/test_title_pin.py -q`
Expected: PASS, 4 sites.

Run: `make title-pin`
Expected: writes `/tmp/pr_title_pin/PRAGE.EXE`, exit 0, no write under `data/`.

- [ ] **Step 5: Back up the pinned reference, then capture the un-pinned original twice**

The capture overwrites `data/title-captures/title` and `title2`, the only
reference for the currently green oracle. Copy them aside first:

```bash
cp -R data/title-captures data/title-captures.pinned-backup
```

That backup is git-ignored scratch; it exists so the pinned reference can be
restored without re-capturing if the un-pinned capture cannot be made to work.

Then capture twice:

```bash
python3 tools/title_capture.py --out data/title-captures/title  --time-limit 45
python3 tools/title_capture.py --out data/title-captures/title2 --time-limit 45
```
Expected: both directories populated with `frame_%04d.raw` and `window.txt`. If the runner needs interaction, follow `tools/title_capture.py`'s own instructions; record what was needed in the Task 6 report.

- [ ] **Step 6: Measure the drift and record the gate decision**

Run: `make title-oracle`
Expected: it prints, per capture, the counts of clean / splice / transition / unexplained frames and the exhibited port frames.

Record in the plan's execution log (and later the report):
- the two captures' counts,
- the port frames still unexplained after un-pinning,
- whether the drift falls at frames 32/64/96 only (⇒ Task 5 ports `0x2BF08`) or is empty (⇒ `0x2BF08` does not reach the composite and is recorded as a gap) or is wider (⇒ **STOP**; report to the human and re-scope).

- [ ] **Step 7: Commit**

```bash
git add tools/title_pin.py tools/tests/test_title_pin.py
git commit -m "title: drop the 0x2BF08 inert pin so the oracle covers the true original"
```

---

### Task 3: Port the `0x2BF08` overlay and seed the captured values

Diagnosis: `docs/superpowers/plans/2026-09-18-bf08-overlay-diagnosis.md`. The
original draws a centred `CREDITS:5` line at rows 7–12 on **every** frame,
through the ungated `DS_00105C00 != 0` branch of `0x2BF08`. All the rendering is
already ported; only the control flow and the data are new.

**Files:**
- Modify: `port/src/game/flow.c:863-875` (the `0x2BF08` PORT marker) and the
  init pin block near `flow.c:698-705`
- Modify: `port/tests/test_flow.c`

**Interfaces:**
- Consumes: `game_string_get` (`flow.c:254`), `text_cursor_set` (`0x2F198`),
  `text_cursor_hold` (`0x2F4BC`), `text_cells_release` (`0x2F280`), all already
  ported and declared in `game/actors.h` / `game/flow.h`.
- Produces: a real `0x2BF08` in the `0x11D04` tail; pinned `DS_00105C00 = 5` and
  `DS_00105C05 = 1` (text row, not screen row).

- [ ] **Step 1: Pin the raw call sites before writing code**

`0x2BF08` and its callees are `__regparm3`, and the decompiler is already known
to misstate this function: `prage.c:16694` shows `FUN_0001c500(DAT_00105c00)`
where the raw bytes are `mov eax,0x46; call 0x1c500`. Disassemble `0x2BF08`
(file `0x7ED5C`) with capstone and record, per branch, the constant string id
passed to `0x1C500` (`0x45` FREE PLAY, `0x46` CREDITS, `0x47` INSERT COINS) and
the arguments to `0x2F198`/`0x2F4BC`/`0x2F280`. The diagnosis's §3 table is the
expected shape; raw bytes decide.

- [ ] **Step 2: Write the failing test**

In `port/tests/test_flow.c`, assert on the globals for the four branches —
early return on `DS_0009AD58 != 0`; the `DS_00105D60 == 0` FREE-PLAY
hold/release pair keyed on `&0x20`; the `DS_00105C00 != 0` CREDITS branch; the
`DS_00105C00 == 0` insert-coins `&0x1F`/latch sequence. Assert the latch
`DS_00105C04` behaviour and that the CREDITS branch is reached on a
`(&0x1F) != 0` frame while `DS_00105C00 != 0`.

- [ ] **Step 3: Implement**

Transcribe `prage.c:16666-16711` at the PORT marker, in the raw-pinned branch
order, reusing the ported text functions. `FUN_0002CAA8` is
`DS_00105D60 == 0`; `FUN_00065546` becomes
`snprintf(buf, sizeof buf, "%s:%d", game_string_get(0x46), DS_00105C00)`.

Then seed the two runtime values beside the existing `0x2D974` pin
(`flow.c:698-705`), with a `/* PORT: */` marker and `/* TODO(verify): */`:
`DS_00105C00 = 5`, `DS_00105C05 = 1` (text row; the captured glyph is at screen
rows 7–12 and one text row is `20/3` px). The diagnosis forbids porting `0x2D974`
here: the initial value is a save/config read, and the countdown
(`FUN_0002CA48`/`FUN_0002CA7C` via the title input handler `FUN_00011F28`) is
input-driven and belongs to 4b.

- [ ] **Step 4: Run the tests and the oracle**

Run: `cmake --build build && PR_GAME_DIR=data/game/C ./build/run_tests`
Expected: `all checks passed`.

Run: `make title-oracle`
Expected: **green against the un-pinned captures** — this is the proof that
`0x2BF08` moves the composite (spec Decision 5). If it does not go green,
record the exact residual and stop; do not weaken the oracle.

- [ ] **Step 5: Negative control**

Stub `0x2BF08` back to a no-op (or revert the branch) and confirm
`make title-oracle` drifts again on the overlay frames. Restore.

- [ ] **Step 6: Commit**

```bash
git add port/src/game/flow.c port/tests/test_flow.c
git commit -m "flow: port 0x2BF08's title overlay and pin its captured inputs"
```

---

### Task 4: Pin the effect-subsystem register bindings by disassembly

`__regparm3` hides the register arguments and the decompilation contradicts itself on the link-field order (`0x249B0` treats `[0]` as next, `0x249C0` treats `[1]` as next). No port code is written until this is pinned. Produces a committed companion doc, exactly as 4a-ii's `2026-09-17-actor-system-args.md` did.

**Files:**
- Create: `docs/superpowers/plans/2026-09-18-title-residuals-args.md`

**Interfaces:**
- Produces: for each of `0x13C70`, `0x13DF0`, `0x13ADC`, `0x249B0`, `0x249D0`, `0x249C0`, `0x13420`, `0x1B544`: the incoming registers in order, the returned value, the record stride and count, and the free/active list layout. Tasks 3 and 4 consume these names and types verbatim.

- [ ] **Step 1: Disassemble each function from the shipped EXE**

For each virtual address below, disassemble 32-bit GNU syntax from `data/game/C/PRAGE.EXE` at `file_offset = va + 0x52E54` using capstone, and paste the prologue plus every write to a record field:

| va | file offset | role |
|---|---|---|
| `0x13C70` | `0x66AC4` | effect spawn |
| `0x13DF0` | `0x66C44` | effect list clear |
| `0x13ADC` | `0x66930` | free-list build |
| `0x249B0` | `0x77804` | list primitive A |
| `0x249D0` | `0x77824` | list unlink |
| `0x249C0` | `0x77814` | list primitive B |
| `0x13420` | `0x66274` | per-entry teardown |
| `0x1B544` | `0x6E398` | header/table resolve |

- [ ] **Step 2: Record the record layout**

From `0x13ADC`'s stride and the `&DAT_000F0B00` … `&DAT_000FCCE0` bounds, record: the record stride in bytes, the record count, and the first/last record addresses. From `0x13C70`, record the meaning of `+0x8`, `+0xC`, `+0xD`, `+0xE`, `+0xF`, `+0x10+4*i`, `+0x410+4*i`, and the source-record `+0xC` count.

- [ ] **Step 3: Resolve the link-field contradiction**

State explicitly which field is next and which is prev for the effect lists, with the raw instruction that proves it, and therefore which of `0x249B0`/`0x249C0` inserts after which node. This is the fact Task 3's tests encode.

- [ ] **Step 4: Determine whether the free list is populated before the title**

Find the callers of `0x13ADC` and whether any runs on the shipped boot/title path before `0x123EA`. Record the answer: if it does not run, the ported `0x13C70` finds an empty list and its only in-window effect is the `DAT_0009AF3D` increment, which Task 4 and Task 6 must state plainly.

- [ ] **Step 5: Commit**

```bash
git add docs/superpowers/plans/2026-09-18-title-residuals-args.md
git commit -m "docs: pin the 0x13xxx effect-subsystem register bindings"
```

---

### Task 5: Port the list primitives and the effects module

**Files:**
- Create: `port/src/game/effects.h`, `port/src/game/effects.c`
- Create: `port/tests/test_effects.c`
- Modify: `port/tests/test.h` (declare `int test_effects(void);`)
- Modify: `port/tests/run_tests.c` (call `test_effects();` after `test_actors();`)
- Modify: `port/CMakeLists.txt` (add `src/game/effects.c` to `prage_core` and `tests/test_effects.c` to `run_tests`)

**Interfaces:**
- Consumes: the bindings, stride (`0x814`), count (24, `0xF0B00`..`0xFC4CC`), link order (`[+0]`=next, `[+4]`=prev) and the free-list-populated answer from `docs/superpowers/plans/2026-09-18-title-residuals-args.md`.
- Produces: `void effects_init(void)` (`0x13ADC`), `effects_spawn(...)` (`0x13C70`, returns 0 when no free record), `void effects_clear(void)` (`0x13DF0`), `int effects_active(void)` (`DS_0009AF3D`), and the private link/unlink helpers.
  **`effects_spawn` takes three arguments, not two** — the pinned binding in the args doc includes the `EBX` handle (the call site passes `0x419786C` at `0x123DE`), and the entry count is read from the **source** record's `+0xC`. Use the args doc's exact register order.

- [ ] **Step 1: Write the failing tests**

In `port/tests/test_effects.c`, using only `CHECK`/`CHECK_EQ_INT`:

```c
#include "game/effects.h"
#include "mem.h"
#include "test.h"
#include "symbols.h"

/* Scratch for a source record: mem[] above the heap, the base other tests use. */
#define EFFECTS_TEST_SRC 0x3F00000u

int test_effects(void)
{
    int before = g_failures;

    /* Clearing an uninitialised pool must be a safe no-op, not a walk from the
     * zeroed sentinel through mem[] — the same hazard actors_reset guards. */
    mem_fill(DS_000FCCE0, 0, 8u);
    effects_clear();
    CHECK_EQ_INT(effects_active(), 0);

    /* Building the free list makes records available; an empty list is
     * self-linked, so a spawn succeeds and becomes the one active effect. */
    effects_init();
    {
        u32 src = EFFECTS_TEST_SRC;
        u32 rec;
        mem_fill(src, 0, 0x40u);          /* source record, entry count 0 at +0xC */
        rec = effects_spawn(src, 0x2Au);
        CHECK(rec != 0, "spawn takes a record once the free list is built");
        CHECK_EQ_INT(effects_active(), 1);
        CHECK_EQ_INT((int)DSB(rec + 0x0D), 0x2A);
        CHECK_EQ_INT((int)DSB(rec + 0x0F), 0x80);
    }

    /* Clear returns the pool to empty and zeroes the active count, which is the
     * flag 0x121A0's phase-2 exit tests. */
    effects_clear();
    CHECK_EQ_INT(effects_active(), 0);

    /* A second clear on an already-empty pool must not walk or corrupt the
     * self-linked sentinels. */
    effects_clear();
    CHECK_EQ_INT(effects_active(), 0);

    return g_failures - before;
}
```

- [ ] **Step 2: Run it and watch it fail**

Add the test to `test.h`, `run_tests.c` and `CMakeLists.txt`, then:
Run: `cmake --build build && ./build/run_tests`
Expected: compile error or FAIL — `effects.h` does not exist yet.

- [ ] **Step 3: Write the list primitives and the module**

In `port/src/game/effects.c`, implement the link/unlink helpers exactly as pinned in Task 2 (the decompilation is `port/decomp/prage.c:2702` for `0x249B0`, `:2690` for `0x249D0`), then `effects_init`, `effects_spawn` and `effects_clear` as faithful transcriptions of `prage.c:2593` (`0x13ADC`), `:2699` (`0x13C70`) and `:2760` (`0x13DF0`), with the register arguments named per the args doc. Every table copy iterates the source record's `+0xC` count; write no entry past it.

`effects_clear` must guard before walking: when the `DS_000FCCE0` sentinel is not self-linked (the pool was never initialised, so `mem[]` is zeroed), return without touching the list. This mirrors `actors_reset`'s `pool_base() == 0` guard and is what makes the first assertion in Step 1's test pass.

`effects.h` declares the five functions (`effects_init`, `effects_spawn`, `effects_clear`, `effects_active`, and nothing else — no test-only seam) and the `DS_*` addresses it owns.

- [ ] **Step 4: Run the tests green**

Run: `cmake --build build && PR_GAME_DIR=data/game/C ./build/run_tests`
Expected: `all checks passed`.

- [ ] **Step 5: Negative control**

Temporarily make `effects_spawn` return 0 unconditionally; run the tests; confirm the two spawn assertions FAIL. Revert.

- [ ] **Step 6: Commit**

```bash
git add port/src/game/effects.c port/src/game/effects.h port/tests/test_effects.c port/tests/test.h port/tests/run_tests.c port/CMakeLists.txt
git commit -m "effects: port the 0x13xxx effect list (spawn 0x13C70, clear 0x13DF0)"
```

---

### Task 6: Wire the effect list into the engine and the title

**Files:**
- Modify: `port/src/game/actors.c:103-121` (`actors_reset`'s deferred `0x13DF0` marker)
- Modify: `port/src/game/flow.c:414-421` (the `0x123EA` call site)
- Modify: `port/src/game/actors.h` / `port/src/game/flow.c` includes as needed

**Interfaces:**
- Consumes: `effects_init`, `effects_clear`, `effects_spawn` from Task 3.
- Produces: `actors_reset()` calls `effects_clear()`; the title's retired-node walk calls `effects_spawn(node, ...)` for nodes typed `0x3E688`.

- [ ] **Step 1: Replace the deferred markers with the calls**

In `actors_reset`, replace the `/* PORT: 0x13DF0 frees ... */` description with a call to `effects_clear()`.

In `flow.c` at the `0x123D6` arm, replace the `/* PORT: 0x13C70 ... */` comment with the call to `effects_spawn`, passing the node and the byte argument the disassembly pins in Task 2.

Where the shipped init calls `0x13ADC` on the boot path, call `effects_init()` at the same point.

- [ ] **Step 2: Build and unit-test**

Run: `cmake --build build && PR_GAME_DIR=data/game/C ./build/run_tests`
Expected: `all checks passed`.

- [ ] **Step 3: Run the title oracle**

Run: `make title-oracle`
Expected: green against the Task 1 captures, or a recorded, explained change in the unexplained-frame set. If `0x13C70` moves the composite, this run is the proof required by spec Decision 5; if not, record that fact for Task 6.

- [ ] **Step 4: Commit**

```bash
git add port/src/game/actors.c port/src/game/actors.h port/src/game/flow.c
git commit -m "flow: run the 0x13xxx effect list on the title path"
```

---

### Obsolete (was Task 5): conditional `0x2BF08` port — superseded by Task 3

The old Task 5 ported `0x2BF08` only if Task 1's drift landed at frames 32/64/96.
Task 1 and Task 2 falsified that premise: the overlay draws on every frame. The
work is now Task 3, which also pins the two runtime inputs. Nothing in the old
Task 5 remains to execute.

---

### Task 7: Port `0x134C0`, the effect step/age (the count drain)

Task 6's review found that `effects_spawn` increments `DAT_0009AF3D` and nothing
decrements it, so the title's state-2 exit (`DSB(DS_0009AF3D) == 0`, `flow.c:432`)
can no longer fire past frame 95. The decrement lives in `0x134C0`, which the
port marks deferred at `flow.c:779` and which the master loop (`0x255CC`) calls
every frame, after `DS_00104AF4++` and before `gfx_flush_palette`. The human
elected to port it in this cycle rather than park it.

`0x134c0` is `void FUN_000134c0(void)` (no arguments, unlike the `__regparm3`
helpers) and is 1564 bytes with 3 callees. It walks the effect list, ages and
tears effects down, and decrements `DAT_0009AF3D` (five decrement sites in the
decompilation, `prage.c:2422/2478/2515/2547/2574`).

**Files:**
- Modify: `port/src/game/effects.c`, `port/src/game/effects.h`
- Modify: `port/src/game/flow.c:779` (the deferred marker)
- Modify: `port/tests/test_effects.c`

**Interfaces:**
- Consumes: `effects_spawn`/`effects_clear`/`effects_active`, the list
  primitives, `0x13420` teardown, and the args doc's link directions.
- Produces: `void effects_step(void)` (`0x134C0`), called from the master loop.

- [ ] **Step 1: Write the failing test**

In `port/tests/test_effects.c`, assert that after `effects_init()` and one
`effects_spawn(...)`, `effects_active() == 1`, and that driving `effects_step()`
to completion ages the effect out: `effects_active()` returns to 0 and the
record is back on the free list exactly once (no leak, no double-link). Use the
age/lifetime values the raw governs, not invented counts.

- [ ] **Step 2: Run it and watch it fail**

Expected: FAIL — `effects_step` does not exist.

- [ ] **Step 3: Transcribe `0x134C0`**

Follow the decompilation at `prage.c:2315-2591` and the raw bytes
(`file_offset = va + 0x52E54`, i.e. `0x66314`) where the decompiler is
ambiguous. Keep it in `effects.c`; export only `effects_step`. Every path that
removes a record must decrement `DAT_0009AF3D` exactly once per removal, matching
the raw — the count is the whole point.

- [ ] **Step 4: Call it from the master loop**

Replace the `/* PORT: 0x134C0 deferred (scene/narrative). */` marker at
`flow.c:779` with `effects_step();`, in the original's position (after
`DS_00104AF4++`, before `gfx_flush_palette()`).

- [ ] **Step 5: Verify the drain and the window**

Run: `cmake --build build && PR_GAME_DIR=data/game/C ./build/run_tests`
Expected: `all checks passed`, including the new drain assertions.

Run: `make title-oracle`
Expected: still **green** (0 unexplained) — the step runs in the window and must
not change the composite, since the spawned effect has no renderer.

Then confirm the regression is gone: run the port past frame 95 headless (e.g.
`./build/prageport --game-dir data/game/C --check 130`) and assert the title
leaves state 2 rather than stalling. Record how you observed it.

- [ ] **Step 6: Negative control**

Disable the drain (stub `effects_step` to a no-op) and confirm the new
`effects_active()`-returns-to-0 assertion fails. Restore.

- [ ] **Step 7: Commit**

```bash
git add port/src/game/effects.c port/src/game/effects.h port/src/game/flow.c port/tests/test_effects.c
git commit -m "effects: port 0x134C0 step/age so the effect count drains"
```

---

### Task 8: Record the falsifiability outcome and verify the ladder

**Files:**
- Create: `docs/superpowers/plans/2026-09-18-title-residuals-report.md`
- Modify: `port/RE_GUIDE.md`, `port/spec/game_flow.md`, `README.md`

**Interfaces:**
- Consumes: every prior task's result.
- Produces: the cycle report and the updated docs.

- [ ] **Step 1: Write the report**

Record: the un-pinned capture counts; Task 1's drift measurement; for **each** of `0x13C70` and `0x2BF08`, either the oracle evidence that it moves the composite or an explicit statement that it does not, with the unit-level proof that carries it (spec Decision 5); the free-list-populated answer from Task 2; and the residuals carried (spec §8).

- [ ] **Step 2: Update the docs**

`README.md`: the title paragraph now says the oracle covers the un-pinned original; `port/spec/game_flow.md`: the `0x13C70`/`0x2BF08` entries move from deferred to ported; `port/RE_GUIDE.md`: the effect subsystem's address block.

- [ ] **Step 3: Full ladder**

Run: `rm -rf build && make verify`
Expected: exit 0, 0 warnings, `all checks passed`, `smk_compare` 120/120 + 41/41, `title_compare` as recorded in Step 1, `symbols.h` byte-identical.

- [ ] **Step 4: Commit**

```bash
git add docs/superpowers/plans/2026-09-18-title-residuals-report.md port/RE_GUIDE.md port/spec/game_flow.md README.md
git commit -m "docs: record the 4a-iii residual outcome and the falsifiability verdict"
```

---

## Self-Review

**Spec coverage:** §1.1 (2b-ii dropped) is a decision, recorded in the spec and
restated in Task 7's residuals. §2 in-scope items map to Task 1 (pin/capture),
Task 3 (`0x2BF08` overlay + seeded inputs), Tasks 4–6 (the `0x13xxx` effect
slice), Task 7 (record + ladder). §5 (oracle change) is Task 1. §6 (DoD,
falsifiability) is Task 7 Step 1 plus each task's tests. §7 (invariants) is
encoded in Task 5's tests and Task 1's fail-closed checks. §8 residuals are
restated in Task 7. The re-spec after Task 1 and Task 2 is recorded in the two
errata blocks and the obsolete-Task-5 note.

**Placeholder scan:** no TBD. Task 3's per-branch string ids and text-call
arguments are pinned from raw bytes in its Step 1 and consumed by its Step 3 —
the plan's own interface mechanism, not a placeholder. The effect-slice record
fields are produced by Task 4 and consumed by Task 5.

**Type consistency:** `effects_init`, `effects_spawn`, `effects_clear`,
`effects_active` are used with one signature each across Tasks 5 and 6.
`DS_0009AF3D` (active count) and `DS_0009AF3C` (lock) are used consistently with
the spec; `DS_00105C00`/`DS_00105C05`/`DS_00105C04` match the diagnosis doc's
names and roles.
