/* port/tests/test_fight.c */
#include "game/camera.h"
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

int test_fight(void)
{
    int before = g_failures;

    u8 s_f0ae0[0x20];
    u8 s_proj[0xF4];
    u8 s_slots[0x160];
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

    check_projection();
    check_dispatch();
    check_y_commit();
    check_dust_gate();
    check_screen_base();
    check_decay();

    put(s_f0ae0, 0x000F0AE0u, 0x20u);
    put(s_proj, 0x00100A70u, 0xF4u);
    put(s_slots, 0x001077A0u, 0x160u);
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
