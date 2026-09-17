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

int test_gfx(void)
{
    int before = g_failures;

    /* Raw-pointer record: R/G/B field order, first-index placement, count
     * discrimination (only the addressed entries are written), head reset, and
     * consumed marking. */
    put_record(SCRATCH, 5, 1, 0);
    u32 word = (0x40u << 2) | (0x80u << 10) | (0xC0u << 18);
    DSD(SCRATCH) = word;
    DSD(SCRATCH + 4) = (0x11u << 2) | (0x22u << 10) | (0x33u << 18);
    gfx_flush_palette();
    CHECK_EQ_INT(gfx_dac[5][0], 0x40);
    CHECK_EQ_INT(gfx_dac[5][1], 0x80);
    CHECK_EQ_INT(gfx_dac[5][2], 0xC0);
    CHECK(gfx_dac[4][0] == 0, "only the addressed DAC entry was written");
    CHECK_EQ_INT(DSD(HEAD), REC);            /* head reset to base */
    CHECK_EQ_INT(DSD(REC + 4), 0xFFFFFFFFu); /* record marked consumed */

    /* count=2 writes both addressed entries, and nothing past count. */
    put_record(SCRATCH, 5, 2, 0);
    DSD(SCRATCH) = word;
    DSD(SCRATCH + 4) = (0x11u << 2) | (0x22u << 10) | (0x33u << 18);
    gfx_flush_palette();
    CHECK_EQ_INT(gfx_dac[5][0], 0x40);
    CHECK_EQ_INT(gfx_dac[6][0], 0x11);
    CHECK_EQ_INT(gfx_dac[6][1], 0x22);
    CHECK_EQ_INT(gfx_dac[6][2], 0x33);
    CHECK(gfx_dac[7][0] == 0, "count bounds the write");

    /* Clamp: first 0xFE + count 8 overruns the DAC. Without the clamp the index
     * wraps and entry 0/1 get written; with it only 0xFE/0xFF are. */
    gfx_dac[0][0] = 0;
    gfx_dac[1][0] = 0;
    put_record(SCRATCH, 0xFE, 8, 0);
    for (int i = 0; i < 8; i++) DSD(SCRATCH + (u32)i * 4) = (u32)(0x10 + i) << 2;
    gfx_flush_palette();
    CHECK_EQ_INT(gfx_dac[0xFE][0], 0x10);
    CHECK_EQ_INT(gfx_dac[0xFF][0], 0x11);
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
            CHECK_EQ_INT(gfx_dac[0x10][0], 0x21);
            CHECK_EQ_INT(gfx_dac[0x10][1], 0x22);
            CHECK_EQ_INT(gfx_dac[0x10][2], 0x23);
            CHECK_EQ_INT(gfx_dac[0x11][0], 0x31);
            CHECK_EQ_INT(gfx_dac[0x11][1], 0x32);
            CHECK_EQ_INT(gfx_dac[0x11][2], 0x33);
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

    /* Task 15: the title-screen index comparison against the independent Python
     * decoder. port/tests/s16title_frame10.idx is the raw 320x200 8-bit index
     * buffer tools/gra_render.py decodes for S16TITLE frame 10 — the first image
     * the port's title player presents, so --check frame_0001.idx is this same
     * buffer at runtime. It is generated locally and git-ignored like
     * ghidra_data.bin, and is required only when PR_ORACLE_REQUIRED=1.
     *
     * This is the comparison that can be exact: both sides are index buffers
     * from two independent decoders, and all four full-screen title frames are
     * fully opaque (no transparent run and no opaque index 0), so the port's
     * index-0-for-transparent model and its flat-palette choice cannot mask a
     * difference. The PPM/RGB check and the emulator capture cannot pin the
     * indices and are NOT asserted here (see the Task 15 report). */
    {
        FILE *of = fopen("port/tests/s16title_frame10.idx", "rb");
        if (!of && getenv("PR_ORACLE_REQUIRED")) {
            CHECK(0, "PR_ORACLE_REQUIRED=1 but s16title_frame10.idx is missing");
        } else if (!of) {
            printf("SKIP title index oracle (generate it with tools/gra_render.py "
                   "S16TITLE.GRA 2 out.ppm --frame 10 --indices "
                   "port/tests/s16title_frame10.idx, or set PR_ORACLE_REQUIRED=1 "
                   "to require it)\n");
        } else {
            static u8 want[320 * 200];
            size_t nread = fread(want, 1, sizeof want, of);
            fclose(of);
            CHECK_EQ_INT(nread, sizeof want);

            /* INDEX names are lowercase and not NUL-terminated at 12 chars. */
            u32 idx = res_count();   /* sentinel: not found */
            for (u32 i = 0; i < res_count(); i++)
                if (strncasecmp(res_name(i), "s16title.gra", 12) == 0) { idx = i; break; }
            CHECK(idx < res_count(), "s16title.gra is in the INDEX");
            if (idx < res_count()) {
                u32 off = (u32)((const u8 *)res_resolve(res_handle(idx, 0)) - mem);
                GraChunk ch[8];
                int n = 0;
                CHECK(gra_open(off, res_size(idx), ch, 8, &n), "S16TITLE chain");
                static u8 got_idx[320 * 200];
                int used = gra_decode_frame(off, ch, n, 10, got_idx, sizeof got_idx);
                CHECK(used > 0, "S16TITLE frame 10 decodes");
                CHECK(nread == sizeof want && memcmp(got_idx, want, sizeof want) == 0,
                      "title frame 10 index buffer is byte-identical to the "
                      "independent Python decoder");
            }

            /* If a --check run left frame_0001.idx in the CWD, the live capture
             * must be that same buffer: this pins the runtime path, not just the
             * decoder. Optional because a standalone suite run has no frames;
             * when the artifact is present it is a real assertion. */
            FILE *fr = fopen("frame_0001.idx", "rb");
            if (fr) {
                static u8 runtime_idx[320 * 200];
                size_t rn = fread(runtime_idx, 1, sizeof runtime_idx, fr);
                fclose(fr);
                CHECK(rn == sizeof want &&
                      memcmp(runtime_idx, want, sizeof want) == 0,
                      "--check frame_0001.idx is byte-identical to the independent "
                      "Python decoder");
            }
        }
    }

    return g_failures - before;
}
