/* The small attract units that need no game_init(): the pause/continue tails
 * (0x10DB0/0x10E18), the state reset (0x10EE4), the config volumes (0x2C8F0
 * with eax = -2), the voice/rng scheduler (0x10F28) and the per-bit scene tick
 * (0x292AC). Every expected value is derived from the raw in
 * docs/superpowers/plans/2026-09-19-attract-derivations.md. */
#include "game/attract.h"
#include "game/config.h"
#include "game/rng.h"
#include "mem.h"
#include "symbols.h"
#include "test.h"

/* The PR_ATTRACT_DUMP continuous-run driver (4d): the real init and master loop,
 * plus the shared title window. */
#include "game/actors.h"
#include "game/flow.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

/* Highest 0x11000 phase (0xC). The boot cycle runs phases 0..9, 0xC and 0xB;
 * phase 0xA is the other arm of phase 9's DS_000F0A5C branch and is skipped
 * while the cycle counter is 0. */
#define ATTRACT_PHASE_MAX 0xCu

/* FNV-1a over the presented index buffer, the same stream the state-2 driver
 * logs. Deterministic per run; two runs' logs must diff clean. */
static u32 attract_frame_hash(const u8 *fb)
{
    u32 h = 2166136261u;
    for (u32 b = 0; b < 320u * 200u; b++) h = (h ^ fb[b]) * 16777619u;
    return h;
}

/* 0x292AC dispatch probes. The addresses are outside the code object, so they
 * cannot collide with a real original function. */
static int g_scene_probe[2];
static void scene_probe0(void) { g_scene_probe[0]++; }
static void scene_probe1(void) { g_scene_probe[1]++; }

int test_attract(void)
{
    int before = g_failures;

    /* The unit checks read the shipped data object (the config defaults and the
     * DS_000A8744 table). The shared suite maps PRAGE.EXE in test_le() first,
     * but the PR_ATTRACT_DUMP branch runs this file alone, so map it here. */
    {
        const char *gdir = getenv("PR_GAME_DIR");
        char exe[560];
        if (gdir == NULL || gdir[0] == '\0') gdir = "data/game/C";
        snprintf(exe, sizeof exe, "%s/PRAGE.EXE", gdir);
        if (DSD(DS_000A8744) == 0u)
            CHECK(mem_load_le(exe, NULL) == 1,
                  "PRAGE.EXE maps for the attract unit checks");
    }

    /* 0x10DB0: pause tail. Gate is DS_000F0A71 == 0 AND DS_001088D8 byte 3 bit
     * 0x20 AND byte 1 bit 0x10. byte 3 of the little-endian dword is 0x20 in
     * 0x20000000; byte 1 is 0x10 in 0x00001000. */
    {
        const u8  saved71 = DSB(DS_000F0A71);
        const u32 saved_d8 = DSD(DS_001088D8);
        const u16 saved64 = DSW(DS_000F0A64);
        const u16 saved6c = DSW(DS_000F0A6C);
        const u8  saved15 = DSB(DS_00104B15);
        const u8  saved19b2 = DSB(DS_00104B19 + 2u);

        /* Gate closed: no input bits. */
        DSB(DS_000F0A71) = 0;
        DSD(DS_001088D8) = 0;
        DSW(DS_000F0A64) = 3;
        DSW(DS_000F0A6C) = 3;
        DSB(DS_00104B15) = 0x9A;
        DSB(DS_00104B19 + 2u) = 0;
        frontend_pause_tail();
        CHECK_EQ_INT((int)DSW(DS_000F0A64), 3);      /* unchanged */
        CHECK_EQ_INT((int)DSB(DS_000F0A71), 0);      /* not latched */
        CHECK_EQ_INT((int)DSW(DS_000F0A6C), 3);
        CHECK_EQ_INT((int)DSB(DS_00104B15), 0x9A);

        /* The continue bits must not open the pause gate. */
        DSD(DS_001088D8) = 0x10000000u | 0x00002000u;
        frontend_pause_tail();
        CHECK_EQ_INT((int)DSW(DS_000F0A64), 3);
        CHECK_EQ_INT((int)DSB(DS_000F0A71), 0);

        /* Gate open, DS_00104B19 byte 2 == 0: latch then state 4, DS_000F0A6C
         * untouched, DS_00104B15 untouched. */
        DSD(DS_001088D8) = 0x20000000u | 0x00001000u;
        frontend_pause_tail();
        CHECK_EQ_INT((int)DSW(DS_000F0A64), 4);
        CHECK_EQ_INT((int)DSB(DS_000F0A71), 1);
        CHECK_EQ_INT((int)DSW(DS_000F0A6C), 3);
        CHECK_EQ_INT((int)DSB(DS_00104B15), 0x9A);

        /* Latched: the gate byte blocks a second transition. */
        DSW(DS_000F0A64) = 7;
        frontend_pause_tail();
        CHECK_EQ_INT((int)DSW(DS_000F0A64), 7);
        CHECK_EQ_INT((int)DSB(DS_000F0A71), 1);

        /* Gate open, DS_00104B19 byte 2 != 0: clear byte 2 and DS_00104B15,
         * set DS_000F0A6C and DS_000F0A64 to 4. */
        DSB(DS_000F0A71) = 0;
        DSB(DS_00104B15) = 0x9A;
        DSB(DS_00104B19 + 2u) = 0x5A;
        DSW(DS_000F0A6C) = 3;
        DSW(DS_000F0A64) = 3;
        frontend_pause_tail();
        CHECK_EQ_INT((int)DSB(DS_00104B19 + 2u), 0);
        CHECK_EQ_INT((int)DSB(DS_00104B15), 0);
        CHECK_EQ_INT((int)DSW(DS_000F0A6C), 4);
        CHECK_EQ_INT((int)DSW(DS_000F0A64), 4);

        DSB(DS_000F0A71) = saved71;  DSD(DS_001088D8) = saved_d8;
        DSW(DS_000F0A64) = saved64;  DSW(DS_000F0A6C) = saved6c;
        DSB(DS_00104B15) = saved15;
        DSB(DS_00104B19 + 2u) = saved19b2;
    }

    /* 0x10E18: continue tail -> state 5. Same shape, swapped bits: byte 3 bit
     * 0x10 and byte 1 bit 0x20, i.e. 0x10000000 | 0x00002000. */
    {
        const u8  saved71 = DSB(DS_000F0A71);
        const u32 saved_d8 = DSD(DS_001088D8);
        const u16 saved64 = DSW(DS_000F0A64);
        const u16 saved6c = DSW(DS_000F0A6C);
        const u8  saved15 = DSB(DS_00104B15);
        const u8  saved19b2 = DSB(DS_00104B19 + 2u);

        DSB(DS_000F0A71) = 0;
        DSD(DS_001088D8) = 0x20000000u | 0x00001000u;   /* pause bits: no */
        DSW(DS_000F0A64) = 3;
        DSW(DS_000F0A6C) = 3;
        DSB(DS_00104B15) = 0x9A;
        DSB(DS_00104B19 + 2u) = 0;
        frontend_continue_tail();
        CHECK_EQ_INT((int)DSW(DS_000F0A64), 3);
        CHECK_EQ_INT((int)DSB(DS_000F0A71), 0);

        DSD(DS_001088D8) = 0x10000000u | 0x00002000u;
        frontend_continue_tail();
        CHECK_EQ_INT((int)DSW(DS_000F0A64), 5);
        CHECK_EQ_INT((int)DSB(DS_000F0A71), 1);
        CHECK_EQ_INT((int)DSW(DS_000F0A6C), 3);
        CHECK_EQ_INT((int)DSB(DS_00104B15), 0x9A);

        /* Byte-2 branch: state and DS_000F0A6C become 5. */
        DSB(DS_000F0A71) = 0;
        DSB(DS_00104B15) = 0x9A;
        DSB(DS_00104B19 + 2u) = 0x5A;
        DSW(DS_000F0A6C) = 3;
        DSW(DS_000F0A64) = 3;
        frontend_continue_tail();
        CHECK_EQ_INT((int)DSB(DS_00104B19 + 2u), 0);
        CHECK_EQ_INT((int)DSB(DS_00104B15), 0);
        CHECK_EQ_INT((int)DSW(DS_000F0A6C), 5);
        CHECK_EQ_INT((int)DSW(DS_000F0A64), 5);

        DSB(DS_000F0A71) = saved71;  DSD(DS_001088D8) = saved_d8;
        DSW(DS_000F0A64) = saved64;  DSW(DS_000F0A6C) = saved6c;
        DSB(DS_00104B15) = saved15;
        DSB(DS_00104B19 + 2u) = saved19b2;
    }

    /* 0x10EE4: 0x32970(0) (unported, no-op), 0x4F1E4 (DS_00104B15 = 0),
     * 0x2BAF4(1) (actors_reset, which zeroes DS_00104AD0), then the four state
     * writes. edx = 3 survives 0x4F1E4's push/pop and becomes DS_00104B00. */
    {
        const u16 saved_b00 = DSW(DS_00104B00);
        const u16 saved64 = DSW(DS_000F0A64);
        const u8  saved71 = DSB(DS_000F0A71);
        const u8  saved6f = DSB(DS_000F0A6F);
        const u8  saved15 = DSB(DS_00104B15);
        const u32 saved_ad0 = DSD(DS_00104AD0);

        DSW(DS_00104B00) = 0xAAAA;
        DSW(DS_000F0A64) = 0x1234;
        DSB(DS_000F0A71) = 0x77;
        DSB(DS_000F0A6F) = 0x55;
        DSB(DS_00104B15) = 0x9A;
        DSD(DS_00104AD0) = 0xFFFFFFFFu;   /* 0x2BAF4 must clear it */

        attract_state_reset();

        CHECK_EQ_INT((int)DSW(DS_00104B00), 3);
        CHECK_EQ_INT((int)DSW(DS_000F0A64), 0);
        CHECK_EQ_INT((int)DSB(DS_000F0A71), 0);
        CHECK_EQ_INT((int)DSB(DS_000F0A6F), 0);
        CHECK_EQ_INT((int)DSB(DS_00104B15), 0);   /* 0x4F1E4 */
        /* 0x2BAF4 clears DS_00104AD0 only once the actor pool exists;
         * actors_reset() is a documented no-op without it (the shared suite's
         * test_actors provides the pool, the isolated PR_ATTRACT_DUMP run does
         * not run before this check). */
        if (DSD(DS_001014F4) != 0)
            CHECK_EQ_INT((int)DSD(DS_00104AD0), 0);   /* 0x2BAF4 */

        DSW(DS_00104B00) = saved_b00;  DSW(DS_000F0A64) = saved64;
        DSB(DS_000F0A71) = saved71;    DSB(DS_000F0A6F) = saved6f;
        DSB(DS_00104B15) = saved15;    DSD(DS_00104AD0) = saved_ad0;
    }

    /* 0x2C8F0(eax = -2): scale = config_field_get(0x2A) & 3; the music volume
     * DS_000A2CB8 = (m * scale / 3) >> 1 from field 0x35, the SFX volume
     * DS_000A2CB4 from field 0x37, with 8 / 0x10 for the -1 sentinel. */
    {
        const u32 saved2a = config_field_get(0x2Au);
        const u32 saved35 = config_field_get(0x35u);
        const u32 saved37 = config_field_get(0x37u);
        const u32 saved_b8 = DSD(DS_000A2CB8);
        const u32 saved_b4 = DSD(DS_000A2CB4);

        /* scale 3, m 0x2A00 (10752), s 0x00A0 (160):
         *   0x2A00*3/3 = 0x2A00, >>1 = 0x1500
         *   0xA0*3/3   = 0xA0,   >>1 = 80 = 0x50. */
        config_field_set(0x2Au, 3u);
        config_field_set(0x35u, 0x2A00u);
        config_field_set(0x37u, 0x00A0u);
        CHECK_EQ_INT((int)(config_field_get(0x2Au) & 3u), 3);
        CHECK_EQ_INT((int)config_field_get(0x35u), 0x2A00);
        CHECK_EQ_INT((int)config_field_get(0x37u), 0x00A0);
        attract_config_volumes();
        CHECK_EQ_INT((int)DSD(DS_000A2CB8), 0x1500);
        CHECK_EQ_INT((int)DSD(DS_000A2CB4), 0x50);

        /* scale 2, m 0x2A01 (10753), s 0x00A1 (161):
         *   10753*2 = 21506, /3 = 7168 (trunc), >>1 = 3584
         *   161*2   = 322,   /3 = 107 (trunc),  >>1 = 53. */
        config_field_set(0x2Au, 2u);
        config_field_set(0x35u, 0x2A01u);
        config_field_set(0x37u, 0x00A1u);
        attract_config_volumes();
        CHECK_EQ_INT((int)DSD(DS_000A2CB8), 3584);
        CHECK_EQ_INT((int)DSD(DS_000A2CB4), 53);

        /* scale 0 zeroes both. */
        config_field_set(0x2Au, 0u);
        config_field_set(0x35u, 0x2A00u);
        config_field_set(0x37u, 0x00A0u);
        attract_config_volumes();
        CHECK_EQ_INT((int)DSD(DS_000A2CB8), 0);
        CHECK_EQ_INT((int)DSD(DS_000A2CB4), 0);

        /* scale 3, field 1: (1*3)/3 = 1, >>1 = 0. */
        config_field_set(0x2Au, 3u);
        config_field_set(0x35u, 1u);
        config_field_set(0x37u, 1u);
        attract_config_volumes();
        CHECK_EQ_INT((int)DSD(DS_000A2CB8), 0);
        CHECK_EQ_INT((int)DSD(DS_000A2CB4), 0);

        config_field_set(0x2Au, saved2a);
        config_field_set(0x35u, saved35);
        config_field_set(0x37u, saved37);
        DSD(DS_000A2CB8) = saved_b8;
        DSD(DS_000A2CB4) = saved_b4;
    }

    /* 0x10F28: two signed 16-bit countdowns. Each reload is rng_next(N) + N
     * with N = 0x2D / 0x3C; the second branch also consumes rng_next(2) for the
     * stubbed voice, so skipping it would shift the stream. */
    {
        const u16 saved60 = DSW(DS_000F0A60);
        const u16 saved62 = DSW(DS_000F0A62);

        /* Neither expires: both decrement, no draw (values stay deterministic). */
        rng_seed(0xABCDu);
        DSW(DS_000F0A60) = 5;
        DSW(DS_000F0A62) = 5;
        attract_voice_tick();
        CHECK_EQ_INT((int)DSW(DS_000F0A60), 4);
        CHECK_EQ_INT((int)DSW(DS_000F0A62), 4);
        CHECK_EQ_INT((int)rng_next(0x2Du), 6);   /* stream untouched by the tick */

        /* Both expire from 1. Seed 0xABCD: rng_next(0x2D) = 6 -> 0x33;
         * rng_next(2) = 1 (discarded; without it the next draw is 52 -> 0x70);
         * rng_next(0x3C) = 6 -> 0x42. */
        rng_seed(0xABCDu);
        DSW(DS_000F0A60) = 1;
        DSW(DS_000F0A62) = 1;
        attract_voice_tick();
        CHECK_EQ_INT((int)DSW(DS_000F0A60), 0x33);
        CHECK_EQ_INT((int)DSW(DS_000F0A62), 0x42);

        /* Only the first expires: one draw, the second countdown just decrements. */
        rng_seed(0xABCDu);
        DSW(DS_000F0A60) = 1;
        DSW(DS_000F0A62) = 4;
        attract_voice_tick();
        CHECK_EQ_INT((int)DSW(DS_000F0A60), 0x33);
        CHECK_EQ_INT((int)DSW(DS_000F0A62), 3);

        /* Zero and 0xFFFF (=-1 signed) both take the signed `<= 0` reload. */
        rng_seed(0xABCDu);
        DSW(DS_000F0A60) = 0;
        DSW(DS_000F0A62) = 0;
        attract_voice_tick();
        CHECK_EQ_INT((int)DSW(DS_000F0A60), 0x33);
        CHECK_EQ_INT((int)DSW(DS_000F0A62), 0x42);

        rng_seed(0xABCDu);
        DSW(DS_000F0A60) = 0xFFFF;
        DSW(DS_000F0A62) = 0xFFFF;
        attract_voice_tick();
        CHECK_EQ_INT((int)DSW(DS_000F0A60), 0x33);
        CHECK_EQ_INT((int)DSW(DS_000F0A62), 0x42);

        DSW(DS_000F0A60) = saved60;
        DSW(DS_000F0A62) = saved62;
        rng_seed(0xABCDu);   /* the game's canonical seed (0x20C10) */
    }

    /* 0x292AC: one call per set bit of DS_00104AD0, at table entry
     * DS_000A8744 + i*4. The shipped table's two callees (0x4F7F4 at i = 0 and
     * 0x5D812, `xor eax,eax; ret`, for every other slot) both ignore the raw's
     * eax = i argument, so the port's no-arg fn_resolve call is faithful. */
    {
        const u32 slot0 = DSD(DS_000A8744);
        const u32 slot1 = DSD(DS_000A8744 + 4u);
        const u32 slot2 = DSD(DS_000A8744 + 8u);
        const u32 saved_mask = DSD(DS_00104AD0);

        /* The shipped table, from the LE load of PRAGE.EXE. */
        CHECK_EQ_INT((int)slot0, 0x4F7F4);
        CHECK_EQ_INT((int)slot1, 0x5D812);
        CHECK_EQ_INT((int)slot2, 0x5D812);

        fn_register(0xF00D0u, scene_probe0);
        fn_register(0xF00D4u, scene_probe1);
        DSD(DS_000A8744) = 0xF00D0u;
        DSD(DS_000A8744 + 4u) = 0xF00D4u;
        g_scene_probe[0] = g_scene_probe[1] = 0;

        DSD(DS_00104AD0) = 0;                    /* no bits: nothing runs */
        attract_scene_tick();
        CHECK_EQ_INT(g_scene_probe[0], 0);
        CHECK_EQ_INT(g_scene_probe[1], 0);

        DSD(DS_00104AD0) = 1u;                   /* bit 0 -> entry 0 */
        attract_scene_tick();
        CHECK_EQ_INT(g_scene_probe[0], 1);
        CHECK_EQ_INT(g_scene_probe[1], 0);

        DSD(DS_00104AD0) = 2u;                   /* bit 1 -> entry at i = 4 */
        attract_scene_tick();
        CHECK_EQ_INT(g_scene_probe[0], 1);
        CHECK_EQ_INT(g_scene_probe[1], 1);

        DSD(DS_00104AD0) = 3u;                   /* both bits */
        attract_scene_tick();
        CHECK_EQ_INT(g_scene_probe[0], 2);
        CHECK_EQ_INT(g_scene_probe[1], 2);

        DSD(DS_000A8744) = slot0;
        DSD(DS_000A8744 + 4u) = slot1;
        DSD(DS_00104AD0) = saved_mask;
    }

    /* 0x11000 phase 0xC: countdown, then hand to DS_000F0A70.
     * 0x11000 phase 0xB: DS_000F0A48 = 0, DS_000F0A6F = 0, and the state handoff.
     * Both phases fall through to the 0x11550 tail, which only runs the 0x10F28
     * voice scheduler when DS_0009AD58 == 0. */
    {
        const u8 saved6f = DSB(DS_000F0A6F);
        const u8 saved6e = DSB(DS_000F0A6E);
        const u16 saved68 = DSW(DS_000F0A68);
        const u8 saved70 = DSB(DS_000F0A70);
        const u8 saved5c = DSB(DS_000F0A5C);
        const u16 saved64 = DSW(DS_000F0A64);
        const u32 saved48 = DSD(DS_000F0A48);
        const u8 saved73 = DSB(DS_00108173);
        const u16 saved60 = DSW(DS_000F0A60);
        const u16 saved62 = DSW(DS_000F0A62);
        const u8  saved58 = DSB(DS_0009AD58);
        const u32 saved_scratch = DSD(0x3F00000u + 0x24u);

        DSB(DS_0009AD58) = 1;   /* suppress the tail's rng draw while testing */

        DSB(DS_000F0A6F) = 0xC; DSB(DS_000F0A70) = 3; DSW(DS_000F0A68) = 1;
        attract_step();
        CHECK_EQ_INT((int)DSW(DS_000F0A68), 0);
        CHECK_EQ_INT((int)DSB(DS_000F0A6F), 0xC);      /* original 1 -> not yet < 1 */
        attract_step();
        CHECK_EQ_INT((int)DSB(DS_000F0A6F), 3);        /* original 0 -> hand off */

        DSB(DS_000F0A6F) = 0xB; DSB(DS_00108173) = 0; DSB(DS_000F0A5C) = 0;
        DSB(DS_000F0A64) = 0xFF;
        attract_step();
        CHECK_EQ_INT((int)DSD(DS_000F0A48), 0);
        CHECK_EQ_INT((int)DSB(DS_000F0A6F), 0);
        CHECK_EQ_INT((int)DSW(DS_000F0A64), 1);

        /* 0x11000 phase 8: DS_000F0A4C+0x24 clear -> set the 0xC countdown and
         * target 0xC. DS_000F0A4C is pointed at a zeroed scratch cell so no
         * loaded scene is needed. */
        {
            const u32 saved4c = DSD(DS_000F0A4C);
            DSD(DS_000F0A4C) = 0x3F00000u;   /* free scratch near the top of mem[] */
            DSD(0x3F00000u + 0x24u) = 0;
            DSB(DS_000F0A6F) = 8;
            DSW(DS_000F0A68) = 0;
            DSB(DS_000F0A70) = 0;
            attract_step();
            CHECK_EQ_INT((int)DSW(DS_000F0A68), 0x40);
            CHECK_EQ_INT((int)DSB(DS_000F0A70), 9);
            CHECK_EQ_INT((int)DSB(DS_000F0A6F), 0xC);
            DSD(DS_000F0A4C) = saved4c;
        }

        /* restore */
        DSB(DS_000F0A6F) = saved6f; DSB(DS_000F0A6E) = saved6e;
        DSW(DS_000F0A68) = saved68; DSB(DS_000F0A70) = saved70;
        DSB(DS_000F0A5C) = saved5c; DSW(DS_000F0A64) = saved64;
        DSD(DS_000F0A48) = saved48; DSB(DS_00108173) = saved73;
        DSW(DS_000F0A60) = saved60; DSW(DS_000F0A62) = saved62;
        DSB(DS_0009AD58) = saved58;
        DSD(0x3F00000u + 0x24u) = saved_scratch;
    }

    /* 0x11000 spawn phases 3/5/6/7/0xA. actor_spawn's register binding is
     * pinned as a2 = EDX, a3 = ECX, a4 = EBX, a5 = stack (docs/.../
     * 2026-09-17-actor-system-args.md); the record stores a2 at +0x18, a4 at
     * +0x1c and -- for these descriptors, whose flags word has bit 0x2000 --
     * the (u8)a3 layer at +0x49. This is the permanent guard for the phase
     * 5/6/7 mis-slotting that put 0xE2/0xE6/0xE8 in EBX and left ECX zero.
     * Needs the actor pool, which res_load_index provides in the shared suite
     * (test_actors); the isolated PR_ATTRACT_DUMP run loads no INDEX, so this
     * block is skipped there exactly like the pool-dependent attract checks. */
    if (DSD(DS_001014F4) != 0) {
        const u8  saved6f = DSB(DS_000F0A6F);
        const u8  saved58 = DSB(DS_0009AD58);
        const u32 saved50 = DSD(DS_000F0A50);
        const u32 saved4c = DSD(DS_000F0A4C);

        DSB(DS_0009AD58) = 1;   /* suppress the tail's rng draw */
        actors_reset();

        /* Phase 3: desc 0x9ACCC, EDX = 0x2A00 (a2), ECX = 0xE0 (a3),
         * EBX = 0x1E00 (a4), stack = 0. Raw 0x11199-0x111AF. */
        DSB(DS_000F0A6F) = 3;
        attract_step();
        u32 r3 = DSD(DS_000F0A50);
        CHECK(r3 != 0, "phase 3 spawned a record");
        if (r3 != 0) {
            CHECK_EQ_INT((int)DSD(r3 + 0x18), 0x2A00);   /* a2 */
            CHECK_EQ_INT((int)DSD(r3 + 0x1c), 0x1E00);   /* a4 (EBX) */
            CHECK_EQ_INT((int)DSB(r3 + 0x49), 0xE0);     /* a3 (ECX) */
        }

        /* Phases 5/6/7 respawn only while the pointed record's +0x24 is clear;
         * seed a zero-+0x24 record each time. */
        u32 seed = r3;
        DSB(DS_000F0A6F) = 5;
        if (seed != 0) DSD(seed + 0x24) = 0;
        DSD(DS_000F0A50) = seed;
        attract_step();
        u32 r5 = DSD(DS_000F0A50);
        CHECK(r5 != 0 && r5 != seed, "phase 5 spawned a new record");
        if (r5 != 0) {
            CHECK_EQ_INT((int)DSD(r5 + 0x1c), 0);        /* a4 = EBX = 0 */
            CHECK_EQ_INT((int)DSB(r5 + 0x49), 0xE2);     /* a3 = ECX = 0xE2 */
        }

        seed = r5;
        DSB(DS_000F0A6F) = 6;
        if (seed != 0) DSD(seed + 0x24) = 0;
        DSD(DS_000F0A50) = seed;
        attract_step();
        u32 r6 = DSD(DS_000F0A50);
        CHECK(r6 != 0 && r6 != seed, "phase 6 spawned a new record");
        if (r6 != 0) {
            CHECK_EQ_INT((int)DSD(r6 + 0x1c), 0);        /* a4 = EBX = 0 */
            CHECK_EQ_INT((int)DSB(r6 + 0x49), 0xE6);     /* a3 = ECX = 0xE6 */
        }

        seed = r6;
        DSB(DS_000F0A6F) = 7;
        if (seed != 0) DSD(seed + 0x24) = 0;
        DSD(DS_000F0A50) = seed;
        attract_step();
        u32 r7 = DSD(DS_000F0A4C);
        CHECK(r7 != 0, "phase 7 spawned a record into DS_000F0A4C");
        if (r7 != 0) {
            CHECK_EQ_INT((int)DSD(r7 + 0x1c), 0);        /* a4 = EBX = 0 */
            CHECK_EQ_INT((int)DSB(r7 + 0x49), 0xE8);     /* a3 = ECX = 0xE8 */
        }

        /* Phase 0xA: desc 0x9AD30, a2 = high word of DSD(0x9ACC4) = 0x3A00,
         * a3 = ECX = 0xD0, a4 = high word of DSD(0x9ACC6) = 0 (raw
         * 0x11478-0x11496). The return value is discarded, so read the new
         * active-list head. */
        DSB(DS_000F0A6F) = 0xA;
        attract_step();
        u32 rA = actor_list_head();
        CHECK(rA != 0, "phase 0xA spawned a record");
        if (rA != 0) {
            CHECK_EQ_INT((int)DSD(rA + 0x18), 0x3A00);   /* a2 */
            CHECK_EQ_INT((int)DSD(rA + 0x1c), 0);        /* a4 (EBX) */
            CHECK_EQ_INT((int)DSB(rA + 0x49), 0xD0);     /* a3 (ECX) */
        }

        actors_reset();
        DSB(DS_000F0A6F) = saved6f; DSB(DS_0009AD58) = saved58;
        DSD(DS_000F0A50) = saved50; DSD(DS_000F0A4C) = saved4c;
    }

    /* PR_ATTRACT_DUMP: the continuous state-0 run. game_init() may run once per
     * process, so the attract and the title share this one run — the attract is
     * dumped to <dir>/attract/ by game_attract_dump_frame, and the title window
     * to <dir>/title/ because game_title_dump_frame falls back to
     * PR_ATTRACT_DUMP. run_tests.c runs this file alone when the env var is set.
     * The phase sequence, the DS_000F0A5C wrap, the state-1 handoff and a
     * per-frame hash log are asserted here; run-to-run log stability is two
     * invocations + diff (the Task 1/4 determinism pattern). */
    {
        const char *dump = getenv("PR_ATTRACT_DUMP");
        if (dump != NULL && dump[0] != '\0') {
            const char *dir = getenv("PR_GAME_DIR");
            if (dir == NULL || dir[0] == '\0') dir = "data/game/C";
            game_set_game_dir(dir);
            game_init();
            actors_pin_anim_tick_zero(1);

            mkdir(dump, 0777);      /* game_attract_dump_frame also makes it */
            char log_path[1200];
            snprintf(log_path, sizeof log_path, "%s/attract.log", dump);
            FILE *log = fopen(log_path, "w");
            CHECK(log != NULL, "attract hash log opens");

            /* The boot cycle runs phases 0..9, 0xC and 0xB. Phase 0xA is the
             * other arm of phase 9's DS_000F0A5C branch: at boot phase 2 wraps
             * DS_000F0A5C 4 -> 0, so phase 9's `== 0` arm sets DS_000F0A70 = 0xB
             * and 0xA is skipped until a later cycle. */
            const u32 expected_phases = 0x3FFu | (1u << 0xBu) | (1u << 0xCu);
            u32 phase_mask = 0;     /* phases seen */
            u32 fivec_mask = 0;     /* DS_000F0A5C values seen */
            int frames = 0;
            int guard = 0;
            /* The handoff iteration ends with DS_000F0A64 == 1 but ran the
             * attract; the title entry is the next iteration. */
            while (DSW(DS_000F0A64) != 1 && guard++ < 200000) {
                u8 ph = DSB(DS_000F0A6F);
                if (ph <= ATTRACT_PHASE_MAX) phase_mask |= 1u << ph;
                if (DSB(DS_000F0A5C) < 32u) fivec_mask |= 1u << DSB(DS_000F0A5C);
                DSB(DS_000A81A8) = 1;   /* exactly one game_loop iteration */
                game_loop();
                if (log != NULL) {
                    const u8 *fb = mem + DSD(DS_000E87A4);
                    fprintf(log, "%d %u %u\n", frames, (unsigned)ph,
                            attract_frame_hash(fb));
                }
                frames++;
            }
            if (log != NULL) fclose(log);

            CHECK(guard < 200000, "attract reached the handoff within the bound");
            CHECK_EQ_INT((int)DSW(DS_000F0A64), 1);
            CHECK_EQ_INT((int)phase_mask, (int)expected_phases);
            CHECK((fivec_mask & (1u << 4)) != 0u, "DS_000F0A5C starts at 4");
            CHECK((fivec_mask & 1u) != 0u, "DS_000F0A5C wraps to 0");

            /* The title window joins the same run (no second game_init()). */
            test_title_window(dump);
            game_shutdown();
        }
    }

    return g_failures - before;
}
