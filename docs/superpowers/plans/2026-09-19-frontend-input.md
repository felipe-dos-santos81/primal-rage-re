# Front-End Input, Credits and Select State (4b-B) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Port the game's input bitfield, its credit layer, the coin poll and the state-2 selector so the port's front-end becomes interactive, with the credit counter driven by real input and the selector running as the raw's four-phase machine.

**Architecture:** The input state (`0x500C4`/`0x50161`/`0x4F644`) extends `port/src/platform/input.c`, its only consumers. The credit layer (`0x2CAA8`/`0x2CA2C`/`0x2CA48`/`0x2CA7C`/`0x2C060`/`0x2C06C`/`0x2BF00`) extends `port/src/game/config.c`, whose theme becomes "config and credit state". The selector `0x11F6C` and its helpers land in `port/src/game/flow.c` next to the existing title state. Every table (`0x9ACBC`, `0x9AEE8`, the key state) is read from `mem[]`; nothing is transcribed.

**Tech Stack:** C11 (fixed port profile), the existing `make verify` ladder, capstone for raw-byte derivations, SDL3 only in `host.c`/`main.c`.

## Global Constraints

- Spec: `docs/superpowers/specs/2026-09-19-frontend-input-design.md`. Base is `main` at `5f3486d`.
- `data/` is read-only; `tools/` must not be modified.
- No new dependencies (capstone is already used under `tools/`).
- `mem[]` holds original state at original offsets; never write `mem[0xA0000]`.
- SDL only in `port/src/host.c` and `port/src/main.c`.
- Comments in `port/src` only `/* PORT: ... */`, `/* TODO(verify): ... */`, and address tags.
- Tests: only `CHECK(cond,msg)` / `CHECK_EQ_INT(a,b)` (no message arg on `CHECK_EQ_INT`); declared in `port/tests/test.h`, called from `run_tests.c`.
- `port/src/symbols.h` must regenerate byte-identically: `python3 tools/gen_symbols.py port/decomp port/src/symbols.h`.
- Build 0 warnings. Stage explicit paths only; never `git add -A`. Commit style `<area>: <what changed>`.
- **Register-level fidelity, no fitted constants.** Every value comes from the raw bytes of `data/game/C/PRAGE.EXE` or from an existing port constant. `file_offset = va + 0x52E54` for obj-0 code. Where the decompiler and the raw bytes disagree, the raw bytes win.
- **Address bases.** Symbols in `symbols.h` are LINEAR addresses; raw object-relative displacements need a per-site base (obj-0 `+0x10000`, obj-1 `+0x80000`). Determine each immediate's base from its LE fixup's target object; do not assume.
- `game_init()` may be called **once per process** (the 64 MB bump allocator's `res_load_index` is one-shot). Any test that calls it must run alone, gated by an environment variable, exactly as `test_title()` does.
- The attract machine `0x11000`, states 3-9, the voice system `0x2C3FC` and the fight engine are **out of scope** (spec §7). The `DS_00105C05` row stand-in survives this cycle (spec §5).

---

### Task 1: The credit layer

`0x2CAA8`/`0x2CA2C`/`0x2CA48`/`0x2CA7C`/`0x2C060` are the free-play and credit predicates; `0x2C06C`/`0x2BF00` write the overlay's text row. Their bodies are eight to forty-five bytes and were verified instruction-by-instruction against the raw during design, so this task carries their complete translations.

**Files:**
- Modify: `port/src/game/config.h`
- Modify: `port/src/game/config.c`
- Modify: `port/tests/test_config.c`

**Interfaces:**
- Consumes: `DSB`/`DSD` from `mem.h`; `DS_00105C00`, `DS_00105C05`, `DS_00105D60`, `DS_00104B1F` from `symbols.h`.
- Produces:
  - `u32 config_not_free_play(void);` (`0x2CAA8`)
  - `u32 config_has_credit(void);` (`0x2CA2C`)
  - `u32 config_credit_ready(void);` (`0x2C060`)
  - `u32 config_credit_take(void);` (`0x2CA48`)
  - `u32 config_credit_spend(u32 n);` (`0x2CA7C`)
  - `void config_set_credit_row(u8 row);` (`0x2C06C`)
  - `void config_set_credit_row_init(void);` (`0x2BF00`)

- [ ] **Step 1: Write the failing test**

Append to `port/tests/test_config.c`, before the final `return 0;` of `test_config`:

```c
    {
        /* The credit layer: free play, the credit counter and the suppression
         * flag DS_00104B1F. */
        u32 credits = DSD(DS_00105C00);
        u8 free_play = DSB(DS_00105D60);
        u8 suppress = DSB(DS_00104B1F);

        DSB(DS_00105D60) = 0;
        DSB(DS_00104B1F) = 0;
        DSD(DS_00105C00) = 5u;

        CHECK_EQ_INT((int)config_not_free_play(), 1);
        CHECK_EQ_INT((int)config_has_credit(), 1);
        CHECK_EQ_INT((int)config_credit_ready(), 1);

        /* 0x2CA48: takes one credit and reports success. */
        CHECK_EQ_INT((int)config_credit_take(), 1);
        CHECK_EQ_INT((int)DSD(DS_00105C00), 4);

        /* 0x2CA7C: the n > credits guard leaves the counter alone. */
        CHECK_EQ_INT((int)config_credit_spend(5u), 0);
        CHECK_EQ_INT((int)DSD(DS_00105C00), 4);
        CHECK_EQ_INT((int)config_credit_spend(4u), 1);
        CHECK_EQ_INT((int)DSD(DS_00105C00), 0);

        /* No credits and no free play: both predicates and the take fail. */
        CHECK_EQ_INT((int)config_has_credit(), 0);
        CHECK_EQ_INT((int)config_credit_take(), 0);
        CHECK_EQ_INT((int)config_credit_spend(1u), 0);

        /* DS_00105D60 (free play) short-circuits to success, counter untouched. */
        DSB(DS_00105D60) = 1;
        CHECK_EQ_INT((int)config_not_free_play(), 0);
        DSD(DS_00105C00) = 2u;
        CHECK_EQ_INT((int)config_credit_take(), 1);
        CHECK_EQ_INT((int)DSD(DS_00105C00), 2);
        CHECK_EQ_INT((int)config_credit_spend(9u), 1);
        CHECK_EQ_INT((int)DSD(DS_00105C00), 2);
        DSB(DS_00105D60) = 0;

        /* DS_00104B1F suppresses the debit while still reporting success. */
        DSD(DS_00105C00) = 3u;
        DSB(DS_00104B1F) = 1;
        CHECK_EQ_INT((int)config_credit_take(), 1);
        CHECK_EQ_INT((int)DSD(DS_00105C00), 3);
        CHECK_EQ_INT((int)config_credit_spend(1u), 1);
        CHECK_EQ_INT((int)DSD(DS_00105C00), 3);
        DSB(DS_00104B1F) = 0;

        /* 0x2C06C/0x2BF00: the overlay row writers. */
        config_set_credit_row(7u);
        CHECK_EQ_INT((int)DSB(DS_00105C05), 7);
        config_set_credit_row_init();
        CHECK_EQ_INT((int)DSB(DS_00105C05), 0x1D);

        DSD(DS_00105C00) = credits;
        DSB(DS_00105D60) = free_play;
        DSB(DS_00104B1F) = suppress;
    }
```

- [ ] **Step 2: Run it and watch it fail**

Run: `cmake --build build && PR_GAME_DIR=data/game/C ./build/run_tests`
Expected: FAIL — the seven declarations do not exist (compile error).

- [ ] **Step 3: Declare the credit layer**

Append to `port/src/game/config.h`, before the closing `#endif`:

```c
/* ---- credit layer (0x2Cxxx) ---------------------------------------------
 * The credit counter DS_00105C00, the FREE PLAY flag DS_00105D60 and the
 * debit-suppression flag DS_00104B1F. Read and written where the raw does. */

/* 0x2CAA8. 1 when the machine is not in free play. */
u32 config_not_free_play(void);

/* 0x2CA2C. 1 when a credit is available or free play is on. */
u32 config_has_credit(void);

/* 0x2C060. Calls 0x2CAA8 then tail-jumps to 0x2CA2C, discarding the 0x2CAA8
 * result, so this is config_has_credit(). */
u32 config_credit_ready(void);

/* 0x2CA48. Free play -> 1; no credits -> 0; otherwise decrement one credit
 * (unless DS_00104B1F suppresses it) and -> 1. */
u32 config_credit_take(void);

/* 0x2CA7C(n). Free play -> 1; n > credits -> 0; otherwise subtract n (unless
 * DS_00104B1F suppresses it) and -> 1. */
u32 config_credit_spend(u32 n);

/* 0x2C06C. Writes the overlay's text row DS_00105C05. */
void config_set_credit_row(u8 row);

/* 0x2BF00. The init-time row write: 0x1D. One caller, 0x20CCC in 0x20C10. */
void config_set_credit_row_init(void);
```

- [ ] **Step 4: Implement the credit layer**

Append to `port/src/game/config.c`:

```c
/* ---- credit layer -------------------------------------------------------- */

/* 0x2CAA8. `cmp byte [0x85d60],0; sete al; and eax,0xff`. */
u32 config_not_free_play(void)
{
    return DSB(DS_00105D60) == 0u ? 1u : 0u;
}

/* 0x2CA2C. `mov edx,[0x85c00]; mov al,[0x85d60]; or eax,edx; setne al`. */
u32 config_has_credit(void)
{
    return (u32)((DSB(DS_00105D60) | DSD(DS_00105C00)) != 0u);
}

/* 0x2C060. `call 0x2caa8; jmp 0x2ca2c` — the 0x2CAA8 result is discarded. */
u32 config_credit_ready(void)
{
    (void)config_not_free_play();
    return config_has_credit();
}

/* 0x2CA48. Free play, then the zero-credit guard, then the suppressed debit. */
u32 config_credit_take(void)
{
    if (DSB(DS_00105D60) != 0u) return 1u;            /* 0x2CA51 */
    if (DSD(DS_00105C00) == 0u) return 0u;            /* 0x2CA5E */
    if (DSB(DS_00104B1F) == 0u) DSD(DS_00105C00)--;   /* 0x2CA69 */
    return 1u;
}

/* 0x2CA7C. `cmp eax,[0x85c00]; ja` is an unsigned guard. */
u32 config_credit_spend(u32 n)
{
    if (DSB(DS_00105D60) != 0u) return 1u;            /* 0x2CA85 */
    if (n > DSD(DS_00105C00)) return 0u;              /* 0x2CA91 */
    if (DSB(DS_00104B1F) == 0u) DSD(DS_00105C00) -= n;/* 0x2CA9C */
    return 1u;
}

/* 0x2C06C. `mov byte [0x85c05], al`. */
void config_set_credit_row(u8 row)
{
    DSB(DS_00105C05) = row;
}

/* 0x2BF00. `mov byte [0x85c05], 0x1d`. */
void config_set_credit_row_init(void)
{
    DSB(DS_00105C05) = 0x1Du;
}
```

- [ ] **Step 5: Run the tests green**

Run: `cmake --build build && PR_GAME_DIR=data/game/C ./build/run_tests`
Expected: `all checks passed`, 0 warnings.

- [ ] **Step 6: Commit**

```bash
git add port/src/game/config.h port/src/game/config.c port/tests/test_config.c
git commit -m "config: add the credit layer (0x2CAA8/0x2CA2C/0x2C060/0x2CA48/0x2CA7C/0x2C06C/0x2BF00)"
```

---

### Task 2: The input state

`0x500C4` samples a two-byte key bitmap into a debounced level, `0x50161` selects and latches bits, and `0x4F644` builds the two masks the game reads: `DS_001088E4` (newly pressed) and `DS_001088D8` (held). The joystick accessor `0x2D2F0` lives here with its only caller. The host binding — which SDL key drives which input bit — is a documented port choice.

**Files:**
- Modify: `port/src/platform/input.h`
- Modify: `port/src/platform/input.c`
- Modify: `port/src/game/flow.c`   (delete the old `0x500C4` placeholder)
- Modify: `port/src/host.h`
- Modify: `port/src/host.c`
- Modify: `port/tests/test_input.c`

**Interfaces:**
- Consumes: `mem[]`/`DSB`/`DSD`/`DSW` from `mem.h`; `DS_000E1C30`, `DS_000E1C34`, `DS_000E1C38`, `DS_000E1C3C`, `DS_000E1C40`, `DS_000E1C42`, `DS_000E1C44`, `DS_001088D4`, `DS_001088D8`, `DS_001088DC`, `DS_001088E0`, `DS_001088E2`, `DS_001088E4`, `DS_00101514` from `symbols.h`; `host_key_bits()` from `host.h`.
- Produces:
  - `u32 input_pump(void);` (`0x500C4`) — now **returns** the level word. `flow.c`'s existing `static void input_pump(void) { host_pump(); }` placeholder is deleted in Step 4; the `game_loop` call site then resolves to this one. SDL event pumping is unaffected: `host_wait_vblank()` (already called at the loop tail) invokes `host_pump()`.
  - `u32 input_select_bits(u32 mask);` (`0x50161`)
  - `u32 input_joystick_device(u32 selector);` (`0x2D2F0`)
  - `void input_state_update(void);` (`0x4F644`)
  - `u16 host_key_bits(void);` in `host.h`/`host.c`

- [ ] **Step 1: Write the failing test**

Append to `port/tests/test_input.c`, before the final `return 0;` of `test_input`, and add `#include "mem.h"`, `#include "symbols.h"`, `#include <stdint.h>` to its includes:

```c
    {
        /* The input bitfield. 0x50161 is a level/latch selector over the raw
         * level word DAT_000E1C34; 0x4F644 turns it into the two masks the game
         * reads. Host bits enter through the key bitmap at
         * DS_00101514 + 0x2d8/0x2d9, which the test drives directly. */
        u32 saved_base = DSD(DS_00101514);
        u32 saved_30 = DSD(DS_000E1C30);
        u32 saved_34 = DSD(DS_000E1C34);
        u32 saved_38 = DSD(DS_000E1C38);
        u32 saved_3c = DSD(DS_000E1C3C);
        u16 saved_40 = DSW(DS_000E1C40);
        u8 saved_e4[4], saved_d8[4];

        static u8 page[0x400];
        for (u32 i = 0; i < sizeof page; i++) page[i] = 0;
        DSD(DS_00101514) = (u32)(uintptr_t)page;
        for (u32 i = 0; i < 4u; i++) {
            saved_e4[i] = DSB(DS_001088E4 + i);
            saved_d8[i] = DSB(DS_001088D8 + i);
        }
        DSD(DS_000E1C30) = 0; DSD(DS_000E1C34) = 0; DSD(DS_000E1C38) = 0;
        DSD(DS_000E1C3C) = 0; DSW(DS_000E1C40) = 0;

        /* 0x2D2F0 is a constant 0 (`xor eax,eax; ret`). */
        CHECK_EQ_INT((int)input_joystick_device(0xFFu), 0);

        /* 0x500C4: level word = (byte[+0x2d8] << 24) | (byte[+0x2d9] << 8).
         * The debounce holds a bit's previous level for one frame, so a press
         * needs two samples to appear. */
        page[0x2d9] = 0x01;                          /* mask 0x00000100 */
        CHECK_EQ_INT((int)input_pump(), 0);          /* change is debounced */
        CHECK_EQ_INT((int)input_pump(), 0x00000100); /* second sample holds it */

        /* 0x4F644: with the joystick accessor 0 the merge never fires, so
         * DS_001088E4 is the newly-pressed bits and DS_001088D8 the held bits.
         * The press is visible in E4 for exactly one frame. */
        input_state_update();
        CHECK_EQ_INT((int)DSD(DS_001088E4), 0x00000100);
        CHECK_EQ_INT((int)DSD(DS_001088D8), 0x00000100);

        input_state_update();
        CHECK_EQ_INT((int)DSD(DS_001088E4), 0);          /* no longer new */
        CHECK_EQ_INT((int)DSD(DS_001088D8), 0x00000100); /* still held */

        /* Release: the level drops after the debounce, both masks clear. */
        page[0x2d9] = 0;
        input_pump(); input_pump(); input_state_update();
        CHECK_EQ_INT((int)DSD(DS_001088E4), 0x00000100);
        input_state_update();
        CHECK_EQ_INT((int)DSD(DS_001088E4), 0);
        CHECK_EQ_INT((int)DSD(DS_001088D8), 0);

        /* 0x50161 applies its mask to the latch and leaves the unmasked bits of
         * the level word alone. */
        DSD(DS_000E1C34) = 0x0F000000u;
        DSD(DS_000E1C38) = 0;
        CHECK_EQ_INT((int)input_select_bits(0x01000000u), 0x0F000000);
        CHECK_EQ_INT((int)DSD(DS_000E1C38), 0x01000000);
        CHECK_EQ_INT((int)input_select_bits(0x10000000u), 0x0F000000);
        CHECK_EQ_INT((int)DSD(DS_000E1C38), 0x11000000);

        DSD(DS_00101514) = saved_base;
        DSD(DS_000E1C30) = saved_30; DSD(DS_000E1C34) = saved_34;
        DSD(DS_000E1C38) = saved_38; DSD(DS_000E1C3C) = saved_3c;
        DSW(DS_000E1C40) = saved_40;
        for (u32 i = 0; i < 4u; i++) {
            DSB(DS_001088E4 + i) = saved_e4[i];
            DSB(DS_001088D8 + i) = saved_d8[i];
        }
    }
```

- [ ] **Step 2: Run it and watch it fail**

Run: `cmake --build build && PR_GAME_DIR=data/game/C ./build/run_tests`
Expected: FAIL — `input_pump`, `input_select_bits`, `input_joystick_device` and `input_state_update` are undeclared.

- [ ] **Step 3: Declare the input state**

Append to `port/src/platform/input.h`, before the closing `#endif`:

```c
/* ---- game input bitfield (0x500C4 / 0x50161 / 0x4F644) ------------------
 * The original keeps a debounced key level in DAT_000E1C34, a hold latch in
 * DAT_000E1C38 and a repeat mask in DAT_000E1C3C, sampled from the key bitmap
 * at DAT_00101514 + 0x2d8/0x2d9, and turns it into the two masks the game
 * reads: DS_001088E4 (newly pressed) and DS_001088D8 (held). The host fills the
 * bitmap; everything below is the raw's arithmetic. */

/* 0x500C4. Samples the host key bitmap, applies the raw's one-frame debounce
 * and repeat-timer logic, and returns the debounced level word. */
u32 input_pump(void);

/* 0x50161. Reports `level & ~mask` OR'd with the bits of `mask` newly set
 * since the last call, latching them into DAT_000E1C38 as it goes. */
u32 input_select_bits(u32 mask);

/* 0x2D2F0. The joystick accessor: a constant 0 in the shipped profile
 * (`xor eax,eax; ret`), so the joystick half of 0x4F644 is inert. */
u32 input_joystick_device(u32 selector);

/* 0x4F644. Builds DS_001088E4 (newly pressed) and DS_001088D8 (held) from the
 * level word and the two 0x50161 mask families. */
void input_state_update(void);
```

- [ ] **Step 4: Implement the input state**

Append to `port/src/platform/input.c`:

```c
/* 0x2D2F0. `xor eax,eax; ret`. */
u32 input_joystick_device(u32 selector)
{
    (void)selector;
    return 0u;
}

/* 0x50161. Level/latch bit selector. */
u32 input_select_bits(u32 mask)
{
    u32 level = DSD(DS_000E1C34);
    if (mask != 0u) {
        u32 latch = DSD(DS_000E1C38);
        u32 edge = (latch ^ level) & mask;      /* 0x50174/0x50176 */
        DSD(DS_000E1C38) = latch | edge;        /* 0x50178 */
        level = (level & ~mask) | edge;         /* 0x5017E/0x50180/0x50182 */
    }
    return level;
}

/* 0x500C4. The key bytes are combined as (byte[+0x2d8] << 24) |
 * (byte[+0x2d9] << 8), so the level word only ever occupies the 0xFF00FF00
 * positions. The new level keeps the previous value for every bit that changed
 * this frame - the raw's one-frame debounce. */
u32 input_pump(void)
{
    const u8 *k = mem + DSD(DS_00101514);
    u32 cur = ((u32)k[0x2d8] << 24) | ((u32)k[0x2d9] << 8);
    u32 changed = DSD(DS_000E1C30) ^ cur;
    DSD(DS_000E1C30) = cur;
    u32 level = DSD(DS_000E1C34);
    u32 newlevel = (~changed & cur) | (changed & level);
    DSD(DS_000E1C34) = newlevel;
    DSD(DS_000E1C38) &= newlevel;

    u32 rpt = DSD(DS_000E1C3C);
    if ((changed & level & rpt) != 0u || (newlevel & rpt) == 0u) {
        DSW(DS_000E1C40) = DSW(DS_000E1C42);            /* 0x50130 */
    } else if (--DSW(DS_000E1C40) == 0u) {              /* 0x5010F */
        DSW(DS_000E1C40) = DSW(DS_000E1C44);            /* 0x50118 */
        DSD(DS_000E1C38) &= ~rpt;                       /* 0x50128 */
    }
    return DSD(DS_000E1C34);
}

/* 0x4F644. The first selector uses the 0xFF00FF00 family (the bits the level
 * word can carry) and yields the newly-pressed mask; the second uses the
 * complementary family and keeps only the level half, yielding the held mask.
 * Both share the DAT_000E1C38 latch, in this order. */
void input_state_update(void)
{
    DSD(DS_001088E4) = input_select_bits(0xFF00FF00u);              /* 0x4F64A */
    DSD(DS_001088D8) = input_select_bits(0x00FF00FFu) & 0xFF00FF00u;/* 0x4F659 */
    DSD(DS_001088DC) = input_joystick_device(0xFFu);                /* 0x4F66D */
    DSD(DS_001088D4) = input_joystick_device(0xFFFFFF00u) & 0xFFu;  /* 0x4F67C */
    if ((DSB(DS_001088D4) & 2u) != 0u)                              /* 0x4F68B */
        DSD(DS_001088E4) |= DSD(DS_001088D8);
    DSW(DS_001088E0) = (u16)(((DSD(DS_001088E4) & 0xFF000000u) >> 24) |
                             ((DSD(DS_001088D8) & 0xFF000000u) >> 16));
    DSW(DS_001088E2) = (u16)(((DSD(DS_001088E4) & 0x0000FF00u) >> 8) |
                             (DSD(DS_001088D8) & 0x0000FF00u));
}
```

- [ ] **Step 5: Delete the old `0x500C4` placeholder in `flow.c`**

Delete `static void input_pump(void) { host_pump(); }` and its forward declaration. The `game_loop` call site keeps its `input_pump();  /* 0x500C4 */` line, which now resolves to the ported sampler through `platform/input.h` (already included for `input_check_key`/`input_clear`). Confirm `host_pump()` is still reached each frame — `host_wait_vblank()` calls it at the loop tail — and keep a `/* PORT: ... */` note at the deleted site recording that the event-pump half moved to `host_wait_vblank()`.

- [ ] **Step 6: Add the host binding**

In `port/src/host.h`, before the closing `#endif`:

```c
/* PORT: the 16 input bits the game's bitfield carries, packed as the two key
 * bytes at DAT_00101514 + 0x2d8/0x2d9. Which physical key drives which bit is a
 * port choice: the original's mapping lives in a hardware keyboard handler and
 * BIOS scancode space SDL does not have. The binding table lives in host.c and
 * is the single place to change it. */
u16 host_key_bits(void);
```

In `port/src/host.c`, add the table and reader:

```c
/* PORT: the host key binding. Bit 0 is the coin input 0x11F28 debits against;
 * the rest are the held inputs the mode transitions and the menus read. */
static const SDL_Scancode k_input_bind[16] = {
    SDL_SCANCODE_5,      /* 0: coin */
    SDL_SCANCODE_1,      /* 1: player 1 start */
    SDL_SCANCODE_2,      /* 2: player 2 start */
    SDL_SCANCODE_UP,     /* 3 */
    SDL_SCANCODE_DOWN,   /* 4 */
    SDL_SCANCODE_LEFT,   /* 5 */
    SDL_SCANCODE_RIGHT,  /* 6 */
    SDL_SCANCODE_SPACE,  /* 7 */
    SDL_SCANCODE_A, SDL_SCANCODE_S, SDL_SCANCODE_D, SDL_SCANCODE_F,
    SDL_SCANCODE_G, SDL_SCANCODE_H, SDL_SCANCODE_J, SDL_SCANCODE_K,
};

u16 host_key_bits(void)
{
    const bool *st = SDL_GetKeyboardState(NULL);
    if (st == NULL) return 0u;
    u16 bits = 0u;
    for (int i = 0; i < 16; i++)
        if (st[k_input_bind[i]]) bits |= (u16)(1u << i);
    return bits;
}
```

- [ ] **Step 7: Run the tests green**

Run: `cmake --build build && PR_GAME_DIR=data/game/C ./build/run_tests`
Expected: `all checks passed`, 0 warnings.

- [ ] **Step 8: Commit**

```bash
git add port/src/platform/input.h port/src/platform/input.c port/src/game/flow.c port/src/host.h port/src/host.c port/tests/test_input.c
git commit -m "input: add the game input bitfield (0x500C4/0x50161/0x4F644)"
```

---

### Task 3: The select state and its helpers

`0x11F6C` is a four-phase machine over entries 0..5 that spawns an actor and draws a string pair per entry, then hands off to state 3. `0x1C6D4` is a table membership predicate and `0x33904` is the list iterator the title already uses as `title_retire_next`.

**This task is derivation-driven.** The decompiler drops register arguments across `0x11F6C` (it renders several as `extraout_*`), so Step 1 derives the body from the raw bytes before any code is written, and the code block in Step 4 carries explicit `STEP 1` markers for the values the derivation supplies. **No `STEP 1` marker may remain in the committed code**; substitute every one from the derivation.

**Files:**
- Modify: `port/src/game/flow.c`
- Create: `port/tests/test_frontend.c`
- Modify: `port/tests/test.h`
- Modify: `port/tests/run_tests.c`
- Modify: `port/CMakeLists.txt`
- Create: `docs/superpowers/plans/2026-09-19-frontend-input-derivations.md`

**Interfaces:**
- Consumes: `config_set_credit_row` (Task 1); `actors_reset`, `actor_spawn`, `actor_set_dead` from `actors.h`; `effects_spawn_pulse` from `effects.h`; the existing `flow.c` statics `title_input_reset`, `title_spawn_row`, `title_retire_next`, `text_cursor_set`, `game_string_get`; `DS_000F0A40`, `DS_000F0A44`, `DS_000F0A64`, `DS_000F0A68`, `DS_000F0A6E`, `DS_000F0A6F`, `DS_000F0A70`, `DS_0009AEE8`, `DS_00104528`, `DS_00107608`.
- Produces:
  - `static u32 frontend_list_next(u32 node);` — `0x33904` (the renamed `title_retire_next`)
  - `static u32 frontend_resource_known(u32 rec);` — `0x1C6D4`
  - `static void game_state_select(void);` — `0x11F6C`, called from `game_state_step` case 2

- [ ] **Step 1: Derive `0x11F6C` and `0x1C6D4` from the raw bytes**

Run (read-only):

```bash
python3 - <<'EOF'
from capstone import *
d = open('data/game/C/PRAGE.EXE', 'rb').read()
md = Cs(CS_ARCH_X86, CS_MODE_32)
for va, n in ((0x11f6c, 562), (0x1c6d4, 94)):
    print('===', hex(va))
    for i in md.disasm(d[va + 0x52E54: va + 0x52E54 + n], va):
        print(hex(i.address), i.bytes.hex(), i.mnemonic, i.op_str)
EOF
```

Record in `docs/superpowers/plans/2026-09-19-frontend-input-derivations.md`:

1. The phase dispatch on `DS_000F0A6F`: where each case begins and ends, and the fact that case 0 falls through into case 1.
2. The exact arguments to every call in phases 0 and 1: `0x2C06C`'s row value, `0x2AE14`'s arguments, the two `0x1C500`/`0x2F198` pairs, and the byte offset of the `DS_0009AEE8 + DS_000F0A6E * 0xC` gate. The raw uses Watcom register passing (`eax` first; `edx`/`ebx`/`ecx` where present), so read the preceding `mov`s, not the decompiler's argument list.
3. That phase 3's `DS_000F0A64` store is the literal 3: the raw at `0x1215e` is `mov edx, 3`, `0x2B150` preserves `edx` (`push edx` at `0x2B152` … `pop edx; pop ecx; pop ebx; ret` at `0x2B1E0`), and `0x1216a` stores `dx` as a word. Record the file offsets.
4. `0x1C6D4`'s membership set: the nine resource addresses it accepts and the two it rejects, with the `cmp`/`jb`/`jbe` offsets.
5. Whether phase 2's advance uses `DS_000F0A6E` as an index into `DS_0009AEE8`, and the entry record's field layout.

If the raw contradicts the design summary below, follow the raw and say so in the report.

**Design-verified summary:** phase 0 = `0x29D60` (a `ret`-only no-op), `0x2BAF4` (actors reset), `0x4F1D0` (input reset), `0x38B18` (spawn rows), `0x2C06C` (row write); `DS_000F0A6E = 0`, `DS_000F0A44 = 0`, and `DS_000F0A40 = 0` when `DS_00104528` byte 1 bit 1 is set; then falls through. Phase 1 = `0x2BAF4`, `0x4F1D0`, `0x38B18`, `DS_000F0A44 = 0x2AE14(0)`, the `0x33904` -> `0x1C6D4` -> `0x13E28` loop, the text or sprite branch, then `DS_000F0A70 = 2`, `DS_000F0A6F = 4`, `DS_000F0A68 = 0x5A`. Phase 2 = `DS_000F0A6E++`; at 6 sets `DS_000F0A70 = 3`, `DS_000F0A6F = 4`, `DS_000F0A68 = 0x1E`; otherwise `DS_000F0A6F = 1`. Phase 3 = `0x2B150(DS_000F0A44, 3)`, `DS_000F0A64 = 3`, `DS_000F0A6F = 0`. Phase 4 = count `DS_000F0A68` down; at expiry `DS_000F0A6F = DS_000F0A70`.

- [ ] **Step 2: Write the failing unit tests for the pure helpers**

Create `port/tests/test_frontend.c`:

```c
/* The front-end helpers 0x1C6D4 and 0x33904, and (under PR_FRONTEND_DUMP) the
 * state-2 driver. The driver calls game_init(), which may run once per process,
 * so it is env-gated exactly like test_title(). */
#include "game/flow.h"
#include "game/actors.h"
#include "mem.h"
#include "symbols.h"
#include "test.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int test_frontend(void)
{
    int before = g_failures;

    /* 0x1C6D4: membership over nine resource addresses read from mem[]. */
    {
        static const u32 known[9] = {
            0x80995Cu, 0x80997Cu, 0x809984u, 0x80998Cu, 0x809994u,
            0x80999Cu, 0x8099A4u, 0x8099ACu, 0x8099CCu,
        };
        static u32 cell;
        for (u32 i = 0; i < 9u; i++) {
            cell = known[i];
            CHECK_EQ_INT((int)frontend_resource_known(
                             (u32)(uintptr_t)&cell), 1);
        }
        cell = 0x809980u;                    /* between two members */
        CHECK_EQ_INT((int)frontend_resource_known((u32)(uintptr_t)&cell), 0);
        cell = 0x8099B0u;                    /* past the last member */
        CHECK_EQ_INT((int)frontend_resource_known((u32)(uintptr_t)&cell), 0);
    }

    /* 0x33904: the 0x10-stride table iterator at DS_00107608, bounded by
     * 0x107797 and stopping at a nonzero dword at +0x14. */
    {
        u32 first = frontend_list_next(0);
        CHECK(first != 0, "iterator finds the first live table entry");
        CHECK(first > DS_00107608 && first <= 0x107797u,
              "iterator stays inside the table");
        CHECK_EQ_INT((int)((first - DS_00107608) % 0x10u), 0);
    }

    const char *dump = getenv("PR_FRONTEND_DUMP");
    if (dump == NULL || dump[0] == '\0') {
        printf("test_frontend: PR_FRONTEND_DUMP unset, state-2 driver skipped\n");
        return g_failures - before;
    }
    return g_failures - before;
}
```

- [ ] **Step 3: Run it and watch it fail**

Run: `cmake --build build && PR_GAME_DIR=data/game/C ./build/run_tests`
Expected: FAIL — `frontend_resource_known` and `frontend_list_next` are undeclared.

- [ ] **Step 4: Implement the helpers and the selector**

In `port/src/game/flow.c`:

Rename `title_retire_next` to `frontend_list_next` (keep it `static`), update its two call sites in `game_state_title`, and keep its `0x33904` address tag.

Add:

```c
/* 0x1C6D4. Membership test: is the dword at `rec` one of the nine resource
 * addresses the raw's cmp/jb/jbe tree accepts? The set is what it accepts. */
static u32 frontend_resource_known(u32 rec)
{
    u32 v = DSD(rec);
    switch (v) {
    case 0x80995Cu: case 0x80997Cu: case 0x809984u: case 0x80998Cu:
    case 0x809994u: case 0x80999Cu: case 0x8099A4u: case 0x8099ACu:
    case 0x8099CCu:
        return 1u;
    default:
        return 0u;
    }
}

/* 0x11F6C: the six-entry selector. Phases: 0/1 draw an entry, 4 pauses, 2
 * advances the entry (and leaves for state 3 past the sixth), 3 hands off. */
static void game_state_select(void)
{
    switch (DSB(DS_000F0A6F)) {
    case 0:
        /* 0x29D60 is a ret-only no-op. */
        actors_reset();                        /* 0x2BAF4 */
        title_input_reset();                   /* 0x4F1D0 */
        title_spawn_row((const u32 *)(mem + 0x9AC1Cu), 0u, 0u);   /* 0x38B18 */
        config_set_credit_row(STEP1_ROW_VALUE);/* 0x2C06C */
        DSB(DS_000F0A6E) = 0;
        DSD(DS_000F0A44) = 0;
        if ((DSB(DS_00104528 + 1) & 2u) != 0u) DSD(DS_000F0A40) = 0;
        /* falls through, exactly as the raw does */
        /* FALLTHROUGH */
    case 1: {
        actors_reset();                        /* 0x2BAF4 */
        title_input_reset();                   /* 0x4F1D0 */
        title_spawn_row((const u32 *)(mem + 0x9AC1Cu), 0u, 0u);   /* 0x38B18 */
        DSD(DS_000F0A44) = actor_spawn(STEP1_SPAWN_ARGS);         /* 0x2AE14 */
        for (u32 node = frontend_list_next(0); node != 0;
             node = frontend_list_next(node)) {
            if (!frontend_resource_known(node))
                effects_spawn_pulse(node, STEP1_PULSE_ARG);       /* 0x13E28 */
        }
        if ((DSB(DS_00104528 + 1) & 2u) == 0u) {
            /* text branch: STEP1_TEXT_PAIRS, the second gated on
             * DSD(DS_0009AEE8 + DSB(DS_000F0A6E) * 0xC) != 0 */
        } else {
            DSD(DS_000F0A40) = STEP1_SPAWN_ARGS2;                 /* 0x2AE14 */
        }
        DSB(DS_000F0A70) = 2;
        DSB(DS_000F0A6F) = 4;
        DSW(DS_000F0A68) = 0x5A;
        return;
    }
    case 2:
        break;                                 /* handled after the switch */
    case 3:
        actor_set_dead(DSD(DS_000F0A44));      /* 0x2B150, edx = 3 */
        DSW(DS_000F0A64) = 3;                  /* literal 3, Step 1 item 3 */
        DSB(DS_000F0A6F) = 0;
        return;
    case 4:
        if (--DSW(DS_000F0A68) < 1)
            DSB(DS_000F0A6F) = DSB(DS_000F0A70);
        return;
    default:
        return;
    }

    /* phase 2: advance the entry, or leave the carousel past the sixth */
    DSB(DS_000F0A6E)++;
    if (DSB(DS_000F0A6E) == 6u) {
        DSB(DS_000F0A70) = 3;
        DSB(DS_000F0A6F) = 4;
        DSW(DS_000F0A68) = 0x1E;
        return;
    }
    DSB(DS_000F0A6F) = 1;
}
```

Every `STEP1_*` marker must be replaced with the Step 1 derivation's value. Replace the `case 2:` stub in `game_state_step` with `game_state_select();`.

- [ ] **Step 5: Run the unit tests green**

Run: `cmake --build build && PR_GAME_DIR=data/game/C ./build/run_tests`
Expected: `all checks passed`, 0 warnings.

- [ ] **Step 6: Add the state-2 driver and its determinism check**

Append to `port/tests/test_frontend.c` after the helper blocks, before the `dump` guard:

```c
    /* The state-2 driver runs the real init and loop for a fixed window, as the
     * title driver does, because game_init() may run once per process.
     * PR_FRONTEND_DUMP names a directory to receive the hash log. */
    const char *dir = getenv("PR_GAME_DIR");
    if (dir == NULL || dir[0] == '\0') dir = "data/game/C";
    game_set_game_dir(dir);
    game_init();
    actors_pin_anim_tick_zero(1);

    /* Enter state 2 at phase 0, as the title's own transition does. */
    DSW(DS_000F0A64) = 2;
    DSB(DS_000F0A6F) = 0;

    char log_path[1200];
    snprintf(log_path, sizeof log_path, "%s/select.log", dump);
    FILE *log = fopen(log_path, "w");
    CHECK(log != NULL, "state-2 hash log opens");

    u32 seen_entries = 0;
    for (int i = 0; i < 640; i++) {
        DSB(DS_000A81A8) = 1;          /* exactly one game_loop iteration */
        game_loop();
        if (DSB(DS_000F0A6E) < 6u) seen_entries |= 1u << DSB(DS_000F0A6E);
        if (log != NULL) {
            /* Hash the presented index buffer without reading pixels. */
            const u8 *fb = mem + DSD(DS_000E87A4);
            u32 h = 2166136261u;
            for (u32 b = 0; b < 320u * 200u; b++) h = (h ^ fb[b]) * 16777619u;
            fprintf(log, "%d %u %u\n", i, (unsigned)DSB(DS_000F0A6F), h);
        }
    }
    if (log != NULL) fclose(log);

    /* The raw's timeline: six entries, each drawn then paused, then state 3. */
    CHECK_EQ_INT((int)seen_entries, 0x3F);
    CHECK_EQ_INT((int)DSW(DS_000F0A64), 3);
    game_shutdown();
```

- [ ] **Step 7: Run the driver and check determinism**

```bash
mkdir -p /tmp/pr_s2_a /tmp/pr_s2_b
cmake --build build
PR_FRONTEND_DUMP=/tmp/pr_s2_a PR_GAME_DIR=data/game/C ./build/run_tests
PR_FRONTEND_DUMP=/tmp/pr_s2_b PR_GAME_DIR=data/game/C ./build/run_tests
diff /tmp/pr_s2_a/select.log /tmp/pr_s2_b/select.log && echo "state-2 frames deterministic"
```

Expected: `all checks passed` both runs; `diff` silent. Record both outputs and the `make verify`-relevant checks in the report.

- [ ] **Step 8: Register the test and commit**

Add `int test_frontend(void);` to `port/tests/test.h`, call `test_frontend();` in `run_tests.c` after `test_config();`, and add `tests/test_frontend.c` to the `run_tests` source list in `port/CMakeLists.txt`.

```bash
git add port/src/game/flow.c port/tests/test_frontend.c port/tests/test.h port/tests/run_tests.c port/CMakeLists.txt docs/superpowers/plans/2026-09-19-frontend-input-derivations.md
git commit -m "flow: add the select state and its helpers (0x11F6C/0x1C6D4/0x33904)"
```

---

### Task 4: Wire the coin poll, the input update and the row writers; docs and ladder

The layers exist but nothing calls them: the coin poll is still a `PORT:` no-op in `game_state_step`, `game_frame` does not build the input masks, and `game_init` does not make the raw's init-time row write. This task connects them, records the surviving `DS_00105C05` stand-in with its raw site, and runs the full ladder.

**Files:**
- Modify: `port/src/game/flow.c`
- Modify: `port/spec/game_flow.md`
- Modify: `README.md`
- Create: `docs/superpowers/plans/2026-09-19-frontend-input-report.md`

**Interfaces:**
- Consumes: `config_credit_ready`, `config_credit_spend`, `config_set_credit_row_init` (Task 1); `input_pump`, `input_state_update` (Task 2); `game_state_select` (Task 3); `host_key_bits` from `host.h`.
- Produces: a live coin poll; `DS_001088E4`/`DS_001088D8` built each frame; the init-time row write.

- [ ] **Step 1: Derive `0x11F28`'s call sites from the raw bytes**

```bash
python3 - <<'EOF'
from capstone import *
d = open('data/game/C/PRAGE.EXE', 'rb').read()
md = Cs(CS_ARCH_X86, CS_MODE_32)
for i in md.disasm(d[0x11d04 + 0x52E54: 0x11d04 + 0x52E54 + 64], 0x11d04):
    print(hex(i.address), i.bytes.hex(), i.mnemonic, i.op_str)
EOF
```

Record in the cycle report: the register flow of the two `0x11F28` calls at `0x11D15` and `0x11D28`, the event code each receives, and what the `|= 2` / divert does when either reports an accepted credit. Follow the raw where it contradicts the decompiler.

- [ ] **Step 2: Replace the coin-poll no-op in `game_state_step`**

In `port/src/game/flow.c`, replace the `PORT:` comment inside `if (DSB(DS_00104B1D) == 0)`. Its shape (event codes and order from Step 1):

```c
    if (DSB(DS_00104B1D) == 0) {
        /* 0x11D15/0x11D28: the coin poll. 0x11F28 tests the event code's mask
         * in DS_0009ACBC against the newly-pressed bits in DS_001088E4 and
         * debits one credit through 0x2CA7C. */
        if (config_credit_ready() &&
            (DSD(DS_0009ACBC + STEP1_EVENT_CODE * 4u) & DSD(DS_001088E4)) != 0u) {
            (void)config_credit_spend(1u);
        }
        /* PORT: the raw's accepted-credit divert (0x32970/0x257A4) is
         * unported; the state dispatch below still runs. */
    }
```

The raw calls `0x11F28` twice; if Step 1 shows two different event codes, emit two guarded blocks. **No `STEP1_` marker may remain in the committed code.**

- [ ] **Step 3: Build the input masks and sample the host bitmap each frame**

In `game_frame`, before the `DS_00104B00` switch, add where `0x24C5C` calls `0x4F644` (`0x24C6E`):

```c
    input_state_update();                              /* 0x4F644 */
```

In `game_loop`, fill the key bitmap from the host immediately before the existing `input_pump()` call (which Task 2 repointed at the ported sampler):

```c
        {
            /* PORT: the host fills the key bitmap 0x500C4 samples; the binding
             * is host.c's table (host_key_bits()). */
            u16 bits = host_key_bits();
            u8 *k = mem + DSD(DS_00101514);
            k[0x2d8] = (u8)(bits >> 8);
            k[0x2d9] = (u8)bits;
        }
        input_pump();                        /* 0x500C4 */
```

Add `#include "host.h"` if absent (`platform/input.h` is already included).

- [ ] **Step 4: Port the init-time row write and re-document the stand-in**

In `game_init`'s overlay block, replace the lone row seed with the raw's init write plus the documented stand-in:

```c
    /* 0x20CCC: the init chain writes the overlay row to 0x1D. */
    config_set_credit_row_init();
    /* The captured title shows row 1, written by 0x2C06C(1) at 0x110CE inside
     * the attract machine 0x11000, which is unported (4d). Until 4d lands the
     * port supplies that value here rather than fitting a constant into
     * config.c. TODO(verify): remove when 0x11000 lands. */
    DSB(DS_00105C05) = 1;
```

Confirm with `make title-oracle` that the title still matches, and record in the report that the stand-in survives by design.

- [ ] **Step 5: Update the docs and write the cycle report**

Update `port/spec/game_flow.md` to name the input bitfield, the credit layer and the select state, and to record that state 2's exit advances to the state-3 stub. Update `README.md` only if its coverage paragraph is now stale.

Create `docs/superpowers/plans/2026-09-19-frontend-input-report.md` recording: the modules and their ownership; the derived bodies and any point where the raw contradicted the design; the Step 1 answers from Task 3 and Task 4; the determinism-oracle result (both runs, the diff); the surviving `DS_00105C05` stand-in with its raw site; and the declared gaps (no pixel oracle for state 2, the attract machine, states 3-9, `0x10DB0`/`0x10E18` deferred, the voice system, the fight engine).

- [ ] **Step 6: Full ladder**

Run: `rm -rf build && make verify`
Expected: exit 0, 0 warnings, `all checks passed`, the title oracle 0 unexplained on both captures, `smk_compare` 120/120 + 41/41, the capture-comparison line unchanged (this cycle does not touch audio), `symbols.h` byte-identical.

- [ ] **Step 7: Commit**

```bash
git add port/src/game/flow.c port/spec/game_flow.md README.md docs/superpowers/plans/2026-09-19-frontend-input-report.md
git commit -m "flow: wire the coin poll and the input update; record the outcome"
```

---

## Self-Review

**Spec coverage:** §1 goal -> Tasks 1-4. §2.1 boundary -> Tasks 1-3. §2.2 structure -> Tasks 1 (config.c), 2 (input.c), 3 (flow.c). §2.3 proof contract -> Task 3 Steps 6-7 (determinism oracle) + DoD 4. §2.4 host binding -> Task 2 Step 6. §2.5 deferred transitions -> Task 4 Step 5 (declared). §2.6 stand-in -> Task 4 Step 4. §3 evidence -> the code in Tasks 1-2 and the derivations in Task 3 Step 1 / Task 4 Step 1. §4 components -> Tasks 1-3. §5 pins -> Task 4 Steps 2-4. §6 DoD -> Tasks 1-4. §7 gaps -> Task 4 Step 5. §8 risks -> Task 2 Steps 1/4 (level vs edge) and Task 3/4 Step 1 derivations.

**Placeholder scan:** Tasks 1 and 2 carry their complete code. Task 3 is explicitly derivation-driven: its code block carries `STEP1_*` markers that Step 4's instructions require be substituted from Step 1, and the plan states no marker may be committed. Task 4 Step 2 carries one such marker under the same rule. This mirrors the pattern this repository used for `0x2D6F8` in cycle 4b-A, where the decompiler was likewise unreliable; it is a deliberate derivation hand-off, not an unfinished step.

**Type consistency:** `config_not_free_play`/`config_has_credit`/`config_credit_ready`/`config_credit_take`/`config_credit_spend` all return `u32`; `config_set_credit_row` takes `u8` and returns `void`; `config_set_credit_row_init` takes and returns `void`. `input_pump` returns `u32` (Task 2 Step 5 replaces the `static void` placeholder it collides with). `input_select_bits(u32)->u32`, `input_joystick_device(u32)->u32`, `input_state_update(void)->void`, `host_key_bits(void)->u16`. `frontend_list_next`/`frontend_resource_known` are `static u32 (u32)`; `game_state_select` is `static void (void)`. Names match across tasks, tests and the spec.

**Ordering:** Task 2 Step 5 deletes `flow.c`'s `static void input_pump(void)` placeholder in the same task that adds the ported `input_pump()`, so the symbol never shadows; the `game_loop` call site keeps its line and simply changes meaning. Task 4 Step 3 then adds the host bitmap fill ahead of it. Building after Task 2 is the check that the deletion landed.

**Known derivation hand-offs:** Task 3 Step 4 and Task 4 Step 2 each carry `STEP1_*` markers that must be substituted from that task's Step 1 raw derivation before committing. This is the repository's established pattern for decompiler-unreliable functions (cycle 4b-A's `0x2D6F8`).
