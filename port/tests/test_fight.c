/* port/tests/test_fight.c */
#include "game/actors.h"
#include "game/camera.h"
#include "game/fight.h"
#include "game/fighter.h"
#include "game/flow.h"
#include "game/rng.h"
#include "mem.h"
#include "platform/render.h"
#include "symbols.h"
#include "test.h"
#include "test_fixtures.h"
#include <string.h>

/* The derivation record §1.3's four worked pairs: actor+4, actor+8, the
 * projection stage (actor4+0x20)>>6, (actor8+0x20)>>6. The final stored globals
 * subtract the sprite origin; these assert the statically-pinnable stage with
 * res_resolve forced to NULL. */
static const u32 s_pairs[4][4] = {
    { 0x00001000u, 0x00002000u, 0x00000040u, 0x00000080u },
    { 0x000003FFu, 0x00000040u, 0x00000010u, 0x00000001u },
    { 0xFFFFFFFFu, 0xFFFFFFC0u, 0x00000000u, 0xFFFFFFFFu },
    { 0x00000040u, 0x00000080u, 0x00000001u, 0x00000002u },
};

/* The repeated in-file setups, named. Each body is exactly the sequence the
 * checks below used to repeat, in the same order and with the same values. */

/* Point the two fighter-slot bases at the scratch records (0x100 apart). */
static void fight_reset_bases(void)
{
    DSD(DS_001077B0) = FIGHT_RECS;
    DSD(DS_00107844) = FIGHT_RECS + 0x100u;
}

/* Zero the record scratch and point the two slot bases at it. */
static void fight_reset_recs(void)
{
    mem_fill(FIGHT_RECS, 0, 0x200);
    fight_reset_bases();
}

/* Zero the actor pool and point DS_001014EC at it. */
static void fight_reset_actors(void)
{
    mem_fill(FIGHT_ACTORS, 0, 0x80);
    DSD(DS_001014EC) = FIGHT_ACTORS;
}

/* Zero the three per-slot hitbox arrays at 0x107D58. */
static void fight_reset_slots(void)
{
    mem_fill(0x00107D58u, 0, 0x180u);
}

/* Wire DS_001077A8 to the two slots and each slot's +0 to its record. */
static void fight_reset_slot_pair(u32 s0, u32 s1, u32 r0, u32 r1)
{
    DSD(DS_001077A8) = s0;
    DSD(DS_001077A8 + 4u) = s1;
    DSD(s0) = r0;
    DSD(s1) = r1;
}

/* Wire DS_001077A8 to slot 0 only (side 1 inert) and slot 0's record. */
static void fight_reset_slot0(u32 p0, u32 r0)
{
    DSD(DS_001077A8) = p0;
    DSD(DS_001077A8 + 4u) = 0;
    DSD(p0) = r0;
}

/* The state-6 / game_frame tail globals, zeroed with the 0x0F timer armed. */
static void fight_reset_state6(void)
{
    DSB(DS_00104B15) = 0;
    DSB(DS_00104B19 + 2u) = 0;
    DSW(DS_001082CC) = 0;
    DSW(DS_00104AFC) = 0;
    DSW(DS_000F0A6A) = 0;
    DSW(DS_000F0A72) = 5;
    DSW(DS_000F0A6C) = 0;
    DSB(DS_000F0A6F) = 0xFF;
}

/* 0x17FA0: the four worked pairs at the (x+0x20)>>6 stage, plus side selection,
 * the facing/page flags, the 0x100AF0 index base and the sprite-origin
 * subtraction on a seeded fake resource. */
static void check_projection(void)
{
    u32 p0 = FIGHT_RECS;
    u32 p1 = FIGHT_RECS + 0x100u;
    u32 a0 = FIGHT_ACTORS + 1u * 0x20u;
    u32 a1 = FIGHT_ACTORS + 2u * 0x20u;
    u32 saved_tab = DSD(DS_001014E0), saved_n = DSD(DS_001014F0);

    /* The data object must be loaded (test_le runs first): 0x17EEC reads the
     * per-character constant table and the sprite table lives at 0xA8B30. */
    CHECK_EQ_INT((int)DSW(DS_000E6DD0), 0x0EE4);

    DSD(DS_001014EC) = FIGHT_ACTORS;
    DSD(DS_001077B8) = 0;               /* skip the side-independent blocks */
    DSD(DS_0010784C) = 0;
    fight_reset_bases();
    DSW(p0 + 0x28u) = 0x4000;           /* facing bit set for side 0 */
    DSW(p0 + 0x56u) = 1;                /* actor index */
    DSB(DS_0010782A) = 0;               /* side-0 char -> 0x0EE4 */
    DSW(p1 + 0x28u) = 0;                /* facing clear for side 1 */
    DSW(p1 + 0x56u) = 2;
    DSB(DS_001078BE) = 0;               /* side-1 char -> 0x0EE4 */
    DSW(a0) = 1;                        /* actor word 0: bit 15 clear */
    DSW(a1) = 1;

    /* No resource: 0x16308 resolves to NULL and the origin subtraction is
     * skipped, exposing the statically-determinable stage. */
    DSD(DS_001014F0) = 0;

    /* Seed the page flag: BSS 0 cannot distinguish "wrote 0" from "never
     * touched it", so the 0x100B60 assertion below needs a differing sentinel. */
    DSB(DS_00100B60) = 0xFF;

    for (int i = 0; i < 4; i++) {
        DSD(a0 + 4u) = s_pairs[i][0];
        DSD(a0 + 8u) = s_pairs[i][1];
        DSD(DS_00100B08) = 0xDEADBEEFu;
        DSD(DS_00100B00) = 0xDEADBEEFu;
        camera_project(0, DS_00100B08, DS_00100B00, DS_00100B62,
                       DS_00100B60, DS_00100AF0);
        CHECK_EQ_INT((int)DSD(DS_00100B08), (int)s_pairs[i][2]);
        CHECK_EQ_INT((int)DSD(DS_00100B00), (int)s_pairs[i][3]);
    }

    /* The facing byte is the record's +0x28 bit 0x4000; the page flag is
     * zeroed at 0x18092 and stays 0 because the tail's state gate (slot+0x53
     * not 7/8) rejects here. The 0xFF sentinel proves the tail zeroed it. */
    CHECK_EQ_INT((int)DSB(DS_00100B62), 1);
    CHECK_EQ_INT((int)DSB(DS_00100B60), 0);
    /* 0x100AF0 = (actor.word0 & 0x7FFF) - 0x17EEC(0) = 1 - 0x0EE4. */
    CHECK_EQ_INT((int)DSD(DS_00100AF0), (int)(1 - 0x0EE4));

    /* Side 1 writes the other globals with its own record/actor, so a side
     * swap cannot pass on side 0's data. */
    DSD(a1 + 4u) = 0x00000800u;
    DSD(a1 + 8u) = 0x00001000u;
    DSD(DS_00100B0C) = 0xDEADBEEFu;
    DSD(DS_00100B04) = 0xDEADBEEFu;
    DSD(DS_00100AF4) = 0xDEADBEEFu;
    camera_project(1, DS_00100B0C, DS_00100B04, DS_00100B63,
                   DS_00100B61, DS_00100AF4);
    CHECK_EQ_INT((int)DSD(DS_00100B0C), 0x20);
    CHECK_EQ_INT((int)DSD(DS_00100B04), 0x40);
    CHECK_EQ_INT((int)DSB(DS_00100B63), 0);
    CHECK_EQ_INT((int)DSB(DS_00100B61), 0);
    CHECK_EQ_INT((int)DSD(DS_00100AF4), (int)(1 - 0x0EE4));

    /* The sprite-origin subtraction must be real, not vacuous: seed a fake
     * resource whose index 31 (the handle at 0xA8B34 = 0xF8045DC) resolves to a
     * sprite with known origin fields. */
    {
        u32 tab = FIGHT_RECS + 0x2000u;             /* 32 entries * 0x14 */
        u32 sbase = FIGHT_RECS + 0x1000u;
        u32 sprite = sbase + (0xF8045DCu & 0x7FFFFFu);
        mem_fill(tab, 0, 32u * 0x14u);
        DSD(DS_001014E0) = tab;
        DSD(DS_001014F0) = 32;
        DSD(tab + 31u * 0x14u + 16u) = sbase;
        DSW(sprite) = 5;                            /* (s16)word[sprite] */
        DSB(sprite + 4u) = 0x0A;                    /* DSD(sprite+2)>>16 = 10 */
        DSB(sprite + 5u) = 0x00;
        DSB(sprite + 6u) = 0x0B;                    /* DSD(sprite+4)>>16 = 11 */
        DSB(sprite + 7u) = 0x00;
        DSD(a0 + 4u) = 0x00001000u;                 /* stage 0x40 */
        DSD(a0 + 8u) = 0x00002000u;                 /* stage 0x80 */
        DSW(a0) = 1;                                /* 0x1A570 true */
        camera_project(0, DS_00100B08, DS_00100B00, DS_00100B62,
                       DS_00100B60, DS_00100AF0);
        CHECK_EQ_INT((int)DSD(DS_00100B08), 0x40 - 10);
        CHECK_EQ_INT((int)DSD(DS_00100B00), 0x80 - 11);
        /* bit 15 set selects the other local_A: 5 - 10 - 1 = -6. */
        DSW(a0) = 0x8001;
        camera_project(0, DS_00100B08, DS_00100B00, DS_00100B62,
                       DS_00100B60, DS_00100AF0);
        CHECK_EQ_INT((int)DSD(DS_00100B08), 0x40 - (-6));
    }

    DSD(DS_001014E0) = saved_tab;
    DSD(DS_001014F0) = saved_n;
}

/* 0x12D48: the clamp and the mode-2 run. */
static void check_dispatch(void)
{
    u32 p0 = FIGHT_RECS, p1 = FIGHT_RECS + 0x100u;

    DSB(DS_000F0AFE) = 4;                       /* no mode helper */
    DSD(DS_000F0AF0) = 0x7000;
    camera_dispatch();
    CHECK_EQ_INT((int)DSD(DS_000F0AF0), 0x5D00);
    DSD(DS_000F0AF0) = 0xFFFFA000u;             /* -0x6000 */
    camera_dispatch();
    CHECK_EQ_INT((int)DSD(DS_000F0AF0), (int)0xFFFFA300u);   /* -0x5D00 */

    /* Mode 2 centers on the two records' +0x18 midpoint. */
    fight_reset_bases();
    DSD(p0 + 0x18u) = 0x1000;
    DSD(p1 + 0x18u) = 0x3000;
    DSD(DS_000F0AF0) = 0;
    DSD(DS_000F0AEC) = 0;
    DSB(DS_001078FE) = 0;
    DSB(DS_000F0AFE) = 2;
    camera_dispatch();
    CHECK_EQ_INT((int)DSD(DS_000F0AF0), 0x40);  /* one 0x40 step toward 0x2000 */
    CHECK_EQ_INT((int)DSB(DS_000F0AFE), 2);     /* DS_001078FE clear: no settle */

    /* The settle transition fires when camera y is 0 and the gate is set. */
    DSD(DS_000F0AF0) = 0x2000;
    DSB(DS_001078FE) = 1;
    camera_dispatch();
    CHECK_EQ_INT((int)DSD(DS_000F0AF0), 0x2000);
    CHECK_EQ_INT((int)DSB(DS_000F0AFE), 4);
}

/* Seed one side for 0x12E3C's split arm: slot +0x34/+0x38 (the +0x34 latch
 * and its previous-frame copy), char 0, the actor sprite id that 0x18540 turns
 * into `anchor` (word[0xE6DD0] = 0x0EE4 is char 0's constant), slot+0x20 unlike
 * the anchor so 0x18350 re-derives DS_00100AB0/AB4, and sentinels in +0x2C,
 * the record's +0x18, DS_00100AF0 and DS_00100AB0/AB4. */
static void cs_seed(u32 side, s32 s34, s32 s38, u32 anchor)
{
    u32 slot = DS_001077B0 + side * 0x94u;
    u32 rec = FIGHT_RECS + side * 0x100u;
    DSD(slot) = rec;
    DSD(DS_001077A8 + side * 4u) = slot;
    DSB(slot + 0x7Au) = 0;
    DSB(slot + 0x42u) = 0;
    DSD(slot + 0x20u) = 0xFFFFFFFFu;
    DSD(slot + 0x2Cu) = 0x7770u + side;
    DSD(slot + 0x34u) = (u32)s34;
    DSD(slot + 0x38u) = (u32)s38;
    DSW(rec + 0x56u) = (u16)(side + 1u);
    DSW(FIGHT_ACTORS + (side + 1u) * 0x20u) = (u16)(DSW(0x000E6DD0u) + anchor);
    DSD(rec + 0x18u) = 0x5550u + side;
    DSD(DS_00100AF0 + side * 4u) = 0x3330u + side;
    DSD(0x00100AB0u + side * 8u) = 0x1230u + side;
    DSD(0x00100AB4u + side * 8u) = 0x4560u + side;
}

/* 0x12E3C's split arm (demo-pose record §24). When the pair's +0x34 latches
 * are more than word[0x9AF28] = 0x5000 apart, a slot that moved outward past
 * its +0x38 latch is pulled back: +0x34 and +0x2C take the latch
 * (0x12EF6/0x12EF9, 0x12F16/0x12F19), then 0x18714 (EAX = that slot's side,
 * 0x12EFC/0x12F1C) rewrites its record's +0x18 (0x12F09/0x12F29). The anchors
 * 370 and 381 read (5, 9) and (4, 48) at 0xCEB00 + anchor*2, so AB0 is 320 for
 * side 0 and 256 for side 1. */
static void check_camera_split(void)
{
    u32 p0 = FIGHT_RECS, p1 = FIGHT_RECS + 0x100u;
    u32 s0 = DS_001077B0, s1 = DS_001077B0 + 0x94u;

    fight_reset_recs();
    fight_reset_actors();

    /* A, the demo at f = 514: side 0 (the T-rex, slot b) 26674 against its
     * latch 26456; side 1 (the raptor, slot a) 6148 at its latch. 26674 - 6148
     * = 20526 > 0x5000. */
    cs_seed(0, 26674, 26456, 370);
    cs_seed(1, 6148, 6148, 381);
    DSB(DS_000F0AFE) = 1;
    DSD(DS_000F0AF0) = 0x4000;
    DSW(DS_000F0AFC) = 0x400;
    camera_dispatch();
    CHECK_EQ_INT((int)DSD(s0 + 0x34u), 26456);
    CHECK_EQ_INT((int)DSD(s0 + 0x2Cu), 26456);
    CHECK_EQ_INT((int)DSD(p0 + 0x18u), 26456 - 320);
    CHECK_EQ_INT((int)DSD(DS_00100AF0), 370);
    CHECK_EQ_INT((int)DSD(0x00100AB0u), 320);
    CHECK_EQ_INT((int)DSD(0x00100AB4u), 576);
    CHECK_EQ_INT((int)DSD(s0 + 0x20u), -1);             /* 0x18714 stores no anchor */
    CHECK_EQ_INT((int)DSD(s1 + 0x34u), 6148);
    CHECK_EQ_INT((int)DSD(s1 + 0x2Cu), 0x7771);
    CHECK_EQ_INT((int)DSD(p1 + 0x18u), 0x5551);
    CHECK_EQ_INT((int)DSD(DS_00100AF4), 0x3331);
    CHECK_EQ_INT((int)DSD(0x00100AB8u), 0x1231);
    /* d0 = 26456 - 0x4000 -> 3928, d1 = 6148 - 0x4000 -> -4092; the split
     * pair is 20308 > 0x3000 apart, so the midpoint -82 is committed. */
    CHECK_EQ_INT((int)DSD(DS_000F0AF0), 0x4000 - 82);

    /* B, slot a: side 1 (the lower +0x34) moved left past its latch. EAX is
     * side 1, so its own anchor 381 (AB0 256) gives 6148 - 256; side 0 stays. */
    cs_seed(0, 27000, 27000, 370);
    cs_seed(1, 6000, 6148, 381);
    DSD(DS_000F0AF0) = 0x4000;
    DSW(DS_000F0AFC) = 0x400;
    camera_dispatch();
    CHECK_EQ_INT((int)DSD(s1 + 0x34u), 6148);
    CHECK_EQ_INT((int)DSD(s1 + 0x2Cu), 6148);
    CHECK_EQ_INT((int)DSD(p1 + 0x18u), 6148 - 256);
    CHECK_EQ_INT((int)DSD(DS_00100AF4), 381);
    CHECK_EQ_INT((int)DSD(0x00100AB8u), 256);
    CHECK_EQ_INT((int)DSD(0x00100ABCu), 48 * 64);
    CHECK_EQ_INT((int)DSD(s0 + 0x2Cu), 0x7770);
    CHECK_EQ_INT((int)DSD(p0 + 0x18u), 0x5550);
    CHECK_EQ_INT((int)DSD(DS_00100AF0), 0x3330);
    CHECK_EQ_INT((int)DSD(0x00100AB0u), 0x1230);

    /* C, the gate is `jle` (0x12EE4): exactly 0x5000 apart does not split. */
    cs_seed(0, 6148 + 0x5000, 26456, 370);
    cs_seed(1, 6148, 6148, 381);
    DSD(DS_000F0AF0) = 0x4000;
    DSW(DS_000F0AFC) = 0x400;
    camera_dispatch();
    CHECK_EQ_INT((int)DSD(s0 + 0x34u), 6148 + 0x5000);
    CHECK_EQ_INT((int)DSD(s0 + 0x2Cu), 0x7770);
    CHECK_EQ_INT((int)DSD(p0 + 0x18u), 0x5550);
    CHECK_EQ_INT((int)DSD(DS_00100AF0), 0x3330);

    /* D, slot+0x42 bit 3 makes 0x18714 return the record's own +0x18
     * (0x18729/0x18738): the slot is still pulled back, the record is not. */
    cs_seed(0, 26674, 26456, 370);
    cs_seed(1, 6148, 6148, 381);
    DSB(s0 + 0x42u) = 0x08;
    DSD(DS_000F0AF0) = 0x4000;
    DSW(DS_000F0AFC) = 0x400;
    camera_dispatch();
    CHECK_EQ_INT((int)DSD(s0 + 0x2Cu), 26456);
    CHECK_EQ_INT((int)DSD(p0 + 0x18u), 0x5550);
    CHECK_EQ_INT((int)DSD(DS_00100AF0), 0x3330);

    DSB(DS_000F0AFE) = 4;
    fight_reset_recs();
    fight_reset_actors();
}

/* 0x12DA8/0x1317C/0x12CD4 exercised through camera_scene_step: the selected
 * player y commits to DS_001078F4 and camera y steps toward the curve target. */
static void check_y_commit(void)
{
    DSW(DS_000EF6DC) = 1;                       /* dust gate closed */
    DSD(DS_001077E0) = 0;
    DSD(DS_00107874) = 0;
    DSW(DS_00104AFC) = 0;

    /* Mode 2 selects the signed max: 0x12345678 over the negative 0x9ABCDEF0. */
    DSB(DS_000F0AFE) = 2;
    DSD(DS_001077E0) = 0x12345678u;
    DSD(DS_00107874) = 0x9ABCDEF0u;
    DSW(DS_001078F2 + 2u) = 0;
    camera_scene_step();
    CHECK_EQ_INT((int)DSW(DS_001078F2 + 2u), 0x5678);

    /* Mode 2 max, not min: 0x3000 over 0x1000. */
    DSD(DS_001077E0) = 0x00001000u;
    DSD(DS_00107874) = 0x00003000u;
    DSW(DS_001078F2 + 2u) = 0;
    camera_scene_step();
    CHECK_EQ_INT((int)DSW(DS_001078F2 + 2u), 0x3000);

    /* Mode 0 reads slot[DS_000F0AFF] + 0x30. */
    DSB(DS_000F0AFE) = 0;
    DSB(DS_000F0AFF) = 1;
    DSW(DS_001077E0 + 0x94u) = 0x2222;
    DSW(DS_001078F2 + 2u) = 0;
    camera_scene_step();
    CHECK_EQ_INT((int)DSW(DS_001078F2 + 2u), 0x2222);

    /* 0x12CD4 step down: target 0 (x = 0), camera y 0x250 -> 0x150. */
    DSB(DS_000F0AFE) = 2;
    DSD(DS_001077E0) = 0;
    DSD(DS_00107874) = 0;
    DSW(DS_000F0AF4) = 0;
    DSD(DS_001078F2) = 0;
    DSD(DS_000F0AEC) = 0x250;
    camera_scene_step();
    CHECK_EQ_INT((int)DSD(DS_000F0AEC), 0x150);

    /* 0x12CD4 adds the sign-extended shake offset DS_000F0AF4. */
    DSD(DS_000F0AEC) = 0x200;
    DSW(DS_000F0AF4) = 0x0020;
    camera_scene_step();
    CHECK_EQ_INT((int)DSD(DS_000F0AEC), 0x120);

    /* 0x12CD4 step up: x = 0x2000 gives a curve target > 0x100. */
    DSB(DS_000F0AFE) = 0;
    DSB(DS_000F0AFF) = 0;
    DSD(DS_001077E0) = 0x00002000u;
    DSW(DS_000F0AF4) = 0;
    DSD(DS_000F0AEC) = 0;
    camera_scene_step();
    CHECK_EQ_INT((int)DSD(DS_000F0AEC), 0x100);

    /* 0x12CD4 snap: x = 0x1800 gives the curve target 37 (< 0x100). */
    DSD(DS_001077E0) = 0x00001800u;
    DSD(DS_000F0AEC) = 0;
    camera_scene_step();
    CHECK_EQ_INT((int)DSD(DS_000F0AEC), 37);
}

/* 0x1282C: the dust gate. Closed on any non-multiple of 0x40; open on a 0x40
 * frame advances the RNG once when rng(7)&3 != 0. */
static void check_dust_gate(void)
{
    DSB(DS_000F0AFE) = 4;
    DSD(DS_000EF6D8) = 0x5A5Au;
    DSW(DS_000EF6DC) = 1;                       /* 1 & 0x3F != 0 */
    camera_scene_step();
    CHECK_EQ_INT((int)DSD(DS_000EF6D8), 0x5A5A);

    DSD(DS_000EF6D8) = 0x1234u;                 /* rng(7) = 5, &3 = 1 */
    DSW(DS_000EF6DC) = 0x40;                    /* 0x40 & 0x3F == 0 */
    camera_scene_step();
    CHECK_EQ_INT((int)DSD(DS_000EF6D8), (int)0xBAC6D4B3u);
}

/* 0x16D58: the per-side screen base and its range guards. */
static void check_screen_base(void)
{
    DSD(DS_00100A70) = 0xDEADBEEFu;
    DSD(DS_00100A98) = 0xDEADBEEFu;
    camera_screen_base(0, 3);
    CHECK_EQ_INT((int)DSD(DS_00100A70), 3 * 0x400 + 0xCC300);
    CHECK_EQ_INT((int)DSD(DS_00100A98), 3 * 0x3E8 + 0xC9BF0);

    DSD(DS_00100A70) = 0xDEADBEEFu;
    DSD(DS_00100A98) = 0xDEADBEEFu;
    camera_screen_base(5, 3);                   /* side out of [0,1] */
    camera_screen_base(0, 0xA);                 /* char out of [0,0xA) */
    CHECK_EQ_INT((int)DSD(DS_00100A70), (int)0xDEADBEEFu);
    CHECK_EQ_INT((int)DSD(DS_00100A98), (int)0xDEADBEEFu);
}

/* 0x17580: the four decays, the three zeroed globals and the countdowns. */
static void check_decay(void)
{
    DSD(DS_00100B08) = 0x1000;
    DSD(DS_00100B0C) = 0;
    DSD(DS_00100B00) = 0;
    DSD(DS_00100B04) = 0;
    DSD(DS_00100B54) = 0xDEADBEEFu;
    DSD(DS_00100AF8) = 0xDEADBEEFu;
    DSD(DS_00100AFC) = 0xDEADBEEFu;
    DSW(DS_00107824) = 5;
    DSW(DS_00107826) = 0;
    DSW(DS_001078B8) = 1;
    DSW(DS_001078BA) = 0;
    camera_decay();
    CHECK_EQ_INT((int)DSD(DS_00100B08), 0xF3D);
    CHECK_EQ_INT((int)DSD(DS_00100B00), 0);
    CHECK_EQ_INT((int)DSD(DS_00100B54), 0);
    CHECK_EQ_INT((int)DSD(DS_00100AF8), 0);
    CHECK_EQ_INT((int)DSD(DS_00100AFC), 0);
    CHECK_EQ_INT((int)DSW(DS_00107824), 4);
    CHECK_EQ_INT((int)DSW(DS_00107826), 0);
    CHECK_EQ_INT((int)DSW(DS_001078B8), 0);
    CHECK_EQ_INT((int)DSW(DS_001078BA), 0);

    /* The 0xD56 scale on B00. */
    DSD(DS_00100B08) = 0;
    DSD(DS_00100B00) = 0x1000;
    camera_decay();
    CHECK_EQ_INT((int)DSD(DS_00100B00), 0xD56);

    /* The signed divide truncates toward zero, not floor. */
    DSD(DS_00100B08) = 0xFFFFF000u;             /* -0x1000 */
    camera_decay();
    CHECK_EQ_INT((int)DSD(DS_00100B08), (int)0xFFFFF0C4u);   /* -0xF3C */
}

/* 0x140E4: the box-overlap bool and its 0x14080 intersection. Reuses the
 * check_projection fake resource: the sprite table's index 1 (0xA8B34) holds
 * the handle 0xF8045DC, whose resource entry 31 resolves into the scratch
 * sprite (width 64, height 64, origin 0). The actor indices are the raw's
 * table indices 1 and 2 (slot+0x56), not side numbers. */
static void check_box_overlap(void)
{
    u32 saved_tab = DSD(DS_001014E0), saved_n = DSD(DS_001014F0);
    u32 saved_actors = DSD(DS_001014EC);
    u32 a0 = FIGHT_ACTORS + 1u * 0x20u;
    u32 a1 = FIGHT_ACTORS + 2u * 0x20u;
    u32 tab = FIGHT_RECS + 0x2000u;
    u32 sbase = FIGHT_RECS + 0x1000u;
    u32 sprite = sbase + (0xF8045DCu & 0x7FFFFFu);

    mem_fill(tab, 0, 32u * 0x14u);
    DSD(DS_001014E0) = tab;
    DSD(DS_001014F0) = 32;
    DSD(tab + 31u * 0x14u + 16u) = sbase;
    mem_fill(sprite, 0, 8u);
    DSW(sprite) = 0x40;                         /* (s16)word[sprite] = 64 */
    DSB(sprite + 6u) = 0x40;                    /* DSD(sprite+4) >> 16 = 64 */
    mem_fill(FIGHT_ACTORS, 0, 0x80);
    DSD(DS_001014EC) = FIGHT_ACTORS;

    /* Identical boxes: the max/min intersection is non-empty. */
    DSW(a0) = 1; DSD(a0 + 4u) = 0; DSD(a0 + 8u) = 0;
    DSW(a1) = 1; DSD(a1 + 4u) = 0; DSD(a1 + 8u) = 0;
    CHECK_EQ_INT(camera_box_overlap(1, 2), 1);

    /* A two-pixel x offset still overlaps (0x800 >> 6 = 2). */
    DSD(a1 + 4u) = 0x00000080u;
    CHECK_EQ_INT(camera_box_overlap(1, 2), 1);

    /* The bit-15 origin flip (0x14132..0x14137) shifts actor 0 left by
     * w - local_a - 1 = 63, so the same pair no longer overlaps. */
    DSW(a0) = 0x8001;
    CHECK_EQ_INT(camera_box_overlap(1, 2), 0);

    /* Far apart: 0x14080's empty-intersection return. */
    DSW(a0) = 1;
    DSD(a1 + 4u) = 0x00080000u;
    CHECK_EQ_INT(camera_box_overlap(1, 2), 0);
    CHECK_EQ_INT(camera_box_overlap(2, 1), 0);

    DSD(DS_001014E0) = saved_tab;
    DSD(DS_001014F0) = saved_n;
    DSD(DS_001014EC) = saved_actors;
}

/* The two B08/B00 pairs camera_decay scales: re-seeded before each call so the
 * decayed values (0xF3D / 0xD56) and the 0x181D0 offsets (0) are pinned. */
static void unfreeze_seed_b(void)
{
    DSD(DS_00100B08) = 0x1000;
    DSD(DS_00100B0C) = 0x1000;
    DSD(DS_00100B00) = 0x1000;
    DSD(DS_00100B04) = 0x1000;
}

/* 0x17580/0x170A0: the unfreeze half. The 0x140E4 overlap gate selects the
 * 0x170A0 calls; each side's call writes DS_00100AE0/AD8/AE8 (before the
 * 0x16DA4 sprite path) and DS_00100AF8[side] = DS_00100B54. The AE0 sentinel
 * distinguishes "0x170A0 ran" from "the gate or the guard returned", so the
 * B60/B61 gates and the 0x170C5/0x170E2 guard are pinned without a fitted
 * number. B54's value is the record's named gap §7.3, so the invariant
 * AF8 == B54 and B54 != 0 are asserted instead. The fixture: both actors use
 * sprite-table index 4 (0xA8B30[4]) resolved to a fake 8x8 sprite whose pixel
 * stream is eight all-on rows, so 0x16DA4 accumulates a non-zero overlap. */
static void check_unfreeze(void)
{
    u8  s_b[0x9C];                              /* 0x100B64..0x100C00 */
    u8  s_row0[0x130], s_row1[0x130];          /* 0xFD160 / 0xFEDE0 rows */
    u32 s_res_tab = DSD(DS_001014E0), s_res_cnt = DSD(DS_001014F0);
    u32 s_actor_tab = DSD(DS_001014EC);
    u32 s_824 = DSW(DS_00107824), s_826 = DSW(DS_00107826);
    u32 s_8b8 = DSW(DS_001078B8), s_8ba = DSW(DS_001078BA);
    u32 s_8b8s = DSW(DS_00107824 + 0x94u), s_8bas = DSW(DS_00107826 + 0x94u);
    u32 s_8b8b = DSW(DS_001078B8 + 0x94u), s_8bab = DSW(DS_001078BA + 0x94u);
    u8  s_60 = DSB(DS_00100B60), s_61 = DSB(DS_00100B61);
    u8  s_2a = DSB(DS_0010782A), s_be = DSB(DS_001078BE);
    u32 s_af0 = DSD(DS_00100AF0), s_af4 = DSD(DS_00100AF4);
    u32 tab = FIGHT_RECS + 0x2000u;
    u32 sbase = FIGHT_RECS + 0x1000u;
    u32 pixelbase = FIGHT_RECS + 0x1800u;
    u32 handle = DSD(DS_000A8B30 + 4u * 4u);
    u32 sprite = sbase + (handle & 0x7FFFFFu);

    tf_snap(s_b, DS_00100B64, sizeof s_b);
    tf_snap(s_row0, 0x000FD160u, sizeof s_row0);
    tf_snap(s_row1, 0x000FEDE0u, sizeof s_row1);

    /* The two scratch records and actors. Actor word0 = 4 makes the sprite
     * table index 4 (char 0's constant 0xEE4 + AF0) and 0x100AF0 the same. */
    mem_fill(FIGHT_RECS, 0, 0x400);
    fight_reset_bases();
    fight_reset_actors();
    DSW(FIGHT_RECS + 0x56u) = 1;
    DSW(FIGHT_RECS + 0x100u + 0x56u) = 2;
    DSW(FIGHT_RECS + 0x28u) = 0;
    DSW(FIGHT_RECS + 0x100u + 0x28u) = 0;
    DSW(FIGHT_ACTORS + 1u * 0x20u) = 4;
    DSW(FIGHT_ACTORS + 2u * 0x20u) = 4;
    DSD(FIGHT_ACTORS + 1u * 0x20u + 4u) = 0;
    DSD(FIGHT_ACTORS + 1u * 0x20u + 8u) = 0x2000u;
    DSD(FIGHT_ACTORS + 2u * 0x20u + 4u) = 0;
    DSD(FIGHT_ACTORS + 2u * 0x20u + 8u) = 0x2000u;
    /* The slot char bytes (real slot region) and the two 0x100AF0 indices:
     * char 0's constant is 0xEE4, so index 4 - 0xEE4 makes the sprite-table
     * index and the palette-table index both 4. */
    DSB(DS_0010782A) = 0;
    DSB(DS_001078BE) = 0;
    DSD(DS_00100AF0) = (u32)(4 - 0x0EE4);
    DSD(DS_00100AF4) = (u32)(4 - 0x0EE4);

    /* The fake resource table: entry 31 = the sprite base, entry 30 = the
     * pixel stream. The sprite's +8 names entry 30. */
    mem_fill(tab, 0, 32u * 0x14u);
    DSD(DS_001014E0) = tab;
    DSD(DS_001014F0) = 32;
    DSD(tab + 31u * 0x14u + 16u) = sbase;
    DSD(tab + 30u * 0x14u + 16u) = pixelbase;
    mem_fill(sprite, 0, 0x10);
    DSD(sprite) = 0x00080008u;                  /* width 8, height 8 */
    DSD(sprite + 8u) = 0x0F000000u;             /* entry 30, offset 0 */
    mem_fill(pixelbase, 0, 0x60);
    for (u32 r = 0; r < 8u; r++) DSB(pixelbase + r * 9u) = 0x08u;

    /* The four screen boxes (0x100AC0/4/8/C): raw (0,36,2,3) clips to a
     * positive 8x8 box against the sprite rect. */
    for (u32 i = 0; i < 4u; i++) {
        DSB(DS_00100AC0 + i * 4u + 0u) = 0;
        DSB(DS_00100AC0 + i * 4u + 1u) = 36;
        DSB(DS_00100AC0 + i * 4u + 2u) = 2;
        DSB(DS_00100AC0 + i * 4u + 3u) = 3;
    }
    DSB(DS_00100B62) = 0;
    DSB(DS_00100B63) = 0;
    mem_fill(DS_00100BAE, 0xFF, 0x25u);
    mem_fill(DS_00100B64, 0xFF, 0x25u);
    mem_fill(DS_00100BD3, 0xFF, 0x25u);

    unfreeze_seed_b();

    /* A: disjoint boxes (actor 1 far right): the 0x140E4 gate fails, so no
     * 0x170A0 runs and AE0 keeps its sentinel. */
    DSD(FIGHT_ACTORS + 2u * 0x20u + 4u) = 0x00080000u;
    DSB(DS_00100B60) = 1;
    DSB(DS_00100B61) = 1;
    DSD(DS_00100B54) = 0xDEADBEEFu;
    DSD(DS_00100AF8) = 0xDEADBEEFu;
    DSD(DS_00100AFC) = 0xDEADBEEFu;
    DSD(DS_00100AE0) = 0xDEADBEEFu;
    DSD(DS_00100AE0 + 4u) = 0xDEADBEEFu;
    unfreeze_seed_b();
    camera_decay();
    CHECK_EQ_INT((int)DSD(DS_00100B54), 0);
    CHECK_EQ_INT((int)DSD(DS_00100AF8), 0);
    CHECK_EQ_INT((int)DSD(DS_00100AFC), 0);
    CHECK_EQ_INT((int)DSD(DS_00100AE0), (int)0xDEADBEEFu);
    CHECK_EQ_INT((int)DSD(DS_00100AE0 + 4u), (int)0xDEADBEEFu);

    /* B: overlap, B60 = 0, B61 = 1. Side 0's 0x170A0 is not called; side 1's
     * runs and writes AE0[1] = B08[1] (0x1000 decays to 0xF3D) and AF8[1]. */
    DSD(FIGHT_ACTORS + 2u * 0x20u + 4u) = 0;
    DSB(DS_00100B60) = 0;
    DSB(DS_00100B61) = 1;
    DSW(DS_00107824) = 0; DSW(DS_00107826) = 0;
    DSW(DS_00107824 + 0x94u) = 0; DSW(DS_00107826 + 0x94u) = 0;
    DSD(DS_00100AE0) = 0xDEADBEEFu;
    DSD(DS_00100AE0 + 4u) = 0xDEADBEEFu;
    DSD(DS_00100B54) = 0xDEADBEEFu;
    unfreeze_seed_b();
    camera_decay();
    CHECK_EQ_INT((int)DSD(DS_00100AE0), (int)0xDEADBEEFu);   /* 0x176A8 */
    CHECK(DSD(DS_00100AE0 + 4u) != 0xDEADBEEFu, "B61 gate: side 1 0x170A0 ran");
    CHECK_EQ_INT((int)DSD(DS_00100AE0 + 4u), 0xF3D);
    CHECK(DSD(DS_00100B54) != 0u, "0x16DA4 accumulated a non-zero B54");
    CHECK_EQ_INT((int)DSD(DS_00100AF8), 0);
    CHECK_EQ_INT((int)DSD(DS_00100AF8 + 4u), (int)DSD(DS_00100B54));

    /* C: overlap, B60 = 1, B61 = 0: the mirror image. */
    DSB(DS_00100B60) = 1;
    DSB(DS_00100B61) = 0;
    DSD(DS_00100AE0) = 0xDEADBEEFu;
    DSD(DS_00100AE0 + 4u) = 0xDEADBEEFu;
    unfreeze_seed_b();
    camera_decay();
    CHECK(DSD(DS_00100AE0) != 0xDEADBEEFu, "B60 gate: side 0 0x170A0 ran");
    CHECK_EQ_INT((int)DSD(DS_00100AE0 + 4u), (int)0xDEADBEEFu);   /* 0x176B8 */
    CHECK_EQ_INT((int)DSD(DS_00100AF8 + 4u), 0);
    CHECK_EQ_INT((int)DSD(DS_00100AF8), (int)DSD(DS_00100B54));

    /* D: both gates: both 0x170A0 calls run and AF8[0] == AF8[1] == B54. */
    DSB(DS_00100B60) = 1;
    DSB(DS_00100B61) = 1;
    DSD(DS_00100AE0) = 0xDEADBEEFu;
    DSD(DS_00100AE0 + 4u) = 0xDEADBEEFu;
    unfreeze_seed_b();
    camera_decay();
    CHECK(DSD(DS_00100AE0) != 0xDEADBEEFu, "both: side 0 0x170A0 ran");
    CHECK(DSD(DS_00100AE0 + 4u) != 0xDEADBEEFu, "both: side 1 0x170A0 ran");
    CHECK_EQ_INT((int)DSD(DS_00100AF8), (int)DSD(DS_00100B54));
    CHECK_EQ_INT((int)DSD(DS_00100AF8 + 4u), (int)DSD(DS_00100B54));

    /* E: the 0x170C5 guard. Other side 0's countdown is 2, so camera_decay
     * leaves 1 and side 1's 0x170A0 returns before writing AE0/AF8; side 0
     * (other 1's countdown 0) still runs. */
    DSB(DS_00100B60) = 1;
    DSB(DS_00100B61) = 1;
    DSW(DS_00107824) = 2; DSW(DS_00107826) = 0;
    DSW(DS_00107824 + 0x94u) = 0; DSW(DS_00107826 + 0x94u) = 0;
    DSD(DS_00100AE0) = 0xDEADBEEFu;
    DSD(DS_00100AE0 + 4u) = 0xDEADBEEFu;
    unfreeze_seed_b();
    camera_decay();
    CHECK(DSD(DS_00100AE0) != 0xDEADBEEFu, "guard: side 0 ran");
    CHECK_EQ_INT((int)DSD(DS_00100AE0 + 4u), (int)0xDEADBEEFu);   /* 0x170C5 */
    CHECK_EQ_INT((int)DSD(DS_00100AF8 + 4u), 0);

    /* F: both boxes present, only side 0 runs, and B08[0] != B08[1]. The first
     * 0x181D0 call's p3 is (box_a[0] + B08[0]) - (box_b[0] + B08[1])
     * (0x172f8..0x17324); with B08[1] far larger it is negative enough that the
     * clip is empty, so 0x170A0 returns at the first sync and B1C/B14/B34 keep
     * their sentinels. Dropping either per-side offset makes p3 = box_a[0] -
     * box_b[0] = 0, the clip stays visible and those globals are written. */
    DSB(DS_00100B60) = 1;
    DSB(DS_00100B61) = 0;
    DSW(DS_00107824) = 0; DSW(DS_00107826) = 0;
    DSW(DS_00107824 + 0x94u) = 0; DSW(DS_00107826 + 0x94u) = 0;
    unfreeze_seed_b();
    DSD(DS_00100B08) = 0x1000;                  /* decays to 0xF3D */
    DSD(DS_00100B08 + 4u) = 0x2000;             /* decays to 0x1E7A */
    DSD(DS_00100B1C) = 0xDEADBEEFu;
    DSD(DS_00100B14) = 0xDEADBEEFu;
    DSD(DS_00100B34) = 0xDEADBEEFu;
    camera_decay();
    CHECK_EQ_INT((int)DSD(DS_00100AE0), 0xF3D);              /* body ran */
    CHECK_EQ_INT((int)DSD(DS_00100B1C), (int)0xDEADBEEFu);   /* 0x17324 p3 */
    CHECK_EQ_INT((int)DSD(DS_00100B14), (int)0xDEADBEEFu);
    CHECK_EQ_INT((int)DSD(DS_00100B34), (int)0xDEADBEEFu);

    /* G: the box_o == 0 branch (0x17182), the live demo path (0x100AC0 is 0
     * there). Zero the other side's box +2/+3 so box_o = 0 and the sp_b arm
     * runs; the body still completes and accumulates a non-zero B54. With the
     * branch removed the box_b arm gets a zero-height box and returns at the
     * first 0x181D0 sync, leaving B54 at 0. */
    DSB(DS_00100B60) = 0;
    DSB(DS_00100B61) = 1;
    DSW(DS_00107824) = 0; DSW(DS_00107826) = 0;
    DSW(DS_00107824 + 0x94u) = 0; DSW(DS_00107826 + 0x94u) = 0;
    DSB(DS_00100AC0 + 2u) = 0;          /* other side's box absent -> box_o = 0 */
    DSB(DS_00100AC0 + 3u) = 0;
    DSD(DS_00100AF8 + 4u) = 0xDEADBEEFu;
    unfreeze_seed_b();
    camera_decay();
    CHECK(DSD(DS_00100B54) != 0u, "box_o==0: B54 accumulated");
    CHECK_EQ_INT((int)DSD(DS_00100AF8 + 4u), (int)DSD(DS_00100B54));

    tf_put(s_b, DS_00100B64, sizeof s_b);
    tf_put(s_row0, 0x000FD160u, sizeof s_row0);
    tf_put(s_row1, 0x000FEDE0u, sizeof s_row1);
    DSD(DS_001014E0) = s_res_tab;
    DSD(DS_001014F0) = s_res_cnt;
    DSD(DS_001014EC) = s_actor_tab;
    DSW(DS_00107824) = (u16)s_824; DSW(DS_00107826) = (u16)s_826;
    DSW(DS_001078B8) = (u16)s_8b8; DSW(DS_001078BA) = (u16)s_8ba;
    DSW(DS_00107824 + 0x94u) = (u16)s_8b8s;
    DSW(DS_00107826 + 0x94u) = (u16)s_8bas;
    DSW(DS_001078B8 + 0x94u) = (u16)s_8b8b;
    DSW(DS_001078BA + 0x94u) = (u16)s_8bab;
    DSB(DS_00100B60) = s_60;
    DSB(DS_00100B61) = s_61;
    DSB(DS_0010782A) = s_2a;
    DSB(DS_001078BE) = s_be;
    DSD(DS_00100AF0) = s_af0;
    DSD(DS_00100AF4) = s_af4;
}

/* 0x263F4: the arena frame's order and its two observable contracts. The two
 * latch sentinels differ from the values they copy, so a missing latch fails;
 * the 0x19068 pass stores a record float and clears the record's +0x20, so a
 * missing fighter update fails. */
static void check_arena_frame(void)
{
    u32 p0 = tf_demo_fixture();

    fight_arena_frame();

    CHECK_EQ_INT((int)DSD(DS_001077E8), 0x1111);
    CHECK_EQ_INT((int)DSD(DS_0010787C), 0x3333);
    CHECK_EQ_INT((int)DSD(p0 + 0x24u), 0x40400000);   /* (float)3.0 */
    CHECK_EQ_INT((int)DSD(p0 + 0x20u), 0);
}

/* 0x2A17C: a zero handle returns at 0x2A1AC (`test ebx,ebx / je 0x2A1F8`) and
 * leaves the pset's palette entry (+0x18) unchanged; the 0x2A1F5 store is dead.
 * The +0x18 sentinel differs from any acquired entry, so the old clearing
 * behaviour fails here. The +2 write still runs. */
static void check_pset_palette_zero_handle(void)
{
    u32 rec = DSD(DS_001014F4);                     /* a valid pool record */
    if (rec == 0) { CHECK(0, "actor pool base set"); return; }
    u16 saved_56 = DSW(rec + 0x56u);
    u8  saved_5f = DSB(rec + 0x5fu);

    fight_reset_actors();
    DSW(rec + 0x56u) = 0;                           /* pset slot 0 */
    DSB(rec + 0x5fu) = 0;
    DSD(FIGHT_ACTORS + 0x18u) = 0xDEADBEEFu;        /* pset+0x18 sentinel */
    DSW(FIGHT_ACTORS + 2u) = 0xAAAAu;               /* pset+2 sentinel */

    actor_pset_palette(rec, 0x1234u, 0u);
    CHECK_EQ_INT((int)DSD(FIGHT_ACTORS + 0x18u), (int)0xDEADBEEFu);
    CHECK_EQ_INT((int)DSW(FIGHT_ACTORS + 2u), 0x1234);

    DSW(rec + 0x56u) = saved_56;
    DSB(rec + 0x5fu) = saved_5f;
}

/* Task 6 Step 5: with the spawn live, the game_frame tail's per-side
 * 0x186D0/0x2A690 calls run on the spawned records. State 6 spawns both sides,
 * then a state-7 game_frame syncs P0's record pset (0x2A690 writes rec+0x3C
 * from pset+4), so a seeded rec+0x3C sentinel must move; a skipped spawn leaves
 * DS_001077A8 null and the record untouched. */
static void check_arena_frame_live(void)
{
    u32 rec0;

    (void)tf_demo_fixture();
    DSB(DS_00104B1D) = 1;               /* skip the coin poll */
    DSD(DS_001088E4) = 0;
    DSB(DS_00104528 + 1u) = 2;          /* skip the text rows */
    actors_reset();                     /* deterministic pool for the spawn */

    /* State 6 (0x11A8C) runs the spawn; its slot stores are the live fixture.
     * The sentinels are readable in-range offsets that differ from the slots,
     * so a skipped spawn fails the assertions without an out-of-range deref. */
    DSD(DS_001077A8) = FIGHT_RECS;
    DSD(DS_001077A8 + 4u) = FIGHT_RECS + 0x100u;
    fight_reset_state6();
    rng_seed(0x1234u);
    DSW(DS_000F0A64) = 6;
    game_state_step();

    CHECK_EQ_INT((int)DSD(DS_001077A8), (int)DS_001077B0);
    CHECK_EQ_INT((int)DSD(DS_001077A8 + 4u), (int)(DS_001077B0 + 0x94u));
    rec0 = DSD(DS_001077B0);
    CHECK(rec0 != 0, "live P0 record");
    if (rec0 == 0) return;

    DSD(rec0 + 0x3Cu) = 0xDEADBEEFu;    /* no pset x can equal this */

    /* A state-7 game_frame runs 0x263F4, 0x33F08 and the DS_00104B15 tail
     * (0x12D48 then 0x186D0 + 0x2A690 per live side, then 0x33F08). */
    DSD(DS_00104B00) = 3;
    DSD(DS_00104AE8) = 0;
    DSW(DS_000F0A6A) = 100;
    DSB(DS_00104B15) = 1;
    game_frame();

    CHECK(DSD(rec0 + 0x3Cu) != 0xDEADBEEFu, "live slot record synced across a frame");
}

/* 0x18950: the move-connectivity query fighter_pass_a's winner gate makes. The
 * record §3's demo returns (0,1) rest on reading the odd 8-byte record's low
 * dword as 0x0000FF00; the raw bytes are 00 00 FF 00 (dword 0x00FF0000, bits
 * 16..23), so with the demo's states (slot0 0x0B, slot1 0x01) both queries
 * return 0 and the position compare decides (side 0), matching cycle-3 §10.2.
 * The row pointers are the shipped table at 0xA1290 (loaded by test_le). */
static void check_connect_query(void)
{
    /* The demo's characters and states (record §3.3). */
    DSB(DS_0010782A) = 0;               /* slot0 char */
    DSB(DS_001078BE) = 3;               /* slot1 char */
    DSB(DS_0010780F) = 0x0B;            /* slot0 state */
    DSB(0x001078A3u) = 0x01;            /* slot1 state */

    /* The raw's demo returns: 0x18950(0,1) = 0 and 0x18950(1,0) = 0. */
    CHECK_EQ_INT(fighter_connect_query(0u, 1u), 0);
    CHECK_EQ_INT(fighter_connect_query(1u, 0u), 0);

    /* Discriminating low-branch case: state0 = 0 lands on the even record
     * (dword 0x0000AAAA) and bit state1 = 3 is set -> 1. The stub returns 0. */
    DSB(DS_0010780F) = 0;
    DSB(0x001078A3u) = 3;
    CHECK_EQ_INT(fighter_connect_query(0u, 1u), 1);

    /* The state2 >= 0x20 branch reads the record's +4 dword. The pair
     * (char0 = 0, char1 = 1) is table index 1, whose records carry 0x00000200
     * there, so bit 9 is set and bit 8 is not. */
    DSB(DS_001078BE) = 1;
    DSB(DS_0010780F) = 0;
    DSB(0x001078A3u) = 0x29;            /* 1 << 9 */
    CHECK_EQ_INT(fighter_connect_query(0u, 1u), 1);
    DSB(0x001078A3u) = 0x28;            /* 1 << 8: clear */
    CHECK_EQ_INT(fighter_connect_query(0u, 1u), 0);

    /* The wiring: (state0, state1) = (0, 3) gives (bl, al) = (1, 0), so the raw
     * zeroes AFC (side 0 wins) even though the position words favour side 1;
     * the take-both-as-0 stub would zero AF8 instead. */
    DSB(DS_001078BE) = 3;
    DSB(DS_0010780F) = 0;
    DSB(0x001078A3u) = 3;
    DSB(DS_001078FA) = 2;
    DSB(0x00107803u) = 0;               /* slot0 +0x53: not 0x0A */
    DSB(0x00107897u) = 0;               /* slot1 +0x53 */
    DSW(DS_00107826) = 0;               /* slot0 +0x76: skip the anim gate */
    DSW(0x001078BAu) = 0;               /* slot1 +0x76 */
    DSB(0x001077F0u) = 0;               /* the +0x40 overrides clear */
    DSB(0x00107884u) = 0;
    DSD(DS_00100AF8) = 0x1111u;
    DSD(DS_00100AFC) = 0x2222u;
    DSW(0x00107838u) = 1;               /* the stub would pick side 1 */
    DSW(0x001078CCu) = 0;
    DSB(0x0010783Au) = 0;               /* no winner body */
    DSB(0x001078CEu) = 0;

    fighter_pass_a();
    CHECK_EQ_INT((int)DSD(DS_00100AF8), 0x1111);   /* 0x1969A: AFC cleared */
    CHECK_EQ_INT((int)DSD(DS_00100AFC), 0);
}

/* 0x1958C: a side whose +0x803 state byte is 0x0A clears its DS_00100AF8
 * entry. The sentinel differs from the post-condition. */
static void check_fighter_pass_a(void)
{
    DSD(DS_001014EC) = FIGHT_ACTORS;
    fight_reset_bases();
    DSB(DS_001078FA) = 2;
    DSD(DS_00100AF8) = 0xDEADBEEFu;
    DSD(DS_00100AFC) = 0xDEADBEEFu;
    DSB(DS_00107803) = 0x0A;            /* side 0 leaves the fight */
    DSB(DS_00107803 + 0x94u) = 0;       /* side 1 stays */
    DSB(0x0010780Fu) = 0x40;            /* slot0 +0x5F >= 0x40: anim block off */
    DSB(0x001078A3u) = 0x40;            /* slot1 +0x5F */
    DSB(DS_001077F0) = 0;
    DSB(DS_00107884) = 0;
    /* Pin the winner comparison so only the 0x0A clear can zero the flag: the
     * lower +0x34 word makes the post-loop logic clear DS_00100AFC instead. */
    DSW(DS_00107838) = 0;
    DSW(DS_001078CC) = 1;
    DSB(DS_0010780A) = 0;
    DSB(DS_0010789E) = 0;
    DSB(DS_0010783A) = 0;
    DSB(DS_001078CE) = 0;

    fighter_pass_a();
    CHECK_EQ_INT((int)DSD(DS_00100AF8), 0);
}

/* 0x19068: the +0x5E timer and the +0x5A stance timer arm the record's
 * animation float from +0x5C and clear the record's +0x20. */
static void check_fighter_pass_b(void)
{
    u32 p0 = FIGHT_RECS;

    fight_reset_recs();
    DSB(DS_00107802) = 0;
    DSB(DS_00107896) = 0;
    DSB(DS_00107EE0) = 0;               /* 0x3C570(5) returns 0 (frame path) */
    DSB(0x0010780Fu) = 0;
    DSB(DS_00100B58) = 0;
    DSB(DS_00100B5E) = 1;               /* -> 0 */
    DSB(DS_00100B5A) = 1;               /* == 1 -> the float store */
    DSB(DS_00100B5C) = 3;
    DSD(p0 + 0x24u) = 0;
    DSD(p0 + 0x20u) = 0xDEADBEEFu;
    DSB(0x001078A3u) = 0;
    DSB(DS_00100B58 + 1u) = 0x55;            /* side 1 skips through the gap */

    fighter_pass_b(1);                  /* arg != 0 bypasses the 0x3C570 gate */
    CHECK_EQ_INT((int)DSB(DS_00100B5E), 0x0A);
    CHECK_EQ_INT((int)DSD(p0 + 0x24u), 0x40400000);
    CHECK_EQ_INT((int)DSD(p0 + 0x20u), 0);

    /* 0xFF is a real stance value (0x3BDDC writes it): it must take the 0x1922C
     * skip even when it equals DS_00100B58[side]. The timer and record fields
     * keep their sentinels. */
    DSB(0x0010780Fu) = 0xFF;
    DSB(DS_00100B58) = 0xFF;
    DSB(DS_00100B5E) = 5;
    DSB(DS_00100B5A) = 1;
    DSB(DS_00100B5C) = 3;
    DSD(p0 + 0x24u) = 0xDEADBEEFu;
    DSD(p0 + 0x20u) = 0x12345678u;
    DSB(0x001078A3u) = 0xFF;
    DSB(DS_00100B58 + 1u) = 0xFF;

    fighter_pass_b(1);
    CHECK_EQ_INT((int)DSB(DS_00100B5E), 5);
    CHECK_EQ_INT((int)DSD(p0 + 0x24u), (int)0xDEADBEEFu);
    CHECK_EQ_INT((int)DSD(p0 + 0x20u), 0x12345678);
}

/* 0x35658: the HUD pass's `rec+0x28 = *rec+0x18` and `*rec+0x28 |= 1` walk the
 * two-level record DS_001077A8[side] -> fighter. Seeded so the wrong base (rec
 * itself rather than *rec) fails. */
static void check_hud_pass(void)
{
    u32 rec = FIGHT_RECS + 0x400u;
    u32 fighter = FIGHT_RECS + 0x500u;

    mem_fill(FIGHT_RECS + 0x400u, 0, 0x180);
    DSD(DS_001077A8) = rec;
    DSD(DS_001077A8 + 4u) = 0;          /* side 1: no record */
    DSD(rec) = fighter;                 /* the two-level pointer */
    DSD(rec + 0x18u) = 0x0000CCCCu;     /* a wrong-base source would copy this */
    DSD(rec + 0x28u) = 0x0000BBBBu;     /* sentinel, differs from the copy */
    DSD(fighter + 0x18u) = 0x0000AAAAu;
    DSB(fighter + 0x28u) = 0;
    DSB(rec + 0x52u) = 0;               /* 0x34B6C does not dispatch to 0x1A978 */
    DSD(DS_00104B00) = 3;               /* not 4: the mode-4 arm is skipped */

    fight_hud_pass(0);
    CHECK_EQ_INT((int)DSD(rec + 0x28u), 0x0000AAAAu);
    CHECK_EQ_INT((int)DSB(fighter + 0x28u), 1);

    /* The mode is the 16-bit word at 0x104B00; DS_00104B02 is a separate global.
     * A dword read would see 0x00010004 and miss the mode-4 arm, run the spine
     * and overwrite both sentinels. */
    DSW(DS_00104B00) = 4;
    DSW(DS_00104B02) = 1;
    DSB(rec + 0x43u) = 0;               /* the mode-4 arm needs bit 0x80 clear */
    DSD(rec + 0x28u) = 0x0000BBBBu;
    DSB(fighter + 0x28u) = 0;
    fight_hud_pass(0);
    CHECK_EQ_INT((int)DSD(rec + 0x28u), 0x0000BBBBu);
    CHECK_EQ_INT((int)DSB(fighter + 0x28u), 0);
}

/* 0x35813 0x2A1FC: the HUD pass's `|= 1` is immediately followed by the
 * per-record sync (the raw's `mov eax,ebx; call 0x2A1FC` with ebx = the fighter
 * record), so the fighter's animation advances inside the state-7 arena's
 * 0x2651B HUD pass. sync_record's 0x2AA70 frame_timer decrements the record's
 * +0x20 float by 1.0. The spine before it is seeded inert (no 0x52 dispatch,
 * mode != 4) and +0x24 is non-zero so frame_timer runs; the 3.0f sentinel
 * differs from the 2.0f post-condition, so the omitted sync fails here. */
static void check_hud_sync(void)
{
    u32 rec = FIGHT_RECS + 0x400u;
    u32 fighter = FIGHT_RECS + 0x500u;

    mem_fill(FIGHT_RECS + 0x400u, 0, 0x180);
    DSD(DS_001014EC) = FIGHT_ACTORS;
    DSD(DS_001077A8) = rec;
    DSD(DS_001077A8 + 4u) = 0;          /* side 1: no record */
    DSD(rec) = fighter;                 /* the two-level pointer */
    DSB(rec + 0x52u) = 0;               /* 0x34B6C does not dispatch to 0x1A978 */
    DSD(DS_00104B00) = 3;               /* not 4: the mode-4 arm is skipped */
    DSB(DS_00104B26) = 0;               /* the sync takes the timer path */

    DSW(fighter + 0x56u) = 0;           /* pset slot 0 */
    DSW(fighter + 0x28u) = 0;           /* bit 0 clear; bits 4/11 and +0x2a clear */
    DSW(fighter + 0x2au) = 0;
    DSD(fighter + 0x24u) = 0x3F800000u; /* 1.0f: the frame_timer precondition */
    DSD(fighter + 0x20u) = 0x40400000u; /* 3.0f sentinel */

    fight_hud_pass(0);
    CHECK_EQ_INT((int)DSD(fighter + 0x20u), 0x40000000);   /* 2.0f: synced */
    CHECK_EQ_INT((int)DSB(fighter + 0x28u), 1);
}

/* 0x35829 0x186C4: after the 0x35813 sync, the HUD pass re-latches BOTH slots
 * (0x186C4 = 0x186D0(0), then falls into 0x186D0 with EAX = 1). Slot+0x42 bit 3
 * takes the clean rec+0x18 copy and slot+0x41 bit 7 latches +0x2C into +0x34
 * (the camera's 0x12E3C input). The fighter carries an x velocity of 5
 * (rec+0x34, 0x2A516's dword+0x32 >> 16), so the latched x is the post-sync
 * 0xAAAF, not the pre-sync 0xAAAA: a latch before the sync fails. Side 1 has
 * no camera-target record (the pass returns early for it) yet is latched,
 * which is 0x186C4's second half. Sentinels differ from every post-state. */
static void check_hud_latch(void)
{
    u32 rec = FIGHT_RECS + 0x400u;
    u32 fighter = FIGHT_RECS + 0x500u;
    u32 other = FIGHT_RECS + 0x600u;
    u32 s0 = DS_001077B0;
    u32 s1 = DS_001077B0 + 0x94u;
    u32 sv_s0 = DSD(s0), sv_s1 = DSD(s1);
    u8 sv_42a = DSB(s0 + 0x42u), sv_42b = DSB(s1 + 0x42u);
    u8 sv_41a = DSB(s0 + 0x41u), sv_41b = DSB(s1 + 0x41u);
    u8 sv_40a = DSB(s0 + 0x40u);

    mem_fill(FIGHT_RECS + 0x400u, 0, 0x280);
    DSD(DS_001014EC) = FIGHT_ACTORS;
    DSD(DS_001077A8) = rec;
    DSD(DS_001077A8 + 4u) = 0;          /* side 1: no camera-target record */
    DSD(rec) = fighter;                 /* the two-level pointer */
    DSB(rec + 0x52u) = 0;               /* 0x34B6C does not dispatch to 0x1A978 */
    DSB(rec + 0x53u) = 1;               /* 0x3531C case 1: a bare return */
    DSD(DS_00104B00) = 3;               /* not 4: the mode-4 arm is skipped */
    DSB(DS_00104B26) = 0;

    DSW(fighter + 0x56u) = 0;           /* pset slot 0 */
    DSW(fighter + 0x28u) = 0;           /* motion_step and pset_write run */
    DSW(fighter + 0x2Au) = 0x8000u;     /* 0x2A501: the motion gate */
    DSD(fighter + 0x18u) = 0x0000AAAAu;
    DSD(fighter + 0x32u) = 0x00050000u; /* x velocity 5 (word +0x34) */
    DSD(other + 0x18u) = 0x0000BBBBu;

    DSD(s0) = fighter;
    DSD(s1) = other;
    DSB(s0 + 0x42u) = 0x08u;            /* bit 3: the clean copy */
    DSB(s1 + 0x42u) = 0x08u;
    DSB(s0 + 0x40u) = 0x80u;            /* 0x3BDF3: no attack consume */
    DSB(s0 + 0x41u) = 0x80u;            /* bit 7: +0x34 latches +0x2C */
    DSB(s1 + 0x41u) = 0x80u;
    DSD(s0 + 0x2Cu) = 0xDEADu;
    DSD(s0 + 0x34u) = 0xDEADu;
    DSD(s1 + 0x2Cu) = 0xDEADu;
    DSD(s1 + 0x34u) = 0xDEADu;

    fight_hud_pass(0);
    CHECK_EQ_INT((int)DSD(fighter + 0x18u), 0xAAAF);   /* the sync moved it */
    CHECK_EQ_INT((int)DSD(s0 + 0x2Cu), 0xAAAF);
    CHECK_EQ_INT((int)DSD(s0 + 0x34u), 0xAAAF);
    CHECK_EQ_INT((int)DSD(s1 + 0x2Cu), 0xBBBB);
    CHECK_EQ_INT((int)DSD(s1 + 0x34u), 0xBBBB);

    DSD(s0) = sv_s0;
    DSD(s1) = sv_s1;
    DSB(s0 + 0x42u) = sv_42a;
    DSB(s1 + 0x42u) = sv_42b;
    DSB(s0 + 0x41u) = sv_41a;
    DSB(s1 + 0x41u) = sv_41b;
    DSB(s0 + 0x40u) = sv_40a;
}

/* 0x49C78: the direct RNG call sites and their gates. A case-3 entry issues
 * exactly one rng(0x3C); the DS_001088BF tail issues one rng(2) only inside
 * 1..4. The RNG state is the proof; the sentinel seeds prove the gate. */
static void check_effects_rng(void)
{
    u32 entry = FIGHT_RECS + 0x3000u;
    u32 rec = FIGHT_RECS + 0x3100u;
    u32 stream = FIGHT_RECS + 0x3800u;
    u32 sv_tab = DSD(0x000C9634u);       /* the case-3 landing stream table */
    u32 saved;

    mem_fill(FIGHT_RECS, 0, 0x4000);
    DSW(stream) = 0x0123u;              /* a literal sprite id: the landing's
                                         * 0x2BC30 walks no RNG opcode */
    DSD(0x000C9634u) = stream;
    DSD(DS_001014EC) = FIGHT_ACTORS;
    fight_reset_bases();
    DSW(FIGHT_RECS + 0x56u) = 1;
    DSW(FIGHT_RECS + 0x100u + 0x56u) = 2;
    DSW(DS_00104B00) = 3;
    DSB(DS_001088BF) = 0;
    DSB(DS_00104AEC) = 0xFF;

    /* The one-entry list: 0x10884C -> entry -> 0x10884C. */
    DSD(DS_0010884C) = entry;
    DSD(entry) = DS_0010884C;
    DSB(entry + 0x21u) = 0;
    DSD(entry + 8u) = rec;
    DSD(entry + 0xCu) = DS_001077B0;    /* the landing's 0x2BE1C fighter */
    DSB(entry + 0x1Eu) = 3;
    DSB(rec + 0x48u) = 0x20;            /* the raw's `si` is 0 */
    DSD(rec + 0x30u) = 0;               /* the 0xBD898 gate passes */

    rng_seed(0x1234u);
    (void)rng_next(0x3Cu);
    saved = DSD(DS_000EF6D8);
    rng_seed(0x1234u);
    fight_effects_pass();
    CHECK_EQ_INT((int)DSD(DS_000EF6D8), (int)saved);

    /* An empty list draws nothing when the tail gate is closed. */
    DSD(DS_0010884C) = DS_0010884C;
    DSB(DS_001088BF) = 0;
    rng_seed(0x1234u);
    fight_effects_pass();
    CHECK_EQ_INT((int)DSD(DS_000EF6D8), 0x1234);

    /* The tail gate open (2, inside 1..4) issues one rng(2). */
    DSB(DS_001088BF) = 2;
    rng_seed(0x1234u);
    (void)rng_next(2u);
    saved = DSD(DS_000EF6D8);
    rng_seed(0x1234u);
    fight_effects_pass();
    CHECK_EQ_INT((int)DSD(DS_000EF6D8), (int)saved);
    CHECK_EQ_INT((int)DSB(DS_001088BF), 0);

    DSD(0x000C9634u) = sv_tab;
}

/* 0x49C78 case 1 (0x49D2F) and its arrival 0x4AC38: a walking type-1 entry
 * stops once |actor+0x18 - entry+0x14| is within one step, the magnitude of
 * the velocity word +0x34 read as `[+0x32] >> 16`. The first seed is the demo's
 * type-0x20 worshipper at f = 87 (x -4369, target -4303, step 0x80). The
 * arrival zeroes +0x34/+0x36/+0x38, clears hflip, sets +0x29 bit 0x10, returns
 * the entry to type 0 and begins the 0xC9544[index] stream with the hold 5.0.
 * +0x32's low word holds 0x1234, so a word read of +0x32 (4660) arrives
 * where the raw does not; the negative-velocity and d == step seeds pin the
 * negation and the signed `jg`. */
static void check_effects_arrival(void)
{
    u32 entry = FIGHT_RECS + 0x3000u;
    u32 rec = FIGHT_RECS + 0x3100u;
    u32 stream = FIGHT_RECS + 0x3800u;
    u32 pset = FIGHT_ACTORS + 3u * 0x20u;
    u32 sv_tab = DSD(0x000C9544u);       /* DS_000C9544: no symbols.h name */

    mem_fill(FIGHT_RECS, 0, 0x4000);
    DSD(DS_001014EC) = FIGHT_ACTORS;
    fight_reset_bases();
    DSW(FIGHT_RECS + 0x56u) = 1;
    DSW(FIGHT_RECS + 0x100u + 0x56u) = 2;
    DSW(DS_00104B00) = 3;
    DSB(DS_001088C2) = 0;
    DSB(DS_001088BF) = 0;
    DSW(stream) = 0x0123u;              /* a literal sprite id */
    DSD(0x000C9544u) = stream;

    DSD(DS_0010884C) = entry;
    DSD(entry) = DS_0010884C;
    DSD(entry + 8u) = rec;
    DSB(rec + 0x48u) = 0x20;            /* the raw's `si` is 0 */
    DSW(rec + 0x56u) = 3;

    /* Arrival: d = 66 <= 0x80. */
    DSB(entry + 0x1Eu) = 1;
    DSD(entry + 0x14u) = (u32)-4303;
    DSD(rec + 0x18u) = (u32)-4369;
    DSD(rec + 0x08u) = 0xEE13Au;
    DSD(rec + 0x24u) = 0x40400000u;
    DSD(rec + 0x32u) = 0x00801234u;     /* +0x34 = 0x80, +0x32 = 0x1234 */
    DSW(rec + 0x36u) = 0x5555u;
    DSW(rec + 0x38u) = 0x6666u;
    DSB(rec + 0x29u) = 0x4Bu;
    DSW(pset) = 0x07E1u;
    fight_effects_pass();
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 0);
    CHECK_EQ_INT((int)DSW(rec + 0x34u), 0);
    CHECK_EQ_INT((int)DSW(rec + 0x36u), 0);
    CHECK_EQ_INT((int)DSW(rec + 0x38u), 0);
    CHECK_EQ_INT((int)DSB(rec + 0x29u), 0x13);   /* 0x2BC30 also clears 0x08 */
    CHECK_EQ_INT((int)DSD(rec + 0x08u), (int)stream);
    CHECK_EQ_INT((int)DSD(rec + 0x24u), 0x40A00000);
    CHECK_EQ_INT((int)DSW(pset), 0x0123);

    /* Still walking: d = 194 > 0x80 (a word read of +0x32 would arrive). */
    DSB(entry + 0x1Eu) = 1;
    DSD(rec + 0x18u) = (u32)-4497;
    DSD(rec + 0x08u) = 0xEE13Au;
    DSD(rec + 0x32u) = 0x00801234u;
    DSW(pset) = 0x07E1u;
    fight_effects_pass();
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 1);
    CHECK_EQ_INT((int)DSW(rec + 0x34u), 0x80);
    CHECK_EQ_INT((int)DSD(rec + 0x08u), 0xEE13A);
    CHECK_EQ_INT((int)DSW(pset), 0x07E1);

    /* A leftward walker: +0x34 = -0x80 gives the step 0x80; d = 100 arrives. */
    DSD(entry + 0x14u) = 574;
    DSD(rec + 0x18u) = 674;
    DSD(rec + 0x32u) = 0xFF801234u;
    fight_effects_pass();
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 0);
    CHECK_EQ_INT((int)DSW(rec + 0x34u), 0);

    /* d == step arrives (the raw's `jg` leaves on d > step only). */
    DSB(entry + 0x1Eu) = 1;
    DSD(rec + 0x18u) = 702;
    DSD(rec + 0x32u) = 0xFF801234u;
    fight_effects_pass();
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 0);

    /* The DS_001088C2 pre-gate (0x49D2F): with it set and 0xC9754[index]
     * non-zero, 0x4BD4C retargets the entry to type 8 with the hold 4.0 before
     * the arrival test, although d == step would arrive. The tail (0x4A5A0)
     * clears DS_001088C2. With the 0xC9754 stream zero, 0x4BD4C declines and the
     * entry arrives. */
    {
        u32 stream8 = FIGHT_RECS + 0x3840u;
        u32 sv_tab8 = DSD(DS_000C9754);
        DSW(stream8) = 0x0456u;             /* a literal sprite id */
        DSD(DS_000C9754) = stream8;
        DSB(entry + 0x1Eu) = 1;
        DSD(rec + 0x18u) = 702;
        DSD(rec + 0x32u) = 0xFF801234u;
        DSD(rec + 0x24u) = 0x3F800000u;
        DSW(pset) = 0x07E1u;
        DSB(DS_001088C2) = 1;
        fight_effects_pass();
        CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 8);
        CHECK_EQ_INT((int)DSD(rec + 0x24u), 0x40800000);
        CHECK_EQ_INT((int)DSW(pset), 0x0456);
        CHECK_EQ_INT((int)DSB(DS_001088C2), 0);

        DSD(DS_000C9754) = 0;
        DSB(entry + 0x1Eu) = 1;
        DSD(rec + 0x32u) = 0xFF801234u;
        DSD(rec + 0x24u) = 0x3F800000u;
        DSB(DS_001088C2) = 1;
        fight_effects_pass();
        CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 0);
        CHECK_EQ_INT((int)DSD(rec + 0x24u), 0x40A00000);
        DSD(DS_000C9754) = sv_tab8;
    }

    DSD(0x000C9544u) = sv_tab;
    DSD(DS_0010884C) = DS_0010884C;
}

/* 0x49C78 cases 3, 4 and 5 (0x49DB3, 0x49E5A, 0x49EC0): a scared worshipper's
 * fall, lie and climb (record §28; the demo's side-1 worshippers land at f =
 * 675/676). The entry's `si` is 3 (+0x48 = 0x23) and the three stream tables'
 * entries 0 and 3 hold distinct literal sprite ids, so a wrong index fails.
 * Case 3 sets +0x2C = 0x496AC(y) every frame and lands at y <= the
 * zero-extended word DS_000BD898: hflip OR-ed in when 0x2BE1C(actor, fighter)
 * > 0, the 0xC9634[si] stream at 2.0, +0x38/+0x34 zeroed, the entry's +0x18 =
 * rng(0x3C) + 0x3C, +0x1C |= 0x80, type 4. Case 4 counts +0x18 down (signed)
 * and at zero takes the 0xC95EC[si] stream at 3.0, +0x38 = 0x40, +0x34 = -0x40
 * or 0x40 by the fighter's +0x28 bit 0x4000, clears +0x1C bit 7, type 5. Case
 * 5 sets +0x2C and, once the y word +0x32 >= the entry's +0x1A (signed), takes
 * the 0xC9544[si] stream at 3.0, zeroes +0x38/+0x34 and returns to type 0. */
static void check_effects_fall(void)
{
    u32 entry = FIGHT_RECS + 0x3000u;
    u32 rec = FIGHT_RECS + 0x3100u;
    u32 fighter = FIGHT_RECS;           /* DSD(DS_001077B0) after the reset */
    u32 st_land = FIGHT_RECS + 0x3800u, st_rise = FIGHT_RECS + 0x3840u;
    u32 st_idle = FIGHT_RECS + 0x3880u, st_wrong = FIGHT_RECS + 0x38C0u;
    u32 pset = FIGHT_ACTORS + 3u * 0x20u;
    u32 sv_l0 = DSD(0x000C9634u), sv_l3 = DSD(0x000C9634u + 12u);
    u32 sv_r0 = DSD(0x000C95ECu), sv_r3 = DSD(0x000C95ECu + 12u);
    u32 sv_i0 = DSD(0x000C9544u), sv_i3 = DSD(0x000C9544u + 12u);
    u16 sv_898 = DSW(DS_000BD898);
    u32 expect_t;

    mem_fill(FIGHT_RECS, 0, 0x4000);
    DSD(DS_001014EC) = FIGHT_ACTORS;
    fight_reset_bases();
    DSW(fighter + 0x56u) = 1;
    DSW(FIGHT_RECS + 0x100u + 0x56u) = 2;
    DSW(DS_00104B00) = 3;
    DSB(DS_001088C2) = 0;
    DSB(DS_001088BF) = 0;
    DSW(st_land) = 0x0321u;
    DSW(st_rise) = 0x0654u;
    DSW(st_idle) = 0x0987u;
    DSW(st_wrong) = 0x0BADu;
    DSD(0x000C9634u) = st_wrong;
    DSD(0x000C9634u + 12u) = st_land;
    DSD(0x000C95ECu) = st_wrong;
    DSD(0x000C95ECu + 12u) = st_rise;
    DSD(0x000C9544u) = st_wrong;
    DSD(0x000C9544u + 12u) = st_idle;

    DSD(DS_0010884C) = entry;
    DSD(entry) = DS_0010884C;
    DSD(entry + 8u) = rec;
    DSD(entry + 0xCu) = DS_001077B0;    /* the slot: its +0 is `fighter` */
    DSB(rec + 0x48u) = 0x23;            /* `si` = 3 */
    DSW(rec + 0x56u) = 3;

    /* Still falling: y = 0x800 > 0x400. Only +0x2C moves, to 0x496AC(0x800) =
     * (0xB00 - 0x800) / 2 + 0xC00 = 0xD80; no draw. */
    DSB(entry + 0x1Eu) = 3;
    DSW(entry + 0x18u) = 0x7777u;
    DSB(entry + 0x1Cu) = 0x05u;
    DSD(rec + 0x30u) = 0x08000000u;
    DSW(rec + 0x2Cu) = 0x9999u;
    DSW(rec + 0x28u) = 0x0011u;
    DSW(rec + 0x34u) = 0x0040u;
    DSW(rec + 0x38u) = 0xFFC0u;
    DSD(rec + 0x08u) = 0xEEFD4u;
    DSW(pset) = 0x07E1u;
    rng_seed(0x1234u);
    fight_effects_pass();
    CHECK_EQ_INT((int)DSW(rec + 0x2Cu), 0xD80);
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 3);
    CHECK_EQ_INT((int)DSW(rec + 0x38u), 0xFFC0);
    CHECK_EQ_INT((int)DSW(entry + 0x18u), 0x7777);
    CHECK_EQ_INT((int)DSD(rec + 0x08u), 0xEEFD4);
    CHECK_EQ_INT((int)DSW(pset), 0x07E1);
    CHECK_EQ_INT((int)DSD(DS_000EF6D8), 0x1234);

    /* y = 0x401: one above the gate, still falling. */
    DSD(rec + 0x30u) = 0x04010000u;
    fight_effects_pass();
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 3);

    /* The landing at y = 0x400 (the gate is `jg`): 0x2BE1C = 0x5000 - 0x1000
     * > 0 ORs in hflip before 0x2BC30 (0x49E0D, then 0x49E24), whose `and
     * word [rec+0x28], 0xF7EB` leaves 0x0011 | 0x4000 = 0x4001, and whose first
     * sprite id then carries the hflip bit 0x8000; +0x2C = 0xF80 (y <= 0x400). */
    DSD(rec + 0x30u) = 0x04000000u;
    DSD(FIGHT_ACTORS + 3u * 0x20u + 4u) = 0x5000u;  /* the actor's 0x2BE00 */
    DSD(FIGHT_ACTORS + 1u * 0x20u + 4u) = 0x1000u;  /* the fighter's 0x2BE00 */
    rng_seed(0x1234u);
    expect_t = rng_next(0x3Cu) + 0x3Cu;
    rng_seed(0x1234u);
    fight_effects_pass();
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 4);
    CHECK_EQ_INT((int)DSW(rec + 0x28u), 0x4001);
    CHECK_EQ_INT((int)DSD(rec + 0x08u), (int)st_land);
    CHECK_EQ_INT((int)DSD(rec + 0x24u), 0x40000000);
    CHECK_EQ_INT((int)DSW(pset), 0x8321);
    CHECK_EQ_INT((int)DSW(rec + 0x34u), 0);
    CHECK_EQ_INT((int)DSW(rec + 0x38u), 0);
    CHECK_EQ_INT((int)DSW(entry + 0x18u), (int)expect_t);
    CHECK_EQ_INT((int)DSB(entry + 0x1Cu), 0x85);
    CHECK_EQ_INT((int)DSW(rec + 0x2Cu), 0xF80);

    /* A landing with 0x2BE1C < 0 keeps hflip as it was (OR only): set stays
     * set, clear stays clear. y = -0x100 is signed: 0x496AC gives 0xF80. */
    DSD(FIGHT_ACTORS + 3u * 0x20u + 4u) = 0x1000u;
    DSD(FIGHT_ACTORS + 1u * 0x20u + 4u) = 0x5000u;
    DSD(rec + 0x30u) = 0xFF000000u;
    DSW(rec + 0x2Cu) = 0x9999u;
    DSB(entry + 0x1Eu) = 3;
    DSW(rec + 0x28u) = 0x4011u;
    fight_effects_pass();
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 4);
    CHECK_EQ_INT((int)DSW(rec + 0x28u), 0x4001);
    CHECK_EQ_INT((int)DSW(rec + 0x2Cu), 0xF80);
    DSB(entry + 0x1Eu) = 3;
    DSW(rec + 0x28u) = 0x0011u;
    DSW(pset) = 0x07E1u;
    fight_effects_pass();
    CHECK_EQ_INT((int)DSW(rec + 0x28u), 0x0001);
    CHECK_EQ_INT((int)DSW(pset), 0x0321);

    /* The gate word is zero-extended: with DS_000BD898 = 0x8000, y = 0x100
     * lands (a sign-extended -0x8000 would not). */
    DSW(DS_000BD898) = 0x8000u;
    DSD(rec + 0x30u) = 0x01000000u;
    DSB(entry + 0x1Eu) = 3;
    fight_effects_pass();
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 4);
    DSW(DS_000BD898) = sv_898;

    /* Case 4, the countdown: 2 -> 1 stays; 1 -> 0 rises. The fighter's +0x28
     * bit 0x4000 clear gives +0x34 = 0x40. */
    DSB(entry + 0x1Eu) = 4;
    DSW(entry + 0x18u) = 2;
    DSB(entry + 0x1Cu) = 0x85u;
    DSW(fighter + 0x28u) = 0x0000u;
    DSD(rec + 0x08u) = st_land;
    DSW(pset) = 0x07E1u;
    DSW(rec + 0x34u) = 0x1111u;
    DSW(rec + 0x38u) = 0x2222u;
    rng_seed(0x1234u);
    fight_effects_pass();
    CHECK_EQ_INT((int)DSW(entry + 0x18u), 1);
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 4);
    CHECK_EQ_INT((int)DSD(rec + 0x08u), (int)st_land);
    CHECK_EQ_INT((int)DSW(rec + 0x38u), 0x2222);
    fight_effects_pass();
    CHECK_EQ_INT((int)DSW(entry + 0x18u), 0);
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 5);
    CHECK_EQ_INT((int)DSD(rec + 0x08u), (int)st_rise);
    CHECK_EQ_INT((int)DSD(rec + 0x24u), 0x40400000);
    CHECK_EQ_INT((int)DSW(pset), 0x0654);
    CHECK_EQ_INT((int)DSW(rec + 0x38u), 0x40);
    CHECK_EQ_INT((int)DSW(rec + 0x34u), 0x40);
    CHECK_EQ_INT((int)DSB(entry + 0x1Cu), 0x05);
    CHECK_EQ_INT((int)DSD(DS_000EF6D8), 0x1234);

    /* The fighter's +0x28 bit 0x4000 set gives +0x34 = -0x40; the counter is
     * signed: 0x8001 - 1 = 0x8000 < 0 rises at once. */
    DSB(entry + 0x1Eu) = 4;
    DSW(entry + 0x18u) = 0x8001u;
    DSW(fighter + 0x28u) = 0x4000u;
    DSW(rec + 0x34u) = 0x1111u;
    fight_effects_pass();
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 5);
    CHECK_EQ_INT((int)DSW(rec + 0x34u), 0xFFC0);

    /* Case 5, the climb: y 0x800 < +0x1A 0x900 keeps climbing (+0x2C =
     * 0xD80); y 0x900 arrives (+0x2C = 0xD00). */
    DSB(entry + 0x1Eu) = 5;
    DSW(entry + 0x1Au) = 0x0900u;
    DSD(rec + 0x30u) = 0x08000000u;
    DSW(rec + 0x2Cu) = 0x9999u;
    DSD(rec + 0x08u) = st_rise;
    DSW(pset) = 0x07E1u;
    DSW(rec + 0x34u) = 0x0040u;
    DSW(rec + 0x38u) = 0x0040u;
    fight_effects_pass();
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 5);
    CHECK_EQ_INT((int)DSW(rec + 0x2Cu), 0xD80);
    CHECK_EQ_INT((int)DSD(rec + 0x08u), (int)st_rise);
    CHECK_EQ_INT((int)DSW(rec + 0x38u), 0x40);
    DSD(rec + 0x30u) = 0x09000000u;
    fight_effects_pass();
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 0);
    CHECK_EQ_INT((int)DSW(rec + 0x2Cu), 0xD00);
    CHECK_EQ_INT((int)DSD(rec + 0x08u), (int)st_idle);
    CHECK_EQ_INT((int)DSD(rec + 0x24u), 0x40400000);
    CHECK_EQ_INT((int)DSW(pset), 0x0987);
    CHECK_EQ_INT((int)DSW(rec + 0x34u), 0);
    CHECK_EQ_INT((int)DSW(rec + 0x38u), 0);

    /* The y compare is signed: +0x32 = 0x8000 (negative) stays below 0x100. */
    DSB(entry + 0x1Eu) = 5;
    DSW(entry + 0x1Au) = 0x0100u;
    DSD(rec + 0x30u) = 0x80000000u;
    fight_effects_pass();
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 5);

    DSD(0x000C9634u) = sv_l0;
    DSD(0x000C9634u + 12u) = sv_l3;
    DSD(0x000C95ECu) = sv_r0;
    DSD(0x000C95ECu + 12u) = sv_r3;
    DSD(0x000C9544u) = sv_i0;
    DSD(0x000C9544u + 12u) = sv_i3;
    DSD(DS_0010884C) = DS_0010884C;
}

/* 0x4A634, the effects pass's tail at 0x4A591: each side's slot +0x42 loses
 * bits 0/1 and its 0x10889E/0x1088B2 bytes are zeroed, every frame (the list is
 * empty here). With +0x42 bit 1 set, the side's 0x1088A8 byte draws rng(3) in
 * 0x20..0x3F, or rng(2) and a second rng(2) when the first is non-zero in
 * 0x10..0x17. The RNG state is the proof: seed 0x1234 steps to 0xBAC6D4B3 then
 * 0x5589507A (its first rng(2) is 1); seed 0x1235 steps to 0x73D3E76C (its
 * first rng(2) is 0). Each case seeds the other side's 0x1088A8 byte with a
 * value that would draw differently, so a wrong side index fails. */
static void check_effects_tail(void)
{
    u32 s0 = DS_001077B0;
    u32 s1 = DS_001077B0 + 0x94u;
    u8 sv_42a = DSB(s0 + 0x42u), sv_42b = DSB(s1 + 0x42u);
    u8 sv_a8 = DSB(DS_001088A8), sv_a9 = DSB(DS_001088A8 + 1u);
    u8 sv_9e[3], sv_b2[3];
    u32 i;

    for (i = 0; i < 3u; i++) {
        sv_9e[i] = DSB(DS_0010889E + i);
        sv_b2[i] = DSB(DS_001088B2 + i);
    }
    mem_fill(FIGHT_RECS, 0, 0x4000);
    DSD(DS_001014EC) = FIGHT_ACTORS;
    fight_reset_bases();
    DSW(DS_00104B00) = 3;
    DSB(DS_001088BF) = 0;
    DSB(DS_001088C2) = 0;
    DSD(DS_0010884C) = DS_0010884C;

    /* The clear, no draw: side 0 has bit 1 but 0x18 is in neither range; side
     * 1 has 0x20 but no bit 1. The third bytes of both tables are untouched. */
    DSB(s0 + 0x42u) = 0x5Bu;
    DSB(s1 + 0x42u) = 0xA5u;
    DSB(DS_001088A8) = 0x18u;
    DSB(DS_001088A8 + 1u) = 0x20u;
    DSB(DS_0010889E) = 0x11u;
    DSB(DS_0010889E + 1u) = 0x22u;
    DSB(DS_0010889E + 2u) = 0x77u;
    DSB(DS_001088B2) = 0x33u;
    DSB(DS_001088B2 + 1u) = 0x44u;
    DSB(DS_001088B2 + 2u) = 0x88u;
    rng_seed(0x1234u);
    fight_effects_pass();
    CHECK_EQ_INT((int)DSB(s0 + 0x42u), 0x58);
    CHECK_EQ_INT((int)DSB(s1 + 0x42u), 0xA4);
    CHECK_EQ_INT((int)DSB(DS_0010889E), 0);
    CHECK_EQ_INT((int)DSB(DS_0010889E + 1u), 0);
    CHECK_EQ_INT((int)DSB(DS_0010889E + 2u), 0x77);
    CHECK_EQ_INT((int)DSB(DS_001088B2), 0);
    CHECK_EQ_INT((int)DSB(DS_001088B2 + 1u), 0);
    CHECK_EQ_INT((int)DSB(DS_001088B2 + 2u), 0x88);
    CHECK_EQ_INT((int)DSD(DS_000EF6D8), 0x1234);

    /* 0x3F on side 1 draws one rng(3); side 0's 0x10 (no bit 1) draws none. */
    DSB(s0 + 0x42u) = 0x00u;
    DSB(s1 + 0x42u) = 0x02u;
    DSB(DS_001088A8) = 0x10u;
    DSB(DS_001088A8 + 1u) = 0x3Fu;
    rng_seed(0x1234u);
    fight_effects_pass();
    CHECK_EQ_INT((int)DSD(DS_000EF6D8), (int)0xBAC6D4B3u);
    CHECK_EQ_INT((int)DSB(s1 + 0x42u), 0);

    /* 0x10 on side 1, first rng(2) = 1: a second rng(2). */
    DSB(s1 + 0x42u) = 0x02u;
    DSB(DS_001088A8) = 0x3Fu;
    DSB(DS_001088A8 + 1u) = 0x10u;
    rng_seed(0x1234u);
    fight_effects_pass();
    CHECK_EQ_INT((int)DSD(DS_000EF6D8), 0x5589507A);

    /* 0x17 on side 0, first rng(2) = 0: no second draw. */
    DSB(s0 + 0x42u) = 0x02u;
    DSB(DS_001088A8) = 0x17u;
    DSB(DS_001088A8 + 1u) = 0x05u;
    rng_seed(0x1235u);
    fight_effects_pass();
    CHECK_EQ_INT((int)DSD(DS_000EF6D8), 0x73D3E76C);

    /* 0x1F and 0x40 are in neither range. */
    DSB(s0 + 0x42u) = 0x02u;
    DSB(s1 + 0x42u) = 0x02u;
    DSB(DS_001088A8) = 0x1Fu;
    DSB(DS_001088A8 + 1u) = 0x40u;
    rng_seed(0x1234u);
    fight_effects_pass();
    CHECK_EQ_INT((int)DSD(DS_000EF6D8), 0x1234);

    DSB(s0 + 0x42u) = sv_42a;
    DSB(s1 + 0x42u) = sv_42b;
    DSB(DS_001088A8) = sv_a8;
    DSB(DS_001088A8 + 1u) = sv_a9;
    for (i = 0; i < 3u; i++) {
        DSB(DS_0010889E + i) = sv_9e[i];
        DSB(DS_001088B2 + i) = sv_b2[i];
    }
}

/* 0x3B134: the command-word mapper's stance branch (record §8.13). The three
 * side-0 cases differ only in the other slot's +0x34 sign and +0x64 stance, so
 * a swapped branch or a missing table select fails; the side-1 case proves the
 * side index reaches DS_001088E2, not DS_001088E0. Every case seeds the command
 * word with 0xFFFF, which differs from every expected value. The mapper's
 * rng(100) roll is bypassed with a non-zero override. */
static void check_command_map(void)
{
    u32 r = FIGHT_RECS + 0x400u;

    mem_fill(FIGHT_RECS, 0, 0x800);
    fight_reset_bases();
    DSB(0x001077B0u + 0x63u) = 1;               /* side-0 gate A */
    DSB(0x001077B0u + 0x54u) = 0;               /* 0x1AB10 */
    DSB(0x001077B0u + 0x53u) = 0;
    DSD(0x001077DCu) = 0x3000u;                 /* self +0x2C */
    DSD(0x00107870u) = 0x1000u;                 /* other +0x2C */
    DSB(0x001078A8u) = 0x01u;                   /* other +0x64 != 0xFF */
    DSD(0x0010784Cu) = r;                       /* other +0x08 */
    DSB(DS_0010782A) = 0;                       /* side-0 anim char */
    DSB(DS_001078BE) = 0;                       /* side-1 anim char */

    DSW(DS_001088E0) = 0xFFFFu;                 /* sentinel */
    DSW(r + 0x34u) = 0xFFFFu;                   /* negative -> 0x6000 */
    fight_command_map(0u, 0u, 1u);
    CHECK_EQ_INT((int)DSW(DS_001088E0), 0x6000);

    DSW(DS_001088E0) = 0xFFFFu;
    DSW(r + 0x34u) = 0x0001u;                   /* non-negative -> 0x5000 */
    fight_command_map(0u, 0u, 1u);
    CHECK_EQ_INT((int)DSW(DS_001088E0), 0x5000);

    /* The anim branch: other +0x64 == 0xFF and self left of other -> 0x2000. */
    DSB(0x001078A8u) = 0xFFu;
    DSD(0x001077DCu) = 0x1000u;
    DSD(0x00107870u) = 0x3000u;
    DSW(DS_001088E0) = 0xFFFFu;
    fight_command_map(0u, 0u, 1u);
    CHECK_EQ_INT((int)DSW(DS_001088E0), 0x2000);

    /* Side 1 writes DS_001088E2; DS_001088E0 keeps its sentinel. */
    DSB(0x001078A7u) = 1;                       /* side-1 gate A */
    DSB(0x00107898u) = 0;                       /* side-1 +0x54 */
    DSB(0x00107897u) = 0;                       /* side-1 +0x53 */
    DSB(0x00107814u) = 0x01u;                   /* other (slot 0) +0x64 */
    DSD(0x001077B8u) = r;                       /* other (slot 0) +0x08 */
    DSW(r + 0x34u) = 0xFFFFu;
    DSW(DS_001088E0) = 0x1234u;
    DSW(DS_001088E2) = 0xFFFFu;
    fight_command_map(1u, 0u, 1u);
    CHECK_EQ_INT((int)DSW(DS_001088E2), 0x6000);
    CHECK_EQ_INT((int)DSW(DS_001088E0), 0x1234);
}

/* §26: 0x1975C's first call is the projectile collision step 0x17CB0, which
 * zeroes DS_00100AD0/AD4 before 0x176CC writes them. So seeded counts with no
 * live projectile (slot+0x08 = 0) run no think driver: every seed below keeps
 * its sentinel (the pre-§26 port, which skipped 0x17CB0, ran both drivers and
 * wrote +0x64/+0x41/+0x67/+0x86). */
static void check_think_chain(void)
{
    u8 sv_slots[0x128];
    u8 sv_ad[8];

    tf_snap(sv_slots, DS_001077B0, sizeof sv_slots);
    tf_snap(sv_ad, DS_00100AD0, sizeof sv_ad);
    mem_fill(FIGHT_RECS, 0, 0x800);
    fight_reset_bases();
    DSD(DS_00100AD0) = 3;
    DSD(DS_00100AD0 + 4u) = 3;
    /* The other-slot +0x64 latches and the frame test-and-set word. */
    DSB(0x00107814u) = 0;
    DSB(0x001078A8u) = 0;
    DSB(0x001077F1u) = 0;
    DSB(0x00107885u) = 0;
    DSB(0x00107817u) = 0xAAu;
    DSB(0x001078ABu) = 0xAAu;
    DSD(DS_00107D50) = 0;
    DSD(DS_00107D54) = 0;
    DSB(0x001077B0u + 0x63u) = 0;
    DSB(0x00107844u + 0x63u) = 0;
    /* No live projectile on either side. */
    DSD(0x001077B8u) = 0;
    DSD(0x0010784Cu) = 0;
    /* The entry-copy sentinels. */
    DSW(0x00107834u) = 0x1111u;         /* slot0 +0x84 */
    DSW(0x001078C8u) = 0x2222u;         /* slot1 +0x84 */
    DSW(0x00107836u) = 0x3333u;         /* slot0 +0x86 */
    DSW(0x001078CAu) = 0x4444u;         /* slot1 +0x86 */

    fighter_think();

    CHECK_EQ_INT((int)DSD(DS_00100AD0), 0);         /* 0x17CDC */
    CHECK_EQ_INT((int)DSD(DS_00100AD0 + 4u), 0);    /* 0x17CE2 */
    CHECK_EQ_INT((int)DSB(0x00107814u), 0);         /* slot0 +0x64 kept */
    CHECK_EQ_INT((int)DSB(0x001078A8u), 0);         /* slot1 +0x64 kept */
    CHECK_EQ_INT((int)DSB(0x001077F1u), 0);
    CHECK_EQ_INT((int)DSB(0x00107885u), 0);
    CHECK_EQ_INT((int)DSB(0x00107817u), 0xAA);      /* slot0 +0x67 kept */
    CHECK_EQ_INT((int)DSB(0x001078ABu), 0xAA);      /* slot1 +0x67 kept */
    CHECK_EQ_INT((int)DSW(0x001078CAu), 0x4444);
    CHECK_EQ_INT((int)DSW(0x00107836u), 0x3333);

    tf_put(sv_slots, DS_001077B0, sizeof sv_slots);
    tf_put(sv_ad, DS_00100AD0, sizeof sv_ad);
}

/* The §26 collision fixture: check_unfreeze's fake 8x8 all-on sprite (sprite
 * table index 4) for both fighters' actors 1/2 and the projectiles' actors 3/4,
 * every actor at (0, 0x2000). Slot 0 throws the projectile P (record +0x56 =
 * 3). The collision globals are seeded so the 0x176CC syncs are exact:
 * AA8[0] = B08[1] (DS_00100B0C) and AA0[0] = B00[1] (DS_00100B04), so both p3
 * are 0 and 0x181D0 clips the 0x20 box to the sprite's 8 x 8. */
static void pc_seed(u32 p, u32 p2)
{
    u32 tab = FIGHT_RECS + 0x2000u;
    u32 sbase = FIGHT_RECS + 0x1000u;
    u32 pixelbase = FIGHT_RECS + 0x1800u;
    u32 handle = DSD(DS_000A8B30 + 4u * 4u);
    u32 sprite = sbase + (handle & 0x7FFFFFu);
    u32 i;

    mem_fill(FIGHT_RECS, 0, 0x2400u);
    mem_fill(FIGHT_ACTORS, 0, 0x100u);
    fight_reset_bases();
    DSD(DS_001014EC) = FIGHT_ACTORS;
    DSW(FIGHT_RECS + 0x56u) = 1;
    DSW(FIGHT_RECS + 0x100u + 0x56u) = 2;
    DSW(p + 0x56u) = 3;
    DSW(p2 + 0x56u) = 4;
    for (i = 1; i <= 4u; i++) {
        DSW(FIGHT_ACTORS + i * 0x20u) = 4;
        DSD(FIGHT_ACTORS + i * 0x20u + 4u) = 0;
        DSD(FIGHT_ACTORS + i * 0x20u + 8u) = 0x2000u;
    }
    mem_fill(tab, 0, 32u * 0x14u);
    DSD(DS_001014E0) = tab;
    DSD(DS_001014F0) = 32;
    DSD(tab + 31u * 0x14u + 16u) = sbase;
    DSD(tab + 30u * 0x14u + 16u) = pixelbase;
    DSD(sprite) = 0x00080008u;                  /* width 8, height 8 */
    DSD(sprite + 8u) = 0x0F000000u;             /* entry 30, offset 0 */
    for (i = 0; i < 8u; i++) DSB(pixelbase + i * 9u) = 0x08u;

    mem_fill(DS_001077B0 + 4u, 0, 0x90u);
    mem_fill(DS_001077B0 + 0x94u + 4u, 0, 0x90u);
    DSB(DS_0010782A) = 0;                       /* both char 0 */
    DSB(DS_001078BE) = 0;
    DSD(DS_00100AF0) = (u32)(4 - 0x0EE4);
    DSD(DS_00100AF4) = (u32)(4 - 0x0EE4);
    mem_fill(DS_00100AC0, 0, 16u);              /* no boxes: {0,0,0x20,0x20} */
    DSD(DS_00100AA8) = 0x100u; DSD(DS_00100AAC) = 0x100u;
    DSD(DS_00100AA0) = 0x80u;  DSD(DS_00100AA4) = 0x80u;
    DSD(DS_00100B08) = 0x111u; DSD(DS_00100B0C) = 0x100u;
    DSD(DS_00100B00) = 0x99u;  DSD(DS_00100B04) = 0x80u;
    DSB(DS_00100B62) = 0x5Au;                   /* 0x178CF clears, 0x178FA restores */
    DSB(DS_00100B63) = 0;
    mem_fill(DS_00100B64, 0, 0x9Cu);            /* 0x100B64..0x100C00 row buffers */
    DSB(DS_00100B5A) = 0; DSB(DS_00100B5A + 1u) = 0;
    DSD(DS_00100AD0) = 0xDEADBEEFu;
    DSD(DS_00100AD0 + 4u) = 0xDEADBEEFu;

    /* The thrower (slot 0): the reaction in +0x64, P in +0x08. */
    DSB(DS_001077B0 + 0x64u) = 0x20u;
    DSB(DS_001077B0 + 0x67u) = 0xAAu;
    DSD(DS_001077B0 + 0x08u) = p;
    DSW(DS_001077B0 + 0x84u) = 0x1111u;
    DSW(DS_001077B0 + 0x86u) = 0x3333u;
    /* The struck side (slot 1). */
    DSB(DS_001077B0 + 0x94u + 0x64u) = 0xFFu;
    DSB(DS_001077B0 + 0x94u + 0x67u) = 0xAAu;
    DSW(DS_001077B0 + 0x94u + 0x86u) = 0x4444u;
    DSD(DS_001077B0 + 0x94u + 0x08u) = 0;
    DSD(DS_00107D50) = 0;
    DSD(DS_00107D54) = 0;
    DSW(0x00107D2Cu) = 0;                       /* 0x3962C/0x396AC: k < 1 */
    DSW(DS_001088EC) = 0x7777u;
    /* 0x3B298's block test sees no input: an empty ring, no command word and
     * 0x3B3B2's word[0x100CE0] = 0. */
    mem_fill(DS_00108270, 0, 0x50u);
    DSW(DS_001088E0) = 0;
    DSW(DS_001088E2) = 0;
    DSW(0x00100CE0u) = 0;
    DSB(DS_001077B0 + 0x8Au) = 0x55u;

    DSB(p + 0x48u) = 8u;                        /* 0x3B543: the 0x1922C arm */
    DSW(p + 0x34u) = 0x1234u;
    DSW(p + 0x36u) = 0x1234u;
}

/* §26: 0x17CB0 -> 0x176CC -> 0x1975C -> 0x3B464 -> 0x3B938 on the fixture,
 * plus 0x17BC8's clash, the 0x176CC guard and 0x3B938/0x3A95C directly.
 * The overlap count is derived from the raw: the projectile row is 0xA173C's
 * `ff ff ff ff` (0x16DA4 mode 0), ANDed with the 0xFF plane 0x17CB0 fills
 * (0x15F48); the other's decoded row is 0xFF in byte 0 and the seeded 0 in the
 * rest, so each of the 8 rows (B18 = 8) adds popcount 8: 64. Then
 * (64 << 12) / 0xF3D = 67, (67 << 12) / 0xD56 = 80, 80 / 16 = 5 (0x1703E..
 * 0x1706F), so AD0[0] = 5 > 2 and the think step runs for thrower 0. */
static void check_projectile_step(void)
{
    u8 sv_slots[0x128], sv_g[0x1B0], sv_rows0[0x130], sv_rows1[0x130];
    u8 sv_7d[0x40], sv_7a80[0x80];
    u32 sv_res_tab = DSD(DS_001014E0), sv_res_cnt = DSD(DS_001014F0);
    u32 sv_actor_tab = DSD(DS_001014EC);
    u16 sv_88ec = DSW(DS_001088EC), sv_88e0 = DSW(DS_001088E0);
    u16 sv_88e2 = DSW(DS_001088E2), sv_ce0 = DSW(0x00100CE0u);
    u8 sv_ring[0x50];
    u32 s0 = DS_001077B0, s1 = DS_001077B0 + 0x94u;
    u32 r1 = FIGHT_RECS + 0x100u;
    u32 p = FIGHT_RECS + 0x200u, p2 = FIGHT_RECS + 0x300u;

    tf_snap(sv_slots, DS_001077B0, sizeof sv_slots);
    tf_snap(sv_g, DS_00100A70, sizeof sv_g);
    tf_snap(sv_rows0, 0x000FD160u, sizeof sv_rows0);
    tf_snap(sv_rows1, 0x000FEDE0u, sizeof sv_rows1);
    tf_snap(sv_7d, DS_00107D20, sizeof sv_7d);
    tf_snap(sv_7a80, DS_00107A80, sizeof sv_7a80);
    tf_snap(sv_ring, DS_00108270, sizeof sv_ring);

    /* A: the hit. 0x176CC writes AD0[0] = B54 = 5 and restores B62[0]; the
     * think step applies the hit to side 1 through 0x3B464 (P +0x48 = 8: the
     * 0x1922C arm, then the tail) and bursts P through 0x3B938 (char 0: the
     * 0xBDFC8 stream 0xE85E0, whose walk stops on `CD40 03FA`'s operand word
     * 0xE85E4, at hold byte[0xBDFF0] = 2 -> 2.0f). */
    pc_seed(p, p2);
    fighter_think();
    CHECK_EQ_INT((int)DSD(DS_00100AD0), 5);
    CHECK_EQ_INT((int)DSD(DS_00100B54), 5);
    CHECK_EQ_INT((int)DSD(DS_00100AD0 + 4u), 0);
    CHECK_EQ_INT((int)DSD(DS_00100B1C), 8);         /* 0x181D0 x extent */
    CHECK_EQ_INT((int)DSD(DS_00100B18), 8);         /* 0x181D0 y extent */
    CHECK_EQ_INT((int)DSB(DS_00100B62), 0x5A);
    CHECK_EQ_INT((int)DSB(s1 + 0x67u), 1);          /* 0x197E8 */
    CHECK_EQ_INT((int)DSB(s0 + 0x67u), 0xAA);
    CHECK_EQ_INT((int)(DSB(s1 + 0x41u) & 0x80u), 0x80);   /* 0x3B6B0 */
    CHECK_EQ_INT((int)DSW(s1 + 0x86u), 0x1111);     /* 0x3B2D6 */
    CHECK_EQ_INT((int)DSW(s0 + 0x86u), 0x3333);
    CHECK_EQ_INT((int)DSB(s0 + 0x64u), 0xFF);       /* 0x3B6B8/0x3B985 */
    CHECK_EQ_INT((int)DSD(s0 + 0x08u), 0);          /* 0x3B9A4 */
    CHECK_EQ_INT((int)DSB(p + 0x48u), 0);           /* 0x3B989 */
    CHECK_EQ_INT((int)DSW(p + 0x34u), 0);
    CHECK_EQ_INT((int)DSW(p + 0x36u), 0);
    CHECK_EQ_INT((int)DSD(p + 8u), 0x000E85E4);
    CHECK_EQ_INT((int)DSD(p + 0x24u), 0x40000000);
    CHECK_EQ_INT((int)DSW(DS_001088EC), 3);         /* 0x197FF 0x39278(2) */
    CHECK_EQ_INT((int)(DSB(s1 + 0x43u) & 0x30u), 0);    /* no block */
    CHECK_EQ_INT((int)DSB(s0 + 0x8Au), 0x55);

    /* A2: the same hit blocked. The command word 0x1000 overlaps 0x1AB5C's
     * facing base 0x3000 (both +0x2C are 0) without bits 14/15, so 0x3B298
     * sets +0x43 bit 5 and returns 1: 0x3B669 clears the thrower's +0x8A and
     * the 0x3B080/0x3AD98 arm replaces the +0x48 switch; the tail and the
     * 0x3B938 burst still run. */
    pc_seed(p, p2);
    DSW(DS_001088E2) = 0x1000u;
    fighter_think();
    CHECK_EQ_INT((int)DSD(DS_00100AD0), 5);
    CHECK_EQ_INT((int)(DSB(s1 + 0x43u) & 0x30u), 0x20);   /* 0x3B40D */
    CHECK_EQ_INT((int)DSB(s0 + 0x8Au), 0);          /* 0x3B669 */
    CHECK_EQ_INT((int)(DSB(s1 + 0x41u) & 0x80u), 0x80);
    CHECK_EQ_INT((int)DSB(s0 + 0x64u), 0xFF);
    CHECK_EQ_INT((int)DSD(s0 + 0x08u), 0);

    /* B: the 0x17700 guard: the struck side's +0x74 countdown is non-zero, so
     * 0x176CC returns, AD0 stays at 0x17CDC's 0 and nothing thinks. */
    pc_seed(p, p2);
    DSW(s1 + 0x74u) = 1u;
    fighter_think();
    CHECK_EQ_INT((int)DSD(DS_00100AD0), 0);
    CHECK_EQ_INT((int)DSB(s0 + 0x64u), 0x20);
    CHECK_EQ_INT((int)DSD(s0 + 0x08u), (int)p);
    CHECK_EQ_INT((int)DSB(s1 + 0x67u), 0xAA);
    CHECK_EQ_INT((int)DSW(DS_001088EC), 0x7777);

    /* C: 0x17BC8, both projectiles live and on top of each other: the 0x20
     * boxes clip to 0x20 x 0x20 (B1C/B18 > 0), so side 0's P bursts (0x3B938)
     * and side 1's P2 dies (0x2B150 sets +0x28 bit 3); 0x17D01 then skips
     * 0x176CC, so AD0/AD4 stay 0 and nothing thinks. */
    pc_seed(p, p2);
    DSD(s1 + 0x08u) = p2;
    DSB(s1 + 0x64u) = 0x20u;
    camera_projectile_step();
    CHECK_EQ_INT((int)DSD(DS_00100B1C), 0x20);
    CHECK_EQ_INT((int)DSD(DS_00100B18), 0x20);
    CHECK_EQ_INT((int)DSD(s0 + 0x08u), 0);
    CHECK_EQ_INT((int)DSB(s0 + 0x64u), 0xFF);
    CHECK_EQ_INT((int)(DSB(p2 + 0x28u) & 0x08u), 0x08);
    CHECK_EQ_INT((int)(DSB(p + 0x28u) & 0x08u), 0);
    CHECK_EQ_INT((int)DSD(DS_00100AD0), 0);
    CHECK_EQ_INT((int)DSD(DS_00100AD0 + 4u), 0);
    CHECK_EQ_INT((int)DSB(s1 + 0x67u), 0xAA);

    /* F: the 0x181D0 offsets. y: AA0[0] = B00[1] + 4 gives dy = -4, so the
     * y sync's p3 = +4 clips 0x20 to 4 rows starting at the other's row 4
     * (B18 = 4, B30 = 4): 4 x 8 = 32 -> (32 << 12) / 0xF3D = 33 -> (33 << 12)
     * / 0xD56 = 39 -> 39 / 16 = 2, which the (signed) > 2 gate rejects. */
    pc_seed(p, p2);
    DSD(DS_00100AA0) = 0x80u + 4u;              /* B00[1] = B04 = 0x80 */
    fighter_think();
    CHECK_EQ_INT((int)DSD(DS_00100B18), 4);
    CHECK_EQ_INT((int)DSD(DS_00100B30), 4);
    CHECK_EQ_INT((int)DSD(DS_00100AD0), 2);
    CHECK_EQ_INT((int)DSB(s0 + 0x64u), 0x20);
    CHECK_EQ_INT((int)DSD(s0 + 0x08u), (int)p);
    /* x: AA8[0] = B08[1] + 4, so p3 = +4: B1C = 4, B34 = 4, B14 = 0, and
     * B38 = 4 shifts the other's row left 4 bits (0x1617C) before the AND:
     * 8 rows x popcount(0xF0) = 32 -> 2 again. */
    pc_seed(p, p2);
    DSD(DS_00100AA8) = 0x100u + 4u;             /* B08[1] = B0C = 0x100 */
    camera_projectile_step();
    CHECK_EQ_INT((int)DSD(DS_00100B1C), 4);
    CHECK_EQ_INT((int)DSD(DS_00100B34), 4);
    CHECK_EQ_INT((int)DSD(DS_00100B38), 4);
    CHECK_EQ_INT((int)DSD(DS_00100AD0), 2);

    /* D: 0x3B938 directly. +0x48 == 4 takes 0xE1898 at 2.0 (its walk stops on
     * `CD40 0463`'s operand, 0xE189A); char 1 with +0x48 = 2 takes
     * 0xBDFC8[1] = 0xE4FCE (a literal first word) at byte[0xBDFF1] = 1 -> 1.0. */
    pc_seed(p, p2);
    DSB(p + 0x48u) = 4u;
    fighter_3b938(s0);
    CHECK_EQ_INT((int)DSD(p + 8u), 0x000E189A);
    CHECK_EQ_INT((int)DSD(p + 0x24u), 0x40000000);
    CHECK_EQ_INT((int)DSB(p + 0x48u), 0);
    CHECK_EQ_INT((int)DSD(s0 + 0x08u), 0);
    pc_seed(p, p2);
    DSB(p + 0x48u) = 2u;
    DSB(s0 + 0x7Au) = 1u;
    fighter_3b938(s0);
    CHECK_EQ_INT((int)DSD(p + 8u), 0x000E4FCE);
    CHECK_EQ_INT((int)DSD(p + 0x24u), 0x3F800000);
    CHECK_EQ_INT((int)DSB(s0 + 0x64u), 0xFF);

    /* E: 0x3A95C (side 1, b = 5), the grounded stagger 0x3B5A9 runs: the
     * raptor (char 3) restarts on 0xC8FE0[3] = 0xD267E (a literal first word;
     * the demo's f = 617 record +8) at 3.0, state 0x10/0x0A/0, +0x10 cleared,
     * +0x7E = byte[0xBECF8] (0x14) + 5, and 0x188AC's y 0 in rec+0x1C. */
    pc_seed(p, p2);
    DSB(s1 + 0x7Au) = 3u;
    DSB(s1 + 0x52u) = 0x66u; DSB(s1 + 0x53u) = 0x66u; DSB(s1 + 0x54u) = 0x66u;
    DSD(s1 + 0x10u) = 0x00039CC8u;
    DSB(s1 + 0x7Eu) = 0x66u;
    DSD(r1 + 0x18u) = 9000u;
    DSD(r1 + 0x1Cu) = 0x5555u;
    fighter_3a95c(1u, 5u);
    CHECK_EQ_INT((int)DSB(s1 + 0x52u), 0x10);
    CHECK_EQ_INT((int)DSB(s1 + 0x53u), 0x0A);
    CHECK_EQ_INT((int)DSB(s1 + 0x54u), 0);
    CHECK_EQ_INT((int)DSD(s1 + 0x10u), 0);
    CHECK_EQ_INT((int)DSB(s1 + 0x7Eu), 0x19);
    CHECK_EQ_INT((int)DSD(r1 + 8u), 0x000D267E);
    CHECK_EQ_INT((int)DSD(r1 + 0x24u), 0x40400000);
    CHECK_EQ_INT((int)DSD(r1 + 0x18u), 9000);
    CHECK_EQ_INT((int)DSD(r1 + 0x1Cu), 0);
    CHECK_EQ_INT((int)DSB(DS_001077B0 + 0x52u), 0);  /* side 0 untouched */

    tf_put(sv_slots, DS_001077B0, sizeof sv_slots);
    tf_put(sv_g, DS_00100A70, sizeof sv_g);
    tf_put(sv_rows0, 0x000FD160u, sizeof sv_rows0);
    tf_put(sv_rows1, 0x000FEDE0u, sizeof sv_rows1);
    tf_put(sv_7d, DS_00107D20, sizeof sv_7d);
    tf_put(sv_7a80, DS_00107A80, sizeof sv_7a80);
    DSD(DS_001014E0) = sv_res_tab;
    DSD(DS_001014F0) = sv_res_cnt;
    DSD(DS_001014EC) = sv_actor_tab;
    DSW(DS_001088EC) = sv_88ec;
    DSW(DS_001088E0) = sv_88e0;
    DSW(DS_001088E2) = sv_88e2;
    DSW(0x00100CE0u) = sv_ce0;
    tf_put(sv_ring, DS_00108270, sizeof sv_ring);
}

/* §29's fixture on pc_seed's: side 0's 0x100AC8 box is the raw (0, 36, 2, 3)
 * and side 1's is empty unless `both`; the screen x/y of the side(s) under
 * test is 100. 0x15C30 clips (0, 36, 2, 3) to (0, 1, 8, 7): the palette pair
 * at 0xD5100 + 2 * (0xEE4 + AF0 = 4) is (0x00, 0x80), so dx = 0 and dy =
 * 107 - scale(0x80, 0xD56) = 0 against the sprite rect (0, 107, 8, 115); the
 * scaled box (0, 108, 8, 9) keeps x and loses 107 rows on top and 2 below. */
static void ph_seed(u32 p, u32 p2, int both)
{
    pc_seed(p, p2);
    mem_fill(DS_00100AC0, 0, 16u);
    DSB(DS_00100AC8 + 1u) = 36; DSB(DS_00100AC8 + 2u) = 2; DSB(DS_00100AC8 + 3u) = 3;
    DSD(DS_00100B08) = 100; DSD(DS_00100B00) = 100;
    DSD(DS_00100B0C) = 0x200u; DSD(DS_00100B04) = 0x200u;
    if (both) {
        DSB(DS_00100AC8 + 4u + 1u) = 36; DSB(DS_00100AC8 + 4u + 2u) = 2; DSB(DS_00100AC8 + 4u + 3u) = 3;
        DSD(DS_00100B0C) = 100; DSD(DS_00100B04) = 100;
    }
    DSB(DS_00100B62) = 0x5Au;
    DSB(DS_00100B63) = 0xA5u;
    DSW(DS_00104B00) = 3;
    DSD(DS_00100B54) = 0xDEADBEEFu;
    DSD(DS_00100B1C) = 0xDEADBEEFu;
    DSD(DS_00100B18) = 0xDEADBEEFu;
    DSD(DS_00100B30) = 0xDEADBEEFu;
    DSD(DS_00100B10) = 0xDEADBEEFu;
}

/* 0x17D30 / 0x1790C and the effects pass's trample (record §29). The point
 * x = (100 + 0x18) * 64, y = (100 + 0x38) * 64 lands on side 0's screen
 * point: the x syncs see p3 = box0 + 100 - 100 = 0 (B1C = 8, the box) and
 * 100 - 100 = 0 (B1C = 8, the sprite), the y sync p3 = box1 + 100 - 100 = 1
 * (B18 = 7, B30 = 1, B10 = 0 then + box1 = 1). 0x16DA4 mode 2 ANDs each of
 * the 7 decoded rows (0xFF) with the 0xFF column plane (0x15F48 of the 8-bit
 * box) and 0xA1740's first byte 0x0F: 7 x 4 = 28 -> (28 << 12) / 0xF3D = 29
 * -> (29 << 12) / 0xD56 = 34 -> 34 / 16 = 2 = B54 > 0, a hit. Mode 3 (tall)
 * reads 0xA1746's first byte 0x00, so B54 = 0 there. */
static void check_point_trample(void)
{
    u8 sv_slots[0x128], sv_g[0x1B0], sv_rows0[0x130], sv_rows1[0x130];
    u8 sv_7d[0x40], sv_7a80[0x80];
    u32 sv_res_tab = DSD(DS_001014E0), sv_res_cnt = DSD(DS_001014F0);
    u32 sv_actor_tab = DSD(DS_001014EC);
    u16 sv_4b00 = DSW(DS_00104B00);
    u8 sv_1a = DSB(0x00104B1Au), sv_3a = DSB(DS_00105B3A);
    u8 sv_ae0 = DSB(DS_001088AE), sv_ae1 = DSB(DS_001088AE + 1u);
    u32 sv_t[10];
    const u32 tabs[5] = { 0x000C9604u, 0x000BB920u, 0x000C973Cu, 0x000C9544u, 0 };
    u32 p = FIGHT_RECS + 0x200u, p2 = FIGHT_RECS + 0x300u;
    u32 entry = FIGHT_RECS + 0x3000u, rec = FIGHT_RECS + 0x3100u;
    u32 st_tumble = FIGHT_RECS + 0x3800u, st_land = FIGHT_RECS + 0x3840u;
    u32 st_idle = FIGHT_RECS + 0x3880u, st_wrong = FIGHT_RECS + 0x38C0u;
    u32 desc3 = FIGHT_RECS + 0x3900u, desc0 = FIGHT_RECS + 0x3920u;
    u32 ps5 = FIGHT_ACTORS + 5u * 0x20u;
    s32 x_hit = (100 + 0x18) * 64, y_hit = (100 + 0x38) * 64;
    u32 held_byte = DSD(DS_001014F4) + 2u * 0x68u + 0x4Bu;
    u8 sv_held = DSB(held_byte);
    u32 sh_fake = FIGHT_RECS + 0x3200u;     /* a scratch shadow record */
    u32 sv_84c = DSD(DS_0010884C), sv_rng = DSD(DS_000EF6D8);
    u8 sv_8bf = DSB(DS_001088BF), sv_8c2 = DSB(DS_001088C2);
    u32 i, sh;

    tf_snap(sv_slots, DS_001077B0, sizeof sv_slots);
    tf_snap(sv_g, DS_00100A70, sizeof sv_g);
    tf_snap(sv_rows0, 0x000FD160u, sizeof sv_rows0);
    tf_snap(sv_rows1, 0x000FEDE0u, sizeof sv_rows1);
    tf_snap(sv_7d, DS_00107D20, sizeof sv_7d);
    tf_snap(sv_7a80, DS_00107A80, sizeof sv_7a80);
    for (i = 0; i < 4u; i++) {
        sv_t[i * 2u] = DSD(tabs[i]);
        sv_t[i * 2u + 1u] = DSD(tabs[i] + 12u);
    }

    /* A: the hit, side 0 only. The flip bytes and the saved B08..B04 come back. */
    ph_seed(p, p2, 0);
    CHECK_EQ_INT((int)camera_point_hit(x_hit, y_hit, 0u), 1);
    CHECK_EQ_INT((int)DSD(DS_00100B54), 2);
    CHECK_EQ_INT((int)DSD(DS_00100B1C), 8);
    CHECK_EQ_INT((int)DSD(DS_00100B18), 7);
    CHECK_EQ_INT((int)DSD(DS_00100B30), 1);
    CHECK_EQ_INT((int)DSD(DS_00100B10), 1);         /* 0x17B38 + box1 */
    CHECK_EQ_INT((int)DSB(DS_00100B62), 0x5A);      /* 0x17EAE */
    CHECK_EQ_INT((int)DSB(DS_00100B63), 0xA5);      /* 0x17E3C */

    /* B: 0x40 screen units right of the box: the first x sync is empty and
     * nothing is written. */
    ph_seed(p, p2, 0);
    CHECK_EQ_INT((int)camera_point_hit(x_hit + 0x40 * 64, y_hit, 0u), 0);
    CHECK_EQ_INT((int)DSD(DS_00100B54), (int)0xDEADBEEFu);
    /* The first x sync measures the box from its clipped left edge: the raw
     * (1, 36, 1, 3) clips to (4, 1, 4, 7), and px = 100 - 0x2E puts that edge
     * at p3 = 4 + 0x2E = 0x32, past the 0x30 point box, so the sync returns
     * before writing B1C (without box0, p3 = 0x2E would be visible). */
    ph_seed(p, p2, 0);
    DSB(DS_00100AC8) = 1; DSB(DS_00100AC8 + 2u) = 1;
    DSD(DS_00100B1C) = 0xDEADBEEFu;
    CHECK_EQ_INT((int)camera_point_hit((100 - 0x2E + 0x18) * 64, y_hit, 0u), 0);
    CHECK_EQ_INT((int)DSD(DS_00100B1C), (int)0xDEADBEEFu);

    /* C: both boxes: 3. Mode 0x22 tests only the side DS_00104B1A names. */
    ph_seed(p, p2, 1);
    CHECK_EQ_INT((int)camera_point_hit(x_hit, y_hit, 0u), 3);
    DSW(DS_00104B00) = 0x22u;
    DSB(0x00104B1Au) = 1;
    CHECK_EQ_INT((int)camera_point_hit(x_hit, y_hit, 0u), 2);
    DSB(0x00104B1Au) = 0;
    CHECK_EQ_INT((int)camera_point_hit(x_hit, y_hit, 0u), 1);
    DSB(0x00104B1Au) = sv_1a;

    /* D: `tall` (BX != 0): y - 0x30 and the 0x30-row box, and the mode-3 row
     * 0xA1746 whose first byte is 0, so no overlap; the same y without it is a
     * hit (y sync p3 = 1 + 100 - 92 = 9: B30 = 9, B18 = 7). */
    ph_seed(p, p2, 0);
    CHECK_EQ_INT((int)camera_point_hit(x_hit, (100 + 0x30) * 64, 1u), 0);
    CHECK_EQ_INT((int)DSD(DS_00100B54), 0);
    CHECK_EQ_INT((int)DSD(DS_00100B30), 1);
    CHECK_EQ_INT((int)camera_point_hit(x_hit, (100 + 0x30) * 64, 0u), 1);
    CHECK_EQ_INT((int)DSD(DS_00100B30), 9);
    /* The non-tall box is 0x38 rows: y = (49 + 0x38) * 64 gives the y sync
     * p3 = 1 + 100 - 49 = 0x34, visible only below 0x38: B18 = 0x38 - 0x34 = 4
     * rows (1..4) x 4 = 16 -> 16 -> 19 -> B54 = 1, a hit (0x30 rows would
     * leave none). */
    ph_seed(p, p2, 0);
    CHECK_EQ_INT((int)camera_point_hit(x_hit, (49 + 0x38) * 64, 0u), 1);
    CHECK_EQ_INT((int)DSD(DS_00100B18), 4);
    CHECK_EQ_INT((int)DSD(DS_00100B54), 1);

    /* The effects-pass fixture: one entry (si = 3, side byte 1) whose actor's
     * pset (index 5) holds the point; the four tables' entries 0 and 3 are
     * distinct scratch streams / descriptors (a wrong index fails). The two
     * descriptors copy 0xBB920[3]'s 0xBB560 but carry a distinct +0x0C word,
     * which 0x2AE14 stores into the shadow's +0x2C. */
    DSW(st_tumble) = 0x0456u;
    DSW(st_land) = 0x0321u;
    DSW(st_idle) = 0x0987u;
    DSW(st_wrong) = 0x0BADu;
    for (i = 0; i < 0x14u; i++) {
        DSB(desc3 + i) = DSB(0x000BB560u + i);
        DSB(desc0 + i) = DSB(0x000BB560u + i);
    }
    DSW(desc3 + 0x0Cu) = 0x0777u;
    DSW(desc0 + 0x0Cu) = 0x0111u;
    DSD(0x000C9604u) = st_wrong;  DSD(0x000C9604u + 12u) = st_tumble;
    DSD(0x000BB920u) = desc0;     DSD(0x000BB920u + 12u) = desc3;
    DSD(0x000C973Cu) = st_wrong;  DSD(0x000C973Cu + 12u) = st_land;
    DSD(0x000C9544u) = st_wrong;  DSD(0x000C9544u + 12u) = st_idle;
    DSB(DS_00105B3A) = 0;

#define TR_SEED(bit7) do {                                              \
        ph_seed(p, p2, 0);                                              \
        mem_fill(entry, 0, 0x40u); mem_fill(rec, 0, 0x68u);             \
        DSD(DS_0010884C) = entry; DSD(entry) = DS_0010884C;             \
        DSD(entry + 8u) = rec; DSD(entry + 0xCu) = DS_001077B0 + 0x94u; \
        DSB(entry + 0x21u) = 1; DSB(entry + 0x1Eu) = 4;                 \
        DSW(entry + 0x18u) = 50; DSB(entry + 0x1Cu) = (bit7) ? 0x85u : 0x05u; \
        DSB(entry + 0x20u) = 0x77u; DSB(entry + 0x1Fu) = 0;             \
        DSD(entry + 0x10u) = 0;                                         \
        DSB(rec + 0x48u) = 0x23u; DSW(rec + 0x56u) = 5;                 \
        DSD(rec + 0x18u) = 0x1234u; DSD(rec + 0x30u) = 0x05000000u;     \
        DSW(rec + 0x34u) = 0x1111u; DSW(rec + 0x36u) = 0x2222u;         \
        DSD(ps5 + 4u) = (u32)x_hit; DSD(ps5 + 8u) = (u32)y_hit;         \
        DSB(DS_001088BF) = 0; DSB(DS_001088C2) = 0;                     \
    } while (0)

    /* E: +0x1C bit 7 clear: 0x4B69C returns at once (B54 keeps its sentinel)
     * and case 4 counts the lie timer down. */
    TR_SEED(0);
    fight_effects_pass();
    CHECK_EQ_INT((int)DSD(DS_00100B54), (int)0xDEADBEEFu);
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 4);
    CHECK_EQ_INT((int)DSW(entry + 0x18u), 49);
    CHECK_EQ_INT((int)DSB(entry + 0x1Fu), 0);

    /* F: the fighter in its grab move 0xC97F2[0] = 0x2D with +0x52 = 4 inside
     * [0xC97E4[0], 0xC97EB[0]] = [3, 6]: 0x4B788 returns 0 (its grab arm is
     * the named gap), so no trample and case 4 runs. */
    TR_SEED(1);
    DSB(DS_001077B0 + 0x5Fu) = 0x2Du;
    DSB(FIGHT_RECS + 0x52u) = 4;
    fight_effects_pass();
    CHECK_EQ_INT((int)DSD(DS_00100B54), 2);
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 4);
    CHECK_EQ_INT((int)DSW(entry + 0x18u), 49);
    CHECK_EQ_INT((int)DSB(entry + 0x1Cu), 0x85);
    CHECK_EQ_INT((int)DSB(entry + 0x20u), 0x77);
    /* The same grab move tramples when DS_00105B3A > 1 (0x4B7A0) or the other
     * side's slot +0x54 is 3 (0x4B7E0). */
    TR_SEED(1);
    DSB(DS_001077B0 + 0x5Fu) = 0x2Du;
    DSB(FIGHT_RECS + 0x52u) = 4;
    DSB(DS_00105B3A) = 2;
    fight_effects_pass();
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 6);
    DSB(DS_00105B3A) = 0;
    actor_set_dead(DSD(entry + 0x10u));
    actor_free(DSD(entry + 0x10u));
    TR_SEED(1);
    DSB(DS_001077B0 + 0x5Fu) = 0x2Du;
    DSB(FIGHT_RECS + 0x52u) = 4;
    DSB(DS_001077B0 + 0x94u + 0x54u) = 3;
    fight_effects_pass();
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 6);
    actor_set_dead(DSD(entry + 0x10u));
    actor_free(DSD(entry + 0x10u));

    /* G: the demo's trample (f = 689): side 0 not in its grab move. 0x4B69C
     * sets +0x20 = 0 and +0x1F = 1; 0x4B470 starts the 0xC9604[3] stream at
     * 3.0, spawns the shadow from 0xBB920[3] into +0x10, throws the actor away
     * from side 0 (actor word bit 15 clear: 0x1A570 = 1 -> +0x34 = -0x80) with
     * +0x36 = 0x240, clears +0x1C bit 7 and makes the entry type 6 — which the
     * dispatch then runs in the same pass (+0x36 = 0x230), so case 4's timer
     * is not decremented. */
    TR_SEED(1);
    rng_seed(0x1234u);
    fight_effects_pass();
    sh = DSD(entry + 0x10u);
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 6);
    CHECK_EQ_INT((int)DSB(entry + 0x20u), 0);
    CHECK_EQ_INT((int)DSB(entry + 0x1Fu), 1);
    CHECK_EQ_INT((int)DSB(entry + 0x1Cu), 0x05);
    CHECK_EQ_INT((int)DSW(entry + 0x18u), 50);
    CHECK_EQ_INT((int)DSD(rec + 0x08u), (int)st_tumble);
    CHECK_EQ_INT((int)DSD(rec + 0x24u), 0x40400000);
    CHECK_EQ_INT((int)DSW(rec + 0x34u), 0xFF80);
    CHECK_EQ_INT((int)DSW(rec + 0x36u), 0x0230);
    CHECK(sh != 0u, "0x4B470 spawned the shadow into +0x10");
    if (sh != 0u) CHECK_EQ_INT((int)DSW(sh + 0x2Cu), 0x0777);
    CHECK_EQ_INT((int)DSD(DS_000EF6D8), 0x1234);
    /* The shadow's pset index comes from the process-wide free list; the
     * fixture's psets are 1 and 2 (the fighters) and 5 (the worshipper), so a
     * shadow there would have rewritten one after the hit test. */
    if (sh != 0u)
        CHECK(DSW(sh + 0x56u) != 1u && DSW(sh + 0x56u) != 2u
              && DSW(sh + 0x56u) != 5u,
              "the shadow's pset is none of the fixture's psets 1, 2, 5");

    /* H: case 6 airborne: the shadow follows the x dword and the y word +0x32;
     * +0x36 = 0x100 with the height 0x1000 stays up (0xF0) and +0x1C bit 7
     * stays clear; +0x36 = -0x20 falls on (-0x30) and sets bit 7 again. The
     * point is moved off both fighters so the prelude misses; the fighters'
     * psets 1 and 2 are re-seeded as pc_seed has them. */
    for (i = 1; i <= 2u; i++) {
        DSW(FIGHT_ACTORS + i * 0x20u) = 4;
        DSD(FIGHT_ACTORS + i * 0x20u + 4u) = 0;
        DSD(FIGHT_ACTORS + i * 0x20u + 8u) = 0x2000u;
    }
    DSD(ps5 + 4u) = 0; DSD(ps5 + 8u) = 0;
    DSD(rec + 0x18u) = 0x4321u;
    DSD(rec + 0x30u) = 0x06660000u;
    DSD(rec + 0x1Cu) = 0x1000u;
    DSW(rec + 0x36u) = 0x0100u;
    fight_effects_pass();
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 6);
    CHECK_EQ_INT((int)DSW(rec + 0x36u), 0x00F0);
    CHECK_EQ_INT((int)DSB(entry + 0x1Cu), 0x05);
    if (sh != 0u) {
        CHECK_EQ_INT((int)DSD(sh + 0x18u), 0x4321);
        CHECK_EQ_INT((int)DSW(sh + 0x32u), 0x0666);
    }
    DSW(rec + 0x36u) = 0xFFE0u;
    fight_effects_pass();
    CHECK_EQ_INT((int)DSW(rec + 0x36u), 0xFFD0);
    CHECK_EQ_INT((int)DSB(entry + 0x1Cu), 0x85);

    /* I: the landing: -0x10 + 0x10 = 0 is not above zero. The shadow dies
     * (+0x28 bit 3) and +0x10 clears; 0xC973C[3] at 2.0 and type 8; the
     * height, +0x36, +0x34 and +0x1F are zeroed. */
    DSB(entry + 0x1Cu) = 0x05u;
    DSD(rec + 0x1Cu) = 0x10u;
    DSW(rec + 0x36u) = 0xFFF0u;
    DSW(rec + 0x34u) = 0xFF80u;
    fight_effects_pass();
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 8);
    CHECK_EQ_INT((int)DSD(entry + 0x10u), 0);
    if (sh != 0u) CHECK_EQ_INT((int)(DSB(sh + 0x28u) & 0x08u), 0x08);
    CHECK_EQ_INT((int)DSD(rec + 0x08u), (int)st_land);
    CHECK_EQ_INT((int)DSD(rec + 0x24u), 0x40000000);
    CHECK_EQ_INT((int)DSD(rec + 0x1Cu), 0);
    CHECK_EQ_INT((int)DSW(rec + 0x36u), 0);
    CHECK_EQ_INT((int)DSW(rec + 0x34u), 0);
    CHECK_EQ_INT((int)DSB(entry + 0x1Fu), 0);
    CHECK_EQ_INT((int)DSB(entry + 0x1Cu), 0x85);
    if (sh != 0u) actor_free(sh);
    /* With 0xC973C[3] = 0 the landing takes 0xC9544[3] at 3.0 as type 4. */
    DSB(entry + 0x1Eu) = 6;
    DSD(0x000C973Cu + 12u) = 0;
    DSD(rec + 0x1Cu) = 0;
    DSW(rec + 0x36u) = 0;
    fight_effects_pass();
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 4);
    CHECK_EQ_INT((int)DSD(rec + 0x08u), (int)st_idle);
    CHECK_EQ_INT((int)DSD(rec + 0x24u), 0x40400000);
    DSD(0x000C973Cu + 12u) = st_land;

    /* J: a second hit (+0x1F 1 -> 2) reverses the current +0x34 (0x4B50F):
     * -0x80 -> 0x80 and anything else -> -0x80; +0x10 already holds a shadow,
     * so none is spawned. Both sides touching counts as side 0 (0x4B6F5). */
    TR_SEED(1);
    ph_seed(p, p2, 1);
    DSD(ps5 + 4u) = (u32)x_hit; DSD(ps5 + 8u) = (u32)y_hit;
    DSD(entry + 0x10u) = sh_fake;
    DSB(entry + 0x1Fu) = 1;
    DSD(rec + 0x32u) = 0xFF800000u;
    fight_effects_pass();
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 6);
    CHECK_EQ_INT((int)DSB(entry + 0x20u), 0);
    CHECK_EQ_INT((int)DSB(entry + 0x1Fu), 2);
    CHECK_EQ_INT((int)DSW(rec + 0x34u), 0x0080);
    CHECK_EQ_INT((int)DSD(entry + 0x10u), (int)sh_fake);
    DSD(entry + 0x10u) = 0;
    TR_SEED(1);
    DSD(entry + 0x10u) = sh_fake;
    DSB(entry + 0x1Fu) = 1;
    DSD(rec + 0x32u) = 0x00800000u;
    fight_effects_pass();
    CHECK_EQ_INT((int)DSW(rec + 0x34u), 0xFF80);
    /* Mode 0x22 leaves +0x34 as it was (0x4B4E5); side 0 is DS_00104B1A's. */
    TR_SEED(1);
    DSD(entry + 0x10u) = sh_fake;
    DSW(DS_00104B00) = 0x22u;
    DSB(0x00104B1Au) = 0;
    fight_effects_pass();
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 6);
    CHECK_EQ_INT((int)DSW(rec + 0x34u), 0x1111);
    DSB(0x00104B1Au) = sv_1a;
    DSW(DS_00104B00) = 3;

    /* K: a held actor (+0x4A = 2) is released first (0x4B720): the held
     * record's +0x4B, the actor's +0x29 bit 6 and +0x4A, the entry's +0x1C
     * bit 6, and DS_001088AE[+0x21] counts one. */
    TR_SEED(1);
    DSD(entry + 0x10u) = sh_fake;
    DSB(entry + 0x1Cu) = 0xC5u;
    DSB(rec + 0x4Au) = 2;
    DSB(rec + 0x29u) = 0x40u;
    DSB(held_byte) = 0x99u;
    DSB(DS_001088AE + 1u) = 5;
    DSB(DS_001088AE) = 7;
    fight_effects_pass();
    CHECK_EQ_INT((int)DSB(held_byte), 0);
    CHECK_EQ_INT((int)DSB(rec + 0x4Au), 0);
    CHECK_EQ_INT((int)(DSB(rec + 0x29u) & 0x40u), 0);
    CHECK_EQ_INT((int)DSB(entry + 0x1Cu), 0x05);
    CHECK_EQ_INT((int)DSB(DS_001088AE + 1u), 6);
    CHECK_EQ_INT((int)DSB(DS_001088AE), 7);
#undef TR_SEED

    DSD(DS_0010884C) = sv_84c;
    DSD(DS_000EF6D8) = sv_rng;
    DSB(DS_001088BF) = sv_8bf;
    DSB(DS_001088C2) = sv_8c2;
    DSB(held_byte) = sv_held;
    DSB(DS_001088AE) = sv_ae0;
    DSB(DS_001088AE + 1u) = sv_ae1;
    DSB(DS_00105B3A) = sv_3a;
    DSW(DS_00104B00) = sv_4b00;
    for (i = 0; i < 4u; i++) {
        DSD(tabs[i]) = sv_t[i * 2u];
        DSD(tabs[i] + 12u) = sv_t[i * 2u + 1u];
    }
    tf_put(sv_slots, DS_001077B0, sizeof sv_slots);
    tf_put(sv_g, DS_00100A70, sizeof sv_g);
    tf_put(sv_rows0, 0x000FD160u, sizeof sv_rows0);
    tf_put(sv_rows1, 0x000FEDE0u, sizeof sv_rows1);
    tf_put(sv_7d, DS_00107D20, sizeof sv_7d);
    tf_put(sv_7a80, DS_00107A80, sizeof sv_7a80);
    DSD(DS_001014E0) = sv_res_tab;
    DSD(DS_001014F0) = sv_res_cnt;
    DSD(DS_001014EC) = sv_actor_tab;
}

/* 0x3BDDC: the attack/command consumer (record §8.17). Input A drives the
 * transition; B/C prove the +0x40 and command-bit-15 gates; D adds the
 * table-select and the 0x1000/0x2000 command bits. The ring is seeded so
 * 0x4649C returns 0 (0xBEF28) or 1 (0xBEF64). */
static void check_attack_consume(void)
{
    u32 p0 = FIGHT_RECS;
    u32 saved_ring[5];
    u32 saved_pos = DSD(0x001082D2u);

    /* The five ring words 0x4649C reads for side 0 at position 0. */
    const u32 ring[5] = { 0x00108270u, 0x00108290u, 0x00108292u,
                          0x00108294u, 0x00108296u };

    mem_fill(FIGHT_RECS, 0, 0x400);
    fight_reset_bases();
    DSD(0x001082D2u) = 0;                       /* ring position 0 */
    for (int i = 0; i < 5; i++) {
        saved_ring[i] = DSW(ring[i]);
        DSW(ring[i]) = 0;
    }
    DSB(DS_0010782A) = 0;                       /* char 0 -> base + 0 */
    DSB(0x001077B0u + 0x40u) = 0;               /* +0x40 bit 7 clear */
    DSB(DS_00107803) = 1;                       /* reach the continuation */
    DSW(DS_001088E0) = 0x8000u;

    DSW(p0 + 0x34u) = 0x00AAu;
    DSB(p0 + 0x43u) = 0xAAu;
    DSB(p0 + 0x42u) = 0xAAu;
    DSB(0x001077B0u + 0x5Fu) = 0xAAu;
    DSW(DS_001077FE) = 0x1234u;         /* sentinel != the 0 the call writes */
    CHECK_EQ_INT(fighter_attack_consume(0u), 1);
    CHECK_EQ_INT((int)DSW(p0 + 0x34u), 0);
    CHECK_EQ_INT((int)DSB(p0 + 0x43u), 0);
    CHECK_EQ_INT((int)DSB(p0 + 0x42u), 0);
    CHECK_EQ_INT((int)DSB(0x001077B0u + 0x5Fu), 0xFF);
    CHECK_EQ_INT((int)DSB(DS_00107802), 3);
    CHECK_EQ_INT((int)DSB(DS_00107803), 4);
    CHECK_EQ_INT((int)DSB(DS_00107804), 2);
    CHECK_EQ_INT((int)DSB(DS_001078F8), 1);
    CHECK_EQ_INT((int)DSW(DS_001077FE), 0);
    CHECK_EQ_INT((int)DSD(DS_00107D40), 0xBEF28);

    /* The 0x1000 and 0x2000 command bits select +0x4E and the 0xBEF64 table. */
    DSW(ring[0]) = 0x4000u;                     /* 0x4649C -> 1 -> 0xBEF64 */
    DSW(DS_001088E0) = 0x9000u;                 /* bit 15 | 0x1000 */
    CHECK_EQ_INT(fighter_attack_consume(0u), 1);
    CHECK_EQ_INT((int)DSD(DS_00107D40), 0xBEF64);
    CHECK_EQ_INT((int)DSW(DS_001077FE), 1);

    DSW(ring[0]) = 0;
    DSW(DS_001088E0) = 0xA000u;                 /* bit 15 | 0x2000 */
    CHECK_EQ_INT(fighter_attack_consume(0u), 1);
    CHECK_EQ_INT((int)DSW(DS_001077FE), 0xFFFF);

    /* Input B: the +0x40 bit-7 gate rejects; nothing changes. */
    DSB(0x001077B0u + 0x40u) = 0x80u;
    DSW(DS_001088E0) = 0x8000u;
    DSW(p0 + 0x34u) = 0x00AAu;
    DSB(p0 + 0x42u) = 0xAAu;
    CHECK_EQ_INT(fighter_attack_consume(0u), 0);
    CHECK_EQ_INT((int)DSW(p0 + 0x34u), 0x00AA);
    CHECK_EQ_INT((int)DSB(p0 + 0x42u), 0xAA);

    /* Input C: command bit 15 clear rejects. */
    DSB(0x001077B0u + 0x40u) = 0;
    DSW(DS_001088E0) = 0x0000u;
    DSW(p0 + 0x34u) = 0x00AAu;
    CHECK_EQ_INT(fighter_attack_consume(0u), 0);
    CHECK_EQ_INT((int)DSW(p0 + 0x34u), 0x00AA);

    for (int i = 0; i < 5; i++) DSW(ring[i]) = (u16)saved_ring[i];
    DSD(0x001082D2u) = saved_pos;
}

/* 0x41350/0x33C18: the per-side character select. The slot's +0x63 think gate,
 * DS_0010816A (the 0xC835A[char] byte), DS_0010816E, DS_00108860 and the
 * DS_00104B1D suppression are the pinned observables. */
static void check_char_select(void)
{
    fight_reset_recs();
    DSW(DS_00108860) = 0;
    DSW(DS_00108860 + 2u) = 0;
    DSB(DS_0010816A) = 0xFFu;
    DSB(DS_0010816A + 1u) = 0xFFu;
    DSB(DS_0010816E) = 0;
    DSB(DS_0010816E + 1u) = 0;
    DSB(DS_001077B0 + 0x63u) = 0;               /* slot 0 +0x63 */
    DSB(DS_001077B0 + 0x94u + 0x63u) = 0;       /* slot 1 +0x63 */
    DSB(DS_00104B1D) = 0;

    fight_char_select(0u, 1u);
    CHECK_EQ_INT((int)DSB(DS_0010816A), (int)DSB(DS_000C835A + 1u));
    CHECK_EQ_INT((int)DSB(DS_001077B0 + 0x63u), 1);
    CHECK_EQ_INT((int)DSB(DS_0010816E), 0xFF);
    CHECK_EQ_INT((int)DSW(DS_00108860), 100);

    fight_char_select(1u, 3u);
    CHECK_EQ_INT((int)DSB(DS_0010816A + 1u), (int)DSB(DS_000C835A + 3u));
    CHECK_EQ_INT((int)DSB(DS_001077B0 + 0x94u + 0x63u), 1);
    CHECK_EQ_INT((int)DSB(DS_0010816E + 1u), 0xFF);
    CHECK_EQ_INT((int)DSW(DS_00108860 + 2u), 100);
    CHECK_EQ_INT((int)DSB(0x0010810Du), 0);     /* side 1 -> the other index */

    /* DS_00104B1D == 1 suppresses the character store only. */
    DSB(DS_00104B1D) = 1;
    DSB(DS_0010816A) = 0xABu;
    fight_char_select(0u, 5u);
    CHECK_EQ_INT((int)DSB(DS_0010816A), 0xAB);
    CHECK_EQ_INT((int)DSB(DS_001077B0 + 0x63u), 1);
}

/* 0x33F08: the health-bar pass. The health word comes from the slot+0x24 table
 * indexed by (actor word & 0x7fff) - char_const, into the secondary actor's +8;
 * the out-of-range arm writes 0x1E1; actor bit 15 sets the pset+0x29 bit 0x40;
 * 0x2A408 re-asserts the secondary's current sprite word into its pset. */
static void check_health_bars(void)
{
    u32 r = FIGHT_RECS + 0x600u;
    u32 fighter = FIGHT_RECS + 0x700u;
    u32 secondary = FIGHT_RECS + 0x800u;
    u32 hptable = FIGHT_RECS + 0xA00u;

    mem_fill(FIGHT_RECS + 0x600u, 0, 0x600);
    fight_reset_actors();
    DSD(DS_001077A8) = r;
    DSD(DS_001077A8 + 4u) = 0;          /* side 1 inert */
    DSD(r) = fighter;
    DSD(r + 4u) = secondary;
    DSD(r + 0x24u) = hptable;
    DSB(r + 0x7Au) = 3;                 /* char 3 -> 0x16B5 */
    DSB(r + 0x41u) = 0;
    DSW(fighter + 0x56u) = 1;           /* actor index 1 */
    DSW(secondary + 0x56u) = 1;         /* secondary actor index 1 */
    DSW(secondary + 0x28u) = 0x0800u;   /* literal-id arm of 0x2A408 */

    DSW(FIGHT_ACTORS + 1u * 0x20u) = 0x8000u | 0x16B5u;   /* s = 0 */
    DSW(hptable) = 0x1234u;
    DSD(secondary + 8u) = 0xDEADBEEFu;
    fight_health_bars();
    CHECK_EQ_INT((int)DSD(secondary + 8u), 0x1234);
    CHECK_EQ_INT((int)DSB(secondary + 0x29u) & 0x40, 0x40);
    /* 0x2A408 re-asserts the secondary's sprite word; the +0x29 bit 0x40 makes
     * the hflip term clear, so the returned id keeps bit 15. */
    CHECK_EQ_INT((int)DSW(FIGHT_ACTORS + 1u * 0x20u), 0x9234);

    /* actor bit 15 clear: the pset+0x29 bit 0x40 is cleared. */
    DSW(FIGHT_ACTORS + 1u * 0x20u) = 0x16B5u;
    DSD(secondary + 8u) = 0xDEADBEEFu;
    DSW(secondary + 0x28u) = 0x0800u;
    DSB(secondary + 0x29u) = 0x48u;     /* bit 3 keeps the literal arm, bit 6 seeded */
    fight_health_bars();
    CHECK_EQ_INT((int)DSB(secondary + 0x29u) & 0x40, 0);

    /* +0x41 bit 0x20 forces the out-of-range sprite. */
    DSB(r + 0x41u) = 0x20u;
    DSD(secondary + 8u) = 0xDEADBEEFu;
    fight_health_bars();
    CHECK_EQ_INT((int)DSD(secondary + 8u), 0x1E1);
}

/* 0x186D0: the slot position latch. Bit 3 of slot+0x42 takes the clean
 * rec+0x18/+0x1C copy; the clear arm adds the DS_00100AB0/AB4 offsets; bit 7 of
 * slot+0x41 latches slot+0x2C into slot+0x34. */
static void check_slot_latch(void)
{
    u32 p0 = FIGHT_RECS;
    fight_reset_recs();
    DSD(p0 + 0x18u) = 0xAAu;                    /* record position */
    DSD(p0 + 0x1Cu) = 0xBBu;
    DSD(DS_001077B0 + 0x2Cu) = 0xDEADu;         /* slot +0x2C */
    DSD(DS_001077B0 + 0x30u) = 0xDEADu;         /* slot +0x30 */
    DSD(DS_001077B0 + 0x34u) = 0xDEADu;         /* slot +0x34 */
    DSB(DS_001077B0 + 0x42u) = 0x08u;           /* bit 3 set: clean copy */
    DSB(DS_001077B0 + 0x41u) = 0;               /* bit 7 clear: no +0x34 latch */
    fighter_slot_latch(0u);
    CHECK_EQ_INT((int)DSD(DS_001077B0 + 0x2Cu), 0xAA);
    CHECK_EQ_INT((int)DSD(DS_001077B0 + 0x30u), 0xBB);
    CHECK_EQ_INT((int)DSD(DS_001077B0 + 0x34u), 0xDEAD);   /* gate clear */

    DSB(DS_001077B0 + 0x41u) = 0x80u;
    DSD(p0 + 0x18u) = 0x1234u;                  /* the latch copies rec+0x18 */
    fighter_slot_latch(0u);
    CHECK_EQ_INT((int)DSD(DS_001077B0 + 0x34u), 0x1234);   /* +0x34 latch */

    DSB(DS_001077B0 + 0x42u) = 0;               /* bit 3 clear: the anchor sum */
    DSB(DS_001077B0 + 0x41u) = 0;
    DSD(DS_001077A8) = 0;                       /* 0x18540's slot read: early out */
    DSD(DS_00100AF0) = DSD(DS_001077B0 + 0x20u);/* anchor unchanged: no 0x18350 */
    DSD(DS_00100AB0) = 0x10u;
    DSD(DS_00100AB4) = 0x20u;
    DSD(p0 + 0x18u) = 0x100u;
    DSD(p0 + 0x1Cu) = 0x200u;
    fighter_slot_latch(0u);
    CHECK_EQ_INT((int)DSD(DS_001077B0 + 0x2Cu), 0x110);
    CHECK_EQ_INT((int)DSD(DS_001077B0 + 0x30u), 0x220);

    /* 0x18540/0x18350: the screen anchor and offset. With the side's slot live
     * and the camera path (bit 3 clear), 0x18540 seeds DS_00100AF0[side] from
     * the actor's low 15 bits minus the character's camera constant (char 0 ->
     * the >6 default 0xE6DD0 = 3812) and 0x18350 writes the anchor-indexed
     * table pair (0xCEB00 = (-5, 52)), the x negated when the actor's bit 15 is
     * set, both scaled by 64. A sprite of 3815 gives anchor 3, so AB0 = 5*64 =
     * 320 and AB4 = 52*64 = 3328. The seeded sentinels (0xDEADBEEF) differ from
     * every post-condition, and a skipped 0x18540/0x18350 leaves them. */
    DSD(DS_001014EC) = FIGHT_ACTORS;
    DSD(DS_001077A8) = DS_001077B0;             /* the slot 0x18540 classifies */
    DSB(DS_0010782A) = 0;                       /* slot+0x7A: char 0 */
    DSW(p0 + 0x56u) = 1;                        /* actor index */
    DSW(FIGHT_ACTORS + 1u * 0x20u) = 0x8EE7u;   /* sprite 3815, bit 15 set */
    DSB(DS_001077B0 + 0x42u) = 0;               /* bit 3 clear: the camera path */
    DSB(DS_001077B0 + 0x41u) = 0;
    DSD(DS_001077B0 + 0x20u) = 0xDEADBEEFu;     /* differs from the anchor */
    DSD(DS_00100AF0) = 0xDEADBEEFu;
    DSD(DS_00100AB0) = 0xDEADBEEFu;
    DSD(DS_00100AB4) = 0xDEADBEEFu;
    DSD(p0 + 0x18u) = 0x100u;
    DSD(p0 + 0x1Cu) = 0x200u;
    fighter_slot_latch(0u);
    CHECK_EQ_INT((int)DSD(DS_00100AF0), 3);
    CHECK_EQ_INT((int)DSD(DS_00100AB0), 320);
    CHECK_EQ_INT((int)DSD(DS_00100AB4), 3328);
    CHECK_EQ_INT((int)DSD(DS_001077B0 + 0x20u), 3);         /* the latch stores it */
    CHECK_EQ_INT((int)DSD(DS_001077B0 + 0x2Cu), 0x240);
    CHECK_EQ_INT((int)DSD(DS_001077B0 + 0x30u), 0xF00);

    /* The char-6 arm of 0x18350's 0x18334 table: char 6 -> 0xCF399, while the
     * >6 default (0x1838B) is 0xCEB00. Char 6's camera constant 0xE061C = 13015
     * and the clamp 0xE6DB4[6] = 1043, so sprite 13015 gives anchor 0; the same
     * anchor then reads (-1, 60) not the default's (-5, 52), so AB0 = 1*64 = 64
     * and AB4 = 60*64 = 3840. A conflated default (char 6 -> 0xCEB00) gives
     * 320/3328, so the CHECKs below fail under that mutation. */
    DSB(DS_0010782A) = 6;                       /* slot+0x7A: char 6 */
    DSW(FIGHT_ACTORS + 1u * 0x20u) = 0xB2D7u;   /* sprite 13015, bit 15 set */
    DSD(DS_001077B0 + 0x20u) = 0xDEADBEEFu;
    DSD(DS_00100AF0) = 0xDEADBEEFu;
    DSD(DS_00100AB0) = 0xDEADBEEFu;
    DSD(DS_00100AB4) = 0xDEADBEEFu;
    fighter_slot_latch(0u);
    CHECK_EQ_INT((int)DSD(DS_00100AF0), 0);
    CHECK_EQ_INT((int)DSD(DS_00100AB0), 64);
    CHECK_EQ_INT((int)DSD(DS_00100AB4), 3840);
}

/* 0x11A8C: state 6. Fourteen draws from the shared stream — rng(7) at 0x11AAD,
 * then the P0 spawn's dust builder (0x494A8: three draws per iteration over
 * slot+0x81 = 2, so six), rng(6) at 0x11AE9 and the P1 spawn's six — the
 * stores, the arm and the character picks. The draws are precomputed on a fresh
 * seed so a wrong order, count or range fails; DS_00104AFC must carry draw1,
 * DS_0010816A[0]/[1] the two mapped characters, and the LCG state after the
 * handler must equal the model's fourteen-step state (a dropped dust draw or a
 * restored master-loop draw moves it). */
static void check_state6(void)
{
    (void)tf_demo_fixture();

    actors_reset();                     /* the spawn allocates from the pool */
    DSD(DS_001077A8) = FIGHT_RECS;      /* sentinels differ from the slots */
    DSD(DS_001077A8 + 4u) = FIGHT_RECS + 0x100u;

    DSB(DS_00104B1D) = 0;               /* let 0x41350 store the character */
    DSD(DS_001088E4) = 0;               /* coin poll mask: nothing accepted */
    DSB(DS_00104528 + 1u) = 2;          /* (DS_00104528+1)&2 set: skip text */
    DSW(DS_00104B00) = 3;               /* mode 3: 0x49388's range is 0x64 */
    fight_reset_state6();
    DSB(DS_0010816A) = 0xFFu;
    DSB(DS_0010816A + 1u) = 0xFFu;

    rng_seed(0x1234u);
    u32 draw1 = rng_next(7u);
    /* Each spawn's dust builder (0x494A8) draws three values per iteration —
     * 0x49388's pick (the raw's range: mode 3 -> 0x64, 0x49397; else
     * DS_00108860[side], 0x4939E), rng(0x1800) at 0x495DF and rng(step) at
     * 0x495FC with step = 0x300 / slot+0x81 — over slot+0x81 = 2 iterations.
     * P0's six sit between 0x11AAD and 0x11AE9; P1's six follow at 0x11B08.
     * slot+0x81 is 2 here (0x49300 seeds DS_001088CC = 2, DS_000C9520 divides
     * slot+0x3C = 0). Each pick's value maps through the raw's thresholds
     * (0x493B0..0x493E4) to the 0xC9524 descriptor index. */
    u32 dust_idx[4];
    u32 dust_off[4];                /* the loop's running offset */
    u32 dust_dy[4];                 /* the loop's rng(step) draw */
    u32 di = 0;
    u32 draw2 = 0;
    for (u32 side = 0; side < 2u; side++) {
        u32 offset = 0;
        for (u32 i = 0; i < 2u; i++) {
            u32 v = rng_next(0x64u);
            dust_idx[di] = (v < 0x1eu) ? 4u : (v < 0x32u) ? 3u
                           : (v < 0x46u) ? 5u : (v < 0x55u) ? 1u
                           : (v < 0x5fu) ? 0u : 2u;
            (void)rng_next(0x1800u);
            /* The entry's +0x1A is the y (0x49642 reads [ESP+0x4], the value
             * 0x49605 wrote after 0x2AE14's RET 0x4 restores ESP), not the step.
             * y = offset + (rec+0x30 >> 16) + 0x400 + rng(step); the assertion
             * below reads the slot's rec+0x30 after game_state_step. */
            dust_off[di] = offset;
            dust_dy[di] = rng_next(0x300u / 2u);
            offset += 0x180u;
            di++;
        }
        if (side == 0u) draw2 = rng_next(6u);
    }
    u32 p1_char = (draw1 + draw2) % 7u;
    u32 end_state = DSD(DS_000EF6D8);

    DSW(DS_000F0A64) = 6;
    /* Sentinels for the state-6 reset's two camera stores: 0x12C70 seeds the
     * camera-x step (0x20E6A) to 0x400 and 0x20E52 zeroes the camera x. Values
     * that differ from both post-conditions prove the stores ran, independent
     * of whatever an earlier check left here. */
    DSW(DS_000F0AFC) = 0x1234;
    DSD(DS_000F0AF0) = 0x1234;
    /* Demo record §15: 0x20DF4's 0x12750 call (0x20E33) builds the type-0x01
     * node lists; 0xA5 over both sentinels and the nodes differs from every
     * link it writes. */
    mem_fill(0x000F0A78u, 0xA5u, 0x70u);
    rng_seed(0x1234u);
    game_state_step();

    CHECK_EQ_INT((int)DSD(DS_000F0AE0), (int)DS_000F0AE0);   /* 0x12750 */
    CHECK_EQ_INT((int)DSD(DS_000F0AE4), (int)DS_000F0AE0);
    CHECK_EQ_INT((int)DSD(DS_000F0A78), 0x000F0A80);
    CHECK_EQ_INT((int)DSD(DS_000F0A7C), 0x000F0AD4);
    CHECK_EQ_INT((int)DSW(DS_000F0AFC), 0x400);   /* 0x12C70 */
    CHECK_EQ_INT((int)DSD(DS_000F0AF0), 0);       /* 0x20E52 */

    CHECK_EQ_INT((int)DSD(DS_000EF6D8), (int)end_state);
    CHECK_EQ_INT((int)DSW(DS_00104AFC), (int)draw1);

    /* 0x494A8's effect, not only its draws: two entries per side moved to the
     * 0x10884C list (each insert-after puts the newest first, so side 1's two
     * lead), each with a type-0 header, the y at +0x1A, the side at +0x21 and a
     * spawned actor at +8. The actor's +8 is its descriptor's first dword
     * (0x2AE7D), so asserting it against the modeled picker index discriminates
     * the raw's rng(0x64) range from a range that always yields index 4. */
    {
        u32 n = 0;
        for (u32 e = DSD(DS_0010884C); e != DS_0010884C; e = DSD(e)) {
            u32 spawn = 3u - n;         /* the list is newest-first */
            u32 actor = DSD(e + 8u);
            u32 desc = DSD(DS_000C9524 + dust_idx[spawn] * 4u);
            u32 rec = DSD(DSD(e + 0x0Cu));     /* the entry's slot's record */
            u32 y = dust_off[spawn]
                  + (u32)((s32)DSD(rec + 0x30u) >> 16) + 0x400u + dust_dy[spawn];
            CHECK_EQ_INT((int)DSB(e + 0x1Eu), 0);
            CHECK_EQ_INT((int)DSW(e + 0x1Au), (int)(u16)y);
            /* +0x21 is the side (0x49626; the raw reads [ESP+0x8] after
             * 0x2AE14's RET 0x4), not the y low byte. Spawns 0/1 are side 0. */
            CHECK_EQ_INT((int)DSB(e + 0x21u), (int)(spawn < 2u ? 0u : 1u));
            CHECK_EQ_INT((int)DSD(e + 0x0Cu),
                         (int)(DS_001077B0 + (n < 2u ? 0x94u : 0u)));
            CHECK(actor != 0, "dust entry carries a spawned actor");
            /* The actor's +8 is the anim pointer desc[0] (0x2AE7D) after the
             * spawn's walk (0x2AFFA advances it by 2 before the first sprite
             * id); +0x48 keeps the descriptor's type byte (dp[4], 0x20+idx),
             * which the visible arm of 0x2B0D4 does not rewrite. */
            CHECK_EQ_INT((int)DSD(actor + 8u), (int)(DSD(desc) + 2u));
            CHECK_EQ_INT((int)DSB(actor + 0x48u), (int)(0x20u + dust_idx[spawn]));
            n++;
        }
        CHECK_EQ_INT((int)n, 4);
    }
    CHECK_EQ_INT((int)DSB(DS_0010816A), (int)DSB(DS_000C835A + draw1));
    CHECK_EQ_INT((int)DSB(DS_0010816A + 1u), (int)DSB(DS_000C835A + p1_char));
    CHECK_EQ_INT((int)DSB(DS_001077B0 + 0x63u), 1);
    CHECK_EQ_INT((int)DSB(DS_001077B0 + 0x94u + 0x63u), 1);
    CHECK_EQ_INT((int)DSB(DS_0010816E), 0xFF);
    CHECK_EQ_INT((int)DSB(DS_00104B15), 1);
    CHECK_EQ_INT((int)DSB(DS_00104B19 + 2u), 1);
    CHECK_EQ_INT((int)DSW(DS_001082CC), 3);
    CHECK_EQ_INT((int)DSW(DS_000F0A6A), 900);
    CHECK_EQ_INT((int)DSW(DS_000F0A6C), 5);
    CHECK_EQ_INT((int)DSW(DS_000F0A64), 7);
    CHECK_EQ_INT((int)DSB(DS_000F0A6F), 0);
    CHECK_EQ_INT((int)DSD(DS_001082C8), 7);         /* the 0x33EB4 EDX handoff */

    /* 0x33EB4/0x33C78: both slots are live. DS_001077A8 holds the slot
     * addresses 0x1077B0/0x107844, slot+0x7A the picked character, slot+0x00
     * the spawned fighter record, and DS_001078FA counted both spawns. */
    CHECK_EQ_INT((int)DSD(DS_001077A8), (int)DS_001077B0);
    CHECK_EQ_INT((int)DSD(DS_001077A8 + 4u), (int)(DS_001077B0 + 0x94u));
    CHECK_EQ_INT((int)DSB(DS_001077B0 + 0x7Au), (int)DSB(DS_0010816A));
    CHECK_EQ_INT((int)DSB(DS_001077B0 + 0x94u + 0x7Au), (int)DSB(DS_0010816A + 1u));
    CHECK(DSD(DS_001077B0) != 0, "P0 spawn produced a record");
    CHECK(DSD(DS_001077B0 + 0x94u) != 0, "P1 spawn produced a record");
    CHECK_EQ_INT((int)DSB(DS_001078FA), 2);

    /* 0x20DF4's third branch call (0x20E86): 0x412A0/0x2C320 spawn the scene
     * draw1 selects. The first prop triple's a2 must be a live record's, and a
     * scene with a crowd must leave the crowd table at DS_00105C08. Both are
     * read from the shipped tables, so the assertion cannot pass on a value the
     * test itself wrote. */
    {
        u32 prop_tab = DSD(DS_000C82CC + draw1 * 4u);
        if (DSD(prop_tab) != 0u) {
            u32 a2 = DSD(prop_tab + 4u);
            u32 found = 0;
            for (u32 r = actor_list_head(); r != 0; r = actor_next(r))
                if (DSD(r + 0x18u) == a2) found = 1;
            CHECK(found, "state 6 spawns the scene's first prop");
        }
        if (DSW(DS_000BBD98 + draw1 * 2u) != 0u)
            CHECK_EQ_INT((int)DSD(DS_00105C08),
                         (int)DSD(DS_000BBDA8 + draw1 * 4u));
    }
}

/* Case 7: the pre-decrement timer. With timer 2 one call runs the arena and
 * leaves state 7; with timer 1 it exits through 0x11BCC and does not run the
 * arena. The fixture's latch sentinels (077E8/0787C) prove which arm ran. */
static void check_state7(void)
{
    (void)tf_demo_fixture();
    DSB(DS_00104B1D) = 1;
    DSW(DS_000F0A64) = 7;
    DSW(DS_000F0A6A) = 2;
    DSW(DS_000F0A6C) = 4;
    DSB(DS_00104B15) = 1;
    DSB(DS_00104B19 + 2u) = 1;
    game_state_step();
    CHECK_EQ_INT((int)DSW(DS_000F0A6A), 1);
    CHECK_EQ_INT((int)DSW(DS_000F0A64), 7);
    CHECK_EQ_INT((int)DSD(DS_001077E8), 0x1111);   /* the arena latch ran */
    CHECK_EQ_INT((int)DSD(DS_0010787C), 0x3333);

    (void)tf_demo_fixture();
    DSB(DS_00104B1D) = 1;
    DSW(DS_000F0A64) = 7;
    DSW(DS_000F0A6A) = 1;
    DSW(DS_000F0A6C) = 4;
    DSB(DS_00104B15) = 1;
    DSB(DS_00104B19 + 2u) = 1;
    game_state_step();
    CHECK_EQ_INT((int)DSW(DS_000F0A6A), 0);
    CHECK_EQ_INT((int)DSW(DS_000F0A64), 4);
    CHECK_EQ_INT((int)DSB(DS_00104B15), 0);
    CHECK_EQ_INT((int)DSB(DS_00104B19 + 2u), 0);
    CHECK_EQ_INT((int)DSD(DS_001077E8), 0x2222);   /* sentinel: no arena */
}

/* 0x24C5C's DS_00104B15 tail: with the flag armed the tail's 0x12D48 clamp
 * runs; disarmed it is skipped. The fixture's camera mode 4 makes the dispatcher
 * clamp only, so the camera-x value is the whole observable. */
static void check_game_frame_tail(void)
{
    (void)tf_demo_fixture();
    DSB(DS_00104B1D) = 1;
    DSD(DS_00104B00) = 3;
    DSW(DS_000F0A64) = 7;
    DSW(DS_000F0A6A) = 2;
    DSB(DS_00104B15) = 1;
    DSB(DS_000F0AFE) = 4;
    DSD(DS_000F0AF0) = 0x7000u;
    DSD(DS_00104AE8) = 0;
    /* 0x24CCD..0x24CDB: the frame counter is a word. 0xFFFF wraps to 0 and
     * must not carry into the separate global at 0xEF6DE (sentinel 0x5A5A);
     * a dword increment leaves 0x5A5B there. The wrap opens 0x1282C's gate,
     * so its node list is seeded empty: the spawn is refused after its
     * draws. */
    DSW(DS_000EF6DC) = 0xFFFFu;
    DSW(DS_000EF6DC + 2u) = 0x5A5Au;
    DSD(0x000F0A78u) = 0x000F0A78u;
    DSD(0x000F0A7Cu) = 0x000F0A78u;
    /* 0x2541D: the tail's 0x3BB90 body push clears DS_00107D30 first. */
    DSB(DS_00107D30) = 1u;
    game_frame();
    CHECK_EQ_INT((int)DSD(DS_000F0AF0), 0x5D00);
    CHECK_EQ_INT((int)DSW(DS_000EF6DC), 0);
    CHECK_EQ_INT((int)DSW(DS_000EF6DC + 2u), 0x5A5A);
    CHECK_EQ_INT((int)DSB(DS_00107D30), 0);

    (void)tf_demo_fixture();
    DSB(DS_00104B1D) = 1;
    DSD(DS_00104B00) = 3;
    DSW(DS_000F0A64) = 7;
    DSW(DS_000F0A6A) = 2;
    DSB(DS_00104B15) = 0;
    DSB(DS_000F0AFE) = 4;
    DSD(DS_000F0AF0) = 0x7000u;
    DSD(DS_00104AE8) = 0;
    DSB(DS_00107D30) = 1u;
    game_frame();
    CHECK_EQ_INT((int)DSD(DS_000F0AF0), 0x7000);
    CHECK_EQ_INT((int)DSB(DS_00107D30), 1);    /* the tail (and 0x3BB90) skipped */
}

/* 0x49300, state 6's fight-effect list init. It self-links the 0x1083C4 and
 * 0x10884C sentinels, inserts the 0x24-stride nodes into the 0x1083C4 list with
 * 0x249C0, and seeds DS_001088CC/CB from DS_00104AFC. The sentinel is zeroed
 * first so the check cannot pass on stale state, and the 0x1083C4..0x10883F
 * region (not covered by test_fight's other snapshots) is saved and restored. */
/* 0x412A0/0x2C320: the scene's props and crowd. Scene 0's prop table
 * (0xC82CC[0] = 0xC7F78) has five non-zero triples and the crowd count
 * (0xBBD98[0]) is 3, so the pair issues 8 spawns. The crowd's first actor
 * (descriptor 0xC7850, whose dword0 is the animation stream 0xE8EB2) then runs
 * the animation walk, whose opcode-0x0C targets (the stream words at
 * 0xE8EB2/0xE8EBC are 0xCC01, (word>>8)&0x1F = 0x0C, dispatched at 0x2B484
 * through anim_operand's mode-0x4000 load of DS_00105BD4) spawn 2 child actors
 * from 0xC7864/0xC7878 (a5 = 0x407: the parent bit + slot 7); opcode 0x11
 * (0x2B57F) only does anim_indirect and spawns nothing on this stream. So the
 * active list grows by 10. The count difference (not an absolute count) is the
 * anchor, and each record is found by its raw-derived a2/a3 rather than by list
 * position. The props' a4 (0x2C320's `(s16)[e+4]`) and 0x412A0's a4 = 0 are
 * left unasserted: both are zero on the shipped scene data, so a CHECK on them
 * would pass on an unseeded BSS zero (AGENTS.md) — the count and the a2/a3/id
 * triples above are the discriminating anchors. */
static void check_scene_props(void)
{
    u32 saved_5c08 = DSD(DS_00105C08);
    u32 before, after;

    actors_reset();                     /* the spawns allocate from the pool */
    before = 0;
    for (u32 r = actor_list_head(); r != 0; r = actor_next(r)) before++;

    fight_scene_props(0u);

    after = 0;
    for (u32 r = actor_list_head(); r != 0; r = actor_next(r)) after++;
    CHECK_EQ_INT((int)(after - before), 10);

    /* The props: 0xC7F78's five triples' a2 values with their descriptor ids
     * (0xC77EC..0xC783C carry 0x2EF..0x2F3) and a3 = the triples' word at +8.
     * The props' flags (0x1A00) skip the animation walk, so rec+8 keeps the
     * descriptor's id. */
    static const u32 prop_a2[5] = { 0xFFFFAC00u, 0x00004400u, 0xFFFFF000u,
                                    0xFFFFD800u, 0xFFFFD280u };
    static const u16 prop_a3[5] = { 0x0C00u, 0x0C00u, 0x0D40u, 0x0F40u, 0x1080u };
    static const u32 prop_id[5] = { 0x2EFu, 0x2F0u, 0x2F1u, 0x2F2u, 0x2F3u };
    for (u32 i = 0; i < 5u; i++) {
        u32 found = 0;
        for (u32 r = actor_list_head(); r != 0; r = actor_next(r)) {
            if (DSD(r + 0x18u) != prop_a2[i]) continue;
            found = 1;
            CHECK_EQ_INT((int)DSW(r + 0x32u), (int)prop_a3[i]);
            CHECK_EQ_INT((int)DSD(r + 0x08u), (int)prop_id[i]);
        }
        CHECK(found, "the scene spawns each prop triple");
    }

    /* The crowd: 0xBBC18's three records, a2 = the record's first dword and
     * a3 = its word at +6, spawned from 0xBB9D8[k*3] (k = 0x1B/0x2E/0x2F). */
    static const u32 crowd_a2[3] = { 0xFFFFEEA0u, 0xFFFFD7C0u, 0xFFFFEDB0u };
    static const u16 crowd_a3[3] = { 0x2492u, 0x0F1Bu, 0x0D35u };
    for (u32 i = 0; i < 3u; i++) {
        u32 found = 0;
        for (u32 r = actor_list_head(); r != 0; r = actor_next(r)) {
            if (DSD(r + 0x18u) != crowd_a2[i]) continue;
            found = 1;
            CHECK_EQ_INT((int)DSW(r + 0x32u), (int)crowd_a3[i]);
        }
        CHECK(found, "the scene spawns each crowd record");
    }

    /* 0x2C320 stores its table pointer at DS_00105C08 (0x2C339). */
    CHECK_EQ_INT((int)DSD(DS_00105C08), (int)0x000BBC18u);

    /* Scene 2 (0xC82CC[2] = 0xC7FF0, 8 triples) has no crowd (0xBBD98[2] = 0),
     * so 0x2C320 returns before its DS_00105C08 store and a sentinel there must
     * survive. The count stays 8 only because the three descriptors whose
     * word[8] is 0x1200 (0xC78DC/0xC78F0/0xC7904; bit 0x0800 clear, so
     * actor_spawn runs the walk) point at the streams
     * 0xE8EA6/0xE8EAC/0xE8E74, whose initial walks spawn no child — not because
     * every prop skips the walk (the other five carry 0x1A00 or 0x5A00, which
     * set 0x0800). */
    actors_reset();
    before = 0;
    for (u32 r = actor_list_head(); r != 0; r = actor_next(r)) before++;
    DSD(DS_00105C08) = 0xDEADBEEFu;
    fight_scene_props(2u);
    after = 0;
    for (u32 r = actor_list_head(); r != 0; r = actor_next(r)) after++;
    CHECK_EQ_INT((int)(after - before), 8);
    CHECK_EQ_INT((int)DSD(DS_00105C08), (int)0xDEADBEEFu);

    DSD(DS_00105C08) = saved_5c08;
}

static void check_list_init(void)
{
    u8 s_li[0x48C];
    u32 s_a4fc = DSD(DS_00104AFC);

    tf_snap(s_li, 0x001083C4u, sizeof s_li);
    DSD(DS_0010884C) = 0;               /* head would walk address 0 if unset */
    DSD(DS_00108850) = 0;
    DSD(DS_001083C4) = 0;
    DSD(DS_001083C8) = 0;
    DSD(DS_00108880) = 0xDEADBEEFu;
    DSB(DS_001088CC) = 0xAB;
    DSB(DS_001088CB) = 0xCD;
    DSD(DS_00104AFC) = 0;               /* draw1 != 7: the 2/4 arm */

    fight_list_init();

    CHECK_EQ_INT((int)DSD(DS_0010884C), (int)DS_0010884C);
    CHECK_EQ_INT((int)DSD(DS_00108850), (int)DS_0010884C);
    CHECK_EQ_INT((int)DSD(DS_00108880), 0x1500);
    CHECK_EQ_INT((int)DSB(DS_001088CC), 2);
    CHECK_EQ_INT((int)DSB(DS_001088CB), 4);
    /* The loop inserts 32 nodes 0x1083CC..0x108828 into the 0x1083C4 list, each
     * before the sentinel, so the sentinel's next is the first node and its prev
     * is the last; the last node's next wraps back to the sentinel. */
    CHECK_EQ_INT((int)DSD(DS_001083C4), (int)0x001083CCu);
    CHECK_EQ_INT((int)DSD(0x001083CCu), (int)0x001083F0u);
    CHECK_EQ_INT((int)DSD(DS_001083C8), (int)0x00108828u);
    CHECK_EQ_INT((int)DSD(0x00108828u), (int)DS_001083C4);

    /* The draw1 == 7 arm (unreachable from rng(7), but the raw branch exists). */
    DSD(DS_00104AFC) = 7;
    fight_list_init();
    CHECK_EQ_INT((int)DSB(DS_001088CC), 0);
    CHECK_EQ_INT((int)DSB(DS_001088CB), 0);

    DSD(DS_00104AFC) = s_a4fc;
    tf_put(s_li, 0x001083C4u, sizeof s_li);
}

/* 0x24C73/0x47208: the demo's CPU-AI command generator. A live pair of slots
 * (think gate armed, idle state, a legal character) must produce a non-zero
 * command word, and the two sides must be able to differ. The command words are
 * seeded to zero; a missing fighter_command_block() leaves them zero. The AI
 * state blocks 0x1081F0..0x1082EF are saved because they are outside the test's
 * shared snapshots. */
static void check_command_generator(void)
{
    u32 p0 = FIGHT_RECS, p1 = FIGHT_RECS + 0x100u;
    u32 r0 = FIGHT_RECS + 0x200u, r1 = FIGHT_RECS + 0x300u;
    u8 s_ai[0x100];
    int any = 0;

    tf_snap(s_ai, 0x001081F0u, sizeof s_ai);
    mem_fill(0x001081F0u, 0, sizeof s_ai);
    mem_fill(FIGHT_RECS, 0, 0x400);
    mem_fill(FIGHT_ACTORS, 0, 0x80);

    DSD(DS_001014EC) = FIGHT_ACTORS;
    fight_reset_bases();
    fight_reset_slot_pair(p0, p1, r0, r1);
    DSB(p0 + 0x7Au) = 0;                /* character 0 */
    DSB(p1 + 0x7Au) = 0;
    DSB(p0 + 0x63u) = 1;                /* 0x47208's emit gate */
    DSB(p1 + 0x63u) = 1;
    DSB(p0 + 0x5Fu) = 0xFFu;            /* idle: 0x466F4's stance byte */
    DSB(p1 + 0x5Fu) = 0xFFu;
    DSB(p0 + 0x52u) = 0;                /* slot state */
    DSB(p1 + 0x52u) = 0;
    DSW(r0 + 0x56u) = 1;                /* actor index for 0x1A570 */
    DSW(r1 + 0x56u) = 2;
    DSW(r0 + 0x08u) = 0;                /* avoid an animation walk */
    DSW(r1 + 0x08u) = 0;
    DSW(FIGHT_ACTORS + 1u * 0x20u) = 0; /* actor bit 15 clear */
    DSW(FIGHT_ACTORS + 2u * 0x20u) = 0;

    DSB(DS_00104B26) = 0;
    DSB(0x00104B1Bu) = 1;               /* state 6's arm */
    DSB(0x00105B39u) = 0;
    DSD(DS_001082C8) = 7;               /* difficulty, state 6's handoff */
    DSD(DS_001082C8 + 4u) = 7;
    DSW(DS_001088E0) = 0;
    DSW(DS_001088E2) = 0;

    rng_seed(0x1234u);
    for (int i = 0; i < 30; i++) {
        fighter_command_block();
        if (DSW(DS_001088E0) != 0u || DSW(DS_001088E2) != 0u) any = 1;
    }
    CHECK(any, "the demo AI emits a non-zero command word");

    tf_put(s_ai, 0x001081F0u, sizeof s_ai);
}

/* 0x36E2C and the +0x52 dispatch. Input A arms the gate (slot+0x42 bit 0x10)
 * with the fighter out of range, so the position branch's 0x36638 arm runs and
 * sets slot+0x52 = 0x12. Input B clears the gate, so the table dispatches to the
 * default handler 0x349C8 and a 0x1000 command runs 0x35838 -> slot+0x52 = 0x0E.
 * The seeded post-conditions differ, so a swapped or skipped branch fails. */
static void check_state_dispatch(void)
{
    u32 p0 = FIGHT_RECS, r0 = FIGHT_RECS + 0x200u;

    mem_fill(FIGHT_RECS, 0, 0x400);
    fight_reset_actors();
    fight_reset_slot0(p0, r0);
    DSB(p0 + 0x7Au) = 0;
    DSW(r0 + 0x56u) = 0;
    DSB(p0 + 0x5Fu) = 0xFFu;
    DSB(p0 + 0x54u) = 0;
    DSB(p0 + 0x53u) = 0;
    DSB(p0 + 0x43u) = 0;
    DSW(r0 + 0x28u) = 0;                /* facing clear */
    DSD(DS_00104B00) = 3;               /* not 4, not 0x22/0x25 */
    DSW(r0 + 0x08u) = 0;                /* safe animation pointer */

    /* A: gate 1, |x| >= 0x7C00 -> the 0x36638 arm -> slot+0x52 = 0x12. */
    DSD(r0 + 0x18u) = 0x00010000u;      /* x - 0x3000 = 0xD000 */
    DSB(p0 + 0x42u) = 0x10u;
    DSB(p0 + 0x52u) = 0;
    DSW(DS_001088E0) = 0;
    fight_hud_pass(0u);
    CHECK_EQ_INT((int)DSB(p0 + 0x52u), 0x12);
    CHECK_EQ_INT((int)DSB(p0 + 0x41u) & 0x80, 0x80);

    /* B: gate 0 (bit 0x10 clear), +0x52 = 0 -> the table -> 0x349C8, and a
     * 0x1000 command runs 0x35838 -> slot+0x52 = 0x0E. */
    DSB(p0 + 0x42u) = 0;
    DSB(p0 + 0x41u) = 0;
    DSB(p0 + 0x43u) = 0;
    DSB(p0 + 0x52u) = 0;
    DSB(p0 + 0x53u) = 0;
    DSW(DS_001088E0) = 0x1000u;
    fight_hud_pass(0u);
    CHECK_EQ_INT((int)DSB(p0 + 0x52u), 0x0E);
    CHECK_EQ_INT((int)DSB(p0 + 0x43u) & 0x01, 0x01);

    /* C: the same command with +0x52 = 0x0E (a no-op table case) leaves the
     * state alone: the dispatch really is the table, not a fallthrough. */
    DSB(p0 + 0x43u) = 0;
    DSW(DS_001088E0) = 0x1000u;
    fight_hud_pass(0u);
    CHECK_EQ_INT((int)DSB(p0 + 0x52u), 0x0E);
    CHECK_EQ_INT((int)DSB(p0 + 0x43u) & 0x01, 0x00);

    /* D: +0x52 == 3 (the demo's other entered state) must take its own handler
     * 0x35D7C, which clears slot+0x53/+0x54 and then, when the 0x3CF38 chain
     * reports no hit (no armed hitbox), re-arms slot+0x54 = 2, slot+0x53 = 4.
     * The default handler leaves them. Seeded 0xAA differs from both. */
    fight_reset_slots();           /* no armed hitbox */
    DSB(p0 + 0x42u) = 0;
    DSB(p0 + 0x52u) = 3;
    DSB(p0 + 0x53u) = 0xAAu;
    DSB(p0 + 0x54u) = 0xAAu;
    DSW(DS_001088E0) = 0;
    fight_hud_pass(0u);
    CHECK_EQ_INT((int)DSB(p0 + 0x53u), 4);      /* 0x35D7C's no-hit re-arm */
    CHECK_EQ_INT((int)DSB(p0 + 0x54u), 2);
    CHECK_EQ_INT((int)DSB(p0 + 0x52u), 3);      /* 0x35D7C does not re-state */

    /* E: +0x52 == 4 dispatches to 0x35F84 (the table's 0x34C53 entry), which
     * writes +0x52 = 0x14 through the landing gate. The seeded 0x52 = 4
     * differs from the post-value, so a no-op entry fails. */
    mem_fill(FIGHT_RECS, 0, 0x400u);
    fight_reset_actors();
    fight_reset_slot0(p0, r0);
    DSD(DS_001077B0) = r0;                      /* 0x3C148/0x3C16C read this base */
    DSB(p0 + 0x7Au) = 0;                        /* char 0: threshold 0x1600 */
    DSD(p0 + 0x30u) = 0;
    DSW(r0 + 0x36u) = 0xFFFFu;                  /* negative: the gate passes */
    DSB(r0 + 0x51u) = 0;
    DSB(p0 + 0x42u) = 0;
    DSB(p0 + 0x43u) = 0;
    DSB(p0 + 0x52u) = 4;
    DSW(DS_001088E0) = 0;
    fight_hud_pass(0u);
    CHECK_EQ_INT((int)DSB(p0 + 0x52u), 0x14);   /* 0x35F84's landing state */

    /* F: +0x52 == 12 dispatches to 0x361C8 (0x34C9A); its +0x58 == 3 case
     * writes +0x52 = 9. Seeded +0x52 = 12 differs. */
    DSB(p0 + 0x43u) = 0;
    DSB(p0 + 0x58u) = 3;
    DSW(r0 + 0x34u) = 0x0010u;                  /* |+0x34| = 0x10 < 0x11 */
    DSB(p0 + 0x52u) = 12;
    fight_hud_pass(0u);
    CHECK_EQ_INT((int)DSB(p0 + 0x52u), 9);      /* 0x361C8's case 3 */
}

/* ---- Task 6: the 0x3C88C hitbox machine and the 0x3CF38 hit chain --------
 * The per-function fixtures are record §7.1-§7.5, §7.7-§7.9 and §7.11. The
 * slot fields live at DS_001077B0 + off; the fighter record is a scratch
 * address so a wrong slot/record base fails. */

/* §7.8 0x3C600: the per-attack-frame descriptor. */
static void check_hit_frame_desc(void)
{
    u32 saved = DSD(DS_00101514);
    u32 tab = FIGHT_RECS + 0x400u;

    (void)tf_hit_fixture(0);
    CHECK_EQ_INT((int)hit_frame_desc(0u, 0u), 0x000BFE3C);

    /* slot+0x63 clear: the controller selector. */
    mem_fill(tab, 0, 0x300u);
    DSD(DS_00101514) = tab;
    DSB(DS_001077B0 + 0x63u) = 0;
    DSW(tab + 0x2D4u) = 2;                  /* the sel-2 path */
    CHECK_EQ_INT((int)hit_frame_desc(0u, 0u), (int)DSD(0x000C619Cu));
    DSW(tab + 0x2D4u) = 0;                  /* the sel-0/4/6 path */
    CHECK_EQ_INT((int)hit_frame_desc(0u, 0u), (int)DSD(0x000C6B9Cu));

    DSD(DS_00101514) = saved;
}

/* §7.7 0x3C6A8: seed/clear a slot. */
static void check_hit_slot_seed(void)
{
    (void)tf_hit_fixture(0);
    DSW(DS_00107D58) = 0x1234;
    DSW(DS_00107E58) = 0x5678;
    DSW(DS_00107DD8) = 0x9ABC;
    hit_slot_seed(0u, 0u, 0u);
    CHECK_EQ_INT((int)DSW(DS_00107D58), 0);
    CHECK_EQ_INT((int)DSW(DS_00107E58), 0);
    CHECK_EQ_INT((int)DSW(DS_00107DD8), 3);     /* frame 0's +0xC */
}

/* §7.9 0x3C88C: the phase-8 decrement and the clear. */
static void check_hit_machine(void)
{
    (void)tf_hit_fixture(0);
    DSW(DS_00107D58) = 8;
    DSW(DS_00107DD8) = 3;
    hit_slot_step();
    CHECK_EQ_INT((int)DSW(DS_00107DD8), 2);     /* decrement, still >= 1 */
    CHECK_EQ_INT((int)DSW(DS_00107D58), 8);

    (void)tf_hit_fixture(0);
    DSW(DS_00107D58) = 8;
    DSW(DS_00107DD8) = 0;
    hit_slot_step();
    CHECK_EQ_INT((int)DSW(DS_00107D58), 0);     /* signed -1 < 1 clears */
    CHECK_EQ_INT((int)DSW(DS_00107DD8), 3);     /* reloaded from frame 0 */

    /* Phase 7 with a live stun runs the 2..7 block; the exact outcome is
     * data-dependent (0x3C758/0x3C800 are §6.3), so no assertion here. */
    (void)tf_hit_fixture(0);
    DSW(DS_00107D58) = 7;
    DSW(DS_00107DD8) = 5;
    hit_slot_step();

    /* The arm store 0x3C9A3: phase 0 with frame_table[phase].dword0 == 0 (the
     * shipped char-0 entries read 0x00080000, so the entry is seeded here) and a
     * connect that returns 0 (command 0) writes phase = 8. */
    {
        u32 saved0 = DSD(0x000BFE3Cu), saved1 = DSD(0x000BFE3Cu + 0x14u);
        (void)tf_hit_fixture(0);
        DSD(0x000BFE3Cu) = 0;
        DSD(0x000BFE3Cu + 0x14u) = 0;
        DSW(DS_00107D58) = 0;
        DSW(DS_001088E0) = 0;
        hit_slot_step();
        CHECK_EQ_INT((int)DSW(DS_00107D58), 8);     /* armed */
        DSD(0x000BFE3Cu) = saved0;
        DSD(0x000BFE3Cu + 0x14u) = saved1;
    }
}

/* §7.2 0x3CD44 and §7.3 0x3CCEC: the armed scan and the stance test. */
static void check_hit_scan_stance(void)
{
    (void)tf_hit_fixture(0);
    DSW(DS_00107D58 + 0x0Au) = 8;               /* index 5 */
    CHECK_EQ_INT((int)hit_scan(0u), 5);
    DSW(DS_00107D58 + 0x0Au) = 0;
    CHECK_EQ_INT((int)hit_scan(0u), -1);

    /* Entry 0: d = 0x01, e = 0x00. */
    DSB(DS_001077B0 + 0x54u) = 0;
    CHECK_EQ_INT(hit_stance_ok(0u, 0u), 1);     /* d, stance 0 */
    DSB(DS_001077B0 + 0x54u) = 1;
    CHECK_EQ_INT(hit_stance_ok(0u, 0u), 1);     /* d, stance 1 */
    DSB(DS_001077B0 + 0x54u) = 2;
    CHECK_EQ_INT(hit_stance_ok(0u, 0u), 0);     /* e = 0 at stance 2 */
    {
        u8 saved = DSB(0x000C61A3u);
        DSB(0x000C61A3u) = 1;                   /* seed the e flag */
        CHECK_EQ_INT(hit_stance_ok(0u, 0u), 1);
        DSB(0x000C61A3u) = saved;
    }
}

/* §7.5 0x3CD94 and §7.11 0x3CE24: the immunity bitmask and its gate. */
static void check_hit_immunity_gate(void)
{
    (void)tf_hit_fixture(0);
    DSB(DS_001077B0 + 0x5Fu) = 0xFFu;
    CHECK_EQ_INT(hit_immunity(0u, 0u), 1);      /* the 0xFF early-out */

    DSB(DS_001077B0 + 0x5Fu) = 0;               /* row 0, c = 0x20 */
    CHECK_EQ_INT(hit_immunity(0u, 0u), 1);
    DSB(DS_001077B0 + 0x5Fu) = 0x0F;            /* row 0x0F is row 0's twin */
    CHECK_EQ_INT(hit_immunity(0u, 0u), 1);
    DSB(DS_001077B0 + 0x5Fu) = 0x20;            /* rows 0x20.. are zero */
    CHECK_EQ_INT(hit_immunity(0u, 0u), 0);

    /* The 0x3CE24 gate: the signed byte slot+0x56 <= 5. */
    DSB(DS_001077B0 + 0x5Fu) = 0xFFu;
    DSB(DS_001077B0 + 0x56u) = 6;
    CHECK_EQ_INT(hit_gate(0u, 0u), 0);
    DSB(DS_001077B0 + 0x56u) = 5;               /* the boundary */
    CHECK_EQ_INT(hit_gate(0u, 0u), 1);
    DSB(DS_001077B0 + 0x56u) = 0;
    CHECK_EQ_INT(hit_gate(0u, 0u), 1);
}

/* §7.11 0x4CE70, 0x3CBC4, 0x3CC58, 0x34D8C: the allow-list, the reaction
 * variants and the palette-flash pair. */
static void check_hit_reactions(void)
{
    (void)tf_hit_fixture(0);
    CHECK_EQ_INT(hit_reaction_allow(0u, 0x20u), 1);
    CHECK_EQ_INT(hit_reaction_allow(0u, 0x28u), 0);

    DSB(DS_001077B0 + 0x54u) = 2;
    CHECK_EQ_INT(hit_reaction_a(0u), 0x16);
    CHECK_EQ_INT(hit_reaction_b(0u), 0x17);
    DSB(DS_001077B0 + 0x54u) = 0;
    DSW(DS_001088E0) = 0x4000;
    CHECK_EQ_INT(hit_reaction_a(0u), 0x14);
    CHECK_EQ_INT(hit_reaction_b(0u), 0x15);

    DSB(DS_001078FA) = 2;
    DSB(FIGHT_RECS + 0x59u) = 0x55;             /* the record, not the slot */
    DSB(FIGHT_RECS + 0x100u + 0x59u) = 0x55;
    DSB(DS_001077B0 + 0x59u) = 0x55;            /* the slot must stay untouched */
    hit_flash_pair(0u);
    CHECK_EQ_INT((int)DSB(FIGHT_RECS + 0x59u), 1);
    CHECK_EQ_INT((int)DSB(FIGHT_RECS + 0x100u + 0x59u), 0xFF);
    CHECK_EQ_INT((int)DSB(DS_001077B0 + 0x59u), 0x55);

    DSB(DS_001078FA) = 1;
    DSB(FIGHT_RECS + 0x59u) = 0x55;
    DSB(FIGHT_RECS + 0x100u + 0x59u) = 0x55;
    hit_flash_pair(0u);
    CHECK_EQ_INT((int)DSB(FIGHT_RECS + 0x59u), 0x55);
    CHECK_EQ_INT((int)DSB(FIGHT_RECS + 0x100u + 0x59u), 0x55);
}

/* §7.4 0x3CE58: the validate-and-drive. Input A drives the reaction; Input B
 * fails the 0x3CE24 gate; Input C takes the +0x52 = 9 arm via reaction 0. */
static void check_hit_reaction_drive(void)
{
    u16 saved_react = DSW(0x000C619Cu + 4u);
    u16 saved_ac = DSW(0x001080ACu);            /* the last block's 0x3D1E0 store */

    (void)tf_hit_fixture(0);
    DSB(DS_001077B0 + 0x7Cu) = 0;
    DSB(DS_001077B0 + 0x52u) = 0x55;            /* §7.4: unchanged, not 0 */
    /* Reaction 0x20's callback 0x3D17C (record §25) returns at 0x3D187 with a
     * live projectile in slot +0x08, so +0x5F keeps 0x34E2C's 0x20. */
    DSD(DS_001077B0 + 0x08u) = 0x00ABCDEFu;
    DSW(DS_00107D58) = 8;
    DSW(DS_001077B0 + 0x84u) = 0x10;
    CHECK_EQ_INT(hit_reaction_drive(0u, 0u), 1);
    CHECK_EQ_INT((int)DSW(DS_001077B0 + 0x84u), 0x11);
    CHECK_EQ_INT((int)DSB(DS_001077B0 + 0x5Fu), 0x20);
    CHECK_EQ_INT((int)DSB(DS_001077B0 + 0x52u), 0x55);

    (void)tf_hit_fixture(0);
    DSB(DS_001077B0 + 0x56u) = 6;               /* the gate rejects */
    CHECK_EQ_INT(hit_reaction_drive(0u, 0u), 0);

    (void)tf_hit_fixture(0);
    DSB(DS_001077B0 + 0x52u) = 0x55;            /* seeded, must move to 9 */
    DSW(DS_001077B0 + 0x6Au) = 0x40;
    DSW(0x000C619Cu + 4u) = 0;                  /* reaction 0: stream != 0 */
    CHECK_EQ_INT(hit_reaction_drive(0u, 0u), 1);
    CHECK_EQ_INT((int)DSB(DS_001077B0 + 0x52u), 9);
    CHECK_EQ_INT((int)DSB(DS_001077B0 + 0x53u), 8);
    CHECK_EQ_INT((int)DSW(DS_001077B0 + 0x6Au), 0x41);
    DSW(0x000C619Cu + 4u) = saved_react;

    /* §7.11 0x34E2C: the +0x59 pair writes the RECORD (0x34F2E/0x34F47
     * dereference the slot first), not the slot. Isolated from 0x34D8C. */
    (void)tf_hit_fixture(0);
    DSB(DS_001078FA) = 2;
    DSB(FIGHT_RECS + 0x59u) = 0x55;
    DSB(FIGHT_RECS + 0x100u + 0x59u) = 0x55;
    DSB(DS_001077B0 + 0x59u) = 0x55;
    DSB(DS_001077B0 + 0x94u + 0x59u) = 0x55;
    hit_reaction_apply(0u, 0x20u);
    CHECK_EQ_INT((int)DSB(FIGHT_RECS + 0x59u), 1);
    CHECK_EQ_INT((int)DSB(FIGHT_RECS + 0x100u + 0x59u), 0xFF);
    CHECK_EQ_INT((int)DSB(DS_001077B0 + 0x59u), 0x55);
    CHECK_EQ_INT((int)DSB(DS_001077B0 + 0x94u + 0x59u), 0x55);
    DSW(0x001080ACu) = saved_ac;
}

/* §7.1 0x3CF38: a resolved hit consumes the hitbox and drives the reaction. */
static void check_hit_chain(void)
{
    (void)tf_hit_fixture(0);
    DSW(DS_00107D58) = 8;                       /* armed hitbox 0 */
    DSB(DS_001077B0 + 0x7Cu) = 0;
    DSD(DS_001077B0 + 0x08u) = 0x00ABCDEFu;     /* 0x3D17C returns (§25) */
    CHECK_EQ_INT(hit_chain_resolve(0u), 1);
    CHECK_EQ_INT((int)DSB(DS_001077B0 + 0x55u), 0);
    CHECK_EQ_INT((int)DSB(DS_001077B0 + 0x5Fu), 0x20);
    CHECK_EQ_INT((int)DSB(DS_001077B0 + 0x7Cu), 1);
    CHECK_EQ_INT((int)DSW(DS_00107D58), 0);     /* consumed */

    /* No armed hitbox: the raw returns at 0x3CF5E without touching +0x55. */
    (void)tf_hit_fixture(0);
    DSW(DS_00107D58) = 7;
    DSB(DS_001077B0 + 0x55u) = 0x11;
    DSB(DS_001077B0 + 0x7Cu) = 0x40;
    CHECK_EQ_INT(hit_chain_resolve(0u), 0);
    CHECK_EQ_INT((int)DSB(DS_001077B0 + 0x55u), 0x11);  /* untouched */
    CHECK_EQ_INT((int)DSB(DS_001077B0 + 0x7Cu), 0x40);

    /* Armed but the reaction gate fails: the else path resets +0x5F/+0x55. */
    (void)tf_hit_fixture(0);
    DSW(DS_00107D58) = 8;
    DSB(DS_001077B0 + 0x56u) = 6;
    DSB(DS_001077B0 + 0x55u) = 0x11;
    DSB(DS_001077B0 + 0x7Cu) = 0x40;
    CHECK_EQ_INT(hit_chain_resolve(0u), 0);
    CHECK_EQ_INT((int)DSB(DS_001077B0 + 0x5Fu), 0xFF);
    CHECK_EQ_INT((int)DSB(DS_001077B0 + 0x55u), 0xFF);
    CHECK_EQ_INT((int)DSB(DS_001077B0 + 0x7Cu), 0x40);
}

/* §7.11 0x339AC, 0x188AC, 0x188DC, 0x1890C, 0x32BAC. */
static void check_hit_helpers(void)
{
    u32 out[6];
    u32 rec = tf_hit_fixture(0), p1 = FIGHT_RECS + 0x100u;

    /* 0x339AC: rec+0x51 selects the slot pair. */
    DSB(rec + 0x51u) = 1;
    hit_anim_ctx(out, rec);
    CHECK_EQ_INT((int)out[0], 1);
    CHECK_EQ_INT((int)out[1], 0);
    CHECK_EQ_INT((int)out[2], (int)(DS_001077B0 + 0x94u));
    CHECK_EQ_INT((int)out[3], (int)DS_001077B0);
    CHECK_EQ_INT((int)out[4], (int)p1);
    CHECK_EQ_INT((int)out[5], (int)rec);

    /* 0x188AC: write the record's +0x18/+0x1C. */
    hit_anchor_set(0u, 0x1111u, 0x2222u);
    CHECK_EQ_INT((int)DSD(rec + 0x18u), 0x1111);
    CHECK_EQ_INT((int)DSD(rec + 0x1Cu), 0x2222);

    /* 0x188DC: slot+0x2C = x; the 0x18714 tail's clean arm is a self-write. */
    DSB(DS_001077B0 + 0x42u) = 0x08u;
    DSD(DS_001077B0 + 0x2Cu) = 0xAAu;
    hit_anchor_x(0u, 0x3333u);
    CHECK_EQ_INT((int)DSD(DS_001077B0 + 0x2Cu), 0x3333);

    /* 0x1890C: rec+0x1C += (y - slot+0x30) after a latch. */
    DSB(DS_001077B0 + 0x42u) = 0x08u;
    DSB(DS_001077B0 + 0x41u) = 0;
    DSD(rec + 0x18u) = 0xAAu;
    DSD(rec + 0x1Cu) = 0x100u;
    hit_anchor_y(0u, 0x60u);
    CHECK_EQ_INT((int)DSD(rec + 0x1Cu), 0x60);

    /* 0x32BAC: the raw entry is a one-byte RET (record correction, raw wins);
     * the port's hit_sound has no observable effect. */
    DSD(DS_001077B0) = 0xDEADBEEFu;
    DSB(DS_001077B0 + 0x7Cu) = 0x5A;
    hit_sound(0u);
    CHECK_EQ_INT((int)DSD(DS_001077B0), (int)0xDEADBEEFu);
    CHECK_EQ_INT((int)DSB(DS_001077B0 + 0x7Cu), 0x5A);
}

/* Task 6b: the 0x3531C/0x350D0 +0x53 machine and the 0x1DE64 reaction picker.
 * The transitions are the dosbox-x-measured ones (record §7.8): 0x350D0 drives
 * +0x52 to 3 via 0x3BDDC (0x3520E), 0x3531C case 8 calls the chain at 0x354BC,
 * and 0x1DE64 maps the command word to the reaction codes. */
static void check_state_machine(void)
{
    u32 s0 = DS_001077B0, s1 = DS_001077B0 + 0x94u;
    u32 r0 = FIGHT_RECS, r1 = FIGHT_RECS + 0x100u;
    u32 saved_tab = DSD(DS_00101514);
    u32 tab = FIGHT_RECS + 0x500u;

    /* A: +0x53 = 0 (0x3531C's default) and a bit-15 command with +0x52 = 0x0E
     * drive +0x52 to 3 through 0x3BDDC (0x3520E). Seeded +0x54 = 0x55 and
     * +0x53 = 0 differ from the post-conditions 2 and 4. */
    (void)tf_hit_fixture(0);
    fight_reset_slot_pair(s0, s1, r0, r1);
    DSB(r0 + 0x51u) = 0;
    DSB(r1 + 0x51u) = 1;
    DSB(s0 + 0x52u) = 0x0Eu;
    DSB(s0 + 0x53u) = 0;
    DSB(s0 + 0x54u) = 0x55u;
    DSB(s0 + 0x43u) = 0;
    DSB(s0 + 0x41u) = 0;
    DSW(s0 + 0x78u) = 0;
    DSD(s0 + 0x40u) = 0;
    DSW(DS_001088E0) = 0x8000u;
    DSD(DS_00107D40) = 0xDEADBEEFu;
    fighter_state_3531c(0u);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 3);      /* 0x3BF0A via 0x3520E */
    CHECK_EQ_INT((int)DSB(s0 + 0x54u), 2);
    CHECK_EQ_INT((int)DSB(s0 + 0x53u), 4);
    CHECK_EQ_INT((int)DSB(DS_001078F8), 1);
    CHECK_EQ_INT((int)DSD(DS_00107D40), 0x000BEF28);  /* char 0, no 0x4000 */

    /* B: +0x53 = 4 increments +0x56 (0x3540E) and touches nothing else. */
    DSB(s0 + 0x53u) = 4;
    DSB(s0 + 0x56u) = 0x11u;
    DSB(s0 + 0x52u) = 0x77u;
    fighter_state_3531c(0u);
    CHECK_EQ_INT((int)DSB(s0 + 0x56u), 0x12);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 0x77);

    /* C1: +0x53 = 8 with slot+0x88 below the 0xBDBE8 = 3 gate and +0x5F < 0x18
     * calls the 0x3CF38 chain at 0x354BC; an armed hitbox resolves, so +0x7C
     * (the hit counter) increments. */
    (void)tf_hit_fixture(0);
    fight_reset_slot_pair(s0, s1, r0, r1);
    DSB(r0 + 0x51u) = 0;
    DSB(r1 + 0x51u) = 1;
    DSB(s0 + 0x53u) = 8;
    DSB(s0 + 0x56u) = 0;
    DSW(s0 + 0x88u) = 0;                        /* 3 > 0: the gate passes */
    DSB(s0 + 0x5Fu) = 0x10u;                    /* 0x34E20: 0x10 < 0x18 */
    DSB(s0 + 0x7Cu) = 0;
    DSW(DS_00107D58) = 8;                       /* armed hitbox 0 */
    fighter_state_3531c(0u);
    CHECK_EQ_INT((int)DSB(s0 + 0x7Cu), 1);      /* the chain resolved */
    CHECK_EQ_INT((int)DSB(s0 + 0x56u), 1);      /* 0x3547A still ran */

    /* C2: slot+0x88 = 3 closes the 0xBDBE8 gate, so the chain is not called:
     * the hit counter stays 0 and +0x5F keeps the seeded 0x10. */
    (void)tf_hit_fixture(0);
    fight_reset_slot_pair(s0, s1, r0, r1);
    DSB(r0 + 0x51u) = 0;
    DSB(r1 + 0x51u) = 1;
    DSB(s0 + 0x53u) = 8;
    DSW(s0 + 0x88u) = 3;                        /* 3 <= 3: the gate closes */
    DSB(s0 + 0x5Fu) = 0x10u;
    DSB(s0 + 0x7Cu) = 0;
    DSW(DS_00107D58) = 8;                       /* armed, but not reached */
    fighter_state_3531c(0u);
    CHECK_EQ_INT((int)DSB(s0 + 0x7Cu), 0);
    CHECK_EQ_INT((int)DSB(s0 + 0x5Fu), 0x10);
    CHECK_EQ_INT((int)DSB(s0 + 0x53u), 8);

    /* C3: +0x5F >= 0x18 closes the 0x34E20 gate the same way. */
    (void)tf_hit_fixture(0);
    fight_reset_slot_pair(s0, s1, r0, r1);
    DSB(s0 + 0x53u) = 8;
    DSW(s0 + 0x88u) = 0;
    DSB(s0 + 0x5Fu) = 0x18u;                    /* 0x34E20: not < 0x18 */
    DSB(s0 + 0x7Cu) = 0;
    DSW(DS_00107D58) = 8;
    fighter_state_3531c(0u);
    CHECK_EQ_INT((int)DSB(s0 + 0x7Cu), 0);
    CHECK_EQ_INT((int)DSB(s0 + 0x53u), 8);

    /* D: 0x39280 clears slot+0x5D and the +0x43 bit 2. */
    (void)tf_hit_fixture(0);
    DSB(DS_001077B0 + 0x5Du) = 0xAAu;
    DSB(DS_001077B0 + 0x43u) = 0xFFu;
    fighter_state_39280(0u);
    CHECK_EQ_INT((int)DSB(DS_001077B0 + 0x5Du), 0);
    CHECK_EQ_INT((int)DSB(DS_001077B0 + 0x43u), 0xFB);

    /* E: 0x34DDC. char 0's threshold is word[0xBD870] = 0x1180. */
    (void)tf_hit_fixture(0);
    DSD(DS_001077B0 + 0x30u) = 0x2000u;         /* above the threshold */
    DSD(FIGHT_RECS + 0x36u) = 0xFFFFFFFFu;
    CHECK_EQ_INT(fighter_34ddc(0u), 1);
    DSD(DS_001077B0 + 0x30u) = 0x1000u;         /* below, +0x36 negative */
    CHECK_EQ_INT(fighter_34ddc(0u), 0);
    DSD(FIGHT_RECS + 0x36u) = 0;                /* below but +0x36 clear */
    CHECK_EQ_INT(fighter_34ddc(0u), 1);

    /* F: 0x34E20's 0x18 boundary. */
    CHECK_EQ_INT(fighter_34e20(0x17u), 1);
    CHECK_EQ_INT(fighter_34e20(0x18u), 0);

    /* G: 0x3C59C's test-and-set, the preamble gate. */
    DSD(DS_00107D50) = 0;
    CHECK_EQ_INT(fighter_pass_flag(3u, 0u), 0);
    CHECK_EQ_INT((int)DSD(DS_00107D50), 0x08);
    CHECK_EQ_INT(fighter_pass_flag(3u, 0u), 1);
    CHECK_EQ_INT(fighter_pass_flag(2u, 0u), 0); /* a different bit is free */
    CHECK_EQ_INT((int)DSD(DS_00107D50), 0x0C);

    /* H: 0x1DE64's command-word map. slot+0x63 = 1 (the fixture) skips the
     * 0x46460 scan, so the result is the raw command word. */
    (void)tf_hit_fixture(0);
    DSD(DS_00101514) = tab;
    mem_fill(tab, 0, 0x300u);
    DSW(tab + 0x2D4u) = 0;
    DSW(DS_001088E0) = 0;
    CHECK_EQ_INT((int)hit_reaction_pick(0u, 0u), 0xFF);
    DSW(DS_001088E0) = 0x0001u;
    CHECK_EQ_INT((int)hit_reaction_pick(0u, 2u), 0x0C);
    DSW(DS_001088E0) = 0x0002u;
    CHECK_EQ_INT((int)hit_reaction_pick(0u, 2u), 0x0D);
    DSW(DS_001088E0) = 0x0004u;
    CHECK_EQ_INT((int)hit_reaction_pick(0u, 2u), 0x0E);
    DSW(DS_001088E0) = 0x0008u;
    CHECK_EQ_INT((int)hit_reaction_pick(0u, 2u), 0x0F);
    DSW(DS_001088E0) = 0x4001u;                 /* the 0x4000 arm */
    CHECK_EQ_INT((int)hit_reaction_pick(0u, 0u), 8);
    DSW(DS_001088E0) = 0x4002u;
    CHECK_EQ_INT((int)hit_reaction_pick(0u, 0u), 9);
    DSD(DS_00101514) = saved_tab;
}

/* Task 3: the remaining 0x34B14 +0x52 handlers. Each case seeds the inputs the
 * handler reads and a sentinel that differs from the record §2.2 post-value, so
 * a dropped store fails. The dispatch wiring is asserted by check_state_dispatch
 * and check_hud_pass_machine. */
static void check_state_handlers(void)
{
    u32 s0 = DS_001077B0, s1 = DS_001077B0 + 0x94u;
    u32 r0 = FIGHT_RECS, r1 = FIGHT_RECS + 0x100u;

    /* A: 0x35F84 (+0x52 = 4). The landing gate (word[0xBD882] >> 16 = 0x1600
     * for char 0 >= slot+0x30, rec+0x36 <= 0) writes +0x52 = 0x14, +0x53 = 4,
     * +0x78 = word[0xBDC16] and rec+0x24 = 0. The sentinel 0xAA differs from
     * every post-value. */
    (void)tf_hit_fixture(0);
    fight_reset_slot_pair(s0, s1, r0, r1);
    DSB(r0 + 0x51u) = 0;
    DSB(s0 + 0x7Au) = 0;                        /* char 0: threshold 0x1600 */
    DSD(s0 + 0x30u) = 0;                        /* below the threshold */
    DSW(r0 + 0x36u) = 0xFFFFu;                  /* negative */
    DSD(r0 + 0x24u) = 0xDEADBEEFu;
    DSW(r0 + 0x34u) = 0xAAAAu;                  /* 0x3C148's +0x34 sentinel */
    DSB(r0 + 0x42u) = 0xAAu;                    /* 0x3C148's +0x42 sentinel */
    DSB(r0 + 0x43u) = 0xAAu;                    /* 0x3C148's +0x43 sentinel */
    DSW(r0 + 0x44u) = 0xAAAAu;                  /* 0x3C16C's +0x44 sentinel */
    DSB(s0 + 0x41u) = 0;
    DSB(s0 + 0x52u) = 0xAAu;
    DSB(s0 + 0x53u) = 0xAAu;
    DSW(s0 + 0x78u) = 0xAAAAu;
    fighter_state_35f84(s0, r0);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 0x14);
    CHECK_EQ_INT((int)DSB(s0 + 0x53u), 4);
    CHECK_EQ_INT((int)DSW(s0 + 0x78u), (int)DSW(0x000BDC16u));
    CHECK_EQ_INT((int)DSD(r0 + 0x24u), 0);
    CHECK_EQ_INT((int)DSB(s0 + 0x41u) & 0x80, 0x80);
    /* 0x3C148/0x3C16C: every seeded sentinel is cleared. A dropped helper call
     * leaves one of them non-zero. */
    CHECK_EQ_INT((int)DSW(r0 + 0x34u), 0);
    CHECK_EQ_INT((int)DSB(r0 + 0x42u), 0);
    CHECK_EQ_INT((int)DSB(r0 + 0x43u), 0);
    CHECK_EQ_INT((int)DSW(r0 + 0x36u), 0);
    CHECK_EQ_INT((int)DSW(r0 + 0x44u), 0);

    /* A2: the same with rec+0x36 > 0 (the gate fails): the state is untouched. */
    (void)tf_hit_fixture(0);
    fight_reset_slot_pair(s0, s1, r0, r1);
    DSB(r0 + 0x51u) = 0;
    DSB(s0 + 0x7Au) = 0;
    DSD(s0 + 0x30u) = 0;
    DSW(r0 + 0x36u) = 1;                        /* positive: the gate fails */
    DSB(s0 + 0x52u) = 0xAAu;
    fighter_state_35f84(s0, r0);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 0xAA);

    /* B: 0x361C8 (+0x52 = 12). +0x58 = 3 with |rec+0x32 >> 16| < 0x11 zeroes
     * rec+0x34 and writes +0x52 = 9. The sentinel +0x52 = 0xAA fails if the
     * store is dropped. */
    (void)tf_hit_fixture(0);
    fight_reset_slot_pair(s0, s1, r0, r1);
    DSB(r0 + 0x51u) = 0;
    DSB(s0 + 0x58u) = 3;
    DSW(r0 + 0x34u) = 0x0010u;                  /* |+0x34| = 0x10 < 0x11 */
    DSB(s0 + 0x52u) = 0xAAu;
    fighter_state_361c8(s0, r0);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 9);
    CHECK_EQ_INT((int)DSW(r0 + 0x34u), 0);

    /* B2: +0x58 = 2 subtracts 0x38 from rec+0x36 and leaves +0x52. */
    DSB(s0 + 0x58u) = 2;
    DSW(r0 + 0x36u) = 0x0100u;
    DSB(s0 + 0x52u) = 0xAAu;
    fighter_state_361c8(s0, r0);
    CHECK_EQ_INT((int)DSW(r0 + 0x36u), 0x00C8);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 0xAA);

    /* C: 0x36300 (+0x52 = 13), the same case-3 body. */
    (void)tf_hit_fixture(0);
    fight_reset_slot_pair(s0, s1, r0, r1);
    DSB(r0 + 0x51u) = 0;
    DSB(s0 + 0x58u) = 3;
    DSW(r0 + 0x34u) = 0x0010u;                  /* |+0x34| = 0x10 < 0x11 */
    DSB(s0 + 0x52u) = 0xAAu;
    fighter_state_36300(s0, r0);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 9);
    CHECK_EQ_INT((int)DSW(r0 + 0x34u), 0);

    /* D: 0x36710 (+0x52 = 17). +0x58 = 2 with rec+0x36 == 0 and rec+0x1C == 0
     * writes rec+0x43 = byte[0xBD89A] = 8 and +0x52 = 9. The sentinels differ. */
    (void)tf_hit_fixture(0);
    fight_reset_slot_pair(s0, s1, r0, r1);
    DSB(r0 + 0x51u) = 0;
    DSB(s0 + 0x58u) = 2;
    DSW(r0 + 0x36u) = 0;
    DSD(r0 + 0x1Cu) = 0;
    DSB(r0 + 0x43u) = 0xAAu;
    DSB(s0 + 0x52u) = 0xAAu;
    fighter_state_36710(s0, r0);
    CHECK_EQ_INT((int)DSB(r0 + 0x43u), (int)DSB(0x000BD89Au));
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 9);

    /* D2: the first step (+0x58 = 0 -> 1) touches only +0x58. */
    (void)tf_hit_fixture(0);
    DSD(DS_001077A8) = s0;
    DSD(s0) = r0;
    DSB(r0 + 0x51u) = 0;
    DSB(s0 + 0x58u) = 0;
    DSB(s0 + 0x52u) = 0xAAu;
    fighter_state_36710(s0, r0);
    CHECK_EQ_INT((int)DSB(s0 + 0x58u), 1);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 0xAA);

    /* E: 0x399CC (+0x52 = 7). The self slot gets +0x41 bit 2 and +0x74 = 0. */
    (void)tf_hit_fixture(0);
    DSD(DS_001077A8) = s0;
    DSD(s0) = r0;
    DSB(r0 + 0x51u) = 0;
    DSB(s0 + 0x41u) = 0;
    DSW(s0 + 0x74u) = 0xAAAAu;
    DSB(s0 + 0x5Du) = 1;                        /* skip 0x367DC */
    fighter_state_399cc(0u);
    CHECK_EQ_INT((int)DSB(s0 + 0x41u) & 4, 4);
    CHECK_EQ_INT((int)DSW(s0 + 0x74u), 0);

    /* F: 0x36430 (+0x52 = 5). With 0x365C8/0x36638 both clear and the command
     * word 0, the handler writes +0x52 = 9 and +0x54 = 0. */
    (void)tf_hit_fixture(0);
    fight_reset_slot_pair(s0, s1, r0, r1);
    DSB(r0 + 0x51u) = 0;
    DSB(r1 + 0x51u) = 1;
    DSB(s0 + 0x42u) = 0;
    DSB(s0 + 0x43u) = 0;
    DSB(s0 + 0x54u) = 0;
    DSW(DS_001088E0) = 0;
    DSB(s0 + 0x52u) = 0xAAu;
    DSB(s0 + 0x54u) = 0xAAu;
    fighter_state_36430(s0, r0, 0u);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 9);
    CHECK_EQ_INT((int)DSB(s0 + 0x54u), 0);

    /* G: 0x364FC (+0x52 = 21). Command 0x4000 (the +0x40 high bit) with
     * 0x1A640 == 0 writes +0x52 = 5. */
    (void)tf_hit_fixture(0);
    fight_reset_slot_pair(s0, s1, r0, r1);
    DSB(r0 + 0x51u) = 0;
    DSB(r1 + 0x51u) = 1;
    DSB(s0 + 0x42u) = 0;
    DSB(s0 + 0x43u) = 0;
    DSB(s0 + 0x54u) = 0;
    DSW(r0 + 0x28u) = 0;                        /* facing clear: 0x1A640 = 0 */
    DSW(DS_001088E0) = 0x4000u;
    DSB(s0 + 0x52u) = 0xAAu;
    fighter_state_364fc(s0, r0, 0u);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 5);

    /* H: 0x1A640's two non-zero arms and its zero arm. */
    (void)tf_hit_fixture(0);
    DSD(DS_001077B0) = r0;
    DSW(r0 + 0x28u) = 0;                        /* facing clear */
    DSW(DS_001088E0) = 0x1000u;                 /* 0x1000 in the high byte */
    CHECK_EQ_INT(fighter_1a640(0u), 0x1000);
    DSW(r0 + 0x28u) = 0x4000u;                  /* facing set */
    DSW(DS_001088E0) = 0x2000u;
    CHECK_EQ_INT(fighter_1a640(0u), 0x2000);
    DSW(DS_001088E0) = 0;
    CHECK_EQ_INT(fighter_1a640(0u), 0);
}

/* The five remaining +0x52 handlers (record §1). Each case seeds the inputs the
 * handler reads plus a sentinel that differs from the raw's post-value, so a
 * dropped store fails. The data tables the handlers read are seeded because the
 * unit process has no image loaded, and the seeded scalars are restored. */
static void check_gap_handlers(void)
{
    u32 s0 = DS_001077B0, s1 = DS_001077B0 + 0x94u;
    u32 r0 = FIGHT_RECS, r1 = FIGHT_RECS + 0x100u;
    u8 s_max = DSB(DS_000BDA3E);
    u32 s_left = DSD(DS_000BDBEC);
    u16 s_base = DSW(DS_001078DC);
    u8 s_mem1000 = DSB(0x00001000u);
    u32 s_pool = DSD(DS_001014F4);
    u32 s_ec = DSD(DS_001014EC);
    u32 s_pal = DSD(DS_00107798);
    u32 s_tbl0 = DSD(DS_000A8A98);
    u32 s_tbl1 = DSD(DS_000A8A98 + 4u);
    u8  s_5b34 = DSB(DS_00105B34);

    /* A: 0x35D20 (+0x52 = 2). +0x54 == 4 raises DS_001078FE; +0x52 -> 9 and
     * +0x43 bits 0/1 are cleared; +0x53 survives only at 0x0D. */
    (void)tf_hit_fixture(0);
    fight_reset_slot_pair(s0, s1, r0, r1);
    DSB(s0 + 0x52u) = 0xFFu;
    DSB(s0 + 0x43u) = 0xFFu;
    DSB(s0 + 0x53u) = 0x05u;
    DSB(s0 + 0x54u) = 4u;
    DSB(DS_001078FE) = 0;
    fighter_state_35d20(s0, r0);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 9);
    CHECK_EQ_INT((int)DSB(s0 + 0x43u), 0xFC);
    CHECK_EQ_INT((int)DSB(s0 + 0x53u), 0);
    CHECK_EQ_INT((int)DSB(DS_001078FE), 1);

    /* B: 0x35C1C (+0x52 = 2). A negative step underflows the frame to max-1
     * (max = byte[0xBDA3E]); the clamp returns 1 and sets +0x29 bit 8, and the
     * tail sets +0x28 bit 4. max is seeded to 4 so the seek index stays in
     * mem[] (the unit process has no image). */
    (void)tf_hit_fixture(0);
    fight_reset_slot_pair(s0, s1, r0, r1);
    DSB(DS_000BDA3E) = 4;
    DSB(s0 + 0x43u) = 0;
    DSB(r0 + 0x52u) = 0;
    DSB(r0 + 0x58u) = 0xFFu;                    /* step -1 */
    DSB(r0 + 0x29u) = 0;
    DSB(r0 + 0x28u) = 0;
    DSB(r0 + 0x56u) = 0;
    CHECK_EQ_INT(fighter_state_35c1c(s0, r0), 1);
    CHECK_EQ_INT((int)DSB(r0 + 0x52u), 3);      /* max - 1 */
    CHECK_EQ_INT((int)DSB(r0 + 0x29u) & 8, 8);
    CHECK_EQ_INT((int)DSB(r0 + 0x28u) & 4, 4);

    /* C: 0x359E0 (+0x52 = 1). Command 0x2000 makes 0x1A5D4 non-zero and +0x43
     * bit 1 lets the pass tail run: the per-char x delta (0xBDBEC >> 16 = 3)
     * moves rec+0x18, 0x35C1C clamps the frame, and +0x4C = the frame byte << 6
     * is handed to 0x1883C (which adds it to +0x2C). +0x42 bit 3 makes the
     * 0x1883C latch / hit_record_x round-trip rec+0x18 unchanged. */
    (void)tf_hit_fixture(0);
    fight_reset_slot_pair(s0, s1, r0, r1);
    DSB(s0 + 0x42u) = 8;                        /* the latch short path */
    DSB(s1 + 0x42u) = 8;
    DSW(DS_001088E0) = 0x2000u;                 /* 0x1A5D4 != 0 */
    DSB(s0 + 0x43u) = 2;                        /* the +0x43 bit 1 gate */
    DSB(s0 + 0x41u) = 0x40;
    DSB(DS_000EF6DC) = 1;
    DSB(s0 + 0x7Au) = 0;
    DSD(DS_000BDBEC) = 0x00030000u;             /* d = 3 */
    DSB(DS_000BDA3E) = 4;                       /* max, for 0x35C1C */
    DSD(r0 + 0x18u) = 0x400u;
    DSD(r0 + 0x1Cu) = 0x2222u;
    DSB(r0 + 0x52u) = 0;
    DSB(r0 + 0x58u) = 0;
    DSW(r0 + 0x28u) = 0;
    DSB(r0 + 0x29u) = 0;
    DSD(r0 + 0x0Cu) = 0x1000u;                  /* the frame byte table */
    DSB(0x00001000u) = 5;                       /* (s8)5 << 6 = 0x140 */
    DSD(s0 + 0x2Cu) = 0;
    DSD(s0 + 0x30u) = 0;
    DSB(r0 + 0x56u) = 0;
    fighter_state_359e0(s0, r0, 0u);
    CHECK_EQ_INT((int)DSW(s0 + 0x4Cu), 0x140);  /* (s8)5 << 6 */
    CHECK_EQ_INT((int)DSD(r0 + 0x18u), 0x3FD);  /* 0x400 - 3, round-tripped */
    CHECK_EQ_INT((int)DSD(s0 + 0x2Cu), 0x3FD + 0x140);   /* 0x1883C added +0x4C */
    CHECK_EQ_INT((int)DSB(r0 + 0x28u) & 4, 4);  /* 0x35C1C ran */

    /* D: 0x37464 (+0x52 = 8). base = word[0x1078DC] = 0x10; 0x1A570(other) != 0
     * so X = Fo[0x18] - base = 0xF0; |F[0x18] - X| = 0x10 <= 0x200 takes case 0,
     * writing +0x52 = 9 and rec+0x18 = X. */
    (void)tf_hit_fixture(0);
    fight_reset_slot_pair(s0, s1, r0, r1);
    DSW(DS_001078DC) = 0x10;                    /* base */
    DSB(s0 + 0x57u) = 0;
    DSB(s0 + 0x7Au) = 0;
    DSB(s1 + 0x7Au) = 0;
    DSD(r0 + 0x18u) = 0x100u;
    DSD(r1 + 0x18u) = 0x100u;
    DSB(r0 + 0x56u) = 0;
    DSB(r1 + 0x56u) = 0;
    DSB(DS_000BDA3E) = 4;                       /* max, for 0x35B7C */
    DSB(r0 + 0x52u) = 0;
    DSB(r0 + 0x58u) = 0;
    DSB(s0 + 0x43u) = 0;
    DSB(s0 + 0x53u) = 0;
    fighter_state_37464(0u);
    CHECK_EQ_INT((int)DSD(r0 + 0x18u), 0xF0);   /* X = Fo[0x18] - base */
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 9);

    /* E: 0x33B00 (+0x52 = 19). The slot's 0x94 bytes come from `src` and its
     * actor's 0x68 bytes from `dst2`, with +0x5A/+0x5D and the actor's first two
     * dwords restored; +0x08 non-zero also sets +0x64 = 0xFF. */
    (void)tf_hit_fixture(0);
    {
        u32 src = 0x00107BD0u, dst2 = 0x00107B00u, act = 0x3F50000u;
        mem_fill(src, 0x33u, 0x94u);
        mem_fill(dst2, 0x66u, 0x68u);
        mem_fill(act, 0x77u, 0x68u);
        DSD(src + 0u) = act;                    /* the new actor pointer */
        DSD(src + 8u) = 0x12345678u;            /* +0x08 -> the +0x64 reset */
        DSD(dst2 + 0u) = 0xAAAAAAAAu;           /* overwritten by f0 */
        DSD(dst2 + 4u) = 0xBBBBBBBBu;           /* overwritten by f1 */
        DSD(act + 0u) = 0xDEADBEEFu;            /* f0 */
        DSD(act + 4u) = 0xCAFEBABEu;            /* f1 */
        DSB(s0 + 0x5Au) = 0x11u;                /* the saved +0x5A */
        DSB(s0 + 0x5Du) = 0x22u;                /* the saved +0x5D */
        fighter_state_33b00(0u, src, dst2);
        CHECK_EQ_INT((int)DSD(s0 + 0u), (int)act);        /* the slot copy */
        CHECK_EQ_INT((int)DSB(s0 + 0x33u), 0x33);         /* a mid-slot byte */
        CHECK_EQ_INT((int)DSB(s0 + 0x5Au), 0x11);         /* restored */
        CHECK_EQ_INT((int)DSB(s0 + 0x5Du), 0x22);         /* restored */
        CHECK_EQ_INT((int)DSB(s0 + 0x64u), 0xFF);         /* +0x08 was set */
        CHECK_EQ_INT((int)DSD(s0 + 0x08u), 0);            /* +0x08 cleared */
        CHECK_EQ_INT(DSD(act + 0u), 0xDEADBEEFu);         /* F[0] restored */
        CHECK_EQ_INT(DSD(act + 4u), 0xCAFEBABEu);         /* F[1] restored */
        CHECK_EQ_INT((int)DSD(act + 8u), 0x66666666u);    /* mid-actor dword */
    }

    /* F: 0x35E6C (+0x52 = 20). Command 0 (0x3BDDC returns 0) reaches the body:
     * +0x52 -> 9, +0x53/+0x54 cleared, +0x84 incremented, +0x92 cleared and
     * +0x41 bit 0x80 set. */
    (void)tf_hit_fixture(0);
    fight_reset_slot_pair(s0, s1, r0, r1);
    DSB(r0 + 0x51u) = 0;
    DSW(DS_001088E0) = 0;
    DSB(s0 + 0x41u) = 0;
    DSB(s0 + 0x52u) = 0;
    DSB(s0 + 0x53u) = 0xFFu;
    DSB(s0 + 0x54u) = 0xFFu;
    DSW(s0 + 0x84u) = 0;
    DSW(s0 + 0x92u) = 0xFFu;
    DSB(s0 + 0x7Au) = 0;
    DSB(r0 + 0x56u) = 0;
    fighter_state_35e6c(s0, r0);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 9);
    CHECK_EQ_INT((int)DSB(s0 + 0x53u), 0);
    CHECK_EQ_INT((int)DSB(s0 + 0x54u), 0);
    CHECK_EQ_INT((int)DSW(s0 + 0x84u), 1);
    CHECK_EQ_INT((int)DSW(s0 + 0x92u), 0);
    CHECK_EQ_INT((int)DSB(s0 + 0x41u) & 0x80, 0x80);

    /* G: 0x36E78's 0x29BC8 call reads the character from the SLOT, not the
     * actor. Raw 0x36EF0 loads the record DSD(slot) but 0x36EF7 loads the char
     * from DSB(slot+0x7A). The slot char (0) resolves to a zero palette handle,
     * so actor_pset_palette leaves pset+0x18 at its sentinel; the actor char (1)
     * resolves to a non-zero handle, which would release it. Reached through
     * 0x33B00 with the slot's +0x5A >= 0x78. */
    (void)tf_hit_fixture(0);
    fight_reset_slot_pair(s0, s1, r0, r1);
    fight_reset_actors();
    {
        u32 src = 0x00107BD0u, dst2 = 0x00107B00u, act = 0x3F50000u;
        u32 tbl_slot = 0x3F51000u, tbl_actor = 0x3F52000u, old = 0x3F53000u;
        mem_fill(src, 0, 0x94u);
        mem_fill(dst2, 0, 0x68u);
        mem_fill(act, 0, 0x68u);
        DSD(src + 0u) = act;                    /* 0x33B00's rec = DSD(slot) */
        DSD(src + 0x08u) = 0;                   /* skip the +0x64 reset */
        DSB(src + 0x5Bu) = 0;                   /* 0x36E78's second branch */
        DSB(src + 0x7Au) = 0;                   /* the SLOT char -> handle 0 */
        DSB(act + 0x7Au) = 1;                   /* the ACTOR char -> handle 0x1234 */
        DSB(s0 + 0x5Au) = 0x78u;                /* >= 0x78: run 0x36E78 */
        DSW(DS_00104B00) = 0;                   /* != 3: run 0x36E78 */
        DSB(DS_001078FF) = 0;                   /* 0x36E78's palette side */
        DSB(DS_00105B34) = 0;                   /* the per-side table index */
        DSD(DS_000A8A98 + 0u) = tbl_slot;
        DSD(DS_000A8A98 + 4u) = tbl_actor;
        DSD(tbl_slot + 0u) = 0;                 /* handle 0 */
        DSD(tbl_actor + 0u) = 0x1234u;          /* handle 0x1234 */
        DSD(DS_001014F4) = act;                 /* in_pool(act) */
        DSD(DS_001014EC) = FIGHT_ACTORS;        /* actor_pset(act) = FIGHT_ACTORS */
        DSD(old + 4u) = 1;                      /* palette_release(old) drops it */
        DSD(FIGHT_ACTORS + 0x18u) = old;        /* pset+0x18 sentinel */
        fighter_state_33b00(0u, src, dst2);
        CHECK_EQ_INT((int)DSD(FIGHT_ACTORS + 0x18u), (int)old);
        CHECK_EQ_INT((int)DSD(old + 4u), 1);
    }

    DSB(DS_000BDA3E) = s_max;
    DSD(DS_000BDBEC) = s_left;
    DSW(DS_001078DC) = s_base;
    DSB(0x00001000u) = s_mem1000;
    DSD(DS_001014F4) = s_pool;
    DSD(DS_001014EC) = s_ec;
    DSD(DS_00107798) = s_pal;
    DSD(DS_000A8A98) = s_tbl0;
    DSD(DS_000A8A98 + 4u) = s_tbl1;
    DSB(DS_00105B34) = s_5b34;
}

/* Task 6b wiring: fight_hud_pass's 0x35803 call drives +0x52 out of the 0x0E
 * no-op through 0x3531C -> 0x350D0 -> 0x3BDDC. Seeded +0x52 = 0x0E differs
 * from the post-condition 3, and +0x54 = 0x55 from 2. */
static void check_hud_pass_machine(void)
{
    u32 p0 = DS_001077B0, r0 = FIGHT_RECS;

    mem_fill(p0, 0, 0x94u * 2u);                /* the real slot pair */
    mem_fill(FIGHT_RECS, 0, 0x400u);
    mem_fill(FIGHT_ACTORS, 0, 0x80u);
    mem_fill(0x00107D18u, 0, 0x40u);            /* the 0x107D18.. words */
    fight_reset_slots();           /* the three hitbox arrays */
    DSD(DS_001014EC) = FIGHT_ACTORS;
    fight_reset_slot0(p0, r0);
    DSB(p0 + 0x7Au) = 0;
    DSB(p0 + 0x63u) = 1;
    DSB(p0 + 0x5Fu) = 0xFFu;
    DSB(p0 + 0x52u) = 0x0Eu;
    DSB(p0 + 0x53u) = 0;
    DSB(p0 + 0x54u) = 0x55u;
    DSB(p0 + 0x43u) = 0;
    DSB(p0 + 0x41u) = 0;
    DSD(p0 + 0x40u) = 0;
    DSW(p0 + 0x78u) = 0;
    DSB(r0 + 0x51u) = 0;
    DSW(r0 + 0x56u) = 0;
    DSW(r0 + 0x28u) = 0;
    DSD(DS_00104B00) = 3;
    DSW(DS_001088E0) = 0x8000u;
    DSD(DS_00107D50) = 0;                       /* the preamble gate is fresh */

    fight_hud_pass(0u);
    CHECK_EQ_INT((int)DSB(p0 + 0x52u), 3);      /* the machine drove it */
    CHECK_EQ_INT((int)DSB(p0 + 0x54u), 2);
    CHECK_EQ_INT((int)DSB(p0 + 0x53u), 4);
    CHECK_EQ_INT((int)DSB(DS_001078F8), 1);
    CHECK_EQ_INT((int)DSW(p0 + 0x88u), 1);      /* the preamble ran */
}

/* Task 6b fix round 1: 0x367DC's second 0x2BC30 call passes the side's 0x102900
 * record as the record and the fixed 0xE906A stream as the stream (raw
 * 0x36843 EDX = 0xE906A, 0x36848 EAX = 0x102900[side]; 0x2BC30 stores EDX to
 * rec+8 at 0x2BC52, so EDX is the stream). Seeded sentinels at the target
 * record's +0x0C/+8 differ from actors_anim_begin's 0 and 0xE906A. The 0x36BC8
 * twin (0x36CD1, stream 0xE906E) is dead — both its callers require +0x43
 * bit 2, which forces its 0x36CA7 early return — so only 0x367DC is covered. */
static void check_anim_stream_args(void)
{
    u32 tgt = FIGHT_RECS + 0x600u;
    u32 saved900 = DSD(DS_00102900);
    u32 saved900b = DSD(DS_00102900 + 4u);

    /* Reach 0x367DC through 0x3531C case 7: char 4, +0x5F 0x21 and
     * (s32)slot+0x86 >> 16 > 0x5A. Mode 0 (not 3/0x22/0x24) opens the second
     * call. */
    (void)tf_hit_fixture(0);
    DSD(DS_001077A8) = DS_001077B0;
    DSD(DS_001077A8 + 4u) = DS_001077B0 + 0x94u;
    DSB(FIGHT_RECS + 0x51u) = 0;
    DSB(DS_001077B0 + 0x53u) = 7;
    DSB(DS_001077B0 + 0x7Au) = 4;
    DSB(DS_001077B0 + 0x5Fu) = 0x21u;
    DSD(DS_001077B0 + 0x86u) = 0x00600000u;     /* >>16 = 0x60 > 0x5A */
    DSW(DS_00104B00) = 0;
    mem_fill(tgt, 0, 0x94u);
    DSD(DS_00102900) = tgt;
    DSD(tgt + 0x0Cu) = 0xCAFEu;                 /* the sentinel */
    DSD(tgt + 8u) = 0xDEADBEEFu;
    /* The first call's record is FIGHT_RECS (slot+0x00); 0x367DC clears its
     * +0x0C and the slot's +0x52. Seed both so the assertions below cannot pass
     * on a BSS zero (repo convention, cf. test_fight.c:1650). */
    DSD(FIGHT_RECS + 0x0Cu) = 0x00005555u;
    DSB(DS_001077B0 + 0x52u) = 0x55u;

    fighter_state_3531c(0u);

    CHECK_EQ_INT((int)DSD(tgt + 0x0Cu), 0);     /* the 2nd call's record */
    CHECK_EQ_INT((int)DSD(tgt + 8u), 0x000E906A);   /* its stream */
    CHECK_EQ_INT((int)DSD(FIGHT_RECS + 0x0Cu), 0);  /* the 1st call's record */
    CHECK_EQ_INT((int)DSB(DS_001077B0 + 0x52u), 0); /* 0x367DC cleared it */

    DSD(DS_00102900) = saved900;
    DSD(DS_00102900 + 4u) = saved900b;
}

/* Task 3: the 0x349C8 +0x42 bit-6/7 arms' deep callees (0x385B0 and 0x39A10)
 * and the minimal caller chain that wires them. Seeds differ from the
 * post-conditions, so an unwritten field cannot pass.
 *
 * State restore: the cases re-seed the slot pair and records from tf_hit_fixture
 * each time, and the enclosing test_fight snapshots the real slot region
 * (0x1077A0..0x107900, s_slots) and restores it; FIGHT_RECS is a scratch the
 * fixtures re-zero. Only the globals this check perturbs outside those
 * (0x1078DC, 0x104B00, 0x100AF8, 0x1077A8 and the 0x3F40000 scratch) are
 * saved/restored explicitly at the end. */
static void check_deep_callees(void)
{
    u32 s0 = DS_001077B0, s1 = DS_001077B0 + 0x94u;
    u32 r0 = FIGHT_RECS, r1 = FIGHT_RECS + 0x100u;
    u32 s_dc = DSD(DS_001078DC);
    u32 s_40_0 = DSD(s0 + 0x40u);
    u32 s_40_1 = DSD(s1 + 0x40u);
    u32 s_a8_0 = DSD(DS_001077A8);
    u32 s_a8_1 = DSD(DS_001077A8 + 4u);
    u32 s_b00 = DSW(DS_00104B00);
    u32 s_af8 = DSD(DS_00100AF8);
    u32 s_tbl = DSD(0x003F40000u);
    u8  s_42_0 = DSB(s0 + 0x42u);
    u8  s_42_1 = DSB(s1 + 0x42u);
    u8  s_51_0 = DSB(r0 + 0x51u);
    u8  s_51_1 = DSB(r1 + 0x51u);
    u8  s_53_0 = DSB(s0 + 0x53u);
    u8  s_7a_0 = DSB(s0 + 0x7Au);
    u8  s_7a_1 = DSB(s1 + 0x7Au);
    u8  s_59_0 = DSB(r0 + 0x59u);
    u16 s_74_0 = DSW(s0 + 0x74u);
    u16 s_74_1 = DSW(s1 + 0x74u);

    /* A: 0x39A10 writes the +0x74 timer of the slot named by the record's
     * +0x51 (word[0x107824 + side*0x94]). The sentinel 0xFFFF differs from
     * both written values; side 1 proves the +0x51 read, not a fixed slot. */
    (void)tf_hit_fixture(0);
    fight_reset_slot_pair(s0, s1, r0, r1);
    DSB(r0 + 0x51u) = 0;
    DSW(s0 + 0x74u) = 0xFFFFu;
    DSW(s1 + 0x74u) = 0xFFFFu;
    fighter_39a10(r0, 0x1234u);
    CHECK_EQ_INT((int)DSW(s0 + 0x74u), 0x1234);
    DSB(r0 + 0x51u) = 1;
    fighter_39a10(r0, 0x0BADu);
    CHECK_EQ_INT((int)DSW(s1 + 0x74u), 0x0BAD);

    /* B: the 0x349C8 +0x42 bit-7 arm runs 0x37D18 on the other slot when its
     * +0x40 holds 0x8200000. 0x37D18 writes that slot's +0x74 = 0x309 through
     * 0x39A10, then the arm sets this slot's +0x53 = 3. */
    (void)tf_hit_fixture(0);
    fight_reset_slot_pair(s0, s1, r0, r1);
    DSB(r0 + 0x51u) = 0;
    DSB(r1 + 0x51u) = 1;
    DSB(s0 + 0x42u) = 0x80u;                    /* the bit-7 arm */
    DSB(s0 + 0x53u) = 0;                        /* sentinel != 3 */
    DSD(s1 + 0x40u) = 0x8200000u;               /* the other-slot gate */
    DSW(s1 + 0x74u) = 0xFFFFu;                  /* sentinel != 0x309 */
    DSB(s1 + 0x7Au) = 0;
    fighter_state_default(0u);
    CHECK_EQ_INT((int)DSW(s1 + 0x74u), 0x309);
    CHECK_EQ_INT((int)DSB(s0 + 0x53u), 3);

    /* C: 0x385B0 (the 0x36870 mode-0x25 reset). The seed differs from every
     * asserted post-condition: 0x100AF8 = 1, rec+0x28 = 0xFF, S+0x40 =
     * 0xFFFFFFFF, rec+0x42 = 0xFF, S+0x52/+0x53 = 0xFF, S+0x54 = 2. */
    (void)tf_hit_fixture(0);
    fight_reset_slot_pair(s0, s1, r0, r1);
    DSB(r0 + 0x51u) = 0;
    DSB(r0 + 0x7Au) = 0;
    DSD(DS_00100AF8) = 1;
    DSB(r0 + 0x28u) = 0xFFu;
    DSD(s0 + 0x40u) = 0xFFFFFFFFu;
    DSB(r0 + 0x42u) = 0xFFu;
    DSB(s0 + 0x52u) = 0xFFu;
    DSB(s0 + 0x53u) = 0xFFu;
    DSB(s0 + 0x8Au) = 0xAAu;                    /* 0x38657 clears it */
    DSB(s0 + 0x54u) = 2;
    fighter_385b0(r0);
    CHECK_EQ_INT((int)DSD(DS_00100AF8), 0);
    CHECK_EQ_INT((int)DSB(r0 + 0x28u), 0xCB);   /* 0xDF then actors_anim_begin &= 0xEB */
    CHECK_EQ_INT(DSD(s0 + 0x40u), 0xCCF33FFFu); /* 0xCCF3BFFF then +0x41 &= 0x7F */
    CHECK_EQ_INT((int)DSB(r0 + 0x42u), 0);
    CHECK_EQ_INT((int)DSB(s0 + 0x8Au), 0);      /* raw 0x38657 */
    CHECK_EQ_INT((int)DSB(s0 + 0x54u), 0);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 0);
    CHECK_EQ_INT((int)DSB(s0 + 0x53u), 0);

    /* D: the 0x349C8 +0x42 bit-6 arm runs 0x37178 -> 0x36870 -> 0x385B0 under
     * mode 0x25. The two records' facing bits differ so 0x37178 takes the
     * different-facing in-range arm (the one that calls 0x36870); the seeded
     * 0x100AF8 = 1 is cleared only by 0x385B0, so the assertion proves the
     * whole chain ran. */
    (void)tf_hit_fixture(0);
    fight_reset_slot_pair(s0, s1, r0, r1);
    DSD(DS_001078DC) = 0x003F40000u;            /* the approach-table pointer */
    DSW(0x003F40000u) = 0;                       /* base = 0, not -1 */
    DSW(DS_00104B00) = 0x25u;                    /* the 0x385B0 mode */
    DSB(r0 + 0x51u) = 0;
    DSB(r0 + 0x7Au) = 0;
    DSB(s0 + 0x7Au) = 0;
    DSB(s1 + 0x7Au) = 0;
    DSB(s0 + 0x54u) = 0xFFu;
    DSB(s0 + 0x42u) = 0x40u;                     /* the bit-6 arm (after +0x40) */
    DSW(r0 + 0x28u) = 0x4000u;                   /* facing_s != facing_o */
    DSW(r1 + 0x28u) = 0;
    DSD(r0 + 0x18u) = 0x100u;
    DSD(r1 + 0x18u) = 0x100u;
    DSD(DS_00100AF8) = 1;                        /* cleared by 0x385B0 */
    fighter_state_default(0u);
    CHECK_EQ_INT((int)DSB(s1 + 0x42u) & 4, 4);   /* 0x37178 ran */
    CHECK_EQ_INT((int)DSD(DS_00100AF8), 0);
    /* 0x37178 set S+0x52 = 9 before calling 0x36870; only 0x385B0 (the mode
     * 0x25 arm) clears it, so this distinguishes the mode arm from the body. */
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 0);

    /* E: 0x36870's +0x54 == 0 arm restarts the calling side's own record
     * (0x36A8C/0x36AA1 read [esp+0x10] = rec_s) at 0xC8950[S+0x7A] and sets
     * its +0x4D = 0x1E; the other record is untouched. The demo's f = 93
     * raptor (side 1, char 3) takes this arm from its own stream's 0x36870
     * opcode. Mode 3 skips the 0x102900 restart; S+0x43 = 0 and So+0x43 = 0
     * keep 0x365C8/0x36638 at 0 and 0x36BC8 out, so the arm is reached.
     * The char-3 and char-0 table entries point at distinct scratch streams,
     * so reading the other slot's +0x7A fails as well. */
    {
        u32 st3 = FIGHT_RECS + 0x3800u, st0 = FIGHT_RECS + 0x3810u;
        u32 sv_c8950 = DSD(0x000C8950u), sv_c895c = DSD(0x000C895Cu);
        u8 sv_ce0[4], sv_af8[8], sv_d148[8], sv_d20[0x10], sv_a80[0x80];
        tf_snap(sv_ce0, DS_00100CE0, 4u);
        tf_snap(sv_af8, DS_00100AF8, 8u);
        tf_snap(sv_d148, DS_000FD148, 8u);
        tf_snap(sv_d20, DS_00107D20, 0x10u);
        tf_snap(sv_a80, 0x00107A80u, 0x80u);
        (void)tf_hit_fixture(0);
        fight_reset_slot_pair(s0, s1, r0, r1);
        DSW(st3) = 0x0123u;                      /* literal sprite ids */
        DSW(st0) = 0x0456u;
        DSD(0x000C895Cu) = st3;                  /* 0xC8950[3] */
        DSD(0x000C8950u) = st0;                  /* 0xC8950[0] */
        DSW(DS_00104B00) = 3u;
        DSW(DS_00107D2C) = 0;                    /* 0x39040(other) gate shut */
        DSB(r0 + 0x51u) = 0;
        DSB(r1 + 0x51u) = 1;
        DSW(r0 + 0x56u) = 1;
        DSW(r1 + 0x56u) = 2;
        DSW(FIGHT_ACTORS + 0x20u) = 0x7777u;
        DSW(FIGHT_ACTORS + 0x40u) = 0x7777u;
        DSB(s0 + 0x7Au) = 0;
        DSB(s1 + 0x7Au) = 3;
        DSB(s1 + 0x54u) = 0;
        DSB(s1 + 0x42u) = 0;
        DSB(s1 + 0x43u) = 0;
        DSB(s0 + 0x43u) = 0;
        DSB(s1 + 0x52u) = 0x66u;
        DSB(s1 + 0x53u) = 0x66u;
        DSD(r0 + 8u) = 0x00ABCDEFu;
        DSD(r1 + 8u) = 0x00ABCDEFu;
        DSB(r0 + 0x4Du) = 0x55u;
        DSB(r1 + 0x4Du) = 0x55u;
        fighter_36870(r1);
        CHECK_EQ_INT(DSD(r1 + 8u), st3);
        CHECK_EQ_INT((int)DSW(FIGHT_ACTORS + 0x40u), 0x0123);
        CHECK_EQ_INT((int)DSB(r1 + 0x4Du), 0x1E);
        CHECK_EQ_INT(DSD(r0 + 8u), 0x00ABCDEFu);
        CHECK_EQ_INT((int)DSW(FIGHT_ACTORS + 0x20u), 0x7777);
        CHECK_EQ_INT((int)DSB(r0 + 0x4Du), 0x55);
        CHECK_EQ_INT((int)DSB(s1 + 0x52u), 0);
        CHECK_EQ_INT((int)DSB(s1 + 0x53u), 0);
        DSD(0x000C8950u) = sv_c8950;
        DSD(0x000C895Cu) = sv_c895c;
        tf_put(sv_ce0, DS_00100CE0, 4u);
        tf_put(sv_af8, DS_00100AF8, 8u);
        tf_put(sv_d148, DS_000FD148, 8u);
        tf_put(sv_d20, DS_00107D20, 0x10u);
        tf_put(sv_a80, 0x00107A80u, 0x80u);
    }

    /* F: 0x3BDDC's attack transition starts the slot's record on
     * 0xC8B30[slot+0x7A] at hold 1.0 through 0x3C480 (0x3BEF7..0x3BF05), and
     * 0x3C480's 0x188DC tail re-derives rec+0x18 through 0x18714, which runs
     * 0x18540 and (anchor != slot+0x20) 0x18350 for the NEW sprite without
     * storing slot+0x20. The demo's f = 94 raptor (side 1, char 3) moves from
     * the stance 0x16B5 (anchor 0x16B5 - word[0xD2134] = 0; 0xD033B+0: fc 30,
     * x = -4) to 0x1746 (anchor 145; 0xD033B+290: f5 32, x = -11): the 0x188AC
     * latch gives slot+0x2C = 6144 - 256 = 5888, then rec+0x18 = 5888 + 704 =
     * 6592. The char-0 entry points at a distinct stream, so indexing by the
     * other slot's +0x7A fails; S+0x53 = 1 skips the 0x3CF38 chain (0x3BE55). */
    {
        u32 st3 = FIGHT_RECS + 0x3800u, st0 = FIGHT_RECS + 0x3810u;
        u32 st3f = FIGHT_RECS + 0x3820u;
        u32 sv_b30 = DSD(0x000C8B30u), sv_b3c = DSD(0x000C8B3Cu);
        u8 sv_af0[8], sv_ab0[0x10], sv_d40[8], sv_8f8[2];
        tf_snap(sv_af0, DS_00100AF0, 8u);
        tf_snap(sv_ab0, DS_00100AB0, 0x10u);
        tf_snap(sv_d40, DS_00107D40, 8u);
        tf_snap(sv_8f8, DS_001078F8, 2u);
        (void)tf_hit_fixture(0);
        fight_reset_slot_pair(s0, s1, r0, r1);
        DSW(st3) = 0x1746u;                      /* literal sprite ids */
        DSW(st0) = 0x0456u;
        DSD(0x000C8B3Cu) = st3;                  /* 0xC8B30[3] */
        DSD(0x000C8B30u) = st0;                  /* 0xC8B30[0] */
        DSB(r0 + 0x51u) = 0;
        DSB(r1 + 0x51u) = 1;
        DSW(r0 + 0x56u) = 1;
        DSW(r1 + 0x56u) = 2;
        DSW(FIGHT_ACTORS + 0x20u) = 0x7777u;
        DSW(FIGHT_ACTORS + 0x40u) = 0x16B5u;     /* the stance, anchor 0 */
        DSB(s0 + 0x7Au) = 0;
        DSB(s1 + 0x7Au) = 3;
        DSB(s1 + 0x53u) = 1;
        DSW(DS_001088E2) = 0x8000u;
        DSD(r1 + 8u) = 0x00ABCDEFu;
        DSD(r0 + 8u) = 0x00ABCDEFu;
        DSD(r1 + 0x24u) = 0x11111111u;
        DSD(r1 + 0x18u) = 6144u;
        DSD(r1 + 0x1Cu) = 0x5555u;
        DSD(s1 + 0x20u) = 0x77u;                 /* != either anchor */
        DSD(DS_00100AF0) = 0x99u;                /* side 0's words untouched */
        DSD(DS_00100AB0) = 0x88u;
        CHECK_EQ_INT(fighter_attack_consume(1u), 1);
        CHECK_EQ_INT(DSD(r1 + 8u), st3);
        CHECK_EQ_INT((int)DSW(FIGHT_ACTORS + 0x40u), 0x1746);
        CHECK_EQ_INT(DSD(r1 + 0x24u), 0x3F800000u);
        CHECK_EQ_INT((int)DSD(r1 + 0x1Cu), 0);   /* 0x3C497 xor ebx,ebx */
        CHECK_EQ_INT((int)DSD(s1 + 0x2Cu), 5888);
        CHECK_EQ_INT((int)DSD(r1 + 0x18u), 6592);
        CHECK_EQ_INT((int)DSD(DS_00100AF0 + 4u), 145);
        CHECK_EQ_INT((int)DSD(DS_00100AB0 + 8u), -704);
        CHECK_EQ_INT((int)DSD(DS_00100AB0 + 12u), 3200);
        CHECK_EQ_INT((int)DSD(s1 + 0x20u), 0);   /* the latch's, not 0x18714's */
        CHECK_EQ_INT(DSD(r0 + 8u), 0x00ABCDEFu);
        CHECK_EQ_INT((int)DSW(FIGHT_ACTORS + 0x20u), 0x7777);
        CHECK_EQ_INT((int)DSD(DS_00100AF0), 0x99);
        CHECK_EQ_INT((int)DSD(DS_00100AB0), 0x88);
        CHECK_EQ_INT((int)DSB(s1 + 0x52u), 3);

        /* G: from the hflipped stance 0x96B5 (anchor 0, so the latch's 0x18350
         * negates x: DS_00100AB0 = +256) to the unflipped literal 0x16B5 (anchor
         * 0 again). The anchor equals the latched slot+0x20, so 0x18757 skips
         * 0x18350: DS_00100AB0 keeps +256 and rec+0x18 round-trips to 6144.
         * Calling 0x18350 anyway would un-negate it (-256) and give 6656. */
        (void)tf_hit_fixture(0);
        fight_reset_slot_pair(s0, s1, r0, r1);
        DSW(st3f) = 0x16B5u;
        DSD(0x000C8B3Cu) = st3f;
        DSB(r1 + 0x51u) = 1;
        DSW(r1 + 0x56u) = 2;
        DSW(FIGHT_ACTORS + 0x40u) = 0x96B5u;
        DSB(s1 + 0x7Au) = 3;
        DSB(s1 + 0x53u) = 1;
        DSW(DS_001088E2) = 0x8000u;
        DSD(r1 + 0x18u) = 6144u;
        DSD(s1 + 0x20u) = 0x77u;
        CHECK_EQ_INT(fighter_attack_consume(1u), 1);
        CHECK_EQ_INT((int)DSW(FIGHT_ACTORS + 0x40u), 0x16B5);
        CHECK_EQ_INT((int)DSD(s1 + 0x2Cu), 6400);
        CHECK_EQ_INT((int)DSD(DS_00100AB0 + 8u), 256);
        CHECK_EQ_INT((int)DSD(r1 + 0x18u), 6144);

        DSD(0x000C8B30u) = sv_b30;
        DSD(0x000C8B3Cu) = sv_b3c;
        tf_put(sv_af0, DS_00100AF0, 8u);
        tf_put(sv_ab0, DS_00100AB0, 0x10u);
        tf_put(sv_d40, DS_00107D40, 8u);
        tf_put(sv_8f8, DS_001078F8, 2u);
    }

    /* H: 0x35E04, the 0xD000 target at 0xD2278 in the raptor's 0xD2274
     * attack stream. With rec+0x14 set it stores 3.0f into rec+0x20 and
     * rec+0x24, then 0x3BC70(rec+0x51) sets the slot's +0x54/+0x52/+0x53 =
     * 2/4/0 and loads rec+0x44/+0x36/+0x34 from the DS_00107D40[side] row.
     * The row is the char-3 0xBEF28 row (read_memory 0xBEF3A: 23, 550, 150),
     * copied into scratch. The slot's word +0x4E = -1 (the demo's f = 96
     * raptor) negates the horizontal speed (0x3BCCA), +1 keeps it (0x3BCB9)
     * and 0 zeroes it (0x3BCD5). The side comes from rec+0x51, so slot 0
     * keeps its sentinels. Without rec+0x14, nothing is written (0x35E0B). */
    {
        u32 row = FIGHT_RECS + 0x3830u;
        u8 sv_d40[8];
        u16 sv_4e_1 = DSW(s1 + 0x4Eu);
        tf_snap(sv_d40, DS_00107D40, 8u);
        (void)tf_hit_fixture(0);
        fight_reset_slot_pair(s0, s1, r0, r1);
        DSW(row) = 23u;
        DSW(row + 2u) = 550u;
        DSW(row + 4u) = 150u;
        DSD(DS_00107D40) = 0x00ABCDEFu;          /* side 0's row: unread */
        DSD(DS_00107D40 + 4u) = row;
        DSB(r1 + 0x51u) = 1;
        DSD(r1 + 0x14u) = s1;
        DSD(r1 + 0x20u) = 0x11111111u;
        DSD(r1 + 0x24u) = 0x22222222u;
        DSW(r1 + 0x44u) = 0x5555u;
        DSW(r1 + 0x36u) = 0x5555u;
        DSW(r1 + 0x34u) = 0x5555u;
        DSB(s1 + 0x52u) = 0x66u;
        DSB(s1 + 0x53u) = 0x66u;
        DSB(s1 + 0x54u) = 0x66u;
        DSB(s0 + 0x52u) = 0x66u;
        DSW(r0 + 0x44u) = 0x5555u;
        DSW(s1 + 0x4Eu) = 0xFFFFu;
        fighter_35e04(r1);
        CHECK_EQ_INT(DSD(r1 + 0x20u), 0x40400000u);
        CHECK_EQ_INT(DSD(r1 + 0x24u), 0x40400000u);
        CHECK_EQ_INT((int)DSB(s1 + 0x54u), 2);
        CHECK_EQ_INT((int)DSB(s1 + 0x52u), 4);
        CHECK_EQ_INT((int)DSB(s1 + 0x53u), 0);
        CHECK_EQ_INT((int)DSW(r1 + 0x44u), 23);
        CHECK_EQ_INT((int)DSW(r1 + 0x36u), 550);
        CHECK_EQ_INT((int)(s16)DSW(r1 + 0x34u), -150);
        CHECK_EQ_INT((int)DSB(s0 + 0x52u), 0x66);
        CHECK_EQ_INT((int)DSW(r0 + 0x44u), 0x5555);

        DSW(r1 + 0x34u) = 0x5555u;
        DSW(s1 + 0x4Eu) = 1u;
        fighter_35e04(r1);
        CHECK_EQ_INT((int)(s16)DSW(r1 + 0x34u), 150);
        DSW(r1 + 0x34u) = 0x5555u;
        DSW(s1 + 0x4Eu) = 0u;
        fighter_35e04(r1);
        CHECK_EQ_INT((int)DSW(r1 + 0x34u), 0);

        DSD(r1 + 0x14u) = 0;
        DSD(r1 + 0x20u) = 0x11111111u;
        DSB(s1 + 0x52u) = 0x66u;
        DSW(r1 + 0x44u) = 0x5555u;
        fighter_35e04(r1);
        CHECK_EQ_INT(DSD(r1 + 0x20u), 0x11111111u);
        CHECK_EQ_INT((int)DSB(s1 + 0x52u), 0x66);
        CHECK_EQ_INT((int)DSW(r1 + 0x44u), 0x5555);

        DSW(s1 + 0x4Eu) = sv_4e_1;
        tf_put(sv_d40, DS_00107D40, 8u);
    }

    /* I: 0x349C8's command gate (demo-pose record §27). A command with a bit
     * in both (cmd>>8)&3 and (cmd>>8)&0xC returns at 0x34A8D (`jne 0x34B0B`),
     * so 0x6F6F (the demo T-rex's f = 619 word) runs neither the 0x4000 arm
     * nor 0x35838: the sentinels survive. 0x4040 ((cmd>>8)&3 == 0) passes the
     * gate; bit 15 clear makes 0x3BDDC return 0, so the 0x4000 arm starts
     * 0xC8978[slot+0x7A] and sets +0x52/+0x54 = 5/1. So+0x43 = 0 keeps 0x365C8
     * at 0 and S+0x43 = 0 keeps 0x36638 at 0. The char-0 entry points at a
     * distinct stream, so indexing by the other slot's +0x7A fails. */
    {
        u32 st3 = FIGHT_RECS + 0x3840u, st0 = FIGHT_RECS + 0x3850u;
        u32 sv_978 = DSD(0x000C8978u), sv_984 = DSD(0x000C8984u);
        u16 sv_cmd = DSW(DS_001088E2);
        (void)tf_hit_fixture(0);
        fight_reset_slot_pair(s0, s1, r0, r1);
        DSW(st3) = 0x0123u;                      /* literal sprite ids */
        DSW(st0) = 0x0456u;
        DSD(0x000C8984u) = st3;                  /* 0xC8978[3] */
        DSD(0x000C8978u) = st0;                  /* 0xC8978[0] */
        DSB(r1 + 0x51u) = 1;
        DSW(r1 + 0x56u) = 2;
        DSB(s0 + 0x7Au) = 0;
        DSB(s1 + 0x7Au) = 3;
        DSB(s0 + 0x43u) = 0;
        DSB(s1 + 0x43u) = 0;
        DSB(s1 + 0x42u) = 0;
        DSB(s1 + 0x53u) = 0;
        DSD(s1 + 0x40u) = 0;
        DSB(s1 + 0x52u) = 0x66u;
        DSB(s1 + 0x54u) = 0x66u;
        DSD(r1 + 8u) = 0x00ABCDEFu;
        DSD(r1 + 0x20u) = 0x11111111u;
        DSW(FIGHT_ACTORS + 0x40u) = 0x7777u;
        DSW(DS_001088E2) = 0x6F6Fu;
        fighter_state_default(1u);
        CHECK_EQ_INT((int)DSB(s1 + 0x52u), 0x66);
        CHECK_EQ_INT((int)DSB(s1 + 0x54u), 0x66);
        CHECK_EQ_INT(DSD(r1 + 8u), 0x00ABCDEFu);
        CHECK_EQ_INT(DSD(r1 + 0x20u), 0x11111111u);
        CHECK_EQ_INT((int)DSW(FIGHT_ACTORS + 0x40u), 0x7777);

        DSW(DS_001088E2) = 0x4040u;
        fighter_state_default(1u);
        CHECK_EQ_INT((int)DSB(s1 + 0x52u), 5);
        CHECK_EQ_INT((int)DSB(s1 + 0x54u), 1);
        CHECK_EQ_INT(DSD(r1 + 8u), st3);
        CHECK_EQ_INT(DSD(r1 + 0x20u), 0x40000000u);
        CHECK_EQ_INT((int)DSW(FIGHT_ACTORS + 0x40u), 0x0123);

        DSD(0x000C8978u) = sv_978;
        DSD(0x000C8984u) = sv_984;
        DSW(DS_001088E2) = sv_cmd;
    }

    DSD(DS_001078DC) = s_dc;
    DSD(s0 + 0x40u) = s_40_0;
    DSD(s1 + 0x40u) = s_40_1;
    DSD(DS_001077A8) = s_a8_0;
    DSD(DS_001077A8 + 4u) = s_a8_1;
    DSB(s0 + 0x42u) = s_42_0;
    DSB(s1 + 0x42u) = s_42_1;
    DSB(r0 + 0x51u) = s_51_0;
    DSB(r1 + 0x51u) = s_51_1;
    DSB(s0 + 0x53u) = s_53_0;
    DSB(s0 + 0x7Au) = s_7a_0;
    DSB(s1 + 0x7Au) = s_7a_1;
    DSB(r0 + 0x59u) = s_59_0;
    DSW(s0 + 0x74u) = s_74_0;
    DSW(s1 + 0x74u) = s_74_1;
    DSW(DS_00104B00) = (u16)s_b00;
    DSD(DS_00100AF8) = s_af8;
    DSD(0x003F40000u) = s_tbl;
}

/* ---- Task 3a: the 0x3AAFC reaction/pose sub-tree (pose/freeze record §2) --
 * The pose setters and the 0x39834/0x392A0 drivers are exercised through the
 * 0x3AAFC entry; the two pure helpers are unit-tested directly. */

/* §8.2 0x3A280: the reaction predicate boundaries. */
static void check_pose_predicate(void)
{
    CHECK_EQ_INT(fighter_3a280(0x0Fu), 0);      /* below the 0x10 floor */
    CHECK_EQ_INT(fighter_3a280(0x10u), 1);      /* the 0x10..0x17 jump table */
    CHECK_EQ_INT(fighter_3a280(0x17u), 1);
    CHECK_EQ_INT(fighter_3a280(0x18u), 0);      /* the 0x18..0x1F hole */
    CHECK_EQ_INT(fighter_3a280(0x1Fu), 0);
    CHECK_EQ_INT(fighter_3a280(0x20u), 1);      /* the 0x20..0x3F range */
    CHECK_EQ_INT(fighter_3a280(0x3Fu), 1);
    CHECK_EQ_INT(fighter_3a280(0x40u), 0);
    CHECK_EQ_INT(fighter_3a280(0xFFu), 0);
}

/* §2.6 0x46534: the accumulator clamps. */
static void check_pose_accumulator(void)
{
    u32 addr = DS_001082C8 + 4u;                /* side 1 */
    s32 saved = (s32)DSD(addr);
    u32 saved_lo = DSD(DS_001082D0);
    u8 saved_idx = DSB(DS_0010452C);
    u8 saved_cap = DSB(0x000C9408u);

    DSB(DS_0010452C) = 0;                       /* index 0 */
    DSB(0x000C9408u) = 10;                      /* cap 10 */
    DSD(DS_001082D0) = 0;

    DSD(addr) = 5;
    fighter_46534(1u, 3);
    CHECK_EQ_INT((int)DSD(addr), 8);            /* 5 + 3, under the cap */

    DSD(addr) = 5;
    fighter_46534(1u, 0x7F);
    CHECK_EQ_INT((int)DSD(addr), 10);           /* clamped down to the cap */

    DSD(addr) = 5;
    fighter_46534(1u, -100);
    CHECK_EQ_INT((int)DSD(addr), 0);            /* clamped up to 0 */

    DSD(DS_001082D0) = 2;
    DSD(addr) = 5;
    fighter_46534(1u, -100);
    CHECK_EQ_INT((int)DSD(addr), 2);            /* the DS_001082D0 floor */

    DSD(addr) = (u32)saved;
    DSD(DS_001082D0) = saved_lo;
    DSB(DS_0010452C) = saved_idx;
    DSB(0x000C9408u) = saved_cap;
}

/* The pose-chain seed block check_pose_entry/check_reaction/winner_body_setup
 * share: the two-slot reset plus the globals the 0x3AAFC/0x3B714 chain reads,
 * held in their "off" state. Each caller adds the values its path needs. */
static void pose_chain_setup(u32 s0, u32 s1, u32 r0, u32 r1)
{
    fight_reset_recs();
    fight_reset_actors();
    mem_fill(s0, 0, 0x94u);
    mem_fill(s1, 0, 0x94u);
    fight_reset_slot_pair(s0, s1, r0, r1);

    DSB(r0 + 0x51u) = 0;
    DSB(r1 + 0x51u) = 1;
    DSB(DS_0010782A) = 0;                       /* char(slot 0) */
    DSB(DS_001078BE) = 0;                       /* char(slot 1) */
    DSB(s0 + 0x54u) = 0;
    DSB(s0 + 0x42u) = 0;
    DSB(s1 + 0x42u) = 0;
    DSW(s1 + 0x6Cu) = 0;
    DSW(DS_00104B00) = 0;                       /* mode 0 (not 0x22/3) */
    DSB(DS_00104B1D) = 0;                       /* mode2 0 -> the MELSE arm */
    DSB(DS_00105B38) = 0;
    DSB(DS_00105B36) = 1;                       /* skip the +0x5D clamp */
    DSB(DS_00105B3A) = 0;
    DSD(DS_00104ABC) = 0;                       /* skip 0x4F434 */
    DSD(0x00107D2Au) = 0;                       /* k = 0, +0x14 gate clear */
    DSD(0x00107D2Cu) = 0;
    DSW(0x000A6728u) = 0;                       /* key = 0, no effect spawn */
    DSD(0x000A3528u + 8u) = 0;                  /* stream = 0 */
    DSB(0x000DE11Au) = 0;                       /* edx3 = 0 (the s8 at anim3[0]+6) */
    DSB(s0 + 0x5Au) = 0;
    DSB(s0 + 0x5Du) = 0;
}

/* §8.2 0x3AAFC: the pose dispatch through the 0x3A504/0x3A79C arms and the
 * side-1 mirror. The 0x3A280 predicate and the 0x3A0FC spawn are gated off by
 * seeding both +0x5F = 0xFF and the 0xA6728 descriptor to zero. */
static void check_pose_entry(void)
{
    u32 s0 = DS_001077B0;
    u32 s1 = DS_001077B0 + 0x94u;
    u32 r0 = FIGHT_RECS;
    u32 r1 = FIGHT_RECS + 0x100u;
    u16 sv_w0 = DSW(0x000A6728u);
    u16 sv_w2 = DSW(0x000A6728u + 2u);
    u32 sv_d8 = DSD(0x000A3528u + 8u);
    u8  sv_row[11];
    u8  sv_7e0 = DSB(DS_000BECF8);
    u32 i;
    for (i = 0; i < 11u; i++) sv_row[i] = DSB(0x000DE114u + i);

    pose_chain_setup(s0, s1, r0, r1);

    DSB(s0 + 0x5Fu) = 0xFFu;                    /* uVar2 = 0 */
    DSB(s1 + 0x5Fu) = 0xFFu;                    /* 0x3A280 -> 0 */
    DSB(s0 + 0x53u) = 0;
    DSB(s0 + 0x41u) = 0;
    DSW(0x000A6728u + 2u) = 0;                  /* ecx = 0 */
    /* §7.4: the setter's glob_b latch is the caller's BX = slot+0x52 (raw
     * 0x3A56B stores BX; EBX is loaded at 0x3AD2A/0x3AD31/0x3AD3D before the
     * call), not edx3 >> 16. Seed +0x52 and edx3's high word differently so
     * the two forms cannot both pass. The 0x07 seed makes the 0x468D8
     * predicate fire (0x4691E compares +0x52 with 7), so the 0x39834 chain's
     * 0x36D98 writes +0x52 = 9 (0x36E14) before the setter reads it: BX is 9,
     * the at-call value. */
    DSB(s0 + 0x52u) = 0x07;
    DSD(s0 + 0x2Cu) = 0x2222;
    /* The setter's EDX is the anim3 row's byte +6, sign-extended: 0x3AD27
     * `mov esi,[esi+3]` (8b 76 03) loads the dword at row+3 and 0x3AD2E
     * `sar esi,0x18` (c1 fe 18) keeps its top byte. The setter stores
     * +0x7E = byte[0xBECF8] + CL (0x3A549/0x3A54E/0x3A554). Every byte of the
     * row (0xDE114 for char 0, reaction 0) is distinct, so only +6 gives
     * 0x14 + 0xB0 = 0xC4; the old +3 read gives 0x14 + 0x73 = 0x87. */
    for (i = 0; i < 11u; i++) DSB(0x000DE114u + i) = (u8)(0x70u + i);
    DSB(0x000DE11Au) = 0xB0u;                   /* edx3 = 0xFFFFFFB0 */
    DSB(DS_000BECF8) = 0x14u;
    DSB(s0 + 0x7Eu) = 0x5Au;                    /* sentinel */

    fighter_reaction_apply(s0, 0u);
    CHECK_EQ_INT((int)DSB(s0 + 0x7Eu), 0xC4);   /* 0x14 + byte[row+6] */
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 0x10);   /* the 0x3A504 arm */
    CHECK_EQ_INT((int)DSB(s0 + 0x53u), 0x0A);
    CHECK_EQ_INT((int)DSB(s0 + 0x54u), 0);
    CHECK_EQ_INT((int)DSD(s0 + 0x10u), 0x0003A43C);
    CHECK_EQ_INT((int)DSW(DS_00107D14), 0x2222);    /* A = slot+0x2C */
    CHECK_EQ_INT((int)DSW(DS_00107D10), 0x0009);    /* BX = slot+0x52 at the call */
    CHECK_EQ_INT((int)DSW(s1 + 0x6Cu), 1);      /* other +0x6C++ */
    CHECK_EQ_INT((int)(DSB(s1 + 0x42u) & 2u), 2);   /* other +0x42 bit 1 */
    CHECK_EQ_INT((int)(DSB(s0 + 0x42u) & 1u), 1);   /* self +0x42 bit 0 */

    /* ecx bit 3 selects the 0x3A79C variant. The first call's setter wrote
     * +0x52 = 0x10, so this call's BX is 0x10, not the seeded 0x07. */
    DSW(0x000A6728u + 2u) = 8;
    fighter_reaction_apply(s0, 0u);
    CHECK_EQ_INT((int)DSD(s0 + 0x10u), 0x0003A6D4);
    CHECK_EQ_INT((int)DSW(DS_00107D08), (int)DSW(s0 + 0x2Cu));
    CHECK_EQ_INT((int)DSW(DS_00107D04), 0x10);

    /* The side-1 mirror. */
    DSW(0x000A6728u + 2u) = 0;
    fighter_reaction_apply(s1, 0u);
    CHECK_EQ_INT((int)DSB(s1 + 0x52u), 0x10);
    CHECK_EQ_INT((int)DSB(s1 + 0x53u), 0x0A);
    CHECK_EQ_INT((int)DSB(s1 + 0x54u), 0);
    CHECK_EQ_INT((int)DSD(s1 + 0x10u), 0x0003A43C);
    CHECK_EQ_INT((int)DSW(DS_00107D14 + 2u), (int)DSW(s1 + 0x2Cu));

    /* The effect spawns 0x3A0FC/0x3AD98 (0x3A1A5/0x3A241/0x3AE3C): a non-zero
     * stream makes each spawn land at the pool base. The raw passes a2 =
     * *(slot+0x2C) (the value, not its address), a3 = the (rec+0x30)>>16 layer,
     * a4 = the 0xF0AEC-derived offset, and 0x3A0FC reads the stream from
     * anim[1]+8 (0x3A165), not anim[2]+8. A three-record scratch pool keeps the
     * spawns off the real pool. */
    {
        u32 sv_pool = DSD(DS_001014F4);
        u32 sv_free = DSD(DS_00105B3C);
        u32 sv_free4 = DSD(DS_00105B3C + 4u);
        u32 sv_act = DSD(DS_00105BCC);
        u32 sv_act4 = DSD(DS_00105BCC + 4u);
        u32 sv_d8b = DSD(0x000A3528u + 8u);
        u32 pool = 0x003F21000u;
        u32 r2 = pool + 0x68u;
        u32 r3 = pool + 0xD0u;
        u32 abuf = 0x003F22000u;
        u32 anim[3];
        u32 off;

        DSD(DS_001014F4) = pool;
        DSD(pool) = r2;                         /* the three-record free list */
        DSD(pool + 4u) = DS_00105B3C;
        DSD(r2) = r3;
        DSD(r2 + 4u) = pool;
        DSD(r3) = DS_00105B3C;
        DSD(r3 + 4u) = r2;
        DSD(DS_00105B3C) = pool;
        DSD(DS_00105B3C + 4u) = r3;
        DSD(DS_00105BCC) = DS_00105BCC;         /* the active-list sentinel */
        DSD(DS_00105BCC + 4u) = DS_00105BCC;

        DSB(s0 + 0x5Fu) = 0;
        DSB(s1 + 0x5Fu) = 0;
        DSD(s0 + 0x2Cu) = 0x12345678u;          /* the a2 value */
        DSD(r0 + 0x30u) = 0x00070000u;          /* the a3 layer = 7 */
        DSW(r1 + 0x28u) = 0;                    /* facing = 0x4000 */
        DSD(0x000A3528u + 8u) = 0x000E8CBEu;    /* anim[1]+8 -> the first spawn */
        DSW(0x000A6728u) = 1u;                  /* key = 1 -> the second spawn */
        DSB(DS_00105B3A) = 0;
        fighter_reaction_apply(s0, 0u);

        off = DSD(DS_000F0AEC) + 0x3BC0u
            - (DSD(DS_00100AD8 + 4u) << 6)
            - (u32)((s32)DSD(r0 + 0x30u) >> 16);
        CHECK_EQ_INT((int)DSD(pool + 0x18u), 0x12345678);   /* 0x3A1A5 a2 */
        CHECK_EQ_INT((int)DSD(pool + 0x1Cu), (int)off);     /* 0x3A1A5 a4 */
        CHECK_EQ_INT((int)DSW(pool + 0x32u), 7);            /* 0x3A1A5 a3 */
        CHECK_EQ_INT((int)DSD(pool + 8u), 0x000E8CBEu + 2u);  /* anim[1]+8, +2 walk */
        CHECK_EQ_INT((int)DSD(r2 + 0x18u), 0x12345678);     /* 0x3A241 a2 */
        CHECK_EQ_INT((int)DSD(r2 + 0x1Cu), (int)off);       /* 0x3A241 a4 */
        CHECK_EQ_INT((int)DSW(r2 + 0x32u), 7);              /* 0x3A241 a3 */

        /* 0x3AD98: the same argument order with its own anim selector. */
        mem_fill(abuf, 0, 0x10u);               /* its +4/+5 feed 0x392A0 */
        anim[0] = abuf;
        anim[1] = abuf;
        anim[2] = abuf + 8u;
        DSW(anim[2]) = 1u;                      /* stream = 0xE8E08 */
        fighter_3ad98(0u, anim);
        CHECK_EQ_INT((int)DSD(r3 + 0x18u), 0x12345678);     /* 0x3AE3C a2 */
        CHECK_EQ_INT((int)DSD(r3 + 0x1Cu), (int)off);       /* 0x3AE3C a4 */
        CHECK_EQ_INT((int)DSW(r3 + 0x32u), 7);              /* 0x3AE3C a3 */

        DSD(0x000A3528u + 8u) = sv_d8b;
        DSD(DS_001014F4) = sv_pool;
        DSD(DS_00105B3C) = sv_free;
        DSD(DS_00105B3C + 4u) = sv_free4;
        DSD(DS_00105BCC) = sv_act;
        DSD(DS_00105BCC + 4u) = sv_act4;
    }

    DSW(0x000A6728u) = sv_w0;
    DSW(0x000A6728u + 2u) = sv_w2;
    DSD(0x000A3528u + 8u) = sv_d8;
    for (i = 0; i < 11u; i++) DSB(0x000DE114u + i) = sv_row[i];
    DSB(DS_000BECF8) = sv_7e0;
}

/* ---- Task 2: the state-7 pose handler 0x3A43C (demo-pose record §7.1-§7.3) */

/* The §7.2 phase-1 seed: the slot enters with +0x58 = 1, char 0, the +0x90
 * gate open, and the self side's B/A words (the 0x3A504 setter's globs,
 * B = 0x107D10 + side*2, A = 0x107D14 + side*2) zeroed. 0x2BC30 returns with
 * RET 4 (0x2BCEF), popping the 0x3A471 push, so 0x3A48E..0x3A4A8 read ctx[5]
 * (rec_self) and ctx[1] (the side): record §9. Every seeded value differs from
 * its post-condition. */
static void pose_handler_seed(u32 s0, u32 s1, u32 r0, u32 r1)
{
    pose_chain_setup(s0, s1, r0, r1);

    DSB(s0 + 0x58u) = 1;                     /* phase 1 */
    DSB(s0 + 0x7Au) = 0;                     /* char 0: the 0xC8FE0 table */
    DSB(s0 + 0x90u) = 0;                     /* (u8)(0 - 1) > 3: the table arm */
    DSD(s0 + 0x2Cu) = 0x1234;
    DSW(r0 + 0x56u) = 0;                     /* the pset index */
    DSD(r0 + 8u) = 0xDEADBEEFu;              /* the stream sentinel */
    DSB(r0 + 0x52u) = 0x7F;                  /* the animation variable */
    DSD(r0 + 0x24u) = 0xDEADBEEFu;           /* the frame-hold sentinel */
    DSD(r0 + 0x18u) = 0x5678;                /* the hit_anchor_x write's sentinel */
    DSW(FIGHT_ACTORS) = 0xFFFFu;             /* the pset id sentinel */
    DSD(r0 + 0x1Cu) = 0xDEADBEEFu;           /* hit_anchor_set's y sentinel */
    DSD(r1 + 0x1Cu) = 0xDEADBEEFu;           /* the other record: untouched */
    DSB(s1 + 0x52u) = 0x07;
    DSW(0x00107D10u) = 0;                    /* B[0] = 0: the gate closed */
    DSW(0x00107D14u) = 0;                    /* A[0] */
}

/* §7.1-§7.3: the handler's phases, the animation start and the B[side]/+0x90
 * snap gate. The direct calls and the 0x3531C case-10 wiring are both covered;
 * the latter is the registration's end-to-end proof. */
static void check_pose_handler(void)
{
    u32 s0 = DS_001077B0;
    u32 s1 = DS_001077B0 + 0x94u;
    u32 r0 = FIGHT_RECS;
    u32 r1 = FIGHT_RECS + 0x100u;
    u16 sv_78f6 = DSW(DS_001078F6);
    u32 sv_7a8 = DSD(DS_001077A8);
    u32 sv_af0 = DSD(DS_00100AF0);
    u32 sv_ab0 = DSD(DS_00100AB0);

    /* §7.1: phase 0 arms +0x58. The raw's phase dispatch returns for any
     * +0x58 above 1 (0x3A453/0x3A455), so the seed is 0 — which differs from
     * the post-condition 1. */
    pose_chain_setup(s0, s1, r0, r1);
    DSB(s0 + 0x58u) = 0;
    fighter_pose_3a43c(s0, 0u);
    CHECK_EQ_INT((int)DSB(s0 + 0x58u), 1);

    /* 0x3A453/0x3A455: any +0x58 above 1 returns before touching anything. The
     * seeds are sentinels that differ from what phases 0/1 would write. */
    pose_handler_seed(s0, s1, r0, r1);
    DSB(s0 + 0x58u) = 3;
    DSD(r0 + 8u) = 0xCAFEF00Du;
    DSB(s0 + 0x90u) = 0x55;
    fighter_pose_3a43c(s0, 0u);
    CHECK_EQ_INT((int)DSB(s0 + 0x58u), 3);
    CHECK_EQ_INT((int)DSD(r0 + 8u), (int)0xCAFEF00Du);
    CHECK_EQ_INT((int)DSB(s0 + 0x90u), 0x55);

    /* §7.2: phase 1 starts the char-0 stream, re-anchors the self record
     * (0x3A49B: 0x188AC(side, rec_self+0x18, 0)) and closes the B[0] = 0
     * gate. */
    pose_handler_seed(s0, s1, r0, r1);
    fighter_pose_3a43c(s0, 0u);
    CHECK_EQ_INT((int)DSD(r0 + 8u), 0x000E7332);    /* 0xC8FE0[0] */
    CHECK_EQ_INT((int)DSD(r0 + 0x20u), 0x40400000); /* 3.0f */
    CHECK_EQ_INT((int)DSD(r0 + 0x24u), 0x40400000);
    CHECK_EQ_INT((int)DSB(r0 + 0x52u), 0);          /* actors_anim_begin reset */
    CHECK_EQ_INT((int)DSW(FIGHT_ACTORS), 0x1075);   /* the stream's first id */
    CHECK_EQ_INT((int)DSB(s0 + 0x58u), 2);
    CHECK_EQ_INT((int)DSB(s0 + 0x90u), 1);
    CHECK_EQ_INT((int)DSD(r0 + 0x1Cu), 0);          /* hit_anchor_set */
    CHECK_EQ_INT((int)DSD(r1 + 0x1Cu), (int)0xDEADBEEFu); /* not the other */
    CHECK_EQ_INT((int)DSD(r0 + 0x18u), 0x5678);     /* B[0] = 0: no snap */

    /* §7.3: B[0] = 3, A[0] = 0x4321 and +0x90 = 0 open the snap. Slot+0x42 bit
     * 3 is clear, so the raw's 0x18714 (0x1873D..0x18780) calls 0x18540, calls
     * 0x18350 only when DS_00100AF0[side] != slot+0x20, then returns slot+0x2C
     * - DS_00100AB0[side*8]. The seed makes both calls inert (the port's
     * hit_record_x omitted them until demo record §17, and the seed keeps this
     * block independent of them): DS_001077A8[0] =
     * 0 is 0x18540's early-out (0x1854F -> 0x18620, no write) and DS_00100AF0[0]
     * = slot+0x20 skips 0x18350. The rec+0x18 write is then 0x4321 - 0x1000,
     * distinct from its sentinel. (With bit 3 set, the record's own §7.3 seed,
     * the write is the sentinel again, so the assertion could not fail.) */
    pose_handler_seed(s0, s1, r0, r1);
    DSD(DS_001077A8) = 0;
    DSD(DS_00100AF0) = DSD(s0 + 0x20u);
    DSD(DS_00100AB0) = 0x1000;
    DSW(0x00107D10u) = 3;
    DSW(0x00107D14u) = 0x4321;
    fighter_pose_3a43c(s0, 0u);
    CHECK_EQ_INT((int)DSD(s0 + 0x2Cu), 0x4321);
    CHECK_EQ_INT((int)DSD(r0 + 0x18u), 0x4321 - 0x1000);
    DSD(DS_001077A8) = sv_7a8;               /* the seeds are scoped to this block */
    DSD(DS_00100AF0) = sv_af0;
    DSD(DS_00100AB0) = sv_ab0;

    /* §7.3: +0x90 in 1..4 is the table arm (no snap) even with B[0] = 3. */
    pose_handler_seed(s0, s1, r0, r1);
    DSB(s0 + 0x90u) = 4;
    DSW(0x00107D10u) = 3;
    DSW(0x00107D14u) = 0x4321;
    fighter_pose_3a43c(s0, 0u);
    CHECK_EQ_INT((int)DSD(r0 + 0x18u), 0x5678);

    /* §7.3: B[0] = 5 closes the snap. */
    pose_handler_seed(s0, s1, r0, r1);
    DSW(0x00107D10u) = 5;
    DSW(0x00107D14u) = 0x4321;
    fighter_pose_3a43c(s0, 0u);
    CHECK_EQ_INT((int)DSD(r0 + 0x18u), 0x5678);

    /* §9: the B/A words are the self side's, not the other's: B[1] = 3 with
     * A[1] = 0x4321 leaves the gate closed (B[0] = 0). */
    pose_handler_seed(s0, s1, r0, r1);
    DSW(0x00107D12u) = 3;
    DSW(0x00107D16u) = 0x4321;
    fighter_pose_3a43c(s0, 0u);
    CHECK_EQ_INT((int)DSD(r0 + 0x18u), 0x5678);

    /* The wiring: 0x3531C case 10 resolves slot+0x10 and calls it with the
     * raw's (EAX = slot, EBX = side). The registration itself is asserted in
     * check_anim_hold_scaler (which runs actors_init); register it here only
     * when this check runs without it, so the wiring does not depend on order. */
    CHECK(fn_resolve(0x3A43Cu) == (void (*)(void))fighter_pose_3a43c,
          "actors_init registered 0x3A43C as fighter_pose_3a43c");
    if (fn_resolve(0x3A43Cu) == NULL)
        fn_register(0x3A43Cu, (void (*)(void))fighter_pose_3a43c);
    pose_handler_seed(s0, s1, r0, r1);
    DSB(s0 + 0x53u) = 0x0A;
    DSD(s0 + 0x10u) = 0x0003A43Cu;
    DSW(DS_001078F6) = 0;                    /* the +0xEC voice tick is inert */
    fighter_state_3531c(0u);
    CHECK_EQ_INT((int)DSD(r0 + 8u), 0x000E7332);
    CHECK_EQ_INT((int)DSB(s0 + 0x58u), 2);

    DSW(DS_001078F6) = sv_78f6;
}

/* ---- Task 2: the roar stream's frame-hold scaler 0x39A34 (record §7.5/§7.6) */

/* §7.5/§7.6: the roar stream's first opcode (0xD100 = opcode 0x11, mode 0x4000)
 * targets 0x39A34, which rescales the frame hold from the linked record's +0x7E
 * over the operand. Driven through 0x2BC30's pre-walk, so the registration and
 * the code are tested together; the crafted stream is the roar stream's shape
 * (inline pointer at +2, operand at +6, literal id at +8). */
static void check_anim_hold_scaler(void)
{
    u32 rec = FIGHT_RECS;
    u32 linked = FIGHT_RECS + 0x100u;
    u32 stream = FIGHT_RECS + 0x180u;
    u16 *s = (u16 *)(mem + stream);

    mem_fill(FIGHT_RECS, 0, 0x200);
    mem_fill(FIGHT_ACTORS, 0, 0x80);
    DSD(DS_001014EC) = FIGHT_ACTORS;

    /* §7.6: the animation dispatcher resolves both new targets. */
    CHECK(actors_init() == 1, "actors_init validates the pools");
    CHECK(fn_resolve(0x39A34u) != NULL, "0x39A34 is registered");
    CHECK(fn_resolve(0x36870u) != NULL, "0x36870 is registered");
    /* The registered target is the two-argument opcode-target wrapper, not the
     * one-argument fighter_36870 itself (anim_indirect calls it as (rec, arg)). */
    CHECK(fn_resolve(0x36870u) != (void (*)(void))fighter_36870,
          "0x36870 is registered through the (rec, arg) wrapper");
    CHECK(fn_resolve(0x3A43Cu) != NULL, "0x3A43C is registered");
    CHECK(fn_resolve(0x35E04u) != NULL, "0x35E04 is registered");
    CHECK(fn_resolve(0x35E04u) != (void (*)(void))fighter_35e04,
          "0x35E04 is registered through the (rec, arg) wrapper");
    CHECK(fn_resolve(0x3E4E4u) != NULL, "0x3E4E4 is registered");
    CHECK(fn_resolve(0x3E4E4u) != (void (*)(void))fighter_3e4e4,
          "0x3E4E4 is registered through the (rec, arg) wrapper");
    CHECK(fn_resolve(0x3E62Cu) == (void (*)(void))fighter_3e62c,
          "0x3E62C is registered as fighter_3e62c");
    CHECK(fn_resolve(0x3E524u) == (void (*)(void))fighter_3e524,
          "0x3E524 is registered as fighter_3e524");
    CHECK(fn_resolve(0x3E4C4u) == (void (*)(void))fighter_3e4c4,
          "0x3E4C4 is registered as fighter_3e4c4");
    CHECK(fn_resolve(0x39CC8u) == (void (*)(void))fighter_39cc8,
          "0x39CC8 is registered as fighter_39cc8");
    CHECK(fn_resolve(0x347B8u) != NULL, "0x347B8 is registered");
    CHECK(fn_resolve(0x347B8u) != (void (*)(void))fighter_347b8,
          "0x347B8 is registered through the (rec, arg) wrapper");
    CHECK(fn_resolve(0x346F8u) != NULL, "0x346F8 is registered");
    CHECK(fn_resolve(0x346F8u) != (void (*)(void))fighter_346f8,
          "0x346F8 is registered through the (rec, arg) wrapper");
    CHECK(fn_resolve(0x35938u) != NULL, "0x35938 is registered");
    CHECK(fn_resolve(0x35938u) != (void (*)(void))fighter_35938,
          "0x35938 is registered through the (rec, arg) wrapper");
    CHECK(fn_resolve(0x4AC18u) != NULL, "0x4AC18 is registered");
    CHECK(fn_resolve(0x4AC18u) != (void (*)(void))fight_4ac18,
          "0x4AC18 is registered through the (rec, arg) wrapper");
    CHECK(fn_resolve(0x4AC80u) != NULL, "0x4AC80 is registered");
    CHECK(fn_resolve(0x4AC80u) != (void (*)(void))fight_4ac80,
          "0x4AC80 is registered through the (rec, arg) wrapper");
    CHECK(fn_resolve(0x3D17Cu) == (void (*)(void))fighter_3d17c,
          "0x3D17C is registered as fighter_3d17c");
    CHECK(fn_resolve(0x3D214u) != NULL, "0x3D214 is registered");
    CHECK(fn_resolve(0x3D214u) != (void (*)(void))fighter_3d214,
          "0x3D214 is registered through the (rec, arg) wrapper");
    CHECK(fn_resolve(0x3D26Cu) != NULL, "0x3D26C is registered");
    CHECK(fn_resolve(0x3D26Cu) != (void (*)(void))fighter_3d26c,
          "0x3D26C is registered through the (rec, arg) wrapper");

    s[0] = 0xD100;                           /* opcode 0x11, mode 0x4000 */
    s[1] = 0x9A34;                           /* the inline code pointer */
    s[2] = 0x0003;                           /* 0x00039A34 */
    s[3] = 2;                                /* the operand: EDX = 2 */
    s[4] = 0x1075;                           /* the literal id that ends the walk */

    DSD(rec + 0x14u) = linked;
    DSB(linked + 0x7Eu) = 6;
    DSW(rec + 0x56u) = 0;
    actors_anim_begin(rec, stream, 0x40E00000u);     /* 7.0f frame bits */
    CHECK_EQ_INT((int)DSD(rec + 0x24u), 0x40400000); /* 6 / 2 = 3.0f */
    CHECK_EQ_INT((int)DSD(rec + 0x20u), 0x40E00000); /* untouched by the code */
    CHECK_EQ_INT((int)DSW(FIGHT_ACTORS), 0x1075);    /* rec+0x56 = 0: its pset */
    CHECK_EQ_INT((int)DSD(rec + 0x08u), (int)(stream + 8u));
    /* §7.5's early-out: no linked record leaves the hold at the frame bits, so
     * the assertion above distinguishes "wrote 3.0f" from "never touched it". */
    DSD(rec + 0x14u) = 0;
    actors_anim_begin(rec, stream, 0x40E00000u);
    CHECK_EQ_INT((int)DSD(rec + 0x24u), 0x40E00000);

    /* The +0x7E byte is signed (0x39A41 movsx, then FILD word): 0x87 = -121
     * (the value the port's pre-fix 0x3AD27 read gave the demo; the raw's is
     * 0x11, see check_pose_entry), operand 10, so -121 / 10 = -12.1f. In single
     * precision that is 0xC141999A (python: struct.pack('<f', -121/10)); a
     * zero-extending read would give 135 / 10 = 13.5f (0x41580000).
     * rec+0x24 is seeded 0x40E00000 by the begin, which differs. */
    s[3] = 10;
    DSD(rec + 0x14u) = linked;
    DSB(linked + 0x7Eu) = 0x87;
    actors_anim_begin(rec, stream, 0x40E00000u);
    CHECK_EQ_INT((int)DSD(rec + 0x24u), (int)0xC141999Au);

    /* The raptor's attack stream 0xD2274 at 0xD2278: `D000 5E04 0003` is
     * opcode 0x10, mode 0x4000, the inline dword 0x00035E04. The begin's
     * pre-walk dispatches it to 0x35E04, which overwrites the 1.0f frame bits
     * with 3.0f and launches side 0 through 0x3BC70 (slot +0x52 = 4, rec+0x36
     * from the row); an unregistered target would leave all three. */
    {
        u32 slot = DS_001077B0;
        u32 row = FIGHT_RECS + 0x1C0u;
        u8 sv_slot[0x94], sv_d40[8];
        tf_snap(sv_slot, slot, 0x94u);
        tf_snap(sv_d40, DS_00107D40, 8u);
        DSD(slot) = rec;
        DSB(slot + 0x52u) = 0x66u;
        DSW(slot + 0x4Eu) = 1u;
        DSW(row) = 23u;
        DSW(row + 2u) = 550u;
        DSW(row + 4u) = 150u;
        DSD(DS_00107D40) = row;
        DSD(rec + 0x14u) = slot;
        DSB(rec + 0x51u) = 0;
        DSW(rec + 0x36u) = 0x5555u;
        s[0] = 0xD000;
        s[1] = 0x5E04;
        s[2] = 0x0003;
        s[3] = 0x1746;
        actors_anim_begin(rec, stream, 0x3F800000u);
        CHECK_EQ_INT((int)DSD(rec + 0x20u), 0x40400000);
        CHECK_EQ_INT((int)DSB(slot + 0x52u), 4);
        CHECK_EQ_INT((int)DSW(rec + 0x36u), 550);
        CHECK_EQ_INT((int)DSW(FIGHT_ACTORS), 0x1746);
        tf_put(sv_slot, slot, 0x94u);
        tf_put(sv_d40, DS_00107D40, 8u);
    }
}

/* ---- roar-timing Task 9: the T-rex's reaction-0x2B leap (record §19) ------ */

/* §19.3: 0x34E2C calls the (char, reaction) entry's *(u32*)anim[1] callback
 * with EAX = slot, EDX = rec, EBX = side. For char 0, reaction 0x2B the entry
 * is 0xA3884 (read_memory: 2c e6 03 00 00 00 00 00, callback 0x3E62C and no
 * stream), so the demo's f = 105 T-rex runs 0x3E62C: the 0xE7BDE stream
 * through 0x3C4CC at hold 3.0, the x re-anchor, slot +0x57 = 2, state 9/7/2 and
 * the +0x0C callback 0x3E524 that 0x3531C case 7 then runs each frame. The
 * 0xE7BDE stream's `D500 E4E4 0003` reaches 0x3E4E4, the leap. The
 * registrations are asserted in check_anim_hold_scaler (which runs
 * actors_init); they are made here only when this check runs without it. */
static void check_trex_leap(void)
{
    u32 s0 = DS_001077B0, s1 = DS_001077B0 + 0x94u;
    u32 r0 = FIGHT_RECS, r1 = FIGHT_RECS + 0x100u;
    u32 pset0 = FIGHT_ACTORS + 0x20u;
    u16 sv_78f6 = DSW(DS_001078F6);
    u8 sv_b00[4];
    tf_snap(sv_b00, DS_00104B00, 4u);
    if (fn_resolve(0x3E62Cu) == NULL)
        fn_register(0x3E62Cu, (void (*)(void))fighter_3e62c);
    if (fn_resolve(0x3E524u) == NULL)
        fn_register(0x3E524u, (void (*)(void))fighter_3e524);

    /* A: the 0x34E2C dispatch into 0x3E62C (0x35045). The slot is the demo's
     * f = 105 T-rex: state 9/0/0, char 0. Slot +0x42 bit 3 makes 0x18714 and
     * the 0x186D0 latch pass the record's +0x18 through, so the x 0x3E62C reads
     * before 0x3C4CC (0x3E64D, X = 0x1111) differs from the one the 0x3C480
     * latch leaves (the record's R = 0x2222): 0x188DC stores X only when it is
     * read first. +0x52 = 9 sends 0x3C4CC to 0x3C480, whose 0x188AC zeroes
     * rec+0x1C. The stream's `DC00 55B8 000E` loads rec+0x10 and replaces the
     * 3.0 hold with byte[0xE55B8 + rec+0x52] = 1 (read_memory: 01 01 01 02
     * ...), and `CD40 11B3` gives id 0x11B3 + rec+0x52 (0). */
    (void)tf_hit_fixture(0);
    fight_reset_slot_pair(s0, s1, r0, r1);
    DSB(r0 + 0x51u) = 0;
    DSB(r1 + 0x51u) = 1;
    DSW(r0 + 0x56u) = 1;
    DSW(pset0) = 0x0F35u;
    DSB(s0 + 0x42u) = 0x08u;
    DSB(s0 + 0x52u) = 9u;
    DSB(s0 + 0x53u) = 0x66u;
    DSB(s0 + 0x54u) = 0u;
    DSB(s0 + 0x57u) = 0x66u;
    DSB(s0 + 0x41u) = 0u;
    DSD(s0 + 0x0Cu) = 0x11111111u;
    DSD(s0 + 0x18u) = 0x22222222u;
    DSD(s0 + 0x1Cu) = 0x33333333u;
    DSD(s0 + 0x2Cu) = 0x1111u;
    DSD(s1 + 0x2Cu) = 0x7000u;
    DSD(r0 + 0x18u) = 0x2222u;
    DSD(r0 + 0x1Cu) = 0x5555u;
    DSD(r0 + 8u) = 0x00ABCDEFu;
    DSB(s1 + 0x53u) = 0x66u;
    hit_reaction_apply(0u, 0x2Bu);
    CHECK_EQ_INT((int)DSB(s0 + 0x5Fu), 0x2B);
    CHECK_EQ_INT((int)DSB(s0 + 0x57u), 2);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 9);
    CHECK_EQ_INT((int)DSB(s0 + 0x53u), 7);
    CHECK_EQ_INT((int)DSB(s0 + 0x54u), 2);
    CHECK_EQ_INT((int)DSD(s0 + 0x0Cu), 0x0003E524);
    CHECK_EQ_INT((int)DSD(s0 + 0x18u), 0x0003E484);
    CHECK_EQ_INT((int)DSD(s0 + 0x1Cu), 0x0003E4C4);
    CHECK_EQ_INT((int)(DSB(s0 + 0x41u) & 0x80u), 0x80);
    CHECK_EQ_INT((int)DSD(s0 + 0x2Cu), 0x1111);
    CHECK_EQ_INT((int)DSD(r0 + 0x18u), 0x2222);
    CHECK_EQ_INT((int)DSD(r0 + 0x1Cu), 0);
    CHECK_EQ_INT((int)DSD(r0 + 0x10u), 0x000E55B8);
    CHECK_EQ_INT((int)DSD(r0 + 0x24u), 0x3F800000);   /* byte[0xE55B8+0] = 1 */
    CHECK_EQ_INT((int)(DSW(pset0) & 0x7FFFu), 0x11B3);
    CHECK((DSD(r0 + 8u) - 0x000E7BDEu) < 0x60u,
          "0x3E62C starts the record on the 0xE7BDE stream");
    CHECK_EQ_INT((int)DSB(s1 + 0x53u), 0x66);

    /* B: 0x3531C case 7 (0x35431) runs the slot's +0x0C callback with EAX =
     * slot, EDX = rec, EBX = side: 0x3E524's +0x57 = 0 arm sets the horizontal
     * speed from the vertical one, 10 * 701 / 7 = 1001 (0x3E588 idiv
     * truncates 1001.4), negated through 0x3C190 while the pset is unflipped
     * (0x1A570), and keeps +0x57 while the vertical speed is >= 0. The record's
     * +0x63 = 10 clears the slot's +0x8A (0x3E54E). A's 0x18B04 left the pset
     * hflipped (0x1111 < the other slot's 0x7000), so it is cleared here. */
    DSW(DS_001078F6) = 0;
    DSW(pset0) = (u16)(DSW(pset0) & 0x7FFFu);
    DSB(s0 + 0x57u) = 0u;
    DSB(s0 + 0x8Au) = 1u;
    DSB(r0 + 0x63u) = 10u;
    DSB(s0 + 0x43u) = 0x30u;
    DSW(r0 + 0x36u) = 701u;
    DSW(r0 + 0x34u) = 0x5555u;
    fighter_state_3531c(0u);
    CHECK_EQ_INT((int)(s16)DSW(r0 + 0x34u), -1001);
    CHECK_EQ_INT((int)DSB(s0 + 0x57u), 0);
    CHECK_EQ_INT((int)DSB(s0 + 0x8Au), 0);
    CHECK_EQ_INT((int)(DSB(s0 + 0x43u) & 0x30u), 0);
    /* The hflipped pset keeps the sign (0x3C1B8); +0x63 = 9 keeps +0x8A. */
    DSW(pset0) = (u16)(DSW(pset0) | 0x8000u);
    DSB(s0 + 0x8Au) = 1u;
    DSB(r0 + 0x63u) = 9u;
    fighter_3e524(s0, r0, 0u);
    CHECK_EQ_INT((int)(s16)DSW(r0 + 0x34u), 1001);
    CHECK_EQ_INT((int)DSB(s0 + 0x8Au), 1);
    /* Falling (v < 0): +0x57 = 1, gravity 15, no horizontal speed. */
    DSW(r0 + 0x36u) = (u16)-3;
    DSW(r0 + 0x44u) = 0x5555u;
    fighter_3e524(s0, r0, 0u);
    CHECK_EQ_INT((int)DSB(s0 + 0x57u), 1);
    CHECK_EQ_INT((int)DSW(r0 + 0x44u), 15);
    CHECK_EQ_INT((int)DSW(r0 + 0x34u), 0);

    /* C: +0x57 = 1 lands when word[0xBD882 + char*2] >> 16 (char 0: the
     * dword 0x16001180, 0x1600 = 5632) exceeds the slot's +0x30, or when the
     * vertical speed is 0 (0x3E5CE/0x3E5D5). Landing: +0x54 = 0, 0x188AC
     * zeroes rec+0x1C, 0x3C148 zeroes +0x34/+0x43/+0x42, 0x3C16C zeroes
     * +0x36/+0x44, the 0xC8B58[0] stream (0xE6F82, first id 0x0FA7) starts at
     * hold 3.0 and +0x57 = 3. +0x52 = 0 takes 0x3C4CC's plain 0x2BC30 arm. */
    DSB(s0 + 0x52u) = 0u;
    DSB(s0 + 0x57u) = 1u;
    DSB(s0 + 0x54u) = 2u;
    DSD(s0 + 0x30u) = 6000u;
    DSW(r0 + 0x36u) = 5u;
    DSW(r0 + 0x44u) = 0x5555u;
    fighter_3e524(s0, r0, 0u);
    CHECK_EQ_INT((int)DSB(s0 + 0x57u), 1);                /* 5632 <= 6000, v != 0 */
    CHECK_EQ_INT((int)DSB(s0 + 0x54u), 2);
    DSD(s0 + 0x30u) = 5632u;
    fighter_3e524(s0, r0, 0u);
    CHECK_EQ_INT((int)DSB(s0 + 0x57u), 1);                /* `jg`: equal stays */
    DSW(r0 + 0x36u) = 0u;
    DSD(r0 + 0x1Cu) = 0x5555u;
    DSB(r0 + 0x43u) = 0x66u;
    DSW(r0 + 0x34u) = 0x5555u;
    DSD(r0 + 0x24u) = 0x11111111u;
    fighter_3e524(s0, r0, 0u);
    CHECK_EQ_INT((int)DSB(s0 + 0x57u), 3);
    CHECK_EQ_INT((int)DSB(s0 + 0x54u), 0);
    CHECK_EQ_INT((int)DSD(r0 + 0x1Cu), 0);
    CHECK_EQ_INT((int)DSB(r0 + 0x43u), 0);
    CHECK_EQ_INT((int)DSW(r0 + 0x34u), 0);
    CHECK_EQ_INT((int)DSW(r0 + 0x44u), 0);
    CHECK_EQ_INT((int)DSD(r0 + 0x24u), 0x40400000);
    CHECK_EQ_INT((int)(DSW(pset0) & 0x7FFFu), 0x0FA7);
    DSB(s0 + 0x57u) = 1u;
    DSB(s0 + 0x54u) = 2u;
    DSD(s0 + 0x30u) = 5631u;
    DSW(r0 + 0x36u) = 5u;
    fighter_3e524(s0, r0, 0u);
    CHECK_EQ_INT((int)DSB(s0 + 0x57u), 3);                /* 5632 > 5631 */
    /* +0x57 = 3 and 2 return (0x3E624). */
    DSB(s0 + 0x54u) = 2u;
    DSW(r0 + 0x36u) = 701u;
    DSW(r0 + 0x34u) = 0x5555u;
    fighter_3e524(s0, r0, 0u);
    CHECK_EQ_INT((int)DSW(r0 + 0x34u), 0x5555);
    CHECK_EQ_INT((int)DSB(s0 + 0x54u), 2);
    DSB(s0 + 0x57u) = 2u;
    DSD(s0 + 0x30u) = 0u;
    fighter_3e524(s0, r0, 0u);
    CHECK_EQ_INT((int)DSW(r0 + 0x34u), 0x5555);
    CHECK_EQ_INT((int)DSB(s0 + 0x57u), 2);

    /* D: the 0xE7BDE stream's `D500 E4E4 0003` (opcode 0x15, mode 0x4000, the
     * dword 0x0003E4E4) dispatched from a crafted walk: 0x3E4E4 restarts the
     * record at 0xE7BFA, vertical speed 0x320, gravity 0x23, and slot +0x57 =
     * 0. 0xE7BFA's `DA00 5FD8 000E` / `DC00 55D8 000E` load rec+0x0C/+0x10 and
     * the hold byte[0xE55D8 + 0] = 5 (read_memory: 05 01 01 01 ...), which
     * the crafted stream's own words cannot produce. Without rec+0x14 it
     * writes nothing (0x3E4EE). */
    {
        u32 stream = FIGHT_RECS + 0x3900u;
        DSW(stream) = 0xD500u;
        DSW(stream + 2u) = 0xE4E4u;
        DSW(stream + 4u) = 0x0003u;
        DSW(stream + 6u) = 0x1746u;
        DSD(r0 + 0x14u) = s0;
        DSB(s0 + 0x57u) = 0x66u;
        DSW(r0 + 0x36u) = 0x5555u;
        DSW(r0 + 0x44u) = 0x5555u;
        actors_anim_begin(r0, stream, 0x3F800000u);
        CHECK_EQ_INT((int)DSW(r0 + 0x36u), 0x320);
        CHECK_EQ_INT((int)DSW(r0 + 0x44u), 0x23);
        CHECK_EQ_INT((int)DSB(s0 + 0x57u), 0);
        CHECK_EQ_INT((int)DSD(r0 + 0x0Cu), 0x000E5FD8);
        CHECK_EQ_INT((int)DSD(r0 + 0x10u), 0x000E55D8);
        CHECK_EQ_INT((int)DSD(r0 + 0x24u), 0x40A00000);   /* 5.0f */
        CHECK((DSD(r0 + 8u) - 0x000E7BFAu) < 0x40u,
              "0x3E4E4 restarts the record at 0xE7BFA");
        DSD(r0 + 0x14u) = 0;
        DSB(s0 + 0x57u) = 0x66u;
        DSW(r0 + 0x36u) = 0x5555u;
        fighter_3e4e4(r0);
        CHECK_EQ_INT((int)DSB(s0 + 0x57u), 0x66);
        CHECK_EQ_INT((int)DSW(r0 + 0x36u), 0x5555);
    }

    DSW(DS_001078F6) = sv_78f6;
    tf_put(sv_b00, DS_00104B00, 4u);
}

/* ---- roar-timing Task 10: the knockback pose handler 0x39CC8 (record §20) - */

/* A stand-in slot +0x14 target for 0x35050: the raw has no ported writer of a
 * non-zero +0x14, so the call is driven through a test-only registration at an
 * address outside the code object (0x10000..0x73B14). */
#define KB_STUB_14 0x7FFF0000u
static int kb_stub_calls;
static u32 kb_stub_ret;
static u32 kb_stub_arg;
static u32 kb_stub_14(u32 slot)
{
    kb_stub_calls++;
    kb_stub_arg = slot;
    return kb_stub_ret;
}

/* The demo raptor at f = 114: side 1, char 3, in the 0x39F40 pose 0x10/0x0A
 * with +0x10 = 0x39CC8. Slot +0x42 bit 3 makes the 0x186D0 latch copy the
 * record's +0x18/+0x1C into the slot's +0x2C/+0x30, so the y anchors are
 * observable on rec+0x1C. The four pose words are 0x3AAFC's 0x3AC89 call
 * (EDX = 0xFFFFFFB0, EBX = 0x46, ECX = 0x0C, frame 0x14) as 0x39F40 stores
 * them for side 1 (0x107A68/0x107A78/0x107A60/0x107A70 + 4); side 0's words
 * are distinct sentinels, so a read of the wrong side changes every result. */
static void kb_seed(u32 s0, u32 s1, u32 r0, u32 r1)
{
    (void)tf_hit_fixture(0);
    fight_reset_slot_pair(s0, s1, r0, r1);
    DSB(r0 + 0x51u) = 0;
    DSB(r1 + 0x51u) = 1;
    DSW(r1 + 0x56u) = 2;
    DSB(s1 + 0x7Au) = 3;
    DSB(s1 + 0x42u) = 0x08u;
    DSB(s1 + 0x52u) = 0x10u;
    DSB(s1 + 0x53u) = 0x0Au;
    DSB(s1 + 0x54u) = 2u;
    DSD(s1 + 0x10u) = 0x00039CC8u;
    DSB(s1 + 0x41u) = 0;
    DSB(s1 + 0x68u) = 1;
    DSW(s1 + 0x74u) = 0x1234u;
    DSW(s0 + 0x74u) = 0x2222u;
    DSD(s1 + 0x14u) = 0;
    DSD(r1 + 0x18u) = 0x4321u;
    DSD(r1 + 0x1Cu) = 3000u;
    DSD(r1 + 0x30u) = 0x00090000u;
    DSW(r1 + 0x28u) = 0;                     /* pset unflipped (bit 14 clear) */
    DSW(r1 + 0x34u) = 0x5555u;
    DSW(r1 + 0x36u) = 0x5555u;
    DSW(r1 + 0x44u) = 0x5555u;
    DSB(r1 + 0x43u) = 0x66u;
    DSB(r1 + 0x63u) = 0x55u;
    DSD(r1 + 8u) = 0x00ABCDEFu;
    DSD(r1 + 0x24u) = 0x11111111u;
    fighter_slot_latch(1u);                  /* slot+0x2C/+0x30 = 0x4321/3000 */
    DSD(0x00107A60u) = 0x10u;                /* side 0: n */
    DSD(0x00107A64u) = 0x0Cu;                /* side 1: n */
    DSD(0x00107A68u) = 0xFFFFFF00u;          /* side 0: h */
    DSD(0x00107A6Cu) = 0xFFFFFFB0u;          /* side 1: h = -80 */
    DSD(0x00107A70u) = 0x08u;                /* side 0: m */
    DSD(0x00107A74u) = 0x14u;                /* side 1: m */
    DSD(0x00107A78u) = 0x20u;                /* side 0: a */
    DSD(0x00107A7Cu) = 0x46u;                /* side 1: a */
}

/* §20: 0x39CC8's +0x58 machine for the demo raptor, char 3. Expected values
 * are from the raw tables (read_memory): 0xBED10[3] = 7, 0xBED38[3] = 7,
 * 0xBED60/88/B0[3] = 0xD2A6E/0xD2AA4/0xD2ADA (whose ED40 indirections give
 * the first ids 0x17F5/0x18AD/0x18B3), word[0xBD884 + 3*2] = 0x1600 = 5632,
 * word[0xBECFA + 3*2] = 2560. Launch: hold 12/7 (0x3FDB6DB7), gravity
 * 0x39AC8(70, 12) = 8960/144 = 62, vertical 62*12 = 744, horizontal
 * -80*64/32 = -160, negated while unflipped (0x3C190). Fall: hold 20/7
 * (0x4036DB6E), gravity 0x39AC8(|dy/64|, 20). */
static void check_knockback_pose(void)
{
    u32 s0 = DS_001077B0, s1 = DS_001077B0 + 0x94u;
    u32 r0 = FIGHT_RECS, r1 = FIGHT_RECS + 0x100u;
    u32 pset1 = FIGHT_ACTORS + 0x40u;
    u16 sv_78f6 = DSW(DS_001078F6);
    u8 sv_b00[4], sv_a60[0x20];
    tf_snap(sv_b00, DS_00104B00, 4u);
    tf_snap(sv_a60, 0x00107A60u, 0x20u);
    if (fn_resolve(0x39CC8u) == NULL)
        fn_register(0x39CC8u, (void (*)(void))fighter_39cc8);

    /* Phase 0 (0x39CFE): +0x58 = 1 and nothing else. */
    kb_seed(s0, s1, r0, r1);
    DSB(s1 + 0x58u) = 0;
    fighter_39cc8(s1, 1u);
    CHECK_EQ_INT((int)DSB(s1 + 0x58u), 1);
    CHECK_EQ_INT((int)DSW(r1 + 0x36u), 0x5555);

    /* Phase 1 through 0x3531C case 10 (0x354E2), airborne (+0x54 = 2): 0x39B30
     * starts the stream through 0x3C520 and, the ground 5632 being above the
     * slot's +0x30 = 3000, re-anchors y to it through 0x1890C (0x39BF0). */
    kb_seed(s0, s1, r0, r1);
    DSB(s1 + 0x58u) = 1;
    DSW(DS_001078F6) = 0;
    fighter_state_3531c(1u);
    CHECK_EQ_INT((int)DSB(s1 + 0x58u), 2);
    CHECK((DSD(r1 + 8u) - 0x000D2A6Eu) < 0x40u,
          "0x39B30 starts the 0xBED60[3] stream");
    CHECK_EQ_INT((int)DSD(r1 + 0x24u), 0x3FDB6DB7);      /* 12 / 7 */
    CHECK_EQ_INT((int)(DSW(pset1) & 0x7FFFu), 0x17F5);
    CHECK_EQ_INT((int)DSD(r1 + 0x1Cu), 5632);
    CHECK_EQ_INT((int)DSW(r1 + 0x44u), 62);
    CHECK_EQ_INT((int)DSW(r1 + 0x36u), 744);
    CHECK_EQ_INT((int)(s16)DSW(r1 + 0x34u), 160);
    CHECK_EQ_INT((int)DSB(r1 + 0x43u), 0);
    CHECK_EQ_INT((int)DSB(r1 + 0x63u), 0);
    CHECK_EQ_INT((int)(DSB(s1 + 0x41u) & 0x80u), 0x80);
    CHECK_EQ_INT((int)DSB(s1 + 0x68u), 2);
    CHECK_EQ_INT((int)DSW(s1 + 0x74u), 0x1234);          /* +0x68 < 3 */

    /* Airborne above the ground: no re-anchor. The flipped pset keeps -160,
     * and the third launch sets the side's +0x74 word (0x39CA7). */
    kb_seed(s0, s1, r0, r1);
    DSD(r1 + 0x1Cu) = 9000u;
    fighter_slot_latch(1u);
    DSW(r1 + 0x28u) = 0x4000u;
    DSB(s1 + 0x68u) = 2;
    DSB(s1 + 0x58u) = 1;
    fighter_39cc8(s1, 1u);
    CHECK_EQ_INT((int)DSD(r1 + 0x1Cu), 9000);
    CHECK_EQ_INT((int)(s16)DSW(r1 + 0x34u), -160);
    CHECK_EQ_INT((int)DSB(s1 + 0x68u), 3);
    CHECK_EQ_INT((int)DSW(s1 + 0x74u), 0x029A);
    CHECK_EQ_INT((int)DSW(s0 + 0x74u), 0x2222);

    /* Grounded (+0x54 != 2): 0x3C480 zeroes y, then 0x39BC5 re-anchors it to
     * the ground unconditionally (the airborne case above keeps 9000). */
    kb_seed(s0, s1, r0, r1);
    DSD(r1 + 0x1Cu) = 9000u;
    fighter_slot_latch(1u);
    DSB(s1 + 0x54u) = 0;
    DSB(s1 + 0x58u) = 1;
    fighter_39cc8(s1, 1u);
    CHECK_EQ_INT((int)DSD(r1 + 0x1Cu), 5632);
    CHECK_EQ_INT((int)DSD(r1 + 0x24u), 0x3FDB6DB7);

    /* The grounded anchor is unconditional (0x39BC5): with slot +0x42 bit 3
     * clear the latch adds the side's DS_00100AB4 offset, here 8000, so after
     * 0x3C480 zeroes y the slot's +0x30 is 8000 > 5632 and only the
     * unconditional 0x1890C moves rec+0x1C (to 5632 - 8000). DS_001077A8[1] =
     * 0 is 0x18540's early-out and DS_00100AF0[1] = slot+0x20 skips 0x18350,
     * so the offsets stay as seeded. */
    {
        u8 sv_ab0[0x10], sv_af0[8];
        u32 sv_7ac = DSD(DS_001077A8 + 4u);
        tf_snap(sv_ab0, DS_00100AB0, 0x10u);
        tf_snap(sv_af0, DS_00100AF0, 8u);
        kb_seed(s0, s1, r0, r1);
        DSD(DS_001077A8 + 4u) = 0;
        DSD(DS_00100AF0 + 4u) = DSD(s1 + 0x20u);
        DSD(DS_00100AB0 + 8u) = 0;
        DSD(DS_00100AB4 + 8u) = 8000u;
        DSB(s1 + 0x42u) = 0;
        DSD(r1 + 0x1Cu) = 9000u;
        DSB(s1 + 0x54u) = 0;
        DSB(s1 + 0x58u) = 1;
        fighter_39cc8(s1, 1u);
        CHECK_EQ_INT((int)DSD(r1 + 0x1Cu), 5632 - 8000);
        CHECK_EQ_INT((int)DSD(s1 + 0x30u), 5632);
        DSD(DS_001077A8 + 4u) = sv_7ac;
        tf_put(sv_ab0, DS_00100AB0, 0x10u);
        tf_put(sv_af0, DS_00100AF0, 8u);
    }

    /* Phase 2 waits while the vertical speed is >= 0 (0x39D26 setl). */
    kb_seed(s0, s1, r0, r1);
    DSB(s1 + 0x58u) = 2;
    DSW(r1 + 0x36u) = 0;
    fighter_39cc8(s1, 1u);
    CHECK_EQ_INT((int)DSB(s1 + 0x58u), 2);
    CHECK_EQ_INT((int)DSW(r1 + 0x44u), 0x5555);
    CHECK_EQ_INT((int)DSB(r1 + 0x63u), 0x55);

    /* Falling from 12 993 above the ground (dy/64 = 203.02, truncated to
     * 203): gravity 203*128/400 = 64.96 -> 64 (0x39AC8 keeps q), the
     * 0xBED88[3] stream at hold 20/7, +0x58 = 3. */
    kb_seed(s0, s1, r0, r1);
    DSB(s1 + 0x58u) = 2;
    DSW(r1 + 0x36u) = (u16)-62;
    DSD(s1 + 0x30u) = 5632u + 12993u;
    fighter_39cc8(s1, 1u);
    CHECK_EQ_INT((int)DSB(s1 + 0x58u), 3);
    CHECK_EQ_INT((int)DSW(r1 + 0x44u), 64);
    CHECK_EQ_INT((int)DSB(r1 + 0x63u), 0);
    CHECK((DSD(r1 + 8u) - 0x000D2AA4u) < 0x40u,
          "0x39CC8 case 2 starts the 0xBED88[3] stream");
    CHECK_EQ_INT((int)DSD(r1 + 0x24u), 0x4036DB6E);      /* 20 / 7 */
    CHECK_EQ_INT((int)(DSW(pset1) & 0x7FFFu), 0x18AD);
    /* Below the ground: -12993/64 truncates to -203 (a floor would give -204
     * and gravity 65), and its magnitude is taken (no abs: 0xFFC0). */
    kb_seed(s0, s1, r0, r1);
    DSB(s1 + 0x58u) = 2;
    DSW(r1 + 0x36u) = (u16)-62;
    DSD(s1 + 0x30u) = (u32)(5632 - 12993);
    fighter_39cc8(s1, 1u);
    CHECK_EQ_INT((int)DSW(r1 + 0x44u), 64);
    /* 204 * 64 above the ground: 0x39AC8 gives 204*128/400 = 65.28 -> 65. */
    kb_seed(s0, s1, r0, r1);
    DSB(s1 + 0x58u) = 2;
    DSW(r1 + 0x36u) = (u16)-62;
    DSD(s1 + 0x30u) = 5632u + 204u * 64u;
    fighter_39cc8(s1, 1u);
    CHECK_EQ_INT((int)DSW(r1 + 0x44u), 65);

    /* Phase 3, no landing: the ground 5632 is compared with the slot's +0x30
     * before the 0x186D0 latch (0x39E03, then 0x39E0C). 6000 does not land
     * even though the latch then lowers +0x30 to the record's 1000. */
    kb_seed(s0, s1, r0, r1);
    DSB(s1 + 0x58u) = 3;
    DSD(s1 + 0x30u) = 6000u;
    DSD(r1 + 0x1Cu) = 1000u;
    DSB(r1 + 0x28u) = 0;
    fighter_39cc8(s1, 1u);
    CHECK_EQ_INT((int)DSB(s1 + 0x58u), 3);
    CHECK_EQ_INT((int)(DSB(r1 + 0x28u) & 0x20u), 0x20);
    CHECK_EQ_INT((int)DSD(s1 + 0x30u), 1000);            /* the latch ran */
    CHECK_EQ_INT((int)DSB(r1 + 0x63u), 0x55);

    /* Phase 3, landing at the equal value (setge) and with a record above the
     * ground (the pre-latch compare): the 0xBEDB0[3] stream at 3.0, y to 2560,
     * rec+0x43 = 0x14, the side's +0x74 = 0x29A, +0x58 = 4, speeds zeroed and
     * the 0xBB1DC dust at (slot+0x2C, rec+0x30 >> 16, 0). A three-record
     * scratch pool keeps the spawn off the real pool. */
    {
        u32 sv_pool = DSD(DS_001014F4);
        u32 sv_free = DSD(DS_00105B3C);
        u32 sv_free4 = DSD(DS_00105B3C + 4u);
        u32 sv_act = DSD(DS_00105BCC);
        u32 sv_act4 = DSD(DS_00105BCC + 4u);
        u32 pool = 0x003F21000u;
        u32 p2 = pool + 0x68u;
        u32 p3 = pool + 0xD0u;
        int pass;
        for (pass = 0; pass < 2; pass++) {
            DSD(DS_001014F4) = pool;
            DSD(pool) = p2;
            DSD(pool + 4u) = DS_00105B3C;
            DSD(p2) = p3;
            DSD(p2 + 4u) = pool;
            DSD(p3) = DS_00105B3C;
            DSD(p3 + 4u) = p2;
            DSD(DS_00105B3C) = pool;
            DSD(DS_00105B3C + 4u) = p3;
            DSD(DS_00105BCC) = DS_00105BCC;
            DSD(DS_00105BCC + 4u) = DS_00105BCC;
            DSD(pool + 0x18u) = 0x77777777u;
            DSD(pool + 0x1Cu) = 0x77777777u;
            DSW(pool + 0x32u) = 0x7777u;

            kb_seed(s0, s1, r0, r1);
            DSB(s1 + 0x58u) = 3;
            if (pass == 0) {
                DSD(s1 + 0x30u) = 5632u;             /* equal: lands */
            } else {
                DSD(s1 + 0x30u) = 5000u;             /* below, pre-latch */
                DSD(r1 + 0x1Cu) = 9000u;             /* the latch's value */
            }
            DSB(r1 + 0x28u) = 0;
            fighter_39cc8(s1, 1u);
            CHECK_EQ_INT((int)DSB(s1 + 0x58u), 4);
            CHECK_EQ_INT((int)DSB(r1 + 0x63u), 0);
            CHECK((DSD(r1 + 8u) - 0x000D2ADAu) < 0x40u,
                  "0x39CC8 lands on the 0xBEDB0[3] stream");
            CHECK_EQ_INT((int)DSD(r1 + 0x24u), 0x40400000);
            CHECK_EQ_INT((int)(DSW(pset1) & 0x7FFFu), 0x18B3);
            CHECK_EQ_INT((int)DSD(r1 + 0x1Cu), 2560);
            CHECK_EQ_INT((int)DSD(r1 + 0x18u), 0x4321);
            CHECK_EQ_INT((int)DSW(r1 + 0x36u), 0);
            CHECK_EQ_INT((int)DSW(r1 + 0x44u), 0);
            CHECK_EQ_INT((int)DSB(r1 + 0x43u), 0x14);
            CHECK_EQ_INT((int)DSW(s1 + 0x74u), 0x029A);
            CHECK_EQ_INT((int)DSW(s0 + 0x74u), 0x2222);
            CHECK_EQ_INT((int)(DSB(r1 + 0x28u) & 0x20u), 0x20);
            CHECK_EQ_INT((int)DSD(pool + 0x18u), 0x4321);
            CHECK_EQ_INT((int)DSW(pool + 0x32u), 9);
            CHECK_EQ_INT((int)DSD(pool + 0x1Cu), 0);
        }
        DSD(DS_001014F4) = sv_pool;
        DSD(DS_00105B3C) = sv_free;
        DSD(DS_00105B3C + 4u) = sv_free4;
        DSD(DS_00105BCC) = sv_act;
        DSD(DS_00105BCC + 4u) = sv_act4;
    }

    /* Phase 4 clears the record's +0x28 bit 5 and the slot's +0x54; any
     * higher phase returns (0x39CE7). */
    kb_seed(s0, s1, r0, r1);
    DSB(s1 + 0x58u) = 4;
    DSB(r1 + 0x28u) = 0xFFu;
    fighter_39cc8(s1, 1u);
    CHECK_EQ_INT((int)DSB(r1 + 0x28u), 0xDF);
    CHECK_EQ_INT((int)DSB(s1 + 0x54u), 0);
    CHECK_EQ_INT((int)DSB(s1 + 0x58u), 4);
    DSB(s1 + 0x58u) = 5;
    DSB(r1 + 0x28u) = 0xFFu;
    DSB(s1 + 0x54u) = 2;
    fighter_39cc8(s1, 1u);
    CHECK_EQ_INT((int)DSB(r1 + 0x28u), 0xFF);
    CHECK_EQ_INT((int)DSB(s1 + 0x54u), 2);

    /* 0x35050 first (0x39CD9): the side's own slot +0x14 target runs and is
     * cleared when it returns non-zero, kept when it returns zero; the other
     * slot's +0x14 is not called. */
    if (fn_resolve(KB_STUB_14) == NULL)
        fn_register(KB_STUB_14, (void (*)(void))kb_stub_14);
    kb_seed(s0, s1, r0, r1);
    DSB(s1 + 0x58u) = 5;
    DSD(s1 + 0x14u) = KB_STUB_14;
    kb_stub_calls = 0;
    kb_stub_ret = 1;
    kb_stub_arg = 0x5555u;
    fighter_39cc8(s1, 1u);
    CHECK_EQ_INT(kb_stub_calls, 1);
    CHECK_EQ_INT((int)kb_stub_arg, (int)s1);             /* EAX = the slot */
    CHECK_EQ_INT((int)DSD(s1 + 0x14u), 0);
    DSD(s1 + 0x14u) = KB_STUB_14;
    kb_stub_ret = 0;
    fighter_39cc8(s1, 1u);
    CHECK_EQ_INT(kb_stub_calls, 2);
    CHECK_EQ_INT((int)DSD(s1 + 0x14u), (int)KB_STUB_14);
    DSD(s1 + 0x14u) = 0;
    DSD(s0 + 0x14u) = KB_STUB_14;
    fighter_39cc8(s1, 1u);
    CHECK_EQ_INT(kb_stub_calls, 2);
    DSD(s0 + 0x14u) = 0;

    /* 0x350D0 makes the same call at 0x3514C (EAX = EDX = its own slot, a
     * non-zero return zeroes the field at 0x35157) before its +0x78 early
     * return (0x35199). */
    kb_seed(s0, s1, r0, r1);
    DSD(s1 + 0x14u) = KB_STUB_14;
    DSB(s1 + 0x43u) = 0;
    DSW(s1 + 0x78u) = 1u;
    kb_stub_calls = 0;
    kb_stub_ret = 1;
    kb_stub_arg = 0x5555u;
    fighter_state_350d0(1u);
    CHECK_EQ_INT(kb_stub_calls, 1);
    CHECK_EQ_INT((int)kb_stub_arg, (int)s1);
    CHECK_EQ_INT((int)DSD(s1 + 0x14u), 0);
    DSW(s1 + 0x78u) = 0;

    /* A second character whose divisors differ (read_memory: 0xBED10[0] = 5,
     * 0xBED38[0] = 8; the ground word[0xBD884] = 0x1600 as for char 3), so the
     * launch hold n / 0xBED10 (12 / 5 = 0x4019999A) and the fall hold
     * m / 0xBED38 (20 / 8 = 0x40200000) each fail if the tables are swapped
     * (12 / 8, 20 / 5). The streams are 0xBED60[0] = 0xE7748 and 0xBED88[0] =
     * 0xE777A. */
    kb_seed(s0, s1, r0, r1);
    DSB(s1 + 0x7Au) = 0;
    DSB(s1 + 0x58u) = 1;
    fighter_39cc8(s1, 1u);
    CHECK_EQ_INT((int)DSD(r1 + 0x24u), 0x4019999A);      /* 12 / 5 */
    CHECK((DSD(r1 + 8u) - 0x000E7748u) < 0x40u,
          "0x39B30 starts the 0xBED60[0] stream");
    kb_seed(s0, s1, r0, r1);
    DSB(s1 + 0x7Au) = 0;
    DSB(s1 + 0x58u) = 2;
    DSW(r1 + 0x36u) = (u16)-62;
    DSD(s1 + 0x30u) = 5632u + 12993u;
    fighter_39cc8(s1, 1u);
    CHECK_EQ_INT((int)DSD(r1 + 0x24u), 0x40200000);      /* 20 / 8 */
    CHECK((DSD(r1 + 8u) - 0x000E777Au) < 0x40u,
          "0x39CC8 case 2 starts the 0xBED88[0] stream");

    DSW(DS_001078F6) = sv_78f6;
    tf_put(sv_b00, DS_00104B00, 4u);
    tf_put(sv_a60, 0x00107A60u, 0x20u);
}

/* ---- roar-timing Task 11: the knockdown floor 0x347B8 (record §21) -------- */

/* The demo raptor at f = 165: side 1, char 3, still in the knockback pose
 * 0x10/0x0A with the landing's +0x74 counting down, +0x5A = 22 against the
 * T-rex's 9 and its own +0x63 = 1 (the PR_T11 trace), so 0x340BC fails. Every
 * field 0x347B8/0x34168/0x346F8 write is a sentinel that differs from its
 * post-condition. */
static void kf_seed(u32 s0, u32 s1, u32 r0, u32 r1)
{
    (void)tf_hit_fixture(0);
    fight_reset_slot_pair(s0, s1, r0, r1);
    DSB(r0 + 0x51u) = 0;
    DSB(r1 + 0x51u) = 1;
    DSW(r0 + 0x56u) = 1;
    DSW(r1 + 0x56u) = 2;
    DSB(s0 + 0x7Au) = 0;
    DSB(s1 + 0x7Au) = 3;
    DSB(s1 + 0x52u) = 0x10u;
    DSB(s1 + 0x53u) = 0x0Au;
    DSB(s1 + 0x54u) = 2u;
    DSB(s0 + 0x52u) = 0x10u;
    DSB(s0 + 0x53u) = 0x0Au;
    DSB(s0 + 0x54u) = 2u;
    DSB(s0 + 0x43u) = 0xFFu;
    DSB(s1 + 0x43u) = 0xFFu;
    DSB(s0 + 0x41u) = 0;
    DSB(s1 + 0x41u) = 0;
    DSB(s0 + 0x5Du) = 0x66u;
    DSB(s1 + 0x5Du) = 0x66u;
    DSW(s0 + 0x74u) = 0x2222u;
    DSW(s1 + 0x74u) = 0x1234u;
    DSB(s0 + 0x5Au) = 9u;
    DSB(s1 + 0x5Au) = 22u;
    DSB(s0 + 0x63u) = 1u;
    DSB(s1 + 0x63u) = 1u;
    DSW(s0 + 0x8Cu) = 0;
    DSW(s1 + 0x8Cu) = 0;
    DSW(r0 + 0x34u) = 0x5555u;
    DSW(r0 + 0x36u) = 0x5555u;
    DSW(r0 + 0x44u) = 0x5555u;
    DSW(r1 + 0x34u) = 0x5555u;
    DSW(r1 + 0x36u) = 0x5555u;
    DSW(r1 + 0x44u) = 0x5555u;
    DSB(r1 + 0x42u) = 0x66u;
    DSB(r1 + 0x43u) = 0x66u;
    DSD(r0 + 0x10u) = 0x33333333u;
    DSD(r1 + 0x10u) = 0x33333333u;
    DSD(r0 + 0x24u) = 0x11111111u;
    DSD(r1 + 0x24u) = 0x11111111u;
    DSD(r0 + 8u) = 0x00ABCDEFu;
    DSD(r1 + 8u) = 0x00ABCDEFu;
    DSD(DS_001077A0) = 0x11111111u;
    DSD(DS_001077A0 + 4u) = 0x22222222u;
    DSB(DS_001078FF) = 0x66u;
    DSB(DS_00104AE9) = 0x02u;
    DSB(DS_00104B16) = 0;
    DSD(DS_00104AD4) = 0x66u;
}

/* §21: 0x347B8, its gate 0x340BC, the stun start 0x34168 and the get-up
 * 0x346F8. The floor streams and their first DC00 operand (which 0x2BC30's
 * walk stores in rec+0x10) are from read_memory: 0xD28FC `DC00 1478 000D`
 * (the raptor's plain arm, 0x3479C[3]), 0xD2940 `DC00 1618 000D` (its stun
 * arm, 0x34780[3]), 0xE761A `DC00 5598 000E` (0x3479C[0]), 0xE7656
 * `DC00 57F8 000E` (0x34780[0]) and 0xE4290 `DC00 2528 000E` (0x3479C[1] and
 * the char > 6 default). Both raptor streams take the hold byte 2 (0xD1478 /
 * 0xD1618: 02 02 ..) and the first id 0x18B9 from their ED40 tables. */
static void check_knockdown_floor(void)
{
    u32 s0 = DS_001077B0, s1 = DS_001077B0 + 0x94u;
    u32 r0 = FIGHT_RECS, r1 = FIGHT_RECS + 0x100u;
    u32 pset1 = FIGHT_ACTORS + 0x40u;
    u32 stream = FIGHT_RECS + 0x3900u;
    u8 sv_b00[4], sv_7a0[8], sv_ad4[4];
    u8 sv_b16 = DSB(DS_00104B16);
    u8 sv_8ff = DSB(DS_001078FF);
    u8 sv_ae9 = DSB(DS_00104AE9);
    tf_snap(sv_b00, DS_00104B00, 4u);
    tf_snap(sv_7a0, DS_001077A0, 8u);
    tf_snap(sv_ad4, DS_00104AD4, 4u);
    if (fn_resolve(0x347B8u) == NULL)
        fn_register(0x347B8u, (void (*)(void))fighter_347b8);
    if (fn_resolve(0x346F8u) == NULL)
        fn_register(0x346F8u, (void (*)(void))fighter_346f8);

    /* A: the gate 0x340BC, side 1 against side 0. Passing: +0x8C = 0, both
     * +0x63 clear, +0x5A 0x60 against 0x20. Each bound: +0x8C 1 fails and -1
     * passes (signed `jg`), either +0x63 fails, 0x54 fails and 0x55 passes
     * (`jle`), a lead of 0x3C fails and 0x3D passes (`jle`), 0x78 fails and
     * 0x77 passes (`jge`). Side 0 reads its own slot. The demo seed fails. */
    kf_seed(s0, s1, r0, r1);
    CHECK_EQ_INT(fighter_340bc(1u), 0);
    DSB(s0 + 0x63u) = 0;
    DSB(s1 + 0x63u) = 0;
    DSB(s1 + 0x5Au) = 0x60u;
    DSB(s0 + 0x5Au) = 0x20u;
    CHECK_EQ_INT(fighter_340bc(1u), 1);
    CHECK_EQ_INT(fighter_340bc(0u), 0);
    DSW(s1 + 0x8Cu) = 1u;
    CHECK_EQ_INT(fighter_340bc(1u), 0);
    DSW(s1 + 0x8Cu) = 0xFFFFu;
    CHECK_EQ_INT(fighter_340bc(1u), 1);
    DSW(s1 + 0x8Cu) = 0;
    DSB(s1 + 0x63u) = 1u;
    CHECK_EQ_INT(fighter_340bc(1u), 0);
    DSB(s1 + 0x63u) = 0;
    DSB(s0 + 0x63u) = 1u;
    CHECK_EQ_INT(fighter_340bc(1u), 0);
    DSB(s0 + 0x63u) = 0;
    DSB(s0 + 0x5Au) = 0;
    DSB(s1 + 0x5Au) = 0x54u;
    CHECK_EQ_INT(fighter_340bc(1u), 0);
    DSB(s1 + 0x5Au) = 0x55u;
    CHECK_EQ_INT(fighter_340bc(1u), 1);
    DSB(s1 + 0x5Au) = 0x60u;
    DSB(s0 + 0x5Au) = 0x24u;
    CHECK_EQ_INT(fighter_340bc(1u), 0);
    DSB(s0 + 0x5Au) = 0x23u;
    CHECK_EQ_INT(fighter_340bc(1u), 1);
    DSB(s0 + 0x5Au) = 0;
    DSB(s1 + 0x5Au) = 0x78u;
    CHECK_EQ_INT(fighter_340bc(1u), 0);
    DSB(s1 + 0x5Au) = 0x77u;
    CHECK_EQ_INT(fighter_340bc(1u), 1);
    DSB(s0 + 0x5Au) = 0x60u;
    DSB(s1 + 0x5Au) = 0x20u;
    CHECK_EQ_INT(fighter_340bc(0u), 1);
    CHECK_EQ_INT(fighter_340bc(1u), 0);

    /* B: the demo's f = 165 through the dispatcher: the landing stream's
     * `D500 47B8 0003` (opcode 0x15, mode 0x4000) reaches 0x347B8 in mode 3.
     * +0x74 = 0x29A (0x39A10), the record's speeds and +0x42/+0x43 cleared
     * (0x3C148/0x3C16C), state 9/0x0B/0, the slot's +0x43 bits 0..1 cleared,
     * the gate fails so 0x34168 does not run, and the plain 0xD28FC stream
     * starts at hold 3.0, replaced by its 2.0. */
    kf_seed(s0, s1, r0, r1);
    DSW(stream) = 0xD500u;
    DSW(stream + 2u) = 0x47B8u;
    DSW(stream + 4u) = 0x0003u;
    DSW(stream + 6u) = 0x1746u;
    actors_anim_begin(r1, stream, 0x3F800000u);
    CHECK_EQ_INT((int)DSB(s1 + 0x52u), 9);
    CHECK_EQ_INT((int)DSB(s1 + 0x53u), 0x0B);
    CHECK_EQ_INT((int)DSB(s1 + 0x54u), 0);
    CHECK_EQ_INT((int)DSB(s1 + 0x43u), 0xFC);
    CHECK_EQ_INT((int)DSW(s1 + 0x74u), 0x029A);
    CHECK_EQ_INT((int)DSW(s0 + 0x74u), 0x2222);
    CHECK_EQ_INT((int)DSW(r1 + 0x34u), 0);
    CHECK_EQ_INT((int)DSW(r1 + 0x36u), 0);
    CHECK_EQ_INT((int)DSW(r1 + 0x44u), 0);
    CHECK_EQ_INT((int)DSB(r1 + 0x42u), 0);
    CHECK_EQ_INT((int)DSB(r1 + 0x43u), 0);
    CHECK_EQ_INT((int)DSW(r0 + 0x36u), 0x5555);
    CHECK_EQ_INT((int)DSD(r1 + 0x10u), 0x000D1478);
    CHECK((DSD(r1 + 8u) - 0x000D28FCu) < 0x40u,
          "0x347B8 starts the 0x3479C[3] floor stream");
    CHECK_EQ_INT((int)DSD(r1 + 0x24u), 0x40000000);
    CHECK_EQ_INT((int)(DSW(pset1) & 0x7FFFu), 0x18B9);
    CHECK_EQ_INT((int)DSW(s1 + 0x8Cu), 0);
    CHECK_EQ_INT((int)DSD(DS_001077A0 + 4u), 0x22222222);
    CHECK_EQ_INT((int)DSB(DS_001078FF), 0x66);
    CHECK_EQ_INT((int)DSB(s1 + 0x5Du), 0x66);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 0x10);

    /* C: the plain table by character: 0 -> 0xE761A, 1 -> 0xE4290 (the case
     * shares the default's 0x34962), 7 -> the default 0xE4290. */
    kf_seed(s0, s1, r0, r1);
    DSB(s1 + 0x7Au) = 0;
    fighter_347b8(r1);
    CHECK_EQ_INT((int)DSD(r1 + 0x10u), 0x000E5598);
    kf_seed(s0, s1, r0, r1);
    DSB(s1 + 0x7Au) = 1;
    fighter_347b8(r1);
    CHECK_EQ_INT((int)DSD(r1 + 0x10u), 0x000E2528);
    kf_seed(s0, s1, r0, r1);
    DSB(s1 + 0x7Au) = 7;
    fighter_347b8(r1);
    CHECK_EQ_INT((int)DSD(r1 + 0x10u), 0x000E2528);

    /* D: the stun arm. The gate passes, so 0x34168 runs before the 0x34780
     * stream: +0x8C = 0x4B0, the 0xBDBC8[char] actor spawned (EDX = 0x4200
     * for side 1, 0x200 for side 0; EBX = 0xD80 lands in its +0x1C) and
     * stored at DS_001077A0[side], side 1's pset word 0x74 | 0x800 (0x2A17C:
     * the spawn's +0x5F = 1), +0x5D = 0, +0x43 bit 2 cleared on top of
     * 0x347B8's bits 0..1, DS_001078FF = side, DS_00104AE9 bit 0. A
     * three-record scratch pool keeps the spawn off the real pool; its
     * record 0's pset is FIGHT_ACTORS + 0 (the two fighters use 1 and 2). */
    {
        u32 sv_pool = DSD(DS_001014F4);
        u32 sv_free = DSD(DS_00105B3C);
        u32 sv_free4 = DSD(DS_00105B3C + 4u);
        u32 sv_act = DSD(DS_00105BCC);
        u32 sv_act4 = DSD(DS_00105BCC + 4u);
        u32 pool = 0x003F21000u;
        u32 p2 = pool + 0x68u;
        u32 p3 = pool + 0xD0u;
        u32 side;
        for (side = 0; side < 2u; side++) {
            u32 me = side ? s1 : s0, oth = side ? s0 : s1;
            u32 rec = side ? r1 : r0;
            DSD(DS_001014F4) = pool;
            DSD(pool) = p2;
            DSD(pool + 4u) = DS_00105B3C;
            DSD(p2) = p3;
            DSD(p2 + 4u) = pool;
            DSD(p3) = DS_00105B3C;
            DSD(p3 + 4u) = p2;
            DSD(DS_00105B3C) = pool;
            DSD(DS_00105B3C + 4u) = p3;
            DSD(DS_00105BCC) = DS_00105BCC;
            DSD(DS_00105BCC + 4u) = DS_00105BCC;
            DSD(pool + 0x18u) = 0x77777777u;
            DSD(pool + 0x1Cu) = 0x77777777u;
            DSB(pool + 0x49u) = 0x77u;

            kf_seed(s0, s1, r0, r1);
            DSW(FIGHT_ACTORS + 2u) = 0x7777u;
            DSB(s0 + 0x63u) = 0;
            DSB(s1 + 0x63u) = 0;
            DSB(me + 0x5Au) = 0x60u;
            DSB(oth + 0x5Au) = 0x20u;
            fighter_347b8(rec);
            CHECK_EQ_INT((int)DSB(me + 0x52u), 9);
            CHECK_EQ_INT((int)DSB(me + 0x53u), 0x0B);
            CHECK_EQ_INT((int)DSW(me + 0x8Cu), 0x04B0);
            CHECK_EQ_INT((int)DSW(oth + 0x8Cu), 0);
            CHECK_EQ_INT((int)DSD(DS_001077A0 + side * 4u), (int)pool);
            CHECK_EQ_INT((int)DSD(DS_001077A0 + (1u - side) * 4u),
                         side ? 0x11111111 : 0x22222222);
            CHECK_EQ_INT((int)DSD(pool + 0x18u), side ? 0x4200 : 0x200);
            CHECK_EQ_INT((int)DSD(pool + 0x1Cu), 0xD80);
            /* ECX = 0xFF (0x3420D) is the spawn's layer: the descriptor's
             * +0x08 word 0x2200 (read_memory 0xBDB3C/0xBDB78) has bit 13
             * set, so 0x2AE14 stores it in the record's +0x49 byte. */
            CHECK_EQ_INT((int)DSB(pool + 0x49u), 0xFF);
            CHECK_EQ_INT((int)DSB(me + 0x5Du), 0);
            CHECK_EQ_INT((int)DSB(oth + 0x5Du), 0x66);
            CHECK_EQ_INT((int)DSB(me + 0x43u), 0xF8);
            CHECK_EQ_INT((int)DSB(DS_001078FF), (int)side);
            CHECK_EQ_INT((int)DSB(DS_00104AE9), 0x03);
            if (side) {
                CHECK_EQ_INT((int)DSW(FIGHT_ACTORS + 2u), 0x0874);
                CHECK_EQ_INT((int)DSD(r1 + 0x10u), 0x000D1618);
                CHECK((DSD(r1 + 8u) - 0x000D2940u) < 0x40u,
                      "0x347B8 starts the 0x34780[3] stun stream");
            } else {
                CHECK(DSW(FIGHT_ACTORS + 2u) != 0x0874u,
                      "0x34168 runs 0x2A17C for side 1 only");
                CHECK_EQ_INT((int)DSD(r0 + 0x10u), 0x000E57F8);
            }
        }
        DSD(DS_001014F4) = sv_pool;
        DSD(DS_00105B3C) = sv_free;
        DSD(DS_00105B3C + 4u) = sv_free4;
        DSD(DS_00105BCC) = sv_act;
        DSD(DS_00105BCC + 4u) = sv_act4;
    }

    /* E: game mode 7 (0x347E4). With DS_00104B16 = 2 a side other than
     * DS_00104AD4 freezes (0x3480F: hold 0, state 9/3/3, nothing else), the
     * named side gets +0x41 bit 4 and the normal floor; with another value
     * the side freezes when it equals side ^ 1. */
    kf_seed(s0, s1, r0, r1);
    DSW(DS_00104B00) = 7u;
    DSB(DS_00104B16) = 2u;
    DSD(DS_00104AD4) = 0;
    fighter_347b8(r1);
    CHECK_EQ_INT((int)DSD(r1 + 0x24u), 0);
    CHECK_EQ_INT((int)DSB(s1 + 0x52u), 9);
    CHECK_EQ_INT((int)DSB(s1 + 0x53u), 3);
    CHECK_EQ_INT((int)DSB(s1 + 0x54u), 3);
    CHECK_EQ_INT((int)DSW(s1 + 0x74u), 0x1234);
    CHECK_EQ_INT((int)DSW(r1 + 0x36u), 0x5555);
    CHECK_EQ_INT((int)DSD(r1 + 8u), 0x00ABCDEF);
    CHECK_EQ_INT((int)(DSB(s1 + 0x41u) & 0x10u), 0);
    kf_seed(s0, s1, r0, r1);
    DSW(DS_00104B00) = 7u;
    DSB(DS_00104B16) = 2u;
    DSD(DS_00104AD4) = 1u;
    fighter_347b8(r1);
    CHECK_EQ_INT((int)(DSB(s1 + 0x41u) & 0x10u), 0x10);
    CHECK_EQ_INT((int)DSB(s1 + 0x53u), 0x0B);
    CHECK_EQ_INT((int)DSW(s1 + 0x74u), 0x029A);
    kf_seed(s0, s1, r0, r1);
    DSW(DS_00104B00) = 7u;
    DSB(DS_00104B16) = 0u;
    fighter_347b8(r1);
    CHECK_EQ_INT((int)DSB(s1 + 0x53u), 3);
    CHECK_EQ_INT((int)DSD(r1 + 0x24u), 0);
    kf_seed(s0, s1, r0, r1);
    DSW(DS_00104B00) = 7u;
    DSB(DS_00104B16) = 1u;
    fighter_347b8(r1);
    CHECK_EQ_INT((int)DSB(s1 + 0x53u), 0x0B);
    CHECK_EQ_INT((int)(DSB(s1 + 0x41u) & 0x10u), 0);
    kf_seed(s0, s1, r0, r1);
    DSW(DS_00104B00) = 7u;
    DSB(DS_00104B16) = 1u;
    fighter_347b8(r0);
    CHECK_EQ_INT((int)DSB(s0 + 0x53u), 3);
    CHECK_EQ_INT((int)DSD(r0 + 0x24u), 0);

    /* F: the get-up through the dispatcher: the floor stream's `D500 46F8
     * 0003` reaches 0x346F8: +0x76 = word[0xBDBE6] + 1 (read_memory: 03 00,
     * so 4), then 0x36870, which clears +0x74, +0x10 and sets +0x5F = 0xFF;
     * +0x54 = 3 is its empty case. The walk then goes on to the id. */
    kf_seed(s0, s1, r0, r1);
    DSB(s1 + 0x54u) = 3u;
    DSW(s1 + 0x76u) = 0x5555u;
    DSW(s0 + 0x76u) = 0x2222u;
    DSD(s1 + 0x10u) = 0x00039CC8u;
    DSB(s1 + 0x5Fu) = 0x66u;
    DSW(stream) = 0xD500u;
    DSW(stream + 2u) = 0x46F8u;
    DSW(stream + 4u) = 0x0003u;
    DSW(stream + 6u) = 0x1746u;
    actors_anim_begin(r1, stream, 0x3F800000u);
    CHECK_EQ_INT((int)DSW(s1 + 0x76u), 4);
    CHECK_EQ_INT((int)DSW(s1 + 0x74u), 0);
    CHECK_EQ_INT((int)DSD(s1 + 0x10u), 0);
    CHECK_EQ_INT((int)DSB(s1 + 0x5Fu), 0xFF);
    CHECK_EQ_INT((int)(DSW(pset1) & 0x7FFFu), 0x1746);
    CHECK_EQ_INT((int)DSW(s0 + 0x76u), 0x2222);
    DSD(s1 + 0x10u) = 0;
    DSW(s0 + 0x76u) = 0;
    DSW(s1 + 0x76u) = 0;

    tf_put(sv_b00, DS_00104B00, 4u);
    tf_put(sv_7a0, DS_001077A0, 8u);
    tf_put(sv_ad4, DS_00104AD4, 4u);
    DSB(DS_00104B16) = sv_b16;
    DSB(DS_001078FF) = sv_8ff;
    DSB(DS_00104AE9) = sv_ae9;
}

/* ---- roar-timing Task 12: the walk entry 0x35938 (record §22) ----------- */

/* The demo T-rex at f = 201: side 0, char 0, state 0x0E with +0x43 = 0x81
 * (bit 1 clear, so 0x35C1C's default table 0xC8AE0), frame rec+0x52 = 0, step
 * rec+0x58 = 0xFF, hold 2.0 and rec+0x28 = 0x0101 (the PR_T12 trace). Every
 * field 0x35938 writes is a sentinel that differs from its post-condition, and
 * so are +0x53 (0x66) and +0x54 (0x22 for the trace's 0: neither 0 nor the
 * 0x3594B arm's 4, so a stray write of either is seen); the raptor's side 1 is
 * seeded to prove it is not touched. */
static void we_seed(u32 s0, u32 s1, u32 r0, u32 r1)
{
    (void)tf_hit_fixture(0);
    fight_reset_slot_pair(s0, s1, r0, r1);
    DSB(r0 + 0x51u) = 0;
    DSB(r1 + 0x51u) = 1;
    DSW(r0 + 0x56u) = 1;
    DSW(r1 + 0x56u) = 2;
    DSD(r0 + 0x14u) = s0;
    DSD(r1 + 0x14u) = s1;
    DSB(s0 + 0x7Au) = 0;
    DSB(s1 + 0x7Au) = 3;
    DSB(s0 + 0x52u) = 0x0Eu;
    DSB(s0 + 0x53u) = 0x66u;
    DSB(s0 + 0x54u) = 0x22u;
    DSB(s0 + 0x43u) = 0x81u;
    DSB(s1 + 0x52u) = 0x09u;
    DSB(s1 + 0x53u) = 0x0Bu;
    DSB(s1 + 0x54u) = 0x66u;
    DSB(r0 + 0x52u) = 0;
    DSB(r0 + 0x58u) = 0xFFu;
    DSD(r0 + 0x20u) = 0x40000000u;
    DSD(r0 + 0x24u) = 0x40000000u;
    DSW(r0 + 0x28u) = 0x0101u;
    DSD(r0 + 8u) = 0x00ABCDEFu;
    DSD(r1 + 8u) = 0x00ABCDEFu;
    DSD(r1 + 0x24u) = 0x11111111u;
    DSW(FIGHT_ACTORS + 0x20u) = 0x7777u;
    DSW(FIGHT_ACTORS + 0x40u) = 0x7777u;
}

/* §22: 0x35938. The seek tables are 0x35C1C's (read_memory): 0xC8AE0[0] =
 * 0xE6EF8 (words 0x0F80 0x0F81 0x0F82 0x0F83, and 0x0003 at 0xE6EF6, the
 * dword 0x00036870's high word before it), 0xC8A68[0] = 0xE6EA8 (0x0F66) and
 * 0xC8AE0[3] = 0xD2238 (0x1727). */
static void check_walk_entry(void)
{
    u32 s0 = DS_001077B0, s1 = DS_001077B0 + 0x94u;
    u32 r0 = FIGHT_RECS, r1 = FIGHT_RECS + 0x100u;
    u32 pset0 = FIGHT_ACTORS + 0x20u, pset1 = FIGHT_ACTORS + 0x40u;
    u32 stream = FIGHT_RECS + 0x3900u;
    if (fn_resolve(0x35938u) == NULL)
        fn_register(0x35938u, (void (*)(void))fighter_35938);

    /* A: the demo's f = 201, called directly: state 1/0 (+0x54 kept), rec+8
     * the literal id 0x0F80 (0xC8AE0[0] at frame 0) with rec+0x29 bit 3, the
     * seek's +0x28 bits 2/4 cleared then 0x804 set, frame 0, speed and hold
     * 0, step 1. Side 1 is untouched. */
    we_seed(s0, s1, r0, r1);
    DSW(r0 + 0x28u) = 0x0115u;
    fighter_35938(r0);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 1);
    CHECK_EQ_INT((int)DSB(s0 + 0x53u), 0);
    CHECK_EQ_INT((int)DSB(s0 + 0x54u), 0x22);
    CHECK_EQ_INT((int)DSD(r0 + 8u), 0x0F80);
    CHECK_EQ_INT((int)(DSW(pset0) & 0x7FFFu), 0x0F80);
    CHECK_EQ_INT((int)DSW(r0 + 0x28u), 0x0905);
    CHECK_EQ_INT((int)DSB(r0 + 0x52u), 0);
    CHECK_EQ_INT((int)DSD(r0 + 0x20u), 0);
    CHECK_EQ_INT((int)DSD(r0 + 0x24u), 0);
    CHECK_EQ_INT((int)DSB(r0 + 0x58u), 1);
    CHECK_EQ_INT((int)DSB(s1 + 0x52u), 9);
    CHECK_EQ_INT((int)DSB(s1 + 0x53u), 0x0B);
    CHECK_EQ_INT((int)DSD(r1 + 8u), 0x00ABCDEF);
    CHECK_EQ_INT((int)DSW(pset1), 0x7777);

    /* B: the frame index is the record's signed rec+0x52 (0x3597C/0x359A0
     * `sar 0x18`): 3 reads 0x0F83, -1 reads 0xE6EF6 (0x0003), not 0xE6EF8 +
     * 0x1FE; +0x43 bit 1 selects 0xC8A68 (0x0F66); char 3 reads 0xD2238. */
    we_seed(s0, s1, r0, r1);
    DSB(r0 + 0x52u) = 3u;
    fighter_35938(r0);
    CHECK_EQ_INT((int)DSD(r0 + 8u), 0x0F83);
    CHECK_EQ_INT((int)DSB(r0 + 0x52u), 0);
    we_seed(s0, s1, r0, r1);
    DSB(r0 + 0x52u) = 0xFFu;
    fighter_35938(r0);
    CHECK_EQ_INT((int)DSD(r0 + 8u), 0x0003);
    we_seed(s0, s1, r0, r1);
    DSB(s0 + 0x43u) = 0x83u;
    fighter_35938(r0);
    CHECK_EQ_INT((int)DSD(r0 + 8u), 0x0F66);
    we_seed(s0, s1, r0, r1);
    DSB(s1 + 0x43u) = 0x81u;
    fighter_35938(r1);
    CHECK_EQ_INT((int)DSD(r1 + 8u), 0x1727);
    CHECK_EQ_INT((int)(DSW(pset1) & 0x7FFFu), 0x1727);
    CHECK_EQ_INT((int)DSB(s1 + 0x52u), 1);
    CHECK_EQ_INT((int)DSB(s1 + 0x53u), 0);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 0x0E);
    CHECK_EQ_INT((int)DSD(r0 + 8u), 0x00ABCDEF);

    /* C: +0x54 == 4 (0x3594B) takes state 8 and keeps +0x53; the rest of the
     * entry runs. */
    we_seed(s0, s1, r0, r1);
    DSB(s0 + 0x54u) = 4u;
    fighter_35938(r0);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 8);
    CHECK_EQ_INT((int)DSB(s0 + 0x53u), 0x66);
    CHECK_EQ_INT((int)DSB(s0 + 0x54u), 4);
    CHECK_EQ_INT((int)DSD(r0 + 8u), 0x0F80);
    CHECK_EQ_INT((int)DSB(r0 + 0x58u), 1);

    /* D: no owner slot (rec+0x14 = 0, 0x35942) writes nothing. */
    we_seed(s0, s1, r0, r1);
    DSD(r0 + 0x14u) = 0;
    fighter_35938(r0);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 0x0E);
    CHECK_EQ_INT((int)DSB(s0 + 0x53u), 0x66);
    CHECK_EQ_INT((int)DSD(r0 + 8u), 0x00ABCDEF);
    CHECK_EQ_INT((int)DSW(r0 + 0x28u), 0x0101);
    CHECK_EQ_INT((int)DSD(r0 + 0x20u), 0x40000000);
    CHECK_EQ_INT((int)DSB(r0 + 0x58u), 0xFF);
    CHECK_EQ_INT((int)DSW(pset0), 0x7777);

    /* E: through the dispatcher: `D500 5938 0003` (opcode 0x15, mode 0x4000)
     * walked by 0x2BC30, which stores its 1.0 hold first and, on the opcode's
     * status 2, adds 2 to rec+8 (0x2BCC0) before reading the id, so the
     * literal id it loads is 0x0F82. An unregistered target would leave
     * state 0x0E and the 1.0 hold. */
    we_seed(s0, s1, r0, r1);
    DSW(stream) = 0xD500u;
    DSW(stream + 2u) = 0x5938u;
    DSW(stream + 4u) = 0x0003u;
    DSW(stream + 6u) = 0x1746u;
    actors_anim_begin(r0, stream, 0x3F800000u);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 1);
    CHECK_EQ_INT((int)DSB(s0 + 0x53u), 0);
    CHECK_EQ_INT((int)DSD(r0 + 0x24u), 0);
    CHECK_EQ_INT((int)DSD(r0 + 0x20u), 0);
    CHECK_EQ_INT((int)DSD(r0 + 8u), 0x0F82);
    CHECK_EQ_INT((int)(DSW(pset0) & 0x7FFFu), 0x0F82);

    DSD(r0 + 0x14u) = 0;
    DSD(r1 + 0x14u) = 0;
}

/* ---- roar-timing Task 13: the worshipper arrival target 0x4AC18 (§23) ---- */

/* The demo's left-edge worshipper at f = 206 (the PR_T13 trace): its actor
 * record's +0x14 is its fight-effect entry (0x49617 stores it), +0x48 is 0x20
 * and the entry is in type 8 (a cheer). Every field 0x4AC18/0x4AC38 writes is a
 * sentinel that differs from its post-condition. A second actor record (`oth`,
 * pset 4, +0x48 0x22) is seeded so a part can point the entry at it. The
 * 0xC9544 table (and the dwords at 0xC9540, 0xC9744 and 0xC9344 that the
 * index-width parts read) holds crafted literal-id streams. */
static void wa_seed(u32 entry, u32 rec, u32 oth)
{
    mem_fill(FIGHT_RECS + 0x3000u, 0, 0x400u);
    DSD(DS_001014EC) = FIGHT_ACTORS;
    DSD(entry + 8u) = rec;
    DSB(entry + 0x1Eu) = 8u;
    DSD(rec + 0x14u) = entry;
    DSB(oth + 0x1Eu) = 0;
    DSD(oth + 0x14u) = 0;
    DSB(rec + 0x48u) = 0x20u;
    DSB(oth + 0x48u) = 0x22u;
    DSW(rec + 0x56u) = 3u;
    DSW(oth + 0x56u) = 4u;
    DSD(rec + 8u) = 0x00ABCDEFu;
    DSD(oth + 8u) = 0x00ABCDEFu;
    DSD(rec + 0x24u) = 0x40400000u;
    DSD(oth + 0x24u) = 0x40400000u;
    DSW(rec + 0x34u) = 0x1111u;
    DSW(rec + 0x36u) = 0x2222u;
    DSW(rec + 0x38u) = 0x3333u;
    DSW(oth + 0x34u) = 0x1111u;
    DSW(oth + 0x36u) = 0x2222u;
    DSW(oth + 0x38u) = 0x3333u;
    DSB(rec + 0x29u) = 0x4Bu;
    DSB(oth + 0x29u) = 0x4Bu;
    DSW(FIGHT_ACTORS + 3u * 0x20u) = 0x7777u;
    DSW(FIGHT_ACTORS + 4u * 0x20u) = 0x7777u;
}

/* §23: 0x4AC18 (EAX = rec) loads EDX = rec+0x14 (0x4AC1A), returns when it is
 * 0 (0x4AC1F), else calls 0x4AC38(EAX = entry, EDX = (u32)(u8)rec+0x48 -
 * 0x20) (0x4AC21..0x4AC2D). 0x4AC38 acts on entry+8, not on rec, and indexes
 * `[ebx*4 + 0xc9544]` with the full 32-bit EDX. */
static void check_worshipper_arrival(void)
{
    u32 entry = FIGHT_RECS + 0x3000u;
    u32 rec = FIGHT_RECS + 0x3100u;
    u32 oth = FIGHT_RECS + 0x3200u;
    u32 st = FIGHT_RECS + 0x3600u;           /* the crafted streams, 0x10 apart */
    u32 pset3 = FIGHT_ACTORS + 3u * 0x20u, pset4 = FIGHT_ACTORS + 4u * 0x20u;
    u32 sv_tab[4], sv_m1 = DSD(0x000C9540u), sv_80 = DSD(0x000C9744u);
    u32 sv_s80 = DSD(0x000C9344u), sv_14ec = DSD(DS_001014EC), i;
    for (i = 0; i < 4u; i++) sv_tab[i] = DSD(0x000C9544u + i * 4u);
    if (fn_resolve(0x4AC18u) == NULL)
        fn_register(0x4AC18u, (void (*)(void))fight_4ac18);

    for (i = 0; i < 4u; i++) {
        DSW(st + i * 0x10u) = (u16)(0x0120u + i);        /* literal ids */
        DSW(st + i * 0x10u + 2u) = (u16)(0x0220u + i);
        DSD(0x000C9544u + i * 4u) = st + i * 0x10u;
    }
    DSW(st + 0x40u) = 0x0666u;
    DSD(0x000C9540u) = st + 0x40u;           /* index -1 */
    DSW(st + 0x50u) = 0x0680u;
    DSD(0x000C9744u) = st + 0x50u;           /* index 0x80 */
    DSW(st + 0x60u) = 0x0F80u;
    DSD(0x000C9344u) = st + 0x60u;           /* index -0x80 (sign-extended) */

    /* A: the demo's f = 206, called directly: index 0; the actor stops, loses
     * hflip, gains +0x29 bit 4 (0x2BC30 also clears bit 3: 0x4B -> 0x13), the
     * entry returns to type 0 and the actor begins 0xC9544[0] at the hold
     * 5.0. `oth` is untouched. */
    wa_seed(entry, rec, oth);
    fight_4ac18(rec);
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 0);
    CHECK_EQ_INT((int)DSW(rec + 0x34u), 0);
    CHECK_EQ_INT((int)DSW(rec + 0x36u), 0);
    CHECK_EQ_INT((int)DSW(rec + 0x38u), 0);
    CHECK_EQ_INT((int)DSB(rec + 0x29u), 0x13);
    CHECK_EQ_INT((int)DSD(rec + 8u), (int)st);
    CHECK_EQ_INT((int)DSD(rec + 0x24u), 0x40A00000);
    CHECK_EQ_INT((int)DSW(pset3), 0x0120);
    CHECK_EQ_INT((int)DSD(rec + 0x14u), (int)entry);
    CHECK_EQ_INT((int)DSD(entry + 8u), (int)rec);
    CHECK_EQ_INT((int)DSD(oth + 8u), 0x00ABCDEF);
    CHECK_EQ_INT((int)DSW(oth + 0x34u), 0x1111);
    CHECK_EQ_INT((int)DSW(pset4), 0x7777);

    /* B: the index is rec+0x48 - 0x20: 0x22 begins 0xC9544[2]. */
    wa_seed(entry, rec, oth);
    DSB(rec + 0x48u) = 0x22u;
    fight_4ac18(rec);
    CHECK_EQ_INT((int)DSD(rec + 8u), (int)(st + 0x20u));
    CHECK_EQ_INT((int)DSW(pset3), 0x0122);

    /* C: 0x4AC38 acts on entry+8 (here `oth`), with the index from EAX's
     * (rec's) +0x48 = 0x21, not oth's 0x22; rec itself is untouched. */
    wa_seed(entry, rec, oth);
    DSB(rec + 0x48u) = 0x21u;
    DSD(entry + 8u) = oth;
    fight_4ac18(rec);
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 0);
    CHECK_EQ_INT((int)DSD(oth + 8u), (int)(st + 0x10u));
    CHECK_EQ_INT((int)DSW(pset4), 0x0121);
    CHECK_EQ_INT((int)DSD(oth + 0x24u), 0x40A00000);
    CHECK_EQ_INT((int)DSW(oth + 0x34u), 0);
    CHECK_EQ_INT((int)DSW(oth + 0x36u), 0);
    CHECK_EQ_INT((int)DSW(oth + 0x38u), 0);
    CHECK_EQ_INT((int)DSB(oth + 0x29u), 0x13);
    CHECK_EQ_INT((int)DSD(rec + 8u), 0x00ABCDEF);
    CHECK_EQ_INT((int)DSD(rec + 0x24u), 0x40400000);
    CHECK_EQ_INT((int)DSW(rec + 0x34u), 0x1111);
    CHECK_EQ_INT((int)DSB(rec + 0x29u), 0x4B);
    CHECK_EQ_INT((int)DSW(pset3), 0x7777);

    /* D: the index width. +0x48 = 0x1F gives EDX = 0xFFFFFFFF, which reads
     * 0xC9540 (a u16 index would read 0x40000 bytes on); +0x48 = 0xA0 is
     * zero-extended (0x4AC23 `xor edx,edx`, 0x4AC25 `mov dl`) to 0x80 and
     * reads 0xC9744, not the sign-extended 0xC9344. */
    wa_seed(entry, rec, oth);
    DSB(rec + 0x48u) = 0x1Fu;
    fight_4ac18(rec);
    CHECK_EQ_INT((int)DSD(rec + 8u), (int)(st + 0x40u));
    CHECK_EQ_INT((int)DSW(pset3), 0x0666);
    wa_seed(entry, rec, oth);
    DSB(rec + 0x48u) = 0xA0u;
    fight_4ac18(rec);
    CHECK_EQ_INT((int)DSD(rec + 8u), (int)(st + 0x50u));
    CHECK_EQ_INT((int)DSW(pset3), 0x0680);

    /* E: no entry (rec+0x14 = 0, 0x4AC1F) writes nothing. Without the test
     * 0x4AC38 would run on entry 0: its +0x1E byte (linear 0x1E, seeded 0x5A
     * here and restored) and the begin's rec+8 store into actor DSD(8). Entry
     * 0's +0x1C (linear 0x1C) is seeded too, so no part of the body reads an
     * unseeded byte; all three are restored. */
    wa_seed(entry, rec, oth);
    DSD(rec + 0x14u) = 0;
    {
        u8 sv_1e = DSB(0x1Eu), sv_1c = DSB(0x1Cu);
        u32 sv_8 = DSD(8u);
        DSB(0x1Eu) = 0x5Au;
        DSB(0x1Cu) = 0;
        DSD(8u) = 0;
        fight_4ac18(rec);
        CHECK_EQ_INT((int)DSB(0x1Eu), 0x5A);
        CHECK_EQ_INT((int)DSD(8u), 0);
        DSB(0x1Eu) = sv_1e;
        DSB(0x1Cu) = sv_1c;
        DSD(8u) = sv_8;
    }
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 8);
    CHECK_EQ_INT((int)DSD(rec + 8u), 0x00ABCDEF);
    CHECK_EQ_INT((int)DSD(rec + 0x24u), 0x40400000);
    CHECK_EQ_INT((int)DSW(rec + 0x34u), 0x1111);
    CHECK_EQ_INT((int)DSW(rec + 0x36u), 0x2222);
    CHECK_EQ_INT((int)DSW(rec + 0x38u), 0x3333);
    CHECK_EQ_INT((int)DSB(rec + 0x29u), 0x4B);
    CHECK_EQ_INT((int)DSW(pset3), 0x7777);

    /* F: through the dispatcher: `D500 AC18 0004` (0xEE09C's words; opcode
     * 0x15, mode 0x4000) walked by 0x2BC30 with the hold 1.0. The target's
     * nested 0x2BC30 begins 0xC9544[0] at 5.0 and loads 0x0120; the outer
     * 0x2BC30 then takes the status-2 arm (0x2BCC0 `add [ecx+8],2`) and loads
     * the next word, 0x0220. An unregistered target would leave type 8, the
     * 1.0 hold and the id 0x1746 after the dword. */
    wa_seed(entry, rec, oth);
    DSW(st + 0x80u) = 0xD500u;
    DSW(st + 0x82u) = 0xAC18u;
    DSW(st + 0x84u) = 0x0004u;
    DSW(st + 0x86u) = 0x1746u;
    actors_anim_begin(rec, st + 0x80u, 0x3F800000u);
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 0);
    CHECK_EQ_INT((int)DSD(rec + 0x24u), 0x40A00000);
    CHECK_EQ_INT((int)DSD(rec + 8u), (int)(st + 2u));
    CHECK_EQ_INT((int)DSW(pset3), 0x0220);
    CHECK_EQ_INT((int)DSW(rec + 0x34u), 0);

    for (i = 0; i < 4u; i++) DSD(0x000C9544u + i * 4u) = sv_tab[i];
    DSD(0x000C9540u) = sv_m1;
    DSD(0x000C9744u) = sv_80;
    DSD(0x000C9344u) = sv_s80;
    DSD(DS_001014EC) = sv_14ec;
}

/* ---- roar-timing Task 21: the worshipper landing target 0x4AC80 (§31) ---- */

/* The demo's f = 820 worshipper (the PR_T21 trace): entry 0x108438, index 4,
 * +0x1C = 0x80, type 8, side (+0x21) 1, mode 3, DS_001088C5 = 0. wa_seed's
 * records plus: the entry's slot (+0xC) and its fighter record (+0x28 word),
 * +0x1C = 0xD0 (bits 7, 6 and 4: the 0x3F mask keeps only bit 4), +0x21 = 1,
 * the actor's +0x55 = 0x5A, a DS_00108868 base record (pset 5) and a
 * DS_00108864 held entry whose written fields are sentinels, and crafted
 * literal-id streams in the four tables (C955C 0x03xx, C958C 0x04xx, C95D4
 * 0x05xx, C95EC 0x06xx; index = +0x48 - 0x20). */
#define WL_BASE   (FIGHT_RECS + 0x3300u)
#define WL_FREC   (FIGHT_RECS + 0x3340u)
#define WL_HELD   (FIGHT_RECS + 0x3380u)
#define WL_SLOT   (FIGHT_RECS + 0x33C0u)
static void wl_seed(u32 entry, u32 rec, u32 oth)
{
    wa_seed(entry, rec, oth);
    DSB(entry + 0x1Eu) = 2u;
    DSB(entry + 0x1Cu) = 0xD0u;
    DSB(entry + 0x21u) = 1u;
    DSD(entry + 0x14u) = 0x00C0FFEEu;
    DSD(entry + 0xCu) = WL_SLOT;
    DSD(WL_SLOT) = WL_FREC;
    DSW(WL_FREC + 0x28u) = 0x4000u;
    DSB(rec + 0x55u) = 0x5Au;
    DSB(oth + 0x55u) = 0x5Au;
    DSW(WL_BASE + 0x56u) = 5u;
    DSD(FIGHT_ACTORS + 5u * 0x20u + 4u) = 0x10000u;
    DSD(FIGHT_ACTORS + 3u * 0x20u + 4u) = 0x10000u - 0x1741u;
    DSW(WL_HELD + 0x18u) = 0x1234u;
    DSB(WL_HELD + 0x1Cu) = 0xFFu;
    DSB(WL_HELD + 0x1Eu) = 6u;
    DSB(DS_001088C5) = 0;
    DSD(DS_00108868) = WL_BASE;
    DSD(DS_00108864) = WL_HELD;
    DSW(DS_00104B00) = 3u;
    DSW(DS_00104B00 + 2u) = 0x1234u;
    DSB(DS_00104B16) = 0;
}

/* §31: 0x4AC80 (EAX = rec) loads the entry from rec+0x14 (0x4AC8B) and
 * returns when it is 0; the index is the 32-bit (u8)+0x48 - 0x20; the entry's
 * actor (+8) loses bit 6 and gains bit 4 of +0x29 (0x4ACA0/0x4ACA7). Then:
 * +0x1C bit 5 clear, the climb (0xC95EC at 3.0 on the actor; +0x38 = 0x40 and
 * +0x34 = -0x40/+0x40 by the fighter record's +0x28 bit 0x4000 on EAX's rec;
 * type 5; +0x1C &= 0x3F) or, in modes 8/9/0x17, the stop and 0x4B3F0 (side ==
 * DS_00104B16) / 0x4B430 with EBX = 1; bit 5 set with DS_001088C5 = 0, the
 * DS_00108864 release; bit 5 set with DS_001088C5 != 0, the walk to 0x1740
 * beside the DS_00108868 record (0xC95D4, type 1) or the hold (0xC958C, type
 * 8, +0x55 = 1) when no band matches. */
static void check_worshipper_landing(void)
{
    u32 entry = FIGHT_RECS + 0x3000u;
    u32 rec = FIGHT_RECS + 0x3100u;
    u32 oth = FIGHT_RECS + 0x3200u;
    u32 st = FIGHT_RECS + 0x3800u;           /* the crafted streams, 0x10 apart */
    u32 pset3 = FIGHT_ACTORS + 3u * 0x20u, pset4 = FIGHT_ACTORS + 4u * 0x20u;
    static const u32 tabs[4] = { 0x000C955Cu, 0x000C958Cu, 0x000C95D4u, 0x000C95ECu };
    u8 sv_tab[0x200], sv_4b00[4], sv_c5 = DSB(DS_001088C5), sv_b16 = DSB(DS_00104B16);
    u8 sv_pset5[0x20];
    u32 sv_68 = DSD(DS_00108868), sv_64 = DSD(DS_00108864);
    u32 sv_14ec = DSD(DS_001014EC), t, i;
    memcpy(sv_tab, mem + 0x000C9540u, sizeof sv_tab);
    memcpy(sv_4b00, mem + DS_00104B00, sizeof sv_4b00);
    memcpy(sv_pset5, mem + FIGHT_ACTORS + 5u * 0x20u, sizeof sv_pset5);
    for (t = 0; t < 4u; t++)
        for (i = 0; i < 6u; i++) {
            u32 sp = st + (t * 8u + i) * 0x10u;
            DSW(sp) = (u16)(0x0300u + t * 0x100u + i);     /* literal ids */
            DSW(sp + 2u) = (u16)(0x0B00u + t * 0x100u + i);
            DSD(tabs[t] + i * 4u) = sp;
        }

    /* A: the demo's f = 820 climb, index 4 (+0x48 = 0x24): the actor takes
     * 0xC95EC[4] at 3.0, +0x38 = 0x40, +0x34 = -0x40 (fighter +0x28 bit
     * 0x4000), +0x36 kept, +0x29 0x4B -> 0x13 (bit 3 by 0x2BC30), type 5, +0x1C
     * 0xD0 -> 0x10. The entry's +0x14, the actor's +0x55, the held entry and
     * DS_00108864 are untouched. */
    wl_seed(entry, rec, oth);
    DSB(rec + 0x48u) = 0x24u;
    fight_4ac80(rec);
    CHECK_EQ_INT((int)DSD(rec + 8u), (int)(st + (3u * 8u + 4u) * 0x10u));
    CHECK_EQ_INT((int)DSW(pset3), 0x0604);
    CHECK_EQ_INT((int)DSD(rec + 0x24u), 0x40400000);
    CHECK_EQ_INT((int)DSW(rec + 0x38u), 0x40);
    CHECK_EQ_INT((int)DSW(rec + 0x34u), 0xFFC0);
    CHECK_EQ_INT((int)DSW(rec + 0x36u), 0x2222);
    CHECK_EQ_INT((int)DSB(rec + 0x29u), 0x13);
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 5);
    CHECK_EQ_INT((int)DSB(entry + 0x1Cu), 0x10);
    CHECK_EQ_INT((int)DSD(entry + 0x14u), 0x00C0FFEE);
    CHECK_EQ_INT((int)DSB(rec + 0x55u), 0x5A);
    CHECK_EQ_INT((int)DSW(WL_HELD + 0x18u), 0x1234);
    CHECK_EQ_INT((int)DSB(WL_HELD + 0x1Eu), 6);
    CHECK_EQ_INT((int)DSD(DS_00108864), (int)WL_HELD);

    /* A2: fighter +0x28 = 0xBFFF (every bit but 0x4000): +0x34 = 0x40;
     * +0x48 = 0x1F gives index 0xFFFFFFFF, which reads 0xC95E8 = 0xC95D4[5]. */
    wl_seed(entry, rec, oth);
    DSW(WL_FREC + 0x28u) = 0xBFFFu;
    DSB(rec + 0x48u) = 0x1Fu;
    fight_4ac80(rec);
    CHECK_EQ_INT((int)DSW(rec + 0x34u), 0x40);
    CHECK_EQ_INT((int)DSW(pset3), 0x0505);

    /* A3: with entry+8 = oth the stream, +0x29 and type go to oth while the
     * +0x38/+0x34 stores go to EAX's rec (0x4AE6B `[ebx+0x38]`); the index is
     * rec's +0x48 (0x21), not oth's 0x22. */
    wl_seed(entry, rec, oth);
    DSD(entry + 8u) = oth;
    DSB(rec + 0x48u) = 0x21u;
    fight_4ac80(rec);
    CHECK_EQ_INT((int)DSW(pset4), 0x0601);
    CHECK_EQ_INT((int)DSD(oth + 0x24u), 0x40400000);
    CHECK_EQ_INT((int)DSB(oth + 0x29u), 0x13);
    CHECK_EQ_INT((int)DSW(oth + 0x34u), 0x1111);
    CHECK_EQ_INT((int)DSW(oth + 0x38u), 0x3333);
    CHECK_EQ_INT((int)DSW(rec + 0x34u), 0xFFC0);
    CHECK_EQ_INT((int)DSW(rec + 0x38u), 0x40);
    CHECK_EQ_INT((int)DSB(rec + 0x29u), 0x4B);
    CHECK_EQ_INT((int)DSW(pset3), 0x7777);

    /* B: modes 8, 9 and 0x17 stop the actor (+0x38/+0x34/+0x36 = 0) and hold
     * it: side 1 != DS_00104B16 0 takes 0x4B430 (0xC958C), side equal takes
     * 0x4B3F0 (0xC955C), both with +0x55 = 1 and type 8 at 3.0; the climb's
     * stores do not happen. The word DS_00104B02 = 0x1234 proves the mode is a
     * word. Modes 7 and 0x18 climb. */
    {
        static const u16 modes[5] = { 8u, 9u, 0x17u, 7u, 0x18u };
        for (i = 0; i < 5u; i++) {
            wl_seed(entry, rec, oth);
            DSB(rec + 0x48u) = 0x22u;
            DSW(DS_00104B00) = modes[i];
            fight_4ac80(rec);
            if (i < 3u) {
                CHECK_EQ_INT((int)DSW(pset3), 0x0402);
                CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 8);
                CHECK_EQ_INT((int)DSB(rec + 0x55u), 1);
                CHECK_EQ_INT((int)DSW(rec + 0x34u), 0);
                CHECK_EQ_INT((int)DSW(rec + 0x36u), 0);
                CHECK_EQ_INT((int)DSW(rec + 0x38u), 0);
                CHECK_EQ_INT((int)DSB(entry + 0x1Cu), 0xD0);
            } else {
                CHECK_EQ_INT((int)DSW(pset3), 0x0602);
                CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 5);
            }
        }
    }
    wl_seed(entry, rec, oth);
    DSB(rec + 0x48u) = 0x22u;
    DSW(DS_00104B00) = 9u;
    DSB(DS_00104B16) = 1u;
    fight_4ac80(rec);
    CHECK_EQ_INT((int)DSW(pset3), 0x0302);
    CHECK_EQ_INT((int)DSB(rec + 0x55u), 1);
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 8);
    CHECK_EQ_INT((int)DSD(rec + 0x24u), 0x40400000);

    /* B2: an empty hold stream (0xC958C[2] = 0): 0x4B430 returns 0, so only
     * the stop happens; the type, +0x55 and the stream stay. */
    wl_seed(entry, rec, oth);
    DSB(rec + 0x48u) = 0x22u;
    DSW(DS_00104B00) = 0x17u;
    DSD(0x000C958Cu + 2u * 4u) = 0;
    fight_4ac80(rec);
    CHECK_EQ_INT((int)DSW(pset3), 0x7777);
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 2);
    CHECK_EQ_INT((int)DSB(rec + 0x55u), 0x5A);
    CHECK_EQ_INT((int)DSW(rec + 0x36u), 0);
    DSD(0x000C958Cu + 2u * 4u) = st + (1u * 8u + 2u) * 0x10u;

    /* C: +0x1C bit 5 with DS_001088C5 = 0 releases the DS_00108864 entry
     * (+0x18 word 0, +0x1C &= 0xDF, type 4) and zeroes DS_00108864; the own
     * entry keeps its type, +0x1C and streams; only +0x29 changes (0x4B ->
     * 0x1B, no 0x2BC30). */
    wl_seed(entry, rec, oth);
    DSB(entry + 0x1Cu) = 0xF0u;
    fight_4ac80(rec);
    CHECK_EQ_INT((int)DSW(WL_HELD + 0x18u), 0);
    CHECK_EQ_INT((int)DSB(WL_HELD + 0x1Cu), 0xDF);
    CHECK_EQ_INT((int)DSB(WL_HELD + 0x1Eu), 4);
    CHECK_EQ_INT((int)DSD(DS_00108864), 0);
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 2);
    CHECK_EQ_INT((int)DSB(entry + 0x1Cu), 0xF0);
    CHECK_EQ_INT((int)DSB(rec + 0x29u), 0x1B);
    CHECK_EQ_INT((int)DSW(pset3), 0x7777);
    CHECK_EQ_INT((int)DSW(rec + 0x34u), 0x1111);
    CHECK_EQ_INT((int)DSW(rec + 0x38u), 0x3333);

    /* D: bit 5 with DS_001088C5 != 0, base 0x10000 (pset 5), index 0: the
     * bands. Walks set +0x14, type 1, the 0xC95D4 stream at 3.0 and +0x36 = 0,
     * +0x34 = 0x40 toward a larger target (bit 6 clear) else -0x40 with bit 6
     * set (0x2BC30 then marks the pset id with bit 15); no band holds
     * (0xC958C, type 8, +0x55 = 1, +0x14 kept). */
    {
        static const s32 dpos[12] = { -0x1741, -0x1740, -0xBA0, -0xB9F, -1, 0,
                                       1, 0xB9F, 0xBA0, 0x1740, 0x1741, -0x7000 };
        static const s32 dtgt[12] = { -0x1740, 0, 0, -0x1740, -0x1740, 0,
                                       0x1740, 0x1740, 0, 0, 0x1740, -0x1740 };
        for (i = 0; i < 12u; i++) {
            s32 pos = 0x10000 + dpos[i];
            wl_seed(entry, rec, oth);
            DSB(entry + 0x1Cu) = 0x20u;
            DSB(DS_001088C5) = 1u;
            DSD(FIGHT_ACTORS + 3u * 0x20u + 4u) = (u32)pos;
            fight_4ac80(rec);
            CHECK_EQ_INT((int)DSW(rec + 0x36u), 0);
            CHECK_EQ_INT((int)DSD(rec + 0x24u), 0x40400000);
            CHECK_EQ_INT((int)DSB(entry + 0x1Cu), 0x20);
            CHECK_EQ_INT((int)DSB(WL_HELD + 0x1Eu), 6);
            if (dtgt[i] != 0) {
                s32 tg = 0x10000 + dtgt[i];
                CHECK_EQ_INT((int)DSD(entry + 0x14u), (int)tg);
                CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 1);
                CHECK_EQ_INT((int)DSW(pset3), pos < tg ? 0x0500 : 0x8500);
                CHECK_EQ_INT((int)DSB(rec + 0x55u), 0x5A);
                CHECK_EQ_INT((int)DSW(rec + 0x34u), pos < tg ? 0x40 : 0xFFC0);
                CHECK_EQ_INT((int)DSB(rec + 0x29u), pos < tg ? 0x13 : 0x53);
            } else {
                CHECK_EQ_INT((int)DSD(entry + 0x14u), 0x00C0FFEE);
                CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 8);
                CHECK_EQ_INT((int)DSW(pset3), 0x0400);
                CHECK_EQ_INT((int)DSB(rec + 0x55u), 1);
                CHECK_EQ_INT((int)DSW(rec + 0x34u), 0x1111);
            }
        }
    }

    /* D2: signed bands: base 0x2000 and pos -0x100 (below base - 0x1740 only
     * as signed, 0x4ACFA `jge`) walks to 0x8C0, toward the larger target (the
     * 0x4AD4F `jge` signed too); base 0x1740 and pos -5 computes the target 0
     * and so holds (0x4AD3B `test eax,eax`). */
    wl_seed(entry, rec, oth);
    DSB(entry + 0x1Cu) = 0x20u;
    DSB(DS_001088C5) = 0x80u;
    DSD(FIGHT_ACTORS + 5u * 0x20u + 4u) = 0x2000u;
    DSD(FIGHT_ACTORS + 3u * 0x20u + 4u) = (u32)-0x100;
    fight_4ac80(rec);
    CHECK_EQ_INT((int)DSD(entry + 0x14u), 0x8C0);
    CHECK_EQ_INT((int)DSW(rec + 0x34u), 0x40);
    CHECK_EQ_INT((int)DSB(rec + 0x29u), 0x13);
    wl_seed(entry, rec, oth);
    DSB(entry + 0x1Cu) = 0x20u;
    DSB(DS_001088C5) = 1u;
    DSD(FIGHT_ACTORS + 5u * 0x20u + 4u) = 0x1740u;
    DSD(FIGHT_ACTORS + 3u * 0x20u + 4u) = (u32)-5;
    fight_4ac80(rec);
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 8);
    CHECK_EQ_INT((int)DSW(pset3), 0x0400);

    /* D3: the walk acts on entry+8 (oth, pset 4 at the base - 0x1741), with
     * rec's index 0x21; rec is untouched. */
    wl_seed(entry, rec, oth);
    DSB(entry + 0x1Cu) = 0x20u;
    DSB(DS_001088C5) = 1u;
    DSD(entry + 8u) = oth;
    DSB(rec + 0x48u) = 0x21u;
    DSD(FIGHT_ACTORS + 4u * 0x20u + 4u) = 0x10000u + 0xB9Fu;
    fight_4ac80(rec);
    CHECK_EQ_INT((int)DSD(entry + 0x14u), 0x11740);
    CHECK_EQ_INT((int)DSW(pset4), 0x0501);
    CHECK_EQ_INT((int)DSW(oth + 0x34u), 0x40);
    CHECK_EQ_INT((int)DSW(oth + 0x36u), 0);
    CHECK_EQ_INT((int)DSW(rec + 0x34u), 0x1111);
    CHECK_EQ_INT((int)DSW(rec + 0x36u), 0x2222);
    CHECK_EQ_INT((int)DSB(rec + 0x29u), 0x4B);
    CHECK_EQ_INT((int)DSW(pset3), 0x7777);

    /* E: no entry (rec+0x14 = 0, 0x4AC90) writes nothing. Without the test
     * the body would run on entry 0: its +0x1E byte (linear 0x1E, seeded 0x5A
     * here) and its actor DSD(8) (seeded 0). Its +0x1C (linear 0x1C) is
     * seeded 0, bit 5 clear, so that body takes the climb or the mode hold,
     * both of which write +0x1E; all three are restored. */
    wl_seed(entry, rec, oth);
    DSD(rec + 0x14u) = 0;
    {
        u8 sv_1e = DSB(0x1Eu), sv_1c = DSB(0x1Cu);
        u32 sv_8 = DSD(8u);
        DSB(0x1Eu) = 0x5Au;
        DSB(0x1Cu) = 0;
        DSD(8u) = 0;
        fight_4ac80(rec);
        CHECK_EQ_INT((int)DSB(0x1Eu), 0x5A);
        DSB(0x1Eu) = sv_1e;
        DSB(0x1Cu) = sv_1c;
        DSD(8u) = sv_8;
    }
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 2);
    CHECK_EQ_INT((int)DSB(rec + 0x29u), 0x4B);
    CHECK_EQ_INT((int)DSW(pset3), 0x7777);
    CHECK_EQ_INT((int)DSW(rec + 0x34u), 0x1111);

    /* F: through the dispatcher: `D500 AC80 0004` (0xEE3BA's words; opcode
     * 0x15, mode 0x4000) walked by 0x2BC30 with the hold 1.0. The target's
     * nested 0x2BC30 begins 0xC95EC[0] at 3.0 (id 0x0600); the outer one then
     * loads the next word, 0x0B00 + 3 * 0x100. An unregistered target would
     * leave type 2, the 1.0 hold and the id 0x1746 after the dword. */
    wl_seed(entry, rec, oth);
    DSW(st + 0x300u) = 0xD500u;
    DSW(st + 0x302u) = 0xAC80u;
    DSW(st + 0x304u) = 0x0004u;
    DSW(st + 0x306u) = 0x1746u;
    actors_anim_begin(rec, st + 0x300u, 0x3F800000u);
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 5);
    CHECK_EQ_INT((int)DSD(rec + 0x24u), 0x40400000);
    CHECK_EQ_INT((int)DSW(pset3), 0x0E00);
    CHECK_EQ_INT((int)DSW(rec + 0x38u), 0x40);

    memcpy(mem + 0x000C9540u, sv_tab, sizeof sv_tab);
    memcpy(mem + DS_00104B00, sv_4b00, sizeof sv_4b00);
    memcpy(mem + FIGHT_ACTORS + 5u * 0x20u, sv_pset5, sizeof sv_pset5);
    DSB(DS_001088C5) = sv_c5;
    DSB(DS_00104B16) = sv_b16;
    DSD(DS_00108868) = sv_68;
    DSD(DS_00108864) = sv_64;
    DSD(DS_001014EC) = sv_14ec;
}

/* ---- roar-timing Task 15: the T-rex's reaction-0x20 breath (record §25) --- */

/* The demo's f = 559 T-rex (the PR_T15 trace): side 0, char 0, state 9/0/0,
 * slot +0x08 = 0. Every field 0x3D17C writes is a sentinel that differs from
 * its post-condition; side 1's slot and record are seeded the same way. The
 * words 0x1080AC/0x1080AE hold sentinels. */
static void tb_seed(u32 s0, u32 s1, u32 r0, u32 r1)
{
    u32 s[2], r[2], i;
    (void)tf_hit_fixture(0);
    fight_reset_slot_pair(s0, s1, r0, r1);
    s[0] = s0; s[1] = s1; r[0] = r0; r[1] = r1;
    for (i = 0; i < 2u; i++) {
        DSB(r[i] + 0x51u) = (u8)i;
        DSW(r[i] + 0x56u) = (u16)(1u + i);
        DSD(r[i] + 8u) = 0x00ABCDEFu;
        DSD(r[i] + 0x1Cu) = 0x5555u;
        DSD(r[i] + 0x24u) = 0x11111111u;
        DSW(FIGHT_ACTORS + (1u + i) * 0x20u) = 0x0F35u;
        DSB(s[i] + 0x52u) = 9u;
        DSB(s[i] + 0x53u) = 0x66u;
        DSB(s[i] + 0x54u) = 0x66u;
        DSD(s[i] + 0x08u) = 0;
        DSD(s[i] + 0x0Cu) = 0x11111111u;
        DSD(s[i] + 0x18u) = 0x22222222u;
        DSD(s[i] + 0x1Cu) = 0x33333333u;
        DSB(s[i] + 0x5Fu) = 0x3Cu;
        DSB(s[i] + 0x64u) = 0x77u;
    }
    DSW(0x001080ACu) = 0x5555u;
    DSW(0x001080AEu) = 0x6666u;
}

/* The count of records on the active list. */
static u32 tb_active(void)
{
    u32 n = 0, r;
    for (r = actor_list_head(); r != 0; r = actor_next(r)) n++;
    return n;
}

/* §25: 0x34E2C's (char 0, reaction 0x20) entry 0xA37A8 reads `7c d1 03 00 00
 * 00 00 00`: the callback 0x3D17C and no stream. 0x3D17C (EAX = slot, EDX =
 * rec) returns when the slot's +0x08 (the live projectile) is set; otherwise
 * it starts the 0xE84C8 stream (EDX from 0x3D198, preserved by the voice
 * call) at hold 3.0 through 0x3C4CC, sets state 0xB/6/0, clears +0x0C/+0x18/
 * +0x1C, moves +0x5F to +0x64 and stores 0x100 in word 0x1080AC[rec+0x51].
 * The stream's `D100 D214 0003` reaches 0x3D214 (the emitter 0xBB27C), whose
 * stream 0xE8598's `D100 D26C 0003` reaches 0x3D26C (the projectile 0xBB268
 * into the slot's +0x08). The registrations are asserted in
 * check_anim_hold_scaler; they are made here only when missing. */
static void check_trex_breath(void)
{
    u32 s0 = DS_001077B0, s1 = DS_001077B0 + 0x94u;
    u32 r0 = FIGHT_RECS, r1 = FIGHT_RECS + 0x100u;
    u32 pset1 = FIGHT_ACTORS + 0x20u, pset2 = FIGHT_ACTORS + 0x40u;
    u32 em = FIGHT_RECS + 0x3800u;
    u32 sv_14ec = DSD(DS_001014EC);
    u16 sv_ac = DSW(0x001080ACu), sv_ae = DSW(0x001080AEu);
    u16 sv_78f6 = DSW(DS_001078F6);
    u8 sv_b00[4];
    tf_snap(sv_b00, DS_00104B00, 4u);
    if (fn_resolve(0x3D17Cu) == NULL)
        fn_register(0x3D17Cu, (void (*)(void))fighter_3d17c);

    /* A: the demo's f = 559 through 0x34E2C (0x35045). +0x52 = 9 sends
     * 0x3C4CC to 0x3C480, whose 0x188AC zeroes rec+0x1C. The stream's first
     * word is the literal id 0x1131, so the record stays at 0xE84C8 with the
     * hold 3.0. 0x34E2C stored +0x5F = 0x20 (0x35042) before the call. */
    tb_seed(s0, s1, r0, r1);
    hit_reaction_apply(0u, 0x20u);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 0x0B);
    CHECK_EQ_INT((int)DSB(s0 + 0x53u), 6);
    CHECK_EQ_INT((int)DSB(s0 + 0x54u), 0);
    CHECK_EQ_INT((int)DSD(s0 + 0x0Cu), 0);
    CHECK_EQ_INT((int)DSD(s0 + 0x18u), 0);
    CHECK_EQ_INT((int)DSD(s0 + 0x1Cu), 0);
    CHECK_EQ_INT((int)DSB(s0 + 0x5Fu), 0xFF);
    CHECK_EQ_INT((int)DSB(s0 + 0x64u), 0x20);
    CHECK_EQ_INT((int)DSW(0x001080ACu), 0x0100);
    CHECK_EQ_INT((int)DSW(0x001080AEu), 0x6666);
    CHECK_EQ_INT((int)DSD(r0 + 8u), 0x000E84C8);
    CHECK_EQ_INT((int)DSD(r0 + 0x24u), 0x40400000);
    CHECK_EQ_INT((int)DSD(r0 + 0x1Cu), 0);
    CHECK_EQ_INT((int)(DSW(pset1) & 0x7FFFu), 0x1131);
    CHECK_EQ_INT((int)DSB(s1 + 0x52u), 9);
    CHECK_EQ_INT((int)DSB(s1 + 0x64u), 0x77);
    CHECK_EQ_INT((int)DSD(r1 + 8u), 0x00ABCDEF);

    /* B: a live projectile (slot +0x08 != 0, 0x3D187) writes nothing. */
    tb_seed(s0, s1, r0, r1);
    DSD(s0 + 0x08u) = 0x00ABCDEFu;
    fighter_3d17c(s0, r0, 0u);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 9);
    CHECK_EQ_INT((int)DSB(s0 + 0x53u), 0x66);
    CHECK_EQ_INT((int)DSB(s0 + 0x54u), 0x66);
    CHECK_EQ_INT((int)DSD(s0 + 0x0Cu), 0x11111111);
    CHECK_EQ_INT((int)DSD(s0 + 0x18u), 0x22222222);
    CHECK_EQ_INT((int)DSD(s0 + 0x1Cu), 0x33333333);
    CHECK_EQ_INT((int)DSB(s0 + 0x5Fu), 0x3C);
    CHECK_EQ_INT((int)DSB(s0 + 0x64u), 0x77);
    CHECK_EQ_INT((int)DSW(0x001080ACu), 0x5555);
    CHECK_EQ_INT((int)DSD(r0 + 8u), 0x00ABCDEF);
    CHECK_EQ_INT((int)DSD(r0 + 0x24u), 0x11111111);

    /* C: the word index is rec+0x51 (0x3D183), not the EBX side: side 1's
     * record with EBX = 0 writes 0x1080AE. +0x52 = 0 takes 0x3C4CC's plain
     * 0x2BC30 arm (rec+0x1C kept). +0x64 takes the old +0x5F, 0x3C. */
    tb_seed(s0, s1, r0, r1);
    DSB(s1 + 0x52u) = 0u;
    fighter_3d17c(s1, r1, 0u);
    CHECK_EQ_INT((int)DSW(0x001080AEu), 0x0100);
    CHECK_EQ_INT((int)DSW(0x001080ACu), 0x5555);
    CHECK_EQ_INT((int)DSB(s1 + 0x52u), 0x0B);
    CHECK_EQ_INT((int)DSB(s1 + 0x53u), 6);
    CHECK_EQ_INT((int)DSB(s1 + 0x54u), 0);
    CHECK_EQ_INT((int)DSB(s1 + 0x5Fu), 0xFF);
    CHECK_EQ_INT((int)DSB(s1 + 0x64u), 0x3C);
    CHECK_EQ_INT((int)DSD(r1 + 8u), 0x000E84C8);
    CHECK_EQ_INT((int)DSD(r1 + 0x1Cu), 0x5555);
    CHECK_EQ_INT((int)(DSW(pset2) & 0x7FFFu), 0x1131);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 9);
    CHECK_EQ_INT((int)DSB(s0 + 0x64u), 0x77);

    /* D: 0x3D214 (EAX = rec, a pool record here) spawns the emitter 0xBB27C
     * (stream 0xE8598, type 0) as the record's child: a5 = rec+0x56 | 0x400,
     * so +0x4A is the parent's index and +0x28 has 0x400. The emitter gets the
     * slot in +0x14 and +0x60 = 1, and its index goes into the record's +0x4B.
     * (0x3D214's +0x59 = 2 is not asserted: the spawn walk's `9302` at
     * 0xE8598, opcode 0x13, already stores 2.) The walk then stops on `CD40
     * 03E8` (opcode 0x0D), leaving rec+8 at the operand word 0xE859C. Side 0
     * keeps the descriptor's +0x2E = 8 and +0x4E = 0; side 1 adds 4 and sets
     * +0x4E. The record's own +0x60 is seeded so that pointing +0x4B at the
     * record fails. */
    tb_seed(s0, s1, r0, r1);
    actors_reset();
    {
        u32 f[2], e, i, n;
        for (i = 0; i < 2u; i++) {
            f[i] = actor_alloc(0);
            DSW(f[i] + 0x56u) = (u16)actor_index(f[i]);
            DSD(f[i] + 0x14u) = i == 0u ? s0 : s1;
            DSB(f[i] + 0x51u) = (u8)i;
            DSB(f[i] + 0x4Bu) = 0x77u;
            DSB(f[i] + 0x60u) = 0x66u;
        }
        /* Every free record's +0x48 gets a sentinel, so the emitter's +0x48
         * = 0 below is the descriptor's type byte, not the pool's zero. */
        for (i = 0; i < ACTOR_POOL_RECORDS; i++) {
            u32 fr = actor_record(i);
            if (fr != 0u && fr != f[0] && fr != f[1]) DSB(fr + 0x48u) = 0x5Au;
        }
        n = tb_active();
        fighter_3d214(f[0]);
        CHECK_EQ_INT((int)tb_active(), (int)(n + 1u));
        e = actor_record((u32)DSB(f[0] + 0x4Bu));
        CHECK(e != 0u && e != f[0] && e != f[1],
              "0x3D214 stores the new emitter's index in rec+0x4B");
        if (e != 0u) {
            CHECK_EQ_INT((int)DSD(e + 0x14u), (int)s0);
            CHECK_EQ_INT((int)DSB(e + 0x60u), 1);
            CHECK_EQ_INT((int)DSB(e + 0x4Eu), 0);
            CHECK_EQ_INT((int)DSW(e + 0x2Eu), 0x0008);
            CHECK_EQ_INT((int)DSD(e + 8u), 0x000E859C);
            CHECK_EQ_INT((int)DSB(e + 0x48u), 0);
            CHECK_EQ_INT((int)DSB(e + 0x4Au), (int)(actor_index(f[0]) & 0x7Fu));
            CHECK_EQ_INT((int)(DSW(e + 0x28u) & 0x0400u), 0x0400);
        }
        CHECK_EQ_INT((int)DSB(f[0] + 0x60u), 0x66);
        fighter_3d214(f[1]);
        e = actor_record((u32)DSB(f[1] + 0x4Bu));
        CHECK(e != 0u && e != f[0] && e != f[1],
              "0x3D214 (side 1) stores the new emitter's index in rec+0x4B");
        if (e != 0u) {
            CHECK_EQ_INT((int)DSD(e + 0x14u), (int)s1);
            CHECK_EQ_INT((int)DSB(e + 0x4Eu), 1);
            CHECK_EQ_INT((int)DSW(e + 0x2Eu), 0x000C);
            CHECK_EQ_INT((int)DSB(e + 0x4Au), (int)(actor_index(f[1]) & 0x7Fu));
        }
        /* No slot (rec+0x14 = 0, 0x3D220): no spawn, +0x4B kept. */
        DSD(f[0] + 0x14u) = 0;
        DSB(f[0] + 0x4Bu) = 0x77u;
        n = tb_active();
        fighter_3d214(f[0]);
        CHECK_EQ_INT((int)tb_active(), (int)n);
        CHECK_EQ_INT((int)DSB(f[0] + 0x4Bu), 0x77);

        /* E: through the dispatcher: `D100 D214 0003 0000` (0xE84CA's words;
         * opcode 0x11, mode 0x4000) walked by 0x2BC30 spawns the emitter. An
         * unregistered target leaves +0x4B at 0x77 and the list unchanged. */
        {
            u32 st = FIGHT_RECS + 0x3900u;
            DSW(st) = 0xD100u;
            DSW(st + 2u) = 0xD214u;
            DSW(st + 4u) = 0x0003u;
            DSW(st + 6u) = 0x0000u;
            DSW(st + 8u) = 0x1131u;
            DSD(f[0] + 0x14u) = s0;
            n = tb_active();
            actors_anim_begin(f[0], st, 0x3F800000u);
            CHECK_EQ_INT((int)tb_active(), (int)(n + 1u));
            e = actor_record((u32)DSB(f[0] + 0x4Bu));
            CHECK(e != 0u && DSB(f[0] + 0x4Bu) != 0x77u,
                  "the dispatched 0x3D214 spawns the emitter");
            if (e != 0u) CHECK_EQ_INT((int)DSB(e + 0x60u), 1);
        }
    }

    /* F: 0x3D26C (EAX = the emitter, only its +0x14 is read) spawns the
     * projectile 0xBB268 (stream 0xE85BC, type 2; its walk stops on `CD40
     * 03F0`'s operand word 0xE85C0) into slot +0x08 beside the
     * slot's record. Side 0, pset unflipped (0x1A570 = 1): x = 28250 - 0x1800,
     * y = 0x40 + 0x1600, the speed -word[0x1080AC] = -0x100; a3 = +0x30 >> 16
     * = 5 goes to +0x32 (the descriptor's +0x28 bit 13 is clear); +0x28 bit 14
     * gives a5 = 0x4000, which 0x2AE14 ORs into the record's +0x28 (0x80 |
     * 0x4000). */
    tb_seed(s0, s1, r0, r1);
    actors_reset();
    DSW(0x001080ACu) = 0x0100u;
    DSW(0x001080AEu) = 0x0234u;
    DSD(em + 0x14u) = s0;
    DSD(r0 + 0x18u) = 28250u;
    DSD(r0 + 0x1Cu) = 0x40u;
    DSD(r0 + 0x30u) = 0x00050000u;
    DSW(r0 + 0x28u) = 0x4000u;
    DSD(s0 + 0x08u) = 0x00ABCDEFu;
    fighter_3d26c(em);
    {
        u32 p = DSD(s0 + 0x08u);
        CHECK(p != 0x00ABCDEFu && actor_index(p) != 0xFFFFFFFFu,
              "0x3D26C stores the projectile in slot+0x08");
        if (actor_index(p) != 0xFFFFFFFFu) {
            CHECK_EQ_INT((int)DSD(p + 0x18u), 28250 - 0x1800);
            CHECK_EQ_INT((int)DSD(p + 0x1Cu), 0x1640);
            CHECK_EQ_INT((int)DSW(p + 0x34u), 0xFF00);
            CHECK_EQ_INT((int)DSD(p + 0x14u), (int)s0);
            CHECK_EQ_INT((int)DSW(p + 0x32u), 5);
            CHECK_EQ_INT((int)DSW(p + 0x28u), 0x4080);
            CHECK_EQ_INT((int)DSB(p + 0x48u), 2);
            CHECK_EQ_INT((int)DSW(p + 0x2Eu), 0x0008);
            CHECK_EQ_INT((int)DSB(p + 0x4Eu), 0);
            CHECK_EQ_INT((int)DSD(p + 8u), 0x000E85C0);
        }
    }
    /* Side 0, pset hflipped (0x1A570 = 0): x + 0x1800, speed +0x100; +0x28
     * without bit 14 gives a5 = 0 (+0x28 = 0x80). */
    DSW(pset1) = (u16)(DSW(pset1) | 0x8000u);
    DSW(r0 + 0x28u) = 0;
    DSD(s0 + 0x08u) = 0x00ABCDEFu;
    fighter_3d26c(em);
    {
        u32 p = DSD(s0 + 0x08u);
        CHECK(p != 0x00ABCDEFu && actor_index(p) != 0xFFFFFFFFu,
              "0x3D26C (hflipped) stores the projectile in slot+0x08");
        if (actor_index(p) != 0xFFFFFFFFu) {
            CHECK_EQ_INT((int)DSD(p + 0x18u), 28250 + 0x1800);
            CHECK_EQ_INT((int)DSW(p + 0x34u), 0x0100);
            CHECK_EQ_INT((int)DSW(p + 0x28u), 0x0080);
        }
    }
    /* Side 1 (rec+0x51 = 1), unflipped: the speed is -word[0x1080AE] =
     * -0x234, and the projectile's +0x2E gets 4 more and +0x4E is set. */
    DSD(em + 0x14u) = s1;
    DSD(r1 + 0x18u) = 9000u;
    DSD(r1 + 0x1Cu) = 0u;
    DSD(s1 + 0x08u) = 0x00ABCDEFu;
    fighter_3d26c(em);
    {
        u32 p = DSD(s1 + 0x08u);
        CHECK(p != 0x00ABCDEFu && actor_index(p) != 0xFFFFFFFFu,
              "0x3D26C (side 1) stores the projectile in slot+0x08");
        if (actor_index(p) != 0xFFFFFFFFu) {
            CHECK_EQ_INT((int)DSD(p + 0x18u), 9000 - 0x1800);
            CHECK_EQ_INT((int)DSW(p + 0x34u), 0xFDCC);
            CHECK_EQ_INT((int)DSD(p + 0x14u), (int)s1);
            CHECK_EQ_INT((int)DSW(p + 0x2Eu), 0x000C);
            CHECK_EQ_INT((int)DSB(p + 0x4Eu), 1);
        }
    }
    /* No slot (the emitter's +0x14 = 0, 0x3D27A): nothing is spawned. */
    DSD(em + 0x14u) = 0;
    DSD(s0 + 0x08u) = 0x00ABCDEFu;
    {
        u32 n = tb_active();
        fighter_3d26c(em);
        CHECK_EQ_INT((int)tb_active(), (int)n);
        CHECK_EQ_INT((int)DSD(s0 + 0x08u), 0x00ABCDEF);
    }

    actors_reset();
    DSD(em + 0x14u) = 0;
    DSD(DS_001014EC) = sv_14ec;
    DSW(0x001080ACu) = sv_ac;
    DSW(0x001080AEu) = sv_ae;
    DSW(DS_001078F6) = sv_78f6;
    tf_put(sv_b00, DS_00104B00, 4u);
}

/* ---- Task 3b: the 0x3B714 reaction applier (pose/freeze record §2.3) ----- */

/* §2.3: the reaction gates 0x39EFC/0x3B038/0x3B6C4 and the seeds 0x3B080/
 * 0x3AE9C, unit-tested directly. All seeds are sentinels that differ from the
 * post-conditions. */
static void check_reaction_predicates(void)
{
    u32 s0 = DS_001077B0;
    u32 s1 = DS_001077B0 + 0x94u;
    u32 r0 = FIGHT_RECS;
    u32 r1 = FIGHT_RECS + 0x100u;
    u32 sv_b018 = DSD(DS_000BE018);
    u32 sv_bdee = DSD(DS_000BEDEE);
    u8  sv_bdf2 = DSB(DS_000BEDF2);

    fight_reset_recs();
    fight_reset_actors();
    mem_fill(s0, 0, 0x94u);
    mem_fill(s1, 0, 0x94u);
    fight_reset_slot_pair(s0, s1, r0, r1);
    DSB(r0 + 0x51u) = 0;
    DSB(r1 + 0x51u) = 1;

    /* 0x39EFC: the 0x39CC8 pose triple. */
    DSB(s1 + 0x53u) = 0x0A;
    DSD(s1 + 0x10u) = 0x00039CC8u;
    DSB(s1 + 0x58u) = 4u;
    CHECK_EQ_INT(fighter_39efc(1u), 1);
    DSB(s1 + 0x58u) = 5u;
    CHECK_EQ_INT(fighter_39efc(1u), 0);
    DSB(s1 + 0x58u) = 4u;
    DSD(s1 + 0x10u) = 0x00039CC9u;
    CHECK_EQ_INT(fighter_39efc(1u), 0);
    DSD(s1 + 0x10u) = 0x00039CC8u;
    DSB(s1 + 0x53u) = 0x0Bu;
    CHECK_EQ_INT(fighter_39efc(1u), 0);

    /* 0x3B038: the ±(BEDEE>>16 - BE018) window about BE018. */
    DSD(DS_000BE018) = 0x1000;
    DSD(DS_000BEDEE) = 0x0100u << 16;        /* k = 0x100 */
    DSD(s0 + 0x2Cu) = 0x1000;                /* base - k = 0xF00 <= 0x1000 */
    CHECK_EQ_INT(fighter_3b038(0u), 1);
    DSD(s0 + 0x2Cu) = 0x800;                 /* neither arm holds */
    CHECK_EQ_INT(fighter_3b038(0u), 0);
    DSD(s0 + 0x2Cu) = 0xFFFFE000u;           /* k - base = -0xF00 >= -0x2000 */
    CHECK_EQ_INT(fighter_3b038(0u), 1);

    /* 0x3B6C4: the two mid-stance bytes and the actor bit-15 agreement. */
    DSW(r0 + 0x56u) = 1;                     /* side 0's actor */
    DSW(r1 + 0x56u) = 2;                     /* side 1's actor */
    DSW(FIGHT_ACTORS + 1u * 0x20u) = 0;
    DSW(FIGHT_ACTORS + 2u * 0x20u) = 0;
    DSB(s0 + 0x53u) = 8u;
    DSB(s0 + 0x54u) = 2u;
    DSB(s1 + 0x54u) = 0u;
    CHECK_EQ_INT(fighter_3b6c4(0u), 1);
    DSB(s1 + 0x54u) = 2u;                    /* other+0x54 == 2 -> 0 (0x3B6EA) */
    CHECK_EQ_INT(fighter_3b6c4(0u), 0);
    DSB(s1 + 0x54u) = 0u;
    DSW(FIGHT_ACTORS + 2u * 0x20u) = 0x8000u;/* bit 15 disagrees -> 0 */
    CHECK_EQ_INT(fighter_3b6c4(0u), 0);
    DSW(FIGHT_ACTORS + 2u * 0x20u) = 0;
    DSB(s0 + 0x53u) = 7u;
    CHECK_EQ_INT(fighter_3b6c4(0u), 0);

    /* 0x3B080: the ±(param_2*2) seed and the 0x3B038-gated mirror. */
    DSB(DS_000BEDF2) = 0;
    DSB(s0 + 0x53u) = 0;
    DSW(FIGHT_ACTORS + 2u * 0x20u) = 0;      /* 1-side bit 15 clear -> negate */

    /* The 0x3B038(side) arm re-zeroes the primary side (0x3B0F8) and mirrors. */
    DSD(s0 + 0x2Cu) = 0x1000;                /* 0x3B038(0) = 1 */
    DSW(r0 + 0x34u) = 0x1111;
    DSW(r1 + 0x34u) = 0x2222;
    fighter_3b080(0u, 0x10u, 0x77u, 1u);
    CHECK_EQ_INT((int)DSW(r0 + 0x34u), 0);         /* re-zeroed at 0x3B0F8 */
    CHECK_EQ_INT((int)DSB(r0 + 0x43u), 0);
    CHECK_EQ_INT((int)DSW(r1 + 0x34u), 0x0020);    /* -v */
    CHECK_EQ_INT((int)DSB(r1 + 0x43u), 0x77);

    /* 0x3B038(side) == 0 leaves the primary write in place. */
    DSD(s0 + 0x2Cu) = 0x800;                 /* 0x3B038(0) = 0 */
    DSW(r1 + 0x34u) = 0x2222;
    fighter_3b080(0u, 0x10u, 0x55u, 1u);
    CHECK_EQ_INT((int)DSW(r0 + 0x34u), 0xFFE0);    /* -0x20 */
    CHECK_EQ_INT((int)DSB(r0 + 0x43u), 0x55);
    CHECK_EQ_INT((int)DSW(r1 + 0x34u), 0x2222);    /* untouched */

    /* param_4 == 0 also skips the mirror. */
    DSD(s0 + 0x2Cu) = 0x1000;
    DSW(r1 + 0x34u) = 0x2222;
    fighter_3b080(0u, 0x10u, 0x66u, 0u);
    CHECK_EQ_INT((int)DSW(r0 + 0x34u), 0xFFE0);
    CHECK_EQ_INT((int)DSB(r0 + 0x43u), 0x66);
    CHECK_EQ_INT((int)DSW(r1 + 0x34u), 0x2222);

    /* 1-side bit 15 set flips the sign. */
    DSW(FIGHT_ACTORS + 2u * 0x20u) = 0x8000u;
    fighter_3b080(0u, 0x10u, 0x77u, 0u);
    CHECK_EQ_INT((int)DSW(r0 + 0x34u), 0x0020);

    /* the 0xBEDF2 gate returns before any write. */
    DSB(DS_000BEDF2) = 1;
    fighter_3b080(0u, 0x10u, 0x11u, 1u);
    CHECK_EQ_INT((int)DSB(r0 + 0x43u), 0x77);

    /* 0x3AE9C: the landing seed and the sign application. */
    DSB(DS_000BEDF2) = 0;
    DSW(r0 + 0x56u) = 1;
    DSW(FIGHT_ACTORS + 1u * 0x20u) = 0;      /* bit 15 clear -> the sign-0 negate */
    DSB(s0 + 0x7Au) = 1;                     /* char 1 */
    DSB(s0 + 0x53u) = 0;                     /* != 7 */
    DSB(s0 + 0x40u) = 0;                     /* facing bit clear */
    DSW(r0 + 0x36u) = 0;                     /* g = 0 */
    DSW(r0 + 0x34u) = 0;                     /* sign = 0 */
    DSD(r0 + 0x42u) = 0;                     /* k = 0 < 0x1A */
    fighter_3ae9c(0u, 0u);
    CHECK_EQ_INT((int)DSW(r0 + 0x36u), (int)DSW(DS_000BEDDC + 2u));  /* ch 1 */
    CHECK_EQ_INT((int)DSW(r0 + 0x44u), 0x1A);
    CHECK_EQ_INT((int)DSW(r0 + 0x34u), 0xFF9C);                 /* -0x64 */
    DSW(r0 + 0x34u) = 0x1234;                /* the +0x53 == 7 guard */
    DSW(r0 + 0x36u) = 0x5678;
    DSB(s0 + 0x53u) = 7u;
    fighter_3ae9c(0u, 0u);
    CHECK_EQ_INT((int)DSW(r0 + 0x34u), 0x1234);
    CHECK_EQ_INT((int)DSW(r0 + 0x36u), 0x5678);
    DSB(s0 + 0x53u) = 0;
    DSW(r0 + 0x36u) = 1;                     /* g != 0 -> rec+0x34 = 0x64 */
    DSW(r0 + 0x34u) = 3;                     /* sign = 1 */
    fighter_3ae9c(0u, 0u);
    CHECK_EQ_INT((int)DSW(r0 + 0x34u), 0x64);
    DSW(r0 + 0x36u) = 1;
    DSW(r0 + 0x34u) = 0xFFFB;                /* -5, sign = -1 */
    fighter_3ae9c(0u, 0u);
    CHECK_EQ_INT((int)DSW(r0 + 0x34u), 0xFF9C);   /* -(0x64) */
    DSW(r0 + 0x36u) = 0;
    DSW(r0 + 0x34u) = 0;
    DSB(s0 + 0x40u) = 0x80u;                 /* facing bit -> 0x28 vs 0x3C */
    fighter_3ae9c(0u, 1u);
    CHECK_EQ_INT((int)DSW(r0 + 0x44u), 0x28);
    DSW(r0 + 0x36u) = 0;
    fighter_3ae9c(0u, 0u);
    CHECK_EQ_INT((int)DSW(r0 + 0x44u), 0x3C);

    DSD(DS_000BE018) = sv_b018;
    DSD(DS_000BEDEE) = sv_bdee;
    DSB(DS_000BEDF2) = sv_bdf2;
}

/* §8.2: the applier's own writes and the 0x3AAFC pose handoff. The 0x3B298
 * dispatch is forced to 0 by seeding the reaction descriptor's bit pair, so the
 * 0x3AAFC arm runs; the 0x3B080 seed is gated off by 0xBEDF2. */
static void check_reaction(void)
{
    u32 s0 = DS_001077B0;
    u32 s1 = DS_001077B0 + 0x94u;
    u32 r0 = FIGHT_RECS;
    u32 r1 = FIGHT_RECS + 0x100u;
    u8  sv_bdf2 = DSB(DS_000BEDF2);
    u16 sv_w0 = DSW(0x000A6728u);
    u16 sv_w2 = DSW(0x000A6728u + 2u);
    u32 sv_d8 = DSD(0x000A3528u + 8u);
    u8  sv_b11a = DSB(0x000DE11Au);

    pose_chain_setup(s0, s1, r0, r1);

    DSD(DS_00107D50 + 4u) = 0;               /* the 0x3C59C bit for 1-side */
    DSB(s0 + 0x5Fu) = 0;                     /* local_24 = 0 */
    DSB(s1 + 0x5Fu) = 0;
    DSB(s0 + 0x53u) = 0;                     /* 0x39EFC(1) = 0 */
    DSD(s1 + 0x10u) = 0;
    DSB(s1 + 0x58u) = 0;
    DSB(s1 + 0x54u) = 0;                     /* 0x3B080 runs but is gated off */
    DSB(s0 + 0x52u) = 0;                     /* skip 0x3AE9C */
    DSB(s1 + 0x52u) = 0;
    DSB(s1 + 0x65u) = 0xFFu;                 /* sentinel */
    DSB(s1 + 0x41u) = 0;                     /* sentinel */
    DSB(r1 + 0x4Bu) = 0;                     /* skip 0x2BD44 */
    DSB(s1 + 0x63u) = 0;                     /* 0x3B298's fight_command_map early-out */
    DSB(DS_000BEDF2) = 1;                    /* 0x3B080 no-op */
    DSW(0x000A6728u + 2u) = 3;               /* bits 0+1 -> 0x3B298 returns 0; ecx = 3 */
    DSD(r1 + 0x24u) = 0xDEADBEEFu;           /* the pose setter clears it */

    fighter_reaction(s1, s0);
    CHECK_EQ_INT((int)DSB(s1 + 0x65u), 0);          /* = byte[s0+0x5F] */
    CHECK_EQ_INT((int)(DSB(s1 + 0x41u) & 0x80u), 0x80);
    CHECK_EQ_INT((int)DSB(s1 + 0x52u), 0x10);       /* the 0x3A504 pose */
    CHECK_EQ_INT((int)DSB(s1 + 0x53u), 0x0A);
    CHECK_EQ_INT((int)DSB(s1 + 0x54u), 0);
    CHECK_EQ_INT((int)DSD(s1 + 0x10u), 0x0003A43C);
    CHECK_EQ_INT((int)DSD(r1 + 0x24u), 0);          /* the pose setter's write */
    CHECK_EQ_INT((int)DSW(s0 + 0x6Cu), 1);          /* other +0x6C++ */

    /* the 0x3C59C frame gate: the bit is now set, so the next call returns. */
    DSB(s1 + 0x65u) = 0xAAu;
    fighter_reaction(s1, s0);
    CHECK_EQ_INT((int)DSB(s1 + 0x65u), 0xAA);

    /* the local_24 == 0xFF path returns before the 0x62003 stub. */
    DSD(DS_00107D50 + 4u) = 0;
    DSB(s0 + 0x5Fu) = 0xFFu;
    fighter_reaction(s1, s0);
    CHECK_EQ_INT((int)DSB(s1 + 0x65u), 0xAA);

    /* +0x52 == 4 seeds rec+0x34 and +0x4E to 0. */
    DSD(DS_00107D50 + 4u) = 0;
    DSB(s0 + 0x5Fu) = 0;
    DSB(s1 + 0x52u) = 4u;
    DSW(r1 + 0x34u) = 0x1234;
    DSW(s1 + 0x4Eu) = 0x5678;
    fighter_reaction(s1, s0);
    CHECK_EQ_INT((int)DSW(r1 + 0x34u), 0);
    CHECK_EQ_INT((int)DSW(s1 + 0x4Eu), 0);

    DSB(DS_000BEDF2) = sv_bdf2;
    DSW(0x000A6728u) = sv_w0;
    DSW(0x000A6728u + 2u) = sv_w2;
    DSD(0x000A3528u + 8u) = sv_d8;
    DSB(0x000DE11Au) = sv_b11a;
}

/* ---- Task 3c: the 0x193B0 winner body (pose/freeze record §2.2) ---------- */

/* Seed the two slots/records for the winner body's reaction path. Both arms of
 * check_winner_body use this: the 0x3962C/0x396AC gates are held off (the
 * animation descriptor's bit 4 clear, the 0x107D2C word 0) so the +0x84 compare
 * runs and the winner's reaction dispatches through 0x3B714 -> 0x3AAFC. */
static void winner_body_setup(u32 s0, u32 s1, u32 r0, u32 r1)
{
    pose_chain_setup(s0, s1, r0, r1);

    DSD(DS_00107D50) = 0;                    /* the 0x3C59C bits for both sides */
    DSD(DS_00107D50 + 4u) = 0;
    DSB(s0 + 0x5Fu) = 0;                     /* b = 0 for 0x3962C/0x396AC */
    DSB(s1 + 0x5Fu) = 0;
    DSB(s1 + 0x53u) = 0;                     /* 0x39EFC(1) = 0 */
    DSD(s1 + 0x10u) = 0;
    DSB(s1 + 0x58u) = 0;
    DSB(s1 + 0x54u) = 0;                     /* 0x3B080 runs but is gated off */
    DSB(s0 + 0x52u) = 0;                     /* skip 0x3AE9C */
    DSB(s1 + 0x52u) = 0;
    DSB(s0 + 0x63u) = 0;                     /* 0x3B298's fight_command_map early-out */
    DSB(s1 + 0x63u) = 0;
    DSB(r1 + 0x4Bu) = 0;                     /* skip 0x2BD44 */
    DSB(DS_000BEDF2) = 1;                    /* 0x3B080 no-op */
    DSW(0x000A6728u + 2u) = 3;               /* bits 0+1 -> 0x3B298 returns 0 */
    DSB(0x00107A80u) = 0;                    /* 0x396AC's per-side counter */
    DSB(0x00107A80u + 0x40u) = 0;
    DSD(s0 + 0x1Cu) = 0;                     /* the 0x3B714 reaction arm */
    DSD(s1 + 0x14u) = 0;                     /* skip the +0x14 callback */
    DSD(s1 + 0x18u) = 0;
    DSD(s1 + 0x1Cu) = 0;
    DSD(s1 + 0x0Cu) = 0;
    DSB(s0 + 0x43u) = 0xFFu;                 /* the 0xFC mask sentinel */
    DSB(s1 + 0x43u) = 0xFFu;
    DSB(s0 + 0x8Au) = 0xEEu;                 /* cleared by 0x193B0 */
    DSD(r1 + 0x24u) = 0xDEADBEEFu;           /* the pose setter clears it */
    DSW(s0 + 0x84u) = 0x1234u;               /* sentinel, differs from 0x100B50 */
    DSW(s1 + 0x84u) = 0x5678u;
    DSW(DS_00100B50) = 0;                    /* sentinels, differ from +0x84 */
    DSW(DS_00100B50 + 2u) = 0;
    DSB(DS_00100B5A) = 0xEEu;                /* the 0x19164 sentinels */
    DSB(DS_00100B5C) = 0xEEu;
    DSB(DS_00100B58) = 0xEEu;
    DSB(DS_00100B5A + 1u) = 0xEEu;
    DSB(DS_00100B5C + 1u) = 0xEEu;
    DSB(DS_00100B58 + 1u) = 0xEEu;
    DSD(0x001088ECu) = 0xEEEEu;              /* the 0x192DC/0x39278 sentinel */
}

/* §8.2: the winner body's +0x84 count latch and the pose handoff through
 * 0x3B714 -> 0x3AAFC -> the 0x3A504 setter, plus the 0x19164/0x192DC side
 * effects. Every seed is a sentinel that differs from the post-condition. */
static void check_winner_body(void)
{
    u32 s0 = DS_001077B0;
    u32 s1 = DS_001077B0 + 0x94u;
    u32 r0 = FIGHT_RECS;
    u32 r1 = FIGHT_RECS + 0x100u;
    u16 sv_w0 = DSW(0x000A6728u);
    u16 sv_w2 = DSW(0x000A6728u + 2u);
    u32 sv_d8 = DSD(0x000A3528u + 8u);
    u8  sv_b11a = DSB(0x000DE11Au);
    u8  sv_a80 = DSB(0x00107A80u);
    u8  sv_ac0 = DSB(0x00107A80u + 0x40u);

    /* --- side 0 wins: the pose lands on the other (side-1) slot. --- */
    winner_body_setup(s0, s1, r0, r1);
    /* r0+0x20 = 7.5f -> max(5, 7.5) -> truncate 7; r0+0x24 = 3.5f -> 3. */
    DSD(r0 + 0x20u) = 0x40F00000u;
    DSD(r0 + 0x24u) = 0x40600000u;
    /* The other slot's +0x14 callback (0x1952F): EAX = EDX = that slot
     * (0x19535), zeroed on a non-zero return (0x19542). */
    if (fn_resolve(KB_STUB_14) == NULL)
        fn_register(KB_STUB_14, (void (*)(void))kb_stub_14);
    DSD(s1 + 0x14u) = KB_STUB_14;
    kb_stub_calls = 0;
    kb_stub_ret = 1;
    kb_stub_arg = 0x5555u;

    fighter_winner_body(0u);

    CHECK_EQ_INT(kb_stub_calls, 1);
    CHECK_EQ_INT((int)kb_stub_arg, (int)s1);
    CHECK_EQ_INT((int)DSD(s1 + 0x14u), 0);
    CHECK_EQ_INT((int)DSB(s1 + 0x52u), 0x10);   /* the 0x3A504 pose on the other slot */
    CHECK_EQ_INT((int)DSB(s1 + 0x53u), 0x0A);
    CHECK_EQ_INT((int)DSB(s1 + 0x54u), 0);
    CHECK_EQ_INT((int)DSD(s1 + 0x10u), 0x0003A43C);
    CHECK_EQ_INT((int)DSW(DS_00100B50), 0x1234);   /* the 0x19472 latch */
    CHECK_EQ_INT((int)DSW(s0 + 0x84u), 0x1234);
    CHECK_EQ_INT((int)DSB(s0 + 0x8Au), 0);         /* cleared at 0x19576 */
    CHECK_EQ_INT((int)DSB(s0 + 0x43u), 0xFC);      /* the 0xFC mask (0xFF & 0xFC) */
    CHECK_EQ_INT((int)DSB(s1 + 0x43u), 0xF4);      /* 0xFC then &0xF7 at 0x1956E */
    CHECK_EQ_INT((int)DSB(DS_00100B5A), 7);        /* 0x19164: truncate(max(5,7.5)) */
    CHECK_EQ_INT((int)DSB(DS_00100B5C), 3);        /* 0x19164: truncate(3.5) */
    CHECK_EQ_INT((int)DSB(DS_00100B58), 0);        /* = slot0+0x5F */
    CHECK_EQ_INT((int)DSD(0x001088ECu), 3);        /* 0x192DC/0x39278: 2+1 */
    CHECK_EQ_INT((int)DSD(r0 + 0x24u), 0);         /* 0x19164 zeroed it */

    /* --- side 1 wins: the mirror pose lands on slot 0. --- */
    winner_body_setup(s0, s1, r0, r1);
    /* r1+0x20 = 0 -> the 5.0 floor; r1+0x24 = 0 -> the B5C = 2 arm. */
    DSD(r1 + 0x20u) = 0;
    DSD(r1 + 0x24u) = 0;

    fighter_winner_body(1u);

    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 0x10);   /* the mirror pose on slot 0 */
    CHECK_EQ_INT((int)DSB(s0 + 0x53u), 0x0A);
    CHECK_EQ_INT((int)DSB(s0 + 0x54u), 0);
    CHECK_EQ_INT((int)DSD(s0 + 0x10u), 0x0003A43C);
    CHECK_EQ_INT((int)DSW(DS_00100B50 + 2u), 0x5678);   /* the side-1 latch */
    CHECK_EQ_INT((int)DSW(s1 + 0x84u), 0x5678);
    CHECK_EQ_INT((int)DSB(DS_00100B5A + 1u), 5);   /* 0x19164: the 5.0 floor */
    CHECK_EQ_INT((int)DSB(DS_00100B5C + 1u), 2);   /* 0x19164: rec+0x24 <= 0 */
    CHECK_EQ_INT((int)DSB(DS_00100B58 + 1u), 0);   /* = slot1+0x5F */
    CHECK_EQ_INT((int)DSD(0x001088ECu), 3);        /* 0x192DC/0x39278: 2+1 */

    /* --- side 0 wins with the T-rex's +0x1C = 0x3E4C4 (record §19.6): the
     * 0x194B6 non-zero arm sets the other slot's +0x90 = 5 (0x194F7), calls
     * the callback with EAX = side (0x19505), and zeroes +0x18/+0x1C. 0x3E4C4
     * applies 0x3B714(slot[1], slot[0]), so the pose lands on slot 1 as the
     * +0x1C == 0 arm's does; unresolved, nothing would. The registration is
     * asserted in check_anim_hold_scaler; it is made here only when missing. */
    if (fn_resolve(0x3E4C4u) == NULL)
        fn_register(0x3E4C4u, (void (*)(void))fighter_3e4c4);
    winner_body_setup(s0, s1, r0, r1);
    DSD(r0 + 0x20u) = 0x40F00000u;
    DSD(r0 + 0x24u) = 0x40600000u;
    DSD(s0 + 0x18u) = 0x0003E484u;
    DSD(s0 + 0x1Cu) = 0x0003E4C4u;
    DSB(s1 + 0x90u) = 0xEEu;
    DSB(s0 + 0x52u) = 0x66u;                   /* slot 0 keeps its sentinel */

    fighter_winner_body(0u);

    CHECK_EQ_INT((int)DSB(s1 + 0x52u), 0x10);   /* the pose, through 0x3E4C4 */
    CHECK_EQ_INT((int)DSB(s1 + 0x53u), 0x0A);
    CHECK_EQ_INT((int)DSD(s1 + 0x10u), 0x0003A43C);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 0x66);
    CHECK_EQ_INT((int)DSB(s1 + 0x90u), 5);
    CHECK_EQ_INT((int)DSD(s0 + 0x18u), 0);
    CHECK_EQ_INT((int)DSD(s0 + 0x1Cu), 0);
    CHECK_EQ_INT((int)DSW(DS_00100B50), 0x1234);

    DSW(0x000A6728u) = sv_w0;
    DSW(0x000A6728u + 2u) = sv_w2;
    DSD(0x000A3528u + 8u) = sv_d8;
    DSB(0x000DE11Au) = sv_b11a;
    DSB(0x00107A80u) = sv_a80;
    DSB(0x00107A80u + 0x40u) = sv_ac0;
}

/* 0x1974D: fighter_pass_a's tail dispatch. Whichever of AF8[0] (= DS_00100AF8)
 * and AF8[1] (= DS_00100AFC) survives runs fighter_winner_body for that side
 * when the side's slot+0x8A gate byte (0x10783A / 0x1078CE) is set. The direct
 * winner-body test calls fighter_winner_body, so this pins the pass-level
 * wiring. Every seed is a sentinel differing from the post-condition. */
static void check_pass_a_tail(void)
{
    u32 s0 = DS_001077B0;
    u32 s1 = DS_001077B0 + 0x94u;
    u32 r0 = FIGHT_RECS;
    u32 r1 = FIGHT_RECS + 0x100u;
    u16 sv_w0 = DSW(0x000A6728u);
    u16 sv_w2 = DSW(0x000A6728u + 2u);
    u32 sv_d8 = DSD(0x000A3528u + 8u);
    u8  sv_b11a = DSB(0x000DE11Au);
    u8  sv_a80 = DSB(0x00107A80u);
    u8  sv_ac0 = DSB(0x00107A80u + 0x40u);

    /* --- side 0 wins: AF8[0] survives, AF8[1] is 0 -> the tail. --- */
    winner_body_setup(s0, s1, r0, r1);
    DSD(r0 + 0x20u) = 0x40F00000u;      /* 7.5f -> the pose on slot 1 */
    DSD(r0 + 0x24u) = 0x40600000u;
    DSB(DS_001078FA) = 2;               /* the pass gate */
    DSW(s0 + 0x76u) = 0;                /* the anim gate off: AF8 kept */
    DSW(s1 + 0x76u) = 0;
    DSB(DS_00107803) = 0;               /* slot0 +0x53: not 0x0A */
    DSB(DS_00107803 + 0x94u) = 0;
    DSD(DS_00100AF8) = 0x1111u;         /* survives */
    DSD(DS_00100AFC) = 0u;              /* -> goto tail */
    DSB(DS_0010783A) = 0xEEu;           /* slot0 +0x8A: the 0x193B0 gate */
    DSB(DS_001078CE) = 0u;

    fighter_pass_a();
    CHECK_EQ_INT((int)DSB(s1 + 0x52u), 0x10);    /* pose on the other slot */
    CHECK_EQ_INT((int)DSB(s0 + 0x8Au), 0);       /* 0x19576 cleared the gate */
    CHECK_EQ_INT((int)DSW(DS_00100B50), 0x1234); /* the 0x19472 latch */

    /* --- side 1 wins: the mirror. --- */
    winner_body_setup(s0, s1, r0, r1);
    DSD(r1 + 0x20u) = 0;
    DSD(r1 + 0x24u) = 0;
    DSB(DS_001078FA) = 2;
    DSW(s0 + 0x76u) = 0;
    DSW(s1 + 0x76u) = 0;
    DSB(DS_00107803) = 0;
    DSB(DS_00107803 + 0x94u) = 0;
    DSD(DS_00100AF8) = 0u;              /* -> goto tail */
    DSD(DS_00100AFC) = 0x2222u;         /* survives */
    DSB(DS_0010783A) = 0u;
    DSB(DS_001078CE) = 0xEEu;           /* slot1 +0x8A */

    fighter_pass_a();
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 0x10);      /* the mirror pose */
    CHECK_EQ_INT((int)DSB(s1 + 0x8Au), 0);
    CHECK_EQ_INT((int)DSW(DS_00100B50 + 2u), 0x5678);

    DSW(0x000A6728u) = sv_w0;
    DSW(0x000A6728u + 2u) = sv_w2;
    DSD(0x000A3528u + 8u) = sv_d8;
    DSB(0x000DE11Au) = sv_b11a;
    DSB(0x00107A80u) = sv_a80;
    DSB(0x00107A80u + 0x40u) = sv_ac0;
}

/* ---- the 0x3BB90 body push ----------------------------------------------- */

/* The body-push fixture: both slots live (DS_001078FA = 2), chars 0 and 3
 * (0xBEEF8 widths 0x800/0x700), +0x54 = 2 on both (halved: w = 0x400 + 0x380
 * = 1920), +0x40..+0x43 clear, and the latched points (x0, y0) and (x1, y1).
 * The latch runs the anchor path (+0x42 bit 3 clear): the zeroed actor's
 * sprite 0 is below either character's camera constant, so 0x18540 clamps the
 * anchor to 0 = slot+0x20 and 0x18350 does not run; the seeded DS_00100AB0/AB4
 * offsets are the demo's f = 772 ones (192/448 and 64/3200), so each record's
 * +0x18/+0x1C is the point minus them. The speeds are the demo's (T-rex -842,
 * raptor -160), DS_00107D30 is 1 and the 0xD3388..0xD33A8 scratch 0xA5. */
static void bp_seed(s32 x0, s32 y0, s32 x1, s32 y1)
{
    u32 s0 = DS_001077B0;
    u32 s1 = DS_001077B0 + 0x94u;
    u32 r0 = FIGHT_RECS;
    u32 r1 = FIGHT_RECS + 0x100u;
    fight_reset_recs();
    fight_reset_actors();
    mem_fill(s0, 0, 0x94u * 2u);
    fight_reset_slot_pair(s0, s1, r0, r1);
    DSB(DS_001078FA) = 2u;
    DSB(s0 + 0x7Au) = 0u;
    DSB(s1 + 0x7Au) = 3u;
    DSB(s0 + 0x54u) = 2u;
    DSB(s1 + 0x54u) = 2u;
    DSD(DS_00100AB0) = 192u;
    DSD(DS_00100AB4) = 448u;
    DSD(DS_00100AB0 + 8u) = 64u;
    DSD(DS_00100AB4 + 8u) = 3200u;
    DSD(r0 + 0x18u) = (u32)(x0 - 192);
    DSD(r0 + 0x1Cu) = (u32)(y0 - 448);
    DSD(r1 + 0x18u) = (u32)(x1 - 64);
    DSD(r1 + 0x1Cu) = (u32)(y1 - 3200);
    DSD(s0 + 0x2Cu) = 0xDEADBEEFu;
    DSD(s1 + 0x2Cu) = 0xDEADBEEFu;
    DSW(r0 + 0x34u) = (u16)-842;
    DSW(r1 + 0x34u) = (u16)-160;
    DSB(DS_00107D30) = 1u;
    mem_fill(DS_000D3388, 0xA5, 0x24u);
}

/* 0x3BB90 -> 0x4FB20 -> 0x3BAEC -> 0x3B9D8/0x3B8D8 -> 0x1883C. A: the demo's
 * f = 772 (the capture-1659 push): the points (3436, 11393)/(2500, 9932) give
 * dx 936, dy 1461, the estimate 1461 + 234 + 117 = 1812 < 1920, pen 108;
 * side 0 (right) moves +54, side 1 -54, and side 1's speed (left of the other,
 * not > 0) is zeroed while side 0's (right, < 0) is kept. B..D pin 0x4FB20's
 * three gates and its estimate, E the other branch with an odd pen, one halved
 * width and the truncating halves, F..G the +0x42 bit-2/bit-5 gates, H the
 * +0x43 bit-1 arm and 0x3BAEC's order, I the two walls, K equal x, J the
 * entry gates.
 * Every asserted field is seeded to differ from its post-condition. */
static void check_body_push(void)
{
    u32 s0 = DS_001077B0;
    u32 s1 = DS_001077B0 + 0x94u;
    u32 r0 = FIGHT_RECS;
    u32 r1 = FIGHT_RECS + 0x100u;
    u8 sv_scratch[0x24];
    tf_snap(sv_scratch, DS_000D3388, 0x24u);

    /* A: the demo push. */
    bp_seed(3436, 11393, 2500, 9932);
    CHECK_EQ_INT((int)fighter_body_push(), 1);
    CHECK_EQ_INT((int)DSB(DS_00107D30), 0);
    CHECK_EQ_INT((int)DSD(DS_000D3388), 3436);
    CHECK_EQ_INT((int)DSD(DS_000D338C), 11393);
    CHECK_EQ_INT((int)DSD(DS_000D3390), 2500);
    CHECK_EQ_INT((int)DSD(DS_000D3394), 9932);
    CHECK_EQ_INT((int)DSW(DS_000D33A8), 1920);
    CHECK_EQ_INT((int)DSD(DS_000D33A0), 936);
    CHECK_EQ_INT((int)DSD(DS_000D3398), 936);
    CHECK_EQ_INT((int)DSD(DS_000D33A4), 1461);
    CHECK_EQ_INT((int)DSD(DS_000D339C), 1461);
    CHECK_EQ_INT((int)DSD(s0 + 0x2Cu), 3490);
    CHECK_EQ_INT((int)DSD(r0 + 0x18u), 3298);
    CHECK_EQ_INT((int)DSD(s1 + 0x2Cu), 2446);
    CHECK_EQ_INT((int)DSD(r1 + 0x18u), 2382);
    CHECK_EQ_INT((int)DSD(r0 + 0x1Cu), 10945);
    CHECK_EQ_INT((int)DSD(r1 + 0x1Cu), 6732);
    CHECK_EQ_INT((int)(s16)DSW(r0 + 0x34u), -842);
    CHECK_EQ_INT((int)(s16)DSW(r1 + 0x34u), 0);
    CHECK_EQ_INT((int)(DSB(s0 + 0x41u) & 0x80u), 0x80);
    CHECK_EQ_INT((int)(DSB(s1 + 0x41u) & 0x80u), 0x80);
    /* The speed gate's other arms: the left side's +160 (> 0) is kept, the
     * right side's +7 (not < 0) is zeroed. */
    bp_seed(3436, 11393, 2500, 9932);
    DSW(r0 + 0x34u) = 7u;
    DSW(r1 + 0x34u) = 160u;
    CHECK_EQ_INT((int)fighter_body_push(), 1);
    CHECK_EQ_INT((int)(s16)DSW(r0 + 0x34u), 0);
    CHECK_EQ_INT((int)(s16)DSW(r1 + 0x34u), 160);

    /* B: |dx| = 1920 = w passes the `jg` gate (dy is written) but the estimate
     * 1920 + 1 is not below w; |dx| = 1921 returns before dy is read. */
    bp_seed(3436, 11393, 1516, 11388);
    CHECK_EQ_INT((int)fighter_body_push(), 0);
    CHECK_EQ_INT((int)DSD(DS_000D3398), 1920);
    CHECK_EQ_INT((int)DSD(DS_000D33A4), 5);
    CHECK_EQ_INT((int)DSD(s0 + 0x2Cu), 3436);
    CHECK_EQ_INT((int)DSD(r0 + 0x18u), 3244);
    CHECK_EQ_INT((int)(s16)DSW(r1 + 0x34u), -160);
    CHECK_EQ_INT((int)(DSB(s1 + 0x41u) & 0x80u), 0);
    bp_seed(3436, 11393, 1515, 11388);
    CHECK_EQ_INT((int)fighter_body_push(), 0);
    CHECK_EQ_INT((int)DSD(DS_000D33A0), 1921);
    CHECK_EQ_INT((int)DSD(DS_000D33A4), (int)0xA5A5A5A5u);
    /* |dx| 1919, |dy| 5: the estimate 1919 + 1 = 1920 equals w, no push. */
    bp_seed(3436, 11393, 1517, 11388);
    CHECK_EQ_INT((int)fighter_body_push(), 0);
    CHECK_EQ_INT((int)DSD(s0 + 0x2Cu), 3436);
    CHECK_EQ_INT((int)DSD(s1 + 0x2Cu), 1517);

    /* C: |dy| = 1921 (y0 below y1: dy -1921) fails the second gate. */
    bp_seed(3436, 8000, 3336, 9921);
    CHECK_EQ_INT((int)fighter_body_push(), 0);
    CHECK_EQ_INT((int)DSD(DS_000D33A4), -1921);
    CHECK_EQ_INT((int)DSD(DS_000D339C), 1921);
    CHECK_EQ_INT((int)DSD(s1 + 0x2Cu), 3336);

    /* D: |dx| = |dy| = 1200: 300 + 150 + 1200 = 1650, pen 270, halves 135;
     * |dx| = |dy| = 1400: 350 + 175 + 1400 = 1925 >= 1920, no push. */
    bp_seed(3400, 9000, 2200, 7800);
    CHECK_EQ_INT((int)fighter_body_push(), 1);
    CHECK_EQ_INT((int)DSD(s0 + 0x2Cu), 3535);
    CHECK_EQ_INT((int)DSD(s1 + 0x2Cu), 2065);
    bp_seed(3400, 9000, 2000, 7600);
    CHECK_EQ_INT((int)fighter_body_push(), 0);
    CHECK_EQ_INT((int)DSD(s0 + 0x2Cu), 3400);
    CHECK_EQ_INT((int)DSD(s1 + 0x2Cu), 2000);

    /* E: side 1 on the right, |dx| 2000 > |dy| 404: 2000 + 101 + 50 = 2151;
     * only side 0's width halved, w = 0x400 + 0x700 = 2816 (side 1's halved
     * instead gives 2944, both 1920 < 2151), pen 665 (odd): side 0 moves
     * -665/2 = -332 (truncating toward zero), side 1 +332. Side 0 (left, speed
     * -100, not > 0) is zeroed; side 1's +0x54 is 0, so its +50 stays. */
    bp_seed(1000, 5000, 3000, 5404);
    DSB(s1 + 0x54u) = 0u;
    DSW(r0 + 0x34u) = (u16)-100;
    DSW(r1 + 0x34u) = 50u;
    CHECK_EQ_INT((int)fighter_body_push(), 1);
    CHECK_EQ_INT((int)DSW(DS_000D33A8), 2816);
    CHECK_EQ_INT((int)DSD(s0 + 0x2Cu), 668);
    CHECK_EQ_INT((int)DSD(r0 + 0x18u), 476);
    CHECK_EQ_INT((int)DSD(s1 + 0x2Cu), 3332);
    CHECK_EQ_INT((int)(s16)DSW(r0 + 0x34u), 0);
    CHECK_EQ_INT((int)(s16)DSW(r1 + 0x34u), 50);

    /* F: +0x42 bit 2 on either slot returns after the 0x186D0 latches (the
     * +0x2C sentinels are re-latched) and before the scratch copy. */
    bp_seed(3436, 11393, 2500, 9932);
    DSB(s0 + 0x42u) = 0x04u;
    CHECK_EQ_INT((int)fighter_body_push(), 0);
    CHECK_EQ_INT((int)DSD(s0 + 0x2Cu), 3436);
    CHECK_EQ_INT((int)DSD(s1 + 0x2Cu), 2500);
    CHECK_EQ_INT((int)DSD(DS_000D3388), (int)0xA5A5A5A5u);
    bp_seed(3436, 11393, 2500, 9932);
    DSB(s1 + 0x42u) = 0x04u;
    CHECK_EQ_INT((int)fighter_body_push(), 0);
    CHECK_EQ_INT((int)DSD(s1 + 0x2Cu), 2500);
    CHECK_EQ_INT((int)DSD(DS_000D3390), (int)0xA5A5A5A5u);

    /* G: +0x42 bit 5 on side 0: side 0 is neither moved nor speed-gated (its
     * +5, right of the other and not < 0, would be zeroed), side 1 still moves
     * -54 and raises both +0x41 bit 7 (slot 1's at 0x3B9F6, slot 0's at
     * 0x3B9FE). */
    bp_seed(3436, 11393, 2500, 9932);
    DSB(s0 + 0x42u) = 0x20u;
    DSW(r0 + 0x34u) = 5u;
    CHECK_EQ_INT((int)fighter_body_push(), 1);
    CHECK_EQ_INT((int)DSD(s0 + 0x2Cu), 3436);
    CHECK_EQ_INT((int)DSD(s1 + 0x2Cu), 2446);
    CHECK_EQ_INT((int)(s16)DSW(r0 + 0x34u), 5);
    CHECK_EQ_INT((int)(DSB(s0 + 0x41u) & 0x80u), 0x80);
    CHECK_EQ_INT((int)(DSB(s1 + 0x41u) & 0x80u), 0x80);

    /* H: the other slot's +0x43 bit 1 moves the side by that slot's word
     * +0x4C and raises DS_00107D30. Slot 1's bit alone keeps side 0 first:
     * side 0 goes to 3436 - 2000 = 1436, then side 1 (now right) +54. Slot 0's
     * bit alone puts side 1 first: side 1 to 2500 + 2000 = 4500, then side 0
     * (now left) -54 and its -842 zeroed. The other order gives 2446 / 3490. */
    bp_seed(3436, 11393, 2500, 9932);
    DSB(s1 + 0x43u) = 0x02u;
    DSW(s1 + 0x4Cu) = (u16)-2000;
    CHECK_EQ_INT((int)fighter_body_push(), 1);
    CHECK_EQ_INT((int)DSB(DS_00107D30), 1);
    CHECK_EQ_INT((int)DSD(s0 + 0x2Cu), 1436);
    CHECK_EQ_INT((int)DSD(s1 + 0x2Cu), 2554);
    CHECK_EQ_INT((int)(s16)DSW(r1 + 0x34u), -160);
    bp_seed(3436, 11393, 2500, 9932);
    DSB(s0 + 0x43u) = 0x02u;
    DSW(s0 + 0x4Cu) = 2000u;
    CHECK_EQ_INT((int)fighter_body_push(), 1);
    CHECK_EQ_INT((int)DSB(DS_00107D30), 1);
    CHECK_EQ_INT((int)DSD(s1 + 0x2Cu), 4500);
    CHECK_EQ_INT((int)DSD(s0 + 0x2Cu), 3382);
    CHECK_EQ_INT((int)(s16)DSW(r0 + 0x34u), 0);

    /* I: the walls (DS_000BE018 = 0x7C00), with dy 1462: 1462 + 234 + 117 =
     * 1813, pen 107 (odd, halves +53/-53 truncating). Side 0 at 0x7C00 - 53
     * would reach the wall (`jl` fails at equality), so side 1 takes -53
     * twice and side 0 stays; one unit further in, side 0 moves. Side 1 at
     * -0x7C00 + 53 reaches the left wall (`jg` fails at equality), so side 0
     * takes +53 twice. */
    bp_seed(0x7C00 - 53, 11393, 0x7C00 - 53 - 936, 9931);
    CHECK_EQ_INT((int)fighter_body_push(), 1);
    CHECK_EQ_INT((int)DSD(s0 + 0x2Cu), 0x7C00 - 53);
    CHECK_EQ_INT((int)DSD(s1 + 0x2Cu), 0x7C00 - 53 - 936 - 106);
    bp_seed(0x7C00 - 54, 11393, 0x7C00 - 54 - 936, 9931);
    CHECK_EQ_INT((int)fighter_body_push(), 1);
    CHECK_EQ_INT((int)DSD(s0 + 0x2Cu), 0x7C00 - 1);
    bp_seed(-0x7C00 + 53 + 936, 11393, -0x7C00 + 53, 9931);
    CHECK_EQ_INT((int)fighter_body_push(), 1);
    CHECK_EQ_INT((int)DSD(s1 + 0x2Cu), -0x7C00 + 53);
    CHECK_EQ_INT((int)DSD(s0 + 0x2Cu), -0x7C00 + 53 + 936 + 106);

    /* K: equal x (the `jl` at 0x3BA37 fails, so side 0 counts as the right
     * one): dx 0, dy 1000, pen 920; side 0 +460, then side 1 (now left) -460. */
    bp_seed(3000, 9000, 3000, 8000);
    CHECK_EQ_INT((int)fighter_body_push(), 1);
    CHECK_EQ_INT((int)DSD(s0 + 0x2Cu), 3460);
    CHECK_EQ_INT((int)DSD(s1 + 0x2Cu), 2540);

    /* J: DS_001078FA != 2 or a dead slot returns before the latch, after
     * clearing DS_00107D30. */
    bp_seed(3436, 11393, 2500, 9932);
    DSB(DS_001078FA) = 1u;
    CHECK_EQ_INT((int)fighter_body_push(), 0);
    CHECK_EQ_INT((int)DSB(DS_00107D30), 0);
    CHECK_EQ_INT((int)DSD(s0 + 0x2Cu), (int)0xDEADBEEFu);
    bp_seed(3436, 11393, 2500, 9932);
    DSD(DS_001077AC) = 0u;
    CHECK_EQ_INT((int)fighter_body_push(), 0);
    CHECK_EQ_INT((int)DSD(s0 + 0x2Cu), (int)0xDEADBEEFu);
    bp_seed(3436, 11393, 2500, 9932);
    DSD(DS_001077A8) = 0u;
    CHECK_EQ_INT((int)fighter_body_push(), 0);
    CHECK_EQ_INT((int)DSD(s1 + 0x2Cu), (int)0xDEADBEEFu);

    tf_put(sv_scratch, DS_000D3388, 0x24u);
}

/* ---- Task 6: the 0x17FA0 page-flag/visibility tail ---------------------- */

/* 0x16734/0x164C0/0x16AFC/0x164F4 and the 0x17FA0 wiring. The per-character
 * map is pinned directly; the two tails through the 0xCC300 frame table (the
 * 0x16734 code path and the seeded 0x98688/0x96108 scan paths) and their
 * cached-box fallbacks; and camera_project's copy-or-zero of 0x100AC0/
 * 0x100AC8 plus the B60 raise. Every seed differs from the post-condition. */
static void check_page_tail(void)
{
    u32 s0 = DS_001077B0;
    u32 rec = FIGHT_RECS;
    u32 a1 = FIGHT_ACTORS + 1u * 0x20u;
    u8  s_fd[0x40], s_cc[0x400], s_ca[0x10];
    u32 s_ee0 = DSD(DS_00107EE0);
    u32 s_b8 = DSD(DS_001077B8), s_4c = DSD(DS_0010784C);
    const u32 code_idx = 0x000CC300u + 0xD8u * 4u;   /* char 0, code 0xD8 */
    const u32 scan_idx = 0x000CC300u + 0x01u * 4u;   /* char 0, e1 0x01 */

    tf_snap(s_fd, DS_000FD120, sizeof s_fd);
    tf_snap(s_cc, 0x000CC300u, sizeof s_cc);
    tf_snap(s_ca, 0x00098688u, sizeof s_ca);

    fight_reset_recs();
    fight_reset_actors();
    DSD(DS_001077B8) = 0;
    DSD(DS_0010784C) = 0;
    DSB(DS_0010782A) = 0;                   /* char 0 */
    DSW(rec + 0x56u) = 1;                   /* actor index 1 */
    DSW(a1) = 0xF9Fu;                       /* sprite id -> code 0xD8 */

    /* 0x16734: the per-character sprite-id map. */
    CHECK_EQ_INT(camera_sprite_code(0u), 0xD8);
    DSW(a1) = 0xF9Cu; CHECK_EQ_INT(camera_sprite_code(0u), 0xD7);
    DSW(a1) = 0xF98u; CHECK_EQ_INT(camera_sprite_code(0u), 0xD4);
    DSW(a1) = 0xFA1u; CHECK_EQ_INT(camera_sprite_code(0u), 0xD5);
    DSW(a1) = 0xFA2u; CHECK_EQ_INT(camera_sprite_code(0u), 0xD6);
    DSW(a1) = 0x1000u; CHECK_EQ_INT(camera_sprite_code(0u), -1);
    DSW(a1) = 0x8000u | 0xF9Fu;             /* bit 15 is masked off */
    CHECK_EQ_INT(camera_sprite_code(0u), 0xD8);
    DSB(DS_0010782A) = 1;                   /* char 1 */
    DSW(a1) = 0x134Fu; CHECK_EQ_INT(camera_sprite_code(0u), 0xD5);
    DSW(a1) = 0x1351u; CHECK_EQ_INT(camera_sprite_code(0u), 0xD4);
    DSW(a1) = 0x1356u; CHECK_EQ_INT(camera_sprite_code(0u), 0xD6);
    DSW(a1) = 0x1000u; CHECK_EQ_INT(camera_sprite_code(0u), -1);
    DSB(DS_0010782A) = 3;
    DSW(a1) = 0x1745u; CHECK_EQ_INT(camera_sprite_code(0u), 0xD2);
    DSW(a1) = 0x174Eu; CHECK_EQ_INT(camera_sprite_code(0u), 0xD3);
    DSW(a1) = 0x1750u; CHECK_EQ_INT(camera_sprite_code(0u), 0xD5);
    DSW(a1) = 0x1751u; CHECK_EQ_INT(camera_sprite_code(0u), 0xD6);
    DSB(DS_0010782A) = 4;
    DSW(a1) = 0x2024u; CHECK_EQ_INT(camera_sprite_code(0u), 0xD8);
    DSW(a1) = 0x2026u; CHECK_EQ_INT(camera_sprite_code(0u), 0xD9);
    DSW(a1) = 0x202Au; CHECK_EQ_INT(camera_sprite_code(0u), 0xDA);
    DSW(a1) = 0x202Cu; CHECK_EQ_INT(camera_sprite_code(0u), -1);
    DSB(DS_0010782A) = 2;                   /* char 2 */
    DSW(a1) = 0xC48u; CHECK_EQ_INT(camera_sprite_code(0u), 0xD4);
    DSW(a1) = 0xC50u; CHECK_EQ_INT(camera_sprite_code(0u), 0xD5);
    DSW(a1) = 0xC47u; CHECK_EQ_INT(camera_sprite_code(0u), -1);
    DSB(DS_0010782A) = 5;                   /* char 5 */
    DSW(a1) = 0xFA1u; CHECK_EQ_INT(camera_sprite_code(0u), 0xD3);
    DSW(a1) = 0xF98u; CHECK_EQ_INT(camera_sprite_code(0u), 0xD5);
    DSW(a1) = 0xF9Cu; CHECK_EQ_INT(camera_sprite_code(0u), 0xD4);
    DSB(DS_0010782A) = 6;                   /* char 6 */
    DSW(a1) = 0x134Fu; CHECK_EQ_INT(camera_sprite_code(0u), 0xD2);
    DSW(a1) = 0x1352u; CHECK_EQ_INT(camera_sprite_code(0u), 0xD2);
    DSW(a1) = 0x1351u; CHECK_EQ_INT(camera_sprite_code(0u), 0xD3);
    DSW(a1) = 0x1353u; CHECK_EQ_INT(camera_sprite_code(0u), 0xD3);
    DSW(a1) = 0x1357u; CHECK_EQ_INT(camera_sprite_code(0u), 0xD4);
    DSW(a1) = 0x1000u; CHECK_EQ_INT(camera_sprite_code(0u), -1);
    DSB(DS_0010782A) = 7;                   /* char 7: the default arm */
    DSW(a1) = 0xF9Fu; CHECK_EQ_INT(camera_sprite_code(0u), -1);

    /* 0x16AFC first branch: the 0x16734 code path copies the frame table. */
    DSB(DS_0010782A) = 0;
    DSW(a1) = 0xF9Fu;                       /* code 0xD8, actor bit 15 clear */
    DSD(code_idx) = 0x00100A0Bu;
    DSD(DS_00100A78) = 0xDEADBEEFu;
    CHECK_EQ_INT(camera_page_tail_a(0u, DS_00100A78), 1);
    CHECK_EQ_INT((int)DSD(DS_00100A78), 0x00100A0B);

    /* The actor bit 15 mirrors the low byte: 0x00100A0B's bytes 0/2 are
     * 0x0B/0x10, so 0x40 - 0x0B - 0x10 = 0x25. */
    DSW(a1) = 0x8000u | 0xF9Fu;
    DSD(DS_00100A78) = 0xDEADBEEFu;
    CHECK_EQ_INT(camera_page_tail_a(0u, DS_00100A78), 1);
    CHECK_EQ_INT((int)DSB(DS_00100A78), 0x25);

    /* 0x16AFC's scan path (no 0x16734 code): the seeded 0x98688 entry 0
     * (e0 5, e1 7, e2 9) matches rec+0x63 5 once 0x164C0 is true. */
    DSW(a1) = 0x1000u;                      /* no code */
    DSB(0x00098688u) = 5; DSB(0x00098688u + 1u) = 7; DSB(0x00098688u + 2u) = 9;
    DSB(s0 + 0x53u) = 8;                    /* the 7/8 gate */
    DSB(s0 + 0x5Fu) = 0;
    DSB(rec + 0x63u) = 5;
    DSD(rec + 0x24u) = 0x40000000u;         /* 2.0f */
    DSD(rec + 0x20u) = 0x3F800000u;         /* 1.0f: 2.0 - 1.0 == 1.0 */
    DSD(DS_000FD120) = 0;                   /* the cache sentinels */
    DSD(DS_000FD128) = 0;
    DSD(DS_000FD138) = 0;
    DSW(s0 + 0x84u) = 0x7788u;
    DSD(0x000CC31Cu) = 0x0BADF00Du;         /* char 0, e1 7 */
    DSD(DS_00100A78) = 0xDEADBEEFu;
    CHECK_EQ_INT(camera_page_tail_a(0u, DS_00100A78), 1);
    CHECK_EQ_INT((int)DSD(DS_00100A78), 0x0BADF00D);
    CHECK_EQ_INT((int)DSD(DS_000FD128), 9);         /* e2 */
    CHECK_EQ_INT((int)DSD(DS_000FD120), 7);         /* e1 */
    CHECK_EQ_INT((int)DSD(DS_000FD138), 0x0BADF00D);
    CHECK_EQ_INT((int)DSW(DS_00100B3C), 0x7788);    /* slot+0x84 */

    /* 0x164F4's 0x96108 scan path: char 0, slot+0x5F 0, rec+0x63 2 matches
     * entry 0 (e0 2, e1 1, e2 3) once 0x164C0 is true. */
    DSB(rec + 0x63u) = 2;
    DSD(scan_idx) = 0x0BCD1234u;
    DSD(DS_000FD148) = 0;
    DSD(DS_00100A90) = 0xDEADBEEFu;
    DSD(DS_00100AF0) = 0x00001234u;
    CHECK_EQ_INT(camera_page_tail_b(0u, DS_00100A90), 1);
    CHECK_EQ_INT((int)DSD(DS_00100A90), 0x0BCD1234);
    CHECK_EQ_INT((int)DSD(DS_000FD148), 3);         /* e2 */
    CHECK_EQ_INT((int)DSD(DS_000FD140), 1);         /* e1 */
    CHECK_EQ_INT((int)DSD(DS_000FD150), 0x1234);    /* DS_00100AF0 */
    CHECK_EQ_INT((int)DSW(DS_00100B44), 0x7788);
    CHECK_EQ_INT((int)DSW(DS_00100B48), 2);
    CHECK_EQ_INT((int)DSD(DS_000FD158), 0x0BCD1234);

    /* The cached-box path: 0x164C0 now false, so the scan finds no match, but
     * the cached countdown/box/slot+0x84 are intact. */
    DSD(rec + 0x20u) = 0;
    DSD(DS_00100A90) = 0xDEADBEEFu;
    CHECK_EQ_INT(camera_page_tail_b(0u, DS_00100A90), 1);
    CHECK_EQ_INT((int)DSD(DS_00100A90), 0x0BCD1234);

    /* The cached path's rec+0x63 >= cache check: 0 < 2 rejects. */
    DSB(rec + 0x63u) = 0;
    DSD(DS_00100A90) = 0xDEADBEEFu;
    CHECK_EQ_INT(camera_page_tail_b(0u, DS_00100A90), 0);
    CHECK_EQ_INT((int)DSD(DS_00100A90), (int)0xDEADBEEFu);

    /* The 0x17FA0 wiring: tail_a's code path fills 0x100AC0, tail_b's scan
     * path fills 0x100AC8 and raises DS_00100B60. */
    DSB(DS_0010782A) = 0;
    DSW(a1) = 0xF9Fu;                       /* code 0xD8 */
    DSB(s0 + 0x53u) = 8;
    DSB(s0 + 0x5Fu) = 0;
    DSB(rec + 0x63u) = 2;
    DSD(rec + 0x24u) = 0x40000000u;
    DSD(rec + 0x20u) = 0x3F800000u;
    DSD(scan_idx) = 0x0BCD1234u;
    DSD(code_idx) = 0x00100A0Bu;
    DSD(DS_00100AC0) = 0xDEADBEEFu;
    DSD(DS_00100AC8) = 0xDEADBEEFu;
    DSB(DS_00100B60) = 0;                   /* 0x18092 zeroes it first */
    DSD(DS_00100B08) = 0;
    DSD(DS_00100B00) = 0;
    camera_project(0u, DS_00100B08, DS_00100B00, DS_00100B62,
                   DS_00100B60, DS_00100AF0);
    CHECK_EQ_INT((int)DSB(DS_00100B60), 1);          /* 0x18115 */
    CHECK_EQ_INT((int)DSD(DS_00100AC0), 0x00100A0B); /* 0x16AFC copy */
    CHECK_EQ_INT((int)DSD(DS_00100AC8), 0x0BCD1234); /* 0x164F4 copy */

    /* The zero arm: with the state gate closed and no 0x16734 code, both tails
     * return 0 and camera_project zeroes both boxes. */
    DSB(s0 + 0x53u) = 0;
    DSW(a1) = 0x1000u;
    DSD(DS_00100AC0) = 0xDEADBEEFu;
    DSD(DS_00100AC8) = 0xDEADBEEFu;
    DSB(DS_00100B60) = 0xFFu;
    camera_project(0u, DS_00100B08, DS_00100B00, DS_00100B62,
                   DS_00100B60, DS_00100AF0);
    CHECK_EQ_INT((int)DSB(DS_00100B60), 0);
    CHECK_EQ_INT((int)DSD(DS_00100AC0), 0);
    CHECK_EQ_INT((int)DSD(DS_00100AC8), 0);

    tf_put(s_fd, DS_000FD120, sizeof s_fd);
    tf_put(s_cc, 0x000CC300u, sizeof s_cc);
    tf_put(s_ca, 0x00098688u, sizeof s_ca);
    DSD(DS_00107EE0) = s_ee0;
    DSD(DS_001077B8) = s_b8;
    DSD(DS_0010784C) = s_4c;
}

/* ---- the 0xBB9D8 type table's dispatch (arena-backdrop cycle) -----------
 *
 * The table at 0xBB9D8 is 0x30 12-byte records {desc, cb1, cb2} indexed by the
 * record's type byte. actor_spawn's tail (0x2B0D4) calls cb1 = DSD(0xBB9DC +
 * type*0xC) with (rec, slot) and tests AL: non-zero marks the record dead.
 * set_dead (0x2B150) calls cb2 = DSD(0xBB9E0 + type*0xC) when rec+0x2a bit 14
 * is set, then clears rec+0x2b 0x40. */

/* A synthetic 20-byte descriptor for the per-type spawn cases: literal id
 * 0x2F5, the given type at +4, frame 0 at +5, flags 0x1A00 at +8 (bit 0x0800
 * set, so actor_spawn's initial walk is skipped and the literal id is kept),
 * extent 0x40 at +0x0A, no palette. */
#define ARENA_DESC 0x3F40000u

static u32 arena_spawn_type(u32 type)
{
    mem_fill(ARENA_DESC, 0, 0x14u);
    DSW(ARENA_DESC) = 0x02F5u;
    DSB(ARENA_DESC + 4u) = (u8)type;
    DSW(ARENA_DESC + 8u) = 0x1A00u;
    DSW(ARENA_DESC + 0x0Au) = 0x40u;
    return actor_spawn((const u32 *)(mem + ARENA_DESC), 0, 0, 0, 0);
}

/* Seed a splice list as empty (its sentinel's next and prev point at itself). */
static void arena_list_empty(u32 sentinel)
{
    DSD(sentinel) = sentinel;
    DSD(sentinel + 4u) = sentinel;
}

/* Link `node` as the only element of the list at `sentinel`. */
static void arena_list_link(u32 sentinel, u32 node)
{
    DSD(sentinel) = node;
    DSD(sentinel + 4u) = node;
    DSD(node) = sentinel;
    DSD(node + 4u) = sentinel;
}

/* The active record whose pset id is `id`, or 0. */
static u32 arena_find_id(u32 id)
{
    for (u32 r = actor_list_head(); r != 0; r = actor_next(r)) {
        u32 pset = actor_pset(r);
        if (pset != 0 && DSW(pset) == (u16)id) return r;
    }
    return 0;
}

static int arena_u32_in(const u32 *set, u32 n, u32 v)
{
    for (u32 i = 0; i < n; i++) if (set[i] == v) return 1;
    return 0;
}

/* The 0xBB9DC/0xBB9E0 halves: the 16 non-stub entries are the registered
 * callbacks; the rest hold the stub 0x5D812. The five cb1s that pop a list
 * (0x127C0 for 0x01, 0x198E8 for 0x06/0x26/0x27/0x28, 0x28F64 for 0x19,
 * 0x2901C for 0x0A, 0x48CD8 for 0x2D) return 0xFF on an empty list; 0x412F0
 * (0x16) and 0x412FC (0x1B) return 0. */
static const u32 s_cb1_addrs[7] = { 0x127C0u, 0x198E8u, 0x28F64u, 0x2901Cu,
                                    0x48CD8u, 0x412F0u, 0x412FCu };
static const u32 s_cb2_addrs[9] = { 0x12800u, 0x19928u, 0x290D0u, 0x3B9C4u,
                                    0x3D784u, 0x3FC90u, 0x40684u, 0x48D3Cu,
                                    0x49444u };
static const u8 s_pop_types[8] = { 0x01u, 0x06u, 0x26u, 0x27u, 0x28u, 0x19u,
                                   0x0Au, 0x2Du };

/* The table's shape, the registrations, and every type through the real spawn
 * dispatch with the four pop lists empty: the eight pop-cb1 types are killed,
 * all others stay visible. A stub-only dispatch (the pre-fix predicate) leaves
 * the pop types visible and fails the kill assertions. */
static void check_type_table(void)
{
    int n1 = 0, n2 = 0;

    actors_reset();
    for (u32 t = 0; t < 0x30u; t++) {
        u32 c1 = DSD(DS_000BB9DC + t * 0xCu);
        u32 c2 = DSD(DS_000BB9E0 + t * 0xCu);
        if (c1 != FN_0005D812) {
            n1++;
            CHECK(arena_u32_in(s_cb1_addrs, 7u, c1), "cb1 is one of the seven");
        }
        if (c2 != FN_0005D812) {
            n2++;
            CHECK(arena_u32_in(s_cb2_addrs, 9u, c2), "cb2 is one of the nine");
        }
    }
    CHECK_EQ_INT(n1, 10);   /* 0x01, 0x06/0x26/0x27/0x28, 0x19, 0x0A, 0x2D,
                             * 0x16, 0x1B */
    CHECK_EQ_INT(n2, 22);   /* those ten minus 0x16/0x1B, plus 0x1A,
                             * 0x02..0x05, 0x08, 0x10, 0x09, 0x20..0x25 */
    for (u32 i = 0; i < 7u; i++)
        CHECK(fn_resolve(s_cb1_addrs[i]) != NULL, "cb1 registered");
    for (u32 i = 0; i < 9u; i++)
        CHECK(fn_resolve(s_cb2_addrs[i]) != NULL, "cb2 registered");
    CHECK(fn_resolve(FN_0005D812) == NULL, "the stub is unregistered");

    arena_list_empty(0x000F0A78u);
    arena_list_empty(0x00100C20u);
    arena_list_empty(0x00104888u);
    arena_list_empty(0x001082E0u);
    for (u32 t = 0; t < 0x30u; t++) {
        int pops = 0;
        for (u32 i = 0; i < 8u; i++) if (s_pop_types[i] == t) pops = 1;
        u32 expected = DSD(DS_00105B3C);
        u32 got = arena_spawn_type(t);
        CHECK(expected != 0 && expected != DS_00105B3C, "the free-list head");
        if (pops) {
            CHECK_EQ_INT((int)got, 0);
            CHECK_EQ_INT((int)DSB(expected + 0x48u), 0);
            CHECK((DSW(expected + 0x28u) & 8u) != 0,
                  "the pop-cb1 kill sets the dead bit");
        } else {
            CHECK(got != 0, "a non-pop type stays visible");
            if (got != 0)
                CHECK_EQ_INT((int)(DSW(got + 0x28u) & 8u), 0);
        }
    }
}

/* The callback bodies' written fields: the two 9-byte writers, the five
 * list-pop cb1s (empty -> 0xFF, non-empty -> 0 and the node re-linked), then
 * #3/#4's rng tails and 0x2BE5C. */
static void check_type_callbacks(void)
{
    u32 rec, node, node2, pset;
    u32 seed = 0x12345678u;

    actors_reset();
    rec = DSD(DS_001014F4);
    node = rec + ACTOR_REC_SIZE;
    node2 = rec + 2u * ACTOR_REC_SIZE;
    DSW(rec + 0x56) = 0;
    DSW(node + 0x56) = 1;
    pset = actor_pset(rec);

    /* 0x412F0 / 0x412FC: the two 9-byte position writers. */
    {
        u8 (*f6)(u32, u32) = (u8 (*)(u32, u32))(void *)fn_resolve(0x412F0u);
        u8 (*f7)(u32, u32) = (u8 (*)(u32, u32))(void *)fn_resolve(0x412FCu);
        CHECK(f6 != NULL && f7 != NULL, "the 0x16/0x1B writers registered");
        if (f6 != NULL && f7 != NULL) {
            DSW(rec + 0x34) = 0x7FFFu;
            CHECK_EQ_INT((int)f6(rec, 0), 0);
            CHECK_EQ_INT((int)DSW(rec + 0x34), 0x200);
            DSW(rec + 0x34) = 0x7FFFu;
            CHECK_EQ_INT((int)f7(rec, 0), 0);
            CHECK_EQ_INT((int)DSW(rec + 0x34), 0x140);
        }
    }

    /* The five list-pop cb1s: {callback, pop sentinel, insert sentinel}. */
    static const u32 pop[5][3] = {
        { 0x127C0u, 0x000F0A78u, 0x000F0AE0u },
        { 0x198E8u, 0x00100C20u, 0x00100C28u },
        { 0x28F64u, 0x00104888u, 0x00104880u },
        { 0x2901Cu, 0x00104888u, 0x00104880u },
        { 0x48CD8u, 0x001082E0u, 0x00108368u },
    };
    DSB(0x00108398u) = 0;    /* DS_00108398: no symbols.h name */
    for (u32 i = 0; i < 5u; i++) {
        u8 (*f)(u32, u32) = (u8 (*)(u32, u32))(void *)fn_resolve(pop[i][0]);
        CHECK(f != NULL, "the pop cb1 registered");
        if (f == NULL) continue;

        /* Empty list: 0xFF and no write. */
        arena_list_empty(pop[i][1]);
        arena_list_empty(pop[i][2]);
        DSD(rec + 0x14) = 0xDEADBEEFu;
        DSW(rec + 0x34) = 0x7FFFu;
        CHECK_EQ_INT((int)f(rec, 0), 0xFF);
        CHECK_EQ_INT((int)DSD(rec + 0x14), (int)0xDEADBEEFu);
        CHECK_EQ_INT((int)DSW(rec + 0x34), 0x7FFF);

        /* One node: popped, linked at rec+0x14, re-inserted at the insert
         * sentinel's head. */
        arena_list_link(pop[i][1], node);
        DSD(rec + 0x14) = 0xDEADBEEFu;
        CHECK_EQ_INT((int)f(rec, 0), 0);
        CHECK_EQ_INT((int)DSD(rec + 0x14), (int)node);
        CHECK_EQ_INT((int)DSD(node + 8u), (int)rec);
        CHECK_EQ_INT((int)DSD(pop[i][2]), (int)node);
        CHECK_EQ_INT((int)DSD(node), (int)pop[i][2]);
        CHECK_EQ_INT((int)DSD(node + 4u), (int)pop[i][2]);
        CHECK_EQ_INT((int)DSD(pop[i][1]), (int)pop[i][1]);

        if (i == 2u || i == 3u) {          /* 0x28F64 / 0x2901C */
            CHECK_EQ_INT((int)DSW(rec + 0x32), (int)DSW(DS_000BD898));
            CHECK((DSB(rec + 0x29) & 0x10u) != 0,
                  "the 0x19/0x0A head sets rec+0x29 0x10");
            CHECK_EQ_INT((int)DSW(rec + 0x44), 0x0C);
            CHECK_EQ_INT((int)DSB(node + 0x0Cu), 0);
        }
        if (i == 4u) {                     /* 0x48CD8 */
            CHECK((DSB(DS_00104AE8) & 0x02u) != 0,
                  "the 0x2D head sets 0x104AE8 bit 1");
            CHECK_EQ_INT((int)DSB(0x00108398u), 1);
        }

        /* Two nodes: the destination already holds one, so the re-insert end
         * is observable. The popped node must become the sentinel's next
         * (0x249B0 insert-after); the insert-before form would make it the
         * sentinel's prev and put node2 at the head. */
        arena_list_link(pop[i][1], node);
        arena_list_link(pop[i][2], node2);
        DSD(rec + 0x14) = 0xDEADBEEFu;
        CHECK_EQ_INT((int)f(rec, 0), 0);
        CHECK_EQ_INT((int)DSD(rec + 0x14), (int)node);
        CHECK_EQ_INT((int)DSD(pop[i][2]), (int)node);          /* head */
        CHECK_EQ_INT((int)DSD(node), (int)node2);
        CHECK_EQ_INT((int)DSD(node + 4u), (int)pop[i][2]);
        CHECK_EQ_INT((int)DSD(node2), (int)pop[i][2]);
        CHECK_EQ_INT((int)DSD(node2 + 4u), (int)node);
        CHECK_EQ_INT((int)DSD(pop[i][2] + 4u), (int)node2);    /* tail */
        CHECK_EQ_INT((int)DSD(pop[i][1]), (int)pop[i][1]);     /* pop empty */
    }

    /* #3's tail (0x28F64): rec+0x34 takes rng(0x20)+0x20 (the first draw),
     * rec+0x36 rng(0x80)+0xC0 (the second). The head sets rec+0x29 bit 4, so
     * 0x2BE5C takes its mode-1 arm: rec+0x18 = pset+4 + (rec+0x44 >> 16)*2
     * - 0x2A00. The high word of rec+0x44 is the 0x107900 ramp entry written
     * at 0x2BEBC just before the read at 0x2BEC0 (record §0.3.11), and that
     * entry is 0 here, so the value is pset+4 - 0x2A00. DS_000F0AF0 is seeded
     * non-zero so the else arm would add it and fail the assertion; the
     * sibling pset+0x14 == pset+8 assertion discriminates the arm too (only
     * the mode-1 arm writes pset+0x14). The word sentinel 0x2000/0x6000
     * carries bit 14 (the arm selector) and bit 5 (the 0x2BE5C clear), which
     * must differ from the post-state 0. */
    {
        u8 (*f3)(u32, u32) = (u8 (*)(u32, u32))(void *)fn_resolve(0x28F64u);
        u32 e1, e2;
        CHECK(f3 != NULL, "0x28F64 registered");
        if (f3 != NULL) {
            arena_list_empty(0x00104880u);
            arena_list_link(0x00104888u, node);
            DSW(rec + 0x28) = 0x2000u;     /* bit 14 clear, bit 5 sentinel */
            DSB(DS_00104AE8) = 0;
            DSD(pset + 4u) = 0x11223344u;
            DSD(pset + 8u) = 0x33445566u;
            DSD(DS_000F0AF0) = 0x55667788u;
            rng_seed(seed);
            e1 = rng_next(0x20u) + 0x20u;
            e2 = rng_next(0x80u) + 0xc0u;
            rng_seed(seed);
            CHECK_EQ_INT((int)f3(rec, 0), 0);
            CHECK_EQ_INT((int)DSW(rec + 0x34), (int)(u16)e1);
            CHECK_EQ_INT((int)DSW(rec + 0x36), (int)(u16)e2);
            CHECK_EQ_INT((int)DSW(rec + 0x44), 0x0C);
            CHECK_EQ_INT((int)(DSB(rec + 0x29) & 0x40u), 0);
            CHECK((DSB(DS_00104AE8) & 0x80u) != 0,
                  "the 0x19 tail sets 0x104AE8 bit 7");
            CHECK_EQ_INT((int)DSD(rec + 0x18),
                         (int)(0x11223344u - 0x2A00u));  /* the mode-1 arm */
            CHECK_EQ_INT((int)DSD(pset + 0x14u), (int)0x33445566u);
            CHECK_EQ_INT((int)(DSB(rec + 0x29) & 0x20u), 0);

            /* bit 14 set: the negated draw. The 0x40 flip bit is unobservable
             * here: the selector itself is rec+0x29 bit 6. */
            arena_list_link(0x00104888u, node);
            DSW(rec + 0x28) = 0x6000u;
            rng_seed(seed);
            e1 = rng_next(0x20u) + 0x20u;
            rng_seed(seed);
            CHECK_EQ_INT((int)f3(rec, 0), 0);
            CHECK_EQ_INT((int)DSW(rec + 0x34), (int)(u16)(0u - e1));
            CHECK_EQ_INT((int)(DSB(rec + 0x29) & 0x20u), 0);
        }
    }

    /* #4 (0x2901C): both draws are rng(0x80) (no +0x20) and the bit-14
     * polarity is reversed: set takes CX, clear negates and sets the 0x40. */
    {
        u8 (*f4)(u32, u32) = (u8 (*)(u32, u32))(void *)fn_resolve(0x2901Cu);
        u32 e1, e2;
        CHECK(f4 != NULL, "0x2901C registered");
        if (f4 != NULL) {
            arena_list_empty(0x00104880u);
            arena_list_link(0x00104888u, node);
            DSW(rec + 0x28) = 0x6000u;     /* bit 14 set, bit 5 sentinel */
            rng_seed(seed);
            e1 = rng_next(0x80u);
            e2 = rng_next(0x80u) + 0xc0u;
            rng_seed(seed);
            CHECK_EQ_INT((int)f4(rec, 0), 0);
            CHECK_EQ_INT((int)DSW(rec + 0x34), (int)(u16)e1);
            CHECK_EQ_INT((int)DSW(rec + 0x36), (int)(u16)e2);
            CHECK_EQ_INT((int)(DSB(rec + 0x29) & 0x20u), 0);

            arena_list_link(0x00104888u, node);
            DSW(rec + 0x28) = 0x2000u;     /* bit 14 clear, bit 5 sentinel */
            rng_seed(seed);
            e1 = rng_next(0x80u);
            rng_seed(seed);
            CHECK_EQ_INT((int)f4(rec, 0), 0);
            CHECK_EQ_INT((int)DSW(rec + 0x34), (int)(u16)(0u - e1));
            CHECK((DSB(rec + 0x29) & 0x40u) != 0,
                  "the 0x0A clear arm sets rec+0x29 0x40");
            CHECK_EQ_INT((int)(DSB(rec + 0x29) & 0x20u), 0);
        }
    }

    /* 0x2BE5C directly: the bit-12-clear (else) arm and the mode-1 arm. In the
     * mode-1 arm mode1_cursor writes rec+0x64 first, and that byte is the ramp
     * index; with DS_000F0AEC = 0xFFFFC580, rec+0x30 = 0 and DS_00107A4C = 0,
     * v = (0xFFFFC580 + 0x3BC0) >> 6 = 5, so the entry read is 0x107900+10. */
    {
        u16 saved_ramp = DSW(0x0010790Au);
        u16 saved_clamp = DSW(0x00107A4Cu);
        DSD(pset + 4u) = 0x11223344u;
        DSD(pset + 8u) = 0x33445566u;
        DSD(pset + 0x14u) = 0xDEADBEEFu;
        DSD(DS_000F0AF0) = 0x55667788u;
        DSD(DS_000F0AEC) = 0xFFFFC580u;
        DSD(rec + 0x30) = 0;
        DSW(rec + 0x28) = 0x2000u;          /* bit 12 clear, bit 5 sentinel */
        actor_mode1_pset(rec);
        CHECK_EQ_INT((int)DSD(rec + 0x18),
                     (int)(0x55667788u + 0x11223344u - 0x2A00u));
        CHECK_EQ_INT((int)(DSB(rec + 0x29) & 0x20u), 0);

        DSW(0x00107A4Cu) = 0;
        DSW(0x0010790Au) = 0x1234u;
        DSW(rec + 0x28) = 0x1000u;          /* bit 12 set, bit 5 sentinel */
        actor_mode1_pset(rec);
        CHECK_EQ_INT((int)DSB(rec + 0x64u), 5);     /* the ramp index */
        CHECK_EQ_INT((int)DSD(pset + 0x14u), (int)DSD(pset + 8u));
        CHECK_EQ_INT((int)DSW(rec + 0x46u), 0x1234);
        CHECK_EQ_INT((int)DSD(rec + 0x18),
                     (int)(0x11223344u + 0x1234u * 2u - 0x2A00u));
        CHECK_EQ_INT((int)DSD(rec + 0x1c),
                     (int)(0xFFFFC580u + 0x3BC0u - 0x33445566u));
        CHECK_EQ_INT((int)(DSB(rec + 0x29) & 0x20u), 0);
        DSW(0x0010790Au) = saved_ramp;
        DSW(0x00107A4Cu) = saved_clamp;
    }

    /* 0x2A690's two mode-1 x arms (rec+0x28 bit 12 set, rec+0x38 = 0 so
     * 0x2A620 is skipped), seeded with the demo's f = 86 values (demo-pose
     * record §12). Ramp arm (rec+0x64 >= 0): 0x2A72F stores the ramp entry at
     * +0x46 and 0x2A733/0x2A739 read it back as [rec+0x44] >> 16; the low word
     * +0x44 holds the sentinel 0x0777, so a +0x44 read gives -10240 + 0x2A00 -
     * 0xEEE. Temple 0x02F2: entry -12 -> x = -10240 + 0x2A00 + 24 = 536.
     * Velocity arm (rec+0x64 < 0, word +0x34 != 0): 0x2A6E0/0x2A6E9 read the
     * word at +0x34 (320), not +0x32 (9362); with 0x107A46 = -10 the product
     * -3200 truncates to -12 (0x2A6F4..0x2A6FC), so mountain 0x02F4 gives
     * x = -4448 + 0x2A00 + 12 = 6316 (the +0x32 read gives 6669). */
    {
        u16 saved_ramp = DSW(0x00107906u);
        u32 saved_a44 = DSD(DS_00107A44);
        DSW(rec + 0x28) = 0x1000u;          /* bit 12: the mode-1 arms */
        DSW(rec + 0x38) = 0;
        DSW(rec + 0x32) = 0;

        DSW(0x00107906u) = (u16)0xFFF4u;    /* ramp entry 3 = -12 */
        DSB(rec + 0x64u) = 3;
        DSD(rec + 0x44) = 0x55550777u;      /* +0x46 sentinel, +0x44 0x0777 */
        DSD(rec + 0x18) = (u32)-10240;
        DSD(pset + 4u) = 0xDEADBEEFu;
        actor_pset_point(rec);
        CHECK_EQ_INT((int)(s16)DSW(rec + 0x46u), -12);
        CHECK_EQ_INT((int)DSD(pset + 4u), 536);

        DSB(rec + 0x64u) = 0xFFu;
        DSW(rec + 0x32) = 9362;
        DSW(rec + 0x34) = 320;
        DSW(DS_00107A44 + 2u) = (u16)0xFFF6u;   /* 0x107A46 = -10 */
        DSD(rec + 0x18) = (u32)-4448;
        DSD(pset + 4u) = 0xDEADBEEFu;
        actor_pset_point(rec);
        CHECK_EQ_INT((int)DSD(pset + 4u), 6316);

        DSW(rec + 0x32) = 0;
        DSW(rec + 0x34) = 0;
        DSW(0x00107906u) = saved_ramp;
        DSD(DS_00107A44) = saved_a44;
    }
}

/* set_dead's cb2 dispatch (0x2B185) through four teardowns: 0x19928 (type
 * 0x06, the same list 0x100C20 both ways), 0x12800 (type 0x01, node to
 * 0xF0A78), 0x3B9C4 (type 0x02, no list) and 0x48D3C (type 0x2D, the counter
 * underflow). Pre-fix the dispatch was a documented no-op and every
 * rec+0x14/type assertion here fails. */
static void check_type_teardown(void)
{
    u32 rec, node, node2;

    actors_reset();
    rec = actor_alloc(0);
    node = actor_alloc(0);
    node2 = actor_alloc(0);
    CHECK(rec != 0 && node != 0 && node2 != 0, "the teardown seeds");
    if (rec == 0 || node == 0 || node2 == 0) return;
    DSW(rec + 0x56) = 0;

    /* Type 0x06: 0x19928 returns the rec+0x14 node to the 0x100C20 list and
     * clears the type; two nodes make the insert end observable (the head
     * node is re-inserted at the head, so the list is unchanged — the tail
     * form would swap it). */
    arena_list_empty(0x00100C20u);
    DSD(0x00100C20u) = node2;
    DSD(0x00100C24u) = node;
    DSD(node2) = node;
    DSD(node2 + 4u) = 0x00100C20u;
    DSD(node) = 0x00100C20u;
    DSD(node + 4u) = node2;
    DSD(rec + 0x14) = node2;
    DSB(rec + 0x48) = 0x06u;
    DSB(rec + 0x2b) |= 0x40u;
    DSW(rec + 0x2a) |= 0x4000u;
    actor_set_dead(rec);
    CHECK_EQ_INT((int)DSD(rec + 0x14), 0);
    CHECK_EQ_INT((int)DSB(rec + 0x48), 0);
    CHECK_EQ_INT((int)(DSB(rec + 0x2b) & 0x40u), 0);
    CHECK_EQ_INT((int)(DSW(rec + 0x28) & 8u), 8);
    CHECK_EQ_INT((int)DSD(0x00100C20u), (int)node2);
    CHECK_EQ_INT((int)DSD(node2), (int)node);
    CHECK_EQ_INT((int)DSD(node), (int)0x00100C20u);
    CHECK_EQ_INT((int)DSD(0x00100C24u), (int)node);

    /* Type 0x01: 0x12800 moves the node to 0xF0A78 and leaves the type. */
    arena_list_empty(0x000F0A78u);
    arena_list_link(0x000F0AE0u, node);
    DSD(rec + 0x14) = node;
    DSB(rec + 0x48) = 0x01u;
    DSB(rec + 0x2b) |= 0x40u;
    DSW(rec + 0x2a) |= 0x4000u;
    actor_set_dead(rec);
    CHECK_EQ_INT((int)DSD(rec + 0x14), 0);
    CHECK_EQ_INT((int)DSD(0x000F0A78u), (int)node);
    CHECK_EQ_INT((int)DSD(node), (int)0x000F0A78u);
    CHECK_EQ_INT((int)DSB(rec + 0x48), 0x01);     /* 0x12800 does not clear */
    CHECK_EQ_INT((int)(DSB(rec + 0x2b) & 0x40u), 0);

    /* Type 0x02: 0x3B9C4 zeroes the node's +8 and +0x64, leaves rec+0x14. */
    DSD(rec + 0x14) = node;
    DSD(node + 8u) = 0xDEADBEEFu;
    DSB(node + 0x64u) = 0x11u;
    DSB(rec + 0x48) = 0x02u;
    DSB(rec + 0x2b) |= 0x40u;
    DSW(rec + 0x2a) |= 0x4000u;
    actor_set_dead(rec);
    CHECK_EQ_INT((int)DSD(node + 8u), 0);
    CHECK_EQ_INT((int)DSB(node + 0x64u), 0xFF);
    CHECK_EQ_INT((int)DSD(rec + 0x14), (int)node);

    /* Type 0x2D: 0x48D3C returns the node to 0x1082E0 and, on the counter's
     * 0xFF underflow, clears the 0x104AE8 bit 1. */
    arena_list_empty(0x001082E0u);
    arena_list_link(0x00108368u, node);
    DSB(0x00108398u) = 0;
    DSB(DS_00104AE8) = 0xFFu;
    DSD(rec + 0x14) = node;
    DSB(rec + 0x48) = 0x2Du;
    DSB(rec + 0x2b) |= 0x40u;
    DSW(rec + 0x2a) |= 0x4000u;
    actor_set_dead(rec);
    CHECK_EQ_INT((int)DSD(rec + 0x14), 0);
    CHECK_EQ_INT((int)DSD(0x001082E0u), (int)node);
    CHECK_EQ_INT((int)DSB(0x00108398u), 0xFF);
    CHECK_EQ_INT((int)(DSB(DS_00104AE8) & 0x02u), 0);
    CHECK_EQ_INT((int)(DSB(rec + 0x2b) & 0x40u), 0);
}

/* 0x12750 (demo record §15): the type-0x01 node lists. Every byte of the two
 * sentinels and the eight 12-byte nodes starts at 0xA5, so each link below
 * must have been written, and the nodes' +8 (the owner field 0x127C0 writes)
 * must not be. Then the 0xBB254 flier spawn 0x1282C issues is accepted:
 * 0x127C0 pops the free head 0xF0A80 into the in-use list, where on the
 * unbuilt (or empty) list it refuses the spawn. */
static void check_dust_list(void)
{
    mem_fill(0x000F0A78u, 0xA5u, 0x70u);        /* 0xF0A78..0xF0AE7 */
    camera_dust_list_init();
    CHECK_EQ_INT((int)DSD(DS_000F0AE0), (int)DS_000F0AE0);   /* 0x12768 */
    CHECK_EQ_INT((int)DSD(DS_000F0AE4), (int)DS_000F0AE0);   /* 0x12762 */
    CHECK_EQ_INT((int)DSD(DS_000F0A7C), 0x000F0AD4);         /* the tail */
    {
        u32 prev = DS_000F0A78, n = 0, node = DSD(DS_000F0A78);
        while (node != DS_000F0A78 && n < 9u) {
            CHECK_EQ_INT((int)node, (int)(0x000F0A80u + n * 0xCu));
            CHECK_EQ_INT((int)DSD(node + 4u), (int)prev);
            CHECK_EQ_INT((int)DSD(node + 8u), (int)0xA5A5A5A5u);
            prev = node;
            node = DSD(node);
            n++;
        }
        CHECK_EQ_INT((int)n, 8);
    }

    /* The spawn inserts at 0xF0AE0, so it runs only on a linked sentinel (an
     * 0xA5 one would send the insert outside mem[]). */
    if (DSD(DS_000F0AE0) != DS_000F0AE0 || DSD(DS_000F0AE4) != DS_000F0AE0)
        return;
    actors_reset();
    u32 rec = actor_spawn((const u32 *)(mem + 0x000BB254u), 0xFFFFD800u,
                          0x5CAu, 0x1C7Cu, 0x4000u);
    CHECK(rec != 0, "the 0xBB254 spawn is accepted on the built list");
    if (rec != 0) {
        CHECK_EQ_INT((int)DSB(rec + 0x48u), 0x01);
        CHECK_EQ_INT((int)DSD(rec + 0x14u), 0x000F0A80);
        CHECK_EQ_INT((int)DSD(0x000F0A80u + 8u), (int)rec);
        CHECK_EQ_INT((int)DSD(DS_000F0AE0), 0x000F0A80);
        CHECK_EQ_INT((int)DSD(DS_000F0A78), 0x000F0A8C);
    }

    actors_reset();
    arena_list_empty(0x000F0A78u);
    rec = actor_spawn((const u32 *)(mem + 0x000BB254u), 0xFFFFD800u,
                      0x5CAu, 0x1C7Cu, 0x4000u);
    /* 0x127C0 returns 0xFFFFFFFF (0x127E1 `mov eax,-1`); the port's cb1
     * convention is the u8 0xFF, which the spawn tests as non-zero. */
    CHECK_EQ_INT((int)rec, 0);
}

/* The cycle's own invariant (the record's §7.2): the crowd actor 0
 * (descriptor 0xC7850, type 0x1B) survives its type check and carries its two
 * mountain children (the stream's inline opcode-0x0C spawns, ids 757/758)
 * into the render list across an update. Pre-fix the parent is killed at
 * 0x2B0F3 and the children cascade dead through pset_write's parent check. */
static void check_arena_backdrop(void)
{
    u32 dummy, parent, a, b, pset, n;

    actors_reset();
    dummy = actor_alloc(0);           /* consume slot 0: the parent takes 1 */
    CHECK(dummy != 0, "the arena seed's dummy alloc");
    parent = actor_spawn((const u32 *)(mem + 0xC7850u), 0xFFFFEEA0u,
                         0x2492u, 0, 0);
    CHECK(parent != 0, "the crowd actor 0 spawns and survives");
    if (parent == 0) return;

    CHECK_EQ_INT((int)DSW(parent + 0x56), 1);
    CHECK_EQ_INT((int)DSW(parent + 0x34), 0x140);      /* 0x412FC ran */
    CHECK_EQ_INT((int)DSB(parent + 0x48), 0x1B);
    CHECK_EQ_INT((int)(DSW(parent + 0x28) & 8u), 0);
    CHECK_EQ_INT((int)DSB(parent + 0x4A), 0);          /* 0x2B116 cleared it */
    pset = actor_pset(parent);
    CHECK_EQ_INT((int)DSW(pset), 756);
    CHECK_EQ_INT((int)DSW(pset + 0x0Eu), 94);          /* 0xF0 - (0x2492>>6) */

    a = arena_find_id(757);
    b = arena_find_id(758);
    CHECK(a != 0, "mountain child A (id 757) is active");
    CHECK(b != 0, "mountain child B (id 758) is active");
    if (a != 0) {
        CHECK_EQ_INT((int)(DSD(a + 0x32u) >> 16), 0xA8);
        CHECK_EQ_INT((int)DSB(a + 0x4A), 1);
    }
    if (b != 0) {
        CHECK_EQ_INT((int)(DSD(b + 0x32u) >> 16), 0x120);
        CHECK_EQ_INT((int)DSB(b + 0x4A), 1);
    }
    CHECK_EQ_INT(render_list_count(), 3);

    /* One update syncs all four records: pre-fix the parent's dead bit sends
     * it through release_record and each child through pset_write's
     * parent-dead arm, emptying the layer. The children's parent-relative
     * psets resolve against the parent's current position and layer only on
     * this sync (at spawn time the parent's pset was still unset). */
    actors_update();
    n = 0;
    for (u32 r = actor_list_head(); r != 0; r = actor_next(r)) n++;
    CHECK_EQ_INT((int)n, 4);
    CHECK_EQ_INT(render_list_count(), 3);
    CHECK_EQ_INT((int)DSW(actor_pset(parent)), 756);
    CHECK_EQ_INT((int)(DSW(parent + 0x28) & 8u), 0);
    a = arena_find_id(757);
    b = arena_find_id(758);
    CHECK(a != 0, "child A survives the update");
    CHECK(b != 0, "child B survives the update");
    if (a != 0) {
        CHECK_EQ_INT((int)DSW(actor_pset(a) + 0x0Eu), 94);
        CHECK_EQ_INT((int)DSD(actor_pset(a) + 4u),
                     (int)(DSD(pset + 4u) + 0xA8u * 64u));
    }
    if (b != 0) {
        CHECK_EQ_INT((int)DSW(actor_pset(b) + 0x0Eu), 94);
        CHECK_EQ_INT((int)DSD(actor_pset(b) + 4u),
                     (int)(DSD(pset + 4u) + 0x120u * 64u));
    }
}

int test_fight(void)
{
    int before = g_failures;

    u8 s_f0ae0[0x20];
    u8 s_proj[0xF4];
    u8 s_slots[0x160];
    u8 s_d0[0x200];
    u8 s_88[0x100];
    u8 s_a5[0x800];
    u8 s_82[0x80];
    u8 s_8100[0x80];
    u8 s_5b[0x300];
    u8 s_9ad[0x10];
    u8 s_f0a78[0x68];
    u8 s_c20[0x10];
    u8 s_4880[0x10];
    u8 s_82e0[0x90];
    u8 s_8398;
    u32 s_actor_tab = DSD(DS_001014EC);
    u32 s_res_tab = DSD(DS_001014E0);
    u32 s_res_cnt = DSD(DS_001014F0);
    u32 s_a4fc = DSD(DS_00104AFC);
    u8  s_ae8 = DSB(DS_00104AE8);
    u8  s_810d = DSB(0x0010810Du);   /* DS_0010810D: no symbols.h name */
    u32 s_rng = DSD(DS_000EF6D8);
    u32 s_frame = DSD(DS_000EF6DC);

    tf_snap(s_f0ae0, 0x000F0AE0u, 0x20u);
    tf_snap(s_proj, 0x00100A70u, 0xF4u);
    tf_snap(s_slots, 0x001077A0u, 0x160u);
    tf_snap(s_d0, 0x00107D00u, 0x200u);
    tf_snap(s_88, 0x00108840u, 0x100u);
    tf_snap(s_a5, 0x00104500u, 0x800u);
    tf_snap(s_82, 0x00108260u, 0x80u);
    tf_snap(s_8100, 0x00108100u, 0x80u);
    tf_snap(s_5b, 0x00105B00u, 0x300u);
    tf_snap(s_9ad, 0x0009AD50u, 0x10u);
    /* The type-dispatch checks' list sentinels (with the 0x12750 nodes
     * 0xF0A80..0xF0ADF) and the 0x2D counter; 0xF0AE0 is covered by s_f0ae0
     * above, and s_4880 spans the 0x104880/0x104888
     * sentinel pairs. */
    tf_snap(s_f0a78, 0x000F0A78u, 0x68u);
    tf_snap(s_c20, 0x00100C20u, 0x10u);
    tf_snap(s_4880, 0x00104880u, 0x10u);
    tf_snap(s_82e0, 0x001082E0u, 0x90u);
    s_8398 = DSB(0x00108398u);

    check_projection();
    check_dispatch();
    check_camera_split();
    check_y_commit();
    check_dust_gate();
    check_screen_base();
    check_decay();
    check_box_overlap();
    check_unfreeze();
    check_arena_frame();
    check_arena_frame_live();
    check_pset_palette_zero_handle();
    check_connect_query();
    check_fighter_pass_a();
    check_fighter_pass_b();
    check_hud_pass();
    check_hud_sync();
    check_hud_latch();
    check_effects_rng();
    check_effects_arrival();
    check_effects_fall();
    check_effects_tail();
    check_command_map();
    check_think_chain();
    check_projectile_step();
    check_point_trample();
    check_attack_consume();
    check_char_select();
    check_health_bars();
    check_slot_latch();
    check_state6();
    check_state7();
    check_game_frame_tail();
    check_list_init();
    check_scene_props();
    check_command_generator();
    check_state_dispatch();
    check_hit_frame_desc();
    check_hit_slot_seed();
    check_hit_machine();
    check_hit_scan_stance();
    check_hit_immunity_gate();
    check_hit_reactions();
    check_hit_reaction_drive();
    check_hit_chain();
    check_hit_helpers();
    check_state_machine();
    check_state_handlers();
    check_gap_handlers();
    check_deep_callees();
    check_hud_pass_machine();
    check_anim_stream_args();
    check_pose_predicate();
    check_pose_accumulator();
    check_pose_entry();
    check_pose_handler();
    check_anim_hold_scaler();
    check_trex_leap();
    check_knockback_pose();
    check_knockdown_floor();
    check_walk_entry();
    check_worshipper_arrival();
    check_worshipper_landing();
    check_trex_breath();
    check_reaction_predicates();
    check_reaction();
    check_winner_body();
    check_pass_a_tail();
    check_body_push();
    check_page_tail();
    check_type_table();
    check_type_callbacks();
    check_type_teardown();
    check_dust_list();
    check_arena_backdrop();

    tf_put(s_f0ae0, 0x000F0AE0u, 0x20u);
    tf_put(s_proj, 0x00100A70u, 0xF4u);
    tf_put(s_slots, 0x001077A0u, 0x160u);
    tf_put(s_d0, 0x00107D00u, 0x200u);
    tf_put(s_88, 0x00108840u, 0x100u);
    tf_put(s_a5, 0x00104500u, 0x800u);
    tf_put(s_82, 0x00108260u, 0x80u);
    tf_put(s_8100, 0x00108100u, 0x80u);
    tf_put(s_5b, 0x00105B00u, 0x300u);
    tf_put(s_9ad, 0x0009AD50u, 0x10u);
    tf_put(s_f0a78, 0x000F0A78u, 0x68u);
    tf_put(s_c20, 0x00100C20u, 0x10u);
    tf_put(s_4880, 0x00104880u, 0x10u);
    tf_put(s_82e0, 0x001082E0u, 0x90u);
    DSB(0x00108398u) = s_8398;
    DSD(DS_001014EC) = s_actor_tab;
    DSD(DS_001014E0) = s_res_tab;
    DSD(DS_001014F0) = s_res_cnt;
    DSD(DS_00104AFC) = s_a4fc;
    DSB(DS_00104AE8) = s_ae8;
    DSB(0x0010810Du) = s_810d;
    DSD(DS_000EF6D8) = s_rng;
    DSD(DS_000EF6DC) = s_frame;

    return g_failures - before;
}
