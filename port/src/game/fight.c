/* Port of the demo arena frame 0x263F4 and the HUD/health spine
 * 0x35658 -> 0x34B6C -> 0x1A978 -> 0x3B134, plus the scene/effects pass
 * 0x49C78. Addresses, gates and arithmetic are from
 * docs/superpowers/plans/2026-09-20-demo-fight-derivations.md §3-§5. The module
 * registers nothing; the arena frame is the state-7 body's first call
 * (0x11E8F). The 0x1975C think step is fighter_think() (fighter.c). */
#include "game/fight.h"
#include "game/fighter.h"
#include "game/camera.h"
#include "game/actors.h"
#include "game/effects.h"
#include "game/rng.h"
#include "../mem.h"
#include "../symbols.h"

/* 0x10810D: the mode-3 single-player slot index, written by 0x41350. Ghidra
 * emits it only as the `ram0x0010810d` form, so gen_symbols.py has no DS_ name
 * (camera.c's CAMERA_SLOT_INDEX3 is the same address). */
#define FIGHT_SLOT_INDEX 0x0010810Du

/* ---- 0x3C5CC the frame's first call ------------------------------------ */

void fight_slot_clear(void)
{
    DSD(DS_00107EE0) = 0;                       /* 0x3C5CF */
    DSD(DS_00107D50) = 0;                       /* 0x3C5D5 */
    DSD(DS_00107D54) = 0;                       /* 0x3C5DB */
}

/* ---- 0x49300 the fight-effect list init --------------------------------- */

/* 0x49300, called from state 6's 0x20DF4 at 0x11AC4. It self-links the
 * 0x1083C4 and 0x10884C {next@+0; prev@+4} sentinels (inserting each 0x24-stride
 * node into the 0x1083C4 list with 0x249C0) and seeds DS_001088CC/CB from the
 * draw1 value DS_00104AFC. The port skipped 0x20DF4 as a named gap, but the
 * 0x49C78 walk at 0x49CAF (fight_effects_pass) reads DS_0010884C as its head and
 * loops until it returns to the sentinel, so without this init the uninitialised
 * zero head walks address 0 forever. Ported here because 0x49300 is that walk's
 * liveness precondition; 0x20DF4's other resets stay declared gaps. */
void fight_list_init(void)
{
    DSD(DS_00108880) = 0x1500u;                 /* 0x49312 */
    DSD(DS_00108850) = DS_0010884C;            /* 0x49318 */
    DSD(DS_0010884C) = DS_0010884C;            /* 0x4931E */
    DSD(DS_001083C8) = DS_001083C4;            /* 0x49324 */
    DSD(DS_001083C4) = DS_001083C4;            /* 0x4932F */
    for (u32 node = 0x1083CCu; node < DS_0010884C; node += 0x24u)
        effects_list_insert_before(DS_001083C4, node);   /* 0x49347 0x249C0 */
    if ((u32)DSW(DS_00104AFC) == 7u) {          /* 0x4935C */
        DSB(DS_001088CC) = 0;                   /* 0x49363 */
        DSB(DS_001088CB) = 0;                   /* 0x49369 */
        return;
    }
    DSB(DS_001088CC) = 2u;                      /* 0x49377 */
    DSB(DS_001088CB) = 4u;                      /* 0x4937D */
}

/* ---- 0x412A0/0x2C320 the scene's props and crowd ------------------------ */

/* The crowd's descriptor-index table at 0xBB9D8 (stride 0xC, the first dword
 * used); symbols.h emits no name for it. */
#define DS_000BB9D8 0x000BB9D8u

/* 0x2C320. The scene's crowd actors. `n = DSW(0xBBD98 + scene*2)`; for each
 * 12-byte record in the table at `0xBBDA8[scene]` it spawns
 * 0x2AE14(desc = 0xBB9D8[[e+0xA]*3], a2 = [e], a3 = (s16)[e+6], a4 = (s16)[e+4],
 * a5 = ([e+0xB] << 16) | word[e+8]) and stores the table at DS_00105C08. The
 * only caller is 0x412A0 (0x412D8). Scene 0's table is 0xBBC18 with 3 records. */
void fight_scene_crowd(u32 scene)
{
    u16 n = DSW(DS_000BBD98 + scene * 2u);          /* 0x2C325 */
    if (n == 0u) return;                            /* 0x2C32D */
    u32 table = DSD(DS_000BBDA8 + scene * 4u);      /* 0x2C332 */
    DSD(DS_00105C08) = table;                       /* 0x2C339 */
    for (u32 i = 0; i < (u32)n; i++) {              /* 0x2C340 */
        u32 e = table + i * 0xCu;
        u32 a5 = ((u32)DSB(e + 0xbu) << 16) | DSW(e + 8u);        /* 0x2C347 */
        u32 desc = DSD(DS_000BB9D8 + (u32)DSB(e + 0xau) * 0xCu);  /* 0x2C370 */
        (void)actor_spawn((const u32 *)(mem + desc), DSD(e),
                          (u32)(s32)(s16)DSW(e + 6u),   /* a3 = ECX */
                          (u32)(s32)(s16)DSW(e + 4u),   /* a4 = EBX */
                          a5);                          /* 0x2C379 */
    }
}

/* 0x412A0. The scene's prop actors, the `0x20DF4` branch's third call
 * (0x20E86, EAX = the clamped scene index). For each 12-byte triple in the
 * table at `0xC82CC[scene]` — until a zero first dword — it spawns
 * 0x2AE14(desc = [e], a2 = [e+4], a3 = (s16)[e+8], a4 = 0, a5 = 0), then
 * 0x2C320(scene).
 *
 * PORT: the tail call `0xC7F58[scene]()` (0x412DD) is not issued: the eight
 * table entries are no-op targets — 0x412EC is 0x412A0's own `RET` and 0x5D812
 * is `xor eax,eax; ret` (both in the fixed image) — so it spawns nothing.
 * Scene 0's table is 0xC7F78 (5 triples) and its descriptors at 0xC77EC,
 * 0xC7800, 0xC7814, 0xC7828, 0xC783C carry sprite ids 0x2EF..0x2F3, which
 * 0xA8B30 resolves to s16beach descriptor indices 1, 0, 2, 4 (the temple), 3. */
void fight_scene_props(u32 scene)
{
    u32 table = DSD(DS_000C82CC + scene * 4u);      /* 0x412A8 */
    if (DSD(table) != 0u) {                         /* 0x412B3 */
        u32 e = table;
        for (;;) {
            (void)actor_spawn((const u32 *)(mem + DSD(e)), DSD(e + 4u),
                              (u32)(s32)(s16)DSW(e + 8u), 0u, 0u);  /* 0x412C7 */
            e += 0xCu;                              /* 0x412CF */
            if (DSD(e) == 0u) break;                /* 0x412D2 (EBP == 0) */
        }
    }
    fight_scene_crowd(scene);                       /* 0x412D8 */
}

/* ---- 0x494A8 the dust builder (state 6's fighter spawn) ----------------- */

/* 0x49388. The dust descriptor picker. The draw's range is the raw's
 * 0x4938b..0x493ab: `MOV DX,[0x104B00]; CMP EDX,3; JNZ 0x4939E;
 * MOV EAX,0x64; JMP 0x493AB; 0x4939E MOV AX,[EAX*2+0x108860]; AND EAX,0xffff;
 * 0x493AB CALL 0x5D7DC` — the caller's EAX (the side, set at 0x495AE) indexes
 * the table word DS_00108860[side] (0x33C50 seeds it 100), except in mode 3
 * (the demo), where the range is 0x64. One draw; the value maps through the
 * raw's thresholds to a 0xC9524 index. */
static u32 fight_dust_pick(u32 side)
{
    u32 range = (DSW(DS_00104B00) == 3u)
              ? 0x64u
              : (u32)DSW(DS_00108860 + side * 2u);
    u32 v = rng_next(range);                    /* 0x493AB */
    if (v < 0x1eu) return 4;
    if (v < 0x32u) return 3;
    if (v < 0x46u) return 5;
    if (v < 0x55u) return 1;
    if (v < 0x5fu) return 0;
    return 2;
}

/* 0x29CDC. The value written into the picked descriptor's +0x10: the per-side
 * table DS_000A8AF8 (when DS_00105B34[side] == 0) or DS_000A8B14, indexed by
 * the slot's character byte. */
static u32 fight_dust_value(u32 side, u32 ch)
{
    if (DSB(DS_00105B34 + side) == 0u) return DSD(DS_000A8AF8 + ch * 4u);
    return DSD(DS_000A8B14 + ch * 4u);
}

/* 0x496AC. The clamp the dust actor's +0x2C receives. The argument is the
 * loop's step (0x49568's [ESP], reloaded at 0x49629), not the y. */
static u16 fight_dust_clamp(u32 v)
{
    if (v > 0xaffu) return 0xc00u;
    if (v < 0x401u) return 0xf80u;
    return (u16)(((0xb00u - v) >> 1) + 0xc00u);
}

/* 0x494A8. The dust/effect entry builder. 0x33C78 calls it at 0x33E43 when
 * DS_00104B14 == 0; each iteration moves one node from the free fight-effect
 * list (DS_001083C4, built by 0x49300) to the active one (DS_0010884C), picks a
 * descriptor (0xC9524[0x49388]), spawns the dust actor (0x2AE14) and fills the
 * entry's fields. The loop bound is slot+0x81 (0x4967F) and each iteration
 * draws THREE values — 0x49388's, rng(0x1800) (0x495DF) and rng(step)
 * (0x495FC). State 6's stream needs exactly those: the demo's slot+0x81 is 2,
 * so six draws land between the picks at 0x11AAD and 0x11AE9. The entry's
 * +0x1E type is 0, whose 0x49C78 handler (0x4AAD0) is the unported dust
 * behaviour (§7.4); the actor it spawns is an ordinary pool actor and renders. */
void fight_dust_build(u32 side)
{
    /* PORT: 0x494A8's DS_00104AFA == 0x23 arm calls 0x4CF20, a six-entry
     * variant of this builder with its own draws. Not reached in the demo (the
     * reference's state-6 draws are this arm's 3 x slot+0x81) and a named gap. */
    if (DSB(DS_00104AFA) == 0x23u) return;

    DSB(DS_001088AE + side) = 0;                /* 0x494F2 */
    DSB(DS_001088C4) = 0;                       /* 0x494FE */
    DSB(DS_001088A2 + side) = 0;                /* 0x49504 */
    DSB(DS_001088A4 + side) = 0;                /* 0x4950A */
    DSB(DS_001088B2 + side) = 0;                /* 0x49516 */
    DSB(DS_001088BF) = 0;                       /* 0x49524 */
    DSB(DS_001088A8 + side) = 0xffu;            /* 0x4952A */
    DSB(DS_0010889E + side) = 0;                /* 0x49532 */
    DSB(DS_001088B6 + side) = 0;                /* 0x49538 */
    DSW(DS_00108892) = 0;                       /* 0x49546 */

    u32 slot = DS_001077B0 + side * 0x94u;
    u32 n = (u32)DSB(slot + 0x81u);             /* 0x49540 */
    u32 step = (n != 0u) ? (0x300u / n) : 0x300u;   /* 0x49555/0x49563 */
    u32 offset = 0;                             /* 0x49578 */
    for (u32 i = 0; i < n; i++) {               /* 0x4967F */
        u32 entry = DSD(DS_001083C4);           /* 0x49581 */
        if (entry == DS_001083C4) return;       /* 0x4959C: the pool is empty */
        effects_list_unlink(entry);             /* 0x49595 0x249D0 */
        effects_list_insert_after(DS_0010884C, entry);   /* 0x495A9 0x249B0 */
        u32 desc = DSD(DS_000C9524 + fight_dust_pick(side) * 4u);  /* 0x495B9 */
        DSD(desc + 0x10u) = fight_dust_value(side, (u32)DSB(slot + 0x7Au)); /* 0x495CC */
        u32 rec = DSD(slot);                    /* 0x495CF/0x495E6 */
        u32 x = (u32)((s32)DSD(rec + 0x18u) - 0xc00 + (s32)rng_next(0x1800u)); /* 0x495E4 */
        u32 y = offset + (u32)((s32)DSD(rec + 0x30u) >> 16) + 0x400u
                + rng_next(step);               /* 0x49601 */
        u32 actor = actor_spawn((const u32 *)(mem + desc), x, y, 0u, 0u);  /* 0x4960F */
        DSD(entry + 8u) = actor;                /* 0x49614 */
        DSD(actor + 0x14u) = entry;             /* 0x49617 */
        DSB(entry + 0x1Eu) = 0;                 /* 0x4961A */
        DSB(entry + 0x1Fu) = 0;                 /* 0x49622 */
        DSB(entry + 0x21u) = (u8)y;             /* 0x49626 */
        DSD(entry + 0x0Cu) = slot;              /* 0x4962D */
        DSW(actor + 0x2Cu) = fight_dust_clamp(step);   /* 0x49638 */
        DSW(entry + 0x1Cu) = 0;                 /* 0x4963C */
        DSD(entry + 0x10u) = 0;                 /* 0x49646 */
        DSW(entry + 0x1Au) = (u16)step;         /* 0x4964D */
        if (DSB(rec + 0x51u) != 0u) {           /* 0x49651 */
            DSW(actor + 0x2Eu) += 4;            /* 0x4965C */
            DSB(actor + 0x4Eu) = 1;             /* 0x49664 */
        }
        offset += step;                         /* 0x49674 */
    }
    DSB(DS_001088C3) = 0;                       /* 0x49693 */
    DSB(DS_001088C1) = 0;                       /* 0x4969B */
}

/* ---- 0x33C18 the character select's slot reset -------------------------- */

/* 0x33C18. Clear the per-side character fields and reset the 0x108860 word. */
static void fight_char_reset(u32 side)
{
    u32 slot = DS_001077B0 + side * 0x94u;
    DSB(slot + 0x7Fu) = 0;                      /* 0x33C2E */
    DSB(slot + 0x80u) = 0;                      /* 0x33C32 */
    DSD(slot + 0x3Cu) = 0;                      /* 0x33C39 */
    DSB(slot + 0x82u) = 0;                      /* 0x33C40 */
    DSB(slot + 0x5Bu) = 0;                      /* 0x33C4C */
    DSW(DS_00108860 + side * 2u) = 100u;        /* 0x33C50 */
}

/* ---- 0x41350 the per-side character select ------------------------------ */

void fight_char_select(u32 side, u32 char_index)
{
    fight_char_reset(side);                                 /* 0x41354 */
    if (DSB(DS_00104B1D) != 1u)                             /* 0x41359 */
        DSB(DS_0010816A + side) =
            DSB(DS_000C835A + (char_index & 0xFFFFu));      /* 0x41367 */
    DSB(DS_001077B0 + side * 0x94u + 0x63u) = 1u;           /* 0x41385 */
    DSB(DS_0010816E + side) = 0xFFu;                        /* 0x41390 */
    DSB(DS_00105B34 + side) = 0u;                           /* 0x41398 */
    if (DSB(DS_0010816A + side) == DSB(DS_0010816A + (side ^ 1u))
            && DSB(DS_00105B34 + (side ^ 1u)) == 0u)
        DSB(DS_00105B34 + side) = 1u;                       /* 0x413B5 */
    DSB(FIGHT_SLOT_INDEX) = (u8)(side ^ 1u);                /* 0x413BF */
}

/* ---- 0x1D890 the HUD spawn ---------------------------------------------- */

void fight_hud_spawn(u32 enable)
{
    for (u32 side = 0; side < 2u; side++) {                 /* 0x1D8A6 loop */
        DSB(DS_0010780E + side * 0x94u) = 0;                /* 0x1D8AB */
        DSB(DS_0010290C + side) = 0;                        /* 0x1D8B1 */
        DSB(DS_0010780A + side * 0x94u) = 0;                /* 0x1D8B7 */
        DSB(DS_0010290E + side) = 0;                        /* 0x1D8C1 */
        if (enable != 0u) {
            /* PORT: 0x1D8CF..0x1D9DA the HUD actor spawn (0x1D2F0/0x1D464/
             * 0x1D838, the 0xA76xx tables and DS_00104AEC bit 2) is cycle 2's
             * HUD; state 6's EAX is 0, so it is not reached here (gap §10.6). */
        }
    }
}

/* ---- 0x33F08 the health-bar pass ---------------------------------------- */

void fight_health_bars(void)
{
    u32 table = DSD(DS_001014EC);                           /* 0x33F0D */
    for (u32 side = 0; side < 2u; side++) {                 /* 0x33F13 */
        u32 rec = DSD(DS_001077A8 + side * 4u);             /* 0x33F15 */
        if (rec == 0) continue;                             /* 0x33F1D */
        u32 fighter = DSD(rec);                             /* 0x33F76 */
        u32 actorword = DSW(table
            + (u32)(u16)DSW(fighter + 0x56u) * 0x20u);      /* 0x33F87 */
        s32 s = (s32)(actorword & 0x7FFFu)
              - (s32)camera_char_const(DSB(rec + 0x7Au));   /* 0x33F8E */
        u32 pset = DSD(rec + 4u);                           /* 0x33FA8 */
        if ((DSB(rec + 0x41u) & 0x20u) == 0 && s >= 0 && s < 0x4B1) {
            DSD(pset + 8u) = (u32)DSW(DSD(rec + 0x24u) + (u32)s * 2u);  /* 0x33FC4 */
        } else {
            DSD(pset + 8u) = 0x1E1u;                        /* 0x33FAB */
        }
        if ((actorword & 0x8000u) != 0)                     /* 0x33FDE */
            DSB(pset + 0x29u) |= 0x40u;                     /* 0x33FEC */
        else
            DSB(pset + 0x29u) &= 0xBFu;                     /* 0x33FF5 */
        {
            u32 secondary = DSD(rec + 4u);                  /* 0x33FF9 */
            u32 spset = table
                + (u32)(u16)DSW(secondary + 0x56u) * 0x20u; /* 0x34005 */
            DSW(spset) = (u16)anim_next_sprite_id(secondary, spset);
        }
    }
}

/* ---- 0x3CB68 the 2x32 slot pass ---------------------------------------- */

static void fight_slot_pass(void)
{
    /* 0x3CB6B saves DS_00107ED8 and 0x3CB71 zeroes DS_00107EDC; the outer loop
     * increments DS_00107EDC to 2 and the inner runs 0x20 times per side. */
    DSD(DS_00107EDC) = 0;                       /* 0x3CB71 */
    for (u32 side = 0; side < 2u; side++) {
        DSB(DS_00107EE4) = (u8)fighter_actor_bit15_clear(side);   /* 0x3CB89 */
        for (u32 i = 0; i < 0x20u; i++) {
            DSD(DS_00107ED8) = i;               /* 0x3CB96 */
            hit_slot_step();                    /* 0x3CB9B 0x3C88C */
        }
        DSD(DS_00107EDC) = side + 1u;           /* 0x3CBA8 */
    }
    DSD(DS_00107EDC) = 2;                       /* 0x3CBAE */
    DSD(DS_00107ED8) = 0x20u;                   /* 0x3CBB9 */
}

/* ---- 0x1A978 the stance/command pass ----------------------------------- */

void fight_stance_pass(u32 side)
{
    u32 ctx[6];
    fighter_ctx_swap(ctx, side);                /* 0x1A984 0x33A10 */
    u32 self = ctx[3], other = ctx[2];

    /* 0x1A989: other+0x5F != -1 and self+0x62 != 0 arm the command mapper. */
    {
        u8 st = (u8)DSB(other + 0x5fu);
        if (st != 0xFFu && DSB(self + 0x62u) != 0)
            fight_command_map(ctx[1], (u32)st, 0u);  /* 0x1A9AE 0x3B134 */
    }

    /* 0x1A9B3: self +0x63 blocks the +0x61 stance-timer block. */
    if (DSB(self + 0x63u) == 0) {
        DSB(self + 0x61u) = (u8)(DSB(self + 0x61u) - 1u);   /* 0x1A9C2 */
        if ((s8)DSB(self + 0x61u) <= 0) {                    /* 0x1A9CD */
            DSB(self + 0x61u) = 0;                           /* 0x1A9CF */
            /* 0x1A9D2: bit 0x4000 of the side command drives +0x54. */
            if ((DSW(DS_001088E0 + ctx[1] * 2u) & 0x4000u) != 0) {
                if (DSB(self + 0x54u) != 1) {
                    DSB(self + 0x54u) = 1;                   /* 0x1A9F0 */
                    /* PORT: 0x1AA24 0x1A6AC(self, ctx[5]) — named gap (§7.11). */
                }
            } else if (DSB(self + 0x54u) != 0) {
                DSB(self + 0x54u) = 0;                       /* 0x1AA18 */
                /* PORT: 0x1AA24 0x1A6AC — named gap (§7.11). */
            }
        }
    }

    /* 0x1AA29: the +0x62 attack window and the +0x60 timer. */
    if (DSB(self + 0x62u) != 0) {
        DSB(self + 0x60u) = (u8)(DSB(self + 0x60u) - 1u);    /* 0x1AA35 */
        if (((s32)DSD(self + 0x5du) >> 24) < 1) {            /* 0x1AA4A */
            if (DSW(self + 0x86u) != DSW(other + 0x84u))     /* 0x1AA5F */
                DSB(other + 0x8au) = 0;                      /* 0x1AA68 */
            DSB(self + 0x62u) = 0;                           /* 0x1AA73 */
            DSB(self + 0x60u) = 0x28;                        /* 0x1AA7B */
            DSB(self + 0x53u) = 0;                           /* 0x1AA83 */
        }
    } else {
        DSB(self + 0x60u) = (u8)(DSB(self + 0x60u) - 1u);    /* 0x1AA8C */
        /* PORT: 0x1AA8F..0x1AB04. 0x1A640(side) and 0x1A8F4(self, ctx[5]) are
         * named gaps (§7.11); the raw's branch selects between them, so with
         * both unported the arm reduces to the +0x60 decrement. */
    }
}

/* ---- 0x3BDB0 the attack-readiness gate --------------------------------- */

static int fight_attack_ready(u32 side)
{
    u32 ctx[6];
    fighter_ctx_same(ctx, side);                /* 0x3BDB8 0x33950 */
    u32 self = ctx[2];
    return DSB(self + 0x53u) == 0 && DSB(self + 0x54u) != 2;
}

/* ---- 0x3B134 the command-word mapper ----------------------------------- */

void fight_command_map(u32 side, u32 edx_arg, u32 override)
{
    u32 ctx[6];
    fighter_ctx_swap(ctx, side);                             /* 0x3B149 */
    u32 anim[3];
    fighter_anim_triple(anim, ctx[0], (s32)edx_arg);         /* 0x3B155 */

    /* 0x3B168 gate A: slot[side]+0x63. */
    if (DSB(DS_001077B0 + side * 0x94u + 0x63u) == 0) return;
    /* 0x3B17A gate B: 0x1AB10. */
    if (!fighter_state_ok(ctx[1])) return;

    /* 0x3B18C: the rng(100) reaction roll against a per-character/controller
     * threshold; `override` bypasses it (0x3B1AD). */
    {
        u32 draw = rng_next(0x64u);                          /* 0x3B18C */
        u32 c8 = DSD(DS_001082C8 + side * 4u);               /* 0x3B191 */
        u32 idx = (u32)DSB(DS_0010452C) << 4;                /* 0x3B19A */
        s32 thr = (s32)DSD(DS_000BEDF2 + idx + c8 * 2u) >> 16;
        if ((s32)draw > thr && (override & 0xffu) == 0) return;
    }

    u32 self = ctx[3], other = ctx[2];
    /* 0x3B1BC: the facing-derived base from the two slot copies' +0x2C. */
    u32 base = ((s32)DSD(self + 0x2cu) > (s32)DSD(other + 0x2cu)) ? 0x1000u
                                                                  : 0x2000u;

    /* 0x3B1D8: the other slot's stance selects 0x5000/0x6000. */
    if (DSB(other + 0x64u) != 0xFFu && DSD(other + 0x08u) != 0) {
        s16 cx = (s16)DSW(DSD(other + 0x08u) + 0x34u);       /* 0x3B1FC */
        DSW(DS_001088E0 + side * 2u) = (u16)(cx < 0 ? 0x6000u : 0x5000u);
        return;
    }

    /* 0x3B220: the animation pointer's +2 bits select base/base|0x4000/0x8000. */
    {
        u8 b = DSB(anim[2] + 2u);
        if ((b & 1u) == 0) {
            DSW(DS_001088E0 + side * 2u) = (u16)base;
            return;
        }
        if ((b & 2u) == 0) {
            DSW(DS_001088E0 + side * 2u) = (u16)(base | 0x4000u);
            return;
        }
        DSW(DS_001088E0 + side * 2u) = 0x8000u;              /* 0x3B26F */
        if (fight_attack_ready(side))
            (void)fighter_attack_consume(side);              /* 0x3B28C */
    }
}

/* ---- 0x34B6C the health sync ------------------------------------------- */

/* 0x36E2C. The position gate that decides between the 0x34B97 position branch
 * and the 0x34BF4 +0x52 table: 1 when the slot's +0x63 is armable and
 * (DS_00104B1D == 3 || DS_00104B14 == 0 || slot+0x63 != 0), +0x42 bit 0x10 is
 * set, +0x52 is one of {0,1,5,6,7} and +0x54 <= 1. */
static int fight_position_gate(u32 rec)
{
    u8 st;
    if (!(DSB(DS_00104B1D) == 3u || DSB(DS_00104B14) == 0u
          || DSB(rec + 0x63u) != 0u))
        return 0;
    if ((DSB(rec + 0x42u) & 0x10u) == 0u) return 0;
    st = DSB(rec + 0x52u);
    if (!(st == 0u || st == 1u || st == 5u || st == 6u || st == 7u)) return 0;
    return DSB(rec + 0x54u) <= 1u;
}

static void fight_health_sync(u32 side)
{
    u32 rec = DSD(DS_001077A8 + side * 4u);     /* 0x34B73 */
    if (rec == 0) return;                       /* 0x34B7C */
    if (DSD(rec) == 0) return;                  /* 0x34B82/0x34B86 */

    if (fight_position_gate(rec) != 0) {        /* 0x34B8E 0x36E2C */
        u32 fighter = DSD(rec);
        s32 x = ((DSW(fighter + 0x28u) >> 8 & 0x40u) != 0u)
              ? (s32)DSD(fighter + 0x18u) + 0x3000
              : (s32)DSD(fighter + 0x18u) - 0x3000;
        if (x < (s32)DSD(DS_000BE018) && x > -(s32)DSD(DS_000BE018)) {
            /* PORT: 0x34BE8 0x36F10 — the in-range arm. It is unreachable:
             * slot+0x42 bit 0x10 is set only inside 0x36F10 itself (0x36FD4
             * `| 0x820` is bit 0x20, not 0x10; no writer sets bit 0x10), so the
             * gate can never return 1 without this call having already run.
             * Named gap (§7.10). */
        } else {
            DSB(rec + 0x43u) |= 0x40u;          /* 0x34BD1 */
            (void)fighter_state_36638(rec, fighter);   /* 0x34BDE */
        }
        return;
    }

    switch (DSB(rec + 0x52u)) {                 /* 0x34BF4 table 0x34B14 */
    case 0u:                                    /* 0x34C08 -> 0x349C8 */
        fighter_state_default(side);
        break;
    case 1u:  /* PORT: 0x34C15 0x359E0 — unported handler (§7.10) */ break;
    case 2u:  /* PORT: 0x34C26 0x35C1C/0x35D20 — unported (§7.10) */ break;
    case 3u:                                    /* 0x34C46 -> 0x35D7C */
        fighter_state_35d7c(side);
        break;
    case 4u:                                    /* 0x34C53 -> 0x35F84 */
        fighter_state_35f84(rec, DSD(rec));
        break;
    case 5u:                                    /* 0x34C62 -> 0x36430 */
        fighter_state_36430(rec, DSD(rec), side);
        break;
    case 6u:                                    /* 0x34C73 -> 0x1A978 */
        fight_stance_pass(side);
        break;
    case 7u:                                    /* 0x34C80 -> 0x399CC */
        fighter_state_399cc(side);
        break;
    case 8u:  /* PORT: 0x34C8D 0x37464 — unported handler (§7.10) */ break;
    /* 9,10,11,14,15,16 -> 0x34D83, the epilogue no-op. */
    case 9u: case 10u: case 11u: case 14u: case 15u: case 16u:
        break;
    case 12u:                                   /* 0x34C9A -> 0x361C8 */
        fighter_state_361c8(rec, DSD(rec));
        break;
    case 13u:                                   /* 0x34CA9 -> 0x36300 */
        fighter_state_36300(rec, DSD(rec));
        break;
    case 17u:                                   /* 0x34CB8 -> 0x36710 */
        fighter_state_36710(rec, DSD(rec));
        break;
    case 18u: /* PORT: 0x34CC7 inline cmd gate + 0x3BDDC -> 0x18B04 (§7.10) */ break;
    case 19u: /* PORT: 0x34D22 inline +0x8E countdown + 0x33B00 (§7.10) */ break;
    case 20u: /* PORT: 0x34D69 0x35E6C — unported handler (§7.10) */ break;
    case 21u:                                   /* 0x34D78 -> 0x364FC */
        fighter_state_364fc(rec, DSD(rec), side);
        break;
    default:                                    /* >0x15 -> 0x34C08 0x349C8 */
        fighter_state_default(side);
        break;
    }
}

/* ---- 0x35658 the HUD/health pass --------------------------------------- */

void fight_hud_pass(u32 side)
{
    u32 slot = DS_001077B0 + side * 0x94u;
    u32 rec;

    /* 0x35661..0x35773: the per-side preamble. 0x3C59C(3, side) is a
     * test-and-set fight_slot_clear zeroes each arena frame, so it runs once
     * per side; it maintains the slot+0x88/+0x8C/+0x90/+0x92 timers and
     * slot+0x78 that 0x350D0 and 0x3531C case 8 read. */
    if (fighter_pass_flag(3u, side) == 0) {                 /* 0x356BE */
        DSW(slot + 0x88u) = (u16)(DSW(slot + 0x88u) + 1u);  /* 0x356D6 */
        {
            s16 c = (s16)DSW(slot + 0x8Cu);                 /* 0x356CF */
            if (c > 0)      DSW(slot + 0x8Cu) = (u16)(c - 1);   /* 0x356E5 */
            else if (c < 0) DSW(slot + 0x8Cu) = 0;          /* 0x356F2 */
        }
        if ((s32)DSD(slot + 0x90u) >> 16 < 0x270F)          /* 0x35713 */
            DSW(slot + 0x92u) = (u16)(DSW(slot + 0x92u) + 1u);  /* 0x3571B */
    }
    {
        u16 cap = DSW(DS_000BDBEC);                         /* 0x35733 */
        u16 v = DSW(slot + 0x78u);                          /* 0x3573A */
        if ((s16)v > (s16)cap)      DSW(slot + 0x78u) = 0;  /* 0x35748 */
        else if ((s16)v > 0)        DSW(slot + 0x78u) = (u16)((s16)v - 1); /* 0x35757 */
    }
    DSB(DS_001078FB) = (u8)((((DSB(DS_001088D4) & 4u) != 0u) ? 0u : 1u) + 2u); /* 0x35770 */

    rec = DSD(DS_001077A8 + side * 4u);                     /* 0x35775 */
    if (rec == 0) return;                                   /* 0x3577E */

    if (DSW(DS_00104B00) == 4 && (DSB(rec + 0x43u) & 0x80u) == 0) {
        /* PORT: 0x35792..0x357D6. The mode-4 arm (0x33C78 behind the
         * DS_001088E0 bit-0 gate) is a named gap (§7.8). */
        return;
    }

    /* 0x357E4: rec is the camera-target record; *rec is the fighter record.
     * `[eax+0x28] = [*rec+0x18]` (eax=rec), and 0x35810's `[*rec+0x28] |= 1`
     * uses the same ebx (= *rec), which the callee-saved register holds across
     * the calls. */
    {
        u32 fighter = DSD(rec);                 /* 0x357E4 ebx = [eax] */
        DSD(rec + 0x28u) = DSD(fighter + 0x18u);          /* 0x357E6/0x357E9 */
        /* PORT: 0x357EE 0x34038(side) and 0x357F5 0x38D24(side) run before the
         * spine; both are named gaps (§7.8) and are skipped. */
        fight_health_sync(side);                /* 0x357FC 0x34B6C */
        fighter_state_3531c(side);              /* 0x35803 0x3531C */
        DSB(fighter + 0x28u) = (u8)(DSB(fighter + 0x28u) | 1u); /* 0x35808..0x35810 */
        actor_sync(fighter);                    /* 0x35813 0x2A1FC */
    }
    /* PORT: 0x3581C/0x35824 0x354F0(side)/(1-side) and 0x35829 0x186C4 are
     * named gaps (§7.8) and skipped. */
}

/* ---- 0x49C78 the scene/effects pass ------------------------------------ */

/* 0x2BE00. The actor's +4 field for a fighter record (a raw position term the
 * effect bodies use as a sign test). */
static s32 fight_2be00(u32 rec)
{
    u32 actor = DSD(DS_001014EC) + (u32)DSW(rec + 0x56u) * 0x20u;
    return (s32)DSD(actor + 4u);
}

/* 0x493F0. The midpoint of the two fighters' 0x2BE00 values (arithmetic shift:
 * floor-division by two after the difference is made non-negative). */
static u32 fight_midpoint(void)
{
    s32 a = fight_2be00(DSD(DS_001077B0));
    s32 b = fight_2be00(DSD(DS_00107844));
    s32 d = a - b;
    if (d >= 0) return (u32)((d >> 1) + b);
    return (u32)(((-d) >> 1) + a);
}

/* The fighter record cases 13/14 gate on: slot[ (s8)(DS_001088C6 >> 24) ]. */
static u32 fight_case_rec(void)
{
    s32 idx = (s32)DSD(DS_001088C6) >> 24;
    return DSD(DS_001077B0 + (u32)idx * 0x94u);
}

void fight_effects_pass(void)
{
    /* The raw's four frame locals (0x49C7E/0x49C8E). Only the mode-9 block reads
     * them, and the demo runs mode 3; they are kept zero. */
    /* PORT: 0x49C81..0x49CA6. The mode-9 local init is part of the named gap. */

    DSD(DS_00108874) = fight_midpoint();        /* 0x49CAA 0x493F0 */

    {
        u32 head = DSD(DS_0010884C);            /* 0x49CAF */
        if (head != DS_0010884C) {              /* 0x49CC5: empty list skips */
            for (;;) {
                u32 entry = head;
                u32 next = DSD(entry);          /* 0x49CD3 */
                u32 rec = DSD(entry + 8u);      /* 0x49CD8 */
                /* PORT: 0x49CDD..0x49CFC. The per-side counters and the
                 * 0x4B69C(entry, si) prelude are named gaps (§7.4). The `si`
                 * value the RNG cases use is (u16)(rec+0x48 - 0x20). */

                switch (DSB(entry + 0x1Eu)) {   /* 0x49D03 */
                case 3:
                    /* 0x49DC8: the DS_000BD898 position gate; the rest of the
                     * body (0x49DDE..0x49E38) is the named gap (§7.4). */
                    if ((s32)DSD(rec + 0x30u) >> 16
                            <= (s32)(s16)DSW(DS_000BD898))
                        (void)rng_next(0x3Cu);  /* 0x49E3A */
                    break;
                case 13: {
                    /* PORT: 0x4A24A..0x4A2F4. The case-13 body (0x2BE1C,
                     * 0x2BC30, the rec +0x3c/+0x2a/+0x32 gates, 0x2B150) is
                     * the named gap (§7.4); the 0x2BE00 arm is the draw gate. */
                    s32 r = fight_2be00(fight_case_rec());   /* 0x4A2F5 */
                    if (r > 0) (void)rng_next(0xC00u);       /* 0x4A305 */
                    else       (void)rng_next(0xC00u);       /* 0x4A315 */
                    break;
                }
                case 14: {
                    /* PORT: 0x4A346..0x4A412. The case-14 body (0x4A868,
                     * 0x2BC30, the rec +0x2a/+0x3c/+0x32 gates) is the named
                     * gap (§7.4); the 0x2BE00 arm is the draw gate. */
                    s32 r = fight_2be00(fight_case_rec());   /* 0x4A413 */
                    if (r > 0) (void)rng_next(0xC00u);       /* 0x4A439 */
                    else       (void)rng_next(0xC00u);       /* 0x4A449 */
                    break;
                }
                default:
                    /* PORT: 0x49D1E 0x4AAD0 and the other +0x1E cases are
                     * named gaps (§7.4). */
                    break;
                }

                head = next;                    /* 0x4A468 */
                if (head == DS_0010884C) break; /* 0x4A470 */
            }
        }
    }

    /* 0x4A476: the mode-9 block owns the 0x4A4C5/0x4A4F7 draws and the
     * DS_001088B0/0x1088C9/0x1088B4 traffic. The demo runs mode 3 and gates the
     * whole block out (0x4A481 `jne 0x4A591`), so those two sites are not
     * issued in cycle 1. */
    /* PORT: 0x4A487..0x4A58F (mode 9 only) — named gap (§7.4). */

    /* PORT: 0x4A591 0x4A634 — the mode tail is a named gap; its internal
     * rng(3)/rng(2) draws are not issued (§7.4). */
    DSB(DS_001088C2) = 0;                       /* 0x4A5A0 */

    /* 0x4A5A6: the tail rng(2) behind the DS_001088BF 1..4 gate, for modes
     * other than 7/8/9. DS_001088BF is not cleared on that early-skip path. */
    if (DSW(DS_00104B00) != 9 && DSW(DS_00104B00) != 8
            && DSW(DS_00104B00) != 7) {
        u8 bh = (u8)DSB(DS_001088BF);
        if (bh != 0) {
            if ((u8)(bh - 1u) <= 3u)
                (void)rng_next(2u);             /* 0x4A611; 0x4987C is a gap */
            DSB(DS_001088BF) = 0;               /* 0x4A61B */
        }
    }
    DSB(DS_00104AEC) = (u8)(DSB(DS_00104AEC) & 0x7Fu);   /* 0x4A623 */
}

/* ---- 0x263F4 the arena frame ------------------------------------------- */

void fight_arena_frame(void)
{
    fight_slot_clear();                                        /* 0x263F7 */
    camera_screen_base(0, (s32)DSB(DS_0010782A));              /* 0x2640B */
    camera_screen_base(1, (s32)DSB(DS_001078BE));              /* 0x26422 */

    /* 0x2642C/0x2643A: the two previous-position latches. */
    DSD(DS_001077E8) = DSD(DS_001077E4);
    DSD(DS_0010787C) = DSD(DS_00107878);

    camera_project(0, DS_00100B08, DS_00100B00, DS_00100B62,
                   DS_00100B60, DS_00100AF0);                  /* 0x26450 */
    camera_project(1, DS_00100B0C, DS_00100B04, DS_00100B63,
                   DS_00100B61, DS_00100AF4);                  /* 0x26473 */
    camera_decay();                                            /* 0x26478 0x17580 */
    fighter_pass_a();                                          /* 0x2647D 0x1958C */
    fighter_pass_b(0);                                         /* 0x26484 0x19068 */

    camera_project(0, DS_00100B08, DS_00100B00, DS_00100B62,
                   DS_00100B60, DS_00100AF0);                  /* 0x264A4 */
    camera_project(1, DS_00100B0C, DS_00100B04, DS_00100B63,
                   DS_00100B61, DS_00100AF4);                  /* 0x264C7 */

    fighter_think();                                           /* 0x264CC 0x1975C */

    camera_project(0, DS_00100B08, DS_00100B00, DS_00100B62,
                   DS_00100B60, DS_00100AF0);                  /* 0x264EC */
    camera_project(1, DS_00100B0C, DS_00100B04, DS_00100B63,
                   DS_00100B61, DS_00100AF4);                  /* 0x2650F */

    fight_slot_pass();                                         /* 0x26514 0x3CB68 */
    fight_hud_pass(0);                                         /* 0x2651B 0x35658 */
    fight_hud_pass(1);                                         /* 0x26525 0x35658 */
    fight_effects_pass();                                      /* 0x2652A 0x49C78 */
    camera_scene_step();                       /* 0x2652F 0x1282C + 0x26534 0x12DA8 */
}
