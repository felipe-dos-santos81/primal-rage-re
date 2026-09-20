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

/* 0x292AC dispatch probes. The addresses are outside the code object, so they
 * cannot collide with a real original function. */
static int g_scene_probe[2];
static void scene_probe0(void) { g_scene_probe[0]++; }
static void scene_probe1(void) { g_scene_probe[1]++; }

int test_attract(void)
{
    int before = g_failures;

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

    return g_failures - before;
}
