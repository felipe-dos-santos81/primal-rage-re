/* Port of the fight camera state machine and the per-frame projection.
 * Addresses, widths and signedness are from
 * docs/superpowers/plans/2026-09-20-demo-fight-derivations.md §1-§3 and the
 * previous cycle's docs/superpowers/plans/2026-09-20-frontend-chain-derivations.md
 * §5. The module registers nothing: the update table DS_000A8644 has no camera
 * entry, and 0x1324C (the shake decay) is registered by effects.c. */
#include "game/camera.h"
#include "game/actors.h"
#include "game/fighter.h"
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

/* 0x80580: the 32-bit float `00 00 80 BF` = -1.0f the 0x164C0 last-frame test
 * adds to the record's +0x24 before comparing with +0x20. Not a named global. */
#define CAMERA_NEG_ONE 0x00080580u

/* 0x98688 / 0x96108: the two 5-entry x 3-byte scan tables 0x16AFC / 0x164F4
 * index by (character * 0x3C0 + slot+0x5F * 15). Not named globals. */
#define CAMERA_CODE_TABLE_A 0x00098688u
#define CAMERA_CODE_TABLE_B 0x00096108u

/* 0xCC300: the per-character 256-dword screen-x table; the tails index it by
 * (character << 8) + code. camera_screen_base reads the same base as char*0x400. */
#define CAMERA_FRAME_TABLE 0x000CC300u

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

/* A resolved resource pointer's mem[] offset, or 0 when 0x1B544 failed. The
 * raw dereferences the pointer unconditionally; the port keeps its existing
 * resource-failure guard (cf. camera_resolve_sprite). */
static u32 camera_res_off(const void *p)
{
    return p != NULL ? (u32)((const u8 *)p - mem) : 0u;
}

/* 0x16308. res_resolve(DSD(0xA8B30 + (0x17EEC(side) + index) * 4)). The raw
 * tail-jumps 0x1B544; a failed resolve returns the 0 sentinel. */
static u32 camera_resolve_sprite(u32 side, u32 index)
{
    u32 handle = DSD(DS_000A8B30
                     + (camera_char_const(DSB(DS_0010782A + side * 0x94u))
                        + index) * 4u);
    return camera_res_off(res_resolve(handle));
}

/* ---- the 0x17FA0 page-flag/visibility tail ------------------------------ */

/* 0x164C0. 1 iff (rec+0x24) + (-1.0f) == rec+0x20 for slot[side]'s record. The
 * raw loads the float at 0x80580 (`00 00 80 BF` = -1.0), adds it and FCOMPs
 * against +0x20. */
static int camera_frame_last(u32 side)
{
    u32 rec = DSD(DS_001077B0 + side * 0x94u);
    float a = *(const float *)(mem + rec + 0x24u)
              + *(const float *)(mem + CAMERA_NEG_ONE);
    float b = *(const float *)(mem + rec + 0x20u);
    return a == b;
}

/* 0x16734. The per-character sprite-id map: switch on slot[side]'s character
 * (slot+0x7A), comparing the actor's sprite id (word[actor] & 0x7FFF) against
 * the case's literal ids; returns a code 0xD2..0xDA or -1. Its only caller is
 * 0x16AFC. 0x33A10's context supplies the self record (out[5]) and slot (out[3]). */
int camera_sprite_code(u32 side)
{
    u32 ctx[6];
    fighter_ctx_swap(ctx, side);                    /* 0x33A10 */
    u32 rec = ctx[5];                               /* 0x16741 */
    u32 actor = DSD(DS_001014EC) + (u32)DSW(rec + 0x56u) * 0x20u;
    u32 u = (u32)DSW(actor) & 0x7FFFu;
    u32 ch = (u32)DSB(ctx[3] + 0x7Au);              /* 0x16764 */

    switch (ch) {
    case 0:
        if (u == 0xF9Fu) return 0xD8;
        if (u == 0xFA1u) return 0xD5;
        if (u == 0xFA2u) return 0xD6;
        if (u >= 0xF9Cu && u <= 0xF9Eu) return 0xD7;
        if (u >= 0xF98u && u <= 0xF9Bu) return 0xD4;
        break;
    case 1:
        if (u == 0x134Fu || u == 0x1350u) return 0xD5;
        if (u == 0x1351u || u == 0x1352u || u == 0x1353u) return 0xD4;
        if (u == 0x1354u || u == 0x1355u) return 0xD4;
        if (u == 0x1356u || u == 0x1357u || u == 0x1358u) return 0xD6;
        break;
    case 2:
        if (u == 0xC48u || u == 0xC49u) return 0xD4;
        if (u == 0xC50u) return 0xD5;
        break;
    case 3:
        if (u == 0x1745u || u == 0x1746u || u == 0x1747u) return 0xD2;
        if (u == 0x174Eu || u == 0x174Fu) return 0xD3;
        if (u == 0x1750u) return 0xD5;
        if (u == 0x1751u || u == 0x1752u) return 0xD6;
        break;
    case 4:
        if (u == 0x2024u || u == 0x2025u) return 0xD8;
        if (u == 0x2026u || u == 0x2027u || u == 0x2028u) return 0xD9;
        if (u >= 0x2029u && u <= 0x202Bu) return 0xDA;
        break;
    case 5:
        if (u == 0xFA1u || u == 0xFA2u) return 0xD3;
        if (u >= 0xF98u && u <= 0xF9Bu) return 0xD5;
        if (u >= 0xF9Cu && u <= 0xF9Fu) return 0xD4;
        break;
    case 6:
        if (u == 0x134Fu || u == 0x1350u) return 0xD2;
        if (u == 0x1351u || u == 0x1353u) return 0xD3;
        if (u == 0x1352u || u == 0x1354u || u == 0x1355u || u == 0x1356u) return 0xD3;
        if (u == 0x1357u || u == 0x1358u) return 0xD4;
        break;
    default:
        break;
    }
    return -1;
}

/* 0x16AFC. The first page-flag tail 0x17FA0 calls for a side: write the side's
 * screen box to `out` and return 1, or zero `out` and return 0. With a 0x16734
 * code it reads the 0xCC300 frame table; otherwise it runs the per-side
 * countdown (bits 2/3 of DS_00107EE0) and the 5-entry 0x98688 scan keyed by
 * (slot+0x53, slot+0x5F, rec+0x63, 0x164C0), caching into 0xFD120..0xFD138. */
int camera_page_tail_a(u32 side, u32 out)
{
    int code = camera_sprite_code(side);
    if (code != -1) {                               /* 0x16b14 */
        u32 ch = (u32)DSB(DS_0010782A + side * 0x94u);
        u32 addr = CAMERA_FRAME_TABLE + ((ch << 8) + (u32)code) * 4u;
        DSD(out) = DSD(addr);
        if (!camera_actor_bit15_clear(side))
            DSB(out) = (u8)(0x40u - DSB(addr) - DSB(addr + 2u));
        return 1;
    }

    /* 0x16b62: the code-less path's per-side countdown, gated by 0x3C570. */
    if (fighter_slot_flag(side == 0u ? 2u : 3u) == 0) {
        s32 v = (s32)DSD(DS_000FD128 + side * 4u) - 1;
        DSD(DS_000FD128 + side * 4u) = (u32)(v < 0 ? 0 : v);
    }

    {
        u32 st = (u32)DSB(DS_00107803 + side * 0x94u);
        if (st != 8u && st != 7u) return 0;         /* 0x16bce */
    }
    {
        u32 s5f = (u32)DSB(DS_0010780F + side * 0x94u);
        if (s5f == 0xFFu) return 0;                 /* 0x16bfd */
        u32 ch = (u32)DSB(DS_0010782A + side * 0x94u);
        u32 entry = CAMERA_CODE_TABLE_A + ch * 0x3C0u + s5f * 15u;
        u32 rec = DSD(DS_001077B0 + side * 0x94u);
        u32 r63 = (u32)DSB(rec + 0x63u);
        for (u32 i = 0; i < 5u; i++) {              /* 0x16c35 */
            if ((s32)r63 != (s32)(s8)DSB(entry + i * 3u)) continue;
            if (!camera_frame_last(side)) continue;
            u32 e1 = (u32)DSB(entry + i * 3u + 1u);
            u32 addr = CAMERA_FRAME_TABLE + ((ch << 8) + e1) * 4u;
            DSD(out) = DSD(addr);
            if (!camera_actor_bit15_clear(side))
                DSB(out) = (u8)(0x40u - DSB(addr) - DSB(addr + 2u));
            DSD(DS_000FD138 + side * 4u) = DSD(out);            /* 0x16c98 */
            DSD(DS_000FD128 + side * 4u) = (u32)DSB(entry + i * 3u + 2u);
            DSD(DS_000FD120 + side * 4u) = e1;
            DSD(DS_000FD130 + side * 4u) = DSD(DS_00100AF0 + side * 4u);
            DSW(DS_00100B3C + side * 2u) = DSW(0x00107834u + side * 0x94u);
            return 1;
        }
        /* 0x16d0c: the cached-box path. */
        if ((s32)DSD(DS_000FD128 + side * 4u) <= 0) return 0;
        if (DSB(DS_000FD138 + side * 4u + 2u) == 0u) return 0;
        if (DSW(DS_00100B3C + side * 2u)
            != DSW(0x00107834u + side * 0x94u)) return 0;
        DSD(out) = DSD(DS_000FD138 + side * 4u);
        return 1;
    }
}

/* 0x164F4. The second page-flag tail 0x17FA0 calls: the 0x16AFC twin that
 * skips 0x16734, uses bits 0/1 of DS_00107EE0, scans the 0x96108 table and
 * caches into 0xFD140..0xFD158 (its cached-box path also requires rec+0x63 to
 * be >= the matched value). Writes the side's screen box to `out`. */
int camera_page_tail_b(u32 side, u32 out)
{
    if (fighter_slot_flag(side == 0u ? 0u : 1u) == 0) {      /* 0x16500 */
        s32 v = (s32)DSD(DS_000FD148 + side * 4u) - 1;
        DSD(DS_000FD148 + side * 4u) = (u32)(v < 0 ? 0 : v);
    }

    {
        u32 st = (u32)DSB(DS_00107803 + side * 0x94u);
        if (st != 8u && st != 7u) return 0;         /* 0x16567 */
    }
    {
        u32 s5f = (u32)DSB(DS_0010780F + side * 0x94u);
        if (s5f == 0xFFu) return 0;                 /* 0x16594 */
        u32 ch = (u32)DSB(DS_0010782A + side * 0x94u);
        u32 entry = CAMERA_CODE_TABLE_B + ch * 0x3C0u + s5f * 15u;
        u32 rec = DSD(DS_001077B0 + side * 0x94u);
        u32 r63 = (u32)DSB(rec + 0x63u);
        for (u32 i = 0; i < 5u; i++) {              /* 0x165ca */
            if ((s32)r63 != (s32)(s8)DSB(entry + i * 3u)) continue;
            if (!camera_frame_last(side)) continue;
            u32 e1 = (u32)DSB(entry + i * 3u + 1u);
            u32 addr = CAMERA_FRAME_TABLE + ((ch << 8) + e1) * 4u;
            DSD(out) = DSD(addr);
            if (!camera_actor_bit15_clear(side))
                DSB(out) = (u8)(0x40u - DSB(addr) - DSB(addr + 2u));
            DSD(DS_000FD158 + side * 4u) = DSD(out);            /* 0x1662d */
            DSD(DS_000FD148 + side * 4u) = (u32)DSB(entry + i * 3u + 2u);
            DSD(DS_000FD140 + side * 4u) = e1;
            DSD(DS_000FD150 + side * 4u) = DSD(DS_00100AF0 + side * 4u);
            DSW(DS_00100B44 + side * 2u) = DSW(0x00107834u + side * 0x94u);
            DSW(DS_00100B48 + side * 2u) = (u16)r63;
            return 1;
        }
        /* 0x166b6: the cached-box path (the extra rec+0x63 >= cache check). */
        if ((s32)DSD(DS_000FD148 + side * 4u) <= 0) return 0;
        if (DSB(DS_000FD158 + side * 4u + 2u) == 0u) return 0;
        if (DSW(0x00107834u + side * 0x94u)
            != DSW(DS_00100B44 + side * 2u)) return 0;
        if ((s32)r63 < (s32)(u32)DSW(DS_00100B48 + side * 2u)) return 0;
        DSD(out) = DSD(DS_000FD158 + side * 4u);
        return 1;
    }
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

        /* 0x18092: *page_flag = 0; the 0x164F4 tail may raise it to 1 below. */
        DSB(page_flag) = 0;

        u32 actor = DSD(DS_001014EC) + (u32)DSW(p + 0x56u) * 0x20u;

        /* 0x180A1: *index_out = (actor.word0 & 0x7FFF) - 0x17EEC(side). */
        {
            u32 base = (u32)DSW(actor) & 0x7FFFu;
            DSD(index_out) = base
                - camera_char_const(DSB(DS_0010782A + side * 0x94u));
        }

        /* 0x180C9/0x18108: the page-flag/visibility tail. 0x16AFC copies
         * DS_00100A78[side] into DS_00100AC0[side] (or zeroes it); 0x164F4 does
         * the same from DS_00100A90[side] into DS_00100AC8[side] and raises
         * *page_flag on success. Both read the just-written DS_00100AF0[side]. */
        if (camera_page_tail_a(side, DS_00100A78 + side * 4u) != 0)
            DSD(DS_00100AC0 + side * 4u) = DSD(DS_00100A78 + side * 4u);
        else
            mem_fill(DS_00100AC0 + side * 4u, 0, 4u);
        if (camera_page_tail_b(side, DS_00100A90 + side * 4u) != 0) {
            DSB(page_flag) = 1;
            DSD(DS_00100AC8 + side * 4u) = DSD(DS_00100A90 + side * 4u);
        } else {
            mem_fill(DS_00100AC8 + side * 4u, 0, 4u);
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

/* The sprite-origin rect 0x140E4/0x15C30 share: the 0xA8B30 handle is resolved
 * by 0x1B544 with the raw index `word[actor] & 0x7FFF` (no 0x17EEC character
 * constant, unlike 0x16308); the origin's +2/+4 halves and the bit-15 flip feed
 * the 0xF3D/0xD56 projection of the actor's +4/+8 into x1/y1/x2/y2.
 * PORT: the raw dereferences the sprite unconditionally; a failed resolve
 * (res_resolve NULL) is treated as a zero-origin, zero-size sprite rather than
 * reading mem[]. */
static void camera_sprite_rect(u32 actor, int rect[4])
{
    u32 off = camera_res_off(res_resolve(
        DSD(DS_000A8B30 + ((u32)DSW(actor) & 0x7FFFu) * 4u)));
    s32 local_a = 0, local_b = 0, w = 0, h = 0;
    if (off != 0u) {
        local_a = (s32)DSD(off + 2u) >> 16;         /* 0x14114/0x14123 */
        local_b = (s32)DSD(off + 4u) >> 16;         /* 0x1411A/0x1412B */
        w = (s32)(s16)DSW(off);                     /* 0x14132/0x14188 */
        h = (s32)DSD(off) >> 16;                    /* 0x14194/0x1419A */
        if ((DSW(actor) & 0x8000u) != 0)            /* 0x14117/0x1412E */
            local_a = w - local_a - 1;              /* 0x14132..0x14137 */
    }
    s32 x1 = camera_project_axis((s32)DSD(actor + 4u), 0xF3Du) - local_a;
    s32 y1 = camera_project_axis((s32)DSD(actor + 8u), 0xD56u) - local_b;
    rect[0] = x1;                                   /* 0x1415B/0x1420D */
    rect[1] = y1;                                   /* 0x14180/0x14231 */
    rect[2] = x1 + w;                               /* 0x1418D/0x1423D */
    rect[3] = y1 + h;                               /* 0x141A7/0x14252 */
}

/* 0x140E4's per-actor rect (0x140EE..0x141A7 for the first actor,
 * 0x141AB..0x1425A for the second). */
static void camera_actor_rect(u32 actor_idx, int *rect)
{
    camera_sprite_rect(DSD(DS_001014EC) + actor_idx * 0x20u, rect);
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

/* ---- the unfreeze half: 0x15EC0..0x170A0 ------------------------------- */

/* The three data-object tables the bit-plane helpers index. They live in the
 * data object (0x80000..0x10B0CF), so mem[] holds them; the addresses are the
 * raw's immediates. 0xA1420 is the high-bit mask pair (0x00,0x80,..,0xFE), its
 * +8/+9 tails are the low-bit mask (0xFF,0x00,0x01,..,0x7F,0xFF); 0xA1432 is
 * the 256-byte bit-reverse table; 0xA163C is the 256-byte popcount table. */
#define CAMERA_MASK_HI   0x000A1420u
#define CAMERA_MASK_LO   0x000A1429u   /* 0xA1431 - k == 0xA1429 + (8-k) */
#define CAMERA_BITREV    0x000A1432u
#define CAMERA_POPCOUNT  0x000A163Cu

/* 0x15EC0. Reverse the bits of each byte of buf[0..len) in place, through the
 * 0xA1432 table. The raw's return is the half length or a table byte and is
 * ignored by the only caller (0x16DA4). */
static void camera_bitrev(u32 buf, u32 len)
{
    u32 half = (u32)((s32)len >> 1);
    u32 lo = buf, hi = buf + len - 1u;
    for (u32 i = 0; i < half; i++) {
        u8 a = DSB(lo);
        u8 b = DSB(hi);
        DSB(lo) = DSB(CAMERA_BITREV + b);
        DSB(hi) = DSB(CAMERA_BITREV + a);
        lo++; hi--;
    }
    if (((s32)len & 1) != 0) DSB(lo) = DSB(CAMERA_BITREV + DSB(lo));
}

/* 0x1638C. Copy `count` bytes from the per-side row table (0xFD160 side 0 /
 * 0xFEDE0 side 1, stride 0x26) at row `row` to dst. */
static void camera_sprite_row(u32 side, u32 dst, u32 count, u32 row)
{
    u32 src = (side == 0u ? 0x000FD160u : 0x000FEDE0u) + row * 0x26u;
    for (u32 i = 0; i < count; i++) DSB(dst + i) = DSB(src + i);
}

/* 0x41030. Decode `count` RLE-encoded rows of the sprite at `handle` (starting
 * at `frame`) into the per-side row table `table`, and return the sprite's byte
 * width ((s16)word[sprite] + 7) >> 3. The sprite's dword +8 is a second handle
 * to the pixel-data stream. */
static u32 camera_bitplane_accum(u32 handle, u32 frame, u32 count, u32 table)
{
    const u8 *p = (const u8 *)res_resolve(handle);
    u32 sp = camera_res_off(p);
    const u8 *s = (const u8 *)res_resolve(DSD(sp + 8u));
    u32 si = (u32)(s - mem);
    s32 width = (s32)(s16)DSW(sp);                 /* 0x41050 */
    u32 byte_width = (u32)((width + 7) / 8);       /* 0x41062 */

    /* 0x4106f: skip `frame` rows. */
    for (u32 k = 0; k < frame; k++) {
        s32 ww = (s32)(s16)DSW(sp);
        while (ww > 0) {
            u8 b = DSB(si); si++;
            if ((b & 0x80u) != 0) {
                u32 rl = (u32)(b & 0x3fu);
                ww -= (s32)rl;
                if ((b & 0x40u) == 0u) si++;
            } else {
                u32 rl = (u32)(b & 0x7fu);
                si += rl;
                ww -= (s32)rl;
            }
        }
    }

    /* 0x410b8: decode `count` rows. */
    if ((s32)count > 0) {
        u32 row_off = 0;
        u32 total = count * 0x26u;
        do {
            u32 dst = table + row_off;
            mem_fill(dst, 0, byte_width);
            s32 ww = (s32)(s16)DSW(sp);
            u8 bit = 0x80u;
            while (ww > 0) {
                u8 b = DSB(si); si++;
                if ((b & 0x80u) != 0) {
                    u32 rl = (u32)(b & 0x3fu);
                    ww -= (s32)rl;
                    if ((b & 0x40u) == 0u) {
                        si++;
                        for (u32 i = 0; i < rl; i++) {
                            DSB(dst) |= bit;
                            bit >>= 1;
                            if (bit == 0u) { bit = 0x80u; dst++; }
                        }
                    } else {
                        for (u32 i = 0; i < rl; i++) {
                            bit >>= 1;
                            if (bit == 0u) { bit = 0x80u; dst++; }
                        }
                    }
                } else {
                    u32 rl = (u32)(b & 0x7fu);
                    ww -= (s32)rl;
                    si += rl;
                    for (u32 i = 0; i < rl; i++) {
                        DSB(dst) |= bit;
                        bit >>= 1;
                        if (bit == 0u) { bit = 0x80u; dst++; }
                    }
                }
            }
            row_off += 0x26u;
        } while (row_off < total);
    }
    return byte_width;
}

/* 0x1631C. Clamp `frame`/`count` to the sprite's height, then decode those rows
 * into the side's row table. Returns the sprite's byte width. */
static u32 camera_sprite_height(u32 side, u32 index, u32 frame, u32 count)
{
    u32 ch = DSB(DS_0010782A + side * 0x94u);
    u32 handle = DSD(DS_000A8B30
                     + (camera_char_const(ch) + index) * 4u);
    const u8 *p = (const u8 *)res_resolve(handle);
    s32 height = (s32)DSD(camera_res_off(p)) >> 16;
    if ((s32)frame >= height) { count = 1u; frame = (u32)(height - 1); }
    if ((s32)(frame + count) > height) count = (u32)(height - (s32)frame);
    u32 table = (side == 0u ? 0x000FD160u : 0x000FEDE0u);
    return camera_bitplane_accum(handle, frame, count, table);
}

/* 0x15F48. Fill the first (bit/8) bytes of dst with `value`, then set dst[bit/8]
 * to the 0xA1420 mask for bit%8 (unless the bit is byte-aligned). `bit` is the
 * raw's EAX, `rows` the EDX, dst the EBX, value the CL. */
static void camera_bitplane_pixel(u32 bit, u32 rows, u32 dst, u8 value)
{
    if ((s32)bit < 0 || (s32)bit > (s32)(rows * 8u) || bit == 0u) return;
    u32 byte_idx = (u32)((s32)bit / 8);
    u32 bit_idx = (u32)((s32)bit % 8);
    u8 mask = DSB(CAMERA_MASK_HI + bit_idx);
    mem_fill(dst, value, byte_idx);
    if (bit_idx != 0u) DSB(dst + byte_idx) = mask;
}

/* 0x15FD4. Right-shift a bit-plane row buffer `src` into `dst` by `bit` bits,
 * carrying the next byte's low bits, then shift `dst` by bit/8. `rows` is the
 * byte count and must equal the raw's stack arg (arg5). */
static int camera_bitplane_shift(u32 bit, u32 src, u32 rows, u32 dst, u32 arg5)
{
    if ((s32)bit < 0 || (s32)bit > (s32)(rows * 8u)) return 0;
    if ((s32)rows < 1 || (s32)arg5 < 1 || arg5 != rows) return 0;

    u32 byte_count = 0, bit_idx = 0, shift = 0, t_lo = 0, t_hi = 0;
    if (bit == 0u) {
        byte_count = 0; bit_idx = 0;
    } else if (bit == 0x128u) {
        byte_count = 0x25u; shift = arg5 ^ rows;
    } else {
        byte_count = (u32)((s32)bit / 8);
        bit_idx = (u32)((s32)bit % 8);
        shift = 8u - bit_idx;
        t_lo = DSB(CAMERA_MASK_LO + (8u - bit_idx));
        t_hi = DSB(CAMERA_MASK_HI + bit_idx);
    }

    s32 i = (s32)rows - 1;
    u32 s = src + (u32)i, d = dst + (u32)i;
    if (bit_idx == 0u) {
        for (; i >= 0; i--) { DSB(d) = DSB(s); s--; d--; }
    } else {
        for (; i >= 0; i--) {
            if (i == 0)
                DSB(d) = (u8)(t_lo & (DSB(s) >> bit_idx));
            else
                DSB(d) = (u8)(((DSB(s) >> bit_idx) & t_lo)
                              | ((DSB(s - 1u) << shift) & t_hi));
            s--; d--;
        }
    }
    if (byte_count != 0u) {
        u32 p = dst + rows - 1u;
        u32 k = 0;
        u32 q = p - byte_count;
        if ((s32)rows > 0) {
            do {
                DSB(p) = (byte_count < rows) ? DSB(q) : 0u;
                p--; byte_count++; k++; q--;
            } while (k < rows);
        }
    }
    return 1;
}

/* 0x1617C. Left-shift a bit-plane row buffer `src` into `dst` by `bit` bits,
 * carrying the next byte's high bits, then shift `dst` by bit/8. */
static int camera_bitplane_merge(u32 bit, u32 src, u32 rows, u32 dst, u32 arg5)
{
    if ((s32)bit < 0 || (s32)bit > (s32)(rows * 8u)) return 0;
    if ((s32)rows < 1 || (s32)arg5 < 1 || arg5 != rows) return 0;

    u32 byte_count = 0, bit_idx = 0, shift = 0, t_hi = 0, t_lo = 0;
    if (bit == 0u) {
        byte_count = 0; bit_idx = 0;
    } else if (bit == 0x128u) {
        byte_count = 0x25u; bit_idx = 0;
    } else {
        byte_count = (u32)((s32)bit / 8);
        bit_idx = (u32)((s32)bit % 8);
        shift = 8u - bit_idx;
        t_hi = DSB(CAMERA_MASK_LO + bit_idx);
        t_lo = DSB(CAMERA_MASK_HI + shift);
    }

    u32 s = src, d = dst;
    if (bit_idx == 0u) {
        for (u32 i = 0; i < rows; i++) { DSB(d) = DSB(s); s++; d++; }
    } else {
        for (u32 i = 0; i < rows; i++) {
            if (i == rows - 1u)
                DSB(d) = (u8)(t_lo & (DSB(s) << bit_idx));
            else
                DSB(d) = (u8)((t_lo & (DSB(s) << bit_idx))
                              | (t_hi & (DSB(s + 1u) >> shift)));
            s++; d++;
        }
    }
    if (byte_count != 0u && (s32)rows > 0) {
        u32 p = dst + byte_count;
        for (u32 i = 0; i < rows; i++) {
            DSB(dst + i) = (byte_count < rows) ? DSB(p) : 0u;
            byte_count++; p++;
        }
    }
    return 1;
}

/* 0x15B90. Read the box palette/width pair at (0x17EEC(side) + index) from
 * 0xD5100/0xD5101 into the two out pointers, adjust chars 5/6, then mirror the
 * x out when the actor's bit 15 is clear. */
static void camera_box_palette(u32 side, u32 *out_a, u32 *out_b, u32 index)
{
    u32 ch = DSB(DS_0010782A + side * 0x94u);
    u32 base = camera_char_const(ch) + index;
    *out_a = DSB(0x000D5100u + base * 2u);
    *out_b = DSB(0x000D5101u + base * 2u);
    if (ch == 5u || ch == 6u) {
        s32 da = (s32)(0x7fu - *out_a) / 8;
        s32 db = (s32)(0x5fu - *out_b) / 8;
        *out_a = (u32)((s32)*out_a + da);
        *out_b = (u32)((s32)*out_b + db);
    }
    if (!camera_actor_bit15_clear(side)) *out_a = 0x100u - *out_a;
}

/* 0x15C30. Project the side's actor sprite rect and clip the 4-byte box at
 * `box` against it, scaled by the palette offsets 0x15B90 returns. */
static void camera_box_clip(u32 side, u32 index, u8 *box)
{
    u32 rec = DSD(DS_001077B0 + side * 0x94u);
    u32 actor = DSD(DS_001014EC) + (u32)DSW(rec + 0x56u) * 0x20u;
    int rect[4];
    camera_sprite_rect(actor, rect);
    s32 x1 = rect[0], y1 = rect[1];                /* 0x15c76..0x15d08 */
    s32 x2 = rect[2], y2 = rect[3];                /* 0x15d19 */

    u32 out_a, out_b;
    camera_box_palette(side, &out_a, &out_b, index);

    s32 dx = camera_project_axis((s32)DSD(actor + 4u), 0xF3Du)
             - camera_scale(out_a, 0xF3Du);
    s32 dy = camera_project_axis((s32)DSD(actor + 8u), 0xD56u)
             - camera_scale(out_b, 0xD56u);

    box[0] = (u8)(box[0] << 2);      /* 0x15da3 */
    box[1] = (u8)(box[1] * 3u);      /* 0x15da6 */
    box[2] = (u8)(box[2] << 2);      /* 0x15db0 */
    box[3] = (u8)(box[3] * 3u);      /* 0x15db4 */

    u8 c3 = (u8)((u8)x1 - (u8)dx);                 /* 0x15dc4 */
    u8 c4 = (u8)((u8)y1 - (u8)dy);                 /* 0x15dd2 */

    s32 acc = (s32)box[0] + dx;             /* 0x15dc6 */
    if (acc < x1) {                                /* 0x15dd6 */
        if (acc + (s32)box[2] < x1) {
            box[0] = 0; box[2] = 0;
        } else {
            u8 b0 = box[0];
            box[0] = 0;
            box[2] = (u8)(box[2] - (u8)(c3 - b0));
        }
    } else {
        box[0] = (u8)(box[0] - c3);
    }
    if (x2 < (s32)((s32)box[0] + x1)) {     /* 0x15e10 */
        box[0] = 0; box[2] = 0;
    } else {
        s32 t = (s32)box[0] + x1 + (s32)box[2];
        if (x2 < t) box[2] = (u8)(box[2] - (u8)(t - x2));
    }

    s32 accy = (s32)box[1] + dy;            /* 0x15e3f */
    if (accy < y1) {
        if (accy + (s32)box[3] < y1) {
            box[1] = 0; box[3] = 0;
        } else {
            u8 b1 = box[1];
            box[1] = 0;
            box[3] = (u8)(box[3] - (u8)(c4 - b1));
        }
    } else {
        box[1] = (u8)(box[1] - c4);
    }
    if (y2 < (s32)((s32)box[1] + y1)) {     /* 0x15e85 */
        box[1] = 0; box[3] = 0;
    } else {
        s32 t = (s32)box[1] + y1 + (s32)box[3];
        if (y2 < t) box[3] = (u8)(box[3] - (u8)(t - y2));
    }
}

/* 0x181D0. Clip the item [p3, p3+p1) against the container [0, p2) and write
 * the left clip to *p5, the right clip to *p6, the start to *p7, the residual
 * to *p8 and the visible extent to *p4. Returns 0 when the item is visible,
 * 1 when empty, 2/3 when either dimension is non-positive. */
static int camera_sync_visible(u32 p1, u32 p2, s32 p3, u32 p4,
                               u32 p5, u32 p6, u32 p7, u32 p8)
{
    if ((s32)p1 < 1) return 2;                         /* 0x181dd */
    if ((s32)p2 < 1) return 3;                         /* 0x181e9 */
    if (p3 < 0 && -p3 >= (s32)p1) return 1;            /* 0x181f6 */
    if ((s32)p2 < p3) return 1;                        /* 0x1820a */
    if (p3 < 0) {                                      /* 0x18250 */
        DSD(p7) = 0;
        DSD(p5) = (u32)(-p3);
        if ((s32)p2 < (s32)p1 - (s32)DSD(p5)) {
            DSD(p6) = (u32)((s32)p1 - (s32)DSD(p5) - (s32)p2);
            DSD(p8) = 0;
            goto done;
        }
        DSD(p6) = 0;
        p2 = (u32)((s32)p2 - ((s32)p1 - (s32)DSD(p5)));
    } else {                                           /* 0x1821a */
        DSD(p7) = (u32)p3;
        DSD(p5) = 0;
        if ((s32)p2 < p3 + (s32)p1) {
            DSD(p6) = (u32)(p3 + (s32)p1 - (s32)p2);
            DSD(p8) = 0;
            goto done;
        }
        p2 = (u32)(((s32)p2 - (s32)p1) - p3);
        DSD(p6) = 0;
    }
    DSD(p8) = p2;
done:
    {
        s32 r = (s32)p1 - (s32)DSD(p5) - (s32)DSD(p6);
        DSD(p4) = (u32)r;
        return r == 0 ? 1 : 0;
    }
}

/* 0x16DA4. The DS_00100B54 writer: decode the two fighters' sprite rows, AND
 * their bit-planes, weight the overlap with the popcount table, and scale the
 * sum down. Args: flag = (B14 > B34), af0_side/af0_other the two 0x100AF0
 * indices, b10/b30 the two row-frame bases, arg2 the raw's -1, side the side. */
static void camera_winner_height(u32 flag, u32 af0_side, u32 af0_other,
                                 u32 b10, u32 b30, s32 arg2, u32 side)
{
    u32 other = 1u - side;
    u32 width_a = 0, width_b = 0;
    DSD(DS_00100B54) = 0;                              /* 0x16dc8 */
    if (arg2 != 0)
        width_a = camera_sprite_height(side, af0_side, b10, DSD(DS_00100B18));
    if (arg2 == -1 || arg2 == 0)
        width_b = camera_sprite_height(other, af0_other, b30, DSD(DS_00100B18));
    for (u32 row = 0; (s32)row < (s32)DSD(DS_00100B18); row++) {
        camera_sprite_row(side, DS_00100BAE, width_a, row);
        camera_sprite_row(other, DS_00100B64, width_b, row);
        if (DSB(DS_00100B62 + side) != 0u)            /* 0x16f17 */
            camera_bitrev(DS_00100BAE, width_a);
        if (DSB(DS_00100B62 + other) != 0u)           /* 0x16f2e */
            camera_bitrev(DS_00100B64, width_b);
        for (u32 i = 0; i < width_a; i++)              /* 0x16f48 */
            DSB(DS_00100BAE + i) = (u8)(DSB(DS_00100BAE + i)
                                         & DSB(DS_00100BD3 + i));
        if (flag != 0u) {
            camera_bitplane_merge(DSD(DS_00100B38), DS_00100BAE, 0x25u,
                                  DS_00100BF8, 0x25u);    /* 0x16f9e */
            for (u32 i = 0; i < 0x25u; i++)            /* 0x16fa3 */
                DSB(DS_00100B89 + i) = (u8)(DSB(DS_00100BF8 + i)
                                             & DSB(DS_00100B64 + i));
        } else {
            camera_bitplane_merge(DSD(DS_00100B38), DS_00100B64, 0x25u,
                                  DS_00100BF8, 0x25u);    /* 0x16fde */
            for (u32 i = 0; i < 0x25u; i++)            /* 0x16fe3 */
                DSB(DS_00100B89 + i) = (u8)(DSB(DS_00100BF8 + i)
                                             & DSB(DS_00100BAE + i));
        }
        for (u32 i = 0; i < 0x25u; i++)                /* 0x17001 */
            DSD(DS_00100B54) = DSD(DS_00100B54)
                + DSB(CAMERA_POPCOUNT + DSB(DS_00100B89 + i));
    }
    {
        s32 t = (s32)(DSD(DS_00100B54) << 12) / 0xF3D; /* 0x1703e */
        t = (s32)((u32)t << 12) / 0xD56;               /* 0x17051 */
        DSD(DS_00100B54) = (u32)(t / 16);              /* 0x1706f */
    }
}

/* 0x170A0. The unfreeze half's per-side body: clip the side's screen box
 * against the projected sprite (0x15C30), sync the three visible rectangles
 * (0x181D0), and write DS_00100AF8[side] = DS_00100B54. Guarded on the other
 * side's two countdowns (hit-stun/recovery). */
void camera_unfreeze(u32 side)
{
    u32 other = 1u - side;
    if (DSW(DS_00107824 + other * 0x94u) != 0u) return;      /* 0x170c5 */
    if ((u32)DSW(DS_00107826 + other * 0x94u) > 1u) return;  /* 0x170e2 */

    u32 sp_a = camera_resolve_sprite(side, DSD(DS_00100AF0 + side * 4u));
    u32 sp_b = camera_resolve_sprite(other, DSD(DS_00100AF0 + other * 4u));

    u32 box_s = DS_00100AC8 + side * 4u;
    u32 box_o = DS_00100AC0 + other * 4u;
    if (DSB(box_s + 2u) == 0u || DSB(box_s + 3u) == 0u) return;   /* 0x1715e */
    if (DSB(box_o + 2u) == 0u || DSB(box_o + 3u) == 0u) box_o = 0u; /* 0x17182 */

    u8 box_a[4];
    for (u32 i = 0; i < 4u; i++) box_a[i] = DSB(box_s + i);
    camera_box_clip(side, DSD(DS_00100AF0 + side * 4u), box_a); /* 0x171cc */
    u32 sb1 = box_a[1];                                    /* [ESP+0xc] */
    u8 box_b[4] = { 0u, 0u, 0u, 0u };
    if (box_o != 0u) {
        for (u32 i = 0; i < 4u; i++) box_b[i] = DSB(box_o + i);
        camera_box_clip(other, DSD(DS_00100AF0 + other * 4u), box_b); /* 0x1721a */
    }
    u32 ob1 = box_b[1];                                    /* [ESP+0x24] */

    /* 0x1723e: the facing bit 0x4000 selects whether the box width is added. */
    u32 rec = DSD(DS_001077B0 + side * 0x94u);
    u32 facing = ((u32)DSW(rec + 0x28u) & 0x4000u) != 0u;
    u32 ae0 = DSD(DS_00100B08 + side * 4u) + (u32)box_a[0];
    if (facing) ae0 += (u32)box_a[2];
    DSD(DS_00100AE0 + side * 4u) = ae0;                    /* 0x1728f */
    u32 ad8 = DSD(DS_00100B00 + side * 4u) + (u32)box_a[1]
              + (u32)((s32)box_a[3] / 2);                  /* 0x172c0 */
    DSD(DS_00100AD8 + side * 4u) = ad8;
    DSD(DS_00100AE8 + side * 4u) = ad8 - DSD(DS_00100B00 + other * 4u); /* 0x172e5 */

    /* 0x172eb: the three 0x181D0 syncs; a non-zero return aborts the body. */
    {
        int r;
        if (box_o != 0u) {
            r = camera_sync_visible((u32)box_a[2], (u32)box_b[2],
                                    ((s32)box_a[0]
                                     + (s32)DSD(DS_00100B08 + side * 4u))
                                    - ((s32)box_b[0]
                                       + (s32)DSD(DS_00100B08 + other * 4u)),
                                    DS_00100B1C, DS_00100B14, DS_00100B28,
                                    DS_00100B34, DS_00100B24);
        } else {
            r = camera_sync_visible((u32)box_a[2], (s32)(s16)DSW(sp_b),
                                    (s32)box_a[0]
                                    + (s32)DSD(DS_00100B08 + side * 4u)
                                    - (s32)DSD(DS_00100B08 + other * 4u),
                                    DS_00100B1C, DS_00100B14, DS_00100B28,
                                    DS_00100B34, DS_00100B24);
        }
        if (r != 0) return;                                 /* 0x1736c */
    }

    /* 0x17372: build the 0x25-byte bit-plane of the side's sprite. */
    {
        s32 w = (s32)(s16)DSW(sp_a);
        u32 bw = (u32)(w / 8);
        if ((w % 8) != 0) bw++;                            /* 0x1739c */
        mem_fill(DS_00100BD3, 0, 0x25u);                  /* 0x173ad */
        if (box_a[2] != 0u)
            camera_bitplane_pixel((u32)box_a[2], bw, DS_00100BD3, 0xFFu); /* 0x173cb */
        if (box_a[0] != 0u)
            camera_bitplane_shift((u32)box_a[0], DS_00100BD3, bw,
                                  DS_00100BD3, bw);        /* 0x173e6 */
    }

    {
        int r = camera_sync_visible((s32)(s16)DSW(sp_a), (s32)(s16)DSW(sp_b),
                                    (s32)DSD(DS_00100B08 + side * 4u)
                                    - (s32)DSD(DS_00100B08 + other * 4u),
                                    DS_00100B1C, DS_00100B14, DS_00100B28,
                                    DS_00100B34, DS_00100B24);   /* 0x1743c */
        if (r != 0) return;                                 /* 0x17446 */
    }
    {
        int r;
        if (box_o != 0u) {
            r = camera_sync_visible((u32)box_a[3], (u32)box_b[3],
                                    ((s32)DSD(DS_00100B00 + side * 4u) + (s32)box_a[1])
                                    - ((s32)DSD(DS_00100B00 + other * 4u) + (s32)box_b[1]),
                                    DS_00100B18, DS_00100B10, DS_00100B2C,
                                    DS_00100B30, DS_00100B20);
        } else {
            r = camera_sync_visible((u32)box_a[3], (s32)DSD(sp_b) >> 16,
                                    ((s32)DSD(DS_00100B00 + side * 4u) + (s32)box_a[1])
                                    - (s32)DSD(DS_00100B00 + other * 4u),
                                    DS_00100B18, DS_00100B10, DS_00100B2C,
                                    DS_00100B30, DS_00100B20);
        }
        if (r != 0) return;                                 /* 0x174ef */
    }

    DSD(DS_00100B10) = DSD(DS_00100B10) + sb1;             /* 0x174f9 */
    if (box_o != 0u) DSD(DS_00100B30) = DSD(DS_00100B30) + ob1; /* 0x17507 */
    {
        s32 d = (s32)DSD(DS_00100B14) - (s32)DSD(DS_00100B34);
        if (d < 0) d = -d;
        DSD(DS_00100B38) = (u32)d;                         /* 0x17539 */
    }
    DSD(DS_00100B40) = DSD(DS_00100B1C);                   /* 0x17553 */
    {
        u32 flag = ((s32)DSD(DS_00100B14) > (s32)DSD(DS_00100B34)) ? 1u : 0u; /* 0x1751d */
        camera_winner_height(flag, DSD(DS_00100AF0 + side * 4u),
                             DSD(DS_00100AF0 + other * 4u),
                             DSD(DS_00100B10), DSD(DS_00100B30), -1, side); /* 0x17565 */
    }
    DSD(DS_00100AF8 + side * 4u) = DSD(DS_00100B54);       /* 0x1756f */
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
     * B61 != 0 (0x176BF). */
    if (camera_box_overlap(DSW(DSD(DS_001077B0) + 0x56u),
                           DSW(DSD(DS_00107844) + 0x56u)) != 0) {
        if (DSB(DS_00100B60) != 0) camera_unfreeze(0u);    /* 0x176AC */
        if (DSB(DS_00100B61) != 0) camera_unfreeze(1u);    /* 0x176BF */
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
