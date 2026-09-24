/* Port of the fight camera state machine and the per-frame projection.
 * Addresses, widths and signedness are from
 * docs/superpowers/plans/2026-09-20-demo-fight-derivations.md §1-§3 and the
 * previous cycle's docs/superpowers/plans/2026-09-20-frontend-chain-derivations.md
 * §5. The module registers nothing: the update table DS_000A8644 has no camera
 * entry, and 0x1324C (the shake decay) is registered by effects.c. */
#include "game/camera.h"
#include "game/actors.h"
#include "game/rng.h"
#include "../mem.h"
#include "../symbols.h"
#include "platform/res.h"

#include <stddef.h>

/* 0x804A8: the 32-bit float `26 B4 17 38` = 3.616898175096139e-05 used by the
 * 0x1317C camera-y curve. Not a named global in symbols.h. */
#define CAMERA_CURVE 0x000804A8u

/* 0xBB254: the dust actor descriptor 0x1282C spawns (a data-object address). */
#define CAMERA_DUST_DESC 0x000BB254u

/* 0x10810D: the mode-3 single-player slot index. Ghidra emits it only as the
 * `ram0x0010810d` form, so gen_symbols.py has no DS_ name for it. */
#define CAMERA_SLOT_INDEX3 0x0010810Du

/* ---- small pure helpers ------------------------------------------------- */

/* 0x17EEC. The per-character ground constant for character index `ch` (the
 * slot's +0x7A byte). The jump table at 0x17ED0 sends char 0 and char > 6 to
 * word[0xE6DD0]. Exported because 0x33F08 selects from the same table. */
u32 camera_char_const(u32 ch)
{
    switch (ch) {
    case 1:  return DSW(DS_000E39D0);
    case 2:  return DSW(DS_000ECBD8);
    case 3:  return DSW(DS_000D2134);
    case 4:  return DSW(DS_000EA604);
    case 5:  return DSW(DS_000D3E08);
    case 6:  return DSW(DS_000E061C);
    default: return DSW(DS_000E6DD0);
    }
}

/* 0x1A570. The "actor bit 15 clear" predicate: 1 when
 * (word[actor] & 0x8000) == 0 for slot[side]'s actor. */
static int camera_actor_bit15_clear(u32 side)
{
    u32 rec = DSD(DS_001077B0 + side * 0x94u);
    u32 actor = DSD(DS_001014EC) + (u32)DSW(rec + 0x56u) * 0x20u;
    return (DSW(actor) & 0x8000u) == 0;
}

/* 0x16308. res_resolve(DSD(0xA8B30 + (0x17EEC(side) + index) * 4)). The raw
 * tail-jumps 0x1B544; a failed resolve returns the 0 sentinel. */
static u32 camera_resolve_sprite(u32 side, u32 index)
{
    u32 handle = DSD(DS_000A8B30
                     + (camera_char_const(DSB(DS_0010782A + side * 0x94u))
                        + index) * 4u);
    const u8 *p = (const u8 *)res_resolve(handle);
    return p != NULL ? (u32)(p - mem) : 0u;
}

/* ---- the projection 0x17FA0 -------------------------------------------- */

void camera_project(u32 side, u32 out_a, u32 out_b, u32 facing, u32 page_flag,
                    u32 index_out)
{
    /* 0x17FB5/0x1800A: the two "world anchor" blocks. They are independent of
     * the side argument and read the slot[0]/slot[1] secondary pointers
     * DS_001077B8 / DS_0010784C, skipping a null one. */
    {
        u32 r = DSD(DS_001077B8);
        if (r != 0) {
            u32 actor = DSD(DS_001014EC) + (u32)DSW(r + 0x56u) * 0x20u;
            DSD(DS_00100AA0) = (u32)((s32)DSD(actor + 8u) >> 6);   /* 0x17FDA */
            s32 adj = (s32)DSD(DS_000A174C + (u32)DSB(DS_0010782A) * 4u);
            if ((s16)DSW(r + 0x34u) > 0) adj = -adj;               /* 0x18002 */
            DSD(DS_00100AA8) = (u32)((s32)DSD(actor + 4u) >> 6) + (u32)adj;
        }
    }
    {
        u32 r = DSD(DS_0010784C);
        if (r != 0) {
            u32 actor = DSD(DS_001014EC) + (u32)DSW(r + 0x56u) * 0x20u;
            DSD(DS_00100AA4) = (u32)((s32)DSD(actor + 8u) >> 6);   /* 0x1802E */
            s32 adj = (s32)DSD(DS_000A174C + (u32)DSB(DS_001078BE) * 4u);
            if ((s16)DSW(r + 0x34u) > 0) adj = -adj;               /* 0x18053 */
            DSD(DS_00100AAC) = (u32)((s32)DSD(actor + 4u) >> 6) + (u32)adj;
        }
    }

    /* 0x1805B: P = slot[side]'s fighter record pointer (stride 0x94). */
    {
        u32 p = DSD(DS_001077B0 + side * 0x94u);

        /* 0x18070: *facing = (P->+0x28 & 0x4000) != 0. */
        DSB(facing) = (u8)((DSW(p + 0x28u) & 0x4000u) != 0);

        /* 0x18092: *page_flag = 0; the 0x16AFC/0x164F4 tail that could raise it
         * is an unported gap (§7.2), so the flag stays 0. */
        DSB(page_flag) = 0;

        u32 actor = DSD(DS_001014EC) + (u32)DSW(p + 0x56u) * 0x20u;

        /* 0x180A1: *index_out = (actor.word0 & 0x7FFF) - 0x17EEC(side). */
        {
            u32 base = (u32)DSW(actor) & 0x7FFFu;
            DSD(index_out) = base
                - camera_char_const(DSB(DS_0010782A + side * 0x94u));
        }

        /* 0x18140: out_a = ((s32)actor+4 + 0x20) >> 6, arithmetic. */
        DSD(out_a) = (u32)(((s32)DSD(actor + 4u) + 0x20) >> 6);

        /* 0x1815C: out_b = ((s32)actor+8 + 0x20) >> 6, arithmetic. */
        DSD(out_b) = (u32)(((s32)DSD(actor + 8u) + 0x20) >> 6);

        /* 0x18170: sprite = 0x16308(side, *index_out); subtract its origin.
         * local_A = (s32)((u32)sprite[+2]) >> 16, or (s16)sprite[0] - that - 1
         * when 0x1A570(side) == 0; local_B = (s32)((u32)sprite[+4]) >> 16.
         * out_b -= local_B; out_a -= local_A. PORT: res_resolve can return
         * NULL (the raw dereferences unconditionally); the subtraction is
         * skipped there, the port's existing resource-failure guard. */
        {
            u32 sprite = camera_resolve_sprite(side, DSD(index_out));
            if (sprite != 0) {
                s32 local_a = (s32)((u32)DSD(sprite + 2u) >> 16);
                s32 local_b = (s32)((u32)DSD(sprite + 4u) >> 16);
                if (!camera_actor_bit15_clear(side))
                    local_a = (s32)(s16)DSW(sprite) - local_a - 1;
                DSD(out_b) = (u32)((s32)DSD(out_b) - local_b);
                DSD(out_a) = (u32)((s32)DSD(out_a) - local_a);
            }
        }
    }
}

/* ---- the camera chain --------------------------------------------------- */

/* 0x12DF0. Mode 0: track one player's record x (the DS_001077A8 table selected
 * by DS_000F0AFF), within a 0x1800 dead zone. */
static void camera_mode_track_player(void)
{
    s32 cam = (s32)DSD(DS_000F0AF0);
    u32 rec = DSD(DS_001077A8 + (u32)DSB(DS_000F0AFF) * 4u);
    s32 diff = (s32)DSD(rec + 0x34u) - cam;
    s32 mag = diff < 0 ? -diff : diff;
    if (mag >= 0x1800) {
        diff += (diff > 0) ? -0x1800 : 0x1800;
        cam += diff;
    }
    DSD(DS_000F0AF0) = (u32)cam;
}

/* 0x1317C. Camera-y clamp: compute the target from DS_001078F2's high word and
 * step DS_000F0AEC toward it (0x12CD4), then cap at DS_0009AF28[DS_00104AFC]. */
static void camera_y_clamp(void)
{
    s32 x = (s32)DSD(DS_001078F2) >> 16;
    s32 target = 0;
    if (x > 0x1400) {
        /* 0x1318F: the FPU path, (int)trunc(d*d*C) with C the float at 0x804A8.
         * The raw multiplies d * (d*C); the port keeps that order in float. */
        float c = *(const float *)(mem + CAMERA_CURVE);
        float d = (float)(x - 0x1400);
        target = (s32)(d * (d * c));
    }

    /* 0x12CD4: step or snap DS_000F0AEC, then add the shake offset (the
     * sign-extended word at DS_000F0AF4, read as DSD(DS_000F0AF2) >> 16). */
    {
        s32 cam = (s32)DSD(DS_000F0AEC);
        s32 diff = target - cam;
        s32 mag = diff < 0 ? -diff : diff;
        if (mag <= 0x100) cam = target;
        else if (diff > 0) cam += 0x100;
        else cam -= 0x100;
        /* The raw reads DSD(DS_000F0AF2) >> 16, whose high word is the
         * sign-extended shake offset at DS_000F0AF4 (DS_000F0AF2 is not a
         * global of its own). */
        cam += (s32)(s16)DSW(DS_000F0AF4);
        DSD(DS_000F0AEC) = (u32)cam;
    }

    /* 0x131D0: cap camera y at (s32)DSD(0x9AF28 + DS_00104AFC*2) >> 16. */
    {
        u32 idx = DSW(DS_00104AFC);
        s32 lim = (s32)DSD(DS_0009AF28 + idx * 2u) >> 16;
        if (lim < (s32)DSD(DS_000F0AEC)) DSD(DS_000F0AEC) = (u32)lim;
    }
}

/* 0x12C70. The camera-x step seed: its whole body is `MOV word ptr
 * [0x000F0AFC],0x400` (the RET is at 0x12C79). Its sole caller is 0x20DF4 at
 * 0x20E6A (Ghidra xref), the state-6 fight reset, which runs it just before its
 * EDX branch (0x2BAF4/0x38730/0x412A0). camera_x_commit (0x12C7C) reads
 * DS_000F0AFC as the step and re-seeds it to 0x400 on the snap arm, so the seed
 * is only observable on the first commit: without it a nonzero delta would take
 * the `mag > step` arm with step 0, add 0 and never reach the snap. The demo's
 * first state-7 commit has delta 0 (the camera x stays 0), so the port is
 * net-faithful for these frames either way; the store is issued for the raw's
 * own sake. */
void camera_step_seed(void)
{
    DSW(DS_000F0AFC) = 0x400u;                          /* 0x12C70 */
}

/* 0x12C7C. The shared camera-x commit: snap when within DS_000F0AFC, else step
 * by it and reset the step to 0x400. */
static void camera_x_commit(s32 arg)
{
    s32 old = (s32)DSD(DS_000F0AF0);
    s32 step = (s32)DSW(DS_000F0AFC);
    s32 diff = arg - old;
    s32 mag = diff < 0 ? -diff : diff;
    if (mag > step) {
        old += (diff > 0) ? step : -step;
        DSD(DS_000F0AF0) = (u32)old;
    } else {
        DSW(DS_000F0AFC) = 0x400u;
        DSD(DS_000F0AF0) = (u32)arg;
    }
}

/* 0x12E3C. Mode 1: track the pair, front/back ordered. The exact 0x18714
 * updates are a named gap (§7.5): the raw pulls the separated slot toward its
 * +0x38 latch and rewrites the record's +0x18 through the unported 0x18714;
 * those calls are skipped, the rest of the selection is transcribed. */
static void camera_mode_track_pair(void)
{
    s32 cam = (s32)DSD(DS_000F0AF0);
    u32 a = ((s32)DSD(DS_001077E4) >= (s32)DSD(DS_00107878)) ? 1u : 0u;
    u32 b = 1u - a;
    u32 sa = DS_001077B0 + a * 0x94u;
    u32 sb = DS_001077B0 + b * 0x94u;

    /* esp[8+i*4] is slot[i]+0x34; a/b select which slot sits at index 0/1. */
    u32 lo_off = (a == 0u) ? sa : sb;   /* slot[0] */
    u32 hi_off = (a == 0u) ? sb : sa;   /* slot[1] */

    s32 la = (s32)DSD(sa + 0x34u);
    s32 lb = (s32)DSD(sb + 0x34u);
    s32 diff = la - lb;
    s32 diffabs = diff < 0 ? -diff : diff;

    /* 0x12ED7: split the pair when it exceeds word[0x9AF28]. */
    if (diffabs > (s32)DSW(DS_0009AF28)) {
        DSD(DS_000F0AF0) = (u32)cam;
        {
            s32 v = (s32)DSD(sa + 0x34u), latch = (s32)DSD(sa + 0x38u);
            if (v < latch) {
                DSD(sa + 0x34u) = (u32)latch;
                DSD(sa + 0x2cu) = (u32)latch;
                la = latch;
                /* PORT: 0x18714(side a) is unported (gap §7.5); the raw's
                 * `rec+0x18 = result` update is skipped here. */
            }
        }
        {
            s32 v = (s32)DSD(sb + 0x34u), latch = (s32)DSD(sb + 0x38u);
            if (v > latch) {
                DSD(sb + 0x34u) = (u32)latch;
                DSD(sb + 0x2cu) = (u32)latch;
                lb = latch;
                /* PORT: 0x18714(side b) is unported (gap §7.5); skipped. */
            }
        }
        diff = la - lb;
        diffabs = diff < 0 ? -diff : diff;
    }

    /* 0x12F44: the two locals, each folded into the 0x1800 dead zone. */
    s32 d0 = (s32)DSD(lo_off + 0x34u) - cam;
    s32 d1 = (s32)DSD(hi_off + 0x34u) - cam;
    s32 l0 = (d0 < -0x1800) ? d0 + 0x1800 : (d0 > 0x1800 ? d0 - 0x1800 : 0);
    s32 l1 = (d1 < -0x1800) ? d1 + 0x1800 : (d1 > 0x1800 ? d1 - 0x1800 : 0);

    /* 0x12F89: far apart -> midpoint; else the non-zero local. */
    s32 arg;
    if (diffabs > 0x3000) arg = (l0 + l1) / 2;
    else if (l0 == 0)     arg = l1;
    else                  arg = l0;
    arg += cam;
    DSD(DS_000F0AF0) = (u32)cam;
    camera_x_commit(arg);
}

/* 0x13290. Mode 2: center on the midpoint of the two records' +0x18, step 0x40,
 * settle to mode 4 (gated on DS_001078FE). */
static void camera_mode_center_two(void)
{
    s32 cam = (s32)DSD(DS_000F0AF0);
    s32 p0 = (s32)DSD(DSD(DS_001077B0) + 0x18u);
    s32 p1 = (s32)DSD(DSD(DS_00107844) + 0x18u);
    s32 mid = (p0 > p1) ? ((p0 - p1) / 2 + p1) : ((p1 - p0) / 2 + p0);

    s32 diff = mid - cam;
    s32 mag = diff < 0 ? -diff : diff;
    s32 acam = cam < 0 ? -cam : cam;
    int settle = 0;
    if (mag > 0x100) {
        if (acam == 0x5D00) settle = 1;
        else cam += (diff > 0) ? 0x40 : -0x40;
    } else if ((s32)DSD(DS_000F0AEC) == 0) {
        settle = 1;
    }
    if (settle && DSB(DS_001078FE) != 0) DSB(DS_000F0AFE) = 4;
    DSD(DS_000F0AF0) = (u32)cam;
}

/* 0x1333C. Mode 3: center on one indexed record's +0x18; the mode-4 settle is
 * NOT gated on DS_001078FE. */
static void camera_mode_center_one(void)
{
    s32 cam = (s32)DSD(DS_000F0AF0);
    u32 idx = DSB(CAMERA_SLOT_INDEX3);
    s32 px = (s32)DSD(DSD(DS_001077B0 + idx * 0x94u) + 0x18u);
    s32 diff = px - cam;
    s32 mag = diff < 0 ? -diff : diff;
    s32 acam = cam < 0 ? -cam : cam;
    int settle = 0;
    if (mag > 0x100) {
        if (acam == 0x5D00) settle = 1;
        else cam += (diff > 0) ? 0x40 : -0x40;
    } else if ((s32)DSD(DS_000F0AEC) == 0) {
        settle = 1;
    }
    if (settle) DSB(DS_000F0AFE) = 4;
    DSD(DS_000F0AF0) = (u32)cam;
}

void camera_dispatch(void)
{
    switch (DSB(DS_000F0AFE)) {
    case 0: camera_mode_track_player(); break;   /* 0x12D5E -> 0x12DF0 */
    case 1: camera_mode_track_pair();   break;   /* 0x12D65 -> 0x12E3C */
    case 2: camera_mode_center_two();   break;   /* 0x12D6C -> 0x13290 */
    case 3: camera_mode_center_one();   break;   /* 0x12D73 -> 0x1333C */
    default: break;                              /* mode 4+: no call */
    }
    /* 0x12D78: signed clamp to [-0x5D00, 0x5D00]. */
    if ((s32)DSD(DS_000F0AF0) > 0x5D00) DSD(DS_000F0AF0) = 0x5D00u;
    if ((s32)DSD(DS_000F0AF0) < -0x5D00) DSD(DS_000F0AF0) = (u32)-0x5D00;
}

/* ---- 0x17580 the per-frame decay --------------------------------------- */

/* The signed truncating multiply the four decays share: (s32)(x*num + 0x800) /
 * 0x1000 with C truncation toward zero, the raw's 0x175F6..0x1760C idiom. */
static s32 camera_scale(u32 x, u32 num)
{
    return (s32)(x * num + 0x800u) / 0x1000;
}

/* ---- 0x140E4 the box-overlap bool --------------------------------------- */

/* The projection idiom 0x140E4/0x15C30/0x17FA0 share: ((s32)(x >> 6) * K +
 * 0x800) / 0x1000 truncating toward zero (the raw's IMUL; ADD 0x800; SAR
 * 0x1F; SHL 0xC; SBB; SAR 0xC sequence, 0x14140..0x14156). K is 0xF3D for x
 * and 0xD56 for y. Reuses camera_scale's truncating divide. */
static s32 camera_project_axis(s32 x, u32 k)
{
    return camera_scale((u32)(x >> 6), k);
}

/* 0x14080. Intersect `a` and `b` into `out` (left/top = max, right/bottom =
 * min) and return 1 when the result is non-empty. The raw's caller passes
 * out == a (0x1424E LEA EAX,[ESP+0x10], 0x14256 LEA EDX,[ESP+0x10] with
 * EBX = ESP), so the intersection is computed in place. */
static int camera_rect_clip(int *out, const int *a, const int *b)
{
    out[0] = (a[0] > b[0]) ? a[0] : b[0];
    out[1] = (a[1] > b[1]) ? a[1] : b[1];
    out[2] = (a[2] < b[2]) ? a[2] : b[2];
    out[3] = (a[3] < b[3]) ? a[3] : b[3];
    return (out[2] >= out[0]) && (out[3] >= out[1]);
}

/* 0x140E4's per-actor rect (0x140EE..0x141A7 for the first actor,
 * 0x141AB..0x1425A for the second). The sprite origin comes from the
 * 0xA8B30 handle resolved by 0x1B544 with the raw index `word[actor] &
 * 0x7FFF` (no 0x17EEC character constant, unlike 0x16308). */
static void camera_actor_rect(u32 actor_idx, int *rect)
{
    u32 rec = DSD(DS_001014EC) + actor_idx * 0x20u;
    u32 id = (u32)DSW(rec) & 0x7FFFu;
    const u8 *sp = (const u8 *)res_resolve(DSD(DS_000A8B30 + id * 4u));
    s32 local_a = 0, local_b = 0, w = 0, h = 0;
    if (sp != NULL) {
        u32 off = (u32)(sp - mem);
        local_a = (s32)DSD(off + 2u) >> 16;         /* 0x14114/0x14123 */
        local_b = (s32)DSD(off + 4u) >> 16;         /* 0x1411A/0x1412B */
        w = (s32)(s16)DSW(off);                     /* 0x14132/0x14188 */
        h = (s32)DSD(off) >> 16;                    /* 0x14194/0x1419A */
        if ((DSW(rec) & 0x8000u) != 0)              /* 0x14117/0x1412E */
            local_a = w - local_a - 1;              /* 0x14132..0x14137 */
    }
    /* PORT: the raw dereferences the sprite unconditionally; a failed resolve
     * (res_resolve NULL) is treated as a zero-origin, zero-size sprite rather
     * than reading mem[]. */
    s32 x1 = camera_project_axis((s32)DSD(rec + 4u), 0xF3Du) - local_a;
    s32 y1 = camera_project_axis((s32)DSD(rec + 8u), 0xD56u) - local_b;
    rect[0] = x1;                                   /* 0x1415B/0x1420D */
    rect[1] = y1;                                   /* 0x14180/0x14231 */
    rect[2] = x1 + w;                               /* 0x1418D/0x1423D */
    rect[3] = y1 + h;                               /* 0x141A7/0x14252 */
}

/* 0x140E4. Returns 1 iff the two actors' screen boxes overlap. It writes no
 * global of its own (0x1B544's handle expansion aside). 0x17580 uses it as the
 * 0x17698 gate for the 0x170A0 tail. */
int camera_box_overlap(u32 actor0, u32 actor1)
{
    int rect0[4], rect1[4];
    camera_actor_rect(actor0, rect0);
    camera_actor_rect(actor1, rect1);
    return camera_rect_clip(rect0, rect0, rect1);
}

void camera_decay(void)
{
    /* 0x17585: four 16-bit countdowns, decremented only when non-zero. */
    if (DSW(DS_00107824) != 0) DSW(DS_00107824) = (u16)(DSW(DS_00107824) - 1u);
    if (DSW(DS_001078B8) != 0) DSW(DS_001078B8) = (u16)(DSW(DS_001078B8) - 1u);
    if (DSW(DS_00107826) != 0) DSW(DS_00107826) = (u16)(DSW(DS_00107826) - 1u);
    if (DSW(DS_001078BA) != 0) DSW(DS_001078BA) = (u16)(DSW(DS_001078BA) - 1u);

    DSD(DS_00100B54) = 0;                              /* 0x175E4 */
    DSD(DS_00100AF8) = 0;                              /* 0x175EA */
    DSD(DS_00100AFC) = 0;                              /* 0x175F0 */
    DSD(DS_00100B08) = (u32)camera_scale(DSD(DS_00100B08), 0xF3Du);
    DSD(DS_00100B0C) = (u32)camera_scale(DSD(DS_00100B0C), 0xF3Du);
    DSD(DS_00100B00) = (u32)camera_scale(DSD(DS_00100B00), 0xD56u);
    DSD(DS_00100B04) = (u32)camera_scale(DSD(DS_00100B04), 0xD56u);

    /* 0x17680..0x176C4: the 0x140E4 box-overlap gate. The raw reads slot0's
     * actor index (0x17680/0x1768F) and slot1's (0x17685), and when the boxes
     * overlap runs 0x170A0(0) for B60 != 0 (0x176AC) and 0x170A0(1) for
     * B61 != 0 (0x176BF). PORT: 0x170A0 and its bit-plane callees are unported
     * (the fighter screen-sync half; record §1.6) — the tail is skipped and
     * AF8/AFC keep the zeroes above. */
    if (camera_box_overlap(DSW(DSD(DS_001077B0) + 0x56u),
                           DSW(DSD(DS_00107844) + 0x56u)) != 0) {
        /* TODO(verify): 0x176AC/0x176BF 0x170A0(0)/0x170A0(1) — named gap. */
    }
}

/* ---- 0x16D58 the per-side screen base ---------------------------------- */

void camera_screen_base(s32 side, s32 character)
{
    s16 s = (s16)side;
    s16 c = (s16)character;
    if (s < 0 || s > 1) return;                        /* 0x16D59/0x16D61 */
    if (c < 0 || c >= 0xA) return;                     /* 0x16D66/0x16D6E */
    DSD(DS_00100A70 + (u32)s * 4u) = (u32)c * 0x400u + 0xCC300u;
    DSD(DS_00100A98 + (u32)s * 4u) = (u32)c * 0x3E8u + 0xC9BF0u;
}

/* ---- 0x1282C + 0x12DA8 -------------------------------------------------- */

/* 0x1282C. The rare dust spawn: gated on (DS_000EF6DC & 0x3F) == 0, then
 * rng(7) & 3 == 0; draws rng(7)/rng(0x1300)/rng(0x2000) and spawns one actor
 * from 0xBB254. */
static void camera_dust_spawn(void)
{
    if ((DSW(DS_000EF6DC) & 0x3Fu) != 0) return;       /* 0x1283E */
    u32 draw = rng_next(7u);                           /* 0x1284E */
    if ((draw & 3u) != 0) return;                      /* 0x12855 */
    s32 off; s32 yvel; u32 flag5;
    if ((draw & 4u) != 0) {                            /* 0x1285B */
        off = 0x2800; yvel = (s32)0xFFFFFF80; flag5 = 0u;
    } else {                                           /* 0x12869 */
        off = (s32)0xFFFFD800; yvel = 0x80; flag5 = 0x4000u;
    }
    s32 r1 = (s32)rng_next(0x1300u);                   /* 0x1287D */
    s32 r2 = (s32)rng_next(0x2000u);                   /* 0x12898 */
    s32 a2 = (s32)DSD(DS_000F0AF0) + off;
    s32 a3 = (s16)r1;
    s32 a4 = (s16)((0x1300 - r1) + r2);
    u32 rec = actor_spawn((const u32 *)(mem + CAMERA_DUST_DESC), (u32)a2,
                          (u32)a3, (u32)a4, flag5);
    /* PORT: the raw trusts EAX (0x128C5); the port guards the pool-exhaustion
     * 0 rather than write the word before mem[]. */
    if (rec != 0) DSW(rec + 0x34u) = (u16)yvel;        /* 0x128C5 */
}

/* 0x12DA8. The selected player y: mode 0 reads slot[DS_000F0AFF]+0x30's word,
 * else the signed max of DS_001077E0/DS_00107874; store its low word to
 * DS_001078F4 and run the y clamp. */
static void camera_y_commit(void)
{
    s32 y;
    if (DSB(DS_000F0AFE) == 0) {
        y = (s32)(s16)DSW(DS_001077E0 + (u32)DSB(DS_000F0AFF) * 0x94u);
    } else {
        s32 a = (s32)DSD(DS_001077E0);
        s32 b = (s32)DSD(DS_00107874);
        y = (a > b) ? a : b;
    }
    DSW(DS_001078F2 + 2u) = (u16)y;                    /* 0x12DE3: DS_001078F2's high word */
    camera_y_clamp();                                  /* 0x12DE9 */
}

void camera_scene_step(void)
{
    camera_dust_spawn();                               /* 0x1282C */
    camera_y_commit();                                 /* 0x12DA8 */
}
