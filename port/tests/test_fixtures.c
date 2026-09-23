/* port/tests/test_fixtures.c — the fixtures shared by the test files. Every
 * body here is a byte-for-byte copy of its former per-file definition; only
 * the name and the home changed. */
#include "test_fixtures.h"
#include "game/actors.h"
#include "game/effects.h"
#include "platform/gfx.h"
#include "mem.h"
#include "symbols.h"
#include <string.h>

/* ---- test_fight.c ---- */

void tf_snap(u8 *dst, u32 off, u32 len) { memcpy(dst, mem + off, len); }
void tf_put(const u8 *src, u32 off, u32 len) { memcpy(mem + off, src, len); }

/* The arena frame's minimal live fixture: two seeded slots with no camera-target
 * record, an empty effect list and every gate closed, so fight_arena_frame runs
 * its call order without pulling in the gap functions. Returns P0's record. */
u32 tf_demo_fixture(void)
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

/* Zero both slots and the scratch records, make slot[0] live with `ch`, and
 * seed the mode/command/reaction globals the chain reads. */
u32 tf_hit_fixture(u32 ch)
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

/* ---- test_effects.c ---- */

/* The real master loop drains the palette dirty list once per frame (0x25672);
 * these fixtures drive effects_init/spawn/step/teardown directly and would
 * otherwise accumulate records across fixtures — a temporary counter measured
 * 58 records between drains and 4 written past the list before this drain was
 * added. The list holds the 24 records at DS_00107498..DS_00107618, so each
 * fixture drains at its boundary the way the loop does. */
void tf_effects_fixture_begin(void)
{
    effects_init();
    gfx_flush_palette();
}

/* ---- test_anim.c ---- */

u32 tf_anim_alloc_record(void)
{
    actors_reset();
    DSB(DS_00104B24) = 0;
    DSB(DS_00104B26) = 0;
    return actor_alloc(0);
}

/* Build a descriptor at ANIM_DESC whose stream is ANIM_SCRATCH, spawn it. */
u32 tf_anim_spawn_stream(void)
{
    memset(mem + ANIM_DESC, 0, 0x14);
    *(u32 *)(mem + ANIM_DESC + 0x00) = ANIM_SCRATCH;
    DSB(ANIM_DESC + 0x04) = 0x00;              /* render type 0 */
    DSB(ANIM_DESC + 0x05) = 0x00;
    return actor_spawn((const u32 *)(mem + ANIM_DESC), 0, 0, 0, 0);
}

/* ---- test_frontend.c ---- */

/* Seed exactly one live front-end list entry at DS_00107608 with `handle` in
 * its +0 dword. The iterator advances by 0x10 before its first test, so the
 * live dword at tbl+4 is skipped and the entry at tbl+0x10 wins. `saved` must
 * hold 0x190 bytes; tf_frontend_restore_list() puts the whole table back. Shared
 * by the 0x33904 iterator check and the state-3 (0x12484) check. */
void tf_frontend_seed_list(u32 handle, u8 *saved)
{
    const u32 tbl = DS_00107608;
    for (u32 i = 0; i < 0x190u; i++) saved[i] = DSB(tbl + i);
    mem_fill(tbl, 0, 0x190u);
    DSD(tbl + 0x04u) = 1u;            /* tbl's own +4 is live */
    DSD(tbl + 0x10u) = handle;        /* the returned entry's +0 handle */
    DSD(tbl + 0x14u) = 1u;            /* entry at tbl+0x10 is live */
}

void tf_frontend_restore_list(const u8 *saved)
{
    const u32 tbl = DS_00107608;
    for (u32 i = 0; i < 0x190u; i++) DSB(tbl + i) = saved[i];
}
