# Sprite Compositor (sub-project 4a-i) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Port Primal Rage's sprite compositor — the display node, the span blitter and its renderers, and the `render_list` projection/clip driver — so the engine can composite a display list into the back buffer.

**Architecture:** Three units. `platform/gra` gains a handle-addressed sprite-descriptor opener so the 12-byte record layout stays in one place. A new `platform/sprite.{c,h}` owns the display node, the blitter and the renderers as a pure raster unit that consumes a node. A new `platform/render.{c,h}` owns the display list (node pool, insert, remove, sort) and the projection/clip driver. `game/flow.c` calls `render_list_sort()` then `render_list()` where the original's master loop does, so the wiring is live before 4a-ii populates the list.

**Tech Stack:** C99, the repo's existing `mem[]` model, `platform/res` handle resolution, the SIGIL-free `port/tests` harness (`test.h` + `run_tests.c`), Python 3 + `tools/gra_render.py` as the independent reference.

## Global Constraints

These apply to every task. They are the spec's project-wide rules, copied verbatim.

- **Never write `mem[0xA0000]`.** It is data-object offset `0x20000` (live pointer tables), not the VGA aperture. The port composites into `mem + DSD(DS_000E87A4)` and presents through `gfx_present`.
- **No SDL outside `port/src/host.c` and `port/src/main.c`.**
- **`data/` is read-only.** Never write a file under `data/`.
- **Comments only** `/* PORT: ... */` (a deviation from the original) and `/* TODO(verify): ... */` (an unproven doubt). No other comment conventions.
- **No new dependencies.** C99 + the existing tree only.
- **Original engine state lives in `mem[]`**; pointer-valued globals store `mem[]` offsets, consumed as `mem + DSD(...)`.
- **Comparisons have no tolerance.** A pixel mismatch is a failure.
- **Build must produce zero warnings**, and `make verify` must stay green, at the end of every task.
- **Test harness:** `port/tests/test.h` + `run_tests.c`. Conventions, verified:
  exactly one `int test_X(void)` entry point per test file, declared in
  `test.h`, called from `run_tests.c`; the only assertion macros are `CHECK(cond,
  msg)` and `CHECK_EQ_INT(a, b)`; and `port/CMakeLists.txt` lists test sources
  and core sources **explicitly** (no globbing), so a new file must be added to
  both `add_library(prage_core …)` (line 13) and `add_executable(run_tests …)`
  (line 39). There is no `Makefile` edit anywhere in this plan.
- **Commit style:** `<area>: <what changed>` in the imperative, lowercase area (e.g. `sprite: RLE span renderer`).
- **Build/test invocations:** `make`, `make test` (needs `PR_GAME_DIR=data/game/C`), `make verify`.

---

## Format reference (derived from `PRAGE.EXE`; verified by disassembly)

Every fact below was established by instruction-level disassembly and cross-checked
against the fixup-applied image. Addresses are DOS/4GW virtual: code object base
`0x10000`, data object base `0x80000`.

### Dispatch table `PTR_LAB_00080C8C` — 32 dwords, indexed by `node->type & 0x7F`

```
[ 0]=0x51E58   1=0x5D218   2=0x58CBD   3=0x51E58
[ 4]=0x5215C   5=0x51E58   6=0x5215C   7=0x51E58
[ 8]=0x51E58   9=0x57F80  10=0x51E58  11..16=0x51E58
[17]=0x5D28F  18=0x58CBD  19=0x51E58  20=0x5215C  21=0x51E58
[22]=0x5215C  23..24=0x51E58  [25]=0x57FFB  26..31=0x51E58
```

`0x51E58` is a bare `ret` one byte before the real blitter entry `0x51E5C`; a
slot holding it means "no renderer for this type". Live indices: 1, 2, 4, 6, 9,
17, 18, 20, 22, 25.

Type bits: `0x01` RLE base, `0x02` raw base, `0x04` mode-1, `0x08` hflip,
`0x10` clipped. `type & 0x1F` is the meaningful part. **`RAW+HFLIP` (type 10) is
a no-op in the original and must stay one** (hflip is valid only on RLE).

### Sprite descriptor — 12 bytes, addressed by a handle in `DS_000A8B30`

```
struct GraSprite {
    i16 width, height;   /* @0, @2 */
    i16 xorg, yorg;      /* @4 -> X pivot, @6 -> Y pivot (mechanically verified) */
    u32 pixel_handle;    /* @8 */
};
```

`DS_000A8B30[id & 0x7FFF]` yields the descriptor handle. `id & 0x8000` is the
hflip bit. 18,443 consecutive entries exist; the table is static in the data
object and already resident in `mem[]` at `DS_000A8B30`. Both the descriptor
handle and `pixel_handle` resolve through the existing `res_resolve`.

### Display node — 0x40 bytes

| off | width | meaning |
|---|---|---|
| `+0x00` | i32 | screen x (set last, by `render_list`) |
| `+0x04` | i32 | screen y (set last) |
| `+0x08` | i32 | x pivot, hflip-adjusted |
| `+0x0C` | i32 | y pivot |
| `+0x10` | u32 | type bits |
| `+0x14` | i32 | rows; the blitter subtracts `+0x34` for the call and restores |
| `+0x18` | i32 | width |
| `+0x1C` | u32 | palette pointer; bank is `((u8*)pal_ptr)[8] & 0xFF` |
| `+0x20` | u32 | pixel handle |
| `+0x24` | u32 | descriptor handle |
| `+0x28` | i32 | left overhang |
| `+0x2C` | i32 | right overhang |
| `+0x30` | i32 | top overhang (blitter saves/restores) |
| `+0x34` | i32 | bottom overhang (blitter consumes) |
| `+0x38` | i32 | scratch: dest stride |
| `+0x3C` | i32 | scratch: row index (mode-1 only) |

### Control bytes (all four RLE renderers)

| byte | meaning |
|---|---|
| `0x00..0x7F` | literal run of `n = byte` source bytes |
| `0x80..0xBF` | fill run of `n = byte & 0x3F` pixels, colour = `DAT_00081314[src[0]] + bank` |
| `0xC0..0xFF` | transparent run of `n = byte & 0x3F` pixels; **source is not advanced** |

### Colour and bank tables (verified for every entry)

* `DAT_00081314[n] == n * 0x01010101` for `n = 0..255`.
* `DAT_00081310[n] == (n-1) * 0x01010101` for `n = 1..255`.
* `DAT_00081310[0] == 0x0005D110` — a stale code pointer. The original uses it
  as an offset. Do **not** silently substitute 0; pin it (see Task 3).

Because both are replicated bytes and the run routines store a dword at a time,
the net effect is `dst_index = src_index + (bank - 1)`, exact while no byte
overflows. **The port applies the offset byte-wise**, which is correct
regardless of overflow and is observationally identical for the shipped assets.

### `0x14268` — node build (inputs: `eax` = node, `dx` = sprite id)

```
if (id == 0) { rows = width = xorg = yorg = 0; return; }
handle     = DSD(DS_000A8B30 + (id & 0x7FFF) * 4)
node.desc  = handle
hdr        = res_resolve(handle)
if ((i16)hdr.height < 0) { rows = -(i16)hdr.height; width = -(i16)hdr.width; type = 2; }
else                     { rows =  (i16)hdr.height; width =  (i16)hdr.width; type = 1; }
node.pixel = hdr.pixel_handle
node.xorg  = (i16)hdr.xorg
node.yorg  = (i16)hdr.yorg
if (id & 0x8000) { type |= 8; xorg = width - xorg - 1; }
```

x/y and the clip fields are **not** set here.

### `0x51E5C` — blitter (input: `eax` = node)

```
if (node.width == 0 || node.rows == 0) return;
saved_rows = node.rows; saved_top = node.clip_t;
src   = res_resolve(node.pixel);
bank  = bank_table[ ((u8*)node.pal_ptr)[8] & 0xFF ]
dst   = mem + DSD(DS_000E87A4) + DSD(DS_001088F8 + node.y * 4) + node.x
rows  = node.rows - node.clip_b        /* stored into node.rows for the call */
dispatch(node.type & 0x7F)
node.clip_t = saved_top; node.rows = saved_rows;
```

`DS_001088F8[y] == y * 0x140`, already built by the port's `surface_setup`.

### `0x14328` — `render_list` (inputs: `eax` = `&DS_00105B44`, `edx` = camera, `ebx` = clip rect = camera + 8)

```
last_mode1_y = -1
for (node = DSD(DS_00105B44); node != 0; node = DSD(node)) {
    pset = DSD(node + 4)
    sprite_node_build(&n, DSW(pset + 0x00))
    n.pal_ptr = DSD(pset + 0x18)
    layer = DSW(pset + 0x0E)
    if (layer > 2) { px = DSD(pset+4) >> 6; py = DSD(pset+8) >> 6; }
    else           { px = DSD(pset+4);      py = DSD(pset+8); }
    x = proj_x(px) - DSD(camera + 0) - n.xorg
    y = proj_y(py) - DSD(camera + 4) - n.yorg
    if (layer == 1) {
        x -= proj_x((s16)DSW(DS_00107A3E));
        y  = proj_y((s16)DSW(DS_00107A4E) + (DSD(DS_000F0AEC) >> 6)) - DSD(camera + 4);
        last_mode1_y = y; n.type |= 4;
    }
    if (layer == 2) {
        x -= proj_x((s16)DSW(DS_00107A3A));
        if (last_mode1_y == -1) y -= proj_y((s16)DSW(DS_00107A38));
        else                    y  = last_mode1_y - n.rows;
    }
    W = n.width; H = n.rows;
    if (W + x < 0 || H + y < 0) continue;
    n.clip_b = max((H + y) - DSD(clip + 0x0C), 0);
    n.clip_r = max((W + x) - DSD(clip + 0x08), 0);
    t = DSD(clip + 0x04) - y; n.clip_t = max(t, 0); if (t >= 0) y = DSD(clip + 0x04);
    l = DSD(clip + 0x00) - x; n.clip_l = max(l, 0); if (l >= 0) x = DSD(clip + 0x00);
    sumX = n.clip_l + n.clip_r; sumY = n.clip_t + n.clip_b;
    if (sumX + sumY == 0)                     { n.x = x; n.y = y; sprite_blit(&n); }
    else if (sumX < W && sumY < H) { n.type |= 0x10; n.x = x; n.y = y; sprite_blit(&n); }
}
```

`proj_x(v) = round(v * 3901 / 4096)`, `proj_y(v) = round(v * 3414 / 4096)`.

**The rounding must be the original's idiom, not `(v*3901)>>12`.** The original
computes, for `p = v * 3901`:

```
eax = p + 0x800                   /* p = v * num */
edx = eax >> 31                   /* sar 31: the sign mask */
edx <<= 12                        /* 0 or 0xFFFFF000; sets CF when the mask was -1 */
sbb eax, edx                      /* eax - edx - CF */
eax >>= 12                        /* sar 12 */
```

`sbb` subtracts the borrow as well as `edx`, so for a negative sum the net effect is
`p + 0x1000 - 1 == p + 0xFFF`. **This rounds half toward +infinity, not half away
from zero** — which makes the projection asymmetric at negative exact values
(`proj_x(-4096) == -3900`, while the mathematically symmetric answer would be
`-3901`). A plain arithmetic `>>12` truncates toward negative infinity and is
**not** equivalent for negative inputs (`proj_x(-1) == 0`, but `(-3901+0x800)>>12 == -1`).
Implement it as:

```c
static int proj_scale(int v, int num)
{
    int p = v * num + 0x800;
    if (p < 0) p += 0xFFF;
    return p >> 12;
}
```

**The mode-1/mode-2 offset projections are NOT this idiom.** The original uses an
uncorrected `+0x800` then `>>12` for the `DS_00107A3E`, `DS_00107A3A` and
`DS_00107A38` offsets (`prage.c:3166`, `:3173`, `:3175`), while layer 1's `y`
(line 3167-3169) does use the corrected form. Use a separate uncorrected helper for
those three. In a compositor-only run all five of those globals are zero, so the two
forms coincide there. `TODO(verify)`: 4a-ii's pixel oracle must confirm both which
form applies to those operands **and their load width** — the decomp renders
`DAT_00107a3e`/`3a`/`38` as `(uint)` (zero-extended) while the port sign-extends with
`(s16)`; with the globals zero the two are indistinguishable.

### `0x1C3A0` / `0x1C3FC` — list insert and sort

List head `DS_00105B44`; node stride 8: `{next @+0, pset @+4}`. Node pool at
`DS_0010153C`, 580 nodes, free-list head `DS_0010275C`, reset by `0x1C350`.

`0x1C3A0` inserts `target` before the first node whose `pset->layer` (`word` at
`pset+0x0E`, unsigned) is **greater than** the target's, i.e. ascending,
stable for equal keys. `0x1C3FC` is one insertion-sort pass over the list using
`0x1C3A0`.

### Pset — stride 0x20

`+0x00 u16 sprite id (bit15 = hflip)`, `+0x02 u16 flags`, `+0x04 i32 x`,
`+0x08 i32 y`, `+0x0C u16`, `+0x0E i16 layer`, `+0x18 u32 palette pointer`.
`+0x10..+0x17` unused by the display path.

### Camera / clip struct

`{i32 x; i32 y; i32 clip_left; i32 clip_top; i32 clip_right; i32 clip_bottom}`.
The master loop passes `DS_000A87CC` with clip `{0, 0, 320, 200}`; the port
defines this struct once and uses that literal.

### `PORT` deviation to record in `sprite.c`

The original has six separate renderer entry points, and the clipped RLE ones
(`0x5D28F`, `0x57FFB`) are structured as a six-way nest of straddle branches
(line/right/both × span-crosses-edge). The port instead renders **one row at a
time by intersecting each decoded run with the row's visible window**, which is
provably equivalent and collapses the six entries to two functions
(`rle_row` handling clip and mirroring as parameters). The evidence is
Task 3/6's cross-check plus the hand-computed straddle tests. This must be
recorded as a `/* PORT: ... */` comment naming both original addresses.

---

## File structure

| File | Responsibility |
|---|---|
| `port/src/platform/gra.h` / `gra.c` | **modify** — add `GraSprite`, `gra_sprite_open`, `gra_sprite_lookup`, `gra_sprite_pixels`, `gra_decode_frame_at` (a shared-core entry point for the cross-check) |
| `port/src/platform/sprite.h` / `sprite.c` | **new** — `SpriteNode`, `sprite_node_build`, `sprite_blit`, renderers |
| `port/src/platform/render.h` / `render.c` | **new** — node pool, `render_list_init/insert/remove/sort`, `render_list` |
| `port/src/game/flow.c` | **modify** — call `render_list_sort()` + `render_list()` in `game_loop`, and `render_list_init()` in `game_init` |
| `port/tests/test.h` | **modify** — declare `test_sprite`, `test_render`, and the `test_gra` additions |
| `port/tests/test_gra.c` | **modify** — sprite-opener assertions inside the existing `test_gra()` |
| `port/tests/test_sprite.c` | **new** — `int test_sprite(void)`; node build, renderers, bank, blit |
| `port/tests/test_render.c` | **new** — `int test_render(void)`; pool, sort, projection/clip, end-to-end |
| `port/tests/run_tests.c` | **modify** — call `test_sprite()` and `test_render()` |
| `port/CMakeLists.txt` | **modify** — add `src/platform/sprite.c`, `src/platform/render.c` to `prage_core`; add `tests/test_sprite.c`, `tests/test_render.c` to `run_tests` |

---

## Task 0: Branch and baseline

**Files:** none (workflow).

- [ ] **Step 1: Create the branch**

```bash
git checkout -b sprite-compositor
```

- [ ] **Step 2: Confirm the baseline is green, and record it**

```bash
make verify 2>&1 | tail -20
```

Expected: exit 0, `all checks passed`. If it is not green, stop — do not build on
a red baseline.

---

## Task 1: Handle-addressed sprite descriptors in `gra`

**Files:**
- Modify: `port/src/platform/gra.h`, `port/src/platform/gra.c`
- Modify: `port/tests/test_gra.c`

**Interfaces:**
- Consumes: `res_resolve` (existing), `DS_000A8B30` (existing symbol), `mem[]`.
- Produces:
  ```c
  typedef struct { i16 width, height, xorg, yorg; u32 pixel_handle; } GraSprite;
  int gra_sprite_open(u32 desc_handle, GraSprite *out);      /* 1 / 0 */
  int gra_sprite_lookup(u32 sprite_id, GraSprite *out, u32 *desc_handle);
  int gra_sprite_pixels(u32 pixel_handle, const u8 **out);   /* 1 / 0 */
  ```

- [ ] **Step 1: Write the failing test**

`test_gra.c` exposes a single `int test_gra(void)`. Add a `static` helper above
it and call it from it (do not add a second entry point):

```c
static void check_gra_sprites(void)
{
    /* The sprite table itself is static in the data object and already
     * resident, but resolving its handles needs the resource INDEX. test_res
     * loads it earlier in the run_tests order; load it here too (guarded, so
     * the bump allocator is never asked for it twice) to make this test
     * independent of that ordering. Needs platform/res.h. */
    if (DSD(DS_001014F0) == 0)
        CHECK(res_load_index("data/game/C", "data/game/C/INDEX") > 0,
              "resource index loads");

    /* The first entry must resolve, and its descriptor must be self-consistent.
     * Its header values are read from the shipped asset (s16statu.gra): 30x27
     * with X pivot 15 and Y pivot 13, positive height => RLE, not the raw
     * marker. */
    GraSprite s;
    u32 dh = 0;
    CHECK_EQ_INT(gra_sprite_lookup(0, &s, &dh), 1);
    CHECK(dh != 0, "sprite 0 has a descriptor handle");
    CHECK_EQ_INT(s.width, 30);
    CHECK_EQ_INT(s.height, 27);
    CHECK_EQ_INT(s.xorg, 15);
    CHECK_EQ_INT(s.yorg, 13);
    const u8 *px = NULL;
    CHECK_EQ_INT(gra_sprite_pixels(s.pixel_handle, &px), 1);
    CHECK(px != NULL, "pixel handle resolves");

    /* A garbage handle must be rejected. */
    CHECK_EQ_INT(gra_sprite_open(0x7FFFFFFFu, &s), 0);
    CHECK_EQ_INT(gra_sprite_pixels(0x7FFFFFFFu, &px), 0);

    /* The table is the documented 18,443 entries, indices 0..18442. The port
     * masks the id to 0x7FFF exactly as 0x14268 does and applies no other
     * bound. The table's end is NOT discoverable from the data — the dwords
     * after it are nonzero but are not handles (the first all-zero dword is at
     * index 18535, and 18534 reads 0x128D) — so the length is pinned from the
     * disassembly, not inferred. */
    int n = 0;
    for (u32 i = 0; i < 18443u; i++)
        if (DSD(DS_000A8B30 + i * 4u) != 0) n++;
    CHECK_EQ_INT(n, 18443);
}
```

Then add `check_gra_sprites();` inside `int test_gra(void)`.

- [ ] **Step 2: Run and watch it fail**

Run: `PR_GAME_DIR=data/game/C make test`
Expected: fails to build — `gra_sprite_lookup` undeclared.

- [ ] **Step 3: Implement**

In `gra.c`:

```c
int gra_sprite_lookup(u32 sprite_id, GraSprite *out, u32 *desc_handle)
{
    u32 h = DSD(DS_000A8B30 + (sprite_id & 0x7FFFu) * 4u);
    if (h == 0) return 0;
    if (desc_handle) *desc_handle = h;
    return gra_sprite_open(h, out);
}

int gra_sprite_open(u32 desc_handle, GraSprite *out)
{
    const u8 *p = (const u8 *)res_resolve(desc_handle);
    if (p == NULL || out == NULL) return 0;
    memcpy(&out->width,  p + 0, 2);
    memcpy(&out->height, p + 2, 2);
    memcpy(&out->xorg,   p + 4, 2);
    memcpy(&out->yorg,   p + 6, 2);
    out->pixel_handle = (u32)p[8] | ((u32)p[9] << 8) | ((u32)p[10] << 16) |
                        ((u32)p[11] << 24);
    return 1;
}

int gra_sprite_pixels(u32 pixel_handle, const u8 **out)
{
    const u8 *p = (const u8 *)res_resolve(pixel_handle);
    if (p == NULL) return 0;
    if (out) *out = p;
    return 1;
}
```

Note the explicit little-endian byte assembly for `pixel_handle`: the original's
data is little-endian, but the `u32` read must not assume an aligned or
endianness-correct load. `width`/`height`/`xorg`/`yorg` are `i16` and use
`memcpy` for the same reason.

Add the three declarations and `GraSprite` to `gra.h` with a doc comment naming
`DS_000A8B30` and `0x14268`/`0x1B544`.

- [ ] **Step 4: Run the tests**

Run: `PR_GAME_DIR=data/game/C make test`
Expected: pass.

- [ ] **Step 5: Negative control**

Temporarily change the mask from `0x7FFFu` to `0x0000u` (i.e. `sprite_id & 0`) and
confirm `gra_sprite_lookup(0x2C11, …)` now reads entry 0 — a different
descriptor than the correct `0x2C11 & 0x7FFF` uses. Revert. (This is the control
that isolates the mask; an id *above* the table, such as `0x7FFF`, does not test
the mask, because `0x7FFF & 0x7FFF == 0x7FFF`.)

- [ ] **Step 6: `make verify` then commit**

```bash
make verify && git add -A && git commit -m "gra: handle-addressed sprite descriptors"
```

---

## Task 2: `SpriteNode` and `sprite_node_build`

**Files:**
- Create: `port/src/platform/sprite.h`, `port/src/platform/sprite.c`
- Create: `port/tests/test_sprite.c`
- Modify: `port/tests/run_tests.c`, `port/tests/test.h`, `port/CMakeLists.txt`

**Interfaces:**
- Consumes: `GraSprite`, `gra_sprite_lookup` (Task 1), `mem[]`.
- Produces:
  ```c
  typedef struct {
      int x, y, xorg, yorg;
      u32 type;
      int rows, width;
      u32 pal_ptr;
      u32 pixel_handle;
      int clip_l, clip_r, clip_t, clip_b;
      int stride, row;
  } SpriteNode;
  void sprite_node_build(SpriteNode *n, u32 sprite_id);
  ```

- [ ] **Step 1: Write the failing test**

Create `port/tests/test_sprite.c` with the standard entry point, and add it to
`port/tests/test.h` (as `int test_sprite(void);`), `run_tests.c`, and both
CMake source lists.

```c
#include "test.h"
#include "platform/sprite.h"
#include "platform/gra.h"
#include "mem.h"
#include "symbols.h"
#include <string.h>

static void check_node_build(void)
{
    SpriteNode n;
    memset(&n, 0xAA, sizeof n);          /* poison, to catch unwritten fields */

    sprite_node_build(&n, 0);
    CHECK_EQ_INT(n.rows, 0);
    CHECK_EQ_INT(n.width, 0);
    CHECK_EQ_INT(n.xorg, 0);
    CHECK_EQ_INT(n.yorg, 0);

    /* A real RLE sprite, with its header values read from the shipped assets
     * (s16rad.gra): width 15, height 107, X pivot 8, Y pivot 53. The pivots are
     * NOT centred, which is what makes the hflip assertion below non-vacuous. */
    SpriteNode a, b;
    sprite_node_build(&a, 0x0001u);
    CHECK_EQ_INT(a.type & 0x01, 1);
    CHECK_EQ_INT(a.type & 0x02, 0);
    CHECK_EQ_INT(a.type & 0x08, 0);
    CHECK_EQ_INT(a.width, 15);
    CHECK_EQ_INT(a.rows, 107);
    CHECK_EQ_INT(a.xorg, 8);
    CHECK_EQ_INT(a.yorg, 53);
    const u8 *px = NULL;
    CHECK_EQ_INT(gra_sprite_pixels(a.pixel_handle, &px), 1);

    /* The same id with the hflip bit must set type bit 3 and mirror the X
     * pivot as width - xorg - 1 == 15 - 8 - 1 == 6, changing nothing else. */
    sprite_node_build(&b, 0x0001u | 0x8000u);
    CHECK_EQ_INT(b.type & 0x08, 8);
    CHECK_EQ_INT(b.xorg, 6);
    CHECK_EQ_INT(b.width, 15);
    CHECK_EQ_INT(b.rows, 107);
    CHECK_EQ_INT(b.yorg, 53);

    /* A negative-height header selects the raw base and negates both
     * dimensions. The first such entry is id 0x2BDF (s16caves.gra), whose
     * header is (-975, -53), so rows/width are 975/53 and type base is 2. */
    SpriteNode r;
    sprite_node_build(&r, 0x2BDFu);
    CHECK_EQ_INT(r.type & 0x02, 2);
    CHECK_EQ_INT(r.type & 0x01, 0);
    CHECK_EQ_INT(r.rows, 975);
    CHECK_EQ_INT(r.width, 53);
}

int test_sprite(void)
{
    check_node_build();
    return 0;
}
```

- [ ] **Step 2: Run and watch it fail**

Run: `PR_GAME_DIR=data/game/C make test`
Expected: build failure — `platform/sprite.h` not found.

- [ ] **Step 3: Implement**

`sprite.h` declares `SpriteNode` and the two functions with doc comments naming
`0x14268` and `0x51E5C`. `sprite.c`:

```c
void sprite_node_build(SpriteNode *n, u32 sprite_id)
{
    if (n == NULL) return;
    if (sprite_id == 0) {
        n->rows = n->width = n->xorg = n->yorg = 0;
        return;
    }
    GraSprite g;
    u32 dh = 0;
    if (!gra_sprite_lookup(sprite_id, &g, &dh)) {
        n->rows = n->width = n->xorg = n->yorg = 0;
        return;
    }
    n->pal_ptr = 0;
    n->clip_l = n->clip_r = n->clip_t = n->clip_b = 0;
    n->stride = n->row = 0;
    if (g.height < 0) {
        n->rows  = -(int)g.height;
        n->width = -(int)g.width;
        n->type  = 2u;
    } else {
        n->rows  = (int)g.height;
        n->width = (int)g.width;
        n->type  = 1u;
    }
    /* PORT: 0x14268 leaves +0x00/+0x04/+0x1C unset; render_list and the caller
     * fill them. The hflip id bit is masked off before the table lookup. */
    n->pixel_handle = g.pixel_handle;
    n->xorg = g.xorg;
    n->yorg = g.yorg;
    if (sprite_id & 0x8000u) {
        n->type |= 8u;
        n->xorg = n->width - n->xorg - 1;
    }
}
```

- [ ] **Step 4: Run the tests**

Run: `PR_GAME_DIR=data/game/C make test`
Expected: pass.

- [ ] **Step 5: Negative control**

Swap the hflip pivot to `n->xorg = n->xorg` (drop the mirror) and confirm the
hflip assertion fails. Revert.

- [ ] **Step 6: `make verify` then commit**

```bash
make verify && git add -A && git commit -m "sprite: display node and node builder (0x14268)"
```

---

## Task 3: Span primitives, bank arithmetic, and the RLE renderer `0x5D218`

**Files:**
- Modify: `port/src/platform/sprite.c`
- Modify: `port/src/platform/gra.c`, `port/src/platform/gra.h` (add the shared-core `gra_decode_frame_at`)
- Modify: `port/tests/test_sprite.c`

**Interfaces:**
- Consumes: `res_resolve`, `DS_000E87A4`, `DS_001088F8`, `SpriteNode`.
- Produces (internal, `static`):
  ```c
  static u32 sprite_bank(u32 pal_ptr);
  static void copy_run(u8 *dst, const u8 *src, int n, u8 bank);
  static void fill_run(u8 *dst, int n, u8 colour);
  static const u8 *rle_row(u8 *dst, const u8 *src, int width,
                           u8 bank, int mirror);   /* mirror==0 here */
  ```
  and the public seams used elsewhere:
  ```c
  u8  sprite_bank_offset(u8 bank_byte);              /* sprite.h; Task 4 tests it */
  int sprite_render_rle(const u8 *src, u8 *dst, int width, int rows,
                        int stride, u8 bank);
  ```

- [ ] **Step 1: Write the failing cross-check test**

Add another `static` helper to `test_sprite.c` and call it from `test_sprite()`:

```c
/* The port now has THREE independent decoders of the same RLE: the new span
 * renderer, gra_decode_frame (sub-project 1, verified against the Python
 * oracle), and tools/gra_render.py. They must agree byte-for-byte on real
 * assets. This is the compositor's primary oracle and needs no emulator. */
static void check_rle_cross(void)
{
    static u8 expect[320 * 200];
    static u8 got[320 * 200];
    int checked = 0, literals = 0, fills = 0, transparents = 0;

    for (u32 id = 0; id < 0x7FFFu && checked < 32; id++) {
        GraSprite g; u32 dh = 0;
        if (!gra_sprite_lookup(id, &g, &dh)) break;
        if (g.height <= 0 || g.width <= 0) continue;        /* RLE base only */
        if ((int)g.width > 320 || (int)g.height > 200) continue;

        const u8 *px = NULL;
        if (!gra_sprite_pixels(g.pixel_handle, &px)) continue;

        memset(expect, 0, sizeof expect);
        int n = gra_decode_frame_at(px, g.width, g.height, expect, sizeof expect);
        if (n < 0) continue;

        memset(got, 0, sizeof got);
        u8 bank = 1;                       /* offset 0: identity */
        CHECK_EQ_INT(sprite_render_rle(px, got, g.width, g.height, 320, bank), 0);
        CHECK(memcmp(expect, got, 320 * 200) == 0, "span renderer == gra decoder");

        /* Count control-byte classes so the sample cannot pass vacuously. */
        for (const u8 *p = px; p < px + n; ) {
            u8 b = *p++;
            if (b < 0x80) { literals++; p += b; }
            else if (b < 0xC0) { fills++; p += 1; }
            else transparents++;
        }
        checked++;
    }
    CHECK(checked >= 8, "cross-checked at least 8 sprites");
    CHECK(literals > 0 && fills > 0, "sample covers literal and fill runs");
}
```

`gra_decode_frame_at` is a new entry point in `gra.{c,h}`: an internal
`decode_frame_core(const u8 *blob, int w, int h, u8 *dst, u32 cap)` used by both
`gra_decode_frame` (which resolves the blob from its chunk/frame address and then
calls it) and `gra_decode_frame_at` (which takes the blob directly). It must not
duplicate the decode. This is what lets the cross-check point at an arbitrary
handle, which `gra_decode_frame`'s frame-index signature cannot.

**Why this reaches the Python oracle transitively.** Sub-project 1 proved
`gra_decode_frame` byte-identical to `tools/gra_render.py`; this task proves the
new span renderer byte-identical to `gra_decode_frame`. The three-way agreement
therefore holds by transitivity, and no direct Python invocation is required
here. If it is cheap, add one: `PR_GRA_DUMP=<dir>` writing a sampled sprite's
`gra_render.py` output to a file and diffing it in the test is a bonus, not a
requirement.

- [ ] **Step 2: Run and watch it fail**

Run: `PR_GAME_DIR=data/game/C make test`
Expected: build failure — `sprite_render_rle` undeclared.

- [ ] **Step 3: Implement**

```c
/* DAT_00081310[n] == (n-1) replicated for n in 1..255, so the byte-level effect
 * of the original's dword add is `+ (b-1)` per pixel. Exact, because the dword
 * is four identical bytes.
 *
 * [0] is NOT replicated: it holds the stale code pointer 0x0005D110, whose
 * bytes differ, so the original's dword add would give each pixel inside a
 * 4-pixel group a different offset — position-dependent garbage, and a latent
 * defect in the original, not a palette operation. A byte-wise port cannot
 * reproduce an alignment-dependent dword carry, so bank 0 is treated as no
 * offset. TODO(verify): whether any shipped asset ever supplies bank 0; if the
 * 4a-ii pixel oracle diverges in 4-pixel groups, this is the first suspect. */
static u32 sprite_bank(u32 pal_ptr)
{
    const u8 *p = (const u8 *)res_resolve(pal_ptr);
    return sprite_bank_offset((p != NULL) ? p[8] : 0u);
}
```

`sprite_bank_offset` takes the **bank byte** (the value at `pal_ptr[8]`), not the
palette pointer, and it is declared in `sprite.h` so Task 4 can test the mapping
directly:

```c
/* The bank byte's effect on the source index. DAT_00081310[b] == (b-1)
 * replicated for b in 1..255, so this is a plain -1; [0] is the stale code
 * pointer 0x0005D110 and gets no offset (see the note above). */
u8 sprite_bank_offset(u8 bank_byte)
{
    return (bank_byte == 0u) ? 0u : (u8)(bank_byte - 1u);
}
```

Declare `sprite_bank_offset` in `sprite.h` (Task 4's test calls it);
`sprite_bank` stays internal to `sprite.c`.

```c
static void copy_run(u8 *dst, const u8 *src, int n, u8 bank)
{
    for (int i = 0; i < n; i++) dst[i] = (u8)(src[i] + bank);
}
static void fill_run(u8 *dst, int n, u8 colour)
{
    for (int i = 0; i < n; i++) dst[i] = colour;
}

/* Decodes one row of `width` pixels. Returns the advanced source. Runs that
 * extend past the row are clipped to it, matching the original's raster pass. */
static const u8 *rle_row(u8 *dst, const u8 *src, int width, u8 bank, int mirror)
{
    int col = 0;
    while (col < width) {
        u8 b = *src++;
        int n;
        if (b < 0x80) {
            n = b;
            if (n > width - col) n = width - col;
            if (mirror) {
                for (int i = 0; i < n; i++)
                    dst[width - 1 - (col + i)] = (u8)(src[i] + bank);
            } else {
                copy_run(dst + col, src, n, bank);
            }
            src += b;              /* the run length, not the clipped count */
        } else if (b < 0xC0) {
            n = b & 0x3F;
            u8 colour = (u8)(DAT_81314[*src] + bank);
            src++;
            if (n > width - col) n = width - col;
            if (mirror)
                for (int i = 0; i < n; i++) dst[width - 1 - (col + i)] = colour;
            else
                fill_run(dst + col, n, colour);
        } else {
            n = b & 0x3F;
            if (n > width - col) n = width - col;
            /* transparent: destination untouched, source not advanced */
        }
        col += n;
    }
    return src;
}
```

with `DAT_81314` spelled `DS_00081314` and read as `DSB(DS_00081314 + idx)` to
match the repo's accessor convention.

```c
int sprite_render_rle(const u8 *src, u8 *dst, int width, int rows,
                      int stride, u8 bank)
{
    if (src == NULL || dst == NULL || width <= 0 || rows <= 0) return -1;
    for (int r = 0; r < rows; r++) {
        src = rle_row(dst, src, width, bank, 0);
        dst += stride;
    }
    return 0;
}
```

Note `stride` is the *destination* stride (`0x140` for a full-width row);
`sprite_blit` passes `0x140` and offsets `dst` by `node.x`, which reproduces
`0x5D218`'s `0x140 - width` row advance.

- [ ] **Step 4: Run the tests**

Run: `PR_GAME_DIR=data/game/C make test`
Expected: `all checks passed`, with the cross-check reporting at least 8 sprites
and non-zero literal and fill counts.

- [ ] **Step 5: Negative control**

Change `copy_run` to ignore `bank` and confirm the cross-check still passes at
`bank == 1` (offset 0) — proving the test is sensitive to the *pixels* and that
the bank check needs its own test. Then set `bank = 2` in the test and confirm
it fails. Revert both.

- [ ] **Step 6: `make verify` then commit**

```bash
make verify && git add -A && git commit -m "sprite: RLE span renderer, cross-checked against the gra decoder"
```

---

## Task 4: Bank and colour tables, pinned

**Files:**
- Modify: `port/src/platform/sprite.c`, `port/src/platform/sprite.h`
- Modify: `port/tests/test_sprite.c`

**Interfaces:**
- Produces: `u32 sprite_bank(u32 pal_ptr);` — resolves the palette pointer and
  returns the source-index offset (`0` for bank byte 0, else `byte - 1`). Task 9's
  blitter calls it; the test calls it directly, which is also why it is not
  `static` (an unused `static` would break the zero-warnings rule).
- Produces: the pinned table facts as regression assertions.

`DS_00081310` and `DS_00081314` are **not** in the generated `symbols.h` (the
region is one Ghidra never decompiled), so the test spells both addresses as
literals. Both are inside the loaded data object (`DATA_BASE 0x80000`, data size
`0x8B0D0`), i.e. real `mem[]` bytes.

- [ ] **Step 1: Write the failing test**

Add another `static` helper to `test_sprite.c` and call it from `test_sprite()`:

```c
/* The two generated tables live in a region Ghidra never decompiled, so
 * gen_symbols.py emits no DS_ symbols for them; the addresses are literals and
 * are inside the loaded data object. */
#define BANK_TABLE   0x00081310u
#define COLOUR_TABLE 0x00081314u

static void check_bank_and_colour(void)
{
    /* BANK_TABLE[n] == (n-1) replicated, for every n in 1..255. These are
     * static generated table facts, so they are pinned exactly. */
    for (u32 n = 1; n < 256; n++)
        CHECK(DSD(BANK_TABLE + n * 4u) == (n - 1u) * 0x01010101u,
              "bank table entry is (n-1) replicated");
    /* [0] is the stale code pointer, deliberately not replicated. */
    CHECK(DSD(BANK_TABLE) == 0x0005D110u, "bank[0] is the stale pointer");

    /* COLOUR_TABLE[n] == n replicated, for every n. */
    for (u32 n = 0; n < 256; n++)
        CHECK(DSD(COLOUR_TABLE + n * 4u) == n * 0x01010101u,
              "colour table entry is n replicated");

    /* The bank *byte* mapping: (b-1), except that byte 0 is no offset. */
    CHECK_EQ_INT(sprite_bank_offset(1), 0);
    CHECK_EQ_INT(sprite_bank_offset(2), 1);
    CHECK_EQ_INT(sprite_bank_offset(255), 254);
    CHECK_EQ_INT(sprite_bank_offset(0), 0);

    /* sprite_bank resolves a palette pointer and reads its byte 8. Build the
     * pointer in scratch memory: a resolvable handle is not needed for a
     * pointer that is already a mem[] offset only if the caller passes one, so
     * use a resource handle from the sprite table's own descriptor. */
    GraSprite g; u32 dh = 0;
    CHECK_EQ_INT(gra_sprite_lookup(0x2C11u, &g, &dh), 1);
    /* dh resolves to the 12-byte descriptor; byte 8 is the low byte of the
     * pixel handle, which is a non-zero arbitrary bank byte. Assert the
     * relationship rather than a magic value. */
    const u8 *desc = (const u8 *)res_resolve(dh);
    CHECK(desc != NULL, "descriptor resolves");
    u8 b = desc[8];
    CHECK_EQ_INT(sprite_bank(dh), (b == 0u) ? 0 : (int)(u8)(b - 1u));

    /* A non-zero bank must actually shift the drawn pixels. This is the path
     * the cross-check in Task 3 cannot cover: it renders at bank offset 0, so
     * the fill-colour `+ bank` add is otherwise untested. Row: literal 2, fill
     * 3 (colour index 7) -- at offset 2 every drawn byte is +2. */
    static const u8 row[9] = { 0x02, 0x0A, 0x0B, 0x83, 0x07, 0,0,0,0 };
    u8 out[8]; memset(out, 0xEE, sizeof out);
    CHECK_EQ_INT(sprite_render_rle(row, out, 5, 1, 8, 2), 0);
    CHECK_EQ_INT(out[0], 0x0C);   /* 0x0A + 2 */
    CHECK_EQ_INT(out[1], 0x0D);   /* 0x0B + 2 */
    CHECK_EQ_INT(out[2], 0x09);   /* colour index 7, zero-offset byte 7, + 2 */
    CHECK_EQ_INT(out[3], 0x09);
    CHECK_EQ_INT(out[4], 0x09);
    /* No overrun into the row padding. */
    CHECK_EQ_INT(out[5], 0xEE);
}
```

- [ ] **Step 2: Run and watch it fail**

Run: `PR_GAME_DIR=data/game/C make test`. Expected: build failure —
`sprite_bank` undeclared.

- [ ] **Step 3: Implement**

```c
u32 sprite_bank(u32 pal_ptr)
{
    const u8 *p = (const u8 *)res_resolve(pal_ptr);
    return sprite_bank_offset((p != NULL) ? p[8] : 0u);
}
```

Declare it in `sprite.h` next to `sprite_bank_offset`, with a doc comment naming
`0x51E5C` (its only original caller). Nothing else changes: the fill colour in
`rle_row` already computes `COLOUR_TABLE[idx] + bank` byte-wise and Task 3 landed
that. Drop the now-unused `#include "../symbols.h"` from `sprite.c` if nothing in
it references a `DS_*` macro (Task 3's reviewer flagged it as unused).

- [ ] **Step 4: Run the tests** — expect pass.
- [ ] **Step 5: Negative control** — assert `BANK_TABLE[0]` equals 0 instead of
  `0x5D110` and confirm the test fails; then change the render assertion to
  `bank = 0` and confirm the four `+2` assertions fail. Revert both.
- [ ] **Step 6: `make verify` then commit**

```bash
make verify && git add port/src/platform/sprite.c port/src/platform/sprite.h port/tests/test_sprite.c && git commit -m "sprite: pin the bank and colour tables"
```

(This plan is being executed while another session commits to the same branch:
stage explicit paths, never `git add -A`.)

---

## Task 5: Raw copy renderer `0x58CBD`

**Files:**
- Modify: `port/src/platform/sprite.c`
- Modify: `port/tests/test_sprite.c`

**Interfaces:**
- Produces: `int sprite_render_raw(const u8 *src, u8 *dst, int width, int rows,
  int stride, u8 bank);`

- [ ] **Step 1: Write the failing test**

```c
static void check_raw_copy(void)
{
    /* A 4x2 raw fixture with the bank offset applied byte-wise. */
    const u8 src[8] = { 1,2,3,4, 5,6,7,8 };
    u8 dst[16]; memset(dst, 0xEE, sizeof dst);
    CHECK_EQ_INT(sprite_render_raw(src, dst, 4, 2, 16, 3), 0);
    for (int i = 0; i < 4; i++) CHECK_EQ_INT(dst[i], src[i] + 3);
    for (int i = 0; i < 4; i++) CHECK_EQ_INT(dst[16 + i], src[4 + i] + 3);
    /* The row gap is untouched. */
    for (int i = 4; i < 16; i++) CHECK_EQ_INT(dst[i], 0xEE);

    /* Overflow wraps byte-wise, not into the next pixel. */
    const u8 hi[2] = { 0xFE, 0xFF };
    u8 d2[2] = { 0, 0 };
    CHECK_EQ_INT(sprite_render_raw(hi, d2, 2, 1, 2, 4), 0);
    CHECK_EQ_INT(d2[0], 0x02);
    CHECK_EQ_INT(d2[1], 0x03);
}
```

- [ ] **Step 2: Run and watch it fail**
- [ ] **Step 3: Implement** — bulk copy `n = width` bytes per row adding the
  bank byte-wise; `dst += stride` per row; no source advancement beyond
  `width * rows`.
- [ ] **Step 4: Run the tests** — expect pass.
- [ ] **Step 5: Negative control** — apply the bank as a dword add and confirm
  the overflow assertion fails. Revert.
- [ ] **Step 6: `make verify` then commit**

```bash
make verify && git add -A && git commit -m "sprite: raw copy renderer (0x58CBD)"
```

---

## Task 6: Clipped RLE — the `0x5D28F` semantics

**Files:**
- Modify: `port/src/platform/sprite.c`
- Modify: `port/tests/test_sprite.c`

**Interfaces:**
- Produces:
  ```c
  /* `rows` has already had clip_b subtracted by the blitter, so none of these
   * take a clip_b. clip_t is the number of whole rows to skip. */
  int sprite_render_rle_clipped(const u8 *src, u8 *dst, int width, int rows,
                                int stride, u8 bank,
                                int clip_l, int clip_r, int clip_t, int mirror);
  ```

- [ ] **Step 1: Write the failing tests**

Add a `static void check_rle_clipped(void)` helper and call it from
`test_sprite()`. Hand-computed, on a tiny fixture, one assertion per branch. Use
a 6-wide, 3-row sprite whose row is `[literal 2][transparent 1][fill 3]`:

```c
/* row bytes: 02 a b C1 cc 83 c c c  -> cols 0,1 = a,b; col 2 transparent;
 * cols 3,4,5 = fill colour cc */
static const u8 ROW[9] = { 0x02, 0x0A, 0x0B, 0xC1, 0x00, 0x83, 0x07,0,0 };
```

The blob is `ROW` repeated per row (the helper builds a 3-row stream), width 6,
`stride = 16`, `bank = 0`. (`bank` is the **offset** argument; 0 is identity, so
drawn bytes are the source bytes unchanged. A literal `1` would add 1 to every
drawn pixel — the same trap Task 3 hit.)

**Destination indexing is window-relative, and that is load-bearing.** The
blitter computes `dst = mem + … + node.x`, and `render_list` has already clamped
`node.x` inward to the clip edge (`if (l >= 0) x = clip_left`). So `dst[0]` is the
window's *first visible* column and the renderer writes the visible window
sequentially from `dst[0]`; it must **not** index the destination by the original
column. (The original reaches the same result by skipping the clipped-off part of
the first straddling run before its first store.)

Expected rows, `X` = untouched, six columns shown (`dst[0..5]`):

| case | clip | expected row | why |
|---|---|---|---|
| none | L=0 R=0 T=0 | `0A 0B X 07 07 07` | baseline |
| left, mid-literal | L=1 R=0 | `0B X 07 07 07 X` | window is cols 1..5; the literal run is split and its second payload byte is the first drawn pixel |
| left, mid-transparent | L=2 R=0 | `X 07 07 07 X X` | window is cols 2..5; col 2 is the transparent run |
| right only | L=0 R=2 | `0A 0B X 07 X X` | window is cols 0..3; the fill run is cut after col 3 |
| both | L=2 R=2 | `X 07 X X X X` | window is cols 2..3: transparent, then one fill pixel |
| left spans both | L=4 R=1 | `07 X X X X X` | window is col 4 alone, inside the fill run |
| top | T=1 | rows shifted up by one | one whole row of stream is consumed without drawing |
| bottom | — | nothing further | `clip_b` is subtracted by the blitter, not here; the caller passes the reduced `rows` |

Plus: assert the source pointer is left at the same position for the L/R cases as
for the unclipped case — clipping must still consume the whole row's stream.

- [ ] **Step 2: Run and watch it fail**

- [ ] **Step 3: Implement**

The equivalent formulation, recorded with a `/* PORT: ... */` comment naming
`0x5D28F` and `0x57FFB`:

```
vis = width - clip_l - clip_r
if vis <= 0: consume every row's stream without drawing; return 0
consume clip_t whole rows without drawing
per remaining row:
    col = 0
    while col < width:
        b = *src++
        if literal:
            n = b; take = min(n, width - col)
            for i in 0..take:
                c = col + i
                if c >= clip_l and c < clip_l + vis:
                    draw at (mirror ? vis-1-(c-clip_l) : c-clip_l)
            src += n; col += n
        elif fill:
            colour = ...; src++
            n = b & 0x3F; take = ...
            for i in 0..take: same visibility test, fill with colour
            col += n
        else:
            n = b & 0x3F; col += n        /* transparent: nothing drawn */
    dst += stride
```

**Do not write a second control-byte walker.** The "consume without drawing"
paths (the `vis <= 0` case and the `clip_t` rows) reuse the *same* row decoder:
extend Task 3's `rle_row` so it accepts `dst == NULL` and skips every store while
walking exactly the same control bytes, then call it for the skipped rows. A
separate `rle_skip_row` that re-implements the literal/fill/transparent walk would
be verbatim duplication of the logic block and a review finding.

A skipped row still consumes its source bytes in order — that is what keeps the
following rows in sync — it simply stores nothing.

- [ ] **Step 4: Run the tests** — expect pass on every row.
- [ ] **Step 5: Negative control** — draw the clipped part instead of skipping it
  (drop the visibility test) and confirm the left-mid-literal, right-only and
  both-sides expectations fail. Revert.
- [ ] **Step 6: `make verify` then commit**

```bash
make verify && git add port/src/platform/sprite.c port/src/platform/sprite.h port/tests/test_sprite.c && git commit -m "sprite: clipped RLE renderer (0x5D28F semantics)"
```

(This plan is being executed while another session commits to the same branch:
stage explicit paths, never `git add -A`.)

---

## Task 7: Mirrored RLE — `0x57F80` and `0x57FFB`

**Files:**
- Modify: `port/src/platform/sprite.c`
- Modify: `port/tests/test_sprite.c`

**Interfaces:**
- Produces: the mirrored path, exercised through `sprite_render_rle_clipped`'s
  `mirror` parameter — that is the type-`0x09` path (`RLE|hflip`), and the only
  mirrored entry the dispatch table has. `sprite_render_rle` is the unclipped
  *unmirrored* entry and passes `mirror = 0` internally; it takes no mirror
  argument. (The original's `0x57F80` unmirrored / `0x57FFB` mirrored pair maps
  onto the one unified `rle_row`, whose `mirror` parameter Task 6 already
  plumbed — so on this task most of the "implementation" already exists and the
  work is the tests plus the `PORT` note.)
- **TDD note for this task:** because Task 6 unified the row decoder and already
  carries `mirror`, the new mirror test will likely pass on its first run — there
  is no honest RED. Do not fake one. Write the test, observe it pass, and provide
  the sensitivity evidence with Step 5's negative control instead.

- [ ] **Step 1: Write the failing test**

```c
static void check_rle_mirror(void)
{
    const u8 src[9] = { 0x02, 0x0A, 0x0B, 0xC1, 0x00, 0x83, 0x07,0,0 };
    u8 plain[6], mir[6];
    memset(plain, 0xEE, sizeof plain); memset(mir, 0xEE, sizeof mir);
    CHECK_EQ_INT(sprite_render_rle(src, plain, 6, 1, 6, 1), 0);
    /* The clipped entry with mirror=1 and no clip must reverse the columns. */
    CHECK_EQ_INT(sprite_render_rle_clipped(src, mir, 6, 1, 6, 1,
                                          0, 0, 0, /*mirror=*/1), 0);
    for (int i = 0; i < 6; i++) CHECK_EQ_INT(mir[i], plain[5 - i]);
}
```

- [ ] **Step 2: Run and watch it fail**
- [ ] **Step 3: Implement** — the `mirror` branch in `rle_row` (already sketched in
  Task 3) writes to `vis - 1 - (col - clip_l)` within the visible window. Confirm
  `dst` still advances by `stride` (not `0x140 + width`): the port keeps the
  destination row base at `mem + …y… + x` and computes the mirrored column
  inside the row, where the original sets `dst += width - 1` and walks backward.
  Record that as a `/* PORT: ... */` note naming `0x57F80`/`0x57FFB`; the
  observable result is identical.
- [ ] **Step 4: Run the tests** — expect pass.
- [ ] **Step 5: Negative control** — use `col` instead of `vis-1-(col-clip_l)`
  and confirm the mirror assertion fails. Revert.
- [ ] **Step 6: `make verify` then commit**

```bash
make verify && git add -A && git commit -m "sprite: mirrored RLE renderer (0x57F80, 0x57FFB)"
```

---

## Task 8: Mode-1 shear copy `0x5215C`

**Files:**
- Modify: `port/src/platform/sprite.c`
- Modify: `port/tests/test_sprite.c`

**Interfaces:**
- Produces: `int sprite_render_shear(const u8 *src, u8 *dst, int width, int rows,
  int stride, u8 bank, int clip_l, int clip_r, int clip_t);`

- [ ] **Step 1: Write the failing test**

```c
static void check_shear(void)
{
    /* The shear reads `vis` bytes from `src + sh`, where `sh` can be positive
     * (up to +2 in the cases below), so the fixture must carry slack past the
     * last row: 6 columns x 4 rows = 24 bytes for a 3-row image. A 6x3 buffer
     * would read out of bounds on the +2 case. */
    u8 src[6 * 4];
    for (int i = 0; i < 24; i++) src[i] = (u8)(10 + i);
    u8 dst[6 * 3]; memset(dst, 0xEE, sizeof dst);

    /* Zero the table explicitly first: this test must not depend on whatever
     * the loaded data object happens to hold at DS_00107900. With the table
     * zero the shear is zero and this is a plain copy. */
    DSW(DS_00107900 + 0) = 0;
    DSW(DS_00107900 + 2) = 0;
    DSW(DS_00107900 + 4) = 0;
    CHECK_EQ_INT(sprite_render_shear(src, dst, 6, 3, 6, 0, 0,0,0), 0);
    for (int i = 0; i < 18; i++) CHECK_EQ_INT(dst[i], src[i]);

    /* A non-zero ramp shears row r by ((tab[r] - tab[0]) >> 5), arithmetic. */
    DSW(DS_00107900 + 0) = 0;
    DSW(DS_00107900 + 2) = 32;      /* row 1: (32-0)>>5 = 1 */
    DSW(DS_00107900 + 4) = 64;      /* row 2: (64-0)>>5 = 2 */
    memset(dst, 0xEE, sizeof dst);
    CHECK_EQ_INT(sprite_render_shear(src, dst, 6, 3, 6, 0, 0,0,0), 0);
    for (int i = 0; i < 6; i++) CHECK_EQ_INT(dst[i], src[i]);           /* row 0 */
    for (int i = 0; i < 6; i++) CHECK_EQ_INT(dst[6+i], src[6 + i + 1]); /* row 1 */
    for (int i = 0; i < 6; i++) CHECK_EQ_INT(dst[12+i], src[12 + i + 2]);/* row 2 */

    /* Negative shear truncates toward -infinity: -33 >> 5 == -2. */
    DSW(DS_00107900 + 2) = (u16)0xFFDFu;   /* -33 */
    memset(dst, 0xEE, sizeof dst);
    CHECK_EQ_INT(sprite_render_shear(src, dst, 6, 3, 6, 0, 0,0,0), 0);
    /* (i16)0xFFDF == -33, (-33 - 0) >> 5 == -2 */
    for (int i = 0; i < 6; i++) CHECK_EQ_INT(dst[6+i], src[6 + i - 2]);

    /* Reset the table so later tests are unaffected. */
    DSW(DS_00107900 + 0) = 0;
    DSW(DS_00107900 + 2) = 0;
    DSW(DS_00107900 + 4) = 0;
}
```

- [ ] **Step 2: Run and watch it fail**
- [ ] **Step 3: Implement**

```c
int sprite_render_shear(const u8 *src, u8 *dst, int width, int rows,
                        int stride, u8 bank,
                        int clip_l, int clip_r, int clip_t)
{
    int vis = width - clip_l - clip_r;
    if (vis <= 0 || rows - clip_t <= 0) return 0;
    src += clip_t * width + clip_l;
    for (int r = 0; r < rows - clip_t; r++) {
        int ref = (i16)DSW(DS_00107900);
        int sh  = ((int)(i16)DSW(DS_00107900 + (clip_t + r) * 2) - ref) >> 5;
        copy_run(dst, src + sh, vis, bank);
        src += width;               /* net advance = width, as the original */
        dst += stride;
    }
    return 0;
}
```

**The `(i16)` cast is load-bearing**: the table holds signed 16-bit values and
the shift is arithmetic. Reading it as `u16` makes negative shear wrong.

- [ ] **Step 4: Run the tests** — expect pass, including the negative-shear case.
- [ ] **Step 5: Negative control** — replace the arithmetic shift `>> 5` with a
  truncating `/ 32` and confirm the negative-shear assertion fails (`-33 / 32 == -1`,
  not `-2`). Revert. Do **not** use "drop the `(i16)` cast" as the control: that makes
  the shift amount `(65503 - 0) >> 5 == 2046`, a wild positive offset that reads far
  outside the fixture — the assertion would fail for the wrong reason and the read is
  out of bounds.
- [ ] **Step 6: `make verify` then commit**

```bash
make verify && git add port/src/platform/sprite.c port/src/platform/sprite.h port/tests/test_sprite.c && git commit -m "sprite: mode-1 shear copy renderer (0x5215C)"
```

(This plan is being executed while another session commits to the same branch:
stage explicit paths, never `git add -A`.)

---

## Task 9: `sprite_blit` — the dispatcher

**Files:**
- Modify: `port/src/platform/sprite.{h,c}`
- Modify: `port/tests/test_sprite.c`

**Interfaces:**
- Consumes: the renderers from Tasks 3–8, `res_resolve`, `DS_000E87A4`,
  `DS_001088F8`, `DS_00081310`.
- Produces:
  ```c
  void sprite_blit(SpriteNode *n);
  ```

- [ ] **Step 1: Write the failing test**

```c
static void check_blit_dispatch(void)
{
    /* A zero-size node must return without touching the buffer. Snapshot first:
     * `CHECK(1, "no crash")` would assert nothing, and a test that asserts
     * nothing is not a test. */
    u32 icon = DSD(DS_000E87A4);
    static u8 pre[320 * 200];
    memcpy(pre, mem + icon, sizeof pre);
    SpriteNode n; memset(&n, 0, sizeof n);
    sprite_blit(&n);
    CHECK(memcmp(mem + icon, pre, sizeof pre) == 0,
          "a zero-size node blits nothing");

    /* The blitter restores +0x14 and +0x30 after the call. */
    GraSprite g; u32 dh = 0;
    CHECK_EQ_INT(gra_sprite_lookup(0x2C11u, &g, &dh), 1);
    memset(&n, 0, sizeof n);
    sprite_node_build(&n, 0x2C11u);
    n.pal_ptr = dh;                 /* any resolvable pointer with a bank byte */
    n.x = 0; n.y = 0;
    n.rows = 2; n.width = 2;        /* clamp so the fixture is small */
    n.clip_t = 1; n.clip_b = 0;
    int rows_before = n.rows, top_before = n.clip_t;
    sprite_blit(&n);
    CHECK_EQ_INT(n.rows, rows_before);
    CHECK_EQ_INT(n.clip_t, top_before);

    /* RAW+HFLIP (type 10) is a no-op in the original and must stay one. */
    SpriteNode r; memset(&r, 0, sizeof r);
    r.type = 0x0Au; r.rows = 4; r.width = 4;
    r.pixel_handle = n.pixel_handle; r.pal_ptr = dh;
    u8 *back = mem + DSD(DS_000E87A4);
    u8 before = back[DSD(DS_001088F8) + 0];
    sprite_blit(&r);
    CHECK_EQ_INT(back[DSD(DS_001088F8) + 0], before);
}
```

- [ ] **Step 2: Run and watch it fail**
- [ ] **Step 3: Implement**

```c
void sprite_blit(SpriteNode *n)
{
    if (n == NULL || n->width == 0 || n->rows == 0) return;
    const u8 *src = NULL;
    if (!gra_sprite_pixels(n->pixel_handle, &src)) return;

    u8  bank = (u8)sprite_bank(n->pal_ptr);
    u8 *dst  = mem + DSD(DS_000E87A4)
                   + DSD(DS_001088F8 + (u32)n->y * 4u) + (u32)n->x;

    /* PORT: 0x51E5C stores rows - clip_b into the node for the call and
     * restores +0x14/+0x30 afterwards. The port keeps the node intact and
     * passes the reduced row count, so a node can be re-blitted and the
     * renderers never see clip_b. */
    int rows = n->rows - n->clip_b;
    int L = n->clip_l, R = n->clip_r, T = n->clip_t;
    int w = n->width;

    /* The ten live entries of PTR_LAB_00080C8C, as a switch so the mapping is
     * explicit rather than inferred from bit combinations. Everything else is
     * the original's no-op stub, including RAW+HFLIP (type 0x0A), which the
     * original leaves unimplemented. */
    switch (n->type & 0x1Fu) {
    case 0x01: sprite_render_rle(src, dst, w, rows, 320, bank); break;
    case 0x02: sprite_render_raw(src, dst, w, rows, 320, bank); break;
    case 0x04: case 0x06:
        sprite_render_shear(src, dst, w, rows, 320, bank, L, R, T); break;
    case 0x09:
        sprite_render_rle_clipped(src, dst, w, rows, 320, bank, 0, 0, 0, 1); break;
    case 0x11:
        sprite_render_rle_clipped(src, dst, w, rows, 320, bank, L, R, T, 0); break;
    case 0x12:
        sprite_render_raw(src, dst, w, rows, 320, bank); break;
    case 0x14: case 0x16:
        sprite_render_shear(src, dst, w, rows, 320, bank, L, R, T); break;
    case 0x19:
        sprite_render_rle_clipped(src, dst, w, rows, 320, bank, L, R, T, 1); break;
    default: break;                       /* stub slot: no-op */
    }
}
```

Two notes for the implementer, both correctness-relevant:

* **0x12 (RAW|CLIP) and 0x11 (RLE|CLIP) carry clip fields only when the
  composite driver set them.** `render_list` sets `+0x28/+0x2C/+0x30/+0x34` and
  sets type bit `0x10` together, so a node reaching the blitter with `0x11`/
  `0x12` always has consistent clip fields. Do not add extra guards.
* **0x12 has no clip-aware raw renderer in the original** — the table maps it to
  the same `0x58CBD` as the unclipped `0x02`. `sprite_render_raw` therefore
  takes only `width`/`rows`; clipping for the raw path is applied by the
  composite driver's `dst` offset and the reduced `rows`, not by the renderer.
  If a hand-built node pairs `0x12` with non-zero clip fields, that is a caller
  error, not a case to handle.

- [ ] **Step 4: Run the tests** — expect pass.
- [ ] **Step 5: Negative control** — remove the `RAW+HFLIP` early return and
  confirm the type-10 assertion fails. Revert.
- [ ] **Step 6: `make verify` then commit**

```bash
make verify && git add -A && git commit -m "sprite: blitter dispatch table (0x51E5C)"
```

---

## Task 10: `render` — the display list

**Files:**
- Create: `port/src/platform/render.h`, `port/src/platform/render.c`
- Create: `port/tests/test_render.c`
- Modify: `port/tests/run_tests.c`, `port/tests/test.h`, `port/CMakeLists.txt`

**Interfaces:**
- Produces:
  ```c
  void render_list_init(void);                  /* 0x1C350 */
  int  render_list_insert(u32 pset_off);        /* 0x1C390 + 0x1C3A0 */
  void render_list_remove(u32 pset_off);        /* 0x1C3D0 + 0x1C458 */
  void render_list_sort(void);                  /* 0x1C3FC */
  u32  render_list_head(void);                  /* for tests */
  int  render_list_count(void);                 /* for tests */
  ```

- [ ] **Step 1: Write the failing test**

```c
#define RSCRATCH 0x3F00000u

static void check_list_order(void)
{
    render_list_init();
    CHECK_EQ_INT(render_list_count(), 0);

    /* Three hand-built psets in layer order 5, 1, 3 (insertion order).
     * RSCRATCH = 0x3F00000u, a free region near the top of mem[]: below
     * MEM_SIZE (0x4000000, so 0x04000000 and up are out-of-bounds writes) and
     * above test_gra.c's SCRATCH (0x3000000), which whole .GRA files are
     * loaded into. */
    u32 p1 = RSCRATCH + 0x00u, p2 = RSCRATCH + 0x20u, p3 = RSCRATCH + 0x40u;
    DSW(p1 + 0x0E) = 5; DSW(p2 + 0x0E) = 1; DSW(p3 + 0x0E) = 3;
    CHECK_EQ_INT(render_list_insert(p1), 1);
    CHECK_EQ_INT(render_list_insert(p2), 1);
    CHECK_EQ_INT(render_list_insert(p3), 1);
    render_list_sort();

    CHECK_EQ_INT(render_list_count(), 3);
    u32 n = render_list_head();
    CHECK_EQ_INT(DSD(n + 4), p2); n = DSD(n);
    CHECK_EQ_INT(DSD(n + 4), p3); n = DSD(n);
    CHECK_EQ_INT(DSD(n + 4), p1);

    /* Stability: equal layers keep insertion order. */
    render_list_init();
    u32 q1 = RSCRATCH + 0x100u, q2 = RSCRATCH + 0x120u;
    DSW(q1 + 0x0E) = 2; DSW(q2 + 0x0E) = 2;
    CHECK_EQ_INT(render_list_insert(q1), 1);
    CHECK_EQ_INT(render_list_insert(q2), 1);
    render_list_sort();
    n = render_list_head();
    CHECK_EQ_INT(DSD(n + 4), q1); n = DSD(n);
    CHECK_EQ_INT(DSD(n + 4), q2);

    /* Remove returns the node to the free-list and keeps the list sound. */
    render_list_remove(q1);
    CHECK_EQ_INT(render_list_count(), 1);
    CHECK_EQ_INT(DSD(render_list_head() + 4), q2);

    /* Exhaustion: 580 nodes, the 581st insert fails without corrupting. */
    render_list_init();
    for (int i = 0; i < 580; i++)
        CHECK_EQ_INT(render_list_insert(RSCRATCH + 0x200u + (u32)i * 0x20u), 1);
    CHECK_EQ_INT(render_list_insert(RSCRATCH + 0x30000u), 0);
    CHECK_EQ_INT(render_list_count(), 580);
}
```

The pset offsets above are arbitrary `mem[]` scratch addresses in the port's
free region; the test writes only the layer word the list reads.

- [ ] **Step 2: Run and watch it fail**
- [ ] **Step 3: Implement**

`render_list_init` resets the 580-node pool at `DS_0010153C` (stride 8) into the
free-list at `DS_0010275C`, and sets the head `DS_00105B44 = 0` (the port uses
0 as the empty head; the original's `0x14328` tests `*headp == 0`, so a null
head is the faithful representation — record it as a `/* PORT: ... */` note).
Keep a port-side count for `render_list_count`.

`render_list_insert` walks the list and splices before the first node whose
`pset->layer > target->layer`, stable. `render_list_sort` is one insertion-sort
pass that re-inserts every out-of-order node through the same splice, so the
ordering logic exists once.

- [ ] **Step 4: Run the tests** — expect pass.
- [ ] **Step 5: Negative control** — change the comparison to `>=` and confirm
  the stability assertion fails. Revert.
- [ ] **Step 6: `make verify` then commit**

```bash
make verify && git add -A && git commit -m "render: display list pool, insert, remove, sort"
```

---

## Task 11: `render_list` — projection, clipping, composite

**Files:**
- Modify: `port/src/platform/render.{h,c}`
- Modify: `port/tests/test_render.c`

**Interfaces:**
- Consumes: `sprite_node_build`, `sprite_blit`, `render_list_head`.
- Produces:
  ```c
  struct RenderCamera { int x, y, clip_l, clip_t, clip_r, clip_b; };
  extern const struct RenderCamera render_camera_default;   /* {0,0,0,0,320,200} */
  void render_list(void);
  int  render_proj_x(int v);   /* exposed for the rounding test */
  int  render_proj_y(int v);
  ```

- [ ] **Step 1: Write the failing test**

Add these helpers to `test_render.c` and call them from `int test_render(void)`:

```c
static void check_proj_rounding(void)
{
    /* The original's idiom: p = v * 3901 + 0x800, then + 0xFFF when p < 0, then
     * >>12. Half toward +infinity, so the projection is ASYMMETRIC at negative
     * exact values: proj_x(-4096) is -3900, not -3901. The negative cases are
     * the ones a plain >>12 gets wrong. */
    CHECK_EQ_INT(render_proj_x(0), 0);
    CHECK_EQ_INT(render_proj_x(4096), 3901);
    CHECK_EQ_INT(render_proj_x(-4096), -3900);
    CHECK_EQ_INT(render_proj_x(-1), 0);
    /* This one discriminates the corrected idiom from the plausible-looking
     * `p - ((p >> 31) << 12)`: that form yields -1949 here, the original -1950. */
    CHECK_EQ_INT(render_proj_x(-2048), -1950);
    CHECK_EQ_INT(render_proj_x(1), (3901 + 0x800) >> 12);
    CHECK_EQ_INT(render_proj_y(4096), 3414);
    CHECK_EQ_INT(render_proj_y(-4096), -3413);

    /* Exhaustive small-range self-consistency pin, computed with the corrected
     * idiom. The explicit values above are the real discriminators. */
    for (int v = -8192; v <= 8192; v++) {
        int p = v * 3901 + 0x800;
        if (p < 0) p += 0xFFF;
        CHECK_EQ_INT(render_proj_x(v), p >> 12);
    }
}

static void check_offscreen_skip(void)
{
    render_list_init();

    /* A pset whose sprite is entirely off-screen must be skipped and leave the
     * back buffer untouched. Layer 3 takes the un-shifted coordinate path. */
    u32 back = DSD(DS_000E87A4);
    static u8 copy[320 * 200];
    memcpy(copy, mem + back, sizeof copy);

    u32 p = RSCRATCH + 0x400u;
    DSW(p + 0x00) = 0x2C11u;              /* s16attrc.gra, 9x8 RLE */
    DSD(p + 0x04) = -400;                 /* projected x is far negative */
    DSD(p + 0x08) = 0;
    DSW(p + 0x0E) = 3;
    DSD(p + 0x18) = RSCRATCH + 0x600u;    /* palette pointer; bank byte below */
    DSB(RSCRATCH + 0x600u + 8u) = 1;      /* bank 1 => offset 0 */
    CHECK_EQ_INT(render_list_insert(p), 1);
    render_list();
    CHECK(memcmp(mem + back, copy, sizeof copy) == 0,
          "an off-screen sprite composites nothing");

    /* The same sprite moved on-screen must change the buffer, so the check
     * above cannot pass vacuously. Choose pset x so the projected position is
     * 0: x = proj_x(pset_x) - xorg, so pset_x = the value whose projection
     * equals xorg. Search a small range rather than inverting the projection. */
    GraSprite g; u32 dh = 0;
    CHECK_EQ_INT(gra_sprite_lookup(0x2C11u, &g, &dh), 1);
    int px = 0;
    for (int v = 0; v < 4096; v++)
        if (render_proj_x(v) >= (int)g.xorg) { px = v; break; }
    DSD(p + 0x04) = px;
    render_list();
    CHECK(memcmp(mem + back, copy, sizeof copy) != 0,
          "an on-screen sprite changes the buffer");
}
```

The partially-clipped pixel comparison belongs to Task 12's end-to-end test,
which can assert against `sprite_render_rle` output directly.

- [ ] **Step 2: Run and watch it fail**

- [ ] **Step 3: Implement**

`render_list` per the Format reference. Expose `render_proj_x/y` via `render.h`
so the rounding is testable in isolation. Record the `proj_scale` idiom as a
`/* PORT: ... */` comment naming `0x14328`, since the port writes a named helper
where the original inlines the `imul`/`sar`/`sbb` sequence.

Mode-1 and mode-2 depend on `DS_00107A3E`, `DS_00107A4E`, `DS_00107A3A`,
`DS_00107A38` and `DS_000F0AEC`, all zero in a compositor-only run, so mode 1
degenerates to `y = -camera.y` and mode 2 to the last mode-1 `y - rows`. Add a
third helper `check_layer_modes(void)`: with those globals at zero, a
layer-3 entry takes the un-shifted path, a layer-1 entry gets `type |= 4`, and a
following layer-2 entry takes its y from the layer-1 entry's y minus its own
rows. Assert the composited y by comparing against a directly-built node.

- [ ] **Step 4: Run the tests** — expect pass.
- [ ] **Step 5: Negative control** — replace `proj_scale` with a plain `>>12`
  and confirm the negative-coordinate assertions fail. Revert.
- [ ] **Step 6: `make verify` then commit**

```bash
make verify && git add -A && git commit -m "render: projection, clipping and composite driver (0x14328)"
```

---

## Task 12: Wire into `game_loop`, end-to-end

**Files:**
- Modify: `port/src/game/flow.c`
- Modify: `port/tests/test_render.c` (end-to-end)

**Interfaces:**
- Consumes: `render_list_init`, `render_list_sort`, `render_list`.
- Produces: a live composite call in the master loop.

- [ ] **Step 1: Write the failing end-to-end test**

```c
/* The expected buffer: the back buffer as it is now, plus `bank`'s rendering of
 * the sprite at (x, y). Writing into a copy of the live back buffer (rather
 * than into a zeroed one) is what makes the comparison valid — the sprite has
 * transparent runs and the buffer has existing content underneath. */
static void expect_composite(u8 *out, u32 icon, int x, int y,
                             const u8 *px, const GraSprite *g, u8 bank)
{
    memcpy(out, mem + icon, 320 * 200);
    CHECK_EQ_INT(sprite_render_rle(px, out + (u32)y * 320u + (u32)x,
                                   g->width, g->height, 320, bank), 0);
}

static void check_end_to_end(void)
{
    render_list_init();

    GraSprite g; u32 dh = 0;
    CHECK_EQ_INT(gra_sprite_lookup(0x2C11u, &g, &dh), 1);
    const u8 *px = NULL;
    CHECK_EQ_INT(gra_sprite_pixels(g.pixel_handle, &px), 1);

    /* pset+0x18 is a resource HANDLE, not a mem[] offset: the original's title
     * psets hold values like 0x4197C6C, which decode as index 8 / in-range
     * offsets into s16attrc.gra, while as raw offsets they would exceed
     * MEM_SIZE. The blitter resolves it and reads byte 8. So find two real
     * resolvable handles whose bank byte differs, and derive the expected banks
     * from them rather than hard-coding a pointer. */
    u32 pal_a = 0, pal_b = 0;
    u8 bank_a = 0, bank_b = 0;
    for (u32 id = 0; id < 0x7FFFu && pal_b == 0; id++) {
        GraSprite g2; u32 h = 0;
        if (!gra_sprite_lookup(id, &g2, &h)) continue;
        u8 b = (u8)sprite_bank(h);
        if (pal_a == 0) { pal_a = h; bank_a = b; }
        else if (b != bank_a) { pal_b = h; bank_b = b; }
    }
    CHECK(pal_a != 0 && pal_b != 0 && bank_a != bank_b,
          "two resolvable handles with different banks");

    /* Layer-3 psets take their coordinates un-shifted, so choose the pset x/y
     * whose projections equal the pivots and land the sprite at (0, 0). */
    int pset_x = 0, pset_y = 0;
    for (int v = 0; v < 4096; v++) {
        if (render_proj_x(v) >= (int)g.xorg) { pset_x = v; break; }
    }
    for (int v = 0; v < 4096; v++) {
        if (render_proj_y(v) >= (int)g.yorg) { pset_y = v; break; }
    }
    int sx = render_proj_x(pset_x) - (int)g.xorg;
    int sy = render_proj_y(pset_y) - (int)g.yorg;

    u32 pa = RSCRATCH + 0xA00u, pb = RSCRATCH + 0xA20u;
    /* layer > 2 stores its coordinates PRE-SHIFTED: render_list does
     * px = DSD(pset+4) >> 6 for layers 3 and up. */
    DSW(pa + 0x00) = 0x2C11u; DSD(pa + 0x04) = pset_x << 6;  /* 9x8 RLE, s16attrc */
    DSD(pa + 0x08) = pset_y << 6;  DSW(pa + 0x0E) = 3;
    DSD(pa + 0x18) = pal_a;
    DSW(pb + 0x00) = 0x2C11u; DSD(pb + 0x04) = pset_x << 6;  /* same sprite, other bank */
    DSD(pb + 0x08) = pset_y << 6;  DSW(pb + 0x0E) = 4;
    DSD(pb + 0x18) = pal_b;

    static u8 expect[320 * 200];
    u32 back = DSD(DS_000E87A4);

    /* Layer 4 (bank offset 2) is higher, so it wins where they overlap. */
    expect_composite(expect, back, sx, sy, px, &g, bank_b);
    CHECK_EQ_INT(render_list_insert(pa), 1);
    CHECK_EQ_INT(render_list_insert(pb), 1);
    render_list_sort();
    render_list();
    CHECK(memcmp(mem + back, expect, sizeof expect) == 0,
          "the higher layer's pixels win");

    /* Swap the layers: bank offset 0 must now win. Two-sided by construction —
     * if neither pset composited, neither assertion holds. */
    DSW(pa + 0x0E) = 4; DSW(pb + 0x0E) = 3;
    expect_composite(expect, back, sx, sy, px, &g, bank_a);
    render_list_sort();
    render_list();
    CHECK(memcmp(mem + back, expect, sizeof expect) == 0,
          "swapping the layers swaps the winner");
}
```

Note the renderers agree with the blitter on the destination address: the
blitter computes `mem + DSD(DS_000E87A4) + DS_001088F8[y] + x`, and
`DS_001088F8[y] == y * 320`, which is exactly how `expect_composite` indexes.
If this comparison fails while `check_rle_cross` passes, the fault is in
`render_list`'s coordinate or clip arithmetic, not the renderer.

- [ ] **Step 2: Run and watch it fail**
- [ ] **Step 3: Implement the wiring**

In `game_init`, after `surface_setup()` / `palette_list_init()`, add
`render_list_init();`.

In `game_loop`, after the `run_process_table(DS_000A86C4, …)` **render-table**
call (`flow.c:531`) and before `gfx_flush_palette()` (`flow.c:540`), insert:

```c
        render_list_sort();                  /* 0x1C3FC */
        render_list();                       /* 0x14328 */
```

**The insertion point is load-bearing.** The original's order inside `0x255CC` is
`FUN_00024c5c` (game_frame) → render-table dispatch → `0x1C3FC` (sort) →
`0x14328` (composite) → `0x1C470` (palette) → present. The render table is what
updates the psets, so sorting or compositing before it would composite a
one-frame-stale list. (`0x255CC` gates all of sort/composite/present behind
`DS_0010150C == DS_00101508`; the port's vblank pacing subsumes that gate, as the
existing loop already does for present.)

With 4a-ii not yet delivered the list is empty, so this is a no-op —
which is exactly why it is safe to land now.

- [ ] **Step 4: Run the tests and the checks**

```bash
PR_GAME_DIR=data/game/C make test && make verify
```

Expected: pass. The existing `--check` frame captures must be unchanged (an
empty list composites nothing), which is itself the assertion that the wiring
did not disturb the title fake.

- [ ] **Step 5: Confirm the existing behaviour is untouched**

```bash
PR_GAME_DIR=data/game/C make check 2>&1 | tail -5
git diff --stat HEAD~1 -- port/src/game/flow.c
```

Expected: `all checks passed`; the diff is the two call lines plus
`render_list_init()`.

- [ ] **Step 6: `make verify` then commit**

```bash
make verify && git add -A && git commit -m "flow: composite the display list in the master loop"
```

---

## Task 13: Docs and cycle report

**Files:**
- Create: `docs/superpowers/plans/2026-09-17-sprite-compositor-report.md`
- Modify: `FORMATS.md`, `port/RE_GUIDE.md`, `port/spec/game_flow.md`, `README.md`

- [ ] **Step 1: Write the cycle report**

Sections, in the style of the 2b report:
1. What landed, with file list and commit range.
2. The oracle: the three-way RLE agreement (`sprite.c`, `gra_decode_frame`,
   `tools/gra_render.py`), how many sprites it covered, and the control-byte
   class coverage.
3. Proven vs unproven. Explicitly: the clipped/skew/mirror renderers are proven
   by hand-computed tests, not by an emulator; the port's windowed formulation
   is a `PORT` deviation from the original's six straddle branches, and the
   evidence for its equivalence is those tests.
4. The pinned original quirks: `DAT_00081310[0] == 0x5D110`, `RAW+HFLIP` no-op.
5. The corrections this cycle made to earlier notes: `DAT_000BBDC8` static;
   the title's `0x41`/`0x43` as voice cancels; the title's sprites from
   `S16ATTRC`; `+0x2C` not a display-path scale; the clip rect being a literal.
6. What 4a-ii still needs.

- [ ] **Step 2: Update `FORMATS.md`**

Add the sprite descriptor record, the sprite-handle table, the display node, the
control-byte grammar, and the bank/colour tables. Correct any statement that
`+4`/`+6` of the sprite header are `xorg`/`yorg` in the opposite order.

- [ ] **Step 3: Update `port/RE_GUIDE.md` and `port/spec/game_flow.md`**

Record the three-stage pipeline and the compositor's ownership, and point at the
new report.

- [ ] **Step 4: Correct the `flow.c` comments as part of the docs task**

`flow.c:333-337` (the `DAT_000BBDC8` note) and `flow.c:233-241` (the title-music
rationale) are known wrong. Correct them with the evidence, and note in the
report that this corrects 2a's rationale without changing its oracle result.
This is a comment-only change, so it cannot alter behaviour; confirm the build
is unchanged.

- [ ] **Step 5: `make verify` then commit**

```bash
make verify && git add -A && git commit -m "docs: sub-project 4a-i report, format notes, and flow comment corrections"
```

---

## Self-review notes (for the implementer)

* **The projection idiom is the easiest thing to get silently wrong.** Task 11's
  negative-coordinate assertions exist precisely because a truncating `>>12`
  passes every positive test.
* **The clipped RLE renderer is where the original is ugliest.** The port's
  window-intersection formulation is deliberately not a transcription; if you
  find yourself reproducing the six straddle branches, stop and re-read the
  `PORT` note in `sprite.c`.
* **`sprite_render_shear` must read the table as signed `i16`.** The test that
  catches this is the negative-shear one.
* **`bank == 0` is not a palette offset.** The original's `DAT_00081310[0]` is a
  stale code pointer, so its dword add makes the offset vary *within* a 4-pixel
  group — alignment-dependent garbage that no byte-wise port can reproduce. Task
  4 pins the table entry to `0x5D110` *and* treats the bank byte as no offset.
  Do not "fix" the table to hide this, and do not reintroduce the stale value as
  an offset either.
* **Task 12 must not change the title's output.** If `--check` frames move, the
  wiring order is wrong.
