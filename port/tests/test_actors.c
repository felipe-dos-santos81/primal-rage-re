/* port/tests/test_actors.c */
#include "test.h"
#include "game/actors.h"
#include "platform/gfx.h"
#include "platform/res.h"
#include "mem.h"
#include "symbols.h"
#include <string.h>

/* PORT: descriptor 0x9AC30 is the title logo's (data object offset 0x1AC30,
 * resident in mem[] after test_le). Its pinned title call-site arguments are
 * actor_spawn(0x9AC30, 0x4840, 0xE0, 0x1B00, 0); a5 = 0 is also the 0x2AC80
 * alloc flag word — args doc §1 site 2, §3. */
static void check_actor_spawn(void)
{
    actors_reset();
    const u32 *desc = (const u32 *)(mem + 0x9AC30u);
    u32 rec = actor_spawn(desc, 0x4840u, 0xE0u, 0x1B00u, 0u);
    CHECK(rec != 0, "spawn returns a record");
    if (rec == 0) return;
    CHECK_EQ_INT((int)actor_index(rec), 0);
    CHECK_EQ_INT((int)DSW(rec + 0x2e), 0x0000);   /* desc+0x06 */
    CHECK_EQ_INT((int)DSB(rec + 0x48), 0x00);     /* desc+0x04 render type */
    CHECK_EQ_INT((int)DSW(rec + 0x40), 0x2000);   /* desc+0x0A << 6 */
    CHECK_EQ_INT((int)DSW(rec + 0x2c), 0x0010);   /* desc+0x0C */
    CHECK_EQ_INT((int)(DSW(rec + 0x28) & 0xC3), 0);
    CHECK_EQ_INT((int)DSW(rec + 0x56), 0);        /* pset slot */
    CHECK_EQ_INT((int)DSD(rec + 0x18), 0x4840);   /* a2 */
    CHECK_EQ_INT((int)DSD(rec + 0x1c), 0x1B00);   /* a4 */
    u32 pset = actor_pset(rec);
    CHECK_EQ_INT((int)DSB(rec + 0x5f), 1);
    CHECK_EQ_INT((int)DSW(pset + 0x02), 0x0800);  /* rec+0x2E | hflip */

    /* Pool exhaustion: actor_alloc returns 0, so actor_spawn must too, leaving
     * both lists untouched. */
    actors_reset();
    for (u32 i = 0; i < 580; i++) CHECK(actor_alloc(0) != 0, "fill");
    u32 active = actor_list_head();
    u32 free_head = DSD(DS_00105B3C);
    CHECK_EQ_INT((int)actor_spawn(desc, 0x4840u, 0xE0u, 0x1B00u, 0u), 0);
    CHECK_EQ_INT((int)actor_list_head(), (int)active);
    CHECK_EQ_INT((int)DSD(DS_00105B3C), (int)free_head);
}

/* 0x2A31C -> 0x2A1FC -> 0x2A820/0x2A690. The title logo (descriptor 0x9AC30)
 * spawns with the 0x2000 flag, so 0x2A820 takes its high-byte-0x20 branch; the
 * test clears that bit to route through 0x2A690 and exercises the transformed
 * position and the layer from rec+0x59. */
static void check_pset_sync(void)
{
    const u32 *desc = (const u32 *)(mem + 0x9AC30u);

    /* 0x2A690: pset x = rec+0x18 - DS_000F0AF0 + 0x2A00; y =
     * DS_000F0AEC + 0x3BC0 - (rec+0x30>>16) - rec+0x1C. */
    actors_reset();
    DSB(DS_00104B24) = 0;                 /* 0x2A31C's gate */
    DSB(DS_00104B26) = 0;                 /* 0x2A1FC's 0x2A820 short-circuit */
    DSD(DS_000F0AF0) = 0;
    DSD(DS_000F0AEC) = 0;
    u32 rec = actor_spawn(desc, 0x4840u, 0xE0u, 0x1B00u, 0u);
    CHECK(rec != 0, "pset sync record");
    if (rec == 0) return;
    DSW(rec + 0x28) &= 0xdfffu;           /* clear 0x2000: take the 0x2A690 path */
    DSD(rec + 0x18) = 0x1000u;
    DSD(rec + 0x1c) = 0x1000u;
    DSW(rec + 0x2c) = 0x00aa;
    DSB(rec + 0x59) = 8;
    DSB(rec + 0x5a) = 0x11;               /* not read by the sync */
    u32 pset = actor_pset(rec);
    actors_update();
    CHECK_EQ_INT((int)DSD(pset + 0x04), 0x3a00);
    CHECK_EQ_INT((int)DSD(pset + 0x08), 0x2bc0);
    CHECK_EQ_INT((int)DSW(pset + 0x0e), 0x00f8);   /* (s8)8 + 0xF0 */
    CHECK_EQ_INT((int)DSW(pset + 0x0c), 0x00aa);
    CHECK_EQ_INT((int)DSD(rec + 0x3c), 0x3a00);    /* mirrors the written pset x */
    CHECK_EQ_INT((int)(DSB(rec + 0x2b) & 0x18), 0x18);  /* on-screen */

    /* 0x2A39C: rec+0x28 & 4 clears the bit and writes pset+0 from 0x2A408. The
     * logo stream (0x0E9116) begins with the literal 0x2C11, so the reader
     * writes it back unchanged. */
    actors_reset();
    DSB(DS_00104B24) = 0;
    DSB(DS_00104B26) = 0;
    u32 r2 = actor_spawn(desc, 0x4840u, 0xE0u, 0x1B00u, 0u);
    CHECK(r2 != 0, "anim-id record");
    if (r2 != 0) {
        DSD(r2 + 0x24) = 0;               /* no frame_timer, so only 0x2A39C runs */
        DSW(r2 + 0x28) |= 4u;
        u32 p2 = actor_pset(r2);
        DSW(p2 + 0x00) = 0x1234;
        actors_update();
        CHECK_EQ_INT((int)(DSW(r2 + 0x28) & 4u), 0);
        CHECK_EQ_INT((int)DSW(p2 + 0x00), 0x2C11);   /* 0x2A408 literal */
    }

    /* 0x2A820's on-screen test: a record far outside its extent does not get the
     * +0x2B 0x18 visibility bits. */
    actors_reset();
    DSB(DS_00104B24) = 0;
    DSB(DS_00104B26) = 0;
    DSD(DS_000F0AF0) = 0;
    DSD(DS_000F0AEC) = 0;
    u32 r3 = actor_spawn(desc, 0x4840u, 0xE0u, 0x1B00u, 0u);
    CHECK(r3 != 0, "offscreen record");
    if (r3 != 0) {
        DSD(r3 + 0x18) = 0x100000u;       /* beyond extent + 0x5400 */
        DSD(r3 + 0x1c) = 0x1000u;
        DSW(r3 + 0x2c) = 0x00aa;
        DSB(r3 + 0x2b) &= (u8)~0x18u;
        actors_update();
        CHECK_EQ_INT((int)(DSB(r3 + 0x2b) & 0x18), 0);
    }
}

/* 0x2F0F0/0x2F198/0x2F280/0x2F4BC. Plan Format reference H calls these "pset
 * layer select"; the shipped machine is a display-string width / text-cursor /
 * record-grid group (see the PORT note in actors.c). Expected values below are
 * hand-computed from the disassembly and the loaded font tables at DS
 * 0x3D048 / 0x3D1EC / 0x3D38D. */
static void check_pset_layer(void)
{
    actors_reset();

    /* 0x2F0F0. Mode & 3 in {0,1}: strlen. In {2,3}: class = (s8)DSB(0xBD390+c);
     * a negative class is skipped; otherwise add
     * (DSB(table + class*4 + 2) == 8 ? 1 : 2). Loaded data: class('A')=10,
     * class('B')=11 (both widths 16 -> 2); class('I')=18 (table A 16 -> 2,
     * table B 8 -> 1); class('"')=255 (skipped). */
    CHECK_EQ_INT(text_width((const u8 *)"", 0), 0);
    CHECK_EQ_INT(text_width((const u8 *)"ABC", 0), 3);
    CHECK_EQ_INT(text_width((const u8 *)"ABC", 1), 3);
    CHECK_EQ_INT(text_width((const u8 *)"A\"I", 2), 4);
    CHECK_EQ_INT(text_width((const u8 *)"A\"I", 3), 3);

    /* 0x2F198. The cursor is the word pair at DS_00105F34: low = row, high =
     * col + 0x2F830's glyph count (Task 8b; the extent was a 0-returning seam). */
    DSD(DS_00105F34) = 0;
    text_cursor_set(5, 7, (const u8 *)"A", 0);
    CHECK_EQ_INT((int)DSW(DS_00105F34), 7);
    CHECK_EQ_INT((int)DSW(DS_00105F34 + 2), 6);     /* col 5 + 1 glyph */

    DSD(DS_00105F34) = 0;
    text_cursor_set(-1, 9, (const u8 *)"ABC", 0);   /* col = (0x2b - 3) >> 1 */
    CHECK_EQ_INT((int)DSW(DS_00105F34), 9);
    CHECK_EQ_INT((int)DSW(DS_00105F34 + 2), 23);    /* col 20 + 3 glyphs */

    DSW(DS_00105F34) = 0x20;                        /* row == -1 reuses it */
    DSW(DS_00105F34 + 2) = 0x10;
    text_cursor_set(-1, -1, (const u8 *)"", 0);
    CHECK_EQ_INT((int)DSW(DS_00105F34), 0x20);
    CHECK_EQ_INT((int)DSW(DS_00105F34 + 2), 0x10);

    /* 0x2F4BC saves/restores DS_00105F34 around the same call. */
    DSD(DS_00105F34) = 0x12345678;
    text_cursor_hold(1, 2, (const u8 *)"A", 0);
    CHECK_EQ_INT((int)DSD(DS_00105F34), (int)0x12345678u);

    /* 0x2F280 clears `text_width` consecutive cells on the 43-wide diagonal
     * and returns each record through 0x2AD40 (pset+0x0E zeroed, dead bit). */
    actors_reset();
    u32 rec = actor_spawn((const u32 *)(mem + 0x9AC30u), 0x4840u, 0xE0u,
                          0x1B00u, 0u);
    CHECK(rec != 0, "grid record");
    if (rec != 0) {
        u32 pset = actor_pset(rec);
        DSW(pset + 0x0e) = 0xabcd;
        DSD(DS_00105F38) = rec;                     /* row 0, col 0 */
        text_cells_release(0, 0, (const u8 *)"ABC", 0);   /* strlen = 3 */
        CHECK_EQ_INT((int)DSD(DS_00105F38), 0);
        CHECK_EQ_INT((int)DSW(pset + 0x0e), 0);
        CHECK_EQ_INT((int)(DSW(rec + 0x28) & 8u), 8);
    }

    /* A centered run starts at col (0x2b - width) >> 1. */
    actors_reset();
    rec = actor_spawn((const u32 *)(mem + 0x9AC30u), 0x4840u, 0xE0u,
                      0x1B00u, 0u);
    CHECK(rec != 0, "centered grid record");
    if (rec != 0) {
        DSD(DS_00105F38 + 20u * 4u) = rec;          /* row 0, col 20 */
        text_cells_release(-1, 0, (const u8 *)"ABC", 0);
        CHECK_EQ_INT((int)DSD(DS_00105F38 + 20u * 4u), 0);
    }

    /* Zero width clears nothing. */
    actors_reset();
    rec = actor_spawn((const u32 *)(mem + 0x9AC30u), 0x4840u, 0xE0u,
                      0x1B00u, 0u);
    CHECK(rec != 0, "empty-string grid record");
    if (rec != 0) {
        DSD(DS_00105F38) = rec;
        text_cells_release(0, 0, (const u8 *)"", 0);
        CHECK_EQ_INT((int)DSD(DS_00105F38), (int)rec);
    }
}

int test_actors(void)
{
    int before = g_failures;

    /* The pool and pset bases come from res_load_index, so a run that reached
     * here has them; assert the shape the rest of the cycle depends on. Earlier
     * tests load the INDEX; a second load would exhaust the bump allocator's
     * 64 MB mem[], so only load if this test runs first. */
    if (res_count() != 69)
        CHECK(res_load_index("data/game/C", "data/game/C/INDEX") == 69,
              "index loaded");
    CHECK(DSD(DS_001014F4) != 0, "actor pool allocated by res_load_index");
    CHECK(DSD(DS_001014EC) != 0, "pset pool allocated by res_load_index");
    CHECK(actors_init() == 1, "actors_init validates the two pools");

    /* A fresh reset frees every record and leaves both lists empty. */
    actors_reset();
    CHECK_EQ_INT((int)actor_list_head(), 0);
    CHECK(actor_alloc(0) != 0, "alloc after reset returns a record");

    /* Allocating every record then exhausting returns 0, never a duplicate. */
    memset(mem + DSD(DS_001014F4), 0, 0xEBA0);
    actors_reset();
    u32 n = 0, first = actor_alloc(0);
    CHECK(first != 0, "first alloc");
    for (n = 1; n < 580; n++) {
        u32 r = actor_alloc(0);
        CHECK(r != 0, "alloc within the pool");
        if (r == first) { CHECK(0, "alloc returned the same record twice"); break; }
    }
    CHECK_EQ_INT((int)actor_alloc(0), 0);   /* exhaustion */

    /* Free then realloc: the freed record is the one handed back (0x249D0
     * pops the free-list head that 0x249C0 pushed). */
    actors_reset();
    u32 a = actor_alloc(0), b = actor_alloc(0);
    CHECK(b != 0, "second alloc");
    actor_free(a);
    CHECK_EQ_INT((int)actor_alloc(0), (int)a);

    /* An out-of-pool or misaligned offset is ignored, not linked. */
    actors_reset();
    u32 c = actor_alloc(0);
    actor_free(0x1234u);                       /* outside the pool */
    actor_free(c + 1u);                        /* misaligned */
    CHECK_EQ_INT((int)actor_alloc(0), (int)c + 0x68u);

    /* reset() zeroes both process masks (0x2A31C's gates) and rebuilds the
     * lists: after it, allocation order restarts at the pool base. */
    DSD(DS_00104AE8) = 0xFFFFFFFFu;
    DSD(DS_00104AEC) = 0xFFFFFFFFu;
    actors_reset();
    CHECK_EQ_INT((int)DSD(DS_00104AE8), 0);
    CHECK_EQ_INT((int)DSD(DS_00104AEC), 0);
    CHECK_EQ_INT((int)actor_alloc(0), (int)DSD(DS_001014F4));

    /* 0x2BAF4's param_1 != 0 arm runs 0x52106, which clears the screen aperture
     * (0x5214C-0x52151 `mov eax,0xa0000; call 0x51f72`) beside the two offscreen
     * buffers. A sentinel that differs from the post-state cannot survive. */
    memset(gfx_aperture(), 0x5Au, 0xFA00u);
    actors_reset();
    CHECK_EQ_INT((int)gfx_aperture()[0], 0);
    CHECK_EQ_INT((int)gfx_aperture()[0xFA00u - 1u], 0);

    /* The active list is the circular free/active pair 0x2AC80 links into:
     * after two allocs the head is the most recent record, and actor_next
     * walks to the previous one. */
    actors_reset();
    u32 p = actor_alloc(0), q = actor_alloc(0);
    CHECK_EQ_INT((int)actor_list_head(), (int)q);
    CHECK_EQ_INT((int)actor_next(q), (int)p);
    CHECK_EQ_INT((int)actor_next(p), 0);
    CHECK_EQ_INT((int)actor_index(q), 1);
    CHECK_EQ_INT((int)actor_record(1), (int)q);
    CHECK_EQ_INT((int)actor_record(999), 0);
    CHECK_EQ_INT((int)actor_index(0x1234u), -1);
    DSW(q + 0x56) = 3;
    CHECK_EQ_INT((int)actor_pset(q), (int)(DSD(DS_001014EC) + 3u * 0x20u));

    check_pset_sync();
    check_actor_spawn();
    check_pset_layer();

    return g_failures - before;
}
