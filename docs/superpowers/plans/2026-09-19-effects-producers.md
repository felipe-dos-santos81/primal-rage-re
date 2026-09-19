# Effect Producers (4a-iv) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Port the three missing `0x13xxx` effect-list producers (`0x13D4C` type 4, `0x13E28` type 6, `0x13B3C` types 0/2) so every reachable effect type is constructible, and prove the spawn → step → palette-dirty-list → `gfx_dac` chain end-to-end.

**Architecture:** `port/src/game/effects.c` already owns the effect pool, the type-3 producer (`effects_spawn`, `0x13C70`), the per-frame step (`effects_step`) and the teardown. This cycle adds the three missing producers beside it, sharing only a genuinely identical free-list-pop helper, and extends `port/tests/test_effects.c`. The palette path (`palette_record` / `gfx_flush_palette`) is already owned by `port/src/platform/gfx.c` and is exercised, not changed.

**Tech Stack:** C11 (fixed port profile), the existing `make verify` ladder, capstone via `tools/mdi_disasm.py` for the raw-byte derivations.

## Global Constraints

- Spec: `docs/superpowers/specs/2026-09-19-effects-producers-design.md`. Base is `main` at `78e424b`.
- `data/` is read-only. The captures and `tools/opl_trace.py` must not be modified.
- No new dependencies (capstone is already used by `tools/`).
- `mem[]` holds original state at original offsets; never write `mem[0xA0000]`.
- SDL only in `port/src/host.c` and `port/src/main.c`.
- Comments in `port/src` only `/* PORT: ... */`, `/* TODO(verify): ... */`, and address tags.
- Tests: only `CHECK(cond,msg)` / `CHECK_EQ_INT(a,b)`; declared in `port/tests/test.h`, called from `run_tests.c` (both already cover `test_effects`).
- `port/src/symbols.h` must regenerate byte-identically: `python3 tools/gen_symbols.py port/decomp port/src/symbols.h`.
- Build 0 warnings. Stage explicit paths only; never `git add -A`. Commit style `<area>: <what changed>`.
- **Register-level fidelity, no fitted constants.** Every value comes from the raw bytes of `data/game/C/PRAGE.EXE` (`file_offset = va + 0x52E54` for obj-0 code) or from an existing port constant. Where the decompiler and the raw bytes disagree, the raw bytes win — the decompiler drops register arguments here and appears to omit `0x13B3C`'s active-count increment.
- The unbuilt-pool guard (`DSD(DS_000FCCE8) == 0` → return 0) applies to every producer.

---

### Task 1: The type-4 producer (`0x13D4C`) and the shared free-list pop

`0x13D4C` is the "darken to zero" producer: it fills the record's `+0x10` block from the resolved resource block and lets `effects_step`'s case-4 body darken it to black. It also shares its free-list pop exactly with `0x13C70`, so the pop is factored once here and every later producer uses it.

**Files:**
- Modify: `port/src/game/effects.h`
- Modify: `port/src/game/effects.c`
- Modify: `port/tests/test_effects.c`
- Create: `docs/superpowers/plans/2026-09-19-effects-producers-derivations.md`

**Interfaces:**
- Consumes: `res_resolve(u32 handle)`; the pool globals `DS_000FCCE0`/`DS_000FCCE8`/`DS_0009AF3C`/`DS_0009AF3D`; `list_unlink`/`list_insert_after`.
- Produces: `u32 effects_spawn_darken(u32 source_rec, u32 byte_arg);` — returns the record's `mem[]` offset, or `0` when the pool is empty/unbuilt. Sets `+0x0C = 4`, `+0x0F = 0`, `+0x0E = 1`, `+0x08 = source_rec`, `+0x0D = byte_arg`, `+0x10[0..n) = resolved[1+i]`, no `+0x410` fill, `DS_0009AF3D++`. **The resource handle is `DSD(source_rec)`** — the raw dereferences `[source_rec]` (`0x13D86 mov eax,[esi]`); only `0x13C70` takes a handle register.

- [ ] **Step 1: Derive `0x13D4C`'s register binding and confirm the shared pop from raw bytes**

Run, from the repo root (read-only; nothing under `data/` is modified):

```bash
python3 - <<'EOF'
from capstone import *
d = open('data/game/C/PRAGE.EXE','rb').read()
md = Cs(CS_ARCH_X86, CS_MODE_32)
def dis(va, n):
    for i in md.disasm(d[va + 0x52E54: va + 0x52E54 + n], va):
        print(hex(i.address), i.bytes.hex(), i.mnemonic, i.op_str)
dis(0x13d4c, 164)
dis(0x13c70, 96)   # for the shared prologue/pop only
EOF
```

Record in `docs/superpowers/plans/2026-09-19-effects-producers-derivations.md`:
1. The incoming registers for the source record, the byte arg and the handle (as the `2026-09-18-title-residuals-args.md` precedent does).
2. Whether the free-list pop (sentinel test, lock save/set, unlink, lock restore) is instruction-identical to `0x13C70`'s — this decides whether Step 4 extracts `effect_take_free()`.
3. The exact counts/offsets written, with file offsets for every claim.
4. The types-1/5 producer-set proof (spec §5): `DAT_000fcce8` is referenced by exactly four functions (`0x13B3C`, `0x13C70`, `0x13D4C`, `0x13E28`); they write types 0/2, 3, 4, 6.

- [ ] **Step 2: Write the failing test**

Append to `port/tests/test_effects.c`, before the closing `return`:

```c
    {
        /* 0x13D4C: type 4, +0x0F = 0, +0x0E = 1, +0x10 = the resolved block's
         * dwords (no +0x410 fill), active count +1. */
        u32 src = EFFECTS_TEST_SRC;
        u32 saved_tab = DSD(DS_001014E0), saved_n = DSD(DS_001014F0);
        u32 tab = src + 0x100u, blk = src + 0x200u, rec;
        effects_init();
        mem_fill(src, 0, 0x300u);
        DSD(src + 0x00) = 4u;             /* handle = index 0, offset 4 */
        DSD(src + 0x0C) = 2u;
        DSD(DS_001014E0) = tab;
        DSD(DS_001014F0) = 1;
        DSD(tab + 16) = blk;
        DSD(blk + 4) = 0x40404040u;
        DSD(blk + 8) = 0x00707070u;
        DSD(blk + 12) = 0x0A0B0C0Du;
        rec = effects_spawn_darken(src, 0x2Cu);
        CHECK(rec != 0, "0x13D4C takes a record");
        CHECK_EQ_INT(effects_active(), 1);
        CHECK_EQ_INT((int)DSB(rec + 0x0C), 4);
        CHECK_EQ_INT((int)DSB(rec + 0x0F), 0);
        CHECK_EQ_INT((int)DSB(rec + 0x0E), 1);
        CHECK_EQ_INT((int)DSD(rec + 0x08), (int)src);
        CHECK_EQ_INT((int)DSB(rec + 0x0D), 0x2C);
        /* Handles beyond offset 0 prove the handle came from DSD(source_rec):
         * with offset 0 these would be blk+4/blk+8 instead. */
        CHECK_EQ_INT((int)DSD(rec + 0x10), 0x00707070);
        CHECK_EQ_INT((int)DSD(rec + 0x14), 0x0A0B0C0D);
        /* The record front-inserts into the active list like 0x13C70. */
        CHECK_EQ_INT((int)DSD(DS_000FCCE0), (int)rec);
        CHECK_EQ_INT((int)DSD(rec), (int)DS_000FCCE0);
        DSD(DS_001014E0) = saved_tab;
        DSD(DS_001014F0) = saved_n;
    }
    effects_clear();
    CHECK_EQ_INT(effects_active(), 0);
```

- [ ] **Step 3: Run it and watch it fail**

Run: `cmake --build build && PR_GAME_DIR=data/game/C ./build/run_tests`
Expected: FAIL — `effects_spawn_darken` is undefined (compile error), or the assertions fail once the stub returns 0.

- [ ] **Step 4: Implement the shared pop and the producer**

In `port/src/game/effects.c`, extract the pop if Step 1 confirmed the share, then add the producer:

```c
/* The four producers share one pop: take the free-list head under the
 * interrupt lock. Returns 0 when the pool is unbuilt or empty. */
static u32 effect_take_free(void)
{
    if (DSD(DS_000FCCE8) == 0) return 0;               /* PORT: unbuilt pool */
    u32 rec = DSD(DS_000FCCE8);
    if (rec == DS_000FCCE8) return 0;                  /* empty free list */
    u8 saved = DSB(DS_0009AF3C);
    DSB(DS_0009AF3C) = 1;
    list_unlink(rec);
    DSB(DS_0009AF3C) = saved;
    return rec;
}

/* 0x13D4C. Type 4: +0x10 holds the resolved block's colours for the case-4
 * step body to darken to zero. The handle is read from the source record. */
u32 effects_spawn_darken(u32 source_rec, u32 byte_arg)
{
    u32 rec = effect_take_free();
    if (rec == 0) return 0;
    /* PORT: the raw dereferences [source_rec] for the handle (0x13D86);
     * there is no handle register. */
    const u32 *resolved = (const u32 *)res_resolve(DSD(source_rec));
    DSB(rec + 0x0f) = 0;
    DSB(rec + 0x0c) = 4;
    DSD(rec + 8) = source_rec;
    DSB(rec + 0x0d) = (u8)byte_arg;
    s32 count = (s32)DSD(source_rec + 0x0c);
    if (resolved != NULL) {
        for (s32 i = 0; i < count; i++)
            DSD(rec + 0x10 + (u32)i * 4u) = resolved[1 + i];
    }
    DSB(DS_0009AF3C) = 1;
    DSB(rec + 0x0e) = 1;
    list_insert_after(DS_000FCCE0, rec);
    DSB(DS_0009AF3D) = (u8)(DSB(DS_0009AF3D) + 1);
    DSB(DS_0009AF3C) = 0;
    return rec;
}
```

Refactor the existing `effects_spawn` (`0x13C70`) to call `effect_take_free()` so the pop has one owner; the existing `test_effects` suite is the regression net for that refactor. Add the declaration to `port/src/game/effects.h`:

```c
/* 0x13D4C. Pops a free record, fills it as type 4 (darken-to-zero over the
 * resolved block) and head-inserts it into the active list. Same contract as
 * effects_spawn: returns the record offset, or 0 when the pool is
 * unbuilt/empty. The handle is DSD(source_rec) (the raw reads [source_rec]). */
u32 effects_spawn_darken(u32 source_rec, u32 byte_arg);
```

- [ ] **Step 5: Run the tests green**

Run: `cmake --build build && PR_GAME_DIR=data/game/C ./build/run_tests`
Expected: `all checks passed`.

- [ ] **Step 6: Commit**

```bash
git add port/src/game/effects.c port/src/game/effects.h port/tests/test_effects.c docs/superpowers/plans/2026-09-19-effects-producers-derivations.md
git commit -m "effects: port the type-4 producer (0x13D4C)"
```

---

### Task 2: The type-6 producer (`0x13E28`)

`0x13E28` is the "darken toward a target" producer: `+0x10` starts at white (`0xFFFFFF`) and `effects_step`'s case-6 body darkens it toward the resolved block in `+0x410`.

**Files:**
- Modify: `port/src/game/effects.h`
- Modify: `port/src/game/effects.c`
- Modify: `port/tests/test_effects.c`
- Modify: `docs/superpowers/plans/2026-09-19-effects-producers-derivations.md`

**Interfaces:**
- Consumes: `effect_take_free()` from Task 1; `res_resolve`.
- Produces: `u32 effects_spawn_pulse(u32 source_rec, u32 byte_arg);` — sets `+0x0C = 6`, `+0x0F = 0x80`, `+0x0E = 1`, `+0x10[0..n) = 0xFFFFFF`, `+0x410[0..n) = resolved[1+i]`, `DS_0009AF3D++`. **The resource handle is `DSD(source_rec)`** (`0x13E66 mov eax,[ecx]`); there is no handle register.

- [ ] **Step 1: Derive `0x13E28` from raw bytes**

```bash
python3 - <<'EOF'
from capstone import *
d = open('data/game/C/PRAGE.EXE','rb').read()
md = Cs(CS_ARCH_X86, CS_MODE_32)
def dis(va, n):
    for i in md.disasm(d[va + 0x52E54: va + 0x52E54 + n], va):
        print(hex(i.address), i.bytes.hex(), i.mnemonic, i.op_str)
dis(0x13e28, 200)
EOF
```

Record the register binding and every written offset in the derivation doc. Confirm against the raw bytes: type `6`, `+0x0F = 0x80`, the `0xFFFFFF` fill at `+0x10`, the resolved copy into `+0x410`.

- [ ] **Step 2: Write the failing test**

Append to `port/tests/test_effects.c`:

```c
    {
        /* 0x13E28: type 6, +0x0F = 0x80, +0x0E = 1, +0x10 = 0xFFFFFF,
         * +0x410 = the resolved block, count +1. */
        u32 src = EFFECTS_TEST_SRC;
        u32 saved_tab = DSD(DS_001014E0), saved_n = DSD(DS_001014F0);
        u32 tab = src + 0x100u, blk = src + 0x200u, rec;
        effects_init();
        mem_fill(src, 0, 0x300u);
        DSD(src + 0x00) = 4u;             /* handle = index 0, offset 4 */
        DSD(src + 0x0C) = 2u;
        DSD(DS_001014E0) = tab;
        DSD(DS_001014F0) = 1;
        DSD(tab + 16) = blk;
        DSD(blk + 4) = 0x00102030u;
        DSD(blk + 8) = 0x00040506u;
        DSD(blk + 12) = 0x00070809u;
        rec = effects_spawn_pulse(src, 1u);
        CHECK(rec != 0, "0x13E28 takes a record");
        CHECK_EQ_INT(effects_active(), 1);
        CHECK_EQ_INT((int)DSB(rec + 0x0C), 6);
        CHECK_EQ_INT((int)DSB(rec + 0x0F), 0x80);
        CHECK_EQ_INT((int)DSB(rec + 0x0E), 1);
        CHECK_EQ_INT((int)DSD(rec + 0x08), (int)src);
        CHECK_EQ_INT((int)DSD(rec + 0x10), 0x00FFFFFF);
        CHECK_EQ_INT((int)DSD(rec + 0x14), 0x00FFFFFF);
        CHECK_EQ_INT((int)DSD(rec + 0x410), 0x00040506);
        CHECK_EQ_INT((int)DSD(rec + 0x414), 0x00070809);
        DSD(DS_001014E0) = saved_tab;
        DSD(DS_001014F0) = saved_n;
    }
    effects_clear();
    CHECK_EQ_INT(effects_active(), 0);
```

- [ ] **Step 3: Run it and watch it fail**

Run: `cmake --build build && PR_GAME_DIR=data/game/C ./build/run_tests`
Expected: FAIL — `effects_spawn_pulse` undefined.

- [ ] **Step 4: Implement**

```c
/* 0x13E28. Type 6: +0x10 starts white and the case-6 step body darkens it
 * toward the resolved block at +0x410. The handle is read from source_rec. */
u32 effects_spawn_pulse(u32 source_rec, u32 byte_arg)
{
    u32 rec = effect_take_free();
    if (rec == 0) return 0;
    /* PORT: the raw dereferences [source_rec] for the handle (0x13E66). */
    const u32 *resolved = (const u32 *)res_resolve(DSD(source_rec));
    DSB(rec + 0x0f) = 0x80;
    DSB(rec + 0x0c) = 6;
    DSD(rec + 8) = source_rec;
    DSB(rec + 0x0d) = (u8)byte_arg;
    s32 count = (s32)DSD(source_rec + 0x0c);
    for (s32 i = 0; i < count; i++) {
        DSD(rec + 0x10 + (u32)i * 4u) = 0x00FFFFFFu;
        if (resolved != NULL)
            DSD(rec + 0x410 + (u32)i * 4u) = resolved[1 + i];
    }
    DSB(DS_0009AF3C) = 1;
    DSB(rec + 0x0e) = 1;
    list_insert_after(DS_000FCCE0, rec);
    DSB(DS_0009AF3D) = (u8)(DSB(DS_0009AF3D) + 1);
    DSB(DS_0009AF3C) = 0;
    return rec;
}
```

Add the declaration to `port/src/game/effects.h`.

- [ ] **Step 5: Run the tests green**

Run: `cmake --build build && PR_GAME_DIR=data/game/C ./build/run_tests`
Expected: `all checks passed`.

- [ ] **Step 6: Commit**

```bash
git add port/src/game/effects.c port/src/game/effects.h port/tests/test_effects.c docs/superpowers/plans/2026-09-19-effects-producers-derivations.md
git commit -m "effects: port the type-6 producer (0x13E28)"
```

---

### Task 3: The types-0/2 producer (`0x13B3C`)

`0x13B3C` is the scroll/move producer: it copies a run of the resolved block into **both** `+0x14` and `+0x414`, walking the source by a signed offset, and selects type 0 or 2 from a flag. Its active-count behaviour and its zero-flag state byte are the two open questions in the spec (§4); both must be answered from raw bytes here.

**Files:**
- Modify: `port/src/game/effects.h`
- Modify: `port/src/game/effects.c`
- Modify: `port/tests/test_effects.c`
- Modify: `docs/superpowers/plans/2026-09-19-effects-producers-derivations.md`

**Interfaces:**
- Consumes: `effect_take_free()`; `res_resolve`.
- Produces: `u32 effects_spawn_scroll(u32 source_rec, s32 offset, u32 count, u32 flag);` — sets `+0x0C` = 0 (`flag == 0`) or 2, `+0x0E` = 0 or 1, `+0x10` = `offset`, `+0x0F` = `count`, `+0x0D` = `flag`, and fills `+0x14[0..count)` **and** `+0x414[0..count)` from the resolved block walked by `offset`. Returns the record offset or 0.

- [ ] **Step 1: Derive `0x13B3C` from raw bytes and settle the two open questions**

```bash
python3 - <<'EOF'
from capstone import *
d = open('data/game/C/PRAGE.EXE','rb').read()
md = Cs(CS_ARCH_X86, CS_MODE_32)
def dis(va, n):
    for i in md.disasm(d[va + 0x52E54: va + 0x52E54 + n], va):
        print(hex(i.address), i.bytes.hex(), i.mnemonic, i.op_str)
dis(0x13b3c, 308)
EOF
```

Answer, in the derivation doc, with raw evidence:
1. The incoming registers for the source record, the signed offset, the count and the type flag.
2. **Does it increment `DS_0009AF3D`?** The decompile does not show it for this function while showing it for the other three; decide from the bytes.
3. **Does the zero-flag arm really set `+0x0E = 0`?** If so, and `effects_step`'s type-0 branch skips while the state byte is 0, a type-0 record never retires. Record the raw truth; if confirmed, reproduce it faithfully and name it in the doc (do not "fix" it).
4. The exact source-walk direction and the `-0x80`/signed-offset handling, with the instructions that implement them.

- [ ] **Step 2: Write the failing test**

Append to `port/tests/test_effects.c` (adjust only the `offset`/`flag` values if Step 1 shows a different binding — the assertions must mirror the raw contract):

```c
    {
        /* 0x13B3C: flag != 0 -> type 2, +0x0E = 1; the resolved block is
         * copied into BOTH +0x14 and +0x414, walked by the signed offset. */
        u32 src = EFFECTS_TEST_SRC;
        u32 saved_tab = DSD(DS_001014E0), saved_n = DSD(DS_001014F0);
        u32 tab = src + 0x100u, blk = src + 0x200u, rec;
        effects_init();
        mem_fill(src, 0, 0x300u);
        DSD(src + 0x0C) = 3u;
        DSD(DS_001014E0) = tab;
        DSD(DS_001014F0) = 1;
        DSD(tab + 16) = blk;
        for (int i = 0; i < 6; i++) DSD(blk + 4 + (u32)i * 4u) = 0x40u + (u32)i;
        rec = effects_spawn_scroll(src, 0, 2u, 1u);
        CHECK(rec != 0, "0x13B3C takes a record");
        CHECK_EQ_INT((int)DSB(rec + 0x0C), 2);
        CHECK_EQ_INT((int)DSB(rec + 0x0E), 1);
        CHECK_EQ_INT((int)DSB(rec + 0x0F), 2);
        CHECK_EQ_INT((int)DSB(rec + 0x0D), 1);
        CHECK_EQ_INT((int)DSB(rec + 0x10), 0);
        CHECK_EQ_INT((int)DSD(rec + 0x14), 0x40);
        CHECK_EQ_INT((int)DSD(rec + 0x18), 0x41);
        CHECK_EQ_INT((int)DSD(rec + 0x414), 0x40);
        CHECK_EQ_INT((int)DSD(rec + 0x418), 0x41);
        DSD(DS_001014E0) = saved_tab;
        DSD(DS_001014F0) = saved_n;
    }
    effects_clear();
    CHECK_EQ_INT(effects_active(), 0);
```

- [ ] **Step 3: Run it and watch it fail**

Run: `cmake --build build && PR_GAME_DIR=data/game/C ./build/run_tests`
Expected: FAIL — `effects_spawn_scroll` undefined.

- [ ] **Step 4: Implement from the raw derivation**

Transcribe the derived walk exactly. The decompile's shape (a hypothesis only — Step 1 governs) is: for a negative offset the source pointer starts at `resolved + (count - 1)` and walks backward into `+0x14`/`+0x414` descending; for a non-negative offset it starts at `resolved + offset` and walks forward. The type flag selects `+0x0C`/`+0x0E` (0/0 or 2/1). Increment `DS_0009AF3D` only if Step 1 shows the raw does. Add the declaration to `port/src/game/effects.h`.

- [ ] **Step 5: Run the tests green**

Run: `cmake --build build && PR_GAME_DIR=data/game/C ./build/run_tests`
Expected: `all checks passed`.

- [ ] **Step 6: Commit**

```bash
git add port/src/game/effects.c port/src/game/effects.h port/tests/test_effects.c docs/superpowers/plans/2026-09-19-effects-producers-derivations.md
git commit -m "effects: port the types-0/2 producer (0x13B3C)"
```

---

### Task 4: The end-to-end palette chain, docs and the ladder

The headline proof from the spec: an effect spawned by a new producer, stepped, and drained through `gfx_flush_palette()` must land in `gfx_dac`. This task adds that test, records the cycle outcome, and runs the full ladder.

**Files:**
- Modify: `port/tests/test_effects.c`
- Modify: `port/spec/game_flow.md`
- Modify: `README.md`
- Create: `docs/superpowers/plans/2026-09-19-effects-producers-report.md`

**Interfaces:**
- Consumes: `effects_spawn_darken` (Task 1), `effects_spawn_pulse` (Task 2), `palette_list_init()` and `gfx_flush_palette()` from `platform/gfx.h`, `gfx_dac` from `platform/gfx.h`.

- [ ] **Step 1: Write the failing end-to-end test**

Append to `port/tests/test_effects.c` (add `#include "platform/gfx.h"` at the top if absent):

```c
    {
        /* End-to-end: 0x13D4C's type-4 record darkens its +0x10 block toward
         * zero and enqueues it; gfx_flush_palette drains the dirty list into
         * gfx_dac. Each step subtracts 8 from every colour lane, and the DAC
         * reader takes bits 2..7 of each lane, so a 0x40 lane becomes 0x38
         * after one step. */
        u32 src = EFFECTS_TEST_SRC;
        u32 saved_tab = DSD(DS_001014E0), saved_n = DSD(DS_001014F0);
        u32 tab = src + 0x100u, blk = src + 0x200u;
        palette_list_init();
        effects_init();
        mem_fill(src, 0, 0x300u);
        DSD(src + 0x0C) = 2u;
        DSD(src + 0x00) = 4u;             /* handle = index 0, offset 4 */
        DSD(src + 0x08) = 0x40u;          /* first DAC index for the record */
        DSD(src + 0x0C) = 2u;
        DSD(DS_001014E0) = tab;
        DSD(DS_001014F0) = 1;
        DSD(tab + 16) = blk;
        DSD(blk + 4) = 0x40404040u;
        DSD(blk + 8) = 0x40404040u;
        DSD(blk + 12) = 0x40404040u;
        u32 rec = effects_spawn_darken(src, 1u);
        CHECK(rec != 0, "end-to-end spawn");
        effects_step();                    /* state 1 -> 0, case-4 body runs */
        gfx_flush_palette();
        CHECK_EQ_INT(gfx_dac[0x40][0], 0x38);
        CHECK_EQ_INT(gfx_dac[0x40][1], 0x38);
        CHECK_EQ_INT(gfx_dac[0x40][2], 0x38);
        CHECK_EQ_INT(gfx_dac[0x41][0], 0x38);
        DSD(DS_001014E0) = saved_tab;
        DSD(DS_001014F0) = saved_n;
    }
    effects_clear();
    CHECK_EQ_INT(effects_active(), 0);
```

- [ ] **Step 2: Run it and watch it fail**

Run: `cmake --build build && PR_GAME_DIR=data/game/C ./build/run_tests`
Expected: FAIL — the DAC entries are still 0 (the record did not animate) if the producer or the step wiring is wrong. If it passes immediately, tighten it: assert the record drained (`effects_step()` 9 times drives the type-4 record to removal and `effects_active()` back to 0) so the test cannot pass vacuously.

- [ ] **Step 3: Run the tests green**

Run: `cmake --build build && PR_GAME_DIR=data/game/C ./build/run_tests`
Expected: `all checks passed`.

- [ ] **Step 4: Write the cycle report and update the docs**

Create `docs/superpowers/plans/2026-09-19-effects-producers-report.md` recording: the four live producers and their types; the types-1/5 dead proof; the raw evidence for each producer (pointing at the derivation doc); the answers to `0x13B3C`'s two open questions; the end-to-end result; and the declared gaps (no palette capture oracle, no call-site wiring, camera/scene functions untouched).

Update `port/spec/game_flow.md`'s effect coverage to name the four live producers and the two dead types. Update `README.md` only if its effect/coverage paragraph is now stale.

- [ ] **Step 5: Full ladder**

Run: `rm -rf build && make verify`
Expected: exit 0, 0 warnings, `all checks passed`, the title oracle unchanged, `smk_compare` 120/120 + 41/41, the capture comparison line unchanged (this cycle does not touch audio), `symbols.h` byte-identical.

- [ ] **Step 6: Commit**

```bash
git add port/tests/test_effects.c port/spec/game_flow.md README.md docs/superpowers/plans/2026-09-19-effects-producers-report.md
git commit -m "docs: record the effect-producer outcome"
```

---

## Self-Review

**Spec coverage:** §3 architecture → Task 1 (owner + shared pop). §4 behaviour table → Tasks 1–3 (one producer each). §4.1/§4.2 the two `0x13B3C` questions → Task 3 Step 1. §5 reachability → Task 1 Step 1 (producer-set proof) and Task 3 Step 1 (jump table). §6 DoD → Task 4 Steps 1–5 (record-level in Tasks 1–3, end-to-end and ladder in Task 4). §7 declared gaps → Task 4 Step 4. §8 limits → Task 4 Step 4.

**Placeholder scan:** no TBD. Tasks 1 and 2 carry their full code. Task 3's arithmetic is a genuine raw-byte deliverable (spec decision 4): Step 1 states the method, the exact questions to answer, and the evidence required, and Step 2 pins the acceptance contract — the plan deliberately does not pre-state a walk that the binary must decide.

**Type consistency:** `effect_take_free(void) -> u32` is defined in Task 1 and used by Tasks 2–3; `effects_spawn_darken`/`effects_spawn_pulse`/`effects_spawn_scroll` keep their names and signatures across their task, the header, and Task 4's end-to-end test; `palette_list_init`/`gfx_flush_palette`/`gfx_dac` are the existing `platform/gfx.h` signatures.
