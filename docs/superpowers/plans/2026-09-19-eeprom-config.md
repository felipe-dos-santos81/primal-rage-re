# EEPROM/Config Core (4b-A) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Port the game's packed EEPROM/config field getter and setter, the magic/validate/defaults path, and wire the port's existing config consumers so `DS_00104528` and the credit counter stop being pinned literals.

**Architecture:** A new `port/src/game/config.{c,h}` owns the data layer. The 63-entry descriptor table (`mem[]` obj-0 VA `0x2D300`) and the config byte region (`DS_00105D88`, backed by `0x105DE1`/`0x105DAF`) are read from the loaded image, so no table or default is transcribed into C. The storage layer (`0x2E990`, `0x2D638`, the `0x80CE4` image, `0x2D4EC`) and the defaults writer's screen/storage side-effects (`0x1AE20`, `0x2EA78`) are declared no-ops: the port behaves as a fresh EEPROM.

**Tech Stack:** C11 (fixed port profile), the existing `make verify` ladder, capstone for raw-byte derivations.

## Global Constraints

- Spec: `docs/superpowers/specs/2026-09-19-eeprom-config-design.md`. Base is `main` at `0fab6fc`.
- `data/` is read-only; `tools/` must not be modified.
- No new dependencies (capstone is already used under `tools/`).
- `mem[]` holds original state at original offsets; never write `mem[0xA0000]`.
- SDL only in `port/src/host.c` and `port/src/main.c`.
- Comments in `port/src` only `/* PORT: ... */`, `/* TODO(verify): ... */`, and address tags.
- Tests: only `CHECK(cond,msg)` / `CHECK_EQ_INT(a,b)` (no message arg on `CHECK_EQ_INT`); declared in `port/tests/test.h`, called from `run_tests.c`.
- `port/src/symbols.h` must regenerate byte-identically: `python3 tools/gen_symbols.py port/decomp port/src/symbols.h`.
- Build 0 warnings. Stage explicit paths only; never `git add -A`. Commit style `<area>: <what changed>`.
- **Register-level fidelity, no fitted constants.** Every value comes from the raw bytes of `data/game/C/PRAGE.EXE` or from an existing port constant. `file_offset = va + 0x52E54` for obj-0 **code**; where the decompiler and the raw bytes disagree, the raw bytes win.
- **Immediate base trap.** In this module, `0x9C8` (a message pointer) is an **obj-1** data address (`+0x80000` → `0x809C8`), while `0x1D300` (the descriptor table) and `0x22EB4` (the menu table) are **obj-0** addresses (`+0x10000` → `0x2D300` / `0x32EB4`). Determine each immediate's base per site; reading `0x1D300` as obj-1 yields a meaningless `0xAAAA/0x5555` pattern.
- The storage image (`0x80CE4`, length `0x7F8`) and everything that touches it are **no-ops** (spec §7).

---

### Task 1: The config field getter (`0x2D974`)

`0x2D974` reads a packed field out of the config byte region. Its walk is intricate and the decompiler renders it poorly, so the exact translation is given below and verified instruction-by-instruction in the spec (§3). This task creates the module and the getter with a literal-expectation test that does not depend on the setter.

**Files:**
- Create: `port/src/game/config.h`
- Create: `port/src/game/config.c`
- Create: `port/tests/test_config.c`
- Modify: `port/tests/test.h`
- Modify: `port/tests/run_tests.c`
- Modify: `port/CMakeLists.txt`

**Interfaces:**
- Consumes: `DSD`/`DSB` from `mem.h`; `DS_0002D300`, `DS_00105DAF`, `DS_00105DE1` from `symbols.h`.
- Produces: `u32 config_field_get(u32 field);` — returns the packed value, or `0xFFFFFFFFu` when `field > 0x3E`.

- [ ] **Step 1: Write the failing test**

Create `port/tests/test_config.c`:

```c
/* port/tests/test_config.c */
#include "test.h"

#include "game/config.h"
#include "mem.h"
#include "symbols.h"

/* The descriptor table lives in the loaded image (obj-0 VA 0x2D300). These tests
 * run after test_le() has loaded PRAGE.EXE into mem[]. */
int test_config(void)
{
    /* The table must be the real one; a missing load would otherwise read zeros
     * and silently pass. */
    CHECK_EQ_INT((int)DSD(DS_0002D300 + 0x29u * 4u), 0x1D980);

    /* Save the config region: the tests below seed and mutate it. */
    u8 saved[0x100];
    for (u32 i = 0; i < 0x100u; i++) saved[i] = DSB(DS_00105D88 + i);

    /* Field 0x29 has bitpos 102, width 8, no trailing byte: the getter reads
     * DS_00105DE1 indices 54,53,52,51, giving b54<<24 | b53<<16 | b52<<8 | b51. */
    for (u32 i = 0; i < 0x70u; i++)
        DSB(DS_00105DE1 + i) = (u8)i;
    CHECK_EQ_INT((int)config_field_get(0x29u), 0x36353433);

    /* Field 0x2A has bitpos 110, width 1: the low nibble of byte 55. */
    CHECK_EQ_INT((int)config_field_get(0x2Au), 55u & 0xFu);

    /* Field 0x35 has bitpos 131, width 4 (odd start). With a ramp, the value is
     * ((67 & 0xf) << 8 | 66) << 4 | (65 >> 4) = 0x3424. */
    CHECK_EQ_INT((int)config_field_get(0x35u), 0x3424);

    /* Field 0x00 has bitpos 0, width 2 and trailing index 1: the value is byte 0
     * (the even-start walk reads it whole) shifted up 8 and OR'd with
     * DS_00105DAF + 1. */
    DSB(DS_00105DAF + 1u) = 0xA1u;
    CHECK_EQ_INT((int)config_field_get(0x00u), 0xA1);

    /* Out-of-range fields return -1. */
    CHECK_EQ_INT((int)config_field_get(0x3Fu), -1);
    CHECK_EQ_INT((int)config_field_get(0x100u), -1);

    for (u32 i = 0; i < 0x100u; i++) DSB(DS_00105D88 + i) = saved[i];
    return 0;
}
```

Add `int test_config(void);` to `port/tests/test.h` and call `test_config();` in `port/tests/run_tests.c` after `test_effects();`. Add `src/game/config.c` to the `prage_core` source list and `tests/test_config.c` to the `run_tests` source list in `port/CMakeLists.txt`.

- [ ] **Step 2: Run it and watch it fail**

Run: `cmake --build build && PR_GAME_DIR=data/game/C ./build/run_tests`
Expected: FAIL — `config_field_get` is undeclared (compile error), or the assertions fail once a stub returns 0.

- [ ] **Step 3: Implement the header**

Create `port/src/game/config.h`:

```c
/* port/src/game/config.h */
#ifndef PRAGE_GAME_CONFIG_H
#define PRAGE_GAME_CONFIG_H

#include "../types.h"

/* The EEPROM/config data layer (0x2Dxxx). The original keeps 63 packed fields in
 * the byte region DS_00105D88..(+0x1000) and describes each field's bit position
 * and width in the obj-0 descriptor table at 0x2D300. The port reads both out of
 * mem[]; no table or value is transcribed. The storage image (0x80CE4) and the
 * save/load I/O are declared no-ops this cycle (spec §7). */

/* 0x2D974. Reads the packed field `field` (0x00..0x3E) out of the config byte
 * region. Returns 0xFFFFFFFF when field > 0x3E. */
u32 config_field_get(u32 field);

#endif /* PRAGE_GAME_CONFIG_H */
```

- [ ] **Step 4: Implement the getter**

Create `port/src/game/config.c`:

```c
/* port/src/game/config.c */
#include "game/config.h"

#include "../mem.h"
#include "../symbols.h"

/* 0x2D974. The walk mirrors the raw exactly (spec §3): the descriptor gives a bit
 * position and a width; the value is assembled from the byte/nibble array ending
 * at DS_00105DE1 + ((bitpos + width) >> 1) and walking downward, with the final
 * unit depending on whether the width lands on a nibble boundary, then an optional
 * trailing byte from DS_00105DAF + (descriptor & 0x3f). */
u32 config_field_get(u32 field)
{
    if (field > 0x3Eu) return 0xFFFFFFFFu;

    u32 d = DSD(DS_0002D300 + field * 4u);
    u32 width  = ((d >> 14) & 7u) + 1u;
    u32 cursor = ((d >> 6) & 0xFFu) + width;   /* 0x13D9A1: bitpos + width */
    u32 idx    = cursor >> 1;                  /* 0x13D9A5: sar ecx,1 */
    u32 value;
    s32 ebx;

    if (cursor & 1u) {                          /* odd: seed is the low nibble */
        value = DSB(DS_00105DE1 + idx) & 0x0Fu;
        ebx = (s32)idx;
        width -= 1u;
    } else {
        value = 0u;
        ebx = (s32)idx;
    }

    while (width != 0u) {
        ebx -= 1;                               /* 0x13D9C5: dec ebx before the read */
        if (width == 1u) {                      /* 0x13D9CB: final high nibble */
            u32 hi = (DSB(DS_00105DE1 + (u32)ebx) >> 4) & 0x0Fu;
            value = (value << 4) | hi;
            break;
        }
        value = (value << 8) | DSB(DS_00105DE1 + (u32)ebx);   /* 0x13D9E0 */
        width -= 2u;
    }

    u32 trail = d & 0x3Fu;                      /* 0x13D9F2 */
    if (trail != 0u)
        value = (value << 8) | DSB(DS_00105DAF + trail);
    return value;
}
```

- [ ] **Step 5: Run the tests green**

Run: `cmake --build build && PR_GAME_DIR=data/game/C ./build/run_tests`
Expected: `all checks passed`.

- [ ] **Step 6: Commit**

```bash
git add port/src/game/config.c port/src/game/config.h port/tests/test_config.c port/tests/test.h port/tests/run_tests.c port/CMakeLists.txt
git commit -m "config: add the packed field getter (0x2D974)"
```

---

### Task 2: The config field setter (`0x2DA0C`)

`0x2DA0C` is the getter's inverse: it writes the field's bytes back, preserving the
nibbles it does not own, and raises the `DS_00105DD8` dirty flags. Its three
`0x2D4EC` calls touch the storage image and are no-ops this cycle.

**Files:**
- Modify: `port/src/game/config.h`
- Modify: `port/src/game/config.c`
- Modify: `port/tests/test_config.c`

**Interfaces:**
- Consumes: `config_field_get` (Task 1).
- Produces: `u32 config_field_set(u32 field, u32 value);` — returns `0` on success, `0xFFFFFFFFu` when `field > 0x3E`.

- [ ] **Step 1: Write the failing test**

Append to `port/tests/test_config.c` (before `return 0;`):

```c
    {
        /* set -> get round-trips for even/odd starts, width 8 / width 4 / width 1
         * and the trailing-byte case. These are the fields the consumers use plus
         * the boundary shapes of the walk. */
        static const u32 saved_lo = 0x00105DAFu, saved_hi = 0x00105DE1u;
        u8 lo[0x20], hi[0x70];
        for (u32 i = 0; i < 0x20u; i++) lo[i] = DSB(saved_lo + i);
        for (u32 i = 0; i < 0x70u; i++) hi[i] = DSB(saved_hi + i);

        config_field_set(0x29u, 0x0000ABCDu);
        CHECK_EQ_INT((int)config_field_get(0x29u), 0xABCD);

        config_field_set(0x35u, 0x0000000Au);
        CHECK_EQ_INT((int)config_field_get(0x35u), 0x000A);

        config_field_set(0x00u, 0x0000080Fu);
        CHECK_EQ_INT((int)config_field_get(0x00u), 0x80F);

        config_field_set(0x2Au, 0x00000003u);
        CHECK_EQ_INT((int)config_field_get(0x2Au), 3);

        /* The setter raises DS_00105DD8: |1 for a trailing byte, |6 always. */
        DSB(DS_00105DD8) = 0;
        config_field_set(0x00u, 0x10u);
        CHECK_EQ_INT((int)DSB(DS_00105DD8), 0x7);
        DSB(DS_00105DD8) = 0;
        config_field_set(0x29u, 0u);
        CHECK_EQ_INT((int)DSB(DS_00105DD8), 0x6);

        CHECK_EQ_INT((int)config_field_set(0x3Fu, 0u), -1);
        CHECK_EQ_INT((int)config_field_set(0x100u, 0u), -1);

        for (u32 i = 0; i < 0x20u; i++) DSB(saved_lo + i) = lo[i];
        for (u32 i = 0; i < 0x70u; i++) DSB(saved_hi + i) = hi[i];
    }
```

- [ ] **Step 2: Run it and watch it fail**

Run: `cmake --build build && PR_GAME_DIR=data/game/C ./build/run_tests`
Expected: FAIL — `config_field_set` undeclared.

- [ ] **Step 3: Implement**

Add to `port/src/game/config.h`:

```c
/* 0x2DA0C. Writes `value` into the packed field `field` (0x00..0x3E), preserving
 * nibbles the field does not own and raising DS_00105DD8 (|1 for a trailing byte,
 * |6 always). Returns 0, or 0xFFFFFFFF when field > 0x3E. The storage-image
 * maintenance (0x2D4EC) is a no-op this cycle. */
u32 config_field_set(u32 field, u32 value);
```

Add to `port/src/game/config.c`:

```c
/* 0x2D4EC. Maintains the EEPROM storage image at 0x80CE4, which the port does not
 * keep (no save/load I/O, spec §7). The setter's three calls are declared no-ops
 * so a later persistence cycle has the call sites already in place. */
static void config_storage_touch(u32 kind, u32 value)
{
    (void)kind;
    (void)value;
}

/* 0x2DA0C. Inverse of config_field_get (spec §3). */
u32 config_field_set(u32 field, u32 value)
{
    if (field > 0x3Eu) return 0xFFFFFFFFu;

    u32 d = DSD(DS_0002D300 + field * 4u);

    u32 trail = d & 0x3Fu;
    if (trail != 0u) {
        u8 flags = DSB(DS_00105DD8);
        DSB(DS_00105DAF + trail) = (u8)value;      /* 0x2DA34 */
        flags |= 1u;                               /* 0x2DA3B */
        value >>= 8;
        DSB(DS_00105DD8) = flags;
        config_storage_touch(0u, value);           /* 0x2DA49 */
    }

    DSB(DS_00105DD8) |= 6u;                        /* 0x2DA4E */

    u32 bitpos = (d >> 6) & 0xFFu;
    u32 width  = ((d >> 14) & 7u) + 1u;
    s32 ebx = (s32)bitpos >> 1;                    /* 0x2DA6B: sar ebx,1 */

    if (bitpos & 1u) {                             /* 0x2DA6D */
        u8 low = DSB(DS_00105DE1 + (u32)ebx) & 0x0Fu;
        ebx += 1;                                  /* 0x2DA8A: inc ebx */
        width -= 1u;
        /* 0x2DA90 writes at [ebx + 0x85de0], i.e. DS_00105DE1 + (bitpos>>1). */
        DSB(DS_00105DE0 + (u32)ebx) = (u8)(low | ((value & 0x0Fu) << 4));
        value >>= 4;
    }

    for (;;) {
        if (width == 0u) break;                    /* 0x2DA96 */
        if (width == 1u) {                         /* 0x2DA9A */
            u8 high = DSB(DS_00105DE1 + (u32)ebx) & 0xF0u;
            DSB(DS_00105DE1 + (u32)ebx) = (u8)(high | (value & 0x0Fu));
            break;
        }
        width -= 2u;
        DSB(DS_00105DE1 + (u32)ebx) = (u8)value;   /* 0x2DAB9 */
        ebx += 1;
        value >>= 8;
    }

    config_storage_touch(1u, 0u);                  /* 0x2DACA */
    config_storage_touch(2u, 0u);                  /* 0x2DAD4 */
    return 0u;
}
```

- [ ] **Step 4: Run the tests green**

Run: `cmake --build build && PR_GAME_DIR=data/game/C ./build/run_tests`
Expected: `all checks passed`.

- [ ] **Step 5: Commit**

```bash
git add port/src/game/config.c port/src/game/config.h port/tests/test_config.c
git commit -m "config: add the packed field setter (0x2DA0C)"
```

---

### Task 3: Defaults (`0x2CADC`), the menu-default parser (`0x2CCD0`) and validate (`0x2D6F8`)

The magic check at `DS_00105E30` decides whether a stored config is valid. With no
storage, it always fails and the defaults path runs. The decompiler's rendering of
`0x2D6F8` is unreliable (it drops register arguments), so Step 1 derives the
no-storage behaviour from the raw bytes before any code is written.

**Files:**
- Modify: `port/src/game/config.h`
- Modify: `port/src/game/config.c`
- Modify: `port/tests/test_config.c`

**Interfaces:**
- Consumes: `config_field_get`/`config_field_set` (Tasks 1-2).
- Produces:
  - `u32 config_menu_default_bits(u32 table);` (`0x2CCD0`)
  - `void config_set_defaults(void);` (`0x2CADC`)
  - `void config_validate(void);` (`0x2D6F8`)

- [ ] **Step 1: Derive the no-storage behaviour of `0x2D6F8` from the raw bytes**

Run (read-only):

```bash
python3 - <<'EOF'
from capstone import *
d = open('data/game/C/PRAGE.EXE','rb').read()
md = Cs(CS_ARCH_X86, CS_MODE_32)
def dis(va, n):
    for i in md.disasm(d[va + 0x52E54: va + 0x52E54 + n], va):
        print(hex(i.address), i.bytes.hex(), i.mnemonic, i.op_str)
dis(0x2d6f8, 633)
dis(0x2cadc, 150)
dis(0x2ccd0, 93)
EOF
```

Record in `docs/superpowers/plans/2026-09-19-eeprom-config-derivations.md`:
1. Every write `0x2D6F8` performs on the no-storage path (the magic at `DS_00105E30` as four bytes, `DS_00105DD8`, `DS_00105DA4`/`DS_00105DA5`, `DS_00105DA7`, `DS_00105E2F`), with file offsets.
2. The exact magic test (which four bytes, which order) and the exact rewrite.
3. Whether the `0x61A70` calls between the flag writes have a config effect or only side-effects on the unported pset pool (they appear elsewhere as a pool clear — record which, with evidence).
4. `0x2CCD0`'s record layout and the `'*'` test, with offsets.
5. Confirmation that the version-compare arms and the `0x2DE98` high-score call are unreachable/no-op on the no-storage path.

- [ ] **Step 2: Write the failing test**

Append to `port/tests/test_config.c`:

```c
    {
        /* A zeroed config region fails the magic and the defaults path runs:
         * the magic is rewritten, DS_00105DA7 is set, and the four fields the
         * defaults writer touches carry the values from the obj-0 default table. */
        u8 saved[0x100];
        for (u32 i = 0; i < 0x100u; i++) saved[i] = DSB(DS_00105D88 + i);
        for (u32 i = 0; i < 0x100u; i++) DSB(DS_00105D88 + i) = 0;

        config_validate();
        CHECK_EQ_INT((int)DSD(DS_00105E30), (int)0x9C94D2C4u);
        CHECK_EQ_INT((int)DSB(DS_00105DA7), 1);

        /* Field 0x35 and 0x37 default to 0xA0; field 0x2A keeps the top six bits
         * and gets 3 in the low two. */
        CHECK_EQ_INT((int)config_field_get(0x35u), 0xA0);
        CHECK_EQ_INT((int)config_field_get(0x37u), 0xA0);
        CHECK_EQ_INT((int)(config_field_get(0x2Au) & 0x3u), 3);

        /* Field 0x29's default is the menu table's '*' selection; it is whatever
         * the shipped table says, so assert it round-trips through the getter
         * rather than pinning a hand-derived literal. */
        CHECK_EQ_INT((int)config_field_get(0x29u),
                     (int)config_menu_default_bits(0x32EB4u));

        for (u32 i = 0; i < 0x100u; i++) DSB(DS_00105D88 + i) = saved[i];
    }
```

- [ ] **Step 3: Run it and watch it fail**

Run: `cmake --build build && PR_GAME_DIR=data/game/C ./build/run_tests`
Expected: FAIL — `config_validate` / `config_menu_default_bits` undeclared.

- [ ] **Step 4: Implement from the derivation**

Add to `port/src/game/config.h`:

```c
/* 0x2CCD0. Walks the obj-0 menu descriptor list at `table` and returns the
 * bitfield of the entries whose string starts with '*' (the default selection),
 * each placed at its record's shift. */
u32 config_menu_default_bits(u32 table);

/* 0x2CADC. Writes the default config fields. The message draw (0x2F198 and the
 * unported screen setup 0x1AE20) and the storage write 0x2EA78 are declared
 * no-ops (spec §4). */
void config_set_defaults(void);

/* 0x2D6F8. Validates the stored config against the magic at DS_00105E30 and runs
 * the defaults path when it fails. With the storage layer stubbed the magic always
 * fails, so this always takes the defaults path, as on a fresh machine. */
void config_validate(void);
```

Add to `port/src/game/config.c` the three functions, transcribed from Step 1's
derivation. `config_menu_default_bits` implements `0x2CCD0` exactly as the
decompile shows but with the raw offsets (records are 5 dwords: `[0]` presence,
`[1]` shift, `[2]` count, `[4]` pointer to 2-dword entries whose `[0]` is a
`char *`; `[5]` is the next record):

```c
/* 0x2CCD0. */
u32 config_menu_default_bits(u32 table)
{
    u32 bits = 0u;
    u32 rec = table;
    while (DSD(rec) != 0u) {
        u32 shift   = DSD(rec + 4u) & 0x1Fu;
        u32 count   = DSD(rec + 8u);
        u32 entries = DSD(rec + 16u);
        u32 found   = 0u;
        for (u32 i = 0u; i < count && found == 0u; i++) {
            u32 str = DSD(entries + i * 8u);
            if (DSB(str) == (u8)'*') {
                found = 1u;
                bits |= i << shift;
            }
        }
        rec = DSD(rec + 20u);
    }
    return bits;
}

/* 0x2CADC. Order and values are the raw's: set(0x29, menu_default_bits(0x32EB4)),
 * set(0x35, 0xA0), set(0x37, 0xA0), set(0x2A, (get(0x2A) & 0xFC) | 3). The
 * message draw and the storage write are no-ops. */
void config_set_defaults(void)
{
    u32 v29 = config_menu_default_bits(0x32EB4u);
    config_field_set(0x29u, v29);
    config_field_set(0x35u, 0xA0u);
    config_field_set(0x37u, 0xA0u);
    u32 v2a = config_field_get(0x2Au);
    config_field_set(0x2Au, (v2a & 0xFCu) | 3u);
}

/* 0x2D6F8, no-storage path (Step 1 records the exact writes). */
void config_validate(void)
{
    DSB(DS_00105DA5) = 0;
    DSB(DS_00105DA4) = 0;
    /* PORT: 0x2D638's storage read and the two 0x2E990 reads are no-ops, so the
     * magic at DS_00105E30 cannot match and the defaults path runs. */
    DSB(DS_00105DD8) |= 6u;
    config_set_defaults();
    DSD(DS_00105E30) = 0x9C94D2C4u;
    DSB(DS_00105DA7) = 1u;
}
```

If Step 1 shows `0x2D6F8` writes the magic differently (e.g. four bytes in a
different order) or writes `DS_00105E2F`, mirror the raw exactly and say so in the
report; do not keep the shape above on faith.

- [ ] **Step 5: Run the tests green**

Run: `cmake --build build && PR_GAME_DIR=data/game/C ./build/run_tests`
Expected: `all checks passed`.

- [ ] **Step 6: Commit**

```bash
git add port/src/game/config.c port/src/game/config.h port/tests/test_config.c docs/superpowers/plans/2026-09-19-eeprom-config-derivations.md
git commit -m "config: add the defaults path and the magic validate (0x2CADC/0x2D6F8)"
```

---

### Task 4: Wire the consumers, attempt the unseed, docs and the ladder

The module now has real callers. This task replaces the pinned `DS_00104528`
literal with the real config read, ports `0x2C304`'s credit initializer, attempts to
remove the title-overlay seeds, and records the outcome.

**Files:**
- Modify: `port/src/game/flow.c`
- Modify: `port/spec/game_flow.md`
- Modify: `README.md`
- Create: `docs/superpowers/plans/2026-09-19-eeprom-config-report.md`

**Interfaces:**
- Consumes: `config_validate`, `config_field_get` from `game/config.h`.
- Produces: `game_init` reads `DS_00104528` from config; `DS_00105C00` is derived from config.

- [ ] **Step 1: Derive `0x20C5D-0x20CC2` and `0x2C304`'s call site from the raw bytes**

Disassemble `0x20C5D` (the three derived globals) and `0x2C304` plus its caller
`0x10E80`, and the master init `0x2F9CC`. Record in the report:
1. The exact expressions for `DS_00105B3A`, `DS_001088D0`, `DS_0010452C` from `v`.
2. Where `0x2C304` is called from `0x10E80` relative to the rest of the init chain.
3. Where `0x2D6F8` sits in `0x2F9CC` relative to `0x13ADC` (`effects_init`) and to the `0x1BEC4` chain the port's `game_init` transcribes, so the port's `config_validate()` call placement can be justified or corrected.

- [ ] **Step 2: Wire `game_init`**

In `port/src/game/flow.c`, at `game_init`'s config block (`~:698-708`), replace the
pinned literal with the real read and port `0x2C304`:

```c
    /* PORT: 0x20C5D-0x20CC2: DS_00104528 = 0x2D974(0x29) and the three globals
     * derived from v. config_validate() (0x2D6F8) has run, so on a fresh image v
     * is whatever the defaults path wrote. */
    config_validate();          /* 0x2D6F8 */
    u32 v = config_field_get(0x29u);
    DSD(DS_00104528) = v;       /* 0x20C6D */
    DSB(DS_00105B3A) = (u8)((v & 0x100u) >> 4);        /* 0x20C9F */
    DSD(DS_001088D0) = (v & 0xFu) * 5u + 0x1Eu;        /* 0x20CB0 */
    DSB(DS_0010452C) = (u8)((v & 0xF0u) >> 4);         /* 0x20CC2 */
    /* PORT: 0x2C304: DS_00105C00 = ((0x2D974(0x29) & 0xF0000) >> 16) + 1. */
    DSD(DS_00105C00) = ((config_field_get(0x29u) & 0xF0000u) >> 16) + 1u;
```

`config_validate()` must run before the reads. If Step 1 shows `0x2F9CC` places it
earlier in the chain, move it and say why in the report.

- [ ] **Step 3: Attempt the overlay unseed**

Delete the two seeding lines `DSD(DS_00105C00) = 5;` and
`DSB(DS_00105C05) = 1;` in `game_init`'s overlay block (they carry the
`TODO(verify)` comment above them), keeping a `/* PORT: ... */` note that
`DS_00105C00` now comes from `0x2C304`'s config read. Then:

Run: `cmake --build build && make title-oracle`
Expected: either 0 unexplained (the defaults path reproduces the capture) or a
residual on the CREDITS line. Record which. If it does not go green, restore the
`DS_00105C00 = 5` seed **only**, with a comment stating the derived value
(`config_field_get(0x29)` derived credit) and that the captured runtime config is
not in the shipped image, and record the gap in the report. Do not fit a value.

- [ ] **Step 4: Update the docs and write the report**

Update `port/spec/game_flow.md`'s config/overlay notes to name the config module and
its consumers. Update `README.md` only if its coverage paragraph is now stale.

Create `docs/superpowers/plans/2026-09-19-eeprom-config-report.md` recording: the
module and its no-op declarations; the derived getter/setter walks; the defaults and
validate result; whether the unseed succeeded with the exact oracle numbers; the
answers from Step 1; and the declared gaps (no storage I/O, the deferred module
functions, `0x1AE20`/`0x2EA78`, the credit countdown and `0x11F28`).

- [ ] **Step 5: Full ladder**

Run: `rm -rf build && make verify`
Expected: exit 0, 0 warnings, `all checks passed`, the title oracle at 0 unexplained
(or the documented seed residual), `smk_compare` 120/120 + 41/41, the capture
comparison line unchanged (this cycle does not touch audio), `symbols.h`
byte-identical.

- [ ] **Step 6: Commit**

```bash
git add port/src/game/flow.c port/spec/game_flow.md README.md docs/superpowers/plans/2026-09-19-eeprom-config-report.md
git commit -m "config: wire the config consumers and record the outcome"
```

---

## Self-Review

**Spec coverage:** §1 goal → Tasks 1-4. §2 decisions → Task 1 (module owner), Task 4 (consumers), Tasks 1-3 (core-only scope), Task 4 (falsifiable proof). §3 evidence → the exact code in Tasks 1-2 and Step 1 derivations in Tasks 3-4. §4 components → Tasks 1-3. §5 consumers → Task 4. §6 DoD → Tasks 1-2 (round-trip, guards, flags), Task 3 (defaults/magic), Task 4 (consumers, oracle, ladder). §7 gaps → Task 4 Step 4. §8 risks → Task 3 Step 1 and Task 4 Step 3.

**Placeholder scan:** no TBD. Tasks 1 and 2 carry their complete code. Task 3's
`config_validate` body is explicitly marked as requiring Step 1's derivation to
confirm or correct it, with instructions to mirror the raw if it differs — the
decompile is known-unreliable for that function, so the plan does not pretend
certainty.

**Type consistency:** `config_field_get(u32) -> u32`, `config_field_set(u32,u32) -> u32`, `config_menu_default_bits(u32) -> u32`, `config_set_defaults(void)`, `config_validate(void)` are consistent across the header excerpts, the implementations, the tests and Task 4's consumers. `config_storage_touch` is private to `config.c`.
