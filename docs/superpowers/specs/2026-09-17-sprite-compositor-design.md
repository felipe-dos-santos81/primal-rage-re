# Design: Primal Rage (DOS) → SDL3 port — sub-project 4a-i: the sprite compositor

Sub-project 4a-i of the decomposition. This is the first half of what began as
"4a, the actor system"; brainstorming re-sliced it when exploration showed the
draw path is a three-stage pipeline rather than the single scaled blit the first
draft assumed. 4a-i covers **the compositor**: given a list of display objects,
draw the frame. 4a-ii — the actor pool, pset sync, animation-stream interpreter
and the title wiring — is specified separately in
`2026-09-17-actor-system-design.md`.

Sub-projects 1 (engine core), 2a (audio/AIL) and 2b-i (Smacker video) are
complete and merged on `main` (`2761e55`).

## 1. Context, and why this was re-sliced

The first draft of the 4a spec described a single `sprite_blit(dst, …, scale,
pal_map)`. Exploration falsified that model. The real draw path is:

1. **Actor update** — `0x2A31C` walks the actor list and calls `0x2A1FC` →
   `0x2A820`, which syncs each 0x68-byte actor record into a **pset entry** in a
   second pool (`DAT_001014EC`, stride 0x20), computing screen position from
   parent-relative offsets.
2. **Ordering** — `0x1C3FC` insertion-sorts the singly-linked display list at
   `DS_00105B44` ascending by `pset->layer` (word at `pset+0x0E`), stable.
3. **Composite** — `0x14328` walks that list, builds a 0x40-byte **display
   node** per entry via `0x14268`, projects and clips it, and calls the span
   blitter `0x51E5C`, which dispatches through `PTR_LAB_00080C8C` to a renderer.

Four further corrections came out of the same exploration, all load-bearing:

* **The sub-palette rule — sub-project 1's open item — is solved, and it is
  trivial.** The blitter does
  `bank = DAT_00081310[ ((u8*)node->pal_ptr)[8] ]` and adds it to every source
  pixel. `DAT_00081310[n] == (n-1) * 0x01010101` for `n = 1..255` (verified for
  all 255), so the effect is **`dst_index = src_index + (bank - 1)`** — a
  per-pixel index offset, not a 256-entry remap. There is no per-pixel palette
  table anywhere in the path. The first draft's `const u8 *pal_map` interface
  was wrong.
* **The span tables are static generated data.** `0x80D0C`/`0x80E0C`/`0x81010`/
  `0x81110` are tables of unrolled per-count routines baked into the EXE; no
  code writes them. The port implements the *semantics* (copy N bytes adding the
  bank; fill N pixels with a colour) rather than reproducing the unrolling.
* **The "scale" is not in the pset.** Pset `+0x0C` is copied from the actor and
  never read by the display path. The only scaling is the fixed projection
  `round(v * 3901 / 4096)` horizontally and `round(v * 3414 / 4096)` vertically
  (`3901/4096 = 320/336`, `3414/4096 = 200/240`), applied to *positions*, not to
  sprite pixels. Sprites are blitted 1:1. The `+0x2C` field the first draft read
  as a 4.12 scale is an actor-side value whose display role is nil.
* **The clip rectangle is a literal** `{left 0, top 0, right 320, bottom 200}`
  in the data segment (at `0xA87CC+8` for the master loop's camera struct,
  `0xBCD64+8` for the other caller), not a runtime camera viewport.

The consequence for scope: the compositor is a self-contained unit that
consumes psets and a display list, and can be proved without any actor, any
animation interpreter, and without DOSBox.

## 2. Scope

### In scope

* **Handle-addressed sprite descriptors.** The static table `DS_000A8B30` holds
  18,443 consecutive resource handles. `sprite_id & 0x7FFF` indexes it;
  `res_resolve` yields a 12-byte descriptor
  `{i16 width; i16 height; i16 xorg; i16 yorg; u32 pixel_handle}` whose
  `pixel_handle` resolves to the RLE blob. The crate-6 descriptor layout already
  matches `gra_decode_frame`'s; the new work is addressing by handle.
* **The display node** (0x40 bytes) and its builder `0x14268`.
* **The span blitter** `0x51E5C` and the 32-entry dispatch table
  `PTR_LAB_00080C8C`.
* **All six reachable renderers** — `0x5D218` (RLE), `0x5D28F` (RLE+clip),
  `0x57F80` (RLE+hflip), `0x57FFB` (RLE+hflip+clip), `0x58CBD` (raw+clip),
  `0x5215C` (mode-1 shear copy).
* **The palette bank** as an index offset, including the `bank == 0` anomaly.
* **The composite driver** `0x14328`: layer mode projection, the clip rectangle,
  the node `+0x28/+0x2C/+0x30/+0x34` overhangs, the `type |= 0x10` rule and the
  skip conditions.
* **The list sort** `0x1C3FC` + the sorted insert `0x1C3A0`.
* **The list-node pool** — 580 8-byte `{next; &pset}` nodes at `DS_0010153C`,
  free-list head `DS_0010275C`, alloc `0x1C390`, unlink/free `0x1C3D0`,
  find-by-pset `0x1C458`. The pool and the insert are list machinery, so they
  live here; 4a-ii calls the insert from spawn's emit path.
* **The pset and list-node structs as consumed data** — 0x20-byte pset,
  8-byte link node `{next; &pset}`, head `DS_00105B44`.
* The row-offset table `DS_001088F8` (`y*320`) and the destination
  `mem + DSD(DS_000E87A4)`.

### Non-goals (4a-ii)

* The actor pool, `0x2AC80`/`0x2AE14`/`0x2BAF4`/`0x249C0`.
* The pset sync `0x2A820` and the actor walk `0x2A31C`.
* The animation-stream interpreter (`0x2A408`/`0x29F34`) and frame selection.
* The title state `0x121A0`, the RNG, and the DOSBox pixel-oracle pinning.
* `0x2BF08` and the string table.
* `vga` twin `0x51ED8` (the 0xA0000 variant): the port never writes the
  aperture, so it has no caller here. Named, not ported.

## 3. Definition of done

1. Given a display list built by hand in a test, `render_list` composites it
   into `mem + DSD(DS_000E87A4)` byte-identically to the original for the
   unclipped, unprojected case.
2. **The oracle is a cross-check against sub-project 1's already-verified
   decoder.** For a fixed, deterministic list of real sprite ids sampled from
   `DS_000A8B30`, rendering through the new RLE span renderer at bank 1 (offset
   0), no clip, onto a zeroed buffer must equal `gra_decode_frame`'s output for
   the same descriptor, and must equal `tools/gra_render.py`'s. This is a second
   independent implementation of the same RLE agreeing with the first, on real
   asset data. Runs under `make verify`.
3. Unit tests cover what the cross-check cannot: clipping on all four edges,
   the straddle cases, the hflip renderers, the mode-1 shear copy, the bank
   offset, and the `bank == 0` anomaly.
4. `make verify` stays green; no new compiler warnings.
5. No `data/` writes; no new dependencies.

## 4. Evidence the plan builds on

### 4.1 Dispatch table `PTR_LAB_00080C8C` (32 entries, fixup-applied)

```
[ 0]=0x51E58   1=0x5D218   2=0x58CBD   3=0x51E58
[ 4]=0x5215C   5=0x51E58   6=0x5215C   7=0x51E58
[ 8]=0x51E58   9=0x57F80  10=0x51E58  11..16=0x51E58
[17]=0x5D28F  18=0x58CBD  19=0x51E58  20=0x5215C  21=0x51E58
[22]=0x5215C  23..24=0x51E58  [25]=0x57FFB  26..31=0x51E58
```

`0x51E58` is a bare `ret` one byte before the real blitter entry `0x51E5C`. Only
indices 1, 2, 4, 6, 9, 17, 18, 20, 22, 25 are live. Index = `node->type & 0x7F`;
only `type & 0x1F` is meaningful. Type bits: `0x01` RLE base, `0x02` raw base,
`0x04` mode-1, `0x08` hflip, `0x10` clipped. `RAW+HFLIP` (10) is deliberately a
stub — hflip is valid only on the RLE base.

### 4.2 Display node (0x40 bytes, stack-allocated by `0x14328`)

| off | width | filled by | meaning |
|---|---|---|---|
| `+0x00` | i32 | `0x14328` | screen x |
| `+0x04` | i32 | `0x14328` | screen y |
| `+0x08` | i32 | `0x14268` from header `@+4` | x pivot/origin, hflip-adjusted |
| `+0x0C` | i32 | `0x14268` from header `@+6` | y pivot/origin |
| `+0x10` | u32 | `0x14268` + `0x14328` | type bits |
| `+0x14` | i32 | `0x14268` | rows; blitter subtracts `+0x34` |
| `+0x18` | i32 | `0x14268` | width |
| `+0x1C` | u32 | `0x14328` from `pset+0x18` | palette pointer; bank at `[+8]` |
| `+0x20` | u32 | `0x14268` from header `@+8` | pixel handle |
| `+0x24` | u32 | `0x14268` from `DAT_000A8B30[id]` | descriptor handle |
| `+0x28` | i32 | `0x14328` | left overhang |
| `+0x2C` | i32 | `0x14328` | right overhang |
| `+0x30` | i32 | `0x14328` | top overhang (blitter saves/restores) |
| `+0x34` | i32 | `0x14328` | bottom overhang (blitter consumes) |
| `+0x38` | i32 | renderers | scratch: `0x140 - visible_width` (or `0x140 + w` hflip) |
| `+0x3C` | i32 | `0x5215C` | scratch: row index into `DS_00107900` |

Header field order is mechanically verified: `header@+4` feeds the **x** pivot
and `@+6` the **y** pivot. Where `FORMATS.md` names them the other way, the
names are swapped, not the offsets.

### 4.3 `0x14268` — node build (inputs: `eax` = node, `dx` = sprite id)

```
if (id == 0) { rows = width = xorg = yorg = 0; return; }   /* +0x10/+0x1C/+0x20/+0x24 stale */
handle      = DS_000A8B30[id & 0x7FFF]
node->desc  = handle
hdr         = res_resolve(handle)
sign        = (i16)hdr->height
if (sign < 0) { node->rows = -sign; node->width = -(i16)hdr->width; type = 2; }
else          { node->rows =  sign; node->width =  (i16)hdr->width; type = 1; }
node->pixel = hdr->pixel_handle
node->xorg  = (i16)hdr->xorg
node->yorg  = (i16)hdr->yorg
if (id & 0x8000) { type |= 8; node->xorg = node->width - node->xorg - 1; }
```

x and y are **not** inputs here; `0x14328` writes them last, immediately before
the call.

### 4.4 `0x14328` — composite driver (inputs: `eax` = `&DS_00105B44`, `edx` =
camera struct, `ebx` = clip rect = camera+8)

For each list node (`{next @+0, &pset @+4}`), with `pset` a 0x20-byte entry:

```
build_node(&node, pset->id)                        /* pset+0x00 */
node.pal_ptr = pset->pal_ptr                       /* pset+0x18 */
layer = pset->layer                                /* pset+0x0E */
if (layer > 2) { px = pset->x >> 6; py = pset->y >> 6; }   /* 16.6 fixed */
else           { px = pset->x;      py = pset->y; }
x = round(px * 3901 / 4096) - camera->x - node.xorg
y = round(py * 3414 / 4096) - camera->y - node.yorg
if (layer == 1) {
    x -= round(w16(DS_00107A3E) * 3901 / 4096);
    y  = round((w16(DS_00107A4E) + (DS_000F0AEC >> 6)) * 3414 / 4096) - camera->y;
    last_mode1_y = y;  type |= 4;
}
if (layer == 2) {
    x -= round(w16(DS_00107A3A) * 3901 / 4096);
    if (last_mode1_y == -1) y -= round(w16(DS_00107A38) * 3414 / 4096);
    else                    y  = last_mode1_y - node.rows;
}
W = node.width; H = node.rows;
if (W + x < 0 || H + y < 0) continue;              /* fully off-screen */
node.clip_b = max((H + y) - clip.bottom, 0);
node.clip_r = max((W + x) - clip.right,  0);
t = clip.top  - y; node.clip_t = max(t, 0); if (t >= 0) y = clip.top;
l = clip.left - x; node.clip_l = max(l, 0); if (l >= 0) x = clip.left;
sumX = node.clip_l + node.clip_r;  sumY = node.clip_t + node.clip_b;
if (sumX + sumY == 0)                     draw;
else if (sumX < W && sumY < H)          { type |= 0x10; draw; }
else                                      continue;
draw: node.x = x; node.y = y; sprite_blit(&node);
```

`round()` is `(v + 0x800) >> 12` after a sign-corrected multiply — i.e.
round-to-nearest, and it must be reproduced exactly (the original uses the
`imul` / `sar 31` / `shl 12` / `sbb` / `sar 12` idiom). `max(...,0)` clamps are
on signed values.

`last_mode1_y` starts at `-1` and persists across list entries, which is why the
list must be layer-sorted (mode 2 reuses the previous mode-1 entry's y).

### 4.5 `0x51E5C` — blitter (input: `eax` = node)

```
if (node->width == 0 || node->rows == 0) return;
saved_rows = node->rows; saved_top = node->clip_t;
src    = res_resolve(node->pixel);
bank   = DAT_00081310[ ((u8*)node->pal_ptr)[8] & 0xff ]
dst    = mem + DSD(DS_000E87A4) + DS_001088F8[node->y] + node->x
rows   = node->rows - node->clip_b          /* stored back into node->rows */
width  = node->width
call renderer_table[node->type & 0x7F]
node->clip_t = saved_top; node->rows = saved_rows;   /* call is transient */
```

`DST`'s row table `DS_001088F8[y] = y * 0x140` is already built by the port's
`surface_setup` (`0x51F45`). `res_resolve` is the port's existing handle
resolver. `bank == 0` yields `0x0005D110` (a stale code pointer used as a
colour) — see 4.7.

### 4.6 Renderer semantics

Givens: `src` = RLE stream, `dst` = destination, `width` = full row width,
`bank` = replicated-bank dword, stride stored to `node->+0x38`.

**Span primitives** (the port implements these as functions; the original uses
tables of unrolled routines):

* `copy_n(dst, src, n, bank)` — for each of `n`: `*dst++ = *src++ + bank`
  (byte-wise; the original's dword form is equivalent only while no byte
  overflows, so byte-wise is the correct model).
* `fill_n(dst, n, color)` — write `color` (a replicated byte) `n` times;
  `dst += n`, `src` untouched.

**Control byte classification** (identical in all four RLE renderers):

| byte | meaning |
|---|---|
| `0x00..0x7F` | literal run: `n = byte`, then `copy_n(n)` |
| `0x80..0xBF` | fill run: `n = byte & 0x3F`, colour = `DAT_00081314[src[0]] + bank` |
| `0xC0..0xFF` | transparent run: `n = byte & 0x3F`, `dst += n`, `src` not advanced |

`DAT_00081310[n] == (n-1) * 0x01010101` for `n=1..255`; `DAT_00081314[n] == n *
0x01010101` for `n=0..255` — i.e. `colour = (palette_index + bank - 1)`
replicated. Both verified for every entry.

**`0x5D218` (RLE, unclipped).** `node->+0x38 = 0x140 - width`. Per row, decode
control bytes until `width` pixels are consumed, then `dst += node->+0x38`,
`--rows`. `src` is continuous across rows. Does not read `+0x28/+0x2C/+0x30`.

**`0x5D28F` (RLE, clipped).** `vis = width - L - R`;
`node->+0x38 = 0x140 - vis`.
1. Top skip: if `T != 0`, consume `T` whole rows by decoding without drawing
   (literal: `src += n`; fill: `src += 1`; transparent: nothing), then
   `--rows`, `--T`.
2. Dispatch: `L==0 && R==0` → the `0x5D218` body; `L!=0 && R==0` → left-only;
   `L==0 && R!=0` → right-only; both → both.
3. Each row is `[L skip][vis draw][R skip]`, with straddle handling when a
   single span crosses a clip edge: the span is split at the edge, the drawn
   part is drawn, the surplus is carried into the next phase, and `src` is
   rewound (`sub esi, over`) for the literal case so the clipped payload is not
   consumed. Row end: `dst += node->+0x38`, `width` reset, `--rows`.

**`0x57F80` (RLE, hflip).** As `0x5D218` but `node->+0x38 = 0x140 + width`,
`dst += width - 1` at entry, all destinations walk **backward**
(`copy_n`/`fill_n` decrement `dst`), and rows advance by `dst += 0x140 + width`.

**`0x57FFB` (RLE, hflip+clipped).** As `0x5D28F` with the backward span tables
(`0x80D0C` fill, `0x80E0C` copy), backward destination, `dst += vis - 1` at
entry and `0x140 + vis` per row. The left/right overhangs act on the mirrored
image, so the source-side skip is the right overhang. Structure mirrors
`0x5D28F` case for case.

**`0x58CBD` (raw + clip).** No RLE: the sprite is `width * rows` raw index
bytes. `vis = width - L - R`; `node->+0x38 = 0x140 - vis`. If `T`: `rows -= T`,
`src += T * width`. `src += L`; per row copy `vis` bytes in bulk, then
`src += R`, `dst += node->+0x38`.

**`0x5215C` (mode-1 shear copy).** As `0x58CBD` plus a per-row horizontal shear
read from `DS_00107900`, a Q5 signed 16-bit table populated at runtime by
`0x38A38`:

```
node->+0x3C = 0
per row r:
  src += L
  shear = ((i16)DS_00107900[r] - (i16)DS_00107900[0]) >> 5    /* arithmetic */
  copy vis bytes from src + shear
  src += vis + R       /* net advance = width */
  ++node->+0x3C
  dst += node->+0x38
```

`DS_00107900` is zero in the static image and filled by `0x38A38` (the
mode-7-style ramp `*(i16*)(off + i*2) = step * i`, descending) — an actor-side
producer, so 4a-i only consumes it. If the table is all-zero, the shear is zero
and mode 1 degenerates to a plain copy, which is the correct behaviour for the
state a compositor-only test runs in.

### 4.7 Data tables and structs

* Pset, stride 0x20: `+0x00 u16 sprite id (bit15 = hflip)`, `+0x02 u16 flags`,
  `+0x04 i32 x`, `+0x08 i32 y`, `+0x0C u16`, `+0x0E i16 layer`, `+0x18 u32
  palette pointer`. `+0x10..+0x17` are unused by the display path.
* Link node, stride 0x08: `{u32 next; u32 pset}`; head `DS_00105B44`; allocated
  from a 580-node pool at `DS_0010153C` with free-list head `DS_0010275C`
  (4a-ii's business, but the struct is shared).
* Camera struct: `{i32 x; i32 y; i32 clip_left; i32 clip_top; i32 clip_right;
  i32 clip_bottom}`; the master loop passes `0xA87CC` with clip
  `{0, 0, 320, 200}`.
* `DS_001088F8`: 200 dwords, `y * 320`, built by the port's `surface_setup`.
* `DS_00107900`: the mode-1 shear table (see 4.6).

## 5. Architecture

### 5.1 `port/src/platform/gra.{c,h}` — extend

Add a handle-addressed descriptor opener, so the 12-byte record layout lives in
one place (`gra.c` already owns it for `gra_decode_frame`):

```c
/* Resolves a sprite descriptor handle (as stored in DS_000A8B30) to its
 * 12-byte record. Fills *out and returns 1, or returns 0 if the handle does
 * not resolve. */
typedef struct {
    i16 width, height, xorg, yorg;
    u32 pixel_handle;
} GraSprite;
int gra_sprite_open(u32 desc_handle, GraSprite *out);
int gra_sprite_pixels(u32 pixel_handle, const u8 **out);   /* RLE blob */
```

### 5.2 `port/src/platform/sprite.{c,h}` — new

Owns the node, the builder, the blitter, the six renderers and the bank
arithmetic. It never touches the actor pool or the list; it consumes a node.

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

/* 0x14268: fills everything except x/y and the clip fields. */
void sprite_node_build(SpriteNode *n, u32 sprite_id);

/* 0x51E5C: dispatches on n->type and draws into mem + DSD(DS_000E87A4). */
void sprite_blit(SpriteNode *n);
```

The renderers are `static` inside `sprite.c`; the test seam is
`sprite_blit` plus `sprite_node_build`, which is exactly the original's own
seam (the blitter takes a node, not a sprite).

### 5.3 `port/src/platform/render.{c,h}` — new

Owns the display list — its node pool, its insert/remove primitives, its
ordering — and the projection/clip driver, because that is the layer that knows
about psets, layers and the camera:

```c
/* 0x1C350: reset the 580-node pool at DS_0010153C and its free-list. */
void render_list_init(void);

/* 0x1C390 + 0x1C3A0: allocate a node for pset_off and insert it sorted
 * ascending by pset->layer, stable for equal layers. Called by 4a-ii's spawn
 * emit path. Returns 0 when the node pool is exhausted. */
int render_list_insert(u32 pset_off);

/* 0x1C3D0 / 0x1C458: unlink a pset's node and return it to the free-list. */
void render_list_remove(u32 pset_off);

/* 0x1C3FC: insertion-sort the whole list. */
void render_list_sort(void);

/* 0x14328: composite the list into the back buffer. */
void render_list(void);
```

`render_list` reads the camera struct the port owns. `render_list_sort` is
separate because ordering is observable and worth its own test, and
`render_list_insert` is separate because 4a-ii calls it per spawn.

Why a separate file from `sprite.c`: `sprite.c` is a pure raster unit that can
be tested with a hand-built node and no engine state; `render.c` needs psets,
layers and the camera. Keeping them apart is what makes 4a-i's unit tests
possible without 4a-ii.

### 5.4 Changed: `game/flow.c`

`game_loop` currently calls `run_process_table(DS_000A86C4, …)` and never
composites a display list. It gains `render_list_sort()` then `render_list()`
at the point the original's `0x255CC` calls `0x1C3FC` then `0x14328` — after
`game_frame`, before `gfx_flush_palette`. Until 4a-ii populates the list this is
a no-op (empty list), so it is a safe incremental wiring step.

## 6. Data flow

1. `game_frame` runs the per-frame update (4a-ii fills psets and inserts list
   nodes).
2. `render_list_sort()` — insertion sort by layer.
3. `render_list()` — walk the list; per entry `sprite_node_build`, project,
   clip, `sprite_blit`.
4. `sprite_blit` resolves the pixel handle, computes the bank, dispatches to a
   renderer, which writes into `mem + DSD(DS_000E87A4)`.
5. `gfx_flush_palette()` then `gfx_present()` — unchanged.

## 7. Error handling and invariants

**Trust boundary** is the asset set. Every descriptor and pixel handle is
resolved through `res_resolve`, which already validates the index; a handle
that does not resolve makes `sprite_node_build` produce a zero-size node and
`sprite_blit` return early — the same shape as the original's `id == 0` path.
Sprite `rows`/`width` are taken from the asset; a negative width in the header
is the original's own "raw" marker, not an error.

**Never write outside the back buffer.** `sprite_blit` clips on all four edges
via the node's overhangs, and the renderers derive `vis = width - L - R` before
touching `dst`. A negative `vis` (overhangs exceeding the width) is clamped to
zero and the row is skipped rather than written. This is checked in `sprite_blit`
before dispatch, so no renderer can be entered with an impossible row.

**`width == 0 || rows == 0` is a no-op**, matching the blitter's own guard.

**The blitter's transient node mutation is restored.** The original saves and
restores `+0x14` and `+0x30` around the call; the port does the same, so a node
can be re-blitted and `render_list`'s own state is not perturbed.

**Span reads.** The original's unrolled routines read up to 3 bytes past a run
(dword loads for small counts). The port reads byte-wise, so it never over-reads
the blob — a strict improvement with identical output.

**`bank == 0` is an original defect, not a palette operation.** For `n` in
1..255, `DAT_00081310[n]` is `(n-1)` replicated in all four bytes, so the
original's dword add is exactly a per-pixel `+ (n-1)`. `DAT_00081310[0]` holds
the stale code pointer `0x0005D110`, whose four bytes differ, so the same dword
add gives each pixel *inside a 4-pixel group* a different offset — position- and
alignment-dependent garbage. A byte-wise port cannot reproduce an
alignment-dependent carry, so the port treats `bank == 0` as no offset and
records a `/* TODO(verify): */` naming it as the first suspect if the 4a-ii
oracle ever diverges in 4-pixel groups. Whether any shipped asset reaches
`bank == 0` is unknown.

**No dynamic allocation, no SDL** outside `host.c`/`main.c`; `sprite.c` and
`render.c` are pure `mem[]` writers.

**Invariants**
1. Renderers never advance `src` on a transparent run.
2. `node->+0x38` is written before any row loop and never read stale.
3. `type & 0x7F` indexes a 32-entry table; every unlisted index is the no-op.
4. `render_list` never mutates the list.

## 8. Open items

* The exact rounding idiom of the projection must be reproduced bit-for-bit;
  a plain `(v * 3901) >> 12` is **not** equivalent (it truncates toward zero
  rather than to nearest). The plan must transcribe the original idiom.
* `DS_00107900`'s producer `0x38A38` is 4a-ii's; 4a-i only consumes the table.
* `+0x02` pset flags: the display path ignores them; whether the `0x800` bit
  gates visibility is a 4a-ii question.
* `0x51ED8` (the VGA-aperture blitter twin) is not ported; the aperture rule
  forbids writing `0xA0000`.

## 9. Risks

1. **Straddle arithmetic in the clipped RLE renderers** is the fiddliest code in
   the cycle. Mitigated by the cross-check oracle for the unclipped case and by
   hand-computed span tests that walk each straddle branch (span ends inside the
   visible region, span spans one edge, span spans both).
2. **Projection rounding.** A truncating implementation passes casual inspection
   and fails a pixel comparison; the plan transcribes the idiom and adds a unit
   test with negative and positive coordinates.
3. **Mode-1/mode-2 depending on list order.** A wrong sort makes mode-2 produce
   a wrong y silently. Mitigated by a dedicated sort test with equal keys
   (stability) and a mode-2 test whose expected y depends on a prior mode-1
   entry.
4. **The cross-check could pass vacuously** if the sampled sprite ids exercise
   only literal runs. Mitigated by asserting the sample covers all three control
   byte classes (the test counts them).

## 10. Testing and verification

| Test | Proves |
|---|---|
| `test_sprite.c` node build | header → node mapping; negative-height ⇒ type 2 and negated dims; hflip pivot rewrite `w - xorg - 1` |
| RLE cross-check | for N sampled ids: span renderer output == `gra_decode_frame` == `tools/gra_render.py`, on a zeroed buffer, bank 1, no clip; and the sample covers literal/fill/transparent runs |
| clipped RLE | hand-computed expected spans for L-only, R-only, both, and each straddle branch, on a small fixture blob |
| hflip | mirrored expected buffer; row stride `0x140 + w` |
| raw copy | bulk path and `>= 0x80` chunking |
| mode-1 shear | zero table ⇒ plain copy; non-zero table ⇒ hand-computed shifted rows |
| bank | `bank(n)` for n=1..255 equals `(n-1)`; `bank == 0` yields the documented `0x5D110` offset |
| `render.c` pool | alloc/free/reuse ordering; exhaustion returns 0; `render_list_remove` returns the node to the free-list |
| `render.c` sort | ascending by layer; stable for equal layers; list not mutated structurally beyond ordering |
| `render.c` project/clip | negative and positive coords round as the original; off-screen skip; `type |= 0x10` only when partially clipped; each overhang clamp |
| `render.c` end-to-end | a hand-built 2-entry list composites to the expected buffer |

Oracle gate: the cross-check runs under `make verify` and needs only the shipped
assets, so there is no skip/require variable for 4a-i — unlike 2b, this oracle
cannot be absent.

## 11. Stubs consumed and stubs left

Consumed by 4a-i: nothing that was previously a stub — the compositor did not
exist. It creates the seam `render_list()` / `render_list_sort()` that
`game_loop` now calls.

Left for 4a-ii: the actor pool, the pset sync, the animation interpreter, the
title state, the RNG pin, the pixel-exact title oracle, `0x2BF08`, and the
`DS_00107900` producer `0x38A38`. Left for 5: the fight-specific actor types.
