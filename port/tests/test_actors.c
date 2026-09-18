/* port/tests/test_actors.c */
#include "test.h"
#include "game/actors.h"
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

    check_actor_spawn();

    return g_failures - before;
}
