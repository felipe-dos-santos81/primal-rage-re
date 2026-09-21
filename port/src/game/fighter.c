/* Port of the two per-frame fighter passes the arena frame calls around its
 * think step, plus the pure per-fighter helpers the HUD spine shares, plus the
 * think/AI chain 0x1975C -> 0x3B464 -> 0x3B298 -> 0x3B134 -> 0x3BDDC.
 * Addresses, gates, widths and the RNG sites are from
 * docs/superpowers/plans/2026-09-20-demo-fight-derivations.md §3.5, §5 and §8.
 * The module registers nothing; the think chain's command consumer lives here
 * and is called back from fight.c's 0x3B134. */
#include "game/fighter.h"
#include "game/fight.h"
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

        /* 0x190A8: the side is handed to 0x1922C (a named gap §7.6) unless the
         * stance byte equals the latched DS_00100B58 AND is not 0xFF. The raw
         * branches to the skip on `st != B58` (0x190AE) or `st == 0xFF`
         * (0x190BD falls through, not to 0x190CB). */
        if (st != DSB(DS_00100B58 + side) || st == 0xFFu) {
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

/* ---- the think/AI chain 0x1975C --------------------------------------- */

/* 0x1082D2. The raw reads the dword at 0x1082D2 and arithmetic-shifts it right
 * 16; symbols.h names 0x1082D0 but not this word. */
#define DS_001082D2 0x001082D2u

/* 0x46460. One word of player `side`'s 0x28-stride input-history ring at
 * `index` steps behind the ring position (wrapping modulo 0x14). */
static u32 fighter_input_read(u32 side, s32 index)
{
    s32 pos = (s32)DSD(DS_001082D2) >> 16;      /* 0x46466/0x4646E */
    if (index > 0) {
        s32 n = index;
        do {
            if (--pos < 0) pos = 0x13;          /* 0x4647A/0x4647F */
        } while (--n > 0);
    }
    return DSW(DS_00108270 + side * 0x28u + (u32)pos * 2u);   /* 0x4648F */
}

/* 0x4649C. Skip `n1` ring entries, then scan the next `n2` for a word whose low
 * 16 bits overlap `mask`; 1 on the first hit, 0 otherwise. */
static int fighter_input_scan(u32 side, s32 n1, s32 n2, u32 mask)
{
    s32 pos = (s32)DSD(DS_001082D2) >> 16;      /* 0x464A5/0x464AC */
    if (n1 > 0) {
        s32 n = n1;
        do {
            if (--pos < 0) pos = 0x13;          /* 0x464B8 */
        } while (--n > 0);
    }
    if (n2 > 0) {                               /* 0x464C6 */
        s32 i = 0;
        do {
            u16 v = DSW(DS_00108270 + side * 0x28u + (u32)pos * 2u);  /* 0x464D4 */
            if ((v & (u16)mask) != 0) return 1; /* 0x464E2/0x464E6 */
            if (--pos < 0) pos = 0x13;          /* 0x464F1 */
        } while (++i < n2);
    }
    return 0;                                   /* 0x464FB */
}

/* 0x1AB5C. The facing word 0x3B298 compares against: it ORs seven input-ring
 * reads with the side's command word, then returns the 0x1000/0x2000/0x3000
 * facing base, or 0 when 0x1AB10 rejects the fighter. The raw returns EDX (the
 * base), not the accumulated OR. 0x18B04/0x1A7CC are gaps (§7.12). */
static u32 fighter_input_mask(u32 side)
{
    u32 ctx[6];
    fighter_ctx_swap(ctx, side);                /* 0x1AB6C */
    u32 mask = 0;
    for (s32 i = 0; i < 7; i++)                 /* 0x1AB7F */
        mask |= fighter_input_read(ctx[1], i);  /* 0x46460 */
    mask |= DSW(DS_001088E0 + ctx[1] * 2u);     /* 0x1AB88/0x1AB95 */
    if (!fighter_state_ok(ctx[1])) {            /* 0x1AB90/0x1AB99 */
        DSB(ctx[3] + 0x43u) &= 0xCFu;           /* 0x1AB9F */
        return 0;                               /* 0x1ABA3 */
    }
    s32 self2c = (s32)DSD(ctx[3] + 0x2cu);      /* 0x1ABB2 */
    s32 other2c = (s32)DSD(ctx[2] + 0x2cu);     /* 0x1ABB5 */
    u32 base;
    if (self2c < other2c) base = 0x2000u;       /* 0x1ABBA */
    else if (self2c > other2c) base = 0x1000u;  /* 0x1ABD1 */
    else base = 0x3000u;                        /* 0x1ABD8 */
    u8 bl = 0;
    if (DSB(ctx[2] + 0x5fu) != 0xFFu) {         /* 0x1ABEB */
        if (((u16)mask & (u16)base) != 0) {     /* 0x1ABFC */
            bl = 1;                             /* 0x1AC04 */
            /* PORT: 0x1AC06 0x18B04(side) — named gap (§7.12). */
        }
    } else if (DSB(ctx[2] + 0x64u) != 0xFFu) {  /* 0x1AC19 */
        u32 rec2 = DSD(ctx[2] + 0x08u);         /* 0x1AC24 */
        if (rec2 != 0) {                        /* 0x1AC29 */
            if ((s16)DSW(rec2 + 0x34u) < 0) {   /* 0x1AC2B */
                if ((mask & 0x2000u) != 0) bl = 1;   /* 0x1AC3E */
            } else if ((mask & 0x1000u) != 0) {
                bl = 1;                         /* 0x1AC4E */
            }
        }
    }
    if (bl == 0 && DSB(ctx[3] + 0x53u) != 1u)   /* 0x1AC52/0x1AC5E */
        return base;                            /* 0x1AC7F */
    DSB(ctx[3] + 0x54u) = (u8)((mask & 0x4000u) != 0);   /* 0x1AC73 */
    /* PORT: 0x1AC7A 0x1A7CC(side) — named gap (§7.12). */
    return base;                                /* 0x1AC7F */
}

/* 0x39F40. Arm a pose: seed the four per-side pose words, set the slot's state
 * bytes and its +0x10 handler, and zero the record's +0x24. */
static void fighter_pose_start(u32 side, u32 edx, u32 ebx, u32 ecx, u32 frame)
{
    u32 ctx[6];
    fighter_ctx_swap(ctx, side);                /* 0x39F4A */
    u32 s = ctx[1];
    DSD(DS_00107A68 + s * 4u) = edx;            /* 0x39F53 */
    DSD(DS_00107A78 + s * 4u) = ebx;            /* 0x39F5A */
    DSD(DS_00107A60 + s * 4u) = ecx;            /* 0x39F65 */
    DSD(DS_00107A70 + s * 4u) = frame;          /* 0x39F6C */
    DSB(ctx[3] + 0x52u) = 0x10u;                /* 0x39F77 */
    DSB(ctx[3] + 0x53u) = 0x0Au;                /* 0x39F7F */
    DSB(ctx[3] + 0x54u) = 2u;                   /* 0x39F87 */
    DSD(ctx[3] + 0x10u) = 0x39CC8u;             /* 0x39F8F */
    DSB(ctx[3] + 0x58u) = 0;                    /* 0x39F9A */
    DSD(ctx[5] + 0x24u) = 0;                    /* 0x39FA2 */
}

/* 0x3C59C. Test-and-set bit `bit` of DS_00107D50[index]: 1 when already set,
 * else set it and return 0. Distinct from 0x3C570's DS_00107EE0 bits. */
static int fighter_frame_flag(u32 bit, u32 index)
{
    u32 m = 1u << (bit & 0xffu);                /* 0x3C59E/0x3C5AC */
    u32 addr = DS_00107D50 + index * 4u;        /* 0x3C5A5 */
    if ((DSD(addr) & m) != 0) return 1;         /* 0x3C5B4/0x3C5B8 */
    DSD(addr) |= m;                             /* 0x3C5BF/0x3C5C1 */
    return 0;                                   /* 0x3C5C7 */
}

/* 0x3B298. The command/state dispatch: run the mapper, copy the other slot's
 * +0x84 into this slot's +0x86, then scan the input ring for the facing word.
 * Returns 1 when the scan sets the slot's +0x43 bit, else 0. 0x1A734 is a gap
 * (§7.11). */
static int fighter_command_dispatch(u32 side, u32 edx_arg)
{
    u32 ctx[6];
    fighter_ctx_swap(ctx, side);                        /* 0x3B2B2 */
    u32 anim[3];
    fighter_anim_triple(anim, ctx[0], (s32)edx_arg);    /* 0x3B2BC */
    fight_command_map(ctx[1], edx_arg, 0u);             /* 0x3B2C9 */

    DSW(ctx[3] + 0x86u) = DSW(ctx[2] + 0x84u);          /* 0x3B2D6 */

    /* 0x3B2E8: the scan is skipped only when anim[2]+2 has both bits 0 and 1. */
    u16 bits = DSW(anim[2] + 2u);
    if ((bits & 2u) != 0 && (bits & 1u) != 0)
        return 0;                                       /* 0x3B30C */

    u8 b1 = 0, b2 = 0;
    u32 mask = fighter_input_mask(ctx[1]);              /* 0x3B315 */
    s32 count = (s32)DSD(DS_000BEEF2) >> 16;            /* 0x3B361 */
    for (s32 i = 0; i < count; i++) {                   /* 0x3B320 */
        u16 v = (u16)fighter_input_read(ctx[1], i);     /* 0x3B326 */
        if ((v & (u16)mask) == 0) continue;             /* 0x3B335/0x3B337 */
        if ((v & 0x4000u) != 0) b2 = 1;                 /* 0x3B348 */
        else if ((v & 0x8000u) == 0) b1 = 1;            /* 0x3B35B */
    }
    {
        u16 cmd = DSW(DS_001088E0 + ctx[1] * 2u);       /* 0x3B376 */
        if ((cmd & (u16)mask) != 0) {                   /* 0x3B384 */
            if ((cmd & 0x4000u) != 0) b2 = 1;           /* 0x3B397 */
            else if ((cmd & 0x8000u) == 0) b1 = 1;      /* 0x3B3AA */
        }
    }
    if (((s32)DSD(DS_00100CDE + ctx[0] * 2u) >> 16) > 1   /* 0x3B3B2 */
            && (DSW(anim[2] + 2u) & 0x80u) != 0           /* 0x3B3C5 */
            && (DSB(ctx[3] + 0x43u) & 0x30u) != 0)        /* 0x3B3D8 */
        b1 = 1;                                           /* 0x3B3DE */

    if (b1 && (DSW(anim[2] + 2u) & 1u) == 0) {            /* 0x3B3E5 */
        DSB(ctx[3] + 0x43u) &= 0xCFu;                     /* 0x3B405 */
        DSB(ctx[3] + 0x43u) |= 0x20u;                     /* 0x3B40D */
    } else if (b2 && (DSW(anim[2] + 2u) & 2u) == 0) {     /* 0x3B413 */
        DSB(ctx[3] + 0x43u) &= 0xCFu;                     /* 0x3B433 */
        DSB(ctx[3] + 0x43u) |= 0x10u;                     /* 0x3B43B */
    } else {
        return 0;                                         /* 0x3B44A */
    }
    /* PORT: 0x3B443 0x1A734(ctx[1]) — named gap (§7.11). */
    return 1;                                             /* 0x3B448 */
}

/* 0x3B464. The per-fighter think driver. ctx[0]=1-side, ctx[1]=side,
 * ctx[2]=&slot[1-side], ctx[3]=&slot[side], ctx[4]=rec_other, ctx[5]=rec_self.
 * The unported branch targets are named gaps (§7.12). */
static void fighter_think_side(u32 side)
{
    u32 ctx[6];
    fighter_ctx_swap(ctx, side);                        /* 0x3B471 */
    if (fighter_frame_flag(1u, ctx[1]) != 0) return;    /* 0x3B47F */
    if ((s8)DSB(ctx[2] + 0x64u) == -1) return;          /* 0x3B490/0x3B49F */

    u32 stance = (u8)DSB(ctx[2] + 0x64u);               /* 0x3B493/0x3B4B9 */
    u32 anim[3];
    fighter_anim_triple(anim, ctx[0], (s32)stance);     /* 0x3B4AC */
    DSB(ctx[3] + 0x67u) = 1;                            /* 0x3B4BD */

    if (fighter_command_dispatch(ctx[1], stance) != 0) { /* 0x3B4C7 */
        DSB(ctx[2] + 0x8Au) = 0;                        /* 0x3B669 */
        if (DSB(ctx[3] + 0x54u) != 2u) {
            /* PORT: 0x3B69A 0x3B080(side, anim[0]+2, anim[0]+3, 0) §7.12. */
        }
        /* PORT: 0x3B6A7 0x3AD98(side, &anim) — named gap (§7.12). */
        goto tail;
    }

    /* 0x3B4D4: the record's +0x4B indexes the 0x68-stride table at
     * DS_001014F4; a non-zero +0x60 at that entry runs 0x2BD44. */
    {
        u8 idx = DSB(ctx[5] + 0x4Bu);                   /* 0x3B4D8 */
        if (idx != 0) {
            u32 t = DSD(DS_001014F4) + (u32)idx * 0x68u;/* 0x3B4E7..0x3B4FE */
            if (DSB(t + 0x60u) != 0) {
                /* PORT: 0x3B50A 0x2BD44(rec_self) — named gap (§7.12). */
            }
        }
    }
    /* 0x3B50F: the slot's +0x14 callback pointer. */
    if (DSD(ctx[3] + 0x14u) != 0) {
        /* PORT: 0x3B51B call [ctx[3]+0x14] — named gap (unresolved pointer). */
    }
    /* 0x3B52D: the other slot's secondary record +0x48. */
    {
        u32 r1 = DSD(ctx[2] + 0x08u);                   /* 0x3B531 */
        u8 a48 = DSB(r1 + 0x48u);                       /* 0x3B534 */
        if (a48 == 5) {                                 /* 0x3B53B */
            /* PORT: 0x3B638 0x36D20(ctx[3]) — named gap (§7.12). */
            DSD(ctx[3] + 0x18u) = 0;                    /* 0x3B63D */
            DSD(ctx[3] + 0x1Cu) = 0;                    /* 0x3B648 */
            goto tail;
        }
        if (a48 == 8) {                                 /* 0x3B543 */
            /* PORT: 0x3B657 0x1922C(side); 0x3B65E 0x235C4(side) — §7.12. */
            goto tail;
        }
    }

    /* PORT: 0x3B54F 0x39834(side, stance) — named gap (§7.12). */
    if (DSB(ctx[3] + 0x54u) == 2u) {                    /* 0x3B558 */
        /* PORT: 0x3B56C 0x18B04(side) — named gap (§7.12). */
        fighter_pose_start(side, 0xFFFFFFB0u, 0x64u, 0x0Fu, 0x14u);  /* 0x3B57C */
        goto middle;                                    /* 0x3B619 */
    }
    fighter_anim_triple(anim, ctx[0], (s32)stance);     /* 0x3B58F */
    /* PORT: 0x3B5A9 0x3A95C(side, (s8)anim[0]+6, ctx[3]+0x2C) — §7.12. */
    {
        u8 st = (u8)DSB(ctx[2] + 0x64u);                /* 0x3B5B2 */
        if (st != 0 && st != 5) {                       /* 0x3B5BE */
            u8 v = (u8)(DSB(ctx[3] + 0x90u) - 1u);      /* 0x3B5CF */
            if (v > 3u) {
                /* PORT: 0x3B5E5 0x188DC(side) — named gap (§7.12). */
            }
        }
    }
    if (DSB(ctx[3] + 0x54u) != 2u) {
        /* PORT: 0x3B614 0x3B080(side, anim[0]+2, anim[0]+3, 0) — §7.12. */
    }
middle:
    DSD(ctx[3] + 0x18u) = 0;                            /* 0x3B61D */
    DSD(ctx[3] + 0x1Cu) = 0;                            /* 0x3B624 */
tail:
    DSB(ctx[3] + 0x41u) |= 0x80u;                       /* 0x3B6B0 */
    DSB(ctx[2] + 0x64u) = 0xFFu;                        /* 0x3B6B8 */
}

/* 0x1975C. The think step: for each index whose DS_00100AD0 count exceeds 2,
 * run the think driver. The raw loops i in {0,1}, gates on DS_00100AD0[i], but
 * passes ctx[1] (=1-i) to 0x1922C and 0x3B464; the port keeps that flip. */
void fighter_think(void)
{
    for (u32 i = 0; i < 2u; i++) {                      /* 0x19770 */
        u32 ctx[6];
        fighter_ctx_same(ctx, i);                       /* 0x19778 */
        if (DSD(DS_00100AD0 + ctx[0] * 4u) <= 2u)       /* 0x19780/0x19787 */
            continue;
        /* PORT: 0x19791 0x1922C(ctx[1]) — named gap (§7.12). */
        /* PORT: 0x1979B 0x3962C(ctx[0]) — named gap (§7.12); the raw's true
         * branch clears slot+0x8A, calls 0x18B44 and ends the whole step. */
        /* PORT: 0x197BF 0x396AC(ctx[0]) — named gap, same shape. */
        DSB(ctx[3] + 0x67u) = 1;                        /* 0x197E8 */
        fighter_think_side(ctx[1]);                     /* 0x197EF 0x3B464 */
        /* PORT: 0x197F8 0x3B938(ctx[2]); 0x197FF 0x39278 — named gaps. */
    }
}

/* 0x3BDDC. The attack/command consumer. When the slot's +0x40 bit 7 is clear
 * and the side's command word has bit 15 set, it clears the record's
 * +0x34/+0x43/+0x42, sets the slot's +0x5F to 0xFF, and (unless 0x3CF38
 * returns non-zero) writes the attack state: the 0xBEF28/0xBEF64 table at
 * DS_00107D40 + side*4, the slot's +0x52/+0x53/+0x54, DS_001078F8+side and the
 * slot's +0x4E from the command's 0x1000/0x2000 bits. Returns 1 on the attack
 * transition, 0 when either gate rejects. */
int fighter_attack_consume(u32 side)
{
    u32 ctx[6];
    fighter_ctx_same(ctx, side);                        /* 0x3BDEA */
    u32 self = ctx[2];                                  /* &slot[side] */
    if ((DSB(self + 0x40u) & 0x80u) != 0) return 0;     /* 0x3BDF3 */
    u16 cmd = DSW(DS_001088E0 + side * 2u);             /* 0x3BDFD */
    if ((cmd & 0x8000u) == 0) return 0;                 /* 0x3BE09 */

    u32 rec = DSD(DS_001077B0 + side * 0x94u);          /* 0x3BE28 */
    DSW(rec + 0x34u) = 0;                               /* 0x3BE2F */
    DSB(rec + 0x43u) = 0;                               /* 0x3BE35 */
    DSB(rec + 0x42u) = 0;                               /* 0x3BE39 */
    DSB(self + 0x5Fu) = 0xFFu;                          /* 0x3BE43 */

    if (DSB(DS_00107803 + side * 0x94u) == 0) {
        /* PORT: 0x3BE61 0x3CF38(side) — named gap (§7.6); the raw returns 1
         * before the continuation when it is non-zero. */
    }

    u32 base = fighter_input_scan(side, 0, 5, 0x4000u) ? 0xBEF64u
                                                       : 0xBEF28u;  /* 0x3BE7F */
    u32 ch = DSB(self + 0x7Au);                         /* 0x3BEF4 */
    DSD(DS_00107D40 + side * 4u) = base + ch * 6u;      /* 0x3BED4 */
    /* PORT: 0x3BF00 0x3C480(rec, DS_000C8B30[ch], 0x3F800000) — gap (§7.16). */

    DSB(self + 0x52u) = 3u;                             /* 0x3BF0A */
    DSB(self + 0x54u) = 2u;                             /* 0x3BF10 */
    DSB(self + 0x53u) = 4u;                             /* 0x3BF16 */
    DSB(DS_001078F8 + side) = 1u;                       /* 0x3BF1C */
    if ((cmd & 0x1000u) != 0) {                         /* 0x3BF22 */
        DSW(DS_001077FE + side * 0x94u) = 1u;           /* 0x3BF2C */
    } else if ((cmd & 0x2000u) != 0) {                  /* 0x3BF34 */
        DSW(DS_001077FE + side * 0x94u) = 0xFFFFu;      /* 0x3BF40 */
    } else {
        DSW(DS_001077FE + side * 0x94u) = 0;            /* 0x3BF51 */
    }
    return 1;                                           /* 0x3BF57 */
}
