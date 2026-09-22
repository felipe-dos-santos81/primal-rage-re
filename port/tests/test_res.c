#include "platform/res.h"
#include "game/flow.h"
#include "mem.h"
#include "symbols.h"
#include "test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int test_res(void)
{
    int before = g_failures;

    u32 n = res_load_index("data/game/C", "data/game/C/INDEX");
    /* port/FORMATS.md: the shipped C/INDEX has 69 entries. */
    CHECK_EQ_INT(n, 69);
    CHECK_EQ_INT(res_count(), 69);

    /* The table pointer and count live where 0x1B120 puts them. */
    CHECK(mem_in_range(DSD(DS_001014E0), 69 * 20), "table pointer is in range");
    CHECK_EQ_INT(DSD(DS_001014F0), 69);

    /* Every entry got a data block, and DS_001014F8 is the largest size. */
    u32 biggest = 0, have_data = 0;
    for (u32 i = 0; i < res_count(); i++) {
        if (DSD(DSD(DS_001014E0) + i * 20 + 16) != 0) have_data++;
        if (res_size(i) > biggest) biggest = res_size(i);
    }
    CHECK_EQ_INT(have_data, 69);
    CHECK_EQ_INT(DSD(DS_001014F8), biggest);

    /* Names are 12-byte zero-padded ASCII, so compare the first NUL only. */
    int found = 0;
    for (u32 i = 0; i < res_count(); i++) {
        const char *name = res_name(i);
        if (strncmp(name, "s16title.gra", 12) == 0 ||
            strncmp(name, "S16TITLE.GRA", 12) == 0) {
            found = 1;
            /* The size field must equal the on-disk file size. */
            char path[256];
            snprintf(path, sizeof path, "data/game/C/%.*s", 12, name);
            FILE *f = fopen(path, "rb");
            CHECK(f != NULL, "resource file opens");
            if (f) {
                fseek(f, 0, SEEK_END);
                CHECK_EQ_INT(res_size(i), ftell(f));
                fclose(f);
            }
            /* Its bytes were actually read, and a GRA begins with the chunk
             * header: u16 type = 2, then the magic "43". */
            u8 *data = res_resolve(res_handle(i, 0));
            CHECK(data != NULL, "handle resolves to the resource data");
            if (data) {
                CHECK_EQ_INT(data[0], 2);
                CHECK_EQ_INT(data[1], 0);
                CHECK_EQ_INT(data[2], '4');
                CHECK_EQ_INT(data[3], '3');
                /* Offset arithmetic: handle low 23 bits index into the data. */
                CHECK_EQ_INT(res_resolve(res_handle(i, 4)), data + 4);
            }
        }
    }
    CHECK(found, "s16title.gra is in the index");

    /* The loader's residency state (0x1B544/0x1B3AC). The shipped INDEX
     * preloads only s16fonts (1) and s16statu (2); every other entry is read
     * on first resolve, which is when 0x1B3AC presents the `- LOADING -`
     * screen and sets the master loop's full-copy flag DS_001014FC. The flag
     * is seeded to 0 before each step so a missing presentation cannot pass on
     * a stale value. */
    {
        u32 table = DSD(DS_001014E0);
        /* The INDEX's own preload byte: 0x01 vs 0x02. */
        CHECK_EQ_INT((int)(res_flags(1) & 1u), 1);
        CHECK_EQ_INT((int)(res_flags(2) & 1u), 1);
        CHECK_EQ_INT((int)(res_flags(0) & 1u), 0);

        /* A preloaded entry is marked read by the init walk (0x1B47A) and its
         * resolve presents nothing (0x1B569's resident arm). */
        CHECK((DSD(table + 1u * 20u + 12u) & 0x20000000u) != 0u,
              "preloaded entry marked read at init");
        DSD(DS_001014FC) = 0;
        CHECK(res_resolve(res_handle(1u, 0)) != NULL, "s16fonts resolves");
        CHECK_EQ_INT((int)DSD(DS_001014FC), 0);

        /* A lazy entry stays unread until its first resolve: that resolve
         * presents (0x1B3AC's head) and marks it read (0x1B47A). The read's
         * stall advances DS_00101508 by bytes/117882 (record 9.6) and its tail
         * re-syncs DS_0010150C to it (0x1B45F/0x1B464), so the master loop's
         * gate (0x25643) passes on the load frame. Seed the pair to different
         * sentinels: a missing stall leaves 1508 at 0x5678, a missing re-sync
         * leaves 150C at 0x1234. */
        CHECK((DSD(table + 0u * 20u + 12u) & 0x20000000u) == 0u,
              "lazy entry unread after init");
        DSD(DS_001014FC) = 0;
        DSD(DS_00101508) = 0x5678;
        DSD(DS_0010150C) = 0x1234;
        CHECK(res_resolve(res_handle(0u, 0)) != NULL, "s16slabs resolves");
        CHECK_EQ_INT((int)DSD(DS_001014FC), 1);
        /* The exact delta, not merely "advanced": the stall is
         * ceil(res_size(0)/RES_READ_BYTES_PER_TICK) ticks (res.c). The rate is
         * asserted against its literal first — the delta expression below is
         * rate-relative, so without this a changed rate would move both sides
         * together and the pin would be vacuous. res_size(0) is read from the
         * shipped INDEX, so the assertion tracks the real payload. */
        CHECK_EQ_INT((int)RES_READ_BYTES_PER_TICK, 117882);
        CHECK_EQ_INT((int)DSD(DS_00101508),
                     0x5678 + (int)((res_size(0u) + RES_READ_BYTES_PER_TICK - 1u)
                                    / RES_READ_BYTES_PER_TICK));
        CHECK_EQ_INT((int)DSD(DS_0010150C), (int)DSD(DS_00101508));
        CHECK((DSD(table + 0u * 20u + 12u) & 0x20000000u) != 0u,
              "lazy entry marked read by its first resolve");

        /* The second resolve presents nothing (0x1B57F/0x1B585). */
        DSD(DS_001014FC) = 0;
        CHECK(res_resolve(res_handle(0u, 4)) != NULL, "s16slabs resolves again");
        CHECK_EQ_INT((int)DSD(DS_001014FC), 0);
    }

    /* The loader's presentation pixels. String 489 decodes to `- LOADING -`
     * and 0x1C65C/0x1C5E8 blit it at (0,192) — the expression 0x1B3AC uses is
     * game_string_get(0x1E9) with seeds (0, 0xE6). The composite buffer is
     * pointed at a zeroed scratch and the row-192 offset built, so the first
     * resolve of a fresh lazy entry (index 3 s16glife) draws where the check
     * can see it. The capture's overlay is 166 non-zero pixels at rows
     * 192..197, columns 0..85; the box is asserted, not just the count, so a
     * string-id, font or position regression cannot pass. */
    game_string_table_load("data/game/C");
    CHECK(strcmp((const char *)game_string_get(0x1E9u), "- LOADING -") == 0,
          "string 489 decodes to - LOADING -");
    {
        const u32 scratch = 0x3F50000u;   /* above the resource heap */
        u32 saved_base = DSD(DS_000E87A4);
        u32 saved_row = DSD(DS_001088F8 + 192u * 4u);
        mem_fill(scratch, 0, 0xFA00u);
        DSD(DS_000E87A4) = scratch;
        DSD(DS_001088F8 + 192u * 4u) = 192u * 0x140u;
        DSD(DS_001014FC) = 0;
        CHECK(res_resolve(res_handle(3u, 0)) != NULL, "the fresh lazy resolve draws");
        CHECK_EQ_INT((int)DSD(DS_001014FC), 1);
        u32 pixels = 0, rowlo = 200u, rowhi = 0u, collo = 320u, colhi = 0u;
        for (u32 y = 192u; y < 198u; y++) {
            for (u32 x = 0u; x < 320u; x++) {
                if (mem[scratch + y * 320u + x] != 0u) {
                    pixels++;
                    if (y < rowlo) rowlo = y;
                    if (y > rowhi) rowhi = y;
                    if (x < collo) collo = x;
                    if (x > colhi) colhi = x;
                }
            }
        }
        CHECK_EQ_INT((int)pixels, 166);
        CHECK_EQ_INT((int)rowlo, 192);
        CHECK_EQ_INT((int)rowhi, 197);
        CHECK_EQ_INT((int)collo, 0);
        CHECK_EQ_INT((int)colhi, 85);
        DSD(DS_000E87A4) = saved_base;
        DSD(DS_001088F8 + 192u * 4u) = saved_row;
    }

    CHECK(res_resolve(0xFFFFFFFFu) == NULL, "an out-of-range handle resolves to NULL");

    /* The Smacker movies are not in INDEX, so they load by name. The shipped
     * files are uppercase (TWI5.SMK) and the callers use lowercase, so this
     * exercises the case-insensitive scan. */
    u32 off = 0, size = 0;
    CHECK_EQ_INT(res_load_file("data/game/C", "twi5.smk", &off, &size), 1);
    CHECK_EQ_INT(size, 1208576);
    CHECK(mem_in_range(off, size), "movie bytes got a block in mem[]");
    CHECK_EQ_INT(mem[off], 'S');
    CHECK_EQ_INT(mem[off + 1], 'M');
    CHECK_EQ_INT(mem[off + 2], 'K');
    CHECK_EQ_INT(res_load_file("data/game/C", "twg.smk", &off, &size), 1);
    CHECK_EQ_INT(size, 31048);

    /* A missing file fails without touching either output. */
    u32 miss_off = 0xDEADBEEFu, miss_size = 0xFEEDFACEu;
    CHECK_EQ_INT(res_load_file("data/game/C", "nope.smk", &miss_off, &miss_size), 0);
    CHECK_EQ_INT(miss_off, 0xDEADBEEFu);
    CHECK_EQ_INT(miss_size, 0xFEEDFACEu);

    /* Size boundaries, both rejected with the outputs untouched. A sparse file
     * gives the length with no large fixture on disk. 128 MiB is above MEM_SIZE,
     * so the allocator's bound is hit before any read. 4 GiB + 1 is the u32
     * truncation case that used to pass that bound (the low 32 bits are 0) and
     * then over-read past mem[]; the size is now rejected before allocating, so
     * no read happens — if that guard regresses this test faults rather than
     * silently corrupting the flat space. */
    char big_path[] = "/tmp/pr_resbig_XXXXXX";
    int big_fd = mkstemp(big_path);
    CHECK(big_fd >= 0, "sparse fixture created");
    if (big_fd >= 0) {
        u32 big_off = 0xDEADBEEFu, big_size = 0xFEEDFACEu;
        CHECK_EQ_INT(ftruncate(big_fd, 0x8000000), 0);
        CHECK_EQ_INT(res_load_file("/tmp", big_path + 5, &big_off, &big_size), 0);
        CHECK_EQ_INT(big_off, 0xDEADBEEFu);
        CHECK_EQ_INT(big_size, 0xFEEDFACEu);

        big_off = 0xDEADBEEFu, big_size = 0xFEEDFACEu;
        CHECK_EQ_INT(ftruncate(big_fd, 0x100000000L), 0);
        CHECK_EQ_INT(res_load_file("/tmp", big_path + 5, &big_off, &big_size), 0);
        CHECK_EQ_INT(big_off, 0xDEADBEEFu);
        CHECK_EQ_INT(big_size, 0xFEEDFACEu);

        close(big_fd);
        unlink(big_path);
    }

    return g_failures - before;
}
