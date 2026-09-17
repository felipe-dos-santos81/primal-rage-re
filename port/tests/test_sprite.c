#include "test.h"
#include "platform/sprite.h"
#include "platform/gra.h"
#include "platform/res.h"
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
     * 12-byte record is { width -975; height -53; xorg 0; yorg 0 }, so
     * rows = -height = 53 and width = -width = 975, and type base is 2.
     * (The plan wrote this pair swapped; verified against the asset record at
     * s16caves.gra offset 0x477D4 and against 0x14268's own width/height use.) */
    SpriteNode r;
    sprite_node_build(&r, 0x2BDFu);
    CHECK_EQ_INT(r.type & 0x02, 2);
    CHECK_EQ_INT(r.type & 0x01, 0);
    CHECK_EQ_INT(r.rows, 53);
    CHECK_EQ_INT(r.width, 975);
}

/* The port now has THREE independent decoders of the same RLE: the new span
 * renderer, gra_decode_frame (sub-project 1, verified against the Python
 * oracle), and tools/gra_render.py. They must agree byte-for-byte on real
 * assets. This is the compositor's primary oracle and needs no emulator.
 *
 * The gra decoder packs a sprite's rows at `width`; the renderer advances by
 * `stride` (0x140, exactly 0x5D218's row pitch), so the comparison is row by
 * row at the renderer's pitch — a whole-buffer memcmp would trip on the row
 * advance alone, not on pixels. `bank` is the source-index offset the blitter
 * computes (sprite_bank -> sprite_bank_offset), so 0 is identity. */
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

        /* The blob length gra_decode_frame derives internally: from the
         * handle's byte offset to the end of its resource block. */
        u32 idx = g.pixel_handle >> 23;
        u32 off = g.pixel_handle & 0x7FFFFFu;
        if (idx >= res_count() || off >= res_size(idx)) continue;
        u32 len = res_size(idx) - off;

        memset(expect, 0, sizeof expect);
        int n = gra_decode_frame_at(px, len, g.width, g.height, expect,
                                    sizeof expect);
        if (n < 0) continue;

        memset(got, 0, sizeof got);
        u8 bank = 0;                       /* offset 0: identity */
        CHECK_EQ_INT(sprite_render_rle(px, got, g.width, g.height, 320, bank), 0);

        int same = 1;
        for (int r = 0; r < g.height && same; r++)
            if (memcmp(expect + (size_t)r * (size_t)g.width,
                       got + (size_t)r * 320u, (size_t)g.width) != 0)
                same = 0;
        CHECK(same, "span renderer == gra decoder");

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
    CHECK(transparents > 0, "sample covers transparent runs");
}

int test_sprite(void)
{
    check_node_build();
    check_rle_cross();
    return 0;
}
