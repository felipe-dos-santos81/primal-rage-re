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

/* 0x496AC. The clamp the dust actor's +0x2C receives (and the effects pass's
 * cases 3 and 5, 0x49DBC/0x49EC9). The compares are signed (0x496AD `cmp
 * eax,0xb00` / `jl`, 0x496BB `cmp eax,0x400` / `jg`). The argument is the y:
 * 0x49629/0x49642 read `[ESP+0x4]` after 0x2AE14's `RET 0x4` (0x2B14A) has
 * popped the 0x49603 `PUSH 0x0`, so ESP is back at the frame base and
 * `[ESP+0x4]` is the value 0x49605 wrote (`ESP=S-4`, `[ESP+8]` = EBX = the y of
 * 0x49601). The step lives at `[ESP]` (0x495F9) and is not read here. The
 * original's entries confirm it: `entry+0x1a` = 0x0a49/0x0848/0x0a80/0x082d
 * and `actor+0x2c` = 0xc5b/0xd5c/0xc40/0xd69 = the raw's `(0xb00-y)>>1 + 0xc00`. */
static u16 fight_dust_clamp(s32 v)
{
    if (v >= 0xb00) return 0xc00u;
    if (v <= 0x400) return 0xf80u;
    return (u16)(((0xb00 - v) >> 1) + 0xc00);
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
        /* 0x49626. The raw reads `[ESP+0x8]` at 0x4961E, after the
         * 0x4960F 0x2AE14 call. 0x2AE14 ends `RET 0x4`, so it pops the
         * 0x49603 `PUSH 0x0` and ESP returns to the frame base; `[ESP+0x8]`
         * is then the side stored at 0x494B1, NOT the y stored at 0x49605
         * (which is one slot lower). The original's entries carry +0x21 =
         * 1,1,0,0 (the side), and 0x4AAD0 indexes the per-side
         * DS_001088B2/DS_0010889E tables with it. */
        DSB(entry + 0x21u) = (u8)side;          /* 0x49626 */
        DSD(entry + 0x0Cu) = slot;              /* 0x4962D */
        DSW(actor + 0x2Cu) = fight_dust_clamp((s32)y); /* 0x49638 */
        DSW(entry + 0x1Cu) = 0;                 /* 0x4963C */
        DSD(entry + 0x10u) = 0;                 /* 0x49646 */
        DSW(entry + 0x1Au) = (u16)y;            /* 0x4964D */
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
    case 1u:                                    /* 0x34C15 -> 0x359E0 */
        fighter_state_359e0(rec, DSD(rec), side);
        break;
    case 2u:                                    /* 0x34C26 -> 0x35C1C/0x35D20 */
        if (fighter_state_35c1c(rec, DSD(rec)) != 0)
            fighter_state_35d20(rec, DSD(rec));
        break;
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
    case 8u:                                    /* 0x34C8D -> 0x37464 */
        fighter_state_37464(side);
        break;
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
    case 19u:                                   /* 0x34D22 inline + 0x33B00 */
        {
            s16 v = (s16)(DSW(rec + 0x8Eu) - 1u);       /* 0x34D22 */
            DSW(rec + 0x8Eu) = (u16)v;                  /* 0x34D2A */
            if (v < 0) {
                for (u32 i = 0; i < 2u; i++) {
                    u32 s = DS_001077B0 + i * 0x94u;
                    if (DSB(s + 0x52u) == 0x13u)        /* 0x34D3F */
                        fighter_state_33b00(i,
                            0x00107BD0u + i * 0x94u,    /* 0x34D48 */
                            0x00107B00u + i * 0x68u);   /* 0x34D4D */
                }
            }
        }
        break;
    case 20u:                                   /* 0x34D69 -> 0x35E6C */
        fighter_state_35e6c(rec, DSD(rec));
        break;
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
    /* PORT: 0x3581C/0x35824 0x354F0(side)/(1-side), the arena-wall clamp
     * against DS_000BE018 (0x7C00), is a named gap (§7.8) and skipped. */
    fighter_slot_latch_both();                  /* 0x35829 0x186C4 */
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

/* ---- the type-0 effect handler 0x4AAD0 and its callees ------------------ */

/* The 0xC95xx per-dust-descriptor stream/flag tables. The index is the entry's
 * `si` = (u16)(actor+0x48 - 0x20) (0x49CE1/0x49CE7). Ghidra emits them as
 * DAT_000c95xx, so gen_symbols.py has no DS_ name. */
#define DS_000C955C 0x000C955Cu
#define DS_000C958C 0x000C958Cu
#define DS_000C95BC 0x000C95BCu
#define DS_000C95D4 0x000C95D4u
#define DS_000C9754 0x000C9754u

/* 0x2BE1C. The difference of two fighter records' 0x2BE00 position terms. */
static s32 fight_2be1c(u32 rec_a, u32 rec_b)
{
    return fight_2be00(rec_a) - fight_2be00(rec_b);
}

/* 0x2BE4C. `(rec+0x44 >> 16) * 2 + value - 0x2A00` (0x2BE4C..0x2BE5B). */
static s32 fight_2be4c(u32 rec, s32 value)
{
    return (((s32)DSD(rec + 0x44u) >> 16) * 2) + value - 0x2A00;
}

/* 0x4B3F0. When the 0xC955C[index] stream is set, point the entry's actor at it
 * and set the entry's type to 8. The raw's EBX selects actor+0x55; every
 * 0x4AAD0 call site zeroes it (0x4AAFC, 0x4AB18, 0x4AB6D, 0x4AB97), so the port
 * writes 0. */
static int fight_4b3f0(u32 entry, u32 index)
{
    u32 stream = DSD(DS_000C955C + index * 4u);     /* 0x4B3F0 */
    if (stream == 0u) return 0;                     /* 0x4B3F8 */
    u32 actor = DSD(entry + 8u);
    DSB(actor + 0x55u) = 0;                         /* 0x4B407 (EBX == 0) */
    DSB(entry + 0x1Eu) = 8u;                        /* 0x4B40E */
    actors_anim_begin(actor, stream, 0x40400000u);  /* 0x4B421 */
    return 1;                                       /* 0x4B426 */
}

/* 0x4B430. As 0x4B3F0 but over the 0xC958C table. */
static int fight_4b430(u32 entry, u32 index)
{
    u32 stream = DSD(DS_000C958C + index * 4u);     /* 0x4B430 */
    if (stream == 0u) return 0;                     /* 0x4B438 */
    u32 actor = DSD(entry + 8u);
    DSB(actor + 0x55u) = 0;                         /* 0x4B44A (EBX == 0) */
    DSB(entry + 0x1Eu) = 8u;                        /* 0x4B44E */
    actors_anim_begin(actor, stream, 0x40400000u);  /* 0x4B461 */
    return 1;                                       /* 0x4B466 */
}

/* 0x4BD4C. When the 0xC9754[index] stream is set, zero the actor's +0x34/+0x36/
 * +0x38, point it at the stream, then set the entry's type to 8. */
static int fight_4bd4c(u32 entry, u32 index)
{
    u32 stream = DSD(DS_000C9754 + index * 4u);     /* 0x4BD52 */
    if (stream == 0u) return 0;                     /* 0x4BD59 */
    u32 actor = DSD(entry + 8u);
    DSW(actor + 0x38u) = 0;                         /* 0x4BD5E */
    DSW(actor + 0x34u) = 0;                         /* 0x4BD67 */
    DSW(actor + 0x36u) = 0;                         /* 0x4BD70 */
    actors_anim_begin(actor, stream, 0x40800000u);  /* 0x4BD84 */
    DSB(entry + 0x1Eu) = 8u;                        /* 0x4BD8E */
    return 1;                                       /* 0x4BD89 */
}

/* 0x4B5A8. The `DS_001088B6[char] != 0 || (+0x52 == 7 && +0x53 == 2)` gate: on
 * true it retargets the actor at the 0xC95BC[index] stream, sets the entry's
 * +0x1A/+0x14 and type 3, and decrements DS_001088B6[char]. The raw returns the
 * flag in AL; the caller tests AL only. */
static int fight_4b5a8(u32 entry, u32 index)
{
    u32 slot = DSD(entry + 0xCu);
    u32 rec = DSD(slot);
    u8 ch = DSB(rec + 0x51u);                       /* 0x4B5B1 */
    if (DSB(DS_001088B6 + ch) == 0u
            && !(DSB(slot + 0x52u) == 7u && DSB(slot + 0x53u) == 2u))
        return 0;                                   /* 0x4B694 */
    u32 actor = DSD(entry + 8u);
    actors_anim_begin(actor, DSD(DS_000C95BC + index * 4u), 0x40400000u); /* 0x4B5E6 */
    DSW(entry + 0x1Au) = DSW(actor + 0x32u);        /* 0x4B5F2 */
    DSW(actor + 0x38u) = 0xFFC0u;                   /* 0x4B5F9 */
    s32 v;
    if (((DSW(rec + 0x28u) >> 8) & 0x40u) == 0u) {  /* 0x4B612 */
        DSW(actor + 0x34u) = 0xFFC0u;               /* 0x4B632 */
        v = fight_2be00(rec) - 0x1800;              /* 0x4B642 */
    } else {
        DSW(actor + 0x34u) = 0x0040u;               /* 0x4B617 */
        v = fight_2be00(rec) + 0x1800;              /* 0x4B627 */
    }
    DSD(entry + 0x14u) = (u32)fight_2be4c(actor, v);    /* 0x4B650 */
    DSB(entry + 0x1Eu) = 3u;                        /* 0x4B656 */
    u8 n = DSB(slot + 0x81u);
    if (DSB(DS_001088B6 + ch) > n)                  /* 0x4B671 */
        DSB(DS_001088B6 + ch) = n;                  /* 0x4B675 */
    DSB(DS_001088B6 + ch) -= 1u;                    /* 0x4B68B */
    return 1;                                       /* 0x4B691 */
}

/* The 0xC9544 per-descriptor arrival-stream table (0x4AC68 `mov edx,[ebx*4 +
 * 0xc9544]`); Ghidra emits it as DAT_000c9544, so gen_symbols.py has no DS_
 * name. Entry 0 is 0xEE02C, the type-0x20 worshipper's spawn stream. */
#define DS_000C9544 0x000C9544u

/* The 0xC9634 landing- and 0xC95EC rising-stream tables of the effects pass's
 * cases 3 and 4 (0x49E15 `mov edx,[eax*4 + 0xc9634]`, 0x49E74 `mov edx,[edx*4 +
 * 0xc95ec]`); Ghidra emits them as DAT_, so gen_symbols.py has no DS_ name. */
#define DS_000C9634 0x000C9634u
#define DS_000C95EC 0x000C95ECu

/* 0x4AC38. The arrival: the entry's actor stops (the +0x34/+0x36/+0x38 words
 * zeroed), clears its hflip (+0x29 &= 0xBF) and sets +0x29 bit 0x10, the entry
 * returns to type 0 and the actor is pointed at the 0xC9544[index] stream with
 * the hold 5.0 (`push 0x40a00000`, 0x4AC72). EAX = entry, EDX = index. */
static void fight_4ac38(u32 entry, u32 index)
{
    u32 actor = DSD(entry + 8u);
    DSB(actor + 0x29u) &= 0xBFu;                    /* 0x4AC3E */
    DSB(actor + 0x29u) |= 0x10u;                    /* 0x4AC45 */
    DSW(actor + 0x34u) = 0;                         /* 0x4AC4C */
    DSW(actor + 0x36u) = 0;                         /* 0x4AC55 */
    DSW(actor + 0x38u) = 0;                         /* 0x4AC5E */
    DSB(entry + 0x1Eu) = 0;                         /* 0x4AC64 */
    actors_anim_begin(actor, DSD(DS_000C9544 + index * 4u), 0x40A00000u); /* 0x4AC77 */
}

/* 0x4AC18 — demo-pose record §23. The worshipper streams' opcode-0x15 target:
 * the arrival 0x4AC38 for the actor's own +0x14 entry, indexed by the actor's
 * +0x48 descriptor byte less 0x20 (0x4AC25 `mov dl,[eax+0x48]`, 0x4AC28 `sub
 * edx,0x20`, a 32-bit index, unlike the effects pass's u16 `si`). */
void fight_4ac18(u32 rec)
{
    u32 entry = DSD(rec + 0x14u);                   /* 0x4AC1A */
    if (entry == 0u) return;                        /* 0x4AC1F */
    fight_4ac38(entry, (u32)DSB(rec + 0x48u) - 0x20u); /* 0x4AC25..0x4AC2D */
}

/* 0x4B144. The type-0 entry's distance resolution: it walks the entry's actor
 * toward DS_00108874 (the midpoint), drawing rng(2)/rng(0x1200) 1-3 times on
 * the way, then sets the entry's +0x14 and type 1 and retargets the actor at
 * the 0xC95D4[index] stream. The sign/`0x600`/`0x1800` arms are the raw's
 * 0x4B164..0x4B24D. */
static void fight_4b144(u32 entry, u32 index)
{
    u32 rec = DSD(DSD(entry + 0xCu));               /* 0x4B14C */
    s32 pos = fight_2be00(rec);                     /* 0x4B151 */
    s32 mid = (s32)DSD(DS_00108874);                /* 0x4B156 */
    s32 ecx = pos - 0x600;                          /* 0x4B15E */
    s32 result;
    if ((u32)pos < (u32)mid) {                      /* 0x4B166 JNC */
        if (mid > 0x2A00) {                         /* 0x4B172 JLE */
            if (rng_next(2u) != 0u) {               /* 0x4B179 */
                ecx -= (s32)rng_next(0x1200u);      /* 0x4B187 */
                if ((u32)ecx > (u32)mid)            /* 0x4B196 JBE */
                    result = mid - (s32)rng_next(0x1200u);   /* 0x4B1A1 */
                else
                    result = ecx;
                goto lab;
            }
        }
        result = (pos + 0x600) + (s32)rng_next(0x1200u);    /* 0x4B1C0 */
        if ((u32)result > (u32)mid)                 /* 0x4B1CD JBE */
            result = mid - (s32)rng_next(0x1200u);  /* 0x4B1D8 */
    } else {
        if (mid < 0x2A00) {                         /* 0x4B1F2 JGE */
            if (rng_next(2u) != 0u) {               /* 0x4B1F9 */
                ecx -= (s32)rng_next(0x1200u);      /* 0x4B207 */
                if ((u32)ecx < (u32)mid)            /* 0x4B218 JNC */
                    result = mid + (s32)rng_next(0x1200u);   /* 0x4B21F */
                else
                    result = ecx;
                goto lab;
            }
        }
        result = (pos - 0x600) - (s32)rng_next(0x1200u);    /* 0x4B237 */
        if ((u32)result < (u32)mid)                 /* 0x4B246 JNC */
            result = mid + (s32)rng_next(0x1200u);  /* 0x4B24D */
    }
lab:
    {
        u32 actor = DSD(entry + 8u);                /* 0x4B258 */
        DSD(entry + 0x14u) = (u32)fight_2be4c(actor, result);   /* 0x4B268 */
        DSB(entry + 0x1Eu) = 1u;                    /* 0x4B264 */
        if ((s32)DSD(actor + 0x18u) < (s32)DSD(entry + 0x14u)) {    /* 0x4B271 */
            DSW(actor + 0x34u) = 0x0080u;           /* 0x4B276 */
            DSB(actor + 0x29u) &= (u8)~0x40u;       /* 0x4B27F */
        } else {
            DSW(actor + 0x34u) = 0xFF80u;           /* 0x4B285 */
            DSB(actor + 0x29u) |= 0x40u;            /* 0x4B28E */
        }
        actors_anim_begin(actor, DSD(DS_000C95D4 + index * 4u), 0x40400000u); /* 0x4B2A1 */
    }
}

/* 0x4AAD0. The type-0 (and >0xE) fight-effect handler. It gates on the slot's
 * +0x54/+0x42, DS_001088C2, 0x4B5A8 and the 0x1088B2/0x10889E tables, then
 * takes the 0x2BE00/0x2BE1C distance test into 0x4B144 (the four draws the
 * state-7 entry was missing). `entry` is EAX/ECX, `index` is EDX/ESI
 * (0x49D1E/0x4AAD5/0x4AAD7). */
static void fight_4aad0(u32 entry, u32 index)
{
    if (DSW(DS_00104B00) == 9u) {                   /* 0x4AAE1 */
        DSB(entry + 0x1Eu) = 9u;                    /* 0x4AAE6 */
        return;
    }
    u32 slot = DSD(entry + 0xCu);                   /* 0x4AAEF */
    if (DSB(slot + 0x54u) == 3u) {                  /* 0x4AAF2 */
        if (fight_4b430(entry, index) != 0) return; /* 0x4AAFE */
    }
    if (DSB(slot + 0x54u) == 4u) {                  /* 0x4AB0E */
        if (fight_4b3f0(entry, index) != 0) return; /* 0x4AB1A */
    }
    if (DSB(DS_001088C2) != 0u) {                   /* 0x4AB27 */
        if (fight_4bd4c(entry, index) != 0) return; /* 0x4AB34 */
    }
    if (fight_4b5a8(entry, index) != 0) return;     /* 0x4AB45 */
    if ((DSB(slot + 0x42u) & 2u) != 0u
            || DSB(DS_001088B2 + (u32)DSB(entry + 0x21u)) != 0u) {
        if (fight_4b3f0(entry, index) != 0) return; /* 0x4AB6F */
    }
    if ((DSB(slot + 0x42u) & 1u) != 0u
            || DSB(DS_0010889E + (u32)DSB(entry + 0x21u)) != 0u) {
        if (fight_4b430(entry, index) != 0) return; /* 0x4AB99 */
    }
    {
        s32 d = fight_2be1c(DSD(entry + 8u), DSD(slot));    /* 0x4ABAE */
        s32 e_actor = (s32)DSD(DS_00108874) - fight_2be00(DSD(entry + 8u));
        s32 e_rec = (s32)DSD(DS_00108874) - fight_2be00(DSD(slot));
        s32 ad = (d < 0) ? -d : d;
        if (ad >= 0x1800 || ad <= 0x600                     /* 0x4ABE1/0x4ABEF */
                || (e_actor > 0 && e_rec < 0)               /* 0x4ABFD */
                || (e_actor < 0 && e_rec > 0))              /* 0x4ABF7 */
            fight_4b144(entry, index);                      /* 0x4AC0B */
    }
}

/* 0x4A634. The effects pass's per-frame flag reset, called unconditionally at
 * 0x4A591. For each side (EDX = side, EBX = side * 0x94): when the slot's +0x42
 * bit 1 is set, the side's 0x1088A8 reaction byte picks the crowd voice, which
 * draws rng(3) for 0x20..0x3F, or rng(2) and, when that is non-zero, rng(2)
 * again for 0x10..0x17. Then it clears +0x42 bits 0/1 (`and al,0xfc`) and the
 * side's 0x10889E/0x1088B2 bytes (0x4A6EE/0x4A6F4 store at +0x10889D/+0x1088B1
 * after `inc edx`). */
static void fight_4a634(void)
{
    u32 side;
    for (side = 0; side < 2u; side++) {                     /* 0x4A6FA */
        u32 slot = DS_001077B0 + side * 0x94u;              /* 0x4A6E8 */
        if ((DSB(slot + 0x42u) & 2u) != 0u) {               /* 0x4A640 */
            u32 r = (u32)DSB(DS_001088A8 + side);           /* 0x4A64F */
            if (r >= 0x20u && r <= 0x3Fu) {                 /* 0x4A655/0x4A65A */
                (void)rng_next(3u);                         /* 0x4A664 */
                /* PORT: 0x4A6D2 0x2C3FC(0xCD/0xCE/0xCF by the draw) voice, out
                 * of scope (spec §7). */
            } else if (r >= 0x10u && r <= 0x17u) {          /* 0x4A692/0x4A697 */
                if (rng_next(2u) != 0u) {                   /* 0x4A69E */
                    (void)rng_next(2u);                     /* 0x4A6A9 */
                    /* PORT: 0x4A6B7/0x4A6C8 0x2C3FC(0xC9 or 0xCA) and 0x4A6D2
                     * 0x2C3FC(0xDA or 0xDB) voices, out of scope (spec §7). */
                }
            }
        }
        DSB(slot + 0x42u) &= 0xFCu;                         /* 0x4A6DD/0x4A6E0 */
        DSB(DS_0010889E + side) = 0;                        /* 0x4A6EE */
        DSB(DS_001088B2 + side) = 0;                        /* 0x4A6F4 */
    }
}

/* The trample tables of 0x4B470 and the grab-move byte of 0x4B788, all DAT_ in
 * Ghidra (no DS_ name from gen_symbols.py): 0xC9604[si] the tumble stream
 * (0x4B4A6 `mov edx,[ebx + 0xc9604]`), 0xBB920[si] the shadow actor's descriptor
 * (0x4B4CA `mov eax,[ebx + 0xbb920]`), 0xC97F2[ch] the fighter's grab move
 * (0x4B7EF `cmp al,[esi + 0xc97f2]`), and the byte 0x1088F2 0x4B564 reads as
 * `mov eax,[0x1088ef]` / `sar eax,0x18`. */
#define DS_000C9604 0x000C9604u
#define DS_000BB920 0x000BB920u
#define DS_000C97F2 0x000C97F2u
#define DS_001088EF 0x001088EFu

/* 0x4B788 — demo-pose record §29. Fighter side `hit - 1` touching the entry:
 * 1 (the caller tramples it) when DS_00105B3A > 1, when the other side's slot
 * +0x54 is 3, or when the side's slot +0x5F is not its character's grab move
 * 0xC97F2[ch]; else 0 — the grab arm, which returns 0 without grabbing when
 * the entry is already held (+0x1C bit 6), the fighter record's +0x52 is out
 * of [0xC97E4[ch], 0xC97EB[ch]] (signed bytes) or its +0x4B is set. EAX = hit
 * (1/2), EDX = entry, EBX = si. */
static int fight_4b788(u32 hit, u32 entry, u32 index)
{
    (void)index;
    if ((u32)DSB(DS_00105B3A) > 1u) return 1;                   /* 0x4B7A0 */
    u32 s = hit - 1u;                                           /* 0x4B7A9 */
    u32 slot = DS_001077B0 + s * 0x94u;                         /* 0x4B7C4 */
    if (DSB(DS_001077B0 + (s ^ 1u) * 0x94u + 0x54u) == 3u)      /* 0x4B7C0/0x4B7DC */
        return 1;
    s32 ch = (s32)(s8)DSB(slot + 0x7Au);                        /* 0x4B7E9 */
    if (DSB(slot + 0x5Fu) != DSB(DS_000C97F2 + (u32)ch))        /* 0x4B7F5 */
        return 1;
    if ((DSB(entry + 0x1Cu) & 0x40u) != 0u) return 0;           /* 0x4B810 */
    u32 fr = DSD(slot);                                         /* 0x4B816 */
    s8 st = (s8)DSB(fr + 0x52u);
    if (st < (s8)DSB(DS_000C97E4 + (u32)ch)) return 0;          /* 0x4B821 */
    if (st > (s8)DSB(DS_000C97EB + (u32)ch)) return 0;          /* 0x4B82D */
    if (DSB(fr + 0x4Bu) != 0u) return 0;                        /* 0x4B837 */
    /* PORT: 0x4B83D..0x4B98F, the grab (+0x1C |= 0x40, the shadow killed, the
     * worshipper placed at the fighter's 0xC977D offsets, type 8 on the
     * 0xC97AC[ch][si] stream, 0x2BD20, the fighter on 0xC9790[ch], the voice) is
     * a named gap: the demo's fighters never touch a worshipper in their grab
     * move (§29). It returns 0 like the raw. */
    return 0;                                                   /* 0x4B994 */
}

/* 0x4B470 — demo-pose record §29. The trample: the entry's actor takes the
 * 0xC9604[si] tumble stream at 3.0, gets a shadow actor (0x2AE14 from
 * 0xBB920[si] at its x and y) when +0x10 has none, is thrown (+0x34 = ±0x80, away
 * from the hitter on the first hit (0x1A570 of the +0x20 side), else reversed
 * from its current +0x34; not in mode 0x22) with +0x36 = 0x240, and the entry
 * becomes type 6 with +0x1C bit 7 cleared. EAX = entry, EDX = si. */
static void fight_4b470(u32 entry, u32 index)
{
    /* PORT: 0x4B497 0x2C3FC(0xD1 for si < 3, else 0xD0) — voice, out of scope
     * (spec §7). */
    u32 rec = DSD(entry + 8u);
    actors_anim_begin(rec, DSD(DS_000C9604 + index * 4u), 0x40400000u); /* 0x4B4B1 */
    if (DSD(entry + 0x10u) == 0u) {                             /* 0x4B4BB */
        u32 sh = actor_spawn((const u32 *)(mem + DSD(DS_000BB920 + index * 4u)),
                             DSD(rec + 0x18u),
                             (u32)((s32)DSD(rec + 0x30u) >> 16), 0u, 0u); /* 0x4B4D2 */
        DSD(entry + 0x10u) = sh;                                /* 0x4B4D7 */
    }
    if (DSW(DS_00104B00) != 0x22u) {                            /* 0x4B4E5 */
        if (DSB(entry + 0x1Fu) == 1u) {                         /* 0x4B4EF */
            if (fighter_actor_bit15_clear((u32)DSB(entry + 0x20u)) != 0) /* 0x4B4F6 */
                DSW(rec + 0x34u) = 0xFF80u;                     /* 0x4B525 */
            else
                DSW(rec + 0x34u) = 0x0080u;                     /* 0x4B507 */
        } else if (((s32)DSD(rec + 0x32u) >> 16) == -0x80) {    /* 0x4B518 */
            DSW(rec + 0x34u) = 0x0080u;                         /* 0x4B51D */
        } else {
            DSW(rec + 0x34u) = 0xFF80u;                         /* 0x4B525 */
        }
    }
    DSW(rec + 0x36u) = 0x0240u;                                 /* 0x4B52E */
    DSB(entry + 0x1Eu) = 6u;                                    /* 0x4B537 */
    DSB(entry + 0x1Cu) = (u8)(DSB(entry + 0x1Cu) & 0x7Fu);      /* 0x4B544 */
    if (DSB(DS_00104B1D) == 3u || DSB(DS_00104B1D) == 2u) return;  /* 0x4B547/0x4B54C */
    if (DSW(DS_00104AFC) != 0u) return;                         /* 0x4B559 */
    if (DSB(DS_001088C1) != 0u) return;                         /* 0x4B562 */
    if (((s32)DSD(DS_001088EF) >> 24) <= 1) return;             /* 0x4B56F */
    if ((u32)DSB(entry + 0x1Fu) <= 7u) return;                  /* 0x4B579 */
    if (DSB(DS_001088C5) != 0u) return;                         /* 0x4B582 */
    if (DSD(DS_00108864) != 0u) return;                         /* 0x4B58C */
    /* PORT: 0x4B590 0x4BD98(entry) and 0x4B59E 0x4CB18(entry, si, +0x20), the
     * eighth-hit bonus, are a named gap (§29); not reached in the demo. */
}

/* 0x4B69C — demo-pose record §29. The effects pass's per-entry prelude
 * (0x49CFE, before the type dispatch; EAX = entry, EDX = si): an entry whose
 * +0x1C bit 7 is set (a lying or falling worshipper) tests its actor's pset
 * point (the words at pset +4/+8) against both fighters (0x17D30, BX = 0).
 * Both sides hit counts as side 0. When 0x4B788 lets it through, +0x20 = the
 * hitter side, +0x1F counts the hit, a held actor (+0x4A) is released, and
 * 0x4B470 tramples it. */
static void fight_4b69c(u32 entry, u32 index)
{
    if ((DSB(entry + 0x1Cu) & 0x80u) == 0u) return;              /* 0x4B6B3 */
    u32 rec = DSD(entry + 8u);
    u32 ps = DSD(DS_001014EC) + ((u32)DSW(rec + 0x56u) << 5);  /* 0x4B6BC..0x4B6CD */
    u32 hit = camera_point_hit((s32)(s16)DSW(ps + 4u),
                               (s32)(s16)DSW(ps + 8u), 0u);     /* 0x4B6E2 */
    if (hit == 0u) return;                                      /* 0x4B6EC */
    if ((s32)hit > 2) hit = 1u;                                 /* 0x4B6F5 */
    if (fight_4b788(hit, entry, index) == 0) return;            /* 0x4B705 */
    DSB(entry + 0x20u) = (u8)(hit - 1u);                        /* 0x4B713 */
    DSB(entry + 0x1Fu) = (u8)(DSB(entry + 0x1Fu) + 1u);         /* 0x4B716 */
    u32 k = DSB(rec + 0x4Au);
    if (k != 0u) {                                              /* 0x4B720 */
        DSB(DSD(DS_001014F4) + k * 0x68u + 0x4Bu) = 0;          /* 0x4B73B */
        DSB(rec + 0x2Au) = (u8)(DSB(rec + 0x2Au) & 0xF7u);      /* 0x4B743 */
        DSB(rec + 0x29u) = (u8)(DSB(rec + 0x29u) & 0xBFu);      /* 0x4B74A */
        DSB(rec + 0x4Au) = 0;                                   /* 0x4B751 */
        DSB(entry + 0x1Cu) = (u8)(DSB(entry + 0x1Cu) & 0xBFu);  /* 0x4B760 */
        DSB(DS_001088AE + (u32)DSB(entry + 0x21u)) =
            (u8)(DSB(DS_001088AE + (u32)DSB(entry + 0x21u)) + 1u); /* 0x4B763 */
        DSB(DS_001088B2 + (u32)DSB(entry + 0x21u)) = 1u;        /* 0x4B76E */
    }
    fight_4b470(entry, index);                                  /* 0x4B779 */
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
                /* PORT: 0x49CDD..0x49CF8. The per-side counters (the frame
                 * locals only the mode-9 block reads) are a named gap (§7.4).
                 * The `si` the prelude and the handlers index with is
                 * (u16)(rec+0x48 - 0x20) (0x49CE1/0x49CF2). */
                u32 index = (u32)(u16)((u32)DSB(rec + 0x48u) - 0x20u);
                fight_4b69c(entry, index);      /* 0x49CFE */

                /* 0x49D03: AL = +0x1E; CMP AL,0xE; JA 0x49D1E. The jump table
                 * at 0x49C2C sends type 0 to the same 0x49D1E (0x4AAD0), so
                 * both type 0 and >0xE take it; types 1 (0x49D2F), 3..6
                 * (0x49DB3, 0x49E5A, 0x49EC0, 0x49F11) and 8's gate (0x4A08A)
                 * are ported, types 2, 7 and 9..12 are their own (unported)
                 * handlers and stay named gaps (§7.4). The type is read after
                 * the prelude, which can make it 6. */
                u8 type = DSB(entry + 0x1Eu);
                if (type == 0u || type > 0xEu) {
                    fight_4aad0(entry, index);  /* 0x49D1E */
                } else {
                switch (type) {
                case 1: {
                    /* 0x49D2F: the walk's arrival test. The actor stops once
                     * |actor+0x18 - entry+0x14| is within one step, the
                     * magnitude of the velocity word +0x34 (read as the high
                     * word of the dword at +0x32, 0x49D67/0x49D71 `mov eax,
                     * [eax+0x32]` then `sar eax,0x10`, negated when the word is
                     * negative: 0x49D60 `cmp word [eax+0x34],0`). The compare
                     * is signed (0x49D79 `jg`). */
                    if (DSB(DS_001088C2) != 0u) {           /* 0x49D2F */
                        if (fight_4bd4c(entry, index) != 0) break;  /* 0x49D3F */
                    }
                    s32 d = (s32)DSD(rec + 0x18u) - (s32)DSD(entry + 0x14u); /* 0x49D55 */
                    if (d < 0) d = -d;                      /* 0x49D5B */
                    s32 step = (s32)DSD(rec + 0x32u) >> 16; /* 0x49D6A/0x49D74 */
                    if ((s16)DSW(rec + 0x34u) < 0) step = -step;    /* 0x49D6D */
                    if (d <= step)                          /* 0x49D79 */
                        fight_4ac38(entry, index);          /* 0x49D86 */
                    break;
                }
                case 3:
                    /* 0x49DB3: the fall. The actor's +0x2C follows its y
                     * (0x496AC); once the y is at or below the zero-extended
                     * word DS_000BD898 (0x49DCD `mov ax,[0xbd898]` after `xor
                     * eax,eax`, 0x49DD6 signed `cmp`/0x49DD8 `jg`) it lands:
                     * hflip when 0x2BE1C > 0 (an OR only, 0x49E0B), the
                     * 0xC9634[si] stream at 2.0 (`push 0x40000000`), the
                     * velocities +0x38/+0x34 zeroed, the entry's +0x18 word
                     * timer = rng(0x3C) + 0x3C, +0x1C |= 0x80 and type 4. */
                    DSW(rec + 0x2Cu) = fight_dust_clamp((s32)DSD(rec + 0x30u) >> 16); /* 0x49DC4 */
                    if ((s32)DSD(rec + 0x30u) >> 16
                            > (s32)(u32)DSW(DS_000BD898))           /* 0x49DD8 */
                        break;
                    if (fight_2be1c(rec, DSD(DSD(entry + 0xCu))) > 0) /* 0x49DE6 */
                        DSW(rec + 0x28u) = (u16)(DSW(rec + 0x28u) | 0x4000u); /* 0x49E0D */
                    actors_anim_begin(rec, DSD(DS_000C9634 + index * 4u),
                                      0x40000000u);         /* 0x49E24 */
                    DSW(rec + 0x38u) = 0;                   /* 0x49E29 */
                    DSW(rec + 0x34u) = 0;                   /* 0x49E34 */
                    DSW(entry + 0x18u) = (u16)(rng_next(0x3Cu) + 0x3Cu); /* 0x49E3A 0x49E4E */
                    DSB(entry + 0x1Eu) = 4u;                /* 0x49E47 */
                    DSB(entry + 0x1Cu) = (u8)(DSB(entry + 0x1Cu) | 0x80u); /* 0x49E52 */
                    break;
                case 4: {
                    /* 0x49E5A: the lie. The entry's +0x18 word counts down
                     * (signed, 0x49E66 `jg`); at zero the actor takes the
                     * 0xC95EC[si] stream at 3.0, sets +0x38 = 0x40 and +0x34 =
                     * -0x40 when the fighter record's +0x28 word has bit 0x4000
                     * (0x49E94/0x49E96), else 0x40; +0x1C loses bit 0x80 and
                     * the entry becomes type 5. */
                    u16 t = (u16)(DSW(entry + 0x18u) - 1u);  /* 0x49E5E */
                    DSW(entry + 0x18u) = t;                  /* 0x49E5F */
                    if ((s16)t > 0) break;                   /* 0x49E66 */
                    actors_anim_begin(rec, DSD(DS_000C95EC + index * 4u),
                                      0x40400000u);          /* 0x49E80 */
                    DSW(rec + 0x38u) = 0x0040u;              /* 0x49E85 */
                    if ((DSW(DSD(DSD(entry + 0xCu)) + 0x28u) & 0x4000u) != 0u) /* 0x49E96 */
                        DSW(rec + 0x34u) = 0xFFC0u;          /* 0x49EA0 */
                    else
                        DSW(rec + 0x34u) = 0x0040u;          /* 0x49EA8 */
                    DSB(entry + 0x1Eu) = 5u;                 /* 0x49EB1 */
                    DSB(entry + 0x1Cu) = (u8)(DSB(entry + 0x1Cu) & 0x7Fu); /* 0x49EB8 */
                    break;
                }
                case 5:
                    /* 0x49EC0: the climb. +0x2C follows the y (0x496AC); once
                     * the y word +0x32 is back at the entry's +0x1A (signed,
                     * 0x49EDD `jl`), the actor takes the 0xC9544[si] stream at
                     * 3.0, stops (+0x38/+0x34 zeroed) and the entry returns to
                     * type 0. */
                    DSW(rec + 0x2Cu) = fight_dust_clamp((s32)DSD(rec + 0x30u) >> 16); /* 0x49ED1 */
                    if ((s16)DSW(rec + 0x32u) < (s16)DSW(entry + 0x1Au)) /* 0x49ED9 */
                        break;
                    actors_anim_begin(rec, DSD(DS_000C9544 + index * 4u),
                                      0x40400000u);          /* 0x49EF7 */
                    DSW(rec + 0x38u) = 0;                    /* 0x49EFC */
                    DSW(rec + 0x34u) = 0;                    /* 0x49F02 */
                    DSB(entry + 0x1Eu) = 0;                  /* 0x49F08 */
                    break;
                case 6: {
                    /* 0x49F11: the tumble 0x4B470 starts. While the actor's
                     * +0x36 word is negative (falling) the entry's +0x1C bit 7
                     * is set again, so 0x4B69C can re-hit it. The shadow
                     * actor (+0x10) follows the x dword and the y word +0x32.
                     * While the height +0x1C plus the +0x36 step (the high word
                     * of the dword at +0x34, 0x49F3A/0x49F40) stays positive,
                     * +0x36 falls by 0x10 a frame; at or below zero the shadow
                     * dies and the actor lands on the 0xC973C[si] stream at 2.0
                     * as type 8 (or, when that entry is 0, the 0xC9544[si]
                     * stream at 3.0 as type 4), with +0x1C, +0x36, +0x34 and the
                     * entry's hit count +0x1F zeroed. */
                    if ((s16)DSW(rec + 0x36u) < 0)           /* 0x49F16 */
                        DSB(entry + 0x1Cu) = (u8)(DSB(entry + 0x1Cu) | 0x80u); /* 0x49F18 */
                    u32 sh = DSD(entry + 0x10u);
                    if (sh != 0u) {                          /* 0x49F21 */
                        DSD(sh + 0x18u) = DSD(rec + 0x18u);  /* 0x49F29 */
                        DSW(sh + 0x32u) = DSW(rec + 0x32u);  /* 0x49F36 */
                    }
                    if (((s32)DSD(rec + 0x34u) >> 16)
                            + (s32)DSD(rec + 0x1Cu) > 0) {   /* 0x49F47 */
                        DSW(rec + 0x36u) = (u16)(DSW(rec + 0x36u) - 0x10u); /* 0x49FB8 */
                        break;
                    }
                    if (DSD(entry + 0x10u) != 0u) {          /* 0x49F4E */
                        actor_set_dead(DSD(entry + 0x10u));  /* 0x49F52 0x2B150 */
                        DSD(entry + 0x10u) = 0;              /* 0x49F57 */
                    }
                    {
                        u32 st = DSD(DS_000C973C + index * 4u); /* 0x49F66 */
                        if (st != 0u) {
                            actors_anim_begin(rec, st, 0x40000000u); /* 0x49F78 */
                            DSB(entry + 0x1Eu) = 8u;         /* 0x49F7D */
                        } else {
                            actors_anim_begin(rec, DSD(DS_000C9544 + index * 4u),
                                              0x40400000u);  /* 0x49F91 */
                            DSB(entry + 0x1Eu) = 4u;         /* 0x49F96 */
                        }
                    }
                    DSD(rec + 0x1Cu) = 0;                    /* 0x49F9A */
                    DSW(rec + 0x36u) = 0;                    /* 0x49FA1 */
                    DSW(rec + 0x34u) = 0;                    /* 0x49FAB */
                    DSB(entry + 0x1Fu) = 0;                  /* 0x49FAF */
                    break;
                }
                case 8:
                    /* 0x4A08A: the held worshipper. Without +0x1C bit 6 (set
                     * by the grab arms: 0x4B788's, and 0x4D898's at 0x4D963 in
                     * 0x4D2D0's pass, not ported) it does nothing. */
                    if ((DSB(entry + 0x1Cu) & 0x40u) == 0u)  /* 0x4A097 */
                        break;
                    /* PORT: 0x4A09D..0x4A110, the held body (0x4AF04, the
                     * release and 0x4B470), is a named gap with the grab arm
                     * that sets bit 6 (§29). */
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
                    /* PORT: types 2, 7 and 9..12 are named gaps (§7.4). */
                    break;
                }
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

    fight_4a634();                              /* 0x4A591 */
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
