#include "platform/gra.h"
#include "mem.h"
#include "test.h"
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

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

    put_header(SCRATCH, 2, "43", 4);
    CHECK_EQ_INT(gra_open(SCRATCH, 64, c, 4, &n), 0);   /* next inside header */
    CHECK_EQ_INT(n, 0);                                 /* count zeroed on failure */

    CHECK_EQ_INT(gra_open(MEM_SIZE - 4, 64, c, 4, &n), 0); /* file off out of mem */
    CHECK_EQ_INT(n, 0);

    /* `max` caps output; the walk stops without overrunning `out`. */
    u32 len = 0;
    CHECK(load_at("data/game/C/S16FONTS.GRA", SCRATCH, &len), "gra loads for cap test");
    CHECK_EQ_INT(gra_open(SCRATCH, len, c, 2, &n), 1);
    CHECK_EQ_INT(n, 2);

    /* Palette bank: S16FONTS is 27 colours over a 144-byte type-5 body, and
     * the first colour word 0x0090D0F0 packs to (0x3C, 0x34, 0x24). */
    {
        u8 pal[3 * 1024];
        int cols = -1;
        CHECK(load_at("data/game/C/S16FONTS.GRA", SCRATCH, &len), "fonts loads");
        CHECK(gra_open(SCRATCH, len, c, 4, &n), "fonts chain");
        CHECK(gra_decode_palette(SCRATCH, c, n, pal, &cols), "fonts palette decodes");
        CHECK_EQ_INT(cols, 27);
        CHECK_EQ_INT(pal[0], 0x3C);
        CHECK_EQ_INT(pal[1], 0x34);
        CHECK_EQ_INT(pal[2], 0x24);

        CHECK(load_at("data/game/C/S16TITLE.GRA", SCRATCH, &len), "title loads");
        CHECK(gra_open(SCRATCH, len, c, 4, &n), "title chain");
        CHECK(gra_decode_palette(SCRATCH, c, n, pal, &cols), "title palette decodes");
        CHECK_EQ_INT(cols, 720);
    }

    /* A single frame: FONTS frame 0 is 13x7, consumes 92 RLE bytes and has
     * 88 of 91 pixels opaque (Task 8's measured index model). Transparent
     * pixels are written as index 0, so counting non-zero bytes is the
     * opaque count — the reconciliation of indices vs. the RGB PPM oracle. */
    {
        u8 px[64 * 64];
        CHECK(load_at("data/game/C/S16FONTS.GRA", SCRATCH, &len), "fonts loads again");
        CHECK(gra_open(SCRATCH, len, c, 4, &n), "fonts chain again");
        int used = gra_decode_frame(SCRATCH, c, n, 0, px, sizeof px);
        CHECK_EQ_INT(used, 92);
        int opaque = 0;
        for (int i = 0; i < 13 * 7; i++) if (px[i]) opaque++;
        CHECK_EQ_INT(opaque, 88);

        /* Sentinel records must be rejected, not decoded into a huge length. */
        CHECK(load_at("data/game/C/S16TITLE.GRA", SCRATCH, &len), "title loads again");
        CHECK(gra_open(SCRATCH, len, c, 4, &n), "title chain again");
        CHECK_EQ_INT(gra_decode_frame(SCRATCH, c, n, 864, px, sizeof px), -1);
        CHECK(load_at("data/game/C/S16FONTS.GRA", SCRATCH, &len), "fonts loads a third time");
        CHECK(gra_open(SCRATCH, len, c, 4, &n), "fonts chain a third time");
        CHECK_EQ_INT(gra_decode_frame(SCRATCH, c, n, 624, px, sizeof px), -1);
    }

    /* Primary assertion: exact consumption across all 69 shipped files. Every
     * positive-dimension descriptor with a strictly greater sprite offset in
     * its file must RLE-decode to exactly that next offset. This is the count
     * Task 8 measured in Python: 18,201 of 18,202, the one miss being
     * S16TITLE frame 113 (74x167 at 0x349D7). */
    {
        static u8 dst[800 * 700];
        int exact = 0, total = 0, zero = 0, neg = 0, nonext = 0, errors = 0;
        int miss_seen = 0;
        DIR *d = opendir("data/game/C");
        CHECK(d != NULL, "game dir opens");
        struct dirent *ent;
        while (d && (ent = readdir(d)) != NULL) {
            size_t l = strlen(ent->d_name);
            if (l < 4 || strcasecmp(ent->d_name + l - 4, ".GRA") != 0) continue;
            char path[300];
            snprintf(path, sizeof path, "data/game/C/%s", ent->d_name);
            if (!load_at(path, SCRATCH, &len) || !gra_open(SCRATCH, len, c, 4, &n) || n <= 0)
                continue;
            int k6 = -1;
            for (int i = 0; i < n; i++) if (c[i].type == 6) k6 = i;
            if (k6 < 0) continue;
            u32 recs = c[k6].body_len / 12;
            for (u32 i = 0; i < recs; i++) {
                u32 at = SCRATCH + c[k6].body_off + i * 12;
                u16 w = DSW(at), h = DSW(at + 2);
                u32 off = DSD(at + 8) & 0x7FFFFFu;
                if (w == 0 || (w & 0x8000) || (h & 0x8000)) {
                    if (w == 0) zero++; else neg++;
                    CHECK_EQ_INT(gra_decode_frame(SCRATCH, c, n, (int)i, dst,
                                                  sizeof dst), -1);
                    continue;
                }
                u32 next = 0;
                for (u32 j = 0; j < recs; j++) {
                    u32 oj = DSD(SCRATCH + c[k6].body_off + j * 12 + 8) & 0x7FFFFFu;
                    if (oj > off && (next == 0 || oj < next)) next = oj;
                }
                if (next == 0) { nonext++; continue; }
                total++;
                int used = gra_decode_frame(SCRATCH, c, n, (int)i, dst, sizeof dst);
                if (used < 0) { errors++; continue; }
                if ((u32)used == next - off) exact++;
                else if (strcasecmp(ent->d_name, "S16TITLE.GRA") == 0 &&
                         w == 74 && h == 167 && off == 0x349D7) miss_seen = 1;
            }
        }
        if (d) closedir(d);
        CHECK_EQ_INT(zero, 17);
        CHECK_EQ_INT(neg, 10);
        CHECK_EQ_INT(nonext, 60);
        CHECK_EQ_INT(errors, 0);
        CHECK_EQ_INT(total, 18202);
        CHECK_EQ_INT(exact, 18201);
        CHECK(miss_seen, "the single mismatch is the documented S16TITLE record");
    }

    return g_failures - before;
}
