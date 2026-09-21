/* Port of the two per-frame fighter passes the arena frame calls around its
 * think step, plus the pure per-fighter helpers the HUD spine shares.
 * Addresses, gates, widths and the single RNG site are from
 * docs/superpowers/plans/2026-09-20-demo-fight-derivations.md §3.5 and §5.8.
 * The module registers nothing. Task 4 adds the think/AI chain here. */
#include "game/fighter.h"
#include "game/rng.h"
#include "../mem.h"
#include "../symbols.h"

/* ---- the shared per-fighter helpers ------------------------------------ */

void fighter_ctx_swap(u32 out[6], u32 side)
{
    u32 other = 1u - side;
    out[0] = other;                                 /* 0x33A16 */
    out[1] = side;                                  /* 0x33A19 */
    out[2] = DS_001077B0 + other * 0x94u;           /* 0x33A35 */
    out[3] = DS_001077B0 + side * 0x94u;            /* 0x33A56 */
    out[4] = DSD(out[2]);                           /* 0x33A5E */
    out[5] = DSD(out[3]);                           /* 0x33A63 */
}

void fighter_ctx_same(u32 out[6], u32 side)
{
    u32 other = 1u - side;
    out[0] = side;                                  /* 0x33956 */
    out[1] = other;                                 /* 0x3395A */
    out[2] = DS_001077B0 + side * 0x94u;            /* 0x33977 */
    out[3] = DS_001077B0 + other * 0x94u;           /* 0x33998 */
    out[4] = DSD(out[2]);                           /* 0x3399D */
    out[5] = DSD(out[3]);                           /* 0x339A3 */
}

void fighter_anim_triple(u32 out[3], u32 slot_char, s32 edx)
{
    /* 0x3AFC5: edx outside [0,0x40) is the raw's 0x62003 error path; 0x62003 is
     * out of scope, so the port leaves the triple zero there. */
    if (edx < 0 || edx >= 0x40) {
        out[0] = out[1] = out[2] = 0;
        return;
    }
    u32 c = ((u32)DSB(DS_0010782A + slot_char * 0x94u) << 6) + (u32)edx;
    out[0] = 0x000DE114u + c * 11u;                 /* 0x3B007 */
    out[1] = 0x000A3528u + c * 20u;                 /* 0x3B028 */
    out[2] = 0x000A6728u + c * 6u;                  /* 0x3B033 */
}

int fighter_state_ok(u32 side)
{
    u32 ctx[6];
    fighter_ctx_swap(ctx, side);
    u32 self = ctx[3];
    if ((u8)DSB(self + 0x54u) > 1u) return 0;       /* 0x1AB21 */
    if ((u8)DSB(self + 0x53u) > 1u) return 0;       /* 0x1AB3C */
    return 1;
}

int fighter_actor_bit15_clear(u32 side)
{
    u32 rec = DSD(DS_001077B0 + side * 0x94u);
    u32 actor = DSD(DS_001014EC) + (u32)DSW(rec + 0x56u) * 0x20u;
    return (DSW(actor) & 0x8000u) == 0;
}

/* 0x3C570. Test-and-set bit `bit` of DS_00107EE0: 1 when it was already set,
 * else set it and return 0. */
static int fighter_slot_flag(u32 bit)
{
    u32 m = 1u << (bit & 0xffu);
    if ((DSD(DS_00107EE0) & m) != 0) return 1;
    DSD(DS_00107EE0) |= m;
    return 0;
}

/* ---- 0x1958C the first per-frame pass ---------------------------------- */

void fighter_pass_a(void)
{
    if (DSB(DS_001078FA) != 2) return;              /* 0x1959C */

    for (u32 side = 0; side < 2; side++) {
        u32 ctx[6];
        fighter_ctx_same(ctx, side);                /* 0x195AF 0x33950 */

        /* PORT: 0x195B6 0x19020(side) — the per-slot hook is a named gap
         * (§7.6); the raw calls a function pointer at slot+0x18 and writes
         * DS_00100AF8[side] from its result. */

        /* 0x195BB: a side whose slot state byte is 0x0A is out. */
        if (DSB(DS_00107803 + side * 0x94u) == 0x0Au)
            DSD(DS_00100AF8 + side * 4u) = 0;

        /* 0x195CC: the 0x3AFC4-selected animation gates the second clear. */
        {
            u32 self = ctx[2];
            u8 st = (u8)DSB(self + 0x5fu);
            if (st < 0x40u) {                       /* 0x195DC */
                u32 anim[3];
                fighter_anim_triple(anim, ctx[0], (s32)st);   /* 0x195E5 */
                if (DSW(self + 0x76u) > 0u) {       /* 0x195EE */
                    if ((DSW(anim[2] + 2u) & 0x8000u) == 0) { /* 0x19607 */
                        u32 other = ctx[3];
                        if ((DSB(other + 0x40u) & 0x80u) == 0)  /* 0x19611 */
                            DSD(DS_00100AF8 + side * 4u) = 0;
                    }
                }
            }
        }
    }

    /* 0x19632: either flag clear skips straight to the winner tail. */
    if (DSD(DS_00100AF8) == 0 || DSD(DS_00100AFC) == 0) goto tail;   /* 0x19720 */

    /* PORT: 0x19653/0x19661 0x18950(0,1) and 0x18950(1,0) — the two
     * reachability queries are named gaps (§7.6); the port takes both as 0, so
     * the exact-tie branch below is reached. */
    {
        u8 bl = 0;
        u8 al = 0;

        /* 0x19666: the two +0x40 bit-7 overrides. */
        if ((DSB(DS_001077F0) & 0x80u) != 0) {
            if ((DSB(DS_00107884) & 0x80u) != 0) goto after_overrides;
            al = 1; bl = 0;
        }
        if ((DSB(DS_00107884) & 0x80u) != 0) { bl = 1; al = 0; }

    after_overrides:
        if (bl) {                                   /* 0x19694 */
            if (!al) { DSD(DS_00100AFC) = 0; goto tail; }
        }
        if (al) {                                   /* 0x196a7 */
            if (!bl) { DSD(DS_00100AF8) = 0; goto tail; }
        }

        /* 0x196BC: the two +0x34 position words decide, then the +0x8A pair. */
        if (DSW(DS_00107838) < DSW(DS_001078CC)) { DSD(DS_00100AFC) = 0; goto tail; }
        if (DSW(DS_001078CC) < DSW(DS_00107838)) { DSD(DS_00100AF8) = 0; goto tail; }
        if (DSB(DS_0010780A) > DSB(DS_0010789E)) { DSD(DS_00100AFC) = 0; goto tail; }
        if (DSB(DS_0010789E) > DSB(DS_0010780A)) { DSD(DS_00100AF8) = 0; goto tail; }

        /* 0x1970D: the exact tie draws rng(2) and clears that flag. */
        {
            u32 i = rng_next(2u);                   /* 0x19714 */
            DSD(DS_00100AF8 + i * 4u) = 0;
        }
    }

tail:
    /* 0x19720: whichever flag survives runs 0x193B0 (a named gap §7.6). */
    if (DSD(DS_00100AF8) != 0 && DSB(DS_0010783A) != 0) {
        /* PORT: 0x1974D 0x193B0(0) — named gap (§7.6). */
    } else if (DSD(DS_00100AFC) != 0 && DSB(DS_001078CE) != 0) {
        /* PORT: 0x1974D 0x193B0(1) — named gap (§7.6). */
    }
}

/* ---- 0x19068 the second per-frame pass --------------------------------- */

void fighter_pass_b(u32 arg)
{
    if (DSB(DS_00107802) == 0x13u) return;          /* 0x1906E */
    if (DSB(DS_00107896) == 0x13u) return;          /* 0x1907B */

    /* 0x19088: the frame passes EAX=0; 0x3C570(5) is then a once-per-frame
     * test-and-set that 0x3C5CC has just cleared, so it never returns 1. */
    if ((arg & 0xffu) == 0u) {
        if (fighter_slot_flag(5u) != 0) return;     /* 0x19091/0x19098 */
    }

    for (u32 side = 0; side < 2; side++) {
        u32 slot = DS_001077B0 + side * 0x94u;
        u8 st = (u8)DSB(slot + 0x5fu);

        /* 0x190A8: a stance byte that differs from the latched DS_00100B58 is
         * handed to 0x1922C (a named gap §7.6). */
        if (st != DSB(DS_00100B58 + side) && st != 0xFFu) {
            /* PORT: 0x190C1 0x1922C(side) — named gap (§7.6). */
            continue;
        }

        /* 0x190CB: the +0x5E hit-stun countdown. */
        {
            u8 ch = (u8)(DSB(DS_00100B5E + side) - 1u);
            DSB(DS_00100B5E + side) = ch;
            if ((s8)ch > 0) {
                DSB(slot + 0x56u) = 0;              /* 0x190E1 */
                /* PORT: 0x190E7 0x3CF38(side) — named gap (§7.6). */
            } else {
                DSB(DS_00100B5E + side) = 0;        /* 0x190F0 */
            }
        }

        /* 0x190F6: the +0x5A stance timer. The raw reads the high byte of the
         * dword at DS_00100B57+side, i.e. the signed byte at DS_00100B5A+side. */
        {
            s8 v = (s8)DSB(DS_00100B5A + side);
            if (v > 1) {
                DSB(DS_00100B5A + side) = (u8)(DSB(DS_00100B5A + side) - 1u);
                continue;                           /* 0x1910A */
            }
            if (v != 1) continue;                   /* 0x1910C */

            /* 0x1910E: arm the record's animation float from +0x5C. */
            {
                u32 rec = DSD(slot);
                if ((DSD(rec + 0x24u) & 0x7FFFFFFFu) == 0) {
                    union { float f; u32 u; } fu;
                    fu.f = (float)(s16)(s8)DSB(DS_00100B5C + side);
                    DSD(rec + 0x24u) = fu.u;        /* 0x1912B fstp [ecx+0x24] */
                    DSD(rec + 0x20u) = 0;           /* 0x19134 */
                }
                DSB(DS_00100B5A + side) = 0;        /* 0x1913F */
                DSB(DS_00100B5E + side) = 0x0Au;    /* 0x19145 */
            }
        }
    }
}
