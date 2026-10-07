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

/* 0x1C390. Pops and returns the free-list head (its next becomes the head). */
u32 render_pop_free(void);

/* 0x1C3A0. Splices `node` into the list rooted at *headp before the first node
 * with a greater layer (stable for equal layers). */
void render_splice(u32 *headp, u32 node);

/* PORT: 0x1C390 + 0x1C3A0. Returns 1 when pset_off was added and 0 when the
 * pool is exhausted -- the original would dereference a null free head, so the
 * port guards it; nothing is corrupted either way. */
int render_list_insert(u32 pset_off);

/* 0x1C458. The node whose +4 is pset_off in the list rooted at *headp, or 0. */
u32 render_find(u32 *headp, u32 pset_off);

/* 0x1C3D0. Unlinks `node` from the list rooted at *headp and pushes it on the
 * free-list head (a no-op when `node` is not in the list). */
void render_unlink(u32 *headp, u32 node);

/* PORT: 0x1C458 + 0x1C3D0. A pset_off not in the list is a no-op. */
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

/* Record §K3.1, 0x38990: the per-frame projection inputs of 0x389C4:
 * DS_00107A3C = the low word of DS_000F0AEC with its low byte masked by 0xC0,
 * and DS_00107A4A = DS_000F0AEC / 64 (signed, truncating) + DS_00107A4E, low
 * 16 bits. game_frame calls it at 0x24CC8 (and at 0x24CC3 first when
 * DS_00104B26 != 0). */
void render_scroll_track(void);

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

/* 0x4F228. The projection reset: DS_00107A54 = 0 (the per-frame 0x389C4/
 * 0x38A38 gate in the master loop), DS_00107A55 = al, and the words
 * DS_00107A3A and DS_00107A38 = 0. */
void render_projection_reset(u8 al);

#endif /* PR_RENDER_H */
