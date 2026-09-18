#include "platform/gfx.h"
#include "platform/gra.h"
#include "platform/res.h"
#include "mem.h"
#include "test.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* Scratch colour buffer for raw-pointer records, clear of the resource heap
 * (0x10B0D0 .. ~0x2A8C548), same as the GRA test. */
#define SCRATCH 0x3000000u
#define REC   0x107498u
#define HEAD  0x107798u

/* Builds exactly one dirty record and points the head at its end. */
static void put_record(u32 ptr, u32 first, u32 count, u32 flag)
{
    DSD(REC + 0) = ptr;
    DSD(REC + 4) = first;
    DSD(REC + 8) = count;
    DSD(REC + 12) = flag;
    DSD(HEAD) = REC + 16;
}

/* The 6-bit VGA DAC channel expanded to the 8-bit value gfx_dac holds, exactly
 * as gfx_flush_palette does (and as a VGA/DOSBox renders it). */
static u8 exp8(u8 v6) { return (u8)((v6 << 2) | (v6 >> 4)); }

int test_gfx(void)
{
    int before = g_failures;

    /* Raw-pointer record: R/G/B field order, first-index placement, count
     * discrimination (only the addressed entries are written), head reset, and
     * consumed marking. */
    put_record(SCRATCH, 5, 1, 0);
    u32 word = (0x10u << 2) | (0x20u << 10) | (0x30u << 18);
    DSD(SCRATCH) = word;
    DSD(SCRATCH + 4) = (0x11u << 2) | (0x22u << 10) | (0x33u << 18);
    gfx_flush_palette();
    CHECK_EQ_INT(gfx_dac[5][0], exp8(0x10));
    CHECK_EQ_INT(gfx_dac[5][1], exp8(0x20));
    CHECK_EQ_INT(gfx_dac[5][2], exp8(0x30));
    CHECK(gfx_dac[4][0] == 0, "only the addressed DAC entry was written");
    CHECK_EQ_INT(DSD(HEAD), REC);            /* head reset to base */
    CHECK_EQ_INT(DSD(REC + 4), 0xFFFFFFFFu); /* record marked consumed */

    /* count=2 writes both addressed entries, and nothing past count. */
    put_record(SCRATCH, 5, 2, 0);
    DSD(SCRATCH) = word;
    DSD(SCRATCH + 4) = (0x11u << 2) | (0x22u << 10) | (0x33u << 18);
    gfx_flush_palette();
    CHECK_EQ_INT(gfx_dac[5][0], exp8(0x10));
    CHECK_EQ_INT(gfx_dac[6][0], exp8(0x11));
    CHECK_EQ_INT(gfx_dac[6][1], exp8(0x22));
    CHECK_EQ_INT(gfx_dac[6][2], exp8(0x33));
    CHECK(gfx_dac[7][0] == 0, "count bounds the write");

    /* Clamp: first 0xFE + count 8 overruns the DAC. Without the clamp the index
     * wraps and entry 0/1 get written; with it only 0xFE/0xFF are. */
    gfx_dac[0][0] = 0;
    gfx_dac[1][0] = 0;
    put_record(SCRATCH, 0xFE, 8, 0);
    for (int i = 0; i < 8; i++) DSD(SCRATCH + (u32)i * 4) = (u32)(0x10 + i) << 2;
    gfx_flush_palette();
    CHECK_EQ_INT(gfx_dac[0xFE][0], exp8(0x10));
    CHECK_EQ_INT(gfx_dac[0xFF][0], exp8(0x11));
    CHECK_EQ_INT(gfx_dac[0][0], 0);   /* no wrap past the table end */
    CHECK_EQ_INT(gfx_dac[1][0], 0);

    /* Handle path: a non-zero flag byte in [3] makes [0] a resource handle;
     * gfx_flush_palette resolves it and skips the bank's u32 colour count at
     * +4. Re-basing the resolved host pointer instead of its offset reads the
     * wrong address and fails these assertions. */
    {
        u32 ridx = 0;
        while (ridx < res_count() &&
               !(res_resolve(res_handle(ridx, 0)) && res_size(ridx) >= 16))
            ridx++;
        CHECK(ridx < res_count(), "a resource large enough for the handle path");
        if (ridx < res_count()) {
            u32 off = (u32)((const u8 *)res_resolve(res_handle(ridx, 0)) - mem);
            DSD(off + 4) = (0x21u << 2) | (0x22u << 10) | (0x23u << 18);
            DSD(off + 8) = (0x31u << 2) | (0x32u << 10) | (0x33u << 18);
            put_record(res_handle(ridx, 0), 0x10, 2, 1);
            gfx_flush_palette();
            CHECK_EQ_INT(gfx_dac[0x10][0], exp8(0x21));
            CHECK_EQ_INT(gfx_dac[0x10][1], exp8(0x22));
            CHECK_EQ_INT(gfx_dac[0x10][2], exp8(0x23));
            CHECK_EQ_INT(gfx_dac[0x11][0], exp8(0x31));
            CHECK_EQ_INT(gfx_dac[0x11][1], exp8(0x32));
            CHECK_EQ_INT(gfx_dac[0x11][2], exp8(0x33));
        }
    }

    /* Unresolvable handle (index 0x1FF is past the 69-entry table): the record
     * is skipped, not read through NULL, and still marked consumed. */
    gfx_dac[0x20][0] = 0;
    put_record(0xFFFFFFFFu, 0x20, 1, 1);
    gfx_flush_palette();
    CHECK_EQ_INT(gfx_dac[0x20][0], 0);
    CHECK_EQ_INT(DSD(REC + 4), 0xFFFFFFFFu);
    CHECK_EQ_INT(DSD(HEAD), REC);

    return g_failures - before;
}
