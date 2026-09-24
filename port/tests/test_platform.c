/* test_platform.c — the platform suite.
 *
 * Consolidated from: test_mem.c, test_le.c, test_res.c, test_gra.c, test_gfx.c, test_sprite.c, test_render.c, test_input.c, test_host.c, test_rng.c, test_text.c.
 * Every assertion is carried verbatim; only the file's home and the
 * two cross-file static names (none) changed. */

#include "mem.h"
#include "symbols.h"
#include "test.h"
#include "platform/res.h"
#include "platform/gfx.h"
#include "game/flow.h"
#include "platform/gra.h"
#include "platform/sprite.h"
#include "platform/render.h"
#include "platform/input.h"
#include "host.h"
#include "game/rng.h"
#include "game/actors.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <strings.h>
#include <stdint.h>
#include <time.h>


/* ---- test_mem.c ---- */

void fn_probe(void) { }
void fn_probe2(void) { }

int test_mem(void)
{
    int before = g_failures;

    CHECK_EQ_INT(MEM_SIZE, 0x4000000);
    CHECK(mem_in_range(0x80000, 0x8B0D0), "data object range is inside mem[]");
    CHECK(!mem_in_range(MEM_SIZE - 4, 8), "writes past the end are rejected");

    /* DSB offsets include the 0x80000 base: DAT_00080004 is DS:0x0004. */
    DSB(0x80004) = 0xAB;
    CHECK_EQ_INT(mem[0x80004], 0xAB);
    CHECK_EQ_INT(DSB(0x80004), 0xAB);

    DSD(0x80010) = 0x11223344u;
    CHECK_EQ_INT(DSD(0x80010), 0x11223344u);

    mem_fill(0x90000, 0x5A, 4);
    CHECK_EQ_INT(DSB(0x90000), 0x5A);
    CHECK_EQ_INT(DSB(0x90003), 0x5A);
    CHECK_EQ_INT(DSB(0x90004), 0x00);

    /* The macros must be usable as lvalues, not just rvalues. */
    DSW(0x90008) = 0x1234;
    CHECK_EQ_INT(DSW(0x90008), 0x1234);

    /* Code addresses stored in data (process tables, the lock calls in main)
     * must survive a round trip through the flat address space. */
    {
        extern void fn_probe(void);
        extern void fn_probe2(void);
        CHECK(fn_resolve(FN_0002D62C) == NULL, "unregistered address resolves to NULL");
        fn_register(FN_0002D62C, fn_probe);
        CHECK(fn_resolve(FN_0002D62C) == fn_probe, "round trip");
        CHECK_EQ_INT(fn_origin(fn_probe), FN_0002D62C);
        CHECK(fn_resolve(0xDEAD) == NULL, "unknown address is NULL, not garbage");

        /* An unregistered function has origin 0, the sentinel callers test. */
        CHECK_EQ_INT(fn_origin(fn_probe2), 0);

        /* Same address registered twice: the first entry wins and the later
         * registration is ignored, not silently substituted. */
        fn_register(FN_000255CC, fn_probe);
        fn_register(FN_000255CC, fn_probe2);
        CHECK(fn_resolve(FN_000255CC) == fn_probe, "duplicate address keeps first registration");
    }

    return g_failures - before;
}

/* ---- test_le.c ---- */

#define EXE "data/game/C/PRAGE.EXE"

int test_le(void)
{
    int before = g_failures;

    CHECK(mem_load_le(EXE, NULL) == 1, "PRAGE.EXE loads");
    CHECK(mem_load_le("data/game/C/INDEX", NULL) == 0, "a non-LE file is rejected");

    /* The LE header and object table are already verified by tools/le_info.py:
     *   LE header 0x290A4, page size 0x1000, 213 pages,
     *   object 0 code  base 0x10000 size 0x63B15 100 pages
     *   object 1 data  base 0x80000 size 0x8B0D0 113 pages
     * After loading, the data object's string table must be present. These are
     * real bytes from the shipped file (port/decomp/prage.strings.csv lists
     * them at DS:0x0004, 0x0010, 0x001C), so they prove the page map and object
     * mapping are right before any fixup is applied. */
    CHECK(mem_in_range(DATA_BASE, 0x8B0D0), "data object fits");
    CHECK(memcmp(mem + DATA_BASE + 4, "SB16.DIG", 9) == 0,
          "DS:0x0004 is the SB16.DIG string");
    CHECK(memcmp(mem + DATA_BASE + 0x10, "SBPRO.DIG", 10) == 0,
          "DS:0x0010 is the SBPRO.DIG string");
    CHECK(memcmp(mem + DATA_BASE + 0x1C, "SBLASTER.DIG", 13) == 0,
          "DS:0x001C is the SBLASTER.DIG string");

    /* The data object's virtual size (0x8B0D0) exceeds its file-backed pages
     * (113 * 0x1000 == 0x71000). Poison that BSS tail, reload, and require the
     * loader to have zeroed it: this fails if the tail is left to whatever
     * mem[] held before, which a zero-initialised global would disguise. */
    {
        u32 mapped = 113u * 0x1000u;
        mem_fill(DATA_BASE + mapped, 0xFF, 0x8B0D0 - mapped);
        CHECK(mem_load_le(EXE, NULL) == 1, "reload after poisoning the BSS tail");
        int dirty = 0;
        for (u32 i = 0; i < 0x8B0D0 - mapped; i++)
            if (mem[DATA_BASE + mapped + i] != 0) dirty = 1;
        CHECK(!dirty, "data object BSS tail is zero-filled, not stale");
    }
    /* mem[] must equal Ghidra's fixup-applied image of the data object.
     * The oracle is not committed (see Task 3 Step 1), so it is required only
     * when PR_ORACLE_REQUIRED=1 and otherwise skipped with a notice. */
    {
        FILE *g = fopen("port/tests/ghidra_data.bin", "rb");
        if (!g && getenv("PR_ORACLE_REQUIRED")) {
            CHECK(0, "PR_ORACLE_REQUIRED=1 but the Ghidra oracle is missing");
        } else if (!g) {
            printf("SKIP data-object oracle (generate it per Task 3 Step 1, "
                   "or set PR_ORACLE_REQUIRED=1 to require it)\n");
        } else {
            static u8 oracle[0x8B0D0];
            size_t got = fread(oracle, 1, sizeof oracle, g);
            fclose(g);
            CHECK_EQ_INT(got, sizeof oracle);
            int diff = -1;
            for (size_t i = 0; i < sizeof oracle && diff < 0; i++)
                if (mem[DATA_BASE + i] != oracle[i]) diff = (int)i;
            CHECK(diff < 0, "data object matches the Ghidra image");
            if (diff >= 0) printf("  first difference at DS:0x%05x\n", diff);
        }
    }

    return g_failures - before;
}

/* ---- test_res.c ---- */

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
        /* PORT: the loader's text blit is 0x51ED8, whose destination base is the
         * literal VGA aperture 0xA0000 (`add edi, 0xa0000`), not the back buffer
         * the renderer's blit 0x51E5C uses (`add edi, [0x687a4]`). So the text
         * must land in gfx_aperture() and must NOT touch DS_000E87A4. */
        u8 *scratch = gfx_aperture();
        u32 saved_base = DSD(DS_000E87A4);
        u32 saved_row = DSD(DS_001088F8 + 192u * 4u);
        const u32 backbuf = 0x3F60000u;   /* above the resource heap */
        memset(scratch, 0, 0xFA00u);
        mem_fill(backbuf, 0xAAu, 0xFA00u);   /* a sentinel, not the 0 post-state */
        DSD(DS_000E87A4) = backbuf;
        DSD(DS_001088F8 + 192u * 4u) = 192u * 0x140u;
        DSD(DS_001014FC) = 0;
        CHECK(res_resolve(res_handle(3u, 0)) != NULL, "the fresh lazy resolve draws");
        CHECK_EQ_INT((int)DSD(DS_001014FC), 1);
        u32 pixels = 0, rowlo = 200u, rowhi = 0u, collo = 320u, colhi = 0u;
        for (u32 y = 192u; y < 198u; y++) {
            for (u32 x = 0u; x < 320u; x++) {
                if (scratch[y * 320u + x] != 0u) {
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
        /* The back buffer keeps its sentinel: 0x51ED8 never writes it. */
        CHECK_EQ_INT((int)mem[backbuf + 192u * 320u], 0xAA);
        CHECK_EQ_INT((int)mem[backbuf + 197u * 320u + 85u], 0xAA);
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

/* ---- test_gra.c ---- */

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

static void check_gra_sprites(void)
{
    /* The sprite table itself is static in the data object and already
     * resident, but resolving its handles needs the resource INDEX. test_res
     * loads it earlier in the run_tests order; load it here too (guarded, so
     * the bump allocator is never asked for it twice) to make this test
     * independent of that ordering. Needs platform/res.h. */
    if (DSD(DS_001014F0) == 0)
        CHECK(res_load_index("data/game/C", "data/game/C/INDEX") > 0,
              "resource index loads");

    /* The first entry must resolve, and its descriptor must be self-consistent.
     * Its header values are read from the shipped asset (s16statu.gra): 30x27
     * with X pivot 15 and Y pivot 13, positive height => RLE, not the raw
     * marker. */
    GraSprite s;
    u32 dh = 0;
    CHECK_EQ_INT(gra_sprite_lookup(0, &s, &dh), 1);
    CHECK(dh != 0, "sprite 0 has a descriptor handle");
    CHECK_EQ_INT(s.width, 30);
    CHECK_EQ_INT(s.height, 27);
    CHECK_EQ_INT(s.xorg, 15);
    CHECK_EQ_INT(s.yorg, 13);
    const u8 *px = NULL;
    CHECK_EQ_INT(gra_sprite_pixels(s.pixel_handle, &px), 1);
    CHECK(px != NULL, "pixel handle resolves");

    /* A garbage handle must be rejected. */
    CHECK_EQ_INT(gra_sprite_open(0x7FFFFFFFu, &s), 0);
    CHECK_EQ_INT(gra_sprite_pixels(0x7FFFFFFFu, &px), 0);

    /* The table is the documented 18,443 entries, indices 0..18442. The port
     * masks the id to 0x7FFF exactly as 0x14268 does and applies no other
     * bound. The table's end is NOT discoverable from the data — the dwords
     * after it are nonzero but are not handles (the first all-zero dword is at
     * index 18535, and 18534 reads 0x128D) — so the length is pinned from the
     * disassembly, not inferred. */
    int n = 0;
    for (u32 i = 0; i < 18443u; i++)
        if (DSD(DS_000A8B30 + i * 4u) != 0) n++;
    CHECK_EQ_INT(n, 18443);
}

int test_gra(void)
{
    int before = g_failures;

    check_gra_sprites();

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
        u8 pal[4096];
        int cols = -1;
        CHECK(load_at("data/game/C/S16FONTS.GRA", SCRATCH, &len), "fonts loads");
        CHECK(gra_open(SCRATCH, len, c, 4, &n), "fonts chain");
        CHECK(gra_decode_palette(SCRATCH, c, n, pal, sizeof pal, &cols), "fonts palette decodes");
        CHECK_EQ_INT(cols, 27);
        CHECK_EQ_INT(pal[0], 0x3C);
        CHECK_EQ_INT(pal[1], 0x34);
        CHECK_EQ_INT(pal[2], 0x24);

        CHECK(load_at("data/game/C/S16TITLE.GRA", SCRATCH, &len), "title loads");
        CHECK(gra_open(SCRATCH, len, c, 4, &n), "title chain");
        CHECK(gra_decode_palette(SCRATCH, c, n, pal, sizeof pal, &cols), "title palette decodes");
        CHECK_EQ_INT(cols, 720);
    }

    /* The largest bank in the shipped set is S16ATTRC's 1292 colours (3876 B).
     * An exactly-sized buffer succeeds; an undersized buffer is rejected with
     * *count left 0, never written past its capacity. */
    {
        u8 big[1292 * 3];
        u8 tiny[3 * 8];
        int cols = -1;
        CHECK(load_at("data/game/C/S16ATTRC.GRA", SCRATCH, &len), "attrc loads");
        CHECK(gra_open(SCRATCH, len, c, 4, &n), "attrc chain");
        CHECK(gra_decode_palette(SCRATCH, c, n, big, sizeof big, &cols), "largest bank fits");
        CHECK_EQ_INT(cols, 1292);
        CHECK_EQ_INT(gra_decode_palette(SCRATCH, c, n, tiny, sizeof tiny, &cols), 0);
        CHECK_EQ_INT(cols, 0);
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

        /* Sentinel records, by *record index* (the earlier byte offsets 864 and
         * 624 were being misread as indices and only hit the out-of-range
         * guard). S16FONTS has 5 zero-dimension records and S16TITLE has 1
         * negative-dimension record (-320x-200 at index 72); each must be
         * rejected by the sentinel guard. Removing `(s16)w <= 0 || (s16)h <= 0`
         * makes the zero-dimension ones return >= 0, so these tests fail. */
        static const int zero_dim[] = {52, 232, 263, 328, 330};
        CHECK(load_at("data/game/C/S16FONTS.GRA", SCRATCH, &len), "fonts loads a third time");
        CHECK(gra_open(SCRATCH, len, c, 4, &n), "fonts chain a third time");
        for (unsigned k = 0; k < sizeof zero_dim / sizeof zero_dim[0]; k++)
            CHECK_EQ_INT(gra_decode_frame(SCRATCH, c, n, zero_dim[k], px, sizeof px), -1);
        CHECK(load_at("data/game/C/S16TITLE.GRA", SCRATCH, &len), "title loads again");
        CHECK(gra_open(SCRATCH, len, c, 4, &n), "title chain again");
        CHECK_EQ_INT(gra_decode_frame(SCRATCH, c, n, 72, px, sizeof px), -1);
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

/* ---- test_gfx.c ---- */

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

    /* 6-bit VGA truncation: a channel byte >= 0x40 must be masked to 6 bits
     * before the display expansion. (0x50 & 0x3F) = 0x10 -> exp8(0x10) = 0x41;
     * without the mask the full 8-bit channel exp8(0x50) = 0x45, so this pins
     * the truncation. */
    put_record(SCRATCH, 0x30, 1, 0);
    DSD(SCRATCH) = 0x50u << 2;
    gfx_flush_palette();
    CHECK_EQ_INT(gfx_dac[0x30][0], exp8(0x50u & 0x3Fu));
    CHECK(gfx_dac[0x30][0] != exp8(0x50u),
          "the 6-bit VGA truncation is applied");

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

/* ---- test_sprite.c ---- */

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
 * so its formula degenerates to vis-1-c and a bug ignoring the window passes.
 * Here clip_l = 1, clip_r = 2 (vis = 3) and the row is
 * [literal 2][transparent 1][fill 3].
 *
 * The mirrored source window is [clip_r, clip_r + vis), not [clip_l, clip_l +
 * vis). 0x57FFB's entry computes vis = width - clip_l - clip_r and walks the
 * destination backward from dst + vis - 1 (0x58001/0x58008); its three source
 * paths all start the source at the row-relative column clip_r: 0x58090
 * (clip_l != 0, clip_r == 0) reads from the row start with no skip (0x58093),
 * while 0x581D0 (clip_l == 0, clip_r != 0) and 0x582F4 (both) load clip_r into
 * the skip counter (0x581D0/0x582F4 `MOV EDX,[EBP+0x2c]`) and consume that many
 * source columns before drawing vis. The geometric reason: mirroring maps screen
 * column s to source width-1-(s-L), so the screen's right overhang clip_r is the
 * source's left overhang. The window is columns 2,3,4 = {transparent, 0x07,
 * 0x07}; reversed it is {0x07, 0x07, transparent}. The old [clip_l, clip_l+vis)
 * expectation (columns 1,2,3) is corrected here with the addresses above; it is
 * the source of the port's triangle-rendered first fighter
 * (port/src/platform/sprite.c rle_row). */
static void check_rle_mirror_clip(void)
{
    const u8 src[9] = { 0x02, 0x0A, 0x0B, 0xC1, 0x00, 0x83, 0x07,0,0 };
    u8 dst[8]; memset(dst, 0xEE, sizeof dst);
    CHECK_EQ_INT(sprite_render_rle_clipped(src, dst, 6, 1, 8, 0,
                                           1, 2, 0, /*mirror=*/1), 0);
    CHECK_EQ_INT(dst[0], 0x07);
    CHECK_EQ_INT(dst[1], 0x07);
    CHECK_EQ_INT(dst[2], 0xEE);          /* window column 2 is transparent */
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

/* ---- test_render.c ---- */

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

/* 0x389C4 / 0x38A38: the attract scroll/zoom projection. Seed the inputs, run
 * the raw arithmetic, and assert exact words (no tolerance). Every global the
 * two functions touch is saved and restored, including the shear-table
 * entries. The expected values come from the raw disassembly recorded in
 * docs/superpowers/plans/2026-09-19-attract-derivations.md: the raw divides by
 * 2^n through MSVC's `sar`/`shl`/`sbb` idiom, which truncates toward zero
 * (add 2^n-1 when negative), so a plain arithmetic shift is wrong for negative
 * inputs. */
static void check_scroll_projection(void)
{
    u16 s38 = DSW(DS_00107A38), s3a = DSW(DS_00107A3A);
    u16 s3c = DSW(DS_00107A3C), s3e = DSW(DS_00107A3E);
    u16 s40 = DSW(DS_00107A40), s42 = DSW(DS_00107A42);
    u16 s46 = DSW(DS_00107A44 + 2), s48 = DSW(DS_00107A48);
    u16 s4a = DSW(DS_00107A4A), s4c = DSW(DS_00107A4C);
    u16 s4e = DSW(DS_00107A4E), s50 = DSW(DS_00107A50);
    u16 s52 = DSW(DS_00107A52);
    u32 sf0 = DSD(DS_000F0AF0);
    u16 tbl[0x40];
    for (u32 i = 0; i < 0x40; i++) tbl[i] = DSW(DS_00107900 + i * 2u);

    /* Case A (0x389C4, DS_00107A3C < 1): DS_00107A38 = 0x0400 / 64 = 0x0010,
     * then DS_00107A4C = DS_00107A4E = 0x00AB. */
    DSW(DS_00107A3C) = 0;
    DSW(DS_00107A48) = 0x0400;
    DSW(DS_00107A4E) = 0x00AB;
    render_scroll_edge();
    CHECK_EQ_INT((int)DSW(DS_00107A38), 0x0010);
    CHECK_EQ_INT((int)DSW(DS_00107A4C), 0x00AB);

    /* Case B (0x389C4, DS_00107A3C >= 1, early return): the subtrahend is the
     * s16 high word of DSD(DS_00107A3A), i.e. DS_00107A3C = 65, so
     * x = 0 - 65 = -65. The raw's signed /64 gives (-65 + 63) >> 6 = -1 =
     * 0xFFFF; a plain >> 6 would floor to -2 = 0xFFFE. DS_00107A4A (10) >
     * DS_00107A4C (5), the `ja` early return, so 4C stays 5. */
    DSW(DS_00107A3C) = 65;
    DSW(DS_00107A48) = 0;
    DSW(DS_00107A4A) = 10;
    DSW(DS_00107A4C) = 5;
    render_scroll_edge();
    CHECK_EQ_INT((int)DSW(DS_00107A38), 0xFFFF);
    CHECK_EQ_INT((int)DSW(DS_00107A4C), 5);

    /* Case B' (0x389C4, DS_00107A3C >= 1, no early return): DS_00107A4A (3) <=
     * DS_00107A4C (5), so DS_00107A4C = 3. Same x and same 0xFFFF. */
    DSW(DS_00107A4A) = 3;
    DSW(DS_00107A4C) = 5;
    render_scroll_edge();
    CHECK_EQ_INT((int)DSW(DS_00107A38), 0xFFFF);
    CHECK_EQ_INT((int)DSW(DS_00107A4C), 3);

    /* Case C (0x38A38): stride = 0x100 << 8 = 0x10000, step = 0x10000 / 3 =
     * 21845. DS_00107A52 = 4 rows (indices 3,2,1,0), and each table entry is
     * the raw's truncating /256 followed by the arithmetic `sar edx,1`:
     *   i=3 row 0x10000 -> (256)>>1 = 0x0080
     *   i=2 row 0x0AAAB -> (170)>>1 = 0x0055
     *   i=1 row 0x05556 ->  (85)>>1 = 0x002A
     *   i=0 row 0x00001 ->   (0)>>1 = 0x0000
     * filled downward.
     * edge = 0xEE + 3 = 0xF1; rows i=3 (0xF1) and i=2 (0xF0) skip the
     * DS_00107A3E store because their low 16 exceed 0xEF. i=1 (0xEF) writes
     * (0x2A + 0x2B00) / 32 = 0x0159; i=0 (0xEE) writes 0x0158.
     * Tail: after the loop row = 0x10000 - 4*21845 = -21844; minus
     * DS_00107A42 (2) * 21845 = -65534; /256 = -255 = 0xFF01. Then
     * ((-255 >> 1) + 0x41) / 32 = (-128 + 65) / 32 = -1 = 0xFFFF. */
    DSD(DS_000F0AF0) = 0x100u;
    DSW(DS_00107A40) = 3;
    DSW(DS_00107A52) = 4;
    DSW(DS_00107A4C) = 0x00EE;
    DSW(DS_00107A42) = 2;
    DSW(DS_00107A50) = 0x0041;
    DSW(DS_00107A3E) = 0xDEAD;
    render_scroll_fill();
    CHECK_EQ_INT((int)DSW(DS_00107900 + 0), 0x0000);
    CHECK_EQ_INT((int)DSW(DS_00107900 + 2), 0x002A);
    CHECK_EQ_INT((int)DSW(DS_00107900 + 4), 0x0055);
    CHECK_EQ_INT((int)DSW(DS_00107900 + 6), 0x0080);
    CHECK_EQ_INT((int)DSW(DS_00107A3E), 0x0158);
    CHECK_EQ_INT((int)DSW(DS_00107A44 + 2), 0xFF01);
    CHECK_EQ_INT((int)DSW(DS_00107A3A), 0xFFFF);

    /* Case D (0x38A38, every row skips the DS_00107A3E store): one row with
     * edge = 0x100 > 0xEF, so the seeded sentinel must survive. The row value
     * is (0x10000 / 256) >> 1 = 0x0080. */
    DSD(DS_000F0AF0) = 0x100u;
    DSW(DS_00107A40) = 3;
    DSW(DS_00107A52) = 1;
    DSW(DS_00107A4C) = 0x0100;
    DSW(DS_00107A42) = 0;
    DSW(DS_00107A50) = 0x0041;
    DSW(DS_00107A3E) = 0xDEAD;
    render_scroll_fill();
    CHECK_EQ_INT((int)DSW(DS_00107900 + 0), 0x0080);
    CHECK_EQ_INT((int)DSW(DS_00107A3E), 0xDEAD);

    /* Case E: a single row with a NONZERO t (t = (0x10000 / 256) >> 1 = 0x80)
     * and edge = 0 within range writes the shifted t, (0x80 + 0x2B00) / 32 =
     * 0x015C. Case C only observes t == 0 (its last row overwrites the earlier
     * nonzero store), so this pins the shifted-vs-unshifted store. */
    DSD(DS_000F0AF0) = 0x100u;
    DSW(DS_00107A40) = 3;
    DSW(DS_00107A52) = 1;
    DSW(DS_00107A4C) = 0;
    DSW(DS_00107A42) = 0;
    DSW(DS_00107A50) = 0x0041;
    DSW(DS_00107A3E) = 0xDEAD;
    render_scroll_fill();
    CHECK_EQ_INT((int)DSW(DS_00107900 + 0), 0x0080);
    CHECK_EQ_INT((int)DSW(DS_00107A3E), 0x015C);

    DSW(DS_00107A38) = s38; DSW(DS_00107A3A) = s3a;
    DSW(DS_00107A3C) = s3c; DSW(DS_00107A3E) = s3e;
    DSW(DS_00107A40) = s40; DSW(DS_00107A42) = s42;
    DSW(DS_00107A44 + 2) = s46; DSW(DS_00107A48) = s48;
    DSW(DS_00107A4A) = s4a; DSW(DS_00107A4C) = s4c;
    DSW(DS_00107A4E) = s4e; DSW(DS_00107A50) = s50;
    DSW(DS_00107A52) = s52;
    DSD(DS_000F0AF0) = sf0;
    for (u32 i = 0; i < 0x40; i++) DSW(DS_00107900 + i * 2u) = tbl[i];
}

int test_render(void)
{
    check_list_order();
    check_proj_rounding();
    check_offscreen_skip();
    check_layer_modes();
    check_end_to_end();
    check_scroll_projection();
    return 0;
}

/* ---- test_input.c ---- */

/* PORT: scratch linear address inside mem[] the test points the BIOS base
 * (DS_00101514) at, matching game_init()'s GAME_BIOS_BASE. A host pointer would
 * break the mem[] linear-address invariant. */
#define TEST_KEY_BASE 0x3000000u

/* Dequeues one key, failing instead of blocking forever: input_get_key() spins
 * on host_pump() while empty, and the headless suite has no window to produce a
 * key, so a missing key must fail rather than hang. */
static int take(void)
{
    CHECK(input_has_key(), "key present before input_get_key");
    if (!input_has_key()) return -1;
    return (int)input_get_key();
}

int test_input(void)
{
    int before = g_failures;

    input_clear();
    CHECK(!input_has_key(), "cleared queue is empty");
    CHECK_EQ_INT(input_check_key(), 0);

    /* FIFO order and (scan << 8) | ascii packing; check_key() peeks. */
    input_push(0x1E, 'a');
    input_push(0x10, 'Q');
    CHECK(input_has_key(), "has_key after pushes");
    CHECK_EQ_INT(input_check_key(), (0x1E << 8) | 'a');
    CHECK(input_has_key(), "check_key did not consume");
    CHECK_EQ_INT(take(), (0x1E << 8) | 'a');
    CHECK_EQ_INT(take(), (0x10 << 8) | 'Q');
    CHECK(!input_has_key(), "queue drained in FIFO order");

    /* Packing edges: scan 0, ascii 0, both 0xFF. */
    input_push(0x00, 'x');
    input_push(0x48, 0x00);
    input_push(0xFF, 0xFF);
    CHECK_EQ_INT(take(), 0x0078);
    CHECK_EQ_INT(take(), 0x4800);
    CHECK_EQ_INT(take(), 0xFFFF);

    /* input_clear() empties a non-empty queue. */
    input_push(0x1E, 0x1E);
    input_clear();
    CHECK(!input_has_key(), "input_clear() empties the queue");
    CHECK_EQ_INT(input_check_key(), 0);

    /* Overflow drops the OLDEST entry: pushing 0..99 into the 64-entry ring
     * leaves exactly 36..99, in order. Draining the whole window and asserting
     * each value proves which entries survived, not merely that the count
     * capped (a "drop newest" bug would leave 0..63 and fail on the first). */
    input_clear();
    for (int i = 0; i < 100; i++) input_push((u8)i, (u8)(i ^ 0x5A));
    for (int i = 36; i < 100; i++)
        CHECK_EQ_INT(take(), ((i & 0xFF) << 8) | (i ^ 0x5A));
    CHECK(!input_has_key(), "ring held exactly 64 entries");

    {
        /* The input bitfield. 0x50161 is a level/latch selector over the raw
         * level word DAT_000E1C34; 0x4F644 turns it into the two masks the game
         * reads. Host bits enter through the key bitmap at
         * DS_00101514 + 0x2d8/0x2d9, which the test drives directly. */
        u32 saved_base = DSD(DS_00101514);
        u32 saved_30 = DSD(DS_000E1C30);
        u32 saved_34 = DSD(DS_000E1C34);
        u32 saved_38 = DSD(DS_000E1C38);
        u32 saved_3c = DSD(DS_000E1C3C);
        u16 saved_40 = DSW(DS_000E1C40);
        u8 saved_e4[4], saved_d8[4];

        mem_fill(TEST_KEY_BASE, 0, 0x400);
        DSD(DS_00101514) = TEST_KEY_BASE;
        for (u32 i = 0; i < 4u; i++) {
            saved_e4[i] = DSB(DS_001088E4 + i);
            saved_d8[i] = DSB(DS_001088D8 + i);
        }
        DSD(DS_000E1C30) = 0; DSD(DS_000E1C34) = 0; DSD(DS_000E1C38) = 0;
        DSD(DS_000E1C3C) = 0; DSW(DS_000E1C40) = 0;

        /* 0x2D2F0 is a constant 0 (`xor eax,eax; ret`). */
        CHECK_EQ_INT((int)input_joystick_device(0xFFu), 0);

        /* 0x500C4: level word = (byte[+0x2d8] << 24) | (byte[+0x2d9] << 8).
         * The debounce holds a bit's previous level for one frame, so a press
         * needs two samples to appear. */
        DSB(TEST_KEY_BASE + 0x2d9) = 0x01;           /* mask 0x00000100 */
        CHECK_EQ_INT((int)input_pump(), 0);          /* change is debounced */
        CHECK_EQ_INT((int)input_pump(), 0x00000100); /* second sample holds it */

        /* 0x4F644: with the joystick accessor 0 the merge never fires, so
         * DS_001088E4 is the newly-pressed bits and DS_001088D8 the held bits.
         * The press is visible in E4 for exactly one frame. */
        input_state_update();
        CHECK_EQ_INT((int)DSD(DS_001088E4), 0x00000100);
        CHECK_EQ_INT((int)DSD(DS_001088D8), 0x00000100);

        input_state_update();
        CHECK_EQ_INT((int)DSD(DS_001088E4), 0);          /* no longer new */
        CHECK_EQ_INT((int)DSD(DS_001088D8), 0x00000100); /* still held */

        /* Release: the debounce holds the level one more frame, then drops it;
         * both masks clear. E4 is the newly-pressed (rising) edge, so it stays
         * clear through a release. */
        DSB(TEST_KEY_BASE + 0x2d9) = 0;
        CHECK_EQ_INT((int)input_pump(), 0x00000100);  /* still held this frame */
        CHECK_EQ_INT((int)input_pump(), 0);           /* dropped on the second */
        input_state_update();
        CHECK_EQ_INT((int)DSD(DS_001088E4), 0);
        CHECK_EQ_INT((int)DSD(DS_001088D8), 0);

        /* 0x50161 applies its mask to the latch and leaves the unmasked bits of
         * the level word alone. */
        DSD(DS_000E1C34) = 0x0F000000u;
        DSD(DS_000E1C38) = 0;
        CHECK_EQ_INT((int)input_select_bits(0x01000000u), 0x0F000000);
        CHECK_EQ_INT((int)DSD(DS_000E1C38), 0x01000000);
        CHECK_EQ_INT((int)input_select_bits(0x10000000u), 0x0F000000);
        CHECK_EQ_INT((int)DSD(DS_000E1C38), 0x01000000);   /* bit 28 not live yet */
        DSD(DS_000E1C34) = 0x1F000000u;                    /* now bit 28 is live */
        CHECK_EQ_INT((int)input_select_bits(0x10000000u), 0x1F000000);
        CHECK_EQ_INT((int)DSD(DS_000E1C38), 0x11000000);   /* latched, not held */

        DSD(DS_00101514) = saved_base;
        DSD(DS_000E1C30) = saved_30; DSD(DS_000E1C34) = saved_34;
        DSD(DS_000E1C38) = saved_38; DSD(DS_000E1C3C) = saved_3c;
        DSW(DS_000E1C40) = saved_40;
        for (u32 i = 0; i < 4u; i++) {
            DSB(DS_001088E4 + i) = saved_e4[i];
            DSB(DS_001088D8 + i) = saved_d8[i];
        }
    }

    return g_failures - before;
}

/* ---- test_host.c ---- */

static unsigned long long now_ns(void)
{
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (unsigned long long)ts.tv_sec * 1000000000ull +
           (unsigned long long)ts.tv_nsec;
}

int test_host(void)
{
    int before = g_failures;

    /* host_pump()/host_present_rgb()/host_shutdown() before host_init(): the
     * suite runs headless with no window, so all three must be safe no-ops. */
    host_shutdown();
    host_pump();
    host_present_rgb((const u8 *)"\x01\x02\x03", 1, 1);

    /* host_init() reports failure instead of aborting. w <= 0 fails the guard
     * before SDL is touched, so the suite never opens a window. */
    CHECK_EQ_INT(host_init("pr-test", 0, 0), 0);
    host_shutdown();

    /* The real host advances the tick from a clock; the weak no-op left
     * host_tick_count() pinned at 0. host_wait_vblank() sleeps to the next 60 Hz
     * boundary then pumps, so a bounded loop must see the tick move. */
    u32 t0 = host_tick_count();
    unsigned long long deadline = now_ns() + 500ull * 1000000ull;
    while (host_tick_count() == t0 && now_ns() < deadline) host_wait_vblank();
    CHECK(host_tick_count() > t0, "host_wait_vblank advances host_tick_count");
    CHECK(host_tick_count() >= t0, "tick does not go backwards");

    /* A pump with no elapsed interval must not invent ticks (the catch-up clamp
     * must preserve normal pacing). */
    u32 tprev = host_tick_count();
    host_pump();
    CHECK_EQ_INT((int)host_tick_count(), (int)tprev);

    /* File round-trip, over-max rejection, and a missing file. The over-max read
     * must fail cleanly and leave the destination untouched. */
    const char *path = "port_host_test.tmp";
    const u8 out[5] = { 'p', 'r', 'a', 'g', 'e' };
    u8 in[8];
    u32 n = 0;
    CHECK(host_write_file(path, out, sizeof out), "host_write_file succeeds");
    CHECK(host_read_file(path, in, sizeof in, &n), "host_read_file succeeds");
    CHECK_EQ_INT((int)n, (int)sizeof out);
    CHECK(in[0] == 'p' && in[4] == 'e', "file contents round-trip");
    for (size_t i = 0; i < sizeof in; i++) in[i] = 0xAB;
    CHECK(!host_read_file(path, in, 4, &n), "file larger than max fails");
    CHECK(in[0] == 0xAB && in[7] == 0xAB, "over-max read writes nothing");
    CHECK(!host_read_file("no/such/prage/file", in, sizeof in, &n),
          "missing file fails");

    /* A sparse file whose low 32 bits fit `max` must still be rejected: the
     * guard compares the real 64-bit size, not a truncated u32. The old
     * narrowing bug passed the guard here and ran fread(size) into `in`. */
    const char *big = "port_host_big.tmp";
    FILE *bf = fopen(big, "wb");
    if (bf) {
        if (fseek(bf, (long)(4294967296ll + 2 - 1), SEEK_SET) != 0 ||
            fputc(0, bf) == EOF) {
            fclose(bf);
        } else {
            fclose(bf);
            for (size_t i = 0; i < sizeof in; i++) in[i] = 0xCD;
            CHECK(!host_read_file(big, in, 4, &n),
                  "file whose low 32 bits fit max is still rejected");
            CHECK(in[0] == 0xCD && in[7] == 0xCD,
                  "sparse over-size read writes nothing");
        }
        remove(big);
    }
    remove(path);

    /* Audio seam (Task 5). The suite must never open a real device, so every
     * assertion here stays on the closed/no-device path: submit is a no-op and
     * open() of an impossible profile fails through the precondition guard
     * before SDL is touched. */
    host_audio_close();                                  /* before any open */
    CHECK_EQ_INT((int)host_audio_rate(), 0);
    const s16 audio[4] = { 0, 0, 0, 0 };
    host_audio_submit(audio, 2);                         /* no device: no-op */
    host_audio_submit(NULL, 2);
    host_audio_submit(audio, 0);
    host_audio_submit(audio, -1);                        /* negative count */
    CHECK_EQ_INT((int)host_audio_rate(), 0);             /* submits kept seam closed */
    host_audio_close();                                  /* safe after no-op submits */
    CHECK_EQ_INT((int)host_audio_rate(), 0);
    CHECK_EQ_INT(host_audio_open(0, 2), 0);              /* rate <= 0 */
    CHECK_EQ_INT(host_audio_open(44100, 0), 0);          /* channels <= 0 */
    CHECK_EQ_INT(host_audio_open(-44100, -2), 0);
    CHECK_EQ_INT((int)host_audio_rate(), 0);             /* still closed */
    host_audio_close();
    host_audio_close();                                  /* idempotent */
    CHECK_EQ_INT((int)host_audio_rate(), 0);

    /* host_shutdown() tears the audio seam down first, so afterwards every
     * audio entry point must still be a safe no-op reading rate 0. */
    host_shutdown();
    host_audio_submit(audio, 2);
    host_audio_close();
    CHECK_EQ_INT((int)host_audio_rate(), 0);

    return g_failures - before;
}

/* ---- test_rng.c ---- */

int test_rng(void)
{
    /* The recurrence's own outputs, computed independently from
     * state = state*0xB90D12B9 + 0x38CE051F seeded 0xABCD. */
    static const u32 ranges[] = { 0x5Au, 0x7Eu, 2u, 0xFFFFu, 0x10000u, 0u, 0x7FFFFFFFu };
    static const u32 expect[] = { 12u, 111u, 0u, 29617u, 0u, 0u, 11010u };

    rng_seed(0xABCDu);
    CHECK_EQ_INT((int)DSD(DS_000EF6D8), 0xABCD);
    for (unsigned i = 0; i < sizeof ranges / sizeof ranges[0]; i++)
        CHECK_EQ_INT((int)rng_next(ranges[i]), (int)expect[i]);

    /* A range wider than 16 bits is masked, and 0 must not shift undefinedly. */
    rng_seed(0xABCDu);
    CHECK_EQ_INT((int)rng_next(0x7FFFFFFFu), 9158);
    CHECK_EQ_INT((int)rng_next(0u), 0);

    /* rng_step advances the state and discards the value. */
    rng_seed(0xABCDu);
    u32 before = DSD(DS_000EF6D8);
    rng_step();
    CHECK(DSD(DS_000EF6D8) != before, "rng_step advanced the state");
    CHECK_EQ_INT((int)DSD(DS_000EF6D8), (int)(0xABCDu * 0xB90D12B9u + 0x38CE051Fu));

    /* The three title draws, in order, are what Task 1 pins the original to:
     * the immediates 12, 111, 0 in tools/title_pin.py must equal these. */
    rng_seed(0xABCDu);
    CHECK_EQ_INT((int)rng_next(0x5Au), 12);
    CHECK_EQ_INT((int)rng_next(0x7Eu), 111);
    CHECK_EQ_INT((int)rng_next(2u), 0);
    return 0;
}

/* ---- test_text.c ---- */

/* port/tests/test_text.c — the text renderer (Task 8b): 0x2F830 and its
 * 0x2F5A0 glyph emitter. The raw disassembly shows text is not a framebuffer
 * blit: 0x2F5A0 releases the cell's old record through 0x2AD40 and spawns each
 * non-space glyph as an actor through 0x2AE14, storing the record offset in the
 * 43-wide cell grid at DS_00105F38. The glyph's pixels therefore arrive later
 * through the actor display list (0x1C390), so the assertions below check the
 * grid, the spawned glyph's pset sprite id and position, and the cursor extent,
 * all hand-computed from the disassembly and the loaded font tables at
 * DS 0x3D048 / 0x3D1EC / 0x3CD7C / 0x3D38D. */






static u32 grid(s32 row, s32 col)
{
    return DSD(DS_00105F38 + (u32)row * 0xacu + (u32)col * 4u);
}

int test_text(void)
{
    int before = g_failures;
    if (res_count() != 69)
        CHECK(res_load_index("data/game/C", "data/game/C/INDEX") == 69,
              "index loaded");
    CHECK(actors_init() == 1, "actors_init validates the pools");

    /* Mode 3 maps by class: 'A' -> class 10 (table entry sprite 0x3FDD, width
     * 16), 'I' -> class 18 (sprite 0x3FE5, width 8). 0x2F5A0 advances the
     * running column by the glyph width, so "AI" lands at columns 0 and 2; the
     * cursor is advanced by 0x2F830's return (the glyph count), not the pixel
     * width, so its high word is 0 + 2. */
    actors_reset();
    DSD(DS_00105F34) = 0;
    text_cursor_set(0, 0, (const u8 *)"AI", 3);
    u32 ga = grid(0, 0), gi = grid(0, 2);
    CHECK(ga != 0, "A glyph spawned at col 0");
    CHECK(gi != 0, "I glyph spawned at col 2");
    CHECK_EQ_INT((int)grid(0, 1), 0);              /* col 1 skipped: width 16 */
    if (ga != 0) {
        CHECK_EQ_INT((int)(DSW(actor_pset(ga)) & 0x7fffu), 0x3fdd);
        CHECK_EQ_INT((int)DSD(ga + 0x18), 0);      /* x = col 0 * 0x200 */
        CHECK_EQ_INT((int)DSD(ga + 0x1c), 0);      /* y = row 0 * 0x200 */
        CHECK_EQ_INT((int)DSW(ga + 0x40), 0x2000); /* extent 0x80 << 6 */
    }
    if (gi != 0) {
        CHECK_EQ_INT((int)(DSW(actor_pset(gi)) & 0x7fffu), 0x3fe5);
        CHECK_EQ_INT((int)DSD(gi + 0x18), 0x400);  /* x = col 2 * 0x200 */
        CHECK_EQ_INT((int)DSD(gi + 0x1c), 0);
    }
    CHECK_EQ_INT((int)DSW(DS_00105F34), 0);
    CHECK_EQ_INT((int)DSW(DS_00105F34 + 2), 2);

    /* Mode 0 indexes the char table at DS 0x3CD7C (all width 8, advance 1). A
     * space is counted and advances the column but spawns no actor. */
    actors_reset();
    DSD(DS_00105F34) = 0;
    text_cursor_set(0, 0, (const u8 *)"A B", 0);
    u32 g0 = grid(0, 0), g1 = grid(0, 1), g2 = grid(0, 2);
    CHECK(g0 != 0 && g2 != 0, "A and B glyphs");
    CHECK_EQ_INT((int)g1, 0);                      /* space: no actor */
    if (g0 != 0)
        CHECK_EQ_INT((int)(DSW(actor_pset(g0)) & 0x7fffu), 0x3f56);
    if (g2 != 0)
        CHECK_EQ_INT((int)(DSW(actor_pset(g2)) & 0x7fffu), 0x3f57);
    CHECK_EQ_INT((int)DSW(DS_00105F34 + 2), 3);    /* glyph count includes space */

    /* All-spaces takes 0x2F830's early arm: it clears the width-long run of
     * cells through 0x2F280 (releasing their records) and returns 0. */
    actors_reset();
    DSD(DS_00105F34) = 0;
    u32 r0 = actor_spawn((const u32 *)(mem + 0x9AC30u), 0x4840u, 0xE0u,
                         0x1B00u, 0u);
    u32 r1 = actor_spawn((const u32 *)(mem + 0x9AC30u), 0x4840u, 0xE0u,
                         0x1B00u, 0u);
    CHECK(r0 != 0 && r1 != 0, "pre-placed cells");
    DSD(DS_00105F38) = r0;
    DSD(DS_00105F38 + 4u) = r1;
    text_cursor_set(0, 0, (const u8 *)"  ", 0);
    CHECK_EQ_INT((int)DSD(DS_00105F38), 0);
    CHECK_EQ_INT((int)DSD(DS_00105F38 + 4u), 0);
    if (r0 != 0) CHECK_EQ_INT((int)(DSW(r0 + 0x28) & 8u), 8);
    CHECK_EQ_INT((int)DSW(DS_00105F34 + 2), 0);    /* extent 0 */

    /* An occupied cell is released through 0x2AD40 and re-spawned as the glyph:
     * the cell's pset sprite id changes from the pre-placed descriptor's
     * 0x2C11 to the glyph's 0x3F56. */
    actors_reset();
    DSD(DS_00105F34) = 0;
    u32 old = actor_spawn((const u32 *)(mem + 0x9AC30u), 0x4840u, 0xE0u,
                          0x1B00u, 0u);
    CHECK(old != 0, "old cell");
    DSD(DS_00105F38) = old;
    if (old != 0) CHECK_EQ_INT((int)(DSW(actor_pset(old)) & 0x7fffu), 0x2c11);
    text_cursor_set(0, 0, (const u8 *)"A", 0);
    u32 neu = grid(0, 0);
    CHECK(neu != 0, "cell re-spawned");
    if (neu != 0)
        CHECK_EQ_INT((int)(DSW(actor_pset(neu)) & 0x7fffu), 0x3f56);

    /* A negative class (mode 3 '"') makes 0x2F5A0 return 1, so 0x2F830 aborts
     * with 0 and emits nothing. */
    actors_reset();
    DSD(DS_00105F34) = 0;
    text_cursor_set(0, 0, (const u8 *)"\"", 3);
    CHECK_EQ_INT((int)grid(0, 0), 0);
    CHECK_EQ_INT((int)DSW(DS_00105F34 + 2), 0);

    return g_failures - before;
}
