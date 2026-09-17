#include "platform/render.h"
#include "platform/sprite.h"
#include "../mem.h"
#include "../symbols.h"

#define RENDER_NODE_POOL   DS_0010153C
#define RENDER_FREE_HEAD   DS_0010275C
#define RENDER_LIST_HEAD   DS_00105B44
#define RENDER_NODE_COUNT  580
#define RENDER_NODE_BYTES  8
#define RENDER_PSET_LAYER  0x0Eu

const struct RenderCamera render_camera_default = { 0, 0, 0, 0, 320, 200 };

/* PORT: 0x14328's projection. The original seeds +0x800 and then applies the
 * sbb-side correction: when the sum is negative it adds a further 0xFFF (the
 * sign mask shifted by 12, less the borrow), so the result rounds half toward
 * +infinity rather than half away from zero. An arithmetic >>12 alone is the
 * uncorrected truncation and is not equivalent for negative inputs. */
static int proj_scale(int v, int num)
{
    int p = v * num + 0x800;
    if (p < 0) p += 0xFFF;
    return p >> 12;
}

/* TODO(verify): 0x14328's three offset projections (layer-1 x at prage.c:3166,
 * layer-2 x at :3173 and layer-2 y fallback at :3175) are a plain +0x800 then
 * arithmetic >>12 with no sign correction, unlike the main projection used for
 * px/py and layer 1's y. In a compositor-only run all five offset globals are
 * zero, so the two forms coincide; 4a-ii's pixel oracle must confirm which form
 * the original applies to these (decompiled as unsigned) operands. */
static int proj_scale_u(int v, int num)
{
    return (v * num + 0x800) >> 12;
}

int render_proj_x(int v) { return proj_scale(v, 3901); }
int render_proj_y(int v) { return proj_scale(v, 3414); }

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

/* PORT: 0x14328. `last_mode1_y` tracks the most recent layer-1 entry's y in
 * list order (not a running minimum) and starts at -1. The clip rectangle is
 * the camera's clip fields; the camera's own x/y are the projection origin. */
void render_list(void)
{
    const struct RenderCamera *cam = &render_camera_default;
    int last_mode1_y = -1;

    for (u32 node = DSD(RENDER_LIST_HEAD); node != 0; node = DSD(node)) {
        u32 pset = DSD(node + 4);
        SpriteNode n;
        sprite_node_build(&n, DSW(pset + 0x00));
        n.pal_ptr = DSD(pset + 0x18);
        u16 layer = DSW(pset + RENDER_PSET_LAYER);

        int px, py;
        if (layer > 2) {
            px = (s32)DSD(pset + 4) >> 6;
            py = (s32)DSD(pset + 8) >> 6;
        } else {
            px = (s32)DSD(pset + 4);
            py = (s32)DSD(pset + 8);
        }
        int x = render_proj_x(px) - cam->x - n.xorg;
        int y = render_proj_y(py) - cam->y - n.yorg;

        if (layer == 1) {
            x -= proj_scale_u((s16)DSW(DS_00107A3E), 3901);
            y  = render_proj_y((s16)DSW(DS_00107A4E) +
                               ((s32)DSD(DS_000F0AEC) >> 6)) - cam->y;
            last_mode1_y = y;
            n.type |= 4u;
        }
        if (layer == 2) {
            x -= proj_scale_u((s16)DSW(DS_00107A3A), 3901);
            if (last_mode1_y == -1) y -= proj_scale_u((s16)DSW(DS_00107A38), 3414);
            else                    y  = last_mode1_y - n.rows;
        }

        int W = n.width, H = n.rows;
        if (W + x < 0 || H + y < 0) continue;

        n.clip_b = ((H + y) - cam->clip_b > 0) ? (H + y) - cam->clip_b : 0;
        n.clip_r = ((W + x) - cam->clip_r > 0) ? (W + x) - cam->clip_r : 0;
        int t = cam->clip_t - y;
        n.clip_t = (t > 0) ? t : 0;
        if (t >= 0) y = cam->clip_t;
        int l = cam->clip_l - x;
        n.clip_l = (l > 0) ? l : 0;
        if (l >= 0) x = cam->clip_l;

        int sumX = n.clip_l + n.clip_r;
        int sumY = n.clip_t + n.clip_b;
        if (sumX + sumY == 0) {
            n.x = x; n.y = y;
            sprite_blit(&n);
        } else if (sumX < W && sumY < H) {
            n.type |= 0x10u;
            n.x = x; n.y = y;
            sprite_blit(&n);
        }
    }
}
