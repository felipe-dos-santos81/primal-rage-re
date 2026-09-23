/* port/tests/test_fight.c */
#include "game/actors.h"
#include "game/camera.h"
#include "game/fight.h"
#include "game/fighter.h"
#include "game/flow.h"
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

/* The arena frame's minimal live fixture: two seeded slots with no camera-target
 * record, an empty effect list and every gate closed, so fight_arena_frame runs
 * its call order without pulling in the gap functions. Returns P0's record. */
static u32 demo_fixture(void)
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
    DSD(DS_001077A8 + 4u) = 0;
    /* 0x1282C: the dust gate is closed. */
    DSW(DS_000EF6DC) = 1;
    /* 0x1958C: inert unless DS_001078FA == 2. */
    DSB(DS_001078FA) = 0;
    /* 0x12DA8: a non-zero camera mode selects the max arm, no slot deref. */
    DSB(DS_000F0AFE) = 4;

    /* The state 7 / game_frame tails: DS_000F0A71 and the DS_001088D8 input
     * bits must be clear so frontend_pause_tail/frontend_continue_tail return,
     * and the overlay must take its early return. The actor list is emptied so
     * game_frame's actors_update walk is inert. */
    DSB(DS_000F0A71) = 0;
    DSD(DS_001088D8) = 0;
    DSB(DS_0009AD58) = 0;
    DSB(DS_00105D60) = 0;
    DSD(DS_00105C00) = 0;
    DSB(DS_00105C04) = 0;
    DSD(DS_00105BCC) = DS_00105BCC;
    DSB(DS_00104B24) = 0;
    return p0;
}

/* 0x263F4: the arena frame's order and its two observable contracts. The two
 * latch sentinels differ from the values they copy, so a missing latch fails;
 * the 0x19068 pass stores a record float and clears the record's +0x20, so a
 * missing fighter update fails. */
static void check_arena_frame(void)
{
    u32 p0 = demo_fixture();

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

    mem_fill(FIGHT_ACTORS, 0, 0x80u);
    DSD(DS_001014EC) = FIGHT_ACTORS;
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

    (void)demo_fixture();
    DSB(DS_00104B1D) = 1;               /* skip the coin poll */
    DSD(DS_001088E4) = 0;
    DSB(DS_00104528 + 1u) = 2;          /* skip the text rows */
    actors_reset();                     /* deterministic pool for the spawn */

    /* State 6 (0x11A8C) runs the spawn; its slot stores are the live fixture.
     * The sentinels are readable in-range offsets that differ from the slots,
     * so a skipped spawn fails the assertions without an out-of-range deref. */
    DSD(DS_001077A8) = FIGHT_RECS;
    DSD(DS_001077A8 + 4u) = FIGHT_RECS + 0x100u;
    DSB(DS_00104B15) = 0;
    DSB(DS_00104B19 + 2u) = 0;
    DSW(DS_001082CC) = 0;
    DSW(DS_00104AFC) = 0;
    DSW(DS_000F0A6A) = 0;
    DSW(DS_000F0A72) = 5;
    DSW(DS_000F0A6C) = 0;
    DSB(DS_000F0A6F) = 0xFF;
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
    u32 p0 = FIGHT_RECS, p1 = FIGHT_RECS + 0x100u;
    mem_fill(FIGHT_RECS, 0, 0x200);
    DSD(DS_001077B0) = p0;
    DSD(DS_00107844) = p1;
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
    mem_fill(FIGHT_ACTORS, 0, 0x80);
    DSD(DS_001014EC) = FIGHT_ACTORS;
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
    u32 p0 = FIGHT_RECS, p1 = FIGHT_RECS + 0x100u;
    mem_fill(FIGHT_RECS, 0, 0x200);
    DSD(DS_001077B0) = p0;
    DSD(DS_00107844) = p1;
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
    DSD(DS_00100AB0) = 0x10u;
    DSD(DS_00100AB4) = 0x20u;
    DSD(p0 + 0x18u) = 0x100u;
    DSD(p0 + 0x1Cu) = 0x200u;
    fighter_slot_latch(0u);
    CHECK_EQ_INT((int)DSD(DS_001077B0 + 0x2Cu), 0x110);
    CHECK_EQ_INT((int)DSD(DS_001077B0 + 0x30u), 0x220);
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
    (void)demo_fixture();

    actors_reset();                     /* the spawn allocates from the pool */
    DSD(DS_001077A8) = FIGHT_RECS;      /* sentinels differ from the slots */
    DSD(DS_001077A8 + 4u) = FIGHT_RECS + 0x100u;

    DSB(DS_00104B1D) = 0;               /* let 0x41350 store the character */
    DSD(DS_001088E4) = 0;               /* coin poll mask: nothing accepted */
    DSB(DS_00104528 + 1u) = 2;          /* (DS_00104528+1)&2 set: skip text */
    DSW(DS_00104B00) = 3;               /* mode 3: 0x49388's range is 0x64 */
    DSB(DS_00104B15) = 0;
    DSB(DS_00104B19 + 2u) = 0;
    DSW(DS_001082CC) = 0;
    DSW(DS_00104AFC) = 0;
    DSW(DS_000F0A6A) = 0;
    DSW(DS_000F0A72) = 5;
    DSW(DS_000F0A6C) = 0;
    DSB(DS_000F0A6F) = 0xFF;
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
    rng_seed(0x1234u);
    game_state_step();

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
    (void)demo_fixture();
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

    (void)demo_fixture();
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
    (void)demo_fixture();
    DSB(DS_00104B1D) = 1;
    DSD(DS_00104B00) = 3;
    DSW(DS_000F0A64) = 7;
    DSW(DS_000F0A6A) = 2;
    DSB(DS_00104B15) = 1;
    DSB(DS_000F0AFE) = 4;
    DSD(DS_000F0AF0) = 0x7000u;
    DSD(DS_00104AE8) = 0;
    game_frame();
    CHECK_EQ_INT((int)DSD(DS_000F0AF0), 0x5D00);

    (void)demo_fixture();
    DSB(DS_00104B1D) = 1;
    DSD(DS_00104B00) = 3;
    DSW(DS_000F0A64) = 7;
    DSW(DS_000F0A6A) = 2;
    DSB(DS_00104B15) = 0;
    DSB(DS_000F0AFE) = 4;
    DSD(DS_000F0AF0) = 0x7000u;
    DSD(DS_00104AE8) = 0;
    game_frame();
    CHECK_EQ_INT((int)DSD(DS_000F0AF0), 0x7000);
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

    snap(s_li, 0x001083C4u, sizeof s_li);
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
    put(s_li, 0x001083C4u, sizeof s_li);
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

    snap(s_ai, 0x001081F0u, sizeof s_ai);
    mem_fill(0x001081F0u, 0, sizeof s_ai);
    mem_fill(FIGHT_RECS, 0, 0x400);
    mem_fill(FIGHT_ACTORS, 0, 0x80);

    DSD(DS_001014EC) = FIGHT_ACTORS;
    DSD(DS_001077B0) = p0;
    DSD(DS_00107844) = p1;
    DSD(DS_001077A8) = p0;
    DSD(DS_001077A8 + 4u) = p1;
    DSD(p0) = r0;                       /* slot+0 = fighter record */
    DSD(p1) = r1;
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

    put(s_ai, 0x001081F0u, sizeof s_ai);
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
    mem_fill(FIGHT_ACTORS, 0, 0x80);
    DSD(DS_001014EC) = FIGHT_ACTORS;
    DSD(DS_001077A8) = p0;
    DSD(DS_001077A8 + 4u) = 0;          /* side 1 inert */
    DSD(p0) = r0;
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
    mem_fill(0x00107D58u, 0, 0x180u);           /* no armed hitbox */
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
    mem_fill(FIGHT_ACTORS, 0, 0x80u);
    DSD(DS_001014EC) = FIGHT_ACTORS;
    DSD(DS_001077A8) = p0;
    DSD(DS_001077A8 + 4u) = 0;
    DSD(p0) = r0;
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

/* Zero both slots and the scratch records, make slot[0] live with `ch`, and
 * seed the mode/command/reaction globals the chain reads. */
static u32 hit_fixture(u32 ch)
{
    mem_fill(DS_001077B0, 0, 0x94u);
    mem_fill(DS_001077B0 + 0x94u, 0, 0x94u);
    mem_fill(0x00107D58u, 0, 0x180u);           /* the three per-slot arrays */
    mem_fill(FIGHT_RECS, 0, 0x200u);
    mem_fill(FIGHT_ACTORS, 0, 0x80u);
    DSD(DS_001014EC) = FIGHT_ACTORS;
    DSD(DS_001077B0) = FIGHT_RECS;
    DSD(DS_00107844) = FIGHT_RECS + 0x100u;
    DSB(DS_001077B0 + 0x7Au) = (u8)ch;
    DSB(DS_001077B0 + 0x63u) = 1;
    DSB(DS_001077B0 + 0x5Fu) = 0xFFu;
    DSD(DS_00104B00) = 3;
    DSB(DS_001078FA) = 0;
    DSW(DS_001088E0) = 0;
    DSW(DS_001088E2) = 0;
    DSB(DS_00100B5A) = 0;
    DSB(DS_00100B5A + 1u) = 0;
    DSB(DS_00100B5E) = 0;
    DSB(DS_00100B5E + 1u) = 0;
    DSB(DS_00107EE4) = 0;
    DSD(DS_00107ED8) = 0;
    DSD(DS_00107EDC) = 0;
    return FIGHT_RECS;
}

/* §7.8 0x3C600: the per-attack-frame descriptor. */
static void check_hit_frame_desc(void)
{
    u32 saved = DSD(DS_00101514);
    u32 tab = FIGHT_RECS + 0x400u;

    (void)hit_fixture(0);
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
    (void)hit_fixture(0);
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
    (void)hit_fixture(0);
    DSW(DS_00107D58) = 8;
    DSW(DS_00107DD8) = 3;
    hit_slot_step();
    CHECK_EQ_INT((int)DSW(DS_00107DD8), 2);     /* decrement, still >= 1 */
    CHECK_EQ_INT((int)DSW(DS_00107D58), 8);

    (void)hit_fixture(0);
    DSW(DS_00107D58) = 8;
    DSW(DS_00107DD8) = 0;
    hit_slot_step();
    CHECK_EQ_INT((int)DSW(DS_00107D58), 0);     /* signed -1 < 1 clears */
    CHECK_EQ_INT((int)DSW(DS_00107DD8), 3);     /* reloaded from frame 0 */

    /* Phase 7 with a live stun runs the 2..7 block; the exact outcome is
     * data-dependent (0x3C758/0x3C800 are §6.3), so no assertion here. */
    (void)hit_fixture(0);
    DSW(DS_00107D58) = 7;
    DSW(DS_00107DD8) = 5;
    hit_slot_step();

    /* The arm store 0x3C9A3: phase 0 with frame_table[phase].dword0 == 0 (the
     * shipped char-0 entries read 0x00080000, so the entry is seeded here) and a
     * connect that returns 0 (command 0) writes phase = 8. */
    {
        u32 saved0 = DSD(0x000BFE3Cu), saved1 = DSD(0x000BFE3Cu + 0x14u);
        (void)hit_fixture(0);
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
    (void)hit_fixture(0);
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
    (void)hit_fixture(0);
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
    (void)hit_fixture(0);
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

    (void)hit_fixture(0);
    DSB(DS_001077B0 + 0x7Cu) = 0;
    DSB(DS_001077B0 + 0x52u) = 0x55;            /* §7.4: unchanged, not 0 */
    DSW(DS_00107D58) = 8;
    DSW(DS_001077B0 + 0x84u) = 0x10;
    CHECK_EQ_INT(hit_reaction_drive(0u, 0u), 1);
    CHECK_EQ_INT((int)DSW(DS_001077B0 + 0x84u), 0x11);
    CHECK_EQ_INT((int)DSB(DS_001077B0 + 0x5Fu), 0x20);
    CHECK_EQ_INT((int)DSB(DS_001077B0 + 0x52u), 0x55);

    (void)hit_fixture(0);
    DSB(DS_001077B0 + 0x56u) = 6;               /* the gate rejects */
    CHECK_EQ_INT(hit_reaction_drive(0u, 0u), 0);

    (void)hit_fixture(0);
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
    (void)hit_fixture(0);
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
}

/* §7.1 0x3CF38: a resolved hit consumes the hitbox and drives the reaction. */
static void check_hit_chain(void)
{
    (void)hit_fixture(0);
    DSW(DS_00107D58) = 8;                       /* armed hitbox 0 */
    DSB(DS_001077B0 + 0x7Cu) = 0;
    CHECK_EQ_INT(hit_chain_resolve(0u), 1);
    CHECK_EQ_INT((int)DSB(DS_001077B0 + 0x55u), 0);
    CHECK_EQ_INT((int)DSB(DS_001077B0 + 0x5Fu), 0x20);
    CHECK_EQ_INT((int)DSB(DS_001077B0 + 0x7Cu), 1);
    CHECK_EQ_INT((int)DSW(DS_00107D58), 0);     /* consumed */

    /* No armed hitbox: the raw returns at 0x3CF5E without touching +0x55. */
    (void)hit_fixture(0);
    DSW(DS_00107D58) = 7;
    DSB(DS_001077B0 + 0x55u) = 0x11;
    DSB(DS_001077B0 + 0x7Cu) = 0x40;
    CHECK_EQ_INT(hit_chain_resolve(0u), 0);
    CHECK_EQ_INT((int)DSB(DS_001077B0 + 0x55u), 0x11);  /* untouched */
    CHECK_EQ_INT((int)DSB(DS_001077B0 + 0x7Cu), 0x40);

    /* Armed but the reaction gate fails: the else path resets +0x5F/+0x55. */
    (void)hit_fixture(0);
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
    u32 rec = hit_fixture(0), p1 = FIGHT_RECS + 0x100u;

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
    (void)hit_fixture(0);
    DSD(DS_001077A8) = s0;
    DSD(DS_001077A8 + 4u) = s1;
    DSD(s0) = r0;
    DSD(s1) = r1;
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
    (void)hit_fixture(0);
    DSD(DS_001077A8) = s0;
    DSD(DS_001077A8 + 4u) = s1;
    DSD(s0) = r0;
    DSD(s1) = r1;
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
    (void)hit_fixture(0);
    DSD(DS_001077A8) = s0;
    DSD(DS_001077A8 + 4u) = s1;
    DSD(s0) = r0;
    DSD(s1) = r1;
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
    (void)hit_fixture(0);
    DSD(DS_001077A8) = s0;
    DSD(DS_001077A8 + 4u) = s1;
    DSD(s0) = r0;
    DSD(s1) = r1;
    DSB(s0 + 0x53u) = 8;
    DSW(s0 + 0x88u) = 0;
    DSB(s0 + 0x5Fu) = 0x18u;                    /* 0x34E20: not < 0x18 */
    DSB(s0 + 0x7Cu) = 0;
    DSW(DS_00107D58) = 8;
    fighter_state_3531c(0u);
    CHECK_EQ_INT((int)DSB(s0 + 0x7Cu), 0);
    CHECK_EQ_INT((int)DSB(s0 + 0x53u), 8);

    /* D: 0x39280 clears slot+0x5D and the +0x43 bit 2. */
    (void)hit_fixture(0);
    DSB(DS_001077B0 + 0x5Du) = 0xAAu;
    DSB(DS_001077B0 + 0x43u) = 0xFFu;
    fighter_state_39280(0u);
    CHECK_EQ_INT((int)DSB(DS_001077B0 + 0x5Du), 0);
    CHECK_EQ_INT((int)DSB(DS_001077B0 + 0x43u), 0xFB);

    /* E: 0x34DDC. char 0's threshold is word[0xBD870] = 0x1180. */
    (void)hit_fixture(0);
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
    (void)hit_fixture(0);
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
    (void)hit_fixture(0);
    DSD(DS_001077A8) = s0;
    DSD(DS_001077A8 + 4u) = s1;
    DSD(s0) = r0;
    DSD(s1) = r1;
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
    (void)hit_fixture(0);
    DSD(DS_001077A8) = s0;
    DSD(DS_001077A8 + 4u) = s1;
    DSD(s0) = r0;
    DSD(s1) = r1;
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
    (void)hit_fixture(0);
    DSD(DS_001077A8) = s0;
    DSD(DS_001077A8 + 4u) = s1;
    DSD(s0) = r0;
    DSD(s1) = r1;
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
    (void)hit_fixture(0);
    DSD(DS_001077A8) = s0;
    DSD(DS_001077A8 + 4u) = s1;
    DSD(s0) = r0;
    DSD(s1) = r1;
    DSB(r0 + 0x51u) = 0;
    DSB(s0 + 0x58u) = 3;
    DSW(r0 + 0x34u) = 0x0010u;                  /* |+0x34| = 0x10 < 0x11 */
    DSB(s0 + 0x52u) = 0xAAu;
    fighter_state_36300(s0, r0);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 9);
    CHECK_EQ_INT((int)DSW(r0 + 0x34u), 0);

    /* D: 0x36710 (+0x52 = 17). +0x58 = 2 with rec+0x36 == 0 and rec+0x1C == 0
     * writes rec+0x43 = byte[0xBD89A] = 8 and +0x52 = 9. The sentinels differ. */
    (void)hit_fixture(0);
    DSD(DS_001077A8) = s0;
    DSD(DS_001077A8 + 4u) = s1;
    DSD(s0) = r0;
    DSD(s1) = r1;
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
    (void)hit_fixture(0);
    DSD(DS_001077A8) = s0;
    DSD(s0) = r0;
    DSB(r0 + 0x51u) = 0;
    DSB(s0 + 0x58u) = 0;
    DSB(s0 + 0x52u) = 0xAAu;
    fighter_state_36710(s0, r0);
    CHECK_EQ_INT((int)DSB(s0 + 0x58u), 1);
    CHECK_EQ_INT((int)DSB(s0 + 0x52u), 0xAA);

    /* E: 0x399CC (+0x52 = 7). The self slot gets +0x41 bit 2 and +0x74 = 0. */
    (void)hit_fixture(0);
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
    (void)hit_fixture(0);
    DSD(DS_001077A8) = s0;
    DSD(DS_001077A8 + 4u) = s1;
    DSD(s0) = r0;
    DSD(s1) = r1;
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
    (void)hit_fixture(0);
    DSD(DS_001077A8) = s0;
    DSD(DS_001077A8 + 4u) = s1;
    DSD(s0) = r0;
    DSD(s1) = r1;
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
    (void)hit_fixture(0);
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
    mem_fill(0x00107D58u, 0, 0x180u);           /* the three hitbox arrays */
    DSD(DS_001014EC) = FIGHT_ACTORS;
    DSD(DS_001077A8) = p0;
    DSD(DS_001077A8 + 4u) = 0;                  /* side 1 inert */
    DSD(p0) = r0;
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
    (void)hit_fixture(0);
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
    snap(s_8100, 0x00108100u, 0x80u);
    snap(s_5b, 0x00105B00u, 0x300u);
    snap(s_9ad, 0x0009AD50u, 0x10u);

    check_projection();
    check_dispatch();
    check_y_commit();
    check_dust_gate();
    check_screen_base();
    check_decay();
    check_arena_frame();
    check_arena_frame_live();
    check_pset_palette_zero_handle();
    check_fighter_pass_a();
    check_fighter_pass_b();
    check_hud_pass();
    check_hud_sync();
    check_effects_rng();
    check_command_map();
    check_think_chain();
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
    check_hud_pass_machine();
    check_anim_stream_args();

    put(s_f0ae0, 0x000F0AE0u, 0x20u);
    put(s_proj, 0x00100A70u, 0xF4u);
    put(s_slots, 0x001077A0u, 0x160u);
    put(s_d0, 0x00107D00u, 0x200u);
    put(s_88, 0x00108840u, 0x100u);
    put(s_a5, 0x00104500u, 0x800u);
    put(s_82, 0x00108260u, 0x80u);
    put(s_8100, 0x00108100u, 0x80u);
    put(s_5b, 0x00105B00u, 0x300u);
    put(s_9ad, 0x0009AD50u, 0x10u);
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
