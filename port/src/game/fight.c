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
    for (u32 side = 0; side < 2u; side++) {
        DSB(DS_00107EE4) = (u8)fighter_actor_bit15_clear(side);   /* 0x3CB89 */
        for (u32 i = 0; i < 0x20u; i++) {
            /* PORT: 0x3CB96 0x3C88C — the 64-call slot/draw helper is a named
             * gap (§7.7); it reads DS_00107ED8/DC/E4, which the port keeps. */
        }
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

static void fight_health_sync(u32 side)
{
    u32 rec = DSD(DS_001077A8 + side * 4u);     /* 0x34B73 */
    if (rec == 0) return;                       /* 0x34B7C */
    if (DSD(rec) == 0) return;                  /* 0x34B82/0x34B86 */

    /* PORT: 0x34B8C..0x34BF3. The 0x36E2C(rec) gate and the 0x36638/0x36F10
     * position branch are named gaps (§7.10); the raw reaches the +0x52
     * dispatch below only when 0x36E2C returns 0. */
    if (DSB(rec + 0x52u) == 6)                  /* table 0x34B14 -> 0x34C73 */
        fight_stance_pass(side);                /* 0x34C75 0x1A978 */
    /* PORT: 0x34C08..0x34D88. The other +0x52 cases are named gaps (§7.10). */
}

/* ---- 0x35658 the HUD/health pass --------------------------------------- */

void fight_hud_pass(u32 side)
{
    u32 rec = DSD(DS_001077A8 + side * 4u);     /* 0x35775 */
    if (rec == 0) return;                       /* 0x3577E */

    /* PORT: 0x35661..0x35773. The per-side preamble (the 0x3C59C(3) gate, the
     * DS_00107828/38/3c/40/42 timer traffic, DS_001078fb and DS_001088d4) is a
     * named gap (§7.8); the spine below is what is pinned. */
    if (DSD(DS_00104B00) == 4 && (DSB(rec + 0x43u) & 0x80u) == 0) {
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
        DSB(fighter + 0x28u) = (u8)(DSB(fighter + 0x28u) | 1u); /* 0x35808..0x35810 */
    }
    /* PORT: 0x35803 0x3531C(side), 0x35813 0x2A1FC(rec), 0x3581C/0x35824
     * 0x354F0(side)/(1-side) and 0x35829 0x186C4 run after it; all named gaps
     * (§7.8) and skipped. */
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
