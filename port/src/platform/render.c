#include "platform/render.h"
#include "../mem.h"
#include "../symbols.h"

#define RENDER_NODE_POOL   DS_0010153C
#define RENDER_FREE_HEAD   DS_0010275C
#define RENDER_LIST_HEAD   DS_00105B44
#define RENDER_NODE_COUNT  580
#define RENDER_NODE_BYTES  8
#define RENDER_PSET_LAYER  0x0Eu

static int render_count;

static u16 node_layer(u32 node)
{
    return DSW(DSD(node + 4) + RENDER_PSET_LAYER);
}

/* PORT: 0x1C3A0. Splices `node` before the first node whose layer is greater,
 * so equal layers keep their relative order (stable). This is the one place the
 * ordering rule lives: render_list_insert and render_list_sort both go through
 * it. `node` must be detached -- its next field is overwritten here. */
static void render_splice(u32 *headp, u32 node)
{
    u16 layer = node_layer(node);
    u32 prev = 0;
    u32 cur  = *headp;
    while (cur != 0 && node_layer(cur) <= layer) {
        prev = cur;
        cur = DSD(cur);
    }
    DSD(node) = cur;
    if (prev == 0) *headp = node;
    else DSD(prev) = node;
}

void render_list_init(void)
{
    for (u32 i = 0; i < RENDER_NODE_COUNT; i++) {
        u32 node = RENDER_NODE_POOL + i * RENDER_NODE_BYTES;
        DSD(node) = (i + 1u < RENDER_NODE_COUNT) ? node + RENDER_NODE_BYTES : 0u;
        DSD(node + 4) = 0u;
    }
    DSD(RENDER_FREE_HEAD) = RENDER_NODE_POOL;
    DSD(RENDER_LIST_HEAD) = 0u;
    render_count = 0;
}

int render_list_insert(u32 pset_off)
{
    u32 node = DSD(RENDER_FREE_HEAD);
    if (node == 0) return 0;                    /* PORT: pool exhausted guard */
    DSD(RENDER_FREE_HEAD) = DSD(node);
    DSD(node + 4) = pset_off;
    render_splice((u32 *)(mem + RENDER_LIST_HEAD), node);
    render_count++;
    return 1;
}

void render_list_remove(u32 pset_off)
{
    u32 prev = 0;
    u32 cur  = DSD(RENDER_LIST_HEAD);
    while (cur != 0 && DSD(cur + 4) != pset_off) {
        prev = cur;
        cur = DSD(cur);
    }
    if (cur == 0) return;                       /* PORT: 0x1C458 returns null */
    if (prev == 0) DSD(RENDER_LIST_HEAD) = DSD(cur);
    else DSD(prev) = DSD(cur);
    DSD(cur) = DSD(RENDER_FREE_HEAD);
    DSD(RENDER_FREE_HEAD) = cur;
    render_count--;
}

/* PORT: 0x1C3FC. Insertion sort in place: a node whose layer is strictly less
 * than its predecessor's is detached and re-spliced through render_splice, the
 * one place the ordering rule lives. A node that is merely equal never moves,
 * so equal layers keep their order and a full pass leaves the list ascending.
 * Needed because a pset's layer can change while it is listed. */
void render_list_sort(void)
{
    u32 head = DSD(RENDER_LIST_HEAD);
    if (head == 0) return;
    u32 prev = head;
    u32 cur  = DSD(head);
    while (cur != 0) {
        u32 next = DSD(cur);
        if (node_layer(cur) < node_layer(prev)) {
            DSD(prev) = next;
            render_splice((u32 *)(mem + RENDER_LIST_HEAD), cur);
        } else {
            prev = cur;
        }
        cur = next;
    }
}

u32 render_list_head(void)
{
    return DSD(RENDER_LIST_HEAD);
}

int render_list_count(void)
{
    return render_count;
}
