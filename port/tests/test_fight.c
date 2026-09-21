/* port/tests/test_fight.c */
#include "game/camera.h"
#include "game/fight.h"
#include "game/fighter.h"
#include "game/rng.h"
#include "mem.h"
#include "symbols.h"
#include "test.h"
#include <string.h>

/* Scratch above the resource heap (test_effects uses 0x3F00000). */
#define FIGHT_ACTORS 0x3F20000u
#define FIGHT_RECS   0x3F30000u

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

static void snap(u8 *dst, u32 off, u32 len) { memcpy(dst, mem + off, len); }
static void put(const u8 *src, u32 off, u32 len) { memcpy(mem + off, src, len); }

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
    DSD(DS_001077B0) = p0;              /* P0 fighter record pointer */
    DSD(DS_00107844) = p1;              /* P1 fighter record pointer */
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

    /* The facing byte is the record's +0x28 bit 0x4000; the page flag starts
     * 0 (the 0x16AFC/0x164F4 tail is an unported gap). */
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
    DSD(DS_001077B0) = p0;
    DSD(DS_00107844) = p1;
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

/* 0x263F4: the arena frame's order and its two observable contracts. The two
 * latch sentinels differ from the values they copy, so a missing latch fails;
 * the 0x19068 pass stores a record float and clears the record's +0x20, so a
 * missing fighter update fails. */
static void check_arena_frame(void)
{
    u32 p0 = FIGHT_RECS, p1 = FIGHT_RECS + 0x100u;
    u32 a0 = FIGHT_ACTORS + 1u * 0x20u;

    mem_fill(FIGHT_ACTORS, 0, 0x80);
    mem_fill(FIGHT_RECS, 0, 0x200);

    DSD(DS_001014EC) = FIGHT_ACTORS;
    DSD(DS_001077B0) = p0;
    DSD(DS_00107844) = p1;
    DSD(DS_001077B8) = 0;
    DSD(DS_0010784C) = 0;
    DSD(DS_001014E0) = 0;               /* res_resolve NULL: no origin subtract */
    DSD(DS_001014F0) = 0;
    DSW(p0 + 0x28u) = 0;
    DSW(p0 + 0x56u) = 1;
    DSW(p1 + 0x28u) = 0;
    DSW(p1 + 0x56u) = 2;
    DSW(a0) = 0;                        /* actor bit 15 clear */
    DSD(DS_00100AF0) = 0;
    DSD(DS_00100AF4) = 0;

    /* The latches must take the PRE-frame values, not their sentinels. */
    DSD(DS_001077E4) = 0x00001111u;
    DSD(DS_001077E8) = 0x00002222u;
    DSD(DS_00107878) = 0x00003333u;
    DSD(DS_0010787C) = 0x00004444u;

    /* 0x19068 side 0: the +0x5E timer reaches the record float store. */
    DSB(DS_00107802) = 0;
    DSB(DS_00107896) = 0;
    DSB(0x0010780Fu) = 0;               /* slot0 +0x5F */
    DSB(DS_00100B58) = 0;               /* equal -> the stance gate passes */
    DSB(DS_00100B5E) = 1;
    DSB(DS_00100B5A) = 1;
    DSB(DS_00100B5C) = 3;
    DSD(p0 + 0x24u) = 0;
    DSD(p0 + 0x20u) = 0xDEADBEEFu;
    DSB(0x001078A3u) = 0;               /* slot1 +0x5F */
    DSB(DS_00100B58 + 1u) = 0x55;            /* side 1 mismatches -> 0x1922C skip */

    /* 0x49C78: an empty effect list; the DS_001088BF tail gate is closed. */
    DSD(DS_0010884C) = DS_0010884C;
    DSB(DS_001088BF) = 0;
    /* 0x35658: no camera-target record, so the HUD pass returns at 0x3577E. */
    DSD(DS_001077A8) = 0;
    /* 0x1282C: the dust gate is closed. */
    DSW(DS_000EF6DC) = 1;
    /* 0x1958C: inert unless DS_001078FA == 2. */
    DSB(DS_001078FA) = 0;
    /* 0x12DA8: a non-zero camera mode selects the max arm, no slot deref. */
    DSB(DS_000F0AFE) = 4;

    fight_arena_frame();

    CHECK_EQ_INT((int)DSD(DS_001077E8), 0x1111);
    CHECK_EQ_INT((int)DSD(DS_0010787C), 0x3333);
    CHECK_EQ_INT((int)DSD(p0 + 0x24u), 0x40400000);   /* (float)3.0 */
    CHECK_EQ_INT((int)DSD(p0 + 0x20u), 0);
}

/* 0x1958C: a side whose +0x803 state byte is 0x0A clears its DS_00100AF8
 * entry. The sentinel differs from the post-condition. */
static void check_fighter_pass_a(void)
{
    u32 p0 = FIGHT_RECS, p1 = FIGHT_RECS + 0x100u;

    DSD(DS_001014EC) = FIGHT_ACTORS;
    DSD(DS_001077B0) = p0;
    DSD(DS_00107844) = p1;
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
    u32 p0 = FIGHT_RECS, p1 = FIGHT_RECS + 0x100u;

    mem_fill(FIGHT_RECS, 0, 0x200);
    DSD(DS_001077B0) = p0;
    DSD(DS_00107844) = p1;
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
}

/* 0x49C78: the direct RNG call sites and their gates. A case-3 entry issues
 * exactly one rng(0x3C); the DS_001088BF tail issues one rng(2) only inside
 * 1..4. The RNG state is the proof; the sentinel seeds prove the gate. */
static void check_effects_rng(void)
{
    u32 entry = FIGHT_RECS + 0x3000u;
    u32 rec = FIGHT_RECS + 0x3100u;
    u32 saved;

    mem_fill(FIGHT_RECS, 0, 0x4000);
    DSD(DS_001014EC) = FIGHT_ACTORS;
    DSD(DS_001077B0) = FIGHT_RECS;
    DSD(DS_00107844) = FIGHT_RECS + 0x100u;
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
}

/* 0x3B134: the command-word mapper's stance branch (record §8.13). The three
 * side-0 cases differ only in the other slot's +0x34 sign and +0x64 stance, so
 * a swapped branch or a missing table select fails; the side-1 case proves the
 * side index reaches DS_001088E2, not DS_001088E0. Every case seeds the command
 * word with 0xFFFF, which differs from every expected value. The mapper's
 * rng(100) roll is bypassed with a non-zero override. */
static void check_command_map(void)
{
    u32 p0 = FIGHT_RECS, p1 = FIGHT_RECS + 0x100u;
    u32 r = FIGHT_RECS + 0x400u;

    mem_fill(FIGHT_RECS, 0, 0x800);
    DSD(DS_001077B0) = p0;
    DSD(DS_00107844) = p1;
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

/* 0x1975C/0x3B464/0x3B298: one fighter_think() must run both think drivers.
 * The 0x3B464 tail is unconditional (slot+0x64 <- 0xFF and slot+0x41 |= 0x80)
 * once the driver's gates pass, and 0x3B298's entry copy is
 * word[slot[side]+0x86] = word[slot[1-side]+0x84]. The seeds differ from every
 * post-condition, so a missing driver or a skipped side fails. Gate A is left
 * off so no rng(100) roll is consumed. */
static void check_think_chain(void)
{
    u32 p0 = FIGHT_RECS, p1 = FIGHT_RECS + 0x100u;
    u32 r0 = FIGHT_RECS + 0x300u, r1 = FIGHT_RECS + 0x320u;

    mem_fill(FIGHT_RECS, 0, 0x800);
    DSD(DS_001077B0) = p0;
    DSD(DS_00107844) = p1;
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
    /* 0x3B134's gate A off, so the mapper returns without the rng roll. */
    DSB(0x001077B0u + 0x63u) = 0;
    DSB(0x00107844u + 0x63u) = 0;
    /* 0x1AB5C/0x1AB10 state bytes. */
    DSB(0x001077B0u + 0x54u) = 0;
    DSB(0x001077B0u + 0x53u) = 0;
    DSB(0x00107844u + 0x54u) = 0;
    DSB(0x00107844u + 0x53u) = 0;
    /* The +0x48 switch reads the other slot's secondary record. */
    DSD(0x001077B8u) = r0;
    DSD(0x0010784Cu) = r1;
    DSB(r0 + 0x48u) = 0;
    DSB(r1 + 0x48u) = 0;
    /* The entry-copy sentinels. */
    DSW(0x00107834u) = 0x1111u;         /* slot0 +0x84 */
    DSW(0x001078C8u) = 0x2222u;         /* slot1 +0x84 */
    DSW(0x00107836u) = 0x3333u;         /* slot0 +0x86 */
    DSW(0x001078CAu) = 0x4444u;         /* slot1 +0x86 */

    fighter_think();

    CHECK_EQ_INT((int)DSB(0x00107814u), 0xFF);      /* slot0 +0x64 */
    CHECK_EQ_INT((int)DSB(0x001078A8u), 0xFF);      /* slot1 +0x64 */
    CHECK_EQ_INT((int)DSB(0x001077F1u) & 0x80, 0x80);
    CHECK_EQ_INT((int)DSB(0x00107885u) & 0x80, 0x80);
    CHECK_EQ_INT((int)DSB(0x00107817u), 1);         /* slot0 +0x67 */
    CHECK_EQ_INT((int)DSB(0x001078ABu), 1);         /* slot1 +0x67 */
    /* i=0 thinks side 1: slot1+0x86 <- slot0+0x84; i=1 thinks side 0. */
    CHECK_EQ_INT((int)DSW(0x001078CAu), 0x1111);
    CHECK_EQ_INT((int)DSW(0x00107836u), 0x2222);
}

/* 0x3BDDC: the attack/command consumer (record §8.17). Input A drives the
 * transition; B/C prove the +0x40 and command-bit-15 gates; D adds the
 * table-select and the 0x1000/0x2000 command bits. The ring is seeded so
 * 0x4649C returns 0 (0xBEF28) or 1 (0xBEF64). */
static void check_attack_consume(void)
{
    u32 p0 = FIGHT_RECS, p1 = FIGHT_RECS + 0x100u;
    u32 saved_ring[5];
    u32 saved_pos = DSD(0x001082D2u);

    /* The five ring words 0x4649C reads for side 0 at position 0. */
    const u32 ring[5] = { 0x00108270u, 0x00108290u, 0x00108292u,
                          0x00108294u, 0x00108296u };

    mem_fill(FIGHT_RECS, 0, 0x400);
    DSD(DS_001077B0) = p0;
    DSD(DS_00107844) = p1;
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
    u32 s_actor_tab = DSD(DS_001014EC);
    u32 s_res_tab = DSD(DS_001014E0);
    u32 s_res_cnt = DSD(DS_001014F0);
    u32 s_a4fc = DSD(DS_00104AFC);
    u8  s_ae8 = DSB(DS_00104AE8);
    u8  s_810d = DSB(0x0010810Du);   /* DS_0010810D: no symbols.h name */
    u32 s_rng = DSD(DS_000EF6D8);
    u32 s_frame = DSD(DS_000EF6DC);

    snap(s_f0ae0, 0x000F0AE0u, 0x20u);
    snap(s_proj, 0x00100A70u, 0xF4u);
    snap(s_slots, 0x001077A0u, 0x160u);
    snap(s_d0, 0x00107D00u, 0x200u);
    snap(s_88, 0x00108840u, 0x100u);
    snap(s_a5, 0x00104500u, 0x800u);
    snap(s_82, 0x00108260u, 0x80u);

    check_projection();
    check_dispatch();
    check_y_commit();
    check_dust_gate();
    check_screen_base();
    check_decay();
    check_arena_frame();
    check_fighter_pass_a();
    check_fighter_pass_b();
    check_hud_pass();
    check_effects_rng();
    check_command_map();
    check_think_chain();
    check_attack_consume();

    put(s_f0ae0, 0x000F0AE0u, 0x20u);
    put(s_proj, 0x00100A70u, 0xF4u);
    put(s_slots, 0x001077A0u, 0x160u);
    put(s_d0, 0x00107D00u, 0x200u);
    put(s_88, 0x00108840u, 0x100u);
    put(s_a5, 0x00104500u, 0x800u);
    put(s_82, 0x00108260u, 0x80u);
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
