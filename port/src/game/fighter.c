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

/* PORT: data-object addresses with no symbols.h name (the generator emits none
 * for these tables). */
#define FIGHTER_SPAWN_X   0x000BDA38u   /* 0x33EC4: per-side initial x dword */
#define FIGHTER_DESC_A    0x000BB7E0u   /* 0x33CC8: [char*2 + side] fighter */
#define FIGHTER_DESC_B    0x000BB8D0u   /* 0x33D38: [char] secondary actor */

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

/* 0x186D0. The slot position latch (the game_frame tail's 0x25438 call). */
void fighter_slot_latch(u32 side)
{
    u32 slot = DS_001077B0 + side * 0x94u;
    u32 rec = DSD(slot);                            /* 0x186F3/0x18702 */
    if ((DSB(slot + 0x42u) & 0x08u) == 0) {         /* 0x186E6 */
        /* PORT: 0x18627 0x18540(side) and 0x1864D 0x18350(side, anchor) — the
         * screen-anchor path is a named gap (§6.3); the anchor compare and the
         * +0x100AB0/+0x100AB4 offsets are transcribed. */
        u32 anchor = DSD(DS_00100AF0 + side * 4u);
        if (anchor != DSD(slot + 0x20u)) {
            DSD(slot + 0x20u) = anchor;             /* 0x18645 */
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

    /* 0x33E1E 0x29BC8(side, char, rec): the character palette at pset+0x18. */
    {
        u32 tbl = DSD(DS_000A8A98 + ch * 4u);       /* 0x29BCD */
        u32 handle = DSD(tbl + (u32)DSB(DS_00105B34 + side) * 4u);
        actor_pset_palette(rec, 0u, handle);        /* 0x29BE1 0x2A17C */
    }

    DSB(slot + 0x41u) &= 0x7Fu;                     /* 0x33E23..0x33E2F */
    DSD(DS_001077A0 + side * 4u) = 0;               /* 0x33E38 */

    if (DSB(DS_00104B14) == 0) {
        /* PORT: 0x33E43 0x494A8 — the dust/effect entry builder (515 B). It
         * builds an entry on the 0x10884C list and draws rng(0x1800)/rng(0x300),
         * but it is not what makes the slot live (the stores above are), so it
         * is a named gap (§10.5). */
    }
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
        /* PORT: 0x37178 — +0x42 bit 6 arm, a named gap (§7.10). */
        return;
    }
    if ((DSB(slot + 0x42u) & 0x80u) != 0u) {            /* 0x349F7 */
        /* PORT: 0x37D18 — +0x42 bit 7 arm, a named gap (§7.10). */
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
 * when 0x3CF38 reports no hit, arms slot+0x54 = 2 and slot+0x53 = 4. 0x3CF38 is
 * the hit-detection chain (0x3CD44/0x3CE58/0x3C6A8/0x32BAC) already a named gap
 * (§7.6), so the conditional arm is declared, not issued: the port keeps the
 * unconditional clears. */
void fighter_state_35d7c(u32 side)
{
    u32 slot = DSD(DS_001077A8 + side * 4u);            /* 0x35DF1 */
    if (slot == 0u) return;
    DSB(slot + 0x54u) = 0;                              /* 0x35DDA */
    DSB(slot + 0x53u) = 0;                              /* 0x35DDE */
    /* PORT: 0x35DE8 0x3CF38(side) and its 0x35DF5 arm (slot+0x54 = 2,
     * slot+0x53 = 4) are named gaps (§7.6/§11.5). */
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
