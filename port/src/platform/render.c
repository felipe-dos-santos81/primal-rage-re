#include "platform/render.h"
#include "platform/res.h"
#include "platform/sprite.h"
#include "../game/actors.h"
#include "../game/flow.h"
#include "../mem.h"
#include "../symbols.h"

#define RENDER_NODE_POOL   DS_0010153C
#define RENDER_FREE_HEAD   DS_0010275C
#define RENDER_LIST_HEAD   DS_00105B44
#define RENDER_NODE_COUNT  580
#define RENDER_NODE_BYTES  8
#define RENDER_PSET_LAYER  0x0Eu

/* The per-scene actor-descriptor pointer tables 0x387F4/0x38890 read.
 * symbols.h emits no name for either address. */
#define DS_000BDF7C  0x000BDF7Cu
#define DS_000BDF9C  0x000BDF9Cu

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

/* PORT: 0x389C4. The raw divides by 64 with MSVC's `sar`/`shl`/`sbb` signed
 * idiom: for a negative numerator it adds 63, so the quotient truncates toward
 * zero. C's `/` on int is exactly that; an arithmetic `>> 6` would floor and
 * differ on negative inputs. The < 1 arm's numerator is a zero-extended u16, so
 * its bias never applies but the `/ 64` is kept for symmetry with the raw. */
void render_scroll_edge(void)
{
    if ((s16)DSW(DS_00107A3C) < 1) {
        DSW(DS_00107A38) = (u16)((int)DSW(DS_00107A48) / 64);
        DSW(DS_00107A4C) = DSW(DS_00107A4E);
    } else {
        int x = (int)DSW(DS_00107A48) - ((s32)DSD(DS_00107A3A) >> 16);
        DSW(DS_00107A38) = (u16)(x / 64);
        /* PORT: `ja` is the unsigned u16 compare DS_00107A4A > DS_00107A4C;
         * equal falls through to the store. */
        if (DSW(DS_00107A4A) > DSW(DS_00107A4C)) return;
        DSW(DS_00107A4C) = DSW(DS_00107A4A);
    }
}

/* PORT: 0x38A38. The same signed truncating form: every `/ 256` and `/ 32` is
 * the raw's `sar`/`shl`/`sbb` divide by 2^n, truncating toward zero, not a
 * plain arithmetic shift. The table entry is the raw's `t = (row / 256) >> 1`
 * -- the truncating /256 at 0x38a88 (`sar eax,8`) followed by the arithmetic
 * `sar edx,1` at 0x38a8f -- and DS_00107A3E reuses that same shifted t. `edge`
 * mirrors the raw's edi -- (u16)DS_00107A4C plus the initial row index,
 * decremented once per row -- and only its low 16 bits gate the DS_00107A3E
 * store, which is skipped while that value exceeds 0xEF.
 *
 * PORT: DS_00107A40 is the divisor of the raw's `idiv ecx` at 0x38A59; it must
 * be nonzero (a zero divisor traps there, while C's `/` on zero is undefined).
 * Callers seed it from the loaded scroll config before the fill runs. */
void render_scroll_fill(void)
{
    int stride = (int)(DSD(DS_000F0AF0) << 8);
    int step   = stride / (int)DSW(DS_00107A40);
    int idx    = (int)DSW(DS_00107A52) - 1;
    int edge   = (int)DSW(DS_00107A4C) + idx;
    int row    = stride;

    for (; idx >= 0; idx--) {
        int t = (row / 256) >> 1;
        DSW(DS_00107900 + (u32)idx * 2u) = (u16)t;
        if ((u16)edge <= 0xEF)
            DSW(DS_00107A3E) = (u16)((t + 0x2B00) / 32);
        edge--;
        row -= step;
    }
    row -= (int)DSW(DS_00107A42) * step;
    DSW(DS_00107A44 + 2) = (u16)(row / 256);
    DSW(DS_00107A3A) = (u16)((((row / 256) >> 1) + (int)DSW(DS_00107A50)) / 32);
}

/* 0x387F4. The scene's first actor and edge base. Spawns the actor whose
 * descriptor 0xBDF7C[i] points at (layer a3 = 2), seeds DS_00107A55 from the
 * 0xA8A18 byte table and DS_00107A48 from the actor sprite's height, and
 * acquires the four scene palettes. The height comes from the resolved sprite
 * descriptor's first dword (the 12-byte { s16 w; s16 h; ... } record
 * gra_sprite_lookup reads), high word, signed. */
static void render_scroll_scene_a(u32 i)
{
    DSB(DS_00107A55) = DSB(DS_000A8A18 + i);            /* 0x387F9/0x387FF */
    u32 desc = DSD(DS_000BDF7C + i * 4u);               /* 0x38804 */
    u32 handle = DSD(DS_000A8B30 + DSD(desc) * 4u);     /* 0x3880B/0x3880D */
    const u32 *res = res_resolve(handle);               /* 0x38814 (0x1B544) */
    /* PORT: the raw dereferences the resolved pointer unguarded; res_resolve
     * returns NULL for an out-of-range handle, so a missing resource reads 0. */
    int v = res ? (int)(*res) >> 16 : 0;                /* 0x38819/0x3881B */
    /* PORT: the raw's abs() arm (0x38820 JGE skips the 0x38822 MOV EBX,EAX)
     * keeps EBX when the value is non-negative, and 0x38733/0x387CF make that
     * EBX the scene index i. A negative height is negated. */
    int x = (v < 0) ? -v : (int)i;                      /* 0x38822/0x38824 */
    DSW(DS_00107A48) = (u16)((u32)(x - (int)DSW(DS_00107A4E)) << 6u); /* 0x38835/0x3883D */
    DSD(DS_000BDFBC) = actor_spawn((const u32 *)(mem + desc), 0u, 2u, 0u, 0u);  /* 0x3884C/0x38851 */
    if (DSW(DS_00104B00) != 0x13u) {                    /* 0x38856/0x38861 */
        palette_acquire(0x105FF3Cu);                    /* 0x38863/0x38868 */
        palette_acquire(0x080997Cu);                    /* 0x3886D/0x38872 */
        palette_acquire(0x0809984u);                    /* 0x38877/0x3887C */
        palette_acquire(0x080998Cu);                    /* 0x38881/0x38886 */
    }
}

/* 0x38890. The scene's second actor and the shear-table length. Spawns the
 * actor whose descriptor 0xBDF9C[i] points at (layer a3 = 1, a4 = DS_00107A4E +
 * 0x100), seeds DS_00107A56 from the 0xA8A20 byte table and DS_00107A52 =
 * 0xF8 - DS_00107A4E. */
static void render_scroll_scene_b(u32 i)
{
    DSB(DS_00107A56) = DSB(DS_000A8A20 + i);            /* 0x38893/0x3889E */
    DSW(DS_00107A52) = (u16)(0xF8 - (int)DSW(DS_00107A4E));   /* 0x388AB/0x388C3 */
    /* PORT: the raw resolves the two sprite handles at 0x388CA and 0x388E5 and
     * discards both pointers; 0x1B544 has no side effect in the port. */
    u32 desc = DSD(DS_000BDF9C + i * 4u);               /* 0x388F3 */
    DSD(DS_000BDFC0) = actor_spawn((const u32 *)(mem + desc), 0u, 1u,
                                   (u32)DSW(DS_00107A4E) + 0x100u, 0u);  /* 0x38901/0x38906 */
}

/* 0x38730. The attract scene/zoom projection setup: seed the per-scene
 * scroll/zoom tables and enable the per-frame projection. `i` is the scene
 * index, which the demo's only call site passes as the state-6 RNG draw
 * (0x20DF7 clamps EAX to 7 into EBX, 0x20E7D reloads it into EAX). The raw's
 * two 0x2EA30 interrupt-lock calls are inert in the port (actors_reset
 * documents the same). */
void render_scroll_setup(u32 i)
{
    DSW(DS_00107A4E) = DSW(DS_000BDE1C + i * 2u);       /* 0x38741 */
    DSW(DS_00107A42) = DSW(DS_000BDE2C + i * 2u);       /* 0x3874F */
    frontend_origin_zero();                             /* 0x38764 (0x4F1D0) */
    DSW(DS_00107A4A) = DSW(DS_00107A4E);                /* 0x3876F */
    DSW(DS_00107A4C) = DSW(DS_00107A4E);                /* 0x38775 */
    DSW(DS_00107A50) = DSW(DS_000BDE0C + i * 2u);       /* 0x38785 */
    DSW(DS_00107A40) = DSW(DS_000BDDFC + i * 2u);       /* 0x3879A */
    DSW(DS_00107A3A) = (u16)((int)DSW(DS_00107A50) / 32);   /* 0x387AA */
    /* PORT: this divide reads the PRE-call DS_00107A48; 0x387F4 writes the new
     * one below and render_scroll_edge re-derives DS_00107A38 from it on the
     * next master-loop frame. */
    DSW(DS_00107A38) = (u16)((int)DSW(DS_00107A48) / 64);   /* 0x387C6 */
    render_scroll_scene_a(i);                           /* 0x387D1 (0x387F4) */
    render_scroll_scene_b(i);                           /* 0x387D8 (0x38890) */
    render_scroll_fill();                               /* 0x387DD (0x38A38) */
    DSB(DS_00107A54) = 1;                               /* 0x387E2 */
}
