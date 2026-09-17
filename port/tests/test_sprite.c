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

static void check_raw_copy(void)
{
    /* A 4x2 raw fixture with the bank offset applied byte-wise. */
    const u8 src[8] = { 1,2,3,4, 5,6,7,8 };
    /* dst must fit two stride-16 rows: the brief's dst[16] would write row 1
     * at dst[16..19], past the end, clobbering the adjacent stack. */
    u8 dst[32]; memset(dst, 0xEE, sizeof dst);
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

/* 0x5D28F / 0x57FFB semantics. Fixture: a 6-wide, 3-row RLE sprite whose row
 * is [literal 2][transparent 1][fill 3], built by repeating the 9-byte ROW so
 * the three stream rows are byte-identical; stride 16, bank 0 (identity). The
 * blob's trailing zero bytes are no-op literals, so a source desync of a few
 * bytes would hide here -- the distinct-payload AB stream below pins source
 * consumption instead. Destination indexing is window-relative: dst[0] is the
 * window's first visible column (render_list clamped node.x to the clip edge),
 * so the expected rows below are the visible window written from dst[0]. */
static void check_clipped_case(const u8 *blob, int clip_l, int clip_r,
                               const u8 *expect, const char *what)
{
    u8 dst[3 * 16];
    memset(dst, 0xEE, sizeof dst);
    CHECK_EQ_INT(sprite_render_rle_clipped(blob, dst, 6, 3, 16, 0,
                                           clip_l, clip_r, 0, 0), 0);
    /* Every stream row is checked, so a per-row source desync (rather than
     * clipping) would also trip this. */
    int same = 1;
    for (int r = 0; r < 3 && same; r++)
        for (int c = 0; c < 6; c++)
            if (dst[r * 16 + c] != expect[c]) same = 0;
    CHECK(same, what);
}

static void check_rle_clipped(void)
{
    static const u8 ROW[9] = { 0x02, 0x0A, 0x0B, 0xC1, 0x00, 0x83, 0x07,0,0 };
    u8 blob[27];
    for (int i = 0; i < 3; i++) memcpy(blob + i * 9, ROW, sizeof ROW);

    /* 0xEE marks a column the window does not cover (dst untouched). */
    static const u8 e_none[6]   = { 0x0A,0x0B,0xEE,0x07,0x07,0x07 };
    static const u8 e_left[6]   = { 0x0B,0xEE,0x07,0x07,0x07,0xEE };
    static const u8 e_ltrans[6] = { 0xEE,0x07,0x07,0x07,0xEE,0xEE };
    static const u8 e_right[6]  = { 0x0A,0x0B,0xEE,0x07,0xEE,0xEE };
    static const u8 e_both[6]   = { 0xEE,0x07,0xEE,0xEE,0xEE,0xEE };
    static const u8 e_spans[6]  = { 0x07,0xEE,0xEE,0xEE,0xEE,0xEE };

    check_clipped_case(blob, 0, 0, e_none,  "clipped: no clip");
    check_clipped_case(blob, 1, 0, e_left,  "clipped: left cuts mid-literal");
    check_clipped_case(blob, 2, 0, e_ltrans,"clipped: left cuts transparent");
    check_clipped_case(blob, 0, 2, e_right, "clipped: right cuts fill run");
    check_clipped_case(blob, 2, 2, e_both,  "clipped: left and right");
    check_clipped_case(blob, 4, 1, e_spans, "clipped: window inside fill run");

    /* clip_t: whole rows of stream consumed without drawing, so the visible
     * rows shift up by one and the last destination slot stays untouched. */
    u8 dst[3 * 16]; memset(dst, 0xEE, sizeof dst);
    CHECK_EQ_INT(sprite_render_rle_clipped(blob, dst, 6, 3, 16, 0, 0,0,1,0), 0);
    for (int c = 0; c < 6; c++) {
        CHECK_EQ_INT(dst[c], e_none[c]);
        CHECK_EQ_INT(dst[16 + c], e_none[c]);
        CHECK_EQ_INT(dst[32 + c], 0xEE);
    }

    /* Source consumption, on rows with DISTINCT payloads: clipping must still
     * consume each whole row's stream, or row B decodes out of sync. */
    static const u8 AB[12] = {
        0x02,0x11,0x12, 0xC1, 0x83,0x05,
        0x02,0x21,0x22, 0xC1, 0x83,0x06,
    };
    static const u8 e_a[6] = { 0x12,0xEE,0x05,0x05,0x05,0xEE };
    static const u8 e_b[6] = { 0x22,0xEE,0x06,0x06,0x06,0xEE };
    memset(dst, 0xEE, sizeof dst);
    CHECK_EQ_INT(sprite_render_rle_clipped(AB, dst, 6, 2, 16, 0, 1,0,0,0), 0);
    int same = 1;
    for (int c = 0; c < 6; c++) {
        if (dst[c] != e_a[c]) same = 0;
        if (dst[16 + c] != e_b[c]) same = 0;
    }
    CHECK(same, "clipped: L=1 consumes each whole row's stream");

    /* vis <= 0: the whole stream is still consumed, nothing is drawn. */
    memset(dst, 0xEE, sizeof dst);
    CHECK_EQ_INT(sprite_render_rle_clipped(blob, dst, 6, 3, 16, 0, 3,3,0,0), 0);
    same = 1;
    for (int i = 0; i < (int)sizeof dst; i++) if (dst[i] != 0xEE) same = 0;
    CHECK(same, "clipped: zero-width window draws nothing");
}

/* The unified rle_row must reduce to the old unclipped row exactly, including
 * at its boundaries: a run that exactly reaches width, and literal/fill/
 * transparent runs that overshoot it (the original clips those to the row but
 * still consumes the whole run from the source). */
static void check_rle_row_edges(void)
{
    /* literal 2 then fill 3 exactly fills the 5-pixel row. */
    static const u8 exact[5] = { 0x02, 0x0A, 0x0B, 0x83, 0x07 };
    u8 out[6]; memset(out, 0xEE, sizeof out);
    CHECK_EQ_INT(sprite_render_rle(exact, out, 5, 1, 6, 0), 0);
    CHECK_EQ_INT(out[0], 0x0A);
    CHECK_EQ_INT(out[1], 0x0B);
    for (int i = 2; i < 5; i++) CHECK_EQ_INT(out[i], 0x07);
    CHECK_EQ_INT(out[5], 0xEE);

    /* literal 9 on a 4-wide row: 4 drawn, the full 9 consumed. */
    static const u8 lo[10] = { 0x09, 1,2,3,4,5,6,7,8,9 };
    u8 l4[4]; memset(l4, 0xEE, sizeof l4);
    CHECK_EQ_INT(sprite_render_rle(lo, l4, 4, 1, 4, 0), 0);
    for (int i = 0; i < 4; i++) CHECK_EQ_INT(l4[i], i + 1);

    /* fill 10 on a 4-wide row: clipped to 4. */
    static const u8 fo[2] = { 0x8A, 0x03 };
    u8 f4[4]; memset(f4, 0xEE, sizeof f4);
    CHECK_EQ_INT(sprite_render_rle(fo, f4, 4, 1, 4, 0), 0);
    for (int i = 0; i < 4; i++) CHECK_EQ_INT(f4[i], 0x03);

    /* transparent 10 on a 4-wide row: nothing drawn. */
    static const u8 to[1] = { 0xCA };
    u8 t4[4]; memset(t4, 0xEE, sizeof t4);
    CHECK_EQ_INT(sprite_render_rle(to, t4, 4, 1, 4, 0), 0);
    for (int i = 0; i < 4; i++) CHECK_EQ_INT(t4[i], 0xEE);
}

/* 0x57F80 (hflip RLE) and 0x57FFB (hflip + clipped RLE): the same row decoder
 * with mirror set must reverse the visible columns. The row fixture is
 * [literal 2][transparent 1][fill 3], so the plain row is 0A 0B __ 07 07 07
 * and its mirror is 07 07 07 __ 0B 0A -- the transparent gap lands on the
 * opposite side and the two non-uniform runs swap ends, so this is not a
 * palindromic pass. */
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

int test_sprite(void)
{
    check_node_build();
    check_rle_cross();
    check_bank_and_colour();
    check_raw_copy();
    check_rle_clipped();
    check_rle_row_edges();
    check_rle_mirror();
    return 0;
}
