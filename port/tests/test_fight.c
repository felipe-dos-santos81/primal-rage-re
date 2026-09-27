/* port/tests/test_fight.c */
#include "game/actors.h"
#include "game/camera.h"
#include "game/config.h"
#include "game/effects.h"
#include "game/fight.h"
#include "game/fighter.h"
#include "game/flow.h"
#include "game/rng.h"
#include "mem.h"
#include "platform/gfx.h"
#include "platform/render.h"
#include "platform/res.h"
#include "symbols.h"
#include "test.h"
#include "test_fixtures.h"
#include <stdlib.h>
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

/* One worshipper entry of `type` (si = 3, side byte 1) on the one-entry list,
 * its actor (pset 3) on sentinels: x 0x1000, y word 0x800, speed word 0x80,
 * +0x3C 0x100, +0x44 high word 0x10, +0x29 = 0x01, a sentinel stream. */
static void wp_seed(u32 entry, u32 rec, u8 type)
{
    mem_fill(entry, 0, 0x40u);
    mem_fill(rec, 0, 0x68u);
    DSD(DS_0010884C) = entry;
    DSD(DS_0010884C + 4u) = entry;
    DSD(entry) = DS_0010884C;
    DSD(entry + 4u) = DS_0010884C;
    DSD(entry + 8u) = rec;
    DSD(entry + 0xCu) = DS_001077B0;
    DSB(entry + 0x1Eu) = type;
    DSB(entry + 0x1Cu) = 0x05u;
    DSB(entry + 0x21u) = 1u;
    DSD(entry + 0x14u) = 0x5555u;
    DSW(entry + 0x18u) = 0x7777u;
    DSB(rec + 0x48u) = 0x23u;
    DSW(rec + 0x56u) = 3u;
    DSD(rec + 0x08u) = 0xEEFD4u;
    DSD(rec + 0x24u) = 0x3F800000u;
    DSD(rec + 0x18u) = 0x1000u;
    DSD(rec + 0x30u) = 0x08000000u;
    DSW(rec + 0x34u) = 0x0080u;
    DSW(rec + 0x36u) = 0x2222u;
    DSW(rec + 0x38u) = 0x1111u;
    DSW(rec + 0x2Cu) = 0x9999u;
    DSB(rec + 0x29u) = 0x01u;
    DSD(rec + 0x3Cu) = 0x100u;
    DSD(rec + 0x44u) = 0x00100000u;
    DSW(FIGHT_ACTORS + 3u * 0x20u) = 0x07E1u;
    DSB(DS_001088BF) = 0;
    DSB(DS_001088C2) = 0;
}

/* 0x49C78 types 2, 7 and 9..12 with 0x4B2AC and 0x4A7D4 (record §42-D). The
 * three stream tables' entries 0 and 3 are distinct literal sprite ids, so a
 * wrong index fails. Type 9's and 11's frame-local writes are read only by the
 * unported mode-9 block, so only their mem[] effects are asserted. The
 * type-10 cases that prove +0x1C bit 7 is cleared set it, and run in mode 0x22
 * with DS_00104B1A = 2 so 0x4B69C's 0x17D30 tests neither side (0x17DD0/
 * 0x17E41) and the entry is not trampled. */
static void check_effects_worship(void)
{
    u32 entry = FIGHT_RECS + 0x3000u, rec = FIGHT_RECS + 0x3100u;
    u32 st_idle = FIGHT_RECS + 0x3800u, st_walk = FIGHT_RECS + 0x3840u;
    u32 st_hold = FIGHT_RECS + 0x3880u, st_wrong = FIGHT_RECS + 0x38C0u;
    u32 pset = FIGHT_ACTORS + 3u * 0x20u;
    const u32 tabs[3] = { 0x000C9544u, 0x000C95D4u, 0x000C958Cu };
    u32 sv_t[6], sv_rng = DSD(DS_000EF6D8), sv_4ec = DSD(DS_001014EC), i, r;
    u8 sv_88[0x100], sv_sl[0x250], sv_bd3[0x25];
    u16 sv_4b00 = DSW(DS_00104B00);
    u8 sv_1a = DSB(0x00104B1Au);

    for (i = 0; i < 3u; i++) {
        sv_t[i * 2u] = DSD(tabs[i]);
        sv_t[i * 2u + 1u] = DSD(tabs[i] + 12u);
    }
    tf_snap(sv_88, 0x00108840u, sizeof sv_88);
    tf_snap(sv_sl, 0x00107688u, sizeof sv_sl);
    tf_snap(sv_bd3, DS_00100BD3, sizeof sv_bd3);

    mem_fill(FIGHT_RECS, 0, 0x4000);
    mem_fill(FIGHT_ACTORS, 0, 0x80);
    DSD(DS_001014EC) = FIGHT_ACTORS;
    fight_reset_bases();
    DSW(FIGHT_RECS + 0x56u) = 1;
    DSW(FIGHT_RECS + 0x100u + 0x56u) = 2;
    DSW(DS_00104B00) = 3;
    DSW(st_idle) = 0x0321u;
    DSW(st_walk) = 0x0654u;
    DSW(st_hold) = 0x0987u;
    DSW(st_wrong) = 0x0BADu;
    DSD(0x000C9544u) = st_wrong;  DSD(0x000C9544u + 12u) = st_idle;
    DSD(0x000C95D4u) = st_wrong;  DSD(0x000C95D4u + 12u) = st_walk;
    DSD(0x000C958Cu) = st_wrong;  DSD(0x000C958Cu + 12u) = st_hold;

    /* Type 2 (0x49D90): 2 -> 1 waits; 1 -> 0 arrives through 0x4AC38 (the
     * 0xC9544[3] stream at 5.0, +0x34/+0x36/+0x38 zeroed, +0x29 0x4B -> 0x13,
     * type 0). 0x8001 -> 0x8000 is negative (signed `jg`) and arrives. */
    wp_seed(entry, rec, 2u);
    DSW(entry + 0x18u) = 2u;
    fight_effects_pass();
    CHECK_EQ_INT((int)DSW(entry + 0x18u), 1);
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 2);
    CHECK_EQ_INT((int)DSD(rec + 0x08u), 0xEEFD4);
    CHECK_EQ_INT((int)DSW(rec + 0x34u), 0x80);
    DSB(rec + 0x29u) = 0x4Bu;
    fight_effects_pass();
    CHECK_EQ_INT((int)DSW(entry + 0x18u), 0);
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 0);
    CHECK_EQ_INT((int)DSD(rec + 0x08u), (int)st_idle);
    CHECK_EQ_INT((int)DSD(rec + 0x24u), 0x40A00000);
    CHECK_EQ_INT((int)DSW(pset), 0x0321);
    CHECK_EQ_INT((int)DSW(rec + 0x34u), 0);
    CHECK_EQ_INT((int)DSW(rec + 0x36u), 0);
    CHECK_EQ_INT((int)DSW(rec + 0x38u), 0);
    CHECK_EQ_INT((int)DSB(rec + 0x29u), 0x13);
    wp_seed(entry, rec, 2u);
    DSW(entry + 0x18u) = 0x8001u;
    fight_effects_pass();
    CHECK_EQ_INT((int)DSW(entry + 0x18u), 0x8000);
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 0);

    /* Type 7 (0x49FC2). x = 0x100 with +0x1C 0: bit 2 latches, bit 0 clear
     * leaves the speed; with 0x05: no second latch, the speed 0x50 rises. */
    wp_seed(entry, rec, 7u);
    DSB(entry + 0x1Cu) = 0x00u;
    DSW(rec + 0x34u) = 0x0050u;
    DSW(rec + 0x28u) = 0x4011u;
    fight_effects_pass();
    CHECK_EQ_INT((int)DSB(entry + 0x1Cu), 0x04);
    CHECK_EQ_INT((int)DSW(rec + 0x34u), 0x50);
    CHECK_EQ_INT((int)DSW(rec + 0x28u), 0x4011);
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 7);
    DSB(entry + 0x1Cu) = 0x05u;
    fight_effects_pass();
    CHECK_EQ_INT((int)DSB(entry + 0x1Cu), 0x05);
    CHECK_EQ_INT((int)DSW(rec + 0x34u), 0x51);
    /* The steer's four arms: 0x100 rises (`jle`), 0x101 snaps to 0x100, -0x10
     * snaps to -0x100, -0x100 falls (`jle`). */
    DSW(rec + 0x34u) = 0x0100u;
    fight_effects_pass();
    CHECK_EQ_INT((int)DSW(rec + 0x34u), 0x101);
    fight_effects_pass();
    CHECK_EQ_INT((int)DSW(rec + 0x34u), 0x100);
    DSW(rec + 0x34u) = 0xFFF0u;
    fight_effects_pass();
    CHECK_EQ_INT((int)DSW(rec + 0x34u), 0xFF00);
    fight_effects_pass();
    CHECK_EQ_INT((int)DSW(rec + 0x34u), 0xFEFF);
    /* The gates' edges: x = 0 steers without the latch; 0x5400 steers without
     * the latch; 0x5401 does neither; 0x53FF latches. */
    DSD(rec + 0x3Cu) = 0;
    DSB(entry + 0x1Cu) = 0x01u;
    DSW(rec + 0x34u) = 0x0050u;
    fight_effects_pass();
    CHECK_EQ_INT((int)DSB(entry + 0x1Cu), 0x01);
    CHECK_EQ_INT((int)DSW(rec + 0x34u), 0x51);
    DSD(rec + 0x3Cu) = 0x5400u;
    fight_effects_pass();
    CHECK_EQ_INT((int)DSB(entry + 0x1Cu), 0x01);
    CHECK_EQ_INT((int)DSW(rec + 0x34u), 0x52);
    DSD(rec + 0x3Cu) = 0x5401u;
    fight_effects_pass();
    CHECK_EQ_INT((int)DSB(entry + 0x1Cu), 0x01);
    CHECK_EQ_INT((int)DSW(rec + 0x34u), 0x52);
    DSD(rec + 0x3Cu) = 0x53FFu;
    fight_effects_pass();
    CHECK_EQ_INT((int)DSB(entry + 0x1Cu), 0x05);
    /* The exit flag (+0x28 byte bit 7): -0x100 with x < -0x300, 0x100 with
     * x > 0x5700 (the word keeps +0x29 = 0x40). */
    DSW(rec + 0x34u) = 0xFF00u;
    DSD(rec + 0x3Cu) = (u32)-0x300;
    fight_effects_pass();
    CHECK_EQ_INT((int)DSW(rec + 0x28u), 0x4011);
    DSW(rec + 0x34u) = 0xFF01u;
    DSD(rec + 0x3Cu) = (u32)-0x301;
    fight_effects_pass();
    CHECK_EQ_INT((int)DSW(rec + 0x28u), 0x4011);
    DSW(rec + 0x34u) = 0xFF00u;
    fight_effects_pass();
    CHECK_EQ_INT((int)DSW(rec + 0x28u), 0x4091);
    CHECK_EQ_INT((int)DSW(rec + 0x34u), 0xFF00);
    DSW(rec + 0x28u) = 0x4011u;
    DSW(rec + 0x34u) = 0x0100u;
    DSD(rec + 0x3Cu) = 0x5700u;
    fight_effects_pass();
    CHECK_EQ_INT((int)DSW(rec + 0x28u), 0x4011);
    DSD(rec + 0x3Cu) = 0x5701u;
    fight_effects_pass();
    CHECK_EQ_INT((int)DSW(rec + 0x28u), 0x4091);
    DSW(rec + 0x28u) = 0x4011u;
    DSW(rec + 0x34u) = 0xFF00u;
    fight_effects_pass();
    CHECK_EQ_INT((int)DSW(rec + 0x28u), 0x4011);

    /* Type 9 (0x4A115): the word DS_001088B4 (0x0100: a byte read sees 0)
     * moves it to 10. */
    wp_seed(entry, rec, 9u);
    DSW(DS_001088B4) = 0;
    fight_effects_pass();
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 9);
    DSW(DS_001088B4) = 0x0100u;
    fight_effects_pass();
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 10);
    DSW(DS_001088B4) = 0;

    /* Type 10 (0x4A131) with 0x4B2AC. A: side s = C9 ^ 1 = 1: 150007 / 50000
     * = 3, 20 - (2 + 3) = 15 caps at 7. The other side's target DS_0010887C
     * 0x3000 (DS_00108870 = 0 would stop it): +0x14 = 0x10 * 2 + 0x3000 -
     * 0x2A00 = 0x620, left of x 0x1000: +0x34 = -0x80, hflip. Type 11 and
     * +0x1C bit 7 cleared. */
    DSW(DS_00104B00) = 0x22u;
    DSB(0x00104B1Au) = 2u;
    DSB(DS_001088C6) = 0; DSB(0x001088C7u) = 0; DSB(0x001088C8u) = 0;
    DSB(0x001088C9u) = 0; DSB(DS_001088CA) = 0; DSB(DS_001088CC) = 2u;
    DSD(0x00107844u + 0x3Cu) = 150007u;
    DSB(0x00107844u + 0x81u) = 20u;
    DSD(DS_00108870) = 0;
    DSD(DS_0010887C) = 0x3000u;
    DSD(DS_00108878) = 0xA5A5A5A5u;
    wp_seed(entry, rec, 10u);
    DSB(entry + 0x1Cu) = 0x85u;
    rng_seed(0x1234u);
    fight_effects_pass();
    CHECK_EQ_INT((int)DSD(DS_00108878), 7);
    CHECK_EQ_INT((int)DSD(entry + 0x14u), 0x620);
    CHECK_EQ_INT((int)DSW(rec + 0x34u), 0xFF80);
    CHECK_EQ_INT((int)DSB(rec + 0x29u), 0x41);
    CHECK_EQ_INT((int)DSD(rec + 0x08u), (int)st_walk);
    CHECK_EQ_INT((int)DSD(rec + 0x24u), 0x40400000);
    CHECK_EQ_INT((int)DSW(pset), 0x8654);
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 11);
    CHECK_EQ_INT((int)DSB(entry + 0x1Cu), 0x05);
    CHECK_EQ_INT((int)DSD(DS_000EF6D8), 0x1234);
    /* A2: x 0x100 is left of 0x620: +0x34 = 0x80, hflip cleared. */
    wp_seed(entry, rec, 10u);
    DSD(rec + 0x18u) = 0x100u;
    DSB(rec + 0x29u) = 0x41u;
    fight_effects_pass();
    CHECK_EQ_INT((int)DSW(rec + 0x34u), 0x80);
    CHECK_EQ_INT((int)DSB(rec + 0x29u), 0x01);
    CHECK_EQ_INT((int)DSW(pset), 0x0654);
    /* B: the divide is unsigned and the cap signed: 0x80000000 / 50000 =
     * 42949, 20 - (2 + 42949) = -42931 is kept. */
    DSD(0x00107844u + 0x3Cu) = 0x80000000u;
    wp_seed(entry, rec, 10u);
    fight_effects_pass();
    CHECK_EQ_INT((int)DSD(DS_00108878), -42931);
    DSD(0x00107844u + 0x3Cu) = 150007u;
    /* C: the entry's side 0 equals (s8)C9 = 0: DS_00108870 = 0 stops the actor
     * (+0x34 = 0) before the hflip clear; +0x14 and the stream stay. */
    wp_seed(entry, rec, 10u);
    DSB(entry + 0x21u) = 0;
    DSB(rec + 0x29u) = 0x41u;
    fight_effects_pass();
    CHECK_EQ_INT((int)DSW(rec + 0x34u), 0);
    CHECK_EQ_INT((int)DSB(rec + 0x29u), 0x41);
    CHECK_EQ_INT((int)DSD(entry + 0x14u), 0x5555);
    CHECK_EQ_INT((int)DSD(rec + 0x08u), 0xEEFD4);
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 11);
    /* C2: the same side walks to DS_00108870; C3: the other side stops on a
     * zero DS_0010887C. */
    DSD(DS_00108870) = 0x3200u;
    DSD(DS_0010887C) = 0;
    wp_seed(entry, rec, 10u);
    DSB(entry + 0x21u) = 0;
    fight_effects_pass();
    CHECK_EQ_INT((int)DSD(entry + 0x14u), 0x820);
    wp_seed(entry, rec, 10u);
    fight_effects_pass();
    CHECK_EQ_INT((int)DSW(rec + 0x34u), 0);
    CHECK_EQ_INT((int)DSD(entry + 0x14u), 0x5555);
    DSD(DS_00108870) = 0;
    DSD(DS_0010887C) = 0x3000u;
    /* D: C9 = 0xFF: s = (s8)0xFE = -2 reads the slot at 0x107688 (50000 / 50000
     * = 1, 9 - 3 = 6), and the entry's 0xFF (zero-extended) is not -1, so the
     * target is DS_0010887C. */
    DSB(0x001088C9u) = 0xFFu;
    DSD(0x00107688u + 0x3Cu) = 50000u;
    DSB(0x00107688u + 0x81u) = 9u;
    wp_seed(entry, rec, 10u);
    DSB(entry + 0x21u) = 0xFFu;
    fight_effects_pass();
    CHECK_EQ_INT((int)DSD(DS_00108878), 6);
    CHECK_EQ_INT((int)DSD(entry + 0x14u), 0x620);
    DSB(0x001088C9u) = 0;
    /* E: DS_001088C6 non-zero adds rng(0xC00) to the target. */
    DSB(DS_001088C6) = 1u;
    rng_seed(0x1234u);
    r = rng_next(0xC00u);
    wp_seed(entry, rec, 10u);
    rng_seed(0x1234u);
    fight_effects_pass();
    CHECK_EQ_INT((int)DSD(entry + 0x14u), (int)(0x620u + r));
    DSB(DS_001088C6) = 0;
    /* F: |0x2F81 - 0x3000| = 0x7F is under the step 0x80: stop, hflip already
     * cleared. F2: 0x80 is not (`jl`): it walks. F3: the step of a -0x80
     * speed is 0x80. */
    wp_seed(entry, rec, 10u);
    DSD(rec + 0x18u) = 0x2F81u;
    DSB(rec + 0x29u) = 0x41u;
    fight_effects_pass();
    CHECK_EQ_INT((int)DSW(rec + 0x34u), 0);
    CHECK_EQ_INT((int)DSB(rec + 0x29u), 0x01);
    CHECK_EQ_INT((int)DSD(entry + 0x14u), 0x5555);
    CHECK_EQ_INT((int)DSD(rec + 0x08u), 0xEEFD4);
    wp_seed(entry, rec, 10u);
    DSD(rec + 0x18u) = 0x2F80u;
    fight_effects_pass();
    CHECK_EQ_INT((int)DSW(rec + 0x34u), 0xFF80);
    CHECK_EQ_INT((int)DSD(entry + 0x14u), 0x620);
    wp_seed(entry, rec, 10u);
    DSD(rec + 0x18u) = 0x2F81u;
    DSW(rec + 0x34u) = 0xFF80u;
    fight_effects_pass();
    CHECK_EQ_INT((int)DSW(rec + 0x34u), 0);
    /* G: DS_001088C7 set: 0x4B430 holds (type 8, +0x55 = 1, the 0xC958C[3]
     * stream at 3.0) and 0x4B2AC does not run. G2: DS_001088C8 alone.
     * G3: a zero 0xC958C[3] declines, so the walk runs. */
    DSB(0x001088C7u) = 1u;
    DSD(DS_00108878) = 0xA5A5A5A5u;
    wp_seed(entry, rec, 10u);
    fight_effects_pass();
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 8);
    CHECK_EQ_INT((int)DSB(rec + 0x55u), 1);
    CHECK_EQ_INT((int)DSD(rec + 0x08u), (int)st_hold);
    CHECK_EQ_INT((int)DSD(rec + 0x24u), 0x40400000);
    CHECK_EQ_INT((int)DSD(DS_00108878), (int)0xA5A5A5A5u);
    DSB(0x001088C7u) = 0;
    DSB(0x001088C8u) = 1u;
    wp_seed(entry, rec, 10u);
    fight_effects_pass();
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 8);
    DSD(0x000C958Cu + 12u) = 0;
    wp_seed(entry, rec, 10u);
    fight_effects_pass();
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 11);
    CHECK_EQ_INT((int)DSD(DS_00108878), 7);
    DSD(0x000C958Cu + 12u) = st_hold;
    DSB(0x001088C8u) = 0;
    DSW(DS_00104B00) = 3;

    /* Type 11 (0x4A17A) with 0x4A7D4. +0x2C = 0x496AC(0x800) = 0xD80 each
     * frame. |0x1000 - 0x10C0| = 0xC0 <= 2 * 0x80: arrival (+0x38/+0x34/+0x36
     * zeroed, the 0xC9544[3] stream at 5.0); 0x101 past 0x100 keeps walking. */
    wp_seed(entry, rec, 11u);
    DSD(entry + 0x14u) = 0x10C0u;
    fight_effects_pass();
    CHECK_EQ_INT((int)DSW(rec + 0x2Cu), 0xD80);
    CHECK_EQ_INT((int)DSW(rec + 0x34u), 0);
    CHECK_EQ_INT((int)DSW(rec + 0x36u), 0);
    CHECK_EQ_INT((int)DSW(rec + 0x38u), 0);
    CHECK_EQ_INT((int)DSD(rec + 0x08u), (int)st_idle);
    CHECK_EQ_INT((int)DSD(rec + 0x24u), 0x40A00000);
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 11);
    wp_seed(entry, rec, 11u);
    DSD(entry + 0x14u) = 0x1101u;
    fight_effects_pass();
    CHECK_EQ_INT((int)DSW(rec + 0x2Cu), 0xD80);
    CHECK_EQ_INT((int)DSW(rec + 0x34u), 0x80);
    CHECK_EQ_INT((int)DSW(rec + 0x38u), 0x1111);
    CHECK_EQ_INT((int)DSD(rec + 0x08u), 0xEEFD4);
    /* Both magnitudes: x 0x1000 against 0x1100, speed -0x80: 0x100 <= 0x100. */
    wp_seed(entry, rec, 11u);
    DSD(entry + 0x14u) = 0x1100u;
    DSW(rec + 0x34u) = 0xFF80u;
    fight_effects_pass();
    CHECK_EQ_INT((int)DSD(rec + 0x08u), (int)st_idle);
    CHECK_EQ_INT((int)DSW(rec + 0x34u), 0);

    /* Type 12 (0x4A1EB): DS_001088CA = 0 walks left (hflip, -0x80), 1 right;
     * the next type is 14 when DS_001088C6 is non-zero, else 13. */
    wp_seed(entry, rec, 12u);
    DSB(DS_001088CA) = 0;
    DSB(DS_001088C6) = 0;
    fight_effects_pass();
    CHECK_EQ_INT((int)DSB(rec + 0x29u), 0x41);
    CHECK_EQ_INT((int)DSW(rec + 0x34u), 0xFF80);
    CHECK_EQ_INT((int)DSD(rec + 0x08u), (int)st_walk);
    CHECK_EQ_INT((int)DSD(rec + 0x24u), 0x40400000);
    CHECK_EQ_INT((int)DSW(pset), 0x8654);
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 13);
    wp_seed(entry, rec, 12u);
    DSB(rec + 0x29u) = 0x41u;
    DSB(DS_001088CA) = 1u;
    fight_effects_pass();
    CHECK_EQ_INT((int)DSB(rec + 0x29u), 0x01);
    CHECK_EQ_INT((int)DSW(rec + 0x34u), 0x80);
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 13);
    wp_seed(entry, rec, 12u);
    DSB(DS_001088CA) = 0;
    DSB(DS_001088C6) = 1u;
    fight_effects_pass();
    CHECK_EQ_INT((int)DSW(rec + 0x34u), 0xFF80);
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 14);

    for (i = 0; i < 3u; i++) {
        DSD(tabs[i]) = sv_t[i * 2u];
        DSD(tabs[i] + 12u) = sv_t[i * 2u + 1u];
    }
    tf_put(sv_88, 0x00108840u, sizeof sv_88);
    tf_put(sv_sl, 0x00107688u, sizeof sv_sl);
    tf_put(sv_bd3, DS_00100BD3, sizeof sv_bd3);
    DSW(DS_00104B00) = sv_4b00;
    DSB(0x00104B1Au) = sv_1a;
    DSD(DS_000EF6D8) = sv_rng;
    DSD(DS_001014EC) = sv_4ec;
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

/* 0x33ACC's snapshot area (record §41-C): 0x104530 + side * 0x94 (the slot)
 * and 0x104658 + side * 0x68 (the record), 0x1F8 bytes in all. */
#define FIGHT_SNAP_235C4 0x00104530u

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
    u8 sv_snap[0x1F8], sv_474c[4], sv_476c[2];
    u32 sv_fcce8 = DSD(DS_000FCCE8);
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
    tf_snap(sv_snap, FIGHT_SNAP_235C4, sizeof sv_snap);
    tf_snap(sv_474c, DS_0010474C, sizeof sv_474c);
    tf_snap(sv_476c, DS_0010476C, sizeof sv_476c);
    /* The +0x48 == 8 arm's 0x235C4 (record §41-C) spawns a palette effect
     * through 0x13C70: the raw's empty free list, a self-linked head (the
     * 0x13C85 test), keeps it off the shared effect pool. */
    DSD(DS_000FCCE8) = DS_000FCCE8;

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
    /* §41-C: the +0x48 == 8 arm's 0x235C4 (0x3B65E) froze side 1 (pc_seed
     * zeroes the slot, so none of these is a seed). */
    CHECK_EQ_INT((int)DSB(s1 + 0x52u), 0x10);       /* 0x23621 */
    CHECK_EQ_INT((int)DSB(s1 + 0x53u), 0x0A);       /* 0x23629 */
    CHECK_EQ_INT((int)DSD(s1 + 0x10u), 0x00022BEC); /* 0x23631 */
    CHECK_EQ_INT((int)DSB(s1 + 0x58u), 1);          /* 0x22BA0 */

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
    tf_put(sv_snap, FIGHT_SNAP_235C4, sizeof sv_snap);
    tf_put(sv_474c, DS_0010474C, sizeof sv_474c);
    tf_put(sv_476c, DS_0010476C, sizeof sv_476c);
    DSD(DS_000FCCE8) = sv_fcce8;
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

/* The grab arms' streams (record §42-C) start with real opcode words, which
 * the fixtures must not run: the fighter's hold streams 0xC9790[0] = 0xE7B02
 * and 0xC9790[1] = 0xE4744 get a plain sprite word 4 (the fighters' pset id,
 * so the hit test keeps its sprite) in place and stay inside 0x4AF04's ranges;
 * the held streams 0xC97AC[0][3] (0xC9688) and 0xC97AC[1][3] (0xC96B8) point
 * at `hold0`/`hold1`; 0x4BD98's 0xEF66A gets a plain word; and the pool
 * record 1's +0x4B (the fighter 0's link, 0x2BD20) is saved. */
static u16 gr_sv_e7b02, gr_sv_e4744, gr_sv_ef66a;
static u32 gr_sv_c9688, gr_sv_c96b8;
static u8 gr_sv_link1;

static void gr_patch(u32 hold0, u32 hold1)
{
    gr_sv_e7b02 = DSW(0x000E7B02u);
    gr_sv_e4744 = DSW(0x000E4744u);
    gr_sv_ef66a = DSW(0x000EF66Au);
    gr_sv_c9688 = DSD(0x000C9688u);
    gr_sv_c96b8 = DSD(0x000C96B8u);
    gr_sv_link1 = DSB(DSD(DS_001014F4) + 1u * 0x68u + 0x4Bu);
    DSW(0x000E7B02u) = 0x0004u;
    DSW(0x000E4744u) = 0x0004u;
    DSW(0x000EF66Au) = 0x0006u;
    DSD(0x000C9688u) = hold0;
    DSD(0x000C96B8u) = hold1;
}

static void gr_unpatch(void)
{
    DSW(0x000E7B02u) = gr_sv_e7b02;
    DSW(0x000E4744u) = gr_sv_e4744;
    DSW(0x000EF66Au) = gr_sv_ef66a;
    DSD(0x000C9688u) = gr_sv_c9688;
    DSD(0x000C96B8u) = gr_sv_c96b8;
    DSB(DSD(DS_001014F4) + 1u * 0x68u + 0x4Bu) = gr_sv_link1;
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
     * [0xC97E4[0], 0xC97EB[0]] = [3, 6]: 0x4B788 grabs and returns 0 (§42-C,
     * detailed in check_grab_arms), so no trample: the entry is type 8 with
     * +0x1C bit 6 and +0x20 = 0, and case 8 (not case 4) runs, the fighter
     * still in its hold (0x4AF04), so the lie timer is untouched. */
    TR_SEED(1);
    gr_patch(st_wrong, st_wrong);
    DSB(DS_001077B0 + 0x5Fu) = 0x2Du;
    DSB(FIGHT_RECS + 0x52u) = 4;
    fight_effects_pass();
    gr_unpatch();
    CHECK_EQ_INT((int)DSD(DS_00100B54), 2);
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 8);
    CHECK_EQ_INT((int)DSW(entry + 0x18u), 50);
    CHECK_EQ_INT((int)DSB(entry + 0x1Cu), 0xC5);
    CHECK_EQ_INT((int)DSB(entry + 0x20u), 0);
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

/* The grab arms 0x4B788/0x4D898, case 8's held body with 0x4AF04, 0x4B470's
 * eighth-hit tail 0x4BD98 (0x13134) / 0x4CB18, and 0x4D7A4 (record §42-C), on
 * check_point_trample's fixture: one entry (si 3, side byte 1) whose actor's
 * pset 5 holds side 0's point. Fighter 0's record sits at x 0x10000, height
 * 0x800, y word 0x800, +0x52 = 4 (inside [3, 6] for characters 0 and 1). */
static void check_grab_arms(void)
{
    u8 sv_slots[0x128], sv_g[0x1B0], sv_rows0[0x130], sv_rows1[0x130];
    u8 sv_7d[0x40], sv_7a80[0x80], sv_88[0xC0], sv_4a[0x70];
    u32 sv_res_tab = DSD(DS_001014E0), sv_res_cnt = DSD(DS_001014F0);
    u32 sv_actor_tab = DSD(DS_001014EC), sv_rng = DSD(DS_000EF6D8);
    u8 sv_3a = DSB(DS_00105B3A);
    u32 sv_tumble = DSD(0x000C9604u + 12u);
    u32 p = FIGHT_RECS + 0x200u, p2 = FIGHT_RECS + 0x300u;
    u32 entry = FIGHT_RECS + 0x3000u, rec = FIGHT_RECS + 0x3100u;
    u32 sh_fake = FIGHT_RECS + 0x3200u;
    u32 st_tumble = FIGHT_RECS + 0x3800u, st_hold0 = FIGHT_RECS + 0x3840u;
    u32 st_hold1 = FIGHT_RECS + 0x3880u;
    u32 fr0 = FIGHT_RECS, fr1 = FIGHT_RECS + 0x100u;
    u32 ps5 = FIGHT_ACTORS + 5u * 0x20u;
    s32 x_hit = (100 + 0x18) * 64, y_hit = (100 + 0x38) * 64;
    u32 link1 = DSD(DS_001014F4) + 1u * 0x68u + 0x4Bu;
    u32 link2 = DSD(DS_001014F4) + 2u * 0x68u + 0x4Bu;
    u8 sv_link2 = DSB(link2);
    u32 slot1 = DS_001077B0 + 0x94u;
    u32 entry2 = FIGHT_RECS + 0x3040u, rec2 = FIGHT_RECS + 0x3300u;
    u32 st_c955c = FIGHT_RECS + 0x38C0u;
    u32 ps6 = FIGHT_ACTORS + 6u * 0x20u;
    u32 sv_c955c = DSD(DS_000C955C + 12u);
    u32 sv_15e8 = DSD(0x001015E8u);
    u16 sv_763c = DSW(0x0010763Cu);
    u32 a1, a2, i;

    tf_snap(sv_slots, DS_001077B0, sizeof sv_slots);
    tf_snap(sv_g, DS_00100A70, sizeof sv_g);
    tf_snap(sv_rows0, 0x000FD160u, sizeof sv_rows0);
    tf_snap(sv_rows1, 0x000FEDE0u, sizeof sv_rows1);
    tf_snap(sv_7d, DS_00107D20, sizeof sv_7d);
    tf_snap(sv_7a80, DS_00107A80, sizeof sv_7a80);
    tf_snap(sv_88, 0x00108840u, sizeof sv_88);      /* 0x10884C..0x1088F2 */
    tf_snap(sv_4a, 0x00104AB8u, sizeof sv_4a);      /* 0x104ABC..0x104B1D */
    gr_patch(st_hold0, st_hold1);
    DSD(0x000C9604u + 12u) = st_tumble;
    DSW(st_tumble) = 0x0456u;
    DSW(st_hold0) = 0x0321u;
    DSW(st_hold1) = 0x0654u;
    DSW(st_c955c) = 0x0789u;
    DSD(DS_000C955C + 12u) = st_c955c;

/* A second, type-0 entry of side 1 after `entry` in the list (si 3, pset 6
 * at x 0x1000, 0x4B5A8's and 0x4AAD0's other gates closed, DS_001088B2
 * both sides 0), to observe DS_001088B2[1] within the pass. */
#define GR_SECOND() do {                                                \
        mem_fill(entry2, 0, 0x40u); mem_fill(rec2, 0, 0x68u);           \
        DSD(entry) = entry2; DSD(entry2) = DS_0010884C;                 \
        DSD(entry2 + 8u) = rec2; DSD(entry2 + 0xCu) = slot1;            \
        DSB(entry2 + 0x21u) = 1;                                        \
        DSB(rec2 + 0x48u) = 0x23u; DSW(rec2 + 0x56u) = 6;               \
        DSD(rec2 + 0x08u) = 0x0BADu; DSB(rec2 + 0x55u) = 0x77u;         \
        DSD(ps6 + 4u) = 0x1000u;                                        \
        DSB(DS_001088B2) = 0; DSB(DS_001088B2 + 1u) = 0;                \
        DSB(DS_0010889E) = 0; DSB(DS_0010889E + 1u) = 0;                \
        DSB(DS_001088B6 + (u32)DSB(fr1 + 0x51u)) = 0;                   \
    } while (0)

/* Side 0's character with the anchor that keeps its sprite handle index 4
 * (0x100AF0 + 0x17EEC(ch) = 4, as pc_seed has it for character 0). */
#define GR_CHAR(c) do {                                                 \
        DSB(DS_0010782A) = (u8)(c);                                     \
        DSD(DS_00100AF0) = (u32)(4 - (s32)camera_char_const((u32)(c))); \
    } while (0)
#define GR_SEED(c1c) do {                                               \
        ph_seed(p, p2, 0);                                              \
        mem_fill(entry, 0, 0x40u); mem_fill(rec, 0, 0x68u);             \
        mem_fill(sh_fake, 0, 0x68u); DSW(sh_fake + 0x56u) = 7;          \
        DSD(DS_0010884C) = entry; DSD(entry) = DS_0010884C;             \
        DSD(entry + 8u) = rec; DSD(entry + 0xCu) = slot1;               \
        DSB(entry + 0x21u) = 1; DSB(entry + 0x1Eu) = 4;                 \
        DSW(entry + 0x18u) = 50; DSB(entry + 0x1Cu) = (u8)(c1c);        \
        DSB(entry + 0x20u) = 1; DSB(entry + 0x1Fu) = 0;                \
        DSD(entry + 0x10u) = sh_fake;                                   \
        DSB(rec + 0x48u) = 0x23u; DSW(rec + 0x56u) = 5;                 \
        DSD(rec + 0x18u) = 0x1234u; DSD(rec + 0x30u) = 0x05000000u;     \
        DSW(rec + 0x34u) = 0x1111u; DSW(rec + 0x36u) = 0x2222u;         \
        DSW(rec + 0x38u) = 0x3333u;                                     \
        DSB(rec + 0x29u) = 0x50u; DSB(rec + 0x2Au) = 0x0Cu;             \
        DSD(ps5 + 4u) = (u32)x_hit; DSD(ps5 + 8u) = (u32)y_hit;         \
        DSB(DS_001088BF) = 0; DSB(DS_001088C2) = 0;                     \
        DSB(DS_00105B3A) = 0;                                           \
        DSB(DS_001077B0 + 0x5Fu) = 0x2Du;                               \
        DSB(fr0 + 0x52u) = 4; DSB(fr0 + 0x4Bu) = 0;                     \
        DSD(fr0 + 0x18u) = 0x10000u; DSD(fr0 + 0x1Cu) = 0x800u;         \
        DSW(fr0 + 0x32u) = 0x0800u; DSW(fr0 + 0x28u) = 0;               \
        DSB(link1) = 0x99u; DSB(link2) = 0x99u;                         \
        DSB(DS_001088AE) = 7; DSB(DS_001088AE + 1u) = 5;                \
        DSB(DS_001088B2) = 0x33u; DSB(DS_001088B2 + 1u) = 0x33u;        \
        DSB(DS_00104B1D) = 0; DSW(DS_00104AFC) = 0;                     \
        DSB(DS_001088C1) = 0; DSB(DS_001088F2) = 0;                     \
        DSB(DS_001088C5) = 0; DSD(DS_00108864) = 0;                     \
        DSD(DS_00104AD8) = 0; DSD(DS_00104ABC) = 2;                     \
        DSD(DS_001077E4) = 0x1000u; DSD(DS_00107878) = 0x3000u;         \
        DSD(DS_00108854) = 0x100u; DSD(DS_00108880) = 0x5000u;         \
        DSB(DS_00104AEC) = 0x03u;                                       \
        DSD(DS_00108868) = 0x11111111u; DSD(DS_0010886C) = 0x22222222u; \
        DSD(DS_00108884) = 0x33333333u;                                 \
        DSW(DS_001088AA) = 0x5555u; DSW(DS_001088A0) = 0x5555u;         \
        DSW(DS_00108898) = 0x5555u; DSW(DS_001088AC) = 0x5555u;         \
        DSB(DS_0010889C) = 0x55u; DSB(DS_0010889D) = 0x55u;             \
        DSB(DS_001077B0 + 0x5Bu) = 0x33u; DSB(slot1 + 0x5Bu) = 0x10u;   \
        DSB(0x00104B1Au) = 1;                                           \
    } while (0)

    /* A: 0x4B788's grab (side 0, character 0, si 3): bit 6, the shadow
     * killed, +0x29 bits 6 and 4 cleared (unflipped fighter), x = 0x10000 -
     * 0x55 * 64 (0xC9780), height 3 * 64 + 0x800 (0xC9781), the fighter's y
     * word, +0x2C = 0x496AC(0x800) = 0xD80 (after the y store; the seeded
     * 0x500 would give 0xF00), stopped, type 8 on 0xC97AC[0][3] at hold 0,
     * +0x4A = the fighter's pset 1 with the back link 5, and the fighter on
     * 0xE7B02 at 3.0. Case 8 then runs in the same pass: 0x4AF04 finds the
     * fighter in its hold, so nothing more (the lie timer is untouched). */
    GR_SEED(0x85u);
    rng_seed(0x4321u);
    fight_effects_pass();
    CHECK_EQ_INT((int)DSD(DS_00100B54), 2);
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 8);
    CHECK_EQ_INT((int)DSB(entry + 0x1Cu), 0xC5);
    CHECK_EQ_INT((int)DSB(entry + 0x20u), 0);
    CHECK_EQ_INT((int)DSB(entry + 0x1Fu), 0);
    CHECK_EQ_INT((int)DSW(entry + 0x18u), 50);
    CHECK_EQ_INT((int)DSD(entry + 0x10u), 0);
    CHECK_EQ_INT((int)(DSB(sh_fake + 0x28u) & 0x08u), 0x08);
    CHECK_EQ_INT((int)DSB(rec + 0x29u), 0x00);
    CHECK_EQ_INT((int)DSD(rec + 0x18u), 0x10000 - 0x55 * 64);
    CHECK_EQ_INT((int)DSD(rec + 0x1Cu), 3 * 64 + 0x800);
    CHECK_EQ_INT((int)DSW(rec + 0x32u), 0x0800);
    CHECK_EQ_INT((int)DSW(rec + 0x2Cu), 0x0D80);
    CHECK_EQ_INT((int)DSW(rec + 0x34u), 0);
    CHECK_EQ_INT((int)DSW(rec + 0x36u), 0);
    CHECK_EQ_INT((int)DSW(rec + 0x38u), 0);
    CHECK_EQ_INT((int)DSD(rec + 0x08u), (int)st_hold0);
    CHECK_EQ_INT((int)DSD(rec + 0x24u), 0);
    CHECK_EQ_INT((int)DSB(rec + 0x4Au), 1);
    CHECK_EQ_INT((int)DSB(link1), 5);
    CHECK_EQ_INT((int)DSB(link2), 0x99);
    CHECK_EQ_INT((int)DSD(fr0 + 0x08u), 0x000E7B02);
    CHECK_EQ_INT((int)DSD(fr0 + 0x24u), 0x40400000);
    CHECK_EQ_INT((int)DSD(DS_000EF6D8), 0x4321);
    /* The flipped fighter (+0x28 bit 0x4000): +0x29 bit 6 set, x mirrored. */
    GR_SEED(0x85u);
    DSW(fr0 + 0x28u) = 0x4000u;
    fight_effects_pass();
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 8);
    CHECK_EQ_INT((int)DSB(rec + 0x29u), 0x40);
    CHECK_EQ_INT((int)DSD(rec + 0x18u), 0x10000 + 0x55 * 64);
    /* Character 1: the offsets (0x57, 0x0C), 0xC97AC[1][3], 0xE4744 at 5.0;
     * 0x4AF04's character-1 range holds it. */
    GR_SEED(0x85u);
    GR_CHAR(1);
    fight_effects_pass();
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 8);
    CHECK_EQ_INT((int)DSD(rec + 0x18u), 0x10000 - 0x57 * 64);
    CHECK_EQ_INT((int)DSD(rec + 0x1Cu), 0x0C * 64 + 0x800);
    CHECK_EQ_INT((int)DSD(rec + 0x08u), (int)st_hold1);
    CHECK_EQ_INT((int)DSD(fr0 + 0x08u), 0x000E4744);
    CHECK_EQ_INT((int)DSD(fr0 + 0x24u), 0x40A00000);
    /* The return-0 gates grab nothing (the lie timer counts in case 4): bit
     * 6 already set, +0x52 2 or 7, +0x4B set; +0x52 3 and 6 grab. */
    {
        static const u8 c1c[4] = { 0xC5u, 0x85u, 0x85u, 0x85u };
        static const u8 st52[4] = { 4u, 2u, 7u, 4u };
        static const u8 st4b[4] = { 0u, 0u, 0u, 1u };
        for (i = 0; i < 4u; i++) {
            GR_SEED(c1c[i]);
            DSB(fr0 + 0x52u) = st52[i];
            DSB(fr0 + 0x4Bu) = st4b[i];
            fight_effects_pass();
            CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 4);
            CHECK_EQ_INT((int)DSW(entry + 0x18u), 49);
            CHECK_EQ_INT((int)DSB(entry + 0x1Cu), c1c[i]);
            CHECK_EQ_INT((int)DSD(entry + 0x10u), (int)sh_fake);
        }
        for (i = 0; i < 2u; i++) {
            GR_SEED(0x85u);
            DSB(fr0 + 0x52u) = (u8)(i == 0u ? 3u : 6u);
            fight_effects_pass();
            CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 8);
        }
    }

    /* B: case 8's held body. With +0x1C = 0x45 the prelude skips; the holder
     * (+0x20 side) is held while its record's +8 lies in its character's
     * 0x4AF04 range (both ends), released one past either end. */
    {
        static const u32 lo[7] = { 0xE7B02u, 0xE4744u, 0xED7AEu, 0xD2DECu,
                                   0xEB38Au, 0xD4A3Cu, 0xE137Au };
        static const u32 hi[7] = { 0xE7B50u, 0xE47FCu, 0xED80Cu, 0xD2E06u,
                                   0xEB3E4u, 0xD4A8Au, 0xE1432u };
#define GR_HELD(ch, cur) do {                                           \
        GR_SEED(0x45u);                                                 \
        DSB(entry + 0x1Eu) = 8; DSB(entry + 0x20u) = 0;                 \
        DSB(entry + 0x1Fu) = 3; DSB(rec + 0x4Au) = 1;                   \
        GR_CHAR(ch); DSD(fr0 + 8u) = (cur);                             \
        fight_effects_pass();                                           \
    } while (0)
        for (i = 0; i < 7u; i++) {
            GR_HELD(i, lo[i]);
            CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 8);
            GR_HELD(i, hi[i]);
            CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 8);
            GR_HELD(i, hi[i] + 1u);
            CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 6);
            GR_HELD(i, lo[i] - 1u);
            CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 6);
        }
        /* Characters above 6 (unsigned: 7 and 0xFF) are always held. */
        GR_HELD(7u, 0u);
        CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 8);
        GR_HELD(0xFFu, 0u);
        CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 8);
        /* The release (character 0, one past 0xE7B50): the holder's link,
         * +0x2A bit 3, +0x29 bit 6, +0x4A, the entry's bit 6, DS_001088AE[1]
         * counts; 0x4B470 without a +0x1F count (3, so the reversal: +0x34
         * 0x1111 -> 0xFF80), +0x36 = 0x240, the tumble stream at 3.0. */
        GR_HELD(0u, 0xE7B51u);
        CHECK_EQ_INT((int)DSB(link1), 0);
        CHECK_EQ_INT((int)DSB(rec + 0x2Au), 0x04);
        CHECK_EQ_INT((int)DSB(rec + 0x29u), 0x10);
        CHECK_EQ_INT((int)DSB(rec + 0x4Au), 0);
        CHECK_EQ_INT((int)DSB(entry + 0x1Cu), 0x05);
        CHECK_EQ_INT((int)DSB(DS_001088AE + 1u), 6);
        CHECK_EQ_INT((int)DSB(DS_001088AE), 7);
        CHECK_EQ_INT((int)DSB(entry + 0x1Fu), 3);
        CHECK_EQ_INT((int)DSB(entry + 0x20u), 0);
        CHECK_EQ_INT((int)DSW(rec + 0x34u), 0xFF80);
        CHECK_EQ_INT((int)DSW(rec + 0x36u), 0x0240);
        CHECK_EQ_INT((int)DSD(rec + 0x08u), (int)st_tumble);
        CHECK_EQ_INT((int)DSD(rec + 0x24u), 0x40400000);
        CHECK_EQ_INT((int)DSD(entry + 0x10u), (int)sh_fake);
        /* The release's DS_001088B2[1] = 1 is read in the same pass, before
         * 0x4A634 clears it, by a later type-0 entry of side 1: 0x4AAD0's
         * 0x4AB60 gate takes 0x4B3F0 (type 8 on 0xC955C[3], +0x55 = 0).
         * Without the store (DS_001088B2 seeded 0) the entry stays type 0: its
         * distance 0x1000 from side 1's record keeps 0x4B144 out. */
        GR_SEED(0x45u);
        DSB(entry + 0x1Eu) = 8; DSB(entry + 0x20u) = 0;
        DSB(entry + 0x1Fu) = 3; DSB(rec + 0x4Au) = 1;
        DSD(fr0 + 8u) = 0xE7B51u;
        GR_SECOND();
        fight_effects_pass();
        CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 6);
        CHECK_EQ_INT((int)DSB(entry2 + 0x1Eu), 8);
        CHECK_EQ_INT((int)DSD(rec2 + 0x08u), (int)st_c955c);
        CHECK_EQ_INT((int)DSB(rec2 + 0x55u), 0);
        CHECK_EQ_INT((int)DSB(DS_001088B2 + 1u), 0);
        /* The control: no release (the holder still in its hold), no store,
         * and the second entry stays type 0. */
        GR_SEED(0x45u);
        DSB(entry + 0x1Eu) = 8; DSB(entry + 0x20u) = 0;
        DSB(entry + 0x1Fu) = 3; DSB(rec + 0x4Au) = 1;
        DSD(fr0 + 8u) = 0xE7B50u;
        GR_SECOND();
        fight_effects_pass();
        CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 8);
        CHECK_EQ_INT((int)DSB(entry2 + 0x1Eu), 0);
        CHECK_EQ_INT((int)DSD(rec2 + 0x08u), 0x0BAD);
        CHECK_EQ_INT((int)DSB(rec2 + 0x55u), 0x77);
        /* 0x4B69C's release store (0x4B76E, record §29) the same way: side 0
         * out of its grab move tramples the held entry (+0x4A = 1) and the
         * later side-1 type-0 entry sees DS_001088B2[1]. */
        GR_SEED(0xC5u);
        DSB(DS_001077B0 + 0x5Fu) = 0;
        DSB(rec + 0x4Au) = 1;
        GR_SECOND();
        fight_effects_pass();
        CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 6);
        CHECK_EQ_INT((int)DSB(entry + 0x1Cu), 0x05);
        CHECK_EQ_INT((int)DSB(entry2 + 0x1Eu), 8);
        CHECK_EQ_INT((int)DSD(rec2 + 0x08u), (int)st_c955c);
        /* No +0x4A link: nothing, even out of the hold. */
        GR_SEED(0x45u);
        DSB(entry + 0x1Eu) = 8; DSB(entry + 0x20u) = 0;
        DSD(fr0 + 8u) = 0xE7B51u;
        fight_effects_pass();
        CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 8);
        CHECK_EQ_INT((int)DSB(entry + 0x1Cu), 0x45);
        CHECK_EQ_INT((int)DSB(link1), 0x99);
        /* +0x20 = 1 asks side 1's record (character 0 by pc_seed). */
        GR_HELD(0u, 0u);                    /* side 0 out: released */
        CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 6);
        GR_SEED(0x45u);
        DSB(entry + 0x1Eu) = 8; DSB(entry + 0x20u) = 1;
        DSB(rec + 0x4Au) = 1;
        DSD(fr0 + 8u) = 0; DSD(fr1 + 8u) = 0xE7B02u;
        fight_effects_pass();
        CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 8);
#undef GR_HELD
    }

    /* C: the eighth hit. Side 0 out of its grab move tramples (+0x1F 7 -> 8)
     * with every tail gate open: 0x4BD98 makes the entry DS_00108864 with
     * bit 5 and mode 0x21, and spawns the two actors at x = 0x1000 +
     * (0x2000 >> 1) - 0x100 = 0x1F00, depth 0x5000 - 0x3140; then 0x4CB18:
     * +0x34 = -0x2000 / 0x70 = -73 (0x1A570(0) holds), +0x36 = 0x3BC0 /
     * 0x16 = 0x2B7, less case 6's 0x10 in the same pass. */
#define GR_EIGHTH() do {                                                \
        GR_SEED(0x85u);                                                 \
        DSB(DS_001077B0 + 0x5Fu) = 0;                                   \
        DSB(entry + 0x1Fu) = 7; DSB(DS_001088F2) = 2;                   \
    } while (0)
/* actor_alloc pops the free list's head and actor_free pushes there, so the
 * second spawn is freed first: the free list keeps its order. */
#define GR_FREE_SPAWNS() do {                                           \
        a1 = DSD(DS_00108868); a2 = DSD(DS_0010886C);                   \
        if (a2 != 0x22222222u && a2 != 0u) { actor_set_dead(a2); actor_free(a2); } \
        if (a1 != 0x11111111u && a1 != 0u) { actor_set_dead(a1); actor_free(a1); } \
    } while (0)
    GR_EIGHTH();
    rng_seed(0x4321u);
    fight_effects_pass();
    a1 = DSD(DS_00108868); a2 = DSD(DS_0010886C);
    CHECK_EQ_INT((int)DSB(entry + 0x1Fu), 8);
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 6);
    CHECK_EQ_INT((int)DSB(entry + 0x1Cu), 0x25);
    CHECK_EQ_INT((int)DSD(DS_00108864), (int)entry);
    CHECK_EQ_INT((int)DSW(DS_00104B00), 0x21);
    CHECK_EQ_INT((int)DSB(DS_001088C1), 1);
    CHECK_EQ_INT((int)DSB(DS_001088C5), 1);
    CHECK_EQ_INT((int)DSW(DS_001088AA), 0x1E);
    CHECK_EQ_INT((int)DSW(DS_001088A0), 0);
    CHECK_EQ_INT((int)DSW(DS_00108898), 0);
    CHECK_EQ_INT((int)DSW(DS_001088AC), 0);
    CHECK_EQ_INT((int)DSB(DS_0010889C), 0);
    CHECK_EQ_INT((int)DSB(DS_0010889D), 0);
    CHECK_EQ_INT((int)DSB(DS_00104AEC), 0x02);
    CHECK_EQ_INT((int)DSD(DS_00108884), 0x1F00);
    CHECK(a1 != 0x11111111u && a1 != 0u, "0x4BD98 spawned 0xBAB88 into DS_00108868");
    CHECK(a2 != 0x22222222u && a2 != 0u, "0x4BD98 spawned 0xBAB9C into DS_0010886C");
    if (a1 != 0x11111111u && a1 != 0u && a2 != 0x22222222u && a2 != 0u) {
        CHECK_EQ_INT((int)DSD(a1 + 0x08u), 0x788);          /* 0xBAB88[0] */
        CHECK_EQ_INT((int)DSD(a1 + 0x18u), 0x1F00);
        CHECK_EQ_INT((int)DSD(a1 + 0x1Cu), 0x5000 - 0x3140);
        CHECK_EQ_INT((int)DSW(a1 + 0x32u), (int)DSW(DS_000BD898));
        CHECK_EQ_INT((int)DSW(a1 + 0x36u), 0x01A4);
        CHECK_EQ_INT((int)DSD(a2 + 0x08u), 0x000EF66A);
        CHECK_EQ_INT((int)DSD(a2 + 0x24u), 0x3F800000);
        CHECK_EQ_INT((int)DSD(a2 + 0x18u), 0x1F00);
        CHECK_EQ_INT((int)DSW(a2 + 0x36u), 0x01A4);
    }
    CHECK_EQ_INT((int)DSD(rec + 0x08u), (int)st_tumble);
    CHECK_EQ_INT((int)DSD(rec + 0x24u), 0x40400000);
    CHECK_EQ_INT((int)DSW(rec + 0x34u), 0xFFB7);
    CHECK_EQ_INT((int)DSW(rec + 0x36u), 0x02A7);
    CHECK_EQ_INT((int)DSD(DS_000EF6D8), 0x4321);
    GR_FREE_SPAWNS();
    /* The midpoint of -0x1001 and 0 is -0x801 (not the truncated -0x800);
     * the distance 0x1001 gives -0x1001 / 0x70 = -36. */
    GR_EIGHTH();
    DSD(DS_001077E4) = (u32)-0x1001; DSD(DS_00107878) = 0;
    fight_effects_pass();
    CHECK_EQ_INT((int)DSD(DS_00108884), -0x801 - 0x100);
    CHECK_EQ_INT((int)DSW(rec + 0x34u), 0xFFDC);
    GR_FREE_SPAWNS();
    /* 0x4BD98's gates (DS_00104AD8 > 0, DS_00104ABC < 2, 0x13134 both beyond
     * -0x3300 or 0x3300) stop it, but 0x4CB18 still runs. */
    for (i = 0; i < 4u; i++) {
        GR_EIGHTH();
        if (i == 0u) DSD(DS_00104AD8) = 1;
        if (i == 1u) DSD(DS_00104ABC) = 1;
        if (i == 2u) { DSD(fr0 + 0x18u) = (u32)-0x3301; DSD(fr1 + 0x18u) = (u32)-0x3301; }
        if (i == 3u) { DSD(fr0 + 0x18u) = 0x3301u; DSD(fr1 + 0x18u) = 0x3301u; }
        fight_effects_pass();
        CHECK_EQ_INT((int)DSB(entry + 0x1Fu), 8);
        CHECK_EQ_INT((int)DSD(DS_00108864), 0);
        CHECK_EQ_INT((int)DSW(DS_00104B00), 3);
        CHECK_EQ_INT((int)DSD(DS_00108868), 0x11111111);
        CHECK_EQ_INT((int)DSB(entry + 0x1Cu), 0x05);
        CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 6);
        CHECK_EQ_INT((int)DSW(rec + 0x34u), 0xFFB7);
        CHECK_EQ_INT((int)DSW(rec + 0x36u), 0x02A7);
    }
    /* ... and pass it: DS_00104AD8 = -1 (signed), DS_00104ABC = 0xFFFFFFFF
     * (unsigned), both x at -0x3300, both at 0x3300, one each side. */
    for (i = 0; i < 4u; i++) {
        GR_EIGHTH();
        if (i == 0u) { DSD(DS_00104AD8) = 0xFFFFFFFFu; DSD(DS_00104ABC) = 0xFFFFFFFFu; }
        if (i == 1u) { DSD(fr0 + 0x18u) = (u32)-0x3300; DSD(fr1 + 0x18u) = (u32)-0x3300; }
        if (i == 2u) { DSD(fr0 + 0x18u) = 0x3300u; DSD(fr1 + 0x18u) = 0x3300u; }
        if (i == 3u) { DSD(fr0 + 0x18u) = (u32)-0x3301; DSD(fr1 + 0x18u) = 0x3301u; }
        fight_effects_pass();
        CHECK_EQ_INT((int)DSB(entry + 0x1Fu), 8);
        CHECK_EQ_INT((int)DSD(DS_00108864), (int)entry);
        CHECK_EQ_INT((int)DSW(DS_00104B00), 0x21);
        GR_FREE_SPAWNS();
    }
    /* 0x4CB18's side is +0x20: side 1 alone hits (side 0's box emptied, its
     * pset 1 with bit 15 set so 0x1A570(0) = 0 would give +73); side 1's
     * pset 2 is clear, so -73. pc_seed zeroes the slots' +0x34, re-seeded. */
    GR_EIGHTH();
    ph_seed(p, p2, 1);
    DSB(DS_00100AC8 + 2u) = 0;
    DSW(FIGHT_ACTORS + 1u * 0x20u) = 0x8004u;
    DSD(ps5 + 4u) = (u32)x_hit; DSD(ps5 + 8u) = (u32)y_hit;
    DSD(DS_001077E4) = 0x1000u; DSD(DS_00107878) = 0x3000u;
    fight_effects_pass();
    CHECK_EQ_INT((int)DSB(entry + 0x20u), 1);
    CHECK_EQ_INT((int)DSB(entry + 0x1Fu), 8);
    CHECK_EQ_INT((int)DSW(rec + 0x34u), 0xFFB7);
    CHECK_EQ_INT((int)DSW(DS_00104B00), 0x21);
    GR_FREE_SPAWNS();
    /* The tail's own gates each skip 0x4BD98 and 0x4CB18: 0x4B470 alone
     * leaves the reversal 0xFF80 and 0x240 - 0x10. */
    for (i = 0; i < 8u; i++) {
        GR_EIGHTH();
        if (i == 0u) DSB(DS_00104B1D) = 2;
        if (i == 1u) DSB(DS_00104B1D) = 3;
        if (i == 2u) DSW(DS_00104AFC) = 1;
        if (i == 3u) DSB(DS_001088C1) = 1;
        if (i == 4u) DSB(DS_001088F2) = 1;
        if (i == 5u) DSB(entry + 0x1Fu) = 6;
        if (i == 6u) DSB(DS_001088C5) = 1;
        if (i == 7u) DSD(DS_00108864) = sh_fake;
        fight_effects_pass();
        CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 6);
        CHECK_EQ_INT((int)DSW(rec + 0x34u), 0xFF80);
        CHECK_EQ_INT((int)DSW(rec + 0x36u), 0x0230);
        CHECK_EQ_INT((int)DSD(DS_00108868), 0x11111111);
    }
    DSW(DS_00104AFC) = 0;
#undef GR_EIGHTH

    /* D: 0x4CB18 directly (0x4C60C's flag 1 arm too). Distance 0x2000; the
     * height 0x1000. Side 1's pset 2 with bit 15 set makes 0x1A570(1) = 0. */
    {
        static const u32 flag[5] = { 1u, 0u, 1u, 0u, 0u };
        static const u32 side[5] = { 0u, 1u, 1u, 1u, 1u };
        static const u32 e4[5] = { 0x1000u, 0x1000u, 0x1000u, 0x1000u, 0x3000u };
        static const u32 h1c[5] = { 0x1000u, 0x1000u, 0x1000u, 0x5000u, 0x1000u };
        static const int v34[5] = { 0xFF6E, 0x0049, 0x0092, 0x0049, 0x0049 };
        static const int v36[5] = { 0, 0x01FD, 0, 0xFF15, 0x01FD };
        for (i = 0; i < 5u; i++) {
            GR_SEED(0xFFu);
            DSW(FIGHT_ACTORS + 2u * 0x20u) = 0x8004u;
            DSD(DS_001077E4) = e4[i];
            DSD(DS_00107878) = (e4[i] == 0x1000u) ? 0x3000u : 0x1000u;
            DSD(rec + 0x1Cu) = h1c[i];
            fight_4cb18(entry, 3u, flag[i], side[i]);
            CHECK_EQ_INT((int)DSW(rec + 0x34u), v34[i]);
            CHECK_EQ_INT((int)DSW(rec + 0x36u), v36[i]);
            CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 6);
            CHECK_EQ_INT((int)DSB(entry + 0x1Cu), 0x7F);
            CHECK_EQ_INT((int)DSD(rec + 0x08u), (int)st_tumble);
            CHECK_EQ_INT((int)DSD(rec + 0x24u), 0x40400000);
        }
        /* The distance caps at 0x3F00: 0x5000 gives 0x3F00 / 0x70 = 144. */
        GR_SEED(0xFFu);
        DSW(FIGHT_ACTORS + 2u * 0x20u) = 0x8004u;
        DSD(DS_001077E4) = 0; DSD(DS_00107878) = 0x5000u;
        fight_4cb18(entry, 3u, 0u, 1u);
        CHECK_EQ_INT((int)DSW(rec + 0x34u), 0x0090);
    }

    /* E: 0x4D898 directly (the hit test's box moves for characters 5 and
     * 6, 0x15B90's adjustment, so the move gates are called without it).
     * With +0x1C bit 6 set an accepted move returns 0 without a write; a
     * refused one returns 1. */
    {
        static const u8 ok_ch[7] = { 0u, 0u, 4u, 5u, 1u, 6u, 3u };
        static const u8 ok_mv[7] = { 0x2Du, 0x0Au, 0x0Au, 0x0Au, 0x0Bu, 0x0Bu, 0x2Du };
        static const u8 no_ch[8] = { 0u, 1u, 2u, 3u, 3u, 6u, 4u, 5u };
        static const u8 no_mv[8] = { 0x0Bu, 0x0Au, 0x0Au, 0x0Au, 0x0Bu, 0x0Au, 0x0Bu, 0x0Bu };
        for (i = 0; i < 7u; i++) {
            GR_SEED(0xC5u);
            GR_CHAR(ok_ch[i]);
            DSB(DS_001077B0 + 0x5Fu) = ok_mv[i];
            CHECK_EQ_INT(fight_4d898(1u, entry, 3u), 0);
            CHECK_EQ_INT((int)DSB(entry + 0x1Cu), 0xC5);
            CHECK_EQ_INT((int)DSB(entry + 0x20u), 1);
        }
        for (i = 0; i < 8u; i++) {
            GR_SEED(0xC5u);
            GR_CHAR(no_ch[i]);
            DSB(DS_001077B0 + 0x5Fu) = no_mv[i];
            CHECK_EQ_INT(fight_4d898(1u, entry, 3u), 1);
        }
        /* The accepted move's return-0 gates (bit 6 clear): +0x52 2 or 7
         * and +0x4B set grab nothing; +0x52 3 and 6 grab (type 8). */
        {
            static const u8 st52[5] = { 2u, 7u, 4u, 3u, 6u };
            static const u8 st4b[5] = { 0u, 0u, 1u, 0u, 0u };
            for (i = 0; i < 5u; i++) {
                GR_SEED(0x85u);
                DSB(fr0 + 0x52u) = st52[i];
                DSB(fr0 + 0x4Bu) = st4b[i];
                CHECK_EQ_INT(fight_4d898(1u, entry, 3u), 0);
                CHECK_EQ_INT((int)DSB(entry + 0x1Eu), i < 3u ? 4 : 8);
                CHECK_EQ_INT((int)DSB(entry + 0x1Cu), i < 3u ? 0x85 : 0xC5);
            }
        }
        /* Hit 2 reads side 1's slot: its move 0xA with character 4. */
        GR_SEED(0xC5u);
        DSB(slot1 + 0x7Au) = 4;
        DSB(slot1 + 0x5Fu) = 0x0Au;
        DSB(DS_001077B0 + 0x5Fu) = 0;
        CHECK_EQ_INT(fight_4d898(2u, entry, 3u), 0);
        CHECK_EQ_INT(fight_4d898(1u, entry, 3u), 1);
    }
    /* Through 0x4D7A4: the accepted move leaves the entry alone (type 4, no
     * count); a refused one tramples (type 6, +0x20 = 0, +0x1F = 1). */
    GR_SEED(0xC5u);
    fight_4d7a4(entry, 3u);
    CHECK_EQ_INT((int)DSD(DS_00100B54), 2);
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 4);
    CHECK_EQ_INT((int)DSB(entry + 0x1Fu), 0);
    GR_SEED(0xC5u);
    DSB(DS_001077B0 + 0x5Fu) = 0x0Bu;
    fight_4d7a4(entry, 3u);
    CHECK_EQ_INT((int)DSD(DS_00100B54), 2);
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 6);
    CHECK_EQ_INT((int)DSB(entry + 0x20u), 0);
    CHECK_EQ_INT((int)DSB(entry + 0x1Fu), 1);
    /* No DS_00105B3A or other-side +0x54 gate (0x4B788 has both). */
    GR_SEED(0xC5u);
    DSB(DS_00105B3A) = 2;
    fight_4d7a4(entry, 3u);
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 4);
    GR_SEED(0xC5u);
    DSB(slot1 + 0x54u) = 3;
    fight_4d7a4(entry, 3u);
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 4);
    /* Bit 7 clear: no hit test. */
    GR_SEED(0x45u);
    DSB(DS_001077B0 + 0x5Fu) = 0;
    fight_4d7a4(entry, 3u);
    CHECK_EQ_INT((int)DSD(DS_00100B54), (int)0xDEADBEEFu);
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 4);
    /* Both sides count as side 0; a held actor (+0x4A = 2) is released
     * without 0x4B69C's DS_001088B2 store. */
    GR_SEED(0xC5u);
    ph_seed(p, p2, 1);
    DSD(ps5 + 4u) = (u32)x_hit; DSD(ps5 + 8u) = (u32)y_hit;
    DSB(DS_001077B0 + 0x5Fu) = 0;
    DSB(DS_001077B0 + 0x94u + 0x5Fu) = 0;
    DSB(rec + 0x4Au) = 2;
    fight_4d7a4(entry, 3u);
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 6);
    CHECK_EQ_INT((int)DSB(entry + 0x20u), 0);
    CHECK_EQ_INT((int)DSB(link2), 0);
    CHECK_EQ_INT((int)DSB(rec + 0x2Au), 0x04);
    CHECK_EQ_INT((int)DSB(rec + 0x29u), 0x10);
    CHECK_EQ_INT((int)DSB(rec + 0x4Au), 0);
    CHECK_EQ_INT((int)DSB(entry + 0x1Cu), 0x05);
    CHECK_EQ_INT((int)DSB(DS_001088AE + 1u), 6);
    CHECK_EQ_INT((int)DSB(DS_001088B2 + 1u), 0x33);

    /* 0x4D898's grab (character 0, 0x2D): as 0x4B788's, returning 0 (no
     * +0x1F count), then DS_00104B1A = 1's slot +0x5B gains 0x10 * 120 / 100
     * = 19 for the actor's +0x48 = 0x20 (si 3 still picks the stream). */
    GR_SEED(0x85u);
    DSB(rec + 0x48u) = 0x20u;
    fight_4d7a4(entry, 3u);
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 8);
    CHECK_EQ_INT((int)DSB(entry + 0x1Cu), 0xC5);
    CHECK_EQ_INT((int)DSB(entry + 0x20u), 0);
    CHECK_EQ_INT((int)DSB(entry + 0x1Fu), 0);
    CHECK_EQ_INT((int)DSD(entry + 0x10u), 0);
    CHECK_EQ_INT((int)(DSB(sh_fake + 0x28u) & 0x08u), 0x08);
    CHECK_EQ_INT((int)DSB(rec + 0x29u), 0x00);
    CHECK_EQ_INT((int)DSD(rec + 0x18u), 0x10000 - 0x55 * 64);
    CHECK_EQ_INT((int)DSD(rec + 0x1Cu), 3 * 64 + 0x800);
    CHECK_EQ_INT((int)DSW(rec + 0x32u), 0x0800);
    CHECK_EQ_INT((int)DSW(rec + 0x2Cu), 0x0D80);
    CHECK_EQ_INT((int)DSW(rec + 0x34u), 0);
    CHECK_EQ_INT((int)DSW(rec + 0x36u), 0);
    CHECK_EQ_INT((int)DSW(rec + 0x38u), 0);
    CHECK_EQ_INT((int)DSD(rec + 0x08u), (int)st_hold0);
    CHECK_EQ_INT((int)DSD(rec + 0x24u), 0);
    CHECK_EQ_INT((int)DSB(rec + 0x4Au), 1);
    CHECK_EQ_INT((int)DSB(link1), 5);
    CHECK_EQ_INT((int)DSD(fr0 + 0x08u), 0x000E7B02);
    CHECK_EQ_INT((int)DSD(fr0 + 0x24u), 0x40400000);
    CHECK_EQ_INT((int)DSB(slot1 + 0x5Bu), 0x10 + 19);
    CHECK_EQ_INT((int)DSB(DS_001077B0 + 0x5Bu), 0x33);
    /* Flipped: +0x29 bit 6 and the mirrored x. */
    GR_SEED(0x85u);
    DSW(fr0 + 0x28u) = 0x4000u;
    fight_4d7a4(entry, 3u);
    CHECK_EQ_INT((int)DSB(rec + 0x29u), 0x40);
    CHECK_EQ_INT((int)DSD(rec + 0x18u), 0x10000 + 0x55 * 64);
    /* The 0x4D880 weights by +0x48 - 0x20 (0x1F wraps to 0xFF: the default). */
    {
        static const u8 k48[8] = { 0x20u, 0x21u, 0x22u, 0x23u, 0x24u, 0x25u, 0x26u, 0x1Fu };
        static const int add[8] = { 19, 16, 25, 14, 12, 15, 15, 15 };
        for (i = 0; i < 8u; i++) {
            GR_SEED(0x85u);
            DSB(rec + 0x48u) = k48[i];
            fight_4d7a4(entry, 3u);
            CHECK_EQ_INT((int)DSB(slot1 + 0x5Bu), 0x10 + add[i]);
        }
    }
    /* The cap 0x78; the 0xA/0xB moves add 1 (capped the same). */
    GR_SEED(0x85u);
    DSB(rec + 0x48u) = 0x20u;
    DSB(slot1 + 0x5Bu) = 0x70u;
    fight_4d7a4(entry, 3u);
    CHECK_EQ_INT((int)DSB(slot1 + 0x5Bu), 0x78);
    GR_SEED(0x85u);
    DSB(DS_001077B0 + 0x5Fu) = 0x0Au;
    fight_4d7a4(entry, 3u);
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 8);
    CHECK_EQ_INT((int)DSB(slot1 + 0x5Bu), 0x11);
    GR_SEED(0x85u);
    DSB(DS_001077B0 + 0x5Fu) = 0x0Au;
    DSB(slot1 + 0x5Bu) = 0x78u;
    fight_4d7a4(entry, 3u);
    CHECK_EQ_INT((int)DSB(slot1 + 0x5Bu), 0x78);
    /* Character 1 with 0xB: its offsets, 0xC97AC[1][3], 0xE4744 at 5.0. */
    GR_SEED(0x85u);
    GR_CHAR(1);
    DSB(DS_001077B0 + 0x5Fu) = 0x0Bu;
    fight_4d7a4(entry, 3u);
    CHECK_EQ_INT((int)DSB(entry + 0x1Eu), 8);
    CHECK_EQ_INT((int)DSD(rec + 0x18u), 0x10000 - 0x57 * 64);
    CHECK_EQ_INT((int)DSD(rec + 0x1Cu), 0x0C * 64 + 0x800);
    CHECK_EQ_INT((int)DSD(rec + 0x08u), (int)st_hold1);
    CHECK_EQ_INT((int)DSD(fr0 + 0x08u), 0x000E4744);
    CHECK_EQ_INT((int)DSD(fr0 + 0x24u), 0x40A00000);
    CHECK_EQ_INT((int)DSB(slot1 + 0x5Bu), 0x11);
#undef GR_FREE_SPAWNS
#undef GR_SEED
#undef GR_CHAR
#undef GR_SECOND

    gr_unpatch();
    DSB(link2) = sv_link2;
    DSD(DS_000C955C + 12u) = sv_c955c;
    /* The spawns' side effects outside the fixture: 0x1015E8 is render node
     * 21's pset field (the 8-byte node pool at DS_0010153C) and 0x10763C is
     * the refcount of the palette table's third entry (DS_00107618 + 0x20). */
    DSD(0x001015E8u) = sv_15e8;
    DSW(0x0010763Cu) = sv_763c;
    DSD(0x000C9604u + 12u) = sv_tumble;
    DSB(DS_00105B3A) = sv_3a;
    DSD(DS_000EF6D8) = sv_rng;
    tf_put(sv_88, 0x00108840u, sizeof sv_88);
    tf_put(sv_4a, 0x00104AB8u, sizeof sv_4a);
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
    /* Record §38: 0x20DF4's 0x34978 (0x20E42) zeroes the live-fighter count
     * DS_001078FA and the word DS_001078F6 before the two spawns. Seeded with
     * the first demo's leftover count 2 and a sentinel, the spawns must count
     * 0 -> 2 (4 without the reset) and the word must read 0. */
    DSB(DS_001078FA) = 2;
    DSW(DS_001078F6) = 0x1234;
    /* Record §41-D: 0x20DF4's 0x28E98 (0x20E3D) builds the type-0x0A/0x19
     * node lists; 0xA5 over the nodes and both sentinels differs from every
     * link it writes. */
    mem_fill(DS_00104780, 0xA5u, 0x110u);
    /* Record §46-B: state 6 runs 0x20DF4 whole, so its stores the port once
     * left as a gap land too: dword 0xF0A48, byte 0x1088EC (the second
     * demo's state 6 sees 3 there), the 0x2C074 pair 0x105BF0/F4 and the
     * 0x2C390 sentinel 0x105C0C. 0xF0A48 is outside the windows test_fight
     * restores, so it is restored here; 0x1088EC (inside s_88) and the
     * 0x105BF0..0x105C0C words (inside s_5b) are restored there. 0x1088EC is
     * also put back here so this check leaves it as it found it. */
    u32 s_f0a48 = DSD(DS_000F0A48);
    u8 s_1088ec = DSB(DS_001088EC);
    DSD(DS_000F0A48) = 0xDEADBEEFu;
    DSB(DS_001088EC) = 3u;
    DSD(DS_00105BF0) = 0xDEADBEEFu;
    DSD(DS_00105BF4) = 0xDEADBEEFu;
    DSD(DS_00105C0C) = 0xDEADBEEFu;
    rng_seed(0x1234u);
    game_state_step();

    CHECK_EQ_INT((int)DSD(DS_000F0A48), 0);                   /* 0x20DFB */
    CHECK_EQ_INT((int)DSB(DS_001088EC), 0);                   /* 0x20E22 */
    CHECK_EQ_INT((int)DSD(DS_00105BF0), 0);                   /* 0x2C074 */
    CHECK_EQ_INT((int)DSD(DS_00105BF4), 0);
    CHECK_EQ_INT((int)DSD(DS_00105C0C), (int)DS_00105C0C);    /* 0x2C390 */
    DSD(DS_000F0A48) = s_f0a48;
    DSB(DS_001088EC) = s_1088ec;
    CHECK_EQ_INT((int)DSD(DS_000F0AE0), (int)DS_000F0AE0);   /* 0x12750 */
    CHECK_EQ_INT((int)DSD(DS_000F0AE4), (int)DS_000F0AE0);
    CHECK_EQ_INT((int)DSD(DS_000F0A78), 0x000F0A80);
    CHECK_EQ_INT((int)DSD(DS_000F0A7C), 0x000F0AD4);
    CHECK_EQ_INT((int)DSD(DS_00104880), (int)DS_00104880);   /* 0x28E98 */
    CHECK_EQ_INT((int)DSD(DS_00104884), (int)DS_00104880);
    CHECK_EQ_INT((int)DSD(DS_00104888), 0x00104780);
    CHECK_EQ_INT((int)DSD(DS_0010488C), 0x00104870);
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
    CHECK_EQ_INT((int)DSW(DS_001078F6), 0);

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

/* 0x34978 (record §38): the two slot pointers DS_001077A8[0..1] (0x654C7 with
 * ECX = 2 dwords), the word DS_001078F6 and the byte DS_001078FA go to 0; the
 * neighbours DS_001077A4, the slot at DS_001077B0, DS_001078F4, DS_001078F8
 * and DS_001078FB keep their sentinels (a three-dword fill or a dword store at
 * 0x1078F8/0x1078FA would change them). */
static void check_slots_reset(void)
{
    u8 s_a[0x10], s_f[0x08];
    tf_snap(s_a, DS_001077A8 - 4u, sizeof s_a);
    tf_snap(s_f, DS_001078F6 - 2u, sizeof s_f);
    mem_fill(DS_001077A8 - 4u, 0x5Au, sizeof s_a);
    mem_fill(DS_001078F6 - 2u, 0xA5u, sizeof s_f);

    fighter_slots_reset();

    CHECK_EQ_INT((int)DSD(DS_001077A8), 0);
    CHECK_EQ_INT((int)DSD(DS_001077A8 + 4u), 0);
    CHECK_EQ_INT((int)DSW(DS_001078F6), 0);
    CHECK_EQ_INT((int)DSB(DS_001078FA), 0);
    CHECK_EQ_INT((int)DSD(DS_001077A8 - 4u), 0x5A5A5A5A);
    CHECK_EQ_INT((int)DSD(DS_001077B0), 0x5A5A5A5A);
    CHECK_EQ_INT((int)DSW(DS_001078F6 - 2u), 0xA5A5);
    CHECK_EQ_INT((int)DSW(DS_001078F6 + 2u), 0xA5A5);
    CHECK_EQ_INT((int)DSB(DS_001078FA + 1u), 0xA5);

    tf_put(s_a, DS_001077A8 - 4u, sizeof s_a);
    tf_put(s_f, DS_001078F6 - 2u, sizeof s_f);
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

/* Both slots on sentinels for 0x12FD8: bit 3 of +0x42 clear and DS_001077A8
 * empty, so 0x18714 returns +0x2C - DS_00100AB0[side * 8] (0x10 / 0x30). */
static void ph_pair_seed(u32 x0, u32 l0, u32 x1, u32 l1, u32 mid, u32 cam)
{
    u32 s0 = DS_001077B0, s1 = DS_001077B0 + 0x94u;
    fight_reset_recs();
    DSD(DS_001077A8) = 0;
    DSD(DS_001077A8 + 4u) = 0;
    DSB(s0 + 0x42u) = 0;
    DSB(s1 + 0x42u) = 0;
    DSD(s0 + 0x20u) = 0;
    DSD(s1 + 0x20u) = 0;
    DSD(DS_00100AF0) = 0;
    DSD(DS_00100AF4) = 0;
    DSD(DS_00100AB0) = 0x10u;
    DSD(DS_00100AB0 + 8u) = 0x30u;
    DSD(s0 + 0x34u) = x0;
    DSD(s0 + 0x38u) = l0;
    DSD(s1 + 0x34u) = x1;
    DSD(s1 + 0x38u) = l1;
    DSD(s0 + 0x2Cu) = 0xDEADu;
    DSD(s1 + 0x2Cu) = 0xBEEFu;
    DSD(FIGHT_RECS + 0x18u) = 0x5A5Au;
    DSD(FIGHT_RECS + 0x118u) = 0xA5A5u;
    DSD(DS_00108884) = mid;
    DSD(DS_000F0AF0) = cam;
}

/* 0x12FD8 and the 0x2545C mode tail (record §42-D). The camera is called
 * directly for its bands and clamp, then game_frame runs each tail arm: 0x21
 * (0x3BB90 clears DS_00107D30, the held camera clamps to the centre + 0x1500,
 * both slots latch although DS_001077A8 is empty, and each pset x is taken
 * after the camera moved), 0x25 (the camera on the centre, no body push),
 * 0x22 (the dispatch clamp, only the DS_00104B1A slot) and 0x0C (the
 * DS_00104B12 slot's pset word on the frame word's bit 1). */
static void check_mode_tail(void)
{
    u32 s0 = DS_001077B0, s1 = DS_001077B0 + 0x94u;
    u32 r0 = FIGHT_RECS, r1 = FIGHT_RECS + 0x100u;
    /* Every range the seeds, tf_demo_fixture and game_frame write, saved
     * and restored whole. mem[0..0x1F] is pset 0, which actor_pset_point
     * writes for the out-of-pool scratch records. A whole-memory diff around
     * this check (reverted), with sentinels planted in the slots and the
     * scalar globals first, found no other written byte. */
    static const u32 rg[][2] = {
        { 0x00000000u, 0x20u },  { 0x0009AD50u, 0x10u },
        { 0x000EF6D8u, 0x08u },  { 0x000F0A60u, 0xA0u },
        { 0x00100A70u, 0x100u }, { 0x001014E0u, 0x18u },
        { 0x00104AE0u, 0x50u },  { 0x00105BC0u, 0x1B0u },
        { 0x001077A0u, 0x170u }, { 0x00107D20u, 0x20u },
        { 0x00108840u, 0x100u },
    };
    static u8 sv[0x20 + 0x10 + 0x08 + 0xA0 + 0x100 + 0x18 + 0x50 + 0x1B0
                 + 0x170 + 0x20 + 0x100];
    u32 k, o = 0;
    for (k = 0; k < sizeof rg / sizeof rg[0]; k++) {
        tf_snap(sv + o, rg[k][0], rg[k][1]);
        o += rg[k][1];
    }

    /* A: AL = 0 puts the camera on the centre and pulls nothing. */
    ph_pair_seed(0x1000u, 0x1100u, 0x5000u, 0x4000u, 0x1000u, 0x7777u);
    camera_pair_hold(0x100u);                   /* only AL is read */
    CHECK_EQ_INT((int)DSD(DS_000F0AF0), 0x1000);
    CHECK_EQ_INT((int)DSD(s0 + 0x34u), 0x1000);
    CHECK_EQ_INT((int)DSD(FIGHT_RECS + 0x18u), 0x5A5A);

    /* B: centre 0x4000. The left slot 0 at 0x1000 is past 0x4000 - 0x2E80 and
     * its latch 0x1100 is right of it (moving left): pulled, its record x =
     * 0x1100 - 0x10. The right slot 1 at 0x5000 is past 0x4A80 and inside
     * 0x6E80: kept. The camera 0x7000 clamps to 0x5500. */
    ph_pair_seed(0x1000u, 0x1100u, 0x5000u, 0x4000u, 0x4000u, 0x7000u);
    camera_pair_hold(1u);
    CHECK_EQ_INT((int)DSD(s0 + 0x34u), 0x1100);
    CHECK_EQ_INT((int)DSD(s0 + 0x2Cu), 0x1100);
    CHECK_EQ_INT((int)DSD(r0 + 0x18u), 0x10F0);
    CHECK_EQ_INT((int)DSD(s1 + 0x34u), 0x5000);
    CHECK_EQ_INT((int)DSD(s1 + 0x2Cu), 0xBEEF);
    CHECK_EQ_INT((int)DSD(r1 + 0x18u), 0xA5A5);
    CHECK_EQ_INT((int)DSD(DS_000F0AF0), 0x5500);
    /* B2: the same far slot moving right (latch 0xF00) is kept. */
    ph_pair_seed(0x1000u, 0x0F00u, 0x5000u, 0x4000u, 0x4000u, 0x4000u);
    camera_pair_hold(1u);
    CHECK_EQ_INT((int)DSD(s0 + 0x34u), 0x1000);
    CHECK_EQ_INT((int)DSD(r0 + 0x18u), 0x5A5A);
    /* The far band's edge: 0x117F is past 0x1180 (pulled), 0x1180 is not. */
    ph_pair_seed(0x117Fu, 0x1200u, 0x5000u, 0x4000u, 0x4000u, 0x4000u);
    camera_pair_hold(1u);
    CHECK_EQ_INT((int)DSD(s0 + 0x34u), 0x1200);
    ph_pair_seed(0x1180u, 0x1200u, 0x5000u, 0x4000u, 0x4000u, 0x4000u);
    camera_pair_hold(1u);
    CHECK_EQ_INT((int)DSD(s0 + 0x34u), 0x1180);
    /* B3: near the centre (0x3800 >= 0x3580) and moving right (latch 0x3700):
     * pulled. B4: 0x2000 is in neither band: kept. */
    ph_pair_seed(0x3800u, 0x3700u, 0x5000u, 0x4000u, 0x4000u, 0x4000u);
    camera_pair_hold(1u);
    CHECK_EQ_INT((int)DSD(s0 + 0x34u), 0x3700);
    CHECK_EQ_INT((int)DSD(r0 + 0x18u), 0x36F0);
    ph_pair_seed(0x2000u, 0x3700u, 0x5000u, 0x4000u, 0x4000u, 0x4000u);
    camera_pair_hold(1u);
    CHECK_EQ_INT((int)DSD(s0 + 0x34u), 0x2000);
    /* C: the right slot far (0x7000 > 0x6E80) moving right (latch 0x6F00):
     * pulled, its record x = 0x6F00 - 0x30. C2: near (0x4800 <= 0x4A80) and
     * moving left (latch 0x4900): pulled. */
    ph_pair_seed(0x2000u, 0x2000u, 0x7000u, 0x6F00u, 0x4000u, 0x4000u);
    camera_pair_hold(1u);
    CHECK_EQ_INT((int)DSD(s1 + 0x34u), 0x6F00);
    CHECK_EQ_INT((int)DSD(s1 + 0x2Cu), 0x6F00);
    CHECK_EQ_INT((int)DSD(r1 + 0x18u), 0x6ED0);
    CHECK_EQ_INT((int)DSD(s0 + 0x2Cu), 0xDEAD);
    ph_pair_seed(0x2000u, 0x2000u, 0x4800u, 0x4900u, 0x4000u, 0x4000u);
    camera_pair_hold(1u);
    CHECK_EQ_INT((int)DSD(s1 + 0x34u), 0x4900);
    ph_pair_seed(0x2000u, 0x2000u, 0x4800u, 0x4700u, 0x4000u, 0x4000u);
    camera_pair_hold(1u);
    CHECK_EQ_INT((int)DSD(s1 + 0x34u), 0x4800);
    /* D: equal +0x34 (`setge`) makes slot 1 the left one: at 0x2000 it is in
     * no band, and slot 0, the right one, is near and moving left (latch
     * 0x2100): only slot 0 is pulled. */
    ph_pair_seed(0x2000u, 0x2100u, 0x2000u, 0x2200u, 0x4000u, 0x4000u);
    camera_pair_hold(1u);
    CHECK_EQ_INT((int)DSD(s0 + 0x34u), 0x2100);
    CHECK_EQ_INT((int)DSD(s1 + 0x34u), 0x2000);
    /* E: the clamp's low edge (0x1000 -> 0x4000 - 0x1500) and its signed
     * compares: centre -0x100 keeps the camera -0x1000. */
    ph_pair_seed(0x2000u, 0x2000u, 0x5000u, 0x5000u, 0x4000u, 0x1000u);
    camera_pair_hold(1u);
    CHECK_EQ_INT((int)DSD(DS_000F0AF0), 0x2B00);
    ph_pair_seed(0x2000u, 0x2000u, 0x5000u, 0x5000u, (u32)-0x100, (u32)-0x1000);
    camera_pair_hold(1u);
    CHECK_EQ_INT((int)DSD(DS_000F0AF0), -0x1000);

    /* F: game_frame in mode 0x21. The records' x 0x123/0x456, clean latches
     * (+0x42 bit 3). */
#define MT_SEED(mode) do {                                              \
        (void)tf_demo_fixture();                                        \
        DSB(DS_00104B1D) = 1;                                           \
        DSD(DS_00104B00) = (mode);                                      \
        DSW(DS_000F0A64) = 7;                                           \
        DSW(DS_000F0A6A) = 2;                                           \
        DSB(DS_00104B15) = 0;                                           \
        DSD(DS_00104AE8) = 0;                                           \
        DSD(DS_000F0AF0) = 0x7000u;                                     \
        DSD(DS_00108884) = 0x1000u;                                     \
        DSB(DS_00107D30) = 1u;                                          \
        DSB(s0 + 0x42u) = 0x08u; DSB(s1 + 0x42u) = 0x08u;               \
        DSB(s0 + 0x41u) = 0; DSB(s1 + 0x41u) = 0;                       \
        DSD(r0 + 0x18u) = 0x123u; DSD(r1 + 0x18u) = 0x456u;             \
        DSD(s0 + 0x2Cu) = 0xDEADu; DSD(s1 + 0x2Cu) = 0xBEEFu;           \
        DSD(r0 + 0x3Cu) = 0x5A5Au; DSD(r1 + 0x3Cu) = 0xA5A5u;           \
    } while (0)
    MT_SEED(0x21u);
    game_frame();
    CHECK_EQ_INT((int)DSB(DS_00107D30), 0);
    CHECK_EQ_INT((int)DSD(DS_000F0AF0), 0x2500);
    CHECK_EQ_INT((int)DSD(s0 + 0x2Cu), 0x123);
    CHECK_EQ_INT((int)DSD(s1 + 0x2Cu), 0x456);
    CHECK_EQ_INT((int)DSD(r0 + 0x3Cu), 0x623);
    CHECK_EQ_INT((int)DSD(r1 + 0x3Cu), 0x956);

    /* G: mode 0x25: no body push, the camera on the centre 0x1000. */
    MT_SEED(0x25u);
    game_frame();
    CHECK_EQ_INT((int)DSB(DS_00107D30), 1);
    CHECK_EQ_INT((int)DSD(DS_000F0AF0), 0x1000);
    CHECK_EQ_INT((int)DSD(s0 + 0x2Cu), 0x123);
    CHECK_EQ_INT((int)DSD(r1 + 0x3Cu), 0x1E56);

    /* H: modes 0x22/0x23: 0x12D48's clamp (camera mode 4: 0x7000 -> 0x5D00)
     * and only the DS_00104B1A slot. */
    MT_SEED(0x22u);
    DSB(0x00104B1Au) = 1u;
    game_frame();
    CHECK_EQ_INT((int)DSB(DS_00107D30), 1);
    CHECK_EQ_INT((int)DSD(DS_000F0AF0), 0x5D00);
    CHECK_EQ_INT((int)DSD(s0 + 0x2Cu), 0xDEAD);
    CHECK_EQ_INT((int)DSD(s1 + 0x2Cu), 0x456);
    CHECK_EQ_INT((int)DSD(r0 + 0x3Cu), 0x5A5A);
    CHECK_EQ_INT((int)DSD(r1 + 0x3Cu), (int)(0x456u + 0x2A00u - 0x5D00u));
    MT_SEED(0x23u);
    DSB(0x00104B1Au) = 0;
    game_frame();
    CHECK_EQ_INT((int)DSD(s0 + 0x2Cu), 0x123);
    CHECK_EQ_INT((int)DSD(s1 + 0x2Cu), 0xBEEF);

    /* I: mode 0x0C: frame word 1 -> 2 (bit 1) with slot 1 (+0x41 bit 0): its
     * record's pset word 0x8123 is saved and becomes 0x81E1; slot 0's pset
     * keeps its word. Frame 3 -> 4 and a clear bit 0 do nothing. */
    MT_SEED(0x0Cu);
    DSB(DS_00104B12) = 1u;
    DSB(s1 + 0x41u) = 0x01u;
    DSW(FIGHT_ACTORS + 0x20u) = 0x4321u;
    DSW(FIGHT_ACTORS + 0x40u) = 0x8123u;
    DSW(DS_00104AF6) = 0x7777u;
    DSW(DS_000EF6DC) = 1u;
    game_frame();
    CHECK_EQ_INT((int)DSW(DS_00104AF6), 0x8123);
    CHECK_EQ_INT((int)DSW(FIGHT_ACTORS + 0x40u), 0x81E1);
    CHECK_EQ_INT((int)DSW(FIGHT_ACTORS + 0x20u), 0x4321);
    CHECK_EQ_INT((int)DSD(DS_000F0AF0), 0x7000);
    CHECK_EQ_INT((int)DSD(s1 + 0x2Cu), 0xBEEF);
    DSW(FIGHT_ACTORS + 0x40u) = 0x0123u;
    DSW(DS_000EF6DC) = 3u;
    game_frame();
    CHECK_EQ_INT((int)DSW(FIGHT_ACTORS + 0x40u), 0x0123);
    DSB(s1 + 0x41u) = 0xFEu;
    DSW(DS_000EF6DC) = 1u;
    game_frame();
    CHECK_EQ_INT((int)DSW(FIGHT_ACTORS + 0x40u), 0x0123);
    CHECK_EQ_INT((int)DSW(DS_00104AF6), 0x8123);

    /* J: mode 0x24 takes no arm. */
    MT_SEED(0x24u);
    game_frame();
    CHECK_EQ_INT((int)DSB(DS_00107D30), 1);
    CHECK_EQ_INT((int)DSD(DS_000F0AF0), 0x7000);
    CHECK_EQ_INT((int)DSD(s0 + 0x2Cu), 0xDEAD);
#undef MT_SEED

    for (k = 0, o = 0; k < sizeof rg / sizeof rg[0]; k++) {
        tf_put(sv + o, rg[k][0], rg[k][1]);
        o += rg[k][1];
    }
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

/* 0x28E98 (record §41-D): both sentinels self-linked, then the sixteen nodes
 * 0x104780..0x104870 appended to the 0x104888 free list in address order
 * (0x249C0 inserts before the sentinel), so the free list's next is 0x104780
 * and its prev 0x104870. 0xA5 over the nodes, the sentinels and a 0x10-byte
 * margin on each side differs from every link; the nodes' +8/+0xC and the
 * margins must keep it (0x249C0 writes only {next; prev}). */
static void check_type_0a19_list_init(void)
{
    u8 s[0x130];
    tf_snap(s, 0x00104770u, sizeof s);
    mem_fill(0x00104770u, 0xA5u, sizeof s);

    actor_type_0a19_list_init();

    CHECK_EQ_INT((int)DSD(DS_00104880), (int)DS_00104880);
    CHECK_EQ_INT((int)DSD(DS_00104884), (int)DS_00104880);
    CHECK_EQ_INT((int)DSD(DS_00104888), (int)0x00104780u);
    CHECK_EQ_INT((int)DSD(DS_0010488C), (int)0x00104870u);
    for (u32 i = 0; i < 16u; i++) {
        u32 node = DS_00104780 + i * 0x10u;
        u32 next = i < 15u ? node + 0x10u : DS_00104888;
        u32 prev = i > 0u ? node - 0x10u : DS_00104888;
        CHECK_EQ_INT((int)DSD(node), (int)next);
        CHECK_EQ_INT((int)DSD(node + 4u), (int)prev);
        CHECK_EQ_INT((int)DSD(node + 8u), (int)0xA5A5A5A5u);
        CHECK_EQ_INT((int)DSD(node + 0xCu), (int)0xA5A5A5A5u);
    }
    for (u32 o = 0; o < 0x10u; o += 4u) {
        CHECK_EQ_INT((int)DSD(0x00104770u + o), (int)0xA5A5A5A5u);
        CHECK_EQ_INT((int)DSD(0x00104890u + o), (int)0xA5A5A5A5u);
    }

    tf_put(s, 0x00104770u, sizeof s);
}

/* 0x43818 (record §41-D): 0x4F228(0), 0x2BAF4(1), three 0x2AE14 spawns and
 * four 0x33754 acquires. Everything the call writes is saved and restored: the
 * data object, the two pools, the two offscreen buffers, the resource table
 * (a first resolve marks an entry loaded), the DAC and the aperture.
 * - The pool holds two live records beforehand, so only 0x2BAF4's rebuild
 *   makes the spawns land at base, base+0x68 and base+0xD0 with an active
 *   list of exactly three.
 * - The descriptors' flags 0x2800 skip the walk (bit 0x0800) and select the
 *   layer byte (bit 0x2000), so rec+0x49 carries a3 = ECX (0xE0, 0xE2, 0xE2)
 *   and rec+8 keeps desc[0], read from the data object.
 * - DS_0010814C/DS_00108150 are seeded 0xDEADBEEF.
 * - The palette table: 0x2BAF4's 0x336C0 clears it, the three spawns acquire
 *   their descriptors' handle (desc+0x10, read from the data) three times,
 *   then the four literals follow in the raw's order with refcount 1, each
 *   start the previous entry's start + len. A fifth entry's handle is seeded
 *   and must read 0 (the 0x336C0 clear, and no fifth acquire).
 * - The projection sentinels go to 0. 0x2BAF4 makes the same 0x4F228(0) call
 *   at 0x2BBC4, so this group cannot see the 0x43822 call itself: dropping
 *   it is an equivalent mutation (record §41-D). */
static void check_char_screen_setup(void)
{
    static u8 s_data[0x10B0D0u - 0x80000u];
    static u8 s_rec[0xEBA0u], s_pset[0x4880u];
    static u8 s_bufa[0xFA00u], s_bufb[0xFA00u], s_ap[0xFA00u];
    static u8 s_dac[sizeof gfx_dac];
    static u8 s_res[0x14u * 128u];
    u32 rec_pool = DSD(DS_001014F4), pset_pool = DSD(DS_001014EC);
    u32 bufa = DSD(DS_001014E8), bufb = DSD(DS_001014E4);
    u32 res_tab = DSD(DS_001014E0);
    u32 res_len = DSD(DS_001014F0) * 0x14u;
    CHECK(rec_pool != 0u && pset_pool != 0u && bufa != 0u && bufb != 0u,
          "0x43818 needs the pools and buffers");
    CHECK(res_len != 0u && res_len <= sizeof s_res, "the resource table fits");
    if (rec_pool == 0u || pset_pool == 0u || bufa == 0u || bufb == 0u ||
        res_len == 0u || res_len > sizeof s_res)
        return;
    tf_snap(s_data, 0x80000u, sizeof s_data);
    tf_snap(s_rec, rec_pool, sizeof s_rec);
    tf_snap(s_pset, pset_pool, sizeof s_pset);
    tf_snap(s_bufa, bufa, sizeof s_bufa);
    tf_snap(s_bufb, bufb, sizeof s_bufb);
    tf_snap(s_res, res_tab, res_len);
    memcpy(s_ap, gfx_aperture(), sizeof s_ap);
    memcpy(s_dac, gfx_dac, sizeof s_dac);

    actors_reset();
    (void)actor_alloc(0);
    (void)actor_alloc(0);
    DSD(DS_00107618 + 0x50u) = 0xDEADBEEFu;   /* a fifth entry's handle */
    DSD(DS_0010814C) = 0xDEADBEEFu;
    DSD(DS_00108150) = 0xDEADBEEFu;
    DSD(DS_00104AE8) = 0xFFFFFFFFu;
    DSB(DS_00107A54) = 1u;
    DSB(DS_00107A55) = 0x99u;
    DSW(DS_00107A3A) = 0x1234u;
    DSW(DS_00107A38) = 0x4321u;

    fight_char_screen_setup();

    CHECK_EQ_INT((int)DSB(DS_00107A54), 0);
    CHECK_EQ_INT((int)DSB(DS_00107A55), 0);
    CHECK_EQ_INT((int)DSW(DS_00107A3A), 0);
    CHECK_EQ_INT((int)DSW(DS_00107A38), 0);
    CHECK_EQ_INT((int)DSD(DS_00104AE8), 0);

    u32 r0 = rec_pool, r1 = rec_pool + ACTOR_REC_SIZE;
    u32 r2 = rec_pool + 2u * ACTOR_REC_SIZE;
    u32 n = 0;
    for (u32 r = actor_list_head(); r != 0; r = actor_next(r)) n++;
    CHECK_EQ_INT((int)n, 3);
    CHECK_EQ_INT((int)actor_list_head(), (int)r2);  /* 0x249B0: newest first */
    CHECK_EQ_INT((int)DSD(DS_0010814C), (int)r1);
    CHECK_EQ_INT((int)DSD(DS_00108150), (int)r2);
    CHECK_EQ_INT((int)DSD(r0 + 0x08u), (int)DSD(0x000C885Cu));
    CHECK_EQ_INT((int)DSD(r1 + 0x08u), (int)DSD(0x000C87F8u));
    CHECK_EQ_INT((int)DSD(r2 + 0x08u), (int)DSD(0x000C87F8u));
    CHECK_EQ_INT((int)DSD(r0 + 0x18u), 0);
    CHECK_EQ_INT((int)DSD(r0 + 0x1Cu), 0);
    CHECK_EQ_INT((int)DSB(r0 + 0x49u), 0xE0);
    CHECK_EQ_INT((int)DSD(r1 + 0x18u), 0x1500);
    CHECK_EQ_INT((int)DSD(r1 + 0x1Cu), 0x3900);
    CHECK_EQ_INT((int)DSB(r1 + 0x49u), 0xE2);
    CHECK_EQ_INT((int)DSD(r2 + 0x18u), 0x3F00);
    CHECK_EQ_INT((int)DSD(r2 + 0x1Cu), 0x3900);
    CHECK_EQ_INT((int)DSB(r2 + 0x49u), 0xE2);

    static const u32 lit[4] = { 0x098EC50Cu, 0x098EC514u, 0x008099ACu,
                                0x00809984u };
    u32 desc_hdl = DSD(0x000C885Cu + 0x10u);
    CHECK_EQ_INT((int)DSD(0x000C87F8u + 0x10u), (int)desc_hdl);
    CHECK_EQ_INT((int)DSD(DS_00107618), (int)desc_hdl);
    CHECK_EQ_INT((int)DSD(DS_00107618 + 4u), 3);
    CHECK_EQ_INT((int)DSD(DS_00107618 + 8u), 1);
    for (u32 k = 0; k < 4u; k++) {
        u32 e = DS_00107618 + (k + 1u) * 0x10u;
        const u32 *res = (const u32 *)res_resolve(lit[k]);
        CHECK(res != NULL, "each literal palette handle resolves");
        CHECK_EQ_INT((int)DSD(e), (int)lit[k]);
        CHECK_EQ_INT((int)DSD(e + 4u), 1);
        CHECK_EQ_INT((int)DSD(e + 8u), (int)(DSD(e - 8u) + DSD(e - 4u)));
        if (res != NULL) CHECK_EQ_INT((int)DSD(e + 12u), (int)*res);
    }
    CHECK_EQ_INT((int)DSD(DS_00107618 + 0x50u), 0);

    tf_put(s_data, 0x80000u, sizeof s_data);
    tf_put(s_rec, rec_pool, sizeof s_rec);
    tf_put(s_pset, pset_pool, sizeof s_pset);
    tf_put(s_bufa, bufa, sizeof s_bufa);
    tf_put(s_bufb, bufb, sizeof s_bufb);
    tf_put(s_res, res_tab, res_len);
    memcpy(gfx_aperture(), s_ap, sizeof s_ap);
    memcpy(gfx_dac, s_dac, sizeof s_dac);
}

/* Record §42-F: the character screen's entries 0x43738/0x444C8 and their
 * callees 0x43964, 0x43A08, 0x1D810, 0x1D7B8 and 0x2F528. */
static u32 chs_grid(u32 row, u32 col)
{
    return DSD(DS_00105F38 + row * 0xACu + col * 4u);
}

static u32 chs_pal_handle(u32 rec)
{
    u32 e = DSD(actor_pset(rec) + 0x18u);
    return e != 0u ? DSD(e) : 0u;
}

/* The seeds every run starts from: sentinels in each global the entries write,
 * the two marker slots holding live records, an effect count and lock the
 * 0x13DF0 clear must zero, and volumes the 0x2C8F0(-1) arm must replace. */
static void chs_seed(u8 ch0, u8 ch1, u8 b1f, u8 b173)
{
    actors_reset();
    DSD(DS_001028E0) = actor_alloc(0);
    DSD(DS_001028E0 + 4u) = actor_alloc(0);
    DSB(DS_00108166) = ch0;
    DSB(DS_00108166 + 1u) = ch1;
    DSB(DS_00104B1F) = b1f;
    DSB(DS_00108173) = b173;
    for (u32 o = 0; o < 8u; o += 4u) {
        DSD(DS_0010813C + o) = 0xDEADBEEFu;
        DSD(DS_00108144 + o) = 0xDEADBEEFu;
        DSD(DS_00108154 + o) = 0xDEADBEEFu;
        DSD(DS_0010815C + o) = 0xDEADBEEFu;
    }
    DSW(DS_00108170) = 0x7777u;
    DSB(DS_00108174) = 0x77u;
    DSW(DS_0010816C) = 0x7777u;
    DSD(DS_00104AE4) = 0xDEADBEEFu;
    DSB(0x00104B1Bu) = 0x77u;
    DSB(DS_00104B24) = 0x77u;
    DSB(DS_0009AF3C) = 1u;
    DSB(DS_0009AF3D) = 5u;
    config_field_set(0x2Au, 0u);
    config_field_set(0x35u, 0x00A1u);
    config_field_set(0x37u, 0x0041u);
    DSD(DS_000A2CB8) = 0xDEADBEEFu;
    DSD(DS_000A2CB4) = 0xDEADBEEFu;
    DSD(DS_00105F34) = 0x12345678u;
    DSD(DS_00105F38 + 1u * 0xACu + 0x13u * 4u) = 0xDEADBEEFu;
    DSD(DS_00105F38 + 1u * 0xACu + 0x15u * 4u) = 0xDEADBEEFu;
}

/* The prologue and tail both entries share. The seeded marker slots point at
 * base and base + 0x68, where 0x43818's rebuild then places its first two
 * records: without the 0x1D810 releases the later 0x1D7B8 would mark those
 * live records dead through the stale slot, so all three stay undead with
 * their descriptors' palette (0xC885C/0xC87F8 +0x10 = 0x098EC71C). */
static void chs_check_common(u32 base)
{
    for (u32 k = 0; k < 3u; k++) {
        u32 r = base + k * ACTOR_REC_SIZE;
        CHECK_EQ_INT((int)(DSB(r + 0x28u) & 8u), 0);
        CHECK_EQ_INT((int)chs_pal_handle(r), 0x098EC71C);
    }
    CHECK_EQ_INT((int)DSB(0x00104B1Bu), 0);
    CHECK_EQ_INT((int)DSB(DS_00104B24), 0);
    CHECK_EQ_INT((int)DSB(DS_0009AF3C), 0);             /* 0x13DF0 */
    CHECK_EQ_INT((int)DSB(DS_0009AF3D), 0);
    CHECK_EQ_INT((int)DSD(DS_000A2CB8), 0x50);          /* 0xA1 >> 1 */
    CHECK_EQ_INT((int)DSD(DS_000A2CB4), 0x20);          /* 0x41 >> 1 */
    CHECK_EQ_INT((int)DSD(DS_00104AE4), 0x29D60);
    CHECK_EQ_INT((int)DSB(DS_00108174), 0);
    CHECK_EQ_INT((int)DSD(DS_00108144), 0);
    CHECK_EQ_INT((int)DSD(DS_00108144 + 4u), 0);
    CHECK_EQ_INT((int)DSD(DS_00105F34), 0x12345678);
}

/* One filled side. `base` is the first of its four records (0x43964's two,
 * 0x43A08's, 0x1D7B8's, in spawn order). The expected values are the raw
 * tables' (tabs dumped from the data object, record §42-F): x/y 0xC8898/
 * 0xC88A6, sprite ids 0xC88DC, pset words 0xC88F8, palettes 0xC8908, the
 * class byte 0xC8882, the side actor 0xBB938 and the marker 0xA78B0. */
static void chs_check_side(u32 side, u32 base, u32 x, u32 y, u32 panel_id,
                          u32 panel_word, u32 panel_pal, u32 act_pal,
                          u32 act_2c, u32 marker_id)
{
    u32 e = base, p = base + ACTOR_REC_SIZE;
    u32 a = base + 2u * ACTOR_REC_SIZE, m = base + 3u * ACTOR_REC_SIZE;
    CHECK_EQ_INT((int)DSB(DS_00108170 + side), 1);
    CHECK_EQ_INT((int)DSD(DS_00108154 + side * 4u), (int)e);
    CHECK_EQ_INT((int)DSD(DS_0010815C + side * 4u), (int)p);
    CHECK_EQ_INT((int)DSD(DS_0010813C + side * 4u), (int)a);
    CHECK_EQ_INT((int)DSD(DS_001028E0 + side * 4u), (int)m);
    /* 0x43964's entry: 0xC8870[side] = 0xC880C/0xC8820, ids 0x333/0x335. */
    CHECK_EQ_INT((int)DSD(e + 0x08u), side == 0u ? 0x333 : 0x335);
    CHECK_EQ_INT((int)DSD(e + 0x18u), (int)x);
    CHECK_EQ_INT((int)DSD(e + 0x1Cu), (int)y);
    CHECK_EQ_INT((int)DSB(e + 0x49u), 0xFF);
    CHECK_EQ_INT((int)chs_pal_handle(e), side == 0u ? 0x098EC50C : 0x098EC514);
    /* Its panel: 0xC8878[side], a3 0xFE at (0, 0), re-pointed and repaletted. */
    CHECK_EQ_INT((int)DSD(p + 0x08u), (int)panel_id);
    CHECK_EQ_INT((int)(DSW(actor_pset(p)) & 0x7FFFu), (int)panel_id);
    CHECK_EQ_INT((int)DSD(p + 0x18u), 0);
    CHECK_EQ_INT((int)DSD(p + 0x1Cu), 0);
    CHECK_EQ_INT((int)DSB(p + 0x49u), 0xFE);
    CHECK_EQ_INT((int)DSW(actor_pset(p) + 2u), (int)(panel_word | 0x800u));
    CHECK_EQ_INT((int)chs_pal_handle(p), (int)panel_pal);
    /* 0x43A08's side actor: 0xBB938[class][side]; the side's desc[1] byte 2
     * (0x00000300 / 0x00040300) is rec+0x2E, the class's desc+0xC rec+0x2C. */
    CHECK_EQ_INT((int)DSD(a + 0x18u), side == 0u ? 0x1500 : 0x3F00);
    CHECK_EQ_INT((int)DSD(a + 0x1Cu), 0x3200);
    CHECK_EQ_INT((int)DSB(a + 0x49u), 0xFF);
    CHECK_EQ_INT((int)DSB(a + 0x4Du), 0x1E);
    CHECK_EQ_INT((int)(DSW(a + 0x28u) & 0x4000u), side == 0u ? 0x4000 : 0);
    CHECK_EQ_INT((int)DSW(a + 0x2Eu), side == 0u ? 0 : 4);
    CHECK_EQ_INT((int)DSW(a + 0x2Cu), (int)act_2c);
    CHECK_EQ_INT((int)chs_pal_handle(a), (int)act_pal);
    /* 0x1D7B8's marker: 0xA78B0[class], a3 0xFD, y = 0x43A08's EBX 0x1800. */
    CHECK_EQ_INT((int)DSD(m + 0x08u), (int)marker_id);
    CHECK_EQ_INT((int)DSD(m + 0x18u), side == 0u ? 0x200 : 0x4200);
    CHECK_EQ_INT((int)DSD(m + 0x1Cu), 0x1800);
    CHECK_EQ_INT((int)DSB(m + 0x49u), 0xFD);
    /* 0x43818's per-side record, re-pointed at 0x32B. */
    CHECK_EQ_INT((int)DSD(DSD(DS_0010814C + side * 4u) + 0x08u), 0x32B);
}

static u32 chs_active_count(void)
{
    u32 n = 0;
    for (u32 r = actor_list_head(); r != 0; r = actor_next(r)) n++;
    return n;
}

/* Everything the entries write is saved and restored, as for 0x43818: the
 * data object (every global, the config fields, the effect pool, the text
 * grid), both pools, both offscreen buffers, the resource table, the DAC and
 * the aperture. Each run starts from the snapshot. The pool rebuild in 0x43818
 * makes the spawns land at base + k * 0x68 in call order: 0x43818's three,
 * then per filled side 0x43964's two, 0x43A08's one and 0x1D7B8's one, then
 * the countdown's glyphs. No RNG draw is expected on any path. */
static void check_char_screen_open(void)
{
    static u8 s_data[0x10B0D0u - 0x80000u];
    static u8 s_rec[0xEBA0u], s_pset[0x4880u];
    static u8 s_bufa[0xFA00u], s_bufb[0xFA00u], s_ap[0xFA00u];
    static u8 s_dac[sizeof gfx_dac];
    static u8 s_res[0x14u * 128u];
    u32 rec_pool = DSD(DS_001014F4), pset_pool = DSD(DS_001014EC);
    u32 bufa = DSD(DS_001014E8), bufb = DSD(DS_001014E4);
    u32 res_tab = DSD(DS_001014E0);
    u32 res_len = DSD(DS_001014F0) * 0x14u;
    CHECK(rec_pool != 0u && pset_pool != 0u && bufa != 0u && bufb != 0u,
          "0x43738 needs the pools and buffers");
    CHECK(res_len != 0u && res_len <= sizeof s_res, "the resource table fits");
    CHECK(DSD(DS_000FCCE0) != 0u, "the effect pool is built (0x13DF0 runs)");
    if (rec_pool == 0u || pset_pool == 0u || bufa == 0u || bufb == 0u ||
        res_len == 0u || res_len > sizeof s_res)
        return;
    tf_snap(s_data, 0x80000u, sizeof s_data);
    tf_snap(s_rec, rec_pool, sizeof s_rec);
    tf_snap(s_pset, pset_pool, sizeof s_pset);
    tf_snap(s_bufa, bufa, sizeof s_bufa);
    tf_snap(s_bufb, bufb, sizeof s_bufb);
    tf_snap(s_res, res_tab, res_len);
    memcpy(s_ap, gfx_aperture(), sizeof s_ap);
    memcpy(s_dac, gfx_dac, sizeof s_dac);
    const u32 R = ACTOR_REC_SIZE;
    u32 rng0;

    /* The hook: 0x28D68/0x28D80 store 0x43738 (dwords 0x28D6A/0x28D87). */
    CHECK(fn_resolve(0x43738u) == fight_char_screen_open,
          "0x43738 resolves to its port");

    /* (a) 0x43738, DS_00104B1F = 2: side 1 only (its bit is side + 1 = 2),
     * character 6 (class 5); DS_00108173 != 0: the countdown is 5, drawn "05".
     * Side 0 keeps its sentinels, and its marker slot (a live record) is
     * zeroed by 0x1D810(0). */
    chs_seed(3u, 6u, 2u, 1u);
    rng0 = DSD(DS_000EF6D8);
    fight_char_screen_open();
    chs_check_common(rec_pool);
    CHECK_EQ_INT((int)DSD(DS_000EF6D8), (int)rng0);
    CHECK_EQ_INT((int)DSB(DS_00108170), 0);
    CHECK_EQ_INT((int)DSD(DS_00108154), (int)0xDEADBEEFu);
    CHECK_EQ_INT((int)DSD(DS_0010815C), (int)0xDEADBEEFu);
    CHECK_EQ_INT((int)DSD(DS_0010813C), (int)0xDEADBEEFu);
    CHECK_EQ_INT((int)DSD(DS_001028E0), 0);
    chs_check_side(1u, rec_pool + 3u * R, 0x2F40u, 0x0EC0u, 0x331u, 0x2Cu,
                  0x098ECC0Cu, 0x098ECC0Cu, 0x0A00u, 0x402Fu);
    CHECK_EQ_INT((int)DSD(DSD(DS_0010814C) + 0x08u), 0x32A);  /* side 0 as 0x43818 left it */
    CHECK_EQ_INT((int)DSW(DS_0010816C), 5);
    /* 0x2F528: "05" in the 0xBD048 font at row 1: '0' (class 0, sprite
     * 0x3FA0) at col 0x13; the glyphs are 16 wide, so '5' (0x3FA5) at 0x15. */
    CHECK_EQ_INT((int)chs_grid(1u, 0x13u), (int)(rec_pool + 7u * R));
    CHECK_EQ_INT((int)chs_grid(1u, 0x14u), 0);
    CHECK_EQ_INT((int)chs_grid(1u, 0x15u), (int)(rec_pool + 8u * R));
    CHECK_EQ_INT((int)DSD(rec_pool + 7u * R + 0x08u), 0x3FA0);
    CHECK_EQ_INT((int)DSD(rec_pool + 8u * R + 0x08u), 0x3FA5);
    CHECK_EQ_INT((int)chs_pal_handle(rec_pool + 7u * R), 0x008099AC);
    CHECK_EQ_INT((int)chs_active_count(), 9);

    /* (b) 0x43738, DS_00104B1F = 0xFD: side 0 only (bit 1 set, bit 2 clear),
     * character 3 (class 1); DS_00108173 == 0: the countdown is 0xF, "15". */
    tf_put(s_data, 0x80000u, sizeof s_data);
    tf_put(s_rec, rec_pool, sizeof s_rec);
    tf_put(s_pset, pset_pool, sizeof s_pset);
    chs_seed(3u, 6u, 0xFDu, 0u);
    rng0 = DSD(DS_000EF6D8);
    fight_char_screen_open();
    chs_check_common(rec_pool);
    CHECK_EQ_INT((int)DSD(DS_000EF6D8), (int)rng0);
    chs_check_side(0u, rec_pool + 3u * R, 0x3580u, 0x0540u, 0x32Du, 0x18u,
                  0x098EC18Cu, 0x098EC18Cu, 0x0C00u, 0x402Du);
    CHECK_EQ_INT((int)DSB(DS_00108170 + 1u), 0);
    CHECK_EQ_INT((int)DSD(DS_00108154 + 4u), (int)0xDEADBEEFu);
    CHECK_EQ_INT((int)DSD(DS_0010815C + 4u), (int)0xDEADBEEFu);
    CHECK_EQ_INT((int)DSD(DS_0010813C + 4u), (int)0xDEADBEEFu);
    CHECK_EQ_INT((int)DSD(DS_001028E0 + 4u), 0);
    CHECK_EQ_INT((int)DSW(DS_0010816C), 0xF);
    CHECK_EQ_INT((int)DSD(chs_grid(1u, 0x13u) + 0x08u), 0x3FA1);
    CHECK_EQ_INT((int)DSD(chs_grid(1u, 0x15u) + 0x08u), 0x3FA5);
    CHECK_EQ_INT((int)chs_active_count(), 9);

    /* (c) 0x444C8: both sides whatever DS_00104B1F holds (0), characters 5
     * (class 4) and 0 (class 0); no countdown: DS_0010816C keeps its sentinel
     * and the countdown cells stay empty (0x43818's 0x2BAF4 cleared the
     * seeded grid). */
    tf_put(s_data, 0x80000u, sizeof s_data);
    tf_put(s_rec, rec_pool, sizeof s_rec);
    tf_put(s_pset, pset_pool, sizeof s_pset);
    chs_seed(5u, 0u, 0u, 1u);
    rng0 = DSD(DS_000EF6D8);
    fight_char_screen_open_both();
    chs_check_common(rec_pool);
    CHECK_EQ_INT((int)DSD(DS_000EF6D8), (int)rng0);
    chs_check_side(0u, rec_pool + 3u * R, 0x22C0u, 0x0EC0u, 0x330u, 0x28u,
                  0x098ECD0Cu, 0x098ECD0Cu, 0x0C00u, 0x402Cu);
    chs_check_side(1u, rec_pool + 7u * R, 0x1000u, 0x0540u, 0x32Cu, 0x1Cu,
                  0x098ECC8Cu, 0x098ECC8Cu, 0x0C00u, 0x4030u);
    CHECK_EQ_INT((int)DSW(DS_0010816C), 0x7777);
    CHECK_EQ_INT((int)chs_grid(1u, 0x13u), 0);
    CHECK_EQ_INT((int)chs_grid(1u, 0x15u), 0);
    CHECK_EQ_INT((int)chs_active_count(), 11);

    /* (d) The callees alone. 0x1D810 on an empty slot touches nothing; on a
     * live record it sets the dead bit (+0x28 bit 3) and zeroes the slot.
     * 0x43A08's +0x29 |= 8 is invisible after 0x43818 (desc 0xC87F8's flags
     * 0x2800 already carry it), so it is checked on a cleared bit. */
    tf_put(s_data, 0x80000u, sizeof s_data);
    tf_put(s_rec, rec_pool, sizeof s_rec);
    tf_put(s_pset, pset_pool, sizeof s_pset);
    actors_reset();
    {
        u32 r = actor_alloc(0);
        DSD(DS_001028E0) = 0u;
        DSD(DS_001028E0 + 4u) = r;
        DSB(r + 0x28u) = 0u;
        fight_select_marker_release(0u);
        CHECK_EQ_INT((int)DSD(DS_001028E0), 0);
        CHECK_EQ_INT((int)DSD(DS_001028E0 + 4u), (int)r);
        CHECK_EQ_INT((int)DSB(r + 0x28u), 0);
        fight_select_marker_release(1u);
        CHECK_EQ_INT((int)DSD(DS_001028E0 + 4u), 0);
        CHECK_EQ_INT((int)(DSB(r + 0x28u) & 8u), 8);
    }
    fight_char_screen_setup();
    DSB(DS_00108166 + 1u) = 2u;                         /* class 3 */
    DSB(DSD(DS_00108150) + 0x29u) = 0u;
    fight_char_select_actor(1u);
    CHECK_EQ_INT((int)(DSB(DSD(DS_00108150) + 0x29u) & 8u), 8);
    CHECK_EQ_INT((int)DSD(DS_0010813C + 4u), (int)(rec_pool + 3u * R));
    CHECK_EQ_INT((int)chs_pal_handle(rec_pool + 3u * R), 0x098EC10C);
    CHECK_EQ_INT((int)DSD(DS_001028E0 + 4u), (int)(rec_pool + 4u * R));
    CHECK_EQ_INT((int)DSD(rec_pool + 4u * R + 0x08u), 0x4032);   /* 0xA7860 */
    /* 0x1D7B8 on a live marker (the one just spawned, palette 0x98ECBEC):
     * its release arm marks it dead and drops its pset palette before the
     * class-5 marker (0xA7888, id 0x402F) lands in the slot at the caller's
     * y. Every earlier 0x1D7B8 ran on a slot 0x1D810 had already emptied. */
    {
        u32 old = rec_pool + 4u * R;
        CHECK_EQ_INT((int)chs_pal_handle(old), 0x098ECBEC);
        CHECK_EQ_INT((int)(DSB(old + 0x28u) & 8u), 0);
        fight_select_marker_spawn(1u, 5u, 0x1234u);
        CHECK_EQ_INT((int)(DSB(old + 0x28u) & 8u), 8);
        CHECK_EQ_INT((int)DSD(actor_pset(old) + 0x18u), 0);
        CHECK_EQ_INT((int)DSD(DS_001028E0 + 4u), (int)(rec_pool + 5u * R));
        CHECK_EQ_INT((int)DSD(rec_pool + 5u * R + 0x08u), 0x402F);
        CHECK_EQ_INT((int)DSD(rec_pool + 5u * R + 0x18u), 0x4200);
        CHECK_EQ_INT((int)DSD(rec_pool + 5u * R + 0x1Cu), 0x1234);
    }

    tf_put(s_data, 0x80000u, sizeof s_data);
    tf_put(s_rec, rec_pool, sizeof s_rec);
    tf_put(s_pset, pset_pool, sizeof s_pset);
    tf_put(s_bufa, bufa, sizeof s_bufa);
    tf_put(s_bufb, bufb, sizeof s_bufb);
    tf_put(s_res, res_tab, res_len);
    memcpy(gfx_aperture(), s_ap, sizeof s_ap);
    memcpy(gfx_dac, s_dac, sizeof s_dac);
}

/* Record §43-B: the DS_00104AE4 hooks 0x28D68/0x28D80, the mode 0x1A arm
 * 0x4F980, the mode 0x1A/0x1B handlers 0x4F9A0/0x4F9C8 with their wipes
 * 0x4F9E4/0x4FA88, and 0x10D70. The same snapshot as check_char_screen_open,
 * whose seeds (chs_seed) and checks (chs_check_common/chs_check_side) the
 * 0x43738 the hook runs is held to. The wipe tables are the data object's:
 * descriptors 0xC98F4/0xC9908 (ids 0x2DD9/0x2DE9, flags 0x802800), sprite
 * words 0xC991C = 0x2DD9..0x2DE8, 0x3F13 and 0xC993E = 0x2DE9..0x2DF8, 0x3F14;
 * the word before 0xC991C (0xC991A, the high half of 0xC9908's palette
 * 0x3E638) is 3. */
static u16 cm_pset_word(u32 rec)
{
    return DSW(DSD(DS_001014EC) + (u32)DSW(rec + 0x56u) * 0x20u);
}

static void cm_seed_arm(void)
{
    DSD(0x00104AF8u) = 0xA5A5A5A5u;
    DSD(0x00104AFCu) = 0x5A5A5A5Au;
    DSD(DS_00104B00) = 0xBEEFBEEFu;
    DSB(DS_001088F4) = 0x66u;
    DSB(DS_001088F5) = 0x77u;
    DSB(0x001088F6u) = 0x55u;
    DSD(DS_00104AE4) = 0xDEADBEEFu;
}

/* The arm's stores: DS_001088F5 = 0, the word DS_00104AFA, the word
 * DS_00104B00 = 0x1A, with their neighbours intact. */
static void cm_check_arm(u32 ret_mode)
{
    CHECK_EQ_INT((int)DSB(DS_001088F5), 0);
    CHECK_EQ_INT((int)DSB(DS_001088F4), 0x66);
    CHECK_EQ_INT((int)DSB(0x001088F6u), 0x55);
    CHECK_EQ_INT((int)DSD(0x00104AF8u), (int)(0xA5A5u | (ret_mode << 16)));
    CHECK_EQ_INT((int)DSD(0x00104AFCu), (int)0x5A5A5A5Au);
    CHECK_EQ_INT((int)DSD(DS_00104B00), (int)0xBEEF001Au);
}

static void check_char_screen_modes(void)
{
    static u8 s_data[0x10B0D0u - 0x80000u];
    static u8 s_rec[0xEBA0u], s_pset[0x4880u];
    static u8 s_bufa[0xFA00u], s_bufb[0xFA00u], s_ap[0xFA00u];
    static u8 s_dac[sizeof gfx_dac];
    static u8 s_res[0x14u * 128u];
    u32 rec_pool = DSD(DS_001014F4), pset_pool = DSD(DS_001014EC);
    u32 bufa = DSD(DS_001014E8), bufb = DSD(DS_001014E4);
    u32 res_tab = DSD(DS_001014E0);
    u32 res_len = DSD(DS_001014F0) * 0x14u;
    CHECK(rec_pool != 0u && pset_pool != 0u && bufa != 0u && bufb != 0u,
          "the mode 0x1A chain needs the pools and buffers");
    CHECK(res_len != 0u && res_len <= sizeof s_res, "the resource table fits");
    if (rec_pool == 0u || pset_pool == 0u || bufa == 0u || bufb == 0u ||
        res_len == 0u || res_len > sizeof s_res)
        return;
    tf_snap(s_data, 0x80000u, sizeof s_data);
    tf_snap(s_rec, rec_pool, sizeof s_rec);
    tf_snap(s_pset, pset_pool, sizeof s_pset);
    tf_snap(s_bufa, bufa, sizeof s_bufa);
    tf_snap(s_bufb, bufb, sizeof s_bufb);
    tf_snap(s_res, res_tab, res_len);
    memcpy(s_ap, gfx_aperture(), sizeof s_ap);
    memcpy(s_dac, gfx_dac, sizeof s_dac);
    const u32 R = ACTOR_REC_SIZE;

    /* (a) Both hooks are DS_00104AE4 values (code immediates only). */
    CHECK(fn_resolve(0x28D68u) == frontend_char_screen_hook,
          "0x28D68 resolves to its port");
    CHECK(fn_resolve(0x28D80u) == frontend_char_screen_hook_voice,
          "0x28D80 resolves to its port");

    /* (b) 0x4F980 alone, then each hook: the hook becomes 0x43738 and the arm
     * returns to 0x10. */
    cm_seed_arm();
    frontend_wipe_arm(0x1234u);
    cm_check_arm(0x1234u);
    CHECK_EQ_INT((int)DSD(DS_00104AE4), (int)0xDEADBEEFu);
    cm_seed_arm();
    frontend_char_screen_hook();
    cm_check_arm(0x10u);
    CHECK_EQ_INT((int)DSD(DS_00104AE4), 0x43738);
    cm_seed_arm();
    frontend_char_screen_hook_voice();
    cm_check_arm(0x10u);
    CHECK_EQ_INT((int)DSD(DS_00104AE4), 0x43738);

    /* (c) 0x10D70 alone: bit 2 of +0x28 cleared, bit 15 from +0x28 bit 14
     * (set, then cleared on a word that has it), the pset's +2 untouched. */
    actors_reset();
    {
        u32 r = actor_alloc(0);
        u32 ps = DSD(DS_001014EC) + (u32)DSW(r + 0x56u) * 0x20u;
        DSW(ps) = 0xFFFFu;
        DSW(ps + 2u) = 0xABCDu;
        DSW(r + 0x28u) = 0x4004u;
        actor_pset_word_set(r, 0x1234u);
        CHECK_EQ_INT((int)DSW(ps), 0x9234);
        CHECK_EQ_INT((int)DSW(r + 0x28u), 0x4000);
        CHECK_EQ_INT((int)DSW(ps + 2u), 0xABCD);
        DSW(r + 0x28u) = 0x2804u;
        actor_pset_word_set(r, 0x8123u);
        CHECK_EQ_INT((int)DSW(ps), 0x0123);
        CHECK_EQ_INT((int)DSW(r + 0x28u), 0x2800);
    }

    /* (d) The whole chain from 0x28D68: 17 wipe-in frames, the 18th runs
     * 0x43738 (as check_char_screen_open's run (a): side 1, character 6),
     * then 17 wipe-out frames and the 18th returns to mode 0x10. */
    tf_put(s_data, 0x80000u, sizeof s_data);
    tf_put(s_rec, rec_pool, sizeof s_rec);
    tf_put(s_pset, pset_pool, sizeof s_pset);
    chs_seed(3u, 6u, 2u, 1u);
    DSD(DS_000C98F0) = 0u;
    DSB(DS_001088F4) = 0x77u;
    DSB(DS_001088F5) = 0x77u;
    DSD(DS_00101508) = 0x123u;
    DSD(DS_0010150C) = 0x100u;
    frontend_char_screen_hook();
    CHECK_EQ_INT((int)DSW(DS_00104B00), 0x1A);
    frontend_mode_1a_step();                             /* frame 1: the spawn */
    u32 w = DSD(DS_000C98F0);
    CHECK(w != 0u, "0x4F9E4 spawned the wipe-in actor");
    CHECK_EQ_INT((int)DSD(w + 0x08u), 0x2DD9);           /* 0xC98F4 */
    CHECK_EQ_INT((int)DSB(w + 0x49u), 0xFF);
    CHECK_EQ_INT((int)DSD(DS_0010150C), 0x123);
    CHECK_EQ_INT((int)DSB(DS_001088F5), 1);
    CHECK_EQ_INT((int)(cm_pset_word(w) & 0x7FFFu), 0x2DD9);
    DSW(w + 0x28u) |= 0x4004u;
    DSD(DS_0010150C) = 0x200u;
    frontend_mode_1a_step();                             /* frame 2 */
    CHECK_EQ_INT((int)cm_pset_word(w), 0x2DDA | 0x8000);
    CHECK_EQ_INT((int)(DSB(w + 0x28u) & 4u), 0);
    for (u32 k = 3; k <= 17u; k++) frontend_mode_1a_step();
    CHECK_EQ_INT((int)DSB(DS_001088F5), 0x11);
    CHECK_EQ_INT((int)cm_pset_word(w), 0x3F13 | 0x8000);
    CHECK_EQ_INT((int)DSD(DS_000C98F0), (int)w);
    CHECK_EQ_INT((int)DSD(DS_0010150C), 0x200);          /* no resync */
    CHECK_EQ_INT((int)DSW(DS_00104B00), 0x1A);
    CHECK_EQ_INT((int)DSB(DS_001088F4), 0x77);
    CHECK_EQ_INT((int)DSD(DS_00104AE4), 0x43738);        /* the hook has not run */
    CHECK_EQ_INT((int)DSB(DS_00108174), 0x77);
    frontend_mode_1a_step();                             /* frame 18: done */
    CHECK_EQ_INT((int)DSD(DS_000C98F0), 0);
    CHECK_EQ_INT((int)DSB(DS_001088F4), 1);
    CHECK_EQ_INT((int)DSB(DS_001088F5), 0);
    CHECK_EQ_INT((int)DSW(DS_00104B00), 0x1B);
    CHECK_EQ_INT((int)DSW(DS_00104AFA), 0x10);
    chs_check_common(rec_pool);                          /* 0x43738 ran */
    chs_check_side(1u, rec_pool + 3u * R, 0x2F40u, 0x0EC0u, 0x331u, 0x2Cu,
                  0x098ECC0Cu, 0x098ECC0Cu, 0x0A00u, 0x402Fu);
    CHECK_EQ_INT((int)chs_active_count(), 9);
    /* 0x2BAF4's EAX = 1 arm (0x52106 with 0) zeroed both tick counters. */
    CHECK_EQ_INT((int)DSD(DS_0010150C), 0);
    CHECK_EQ_INT((int)DSD(DS_00101508), 0);

    DSD(DS_00101508) = 0x123u;
    DSD(DS_0010150C) = 0x300u;
    frontend_mode_1b_step();                             /* frame 1: the spawn */
    u32 w2 = DSD(DS_000C98F0);
    CHECK(w2 != 0u, "0x4FA88 spawned the wipe-out actor");
    CHECK_EQ_INT((int)DSD(w2 + 0x08u), 0x2DE9);          /* 0xC9908 */
    CHECK_EQ_INT((int)DSB(w2 + 0x49u), 0xFF);
    CHECK_EQ_INT((int)DSB(DS_001088F4), 0);
    CHECK_EQ_INT((int)DSD(DS_0010150C), 0x123);
    CHECK_EQ_INT((int)DSB(DS_001088F5), 1);
    CHECK_EQ_INT((int)chs_active_count(), 10);
    /* A hook with a visible effect (0x29B74: DS_00104AFE = 0x78) must not
     * run before the wipe is done, and must run before the mode copy (it
     * stores mode 0x15). */
    DSD(DS_00104AE4) = 0x29B74u;
    DSW(DS_00104AFE) = 0x1111u;
    DSB(DS_001088F4) = 0x55u;
    DSD(DS_0010150C) = 0x300u;
    frontend_mode_1b_step();                             /* frame 2 */
    CHECK_EQ_INT((int)cm_pset_word(w2), 0x2DEA);
    for (u32 k = 3; k <= 17u; k++) frontend_mode_1b_step();
    CHECK_EQ_INT((int)DSB(DS_001088F5), 0x11);
    CHECK_EQ_INT((int)cm_pset_word(w2), 0x3F14);
    CHECK_EQ_INT((int)DSB(DS_001088F4), 0x55);           /* spawn frame only */
    CHECK_EQ_INT((int)DSD(DS_0010150C), 0x300);
    CHECK_EQ_INT((int)DSW(DS_00104B00), 0x1B);
    CHECK_EQ_INT((int)DSW(DS_00104AFE), 0x1111);
    CHECK_EQ_INT((int)(DSB(w2 + 0x28u) & 8u), 0);
    frontend_mode_1b_step();                             /* frame 18: done */
    CHECK_EQ_INT((int)DSD(DS_000C98F0), 0);
    CHECK_EQ_INT((int)(DSB(w2 + 0x28u) & 8u), 8);        /* 0x2B150 */
    CHECK_EQ_INT((int)DSW(DS_00104AFE), 0x78);           /* the hook ran */
    CHECK_EQ_INT((int)DSW(DS_00104B00), 0x10);           /* then DS_00104AFA */
    CHECK_EQ_INT((int)DSB(DS_001088F5), 0x11);
    CHECK_EQ_INT((int)DSB(DS_001088F4), 0x55);
    CHECK_EQ_INT((int)chs_active_count(), 10);           /* no 0x2BAF4 */

    /* (e) 0x4F9A0's done frame with the no-op hook 0x29D60 (unregistered):
     * 0x4F9E4's own 0x2BAF4 empties the active list. */
    tf_put(s_data, 0x80000u, sizeof s_data);
    tf_put(s_rec, rec_pool, sizeof s_rec);
    tf_put(s_pset, pset_pool, sizeof s_pset);
    actors_reset();
    (void)actor_alloc(0);
    DSD(DS_000C98F0) = actor_alloc(0);
    CHECK_EQ_INT((int)chs_active_count(), 2);
    DSD(DS_00104AE4) = 0x29D60u;
    DSB(DS_001088F5) = 0x11u;
    DSB(DS_001088F4) = 0x77u;
    DSD(DS_00104B00) = 0xBEEF001Au;
    DSD(DS_00101508) = 0x123u;
    DSD(DS_0010150C) = 0x123u;
    frontend_mode_1a_step();
    CHECK_EQ_INT((int)chs_active_count(), 0);
    CHECK_EQ_INT((int)DSD(DS_0010150C), 0);
    CHECK_EQ_INT((int)DSD(DS_000C98F0), 0);
    CHECK_EQ_INT((int)DSB(DS_001088F4), 1);
    CHECK_EQ_INT((int)DSB(DS_001088F5), 0);
    CHECK_EQ_INT((int)DSD(DS_00104B00), (int)0xBEEF001Bu);

    /* (f) The counter is signed (`sar eax,0x18`, `jle`): 0xFF (-1) is not
     * done, and 0x10D70 gets the word before the table, 0xC991A = 3. */
    {
        u32 r = actor_alloc(0);
        DSD(DS_000C98F0) = r;
        DSW(r + 0x28u) = 0u;
        DSB(DS_001088F5) = 0xFFu;
        CHECK_EQ_INT((int)frontend_wipe_in(), 0);
        CHECK_EQ_INT((int)DSB(DS_001088F5), 0);
        CHECK_EQ_INT((int)cm_pset_word(r), 3);
        CHECK_EQ_INT((int)DSD(DS_000C98F0), (int)r);
    }

    tf_put(s_data, 0x80000u, sizeof s_data);
    tf_put(s_rec, rec_pool, sizeof s_rec);
    tf_put(s_pset, pset_pool, sizeof s_pset);
    tf_put(s_bufa, bufa, sizeof s_bufa);
    tf_put(s_bufb, bufb, sizeof s_bufb);
    tf_put(s_res, res_tab, res_len);
    memcpy(gfx_aperture(), s_ap, sizeof s_ap);
    memcpy(gfx_dac, s_dac, sizeof s_dac);
}

/* Record §46-B: the fight reset 0x20DF4 (with 0x2C390/0x2C074), the hooks
 * 0x4F980's callers install (0x430E8, 0x4367C, 0x25BBC, 0x26998, 0x270BC),
 * the follow-on hook 0x430C0 and the callees 0x4F200, 0x25848, 0x46504 and
 * 0x4454C. The same snapshot as check_char_screen_modes. Scene values are the
 * data object's: 0xBDDFC[s] (DS_00107A40 after 0x38730) = 0x60, 0x50, 0x74,
 * 0x50, 0x90, 0xA0, 0x40, 0x50 and, past the table, 0xBDDFC[9] = 0x700;
 * 0xA87C4 = 0, 4, 2, 3, 6, 1, 5; 0xC8880 = 0, 5; the descriptors 0xC8364/
 * 0xC8378 carry ids 0x351/0x352 and 0xC87BC id 0x3F11. */
/* A child record (a5 bit 0x400): 0x2AE14 keeps a2 in the word +0x34 and
 * the layer a3 in +0x49. */
static u32 mh_find_child(u32 a2, u32 a3)
{
    for (u32 r = actor_list_head(); r != 0; r = actor_next(r))
        if (DSW(r + 0x34u) == a2 && DSB(r + 0x49u) == a3
            && (DSW(r + 0x28u) & 0x400u) != 0u) return r;
    return 0;
}

/* The fighter hooks' shared seeds: the stage word, the round byte, the slot
 * bytes the hooks move, the latch pair and sentinels in what they store. No
 * sound bank is read (DS_001028C8 = 0) and no dust is built (DS_00104B14 =
 * 1, the value 0x25A51 stores after arming). */
static void mh_seed_fighters(void)
{
    actors_reset();
    DSW(DS_00104AFC) = 2u;
    DSB(DS_00104B1E) = 0xFFu;
    DSB(DS_00104B13) = 0x77u;
    DSB(DS_001078FA) = 0x77u;
    DSD(DS_001077A8) = 0xDEADBEEFu;
    DSD(DS_001077A8 + 4u) = 0xDEADBEEFu;
    DSB(DS_0010816A) = 1u;
    DSB(DS_0010816B) = 4u;
    DSD(DS_001082C8) = 0x11223344u;
    DSD(DS_001082CC) = 0x55667788u;
    DSD(DS_001082C0) = 0xDEADBEEFu;
    DSD(DS_001082C4) = 0xDEADBEEFu;
    DSD(DS_00104AE4) = 0xDEADBEEFu;
    DSD(DS_000F0A48) = 0xDEADBEEFu;
    DSD(DS_001028C8) = 0u;
    DSB(DS_00104B14) = 1u;
    DSB(0x00104B0Au) = 0x77u;
    DSB(DS_00104B0B) = 0x77u;
    DSB(0x00104B1Au) = 0x77u;
    DSB(DS_0010780B) = 0x10u;                       /* slot 0 +0x5B */
    DSB(DS_0010780B + 0x94u) = 0x77u;               /* slot 1 +0x5B */
    DSW(DS_00107A40) = 0x1234u;
}

static void check_mode_1a_hooks(void)
{
    static u8 s_data[0x10B0D0u - 0x80000u];
    static u8 s_rec[0xEBA0u], s_pset[0x4880u];
    static u8 s_bufa[0xFA00u], s_bufb[0xFA00u], s_ap[0xFA00u];
    static u8 s_dac[sizeof gfx_dac];
    static u8 s_res[0x14u * 128u];
    u32 rec_pool = DSD(DS_001014F4), pset_pool = DSD(DS_001014EC);
    u32 bufa = DSD(DS_001014E8), bufb = DSD(DS_001014E4);
    u32 res_tab = DSD(DS_001014E0);
    u32 res_len = DSD(DS_001014F0) * 0x14u;
    CHECK(rec_pool != 0u && pset_pool != 0u && bufa != 0u && bufb != 0u,
          "the mode 0x1A hooks need the pools and buffers");
    CHECK(res_len != 0u && res_len <= sizeof s_res, "the resource table fits");
    if (rec_pool == 0u || pset_pool == 0u || bufa == 0u || bufb == 0u ||
        res_len == 0u || res_len > sizeof s_res)
        return;
    tf_snap(s_data, 0x80000u, sizeof s_data);
    tf_snap(s_rec, rec_pool, sizeof s_rec);
    tf_snap(s_pset, pset_pool, sizeof s_pset);
    tf_snap(s_bufa, bufa, sizeof s_bufa);
    tf_snap(s_bufb, bufb, sizeof s_bufb);
    tf_snap(s_res, res_tab, res_len);
    memcpy(s_ap, gfx_aperture(), sizeof s_ap);
    memcpy(s_dac, gfx_dac, sizeof s_dac);
#define MH_RESTORE() do { tf_put(s_data, 0x80000u, sizeof s_data);  \
        tf_put(s_rec, rec_pool, sizeof s_rec);                        \
        tf_put(s_pset, pset_pool, sizeof s_pset); } while (0)

    /* (a) The six hooks resolve (code immediates only, §46-B.2). */
    CHECK(fn_resolve(0x430E8u) == fight_hook_430e8, "0x430E8 resolves");
    CHECK(fn_resolve(0x4367Cu) == fight_hook_4367c, "0x4367C resolves");
    CHECK(fn_resolve(0x25BBCu) == game_hook_25bbc, "0x25BBC resolves");
    CHECK(fn_resolve(0x26998u) == game_hook_26998, "0x26998 resolves");
    CHECK(fn_resolve(0x270BCu) == game_hook_270bc, "0x270BC resolves");
    CHECK(fn_resolve(0x430C0u) == fight_hook_430c0, "0x430C0 resolves");

    /* (b) 0x20DF4 with EDX = 0: every pre-branch store and list, no
     * 0x2BAF4 (two seeded records survive) and no 0x38730 (DS_00107A40
     * keeps its sentinel). */
    actors_reset();
    (void)actor_alloc(0);
    (void)actor_alloc(0);
    DSD(DS_000F0A48) = 0xDEADBEEFu;
    DSD(DS_00100B4C) = 0xDEADBEEFu;
    DSD(DS_00104AE8) = 0xDEADBEEFu;
    DSB(DS_001088EC) = 0x77u;
    DSB(DS_001088EC + 1u) = 0x66u;
    DSB(DS_00104B15) = 0x77u;
    mem_fill(DS_00105C0C, 0xA5u, DS_00105D5C - DS_00105C0C);
    DSD(DS_00105BF0 - 4u) = 0x11111111u;
    DSD(DS_00105BF0) = 0xDEADBEEFu;
    DSD(DS_00105BF4) = 0xDEADBEEFu;
    DSD(DS_00105BF4 + 4u) = 0x22222222u;
    DSD(DS_000F0AEC) = 0xDEADBEEFu;
    DSD(DS_000F0AF0) = 0xDEADBEEFu;
    DSD(DS_000F0AF8) = 0xDEADBEEFu;                  /* the words F0AF8/F0AFA */
    DSW(DS_000F0AFC) = 0x1234u;
    mem_fill(0x000F0A78u, 0xA5u, 0x70u);
    DSB(DS_001078FA) = 2u;
    DSD(DS_001077A8) = 0xDEADBEEFu;
    DSW(DS_00107A40) = 0x1234u;
    game_fight_reset(9u, 0u);
    CHECK_EQ_INT((int)DSD(DS_000F0A48), 0);
    CHECK_EQ_INT((int)DSD(DS_00100B4C), 0);
    CHECK_EQ_INT((int)DSD(DS_00104AE8), 0);
    CHECK_EQ_INT((int)DSB(DS_001088EC), 0);
    CHECK_EQ_INT((int)DSB(DS_001088EC + 1u), 0x66);     /* a byte store */
    CHECK_EQ_INT((int)DSB(DS_00104B15), 0);
    CHECK_EQ_INT((int)DSD(DS_00105C0C), (int)DS_00105C0C);    /* 0x2C390 */
    CHECK_EQ_INT((int)DSD(DS_00105C10), (int)DS_00105C0C);
    {
        u32 n = DS_00105C14, k = 0;
        for (k = 0; k < 16u; k++) {
            n = DSD(n);
            CHECK_EQ_INT((int)n, (int)(DS_00105C1C + k * 0x14u));
        }
        CHECK_EQ_INT((int)DSD(n), (int)DS_00105C14);
        CHECK_EQ_INT((int)DSD(DS_00105C18), (int)(DS_00105C1C + 15u * 0x14u));
    }
    CHECK_EQ_INT((int)DSD(DS_00105BF0 - 4u), 0x11111111);   /* 0x2C074 */
    CHECK_EQ_INT((int)DSD(DS_00105BF0), 0);
    CHECK_EQ_INT((int)DSD(DS_00105BF4), 0);
    CHECK_EQ_INT((int)DSD(DS_00105BF4 + 4u), 0x22222222);
    CHECK_EQ_INT((int)DSD(DS_000F0AEC), 0);
    CHECK_EQ_INT((int)DSD(DS_000F0AF0), 0);
    CHECK_EQ_INT((int)DSD(DS_000F0AF8), 0);
    CHECK_EQ_INT((int)DSW(DS_000F0AFC), 0x400);         /* 0x12C70 */
    CHECK_EQ_INT((int)DSD(0x000F0AE0u), 0x000F0AE0);    /* 0x12750 */
    CHECK_EQ_INT((int)DSD(DS_0010884C), (int)DS_0010884C);  /* 0x49300 */
    CHECK_EQ_INT((int)DSD(0x00104880u), 0x00104880);    /* 0x28E98 */
    CHECK_EQ_INT((int)DSB(DS_001078FA), 0);             /* 0x34978 */
    CHECK_EQ_INT((int)DSD(DS_001077A8), 0);
    CHECK_EQ_INT((int)chs_active_count(), 2);           /* no 0x2BAF4 */
    CHECK_EQ_INT((int)DSW(DS_00107A40), 0x1234);        /* no 0x38730 */

    /* (c) EDX = 1: 0x2BAF4 drops the seeded records and 0x38730/0x412A0 run
     * on the stage clamped to 7 (0xBDDFC[7] = 0x50, not [9] = 0x700); an
     * in-range stage is not clamped (0xBDDFC[6] = 0x40). */
    MH_RESTORE();
    actors_reset();
    {
        u32 r = actor_alloc(0);
        DSD(r + 0x08u) = 0x7777u;
    }
    DSW(DS_00107A40) = 0x1234u;
    game_fight_reset(9u, 1u);
    CHECK_EQ_INT((int)DSW(DS_00107A40), 0x50);
    {
        u32 seen = 0;
        for (u32 r = actor_list_head(); r != 0; r = actor_next(r))
            if (DSD(r + 0x08u) == 0x7777u) seen = 1;
        CHECK_EQ_INT((int)seen, 0);
        CHECK(chs_active_count() > 0u, "0x412A0 spawned the scene");
    }
    MH_RESTORE();
    game_fight_reset(6u, 1u);
    CHECK_EQ_INT((int)DSW(DS_00107A40), 0x40);

    /* (d) 0x25BBC: the round byte wraps 0xFF -> 0, both fighters live on
     * the stage word's scene 2 (0xBDDFC[2] = 0x74), the latch pair copied,
     * the hook 0x5D812. */
    MH_RESTORE();
    mh_seed_fighters();
    game_hook_25bbc();
    CHECK_EQ_INT((int)DSB(DS_00104B1E), 0);
    CHECK_EQ_INT((int)DSB(DS_00104B13), 0);
    CHECK_EQ_INT((int)DSB(DS_001078FA), 2);
    CHECK_EQ_INT((int)DSD(DS_001077A8), (int)DS_001077B0);
    CHECK_EQ_INT((int)DSD(DS_001077A8 + 4u), (int)(DS_001077B0 + 0x94u));
    CHECK_EQ_INT((int)DSB(DS_001077B0 + 0x7Au), 1);
    CHECK_EQ_INT((int)DSB(DS_001077B0 + 0x94u + 0x7Au), 4);
    CHECK_EQ_INT((int)DSW(DS_00107A40), 0x74);
    CHECK_EQ_INT((int)DSD(DS_000F0A48), 0);
    CHECK_EQ_INT((int)DSD(DS_001082C0), 0x11223344);    /* 0x46504 */
    CHECK_EQ_INT((int)DSD(DS_001082C4), 0x55667788);
    CHECK_EQ_INT((int)DSD(DS_00104AE4), 0x5D812);

    /* (e) 0x26998, DS_001078A7 == 0: side 1 (DS_00104B1A = 1), its +0x5B
     * 0x77 + 2 capped at 0x78, slot 0's untouched. Then DS_001078A7 != 0:
     * side 0, 0x10 + 2 = 0x12; and 0xFF + 2 wraps to 1, under the cap. */
    MH_RESTORE();
    mh_seed_fighters();
    DSB(DS_001078A7) = 0u;
    game_hook_26998();
    CHECK_EQ_INT((int)DSB(DS_00104B1E), 0);
    CHECK_EQ_INT((int)DSB(0x00104B1Au), 1);
    CHECK_EQ_INT((int)DSD(DS_001077A8), 0);
    CHECK_EQ_INT((int)DSD(DS_001077A8 + 4u), (int)(DS_001077B0 + 0x94u));
    CHECK_EQ_INT((int)DSB(DS_001078FA), 1);
    CHECK_EQ_INT((int)DSB(DS_0010780B + 0x94u), 0x78);
    CHECK_EQ_INT((int)DSB(DS_0010780B), 0x10);
    CHECK_EQ_INT((int)DSB(DS_00104B13), 0x77);          /* not 0x25BBC's */
    CHECK_EQ_INT((int)DSW(DS_00107A40), 0x74);
    CHECK_EQ_INT((int)DSD(DS_001082C0), 0x11223344);
    CHECK_EQ_INT((int)DSD(DS_001082C4), 0x55667788);
    CHECK_EQ_INT((int)DSD(DS_00104AE4), 0x5D812);
    MH_RESTORE();
    mh_seed_fighters();
    DSB(DS_001078A7) = 1u;
    game_hook_26998();
    CHECK_EQ_INT((int)DSB(0x00104B1Au), 0);
    CHECK_EQ_INT((int)DSD(DS_001077A8), (int)DS_001077B0);
    CHECK_EQ_INT((int)DSD(DS_001077A8 + 4u), 0);
    CHECK_EQ_INT((int)DSB(DS_0010780B), 0x12);
    CHECK_EQ_INT((int)DSB(DS_0010780B + 0x94u), 0x77);
    MH_RESTORE();
    mh_seed_fighters();
    DSB(DS_001078A7) = 1u;
    DSB(DS_0010780B) = 0xFFu;
    game_hook_26998();
    CHECK_EQ_INT((int)DSB(DS_0010780B), 1);

    /* (f) 0x270BC: the side is the signed byte DS_0010810D (1), DS_00104B0A
     * = 0, DS_00104B0B = that slot's +0x5B; no 0x46504 (the latch pair keeps
     * its sentinels). */
    MH_RESTORE();
    mh_seed_fighters();
    DSB(0x0010810Du) = 1u;
    DSB(DS_0010780B + 0x94u) = 0x42u;
    game_hook_270bc();
    CHECK_EQ_INT((int)DSB(DS_00104B1E), 0);
    CHECK_EQ_INT((int)DSD(DS_001077A8), 0);
    CHECK_EQ_INT((int)DSD(DS_001077A8 + 4u), (int)(DS_001077B0 + 0x94u));
    CHECK_EQ_INT((int)DSB(0x00104B0Au), 0);
    CHECK_EQ_INT((int)DSB(DS_00104B0B), 0x42);
    CHECK_EQ_INT((int)DSW(DS_00107A40), 0x74);
    CHECK_EQ_INT((int)DSD(DS_001082C0), (int)0xDEADBEEFu);
    CHECK_EQ_INT((int)DSD(DS_00104AE4), 0x5D812);

    /* (g) 0x4367C with DS_00104B1D == 3 is 0x4454C. DS_00108173 == 0: per
     * side 0x10816E = 0xFF, 0x108166 = 0xC8880[s] (0, 5), 0x105B34 = 0, four
     * 0xFF bytes at 0x108134 and four zero dwords at 0x108114, 0x108164 = 0;
     * then 0x444C8 (no countdown: DS_0010816C keeps its sentinel). */
    MH_RESTORE();
    chs_seed(3u, 6u, 0u, 0u);
    DSB(DS_00104B1D) = 3u;
    mem_fill(0x00108110u, 0x77u, 0x2Cu);                /* 0x108110..0x10813B */
    DSW(0x00108164u) = 0x7777u;
    DSB(0x0010816Du) = 0x66u;
    DSW(DS_0010816E) = 0x7777u;
    DSW(DS_00108168) = 0x0302u;
    DSW(0x00105B33u) = 0x7777u;
    DSB(0x00105B35u) = 0x77u;
    fight_hook_4367c();
    CHECK_EQ_INT((int)DSW(DS_0010816E), 0xFFFF);
    CHECK_EQ_INT((int)DSB(DS_00108166), 0);
    CHECK_EQ_INT((int)DSB(DS_00108166 + 1u), 5);
    CHECK_EQ_INT((int)DSB(0x00105B34u), 0);
    CHECK_EQ_INT((int)DSB(0x00105B35u), 0);
    CHECK_EQ_INT((int)DSB(0x00105B33u), 0x77);
    for (u32 k = 0; k < 8u; k++)
        CHECK_EQ_INT((int)DSB(0x00108134u + k), 0xFF);
    for (u32 k = 0; k < 8u; k++)
        CHECK_EQ_INT((int)DSD(0x00108114u + k * 4u), 0);
    CHECK_EQ_INT((int)DSD(0x00108110u), 0x77777777);
    CHECK_EQ_INT((int)DSW(0x00108164u), 0);
    CHECK_EQ_INT((int)DSB(0x0010816Du), 0x66);
    CHECK_EQ_INT((int)DSW(DS_00108168), 0x0302);        /* no step */
    CHECK_EQ_INT((int)DSW(DS_0010816C) & 0xFF, 0x77);   /* 0x444C8, not 0x43738 */
    CHECK_EQ_INT((int)DSB(DS_00108170), 1);             /* both sides */
    CHECK_EQ_INT((int)DSB(DS_00108170 + 1u), 1);
    CHECK_EQ_INT((int)DSD(DS_00104AE4), 0x29D60);

    /* (h) 0x4454C with DS_00108173 != 0: 0x108166/7 take 0x108168/9, and the
     * step: 0x108169 6 -> 7 wraps to 0 and carries 0x108168 2 -> 3. */
    MH_RESTORE();
    chs_seed(3u, 6u, 0u, 1u);
    DSB(DS_00104B1D) = 3u;
    mem_fill(0x00108110u, 0x77u, 0x2Cu);
    DSW(DS_0010816E) = 0x7777u;
    DSB(DS_00108168) = 2u;
    DSB(DS_00108169) = 6u;
    DSW(0x00105B34u) = 0x7777u;
    fight_hook_4367c();
    CHECK_EQ_INT((int)DSW(DS_0010816E), 0xFFFF);
    CHECK_EQ_INT((int)DSB(DS_00108166), 2);
    CHECK_EQ_INT((int)DSB(DS_00108166 + 1u), 6);
    CHECK_EQ_INT((int)DSW(0x00105B34u), 0);
    CHECK_EQ_INT((int)DSB(DS_00108169), 0);
    CHECK_EQ_INT((int)DSB(DS_00108168), 3);
    CHECK_EQ_INT((int)DSB(0x00108134u), 0x77);          /* the other arm's */
    CHECK_EQ_INT((int)DSW(DS_0010816C) & 0xFF, 0x77);

    /* (i) 0x4367C's own arms (DS_00104B1D != 3) end in 0x43738, whose
     * countdown DS_0010816C is 5 / 0xF. DS_00108173 != 0: both bytes wrap
     * (6 -> 0 carries 6 -> 7 -> 0), 0x108166/7 = the old 0x108168/9. Then
     * DS_00108173 == 0: 0x108166/7 = 0xC887F[1..2] = 0, 5 and no step. */
    MH_RESTORE();
    chs_seed(3u, 6u, 2u, 1u);
    DSB(DS_00104B1D) = 0u;
    DSB(DS_00108168) = 6u;
    DSB(DS_00108169) = 5u;
    DSW(DS_0010816E) = 0x7777u;
    DSW(0x00105B34u) = 0x7777u;
    fight_hook_4367c();
    CHECK_EQ_INT((int)DSB(DS_00108166), 6);
    CHECK_EQ_INT((int)DSB(DS_00108166 + 1u), 5);
    CHECK_EQ_INT((int)DSB(DS_00108169), 6);             /* 5 -> 6, no wrap */
    CHECK_EQ_INT((int)DSB(DS_00108168), 6);
    CHECK_EQ_INT((int)DSW(0x00105B34u), 0);
    CHECK_EQ_INT((int)DSW(DS_0010816C), 5);
    CHECK_EQ_INT((int)DSD(DS_00104AE4), 0x29D60);
    MH_RESTORE();
    chs_seed(3u, 6u, 2u, 1u);
    DSB(DS_00104B1D) = 0u;
    DSB(DS_00108168) = 6u;
    DSB(DS_00108169) = 6u;
    fight_hook_4367c();
    CHECK_EQ_INT((int)DSB(DS_00108169), 0);
    CHECK_EQ_INT((int)DSB(DS_00108168), 0);
    MH_RESTORE();
    chs_seed(3u, 6u, 2u, 0u);
    DSB(DS_00104B1D) = 0u;
    DSB(DS_00108168) = 2u;
    DSB(DS_00108169) = 6u;
    DSB(0x0010816Du) = 0x66u;
    fight_hook_4367c();
    CHECK_EQ_INT((int)DSB(DS_00108166), 0);
    CHECK_EQ_INT((int)DSB(DS_00108166 + 1u), 5);
    CHECK_EQ_INT((int)DSB(DS_00108169), 6);
    CHECK_EQ_INT((int)DSB(DS_00108168), 2);
    CHECK_EQ_INT((int)DSW(DS_0010816C), 0xF);

    /* (j) 0x430E8, DS_00104B1D = 1, DS_00104B1F = 2 and DS_00104AB8 = 1:
     * 0x25848 runs first, on DS_00104B1F = 2 (arm 0: 0xA87C4[DS_0010816B =
     * 3] + rng(6) + 1, mod 7); then DS_00104B1F = 1 and 0x41350 runs for
     * side (1 - 1) ^ 1 = 1, whose think gate DS_001078A7 it sets (with
     * DS_00104B1D == 1 it stores no character); the six spawns (the last four
     * are children: a2 in +0x34, a3 in +0x49) and the 0x38B18 row, the hook
     * 0x430C0. Both characters are 3, so the last two spawns share 0xC84E0[3]
     * and the last's +0x2E is the other's + 4. DS_00108173 != 0 sets side
     * 0's think gate DS_00107813 too. */
    MH_RESTORE();
    actors_reset();
    DSD(actor_alloc(0) + 0x08u) = 0x7777u;             /* 0x4F200's 0x2BAF4 drops it */
    DSB(DS_00104B17) = 0u;
    DSB(DS_00104B1D) = 1u;
    DSB(DS_00104AB8) = 1u;
    DSB(DS_00104B1F) = 2u;
    DSB(DS_0010816A) = 3u;
    DSB(DS_0010816B) = 3u;
    DSB(DS_00108173) = 1u;
    DSB(DS_00107813) = 0x77u;
    DSB(DS_001078A7) = 0x77u;
    DSB(DS_00107A55) = 0x99u;
    DSB(DS_00107A54) = 0x99u;
    DSD(DS_00104AE4) = 0xDEADBEEFu;
    mem_fill(DS_00107A1C, 0u, 0x1Cu);
    rng_seed(0x4321u);
    u32 st = rng_next(6u) + 3u + 1u;                    /* 0xA87C4[3] = 3 */
    if (st >= 7u) st -= 7u;
    rng_seed(0x4321u);
    fight_hook_430e8();
    CHECK_EQ_INT((int)DSB(DS_00104B1F), 1);
    CHECK_EQ_INT((int)DSW(DS_00104AFC), (int)st);
    CHECK_EQ_INT((int)DSB(DS_0010816A), 3);
    CHECK_EQ_INT((int)DSB(DS_0010816B), 3);
    CHECK_EQ_INT((int)DSB(DS_00107813), 1);
    CHECK_EQ_INT((int)DSB(DS_001078A7), 1);
    CHECK_EQ_INT((int)DSB(DS_00107A55), 0);             /* 0x4F200(0) */
    for (u32 r = actor_list_head(); r != 0; r = actor_next(r))
        CHECK(DSD(r + 0x08u) != 0x7777u, "0x4F200 reset the actors");
    CHECK_EQ_INT((int)DSB(DS_00107A54), 0);
    {
        u32 a = DSD(DS_001080B4), b = DSD(DS_001080B8);
        CHECK(a != 0u && b != 0u, "0x430E8's first two spawns");
        CHECK_EQ_INT((int)DSD(a + 0x08u), 0x351);
        CHECK_EQ_INT((int)DSD(b + 0x08u), 0x352);
        CHECK_EQ_INT((int)DSB(a + 0x49u), 0xF0);
        CHECK_EQ_INT((int)DSB(b + 0x49u), 0xF1);
        CHECK_EQ_INT((int)DSD(b + 0x18u), 0x2A00);
        u32 r3 = mh_find_child(0xFu, 0xF4u);
        u32 r5 = mh_find_child(0x9Bu, 0xE0u), r6 = mh_find_child(0xDu, 0xE0u);
        CHECK(r3 != 0u && r5 != 0u && r6 != 0u, "0x430E8's portrait spawns");
        if (r3 != 0u) CHECK_EQ_INT((int)DSW(r3 + 0x36u), 7);    /* a4 */
        if (r5 != 0u && r6 != 0u) {
            CHECK_EQ_INT((int)DSD(r5 + 0x08u), (int)DSD(r6 + 0x08u));
            CHECK_EQ_INT((int)DSW(r6 + 0x2Eu), (int)((DSW(r5 + 0x2Eu) + 4u) & 0xFFFFu));
            CHECK_EQ_INT((int)DSB(r6 + 0x4Eu), 1);
            CHECK(DSB(r5 + 0x4Eu) != 1u, "only the last spawn gets +0x4E = 1");
            /* a5's low 7 bits are the parent's index: pa for spawn 5, pb
             * for spawn 6. */
            CHECK_EQ_INT((int)DSB(r5 + 0x4Au), (int)(DSW(a + 0x56u) & 0x7Fu));
            CHECK_EQ_INT((int)DSB(r6 + 0x4Au), (int)(DSW(b + 0x56u) & 0x7Fu));
            CHECK(DSW(a + 0x56u) != DSW(b + 0x56u), "two parents");
        }
        u32 row = DSD(DS_00107A1C);
        CHECK(row != 0u, "0x38B18 filled the row table");
        if (row != 0u) CHECK_EQ_INT((int)DSD(row + 0x08u), 0x3F11);
    }
    CHECK_EQ_INT((int)DSD(DS_00104AE4), 0x430C0);

    /* (k) 0x430E8 with DS_00104B1D = 0, DS_00104B1F = 2: arm 0 reads the
     * character at 0x108169 + 2 = DS_0010816B (5: 0xA87C4[5] = 1), and
     * 0x41350 runs for side (2 - 1) ^ 1 = 0, storing 0xC835A[stage] into
     * DS_0010816A only; with DS_00108173 == 0 side 1's think gate
     * DS_001078A7 is untouched. */
    MH_RESTORE();
    actors_reset();
    DSB(DS_00104B17) = 0u;
    DSB(DS_00104B1D) = 0u;
    DSB(DS_00104B1F) = 2u;
    DSB(DS_0010816A) = 0x77u;
    DSB(DS_0010816B) = 5u;
    DSB(DS_00108173) = 0u;
    DSB(DS_001078A7) = 0x77u;
    rng_seed(0x9876u);
    st = rng_next(6u) + 1u + 1u;
    if (st >= 7u) st -= 7u;
    rng_seed(0x9876u);
    fight_hook_430e8();
    CHECK_EQ_INT((int)DSW(DS_00104AFC), (int)st);
    CHECK_EQ_INT((int)DSB(DS_0010816A), (int)DSB(0x000C835Au + st));
    CHECK_EQ_INT((int)DSB(DS_0010816B), 5);
    CHECK_EQ_INT((int)DSB(DS_001078A7), 0x77);          /* DS_00108173 == 0 */

    /* (k2) 0x430E8 with DS_00104B1F = 3 (no 0x41350) and DS_00108173 != 0:
     * both think gates come from 0x430E8 alone. Characters 0 and 6: spawns 3
     * (0xC84FC[c0], parent pa) and 4 (0xC84FC[c1], parent pb) share a2/a3
     * but not their descriptor. DS_00104B17 = 2 leaves the stage word. */
    MH_RESTORE();
    actors_reset();
    DSB(DS_00104B17) = 2u;
    DSB(DS_00104B1D) = 0u;
    DSB(DS_00104B1F) = 3u;
    DSB(DS_0010816A) = 0u;
    DSB(DS_0010816B) = 6u;
    DSB(DS_00108173) = 1u;
    DSB(DS_00107813) = 0x77u;
    DSB(DS_001078A7) = 0x77u;
    DSW(DS_00104AFC) = 0x7777u;
    fight_hook_430e8();
    CHECK_EQ_INT((int)DSW(DS_00104AFC), 0x7777);
    CHECK_EQ_INT((int)DSB(DS_00107813), 1);
    CHECK_EQ_INT((int)DSB(DS_001078A7), 1);
    CHECK_EQ_INT((int)DSB(DS_0010816A), 0);
    CHECK_EQ_INT((int)DSB(DS_0010816B), 6);
    {
        u32 pa = DSW(DSD(DS_001080B4) + 0x56u) & 0x7Fu;
        u32 pb = DSW(DSD(DS_001080B8) + 0x56u) & 0x7Fu;
        u32 r3 = 0, r4 = 0;
        for (u32 r = actor_list_head(); r != 0; r = actor_next(r)) {
            if (DSW(r + 0x34u) != 0xFu || DSB(r + 0x49u) != 0xF4u) continue;
            if (DSB(r + 0x4Au) == pa) r3 = r;
            if (DSB(r + 0x4Au) == pb) r4 = r;
        }
        CHECK(r3 != 0u && r4 != 0u, "spawns 3 and 4 under their parents");
        if (r3 != 0u && r4 != 0u)
            CHECK(DSD(r3 + 0x08u) != DSD(r4 + 0x08u),
                  "0xC84FC[c0] and 0xC84FC[c1] differ");
    }

    /* (l) 0x25848's other arms. DS_00104B1F = 3: rng(7). DS_00104B17 = 2:
     * nothing. DS_00104B17 = 1 with zero bytes at DS_00108106[2] and [5]:
     * the rng(2)-th of them. With none zero: the first whose low 7 bits are
     * neither DS_0010782A nor DS_001078BE | 0x40; with every one matching,
     * DS_00104AD4 == 2 steps the word (6 -> 0), else the first equal to the
     * frame byte [DS_00104AD4 ^ 1]. */
    MH_RESTORE();
    DSB(DS_00104B17) = 0u;
    DSB(DS_00104B1F) = 3u;
    rng_seed(0x55AAu);
    st = rng_next(7u);
    rng_seed(0x55AAu);
    DSW(DS_00104AFC) = 0x7777u;
    flow_stage_pick();
    CHECK_EQ_INT((int)DSW(DS_00104AFC), (int)st);
    /* Arm 0's reduction at exactly 7: 0xA87C4[5] = 1, so rng(6) = 5 gives
     * 5 + 1 + 1 = 7 -> 0 (the raw's `cmp eax,7; jl`). The seed is found by
     * stepping the model. */
    {
        u32 seed = 1;
        for (; seed < 0x10000u; seed++) {
            rng_seed(seed);
            if (rng_next(6u) == 5u) break;
        }
        CHECK(seed < 0x10000u, "a seed whose rng(6) is 5");
        DSB(DS_00104B1F) = 2u;
        DSB(DS_0010816B) = 5u;
        rng_seed(seed);
        flow_stage_pick();
        CHECK_EQ_INT((int)DSW(DS_00104AFC), 0);
        DSB(DS_00104B1F) = 3u;
    }
    DSB(DS_00104B17) = 2u;
    DSW(DS_00104AFC) = 0x7777u;
    flow_stage_pick();
    CHECK_EQ_INT((int)DSW(DS_00104AFC), 0x7777);
    DSB(DS_00104B17) = 1u;
    for (u32 i = 0; i < 7u; i++) DSB(DS_00108106 + i) = 0x81u;
    DSB(DS_00108106 + 2u) = 0u;
    DSB(DS_00108106 + 5u) = 0u;
    rng_seed(0x55AAu);
    st = rng_next(2u) == 0u ? 2u : 5u;
    rng_seed(0x55AAu);
    flow_stage_pick();
    CHECK_EQ_INT((int)DSW(DS_00104AFC), (int)st);
    DSB(DS_0010782A) = 1u;
    DSB(DS_001078BE) = 2u;                              /* frame byte 0x42 */
    for (u32 i = 0; i < 7u; i++) DSB(DS_00108106 + i) = 0x81u;
    DSB(DS_00108106 + 4u) = 0x42u;
    DSB(DS_00108106 + 6u) = 0x03u;
    DSW(DS_00104AFC) = 0x7777u;
    flow_stage_pick();
    CHECK_EQ_INT((int)DSW(DS_00104AFC), 6);
    DSB(DS_00108106 + 6u) = 0x81u;
    DSD(DS_00104AD4) = 2u;
    DSW(DS_00104AFC) = 6u;
    flow_stage_pick();
    CHECK_EQ_INT((int)DSW(DS_00104AFC), 0);
    DSW(DS_00104AFC) = 3u;
    flow_stage_pick();
    CHECK_EQ_INT((int)DSW(DS_00104AFC), 4);
    DSD(DS_00104AD4) = 0u;                              /* frame byte [1] */
    DSW(DS_00104AFC) = 0x7777u;
    flow_stage_pick();
    CHECK_EQ_INT((int)DSW(DS_00104AFC), 4);
    DSD(DS_00104AD4) = 1u;                              /* frame byte [0] */
    DSW(DS_00104AFC) = 0x7777u;
    flow_stage_pick();
    CHECK_EQ_INT((int)DSW(DS_00104AFC), 0);

    /* (m) The chain: 0x430E8 as the hook of mode 0x1A (as 0x1F447 installs
     * it), 18 wipe-in frames, then 18 wipe-out frames run 0x430C0, which
     * draws "VS" at row 2 from col 0x13 (the direct 0x2F198 call on the same
     * state gives the same glyph ids; mode 0x4000 would give others). */
    MH_RESTORE();
    actors_reset();
    DSB(DS_00104B17) = 2u;                              /* keep the stage */
    DSB(DS_00104B1D) = 0u;
    DSB(DS_00104B1F) = 3u;
    DSB(DS_0010816A) = 0u;
    DSB(DS_0010816B) = 6u;
    DSD(DS_000C98F0) = 0u;
    DSD(DS_00104AE4) = 0x430E8u;
    frontend_wipe_arm(0x2Au);
    for (u32 k = 1; k <= 18u; k++) frontend_mode_1a_step();
    CHECK_EQ_INT((int)DSW(DS_00104B00), 0x1B);
    CHECK_EQ_INT((int)DSD(DS_00104AE4), 0x430C0);
    CHECK(DSD(DS_001080B4) != 0u, "the hook 0x430E8 ran");
    CHECK_EQ_INT((int)chs_grid(2u, 0x13u), 0);
    for (u32 k = 1; k <= 17u; k++) frontend_mode_1b_step();
    CHECK_EQ_INT((int)chs_grid(2u, 0x13u), 0);          /* not before done */
    frontend_mode_1b_step();
    CHECK_EQ_INT((int)DSW(DS_00104B00), 0x2A);
    {
        u32 g0 = chs_grid(2u, 0x13u);
        CHECK(g0 != 0u, "0x430C0 drew at row 2, col 0x13");
        CHECK_EQ_INT((int)chs_grid(1u, 0x13u), 0);
        CHECK_EQ_INT((int)chs_grid(3u, 0x13u), 0);
        u32 id0 = g0 != 0u ? DSD(g0 + 0x08u) : 0u;
        mem_fill(DS_00105F38 + 2u * 0xACu, 0, 0xACu);
        text_cursor_set(0x13, 2, mem + 0x00080C04u, 0x4002u);
        u32 g1 = chs_grid(2u, 0x13u);
        CHECK_EQ_INT((int)(g1 != 0u ? DSD(g1 + 0x08u) : 0u), (int)id0);
        mem_fill(DS_00105F38 + 2u * 0xACu, 0, 0xACu);
        text_cursor_set(0x13, 2, mem + 0x00080C04u, 0x4000u);
        u32 g2 = chs_grid(2u, 0x13u);
        CHECK((g2 != 0u ? DSD(g2 + 0x08u) : 0u) != id0,
              "mode 0x4002's glyph differs from mode 0x4000's");
    }
#undef MH_RESTORE

    tf_put(s_data, 0x80000u, sizeof s_data);
    tf_put(s_rec, rec_pool, sizeof s_rec);
    tf_put(s_pset, pset_pool, sizeof s_pset);
    tf_put(s_bufa, bufa, sizeof s_bufa);
    tf_put(s_bufb, bufb, sizeof s_bufb);
    tf_put(s_res, res_tab, res_len);
    memcpy(gfx_aperture(), s_ap, sizeof s_ap);
    memcpy(gfx_dac, s_dac, sizeof s_dac);
}

/* Record §46-F: the seven remaining DS_00104AE4 values 0x259CC, 0x26978,
 * 0x27134, 0x24B54, 0x25AE8, 0x4142C and 0x10E80, with 0x2C304, 0x413C8 and
 * 0x4246C. The same snapshot as check_mode_1a_hooks. The config field 0x29 is
 * 32 bits wide (descriptor 0x2D3A4 = 0x1D980: width 8 nibbles, no trailing
 * byte). The descriptors 0xC86F0/0xC86DC carry ids 0x2BEF/0x3F12. */
static void mt_seed_arm(void)
{
    DSD(DS_00104AE4) = 0xDEADBEEFu;
    DSB(DS_001088F5) = 0x77u;
    DSW(DS_00104AFA) = 0x7777u;
    DSD(DS_00104B00) = 0xBEEF7777u;
    DSB(DS_00104B25) = 0x77u;
}

/* One actor record the hook's 0x2BAF4 must drop (id 0x7777). */
static void mt_seed_record(void)
{
    actors_reset();
    DSD(actor_alloc(0) + 0x08u) = 0x7777u;
}

static u32 mt_record_seen(void)
{
    for (u32 r = actor_list_head(); r != 0; r = actor_next(r))
        if (DSD(r + 0x08u) == 0x7777u) return 1u;
    return 0u;
}

static void check_mode_17_hooks(void)
{
    static u8 s_data[0x10B0D0u - 0x80000u];
    static u8 s_rec[0xEBA0u], s_pset[0x4880u];
    static u8 s_bufa[0xFA00u], s_bufb[0xFA00u], s_ap[0xFA00u];
    static u8 s_dac[sizeof gfx_dac];
    static u8 s_res[0x14u * 128u];
    u32 rec_pool = DSD(DS_001014F4), pset_pool = DSD(DS_001014EC);
    u32 bufa = DSD(DS_001014E8), bufb = DSD(DS_001014E4);
    u32 res_tab = DSD(DS_001014E0);
    u32 res_len = DSD(DS_001014F0) * 0x14u;
    CHECK(rec_pool != 0u && pset_pool != 0u && bufa != 0u && bufb != 0u,
          "the mode 0x17 hooks need the pools and buffers");
    CHECK(res_len != 0u && res_len <= sizeof s_res, "the resource table fits");
    if (rec_pool == 0u || pset_pool == 0u || bufa == 0u || bufb == 0u ||
        res_len == 0u || res_len > sizeof s_res)
        return;
    tf_snap(s_data, 0x80000u, sizeof s_data);
    tf_snap(s_rec, rec_pool, sizeof s_rec);
    tf_snap(s_pset, pset_pool, sizeof s_pset);
    tf_snap(s_bufa, bufa, sizeof s_bufa);
    tf_snap(s_bufb, bufb, sizeof s_bufb);
    tf_snap(s_res, res_tab, res_len);
    memcpy(s_ap, gfx_aperture(), sizeof s_ap);
    memcpy(s_dac, gfx_dac, sizeof s_dac);
#define MT_RESTORE() do { tf_put(s_data, 0x80000u, sizeof s_data);  \
        tf_put(s_rec, rec_pool, sizeof s_rec);                        \
        tf_put(s_pset, pset_pool, sizeof s_pset); } while (0)

    /* (a) The seven values resolve (code immediates only, §46-F.2). */
    CHECK(fn_resolve(0x259CCu) == game_hook_259cc, "0x259CC resolves");
    CHECK(fn_resolve(0x10E80u) == game_state_init, "0x10E80 resolves");
    CHECK(fn_resolve(0x24B54u) == game_hook_24b54, "0x24B54 resolves");
    CHECK(fn_resolve(0x27134u) == game_hook_27134, "0x27134 resolves");
    CHECK(fn_resolve(0x4142Cu) == fight_hook_4142c, "0x4142C resolves");
    CHECK(fn_resolve(0x25AE8u) == game_hook_25ae8, "0x25AE8 resolves");
    CHECK(fn_resolve(0x26978u) == game_hook_26978, "0x26978 resolves");

    /* (b) 0x259CC, DS_00104B1D != 3: the stage's byte, the zero stores,
     * DS_00104AD4 = -1, DS_00104ADC from bits 20..21 of the config field
     * 0x29 (2 -> 5), the hook 0x25BBC and mode 0x1A with 5; DS_00104B14 = 0
     * and DS_00104B21 untouched. */
    MT_RESTORE();
    config_field_set(0x29u, 0x00230000u);
    for (u32 i = 0; i < 8u; i++) DSB(DS_00108106 + i) = 0x81u;
    DSW(DS_00104AFC) = 3u;
    DSB(DS_00104B1E) = 0x77u;
    DSB(DS_00104AF3) = 0x77u;
    DSB(DS_00104AF2) = 0x77u;
    DSB(DS_00104AF2 - 1u) = 0x66u;
    DSB(DS_00104B14) = 0x77u;
    DSD(DS_00104AD4) = 0x12345678u;
    DSD(DS_00104AC8) = 0xDEADBEEFu;
    DSD(DS_00104ADC) = 0xDEADBEEFu;
    DSB(DS_00104B21) = 0x77u;
    DSB(DS_00104B1D) = 0u;
    mt_seed_arm();
    game_hook_259cc();
    for (u32 i = 0; i < 8u; i++)
        CHECK_EQ_INT((int)DSB(DS_00108106 + i), i == 3u ? 0 : 0x81);
    CHECK_EQ_INT((int)DSB(DS_00104B1E), 0);
    CHECK_EQ_INT((int)DSB(DS_00104AF3), 0);
    CHECK_EQ_INT((int)DSB(DS_00104AF2), 0);
    CHECK_EQ_INT((int)DSB(DS_00104AF2 - 1u), 0x66);
    CHECK_EQ_INT((int)DSB(DS_00104B14), 0);
    CHECK_EQ_INT((int)DSD(DS_00104AD4), -1);
    CHECK_EQ_INT((int)DSD(DS_00104AC8), 0);
    CHECK_EQ_INT((int)DSD(DS_00104ADC), 5);
    CHECK_EQ_INT((int)DSB(DS_00104B21), 0x77);
    CHECK_EQ_INT((int)DSB(DS_00104B25), 1);
    CHECK_EQ_INT((int)DSD(DS_00104AE4), 0x25BBC);
    CHECK_EQ_INT((int)DSB(DS_001088F5), 0);
    CHECK_EQ_INT((int)DSW(DS_00104AFA), 5);
    CHECK_EQ_INT((int)DSD(DS_00104B00), (int)0xBEEF001Au);
    /* The mask: bits 20..21 only (0x00CF0000 -> 1, 0x00100000 -> 3). */
    config_field_set(0x29u, 0x00CF0000u);
    game_hook_259cc();
    CHECK_EQ_INT((int)DSD(DS_00104ADC), 1);
    config_field_set(0x29u, 0x00100000u);
    game_hook_259cc();
    CHECK_EQ_INT((int)DSD(DS_00104ADC), 3);
    /* DS_00104B1D == 3: mode 0x1A with 0x30, then DS_00104B14 = 1 and
     * DS_00104B21 = 0. */
    DSB(DS_00104B1D) = 3u;
    DSB(DS_00104B14) = 0x77u;
    DSB(DS_00104B21) = 0x77u;
    mt_seed_arm();
    game_hook_259cc();
    CHECK_EQ_INT((int)DSB(DS_00104B14), 1);
    CHECK_EQ_INT((int)DSB(DS_00104B21), 0);
    CHECK_EQ_INT((int)DSB(DS_00104B25), 1);
    CHECK_EQ_INT((int)DSD(DS_00104AE4), 0x25BBC);
    CHECK_EQ_INT((int)DSW(DS_00104AFA), 0x30);
    CHECK_EQ_INT((int)DSD(DS_00104B00), (int)0xBEEF001Au);

    /* (c) 0x26978 and 0x27134 arm mode 0x1A with 0x23 / 5 and their hooks;
     * 0x27134 zeroes DS_00104B1E/AF3/AF2, 0x26978 does not. */
    MT_RESTORE();
    DSB(DS_00104B1E) = 0x77u;
    DSB(DS_00104AF3) = 0x77u;
    DSB(DS_00104AF2) = 0x77u;
    mt_seed_arm();
    game_hook_26978();
    CHECK_EQ_INT((int)DSB(DS_00104B25), 1);
    CHECK_EQ_INT((int)DSD(DS_00104AE4), 0x26998);
    CHECK_EQ_INT((int)DSB(DS_001088F5), 0);
    CHECK_EQ_INT((int)DSW(DS_00104AFA), 0x23);
    CHECK_EQ_INT((int)DSD(DS_00104B00), (int)0xBEEF001Au);
    CHECK_EQ_INT((int)DSB(DS_00104B1E), 0x77);
    CHECK_EQ_INT((int)DSB(DS_00104AF3), 0x77);
    mt_seed_arm();
    game_hook_27134();
    CHECK_EQ_INT((int)DSB(DS_00104B1E), 0);
    CHECK_EQ_INT((int)DSB(DS_00104AF3), 0);
    CHECK_EQ_INT((int)DSB(DS_00104AF2), 0);
    CHECK_EQ_INT((int)DSB(DS_00104B25), 1);
    CHECK_EQ_INT((int)DSD(DS_00104AE4), 0x270BC);
    CHECK_EQ_INT((int)DSB(DS_001088F5), 0);
    CHECK_EQ_INT((int)DSW(DS_00104AFA), 5);
    CHECK_EQ_INT((int)DSD(DS_00104B00), (int)0xBEEF001Au);

    /* (d) 0x24B54: 0x4F1E4 (DS_00104B15 = 0), 0x2BAF4 (the record is gone),
     * the mode word 0x27 (the upper word kept), the bytes DS_00104B1D and
     * DS_00104B1F = 0 with DS_00104B1E between them untouched, and the dword
     * DS_00104AB8 = 0. */
    MT_RESTORE();
    mt_seed_record();
    DSB(DS_00104B15) = 0x77u;
    DSD(DS_00104B00) = 0xBEEF7777u;
    DSB(DS_00104B1D) = 0x77u;
    DSB(DS_00104B1E) = 0x66u;
    DSB(DS_00104B1F) = 0x77u;
    DSB(DS_00104B1F + 1u) = 0x66u;
    DSD(DS_00104AB8) = 0xDEADBEEFu;
    game_hook_24b54();
    CHECK_EQ_INT((int)mt_record_seen(), 0);
    CHECK_EQ_INT((int)DSB(DS_00104B15), 0);
    CHECK_EQ_INT((int)DSD(DS_00104B00), (int)0xBEEF0027u);
    CHECK_EQ_INT((int)DSB(DS_00104B1D), 0);
    CHECK_EQ_INT((int)DSB(DS_00104B1E), 0x66);
    CHECK_EQ_INT((int)DSB(DS_00104B1F), 0);
    CHECK_EQ_INT((int)DSB(DS_00104B1F + 1u), 0x66);
    CHECK_EQ_INT((int)DSD(DS_00104AB8), 0);

    /* (e) 0x10E80 (game_state_init) and 0x2C304: the same resets as 0x24B54
     * for mode 3, the word DS_000F0A64, the bytes DS_000F0A71/6F, the dword
     * DS_000F0A5C = 4, DS_00105C00 = ((field & 0xF0000) >> 16) + 1 (3 + 1),
     * DS_00104AB8 = 0 and the byte DS_00104B1F = 0. */
    MT_RESTORE();
    config_field_set(0x29u, 0x00F30000u);
    mt_seed_record();
    DSB(DS_00104B15) = 0x77u;
    DSD(DS_00104B00) = 0xBEEF7777u;
    DSD(DS_000F0A64) = 0xBEEF7777u;
    DSB(DS_000F0A71) = 0x77u;
    DSD(DS_000F0A5C) = 0xDEADBEEFu;
    DSB(DS_000F0A6F) = 0x77u;
    DSD(DS_00105C00) = 0xDEADBEEFu;
    DSD(DS_00104AB8) = 0xDEADBEEFu;
    DSB(DS_00104B1E) = 0x66u;
    DSB(DS_00104B1F) = 0x77u;
    DSB(DS_00104B1F + 1u) = 0x66u;
    game_state_init();
    CHECK_EQ_INT((int)mt_record_seen(), 0);
    CHECK_EQ_INT((int)DSB(DS_00104B15), 0);
    CHECK_EQ_INT((int)DSD(DS_00104B00), (int)0xBEEF0003u);
    CHECK_EQ_INT((int)DSD(DS_000F0A64), (int)0xBEEF0000u);
    CHECK_EQ_INT((int)DSB(DS_000F0A71), 0);
    CHECK_EQ_INT((int)DSD(DS_000F0A5C), 4);
    CHECK_EQ_INT((int)DSB(DS_000F0A6F), 0);
    CHECK_EQ_INT((int)DSD(DS_00105C00), 4);
    CHECK_EQ_INT((int)DSD(DS_00104AB8), 0);
    CHECK_EQ_INT((int)DSB(DS_00104B1E), 0x66);
    CHECK_EQ_INT((int)DSB(DS_00104B1F), 0);
    CHECK_EQ_INT((int)DSB(DS_00104B1F + 1u), 0x66);
    config_field_set(0x29u, 0x000A0000u);
    DSD(DS_00105C00) = 0xDEADBEEFu;
    config_credits_init();
    CHECK_EQ_INT((int)DSD(DS_00105C00), 0xB);

    /* (f) 0x4246C: the seven stage bytes and DS_00108111 = 0, their
     * neighbours DS_00108105, DS_0010810D, DS_00108110 and DS_00108112 kept. */
    MT_RESTORE();
    for (u32 i = 0; i < 0x10u; i++) DSB(DS_00108104 + i) = 0x77u;
    fight_stage_marks_clear();
    for (u32 i = 0; i < 0x10u; i++) {
        u32 a = DS_00108104 + i;
        int zero = (a >= DS_00108106 && a < DS_00108106 + 7u) || a == DS_00108111;
        CHECK_EQ_INT((int)DSB(a), zero ? 0 : 0x77);
    }

    /* (g) 0x25AE8, DS_00104B1D == 0: DS_00104B14 = 0, 0x4F1E4, 0x2BAF4,
     * DS_00104ABC = 1 (DS_00104B1F != 3), string 0x52 at row 0xE (the same
     * glyphs as a direct 0x2F510 call, which ORs the class bit 2 into 0x4000;
     * 0x2F4BC with 0x4000 draws others), 0x4246C, then mode
     * 0x17 with the countdowns 0xB4 and the hook 0x10E80. */
    MT_RESTORE();
    game_string_table_load("data/game/C");
    const u8 *s52 = game_string_get(0x52u);
    CHECK(s52[0] != 0u, "string 0x52 is not empty");
    mt_seed_record();
    DSB(DS_00104B14) = 0x77u;
    DSB(DS_00104B15) = 0x77u;
    DSB(DS_00104B1D) = 0u;
    DSB(DS_00104B1F) = 2u;
    DSD(DS_00104ABC) = 0xDEADBEEFu;
    for (u32 i = 0; i < 0x10u; i++) DSB(DS_00108104 + i) = 0x77u;
    mt_seed_arm();
    DSW(DS_001088EE) = 0x7777u;
    DSW(DS_00104AFE) = 0x7777u;
    mem_fill(DS_00105F38 + 0xDu * 0xACu, 0, 3u * 0xACu);
    game_hook_25ae8();
    CHECK_EQ_INT((int)mt_record_seen(), 0);
    CHECK_EQ_INT((int)DSB(DS_00104B14), 0);
    CHECK_EQ_INT((int)DSB(DS_00104B15), 0);
    CHECK_EQ_INT((int)DSD(DS_00104ABC), 1);
    CHECK_EQ_INT((int)DSB(DS_00108106), 0);             /* 0x4246C */
    CHECK_EQ_INT((int)DSB(DS_00108111), 0);
    CHECK_EQ_INT((int)DSB(DS_00108104 + 1u), 0x77);
    CHECK_EQ_INT((int)DSD(DS_00104AE4), 0x10E80);
    CHECK_EQ_INT((int)DSW(DS_001088EE), 0xB4);
    CHECK_EQ_INT((int)DSW(DS_00104AFE), 0xB4);
    CHECK_EQ_INT((int)DSD(DS_00104B00), (int)0xBEEF0017u);
    CHECK_EQ_INT((int)DSB(DS_001088F5), 0x77);          /* no 0x4F980 */
    {
        u32 g[43], n = 0;
        for (u32 c = 0; c < 43u; c++) {
            u32 r = chs_grid(0xEu, c);
            g[c] = r != 0u ? DSD(r + 0x08u) : 0u;
            if (g[c] != 0u) n++;
            CHECK_EQ_INT((int)chs_grid(0xDu, c), 0);
            CHECK_EQ_INT((int)chs_grid(0xFu, c), 0);
        }
        CHECK(n != 0u, "0x25AE8 drew string 0x52 at row 0xE");
        mem_fill(DS_00105F38 + 0xEu * 0xACu, 0, 0xACu);
        text_cursor_hold_font2(-1, 0xE, game_string_get(0x52u), 0x4000u);
        u32 same = 1;
        for (u32 c = 0; c < 43u; c++) {
            u32 r = chs_grid(0xEu, c);
            if ((r != 0u ? DSD(r + 0x08u) : 0u) != g[c]) same = 0;
        }
        CHECK_EQ_INT((int)same, 1);
        mem_fill(DS_00105F38 + 0xEu * 0xACu, 0, 0xACu);
        text_cursor_hold(-1, 0xE, game_string_get(0x52u), 0x4000u);
        u32 diff = 0;
        for (u32 c = 0; c < 43u; c++) {
            u32 r = chs_grid(0xEu, c);
            if ((r != 0u ? DSD(r + 0x08u) : 0u) != g[c]) diff = 1;
        }
        CHECK(diff != 0u, "0x2F4BC's mode 0x4000 (no class bit) draws other glyphs");
    }
    /* DS_00104B1D = 2 (not 0) and DS_00104B1F == 3: DS_00104ABC = 2 and the
     * hook 0x24B54. */
    mt_seed_record();
    DSB(DS_00104B1D) = 2u;
    DSB(DS_00104B1F) = 3u;
    DSD(DS_00104ABC) = 0xDEADBEEFu;
    mt_seed_arm();
    DSW(DS_001088EE) = 0x7777u;
    DSW(DS_00104AFE) = 0x7777u;
    game_hook_25ae8();
    CHECK_EQ_INT((int)DSD(DS_00104ABC), 2);
    CHECK_EQ_INT((int)DSD(DS_00104AE4), 0x24B54);
    CHECK_EQ_INT((int)DSW(DS_001088EE), 0xB4);
    CHECK_EQ_INT((int)DSW(DS_00104AFE), 0xB4);
    CHECK_EQ_INT((int)DSD(DS_00104B00), (int)0xBEEF0017u);

    /* (h) 0x4142C: DS_00104AE8 = 0, 0x4F1E4, 0x2BAF4, the 0x38B18 row
     * (0xC86F0, id 0x2BEF), 0x413C8's spawns (0xC86DC, id 0x3F12, at x
     * 0x2A00 on layer 0xE0 into DS_001080F4; seven children of it on layer
     * 0xE2 into DS_001080C0[0..6], each with the id of 0xC85BC[i]'s
     * descriptor), the four zero bytes, DS_00108112 = 0, DS_00104B25 = 8,
     * mode 0x12, the countdown 0x1E and DS_00104B23 = 0. */
    MT_RESTORE();
    mt_seed_record();
    DSD(DS_00104AE8) = 0xDEADBEEFu;
    DSB(DS_00104B15) = 0x77u;
    mem_fill(DS_00107A1C, 0u, 0x1Cu);
    mem_fill(DS_001080C0 - 4u, 0x77u, 0x24u);
    DSD(DS_001080F4) = 0xDEADBEEFu;
    for (u32 i = 0; i < 0x10u; i++) DSB(DS_00108104 + i) = 0x77u;
    DSD(DS_00104B00) = 0xBEEF7777u;
    DSW(DS_00104AFE) = 0x7777u;
    DSB(DS_00104B25) = 0x77u;
    DSB(DS_00104B23) = 0x77u;
    fight_hook_4142c();
    CHECK_EQ_INT((int)mt_record_seen(), 0);
    CHECK_EQ_INT((int)DSD(DS_00104AE8), 0);
    CHECK_EQ_INT((int)DSB(DS_00104B15), 0);
    {
        u32 row = DSD(DS_00107A1C);
        CHECK(row != 0u, "0x38B18 filled the row table");
        if (row != 0u) CHECK_EQ_INT((int)DSD(row + 0x08u), 0x2BEF);
        u32 p = DSD(DS_001080F4);
        CHECK(p != 0u && p != 0xDEADBEEFu, "0x413C8's parent spawn");
        if (p != 0u && p != 0xDEADBEEFu) {
            CHECK_EQ_INT((int)DSD(p + 0x08u), 0x3F12);
            CHECK_EQ_INT((int)DSD(p + 0x18u), 0x2A00);
            CHECK_EQ_INT((int)DSD(p + 0x1Cu), 0x1B00);
            CHECK_EQ_INT((int)DSB(p + 0x49u), 0xE0);
            for (u32 i = 0; i < 7u; i++) {
                u32 c = DSD(DS_001080C0 + i * 4u);
                CHECK(c != 0u && c != 0x77777777u, "0x413C8's child spawn");
                if (c == 0u || c == 0x77777777u) continue;
                CHECK_EQ_INT((int)DSD(c + 0x08u),
                             (int)DSD(DSD(0x000C85BCu + i * 4u)));
                CHECK_EQ_INT((int)DSB(c + 0x49u), 0xE2);
                CHECK_EQ_INT((int)DSW(c + 0x34u), 0);
                CHECK_EQ_INT((int)DSW(c + 0x36u), 0);
                CHECK_EQ_INT((int)DSB(c + 0x4Au), (int)(DSW(p + 0x56u) & 0x7Fu));
                CHECK((DSW(c + 0x28u) & 0x400u) != 0u, "a child (a5 | 0x400)");
            }
        }
        CHECK_EQ_INT((int)DSD(DS_001080C0 - 4u), 0x77777777);
        CHECK_EQ_INT((int)DSD(DS_001080C0 + 0x1Cu), 0x77777777);
    }
    for (u32 i = 0; i < 0x10u; i++) {
        u32 a = DS_00108104 + i;
        int zero = a == DS_00108104 || a == DS_00108104 + 1u || a == DS_0010810F
                   || a == DS_00108111 || a == DS_00108112;
        CHECK_EQ_INT((int)DSB(a), zero ? 0 : 0x77);
    }
    CHECK_EQ_INT((int)DSB(DS_00104B25), 8);
    CHECK_EQ_INT((int)DSD(DS_00104B00), (int)0xBEEF0012u);
    CHECK_EQ_INT((int)DSW(DS_00104AFE), 0x1E);
    CHECK_EQ_INT((int)DSB(DS_00104B23), 0);

    /* (i) The chain: 0x259CC as mode 0x17's hook installs 0x25BBC for mode
     * 0x1A; 18 wipe-in frames run it (the round byte 0 -> 1, both fighters,
     * the hook 0x5D812, mode 0x1B). */
    MT_RESTORE();
    mh_seed_fighters();
    DSB(DS_00104B1D) = 0u;
    DSD(DS_000C98F0) = 0u;
    config_field_set(0x29u, 0u);
    game_hook_259cc();
    CHECK_EQ_INT((int)DSB(DS_00104B1E), 0);
    CHECK_EQ_INT((int)DSD(DS_00104ADC), 1);
    for (u32 k = 1; k <= 17u; k++) frontend_mode_1a_step();
    CHECK_EQ_INT((int)DSD(DS_00104AE4), 0x25BBC);
    frontend_mode_1a_step();
    CHECK_EQ_INT((int)DSB(DS_00104B1E), 1);
    CHECK_EQ_INT((int)DSD(DS_001077A8), (int)DS_001077B0);
    CHECK_EQ_INT((int)DSD(DS_001077A8 + 4u), (int)(DS_001077B0 + 0x94u));
    CHECK_EQ_INT((int)DSD(DS_00104AE4), 0x5D812);
    CHECK_EQ_INT((int)DSW(DS_00104B00), 0x1B);
    CHECK_EQ_INT((int)DSW(DS_00104AFA), 5);
#undef MT_RESTORE

    tf_put(s_data, 0x80000u, sizeof s_data);
    tf_put(s_rec, rec_pool, sizeof s_rec);
    tf_put(s_pset, pset_pool, sizeof s_pset);
    tf_put(s_bufa, bufa, sizeof s_bufa);
    tf_put(s_bufb, bufb, sizeof s_bufb);
    tf_put(s_res, res_tab, res_len);
    memcpy(gfx_aperture(), s_ap, sizeof s_ap);
    memcpy(gfx_dac, s_dac, sizeof s_dac);
}

/* Record §46-G: one 0x4F318 step from sentinels. The words either side of
 * DS_001088EE (0x1088EC, 0x1088F0) and DS_00104AFE (0x104AFC, the mode word
 * 0x104B00) are seeded to catch a width change. The hook is 0x26978, which
 * stores data only: DS_00104B25 = 1, the hook 0x26998, DS_001088F5 = 0,
 * DS_00104AFA = 0x23 and mode 0x1A. */
static void ms_step(u32 hold, u32 afe, u32 held, u32 pressed)
{
    DSW(DS_001088EE - 2u) = 0x6666u;
    DSW(DS_001088EE) = (u16)hold;
    DSW(DS_001088EE + 2u) = 0x5555u;
    DSW(DS_00104AFE - 2u) = 0x4444u;
    DSW(DS_00104AFE) = (u16)afe;
    DSD(DS_00104B00) = 0xBEEF7777u;
    DSD(DS_001088D8) = held;
    DSD(DS_001088E4) = pressed;
    DSD(DS_00104AE4) = 0x26978u;
    DSB(DS_00104B25) = 0x77u;
    DSB(DS_001088F5) = 0x77u;
    DSW(DS_00104AFA) = 0x7777u;
    frontend_mode_17_step();
}

/* Whether ms_step's hook ran; the neighbours and the input words kept. */
static u32 ms_fired(u32 held, u32 pressed)
{
    CHECK_EQ_INT((int)DSW(DS_001088EE - 2u), 0x6666);
    CHECK_EQ_INT((int)DSW(DS_001088EE + 2u), 0x5555);
    CHECK_EQ_INT((int)DSW(DS_00104AFE - 2u), 0x4444);
    CHECK_EQ_INT((int)DSD(DS_001088D8), (int)held);
    CHECK_EQ_INT((int)DSD(DS_001088E4), (int)pressed);
    if (DSD(DS_00104AE4) == 0x26978u) {
        CHECK_EQ_INT((int)DSD(DS_00104B00), (int)0xBEEF7777u);
        CHECK_EQ_INT((int)DSB(DS_00104B25), 0x77);
        return 0u;
    }
    CHECK_EQ_INT((int)DSD(DS_00104AE4), 0x26998);
    CHECK_EQ_INT((int)DSD(DS_00104B00), (int)0xBEEF001Au);
    CHECK_EQ_INT((int)DSB(DS_00104B25), 1);
    CHECK_EQ_INT((int)DSB(DS_001088F5), 0);
    CHECK_EQ_INT((int)DSW(DS_00104AFA), 0x23);
    return 1u;
}

static void check_mode_17_step(void)
{
    static u8 s_data[0x10B0D0u - 0x80000u];
    tf_snap(s_data, 0x80000u, sizeof s_data);
    const u32 M0 = 0x0F000000u, M1 = 0x00000F00u;

    /* (a) The masks 0xC9898[0..1] and 0x4F778 on each. */
    CHECK_EQ_INT((int)DSD(DS_000C9898), (int)M0);
    CHECK_EQ_INT((int)DSD(DS_000C9898 + 4u), (int)M1);
    DSD(DS_001088E4) = 0x00000100u;
    CHECK_EQ_INT((int)frontend_buttons_pressed(1u), 1);
    CHECK_EQ_INT((int)frontend_buttons_pressed(0u), 0);
    DSD(DS_001088E4) = 0x08000000u;
    CHECK_EQ_INT((int)frontend_buttons_pressed(0u), 1);
    CHECK_EQ_INT((int)frontend_buttons_pressed(1u), 0);

    /* (b) 0x4F790: either mask held whole gives 2 (over a press), a press
     * of a bit of either mask gives 1, anything else 0. */
    static const u32 sk[][3] = {
        /* held, pressed, AL */
        { 0x0F000000u, 0x01000000u, 2u }, { 0x00000F00u, 0u, 2u },
        { 0xFFFFFFFFu, 0u, 2u },          { 0x07000F00u, 0u, 2u },
        { 0x07000700u, 0u, 0u },          { 0x0E000E00u, 0u, 0u },
        { 0x07000000u, 0x01000000u, 1u }, { 0u, 0x00000800u, 1u },
        { 0u, 0xF0FFF0FFu, 0u },          { 0u, 0u, 0u },
    };
    for (u32 k = 0; k < sizeof sk / sizeof sk[0]; k++) {
        DSD(DS_001088D8) = sk[k][0];
        DSD(DS_001088E4) = sk[k][1];
        CHECK_EQ_INT((int)frontend_skip_check(), (int)sk[k][2]);
        CHECK_EQ_INT((int)DSD(DS_001088D8), (int)sk[k][0]);
    }

    /* (c) DS_001088EE != 0: it counts down and the skip test does not run
     * (the held mask would zero the countdown). */
    ms_step(5u, 0x10u, M0, 0u);
    CHECK_EQ_INT((int)DSW(DS_001088EE), 4);
    CHECK_EQ_INT((int)DSW(DS_00104AFE), 0x0F);
    CHECK_EQ_INT((int)ms_fired(M0, 0u), 0);

    /* (d) DS_001088EE == 0, no input: DS_00104AFE - 1 only. */
    ms_step(0u, 0x10u, 0u, 0u);
    CHECK_EQ_INT((int)DSW(DS_001088EE), 0);
    CHECK_EQ_INT((int)DSW(DS_00104AFE), 0x0F);
    CHECK_EQ_INT((int)ms_fired(0u, 0u), 0);

    /* (e) A press takes 0x3C more (either mask); 0x3D leaves 0, not fired;
     * 0x3C leaves -1, fired. */
    ms_step(0u, 0x100u, 0u, 0x01000000u);
    CHECK_EQ_INT((int)DSW(DS_00104AFE), 0x100 - 0x3C - 1);
    CHECK_EQ_INT((int)ms_fired(0u, 0x01000000u), 0);
    ms_step(0u, 0x100u, 0u, 0x00000200u);
    CHECK_EQ_INT((int)DSW(DS_00104AFE), 0x100 - 0x3C - 1);
    CHECK_EQ_INT((int)ms_fired(0u, 0x00000200u), 0);
    ms_step(0u, 0x3Du, 0u, 0x00000200u);
    CHECK_EQ_INT((int)DSW(DS_00104AFE), 0);
    CHECK_EQ_INT((int)ms_fired(0u, 0x00000200u), 0);
    CHECK_EQ_INT((int)DSW(DS_001088EE), 0);
    ms_step(0u, 0x3Cu, 0u, 0x00000200u);
    CHECK_EQ_INT((int)DSW(DS_00104AFE), 0xFFFF);
    CHECK_EQ_INT((int)ms_fired(0u, 0x00000200u), 1);
    CHECK_EQ_INT((int)DSW(DS_001088EE), 0xFFFF);

    /* (f) A held mask zeroes the countdown: it fires at once. */
    ms_step(0u, 0x100u, M1, 0u);
    CHECK_EQ_INT((int)DSW(DS_00104AFE), 0xFFFF);
    CHECK_EQ_INT((int)ms_fired(M1, 0u), 1);
    CHECK_EQ_INT((int)DSW(DS_001088EE), 0xFFFF);

    /* (g) The signed test on the old value: 1 does not fire, 0 and 0x8000
     * do, 0x7FFF does not; a non-zero DS_001088EE is then overwritten. */
    ms_step(3u, 1u, 0u, 0u);
    CHECK_EQ_INT((int)DSW(DS_00104AFE), 0);
    CHECK_EQ_INT((int)ms_fired(0u, 0u), 0);
    CHECK_EQ_INT((int)DSW(DS_001088EE), 2);
    ms_step(3u, 0u, 0u, 0u);
    CHECK_EQ_INT((int)DSW(DS_00104AFE), 0xFFFF);
    CHECK_EQ_INT((int)ms_fired(0u, 0u), 1);
    CHECK_EQ_INT((int)DSW(DS_001088EE), 0xFFFF);
    ms_step(3u, 0x8000u, 0u, 0u);
    CHECK_EQ_INT((int)DSW(DS_00104AFE), 0x7FFF);
    CHECK_EQ_INT((int)ms_fired(0u, 0u), 1);
    ms_step(3u, 0x7FFFu, 0u, 0u);
    CHECK_EQ_INT((int)DSW(DS_00104AFE), 0x7FFE);
    CHECK_EQ_INT((int)ms_fired(0u, 0u), 0);

    /* (h) After firing, DS_001088EE = 0xFFFF counts down without the skip
     * test, and an unregistered hook is skipped. */
    ms_step(0xFFFFu, 0x10u, M0, 0u);
    CHECK_EQ_INT((int)DSW(DS_001088EE), 0xFFFE);
    CHECK_EQ_INT((int)DSW(DS_00104AFE), 0x0F);
    DSD(DS_00104AE4) = 0xDEADBEEFu;
    DSW(DS_00104AFE) = 0u;
    DSW(DS_001088EE) = 0u;
    frontend_mode_17_step();
    CHECK_EQ_INT((int)DSW(DS_001088EE), 0xFFFF);
    CHECK_EQ_INT((int)DSD(DS_00104AE4), (int)0xDEADBEEFu);

    /* (i) The hook runs after the 0xFFFF store: 0x29B74 re-arms DS_001088EE
     * = 0x78. The list it darkens is emptied and the effect pool guard
     * DS_000FCCE0 = 0, so it spawns and tears down nothing. */
    DSD(DS_000FCCE0) = 0u;
    mem_fill(DS_00107608, 0u, DS_00107798 - DS_00107608);
    DSD(DS_00104AE4) = 0x29B74u;
    DSW(DS_00104AFE) = 0u;
    DSW(DS_001088EE) = 3u;
    DSD(DS_00104B00) = 0xBEEF7777u;
    frontend_mode_17_step();
    CHECK_EQ_INT((int)DSW(DS_001088EE), 0x78);
    CHECK_EQ_INT((int)DSW(DS_00104AFE), 0x78);
    CHECK_EQ_INT((int)DSD(DS_00104B00), (int)0xBEEF0015u);

    tf_put(s_data, 0x80000u, sizeof s_data);
}

/* Record §47-B: game_frame's mode switch (0x24C5C, table 0x24B8C) on the word
 * DS_00104B00. The upper word 0x104B02 holds 0xBEEF throughout, so a dword
 * (or byte) read of the mode dispatches nothing (or the wrong case). The
 * update table, the command block and the DS_00104B15 tail are gated off. */
static void ms_seed(u32 mode_dword)
{
    DSD(DS_00104AE8) = 0u;                  /* no update-table bit */
    DSB(DS_00104B19 + 2u) = 0u;             /* 0x24C7C: no command block */
    DSB(DS_00104B15) = 0u;                  /* no 0x25414 tail */
    DSD(DS_00104B00) = mode_dword;
}

static void check_mode_switch(void)
{
    static u8 s_data[0x10B0D0u - 0x80000u];
    static u8 s_rec[0xEBA0u], s_pset[0x4880u];
    static u8 s_bufa[0xFA00u], s_bufb[0xFA00u], s_ap[0xFA00u];
    static u8 s_dac[sizeof gfx_dac];
    static u8 s_res[0x14u * 128u];
    u32 rec_pool = DSD(DS_001014F4), pset_pool = DSD(DS_001014EC);
    u32 bufa = DSD(DS_001014E8), bufb = DSD(DS_001014E4);
    u32 res_tab = DSD(DS_001014E0);
    u32 res_len = DSD(DS_001014F0) * 0x14u;
    CHECK(rec_pool != 0u && pset_pool != 0u && bufa != 0u && bufb != 0u,
          "the mode switch needs the pools and buffers");
    CHECK(res_len != 0u && res_len <= sizeof s_res, "the resource table fits");
    if (rec_pool == 0u || pset_pool == 0u || bufa == 0u || bufb == 0u ||
        res_len == 0u || res_len > sizeof s_res)
        return;
    tf_snap(s_data, 0x80000u, sizeof s_data);
    tf_snap(s_rec, rec_pool, sizeof s_rec);
    tf_snap(s_pset, pset_pool, sizeof s_pset);
    tf_snap(s_bufa, bufa, sizeof s_bufa);
    tf_snap(s_bufb, bufb, sizeof s_bufb);
    tf_snap(s_res, res_tab, res_len);
    memcpy(s_ap, gfx_aperture(), sizeof s_ap);
    memcpy(s_dac, gfx_dac, sizeof s_dac);
#define MS_RESTORE() do { tf_put(s_data, 0x80000u, sizeof s_data);  \
        tf_put(s_rec, rec_pool, sizeof s_rec);                        \
        tf_put(s_pset, pset_pool, sizeof s_pset); } while (0)

    /* (a) Case 3 runs 0x11D04: state 9 counts its timer down (5 -> 4) with
     * the coin poll, the two tails and the overlay skipped. The word 0x0103
     * (low byte 3, above 0x33) and the 0x29B70 cases 1, 2 and 0x20 leave the
     * timer alone. */
    {
        static const u32 none[] = { 0xBEEF0103u, 0xBEEF0001u, 0xBEEF0002u,
                                    0xBEEF0020u };
        MS_RESTORE();
        DSB(DS_00104B1D) = 1u;
        DSB(DS_000F0A71) = 1u;
        DSB(DS_0009AD58) = 1u;
        DSW(DS_000F0A64) = 9u;
        DSW(DS_000F0A6A) = 5u;
        ms_seed(0xBEEF0003u);
        game_frame();
        CHECK_EQ_INT((int)DSW(DS_000F0A6A), 4);
        CHECK_EQ_INT((int)DSW(DS_000F0A64), 9);
        for (u32 i = 0; i < sizeof none / sizeof none[0]; i++) {
            DSW(DS_000F0A6A) = 5u;
            ms_seed(none[i]);
            game_frame();
            CHECK_EQ_INT((int)DSW(DS_000F0A6A), 5);
            CHECK_EQ_INT((int)DSD(DS_00104B00), (int)none[i]);
        }
    }

    /* (b) Case 0x11 (0x2538F, inline): DS_00104AFE = 0xF0, DS_001088EE = 0,
     * the hook 0x259CC and mode 0x17, the mode's upper word kept. */
    MS_RESTORE();
    DSW(DS_00104AFE) = 0x7777u;
    DSW(DS_001088EE) = 0x7777u;
    DSW(DS_001088EE + 2u) = 0x6666u;
    DSD(DS_00104AE4) = 0xDEADBEEFu;
    ms_seed(0xBEEF0011u);
    game_frame();
    CHECK_EQ_INT((int)DSW(DS_00104AFE), 0xF0);
    CHECK_EQ_INT((int)DSW(DS_001088EE), 0);
    CHECK_EQ_INT((int)DSW(DS_001088EE + 2u), 0x6666);
    CHECK_EQ_INT((int)DSD(DS_00104AE4), 0x259CC);
    CHECK_EQ_INT((int)DSD(DS_00104B00), (int)0xBEEF0017u);

    /* (c) Case 0x14 calls 0x25AE8: DS_00104B1D == 0 gives the hook 0x10E80,
     * mode 0x17 and the countdowns 0xB4, and DS_00104B14 = 0. */
    MS_RESTORE();
    game_string_table_load("data/game/C");
    mem_fill(DS_00105F38 + 0xDu * 0xACu, 0, 3u * 0xACu);
    DSB(DS_00104B14) = 0x77u;
    DSB(DS_00104B1D) = 0u;
    DSD(DS_00104AE4) = 0xDEADBEEFu;
    DSW(DS_001088EE) = 0x7777u;
    DSW(DS_00104AFE) = 0x7777u;
    ms_seed(0xBEEF0014u);
    game_frame();
    CHECK_EQ_INT((int)DSB(DS_00104B14), 0);
    CHECK_EQ_INT((int)DSD(DS_00104AE4), 0x10E80);
    CHECK_EQ_INT((int)DSW(DS_001088EE), 0xB4);
    CHECK_EQ_INT((int)DSW(DS_00104AFE), 0xB4);
    CHECK_EQ_INT((int)DSD(DS_00104B00), (int)0xBEEF0017u);

    /* (c2) Case 0x17 calls frontend_mode_17_step (0x4F318): with the
     * countdown DS_001088EE non-zero it just counts down, proving game_frame
     * really routes mode 0x17 there and not into the named-gap default. */
    MS_RESTORE();
    DSW(DS_001088EE) = 5u;
    DSW(DS_00104AFE) = 9u;
    DSD(DS_00104AE4) = 0xDEADBEEFu;
    ms_seed(0xBEEF0017u);
    game_frame();
    CHECK_EQ_INT((int)DSW(DS_001088EE), 4);
    CHECK_EQ_INT((int)DSW(DS_00104AFE), 8);
    CHECK_EQ_INT((int)DSD(DS_00104AE4), (int)0xDEADBEEFu);
    CHECK_EQ_INT((int)DSD(DS_00104B00), (int)0xBEEF0017u);

    /* (d) Case 0x1A calls 0x4F9A0: with the wipe-in counter past 0x10 the
     * record is dropped, the render gate set, the hook (0x29D60, a `ret`)
     * skipped, the counter cleared and the mode becomes 0x1B. */
    MS_RESTORE();
    actors_reset();
    DSD(DS_000C98F0) = actor_alloc(0);
    DSD(DS_00104AE4) = 0x29D60u;
    DSB(DS_001088F5) = 0x11u;
    DSB(DS_001088F4) = 0x77u;
    ms_seed(0xBEEF001Au);
    game_frame();
    CHECK_EQ_INT((int)DSD(DS_000C98F0), 0);
    CHECK_EQ_INT((int)DSB(DS_001088F4), 1);
    CHECK_EQ_INT((int)DSB(DS_001088F5), 0);
    CHECK_EQ_INT((int)DSD(DS_00104B00), (int)0xBEEF001Bu);

    /* (e) Case 0x1B calls 0x4F9C8: the wipe-out finishes and the mode takes
     * the saved word DS_00104AFA (0x26, a table entry that runs nothing). */
    MS_RESTORE();
    actors_reset();
    DSD(DS_000C98F0) = actor_alloc(0);
    DSD(DS_00104AE4) = 0x29D60u;
    DSB(DS_001088F5) = 0x11u;
    DSB(DS_001088F4) = 0x77u;
    DSW(DS_00104AFA) = 0x26u;
    ms_seed(0xBEEF001Bu);
    game_frame();
    CHECK_EQ_INT((int)DSD(DS_000C98F0), 0);
    CHECK_EQ_INT((int)DSB(DS_001088F4), 0x77);
    CHECK_EQ_INT((int)DSD(DS_00104B00), (int)0xBEEF0026u);
#undef MS_RESTORE

    tf_put(s_data, 0x80000u, sizeof s_data);
    tf_put(s_rec, rec_pool, sizeof s_rec);
    tf_put(s_pset, pset_pool, sizeof s_pset);
    tf_put(s_bufa, bufa, sizeof s_bufa);
    tf_put(s_bufb, bufb, sizeof s_bufb);
    tf_put(s_res, res_tab, res_len);
    memcpy(gfx_aperture(), s_ap, sizeof s_ap);
    memcpy(gfx_dac, s_dac, sizeof s_dac);
}

/* Record §47-M: the mode 0x10 handler 0x438B4 and its join test 0x43928.
 * m10_seed sets the sub-state DS_00108174, the side bytes DS_00108170[0..1]
 * with 0x77 either side, the byte DS_00108172 (0x5A, what a finished pass
 * copies), the countdown word DS_0010816C with 0x66 above it, the credit
 * layer (no free play, `credits`, DS_00104B1F = b1f) and the pressed/held
 * words DS_001088E4/DS_001088D8. The image masks: 0x9ACBC = 0x01000000 (side
 * 0) and 0x100 (side 1); 0xC9898 = 0x0F000000 and 0xF00. */
static void m10_seed(u32 b1d, u32 sub, u32 cd, u32 credits, u32 b1f,
                     u32 pressed, u32 held)
{
    DSB(DS_00104B1D) = (u8)b1d;
    DSB(DS_00108174) = (u8)sub;
    DSB(DS_00108174 + 1u) = 0x77u;
    DSB(DS_00108170 - 1u) = 0x77u;
    DSB(DS_00108170) = 0u;
    DSB(DS_00108170 + 1u) = 0u;
    DSB(DS_00108172) = 0x5Au;
    DSB(DS_00108172 + 1u) = 0x77u;
    DSW(DS_0010816C) = (u16)cd;
    DSB(DS_0010816C + 2u) = 0x66u;
    DSB(DS_00105D60) = 0u;
    DSD(DS_00105C00) = credits;
    DSB(DS_00104B1F) = (u8)b1f;
    DSD(DS_001088E4) = pressed;
    DSD(DS_001088D8) = held;
}

static void check_mode_10_step(void)
{
    static u8 s_data[0x10B0D0u - 0x80000u];
    static u8 s_rec[0xEBA0u], s_pset[0x4880u];
    static u8 s_bufa[0xFA00u], s_bufb[0xFA00u], s_ap[0xFA00u];
    static u8 s_dac[sizeof gfx_dac];
    static u8 s_res[0x14u * 128u];
    u32 rec_pool = DSD(DS_001014F4), pset_pool = DSD(DS_001014EC);
    u32 bufa = DSD(DS_001014E8), bufb = DSD(DS_001014E4);
    u32 res_tab = DSD(DS_001014E0);
    u32 res_len = DSD(DS_001014F0) * 0x14u;
    CHECK(rec_pool != 0u && pset_pool != 0u && bufa != 0u && bufb != 0u,
          "mode 0x10 needs the pools and buffers");
    CHECK(res_len != 0u && res_len <= sizeof s_res, "the resource table fits");
    if (rec_pool == 0u || pset_pool == 0u || bufa == 0u || bufb == 0u ||
        res_len == 0u || res_len > sizeof s_res)
        return;
    tf_snap(s_data, 0x80000u, sizeof s_data);
    tf_snap(s_rec, rec_pool, sizeof s_rec);
    tf_snap(s_pset, pset_pool, sizeof s_pset);
    tf_snap(s_bufa, bufa, sizeof s_bufa);
    tf_snap(s_bufb, bufb, sizeof s_bufb);
    tf_snap(s_res, res_tab, res_len);
    memcpy(s_ap, gfx_aperture(), sizeof s_ap);
    memcpy(s_dac, gfx_dac, sizeof s_dac);
    CHECK_EQ_INT((int)DSD(DS_0009ACBC), 0x01000000);
    CHECK_EQ_INT((int)DSD(DS_0009ACBC + 4u), 0x100);
#define M10_RESTORE() tf_put(s_data, 0x80000u, sizeof s_data)

    /* (a) 0x43928. Both sides pressed with 5 credits and DS_00104B1F = 0:
     * side 0 is debited (B1F still 0), then side 1 is not (B1F is 1 by then),
     * so 4 credits are left; both bytes 1, B1F = 3, neighbours kept. */
    M10_RESTORE();
    m10_seed(0u, 1u, 5u, 5u, 0u, 0x01000100u, 0u);
    fight_char_join();
    CHECK_EQ_INT((int)DSB(DS_00104B1F), 3);
    CHECK_EQ_INT((int)DSB(DS_00108170), 1);
    CHECK_EQ_INT((int)DSB(DS_00108170 + 1u), 1);
    CHECK_EQ_INT((int)DSD(DS_00105C00), 4);
    CHECK_EQ_INT((int)DSB(DS_00108170 - 1u), 0x77);
    CHECK_EQ_INT((int)DSB(DS_00108172), 0x5A);
    CHECK_EQ_INT((int)DSB(DS_00108174), 1);

    /* (a2) Side 1's byte already 5: it is not polled (B1F gets no bit 2 and
     * the byte stays 5); side 0 joins with B1F = 0x40 kept in the or, and no
     * debit since B1F is non-zero. */
    M10_RESTORE();
    m10_seed(0u, 1u, 5u, 5u, 0x40u, 0x01000100u, 0u);
    DSB(DS_00108170 + 1u) = 5u;
    fight_char_join();
    CHECK_EQ_INT((int)DSB(DS_00104B1F), 0x41);
    CHECK_EQ_INT((int)DSB(DS_00108170), 1);
    CHECK_EQ_INT((int)DSB(DS_00108170 + 1u), 5);
    CHECK_EQ_INT((int)DSD(DS_00105C00), 5);

    /* (a3) Only side 1's mask pressed: bit 2 and byte 1 for side 1 only. */
    M10_RESTORE();
    m10_seed(0u, 1u, 5u, 5u, 0u, 0x100u, 0u);
    fight_char_join();
    CHECK_EQ_INT((int)DSB(DS_00104B1F), 2);
    CHECK_EQ_INT((int)DSB(DS_00108170), 0);
    CHECK_EQ_INT((int)DSB(DS_00108170 + 1u), 1);
    CHECK_EQ_INT((int)DSD(DS_00105C00), 4);

    /* (a4) No credit: both pressed, nothing joins. */
    M10_RESTORE();
    m10_seed(0u, 1u, 5u, 0u, 0x40u, 0x01000100u, 0u);
    fight_char_join();
    CHECK_EQ_INT((int)DSB(DS_00104B1F), 0x40);
    CHECK_EQ_INT((int)DSB(DS_00108170), 0);
    CHECK_EQ_INT((int)DSB(DS_00108170 + 1u), 0);

    /* (b) 0x438B4, DS_00104B1D = 0, sub-state 1, no input: the countdown
     * word runs 2 -> 1 (sub-state kept), then 1 -> 0 and the sub-state takes
     * DS_00108172. The byte above the countdown keeps 0x66. */
    M10_RESTORE();
    m10_seed(0u, 1u, 2u, 0u, 0x40u, 0u, 0u);
    fight_mode_10_step();
    CHECK_EQ_INT((int)DSW(DS_0010816C), 1);
    CHECK_EQ_INT((int)DSB(DS_00108174), 1);
    fight_mode_10_step();
    CHECK_EQ_INT((int)DSW(DS_0010816C), 0);
    CHECK_EQ_INT((int)DSB(DS_00108174), 0x5A);
    CHECK_EQ_INT((int)DSB(DS_0010816C + 2u), 0x66);
    CHECK_EQ_INT((int)DSB(DS_00108174 + 1u), 0x77);

    /* (b2) The test is signed on the new word: 0x8000 - 1 = 0x7FFF > 0 keeps
     * the sub-state; 0 - 1 = 0xFFFF (-1) ends it. */
    M10_RESTORE();
    m10_seed(0u, 1u, 0x8000u, 0u, 0x40u, 0u, 0u);
    fight_mode_10_step();
    CHECK_EQ_INT((int)DSW(DS_0010816C), 0x7FFF);
    CHECK_EQ_INT((int)DSB(DS_00108174), 1);
    DSW(DS_0010816C) = 0u;
    fight_mode_10_step();
    CHECK_EQ_INT((int)DSW(DS_0010816C), 0xFFFF);
    CHECK_EQ_INT((int)DSB(DS_00108174), 0x5A);

    /* (c) 0x4F790 non-zero ends the pass at once, the countdown untouched:
     * side 0's mask 0x01000000 newly pressed (AL 1, no credit so no join),
     * then the held mask 0x0F000000 (AL 2). */
    M10_RESTORE();
    m10_seed(0u, 1u, 9u, 0u, 0x40u, 0x01000000u, 0u);
    fight_mode_10_step();
    CHECK_EQ_INT((int)DSW(DS_0010816C), 9);
    CHECK_EQ_INT((int)DSB(DS_00108174), 0x5A);
    CHECK_EQ_INT((int)DSB(DS_00104B1F), 0x40);
    M10_RESTORE();
    m10_seed(0u, 1u, 9u, 0u, 0x40u, 0u, 0x0F000000u);
    fight_mode_10_step();
    CHECK_EQ_INT((int)DSW(DS_0010816C), 9);
    CHECK_EQ_INT((int)DSB(DS_00108174), 0x5A);

    /* (d) DS_00104B1D == 3: no input returns with no countdown (9 kept,
     * sub-state 1 even with the word at 1); a press ends the pass. */
    M10_RESTORE();
    m10_seed(3u, 1u, 1u, 0u, 0x40u, 0u, 0u);
    fight_mode_10_step();
    CHECK_EQ_INT((int)DSW(DS_0010816C), 1);
    CHECK_EQ_INT((int)DSB(DS_00108174), 1);
    m10_seed(3u, 1u, 9u, 0u, 0x40u, 0x100u, 0u);
    fight_mode_10_step();
    CHECK_EQ_INT((int)DSW(DS_0010816C), 9);
    CHECK_EQ_INT((int)DSB(DS_00108174), 0x5A);

    /* (e) The join runs inside the pass (both DS_00104B1D copies): side 1
     * pressed with credits joins (B1F |= 2), and the same press (in 0xF00)
     * ends the pass. */
    for (u32 b1d = 0u; b1d <= 3u; b1d += 3u) {
        M10_RESTORE();
        m10_seed(b1d, 1u, 9u, 5u, 0x40u, 0x100u, 0u);
        fight_mode_10_step();
        CHECK_EQ_INT((int)DSB(DS_00104B1F), 0x42);
        CHECK_EQ_INT((int)DSB(DS_00108170), 0);
        CHECK_EQ_INT((int)DSB(DS_00108170 + 1u), 1);
        CHECK_EQ_INT((int)DSB(DS_00108174), 0x5A);
        CHECK_EQ_INT((int)DSW(DS_0010816C), 9);
    }

    /* (f) Sub-state 0 (the 0x43B24/0x44798 gap) and 2 run nothing in either
     * copy: with credits and both presses available, no side joins, the
     * countdown keeps 1 and the sub-state is not copied. */
    {
        static const u32 subs[] = { 0u, 2u };
        for (u32 i = 0; i < 2u; i++) {
            for (u32 b1d = 0u; b1d <= 3u; b1d += 3u) {
                M10_RESTORE();
                m10_seed(b1d, subs[i], 1u, 5u, 0x40u, 0x01000100u, 0u);
                fight_mode_10_step();
                CHECK_EQ_INT((int)DSB(DS_00108174), (int)subs[i]);
                CHECK_EQ_INT((int)DSW(DS_0010816C), 1);
                CHECK_EQ_INT((int)DSB(DS_00104B1F), 0x40);
                CHECK_EQ_INT((int)DSB(DS_00108170), 0);
            }
        }
    }

    /* (g) game_frame routes the word 0x10 (dword 0xBEEF0010) to 0x438B4:
     * the countdown runs 2 -> 1 and the mode is kept. */
    M10_RESTORE();
    m10_seed(0u, 1u, 2u, 0u, 0x40u, 0u, 0u);
    ms_seed(0xBEEF0010u);
    game_frame();
    CHECK_EQ_INT((int)DSW(DS_0010816C), 1);
    CHECK_EQ_INT((int)DSB(DS_00108174), 1);
    CHECK_EQ_INT((int)DSD(DS_00104B00), (int)0xBEEF0010u);
#undef M10_RESTORE

    tf_put(s_data, 0x80000u, sizeof s_data);
    tf_put(s_rec, rec_pool, sizeof s_rec);
    tf_put(s_pset, pset_pool, sizeof s_pset);
    tf_put(s_bufa, bufa, sizeof s_bufa);
    tf_put(s_bufb, bufb, sizeof s_bufb);
    tf_put(s_res, res_tab, res_len);
    memcpy(gfx_aperture(), s_ap, sizeof s_ap);
    memcpy(gfx_dac, s_dac, sizeof s_dac);
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
    DSW(DS_00107D18) = 2;                       /* 0x357F5 0x38D24 counts it */
    DSW(DS_00107D18 + 2u) = 5;                  /* side 1's is not touched */

    fight_hud_pass(0u);
    CHECK_EQ_INT((int)DSW(DS_00107D18), 1);
    CHECK_EQ_INT((int)DSW(DS_00107D18 + 2u), 5);
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

/* ---- §41-A: the 0x3A79C family's pose handler 0x3A6D4 ------------------- */

/* The phase-1 seed for 0x3A6D4: pose_handler_seed with character 3 (the
 * raptor, 0xC9008[3] = 0xD26A6, whose first word is the sprite 0x1886), the
 * +0x90 gate open (0: (u8)(0 - 1) > 3), the 0x3A79C setter's B/A words
 * (B = 0x107D04 + side*2, A = 0x107D08 + side*2) zeroed, and 0x3A43C's pair
 * (0x107D10/0x107D14) armed as a trap: B = 3, A = 0x4321 there would snap if
 * the handler read them. The hit_anchor_x path is made inert as in
 * check_pose_handler: DS_001077A8[side] = 0 is 0x18540's early-out and
 * DS_00100AF0[side] = slot+0x20 skips 0x18350, so the snap writes A[side] -
 * DS_00100AB0[side*8] = A - 0x1000. */
static void pose_3a6d4_seed(u32 s0, u32 s1, u32 r0, u32 r1)
{
    pose_handler_seed(s0, s1, r0, r1);
    DSB(s0 + 0x7Au) = 3;                     /* char 3: 0xC9008[3] */
    DSB(s1 + 0x7Au) = 3;
    DSB(s1 + 0x58u) = 1;
    DSD(r1 + 8u) = 0xDEADBEEFu;
    DSD(r1 + 0x24u) = 0xDEADBEEFu;
    DSD(r1 + 0x18u) = 0x5678;
    DSW(r1 + 0x56u) = 1;                     /* side 1's pset index */
    DSD(s0 + 0x2Cu) = 0x1234;
    DSD(s1 + 0x2Cu) = 0x1234;
    DSW(0x00107D04u) = 0;                    /* B[0] = 0: the gate closed */
    DSW(0x00107D06u) = 0;                    /* B[1] */
    DSW(0x00107D08u) = 0;                    /* A[0] */
    DSW(0x00107D0Au) = 0;                    /* A[1] */
    DSW(0x00107D10u) = 3;                    /* 0x3A43C's B[0]: the trap */
    DSW(0x00107D14u) = 0x4321;               /* 0x3A43C's A[0] */
    DSD(DS_001077A8) = 0;
    DSD(DS_001077A8 + 4u) = 0;
    DSD(DS_00100AF0) = DSD(s0 + 0x20u);
    DSD(DS_00100AF0 + 4u) = DSD(s1 + 0x20u);
    DSD(DS_00100AB0) = 0x1000;
    DSD(DS_00100AB0 + 8u) = 0x2000;
}

/* §41-A: 0x3A6D4's phases, the char-indexed 0xC9008 stream, the +0x90 = 3
 * store, the 0x107D04/0x107D08 snap gate and its 0x3A6C4 table arm, the
 * side-1 mirror and the 0x3531C case-10 wiring through the registration. The
 * test_fight snapshots cover the slots, 0x107D00..0x107EFF and 0x100A70..;
 * the rest this check (and pose_chain_setup) writes is restored here. */
static void check_pose_handler_3a6d4(void)
{
    u32 s0 = DS_001077B0;
    u32 s1 = DS_001077B0 + 0x94u;
    u32 r0 = FIGHT_RECS;
    u32 r1 = FIGHT_RECS + 0x100u;
    u8 sv_slots[0x160];
    u8 sv_d00[0x30];
    u8 sv_ab0[0x50];
    u16 sv_78f6 = DSW(DS_001078F6);
    u32 sv_4ec = DSD(DS_001014EC);
    u16 sv_4b00 = DSW(DS_00104B00);
    u8 sv_4b1d = DSB(DS_00104B1D);
    u8 sv_5b38 = DSB(DS_00105B38);
    u8 sv_5b36 = DSB(DS_00105B36);
    u8 sv_5b3a = DSB(DS_00105B3A);
    u32 sv_4abc = DSD(DS_00104ABC);
    u16 sv_w0 = DSW(0x000A6728u);
    u32 sv_d8 = DSD(0x000A3528u + 8u);
    u8 sv_e11a = DSB(0x000DE11Au);
    tf_snap(sv_slots, 0x001077A0u, 0x160u);
    tf_snap(sv_d00, 0x00107D00u, 0x30u);
    tf_snap(sv_ab0, 0x00100AB0u, 0x50u);

    /* 0x3A6F1/0x3A6FD: phase 0 arms +0x58 and touches nothing else. */
    pose_3a6d4_seed(s0, s1, r0, r1);
    DSB(s0 + 0x58u) = 0;
    DSB(s0 + 0x90u) = 0x55;
    fighter_pose_3a6d4(s0, 0u);
    CHECK_EQ_INT((int)DSB(s0 + 0x58u), 1);
    CHECK_EQ_INT((int)DSB(s0 + 0x90u), 0x55);
    CHECK_EQ_INT((int)DSD(r0 + 8u), (int)0xDEADBEEFu);

    /* 0x3A6EB/0x3A6ED: +0x58 above 1 returns before anything. */
    pose_3a6d4_seed(s0, s1, r0, r1);
    DSB(s0 + 0x58u) = 2;
    DSB(s0 + 0x90u) = 0x55;
    fighter_pose_3a6d4(s0, 0u);
    CHECK_EQ_INT((int)DSB(s0 + 0x58u), 2);
    CHECK_EQ_INT((int)DSD(r0 + 8u), (int)0xDEADBEEFu);
    CHECK_EQ_INT((int)DSB(s0 + 0x90u), 0x55);
    CHECK_EQ_INT((int)DSD(r0 + 0x1Cu), (int)0xDEADBEEFu);

    /* Phase 1 with B[0] = 0: the 0xC9008[3] stream at 3.0 (0x3A709/0x3A716),
     * the re-anchor (0x3A733: x kept, y = 0), +0x58 = 2, +0x90 = 3 and no
     * snap. The side-1 record is untouched. */
    pose_3a6d4_seed(s0, s1, r0, r1);
    fighter_pose_3a6d4(s0, 0u);
    CHECK_EQ_INT((int)DSD(r0 + 8u), 0x000D26A6);    /* 0xC9008[3] */
    CHECK_EQ_INT((int)DSD(r0 + 0x20u), 0x40400000); /* 3.0f */
    CHECK_EQ_INT((int)DSD(r0 + 0x24u), 0x40400000);
    CHECK_EQ_INT((int)DSB(r0 + 0x52u), 0);          /* actors_anim_begin reset */
    CHECK_EQ_INT((int)DSW(FIGHT_ACTORS), 0x1886);   /* the stream's first id */
    CHECK_EQ_INT((int)DSB(s0 + 0x58u), 2);
    CHECK_EQ_INT((int)DSB(s0 + 0x90u), 3);          /* 0x3A78E, not 0x3A43C's 1 */
    CHECK_EQ_INT((int)DSD(r0 + 0x1Cu), 0);          /* hit_anchor_set */
    CHECK_EQ_INT((int)DSD(r0 + 0x18u), 0x5678);     /* B[0] = 0: no snap */
    CHECK_EQ_INT((int)DSD(r1 + 8u), (int)0xDEADBEEFu);
    CHECK_EQ_INT((int)DSD(r1 + 0x1Cu), (int)0xDEADBEEFu);

    /* Character 0 reads 0xC9008[0] = 0xE7358 (first word 0x11D1). */
    pose_3a6d4_seed(s0, s1, r0, r1);
    DSB(s0 + 0x7Au) = 0;
    fighter_pose_3a6d4(s0, 0u);
    CHECK_EQ_INT((int)DSD(r0 + 8u), 0x000E7358);
    CHECK_EQ_INT((int)DSW(FIGHT_ACTORS), 0x11D1);

    /* B[0] = 3, A[0] = 0x4321, +0x90 = 0: the snap (0x3A780/0x3A785). */
    pose_3a6d4_seed(s0, s1, r0, r1);
    DSW(0x00107D04u) = 3;
    DSW(0x00107D08u) = 0x4321;
    fighter_pose_3a6d4(s0, 0u);
    CHECK_EQ_INT((int)DSD(s0 + 0x2Cu), 0x4321);
    CHECK_EQ_INT((int)DSD(r0 + 0x18u), 0x4321 - 0x1000);
    CHECK_EQ_INT((int)DSB(s0 + 0x90u), 3);

    /* A negative A word is sign-extended (0x3A757 SAR 16). */
    pose_3a6d4_seed(s0, s1, r0, r1);
    DSW(0x00107D04u) = 3;
    DSW(0x00107D08u) = 0x8001;
    fighter_pose_3a6d4(s0, 0u);
    CHECK_EQ_INT((int)DSD(s0 + 0x2Cu), (int)0xFFFF8001u);

    /* The 0x3A6C4 table: +0x90 = 1 and 4 skip the snap; 5 is past it. */
    pose_3a6d4_seed(s0, s1, r0, r1);
    DSB(s0 + 0x90u) = 1;
    DSW(0x00107D04u) = 3;
    DSW(0x00107D08u) = 0x4321;
    fighter_pose_3a6d4(s0, 0u);
    CHECK_EQ_INT((int)DSD(r0 + 0x18u), 0x5678);
    CHECK_EQ_INT((int)DSB(s0 + 0x90u), 3);

    pose_3a6d4_seed(s0, s1, r0, r1);
    DSB(s0 + 0x90u) = 4;
    DSW(0x00107D04u) = 3;
    DSW(0x00107D08u) = 0x4321;
    fighter_pose_3a6d4(s0, 0u);
    CHECK_EQ_INT((int)DSD(r0 + 0x18u), 0x5678);

    pose_3a6d4_seed(s0, s1, r0, r1);
    DSB(s0 + 0x90u) = 5;
    DSW(0x00107D04u) = 3;
    DSW(0x00107D08u) = 0x4321;
    fighter_pose_3a6d4(s0, 0u);
    CHECK_EQ_INT((int)DSD(r0 + 0x18u), 0x4321 - 0x1000);

    /* B[0] = 5 closes the snap (0x3A75E). */
    pose_3a6d4_seed(s0, s1, r0, r1);
    DSW(0x00107D04u) = 5;
    DSW(0x00107D08u) = 0x4321;
    fighter_pose_3a6d4(s0, 0u);
    CHECK_EQ_INT((int)DSD(r0 + 0x18u), 0x5678);

    /* The B/A words are the self side's: B[1] = 3, A[1] = 0x4321 leave side 0's
     * gate closed (and the seed's 0x3A43C trap stays unread). */
    pose_3a6d4_seed(s0, s1, r0, r1);
    DSW(0x00107D06u) = 3;
    DSW(0x00107D0Au) = 0x4321;
    fighter_pose_3a6d4(s0, 0u);
    CHECK_EQ_INT((int)DSD(r0 + 0x18u), 0x5678);

    /* The side-1 mirror: EBX = 1 drives slot 1 and record 1 from B[1]/A[1]
     * (0x3A744/0x3A74B index by side*2) and DS_00100AB0[8]. */
    pose_3a6d4_seed(s0, s1, r0, r1);
    DSW(0x00107D06u) = 3;
    DSW(0x00107D0Au) = 0x6543;
    fighter_pose_3a6d4(s1, 1u);
    CHECK_EQ_INT((int)DSD(r1 + 8u), 0x000D26A6);
    CHECK_EQ_INT((int)DSW(FIGHT_ACTORS + 0x20u), 0x1886);
    CHECK_EQ_INT((int)DSB(s1 + 0x58u), 2);
    CHECK_EQ_INT((int)DSB(s1 + 0x90u), 3);
    CHECK_EQ_INT((int)DSD(s1 + 0x2Cu), 0x6543);
    CHECK_EQ_INT((int)DSD(r1 + 0x18u), 0x6543 - 0x2000);
    CHECK_EQ_INT((int)DSB(s0 + 0x58u), 1);
    CHECK_EQ_INT((int)DSD(r0 + 8u), (int)0xDEADBEEFu);

    /* The wiring: actors_init (check_anim_hold_scaler, before this check)
     * registered 0x3A6D4, and 0x3531C case 10 resolves slot+0x10 and calls it
     * with (EAX = slot, EBX = side). */
    CHECK(fn_resolve(0x3A6D4u) == (void (*)(void))fighter_pose_3a6d4,
          "actors_init registered 0x3A6D4 as fighter_pose_3a6d4");
    pose_3a6d4_seed(s0, s1, r0, r1);
    DSB(s0 + 0x53u) = 0x0A;
    DSD(s0 + 0x10u) = 0x0003A6D4u;
    DSW(DS_001078F6) = 0;                    /* the +0xEC voice tick is inert */
    DSD(DS_001077A8) = s0;                   /* 0x3531C's slot read */
    fighter_state_3531c(0u);
    CHECK_EQ_INT((int)DSD(r0 + 8u), 0x000D26A6);
    CHECK_EQ_INT((int)DSB(s0 + 0x58u), 2);
    CHECK_EQ_INT((int)DSB(s0 + 0x90u), 3);

    tf_put(sv_slots, 0x001077A0u, 0x160u);
    tf_put(sv_d00, 0x00107D00u, 0x30u);
    tf_put(sv_ab0, 0x00100AB0u, 0x50u);
    DSW(DS_001078F6) = sv_78f6;
    DSD(DS_001014EC) = sv_4ec;
    DSW(DS_00104B00) = sv_4b00;
    DSB(DS_00104B1D) = sv_4b1d;
    DSB(DS_00105B38) = sv_5b38;
    DSB(DS_00105B36) = sv_5b36;
    DSB(DS_00105B3A) = sv_5b3a;
    DSD(DS_00104ABC) = sv_4abc;
    DSW(0x000A6728u) = sv_w0;
    DSD(0x000A3528u + 8u) = sv_d8;
    DSB(0x000DE11Au) = sv_e11a;
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
    CHECK(fn_resolve(0x3C0A4u) != NULL, "0x3C0A4 is registered");
    CHECK(fn_resolve(0x3C0A4u) != (void (*)(void))fighter_3c0a4,
          "0x3C0A4 is registered through the (slot, rec, side) wrapper");
    CHECK(fn_resolve(0x3BF70u) != NULL, "0x3BF70 is registered");
    CHECK(fn_resolve(0x3BF70u) != (void (*)(void))fighter_3bf70,
          "0x3BF70 is registered through the (slot, rec, side) wrapper");
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

/* tb_seed plus the fields 0x3BF70/0x3C0A4 write, each a sentinel that differs
 * from its post-condition: both records' +0x34/+0x43/+0x42, the slots'
 * +0x40/+0x4E/+0x52 (0 takes 0x3C4CC's plain 0x2BC30 arm) and +0x7A (chars
 * 1 and 2), the DS_00107D40 rows, the DS_001078F8 bytes, unflipped psets, and
 * crafted literal-id streams (id 0x1100 + char) in the 0xC8B30 table. */
static void ra_seed(u32 s0, u32 s1, u32 r0, u32 r1, u32 st)
{
    u32 s[2], r[2], i;
    tb_seed(s0, s1, r0, r1);
    s[0] = s0; s[1] = s1; r[0] = r0; r[1] = r1;
    for (i = 0; i < 2u; i++) {
        DSW(r[i] + 0x34u) = (u16)(0xAAAAu + i * 0x1111u);
        DSB(r[i] + 0x43u) = (u8)(0xA3u + i);
        DSB(r[i] + 0x42u) = (u8)(0xA5u + i);
        DSB(s[i] + 0x40u) = (u8)(0x05u + i);
        DSW(s[i] + 0x4Eu) = (u16)(0x1234u + i);
        DSB(s[i] + 0x52u) = 0u;
        DSB(s[i] + 0x7Au) = (u8)(1u + i);
    }
    DSD(DS_00107D40) = 0xDEADBEEFu;
    DSD(DS_00107D40 + 4u) = 0xCAFEBABEu;
    DSB(DS_001078F8) = 0x5Au;
    DSB(DS_001078F8 + 1u) = 0x5Bu;
    for (i = 0; i < 7u; i++) {
        DSW(st + i * 0x10u) = (u16)(0x1100u + i);
        DSD(0x000C8B30u + i * 4u) = st + i * 0x10u;
    }
}

/* 0x3BF70 and 0x3C0A4, 0x34E2C's reaction callbacks 0x3F and 0x3E (the T-rex's
 * records 0xA3A14 and 0xA3A00 read `70 bf 03 00 00 00 00 00` and `a4 c0 03 00
 * 00 00 00 00`: no stream). EAX = slot, EDX = rec, EBX = side. 0x3BF70 tests
 * the side's slot +0x40 bit 7 (ctx[2], 0x3BF86), clears the side's record's
 * +0x34/+0x43/+0x42, sets the side's slot +0x5F = 0xFF and DS_00107D40 +
 * side*4 = 0xBEFA0 + 6 * the side's char, begins the EDX record on
 * 0xC8B30[the EAX slot's char] at hold 2.0 through 0x3C4CC (which reads +0x52
 * before 0x3C009 writes 3), then writes the EAX slot's 3/4/2, +0x40 bit 7 and
 * +0x4E (0xFFFF when 0x1A570(side) is non-zero, else 1) and DS_001078F8 +
 * side = 1. 0x3C0A4 turns that +0x4E the other way. The demo's f = 850 call
 * is 0x3C0A4 for side 0. */
static void check_reaction_attack(void)
{
    u32 s0 = DS_001077B0, s1 = DS_001077B0 + 0x94u;
    u32 r0 = FIGHT_RECS, r1 = FIGHT_RECS + 0x100u;
    u32 pset1 = FIGHT_ACTORS + 0x20u, pset2 = FIGHT_ACTORS + 0x40u;
    u32 st = FIGHT_RECS + 0x3A00u;           /* the crafted streams, 0x10 apart */
    u32 sv_14ec = DSD(DS_001014EC);
    u16 sv_ac = DSW(0x001080ACu), sv_ae = DSW(0x001080AEu);
    u8 sv_b00[4], sv_d40[8], sv_f8[2], sv_tab[28], sv_a8[2];
    tf_snap(sv_b00, DS_00104B00, 4u);
    tf_snap(sv_d40, DS_00107D40, 8u);
    tf_snap(sv_f8, DS_001078F8, 2u);
    tf_snap(sv_tab, 0x000C8B30u, 28u);
    tf_snap(sv_a8, DS_001088A8, 2u);

    /* A: 0x3BF70 for side 0, unflipped (0x1A570 = 1): +0x4E = 0xFFFF. */
    ra_seed(s0, s1, r0, r1, st);
    CHECK_EQ_INT(fighter_3bf70(s0, r0, 0u), 1);
    CHECK_EQ_INT((int)DSW(r0 + 0x34u), 0);
    CHECK_EQ_INT((int)DSB(r0 + 0x43u), 0);
    CHECK_EQ_INT((int)DSB(r0 + 0x42u), 0);
    CHECK_EQ_INT((int)DSB(s0 + 0x5Fu), 0xFF);
    CHECK_EQ_INT((int)DSD(DS_00107D40), 0x000BEFA6);
    CHECK_EQ_INT((int)DSD(DS_00107D40 + 4u), (int)0xCAFEBABEu);
    CHECK_EQ_INT((int)DSD(r0 + 8u), (int)(st + 0x10u));
    CHECK_EQ_INT((int)DSD(r0 + 0x24u), 0x40000000);
    CHECK_EQ_INT((int)(DSW(pset1) & 0x7FFFu), 0x1101);
    CHECK_EQ_INT((int)DSD(r0 + 0x1Cu), 0x5555);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 3);
    CHECK_EQ_INT((int)DSB(s0 + 0x53u), 4);
    CHECK_EQ_INT((int)DSB(s0 + 0x54u), 2);
    CHECK_EQ_INT((int)DSB(s0 + 0x40u), 0x85);
    CHECK_EQ_INT((int)DSW(s0 + 0x4Eu), 0xFFFF);
    CHECK_EQ_INT((int)DSB(DS_001078F8), 1);
    CHECK_EQ_INT((int)DSB(DS_001078F8 + 1u), 0x5B);
    CHECK_EQ_INT((int)DSW(r1 + 0x34u), 0xBBBB);
    CHECK_EQ_INT((int)DSB(s1 + 0x52u), 0);
    CHECK_EQ_INT((int)DSW(s1 + 0x4Eu), 0x1235);

    /* A2: flipped (0x1A570 = 0): +0x4E = 1. The record's +0x29 bit 6 makes
     * the begin write the pset's bit 15, so 0x1A570 must run after 0x3C4CC
     * (0x3C028): the seeded pset is unflipped. */
    ra_seed(s0, s1, r0, r1, st);
    DSB(r0 + 0x29u) = 0x40u;
    CHECK_EQ_INT(fighter_3bf70(s0, r0, 0u), 1);
    CHECK_EQ_INT((int)DSW(s0 + 0x4Eu), 1);

    /* B: 0x3C0A4 turns +0x4E the other way: 1 unflipped, 0xFFFF flipped (the
     * demo's f = 850 call); the rest is 0x3BF70's. */
    ra_seed(s0, s1, r0, r1, st);
    CHECK_EQ_INT(fighter_3c0a4(s0, r0, 0u), 1);
    CHECK_EQ_INT((int)DSW(s0 + 0x4Eu), 1);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 3);
    CHECK_EQ_INT((int)DSB(s0 + 0x5Fu), 0xFF);
    CHECK_EQ_INT((int)DSD(r0 + 8u), (int)(st + 0x10u));
    CHECK_EQ_INT((int)DSD(DS_00107D40), 0x000BEFA6);
    CHECK_EQ_INT((int)DSB(DS_001078F8), 1);
    ra_seed(s0, s1, r0, r1, st);
    DSB(r0 + 0x29u) = 0x40u;
    CHECK_EQ_INT(fighter_3c0a4(s0, r0, 0u), 1);
    CHECK_EQ_INT((int)DSW(s0 + 0x4Eu), 0xFFFF);

    /* C: the side's slot +0x40 bit 7 rejects; both return 0 and write
     * nothing (0x3C0A4 leaves +0x4E). */
    ra_seed(s0, s1, r0, r1, st);
    DSB(s0 + 0x40u) = 0x85u;
    CHECK_EQ_INT(fighter_3bf70(s0, r0, 0u), 0);
    CHECK_EQ_INT(fighter_3c0a4(s0, r0, 0u), 0);
    CHECK_EQ_INT((int)DSW(r0 + 0x34u), 0xAAAA);
    CHECK_EQ_INT((int)DSB(r0 + 0x43u), 0xA3);
    CHECK_EQ_INT((int)DSB(r0 + 0x42u), 0xA5);
    CHECK_EQ_INT((int)DSB(s0 + 0x5Fu), 0x3C);
    CHECK_EQ_INT((int)DSD(DS_00107D40), (int)0xDEADBEEFu);
    CHECK_EQ_INT((int)DSD(r0 + 8u), 0x00ABCDEF);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 0);
    CHECK_EQ_INT((int)DSB(s0 + 0x53u), 0x66);
    CHECK_EQ_INT((int)DSB(s0 + 0x40u), 0x85);
    CHECK_EQ_INT((int)DSW(s0 + 0x4Eu), 0x1234);
    CHECK_EQ_INT((int)DSB(DS_001078F8), 0x5A);

    /* D: EAX = slot 1, EDX = record 1, EBX = side 0. The gate, the cleared
     * record, +0x5F, the row (char 1), DS_001078F8 and 0x1A570 are side 0's;
     * the stream (char 2), the state, +0x40 and +0x4E are slot 1's, and
     * record 1 begins. Record 1's +0x51 = 1 sends 0x3C4CC to slot 1's +0x52
     * = 9: the 0x3C480 arm zeroes +0x1C. Slot 1's own +0x40 bit 7 does not
     * gate, and record 1's flip (+0x29 bit 6) does not reach +0x4E. */
    ra_seed(s0, s1, r0, r1, st);
    DSB(s1 + 0x40u) = 0x86u;
    DSB(s1 + 0x52u) = 9u;
    DSB(r1 + 0x29u) = 0x40u;
    CHECK_EQ_INT(fighter_3bf70(s1, r1, 0u), 1);
    CHECK_EQ_INT((int)DSW(r0 + 0x34u), 0);
    CHECK_EQ_INT((int)DSB(r0 + 0x43u), 0);
    CHECK_EQ_INT((int)DSB(r0 + 0x42u), 0);
    CHECK_EQ_INT((int)DSW(r1 + 0x34u), 0xBBBB);
    CHECK_EQ_INT((int)DSB(r1 + 0x43u), 0xA4);
    CHECK_EQ_INT((int)DSB(s0 + 0x5Fu), 0xFF);
    CHECK_EQ_INT((int)DSB(s1 + 0x5Fu), 0x3C);
    CHECK_EQ_INT((int)DSD(DS_00107D40), 0x000BEFA6);
    CHECK_EQ_INT((int)DSD(DS_00107D40 + 4u), (int)0xCAFEBABEu);
    CHECK_EQ_INT((int)DSD(r1 + 8u), (int)(st + 0x20u));
    CHECK_EQ_INT((int)(DSW(pset2) & 0x7FFFu), 0x1102);
    CHECK_EQ_INT((int)DSD(r1 + 0x1Cu), 0);
    CHECK_EQ_INT((int)DSD(r0 + 8u), 0x00ABCDEF);
    CHECK_EQ_INT((int)DSB(s1 + 0x52u), 3);
    CHECK_EQ_INT((int)DSB(s1 + 0x53u), 4);
    CHECK_EQ_INT((int)DSB(s1 + 0x54u), 2);
    CHECK_EQ_INT((int)DSB(s1 + 0x40u), 0x86);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 0);
    CHECK_EQ_INT((int)DSB(s0 + 0x40u), 0x05);
    CHECK_EQ_INT((int)DSW(s1 + 0x4Eu), 0xFFFF);
    CHECK_EQ_INT((int)DSW(s0 + 0x4Eu), 0x1234);
    CHECK_EQ_INT((int)DSB(DS_001078F8), 1);
    CHECK_EQ_INT((int)DSB(DS_001078F8 + 1u), 0x5B);
    /* D2: the same call with side 0's +0x40 bit 7 set rejects. */
    ra_seed(s0, s1, r0, r1, st);
    DSB(s0 + 0x40u) = 0x85u;
    CHECK_EQ_INT(fighter_3bf70(s1, r1, 0u), 0);
    CHECK_EQ_INT((int)DSB(s1 + 0x52u), 0);
    CHECK_EQ_INT((int)DSD(r1 + 8u), 0x00ABCDEF);

    /* E: side 1 on its own slot: the row (char 2, 0xBEFAC) goes to
     * DS_00107D44 and the flag to DS_001078F9; 0x3C0A4's +0x4E follows side
     * 1's pset (flipped by the record's +0x29 bit 6: 0xFFFF). */
    ra_seed(s0, s1, r0, r1, st);
    DSB(r1 + 0x29u) = 0x40u;
    CHECK_EQ_INT(fighter_3c0a4(s1, r1, 1u), 1);
    CHECK_EQ_INT((int)DSD(DS_00107D40 + 4u), 0x000BEFAC);
    CHECK_EQ_INT((int)DSD(DS_00107D40), (int)0xDEADBEEFu);
    CHECK_EQ_INT((int)DSB(DS_001078F8 + 1u), 1);
    CHECK_EQ_INT((int)DSB(DS_001078F8), 0x5A);
    CHECK_EQ_INT((int)DSW(r1 + 0x34u), 0);
    CHECK_EQ_INT((int)DSW(r0 + 0x34u), 0xAAAA);
    CHECK_EQ_INT((int)DSB(s1 + 0x5Fu), 0xFF);
    CHECK_EQ_INT((int)DSW(s1 + 0x4Eu), 0xFFFF);
    CHECK_EQ_INT((int)DSD(r1 + 8u), (int)(st + 0x20u));

    /* F: through 0x34E2C (0x35045) for the T-rex (char 0): reaction 0x3E
     * reaches 0x3C0A4 (unflipped: +0x4E = 1) and 0x3F reaches 0x3BF70
     * (+0x4E = 0xFFFF). An unregistered target leaves the state and +0x4E. */
    ra_seed(s0, s1, r0, r1, st);
    DSB(s0 + 0x7Au) = 0u;
    hit_reaction_apply(0u, 0x3Eu);
    CHECK_EQ_INT((int)DSW(s0 + 0x4Eu), 1);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 3);
    CHECK_EQ_INT((int)DSB(s0 + 0x53u), 4);
    CHECK_EQ_INT((int)DSB(s0 + 0x54u), 2);
    CHECK_EQ_INT((int)DSB(s0 + 0x5Fu), 0xFF);
    CHECK_EQ_INT((int)DSD(r0 + 8u), (int)st);
    CHECK_EQ_INT((int)DSD(DS_00107D40), 0x000BEFA0);
    ra_seed(s0, s1, r0, r1, st);
    DSB(s0 + 0x7Au) = 0u;
    hit_reaction_apply(0u, 0x3Fu);
    CHECK_EQ_INT((int)DSW(s0 + 0x4Eu), 0xFFFF);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 3);
    CHECK_EQ_INT((int)DSD(r0 + 8u), (int)st);

    tf_put(sv_tab, 0x000C8B30u, 28u);
    tf_put(sv_f8, DS_001078F8, 2u);
    tf_put(sv_d40, DS_00107D40, 8u);
    tf_put(sv_a8, DS_001088A8, 2u);
    DSD(DS_001014EC) = sv_14ec;
    DSW(0x001080ACu) = sv_ac;
    DSW(0x001080AEu) = sv_ae;
    tf_put(sv_b00, DS_00104B00, 4u);
}

/* ---- roar-timing Task 24: the T-rex's reaction-0x2A callback (record §34) - */

/* tb_seed plus the fields 0x3E3A8/0x3E328 write, each a sentinel that differs
 * from its post-condition: the slots' +0x41, +0x52 = 0 (0x3C4CC's plain
 * 0x2BC30 arm), +0x57, +0x7A (chars 1 and 2), +0x86 and +0x8A, and crafted
 * literal-id streams (id 0x1200 + char) in the 0xC8950 table. */
static void tg_seed(u32 s0, u32 s1, u32 r0, u32 r1, u32 st)
{
    u32 s[2], i;
    tb_seed(s0, s1, r0, r1);
    s[0] = s0; s[1] = s1;
    for (i = 0; i < 2u; i++) {
        DSB(s[i] + 0x41u) = (u8)(0x05u + i);
        DSB(s[i] + 0x52u) = 0u;
        DSB(s[i] + 0x57u) = (u8)(0x99u + i);
        DSB(s[i] + 0x7Au) = (u8)(1u + i);
        DSD(s[i] + 0x86u) = 0x00630001u + i;
        DSB(s[i] + 0x8Au) = (u8)(0x77u + i);
    }
    for (i = 0; i < 7u; i++) {
        DSW(st + i * 0x10u) = (u16)(0x1200u + i);
        DSD(0x000C8950u + i * 4u) = st + i * 0x10u;
    }
}

/* 0x3E3A8 (*(u32*)0xA3870 reads `a8 e3 03 00 00 00 00 00`: the T-rex's
 * reaction 0x2A, no stream) builds 0x33950's context from EBX = side (EAX and
 * EDX are overwritten at 0x3E3AB/0x3E3AD), begins ctx[4] on 0xC8950[ctx[2]'s
 * char] at hold 2.0 through 0x3C4CC, then writes ctx[2]'s 7/9/0, +0x0C =
 * 0x3E328, +0x18 = 0x3E1D0, +0x1C = 0x3E244, +0x57 = 0 and +0x41 bit 7. Its
 * +0x0C callback 0x3E328 (again EBX-side) steps +0x57: 0 -> 1 once +0x86 >> 16
 * (signed) exceeds 3; 1 -> 2 with +0x8A = 0, the 0xE84B6 stream at hold 4.0,
 * 0x3E0F0's child (+0x53 = 1) and +0x52 = 9. 0x3E0F0 writes 0x29C08(side,
 * char) = DSD(DSD(0xA8A98 + char*4) + DSB(0x105B34 + side)*4) into the side's
 * 0xC760C descriptor's +0x10 before spawning it. The demo's f = 962 call is
 * 0x3E3A8 for side 0 with +0x52 = 9; f = 963's 0x3E328 sees +0x86 >> 16 = 1. */
static void check_trex_grab(void)
{
    u32 s0 = DS_001077B0, s1 = DS_001077B0 + 0x94u;
    u32 r0 = FIGHT_RECS, r1 = FIGHT_RECS + 0x100u;
    u32 pset1 = FIGHT_ACTORS + 0x20u, pset2 = FIGHT_ACTORS + 0x40u;
    u32 st = FIGHT_RECS + 0x3A00u;           /* the crafted streams, 0x10 apart */
    u32 d0 = 0x000BB3F8u + 0x10u, d1 = 0x000BB40Cu + 0x10u;  /* 0xC760C[0/1] +0x10 */
    u32 sv_14ec = DSD(DS_001014EC);
    u32 sv_d0 = DSD(d0), sv_d1 = DSD(d1);
    u16 sv_ac = DSW(0x001080ACu), sv_ae = DSW(0x001080AEu);
    u16 sv_78f6 = DSW(DS_001078F6);
    u8 sv_b00[4], sv_tab[28], sv_a8[2], sv_b34[2], sv_f8[2];
    tf_snap(sv_b00, DS_00104B00, 4u);
    tf_snap(sv_tab, 0x000C8950u, 28u);
    tf_snap(sv_a8, DS_001088A8, 2u);
    tf_snap(sv_b34, DS_00105B34, 2u);
    tf_snap(sv_f8, DS_001078F8, 2u);

    CHECK(fn_resolve(0x3E3A8u) == (void (*)(void))fighter_3e3a8,
          "0x3E3A8 is registered as fighter_3e3a8");
    CHECK(fn_resolve(0x3E328u) == (void (*)(void))fighter_3e328,
          "0x3E328 is registered as fighter_3e328");
    CHECK_EQ_INT((int)DSD(0x000C760Cu), (int)0x000BB3F8u);
    CHECK_EQ_INT((int)DSD(0x000C7610u), (int)0x000BB40Cu);

    /* A: through 0x34E2C (0x35045) for the T-rex (char 0): reaction 0x2A
     * reaches 0x3E3A8. No stream, so 0x34E2C leaves +0x52..+0x54 and sets
     * +0x5F = 0x2A (0x35042) before the call. */
    tg_seed(s0, s1, r0, r1, st);
    DSB(s0 + 0x7Au) = 0u;
    hit_reaction_apply(0u, 0x2Au);
    CHECK_EQ_INT((int)DSD(r0 + 8u), (int)st);
    CHECK_EQ_INT((int)DSD(r0 + 0x24u), 0x40000000);
    CHECK_EQ_INT((int)(DSW(pset1) & 0x7FFFu), 0x1200);
    CHECK_EQ_INT((int)DSB(s0 + 0x53u), 7);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 9);
    CHECK_EQ_INT((int)DSB(s0 + 0x54u), 0);
    CHECK_EQ_INT((int)DSD(s0 + 0x0Cu), 0x0003E328);
    CHECK_EQ_INT((int)DSD(s0 + 0x18u), 0x0003E1D0);
    CHECK_EQ_INT((int)DSD(s0 + 0x1Cu), 0x0003E244);
    CHECK_EQ_INT((int)DSB(s0 + 0x57u), 0);
    CHECK_EQ_INT((int)DSB(s0 + 0x41u), 0x85);
    CHECK_EQ_INT((int)DSB(s0 + 0x5Fu), 0x2A);
    CHECK_EQ_INT((int)DSB(s1 + 0x53u), 0x66);
    CHECK_EQ_INT((int)DSD(r1 + 8u), 0x00ABCDEF);

    /* B: EAX = slot 1, EDX = record 1, EBX = side 0: every write is side 0's
     * (char 1's stream); slot 1 and record 1 keep their sentinels. +0x52 = 9
     * sends 0x3C4CC to 0x3C480, whose anchor zeroes rec+0x1C. */
    tg_seed(s0, s1, r0, r1, st);
    DSB(s0 + 0x52u) = 9u;
    fighter_3e3a8(s1, r1, 0u);
    CHECK_EQ_INT((int)DSD(r0 + 8u), (int)(st + 0x10u));
    CHECK_EQ_INT((int)(DSW(pset1) & 0x7FFFu), 0x1201);
    CHECK_EQ_INT((int)DSD(r0 + 0x1Cu), 0);
    CHECK_EQ_INT((int)DSB(s0 + 0x53u), 7);
    CHECK_EQ_INT((int)DSD(s0 + 0x0Cu), 0x0003E328);
    CHECK_EQ_INT((int)DSB(s0 + 0x57u), 0);
    CHECK_EQ_INT((int)DSD(r1 + 8u), 0x00ABCDEF);
    CHECK_EQ_INT((int)DSD(r1 + 0x1Cu), 0x5555);
    CHECK_EQ_INT((int)DSB(s1 + 0x52u), 0);
    CHECK_EQ_INT((int)DSB(s1 + 0x53u), 0x66);
    CHECK_EQ_INT((int)DSB(s1 + 0x54u), 0x66);
    CHECK_EQ_INT((int)DSD(s1 + 0x0Cu), 0x11111111);
    CHECK_EQ_INT((int)DSD(s1 + 0x18u), 0x22222222);
    CHECK_EQ_INT((int)DSD(s1 + 0x1Cu), 0x33333333);
    CHECK_EQ_INT((int)DSB(s1 + 0x57u), 0x9A);
    CHECK_EQ_INT((int)DSB(s1 + 0x41u), 0x06);

    /* C: side 1 on its own slot: char 2's stream on record 1; side 0 kept. */
    tg_seed(s0, s1, r0, r1, st);
    fighter_3e3a8(s1, r1, 1u);
    CHECK_EQ_INT((int)DSD(r1 + 8u), (int)(st + 0x20u));
    CHECK_EQ_INT((int)(DSW(pset2) & 0x7FFFu), 0x1202);
    CHECK_EQ_INT((int)DSD(r1 + 0x24u), 0x40000000);
    CHECK_EQ_INT((int)DSB(s1 + 0x53u), 7);
    CHECK_EQ_INT((int)DSB(s1 + 0x52u), 9);
    CHECK_EQ_INT((int)DSB(s1 + 0x54u), 0);
    CHECK_EQ_INT((int)DSD(s1 + 0x18u), 0x0003E1D0);
    CHECK_EQ_INT((int)DSD(s1 + 0x1Cu), 0x0003E244);
    CHECK_EQ_INT((int)DSB(s1 + 0x57u), 0);
    CHECK_EQ_INT((int)DSB(s1 + 0x41u), 0x86);
    CHECK_EQ_INT((int)DSD(r0 + 8u), 0x00ABCDEF);
    CHECK_EQ_INT((int)DSB(s0 + 0x53u), 0x66);

    /* D: 0x3E328's +0x57 == 0 arm reads ctx[2] (EBX = side 0, EAX = slot 1):
     * +0x86 >> 16 = 3 keeps 0, 4 moves to 1, and a negative high word
     * (signed, 0x3E353 sar) keeps 0. Nothing else is written. */
    tg_seed(s0, s1, r0, r1, st);
    DSB(s0 + 0x57u) = 0u;
    DSB(s1 + 0x57u) = 0u;
    DSD(s0 + 0x86u) = 0x0003FFFFu;
    DSD(s1 + 0x86u) = 0x00070000u;
    fighter_3e328(s1, r1, 0u);
    CHECK_EQ_INT((int)DSB(s0 + 0x57u), 0);
    DSD(s0 + 0x86u) = 0xFFFF0000u;
    fighter_3e328(s1, r1, 0u);
    CHECK_EQ_INT((int)DSB(s0 + 0x57u), 0);
    DSD(s0 + 0x86u) = 0x00040000u;
    fighter_3e328(s1, r1, 0u);
    CHECK_EQ_INT((int)DSB(s0 + 0x57u), 1);
    CHECK_EQ_INT((int)DSB(s1 + 0x57u), 0);
    CHECK_EQ_INT((int)DSB(s0 + 0x8Au), 0x77);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 0);
    CHECK_EQ_INT((int)DSD(r0 + 8u), 0x00ABCDEF);

    /* E: +0x57 = 2 and 5 return (0x3E341/0x3E3A2): nothing is written and no
     * record spawns. */
    tg_seed(s0, s1, r0, r1, st);
    actors_reset();
    {
        u32 n = tb_active(), v;
        for (v = 2u; v <= 5u; v += 3u) {
            DSB(s0 + 0x57u) = (u8)v;
            DSD(s0 + 0x86u) = 0x00400000u;
            fighter_3e328(s0, r0, 0u);
            CHECK_EQ_INT((int)DSB(s0 + 0x57u), (int)v);
            CHECK_EQ_INT((int)DSB(s0 + 0x8Au), 0x77);
            CHECK_EQ_INT((int)DSB(s0 + 0x52u), 0);
            CHECK_EQ_INT((int)DSD(r0 + 8u), 0x00ABCDEF);
            CHECK_EQ_INT((int)tb_active(), (int)n);
        }
    }

    /* F: the +0x57 == 1 arm with a pool record as side 0's record (ctx[4]).
     * Char 3, variant 1: 0x29C08 = DSD(0xA8A28 + 3*16 + 4) = 0x10A50EF0
     * (read_memory 0xA8A58: 70 0f a5 10 f0 0e a5 10 ...) goes into 0xBB3F8's
     * +0x10 before the spawn; the child (0x2AE14, a5 = rec+0x56 | 0x400) gets
     * +0x53 = 1 and +0x60 = 1 and its index goes into the record's +0x4B.
     * The record is on 0xE84B6 (literal id 0x10BA first) at hold 4.0; the
     * slot's +0x52 = 0x33 takes 0x3C480. Then +0x8A = 0, +0x52 = 9, +0x57 =
     * 2. Side 1's descriptor keeps its sentinel. */
    tg_seed(s0, s1, r0, r1, st);
    actors_reset();
    {
        u32 f0 = actor_alloc(0), e, n;
        DSW(f0 + 0x56u) = (u16)actor_index(f0);
        DSB(f0 + 0x51u) = 0u;
        DSB(f0 + 0x4Bu) = 0x77u;
        DSB(f0 + 0x60u) = 0x66u;
        DSB(f0 + 0x53u) = 0x44u;
        DSD(s0) = f0;
        DSB(s0 + 0x57u) = 1u;
        DSB(s0 + 0x52u) = 0x33u;
        DSB(s0 + 0x7Au) = 3u;
        DSB(DS_00105B34) = 1u;
        DSB(DS_00105B34 + 1u) = 3u;
        DSD(d0) = 0x5A5A5A5Au;
        DSD(d1) = 0x6B6B6B6Bu;
        n = tb_active();
        fighter_3e328(s1, r1, 0u);
        CHECK_EQ_INT((int)tb_active(), (int)(n + 1u));
        CHECK_EQ_INT((int)DSD(d0), 0x10A50EF0);
        CHECK_EQ_INT((int)DSD(d1), 0x6B6B6B6B);
        e = actor_record((u32)DSB(f0 + 0x4Bu));
        CHECK(e != 0u && e != f0 && DSB(f0 + 0x4Bu) != 0x77u,
              "0x3E0F0 stores the new child's index in rec+0x4B");
        if (e != 0u && e != f0) {
            CHECK_EQ_INT((int)DSB(e + 0x53u), 1);
            CHECK_EQ_INT((int)DSB(e + 0x60u), 1);
            CHECK_EQ_INT((int)DSB(e + 0x4Au), (int)(actor_index(f0) & 0x7Fu));
            CHECK_EQ_INT((int)(DSW(e + 0x28u) & 0x0400u), 0x0400);
        }
        CHECK_EQ_INT((int)DSB(f0 + 0x53u), 0x44);
        CHECK_EQ_INT((int)DSB(f0 + 0x60u), 0x66);
        CHECK_EQ_INT((int)DSD(f0 + 8u), 0x000E84B6);
        CHECK_EQ_INT((int)DSD(f0 + 0x24u), 0x40800000);
        CHECK_EQ_INT((int)DSB(s0 + 0x8Au), 0);
        CHECK_EQ_INT((int)DSB(s0 + 0x52u), 9);
        CHECK_EQ_INT((int)DSB(s0 + 0x57u), 2);
        CHECK_EQ_INT((int)DSB(s1 + 0x57u), 0x9A);
        CHECK_EQ_INT((int)DSB(s1 + 0x8Au), 0x78);

        /* F2: side 1 (record +0x51 = 1, char 2, variant 3): DSD(0xA8A28 +
         * 2*16 + 12) = 0x20A13DE0 into 0xBB40C's +0x10; side 0's kept. */
        {
            u32 f1 = actor_alloc(0);
            DSW(f1 + 0x56u) = (u16)actor_index(f1);
            DSB(f1 + 0x51u) = 1u;
            DSB(f1 + 0x4Bu) = 0x77u;
            DSD(s1) = f1;
            DSB(s1 + 0x57u) = 1u;
            DSD(d0) = 0x5A5A5A5Au;
            n = tb_active();
            fighter_3e328(s0, r0, 1u);
            CHECK_EQ_INT((int)tb_active(), (int)(n + 1u));
            CHECK_EQ_INT((int)DSD(d1), 0x20A13DE0);
            CHECK_EQ_INT((int)DSD(d0), 0x5A5A5A5A);
            e = actor_record((u32)DSB(f1 + 0x4Bu));
            CHECK(e != 0u && e != f1 && DSB(f1 + 0x4Bu) != 0x77u,
                  "0x3E0F0 (side 1) stores the child's index in rec+0x4B");
            if (e != 0u && e != f1) CHECK_EQ_INT((int)DSB(e + 0x53u), 1);
            CHECK_EQ_INT((int)DSB(s1 + 0x57u), 2);
            CHECK_EQ_INT((int)DSB(s1 + 0x8Au), 0);
            CHECK_EQ_INT((int)DSB(s0 + 0x57u), 2);
        }
    }

    /* G: through 0x3531C case 7 (0x35431): the slot's +0x0C = 0x3E328
     * resolves and steps +0x57 from 0 to 1 at +0x86 >> 16 = 4. */
    tg_seed(s0, s1, r0, r1, st);
    DSW(DS_001078F6) = 0;
    DSB(s0 + 0x53u) = 7u;
    DSD(s0 + 0x0Cu) = 0x0003E328u;
    DSB(s0 + 0x57u) = 0u;
    DSD(s0 + 0x86u) = 0x00040000u;
    fighter_state_3531c(0u);
    CHECK_EQ_INT((int)DSB(s0 + 0x57u), 1);

    tf_put(sv_f8, DS_001078F8, 2u);
    tf_put(sv_b34, DS_00105B34, 2u);
    tf_put(sv_a8, DS_001088A8, 2u);
    tf_put(sv_tab, 0x000C8950u, 28u);
    DSD(d0) = sv_d0;
    DSD(d1) = sv_d1;
    DSW(DS_001078F6) = sv_78f6;
    DSD(DS_001014EC) = sv_14ec;
    DSW(0x001080ACu) = sv_ac;
    DSW(0x001080AEu) = sv_ae;
    tf_put(sv_b00, DS_00104B00, 4u);
}

/* ---- roar-timing Task 27: character 3's reaction 0x1490C (record §37) ---- */

/* The FD108 dword (0x148E2) symbols.h does not name. */
#define FIGHT_FD108_T 0x000FD108u

/* The five 0x1490C-family stream heads, and a plain frame word for each so
 * that 0x2BC30 stops at once (bit 15 clear) and the pset takes that id. */
static const u32 c3_streams[5] = {
    0x000D2E86u, 0x000D2E9Au, 0x000D2EDCu, 0x000D2EBAu, 0x000D2EFCu
};

/* tg_seed for character 3 on side 0: both slots' +0x10 = 0 and +0x2C set,
 * the per-side words 0x107D2C = 0, the 0x107A80 table and the FD108/FD114/
 * FD11C/0x1078F8 bytes on sentinels, both actors' bit 15 clear. */
static void c3_seed(u32 s0, u32 s1, u32 r0, u32 r1, u32 st)
{
    u32 i;
    tg_seed(s0, s1, r0, r1, st);
    DSD(DS_001014EC) = FIGHT_ACTORS;
    DSW(DS_00104B00) = 3u;
    DSB(s0 + 0x7Au) = 3u;
    DSB(s0 + 0x63u) = 1u;
    DSB(s1 + 0x63u) = 1u;
    DSD(s0 + 0x10u) = 0;
    DSD(s1 + 0x10u) = 0;
    DSD(s0 + 0x2Cu) = 0x100u;
    DSD(s1 + 0x2Cu) = 0x80u;
    DSD(s0 + 0x40u) = 0x00010005u;
    DSB(s1 + 0x68u) = 0x44u;
    DSB(r0 + 0x55u) = 0x77u;
    DSW(DS_00107D2C) = 0;
    DSW(DS_00107D2C + 2u) = 0;
    for (i = 0; i < 0x80u; i++) DSB(DS_00107A80 + i) = 0;
    DSD(FIGHT_FD108_T) = 0xDEADu;
    DSD(FIGHT_FD108_T + 4u) = 0xBEEFu;
    DSW(DS_000FD114) = 0x7777u;
    DSB(DS_000FD11C) = 0x55u;
    DSB(DS_000FD11C + 1u) = 0x66u;
    DSB(DS_001078F8) = 0x33u;
    DSW(FIGHT_ACTORS + 0x20u) = 0x0F35u;
    DSW(FIGHT_ACTORS + 0x40u) = 0x0F35u;
    for (i = 0; i < 5u; i++) DSW(c3_streams[i]) = (u16)(0x1300u + i);
}

/* Record §38: the block family 0x1AB5C's arm reaches. 0x1A7CC (EAX = side):
 * +0x43 &= 0xFD, 0x18B04, +0x61/+0x60/+0x62 = 0/0x1E/1, +0x60 = byte +0xA of
 * the other side's 0x3AFC4 record (its +0x5F <= 0x3F), 0x1A6AC(slot, rec),
 * then the other side's 0x100CE0 word + 1 = k: k in 0..6 scales +0x60 by
 * the word at 0xA2C4C + 2k (percent), else +0x60 = 2; +0x52/+0x53 = 6/1.
 * 0x1A6AC starts 0xC8F40[char] (+0x54 0, +0x43 bit 0x20 clear) or
 * 0xC8F90[char] (+0x54 1, bit 0x10 clear) at 3.0 through 0x3C480 and sets
 * that bit alone of the pair. 0x1A8F4 (EDX = rec) restarts 0xC8F68[char]
 * (0xC8FB8[char] for +0x54 1) through 0x2BC30 with 9/0 and +0x62/+0x60 = 0.
 * 0x1A640 is the held-back direction. The four char-3 table entries are
 * pointed at crafted one-word streams and restored. */
static void check_block(void)
{
    u32 s0 = DS_001077B0, s1 = DS_001077B0 + 0x94u;
    u32 r0 = FIGHT_RECS, r1 = FIGHT_RECS + 0x100u;
    u32 pset1 = FIGHT_ACTORS + 0x20u;
    u32 st = FIGHT_RECS + 0x3A00u;
    u32 bst = FIGHT_RECS + 0x3B00u;
    static const u32 tabs[4] = { 0x000C8F40u, 0x000C8F90u, 0x000C8F68u,
                                 0x000C8FB8u };
    u32 sv_14ec = DSD(DS_001014EC);
    u8 sv_b00[4], sv_tab[28], sv_ce0[4], sv_e0[4], sv_bt[16];
    /* c3_seed's and the block path's shared state (the stream words, the
     * 0x396AC counters, 0x107D2C, FD108..FD11F, 0x1078F8, tb_seed's
     * 0x1080AC/AE, and 0x18B04 -> 0x18714's 0x100AF0/0x100AB0 writes). */
    u16 sv_st[5];
    u8 sv_a80[0x80], sv_d2c[4], sv_fd[0x18], sv_f8[2], sv_0ac[4];
    u8 sv_af0[8], sv_ab0[0x10];
    u32 i, tri0, k;
    s32 want;

    for (i = 0; i < 5u; i++) sv_st[i] = DSW(c3_streams[i]);
    tf_snap(sv_a80, DS_00107A80, 0x80u);
    tf_snap(sv_d2c, DS_00107D2C, 4u);
    tf_snap(sv_fd, FIGHT_FD108_T, 0x18u);
    tf_snap(sv_f8, DS_001078F8, 2u);
    tf_snap(sv_0ac, 0x001080ACu, 4u);
    tf_snap(sv_af0, 0x00100AF0u, 8u);
    tf_snap(sv_ab0, 0x00100AB0u, 0x10u);
    tf_snap(sv_b00, DS_00104B00, 4u);
    tf_snap(sv_tab, 0x000C8950u, 28u);
    tf_snap(sv_ce0, DS_00100CE0, 4u);
    tf_snap(sv_e0, DS_001088E0, 4u);
    for (i = 0; i < 4u; i++) tf_snap(sv_bt + i * 4u, tabs[i] + 3u * 4u, 4u);

    /* A: 0x1A7CC for side 0 (character 3). The other side (character 2)
     * has +0x5F = 0x3C, so +0x60 starts from byte +0xA of 0xDE114 + 11 *
     * (2 * 64 + 0x3C); its counter goes 1 -> 2, so k = 2 and the percentage
     * is the word at 0xA2C50 (0x55). */
    c3_seed(s0, s1, r0, r1, st);
    for (i = 0; i < 4u; i++) {
        DSW(bst + i * 0x10u) = (u16)(0x1400u + i);
        DSD(tabs[i] + 3u * 4u) = bst + i * 0x10u;
    }
    DSB(s0 + 0x43u) = 0x1Au;
    DSB(s0 + 0x54u) = 0u;
    DSB(s0 + 0x61u) = 0x55u;
    DSB(s0 + 0x62u) = 0u;
    DSW(DS_00100CE0) = 0x0005u;
    DSW(DS_00100CE0 + 2u) = 0x0001u;
    fighter_block_start(0u);
    tri0 = 0x000DE114u + 11u * ((2u << 6) + 0x3Cu);
    k = 2u;
    want = (s32)(s8)DSB(tri0 + 0x0Au) * (s32)(s16)DSW(DS_000A2C4A + 2u + k * 2u)
         / 100;
    CHECK_EQ_INT((int)DSW(DS_000A2C4A + 2u + k * 2u), 0x55);
    CHECK_EQ_INT((int)DSB(s0 + 0x60u), (int)(u8)want);
    CHECK_EQ_INT((int)DSW(DS_00100CE0 + 2u), 2);
    CHECK_EQ_INT((int)DSW(DS_00100CE0), 5);
    CHECK_EQ_INT((int)DSB(s0 + 0x43u), 0x28);
    CHECK_EQ_INT((int)DSB(s0 + 0x61u), 0);
    CHECK_EQ_INT((int)DSB(s0 + 0x62u), 1);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 6);
    CHECK_EQ_INT((int)DSB(s0 + 0x53u), 1);
    CHECK_EQ_INT((int)DSD(r0 + 8u), (int)bst);
    CHECK_EQ_INT((int)DSD(r0 + 0x24u), 0x40400000);
    CHECK_EQ_INT((int)(DSW(pset1) & 0x7FFFu), 0x1400);
    CHECK_EQ_INT((int)DSB(s1 + 0x52u), 0);
    CHECK_EQ_INT((int)DSD(r1 + 8u), 0x00ABCDEF);

    /* A2: k = 7 (6 + 1) and k = -1 (0xFFFE + 1) both give +0x60 = 2. */
    DSW(DS_00100CE0 + 2u) = 0x0006u;
    DSB(s0 + 0x43u) = 0x20u;                /* 0x1A6AC skips: bit 0x20 set */
    DSD(r0 + 8u) = 0x00ABCDEFu;
    fighter_block_start(0u);
    CHECK_EQ_INT((int)DSB(s0 + 0x60u), 2);
    CHECK_EQ_INT((int)DSW(DS_00100CE0 + 2u), 7);
    CHECK_EQ_INT((int)DSD(r0 + 8u), 0x00ABCDEF);
    CHECK_EQ_INT((int)DSB(s0 + 0x43u), 0x20);
    DSW(DS_00100CE0 + 2u) = 0xFFFEu;
    fighter_block_start(0u);
    CHECK_EQ_INT((int)DSB(s0 + 0x60u), 2);
    CHECK_EQ_INT((int)DSW(DS_00100CE0 + 2u), 0xFFFF);
    /* A3: the other side's +0x5F above 0x3F (0x1A810 `jg`) skips both
     * record reads, even with its +0x64 below 0x40 (0x0C, whose record byte
     * +0xA is 33, not 0x1E), leaving +0x60 at 0x1E
     * before the scaling: k = 1 is 100 percent. */
    DSW(DS_00100CE0 + 2u) = 0u;
    DSB(s1 + 0x5Fu) = 0x40u;
    DSB(s1 + 0x64u) = 0x0Cu;                /* its record byte +0xA is 33 */
    fighter_block_start(0u);
    CHECK_EQ_INT((int)DSB(0x000DE114u + 11u * ((2u << 6) + 0x0Cu) + 0x0Au), 33);
    CHECK_EQ_INT((int)DSW(DS_000A2C4A + 2u + 2u), 0x64);
    CHECK_EQ_INT((int)DSB(s0 + 0x60u), 0x1E);
    /* A4: the other side as character 6 with +0x5F = 0x24, whose record byte
     * +0xA is 55; k = 2 gives 55 * 0x55 / 100 = 46 (the raw's idiv by 100,
     * not 99, which would give 47). */
    DSB(s1 + 0x7Au) = 6u;
    DSB(s1 + 0x5Fu) = 0x24u;
    DSW(DS_00100CE0 + 2u) = 1u;
    fighter_block_start(0u);
    tri0 = 0x000DE114u + 11u * ((6u << 6) + 0x24u);
    CHECK_EQ_INT((int)(s8)DSB(tri0 + 0x0Au), 55);
    CHECK_EQ_INT((int)DSB(s0 + 0x60u), 46);
    DSB(s1 + 0x7Au) = 2u;
    DSB(s1 + 0x5Fu) = 0x3Cu;

    /* B: 0x1A6AC with +0x54 = 1: bit 0x10 clear starts 0xC8F90[3]; bit 0x10
     * set leaves the record. Its 0x18B04 is side 0's (0x33A68 from rec+0x51),
     * so side 1's record +0x18 keeps its sentinel. */
    DSB(s0 + 0x54u) = 1u;
    DSB(s0 + 0x43u) = 0x2Au;
    DSD(r0 + 8u) = 0x00ABCDEFu;
    DSD(r1 + 0x18u) = 0x5A5A5A5Au;
    fighter_block_anim(s0, r0);
    CHECK_EQ_INT((int)DSD(r0 + 8u), (int)(bst + 0x10u));
    CHECK_EQ_INT((int)DSB(s0 + 0x43u), 0x1A);
    CHECK_EQ_INT((int)DSD(r1 + 0x18u), 0x5A5A5A5A);
    DSD(r0 + 8u) = 0x00ABCDEFu;
    fighter_block_anim(s0, r0);
    CHECK_EQ_INT((int)DSD(r0 + 8u), 0x00ABCDEF);
    CHECK_EQ_INT((int)DSB(s0 + 0x43u), 0x1A);
    /* B2: +0x54 = 2 starts nothing. */
    DSB(s0 + 0x54u) = 2u;
    DSB(s0 + 0x43u) = 0u;
    fighter_block_anim(s0, r0);
    CHECK_EQ_INT((int)DSD(r0 + 8u), 0x00ABCDEF);
    CHECK_EQ_INT((int)DSB(s0 + 0x43u), 0);

    /* C: 0x1A8F4 from rec+0x51 (side 0): +0x54 = 0 -> 0xC8F68[3], 1 ->
     * 0xC8FB8[3], at 3.0, 9/0, +0x62/+0x60 = 0, +0x43 &= 0xCF. */
    DSB(s0 + 0x54u) = 0u;
    DSB(s0 + 0x43u) = 0xFFu;
    DSB(s0 + 0x52u) = 6u; DSB(s0 + 0x53u) = 1u;
    DSB(s0 + 0x62u) = 1u; DSB(s0 + 0x60u) = 7u;
    fighter_block_end(r0);
    CHECK_EQ_INT((int)DSD(r0 + 8u), (int)(bst + 0x20u));
    CHECK_EQ_INT((int)DSD(r0 + 0x24u), 0x40400000);
    CHECK_EQ_INT((int)(DSW(pset1) & 0x7FFFu), 0x1402);
    CHECK_EQ_INT((int)DSB(s0 + 0x43u), 0xCF);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 9);
    CHECK_EQ_INT((int)DSB(s0 + 0x53u), 0);
    CHECK_EQ_INT((int)DSB(s0 + 0x62u), 0);
    CHECK_EQ_INT((int)DSB(s0 + 0x60u), 0);
    DSB(s0 + 0x54u) = 1u;
    fighter_block_end(r0);
    CHECK_EQ_INT((int)DSD(r0 + 8u), (int)(bst + 0x30u));

    /* D: 0x1A640 (fighter_1a640, 0x1AA93's call) on slot 0's record +0x28
     * bit 0x4000 and command word. */
    DSW(r0 + 0x28u) &= 0xBFFFu;
    DSW(DS_001088E0) = 0x1000u;
    CHECK_EQ_INT(fighter_1a640(0u), 0x1000);
    DSW(DS_001088E0) = 0x2000u;
    CHECK_EQ_INT(fighter_1a640(0u), 0);
    DSW(r0 + 0x28u) |= 0x4000u;
    CHECK_EQ_INT(fighter_1a640(0u), 0x2000);
    DSW(DS_001088E0) = 0x1000u;
    CHECK_EQ_INT(fighter_1a640(0u), 0);

    /* E: 0x1A978 (the +0x52 == 6 handler) with +0x62 = 0 and +0x63 set:
     * +0x60 decrements; held back with no 0x8000/0x000F bits and +0x60 still
     * >= 1 keeps the block; releasing it (0x1A640 = 0) ends it (0x1A8F4). */
    DSW(r0 + 0x28u) &= 0xBFFFu;
    DSW(DS_001088E0) = 0x1000u;
    DSB(s1 + 0x5Fu) = 0xFFu;
    DSB(s1 + 0x64u) = 0xFFu;
    DSB(s0 + 0x63u) = 1u;
    DSB(s0 + 0x54u) = 0u;
    DSB(s0 + 0x52u) = 6u; DSB(s0 + 0x62u) = 0u; DSB(s0 + 0x60u) = 5u;
    DSD(r0 + 8u) = 0x00ABCDEFu;
    fight_stance_pass(0u);
    CHECK_EQ_INT((int)DSB(s0 + 0x60u), 4);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 6);
    CHECK_EQ_INT((int)DSD(r0 + 8u), 0x00ABCDEF);
    DSW(DS_001088E0) = 0x1008u;             /* a 0x000F bit ends it */
    fight_stance_pass(0u);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 9);
    CHECK_EQ_INT((int)DSD(r0 + 8u), (int)(bst + 0x20u));
    DSB(s0 + 0x52u) = 6u; DSB(s0 + 0x60u) = 5u;
    DSW(DS_001088E0) = 0x9000u;             /* the 0x8000 bit ends it */
    fight_stance_pass(0u);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 9);
    DSB(s0 + 0x52u) = 6u; DSB(s0 + 0x60u) = 1u;
    DSW(DS_001088E0) = 0x1000u;             /* +0x60 0 with no attacker */
    fight_stance_pass(0u);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 9);
    DSB(s0 + 0x52u) = 6u; DSB(s0 + 0x60u) = 5u;
    DSW(DS_001088E0) = 0u;                  /* not held back */
    fight_stance_pass(0u);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 9);

    /* F: the +0x62 arm's 0x1AA5F: the other side's +0x8A is cleared only
     * when self+0x86 equals other+0x84. */
    DSB(s0 + 0x62u) = 1u; DSB(s0 + 0x60u) = 1u;
    DSW(s0 + 0x86u) = 0x1234u; DSW(s1 + 0x84u) = 0x1234u;
    DSB(s1 + 0x8Au) = 0x77u;
    fight_stance_pass(0u);
    CHECK_EQ_INT((int)DSB(s1 + 0x8Au), 0);
    CHECK_EQ_INT((int)DSB(s0 + 0x60u), 0x28);
    CHECK_EQ_INT((int)DSB(s0 + 0x62u), 0);
    DSB(s0 + 0x62u) = 1u; DSB(s0 + 0x60u) = 1u;
    DSW(s1 + 0x84u) = 0x4321u;
    DSB(s1 + 0x8Au) = 0x77u;
    fight_stance_pass(0u);
    CHECK_EQ_INT((int)DSB(s1 + 0x8Au), 0x77);

    /* G: the +0x61 timer arm (+0x63 clear, +0x61 1 -> 0): command bit 0x4000
     * with +0x54 != 1 sets +0x54 = 1 and calls 0x1A6AC (0xC8F90[3]); bit
     * 0x4000 clear with +0x54 != 0 sets 0 and calls it (0xC8F40[3]). */
    DSB(s0 + 0x63u) = 0u;
    DSB(s0 + 0x62u) = 1u; DSB(s0 + 0x60u) = 9u;
    DSB(s0 + 0x61u) = 1u; DSB(s0 + 0x54u) = 0u; DSB(s0 + 0x43u) = 0u;
    DSW(DS_001088E0) = 0x4000u;
    DSD(r0 + 8u) = 0x00ABCDEFu;
    fight_stance_pass(0u);
    CHECK_EQ_INT((int)DSB(s0 + 0x54u), 1);
    CHECK_EQ_INT((int)DSD(r0 + 8u), (int)(bst + 0x10u));
    CHECK_EQ_INT((int)DSB(s0 + 0x43u), 0x10);
    DSB(s0 + 0x61u) = 1u; DSB(s0 + 0x43u) = 0u;
    DSW(DS_001088E0) = 0u;
    DSD(r0 + 8u) = 0x00ABCDEFu;
    fight_stance_pass(0u);
    CHECK_EQ_INT((int)DSB(s0 + 0x54u), 0);
    CHECK_EQ_INT((int)DSD(r0 + 8u), (int)bst);
    CHECK_EQ_INT((int)DSB(s0 + 0x43u), 0x20);

    /* H: 0x1A734 (record §39), 0x3B298's call at 0x3B443. +0x61 = 0x0C,
     * capped at +0x60 when 0x0C > (s8)+0x60 and +0x62 != 0; then, whatever
     * the stream, bit 0x20 restarts 0xC8F40[3] with +0x54 = 0 and bit 0x10
     * restarts 0xC8F90[3] with +0x54 = 1, at 3.0. Its 0x18B04 is side 0's:
     * side 0's record +0x18 takes 0x18714's x, side 1's keeps its sentinel. */
    DSB(s0 + 0x43u) = 0x2Au; DSB(s0 + 0x54u) = 1u;
    DSB(s0 + 0x60u) = 5u; DSB(s0 + 0x61u) = 0x77u; DSB(s0 + 0x62u) = 1u;
    DSD(r0 + 8u) = 0x00ABCDEFu; DSD(r0 + 0x24u) = 0x11111111u;
    DSW(pset1) = 0x0F35u;
    DSD(r0 + 0x18u) = 0x5A5A5A5Au;
    DSD(r1 + 0x18u) = 0x5A5A5A5Au;
    fighter_block_hit(0u);
    CHECK(DSD(r0 + 0x18u) != 0x5A5A5A5Au,
          "0x1A734's 0x18B04 rewrites side 0's record +0x18");
    CHECK_EQ_INT((int)DSD(r0 + 0x18u), (int)hit_record_x(0u));
    CHECK_EQ_INT((int)DSB(s0 + 0x61u), 5);
    CHECK_EQ_INT((int)DSB(s0 + 0x54u), 0);
    CHECK_EQ_INT((int)DSD(r0 + 8u), (int)bst);
    CHECK_EQ_INT((int)DSD(r0 + 0x24u), 0x40400000);
    CHECK_EQ_INT((int)(DSW(pset1) & 0x7FFFu), 0x1400);
    CHECK_EQ_INT((int)DSB(s0 + 0x43u), 0x2A);
    CHECK_EQ_INT((int)DSD(r1 + 0x18u), 0x5A5A5A5A);
    /* H2: bit 0x10 alone, +0x60 = 0x20 above 0x0C: no cap. */
    DSB(s0 + 0x43u) = 0x10u; DSB(s0 + 0x54u) = 0u;
    DSB(s0 + 0x60u) = 0x20u; DSB(s0 + 0x61u) = 0x77u;
    DSD(r0 + 8u) = 0x00ABCDEFu;
    fighter_block_hit(0u);
    CHECK_EQ_INT((int)DSB(s0 + 0x61u), 0x0C);
    CHECK_EQ_INT((int)DSB(s0 + 0x54u), 1);
    CHECK_EQ_INT((int)DSD(r0 + 8u), (int)(bst + 0x10u));
    CHECK_EQ_INT((int)DSB(s0 + 0x43u), 0x10);
    /* H3: the compare is signed (`jle`): +0x60 = 0x80 (-128) caps +0x61 to
     * 0x80; with +0x62 = 0 it stays 0x0C. */
    DSB(s0 + 0x60u) = 0x80u; DSB(s0 + 0x61u) = 0x77u;
    fighter_block_hit(0u);
    CHECK_EQ_INT((int)DSB(s0 + 0x61u), 0x80);
    DSB(s0 + 0x62u) = 0u; DSB(s0 + 0x60u) = 5u; DSB(s0 + 0x61u) = 0x77u;
    fighter_block_hit(0u);
    CHECK_EQ_INT((int)DSB(s0 + 0x61u), 0x0C);
    /* H4: neither bit: no stream, +0x54 kept. */
    DSB(s0 + 0x43u) = 0x0Au; DSB(s0 + 0x54u) = 7u;
    DSD(r0 + 8u) = 0x00ABCDEFu;
    fighter_block_hit(0u);
    CHECK_EQ_INT((int)DSD(r0 + 8u), 0x00ABCDEF);
    CHECK_EQ_INT((int)DSB(s0 + 0x54u), 7);
    CHECK_EQ_INT((int)DSB(s0 + 0x43u), 0x0A);

    for (i = 0; i < 4u; i++) tf_put(sv_bt + i * 4u, tabs[i] + 3u * 4u, 4u);
    tf_put(sv_ab0, 0x00100AB0u, 0x10u);
    tf_put(sv_af0, 0x00100AF0u, 8u);
    tf_put(sv_0ac, 0x001080ACu, 4u);
    tf_put(sv_f8, DS_001078F8, 2u);
    tf_put(sv_fd, FIGHT_FD108_T, 0x18u);
    tf_put(sv_d2c, DS_00107D2C, 4u);
    tf_put(sv_a80, DS_00107A80, 0x80u);
    for (i = 0; i < 5u; i++) DSW(c3_streams[i]) = sv_st[i];
    tf_put(sv_e0, DS_001088E0, 4u);
    tf_put(sv_ce0, DS_00100CE0, 4u);
    tf_put(sv_tab, 0x000C8950u, 28u);
    tf_put(sv_b00, DS_00104B00, 4u);
    DSD(DS_001014EC) = sv_14ec;
}

/* 0x1490C (*(u32*)0xA4734 reads `0c 49 01 00 00 00 00 00`: character 3's
 * reaction 0x27, no stream; 0xA4734 - 0xA3528 = 231 records of 0x14 = 3*64 +
 * 0x27), reached on the second demo's first state-7 frame. EAX = slot, EDX =
 * rec (EBX pushed, unread): 0x14814 then FD11C[side] = 1. 0x14814: 0x396AC
 * (ctx[0], 0) true -> 0x18B44(ctx[2]), stream 0xD2E86 at the 0x9AFE4 hold
 * (0x40200000), state 9/4/0; false -> 0xD2EDC (ctx[3]+0x68 - 1) when 0x14590
 * holds, else 0xD2E9A, at 1.0, state 9/7/0, +0x57 = 0, +0x0C/+0x18/+0x1C =
 * 0x1461C/0x145CC/0x145E4, +0x40 |= 0x48000, rec+0x55 = 0, FD108[side] =
 * 0x2000 (self x above the other's) or 0x1000, FD11C[side] = 0. 0x14590:
 * ctx[3]'s +0x10 == 0x39CC8 or +0x52 == 0x11, and the 0x107D2C word > 0.
 * 0x396AC's threshold for (char 3, 0x27) is the word at 0xA6C96 = 1. */
static void check_char3_reaction(void)
{
    u32 s0 = DS_001077B0, s1 = DS_001077B0 + 0x94u;
    u32 r0 = FIGHT_RECS, r1 = FIGHT_RECS + 0x100u;
    u32 pset1 = FIGHT_ACTORS + 0x20u;
    u32 st = FIGHT_RECS + 0x3A00u;
    u32 sv_14ec = DSD(DS_001014EC);
    u16 sv_st[5];
    u8 sv_b00[4], sv_tab[28], sv_a80[0x80], sv_d2c[4], sv_d20[4], sv_fd[0x18];
    u8 sv_f8[2], sv_hud[10], sv_ae9, sv_c1d, sv_af8[8], sv_e0[4], sv_d28[4];
    u32 i;
    typedef void (*anim_fn)(u32 rec, u32 arg);
    anim_fn f37;

    for (i = 0; i < 5u; i++) sv_st[i] = DSW(c3_streams[i]);
    tf_snap(sv_b00, DS_00104B00, 4u);
    tf_snap(sv_tab, 0x000C8950u, 28u);
    tf_snap(sv_a80, DS_00107A80, 0x80u);
    tf_snap(sv_d2c, DS_00107D2C, 4u);
    tf_snap(sv_d20, 0x00107D20u, 4u);
    tf_snap(sv_fd, FIGHT_FD108_T, 0x18u);
    tf_snap(sv_f8, DS_001078F8, 2u);
    tf_snap(sv_hud, DS_001088E8, 10u);
    tf_snap(sv_af8, DS_00100AF8, 8u);
    tf_snap(sv_e0, DS_001088E0, 4u);
    tf_snap(sv_d28, DS_00107D28, 4u);
    sv_ae9 = DSB(DS_00104AE9);
    sv_c1d = DSB(DS_00100C1D);

    CHECK(fn_resolve(0x1490Cu) == (void (*)(void))fighter_1490c,
          "0x1490C is registered as fighter_1490c");
    CHECK(fn_resolve(0x14814u) == (void (*)(void))fighter_14814,
          "0x14814 is registered as fighter_14814");
    CHECK(fn_resolve(0x1461Cu) == (void (*)(void))fighter_1461c,
          "0x1461C is registered as fighter_1461c");
    CHECK(fn_resolve(0x145CCu) == (void (*)(void))fighter_145cc,
          "0x145CC is registered as fighter_145cc");
    CHECK(fn_resolve(0x145E4u) == (void (*)(void))fighter_145e4,
          "0x145E4 is registered as fighter_145e4");
    f37 = (anim_fn)(void *)fn_resolve(0x37CD4u);
    CHECK(f37 != NULL, "0x37CD4 is a registered stream target");

    /* A: through 0x34E2C (0x35045) for character 3, reaction 0x27. 0x396AC
     * bumps 0x107A80[0x27] to 1 (<= 1: false); 0x14590 is false (+0x10 0,
     * +0x52 0), so 0xD2E9A at 1.0 through 0x2BC30 (+0x52 = 0). */
    c3_seed(s0, s1, r0, r1, st);
    DSW(DS_001088E0) = 0;
    hit_reaction_apply(0u, 0x27u);
    CHECK_EQ_INT((int)DSB(DS_00107A80 + 0x27u), 1);
    CHECK_EQ_INT((int)DSD(r0 + 8u), 0x000D2E9A);
    CHECK_EQ_INT((int)DSD(r0 + 0x24u), 0x3F800000);
    CHECK_EQ_INT((int)(DSW(pset1) & 0x7FFFu), 0x1301);
    CHECK_EQ_INT((int)DSB(s0 + 0x5Fu), 0x27);
    CHECK_EQ_INT((int)DSB(s0 + 0x57u), 0);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 9);
    CHECK_EQ_INT((int)DSB(s0 + 0x53u), 7);
    CHECK_EQ_INT((int)DSB(s0 + 0x54u), 0);
    CHECK_EQ_INT((int)DSD(s0 + 0x0Cu), 0x0001461C);
    CHECK_EQ_INT((int)DSD(s0 + 0x18u), 0x000145CC);
    CHECK_EQ_INT((int)DSD(s0 + 0x1Cu), 0x000145E4);
    CHECK_EQ_INT((int)DSD(s0 + 0x40u), 0x00058005);
    CHECK_EQ_INT((int)DSB(r0 + 0x55u), 0);
    CHECK_EQ_INT((int)DSD(FIGHT_FD108_T), 0x2000);
    CHECK_EQ_INT((int)DSD(FIGHT_FD108_T + 4u), 0xBEEF);
    CHECK_EQ_INT((int)DSB(DS_000FD11C), 1);
    CHECK_EQ_INT((int)DSB(DS_000FD11C + 1u), 0x66);
    CHECK_EQ_INT((int)DSB(s1 + 0x68u), 0x44);
    CHECK_EQ_INT((int)DSD(r1 + 8u), 0x00ABCDEF);
    CHECK_EQ_INT((int)DSD(s1 + 0x0Cu), 0x11111111);

    /* A2: 0x14814's own entrance, *(u32*)0xA46D0 (`14 48 01 00 00 00 00 00`:
     * character 3's reaction 0x22, no stream), through 0x34E2C. The counter
     * 0x107A80[0x22] goes to 1 (the word at 0xA6C78 is 1: false), so 0xD2E9A
     * and the slot armed as in A, and FD11C[0] goes from its sentinel to 0:
     * no 0x1490C follows to set it. */
    c3_seed(s0, s1, r0, r1, st);
    DSW(DS_001088E0) = 0;
    hit_reaction_apply(0u, 0x22u);
    CHECK_EQ_INT((int)DSB(DS_00107A80 + 0x22u), 1);
    CHECK_EQ_INT((int)DSD(r0 + 8u), 0x000D2E9A);
    CHECK_EQ_INT((int)DSB(s0 + 0x5Fu), 0x22);
    CHECK_EQ_INT((int)DSD(s0 + 0x0Cu), 0x0001461C);
    CHECK_EQ_INT((int)DSB(DS_000FD11C), 0);
    CHECK_EQ_INT((int)DSB(DS_000FD11C + 1u), 0x66);
    CHECK_EQ_INT((int)DSD(FIGHT_FD108_T), 0x2000);

    /* B: direct, EBX = 1 unread (side 0 from rec+0x51). +0x5F = 0xFF keeps
     * 0x396AC out (b >= 0x40, no bump). The other slot in the knockback pose
     * (+0x10 = 0x39CC8) with 0x107D2C = 1: 0x14590 holds, so 0xD2EDC and
     * ctx[3]'s +0x68 - 1; x below the other's gives 0x1000. */
    c3_seed(s0, s1, r0, r1, st);
    DSB(s0 + 0x5Fu) = 0xFFu;
    DSD(s1 + 0x10u) = 0x00039CC8u;
    DSW(DS_00107D2C) = 1u;
    DSD(s0 + 0x2Cu) = 0x80u;
    DSD(s1 + 0x2Cu) = 0x100u;
    fighter_1490c(s0, r0, 1u);
    CHECK_EQ_INT((int)DSD(r0 + 8u), 0x000D2EDC);
    CHECK_EQ_INT((int)DSB(s1 + 0x68u), 0x43);
    CHECK_EQ_INT((int)DSD(FIGHT_FD108_T), 0x1000);
    CHECK_EQ_INT((int)DSB(DS_000FD11C), 1);
    CHECK_EQ_INT((int)DSB(DS_000FD11C + 1u), 0x66);
    CHECK_EQ_INT((int)DSB(DS_00107A80 + 0x3Fu), 0);
    /* B2: +0x52 == 0x11 also holds (0x145AA); 0x107D2C = 0 does not. */
    c3_seed(s0, s1, r0, r1, st);
    DSB(s0 + 0x5Fu) = 0xFFu;
    DSB(s1 + 0x52u) = 0x11u;
    DSW(DS_00107D2C) = 1u;
    fighter_1490c(s0, r0, 0u);
    CHECK_EQ_INT((int)DSD(r0 + 8u), 0x000D2EDC);
    c3_seed(s0, s1, r0, r1, st);
    DSB(s0 + 0x5Fu) = 0xFFu;
    DSD(s1 + 0x10u) = 0x00039CC8u;
    fighter_1490c(s0, r0, 0u);
    CHECK_EQ_INT((int)DSD(r0 + 8u), 0x000D2E9A);
    CHECK_EQ_INT((int)DSB(s1 + 0x68u), 0x44);

    /* C: 0x396AC true (0x107A80[0x27] = 1 -> 2 > 1, 0x107D2C = 1): 0x18B44
     * (slot +0x63 = 1 returns at once), 0xD2E86 at the 0x9AFE4 hold, state
     * 9/4/0; the callbacks, +0x57 and FD108 keep their sentinels. */
    c3_seed(s0, s1, r0, r1, st);
    DSB(s0 + 0x5Fu) = 0x27u;
    DSB(DS_00107A80 + 0x27u) = 1u;
    DSW(DS_00107D2C) = 1u;
    DSB(DS_00100C1D) = 0;
    fighter_1490c(s0, r0, 0u);
    CHECK_EQ_INT((int)DSB(DS_00107A80 + 0x27u), 2);
    CHECK_EQ_INT((int)DSD(r0 + 8u), 0x000D2E86);
    CHECK_EQ_INT((int)DSD(r0 + 0x24u), 0x40200000);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 9);
    CHECK_EQ_INT((int)DSB(s0 + 0x53u), 4);
    CHECK_EQ_INT((int)DSB(s0 + 0x54u), 0);
    CHECK_EQ_INT((int)DSD(s0 + 0x0Cu), 0x11111111);
    CHECK_EQ_INT((int)DSB(s0 + 0x57u), 0x99);
    CHECK_EQ_INT((int)DSD(FIGHT_FD108_T), 0xDEAD);
    CHECK_EQ_INT((int)DSB(DS_000FD11C), 1);
    CHECK_EQ_INT((int)DSB(DS_00100C1D), 0);

    /* D: 0x1461C through 0x3531C case 7's register shape: EAX = slot 1, EDX
     * = rec 1, EBX = side 0 (every read and write is side 0's). */
    c3_seed(s0, s1, r0, r1, st);
    DSB(DS_001088F0) = 0x77u;
    DSW(DS_001088E8) = 0x1234u;
    DSB(DS_00104AE9) = 0x01u;
    DSB(s0 + 0x57u) = 0u;
    DSB(s0 + 0x42u) = 0x04u;
    fighter_1461c(s1, r1, 0u);                      /* bit 3 clear: waits */
    CHECK_EQ_INT((int)DSB(s0 + 0x57u), 0);
    CHECK_EQ_INT((int)DSW(DS_000FD114), 0x7777);
    DSB(s0 + 0x42u) = 0x08u;
    fighter_1461c(s1, r1, 0u);                      /* 0x14590 false */
    CHECK_EQ_INT((int)DSB(s0 + 0x57u), 1);
    CHECK_EQ_INT((int)DSW(DS_000FD114), 0x14);
    CHECK_EQ_INT((int)DSB(DS_001088F0), 0x77);
    CHECK_EQ_INT((int)DSB(s1 + 0x57u), 0x9A);
    DSB(s0 + 0x57u) = 0u;
    DSD(s1 + 0x10u) = 0x00039CC8u;
    DSW(DS_00107D2C) = 1u;
    fighter_1461c(s1, r1, 0u);                      /* 0x14590 true */
    CHECK_EQ_INT((int)DSB(s0 + 0x57u), 1);
    CHECK_EQ_INT((int)DSW(DS_000FD114), 4);
    CHECK_EQ_INT((int)DSB(DS_001088F0), 1);
    CHECK_EQ_INT((int)DSW(DS_001088E8), 0);
    CHECK_EQ_INT((int)DSW(0x001088EAu), 0x10);
    CHECK_EQ_INT((int)DSB(DS_00104AE9), 0x09);

    /* D2: +0x57 = 1: the counter steps down and 0x3F720(0, old) sets the
     * record's +0x34. Both slots' +0x42 bit 3 make 0x187FC's latch copy
     * rec+0x18 (x 0x30 and 0x90: |d| = 0x60). Actor 0's bit 15 clear:
     * 0x189FC(1) is 0x90 < 0x30 = 0, so 100, negated (unflipped) = 0xFF9C;
     * the counter 3 -> 2 keeps +0x57 = 1. Bit 15 set: 0x90 > 0x30 = 1, so
     * 0x60 / 3 = 0x20 unnegated; 1 -> 0 sets +0x57 = 2; 0 -> -1 divides by 1. */
    c3_seed(s0, s1, r0, r1, st);
    DSB(s0 + 0x57u) = 1u;
    DSB(s0 + 0x42u) = 0x08u;
    DSB(s1 + 0x42u) = 0x08u;
    DSD(r0 + 0x18u) = 0x30u;
    DSD(r1 + 0x18u) = 0x90u;
    DSW(r0 + 0x34u) = 0x7777u;
    DSW(DS_000FD114) = 3u;
    fighter_1461c(s1, r1, 0u);
    CHECK_EQ_INT((int)DSW(DS_000FD114), 2);
    CHECK_EQ_INT((int)DSW(r0 + 0x34u), 0xFF9C);
    CHECK_EQ_INT((int)DSB(s0 + 0x57u), 1);
    DSW(pset1) = 0x8F35u;
    DSW(DS_000FD114) = 3u;
    fighter_1461c(s1, r1, 0u);
    CHECK_EQ_INT((int)DSW(r0 + 0x34u), 0x0020);
    DSW(DS_000FD114) = 1u;
    fighter_1461c(s1, r1, 0u);
    CHECK_EQ_INT((int)DSW(DS_000FD114), 0);
    CHECK_EQ_INT((int)DSW(r0 + 0x34u), 0x0060);
    CHECK_EQ_INT((int)DSB(s0 + 0x57u), 2);
    DSB(s0 + 0x57u) = 1u;
    DSW(DS_000FD114) = 0u;
    DSW(r0 + 0x34u) = 0x7777u;
    fighter_1461c(s1, r1, 0u);
    CHECK_EQ_INT((int)(s16)DSW(DS_000FD114), -1);
    CHECK_EQ_INT((int)DSW(r0 + 0x34u), 0x0060);

    /* D3: +0x57 = 2 runs 0x146F0 on EDX's record (here the slot's own).
     * FD11C 0, 0x14590 false: 0xD2EBA through 0x3C480; the 0x1088E0 word 0
     * takes FD108 (0x2000), so x = the other's +0x2C - 0x1180 (the 0x9AFDA
     * dword's high half); 0x3C148 zeroes rec+0x34; +0x42 bit 2 cleared,
     * +0x57 = 3, the 0x1078F8 byte kept. */
    c3_seed(s0, s1, r0, r1, st);
    DSB(s0 + 0x57u) = 2u;
    DSB(s0 + 0x42u) = 0x0Cu;
    DSB(s1 + 0x42u) = 0x08u;
    DSW(DS_001088E0) = 0;
    DSD(FIGHT_FD108_T) = 0x2000u;
    DSD(s1 + 0x2Cu) = 0x5000u;
    DSW(r0 + 0x34u) = 0x7777u;
    DSB(DS_000FD11C) = 0;
    fighter_1461c(s0, r0, 0u);
    CHECK_EQ_INT((int)DSB(s0 + 0x57u), 3);
    CHECK_EQ_INT((int)DSD(r0 + 8u), 0x000D2EBA);
    CHECK_EQ_INT((int)DSD(r0 + 0x24u), 0x3F800000);
    CHECK_EQ_INT((int)DSD(s0 + 0x2Cu), 0x5000 - 0x1180);
    CHECK_EQ_INT((int)DSW(r0 + 0x34u), 0);
    CHECK_EQ_INT((int)DSB(s0 + 0x42u), 0x08);
    CHECK_EQ_INT((int)DSB(DS_001078F8), 0x33);
    CHECK_EQ_INT((int)DSB(s0 + 0x54u), 0x66);
    /* D4: FD11C set, 0x14590 true: 0xD2EFC through 0x3C520, the anchor y
     * 0x3200 (0x9AFDC's high half), +0x54 = 2, rec +0x36/+0x44 = 0x19 * 3 /
     * 0x19, 0x1078F8[0] = 1; the 0x1088E0 word 0x1000 (bits 0x3000 set)
     * skips FD108, so x = the other's +0x2C + 0x1180. */
    c3_seed(s0, s1, r0, r1, st);
    DSB(s0 + 0x57u) = 2u;
    DSB(s0 + 0x42u) = 0x0Cu;
    DSB(s1 + 0x42u) = 0x08u;
    DSD(s1 + 0x10u) = 0x00039CC8u;
    DSW(DS_00107D2C) = 1u;
    DSB(DS_000FD11C) = 1u;
    DSW(DS_001088E0) = 0x1000u;
    DSD(FIGHT_FD108_T) = 0x2000u;
    DSD(s1 + 0x2Cu) = 0x5000u;
    DSD(r0 + 0x1Cu) = 0;
    fighter_1461c(s0, r0, 0u);
    CHECK_EQ_INT((int)DSB(s0 + 0x57u), 3);
    CHECK_EQ_INT((int)DSD(r0 + 8u), 0x000D2EFC);
    CHECK_EQ_INT((int)DSD(r0 + 0x1Cu), 0x3200);
    CHECK_EQ_INT((int)DSB(s0 + 0x54u), 2);
    CHECK_EQ_INT((int)DSW(r0 + 0x36u), 0x4B);
    CHECK_EQ_INT((int)DSW(r0 + 0x44u), 0x19);
    CHECK_EQ_INT((int)DSB(DS_001078F8), 1);
    CHECK_EQ_INT((int)DSD(s0 + 0x2Cu), 0x5000 + 0x1180);
    /* D5: +0x57 = 3 returns; nothing is written. */
    DSW(DS_000FD114) = 0x7777u;
    DSD(r0 + 8u) = 0x00ABCDEFu;
    fighter_1461c(s0, r0, 0u);
    CHECK_EQ_INT((int)DSB(s0 + 0x57u), 3);
    CHECK_EQ_INT((int)DSW(DS_000FD114), 0x7777);
    CHECK_EQ_INT((int)DSD(r0 + 8u), 0x00ABCDEF);

    /* E: 0x145CC through 0x19020: returns 1, so DS_00100AF8[0] = 0. */
    c3_seed(s0, s1, r0, r1, st);
    DSD(s0 + 0x18u) = 0x000145CCu;
    DSD(DS_00100AF8) = 5u;
    CHECK_EQ_INT((int)fighter_145cc(0u), 1);
    fighter_19020(0u);
    CHECK_EQ_INT((int)DSD(DS_00100AF8), 0);

    /* F: 0x145E4(0) is 0x39834(1, slot 0's +0x5F): 0x39834 counts the hit on
     * 0x33A10(1)'s ctx[0] = side 0 (the 0x107D2C word + 1). */
    c3_seed(s0, s1, r0, r1, st);
    DSB(s0 + 0x5Fu) = 0x27u;
    DSB(s1 + 0x5Fu) = 0x25u;
    DSB(s0 + 0x64u) = 0x21u;
    DSW(DS_00107D2C) = 7u;
    DSW(DS_00107D2C + 2u) = 9u;
    DSD(DS_00107D28) = 0x5A5A5A5Au;
    fighter_145e4(0u);
    CHECK_EQ_INT((int)DSW(DS_00107D2C), 8);
    CHECK_EQ_INT((int)DSW(DS_00107D2C + 2u), 9);
    CHECK_EQ_INT((int)DSD(DS_00107D28), 0x27);                  /* 0x39953 */
    /* F2: 0x145FC `and edx,0xff` zero-extends the byte. 0xA7 is outside
     * 0x3AFC4's 0..0x3F (the raw's 0x62003 error; the port's zero triple),
     * but 0x39834 still stores b at 0x39953. */
    DSB(s0 + 0x5Fu) = 0xA7u;
    DSD(DS_00107D28) = 0x5A5A5A5Au;
    fighter_145e4(0u);
    CHECK_EQ_INT((int)DSD(DS_00107D28), 0xA7);

    /* G: 0x37CD4 as anim_indirect calls it, (rec, operand): toggles the
     * slot's +0x42 bit 3 with +0x74 = 0x378 / 0; no slot, no write. */
    if (f37 != NULL) {
        DSD(r1 + 0x14u) = s0;
        DSB(s0 + 0x42u) = 0x04u;
        DSW(s0 + 0x74u) = 0x1111u;
        f37(r1, 0xFFFFu);
        CHECK_EQ_INT((int)DSB(s0 + 0x42u), 0x0C);
        CHECK_EQ_INT((int)DSW(s0 + 0x74u), 0x378);
        f37(r1, 0u);
        CHECK_EQ_INT((int)DSB(s0 + 0x42u), 0x04);
        CHECK_EQ_INT((int)DSW(s0 + 0x74u), 0);
        DSD(r1 + 0x14u) = 0;
        DSW(s0 + 0x74u) = 0x2222u;
        f37(r1, 0u);
        CHECK_EQ_INT((int)DSB(s0 + 0x42u), 0x04);
        CHECK_EQ_INT((int)DSW(s0 + 0x74u), 0x2222);
    }

    for (i = 0; i < 5u; i++) DSW(c3_streams[i]) = sv_st[i];
    DSB(DS_00100C1D) = sv_c1d;
    DSB(DS_00104AE9) = sv_ae9;
    tf_put(sv_e0, DS_001088E0, 4u);
    tf_put(sv_d28, DS_00107D28, 4u);
    tf_put(sv_af8, DS_00100AF8, 8u);
    tf_put(sv_hud, DS_001088E8, 10u);
    tf_put(sv_f8, DS_001078F8, 2u);
    tf_put(sv_fd, FIGHT_FD108_T, 0x18u);
    tf_put(sv_d20, 0x00107D20u, 4u);
    tf_put(sv_d2c, DS_00107D2C, 4u);
    tf_put(sv_a80, DS_00107A80, 0x80u);
    tf_put(sv_tab, 0x000C8950u, 28u);
    tf_put(sv_b00, DS_00104B00, 4u);
    DSD(DS_001014EC) = sv_14ec;
}

/* ---- roar-timing Task 25: the slot +0x18 hook 0x19020 (record §35) ------- */

/* tb_seed with every field 0x18C14 reads set so that no check fires: the
 * slots' +0x18/+0x42/+0x43/+0x53/+0x54/+0x58/+0x10/+0x62/+0x74/+0x76/+0x7A/
 * +0x2C/+0x86 = 0, +0x63 = 1 (0x18B44 returns at once), +0x8A sentinels
 * 0x77/0x78, the records' +0x61/+0x18/+0x1C = 0, both actors' bit 15 clear,
 * and DS_00100AF8/AFC = 5 (> 0). */
static void sh_seed(u32 s0, u32 s1, u32 r0, u32 r1)
{
    u32 s[2], r[2], i;
    tb_seed(s0, s1, r0, r1);
    DSD(DS_001014EC) = FIGHT_ACTORS;
    s[0] = s0; s[1] = s1; r[0] = r0; r[1] = r1;
    for (i = 0; i < 2u; i++) {
        DSD(s[i] + 0x10u) = 0;
        DSD(s[i] + 0x18u) = 0;
        DSD(s[i] + 0x2Cu) = 0;
        DSD(s[i] + 0x30u) = 0;
        DSB(s[i] + 0x41u) = 0;
        DSB(s[i] + 0x42u) = 0;
        DSB(s[i] + 0x43u) = 0;
        DSB(s[i] + 0x53u) = 0;
        DSB(s[i] + 0x54u) = 0;
        DSB(s[i] + 0x58u) = 0;
        DSB(s[i] + 0x5Fu) = 0;
        DSB(s[i] + 0x62u) = 0;
        DSB(s[i] + 0x63u) = 1u;
        DSW(s[i] + 0x74u) = 0;
        DSW(s[i] + 0x76u) = 0;
        DSB(s[i] + 0x7Au) = 0;
        DSW(s[i] + 0x84u) = (u16)(0x1234u + i);
        DSD(s[i] + 0x86u) = 0;
        DSB(s[i] + 0x8Au) = (u8)(0x77u + i);
        DSB(r[i] + 0x61u) = 0;
        DSD(r[i] + 0x18u) = 0;
        DSD(r[i] + 0x1Cu) = 0;
        DSW(FIGHT_ACTORS + (1u + i) * 0x20u) = 0x0F35u;
    }
    DSD(DS_00100AF8) = 5u;
    DSD(DS_00100AFC) = 5u;
}

/* 0x18C14 with every flag 2 except flags[idx] = val, the default box tables
 * (box_a = box_b = 0) unless given. */
static int sh_walk(u32 side, u32 idx, u8 val, u32 box_a, u32 box_b)
{
    u8 flags[16];
    u32 i;
    for (i = 0; i < 16u; i++) flags[i] = 2u;
    flags[idx] = val;
    return fighter_18c14(side, flags, box_a, box_b);
}

/* 0x18C14 (the raw at 0x18C14..0x1901E, capstone over read_memory): each flag
 * 2 skips its check, 0 returns 1 when the condition holds and 1 when it fails
 * (flag 0 the other way round, rewriting the flag to 4/3); the checks run in
 * the order 0, 1, 0xF, 2, 3, 5, 6, 7, 8, 9, 0xA, 0xB, 0xD, 0xC, 0xE, 4; flags
 * 7/0xD clear ctx[2]'s +0x8A and run 0x18B44, flag 0xE calls 0x3B298 for any
 * value but 2. 0x19020 stores (hook(side) == 0) in DS_00100AF8[side]; the
 * hooks 0x3E484 (flags 0 = 1, 1 = 0, 8 = 0) and 0x3E1D0 (flag 5 = 1 on the
 * 0xC75F5/0xC75FF boxes, 1/4/7/8/0xD/0xE = 0, gated on +0x86 >> 16 in
 * 1..3). The box tables read `6e`x7 (0xC75F5), `63`x7 (0xC75FF) and `e7`x7
 * (0xA1818, 0xA1822) from read_memory. */
static void check_slot_hook(void)
{
    u32 s0 = DS_001077B0, s1 = DS_001077B0 + 0x94u;
    u32 r0 = FIGHT_RECS, r1 = FIGHT_RECS + 0x100u;
    u32 ba = FIGHT_RECS + 0x3C00u, bb = FIGHT_RECS + 0x3C10u;
    u32 a0 = FIGHT_ACTORS + 0x20u, a1 = FIGHT_ACTORS + 0x40u;
    u32 sv_14ec = DSD(DS_001014EC);
    u16 sv_ac = DSW(0x001080ACu), sv_ae = DSW(0x001080AEu);
    u16 sv_w2 = DSW(DS_000A6728 + 2u);
    u8 sv_b00[4], sv_af8[8], sv_c1d = DSB(DS_00100C1D);
    u8 sv_7a8[8], sv_ab0[16], sv_ring[0x50], sv_8e0[4];
    u8 sv_78fa = DSB(DS_001078FA);
    u8 flags[16];
    u32 i;
    tf_snap(sv_b00, DS_00104B00, 4u);
    tf_snap(sv_af8, DS_00100AF8, 8u);
    tf_snap(sv_7a8, DS_001077A8, 8u);
    tf_snap(sv_ab0, DS_00100AB0, 16u);
    tf_snap(sv_ring, DS_00108270, 0x50u);
    tf_snap(sv_8e0, DS_001088E0, 4u);

    CHECK(fn_resolve(0x3E484u) == (void (*)(void))fighter_3e484,
          "0x3E484 is registered as fighter_3e484");
    CHECK(fn_resolve(0x3E1D0u) == (void (*)(void))fighter_3e1d0,
          "0x3E1D0 is registered as fighter_3e1d0");
    CHECK(fn_resolve(0x3D484u) == NULL, "0x3D484 stays unregistered");
    CHECK_EQ_INT((int)DSB(0x000C75F5u), 0x6E);
    CHECK_EQ_INT((int)DSB(0x000C75FFu), 0x63);
    CHECK_EQ_INT((int)DSB(0x000A1818u), 0xE7);
    CHECK_EQ_INT((int)DSB(0x000A1822u), 0xE7);

    /* A: every flag 2 passes every check: 0 for a live side. */
    sh_seed(s0, s1, r0, r1);
    CHECK_EQ_INT(sh_walk(0u, 0u, 2u, 0u, 0u), 0);
    CHECK_EQ_INT(sh_walk(1u, 0u, 2u, 0u, 0u), 0);

    /* B: flag 0 on DS_00100AF8[side] <= 0 (signed), inverted, rewritten. */
    for (i = 0; i < 16u; i++) flags[i] = 2u;
    flags[0] = 0;
    CHECK_EQ_INT(fighter_18c14(0u, flags, 0u, 0u), 1);          /* 5 > 0 */
    CHECK_EQ_INT((int)flags[0], 4);
    flags[0] = 1u;
    CHECK_EQ_INT(fighter_18c14(0u, flags, 0u, 0u), 0);
    CHECK_EQ_INT((int)flags[0], 1);
    DSD(DS_00100AF8) = 0;
    CHECK_EQ_INT(fighter_18c14(0u, flags, 0u, 0u), 1);
    CHECK_EQ_INT((int)flags[0], 3);
    flags[0] = 0;
    CHECK_EQ_INT(fighter_18c14(0u, flags, 0u, 0u), 0);
    CHECK_EQ_INT((int)flags[0], 0);
    CHECK_EQ_INT(sh_walk(1u, 0u, 1u, 0u, 0u), 0);               /* AF8[1] = 5 */
    DSD(DS_00100AFC) = 0;
    CHECK_EQ_INT(sh_walk(1u, 0u, 1u, 0u, 0u), 1);
    DSD(DS_00100AF8) = 0xFFFFFFFFu;                            /* -1 */
    CHECK_EQ_INT(sh_walk(0u, 0u, 1u, 0u, 0u), 1);

    /* C: flag 1 on the other slot's words +0x74/+0x76. */
    sh_seed(s0, s1, r0, r1);
    DSW(s1 + 0x76u) = 1u;
    DSW(s0 + 0x74u) = 9u;                    /* the self slot is not read */
    CHECK_EQ_INT(sh_walk(0u, 1u, 0u, 0u, 0u), 0);
    CHECK_EQ_INT(sh_walk(0u, 1u, 1u, 0u, 0u), 1);
    DSW(s1 + 0x76u) = 2u;
    CHECK_EQ_INT(sh_walk(0u, 1u, 0u, 0u, 0u), 1);
    CHECK_EQ_INT(sh_walk(0u, 1u, 1u, 0u, 0u), 1);                /* +0x74 = 0 */
    DSW(s1 + 0x74u) = 1u;
    CHECK_EQ_INT(sh_walk(0u, 1u, 1u, 0u, 0u), 0);
    DSW(s1 + 0x76u) = 1u;
    CHECK_EQ_INT(sh_walk(0u, 1u, 1u, 0u, 0u), 1);
    DSW(s1 + 0x76u) = 0;
    CHECK_EQ_INT(sh_walk(0u, 1u, 0u, 0u, 0u), 1);
    CHECK_EQ_INT(sh_walk(0u, 1u, 1u, 0u, 0u), 1);

    /* D: flag 0xF on the self slot's +0x43 bit 2. */
    sh_seed(s0, s1, r0, r1);
    DSB(s1 + 0x43u) = 4u;
    CHECK_EQ_INT(sh_walk(0u, 0xFu, 0u, 0u, 0u), 0);
    CHECK_EQ_INT(sh_walk(0u, 0xFu, 1u, 0u, 0u), 1);
    DSB(s0 + 0x43u) = 4u;
    CHECK_EQ_INT(sh_walk(0u, 0xFu, 0u, 0u, 0u), 1);
    CHECK_EQ_INT(sh_walk(0u, 0xFu, 1u, 0u, 0u), 0);

    /* E: flags 2/3/6/4 on the other slot's +0x54 == 0/1/7/2. */
    sh_seed(s0, s1, r0, r1);
    {
        static const u8 idx[4] = { 2u, 3u, 6u, 4u };
        static const u8 val[4] = { 0u, 1u, 7u, 2u };
        u32 k;
        for (k = 0; k < 4u; k++) {
            DSB(s1 + 0x54u) = (u8)(val[k] ^ 0x10u);
            DSB(s0 + 0x54u) = val[k];        /* the self slot is not read */
            CHECK_EQ_INT(sh_walk(0u, idx[k], 0u, 0u, 0u), 0);
            CHECK_EQ_INT(sh_walk(0u, idx[k], 1u, 0u, 0u), 1);
            DSB(s1 + 0x54u) = val[k];
            DSB(s0 + 0x54u) = (u8)(val[k] ^ 0x10u);
            CHECK_EQ_INT(sh_walk(0u, idx[k], 0u, 0u, 0u), 1);
            CHECK_EQ_INT(sh_walk(0u, idx[k], 1u, 0u, 0u), 0);
        }
    }

    /* F: flag 5, 0x1DDF4(ctx[1], box a, box b) with both slots latched from
     * their records (+0x42 bit 3): |x0 - x1| = 100 against box a[char 1]
     * << 6, |y0 - y1| against box b[char 1] << 6. */
    sh_seed(s0, s1, r0, r1);
    DSB(s0 + 0x42u) = 8u;
    DSB(s1 + 0x42u) = 8u;
    DSD(r0 + 0x18u) = 100u;
    DSB(s0 + 0x7Au) = 1u;                    /* self's char selects nothing */
    DSB(ba + 0) = 2u; DSB(ba + 1) = 1u;      /* 128 passes, 64 fails */
    DSB(bb + 0) = 0; DSB(bb + 1) = 0xFFu;
    CHECK_EQ_INT(sh_walk(0u, 5u, 0u, ba, bb), 1);
    CHECK_EQ_INT(sh_walk(0u, 5u, 1u, ba, bb), 0);
    DSB(ba + 0) = 1u;
    CHECK_EQ_INT(sh_walk(0u, 5u, 0u, ba, bb), 0);
    CHECK_EQ_INT(sh_walk(0u, 5u, 1u, ba, bb), 1);
    DSD(r0 + 0x18u) = 0;                     /* box b: |y| = 100 */
    DSD(r0 + 0x1Cu) = 100u;
    DSB(ba + 0) = 0xFFu;
    DSB(bb + 0) = 1u;
    CHECK_EQ_INT(sh_walk(0u, 5u, 1u, ba, bb), 1);
    DSB(bb + 0) = 2u;
    CHECK_EQ_INT(sh_walk(0u, 5u, 1u, ba, bb), 0);
    DSD(r0 + 0x1Cu) = 0;                     /* the defaults: 0xE7 << 6 */
    DSD(r0 + 0x18u) = 0xE7u << 6;
    CHECK_EQ_INT(sh_walk(0u, 5u, 1u, 0u, 0u), 0);
    DSD(r0 + 0x18u) = (0xE7u << 6) + 1u;
    CHECK_EQ_INT(sh_walk(0u, 5u, 1u, 0u, 0u), 1);
    DSD(r0 + 0x18u) = 0;
    DSD(r0 + 0x1Cu) = 0xE7u << 6;
    CHECK_EQ_INT(sh_walk(0u, 5u, 1u, 0u, 0u), 0);
    DSD(r0 + 0x1Cu) = (0xE7u << 6) + 1u;
    CHECK_EQ_INT(sh_walk(0u, 5u, 1u, 0u, 0u), 1);

    /* G: flag 7 on the other slot's +0x62; a firing check clears the self
     * slot's +0x8A and runs 0x18B44 (DS_00100C1D 0 -> 1 with +0x63 = 0). */
    sh_seed(s0, s1, r0, r1);
    CHECK_EQ_INT(sh_walk(0u, 7u, 0u, 0u, 0u), 0);
    CHECK_EQ_INT((int)DSB(s0 + 0x8Au), 0x77);
    CHECK_EQ_INT(sh_walk(0u, 7u, 1u, 0u, 0u), 1);
    CHECK_EQ_INT((int)DSB(s0 + 0x8Au), 0);
    CHECK_EQ_INT((int)DSB(s1 + 0x8Au), 0x78);
    DSB(s0 + 0x8Au) = 0x77u;
    DSB(s1 + 0x62u) = 1u;
    CHECK_EQ_INT(sh_walk(0u, 7u, 1u, 0u, 0u), 0);
    CHECK_EQ_INT((int)DSB(s0 + 0x8Au), 0x77);
    DSB(s0 + 0x63u) = 0;
    DSB(DS_00100C1D) = 0;
    CHECK_EQ_INT(sh_walk(0u, 7u, 0u, 0u, 0u), 1);
    CHECK_EQ_INT((int)DSB(s0 + 0x8Au), 0);
    CHECK_EQ_INT((int)DSB(DS_00100C1D), 1);

    /* H: flag 8 on the other slot's +0x42 bit 3. */
    sh_seed(s0, s1, r0, r1);
    DSB(s0 + 0x42u) = 8u;
    CHECK_EQ_INT(sh_walk(0u, 8u, 0u, 0u, 0u), 0);
    CHECK_EQ_INT(sh_walk(0u, 8u, 1u, 0u, 0u), 1);
    DSB(s1 + 0x42u) = 8u;
    CHECK_EQ_INT(sh_walk(0u, 8u, 0u, 0u, 0u), 1);
    CHECK_EQ_INT(sh_walk(0u, 8u, 1u, 0u, 0u), 0);

    /* I: flag 9, 0x189FC(ctx[1]): with side 0's actor bit 15 clear, 1 when
     * slot 1's +0x2C is below slot 0's; with it set, when above. */
    sh_seed(s0, s1, r0, r1);
    DSD(s0 + 0x2Cu) = 10u;
    DSD(s1 + 0x2Cu) = 5u;
    CHECK_EQ_INT(sh_walk(0u, 9u, 0u, 0u, 0u), 1);
    CHECK_EQ_INT(sh_walk(0u, 9u, 1u, 0u, 0u), 0);
    DSD(s1 + 0x2Cu) = 20u;
    CHECK_EQ_INT(sh_walk(0u, 9u, 0u, 0u, 0u), 0);
    CHECK_EQ_INT(sh_walk(0u, 9u, 1u, 0u, 0u), 1);
    DSW(a0) = 0x8F35u;
    CHECK_EQ_INT(sh_walk(0u, 9u, 0u, 0u, 0u), 1);
    DSD(s1 + 0x2Cu) = 0xFFFFFFF0u;           /* -16: a signed compare */
    CHECK_EQ_INT(sh_walk(0u, 9u, 0u, 0u, 0u), 0);

    /* J: flag 0xA, 0x18A4C(side): 0x189FC(1-side) and the two actors' bit 15
     * differ. */
    sh_seed(s0, s1, r0, r1);
    DSD(s0 + 0x2Cu) = 10u;
    DSD(s1 + 0x2Cu) = 5u;                    /* 0x189FC(1) = 1 (bit 15 clear) */
    CHECK_EQ_INT(sh_walk(0u, 0xAu, 0u, 0u, 0u), 0);             /* both clear */
    DSW(a1) = 0x8F35u;
    CHECK_EQ_INT(sh_walk(0u, 0xAu, 0u, 0u, 0u), 1);
    CHECK_EQ_INT(sh_walk(0u, 0xAu, 1u, 0u, 0u), 0);
    DSD(s1 + 0x2Cu) = 20u;                   /* 0x189FC(1) = 0 */
    CHECK_EQ_INT(sh_walk(0u, 0xAu, 0u, 0u, 0u), 0);
    DSW(a1) = 0x0F35u;
    DSW(a0) = 0x8F35u;                       /* 0x189FC(1): 20 > 10 -> 1 */
    CHECK_EQ_INT(sh_walk(0u, 0xAu, 0u, 0u, 0u), 1);
    DSW(a1) = 0x8F35u;                       /* both set */
    CHECK_EQ_INT(sh_walk(0u, 0xAu, 0u, 0u, 0u), 0);

    /* K: flag 0xB on the self record's +0x61. */
    sh_seed(s0, s1, r0, r1);
    DSB(r1 + 0x61u) = 1u;
    CHECK_EQ_INT(sh_walk(0u, 0xBu, 0u, 0u, 0u), 0);
    CHECK_EQ_INT(sh_walk(0u, 0xBu, 1u, 0u, 0u), 1);
    DSB(r0 + 0x61u) = 1u;
    CHECK_EQ_INT(sh_walk(0u, 0xBu, 0u, 0u, 0u), 1);
    CHECK_EQ_INT(sh_walk(0u, 0xBu, 1u, 0u, 0u), 0);

    /* L: flag 0xD, 0x39EFC(ctx[1]) (the other slot in the 0x39CC8 pose),
     * clearing +0x8A; flag 0xC on the other slot's +0x53 == 0x0A. */
    sh_seed(s0, s1, r0, r1);
    CHECK_EQ_INT(sh_walk(0u, 0xDu, 0u, 0u, 0u), 0);
    CHECK_EQ_INT((int)DSB(s0 + 0x8Au), 0x77);
    CHECK_EQ_INT(sh_walk(0u, 0xDu, 1u, 0u, 0u), 1);
    CHECK_EQ_INT((int)DSB(s0 + 0x8Au), 0);
    CHECK_EQ_INT(sh_walk(0u, 0xCu, 0u, 0u, 0u), 0);
    CHECK_EQ_INT(sh_walk(0u, 0xCu, 1u, 0u, 0u), 1);
    DSB(s0 + 0x8Au) = 0x77u;
    DSB(s1 + 0x53u) = 0x0Au;
    DSD(s1 + 0x10u) = 0x00039CC8u;
    DSB(s1 + 0x58u) = 4u;
    CHECK_EQ_INT(sh_walk(0u, 0xDu, 1u, 0u, 0u), 0);
    CHECK_EQ_INT((int)DSB(s0 + 0x8Au), 0x77);
    CHECK_EQ_INT(sh_walk(0u, 0xDu, 0u, 0u, 0u), 1);
    CHECK_EQ_INT((int)DSB(s0 + 0x8Au), 0);
    CHECK_EQ_INT(sh_walk(0u, 0xCu, 0u, 0u, 0u), 1);
    CHECK_EQ_INT(sh_walk(0u, 0xCu, 1u, 0u, 0u), 0);
    /* the order: 0xD before 0xC (the +0x8A write shows which fired). */
    DSB(s0 + 0x8Au) = 0x77u;
    for (i = 0; i < 16u; i++) flags[i] = 2u;
    flags[0xC] = 0; flags[0xD] = 0;
    CHECK_EQ_INT(fighter_18c14(0u, flags, 0u, 0u), 1);
    CHECK_EQ_INT((int)DSB(s0 + 0x8Au), 0);

    /* M: flag 0xE, 0x3B298(ctx[1], self +0x5F) first held at 0 (the other
     * slot's +0x63 = 0 makes 0x3B149's map return, 0xA6728's word +2 = 3
     * returns 0). The call is seen through its 0x3B2D6 copy: other +0x86 =
     * self +0x84. M2 below makes it return 1. */
    sh_seed(s0, s1, r0, r1);
    DSB(s1 + 0x63u) = 0;
    DSW(DS_000A6728 + 2u) = 3u;
    DSW(s1 + 0x86u) = 0x4321u;
    CHECK_EQ_INT(sh_walk(0u, 0xEu, 2u, 0u, 0u), 0);
    CHECK_EQ_INT((int)DSW(s1 + 0x86u), 0x4321);
    CHECK_EQ_INT(sh_walk(0u, 0xEu, 0u, 0u, 0u), 0);
    CHECK_EQ_INT((int)DSW(s1 + 0x86u), 0x1234);
    CHECK_EQ_INT((int)DSB(s0 + 0x8Au), 0x77);
    DSW(s1 + 0x86u) = 0x4321u;
    CHECK_EQ_INT(sh_walk(0u, 0xEu, 3u, 0u, 0u), 0);
    CHECK_EQ_INT((int)DSW(s1 + 0x86u), 0x1234);
    CHECK_EQ_INT(sh_walk(0u, 0xEu, 1u, 0u, 0u), 1);
    CHECK_EQ_INT((int)DSB(s0 + 0x8Au), 0);
    /* M2: 0x3B298 returns 1 (as check_think_chain's A2): 0xA6728's word +2 =
     * 0 lets the scan run, the ring is empty and side 1's command word 0x1000
     * overlaps 0x1AB5C's facing base 0x3000 (both +0x2C are 0), so 0x3B40D
     * sets the other slot's +0x43 bit 5 and it returns 1. Flag 0 then fires
     * with +0x8A = 0 (0x18FC9), flag 1 fires the same way, and 3 calls but
     * falls through. */
    DSW(DS_000A6728 + 2u) = 0;
    mem_fill(DS_00108270, 0, 0x50u);
    DSW(DS_001088E0) = 0;
    DSW(DS_001088E2) = 0x1000u;
    DSB(s0 + 0x8Au) = 0x77u;
    CHECK_EQ_INT(sh_walk(0u, 0xEu, 2u, 0u, 0u), 0);
    CHECK_EQ_INT((int)(DSB(s1 + 0x43u) & 0x30u), 0);
    CHECK_EQ_INT(sh_walk(0u, 0xEu, 0u, 0u, 0u), 1);
    CHECK_EQ_INT((int)(DSB(s1 + 0x43u) & 0x30u), 0x20);
    CHECK_EQ_INT((int)DSB(s0 + 0x8Au), 0);
    DSB(s0 + 0x8Au) = 0x77u;
    DSB(s1 + 0x43u) = 0;
    CHECK_EQ_INT(sh_walk(0u, 0xEu, 3u, 0u, 0u), 0);
    CHECK_EQ_INT((int)(DSB(s1 + 0x43u) & 0x30u), 0x20);
    CHECK_EQ_INT((int)DSB(s0 + 0x8Au), 0x77);
    CHECK_EQ_INT(sh_walk(0u, 0xEu, 1u, 0u, 0u), 1);
    CHECK_EQ_INT((int)DSB(s0 + 0x8Au), 0);
    DSW(DS_001088E2) = 0;
    DSW(DS_000A6728 + 2u) = 3u;
    DSB(s1 + 0x43u) = 0;
    /* the order: 0xE before 4, 7 before 8, 5 before 7. */
    DSB(s0 + 0x8Au) = 0x77u;
    DSB(s1 + 0x54u) = 2u;
    for (i = 0; i < 16u; i++) flags[i] = 2u;
    flags[4] = 0; flags[0xE] = 1u;
    CHECK_EQ_INT(fighter_18c14(0u, flags, 0u, 0u), 1);
    CHECK_EQ_INT((int)DSB(s0 + 0x8Au), 0);
    DSB(s0 + 0x8Au) = 0x77u;
    DSB(s1 + 0x62u) = 1u;
    DSB(s1 + 0x42u) = 8u;
    for (i = 0; i < 16u; i++) flags[i] = 2u;
    flags[7] = 0; flags[8] = 0;
    CHECK_EQ_INT(fighter_18c14(0u, flags, 0u, 0u), 1);
    CHECK_EQ_INT((int)DSB(s0 + 0x8Au), 0);
    DSB(s0 + 0x8Au) = 0x77u;
    DSB(s0 + 0x42u) = 8u;
    DSD(r0 + 0x18u) = 100u;
    DSB(ba + 0) = 1u; DSB(bb + 0) = 0;       /* geometry fails */
    for (i = 0; i < 16u; i++) flags[i] = 2u;
    flags[5] = 1u; flags[7] = 0;
    CHECK_EQ_INT(fighter_18c14(0u, flags, ba, bb), 1);
    CHECK_EQ_INT((int)DSB(s0 + 0x8Au), 0x77);

    /* N: 0x3E484 (flags 0 = 1, 1 = 0, 8 = 0): the demo's f = 106..113 (AF8 0
     * -> 1) and f = 114 (AF8 6, the other slot quiet -> 0); flags 2..7 stay
     * skipped (the other slot's +0x54 = 2 and +0x62 = 1 do not fire). */
    sh_seed(s0, s1, r0, r1);
    DSB(s1 + 0x54u) = 2u;
    DSB(s1 + 0x62u) = 1u;
    DSB(s1 + 0x53u) = 0x0Au;
    DSD(DS_00100AF8) = 0;
    CHECK_EQ_INT((int)fighter_3e484(0u), 1);
    DSD(DS_00100AF8) = 6u;
    CHECK_EQ_INT((int)fighter_3e484(0u), 0);
    CHECK_EQ_INT((int)DSB(s0 + 0x8Au), 0x77);
    DSW(s1 + 0x76u) = 2u;
    CHECK_EQ_INT((int)fighter_3e484(0u), 1);
    DSW(s1 + 0x76u) = 1u;
    CHECK_EQ_INT((int)fighter_3e484(0u), 0);
    DSW(s1 + 0x74u) = 1u;
    CHECK_EQ_INT((int)fighter_3e484(0u), 1);
    DSW(s1 + 0x74u) = 0;
    DSB(s1 + 0x42u) = 8u;
    CHECK_EQ_INT((int)fighter_3e484(0u), 1);
    DSB(s1 + 0x42u) = 0;
    DSD(DS_00100AFC) = 0;                    /* side 1 reads AF8[1] */
    CHECK_EQ_INT((int)fighter_3e484(1u), 1);
    DSD(DS_00100AFC) = 9u;
    DSW(s0 + 0x74u) = 1u;                    /* side 1's other is slot 0 */
    CHECK_EQ_INT((int)fighter_3e484(1u), 1);
    DSW(s0 + 0x74u) = 0;
    CHECK_EQ_INT((int)fighter_3e484(1u), 0);

    /* O: 0x3E1D0: +0x86 >> 16 outside 1..3 returns 1 before 0x18C14; inside,
     * flag 5 on the 0xC75F5/0xC75FF boxes, then flag 7 (the other slot's
     * +0x62 = 1) clears +0x8A. |x| = 7041 fails 0xC75F5 (0x6E << 6 = 7040)
     * but passes the default 0xE7 << 6; |y| = 6337 fails 0xC75FF (0x63 << 6)
     * but passes 0xC75F5's 7040. */
    sh_seed(s0, s1, r0, r1);
    DSB(s0 + 0x42u) = 8u;
    DSB(s1 + 0x42u) = 8u;
    DSB(s1 + 0x62u) = 1u;
    {
        static const u32 v86[6] = { 0x0000FFFFu, 0x00010000u, 0x00030000u,
                                    0x00040000u, 0xFFFF0000u, 0x0002FFFFu };
        static const int in[6] = { 0, 1, 1, 0, 0, 1 };
        u32 k;
        for (k = 0; k < 6u; k++) {
            DSB(s0 + 0x8Au) = 0x77u;
            DSD(s0 + 0x86u) = v86[k];
            CHECK_EQ_INT((int)fighter_3e1d0(0u), 1);
            CHECK_EQ_INT((int)DSB(s0 + 0x8Au), in[k] ? 0 : 0x77);
        }
    }
    DSD(s0 + 0x86u) = 0x00020000u;
    DSB(s0 + 0x8Au) = 0x77u;
    DSD(r0 + 0x18u) = 7040u;
    CHECK_EQ_INT((int)fighter_3e1d0(0u), 1);
    CHECK_EQ_INT((int)DSB(s0 + 0x8Au), 0);
    DSB(s0 + 0x8Au) = 0x77u;
    DSD(r0 + 0x18u) = 7041u;
    CHECK_EQ_INT((int)fighter_3e1d0(0u), 1);
    CHECK_EQ_INT((int)DSB(s0 + 0x8Au), 0x77);
    DSD(r0 + 0x18u) = 0;
    DSD(r0 + 0x1Cu) = 6336u;
    CHECK_EQ_INT((int)fighter_3e1d0(0u), 1);
    CHECK_EQ_INT((int)DSB(s0 + 0x8Au), 0);
    DSB(s0 + 0x8Au) = 0x77u;
    DSD(r0 + 0x1Cu) = 6337u;
    CHECK_EQ_INT((int)fighter_3e1d0(0u), 1);
    CHECK_EQ_INT((int)DSB(s0 + 0x8Au), 0x77);
    /* the flags it sets: 1 = 0 (other +0x74) fires before 5. */
    DSD(r0 + 0x1Cu) = 0;
    DSW(s1 + 0x74u) = 1u;
    CHECK_EQ_INT((int)fighter_3e1d0(0u), 1);
    CHECK_EQ_INT((int)DSB(s0 + 0x8Au), 0x77);
    /* past flag 7 (other +0x62 = 0): 8 = 0, 0xD = 0, 0xE = 0 and 4 = 0. The
     * slots latch with +0x42 bit 3 clear: no DS_001077A8 record (0x18540
     * returns), +0x20 = the DS_00100AF0 anchor (no 0x18350), and a zero
     * DS_00100AB0/AB4 offset, so +0x2C/+0x30 = the records' +0x18/+0x1C = 0.
     * 0x3B298 is held at 0 as in M; its 0x3B2D6 copy (other +0x86 = self
     * +0x84) shows whether flag 0xE ran. */
    DSW(s1 + 0x74u) = 0;
    DSB(s1 + 0x62u) = 0;
    DSB(s0 + 0x42u) = 0;
    DSB(s1 + 0x42u) = 8u;
    DSD(DS_001077A8) = 0;
    DSD(DS_001077A8 + 4u) = 0;
    for (i = 0; i < 2u; i++) {
        DSD(DS_001077B0 + i * 0x94u + 0x20u) = DSD(0x00100AF0u + i * 4u);
        DSD(DS_00100AB0 + i * 8u) = 0;
        DSD(DS_00100AB4 + i * 8u) = 0;
    }
    DSB(s1 + 0x63u) = 0;
    DSW(DS_000A6728 + 2u) = 3u;
    DSW(s1 + 0x86u) = 0x4321u;
    DSB(s0 + 0x8Au) = 0x77u;
    CHECK_EQ_INT((int)fighter_3e1d0(0u), 1);                    /* flag 8 */
    CHECK_EQ_INT((int)DSW(s1 + 0x86u), 0x4321);
    DSB(s1 + 0x42u) = 0;
    DSB(s1 + 0x53u) = 0x0Au;                 /* 0x39EFC(1) = 1: flag 0xD */
    DSD(s1 + 0x10u) = 0x00039CC8u;
    DSB(s1 + 0x58u) = 4u;
    CHECK_EQ_INT((int)fighter_3e1d0(0u), 1);
    CHECK_EQ_INT((int)DSB(s0 + 0x8Au), 0);
    CHECK_EQ_INT((int)DSW(s1 + 0x86u), 0x4321);
    DSB(s0 + 0x8Au) = 0x77u;
    DSB(s1 + 0x58u) = 0;                     /* 0x39EFC(1) = 0 */
    CHECK_EQ_INT((int)fighter_3e1d0(0u), 0);                    /* all pass */
    CHECK_EQ_INT((int)DSW(s1 + 0x86u), 0x1234);                 /* 0xE ran */
    CHECK_EQ_INT((int)DSB(s0 + 0x8Au), 0x77);
    DSB(s1 + 0x54u) = 2u;                    /* flag 4 */
    CHECK_EQ_INT((int)fighter_3e1d0(0u), 1);
    DSB(s1 + 0x54u) = 0;

    /* P: 0x19020: +0x18 = 0 or an unregistered hook leaves AF8[side]; a
     * registered hook stores (result == 0). */
    sh_seed(s0, s1, r0, r1);
    DSD(DS_00100AF8) = 0x5Au;
    DSD(DS_00100AFC) = 0x5Bu;
    fighter_19020(0u);
    CHECK_EQ_INT((int)DSD(DS_00100AF8), 0x5A);
    DSD(s0 + 0x18u) = 0x0003D484u;
    fighter_19020(0u);
    CHECK_EQ_INT((int)DSD(DS_00100AF8), 0x5A);
    DSD(s0 + 0x18u) = 0x0003E484u;           /* AF8 0x5A > 0, quiet: 0 */
    fighter_19020(0u);
    CHECK_EQ_INT((int)DSD(DS_00100AF8), 1);
    CHECK_EQ_INT((int)DSD(DS_00100AFC), 0x5B);
    DSW(s1 + 0x74u) = 1u;                    /* 1 -> AF8 = 0 */
    fighter_19020(0u);
    CHECK_EQ_INT((int)DSD(DS_00100AF8), 0);
    DSW(s1 + 0x74u) = 0;
    DSD(DS_00100AF8) = 0x5Au;
    DSD(s0 + 0x18u) = 0;
    DSD(s1 + 0x18u) = 0x0003E484u;
    fighter_19020(1u);
    CHECK_EQ_INT((int)DSD(DS_00100AFC), 1);
    CHECK_EQ_INT((int)DSD(DS_00100AF8), 0x5A);

    /* Q: through 0x1958C's 0x195B6: AF8[0] = 0 with the 0x3E484 hook held
     * (the demo's f = 106) stays 0, and the hook's 1 at AF8 = 6 (f = 114) is
     * what the pass then sees. Both slots' +0x5F >= 0x40 keep the 0x195CC
     * block off; side 1 has no hook. */
    sh_seed(s0, s1, r0, r1);
    DSB(DS_001078FA) = 2u;
    DSB(s0 + 0x5Fu) = 0x40u;
    DSB(s1 + 0x5Fu) = 0x40u;
    DSD(s0 + 0x18u) = 0x0003E484u;
    DSD(DS_00100AF8) = 6u;
    DSD(DS_00100AFC) = 0;                    /* the 0x19632 skip to the tail */
    DSB(s0 + 0x8Au) = 0;                     /* the tail runs no 0x193B0 */
    DSB(s1 + 0x8Au) = 0;
    fighter_pass_a();
    CHECK_EQ_INT((int)DSD(DS_00100AF8), 1);
    DSD(DS_00100AF8) = 0;
    fighter_pass_a();
    CHECK_EQ_INT((int)DSD(DS_00100AF8), 0);

    tf_put(sv_8e0, DS_001088E0, 4u);
    tf_put(sv_ring, DS_00108270, 0x50u);
    DSB(DS_001078FA) = sv_78fa;
    tf_put(sv_ab0, DS_00100AB0, 16u);
    tf_put(sv_7a8, DS_001077A8, 8u);
    DSB(DS_00100C1D) = sv_c1d;
    DSW(DS_000A6728 + 2u) = sv_w2;
    tf_put(sv_af8, DS_00100AF8, 8u);
    DSD(DS_001014EC) = sv_14ec;
    DSW(0x001080ACu) = sv_ac;
    DSW(0x001080AEu) = sv_ae;
    tf_put(sv_b00, DS_00104B00, 4u);
}

/* ---- roar-timing Task 30: character 3's grab 0x14E44 (record §40) -------- */

/* The grab stream's head 0xD3026 and the miss stream's head 0xD3062. */
#define GR_ANIM_A 0x000D3026u
#define GR_ANIM_B 0x000D3062u

/* sh_seed for 0x14CC4 on side 0: every check its flags name passes (the other
 * slot quiet, 0x39EFC(1) = 0, 0x3B298 held at 0 as in check_slot_hook's M/O,
 * its 0x3B2D6 copy marked by the other slot's +0x86 = 0x4321); the slots latch
 * with +0x42 bit 3 clear and no DS_001077A8 record, so 0x187FC's latch gives
 * +0x2C = the records' +0x18 (x0, x1), which the slots also hold for flag 9's
 * 0x189FC; the self record's +0x61 = 1, the other's 0x5A. */
static void gr_seed(u32 s0, u32 s1, u32 r0, u32 r1, s32 x0, s32 x1)
{
    u32 i;
    sh_seed(s0, s1, r0, r1);
    DSB(s1 + 0x63u) = 0;
    DSW(DS_000A6728 + 2u) = 3u;
    DSW(s1 + 0x86u) = 0x4321u;
    DSD(DS_001077A8) = 0;
    DSD(DS_001077A8 + 4u) = 0;
    for (i = 0; i < 2u; i++) {
        DSD(DS_001077B0 + i * 0x94u + 0x20u) = DSD(0x00100AF0u + i * 4u);
        DSD(DS_00100AB0 + i * 8u) = 0;
        DSD(DS_00100AB4 + i * 8u) = 0;
    }
    DSD(r0 + 0x18u) = (u32)x0;
    DSD(r1 + 0x18u) = (u32)x1;
    DSD(s0 + 0x2Cu) = (u32)x0;
    DSD(s1 + 0x2Cu) = (u32)x1;
    DSB(r0 + 0x61u) = 1u;
    DSB(r1 + 0x61u) = 0x5Au;
}

/* Record §40. 0x14E44 (*(u32*)0xA46E4 reads `44 4e 01 00 00 00 00 00`:
 * character 3's reaction 0x23, no stream; Ghidra has no function there):
 * EAX = slot, EDX = rec, EBX unread; 0xD3026 at hold 2.0 through 0x3C4CC,
 * state 9/7/0, +0x18 = 0x14CC4, +0x1C = 0x14D7C, +0x42 |= 4; +0x0C and +0x57
 * untouched. 0x14CC4 (the +0x18 hook, EAX = side, 0x33950 context): 1 while
 * the self record's +0x61 is 0; else 0x18C14 with flags 1/4/8/0xD/0xE = 0
 * and 9 = 1, then |0x187FC()| in 0x1900..0x3200 gives 0; a 1 restarts the
 * record on 0xD3062 at 3.0 (0x2BC30); +0x61 = 0. 0x14EA4 (the 0xD100 target
 * at 0xD3036): the other side's record x += (s16)(word[0x9AFA8 + 2 * (s8)
 * rec+0x52] << 6), subtracted when 0x1A570(side) is 0. The two stream heads
 * are patched to plain frame words and restored. */
static void check_char3_grab(void)
{
    u32 s0 = DS_001077B0, s1 = DS_001077B0 + 0x94u;
    u32 r0 = FIGHT_RECS, r1 = FIGHT_RECS + 0x100u;
    u32 st = FIGHT_RECS + 0x3A00u;
    u32 pset1 = FIGHT_ACTORS + 0x20u;
    u32 a0 = FIGHT_ACTORS + 0x20u, a1 = FIGHT_ACTORS + 0x40u;
    u32 sv_14ec = DSD(DS_001014EC);
    u16 sv_ga = DSW(GR_ANIM_A), sv_gb = DSW(GR_ANIM_B);
    u16 sv_st[5];
    u16 sv_ac = DSW(0x001080ACu), sv_ae = DSW(0x001080AEu);
    u16 sv_w2 = DSW(DS_000A6728 + 2u);
    u8 sv_slots[0x128], sv_7a8[8], sv_b00[4], sv_tab[28], sv_a80[0x80];
    u8 sv_d2c[4], sv_fd[0x18], sv_f8[2], sv_af8[8], sv_ab0[16], sv_e0[4];
    u8 sv_c1d = DSB(DS_00100C1D);
    u8 sv_8a8[2];
    u32 i;
    typedef void (*anim_fn)(u32 rec, u32 arg);
    anim_fn fea4;

    for (i = 0; i < 5u; i++) sv_st[i] = DSW(c3_streams[i]);
    tf_snap(sv_slots, DS_001077B0, 0x128u);
    tf_snap(sv_7a8, DS_001077A8, 8u);
    tf_snap(sv_b00, DS_00104B00, 4u);
    tf_snap(sv_tab, 0x000C8950u, 28u);
    tf_snap(sv_a80, DS_00107A80, 0x80u);
    tf_snap(sv_d2c, DS_00107D2C, 4u);
    tf_snap(sv_fd, FIGHT_FD108_T, 0x18u);
    tf_snap(sv_f8, DS_001078F8, 2u);
    tf_snap(sv_af8, DS_00100AF8, 8u);
    tf_snap(sv_ab0, DS_00100AB0, 16u);
    tf_snap(sv_e0, DS_001088E0, 4u);
    tf_snap(sv_8a8, DS_001088A8, 2u);

    CHECK(fn_resolve(0x14E44u) == (void (*)(void))fighter_14e44,
          "0x14E44 is registered as fighter_14e44");
    CHECK(fn_resolve(0x14CC4u) == (void (*)(void))fighter_14cc4,
          "0x14CC4 is registered as fighter_14cc4");
    fea4 = (anim_fn)(void *)fn_resolve(0x14EA4u);
    CHECK(fea4 != NULL, "0x14EA4 is a registered stream target");
    CHECK(fn_resolve(0x14D7Cu) == (void (*)(void))fighter_14d7c,
          "0x14D7C is registered as fighter_14d7c");
    CHECK(fn_resolve(0x14E80u) != NULL, "0x14E80 is a registered stream target");

    /* A: through 0x34E2C (0x35045) for character 3, reaction 0x23. */
    c3_seed(s0, s1, r0, r1, st);
    DSW(GR_ANIM_A) = 0x1310u;
    DSW(DS_001088E0) = 0;
    DSB(s0 + 0x42u) = 0x09u;
    hit_reaction_apply(0u, 0x23u);
    CHECK_EQ_INT((int)DSB(s0 + 0x5Fu), 0x23);
    CHECK_EQ_INT((int)DSD(r0 + 8u), (int)GR_ANIM_A);
    CHECK_EQ_INT((int)DSD(r0 + 0x24u), 0x40000000);
    CHECK_EQ_INT((int)(DSW(pset1) & 0x7FFFu), 0x1310);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 9);
    CHECK_EQ_INT((int)DSB(s0 + 0x53u), 7);
    CHECK_EQ_INT((int)DSB(s0 + 0x54u), 0);
    CHECK_EQ_INT((int)DSD(s0 + 0x18u), 0x00014CC4);
    CHECK_EQ_INT((int)DSD(s0 + 0x1Cu), 0x00014D7C);
    CHECK_EQ_INT((int)DSB(s0 + 0x42u), 0x0D);
    CHECK_EQ_INT((int)DSD(s0 + 0x0Cu), 0x11111111);
    CHECK_EQ_INT((int)DSB(s0 + 0x57u), 0x99);
    CHECK_EQ_INT((int)DSD(s1 + 0x18u), 0x22222222);
    CHECK_EQ_INT((int)DSD(s1 + 0x1Cu), 0x33333333);
    CHECK_EQ_INT((int)DSD(r1 + 8u), 0x00ABCDEF);
    /* A2: direct, EBX = 1 unread; +0x42 bit 2 already set stays set. */
    c3_seed(s0, s1, r0, r1, st);
    DSW(GR_ANIM_A) = 0x1311u;
    DSB(s0 + 0x42u) = 0x04u;
    fighter_14e44(s0, r0, 1u);
    CHECK_EQ_INT((int)DSD(r0 + 8u), (int)GR_ANIM_A);
    CHECK_EQ_INT((int)(DSW(pset1) & 0x7FFFu), 0x1311);
    CHECK_EQ_INT((int)DSB(s0 + 0x42u), 0x04);
    CHECK_EQ_INT((int)DSD(s0 + 0x18u), 0x00014CC4);
    CHECK_EQ_INT((int)DSD(s1 + 0x18u), 0x22222222);
    CHECK_EQ_INT((int)DSD(r1 + 8u), 0x00ABCDEF);

    /* B: 0x14CC4 with the self record's +0x61 = 0 returns 1 before 0x18C14
     * (the 0x4321 mark stays) and starts nothing; the other record's +0x61
     * is not the gate. */
    DSW(GR_ANIM_B) = 0x1312u;
    gr_seed(s0, s1, r0, r1, 0x3000, 0x1000);
    DSB(r0 + 0x61u) = 0;
    CHECK_EQ_INT((int)fighter_14cc4(0u), 1);
    CHECK_EQ_INT((int)DSW(s1 + 0x86u), 0x4321);
    CHECK_EQ_INT((int)DSD(r0 + 8u), 0x00ABCDEF);
    CHECK_EQ_INT((int)DSB(r0 + 0x61u), 0);
    CHECK_EQ_INT((int)DSB(r1 + 0x61u), 0x5A);
    /* B2: +0x61 = 1, every check passes, |d| = 0x2000: 0; flag 0xE ran (the
     * copy 0x1234), +0x61 cleared, no stream, the other record untouched. */
    gr_seed(s0, s1, r0, r1, 0x3000, 0x1000);
    CHECK_EQ_INT((int)fighter_14cc4(0u), 0);
    CHECK_EQ_INT((int)DSW(s1 + 0x86u), 0x1234);
    CHECK_EQ_INT((int)DSB(r0 + 0x61u), 0);
    CHECK_EQ_INT((int)DSB(r1 + 0x61u), 0x5A);
    CHECK_EQ_INT((int)DSD(r0 + 8u), 0x00ABCDEF);
    CHECK_EQ_INT((int)DSD(r0 + 0x24u), 0x11111111);
    CHECK_EQ_INT((int)DSB(s0 + 0x8Au), 0x77);
    /* B3: the range's edges, signed. 0x3200 and 0x1900 are in; 0x3201 and
     * 0x18FF miss: 0xD3062 at hold 3.0. */
    {
        static const s32 d[4] = { 0x3200, 0x3201, 0x1900, 0x18FF };
        static const int miss[4] = { 0, 1, 0, 1 };
        u32 k;
        for (k = 0; k < 4u; k++) {
            gr_seed(s0, s1, r0, r1, 0x1000 + d[k], 0x1000);
            CHECK_EQ_INT((int)fighter_14cc4(0u), miss[k]);
            CHECK_EQ_INT((int)DSD(r0 + 8u),
                         miss[k] ? (int)GR_ANIM_B : 0x00ABCDEF);
            CHECK_EQ_INT((int)DSD(r0 + 0x24u),
                         miss[k] ? 0x40400000 : 0x11111111);
            CHECK_EQ_INT((int)DSB(r0 + 0x61u), 0);
        }
        CHECK_EQ_INT((int)(DSW(pset1) & 0x7FFFu), 0x1312);
    }
    /* B4: a negative distance (x0 below x1; actor 0's bit 15 set keeps flag
     * 9's 0x189FC(1) at 1): |d| is what counts. */
    gr_seed(s0, s1, r0, r1, 0x1000, 0x3000);
    DSW(a0) = 0x8F35u;
    CHECK_EQ_INT((int)fighter_14cc4(0u), 0);
    gr_seed(s0, s1, r0, r1, 0x1000, 0x1000 + 0x3201);
    DSW(a0) = 0x8F35u;
    CHECK_EQ_INT((int)fighter_14cc4(0u), 1);
    CHECK_EQ_INT((int)DSD(r0 + 8u), (int)GR_ANIM_B);
    /* B5: each flag it sets fires, with |d| = 0x2000 in range: 1 (+0x74),
     * 4 (+0x54 = 2), 8 (+0x42 bit 3), 9 (x1 above x0, bit 15 clear), 0xD
     * (0x39EFC(1): +0x8A cleared); each gives 1 and the miss stream. Flag
     * 0xE = 0 is B2's pass (as 1, 0x3B298's 0 would fire). */
    {
        u32 k;
        for (k = 0; k < 5u; k++) {
            gr_seed(s0, s1, r0, r1, 0x3000, 0x1000);
            if (k == 0u) DSW(s1 + 0x74u) = 1u;
            if (k == 1u) DSB(s1 + 0x54u) = 2u;
            if (k == 2u) DSB(s1 + 0x42u) = 8u;
            if (k == 3u) {
                DSD(s0 + 0x2Cu) = 0x1000u;
                DSD(r0 + 0x18u) = 0x1000u;
                DSD(s1 + 0x2Cu) = 0x3000u;
                DSD(r1 + 0x18u) = 0x3000u;
            }
            if (k == 4u) {
                DSB(s1 + 0x53u) = 0x0Au;
                DSD(s1 + 0x10u) = 0x00039CC8u;
                DSB(s1 + 0x58u) = 4u;
            }
            CHECK_EQ_INT((int)fighter_14cc4(0u), 1);
            CHECK_EQ_INT((int)DSD(r0 + 8u), (int)GR_ANIM_B);
            CHECK_EQ_INT((int)DSB(r0 + 0x61u), 0);
            /* flag 4 is checked after 0xE, which has run by then */
            CHECK_EQ_INT((int)DSW(s1 + 0x86u), k == 1u ? 0x1234 : 0x4321);
        }
        CHECK_EQ_INT((int)DSB(s0 + 0x8Au), 0);
    }
    /* B6: through 0x19020: AF8[0] = (hook == 0). */
    gr_seed(s0, s1, r0, r1, 0x3000, 0x1000);
    DSD(s0 + 0x18u) = 0x00014CC4u;
    DSD(DS_00100AF8) = 0x5Au;
    fighter_19020(0u);
    CHECK_EQ_INT((int)DSD(DS_00100AF8), 1);
    DSB(r0 + 0x61u) = 0;
    fighter_19020(0u);
    CHECK_EQ_INT((int)DSD(DS_00100AF8), 0);

    /* C: 0x14EA4 as anim_indirect calls it, (rec, operand). The word table
     * at 0x9AFA8 reads 2D00 3840 2F80 2D00 29C0 (read_memory); << 6 keeps 16
     * bits: index 1 gives 0x1000 (not 0xE1000), index 2 gives -0x2000. */
    CHECK_EQ_INT((int)DSW(0x0009AFAAu), 0x3840);
    CHECK_EQ_INT((int)DSW(0x0009AFACu), 0x2F80);
    CHECK_EQ_INT((int)DSW(0x0009AFA6u), 0x2D00);
    if (fea4 != NULL) {
        tb_seed(s0, s1, r0, r1);
        DSD(DS_001014EC) = FIGHT_ACTORS;
        DSD(r0 + 0x18u) = 0x10000u;
        DSD(r1 + 0x18u) = 0x20000u;
        DSB(r0 + 0x52u) = 1u;
        fea4(r0, 0xFFFFu);                      /* side 0 moves side 1 */
        CHECK_EQ_INT((int)DSD(r1 + 0x18u), 0x21000);
        CHECK_EQ_INT((int)DSD(r0 + 0x18u), 0x10000);
        DSB(r0 + 0x52u) = 2u;
        fea4(r0, 0u);
        CHECK_EQ_INT((int)DSD(r1 + 0x18u), 0x1F000);
        DSW(a0) = 0x8F35u;                      /* 0x1A570(0) = 0: subtract */
        fea4(r0, 0u);
        CHECK_EQ_INT((int)DSD(r1 + 0x18u), 0x21000);
        DSB(r0 + 0x52u) = 0xFFu;                /* (s8) -1: word 0x9AFA6 */
        fea4(r0, 0u);
        CHECK_EQ_INT((int)DSD(r1 + 0x18u), 0x21000 - 0x4000);
        /* side 1 moves side 0 on actor 1's bit */
        DSB(r1 + 0x52u) = 1u;
        fea4(r1, 0u);
        CHECK_EQ_INT((int)DSD(r0 + 0x18u), 0x11000);
        DSW(a1) = 0x8F35u;
        fea4(r1, 0u);
        CHECK_EQ_INT((int)DSD(r0 + 0x18u), 0x10000);
        /* no other slot: it returns before any dereference (0x14EBB), so
         * a pointer planted at mem[0] is not followed */
        {
            u32 sv0 = DSD(0u);
            DSD(DS_001077A8) = 0;
            DSD(0u) = r0;
            fea4(r1, 0u);
            CHECK_EQ_INT((int)DSD(r0 + 0x18u), 0x10000);
            DSD(0u) = sv0;
        }
    }

    tf_put(sv_8a8, DS_001088A8, 2u);
    tf_put(sv_e0, DS_001088E0, 4u);
    tf_put(sv_ab0, DS_00100AB0, 16u);
    tf_put(sv_af8, DS_00100AF8, 8u);
    tf_put(sv_f8, DS_001078F8, 2u);
    tf_put(sv_fd, FIGHT_FD108_T, 0x18u);
    tf_put(sv_d2c, DS_00107D2C, 4u);
    tf_put(sv_a80, DS_00107A80, 0x80u);
    tf_put(sv_tab, 0x000C8950u, 28u);
    tf_put(sv_b00, DS_00104B00, 4u);
    tf_put(sv_7a8, DS_001077A8, 8u);
    tf_put(sv_slots, DS_001077B0, 0x128u);
    DSB(DS_00100C1D) = sv_c1d;
    DSW(DS_000A6728 + 2u) = sv_w2;
    DSW(0x001080ACu) = sv_ac;
    DSW(0x001080AEu) = sv_ae;
    for (i = 0; i < 5u; i++) DSW(c3_streams[i]) = sv_st[i];
    DSW(GR_ANIM_B) = sv_gb;
    DSW(GR_ANIM_A) = sv_ga;
    DSD(DS_001014EC) = sv_14ec;
}

/* Record §41. 0x3C32C (the 0xD500 target of 9 stream sites, the first at
 * 0xD24FE in character 3's reaction stream 0xD24F0; Ghidra has no function
 * there): EAX = rec, the operand unread; the byte at 0x107804 + (rec+0x51) *
 * 0x94 (the side's slot +0x54) = 0, then 0x36870(rec). Both slots are seeded
 * with +0x54 = 3: 0x36870 still runs its resets before the switch, but its
 * case-3 arm does nothing, so only the clear reaches the +0x54 == 0 arm: the
 * side's record restarts on 0xC8950[+0x7A] with +0x4D = 0x1E and state 0/0
 * (check_deep_callees E's setting: mode 3, +0x42/+0x43 = 0). Both words of
 * DS_00107D2C are 0, so 0x39040(other) (the gate word 0x107D2C + other * 2)
 * skips its body for either side. The other side keeps its sentinels. */
static void sr_seed(u32 s0, u32 s1, u32 r0, u32 r1, u32 st0, u32 st3)
{
    mem_fill(s0, 0, 0x94u);
    mem_fill(s1, 0, 0x94u);
    mem_fill(FIGHT_RECS, 0, 0x200u);
    mem_fill(FIGHT_ACTORS, 0, 0x80u);
    DSD(DS_001014EC) = FIGHT_ACTORS;
    fight_reset_slot_pair(s0, s1, r0, r1);
    DSW(st3) = 0x0123u;                      /* literal sprite ids */
    DSW(st0) = 0x0456u;
    DSD(0x000C895Cu) = st3;                  /* 0xC8950[3] */
    DSD(0x000C8950u) = st0;                  /* 0xC8950[0] */
    DSW(DS_00104B00) = 3u;
    DSW(DS_00107D2C) = 0;                    /* 0x39040(0) gate shut (k = 1) */
    DSW(DS_00107D2C + 2u) = 0;               /* 0x39040(1) gate shut (k = 0) */
    DSB(r0 + 0x51u) = 0;
    DSB(r1 + 0x51u) = 1;
    DSW(r0 + 0x56u) = 1;
    DSW(r1 + 0x56u) = 2;
    DSW(FIGHT_ACTORS + 0x20u) = 0x7777u;
    DSW(FIGHT_ACTORS + 0x40u) = 0x7777u;
    DSB(s0 + 0x7Au) = 0;
    DSB(s1 + 0x7Au) = 3;
    DSB(s0 + 0x54u) = 3u;
    DSB(s1 + 0x54u) = 3u;
    DSB(s0 + 0x52u) = 0x66u;
    DSB(s0 + 0x53u) = 0x66u;
    DSB(s1 + 0x52u) = 0x66u;
    DSB(s1 + 0x53u) = 0x66u;
    DSD(r0 + 8u) = 0x00ABCDEFu;
    DSD(r1 + 8u) = 0x00ABCDEFu;
    DSB(r0 + 0x4Du) = 0x55u;
    DSB(r1 + 0x4Du) = 0x55u;
}

static void check_stance_return(void)
{
    u32 s0 = DS_001077B0, s1 = DS_001077B0 + 0x94u;
    u32 r0 = FIGHT_RECS, r1 = FIGHT_RECS + 0x100u;
    u32 st3 = FIGHT_RECS + 0x3800u, st0 = FIGHT_RECS + 0x3810u;
    u32 sv_14ec = DSD(DS_001014EC);
    u32 sv_c8950 = DSD(0x000C8950u), sv_c895c = DSD(0x000C895Cu);
    u8 sv_slots[0x130], sv_b00[4], sv_d2c[4], sv_ce0[4], sv_af8[8];
    u8 sv_d148[8], sv_d20[0x10], sv_a80[0x80];
    u32 k;
    typedef void (*anim_fn)(u32 rec, u32 arg);
    anim_fn f;

    tf_snap(sv_slots, DS_001077A8, 0x130u);
    tf_snap(sv_b00, DS_00104B00, 4u);
    tf_snap(sv_d2c, DS_00107D2C, 4u);
    tf_snap(sv_ce0, DS_00100CE0, 4u);
    tf_snap(sv_af8, DS_00100AF8, 8u);
    tf_snap(sv_d148, DS_000FD148, 8u);
    tf_snap(sv_d20, DS_00107D20, 0x10u);
    tf_snap(sv_a80, 0x00107A80u, 0x80u);

    f = (anim_fn)(void *)fn_resolve(0x3C32Cu);
    CHECK(f != NULL, "0x3C32C is a registered stream target");
    CHECK(fn_resolve(0x3C32Cu) != fn_resolve(0x36870u),
          "0x3C32C is its own target, not 0x36870's");
    for (k = 0; f != NULL && k < 2u; k++) {
        u32 s = k ? s1 : s0, so = k ? s0 : s1;
        u32 r = k ? r1 : r0, ro = k ? r0 : r1;
        sr_seed(s0, s1, r0, r1, st0, st3);
        f(r, 0xFFFFFFFFu);
        CHECK_EQ_INT((int)DSB(s + 0x54u), 0);
        CHECK_EQ_INT(DSD(r + 8u), k ? st3 : st0);
        CHECK_EQ_INT((int)DSW(FIGHT_ACTORS + (k ? 0x40u : 0x20u)),
                     k ? 0x0123 : 0x0456);
        CHECK_EQ_INT((int)DSB(r + 0x4Du), 0x1E);
        CHECK_EQ_INT((int)DSB(s + 0x52u), 0);
        CHECK_EQ_INT((int)DSB(s + 0x53u), 0);
        CHECK_EQ_INT((int)DSB(so + 0x54u), 3);
        CHECK_EQ_INT((int)DSB(so + 0x52u), 0x66);
        CHECK_EQ_INT(DSD(ro + 8u), 0x00ABCDEFu);
        CHECK_EQ_INT((int)DSB(ro + 0x4Du), 0x55);
    }

    tf_put(sv_a80, 0x00107A80u, 0x80u);
    tf_put(sv_d20, DS_00107D20, 0x10u);
    tf_put(sv_d148, DS_000FD148, 8u);
    tf_put(sv_af8, DS_00100AF8, 8u);
    tf_put(sv_ce0, DS_00100CE0, 4u);
    tf_put(sv_d2c, DS_00107D2C, 4u);
    tf_put(sv_b00, DS_00104B00, 4u);
    tf_put(sv_slots, DS_001077A8, 0x130u);
    DSD(0x000C8950u) = sv_c8950;
    DSD(0x000C895Cu) = sv_c895c;
    DSD(DS_001014EC) = sv_14ec;
}

/* ---- roar-timing Task 32: character 3's reactions 0x25/0x24 (record §44-A) - */

#define C25_ANIM_A  0x000D311Au   /* 0x15350's reaction stream head */
#define C25_ANIM_B  0x000D315Au   /* 0x152D4's +0x57 == 1 stream head */
#define C24_ANIM    0x000D3078u   /* 0x151C0's reaction stream head */
#define C25_FD118   0x000FD118u   /* 0x153C9: a byte per side */
#define P7_LAND     0x000E8D50u   /* 0x2910C's landing stream head */
#define P7_P1       0x000E8D14u   /* its phase 0 -> 1 stream head */
#define P7_P2       0x000E8D28u   /* phase 1 -> 2 */
#define P7_P3       0x000E8D3Cu   /* phase 2 -> 3 */
#define C25_SCR     (FIGHT_RECS + 0x7000u)   /* scratch rows, actors, streams */

/* Link `n` nodes after the sentinel `head` into a circular {next; prev} list. */
static void c25_link(u32 head, const u32 *node, u32 n)
{
    u32 prev = head, i;
    for (i = 0; i < n; i++) {
        DSD(prev) = node[i];
        DSD(node[i] + 4u) = prev;
        prev = node[i];
    }
    DSD(prev) = head;
    DSD(head + 4u) = prev;
}

/* Record §44-A. 0x15350 (*(u32*)0xA470C reads `50 53 01 00 00 00 00 00`:
 * character 3's reaction 0x25, no stream; Ghidra has no function there): EAX
 * = slot, EDX = rec, EBX unread; nothing (AL 0) when 0x468D8(rec+0x51 ^ 1)
 * holds; else 0xD311A at hold 3.0 through 0x3C4CC, 9/7/0, +0x57 = 0, +0x0C/
 * +0x18/+0x1C = 0x152D4/0x15208/0x1527C, +0x42 |= 4 and the side's FD118
 * byte = 0. 0x152D4 (+0x0C, EBX = side): with +0x57 == 1 and +0x88 above the
 * byte 0x28 at 0x9AFF8 (signed), +0x57 = 2, 0x2BD44 on the +0x4B row and
 * 0xD315A at 3.0. 0x15208 (+0x18, EAX = side): 0x18C14 with flags 1/4/7/8/
 * 0xD/0xE = 0, 5/9 = 1 on the box tables 0x9AFFA (`6e` x7) / 0x9B001 (`63`
 * x7), or 1 while +0x88 is below the byte 1 at 0x9AFF9. 0x1527C (+0x1C):
 * 0x39834(1 - side, +0x5F), 0x36D20(other slot), 0x188AC(1 - side, the other
 * record's +0x18, 0), the other slot's +0x43 &= 0xCF, +0x57 = 1. 0x151C0
 * (*(u32*)0xA46F8, reaction 0x24): 0xD3078 at 3.0, 9/7/0, +0x57 = 0, +0x0C =
 * 0, +0x18/+0x1C = 0x15160/0x151A0, +0x42 |= 4. 0x15160: 0x18C14 with flag 0
 * = 1, 1/8 = 0, default boxes. 0x151A0: 0x3B714(other slot, slot). The
 * stream targets 0x153D8 (operand 0: 0xBB394, else 0x9B008; slot-gated;
 * child +0x14/+0x59/+0x60 and the side-1 +0x4E/+0x2E), 0x1543C (0xBB3A8, no
 * gate) and 0x37CFC (kills the record when its slot's +0x53 is not 7); the
 * opcode-0x0C spawn's a5 (0x2B4C4: the parent's +0x28 & 0x4000); the process
 * 0x2910C over the type-0x0A/0x19 node list. Stream heads it starts are
 * patched to plain frame words and restored. The actor pool is used after
 * actors_reset() and left reset, as check_trex_breath leaves it. */
static void check_char3_2425(void)
{
    u32 s0 = DS_001077B0, s1 = DS_001077B0 + 0x94u;
    u32 r0 = FIGHT_RECS, r1 = FIGHT_RECS + 0x100u;
    u32 st = FIGHT_RECS + 0x3A00u;
    u32 pset1 = FIGHT_ACTORS + 0x20u, pset2 = FIGHT_ACTORS + 0x40u;
    static const u32 heads[7] = { C25_ANIM_A, C25_ANIM_B, C24_ANIM, P7_LAND,
                                  P7_P1, P7_P2, P7_P3 };
    u16 sv_heads[7], sv_st[5];
    u32 sv_14ec = DSD(DS_001014EC), sv_14f4 = DSD(DS_001014F4);
    u16 sv_ac = DSW(0x001080ACu), sv_ae = DSW(0x001080AEu);
    u16 sv_w0 = DSW(0x000A6728u), sv_w2 = DSW(0x000A6728u + 2u);
    u32 sv_d8 = DSD(0x000A3528u + 8u), sv_rng = DSD(DS_000EF6D8);
    u8 sv_bdf2 = DSB(DS_000BEDF2), sv_b11a = DSB(0x000DE11Au);
    u8 sv_c1d = DSB(DS_00100C1D), sv_ae8 = DSB(DS_00104AE8);
    u8 sv_slots[0x130], sv_b00[4], sv_tab[28], sv_a80[0x80], sv_d20[0x10];
    u8 sv_fd[0x18], sv_f8[2], sv_af8[8], sv_ab0[16], sv_e0[4], sv_8a8[2];
    u8 sv_ring[0x50], sv_ce0[4], sv_d148[8], sv_d50[8], sv_78f2[8];
    u8 sv_nodes[0x110], sv_misc[0x40], sv_b5a[6], sv_d58[0x180], sv_ed8[0x10];
    u8 sv_bd4[0x16], sv_pr1[4], sv_pr2[4], sv_78fa = DSB(DS_001078FA);
    u32 i;
    typedef void (*anim_fn)(u32 rec, u32 arg);
    anim_fn f153, f154, f37;
    void (*p7)(void);

    for (i = 0; i < 7u; i++) sv_heads[i] = DSW(heads[i]);
    for (i = 0; i < 5u; i++) sv_st[i] = DSW(c3_streams[i]);
    tf_snap(sv_slots, DS_001077A8, 0x130u);
    tf_snap(sv_b00, DS_00104B00, 4u);
    tf_snap(sv_tab, 0x000C8950u, 28u);
    tf_snap(sv_a80, DS_00107A80, 0x80u);
    tf_snap(sv_d20, DS_00107D20, 0x10u);
    tf_snap(sv_fd, FIGHT_FD108_T, 0x18u);
    tf_snap(sv_f8, DS_001078F8, 2u);
    tf_snap(sv_af8, DS_00100AF8, 8u);
    tf_snap(sv_ab0, DS_00100AB0, 16u);
    tf_snap(sv_e0, DS_001088E0, 4u);
    tf_snap(sv_8a8, DS_001088A8, 2u);
    tf_snap(sv_ring, DS_00108270, 0x50u);
    tf_snap(sv_ce0, DS_00100CE0, 4u);
    tf_snap(sv_d148, DS_000FD148, 8u);
    tf_snap(sv_d50, DS_00107D50, 8u);
    tf_snap(sv_78f2, DS_001078F2, 8u);
    tf_snap(sv_nodes, DS_00104780, 0x110u);
    tf_snap(sv_misc, 0x00104B00u, 0x40u);
    tf_snap(sv_b5a, DS_00100B5A, 6u);
    tf_snap(sv_d58, 0x00107D58u, 0x180u);
    tf_snap(sv_ed8, DS_00107ED8, 0x10u);
    tf_snap(sv_bd4, DS_00105BD4, 0x16u);
    tf_snap(sv_pr1, 0x001074A8u, 4u);
    tf_snap(sv_pr2, 0x001074B8u, 4u);

    CHECK(fn_resolve(0x15350u) == (void (*)(void))fighter_15350,
          "0x15350 is registered as fighter_15350");
    CHECK(fn_resolve(0x152D4u) == (void (*)(void))fighter_152d4,
          "0x152D4 is registered as fighter_152d4");
    CHECK(fn_resolve(0x15208u) == (void (*)(void))fighter_15208,
          "0x15208 is registered as fighter_15208");
    CHECK(fn_resolve(0x1527Cu) == (void (*)(void))fighter_1527c,
          "0x1527C is registered as fighter_1527c");
    CHECK(fn_resolve(0x151C0u) == (void (*)(void))fighter_151c0,
          "0x151C0 is registered as fighter_151c0");
    CHECK(fn_resolve(0x15160u) == (void (*)(void))fighter_15160,
          "0x15160 is registered as fighter_15160");
    CHECK(fn_resolve(0x151A0u) == (void (*)(void))fighter_151a0,
          "0x151A0 is registered as fighter_151a0");
    f153 = (anim_fn)(void *)fn_resolve(0x153D8u);
    f154 = (anim_fn)(void *)fn_resolve(0x1543Cu);
    f37 = (anim_fn)(void *)fn_resolve(0x37CFCu);
    p7 = fn_resolve(0x2910Cu);
    CHECK(f153 != NULL, "0x153D8 is a registered stream target");
    CHECK(f154 != NULL, "0x1543C is a registered stream target");
    CHECK(f37 != NULL, "0x37CFC is a registered stream target");
    CHECK(p7 != NULL, "0x2910C (process entry 7) is registered");
    CHECK_EQ_INT((int)DSD(0x000A8660u), 0x0002910C);   /* DS_000A8644[7] */
    CHECK_EQ_INT((int)DSB(0x0009AFF8u), 0x28);
    CHECK_EQ_INT((int)DSB(0x0009AFF9u), 0x01);
    CHECK_EQ_INT((int)DSB(0x0009AFFAu), 0x6E);
    CHECK_EQ_INT((int)DSB(0x0009B001u), 0x63);

    /* A: 0x15350 through 0x34E2C (0x35045) for character 3, reaction 0x25. */
    c3_seed(s0, s1, r0, r1, st);
    DSW(C25_ANIM_A) = 0x1320u;
    DSW(DS_001088E0) = 0;
    DSB(s0 + 0x42u) = 0x09u;
    DSB(C25_FD118) = 0x5Au;
    DSB(C25_FD118 + 1u) = 0x6Bu;
    hit_reaction_apply(0u, 0x25u);
    CHECK_EQ_INT((int)DSB(s0 + 0x5Fu), 0x25);
    CHECK_EQ_INT((int)DSD(r0 + 8u), (int)C25_ANIM_A);
    CHECK_EQ_INT((int)DSD(r0 + 0x24u), 0x40400000);
    CHECK_EQ_INT((int)(DSW(pset1) & 0x7FFFu), 0x1320);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 9);
    CHECK_EQ_INT((int)DSB(s0 + 0x53u), 7);
    CHECK_EQ_INT((int)DSB(s0 + 0x54u), 0);
    CHECK_EQ_INT((int)DSB(s0 + 0x57u), 0);
    CHECK_EQ_INT((int)DSD(s0 + 0x0Cu), 0x000152D4);
    CHECK_EQ_INT((int)DSD(s0 + 0x18u), 0x00015208);
    CHECK_EQ_INT((int)DSD(s0 + 0x1Cu), 0x0001527C);
    CHECK_EQ_INT((int)DSB(s0 + 0x42u), 0x0D);
    CHECK_EQ_INT((int)DSB(C25_FD118), 0);
    CHECK_EQ_INT((int)DSB(C25_FD118 + 1u), 0x6B);
    CHECK_EQ_INT((int)DSD(s1 + 0x0Cu), 0x11111111);
    CHECK_EQ_INT((int)DSD(s1 + 0x18u), 0x22222222);
    CHECK_EQ_INT((int)DSD(r1 + 8u), 0x00ABCDEF);
    /* A2: direct on side 1 (EBX = 0 unread): the other side is 0; bit 2
     * already set stays set. */
    c3_seed(s0, s1, r0, r1, st);
    DSW(C25_ANIM_A) = 0x1321u;
    DSB(s1 + 0x42u) = 0x04u;
    DSB(C25_FD118) = 0x5Au;
    DSB(C25_FD118 + 1u) = 0x6Bu;
    fighter_15350(s1, r1, 0u);
    CHECK_EQ_INT((int)DSD(r1 + 8u), (int)C25_ANIM_A);
    CHECK_EQ_INT((int)(DSW(pset2) & 0x7FFFu), 0x1321);
    CHECK_EQ_INT((int)DSB(s1 + 0x52u), 9);
    CHECK_EQ_INT((int)DSB(s1 + 0x53u), 7);
    CHECK_EQ_INT((int)DSB(s1 + 0x57u), 0);
    CHECK_EQ_INT((int)DSD(s1 + 0x0Cu), 0x000152D4);
    CHECK_EQ_INT((int)DSB(s1 + 0x42u), 0x04);
    CHECK_EQ_INT((int)DSB(C25_FD118 + 1u), 0);
    CHECK_EQ_INT((int)DSB(C25_FD118), 0x5A);
    CHECK_EQ_INT((int)DSD(r0 + 8u), 0x00ABCDEF);
    CHECK_EQ_INT((int)DSD(s0 + 0x0Cu), 0x11111111);
    /* A3: 0x468D8 on the other side holds (its +0x52 == 7, or the knockdown
     * handler 0x22BEC with +0x24 == 0 and +0x54 != 2): nothing is written. */
    for (i = 0; i < 3u; i++) {
        c3_seed(s0, s1, r0, r1, st);
        DSB(s0 + 0x42u) = 0x09u;
        DSB(C25_FD118) = 0x5Au;
        if (i == 0u) DSB(s1 + 0x52u) = 7u;
        if (i >= 1u) {
            DSD(s1 + 0x10u) = 0x00022BECu;
            DSD(r1 + 0x24u) = 0;
            DSB(s1 + 0x54u) = (u8)(i == 1u ? 0x66u : 2u);
        }
        fighter_15350(s0, r0, 1u);
        if (i < 2u) {
            CHECK_EQ_INT((int)DSD(r0 + 8u), 0x00ABCDEF);
            CHECK_EQ_INT((int)DSB(s0 + 0x52u), 0);
            CHECK_EQ_INT((int)DSB(s0 + 0x53u), 0x66);
            CHECK_EQ_INT((int)DSB(s0 + 0x57u), 0x99);
            CHECK_EQ_INT((int)DSD(s0 + 0x0Cu), 0x11111111);
            CHECK_EQ_INT((int)DSD(s0 + 0x18u), 0x22222222);
            CHECK_EQ_INT((int)DSD(s0 + 0x1Cu), 0x33333333);
            CHECK_EQ_INT((int)DSB(s0 + 0x42u), 0x09);
            CHECK_EQ_INT((int)DSB(C25_FD118), 0x5A);
        } else {                                /* +0x54 == 2: the arm fails */
            CHECK_EQ_INT((int)DSD(r0 + 8u), (int)C25_ANIM_A);
            CHECK_EQ_INT((int)DSB(s0 + 0x53u), 7);
            CHECK_EQ_INT((int)DSB(C25_FD118), 0);
        }
    }

    /* B: 0x152D4 (the slot/rec registers unread: passed the other side's). */
    c3_seed(s0, s1, r0, r1, st);
    DSW(C25_ANIM_B) = 0x1322u;
    {
        static const u8 g57[5] = { 0x99u, 2u, 0u, 1u, 1u };
        static const u16 g88[5] = { 0x0100u, 0x0100u, 0x0100u, 0x0028u, 0x8000u };
        u32 k;
        for (k = 0; k < 5u; k++) {
            DSB(s0 + 0x57u) = g57[k];
            DSW(s0 + 0x88u) = g88[k];
            fighter_152d4(s1, r1, 0u);
            CHECK_EQ_INT((int)DSB(s0 + 0x57u), (int)g57[k]);
            CHECK_EQ_INT((int)DSD(r0 + 8u), 0x00ABCDEF);
        }
    }
    DSB(s0 + 0x57u) = 1u;
    DSW(s0 + 0x88u) = 0x0029u;
    DSB(r0 + 0x4Bu) = 0;
    fighter_152d4(s1, r1, 0u);
    CHECK_EQ_INT((int)DSB(s0 + 0x57u), 2);
    CHECK_EQ_INT((int)DSD(r0 + 8u), (int)C25_ANIM_B);
    CHECK_EQ_INT((int)DSD(r0 + 0x24u), 0x40400000);
    CHECK_EQ_INT((int)(DSW(pset1) & 0x7FFFu), 0x1322);
    CHECK_EQ_INT((int)DSB(r0 + 0x4Bu), 0);
    CHECK_EQ_INT((int)DSB(s1 + 0x57u), 0x9A);
    CHECK_EQ_INT((int)DSD(r1 + 8u), 0x00ABCDEF);
    /* B2: a non-zero +0x4B: 0x2BD44(rec, DS_001014F4 + 0x68 * idx) copies
     * the row's +0x4B, re-arms it (+8 = 0x1E1, +0x24 = 0) and kills it. */
    {
        u32 row = C25_SCR + 0x68u;
        mem_fill(C25_SCR, 0, 0x100u);
        DSD(DS_001014F4) = C25_SCR;
        DSB(row + 0x4Bu) = 0x3Cu;
        DSD(row + 0x08u) = 0x12345678u;
        DSD(row + 0x24u) = 0x11111111u;
        DSW(row + 0x56u) = 3u;
        DSD(FIGHT_ACTORS + 0x60u + 0x18u) = 0;
        DSB(s0 + 0x57u) = 1u;
        DSB(r0 + 0x4Bu) = 1u;
        DSD(r0 + 8u) = 0x00ABCDEFu;
        fighter_152d4(s0, r0, 0u);
        CHECK_EQ_INT((int)DSB(r0 + 0x4Bu), 0x3C);
        CHECK_EQ_INT((int)DSD(row + 0x08u), 0x1E1);
        CHECK_EQ_INT((int)DSD(row + 0x24u), 0);
        CHECK_EQ_INT((int)(DSB(row + 0x28u) & 8u), 8);
        CHECK_EQ_INT((int)DSD(r0 + 8u), (int)C25_ANIM_B);
        CHECK_EQ_INT((int)DSB(s0 + 0x57u), 2);
        DSD(DS_001014F4) = sv_14f4;
    }

    /* C: 0x15208 on gr_seed's passing context (every 0x14CC4 check passes,
     * flag 0xE's 0x3B298 copy marks +0x86 = 0x1234; the records' +0x1C = 0).
     * |x0 - x1| = 0x1000 is inside box a 0x6E << 6 = 0x1B80. */
    gr_seed(s0, s1, r0, r1, 0x2000, 0x1000);
    DSW(s0 + 0x88u) = 1u;
    CHECK_EQ_INT((int)fighter_15208(0u), 0);
    CHECK_EQ_INT((int)DSW(s1 + 0x86u), 0x1234);
    /* C2: box a is 0x9AFFA's (x), box b 0x9B001's (y; 0x63 << 6 = 0x18C0). */
    {
        static const s32 x0[4] = { 0x1000 + 0x1B80, 0x1000 + 0x1B81, 0x2000, 0x2000 };
        static const s32 y0[4] = { 0, 0, 0x18C0, 0x18C1 };
        static const int want[4] = { 0, 1, 0, 1 };
        u32 k;
        for (k = 0; k < 4u; k++) {
            gr_seed(s0, s1, r0, r1, x0[k], 0x1000);
            DSD(r0 + 0x1Cu) = (u32)y0[k];
            DSW(s0 + 0x88u) = 1u;
            CHECK_EQ_INT((int)fighter_15208(0u), want[k]);
        }
    }
    /* C3: +0x88 below the 0x9AFF9 byte (signed) returns 1 on a pass. */
    {
        static const u16 g88[3] = { 0u, 0xFFFFu, 2u };
        static const int want[3] = { 1, 1, 0 };
        u32 k;
        for (k = 0; k < 3u; k++) {
            gr_seed(s0, s1, r0, r1, 0x2000, 0x1000);
            DSW(s0 + 0x88u) = g88[k];
            CHECK_EQ_INT((int)fighter_15208(0u), want[k]);
        }
    }
    /* C4: the flags it sets, each firing alone: 1 (+0x76), 4 (+0x54 = 2), 7
     * (+0x62), 8 (+0x42 bit 3), 9 = 1 (x0 below x1), 0xD (0x39EFC(1): slot 1
     * in the 0x39CC8 pose, +0x53 = 0x0A, +0x58 = 4; its firing clears slot
     * 0's +0x8A). Flag 0xE's pass is C's +0x86 = 0x1234 mark; flag 5 is
     * C2's box edges. */
    {
        u32 k;
        for (k = 0; k < 6u; k++) {
            gr_seed(s0, s1, r0, r1, 0x2000, 0x1000);
            DSW(s0 + 0x88u) = 1u;
            if (k == 0u) DSW(s1 + 0x76u) = 2u;
            if (k == 1u) DSB(s1 + 0x54u) = 2u;
            if (k == 2u) DSB(s1 + 0x62u) = 1u;
            if (k == 3u) DSB(s1 + 0x42u) = 8u;
            if (k == 4u) {
                DSD(s0 + 0x2Cu) = 0x1000u;
                DSD(r0 + 0x18u) = 0x1000u;
                DSD(s1 + 0x2Cu) = 0x2000u;
                DSD(r1 + 0x18u) = 0x2000u;
            }
            if (k == 5u) {
                DSB(s1 + 0x53u) = 0x0Au;
                DSD(s1 + 0x10u) = 0x00039CC8u;
                DSB(s1 + 0x58u) = 4u;
            }
            CHECK_EQ_INT((int)fighter_15208(0u), 1);
            if (k == 5u) CHECK_EQ_INT((int)DSB(s0 + 0x8Au), 0);
        }
    }
    /* C5: through 0x19020: DS_00100AF8[0] = (hook == 0). */
    gr_seed(s0, s1, r0, r1, 0x2000, 0x1000);
    DSD(s0 + 0x18u) = 0x00015208u;
    DSW(s0 + 0x88u) = 1u;
    DSD(DS_00100AF8) = 0x5Au;
    fighter_19020(0u);
    CHECK_EQ_INT((int)DSD(DS_00100AF8), 1);
    DSW(s0 + 0x88u) = 0;
    fighter_19020(0u);
    CHECK_EQ_INT((int)DSD(DS_00100AF8), 0);

    /* D: 0x1527C(0): 0x39834(1, +0x5F) counts on side 0's 0x107D2C word and
     * stores b; 0x36D20(slot 1) zeroes record 1's +0x34 (its +0x54 != 2 arm);
     * 0x188AC(1, record 1's +0x18, 0) zeroes record 1's +0x1C; slot 1's
     * +0x43 bits 4/5 cleared; slot 0's +0x57 = 1. */
    c3_seed(s0, s1, r0, r1, st);
    DSB(s0 + 0x5Fu) = 0x25u;
    DSB(s1 + 0x5Fu) = 0x27u;
    DSW(DS_00107D2C) = 7u;
    DSW(DS_00107D2C + 2u) = 9u;
    DSD(DS_00107D28) = 0x5A5A5A5Au;
    DSB(s1 + 0x43u) = 0xFFu;
    DSB(s0 + 0x43u) = 0xFFu;
    DSW(r1 + 0x34u) = 0x3434u;
    DSW(r0 + 0x34u) = 0x3434u;
    DSD(r1 + 0x18u) = 0x00012345u;
    fighter_1527c(0u);
    CHECK_EQ_INT((int)DSW(DS_00107D2C), 8);
    CHECK_EQ_INT((int)DSW(DS_00107D2C + 2u), 9);
    CHECK_EQ_INT((int)DSD(DS_00107D28), 0x25);
    CHECK_EQ_INT((int)DSW(r1 + 0x34u), 0);
    CHECK_EQ_INT((int)DSW(r0 + 0x34u), 0x3434);
    CHECK_EQ_INT((int)DSD(r1 + 0x1Cu), 0);
    CHECK_EQ_INT((int)DSD(r1 + 0x18u), 0x00012345);
    CHECK_EQ_INT((int)DSD(r0 + 0x1Cu), 0x5555);
    CHECK_EQ_INT((int)(DSB(s1 + 0x43u) & 0x30u), 0);
    CHECK_EQ_INT((int)DSB(s0 + 0x43u), 0xFF);
    CHECK_EQ_INT((int)DSB(s0 + 0x57u), 1);
    CHECK_EQ_INT((int)DSB(s1 + 0x57u), 0x9A);

    /* E: 0x151C0 through 0x34E2C for character 3, reaction 0x24. */
    c3_seed(s0, s1, r0, r1, st);
    DSW(C24_ANIM) = 0x1323u;
    DSW(DS_001088E0) = 0;
    DSB(s0 + 0x42u) = 0x09u;
    hit_reaction_apply(0u, 0x24u);
    CHECK_EQ_INT((int)DSB(s0 + 0x5Fu), 0x24);
    CHECK_EQ_INT((int)DSD(r0 + 8u), (int)C24_ANIM);
    CHECK_EQ_INT((int)DSD(r0 + 0x24u), 0x40400000);
    CHECK_EQ_INT((int)(DSW(pset1) & 0x7FFFu), 0x1323);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 9);
    CHECK_EQ_INT((int)DSB(s0 + 0x53u), 7);
    CHECK_EQ_INT((int)DSB(s0 + 0x54u), 0);
    CHECK_EQ_INT((int)DSB(s0 + 0x57u), 0);
    CHECK_EQ_INT((int)DSD(s0 + 0x0Cu), 0);
    CHECK_EQ_INT((int)DSD(s0 + 0x18u), 0x00015160);
    CHECK_EQ_INT((int)DSD(s0 + 0x1Cu), 0x000151A0);
    CHECK_EQ_INT((int)DSB(s0 + 0x42u), 0x0D);
    CHECK_EQ_INT((int)DSD(s1 + 0x0Cu), 0x11111111);
    CHECK_EQ_INT((int)DSD(r1 + 8u), 0x00ABCDEF);
    /* E2: direct on side 1. */
    c3_seed(s0, s1, r0, r1, st);
    DSW(C24_ANIM) = 0x1324u;
    fighter_151c0(s1, r1, 0u);
    CHECK_EQ_INT((int)DSD(r1 + 8u), (int)C24_ANIM);
    CHECK_EQ_INT((int)(DSW(pset2) & 0x7FFFu), 0x1324);
    CHECK_EQ_INT((int)DSB(s1 + 0x53u), 7);
    CHECK_EQ_INT((int)DSD(s1 + 0x18u), 0x00015160);
    CHECK_EQ_INT((int)DSD(s0 + 0x18u), 0x22222222);
    CHECK_EQ_INT((int)DSD(r0 + 8u), 0x00ABCDEF);

    /* F: 0x15160 on sh_seed (every check passes, DS_00100AF8 = 5). */
    sh_seed(s0, s1, r0, r1);
    CHECK_EQ_INT((int)fighter_15160(0u), 0);
    DSD(DS_00100AF8) = 0;                   /* flag 0 = 1: AF8[0] <= 0 fires */
    CHECK_EQ_INT((int)fighter_15160(0u), 1);
    DSD(DS_00100AF8) = 5u;
    DSW(s1 + 0x76u) = 2u;                   /* flag 1 */
    CHECK_EQ_INT((int)fighter_15160(0u), 1);
    DSW(s1 + 0x76u) = 0;
    DSB(s1 + 0x42u) = 8u;                   /* flag 8 */
    CHECK_EQ_INT((int)fighter_15160(0u), 1);
    DSB(s1 + 0x42u) = 0;
    DSB(s1 + 0x54u) = 2u;                   /* flag 4 is not set */
    DSB(s1 + 0x62u) = 1u;                   /* flag 7 is not set */
    CHECK_EQ_INT((int)fighter_15160(0u), 0);
    DSD(s0 + 0x18u) = 0x00015160u;
    fighter_19020(0u);
    CHECK_EQ_INT((int)DSD(DS_00100AF8), 1);

    /* G: 0x151A0(0) is 0x3B714(slot 1, slot 0): check_reaction's seeds, so
     * slot 1 takes slot 0's +0x5F into +0x65 and the 0x3A504 pose. */
    pose_chain_setup(s0, s1, r0, r1);
    DSD(DS_00107D50 + 4u) = 0;
    DSB(s0 + 0x5Fu) = 0;
    DSB(s1 + 0x5Fu) = 0;
    DSB(s0 + 0x53u) = 0;
    DSD(s1 + 0x10u) = 0;
    DSB(s1 + 0x58u) = 0;
    DSB(s1 + 0x54u) = 0;
    DSB(s0 + 0x52u) = 0;
    DSB(s1 + 0x52u) = 0;
    DSB(s1 + 0x65u) = 0xFFu;
    DSB(s0 + 0x65u) = 0xFFu;
    DSB(s1 + 0x41u) = 0;
    DSB(r1 + 0x4Bu) = 0;
    DSB(s1 + 0x63u) = 0;
    DSB(DS_000BEDF2) = 1;
    DSW(0x000A6728u + 2u) = 3;
    DSD(r1 + 0x24u) = 0xDEADBEEFu;
    fighter_151a0(0u);
    CHECK_EQ_INT((int)DSB(s1 + 0x65u), 0);
    CHECK_EQ_INT((int)DSB(s0 + 0x65u), 0xFF);
    CHECK_EQ_INT((int)DSB(s1 + 0x52u), 0x10);
    CHECK_EQ_INT((int)DSD(s1 + 0x10u), 0x0003A43C);
    CHECK_EQ_INT((int)DSW(s0 + 0x6Cu), 1);

    /* H: the stream targets on pool records. */
    tb_seed(s0, s1, r0, r1);
    actors_reset();
    if (f153 != NULL && f154 != NULL && f37 != NULL) {
        u32 f[2], e, n;
        for (i = 0; i < 2u; i++) {
            f[i] = actor_alloc(0);
            DSW(f[i] + 0x56u) = (u16)actor_index(f[i]);
            DSD(f[i] + 0x14u) = i == 0u ? s0 : s1;
            DSB(f[i] + 0x51u) = (u8)i;
            DSB(f[i] + 0x4Bu) = 0x77u;
        }
        /* H1: 0x153D8 with operand 0 spawns 0xBB394 (stream 0xD318E, whose
         * first word 0x06D5 is a frame) as f0's child: +0x14 = the slot,
         * +0x59 = 2, +0x60 = 1; side 0 keeps the descriptor's +0x2E = 9. */
        n = tb_active();
        f153(f[0], 0u);
        CHECK_EQ_INT((int)tb_active(), (int)(n + 1u));
        e = actor_record((u32)DSB(f[0] + 0x4Bu));
        CHECK(e != 0u && e != f[0] && e != f[1],
              "0x153D8 stores the child's index in rec+0x4B");
        if (e != 0u) {
            CHECK_EQ_INT((int)DSD(e + 8u), 0x000D318E);
            CHECK_EQ_INT((int)DSD(e + 0x14u), (int)s0);
            CHECK_EQ_INT((int)DSB(e + 0x59u), 2);
            CHECK_EQ_INT((int)DSB(e + 0x60u), 1);
            CHECK_EQ_INT((int)DSB(e + 0x4Eu), 0);
            CHECK_EQ_INT((int)DSW(e + 0x2Eu), 0x0009);
            CHECK_EQ_INT((int)(DSW(e + 0x28u) & 0x0400u), 0x0400);
            /* H2: 0x37CFC on that child: its slot's +0x53 == 7 keeps it. */
            DSB(e + 0x55u) = 0x33u;
            DSB(s0 + 0x53u) = 7u;
            f37(e, 0u);
            CHECK_EQ_INT((int)DSB(e + 0x55u), 0x33);
            CHECK_EQ_INT((int)(DSB(e + 0x28u) & 8u), 0);
            DSD(e + 0x14u) = 0;                 /* no slot: kept */
            DSB(s0 + 0x53u) = 6u;
            f37(e, 0u);
            CHECK_EQ_INT((int)DSB(e + 0x55u), 0x33);
            CHECK_EQ_INT((int)(DSB(e + 0x28u) & 8u), 0);
            DSD(e + 0x14u) = s0;                /* +0x53 != 7: killed */
            f37(e, 0u);
            CHECK_EQ_INT((int)DSB(e + 0x55u), 1);
            CHECK_EQ_INT((int)(DSB(e + 0x28u) & 8u), 8);
        }
        /* H3: side 1 adds 4 to +0x2E and sets +0x4E. */
        f153(f[1], 0u);
        e = actor_record((u32)DSB(f[1] + 0x4Bu));
        CHECK(e != 0u && e != f[0] && e != f[1],
              "0x153D8 (side 1) stores the child's index in rec+0x4B");
        if (e != 0u) {
            CHECK_EQ_INT((int)DSD(e + 0x14u), (int)s1);
            CHECK_EQ_INT((int)DSB(e + 0x4Eu), 1);
            CHECK_EQ_INT((int)DSW(e + 0x2Eu), 0x000D);
        }
        /* H4: operand 1 selects 0x9B008 (stream 0xD31AE: the walk stops on
         * `CD40 06D5`, opcode 0x0D, leaving +8 at the operand word 0xD31B0,
         * as 0xE8598's `CD40 03E8` does in check_trex_breath). */
        f153(f[0], 1u);
        e = actor_record((u32)DSB(f[0] + 0x4Bu));
        CHECK(e != 0u, "0x153D8's non-zero operand spawns a child");
        if (e != 0u) CHECK_EQ_INT((int)DSD(e + 8u), 0x000D31B0);
        /* H5: no slot (+0x14 = 0, 0x153F1): no spawn, +0x4B kept. */
        DSD(f[0] + 0x14u) = 0;
        DSB(f[0] + 0x4Bu) = 0x77u;
        n = tb_active();
        f153(f[0], 0u);
        CHECK_EQ_INT((int)tb_active(), (int)n);
        CHECK_EQ_INT((int)DSB(f[0] + 0x4Bu), 0x77);
        /* H6: 0x1543C has no slot gate and no side adjust: 0xBB3A8 (stream
         * 0xD30F2: `9302` sets +0x59 = 2, then the frame 0x06F2). */
        DSD(f[1] + 0x14u) = 0;
        DSB(f[1] + 0x4Bu) = 0x77u;
        n = tb_active();
        f154(f[1], 0xFFFFu);
        CHECK_EQ_INT((int)tb_active(), (int)(n + 1u));
        e = actor_record((u32)DSB(f[1] + 0x4Bu));
        CHECK(e != 0u && DSB(f[1] + 0x4Bu) != 0x77u,
              "0x1543C stores the child's index in rec+0x4B");
        if (e != 0u) {
            CHECK_EQ_INT((int)DSD(e + 8u), 0x000D30F4);
            CHECK_EQ_INT((int)DSB(e + 0x60u), 1);
            CHECK_EQ_INT((int)DSB(e + 0x4Eu), 0);
            CHECK_EQ_INT((int)DSW(e + 0x2Eu), 0x0018);
        }
    }
    /* I: the opcode-0x0C spawn (0xE8CE6's `CC00 B0C4 000B 0000 0000`, here
     * with 0xBB394, type 0) passes the parent's +0x28 bit 14 as a5 = 0x4000,
     * which actor_spawn copies into the child's +0x28. */
    for (i = 0; i < 2u; i++) {
        u32 p, sp = C25_SCR + 0x100u, r, found = 0;
        actors_reset();
        p = actor_alloc(0);
        DSW(p + 0x56u) = (u16)actor_index(p);
        DSW(p + 0x28u) = (u16)(i == 0u ? 0x4000u : 0u);
        DSW(sp) = 0xCC00u;
        DSW(sp + 2u) = 0xB394u;
        DSW(sp + 4u) = 0x000Bu;
        DSW(sp + 6u) = 0;
        DSW(sp + 8u) = 0;
        DSW(sp + 10u) = 0x1325u;
        actors_anim_begin(p, sp, 0x3F800000u);
        for (r = actor_list_head(); r != 0; r = actor_next(r)) {
            if (r == p || DSD(r + 8u) != 0x000D318Eu) continue;
            found++;
            CHECK_EQ_INT((int)(DSW(r + 0x28u) & 0x4000u),
                         i == 0u ? 0x4000 : 0);
        }
        CHECK_EQ_INT((int)found, 1);
    }
    actors_reset();

    /* J: 0x2910C over eight in-use nodes (the free list empty). A lands (y =
     * +0x1C + (s16)+0x36 < 0) and goes to the free list first in the walk;
     * the next node was read before, so B still runs. B 0 -> 1 (|vy| <
     * |vx|), C stays 0 (equal), D 1 -> 2 (|vy| > |vx|), E stays 2 (|vy| ==
     * 2|vx|), F 2 -> 3, G stays 3, H (phase 7, above 3) untouched. */
    if (p7 != NULL) {
        static const s16 vx[8] = { 5, -0x21, 0x10, -5, 3, 3, 1, 1 };
        static const s16 vy[8] = { -0x20, -0x20, 0x10, 6, 6, -7, 0x40, 0x40 };
        static const s32 y[8] = { 0x10, 0x20, 0x100, 0x100, 0x100, 0x100, 0x100, 0x100 };
        static const u8 ph[8] = { 0, 0, 0, 1, 2, 2, 3, 7 };
        static const u8 ph_after[8] = { 0, 1, 0, 2, 2, 3, 3, 7 };
        u32 node[8], act[8];
        mem_fill(C25_SCR + 0x200u, 0, 8u * 0x68u);
        DSD(DS_001014EC) = FIGHT_ACTORS;
        for (i = 0; i < 7u; i++) DSW(heads[i]) = (u16)(0x1330u + i);
        for (i = 0; i < 8u; i++) {
            node[i] = DS_00104780 + i * 0x10u;
            act[i] = C25_SCR + 0x200u + i * 0x68u;
            DSD(node[i] + 8u) = act[i];
            DSB(node[i] + 0x0Cu) = ph[i];
            DSD(act[i] + 0x14u) = node[i];
            DSW(act[i] + 0x34u) = (u16)vx[i];
            DSW(act[i] + 0x36u) = (u16)vy[i];
            DSD(act[i] + 0x1Cu) = (u32)y[i];
            DSW(act[i] + 0x44u) = 0x0Cu;
            DSB(act[i] + 0x48u) = 0x0Au;
            DSB(act[i] + 0x59u) = 2u;
            DSW(act[i] + 0x56u) = 1u;
            DSD(act[i] + 8u) = 0x00ABCDEFu;
            DSD(act[i] + 0x24u) = 0x11111111u;
        }
        c25_link(DS_00104880, node, 8u);
        c25_link(0x00104888u, node, 0u);
        DSB(DS_00104AE8) = 0x80u;
        p7();
        /* A landed. */
        CHECK_EQ_INT((int)DSD(act[0] + 0x1Cu), 0);
        CHECK_EQ_INT((int)DSW(act[0] + 0x34u), 0);
        CHECK_EQ_INT((int)DSW(act[0] + 0x36u), 0);
        CHECK_EQ_INT((int)DSW(act[0] + 0x44u), 0);
        CHECK_EQ_INT((int)DSB(act[0] + 0x48u), 0);
        CHECK_EQ_INT((int)DSB(act[0] + 0x59u), 0xFE);
        CHECK_EQ_INT((int)DSD(act[0] + 8u), (int)P7_LAND);
        CHECK_EQ_INT((int)DSD(act[0] + 0x24u), 0x40400000);
        CHECK_EQ_INT((int)DSD(act[0] + 0x14u), 0);
        CHECK_EQ_INT((int)DSD(0x00104888u), (int)node[0]);
        CHECK_EQ_INT((int)DSD(node[0]), 0x00104888);
        CHECK_EQ_INT((int)DSD(DS_00104880), (int)node[1]);
        CHECK_EQ_INT((int)DSD(node[1] + 4u), (int)DS_00104880);
        /* The rest: y = 0 is not a landing (B). */
        for (i = 1; i < 8u; i++) {
            static const u32 want_st[8] = { 0, P7_P1, 0, P7_P2, 0, P7_P3, 0, 0 };
            CHECK_EQ_INT((int)DSB(node[i] + 0x0Cu), (int)ph_after[i]);
            CHECK_EQ_INT((int)DSD(act[i] + 8u),
                         want_st[i] ? (int)want_st[i] : 0x00ABCDEF);
            CHECK_EQ_INT((int)DSD(act[i] + 0x24u),
                         want_st[i] ? 0x40000000 : 0x11111111);
            CHECK_EQ_INT((int)DSD(act[i] + 0x14u), (int)node[i]);
            CHECK_EQ_INT((int)DSB(act[i] + 0x59u), 2);
        }
        CHECK_EQ_INT((int)DSB(DS_00104AE8), 0x80);
    }

    tf_put(sv_pr2, 0x001074B8u, 4u);
    tf_put(sv_pr1, 0x001074A8u, 4u);
    tf_put(sv_bd4, DS_00105BD4, 0x16u);
    tf_put(sv_ed8, DS_00107ED8, 0x10u);
    tf_put(sv_d58, 0x00107D58u, 0x180u);
    tf_put(sv_b5a, DS_00100B5A, 6u);
    DSB(DS_001078FA) = sv_78fa;
    tf_put(sv_misc, 0x00104B00u, 0x40u);
    tf_put(sv_nodes, DS_00104780, 0x110u);
    tf_put(sv_78f2, DS_001078F2, 8u);
    tf_put(sv_d50, DS_00107D50, 8u);
    tf_put(sv_d148, DS_000FD148, 8u);
    tf_put(sv_ce0, DS_00100CE0, 4u);
    tf_put(sv_ring, DS_00108270, 0x50u);
    tf_put(sv_8a8, DS_001088A8, 2u);
    tf_put(sv_e0, DS_001088E0, 4u);
    tf_put(sv_ab0, DS_00100AB0, 16u);
    tf_put(sv_af8, DS_00100AF8, 8u);
    tf_put(sv_f8, DS_001078F8, 2u);
    tf_put(sv_fd, FIGHT_FD108_T, 0x18u);
    tf_put(sv_d20, DS_00107D20, 0x10u);
    tf_put(sv_a80, DS_00107A80, 0x80u);
    tf_put(sv_tab, 0x000C8950u, 28u);
    tf_put(sv_b00, DS_00104B00, 4u);
    tf_put(sv_slots, DS_001077A8, 0x130u);
    DSB(DS_00104AE8) = sv_ae8;
    DSB(DS_00100C1D) = sv_c1d;
    DSB(0x000DE11Au) = sv_b11a;
    DSB(DS_000BEDF2) = sv_bdf2;
    DSD(DS_000EF6D8) = sv_rng;
    DSD(0x000A3528u + 8u) = sv_d8;
    DSW(0x000A6728u) = sv_w0;
    DSW(0x000A6728u + 2u) = sv_w2;
    DSW(0x001080ACu) = sv_ac;
    DSW(0x001080AEu) = sv_ae;
    for (i = 0; i < 5u; i++) DSW(c3_streams[i]) = sv_st[i];
    for (i = 0; i < 7u; i++) DSW(heads[i]) = sv_heads[i];
    DSD(DS_001014F4) = sv_14f4;
    DSD(DS_001014EC) = sv_14ec;
}

/* ---- §41-C: the projectile +0x48 == 8 freeze 0x235C4 and 0x370F0 -------- */

#define FZ_POOL  (FIGHT_RECS + 0x5000u)   /* r0/r1, 0x68 apart: in_pool */
#define FZ_SRC   (FIGHT_RECS + 0x3800u)   /* the psets' +0x18 entries, 0x10 apart */
#define FZ_EFX   (FIGHT_RECS + 0x4000u)   /* the one free effect record */
#define FZ_TAB   (FIGHT_RECS + 0x3900u)   /* char 2's crafted 0xA8A98 row */
#define FZ_OLD   (FIGHT_RECS + 0x3980u)   /* a palette entry the pset holds */
#define FZ_ST    (FIGHT_RECS + 0x39C0u)   /* a one-word stream for 0xBDC2C[2] */
#define FZ_HANDLE 0x7F00ABCDu             /* a handle the palette table holds */

/* Both slots on the pool records r0/r1 (side 0 char 0, side 1 char 2), every
 * field the freeze family writes on a sentinel that differs from its
 * post-condition, the psets' +0x18 on FZ_SRC + 0x10 * side and +0x1C on
 * FZ_SRC + 0x40 + 0x10 * side (all +0xC = 0 entries),
 * the 0x10474C/0x10476C words and bytes, the snapshot area filled with 0xEE,
 * no live effect (0x9AF3D = 0) and an empty effect free list. */
static void fz_seed(u32 r0, u32 r1)
{
    u32 s[2], r[2], i;
    (void)tf_hit_fixture(0);
    mem_fill(r0, 0, 2u * ACTOR_REC_SIZE);
    mem_fill(FZ_SRC, 0, 0x60u);
    fight_reset_slot_pair(DS_001077B0, DS_001077B0 + 0x94u, r0, r1);
    DSD(DS_001014F4) = r0;
    s[0] = DS_001077B0; s[1] = DS_001077B0 + 0x94u;
    r[0] = r0; r[1] = r1;
    for (i = 0; i < 2u; i++) {
        DSB(r[i] + 0x51u) = (u8)i;
        DSW(r[i] + 0x56u) = (u16)(1u + i);
        DSD(r[i] + 0x08u) = 0x00ABCDEFu;
        DSD(r[i] + 0x24u) = 0x11111111u;
        DSW(r[i] + 0x34u) = 0x3434u;
        DSW(r[i] + 0x36u) = 0x3636u;
        DSB(r[i] + 0x42u) = 0x42u;
        DSB(r[i] + 0x43u) = 0x43u;
        DSW(r[i] + 0x44u) = 0x4444u;
        DSB(r[i] + 0x53u) = 0x35u;
        DSD(FIGHT_ACTORS + (1u + i) * 0x20u + 0x18u) = FZ_SRC + i * 0x10u;
        DSD(FIGHT_ACTORS + (1u + i) * 0x20u + 0x1Cu) = FZ_SRC + 0x40u + i * 0x10u;
        DSW(FIGHT_ACTORS + (1u + i) * 0x20u + 2u) = 0x1234u;
        DSD(s[i] + 0x10u) = 0x10101010u;
        DSD(s[i] + 0x14u) = 0x14141414u;
        DSD(s[i] + 0x18u) = 0x18181818u;
        DSD(s[i] + 0x1Cu) = 0x1C1C1C1Cu;
        DSD(s[i] + 0x2Cu) = 0x2C2Cu;
        DSB(s[i] + 0x42u) = 0x02u;
        DSB(s[i] + 0x43u) = 0xFBu;
        DSB(s[i] + 0x52u) = 0x55u;
        DSB(s[i] + 0x53u) = 0x66u;
        DSB(s[i] + 0x54u) = 0x77u;
        DSB(s[i] + 0x58u) = 0x88u;
        DSB(s[i] + 0x7Au) = (u8)(2u * i);
    }
    DSW(DS_0010474C) = 0x4C4Cu;
    DSW(DS_0010474C + 2u) = 0x4E4Eu;
    DSB(DS_0010476C) = 0x6Cu;
    DSB(DS_0010476C + 1u) = 0x6Du;
    mem_fill(FIGHT_SNAP_235C4, 0xEEu, 0x1F8u);
    DSD(DS_00107D28) = 0xD28D28u;
    DSB(DS_0009AF3D) = 0;
    DSD(DS_000FCCE0) = DS_000FCCE0;
    DSD(DS_000FCCE0 + 4u) = DS_000FCCE0;
    DSD(DS_000FCCE8) = DS_000FCCE8;
    DSD(DS_000FCCE8 + 4u) = DS_000FCCE8;
    DSW(DS_001078F6) = 0;
    DSD(DS_001014F0) = 0;                    /* res_resolve: every handle NULL */
    DSB(DS_00104B14) = 1u;                   /* 0x399AC: no 0x4F434 in 0x39834 */
}

/* Both sides' snapshot halves as copies of the live slots and records (valid
 * record pointers), side 0's marked by +0x52 = 0x0C and char 2's 0xA8A98 row
 * on FZ_TAB with handle 0: a 0x33B00 restore neither faults nor acquires. */
static void fz_snap_live(u32 r0, u32 r1)
{
    u32 s0 = DS_001077B0, s1 = DS_001077B0 + 0x94u;
    memcpy(mem + FIGHT_SNAP_235C4, mem + s0, 0x94u);
    memcpy(mem + FIGHT_SNAP_235C4 + 0x94u, mem + s1, 0x94u);
    memcpy(mem + FIGHT_SNAP_235C4 + 0x128u, mem + r0, 0x68u);
    memcpy(mem + FIGHT_SNAP_235C4 + 0x190u, mem + r1, 0x68u);
    DSB(FIGHT_SNAP_235C4 + 0x52u) = 0x0Cu;
    DSD(DS_000A8A98 + 8u) = FZ_TAB;
    DSD(FZ_TAB) = 0;
    DSD(FZ_TAB + 4u) = 0;
}

/* One free effect record FZ_EFX on the free list (0x13C70 pops it). */
static void fz_one_free(void)
{
    DSD(DS_000FCCE8) = FZ_EFX;
    DSD(DS_000FCCE8 + 4u) = FZ_EFX;
    DSD(FZ_EFX) = DS_000FCCE8;
    DSD(FZ_EFX + 4u) = DS_000FCCE8;
    mem_fill(FZ_EFX + 8u, 0xEEu, 0x10u);
}

/* Record §41-C. 0x235C4 (EAX = side; 0x3B65E is its only caller): the 0x33A10
 * context, 0x33ACC (slot[side]'s 0x94 bytes to 0x104530 + side * 0x94, its
 * record's 0x68 to 0x104658 + side * 0x68), 0x39834(side, 0x2A) (which stores
 * the reaction in DS_00107D28 at 0x39953), +0x52/+0x53 = 0x10/0x0A, +0x10 =
 * 0x22BEC, +0x18/+0x1C = 0, then 0x22B28: 0x13C70(pset[rec+0x56]+0x18, 1,
 * 0x105FDB0), +0x14 = 0x29D04, word 0x10474C[side] = 0, 0x3C16C/0x3C148 (the
 * record's +0x36/+0x44 and +0x34/+0x42/+0x43), the record's +0x24 = 0, +0x58
 * = 1, +0x43 &= 0xFD, 0x10476C[side] = (the other slot's +0x53 == 0x0A).
 * 0x22BEC (EBX = side): the tick, then +0x58 1 (hold past 0x78, signed, two
 * ticks while 0x10476C[side]) and 2 (0x35050, 0x33B00 from the snapshot,
 * 0x188DC with the pre-restore x, 0x39280, +0x14 = 0x29D04 when still
 * pending); 0, 3 and > 3 only tick. 0x29D04 (EAX = slot): 0 while DS_0009AF3D,
 * else 0x2A17C(rec, 0, DSD(DSD(0xA8A98 + char * 4) + 0x105B34[side] * 4)) and
 * 1. 0x370F0 (EAX = rec): the DS_001077A8 gates, +0x54 = 3, +0x42 |= 4, +0x52
 * = 0x0A, then 0xBDC2C[char] at 1.0 (DS_00104B14 set) or the other slot's
 * +0x42 |= 0x40, the RECORD's +0x53 = 0 and DS_000F0AFE = 2. */
static void check_freeze_235c4(void)
{
    u32 s0 = DS_001077B0, s1 = DS_001077B0 + 0x94u;
    u32 r0 = FZ_POOL, r1 = FZ_POOL + ACTOR_REC_SIZE;
    u32 pset1 = FIGHT_ACTORS + 0x20u, pset2 = FIGHT_ACTORS + 0x40u;
    u32 snap_s1 = FIGHT_SNAP_235C4 + 0x94u;
    u32 snap_r0 = FIGHT_SNAP_235C4 + 0x128u, snap_r1 = snap_r0 + 0x68u;
    u8 pre_s[0x94], pre_r[0x68];
    u8 sv_slots[0x128], sv_7a8[8], sv_snap[0x1F8], sv_474c[4], sv_476c[2];
    u8 sv_cce0[16], sv_af3c[2], sv_pal[0x184], sv_a98[12], sv_b34[2];
    u8 sv_7d[0x1C8], sv_b00[4], sv_af0[8], sv_8e0[4], sv_b5a[6];
    u8 sv_78fa = DSB(DS_001078FA);
    u32 sv_14ec = DSD(DS_001014EC), sv_14f0 = DSD(DS_001014F0);
    u32 sv_14f4 = DSD(DS_001014F4), sv_dc2c = DSD(0x000BDC2Cu + 8u);
    u16 sv_78f6 = DSW(DS_001078F6);
    u8 sv_4b14 = DSB(DS_00104B14), sv_afe = DSB(DS_000F0AFE);
    typedef void (*anim_fn)(u32 rec, u32 arg);
    anim_fn f370;

    tf_snap(sv_slots, DS_001077B0, 0x128u);
    tf_snap(sv_7a8, DS_001077A8, 8u);
    tf_snap(sv_snap, FIGHT_SNAP_235C4, 0x1F8u);
    tf_snap(sv_474c, DS_0010474C, 4u);
    tf_snap(sv_476c, DS_0010476C, 2u);
    tf_snap(sv_cce0, DS_000FCCE0, 16u);
    tf_snap(sv_af3c, DS_0009AF3C, 2u);
    tf_snap(sv_pal, DS_00107618, 0x184u);
    tf_snap(sv_a98, DS_000A8A98, 12u);
    tf_snap(sv_b34, DS_00105B34, 2u);
    tf_snap(sv_7d, DS_00107D20, 0x1C8u);   /* 0x107D20..0x107EE7 */
    tf_snap(sv_8e0, DS_001088E0, 4u);
    tf_snap(sv_b5a, DS_00100B5A, 6u);
    tf_snap(sv_b00, DS_00104B00, 4u);
    tf_snap(sv_af0, DS_00100AF0, 8u);

    CHECK(fn_resolve(0x22BECu) == (void (*)(void))fighter_22bec,
          "0x22BEC is registered as fighter_22bec");
    CHECK(fn_resolve(0x29D04u) == (void (*)(void))fighter_29d04,
          "0x29D04 is registered as fighter_29d04");
    f370 = (anim_fn)(void *)fn_resolve(0x370F0u);
    CHECK(f370 != NULL, "0x370F0 is a registered stream target");

    /* A: 0x235C4 for side 1, the other slot at +0x53 = 0x0A, one free effect. */
    fz_seed(r0, r1);
    fz_one_free();
    DSB(s0 + 0x53u) = 0x0Au;
    memcpy(pre_s, mem + s1, sizeof pre_s);
    memcpy(pre_r, mem + r1, sizeof pre_r);
    fighter_235c4(1u);
    CHECK(memcmp(mem + snap_s1, pre_s, sizeof pre_s) == 0,
          "0x33ACC copied slot 1 to 0x104530 + 0x94");
    CHECK(memcmp(mem + snap_r1, pre_r, sizeof pre_r) == 0,
          "0x33ACC copied record 1 to 0x104658 + 0x68");
    CHECK_EQ_INT((int)DSB(FIGHT_SNAP_235C4), 0xEE);            /* side 0 slot */
    CHECK_EQ_INT((int)DSB(FIGHT_SNAP_235C4 + 0x93u), 0xEE);
    CHECK_EQ_INT((int)DSB(snap_r0), 0xEE);                     /* side 0 rec */
    CHECK_EQ_INT((int)DSB(snap_r0 + 0x67u), 0xEE);
    CHECK_EQ_INT((int)DSD(DS_00107D28), 0x2A);                 /* 0x39953 */
    CHECK_EQ_INT((int)DSB(s1 + 0x52u), 0x10);
    CHECK_EQ_INT((int)DSB(s1 + 0x53u), 0x0A);
    CHECK_EQ_INT((int)DSD(s1 + 0x10u), 0x00022BEC);
    CHECK_EQ_INT((int)DSD(s1 + 0x18u), 0);
    CHECK_EQ_INT((int)DSD(s1 + 0x1Cu), 0);
    CHECK_EQ_INT((int)DSD(s1 + 0x14u), 0x00029D04);
    CHECK_EQ_INT((int)DSB(s1 + 0x58u), 1);
    CHECK_EQ_INT((int)DSB(s1 + 0x43u), 0xF9);                  /* 0xFB & 0xFD */
    CHECK_EQ_INT((int)DSB(s1 + 0x54u), 0x77);
    CHECK_EQ_INT((int)DSD(r1 + 0x24u), 0);
    CHECK_EQ_INT((int)DSW(r1 + 0x34u), 0);
    CHECK_EQ_INT((int)DSW(r1 + 0x36u), 0);
    CHECK_EQ_INT((int)DSB(r1 + 0x42u), 0);
    CHECK_EQ_INT((int)DSB(r1 + 0x43u), 0);
    CHECK_EQ_INT((int)DSW(r1 + 0x44u), 0);
    CHECK_EQ_INT((int)DSW(DS_0010474C + 2u), 0);
    CHECK_EQ_INT((int)DSW(DS_0010474C), 0x4C4C);
    CHECK_EQ_INT((int)DSB(DS_0010476C + 1u), 1);
    CHECK_EQ_INT((int)DSB(DS_0010476C), 0x6C);
    CHECK_EQ_INT((int)DSD(DS_000FCCE0), (int)FZ_EFX);          /* 0x13C70 */
    CHECK_EQ_INT((int)DSD(FZ_EFX + 8u), (int)(FZ_SRC + 0x10u));
    CHECK_EQ_INT((int)DSB(FZ_EFX + 0x0Du), 1);
    CHECK_EQ_INT((int)DSB(DS_0009AF3D), 1);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 0x55);                  /* side 0 kept */
    CHECK_EQ_INT((int)DSD(s0 + 0x10u), 0x10101010);
    CHECK_EQ_INT((int)DSD(r0 + 0x24u), 0x11111111);
    /* A2: side 0, the other slot's +0x53 = 0x66, no free effect: side 0's
     * snapshot halves, 0x10476C[0] = 0, and no spawn. */
    fz_seed(r0, r1);
    memcpy(pre_s, mem + s0, sizeof pre_s);
    memcpy(pre_r, mem + r0, sizeof pre_r);
    fighter_235c4(0u);
    CHECK(memcmp(mem + FIGHT_SNAP_235C4, pre_s, sizeof pre_s) == 0,
          "0x33ACC copied slot 0 to 0x104530");
    CHECK(memcmp(mem + snap_r0, pre_r, sizeof pre_r) == 0,
          "0x33ACC copied record 0 to 0x104658");
    CHECK_EQ_INT((int)DSB(snap_s1), 0xEE);
    CHECK_EQ_INT((int)DSB(snap_r1 + 0x67u), 0xEE);
    CHECK_EQ_INT((int)DSB(DS_0010476C), 0);
    CHECK_EQ_INT((int)DSB(DS_0010476C + 1u), 0x6D);
    CHECK_EQ_INT((int)DSW(DS_0010474C), 0);
    CHECK_EQ_INT((int)DSW(DS_0010474C + 2u), 0x4E4E);
    CHECK_EQ_INT((int)DSD(DS_000FCCE0), (int)DS_000FCCE0);
    CHECK_EQ_INT((int)DSB(DS_0009AF3D), 0);
    CHECK_EQ_INT((int)DSD(s0 + 0x10u), 0x00022BEC);
    CHECK_EQ_INT((int)DSD(s1 + 0x10u), 0x10101010);

    /* B: 0x22BEC for side 1 through 0x3531C case 10 (+0x53 = 0x0A): phase 0
     * only ticks. */
    fz_seed(r0, r1);
    fz_snap_live(r0, r1);
    DSB(s1 + 0x53u) = 0x0Au;
    DSD(s1 + 0x10u) = 0x00022BECu;
    DSB(s1 + 0x58u) = 0;
    DSW(DS_0010474C + 2u) = 5u;
    fighter_state_3531c(1u);
    CHECK_EQ_INT((int)DSW(DS_0010474C + 2u), 6);
    CHECK_EQ_INT((int)DSW(DS_0010474C), 0x4C4C);
    CHECK_EQ_INT((int)DSB(s1 + 0x58u), 0);
    /* B1: phase 1 with 0x10476C[1] = 0: 0x77 -> 0x78 holds, 0x78 -> 0x79
     * moves to phase 2; 0x7FFF -> 0x8000 is negative and holds. */
    DSB(DS_0010476C + 1u) = 0;
    DSB(s1 + 0x58u) = 1u;
    DSW(DS_0010474C + 2u) = 0x77u;
    fighter_22bec(s0, 1u);
    CHECK_EQ_INT((int)DSW(DS_0010474C + 2u), 0x78);
    CHECK_EQ_INT((int)DSB(s1 + 0x58u), 1);
    fighter_22bec(s0, 1u);
    CHECK_EQ_INT((int)DSW(DS_0010474C + 2u), 0x79);
    CHECK_EQ_INT((int)DSB(s1 + 0x58u), 2);
    DSB(s1 + 0x58u) = 1u;
    DSW(DS_0010474C + 2u) = 0x7FFFu;
    fighter_22bec(s0, 1u);
    CHECK_EQ_INT((int)DSW(DS_0010474C + 2u), 0x8000);
    CHECK_EQ_INT((int)DSB(s1 + 0x58u), 1);
    /* B2: 0x10476C[1] set: two ticks, 0x77 -> 0x79, phase 2; side 0's byte
     * is not the gate. */
    DSB(DS_0010476C + 1u) = 1u;
    DSB(DS_0010476C) = 0;
    DSW(DS_0010474C + 2u) = 0x77u;
    fighter_22bec(s0, 1u);
    CHECK_EQ_INT((int)DSW(DS_0010474C + 2u), 0x79);
    CHECK_EQ_INT((int)DSB(s1 + 0x58u), 2);
    DSB(DS_0010476C + 1u) = 0;
    DSB(DS_0010476C) = 1u;
    DSB(s1 + 0x58u) = 1u;
    DSW(DS_0010474C + 2u) = 0x77u;
    fighter_22bec(s0, 1u);
    CHECK_EQ_INT((int)DSW(DS_0010474C + 2u), 0x78);
    CHECK_EQ_INT((int)DSB(s1 + 0x58u), 1);
    /* B3: phases 3 and 5 only tick. */
    DSB(DS_0010476C + 1u) = 1u;
    DSB(s1 + 0x58u) = 3u;
    DSW(DS_0010474C + 2u) = 0x100u;
    fighter_22bec(s0, 1u);
    CHECK_EQ_INT((int)DSW(DS_0010474C + 2u), 0x101);
    CHECK_EQ_INT((int)DSB(s1 + 0x58u), 3);
    CHECK_EQ_INT((int)DSD(s1 + 0x14u), 0x14141414);
    CHECK_EQ_INT((int)DSB(s1 + 0x52u), 0x55);
    DSB(s1 + 0x58u) = 5u;
    fighter_22bec(s0, 1u);
    CHECK_EQ_INT((int)DSW(DS_0010474C + 2u), 0x102);
    CHECK_EQ_INT((int)DSB(s1 + 0x58u), 5);
    CHECK_EQ_INT((int)DSD(s1 + 0x2Cu), 0x2C2C);

    /* B4: phase 2, the +0x14 callback 0x29D04 still pending (a live effect):
     * 0x33B00 restores slot 1 from the snapshot (+0x52/+0x10/+0x14 from it,
     * +0x5A kept), 0x188DC re-anchors the live x 0x4000 (not the snapshot's
     * 0x1000), 0x39280 clears +0x5D, and +0x14 is re-armed. Side 0's half
     * (+0x52 = 0x0C) is not the source. */
    fz_seed(r0, r1);
    fz_snap_live(r0, r1);
    DSB(snap_s1 + 0x52u) = 9u;
    DSB(snap_s1 + 0x53u) = 0;
    DSD(snap_s1 + 0x10u) = 0;
    DSD(snap_s1 + 0x14u) = 0x13579BDFu;
    DSD(snap_s1 + 0x2Cu) = 0x1000u;
    DSD(snap_r1 + 0x24u) = 0x40400000u;
    DSB(s1 + 0x42u) = 0x08u;                 /* 0x18714: x kept */
    DSD(s1 + 0x14u) = 0x00029D04u;
    DSD(s1 + 0x2Cu) = 0x4000u;
    DSB(s1 + 0x58u) = 2u;
    DSB(s1 + 0x5Au) = 0x20u;
    DSB(s1 + 0x5Du) = 0x22u;
    DSD(r1 + 0x24u) = 0;
    DSB(DS_0009AF3D) = 1u;
    fighter_22bec(s0, 1u);
    CHECK_EQ_INT((int)DSB(s1 + 0x52u), 9);
    CHECK_EQ_INT((int)DSD(s1 + 0x10u), 0);
    CHECK_EQ_INT((int)DSD(s1 + 0x14u), 0x00029D04);
    CHECK_EQ_INT((int)DSD(s1 + 0x2Cu), 0x4000);
    CHECK_EQ_INT((int)DSB(s1 + 0x5Au), 0x20);
    CHECK_EQ_INT((int)DSB(s1 + 0x5Du), 0);
    CHECK_EQ_INT((int)DSD(r1 + 0x24u), 0x40400000);
    CHECK_EQ_INT((int)DSW(DS_0010474C + 2u), 0x4E4F);
    /* B5: no live effect: 0x29D04 returns 1, so 0x35050 clears +0x14 and the
     * restored snapshot value stays. */
    DSD(s1 + 0x14u) = 0x00029D04u;
    DSD(s1 + 0x2Cu) = 0x4000u;
    DSB(s1 + 0x52u) = 0x55u;
    DSB(s1 + 0x58u) = 2u;
    DSB(DS_0009AF3D) = 0;
    fighter_22bec(s0, 1u);
    CHECK_EQ_INT((int)DSD(s1 + 0x14u), 0x13579BDF);
    CHECK_EQ_INT((int)DSB(s1 + 0x52u), 9);

    /* C: 0x29D04 directly on slot 1 (char 2, 0x105B34[1] = 1): a live effect
     * returns 0 and touches nothing; with none, the row's entry 1 handle, held
     * by palette entry 0, replaces FZ_OLD in pset+0x18 (0x2A17C). */
    fz_seed(r0, r1);
    DSD(DS_000A8A98 + 8u) = FZ_TAB;
    DSD(FZ_TAB) = 0;
    DSD(FZ_TAB + 4u) = FZ_HANDLE;
    DSB(DS_00105B34) = 0;
    DSB(DS_00105B34 + 1u) = 1u;
    DSD(DS_00107618) = FZ_HANDLE;
    DSD(DS_00107618 + 4u) = 5u;
    DSD(FZ_OLD + 4u) = 2u;
    DSD(pset2 + 0x18u) = FZ_OLD;
    DSB(DS_0009AF3D) = 2u;
    CHECK_EQ_INT((int)fighter_29d04(s1), 0);
    CHECK_EQ_INT((int)DSW(pset2 + 2u), 0x1234);
    CHECK_EQ_INT((int)DSD(pset2 + 0x18u), (int)FZ_OLD);
    DSB(DS_0009AF3D) = 0;
    CHECK_EQ_INT((int)fighter_29d04(s1), 1);
    CHECK_EQ_INT((int)DSW(pset2 + 2u), 0);
    CHECK_EQ_INT((int)DSD(pset2 + 0x18u), (int)DS_00107618);
    CHECK_EQ_INT((int)DSD(DS_00107618 + 4u), 6);
    CHECK_EQ_INT((int)DSD(FZ_OLD + 4u), 1);
    CHECK_EQ_INT((int)DSW(pset1 + 2u), 0x1234);

    /* D: 0x370F0 on record 1 (through the registered stream target). Either
     * DS_001077A8 slot missing returns at once. */
    fz_seed(r0, r1);
    DSD(DS_001077A8 + 4u) = 0;
    if (f370 != NULL) f370(r1, 0xFFFFu);
    CHECK_EQ_INT((int)DSB(s1 + 0x54u), 0x77);
    DSD(DS_001077A8 + 4u) = s1;
    DSD(DS_001077A8) = 0;
    if (f370 != NULL) f370(r1, 0xFFFFu);
    CHECK_EQ_INT((int)DSB(s1 + 0x54u), 0x77);
    CHECK_EQ_INT((int)DSB(s1 + 0x52u), 0x55);
    /* D2: DS_00104B14 = 0: the other slot's +0x42 bit 6, the record's +0x53
     * (not the slot's) = 0 and DS_000F0AFE = 2. */
    DSD(DS_001077A8) = s0;
    DSB(DS_00104B14) = 0;
    DSB(DS_000F0AFE) = 0x99u;
    if (f370 != NULL) f370(r1, 0xFFFFu);
    CHECK_EQ_INT((int)DSB(s1 + 0x54u), 3);
    CHECK_EQ_INT((int)DSB(s1 + 0x42u), 0x06);
    CHECK_EQ_INT((int)DSB(s1 + 0x52u), 0x0A);
    CHECK_EQ_INT((int)DSB(s0 + 0x42u), 0x42);
    CHECK_EQ_INT((int)DSB(r1 + 0x53u), 0);
    CHECK_EQ_INT((int)DSB(s1 + 0x53u), 0x66);
    CHECK_EQ_INT((int)DSB(DS_000F0AFE), 2);
    CHECK_EQ_INT((int)DSD(r1 + 0x08u), 0x00ABCDEF);
    /* D3: DS_00104B14 set: 0xBDC2C[slot 1's char 2] at 1.0 instead. */
    fz_seed(r0, r1);
    DSD(0x000BDC2Cu + 8u) = FZ_ST;
    DSW(FZ_ST) = 0x1321u;
    DSB(DS_00104B14) = 1u;
    DSB(DS_000F0AFE) = 0x99u;
    fighter_370f0(r1);
    CHECK_EQ_INT((int)DSD(r1 + 0x08u), (int)FZ_ST);
    CHECK_EQ_INT((int)DSD(r1 + 0x24u), 0x3F800000);
    CHECK_EQ_INT((int)(DSW(pset2) & 0x7FFFu), 0x1321);
    CHECK_EQ_INT((int)DSB(s1 + 0x54u), 3);
    CHECK_EQ_INT((int)DSB(s1 + 0x42u), 0x06);
    CHECK_EQ_INT((int)DSB(s1 + 0x52u), 0x0A);
    CHECK_EQ_INT((int)DSB(s0 + 0x42u), 0x02);
    CHECK_EQ_INT((int)DSB(DS_000F0AFE), 0x99);

    tf_put(sv_af0, DS_00100AF0, 8u);
    tf_put(sv_b00, DS_00104B00, 4u);
    tf_put(sv_7d, DS_00107D20, 0x1C8u);
    tf_put(sv_8e0, DS_001088E0, 4u);
    tf_put(sv_b5a, DS_00100B5A, 6u);
    DSB(DS_001078FA) = sv_78fa;
    tf_put(sv_b34, DS_00105B34, 2u);
    tf_put(sv_a98, DS_000A8A98, 12u);
    tf_put(sv_pal, DS_00107618, 0x184u);
    tf_put(sv_af3c, DS_0009AF3C, 2u);
    tf_put(sv_cce0, DS_000FCCE0, 16u);
    tf_put(sv_476c, DS_0010476C, 2u);
    tf_put(sv_474c, DS_0010474C, 4u);
    tf_put(sv_snap, FIGHT_SNAP_235C4, 0x1F8u);
    tf_put(sv_7a8, DS_001077A8, 8u);
    tf_put(sv_slots, DS_001077B0, 0x128u);
    DSD(DS_001014EC) = sv_14ec;
    DSD(DS_001014F0) = sv_14f0;
    DSD(DS_001014F4) = sv_14f4;
    DSD(0x000BDC2Cu + 8u) = sv_dc2c;
    DSW(DS_001078F6) = sv_78f6;
    DSB(DS_00104B14) = sv_4b14;
    DSB(DS_000F0AFE) = sv_afe;
}

/* ---- §42-A: update-table entry 7 0x2910C and character 1's second freeze -- */

#define Q42_POOL  (FIGHT_RECS + 0x6000u)  /* 0x2910C's records, 0x68 apart */
#define Q42_RECS  0x6400u                 /* FIGHT_RECS..+0x6400: every scratch record */
#define Q42_ACTS  0x100u                  /* FIGHT_ACTORS psets 0..7 */
#define Q42_NODE(i) (DS_00104780 + (u32)(i) * 0x10u)
#define Q42_104728 0x00104728u
#define Q42_104750 0x00104750u
#define Q42_10476A 0x0010476Au
#define Q42_DUMMY0 (FIGHT_RECS + 0x6300u) /* 0x104728's sentinels: scratch */
#define Q42_DUMMY1 (FIGHT_RECS + 0x6380u) /* records, so a stray write lands */

/* The whole data object and the scratch records/psets, saved before and put
 * back after each §42-A group. */
static u8 s_q42_data[0x10B0D0u - 0x80000u];
static u8 s_q42_recs[Q42_RECS];
static u8 s_q42_acts[Q42_ACTS];

static void q42_save(void)
{
    tf_snap(s_q42_data, 0x80000u, sizeof s_q42_data);
    tf_snap(s_q42_recs, FIGHT_RECS, Q42_RECS);
    tf_snap(s_q42_acts, FIGHT_ACTORS, Q42_ACTS);
}

static void q42_restore(void)
{
    tf_put(s_q42_acts, FIGHT_ACTORS, Q42_ACTS);
    tf_put(s_q42_recs, FIGHT_RECS, Q42_RECS);
    tf_put(s_q42_data, 0x80000u, sizeof s_q42_data);
}

/* Both type-0x0A/0x19 sentinels self-linked and the pool/psets pointed at
 * Q42_POOL/FIGHT_ACTORS. */
static void q42_lists(void)
{
    DSD(DS_00104880) = DS_00104880;
    DSD(DS_00104884) = DS_00104880;
    DSD(DS_00104888) = DS_00104888;
    DSD(DS_0010488C) = DS_00104888;
    DSD(DS_001014F4) = Q42_POOL;
    DSD(DS_001014EC) = FIGHT_ACTORS;
    mem_fill(FIGHT_ACTORS, 0, Q42_ACTS);
}

/* Append `node` to the list whose sentinel is `sent` (0x249C0's shape). */
static void q42_append(u32 sent, u32 node)
{
    u32 prev = DSD(sent + 4u);
    DSD(node) = sent;
    DSD(node + 4u) = prev;
    DSD(prev) = node;
    DSD(sent + 4u) = node;
}

/* Record i of Q42_POOL on node `node` (in the in-use list, phase `ph`), with
 * rec+0x14 = `link`, y = rec+0x1C, the words +0x34/+0x36, and every field the
 * landing or a stream start overwrites on a sentinel. */
static u32 q42_rec(u32 i, u32 node, u32 link, u8 ph, u32 y, u16 vx, u16 vy)
{
    u32 rec = Q42_POOL + i * ACTOR_REC_SIZE;
    mem_fill(rec, 0, ACTOR_REC_SIZE);
    DSW(rec + 0x56u) = (u16)i;
    DSD(rec + 0x08u) = 0x00ABCDEFu;
    DSD(rec + 0x14u) = link;
    DSD(rec + 0x1Cu) = y;
    DSD(rec + 0x24u) = 0x11111111u;
    DSW(rec + 0x34u) = vx;
    DSW(rec + 0x36u) = vy;
    DSW(rec + 0x44u) = 0x4444u;
    DSB(rec + 0x48u) = 0x0Au;
    DSB(rec + 0x59u) = 0x59u;
    DSD(node + 8u) = rec;
    DSB(node + 0x0Cu) = ph;
    q42_append(DS_00104880, node);
    return rec;
}

/* Record §42-A. 0x2910C (update-table entry 7, through its registration):
 * the walk over 0x104880 with the next node read first; a landing (rec+0x1C
 * plus the SIGNED word +0x36, a 32-bit sum, below 0) zeroes +0x1C/+0x34/+0x36/
 * +0x44/+0x48, sets +0x59 = 0xFE, starts 0xE8D50 at 3.0 and moves rec+0x14's
 * node to the free-list head (skipped when +0x14 is 0); otherwise the phase
 * byte picks 0xE8D14 (0: |+0x36| < |+0x34|), 0xE8D28 (1: |+0x36| > |+0x34|),
 * 0xE8D3C (2: |+0x36| > 2|+0x34|) at 2.0; 3 and above hold. Every stream
 * starts with a 0xCD40 word: opcode 0x0D stops 0x2BC30's pre-walk and 0x2A408
 * reads it as a variable sprite whose operand word follows, leaving rec+0x08 =
 * the stream + 2. Also 0x2A148, which 0x22E44 calls. */
static void check_type_0a19_update(void)
{
    typedef void (*proc_fn)(void);
    proc_fn walk;
    u32 A = Q42_NODE(0), B = Q42_NODE(1), C = Q42_NODE(2), D = Q42_NODE(3);
    u32 E = Q42_NODE(4), F = Q42_NODE(5), G = Q42_NODE(6), H = Q42_NODE(7);
    u32 r0, r1, r2, r3, r4, r5, r6;

    q42_save();
    walk = (proc_fn)fn_resolve(0x2910Cu);
    CHECK(walk != NULL, "0x2910C (update-table entry 7) is registered");
    CHECK_EQ_INT((int)DSD(0x000A8644u + 7u * 4u), 0x0002910C);
    CHECK_EQ_INT((int)DSW(0x000E8D50u), 0xCD40);
    CHECK_EQ_INT((int)DSW(0x000E8D14u), 0xCD40);
    CHECK_EQ_INT((int)DSW(0x000E8D28u), 0xCD40);
    CHECK_EQ_INT((int)DSW(0x000E8D3Cu), 0xCD40);

    /* A: A lands (+0x34 positive, the signed +0x36 -0x20 against y 0x10),
     * B sits at the 0 edge and moves to phase 1, C holds on |vy| == |vx|, D
     * holds on |0x7FFF| < |0x8000| (32768 in 32 bits), E moves 2 -> 3. The
     * free list holds F alone. */
    q42_lists();
    r0 = q42_rec(0, A, A, 2u, 0x10u, 0x1234u, 0xFFE0u);
    r1 = q42_rec(1, B, B, 0u, 0x20u, 0x0021u, 0xFFE0u);
    r2 = q42_rec(2, C, C, 0u, 0x100u, 0xFFC0u, 0x0040u);
    r3 = q42_rec(3, D, D, 1u, 0x100u, 0x8000u, 0x7FFFu);
    r4 = q42_rec(4, E, E, 2u, 0x100u, 0x0010u, 0xFFDFu);
    q42_append(DS_00104888, F);
    if (walk != NULL) walk();
    CHECK_EQ_INT((int)DSD(r0 + 0x1Cu), 0);
    CHECK_EQ_INT((int)DSW(r0 + 0x34u), 0);
    CHECK_EQ_INT((int)DSW(r0 + 0x36u), 0);
    CHECK_EQ_INT((int)DSW(r0 + 0x44u), 0);
    CHECK_EQ_INT((int)DSB(r0 + 0x48u), 0);
    CHECK_EQ_INT((int)DSB(r0 + 0x59u), 0xFE);
    CHECK_EQ_INT((int)DSD(r0 + 0x08u), 0x000E8D52);
    CHECK_EQ_INT((int)DSD(r0 + 0x24u), 0x40400000);
    CHECK_EQ_INT((int)DSD(r0 + 0x14u), 0);
    CHECK_EQ_INT((int)DSD(DS_00104888), (int)A);        /* the free-list head */
    CHECK_EQ_INT((int)DSD(A), (int)F);
    CHECK_EQ_INT((int)DSD(A + 4u), (int)DS_00104888);
    CHECK_EQ_INT((int)DSD(F + 4u), (int)A);
    CHECK_EQ_INT((int)DSD(DS_0010488C), (int)F);
    CHECK_EQ_INT((int)DSD(A + 8u), (int)r0);
    CHECK_EQ_INT((int)DSB(A + 0x0Cu), 2);
    CHECK_EQ_INT((int)DSD(DS_00104880), (int)B);        /* the in-use list */
    CHECK_EQ_INT((int)DSD(B + 4u), (int)DS_00104880);
    CHECK_EQ_INT((int)DSD(E), (int)DS_00104880);
    CHECK_EQ_INT((int)DSD(DS_00104884), (int)E);
    CHECK_EQ_INT((int)DSD(r1 + 0x08u), 0x000E8D16);
    CHECK_EQ_INT((int)DSD(r1 + 0x24u), 0x40000000);
    CHECK_EQ_INT((int)DSB(B + 0x0Cu), 1);
    CHECK_EQ_INT((int)DSD(r1 + 0x1Cu), 0x20);
    CHECK_EQ_INT((int)DSB(r1 + 0x59u), 0x59);
    CHECK_EQ_INT((int)DSD(r1 + 0x14u), (int)B);
    CHECK_EQ_INT((int)DSD(r2 + 0x08u), 0x00ABCDEF);
    CHECK_EQ_INT((int)DSD(r2 + 0x24u), 0x11111111);
    CHECK_EQ_INT((int)DSB(C + 0x0Cu), 0);
    CHECK_EQ_INT((int)DSD(r3 + 0x08u), 0x00ABCDEF);
    CHECK_EQ_INT((int)DSB(D + 0x0Cu), 1);
    CHECK_EQ_INT((int)DSD(r4 + 0x08u), 0x000E8D3E);
    CHECK_EQ_INT((int)DSD(r4 + 0x24u), 0x40000000);
    CHECK_EQ_INT((int)DSB(E + 0x0Cu), 3);

    /* A2: B moves 1 -> 2 (|0x11| > |0x10|), C holds at 2 (|0x20| == 2|0x10|),
     * D (3) and E (5) hold, A lands from y -0x100 + 0xFF into an empty free
     * list, G lands with rec+0x14 = 0 (its node stays in the in-use list and
     * the free list is not touched for it), and H holds at 1 on |vy| == |vx|. */
    q42_lists();
    r0 = q42_rec(0, A, A, 1u, 0xFFFFFF00u, 0x0000u, 0x00FFu);
    r1 = q42_rec(1, B, B, 1u, 0x100u, 0x0010u, 0x0011u);
    r2 = q42_rec(2, C, C, 2u, 0x100u, 0xFFF0u, 0x0020u);
    r3 = q42_rec(3, D, D, 3u, 0x100u, 0x0000u, 0x0100u);
    r4 = q42_rec(4, E, E, 5u, 0x100u, 0x0000u, 0x0100u);
    r5 = q42_rec(5, G, 0u, 0u, 0xFFFFFFFFu, 0x0000u, 0x0000u);
    r6 = q42_rec(6, H, H, 1u, 0x100u, 0x0030u, 0xFFD0u);
    if (walk != NULL) walk();
    CHECK_EQ_INT((int)DSD(r0 + 0x08u), 0x000E8D52);
    CHECK_EQ_INT((int)DSD(DS_00104888), (int)A);
    CHECK_EQ_INT((int)DSD(A), (int)DS_00104888);
    CHECK_EQ_INT((int)DSD(DS_0010488C), (int)A);
    CHECK_EQ_INT((int)DSD(r1 + 0x08u), 0x000E8D2A);
    CHECK_EQ_INT((int)DSB(B + 0x0Cu), 2);
    CHECK_EQ_INT((int)DSD(r2 + 0x08u), 0x00ABCDEF);
    CHECK_EQ_INT((int)DSB(C + 0x0Cu), 2);
    CHECK_EQ_INT((int)DSD(r3 + 0x08u), 0x00ABCDEF);
    CHECK_EQ_INT((int)DSB(D + 0x0Cu), 3);
    CHECK_EQ_INT((int)DSD(r4 + 0x08u), 0x00ABCDEF);
    CHECK_EQ_INT((int)DSB(E + 0x0Cu), 5);
    CHECK_EQ_INT((int)DSD(r5 + 0x08u), 0x000E8D52);
    CHECK_EQ_INT((int)DSB(r5 + 0x59u), 0xFE);
    CHECK_EQ_INT((int)DSD(r5 + 0x1Cu), 0);
    CHECK_EQ_INT((int)DSD(E), (int)G);
    CHECK_EQ_INT((int)DSD(G), (int)H);
    CHECK_EQ_INT((int)DSD(DS_00104884), (int)H);
    CHECK_EQ_INT((int)DSD(DS_00104880), (int)B);
    CHECK_EQ_INT((int)DSD(r6 + 0x08u), 0x00ABCDEF);
    CHECK_EQ_INT((int)DSB(H + 0x0Cu), 1);

    /* B: 0x2A148 stores the flag in +0x5F and the pset +2 word from +0x2E
     * with 0x800 while the flag is non-zero. */
    q42_lists();
    r0 = Q42_POOL + 2u * ACTOR_REC_SIZE;
    mem_fill(r0, 0, ACTOR_REC_SIZE);
    DSW(r0 + 0x56u) = 2u;
    DSW(r0 + 0x2Eu) = 0x0123u;
    DSB(r0 + 0x5Fu) = 0x5Fu;
    DSW(FIGHT_ACTORS + 0x40u + 2u) = 0xFFFFu;
    actor_pset_flag_5f(r0, 1u);
    CHECK_EQ_INT((int)DSB(r0 + 0x5Fu), 1);
    CHECK_EQ_INT((int)DSW(FIGHT_ACTORS + 0x40u + 2u), 0x0923);
    actor_pset_flag_5f(r0, 0u);
    CHECK_EQ_INT((int)DSB(r0 + 0x5Fu), 0);
    CHECK_EQ_INT((int)DSW(FIGHT_ACTORS + 0x40u + 2u), 0x0123);

    q42_restore();
}

/* The §41-C freeze seed plus the §42-A fields: +0x57/+0x5F/+0x74/+0x8A and
 * the 0x104728/0x104750/0x10476A arrays on sentinels (0x104728's are zeroed
 * scratch records, so a write through the wrong entry fails an assertion
 * instead of faulting). */
static void q42_fz_seed(u32 r0, u32 r1)
{
    u32 s[2], i;
    fz_seed(r0, r1);
    s[0] = DS_001077B0; s[1] = DS_001077B0 + 0x94u;
    for (i = 0; i < 2u; i++) {
        DSB(s[i] + 0x57u) = 0x57u;
        DSB(s[i] + 0x5Fu) = (u8)(0x28u + i);
        DSW(s[i] + 0x74u) = 0x7474u;
        DSB(s[i] + 0x8Au) = 0x8Au;
        DSD(s[i] + 0x0Cu) = 0x0C0C0C0Cu;
        DSB(s[i] + 0x64u) = 0x64u;
    }
    mem_fill(Q42_DUMMY0, 0, 0x100u);
    DSD(Q42_104728) = Q42_DUMMY0;
    DSD(Q42_104728 + 4u) = Q42_DUMMY1;
    DSW(Q42_104750) = 0x3333u;
    DSW(Q42_104750 + 2u) = 0x3434u;
    DSB(Q42_10476A) = 0x6Au;
    DSB(Q42_10476A + 1u) = 0x6Bu;
    /* 0x39834's per-side words (its ctx[0] = the other side): the hit count
     * DS_00107D2C (3/5: below 0xB, so 0x39865's scale reads 0xBEBF8 and
     * 0x39973's >= 0x14 store stays off) and the damage sum DS_00107D20. */
    DSW(DS_00107D2C) = 3u;
    DSW(DS_00107D2C + 2u) = 5u;
    DSW(DS_00107D20) = 0x2020u;
    DSW(DS_00107D20 + 2u) = 0x2121u;
}

/* The actor free list holding only `rec` (a zeroed pool record) and an empty
 * active list: 0x2AE14's 0x2AC80 pops it. */
static void q42_one_actor(u32 rec)
{
    mem_fill(rec, 0, ACTOR_REC_SIZE);
    DSD(DS_00105B3C) = rec;
    DSD(DS_00105B3C + 4u) = rec;
    DSD(rec) = DS_00105B3C;
    DSD(rec + 4u) = DS_00105B3C;
    DSD(DS_00105BCC) = DS_00105BCC;
    DSD(DS_00105BD0) = DS_00105BCC;
}

/* Record §42-A. 0x22CE4 (EAX = side; the 0x33A10 context): 0x33ACC,
 * 0x39834(side, the other slot's +0x5F), the other slot's +0x57 = 2, the
 * side's slot 0x10/0x0A with +0x10 0x22BEC and +0x5F 0xFF (+0x18/+0x1C
 * kept), 0x22B28, then 0x10476A[side ^ 1] = 1. 0x22E44 (+0x1C, fn(side), on
 * 0x33950): +0x57 = 2, 0x22CE4(the other side), 0x39A10(the record, 0x29A),
 * the 0xBB3E4 spawn at x -/+0x1000 (0x1A570), y 0xFFFFCC00, height +0x32,
 * into 0x104728[side] with +0x14 = the slot, +0x36 = 0x200, +0x59 = 0xFE,
 * 0x10476A[side] = 0, 0x2A148(it, 0) and DS_00104AE8 |= 0x20. */
static void check_freeze_22ce4(void)
{
    typedef void (*side_fn)(u32 side);
    side_fn f44;
    u32 s0 = DS_001077B0, s1 = DS_001077B0 + 0x94u;
    u32 r0 = FZ_POOL, r1 = FZ_POOL + ACTOR_REC_SIZE;
    u32 p = FZ_POOL + 3u * ACTOR_REC_SIZE;
    u32 snap_s1 = FIGHT_SNAP_235C4 + 0x94u;
    u32 snap_r0 = FIGHT_SNAP_235C4 + 0x128u, snap_r1 = snap_r0 + 0x68u;
    u8 pre_s[0x94], pre_r[0x68];

    q42_save();
    f44 = (side_fn)(void *)fn_resolve(0x22E44u);
    CHECK(f44 == fighter_22e44, "0x22E44 is registered as fighter_22e44");

    /* A: 0x22CE4(1): side 1 frozen on the other slot's reaction 0x28. */
    q42_fz_seed(r0, r1);
    fz_one_free();
    DSB(s0 + 0x53u) = 0x0Au;
    memcpy(pre_s, mem + s1, sizeof pre_s);
    memcpy(pre_r, mem + r1, sizeof pre_r);
    fighter_22ce4(1u);
    CHECK(memcmp(mem + snap_s1, pre_s, sizeof pre_s) == 0,
          "0x22CE4's 0x33ACC copied slot 1");
    CHECK(memcmp(mem + snap_r1, pre_r, sizeof pre_r) == 0,
          "0x22CE4's 0x33ACC copied record 1");
    CHECK_EQ_INT((int)DSB(FIGHT_SNAP_235C4), 0xEE);
    CHECK_EQ_INT((int)DSB(snap_r0), 0xEE);
    CHECK_EQ_INT((int)DSD(DS_00107D28), 0x28);                 /* slot 0's +0x5F */
    CHECK_EQ_INT((int)DSW(DS_00107D2C), 4);          /* 0x39834(1): its ctx[0] = 0 */
    CHECK_EQ_INT((int)DSW(DS_00107D2C + 2u), 5);
    CHECK_EQ_INT((int)DSW(DS_00107D20 + 2u), 0x2121);
    CHECK_EQ_INT((int)DSB(s0 + 0x57u), 2);
    CHECK_EQ_INT((int)DSB(s1 + 0x57u), 0x57);
    CHECK_EQ_INT((int)DSB(s1 + 0x52u), 0x10);
    CHECK_EQ_INT((int)DSB(s1 + 0x53u), 0x0A);
    CHECK_EQ_INT((int)DSD(s1 + 0x10u), 0x00022BEC);
    CHECK_EQ_INT((int)DSB(s1 + 0x5Fu), 0xFF);
    CHECK_EQ_INT((int)DSB(s0 + 0x5Fu), 0x28);
    CHECK_EQ_INT((int)DSD(s1 + 0x18u), 0x18181818);
    CHECK_EQ_INT((int)DSD(s1 + 0x1Cu), 0x1C1C1C1C);
    CHECK_EQ_INT((int)DSD(s1 + 0x14u), 0x00029D04);            /* 0x22B28 */
    CHECK_EQ_INT((int)DSB(s1 + 0x58u), 1);
    CHECK_EQ_INT((int)DSW(DS_0010474C + 2u), 0);
    CHECK_EQ_INT((int)DSB(DS_0010476C + 1u), 1);
    CHECK_EQ_INT((int)DSD(DS_000FCCE0), (int)FZ_EFX);
    CHECK_EQ_INT((int)DSB(Q42_10476A), 1);
    CHECK_EQ_INT((int)DSB(Q42_10476A + 1u), 0x6B);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 0x55);
    /* A2: 0x22CE4(0), the mirror, on slot 1's reaction 0x29. */
    q42_fz_seed(r0, r1);
    memcpy(pre_s, mem + s0, sizeof pre_s);
    fighter_22ce4(0u);
    CHECK(memcmp(mem + FIGHT_SNAP_235C4, pre_s, sizeof pre_s) == 0,
          "0x22CE4's 0x33ACC copied slot 0");
    CHECK_EQ_INT((int)DSB(snap_s1), 0xEE);
    CHECK_EQ_INT((int)DSD(DS_00107D28), 0x29);
    CHECK_EQ_INT((int)DSW(DS_00107D2C + 2u), 6);     /* 0x39834(0): its ctx[0] = 1 */
    CHECK_EQ_INT((int)DSW(DS_00107D2C), 3);
    CHECK_EQ_INT((int)DSW(DS_00107D20), 0x2020);
    CHECK_EQ_INT((int)DSB(s1 + 0x57u), 2);
    CHECK_EQ_INT((int)DSB(s0 + 0x57u), 0x57);
    CHECK_EQ_INT((int)DSD(s0 + 0x10u), 0x00022BEC);
    CHECK_EQ_INT((int)DSB(s0 + 0x5Fu), 0xFF);
    CHECK_EQ_INT((int)DSD(s1 + 0x10u), 0x10101010);
    CHECK_EQ_INT((int)DSB(Q42_10476A + 1u), 1);
    CHECK_EQ_INT((int)DSB(Q42_10476A), 0x6A);

    /* B: 0x22E44(0) through its registration: slot 1 frozen, slot 0's
     * +0x57/+0x74, the projectile p at 0x5000 - 0x1000 (side 0's actor bit 15
     * clear), height 0x42, y 0xFFFFCC00, and the arrays at index 0. */
    q42_fz_seed(r0, r1);
    fz_one_free();
    q42_one_actor(p);
    DSD(r0 + 0x18u) = 0x5000u;
    DSD(r0 + 0x30u) = 0x0042ABCDu;
    DSB(DS_00104AE8) = 0x41u;
    if (f44 != NULL) f44(0u);
    CHECK_EQ_INT((int)DSB(s0 + 0x57u), 2);
    CHECK_EQ_INT((int)DSD(s1 + 0x10u), 0x00022BEC);
    CHECK_EQ_INT((int)DSB(s1 + 0x52u), 0x10);
    CHECK_EQ_INT((int)DSD(s0 + 0x10u), 0x10101010);
    CHECK_EQ_INT((int)DSW(s0 + 0x74u), 0x29A);
    CHECK_EQ_INT((int)DSW(s1 + 0x74u), 0x7474);
    CHECK_EQ_INT((int)DSD(Q42_104728), (int)p);
    CHECK_EQ_INT((int)DSD(Q42_104728 + 4u), (int)Q42_DUMMY1);
    CHECK_EQ_INT((int)DSD(DS_00105BCC), (int)p);
    CHECK_EQ_INT((int)DSD(p + 0x08u), 0x000E8E64);
    CHECK_EQ_INT((int)DSD(p + 0x14u), (int)s0);
    CHECK_EQ_INT((int)DSD(p + 0x18u), 0x4000);
    CHECK_EQ_INT((int)DSD(p + 0x1Cu), (int)0xFFFFCC00u);
    CHECK_EQ_INT((int)DSW(p + 0x32u), 0x42);
    CHECK_EQ_INT((int)DSW(p + 0x36u), 0x200);
    CHECK_EQ_INT((int)DSB(p + 0x59u), 0xFE);
    CHECK_EQ_INT((int)DSB(p + 0x5Fu), 0);
    CHECK_EQ_INT((int)DSW(FIGHT_ACTORS + 3u * 0x20u + 2u), 0x0018);
    CHECK_EQ_INT((int)DSB(Q42_10476A), 0);
    CHECK_EQ_INT((int)DSB(Q42_10476A + 1u), 0x6B);
    CHECK_EQ_INT((int)DSB(DS_00104AE8), 0x61);
    /* B2: side 0's actor bit 15 set: the projectile at 0x5000 + 0x1000. */
    q42_fz_seed(r0, r1);
    q42_one_actor(p);
    DSD(r0 + 0x18u) = 0x5000u;
    DSW(FIGHT_ACTORS + 0x20u) = 0x8000u;
    if (f44 != NULL) f44(0u);
    CHECK_EQ_INT((int)DSD(p + 0x18u), 0x6000);
    /* B3: 0x22E44(1): slot 0 frozen, the arrays at index 1, x from record 1. */
    q42_fz_seed(r0, r1);
    q42_one_actor(p);
    DSD(r1 + 0x18u) = 0x7000u;
    DSB(DS_00104AE8) = 0;
    if (f44 != NULL) f44(1u);
    CHECK_EQ_INT((int)DSB(s1 + 0x57u), 2);
    CHECK_EQ_INT((int)DSD(s0 + 0x10u), 0x00022BEC);
    CHECK_EQ_INT((int)DSW(s1 + 0x74u), 0x29A);
    CHECK_EQ_INT((int)DSD(Q42_104728 + 4u), (int)p);
    CHECK_EQ_INT((int)DSD(Q42_104728), (int)Q42_DUMMY0);
    CHECK_EQ_INT((int)DSD(p + 0x14u), (int)s1);
    CHECK_EQ_INT((int)DSD(p + 0x18u), 0x6000);
    CHECK_EQ_INT((int)DSB(Q42_10476A + 1u), 0);
    CHECK_EQ_INT((int)DSB(Q42_10476A), 0x6A);
    CHECK_EQ_INT((int)DSB(DS_00104AE8), 0x20);

    q42_restore();
}

/* Record §42-A. 0x22D8C (+0x18, fn(side), on 0x33950): 1 while the other
 * slot's +0x10 is 0x22BEC or the side's signed 0x104750 tick is above 0x10 or
 * below 3 (6 with the slot's +0x76 zero); else 0x18C14 with flags 5/9 = 1 and
 * 1/8/0xD/0xE = 0 on the 0xA8328/0xA8332 boxes (0x8C/0xC7 << 6) while the
 * signed DS_00107D2C[side] is positive, else 0xA8314/0xA831E (0x69/0x96 << 6).
 * The sh_seed context passes every check once side 0 is latched from its
 * record (+0x42 bit 3), slot 1 through 0x18540 (DS_001077A8[1] = 0, its
 * anchor already current, no screen offset), slot 1 below slot 0 (0x189FC)
 * and 0x3B298 held at 0. 0x22F14 (+0x0C): the tick and +0x57 1 -> 2 (+0x8A
 * = 0) past 0x10 (signed). */
static void check_hook_22d8c(void)
{
    typedef u32 (*hook_fn)(u32 side);
    typedef void (*slot_fn)(u32 slot, u32 rec, u32 side);
    hook_fn hook;
    slot_fn f14;
    u32 s0 = DS_001077B0, s1 = DS_001077B0 + 0x94u;
    u32 r0 = FIGHT_RECS, r1 = FIGHT_RECS + 0x100u;

    q42_save();
    hook = (hook_fn)(void *)fn_resolve(0x22D8Cu);
    CHECK(hook == fighter_22d8c, "0x22D8C is registered as fighter_22d8c");
    f14 = (slot_fn)(void *)fn_resolve(0x22F14u);
    CHECK(f14 == fighter_22f14, "0x22F14 is registered as fighter_22f14");
    CHECK_EQ_INT((int)DSB(0x000A8314u), 0x69);
    CHECK_EQ_INT((int)DSB(0x000A831Eu), 0x96);
    CHECK_EQ_INT((int)DSB(0x000A8328u), 0x8C);
    CHECK_EQ_INT((int)DSB(0x000A8332u), 0xC7);

    sh_seed(s0, s1, r0, r1);
    DSB(s0 + 0x42u) = 8u;
    DSD(DS_001077A8 + 4u) = 0;
    DSD(s1 + 0x20u) = DSD(DS_00100AF0 + 4u);
    DSD(DS_00100AB0 + 8u) = 0;
    DSD(DS_00100AB4 + 8u) = 0;
    DSB(s1 + 0x63u) = 0;
    DSW(DS_000A6728 + 2u) = 3u;
    DSD(r0 + 0x18u) = 0x2000u;
    DSW(s0 + 0x76u) = 1u;                    /* lim 3 */
    DSW(Q42_104750) = 8u;
    DSW(Q42_104750 + 2u) = 0x40u;           /* side 1's tick: not read */
    DSW(DS_00107D2C) = 1u;
    DSW(DS_00107D2C + 2u) = 0;

    /* A: |x| 0x2000 passes 0x8C << 6 (0x2300): 0; the other slot frozen: 1. */
    CHECK_EQ_INT((int)fighter_22d8c(0u), 0);
    DSD(s1 + 0x10u) = 0x00022BECu;
    CHECK_EQ_INT((int)fighter_22d8c(0u), 1);
    DSD(s0 + 0x10u) = 0x00022BECu;          /* the self slot is not the gate */
    DSD(s1 + 0x10u) = 0;
    CHECK_EQ_INT((int)fighter_22d8c(0u), 0);
    DSD(s0 + 0x10u) = 0;
    /* A2: through 0x19020: DS_00100AF8[0] = (hook == 0). */
    DSD(s0 + 0x18u) = 0x00022D8Cu;
    DSD(DS_00100AF8) = 0x55u;
    fighter_19020(0u);
    CHECK_EQ_INT((int)DSD(DS_00100AF8), 1);
    DSD(s1 + 0x10u) = 0x00022BECu;
    fighter_19020(0u);
    CHECK_EQ_INT((int)DSD(DS_00100AF8), 0);
    DSD(s1 + 0x10u) = 0;
    DSD(s0 + 0x18u) = 0;

    /* B: DS_00107D2C[0] not positive (0, then 0x8000 = -32768): the 0x69 << 6
     * (0x1A40) box fails |x| 0x2000. */
    DSW(DS_00107D2C) = 0;
    CHECK_EQ_INT((int)fighter_22d8c(0u), 1);
    DSW(DS_00107D2C) = 0x8000u;
    CHECK_EQ_INT((int)fighter_22d8c(0u), 1);
    DSD(r0 + 0x18u) = 0x1800u;               /* 0x1800 passes 0x1A40 */
    CHECK_EQ_INT((int)fighter_22d8c(0u), 0);
    /* B2: box b by |y|: 0x96 << 6 (0x2580) passes 0x2400, fails 0x2800;
     * 0xC7 << 6 (0x31C0) passes 0x2800. */
    DSD(r0 + 0x18u) = 0x1000u;               /* passes both a boxes, above slot 1 */
    DSD(r0 + 0x1Cu) = 0x2400u;
    CHECK_EQ_INT((int)fighter_22d8c(0u), 0);
    DSD(r0 + 0x1Cu) = 0x2800u;
    CHECK_EQ_INT((int)fighter_22d8c(0u), 1);
    DSW(DS_00107D2C) = 1u;
    CHECK_EQ_INT((int)fighter_22d8c(0u), 0);
    DSD(r0 + 0x1Cu) = 0x31C1u;
    CHECK_EQ_INT((int)fighter_22d8c(0u), 1);
    DSD(r0 + 0x1Cu) = 0;
    DSD(r0 + 0x18u) = 0x2000u;

    /* C: the tick window, 3..0x10 with +0x76 set, 6..0x10 with it clear. */
    {
        static const u16 tick[8] = { 2u, 3u, 0x10u, 0x11u, 0xFFFFu, 5u, 6u, 0x10u };
        static const u16 w76[8]  = { 1u, 1u, 1u, 1u, 1u, 0u, 0u, 0u };
        static const int want[8] = { 1, 0, 0, 1, 1, 1, 0, 0 };
        u32 k;
        for (k = 0; k < 8u; k++) {
            DSW(Q42_104750) = tick[k];
            DSW(s0 + 0x76u) = w76[k];
            CHECK_EQ_INT((int)fighter_22d8c(0u), want[k]);
        }
    }
    /* C2: the flags reach 0x18C14: flag 8 (the other slot's +0x42 bit 3) and
     * flag 9 (slot 1 above slot 0) each fire. */
    DSW(Q42_104750) = 8u;
    DSB(s1 + 0x42u) = 8u;
    CHECK_EQ_INT((int)fighter_22d8c(0u), 1);
    DSB(s1 + 0x42u) = 0;
    DSD(r1 + 0x18u) = 0x3000u;
    CHECK_EQ_INT((int)fighter_22d8c(0u), 1);
    DSD(r1 + 0x18u) = 0;
    CHECK_EQ_INT((int)fighter_22d8c(0u), 0);
    /* C3: flag 1 (the other slot's +0x74) and flag 0xD (0x39EFC: the other
     * slot in the 0x39CC8 pose) each fire; flag 0xE fires when 0x3B298
     * returns 1 (check_slot_hook's M2 with side 1's command word 0x2000, the
     * facing base for slot 1 below slot 0; it sets the other slot's +0x43
     * bit 5). */
    DSW(s1 + 0x74u) = 1u;
    CHECK_EQ_INT((int)fighter_22d8c(0u), 1);
    DSW(s1 + 0x74u) = 0;
    DSB(s1 + 0x53u) = 0x0Au;
    DSD(s1 + 0x10u) = 0x00039CC8u;
    DSB(s1 + 0x58u) = 4u;
    CHECK_EQ_INT((int)fighter_22d8c(0u), 1);
    DSB(s1 + 0x53u) = 0;
    DSD(s1 + 0x10u) = 0;
    DSB(s1 + 0x58u) = 0;
    CHECK_EQ_INT((int)fighter_22d8c(0u), 0);
    DSW(DS_000A6728 + 2u) = 0;
    mem_fill(DS_00108270, 0, 0x50u);
    DSW(DS_001088E0) = 0;
    DSW(DS_001088E2) = 0x2000u;              /* slot 1 below slot 0: 0x2000 */
    CHECK_EQ_INT((int)fighter_22d8c(0u), 1);
    CHECK_EQ_INT((int)(DSB(s1 + 0x43u) & 0x30u), 0x20);
    DSW(DS_001088E2) = 0;
    DSW(DS_000A6728 + 2u) = 3u;
    DSB(s1 + 0x43u) = 0;
    CHECK_EQ_INT((int)fighter_22d8c(0u), 0);

    /* D: 0x22F14 through its registration, with slot 1 and record 1 as EAX/
     * EDX but side 0 in EBX: slot 0's +0x57 1 -> 2 past a tick of 0x10. */
    DSB(s0 + 0x57u) = 1u;
    DSB(s0 + 0x8Au) = 0x8Au;
    DSB(s1 + 0x57u) = 1u;
    DSW(Q42_104750) = 0x0Fu;
    DSW(Q42_104750 + 2u) = 0x40u;
    if (f14 != NULL) f14(s1, r1, 0u);
    CHECK_EQ_INT((int)DSW(Q42_104750), 0x10);
    CHECK_EQ_INT((int)DSB(s0 + 0x57u), 1);
    CHECK_EQ_INT((int)DSB(s0 + 0x8Au), 0x8A);
    if (f14 != NULL) f14(s1, r1, 0u);
    CHECK_EQ_INT((int)DSW(Q42_104750), 0x11);
    CHECK_EQ_INT((int)DSB(s0 + 0x57u), 2);
    CHECK_EQ_INT((int)DSB(s0 + 0x8Au), 0);
    CHECK_EQ_INT((int)DSB(s1 + 0x57u), 1);
    CHECK_EQ_INT((int)DSW(Q42_104750 + 2u), 0x40);
    DSB(s0 + 0x8Au) = 0x8Au;
    fighter_22f14(s1, r1, 0u);
    CHECK_EQ_INT((int)DSW(Q42_104750), 0x12);
    CHECK_EQ_INT((int)DSB(s0 + 0x57u), 2);
    CHECK_EQ_INT((int)DSB(s0 + 0x8Au), 0x8A);
    /* D2: 0x7FFF -> 0x8000 is negative and holds; +0x57 = 0 only ticks. */
    DSB(s0 + 0x57u) = 1u;
    DSW(Q42_104750) = 0x7FFFu;
    fighter_22f14(s1, r1, 0u);
    CHECK_EQ_INT((int)DSB(s0 + 0x57u), 1);
    DSB(s0 + 0x57u) = 0;
    DSW(Q42_104750) = 0x20u;
    fighter_22f14(s1, r1, 0u);
    CHECK_EQ_INT((int)DSW(Q42_104750), 0x21);
    CHECK_EQ_INT((int)DSB(s0 + 0x57u), 0);
    CHECK_EQ_INT((int)DSB(s0 + 0x8Au), 0x8A);
    /* D3: side 1 ticks its own word. */
    fighter_22f14(s0, r0, 1u);
    CHECK_EQ_INT((int)DSW(Q42_104750 + 2u), 0x41);
    CHECK_EQ_INT((int)DSW(Q42_104750), 0x21);
    CHECK_EQ_INT((int)DSB(s1 + 0x57u), 2);                  /* 0x41 > 0x10 */
    CHECK_EQ_INT((int)DSB(s1 + 0x8Au), 0);

    q42_restore();
}

/* Record §42-A. Character 1's reaction callbacks: 0x22F74 (*(u32*)0xA3D5C,
 * reaction 0x29) arms the slot and starts 0xE4952 at 3.0 through 0x3C4CC
 * (+0x52 = 9 takes the 0x3C480 arm; the 0xDC00 opcode consumes its dword
 * 0xE25A8, and 0x2A408 steps over the 0xED40 table sprite and its dword, so
 * rec+0x08 = 0xE495C); 0x2365C (*(u32*)0xA3D70, reaction 0x2A)
 * returns 0 on the slot's +0x08 or the other slot (the RECORD's +0x51 ^ 1)
 * frozen, else starts 0xE4996 at 3.0, 0x0B/6/0, +0x0C = 0 and +0x5F -> +0x64.
 * 0x22FE8 (update-table entry 5): each side's 0x104728 projectile. */
static void check_char1_reactions(void)
{
    typedef void (*react_fn)(u32 slot, u32 rec, u32 side);
    typedef void (*proc_fn)(void);
    react_fn f74, f5c;
    proc_fn pe;
    u32 s0 = DS_001077B0, s1 = DS_001077B0 + 0x94u;
    u32 r0 = FZ_POOL, r1 = FZ_POOL + ACTOR_REC_SIZE;
    u32 p0 = FZ_POOL + 3u * ACTOR_REC_SIZE, p1 = FZ_POOL + 4u * ACTOR_REC_SIZE;

    q42_save();
    f74 = (react_fn)(void *)fn_resolve(0x22F74u);
    f5c = (react_fn)(void *)fn_resolve(0x2365Cu);
    pe = (proc_fn)fn_resolve(0x22FE8u);
    CHECK(f74 != NULL, "0x22F74 is a registered reaction callback");
    CHECK(f5c != NULL, "0x2365C is a registered reaction callback");
    CHECK(pe == fighter_22fe8, "0x22FE8 (update-table entry 5) is registered");
    CHECK_EQ_INT((int)DSD(0x000A3D5Cu), 0x00022F74);
    CHECK_EQ_INT((int)DSD(0x000A3D70u), 0x0002365C);
    CHECK_EQ_INT((int)DSD(0x000A8644u + 5u * 4u), 0x00022FE8);
    CHECK_EQ_INT((int)DSW(0x000E4996u), 0xCD40);
    CHECK_EQ_INT((int)DSW(0x000E8E66u), 0xCD40);

    /* A: 0x22F74 for side 0 (EAX/EDX unread: slot 1/record 1 passed). */
    q42_fz_seed(r0, r1);
    if (f74 != NULL) f74(s1, r1, 0u);
    CHECK_EQ_INT((int)DSD(s0 + 0x18u), 0x00022D8C);
    CHECK_EQ_INT((int)DSD(s0 + 0x1Cu), 0x00022E44);
    CHECK_EQ_INT((int)DSD(s0 + 0x0Cu), 0x00022F14);
    CHECK_EQ_INT((int)DSB(s0 + 0x57u), 1);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 9);
    CHECK_EQ_INT((int)DSB(s0 + 0x53u), 7);
    CHECK_EQ_INT((int)DSB(s0 + 0x54u), 0);
    CHECK_EQ_INT((int)DSW(Q42_104750), 0);
    CHECK_EQ_INT((int)DSW(Q42_104750 + 2u), 0x3434);
    CHECK_EQ_INT((int)DSD(r0 + 0x24u), 0x40400000);
    CHECK_EQ_INT((int)DSD(r0 + 0x08u), 0x000E495C);
    CHECK_EQ_INT((int)DSD(s1 + 0x18u), 0x18181818);
    CHECK_EQ_INT((int)DSB(s1 + 0x52u), 0x55);
    CHECK_EQ_INT((int)DSD(r1 + 0x24u), 0x11111111);
    CHECK_EQ_INT(fighter_22f74(s1, r1, 1u), 1);
    CHECK_EQ_INT((int)DSD(s1 + 0x18u), 0x00022D8C);
    CHECK_EQ_INT((int)DSW(Q42_104750 + 2u), 0);

    /* B: 0x2365C on slot 1/record 1 (+0x51 = 1, so the other slot is 0). */
    q42_fz_seed(r0, r1);
    DSD(s1 + 0x08u) = 0x1234u;
    CHECK_EQ_INT(fighter_2365c(s1, r1, 1u), 0);
    CHECK_EQ_INT((int)DSB(s1 + 0x52u), 0x55);
    CHECK_EQ_INT((int)DSD(r1 + 0x24u), 0x11111111);
    DSD(s1 + 0x08u) = 0;
    DSD(s0 + 0x10u) = 0x00022BECu;
    CHECK_EQ_INT(fighter_2365c(s1, r1, 0u), 0);  /* EBX = 0 is not the side */
    CHECK_EQ_INT((int)DSB(s1 + 0x5Fu), 0x29);
    CHECK_EQ_INT((int)DSD(r1 + 0x24u), 0x11111111);
    DSD(s0 + 0x10u) = 0x10101010u;
    DSD(s1 + 0x10u) = 0x00022BECu;           /* the slot's own +0x10 */
    if (f5c != NULL) f5c(s1, r1, 0u);
    CHECK_EQ_INT((int)DSD(r1 + 0x24u), 0x40400000);
    CHECK_EQ_INT((int)DSD(r1 + 0x08u), 0x000E4998);
    CHECK_EQ_INT((int)DSB(s1 + 0x52u), 0x0B);
    CHECK_EQ_INT((int)DSB(s1 + 0x53u), 6);
    CHECK_EQ_INT((int)DSB(s1 + 0x54u), 0);
    CHECK_EQ_INT((int)DSD(s1 + 0x0Cu), 0);
    CHECK_EQ_INT((int)DSB(s1 + 0x64u), 0x29);
    CHECK_EQ_INT((int)DSB(s1 + 0x5Fu), 0xFF);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 0x55);
    CHECK_EQ_INT((int)DSB(s0 + 0x64u), 0x64);
    q42_fz_seed(r0, r1);
    CHECK_EQ_INT(fighter_2365c(s1, r1, 1u), 1);

    /* C: 0x22FE8 through its registration. p0 on slot 0 (the other slot for
     * its record's +0x51 = 0 is DS_001077A8[1] = slot 1), p1 on slot 1. */
    {
        u32 k;
        q42_fz_seed(r0, r1);
        DSD(DS_001077A8) = s0;
        DSD(DS_001077A8 + 4u) = s1;
        for (k = 0; k < 2u; k++) {
            u32 pp = k ? p1 : p0;
            mem_fill(pp, 0, ACTOR_REC_SIZE);
            DSW(pp + 0x56u) = (u16)(3u + k);
            DSD(pp + 0x08u) = 0x00ABCDEFu;
            DSD(pp + 0x14u) = k ? s1 : s0;
            DSD(pp + 0x1Cu) = 0x100u;
            DSW(pp + 0x36u) = 0xFF00u;       /* y + vy = 0: at the floor */
            DSD(pp + 0x24u) = 0x11111111u;
        }
        /* C1: both empty: bit 5 cleared. */
        DSD(Q42_104728) = 0;
        DSD(Q42_104728 + 4u) = 0;
        DSB(DS_00104AE8) = 0xFFu;
        if (pe != NULL) pe();
        CHECK_EQ_INT((int)DSB(DS_00104AE8), 0xDF);
        /* C2: p0, 0x10476A[0] = 0, slot 0's +0x53 = 0x66: floored and
         * retired; bit 5 kept (the entry was live). */
        DSD(Q42_104728) = p0;
        DSB(Q42_10476A) = 0;
        DSB(DS_00104AE8) = 0xFFu;
        if (pe != NULL) pe();
        CHECK_EQ_INT((int)DSD(p0 + 0x1Cu), 0);
        CHECK_EQ_INT((int)DSW(p0 + 0x36u), 0);
        CHECK_EQ_INT((int)DSD(p0 + 0x08u), 0x000E8E68);
        CHECK_EQ_INT((int)DSD(p0 + 0x24u), 0x40400000);
        CHECK_EQ_INT((int)DSD(Q42_104728), 0);
        CHECK_EQ_INT((int)DSB(DS_00104AE8), 0xFF);
        /* C3: slot 0's +0x53 = 7 holds; above the floor nothing is zeroed. */
        DSD(Q42_104728) = p0;
        DSD(p0 + 0x08u) = 0x00ABCDEFu;
        DSD(p0 + 0x1Cu) = 0x100u;
        DSW(p0 + 0x36u) = 0xFE00u;
        DSB(s0 + 0x53u) = 7u;
        if (pe != NULL) pe();
        CHECK_EQ_INT((int)DSD(Q42_104728), (int)p0);
        CHECK_EQ_INT((int)DSD(p0 + 0x08u), 0x00ABCDEF);
        CHECK_EQ_INT((int)DSD(p0 + 0x1Cu), 0x100);
        CHECK_EQ_INT((int)DSW(p0 + 0x36u), 0xFE00);
        /* C4: 0x10476A[0] = 1: held while slot 1 is frozen, else retired. */
        DSB(Q42_10476A) = 1u;
        DSD(s1 + 0x10u) = 0x00022BECu;
        if (pe != NULL) pe();
        CHECK_EQ_INT((int)DSD(Q42_104728), (int)p0);
        DSD(s1 + 0x10u) = 0;
        if (pe != NULL) pe();
        CHECK_EQ_INT((int)DSD(Q42_104728), 0);
        CHECK_EQ_INT((int)DSD(p0 + 0x08u), 0x000E8E68);
        /* C5: 0x10476A[0] = 2 holds. */
        DSD(Q42_104728) = p0;
        DSD(p0 + 0x08u) = 0x00ABCDEFu;
        DSB(Q42_10476A) = 2u;
        DSB(s0 + 0x53u) = 0x66u;
        if (pe != NULL) pe();
        CHECK_EQ_INT((int)DSD(Q42_104728), (int)p0);
        CHECK_EQ_INT((int)DSD(p0 + 0x08u), 0x00ABCDEF);
        /* C6: DS_001077A8[1] = 0 returns at p0: p1 (retirable) untouched,
         * p0 not floored, bit 5 kept. */
        DSB(Q42_10476A) = 0;
        DSB(Q42_10476A + 1u) = 0;
        DSD(Q42_104728 + 4u) = p1;
        DSD(DS_001077A8 + 4u) = 0;
        DSW(p0 + 0x36u) = 0xFF00u;
        DSB(DS_00104AE8) = 0xFFu;
        if (pe != NULL) pe();
        CHECK_EQ_INT((int)DSD(Q42_104728), (int)p0);
        CHECK_EQ_INT((int)DSD(p0 + 0x1Cu), 0x100);
        CHECK_EQ_INT((int)DSD(Q42_104728 + 4u), (int)p1);
        CHECK_EQ_INT((int)DSD(p1 + 0x08u), 0x00ABCDEF);
        /* C7: p0 with no slot is skipped but keeps bit 5; p1 (other slot 0
         * through its record's +0x51 = 1) retires. */
        DSD(DS_001077A8 + 4u) = s1;
        DSD(p0 + 0x14u) = 0;
        DSB(DS_00104AE8) = 0xFFu;
        if (pe != NULL) pe();
        CHECK_EQ_INT((int)DSD(Q42_104728), (int)p0);
        CHECK_EQ_INT((int)DSD(p0 + 0x08u), 0x00ABCDEF);
        CHECK_EQ_INT((int)DSD(Q42_104728 + 4u), 0);
        CHECK_EQ_INT((int)DSD(p1 + 0x08u), 0x000E8E68);
        CHECK_EQ_INT((int)DSD(p1 + 0x1Cu), 0);
        CHECK_EQ_INT((int)DSB(DS_00104AE8), 0xFF);
        /* C7b: the slot-less p0 alone still keeps bit 5 (0x23017 zeroes the
         * flag before the slot test). */
        DSD(Q42_104728 + 4u) = 0;
        DSB(DS_00104AE8) = 0xFFu;
        if (pe != NULL) pe();
        CHECK_EQ_INT((int)DSB(DS_00104AE8), 0xFF);
        CHECK_EQ_INT((int)DSD(Q42_104728), (int)p0);
        /* C8: p1 alone with 0x10476A[1] = 1 and slot 0 frozen: held. */
        DSD(Q42_104728) = 0;
        DSD(Q42_104728 + 4u) = p1;
        DSD(p1 + 0x08u) = 0x00ABCDEFu;
        DSB(Q42_10476A + 1u) = 1u;
        DSD(s0 + 0x10u) = 0x00022BECu;
        if (pe != NULL) pe();
        CHECK_EQ_INT((int)DSD(Q42_104728 + 4u), (int)p1);
        CHECK_EQ_INT((int)DSD(p1 + 0x08u), 0x00ABCDEF);
    }

    q42_restore();
}

/* Record §43-C. Character 1's reaction callbacks 0x230F0 (0x25, *(u32*)
 * 0xA3D0C), 0x23130 (0x20, 0xA3CA8) and 0x23178 (0x28, 0xA3D48), EAX = slot,
 * EDX = rec, EBX unread (its 0x33950 context is dead): state 9/7/0, +0x0C = 0
 * and the 0xE483E/0xE4872/0xE48DC stream at 3.0 through 0x3C4CC, which reads
 * the slot's +0x52. 0x230F0 starts the stream first: a +0x52 = 0 seed takes
 * 0x3C4CC's plain 0x2BC30 arm and keeps rec+0x1C. 0x23130/0x23178 store +0x52
 * = 9 first: 0x3C4CC takes its 0x3C480 arm, whose 0x188AC zeroes rec+0x1C.
 * Each stream opens with `DC00 <table>` (opcode 0x1C): rec+0x10 = the table
 * (0xE22C8/0xE22E8/0xE2328) and the 3.0 hold is replaced by its first byte
 * (2/1/2). The walk stops on the first sprite: 0xE483E's `ED40` table sprite
 * at 0xE4844 leaves rec+8 = 0xE4848; 0xE4872 runs `DA00`, two `FF2x` and
 * reaches `ED40` at 0xE488E (rec+8 = 0xE4892), and 0xE48DC's `C300` jumps
 * to 0xE4872's `FF20` at 0xE487E, so it ends there too. */
static void check_char1_reactions_b(void)
{
    typedef void (*react_fn)(u32 slot, u32 rec, u32 side);
    react_fn f0, f30, f78;
    u32 s0 = DS_001077B0, s1 = DS_001077B0 + 0x94u;
    u32 r0 = FZ_POOL, r1 = FZ_POOL + ACTOR_REC_SIZE;

    q42_save();
    f0 = (react_fn)(void *)fn_resolve(0x230F0u);
    f30 = (react_fn)(void *)fn_resolve(0x23130u);
    f78 = (react_fn)(void *)fn_resolve(0x23178u);
    CHECK(f0 != NULL, "0x230F0 is a registered reaction callback");
    CHECK(f30 != NULL, "0x23130 is a registered reaction callback");
    CHECK(f78 != NULL, "0x23178 is a registered reaction callback");
    CHECK_EQ_INT((int)DSD(0x000A3D0Cu), 0x000230F0);
    CHECK_EQ_INT((int)DSD(0x000A3CA8u), 0x00023130);
    CHECK_EQ_INT((int)DSD(0x000A3D48u), 0x00023178);
    CHECK_EQ_INT((int)DSD(0x000A3D0Cu + 4u), 0);   /* no table stream */
    CHECK_EQ_INT((int)DSD(0x000A3CA8u + 4u), 0);
    CHECK_EQ_INT((int)DSD(0x000A3D48u + 4u), 0);

    /* A: 0x230F0 through its registration on slot 1/record 1, EBX = 0.
     * +0x52 = 0 is read by 0x3C4CC before the store: rec+0x1C kept. */
    q42_fz_seed(r0, r1);
    DSD(r1 + 0x1Cu) = 0x5555u;
    DSB(s1 + 0x52u) = 0;
    if (f0 != NULL) f0(s1, r1, 0u);
    CHECK_EQ_INT((int)DSB(s1 + 0x52u), 9);
    CHECK_EQ_INT((int)DSB(s1 + 0x53u), 7);
    CHECK_EQ_INT((int)DSB(s1 + 0x54u), 0);
    CHECK_EQ_INT((int)DSD(s1 + 0x0Cu), 0);
    CHECK_EQ_INT((int)DSD(r1 + 0x10u), 0x000E22C8);
    CHECK_EQ_INT((int)DSD(r1 + 0x24u), 0x40000000);
    CHECK_EQ_INT((int)DSD(r1 + 0x08u), 0x000E4848);
    CHECK_EQ_INT((int)DSD(r1 + 0x1Cu), 0x5555);
    CHECK_EQ_INT((int)DSB(s1 + 0x5Fu), 0x29);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 0x55);
    CHECK_EQ_INT((int)DSD(s0 + 0x0Cu), 0x0C0C0C0C);
    CHECK_EQ_INT((int)DSD(r0 + 0x24u), 0x11111111);
    /* A2: +0x52 = 0x55 (not in 0x3C4CC's plain set) takes 0x3C480. */
    q42_fz_seed(r0, r1);
    DSD(r1 + 0x1Cu) = 0x5555u;
    CHECK_EQ_INT(fighter_230f0(s1, r1, 1u), 1);
    CHECK_EQ_INT((int)DSD(r1 + 0x1Cu), 0);

    /* B: 0x23130 through its registration on slot 0/record 0, EBX = 1. The
     * +0x52 = 0 seed is overwritten with 9 before 0x3C4CC: rec+0x1C zeroed. */
    q42_fz_seed(r0, r1);
    DSD(r0 + 0x1Cu) = 0x5555u;
    DSB(s0 + 0x52u) = 0;
    if (f30 != NULL) f30(s0, r0, 1u);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 9);
    CHECK_EQ_INT((int)DSB(s0 + 0x53u), 7);
    CHECK_EQ_INT((int)DSB(s0 + 0x54u), 0);
    CHECK_EQ_INT((int)DSD(s0 + 0x0Cu), 0);
    CHECK_EQ_INT((int)DSD(r0 + 0x10u), 0x000E22E8);
    CHECK_EQ_INT((int)DSD(r0 + 0x24u), 0x3F800000);
    CHECK_EQ_INT((int)DSD(r0 + 0x08u), 0x000E4892);
    CHECK_EQ_INT((int)DSD(r0 + 0x1Cu), 0);
    CHECK_EQ_INT((int)DSB(s1 + 0x52u), 0x55);
    CHECK_EQ_INT((int)DSD(s1 + 0x0Cu), 0x0C0C0C0C);
    CHECK_EQ_INT((int)DSD(r1 + 0x24u), 0x11111111);
    q42_fz_seed(r0, r1);
    CHECK_EQ_INT(fighter_23130(s0, r0, 0u), 1);

    /* C: 0x23178 through its registration on slot 1/record 1, the same
     * order as 0x23130, with the 0xE48DC stream. */
    q42_fz_seed(r0, r1);
    DSD(r1 + 0x1Cu) = 0x5555u;
    DSB(s1 + 0x52u) = 0;
    if (f78 != NULL) f78(s1, r1, 0u);
    CHECK_EQ_INT((int)DSB(s1 + 0x52u), 9);
    CHECK_EQ_INT((int)DSB(s1 + 0x53u), 7);
    CHECK_EQ_INT((int)DSB(s1 + 0x54u), 0);
    CHECK_EQ_INT((int)DSD(s1 + 0x0Cu), 0);
    CHECK_EQ_INT((int)DSD(r1 + 0x10u), 0x000E2328);
    CHECK_EQ_INT((int)DSD(r1 + 0x24u), 0x40000000);
    CHECK_EQ_INT((int)DSD(r1 + 0x08u), 0x000E4892);
    CHECK_EQ_INT((int)DSD(r1 + 0x1Cu), 0);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 0x55);
    CHECK_EQ_INT((int)DSD(r0 + 0x24u), 0x11111111);
    q42_fz_seed(r0, r1);
    CHECK_EQ_INT(fighter_23178(s1, r1, 0u), 1);

    /* D: 0x34E2C (0x35045) dispatching character 1's reaction 0x25 to
     * 0x230F0 through the 0xA3528 table: slot 0's +0x7A = 1. */
    q42_fz_seed(r0, r1);
    DSB(s0 + 0x7Au) = 1u;
    hit_reaction_apply(0u, 0x25u);
    CHECK_EQ_INT((int)DSB(s0 + 0x5Fu), 0x25);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 9);
    CHECK_EQ_INT((int)DSB(s0 + 0x53u), 7);
    CHECK_EQ_INT((int)DSD(s0 + 0x0Cu), 0);
    CHECK_EQ_INT((int)DSD(r0 + 0x24u), 0x40000000);
    CHECK_EQ_INT((int)DSD(r0 + 0x08u), 0x000E4848);

    q42_restore();
}

/* Record §43-C. 0x236D8 (0xE4996's 0xD100 target; EAX = rec, EDX unread):
 * with rec+0x14 (the slot) set, the 0xBB3BC child of rec (a5 = rec+0x56 |
 * 0x400: +0x4A = the parent's index, +0x28 bit 10, +0x49 = the parent pset's
 * layer + the walk's +0x59, the parent's +0x4F + 1) with +0x14 = the slot and
 * rec's +0x24; side 1 adds 4 to +0x2E and sets +0x4E. Its stream 0xE4F94
 * opens `9302 CD40 071D`, so the spawn walk leaves +8 at 0xE4F98. (That
 * `9302`, opcode 0x13, already stores +0x59 = 2, so dropping 0x236D8's own
 * store is equivalent; a wrong value still fails.) 0x2372C (0xE4F94's 0xD100 target
 * and 0x2463E's call; EAX = rec, EDX unread): with rec+0x14 set, the type-8
 * 0xBB3D0 projectile into slot+0x08 at the slot record's x -/+0xC00 (minus
 * while REC's +0x28 lacks bit 14), y + 0x1480, a3 = +0x30 >> 16 and a5 =
 * 0x4000 from the SLOT RECORD's +0x28 bit 14; +0x34 = -/+0x113 (rec's bit),
 * +0x14 = the slot, side 1 +0x2E + 4 and +0x4E = 1. 0xE4FB8 opens `9302 CD40
 * 072A`: +8 = 0xE4FBC. The pool is FZ_POOL (r0 index 0, r1 index 1), and the
 * one free record p is what the spawn pops. */
static void check_char1_chain(void)
{
    typedef void (*anim_fn)(u32 rec, u32 arg);
    anim_fn fd8, f2c;
    u32 s0 = DS_001077B0, s1 = DS_001077B0 + 0x94u;
    u32 r0 = FZ_POOL, r1 = FZ_POOL + ACTOR_REC_SIZE;
    u32 e = FZ_POOL + 2u * ACTOR_REC_SIZE;
    u32 p = FZ_POOL + 3u * ACTOR_REC_SIZE;
    u32 st = FIGHT_RECS + 0x4800u;

    q42_save();
    fd8 = (anim_fn)(void *)fn_resolve(0x236D8u);
    f2c = (anim_fn)(void *)fn_resolve(0x2372Cu);
    CHECK(fd8 != NULL, "0x236D8 is registered");
    CHECK(fn_resolve(0x236D8u) != (void (*)(void))fighter_236d8,
          "0x236D8 is registered through the (rec, arg) wrapper");
    CHECK(f2c != NULL, "0x2372C is registered");
    CHECK(fn_resolve(0x2372Cu) != (void (*)(void))fighter_2372c,
          "0x2372C is registered through the (rec, arg) wrapper");
    CHECK_EQ_INT((int)DSW(0x000E49A2u), 0xD100);
    CHECK_EQ_INT((int)DSD(0x000E49A4u), 0x000236D8);
    CHECK_EQ_INT((int)DSW(0x000E4FA2u), 0xD100);
    CHECK_EQ_INT((int)DSD(0x000E4FA4u), 0x0002372C);
    CHECK_EQ_INT((int)DSD(0x000BB3BCu), 0x000E4F94);
    CHECK_EQ_INT((int)DSD(0x000BB3D0u), 0x000E4FB8);
    CHECK_EQ_INT((int)DSB(0x000BB3D0u + 4u), 8);

    /* A: 0x236D8 on record 1 (side 1, pool index 1) through its
     * registration. */
    q42_fz_seed(r0, r1);
    DSW(r1 + 0x56u) = 1u;
    DSD(r1 + 0x14u) = s1;
    DSD(r1 + 0x24u) = 0x3FC00000u;
    DSW(FIGHT_ACTORS + 0x20u + 0x0Eu) = 0x30u;  /* the parent pset's layer */
    DSB(r1 + 0x4Fu) = 0x10u;
    q42_one_actor(p);
    if (fd8 != NULL) fd8(r1, 0xDEADu);
    CHECK_EQ_INT((int)DSD(DS_00105BCC), (int)p);
    CHECK_EQ_INT((int)DSD(p + 0x08u), 0x000E4F98);
    CHECK_EQ_INT((int)DSD(p + 0x14u), (int)s1);
    CHECK_EQ_INT((int)DSD(p + 0x24u), 0x3FC00000);
    CHECK_EQ_INT((int)DSB(p + 0x59u), 2);
    CHECK_EQ_INT((int)DSB(p + 0x4Au), 1);
    CHECK_EQ_INT((int)(DSW(p + 0x28u) & 0x0400u), 0x0400);
    CHECK_EQ_INT((int)DSB(p + 0x49u), 0x32);   /* 0x2A820: parent layer + the walk's +0x59 */
    CHECK_EQ_INT((int)DSB(r1 + 0x4Fu), 0x11);
    CHECK_EQ_INT((int)DSW(p + 0x2Eu), 0x000C);
    CHECK_EQ_INT((int)DSB(p + 0x4Eu), 1);
    /* B: record 0 (side 0, pool index 0): +0x2E kept at 8, +0x4E clear. */
    q42_fz_seed(r0, r1);
    DSW(r0 + 0x56u) = 0;
    DSD(r0 + 0x14u) = s0;
    DSD(r0 + 0x24u) = 0x3F400000u;
    q42_one_actor(p);
    fighter_236d8(r0);
    CHECK_EQ_INT((int)DSD(DS_00105BCC), (int)p);
    CHECK_EQ_INT((int)DSD(p + 0x14u), (int)s0);
    CHECK_EQ_INT((int)DSD(p + 0x24u), 0x3F400000);
    CHECK_EQ_INT((int)DSB(p + 0x4Au), 0);
    CHECK_EQ_INT((int)DSW(p + 0x2Eu), 0x0008);
    CHECK_EQ_INT((int)DSB(p + 0x4Eu), 0);
    /* C: no slot (rec+0x14 = 0, 0x236E4): nothing is spawned. */
    q42_fz_seed(r0, r1);
    DSW(r1 + 0x56u) = 1u;
    DSD(r1 + 0x14u) = 0;
    DSB(r1 + 0x4Fu) = 0x10u;
    q42_one_actor(p);
    fighter_236d8(r1);
    CHECK_EQ_INT((int)DSD(DS_00105BCC), (int)DS_00105BCC);
    CHECK_EQ_INT((int)DSD(DS_00105B3C), (int)p);
    CHECK_EQ_INT((int)DSB(r1 + 0x4Fu), 0x10);
    /* D: through the dispatcher: `D100 36D8 0002 0000` (0xE49A2's words;
     * opcode 0x11, mode 0x4000) walked by 0x2BC30 spawns the child. */
    q42_fz_seed(r0, r1);
    DSW(r1 + 0x56u) = 1u;
    DSD(r1 + 0x14u) = s1;
    q42_one_actor(p);
    DSW(st) = 0xD100u;
    DSW(st + 2u) = 0x36D8u;
    DSW(st + 4u) = 0x0002u;
    DSW(st + 6u) = 0x0000u;
    DSW(st + 8u) = 0x1131u;
    actors_anim_begin(r1, st, 0x3F800000u);
    CHECK_EQ_INT((int)DSD(DS_00105BCC), (int)p);
    CHECK_EQ_INT((int)DSD(p + 0x14u), (int)s1);

    /* E: 0x2372C through its registration on the child e (slot 0; e's +0x28
     * bit 14 clear, record 0's set): x 0x5000 - 0xC00, y 0x40 + 0x1480,
     * height 5, speed 0xFEED, a5 = 0x4000 into +0x28 (0x80 | 0x4000). */
    q42_fz_seed(r0, r1);
    mem_fill(e, 0, ACTOR_REC_SIZE);
    DSD(e + 0x14u) = s0;
    DSD(r0 + 0x18u) = 0x5000u;
    DSD(r0 + 0x1Cu) = 0x40u;
    DSD(r0 + 0x30u) = 0x00050000u;
    DSW(r0 + 0x28u) = 0x4000u;
    DSD(s0 + 0x08u) = 0x00ABCDEFu;
    DSD(s1 + 0x08u) = 0x00FEDCBAu;
    q42_one_actor(p);
    if (f2c != NULL) f2c(e, 0xDEADu);
    CHECK_EQ_INT((int)DSD(s0 + 0x08u), (int)p);
    CHECK_EQ_INT((int)DSD(s1 + 0x08u), 0x00FEDCBA);
    CHECK_EQ_INT((int)DSD(DS_00105BCC), (int)p);
    CHECK_EQ_INT((int)DSD(p + 0x18u), 0x4400);
    CHECK_EQ_INT((int)DSD(p + 0x1Cu), 0x14C0);
    CHECK_EQ_INT((int)DSW(p + 0x32u), 5);
    CHECK_EQ_INT((int)DSW(p + 0x34u), 0xFEED);
    CHECK_EQ_INT((int)DSD(p + 0x14u), (int)s0);
    CHECK_EQ_INT((int)DSW(p + 0x28u), 0x4080);
    CHECK_EQ_INT((int)DSB(p + 0x48u), 8);
    CHECK_EQ_INT((int)DSD(p + 0x08u), 0x000E4FBC);
    CHECK_EQ_INT((int)DSW(p + 0x2Eu), 0x0008);
    CHECK_EQ_INT((int)DSB(p + 0x4Eu), 0);
    /* F: the sign from e's own +0x28 (set), a5 from record 0's (clear). */
    q42_fz_seed(r0, r1);
    mem_fill(e, 0, ACTOR_REC_SIZE);
    DSD(e + 0x14u) = s0;
    DSW(e + 0x28u) = 0x4000u;
    DSD(r0 + 0x18u) = 0x5000u;
    DSW(r0 + 0x28u) = 0;
    q42_one_actor(p);
    fighter_2372c(e);
    CHECK_EQ_INT((int)DSD(p + 0x18u), 0x5C00);
    CHECK_EQ_INT((int)DSW(p + 0x34u), 0x0113);
    CHECK_EQ_INT((int)DSW(p + 0x28u), 0x0080);
    /* G: slot 1 (record 1's +0x51 = 1): +0x2E + 4, +0x4E set, slot 0 kept. */
    q42_fz_seed(r0, r1);
    mem_fill(e, 0, ACTOR_REC_SIZE);
    DSD(e + 0x14u) = s1;
    DSD(r1 + 0x18u) = 0x7000u;
    DSD(s0 + 0x08u) = 0x00ABCDEFu;
    q42_one_actor(p);
    fighter_2372c(e);
    CHECK_EQ_INT((int)DSD(s1 + 0x08u), (int)p);
    CHECK_EQ_INT((int)DSD(s0 + 0x08u), 0x00ABCDEF);
    CHECK_EQ_INT((int)DSD(p + 0x14u), (int)s1);
    CHECK_EQ_INT((int)DSD(p + 0x18u), 0x6400);
    CHECK_EQ_INT((int)DSW(p + 0x2Eu), 0x000C);
    CHECK_EQ_INT((int)DSB(p + 0x4Eu), 1);
    /* H: no slot (rec+0x14 = 0, 0x23737): nothing is spawned or stored. */
    q42_fz_seed(r0, r1);
    mem_fill(e, 0, ACTOR_REC_SIZE);
    DSD(s0 + 0x08u) = 0x00ABCDEFu;
    q42_one_actor(p);
    fighter_2372c(e);
    CHECK_EQ_INT((int)DSD(s0 + 0x08u), 0x00ABCDEF);
    CHECK_EQ_INT((int)DSD(DS_00105BCC), (int)DS_00105BCC);
    /* I: through the dispatcher: `D100 372C 0002 0000` (0xE4FA2's words). */
    q42_fz_seed(r0, r1);
    mem_fill(e, 0, ACTOR_REC_SIZE);
    DSW(e + 0x56u) = 2u;
    DSD(e + 0x14u) = s0;
    DSD(s0 + 0x08u) = 0x00ABCDEFu;
    q42_one_actor(p);
    DSW(st) = 0xD100u;
    DSW(st + 2u) = 0x372Cu;
    DSW(st + 4u) = 0x0002u;
    DSW(st + 6u) = 0x0000u;
    DSW(st + 8u) = 0x1131u;
    actors_anim_begin(e, st, 0x3F800000u);
    CHECK_EQ_INT((int)DSD(s0 + 0x08u), (int)p);

    q42_restore();
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

/* ---- §41-B: the placement 0x3C208 and the raptor's throw 0x14D7C -------- */

/* The raptor's thrown stream for character 4 (0xC91C0[4], read_memory). */
#define TW_ANIM_C4 0x000EAE36u

/* bp_seed plus the 0x3C208 inputs: each record on its own actor (r0 on 1,
 * r1 on 2, both words 0: sprite 0, bit 15 clear), DS_00104B00 = 3 (0x18B04
 * runs), and sentinels in both records' +0x34/+0x42/+0x43 and +0x29 (r0
 * 0x41, r1 0x02). */
static void tw_seed(s32 x0, s32 x1)
{
    u32 r0 = FIGHT_RECS, r1 = FIGHT_RECS + 0x100u;
    bp_seed(x0, 9000, x1, 9000);
    DSW(r0 + 0x56u) = 1u;
    DSW(r1 + 0x56u) = 2u;
    DSW(DS_00104B00) = 3u;
    DSW(r0 + 0x34u) = 0x1234u;
    DSW(r1 + 0x34u) = 0x5678u;
    DSB(r0 + 0x42u) = 0x5Au;
    DSB(r0 + 0x43u) = 0x5Bu;
    DSB(r1 + 0x42u) = 0x5Cu;
    DSB(r1 + 0x43u) = 0x5Du;
    DSB(r0 + 0x29u) = 0x41u;
    DSB(r1 + 0x29u) = 0x02u;
}

/* Record §41-B. 0x3C208 (EAX = side, EDX = dist; read_memory + capstone at
 * 0x3C208..0x3C32B): latch both slots, 0x18AF8 (0x18B04(0), 0x18B04(1)),
 * clear both records' +0x34/+0x43/+0x42, want = |dist|, d = |0x187FC()|,
 * gap = |d - want|. d >= want (signed `jl`): 0x1883C(other, 0x1A570(side) ?
 * gap : -gap). Closer: e = 0x1A570(side) ? -gap : gap; 0x3B8D8(other, e) = 0
 * gives 0x1883C(other, e), else 0x188DC(other, 0x3B90C(other, e)) and
 * 0x188DC(side, other's new x +/- want). The walls are DS_000BE018 = 0x7C00.
 * 0x14D7C (EAX = side): 0x18B04(other) on the un-latched slots, 0x1088BF = 4
 * when the side's 0x107D2C word >= 4 (signed), both records' slots +0x74 =
 * 0x309, 0x3C208(side, s16 [0x9AFA4 + 2 * other char]), 0x39834(other, the
 * slot's +0x5F), the other record on 0xC91C0[other char] at 3.0, the other
 * slot's +0x41 |= 0x80 and 9/4. 0x14E80 (rec, operand): the other side's
 * DS_001077A8 slot's record +0x55 = 1 and the slot's +0x74 = 0. */
static void check_throw_3c208(void)
{
    u32 s0 = DS_001077B0, s1 = DS_001077B0 + 0x94u;
    u32 r0 = FIGHT_RECS, r1 = FIGHT_RECS + 0x100u;
    u32 a1 = FIGHT_ACTORS + 0x20u, a2 = FIGHT_ACTORS + 0x40u;
    u32 st = FIGHT_RECS + 0x3A00u;
    const s32 W = 0x7C00;
    u32 sv_14ec = DSD(DS_001014EC);
    u16 sv_tw = DSW(TW_ANIM_C4);
    u16 sv_ac = DSW(0x001080ACu), sv_ae = DSW(0x001080AEu);
    u16 sv_w2 = DSW(DS_000A6728 + 2u);
    u16 sv_st[5];
    u8 sv_slots[0x128], sv_7a8[8], sv_b00[4], sv_af0[8], sv_ab0[16];
    u8 sv_scr[0x24], sv_7d20[0x14], sv_8bf = DSB(DS_001088BF);
    u8 sv_78fa = DSB(DS_001078FA), sv_tab[28], sv_a80[0x80], sv_fd[0x18];
    u8 sv_f8[2], sv_e0[4], sv_lo[0x78], sv_c1d = DSB(DS_00100C1D);
    u8 sv_b5a[6], sv_7ed8[0xD], sv_d58[0x180];
    u32 i;
    typedef void (*anim_fn)(u32 rec, u32 arg);
    anim_fn fe80 = (anim_fn)(void *)fn_resolve(0x14E80u);

    for (i = 0; i < 5u; i++) sv_st[i] = DSW(c3_streams[i]);
    tf_snap(sv_slots, DS_001077B0, 0x128u);
    tf_snap(sv_7a8, DS_001077A8, 8u);
    tf_snap(sv_b00, DS_00104B00, 4u);
    tf_snap(sv_af0, 0x00100AF0u, 8u);
    tf_snap(sv_ab0, DS_00100AB0, 16u);
    tf_snap(sv_scr, DS_000D3388, 0x24u);
    tf_snap(sv_7d20, 0x00107D20u, 0x14u);
    tf_snap(sv_b5a, DS_00100B5A, 6u);
    tf_snap(sv_7ed8, DS_00107ED8, 0xDu);
    tf_snap(sv_d58, 0x00107D58u, 0x180u);
    tf_snap(sv_tab, 0x000C8950u, 28u);
    tf_snap(sv_a80, DS_00107A80, 0x80u);
    tf_snap(sv_fd, FIGHT_FD108_T, 0x18u);
    tf_snap(sv_f8, DS_001078F8, 2u);
    tf_snap(sv_e0, DS_001088E0, 4u);
    tf_snap(sv_lo, 0u, 0x78u);

    CHECK_EQ_INT((int)DSD(DS_000BE018), W);

    /* A: d = 4000 >= 3000, 0x1A570(0) holds: side 1 moves +1000 (from 1000
     * to 2000, the record 2000 - 64). Both records' +0x34/+0x42/+0x43
     * cleared; 0x18AF8's facing bits: side 0 (right) clears 0x40, side 1
     * (left) sets it. */
    tw_seed(5000, 1000);
    fighter_3c208(0u, 3000);
    CHECK_EQ_INT((int)DSD(s1 + 0x2Cu), 2000);
    CHECK_EQ_INT((int)DSD(r1 + 0x18u), 1936);
    CHECK_EQ_INT((int)DSD(r1 + 0x1Cu), 9000 - 3200);
    CHECK_EQ_INT((int)DSD(s0 + 0x2Cu), 5000);
    CHECK_EQ_INT((int)DSD(r0 + 0x18u), 4808);
    CHECK_EQ_INT((int)DSW(r0 + 0x34u), 0);
    CHECK_EQ_INT((int)DSW(r1 + 0x34u), 0);
    CHECK_EQ_INT((int)DSB(r0 + 0x42u), 0);
    CHECK_EQ_INT((int)DSB(r0 + 0x43u), 0);
    CHECK_EQ_INT((int)DSB(r1 + 0x42u), 0);
    CHECK_EQ_INT((int)DSB(r1 + 0x43u), 0);
    CHECK_EQ_INT((int)DSB(r0 + 0x29u), 0x01);
    CHECK_EQ_INT((int)DSB(r1 + 0x29u), 0x42);
    /* A2: a negative dist is its magnitude; side 1's actor bit 15 is not the
     * gate. */
    tw_seed(5000, 1000);
    DSW(a2) = 0x8000u;
    fighter_3c208(0u, -3000);
    CHECK_EQ_INT((int)DSD(s1 + 0x2Cu), 2000);
    /* A3: 0x18B04 returns at once in mode 0x22: the +0x29 sentinels stay. */
    tw_seed(5000, 1000);
    DSW(DS_00104B00) = 0x22u;
    fighter_3c208(0u, 3000);
    CHECK_EQ_INT((int)DSB(r0 + 0x29u), 0x41);
    CHECK_EQ_INT((int)DSB(r1 + 0x29u), 0x02);
    CHECK_EQ_INT((int)DSD(s1 + 0x2Cu), 2000);
    /* B: 0x1A570(0) = 0 (actor 1's bit 15): side 1 moves -1000, to 0. */
    tw_seed(5000, 1000);
    DSW(a1) = 0x8000u;
    fighter_3c208(0u, 3000);
    CHECK_EQ_INT((int)DSD(s1 + 0x2Cu), 0);
    CHECK_EQ_INT((int)DSD(r1 + 0x18u), -64);
    CHECK_EQ_INT((int)DSD(s0 + 0x2Cu), 5000);
    /* C: side 1 places side 0 (other = 0): d 4000, 0x1A570(1) holds (actor
     * 2 clear, actor 1 set): side 0 moves +1000. */
    tw_seed(5000, 1000);
    DSW(a1) = 0x8000u;
    fighter_3c208(1u, 3000);
    CHECK_EQ_INT((int)DSD(s0 + 0x2Cu), 6000);
    CHECK_EQ_INT((int)DSD(r0 + 0x18u), 5808);
    CHECK_EQ_INT((int)DSD(s1 + 0x2Cu), 1000);
    /* D: d = want at the right wall (side 0 left of side 1, d = -4000): the
     * `jl` fails at equality, so 0x1883C(1, 0) leaves both; the closer arm
     * would clamp side 1 and put side 0 at 0x7C00 + 4000. */
    tw_seed(W - 4000, W);
    fighter_3c208(0u, 4000);
    CHECK_EQ_INT((int)DSD(s1 + 0x2Cu), W);
    CHECK_EQ_INT((int)DSD(s0 + 0x2Cu), W - 4000);
    /* E: closer (d 2000 < 3000), no wall. 0x1A570(0) holds: side 1 moves
     * -1000; else +1000. */
    tw_seed(5000, 3000);
    fighter_3c208(0u, 3000);
    CHECK_EQ_INT((int)DSD(s1 + 0x2Cu), 2000);
    CHECK_EQ_INT((int)DSD(r1 + 0x18u), 1936);
    CHECK_EQ_INT((int)DSD(s0 + 0x2Cu), 5000);
    tw_seed(5000, 3000);
    DSW(a1) = 0x8000u;
    fighter_3c208(0u, 3000);
    CHECK_EQ_INT((int)DSD(s1 + 0x2Cu), 4000);
    CHECK_EQ_INT((int)DSD(s0 + 0x2Cu), 5000);
    /* F: closer at the left wall, 0x1A570(0) holds: side 1 at -0x7C00 + 500
     * would move -1000 past the wall, so it is clamped to -0x7C00 (0x188DC:
     * the record -0x7C00 - 64) and side 0 goes to -0x7C00 + 3000 (the
     * record that - 192). */
    tw_seed(-W + 2500, -W + 500);
    fighter_3c208(0u, 3000);
    CHECK_EQ_INT((int)DSD(s1 + 0x2Cu), -W);
    CHECK_EQ_INT((int)DSD(r1 + 0x18u), -W - 64);
    CHECK_EQ_INT((int)DSD(s0 + 0x2Cu), -W + 3000);
    CHECK_EQ_INT((int)DSD(r0 + 0x18u), -W + 3000 - 192);
    /* G: closer at the right wall, 0x1A570(0) = 0: side 1 at 0x7C00 - 500
     * moves +1000, clamped to 0x7C00; side 0 to 0x7C00 - 3000. */
    tw_seed(W - 2500, W - 500);
    DSW(a1) = 0x8000u;
    fighter_3c208(0u, 3000);
    CHECK_EQ_INT((int)DSD(s1 + 0x2Cu), W);
    CHECK_EQ_INT((int)DSD(r1 + 0x18u), W - 64);
    CHECK_EQ_INT((int)DSD(s0 + 0x2Cu), W - 3000);
    CHECK_EQ_INT((int)DSD(r0 + 0x18u), W - 3000 - 192);

    /* H: 0x3B90C directly: inside, both edges (x = +/-0x7C00 clamp to
     * themselves), one past each wall and further; side 1 reads its own
     * slot. */
    DSD(s0 + 0x2Cu) = 100u;
    DSD(s1 + 0x2Cu) = (u32)(W - 10);
    CHECK_EQ_INT((int)fighter_3b90c(0u, 50), 150);
    CHECK_EQ_INT((int)fighter_3b90c(1u, 9), W - 1);
    CHECK_EQ_INT((int)fighter_3b90c(1u, 10), W);
    CHECK_EQ_INT((int)fighter_3b90c(1u, 11), W);
    CHECK_EQ_INT((int)fighter_3b90c(1u, 20), W);
    DSD(s1 + 0x2Cu) = (u32)(-W + 10);
    CHECK_EQ_INT((int)fighter_3b90c(1u, -9), -W + 1);
    CHECK_EQ_INT((int)fighter_3b90c(1u, -10), -W);
    CHECK_EQ_INT((int)fighter_3b90c(1u, -11), -W);
    CHECK_EQ_INT((int)fighter_3b90c(1u, -20), -W);

    /* I: 0x14D7C(0), the raptor (side 0, char 3) throwing character 4
     * (0x9AFAC: 0x2F80 = 12160; 0xC91C0[4] = 0xEAE36). Side 1's slot +0x2C
     * holds a stale 8000 (its record 5936, i.e. 6000): 0x18B04(1) runs
     * before any latch and writes the record from it (7936), so the latch
     * keeps 8000. Side 0 at -8000 is left of side 1 and 0x1A570(0) holds:
     * d = 16000, gap 3840, side 1 to 11840 (from 6000: 7840). 0x107D2C[0]
     * = 3 is below 4 (0x1088BF stays), 0x39834 then counts it to 4. */
    CHECK_EQ_INT((int)DSW(0x0009AFACu), 0x2F80);
    CHECK_EQ_INT((int)DSD(0x000C91C0u + 16u), (int)TW_ANIM_C4);
    for (i = 0; i < 3u; i++) {
        static const u16 d2c[3] = { 3u, 4u, 0x8004u };
        static const int b8bf[3] = { 0x5A, 4, 0x5A };
        c3_seed(s0, s1, r0, r1, st);
        DSW(TW_ANIM_C4) = 0x1330u;
        DSW(a1) = 0;
        DSW(a2) = 0;
        DSD(s0 + 0x20u) = 0;
        DSD(s1 + 0x20u) = 0;
        DSB(s0 + 0x42u) = 0;
        DSB(s1 + 0x42u) = 0;
        DSB(s1 + 0x7Au) = 4u;
        DSD(DS_00100AB0) = 192u;
        DSD(DS_00100AB4) = 448u;
        DSD(DS_00100AB0 + 8u) = 64u;
        DSD(DS_00100AB4 + 8u) = 3200u;
        DSD(r0 + 0x18u) = (u32)(-8000 - 192);
        DSD(s0 + 0x2Cu) = (u32)-8000;
        DSD(r1 + 0x18u) = 6000u - 64u;
        DSD(s1 + 0x2Cu) = 8000u;
        DSB(s0 + 0x5Fu) = 0x27u;
        DSW(s0 + 0x74u) = 0x1111u;
        DSW(s1 + 0x74u) = 0x2222u;
        DSW(r0 + 0x34u) = 0x1234u;
        DSB(r1 + 0x43u) = 0x5Du;
        DSW(DS_00107D2C) = d2c[i];
        DSW(DS_00107D2C + 2u) = 9u;
        DSD(DS_00107D28) = 0x5A5A5A5Au;
        DSB(DS_001088BF) = 0x5Au;
        fighter_14d7c(0u);
        CHECK_EQ_INT((int)DSB(DS_001088BF), b8bf[i]);
        CHECK_EQ_INT((int)DSW(DS_00107D2C), (int)(u16)(d2c[i] + 1u));
        CHECK_EQ_INT((int)DSW(DS_00107D2C + 2u), 9);
        CHECK_EQ_INT((int)DSD(DS_00107D28), 0x27);
        CHECK_EQ_INT((int)DSW(s0 + 0x74u), 0x309);
        CHECK_EQ_INT((int)DSW(s1 + 0x74u), 0x309);
        CHECK_EQ_INT((int)DSD(s1 + 0x2Cu), 11840);
        CHECK_EQ_INT((int)DSD(r1 + 0x18u), 11840 - 64);
        CHECK_EQ_INT((int)DSD(s0 + 0x2Cu), -8000);
        CHECK_EQ_INT((int)DSW(r0 + 0x34u), 0);
        CHECK_EQ_INT((int)DSB(r1 + 0x43u), 0);
        CHECK_EQ_INT((int)DSD(r1 + 8u), (int)TW_ANIM_C4);
        CHECK_EQ_INT((int)DSD(r1 + 0x24u), 0x40400000);
        CHECK_EQ_INT((int)(DSW(a2) & 0x7FFFu), 0x1330);
        CHECK_EQ_INT((int)DSD(r0 + 8u), 0x00ABCDEF);
        CHECK_EQ_INT((int)(DSB(s1 + 0x41u) & 0x80u), 0x80);
        CHECK_EQ_INT((int)DSB(s1 + 0x52u), 9);
        CHECK_EQ_INT((int)DSB(s1 + 0x53u), 4);
    }

    /* J: 0x14E80 as anim_indirect calls it, (rec, operand): side 0's record
     * marks side 1's record +0x55 and zeroes side 1's slot +0x74, and back;
     * no other slot returns before any dereference (mem[0] planted). */
    CHECK(fe80 != NULL, "0x14E80 resolves");
    if (fe80 != NULL) {
        tb_seed(s0, s1, r0, r1);
        DSB(r0 + 0x55u) = 0x66u;
        DSB(r1 + 0x55u) = 0x77u;
        DSW(s0 + 0x74u) = 0x1111u;
        DSW(s1 + 0x74u) = 0x2222u;
        fe80(r0, 0xFFFFu);
        CHECK_EQ_INT((int)DSB(r1 + 0x55u), 1);
        CHECK_EQ_INT((int)DSW(s1 + 0x74u), 0);
        CHECK_EQ_INT((int)DSB(r0 + 0x55u), 0x66);
        CHECK_EQ_INT((int)DSW(s0 + 0x74u), 0x1111);
        fe80(r1, 0u);
        CHECK_EQ_INT((int)DSB(r0 + 0x55u), 1);
        CHECK_EQ_INT((int)DSW(s0 + 0x74u), 0);
        DSD(DS_001077A8 + 4u) = 0;
        DSD(0u) = r1;
        DSB(r1 + 0x55u) = 0x33u;
        DSW(0x74u) = 0x4444u;
        fe80(r0, 0u);
        CHECK_EQ_INT((int)DSB(r1 + 0x55u), 0x33);
        CHECK_EQ_INT((int)DSW(0x74u), 0x4444);
    }

    tf_put(sv_lo, 0u, 0x78u);
    tf_put(sv_e0, DS_001088E0, 4u);
    tf_put(sv_f8, DS_001078F8, 2u);
    tf_put(sv_fd, FIGHT_FD108_T, 0x18u);
    tf_put(sv_a80, DS_00107A80, 0x80u);
    tf_put(sv_tab, 0x000C8950u, 28u);
    tf_put(sv_d58, 0x00107D58u, 0x180u);
    tf_put(sv_7ed8, DS_00107ED8, 0xDu);
    tf_put(sv_b5a, DS_00100B5A, 6u);
    tf_put(sv_7d20, 0x00107D20u, 0x14u);
    tf_put(sv_scr, DS_000D3388, 0x24u);
    tf_put(sv_ab0, DS_00100AB0, 16u);
    tf_put(sv_af0, 0x00100AF0u, 8u);
    tf_put(sv_b00, DS_00104B00, 4u);
    tf_put(sv_7a8, DS_001077A8, 8u);
    tf_put(sv_slots, DS_001077B0, 0x128u);
    for (i = 0; i < 5u; i++) DSW(c3_streams[i]) = sv_st[i];
    DSB(DS_00100C1D) = sv_c1d;
    DSB(DS_001078FA) = sv_78fa;
    DSB(DS_001088BF) = sv_8bf;
    DSW(DS_000A6728 + 2u) = sv_w2;
    DSW(0x001080ACu) = sv_ac;
    DSW(0x001080AEu) = sv_ae;
    DSW(TW_ANIM_C4) = sv_tw;
    DSD(DS_001014EC) = sv_14ec;
}

/* ---- §42-B: 0x3C358/0x3E244, 0x36280 and 0x48AAC/0x48D94 ---------------- */

#define G2_ST     (FIGHT_RECS + 0x6000u)   /* crafted one-word streams, 0x10 apart */
#define G2_C3ST   (FIGHT_RECS + 0x6200u)   /* c3_seed's 0xC8950 streams */
#define G2_DESC   (FIGHT_RECS + 0x6300u)   /* a crafted descriptor */
#define G2_ROW    (FIGHT_RECS + 0x6400u)   /* a crafted 0xA8A98 row */
#define G2_REC2   (FIGHT_RECS + 0x200u)    /* a third record */
#define G2_HANDLE 0x7F00BEEFu              /* a handle palette entry 0 holds */
#define G2_JUNK   (FIGHT_RECS + 0x6500u)   /* a harmless link target */

/* The §42-B checks restore the whole data object and both real pools, so no
 * global, table patch or pool record they touch outlives them. */
static u8 g2_data[0x10B0D0u - 0x80000u];
static u8 g2_rec[0xEBA0u], g2_pset[0x4880u];
static u32 g2_rec_pool, g2_pset_pool;

static void g2_save(void)
{
    g2_rec_pool = DSD(DS_001014F4);
    g2_pset_pool = DSD(DS_001014EC);
    tf_snap(g2_data, 0x80000u, sizeof g2_data);
    tf_snap(g2_rec, g2_rec_pool, sizeof g2_rec);
    tf_snap(g2_pset, g2_pset_pool, sizeof g2_pset);
}

static void g2_restore(void)
{
    tf_put(g2_data, 0x80000u, sizeof g2_data);
    tf_put(g2_rec, g2_rec_pool, sizeof g2_rec);
    tf_put(g2_pset, g2_pset_pool, sizeof g2_pset);
}

/* A one-word stream (a plain frame id, bit 15 clear) at G2_ST + 0x10 * k. */
static u32 g2_stream(u32 k, u16 id)
{
    DSW(G2_ST + k * 0x10u) = id;
    return G2_ST + k * 0x10u;
}

/* The first active record whose stream cursor is `stream`, or 0. */
static u32 g2_find(u32 stream)
{
    u32 r;
    for (r = actor_list_head(); r != 0; r = actor_next(r))
        if (DSD(r + 8u) == stream) return r;
    return 0;
}

/* c3_seed for 0x3E244 with side `a` the T-rex (char 0) and the other side
 * char 4 (0xC759C[4] = 0x1700, [0] = 0x1300), a pool with three dummy
 * records (so no spawn's pset lands on pset 1/2), the psets' bit 15 clear,
 * side a at x 5000 and the other at 1000 (records minus the f = 772 offsets
 * 192/64 and 448/3200, y 9000), DS_001078FA = 2, and sentinels in every
 * field 0x3E244 writes: both records' +0x59, +0x4B, +0x34, +0x42, +0x43 and
 * +0x1C, both slots' +0x74, 0x107D28/0x107D2C, 0xC760C's two +0x10s. The
 * 0xC90F8 table points at one-word streams and 0xE843A's first word is
 * patched, all to frame 0 (sprite 0 keeps 0x18540's anchor at 0, as in
 * bp_seed, so the latched x are the seeded ones), and char 0's 0xA8A98 row
 * is zero (no palette acquire in 0x3E0F0). */
static void g2h_seed(u32 a)
{
    u32 s[2], r[2], i, b = 1u - a;
    static const u32 ab0[2] = { 192u, 64u }, ab4[2] = { 448u, 3200u };
    u32 x[2];
    s[0] = DS_001077B0; s[1] = DS_001077B0 + 0x94u;
    r[0] = FIGHT_RECS; r[1] = FIGHT_RECS + 0x100u;
    c3_seed(s[0], s[1], r[0], r[1], G2_C3ST);
    actors_reset();
    (void)actor_alloc(0);
    (void)actor_alloc(0);
    (void)actor_alloc(0);
    DSB(DS_001078FA) = 2u;
    DSB(DS_00104B14) = 1u;                   /* 0x399AC: no 0x4F434 */
    DSB(s[a] + 0x7Au) = 0u;
    DSB(s[b] + 0x7Au) = 4u;
    x[a] = 5000u;
    x[b] = 1000u;
    for (i = 0; i < 2u; i++) {
        DSD(DS_00100AB0 + i * 8u) = ab0[i];
        DSD(DS_00100AB4 + i * 8u) = ab4[i];
        DSD(s[i] + 0x20u) = 0;
        DSB(s[i] + 0x42u) = 0;
        DSD(s[i] + 0x2Cu) = x[i];
        DSW(s[i] + 0x74u) = (u16)(0x1111u * (1u + i));
        DSD(r[i] + 0x18u) = x[i] - ab0[i];
        DSD(r[i] + 0x1Cu) = 9000u - ab4[i];
        DSB(r[i] + 0x59u) = (u8)(0x33u + i);
        DSB(r[i] + 0x4Bu) = (u8)(0x77u + i);
        DSW(r[i] + 0x34u) = (u16)(0x1234u + i);
        DSB(r[i] + 0x42u) = (u8)(0x5Au + i);
        DSB(r[i] + 0x43u) = (u8)(0x5Cu + i);
        DSW(FIGHT_ACTORS + (1u + i) * 0x20u) = 0;
    }
    DSB(s[a] + 0x5Fu) = 0x27u;
    DSW(DS_00107D2C) = 3u;
    DSW(DS_00107D2C + 2u) = 9u;
    DSD(DS_00107D28) = 0x5A5A5A5Au;
    for (i = 0; i < 7u; i++)
        DSD(0x000C90F8u + i * 4u) = g2_stream(i, 0u);
    DSW(0x000E843Au) = 0;
    DSD(DS_000A8A98) = G2_ROW;
    mem_fill(G2_ROW, 0, 0x20u);
    DSD(0x000BB3F8u + 0x10u) = 0x5A5A5A5Au;
    DSD(0x000BB40Cu + 0x10u) = 0x6B6B6B6Bu;
}

/* Record §42-B. 0x3C358 (EAX = side): the side's slot 9/7 and +0x42 |= 4,
 * the other slot 0x10/0x0A, +0x54 = 0 and +0x0C = 0; both slot records'
 * +0x34/+0x43/+0x42 and +0x1C cleared. 0x3E244 (EAX = side, the +0x1C
 * callback 0x3E3A8 stores): 0x34D8C (+0x59 1/0xFF), the side's record on
 * 0xE843A (first word 0x10BA, patched to 0) at 2.0 through 0x3C4CC (+0x52 =
 * 0: 0x2BC30),
 * 0x3E0F0's child, the other record on 0xC90F8[other char] at 2.0,
 * 0x3C208(side, 0xC759C[other char]), 0x18AF8, 0x39834(other, +0x5F),
 * 0x3C358, both +0x74 = 0x309, then +0x57 = 2 and the other's +0x53 = 0x0F.
 * With x 5000/1000 and want 0x1700 = 5888 the closer arm moves the other
 * side by -1888 (0x1A570 holds) to -888. */
static void check_hold_3e244(void)
{
    u32 s0 = DS_001077B0, s1 = DS_001077B0 + 0x94u;
    u32 r0 = FIGHT_RECS, r1 = FIGHT_RECS + 0x100u;
    u32 n, e;

    g2_save();
    CHECK(fn_resolve(0x3E244u) == (void (*)(void))fighter_3e244,
          "0x3E244 is registered as fighter_3e244");
    CHECK_EQ_INT((int)DSW(0x000C759Cu + 8u), 0x1700);
    CHECK_EQ_INT((int)DSW(0x000E843Au), 0x10BA);

    /* A: 0x3C358(0) directly. */
    g2h_seed(0u);
    DSB(s0 + 0x52u) = 0x55u; DSB(s0 + 0x53u) = 0x66u; DSB(s0 + 0x54u) = 0x77u;
    DSB(s0 + 0x42u) = 0x21u; DSD(s0 + 0x0Cu) = 0x0C0C0C0Cu;
    DSB(s1 + 0x52u) = 0x56u; DSB(s1 + 0x53u) = 0x67u; DSB(s1 + 0x54u) = 0x78u;
    DSB(s1 + 0x42u) = 0x22u; DSD(s1 + 0x0Cu) = 0x0D0D0D0Du;
    fighter_3c358(0u);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 9);
    CHECK_EQ_INT((int)DSB(s0 + 0x53u), 7);
    CHECK_EQ_INT((int)DSB(s0 + 0x42u), 0x25);
    CHECK_EQ_INT((int)DSB(s0 + 0x54u), 0x77);
    CHECK_EQ_INT((int)DSD(s0 + 0x0Cu), 0x0C0C0C0C);
    CHECK_EQ_INT((int)DSB(s1 + 0x52u), 0x10);
    CHECK_EQ_INT((int)DSB(s1 + 0x53u), 0x0A);
    CHECK_EQ_INT((int)DSB(s1 + 0x54u), 0);
    CHECK_EQ_INT((int)DSD(s1 + 0x0Cu), 0);
    CHECK_EQ_INT((int)DSB(s1 + 0x42u), 0x22);
    CHECK_EQ_INT((int)DSW(r0 + 0x34u), 0);
    CHECK_EQ_INT((int)DSB(r0 + 0x42u), 0);
    CHECK_EQ_INT((int)DSB(r0 + 0x43u), 0);
    CHECK_EQ_INT((int)DSD(r0 + 0x1Cu), 0);
    CHECK_EQ_INT((int)DSW(r1 + 0x34u), 0);
    CHECK_EQ_INT((int)DSB(r1 + 0x42u), 0);
    CHECK_EQ_INT((int)DSB(r1 + 0x43u), 0);
    CHECK_EQ_INT((int)DSD(r1 + 0x1Cu), 0);
    CHECK_EQ_INT((int)DSD(r0 + 0x18u), 5000 - 192);
    /* A2: 0x3C358(1): the roles swap. */
    g2h_seed(0u);
    DSB(s0 + 0x42u) = 0x21u; DSB(s0 + 0x54u) = 0x77u;
    DSD(s0 + 0x0Cu) = 0x0C0C0C0Cu;
    DSB(s1 + 0x42u) = 0x22u; DSB(s1 + 0x54u) = 0x78u;
    DSD(s1 + 0x0Cu) = 0x0D0D0D0Du;
    fighter_3c358(1u);
    CHECK_EQ_INT((int)DSB(s1 + 0x52u), 9);
    CHECK_EQ_INT((int)DSB(s1 + 0x53u), 7);
    CHECK_EQ_INT((int)DSB(s1 + 0x42u), 0x26);
    CHECK_EQ_INT((int)DSB(s1 + 0x54u), 0x78);
    CHECK_EQ_INT((int)DSD(s1 + 0x0Cu), 0x0D0D0D0D);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 0x10);
    CHECK_EQ_INT((int)DSB(s0 + 0x53u), 0x0A);
    CHECK_EQ_INT((int)DSB(s0 + 0x54u), 0);
    CHECK_EQ_INT((int)DSD(s0 + 0x0Cu), 0);
    CHECK_EQ_INT((int)DSB(s0 + 0x42u), 0x21);
    CHECK_EQ_INT((int)DSD(r0 + 0x1Cu), 0);
    CHECK_EQ_INT((int)DSD(r1 + 0x1Cu), 0);

    /* B: 0x3E244(0), the T-rex throwing character 4. */
    g2h_seed(0u);
    n = tb_active();
    fighter_3e244(0u);
    CHECK_EQ_INT((int)DSB(r0 + 0x59u), 1);                  /* 0x34D8C */
    CHECK_EQ_INT((int)DSB(r1 + 0x59u), 0xFF);
    CHECK_EQ_INT((int)DSD(r0 + 8u), 0x000E843A);
    CHECK_EQ_INT((int)DSD(r0 + 0x24u), 0x40000000);
    CHECK_EQ_INT((int)DSD(r1 + 8u), (int)(G2_ST + 0x40u));  /* 0xC90F8[4] */
    CHECK_EQ_INT((int)DSD(r1 + 0x24u), 0x40000000);
    CHECK_EQ_INT((int)tb_active(), (int)(n + 1u));          /* 0x3E0F0 */
    e = actor_record((u32)DSB(r0 + 0x4Bu));
    CHECK(DSB(r0 + 0x4Bu) != 0x77u && e != 0u,
          "0x3E0F0 spawned the side record's child");
    if (DSB(r0 + 0x4Bu) != 0x77u && e != 0u)
        CHECK_EQ_INT((int)DSB(e + 0x60u), 1);
    CHECK_EQ_INT((int)DSB(r1 + 0x4Bu), 0x78);
    CHECK_EQ_INT((int)DSD(0x000BB3F8u + 0x10u), 0);
    CHECK_EQ_INT((int)DSD(0x000BB40Cu + 0x10u), 0x6B6B6B6B);
    CHECK_EQ_INT((int)DSD(s1 + 0x2Cu), -888);               /* 0x3C208 */
    CHECK_EQ_INT((int)DSD(r1 + 0x18u), -888 - 64);
    CHECK_EQ_INT((int)DSD(s0 + 0x2Cu), 5000);
    CHECK_EQ_INT((int)DSD(DS_00107D28), 0x27);              /* 0x39834 */
    CHECK_EQ_INT((int)DSW(DS_00107D2C), 4);
    CHECK_EQ_INT((int)DSW(DS_00107D2C + 2u), 9);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 9);                  /* 0x3C358 */
    CHECK_EQ_INT((int)DSB(s0 + 0x53u), 7);
    CHECK_EQ_INT((int)DSB(s0 + 0x42u), 0x04);
    CHECK_EQ_INT((int)DSB(s1 + 0x52u), 0x10);
    CHECK_EQ_INT((int)DSB(s1 + 0x54u), 0);
    CHECK_EQ_INT((int)DSD(s1 + 0x0Cu), 0);
    CHECK_EQ_INT((int)DSD(r0 + 0x1Cu), 0);
    CHECK_EQ_INT((int)DSD(r1 + 0x1Cu), 0);
    CHECK_EQ_INT((int)DSW(r0 + 0x34u), 0);
    CHECK_EQ_INT((int)DSB(r1 + 0x43u), 0);
    CHECK_EQ_INT((int)DSW(s0 + 0x74u), 0x309);              /* 0x39A10 */
    CHECK_EQ_INT((int)DSW(s1 + 0x74u), 0x309);
    CHECK_EQ_INT((int)DSB(s0 + 0x57u), 2);
    CHECK_EQ_INT((int)DSB(s1 + 0x57u), 0x9A);
    CHECK_EQ_INT((int)DSB(s1 + 0x53u), 0x0F);

    /* C: 0x3E244(1), the mirror (side 1 the T-rex, side 0 char 4). */
    g2h_seed(1u);
    n = tb_active();
    fighter_3e244(1u);
    CHECK_EQ_INT((int)DSB(r1 + 0x59u), 1);
    CHECK_EQ_INT((int)DSB(r0 + 0x59u), 0xFF);
    CHECK_EQ_INT((int)DSD(r1 + 8u), 0x000E843A);
    CHECK_EQ_INT((int)DSD(r0 + 8u), (int)(G2_ST + 0x40u));
    CHECK_EQ_INT((int)tb_active(), (int)(n + 1u));
    CHECK(DSB(r1 + 0x4Bu) != 0x78u, "0x3E0F0 spawned side 1's child");
    CHECK_EQ_INT((int)DSB(r0 + 0x4Bu), 0x77);
    CHECK_EQ_INT((int)DSD(0x000BB40Cu + 0x10u), 0);
    CHECK_EQ_INT((int)DSD(0x000BB3F8u + 0x10u), 0x5A5A5A5A);
    CHECK_EQ_INT((int)DSD(s0 + 0x2Cu), -888);
    CHECK_EQ_INT((int)DSD(r0 + 0x18u), -888 - 192);
    CHECK_EQ_INT((int)DSD(s1 + 0x2Cu), 5000);
    CHECK_EQ_INT((int)DSD(DS_00107D28), 0x27);
    CHECK_EQ_INT((int)DSW(DS_00107D2C), 3);
    CHECK_EQ_INT((int)DSW(DS_00107D2C + 2u), 10);
    CHECK_EQ_INT((int)DSB(s1 + 0x52u), 9);
    CHECK_EQ_INT((int)DSB(s1 + 0x53u), 7);
    CHECK_EQ_INT((int)DSB(s1 + 0x57u), 2);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 0x10);
    CHECK_EQ_INT((int)DSB(s0 + 0x53u), 0x0F);
    CHECK_EQ_INT((int)DSB(s0 + 0x57u), 0x99);
    CHECK_EQ_INT((int)DSW(s0 + 0x74u), 0x309);
    CHECK_EQ_INT((int)DSW(s1 + 0x74u), 0x309);

    /* D: the T-rex at 4000 facing away from char 4 at 5000: its record's
     * +0x28 bit 14 (+0x29 bit 6) is clear when 0x3C4CC writes the pset, so
     * 0x1A570(0) holds and, with d = 1000 < 5888, 0x3C208 moves side 1 by
     * -4888 to 112, past side 0. 0x3C208's own 0x18AF8 ran before the move
     * (side 0 left: bit 6 set; side 1 clear); 0x3E244's 0x18AF8 after it
     * flips both back. */
    g2h_seed(0u);
    DSD(r0 + 0x18u) = 4000u - 192u;
    DSD(s0 + 0x2Cu) = 4000u;
    DSD(r1 + 0x18u) = 5000u - 64u;
    DSD(s1 + 0x2Cu) = 5000u;
    DSB(r0 + 0x29u) = 0x01u;
    DSB(r1 + 0x29u) = 0x42u;
    fighter_3e244(0u);
    CHECK_EQ_INT((int)DSD(s1 + 0x2Cu), 112);
    CHECK_EQ_INT((int)DSD(s0 + 0x2Cu), 4000);
    CHECK_EQ_INT((int)DSB(r0 + 0x29u), 0x01);
    CHECK_EQ_INT((int)DSB(r1 + 0x29u), 0x42);

    g2_restore();
}

/* bp_seed(3000, 9000, 1000, 9000) plus a pool with three dummy records,
 * 0xBB1DC's stream pointed at a one-word stream (id 0x1250) with no palette
 * handle, and sentinels in every field 0x36280 writes. */
static void g2l_seed(void)
{
    u32 s[2], r[2], i;
    bp_seed(3000, 9000, 1000, 9000);
    actors_reset();
    (void)actor_alloc(0);
    (void)actor_alloc(0);
    (void)actor_alloc(0);
    s[0] = DS_001077B0; s[1] = DS_001077B0 + 0x94u;
    r[0] = FIGHT_RECS; r[1] = FIGHT_RECS + 0x100u;
    for (i = 0; i < 2u; i++) {
        DSD(r[i] + 0x14u) = s[i];
        DSB(r[i] + 0x51u) = (u8)i;
        DSW(r[i] + 0x56u) = (u16)(1u + i);
        DSD(r[i] + 0x24u) = 0x11111111u;
        DSD(r[i] + 0x30u) = 0xFEDC1234u + i;
        DSW(r[i] + 0x36u) = 0x3636u;
        DSB(s[i] + 0x42u) = (u8)(0x05u + i * 0x10u);
        DSB(s[i] + 0x52u) = 0x0Du;
        DSB(s[i] + 0x54u) = 0x44u;
        DSB(s[i] + 0x58u) = 0x41u;
    }
    DSD(0x000BB1DCu) = g2_stream(7u, 0x1250u);
    DSD(0x000BB1DCu + 0x10u) = 0;
}

/* Record §42-B. 0x36280 (EAX = rec; the 0xD000 target of the 0xC90A8/0xC90D0
 * fall streams): with the owner slot rec+0x14, +0x58 = 3 for +0x52 0x0D, else
 * +0x58 + 1; 0x188AC(rec+0x51, rec+0x18, 0) (the slot record's +0x18/+0x1C,
 * then the latch: x 3000 over the 0xDEADBEEF sentinel); +0x54 = 0, +0x42 bit 2
 * clear; the record's +0x36 = 0 and +0x24 = 3.0; 0xBB1DC spawned at (the
 * latched slot+0x2C, the s16 rec+0x32, 0) with a5 = 0. */
static void check_land_36280(void)
{
    u32 s0 = DS_001077B0, s1 = DS_001077B0 + 0x94u;
    u32 r0 = FIGHT_RECS, r1 = FIGHT_RECS + 0x100u;
    typedef void (*anim_fn)(u32 rec, u32 arg);
    anim_fn f;
    u32 n, c;

    g2_save();
    f = (anim_fn)(void *)fn_resolve(0x36280u);
    CHECK(f != NULL, "0x36280 is a registered stream target");
    CHECK_EQ_INT((int)DSD(0x000BB1DCu), 0x000E8CA8);

    /* A: record 0, +0x52 = 0x0D, through the registered target. */
    g2l_seed();
    n = tb_active();
    if (f != NULL) f(r0, 0xFFFFu);
    CHECK_EQ_INT((int)DSB(s0 + 0x58u), 3);
    CHECK_EQ_INT((int)DSD(r0 + 0x18u), 3000 - 192);
    CHECK_EQ_INT((int)DSD(r0 + 0x1Cu), 0);
    CHECK_EQ_INT((int)DSD(s0 + 0x2Cu), 3000);
    CHECK_EQ_INT((int)DSB(s0 + 0x54u), 0);
    CHECK_EQ_INT((int)DSB(s0 + 0x42u), 0x01);
    CHECK_EQ_INT((int)DSW(r0 + 0x36u), 0);
    CHECK_EQ_INT((int)DSD(r0 + 0x24u), 0x40400000);
    CHECK_EQ_INT((int)tb_active(), (int)(n + 1u));
    c = g2_find(G2_ST + 0x70u);
    CHECK(c != 0u, "0x36280 spawned 0xBB1DC");
    if (c != 0u) {
        CHECK_EQ_INT((int)DSD(c + 0x18u), 3000);
        CHECK_EQ_INT((int)DSD(c + 0x1Cu), 0);
        CHECK_EQ_INT((int)DSW(c + 0x32u), 0xFEDC);
        CHECK_EQ_INT((int)(DSW(c + 0x28u) & 0x0400u), 0);
    }
    CHECK_EQ_INT((int)DSB(s1 + 0x58u), 0x41);
    CHECK_EQ_INT((int)DSB(s1 + 0x54u), 0x44);
    CHECK_EQ_INT((int)DSD(s1 + 0x2Cu), (int)0xDEADBEEFu);
    CHECK_EQ_INT((int)DSW(r1 + 0x36u), 0x3636);

    /* B: +0x52 = 0x0C: +0x58 + 1. */
    g2l_seed();
    DSB(s0 + 0x52u) = 0x0Cu;
    fighter_36280(r0);
    CHECK_EQ_INT((int)DSB(s0 + 0x58u), 0x42);
    CHECK_EQ_INT((int)DSW(r0 + 0x36u), 0);

    /* C: no owner slot: nothing is written and nothing spawns. */
    g2l_seed();
    DSD(r0 + 0x14u) = 0;
    n = tb_active();
    fighter_36280(r0);
    CHECK_EQ_INT((int)DSB(s0 + 0x58u), 0x41);
    CHECK_EQ_INT((int)DSW(r0 + 0x36u), 0x3636);
    CHECK_EQ_INT((int)DSD(r0 + 0x24u), 0x11111111);
    CHECK_EQ_INT((int)DSD(r0 + 0x1Cu), 9000 - 448);
    CHECK_EQ_INT((int)tb_active(), (int)n);

    /* D: a third record owned by slot 0 but with +0x51 = 1: 0x188AC anchors
     * side 1 (record 1 takes its +0x18 and y 0, slot 1 latches 5000), slot 0
     * takes the state writes, and the spawn x is slot 0's +0x2C (the
     * unlatched sentinel). */
    g2l_seed();
    mem_fill(G2_REC2, 0, ACTOR_REC_SIZE);
    DSD(G2_REC2 + 0x14u) = s0;
    DSB(G2_REC2 + 0x51u) = 1u;
    DSD(G2_REC2 + 0x18u) = 5000u - 64u;
    DSD(G2_REC2 + 0x30u) = 0x07770000u;
    DSD(s0 + 0x2Cu) = 0x1357u;
    fighter_36280(G2_REC2);
    CHECK_EQ_INT((int)DSD(r1 + 0x18u), 5000 - 64);
    CHECK_EQ_INT((int)DSD(r1 + 0x1Cu), 0);
    CHECK_EQ_INT((int)DSD(s1 + 0x2Cu), 5000);
    CHECK_EQ_INT((int)DSB(s0 + 0x58u), 3);
    CHECK_EQ_INT((int)DSB(s1 + 0x58u), 0x41);
    CHECK_EQ_INT((int)DSB(s0 + 0x54u), 0);
    CHECK_EQ_INT((int)DSW(G2_REC2 + 0x36u), 0);
    CHECK_EQ_INT((int)DSW(r0 + 0x36u), 0x3636);
    CHECK_EQ_INT((int)DSD(r0 + 0x1Cu), 9000 - 448);
    c = g2_find(G2_ST + 0x70u);
    CHECK(c != 0u, "0x36280 spawned 0xBB1DC for the third record");
    if (c != 0u) {
        CHECK_EQ_INT((int)DSD(c + 0x18u), 0x1357);
        CHECK_EQ_INT((int)DSW(c + 0x32u), 0x0777);
    }

    g2_restore();
}

/* bp_seed(5000, 9000, 1000, 9000), a pool with three dummies, side 0 char 2
 * (the finisher's owner), side 1 char 4, both slots' +0x2C set (0x48AAC reads
 * them unlatched), both records' +0x56 = 1/2 and +0x51 = 0/1, DS_00104B14 =
 * 0 (0x370F0's other-slot arm), the psets' bit 15 clear, and sentinels:
 * DS_00104AE9 0x81, the timer 0x1234, 0x108399 0xA5, DS_001078FC 0x66,
 * DS_000F0AFE 0x99, DS_000C951C, both +0x53/+0x52/+0x57, and the type-0x2D
 * list region 0x1082E0..0x10836F zero (a null link, not a wild one, if a
 * sentinel store is missing). */
static void g2f_seed(void)
{
    u32 s[2], r[2], i;
    bp_seed(5000, 9000, 1000, 9000);
    actors_reset();
    (void)actor_alloc(0);
    (void)actor_alloc(0);
    (void)actor_alloc(0);
    s[0] = DS_001077B0; s[1] = DS_001077B0 + 0x94u;
    r[0] = FIGHT_RECS; r[1] = FIGHT_RECS + 0x100u;
    for (i = 0; i < 2u; i++) {
        DSB(r[i] + 0x51u) = (u8)i;
        DSW(r[i] + 0x56u) = (u16)(1u + i);
        DSD(r[i] + 0x08u) = 0x00ABCDEFu;
        DSB(r[i] + 0x53u) = 0x35u;
        DSB(s[i] + 0x52u) = (u8)(0x55u + i);
        DSB(s[i] + 0x53u) = (u8)(0x66u + i);
        DSB(s[i] + 0x57u) = 0;
        DSB(s[i] + 0x42u) = 0;
        DSW(FIGHT_ACTORS + (1u + i) * 0x20u) = 0;
    }
    DSD(s[0] + 0x2Cu) = 5000u;
    DSD(s[1] + 0x2Cu) = 1000u;
    DSB(s[0] + 0x7Au) = 2u;
    DSB(s[1] + 0x7Au) = 4u;
    DSB(DS_00104B14) = 0;
    DSB(DS_00104AE9) = 0x81u;
    DSW(DS_00108390) = 0x1234u;
    DSB(DS_00108399) = 0xA5u;
    DSB(DS_001078FC) = 0x66u;
    DSB(DS_000F0AFE) = 0x99u;
    DSD(DS_000C951C) = 0x51C51C51u;
    DSW(DS_001078F6) = 0;
    mem_fill(DS_001082E0, 0, 0x90u);         /* the type-0x2D lists, unbuilt */
}

/* Record §42-B. 0x48AAC (EAX = slot, EDX = rec, EBX = side; the +0x0C
 * callback 0x48BE0 stores): +0x57 0 closes to |dx| <= 0x100 (signed), then
 * 0x188DC(side, other x), DS_00104AE9 |= 4, rec+0x34 = 0, timer 0xF0, 1; 1
 * ticks the word timer DS_00108390 and at <= 0 runs 0x380C4(other) (the
 * other record on 0xBDDC4[char] at 1.0 and 0xBDDE0[char] spawned as its
 * child with +0x59 = 1), 0x3C190(rec+0x51, -0x80), clears the bit, timer
 * 0x3C, 2; 2 ticks and at <= 0 starts rec on 0xEDD64 at 2.0, 0x370F0(the
 * other record), 9/3 and DS_001078FC = 1; above 2 returns. 0x48D94 (same
 * registers; 0x48F54 stores it): 0 and > 4 return; 1 builds the type-0x2D
 * lists (0x48C4C) and goes to 2; 2 spawns 0xC950C (speed +/-(rng(0x80) +
 * 0x80), x DS_000F0AF0 -/+ 0x2A00, y word +0x32 - 0x200 + rng(0x200), the
 * pset word +0x2E, 0xC950C's +0x10 = 0x29C08) and goes to 3 with the next
 * state 2 (timer rng(4) + 4) or, at a type-0x2D count >= 8 (signed), 4
 * (timer 0xB4); 3 ticks to the next state; 4 starts rec on 0xED4FA at 3.0,
 * 0x370F0(the other record), 9/3 and DS_001078FC = 1. */
static void check_finisher_48aac(void)
{
    u32 s0 = DS_001077B0, s1 = DS_001077B0 + 0x94u;
    u32 r0 = FIGHT_RECS, r1 = FIGHT_RECS + 0x100u;
    u32 p1 = FIGHT_ACTORS + 0x20u;
    u32 n, c, i, st2, st3, q1, q2, q3, par;

    g2_save();
    CHECK(fn_resolve(0x48AACu) == (void (*)(void))fighter_48aac,
          "0x48AAC is registered as fighter_48aac");
    CHECK(fn_resolve(0x48D94u) == (void (*)(void))fighter_48d94,
          "0x48D94 is registered as fighter_48d94");

    /* A: +0x57 = 0 out of range both ways (0x101): nothing. */
    g2f_seed();
    DSD(s1 + 0x2Cu) = 5000u + 0x101u;
    fighter_48aac(s0, r0, 0u);
    DSD(s1 + 0x2Cu) = 5000u - 0x101u;
    fighter_48aac(s0, r0, 0u);
    CHECK_EQ_INT((int)DSB(s0 + 0x57u), 0);
    CHECK_EQ_INT((int)DSD(s0 + 0x2Cu), 5000);
    CHECK_EQ_INT((int)DSW(DS_00108390), 0x1234);
    CHECK_EQ_INT((int)DSB(DS_00104AE9), 0x81);
    /* A2: 0x100 closes: side 0 onto the other's x 4744 (record 4552). */
    DSD(s1 + 0x2Cu) = 5000u - 0x100u;
    fighter_48aac(s0, r0, 0u);
    CHECK_EQ_INT((int)DSD(s0 + 0x2Cu), 4744);
    CHECK_EQ_INT((int)DSD(r0 + 0x18u), 4744 - 192);
    CHECK_EQ_INT((int)DSB(DS_00104AE9), 0x85);
    CHECK_EQ_INT((int)DSW(r0 + 0x34u), 0);
    CHECK_EQ_INT((int)DSW(DS_00108390), 0xF0);
    CHECK_EQ_INT((int)DSB(s0 + 0x57u), 1);
    CHECK_EQ_INT((int)DSD(s1 + 0x2Cu), 4744);
    /* A3: the compare is signed: 0x80000000 - 0 negates to itself (< 0). */
    g2f_seed();
    DSD(s0 + 0x2Cu) = 0x80000000u;
    DSD(s1 + 0x2Cu) = 0;
    fighter_48aac(s0, r0, 0u);
    CHECK_EQ_INT((int)DSB(s0 + 0x57u), 1);
    /* A4: the passed record (EDX), not the slot's, takes +0x34 = 0. */
    g2f_seed();
    DSD(s1 + 0x2Cu) = 5000u;
    mem_fill(G2_REC2, 0, ACTOR_REC_SIZE);
    DSW(G2_REC2 + 0x34u) = 0x3434u;
    DSW(r0 + 0x34u) = 0x1111u;
    fighter_48aac(s0, G2_REC2, 0u);
    CHECK_EQ_INT((int)DSW(G2_REC2 + 0x34u), 0);
    CHECK_EQ_INT((int)DSW(r0 + 0x34u), 0x1111);

    /* B: +0x57 = 1, timer 2 -> 1 holds. */
    g2f_seed();
    DSD(0x000BDDC4u + 16u) = g2_stream(8u, 0x1260u);
    DSD(0x000BDDE0u + 16u) = G2_DESC;
    mem_fill(G2_DESC, 0, 0x14u);
    DSD(G2_DESC) = g2_stream(9u, 0x1270u);
    DSB(s0 + 0x57u) = 1u;
    DSW(DS_00108390) = 2u;
    DSB(DS_00104AE9) = 0x85u;
    DSW(r1 + 0x28u) = 0x4000u;
    DSW(r0 + 0x34u) = 0x3434u;
    n = tb_active();
    fighter_48aac(s0, r0, 0u);
    CHECK_EQ_INT((int)DSW(DS_00108390), 1);
    CHECK_EQ_INT((int)DSB(s0 + 0x57u), 1);
    CHECK_EQ_INT((int)DSD(r1 + 8u), 0x00ABCDEF);
    CHECK_EQ_INT((int)DSB(DS_00104AE9), 0x85);
    /* B2: 1 -> 0 fires: 0x380C4(1), 0x3C190(0, -0x80) (unflipped: +0x80),
     * bit 2 cleared, timer 0x3C, +0x57 = 2. */
    par = actor_record(2u);
    DSB(par + 0x4Fu) = 0;
    fighter_48aac(s0, r0, 0u);
    CHECK_EQ_INT((int)DSD(r1 + 8u), (int)(G2_ST + 0x80u));
    CHECK_EQ_INT((int)DSD(r1 + 0x24u), 0x3F800000);
    CHECK_EQ_INT((int)tb_active(), (int)(n + 1u));
    c = g2_find(G2_ST + 0x90u);
    CHECK(c != 0u, "0x380C4 spawned 0xBDDE0[4]");
    if (c != 0u) {
        CHECK_EQ_INT((int)DSB(c + 0x59u), 1);
        CHECK_EQ_INT((int)DSB(c + 0x4Au), 2);           /* r1+0x56 */
        CHECK_EQ_INT((int)(DSW(c + 0x28u) & 0x4400u), 0x4400);
    }
    CHECK_EQ_INT((int)DSB(par + 0x4Fu), 1);
    CHECK_EQ_INT((int)DSW(r0 + 0x34u), 0x80);
    CHECK_EQ_INT((int)DSB(DS_00104AE9), 0x81);
    CHECK_EQ_INT((int)DSW(DS_00108390), 0x3C);
    CHECK_EQ_INT((int)DSB(s0 + 0x57u), 2);
    CHECK_EQ_INT((int)DSD(r0 + 8u), 0x00ABCDEF);
    /* B3: the timer test is signed: 0x8001 -> 0x8000 fires; the passed
     * record's +0x51 = 1 picks side 1 for 0x3C190 (record 1's +0x34); a
     * clear +0x28 bit 14 leaves the child's clear. */
    g2f_seed();
    DSD(0x000BDDC4u + 16u) = g2_stream(8u, 0x1260u);
    DSD(0x000BDDE0u + 16u) = G2_DESC;
    mem_fill(G2_DESC, 0, 0x14u);
    DSD(G2_DESC) = g2_stream(9u, 0x1270u);
    mem_fill(G2_REC2, 0, ACTOR_REC_SIZE);
    DSB(G2_REC2 + 0x51u) = 1u;
    DSB(s0 + 0x57u) = 1u;
    DSW(DS_00108390) = 0x8001u;
    DSW(r0 + 0x34u) = 0x3434u;
    DSW(r1 + 0x34u) = 0x5656u;
    fighter_48aac(s0, G2_REC2, 0u);
    CHECK_EQ_INT((int)DSB(s0 + 0x57u), 2);
    CHECK_EQ_INT((int)DSW(r1 + 0x34u), 0x80);
    CHECK_EQ_INT((int)DSW(r0 + 0x34u), 0x3434);
    c = g2_find(G2_ST + 0x90u);
    if (c != 0u) CHECK_EQ_INT((int)(DSW(c + 0x28u) & 0x4000u), 0);

    /* C: +0x57 = 2: timer 2 -> 1 holds; 1 -> 0 fires. */
    g2f_seed();
    DSW(0x000EDD64u) = 0x1280u;
    DSB(s0 + 0x57u) = 2u;
    DSW(DS_00108390) = 2u;
    fighter_48aac(s0, r0, 0u);
    CHECK_EQ_INT((int)DSW(DS_00108390), 1);
    CHECK_EQ_INT((int)DSD(r0 + 8u), 0x00ABCDEF);
    CHECK_EQ_INT((int)DSB(DS_001078FC), 0x66);
    fighter_48aac(s0, r0, 0u);
    CHECK_EQ_INT((int)DSW(DS_00108390), 0);
    CHECK_EQ_INT((int)DSD(r0 + 8u), 0x000EDD64);
    CHECK_EQ_INT((int)DSD(r0 + 0x24u), 0x40000000);
    CHECK_EQ_INT((int)(DSW(p1) & 0x7FFFu), 0x1280);
    CHECK_EQ_INT((int)DSB(s1 + 0x52u), 0x0A);               /* 0x370F0(r1) */
    CHECK_EQ_INT((int)DSB(s1 + 0x54u), 3);
    CHECK_EQ_INT((int)DSB(r1 + 0x53u), 0);
    CHECK_EQ_INT((int)DSB(r0 + 0x53u), 0x35);
    CHECK_EQ_INT((int)DSB(DS_000F0AFE), 2);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 9);
    CHECK_EQ_INT((int)DSB(s0 + 0x53u), 3);
    CHECK_EQ_INT((int)DSB(s0 + 0x42u), 0x40);
    CHECK_EQ_INT((int)DSB(DS_001078FC), 1);
    CHECK_EQ_INT((int)DSB(s0 + 0x57u), 2);
    /* C2: above 2 returns without storing the tick. */
    g2f_seed();
    DSB(s0 + 0x57u) = 3u;
    DSW(DS_00108390) = 1u;
    fighter_48aac(s0, r0, 0u);
    CHECK_EQ_INT((int)DSW(DS_00108390), 1);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 0x55);
    /* C3: through 0x3531C case 7 (+0x53 = 7, +0x0C = 0x48AAC). */
    g2f_seed();
    DSD(s1 + 0x2Cu) = 5000u;
    DSB(s0 + 0x53u) = 7u;
    DSD(s0 + 0x0Cu) = 0x00048AACu;
    fighter_state_3531c(0u);
    CHECK_EQ_INT((int)DSB(s0 + 0x57u), 1);
    CHECK_EQ_INT((int)DSW(DS_00108390), 0xF0);

    /* D: 0x48D94 +0x57 = 0 and 5 return: no store, no draw. */
    g2f_seed();
    rng_seed(0xA1943569u);
    for (i = 0; i <= 5u; i += 5u) {
        DSB(s0 + 0x57u) = (u8)i;
        fighter_48d94(s0, r0, 0u);
        CHECK_EQ_INT((int)DSB(s0 + 0x57u), (int)i);
    }
    CHECK_EQ_INT((int)DSW(DS_00108390), 0x1234);
    CHECK_EQ_INT((int)DSD(DS_000EF6D8), (int)0xA1943569u);
    CHECK_EQ_INT((int)DSD(DS_000C951C), 0x51C51C51);
    /* D2: +0x57 = 1: 0x48C4C's lists and 2. The region 0x1082D0..0x10839F
     * holds the dword G2_JUNK (a scratch address, so a link the function
     * failed to write is followed harmlessly), the bytes 0x108396..0x108399
     * 0xA5. */
    for (i = 0; i < 0xD0u; i += 4u) DSD(0x001082D0u + i) = G2_JUNK;
    DSD(0x00108396u) = 0xA5A5A5A5u;
    DSB(s0 + 0x57u) = 1u;
    fighter_48d94(s0, r0, 0u);
    CHECK_EQ_INT((int)DSB(s0 + 0x57u), 2);
    CHECK_EQ_INT((int)DSD(DS_00108368), (int)DS_00108368);
    CHECK_EQ_INT((int)DSD(DS_0010836C), (int)DS_00108368);
    CHECK_EQ_INT((int)DSD(DS_001082E0), (int)DS_001082E8);
    CHECK_EQ_INT((int)DSD(DS_001082E4), 0x00108358);
    for (i = 0; i < 8u; i++) {
        u32 node = DS_001082E8 + i * 0x10u;
        CHECK_EQ_INT((int)DSD(node), (int)(i < 7u ? node + 0x10u : DS_001082E0));
        CHECK_EQ_INT((int)DSD(node + 4u), (int)(i > 0u ? node - 0x10u : DS_001082E0));
        CHECK_EQ_INT((int)DSD(node + 8u), (int)G2_JUNK);
    }
    CHECK_EQ_INT((int)DSB(0x00108396u), 0);
    CHECK_EQ_INT((int)DSB(0x00108397u), 0);
    CHECK_EQ_INT((int)DSB(0x00108398u), 0);
    CHECK_EQ_INT((int)DSB(0x00108399u), 0xA5);
    CHECK_EQ_INT((int)DSD(0x001082DCu), (int)G2_JUNK);
    CHECK_EQ_INT((int)DSD(0x00108370u), (int)G2_JUNK);

    /* E: +0x57 = 2 with the lists built: one 0xC950C spawn. Side 0 char 2's
     * 0xA8A98 row entry 1 (0x105B34[0] = 1) is G2_HANDLE, which palette
     * entry 0 holds; the expected draws are replayed from the same seed. */
    g2f_seed();
    actor_type_2d_list_init();
    DSD(0x000C950Cu) = g2_stream(10u, 0x12A0u);
    DSD(DS_000A8A98 + 8u) = G2_ROW;
    mem_fill(G2_ROW, 0, 0x20u);
    DSD(G2_ROW + 4u) = G2_HANDLE;
    DSB(DS_00105B34) = 1u;
    DSB(DS_00105B34 + 1u) = 0;              /* side 1 would read entry 0 (0) */
    DSD(DS_00107618) = G2_HANDLE;
    DSD(DS_00107618 + 4u) = 5u;
    DSD(DS_000F0AF0) = 0x3000u;
    DSW(r0 + 0x28u) = 0;
    DSW(r0 + 0x32u) = 0x2000u;
    DSW(r0 + 0x2Eu) = 0x0123u;
    rng_seed(0xA1943569u);
    q1 = rng_next(0x80u);
    q2 = rng_next(0x200u);
    st2 = DSD(DS_000EF6D8);
    q3 = rng_next(4u);
    st3 = DSD(DS_000EF6D8);
    rng_seed(0xA1943569u);
    DSB(s0 + 0x57u) = 2u;
    n = tb_active();
    fighter_48d94(s0, r0, 0u);
    CHECK_EQ_INT((int)DSD(DS_000EF6D8), (int)st3);
    CHECK_EQ_INT((int)DSD(DS_000C951C), (int)G2_HANDLE);
    CHECK_EQ_INT((int)tb_active(), (int)(n + 1u));
    c = g2_find(G2_ST + 0xA0u);
    CHECK(c != 0u, "0x48D94 spawned 0xC950C");
    if (c != 0u) {
        u32 ps = DSD(DS_001014EC) + (u32)DSW(c + 0x56u) * PSET_SIZE;
        CHECK_EQ_INT((int)DSW(c + 0x34u), (int)(u16)(0u - (q1 + 0x80u)));
        CHECK_EQ_INT((int)DSD(c + 0x18u), 0x3000 + 0x2A00);
        CHECK_EQ_INT((int)DSW(c + 0x32u), (int)(u16)(0x2000u - 0x200u + q2));
        CHECK_EQ_INT((int)DSD(c + 0x1Cu), 0);
        CHECK_EQ_INT((int)(DSW(c + 0x28u) & 0x4000u), 0);
        CHECK_EQ_INT((int)DSB(c + 0x48u), 0x2D);
        CHECK_EQ_INT((int)DSW(ps + 2u), 0x0923);                /* 0x2A17C */
        CHECK_EQ_INT((int)DSD(ps + 0x18u), (int)DS_00107618);
    }
    CHECK_EQ_INT((int)DSD(DS_00107618 + 4u), 6);
    CHECK_EQ_INT((int)DSB(0x00108398u), 1);
    CHECK_EQ_INT((int)DSB(DS_00108399), 2);
    CHECK_EQ_INT((int)DSW(DS_00108390), (int)(q3 + 4u));
    CHECK_EQ_INT((int)DSB(s0 + 0x57u), 3);
    /* E2: +0x28 bit 14 set: the speed is positive and x is 0x3000 - 0x2A00;
     * a count of 7 reaches 8: timer 0xB4, next state 4, no third draw. */
    DSW(r0 + 0x28u) = 0x4000u;
    DSB(0x00108398u) = 7u;
    DSB(s0 + 0x57u) = 2u;
    rng_seed(0xA1943569u);
    fighter_48d94(s0, r0, 0u);
    c = g2_find(G2_ST + 0xA0u);
    if (c != 0u) {
        CHECK_EQ_INT((int)DSW(c + 0x34u), (int)(q1 + 0x80u));
        CHECK_EQ_INT((int)DSD(c + 0x18u), 0x3000 - 0x2A00);
        CHECK_EQ_INT((int)(DSW(c + 0x28u) & 0x4000u), 0x4000);
    }
    CHECK_EQ_INT((int)DSB(0x00108398u), 8);
    CHECK_EQ_INT((int)DSW(DS_00108390), 0xB4);
    CHECK_EQ_INT((int)DSB(DS_00108399), 4);
    CHECK_EQ_INT((int)DSB(s0 + 0x57u), 3);
    CHECK_EQ_INT((int)DSD(DS_000EF6D8), (int)st2);
    /* E3: the count test is signed: 0x7F + 1 = 0x80 (-128) takes the rng(4)
     * arm. */
    DSW(r0 + 0x28u) = 0;
    DSB(0x00108398u) = 0x7Fu;
    DSB(s0 + 0x57u) = 2u;
    rng_seed(0xA1943569u);
    fighter_48d94(s0, r0, 0u);
    CHECK_EQ_INT((int)DSD(DS_000EF6D8), (int)st3);
    CHECK_EQ_INT((int)DSB(0x00108398u), 0x80);
    CHECK_EQ_INT((int)DSB(DS_00108399), 2);
    CHECK_EQ_INT((int)DSW(DS_00108390), (int)(q3 + 4u));
    /* E4: an empty free list refuses the spawn: +0x57, the timer and
     * 0x108399 stay; both draws and the 0xC951C store happened. */
    g2f_seed();
    DSD(DS_001082E0) = DS_001082E0;
    DSD(DS_001082E4) = DS_001082E0;
    DSD(0x000C950Cu) = g2_stream(10u, 0x12A0u);
    DSD(DS_000A8A98 + 8u) = G2_ROW;
    mem_fill(G2_ROW, 0, 0x20u);
    DSB(s0 + 0x57u) = 2u;
    rng_seed(0xA1943569u);
    fighter_48d94(s0, r0, 0u);
    CHECK_EQ_INT((int)DSB(s0 + 0x57u), 2);
    CHECK_EQ_INT((int)DSW(DS_00108390), 0x1234);
    CHECK_EQ_INT((int)DSB(DS_00108399), 0xA5);
    CHECK_EQ_INT((int)DSD(DS_000C951C), 0);
    CHECK_EQ_INT((int)DSD(DS_000EF6D8), (int)st2);

    /* F: +0x57 = 3: 2 -> 1 holds; 1 -> 0 moves to 0x108399; 0x8001 -> 0x8000
     * (signed) moves too. */
    g2f_seed();
    DSB(DS_00108399) = 4u;
    DSB(s0 + 0x57u) = 3u;
    DSW(DS_00108390) = 2u;
    fighter_48d94(s0, r0, 0u);
    CHECK_EQ_INT((int)DSB(s0 + 0x57u), 3);
    CHECK_EQ_INT((int)DSW(DS_00108390), 1);
    fighter_48d94(s0, r0, 0u);
    CHECK_EQ_INT((int)DSB(s0 + 0x57u), 4);
    CHECK_EQ_INT((int)DSW(DS_00108390), 0);
    DSB(s0 + 0x57u) = 3u;
    DSW(DS_00108390) = 0x8001u;
    DSB(DS_00108399) = 2u;
    fighter_48d94(s0, r0, 0u);
    CHECK_EQ_INT((int)DSB(s0 + 0x57u), 2);

    /* G: +0x57 = 4: rec on 0xED4FA at 3.0, 0x370F0(r1), 9/3, DS_001078FC. */
    g2f_seed();
    DSW(0x000ED4FAu) = 0x1290u;
    DSB(s0 + 0x57u) = 4u;
    fighter_48d94(s0, r0, 0u);
    CHECK_EQ_INT((int)DSD(r0 + 8u), 0x000ED4FA);
    CHECK_EQ_INT((int)DSD(r0 + 0x24u), 0x40400000);
    CHECK_EQ_INT((int)(DSW(p1) & 0x7FFFu), 0x1290);
    CHECK_EQ_INT((int)DSB(s1 + 0x52u), 0x0A);
    CHECK_EQ_INT((int)DSB(r1 + 0x53u), 0);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 9);
    CHECK_EQ_INT((int)DSB(s0 + 0x53u), 3);
    CHECK_EQ_INT((int)DSB(DS_001078FC), 1);
    CHECK_EQ_INT((int)DSB(s0 + 0x57u), 4);

    /* H: the whole machine through 0x3531C case 7 from +0x57 = 1: eight
     * type-0x2D spawns, then the 0xB4 wait and state 4 (which leaves case 7
     * with +0x53 = 3). */
    g2f_seed();
    DSD(0x000C950Cu) = g2_stream(10u, 0x12A0u);
    DSD(DS_000A8A98 + 8u) = G2_ROW;
    mem_fill(G2_ROW, 0, 0x20u);
    DSW(0x000ED4FAu) = 0x1290u;
    DSB(s0 + 0x53u) = 7u;
    DSD(s0 + 0x0Cu) = 0x00048D94u;
    DSB(s0 + 0x57u) = 1u;
    n = tb_active();
    for (i = 0; i < 1000u && DSB(s0 + 0x53u) == 7u; i++)
        fighter_state_3531c(0u);
    CHECK_EQ_INT((int)DSB(s0 + 0x53u), 3);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 9);
    CHECK_EQ_INT((int)DSB(0x00108398u), 8);
    CHECK_EQ_INT((int)tb_active(), (int)(n + 8u));
    CHECK_EQ_INT((int)DSD(r0 + 8u), 0x000ED4FA);
    CHECK_EQ_INT((int)DSD(DS_001082E0), (int)DS_001082E0);

    /* I: the finisher entries through their registrations, as 0x379C4 calls
     * DS_001078E8 (slot, rec; non-zero return). 0x48BE0: the record on
     * 0xEDD34 (first word patched to 0x12B0) at 2.0, 7/9/0, +0x0C = 0x48AAC,
     * +0x57/+0x18/+0x1C/+0x14 = 0, +0x42 |= 8, and 0x3C190(rec+0x51 = 1,
     * 0x80): record 1's +0x34 = -0x80 (unflipped), record 0's kept. */
    CHECK(fn_resolve(0x48BE0u) == (void (*)(void))fighter_48be0,
          "0x48BE0 is registered as fighter_48be0");
    CHECK(fn_resolve(0x48F54u) == (void (*)(void))fighter_48f54,
          "0x48F54 is registered as fighter_48f54");
    CHECK_EQ_INT((int)DSD(0x000BDAE4u + 8u), 0x00048BE0);
    CHECK_EQ_INT((int)DSD(0x000BDB00u + 8u), 0x00048F54);
    g2f_seed();
    DSW(0x000EDD34u) = 0x12B0u;
    DSW(0x000EDC82u) = 0x12C0u;
    mem_fill(G2_REC2, 0, ACTOR_REC_SIZE);
    DSB(G2_REC2 + 0x51u) = 1u;
    DSW(G2_REC2 + 0x56u) = 1u;
    DSB(s0 + 0x54u) = 0x44u;
    DSB(s0 + 0x57u) = 0x33u;
    DSB(s0 + 0x42u) = 0x21u;
    DSD(s0 + 0x0Cu) = 0x0C0C0C0Cu;
    DSD(s0 + 0x14u) = 0x14141414u;
    DSD(s0 + 0x18u) = 0x18181818u;
    DSD(s0 + 0x1Cu) = 0x1C1C1C1Cu;
    DSW(r0 + 0x34u) = 0x3434u;
    DSW(r1 + 0x34u) = 0x5656u;
    {
        typedef int (*entry_fn)(u32 slot, u32 rec);
        entry_fn e1 = (entry_fn)(void *)fn_resolve(0x48BE0u);
        entry_fn e2 = (entry_fn)(void *)fn_resolve(0x48F54u);
        CHECK(e1 != NULL && e1(s0, G2_REC2) != 0, "0x48BE0 returns non-zero");
        CHECK_EQ_INT((int)DSD(G2_REC2 + 8u), 0x000EDD34);
        CHECK_EQ_INT((int)DSD(G2_REC2 + 0x24u), 0x40000000);
        CHECK_EQ_INT((int)DSB(s0 + 0x53u), 7);
        CHECK_EQ_INT((int)DSB(s0 + 0x52u), 9);
        CHECK_EQ_INT((int)DSB(s0 + 0x54u), 0);
        CHECK_EQ_INT((int)DSD(s0 + 0x0Cu), 0x00048AAC);
        CHECK_EQ_INT((int)DSB(s0 + 0x57u), 0);
        CHECK_EQ_INT((int)DSD(s0 + 0x14u), 0);
        CHECK_EQ_INT((int)DSD(s0 + 0x18u), 0);
        CHECK_EQ_INT((int)DSD(s0 + 0x1Cu), 0);
        CHECK_EQ_INT((int)DSB(s0 + 0x42u), 0x29);
        CHECK_EQ_INT((int)DSW(r1 + 0x34u), 0xFF80);
        CHECK_EQ_INT((int)DSW(r0 + 0x34u), 0x3434);
        /* 0x48F54: 0xEDC82 at 2.0, 7/9/0, +0x0C = 0x48D94, the clears; no
         * +0x42 bit and no 0x3C190. */
        DSB(s0 + 0x52u) = 0x55u; DSB(s0 + 0x53u) = 0x66u;
        DSB(s0 + 0x54u) = 0x44u; DSB(s0 + 0x57u) = 0x33u;
        DSB(s0 + 0x42u) = 0x21u;
        DSD(s0 + 0x14u) = 0x14141414u;
        DSD(s0 + 0x18u) = 0x18181818u;
        DSD(s0 + 0x1Cu) = 0x1C1C1C1Cu;
        DSW(r1 + 0x34u) = 0x5656u;
        CHECK(e2 != NULL && e2(s0, G2_REC2) != 0, "0x48F54 returns non-zero");
        CHECK_EQ_INT((int)DSD(G2_REC2 + 8u), 0x000EDC82);
        CHECK_EQ_INT((int)DSD(G2_REC2 + 0x24u), 0x40000000);
        CHECK_EQ_INT((int)DSB(s0 + 0x53u), 7);
        CHECK_EQ_INT((int)DSB(s0 + 0x52u), 9);
        CHECK_EQ_INT((int)DSB(s0 + 0x54u), 0);
        CHECK_EQ_INT((int)DSD(s0 + 0x0Cu), 0x00048D94);
        CHECK_EQ_INT((int)DSB(s0 + 0x57u), 0);
        CHECK_EQ_INT((int)DSD(s0 + 0x14u), 0);
        CHECK_EQ_INT((int)DSD(s0 + 0x18u), 0);
        CHECK_EQ_INT((int)DSD(s0 + 0x1Cu), 0);
        CHECK_EQ_INT((int)DSB(s0 + 0x42u), 0x21);
        CHECK_EQ_INT((int)DSW(r1 + 0x34u), 0x5656);
    }

    g2_restore();
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
    /* Record §42-E: the stub is 3 bytes, `xor eax,eax; ret`, between
     * 0x5D808's `ret` (0x5D811) and the next function's `push ebx` (0x5D815).
     * The image's other holders are the four `mov edx,0x5d812` stores into
     * DS_00104AE4 (the no-handler value), fixed up from 0x4D812. */
    CHECK_EQ_INT((int)DSB(0x5D811u), 0xC3);
    CHECK_EQ_INT((int)DSB(0x5D812u), 0x33);
    CHECK_EQ_INT((int)DSB(0x5D813u), 0xC0);
    CHECK_EQ_INT((int)DSB(0x5D814u), 0xC3);
    CHECK_EQ_INT((int)DSB(0x5D815u), 0x53);
    {
        static const u32 imm[4] = { 0x25C05u, 0x26A28u, 0x27123u, 0x29627u };
        for (u32 i = 0; i < 4u; i++) {
            CHECK_EQ_INT((int)DSB(imm[i] - 1u), 0xBA);          /* mov edx */
            CHECK_EQ_INT((int)DSD(imm[i]), (int)FN_0005D812);
        }
    }

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

/* ---- the combo text (record §33) ---------------------------------------- */

/* The record in text-grid cell (row, col), and its mode-0 glyph sprite id. */
static u32 ct_cell(s32 row, s32 col)
{
    return DSD(DS_00105F38 + (u32)row * 0xacu + (u32)col * 4u);
}

static u32 ct_sprite(s32 row, s32 col)
{
    u32 r = ct_cell(row, col);
    return r != 0 ? (u32)(DSW(actor_pset(r)) & 0x7fffu) : 0u;
}

/* Plant a non-glyph record (sprite 0x2C11) in cell (row, col). */
static u32 ct_plant(s32 row, s32 col)
{
    u32 r = actor_spawn((const u32 *)(mem + 0x9AC30u), 0x4840u, 0xE0u,
                        0x1B00u, 0u);
    DSD(DS_00105F38 + (u32)row * 0xacu + (u32)col * 4u) = r;
    return r;
}

/* 0x38C5C/0x38D24/0x38D90/0x38ED0/0x38FEC and 0x39040's two calls. Glyph
 * sprite ids are the font table's at 0xBCD7C: '0'.. = 0x3F45.. ('2' 0x3F47,
 * '5' 0x3F4A, '6' 0x3F4B), '%' 0x3F3A, the 0x1B HIT glyph 0x3F30, 'A'.. = 0x3F56..
 * ('B' 0x3F57, 'C' 0x3F58, 'E' 0x3F5A, 'M' 0x3F62, 'O' 0x3F64, 'P' 0x3F65,
 * 'S' 0x3F68, 'T' 0x3F69). String 0xE5 is 60 spaces; the T-rex's combo records at
 * 0xBE024 (7) name 0xE6/0xE7 at 7 hits, 0xE8 "EXTRA CRUNCHY"/0xE9 " " at 6,
 * 0xEA/0xEB at 4 when 0x107A80[side][42] >= 1; record 1 (0xBE068) needs
 * [36] and names 0xEE "SUPER EAR"/0xEF "SPLITTER" at 6. Planted records
 * (sprite 0x2C11) are the sentinels for every cleared cell. Runs on the real
 * actor pool, after test_fight's restores. */
static void check_combo_text(void)
{
    u32 s0 = DS_001077B0, s1 = DS_001077B0 + 0x94u;
    u8 sv_slots[0x128], sv_d00[0x40], sv_a80[0x80];
    u32 sv_cur = DSD(DS_00105F34);
    u8 sv_8b6 = DSB(DS_001088B6), sv_8b7 = DSB(DS_001088B6 + 1u);
    u32 sv_rng = DSD(DS_000EF6D8), sv_frame = DSD(DS_000EF6DC);
    u32 sv_mode = DSD(DS_00104B00);
    tf_snap(sv_slots, s0, 0x128u);
    tf_snap(sv_d00, 0x00107D00u, 0x40u);
    tf_snap(sv_a80, DS_00107A80, 0x80u);

    /* --- A. 0x38D90 side 0, slot+0x63 clear: hits 2 at row 8, the HIT glyph
     * and COMBO down col 2 from row 9, " 75" and "%" on row 0x10. --- */
    actors_reset();
    DSB(s0 + 0x63u) = 0;
    DSW(0x00107D2Cu) = 2;
    DSW(DS_00107D20) = 75;
    ct_plant(8, 3);                 /* cleared by 0x38C5C's row-8 run
                                     * and again by the 0xE5 row-7 clear's wrap
                                     * (row 8 cols 0..15), so it does not
                                     * isolate 0x38C5C; part C does */
    ct_plant(0x10, 4);              /* row 0x10 runs: cols 0..5 */
    ct_plant(0x10, 5);
    ct_plant(6, 0);                 /* string 0xE5 clears rows 6/7 */
    ct_plant(7, 5);
    ct_plant(15, 2);                /* below COMBO: nothing reaches it */
    DSD(DS_00105F34) = 0x55555555u;
    fighter_38d90(0u);
    CHECK_EQ_INT((int)ct_sprite(8, 2), 0x3f47);
    CHECK_EQ_INT((int)ct_cell(8, 3), 0);
    CHECK_EQ_INT((int)ct_sprite(9, 2), 0x3f30);
    CHECK_EQ_INT((int)ct_sprite(10, 2), 0x3f58);
    CHECK_EQ_INT((int)ct_sprite(11, 2), 0x3f64);
    CHECK_EQ_INT((int)ct_sprite(12, 2), 0x3f62);
    CHECK_EQ_INT((int)ct_sprite(13, 2), 0x3f57);
    CHECK_EQ_INT((int)ct_sprite(14, 2), 0x3f64);
    CHECK_EQ_INT((int)ct_sprite(15, 2), 0x2c11);
    CHECK_EQ_INT((int)ct_cell(9, 3), 0);
    CHECK_EQ_INT((int)ct_cell(0x10, 0), 0);
    CHECK_EQ_INT((int)ct_sprite(0x10, 1), 0x3f4c);
    CHECK_EQ_INT((int)ct_sprite(0x10, 2), 0x3f4a);
    CHECK_EQ_INT((int)ct_sprite(0x10, 3), 0x3f3a);
    CHECK_EQ_INT((int)ct_cell(0x10, 4), 0);
    CHECK_EQ_INT((int)ct_cell(0x10, 5), 0);
    CHECK_EQ_INT((int)ct_cell(6, 0), 0);
    CHECK_EQ_INT((int)ct_cell(7, 5), 0);
    CHECK_EQ_INT((int)DSW(DS_00105F34), 9);         /* 0x2F20C's cursor */
    CHECK_EQ_INT((int)DSW(DS_00105F34 + 2u), 8);    /* col 2 + 6 glyphs */

    /* --- A2. slot+0x63 set: no row-0x10 draw; the planted cells are still
     * cleared by 0x38C5C. --- */
    actors_reset();
    DSB(s0 + 0x63u) = 1;
    ct_plant(0x10, 1);
    ct_plant(0x10, 3);
    fighter_38d90(0u);
    CHECK_EQ_INT((int)ct_sprite(8, 2), 0x3f47);
    CHECK_EQ_INT((int)ct_cell(0x10, 1), 0);
    CHECK_EQ_INT((int)ct_cell(0x10, 3), 0);

    /* --- B. side 1: col 0x25 + 2 = 39, its own hit count, +0x63 and 0x107D22.
     * Side 0's words are sentinels that would draw other digits. --- */
    actors_reset();
    DSB(s0 + 0x63u) = 1;
    DSB(s1 + 0x63u) = 0;
    DSW(0x00107D2Cu) = 7;
    DSW(0x00107D2Eu) = 3;
    DSW(DS_00107D20) = 2;
    DSW(DS_00107D20 + 2u) = 5;
    fighter_38d90(1u);
    CHECK_EQ_INT((int)ct_sprite(8, 39), 0x3f48);
    CHECK_EQ_INT((int)ct_sprite(9, 39), 0x3f30);
    CHECK_EQ_INT((int)ct_sprite(14, 39), 0x3f64);
    CHECK_EQ_INT((int)ct_cell(8, 2), 0);
    CHECK_EQ_INT((int)ct_cell(0x10, 37), 0);
    CHECK_EQ_INT((int)ct_cell(0x10, 38), 0);
    CHECK_EQ_INT((int)ct_sprite(0x10, 39), 0x3f4a);
    CHECK_EQ_INT((int)ct_sprite(0x10, 40), 0x3f3a);

    /* --- C. 0x38D24: the timer counts down; only the step reaching zero
     * clears (0x38C5C + the 0xE5 rows); zero stays zero. --- */
    actors_reset();
    DSW(DS_00107D18) = 3;
    DSW(DS_00107D18 + 2u) = 9;
    ct_plant(8, 2);
    ct_plant(6, 0);
    ct_plant(12, 2);                /* 0x2F314's six rows 9..14 */
    ct_plant(14, 2);
    ct_plant(15, 2);
    fighter_38d24(0u);
    CHECK_EQ_INT((int)DSW(DS_00107D18), 2);
    CHECK_EQ_INT((int)ct_sprite(8, 2), 0x2c11);
    fighter_38d24(0u);
    CHECK_EQ_INT((int)DSW(DS_00107D18), 1);
    CHECK_EQ_INT((int)DSW(DS_00107D18 + 2u), 9);
    CHECK_EQ_INT((int)ct_sprite(8, 2), 0x2c11);
    CHECK_EQ_INT((int)ct_sprite(12, 2), 0x2c11);
    fighter_38d24(0u);
    CHECK_EQ_INT((int)DSW(DS_00107D18), 0);
    CHECK_EQ_INT((int)ct_cell(8, 2), 0);
    CHECK_EQ_INT((int)ct_cell(6, 0), 0);
    CHECK_EQ_INT((int)ct_cell(12, 2), 0);
    CHECK_EQ_INT((int)ct_cell(14, 2), 0);
    CHECK_EQ_INT((int)ct_sprite(15, 2), 0x2c11);
    ct_plant(8, 2);
    fighter_38d24(0u);
    CHECK_EQ_INT((int)DSW(DS_00107D18), 0);
    CHECK_EQ_INT((int)ct_sprite(8, 2), 0x2c11);
    /* Side 1 counts its own word and clears its own column. The row-7 0xE5
     * clear (60 cells from col 0) runs on through col 0x2B, which is row 8's
     * col 0, and then row 8's cols 0..15 (0x2F280's wrap): (8, 15) goes,
     * (8, 16) stays. */
    DSW(DS_00107D18 + 2u) = 1;
    ct_plant(8, 38);                /* side 1's runs: row 8 cols 39/40 */
    ct_plant(8, 39);
    ct_plant(8, 40);
    ct_plant(0x10, 42);             /* row 0x10 cols 37..42 */
    ct_plant(8, 15);
    ct_plant(8, 16);
    fighter_38d24(1u);
    CHECK_EQ_INT((int)DSW(DS_00107D18 + 2u), 0);
    CHECK_EQ_INT((int)ct_sprite(8, 38), 0x2c11);
    CHECK_EQ_INT((int)ct_cell(8, 39), 0);
    CHECK_EQ_INT((int)ct_cell(8, 40), 0);
    CHECK_EQ_INT((int)ct_cell(0x10, 42), 0);
    CHECK_EQ_INT((int)ct_cell(8, 15), 0);
    CHECK_EQ_INT((int)ct_sprite(8, 16), 0x2c11);

    /* --- D. 0x38ED0/0x38FEC: the T-rex's records. --- */
    mem_fill(DS_00107A80, 0, 0x80u);
    DSB(s0 + 0x7Au) = 0;
    DSB(s0 + 0x63u) = 0;
    /* D1. 6 hits and [42] = 1: record 0's second threshold names row 6
     * "EXTRA CRUNCHY" (col (0x2b - 13) >> 1 = 15) and row 7 " " (a clear).
     * The (7, 21) plant is cleared by the 0xE5 row-7 clear before the " "
     * draw, so it shows only that row 7 ends empty, not the " " itself. */
    actors_reset();
    DSW(0x00107D2Cu) = 6;
    DSB(DS_00107A80 + 42u) = 1;
    ct_plant(7, 21);
    CHECK_EQ_INT((int)fighter_38ed0(0u, 0x000BE024u), 1);
    CHECK_EQ_INT((int)ct_sprite(6, 15), 0x3f5a);
    CHECK_EQ_INT((int)ct_cell(7, 21), 0);
    /* D2. 7 hits: the first threshold, 0xE6 "TAKE A BITE" at col 16. */
    actors_reset();
    DSW(0x00107D2Cu) = 7;
    CHECK_EQ_INT((int)fighter_38ed0(0u, 0x000BE024u), 1);
    CHECK_EQ_INT((int)ct_sprite(6, 16), 0x3f69);
    /* D3. 3 hits: no threshold, 0 and nothing drawn. */
    actors_reset();
    DSW(0x00107D2Cu) = 3;
    ct_plant(6, 0);
    CHECK_EQ_INT((int)fighter_38ed0(0u, 0x000BE024u), 0);
    CHECK_EQ_INT((int)ct_sprite(6, 0), 0x2c11);
    /* D4. [42] below the need: 0 at the list, before any threshold. */
    DSW(0x00107D2Cu) = 7;
    DSB(DS_00107A80 + 42u) = 0;
    CHECK_EQ_INT((int)fighter_38ed0(0u, 0x000BE024u), 0);
    CHECK_EQ_INT((int)ct_sprite(6, 0), 0x2c11);
    /* D5. slot+0x63 set: 1 but no draw. */
    DSB(DS_00107A80 + 42u) = 1;
    DSB(s0 + 0x63u) = 1;
    CHECK_EQ_INT((int)fighter_38ed0(0u, 0x000BE024u), 1);
    CHECK_EQ_INT((int)ct_sprite(6, 0), 0x2c11);
    DSB(s0 + 0x63u) = 0;
    /* D6. 0x38FEC: record 0 fails its [42] need, record 1 ([36]) names 0xEE
     * "SUPER EAR" (col 17) and 0xEF "SPLITTER" (col 17). */
    actors_reset();
    DSW(0x00107D2Cu) = 6;
    DSB(DS_00107A80 + 42u) = 0;
    DSB(DS_00107A80 + 36u) = 1;
    fighter_38fec(0u);
    CHECK_EQ_INT((int)ct_sprite(6, 17), 0x3f68);
    CHECK_EQ_INT((int)ct_sprite(7, 17), 0x3f68);
    CHECK_EQ_INT((int)ct_sprite(7, 18), 0x3f65);     /* 'P' */
    /* D7. Side 1 reads its own 0x107A80 half, hit count, +0x63 and char;
     * side 0's are set to name a combo and must not. */
    actors_reset();
    DSB(DS_00107A80 + 36u) = 0;
    DSB(DS_00107A80 + 42u) = 1;
    DSW(0x00107D2Cu) = 7;
    DSB(s1 + 0x7Au) = 0;
    DSB(s1 + 0x63u) = 0;
    DSW(0x00107D2Eu) = 6;
    ct_plant(6, 16);
    CHECK_EQ_INT((int)fighter_38ed0(1u, 0x000BE024u), 0);
    CHECK_EQ_INT((int)ct_sprite(6, 16), 0x2c11);
    DSB(DS_00107A80 + 0x40u + 42u) = 1;
    CHECK_EQ_INT((int)fighter_38ed0(1u, 0x000BE024u), 1);
    CHECK_EQ_INT((int)ct_sprite(6, 15), 0x3f5a);   /* 6 hits: EXTRA CRUNCHY */
    DSB(s1 + 0x63u) = 1;
    ct_plant(6, 0);
    CHECK_EQ_INT((int)fighter_38ed0(1u, 0x000BE024u), 1);
    CHECK_EQ_INT((int)ct_sprite(6, 0), 0x2c11);
    /* D8. 0x38FEC by char: side 0 as char 1 walks 0xBEB90[1]'s records,
     * which do not use id 42, so the T-rex names are not drawn. */
    actors_reset();
    DSB(s0 + 0x7Au) = 1;
    mem_fill(DS_00107A80, 0, 0x40u);
    DSB(DS_00107A80 + 42u) = 1;
    DSW(0x00107D2Cu) = 7;
    ct_plant(6, 16);
    fighter_38fec(0u);
    CHECK_EQ_INT((int)ct_sprite(6, 16), 0x2c11);
    DSB(s0 + 0x7Au) = 0;
    /* D9. The walk stops at the first record that names: with [42] and [36]
     * both met, record 0 draws "EXTRA CRUNCHY" (col 15..27) and " " on row 7;
     * record 1's "SUPER EAR"/"SPLITTER" (col 17) must not follow. The row-6
     * 'T' is the discriminating check; the (7, 17) plant goes with the 0xE5
     * row-7 clear either way and stays empty only while "SPLITTER" is not
     * drawn. D6 covers record 1's names. */
    actors_reset();
    DSB(DS_00107A80 + 36u) = 1;
    DSW(0x00107D2Cu) = 6;
    ct_plant(7, 17);
    fighter_38fec(0u);
    CHECK_EQ_INT((int)ct_sprite(6, 17), 0x3f69);     /* 'T' of EXTRA */
    CHECK_EQ_INT((int)ct_cell(7, 17), 0);
    DSB(DS_00107A80 + 36u) = 0;

    /* --- E. 0x39040 draws through 0x38D90 and 0x38FEC and sets the 0xB4
     * timer; its tail zeroes the hit count. Mode 3 keeps 0x41310 out, and
     * 0x107D20 = 0x10 takes the rng arm (the rng words are restored). --- */
    actors_reset();
    mem_fill(DS_00107A80, 0, 0x80u);
    DSB(DS_00107A80 + 42u) = 1;
    DSB(s0 + 0x63u) = 0;
    DSD(s0 + 0x3Cu) = 0;
    DSW(DS_00104B00) = 3;
    DSW(0x00107D2Cu) = 6;
    DSW(DS_00107D20) = 0x10;
    DSW(DS_00107D18) = 0x1234u;
    fighter_39040(0u);
    CHECK_EQ_INT((int)ct_sprite(9, 2), 0x3f30);
    CHECK_EQ_INT((int)ct_sprite(0x10, 1), 0x3f46);     /* " 16" */
    CHECK_EQ_INT((int)ct_sprite(0x10, 2), 0x3f4b);
    CHECK_EQ_INT((int)ct_sprite(6, 15), 0x3f5a);       /* 0x38FEC: EXTRA CRUNCHY */
    /* 0x38ED0's row-7 0xE5 clear wraps over row 8's cols 0..15, so the '6'
     * that 0x38D90 put at (8, 2) is gone again. */
    CHECK_EQ_INT((int)ct_cell(8, 2), 0);
    CHECK_EQ_INT((int)DSW(DS_00107D18), 0xB4);
    CHECK_EQ_INT((int)DSW(0x00107D2Cu), 0);

    actors_reset();
    tf_put(sv_slots, s0, 0x128u);
    tf_put(sv_d00, 0x00107D00u, 0x40u);
    tf_put(sv_a80, DS_00107A80, 0x80u);
    DSD(DS_00105F34) = sv_cur;
    DSB(DS_001088B6) = sv_8b6;
    DSB(DS_001088B6 + 1u) = sv_8b7;
    DSD(DS_000EF6D8) = sv_rng;
    DSD(DS_000EF6DC) = sv_frame;
    DSD(DS_00104B00) = sv_mode;
}

/* ---- record §43-A: the mode-0x22/0x24 pass 0x4D2D0, 0x4987C, the volleyball */

/* The §43-A checks spawn pool actors (0x2AE14), kill records (0x2B150), draw
 * glyph text and patch tables and streams, so each saves and restores the
 * whole data object, the actor record pool, the pset pool the process runs
 * on, the FIGHT_* scratch and mem[0..0x3F] (a whole-memory diff around the
 * three checks showed pset 0's +0x18 written there, as §42-D found for
 * 0x2B150 on out-of-pool scratch records; the checks also plant sentinels
 * in that range). */
#define MZ_DATA_LO   0x00080000u
#define MZ_DATA_LEN  (0x0010B0D0u - 0x00080000u)
#define MZ_POOL_LEN  (ACTOR_POOL_RECORDS * ACTOR_REC_SIZE)
#define MZ_PSET_LEN  (ACTOR_POOL_RECORDS * PSET_SIZE)
#define MZ_FIGHT_LEN (FIGHT_RECS + 0x4000u - FIGHT_ACTORS)
static u8 *mz_buf;
static u8 mz_low[0x40];
static u32 mz_pool, mz_pset;

static int mz_save(void)
{
    mz_pool = DSD(DS_001014F4);
    mz_pset = DSD(DS_001014EC);
    mz_buf = (u8 *)malloc(MZ_DATA_LEN + MZ_POOL_LEN + MZ_PSET_LEN + MZ_FIGHT_LEN);
    if (mz_buf == NULL) return 0;
    tf_snap(mz_buf, MZ_DATA_LO, MZ_DATA_LEN);
    tf_snap(mz_buf + MZ_DATA_LEN, mz_pool, MZ_POOL_LEN);
    tf_snap(mz_buf + MZ_DATA_LEN + MZ_POOL_LEN, mz_pset, MZ_PSET_LEN);
    tf_snap(mz_buf + MZ_DATA_LEN + MZ_POOL_LEN + MZ_PSET_LEN, FIGHT_ACTORS,
            MZ_FIGHT_LEN);
    tf_snap(mz_low, 0u, sizeof mz_low);
    return 1;
}

static void mz_restore(void)
{
    if (mz_buf == NULL) return;
    tf_put(mz_buf, MZ_DATA_LO, MZ_DATA_LEN);
    tf_put(mz_buf + MZ_DATA_LEN, mz_pool, MZ_POOL_LEN);
    tf_put(mz_buf + MZ_DATA_LEN + MZ_POOL_LEN, mz_pset, MZ_PSET_LEN);
    tf_put(mz_buf + MZ_DATA_LEN + MZ_POOL_LEN + MZ_PSET_LEN, FIGHT_ACTORS,
           MZ_FIGHT_LEN);
    tf_put(mz_low, 0u, sizeof mz_low);
    free(mz_buf);
    mz_buf = NULL;
}

/* The fixture (mz_free_list_high first keeps the pool records 0..8 out of
 * every spawn): check_point_trample's (ph_seed, side 0's box, the point x/y
 * below on side 0), one entry E (si 3, side byte 1, slot 1) on the active
 * list with its actor R (pset 5, +0x14 = E), the shadow SH, an empty free
 * list, mode 0x24, DS_00104B1A = 0, DS_00104AC4 = 2 (no stop), DS_001088C2 =
 * 1 (the pass clears it), and the eleven stream tables' entry 3 pointed at
 * distinct scratch streams (a plain sprite word each). 0xC9754[3] is 0 so
 * 0x4BD4C declines unless a check sets it. */
#define MZ_E   (FIGHT_RECS + 0x3000u)
#define MZ_E2  (FIGHT_RECS + 0x3040u)
#define MZ_F1  (FIGHT_RECS + 0x3080u)
#define MZ_F2  (FIGHT_RECS + 0x30C0u)
#define MZ_R   (FIGHT_RECS + 0x3100u)
#define MZ_SH  (FIGHT_RECS + 0x3200u)
#define MZ_R2  (FIGHT_RECS + 0x3300u)
#define MZ_B1  (FIGHT_RECS + 0x3400u)
#define MZ_B2  (FIGHT_RECS + 0x3480u)
#define MZ_ST(k) (FIGHT_RECS + 0x3800u + (u32)(k) * 0x10u)
#define MZ_S0  DS_001077B0
#define MZ_S1  (DS_001077B0 + 0x94u)
#define MZ_PS(i) (FIGHT_ACTORS + (u32)(i) * 0x20u)
/* 0xC9544 0, 0xC95BC 1, 0xC95D4 2, 0xC95EC 3, 0xC9634 4, 0xC9724 5,
 * 0xC973C 6, 0xC9754 7, 0xC955C 8, 0xC958C 9, 0xC9604 10. */
static const u32 mz_tabs[11] = {
    0x000C9544u, 0x000C95BCu, 0x000C95D4u, 0x000C95ECu, 0x000C9634u,
    0x000C9724u, 0x000C973Cu, 0x000C9754u, 0x000C955Cu, 0x000C958Cu,
    0x000C9604u,
};

/* Take the pool records 0..8 off the actor free list DS_00105B3C (0x249D0
 * on the free ones only): a record's pset is its index, and the fixture's
 * psets 1..8 (FIGHT_ACTORS) hold the fighters, the projectiles, R, R2/B1,
 * SH and B2, so a spawn handed one of them would overwrite a fixture
 * position. Which records are free depends on the tests run before (main's
 * check_char3_2425 leaves index 2 at the head); mz_restore puts the list
 * back. */
static void mz_free_list_high(void)
{
    u32 pool = DSD(DS_001014F4);
    u32 r = DSD(DS_00105B3C);
    while (r != DS_00105B3C && r != 0u) {
        u32 next = DSD(r);
        if (r >= pool && r < pool + 9u * ACTOR_REC_SIZE)
            effects_list_unlink(r);
        r = next;
    }
}

static void mz_seed(u8 type, u8 c1c)
{
    u32 k;
    ph_seed(FIGHT_RECS + 0x200u, FIGHT_RECS + 0x300u, 0);
    mz_free_list_high();
    mem_fill(FIGHT_RECS + 0x3000u, 0, 0x500u);
    mem_fill(MZ_PS(5), 0, 0x80u);
    DSW(MZ_SH + 0x56u) = 7;
    DSD(DS_0010884C) = MZ_E; DSD(DS_0010884C + 4u) = MZ_E;
    DSD(MZ_E) = DS_0010884C; DSD(MZ_E + 4u) = DS_0010884C;
    DSD(DS_001083C4) = DS_001083C4; DSD(DS_001083C4 + 4u) = DS_001083C4;
    DSD(MZ_E + 8u) = MZ_R; DSD(MZ_E + 0xCu) = MZ_S1;
    DSB(MZ_E + 0x21u) = 1; DSB(MZ_E + 0x1Eu) = type; DSB(MZ_E + 0x1Cu) = c1c;
    DSW(MZ_E + 0x18u) = 50; DSW(MZ_E + 0x1Au) = 0x0500u;
    DSB(MZ_E + 0x1Fu) = 3; DSB(MZ_E + 0x20u) = 0x55u;
    DSD(MZ_E + 0x14u) = 0x7777u;
    DSB(MZ_R + 0x48u) = 0x23u; DSW(MZ_R + 0x56u) = 5; DSD(MZ_R + 0x14u) = MZ_E;
    DSD(MZ_R + 0x08u) = 0x0BADu; DSW(MZ_R + 0x28u) = 0x0002u;
    DSB(MZ_R + 0x2Au) = 0x0Cu;
    DSD(MZ_R + 0x18u) = 0x1000u; DSD(MZ_R + 0x1Cu) = 0x100u;
    DSD(MZ_R + 0x30u) = 0x06000000u;        /* the y word +0x32 = 0x600 */
    DSW(MZ_R + 0x34u) = 0x1111u; DSW(MZ_R + 0x36u) = 0x2222u;
    DSW(MZ_R + 0x38u) = 0x3333u; DSB(MZ_R + 0x29u) = 0x40u;
    DSW(MZ_R + 0x2Cu) = 0x5555u; DSB(MZ_R + 0x55u) = 0x77u;
    DSW(DS_00104B00) = 0x24u; DSB(0x00104B1Au) = 0; DSD(DS_00104AC4) = 2u;
    DSB(DS_001088C2) = 1u;
    DSB(DS_001088B2) = 0; DSB(DS_001088B2 + 1u) = 0;
    DSB(DS_0010889E) = 0; DSB(DS_0010889E + 1u) = 0;
    for (k = 0; k < 11u; k++) {
        DSW(MZ_ST(k)) = (u16)(0x0301u + k);
        DSD(mz_tabs[k] + 12u) = MZ_ST(k);
    }
    DSD(0x000C9754u + 12u) = 0;
    DSB(0x00000029u) = 0x0Fu;       /* what a DSD(entry - 8) slip would read */
    DSB(0x00000028u) = 0;           /* 0x2B150 on record 0 sets bit 3 here */
    DSW(0x00000032u) = 0x5A5Au;     /* a shadow 0 followed would write here */
    DSB(0x0000001Eu) = 0x77u;       /* 0x4AC38 on entry 0 writes here */
    rng_seed(0x100u);
}

/* The LCG state after `n` draws from `seed` (0x5D7DC; the range does not
 * change the state). */
static u32 mz_rng_after(u32 seed, u32 n)
{
    u32 s = seed;
    while (n--) s = s * 0xB90D12B9u + 0x38CE051Fu;
    return s;
}

/* Put a second, inert (type 8) entry E2 after E on the active list. */
static void mz_second(void)
{
    DSD(MZ_E) = MZ_E2; DSD(MZ_E2 + 4u) = MZ_E;
    DSD(MZ_E2) = DS_0010884C; DSD(DS_0010884C + 4u) = MZ_E2;
    DSD(MZ_E2 + 8u) = MZ_R2; DSD(MZ_E2 + 0xCu) = MZ_S1;
    DSB(MZ_E2 + 0x1Eu) = 8; DSB(MZ_E2 + 0x1Cu) = 0x40u;
    DSB(MZ_R2 + 0x48u) = 0x23u; DSW(MZ_R2 + 0x56u) = 6;
}

/* Put F1 (and, with `two`, F2 after it) on the free list DS_001083C4. */
static void mz_free(int two)
{
    DSD(DS_001083C4) = MZ_F1; DSD(MZ_F1 + 4u) = DS_001083C4;
    if (two) {
        DSD(MZ_F1) = MZ_F2; DSD(MZ_F2 + 4u) = MZ_F1;
        DSD(MZ_F2) = DS_001083C4; DSD(DS_001083C4 + 4u) = MZ_F2;
    } else {
        DSD(MZ_F1) = DS_001083C4; DSD(DS_001083C4 + 4u) = MZ_F1;
    }
    DSB(MZ_F1 + 0x1Fu) = 0x33u; DSD(MZ_F1 + 0x10u) = 0x4444u;
    DSB(MZ_F1 + 0x1Eu) = 0x66u; DSW(MZ_F1 + 0x1Cu) = 0x8888u;
    DSB(MZ_F2 + 0x1Fu) = 0x33u; DSD(MZ_F2 + 0x10u) = 0x4444u;
    DSB(MZ_F2 + 0x1Eu) = 0x66u; DSW(MZ_F2 + 0x1Cu) = 0x8888u;
}

/* 0x4D2D0 and its handlers 0x4D224/0x4D108/0x4D150 (record §43-A). The rng
 * seeds were chosen for the draws they give (scratchpad seeds.py/seeds2.py),
 * each also unlike the draw of a range one larger: 0x10A -> rng(2) 1, then
 * rng(0x3C) 28; 0x103 -> rng(2) 0 (rng(3) 1), then rng(0x3C) 26 or rng(2)
 * 0; 0x115 -> rng(2) 0, 1, 1, rng(0x1E) 16; 0x10B -> 0, 1, 0, rng(0x1E) 2;
 * 0x100 -> rng(2) 0, 1 (and rng(0x3C) 16, 34); 0x69D -> rng(0x3C) 0;
 * 0x11865 -> rng(0x3C) 31, 0, rng(2) 1, rng(4) 2; 0x53A -> 24, 0, 0,
 * rng(4) 3. */
static void check_mode22_pass(void)
{
    u32 bd, a, i;
    if (!mz_save()) { CHECK(0, "the §43-A snapshot allocates"); return; }
    bd = DSW(DS_000BD898);

    /* A: an empty list: only DS_001088C2 clears. */
    mz_seed(4, 0x80u);
    DSD(DS_0010884C) = DS_0010884C; DSD(DS_0010884C + 4u) = DS_0010884C;
    fight_4d2d0();
    CHECK_EQ_INT((int)DSB(DS_001088C2), 0);
    CHECK_EQ_INT((int)DSB(MZ_E + 0x1Eu), 4);
    CHECK_EQ_INT((int)DSW(MZ_E + 0x18u), 50);

    /* B: the stop while DS_00104AC4 <= 1 (signed): +0x38/+0x34/+0x36 zeroed,
     * +0x1C bit 2, 0x4AC38 for R's +0x14 entry (the 0xC9544[3] stream at
     * 5.0, +0x29 0x40 -> 0x10), then type 8; case 4 does not run. */
    for (i = 0; i < 2u; i++) {
        mz_seed(4, 0x80u);
        DSD(DS_00104AC4) = (i == 0u) ? 1u : 0xFFFFFFFFu;
        fight_4d2d0();
        CHECK_EQ_INT((int)DSW(MZ_R + 0x38u), 0);
        CHECK_EQ_INT((int)DSW(MZ_R + 0x34u), 0);
        CHECK_EQ_INT((int)DSW(MZ_R + 0x36u), 0);
        CHECK_EQ_INT((int)DSB(MZ_E + 0x1Cu), 0x84);
        CHECK_EQ_INT((int)DSB(MZ_E + 0x1Eu), 8);
        CHECK_EQ_INT((int)DSD(MZ_R + 0x08u), (int)MZ_ST(0));
        CHECK_EQ_INT((int)DSD(MZ_R + 0x24u), 0x40A00000);
        CHECK_EQ_INT((int)DSB(MZ_R + 0x29u), 0x10);
        CHECK_EQ_INT((int)DSW(MZ_E + 0x18u), 50);
        CHECK_EQ_INT((int)DSB(DS_001088C2), 0);
    }
    /* No stop: DS_00104AC4 = 2, +0x1C bit 2 or bit 6, or type 6 (case 4
     * counts; case 6 steps +0x36 = 0x2222 - 0x10). R's +0x14 = 0: the stop
     * without 0x4AC38. */
    {
        static const u32 ac4[3] = { 2u, 1u, 1u };
        static const u8 c1c[3] = { 0x80u, 0x84u, 0xC0u };
        for (i = 0; i < 3u; i++) {
            mz_seed(4, c1c[i]);
            DSD(DS_00104AC4) = ac4[i];
            fight_4d2d0();
            CHECK_EQ_INT((int)DSB(MZ_E + 0x1Eu), 4);
            CHECK_EQ_INT((int)DSW(MZ_E + 0x18u), 49);
            CHECK_EQ_INT((int)DSW(MZ_R + 0x38u), 0x3333);
        }
    }
    mz_seed(6, 0x00u);
    DSD(DS_00104AC4) = 1u;
    fight_4d2d0();
    CHECK_EQ_INT((int)DSB(MZ_E + 0x1Eu), 6);
    CHECK_EQ_INT((int)DSW(MZ_R + 0x36u), 0x2212);
    CHECK_EQ_INT((int)DSW(MZ_R + 0x38u), 0x3333);
    /* Without an owner the stop's own zeroing shows (0x4AC38 zeroes the
     * same words), and no 0x4AC38 runs (entry 0's type byte, mem[0x1E],
     * keeps its sentinel; mz_restore puts mem[0..0x1F] back). +0x1C bit 0
     * does not gate the stop. */
    mz_seed(4, 0x81u);
    DSD(DS_00104AC4) = 1u;
    DSD(MZ_R + 0x14u) = 0;
    DSB(0x0000001Eu) = 0x77u;
    fight_4d2d0();
    CHECK_EQ_INT((int)DSB(MZ_E + 0x1Eu), 8);
    CHECK_EQ_INT((int)DSB(MZ_E + 0x1Cu), 0x85);
    CHECK_EQ_INT((int)DSD(MZ_R + 0x08u), 0x0BAD);
    CHECK_EQ_INT((int)DSB(MZ_R + 0x29u), 0x40);
    CHECK_EQ_INT((int)DSW(MZ_R + 0x38u), 0);
    CHECK_EQ_INT((int)DSW(MZ_R + 0x34u), 0);
    CHECK_EQ_INT((int)DSW(MZ_R + 0x36u), 0);
    CHECK_EQ_INT((int)DSB(0x0000001Eu), 0x77);

    /* C: type 8 does nothing. */
    mz_seed(8, 0x80u);
    fight_4d2d0();
    CHECK_EQ_INT((int)DSB(MZ_E + 0x1Eu), 8);
    CHECK_EQ_INT((int)DSW(MZ_E + 0x18u), 50);
    CHECK_EQ_INT((int)DSD(MZ_R + 0x08u), 0x0BAD);
    CHECK_EQ_INT((int)DSD(DS_000EF6D8), 0x100);

    /* D: type 2 counts +0x18 down (signed); at zero 0x4AC38. */
    mz_seed(2, 0x80u);
    DSW(MZ_E + 0x18u) = 2;
    fight_4d2d0();
    CHECK_EQ_INT((int)DSW(MZ_E + 0x18u), 1);
    CHECK_EQ_INT((int)DSB(MZ_E + 0x1Eu), 2);
    fight_4d2d0();
    CHECK_EQ_INT((int)DSW(MZ_E + 0x18u), 0);
    CHECK_EQ_INT((int)DSB(MZ_E + 0x1Eu), 0);
    CHECK_EQ_INT((int)DSD(MZ_R + 0x08u), (int)MZ_ST(0));
    mz_seed(2, 0x80u);
    DSW(MZ_E + 0x18u) = 0x8001u;
    fight_4d2d0();
    CHECK_EQ_INT((int)DSB(MZ_E + 0x1Eu), 0);

    /* E: type 1, the walk's arrival: |x - target| <= |step| (signed; the
     * step is the +0x34 word, negated when negative). */
    {
        static const u32 tgt[5] = { 0x1040u, 0x1041u, 0x0FC0u, 0x1040u, 0x1041u };
        static const u16 v34[5] = { 0x0040u, 0x0040u, 0x0040u, 0xFFC0u, 0xFFC0u };
        static const int arrive[5] = { 1, 0, 1, 1, 0 };
        for (i = 0; i < 5u; i++) {
            mz_seed(1, 0x80u);
            DSD(MZ_E + 0x14u) = tgt[i];
            DSW(MZ_R + 0x34u) = v34[i];
            fight_4d2d0();
            CHECK_EQ_INT((int)DSB(MZ_E + 0x1Eu), arrive[i] ? 0 : 1);
            CHECK_EQ_INT((int)DSD(MZ_R + 0x08u), arrive[i] ? (int)MZ_ST(0) : 0x0BAD);
        }
    }
    /* 0x4BD4C with DS_001088C2 set takes it (type 8, 0xC9754[3] at 4.0);
     * without DS_001088C2 the arrival runs instead. */
    mz_seed(1, 0x80u);
    DSD(0x000C9754u + 12u) = MZ_ST(7);
    fight_4d2d0();
    CHECK_EQ_INT((int)DSB(MZ_E + 0x1Eu), 8);
    CHECK_EQ_INT((int)DSD(MZ_R + 0x08u), (int)MZ_ST(7));
    CHECK_EQ_INT((int)DSD(MZ_R + 0x24u), 0x40800000);
    mz_seed(1, 0x80u);
    DSD(0x000C9754u + 12u) = MZ_ST(7);
    DSB(DS_001088C2) = 0;
    DSD(MZ_E + 0x14u) = 0x1000u;
    fight_4d2d0();
    CHECK_EQ_INT((int)DSB(MZ_E + 0x1Eu), 0);
    CHECK_EQ_INT((int)DSD(MZ_R + 0x08u), (int)MZ_ST(0));

    /* F: type 3, the fall. +0x2C follows the y (0x496AC); above the layer
     * word it keeps falling; at it, it lands: +0x28 |= 0x4000 when 0x2BE1C
     * (R's pset x less side 1's) is positive, then by rng(2) 0xC9634[3] at
     * 2.0 or 0xC9724[3] at 5.0, +0x38/+0x34 zeroed, the timer rng(0x3C) +
     * 0x3C, type 4 and +0x1C bit 7. */
    mz_seed(3, 0x05u);
    DSD(MZ_R + 0x30u) = (bd + 1u) << 16;
    fight_4d2d0();
    CHECK_EQ_INT((int)DSB(MZ_E + 0x1Eu), 3);
    CHECK_EQ_INT((int)DSW(MZ_R + 0x2Cu), (int)(((0xB00u - (bd + 1u)) >> 1) + 0xC00u));
    CHECK_EQ_INT((int)DSD(DS_000EF6D8), 0x100);
    mz_seed(3, 0x05u);
    DSD(MZ_R + 0x30u) = bd << 16;
    DSB(MZ_R + 0x29u) = 0;                  /* +0x28 = 0x0002 */
    DSD(MZ_PS(5) + 4u) = 0x100u;
    rng_seed(0x10Au);
    fight_4d2d0();
    CHECK_EQ_INT((int)DSB(MZ_E + 0x1Eu), 4);
    CHECK_EQ_INT((int)DSD(MZ_R + 0x08u), (int)MZ_ST(4));
    CHECK_EQ_INT((int)DSD(MZ_R + 0x24u), 0x40000000);
    CHECK_EQ_INT((int)DSW(MZ_R + 0x28u), 0x4002);
    CHECK_EQ_INT((int)DSW(MZ_R + 0x38u), 0);
    CHECK_EQ_INT((int)DSW(MZ_R + 0x34u), 0);
    CHECK_EQ_INT((int)DSW(MZ_R + 0x36u), 0x2222);
    CHECK_EQ_INT((int)DSW(MZ_E + 0x18u), 0x3C + 28);
    CHECK_EQ_INT((int)DSB(MZ_E + 0x1Cu), 0x85);
    CHECK_EQ_INT((int)DSD(DS_000EF6D8), (int)mz_rng_after(0x10Au, 2u));
    /* R's x 0x100 left of the slot's record 0x200 (the slot's, not another
     * record's): 0x2BE1C < 0, no flip; the other stream. */
    mz_seed(3, 0x05u);
    DSD(MZ_R + 0x30u) = bd << 16;
    DSB(MZ_R + 0x29u) = 0;
    DSD(MZ_PS(5) + 4u) = 0x100u;
    DSD(MZ_PS(2) + 4u) = 0x200u;
    rng_seed(0x103u);
    fight_4d2d0();
    CHECK_EQ_INT((int)DSD(MZ_R + 0x08u), (int)MZ_ST(5));
    CHECK_EQ_INT((int)DSD(MZ_R + 0x24u), 0x40A00000);
    CHECK_EQ_INT((int)DSW(MZ_R + 0x28u), 0x0002);
    CHECK_EQ_INT((int)DSW(MZ_E + 0x18u), 0x3C + 26);
    /* Equal x (0x2BE1C = 0): no flip either. */
    mz_seed(3, 0x05u);
    DSD(MZ_R + 0x30u) = bd << 16;
    DSB(MZ_R + 0x29u) = 0;
    DSD(MZ_PS(5) + 4u) = 0x200u;
    DSD(MZ_PS(2) + 4u) = 0x200u;
    fight_4d2d0();
    CHECK_EQ_INT((int)DSW(MZ_R + 0x28u), 0x0002);

    /* G: type 4, the lie. +0x18 counts (signed); at zero: rng(2) re-arms
     * the timer (rng(0x3C) + 0x3C); else rng(2) and |x| < 0x4D00 walk (the
     * direction rng(2): +0x34 0x80 hflip clear, or -0x80 hflip set; the
     * 0xC95D4[3] stream at 3.0, type 4, timer rng(0x1E) + 0x3C); else the
     * climb (0xC95EC[3] at 3.0, +0x38 = 0x20, type 5, +0x1C bit 7 cleared). */
    mz_seed(4, 0x85u);
    DSW(MZ_E + 0x18u) = 2;
    fight_4d2d0();
    CHECK_EQ_INT((int)DSW(MZ_E + 0x18u), 1);
    CHECK_EQ_INT((int)DSD(DS_000EF6D8), 0x100);
    mz_seed(4, 0x85u);
    DSW(MZ_E + 0x18u) = 1;
    rng_seed(0x10Au);
    fight_4d2d0();
    CHECK_EQ_INT((int)DSW(MZ_E + 0x18u), 0x3C + 28);
    CHECK_EQ_INT((int)DSB(MZ_E + 0x1Eu), 4);
    CHECK_EQ_INT((int)DSD(MZ_R + 0x08u), 0x0BAD);
    CHECK_EQ_INT((int)DSD(DS_000EF6D8), (int)mz_rng_after(0x10Au, 2u));
    {
        static const u32 x[5] = { 0x1000u, 0x4CFFu, (u32)-0x4CFF, 0x1000u, 0x1000u };
        static const u32 seed[5] = { 0x115u, 0x115u, 0x115u, 0x10Bu, 0x10Bu };
        static const u16 v34[5] = { 0x0080u, 0x0080u, 0x0080u, 0xFF80u, 0xFF80u };
        static const u8 v29[5] = { 0x00u, 0x00u, 0x00u, 0x40u, 0x40u };
        static const int t18[5] = { 0x3C + 16, 0x3C + 16, 0x3C + 16, 0x3C + 2, 0x3C + 2 };
        for (i = 0; i < 5u; i++) {
            mz_seed(4, 0x85u);
            DSW(MZ_E + 0x18u) = (i == 4u) ? 0u : 1u;
            DSD(MZ_R + 0x18u) = x[i];
            DSB(MZ_R + 0x29u) = (u8)(0x40u - v29[i]);
            rng_seed(seed[i]);
            fight_4d2d0();
            CHECK_EQ_INT((int)DSW(MZ_R + 0x34u), v34[i]);
            CHECK_EQ_INT((int)DSB(MZ_R + 0x29u), v29[i]);
            CHECK_EQ_INT((int)DSD(MZ_R + 0x08u), (int)MZ_ST(2));
            CHECK_EQ_INT((int)DSD(MZ_R + 0x24u), 0x40400000);
            CHECK_EQ_INT((int)DSB(MZ_E + 0x1Eu), 4);
            CHECK_EQ_INT((int)DSW(MZ_E + 0x18u), t18[i]);
            CHECK_EQ_INT((int)DSB(MZ_E + 0x1Cu), 0x85);
            CHECK_EQ_INT((int)DSB(MZ_E + 0x1Fu), 3);
            CHECK_EQ_INT((int)DSD(DS_000EF6D8), (int)mz_rng_after(seed[i], 4u));
        }
    }
    {
        static const u32 x[3] = { 0x4D00u, (u32)-0x4D00, 0x1000u };
        static const u32 seed[3] = { 0x100u, 0x100u, 0x103u };
        for (i = 0; i < 3u; i++) {
            mz_seed(4, 0x85u);
            DSW(MZ_E + 0x18u) = 1;
            DSD(MZ_R + 0x18u) = x[i];
            rng_seed(seed[i]);
            fight_4d2d0();
            CHECK_EQ_INT((int)DSD(MZ_R + 0x08u), (int)MZ_ST(3));
            CHECK_EQ_INT((int)DSD(MZ_R + 0x24u), 0x40400000);
            CHECK_EQ_INT((int)DSW(MZ_R + 0x38u), 0x0020);
            CHECK_EQ_INT((int)DSW(MZ_R + 0x34u), 0x1111);
            CHECK_EQ_INT((int)DSB(MZ_E + 0x1Eu), 5);
            CHECK_EQ_INT((int)DSB(MZ_E + 0x1Cu), 0x05);
            CHECK_EQ_INT((int)DSD(DS_000EF6D8), (int)mz_rng_after(seed[i], 2u));
        }
    }

    /* H: type 5, the climb: +0x2C = 0x496AC(0x600) = 0xE80; the y word
     * +0x32 below the entry's +0x1A (signed) keeps climbing, else 0xC9544[3]
     * at 3.0, stopped, type 0. */
    {
        static const u16 y32[4] = { 0x0600u, 0x0600u, 0x0600u, 0xFFF0u };
        static const u16 e1a[4] = { 0x0601u, 0x0600u, 0xFFFFu, 0x0000u };
        static const int land[4] = { 0, 1, 1, 0 };
        for (i = 0; i < 4u; i++) {
            mz_seed(5, 0x05u);
            DSW(MZ_R + 0x32u) = y32[i];
            DSW(MZ_E + 0x1Au) = e1a[i];
            fight_4d2d0();
            CHECK_EQ_INT((int)DSB(MZ_E + 0x1Eu), land[i] ? 0 : 5);
            CHECK_EQ_INT((int)DSD(MZ_R + 0x08u), land[i] ? (int)MZ_ST(0) : 0x0BAD);
            CHECK_EQ_INT((int)DSW(MZ_R + 0x38u), land[i] ? 0 : 0x3333);
            CHECK_EQ_INT((int)DSW(MZ_R + 0x34u), land[i] ? 0 : 0x1111);
        }
        CHECK_EQ_INT((int)DSW(MZ_R + 0x2Cu), 0x0F80);  /* y -16 -> 0x496AC 0xF80 */
        mz_seed(5, 0x05u);
        fight_4d2d0();
        CHECK_EQ_INT((int)DSW(MZ_R + 0x2Cu), 0x0E80);
        CHECK_EQ_INT((int)DSD(MZ_R + 0x24u), 0x40400000);
    }

    /* I: type 6, the tumble. Falling (+0x36 < 0) sets +0x1C bit 7; the
     * shadow follows x and the y word; above ground +0x36 -= 0x10. */
    mz_seed(6, 0x05u);
    DSD(MZ_E + 0x10u) = MZ_SH;
    DSW(MZ_R + 0x34u) = 0; DSW(MZ_R + 0x36u) = 0xFFE0u;
    fight_4d2d0();
    CHECK_EQ_INT((int)DSB(MZ_E + 0x1Cu), 0x85);
    CHECK_EQ_INT((int)DSW(MZ_R + 0x36u), 0xFFD0);
    CHECK_EQ_INT((int)DSD(MZ_SH + 0x18u), 0x1000);
    CHECK_EQ_INT((int)DSW(MZ_SH + 0x32u), 0x0600);
    CHECK_EQ_INT((int)DSB(MZ_E + 0x1Eu), 6);
    mz_seed(6, 0x05u);
    DSW(MZ_R + 0x34u) = 0; DSW(MZ_R + 0x36u) = 0x0100u;
    fight_4d2d0();
    CHECK_EQ_INT((int)DSB(MZ_E + 0x1Cu), 0x05);
    CHECK_EQ_INT((int)DSW(MZ_R + 0x36u), 0x00F0);
    /* +0x36 = 0 is not falling (bit 7 stays clear); no shadow to follow
     * (the words at record 0 keep their sentinels). A height sum of 1 is
     * still airborne. */
    mz_seed(6, 0x05u);
    DSW(MZ_R + 0x34u) = 0; DSW(MZ_R + 0x36u) = 0;
    fight_4d2d0();
    CHECK_EQ_INT((int)DSB(MZ_E + 0x1Cu), 0x05);
    CHECK_EQ_INT((int)DSW(MZ_R + 0x36u), 0xFFF0);
    CHECK_EQ_INT((int)DSW(0x00000032u), 0x5A5A);
    mz_seed(6, 0x05u);
    DSD(MZ_R + 0x34u) = 0xFFF00000u;
    DSD(MZ_R + 0x1Cu) = 0x11u;
    fight_4d2d0();
    CHECK_EQ_INT((int)DSW(MZ_R + 0x36u), 0xFFE0);
    CHECK_EQ_INT((int)DSB(MZ_E + 0x1Eu), 6);
    /* The landing (-0x10 + 0x10 = 0): the shadow dies and +0x10 clears;
     * 0xC973C[3] at 2.0 and type 8 (0xC973C[3] = 0: 0xC9544[3] at 3.0,
     * type 4); +0x1C, +0x36, +0x34 and +0x1F zeroed; +0x1C bit 7 from the
     * falling +0x36. DS_00104AC4 = 2: no stop (+0x38 and +0x29 kept, +0x1C
     * bit 2 clear); 1: the stop's +0x1C bit 2, +0x38 and 0x4AC38's +0x29. */
    for (i = 0; i < 4u; i++) {
        mz_seed(6, 0x01u);
        DSD(MZ_E + 0x10u) = MZ_SH;
        DSD(MZ_R + 0x1Cu) = 0x10u;
        DSW(MZ_R + 0x34u) = 0x0777u; DSW(MZ_R + 0x36u) = 0xFFF0u;
        DSD(MZ_R + 0x34u) = 0xFFF00777u;
        if (i & 1u) DSD(0x000C973Cu + 12u) = 0;
        if (i & 2u) DSD(DS_00104AC4) = 1u;
        fight_4d2d0();
        CHECK_EQ_INT((int)DSD(MZ_E + 0x10u), 0);
        CHECK_EQ_INT((int)(DSB(MZ_SH + 0x28u) & 0x08u), 0x08);
        CHECK_EQ_INT((int)DSD(MZ_R + 0x08u), (i & 1u) ? (int)MZ_ST(0) : (int)MZ_ST(6));
        CHECK_EQ_INT((int)DSD(MZ_R + 0x24u), (i & 1u) ? 0x40400000 : 0x40000000);
        CHECK_EQ_INT((int)DSB(MZ_E + 0x1Eu), (i & 1u) ? 4 : 8);
        CHECK_EQ_INT((int)DSD(MZ_R + 0x1Cu), 0);
        CHECK_EQ_INT((int)DSW(MZ_R + 0x36u), 0);
        CHECK_EQ_INT((int)DSW(MZ_R + 0x34u), 0);
        CHECK_EQ_INT((int)DSB(MZ_E + 0x1Fu), 0);
        CHECK_EQ_INT((int)DSB(MZ_E + 0x1Cu), (i & 2u) ? 0x85 : 0x81);
        CHECK_EQ_INT((int)DSW(MZ_R + 0x38u), (i & 2u) ? 0 : 0x3333);
        CHECK_EQ_INT((int)DSB(MZ_R + 0x29u), (i & 2u) ? 0x10 : 0x40);
        CHECK_EQ_INT((int)DSD(MZ_E + 0x14u), 0x7777);
    }
    /* The landing without a shadow, with the stop but no owner (the
     * stop's own +0x38; no 0x4AC38 on entry 0). */
    mz_seed(6, 0x01u);
    DSD(MZ_R + 0x1Cu) = 0x10u;
    DSD(MZ_R + 0x34u) = 0xFFF00777u;
    DSD(DS_00104AC4) = 1u;
    DSD(MZ_R + 0x14u) = 0;
    fight_4d2d0();
    CHECK_EQ_INT((int)DSB(0x0000001Eu), 0x77);
    CHECK_EQ_INT((int)(DSB(0x00000028u) & 0x08u), 0);
    CHECK_EQ_INT((int)DSW(MZ_R + 0x38u), 0);
    CHECK_EQ_INT((int)DSB(MZ_R + 0x29u), 0x40);
    CHECK_EQ_INT((int)DSB(MZ_E + 0x1Eu), 8);
    CHECK_EQ_INT((int)DSD(MZ_R + 0x08u), (int)MZ_ST(6));

    /* J: types 0, 7 and 9 take 0x4D224. The rise 0x4D108 (rng(0x3C) = 0):
     * 0xC95BC[3] at 3.0, +0x1A = the y word 0x600, +0x38 = -0x20, type 3. */
    {
        static const u8 ty[3] = { 0u, 7u, 9u };
        for (i = 0; i < 3u; i++) {
            mz_seed(ty[i], 0x05u);
            DSW(MZ_E + 0x1Au) = 0x1234u;
            rng_seed(0x69Du);
            fight_4d2d0();
            CHECK_EQ_INT((int)DSB(MZ_E + 0x1Eu), 3);
            CHECK_EQ_INT((int)DSD(MZ_R + 0x08u), (int)MZ_ST(1));
            CHECK_EQ_INT((int)DSD(MZ_R + 0x24u), 0x40400000);
            CHECK_EQ_INT((int)DSW(MZ_E + 0x1Au), 0x0600);
            CHECK_EQ_INT((int)DSW(MZ_R + 0x38u), 0xFFE0);
            CHECK_EQ_INT((int)DSD(DS_000EF6D8), (int)mz_rng_after(0x69Du, 1u));
        }
    }
    /* The walk 0x4D150: rng(4) * 0xC00 right (rng(2) 1) or left of
     * DS_00104B1A's fighter (its pset x), through 0x2BE4C (R's +0x44 word
     * 0x10: + 0x20 - 0x2A00) into +0x14; type 1; +0x34 0x80 with the hflip
     * cleared when the target is right of R's x (signed), else -0x80 and
     * set; 0xC95D4[3] at 3.0. */
    {
        static const u32 seed[4] = { 0x11865u, 0x11865u, 0x11865u, 0x53Au };
        static const u8 side[4] = { 0u, 1u, 0u, 0u };
        static const u32 rx[4] = { 0x1000u, 0x1000u, 0x1E20u, 0x1000u };
        static const int tgt[4] = { 0x1E20, 0x3E20, 0x1E20, -0x1DE0 };
        static const u16 v34[4] = { 0x0080u, 0x0080u, 0xFF80u, 0xFF80u };
        static const u8 v29[4] = { 0x00u, 0x00u, 0x40u, 0x40u };
        for (i = 0; i < 4u; i++) {
            mz_seed(0, 0x05u);
            DSB(0x00104B1Au) = side[i];
            DSD(MZ_PS(1) + 4u) = 0x3000u;
            DSD(MZ_PS(2) + 4u) = 0x5000u;
            DSD(MZ_R + 0x44u) = 0x00100000u;
            DSD(MZ_R + 0x18u) = rx[i];
            DSB(MZ_R + 0x29u) = (u8)(0x40u - v29[i]);
            rng_seed(seed[i]);
            fight_4d2d0();
            CHECK_EQ_INT((int)DSB(MZ_E + 0x1Eu), 1);
            CHECK_EQ_INT((int)DSD(MZ_E + 0x14u), tgt[i]);
            CHECK_EQ_INT((int)DSW(MZ_R + 0x34u), v34[i]);
            CHECK_EQ_INT((int)DSB(MZ_R + 0x29u), v29[i]);
            CHECK_EQ_INT((int)DSD(MZ_R + 0x08u), (int)MZ_ST(2));
            CHECK_EQ_INT((int)DSD(MZ_R + 0x24u), 0x40400000);
            CHECK_EQ_INT((int)DSD(DS_000EF6D8), (int)mz_rng_after(seed[i], 4u));
        }
    }
    /* Neither draw: the slot's +0x42 bit 1 or DS_001088B2[+0x21] takes
     * 0x4B3F0 (0xC955C[3], type 8, +0x55 = 0); +0x42 bit 0 or
     * DS_0010889E[+0x21] 0x4B430 (0xC958C[3]); bit 1 with 0xC955C[3] = 0
     * falls through to bit 0; DS_001088B2[0] (the other side) does nothing. */
    for (i = 0; i < 7u; i++) {
        mz_seed(0, 0x05u);
        if (i == 0u) DSB(MZ_S1 + 0x42u) = 2;
        if (i == 1u) DSB(DS_001088B2 + 1u) = 1;
        if (i == 2u) DSB(MZ_S1 + 0x42u) = 1;
        if (i == 3u) DSB(DS_0010889E + 1u) = 1;
        if (i == 4u) { DSB(MZ_S1 + 0x42u) = 3; DSD(0x000C955Cu + 12u) = 0; }
        if (i == 5u) { DSB(DS_001088B2) = 1; DSB(DS_0010889E) = 1; }
        fight_4d2d0();
        if (i <= 1u) {
            CHECK_EQ_INT((int)DSD(MZ_R + 0x08u), (int)MZ_ST(8));
            CHECK_EQ_INT((int)DSB(MZ_R + 0x55u), 0);
        } else if (i <= 4u) {
            CHECK_EQ_INT((int)DSD(MZ_R + 0x08u), (int)MZ_ST(9));
            CHECK_EQ_INT((int)DSB(MZ_R + 0x55u), 0);
        } else {
            CHECK_EQ_INT((int)DSD(MZ_R + 0x08u), 0x0BAD);
            CHECK_EQ_INT((int)DSB(MZ_R + 0x55u), 0x77);
        }
        CHECK_EQ_INT((int)DSB(MZ_E + 0x1Eu), (i <= 4u) ? 8 : 0);
        CHECK_EQ_INT((int)DSD(DS_000EF6D8), (int)mz_rng_after(0x100u, 2u));
    }
    /* 0x4BD4C first (DS_001088C2 and 0xC9754[3]): no draw. */
    mz_seed(0, 0x05u);
    DSD(0x000C9754u + 12u) = MZ_ST(7);
    fight_4d2d0();
    CHECK_EQ_INT((int)DSB(MZ_E + 0x1Eu), 8);
    CHECK_EQ_INT((int)DSD(MZ_R + 0x08u), (int)MZ_ST(7));
    CHECK_EQ_INT((int)DSD(DS_000EF6D8), 0x100);

    /* K: mode 0x22 runs the prelude 0x4D7A4 (its hit test writes B54 = 2;
     * the grab move with +0x1C bit 6 returns 0 without a write), mode 0x24
     * does not (B54 keeps its sentinel). */
    for (i = 0; i < 2u; i++) {
        mz_seed(4, 0xC5u);
        DSW(DS_00104B00) = (i == 0u) ? 0x22u : 0x24u;
        DSD(MZ_PS(5) + 4u) = (u32)((100 + 0x18) * 64);
        DSD(MZ_PS(5) + 8u) = (u32)((100 + 0x38) * 64);
        DSB(MZ_S0 + 0x5Fu) = 0x2Du;
        fight_4d2d0();
        CHECK_EQ_INT((int)DSD(DS_00100B54), (i == 0u) ? 2 : (int)0xDEADBEEFu);
        CHECK_EQ_INT((int)DSB(MZ_E + 0x1Eu), 4);
        CHECK_EQ_INT((int)DSW(MZ_E + 0x18u), 49);
    }

    /* L: the count and the refill. The entries count into DS_00104B1A's
     * word; when the side's slot +0x81 exceeds it (a signed word)
     * 0x4987C(side, the difference, 0) takes entries off the free list. With
     * two type-8 entries, side 1 and +0x81 = 3: one refill entry F1 (seed
     * 0x101: 0x49388's rng(100) = 99 -> descriptor 2, 0xBB498, +0x48 0x22;
     * y = side 1's y 0x80 + 0x400 + rng(0x300) 666 = 0x71A), x = 0x2BE4C(side
     * 1's record, its pset x 0x5000 + 0x5780) = 0x7D80 (it is right of side
     * 0's 0x3000), then 0x4B144 makes it type 1. */
    mz_seed(8, 0x40u);
    mz_second();
    mz_free(0);
    DSW(DS_00104B00) = 0x24u;
    DSB(0x00104B1Au) = 1;
    DSB(MZ_S1 + 0x81u) = 3;
    DSB(MZ_S0 + 0x81u) = 9;
    DSW(DS_00108860 + 2u) = 100;
    DSD(MZ_PS(1) + 4u) = 0x3000u;
    DSD(MZ_PS(2) + 4u) = 0x5000u;
    DSD(FIGHT_RECS + 0x100u + 0x30u) = 0x00800000u;
    rng_seed(0x101u);
    fight_4d2d0();
    CHECK_EQ_INT((int)DSD(DS_0010884C), (int)MZ_F1);
    CHECK_EQ_INT((int)DSD(DS_001083C4), (int)DS_001083C4);
    a = DSD(MZ_F1 + 8u);
    CHECK(a != 0u, "0x4987C spawned the refill's actor");
    if (a != 0u) {
        CHECK_EQ_INT((int)DSD(a + 0x14u), (int)MZ_F1);
        CHECK_EQ_INT((int)DSB(a + 0x48u), 0x22);
        CHECK_EQ_INT((int)DSW(a + 0x32u), 0x071A);
        CHECK_EQ_INT((int)DSD(a + 0x18u), 0x7D80);
        CHECK_EQ_INT((int)DSW(a + 0x2Cu), (int)(((0xB00u - 0x71Au) >> 1) + 0xC00u));
        CHECK_EQ_INT((int)(DSB(a + 0x29u) & 0x10u), 0x10);
    }
    CHECK_EQ_INT((int)DSB(MZ_F1 + 0x21u), 1);
    CHECK_EQ_INT((int)DSD(MZ_F1 + 0x0Cu), (int)MZ_S1);
    CHECK_EQ_INT((int)DSB(MZ_F1 + 0x1Fu), 0);
    CHECK_EQ_INT((int)DSD(MZ_F1 + 0x10u), 0);
    CHECK_EQ_INT((int)DSB(MZ_F1 + 0x1Eu), 1);
    CHECK_EQ_INT((int)DSW(MZ_F1 + 0x1Cu), 0);
    /* +0x81 equal to the count (2), or the other side's +0x81: no refill,
     * no draw. */
    for (i = 0; i < 2u; i++) {
        mz_seed(8, 0x40u);
        mz_second();
        mz_free(0);
        DSB(0x00104B1Au) = (u8)i;
        DSB(MZ_S1 + 0x81u) = (i == 0u) ? 9u : 2u;
        DSB(MZ_S0 + 0x81u) = (i == 0u) ? 2u : 9u;
        fight_4d2d0();
        CHECK_EQ_INT((int)DSD(DS_001083C4), (int)MZ_F1);
        CHECK_EQ_INT((int)DSD(DS_0010884C), (int)MZ_E);
        CHECK_EQ_INT((int)DSD(DS_000EF6D8), 0x100);
    }
    /* One entry, side 0 wanting 3: two refills (both free entries, side 0). */
    mz_seed(8, 0x40u);
    mz_free(1);
    DSB(MZ_S0 + 0x81u) = 3;
    DSW(DS_00108860) = 100;
    fight_4d2d0();
    CHECK_EQ_INT((int)DSD(DS_001083C4), (int)DS_001083C4);
    CHECK_EQ_INT((int)DSD(DS_0010884C), (int)MZ_F2);
    CHECK_EQ_INT((int)DSD(MZ_F2), (int)MZ_F1);
    CHECK_EQ_INT((int)DSB(MZ_F1 + 0x21u), 0);
    CHECK_EQ_INT((int)DSB(MZ_F2 + 0x21u), 0);
    CHECK_EQ_INT((int)DSD(MZ_F2 + 0x0Cu), (int)MZ_S0);

    mz_restore();
}

/* 0x4987C's flyers (kind 1/2) and its kind-0 arm directly, and the effects
 * tail 0x4A616 that calls it (record §43-A). Side 0's pset x is 0x3000,
 * side 1's 0x5000, both records' +0x44 zero, the layer word bd. */
static void check_flyers_4987c(void)
{
    u32 bd, a, i;
    if (!mz_save()) { CHECK(0, "the §43-A snapshot allocates"); return; }
    bd = DSW(DS_000BD898);

#define MZ_FLY() do {                                                   \
        mz_seed(8, 0x40u);                                              \
        DSD(DS_0010884C) = DS_0010884C; DSD(DS_0010884C + 4u) = DS_0010884C; \
        mz_free(1);                                                     \
        DSW(DS_00104B00) = 3;                                           \
        DSD(MZ_PS(1) + 4u) = 0x3000u;                                   \
        DSD(MZ_PS(2) + 4u) = 0x5000u;                                   \
    } while (0)

    /* A: one flyer, kind 1 (0xC9538[1] = 0xBB59C, +0x48 0x20) and kind 2
     * (0xC9538[2] = 0xBB5B0, 0x23). Side 0 is left of side 1, so x =
     * 0x2BE4C(side 0's record, 0x3000 - 0x5780) = -0x5180, the flag set:
     * +0x34 = 0x100 and no hflip; y = bd - 0x200 outside mode 0x22; type 7,
     * +0x1C = 0x10 (no bit 0 for one), +0x2E the descriptor's 0x40. No draw. */
    for (i = 1; i <= 2u; i++) {
        u32 sv_c953c = DSD(0x000C953Cu), k;
        MZ_FLY();
        /* Kind 1 spawns a copy of 0xBB59C whose +0x28 word (desc +8) is
         * 0x4000 in place of 0x1000 and whose stream is a scratch sprite
         * word, so +0x29 bit 4 can only come from 0x4987C's own OR
         * (0x49B55) and must keep the spawn's bit 6. */
        if (i == 1u) {
            for (k = 0; k < 0x14u; k++)
                DSB(FIGHT_RECS + 0x3A00u + k) = DSB(0x000BB59Cu + k);
            DSW(FIGHT_RECS + 0x3A00u + 8u) = 0x4000u;
            DSD(FIGHT_RECS + 0x3A00u) = MZ_ST(11);
            DSD(0x000C953Cu) = FIGHT_RECS + 0x3A00u;
        }
        fight_4987c(0u, 1, i);
        DSD(0x000C953Cu) = sv_c953c;
        CHECK_EQ_INT((int)DSD(DS_0010884C), (int)MZ_F1);
        CHECK_EQ_INT((int)DSD(DS_001083C4), (int)MZ_F2);
        a = DSD(MZ_F1 + 8u);
        CHECK(a != 0u, "0x4987C spawned a flyer");
        if (a == 0u) continue;
        CHECK_EQ_INT((int)DSB(a + 0x48u), i == 1u ? 0x20 : 0x23);
        CHECK_EQ_INT((int)DSD(a + 0x18u), -0x5180);
        CHECK_EQ_INT((int)DSW(a + 0x32u), (int)(bd - 0x200u));
        CHECK_EQ_INT((int)DSW(a + 0x2Cu), 0x0F80);
        CHECK_EQ_INT((int)DSW(a + 0x34u), 0x0100);
        CHECK_EQ_INT((int)DSB(a + 0x29u), i == 1u ? 0x50 : 0x10);
        if (i == 1u) CHECK_EQ_INT((int)DSD(a + 0x08u), (int)MZ_ST(11));
        CHECK_EQ_INT((int)DSD(a + 0x1Cu), 0);           /* EBX = 0 */
        CHECK_EQ_INT((int)DSB(a + 0x4Au), 0);           /* the stack 0 */
        CHECK_EQ_INT((int)DSW(a + 0x2Eu), 0x0040);
        CHECK_EQ_INT((int)DSB(a + 0x4Eu), 0);
        CHECK_EQ_INT((int)DSD(a + 0x14u), (int)MZ_F1);
        CHECK_EQ_INT((int)DSB(MZ_F1 + 0x1Eu), 7);
        CHECK_EQ_INT((int)DSW(MZ_F1 + 0x1Cu), 0x0010);
        CHECK_EQ_INT((int)DSB(MZ_F1 + 0x21u), 0);
        CHECK_EQ_INT((int)DSD(MZ_F1 + 0x0Cu), (int)MZ_S0);
        CHECK_EQ_INT((int)DSB(MZ_F1 + 0x1Fu), 0);
        CHECK_EQ_INT((int)DSD(MZ_F1 + 0x10u), 0);
        CHECK_EQ_INT((int)DSD(DS_000EF6D8), 0x100);
    }

    /* B: a pair. Kind 1: F1 0xC953C[0] (0x20) with +0x1C bit 0, 0x780
     * further out (-0x5900) and +0x34 = 0xC0; F2 0xC953C[1] (0x23) at
     * -0x5180, 0x100. Kind 2 swaps the descriptors. The list is head, F2,
     * F1. Side 1 (right of side 0): x = 0x2BE4C(side 1's record, 0x5000 +
     * 0x5780) = 0x7D80 (F1 0x780 further: 0x8500), the flag clear: +0x34 =
     * -0xC0 / -0x100 and the hflip set. */
    for (i = 0; i < 3u; i++) {
        u32 side = (i == 2u) ? 1u : 0u;
        u32 kind = (i == 1u) ? 2u : 1u;
        u32 f1, f2;
        MZ_FLY();
        fight_4987c(side, 2, kind);
        CHECK_EQ_INT((int)DSD(DS_0010884C), (int)MZ_F2);
        CHECK_EQ_INT((int)DSD(MZ_F2), (int)MZ_F1);
        CHECK_EQ_INT((int)DSD(DS_001083C4), (int)DS_001083C4);
        f1 = DSD(MZ_F1 + 8u); f2 = DSD(MZ_F2 + 8u);
        CHECK(f1 != 0u && f2 != 0u, "0x4987C spawned the pair");
        if (f1 == 0u || f2 == 0u) continue;
        CHECK_EQ_INT((int)DSB(f1 + 0x48u), kind == 1u ? 0x20 : 0x23);
        CHECK_EQ_INT((int)DSB(f2 + 0x48u), kind == 1u ? 0x23 : 0x20);
        CHECK_EQ_INT((int)DSW(MZ_F1 + 0x1Cu), 0x0011);
        CHECK_EQ_INT((int)DSW(MZ_F2 + 0x1Cu), 0x0010);
        CHECK_EQ_INT((int)DSD(f1 + 0x18u), side ? 0x8500 : -0x5900);
        CHECK_EQ_INT((int)DSD(f2 + 0x18u), side ? 0x7D80 : -0x5180);
        CHECK_EQ_INT((int)DSW(f1 + 0x34u), side ? 0xFF40 : 0x00C0);
        CHECK_EQ_INT((int)DSW(f2 + 0x34u), side ? 0xFF00 : 0x0100);
        CHECK_EQ_INT((int)DSB(f1 + 0x29u), side ? 0x50 : 0x10);
        CHECK_EQ_INT((int)DSB(f2 + 0x29u), side ? 0x50 : 0x10);
        CHECK_EQ_INT((int)DSB(MZ_F2 + 0x21u), (int)side);
        CHECK_EQ_INT((int)DSD(MZ_F2 + 0x0Cu), (int)(side ? MZ_S1 : MZ_S0));
    }

    /* C: mode 0x22: the side comes from rng(2) (1: -0x5780, the flag set,
     * even for side 1, which mode 3 would send right), y = bd, and +0x2E +=
     * 4 with +0x4E = 1; rng(2) = 0 goes right (seed 0x103: rng(3) would be
     * 1). A character-marked fighter (+0x51) does the same outside mode
     * 0x22. */
    for (i = 0; i < 3u; i++) {
        MZ_FLY();
        if (i < 2u) {
            DSW(DS_00104B00) = 0x22u;
            rng_seed(i == 0u ? 0x101u : 0x103u);
        } else {
            DSB(FIGHT_RECS + 0x100u + 0x51u) = 1;
        }
        fight_4987c(1u, 1, 1u);
        a = DSD(MZ_F1 + 8u);
        CHECK(a != 0u, "0x4987C spawned a flyer");
        if (a == 0u) continue;
        if (i == 0u) {
            CHECK_EQ_INT((int)DSD(a + 0x18u), 0x5000 - 0x5780 - 0x2A00);
            CHECK_EQ_INT((int)DSW(a + 0x34u), 0x0100);
        } else {
            CHECK_EQ_INT((int)DSD(a + 0x18u), 0x7D80);
            CHECK_EQ_INT((int)DSW(a + 0x34u), 0xFF00);
        }
        CHECK_EQ_INT((int)DSW(a + 0x32u), (int)(i < 2u ? bd : bd - 0x200u));
        CHECK_EQ_INT((int)DSW(a + 0x2Eu), 0x0044);
        CHECK_EQ_INT((int)DSB(a + 0x4Eu), 1);
        CHECK_EQ_INT((int)DSD(DS_000EF6D8),
                     (int)(i < 2u ? mz_rng_after(i == 0u ? 0x101u : 0x103u, 1u) : 0x100u));
    }
    /* Mode 0x22's pair: the first flyer 0x780 further out on the side
     * rng(2) picks (each flyer draws its own). */
    for (i = 0; i < 2u; i++) {
        MZ_FLY();
        DSW(DS_00104B00) = 0x22u;
        rng_seed(i == 0u ? 0x101u : 0x103u);
        fight_4987c(1u, 2, 1u);
        a = DSD(MZ_F1 + 8u);
        CHECK(a != 0u, "0x4987C spawned a mode-0x22 pair");
        if (a == 0u) continue;
        CHECK_EQ_INT((int)DSD(a + 0x18u), i == 0u ? 0x5000 - 0x5780 - 0x2A00 - 0x780
                                                  : 0x7D80 + 0x780);
        CHECK_EQ_INT((int)DSW(a + 0x34u), i == 0u ? 0x00C0 : 0xFF40);
    }
    /* Equal fighter x (signed `jge`): to the right, the flag clear. */
    MZ_FLY();
    DSD(MZ_PS(2) + 4u) = 0x3000u;
    fight_4987c(0u, 1, 1u);
    a = DSD(MZ_F1 + 8u);
    if (a != 0u) {
        CHECK_EQ_INT((int)DSD(a + 0x18u), 0x3000 + 0x5780 - 0x2A00);
        CHECK_EQ_INT((int)DSW(a + 0x34u), 0xFF00);
    }

    /* D: a count of 0 or -1 takes nothing; an empty free list stops it. */
    for (i = 0; i < 3u; i++) {
        MZ_FLY();
        if (i == 2u) { DSD(DS_001083C4) = DS_001083C4; DSD(DS_001083C4 + 4u) = DS_001083C4; }
        fight_4987c(0u, i == 0u ? 0 : (i == 1u ? -1 : 1), 1u);
        CHECK_EQ_INT((int)DSD(DS_0010884C), (int)DS_0010884C);
        CHECK_EQ_INT((int)DSD(DS_001083C4), i == 2u ? (int)DS_001083C4 : (int)MZ_F1);
    }

    /* E: kind 0 in mode 3 (0x49388's range 0x64): seed 0x101 picks 0xBB498
     * (+0x48 0x22) whose +0x10 becomes 0x29CDC(0, character 2) = the
     * DS_000A8AF8 table's entry 2 (DS_00105B34[0] = 0); y = 0x80 + 0x400 +
     * 666; 0x4B144 walks it (type 1); no +0x1C bit 0, no type 7. */
    MZ_FLY();
    DSB(MZ_S0 + 0x7Au) = 2;
    DSB(DS_00105B34) = 0;
    DSD(0x000BB498u + 0x10u) = 0x12345678u;
    DSD(FIGHT_RECS + 0x30u) = 0x00800000u;
    DSD(0x000C95D4u + 8u) = MZ_ST(11);      /* 0x4B144's stream, si 2 */
    rng_seed(0x101u);
    fight_4987c(0u, 1, 0u);
    a = DSD(MZ_F1 + 8u);
    CHECK(a != 0u, "0x4987C kind 0 spawned");
    if (a != 0u) {
        CHECK_EQ_INT((int)DSB(a + 0x48u), 0x22);
        CHECK_EQ_INT((int)DSD(a + 0x08u), (int)MZ_ST(11));
        CHECK_EQ_INT((int)DSW(a + 0x32u), 0x071A);
        CHECK_EQ_INT((int)DSD(a + 0x18u), -0x5180);
    }
    CHECK_EQ_INT((int)DSD(0x000BB498u + 0x10u), (int)DSD(DS_000A8AF8 + 8u));
    CHECK_EQ_INT((int)DSB(MZ_F1 + 0x1Eu), 1);
    CHECK_EQ_INT((int)DSW(MZ_F1 + 0x1Cu), 0);

    /* F: the effects tail 0x4A5A6..0x4A616: DS_001088BF 1..4 -> 0x4987C(
     * rng(2), count, kind) with (1, 1), (1, 2), (2, 1), (2, 2); 5 draws
     * nothing; the byte clears. Seed 0x101: rng(2) = 1 (side 1). */
    {
        static const int cnt[4] = { 1, 1, 2, 2 };
        static const u8 k48[4] = { 0x20u, 0x23u, 0x20u, 0x23u };
        for (i = 0; i < 5u; i++) {
            MZ_FLY();
            DSB(DS_001088BF) = (u8)(i + 1u);
            rng_seed(0x101u);
            fight_effects_pass();
            CHECK_EQ_INT((int)DSB(DS_001088BF), 0);
            if (i == 4u) {
                CHECK_EQ_INT((int)DSD(DS_0010884C), (int)DS_0010884C);
                CHECK_EQ_INT((int)DSD(DS_000EF6D8), 0x101);
                continue;
            }
            CHECK_EQ_INT((int)DSD(DS_0010884C), cnt[i] == 1 ? (int)MZ_F1 : (int)MZ_F2);
            CHECK_EQ_INT((int)DSB(MZ_F1 + 0x21u), 1);
            CHECK_EQ_INT((int)DSB(MZ_F1 + 0x1Eu), 7);
            CHECK_EQ_INT((int)DSD(DS_001083C4), cnt[i] == 1 ? (int)MZ_F2 : (int)DS_001083C4);
            a = DSD(MZ_F1 + 8u);
            if (a != 0u) CHECK_EQ_INT((int)DSB(a + 0x48u), k48[i]);
            CHECK_EQ_INT((int)DSD(DS_000EF6D8), (int)mz_rng_after(0x101u, 1u));
        }
    }
    /* Modes 7, 8 and 9 skip the tail: no flyer, DS_001088BF kept. */
    {
        static const u16 md[3] = { 7u, 8u, 9u };
        for (i = 0; i < 3u; i++) {
            MZ_FLY();
            DSW(DS_00104B00) = md[i];
            DSB(DS_001088BF) = 1;
            fight_effects_pass();
            CHECK_EQ_INT((int)DSB(DS_001088BF), 1);
            CHECK_EQ_INT((int)DSD(DS_001083C4), (int)MZ_F1);
        }
    }
#undef MZ_FLY
    mz_restore();
}

/* Count the non-empty glyph cells of text row `row`. */
static int mz_row_cells(s32 row)
{
    int n = 0;
    s32 c;
    for (c = 0; c < 0x2B; c++) if (ct_cell(row, c) != 0u) n++;
    return n;
}

/* Whether text row `row` holds exactly the glyphs 0x2F4BC draws for string
 * `id` centred in the class font (mode 0x4002): the row is saved, redrawn
 * from empty and compared cell by cell (sprite ids), then left redrawn. */
static int mz_row_ref(s32 row, u32 id)
{
    u32 got[0x2B];
    s32 c;
    int same = 1;
    for (c = 0; c < 0x2B; c++) got[c] = ct_sprite(row, c);
    mem_fill(DS_00105F38 + (u32)row * 0xACu, 0, 0xACu);
    text_cursor_hold(-1, row, game_string_get(id), 0x4002u);
    for (c = 0; c < 0x2B; c++) if (ct_sprite(row, c) != got[c]) same = 0;
    return same;
}

/* Give `side`'s fighter character `c` with the anchor that keeps its sprite
 * handle index 4 (as check_grab_arms' GR_CHAR), so the hit test keeps its
 * box. Characters 5 and 6 shift the box left by 0x15B90's palette
 * adjustment (a = 0 -> 15), off the 8-pixel sprite; the side's raw box x 4
 * and the ball's point 8 screen units left keep the hit (a scan of the
 * point found hits for x offsets -37..-1). */
static void mz_char(u32 side, u32 c)
{
    DSB(DS_0010782A + side * 0x94u) = (u8)c;
    DSD(DS_00100AF0 + side * 4u) = (u32)(4 - (s32)camera_char_const(c));
    if (c == 5u || c == 6u) {
        DSB(DS_00100AC8 + side * 4u) = 4u;
        DSD(MZ_PS(5) + 4u) = (u32)((100 + 0x18 - 8) * 64);
    }
}

/* 0x4C60C, 0x4C784, 0x4CC0C and 0x2F510 (record §43-A), on mz_seed's entry
 * E as the ball (DS_00108864, its point on side 0), DS_00108868 = B1 (pset
 * 6, x 0x4000) and DS_0010886C = B2 (pset 8). The 0xC976C spit stream
 * 0xEF65A and B2's 0xEF680/0xEF6AC start with opcode words, so each gets a
 * plain sprite word 4 in place. */
static void check_volleyball(void)
{
    u32 bd, a, i, spit;
    if (!mz_save()) { CHECK(0, "the §43-A snapshot allocates"); return; }
    bd = DSW(DS_000BD898);
    game_string_table_load("data/game/C");

#define MZ_BALL(c1c) do {                                               \
        mz_seed(4, (u8)(c1c));                                          \
        DSW(DS_00104B00) = 3u;                                          \
        DSD(MZ_PS(5) + 4u) = (u32)((100 + 0x18) * 64);                  \
        DSD(MZ_PS(5) + 8u) = (u32)((100 + 0x38) * 64);                  \
        DSD(MZ_E + 0x10u) = MZ_SH;                                      \
        DSD(DS_00108864) = MZ_E;                                        \
        mem_fill(MZ_PS(6), 0, 0x20u); mem_fill(MZ_PS(8), 0, 0x20u);     \
        DSW(MZ_B1 + 0x56u) = 6; DSW(MZ_B2 + 0x56u) = 8;                 \
        DSD(MZ_PS(6) + 4u) = 0x4000u;                                   \
        DSW(MZ_B1 + 0x36u) = 0x1234u; DSW(MZ_B2 + 0x36u) = 0x1234u;     \
        DSD(DS_00108868) = MZ_B1; DSD(DS_0010886C) = MZ_B2;             \
        DSD(DS_001077E4) = 0; DSD(DS_00107878) = 0;                     \
        DSW(DS_00108898) = 0x5555u; DSW(DS_001088AC) = 0x5555u;         \
        DSW(DS_001088A0) = 0;                                           \
        DSB(DS_0010889C) = 0; DSB(DS_0010889D) = 0;                     \
        DSB(DS_001088AE) = 7; DSB(DS_001088AE + 1u) = 5;                \
        DSW(0x000EF65Au) = 4; DSW(0x000EF680u) = 4; DSW(0x000EF6ACu) = 4; \
        DSD(DS_00105F34) = 0x55555555u;                                 \
        mem_fill(DS_00105F38, 0, 0xACu * 12u);                          \
    } while (0)

    /* A: struck. Side 0 of character 1 (not an eater): DS_00108898 = 0,
     * +0x20 = 0, +0x1C bit 3 cleared, +0x1F 3 -> 4, and 0x4CB18 with flag 0
     * (+0x36 = (0x3BC0 - 0x100) / 0x16 = 0x2AB, type 6, bit 7 cleared) for a
     * +0x5F outside 0xC..0xF, else flag 1 (+0x36 = 0) and +0x1C bit 3 set. */
    {
        static const u8 mv[5] = { 0x00u, 0x0Bu, 0x0Cu, 0x0Fu, 0x10u };
        for (i = 0; i < 5u; i++) {
            int f = (mv[i] >= 0x0Cu && mv[i] <= 0x0Fu);
            MZ_BALL(0x8Du);
            mz_char(0u, 1u);
            DSB(MZ_S0 + 0x5Fu) = mv[i];
            fight_4c60c(MZ_E, 3u);
            CHECK_EQ_INT((int)DSD(DS_00100B54), 2);
            CHECK_EQ_INT((int)DSW(DS_00108898), 0);
            CHECK_EQ_INT((int)DSB(MZ_E + 0x20u), 0);
            CHECK_EQ_INT((int)DSB(MZ_E + 0x1Fu), 4);
            CHECK_EQ_INT((int)DSB(MZ_E + 0x1Eu), 6);
            CHECK_EQ_INT((int)DSW(MZ_R + 0x36u), f ? 0 : 0x02AB);
            CHECK_EQ_INT((int)DSB(MZ_E + 0x1Cu), f ? 0x0D : 0x05);
            CHECK_EQ_INT((int)DSD(MZ_R + 0x08u), (int)MZ_ST(10));
            CHECK_EQ_INT((int)DSB(DS_001088AE + 1u), 5);    /* no +0x4A */
        }
    }
    /* Both sides hit (3): side 0 (0x4C653). */
    MZ_BALL(0x8Du);
    ph_seed(FIGHT_RECS + 0x200u, FIGHT_RECS + 0x300u, 1);
    DSD(MZ_PS(5) + 4u) = (u32)((100 + 0x18) * 64);
    DSD(MZ_PS(5) + 8u) = (u32)((100 + 0x38) * 64);
    mz_char(0u, 1u); mz_char(1u, 1u);
    DSB(MZ_S1 + 0x5Fu) = 0x0Du;
    fight_4c60c(MZ_E, 3u);
    CHECK_EQ_INT((int)DSB(MZ_E + 0x20u), 0);
    CHECK_EQ_INT((int)DSW(MZ_R + 0x36u), 0x02AB);
    /* A held actor (+0x4A = 2) is released first: the link, +0x2A bit 3,
     * +0x29 bit 6, +0x4A, the entry's bit 6, DS_001088AE[+0x21] counts. */
    MZ_BALL(0xCDu);
    mz_char(0u, 4u);
    DSB(MZ_R + 0x4Au) = 2; DSB(MZ_R + 0x2Au) = 0x0Cu; DSB(MZ_R + 0x29u) = 0x50u;
    DSB(DSD(DS_001014F4) + 2u * 0x68u + 0x4Bu) = 0x99u;
    fight_4c60c(MZ_E, 3u);
    CHECK_EQ_INT((int)DSB(DSD(DS_001014F4) + 2u * 0x68u + 0x4Bu), 0);
    CHECK_EQ_INT((int)DSB(MZ_R + 0x2Au), 0x04);
    CHECK_EQ_INT((int)DSB(MZ_R + 0x29u), 0x10);
    CHECK_EQ_INT((int)DSB(MZ_R + 0x4Au), 0);
    CHECK_EQ_INT((int)DSB(MZ_E + 0x1Cu), 0x05);
    CHECK_EQ_INT((int)DSB(DS_001088AE + 1u), 6);
    CHECK_EQ_INT((int)DSB(DS_001088AE), 7);
    /* Side 1 alone (side 0's box emptied): +0x20 = 1 and side 1's move. */
    MZ_BALL(0x8Du);
    ph_seed(FIGHT_RECS + 0x200u, FIGHT_RECS + 0x300u, 1);
    DSB(DS_00100AC8 + 2u) = 0;
    DSD(MZ_PS(5) + 4u) = (u32)((100 + 0x18) * 64);
    DSD(MZ_PS(5) + 8u) = (u32)((100 + 0x38) * 64);
    mz_char(1u, 6u); DSB(MZ_S1 + 0x5Fu) = 0x0Du;
    mz_char(0u, 7u); DSB(MZ_S0 + 0x5Fu) = 0;
    fight_4c60c(MZ_E, 3u);
    CHECK_EQ_INT((int)DSB(MZ_E + 0x1Fu), 4);
    CHECK_EQ_INT((int)DSB(MZ_E + 0x20u), 1);
    CHECK_EQ_INT((int)DSW(MZ_R + 0x36u), 0);
    /* No hit, or a character above 6 (7, 0xFF): nothing. */
    for (i = 0; i < 3u; i++) {
        MZ_BALL(0x8Du);
        if (i == 0u) DSD(MZ_PS(5) + 4u) = (u32)((100 + 0x18 + 0x40) * 64);
        mz_char(0u, i == 1u ? 7u : (i == 2u ? 0xFFu : 1u));
        fight_4c60c(MZ_E, 3u);
        CHECK_EQ_INT((int)DSB(MZ_E + 0x1Fu), 3);
        CHECK_EQ_INT((int)DSW(DS_00108898), 0x5555);
        CHECK_EQ_INT((int)DSB(MZ_E + 0x1Eu), 4);
    }
    /* The eaters: characters 0, 3, 5 with +0x5F = 0 and 2 with +0x5F = 1
     * call 0x4C784 (the other side scores; the ball entry is not struck);
     * 0/3/5 with 1, 2 with 0, and 4 with 0 are struck. */
    {
        static const u8 ch[9] = { 0u, 3u, 5u, 2u, 0u, 3u, 5u, 2u, 4u };
        static const u8 mv[9] = { 0u, 0u, 0u, 1u, 1u, 1u, 1u, 0u, 0u };
        for (i = 0; i < 9u; i++) {
            MZ_BALL(0x8Du);
            mz_free(0);
            mz_char(0u, ch[i]);
            DSB(MZ_S0 + 0x5Fu) = mv[i];
            fight_4c60c(MZ_E, 3u);
            CHECK_EQ_INT((int)DSB(DS_0010889D), i < 4u ? 1 : 0);
            CHECK_EQ_INT((int)DSB(MZ_E + 0x1Fu), i < 4u ? 3 : 4);
            CHECK_EQ_INT((int)DSB(MZ_E + 0x1Eu), i < 4u ? 4 : 6);
        }
    }

    /* B: 0x4C784 directly, side 0 eats. The spit actor (the actor free
     * list's head) spawns from 0xC976C at the ball's x/height with y = bd -
     * 0x100 and +0x28 bit 0x4000 (side 0's fighter faces right), on 0xEF65A
     * at 3.0; the shadow and the ball die; DS_0010889D (side 1) scores; B2
     * turns on 0xEF680 (DS_00108884 0x2001 right of side 1's record x
     * 0x2000); DS_001088AC = 0x69, DS_00108898 = 2; "BALL EATEN!" centred on
     * row 6 (the cursor kept); the new ball F1: descriptor 4 (side 1's
     * range DS_00108860[1] = 0 draws 0; side 0's 100 would give 2 with seed
     * 0x101), +0x10 = 0x29CDC(0, DS_001077A8[0]'s character 2), at
     * 0x2BE4C(B1, 0x4000 - 0x2580) = -0xF80 (side 0 is left of side 1),
     * target 0x4000 + 0x1740, owned by the ball's side 1, type 1, +0x1C
     * 0x20, and DS_00108864. */
    MZ_BALL(0x8Du);
    mz_free(0);
    DSD(FIGHT_RECS + 0x100u + 0x18u) = 0x2000u;
    DSD(DS_00108884) = 0x2001u;
    DSW(DS_00104B00) = 0x21u;               /* 0x49388's per-side range */
    DSD(MZ_PS(1) + 4u) = 0x3000u;
    DSD(MZ_PS(2) + 4u) = 0x5000u;
    DSW(DS_00108860) = 100; DSW(DS_00108860 + 2u) = 0;
    DSD(DS_001077A8) = MZ_S1; DSB(MZ_S1 + 0x7Au) = 2; DSB(DS_00105B34) = 0;
    DSD(0x000BB4C0u + 0x10u) = 0x12345678u;
    DSD(0x000C95D4u + 16u) = MZ_ST(12);     /* the new ball's si 4 stream */
    DSB(FIGHT_RECS + 0x100u + 0x51u) = 1;   /* side 1's record marked */
    rng_seed(0x101u);
    spit = DSD(DS_00105B3C);
    fight_4c784(0u);
    CHECK_EQ_INT((int)DSD(spit + 0x08u), 0x000EF65A);
    CHECK_EQ_INT((int)DSD(spit + 0x24u), 0x40400000);
    CHECK_EQ_INT((int)DSD(spit + 0x18u), 0x1000);
    CHECK_EQ_INT((int)DSD(spit + 0x1Cu), 0x100);
    CHECK_EQ_INT((int)DSW(spit + 0x32u), (int)(bd - 0x100u));
    CHECK_EQ_INT((int)(DSW(spit + 0x28u) & 0x4000u), 0x4000);
    CHECK_EQ_INT((int)DSB(spit + 0x4Au), 0);
    CHECK_EQ_INT((int)(DSB(MZ_SH + 0x28u) & 0x08u), 0x08);
    CHECK_EQ_INT((int)DSD(MZ_E + 0x14u), 0x7777);
    CHECK_EQ_INT((int)(DSB(MZ_R + 0x28u) & 0x08u), 0x08);
    CHECK_EQ_INT((int)DSD(MZ_E + 0x10u), 0);
    CHECK_EQ_INT((int)DSB(DS_0010889D), 1);
    CHECK_EQ_INT((int)DSB(DS_0010889C), 0);
    CHECK_EQ_INT((int)DSD(MZ_B2 + 0x08u), 0x000EF680);
    CHECK_EQ_INT((int)DSD(MZ_B2 + 0x24u), 0x40400000);
    CHECK_EQ_INT((int)DSW(DS_001088AC), 0x69);
    CHECK_EQ_INT((int)DSW(DS_00108898), 2);
    CHECK(mz_row_cells(6) > 0, "0x4C784 drew BALL EATEN! on row 6");
    CHECK(mz_row_ref(6, 0x56u), "row 6 is 0x56 centred in the class font");
    CHECK_EQ_INT((int)DSD(DS_00105F34), 0x55555555);
    CHECK_EQ_INT((int)DSD(DS_00108864), (int)MZ_F1);
    CHECK_EQ_INT((int)DSD(DS_0010884C), (int)MZ_F1);
    CHECK_EQ_INT((int)DSD(DS_001083C4), (int)DS_001083C4);
    a = DSD(MZ_F1 + 8u);
    CHECK(a != 0u, "0x4C784 spawned the new ball");
    if (a != 0u) {
        u32 nps = DSD(DS_001014EC) + (u32)DSW(a + 0x56u) * 0x20u;
        int right = (s32)DSD(nps + 4u) < 0x5740;
        CHECK_EQ_INT((int)DSB(a + 0x48u), 0x24);
        CHECK_EQ_INT((int)DSD(a + 0x18u), -0xF80);
        CHECK_EQ_INT((int)DSW(a + 0x32u), (int)bd);
        CHECK_EQ_INT((int)DSW(a + 0x28u), 0);
        CHECK_EQ_INT((int)DSD(a + 0x14u), (int)MZ_F1);
        CHECK(right, "the new ball's 0x2BE00 (0x1A80) is below its target");
        CHECK_EQ_INT((int)DSW(a + 0x34u), 0x0080);
        CHECK_EQ_INT((int)DSB(a + 0x29u), 0);
        CHECK_EQ_INT((int)DSD(a + 0x1Cu), 0);
        CHECK_EQ_INT((int)DSB(a + 0x4Au), 0);
        CHECK_EQ_INT((int)DSW(a + 0x2Cu), 0x0F80);  /* 0x496AC(0x400) */
        CHECK_EQ_INT((int)DSW(a + 0x2Eu), 0x0044);
        CHECK_EQ_INT((int)DSB(a + 0x4Eu), 1);
        CHECK_EQ_INT((int)DSD(a + 0x08u), (int)MZ_ST(12));
        CHECK_EQ_INT((int)DSD(a + 0x24u), 0x40400000);
    }
    CHECK_EQ_INT((int)DSD(0x000BB4C0u + 0x10u), (int)DSD(DS_000A8AF8 + 8u));
    CHECK_EQ_INT((int)DSD(MZ_F1 + 0x14u), 0x5740);
    CHECK_EQ_INT((int)DSB(MZ_F1 + 0x21u), 1);
    CHECK_EQ_INT((int)DSD(MZ_F1 + 0x0Cu), (int)MZ_S1);
    CHECK_EQ_INT((int)DSB(MZ_F1 + 0x1Eu), 1);
    CHECK_EQ_INT((int)DSW(MZ_F1 + 0x1Cu), 0x0020);
    CHECK_EQ_INT((int)DSB(MZ_F1 + 0x1Fu), 0);
    CHECK_EQ_INT((int)DSD(MZ_F1 + 0x10u), 0);
    CHECK_EQ_INT((int)DSW(MZ_B1 + 0x36u), 0x1234);
    /* Side 1 eats (right of side 0): side 0 scores, DS_00108898 = 1, B2 on
     * 0xEF6AC (DS_00108884 not right of side 0's record x), the fighter
     * facing left gives no 0x4000, the new ball 0x2580 to the right with
     * the target 0x4000 - 0x1740, owned by the ball's side 1. */
    MZ_BALL(0x8Du);
    mz_free(0);
    DSD(FIGHT_RECS + 0x18u) = 0x2000u;
    DSD(DS_00108884) = 0x2000u;
    DSW(FIGHT_RECS + 0x100u + 0x28u) = 0x4000u;
    DSW(DS_00104B00) = 0x21u;
    DSD(MZ_PS(1) + 4u) = 0x3000u;
    DSD(MZ_PS(2) + 4u) = 0x5000u;
    DSW(DS_00108860) = 0; DSW(DS_00108860 + 2u) = 0;
    /* 0x29CDC(1, DS_001077A8[1]'s character 3); [0] would name 5. */
    DSD(DS_001077A8) = MZ_S0; DSB(MZ_S0 + 0x7Au) = 5;
    DSD(DS_001077AC) = MZ_S1; DSB(MZ_S1 + 0x7Au) = 3;
    DSB(DS_00105B34) = 0; DSB(DS_00105B34 + 1u) = 0;
    DSD(0x000BB4C0u + 0x10u) = 0x12345678u;
    spit = DSD(DS_00105B3C);
    fight_4c784(1u);
    CHECK_EQ_INT((int)(DSW(spit + 0x28u) & 0x4000u), 0);
    CHECK_EQ_INT((int)DSB(DS_0010889C), 1);
    CHECK_EQ_INT((int)DSB(DS_0010889D), 0);
    CHECK_EQ_INT((int)DSW(DS_00108898), 1);
    CHECK_EQ_INT((int)DSD(MZ_B2 + 0x08u), 0x000EF6AC);
    a = DSD(MZ_F1 + 8u);
    CHECK(a != 0u, "0x4C784 spawned the new ball (side 1)");
    if (a != 0u) {
        /* Its 0x2BE00 (pset +4) is 0x6580, not below the target 0x28C0:
         * -0x80 and the hflip; no +0x51 mark on side 1's record here. */
        CHECK_EQ_INT((int)DSD(a + 0x18u), 0x4000 + 0x2580 - 0x2A00);
        CHECK_EQ_INT((int)DSD(DSD(DS_001014EC) + (u32)DSW(a + 0x56u) * 0x20u + 4u),
                     0x4000 + 0x2580);
        CHECK_EQ_INT((int)DSW(a + 0x34u), 0xFF80);
        CHECK_EQ_INT((int)DSB(a + 0x29u), 0x40);
        CHECK_EQ_INT((int)DSW(a + 0x2Eu), 0x0040);
        CHECK_EQ_INT((int)DSB(a + 0x4Eu), 0);
    }
    CHECK_EQ_INT((int)DSD(0x000BB4C0u + 0x10u), (int)DSD(DS_000A8AF8 + 12u));
    CHECK_EQ_INT((int)DSD(MZ_F1 + 0x14u), 0x4000 - 0x1740);
    CHECK_EQ_INT((int)DSB(MZ_F1 + 0x21u), 1);
    /* DS_00108884 equal to side 1's record x is not right of it (0xEF6AC);
     * equal fighter x (signed `jge`) spawns to the right with the target
     * base - 0x1740; a ball without a shadow. */
    MZ_BALL(0x8Du);
    mz_free(0);
    DSD(MZ_E + 0x10u) = 0;
    DSD(FIGHT_RECS + 0x100u + 0x18u) = 0x2000u;
    DSD(DS_00108884) = 0x2000u;
    DSD(MZ_PS(1) + 4u) = 0x5000u;
    DSD(MZ_PS(2) + 4u) = 0x5000u;
    fight_4c784(0u);
    CHECK_EQ_INT((int)DSD(MZ_B2 + 0x08u), 0x000EF6AC);
    CHECK_EQ_INT((int)DSD(MZ_F1 + 0x14u), 0x4000 - 0x1740);
    CHECK_EQ_INT((int)DSD(MZ_E + 0x14u), 0x7777);
    CHECK_EQ_INT((int)(DSB(MZ_R + 0x28u) & 0x08u), 0x08);
    CHECK_EQ_INT((int)(DSB(0x00000028u) & 0x08u), 0);
    /* An empty free list: the text, no new ball; DS_00108864 stays. */
    MZ_BALL(0x8Du);
    fight_4c784(0u);
    CHECK(mz_row_cells(6) > 0, "0x4C784 drew BALL EATEN! with no free entry");
    CHECK_EQ_INT((int)DSD(DS_00108864), (int)MZ_E);
    CHECK_EQ_INT((int)DSD(DS_0010884C), (int)MZ_E);
    /* The third point: the ball's +0x1F clears, 0x4CC0C ends the game
     * (DS_001088A0 = 0: B1/B2 +0x36 = -0x1A4, DS_001088A0 = 0x3C) and
     * DS_00108864 = 0, no new ball and no BALL EATEN! (row 6 holds 0x4CC0C's
     * centred string instead). DS_001088A0 running: no 0x4CC0C. */
    for (i = 0; i < 2u; i++) {
        MZ_BALL(0x8Du);
        mz_free(0);
        DSB(DS_0010889D) = 2;
        DSW(DS_001088A0) = (u16)(i == 0u ? 0u : 5u);
        fight_4c784(0u);
        CHECK_EQ_INT((int)DSB(DS_0010889D), 3);
        CHECK_EQ_INT((int)DSB(MZ_E + 0x1Fu), 0);
        CHECK_EQ_INT((int)DSD(DS_00108864), 0);
        CHECK_EQ_INT((int)DSD(DS_001083C4), (int)MZ_F1);
        CHECK_EQ_INT((int)DSW(MZ_B1 + 0x36u), i == 0u ? 0xFE5C : 0x1234);
        CHECK_EQ_INT((int)DSW(MZ_B2 + 0x36u), i == 0u ? 0xFE5C : 0x1234);
        CHECK_EQ_INT((int)DSW(DS_001088A0), i == 0u ? 0x3C : 5);
        if (i == 1u) CHECK_EQ_INT(mz_row_cells(6), 0);
    }

    /* C: 0x4CC0C directly. Each space string clears its planted cell, one
     * only it covers (text_width in mode 0x5002: 0x57 10 cells, 0x5C and 0x5D
     * 12, the others 4): 0x57 (8, 19), 0x58 (4, 0x13), 0x59 (7, 1), 0x5A (7, 0x26), 0x5B (1, 0x13),
     * 0x5C (8, 7), 0x5D (8, 0x16). Rows 6 and 9: equal scores "VOLLEYBALL
     * GAME" (14 glyphs) and "TIED" (4); DS_0010889C below DS_0010889D "RIGHT
     * PLAYER" (11), above "LEFT PLAYER" (10), each over "VOLLEYBALL GAME". */
    {
        static const s32 pr[7] = { 8, 4, 7, 7, 1, 8, 8 };
        static const s32 pc[7] = { 19, 0x13, 1, 0x26, 0x13, 7, 0x16 };
        static const u8 s9c[3] = { 2u, 1u, 2u };
        static const u8 s9d[3] = { 2u, 2u, 1u };
        static const int r6[3] = { 14, 11, 10 };
        static const int r9[3] = { 4, 14, 14 };
        u32 k;
        for (i = 0; i < 3u; i++) {
            MZ_BALL(0x8Du);
            for (k = 0; k < 7u; k++) ct_plant(pr[k], pc[k]);
            ct_plant(8, 20);                /* between 0x57's and 0x5D's runs */
            DSB(DS_0010889C) = s9c[i]; DSB(DS_0010889D) = s9d[i];
            fight_4cc0c();
            for (k = 0; k < 7u; k++) CHECK_EQ_INT((int)ct_cell(pr[k], pc[k]), 0);
            CHECK(ct_cell(8, 20) != 0u, "0x4CC0C leaves (8, 20)");
            CHECK_EQ_INT(mz_row_cells(6), r6[i]);
            CHECK_EQ_INT(mz_row_cells(9), r9[i]);
            CHECK(mz_row_ref(6, i == 0u ? 0x5Eu : (i == 1u ? 0x16u : 0x17u)),
                  "0x4CC0C's row 6 string");
            CHECK(mz_row_ref(9, i == 0u ? 0x5Fu : 0x5Eu), "0x4CC0C's row 9 string");
            CHECK_EQ_INT((int)DSW(MZ_B1 + 0x36u), 0xFE5C);
            CHECK_EQ_INT((int)DSW(MZ_B2 + 0x36u), 0xFE5C);
            CHECK_EQ_INT((int)DSW(DS_001088A0), 0x3C);
            CHECK_EQ_INT((int)DSD(DS_00105F34), 0x55555555);
        }
    }

    /* D: 0x2F510 is 0x2F4BC with the mode ORed with 2: the same glyphs as
     * 0x2F4BC's mode 0x4002, other glyphs than its 0x4000, the cursor kept. */
    {
        u32 g2[4], g0[4];
        MZ_BALL(0x8Du);
        text_cursor_hold_font2(3, 6, (const u8 *)"TIED", 0x4000u);
        CHECK_EQ_INT((int)DSD(DS_00105F34), 0x55555555);
        for (i = 0; i < 4u; i++) g2[i] = ct_sprite(6, 3 + (s32)i);
        mem_fill(DS_00105F38 + 6u * 0xACu, 0, 0xACu);
        text_cursor_hold(3, 6, (const u8 *)"TIED", 0x4000u);
        for (i = 0; i < 4u; i++) g0[i] = ct_sprite(6, 3 + (s32)i);
        CHECK(g2[0] != g0[0], "0x2F510 draws the class font, not mode 0x4000's");
        mem_fill(DS_00105F38 + 6u * 0xACu, 0, 0xACu);
        text_cursor_hold(3, 6, (const u8 *)"TIED", 0x4002u);
        for (i = 0; i < 4u; i++) CHECK_EQ_INT((int)ct_sprite(6, 3 + (s32)i), (int)g2[i]);
    }
#undef MZ_BALL
    mz_restore();
}

/* ---- the spawn's sound banks and the voice 0x4D (record §45-A) ---------- */

/* The INDEX entries DS_000BDB1C names for characters 0..6 (the sd banks). */
static const u8 sb_bank[7] = { 60, 54, 68, 36, 64, 42, 48 };

static int sb_loaded(u32 e)
{
    return (DSD(DSD(DS_001014E0) + e * 20u + 12u) & 0x20000000u) != 0u;
}

static void sb_unload(u32 e)
{
    DSD(DSD(DS_001014E0) + e * 20u + 12u) &= ~0x20000000u;
}

static int sb_hook_n;
static void sb_hook(void) { sb_hook_n++; }

/* 0x33C78's tail (0x33E48..0x33EA6): with the DIG driver set and samples not
 * paused, the spawn reads the character's sound bank DS_000BDB1C[ch] and
 * s16sound (0x287B2F5, entry 5); with either gate closed it reads neither.
 * And 0x1543C's voice 0x4D: s16cobsd (36) and s16spisd (64) read on its first
 * call, nothing without the DIG driver. Every scenario starts from the same
 * snapshot (the data object, the INDEX table, both pools, the DAC and the
 * aperture), restored at the end. */
static void check_spawn_sound(void)
{
    static u8 sd[0x8B0D0], si[256u * 20u], spa[0x4880], spb[0xEBA0];
    static u8 sap[320u * 200u], sdac[256][3];
    const u32 idx = DSD(DS_001014E0), nidx = res_count() * 20u;
    const u32 pa = DSD(DS_001014EC), pb = DSD(DS_001014F4);
    u32 ch, k;

    CHECK(nidx <= sizeof si, "the INDEX table fits the snapshot");
    tf_snap(sd, DATA_BASE, 0x8B0D0u);
    tf_snap(si, idx, nidx);
    tf_snap(spa, pa, 0x4880u);
    tf_snap(spb, pb, 0xEBA0u);
    memcpy(sap, gfx_aperture(), sizeof sap);
    memcpy(sdac, gfx_dac, sizeof sdac);

    /* A: each character 0..6 on side 0, gates open: its bank and s16sound
     * are read, no other character's bank. */
    for (ch = 0; ch < 7u; ch++) {
        tf_put(sd, DATA_BASE, 0x8B0D0u);
        tf_put(si, idx, nidx);
        actors_reset();
        for (k = 0; k < 7u; k++) sb_unload(sb_bank[k]);
        sb_unload(5u);
        DSB(DS_0010816A) = (u8)ch;
        DSD(DS_001028C8) = 1u;
        DSB(DS_001028DB) = 0;
        fighter_spawn(0u);
        CHECK_EQ_INT((int)DSB(DS_001077B0 + 0x7Au), (int)ch);
        for (k = 0; k < 7u; k++)
            CHECK_EQ_INT(sb_loaded(sb_bank[k]), k == ch ? 1 : 0);
        CHECK_EQ_INT(sb_loaded(5u), 1);
    }
    /* B: no DIG driver (0x1CEBC's first test), then paused samples (its
     * second): neither bank is read. */
    for (k = 0; k < 2u; k++) {
        tf_put(sd, DATA_BASE, 0x8B0D0u);
        tf_put(si, idx, nidx);
        actors_reset();
        sb_unload(36u);
        sb_unload(5u);
        DSB(DS_0010816A) = 3u;
        DSD(DS_001028C8) = (k == 0u) ? 0u : 1u;
        DSB(DS_001028DB) = (k == 0u) ? 0u : 1u;
        fighter_spawn(0u);
        CHECK_EQ_INT(sb_loaded(36u), 0);
        CHECK_EQ_INT(sb_loaded(5u), 0);
    }

    /* C: 0x1543C's voice 0x4D on a pool record: the two banks, each read
     * once (two loader screens); a second call reads nothing. */
    tf_put(sd, DATA_BASE, 0x8B0D0u);
    tf_put(si, idx, nidx);
    actors_reset();
    {
        typedef void (*anim_fn)(u32 rec, u32 arg);
        anim_fn f154 = (anim_fn)(void *)fn_resolve(0x1543Cu);
        u32 r = actor_alloc(0);
        CHECK(f154 != NULL && r != 0u, "0x1543C and a pool record");
        if (f154 != NULL && r != 0u) {
            DSW(r + 0x56u) = (u16)actor_index(r);
            sb_unload(36u);
            sb_unload(64u);
            DSD(DS_001028C8) = 1u;
            DSB(DS_001028DB) = 0;
            for (k = 0; k < 4u; k++) DSD(DS_0010286C + k * 0x18u) = 0;
            sb_hook_n = 0;
            res_set_screen_hook(sb_hook);
            f154(r, 0u);
            CHECK_EQ_INT(sb_loaded(36u), 1);
            CHECK_EQ_INT(sb_loaded(64u), 1);
            CHECK_EQ_INT(sb_hook_n, 2);
            f154(r, 0u);
            CHECK_EQ_INT(sb_hook_n, 2);
            sb_unload(36u);
            sb_unload(64u);
            DSD(DS_001028C8) = 0;
            f154(r, 0u);
            CHECK_EQ_INT(sb_loaded(36u), 0);
            CHECK_EQ_INT(sb_loaded(64u), 0);
            CHECK_EQ_INT(sb_hook_n, 2);
            res_set_screen_hook(NULL);
        }
    }

    actors_reset();
    memcpy(gfx_dac, sdac, sizeof sdac);
    memcpy(gfx_aperture(), sap, sizeof sap);
    tf_put(spb, pb, 0xEBA0u);
    tf_put(spa, pa, 0x4880u);
    tf_put(si, idx, nidx);
    tf_put(sd, DATA_BASE, 0x8B0D0u);
}

/* ---- character 1's entrance 0x24568, 0x246D4 and 0x3BCE0 (record §46-C) - */

#define CE_10810D 0x0010810Du   /* the side 0x24568 places against */
#define CE_104B03 0x00104B03u   /* its +0x41 bit-0 mask byte */
#define CE_DECOY  (FIGHT_RECS + 0x6400u)  /* a scratch record, saved/restored */

static u8 s_ce_data[0x8B0D0], s_ce_idx[256u * 20u], s_ce_pa[0x4880], s_ce_pb[0xEBA0];
static u8 s_ce_decoy[ACTOR_REC_SIZE];

/* Back to the snapshot, a fresh actor pool, both sides character `ch`, the
 * audio gate closed; then the side `sel` spawned with its record at x0 and
 * its pset word's bit 15 (0x1A570 fails when set), DS_0010810D = sel, the
 * bound DS_000BE018 = w and DS_00104B03 = b03. Returns sel's record. */
static u32 ce_seed(u32 ch, u32 sel, s32 x0, int bit15, u32 w, u8 b03)
{
    const u32 idx = DSD(DS_001014E0), nidx = res_count() * 20u;
    u32 rec, ps;
    tf_put(s_ce_data, DATA_BASE, 0x8B0D0u);
    tf_put(s_ce_idx, idx, nidx);
    actors_reset();
    DSB(DS_0010816A) = (u8)ch;
    DSB(DS_0010816A + 1u) = (u8)ch;
    DSD(DS_001028C8) = 0;
    fighter_spawn(sel);
    rec = DSD(DS_001077B0 + sel * 0x94u);
    DSD(rec + 0x18u) = (u32)x0;
    ps = DSD(DS_001014EC) + (u32)DSW(rec + 0x56u) * 0x20u;
    DSW(ps) = (u16)(bit15 ? 0x8123u : 0x0123u);
    DSB(CE_10810D) = (u8)sel;
    DSD(DS_000BE018) = w;
    DSB(CE_104B03) = b03;
    return rec;
}

/* 0x24568 through the table dword 0xA862C, 0x246D4 through its registration
 * and through the 0xD500 word of the real stream at 0xE4542, 0x3BCE0 directly.
 * The data object, the INDEX table, both pools, the DAC and the aperture are
 * snapshot first and restored at the end. */
static void check_char1_entry(void)
{
    typedef void (*side_fn)(u32 side);
    typedef void (*anim_fn)(u32 rec, u32 arg);
    static u8 sap[320u * 200u], sdac[256][3];
    const u32 idx = DSD(DS_001014E0), nidx = res_count() * 20u;
    const u32 pa = DSD(DS_001014EC), pb = DSD(DS_001014F4);
    side_fn f68;
    anim_fn fd4;
    u32 i;

    CHECK(nidx <= sizeof s_ce_idx, "the INDEX table fits the snapshot");
    tf_snap(s_ce_data, DATA_BASE, 0x8B0D0u);
    tf_snap(s_ce_idx, idx, nidx);
    tf_snap(s_ce_pa, pa, 0x4880u);
    tf_snap(s_ce_pb, pb, 0xEBA0u);
    tf_snap(s_ce_decoy, CE_DECOY, ACTOR_REC_SIZE);
    memcpy(sap, gfx_aperture(), sizeof sap);
    memcpy(sdac, gfx_dac, sizeof sdac);

    /* The data: 0xA8628's entry 1, the entrance stream's first word (a
     * sprite), its 0xD500 word and target, and the stance streams. */
    CHECK_EQ_INT((int)DSD(0x000A862Cu), 0x00024568);
    CHECK_EQ_INT((int)DSW(0x000E453Au), 0x12A2);
    CHECK_EQ_INT((int)DSW(0x000E4542u), 0xD500);
    CHECK_EQ_INT((int)DSD(0x000E4544u), 0x000246D4);
    CHECK_EQ_INT((int)DSD(0x000C8B30u + 4u), 0x000E3AF0);
    CHECK_EQ_INT((int)DSD(0x000C8B30u + 12u), 0x000D2274);
    f68 = (side_fn)(void *)fn_resolve(DSD(0x000A862Cu));
    fd4 = (anim_fn)(void *)fn_resolve(0x246D4u);
    CHECK(f68 == (side_fn)fighter_24568, "0xA862C resolves to 0x24568, fn(side)");
    CHECK(fd4 != NULL, "0x246D4 is registered");
    CHECK(fn_resolve(0x246D4u) != (void (*)(void))fighter_246d4,
          "0x246D4 is registered through the (rec, arg) wrapper");

    /* A: 0x24568. Each row: sel, the entering side, 0x1A570's bit 15, x0,
     * the bound, the mask byte; the spawn's x and a5, and +0x41 bit 0. The
     * left test is x0 - 0x5000 >= -bound, the right x0 + 0x5000 <= bound,
     * both signed; bit 15 clear tries the left first. The mask is 0x80 for
     * sel 0, 0x40 for 1, whichever side enters (the last two rows enter on
     * sel's own side, whose old record 0x24568 then places against). */
    {
        static const struct {
            u32 sel, ent; int b15; s32 x0; u32 w; u8 b03; s32 x; u32 a5; u8 blink;
        } row[] = {
            { 0u, 1u, 0,  0x2000, 0x7C00u, 0x80u, -0x3000, 0x4000u, 1u }, /* left */
            { 0u, 1u, 0, -0x3000, 0x7C00u, 0x40u,  0x2000, 0u,      0u }, /* left fails */
            { 0u, 1u, 0, -0x3000, 0x8000u, 0x7Fu, -0x8000, 0x4000u, 0u }, /* the bound read */
            { 1u, 0u, 1,  0x1000, 0x7C00u, 0x40u,  0x6000, 0u,      1u }, /* right */
            { 1u, 0u, 1,  0x3000, 0x7C00u, 0xBFu, -0x2000, 0x4000u, 0u }, /* right fails */
            { 0u, 1u, 0, -0x2C00, 0x7C00u, 0x00u, -0x7C00, 0x4000u, 0u }, /* left equal */
            { 1u, 0u, 1,  0x2C00, 0x7C00u, 0xC0u,  0x7C00, 0u,      1u }, /* right equal */
            { 0u, 1u, 0,  0x6000, 0x7C00u, 0xFFu,  0x1000, 0x4000u, 1u }, /* signed left */
            { 1u, 0u, 1, -0x6000, 0x7C00u, 0x00u, -0x1000, 0u,      0u }, /* signed right */
            { 1u, 1u, 0,  0x2000, 0x7C00u, 0x40u, -0x3000, 0x4000u, 1u }, /* sel enters */
            { 0u, 0u, 1,  0x1000, 0x7C00u, 0x80u,  0x6000, 0u,      1u }, /* sel enters */
        };
        for (i = 0; f68 != NULL && i < sizeof row / sizeof row[0]; i++) {
            u32 side = row[i].ent;
            u32 slot = DS_001077B0 + side * 0x94u;
            u32 other = DS_001077B0 + row[i].sel * 0x94u;
            int apart = row[i].sel != side;
            u32 rec, p, orec;
            orec = ce_seed(1u, row[i].sel, row[i].x0, row[i].b15, row[i].w, row[i].b03);
            /* The entering side's old record (when it is not sel's): a decoy
             * on pset 500 whose bit 15 is the opposite of sel's, so 0x1A570
             * on the wrong side fails. */
            if (apart) {
                mem_fill(CE_DECOY, 0, ACTOR_REC_SIZE);
                DSW(CE_DECOY + 0x56u) = 500u;
                DSW(DSD(DS_001014EC) + 500u * 0x20u) = (u16)(row[i].b15 ? 0x0123u : 0x8123u);
                DSD(slot) = CE_DECOY;
                DSB(other + 0x52u) = 0x52u;
            }
            DSB(slot + 0x63u) = 0x63u;
            DSW(slot + 0x74u) = 0x7474u;
            /* The spawn's layer word[0xBD898] (0x400 in the image), another
             * value on one row so a literal 0x400 fails too. */
            if (i == 3u) DSW(DS_000BD898) = 0x0500u;
            f68(side);
            rec = DSD(slot);
            CHECK(rec != 0u && rec != orec, "0x24568 spawned the side");
            if (rec == 0u) continue;
            CHECK_EQ_INT((int)DSD(DS_001077A8 + side * 4u), (int)slot);
            CHECK_EQ_INT((int)DSB(slot + 0x7Au), 1);
            CHECK_EQ_INT((int)DSB(rec + 0x51u), (int)side);
            CHECK_EQ_INT((int)DSD(rec + 0x18u), (int)row[i].x);
            CHECK_EQ_INT((int)DSW(rec + 0x32u), (int)DSW(DS_000BD898));
            CHECK_EQ_INT((int)(DSW(rec + 0x28u) & 0x4000u), (int)row[i].a5);
            CHECK_EQ_INT((int)DSD(rec + 0x08u), 0x000E453A);
            CHECK_EQ_INT((int)DSD(rec + 0x24u), 0x40800000);
            CHECK_EQ_INT((int)DSD(rec + 0x20u), 0x40800000);
            CHECK_EQ_INT((int)DSW(slot + 0x74u), 0x0309);
            p = DSD(slot + 0x08u);
            CHECK(p != 0u, "0x24568 fired 0x2372C's projectile");
            if (p != 0u) {
                CHECK_EQ_INT((int)DSB(p + 0x48u), 8);
                CHECK_EQ_INT((int)DSD(p + 0x14u), (int)slot);
                CHECK_EQ_INT((int)DSW(p + 0x34u), row[i].a5 ? 0x0113 : 0xFEED);
                CHECK_EQ_INT((int)DSD(p + 0x18u),
                             (int)(row[i].x + (row[i].a5 ? 0xC00 : -0xC00)));
            }
            /* The spawn leaves +0x40 = 0x80000000 (0x33D91's 0x80008000,
             * then 0x33E2F's +0x41 &= 0x7F). */
            CHECK_EQ_INT((int)DSB(slot + 0x41u), 0x10 | row[i].blink);
            CHECK_EQ_INT((int)DSB(slot + 0x52u), 9);
            CHECK_EQ_INT((int)DSB(slot + 0x53u), 3);
            CHECK_EQ_INT((int)DSB(slot + 0x64u), 0x2A);
            CHECK_EQ_INT((int)DSD(slot + 0x40u),
                         (int)(0x80081000u | ((u32)row[i].blink << 8)));
            CHECK_EQ_INT((int)DSB(slot + 0x63u), 1);
            if (apart) {
                CHECK_EQ_INT((int)DSB(other + 0x52u), 0x52);
                CHECK_EQ_INT((int)DSD(other), (int)orec);
            }
        }
    }

    /* B: 0x3BCE0 on side 1, characters 1 and 3: DS_00107D40[1] = 0xBEF64 +
     * char * 6, the stance stream at 2.0, state 3/4/2, DS_001078F8[1] = 1;
     * +0x4E from DS_001088E0[1]'s high byte: bit 4 (over bit 5) 1, bit 5
     * alone 0xFFFF, neither 0 (the low byte and the other bits ignored).
     * Side 0's words are kept. */
    {
        static const u16 in[6] = { 0x1000u, 0x3000u, 0x2000u, 0x0000u, 0x00FFu, 0xCF00u };
        static const u16 out[6] = { 1u, 1u, 0xFFFFu, 0u, 0u, 0u };
        u32 slot = DS_001077B0 + 0x94u;
        for (i = 0; i < 12u; i++) {
            u32 ch = i < 6u ? 1u : 3u, k = i % 6u, rec;
            (void)ce_seed(ch, 1u, 0x1000, 0, 0x7C00u, 0u);
            rec = DSD(slot);
            DSD(DS_00107D40) = 0x40404040u; DSD(DS_00107D40 + 4u) = 0x41414141u;
            DSB(DS_001078F8) = 0x55u; DSB(DS_001078F8 + 1u) = 0x56u;
            DSB(slot + 0x52u) = 0x52u; DSB(slot + 0x53u) = 0x53u; DSB(slot + 0x54u) = 0x54u;
            DSW(slot + 0x4Eu) = 0x4E4Eu;
            DSW(DS_001077B0 + 0x4Eu) = 0x4D4Du;
            DSW(DS_001088E0) = 0x3000u;
            DSW(DS_001088E0 + 2u) = in[k];
            fighter_3bce0(1u);
            CHECK_EQ_INT((int)DSD(DS_00107D40 + 4u), (int)(0x000BEF64u + ch * 6u));
            CHECK_EQ_INT((int)DSD(DS_00107D40), 0x40404040);
            CHECK_EQ_INT((int)DSD(rec + 0x08u), ch == 1u ? 0x000E3AF0 : 0x000D2274);
            CHECK_EQ_INT((int)DSD(rec + 0x24u), 0x40000000);
            CHECK_EQ_INT((int)DSB(slot + 0x52u), 3);
            CHECK_EQ_INT((int)DSB(slot + 0x53u), 4);
            CHECK_EQ_INT((int)DSB(slot + 0x54u), 2);
            CHECK_EQ_INT((int)DSB(DS_001078F8 + 1u), 1);
            CHECK_EQ_INT((int)DSB(DS_001078F8), 0x55);
            CHECK_EQ_INT((int)DSW(slot + 0x4Eu), (int)out[k]);
            CHECK_EQ_INT((int)DSW(DS_001077B0 + 0x4Eu), 0x4D4D);
        }
    }

    /* C: 0x246D4 on side 0's record, through its registration (bit 15 clear
     * and set) and through the real stream word 0xE4542 (bit 15 clear):
     * DS_001088E0[0] = 0xA000 (then +0x4E = 0xFFFF) or 0x9000 (+0x4E = 1),
     * 0x3BCE0's state 3/4/2, +0x74 = 0, +0x40 without bits 12 and 19,
     * DS_000F0AFE = 1 and DS_000F0AFC = 0x100. Side 1's words are kept. */
    for (i = 0; i < 3u; i++) {
        u32 slot = DS_001077B0, rec;
        rec = ce_seed(1u, 0u, 0x1000, i == 1u, 0x7C00u, 0u);
        DSW(DS_001088E0) = 0x5555u; DSW(DS_001088E0 + 2u) = 0x5656u;
        DSB(slot + 0x52u) = 0x52u; DSB(slot + 0x53u) = 0x53u; DSB(slot + 0x54u) = 0x54u;
        DSW(slot + 0x74u) = 0x7474u;
        DSD(slot + 0x40u) = 0xFFFFFFFFu;
        DSW(slot + 0x4Eu) = 0x4E4Eu;
        DSB(DS_000F0AFE) = 0x55u;
        DSW(DS_000F0AFC) = 0x5555u;
        if (i < 2u) { if (fd4 != NULL) fd4(rec, 0x1234u); }
        else actors_anim_begin(rec, 0x000E4542u, 0x3F800000u);
        CHECK_EQ_INT((int)DSW(DS_001088E0), i == 1u ? 0x9000 : 0xA000);
        CHECK_EQ_INT((int)DSW(DS_001088E0 + 2u), 0x5656);
        CHECK_EQ_INT((int)DSW(slot + 0x4Eu), i == 1u ? 1 : 0xFFFF);
        CHECK_EQ_INT((int)DSD(DS_00107D40), (int)(0x000BEF64u + 6u));
        CHECK_EQ_INT((int)DSB(slot + 0x52u), 3);
        CHECK_EQ_INT((int)DSB(slot + 0x53u), 4);
        CHECK_EQ_INT((int)DSB(slot + 0x54u), 2);
        CHECK_EQ_INT((int)DSW(slot + 0x74u), 0);
        CHECK_EQ_INT((int)DSD(slot + 0x40u), (int)0xFFF7EFFFu);
        CHECK_EQ_INT((int)DSB(DS_000F0AFE), 1);
        CHECK_EQ_INT((int)DSW(DS_000F0AFC), 0x0100);
    }

    actors_reset();
    memcpy(gfx_dac, sdac, sizeof sdac);
    memcpy(gfx_aperture(), sap, sizeof sap);
    tf_put(s_ce_decoy, CE_DECOY, ACTOR_REC_SIZE);
    tf_put(s_ce_pb, pb, 0xEBA0u);
    tf_put(s_ce_pa, pa, 0x4880u);
    tf_put(s_ce_idx, idx, nidx);
    tf_put(s_ce_data, DATA_BASE, 0x8B0D0u);
}
#undef CE_10810D
#undef CE_104B03
#undef CE_DECOY

/* ---- §46-D: update-table entries 1 (0x48F98) and 10 (0x28F08), 0x37B54 --- */

#define R46_NODE(i) (DS_001082E8 + (u32)(i) * 0x10u)    /* the type-0x2D nodes */
#define R46_REC(i)  (Q42_POOL + (u32)(i) * ACTOR_REC_SIZE)
#define R46_108396  0x00108396u
#define R46_108397  0x00108397u
#define R46_DESC19  0x000A89ACu

/* A reference state: the data object, the scratch records and psets, and
 * mem[0..0xFF] (where a write through a zero slot would land). */
static u8 s_r46_data[0x10B0D0u - 0x80000u];
static u8 s_r46_recs[Q42_RECS];
static u8 s_r46_acts[Q42_ACTS];
static u8 s_r46_low[0x100];

static void r46_snap(void)
{
    tf_snap(s_r46_data, 0x80000u, sizeof s_r46_data);
    tf_snap(s_r46_recs, FIGHT_RECS, Q42_RECS);
    tf_snap(s_r46_acts, FIGHT_ACTORS, Q42_ACTS);
    tf_snap(s_r46_low, 0u, sizeof s_r46_low);
}

/* 1 when the live state equals the reference state. */
static int r46_same(void)
{
    return memcmp(mem + 0x80000u, s_r46_data, sizeof s_r46_data) == 0
        && memcmp(mem + FIGHT_RECS, s_r46_recs, sizeof s_r46_recs) == 0
        && memcmp(mem + FIGHT_ACTORS, s_r46_acts, sizeof s_r46_acts) == 0
        && memcmp(mem, s_r46_low, sizeof s_r46_low) == 0;
}

/* The data object as q42_save() found it (so every run starts from the same
 * render list and counters), then records 0..7 of Q42_POOL zeroed with their
 * +0x56 index and sentinels in
 * +0x08/+0x24/+0x53; the type-0x2D in-use list empty; slots 0/1 on records 4
 * and 5 (x 0x90000 and 0x20000, +0x51 0 and 1, +0x30 0x0140ABCD and
 * 0x0100ABCD), DS_001078FD = 1, DS_00104AD4 = 0, 0x108396 = 5, 0x108397 =
 * 0x7F; the actor free list holds record 6 alone and the type-0x19 free list
 * the node 0x104780 alone. */
static void r46_seed(void)
{
    u32 s0 = DS_001077B0, s1 = DS_001077B0 + 0x94u, i;
    tf_put(s_q42_data, 0x80000u, sizeof s_q42_data);
    q42_lists();
    for (i = 0; i < 8u; i++) {
        u32 rec = R46_REC(i);
        mem_fill(rec, 0, ACTOR_REC_SIZE);
        DSW(rec + 0x56u) = (u16)i;
        DSD(rec + 0x08u) = 0x00ABCDEFu;
        DSD(rec + 0x24u) = 0x11111111u;
        DSB(rec + 0x53u) = 0x77u;
    }
    DSD(DS_00108368) = DS_00108368;
    DSD(DS_0010836C) = DS_00108368;
    DSD(DS_001077A8) = s0;
    DSD(DS_001077A8 + 4u) = s1;
    DSD(s0) = R46_REC(4);
    DSD(s1) = R46_REC(5);
    DSD(s0 + 0x2Cu) = 0x2C2C0000u;
    DSD(s1 + 0x2Cu) = 0x00031234u;
    DSB(R46_REC(5) + 0x51u) = 1u;
    DSD(R46_REC(4) + 0x18u) = 0x00090000u;
    DSD(R46_REC(5) + 0x18u) = 0x00020000u;
    DSD(R46_REC(4) + 0x30u) = 0x0140ABCDu;
    DSD(R46_REC(5) + 0x30u) = 0x0100ABCDu;
    DSB(DS_001078FD) = 1u;
    DSD(DS_00104AD4) = 0u;
    DSB(R46_108396) = 0x05u;
    DSB(R46_108397) = 0x7Fu;
    DSD(DS_000F0AF0) = 0u;
    q42_one_actor(R46_REC(6));
    q42_append(DS_00104888, Q42_NODE(0));
    mem_fill(0u, 0, 0x100u);
}

/* Type-0x2D node i on record i, phase `ph`, x `x`, word +0x34 `vx`. */
static u32 r46_node(u32 i, u8 ph, u32 x, u16 vx)
{
    u32 node = R46_NODE(i), rec = R46_REC(i);
    DSD(rec + 0x18u) = x;
    DSW(rec + 0x34u) = vx;
    DSD(node + 8u) = rec;
    DSB(node + 0x0Cu) = ph;
    q42_append(DS_00108368, node);
    return rec;
}

/* The first seed from 1 whose rng(r1) draw sits at the gate's edge and whose
 * next rng(2) draw is (non-zero == nz2): with z1 a 0 that rng(r1 + 1) would
 * not give, else a 1 that rng(r1 - 1) would make 0. A gate on any other
 * range then flips the outcome of one of the two. */
static u32 r46_rng_seed(u32 r1, int z1, int nz2)
{
    u32 s;
    for (s = 1u; s < 0x100000u; s++) {
        u32 a, b, lo, hi;
        rng_seed(s);
        lo = rng_next(r1 - 1u);
        rng_seed(s);
        hi = rng_next(r1 + 1u);
        rng_seed(s);
        a = rng_next(r1);
        b = rng_next(2u);
        if (z1 ? (a != 0u || hi == 0u) : (a != 1u || lo != 0u)) continue;
        if ((b != 0u) == (nz2 != 0)) return s;
    }
    return 0u;
}

/* Phase-1 node 0 within reach of record 5 (x 0x1F000, height 0x180). */
static void r46_d_setup(void)
{
    u32 r0;
    r46_seed();
    r0 = r46_node(0, 1u, 0x0001F000u, 0x0040u);
    DSD(r0 + 0x30u) = 0x0180ABCDu;
}

/* The expected spawn: from `seed`, the gate draw rng(r1) and the a5 draw
 * rng(2), then 0x2AE14(0xA89AC, a2, a3, a4, a5). */
static void r46_mirror(u32 seed, u32 r1, u32 a2, u32 a3, u32 a4, u32 a5)
{
    rng_seed(seed);
    (void)rng_next(r1);
    (void)rng_next(2u);
    (void)actor_spawn((const u32 *)(mem + R46_DESC19), a2, a3, a4, a5);
}

/* Record §46-D. 0x37B54 (EAX = rec): the slot DS_001077A8[(rec+0x51) ^ 1]'s
 * record +0x53 = 1, skipped for a zero slot; also through its registration
 * as the 0xD000 target (rec, arg). 0x48F98 (update-table entry 1, through
 * its registration) over the type-0x2D list 0x108368 against slot
 * DS_001078FD's record: phase 0 within 0x1000 starts 0xEDD20 at 2.0, halves
 * +0x34 toward 0, bumps 0x108397 and the phase, and the first such node
 * (0x108396 bit 7 clear) calls 0x37B54 on slot DS_00104AD4's record and sets
 * the bit; phase 1 beyond 0x1000 starts 0xEDCEA at 2.0, doubles +0x34, drops
 * 0x108397 and bumps the phase; within 0x1000 it spawns 0xA89AC on
 * rng(0x14) == 0 at (x, +0x30 SAR 16, 0) with a5 = rng(2) ? 0x4000 : 0;
 * phases above 1 hold. 0x28F08 (entry 10) spawns 0xA89AC on rng(6) == 0 at
 * (slot +0x2C, its record's +0x30 SAR 16, 0xC00) from the slot
 * DS_001077A8[DS_00104AD4 ^ 1], a5 as above. A spawn is checked against the
 * port's 0x2AE14 run from the same state with the raw's arguments. */
static void check_update_48f98(void)
{
    typedef void (*proc_fn)(void);
    typedef void (*anim_fn)(u32 rec, u32 arg);
    proc_fn p1, p10;
    anim_fn a37;
    u32 r0, r1, r2, r3, r4, near_p, far_p, seed, i;
    u32 f0 = R46_REC(4), f1 = R46_REC(5), sp = R46_REC(6);
    u8 low[0x100];

    q42_save();
    tf_snap(low, 0u, sizeof low);          /* r46_seed zeroes mem[0..0xFF] */
    p1 = (proc_fn)fn_resolve(0x48F98u);
    p10 = (proc_fn)fn_resolve(0x28F08u);
    a37 = (anim_fn)(void *)fn_resolve(0x37B54u);
    CHECK(p1 != NULL, "0x48F98 (update-table entry 1) is registered");
    CHECK(p10 != NULL, "0x28F08 (update-table entry 10) is registered");
    CHECK(a37 != NULL, "0x37B54 (the 0xD000 target) is registered");
    CHECK_EQ_INT((int)DSD(0x000A8644u + 1u * 4u), 0x00048F98);
    CHECK_EQ_INT((int)DSD(0x000A8644u + 10u * 4u), 0x00028F08);
    CHECK_EQ_INT((int)DSD(0x000E8564u), 0x00037B54);
    CHECK_EQ_INT((int)DSD(0x000EDAFCu), 0x00037B54);
    CHECK_EQ_INT((int)DSW(0x000E8562u), 0xD000);
    CHECK_EQ_INT((int)DSW(0x000EDAFAu), 0xD000);
    CHECK_EQ_INT((int)DSB(R46_DESC19 + 4u), 0x19);

    /* The cursor 0x2BC30 leaves on each stream (a scratch record), so a
     * start is told apart from the other stream and from no start. */
    r46_seed();
    actors_anim_begin(R46_REC(7), 0x000EDD20u, 0x40000000u);
    near_p = DSD(R46_REC(7) + 0x08u);
    actors_anim_begin(R46_REC(7), 0x000EDCEAu, 0x40000000u);
    far_p = DSD(R46_REC(7) + 0x08u);
    CHECK(near_p != far_p, "0xEDD20 and 0xEDCEA leave different cursors");

    /* A: 0x37B54 on record 4 (+0x51 0): slot 1's record 5 takes +0x53 = 1;
     * on record 5 (+0x51 1) slot 0's record 4; through the registration.
     * +0x52 is seeded non-zero, so only the byte index (`and eax,0xff`)
     * reaches the slot table. A zero slot writes nothing. */
    if (a37 != NULL) {
        r46_seed();
        DSB(f0 + 0x52u) = 1u;
        a37(f0, 0x1234u);
        CHECK_EQ_INT((int)DSB(f1 + 0x53u), 1);
        CHECK_EQ_INT((int)DSB(f0 + 0x53u), 0x77);
        r46_seed();
        DSB(f1 + 0x52u) = 0x80u;
        a37(f1, 0u);
        CHECK_EQ_INT((int)DSB(f0 + 0x53u), 1);
        CHECK_EQ_INT((int)DSB(f1 + 0x53u), 0x77);
        r46_seed();
        DSD(DS_001077A8 + 4u) = 0u;
        r46_snap();
        a37(f0, 0u);
        CHECK(r46_same(), "0x37B54 with a zero slot writes nothing");
    }

    /* B: phase 0. Record 5 (slot DS_001078FD = 1) is at x 0x20000. Node 0 at
     * +0x1000 (the edge, near; +0x34 -3 -> -1), node 1 at +0x1001 (far),
     * node 2 at -0x1000 (near; +0x34 5 -> 2), node 3 at the same x in phase
     * 2 (holds), node 4 near with +0x34 0x8000 -> 0xC000. The first near node
     * calls 0x37B54 on slot DS_00104AD4 = 0's record 4, so record 5 takes
     * +0x53 = 1 and record 4 keeps 0x77. */
    r46_seed();
    r0 = r46_node(0, 0u, 0x00021000u, 0xFFFDu);
    r1 = r46_node(1, 0u, 0x00021001u, 0x0040u);
    r2 = r46_node(2, 0u, 0x0001F000u, 0x0005u);
    r3 = r46_node(3, 2u, 0x00020000u, 0x0040u);
    r4 = r46_node(4, 0u, 0x00020000u, 0x8000u);
    if (p1 != NULL) p1();
    CHECK_EQ_INT((int)DSD(r0 + 0x08u), (int)near_p);
    CHECK_EQ_INT((int)DSD(r0 + 0x24u), 0x40000000);
    CHECK_EQ_INT((int)DSW(r0 + 0x34u), 0xFFFF);
    CHECK_EQ_INT((int)DSB(R46_NODE(0) + 0x0Cu), 1);
    CHECK_EQ_INT((int)DSD(r1 + 0x08u), 0x00ABCDEF);
    CHECK_EQ_INT((int)DSW(r1 + 0x34u), 0x0040);
    CHECK_EQ_INT((int)DSB(R46_NODE(1) + 0x0Cu), 0);
    CHECK_EQ_INT((int)DSD(r2 + 0x08u), (int)near_p);
    CHECK_EQ_INT((int)DSW(r2 + 0x34u), 0x0002);
    CHECK_EQ_INT((int)DSB(R46_NODE(2) + 0x0Cu), 1);
    CHECK_EQ_INT((int)DSD(r3 + 0x08u), 0x00ABCDEF);
    CHECK_EQ_INT((int)DSB(R46_NODE(3) + 0x0Cu), 2);
    CHECK_EQ_INT((int)DSD(r4 + 0x08u), (int)near_p);
    CHECK_EQ_INT((int)DSW(r4 + 0x34u), 0xC000);
    CHECK_EQ_INT((int)DSB(R46_NODE(4) + 0x0Cu), 1);
    CHECK_EQ_INT((int)DSB(R46_108397), 0x82);
    CHECK_EQ_INT((int)DSB(R46_108396), 0x85);
    CHECK_EQ_INT((int)DSB(f1 + 0x53u), 1);
    CHECK_EQ_INT((int)DSB(f0 + 0x53u), 0x77);
    CHECK_EQ_INT((int)DSD(DS_00105BCC), (int)DS_00105BCC);   /* no spawn */
    /* B2: 0x108396 bit 7 already set: no 0x37B54 (record 5 keeps 0x77). */
    r46_seed();
    DSB(R46_108396) = 0x80u;
    r0 = r46_node(0, 0u, 0x00020000u, 0x0010u);
    if (p1 != NULL) p1();
    CHECK_EQ_INT((int)DSD(r0 + 0x08u), (int)near_p);
    CHECK_EQ_INT((int)DSW(r0 + 0x34u), 0x0008);
    CHECK_EQ_INT((int)DSB(R46_108397), 0x80);
    CHECK_EQ_INT((int)DSB(R46_108396), 0x80);
    CHECK_EQ_INT((int)DSB(f1 + 0x53u), 0x77);
    CHECK_EQ_INT((int)DSB(f0 + 0x53u), 0x77);
    /* B3: DS_00104AD4 = 1: 0x37B54 on record 5, so record 4 takes it. */
    r46_seed();
    DSD(DS_00104AD4) = 1u;
    r0 = r46_node(0, 0u, 0x00020000u, 0x0010u);
    if (p1 != NULL) p1();
    CHECK_EQ_INT((int)DSB(f0 + 0x53u), 1);
    CHECK_EQ_INT((int)DSB(f1 + 0x53u), 0x77);

    /* C: DS_001078FD = 0 moves the watched record to record 4 (x 0x90000).
     * Phase 1 beyond 0x1000: node 0 at -0x1001 (+0x34 0x4001 -> 0x8002),
     * node 1 at +0x2000 (+0x34 0xFFF0 -> 0xFFE0) and node 7 at 0x20000
     * (record 5's x, so within reach only of the wrong record; +0x34 0x0100
     * -> 0x0200); node 2 in phase 3 holds. Node 3, phase 0 at 0x90000, is
     * within reach of record 4 only, so it starts 0xEDD20 and calls 0x37B54.
     * 0x108397: 2 - 3 + 1 = 0, and nothing draws. */
    r46_seed();
    DSB(DS_001078FD) = 0u;
    DSB(R46_108397) = 2u;
    r0 = r46_node(0, 1u, 0x0008EFFFu, 0x4001u);
    r1 = r46_node(1, 1u, 0x00092000u, 0xFFF0u);
    r2 = r46_node(2, 3u, 0x00090000u, 0x0040u);
    r3 = r46_node(3, 0u, 0x00090000u, 0x0010u);
    r4 = r46_node(7, 1u, 0x00020000u, 0x0100u);
    rng_seed(0x1234u);
    if (p1 != NULL) p1();
    CHECK_EQ_INT((int)DSD(r3 + 0x08u), (int)near_p);
    CHECK_EQ_INT((int)DSW(r3 + 0x34u), 0x0008);
    CHECK_EQ_INT((int)DSB(R46_NODE(3) + 0x0Cu), 1);
    CHECK_EQ_INT((int)DSD(r4 + 0x08u), (int)far_p);
    CHECK_EQ_INT((int)DSW(r4 + 0x34u), 0x0200);
    CHECK_EQ_INT((int)DSB(R46_NODE(7) + 0x0Cu), 2);
    CHECK_EQ_INT((int)DSB(f1 + 0x53u), 1);
    CHECK_EQ_INT((int)DSD(r0 + 0x08u), (int)far_p);
    CHECK_EQ_INT((int)DSD(r0 + 0x24u), 0x40000000);
    CHECK_EQ_INT((int)DSW(r0 + 0x34u), 0x8002);
    CHECK_EQ_INT((int)DSB(R46_NODE(0) + 0x0Cu), 2);
    CHECK_EQ_INT((int)DSD(r1 + 0x08u), (int)far_p);
    CHECK_EQ_INT((int)DSW(r1 + 0x34u), 0xFFE0);
    CHECK_EQ_INT((int)DSB(R46_NODE(1) + 0x0Cu), 2);
    CHECK_EQ_INT((int)DSD(r2 + 0x08u), 0x00ABCDEF);
    CHECK_EQ_INT((int)DSB(R46_NODE(2) + 0x0Cu), 3);
    CHECK_EQ_INT((int)DSB(R46_108397), 0);
    CHECK_EQ_INT((int)DSD(DS_000EF6D8), 0x1234);    /* no draw */
    CHECK_EQ_INT((int)DSB(R46_108396), 0x85);

    /* D: phase 1 within 0x1000 (node 0 at +0x1000): rng(0x14) != 0 draws
     * once and does nothing else. */
    seed = r46_rng_seed(0x14u, 0, 1);
    CHECK(seed != 0u, "a seed with rng(0x14) != 0");
    r46_seed();
    r0 = r46_node(0, 1u, 0x00021000u, 0x0040u);
    DSD(R46_REC(0) + 0x30u) = 0x0180ABCDu;
    rng_seed(seed);
    (void)rng_next(0x14u);
    r46_snap();
    rng_seed(seed);
    if (p1 != NULL) p1();
    CHECK(r46_same(), "0x48F98 phase 1 near with rng(0x14) != 0: one draw only");
    /* D2/D3: rng(0x14) == 0 with rng(2) != 0 (a5 0x4000) and == 0 (a5 0):
     * the spawn equals 0x2AE14(0xA89AC, x 0x1F000, 0x180, 0, a5) run from the
     * same state after the two draws, and differs from it with a3/a4
     * swapped; the actor comes off the free list with type 0x19. */
    for (i = 0; i < 2u; i++) {
        u32 a5 = i == 0u ? 0x4000u : 0u;
        seed = r46_rng_seed(0x14u, 1, i == 0u);
        CHECK(seed != 0u, "a seed with rng(0x14) == 0");
        r46_d_setup();
        rng_seed(seed);
        if (p1 != NULL) p1();
        CHECK_EQ_INT((int)DSD(DS_00105BCC), (int)sp);
        CHECK_EQ_INT((int)DSB(sp + 0x48u), 0x19);
        CHECK_EQ_INT((int)(DSW(sp + 0x28u) & 0x4000u), (int)a5);
        CHECK_EQ_INT((int)DSB(R46_NODE(0) + 0x0Cu), 1);
        CHECK_EQ_INT((int)DSD(R46_REC(0) + 0x08u), 0x00ABCDEF);
        r46_snap();
        r46_d_setup();
        r46_mirror(seed, 0x14u, 0x0001F000u, 0x180u, 0u, a5);
        CHECK(r46_same(), "0x48F98's spawn equals 0x2AE14 with the raw's arguments");
        r46_d_setup();
        r46_mirror(seed, 0x14u, 0x0001F000u, 0u, 0x180u, a5);
        CHECK(!r46_same(), "0x48F98's spawn: a3/a4 swapped differ");
    }

    /* E: 0x28F08 with DS_00104AD4 = 0 reads slot 1 (x 0x31234, record 5's
     * height 0x100); rng(6) != 0 draws once only. */
    seed = r46_rng_seed(6u, 0, 1);
    CHECK(seed != 0u, "a seed with rng(6) != 0");
    r46_seed();
    rng_seed(seed);
    (void)rng_next(6u);
    r46_snap();
    r46_seed();
    rng_seed(seed);
    if (p10 != NULL) p10();
    CHECK(r46_same(), "0x28F08 with rng(6) != 0: one draw only");
    /* E2/E3: rng(6) == 0, a5 0x4000 then 0; DS_00104AD4 = 1 in E3 reads slot
     * 0 (x 0x2C2C0000, record 4's height 0x140). Checked against 0x2AE14
     * with (x, height, 0xC00, a5), and against a3/a4 swapped. */
    for (i = 0; i < 2u; i++) {
        u32 a5 = i == 0u ? 0x4000u : 0u;
        u32 x = i == 0u ? 0x00031234u : 0x2C2C0000u;
        u32 y = i == 0u ? 0x100u : 0x140u;
        seed = r46_rng_seed(6u, 1, i == 0u);
        CHECK(seed != 0u, "a seed with rng(6) == 0");
        r46_seed();
        DSD(DS_00104AD4) = i;
        rng_seed(seed);
        if (p10 != NULL) p10();
        CHECK_EQ_INT((int)DSD(DS_00105BCC), (int)sp);
        CHECK_EQ_INT((int)DSB(sp + 0x48u), 0x19);
        CHECK_EQ_INT((int)(DSW(sp + 0x28u) & 0x4000u), (int)a5);
        r46_snap();
        r46_seed();
        DSD(DS_00104AD4) = i;
        r46_mirror(seed, 6u, x, y, 0x0C00u, a5);
        CHECK(r46_same(), "0x28F08's spawn equals 0x2AE14 with the raw's arguments");
        r46_seed();
        DSD(DS_00104AD4) = i;
        r46_mirror(seed, 6u, x, 0x0C00u, y, a5);
        CHECK(!r46_same(), "0x28F08's spawn: a3/a4 swapped differ");
    }
    /* E4: a zero slot 1 is not tested: 0x28F08 reads x from mem[0x2C] and
     * the height through the dword at mem[0] (0x40 -> mem[0x70]). */
    seed = r46_rng_seed(6u, 1, 1);
    r46_seed();
    DSD(DS_001077A8 + 4u) = 0u;
    DSD(0x00u) = 0x40u;
    DSD(0x2Cu) = 0x00055555u;
    DSD(0x70u) = 0x0120ABCDu;
    rng_seed(seed);
    if (p10 != NULL) p10();
    CHECK_EQ_INT((int)DSD(DS_00105BCC), (int)sp);
    r46_snap();
    r46_seed();
    DSD(DS_001077A8 + 4u) = 0u;
    DSD(0x00u) = 0x40u;
    DSD(0x2Cu) = 0x00055555u;
    DSD(0x70u) = 0x0120ABCDu;
    r46_mirror(seed, 6u, 0x00055555u, 0x120u, 0x0C00u, 0x4000u);
    CHECK(r46_same(), "0x28F08 spawns through a zero slot as the raw does");

    q42_restore();
    tf_put(low, 0u, sizeof low);
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
    u8 s_4880[0x110];
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
     * above, and s_4880 spans the 0x104780..0x104870 nodes and the
     * 0x104880/0x104888 sentinel pairs (state 6's 0x28E98 writes both). */
    tf_snap(s_f0a78, 0x000F0A78u, 0x68u);
    tf_snap(s_c20, 0x00100C20u, 0x10u);
    tf_snap(s_4880, 0x00104780u, 0x110u);
    tf_snap(s_82e0, 0x001082E0u, 0x90u);
    s_8398 = DSB(0x00108398u);

    /* First, while the shipped INDEX is still in place (the fixtures below
     * zero its pointer). */
    check_spawn_sound();
    check_char1_entry();
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
    check_effects_worship();
    check_command_map();
    check_think_chain();
    check_projectile_step();
    check_point_trample();
    check_grab_arms();
    check_attack_consume();
    check_char_select();
    check_health_bars();
    check_slot_latch();
    check_state6();
    check_slots_reset();
    check_state7();
    check_game_frame_tail();
    check_mode_tail();
    check_list_init();
    check_type_0a19_list_init();
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
    check_pose_handler_3a6d4();
    check_trex_leap();
    check_knockback_pose();
    check_knockdown_floor();
    check_walk_entry();
    check_worshipper_arrival();
    check_worshipper_landing();
    check_trex_breath();
    check_reaction_attack();
    check_trex_grab();
    check_char3_reaction();
    check_block();
    check_slot_hook();
    check_char3_grab();
    check_stance_return();
    check_freeze_235c4();
    check_type_0a19_update();
    check_freeze_22ce4();
    check_hook_22d8c();
    check_char1_reactions();
    check_char1_reactions_b();
    check_char1_chain();
    check_reaction_predicates();
    check_reaction();
    check_winner_body();
    check_pass_a_tail();
    check_body_push();
    check_throw_3c208();
    check_hold_3e244();
    check_land_36280();
    check_finisher_48aac();
    check_page_tail();
    check_type_table();
    check_type_callbacks();
    check_type_teardown();
    check_dust_list();
    check_arena_backdrop();
    check_char3_2425();
    check_mode22_pass();
    check_flyers_4987c();
    check_volleyball();
    check_update_48f98();

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
    tf_put(s_4880, 0x00104780u, 0x110u);
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

    check_combo_text();
    /* After the restores above: it needs the real resource table, which the
     * earlier fixtures replace, and it saves and restores all it writes. */
    check_char_screen_setup();
    check_char_screen_open();
    check_char_screen_modes();
    check_mode_1a_hooks();
    check_mode_17_hooks();
    check_mode_17_step();
    check_mode_switch();
    check_mode_10_step();

    return g_failures - before;
}
