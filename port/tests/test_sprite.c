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

/* Scratch for a fake 0x33754 palette-table entry (16 bytes: handle, refcount,
 * start, len). Clear of the resource heap and of the other tests' scratch. */
#define PAL_ENTRY    0x3F00000u

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

    /* sprite_bank interprets its argument as a 0x33754 palette-table entry
     * ({handle; refcount; start; len}), not a resource handle: the bank is the
     * low byte of the entry's `start` field at +8. Build entries in scratch
     * memory. (The previous version of this check resolved a sprite descriptor
     * handle through res_resolve and read its byte 8; that encoded the
     * sprite_bank bug the fix removes.) */
    u32 entry = PAL_ENTRY;
    DSD(entry + 8) = 0x41u;
    CHECK_EQ_INT(sprite_bank(entry), 0x40);       /* sprite_bank_offset(0x41) */
    DSB(entry + 8) = 0;                            /* start 0 => no offset */
    CHECK_EQ_INT(sprite_bank(entry), 0);
    CHECK_EQ_INT(sprite_bank(0), 0);               /* null palette entry */

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
    CHECK_EQ_INT(sprite_render_raw(src, dst, 4, 2, 16, 3, 0,0,0), 0);
    for (int i = 0; i < 4; i++) CHECK_EQ_INT(dst[i], src[i] + 3);
    for (int i = 0; i < 4; i++) CHECK_EQ_INT(dst[16 + i], src[4 + i] + 3);
    /* The row gap is untouched. */
    for (int i = 4; i < 16; i++) CHECK_EQ_INT(dst[i], 0xEE);

    /* Overflow wraps byte-wise, not into the next pixel. */
    const u8 hi[2] = { 0xFE, 0xFF };
    u8 d2[2] = { 0, 0 };
    CHECK_EQ_INT(sprite_render_raw(hi, d2, 2, 1, 2, 4, 0,0,0), 0);
    CHECK_EQ_INT(d2[0], 0x02);
    CHECK_EQ_INT(d2[1], 0x03);
}

/* Clipped raw (0x58CBD, type 0x12): clip_t whole source rows and clip_l source
 * columns are skipped, and `vis = width - clip_l - clip_r` bytes per drawn row
 * are copied from the window origin. Fixture: 6-wide, 3-row raw, clip_l 1,
 * clip_r 2 (vis 3), clip_t 1 (2 drawn rows), stride 8. Hand-computed: the
 * skipped row 0 and the clipped columns/gap must stay 0xEE, and each drawn
 * row's bytes land at dst[0..2]. A no-clip copy of `width` bytes would draw
 * row 0 and overwrite the dst[3..7] padding -- both caught below. */
static void check_raw_clipped(void)
{
    u8 src[18];
    for (int i = 0; i < 18; i++) src[i] = (u8)(10 + i);
    /* row0 = 10..15 (skipped), row1 = 16..21, row2 = 22..27. */
    u8 dst[2 * 8]; memset(dst, 0xEE, sizeof dst);

    CHECK_EQ_INT(sprite_render_raw(src, dst, 6, 3, 8, 0, 1, 2, 1), 0);
    /* src = row1 + clip_l = index 6+1 = 7 -> 17,18,19. */
    CHECK_EQ_INT(dst[0], 17);
    CHECK_EQ_INT(dst[1], 18);
    CHECK_EQ_INT(dst[2], 19);
    for (int i = 3; i < 8; i++) CHECK_EQ_INT(dst[i], 0xEE);
    /* src advances a whole width to row2 + clip_l = index 12+1 = 13. */
    CHECK_EQ_INT(dst[8], 23);
    CHECK_EQ_INT(dst[9], 24);
    CHECK_EQ_INT(dst[10], 25);
    for (int i = 11; i < 16; i++) CHECK_EQ_INT(dst[i], 0xEE);

    /* vis <= 0 draws nothing; rows - clip_t <= 0 draws nothing. */
    memset(dst, 0xEE, sizeof dst);
    CHECK_EQ_INT(sprite_render_raw(src, dst, 6, 3, 8, 0, 3, 3, 0), 0);
    CHECK_EQ_INT(sprite_render_raw(src, dst, 6, 3, 8, 0, 0, 0, 3), 0);
    for (int i = 0; i < (int)sizeof dst; i++) CHECK_EQ_INT(dst[i], 0xEE);
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

/* Mirrored + clipped RLE (0x57FFB, type 0x19): the visible window is reversed,
 * not the whole `width`. The earlier mirror test uses clip_l = 0 / vis = width,
 * so its formula degenerates to vis-1-c and a bug ignoring clip_l passes. Here
 * clip_l = 1, clip_r = 2 (vis = 3) and the row is
 * [literal 2][transparent 1][fill 3], so the window covers image columns
 * 1,2,3 = {0x0B, transparent, 0x07}; reversed it is {0x07, transparent, 0x0B}.
 * Ignoring clip_l would instead reverse columns 0,1,2 = {0x0A, 0x0B, tr}. */
static void check_rle_mirror_clip(void)
{
    const u8 src[9] = { 0x02, 0x0A, 0x0B, 0xC1, 0x00, 0x83, 0x07,0,0 };
    u8 dst[8]; memset(dst, 0xEE, sizeof dst);
    CHECK_EQ_INT(sprite_render_rle_clipped(src, dst, 6, 1, 8, 0,
                                           1, 2, 0, /*mirror=*/1), 0);
    CHECK_EQ_INT(dst[0], 0x07);
    CHECK_EQ_INT(dst[1], 0xEE);          /* window column 2 is transparent */
    CHECK_EQ_INT(dst[2], 0x0B);
    for (int i = 3; i < 8; i++) CHECK_EQ_INT(dst[i], 0xEE);
}

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

/* Clipped shear (0x5215C, type 0x16) with clip_t > 0 settles the table index:
 * the disassembly indexes DS_00107900 by the DRAWN row, not the image row (the
 * original zeroes node->+0x3C before the loop and increments it only for drawn
 * rows). Fixture: 6-wide, 4-row, clip_l 1, clip_r 2 (vis 3), clip_t 1 (3 drawn
 * rows), stride 8, ramp tab = {0,32,64,96}: drawn-row shears are
 * 0, (32-0)>>5=1, (64-0)>>5=2. The image-row form would give 1,2,3, so row 0
 * discriminates (it would write src index 8, not 7). */
static void check_shear_clipped(void)
{
    u8 src[30];
    for (int i = 0; i < 30; i++) src[i] = (u8)(10 + i);
    u8 dst[3 * 8]; memset(dst, 0xEE, sizeof dst);

    DSW(DS_00107900 + 0) = 0;
    DSW(DS_00107900 + 2) = 32;
    DSW(DS_00107900 + 4) = 64;
    DSW(DS_00107900 + 6) = 96;

    CHECK_EQ_INT(sprite_render_shear(src, dst, 6, 4, 8, 0, 1, 2, 1), 0);
    /* src = clip_t*width + clip_l = 6+1 = 7 (value 17). Drawn row 0: sh 0. */
    CHECK_EQ_INT(dst[0], 17);
    CHECK_EQ_INT(dst[1], 18);
    CHECK_EQ_INT(dst[2], 19);
    for (int i = 3; i < 8; i++) CHECK_EQ_INT(dst[i], 0xEE);
    /* src advances to 13 (value 23). Drawn row 1: sh 1 -> indices 14,15,16. */
    CHECK_EQ_INT(dst[8], 24);
    CHECK_EQ_INT(dst[9], 25);
    CHECK_EQ_INT(dst[10], 26);
    for (int i = 11; i < 16; i++) CHECK_EQ_INT(dst[i], 0xEE);
    /* src advances to 19 (value 29). Drawn row 2: sh 2 -> indices 21,22,23. */
    CHECK_EQ_INT(dst[16], 31);
    CHECK_EQ_INT(dst[17], 32);
    CHECK_EQ_INT(dst[18], 33);
    for (int i = 19; i < 24; i++) CHECK_EQ_INT(dst[i], 0xEE);

    DSW(DS_00107900 + 0) = 0;
    DSW(DS_00107900 + 2) = 0;
    DSW(DS_00107900 + 4) = 0;
    DSW(DS_00107900 + 6) = 0;
}

/* Dispatch pinning: compare the blitter's whole back-buffer output against the
 * raster of the renderer the original's PTR_LAB_00080C8C selects. Comparing
 * output (not the call site) makes a swap between renderer classes -- or wrong
 * clip arguments into a shared renderer -- change the compared bytes. */
#define BLIT_BUF (320 * 200)

static u8 blit_pristine[BLIT_BUF];

enum {
    REF_RLE,
    REF_RAW,
    REF_RLE_MIRROR,
    REF_RLE_CLIP,
    REF_RLE_MIRROR_CLIP,
    REF_SHEAR
};

static void blit_ref(u8 *out, int which, const u8 *src, int w, int rows,
                     u8 bank, int x, int y, int L, int R, int T)
{
    memcpy(out, blit_pristine, BLIT_BUF);
    u8 *dst = out + DSD(DS_001088F8 + (u32)y * 4u) + (u32)x;
    switch (which) {
    case REF_RLE:
        sprite_render_rle(src, dst, w, rows, 320, bank); break;
    case REF_RAW:
        sprite_render_raw(src, dst, w, rows, 320, bank, L,R,T); break;
    case REF_RLE_MIRROR:
        sprite_render_rle_clipped(src, dst, w, rows, 320, bank, 0,0,0,1); break;
    case REF_RLE_CLIP:
        sprite_render_rle_clipped(src, dst, w, rows, 320, bank, L,R,T,0); break;
    case REF_RLE_MIRROR_CLIP:
        sprite_render_rle_clipped(src, dst, w, rows, 320, bank, L,R,T,1); break;
    case REF_SHEAR:
        sprite_render_shear(src, dst, w, rows, 320, bank, L,R,T); break;
    }
}

static void blit_node(SpriteNode *n, u32 type, u32 pixels, u32 pal,
                      int w, int rows, int L, int R, int T)
{
    memset(n, 0, sizeof *n);
    n->type = type; n->pixel_handle = pixels; n->pal_ptr = pal;
    n->width = w; n->rows = rows;
    n->clip_l = L; n->clip_r = R; n->clip_t = T;
}

/* Blit one node against the starting buffer and capture the whole back buffer. */
static void blit_run(u8 *out, SpriteNode *n, u32 icon)
{
    memcpy(mem + icon, blit_pristine, BLIT_BUF);
    sprite_blit(n);
    memcpy(out, mem + icon, BLIT_BUF);
}

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
    u32 pal = PAL_ENTRY;            /* fake 0x33754 palette-table entry */
    DSB(pal + 8) = 1;               /* start 1 => bank offset 0 */
    memset(&n, 0, sizeof n);
    sprite_node_build(&n, 0x2C11u);
    n.pal_ptr = pal;                 /* any resolvable pointer with a bank byte */
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
    r.pixel_handle = n.pixel_handle; r.pal_ptr = pal;
    u8 *back = mem + DSD(DS_000E87A4);
    u8 before = back[DSD(DS_001088F8) + 0];
    sprite_blit(&r);
    CHECK_EQ_INT(back[DSD(DS_001088F8) + 0], before);

    /* ---- Pin every live dispatch class by its whole-buffer output raster. */
    {
        const u8 *rle = NULL;
        u32 icon2 = DSD(DS_000E87A4);
        u32 pix = n.pixel_handle;
        u8 bank = (u8)sprite_bank(pal);
        SpriteNode s;
        static u8 got[BLIT_BUF], got2[BLIT_BUF], ref[BLIT_BUF], ref2[BLIT_BUF];

        memcpy(blit_pristine, mem + icon2, BLIT_BUF);

        u32 idx = pix >> 23, off = pix & 0x7FFFFFu;
        u32 blen = (idx < (u32)res_count() && off < res_size(idx))
                       ? res_size(idx) - off : 0;
        CHECK(blen >= 40, "0x2C11 blob is long enough for the fixtures");
        CHECK_EQ_INT(gra_sprite_pixels(pix, &rle), 1);

        /* 0x01 -> unclipped RLE (0x5D218). */
        blit_node(&s, 0x01, pix, pal, 9, 1, 0,0,0);
        blit_run(got, &s, icon2);
        blit_ref(ref, REF_RLE, rle, 9, 1, bank, 0, 0, 0,0,0);
        CHECK(memcmp(got, ref, BLIT_BUF) == 0,
              "0x01 routes to the RLE renderer");
        /* The real blob's control bytes must make RLE differ from a byte copy,
         * or this case could pass a raw/RLE swap. */
        blit_ref(ref2, REF_RAW, rle, 9, 1, bank, 0, 0, 0,0,0);
        CHECK(memcmp(ref, ref2, BLIT_BUF) != 0,
              "0x2C11 blob: RLE raster != raw copy");

        /* 0x02 raw (0x58CBD); 0x12 (RAW|CLIP) shares the same entry, so its
         * raster must equal 0x02's. */
        blit_node(&s, 0x02, pix, pal, 9, 3, 0,0,0);
        blit_run(got, &s, icon2);
        blit_ref(ref, REF_RAW, rle, 9, 3, bank, 0, 0, 0,0,0);
        CHECK(memcmp(got, ref, BLIT_BUF) == 0,
              "0x02 routes to the raw renderer");
        blit_node(&s, 0x12, pix, pal, 9, 3, 0,0,0);
        blit_run(got2, &s, icon2);
        CHECK(memcmp(got2, ref, BLIT_BUF) == 0,
              "0x12 routes to the raw renderer");
        CHECK(memcmp(got, got2, BLIT_BUF) == 0,
              "0x12 shares 0x58CBD with 0x02");

        /* 0x12 with real overhangs must use the clip-aware raw path: its raster
         * matches sprite_render_raw at the node's clip args and differs from the
         * no-clip raster. This pins the Critical fix. */
        blit_node(&s, 0x12, pix, pal, 9, 3, 2,1,1);
        blit_run(got2, &s, icon2);
        blit_ref(ref2, REF_RAW, rle, 9, 3, bank, 0, 0, 2,1,1);
        CHECK(memcmp(got2, ref2, BLIT_BUF) == 0,
              "0x12 with clip routes to clipped raw");
        CHECK(memcmp(ref2, ref, BLIT_BUF) != 0,
              "0x12's clip arguments change the raster");

        /* 0x11 -> clipped RLE, mirror off (0x5D28F). */
        blit_node(&s, 0x11, pix, pal, 9, 1, 2,1,0);
        blit_run(got, &s, icon2);
        blit_ref(ref, REF_RLE_CLIP, rle, 9, 1, bank, 0, 0, 2,1,0);
        CHECK(memcmp(got, ref, BLIT_BUF) == 0,
              "0x11 routes to clipped RLE");

        /* 0x09 (mirror, no clip; 0x57F80) vs 0x19 (mirror+clip; 0x57FFB):
         * same renderer, different arguments -- the rasters must differ. */
        blit_node(&s, 0x09, pix, pal, 9, 1, 0,0,0);
        blit_run(got, &s, icon2);
        blit_ref(ref, REF_RLE_MIRROR, rle, 9, 1, bank, 0, 0, 0,0,0);
        CHECK(memcmp(got, ref, BLIT_BUF) == 0,
              "0x09 routes to mirrored RLE");
        blit_node(&s, 0x19, pix, pal, 9, 1, 2,1,0);
        blit_run(got2, &s, icon2);
        blit_ref(ref2, REF_RLE_MIRROR_CLIP, rle, 9, 1, bank, 0, 0, 2,1,0);
        CHECK(memcmp(got2, ref2, BLIT_BUF) == 0,
              "0x19 routes to mirrored clipped RLE");
        CHECK(memcmp(ref, ref2, BLIT_BUF) != 0,
              "0x19's clip arguments change the raster");
        /* The mirror itself must change the raster, distinguishing 0x09 from
         * the unmirrored RLE cases. */
        blit_ref(ref2, REF_RLE, rle, 9, 1, bank, 0, 0, 0,0,0);
        CHECK(memcmp(ref, ref2, BLIT_BUF) != 0,
              "0x09's mirror changes the raster");

        /* Mode-1 shear (0x04/0x14 unclipped, 0x06/0x16 clipped; 0x5215C). A
         * non-zero DS_00107900 makes the raster differ from a plain copy. */
        DSW(DS_00107900 + 0) = 0;
        DSW(DS_00107900 + 2) = 32;
        DSW(DS_00107900 + 4) = 64;

        blit_node(&s, 0x04, pix, pal, 9, 3, 0,0,0);
        blit_run(got, &s, icon2);
        blit_ref(ref, REF_SHEAR, rle, 9, 3, bank, 0, 0, 0,0,0);
        CHECK(memcmp(got, ref, BLIT_BUF) == 0,
              "0x04 routes to the shear renderer");
        blit_node(&s, 0x14, pix, pal, 9, 3, 0,0,0);
        blit_run(got2, &s, icon2);
        CHECK(memcmp(got2, ref, BLIT_BUF) == 0,
              "0x14 shares the shear raster with 0x04");
        blit_ref(ref2, REF_RAW, rle, 9, 3, bank, 0, 0, 0,0,0);
        CHECK(memcmp(ref, ref2, BLIT_BUF) != 0,
              "non-zero shear table makes 0x04 differ from a copy");

        blit_node(&s, 0x06, pix, pal, 9, 3, 2,1,1);
        blit_run(got, &s, icon2);
        blit_ref(ref, REF_SHEAR, rle, 9, 3, bank, 0, 0, 2,1,1);
        CHECK(memcmp(got, ref, BLIT_BUF) == 0,
              "0x06 routes to clipped shear");
        blit_node(&s, 0x16, pix, pal, 9, 3, 2,1,1);
        blit_run(got2, &s, icon2);
        CHECK(memcmp(got2, ref, BLIT_BUF) == 0,
              "0x16 shares the shear raster with 0x06");

        DSW(DS_00107900 + 0) = 0;
        DSW(DS_00107900 + 2) = 0;
        DSW(DS_00107900 + 4) = 0;
    }
}

int test_sprite(void)
{
    check_node_build();
    check_rle_cross();
    check_bank_and_colour();
    check_raw_copy();
    check_raw_clipped();
    check_rle_clipped();
    check_rle_row_edges();
    check_rle_mirror();
    check_rle_mirror_clip();
    check_shear();
    check_shear_clipped();
    check_blit_dispatch();
    return 0;
}
