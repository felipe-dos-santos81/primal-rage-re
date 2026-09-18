#include "test.h"
#include "platform/render.h"
#include "platform/sprite.h"
#include "platform/gra.h"
#include "mem.h"
#include "symbols.h"
#include <string.h>

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

    /* Insertion kept the list ordered, so the sort above was a no-op. Disorder
     * a layer in place (as animation does after insertion) and sort again: a
     * single bubble step would leave p1 in the middle, so this pins a full
     * reorder. */
    DSW(p1 + 0x0E) = 0;
    render_list_sort();
    n = render_list_head();
    CHECK_EQ_INT(DSD(n + 4), p1); n = DSD(n);
    CHECK_EQ_INT(DSD(n + 4), p2); n = DSD(n);
    CHECK_EQ_INT(DSD(n + 4), p3);

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

static void check_proj_rounding(void)
{
    /* 0x14328's projection: seed +0x800, then for a negative sum add a further
     * 0xFFF (the sbb-side borrow correction), so the result rounds half toward
     * +infinity, NOT half away from zero. The negative cases are the ones a
     * plain >>12 gets wrong, and -2048 distinguishes this form from the
     * +0x1000 variant (which yields -1949). */
    CHECK_EQ_INT(render_proj_x(0), 0);
    CHECK_EQ_INT(render_proj_x(4096), 3901);
    CHECK_EQ_INT(render_proj_x(-4096), -3900);   /* plain >>12 gives -3901 */
    CHECK_EQ_INT(render_proj_x(-1), 0);          /* plain >>12 gives -1 */
    CHECK_EQ_INT(render_proj_x(-2048), -1950);   /* the +0x1000 form gives -1949 */
    CHECK_EQ_INT(render_proj_x(1), (3901 + 0x800) >> 12);
    CHECK_EQ_INT(render_proj_y(4096), 3414);
    CHECK_EQ_INT(render_proj_y(-4096), -3413);

    /* Exhaustive small-range self-consistency pin: p = v*3901 + 0x800; if
     * p < 0, p += 0xFFF; result = p >> 12. The explicit values above, above all
     * -2048, are the discriminators; this loop keeps the idiom honest. */
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

/* The mode-1/mode-2 y rules. With the five mode globals zero, a layer-3 entry
 * keeps its projected y, a layer-1 entry is forced to y = -camera.y and gets
 * type |= 4, and a following layer-2 entry takes y = last_mode1_y - rows. Since
 * last_mode1_y is 0, the layer-2 node is top-clipped away entirely. Each of the
 * three outcomes is pinned by comparing the composited buffer against nodes
 * built directly and blitted by hand. */
static void check_layer_modes(void)
{
    DSW(DS_00107A3E) = 0; DSW(DS_00107A4E) = 0;
    DSW(DS_00107A3A) = 0; DSW(DS_00107A38) = 0;
    DSD(DS_000F0AEC) = 0;
    /* A non-zero shear ramp makes mode-1's type bit change the raster, so a
     * layer-1 entry cannot pass by accumulating into the raw renderer. Zero the
     * whole ramp first: sprite_render_shear reads one entry per image row, and
     * this test must not depend on whatever the loaded data object holds. */
    for (u32 i = 0; i < 64; i++) DSW(DS_00107900 + i * 2u) = 0;
    DSW(DS_00107900 + 2) = 32;            /* row 1 shifts by 1 */
    DSW(DS_00107900 + 4) = 64;            /* row 2 shifts by 2 */

    render_list_init();
    u32 back = DSD(DS_000E87A4);
    static u8 pristine[320 * 200];
    static u8 got[320 * 200];
    memcpy(pristine, mem + back, sizeof pristine);

    /* Layer 1: the raw sprite 0x2BDF (975x53, pivots 0). Mode-1 forces y to
     * -camera.y == 0 and sets type bit 2, turning raw (2) into clipped shear
     * (0x16) once the 975-wide sprite overhangs the 320-wide clip. Its pset y is
     * set to a value that would be off-screen if the mode-1 rewrite did not
     * happen, so dropping the rewrite is visible. */
    u32 p1 = RSCRATCH + 0x800u;
    DSW(p1 + 0x00) = 0x2BDFu;
    DSD(p1 + 0x04) = 0;                   /* layer <= 2: unshifted -> x = 0 */
    DSD(p1 + 0x08) = 7 * 64;              /* discarded by the mode-1 rewrite */
    DSW(p1 + 0x0E) = 1;
    DSD(p1 + 0x18) = 0;
    CHECK_EQ_INT(render_list_insert(p1), 1);

    /* Layer 2: the 9x8 RLE sprite 0x2C11, following layer 1. Its y is
     * last_mode1_y - rows = 0 - 8 = -8, so it is entirely top-clipped and never
     * blitted. Its pset y makes the fallback branch (y = proj_y(py) - yorg) put
     * it on screen, so ignoring last_mode1_y would draw extra pixels. */
    u32 p2 = RSCRATCH + 0x840u;
    DSW(p2 + 0x00) = 0x2C11u;
    DSD(p2 + 0x04) = 0;                   /* x = proj_x(0) - xorg = -4 */
    DSD(p2 + 0x08) = 144;                 /* proj_y(144) = 120: a visible fallback */
    DSW(p2 + 0x0E) = 2;
    DSD(p2 + 0x18) = 0;
    CHECK_EQ_INT(render_list_insert(p2), 1);

    /* Layer 3: the un-shifted mode path. The stored pset coords are pre-shifted
     * (>>6), so 8<<6 and 144<<6 give px = 8, py = 144 and x = proj_x(8) - 4 = 4,
     * y = proj_y(144) - 4 = 116, both fully on-screen with no clipping. */
    u32 p3 = RSCRATCH + 0x880u;
    DSW(p3 + 0x00) = 0x2C11u;
    DSD(p3 + 0x04) = 8u << 6;
    DSD(p3 + 0x08) = 144u << 6;
    DSW(p3 + 0x0E) = 3;
    DSD(p3 + 0x18) = 0;
    CHECK_EQ_INT(render_list_insert(p3), 1);

    render_list();
    memcpy(got, mem + back, sizeof got);

    /* Rebuild the same composite from directly-built nodes: layer 1 at y = 0
     * with type 0x16 and clip_r 655; layer 3 at (4, 116), type 1, no clip.
     * Layer 2 contributes nothing. */
    memcpy(mem + back, pristine, sizeof pristine);
    SpriteNode n;
    sprite_node_build(&n, 0x2BDFu);
    n.pal_ptr = 0;
    n.type |= 4u | 0x10u;
    n.x = 0; n.y = 0;
    n.clip_l = 0; n.clip_r = 655; n.clip_t = 0; n.clip_b = 0;
    sprite_blit(&n);

    sprite_node_build(&n, 0x2C11u);
    n.pal_ptr = 0;
    n.x = 4; n.y = 116;
    n.clip_l = n.clip_r = n.clip_t = n.clip_b = 0;
    sprite_blit(&n);

    CHECK(memcmp(got, mem + back, sizeof got) == 0,
          "layer modes composite as directly-built nodes");

    /* Restore the shear ramp so later tests start from zero. */
    for (u32 i = 0; i < 64; i++) DSW(DS_00107900 + i * 2u) = 0;
}

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

    /* pset+0x18 is a 0x33754 palette-table entry ({handle; refcount; start;
     * len}), not a resource handle: sprite_bank reads the low byte of the
     * entry's `start` field at +8. Build two entries in scratch with different
     * start bytes, so the two layers' pixels are distinguishable. (The previous
     * version of this check treated pset+0x18 as a resolvable resource handle
     * and searched handles for differing bank bytes; that encoded the
     * sprite_bank bug the fix removes.) */
    u32 pal_a = RSCRATCH + 0x1000u, pal_b = RSCRATCH + 0x1010u;
    DSB(pal_a + 8) = 1;                 /* start 1 => bank offset 0 */
    DSB(pal_b + 8) = 3;                 /* start 3 => bank offset 2 */
    u8 bank_a = (u8)sprite_bank(pal_a);
    u8 bank_b = (u8)sprite_bank(pal_b);
    CHECK(bank_a != bank_b, "the two palette entries give different banks");

    /* Layer > 2 psets store their coordinates pre-shifted by 6 (render_list
     * decodes them with >> 6), so choose the pset x/y whose projections equal
     * the pivots and land the sprite at (0, 0). */
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
    DSW(pa + 0x00) = 0x2C11u; DSD(pa + 0x04) = pset_x << 6;   /* 9x8 RLE, s16attrc */
    DSD(pa + 0x08) = pset_y << 6;  DSW(pa + 0x0E) = 3;
    DSD(pa + 0x18) = pal_a;
    DSW(pb + 0x00) = 0x2C11u; DSD(pb + 0x04) = pset_x << 6;   /* same sprite, other bank */
    DSD(pb + 0x08) = pset_y << 6;  DSW(pb + 0x0E) = 4;
    DSD(pb + 0x18) = pal_b;

    static u8 expect[320 * 200];
    u32 back = DSD(DS_000E87A4);

    /* Layer 4 (bank_b) is higher, so it wins where they overlap. */
    expect_composite(expect, back, sx, sy, px, &g, bank_b);
    CHECK_EQ_INT(render_list_insert(pa), 1);
    CHECK_EQ_INT(render_list_insert(pb), 1);
    render_list_sort();
    render_list();
    CHECK(memcmp(mem + back, expect, sizeof expect) == 0,
          "the higher layer's pixels win");

    /* Swap the layers: bank_a must now win. Two-sided by construction —
     * if neither pset composited, neither assertion holds. */
    DSW(pa + 0x0E) = 4; DSW(pb + 0x0E) = 3;
    expect_composite(expect, back, sx, sy, px, &g, bank_a);
    render_list_sort();
    render_list();
    CHECK(memcmp(mem + back, expect, sizeof expect) == 0,
          "swapping the layers swaps the winner");
}

int test_render(void)
{
    check_list_order();
    check_proj_rounding();
    check_offscreen_skip();
    check_layer_modes();
    check_end_to_end();
    return 0;
}
