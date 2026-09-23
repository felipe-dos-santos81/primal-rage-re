/* Port of the two per-frame fighter passes the arena frame calls around its
 * think step, plus the pure per-fighter helpers the HUD spine shares, plus the
 * think/AI chain 0x1975C -> 0x3B464 -> 0x3B298 -> 0x3B134 -> 0x3BDDC.
 * Addresses, gates, widths and the RNG sites are from
 * docs/superpowers/plans/2026-09-20-demo-fight-derivations.md §3.5, §5 and §8.
 * The module registers nothing; the think chain's command consumer lives here
 * and is called back from fight.c's 0x3B134. */
#include "game/fighter.h"
#include "game/fight.h"
#include "game/actors.h"
#include "game/rng.h"
#include "../mem.h"
#include "../symbols.h"
#include <string.h>

/* PORT: data-object addresses with no symbols.h name (the generator emits none
 * for these tables). */
#define FIGHTER_SPAWN_X   0x000BDA38u   /* 0x33EC4: per-side initial x dword */
#define FIGHTER_DESC_A    0x000BB7E0u   /* 0x33CC8: [char*2 + side] fighter */
#define FIGHTER_DESC_B    0x000BB8D0u   /* 0x33D38: [char] secondary actor */

/* 0x29BC8. The character-palette acquire the spawn (0x33E1E) and the +0x52
 * handler block (0x33B00/0x36E78) share; defined with that block. */
static void fighter_29bc8(u32 side, u32 rec, u32 ch);

/* 0x39A10/0x37D18/0x36870/0x37178/0x379C4. The 0x349C8 +0x42 bit-6/7 arms'
 * chains; defined together with the other 0x36xxx/0x37xxx handlers below.
 * 0x37178 -> 0x36870 -> 0x379C4 -> 0x37178 is mutually recursive. */
void fighter_39a10(u32 rec, u32 value);                  /* 0x39A10 */
void fighter_37d18(u32 slot, u32 rec);                   /* 0x37D18 */
void fighter_36870(u32 rec);                             /* 0x36870 */
void fighter_37178(u32 slot);                            /* 0x37178 */
void fighter_385b0(u32 rec);                             /* 0x385B0 */
static void fighter_379c4(u32 slot);                     /* 0x379C4 */
static void fighter_164e8(u32 side);                     /* 0x164E8 */

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

/* 0x18540. The per-side screen anchor DS_00100AF0[side]: the slot's actor
 * sprite id (its low 15 bits) minus the character's camera-x constant
 * (0xE39D0/0xECBD8/0xD2134/0xEA604/0xD3E08/0xE061C, table 0x18524; the >6
 * default 0xE6DD0), clamped to 0 when negative or at/over 0xE6DB4[char]. The
 * raw's 0x18460 call is effect-free in this build: its 0x18428 dispatch's
 * 0x1840C table resolves to 0x18408 (0x18350's epilogue), so it writes
 * nothing (the port's fixup-applied mem[] holds the seven 0x18408 entries). */
static void fighter_18540(u32 side)
{
    u32 slot = DSD(DS_001077A8 + side * 4u);            /* 0x18546 */
    if (slot == 0) return;                              /* 0x1854F */
    u32 ch = (u32)DSB(slot + 0x7Au);                    /* 0x18555 */
    u32 cam;
    switch (ch) {                                       /* 0x18561 table 0x18524 */
    case 1u: cam = (u32)DSW(0x000E39D0u); break;
    case 2u: cam = (u32)DSW(0x000ECBD8u); break;
    case 3u: cam = (u32)DSW(0x000D2134u); break;
    case 4u: cam = (u32)DSW(0x000EA604u); break;
    case 5u: cam = (u32)DSW(0x000D3E08u); break;
    case 6u: cam = (u32)DSW(0x000E061Cu); break;
    default: cam = (u32)DSW(0x000E6DD0u); break;        /* char 0 and >6 */
    }
    u32 sprite = (u32)DSW(DSD(DS_001014EC)
        + (u32)DSW(DSD(slot) + 0x56u) * 0x20u) & 0x7FFFu;   /* 0x185BB */
    s32 anchor = (s32)sprite - (s32)cam;                /* 0x185DB */
    DSD(0x00100AF0u + side * 4u) = (u32)anchor;         /* 0x185E4 */
    if (anchor < 0 || (u32)anchor >= DSD(0x000E6DB4u + ch * 4u))
        DSD(0x00100AF0u + side * 4u) = 0;               /* 0x18617 */
}

/* 0x18350. The per-side screen offset DS_00100AB0/AB4[side*8]: the signed byte
 * pair at the character's anchor-indexed table (0xCEB00/0xCF399/0xCFC32/0xD033B/
 * 0xD0A44, table 0x18334), the x negated when the actor's bit 15 is set, both
 * scaled by 64. */
static void fighter_18350(u32 side, u32 anchor)
{
    u32 ch = (u32)DSB(DS_001077B0 + side * 0x94u + 0x7Au);   /* 0x1835F */
    const u8 *p;
    switch (ch) {                                       /* 0x18384 table 0x18334 */
    case 0u: p = mem + 0x000CEB00u + anchor * 2u; break;
    case 1u: p = mem + 0x000CF399u + anchor * 2u; break;
    case 2u: p = mem + 0x000CFC32u + anchor * 2u; break;
    case 3u: p = mem + 0x000D033Bu + anchor * 2u; break;
    case 4u: p = mem + 0x000D0A44u + anchor * 2u; break;
    case 5u: p = mem + 0x000CEB00u + anchor * 2u; break;
    case 6u: p = mem + 0x000CF399u + anchor * 2u; break;
    default: p = mem + 0x000CEB00u + anchor * 2u; break;   /* char >6 (0x1838B) */
    }
    DSD(0x00100AB0u + side * 8u) = (u32)(s32)(s8)p[0];   /* 0x183BA */
    DSD(0x00100AB4u + side * 8u) = (u32)(s32)(s8)p[1];   /* 0x183C4 */
    if (fighter_actor_bit15_clear(side) == 0)            /* 0x183CC */
        DSD(0x00100AB0u + side * 8u) = (u32)(-(s32)DSD(0x00100AB0u + side * 8u));
    DSD(0x00100AB0u + side * 8u) <<= 6;                  /* 0x183F1 */
    DSD(0x00100AB4u + side * 8u) <<= 6;                  /* 0x183F4 */
}

/* 0x186D0. The slot position latch (the game_frame tail's 0x25438 call). */
void fighter_slot_latch(u32 side)
{
    u32 slot = DS_001077B0 + side * 0x94u;
    u32 rec = DSD(slot);                            /* 0x186F3/0x18702 */
    if ((DSB(slot + 0x42u) & 0x08u) == 0) {         /* 0x186E6 */
        fighter_18540(side);                        /* 0x18627 */
        u32 anchor = DSD(DS_00100AF0 + side * 4u);
        if (anchor != DSD(slot + 0x20u)) {
            DSD(slot + 0x20u) = anchor;             /* 0x18645 */
            fighter_18350(side, anchor);            /* 0x1864D */
        }
        DSD(slot + 0x2Cu) = DSD(rec + 0x18u) + DSD(DS_00100AB0 + side * 8u);
        DSD(slot + 0x30u) = DSD(rec + 0x1Cu) + DSD(DS_00100AB4 + side * 8u);
    } else {
        DSD(slot + 0x2Cu) = DSD(rec + 0x18u);       /* 0x186FC */
        DSD(slot + 0x30u) = DSD(rec + 0x1Cu);       /* 0x1860B */
    }
    if ((DSB(slot + 0x41u) & 0x80u) != 0)           /* 0x186A5 */
        DSD(slot + 0x34u) = DSD(slot + 0x2Cu);      /* 0x186B4 */
}

/* 0x1CEBC. The audio gate the spawn tail tests: 1 when the AIL sequence handle
 * DS_001028C8 is live (non-zero) and its busy byte DS_001028DB is clear. The
 * port keeps its AIL handles outside mem[], so this is 0 and the tail's
 * res_resolve calls (named gap, §10.4) are skipped. */
static int fighter_spawn_audio_gate(void)
{
    if (DSD(DS_001028C8) == 0) return 0;            /* 0x1CEC3 */
    if (DSB(DS_001028DB) != 0) return 0;            /* 0x1CECC */
    return 1;                                       /* 0x1CECE */
}

/* 0x33C78. The spawn core. EAX = side, EDX = the initial x (the dword at
 * DS_000BDA38 + side*2 shifted right 16), ECX = word[0xBD898] = 0x400 (the
 * layer), and the stack argument = 0x4000 for side 0 / 0 for side 1. The three
 * stores that make the slot live are DS_001077A8[side] (0x33CA0), slot+0x7A
 * (0x33CB8) and slot+0x00 (0x33CD8). */
static void fighter_spawn_slot(u32 side, u32 a2, u32 a3, u32 a5)
{
    u32 slot = DS_001077B0 + side * 0x94u;          /* 0x33C84..0x33C95 */
    DSD(DS_001077A8 + side * 4u) = slot;            /* 0x33CA0 */
    u32 ch = (u32)DSB(DS_0010816A + side);          /* 0x33CB2 */
    DSB(slot + 0x7Au) = (u8)ch;                     /* 0x33CB8 */

    /* 0x33CC8/0x33CD3: the fighter record from descriptor [char*2 + side]. */
    u32 desc = DSD(FIGHTER_DESC_A + (ch * 2u + side) * 4u);
    u32 rec = actor_spawn((const u32 *)(mem + desc), a2, a3, 0u, a5);
    DSD(slot) = rec;                                /* 0x33CD8 */

    u8 n = (u8)(DSB(DS_001078FA) + 1u);             /* 0x33CDA..0x33CEA */
    DSB(slot + 0x52u) = 0;                          /* 0x33CE0 */
    DSB(slot + 0x53u) = 0;                          /* 0x33CE6 */
    DSB(DS_001078FA) = n;
    DSB(slot + 0x5Fu) = 0xFFu;                      /* 0x33CF2 */
    DSB(slot + 0x64u) = 0xFFu;                      /* 0x33CFB */
    if (n == 2u) {                                  /* 0x33CFF..0x33D0B */
        DSB(DS_000F0AFE) = 1;                       /* 0x33D04 */
    } else {
        DSB(DS_000F0AFE) = 0;                       /* 0x33D13 (DL=0) */
        DSB(DS_000F0AFF) = (u8)side;                /* 0x33D19 */
    }

    /* 0x33D1E..0x33D46: the secondary actor, parented to this record
     * (a5 = actor index | 0x400 selects 0x2AE14's child arm). */
    u32 idx = (u32)DSW(rec + 0x56u) | 0x400u;       /* 0x33D20..0x33D27 */
    u32 desc2 = DSD(FIGHTER_DESC_B + ch * 4u);
    u32 rec2 = actor_spawn((const u32 *)(mem + desc2), 0u, 0u, 0u, idx);
    DSD(slot + 0x04u) = rec2;                       /* 0x33D46 */
    DSB(rec2 + 0x59u) = DSB(DS_000BDB38);           /* 0x33D4E */

    DSD(slot + 0x24u) = DSD(DS_000BDA8C + ch * 4u); /* 0x33D5D */
    DSD(rec + 0x14u) = slot;                        /* 0x33D62 */
    DSB(rec + 0x51u) = (u8)side;                    /* 0x33D6B */
    DSB(rec + 0x4Cu) = 0;                           /* 0x33D70 */
    DSB(rec + 0x4Du) = 0x1Eu;                       /* 0x33D7D */

    /* 0x33D86..0x33DDA: slot+0x81 = DS_001088CC + min(slot+0x3C / 0xC350,
     * DS_001088CB); the divide is unsigned (`div`). */
    {
        u32 q = DSD(slot + 0x3Cu) / DSD(DS_000C9520);
        u32 cap = (u32)DSB(DS_001088CB);
        if (q >= cap) q = cap;                      /* 0x33DCC..0x33DD0 */
        DSB(slot + 0x81u) = (u8)(DSB(DS_001088CC) + q);
    }

    DSB(slot + 0x55u) = 0xFFu;                      /* 0x33D8D */
    DSD(slot + 0x40u) = 0x80008000u;                /* 0x33D91 */
    DSB(slot + 0x54u) = 0;                          /* 0x33D98 */
    DSB(slot + 0x5Du) = 0;                          /* 0x33D9C */
    DSD(slot + 0x08u) = 0;                          /* 0x33DA0 */
    DSW(slot + 0x6Au) = 0;                          /* 0x33DA7 */
    DSW(slot + 0x6Cu) = 0;                          /* 0x33DAD */
    DSB(slot + 0x7Bu) = 0;                          /* 0x33DB3 */
    DSB(slot + 0x7Cu) = 0;                          /* 0x33DB9 */
    DSW(slot + 0x88u) = 0;                          /* 0x33DC3 */
    DSD(slot + 0x20u) = 0xFFFFFFFFu;                /* 0x33DE4 */

    fighter_slot_latch(side);                       /* 0x33DEB 0x186D0 */

    DSD(slot + 0x0Cu) = 0;                          /* 0x33DF0 */
    DSD(slot + 0x10u) = 0;                          /* 0x33DF7 */
    DSD(slot + 0x14u) = 0;                          /* 0x33DFE */
    DSD(slot + 0x18u) = 0;                          /* 0x33E07 */
    DSD(slot + 0x1Cu) = 0;                          /* 0x33E17 */

    /* 0x33E1E 0x29BC8(side, rec, ch): the character palette at pset+0x18. */
    fighter_29bc8(side, rec, ch);

    DSB(slot + 0x41u) &= 0x7Fu;                     /* 0x33E23..0x33E2F */
    DSD(DS_001077A0 + side * 4u) = 0;               /* 0x33E38 */

    if (DSB(DS_00104B14) == 0)
        fight_dust_build(side);                     /* 0x33E43 0x494A8 */
    if (fighter_spawn_audio_gate()) {               /* 0x33E48 0x1CEBC */
        /* PORT: 0x33E51..0x33EA6 resolves DS_000BDB1C[char] then the fixed
         * 0x287B2F5 through res_resolve; both are unported audio resources and
         * neither return is read, so the tail is a named gap (§10.4). */
    }
}

void fighter_spawn(u32 side)
{
    /* 0x33EB4: A5 = 0x4000/0 by side; a3 = word[0xBD898] = 0x400; the initial x
     * is the dword at DS_000BDA38 + side*2 shifted right 16 (0x33EC4/0x33ECB). */
    u32 a3 = (u32)DSW(DS_000BD898);
    u32 a5 = (side == 0u) ? 0x4000u : 0u;
    u32 a2 = (u32)((s32)DSD(FIGHTER_SPAWN_X + side * 2u) >> 16);
    fighter_spawn_slot(side, a2, a3, a5);
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
                (void)hit_chain_resolve(side);      /* 0x190E7 0x3CF38 */
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

/* PORT: 0x1975C is unexercised by the attract demo. Every writer of slot+0x64
 * in the image sets 0xFF (0x33CFB spawn, 0x33B00 case 19 only, 0x3B6B8/0x3B985/
 * 0x3B9D2), and the only non-0xFF writer 0x2A620 writes an actor record, not the
 * slot, so 0x3B464 returns at 0x3B49F for both demo fighters and 0x3B134
 * (fight_command_map) is unreachable from the demo. This chain is owned by the
 * interactive match; its unit tests are its only evidence
 * (docs/superpowers/plans/2026-09-21-demo-fight-closure-derivations.md §8). */
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

/* ---- 0x47208 the demo/CPU-AI command generator -------------------------
 *
 * The attract demo's command words come from here, not from the think chain:
 * game_frame (0x24C73) runs 0x47208 per side when DS_00104B26 == 0 and
 * DS_00104B19+2 != 0, which is the whole demo. 0x47208 walks the per-side AI
 * state block at DS_001081F0 + side*0x40: it classifies the fighter's state
 * (0x469A8), manages the move selection (0x470F8 -> 0x46F4C, which draws
 * rng(0x64) = 0x5D7DC), and maps the selected move step's input word through
 * 0x3C6E8 into DS_001088E0/E2. Addresses, widths and control flow are from
 * docs/superpowers/plans/2026-09-20-demo-fight-derivations.md §11.
 *
 * The per-side AI block (0x40 bytes, base DS_001081F0 + side*0x40):
 *   +0x00 frame counter      +0x04 active flag    +0x08 step index
 *   +0x0C frames-on-move     +0x10 last state     +0x14 band-table ptr
 *   +0x18 move-id table ptr  +0x1C move-step ptr  +0x20 weight table ptr
 *   +0x24 prev state         +0x28 state          +0x2C weight id
 *   +0x2D band id            +0x2E move id        +0x2F weight sub-index
 *   +0x30 cursor             +0x31 step timer     +0x34/+0x38 aux ptrs
 *   +0x3C aux dword
 */

#define AI_BASE 0x001081F0u

/* 0x1081F0 + side*0x40. */
static u32 ai_b(u32 side)
{
    return AI_BASE + side * 0x40u;
}

/* 0x187FC. The two slots latched, then slot0+0x2C - slot1+0x2C. 0x46DD4 calls
 * it once for the sign and once for the value, so the port keeps both calls. */
static s32 ai_distance(void)
{
    fighter_slot_latch(0u);                             /* 0x18800 */
    fighter_slot_latch(1u);                             /* 0x18808 */
    return (s32)DSD(0x001077DCu) - (s32)DSD(0x00107870u);   /* 0x1880D */
}

/* 0x3C6E8. Map a move step's input bitfield to the command word: the four
 * 0x10000/0x20000/0x40000/0x80000 gates OR 0x10/0x20/0x1000/0x2000 in the
 * order `facing` selects (the raw's EAX/EBX/ECX/EDI/ESI). */
static u16 fighter_cmd_facing(u32 facing, u32 input)
{
    u32 eax = input;
    u32 a, b, c, d;
    if (facing != 0u) { a = 0x20u; b = 0x2000u; c = 0x10u; d = 0x1000u; }
    else              { a = 0x10u; b = 0x1000u; c = 0x20u; d = 0x2000u; }
    if ((input & 0x10000u) != 0u) eax |= a;             /* 0x3C726 */
    if ((input & 0x40000u) != 0u) eax |= c;             /* 0x3C730 */
    if ((input & 0x20000u) != 0u) eax |= b;             /* 0x3C73A */
    if ((input & 0x80000u) != 0u) eax |= d;             /* 0x3C744 */
    return (u16)eax;                                    /* 0x3C748 */
}

/* 0x46F4C's per-character tables: the move-step pointer table (EBP) and the
 * aux table ([ESP]). Both are data-object pointer arrays read from mem[]. */
static void ai_move_tables(u32 ch, u32 *step, u32 *aux)
{
    switch (ch & 0xFFu) {
    case 0u: *step = 0x0008C0BCu; *aux = 0x0008A4A0u; break;
    case 1u: *step = 0x00083C68u; *aux = 0x00081E24u; break;
    case 2u: *step = 0x00091D0Cu; *aux = 0x00090154u; break;
    case 3u: *step = 0x000949E4u; *aux = 0x00092F9Cu; break;
    case 4u: *step = 0x0008EF0Cu; *aux = 0x0008D2FCu; break;
    case 5u: *step = 0x000892ACu; *aux = 0x00087B1Cu; break;
    default: *step = 0x000868D4u; *aux = 0x00084F78u; break;  /* char 6 */
    }
}

/* 0x46DD4's per-character tables: the move-id byte table (EBP) and the aux
 * pointer table (EDI). */
static void ai_band_tables(u32 ch, u32 *ids, u32 *aux)
{
    switch (ch & 0xFFu) {
    case 0u: *ids = 0x0008C238u; *aux = 0x0008A4A4u; break;
    case 1u: *ids = 0x00083E70u; *aux = 0x00081E28u; break;
    case 2u: *ids = 0x00091F0Cu; *aux = 0x00090158u; break;
    case 3u: *ids = 0x00094B8Cu; *aux = 0x00092FA0u; break;
    case 4u: *ids = 0x0008F0E4u; *aux = 0x0008D300u; break;
    case 5u: *ids = 0x000893E0u; *aux = 0x00087B20u; break;
    default: *ids = 0x00086A34u; *aux = 0x00084F7Cu; break;  /* char 6 */
    }
}

/* 0x46BBC. Store the weight id (+0x2C) and select this side's band table
 * (+0x14) and aux table (+0x34) from the char-pair index. */
static void ai_weight(u32 side, u32 id)
{
    u32 b = ai_b(side);
    u32 idx = (u32)DSB(DS_001077B0 + side * 0x94u + 0x7Au) * 10u
            + (u32)DSB(DS_001077B0 + (side ^ 1u) * 0x94u + 0x7Au);
    u32 p1 = DSD(0x00095CA8u + idx * 4u);               /* 0x46BD3 */
    u32 p2 = DSD(0x00095E38u + idx * 4u);
    DSB(b + 0x2Cu) = (u8)id;                            /* 0x46BE8 */
    DSD(b + 0x14u) = DSD(p1 + id * 4u);                 /* 0x46C05 */
    DSD(b + 0x34u) = DSD(p2 + id * 4u);                 /* 0x46C15 */
}

/* 0x46C78. The state -> weight-id switch over the *other* slot's +0x5F/+0x8A/
 * +0x64 (0x33950's ctx[3] = &slot[1-side]), then +0x10 = state. */
static void ai_state_setup(u32 side)
{
    u32 b = ai_b(side);
    u32 state = DSD(b + 0x28u);                         /* 0x46C94 */
    u32 other = DS_001077B0 + (side ^ 1u) * 0x94u;
    switch (state) {
    case 0u:  ai_weight(side, (u32)DSB(other + 0x5Fu)); break;          /* 0x46CB0 */
    case 1u:                                                            /* 0x46CBE */
        if (DSB(other + 0x8Au) == 0u)
            ai_weight(side, (u32)DSB(other + 0x5Fu) + 0x40u);
        else
            ai_weight(side, 0x88u);
        break;
    case 8u:  ai_weight(side, 0x88u); break;
    case 2u:  ai_weight(side, 0x82u); break;
    case 3u:  ai_weight(side, 0x85u); break;
    case 4u:  ai_weight(side, 0x87u); break;
    case 5u:  ai_weight(side, 0x83u); break;
    case 6u:  ai_weight(side, 0x84u); break;
    case 7u:  ai_weight(side, 0x81u); break;
    case 9u:  ai_weight(side, 0x86u); break;
    case 0xBu:                                                          /* 0x46D59 */
        if (DSB(other + 0x64u) != 0xFFu)
            ai_weight(side, (u32)DSB(other + 0x64u));
        else
            ai_weight(side, 0x88u);
        break;
    case 0xCu: ai_weight(side, 0x89u); break;
    case 0xDu: ai_weight(side, 0x8Au); break;
    default: break;   /* 0x62003 out of scope; state is 0..0xD by construction */
    }
    DSD(b + 0x10u) = state;                             /* 0x46DA4 */
}

/* 0x46DD4. Pick the range band from the two-fighter distance (>>6, clamped to
 * 0xFF) and select the move-id table (+0x18) and aux table (+0x38). */
static void ai_band(u32 side)
{
    u32 b = ai_b(side);
    u32 ids, aux;
    ai_band_tables(DSB(DS_001077B0 + side * 0x94u + 0x7Au), &ids, &aux);
    {
        s32 sign = ai_distance();                       /* 0x46E58 */
        s32 v = ai_distance();
        s32 mag = (sign < 0) ? -v : v;
        u32 table, entry, i = 0;
        mag = mag >> 6;                                 /* 0x46E79..0x46E83 */
        table = DSD(b + 0x14u);                         /* 0x46E8B */
        entry = table;
        for (;;) {
            /* PORT: 0x46E96. The raw re-clamps d to 0xFF inside the loop
             * (0x46E94 starts EBX=0, 0x46E9C `jle`); it is constant, kept
             * verbatim. */
            u32 m = ((u32)mag > 0xFFu) ? 0xFFu : (u32)mag;
            if ((u32)DSB(entry + 2u) <= m && m <= (u32)DSB(entry + 3u)) break;
            i++;
            entry += 4u;
            if (i >= 0x14u) { entry = table; break; }   /* 0x46EC5 fallthrough */
        }
        DSB(b + 0x2Du) = DSB(entry);                    /* 0x46EB7/0x46ECA */
        DSB(b + 0x2Fu) = DSB(entry + 1u);               /* 0x46ED0 */
    }
    {
        u32 id = (u32)DSB(b + 0x2Du);
        DSD(b + 0x18u) = DSD(ids + id * 4u);            /* 0x46EDB */
        DSD(b + 0x38u) = DSD(aux + id * 4u);            /* 0x46EE7 */
    }
}

/* 0x46F4C. Commit the move: clamp the difficulty DS_001082C8 to 0..7, build the
 * weight-table pointer from the band sub-index, draw rng(0x64), pick the first
 * weighted index whose cumulative weight reaches the draw (index 9 is the
 * fallback), then load the move id and its step/aux pointers. */
static void ai_pick(u32 side)
{
    u32 b = ai_b(side);
    u32 step, aux;
    ai_move_tables(DSB(DS_001077B0 + side * 0x94u + 0x7Au), &step, &aux);
    ai_state_setup(side);                               /* 0x46FE3 */
    ai_band(side);                                      /* 0x46FEC */
    {
        s32 diff = (s32)DSD(DS_001082C8 + side * 4u);   /* 0x46FFB */
        if (diff < 0) DSD(DS_001082C8 + side * 4u) = 0;
        else if (diff >= 8) DSD(DS_001082C8 + side * 4u) = 7;
        diff = (s32)DSD(DS_001082C8 + side * 4u);
        DSD(b + 0x20u) = 0x00095FC8u + (u32)DSB(b + 0x2Fu) * 0x50u
                       + (u32)diff * 0x0Au;             /* 0x47058 */
    }
    {
        s32 r = (s32)rng_next(0x64u);                   /* 0x47063 */
        u32 i = 0;
        s32 cum = 0;
        for (;;) {
            s8 wt;
            if (i == 9u) break;                         /* 0x47073 */
            wt = (s8)DSB(DSD(b + 0x20u) + i);           /* 0x470a4 */
            if (wt != 0) {
                cum += (s32)wt;
                if (r <= cum) break;                    /* 0x470af */
            }
            i++;
            /* PORT: 0x470E8. The raw's `cmp eax,0xa; jl` loop tail is
             * unreachable: i == 9 breaks at 0x47073 first, so i never reaches
             * 10. Kept verbatim. */
            if (i >= 0xAu) return;
        }
        DSD(b + 0x08u) = i;                             /* 0x47076/0x470b6 */
        {
            u8 id = DSB(DSD(b + 0x18u) + i);            /* 0x470b9 */
            DSB(b + 0x2Eu) = id;
            DSD(b + 0x1Cu) = DSD(step + (u32)id * 4u);  /* 0x470c8 */
            DSD(b + 0x3Cu) = DSD(aux + (u32)id * 4u);   /* 0x470da */
        }
    }
}

/* 0x1A5D4. 0x2000 when the other side's facing bit 0x4000 is clear and the
 * command's 0x2000 is set; 0x1000 in the mirror; else 0. */
static u32 ai_facing_cmd(u32 side)
{
    u32 rec = DSD(DS_001077B0 + side * 0x94u);          /* 0x1A5E0 */
    u16 cmd = DSW(DS_001088E0 + side * 2u);
    if ((DSW(rec + 0x28u) >> 8 & 0x40u) == 0u) {
        if ((cmd >> 8 & 0x20u) != 0u) return 0x2000u;
    } else if ((cmd >> 8 & 0x10u) != 0u) {
        return 0x1000u;
    }
    return 0u;
}

/* 0x46958. slot+0x53 in {7,8} and slot+0x5F < 0x40. */
static int ai_ready(u32 side)
{
    u32 slot = DS_001077B0 + side * 0x94u;
    u8 st = DSB(slot + 0x53u);
    if (st == 8u || st == 7u) {
        if ((u32)DSB(slot + 0x5Fu) < 0x40u) return 1;
    }
    return 0;
}

/* 0x46698. slot+0x40 bit 7 set, then the +0x2C ordering and rec_self+0x34. */
static int ai_pred_46698(u32 side)
{
    u32 ctx[6];
    fighter_ctx_same(ctx, side);                        /* 0x466A0 */
    if ((DSB(ctx[2] + 0x40u) & 0x80u) == 0u) return 0;
    if ((s32)DSD(ctx[3] + 0x2Cu) > (s32)DSD(ctx[2] + 0x2Cu))
        return (s16)DSW(ctx[4] + 0x34u) >= 0;
    return (s16)DSW(ctx[4] + 0x34u) <= 0;
}

/* 0x466F4. Idle: +0x5F == 0xFF, +0x54 == 0, +0x53 == 0, +0x52 == 0. */
static int ai_pred_466f4(u32 side)
{
    u32 slot = DS_001077B0 + side * 0x94u;
    if (DSB(slot + 0x5Fu) != 0xFFu) return 0;
    if (DSB(slot + 0x54u) != 0u) return 0;
    if (DSB(slot + 0x53u) != 0u) return 0;
    return DSB(slot + 0x52u) == 0u;
}

/* 0x4673C. +0x54 == 1 and +0x52 in {5,0x15}. */
static int ai_pred_4673c(u32 side)
{
    u32 slot = DS_001077B0 + side * 0x94u;
    if (DSB(slot + 0x5Fu) != 0xFFu) return 0;
    if (DSB(slot + 0x54u) != 1u) return 0;
    if (DSB(slot + 0x53u) != 0u) return 0;
    return DSB(slot + 0x52u) == 5u || DSB(slot + 0x52u) == 0x15u;
}

/* 0x46794. +0x54 == 2 and +0x52 == 4. */
static int ai_pred_46794(u32 side)
{
    u32 slot = DS_001077B0 + side * 0x94u;
    if (DSB(slot + 0x5Fu) != 0xFFu) return 0;
    if (DSB(slot + 0x54u) != 2u) return 0;
    if (DSB(slot + 0x53u) != 0u) return 0;
    return DSB(slot + 0x52u) == 4u;
}

/* 0x467DC / 0x4682C. +0x54 == 0, +0x53 == 0, +0x52 == 1, then the 0x1A5D4
 * command-sign test (0x467DC: zero, 0x4682C: non-zero). */
static int ai_pred_cmd_sign(u32 side, int want_zero)
{
    u32 slot = DS_001077B0 + side * 0x94u;
    if (DSB(slot + 0x54u) != 0u) return 0;
    if (DSB(slot + 0x53u) != 0u) return 0;
    if (DSB(slot + 0x52u) != 1u) return 0;
    return (ai_facing_cmd(side) == 0u) == (want_zero != 0);
}

/* 0x46898 / 0x468D8. The +0x10 identity is read from ctx_swap's [ESP+0xc]
 * (= ctx[3] = &slot[side], the slot's 0x22BEC code pointer), the +0x24 dword
 * from [ESP+0x14] (= ctx[5] = rec_self); 0x46898 wants slot+0x54 == 2, 0x468D8
 * wants slot+0x54 != 2 or slot+0x52 == 7. */
static int ai_pred_46898(u32 side)
{
    u32 ctx[6];
    fighter_ctx_swap(ctx, side);                        /* 0x468A0 */
    if (DSD(ctx[3] + 0x10u) != 0x22BECu) return 0;
    if ((DSD(ctx[5] + 0x24u) & 0x7FFFFFFFu) != 0u) return 0;
    return DSB(ctx[3] + 0x54u) == 2u;
}

static int ai_pred_468d8(u32 side)
{
    u32 ctx[6];
    fighter_ctx_swap(ctx, side);                        /* 0x468E3 */
    if (DSD(ctx[3] + 0x10u) == 0x22BECu
            && (DSD(ctx[5] + 0x24u) & 0x7FFFFFFFu) == 0u
            && DSB(ctx[3] + 0x54u) != 2u)
        return 1;
    return DSB(ctx[3] + 0x52u) == 7u;
}

/* 0x469A8. Classify `param`'s slot state into the AI state of block[1-param],
 * then store it. The raw reads slot[param] (EBX) and writes block[1-param]
 * (ECX = 1-param); the port keeps that transpose. */
static void ai_classify(u32 param)
{
    u32 ecx = 1u - param;
    if (fighter_frame_flag(0u, param) == 0) {           /* 0x469BB */
        DSD(0x00108214u + ecx * 0x40u) = DSD(0x00108218u + ecx * 0x40u);
    }
    if (ai_ready(param) != 0) {                         /* 0x469D7 */
        u32 prev = DSD(0x00108214u + ecx * 0x40u);
        u32 v = (prev == 0u || prev == 1u) ? 1u : 0u;
        if (v != 0u) { DSD(0x00108218u + ecx * 0x40u) = 1u; return; }
        DSD(0x00108218u + ecx * 0x40u) =
            (ai_pred_46698(param) != 0) ? 0x0Du : 0u;
        return;
    }
    if (ai_pred_466f4(param) != 0) {                    /* 0x46A20 */
        DSD(0x00108218u + ecx * 0x40u) = 2u; return;
    }
    if (ai_pred_4673c(param) != 0) { DSD(0x00108218u + ecx * 0x40u) = 3u; return; }
    if (ai_pred_46794(param) != 0) { DSD(0x00108218u + ecx * 0x40u) = 4u; return; }
    if (ai_pred_cmd_sign(param, 1) != 0) { DSD(0x00108218u + ecx * 0x40u) = 6u; return; }
    if (ai_pred_cmd_sign(param, 0) != 0) { DSD(0x00108218u + ecx * 0x40u) = 5u; return; }
    if (ai_pred_468d8(param) != 0) { DSD(0x00108218u + ecx * 0x40u) = 7u; return; }
    if (DSB(DS_001077B0 + param * 0x94u + 0x53u) == 0x0Bu) {   /* 0x46AE4 */
        DSD(0x00108218u + ecx * 0x40u) = 9u; return;
    }
    if (DSB(DS_001077B0 + param * 0x94u + 0x53u) == 6u) {      /* 0x46B12 */
        DSD(0x00108218u + ecx * 0x40u) = 0x0Bu; return;
    }
    if (ai_pred_46898(param) != 0) { DSD(0x00108218u + ecx * 0x40u) = 0xCu; return; }
    DSD(0x00108218u + ecx * 0x40u) = 8u;                /* 0x46B5D */
}

/* 0x470F8. Reset the active flag when the state transitioned, then, when the
 * fighter is idle (slot+0x53 == 0) and no move is active, pick a new move. */
static void ai_manage(u32 side)
{
    u32 b = ai_b(side);
    u32 slot = DS_001077B0 + side * 0x94u;
    u32 state = DSD(b + 0x28u);
    if (state == 0u && DSB(slot + 0x54u) != 2u) {
        DSD(b + 0x04u) = 0;                             /* 0x47126 */
    } else if (state == 7u && DSD(b + 0x10u) != 7u && DSB(slot + 0x54u) != 2u) {
        DSD(b + 0x04u) = 0;                             /* 0x47163 */
    } else if (state == 9u && DSD(b + 0x10u) != 9u && DSB(slot + 0x54u) != 2u) {
        DSD(b + 0x04u) = 0;                             /* 0x4719D */
    }
    if (DSB(slot + 0x53u) != 0u) return;                /* 0x471B1 */
    if (DSD(b + 0x04u) != 0u) return;                   /* 0x471C6 */
    ai_pick(side);                                      /* 0x471CA */
    DSD(b + 0x0Cu) = 0;                                 /* 0x471DA */
    DSD(b + 0x04u) = 1;                                 /* 0x471E2 */
    DSD(b + 0x00u) = DSD(b + 0x00u) + 1u;               /* 0x471F2 */
    DSB(b + 0x30u) = 0;                                 /* 0x471EA */
    DSB(b + 0x31u) = 0;                                 /* 0x471F8 */
}

/* 0x47208. One side's command word for this frame. The 0x2D974/0x2CAA8 block
 * at 0x47241 (a DIP bit 0x800 plus 0x2CAA8 = config_not_free_play) is inert in
 * the demo: DS_00105D60 (FREE PLAY) is 0, so 0x2CAA8 returns 1 and
 * `(dip & 0x800) && 0x2CAA8() == 0` is false at 0x47237/0x4723B. It is a named
 * gap because it can run under free play. */
void fighter_command_generate(u32 side)
{
    u32 ctx[6];
    fighter_ctx_same(ctx, side);                        /* 0x47216 */
    ai_classify(ctx[1]);                                /* 0x47291 */
    {
        u32 slot = ctx[2];
        u32 b = ai_b(side);
        u32 entry;
        if (DSB(slot + 0x63u) == 0u) return;            /* 0x472AA */
        DSW(DS_001088E0 + side * 2u) = 0;               /* 0x472CA */
        if (DSB(0x00105B39u) != 0u) return;             /* 0x472D4 */
        ai_manage(side);                                /* 0x472DA */
        entry = DSD(b + 0x1Cu) + (u32)DSB(b + 0x30u) * 8u;   /* 0x472E7 */
        DSW(DS_001088E0 + side * 2u) =
            fighter_cmd_facing(fighter_actor_bit15_clear(side), DSD(entry));
        if (DSD(entry) == 0xFFFFu) {                    /* 0x4731C */
            DSD(b + 0x04u) = 0;
            DSW(DS_001088E0 + side * 2u) = 0;
        } else if ((s16)DSB(b + 0x31u) >= (s16)DSW(entry + 4u)) {   /* 0x47338 */
            DSB(b + 0x31u) = 0;
            DSB(b + 0x30u) = (u8)(DSB(b + 0x30u) + 1u);
        }
        DSB(b + 0x31u)++;                               /* 0x4734F */
        DSD(b + 0x0Cu)++;                               /* 0x47355 */
    }
}

/* 0x461DC's per-word transform (the EAX == 4 arm): a conditional bit
 * swap/toggle over the local command copy. Only reached when
 * slot[side]+0x63 == 0 and word[DS_00101514 + 0x2D4/0x2D6] == 4. */
static void ai_ring_transform(u16 *w, u8 *flag)
{
    if ((*w & 0x0001u) != 0u && (*w & 0x3030u) == 0u) goto done;
    if ((*w & 0x0001u) != 0u && (*w & 0x1010u) != 0u) {
        if ((*w & 0x0100u) != 0u) *w ^= 0x0101u;
        else                      *w ^= 0x0001u;
        *w |= 0x0202u;
        *flag = 1;
        goto toggle;
    }
    if ((*w & 0x0001u) != 0u && (*w & 0x2020u) != 0u) {
        *w |= 0x0002u;
        if ((*w & 0x0100u) != 0u) *w ^= 0x0100u;
        *flag = 1;
        goto toggle;
    }
    if ((*w & 0x0004u) != 0u && (*w & 0x3030u) == 0u) goto done;
    if ((*w & 0x0004u) != 0u && (*w & 0x1010u) != 0u) {
        if ((*w & 0x0400u) != 0u) *w ^= 0x0404u;
        else                      *w ^= 0x0004u;
        *w |= 0x0808u;
        *flag = 1;
        goto toggle;
    }
    if ((*w & 0x0004u) != 0u && (*w & 0x2020u) != 0u) {
        *w |= 0x0008u;
        if ((*w & 0x0400u) != 0u) *w ^= 0x0400u;
        *flag = 1;
    }
toggle:
    if (*flag == 1u) {
        if ((*w & 0x0010u) != 0u) *w ^= 0x0010u;
        if ((*w & 0x1000u) != 0u) *w ^= 0x1000u;
        if ((*w & 0x0020u) != 0u) *w ^= 0x0020u;
        if ((*w & 0x0020u) != 0u) *w ^= 0x2000u;
    }
done:
    ;
}

/* 0x461DC. Write each side's command word into the 0x14-word input ring at
 * DS_00108270 (+0x28 per side) and advance DS_001082D4. When slot+0x63 is
 * clear and word[DS_00101514 + 0x2D4/0x2D6] == 4 the word goes through
 * 0x3C6E8 and ai_ring_transform first. */
void fighter_input_ring_update(void)
{
    u16 loc[2];
    for (u32 side = 0; side < 2u; side++) {             /* 0x461EF */
        u32 slot = DS_001077B0 + side * 0x94u;
        u32 sel = 2u;                                   /* 0x461F8 */
        if (DSB(slot + 0x63u) == 0u) {                  /* 0x461FF */
            sel = (u32)DSW(DSD(DS_00101514) + 0x2D4u + side * 2u) & 0xFFFFu;
        }
        if (sel == 4u) {                                /* 0x46222 */
            u16 w;
            u8 fl = 0;
            if ((fighter_actor_bit15_clear(side) & 0xFFu) == 1u) {
                w = fighter_cmd_facing(side, DSW(DS_001088E0 + side * 2u));
            } else {
                w = DSW(DS_001088E0 + side * 2u);
            }
            ai_ring_transform(&w, &fl);
            loc[side] = w;
        } else {
            loc[side] = DSW(DS_001088E0 + side * 2u);   /* 0x46402 */
        }
    }
    {
        u32 pos = ((DSD(DS_001082D2) >> 16) + 1u);      /* 0x46420 */
        if (pos >= 0x14u) pos = 0u;
        DSW(DS_00108270 + pos * 2u) = loc[0];           /* 0x4643C */
        DSW(0x001082D4u) = (u16)pos;                    /* 0x46447 */
        DSW(0x00108298 + pos * 2u) = loc[1];            /* 0x4644E */
    }
}

/* 0x24C73. game_frame's per-side command block: fill both command words. */
void fighter_command_block(void)
{
    if (DSB(DS_00104B26) != 0u) return;                 /* 0x24C73 */
    if (DSB(0x00104B1Bu) == 0u) return;                 /* 0x24C7C */
    for (u32 side = 0; side < 2u; side++) {             /* 0x24C8B */
        if ((DSB(DS_001077B0 + side * 0x94u + 0x41u) & 0x10u) == 0u)
            fighter_command_generate(side);             /* 0x24CA1 */
        else
            DSW(DS_001088E0 + side * 2u) = 0;           /* 0x24C96 */
    }
    fighter_input_ring_update();                        /* 0x24CB5 */
}

/* ---- 0x34B6C's +0x52 handlers the demo enters -------------------------- */

/* 0x36638. Clear slot+0x43 bit 0x40 and restart the fighter's animation when
 * that bit is set; the +0x54 arm picks the start animation. Called by 0x349C8,
 * 0x35838 and the 0x34B6C position branch. The +0x54 == 5 arm (0x366FB ->
 * 0x38154) is a named gap (§7.10); no demo path reaches it. */
int fighter_state_36638(u32 slot, u32 rec)
{
    if (DSB(slot + 0x54u) == 3u) {                      /* 0x3663E */
        DSB(slot + 0x43u) &= 0xBFu;
        return 0;
    }
    if ((DSB(slot + 0x43u) & 0x40u) == 0u) return 0;    /* 0x36656 */
    DSD(slot + 0x40u) &= 0xBFFF7FFFu;                   /* 0x3665C */
    DSB(slot + 0x41u) |= 0x80u;                         /* 0x36672 */
    if (DSW(DS_00104B00) == 0x25u) DSB(slot + 0x53u) = 0x0Cu;   /* 0x3667A */
    else                           DSB(slot + 0x53u) = 0u;     /* 0x36680 */
    switch (DSB(slot + 0x54u)) {                        /* 0x36684 table 0x36620 */
    case 1u:                                            /* 0x366B9 */
        actors_anim_begin(rec, DSD(0x000C9210u + (u32)DSB(slot + 0x7Au) * 4u),
                         0x3F800000u);
        DSB(slot + 0x52u) = 0x12u;
        return 1;
    case 4u:                                            /* 0x366D8 */
        actors_anim_begin(rec, DSD(0x000C91E8u + (u32)DSB(slot + 0x7Au) * 4u),
                         0x3F800000u);
        DSB(slot + 0x52u) = 8u;
        DSB(slot + 0x57u) = 2u;
        return 0;
    case 5u:                                            /* 0x366FB */
        /* PORT: 0x38154 — the +0x54 == 5 arm is a named gap (§7.10). */
        return 0;
    default:                                            /* 0x3669A; 0,2,3,>5 */
        actors_anim_begin(rec, DSD(0x000C91E8u + (u32)DSB(slot + 0x7Au) * 4u),
                         0x3F800000u);
        DSB(slot + 0x52u) = 0x12u;
        return 1;
    }
}

/* 0x365C8. 1 when this slot is behind the other's +0x2C in the facing
 * direction and the other slot's +0x43 bit 0x80 is set. */
static int fighter_state_365c8(u32 slot, u32 rec, u32 side)
{
    u32 other;
    if ((DSB(slot + 0x42u) & 0x10u) != 0u) return 0;
    other = DSD(DS_001077A8 + (side ^ 1u) * 4u);
    if (other == 0u) return 0;
    if ((DSB(slot + 0x42u) & 0x08u) != 0u) return 0;
    if ((DSB(other + 0x43u) & 0x80u) == 0u) return 0;
    if ((DSB(other + 0x42u) & 0x08u) != 0u) return 0;
    if ((DSW(rec + 0x28u) >> 8 & 0x40u) == 0u) {
        if ((s32)DSD(slot + 0x2Cu) <= (s32)DSD(other + 0x2Cu)) return 1;
    } else if ((s32)DSD(other + 0x2Cu) < (s32)DSD(slot + 0x2Cu)) {
        return 1;
    }
    return 0;
}

/* 0x35838. The command's direction bits (EBX = cmd & 0xF000) select a fresh
 * animation and set slot+0x52 = 0xE. */
static void fighter_state_35838(u32 slot, u32 rec, u32 dirbits)
{
    DSD(slot + 0x40u) &= 0xFCFF7FFFu;                   /* 0x3583F */
    DSB(slot + 0x41u) |= 0x80u;                         /* 0x35846 */
    if ((DSW(rec + 0x28u) >> 8 & 0x40u) == 0u) {        /* 0x35859 */
        if ((dirbits & 0x2000u) != 0u) {                /* 0x35866 */
            actors_anim_begin(rec, DSD(0x000C8A40u + (u32)DSB(slot + 0x7Au) * 4u),
                             0x3F800000u);
            DSB(slot + 0x43u) |= 2u;
            goto tail;
        }
        if (DSW(DS_00104B00) == 0x22u) {                /* 0x35891 */
            DSB(slot + 0x43u) |= 0x40u;
            (void)fighter_state_36638(slot, rec);
            goto tail;
        }
    } else {
        if ((dirbits & 0x1000u) != 0u) {                /* 0x358B9 */
            actors_anim_begin(rec, DSD(0x000C8A40u + (u32)DSB(slot + 0x7Au) * 4u),
                             0x3F800000u);
            DSB(slot + 0x43u) |= 2u;
            DSW(slot + 0x4Cu) = 0;
            DSB(slot + 0x52u) = 0x0Eu;
            DSB(slot + 0x53u) = 0u;
            return;
        }
        if (DSW(DS_00104B00) == 0x22u) {                /* 0x358EF */
            DSB(slot + 0x43u) |= 0x40u;
            (void)fighter_state_36638(slot, rec);
            DSW(slot + 0x4Cu) = 0;
            DSB(slot + 0x52u) = 0x0Eu;
            DSB(slot + 0x53u) = 0u;
            return;
        }
    }
    actors_anim_begin(rec, DSD(0x000C8AB8u + (u32)DSB(slot + 0x7Au) * 4u),
                     0x3F800000u);
    DSB(slot + 0x43u) |= 1u;
tail:
    DSW(slot + 0x4Cu) = 0;                              /* 0x35928 */
    DSB(slot + 0x52u) = 0x0Eu;                          /* 0x3592E */
    DSB(slot + 0x53u) = 0u;
}

/* 0x349C8. The +0x52 == 0 default handler. Its +0x42 bit 6/7 arms
 * (0x37178/0x37D18) are named gaps (§7.10); no demo writer sets either bit. */
void fighter_state_default(u32 side)
{
    u32 slot = DSD(DS_001077A8 + side * 4u);            /* 0x349CF */
    u32 rec;
    if (slot == 0u) return;
    rec = DSD(slot);
    if ((DSB(slot + 0x42u) & 0x40u) != 0u) {            /* 0x349E6 */
        fighter_37178(slot);                            /* 0x349EA */
        return;
    }
    if ((DSB(slot + 0x42u) & 0x80u) != 0u) {            /* 0x349F7 */
        /* 0x349F9 reads the side from the actor (ESI = rec), then the other
         * slot; only when the other slot's +0x40 holds 0x8200000 does it run
         * 0x37D18 on that slot and set this slot's +0x53 = 3. */
        u32 other = DSD(DS_001077A8                     /* 0x34A03 */
                        + ((u32)DSB(rec + 0x51u) ^ 1u) * 4u);
        if (other != 0u                                /* 0x34A0C */
                && (DSD(other + 0x40u) & 0x8200000u) == 0x8200000u) {   /* 0x34A1B */
            fighter_37d18(other, DSD(other));           /* 0x34A29 */
            DSB(slot + 0x53u) = 3u;                     /* 0x34A2E */
        }
        return;
    }
    if (fighter_state_365c8(slot, rec, side) != 0)      /* 0x34A3E */
        DSB(slot + 0x43u) |= 0x40u;
    else
        DSB(slot + 0x43u) &= 0xBFu;
    if (fighter_state_36638(slot, rec) != 0) return;    /* 0x34A55 */
    {
        u16 cmd = DSW(DS_001088E0 + side * 2u);         /* 0x34A62 */
        int bvar2 = ((cmd >> 8) & 3u) != 0u && ((cmd >> 8) & 0xCu) != 0u;
        if (!bvar2) {
            if (fighter_attack_consume(side) != 0) return;   /* 0x34A9F */
        }
        if ((cmd & 0x4000u) != 0u) {                    /* 0x34AB5 */
            actors_anim_begin(rec,
                DSD(0x000C8978u + (u32)DSB(slot + 0x7Au) * 4u), 0x40000000u);
            DSB(slot + 0x52u) = 5u;
            DSB(slot + 0x54u) = 1u;
            return;
        }
        if ((DSB(slot + 0x53u) == 0u && (cmd & 0x1000u) != 0u)   /* 0x34AE1 */
                || (cmd & 0x2000u) != 0u)
            fighter_state_35838(slot, rec, (u32)(cmd & 0xF000u));   /* 0x34B06 */
    }
}

/* 0x35D7C. The +0x52 == 3 handler. It clears the slot's +0x53/+0x54, then,
 * when 0x3CF38 reports no hit, arms slot+0x54 = 2 and slot+0x53 = 4. */
void fighter_state_35d7c(u32 side)
{
    u32 slot = DSD(DS_001077A8 + side * 4u);            /* 0x35DF1 */
    if (slot == 0u) return;
    DSB(slot + 0x54u) = 0;                              /* 0x35DDA */
    DSB(slot + 0x53u) = 0;                              /* 0x35DDE */
    if (hit_chain_resolve(side) == 0) {                 /* 0x35DE8 0x3CF38 */
        DSB(slot + 0x54u) = 2u;                         /* 0x35DF5 */
        DSB(slot + 0x53u) = 4u;
    }
}

/* ---- the remaining 0x34B14 +0x52 handlers (record §2.2) ------------------
 * Each handler is one C function per original function, ported from the
 * Ghidra decompilation + disassembly (register args resolved from each
 * prologue). The dispatch table is 0x34B14; the entries are in fight.c. */

static void fighter_state_367dc(u32 slot, u32 rec);         /* 0x367DC */
static void hit_stance_timer(u32 side);                     /* 0x1922C */
static void hit_anim_start_a(u32 rec, u32 stream, u32 frame_bits); /* 0x3C480 */
static void hit_anim_start_c(u32 rec, u32 stream, u32 frame_bits); /* 0x3C520 */

/* PORT: data-object addresses symbols.h does not name. */
#define FIGHT_LAND_THR     0x000BD882u  /* 0x35F84: per-char landing dword */
#define FIGHT_35F84_78     0x000BDC16u  /* 0x35F84: the +0x78 word source */
#define FIGHT_361C8_THR    0x000BDA68u  /* 0x361C8: per-char ushort */
#define FIGHT_ANIM_361C8   0x000C90A8u  /* 0x361C8: per-char anim stream */
#define FIGHT_36300_THR    0x000BDA76u  /* 0x36300: per-char ushort */
#define FIGHT_36300_STEP   0x000BDA84u  /* 0x36300: per-char speed byte */
#define FIGHT_ANIM_36300   0x000C90D0u  /* 0x36300: per-char anim stream */
#define FIGHT_36710_FLAG   0x00107808u  /* 0x36710: slot+0x58 */
#define FIGHT_36710_43     0x000BD89Au  /* 0x36710: the rec+0x43 byte source */
#define FIGHT_ANIM_36710   0x000C9080u  /* 0x36710: per-char anim stream */
#define FIGHT_ANIM_36430_A 0x000C89C8u  /* 0x36430/0x364FC: first stream */
#define FIGHT_ANIM_36430_B 0x000C8F90u  /* 0x36430: the +0x52=0x15 stream */
#define FIGHT_ANIM_364FC_B 0x000C8FB8u  /* 0x364FC: the +0x52=5 stream */

/* 0x3C148. Zero the record's +0x34 word and +0x43/+0x42 bytes. */
static void fighter_3c148(u32 side)
{
    u32 rec = DSD(DS_001077B0 + side * 0x94u);          /* 0x3C14E */
    DSW(rec + 0x34u) = 0;                               /* 0x3C155 */
    DSB(rec + 0x43u) = 0;                               /* 0x3C15B */
    DSB(rec + 0x42u) = 0;                               /* 0x3C160 */
}

/* 0x3C16C. Zero the record's +0x36 and +0x44 words. */
static void fighter_3c16c(u32 side)
{
    u32 rec = DSD(DS_001077B0 + side * 0x94u);          /* 0x3C172 */
    DSW(rec + 0x36u) = 0;                               /* 0x3C179 */
    DSW(rec + 0x44u) = 0;                               /* 0x3C17E */
}

/* 0x1A640. 0x1000 when this side's facing bit 0x4000 is clear and the
 * command's 0x1000 is set; 0x2000 in the mirror; else 0. */
int fighter_1a640(u32 side)
{
    u32 rec = DSD(DS_001077B0 + side * 0x94u);          /* 0x1A64C */
    u16 cmd = DSW(DS_001088E0 + side * 2u);
    if ((DSW(rec + 0x28u) >> 8 & 0x40u) == 0u) {
        if ((cmd >> 8 & 0x10u) != 0u) return 0x1000;    /* 0x1A670 */
    } else if ((cmd >> 8 & 0x20u) != 0u) {
        return 0x2000;                                  /* 0x1A65C */
    }
    return 0;
}

/* 0x35F84. The +0x52 == 4 handler: set +0x41 bit 0x80; when the landing gate
 * (word[0xBD882+char*2] >> 16 >= slot+0x30 && rec+0x36 <= 0) passes, run
 * 0x1922C, zero rec+0x24, set slot+0x52 = 0x14, slot+0x78 = word[0xBDC16],
 * slot+0x53 = 4 and run 0x3C148/0x3C16C. */
void fighter_state_35f84(u32 slot, u32 rec)
{
    u32 side = DSB(rec + 0x51u);
    DSB(slot + 0x41u) |= 0x80u;                         /* 0x35FE9 */
    if ((s32)((s32)DSD(FIGHT_LAND_THR
                       + (u32)DSB(slot + 0x7Au) * 2u) >> 16)
            >= (s32)DSD(slot + 0x30u)                   /* 0x36003 */
        && (s16)DSW(rec + 0x36u) <= 0) {                /* 0x36009 */
        hit_stance_timer(side);                         /* 0x3601D */
        DSD(rec + 0x24u) = 0;                           /* 0x3602A */
        DSB(slot + 0x52u) = 0x14u;                      /* 0x36037 */
        DSW(slot + 0x78u) = DSW(FIGHT_35F84_78);        /* 0x3603B */
        DSB(slot + 0x53u) = 4u;                         /* 0x36042 */
        fighter_3c148(side);                            /* 0x36046 */
        fighter_3c16c(side);                            /* 0x3604E */
    }
}

/* 0x361C8. The +0x52 == 12 handler. */
void fighter_state_361c8(u32 slot, u32 rec)
{
    DSB(slot + 0x41u) |= 0x80u;                         /* 0x361CF */
    switch (DSB(slot + 0x58u)) {                        /* 0x361D6 */
    case 1u:                                            /* 0x361F2 */
        if ((s16)DSW(rec + 0x36u) < 0
                && (s32)DSD(rec + 0x1Cu)
                   <= (s32)(u32)DSW(FIGHT_361C8_THR
                                    + (u32)DSB(slot + 0x7Au) * 2u)) {
            DSB(slot + 0x58u) = (u8)(DSB(slot + 0x58u) + 1u);   /* 0x3620A */
            actors_anim_begin(rec, DSD(FIGHT_ANIM_361C8
                                       + (u32)DSB(slot + 0x7Au) * 4u),
                              0x40000000u);             /* 0x36220 */
            return;
        }
        /* fallthrough */
    case 2u:                                            /* 0x36229 */
        DSW(rec + 0x36u) = (u16)(DSW(rec + 0x36u) - 0x38u);
        return;
    case 3u:
        {
            s32 v = ((s16)DSW(rec + 0x34u) < 0)
                    ? -(s32)((s32)DSD(rec + 0x32u) >> 16)
                    : (s32)((s32)DSD(rec + 0x32u) >> 16);
            if (v < 0x11) {
                DSW(rec + 0x34u) = 0;
                DSB(slot + 0x52u) = 9u;
                return;
            }
            {
                s16 s = (s16)DSW(rec + 0x34u);
                DSW(rec + 0x34u) = (u16)(s > 0 ? s - 0x10 : s + 0x10);
            }
        }
        return;
    default:
        return;
    }
}

/* 0x36300. The +0x52 == 13 handler. */
void fighter_state_36300(u32 slot, u32 rec)
{
    DSB(slot + 0x41u) |= 0x80u;                         /* 0x36309 */
    switch (DSB(slot + 0x58u)) {                        /* 0x3630D */
    case 1u:                                            /* 0x36325 */
        if ((s16)DSW(rec + 0x36u) >= 0
                || (s32)(u32)DSW(FIGHT_36300_THR
                                 + (u32)DSB(slot + 0x7Au) * 2u)
                   < (s32)DSD(rec + 0x1Cu)) {
            DSW(rec + 0x36u) = (u16)(DSW(rec + 0x36u)
                                     - (u32)DSB(slot + 0x5Cu));  /* 0x3637D */
            return;
        }
        DSB(slot + 0x58u) = (u8)(DSB(slot + 0x58u) + 1u);   /* 0x36344 */
        {
            union { float f; u32 u; } fu;
            fu.f = (float)(u32)DSB(FIGHT_36300_STEP
                                   + (u32)DSB(slot + 0x7Au));
            actors_anim_begin(rec, DSD(FIGHT_ANIM_36300
                                       + (u32)DSB(slot + 0x7Au) * 4u),
                              fu.u);                    /* 0x3636B */
        }
        break;
    case 2u:                                            /* 0x36372 */
        DSW(rec + 0x36u) = (u16)((s16)DSW(rec + 0x36u)
                                 - (u32)DSB(slot + 0x5Cu));
        return;
    case 3u:
        {
            s32 v = ((s16)DSW(rec + 0x34u) < 0)
                    ? -(s32)((s32)DSD(rec + 0x32u) >> 16)
                    : (s32)((s32)DSD(rec + 0x32u) >> 16);
            if (v < 0x11) {
                DSW(rec + 0x34u) = 0;
                DSB(slot + 0x52u) = 9u;
                return;
            }
            {
                s16 s = (s16)DSW(rec + 0x34u);
                DSW(rec + 0x34u) = (u16)(s > 0 ? s - 0x10 : s + 0x10);
            }
        }
        return;
    default:
        return;
    }
}

/* 0x36710. The +0x52 == 17 handler: step slot+0x58 0 -> 1 -> 2; at 2 with
 * rec+0x36 == 0 and rec+0x1C == 0 write rec+0x43 = byte[0xBD89A], slot+0x52 = 9
 * and call 0x2C3FC(0x6E) (the voice, out of scope). */
void fighter_state_36710(u32 slot, u32 rec)
{
    u32 side = DSB(rec + 0x51u);
    u8 f = DSB(FIGHT_36710_FLAG + side * 0x94u);        /* 0x3671C */
    if (f == 0u) {
        DSB(FIGHT_36710_FLAG + side * 0x94u) = 1u;      /* 0x3672C */
    } else if (f < 2u) {
        hit_anim_start_a(rec, DSD(FIGHT_ANIM_36710
                                  + (u32)DSB(slot + 0x7Au) * 4u),
                         0x40400000u);                  /* 0x367A6 */
        DSB(FIGHT_36710_FLAG + side * 0x94u) = 2u;      /* 0x367AB */
    } else if (f == 2u && (s16)DSW(rec + 0x36u) == 0 && DSD(rec + 0x1Cu) == 0) {
        DSB(rec + 0x43u) = DSB(FIGHT_36710_43);         /* 0x367C3 */
        DSB(slot + 0x52u) = 9u;                         /* 0x367CB */
        /* PORT: 0x367CF 0x2C3FC(0x6E) — the character voice, out of scope. */
    }
}

/* 0x399CC. The +0x52 == 7 handler: 0x33A10's self slot gets +0x41 bit 2 and
 * +0x74 = 0, then, when the side's slot pointer is live and its +0x5D is clear,
 * 0x367DC(slot, *slot). */
void fighter_state_399cc(u32 side)
{
    u32 ctx[6];
    u32 slot;
    fighter_ctx_swap(ctx, side);                        /* 0x399D7 */
    slot = ctx[3];
    DSB(slot + 0x41u) |= 4u;                            /* 0x399E0 */
    DSW(slot + 0x74u) = 0;                              /* 0x399E8 */
    {
        u32 s = DSD(DS_001077A8 + side * 4u);           /* 0x399EE */
        if (s != 0u && DSB(s + 0x5Du) == 0u)
            fighter_state_367dc(slot, DSD(slot));       /* 0x367DC */
    }
}

/* 0x36430. The +0x52 == 5 handler. */
void fighter_state_36430(u32 slot, u32 rec, u32 side)
{
    if (fighter_state_365c8(slot, rec, side) != 0)      /* 0x36439 */
        DSB(slot + 0x43u) |= 0x40u;
    else
        DSB(slot + 0x43u) &= 0xBFu;
    if (fighter_state_36638(slot, rec) != 0) return;    /* 0x36450 */
    {
        u16 cmd = DSW(DS_001088E0 + side * 2u);
        if ((cmd >> 8 & 0x40u) == 0u) {
            if ((cmd >> 8 & 0x80u) == 0u) {
                actors_anim_begin(rec, DSD(FIGHT_ANIM_36430_A
                                           + (u32)DSB(slot + 0x7Au) * 4u),
                                  0x40000000u);         /* 0x364BF */
                DSB(slot + 0x52u) = 9u;                 /* 0x364C4 */
                DSB(slot + 0x54u) = 0u;                 /* 0x364C8 */
                return;
            }
            if ((DSB(slot + 0x43u) & 4u) == 0u) {
                (void)fighter_attack_consume(side);     /* 0x364A5 */
                return;
            }
        } else if (fighter_1a640(side) != 0) {          /* 0x364D2 */
            DSB(slot + 0x52u) = 0x15u;                  /* 0x364E0 */
            actors_anim_begin(rec, DSD(FIGHT_ANIM_36430_B
                                       + (u32)DSB(slot + 0x7Au) * 4u),
                              0x40400000u);             /* 0x364F2 */
        }
    }
}

/* 0x364FC. The +0x52 == 21 handler. */
void fighter_state_364fc(u32 slot, u32 rec, u32 side)
{
    if (fighter_state_365c8(slot, rec, side) != 0)      /* 0x36505 */
        DSB(slot + 0x43u) |= 0x40u;
    else
        DSB(slot + 0x43u) &= 0xBFu;
    if (fighter_state_36638(slot, rec) != 0) return;    /* 0x3651C */
    {
        u16 cmd = DSW(DS_001088E0 + side * 2u);
        if ((cmd >> 8 & 0x40u) == 0u) {
            if ((cmd >> 8 & 0x80u) == 0u) {
                actors_anim_begin(rec, DSD(FIGHT_ANIM_36430_A
                                           + (u32)DSB(slot + 0x7Au) * 4u),
                                  0x40000000u);         /* 0x3658B */
                DSB(slot + 0x52u) = 9u;                 /* 0x36590 */
                DSB(slot + 0x54u) = 0u;                 /* 0x36594 */
                return;
            }
            if ((DSB(slot + 0x43u) & 4u) == 0u) {
                (void)fighter_attack_consume(side);     /* 0x36571 */
                return;
            }
        } else if (fighter_1a640(side) == 0) {          /* 0x3659E */
            DSB(slot + 0x52u) = 5u;                     /* 0x365AA */
            actors_anim_begin(rec, DSD(FIGHT_ANIM_364FC_B
                                       + (u32)DSB(slot + 0x7Au) * 4u),
                              0x40000000u);             /* 0x365BC */
        }
    }
}

/* ---- the five remaining 0x34B14 +0x52 handlers (record §1) ---------------
 * One C function per original: 0x359E0 (state 1), 0x35C1C + 0x35D20 (state 2),
 * 0x37464 (state 8), 0x33B00 (state 19) and 0x35E6C (state 20), plus the
 * helpers 0x35B7C / 0x1883C / 0x36E78 and 0x29BC8 they share. The dispatch
 * wiring is fight.c's fight_health_sync (0x34B6C). */

/* PORT: data-object addresses symbols.h does not name. */
#define FIGHT_35C1C_SEEK_A  0x000C8A68u /* 0x35C1C: +0x43 bit 1 seek table */
#define FIGHT_35C1C_SEEK_B  0x000C8AE0u /* 0x35C1C: default seek table */
#define FIGHT_35D20_ANIM_A  0x000C8A90u /* 0x35D20: +0x43 bit 1 begin stream */
#define FIGHT_35D20_ANIM_B  0x000C8B08u /* 0x35D20: default begin stream */
#define FIGHT_35E6C_ANIM    0x000C8B58u /* 0x35E6C: per-char begin stream */

static void fighter_state_35b7c(u32 slot, u32 rec);         /* 0x35B7C */
void fighter_state_35d20(u32 slot, u32 rec);                /* 0x35D20 */
static void fighter_1883c(u32 side, u32 a, u32 b);          /* 0x1883C */
static void fighter_36e78(u32 slot);                        /* 0x36E78 */
static u32  hit_record_x(u32 side);                         /* 0x18714 */
static u32  hit_record_y(u32 side);                         /* 0x18788 */
static void hit_facing_flag(u32 side);                      /* 0x18B04 */

/* 0x29BC8. Resolve the character's palette handle for `side` and point `rec`'s
 * pset at it (0x2A17C with word 0). */
static void fighter_29bc8(u32 side, u32 rec, u32 ch)
{
    u32 tbl = DSD(DS_000A8A98 + ch * 4u);               /* 0x29BCD */
    u32 handle = DSD(tbl + (u32)DSB(DS_00105B34 + side) * 4u);
    actor_pset_palette(rec, 0u, handle);                /* 0x29BE1 */
}

/* 0x35D20. Start the +0x43-selected landing animation, set +0x52 = 9, clear
 * +0x43 bits 0/1, and (when +0x54 == 4) raise DS_001078FE. +0x53 survives only
 * at 0x0D. */
void fighter_state_35d20(u32 slot, u32 rec)
{
    u32 stream = (DSB(slot + 0x43u) & 2u) != 0u
        ? DSD(FIGHT_35D20_ANIM_A + (u32)DSB(slot + 0x7Au) * 4u)   /* 0x35D31 */
        : DSD(FIGHT_35D20_ANIM_B + (u32)DSB(slot + 0x7Au) * 4u);  /* 0x35D3F */
    actors_anim_begin(rec, stream, 0x3F800000u);        /* 0x35D4B */
    DSB(slot + 0x52u) = 9u;                             /* 0x35D53 */
    DSB(slot + 0x43u) &= 0xFCu;                         /* 0x35D57 */
    if (DSB(slot + 0x54u) == 4u) DSB(DS_001078FE) = 1u; /* 0x35D60 */
    if (DSB(slot + 0x53u) != 0x0Du) DSB(slot + 0x53u) = 0u;   /* 0x35D6C */
}

/* 0x35C1C. Step the animation frame by +0x58, clamp it to [0, max) (max is the
 * low byte of the word 0x35B7C reads), set +0x29 bit 8, seek the +0x43-selected
 * frame stream, set +0x28 bit 4, and return 1 when the clamp fired. */
int fighter_state_35c1c(u32 slot, u32 rec)
{
    u8 max = DSB(DS_000BDA3E + (u32)DSB(slot + 0x7Au) * 2u);   /* 0x35C25 */
    int r = 0;
    DSB(rec + 0x52u) = (u8)(DSB(rec + 0x52u) + DSB(rec + 0x58u));  /* 0x35C2F */
    if ((s8)DSB(rec + 0x52u) < 0) {                     /* 0x35C37 */
        DSB(rec + 0x52u) = (u8)(max - 1u);              /* 0x35C3B */
        r = 1;
    } else if ((s8)max <= (s8)DSB(rec + 0x52u)) {       /* 0x35C47 */
        DSB(rec + 0x52u) = 0u;                          /* 0x35C50 */
        r = 1;
    }
    DSB(rec + 0x29u) |= 8u;                             /* 0x35C5A/0x35C7F */
    {
        u32 base = (DSB(slot + 0x43u) & 2u) != 0u
            ? DSD(FIGHT_35C1C_SEEK_A + (u32)DSB(slot + 0x7Au) * 4u)  /* 0x35C66 */
            : DSD(FIGHT_35C1C_SEEK_B + (u32)DSB(slot + 0x7Au) * 4u); /* 0x35C91 */
        actors_anim_seek(rec, (u32)DSW(base
            + (u32)(s8)DSB(rec + 0x52u) * 2u));         /* 0x35CA3 */
    }
    DSB(rec + 0x28u) |= 4u;                             /* 0x35CA8 */
    return r;
}

/* 0x35B7C. Enter the +0x52 = 2 approach: clear +0x53 unless 0x0D, set the
 * +0x58 frame step from the frame position, and hand off to 0x35C1C/0x35D20. */
static void fighter_state_35b7c(u32 slot, u32 rec)
{
    s32 max;
    s32 frame;
    DSB(slot + 0x52u) = 2u;                             /* 0x35B87 */
    if (DSB(slot + 0x53u) != 0x0Du) DSB(slot + 0x53u) = 0u;   /* 0x35B8B */
    max = (s32)(s16)DSW(DS_000BDA3E + (u32)DSB(slot + 0x7Au) * 2u);  /* 0x35B99 */
    frame = (s8)((s32)DSD(rec + 0x4Fu) >> 24);          /* 0x35BAD */
    if (frame < max / 2)                                /* 0x35BB3 */
        DSB(rec + 0x58u) = (u8)(-(frame / 3));          /* 0x35BF3 */
    else
        DSB(rec + 0x58u) = (u8)((max - (s32)(s8)DSB(rec + 0x52u)) / 3);  /* 0x35BD5 */
    if (DSB(rec + 0x58u) == 0u || fighter_state_35c1c(slot, rec) != 0)
        fighter_state_35d20(slot, rec);                 /* 0x35C0D */
}

/* 0x1883C. Re-latch both slots, add (a, b) to this side's +0x2C/+0x30, then
 * re-derive the record's +0x18/+0x1C through 0x18714/0x18788. */
static void fighter_1883c(u32 side, u32 a, u32 b)
{
    u32 slot = DS_001077B0 + side * 0x94u;
    fighter_slot_latch(0u);                             /* 0x18846 */
    fighter_slot_latch(1u);                             /* 0x18852 */
    DSD(slot + 0x2Cu) += a;                             /* 0x18878 */
    DSD(slot + 0x30u) += b;                             /* 0x18880 */
    DSD(DSD(slot) + 0x18u) = hit_record_x(side);        /* 0x18891 */
    DSD(DSD(slot) + 0x1Cu) = hit_record_y(side);        /* 0x188A1 */
}

/* 0x36E78. The +0x5B/landing reset: when +0x5B is set, arm +0x5A = 0x78 - +0x5B
 * and clear +0x5B; otherwise clear +0x5D, set +0x40 bits 0x1000/0x100000, reset
 * the other slot's +0x5D/+0x43 and the DS_001078FF character's palette. */
static void fighter_36e78(u32 slot)
{
    u8 b = DSB(slot + 0x5Bu);                           /* 0x36E7B */
    if (b != 0u) {
        DSB(slot + 0x5Bu) = 0u;                         /* 0x36E82 */
        DSB(slot + 0x5Au) = (u8)(0x78u - b);            /* 0x36E90 */
        DSB(slot + 0x41u) |= 8u;                        /* 0x36E93 */
        return;
    }
    DSB(slot + 0x5Du) = 0u;                             /* 0x36EA3 */
    DSD(slot + 0x40u) = (DSD(slot + 0x40u) | 0x00101000u) & 0xFBFFF7FFu; /* 0x36EAF */
    {
        u32 other = DSD(DS_001077A8
            + ((u32)DSB(DSD(slot) + 0x51u) ^ 1u) * 4u); /* 0x36EBE */
        if (other != 0u) {
            u32 side;
            u32 slot2;
            DSB(other + 0x5Du) = 0u;                    /* 0x36EC9 */
            DSB(other + 0x43u) &= 0xFBu;                /* 0x36EDB */
            side = (u32)DSB(DS_001078FF);               /* 0x36ED5 */
            slot2 = DS_001077B0 + side * 0x94u;         /* 0x36EF0 */
            /* 0x36EF0 reads DSD(slot2) as the record; 0x36EF7 reads the char
             * from DSB(slot2 + 0x7A) (the slot), not the actor. */
            fighter_29bc8(side, DSD(slot2),
                          (u32)DSB(slot2 + 0x7Au));     /* 0x36F00 */
            DSB(DS_00104AE9) &= 0xFEu;                  /* 0x36F05 */
        }
    }
}

/* 0x359E0. State 1: the approach/command gate. When 0x365C8 flags the pair the
 * slot's +0x43 bit 0x40 is set and 0x35B7C runs; the mode/input gate then either
 * 0x367DC-resets or 0x35B7C-approaches, and the pass tail applies the per-side
 * x delta, 0x35C1C's animation step and the 0x1883C anchor write. */
void fighter_state_359e0(u32 slot, u32 rec, u32 side)
{
    u16 cmd = DSW(DS_001088E0 + side * 2u);
    if (fighter_state_365c8(slot, rec, side) != 0) {    /* 0x359EA */
        DSB(slot + 0x43u) |= 0x40u;                     /* 0x359F8 */
        fighter_state_35b7c(slot, rec);                 /* 0x359FF */
        return;
    }
    if ((u8)(cmd >> 8) & 0xC0u) {                       /* 0x35A09 */
        fighter_state_367dc(slot, rec);                 /* 0x35A21 */
        return;
    }
    if (ai_facing_cmd(side) != 0u) {                    /* 0x35A2D 0x1A5D4 */
        if ((DSB(slot + 0x43u) & 2u) == 0u) {           /* 0x35A36 */
            fighter_state_35b7c(slot, rec);             /* 0x35A40 */
            return;
        }
    } else if (fighter_1a640(side) == 0u                   /* 0x35A4C */
               || (DSB(slot + 0x43u) & 1u) == 0u) {    /* 0x35A55 */
        fighter_state_35b7c(slot, rec);                 /* 0x35A5F */
        return;
    }
    if ((DSB(slot + 0x41u) & 0x40u) != 0u                /* 0x35A69 */
            && ((u8)(cmd >> 8) & 5u) != 5u)
        DSB(slot + 0x41u) &= 0xBFu;                     /* 0x35A8F */
    if ((DSB(slot + 0x41u) & 0x40u) == 0u               /* 0x35A92 */
            && (DSB(DS_000EF6DC) & 1u) == 0u)
        return;
    if (ai_facing_cmd(side) != 0u) {                    /* 0x35AAD */
        s32 d = (s32)DSD(DS_000BDBEC + (u32)DSB(slot + 0x7Au) * 2u) >> 16;
        if (fighter_actor_bit15_clear(side) != 0)       /* 0x35AB8 */
            DSD(rec + 0x18u) -= (u32)d;                 /* 0x35AD7 */
        else
            DSD(rec + 0x18u) += (u32)d;                 /* 0x35AF0 */
    } else {
        s32 d = (s32)DSD(DS_000BDC00 + (u32)DSB(slot + 0x7Au) * 2u) >> 16;
        if (fighter_actor_bit15_clear(side) != 0)       /* 0x35AF7 */
            DSD(rec + 0x18u) += (u32)d;                 /* 0x35B14 */
        else
            DSD(rec + 0x18u) -= (u32)d;                 /* 0x35B2D */
    }
    (void)fighter_state_35c1c(slot, rec);               /* 0x35B34 */
    {
        s32 frame = (s8)((s32)DSD(rec + 0x4Fu) >> 24);  /* 0x35B39 */
        s16 sx = (s16)((s32)(s8)*(mem
            + (u32)((s32)DSD(rec + 0x0Cu) + frame * 2)) << 6);   /* 0x35B4F */
        if ((DSW(rec + 0x28u) >> 8 & 0x40u) != 0u)      /* 0x35B53 */
            sx = (s16)(-sx);                            /* 0x35B66 */
        DSW(slot + 0x4Cu) = (u16)sx;                    /* 0x35B4F */
        /* 0x35B49 reads the side from F+0x51 (the actor), not the dispatch
         * argument; the two are equal by construction. */
        fighter_1883c((u32)DSB(rec + 0x51u), (u32)(s32)sx, 0u);   /* 0x35B72 */
    }
}

/* 0x37464. State 8: the other-side approach. Compute the target x from the other
 * fighter and the 0x1078DC pair table; on the +0x57 == 0/1 arms with
 * |diff| <= 0x200 either hand off to 0x35B7C (state 9) or set +0x43 bit 0x40
 * and run 0x36638; the default arm runs 0x35C1C and the 0x1883C anchor write. */
void fighter_state_37464(u32 side)
{
    u32 other = 1u - side;
    u32 slot = DS_001077B0 + side * 0x94u;
    u32 so = DS_001077B0 + other * 0x94u;
    u32 rec = DSD(slot);
    u32 ro = DSD(so);
    /* TODO(verify): 0x374E3 loads DSD(0x1078DC) before indexing, and the raw's
     * 0x36F10 (the unported pose/winner chain) initializes that pointer to
     * 0xBD89C. This reads the pointer word as the table; the faithful form is
     * DSW(DSD(0x1078DC) + …) once 0x36F10's initialization is ported.
     * Record §1.3's Task-3 correction. */
    s16 base = (s16)DSW(DS_001078DC
        + (u32)DSB(slot + 0x7Au) * 14u + (u32)DSB(so + 0x7Au) * 2u);  /* 0x374D1 */
    s16 x;
    s16 diff;
    if (fighter_actor_bit15_clear(other) == 0)          /* 0x374C8 */
        x = (s16)((s16)DSW(ro + 0x18u) + base);         /* 0x3753C */
    else
        x = (s16)((s16)DSW(ro + 0x18u) - base);         /* 0x37509 */
    diff = (s16)((s16)DSW(rec + 0x18u) - x);            /* 0x3754D */
    switch (DSB(slot + 0x57u)) {                        /* 0x3754A */
    case 0u:                                            /* 0x37561 */
        if (diff >= -0x200 && diff <= 0x200) {          /* 0x37570 */
            DSB(slot + 0x52u) = 9u;                     /* 0x3757B */
            DSD(rec + 0x18u) = (u32)(s32)x;             /* 0x37586 */
            fighter_state_35b7c(slot, rec);             /* 0x3758D */
            return;
        }
        break;
    case 1u:                                            /* 0x37597 */
        if (diff >= -0x200 && diff <= 0x200) {          /* 0x375A2 */
            DSB(slot + 0x43u) |= 0x40u;                 /* 0x375AD */
            (void)fighter_state_36638(slot, rec);       /* 0x375B5 */
            return;
        }
        break;
    case 2u:                                            /* 0x37559 */
        return;
    default:
        break;
    }
    if ((DSB(DS_000EF6DC) & 1u) != 0u) {                /* 0x375BF */
        s32 frame;
        s16 sx;
        (void)fighter_state_35c1c(slot, rec);           /* 0x375D8 */
        frame = (s8)((s32)DSD(rec + 0x4Fu) >> 24);
        sx = (s16)((s32)(s8)*(mem
            + (u32)((s32)DSD(rec + 0x0Cu) + frame * 2)) << 6);
        if ((DSW(rec + 0x28u) >> 8 & 0x40u) != 0u)
            sx = (s16)(-sx);
        DSW(slot + 0x4Cu) = (u16)sx;
        /* 0x375E3 reads the side from F+0x51 (the actor), not the parameter;
         * the two are equal by construction. */
        fighter_1883c((u32)DSB(rec + 0x51u), (u32)(s32)sx, 0u);   /* 0x37631 */
    }
}

/* 0x33B00. State 19: overwrite the slot's 0x94 bytes from `src` and its actor's
 * 0x68 bytes from `dst2` (restoring the slot's +0x5A/+0x5D and the actor's first
 * two dwords), clear the +0x08 pointer, run the 0x36E78 landing reset when
 * +0x5A >= 0x78, and re-acquire the character palette. */
void fighter_state_33b00(u32 side, u32 src, u32 dst2)
{
    u32 slot = DS_001077B0 + side * 0x94u;
    u8 a = DSB(slot + 0x5Au);                           /* 0x33B21 */
    u8 b = DSB(slot + 0x5Du);                           /* 0x33B2D */
    u32 rec;
    u32 f0;
    u32 f1;
    memcpy(mem + slot, mem + src, 0x94u);               /* 0x33B42 */
    /* PORT: 0x33B44 0x2EA30(0x2700) interrupt-lock — inert in the port. */
    rec = DSD(slot);                                    /* 0x33B49 */
    f0 = DSD(rec + 0u);
    f1 = DSD(rec + 4u);
    memcpy(mem + rec, mem + dst2, 0x68u);               /* 0x33B62 */
    DSD(rec + 0u) = f0;                                 /* 0x33B6D */
    DSD(rec + 4u) = f1;                                 /* 0x33B79 */
    /* PORT: 0x33B7C 0x2EA30(0x2700) interrupt-lock — inert in the port. */
    DSB(slot + 0x5Au) = a;                              /* 0x33B85 */
    DSB(slot + 0x5Du) = b;                              /* 0x33B95 */
    if (DSD(slot + 0x08u) != 0u) {                      /* 0x33B9B */
        DSB(slot + 0x64u) = 0xFFu;                      /* 0x33BA3 */
        DSD(slot + 0x08u) = 0u;                         /* 0x33BA9 */
    }
    if ((u8)DSB(slot + 0x5Au) >= 0x78u                  /* 0x33BC8 */
            && DSW(DS_00104B00) != 3u)
        fighter_36e78(slot);                            /* 0x33BE1 */
    fighter_29bc8(side, DSD(slot), (u32)DSB(slot + 0x7Au));   /* 0x33C0A */
}

/* 0x35E6C. State 20: set +0x41 bit 0x80 and, unless the command's high bits or
 * 0x3BDDC veto, clear +0x54/+0x53, set +0x52 = 9, bump the +0x84 counter, zero
 * +0x92, start the +0x52 = 9 animation and re-anchor the record. */
void fighter_state_35e6c(u32 slot, u32 rec)
{
    u32 side = (u32)DSB(rec + 0x51u);
    u16 cmd = DSW(DS_001088E0 + side * 2u);
    int bvar = ((cmd >> 8) & 3u) != 0u && ((cmd >> 8) & 0xCu) != 0u;  /* 0x35EE1 */
    DSB(slot + 0x41u) |= 0x80u;                         /* 0x35ED0 */
    /* PORT: 0x35ED9 0x2C3FC(0x6D) — the character voice, out of scope. */
    if (!bvar && fighter_attack_consume(side) != 0) {   /* 0x35F15 */
        hit_facing_flag(side);                          /* 0x35F21 */
        return;
    }
    DSB(slot + 0x54u) = 0u;                             /* 0x35F2F */
    DSB(slot + 0x52u) = 9u;                             /* 0x35F33 */
    DSB(slot + 0x53u) = 0u;                             /* 0x35F3E */
    DSW(slot + 0x84u) = (u16)(DSW(slot + 0x84u) + 1u);  /* 0x35F43 */
    DSW(slot + 0x92u) = 0u;                             /* 0x35F4F */
    hit_anim_start_a(rec, DSD(FIGHT_35E6C_ANIM
                              + (u32)DSB(slot + 0x7Au) * 4u),
                     0x40000000u);                      /* 0x35F68 */
    hit_anchor_set(side, DSD(rec + 0x18u), 0u);         /* 0x35F79 */
}

/* ---- the 0x3531C/0x350D0 +0x53 machine (fight_hud_pass's 0x35803 call) ----
 * 0x3531C dispatches on slot+0x53 and its default (and case 0/0xd) runs
 * 0x350D0, the core per-frame body. 0x350D0 drives slot+0x52 to 3 through
 * 0x3BDDC (0x3520E) and calls the hit chain 0x3CF38 directly at 0x352A6;
 * 0x3531C case 8 calls it at 0x354BC. Addresses and gates are from
 * docs/superpowers/plans/2026-09-20-demo-fight-derivations.md §3.7, §11.3 and
 * the cycle-2 record §7.8/§7.10. */

/* The chain helpers 0x350D0 shares; defined with the rest of the chain below. */
static void hit_facing_flag(u32 side);                      /* 0x18B04 */

/* PORT: data-object addresses symbols.h does not name. */
#define FIGHT_STUN_BASE  0x00107A80u  /* 0x107A80: 0x40-byte per-side table */
#define FIGHT_D24_BASE   0x00107D24u  /* 0x107D24: per-side word, 0x38BC8 clears */
#define FIGHT_D2C_BASE   0x00107D2Cu  /* 0x107D2C: the per-side round word */
#define FIGHT_ANIM_367DC 0x000C8950u  /* 0xC8950: 0x367DC's per-character anim */
#define FIGHT_ANIM_36BC8 0x000C8A18u  /* 0xC8A18: 0x36BC8's per-character anim */
#define FIGHT_367DC_STREAM 0x000E906Au /* 0xE906A: 0x367DC's second-call stream */
#define FIGHT_36BC8_STREAM 0x000E906Eu /* 0xE906E: 0x36BC8's second-call stream */
#define FIGHT_ROUND_HI   0x001088EAu  /* 0x1088EA: 0x4F944's second word */
#define FIGHT_THR_34DDC  0x000BD870u  /* 0xBD870: 0x34DDC's per-char threshold */
#define FIGHT_THR_350D0  0x000BEF14u  /* 0xBEF14: 0x350D0's per-char threshold */

/* 0x3C59C. Test-and-set bit `bit` of DSD(DS_00107D50 + side*4): 0 the first
 * time (and sets it), 1 when it was already set. fight_slot_clear zeroes the
 * pair each arena frame, so 0x35658's preamble gate is 0 once per side. */
int fighter_pass_flag(u32 bit, u32 side)
{
    u32 m = 1u << (bit & 0x1Fu);
    if ((DSD(DS_00107D50 + side * 4u) & m) != 0u) return 1;   /* 0x3C5B3 */
    DSD(DS_00107D50 + side * 4u) |= m;                        /* 0x3C5C2 */
    return 0;
}

/* 0x39280. Clear the slot's +0x5D and its +0x43 bit 2 (the screen-anchor
 * flag). Called by 0x350D0's +0x41 bit 2 arm and its DS_001078F2 block. */
void fighter_state_39280(u32 side)
{
    u32 slot = DS_001077B0 + side * 0x94u;
    DSB(slot + 0x5Du) = 0;                                  /* 0x3928F */
    DSB(slot + 0x43u) &= 0xFBu;                             /* 0x39296 */
}

/* 0x34DDC. 0 when the slot's +0x30 is below the per-character threshold
 * (word[0xBD870 + char*2]) and the record's +0x36 is negative, else 1. */
int fighter_34ddc(u32 side)
{
    u32 slot = DS_001077B0 + side * 0x94u;
    u32 ch = (u32)DSB(slot + 0x7Au);
    s32 thr = (s32)(s16)DSW(FIGHT_THR_34DDC + ch * 2u);
    if (thr > (s32)DSD(slot + 0x30u)) {                     /* 0x34E05 */
        u32 rec = DSD(slot);                                /* 0x34E09 */
        if ((s16)DSW(rec + 0x36u) < 0) return 0;            /* 0x34E16 */
    }
    return 1;
}

/* 0x34E20. 1 when the reaction byte is below 0x18. */
int fighter_34e20(u32 reaction)
{
    return reaction < 0x18u;
}

/* 0x38BB0. Clear the 0x40-byte per-side table at 0x107A80 + side*0x40. */
static void fighter_38bb0(u32 side)
{
    u32 base = FIGHT_STUN_BASE + side * 0x40u;
    for (u32 i = 0; i < 0x40u; i++) DSB(base + i) = 0;      /* 0x38BBB */
}

/* 0x38BC8. 0x38BB0 plus DSW(0x107D24 + side*2) = 0. */
static void fighter_38bc8(u32 side)
{
    DSW(FIGHT_D24_BASE + side * 2u) = 0;                    /* 0x38BCC */
    fighter_38bb0(side);                                    /* 0x38BDA */
}

/* 0x41310. Add `delta` to the camera-target record's +0x3C (mode 3 excluded);
 * a negative delta that would underflow clamps to 0. */
static void fighter_41310(u32 side, s32 delta)
{
    u32 rec;
    s32 v;
    if (DSW(DS_00104B00) == 3u) return;                     /* 0x41313 */
    rec = DSD(DS_001077A8 + side * 4u);                     /* 0x41317 */
    v = (s32)DSD(rec + 0x3Cu) + delta;                      /* 0x41322 */
    if (delta < 0 && v < 1) {                               /* 0x41328 */
        DSD(rec + 0x3Cu) = 0;                               /* 0x41330 */
        return;
    }
    DSD(rec + 0x3Cu) = (u32)v;                              /* 0x4133C */
}

/* 0x4F944. Clamp to 0x14, then write the HUD counter globals. */
static void fighter_4f944(u32 v)
{
    if (v > 0x14u) v = 0x14u;                               /* 0x4F94A */
    DSB(DS_001088F0) = (u8)v;                               /* 0x4F953 */
    DSW(DS_001088E8) = 0;                                   /* 0x4F95F */
    DSW(FIGHT_ROUND_HI) = 0x10u;                            /* 0x4F969 */
    DSB(DS_00104AE9) |= 8u;                                 /* 0x4F974 */
}

/* 0x367DC. The +0x53 == 7 arm's reset: restart the record's animation at
 * 0xC8950[char], clear the slot's +0x52/+0x53/+0x54/+0x55/+0x5F, zero the
 * record's +0x4C/+0x4D and mask the slot's +0x40; in modes other than
 * 3/0x22/0x24 a second animation on the side's 0x102900 record with the fixed
 * 0xE906A stream (raw 0x36843 EDX = 0xE906A, 0x36848 EAX = 0x102900[side];
 * 0x2BC30 stores EDX to rec+8 at 0x2BC52, so EDX is the stream). */
static void fighter_state_367dc(u32 slot, u32 rec)
{
    actors_anim_begin(rec, DSD(FIGHT_ANIM_367DC
                               + (u32)DSB(slot + 0x7Au) * 4u),
                      0x40400000u);                         /* 0x367F5 */
    DSB(rec + 0x4Cu) = 0;                                   /* 0x367FF */
    DSB(rec + 0x4Du) = 0x1Eu;                               /* 0x36803 */
    DSB(slot + 0x52u) = 0;                                  /* 0x36806 */
    DSB(slot + 0x53u) = 0;                                  /* 0x3680A */
    DSB(slot + 0x5Fu) = 0xFFu;                              /* 0x3680E */
    DSB(slot + 0x55u) = 0xFFu;                              /* 0x36812 */
    DSB(slot + 0x54u) = 0;                                  /* 0x36819 */
    DSD(slot + 0x40u) &= 0xCCF7BFFFu;                       /* 0x3681D */
    if (DSW(DS_00104B00) != 3u && DSW(DS_00104B00) != 0x22u
            && DSW(DS_00104B00) != 0x24u) {
        actors_anim_begin(DSD(DS_00102900 + (u32)DSB(rec + 0x51u) * 4u),
                          FIGHT_367DC_STREAM, 0x3F800000u);   /* 0x36854 */
    }
}

/* 0x36BC8. The +0x43 bit 2 arm: clear this slot's +0x74, run the 0x38BC8/
 * 0x38BB0 stun clear selected by the *other* side's DSW(0x107D2C+side*2) > 0,
 * restart this record's animation at 0xC8A18[char], anchor it, then set
 * slot+0x52 = 7, +0x53 = 2, +0x54 = 0, +0x5D = 0x44 and clear +0x43 bit 2.
 * Returns 7. The 0x36CA9 second animation (0xE906E on the side's 0x102900
 * record) is dead: both callers (0x350D0 0x35162, 0x36870 0x36A3F) require
 * +0x43 bit 2 set, which forces this function's 0x36CA7 early return. */
static int fighter_state_36bc8(u32 slot, u32 rec)
{
    u32 side = (u32)DSB(rec + 0x51u);
    u32 other = 1u - side;
    DSW(slot + 0x74u) = 0;                                  /* 0x36C32 */
    if ((s16)DSW(FIGHT_D2C_BASE + other * 2u) < 1)          /* 0x36C46 */
        fighter_38bc8(other);                               /* 0x36C4E */
    else
        fighter_38bb0(other);                               /* 0x36C58 */
    actors_anim_begin(rec, DSD(FIGHT_ANIM_36BC8
                               + (u32)DSB(slot + 0x7Au) * 4u),
                      0x40800000u);                         /* 0x36C70 */
    hit_anchor_set(side, DSD(rec + 0x18u), 0u);             /* 0x36C82 */
    DSB(slot + 0x5Du) = 0x44u;                              /* 0x36C87 */
    DSB(slot + 0x54u) = 0;                                  /* 0x36C8B */
    DSB(slot + 0x52u) = 7u;                                 /* 0x36C8F */
    DSB(slot + 0x53u) = 2u;                                 /* 0x36C96 */
    if ((DSB(slot + 0x43u) & 4u) != 0u) {                   /* 0x36C9A */
        DSB(slot + 0x43u) &= 0xFBu;                         /* 0x36CA1 */
        return 7;
    }
    if (DSW(DS_00104B00) != 3u && DSW(DS_00104B00) != 0x22u) {
        actors_anim_begin(DSD(DS_00102900 + (u32)DSB(rec + 0x51u) * 4u),
                          FIGHT_36BC8_STREAM, 0x40400000u);   /* 0x36CD1 */
    }
    return 7;                                               /* 0x36CD6 */
}

/* PORT: 0xC9238 (0x37D18's per-character animation stream) has no symbols.h
 * name. */
#define FIGHT_ANIM_37D18  0x000C9238u

/* 0x37D18. The 0x349C8 +0x42 bit-7 arm's callee. Sets the slot's dispatch state
 * to +0x52 = 9, +0x53 = 3, +0x54 = 3 and +0x42 = (+0x42 & 0xDB) | 4, starts the
 * 0xC9238[char] animation at 2.0, writes the +0x74 timer = 0x309 (via 0x39A10),
 * resets the 0x1078DC approach-table pointer to 0xBD89C, then, when the other
 * slot exists, sets the other actor's +0x59 = 0xFF and the other slot's +0x40
 * bits 0x801000. EAX = slot, EDX = rec. */
void fighter_37d18(u32 slot, u32 rec)
{
    DSB(slot + 0x52u) = 9u;                                 /* 0x37D1E */
    DSB(slot + 0x53u) = 3u;                                 /* 0x37D22 */
    DSB(slot + 0x54u) = 3u;                                 /* 0x37D26 */
    DSB(slot + 0x42u) = (u8)((DSB(slot + 0x42u) & 0xDBu) | 0x04u);   /* 0x37D35 */
    actors_anim_begin(rec, DSD(FIGHT_ANIM_37D18                    /* 0x37D4B */
                               + (u32)DSB(slot + 0x7Au) * 4u),
                      0x40800000u);
    fighter_39a10(rec, 0x309u);                             /* 0x37D57 */
    /* PORT: 0x37D5C..0x37D73 0x2C3FC voice, out of scope. The raw loads
     * EDX = 0xBD89C (the voice's second argument) at 0x37D6E; 0x2C3FC preserves
     * EDX, so 0x37D7B stores 0xBD89C to the 0x1078DC approach-table pointer. */
    DSD(DS_001078DC) = DS_000BD89C;                         /* 0x37D7B */
    {
        u32 other = DSD(DS_001077A8                         /* 0x37D88 */
                        + ((u32)DSB(rec + 0x51u) ^ 1u) * 4u);
        if (other != 0u) {                                  /* 0x37D91 */
            DSB(DSD(other) + 0x59u) = 0xFFu;                /* 0x37D95 */
            DSD(other + 0x40u) |= 0x801000u;                /* 0x37D99 */
        }
    }
}

/* 0x39A10. Write `value` (a signed word, truncated) to the +0x74 timer of the
 * slot named by the record's +0x51: word[0x107824 + side*0x94]. EAX = rec,
 * EDX = value. */
void fighter_39a10(u32 rec, u32 value)
{
    u32 side = (u32)DSB(rec + 0x51u);                       /* 0x39A11 */
    DSW(DS_001077B0 + side * 0x94u + 0x74u) = (u16)value;   /* 0x39A28 */
}

/* PORT: data-object addresses with no symbols.h name. */
#define FIGHT_ANIM_36870_1  0x000C89A0u  /* 0x36870 case 1: the 0xC89A0 stream */
#define FIGHT_ANIM_36870_2  0x000C89F0u  /* 0x36870 case 2: the 0xC89F0 stream */
#define FIGHT_ANIM_379C4    0x000C9260u  /* 0x379C4: the 0xC9260 stream */
#define FIGHT_379C4_STREAM  0x001078E4u  /* 0x379C4: the +0x41 bit 2 alt stream */

/* 0x164E8. Zero the per-side dword at 0xFD148 + side*4. */
static void fighter_164e8(u32 side)
{
    DSD(DS_000FD148 + side * 4u) = 0;                       /* 0x164EB */
}

/* 0x385B0. The slot reset the 0x36870 mode-0x25 arm runs: side = rec+0x51,
 * S = 0x1077B0 + side*0x94; clear 0x100AF8[side], rec+0x28 bit 5, S+0x62,
 * S+0x8A, S+0x5F/S+0x55 (0xFF), S+0x67, S+0x65 (0xFF), S+0x74, S+0x0C..+0x1C,
 * bump S+0x84, run 0x164E8, mask S+0x40 to 0xCCF3BFFF, zero rec+0x42; on
 * S+0x54 in {0,1} also clear S+0x68 and rec+0x44/+0x43/+0x42/+0x34/+0x36/+0x1C;
 * unless S+0x54 == 5, clear S+0x54, S+0x41 bit 7 and rec+0x4C, start the
 * 0xC8950[char] animation at 3.0, set rec+0x4D = 0x1E and S+0x52/+0x53 = 0.
 * The raw computes So/rec_o but never reads them. EAX = rec. */
void fighter_385b0(u32 rec)
{
    u32 side = (u32)DSB(rec + 0x51u);                       /* 0x385BA */
    u32 s = DS_001077B0 + side * 0x94u;                     /* 0x385E6 */
    DSD(DS_00100AF8 + side * 4u) = 0;                       /* 0x38625 */
    DSB(rec + 0x28u) &= 0xDFu;                              /* 0x38649 */
    DSB(s + 0x62u) = 0;                                     /* 0x3864C */
    DSB(s + 0x8Au) = 0;                                     /* 0x38657 */
    DSW(s + 0x84u) = (u16)(DSW(s + 0x84u) + 1u);            /* 0x3865F */
    fighter_164e8(side);                                    /* 0x38666 */
    DSD(s + 0x40u) &= 0xCCF3BFFFu;                          /* 0x3866B */
    DSB(rec + 0x42u) = 0;                                   /* 0x38672 */
    DSB(s + 0x5Fu) = 0xFFu;                                 /* 0x38676 */
    DSB(s + 0x55u) = 0xFFu;                                 /* 0x3867A */
    DSD(s + 0x0Cu) = 0;                                     /* 0x3867E */
    DSD(s + 0x10u) = 0;                                     /* 0x38685 */
    DSD(s + 0x18u) = 0;                                     /* 0x3868C */
    DSD(s + 0x1Cu) = 0;                                     /* 0x38693 */
    DSB(s + 0x67u) = 0;                                     /* 0x3869A */
    DSB(s + 0x65u) = 0xFFu;                                 /* 0x3869E */
    DSW(s + 0x74u) = 0;                                     /* 0x386A5 */
    if (DSB(s + 0x54u) == 0u || DSB(s + 0x54u) == 1u) {     /* 0x386AB */
        DSB(s + 0x68u) = 0;                                 /* 0x386B4 */
        DSW(rec + 0x44u) = 0;                               /* 0x386B8 */
        DSB(rec + 0x43u) = 0;                               /* 0x386BE */
        DSB(rec + 0x42u) = 0;                               /* 0x386C2 */
        DSW(rec + 0x34u) = 0;                               /* 0x386C6 */
        DSW(rec + 0x36u) = 0;                               /* 0x386CC */
        DSD(rec + 0x1Cu) = 0;                               /* 0x386D2 */
    }
    if (DSB(s + 0x54u) != 5u) {                             /* 0x386D9 */
        DSB(s + 0x54u) = 0;                                 /* 0x386E7 */
        DSB(s + 0x41u) &= 0x7Fu;                            /* 0x386EE */
        DSB(rec + 0x4Cu) = 0;                               /* 0x386F3 */
        actors_anim_begin(rec, DSD(FIGHT_ANIM_367DC        /* 0x38708 */
                                   + (u32)DSB(s + 0x7Au) * 4u),
                          0x40400000u);
        DSB(rec + 0x4Du) = 0x1Eu;                           /* 0x38712 */
        DSB(s + 0x52u) = 0;                                 /* 0x38715 */
        DSB(s + 0x53u) = 0;                                 /* 0x38719 */
    } else {
        /* PORT: 0x38154 — the +0x54 == 5 arm is a named gap (§7.10). */
    }
}

/* 0x379C4. The 0x36870 +0x54 == 4 arm. When 0x1078FE is clear, re-enter
 * 0x37178 iff +0x57 == 2. Else when +0x41 bit 2 is set: when 0x1078E8 is
 * non-null run that callback (EAX = slot, EDX = rec) and return if it returns
 * non-zero; otherwise set 0x1078FC = 1 and start the 0xC9260[char] animation at
 * 2.0 (or the 0x1078E4 stream when 0x1078E8 is null). Else start the
 * 0xC9260[char] animation at 2.0. EAX = slot. */
static void fighter_379c4(u32 slot)
{
    u32 rec = DSD(slot);
    if (DSB(DS_001078FE) == 0u) {                           /* 0x379C8 */
        if (DSB(slot + 0x57u) == 2u) fighter_37178(slot);   /* 0x37A4F */
        return;
    }
    if ((DSB(slot + 0x41u) & 2u) != 0u) {                   /* 0x379D5 */
        if (DSD(DS_001078E8) != 0u) {                       /* 0x379DB */
            /* PORT: 0x379E8 the indirect CALL [0x1078E8] (EAX = slot, EDX =
             * rec). The raw tests only [0x1078E8] != 0 and calls it; the extra
             * `cb != 0` is the port-level guard the raw has no need of — the
             * raw's pointer IS the target, while fn_resolve returns NULL for a
             * target with no C registration. 0x1078E8 is set only by the
             * unported pose chain, so it is 0 here and the arm is dead. */
            int (*cb)(u32, u32) =
                (int (*)(u32, u32))(void *)fn_resolve(DSD(DS_001078E8));
            if (cb != 0 && cb(slot, rec) != 0) return;      /* 0x379F0 */
            DSB(DS_001078FC) = 1u;                          /* 0x379F7 */
            actors_anim_begin(rec, DSD(FIGHT_ANIM_379C4     /* 0x37A0B */
                                       + (u32)DSB(slot + 0x7Au) * 4u),
                              0x40400000u);
            return;
        }
        actors_anim_begin(rec, DSD(FIGHT_379C4_STREAM),     /* 0x37A20 */
                          0x40400000u);
        return;
    }
    actors_anim_begin(rec, DSD(FIGHT_ANIM_379C4             /* 0x37A3B */
                               + (u32)DSB(slot + 0x7Au) * 4u),
                      0x40400000u);
}

/* 0x36870. The +0x54 machine 0x37178/0x379C4 (and the unported 0x3FD30) call.
 * Under mode 0x25 it runs 0x385B0 and returns. Else it resets the pair (the
 * 0x100CE0 word, S+0x90, S+0x40's 0x4000000/0x1000000 mask + 0x39280, the
 * 0x100AF8 entry, rec_s+0x28 bit 5, S+0x62/+0x8A, the S+0x84 counter, 0x164E8,
 * S+0x40's 0xCCF3BFFF mask, rec_s+0x42, 0x39040(other), S+0x5F/+0x55/+0x67/
 * +0x65/+0x74/+0x0C..+0x1C, So+0x65/+0x66) and switches on S+0x54:
 *   0: mask S+0x40 to 0x7F7F; on S+0x42 bit 5 run 0x37D18; else 0x365C8 ->
 *      S+0x43 bit 0x40, then S+0x42 bit 4 / S+0x43 bit 2 -> 0x36BC8, then
 *      0x36638 -> (on 0) restart rec_o's animation, rec_o+0x4D = 0x1E, S+0x52/
 *      +0x53 = 0; in modes other than 3/0x22/0x24 also restart the side's
 *      0x102900 record with the 0xE906A stream at 1.0;
 *   1: mask S+0x40/S+0x41 to 0x7F, S+0x68 = 0, 0x365C8 -> S+0x43 bit 0x40,
 *      0x36638 -> (on 0) S+0x52 = 5, S+0x53 = 0, rec_s+0x4D = 0x14,
 *      rec_s+0x4C = 0, the 0xC89A0[char] animation at 2.0;
 *   2: S+0x52 = 4, S+0x53 = 0, 0x3C520(rec_s, 0xC89F0[char], 2.0);
 *   3: nothing; 4: 0x379C4(S). EAX = rec. */
void fighter_36870(u32 rec)
{
    u32 side, other, s, so, rec_s, rec_o;
    if (DSW(DS_00104B00) == 0x25u) {                        /* 0x3687F */
        fighter_385b0(rec);                                 /* 0x36884 */
        return;
    }
    side = (u32)DSB(rec + 0x51u);                           /* 0x3688E */
    other = 1u - side;                                      /* 0x368A1 */
    s = DS_001077B0 + side * 0x94u;                         /* 0x368BD */
    so = DS_001077B0 + other * 0x94u;                       /* 0x368DD */
    rec_s = DSD(s);                                         /* 0x368E3 */
    rec_o = DSD(so);                                        /* 0x368E9 */
    DSW(DS_00100CE0 + other * 2u) = 0;                      /* 0x368F9 */
    DSB(s + 0x90u) = 0;                                     /* 0x36908 */
    if ((DSB(s + 0x41u) & 4u) != 0u) {                      /* 0x3690F */
        DSD(s + 0x40u) &= 0xFBFFFBFFu;                      /* 0x36914 */
        fighter_state_39280(side);                          /* 0x3691E */
    }
    DSD(DS_00100AF8 + side * 4u) = 0;                       /* 0x36928 */
    DSB(rec_s + 0x28u) &= 0xDFu;                            /* 0x36933 */
    DSB(s + 0x62u) = 0;                                     /* 0x3693B */
    DSW(s + 0x84u) = (u16)(DSW(s + 0x84u) + 1u);            /* 0x3694E */
    DSB(s + 0x8Au) = 0;                                     /* 0x36946 */
    fighter_164e8(side);                                    /* 0x36958 */
    DSD(s + 0x40u) &= 0xCCF3BFFFu;                          /* 0x36961 */
    DSB(rec_s + 0x42u) = 0;                                 /* 0x3696C */
    fighter_39040(other);                                   /* 0x36974 */
    DSB(s + 0x5Fu) = 0xFFu;                                 /* 0x3697D */
    DSB(s + 0x55u) = 0xFFu;                                 /* 0x36981 */
    DSB(s + 0x67u) = 0;                                     /* 0x36985 */
    DSB(s + 0x65u) = 0xFFu;                                 /* 0x36989 */
    DSW(s + 0x74u) = 0;                                     /* 0x3698D */
    DSD(s + 0x0Cu) = 0;                                     /* 0x36993 */
    DSD(s + 0x10u) = 0;                                     /* 0x36996 */
    DSD(s + 0x18u) = 0;                                     /* 0x36999 */
    DSD(s + 0x1Cu) = 0;                                     /* 0x3699C */
    DSB(so + 0x65u) = 0xFFu;                                /* 0x369A3 */
    DSB(so + 0x66u) = 0;                                    /* 0x369A7 */
    if (DSB(s + 0x54u) == 0u || DSB(s + 0x54u) == 1u) {     /* 0x369AF */
        DSB(s + 0x68u) = 0;                                 /* 0x369BF */
        DSW(rec_s + 0x44u) = 0;                             /* 0x369C7 */
        DSB(rec_s + 0x43u) = 0;                             /* 0x369CD */
        DSB(rec_s + 0x42u) = 0;                             /* 0x369D1 */
        DSW(rec_s + 0x34u) = 0;                             /* 0x369D5 */
        DSW(rec_s + 0x36u) = 0;                             /* 0x369DB */
        DSD(rec_s + 0x1Cu) = 0;                             /* 0x369E1 */
    }
    switch (DSB(s + 0x54u)) {                               /* 0x369FC 0x3685C */
    case 0u:                                                /* 0x36A04 */
        DSW(s + 0x40u) &= 0x7F7Fu;                          /* 0x36A08 */
        DSB(rec_s + 0x4Cu) = 0;                             /* 0x36A12 */
        if ((DSB(s + 0x42u) & 0x20u) != 0u) {               /* 0x36A1A */
            fighter_37d18(s, rec_s);                        /* 0x36A24 */
            return;
        }
        if (fighter_state_365c8(s, rec_s, side) != 0)       /* 0x36A37 */
            DSB(s + 0x43u) |= 0x40u;                        /* 0x36A44 */
        else
            DSB(s + 0x43u) &= 0xBFu;                        /* 0x36A4E */
        if ((DSB(s + 0x42u) & 0x10u) == 0u                 /* 0x36A5A */
                && (DSB(s + 0x43u) & 4u) != 0u) {
            (void)fighter_state_36bc8(s, rec_s);            /* 0x36A66 */
            return;
        }
        if (fighter_state_36638(s, rec_s) == 0) {           /* 0x36A7A */
            actors_anim_begin(rec_o, DSD(FIGHT_ANIM_367DC /* 0x36A9C */
                                       + (u32)DSB(s + 0x7Au) * 4u),
                              0x40400000u);
            DSB(rec_o + 0x4Du) = 0x1Eu;                     /* 0x36AAA */
            DSB(s + 0x52u) = 0;                             /* 0x36AB1 */
            DSB(s + 0x53u) = 0;                             /* 0x36AB5 */
        }
        if (DSW(DS_00104B00) == 3u || DSW(DS_00104B00) == 0x22u
                || DSW(DS_00104B00) == 0x24u)               /* 0x36AC1 */
            return;
        actors_anim_begin(DSD(DS_00102900                    /* 0x36AF6 */
                              + (u32)DSB(rec_s + 0x51u) * 4u),
                          FIGHT_367DC_STREAM, 0x3F800000u);
        return;
    case 1u:                                                /* 0x36B02 */
        DSB(s + 0x40u) &= 0x7Fu;                            /* 0x36B13 */
        if (fighter_state_365c8(s, rec_s, side) != 0)       /* 0x36B16 */
            DSB(s + 0x43u) |= 0x40u;
        else
            DSB(s + 0x43u) &= 0xBFu;
        DSB(s + 0x41u) &= 0x7Fu;                            /* 0x36B35 */
        DSB(s + 0x68u) = 0;                                 /* 0x36B3D */
        if (fighter_state_36638(s, rec_s) != 0)             /* 0x36B41 */
            return;
        DSB(s + 0x52u) = 5u;                                /* 0x36B4E */
        DSB(s + 0x53u) = 0;                                 /* 0x36B56 */
        DSB(rec_s + 0x4Du) = 0x14u;                         /* 0x36B5F */
        DSB(rec_s + 0x4Cu) = 0;                             /* 0x36B66 */
        actors_anim_begin(rec_s, DSD(FIGHT_ANIM_36870_1     /* 0x36B7F */
                                     + (u32)DSB(s + 0x7Au) * 4u),
                          0x40400000u);
        return;
    case 2u:                                                /* 0x36B8B */
        DSB(s + 0x52u) = 4u;                                /* 0x36B91 */
        DSB(s + 0x53u) = 0;                                 /* 0x36B98 */
        hit_anim_start_c(rec_s, DSD(FIGHT_ANIM_36870_2      /* 0x36BAC */
                                    + (u32)DSB(s + 0x7Au) * 4u),
                         0x40000000u);
        return;
    case 3u:                                                /* 0x36BC1 */
        return;
    case 4u:                                                /* 0x36BB8 */
        fighter_379c4(s);                                   /* 0x36BBC */
        return;
    default:
        return;
    }
}

/* 0x37178. The 0x349C8 +0x42 bit-6 arm. Clears 0x1078FE; sets S+0x54 = 4,
 * S+0x40 = (S+0x40 & 0xFFBFFFBF) | 0x40 and So+0x42 |= 4; reads the approach
 * base = (s16)word[DSD(0x1078DC) + S[0x7A]*14 + So[0x7A]*2]. On base == -1 it
 * sets S+0x52 = 9, self-assigns rec_s+0x18, sets 0x1078FE = 1 and runs 0x36870.
 * Else X = rec_o+0x18 - base (when 0x1A570(other) != 0) or + base; diff =
 * rec_s+0x18 - X (all 32-bit); when the two records' +0x28 bit 0x4000 differ,
 * |diff| <= 0x400 sets S+0x52 = 9, rec_s+0x18 = X, 0x1078FE = 1 and runs
 * 0x36870; the far arm sets S+0x43 bit 0x40 and runs 0x36638, or 0x35838 with
 * dirbits 0x1000/0x2000 (S+0x57 = 0) when 0x1A570(side) selects it. When the
 * facing bits agree, |diff| <= 0x400 sets S+0x43 bit 0x40 and runs 0x36638;
 * the far arm mirrors with S+0x57 = 1. EAX = slot. */
void fighter_37178(u32 slot)
{
    u32 rec = DSD(slot);                                    /* 0x37180 */
    u32 side = (u32)DSB(rec + 0x51u);                       /* 0x37188 */
    u32 other = 1u - side;                                  /* 0x37195 */
    u32 s = DS_001077B0 + side * 0x94u;                     /* 0x371B2 */
    u32 so = DS_001077B0 + other * 0x94u;                   /* 0x371D2 */
    u32 rec_s = DSD(s);                                     /* 0x371D8 */
    u32 rec_o = DSD(so);                                    /* 0x371DE */
    s32 base;
    s32 x;
    s32 diff;
    int facing_s;
    int facing_o;
    DSB(DS_001078FE) = 0;                                   /* 0x37182 */
    DSD(s + 0x40u) &= 0xFFBFFFBFu;                          /* 0x371EC */
    DSB(s + 0x54u) = 4u;                                    /* 0x371F6 */
    DSB(s + 0x40u) |= 0x40u;                                /* 0x371FD */
    DSB(so + 0x42u) |= 4u;                                  /* 0x37204 */
    base = (s16)DSW(DSD(DS_001078DC)                       /* 0x3722D */
                    + (u32)DSB(s + 0x7Au) * 14u
                    + (u32)DSB(so + 0x7Au) * 2u);
    if (base == -1) {                                       /* 0x37231 */
        DSB(s + 0x52u) = 9u;                                /* 0x37241 */
        DSD(rec_s + 0x18u) = DSD(rec_s + 0x18u);            /* 0x3724B self */
        DSB(DS_001078FE) = 1u;                              /* 0x37250 */
        fighter_36870(rec_s);                               /* 0x37256 */
        return;
    }
    if (fighter_actor_bit15_clear(other) != 0)              /* 0x37264 */
        x = (s32)DSD(rec_o + 0x18u) - base;                 /* 0x37299 */
    else
        x = (s32)DSD(rec_o + 0x18u) + base;                 /* 0x372C8 */
    diff = (s32)DSD(rec_s + 0x18u) - x;                     /* 0x372DC */
    facing_s = (int)((DSW(rec_s + 0x28u) >> 8) & 0x40u);    /* 0x372D3 */
    facing_o = (int)((DSW(rec_o + 0x28u) >> 8) & 0x40u);    /* 0x372E5 */
    if (facing_s != facing_o) {                             /* 0x372FA */
        if (diff >= -0x400 && diff <= 0x400) {              /* 0x3730E */
            DSB(s + 0x52u) = 9u;                            /* 0x3731A */
            DSD(rec_s + 0x18u) = (u32)x;                    /* 0x37324 */
            DSB(DS_001078FE) = 1u;                          /* 0x37329 */
            fighter_36870(rec_s);                           /* 0x3732F */
            return;
        }
        if (fighter_actor_bit15_clear(side) != 0) {         /* 0x3733E */
            if (diff > 0) {                                 /* 0x37349 */
                fighter_state_35838(s, rec_s, 0x2000u);     /* 0x37358 */
                DSB(s + 0x57u) = 0;                         /* 0x37361 */
                return;
            }
            DSB(s + 0x43u) |= 0x40u;                        /* 0x37370 */
            (void)fighter_state_36638(s, rec_s);            /* 0x37456 */
            return;
        }
        if (diff < 0) {                                     /* 0x3737F */
            fighter_state_35838(s, rec_s, 0x1000u);         /* 0x3738E */
            DSB(s + 0x57u) = 0;                             /* 0x37397 */
            return;
        }
        DSB(s + 0x43u) |= 0x40u;                            /* 0x3736C */
        (void)fighter_state_36638(s, rec_s);                /* 0x37456 */
        return;
    }
    if (diff >= -0x400 && diff <= 0x400) {                  /* 0x373AE */
        DSB(s + 0x43u) |= 0x40u;                            /* 0x373BF */
        (void)fighter_state_36638(s, rec_s);                /* 0x373C6 */
        return;
    }
    if (fighter_actor_bit15_clear(side) != 0) {             /* 0x373D5 */
        if (diff > 0) {                                     /* 0x373E0 */
            fighter_state_35838(s, rec_s, 0x2000u);         /* 0x373EF */
            DSB(s + 0x57u) = 1;                             /* 0x373F8 */
            return;
        }
        DSB(s + 0x43u) |= 0x40u;                            /* 0x3740A */
        (void)fighter_state_36638(s, rec_s);                /* 0x37414 */
        return;
    }
    if (diff < 0) {                                         /* 0x37420 */
        fighter_state_35838(s, rec_s, 0x1000u);             /* 0x37431 */
        DSB(s + 0x57u) = 1;                                 /* 0x3743A */
        return;
    }
    DSB(s + 0x43u) |= 0x40u;                                /* 0x37449 */
    (void)fighter_state_36638(s, rec_s);                    /* 0x37456 */
}

/* 0x39040. The per-side round/timer pass. Gated on DSW(0x107D2C + side*2) > 1;
 * the body updates the round resource byte, slot+0x81's band, the 0x107D18
 * timer, the 0x41310/0x4F944 counters and (in the non-0x63 arm) the 0x1088BF
 * roll, and draws the round text through 0x38D90/0x38FEC (the 0x2F4D0/0x2EFD4
 * text-grid formatter is a declared gap, record §7.8 / frontend §7.2). The tail
 * always clears DSW(0x107D2C/0x107D20/0x107D24 + side*2) and the 0x40-byte
 * table at 0x107A80 + side*0x40. */
void fighter_39040(u32 side)
{
    u32 slot = DS_001077B0 + side * 0x94u;
    if ((s16)DSW(FIGHT_D2C_BASE + side * 2u) > 1) {         /* 0x39056 */
        u32 q = DSD(slot + 0x3Cu) / DSD(DS_000C9520);       /* 0x3908C */
        DSB(DS_001088B6 + side) = (u8)DSB(FIGHT_D2C_BASE + side * 2u); /* 0x3907B */
        {
            s32 lim = (s32)(u32)DSB(slot + 0x81u)
                    - (s32)(u32)DSB(DS_001088CC);           /* 0x390A2 */
            if ((s32)q > lim) {
                u32 cap = (u32)DSB(DS_001088CB);
                if (q >= cap) q = cap;                      /* 0x390B4 */
                DSB(slot + 0x81u) =
                    (u8)((u32)DSB(DS_001088CC) + (u8)q);    /* 0x390CC */
            }
        }
        DSW(DS_00107D18 + side * 2u) = 0xB4u;               /* 0x390E1 */
        /* PORT: 0x390E8 0x38D90(side) and 0x390EF 0x38FEC(side) are the
         * round-text draws; 0x38D90 reaches the unmodelled 0x2F4D0/0x2EFD4
         * formatter (named gap, record §7.8). */
        DSW(DS_00107D1C + side * 2u) = DSW(DS_00107D20 + side * 2u); /* 0x390FB */
        DSB(slot + 0x7Bu) = (u8)(DSB(slot + 0x7Bu) + 1u);   /* 0x39110 */
        if (DSB(slot + 0x63u) == 0u) {                      /* 0x39117 */
            s32 round = (s32)(s16)DSW(FIGHT_D2C_BASE + side * 2u);
            if (round == 2) {
                fighter_41310(side, 0x3E8);                 /* 0x3913A */
            } else if (round >= 3) {
                fighter_41310(side, round <= 0xC
                              ? (s32)(u32)((round - 2) * 0x7D0)
                              : 0x4E20);                    /* 0x3916D */
                {
                    s32 a = (s32)(s16)DSW(DS_00107D20 + side * 2u);
                    s32 r2 = (s32)(s16)DSW(FIGHT_D2C_BASE + side * 2u);
                    if (a >= 0x23 && r2 >= 4) {             /* 0x39190 */
                        fighter_4f944((u32)r2);             /* 0x39195 */
                        if (a >= 0x41 && r2 >= 5)           /* 0x391B8 */
                            DSB(DS_001088BF) =
                                (u8)(rng_next(2u) + 1u);    /* 0x391CA */
                    } else if (a <= 0x23) {                 /* 0x391DE */
                        u32 r3 = (u32)DSB(DS_001088A8 + side);
                        /* PORT: 0x3923D 0x2C3FC(code) is the character voice,
                         * out of scope; the rng draws stay. */
                        if (r3 >= 0x20u && r3 <= 0x3Fu)
                            (void)rng_next(3u);             /* 0x391FE */
                        else
                            (void)rng_next(2u);             /* 0x39228 */
                    }
                }
            }
        }
    }
    DSW(FIGHT_D2C_BASE + side * 2u) = 0;                    /* 0x39244 */
    DSW(DS_00107D20 + side * 2u) = 0;                       /* 0x3924C */
    DSW(FIGHT_D24_BASE + side * 2u) = 0;                    /* 0x39254 */
    fighter_38bb0(side);                                    /* 0x3925C */
}

/* 0x1DE64. The reaction picker 0x350D0 calls after a miss. With the slot's
 * +0x63 clear it scans 0x46460/0x4649C for a live input (a named gap: those
 * two are the unported input scanner, §7.12); otherwise the result is the
 * side's command word. The command's bits then map to a reaction code, the
 * 0x10/0x11/0x14/0x15 codes selected by 0x1DDF4's geometry test. Returns 0xFF
 * when nothing maps. */
u32 hit_reaction_pick(u32 side, u32 stance)
{
    u32 ctx[6];
    u32 slot;
    u32 r = 0xFFu;
    fighter_ctx_same(ctx, side);                            /* 0x1DE76 */
    slot = ctx[2];                                          /* &slot[side] */
    {
        s32 n = ((s32)DSD(slot + 0x90u) >> 16 < 0xF) ? 5 : 0xF;   /* 0x1DEA1 */
        if (DSB(slot + 0x63u) == 0u) {                      /* 0x1DEB8 */
            /* PORT: 0x1DEDC 0x46460/0x4649C — the input scanner, a named gap
             * (§7.12). The demo's slot+0x63 is 1, so this arm is unreachable. */
            (void)n;
        } else {
            r = (u32)DSW(DS_001088E0 + side * 2u);          /* 0x1DEC2 */
        }
    }
    if (r != 0xFFu) {                                       /* 0x1DF5C */
        s32 sel = (side == 0u)
                ? (s32)(s16)DSW(DSD(DS_00101514) + 0x2D4u)  /* 0x1DF6B */
                : (s32)(s16)DSW(DSD(DS_00101514) + 0x2D6u); /* 0x1DF79 */
        u32 b4 = r & 0xCu;                                  /* 0x1DFB7 */
        u32 b3 = r & 3u;                                    /* 0x1DFBD */
        u32 ch = (u32)DSB(slot + 0x7Au);
        if (stance == 2u) {                                 /* 0x1DFC4 */
            if ((r & 1u) != 0u) return 0xCu;                /* 0x1DFD4 */
            if ((r & 2u) != 0u) return 0xDu;                /* 0x1DFE4 */
            if ((r & 4u) != 0u) return 0xEu;                /* 0x1DFF7 */
            if ((r & 8u) != 0u) return 0xFu;                /* 0x1E00A */
            if (sel == 4) {                                 /* 0x1E015 */
                if (b3 == 3u) return 0x16u;                 /* 0x1E028 */
                if (b4 == 0xCu) return 0x11u;               /* 0x1E041 */
            }
        } else if ((r & 0x4000u) == 0u) {                   /* 0x1E052 */
            if (sel == 4) {                                 /* 0x1E0E5 */
                if (b3 == 3u)                               /* 0x1E0F6 */
                    return hit_geometry(1u - side,
                        DSD(0x000A7AF4u + ch * 4u), 0x000A7A70u)
                        ? 0x12u : 0x10u;                    /* 0x1E11D */
                if (b4 == 0xCu)                             /* 0x1E13B */
                    return hit_geometry(1u - side,
                        DSD(0x000A7B1Cu + ch * 4u), 0x000A7A70u)
                        ? 0x13u : 0x11u;                    /* 0x1E166 */
            }
            if ((r & 1u) != 0u)                             /* 0x1E17E */
                return hit_geometry(1u - side,
                    DSD(0x000A7AF4u + ch * 4u), 0x000A7A70u)
                    ? 4u : 0u;                              /* 0x1E1A9 */
            if ((r & 2u) != 0u)                             /* 0x1E1C1 */
                return hit_geometry(1u - side,
                    DSD(0x000A7B1Cu + ch * 4u), 0x000A7A70u)
                    ? 5u : 1u;                              /* 0x1E1E8 */
            if ((r & 4u) != 0u)                             /* 0x1E20B */
                return hit_geometry(1u - side,
                    DSD(0x000A7A7Cu + ch * 4u), 0x000A7A70u)
                    ? 6u : 2u;                              /* 0x1E22F */
            if ((r & 8u) != 0u)                             /* 0x1E252 */
                return hit_geometry(1u - side,
                    DSD(0x000A7AA4u + ch * 4u), 0x000A7A70u)
                    ? 7u : 3u;                              /* 0x1E279 */
        } else {                                            /* 0x1E058 */
            if ((r & 1u) != 0u) return 8u;                  /* 0x1E069 */
            if ((r & 2u) != 0u) return 9u;                  /* 0x1E07D */
            if ((r & 4u) != 0u) return 0xAu;                /* 0x1E090 */
            if ((r & 8u) != 0u) return 0xBu;                /* 0x1E0A3 */
            if (sel == 4) {                                 /* 0x1E0AE */
                if (b3 == 3u) return 0x14u;                 /* 0x1E0C1 */
                if (b4 == 0xCu) return 0x15u;               /* 0x1E0DA */
            }
        }
    }
    return 0xFFu;                                           /* 0x1E28F */
}

/* 0x350D0. The core per-frame body 0x3531C's default runs. It drives the
 * +0x52/+0x53/+0x54 state (0x3BDDC at 0x3520E), calls the 0x3CF38 hit chain
 * directly at 0x352A6, and on a miss picks a reaction with 0x1DE64 and applies
 * it through 0x34E2C. */
void fighter_state_350d0(u32 side)
{
    u32 ctx[6];
    u32 slot, rec;
    fighter_ctx_same(ctx, side);                            /* 0x3513B */
    slot = ctx[2];
    rec = ctx[4];
    if (DSD(slot + 0x14u) != 0u) {                          /* 0x35144 */
        /* PORT: 0x3514C. The raw calls the slot+0x14 callback and zeroes the
         * field when it returns non-zero; the port's fn_resolve callbacks are
         * void and no ported writer registers one (the spawn zeroes it), so the
         * field is left as-is. */
        void (*fn)(void) = fn_resolve(DSD(slot + 0x14u));
        if (fn) fn();
    }
    if ((DSB(slot + 0x43u) & 4u) != 0u
            && DSB(slot + 0x54u) != 2u) {                   /* 0x3515E */
        (void)fighter_state_36bc8(slot, rec);               /* 0x35172 */
        return;
    }
    if ((DSB(slot + 0x41u) & 4u) != 0u) {                   /* 0x35180 */
        DSD(slot + 0x40u) &= 0xFBFFFBFFu;                   /* 0x35186 */
        fighter_state_39280(side);                          /* 0x35190 */
    }
    if ((s16)DSW(slot + 0x78u) > 0) return;                 /* 0x35199 */
    if (DSB(slot + 0x54u) == 2u) {                          /* 0x351A4 */
        s32 t = (s32)(s16)DSW(FIGHT_THR_350D0
                              + (u32)DSB(slot + 0x7Au) * 2u);
        if (t > (s32)DSD(slot + 0x30u)
                && (s16)DSW(rec + 0x36u) < 0) {             /* 0x351C2 */
            DSB(slot + 0x53u) = 0xCu;                       /* 0x351CF */
            return;
        }
    } else {                                                /* 0x351DB */
        u16 cmd = DSW(DS_001088E0 + side * 2u);
        int bvar2 = ((cmd >> 8) & 3u) != 0u
                 && ((cmd >> 8) & 0xCu) != 0u;              /* 0x35201 */
        if (!bvar2 && fighter_attack_consume(side) != 0) {  /* 0x3520E */
            hit_facing_flag(side);                          /* 0x3521A */
            return;
        }
    }
    {
        u32 s = DSD(DS_001077A8 + side * 4u);               /* 0x35227 */
        if (s == 0u || DSD(s) == 0u) return;                /* 0x3523A */
        fighter_39040(1u - side);                           /* 0x35244 */
        if (DSB(DS_001078F2 + side) != 0u) {                /* 0x3524C */
            DSB(DS_001078F2 + side) = 0;                    /* 0x35257 */
            fighter_state_39280(side);                      /* 0x3525D */
        }
        hit_stance_timer(side);                             /* 0x35265 */
        DSW(slot + 0x74u) = 0;                              /* 0x3526E */
        if ((DSB(s + 0x42u) & 0x90u) != 0u) {               /* 0x35274 */
            DSB(s + 0x53u) = 3u;                            /* 0x3527A */
            return;
        }
        if (DSB(s + 0x54u) == 2u) {                         /* 0x35286 */
            if (fighter_34ddc(side) == 0) return;           /* 0x3528E */
            if (DSB(DS_001078F8 + side) == 0u) return;      /* 0x35297 */
        }
        DSB(s + 0x56u) = 0;                                 /* 0x352A2 */
        if (hit_chain_resolve(side) != 0) return;           /* 0x352A6 */
        {
            u32 r = hit_reaction_pick(side, (u32)DSB(s + 0x54u)); /* 0x352BA */
            if (r == 0xFFu) return;                         /* 0x352C9 */
            hit_reaction_apply(side, r);                    /* 0x352CD */
        }
    }
}

/* 0x3531C. The +0x53 dispatcher fight_hud_pass calls at 0x35803. Cases 0, 0xd
 * and >0xf run 0x350D0; 4 increments +0x56; 7 runs the +0x41 bit 7 / +0xC
 * callback and, for char 4 with a live +0x5F and +0x86 > 0x5A, 0x367DC; 8 runs
 * the 0xBDBE8/+0x88 gate, 0x34E20(+0x5F), clears +0x53 and calls the 0x3CF38
 * chain directly at 0x354BC; 10 runs the +0x10 callback. */
void fighter_state_3531c(u32 side)
{
    u32 slot = DSD(DS_001077A8 + side * 4u);                /* 0x35383 */
    if (slot == 0u || DSD(slot) == 0u) return;              /* 0x3539A */
    if ((s16)DSW(DS_001078F6) != 0) {                       /* 0x353A7 */
        DSW(DS_001078F6) = (u16)(DSW(DS_001078F6) - 1u);    /* 0x353AF */
        if ((s16)DSW(DS_001078F6) < 1) {
            /* PORT: 0x353C0/0x353CA 0x2C3FC(0xEC/0xE0) and 0x353DD 0x2B150 are
             * the voice/cutscene pair; 0x36F10 (their only writer of
             * DS_001078F6) is unreachable, so this arm is inert. */
            DSD(slot + 0x40u) |= 0x801000u;                 /* 0x353E2 */
        }
    }
    switch (DSB(slot + 0x53u)) {                            /* 0x353E9 table 0x352DC */
    case 4u:
        DSB(slot + 0x56u) = (u8)(DSB(slot + 0x56u) + 1u);   /* 0x3540E */
        return;
    case 7u:
        DSB(slot + 0x41u) |= 0x80u;                         /* 0x3541B */
        DSB(slot + 0x43u) &= 0xCFu;                         /* 0x35422 */
        {
            void (*fn)(void) = fn_resolve(DSD(slot + 0xCu));
            if (fn) fn();                                   /* 0x35431 */
        }
        if (DSB(slot + 0x7Au) == 4u                         /* 0x3543C */
                && (DSB(slot + 0x5Fu) == 0x21u
                    || DSB(slot + 0x5Fu) == 0x22u)
                && (s32)DSD(slot + 0x86u) >> 16 > 0x5A) {
            fighter_state_367dc(slot, DSD(slot));           /* 0x3546B */
            return;
        }
        return;
    case 8u:
        DSB(slot + 0x56u) = (u8)(DSB(slot + 0x56u) + 1u);   /* 0x3547A */
        DSB(slot + 0x43u) &= 0xCFu;                         /* 0x35487 */
        if ((s16)DSW(DS_000BDBE8) <= (s16)DSW(slot + 0x88u))  /* 0x35498 */
            return;
        if (fighter_34e20((u32)DSB(slot + 0x5Fu)) == 0)     /* 0x354A8 */
            return;
        {
            u32 r = (u32)DSB(slot + 0x5Fu);
            DSB(slot + 0x53u) = 0;                          /* 0x354B5 */
            if (hit_chain_resolve(side) != 0) return;       /* 0x354BC */
            DSB(slot + 0x53u) = 8u;                         /* 0x354C9 */
            DSB(slot + 0x5Fu) = (u8)r;                      /* 0x354CD */
        }
        return;
    case 10u:
        {
            void (*fn)(void) = fn_resolve(DSD(slot + 0x10u));
            if (fn) fn();                                   /* 0x354E2 */
        }
        return;
    case 1u: case 2u: case 3u: case 5u: case 6u:
    case 9u: case 11u: case 12u: case 14u: case 15u:
        return;                                             /* 0x354E5 */
    default:                                                /* 0, 13, >15 */
        fighter_state_350d0(side);                          /* 0x353FF */
        return;
    }
}

/* 0x3BDDC. The attack/command consumer. When the slot's +0x40 bit 7 is clear
 * and the side's command word has bit 15 set, it clears the record's
 * +0x34/+0x43/+0x42, sets the slot's +0x5F to 0xFF, and (unless 0x3CF38
 * returns non-zero) writes the attack state: the 0xBEF28/0xBEF64 table at
 * DS_00107D40 + side*4, the slot's +0x52/+0x53/+0x54, DS_001078F8+side and the
 * slot's +0x4E from the command's 0x1000/0x2000 bits. Returns 1 on the attack
 * transition, 0 when either the slot's +0x40 bit 7 or the command's bit 15
 * rejects. Its 0x3CF38 gate and 0x3C480 continuation are named gaps (§7.6/§7.16). */
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
        if (hit_chain_resolve(side) != 0) return 1;     /* 0x3BE61 0x3CF38 */
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

/* ---- the 0x3C88C hitbox machine and the 0x3CF38 hit chain ----------------
 * 0x3C88C is the per-slot attack-frame state machine that arms the hitboxes;
 * 0x3CF38 scans them, validates the hit against the target's stance and
 * hit-stun, drives the reaction and consumes the hitbox. Addresses, table
 * layouts, gates and the pinned fixtures are from
 * docs/superpowers/plans/2026-09-21-demo-fight-closure-derivations.md
 * §2, §3, §7.1-§7.5, §7.7-§7.9 and §7.11. */

/* Data-object addresses the generator emits no symbols.h name for. */
#define HIT_TABLE_PLAYER 0x000C619Cu  /* 0xC619C: {u32 frame; u16 reaction; u8 d; u8 e} */
#define HIT_TABLE_CPU    0x000C6B9Cu  /* 0xC6B9C: the CPU-controller variant */
#define HIT_IMMUNE_MASK  0x000A182Cu  /* 0xA182C: 64 rows x 8 bytes per character */
#define HIT_GEOM_TABLE   0x000A7A70u  /* 0xA7A70: 0x1DDF4's per-character threshold */
#define HIT_GEOM_TABLE2  0x000A7B44u  /* 0xA7B44: 0x1DDF4's per-character table pointer */

/* 0x3C600. The per-attack-frame descriptor for (side, i). `sel` is 2 whenever
 * slot+0x63 != 0 (always in the demo), so the player table 0xC619C is read. */
u32 hit_frame_desc(u32 side, u32 i)
{
    u32 slot = DS_001077B0 + side * 0x94u;
    u32 sel;
    if (DSB(slot + 0x63u) != 0u) {
        sel = 2u;
    } else if (side == 0u) {
        sel = (u32)DSW(DSD(DS_00101514) + 0x2D4u);
    } else {
        sel = (u32)DSW(DSD(DS_00101514) + 0x2D6u);
    }
    {
        u32 entry = (u32)DSB(slot + 0x7Au) * 0x20u + i;
        switch (sel) {
        case 0u: case 4u: case 6u: return DSD(HIT_TABLE_CPU + entry * 8u);
        case 2u:                   return DSD(HIT_TABLE_PLAYER + entry * 8u);
        default:                   return 0u;
        /* PORT: 0x3C6A0 the raw's default returns the caller's ECX; unreachable
         * while slot+0x63 != 0 (the demo), so the port returns 0. */
        }
    }
}

/* 0x3C6A8. Seed/clear slot (side, i): phase = value, the displacement
 * accumulator = 0, and the stun countdown = the frame's word at +0xC. */
void hit_slot_seed(u32 side, u32 value, u32 i)
{
    u32 desc = hit_frame_desc(side, i);
    u32 off = side * 0x40u + i * 2u;
    DSW(DS_00107D58 + off) = (u16)value;
    DSW(DS_00107E58 + off) = 0;
    DSW(DS_00107DD8 + off) = DSW(desc + 0xCu + value * 0x14u);
}

/* 0x3C758. The connect/input test. `facing` is the raw's EDX (DS_00107EE4);
 * `*out` accumulates the command bits the frame's first word maps to. Returns
 * 2 (the third word overlaps the command), 1 (the second word, or no
 * accumulation), 0 (the accumulated mask equals the first word) or 3 (partial
 * overlap). §6.3: transcribed, not unit-pinned. */
static u32 hit_connect(u32 side, u32 facing, u32 i, u16 *out)
{
    u32 desc = hit_frame_desc(side, i);
    u32 off = side * 0x40u + i * 2u;
    u32 phase = (u32)DSW(DS_00107D58 + off);
    u16 m0 = fighter_cmd_facing(facing, DSD(desc + phase * 0x14u));
    u16 m1 = fighter_cmd_facing(facing, DSD(desc + phase * 0x14u + 4u));
    u16 m2 = fighter_cmd_facing(facing, DSD(desc + phase * 0x14u + 8u));
    u16 cmd = DSW(DS_001088E0 + side * 2u);
    if ((cmd & m2) != 0u) return 2u;
    if ((cmd & m1) != 0u) return 1u;
    {
        u32 acc = (u32)*out | (u32)(cmd & m0);
        *out = (u16)acc;
        if ((u32)m0 == acc) return 0u;
        if (((u32)m0 & acc) != 0u) return 3u;
    }
    return 1u;
}

/* 0x3C800. The displacement test. On a command overlap with the mapped word at
 * descriptor+phase*0x14+0x10 it returns 1 and writes the frame's displacement
 * (the high word of the dword at +0x0A, i.e. the word at +0x0C). §6.3. */
static u32 hit_displace(u32 side, u32 facing, u32 i, u16 *out)
{
    u32 desc = hit_frame_desc(side, i);
    u32 off = side * 0x40u + i * 2u;
    u32 phase = (u32)DSW(DS_00107D58 + off);
    u16 m = fighter_cmd_facing(facing, DSD(desc + phase * 0x14u + 0x10u));
    if ((DSW(DS_001088E0 + side * 2u) & m) == 0u) return 0u;
    *out = (u16)(DSD(desc + phase * 0x14u + 0xAu) >> 16);
    return 1u;
}

/* 0x3C88C. The per-slot attack-frame state machine. */
void hit_slot_step(void)
{
    u32 i = DSD(DS_00107ED8);
    u32 side = DSD(DS_00107EDC);
    u32 facing = (u32)DSB(DS_00107EE4);
    u32 off = side * 0x40u + i * 2u;
    u32 desc = hit_frame_desc(side, i);
    s32 phase = (s16)DSW(DS_00107D58 + off);
    u16 out;

    if (desc == 0u) {                                   /* 0x3C8C5 */
        hit_slot_seed(side, 0u, i);
        return;
    }
    if (phase < 0 || phase > 8) {                       /* 0x3C8E5 */
        hit_slot_seed(side, 0u, i);
        return;
    }
    if (phase == 8) {                                   /* 0x3C8F9 */
        DSW(DS_00107DD8 + off) = (u16)(DSW(DS_00107DD8 + off) - 1u);
        if ((s16)DSW(DS_00107DD8 + off) >= 1) return;    /* 0x3C909 */
        hit_slot_seed(side, 0u, i);                      /* 0x3C912 */
        return;
    }
    if (phase == 0) {                                    /* 0x3C92D */
        out = 0;
        if (hit_connect(side, facing, i, &out) == 0u)
            hit_slot_seed(side, 1u, i);                  /* 0x3C96A */
        {
            s32 ph = (s16)DSW(DS_00107D58 + off);
            if (DSD(desc + (u32)ph * 0x14u) == 0u
                    && DSW(DS_00107D58 + off) != 0u)
                DSW(DS_00107D58 + off) = 8u;             /* 0x3C9A3 ARM */
        }
        return;                                          /* 0x3C9EC: phase < 2 */
    }
    if (phase == 1) {                                    /* 0x3C9AE */
        out = 0;
        if (hit_connect(side, facing, i, &out) != 0u)
            hit_slot_seed(side, 2u, i);                  /* 0x3C9E7 */
        return;                                          /* 0x3C9EC: phase < 2 */
    }
    /* phases 2..7 */                                    /* 0x3C9EC */
    DSW(DS_00107DD8 + off) = (u16)(DSW(DS_00107DD8 + off) - 1u);
    if ((s16)DSW(DS_00107DD8 + off) < 1) {               /* 0x3CA1E */
        hit_slot_seed(side, 0u, i);                      /* 0x3CA30 */
        phase = (s16)DSW(DS_00107D58 + off);             /* 0x3CA43 reload */
    }
    out = DSW(DS_00107E58 + off);                        /* 0x3CA61 */
    if (phase >= 2 && phase < 8) {                       /* 0x3CA6B */
        u16 disp = 0;
        if (hit_displace(side, facing, i, &disp) != 0u)  /* 0x3CA94 */
            DSW(DS_00107DD8 + off) = disp;               /* 0x3CAB3 */
    }
    {
        u16 acc = out;
        switch (hit_connect(side, facing, i, &acc)) {    /* 0x3CAD2 */
        case 0u: {                                       /* 0x3CAE8 advance */
            hit_slot_seed(side, (u32)phase + 1u, i);
            {
                s32 ph = (s16)DSW(DS_00107D58 + off);
                if (DSD(desc + (u32)ph * 0x14u) == 0u
                        && DSW(DS_00107D58 + off) != 0u)
                    DSW(DS_00107D58 + off) = 8u;         /* 0x3CB2F ARM */
            }
            break;
        }
        case 2u:                                         /* 0x3C912 */
            hit_slot_seed(side, 0u, i);
            break;
        case 3u:                                         /* 0x3CB41 */
            DSW(DS_00107E58 + off) = acc;
            break;
        default:
            break;
        }
    }
}

/* 0x3CCEC. 1 when the hitbox is valid against the target's stance: the
 * descriptor's `e` flag (byte +7) for stance 2, `d` (byte +6) for stance 0/1. */
int hit_stance_ok(u32 side, u32 i)
{
    u32 slot = DS_001077B0 + side * 0x94u;
    u32 entry = (u32)DSB(slot + 0x7Au) * 0x20u + i;
    u8 e = DSB(HIT_TABLE_PLAYER + entry * 8u + 7u);
    u8 d = DSB(HIT_TABLE_PLAYER + entry * 8u + 6u);
    u8 st = DSB(slot + 0x54u);
    if (e != 0u && st == 2u) return 1;
    if (d != 0u && (st == 1u || st == 0u)) return 1;
    return 0;
}

/* 0x3CD44. The armed-hitbox scan: the first slot whose phase is 8 and whose
 * stance is valid, or -1. */
s32 hit_scan(u32 side)
{
    for (u32 i = 0; i < 0x20u; i++) {
        if ((s16)DSW(DS_00107D58 + side * 0x40u + i * 2u) == 8
                && hit_stance_ok(side, i))
            return (s32)i;
    }
    return -1;
}

/* 0x3CD94. The hit-stun immunity bitmask: a fresh reaction 0xFF is immune (1);
 * a reaction 0..0x3F is immune only when the per-character mask's bit for the
 * candidate reaction is set. */
int hit_immunity(u32 side, u32 i)
{
    u32 slot = DS_001077B0 + side * 0x94u;
    u8 r = DSB(slot + 0x5Fu);
    if (r == 0xFFu) return 1;
    if (r >= 0x40u) return 0;
    {
        u32 ch = (u32)DSB(slot + 0x7Au);
        u32 entry = ch * 0x20u + i;
        s32 c = (s16)DSW(HIT_TABLE_PLAYER + entry * 8u + 4u);
        u32 base = HIT_IMMUNE_MASK + ch * 0x200u + (u32)r * 8u;
        if (c < 0x20) return (int)((DSD(base) >> ((u32)c & 0x1Fu)) & 1u);
        return (int)((DSD(base + 4u) >> (((u32)c - 0x20u) & 0x1Fu)) & 1u);
    }
}

/* 0x3CE24. The stance/hit-stun gate: the signed byte slot+0x56 (the high byte
 * of the dword at slot+0x53) <= 5 and 0x3CD94 != 0. */
int hit_gate(u32 side, u32 i)
{
    u32 slot = DS_001077B0 + side * 0x94u;
    if ((s8)DSB(slot + 0x56u) >= 6) return 0;
    return hit_immunity(side, i) != 0;
}

/* 0x4CE70. The per-character reaction allow-list: 1 unless `reaction` is one of
 * the character's blocked ids. */
int hit_reaction_allow(u32 side, u32 reaction)
{
    u32 ch = (u32)DSB(DS_001077B0 + side * 0x94u + 0x7Au);
    if (ch < 7u) {
        switch (ch) {
        case 0u:
            if (reaction == 0x28u || reaction == 0x29u
                    || reaction == 0x2Au || reaction == 0x2Cu) return 0;
            break;
        case 1u:
            if (reaction == 0x22u || reaction == 0x23u
                    || reaction == 0x24u) return 0;
            break;
        case 2u:
            if (reaction == 0x22u || reaction == 0x24u
                    || reaction == 0x26u || reaction == 0x27u) return 0;
            break;
        case 3u:
            if (reaction == 0x22u || reaction == 0x23u) return 0;
            break;
        case 4u:
            if (reaction == 0x24u || reaction == 0x26u) return 0;
            break;
        case 5u:
            if (reaction == 0x21u || reaction == 0x22u) return 0;
            break;
        case 6u:
            if (reaction == 0x21u || reaction == 0x24u) return 0;
            break;
        }
    }
    return 1;
}

/* 0x1881C. The two slots latched, then slot0+0x30 - slot1+0x30. */
static s32 hit_vert_distance(void)
{
    fighter_slot_latch(0u);                             /* 0x18820 */
    fighter_slot_latch(1u);                             /* 0x18828 */
    return (s32)DSD(DS_001077E0) - (s32)DSD(DS_001077E0 + 0x94u);
}

/* 0x1DDF4. The attacker/defender geometry test: |0x187FC| <= threshold1 and
 * |0x1881C| <= threshold2, the thresholds being the two per-character byte
 * tables shifted left 6. `side` is the raw's EAX (the attacker's opposite);
 * `table` is 0xA7B44[char(side)] and `idx` is 0xA7A70. §6.7. */
int hit_geometry(u32 side, u32 table, u32 idx)
{
    u32 ch = (u32)DSB(DS_001077B0 + side * 0x94u + 0x7Au);
    u32 thr1 = (u32)DSB(table + ch) << 6;
    u32 thr2 = (u32)DSB(idx + ch) << 6;
    s32 d1 = ai_distance();
    if (d1 < 0) d1 = -d1;
    if (d1 > (s32)thr1) return 0;
    {
        s32 d2 = hit_vert_distance();
        if (d2 < 0) d2 = -d2;
        if (d2 > (s32)thr2) return 0;
    }
    return 1;
}

/* 0x3CBC4. Reaction variant A: 0x16 crouching, 0x14 on command bit 0x4000,
 * 0x12/0x10 from 0x1DDF4. The 0x33950 call only supplies the register
 * leftovers 0x1DDF4 reads (ctx[2] = &slot[side] for the character byte). */
u16 hit_reaction_a(u32 side)
{
    u32 slot = DS_001077B0 + side * 0x94u;
    if (DSB(slot + 0x54u) == 2u) return 0x16u;
    if ((DSW(DS_001088E0 + side * 2u) & 0x4000u) != 0u) return 0x14u;
    if (hit_geometry(1u - side,
                     DSD(HIT_GEOM_TABLE2 + (u32)DSB(slot + 0x7Au) * 4u),
                     HIT_GEOM_TABLE))
        return 0x12u;
    return 0x10u;
}

/* 0x3CC58. Reaction variant B: the same shape with 0x17/0x15/0x13/0x11. */
u16 hit_reaction_b(u32 side)
{
    u32 slot = DS_001077B0 + side * 0x94u;
    if (DSB(slot + 0x54u) == 2u) return 0x17u;
    if ((DSW(DS_001088E0 + side * 2u) & 0x4000u) != 0u) return 0x15u;
    if (hit_geometry(1u - side,
                     DSD(HIT_GEOM_TABLE2 + (u32)DSB(slot + 0x7Au) * 4u),
                     HIT_GEOM_TABLE))
        return 0x13u;
    return 0x11u;
}

/* 0x34D8C. The +0x59 palette-flash pair, only when DS_001078FA == 2. The raw
 * dereferences each slot to its record (0x34DB1/0x34DCC) before the +0x59. */
void hit_flash_pair(u32 side)
{
    if (DSB(DS_001078FA) != 2u) return;
    DSB(DSD(DS_001077B0 + side * 0x94u) + 0x59u) = 1u;          /* 0x34DBA */
    DSB(DSD(DS_001077B0 + (1u - side) * 0x94u) + 0x59u) = 0xFFu; /* 0x34DD3 */
}

/* 0x339AC. The ctx builder from a record: out[0]=rec+0x51, out[1]=1-out[0],
 * out[2]=&slot[out[0]], out[3]=&slot[out[1]], out[4]=*out[2], out[5]=*out[3]. */
void hit_anim_ctx(u32 out[6], u32 rec)
{
    u32 s = (u32)DSB(rec + 0x51u);
    out[0] = s;
    out[1] = 1u - s;
    out[2] = DS_001077B0 + s * 0x94u;
    out[3] = DS_001077B0 + (1u - s) * 0x94u;
    out[4] = DSD(out[2]);
    out[5] = DSD(out[3]);
}

/* 0x188AC. Write the record's +0x18/+0x1C and re-latch the slot. */
void hit_anchor_set(u32 side, u32 x, u32 y)
{
    u32 rec = DSD(DS_001077B0 + side * 0x94u);
    DSD(rec + 0x18u) = x;
    DSD(rec + 0x1Cu) = y;
    fighter_slot_latch(side);                           /* 0x188D2 */
}

/* 0x18714. The record-x the 0x188DC tail writes: slot+0x42 bit 3 set takes the
 * record's +0x18; otherwise slot+0x2C minus DS_00100AB0[side]. */
static u32 hit_record_x(u32 side)
{
    u32 slot = DS_001077B0 + side * 0x94u;
    if ((DSB(slot + 0x42u) & 0x08u) != 0u)
        return DSD(DSD(slot) + 0x18u);                  /* 0x18738 */
    /* PORT: 0x1873F 0x18540(side) and 0x1875F 0x18350(side, anchor) — the
     * screen-anchor path is the same named gap as fighter_slot_latch's (§6.3);
     * the raw's final `slot+0x2C - DS_00100AB0[side]` is kept. */
    return DSD(slot + 0x2Cu) - DSD(DS_00100AB0 + side * 8u);
}

/* 0x18788. The record-y twin of 0x18714: slot+0x42 bit 3 set takes the record's
 * +0x1C; otherwise slot+0x30 minus DS_00100AB4[side]. */
static u32 hit_record_y(u32 side)
{
    u32 slot = DS_001077B0 + side * 0x94u;
    if ((DSB(slot + 0x42u) & 0x08u) != 0u)
        return DSD(DSD(slot) + 0x1Cu);                  /* 0x187AC */
    /* PORT: 0x187B3 0x18540(side) and 0x187D3 0x18350(side, anchor) — the same
     * screen-anchor path as hit_record_x's (0x18714); the raw's final
     * `slot+0x30 - DS_00100AB4[side]` is kept. */
    return DSD(slot + 0x30u) - DSD(DS_00100AB4 + side * 8u);
}

/* 0x188DC. Set slot+0x2C to x, then write the record's +0x18 from 0x18714. */
void hit_anchor_x(u32 side, u32 x)
{
    DSD(DS_001077B0 + side * 0x94u + 0x2Cu) = x;        /* 0x188F2 */
    DSD(DSD(DS_001077B0 + side * 0x94u) + 0x18u) = hit_record_x(side);
}

/* 0x1890C. Re-latch, then rec+0x1C += (y - slot+0x30), then re-latch. */
void hit_anchor_y(u32 side, u32 y)
{
    u32 slot = DS_001077B0 + side * 0x94u;
    u32 rec;
    fighter_slot_latch(side);                           /* 0x18916 */
    rec = DSD(slot);
    DSD(rec + 0x1Cu) = (u32)((s32)DSD(rec + 0x1Cu)
                             + ((s32)y - (s32)DSD(slot + 0x30u)));
    fighter_slot_latch(side);                           /* 0x18943 */
}

/* 0x3C480. anim-begin plus the two anchor writes. */
static void hit_anim_start_a(u32 rec, u32 stream, u32 frame_bits)
{
    u32 ctx[6];
    hit_anim_ctx(ctx, rec);                             /* 0x3C48E */
    hit_anchor_set(ctx[0], DSD(ctx[4] + 0x18u), 0u);    /* 0x3C49F */
    actors_anim_begin(rec, stream, frame_bits);         /* 0x3C4B3 */
    hit_anchor_x(ctx[0], DSD(ctx[2] + 0x2Cu));          /* 0x3C4BD */
}

/* 0x3C4CC. Dispatch on slot+0x52: {0,1,2,5,0xE,0x15} -> 0x2BC30, else 0x3C480. */
static void hit_anim_start_b(u32 rec, u32 stream, u32 frame_bits)
{
    u32 ctx[6];
    hit_anim_ctx(ctx, rec);                             /* 0x3C4D6 */
    {
        u8 st = DSB(ctx[2] + 0x52u);
        if (st == 0u || st == 1u || st == 2u || st == 5u
                || st == 0xEu || st == 0x15u)
            actors_anim_begin(ctx[4], stream, frame_bits);   /* 0x3C502 */
        else
            hit_anim_start_a(ctx[4], stream, frame_bits);    /* 0x3C513 */
    }
}

/* 0x3C520. anim-begin plus the +0x2C/+0x30 anchor writes. */
static void hit_anim_start_c(u32 rec, u32 stream, u32 frame_bits)
{
    u32 ctx[6];
    hit_anim_ctx(ctx, rec);                             /* 0x3C52F */
    actors_anim_begin(rec, stream, frame_bits);         /* 0x3C54A */
    hit_anchor_x(ctx[0], DSD(ctx[2] + 0x2Cu));          /* 0x3C554 */
    hit_anchor_y(ctx[0], DSD(ctx[2] + 0x30u));          /* 0x3C55E */
}

/* 0x18B04. The attacker/defender facing flag: when mode != 0x22 and
 * self+0x2C < other+0x2C set self_rec+0x29 bit 0x40 (else clear it), then
 * slot+0x2C = self+0x2C and rec+0x18 = 0x18714(side). */
static void hit_facing_flag(u32 side)
{
    u32 ctx[6];
    if (DSW(DS_00104B00) == 0x22u) return;              /* 0x18B16 */
    fighter_ctx_same(ctx, side);                        /* 0x18B1C */
    if ((s32)DSD(ctx[2] + 0x2Cu) < (s32)DSD(ctx[3] + 0x2Cu))
        DSB(ctx[4] + 0x29u) |= 0x40u;                   /* 0x18B39 */
    else
        DSB(ctx[4] + 0x29u) &= 0xBFu;                   /* 0x18AAE */
    DSD(ctx[2] + 0x2Cu) = DSD(ctx[2] + 0x2Cu);          /* 0x18AC7: self -> self */
    DSD(ctx[4] + 0x18u) = hit_record_x(side);           /* 0x18AEC */
}

/* 0x1922C. The stance timer: when DS_00100B5A[side] > 0 and the record's +0x24
 * float is clear, seed it from the signed byte DS_00100B5C[side], zero +0x20,
 * and clamp a value outside [1.0, DS_0008058C] to 3.0. Then clear B5A (only on
 * the taken arm) and B5E. */
static void hit_stance_timer(u32 side)
{
    if ((s8)DSB(DS_00100B5A + side) > 0) {              /* 0x19244 */
        u32 rec = DSD(DS_001077B0 + side * 0x94u);
        if ((DSD(rec + 0x24u) & 0x7FFFFFFFu) == 0u) {   /* 0x19251 */
            union { float f; u32 u; } fu;
            fu.f = (float)(s16)(s8)DSB(DS_00100B5C + side);
            DSD(rec + 0x24u) = fu.u;                    /* 0x1926A */
            DSD(rec + 0x20u) = 0;                       /* 0x19271 */
            if (!(fu.f >= 1.0f) || fu.f > *(const float *)(mem + DS_0008058C))
                DSD(rec + 0x24u) = 0x40400000u;         /* 0x1929C 3.0f */
        }
        DSB(DS_00100B5A + side) = 0;                    /* 0x192A8 */
    }
    DSB(DS_00100B5E + side) = 0;                        /* 0x192B3 */
}

/* 0x32BAC. The raw entry the chain calls is a one-byte RET: the 44-byte extent
 * record §0.3.4 gives it is the orphaned 0x32BB0 body, whose callers are
 * elsewhere, so the hit sound is dead on this path. The demo also skips the call
 * entirely (slot+0x63 != 0). Correction to record §3.6/§7.11 (raw wins). */
void hit_sound(u32 ch)
{
    (void)ch;
}

/* 0x34E2C. The reaction driver. With reaction == 0xFF it returns; otherwise it
 * resets the reaction state, plays the animation the (char, reaction) table
 * selects and drives the +0x52/+0x53 transitions. The 0xA3528 entry fields and
 * the *(u32*)anim[1] callback (0x3D17C) are the §6.9 gap, and the 0x2C3FC voice
 * is out of scope; the +0x5F stores stay. */
void hit_reaction_apply(u32 side, u32 reaction)
{
    u32 slot = DS_001077B0 + side * 0x94u;
    u32 other = DS_001077B0 + (1u - side) * 0x94u;
    u32 rec = DSD(slot);
    u32 anim[3];
    u32 stream = 0;
    u32 callback;

    if (reaction == 0xFFu) return;                      /* 0x34E36 */

    DSW(slot + 0x88u) = 0;                              /* 0x34E9E */
    DSB(slot + 0x8Au) = 1;                              /* 0x34EAE */
    if (DSB(slot + 0x54u) == 2u)
        DSB(DS_001078F8 + side) = 0;                    /* 0x34EBC */
    else
        hit_facing_flag(side);                          /* 0x34ECF 0x18B04 */
    hit_stance_timer(side);                             /* 0x34ED7 0x1922C */
    DSW(slot + 0x84u) = (u16)(DSW(slot + 0x84u) + 1u);  /* 0x34EEB */
    DSB(slot + 0x5Fu) = (u8)reaction;                   /* 0x34EE7 */
    DSB(DS_001088A8 + side) = (u8)reaction;             /* 0x34EF6 */
    DSB(rec + 0x63u) = 0;                               /* 0x34EFC */
    if (DSB(DS_001078FA) == 2u) {                       /* 0x34F07 */
        DSB(rec + 0x59u) = 1u;                          /* 0x34F2E */
        DSB(DSD(other) + 0x59u) = 0xFFu;                /* 0x34F47 */
    }
    fighter_anim_triple(anim, side, (s32)reaction);     /* 0x34F5B 0x3AFC4 */
    {
        u32 p = DSD(anim[1] + 4u);                      /* 0x34F64 */
        if (p != 0u) stream = DSD(p);
    }
    callback = DSD(anim[1]);                            /* 0x34F77 */
    {
        u16 bx = DSW(anim[2] + 2u);                     /* 0x34F7F */
        if (stream != 0u) {                             /* 0x34F85 */
            /* PORT: 0x34F97/0x34FA4 the 0xE9308 sound through 0x2C3FC, out of
             * scope (spec §7). */
            if (DSB(slot + 0x54u) != 2u)
                hit_anim_start_b(rec, stream, 0x40000000u);   /* 0x34FBC 0x3C4CC */
            else
                hit_anim_start_c(rec, stream, 0x40000000u);   /* 0x34FCC 0x3C520 */
            if (DSB(slot + 0x52u) != 4u)
                DSB(slot + 0x52u) = 9u;                 /* 0x34FDB */
            DSB(slot + 0x53u) = 8u;                     /* 0x34FE3 */
            DSW(slot + 0x6Au) = (u16)(DSW(slot + 0x6Au) + 1u);   /* 0x34FF7 */
            if (((bx >> 8) & 0x10u) != 0u)
                DSB(slot + 0x41u) |= 0x80u;             /* 0x35004 */
        } else {
            DSB(slot + 0x5Fu) = 0xFFu;                  /* 0x3500E */
        }
    }
    if (callback != 0u) {                               /* 0x35017 */
        /* PORT: 0x35032 the 0x2C3FC voice (out of scope) and 0x35045 the
         * *(u32*)anim[1] callback 0x3D17C (§6.9 gap); the +0x5F store stays. */
        DSB(slot + 0x5Fu) = (u8)reaction;               /* 0x35042 */
    }
}

/* 0x3CE58. Validate the hitbox and drive the reaction: the 0x3CE24 gate, the
 * 0x4CE70 allow-list in modes 0x21/0x22, 0x34D8C, the 0x10/0x11 variant select
 * and 0x34E2C. */
int hit_reaction_drive(u32 side, u32 i)
{
    u32 slot = DS_001077B0 + side * 0x94u;
    u32 reaction;
    if (!hit_gate(side, i)) return 0;                   /* 0x3CE5E */
    if (i >= 0x20u) return 1;                           /* 0x3CE73 */
    reaction = (u32)(s16)DSW(HIT_TABLE_PLAYER
                             + ((u32)DSB(slot + 0x7Au) * 0x20u + i) * 8u + 4u);
    if ((DSW(DS_00104B00) == 0x21u || DSW(DS_00104B00) == 0x22u)
            && !hit_reaction_allow(side, reaction))
        return 0;                                       /* 0x3CEC0 */
    hit_flash_pair(side);                               /* 0x3CEC8 */
    if (reaction == 0x10u)      reaction = hit_reaction_a(side);   /* 0x3CF04 */
    else if (reaction == 0x11u) reaction = hit_reaction_b(side);   /* 0x3CF0D */
    DSB(slot + 0x5Fu) = (u8)reaction;                   /* 0x3CF25 */
    hit_reaction_apply(side, reaction);                 /* 0x3CF2E */
    return 1;
}

/* 0x3CF38. The hit wrapper. */
int hit_chain_resolve(u32 side)
{
    u32 slot = DS_001077B0 + side * 0x94u;
    s32 i = hit_scan(side);                             /* 0x3CF54 */
    if (i == -1) return 0;                              /* 0x3CF5E */
    {
        u8 r = DSB(slot + 0x5Fu);                       /* 0x3CF64 */
        if ((r >= 0x10u && r <= 0x17u) && (u32)i >= 0x1Cu)
            return 0;                                   /* 0x3CF75 */
    }
    if (hit_reaction_drive(side, (u32)i) != 0) {        /* 0x3CF7F */
        DSB(slot + 0x7Cu) = (u8)(DSB(slot + 0x7Cu) + 1u);   /* 0x3CFA6 */
        DSB(slot + 0x55u) = (u8)i;                      /* 0x3CFAE */
        hit_slot_seed(side, 0u, (u32)i);                /* 0x3CFB4 consume */
        if (DSB(slot + 0x63u) == 0u)
            hit_sound((u32)DSB(slot + 0x7Au));          /* 0x3CFCA 0x32BAC */
        return 1;
    }
    if (DSB(slot + 0x53u) == 0u)
        DSB(slot + 0x5Fu) = 0xFFu;                      /* 0x3CFDF */
    DSB(slot + 0x55u) = 0xFFu;                          /* 0x3CFF4 */
    return 0;
}
