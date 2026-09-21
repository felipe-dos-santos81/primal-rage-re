/* PORT: the sprite compositor's display list. A node is 8 bytes,
 * { next @+0, pset @+4 }; the pset's layer is the u16 at pset + 0x0E, and the
 * list is kept ascending by layer, stable for equal layers. The pool is 580
 * nodes at DS_0010153C (580 * 8 = 0x1220 bytes, ending exactly at the free-list
 * head DS_0010275C). The list head DS_00105B44 is 0 when empty: the original's
 * 0x14328 tests *headp == 0, so a null head is the faithful representation. */
#ifndef PR_RENDER_H
#define PR_RENDER_H

#include "types.h"

/* PORT: 0x1C350. Rebuilds the free list over the 580-node pool and empties the
 * display list (head = 0). */
void render_list_init(void);

/* PORT: 0x1C390 (pop the free-list head) + 0x1C3A0 (splice before the first
 * node with a greater layer, stable). Returns 1 when pset_off was added and 0
 * when the pool is exhausted -- the original would dereference a null free
 * head, so the port guards it; nothing is corrupted either way. */
int render_list_insert(u32 pset_off);

/* PORT: 0x1C458 (find the node whose pset is pset_off) + 0x1C3D0 (unlink it and
 * return it to the free-list head). A pset_off not in the list is a no-op. */
void render_list_remove(u32 pset_off);

/* PORT: 0x1C3FC. Restores ascending-by-layer order (stable) by detaching every
 * node that is strictly out of order and re-splicing it through the same splice
 * render_list_insert uses, so the ordering rule exists in one place. Needed
 * because a pset's layer can change while it is listed. */
void render_list_sort(void);

/* PORT: head node's mem[] offset, or 0 when the list is empty. */
u32 render_list_head(void);

/* PORT: number of nodes currently listed. */
int render_list_count(void);

/* PORT: the compositor's camera and clip rectangle. The original's master loop
 * passes DS_000A87CC in edx and `camera + 8` in ebx, so clip_l/t/r/b are the
 * fields at +8..+20. */
struct RenderCamera {
    int x, y;
    int clip_l, clip_t, clip_r, clip_b;
};

/* PORT: the master loop's camera: position {0, 0} and clip {0, 0, 320, 200}. */
extern const struct RenderCamera render_camera_default;

/* PORT: 0x14328. Walks the display list, projects each pset's position, applies
 * the layer-1/layer-2 mode y-rules and the clip rectangle, and blits the visible
 * nodes into the back buffer. */
void render_list(void);

/* PORT: 0x14328's projection: round(v * 3901 / 4096) and round(v * 3414 /
 * 4096). Exposed so the original's rounding idiom can be tested in isolation
 * from the driver. */
int render_proj_x(int v);
int render_proj_y(int v);

/* PORT: 0x389C4. Maintains the attract scroll/zoom projection. When
 * DS_00107A3C >= 1 it derives DS_00107A38 from DS_00107A48 minus the s16 high
 * word of DS_00107A3A, and advances DS_00107A4C to DS_00107A4A only when
 * DS_00107A4A <= DS_00107A4C; otherwise DS_00107A38 = DS_00107A48 and
 * DS_00107A4C = DS_00107A4E. Both arms divide by 64 through the raw's signed
 * truncating form. */
void render_scroll_edge(void);

/* PORT: 0x38A38. Fills the signed-16-bit shear table at DS_00107900 downward
 * from DS_00107A52 - 1, accumulating scaled DS_000F0AF0, conditionally writes
 * DS_00107A3E, then writes DS_00107A44+2 and DS_00107A3A. Each table entry is
 * the raw's signed truncating /256 followed by an arithmetic >> 1; the tail and
 * DS_00107A3E divides are the same signed truncating /256 and /32. */
void render_scroll_fill(void);

/* PORT: 0x38730. The attract scene/zoom projection setup for scene `i`: seeds
 * DS_00107A4E/42/50/40 from the per-scene tables at DS_000BDE1C/BDE2C/BDE0C/
 * BDDFC, derives DS_00107A3A/38, runs 0x387F4/0x38890 (the two scene actors,
 * DS_00107A55/56/48/52) and 0x38A38, then enables the projection with
 * DS_00107A54 = 1. The demo's only call site (0x11AC4 -> 0x20DF4 -> 0x20E7F)
 * passes the state-6 RNG draw as `i`. */
void render_scroll_setup(u32 i);

#endif /* PR_RENDER_H */
