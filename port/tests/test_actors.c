/* port/tests/test_actors.c */
#include "test.h"
#include "game/actors.h"
#include "platform/res.h"
#include "mem.h"
#include "symbols.h"
#include <string.h>

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
    CHECK(actor_alloc() != 0, "alloc after reset returns a record");

    /* Allocating every record then exhausting returns 0, never a duplicate. */
    memset(mem + DSD(DS_001014F4), 0, 0xEBA0);
    actors_reset();
    u32 n = 0, first = actor_alloc();
    CHECK(first != 0, "first alloc");
    for (n = 1; n < 580; n++) {
        u32 r = actor_alloc();
        CHECK(r != 0, "alloc within the pool");
        if (r == first) { CHECK(0, "alloc returned the same record twice"); break; }
    }
    CHECK_EQ_INT((int)actor_alloc(), 0);   /* exhaustion */

    /* Free then realloc: the freed record is the one handed back (0x249D0
     * pops the free-list head that 0x249C0 pushed). */
    actors_reset();
    u32 a = actor_alloc(), b = actor_alloc();
    CHECK(b != 0, "second alloc");
    actor_free(a);
    CHECK_EQ_INT((int)actor_alloc(), (int)a);

    /* An out-of-pool or misaligned offset is ignored, not linked. */
    actors_reset();
    u32 c = actor_alloc();
    actor_free(0x1234u);                       /* outside the pool */
    actor_free(c + 1u);                        /* misaligned */
    CHECK_EQ_INT((int)actor_alloc(), (int)c + 0x68u);

    /* reset() zeroes both process masks (0x2A31C's gates) and rebuilds the
     * lists: after it, allocation order restarts at the pool base. */
    DSD(DS_00104AE8) = 0xFFFFFFFFu;
    DSD(DS_00104AEC) = 0xFFFFFFFFu;
    actors_reset();
    CHECK_EQ_INT((int)DSD(DS_00104AE8), 0);
    CHECK_EQ_INT((int)DSD(DS_00104AEC), 0);
    CHECK_EQ_INT((int)actor_alloc(), (int)DSD(DS_001014F4));

    /* The active list is the circular free/active pair 0x2AC80 links into:
     * after two allocs the head is the most recent record, and actor_next
     * walks to the previous one. */
    actors_reset();
    u32 p = actor_alloc(), q = actor_alloc();
    CHECK_EQ_INT((int)actor_list_head(), (int)q);
    CHECK_EQ_INT((int)actor_next(q), (int)p);
    CHECK_EQ_INT((int)actor_next(p), 0);
    CHECK_EQ_INT((int)actor_index(q), 1);
    CHECK_EQ_INT((int)actor_record(1), (int)q);
    CHECK_EQ_INT((int)actor_record(999), 0);
    CHECK_EQ_INT((int)actor_index(0x1234u), -1);
    DSW(q + 0x56) = 3;
    CHECK_EQ_INT((int)actor_pset(q), (int)(DSD(DS_001014EC) + 3u * 0x20u));

    return g_failures - before;
}
