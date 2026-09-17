#include "test.h"
#include "platform/render.h"
#include "mem.h"
#include "symbols.h"

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

int test_render(void)
{
    check_list_order();
    return 0;
}
