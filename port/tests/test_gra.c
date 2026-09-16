#include "platform/gra.h"
#include "mem.h"
#include "test.h"
#include <stdio.h>

/* Scratch base for GRA images. The plan sketched 0xE00000, but that is NOT
 * clear of the data: after res_load_index the resource heap runs from
 * 0x10B0D0 up to about 0x2A8C548 (~44.6 MB), so a GRA loaded at 0xE00000
 * would land on top of loaded resources. Start above the heap instead. */
#define SCRATCH 0x3000000u

static int load_at(const char *path, u32 at, u32 *len_out)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (!mem_in_range(at, (u32)sz)) { fclose(f); return 0; }
    size_t got = fread(mem + at, 1, (size_t)sz, f);
    fclose(f);
    *len_out = (u32)sz;
    return got == (size_t)sz;
}

static void check_chain(const char *name, int want_count, const u16 *types,
                        const u32 *lens)
{
    char path[256];
    snprintf(path, sizeof path, "data/game/C/%s", name);
    u32 len = 0;
    CHECK(load_at(path, SCRATCH, &len), "gra file loads");
    GraChunk chunks[8];
    int count = -1;
    CHECK(gra_open(SCRATCH, len, chunks, 8, &count), "valid chain accepted");
    CHECK_EQ_INT(count, want_count);
    for (int i = 0; i < want_count && i < count; i++) {
        CHECK_EQ_INT(chunks[i].type, types[i]);
        CHECK_EQ_INT(chunks[i].body_len, lens[i]);
        CHECK_EQ_INT(chunks[i].body_off, chunks[i].off + 8);
    }
}

static void put_header(u32 at, u16 type, const char magic[2], u32 next)
{
    DSW(at) = type;
    DSB(at + 2) = (u8)magic[0];
    DSB(at + 3) = (u8)magic[1];
    DSD(at + 4) = next;
}

int test_gra(void)
{
    int before = g_failures;

    check_chain("S16FONTS.GRA", 3, (const u16[]){2, 5, 6},
                (const u32[]){39244, 144, 4644});
    check_chain("S16CAGE.GRA", 2, (const u16[]){2, 6},
                (const u32[]){187712, 324});
    check_chain("S16TITLE.GRA", 3, (const u16[]){2, 5, 6},
                (const u32[]){1501656, 2928, 1380});
    check_chain("S16COBSD.GRA", 1, (const u16[]){2},
                (const u32[]){145427});

    /* Malformed chains must be rejected rather than walked out of bounds. */
    GraChunk c[4];
    int n = -1;
    mem_fill(SCRATCH, 0, 64);
    put_header(SCRATCH, 2, "44", 0);
    CHECK_EQ_INT(gra_open(SCRATCH, 64, c, 4, &n), 0);   /* bad magic */

    put_header(SCRATCH, 2, "43", 16);
    put_header(SCRATCH + 16, 6, "43", 16);
    CHECK_EQ_INT(gra_open(SCRATCH, 64, c, 4, &n), 0);   /* next not increasing */

    put_header(SCRATCH, 2, "43", 1000);
    CHECK_EQ_INT(gra_open(SCRATCH, 64, c, 4, &n), 0);   /* next past file */

    put_header(SCRATCH, 2, "43", 0);
    CHECK_EQ_INT(gra_open(SCRATCH, 4, c, 4, &n), 0);    /* header truncated */

    /* `max` caps output; the walk stops without overrunning `out`. */
    u32 len = 0;
    CHECK(load_at("data/game/C/S16FONTS.GRA", SCRATCH, &len), "gra loads for cap test");
    CHECK_EQ_INT(gra_open(SCRATCH, len, c, 2, &n), 1);
    CHECK_EQ_INT(n, 2);

    return g_failures - before;
}
