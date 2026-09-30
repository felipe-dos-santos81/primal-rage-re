/* test_game.c — the game suite.
 *
 * Consolidated from: test_flow.c, test_actors.c, test_effects.c, test_config.c, test_anim.c, test_frontend.c, test_attract.c, test_title.c.
 * Every assertion is carried verbatim; only the file's home and the
 * two cross-file static names (none) changed. */

#include "game/flow.h"
#include "game/actors.h"
#include "game/rng.h"
#include "host.h"
#include "mem.h"
#include "symbols.h"
#include "platform/gfx.h"
#include "platform/audio/ail.h"
#include "platform/audio/mixer.h"
#include "platform/audio/patches.h"
#include "platform/audio/sequencer.h"
#include "platform/render.h"
#include "platform/res.h"
#include "platform/sprite.h"
#include "test.h"
#include "game/effects.h"
#include "test_fixtures.h"
#include "game/config.h"
#include "platform/input.h"
#include "game/fight.h"
#include "game/fighter.h"
#include "game/attract.h"
#include "game/movie.h"
#include "game/nameentry.h"
#include "game/svcmenu.h"
#include "game/menu.h"
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <dirent.h>


/* ---- test_flow.c ---- */

static int g_called;

static void probe_task(void) { g_called++; }

/* Negative control for the music-bank size guard. A FORM/XMID placed at offset
 * 248 with fsz = 0xFFFFFF00 makes the old `i + 8 + fsz > size` u32 sum wrap to 0
 * and pass; the wrap-free guard must reject it. A well-formed container in the
 * same buffer must still be found. */
static void check_bank_guard(void)
{
    static u8 buf[512];
    memset(buf, 0, sizeof buf);
    u8 *p = buf + 248;
    memcpy(p, "FORM", 4);
    p[4] = 0xFF; p[5] = 0xFF; p[6] = 0xFF; p[7] = 0x00;   /* fsz = 0xFFFFFF00 */
    memcpy(p + 8, "XMID", 4);
    CHECK(game_music_bank_find(buf, sizeof buf) == NULL,
          "wrapping FORM size is rejected");

    p[4] = 0x00; p[5] = 0x00; p[6] = 0x00; p[7] = 0x10;   /* fsz = 16 */
    CHECK(game_music_bank_find(buf, sizeof buf) == p,
          "well-formed FORM/XMID is found");

    memset(buf, 0, sizeof buf);
    CHECK(game_music_bank_find(buf, sizeof buf) == NULL,
          "absent FORM/XMID is rejected");
}

/* The 0x47370/0x1C500/0x474E4 localisation reader over the shipped ENGLISH.TXT:
 * 0x121A0's caption is string 0x15, so pin the decode of that id and two
 * neighbours (a non-zero length-XOR, and a plain string) against the file. */
static void check_localised_string(void)
{
    game_string_table_load("data/game/C");
    CHECK(strcmp((const char *)game_string_get(0x15u), "THE FUTURE...") == 0,
          "ENGLISH.TXT id 0x15 is the title caption");
    CHECK(strcmp((const char *)game_string_get(0x14u), "THE NEW URTH?") == 0,
          "ENGLISH.TXT id 0x14 decodes");
    CHECK(strcmp((const char *)game_string_get(0x01u),
                 "TM   1994 ATARI GAMES CORP.") == 0,
          "ENGLISH.TXT id 0x01 decodes");
}

/* Task 3: 0x2BF08's per-frame message line. Drive the exported tail entry
 * directly and assert on the text grid DS_00105F38 and the latch DS_00105C04,
 * not on rendering. Text row 1 is the captured DS_00105C05 (screen row 7). */
static int overlay_row_cells(int row)
{
    int n = 0;
    for (int c = 0; c < 43; c++)
        if (DSD(DS_00105F38 + (u32)row * 0xacu + (u32)c * 4u) != 0) n++;
    return n;
}

static void check_title_overlay(void)
{
    DSB(DS_00105C05) = 1;       /* captured text row (screen rows 7-12) */
    DSD(DS_000EF6DC) = 0;
    DSB(DS_00105D60) = 0;
    DSD(DS_00105C00) = 5;       /* captured credit count */
    DSB(DS_00105C04) = 0;
    DSB(DS_0009AD58) = 0;
    for (u32 i = 0; i + 4u <= 0x14D4u; i += 4u) DSD(DS_00105F38 + i) = 0;

    /* DS_0009AD58 != 0 is the one true early return: nothing drawn or latched. */
    DSB(DS_0009AD58) = 1;
    DSB(DS_00105C04) = 0x5a;
    game_overlay_step();
    CHECK_EQ_INT(DSB(DS_00105C04), 0x5a);
    CHECK_EQ_INT(overlay_row_cells(1), 0);
    DSB(DS_0009AD58) = 0;

    /* DS_00105D60 != 0: FREE PLAY. &0x20 picks hold (spawn) vs release. */
    DSB(DS_00105D60) = 1;
    DSB(DS_00105C04) = 0;
    DSD(DS_000EF6DC) = 0x20;
    game_overlay_step();
    CHECK(overlay_row_cells(1) > 0, "FREE PLAY &0x20 holds its line");
    DSD(DS_000EF6DC) = 0x00;
    game_overlay_step();
    CHECK_EQ_INT(overlay_row_cells(1), 0);  /* release clears the same cells */

    /* DS_00105D60 == 0, DS_00105C00 != 0: CREDITS, ungated by &0x1f. */
    DSB(DS_00105D60) = 0;
    DSD(DS_000EF6DC) = 0x1f;
    game_overlay_step();
    CHECK(overlay_row_cells(1) > 0, "CREDITS drawn on a &0x1f frame");
    CHECK_EQ_INT((int)DSW(DS_00105F34), 1);  /* message cursor row */
    CHECK_EQ_INT(DSB(DS_00105C04), 0);       /* latch cleared */
    for (u32 i = 0; i + 4u <= 0x14D4u; i += 4u) DSD(DS_00105F38 + i) = 0;

    /* DS_00105C00 == 0, &0x1f != 0, latch clear: skip. */
    DSD(DS_00105C00) = 0;
    DSD(DS_000EF6DC) = 0x1f;
    DSB(DS_00105C04) = 0;
    game_overlay_step();
    CHECK_EQ_INT(overlay_row_cells(1), 0);

    /* DS_00105C00 == 0, &0x1f == 0, &0x20 != 0: INSERT COINS hold. */
    DSD(DS_000EF6DC) = 0x20;
    game_overlay_step();
    CHECK(overlay_row_cells(1) > 0, "INSERT COINS &0x20 holds its line");
    CHECK_EQ_INT(DSB(DS_00105C04), 0);

    /* Latch set with &0x1f != 0 falls through to the release arm. */
    DSB(DS_00105C04) = 1;
    DSD(DS_000EF6DC) = 0x1f;
    game_overlay_step();
    CHECK_EQ_INT(overlay_row_cells(1), 0);
}

/* Task 9: the demo's state-9 hold is a pure countdown. The raw's 0x11D04 case
 * 9 decrements DS_000F0A6A and, on the frame whose PRE value is 1, restores
 * DS_000F0A64 = DS_000F0A6C (the state-6 entry); it draws no RNG (verified
 * against the fixed-up image at 0x11D04's case-9 arm). That matters to the demo
 * window: the port's RNG stream position at state 6 is the raw's only if state 9
 * consumes no draw, and the first unexplained demo frame (capture 811) is inside
 * this hold, so a draw here would be a determinism site and not a render gap.
 * The two LCG-state assertions fail if case 9 ever draws. */
static void check_state9_countdown(void)
{
    /* Save and restore every global this seeds: the following state-6 block
     * asserts DS_000F0A64 == 7, and a leaked DS_000F0A71 == 1 would make
     * game_state_step's two per-state tails short-circuit for it (they are gated
     * on that flag), so the state-6 assertions could pass for the wrong reason. */
    const u32 saved_rng = DSD(DS_000EF6D8);
    const u8  saved_b1d = DSB(DS_00104B1D);
    const u8  saved_a71 = DSB(DS_000F0A71);
    const u16 saved_64  = DSW(DS_000F0A64);
    const u16 saved_6a  = DSW(DS_000F0A6A);
    const u16 saved_6c  = DSW(DS_000F0A6C);

    DSB(DS_00104B1D) = 1;          /* skip the deferred coin poll */
    DSB(DS_000F0A71) = 1;          /* both per-state tails return immediately */
    DSW(DS_000F0A64) = 9;
    DSW(DS_000F0A6A) = 3;          /* pre 3 -> new 2: no transition */
    DSW(DS_000F0A6C) = 6;
    game_state_step();
    CHECK_EQ_INT((int)DSW(DS_000F0A6A), 2);
    CHECK_EQ_INT((int)DSW(DS_000F0A64), 9);
    CHECK_EQ_INT((int)DSD(DS_000EF6D8), (long)saved_rng);

    DSW(DS_000F0A64) = 9;
    DSW(DS_000F0A6A) = 1;          /* pre 1 -> new 0: restore the target */
    game_state_step();
    CHECK_EQ_INT((int)DSW(DS_000F0A6A), 0);
    CHECK_EQ_INT((int)DSW(DS_000F0A64), 6);
    CHECK_EQ_INT((int)DSD(DS_000EF6D8), (long)saved_rng);

    DSD(DS_000EF6D8) = saved_rng;
    DSB(DS_00104B1D) = saved_b1d;
    DSB(DS_000F0A71) = saved_a71;
    DSW(DS_000F0A64) = saved_64;
    DSW(DS_000F0A6A) = saved_6a;
    DSW(DS_000F0A6C) = saved_6c;
}

/* Task 7: the demo's timer exit, 0x11D04 case 7 -> 0x11BCC. The raw's case-7
 * arm (0x11E67) stores DS_000F0A6A = timer-1 and, when the new value is 0
 * (0x11E72), calls 0x11BCC: clear DS_00104B19+2 (0x11BCE), clear DS_00104B15
 * (0x11BD4), hand DS_000F0A64 = DS_000F0A6C (0x11BE0). Record §7.6 Input A pins
 * the exit frame as DS_000F0A6A = 1 (the frame whose PRE value is 1) with
 * DS_000F0A6C = 0, so the handoff is state 0 — the attract sub-machine (§4.2).
 * The 0x263F4 arena frame must not run on the exit frame: DS_00107EE0 is the
 * word its first callee 0x3C5CC zeroes, seeded with a sentinel that differs from
 * the post-condition, so a spurious arena call fails rather than passing on a
 * zero the test never touched. */
static void check_timer_exit(void)
{
    const u16 saved_64  = DSW(DS_000F0A64);
    const u16 saved_6a  = DSW(DS_000F0A6A);
    const u16 saved_6c  = DSW(DS_000F0A6C);
    const u8  saved_b15 = DSB(DS_00104B15);
    const u8  saved_b1b = DSB(DS_00104B19 + 2u);
    const u8  saved_b1d = DSB(DS_00104B1D);
    const u8  saved_a71 = DSB(DS_000F0A71);
    const u32 saved_ee0 = DSD(DS_00107EE0);

    DSB(DS_00104B1D) = 1;          /* skip the deferred coin poll */
    DSB(DS_000F0A71) = 1;          /* both per-state tails return immediately */
    DSW(DS_000F0A64) = 7;          /* the state-7 arm */
    DSW(DS_000F0A6A) = 1;          /* record §7.6 Input A: pre 1 -> new 0 */
    DSW(DS_000F0A6C) = 0;          /* the demo's chain target (record §4.2) */
    DSB(DS_00104B15) = 1;
    DSB(DS_00104B19 + 2u) = 1;
    DSD(DS_00107EE0) = 0x5EEDu;    /* the 0x263F4 sentinel */

    game_state_step();

    CHECK_EQ_INT((int)DSW(DS_000F0A6A), 0);          /* 0x11E67 */
    CHECK_EQ_INT((int)DSW(DS_000F0A64), 0);          /* 0x11BE0: DS_000F0A6C */
    CHECK_EQ_INT((int)DSB(DS_00104B15), 0);          /* 0x11BD4 */
    CHECK_EQ_INT((int)DSB(DS_00104B19 + 2u), 0);     /* 0x11BCE */
    CHECK_EQ_INT((int)DSD(DS_00107EE0), 0x5EED);     /* 0x263F4 did not run */

    DSW(DS_000F0A64) = saved_64;
    DSW(DS_000F0A6A) = saved_6a;
    DSW(DS_000F0A6C) = saved_6c;
    DSB(DS_00104B15) = saved_b15;
    DSB(DS_00104B19 + 2u) = saved_b1b;
    DSB(DS_00104B1D) = saved_b1d;
    DSB(DS_000F0A71) = saved_a71;
    DSD(DS_00107EE0) = saved_ee0;
}

/* symbols.h emits no name for the 0x387F4/0x38890 scene-descriptor tables. */
#define TEST_DS_000BDF7C 0x000BDF7Cu

/* Task 3: 0x38730's projection setup. Every input and output is seeded with a
 * sentinel that differs from its post-condition. The anchors are the shipped
 * bytes: 0xBDE1C = {0xA4,0xA4,0x94,...}, 0xBDE2C[0..1] = 0, 0xBDE0C =
 * {0x0B00,0x0700,...}, 0xBDDFC = {0x0060,0x0050,...}, 0xA8A18 = {3,7,...},
 * 0xA8A20 = {1,1,...}. 0xBDF7C[0] -> descriptor 0xBDE50 -> sprite id 0x2BE0 ->
 * s16beach.gra+0x38CF8 (w=549, h=213); 0xBDF7C[1] -> 0xBDEB4 -> 0x2BEA ->
 * s16volcn.gra+0x3988C (w=488, h=213). Both heights are positive, so the raw
 * keeps EBX = i (0x38820 JGE skips the 0x38822 MOV EBX,EAX) and the two cases
 * differ only in their table rows and their i. */
static void check_scroll_setup(void)
{
    const u16 saved[15] = {
        DSW(DS_00107A38), DSW(DS_00107A3A), DSW(DS_00107A3E),
        DSW(DS_00107A40), DSW(DS_00107A42), DSW(DS_00107A44 + 2),
        DSW(DS_00107A48), DSW(DS_00107A4A), DSW(DS_00107A4C),
        DSW(DS_00107A4E), DSW(DS_00107A50), DSW(DS_00107A52),
        DSW(DS_00107A54), DSW(DS_00107A55), DSW(DS_00107A56),
    };
    const u32 saved_f0 = DSD(DS_000F0AF0);
    const u32 saved_bc = DSD(DS_000BDFBC);
    const u32 saved_c0 = DSD(DS_000BDFC0);
    const u16 saved_shear0 = DSW(DS_00107900 + 0u);
    const u16 saved_shear53 = DSW(DS_00107900 + 0x53u * 2u);

    DSD(DS_000F0AF0) = 0;               /* 0x20DF4 zeroes it before 0x38730 */
    DSW(DS_00107900 + 0u) = 0xDEAD;
    DSW(DS_00107900 + 0x53u * 2u) = 0xDEAD;
    DSB(DS_00107A54) = 0;
    DSB(DS_00107A55) = 0x99; DSB(DS_00107A56) = 0x99;
    DSW(DS_00107A4E) = 0x5678; DSW(DS_00107A42) = 0x5678;
    DSW(DS_00107A4A) = 0x5678; DSW(DS_00107A4C) = 0x5678;
    DSW(DS_00107A50) = 0x5678; DSW(DS_00107A40) = 0x5678;
    DSW(DS_00107A3A) = 0x5678; DSW(DS_00107A38) = 0x5678;
    DSW(DS_00107A44 + 2) = 0x5678;
    DSW(DS_00107A48) = 0x1234;          /* 0x38730 divides this pre-call value */
    DSD(DS_000BDFBC) = 0; DSD(DS_000BDFC0) = 0;

    render_scroll_setup(0u);

    CHECK_EQ_INT((int)DSW(DS_00107A4E), 0xA4);
    CHECK_EQ_INT((int)DSW(DS_00107A42), 0);
    CHECK_EQ_INT((int)DSW(DS_00107A50), 0x0B00);
    CHECK_EQ_INT((int)DSW(DS_00107A40), 0x60);
    CHECK_EQ_INT((int)DSW(DS_00107A4A), 0xA4);
    CHECK_EQ_INT((int)DSW(DS_00107A4C), 0xA4);
    CHECK_EQ_INT((int)DSB(DS_00107A55), 3);
    CHECK_EQ_INT((int)DSB(DS_00107A56), 1);
    CHECK_EQ_INT((int)DSW(DS_00107A52), 0x54);
    /* 0x387C6 divides the PRE-call DS_00107A48 (0x1234 / 64 = 0x48); 0x387F4
     * then writes it: h = 213 is non-negative, so the raw keeps EBX = i = 0 and
     * the value is (0 - 0xA4) << 6 = 0xD700. */
    CHECK_EQ_INT((int)DSW(DS_00107A38), 0x48);
    CHECK_EQ_INT((int)DSW(DS_00107A48), 0xD700);
    /* 0x38A38's tail with stride 0: DS_00107A3A = (0 + 0x0B00) / 32 = 0x58. */
    CHECK_EQ_INT((int)DSW(DS_00107A3A), 0x58);
    CHECK_EQ_INT((int)DSW(DS_00107A44 + 2), 0);
    CHECK_EQ_INT((int)DSW(DS_00107900 + 0u), 0);
    CHECK_EQ_INT((int)DSW(DS_00107900 + 0x53u * 2u), 0);
    /* edge = 0xA4 + 0x53 walks down to 0xA4, so the <= 0xEF rows store
     * (0 + 0x2B00) / 32 = 0x158. */
    CHECK_EQ_INT((int)DSW(DS_00107A3E), 0x158);
    CHECK_EQ_INT((int)DSB(DS_00107A54), 1);
    CHECK(DSD(DS_000BDFBC) != 0, "scene A actor spawned");
    CHECK(DSD(DS_000BDFC0) != 0, "scene B actor spawned");

    /* Scene 1: same height, different table rows. 0xBDE0C[1] = 0x0700 gives
     * DS_00107A3A = 0x0700 / 32 = 0x38, and (1 - 0xA4) << 6 = 0xD740. */
    DSW(DS_00107A48) = 0x1234;
    render_scroll_setup(1u);
    CHECK_EQ_INT((int)DSW(DS_00107A4E), 0xA4);
    CHECK_EQ_INT((int)DSW(DS_00107A50), 0x0700);
    CHECK_EQ_INT((int)DSW(DS_00107A40), 0x50);
    CHECK_EQ_INT((int)DSW(DS_00107A48), 0xD740);
    CHECK_EQ_INT((int)DSW(DS_00107A3A), 0x38);
    CHECK_EQ_INT((int)DSB(DS_00107A55), 7);
    CHECK_EQ_INT((int)DSB(DS_00107A54), 1);

    /* The negative-height arm: the raw negates (0x38822/0x38824) instead of
     * keeping i. The shipped first-set heights are all positive, so seed the
     * scene-0 sprite's s16 height to -0x100: (0x100 - 0xA4) << 6 = 0x1700. */
    {
        u32 handle = DSD(DS_000A8B30
                         + DSD(DSD(TEST_DS_000BDF7C)) * 4u);
        u8 *sp = (u8 *)res_resolve(handle);
        if (sp == NULL) {
            CHECK(0, "scene 0 sprite descriptor resolves");
        } else {
            u8 h0 = sp[2], h1 = sp[3];
            sp[2] = 0x00; sp[3] = 0xFF;     /* height = -0x100 */
            DSW(DS_00107A48) = 0x1234;
            render_scroll_setup(0u);
            CHECK_EQ_INT((int)DSW(DS_00107A48), 0x1700);
            sp[2] = h0; sp[3] = h1;
        }
    }

    DSW(DS_00107A38) = saved[0]; DSW(DS_00107A3A) = saved[1];
    DSW(DS_00107A3E) = saved[2]; DSW(DS_00107A40) = saved[3];
    DSW(DS_00107A42) = saved[4]; DSW(DS_00107A44 + 2) = saved[5];
    DSW(DS_00107A48) = saved[6]; DSW(DS_00107A4A) = saved[7];
    DSW(DS_00107A4C) = saved[8]; DSW(DS_00107A4E) = saved[9];
    DSW(DS_00107A50) = saved[10]; DSW(DS_00107A52) = saved[11];
    DSW(DS_00107A54) = saved[12]; DSW(DS_00107A55) = saved[13];
    DSW(DS_00107A56) = saved[14];
    DSD(DS_000F0AF0) = saved_f0;
    DSD(DS_000BDFBC) = saved_bc;
    DSD(DS_000BDFC0) = saved_c0;
    DSW(DS_00107900 + 0u) = saved_shear0;
    DSW(DS_00107900 + 0x53u * 2u) = saved_shear53;
}

/* ---- the voice dispatcher 0x2C3FC and the sound module (record §45-A) ---- */

static int sv_hook_n;
static void sv_hook(void) { sv_hook_n++; }

/* Clears entry e's loaded bit (0x20000000 in its +0xC). */
static void sv_unload(u32 e)
{
    DSD(DSD(DS_001014E0) + e * 20u + 12u) &= ~0x20000000u;
}

static int sv_loaded(u32 e)
{
    return (DSD(DSD(DS_001014E0) + e * 20u + 12u) & 0x20000000u) != 0u;
}

/* Seeds the sound module's state with sentinels that differ from every
 * post-condition asserted below: the DIG handle set, no pause, no sequence,
 * the current/pending song words, and each slot's queued (+0x04) and playing
 * (+0x0C) handles. */
static void sv_seed(void)
{
    DSD(DS_001028C8) = 1u;
    DSB(DS_001028DB) = 0;
    DSB(DS_001028DA) = 0;
    DSD(DS_001028C0) = 0;
    DSD(DS_001028CC) = 0x5E5E5E5Eu;
    DSD(DS_001028D4) = 0xD4D4D4D4u;
    DSB(DS_001028D9) = 0x99u;
    DSD(DS_00105D5C) = 0x5C5C5C5Cu;
    for (u32 i = 0; i < 4u; i++) {
        DSD(DS_00102864 + i * 0x18u) = 0x44440000u + i;
        DSD(DS_0010286C + i * 0x18u) = 0xCC000000u + i;
    }
    DSD(DS_001014FC) = 0;
    sv_hook_n = 0;
}

/* Forces slot i's AIL status: 2 (inited) or 4 (playing a short buffer). */
static void sv_status(u32 i, int st)
{
    static const u8 pcm[64] = { 0x80 };
    struct AIL_SAMPLE *h = sound_slot_handle(i);
    AIL_init_sample(h);
    if (st == 4) {
        AIL_set_sample_address(h, pcm, sizeof pcm);
        AIL_start_sample(h);
    }
    CHECK_EQ_INT((int)AIL_sample_status(h), st);
}

/* 0x1CED4 and the pause module 0x1D1B0/0x1D220/0x1D250/0x1D270 (record §50-D)
 * on the live handles. Every asserted post-condition differs from its seed. */
static void check_sound_pause_volume(void)
{
    u32 i;

    /* 0x1CED4: a new value is stored and reaches exactly the playing slots
     * (1 and 3); the same value again pushes nothing (slot 1 keeps 0x22). */
    sv_seed();
    for (i = 0; i < 4u; i++) sv_status(i, (i & 1u) ? 4 : 2);
    for (i = 0; i < 4u; i++) AIL_set_sample_volume(sound_slot_handle(i), 0x11);
    DSD(DS_000A2CB4) = 0x7Fu;
    sound_sfx_volume(0x33u);
    CHECK_EQ_INT((int)DSD(DS_000A2CB4), 0x33);
    for (i = 0; i < 4u; i++)
        CHECK_EQ_INT((int)AIL_sample_volume(sound_slot_handle(i)), (i & 1u) ? 0x33 : 0x11);
    AIL_set_sample_volume(sound_slot_handle(1u), 0x22);
    sound_sfx_volume(0x33u);
    CHECK_EQ_INT((int)AIL_sample_volume(sound_slot_handle(1u)), 0x22);
    sound_sfx_volume(0x40u);
    CHECK_EQ_INT((int)AIL_sample_volume(sound_slot_handle(1u)), 0x40);
    CHECK_EQ_INT((int)AIL_sample_volume(sound_slot_handle(0u)), 0x11);

    /* 0x1CAB8: stored when it differs; without a sequence handle nothing
     * plays and nothing else changes. */
    DSD(DS_000A2CB8) = 0x7Fu;
    DSD(DS_001028C0) = 0;
    sound_music_volume(0x21u);
    CHECK_EQ_INT((int)DSD(DS_000A2CB8), 0x21);
    sound_music_volume(0x21u);
    CHECK_EQ_INT((int)DSD(DS_000A2CB8), 0x21);

    /* 0x1D1B0 with the music running and no sequence handle: it pauses (byte
     * DS_001028DA = 1, pending song untouched); the second call resumes and,
     * with no handle, leaves the pending song alone. */
    sv_seed();
    for (i = 0; i < 4u; i++) sv_status(i, 2);
    sound_music_pause_toggle();
    CHECK_EQ_INT((int)DSB(DS_001028DA), 1);
    CHECK_EQ_INT((int)DSD(DS_001028CC), 0x5E5E5E5E);
    sound_music_pause_toggle();
    CHECK_EQ_INT((int)DSB(DS_001028DA), 0);
    CHECK_EQ_INT((int)DSD(DS_001028CC), 0x5E5E5E5E);
    /* Resuming with a handle, a song byte and a song makes the song pending;
     * each of the three missing leaves the pending word alone. */
    DSD(DS_001028C0) = 0x1234u;
    DSB(DS_001028DA) = 1u;
    sound_music_pause_toggle();
    CHECK_EQ_INT((int)DSB(DS_001028DA), 0);
    CHECK_EQ_INT((int)DSD(DS_001028CC), (int)0xD4D4D4D4u);
    DSD(DS_001028CC) = 0x5E5E5E5Eu;
    DSB(DS_001028DA) = 1u;
    DSB(DS_001028D9) = 0;
    sound_music_pause_toggle();
    CHECK_EQ_INT((int)DSD(DS_001028CC), 0x5E5E5E5E);
    DSB(DS_001028DA) = 1u;
    DSB(DS_001028D9) = 0x99u;
    DSD(DS_001028D4) = 0;
    sound_music_pause_toggle();
    CHECK_EQ_INT((int)DSD(DS_001028CC), 0x5E5E5E5E);
    DSD(DS_001028C0) = 0;

    /* 0x1D220: the first press sets DS_001028DB and stops every sample (the
     * queued +0x04 and playing +0x0C handles clear, the playing slots are
     * inited); the second clears the byte and touches nothing. */
    sv_seed();
    for (i = 0; i < 4u; i++) sv_status(i, (i & 1u) ? 4 : 2);
    sound_sample_pause_toggle();
    CHECK_EQ_INT((int)DSB(DS_001028DB), 1);
    for (i = 0; i < 4u; i++) {
        CHECK_EQ_INT((int)DSD(DS_00102864 + i * 0x18u), 0);
        CHECK_EQ_INT((int)DSD(DS_0010286C + i * 0x18u), 0);
        CHECK_EQ_INT((int)AIL_sample_status(sound_slot_handle(i)), 2);
    }
    sv_seed();
    DSB(DS_001028DB) = 1u;
    sound_sample_pause_toggle();
    CHECK_EQ_INT((int)DSB(DS_001028DB), 0);
    CHECK_EQ_INT((int)DSD(DS_00102864), 0x44440000);

    /* 0x1D250: music running: pauses it, DS_001028D8 = 1, samples stop. Music
     * already paused: D8 keeps its seed, samples stop. */
    sv_seed();
    DSB(DS_001028D8) = 0x55u;
    sound_pause();
    CHECK_EQ_INT((int)DSB(DS_001028DA), 1);
    CHECK_EQ_INT((int)DSB(DS_001028D8), 1);
    CHECK_EQ_INT((int)DSD(DS_00102864), 0);
    sv_seed();
    DSB(DS_001028DA) = 1u;
    DSB(DS_001028D8) = 0x55u;
    sound_pause();
    CHECK_EQ_INT((int)DSB(DS_001028DA), 1);
    CHECK_EQ_INT((int)DSB(DS_001028D8), 0x55);
    CHECK_EQ_INT((int)DSD(DS_0010286C + 0x18u), 0);

    /* 0x1D270: D8 == 1 toggles the music back and clears D8; any other D8
     * changes nothing. */
    DSB(DS_001028D8) = 1u;
    sound_resume();
    CHECK_EQ_INT((int)DSB(DS_001028DA), 0);
    CHECK_EQ_INT((int)DSB(DS_001028D8), 0);
    DSB(DS_001028DA) = 1u;
    DSB(DS_001028D8) = 0x55u;
    sound_resume();
    CHECK_EQ_INT((int)DSB(DS_001028DA), 1);
    CHECK_EQ_INT((int)DSB(DS_001028D8), 0x55);
    DSB(DS_001028DA) = 0;
    DSB(DS_001028D8) = 0;

    /* 0x2C9B8: negative runs the (no-op) voice 0 and returns 0x10000. */
    DSD(DS_001028D4) = 0xD4D4D4D4u;
    CHECK_EQ_INT((int)config_voice_gate(-1), 0x10000);
    CHECK_EQ_INT((int)config_voice_gate((s32)0x80000000u), 0x10000);
    CHECK_EQ_INT((int)config_voice_gate(0), 0);
    CHECK_EQ_INT((int)config_voice_gate(7), 0);
    CHECK_EQ_INT((int)config_voice_gate(0x7FFFFFFF), 0);
    CHECK_EQ_INT((int)DSD(DS_001028D4), (int)0xD4D4D4D4u);
}

/* Record §K6: 0x4F714 and 0x4F728 over the shipped voice records (case 1 or
 * 5 only, so no resource read), on sv_seed's sentinels (DS_00105D5C
 * 0x5C5C5C5C, DS_001028D4 0xD4D4D4D4, DS_001028D9 0x99). DS_001028D4 and
 * DS_00105D5C name the case-1 handle that played: 0xDF's 0x02806EC8 or 0x23's
 * 0x02805B88. The data object is snapshotted: the result, the slots' +0x63
 * bytes, DS_001088F2 and (last) 0x23's record are seeded. */
#define VW_SLOT63(k) (DS_00107813 + (u32)(k) * 0x94u)

static void vw_match(u32 r, u8 own, u8 other, u8 f2)
{
    u32 k;
    sv_seed();
    for (k = 0; k < 6u; k++) DSB(VW_SLOT63(k) - 0x94u) = other;   /* slots -1..4 */
    DSD(DS_00104AD4) = r;
    DSB(VW_SLOT63(r)) = own;
    DSB(DS_001088F2) = f2;
    sound_voice_match_end();
}

static void check_voice_wrappers(void)
{
    static u8 vw_data[0x8B0D0];
    u32 i;
    tf_snap(vw_data, DATA_BASE, 0x8B0D0u);

    /* 0x4F714: the stage word indexes 0xC9888; stages 0, 2 and 7 name 0x20,
     * 0x1B and 0x1F, each case 1 with byte 1. */
    {
        static const u32 st[3] = { 0u, 2u, 7u };
        static const u32 h[3] = { 0x0A800008u, 0x0D000008u, 0x0B000008u };
        for (i = 0; i < 3u; i++) {
            sv_seed();
            CHECK_EQ_INT((int)sound_voice_stage(st[i]), 1);
            CHECK_EQ_INT((int)DSD(DS_00105D5C), (int)h[i]);
            CHECK_EQ_INT((int)DSD(DS_001028D4), (int)h[i]);
            CHECK_EQ_INT((int)DSB(DS_001028D9), 1);
        }
    }

    /* 0x4F728's gate: result r (not -1/3), slot r's +0x63 == 0 and the
     * signed DS_001088F2 > 0 play 0xDF, else 0x23; the trailing 0x22 finds a
     * handle in DS_00105D5C and keeps the song words. */
    {
        static const u32 rs[10]  = { 0u, 1u, 0u, 1u, 0xFFFFFFFFu, 3u, 2u, 0u, 0u, 0u };
        static const u8  own[10] = { 0,  0,  1,  1,  0,           0,  0,  0,  0,  0 };
        static const u8  oth[10] = { 1,  1,  0,  0,  0,           0,  0,  0,  0,  0 };
        static const u8  f2[10]  = { 1,  1,  1,  1,  1,           1,  1,  0,  0x80, 0x7F };
        static const int df[10]  = { 1,  1,  0,  0,  0,           0,  1,  0,  0,  1 };
        for (i = 0; i < 10u; i++) {
            u32 h = df[i] ? 0x02806EC8u : 0x02805B88u;
            vw_match(rs[i], own[i], oth[i], f2[i]);
            CHECK_EQ_INT((int)DSD(DS_00105D5C), (int)h);
            CHECK_EQ_INT((int)DSD(DS_001028D4), (int)h);
            CHECK_EQ_INT((int)DSB(DS_001028D9), 0);
        }
    }

    /* The trailing 0x22 and its order: with 0x23's record handle retyped to
     * 0x1B, 0x23 makes 0x1B the current voice and 0x22 then stops the music
     * (DS_001028D4 = 0). Played first, or not at all, 0x22 would leave
     * DS_001028D4 = 0x1B. */
    DSD(DS_000BBDC8 + 0x23u * 12u + 4u) = 0x1Bu;
    vw_match(0xFFFFFFFFu, 0, 0, 1);
    CHECK_EQ_INT((int)DSD(DS_00105D5C), 0x1B);
    CHECK_EQ_INT((int)DSD(DS_001028D4), 0);

    tf_put(vw_data, DATA_BASE, 0x8B0D0u);
}

/* 0x2C3FC over the shipped records at DS_000BBDC8 and the sound module's
 * slot scans on the live AIL handles game_audio_init allocated. Snapshots
 * and restores the data object, the INDEX table, both pools, the DAC and the
 * aperture (each first read draws the loader screen). */
static void check_sound_voice(void)
{
    static u8 sv_data[0x8B0D0], sv_idx[256u * 20u], sv_pa[0x4880], sv_pb[0xEBA0];
    static u8 sv_ap[320u * 200u], sv_dac[256][3];
    const u32 idx = DSD(DS_001014E0), nidx = res_count() * 20u;
    const u32 pa = DSD(DS_001014EC), pb = DSD(DS_001014F4);
    u32 i;

    CHECK(nidx <= sizeof sv_idx, "the INDEX table fits the snapshot");
    for (i = 0; i < 4u; i++)
        CHECK(sound_slot_handle(i) != NULL, "game_audio_init allocated slot handles");
    CHECK(sound_slot_handle(4u) == NULL, "slot 4 is out of range");
    tf_snap(sv_data, DATA_BASE, 0x8B0D0u);
    tf_snap(sv_idx, idx, nidx);
    tf_snap(sv_pa, pa, 0x4880u);
    tf_snap(sv_pb, pb, 0xEBA0u);
    memcpy(sv_ap, gfx_aperture(), sizeof sv_ap);
    memcpy(sv_dac, gfx_dac, sizeof sv_dac);
    res_set_screen_hook(sv_hook);
    for (i = 0; i < 4u; i++) sv_status(i, 2);

    /* A: id 0 (0x2C401): AL = 0, nothing written. Case 0 (0x29's record):
     * AL = 1, nothing written. Case 6 (id 1): AL = 0. */
    sv_seed();
    CHECK_EQ_INT((int)sound_voice(0u), 0);
    CHECK_EQ_INT((int)DSD(DS_001028D4), (int)0xD4D4D4D4u);
    CHECK_EQ_INT((int)DSB(DS_000BBDC8 + 0x29u * 12u), 0);
    CHECK_EQ_INT((int)sound_voice(0x29u), 1);
    CHECK_EQ_INT((int)DSD(DS_001028D4), (int)0xD4D4D4D4u);
    CHECK_EQ_INT((int)DSD(DS_00105D5C), 0x5C5C5C5C);
    CHECK_EQ_INT((int)DSB(DS_000BBDC8 + 1u * 12u), 6);
    CHECK_EQ_INT((int)sound_voice(1u), 0);

    /* B: case 1 (id 0x21: 0x2803E64, byte 1): the voice word, 0x1CA14's song
     * and byte; no sequence handle, so DS_001028CC keeps its sentinel. With
     * one (and the music unpaused) it becomes the pending song; paused, not. */
    CHECK_EQ_INT((int)sound_voice(0x21u), 1);
    CHECK_EQ_INT((int)DSD(DS_00105D5C), 0x2803E64);
    CHECK_EQ_INT((int)DSD(DS_001028D4), 0x2803E64);
    CHECK_EQ_INT((int)DSB(DS_001028D9), 1);
    CHECK_EQ_INT((int)DSD(DS_001028CC), 0x5E5E5E5E);
    DSD(DS_001028C0) = 0x1234u;
    CHECK_EQ_INT((int)sound_voice(0x21u), 1);
    CHECK_EQ_INT((int)DSD(DS_001028CC), 0x2803E64);
    DSD(DS_001028CC) = 0x5E5E5E5Eu;
    DSB(DS_001028DA) = 1u;
    CHECK_EQ_INT((int)sound_voice(0x21u), 1);
    CHECK_EQ_INT((int)DSD(DS_001028CC), 0x5E5E5E5E);

    /* C: case 2 (id 0x3E: 0x1800EBC9, s16havsd = entry 48). Not playing:
     * 0x1CC28 resolves it, the bank's first read (the loader screen). */
    sv_seed();
    sv_unload(48u);
    CHECK_EQ_INT((int)sound_voice(0x3Eu), 1);
    CHECK(sv_loaded(48u), "case 2 reads the voice's bank");
    CHECK_EQ_INT(sv_hook_n, 1);
    CHECK_EQ_INT((int)DSD(DS_001014FC), 1);
    /* Playing on slot 2 (+0x0C = the handle, status 4): AL = 0, no read. */
    sv_seed();
    sv_unload(48u);
    DSD(DS_0010286C + 2u * 0x18u) = 0x1800EBC9u;
    sv_status(2u, 4);
    CHECK_EQ_INT((int)sound_voice(0x3Eu), 0);
    CHECK(!sv_loaded(48u), "a playing sample is not queued again");
    CHECK_EQ_INT((int)DSD(DS_0010286C + 2u * 0x18u), 0x1800EBC9);
    /* Without a DIG driver 0x1CE70 answers 0 unscanned and 0x1CC28 reads
     * nothing: AL = 1 and the bank stays unread. */
    DSD(DS_001028C8) = 0;
    CHECK_EQ_INT((int)sound_voice(0x3Eu), 1);
    CHECK(!sv_loaded(48u), "no DIG driver, no read");
    /* The sample pause byte DS_001028DB blocks the read too. */
    DSD(DS_001028C8) = 1u;
    DSB(DS_001028DB) = 1u;
    sv_status(2u, 2);
    CHECK_EQ_INT((int)sound_voice(0x3Eu), 1);
    CHECK(!sv_loaded(48u), "paused samples, no read");
    /* 0x1CE70 clears a stopped slot's +0x0C and scans on: slot 0 stopped and
     * slot 1 playing the handle answer 1, with slot 0 cleared. */
    sv_seed();
    DSD(DS_0010286C) = 0x1800EBC9u;
    DSD(DS_0010286C + 0x18u) = 0x1800EBC9u;
    sv_status(0u, 2);
    sv_status(1u, 4);
    CHECK_EQ_INT((int)sound_voice(0x3Eu), 0);
    CHECK_EQ_INT((int)DSD(DS_0010286C), 0);
    CHECK_EQ_INT((int)DSD(DS_0010286C + 0x18u), 0x1800EBC9);
    /* A stopped slot alone: cleared, and the voice is queued. */
    sv_seed();
    sv_unload(48u);
    DSD(DS_0010286C + 0x18u) = 0x1800EBC9u;
    sv_status(1u, 2);
    CHECK_EQ_INT((int)sound_voice(0x3Eu), 1);
    CHECK_EQ_INT((int)DSD(DS_0010286C + 0x18u), 0);
    CHECK(sv_loaded(48u), "a stopped sample is queued again");

    /* D: case 3, id 0x4D: 0x1201D606 (s16cobsd, 36) and 0x2001513C (s16spisd,
     * 64), each read the first time (two loader screens). */
    sv_seed();
    sv_unload(36u);
    sv_unload(64u);
    CHECK_EQ_INT((int)DSB(DS_000BBDC8 + 0x4Du * 12u), 3);
    CHECK_EQ_INT((int)sound_voice(0x4Du), 1);
    CHECK(sv_loaded(36u) && sv_loaded(64u), "0x4D reads s16cobsd and s16spisd");
    CHECK_EQ_INT(sv_hook_n, 2);
    /* 0x1201D606 playing: AL = 0 and neither is read. */
    sv_seed();
    sv_unload(36u);
    sv_unload(64u);
    DSD(DS_0010286C + 3u * 0x18u) = 0x1201D606u;
    sv_status(3u, 4);
    CHECK_EQ_INT((int)sound_voice(0x4Du), 0);
    CHECK(!sv_loaded(36u) && !sv_loaded(64u), "0x4D's first sample plays");
    sv_status(3u, 2);
    /* Ids 0x46 and 0x5D: both samples are s16sound's (entry 5). 0x46 tests
     * the second of its pair (0x2886158), 0x5D its first (0x281A726); the
     * pair's other handle playing does not refuse the voice. */
    sv_seed();
    sv_unload(5u);
    CHECK_EQ_INT((int)sound_voice(0x46u), 1);
    CHECK(sv_loaded(5u), "0x46 reads s16sound");
    sv_seed();
    DSD(DS_0010286C) = 0x2886158u;
    sv_status(0u, 4);
    CHECK_EQ_INT((int)sound_voice(0x46u), 0);
    DSD(DS_0010286C) = 0x28847C9u;
    CHECK_EQ_INT((int)sound_voice(0x46u), 1);
    sv_seed();
    sv_unload(5u);
    CHECK_EQ_INT((int)sound_voice(0x5Du), 1);
    CHECK(sv_loaded(5u), "0x5D reads s16sound");
    sv_seed();
    DSD(DS_0010286C) = 0x281A726u;
    sv_status(0u, 4);
    CHECK_EQ_INT((int)sound_voice(0x5Du), 0);
    DSD(DS_0010286C) = 0x2819183u;
    CHECK_EQ_INT((int)sound_voice(0x5Du), 1);
    sv_status(0u, 2);
    /* An unlisted case-3 id (0x47's record retyped for the test): AL = 0. */
    sv_seed();
    DSB(DS_000BBDC8 + 0x47u * 12u) = 3u;
    sv_unload(5u);
    CHECK_EQ_INT((int)sound_voice(0x47u), 0);
    CHECK(!sv_loaded(5u), "an unlisted case-3 id reads nothing");

    /* E: case 4 (id 3): both pauses cleared, voice 0x21, 0x1CA14(0x2803E64,
     * 0), and, 0x180122FD not playing, every slot cleared (0x1CD9C: a playing
     * one stopped) and 0x180122FD queued (s16havsd, entry 48). */
    sv_seed();
    DSB(DS_001028DA) = 1u;
    DSB(DS_001028DB) = 1u;
    sv_unload(48u);
    sv_status(0u, 4);
    CHECK_EQ_INT((int)DSB(DS_000BBDC8 + 3u * 12u), 4);
    {
        int voices = mixer_active_voices();
        CHECK_EQ_INT((int)sound_voice(3u), 1);
        CHECK_EQ_INT(mixer_active_voices(), voices - 1);   /* 0x1CDD6 0x5DC8B */
    }
    CHECK_EQ_INT((int)DSB(DS_001028DA), 0);
    CHECK_EQ_INT((int)DSB(DS_001028DB), 0);
    CHECK_EQ_INT((int)DSD(DS_00105D5C), 0x21);
    CHECK_EQ_INT((int)DSD(DS_001028D4), 0x2803E64);
    CHECK_EQ_INT((int)DSB(DS_001028D9), 0);
    /* 0x1CD9C cleared every slot, then 0x1CC28 queued 0x180122FD (0x79C0
     * bytes > 0x6000, so only slot 0; free after the stop) (record k7-k12 §3). */
    CHECK_EQ_INT((int)DSD(DS_00102864), 0x180122FD);
    for (i = 0; i < 4u; i++) {
        if (i != 0u) CHECK_EQ_INT((int)DSD(DS_00102864 + i * 0x18u), 0);
        CHECK_EQ_INT((int)DSD(DS_0010286C + i * 0x18u), 0);
    }
    CHECK_EQ_INT((int)AIL_sample_status(sound_slot_handle(0u)), 2);
    CHECK(sv_loaded(48u), "case 4 queues 0x180122FD");
    /* 0x180122FD playing: the slots are kept and nothing is read. */
    sv_seed();
    sv_unload(48u);
    DSD(DS_0010286C + 0x18u) = 0x180122FDu;
    sv_status(1u, 4);
    CHECK_EQ_INT((int)sound_voice(3u), 1);
    CHECK_EQ_INT((int)DSD(DS_00102864), 0x44440000);
    CHECK_EQ_INT((int)DSD(DS_0010286C + 0x18u), 0x180122FD);
    CHECK(!sv_loaded(48u), "case 4 with 0x180122FD playing reads nothing");
    CHECK_EQ_INT((int)DSD(DS_00105D5C), 0x21);
    sv_status(1u, 2);

    /* F: case 5, id 0x100 (id 0's record): 0x1CA6C clears the song words and
     * 0x1CD9C every slot's handles; without a DIG driver only the former. */
    sv_seed();
    CHECK_EQ_INT((int)sound_voice(0x100u), 1);
    CHECK_EQ_INT((int)DSD(DS_001028D4), 0);
    CHECK_EQ_INT((int)DSB(DS_001028D9), 0);
    CHECK_EQ_INT((int)DSD(DS_00102864 + 0x30u), 0);
    CHECK_EQ_INT((int)DSD(DS_0010286C + 0x48u), 0);
    sv_seed();
    DSD(DS_001028C8) = 0;
    CHECK_EQ_INT((int)sound_voice(0x100u), 1);
    CHECK_EQ_INT((int)DSD(DS_001028D4), 0);
    CHECK_EQ_INT((int)DSD(DS_00102864), 0x44440000);
    CHECK_EQ_INT((int)DSD(DS_0010286C), (int)0xCC000000u);
    /* 0x1CA6C with a sequence handle and the title music playing: the pending
     * song cleared and the sequence stopped (0x5DEAF); stopped, kept. */
    CHECK(seq_playing(), "the title music plays before the 0x1CA6C case");
    sv_seed();
    DSD(DS_001028C0) = 0x1234u;
    CHECK_EQ_INT((int)sound_voice(0x100u), 1);
    CHECK_EQ_INT((int)DSD(DS_001028CC), 0);
    CHECK(!seq_playing(), "0x1CA6C stops the playing sequence");
    sv_seed();
    DSD(DS_001028C0) = 0x1234u;
    CHECK_EQ_INT((int)sound_voice(0x100u), 1);
    CHECK_EQ_INT((int)DSD(DS_001028CC), 0x5E5E5E5E);
    /* The music stops, each keyed on the current voice DS_00105D5C: a listed
     * voice clears DS_001028D4, any other keeps it. */
    {
        static const u16 stops[][2] = {
            { 0x22, 0x1B }, { 0x22, 0x21 }, { 0x22, 0x25 }, { 0x22, 0x26 },
            { 0x2B, 0x2A }, { 0x2D, 0x2C }, { 0x2F, 0x2E }, { 0x2F, 0x30 },
            { 0x33, 0x32 }, { 0x3C, 0x3B }, { 0x55, 0x54 }, { 0x57, 0x56 },
            { 0xE0, 0xDF }, { 0xE2, 0xE1 }, { 0xE2, 0xE3 },
        };
        static const u16 keeps[][2] = {
            { 0x22, 0x1A }, { 0x22, 0x22 }, { 0x22, 0x24 }, { 0x22, 0x27 },
            { 0x2B, 0x2B }, { 0x2D, 0x2D }, { 0x2F, 0x2F }, { 0x2F, 0x31 },
            { 0x33, 0x33 }, { 0x3C, 0x3C }, { 0x55, 0x55 }, { 0x57, 0x57 },
            { 0xE0, 0xE0 }, { 0xE2, 0xE2 }, { 0xE2, 0xE4 },
        };
        for (i = 0; i < sizeof stops / sizeof stops[0]; i++) {
            sv_seed();
            DSD(DS_00105D5C) = stops[i][1];
            CHECK_EQ_INT((int)sound_voice(stops[i][0]), 1);
            CHECK_EQ_INT((int)DSD(DS_001028D4), 0);
        }
        for (i = 0; i < sizeof keeps / sizeof keeps[0]; i++) {
            sv_seed();
            DSD(DS_00105D5C) = keeps[i][1];
            CHECK_EQ_INT((int)sound_voice(keeps[i][0]), 1);
            CHECK_EQ_INT((int)DSD(DS_001028D4), (int)0xD4D4D4D4u);
        }
    }
    /* G: the sample stops (0x1CE04): slot 1 playing the id's handle is ended,
     * re-inited and its +0x0C cleared; one already stopped (status 2) is
     * left as it is. The song words are untouched. */
    {
        static const u32 ce04[][2] = {
            { 0x3F, 0x1800EBC9u }, { 0x41, 0x383B6F4u }, { 0x43, 0x3837440u },
            { 0x4C, 0x22008696u }, { 0x4F, 0x1501053Cu }, { 0x5B, 0x1B01AF00u },
            { 0xF1, 0x22018405u },
        };
        for (i = 0; i < sizeof ce04 / sizeof ce04[0]; i++) {
            sv_seed();
            DSD(DS_0010286C + 0x18u) = ce04[i][1];
            sv_status(1u, 4);
            int voices = mixer_active_voices();
            CHECK_EQ_INT((int)sound_voice(ce04[i][0]), 1);
            CHECK_EQ_INT((int)DSD(DS_0010286C + 0x18u), 0);
            CHECK_EQ_INT((int)AIL_sample_status(sound_slot_handle(1u)), 2);
            CHECK_EQ_INT(mixer_active_voices(), voices - 1);   /* 0x5DC8B */
            CHECK_EQ_INT((int)DSD(DS_001028D4), (int)0xD4D4D4D4u);
            sv_seed();
            DSD(DS_0010286C + 0x18u) = ce04[i][1];
            sv_status(1u, 2);
            CHECK_EQ_INT((int)sound_voice(ce04[i][0]), 1);
            CHECK_EQ_INT((int)DSD(DS_0010286C + 0x18u), (int)ce04[i][1]);
        }
        /* Only the first playing match is stopped (0x1CE5B returns). */
        sv_seed();
        DSD(DS_0010286C + 0x18u) = 0x1800EBC9u;
        DSD(DS_0010286C + 0x30u) = 0x1800EBC9u;
        sv_status(1u, 4);
        sv_status(2u, 4);
        CHECK_EQ_INT((int)sound_voice(0x3Fu), 1);
        CHECK_EQ_INT((int)DSD(DS_0010286C + 0x18u), 0);
        CHECK_EQ_INT((int)DSD(DS_0010286C + 0x30u), 0x1800EBC9);
        /* Without a DIG driver nothing is stopped. */
        sv_seed();
        DSD(DS_001028C8) = 0;
        DSD(DS_0010286C + 0x30u) = 0x1800EBC9u;
        CHECK_EQ_INT((int)sound_voice(0x3Fu), 1);
        CHECK_EQ_INT((int)DSD(DS_0010286C + 0x30u), 0x1800EBC9);
    }

    for (i = 0; i < 4u; i++) sv_status(i, 2);
    res_set_screen_hook(NULL);
    memcpy(gfx_dac, sv_dac, sizeof sv_dac);
    memcpy(gfx_aperture(), sv_ap, sizeof sv_ap);
    tf_put(sv_pb, pb, 0xEBA0u);
    tf_put(sv_pa, pa, 0x4880u);
    tf_put(sv_idx, idx, nidx);
    tf_put(sv_data, DATA_BASE, 0x8B0D0u);
}

/* Record §K7 (k7-k12 derivations §0.7.3/§0.7.4, Task 3 §3): 0x1CC28's slot
 * choice and 0x1CB18's start on the live handles and 0x1D0BC's buffers.
 * 0x42 = 0x03837440 (0x1D85 bytes, loop byte 1), 0x40 = 0x0383B6F4 (0x5FAE,
 * loop byte 1), 0x3A = 0x03022554 (0x8320 > 0x6000, loop byte 0). */
static void ss_seed(void)
{
    DSD(DS_001028C8) = 1u;
    DSB(DS_001028DB) = 0;
    for (u32 i = 0; i < 4u; i++) {
        DSD(DS_00102864 + i * 0x18u) = 0;
        DSB(DS_00102868 + i * 0x18u) = 0x77u;
        DSD(DS_0010286C + i * 0x18u) = 0;
        DSD(DS_00102874 + i * 0x18u) = 0x10u + i;
        AIL_init_sample(sound_slot_handle(i));
    }
    mixer_stop_samples();
    DSD(DS_00101500) = 0x100u;
}

static void check_sample_slots(void)
{
    static u8 ss_ap[320u * 200u], ss_dac[256][3], ss_rec[4u * 0x18u];
    static s16 ss_buf[4096 * 2];
    const u32 s_1500 = DSD(DS_00101500), s_c8 = DSD(DS_001028C8);
    const u8 s_db = DSB(DS_001028DB);
    u32 i;
    memcpy(ss_ap, gfx_aperture(), sizeof ss_ap);
    memcpy(ss_dac, gfx_dac, sizeof ss_dac);
    memcpy(ss_rec, mem + DS_00102860, sizeof ss_rec);
    for (i = 0; i < 4u; i++)
        CHECK(DSD(DS_00102870 + i * 0x18u) != 0u, "0x1D0BC gave every slot a buffer");

    /* A: size <= 0x6000 takes the first free slot from 3 down (0x1CCC3). */
    ss_seed();
    CHECK_EQ_INT((int)sound_voice(0x42u), 1);
    CHECK_EQ_INT((int)DSD(DS_00102864 + 3u * 0x18u), 0x03837440);
    CHECK_EQ_INT((int)DSB(DS_00102868 + 3u * 0x18u), 1);
    CHECK_EQ_INT((int)DSD(DS_00102874 + 3u * 0x18u), 0x100);
    CHECK_EQ_INT((int)DSD(DS_00102864 + 2u * 0x18u), 0);
    CHECK_EQ_INT((int)sound_voice(0x40u), 1);
    CHECK_EQ_INT((int)DSD(DS_00102864 + 2u * 0x18u), 0x0383B6F4);
    CHECK_EQ_INT((int)DSD(DS_00102864 + 1u * 0x18u), 0);
    {
        /* A2: slot 3 is not free while it plays (0x1CCE6 0x5DD03 status 4,
         * 0x1CCF1) or without a buffer (0x1CCCD/0x1CCD4): 0x42 goes to slot 2. */
        u32 b3 = DSD(DS_00102870 + 3u * 0x18u);
        ss_seed();
        sv_status(3u, 4);
        CHECK_EQ_INT((int)sound_voice(0x42u), 1);
        CHECK_EQ_INT((int)DSD(DS_00102864 + 3u * 0x18u), 0);
        CHECK_EQ_INT((int)DSD(DS_00102864 + 2u * 0x18u), 0x03837440);
        ss_seed();
        DSD(DS_00102870 + 3u * 0x18u) = 0;
        CHECK_EQ_INT((int)sound_voice(0x42u), 1);
        CHECK_EQ_INT((int)DSD(DS_00102864 + 3u * 0x18u), 0);
        CHECK_EQ_INT((int)DSD(DS_00102864 + 2u * 0x18u), 0x03837440);
        DSD(DS_00102870 + 3u * 0x18u) = b3;
        ss_seed();
        CHECK_EQ_INT((int)sound_voice(0x42u), 1);
        CHECK_EQ_INT((int)sound_voice(0x40u), 1);
    }

    /* B: 0x1CF20 -> 0x1CB18 per slot: copy, start, +0x0C = +0x04, +0x04 = 0. */
    game_audio_service();
    CHECK_EQ_INT((int)DSD(DS_0010286C + 3u * 0x18u), 0x03837440);
    CHECK_EQ_INT((int)DSD(DS_00102864 + 3u * 0x18u), 0);
    CHECK_EQ_INT((int)DSD(DS_0010286C + 2u * 0x18u), 0x0383B6F4);
    CHECK_EQ_INT((int)AIL_sample_status(sound_slot_handle(3u)), 4);
    CHECK_EQ_INT((int)AIL_sample_status(sound_slot_handle(0u)), 2);
    {
        const u8 *p = (const u8 *)res_resolve(0x03837440u);
        CHECK(p != NULL && memcmp(mem + DSD(DS_00102870 + 3u * 0x18u), p + 4, 0x1D85u) == 0,
              "0x1CB18 copied the sample into the slot's buffer");
    }
    CHECK_EQ_INT(mixer_active_voices(), 2);
    for (i = 0; i < 24u; i++) mixer_render(ss_buf, 4096, MIXER_OPL_RATE);
    CHECK_EQ_INT(mixer_active_voices(), 2);                 /* loop byte 1: count 0 */
    {
        /* B2: the loop count is set only for loop byte 1 (0x1CBD3 `cmp eax,1`):
         * byte 2 keeps count 1 and the voice ends (0x1D85 bytes, ~34k output
         * frames). Slot 1, so B's two voices stay. */
        DSD(DS_00102864 + 1u * 0x18u) = 0x03837440u;
        DSB(DS_00102868 + 1u * 0x18u) = 2u;
        sound_sample_start(1u);
        CHECK_EQ_INT(mixer_active_voices(), 3);
        for (i = 0; i < 24u; i++) mixer_render(ss_buf, 4096, MIXER_OPL_RATE);
        CHECK_EQ_INT(mixer_active_voices(), 2);
        CHECK_EQ_INT((int)AIL_sample_status(sound_slot_handle(1u)), 2);
    }

    /* C: 0x41 stops 0x40's handle (0x2C7A5 -> 0x1CE04), 0x43 the other. */
    CHECK_EQ_INT((int)sound_voice(0x41u), 1);
    CHECK_EQ_INT((int)DSD(DS_0010286C + 2u * 0x18u), 0);
    CHECK_EQ_INT((int)AIL_sample_status(sound_slot_handle(2u)), 2);
    CHECK_EQ_INT(mixer_active_voices(), 1);
    CHECK_EQ_INT((int)sound_voice(0x43u), 1);
    CHECK_EQ_INT(mixer_active_voices(), 0);

    /* D: size > 0x6000 only on slot 0 (0x1CC68); slot 0 busy forces it.
     * The free arm stamps slot 0's +0x14 (seeded 0x10) with the clock read
     * again at the store (0x1CCA7/0x1CCAC), after the resolve: in the
     * suite's order this first s16snd2 read stalls, so it is 0x102, not the
     * entry's 0x100. */
    ss_seed();
    CHECK_EQ_INT((int)sound_voice(0x3Au), 1);
    CHECK_EQ_INT((int)DSD(DS_00102874), (int)DSD(DS_00101500));
    CHECK_EQ_INT((int)DSD(DS_00102864), 0x03022554);
    CHECK_EQ_INT((int)DSB(DS_00102868), 0);
    CHECK_EQ_INT((int)DSD(DS_00102864 + 3u * 0x18u), 0);
    DSD(DS_00101500) = 0x180u;
    CHECK_EQ_INT((int)sound_voice(0x3Au), 1);
    CHECK_EQ_INT((int)DSD(DS_00102874), 0x180);
    /* D2: slot 0 playing is not free (0x1CC89/0x1CC94); the forced arm ends
     * it (0x1CD4F 0x5DC8B: the mixer voice goes) before queueing. */
    ss_seed();
    sv_status(0u, 4);
    CHECK_EQ_INT(mixer_active_voices(), 1);
    CHECK_EQ_INT((int)sound_voice(0x3Au), 1);
    CHECK_EQ_INT(mixer_active_voices(), 0);
    CHECK_EQ_INT((int)DSD(DS_00102864), 0x03022554);

    /* E: none free: the smallest +0x14 below now (0x1CD22 `jbe`, unsigned),
     * scanned 3..0; all at now leaves the candidate at slot 0 (0x1CC64). */
    ss_seed();
    {
        static const u32 t[4] = { 0x90u, 0x50u, 0x70u, 0x60u };
        for (i = 0; i < 4u; i++) {
            DSD(DS_00102864 + i * 0x18u) = 0x44440000u + i;
            DSD(DS_00102874 + i * 0x18u) = t[i];
        }
    }
    CHECK_EQ_INT((int)sound_voice(0x42u), 1);
    CHECK_EQ_INT((int)DSD(DS_00102864 + 1u * 0x18u), 0x03837440);
    CHECK_EQ_INT((int)DSD(DS_00102874 + 1u * 0x18u), 0x100);
    CHECK_EQ_INT((int)DSD(DS_00102864 + 3u * 0x18u), 0x44440003);
    CHECK_EQ_INT((int)DSD(DS_00102864), 0x44440000);
    ss_seed();
    for (i = 0; i < 4u; i++) {
        DSD(DS_00102864 + i * 0x18u) = 0x44440000u + i;
        DSD(DS_00102874 + i * 0x18u) = 0x100u;
    }
    CHECK_EQ_INT((int)sound_voice(0x42u), 1);
    CHECK_EQ_INT((int)DSD(DS_00102864), 0x03837440);
    CHECK_EQ_INT((int)DSD(DS_00102864 + 3u * 0x18u), 0x44440003);
    {
        /* E2: the strict `min > t` (0x1CD22 `jbe` skips equal): a tie keeps
         * the first slot met from 3 down, and a slot at now is never taken
         * (slot 0 above now, 1..3 at now: the candidate stays 0). A `>=`
         * would take slot 1 in both. */
        static const u32 tt[2][4] = {
            { 0x90u, 0x50u, 0x70u, 0x50u },
            { 0x200u, 0x100u, 0x100u, 0x100u },
        };
        static const u32 want[2] = { 3u, 0u };
        for (u32 v = 0; v < 2u; v++) {
            ss_seed();
            for (i = 0; i < 4u; i++) {
                DSD(DS_00102864 + i * 0x18u) = 0x44440000u + i;
                DSD(DS_00102874 + i * 0x18u) = tt[v][i];
            }
            CHECK_EQ_INT((int)sound_voice(0x42u), 1);
            CHECK_EQ_INT((int)DSD(DS_00102864 + want[v] * 0x18u), 0x03837440);
            CHECK_EQ_INT((int)DSD(DS_00102864 + 1u * 0x18u), 0x44440001);
        }
    }
    {
        /* E3: the compare is unsigned (0x1CD22 `jbe`). now = 0x90000000 and
         * slot 0 at 0x88000000: unsigned, slots 3, 2, 1 (0x60, 0x50, 0x40)
         * each go below and slot 0 does not, so slot 1 is taken; a signed
         * compare would take only slot 0. */
        static const u32 t3[4] = { 0x88000000u, 0x40u, 0x50u, 0x60u };
        ss_seed();
        for (i = 0; i < 4u; i++) {
            DSD(DS_00102864 + i * 0x18u) = 0x44440000u + i;
            DSD(DS_00102874 + i * 0x18u) = t3[i];
        }
        DSD(DS_00101500) = 0x90000000u;
        CHECK_EQ_INT((int)sound_voice(0x42u), 1);
        CHECK_EQ_INT((int)DSD(DS_00102864 + 1u * 0x18u), 0x03837440);
        CHECK_EQ_INT((int)DSD(DS_00102864), 0x44440000);
    }

    /* F: PORT guard: a bufferless slot is not started (the raw would copy
     * to linear 0). mem[0] carries a sentinel the sample bytes differ from;
     * the low 0x2000 bytes are snapshotted so a mutated copy cannot outlive
     * the vector. */
    ss_seed();
    {
        static u8 ss_low[0x2000];
        u32 b3 = DSD(DS_00102870 + 3u * 0x18u);
        memcpy(ss_low, mem, sizeof ss_low);
        DSD(0u) = 0x5A5A5A5Au;
        DSD(DS_00102870 + 3u * 0x18u) = 0;
        DSD(DS_00102864 + 3u * 0x18u) = 0x03837440u;
        sound_sample_start(3u);
        CHECK_EQ_INT((int)DSD(DS_00102864 + 3u * 0x18u), 0x03837440);
        CHECK_EQ_INT((int)DSD(DS_0010286C + 3u * 0x18u), 0);
        CHECK_EQ_INT((int)DSD(0u), 0x5A5A5A5A);
        DSD(DS_00102870 + 3u * 0x18u) = b3;
        AIL_stop_sample(sound_slot_handle(3u));
        memcpy(mem, ss_low, sizeof ss_low);
    }
    /* ss_seed stops every voice; then the four slot records, the ISR clock,
     * the DIG handle and the pause byte are put back as the vectors found
     * them, so no later test inherits ss_seed's 0x77 loop bytes, its
     * 0x10 + i queue times or now = 0x100. */
    ss_seed();
    memcpy(mem + DS_00102860, ss_rec, sizeof ss_rec);
    DSD(DS_00101500) = s_1500;
    DSD(DS_001028C8) = s_c8;
    DSB(DS_001028DB) = s_db;
    memcpy(gfx_aperture(), ss_ap, sizeof ss_ap);
    memcpy(gfx_dac, ss_dac, sizeof ss_dac);
}

/* ---- the attract's high-score screen 0x1EA08 (record §46-A) ---- */

static u32 hs_cell(s32 row, s32 col)
{
    return DSD(DS_00105F38 + (u32)row * 0xACu + (u32)col * 4u);
}

/* A cell's glyph: the sprite id and the pset's flags word (+2), with the
 * palette entry's handle folded in. */
static u32 hs_sprite(s32 row, s32 col)
{
    u32 r = hs_cell(row, col);
    if (r == 0) return 0u;
    u32 pal = DSD(actor_pset(r) + 0x18u);
    return ((u32)(DSW(actor_pset(r)) & 0x7FFFu) | ((u32)DSW(actor_pset(r) + 2u) << 16))
           ^ (pal != 0u ? DSD(pal) : 0u);
}

/* 1 when the cells from `col` on row `row` hold the glyphs text_cursor_hold
 * draws for `s` at the same place (a space leaves its cell empty). */
static int hs_row_is(s32 row, s32 col, const char *s, u32 mode)
{
    u32 got[40], n = (u32)strlen(s), i;
    int ok = 1;
    for (i = 0; i < n; i++) got[i] = hs_sprite(row, col + (s32)i);
    text_cells_release(col, row, (const u8 *)s, mode);
    text_cursor_hold(col, row, (const u8 *)s, mode);
    for (i = 0; i < n; i++) {
        if (s[i] == ' ' ? got[i] != 0u : got[i] != hs_sprite(row, col + (s32)i)) ok = 0;
        if (s[i] != ' ' && got[i] == 0u) ok = 0;
    }
    return ok;
}

/* The figure: the one record 0x2AE14 gave EDX 0x2A00 and EBX 0x1C80. */
static u32 hs_figure(void)
{
    u32 found = 0u, n = 0u;
    for (u32 r = actor_list_head(); r != 0u; r = actor_next(r))
        if (DSD(r + 0x18u) == 0x2A00u && DSD(r + 0x1Cu) == 0x1C80u) { found = r; n++; }
    return n == 1u ? found : 0u;
}

/* The first sprite id 0x2AE14 gives descriptor 0xA7DCC[k] on an empty pool. */
static u32 hs_ref_sprite(u32 k)
{
    actors_reset();
    u32 r = actor_spawn((const u32 *)(mem + DSD(0xA7DCCu + k * 4u)), 0x2A00u, 0xFFu,
                        0x1C80u, 0u);
    return r != 0u ? (u32)DSW(actor_pset(r)) : 0xFFFFFFFFu;
}

static u32 hs_figure_sprite(void)
{
    u32 r = hs_figure();
    return r != 0u ? (u32)DSW(actor_pset(r)) : 0xFFFFFFFEu;
}

/* 0x1EA08 on the live resources: the champion on row 2, table 0's records 1..9
 * at the 0xA7B94 layout, and the figure from 0xA7DCC selected by the name's
 * character 0x12. The data object, the INDEX table, both pools, the DAC and
 * the aperture are restored. */
static void check_hiscore_screen(void)
{
    static u8 sv_data[0x8B0D0], sv_idx[256u * 20u], sv_pa[0x4880], sv_pb[0xEBA0];
    static u8 sv_ap[320u * 200u], sv_dac[256][3];
    const u32 idx = DSD(DS_001014E0), nidx = res_count() * 20u;
    const u32 pa = DSD(DS_001014EC), pb = DSD(DS_001014F4);
    static const char *rows[10][3] = {
        { " 1", "TEENY WEENY GAMES ", " 500000" },
        { " 2", "CFF", " 400000" }, { " 3", "AMR", " 350000" },
        { " 4", "MSG", " 300000" }, { " 5", "JSY", " 250000" },
        { " 6", "ACW", " 200000" }, { " 7", "MRP", "  90210" },
        { " 8", "HUH", "  50000" }, { " 9", "WHU", "  20000" },
        { "10", "DUD", "    100" },
    };
    u32 i, rec;

    CHECK(nidx <= sizeof sv_idx, "the INDEX table fits the snapshot");
    tf_snap(sv_data, DATA_BASE, 0x8B0D0u);
    tf_snap(sv_idx, idx, nidx);
    tf_snap(sv_pa, pa, 0x4880u);
    tf_snap(sv_pb, pb, 0xEBA0u);
    memcpy(sv_ap, gfx_aperture(), sizeof sv_ap);
    memcpy(sv_dac, gfx_dac, sizeof sv_dac);

    /* The tables as 0x1E824 leaves them on a fresh CMOS. */
    mem_fill(0x105E34u, 0, 153u);
    DSD(DS_00104528) = 0x142095u;
    hiscore_init();

    frontend_match_start();
    /* Record 0 of table 0 is not drawn: row [0xA7B94] holds no cell. */
    for (i = 0; i < 0x2Bu; i++)
        CHECK_EQ_INT((int)hs_cell(DSB(0xA7B94u), (s32)i), 0);
    CHECK(hs_row_is(2, DSB(0xA7B95u), rows[0][0], 0x3000u), "rank 1");
    CHECK(hs_row_is(2, DSB(0xA7B96u), rows[0][1], 0x2000u), "the champion's name");
    CHECK(hs_row_is(2, DSB(0xA7B97u), rows[0][2], 0x2000u), "the champion's score");
    for (i = 1; i < 10u; i++) {
        s32 row = DSB(0xA7B94u + i * 4u);
        CHECK(hs_row_is(row, DSB(0xA7B95u + i * 4u), rows[i][0], 0x3000u), "a rank");
        CHECK(hs_row_is(row, DSB(0xA7B96u + i * 4u), rows[i][1], 0x3000u), "a name");
        CHECK(hs_row_is(row, DSB(0xA7B97u + i * 4u), rows[i][2], 0x3000u), "a score");
    }
    /* The figure: on 0x2A17C's word 0 (with the 0x5F flag's 0x800) and the
     * palette entry of handle 0x105FD30. */
    rec = hs_figure();
    CHECK(rec != 0u, "one figure record");
    if (rec != 0u) {
        CHECK_EQ_INT((int)DSW(actor_pset(rec) + 0x02u), 0x800);
        CHECK_EQ_INT((int)DSD(DSD(actor_pset(rec) + 0x18u)), 0x105FD30);
    }
    {
        /* The selection. The reference sprites of descriptors 0, 2 and 6
         * differ, so each case below can tell them apart. */
        u32 f0, f2, f6;
        frontend_match_start();
        f0 = hs_figure_sprite();
        u32 r0 = hs_ref_sprite(0u), r2 = hs_ref_sprite(2u), r6 = hs_ref_sprite(6u);
        CHECK(r0 != r2 && r0 != r6 && r2 != r6, "descriptors 0, 2, 6 differ");
        /* Character 0x12 is a space (no string starts with one): 0. */
        CHECK_EQ_INT((int)f0, (int)r0);
        /* 'T' is the third string: 2. */
        DSW(0x105EACu + 4u + 12u) = 0x0014u;
        frontend_match_start();
        f2 = hs_figure_sprite();
        CHECK_EQ_INT((int)f2, (int)r2);
        /* 'H' is the seventh (index 6): kept. */
        DSW(0x105EACu + 4u + 12u) = 0x0008u;
        frontend_match_start();
        f6 = hs_figure_sprite();
        CHECK_EQ_INT((int)f6, (int)r6);
        /* 'X' is the eighth (index 7): above 6, back to 0. */
        DSW(0x105EACu + 4u + 12u) = 0x0018u;
        frontend_match_start();
        CHECK_EQ_INT((int)hs_figure_sprite(), (int)r0);
        /* The first match wins: with string 1 aimed at string 2's 'T', 'T'
         * selects 1 (the image's strings are distinct but for the X's). */
        {
            u32 s1 = DSD(DS_000A7DA0 + 4u), r1 = hs_ref_sprite(1u);
            CHECK(r1 != r2, "descriptors 1 and 2 differ");
            DSD(DS_000A7DA0 + 4u) = DSD(DS_000A7DA0 + 8u);
            DSW(0x105EACu + 4u + 12u) = 0x0014u;
            frontend_match_start();
            CHECK_EQ_INT((int)hs_figure_sprite(), (int)r1);
            DSD(DS_000A7DA0 + 4u) = s1;
        }
    }

    actors_reset();
    memcpy(gfx_dac, sv_dac, sizeof sv_dac);
    memcpy(gfx_aperture(), sv_ap, sizeof sv_ap);
    tf_put(sv_pb, pb, 0xEBA0u);
    tf_put(sv_pa, pa, 0x4880u);
    tf_put(sv_idx, idx, nidx);
    tf_put(sv_data, DATA_BASE, 0x8B0D0u);
}

/* Records §K2.1/§K2.2 (2026-09-29-k2-k5-derivations.md): 0x29B70 and 0x32968
 * are a bare `ret` (`c3`). The data object is filled with 0xA5, which no
 * image byte pattern guarantees, and must hold it after each call. */
static void check_null_fns(void)
{
    enum { DATA_LEN = 0x8B0D0 };
    static u8 saved[DATA_LEN];
    memcpy(saved, mem + DATA_BASE, DATA_LEN);
    memset(mem + DATA_BASE, 0xA5, DATA_LEN);
    game_null_step();
    u32 bad = 0u;
    for (u32 i = 0; i < (u32)DATA_LEN; i++)
        if (DSB(DATA_BASE + i) != 0xA5u) bad++;
    CHECK_EQ_INT((int)bad, 0);
    game_init_null();
    bad = 0u;
    for (u32 i = 0; i < (u32)DATA_LEN; i++)
        if (DSB(DATA_BASE + i) != 0xA5u) bad++;
    CHECK_EQ_INT((int)bad, 0);
    memcpy(mem + DATA_BASE, saved, DATA_LEN);
}

/* Record §K7 (2026-09-29-k7-k12-derivations.md §0.7.1, Task 2 §2): 0x1D0BC.
 * Every asserted post-value differs from its seed, or is a seeded value the
 * mutated code would overwrite. res_block_alloc(0) peeks the bump allocator's
 * next block: it aligns the heap and does not advance it. */
static void check_sound_buffers(void)
{
    u32 s[4], i, peek;
    const u32 s_c0 = DSD(DS_001028C0), s_c4 = DSD(DS_001028C4);
    const u32 s_d0 = DSD(DS_001028D0), s_c8 = DSD(DS_001028C8);
    const u8 s_b0 = DSB(DS_000A2CB0);
    for (i = 0; i < 4u; i++) s[i] = DSD(DS_00102870 + i * 0x18u);

    /* Already run (DS_000A2CB0 set, 0x1D0BF): AL = 0, nothing allocated. */
    DSB(DS_000A2CB0) = 1u;
    DSD(DS_001028C8) = 1u;
    for (i = 0; i < 4u; i++) DSD(DS_00102870 + i * 0x18u) = 0;
    CHECK_EQ_INT((int)sound_buffers_alloc(), 0);
    CHECK_EQ_INT((int)DSD(DS_00102870), 0);

    /* First run, DIG set, no sequence: the MIDI arm is skipped (C0 = 0,
     * 0x1D0D5) and slots 0..3 get 0x8C00, 0x6000, 0x6000, 0x6000 in order
     * (0x1D14D/0x1D154, the bump allocator returns consecutive blocks). */
    DSB(DS_000A2CB0) = 0;
    DSD(DS_001028C0) = 0;
    DSD(DS_001028C4) = 0;
    DSD(DS_001028D0) = 0xD0D0D0D0u;
    peek = res_block_alloc(0u);
    CHECK_EQ_INT((int)sound_buffers_alloc(), 1);
    CHECK_EQ_INT((int)DSB(DS_000A2CB0), 1);
    CHECK_EQ_INT((int)DSD(DS_001028C8), 1);
    CHECK_EQ_INT((int)DSD(DS_001028D0), (int)0xD0D0D0D0u);
    CHECK(DSD(DS_00102870) != 0u, "0x1D163 gives slot 0 a buffer");
    CHECK_EQ_INT((int)(DSD(DS_00102870 + 0x18u) - DSD(DS_00102870)), 0x8C00);
    CHECK_EQ_INT((int)(DSD(DS_00102870 + 0x30u) - DSD(DS_00102870 + 0x18u)), 0x6000);
    CHECK_EQ_INT((int)(DSD(DS_00102870 + 0x48u) - DSD(DS_00102870 + 0x30u)), 0x6000);
    CHECK_EQ_INT((int)DSD(DS_00102870), (int)peek);   /* slot 0 is the next block */
    CHECK_EQ_INT((int)(res_block_alloc(0u) - DSD(DS_00102870 + 0x48u)), 0x6000);   /* slot 3's size */

    /* The MIDI gate's C4 half (0x1D0CC): C0 set, C4 clear, no MIDI buffer is
     * taken. The slots run with slot 4's +0x10 (= DS_001028D0) clear, so a
     * loop bound past 4 (0x1D171) would allocate into DS_001028D0 too. */
    DSB(DS_000A2CB0) = 0;
    DSD(DS_001028C0) = 0x1234u;
    DSD(DS_001028C4) = 0;
    DSD(DS_001028D0) = 0;
    DSD(DS_001028C8) = 1u;
    for (i = 0; i < 4u; i++) DSD(DS_00102870 + i * 0x18u) = 0;
    CHECK_EQ_INT((int)sound_buffers_alloc(), 1);
    CHECK_EQ_INT((int)DSD(DS_001028D0), 0);
    CHECK(DSD(DS_00102870 + 0x48u) != 0u, "the four slots are allocated");

    /* The MIDI gate's C0 half (0x1D0D5): C4 set, C0 clear (the port's MDI
     * state), no MIDI buffer is taken; no DIG, so no slot is allocated. */
    DSB(DS_000A2CB0) = 0;
    DSD(DS_001028C0) = 0;
    DSD(DS_001028C4) = 0x5678u;
    DSD(DS_001028D0) = 0;
    DSD(DS_001028C8) = 0;
    CHECK_EQ_INT((int)sound_buffers_alloc(), 1);
    CHECK_EQ_INT((int)DSD(DS_001028D0), 0);

    /* The MIDI gate's D0 half (0x1D0E6): C4 and C0 set, a MIDI buffer already
     * set, nothing is taken. */
    DSB(DS_000A2CB0) = 0;
    DSD(DS_001028C0) = 0x1234u;
    DSD(DS_001028C4) = 0x5678u;
    DSD(DS_001028D0) = 0xD0D0D0D0u;
    DSD(DS_001028C8) = 0;
    CHECK_EQ_INT((int)sound_buffers_alloc(), 1);
    CHECK_EQ_INT((int)DSD(DS_001028D0), (int)0xD0D0D0D0u);

    /* The loop's stop at a set slot (0x1D176..0x1D17D): slot 2 holds a buffer,
     * so slots 0 and 1 are allocated and slots 2 and 3 are left alone. */
    DSB(DS_000A2CB0) = 0;
    DSD(DS_001028C0) = 0;
    DSD(DS_001028C4) = 0;
    DSD(DS_001028C8) = 1u;
    for (i = 0; i < 4u; i++) DSD(DS_00102870 + i * 0x18u) = 0;
    DSD(DS_00102870 + 0x30u) = 0x0BADu;
    CHECK_EQ_INT((int)sound_buffers_alloc(), 1);
    CHECK(DSD(DS_00102870 + 0x18u) != 0u, "slot 1 is allocated");
    CHECK_EQ_INT((int)DSD(DS_00102870 + 0x30u), 0x0BAD);
    CHECK_EQ_INT((int)DSD(DS_00102870 + 0x48u), 0);
    CHECK_EQ_INT((int)DSD(DS_001028C8), 1);

    /* The MIDI arm (0x1D0CC..0x1D10C): a sequence handle and no buffer yet
     * allocate 0x5100 bytes into DS_001028D0; no DIG, so the slots keep their
     * sentinels (0x1D132). */
    DSB(DS_000A2CB0) = 0;
    DSD(DS_001028C0) = 0x1234u;
    DSD(DS_001028C4) = 0x5678u;
    DSD(DS_001028D0) = 0;
    DSD(DS_001028C8) = 0;
    for (i = 0; i < 4u; i++) DSD(DS_00102870 + i * 0x18u) = 0x0BADu;
    peek = res_block_alloc(0u);
    memset(mem + peek, 0xA5, 0x5100u);        /* 0x61A70 must clear these */
    CHECK_EQ_INT((int)sound_buffers_alloc(), 1);
    CHECK(DSD(DS_001028D0) != 0u, "0x1D0F7 stores the MIDI buffer");
    CHECK_EQ_INT((int)DSD(DS_001028C0), 0x1234);
    CHECK_EQ_INT((int)DSD(DS_00102870), 0x0BAD);
    CHECK_EQ_INT((int)DSD(DS_001028D0), (int)peek);
    {
        u32 nz = 0;
        for (i = 0; i < 0x5100u; i++) if (DSB(peek + i) != 0u) nz++;
        CHECK_EQ_INT((int)nz, 0);
    }
    CHECK_EQ_INT((int)(res_block_alloc(0u) - peek), 0x5100);   /* the MIDI size */

    /* Slot 0's buffer already set at entry: the loop is skipped (0x1D147),
     * ecx stays 0 and 0x1D195 turns the DIG driver off, as the raw does. */
    DSB(DS_000A2CB0) = 0;
    DSD(DS_001028C0) = 0;
    DSD(DS_001028C4) = 0;
    DSD(DS_001028C8) = 1u;
    DSD(DS_00102870) = 0x0BADu;
    DSD(DS_00102870 + 0x18u) = 0;
    CHECK_EQ_INT((int)sound_buffers_alloc(), 1);
    CHECK_EQ_INT((int)DSD(DS_001028C8), 0);
    CHECK_EQ_INT((int)DSD(DS_00102870 + 0x18u), 0);

    /* The ISR's counter pair (PORT, 0x1BE0E..0x1BE16). */
    DSD(DS_00101508) = 0x10u;
    DSD(DS_00101500) = 0x2000u;
    game_isr_ticks(3u);
    CHECK_EQ_INT((int)DSD(DS_00101508), 0x13);
    CHECK_EQ_INT((int)DSD(DS_00101500), 0x2003);

    DSD(DS_001028C0) = s_c0; DSD(DS_001028C4) = s_c4;
    DSD(DS_001028D0) = s_d0; DSD(DS_001028C8) = s_c8;
    DSB(DS_000A2CB0) = s_b0;
    for (i = 0; i < 4u; i++) DSD(DS_00102870 + i * 0x18u) = s[i];
}

/* Record k7-k12 §2.6: the idle timeout 0x2EB80 (config_key_latched) over the
 * ISR clock. Its difference 0x500BB - DS_00105F2C (0x2EB8F..0x2EB9F) now grows
 * with every master-loop tick (game_isr_ticks), so the master-loop menu
 * 0x2FFC4 (0x303D9) reaches the timeout after 0x4B0 idle ticks. PORT (named
 * gap): the raw stores DS_00107414 = 0 (0x2EBA8) and longjmps (0x2EBB3, jmp
 * 0x65431 with 0x1044F4, 1); the port keeps only the store, so the menu
 * re-initialises on its next step. Pinned here so the behaviour cannot
 * silently change. */
static void check_idle_timeout_clock(void)
{
    const u32 s_00 = DSD(DS_00101500), s_08 = DSD(DS_00101508);
    const u32 s_2c = DSD(DS_00105F2C), s_30 = DSD(DS_00105F30);
    const u8 s_14 = DSB(DS_00107414);

    DSD(DS_00105F30) = 0;                    /* no latched key (0x2EB87) */
    DSD(DS_00101500) = 0x7000u;
    DSD(DS_00105F2C) = 0x7000u;              /* stamped now (0x2FFDA/0x2EEFB) */
    DSB(DS_00107414) = 0x5Au;
    game_isr_ticks(0x4B0u);                  /* 0x4B0 idle ticks: not over */
    CHECK_EQ_INT((int)config_key_latched(), 0);
    CHECK_EQ_INT((int)DSB(DS_00107414), 0x5A);
    game_isr_ticks(1u);                      /* one more: the timeout */
    CHECK_EQ_INT((int)config_key_latched(), 0);
    CHECK_EQ_INT((int)DSB(DS_00107414), 0);

    DSD(DS_00101500) = s_00; DSD(DS_00101508) = s_08;
    DSD(DS_00105F2C) = s_2c; DSD(DS_00105F30) = s_30;
    DSB(DS_00107414) = s_14;
}

int test_flow(void)
{
    int before = g_failures;

    check_bank_guard();
    check_localised_string();

    /* Resources must be in mem[] for the title to decode. Earlier tests load
     * them; only load if this test runs first (a second load would exhaust the
     * bump allocator's 64 MB mem[]). */
    if (res_count() != 69)
        CHECK(res_load_index("data/game/C", "data/game/C/INDEX") == 69,
              "resource index loads");
    CHECK_EQ_INT(res_count(), 69);

    /* surface_setup is internal to flow.c; stand its two bindings up directly
     * (the same fields 0x51F45 assigns) so game_frame has a draw target. */
    DSD(DS_000E87A0) = DSD(DS_001014E4);
    DSD(DS_000E87A4) = DSD(DS_001014E8);

    /* The update process table is the engine's extension seam: a registered
     * function at a live bit must be called by game_frame(). */
    fn_register(FN_0004FBA2, probe_task);   /* address no other test registers */
    DSD(DS_000A8644) = FN_0004FBA2;
    DSD(DS_00104AE8) = 1;
    g_called = 0;

    DSD(DS_00104B00) = 3;      /* the state-machine mode 0x24C5C switches on */
    DSW(DS_000F0A64) = 1;      /* title state (chosen, see port/spec) */
    DSB(DS_000A81A8) = 0;      /* not quitting */
    DSB(DS_00104B1D) = 1;      /* skip the deferred menu poll */
    /* This test drives the state machine without game_init(), so stand up the
     * boot RNG seed that game_init() installs (0x20C62). Nothing consumes RNG
     * between that seed and the title's three draws, so they are the first
     * three from 0xABCD and land on the Task 1 pin (12, 111, 0). */
    rng_seed(0xABCDu);
    u32 frame0 = DSD(DS_000EF6DC);

    /* Task 9: the real title. 0x121A0's entry frame resets and repopulates the
     * actor pools; Format reference G pins the entry-frame invariants:
     * DS_00107A50 = 0x2420, DS_00107A3A = 0x121, DS_000F0A66 = 0x600. */
    game_frame();

    CHECK(g_called == 1, "game_frame runs the update process table");
    CHECK_EQ_INT(DSD(DS_000EF6DC), (long)frame0 + 1);
    DSD(DS_00104AE8) = 0;

    CHECK(actor_list_head() != 0, "title entry allocated actor records");
    CHECK_EQ_INT((int)DSW(DS_000F0A66), 0x600);
    CHECK_EQ_INT((int)DSW(DS_00107A50), 0x2420);
    CHECK_EQ_INT((int)DSW(DS_00107A3A), 0x121);
    {
        u32 logo = DSD(DS_000F0A58);
        CHECK(logo != 0, "title logo record exists");
        if (logo != 0)
            CHECK(DSD(actor_pset(logo) + 0x18) != 0,
                  "logo pset carries a palette handle");
    }

    /* Phase 0's text branch ("THE FUTURE...") spawned one actor per non-space
     * glyph into the 31x43 record grid at DS_00105F38; spaces release a cell and
     * never spawn. */
    {
        int glyphs = 0;
        for (u32 i = 0; i + 4u <= 0x14D4u; i += 4u)
            if (DSD(DS_00105F38 + i) != 0) glyphs++;
        CHECK(glyphs >= 10, "title caption laid out its glyphs in the grid");
    }

    /* DS_000F0A66 decreases 0x10 per presented title frame from 0x600. */
    for (int i = 0; i < 8; i++) {
        u16 before = DSW(DS_000F0A66);
        game_frame();
        CHECK_EQ_INT((int)DSW(DS_000F0A66), (int)before - 0x10);
    }

    /* The title enqueued a palette record; flushing it fills the DAC. */
    gfx_dac[1][0] = gfx_dac[1][1] = gfx_dac[1][2] = 0;
    gfx_flush_palette();
    CHECK(gfx_dac[1][0] || gfx_dac[1][1] || gfx_dac[1][2],
          "title palette reached gfx_dac");

    /* Record §K3.2: 0x24C5C calls 0x38990 at 0x24CC8 every frame, before the
     * frame counter and the update table, in every mode. A mode-1 frame (the
     * bare `ret` 0x29B70) must derive DS_00107A3C/DS_00107A4A from the seeded
     * DS_000F0AEC/DS_00107A4E; the sentinels differ from both post-values. */
    {
        u32 sf0 = DSD(DS_000F0AEC), sdc = DSD(DS_000EF6DC);
        u16 s3c = DSW(DS_00107A3C), s4a = DSW(DS_00107A4A);
        u16 s4e = DSW(DS_00107A4E);
        DSD(DS_000F0AEC) = 0x00012345u;
        DSW(DS_00107A4E) = 0x0010u;
        DSW(DS_00107A3C) = 0x7777u;
        DSW(DS_00107A4A) = 0x7777u;
        DSD(DS_00104B00) = 1;
        game_frame();
        CHECK_EQ_INT((int)DSW(DS_00107A3C), 0x2340);
        CHECK_EQ_INT((int)DSW(DS_00107A4A), 0x049D);
        DSD(DS_000F0AEC) = sf0; DSD(DS_000EF6DC) = sdc;
        DSW(DS_00107A3C) = s3c; DSW(DS_00107A4A) = s4a;
        DSW(DS_00107A4E) = s4e;
    }
    /* A mode other than 3 must not run the state machine at all. 0x16 is a
     * still-unported named gap (`game_frame`'s generic no-op case list) as
     * of record §49-F; mode 7 no longer is (game_mode_07_step, record
     * §49-E) — it now runs the fight-frame's unconditional fight_slot_pass/
     * fight_effects_pass chain, which this title-state fixture's actor/
     * effects-list state was never built to tolerate (it hangs in
     * fight_4b69c's trample walk). */
    DSD(DS_00104B00) = 0x16;
    game_frame();

    check_title_overlay();

    /* Task 11: the init chain's audio calls and the title state's music request
     * drive the sequencer with no device open (the suite never opens one).
     * The title state above asked for music; the master-loop service inherits
     * that request, loads the S16TITLE bank and ticks it. */
    game_set_game_dir("data/game/C");
    /* 0x1CF8E: the DIG driver handle lands at DS_001028C8 (the port's non-zero
     * stand-in, record §45-A); 0x1D0A9 clears it at the teardown below. */
    DSD(DS_001028C8) = 0;
    game_audio_init();
    CHECK_EQ_INT((int)DSD(DS_001028C8), 1);
    check_sound_buffers();
    check_idle_timeout_clock();
    /* The init chain must load the FM patch bank (FAT.OPL): the sequencer maps
     * every program change through it, and without it a key-on carries no
     * operator setup, so the OPL core renders silence for the whole run (the
     * live title was silent until this load existed). This test runs before the
     * audio tests, so the count is 0 here unless the init path loaded it. */
    CHECK_EQ_INT(patches_count(), 181);
    CHECK_EQ_INT((int)game_audio_ticks(), 0);
    /* Record §K7 (k7-k12 derivations §0.7.5): the raw's title plays no
     * sample. The sample path is proved end to end on the attract's looping
     * 0x40 (s16title 0x0383B6F4, loop byte 1): 0x1D0BC's buffers, 0x1CC28's
     * queue, 0x1CF20 -> 0x1CB18's start. The title bank's first note is at
     * XMIDI tick 59, so this single-tick render is before the FM sounds and
     * any non-silence is the sample's. */
    DSB(DS_000A2CB0) = 0;
    for (u32 k = 0; k < 4u; k++) DSD(DS_00102870 + k * 0x18u) = 0;
    CHECK_EQ_INT((int)sound_buffers_alloc(), 1);
    CHECK_EQ_INT((int)sound_voice(0x40u), 1);
    host_wait_vblank();
    game_audio_service();
    CHECK(mixer_active_voices() > 0, "the queued 0x40 became an active mixer voice");
    {
        static s16 abuf[4096 * 2];
        mixer_render(abuf, 4096, MIXER_OPL_RATE);
        int nz = 0;
        for (int i = 0; i < 4096 * 2; i++) if (abuf[i]) { nz = 1; break; }
        CHECK(nz, "mixer rendered non-silence with the 0x40 sample active");
    }
    /* The service is paced by the host's 60 Hz clock, not the loop count, so
     * drive that clock here: one host_wait_vblank() per service advances it one
     * tick, which becomes two sequencer ticks. */
    for (int i = 0; i < 40; i++) {
        host_wait_vblank();
        game_audio_service();
    }
    CHECK(game_audio_ticks() > 0, "audio service advances the sequencer");
    CHECK(game_music_notes_seen(), "title music keys notes without a device");
    /* 0x40's record: case 2, handle 0x0383B6F4, loop byte 1, so 0x1CB18 sets
     * loop count 0 (0x1CBE1). Its 0x5FAE frames at 0x2B11 Hz last about
     * 110,450 output frames at 49716 Hz: as a one-shot it ends within 26 of
     * these 4096-frame renders after the first (measured: 25 leave it
     * active), so a voice left after 32 is the loop. 0x41 (case 5, 0x2C7A5)
     * stops it. */
    CHECK_EQ_INT((int)DSD(DS_000BBDC8 + 0x40u * 12u + 4u), 0x0383B6F4);
    {
        static s16 abuf2[4096 * 2];
        for (int i = 0; i < 32; i++) mixer_render(abuf2, 4096, MIXER_OPL_RATE);
        CHECK_EQ_INT(mixer_active_voices(), 1);
        CHECK_EQ_INT((int)sound_voice(0x41u), 1);
        CHECK_EQ_INT(mixer_active_voices(), 0);
    }
    check_sample_slots();
    /* The voice dispatcher 0x2C3FC and the sound module, on the live handles
     * game_audio_init allocated (record §45-A). */
    check_sound_voice();
    check_voice_wrappers();
    check_sound_pause_volume();
    /* The attract's high-score screen 0x1EA08 (record §46-A). */
    check_hiscore_screen();

    game_shutdown();                         /* release handles for later tests */
    CHECK_EQ_INT((int)DSB(DS_000A2CB1), 0);  /* teardown clears the enable flag */
    CHECK_EQ_INT((int)DSD(DS_001028C8), 0);  /* 0x1D0A9 */
    /* 0x1CE70 tests status 4 exactly: a released handle (status 0) is not
     * playing, so its slot's +0x0C is cleared and the voice goes on to
     * 0x1CC28, which the pause byte stops before any read (AL = 1). */
    {
        u32 s_c8 = DSD(DS_001028C8), s_6c = DSD(DS_0010286C);
        u8 s_db = DSB(DS_001028DB);
        CHECK_EQ_INT((int)AIL_sample_status(sound_slot_handle(0u)), 0);
        DSD(DS_001028C8) = 1u;
        DSB(DS_001028DB) = 1u;
        DSD(DS_0010286C) = 0x1800EBC9u;
        CHECK_EQ_INT((int)sound_voice(0x3Eu), 1);
        CHECK_EQ_INT((int)DSD(DS_0010286C), 0);
        DSD(DS_001028C8) = s_c8;
        DSB(DS_001028DB) = s_db;
        DSD(DS_0010286C) = s_6c;
    }

    /* Task 9: the state-9 countdown's faithfulness and its no-draw invariant. */
    check_state9_countdown();

    /* Task 3: the 0x38730 projection setup. It needs the resource table (above)
     * and a live actor pool (the title entry rebuilt it), so it runs here. */
    check_scroll_setup();

    /* 0x11A8C: the live state 6. The old "a deferred state is a harmless no-op"
     * premise is retired — state 6 now draws the shared RNG, runs 0x41350 for
     * both players, spawns the HUD path and arms the 900-frame timer. It is
     * exercised last, after the title-overlay and audio assertions, so its text
     * and overlay writes cannot corrupt them. */
    DSD(DS_00104B00) = 3;
    DSB(DS_00104B1D) = 1;                    /* skip the deferred menu poll */
    DSB(DS_000F0A71) = 0;                    /* the tails must run, not short-circuit */
    DSB(DS_00104B15) = 0;
    DSB(DS_00104B19 + 2u) = 0;
    DSW(DS_001082CC) = 0;
    DSW(DS_000F0A6A) = 0;
    DSW(DS_000F0A72) = 5;
    DSW(DS_000F0A6C) = 0;
    DSB(DS_000F0A6F) = 0xFF;
    DSW(DS_000F0A64) = 6;
    /* Task 3: the 0x20DF4 reset path must run 0x38730, which seeds the
     * scroll/zoom projection and sets DS_00107A54 = 1. The sentinel differs
     * from the post-condition, so a missing store fails rather than passing on
     * a zero the test itself left behind. */
    DSB(DS_00107A54) = 0;
    /* 0x20DF4 zeroes the projection's camera stride input at 0x20E52 before its
     * 0x38730 call. Seed it non-zero: without that zeroing render_scroll_fill's
     * stride is 0x100 << 8, so the first shear row (index DS_00107A52 - 1 = 0x53)
     * is (0x10000 / 256) >> 1 = 0x80 instead of 0. */
    DSD(DS_000F0AF0) = 0x100;
    DSW(DS_00107900 + 0x53u * 2u) = 0xDEAD;
    /* 0x20E78 0x2BAF4(EAX=1): the branch's actor-pool reset. Seed a live sentinel
     * actor and mark its record at +0x44 (0x2AE14 writes that word 0); the pool
     * fill must zero the marker. The seeded value differs from the
     * post-condition, so a missing reset cannot pass on a record the test never
     * touched, and a record the state-6 spawns reuse is written 0 by the spawn. */
    u32 sentinel = actor_spawn((const u32 *)(mem + 0x9AC30u), 0x4840u, 0xE0u,
                               0x1B00u, 0u);
    CHECK(sentinel != 0, "the state-6 reset test seeds a live sentinel actor");
    DSW(sentinel + 0x44u) = 0x5EEDu;
    rng_seed(0xABCDu);
    game_frame();
    CHECK_EQ_INT((int)DSW(sentinel + 0x44u), 0);
    CHECK_EQ_INT((int)DSB(DS_00107A54), 1);
    CHECK_EQ_INT((int)DSW(DS_00107900 + 0x53u * 2u), 0);
    CHECK_EQ_INT((int)DSB(DS_00104B15), 1);
    CHECK_EQ_INT((int)DSW(DS_001082CC), 3);
    CHECK_EQ_INT((int)DSW(DS_000F0A64), 7);
    CHECK_EQ_INT((int)DSW(DS_000F0A6A), 900);
    CHECK_EQ_INT((int)DSW(DS_000F0A6C), 5);
    CHECK_EQ_INT((int)DSB(DS_000F0A6F), 0);

    /* Task 7: the timer exit and its handoff to the attract sub-machine. Runs
     * last and restores every global it seeds, so it cannot perturb the
     * assertions above. */
    check_timer_exit();

    check_null_fns();

    return g_failures - before;
}

/* ---- test_actors.c ---- */

/* PORT: descriptor 0x9AC30 is the title logo's (data object offset 0x1AC30,
 * resident in mem[] after test_le). Its pinned title call-site arguments are
 * actor_spawn(0x9AC30, 0x4840, 0xE0, 0x1B00, 0); a5 = 0 is also the 0x2AC80
 * alloc flag word — args doc §1 site 2, §3. */
static void check_actor_spawn(void)
{
    actors_reset();
    const u32 *desc = (const u32 *)(mem + 0x9AC30u);
    u32 rec = actor_spawn(desc, 0x4840u, 0xE0u, 0x1B00u, 0u);
    CHECK(rec != 0, "spawn returns a record");
    if (rec == 0) return;
    CHECK_EQ_INT((int)actor_index(rec), 0);
    CHECK_EQ_INT((int)DSW(rec + 0x2e), 0x0000);   /* desc+0x06 */
    CHECK_EQ_INT((int)DSB(rec + 0x48), 0x00);     /* desc+0x04 render type */
    CHECK_EQ_INT((int)DSW(rec + 0x40), 0x2000);   /* desc+0x0A << 6 */
    CHECK_EQ_INT((int)DSW(rec + 0x2c), 0x0010);   /* desc+0x0C */
    CHECK_EQ_INT((int)(DSW(rec + 0x28) & 0xC3), 0);
    CHECK_EQ_INT((int)DSW(rec + 0x56), 0);        /* pset slot */
    CHECK_EQ_INT((int)DSD(rec + 0x18), 0x4840);   /* a2 */
    CHECK_EQ_INT((int)DSD(rec + 0x1c), 0x1B00);   /* a4 */
    u32 pset = actor_pset(rec);
    CHECK_EQ_INT((int)DSB(rec + 0x5f), 1);
    CHECK_EQ_INT((int)DSW(pset + 0x02), 0x0800);  /* rec+0x2E | hflip */

    /* Pool exhaustion: actor_alloc returns 0, so actor_spawn must too, leaving
     * both lists untouched. */
    actors_reset();
    for (u32 i = 0; i < 580; i++) CHECK(actor_alloc(0) != 0, "fill");
    u32 active = actor_list_head();
    u32 free_head = DSD(DS_00105B3C);
    CHECK_EQ_INT((int)actor_spawn(desc, 0x4840u, 0xE0u, 0x1B00u, 0u), 0);
    CHECK_EQ_INT((int)actor_list_head(), (int)active);
    CHECK_EQ_INT((int)DSD(DS_00105B3C), (int)free_head);
}

/* 0x2A31C -> 0x2A1FC -> 0x2A820/0x2A690. The title logo (descriptor 0x9AC30)
 * spawns with the 0x2000 flag, so 0x2A820 takes its high-byte-0x20 branch; the
 * test clears that bit to route through 0x2A690 and exercises the transformed
 * position and the layer from rec+0x59. */
static void check_pset_sync(void)
{
    const u32 *desc = (const u32 *)(mem + 0x9AC30u);

    /* 0x2A690: pset x = rec+0x18 - DS_000F0AF0 + 0x2A00; y =
     * DS_000F0AEC + 0x3BC0 - (rec+0x30>>16) - rec+0x1C. */
    actors_reset();
    DSB(DS_00104B24) = 0;                 /* 0x2A31C's gate */
    DSB(DS_00104B26) = 0;                 /* 0x2A1FC's 0x2A820 short-circuit */
    DSD(DS_000F0AF0) = 0;
    DSD(DS_000F0AEC) = 0;
    u32 rec = actor_spawn(desc, 0x4840u, 0xE0u, 0x1B00u, 0u);
    CHECK(rec != 0, "pset sync record");
    if (rec == 0) return;
    DSW(rec + 0x28) &= 0xdfffu;           /* clear 0x2000: take the 0x2A690 path */
    DSD(rec + 0x18) = 0x1000u;
    DSD(rec + 0x1c) = 0x1000u;
    DSW(rec + 0x2c) = 0x00aa;
    DSB(rec + 0x59) = 8;
    DSB(rec + 0x5a) = 0x11;               /* not read by the sync */
    u32 pset = actor_pset(rec);
    actors_update();
    CHECK_EQ_INT((int)DSD(pset + 0x04), 0x3a00);
    CHECK_EQ_INT((int)DSD(pset + 0x08), 0x2bc0);
    CHECK_EQ_INT((int)DSW(pset + 0x0e), 0x00f8);   /* (s8)8 + 0xF0 */
    CHECK_EQ_INT((int)DSW(pset + 0x0c), 0x00aa);
    CHECK_EQ_INT((int)DSD(rec + 0x3c), 0x3a00);    /* mirrors the written pset x */
    CHECK_EQ_INT((int)(DSB(rec + 0x2b) & 0x18), 0x18);  /* on-screen */

    /* 0x2A39C: rec+0x28 & 4 clears the bit and writes pset+0 from 0x2A408. The
     * logo stream (0x0E9116) begins with the literal 0x2C11, so the reader
     * writes it back unchanged. */
    actors_reset();
    DSB(DS_00104B24) = 0;
    DSB(DS_00104B26) = 0;
    u32 r2 = actor_spawn(desc, 0x4840u, 0xE0u, 0x1B00u, 0u);
    CHECK(r2 != 0, "anim-id record");
    if (r2 != 0) {
        DSD(r2 + 0x24) = 0;               /* no frame_timer, so only 0x2A39C runs */
        DSW(r2 + 0x28) |= 4u;
        u32 p2 = actor_pset(r2);
        DSW(p2 + 0x00) = 0x1234;
        actors_update();
        CHECK_EQ_INT((int)(DSW(r2 + 0x28) & 4u), 0);
        CHECK_EQ_INT((int)DSW(p2 + 0x00), 0x2C11);   /* 0x2A408 literal */
    }

    /* 0x2A820's on-screen test: a record far outside its extent does not get the
     * +0x2B 0x18 visibility bits. */
    actors_reset();
    DSB(DS_00104B24) = 0;
    DSB(DS_00104B26) = 0;
    DSD(DS_000F0AF0) = 0;
    DSD(DS_000F0AEC) = 0;
    u32 r3 = actor_spawn(desc, 0x4840u, 0xE0u, 0x1B00u, 0u);
    CHECK(r3 != 0, "offscreen record");
    if (r3 != 0) {
        DSD(r3 + 0x18) = 0x100000u;       /* beyond extent + 0x5400 */
        DSD(r3 + 0x1c) = 0x1000u;
        DSW(r3 + 0x2c) = 0x00aa;
        DSB(r3 + 0x2b) &= (u8)~0x18u;
        actors_update();
        CHECK_EQ_INT((int)(DSB(r3 + 0x2b) & 0x18), 0);
    }
}

/* 0x2F0F0/0x2F198/0x2F280/0x2F4BC. Plan Format reference H calls these "pset
 * layer select"; the shipped machine is a display-string width / text-cursor /
 * record-grid group (see the PORT note in actors.c). Expected values below are
 * hand-computed from the disassembly and the loaded font tables at DS
 * 0x3D048 / 0x3D1EC / 0x3D38D. */
static void check_pset_layer(void)
{
    actors_reset();

    /* 0x2F0F0. Mode & 3 in {0,1}: strlen. In {2,3}: class = (s8)DSB(0xBD390+c);
     * a negative class is skipped; otherwise add
     * (DSB(table + class*4 + 2) == 8 ? 1 : 2). Loaded data: class('A')=10,
     * class('B')=11 (both widths 16 -> 2); class('I')=18 (table A 16 -> 2,
     * table B 8 -> 1); class('"')=255 (skipped). */
    CHECK_EQ_INT(text_width((const u8 *)"", 0), 0);
    CHECK_EQ_INT(text_width((const u8 *)"ABC", 0), 3);
    CHECK_EQ_INT(text_width((const u8 *)"ABC", 1), 3);
    CHECK_EQ_INT(text_width((const u8 *)"A\"I", 2), 4);
    CHECK_EQ_INT(text_width((const u8 *)"A\"I", 3), 3);

    /* 0x2F198. The cursor is the word pair at DS_00105F34: low = row, high =
     * col + 0x2F830's glyph count (Task 8b; the extent was a 0-returning seam). */
    DSD(DS_00105F34) = 0;
    text_cursor_set(5, 7, (const u8 *)"A", 0);
    CHECK_EQ_INT((int)DSW(DS_00105F34), 7);
    CHECK_EQ_INT((int)DSW(DS_00105F34 + 2), 6);     /* col 5 + 1 glyph */

    DSD(DS_00105F34) = 0;
    text_cursor_set(-1, 9, (const u8 *)"ABC", 0);   /* col = (0x2b - 3) >> 1 */
    CHECK_EQ_INT((int)DSW(DS_00105F34), 9);
    CHECK_EQ_INT((int)DSW(DS_00105F34 + 2), 23);    /* col 20 + 3 glyphs */

    DSW(DS_00105F34) = 0x20;                        /* row == -1 reuses it */
    DSW(DS_00105F34 + 2) = 0x10;
    text_cursor_set(-1, -1, (const u8 *)"", 0);
    CHECK_EQ_INT((int)DSW(DS_00105F34), 0x20);
    CHECK_EQ_INT((int)DSW(DS_00105F34 + 2), 0x10);

    /* 0x2F4BC saves/restores DS_00105F34 around the same call. */
    DSD(DS_00105F34) = 0x12345678;
    text_cursor_hold(1, 2, (const u8 *)"A", 0);
    CHECK_EQ_INT((int)DSD(DS_00105F34), (int)0x12345678u);

    /* 0x2F280 clears `text_width` consecutive cells on the 43-wide diagonal
     * and returns each record through 0x2AD40 (pset+0x0E zeroed, dead bit). */
    actors_reset();
    u32 rec = actor_spawn((const u32 *)(mem + 0x9AC30u), 0x4840u, 0xE0u,
                          0x1B00u, 0u);
    CHECK(rec != 0, "grid record");
    if (rec != 0) {
        u32 pset = actor_pset(rec);
        DSW(pset + 0x0e) = 0xabcd;
        DSD(DS_00105F38) = rec;                     /* row 0, col 0 */
        text_cells_release(0, 0, (const u8 *)"ABC", 0);   /* strlen = 3 */
        CHECK_EQ_INT((int)DSD(DS_00105F38), 0);
        CHECK_EQ_INT((int)DSW(pset + 0x0e), 0);
        CHECK_EQ_INT((int)(DSW(rec + 0x28) & 8u), 8);
    }

    /* A centered run starts at col (0x2b - width) >> 1. */
    actors_reset();
    rec = actor_spawn((const u32 *)(mem + 0x9AC30u), 0x4840u, 0xE0u,
                      0x1B00u, 0u);
    CHECK(rec != 0, "centered grid record");
    if (rec != 0) {
        DSD(DS_00105F38 + 20u * 4u) = rec;          /* row 0, col 20 */
        text_cells_release(-1, 0, (const u8 *)"ABC", 0);
        CHECK_EQ_INT((int)DSD(DS_00105F38 + 20u * 4u), 0);
    }

    /* Zero width clears nothing. */
    actors_reset();
    rec = actor_spawn((const u32 *)(mem + 0x9AC30u), 0x4840u, 0xE0u,
                      0x1B00u, 0u);
    CHECK(rec != 0, "empty-string grid record");
    if (rec != 0) {
        DSD(DS_00105F38) = rec;
        text_cells_release(0, 0, (const u8 *)"", 0);
        CHECK_EQ_INT((int)DSD(DS_00105F38), (int)rec);
    }
}

/* Update-table entry 6, 0x25FAC (flow_card_ramp_step, record §K8a): byte
 * [DSD(DS_00104ACC)+0x2D] += 1 (0x25FB2), then once the zero-extended word
 * +0x2C is >= 0x1000 (0x25FBB `cmp edx,0x1000; jl`) it is clamped to 0x1000
 * and DS_00104AE8 bit 0x40 is cleared (0x25FC3..0x25FD2, a byte store). The
 * record is scratch at 0x3E90000; every sentinel differs from its post-value.
 * Runs after actors_init, which registers the entry. */
#define CARD_REC 0x3E90000u
static void check_card_ramp(void)
{
    u32 s_acc = DSD(DS_00104ACC), s_ae8 = DSD(DS_00104AE8);
    u32 s_mode = DSD(DS_00104B00), s_dc = DSD(DS_000EF6DC);
    u16 s3c = DSW(DS_00107A3C), s4a = DSW(DS_00107A4A);
    u8 s_b15 = DSB(DS_00104B15), s_b1b = DSB(0x00104B1Bu);

    /* (a) The table dword at 0xA865C (entry 6) is 0x25FAC, and it resolves. */
    CHECK_EQ_INT((int)DSD(DS_000A8644 + 6u * 4u), 0x25FAC);
    CHECK(fn_resolve(DSD(DS_000A8644 + 6u * 4u)) == flow_card_ramp_step,
          "update-table entry 6 (0x25FAC) resolves to flow_card_ramp_step");

    DSD(DS_00104ACC) = CARD_REC;
    DSD(CARD_REC + 0x28u) = 0xA5A5A5A5u;
    DSD(CARD_REC + 0x30u) = 0x5A5A5A5Au;
    DSW(CARD_REC + 0x2Eu) = 0xBEEFu;

    /* (b) 0x0010 -> 0x0110 (below 0x1000): the mask is kept whole. */
    DSW(CARD_REC + 0x2Cu) = 0x0010u;
    DSD(DS_00104AE8) = 0xFFFFFFFFu;
    flow_card_ramp_step();
    CHECK_EQ_INT((int)DSW(CARD_REC + 0x2Cu), 0x0110);
    CHECK_EQ_INT((int)DSD(DS_00104AE8), (int)0xFFFFFFFFu);

    /* (c) 0x0FFF -> 0x10FF >= 0x1000: clamped to 0x1000, and only bit 0x40
     * of the mask's low byte is cleared. */
    DSW(CARD_REC + 0x2Cu) = 0x0FFFu;
    flow_card_ramp_step();
    CHECK_EQ_INT((int)DSW(CARD_REC + 0x2Cu), 0x1000);
    CHECK_EQ_INT((int)DSD(DS_00104AE8), (int)0xFFFFFFBFu);

    /* (d) 0x7F10 -> 0x8010: the zero-extended compare clamps it (a signed
     * 16-bit compare would not). */
    DSW(CARD_REC + 0x2Cu) = 0x7F10u;
    DSD(DS_00104AE8) = 0x00000040u;
    flow_card_ramp_step();
    CHECK_EQ_INT((int)DSW(CARD_REC + 0x2Cu), 0x1000);
    CHECK_EQ_INT((int)DSD(DS_00104AE8), 0);

    /* (e) 0xFF10: the byte increment wraps +0x2D to 0 (no carry out of the
     * word), 0x0010 < 0x1000, the mask is kept. */
    DSW(CARD_REC + 0x2Cu) = 0xFF10u;
    DSD(DS_00104AE8) = 0x00000040u;
    flow_card_ramp_step();
    CHECK_EQ_INT((int)DSW(CARD_REC + 0x2Cu), 0x0010);
    CHECK_EQ_INT((int)DSD(DS_00104AE8), 0x40);
    CHECK_EQ_INT((int)DSD(CARD_REC + 0x28u), (int)0xA5A5A5A5u);
    CHECK_EQ_INT((int)DSD(CARD_REC + 0x30u), 0x5A5A5A5A);
    CHECK_EQ_INT((int)DSW(CARD_REC + 0x2Eu), 0xBEEF);   /* no carry into +0x2E */

    /* (f) The dispatcher: a mode-1 game_frame with only bit 0x40 armed runs
     * entry 6 once (0x24CEF `call [eax+0xa8644]`), 0x0210 -> 0x0310. */
    DSW(CARD_REC + 0x2Cu) = 0x0210u;
    DSD(DS_00104AE8) = 0x00000040u;
    DSD(DS_00104B00) = 1u;
    /* The demo's per-frame blocks (0x24C7C's AI, gated on DS_00104B1B, and
     * the 0x25414 tail, gated on DS_00104B15) are held off for this frame
     * so it touches no fighter state the later cases rely on. */
    DSB(DS_00104B15) = 0u; DSB(0x00104B1Bu) = 0u;
    game_frame();
    CHECK_EQ_INT((int)DSW(CARD_REC + 0x2Cu), 0x0310);
    CHECK_EQ_INT((int)DSD(DS_00104AE8), 0x40);

    DSD(CARD_REC + 0x28u) = 0; DSD(CARD_REC + 0x2Cu) = 0; DSD(CARD_REC + 0x30u) = 0;
    DSD(DS_00104ACC) = s_acc; DSD(DS_00104AE8) = s_ae8;
    DSD(DS_00104B00) = s_mode; DSD(DS_000EF6DC) = s_dc;
    DSW(DS_00107A3C) = s3c; DSW(DS_00107A4A) = s4a;
    DSB(DS_00104B15) = s_b15; DSB(0x00104B1Bu) = s_b1b;
}
#undef CARD_REC

/* Update-table entries 4, 8, 9, 11, 12 and 17 (record §K8c,
 * 2026-09-29-k8c-derivations.md). Each is driven directly with a seeded
 * mem[] (no no-input path arms any of them, §K8c.0; the setters of 9, 12
 * and 17 are record §D8's, tested in check_anim_setters). Scratch records sit at 0x3E92000; the actor pool is
 * filled with 0xA5 before the reset so every spawned field is a real store.
 * Runs after actors_init, which registers the six entries. */
#define K8C_SCR    0x3E92000u
#define K8C_SLOT0  0x001077B0u
#define K8C_SLOT1  (0x001077B0u + 0x94u)
static void check_update_k8c(void)
{
    static const u32 k_idx[6] = { 4u, 8u, 9u, 11u, 12u, 17u };
    static const u32 k_addr[6] = { 0x37C8Cu, 0x34648u, 0x3800Cu, 0x4F890u,
                                   0x24150u, 0x45D98u };
    void (*const k_fn[6])(void) = { fighter_37c8c, fighter_34648,
                                    fighter_3800c, fighter_4f890,
                                    fighter_24150, fighter_45d98 };
    u8 s_slots[0x160], s_88e8[0x10], s_8180[0x70], s_5b34[2], s_pal[0x180];
    u32 s_ae8 = DSD(DS_00104AE8), s_dc = DSD(DS_000EF6DC), s_rng = DSD(DS_000EF6D8);
    u32 s_4744 = DSD(0x00104744u), s_ad4 = DSD(DS_00104AD4);
    u32 s_9384 = DSD(0x000C9384u), s_row7 = DSD(DS_000A8A98 + 7u * 4u);
    u8 s_4770 = DSB(0x00104770u);
    u32 i, rec, head, pset, seed;
    tf_snap(s_slots, 0x001077A0u, 0x160u);
    tf_snap(s_88e8, 0x001088E8u, 0x10u);
    tf_snap(s_8180, 0x00108180u, 0x70u);
    tf_snap(s_5b34, DS_00105B34, 2u);

    /* (a) The table dwords (0xA8654/64/68/70/74/88) and their resolution. */
    for (i = 0; i < 6u; i++) {
        CHECK_EQ_INT((int)DSD(DS_000A8644 + k_idx[i] * 4u), (int)k_addr[i]);
        CHECK(fn_resolve(k_addr[i]) == k_fn[i],
              "update-table entry resolves to its K8c port");
    }

    memset(mem + DSD(DS_001014F4), 0xA5, 0xEBA0);
    actors_reset();

    /* (b) Entry 4, 0x37C8C. Word +0x34 = 0: bit 0x10 of AE8 cleared, no
     * spawn. Nonzero: frame word & 3 != 0 does nothing; & 3 == 0 spawns
     * 0xBB1DC at (slot+0x2C, rec+0x30 >> 16) (the descriptor's +8 word 0x0080
     * has no 0x2000 bit, so a3 lands in +0x32), a4 = 0. */
    DSD(0x001078E0u) = K8C_SCR;
    DSD(K8C_SCR) = K8C_SCR + 0x100u;
    DSD(K8C_SCR + 0x2Cu) = 0x00012345u;
    DSD(K8C_SCR + 0x130u) = 0xFFF00000u;
    DSW(K8C_SCR + 0x134u) = 0;
    DSD(DS_00104AE8) = 0xFFFFFFFFu;
    DSD(DS_000EF6DC) = 0x00010004u;
    fighter_37c8c();
    CHECK_EQ_INT((int)DSD(DS_00104AE8), (int)0xFFFFFFEFu);
    CHECK_EQ_INT((int)actor_list_head(), 0);
    DSW(K8C_SCR + 0x134u) = 5u;
    DSD(DS_00104AE8) = 0x10u;
    DSD(DS_000EF6DC) = 0x00010001u;
    fighter_37c8c();
    CHECK_EQ_INT((int)actor_list_head(), 0);
    CHECK_EQ_INT((int)DSD(DS_00104AE8), 0x10);
    DSD(DS_000EF6DC) = 0x00010002u;            /* bit 1 alone also skips */
    fighter_37c8c();
    CHECK_EQ_INT((int)actor_list_head(), 0);
    DSD(DS_000EF6DC) = 0x00010004u;
    fighter_37c8c();
    head = actor_list_head();
    CHECK(head != 0u, "entry 4 spawns on a frame word with (w & 3) == 0");
    if (head != 0u) {
        CHECK_EQ_INT((int)DSD(head + 0x18u), 0x12345);
        CHECK_EQ_INT((int)DSW(head + 0x32u), 0xFFF0);
        CHECK_EQ_INT((int)DSD(head + 0x1Cu), 0);
    }
    CHECK_EQ_INT((int)DSD(DS_00104AE8), 0x10);

    /* (c) 0x29C20 on a scratch row as character 7's (0xA8AB4), side 1:
     * DS_00105B34 = {1, 2}. Flag (DL only) -> row[2]; no flag with a
     * nonzero index -> row[0]; index 0 -> row[1] (flag -> row[0]). */
    DSD(DS_000A8A98 + 7u * 4u) = K8C_SCR + 0x200u;
    DSD(K8C_SCR + 0x200u) = 0x11111111u;
    DSD(K8C_SCR + 0x204u) = 0x22222222u;
    DSD(K8C_SCR + 0x208u) = 0x33333333u;
    DSB(DS_001078FF) = 1u;
    DSB(DS_00105B34) = 1u;
    DSB(DS_00105B34 + 1u) = 2u;
    CHECK_EQ_INT((int)fighter_29c20(7u, 1u), 0x33333333);
    CHECK_EQ_INT((int)fighter_29c20(7u, 0x100u), 0x11111111);
    DSB(DS_00105B34 + 1u) = 0u;
    CHECK_EQ_INT((int)fighter_29c20(7u, 0u), 0x22222222);
    CHECK_EQ_INT((int)fighter_29c20(7u, 1u), 0x11111111);

    /* (d) Entry 8, 0x34648. W = the image word 0xBDBE4 = 1. An odd frame
     * word returns; else the flag is (word & 2). Side 1's record takes
     * 0x29C20(char 7, flag) through 0x2A17C with word 0: pset +2 = 0x800 (the
     * record's +0x5F is set), and +0x18 changes only for a nonzero handle. */
    CHECK_EQ_INT((int)DSW(0x000BDBE4u), 1);
    tf_snap(s_pal, DS_00107618, 0x180u);   /* (d) acquires a palette entry */
    rec = actor_alloc(0);
    CHECK(rec != 0u, "entry 8 fixture record");
    if (rec != 0u) {
        DSW(rec + 0x56u) = (u16)actor_index(rec);
        DSB(rec + 0x5Fu) = 1u;
        pset = actor_pset(rec);
        DSD(K8C_SLOT1) = rec;
        DSB(K8C_SLOT1 + 0x7Au) = 7u;
        DSB(DS_00105B34) = 0u;
        DSB(DS_00105B34 + 1u) = 2u;
        DSD(K8C_SCR + 0x200u) = 0u;
        DSD(K8C_SCR + 0x204u) = 0u;
        DSD(K8C_SCR + 0x208u) = DSD(DSD(DS_000A8A98));   /* char 0's row[0] */
        CHECK(DSD(K8C_SCR + 0x208u) != 0u, "a nonzero image palette handle");
        DSW(pset + 2u) = 0x1234u;
        DSD(pset + 0x18u) = 0u;
        DSD(DS_000EF6DC) = 0x00000003u;
        fighter_34648();
        CHECK_EQ_INT((int)DSW(pset + 2u), 0x1234);
        DSD(DS_000EF6DC) = 0x00000004u;        /* no flag: row[0] = 0 */
        fighter_34648();
        CHECK_EQ_INT((int)DSW(pset + 2u), 0x0800);
        CHECK_EQ_INT((int)DSD(pset + 0x18u), 0);
        DSW(pset + 2u) = 0x1234u;
        DSD(DS_000EF6DC) = 0x00000002u;        /* flag: row[2] = the handle */
        fighter_34648();
        CHECK_EQ_INT((int)DSW(pset + 2u), 0x0800);
        CHECK(DSD(pset + 0x18u) != 0u, "entry 8 applies the flagged handle");
    }
    tf_put(s_pal, DS_00107618, 0x180u);    /* release (d)'s reference */

    /* (e) Entry 9, 0x3800C: the word +0x38 counts down while above 1
     * (signed); at or below 1 it becomes 0 and AE9 bit 0x02 is cleared. */
    DSD(0x001078D8u) = K8C_SCR + 0x300u;
    DSW(K8C_SCR + 0x336u) = 0xBEEFu;
    DSD(DS_00104AE8) = 0xFFFFFFFFu;
    DSW(K8C_SCR + 0x338u) = 5u;
    fighter_3800c();
    CHECK_EQ_INT((int)DSW(K8C_SCR + 0x338u), 4);
    DSW(K8C_SCR + 0x338u) = 2u;
    fighter_3800c();
    CHECK_EQ_INT((int)DSW(K8C_SCR + 0x338u), 1);
    CHECK_EQ_INT((int)DSD(DS_00104AE8), (int)0xFFFFFFFFu);
    fighter_3800c();
    CHECK_EQ_INT((int)DSW(K8C_SCR + 0x338u), 0);
    CHECK_EQ_INT((int)DSD(DS_00104AE8), (int)0xFFFFFDFFu);
    DSW(K8C_SCR + 0x338u) = 0x8000u;
    DSD(DS_00104AE8) = 0x00000200u;
    fighter_3800c();
    CHECK_EQ_INT((int)DSW(K8C_SCR + 0x338u), 0);
    CHECK_EQ_INT((int)DSD(DS_00104AE8), 0);
    CHECK_EQ_INT((int)DSW(K8C_SCR + 0x336u), 0xBEEF);

    /* (f) Entry 11, 0x4F890. The countdown word; at <= 0 a layer-0xD8 spawn
     * (0xC98C8 at x = 0x9ACC4's high word 0x3A00 for an odd count, 0xC98DC at
     * x = 0 for an even one; y = 0x9ACC6's high word 0), the count drops and
     * the reload word (not below 4) reloads it; a zero count clears AE9 0x08. */
    actors_reset();
    CHECK_EQ_INT((int)((s32)DSD(0x0009ACC4u) >> 16), 0x3A00);
    CHECK_EQ_INT((int)((s32)DSD(0x0009ACC6u) >> 16), 0);
    DSD(DS_00104AE8) = 0xFFFFFFFFu;
    DSW(DS_001088E8) = 2u; DSB(DS_001088F0) = 3u; DSW(0x001088EAu) = 0x10u;
    fighter_4f890();
    CHECK_EQ_INT((int)DSW(DS_001088E8), 1);
    CHECK_EQ_INT((int)DSB(DS_001088F0), 3);
    CHECK_EQ_INT((int)actor_list_head(), 0);
    fighter_4f890();
    head = actor_list_head();
    CHECK(head != 0u, "entry 11 spawns at a zero countdown");
    if (head != 0u) {
        CHECK_EQ_INT((int)DSD(head + 0x18u), 0x3A00);
        CHECK_EQ_INT((int)DSD(head + 0x1Cu), 0);
        CHECK_EQ_INT((int)DSB(head + 0x49u), 0xD8);
        CHECK(DSD(head + 0x08u) >= DSD(0x000C98C8u)
              && DSD(head + 0x08u) < DSD(0x000C98DCu),
              "odd count: the 0xC98C8 stream");
    }
    CHECK_EQ_INT((int)DSB(DS_001088F0), 2);
    CHECK_EQ_INT((int)DSW(0x001088EAu), 0x0F);
    CHECK_EQ_INT((int)DSW(DS_001088E8), 0x0F);
    CHECK_EQ_INT((int)DSD(DS_00104AE8), (int)0xFFFFFFFFu);
    DSW(DS_001088E8) = 0u;                     /* -1: signed <= 0 */
    fighter_4f890();
    rec = actor_list_head();
    CHECK(rec != 0u && rec != head, "entry 11 spawns at a negative countdown");
    if (rec != 0u) {
        CHECK_EQ_INT((int)DSD(rec + 0x18u), 0);
        CHECK(DSD(rec + 0x08u) >= DSD(0x000C98DCu), "even count: the 0xC98DC stream");
    }
    CHECK_EQ_INT((int)DSB(DS_001088F0), 1);
    CHECK_EQ_INT((int)DSW(DS_001088E8), 0x0E);
    DSW(DS_001088E8) = 1u; DSB(DS_001088F0) = 3u; DSW(0x001088EAu) = 4u;
    fighter_4f890();
    CHECK_EQ_INT((int)DSW(0x001088EAu), 4);
    CHECK_EQ_INT((int)DSW(DS_001088E8), 4);
    DSW(DS_001088E8) = 1u; DSB(DS_001088F0) = 1u; DSW(0x001088EAu) = 0x77u;
    fighter_4f890();
    CHECK_EQ_INT((int)DSB(DS_001088F0), 0);
    CHECK_EQ_INT((int)DSD(DS_00104AE8), (int)0xFFFFF7FFu);
    CHECK_EQ_INT((int)DSW(0x001088EAu), 0x77);
    CHECK_EQ_INT((int)DSW(DS_001088E8), 0);

    /* (g) Entry 12, 0x24150. A nonzero dword +0x1C keeps the step byte;
     * else it advances: below 4 +0x36 = 0x100 >> step, +0x44 = 0xA and +0x34
     * quartered (signed); at 4 +0x34 = 0 and AE9 bit 0x10 is cleared. */
    DSD(0x00104744u) = K8C_SCR + 0x400u;
    DSD(DS_00104AE8) = 0xFFFFFFFFu;
    DSD(K8C_SCR + 0x41Cu) = 0x00010000u;
    DSB(0x00104770u) = 2u;
    DSW(K8C_SCR + 0x434u) = 0x1234u;
    fighter_24150();
    CHECK_EQ_INT((int)DSB(0x00104770u), 2);
    CHECK_EQ_INT((int)DSW(K8C_SCR + 0x434u), 0x1234);
    DSD(K8C_SCR + 0x41Cu) = 0u;
    DSB(0x00104770u) = 0u;
    DSW(K8C_SCR + 0x434u) = 0xFF80u;
    DSW(K8C_SCR + 0x436u) = 0xBEEFu;
    DSW(K8C_SCR + 0x444u) = 0x7777u;
    fighter_24150();
    CHECK_EQ_INT((int)DSB(0x00104770u), 1);
    CHECK_EQ_INT((int)DSW(K8C_SCR + 0x436u), 0x80);
    CHECK_EQ_INT((int)DSW(K8C_SCR + 0x444u), 0x0A);
    CHECK_EQ_INT((int)DSW(K8C_SCR + 0x434u), 0xFFE0);
    CHECK_EQ_INT((int)DSD(DS_00104AE8), (int)0xFFFFFFFFu);
    DSB(0x00104770u) = 3u;
    DSW(K8C_SCR + 0x436u) = 0xBEEFu;
    fighter_24150();
    CHECK_EQ_INT((int)DSB(0x00104770u), 4);
    CHECK_EQ_INT((int)DSW(K8C_SCR + 0x434u), 0);
    CHECK_EQ_INT((int)DSW(K8C_SCR + 0x436u), 0xBEEF);
    CHECK_EQ_INT((int)DSD(DS_00104AE8), (int)0xFFFFEFFFu);
    DSB(0x00104770u) = 0xFFu;
    DSW(K8C_SCR + 0x434u) = 0x0010u;
    fighter_24150();
    CHECK_EQ_INT((int)DSB(0x00104770u), 0);
    CHECK_EQ_INT((int)DSW(K8C_SCR + 0x436u), 0x100);
    CHECK_EQ_INT((int)DSW(K8C_SCR + 0x434u), 0x0004);

    /* (h) Entry 17, 0x45D98, on entry 5 of the twelve (+0x28); the others
     * are done (4). Slot DS_00104AD4 = 0 (record side byte 1, char 3), slot
     * DS_001078FD = 1. The rng draws are replayed from the same seed. */
    actors_reset();
    for (i = 0; i < 0x60u; i += 8u) DSB(0x00108184u + i) = 4u;
    DSD(0x00108180u + 0x28u) = 0x5A5A5A5Au;
    DSB(0x001081EEu) = 0u;
    DSD(DS_00104AD4) = 0u;
    DSB(DS_001078FD) = 1u;
    DSD(K8C_SLOT0) = K8C_SCR + 0x500u;
    DSB(K8C_SCR + 0x551u) = 1u;
    DSB(K8C_SLOT0 + 0x7Au) = 3u;
    DSD(K8C_SLOT0 + 0x2Cu) = 0x00050000u;
    DSD(K8C_SLOT0 + 0x30u) = 0x00002000u;
    DSD(K8C_SLOT1) = K8C_SCR + 0x600u;
    DSD(K8C_SLOT1 + 0x2Cu) = 0x00060000u;
    DSD(K8C_SCR + 0x630u) = 0x00300000u;
    DSB(DS_00105B34) = 0u;
    DSB(DS_00105B34 + 1u) = 1u;
    CHECK(DSD(DSD(DS_000A8A98 + 12u)) != DSD(DSD(DS_000A8A98 + 12u) + 4u),
          "char 3's two palette handles differ");
    DSD(DS_00104AE8) = 0xFFFFFFFFu;
    /* A state above 4 is skipped (0x45DA7 ja). */
    DSB(0x00108184u + 0x28u) = 5u;
    fighter_45d98();
    CHECK_EQ_INT((int)DSB(0x00108184u + 0x28u), 5);
    CHECK_EQ_INT((int)DSD(0x00108180u + 0x28u), 0x5A5A5A5A);
    /* State 0: rng(0xA) != 0 skips; the draw is still taken. */
    DSB(0x00108184u + 0x28u) = 0u;
    for (seed = 1u; seed < 1000u; seed++) { rng_seed(seed); if (rng_next(0x0Au) != 0u) break; }
    rng_seed(seed); (void)rng_next(0x0Au);
    {
        u32 after = DSD(DS_000EF6D8);
        rng_seed(seed);
        fighter_45d98();
        CHECK_EQ_INT((int)DSB(0x00108184u + 0x28u), 0);
        CHECK_EQ_INT((int)DSD(DS_000EF6D8), (int)after);
        CHECK_EQ_INT((int)actor_list_head(), 0);
    }
    /* State 0: rng(0xA) == 0 spawns. The seed's first draw is also nonzero
     * for range 0xB, so the range itself is pinned. */
    for (seed = 1u; seed < 100000u; seed++) {
        rng_seed(seed);
        if (rng_next(0x0Au) != 0u) continue;
        rng_seed(seed);
        if (rng_next(0x0Bu) != 0u) break;
    }
    CHECK(seed < 100000u, "a seed whose first rng(0xA) is 0 and rng(0xB) is not");
    {
        u32 r1, r2;
        rng_seed(seed); (void)rng_next(0x0Au);
        r1 = rng_next(0x800u); r2 = rng_next(0x100u);
        rng_seed(seed);
        DSD(0x000C9384u) = 0xDEADBEEFu;
        fighter_45d98();
        rec = DSD(0x00108180u + 0x28u);
        CHECK(rec != 0u && rec == actor_list_head(), "state 0 stores the spawned record");
        if (rec != actor_list_head()) rec = 0u;   /* no scratch deref below */
        CHECK_EQ_INT((int)DSD(0x000C9384u), (int)DSD(DSD(DS_000A8A98 + 12u) + 4u));
        CHECK_EQ_INT((int)DSB(0x00108184u + 0x28u), 1);
        if (rec != 0u) {
            CHECK_EQ_INT((int)DSD(rec + 0x18u), (int)(0x00050000u - 0x400u + r1));
            CHECK_EQ_INT((int)DSD(rec + 0x1Cu), (int)(0x00002000u + r2));
            CHECK_EQ_INT((int)DSW(rec + 0x32u), 0);
            CHECK_EQ_INT((int)DSW(rec + 0x36u), 0x200);
        }
    }
    if (rec != 0u) {
        u32 r3, r4;
        /* State 1: waits for the signed +0x1C >= 0x3A00. */
        DSD(rec + 0x1Cu) = 0x80000000u;
        fighter_45d98();
        CHECK_EQ_INT((int)DSB(0x00108184u + 0x28u), 1);
        DSD(rec + 0x1Cu) = 0x39FFu;
        fighter_45d98();
        CHECK_EQ_INT((int)DSB(0x00108184u + 0x28u), 1);
        CHECK_EQ_INT((int)DSW(rec + 0x36u), 0x200);
        DSD(rec + 0x1Cu) = 0x3A00u;
        fighter_45d98();
        CHECK_EQ_INT((int)DSB(0x00108184u + 0x28u), 2);
        CHECK_EQ_INT((int)DSB(0x00108185u + 0x28u), 0x1E);
        CHECK_EQ_INT((int)DSW(rec + 0x36u), 0);
        /* State 2: the timer counts; at <= 0 the 0xEB8BE start at 1.0 near
         * slot 1 (x - 0x800 + rng(0x1000), y - 0x100 + rng(0x200)). */
        DSB(0x00108185u + 0x28u) = 2u;
        fighter_45d98();
        CHECK_EQ_INT((int)DSB(0x00108185u + 0x28u), 1);
        CHECK_EQ_INT((int)DSB(0x00108184u + 0x28u), 2);
        rng_seed(0x1234u);
        r3 = rng_next(0x1000u); r4 = rng_next(0x200u);
        rng_seed(0x1234u);
        fighter_45d98();
        CHECK_EQ_INT((int)DSB(0x00108184u + 0x28u), 3);
        CHECK_EQ_INT((int)DSD(rec + 0x24u), 0x3F800000);
        CHECK_EQ_INT((int)DSD(rec + 0x18u), (int)(0x00060000u - 0x800u + r3));
        CHECK_EQ_INT((int)DSW(rec + 0x32u), (int)(u16)(0x30u - 0x100u + r4));
        CHECK_EQ_INT((int)DSW(rec + 0x36u), 0xFC00);
        /* State 3: lands when +0x1C + (s16)+0x36 <= 0. */
        DSW(rec + 0x36u) = 0xFFF0u;
        DSD(rec + 0x1Cu) = 0x11u;
        DSB(0x001081EEu) = 0x0Bu;
        fighter_45d98();
        CHECK_EQ_INT((int)DSB(0x00108184u + 0x28u), 3);
        CHECK_EQ_INT((int)DSB(0x001081EEu), 0x0B);
        CHECK_EQ_INT((int)DSD(DS_00104AE8), (int)0xFFFFFFFFu);
        /* The twelfth landing: 3.0 on slot 0's record, 0x37B54 sets +0x53 on
         * the other side's slot DS_001077A8[1 ^ 1], AEA bit 0x02 cleared. */
        DSD(0x001077A8u) = K8C_SCR + 0x700u;
        DSD(K8C_SCR + 0x700u) = K8C_SCR + 0x800u;
        DSB(K8C_SCR + 0x853u) = 0x77u;
        DSD(K8C_SCR + 0x524u) = 0x12345678u;
        DSD(rec + 0x1Cu) = 0x10u;
        fighter_45d98();
        CHECK_EQ_INT((int)DSB(0x00108184u + 0x28u), 4);
        CHECK_EQ_INT((int)DSD(rec + 0x24u), 0x40400000);
        CHECK_EQ_INT((int)DSD(rec + 0x1Cu), 0);
        CHECK_EQ_INT((int)DSW(rec + 0x36u), 0);
        CHECK_EQ_INT((int)DSB(0x001081EEu), 0x0C);
        CHECK_EQ_INT((int)DSD(K8C_SCR + 0x524u), 0x40400000);
        CHECK_EQ_INT((int)DSB(K8C_SCR + 0x853u), 1);
        CHECK_EQ_INT((int)DSD(DS_00104AE8), (int)0xFFFDFFFFu);
    }

    tf_put(s_slots, 0x001077A0u, 0x160u);
    tf_put(s_88e8, 0x001088E8u, 0x10u);
    tf_put(s_8180, 0x00108180u, 0x70u);
    tf_put(s_5b34, DS_00105B34, 2u);
    DSD(DS_00104AE8) = s_ae8; DSD(DS_000EF6DC) = s_dc; DSD(DS_000EF6D8) = s_rng;
    DSD(0x00104744u) = s_4744; DSD(DS_00104AD4) = s_ad4;
    DSD(0x000C9384u) = s_9384; DSD(DS_000A8A98 + 7u * 4u) = s_row7;
    DSB(0x00104770u) = s_4770;
    actors_reset();
}
#undef K8C_SCR
#undef K8C_SLOT0
#undef K8C_SLOT1

/* Update-table entries 15 (0x260BC) and 16 (0x26194), the two bonus cards
 * (record §B8, 2026-09-29-e-wire-k8b-k8d-derivations.md). Each card record
 * is a pool record; the expected stream cursor is taken from a reference
 * record begun on the same stream through actors_anim_begin (0x2BC30), since
 * the streams open with command words the begin walks. The camera targets
 * DS_001077A8[0/1] are scratch records so 0x2604C's 0x41310(side) call shows
 * which side it got. Runs after actors_init, which registers both entries. */
#define BC_B0E  0x00104B0Eu   /* no symbols.h name: entry 15's timer */
#define BC_B10  0x00104B10u   /* no symbols.h name: entry 16's timer */
#define BC_B1C  0x00104B1Cu   /* no symbols.h name: 0x27C48's bonus byte */
#define BC_SCR  0x3E94000u
static u32 bc_card(void)
{
    u32 r = actor_alloc(0);
    if (r != 0u) {
        DSW(r + 0x56u) = (u16)actor_index(r);
        DSW(r + 0x28u) = 0; DSW(r + 0x2Au) = 0;
        DSD(r + 0x08u) = 0x5A5A5A5Au; DSD(r + 0x24u) = 0;
        DSD(actor_pset(r) + 0x18u) = 0;
    }
    return r;
}
static void check_bonus_cards(void)
{
    u8 s_b0c[0x14], s_a8[4];
    u32 s_ab0 = DSD(DS_00104AB0), s_ab4 = DSD(DS_00104AB4);
    u32 s_mode = DSD(DS_00104B00), s_7a8[2];
    u32 a, b, ref, want;
    s_7a8[0] = DSD(DS_001077A8); s_7a8[1] = DSD(DS_001077A8 + 4u);
    tf_snap(s_b0c, DS_00104B0C, 0x14u);
    tf_snap(s_a8, DS_00104AE8, 4u);

    /* (a) The table dwords (0xA8680/0xA8684) and their resolution. */
    CHECK_EQ_INT((int)DSD(DS_000A8644 + 15u * 4u), 0x260BC);
    CHECK_EQ_INT((int)DSD(DS_000A8644 + 16u * 4u), 0x26194);
    CHECK(fn_resolve(0x260BCu) == flow_bonus_card_a_step,
          "update-table entry 15 (0x260BC) resolves to its port");
    CHECK(fn_resolve(0x26194u) == flow_bonus_card_b_step,
          "update-table entry 16 (0x26194) resolves to its port");

    actors_reset();
    a = bc_card(); b = bc_card(); ref = bc_card();
    CHECK(a != 0u && b != 0u && ref != 0u, "bonus-card fixture records");
    if (a == 0u || b == 0u || ref == 0u) goto out;
    DSD(DS_00104AB4) = a;
    DSD(DS_001077A8) = BC_SCR;       DSD(BC_SCR + 0x3Cu) = 0x2000u;
    DSD(DS_001077A8 + 4u) = BC_SCR + 0x100u; DSD(BC_SCR + 0x13Cu) = 0x1000u;
    DSD(DS_00104B00) = 0x30u;        /* 0x41310 skips mode 3 only */

    /* (b) Entry 15, state 0: +0x2C gains 0x80 below 0x1000; at or above it
     * the word is clamped, the timer = 0x3C and the state = 1. The compare
     * is on the zero-extended word, so 0xFFC0 wraps to 0x0040 unclamped. */
    DSD(DS_00104AE8) = 0xFFFFFFFFu;
    DSB(DS_00104B0D) = 0u; DSB(BC_B0E) = 0x77u;
    DSW(a + 0x2Cu) = 0x0010u;
    flow_bonus_card_a_step();
    CHECK_EQ_INT((int)DSW(a + 0x2Cu), 0x0090);
    CHECK_EQ_INT((int)DSB(DS_00104B0D), 0);
    CHECK_EQ_INT((int)DSB(BC_B0E), 0x77);
    DSW(a + 0x2Cu) = 0xFFC0u;
    flow_bonus_card_a_step();
    CHECK_EQ_INT((int)DSW(a + 0x2Cu), 0x0040);
    CHECK_EQ_INT((int)DSB(DS_00104B0D), 0);
    DSW(a + 0x2Cu) = 0x7FC0u;              /* 0x8040: clamps (a signed compare would not) */
    flow_bonus_card_a_step();
    CHECK_EQ_INT((int)DSW(a + 0x2Cu), 0x1000);
    CHECK_EQ_INT((int)DSB(DS_00104B0D), 1);
    DSB(DS_00104B0D) = 0u; DSB(BC_B0E) = 0x77u;
    DSW(a + 0x2Cu) = 0x0F81u;
    flow_bonus_card_a_step();
    CHECK_EQ_INT((int)DSW(a + 0x2Cu), 0x1000);
    CHECK_EQ_INT((int)DSB(BC_B0E), 0x3C);
    CHECK_EQ_INT((int)DSB(DS_00104B0D), 1);
    CHECK_EQ_INT((int)DSD(DS_00104AE8), (int)0xFFFFFFFFu);

    /* (c) State 1: the timer counts while positive (signed); at or below 0
     * the state = 2 and the card begins 0xE91A4 at 1.0. A negative
     * DS_00104B1C spawns no second card; a side (1) runs 0x2604C(1). */
    DSB(BC_B0E) = 2u;
    flow_bonus_card_a_step();
    CHECK_EQ_INT((int)DSB(BC_B0E), 1);
    CHECK_EQ_INT((int)DSB(DS_00104B0D), 1);
    CHECK_EQ_INT((int)DSD(a + 0x08u), 0x5A5A5A5A);
    actors_anim_begin(ref, 0x000E91A4u, 0x3F800000u);
    want = DSD(ref + 0x08u);
    DSB(BC_B1C) = 0xFFu;
    DSD(DS_00104AB0) = 0x12345678u;
    flow_bonus_card_a_step();
    CHECK_EQ_INT((int)DSB(BC_B0E), 0);
    CHECK_EQ_INT((int)DSB(DS_00104B0D), 2);
    CHECK_EQ_INT((int)DSD(a + 0x08u), (int)want);
    CHECK_EQ_INT((int)DSD(a + 0x24u), 0x3F800000);
    CHECK_EQ_INT((int)DSD(DS_00104AB0), 0x12345678);
    DSB(DS_00104B0D) = 1u; DSB(BC_B0E) = 0x81u;    /* (s8) -127: fires */
    DSB(BC_B1C) = 1u;
    DSB(DS_00104B0F) = 0x77u;
    DSB(DS_00104AEA) = 0u;
    DSD(a + 0x08u) = 0x5A5A5A5Au;
    flow_bonus_card_a_step();
    CHECK_EQ_INT((int)DSB(DS_00104B0D), 2);
    CHECK_EQ_INT((int)DSD(a + 0x08u), (int)want);
    CHECK_EQ_INT((int)DSB(DS_00104B0F), 0);
    CHECK_EQ_INT((int)DSB(DS_00104AEA), 1);
    CHECK(DSD(DS_00104AB0) != 0x12345678u && DSD(DS_00104AB0) == actor_list_head(),
          "0x2604C spawned the second card");
    CHECK_EQ_INT((int)DSD(BC_SCR + 0x13Cu), 0x1000 + 0x4E20);
    CHECK_EQ_INT((int)DSD(BC_SCR + 0x3Cu), 0x2000);

    /* (d) State 2: the word drops by 0x200; at zero the card dies and AE9
     * bit 0x80 (mask bit 15) is cleared. 0x0100 wraps to 0xFF00 (non-zero). */
    DSD(DS_00104AE8) = 0xFFFFFFFFu;
    DSB(DS_00104B0D) = 2u;
    DSW(a + 0x2Cu) = 0x0400u;
    flow_bonus_card_a_step();
    CHECK_EQ_INT((int)DSW(a + 0x2Cu), 0x0200);
    CHECK_EQ_INT((int)(DSB(a + 0x28u) & 8u), 0);
    CHECK_EQ_INT((int)DSD(DS_00104AE8), (int)0xFFFFFFFFu);
    DSW(a + 0x2Cu) = 0x0100u;
    flow_bonus_card_a_step();
    CHECK_EQ_INT((int)DSW(a + 0x2Cu), 0xFF00);
    CHECK_EQ_INT((int)(DSB(a + 0x28u) & 8u), 0);
    DSW(a + 0x2Cu) = 0x0200u;
    flow_bonus_card_a_step();
    CHECK_EQ_INT((int)DSW(a + 0x2Cu), 0);
    CHECK_EQ_INT((int)(DSB(a + 0x28u) & 8u), 8);
    CHECK_EQ_INT((int)DSD(DS_00104AE8), (int)0xFFFF7FFFu);
    CHECK_EQ_INT((int)DSB(DS_00104B0D), 2);

    /* (e) Any state above 2 does nothing. */
    DSB(DS_00104B0D) = 3u; DSB(BC_B0E) = 0x55u; DSW(a + 0x2Cu) = 0x0200u;
    flow_bonus_card_a_step();
    CHECK_EQ_INT((int)DSW(a + 0x2Cu), 0x0200);
    CHECK_EQ_INT((int)DSB(BC_B0E), 0x55);
    CHECK_EQ_INT((int)DSB(DS_00104B0D), 3);

    /* (f) Entry 16, the twin on DS_00104B0F/B10, DS_00104AB0, the stream
     * 0xE91D4 and AEA bit 0x01 (mask bit 16); state 1 has no chain. */
    DSD(DS_00104AB0) = b;
    DSD(DS_00104AE8) = 0xFFFFFFFFu;
    DSB(DS_00104B0F) = 0u; DSB(BC_B10) = 0x77u;
    DSW(b + 0x2Cu) = 0x0010u;
    flow_bonus_card_b_step();
    CHECK_EQ_INT((int)DSW(b + 0x2Cu), 0x0090);
    CHECK_EQ_INT((int)DSB(BC_B10), 0x77);
    DSW(b + 0x2Cu) = 0xFFC0u;
    flow_bonus_card_b_step();
    CHECK_EQ_INT((int)DSW(b + 0x2Cu), 0x0040);
    CHECK_EQ_INT((int)DSB(DS_00104B0F), 0);
    DSW(b + 0x2Cu) = 0x7FC0u;
    flow_bonus_card_b_step();
    CHECK_EQ_INT((int)DSW(b + 0x2Cu), 0x1000);
    CHECK_EQ_INT((int)DSB(DS_00104B0F), 1);
    DSB(DS_00104B0F) = 0u; DSB(BC_B10) = 0x77u;
    DSW(b + 0x2Cu) = 0x0F81u;
    flow_bonus_card_b_step();
    CHECK_EQ_INT((int)DSW(b + 0x2Cu), 0x1000);
    CHECK_EQ_INT((int)DSB(BC_B10), 0x3C);
    CHECK_EQ_INT((int)DSB(DS_00104B0F), 1);
    DSB(BC_B10) = 2u;
    flow_bonus_card_b_step();
    CHECK_EQ_INT((int)DSB(BC_B10), 1);
    CHECK_EQ_INT((int)DSB(DS_00104B0F), 1);
    CHECK_EQ_INT((int)DSD(b + 0x08u), 0x5A5A5A5A);
    actors_anim_begin(ref, 0x000E91D4u, 0x3F800000u);
    want = DSD(ref + 0x08u);
    DSB(BC_B1C) = 1u;
    DSD(DS_00104AB4) = 0x12345678u;
    DSB(DS_00104B0D) = 0x77u;
    DSB(BC_B10) = 0x81u;
    flow_bonus_card_b_step();
    CHECK_EQ_INT((int)DSB(DS_00104B0F), 2);
    CHECK_EQ_INT((int)DSD(b + 0x08u), (int)want);
    CHECK_EQ_INT((int)DSD(b + 0x24u), 0x3F800000);
    CHECK_EQ_INT((int)DSD(DS_00104AB4), 0x12345678);
    CHECK_EQ_INT((int)DSB(DS_00104B0D), 0x77);
    DSW(b + 0x2Cu) = 0x0100u;
    flow_bonus_card_b_step();
    CHECK_EQ_INT((int)DSW(b + 0x2Cu), 0xFF00);
    CHECK_EQ_INT((int)(DSB(b + 0x28u) & 8u), 0);
    DSW(b + 0x2Cu) = 0x0200u;
    flow_bonus_card_b_step();
    CHECK_EQ_INT((int)DSW(b + 0x2Cu), 0);
    CHECK_EQ_INT((int)(DSB(b + 0x28u) & 8u), 8);
    CHECK_EQ_INT((int)DSD(DS_00104AE8), (int)0xFFFEFFFFu);
    DSB(DS_00104B0F) = 3u; DSW(b + 0x2Cu) = 0x0200u;
    flow_bonus_card_b_step();
    CHECK_EQ_INT((int)DSW(b + 0x2Cu), 0x0200);

out:
    tf_put(s_b0c, DS_00104B0C, 0x14u);
    tf_put(s_a8, DS_00104AE8, 4u);
    DSD(DS_00104AB0) = s_ab0; DSD(DS_00104AB4) = s_ab4;
    DSD(DS_00104B00) = s_mode;
    DSD(DS_001077A8) = s_7a8[0]; DSD(DS_001077A8 + 4u) = s_7a8[1];
    actors_reset();
}
#undef BC_B0E
#undef BC_B10
#undef BC_B1C
#undef BC_SCR

/* The animation-opcode setters 0x37EA0, 0x24078 and 0x45D58 (record §D8) and
 * 0x4F944's signed clamp (record §C), 2026-09-29-e-wire-k8b-k8d-derivations.md.
 * The setters are driven directly on pool records (no no-input path reaches
 * their 0xD100 stream words, record §R); the palette table and the fighter
 * slots are saved and restored around them. */
#define AS_SCR    0x3E96000u
#define AS_1078D8 0x001078D8u   /* no symbols.h name: 0x37EA0's spawned record */
#define AS_104744 0x00104744u   /* no symbols.h name: 0x24078's spawned record */
#define AS_104770 0x00104770u   /* no symbols.h name: entry 12's step byte */
#define AS_1081EE 0x001081EEu   /* no symbols.h name: entry 17's done count */
#define AS_1088EA 0x001088EAu   /* no symbols.h name: 0x4F944's reload word */
static u32 as_rec(u8 side, u16 w0)
{
    u32 r = actor_alloc(0);
    if (r != 0u) {
        DSW(r + 0x56u) = (u16)actor_index(r);
        DSW(r + 0x28u) = 0; DSW(r + 0x2Au) = 0;
        DSB(r + 0x51u) = side;
        DSW(actor_pset(r)) = w0;
        DSD(actor_pset(r) + 0x18u) = 0;
    }
    return r;
}
static void check_anim_setters(void)
{
    static const u16 spr[8] = { 0x46B9u, 0x46BAu, 0x46B8u, 0x46B6u,
                                0x46B7u, 0x46B9u, 0x46BAu, 0x46B9u };
    u8 s_pal[0x180], s_slots[0x160], s_8180[0x70], s_88e8[4];
    u32 s_ae8 = DSD(DS_00104AE8), s_rng = DSD(DS_000EF6D8);
    u32 s_78d8 = DSD(AS_1078D8), s_4744 = DSD(AS_104744);
    u32 s_aec = DSD(DS_000F0AEC), s_af0 = DSD(DS_000F0AF0);
    u8 s_b0c = DSB(DS_00104B0C), s_4770 = DSB(AS_104770), s_88f0 = DSB(DS_001088F0);
    u32 i, rec, nw, np, ref, r, orec;
    tf_snap(s_pal, DS_00107618, 0x180u);
    tf_snap(s_slots, 0x001077A0u, 0x160u);
    tf_snap(s_8180, 0x00108180u, 0x70u);
    tf_snap(s_88e8, DS_001088E8, 4u);

    /* (a) Each target resolves (anim_indirect's fn_resolve). */
    CHECK(fn_resolve(0x37EA0u) != NULL, "0x37EA0 is a registered anim target");
    CHECK(fn_resolve(0x24078u) != NULL, "0x24078 is a registered anim target");
    CHECK(fn_resolve(0x45D58u) != NULL, "0x45D58 is a registered anim target");

    /* (b) 0x45D58: the twelve state bytes 0x108184 + 8i and DS_001081EE go
     * to 0, the bytes between them are kept, AEA |= 2 (mask bit 17). */
    memset(mem + 0x00108180u, 0x55, 0x70);
    for (i = 0; i < 0x60u; i += 8u) DSB(0x00108184u + i) = 0x77u;
    DSB(AS_1081EE) = 0x33u;
    DSD(DS_00104AE8) = 0u;
    fighter_45d58();
    for (i = 0; i < 0x60u; i += 8u) {
        CHECK_EQ_INT((int)DSB(0x00108184u + i), 0);
        CHECK_EQ_INT((int)DSB(0x00108185u + i), 0x55);
        CHECK_EQ_INT((int)DSB(0x00108180u + i), 0x55);
    }
    CHECK_EQ_INT((int)DSB(0x00108184u + 0x60u), 0x55);
    CHECK_EQ_INT((int)DSB(AS_1081EE), 0);
    CHECK_EQ_INT((int)DSD(DS_00104AE8), 0x00020000);

    /* (c) 0x24078 with the other side's DS_001077A8 entry empty: only the
     * caller's +0x59 = 0xFD. */
    actors_reset();
    rec = as_rec(0u, 0x0000u);
    CHECK(rec != 0u, "0x24078 fixture record");
    if (rec == 0u) goto out;
    DSD(DS_001077B0) = rec;
    DSD(DS_001077A8 + 4u) = 0u;
    DSB(rec + 0x59u) = 0x11u;
    DSD(AS_104744) = 0xDEADBEEFu;
    DSB(AS_104770) = 0x77u;
    DSD(DS_00104AE8) = 0u;
    fighter_24078(rec);
    CHECK_EQ_INT((int)DSB(rec + 0x59u), 0xFD);
    CHECK_EQ_INT((int)DSD(AS_104744), (int)0xDEADBEEFu);
    CHECK_EQ_INT((int)DSB(AS_104770), 0x77);
    CHECK_EQ_INT((int)DSD(DS_00104AE8), 0);

    /* (d) With it set (char 3, x 0x12340, its record's y 0x00450000): the
     * other record begins 0xA8424[3] at 3.0; 0xA84FC spawns at (0x12340,
     * a3 0x45 -> +0x32, the descriptor has no 0x2000 bit, a4 0x1000) into
     * DS_00104744; +0x34 = 0xFF80 while the caller's side has bit 15 clear,
     * 0x0080 otherwise; +0x36 = 0, +0x44 = 0xA, +0x59 = 0xFE, the step byte 0,
     * AE9 |= 0x10 (mask bit 12). */
    orec = as_rec(1u, 0x0000u);
    ref = as_rec(1u, 0x0000u);
    CHECK(orec != 0u && ref != 0u, "0x24078 other-side records");
    if (orec == 0u || ref == 0u) goto out;
    CHECK_EQ_INT((int)DSD(0x000A8424u + 3u * 4u), 0x000D2C46);
    actors_anim_begin(ref, DSD(0x000A8424u + 3u * 4u), 0x40400000u);
    DSD(AS_SCR) = orec;
    DSB(AS_SCR + 0x7Au) = 3u;
    DSD(AS_SCR + 0x2Cu) = 0x00012340u;
    DSD(orec + 0x30u) = 0x00450000u;
    DSD(orec + 0x08u) = 0x5A5A5A5Au;
    DSD(DS_001077A8 + 4u) = AS_SCR;
    /* The predicate 0x1A570 reads the caller's side (0). Slot 1's record is
     * orec, whose begin leaves its pset word 0 with bit 15 clear, so reading
     * side 1 instead would give 0xFF80 in the second pass. */
    DSD(DS_001077B0 + 0x94u) = orec;
    for (i = 0; i < 2u; i++) {
        DSW(actor_pset(rec)) = i == 0u ? 0x0000u : 0x8000u;
        DSB(rec + 0x59u) = 0x11u;
        DSD(AS_104744) = 0xDEADBEEFu;
        DSB(AS_104770) = 0x77u;
        DSD(DS_00104AE8) = 0u;
        fighter_24078(rec);
        nw = DSD(AS_104744);
        CHECK(nw != 0xDEADBEEFu && nw == actor_list_head(), "0x24078 stores its spawn");
        if (nw != actor_list_head()) break;
        CHECK_EQ_INT((int)DSD(nw + 0x18u), 0x12340);
        CHECK_EQ_INT((int)DSW(nw + 0x32u), 0x45);
        CHECK_EQ_INT((int)DSD(nw + 0x1Cu), 0x1000);
        CHECK_EQ_INT((int)DSW(nw + 0x34u), i == 0u ? 0xFF80 : 0x0080);
        CHECK_EQ_INT((int)DSW(nw + 0x36u), 0);
        CHECK_EQ_INT((int)DSW(nw + 0x44u), 0x0A);
        CHECK_EQ_INT((int)DSB(nw + 0x59u), 0xFE);
        CHECK_EQ_INT((int)DSB(rec + 0x59u), 0xFD);
        CHECK_EQ_INT((int)DSB(AS_104770), 0);
        CHECK_EQ_INT((int)DSD(DS_00104AE8), 0x1000);
        CHECK_EQ_INT((int)DSD(orec + 0x08u), (int)DSD(ref + 0x08u));
        CHECK_EQ_INT((int)DSD(orec + 0x24u), 0x40400000);
        DSD(orec + 0x08u) = 0x5A5A5A5Au;
    }

    /* (e) 0x37EA0 on side 1 (char 3 -> sprite 0x46B6) with its pset's
     * word 0 bit 15 set (a5 = 0x4000, so the new pset word 0 = 0xC6B6): the
     * spawned record DS_001078D8 takes the pset +4/+8, the words +0x32/+0x28
     * (+0x29 | 0x28 | 0x10, then 0x2BE5C clears 0x20), the layer from pset
     * +0xE, the handle 0x0105FEBC; the caller dies; 0x2BE5C's bit-12 arm
     * sets +0x18 from pset +4; +0x38 = rng(0x10) + 0x20; DS_00104B0C = 1 and
     * AE9 |= 2 (mask bit 9). */
    actors_reset();
    DSB(DS_0010782A + 0x94u) = 3u;
    DSD(DS_000F0AF0) = 0x00700000u;
    DSD(DS_000F0AEC) = 0x00001000u;
    rec = as_rec(1u, 0x8123u);
    CHECK(rec != 0u, "0x37EA0 fixture record");
    if (rec == 0u) goto out;
    np = actor_pset(rec);
    DSD(np + 4u) = 0x00011111u; DSD(np + 8u) = 0x00022222u;
    DSW(np + 0x0Eu) = 0x00D0u;
    DSD(rec + 0x18u) = 0x00123456u; DSD(rec + 0x1Cu) = 0x00654321u;
    DSW(rec + 0x32u) = 0x1357u;
    DSW(rec + 0x28u) = 0x2102u;
    DSD(AS_1078D8) = 0xDEADBEEFu;
    DSB(DS_00104B0C) = 0x77u;
    DSD(DS_00104AE8) = 0u;
    rng_seed(0x4321u);
    r = rng_next(0x10u);
    rng_seed(0x4321u);
    fighter_37ea0(rec);
    nw = DSD(AS_1078D8);
    CHECK(nw != 0xDEADBEEFu && nw == actor_list_head(), "0x37EA0 stores its spawn");
    if (nw != actor_list_head()) goto out;
    np = actor_pset(nw);
    CHECK_EQ_INT((int)DSW(np), 0xC6B6);
    CHECK_EQ_INT((int)DSD(np + 4u), 0x11111);
    CHECK_EQ_INT((int)DSD(np + 8u), 0x22222);
    CHECK_EQ_INT((int)DSB(nw + 0x49u), 0xD0);
    CHECK(DSD(np + 0x18u) != 0u && DSD(DSD(np + 0x18u)) == 0x0105FEBCu,
          "0x37EA0's spawn holds the handle 0x0105FEBC");
    CHECK_EQ_INT((int)DSW(nw + 0x32u), 0x1357);
    CHECK_EQ_INT((int)DSB(nw + 0x28u), 0x02);
    CHECK_EQ_INT((int)DSB(nw + 0x29u), 0x19);
    CHECK_EQ_INT((int)DSD(nw + 0x18u),
                 (int)(0x11111u + (u32)((s32)DSD(nw + 0x44u) >> 16) * 2u - 0x2A00u));
    CHECK_EQ_INT((int)DSD(nw + 0x1Cu),
                 (int)(0x1000u + 0x3BC0u - 0x22222u - (u32)((s32)DSD(nw + 0x30u) >> 16)));
    CHECK_EQ_INT((int)DSW(nw + 0x38u), (int)(r + 0x20u));
    CHECK_EQ_INT((int)(DSB(rec + 0x28u) & 8u), 8);
    CHECK_EQ_INT((int)DSB(DS_00104B0C), 1);
    CHECK_EQ_INT((int)DSD(DS_00104AE8), 0x200);

    /* (f) The character's sprite through the seven-way table (0x37E84;
     * above 6 takes the `ja` arm, 0x46B9), a5 = 0: word 0 is the sprite. */
    for (i = 0; i < 8u; i++) {
        actors_reset();
        DSB(DS_0010782A) = (u8)i;
        rec = as_rec(0u, 0x0123u);
        if (rec == 0u) break;
        fighter_37ea0(rec);
        nw = DSD(AS_1078D8);
        CHECK_EQ_INT((int)DSW(actor_pset(nw)), (int)spr[i]);
    }

    /* (g) Record §C, 0x4F944: `cmp eax,0x14; jle` is signed. */
    {
        static const u32 v[4] = { 0xFFFFFFFFu, 0x80000000u, 0x15u, 0x14u };
        static const int want[4] = { 0xFF, 0x00, 0x14, 0x14 };
        for (i = 0; i < 4u; i++) {
            DSB(DS_001088F0) = 0x77u;
            DSW(DS_001088E8) = 0x7777u;
            DSW(AS_1088EA) = 0x7777u;
            DSD(DS_00104AE8) = 0u;
            fighter_4f944(v[i]);
            CHECK_EQ_INT((int)DSB(DS_001088F0), want[i]);
            CHECK_EQ_INT((int)DSW(DS_001088E8), 0);
            CHECK_EQ_INT((int)DSW(AS_1088EA), 0x10);
            CHECK_EQ_INT((int)DSD(DS_00104AE8), 0x800);
        }
    }

out:
    tf_put(s_pal, DS_00107618, 0x180u);
    tf_put(s_slots, 0x001077A0u, 0x160u);
    tf_put(s_8180, 0x00108180u, 0x70u);
    tf_put(s_88e8, DS_001088E8, 4u);
    DSD(DS_00104AE8) = s_ae8; DSD(DS_000EF6D8) = s_rng;
    DSD(AS_1078D8) = s_78d8; DSD(AS_104744) = s_4744;
    DSD(DS_000F0AEC) = s_aec; DSD(DS_000F0AF0) = s_af0;
    DSB(DS_00104B0C) = s_b0c; DSB(AS_104770) = s_4770; DSB(DS_001088F0) = s_88f0;
    actors_reset();
}
#undef AS_SCR
#undef AS_1078D8
#undef AS_104744
#undef AS_104770
#undef AS_1081EE
#undef AS_1088EA

int test_actors(void)
{
    int before = g_failures;

    /* The pool and pset bases come from res_load_index, so a run that reached
     * here has them; assert the shape the rest of the cycle depends on. Earlier
     * tests load the INDEX; a second load would exhaust the bump allocator's
     * 64 MB mem[], so only load if this test runs first. */
    if (res_count() != 69)
        CHECK(res_load_index("data/game/C", "data/game/C/INDEX") == 69,
              "index loaded");
    CHECK(DSD(DS_001014F4) != 0, "actor pool allocated by res_load_index");
    CHECK(DSD(DS_001014EC) != 0, "pset pool allocated by res_load_index");
    CHECK(actors_init() == 1, "actors_init validates the two pools");
    /* Record §42-E: the DS_00104AE4 handler 0x29B74 resolves to its port. */
    CHECK(fn_resolve(FN_00029B74) == frontend_darken_all,
          "actors_init registered 0x29B74 as frontend_darken_all");
    check_card_ramp();
    check_update_k8c();
    check_bonus_cards();
    check_anim_setters();

    /* A fresh reset frees every record and leaves both lists empty. It also
     * runs 0x4F228 with EAX = 0 (0x2BBC0/0x2BBC4): the projection gate
     * DS_00107A54, DS_00107A55 and the words DS_00107A3A/38 go to 0 (the
     * demo's state 6 leaves the gate at 1; record §36). */
    DSB(DS_00107A54) = 1u;
    DSB(DS_00107A55) = 0x99u;
    DSW(DS_00107A3A) = 0x1234u;
    DSW(DS_00107A38) = 0x4321u;
    actors_reset();
    CHECK_EQ_INT((int)DSB(DS_00107A54), 0);
    CHECK_EQ_INT((int)DSB(DS_00107A55), 0);
    CHECK_EQ_INT((int)DSW(DS_00107A3A), 0);
    CHECK_EQ_INT((int)DSW(DS_00107A38), 0);
    CHECK_EQ_INT((int)actor_list_head(), 0);
    CHECK(actor_alloc(0) != 0, "alloc after reset returns a record");

    /* Allocating every record then exhausting returns 0, never a duplicate. */
    memset(mem + DSD(DS_001014F4), 0, 0xEBA0);
    actors_reset();
    u32 n = 0, first = actor_alloc(0);
    CHECK(first != 0, "first alloc");
    for (n = 1; n < 580; n++) {
        u32 r = actor_alloc(0);
        CHECK(r != 0, "alloc within the pool");
        if (r == first) { CHECK(0, "alloc returned the same record twice"); break; }
    }
    CHECK_EQ_INT((int)actor_alloc(0), 0);   /* exhaustion */

    /* Free then realloc: the freed record is the one handed back (0x249D0
     * pops the free-list head that 0x249C0 pushed). */
    actors_reset();
    u32 a = actor_alloc(0), b = actor_alloc(0);
    CHECK(b != 0, "second alloc");
    actor_free(a);
    CHECK_EQ_INT((int)actor_alloc(0), (int)a);

    /* An out-of-pool or misaligned offset is ignored, not linked. */
    actors_reset();
    u32 c = actor_alloc(0);
    actor_free(0x1234u);                       /* outside the pool */
    actor_free(c + 1u);                        /* misaligned */
    CHECK_EQ_INT((int)actor_alloc(0), (int)c + 0x68u);

    /* reset() zeroes both process masks (0x2A31C's gates) and rebuilds the
     * lists: after it, allocation order restarts at the pool base. */
    DSD(DS_00104AE8) = 0xFFFFFFFFu;
    DSD(DS_00104AEC) = 0xFFFFFFFFu;
    actors_reset();
    CHECK_EQ_INT((int)DSD(DS_00104AE8), 0);
    CHECK_EQ_INT((int)DSD(DS_00104AEC), 0);
    CHECK_EQ_INT((int)actor_alloc(0), (int)DSD(DS_001014F4));

    /* 0x2BAF4's param_1 != 0 arm runs 0x52106, which clears the screen aperture
     * (0x5214C-0x52151 `mov eax,0xa0000; call 0x51f72`) beside the two offscreen
     * buffers. A sentinel that differs from the post-state cannot survive. */
    memset(gfx_aperture(), 0x5Au, 0xFA00u);
    actors_reset();
    CHECK_EQ_INT((int)gfx_aperture()[0], 0);
    CHECK_EQ_INT((int)gfx_aperture()[0xFA00u - 1u], 0);

    /* The active list is the circular free/active pair 0x2AC80 links into:
     * after two allocs the head is the most recent record, and actor_next
     * walks to the previous one. */
    actors_reset();
    u32 p = actor_alloc(0), q = actor_alloc(0);
    CHECK_EQ_INT((int)actor_list_head(), (int)q);
    CHECK_EQ_INT((int)actor_next(q), (int)p);
    CHECK_EQ_INT((int)actor_next(p), 0);
    CHECK_EQ_INT((int)actor_index(q), 1);
    CHECK_EQ_INT((int)actor_record(1), (int)q);
    CHECK_EQ_INT((int)actor_record(999), 0);
    CHECK_EQ_INT((int)actor_index(0x1234u), -1);
    DSW(q + 0x56) = 3;
    CHECK_EQ_INT((int)actor_pset(q), (int)(DSD(DS_001014EC) + 3u * 0x20u));

    check_pset_sync();
    check_actor_spawn();
    check_pset_layer();

    return g_failures - before;
}

/* ---- test_effects.c ---- */

/* Scratch for a source record: mem[] above the heap, the base other tests use. */
#define EFFECTS_TEST_SRC 0x3F00000u

/* Zero the shared source scratch (entry count 0 at +0xC). */
static void effects_reset_source(u32 size)
{
    mem_fill(EFFECTS_TEST_SRC, 0, size);
}

/* Install a one-entry resource table at `tab` whose block is `blk`. */
static void effects_reset_restab(u32 tab, u32 blk)
{
    DSD(DS_001014E0) = tab;
    DSD(DS_001014F0) = 1;
    DSD(tab + 16) = blk;
}

/* Put the resource table back the way effects_reset_restab found it. */
static void effects_restore_restab(u32 saved_tab, u32 saved_n)
{
    DSD(DS_001014E0) = saved_tab;
    DSD(DS_001014F0) = saved_n;
}

/* Seed the six-dword resolved block the scroll tests copy. */
static void effects_seed_block6(u32 blk)
{
    for (int i = 0; i < 6; i++) DSD(blk + 4 + (u32)i * 4u) = 0x40u + (u32)i;
}

int test_effects(void)
{
    int before = g_failures;

    /* A spawn before effects_init must not walk mem[]: with the free sentinel
     * zeroed, rec reads as 0 and list_unlink(0) would write mem[0]/mem[4] and
     * then link the phantom record onto the active list. Mirrors the unbuilt-pool
     * guard effects_clear has; the count stays 0 and neither list is touched. */
    mem_fill(DS_000FCCE0, 0, 12u);
    mem_fill(DS_0009AF3D, 0, 1u);
    {
        u32 m0 = DSD(0), m4 = DSD(4);
        CHECK_EQ_INT((int)effects_spawn(EFFECTS_TEST_SRC, 0u, 0u), 0);
        /* The three new producers share effect_take_free, so they return 0 on
         * the unbuilt pool without reading source_rec or touching mem[]. */
        CHECK_EQ_INT((int)effects_spawn_darken(EFFECTS_TEST_SRC, 0u), 0);
        CHECK_EQ_INT((int)effects_spawn_pulse(EFFECTS_TEST_SRC, 0u), 0);
        CHECK_EQ_INT((int)effects_spawn_scroll(EFFECTS_TEST_SRC, 0, 1u, 1u), 0);
        CHECK_EQ_INT(effects_active(), 0);
        CHECK_EQ_INT((int)DSD(DS_000FCCE8), 0);
        CHECK_EQ_INT((int)DSD(0), (int)m0);
        CHECK_EQ_INT((int)DSD(4), (int)m4);
    }

    /* Clearing an uninitialised pool must be a safe no-op, not a walk from the
     * zeroed sentinel through mem[] — the same hazard actors_reset guards. */
    mem_fill(DS_000FCCE0, 0, 8u);
    effects_clear();
    CHECK_EQ_INT(effects_active(), 0);

    /* Building the free list makes records available; an empty list is
     * self-linked, so a spawn succeeds and becomes the one active effect. */
    tf_effects_fixture_begin();

    /* 0x13ADC link direction: both sentinels self-linked; 0x249C0 appends each
     * record before the free sentinel, so the free list runs pool order
     * (0xF0B00 next, 0xFC4CC last) and both ends point at the sentinel. */
    CHECK_EQ_INT((int)DSD(DS_000FCCE0), (int)DS_000FCCE0);
    CHECK_EQ_INT((int)DSD(DS_000FCCE4), (int)DS_000FCCE0);
    CHECK_EQ_INT((int)DSD(DS_000FCCE8), (int)DS_000F0B00);
    CHECK_EQ_INT((int)DSD(DS_000F0B00 + 4), (int)DS_000FCCE8);
    CHECK_EQ_INT((int)DSD(DS_000F0B00), (int)(DS_000F0B00 + EFFECTS_REC_SIZE));
    CHECK_EQ_INT((int)DSD(DS_000FCCE8 + 4), (int)(DS_000FCCE0 - EFFECTS_REC_SIZE));
    CHECK_EQ_INT((int)DSD(DS_000FCCE0 - EFFECTS_REC_SIZE), (int)DS_000FCCE8);

    {
        u32 src = EFFECTS_TEST_SRC;
        u32 rec;
        effects_reset_source(0x40u);          /* source record, entry count 0 at +0xC */
        rec = effects_spawn(src, 0x2Au, 0x419786Cu);
        CHECK(rec != 0, "spawn takes a record once the free list is built");
        CHECK_EQ_INT(effects_active(), 1);
        CHECK_EQ_INT((int)rec, (int)DS_000F0B00);   /* pops the free-list head */
        CHECK_EQ_INT((int)DSB(rec + 0x0D), 0x2A);
        CHECK_EQ_INT((int)DSB(rec + 0x0F), 0x80);
        CHECK_EQ_INT((int)DSD(rec + 0x08), (int)src);
        /* 0x249B0 head-inserts after the active sentinel: rec[0]=next=active,
         * rec[4]=prev=active, and the sentinel's next/prev both front rec. */
        CHECK_EQ_INT((int)DSD(DS_000FCCE0), (int)rec);
        CHECK_EQ_INT((int)DSD(DS_000FCCE4), (int)rec);
        CHECK_EQ_INT((int)DSD(rec), (int)DS_000FCCE0);
        CHECK_EQ_INT((int)DSD(rec + 4), (int)DS_000FCCE0);
        CHECK_EQ_INT((int)DSD(DS_000FCCE8), (int)(DS_000F0B00 + EFFECTS_REC_SIZE));
        /* A second spawn distinguishes insert-AFTER from insert-BEFORE: the new
         * record must front the sentinel and point back at the first, not land
         * behind it. */
        u32 rec2 = effects_spawn(src, 0u, 0u);
        CHECK(rec2 != 0, "second spawn");
        CHECK_EQ_INT(effects_active(), 2);
        CHECK_EQ_INT((int)rec2, (int)(DS_000F0B00 + EFFECTS_REC_SIZE));
        CHECK_EQ_INT((int)DSD(DS_000FCCE0), (int)rec2);   /* sentinel.next = head */
        CHECK_EQ_INT((int)DSD(DS_000FCCE4), (int)rec);    /* sentinel.prev = tail */
        CHECK_EQ_INT((int)DSD(rec2), (int)rec);           /* rec2.next = rec */
        CHECK_EQ_INT((int)DSD(rec2 + 4), (int)DS_000FCCE0);
        CHECK_EQ_INT((int)DSD(rec + 4), (int)rec2);       /* rec.prev = rec2 */
        CHECK_EQ_INT((int)DSD(rec), (int)DS_000FCCE0);
    }

    /* Clear returns the pool to empty and zeroes the active count, which is the
     * flag 0x121A0's phase-2 exit tests. */
    effects_clear();
    CHECK_EQ_INT(effects_active(), 0);
    CHECK_EQ_INT((int)DSD(DS_000FCCE0), (int)DS_000FCCE0);
    CHECK_EQ_INT((int)DSD(DS_000FCCE8), (int)DS_000F0B00);   /* record returned */
    CHECK_EQ_INT((int)DSD(DS_000F0B00 + 4), (int)DS_000FCCE8);
    CHECK_EQ_INT((int)DSD(DS_000F0B00), (int)(DS_000F0B00 + EFFECTS_REC_SIZE));

    {
        /* The entry count is the SOURCE record's +0xC; each entry is zeroed at
         * +0x10 and copied from the resolved handle's block at +0x410. A fake
         * one-entry resource table keeps this block self-contained. */
        u32 src = EFFECTS_TEST_SRC;
        u32 saved_tab = DSD(DS_001014E0), saved_n = DSD(DS_001014F0);
        u32 tab = src + 0x100u;
        u32 blk = src + 0x200u;
        u32 head = DSD(DS_000FCCE8);
        u32 rec;
        effects_reset_restab(tab, blk);
        DSD(blk + 4) = 0x11111111u;
        DSD(blk + 8) = 0x22222222u;
        DSD(blk + 12) = 0x33333333u;
        effects_reset_source(0x40u);
        DSD(src + 0x0C) = 3u;
        /* Dirty the record's entry tables, including one slot PAST the count,
         * so the count-bounded zero/copy loops are proven, not vacuous. */
        DSD(head + 0x10) = 0xAAAAAAAAu;
        DSD(head + 0x14) = 0xAAAAAAABu;
        DSD(head + 0x18) = 0xAAAAAAACu;
        DSD(head + 0x1C) = 0xAAAAAAADu;
        DSD(head + 0x410) = 0xBBBBBBBBu;
        DSD(head + 0x414) = 0xBBBBBBBCu;
        DSD(head + 0x418) = 0xBBBBBBBDu;
        DSD(head + 0x41C) = 0xBBBBBBBEu;
        rec = effects_spawn(src, 0x2Bu, 0u);    /* res_handle(0, 0) */
        CHECK(rec != 0, "spawn with three entries");
        CHECK_EQ_INT((int)rec, (int)head);
        CHECK_EQ_INT(effects_active(), 1);
        CHECK_EQ_INT((int)DSB(rec + 0x0D), 0x2B);
        CHECK_EQ_INT((int)DSD(rec + 0x10), 0);
        CHECK_EQ_INT((int)DSD(rec + 0x14), 0);
        CHECK_EQ_INT((int)DSD(rec + 0x18), 0);
        CHECK_EQ_INT((int)DSD(rec + 0x1C), (int)0xAAAAAAADu);   /* past count kept */
        CHECK_EQ_INT((int)DSD(rec + 0x410), 0x11111111);
        CHECK_EQ_INT((int)DSD(rec + 0x414), 0x22222222);
        CHECK_EQ_INT((int)DSD(rec + 0x418), 0x33333333);
        CHECK_EQ_INT((int)DSD(rec + 0x41C), (int)0xBBBBBBBEu);  /* past count kept */
        /* active head insert again, with the record as the only active node. */
        CHECK_EQ_INT((int)DSD(DS_000FCCE0), (int)rec);
        CHECK_EQ_INT((int)DSD(DS_000FCCE4), (int)rec);
        CHECK_EQ_INT((int)DSD(rec), (int)DS_000FCCE0);
        CHECK_EQ_INT((int)DSD(rec + 4), (int)DS_000FCCE0);

        /* 0x13C70 tests the count signed (`test`/`jle`): a high-bit-set count is
         * non-positive and must be skipped, not iterated ~4e9 times. */
        {
            u32 rec3 = DSD(DS_000FCCE8);
            effects_reset_source(0x40u);
            DSD(src + 0x0C) = 0xFFFFFFFFu;
            u32 rneg = effects_spawn(src, 0u, 0u);
            CHECK(rneg != 0, "non-positive count is skipped");
            CHECK_EQ_INT((int)rneg, (int)rec3);
            CHECK_EQ_INT(effects_active(), 2);
        }

        /* 0x1B544 can fail to resolve. A positive count must not dereference the
         * NULL: the +0x10 zeroing still runs, the +0x410 copy is skipped. */
        {
            u32 rec4 = DSD(DS_000FCCE8);
            DSD(DS_001014F0) = 0;               /* res_resolve(index 0) -> NULL */
            DSD(src + 0x0C) = 3u;
            DSD(rec4 + 0x410) = 0xCCCCCCCCu;
            u32 rnull = effects_spawn(src, 0u, 0u);
            CHECK(rnull != 0, "spawn with an unresolved handle");
            CHECK_EQ_INT((int)rnull, (int)rec4);
            CHECK_EQ_INT((int)DSD(rnull + 0x10), 0);
            CHECK_EQ_INT((int)DSD(rnull + 0x410), (int)0xCCCCCCCCu);
            CHECK_EQ_INT(effects_active(), 3);
            DSD(DS_001014F0) = 1;
        }

        effects_restore_restab(saved_tab, saved_n);
    }

    effects_clear();
    CHECK_EQ_INT(effects_active(), 0);

    {
        /* Full pool: 24 records come out, the 25th spawn finds the self-linked
         * free sentinel and must return 0 without touching either list. */
        u32 src = EFFECTS_TEST_SRC;
        tf_effects_fixture_begin();
        effects_reset_source(0x40u);
        for (u32 i = 0; i < 24u; i++)
            CHECK(effects_spawn(src, 0u, 0u) != 0, "pool has 24 records");
        CHECK_EQ_INT(effects_active(), 24);
        u32 ah = DSD(DS_000FCCE0), fh = DSD(DS_000FCCE8);
        CHECK_EQ_INT((int)fh, (int)DS_000FCCE8);   /* free list empty */
        CHECK_EQ_INT((int)effects_spawn(src, 0u, 0u), 0);
        CHECK_EQ_INT(effects_active(), 24);
        CHECK_EQ_INT((int)DSD(DS_000FCCE0), (int)ah);
        CHECK_EQ_INT((int)DSD(DS_000FCCE8), (int)fh);
    }

    effects_clear();
    CHECK_EQ_INT(effects_active(), 0);

    /* A second clear on an already-empty pool must not walk or corrupt the
     * self-linked sentinels. */
    effects_clear();
    CHECK_EQ_INT(effects_active(), 0);

    {
        /* 0x134C0 drains the list the spawn grew. The record's age counter
         * (rec+0xE) counts down from the byte arg stored at rec+0xD; the title
         * spawn passes 3 (flow.c 0x123EA), so the counter wraps every third
         * step and the type-3 body runs on steps 1 and 4. The first body is the
         * flag(0x80) palette pass; the second animates +0x10 toward +0x410 and,
         * with the source count (+0xC) zero, finds them equal immediately and
         * tears the record down. Four steps is exactly the raw's lifetime. */
        u32 src = EFFECTS_TEST_SRC;
        tf_effects_fixture_begin();
        effects_reset_source(0x40u);          /* source count +0xC == 0 */
        u32 rec = effects_spawn(src, 3u, 0u);
        CHECK_EQ_INT(effects_active(), 1);
        CHECK_EQ_INT((int)rec, (int)DS_000F0B00);
        effects_step();
        CHECK_EQ_INT(effects_active(), 1);   /* wrap 1: flag palette pass */
        effects_step();
        CHECK_EQ_INT(effects_active(), 1);
        effects_step();
        CHECK_EQ_INT(effects_active(), 1);
        effects_step();
        CHECK_EQ_INT(effects_active(), 0);   /* wrap 2: teardown drains it */

        /* The teardown unlinked from active and re-linked to the free list
         * exactly once: the active sentinel is self-linked and the record
         * appears in the free walk exactly once, with all 24 records back. */
        CHECK_EQ_INT((int)DSD(DS_000FCCE0), (int)DS_000FCCE0);
        int seen = 0, n = 0;
        for (u32 p = DSD(DS_000FCCE8); p != DS_000FCCE8; p = DSD(p)) {
            if (p == rec) seen++;
            n++;
        }
        CHECK_EQ_INT(seen, 1);
        CHECK_EQ_INT(n, 24);
    }

    {
        /* 0x13D4C: type 4, +0x0F = 0, +0x0E = 1, +0x10 = the resolved block's
         * dwords (no +0x410 fill), active count +1. */
        u32 src = EFFECTS_TEST_SRC;
        u32 saved_tab = DSD(DS_001014E0), saved_n = DSD(DS_001014F0);
        u32 tab = src + 0x100u, blk = src + 0x200u, rec;
        tf_effects_fixture_begin();
        effects_reset_source(0x300u);
        DSD(src + 0x00) = 4u;             /* handle = index 0, offset 4 */
        DSD(src + 0x0C) = 2u;
        effects_reset_restab(tab, blk);
        DSD(blk + 4) = 0x40404040u;
        DSD(blk + 8) = 0x00707070u;
        DSD(blk + 12) = 0x0A0B0C0Du;
        rec = effects_spawn_darken(src, 0x2Cu);
        CHECK(rec != 0, "0x13D4C takes a record");
        CHECK_EQ_INT(effects_active(), 1);
        CHECK_EQ_INT((int)DSB(rec + 0x0C), 4);
        CHECK_EQ_INT((int)DSB(rec + 0x0F), 0);
        CHECK_EQ_INT((int)DSB(rec + 0x0E), 1);
        CHECK_EQ_INT((int)DSD(rec + 0x08), (int)src);
        CHECK_EQ_INT((int)DSB(rec + 0x0D), 0x2C);
        /* Handles beyond offset 0 prove the handle came from DSD(source_rec):
         * with offset 0 these would be blk+4/blk+8 instead. */
        CHECK_EQ_INT((int)DSD(rec + 0x10), 0x00707070);
        CHECK_EQ_INT((int)DSD(rec + 0x14), 0x0A0B0C0D);
        /* The record front-inserts into the active list like 0x13C70. */
        CHECK_EQ_INT((int)DSD(DS_000FCCE0), (int)rec);
        CHECK_EQ_INT((int)DSD(rec), (int)DS_000FCCE0);
        effects_restore_restab(saved_tab, saved_n);
    }
    effects_clear();
    CHECK_EQ_INT(effects_active(), 0);

    {
        /* 0x13D4C's copy loop tests the source count signed
         * (0x13D9D mov ebp,[esi+0xc]; test/jle, file 0x66BF1): a high-bit-set
         * count is skipped, not iterated ~4e9 times. The record still builds. */
        u32 src = EFFECTS_TEST_SRC;
        tf_effects_fixture_begin();
        effects_reset_source(0x40u);
        DSD(src + 0x0C) = 0xFFFFFFFFu;
        u32 head = DSD(DS_000FCCE8);
        DSD(head + 0x10) = 0xDDDDDDDDu;
        u32 rec = effects_spawn_darken(src, 0u);
        CHECK(rec != 0, "0x13D4C non-positive count is skipped");
        CHECK_EQ_INT((int)rec, (int)head);
        CHECK_EQ_INT((int)DSD(rec + 0x10), (int)0xDDDDDDDDu);  /* no copy */
    }
    effects_clear();
    CHECK_EQ_INT(effects_active(), 0);

    {
        /* 0x1B544 can fail to resolve. The new producers keep the
         * effects_spawn PORT guard: the resolved copy is skipped rather than
         * dereferencing NULL, while the field writes and the list insert still
         * run. Darken: +0x10 untouched; pulse: white fill still runs; scroll:
         * both blocks untouched. */
        u32 src = EFFECTS_TEST_SRC;
        u32 saved_tab = DSD(DS_001014E0), saved_n = DSD(DS_001014F0);
        tf_effects_fixture_begin();
        effects_reset_source(0x40u);
        DSD(src + 0x00) = 4u;             /* handle = index 0, offset 4 */
        DSD(src + 0x0C) = 2u;
        DSD(DS_001014F0) = 0;             /* res_resolve(index 0) -> NULL */
        u32 head = DSD(DS_000FCCE8);
        DSD(head + 0x10) = 0xDDDDDDDDu;
        u32 rd = effects_spawn_darken(src, 0u);
        CHECK(rd != 0, "0x13D4C unresolved handle takes a record");
        CHECK_EQ_INT((int)rd, (int)head);
        CHECK_EQ_INT((int)DSD(rd + 0x10), (int)0xDDDDDDDDu);
        u32 head2 = DSD(DS_000FCCE8);
        DSD(head2 + 0x410) = 0xEEEEEEEEu;
        u32 rp = effects_spawn_pulse(src, 0u);
        CHECK(rp != 0, "0x13E28 unresolved handle takes a record");
        CHECK_EQ_INT((int)rp, (int)head2);
        CHECK_EQ_INT((int)DSD(rp + 0x10), 0x00FFFFFF);        /* white fill ran */
        CHECK_EQ_INT((int)DSD(rp + 0x410), (int)0xEEEEEEEEu); /* copy skipped */
        u32 head3 = DSD(DS_000FCCE8);
        DSD(head3 + 0x14) = 0xCCCCCCCCu;
        DSD(head3 + 0x414) = 0xBBBBBBBBu;
        u32 rs = effects_spawn_scroll(src, 0, 2u, 1u);
        CHECK(rs != 0, "0x13B3C unresolved handle takes a record");
        CHECK_EQ_INT((int)rs, (int)head3);
        CHECK_EQ_INT((int)DSD(rs + 0x14), (int)0xCCCCCCCCu);
        CHECK_EQ_INT((int)DSD(rs + 0x414), (int)0xBBBBBBBBu);
        effects_restore_restab(saved_tab, saved_n);
    }
    effects_clear();
    CHECK_EQ_INT(effects_active(), 0);

    {
        /* 0x13E28: type 6, +0x0F = 0x80, +0x0E = 1, +0x10 = 0xFFFFFF,
         * +0x410 = the resolved block, count +1. */
        u32 src = EFFECTS_TEST_SRC;
        u32 saved_tab = DSD(DS_001014E0), saved_n = DSD(DS_001014F0);
        u32 tab = src + 0x100u, blk = src + 0x200u, rec;
        tf_effects_fixture_begin();
        effects_reset_source(0x300u);
        DSD(src + 0x00) = 4u;             /* handle = index 0, offset 4 */
        DSD(src + 0x0C) = 2u;
        effects_reset_restab(tab, blk);
        DSD(blk + 4) = 0x00102030u;
        DSD(blk + 8) = 0x00040506u;
        DSD(blk + 12) = 0x00070809u;
        rec = effects_spawn_pulse(src, 1u);
        CHECK(rec != 0, "0x13E28 takes a record");
        CHECK_EQ_INT(effects_active(), 1);
        CHECK_EQ_INT((int)DSB(rec + 0x0C), 6);
        CHECK_EQ_INT((int)DSB(rec + 0x0F), 0x80);
        CHECK_EQ_INT((int)DSB(rec + 0x0E), 1);
        CHECK_EQ_INT((int)DSD(rec + 0x08), (int)src);
        CHECK_EQ_INT((int)DSD(rec + 0x10), 0x00FFFFFF);
        CHECK_EQ_INT((int)DSD(rec + 0x14), 0x00FFFFFF);
        CHECK_EQ_INT((int)DSD(rec + 0x410), 0x00040506);
        CHECK_EQ_INT((int)DSD(rec + 0x414), 0x00070809);
        effects_restore_restab(saved_tab, saved_n);
    }
    effects_clear();
    CHECK_EQ_INT(effects_active(), 0);
    {
        /* 0x13E28 at count == 257: the +0x10 white fill (i = 256 -> +0x410) and
         * the +0x410 resolved copy (j = 0 -> +0x410) overlap. The raw runs the
         * whole white loop before the resolved loop, so +0x410 ends as the
         * resolved value; a merged loop would leave 0x00FFFFFF there. */
        u32 src = EFFECTS_TEST_SRC;
        u32 saved_tab = DSD(DS_001014E0), saved_n = DSD(DS_001014F0);
        u32 tab = src + 0x100u, blk = src + 0x200u, rec;
        tf_effects_fixture_begin();
        effects_reset_source(0x300u);
        DSD(src + 0x00) = 4u;             /* handle = index 0, offset 4 */
        DSD(src + 0x0C) = 257u;
        effects_reset_restab(tab, blk);
        DSD(blk + 4) = 0x11111111u;
        DSD(blk + 8) = 0x22222222u;
        rec = effects_spawn_pulse(src, 1u);
        CHECK(rec != 0, "0x13E28 takes a 257-entry record");
        CHECK_EQ_INT((int)DSD(rec + 0x10), 0x00FFFFFF);
        CHECK_EQ_INT((int)DSD(rec + 0x410), 0x22222222);
        effects_restore_restab(saved_tab, saved_n);
    }
    effects_clear();
    CHECK_EQ_INT(effects_active(), 0);

    {
        /* 0x13B3C: flag != 0 -> type 2, +0x0E = 1; the resolved block is
         * copied into BOTH +0x14 and +0x414, walked by the signed offset. */
        u32 src = EFFECTS_TEST_SRC;
        u32 saved_tab = DSD(DS_001014E0), saved_n = DSD(DS_001014F0);
        u32 tab = src + 0x100u, blk = src + 0x200u, rec;
        tf_effects_fixture_begin();
        effects_reset_source(0x300u);
        DSD(src + 0x0C) = 3u;
        effects_reset_restab(tab, blk);
        effects_seed_block6(blk);
        rec = effects_spawn_scroll(src, 0, 2u, 1u);
        CHECK(rec != 0, "0x13B3C takes a record");
        CHECK_EQ_INT((int)DSB(rec + 0x0C), 2);
        CHECK_EQ_INT((int)DSB(rec + 0x0E), 1);
        CHECK_EQ_INT((int)DSB(rec + 0x0F), 2);
        CHECK_EQ_INT((int)DSB(rec + 0x0D), 1);
        CHECK_EQ_INT((int)DSB(rec + 0x10), 0);
        CHECK_EQ_INT((int)DSD(rec + 0x14), 0x40);
        CHECK_EQ_INT((int)DSD(rec + 0x18), 0x41);
        CHECK_EQ_INT((int)DSD(rec + 0x414), 0x40);
        CHECK_EQ_INT((int)DSD(rec + 0x418), 0x41);
        effects_restore_restab(saved_tab, saved_n);
    }
    effects_clear();
    CHECK_EQ_INT(effects_active(), 0);

    {
        /* 0x13B3C negative-offset arm (0x13B97..0x13BEB): a do-while bounded by
         * the byte count runs count+1 times, so offset -1 / count 2 fills
         * +0x14..+0x1C descending from resolved[4] to resolved[2]. */
        u32 src = EFFECTS_TEST_SRC;
        u32 saved_tab = DSD(DS_001014E0), saved_n = DSD(DS_001014F0);
        u32 tab = src + 0x100u, blk = src + 0x200u, rec;
        tf_effects_fixture_begin();
        effects_reset_source(0x300u);
        effects_reset_restab(tab, blk);
        effects_seed_block6(blk);
        rec = effects_spawn_scroll(src, -1, 2u, 1u);
        CHECK(rec != 0, "0x13B3C negative offset");
        CHECK_EQ_INT((int)DSD(rec + 0x14), 0x41);
        CHECK_EQ_INT((int)DSD(rec + 0x18), 0x42);
        CHECK_EQ_INT((int)DSD(rec + 0x1C), 0x43);
        CHECK_EQ_INT((int)DSD(rec + 0x414), 0x41);
        CHECK_EQ_INT((int)DSD(rec + 0x418), 0x42);
        CHECK_EQ_INT((int)DSD(rec + 0x41C), 0x43);
        effects_clear();
        CHECK_EQ_INT(effects_active(), 0);

        /* offset == -0x80 (0x13B9E cmp eax,-0x80) starts at resolved[count]
         * instead of resolved[1 + count - offset]. */
        tf_effects_fixture_begin();
        effects_reset_source(0x300u);
        effects_reset_restab(tab, blk);
        effects_seed_block6(blk);
        rec = effects_spawn_scroll(src, -0x80, 2u, 1u);
        CHECK(rec != 0, "0x13B3C offset -0x80");
        CHECK_EQ_INT((int)DSD(rec + 0x14), 0);      /* resolved[0] */
        CHECK_EQ_INT((int)DSD(rec + 0x18), 0x40);   /* resolved[1] */
        CHECK_EQ_INT((int)DSD(rec + 0x1C), 0x41);   /* resolved[2] */

        /* The descending arm is a do-while, so count == 0 still writes index 0
         * once: sp = resolved + 1 + 0 - (-1) = resolved[2], stored at +0x14
         * and +0x414. */
        rec = effects_spawn_scroll(src, -1, 0u, 1u);
        CHECK(rec != 0, "0x13B3C negative offset count 0");
        CHECK_EQ_INT((int)DSD(rec + 0x14), 0x41);   /* resolved[2] */
        CHECK_EQ_INT((int)DSD(rec + 0x414), 0x41);

        /* flag == 0 -> type 0 and state byte +0x0E = 0 (the raw truth; such a
         * record never retires, and the raw does not bump DS_0009AF3D). */
        effects_clear();
        tf_effects_fixture_begin();
        effects_reset_source(0x300u);
        effects_reset_restab(tab, blk);
        rec = effects_spawn_scroll(src, 0, 0u, 0u);
        CHECK(rec != 0, "0x13B3C zero flag");
        CHECK_EQ_INT((int)DSB(rec + 0x0C), 0);
        CHECK_EQ_INT((int)DSB(rec + 0x0E), 0);
        CHECK_EQ_INT((int)DSB(rec + 0x0D), 0);
        CHECK_EQ_INT((int)DSB(rec + 0x0F), 0);
        CHECK_EQ_INT(effects_active(), 0);   /* raw never bumps the count */

        /* flag == 0 still copies the resolved block; only the type/state bytes
         * differ from the flag != 0 arm. */
        effects_seed_block6(blk);
        rec = effects_spawn_scroll(src, 0, 2u, 0u);
        CHECK(rec != 0, "0x13B3C zero flag with count 2");
        CHECK_EQ_INT((int)DSB(rec + 0x0C), 0);
        CHECK_EQ_INT((int)DSB(rec + 0x0E), 0);
        CHECK_EQ_INT((int)DSD(rec + 0x14), 0x40);
        CHECK_EQ_INT((int)DSD(rec + 0x18), 0x41);
        CHECK_EQ_INT((int)DSD(rec + 0x414), 0x40);
        CHECK_EQ_INT((int)DSD(rec + 0x418), 0x41);
        effects_restore_restab(saved_tab, saved_n);
    }
    effects_clear();
    CHECK_EQ_INT(effects_active(), 0);

    {
        /* End-to-end: 0x13D4C's type-4 record darkens its +0x10 block toward
         * zero and enqueues it; gfx_flush_palette drains the dirty list into
         * gfx_dac. Each step subtracts 8 from every colour lane, and the DAC
         * reader takes bits 2..7 of each lane, so a 0x40 lane becomes 0x38
         * after one step. */
        u32 src = EFFECTS_TEST_SRC;
        u32 saved_tab = DSD(DS_001014E0), saved_n = DSD(DS_001014F0);
        u32 tab = src + 0x100u, blk = src + 0x200u;
        palette_list_init();
        tf_effects_fixture_begin();
        effects_reset_source(0x300u);
        DSD(src + 0x00) = 4u;             /* handle = index 0, offset 4 */
        DSD(src + 0x08) = 0x40u;          /* first DAC index for the record */
        DSD(src + 0x0C) = 2u;
        effects_reset_restab(tab, blk);
        DSD(blk + 4) = 0x40404040u;
        DSD(blk + 8) = 0x40404040u;
        DSD(blk + 12) = 0x40404040u;
        u32 rec = effects_spawn_darken(src, 1u);
        CHECK(rec != 0, "end-to-end spawn");
        effects_step();                    /* state 1 -> 0, case-4 body runs */
        gfx_flush_palette();
        CHECK_EQ_INT(gfx_dac[0x40][0], 0x38);
        CHECK_EQ_INT(gfx_dac[0x40][1], 0x38);
        CHECK_EQ_INT(gfx_dac[0x40][2], 0x38);
        CHECK_EQ_INT(gfx_dac[0x41][0], 0x38);
        /* Cannot pass vacuously: 0x40 darkens 0x38, 0x30, ..., 0x00 across
         * steps 1..8, then step 9 sees every lane zero and retires the record.
         * The active count returning to 0 proves the step actually ran the
         * case-4 body and removed the record, not that the DAC was never
         * written. */
        for (int i = 0; i < 8; i++) effects_step();
        CHECK_EQ_INT(effects_active(), 0);
        effects_restore_restab(saved_tab, saved_n);
    }
    effects_clear();
    CHECK_EQ_INT(effects_active(), 0);

    {
        /* End-to-end type 6 (0x13E28). The first case-6 body (0x13996) is the
         * flag pass: 0x1399c test cl,cl / 0x139a8 call 0x33734 enqueues the
         * white +0x10 block and clears +0x0F (0x139ad). The next body darkens
         * toward the +0x410 target with the subtract-8 clamp (0x139f0 sub
         * edx,8 / 0x139f7 mov edx,eax) and enqueues at 0x13ab4.
         * gfx_flush_palette takes each lane at bits 2..7 and expands 6 -> 8:
         * white 0x00FFFFFF gives r/g/b = (0xFFFFFF >> 2/10/18) & 0x3F = 0x3F,
         * (0x3F << 2) | (0x3F >> 4) = 0xFF. One darken is 0xFF - 8 = 0xF7:
         * v = (0xF7 >> 2) & 0x3F = 0x3D, (0x3D << 2) | (0x3D >> 4) = 0xF7. */
        u32 src = EFFECTS_TEST_SRC;
        u32 saved_tab = DSD(DS_001014E0), saved_n = DSD(DS_001014F0);
        u32 tab = src + 0x100u, blk = src + 0x200u;
        palette_list_init();
        tf_effects_fixture_begin();
        effects_reset_source(0x300u);
        DSD(src + 0x00) = 4u;             /* handle = index 0, offset 4 */
        DSD(src + 0x08) = 0x40u;          /* first DAC index */
        DSD(src + 0x0C) = 1u;
        effects_reset_restab(tab, blk);
        /* handle 4 shifts the resolved base by 4, so the +0x410 target is
         * blk+8. The darken reads lanes at bits 0/8/16 only, so the target
         * must have a zero high byte (0x00404040) for the full-dword equality
         * test to ever succeed and let the record retire. */
        DSD(blk + 4) = 0x00404040u;
        DSD(blk + 8) = 0x00404040u;
        DSD(blk + 12) = 0x00404040u;
        u32 rec = effects_spawn_pulse(src, 1u);
        CHECK(rec != 0, "end-to-end pulse spawn");
        effects_step();                    /* state 1 -> 0, flag pass */
        gfx_flush_palette();
        CHECK_EQ_INT(gfx_dac[0x40][0], 0xFF);
        CHECK_EQ_INT(gfx_dac[0x40][1], 0xFF);
        CHECK_EQ_INT(gfx_dac[0x40][2], 0xFF);
        effects_step();                    /* darken once: 0xFF -> 0xF7 */
        gfx_flush_palette();
        CHECK_EQ_INT(gfx_dac[0x40][0], 0xF7);
        CHECK_EQ_INT(gfx_dac[0x40][1], 0xF7);
        CHECK_EQ_INT(gfx_dac[0x40][2], 0xF7);
        /* Cannot pass vacuously: 0xFF - 8*24 clamps to the 0x40 target, then
         * the all-equal pass takes the removal arm (0x13a92 test al,al /
         * 0x13a96..0x13aac) and retires the record. */
        for (int i = 0; i < 24; i++) effects_step();
        CHECK_EQ_INT(effects_active(), 0);
        effects_restore_restab(saved_tab, saved_n);
    }
    effects_clear();
    CHECK_EQ_INT(effects_active(), 0);

    {
        /* End-to-end type 2 (0x13B3C, flag != 0). The case-2 body (0x135a8)
         * rotates the +0x14 block by one dword and enqueues it through 0x33734
         * at 0x13abf. The positive-offset arm (0x135f1..0x13629) sets the
         * first DAC index to DSD(src+8) + (s8)offset (0x13627 add eax,esi) and
         * the count to DSB(rec+0x0F) (0x13624 mov dl,[edi+0xf]). gfx_flush
         * expands each 6-bit lane: 0x80 -> (0x80 >> 2) & 0x3F = 0x20 -> 0x82;
         * 0x40 -> (0x40 >> 2) & 0x3F = 0x10 -> 0x41. Step 1 rotates
         * [A,B] -> [B,A], so DAC[0x50] = 0x82 (B) and DAC[0x51] = 0x41 (A);
         * step 2 rotates back. */
        u32 src = EFFECTS_TEST_SRC;
        u32 saved_tab = DSD(DS_001014E0), saved_n = DSD(DS_001014F0);
        u32 tab = src + 0x100u, blk = src + 0x200u;
        palette_list_init();
        tf_effects_fixture_begin();
        effects_reset_source(0x300u);
        /* handle 0 (src+0x00 == 0) so resolved[1] = blk+4 = A. */
        DSD(src + 0x08) = 0x50u;          /* first DAC index */
        DSD(src + 0x0C) = 2u;
        effects_reset_restab(tab, blk);
        DSD(blk + 4) = 0x00000040u;       /* A = resolved[1] */
        DSD(blk + 8) = 0x00000080u;       /* B = resolved[2] */
        u32 rec = effects_spawn_scroll(src, 0, 2u, 1u);
        CHECK(rec != 0, "end-to-end scroll spawn");
        CHECK_EQ_INT((int)DSD(rec + 0x14), 0x40);
        CHECK_EQ_INT((int)DSD(rec + 0x18), 0x80);
        effects_step();                    /* state 1 -> 0, rotate + enqueue */
        gfx_flush_palette();
        CHECK_EQ_INT(gfx_dac[0x50][0], 0x82);   /* rotated-in B */
        CHECK_EQ_INT(gfx_dac[0x51][0], 0x41);   /* rotated A */
        /* Non-vacuous: the drain reset the dirty-list head, and the second
         * step re-rotates and re-drains with the opposite order. */
        CHECK_EQ_INT((int)DSD(DS_00107798), (int)DS_00107498);
        effects_step();
        gfx_flush_palette();
        CHECK_EQ_INT(gfx_dac[0x50][0], 0x41);
        CHECK_EQ_INT(gfx_dac[0x51][0], 0x82);
        effects_restore_restab(saved_tab, saved_n);
    }
    effects_clear();
    CHECK_EQ_INT(effects_active(), 0);

    {
        /* ---- the reachable fight-camera update: 0x1324C ------------------- */
        const u32 s_off = DSD(DS_000F0AF4), s_vel = DSD(DS_000F0AF6);
        const u32 s_e4 = DSD(DS_001088E4), s_d8 = DSD(DS_001088D8);
        const u32 s_dc = DSD(DS_001088DC), s_d4 = DSD(DS_001088D4);
        const u16 s_e0 = DSW(DS_001088E0), s_e2 = DSW(DS_001088E2);
        const u32 s_latch = DSD(DS_000E1C38), s_frame = DSD(DS_000EF6DC);
        const u8 s_bec = DSB(DS_00105BEC), s_bee = DSB(DS_00105BEE);
        const u32 s_tab = DSD(DS_000A8644);
        const u32 s_bt = DSD(DS_00104B00);
        const u32 s_lnext = DSD(DS_00105BCC), s_lprev = DSD(DS_00105BD0);

        /* 0x1324C input A (record 8.7): offset 0x10 + velocity -0x20 settles;
         * the offset zeroes and update-mask bit 0 clears, while the velocity
         * still takes its -0x20 step. */
        DSW(DS_000F0AF4) = 0x10u; DSW(DS_000F0AF6) = 0xFFE0u;
        DSB(DS_00104AE8) = 0x01;
        camera_shake_decay();
        CHECK_EQ_INT((int)DSW(DS_000F0AF4), 0);
        CHECK_EQ_INT((int)DSW(DS_000F0AF6), 0xFFE0);
        CHECK_EQ_INT((int)DSB(DS_00104AE8), 0x00);

        /* 0x1324C input B (record 8.7): a positive velocity does not settle; the
         * offset accumulates and the mask is untouched. */
        DSW(DS_000F0AF4) = 0x100u; DSW(DS_000F0AF6) = 0x20u;
        DSB(DS_00104AE8) = 0x01;
        camera_shake_decay();
        CHECK_EQ_INT((int)DSW(DS_000F0AF4), 0x120);
        CHECK_EQ_INT((int)DSW(DS_000F0AF6), 0);
        CHECK_EQ_INT((int)DSB(DS_00104AE8), 0x01);

        /* 0x1324C is update-table entry 0 and runs only when DS_00104AE8 bit 0
         * is set. The REAL dispatch (game_frame's run_process_table) must leave
         * the shake state alone with the bit clear and run the decay with it
         * set. The active-actor sentinel is self-linked so actors_update walks
         * nothing; every global game_frame touches here is restored below. */
        tf_effects_fixture_begin();
        CHECK(fn_resolve(FN_0001324C) != NULL,
              "0x1324C is registered for the update table");
        DSD(DS_000A8644) = FN_0001324C;      /* entry 0 */
        DSD(DS_00104B00) = 0;                /* skip the state machine */
        DSD(DS_00105BCC) = DS_00105BCC;
        DSD(DS_00105BD0) = DS_00105BCC;
        DSW(DS_000F0AF4) = 0x100u; DSW(DS_000F0AF6) = 0x20u;
        DSB(DS_00104AE8) = 0;
        game_frame();
        CHECK_EQ_INT((int)DSW(DS_000F0AF4), 0x100);  /* bit 0 clear: no run */
        CHECK_EQ_INT((int)DSW(DS_000F0AF6), 0x20);
        DSB(DS_00104AE8) = 1;
        game_frame();
        CHECK_EQ_INT((int)DSW(DS_000F0AF4), 0x120);  /* bit 0 set: ran */
        CHECK_EQ_INT((int)DSW(DS_000F0AF6), 0);

        DSD(DS_000F0AF4) = s_off; DSD(DS_000F0AF6) = s_vel;
        DSB(DS_00104AE8) = 0;
        DSD(DS_001088E4) = s_e4; DSD(DS_001088D8) = s_d8;
        DSD(DS_001088DC) = s_dc; DSD(DS_001088D4) = s_d4;
        DSW(DS_001088E0) = s_e0; DSW(DS_001088E2) = s_e2;
        DSD(DS_000E1C38) = s_latch; DSD(DS_000EF6DC) = s_frame;
        DSB(DS_00105BEC) = s_bec; DSB(DS_00105BEE) = s_bee;
        DSD(DS_000A8644) = s_tab; DSD(DS_00104B00) = s_bt;
        DSD(DS_00105BCC) = s_lnext; DSD(DS_00105BD0) = s_lprev;
    }

    {
        /* The PRESENTED palette, not just the dirty list: a spawned type-4
         * effect darkens its block and the render pass's gfx_flush_palette must
         * write gfx_dac, which is what gfx_present reads. Seed a sentinel that
         * differs from both the expected value and the untouched neighbour, so
         * the check cannot pass vacuously. */
        u32 src = EFFECTS_TEST_SRC;
        u32 saved_tab = DSD(DS_001014E0), saved_n = DSD(DS_001014F0);
        u32 tab = src + 0x100u, blk = src + 0x200u;
        palette_list_init();
        tf_effects_fixture_begin();
        effects_reset_source(0x300u);
        DSD(src + 0x00) = 4u;             /* handle = index 0, offset 4 */
        DSD(src + 0x08) = 0x40u;          /* first DAC index */
        DSD(src + 0x0C) = 2u;
        effects_reset_restab(tab, blk);
        DSD(blk + 4) = 0x40404040u;
        DSD(blk + 8) = 0x40404040u;
        DSD(blk + 12) = 0x40404040u;      /* resolved[2], the count-2 block */
        gfx_dac[0x40][0] = 0xAB; gfx_dac[0x40][1] = 0xAB; gfx_dac[0x40][2] = 0xAB;
        gfx_dac[0x41][0] = 0xCD; gfx_dac[0x41][1] = 0xCD; gfx_dac[0x41][2] = 0xCD;
        gfx_dac[0x50][0] = 0x77;          /* outside the enqueued 0x40..0x41 */
        u32 rec = effects_spawn_darken(src, 1u);
        CHECK(rec != 0, "presented-palette spawn");
        effects_step();
        gfx_flush_palette();
        CHECK_EQ_INT(gfx_dac[0x40][0], 0x38);
        CHECK_EQ_INT(gfx_dac[0x40][1], 0x38);
        CHECK_EQ_INT(gfx_dac[0x40][2], 0x38);
        CHECK_EQ_INT(gfx_dac[0x41][0], 0x38);
        CHECK_EQ_INT(gfx_dac[0x50][0], 0x77);
        effects_clear();
        effects_restore_restab(saved_tab, saved_n);
    }
    CHECK_EQ_INT(effects_active(), 0);

    {
        /* 0x29B74 / 0x41578 / 0x32A3C (record §42-E), the two 0x13D4C call
         * sites. The 0x33904 list gets five slots: A (+0 0x1111), B
         * (0x3E688), a dead C (+4 = 0, 0x3E688), D (0x88874B0) and E
         * (0x2222). The resource count is 0, so 0x1B544 resolves nothing and
         * no copy runs; one earlier effect is live, so 0x29B74's clear shows
         * as a count of 4 rather than 5, and 0x41578's missing clear as 3.
         * Every global either touches is saved and put back. */
        static u8 saved_list[0x190];
        const u32 tbl = DS_00107608;
        const u32 ea = tbl + 0x10u, eb = tbl + 0x20u, ec = tbl + 0x30u;
        const u32 ed = tbl + 0x40u, ee = tbl + 0x50u;
        u32 saved_tab = DSD(DS_001014E0), saved_n = DSD(DS_001014F0);
        u32 s_8ee = DSW(DS_001088EE), s_afe = DSW(DS_00104AFE);
        u32 s_afa = DSW(DS_00104AFA), s_b00 = DSD(DS_00104B00);
        u32 s_b25 = DSB(DS_00104B25), s_abc = DSD(DS_00104ABC);
        u32 s_b19 = DSB(DS_00104B19);
        u32 s_acc[4];
        for (u32 i = 0; i < 4u; i++) s_acc[i] = DSD(DS_0010746C + i * 4u);

        for (u32 i = 0; i < 0x190u; i++) saved_list[i] = DSB(tbl + i);
        mem_fill(tbl, 0, 0x190u);
        DSD(ea) = 0x1111u;       DSD(ea + 4u) = 1u;
        DSD(eb) = 0x3E688u;      DSD(eb + 4u) = 1u;
        DSD(ec) = 0x3E688u;                         /* +4 = 0: not live */
        DSD(ed) = 0x088874B0u;   DSD(ed + 4u) = 1u;
        DSD(ee) = 0x2222u;       DSD(ee + 4u) = 1u;
        DSD(DS_001014F0) = 0u;

        /* 0x29B74: clear, then a byte-3 darken for every live entry (no
         * predicate), in list order, each head-inserted. */
        tf_effects_fixture_begin();
        effects_reset_source(0x40u);
        CHECK(effects_spawn(EFFECTS_TEST_SRC, 0u, 0u) != 0, "an earlier effect");
        DSW(DS_001088EE) = 0x1234u;
        DSW(DS_00104AFE) = 0x5678u;
        DSD(DS_00104B00) = 0xDEAD0003u;
        frontend_darken_all();
        CHECK_EQ_INT(effects_active(), 4);
        {
            const u32 want[4] = { ee, ed, eb, ea };   /* head first */
            u32 r = DSD(DS_000FCCE0);
            for (u32 i = 0; i < 4u; i++) {
                CHECK_EQ_INT((int)DSD(r + 0x08u), (int)want[i]);
                CHECK_EQ_INT((int)DSB(r + 0x0Cu), 4);
                CHECK_EQ_INT((int)DSB(r + 0x0Du), 3);
                r = DSD(r);
            }
            CHECK_EQ_INT((int)r, (int)DS_000FCCE0);   /* no fifth record */
        }
        CHECK_EQ_INT((int)DSW(DS_001088EE), 0x78);
        CHECK_EQ_INT((int)DSW(DS_00104AFE), 0x78);
        CHECK_EQ_INT((int)DSD(DS_00104B00), (int)0xDEAD0015u);  /* a word store */
        effects_clear();

        /* 0x41578: no clear; a byte-2 darken only for B and D (the two
         * handles), then 0x32A3C on mode 5 (index 1) and the five stores. */
        tf_effects_fixture_begin();
        CHECK(effects_spawn(EFFECTS_TEST_SRC, 0u, 0u) != 0, "an earlier effect");
        DSW(DS_001088EE) = 0x1234u;
        DSW(DS_00104AFE) = 0x5678u;
        DSW(DS_00104AFA) = 0x9ABCu;
        DSD(DS_00104B00) = 0xDEAD0003u;
        DSB(DS_00104B25) = 0xAAu;
        DSD(DS_00104ABC) = 5u;
        DSB(DS_00104B19) = 1u;
        for (u32 i = 0; i < 4u; i++)
            DSD(DS_0010746C + i * 4u) = 0x1000u + i;
        frontend_darken_marked();
        CHECK_EQ_INT(effects_active(), 3);
        {
            u32 r = DSD(DS_000FCCE0);
            CHECK_EQ_INT((int)DSD(r + 0x08u), (int)ed);
            CHECK_EQ_INT((int)DSB(r + 0x0Cu), 4);
            CHECK_EQ_INT((int)DSB(r + 0x0Du), 2);
            r = DSD(r);
            CHECK_EQ_INT((int)DSD(r + 0x08u), (int)eb);
            CHECK_EQ_INT((int)DSB(r + 0x0Du), 2);
            r = DSD(r);
            CHECK_EQ_INT((int)DSD(r + 0x08u), (int)EFFECTS_TEST_SRC);
        }
        CHECK_EQ_INT((int)DSD(DS_0010746C + 0u), 0x1000);
        CHECK_EQ_INT((int)DSD(DS_0010746C + 4u), 0);      /* 5 & 3 = 1 */
        CHECK_EQ_INT((int)DSD(DS_0010746C + 8u), 0x1002);
        CHECK_EQ_INT((int)DSD(DS_0010746C + 12u), 0x1003);
        CHECK_EQ_INT((int)DSW(DS_00104AFE), 0x78);
        CHECK_EQ_INT((int)DSW(DS_001088EE), 0);
        CHECK_EQ_INT((int)DSW(DS_00104AFA), 0x13);
        CHECK_EQ_INT((int)DSD(DS_00104B00), (int)0xDEAD0015u);
        CHECK_EQ_INT((int)DSB(DS_00104B25), 0);
        effects_clear();

        /* 0x32A3C alone: mode 0 still takes and zeroes its accumulator
         * (0x32A56 runs before the 0x32A5F mode-0 exit). */
        for (u32 i = 0; i < 4u; i++)
            DSD(DS_0010746C + i * 4u) = 0x2000u + i;
        config_play_time_close(4u, 0u);
        CHECK_EQ_INT((int)DSD(DS_0010746C + 0u), 0);
        CHECK_EQ_INT((int)DSD(DS_0010746C + 4u), 0x2001);
        CHECK_EQ_INT((int)DSD(DS_0010746C + 8u), 0x2002);
        CHECK_EQ_INT((int)DSD(DS_0010746C + 12u), 0x2003);

        for (u32 i = 0; i < 4u; i++) DSD(DS_0010746C + i * 4u) = s_acc[i];
        DSB(DS_00104B19) = (u8)s_b19;
        DSD(DS_00104ABC) = s_abc;
        DSB(DS_00104B25) = (u8)s_b25;
        DSD(DS_00104B00) = s_b00;
        DSW(DS_00104AFA) = (u16)s_afa;
        DSW(DS_00104AFE) = (u16)s_afe;
        DSW(DS_001088EE) = (u16)s_8ee;
        effects_restore_restab(saved_tab, saved_n);
        for (u32 i = 0; i < 0x190u; i++) DSB(tbl + i) = saved_list[i];
    }
    CHECK_EQ_INT(effects_active(), 0);

    return g_failures - before;
}

/* ---- the high-score tables (record §46-A) ---- */

/* The original's tables after 0x1E824 on a fresh CMOS (all zero): 0x105E34..
 * 0x105ECC, read from Task 33's DOSBox-X memory file (t33/db/guest.mem). */
static const u8 hs_orig[153] = {
    0x00, 0x07, 0xA1, 0x20, 0xF4, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x06, 0x1A, 0x80, 0xC3, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x05, 0x57, 0x30, 0xA1, 0x49, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x04, 0x93, 0xE0, 0x6D, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x03, 0xD0, 0x90, 0x6A, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x03, 0x0D, 0x40, 0x61, 0x5C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x60, 0x62, 0x4D, 0x42, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xC3, 0x50, 0xA8, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x4E, 0x20, 0x17, 0x55, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x64, 0xA4, 0x12, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x07, 0xA1, 0x20, 0xB4, 0x14, 0x2E, 0x03, 0xB7, 0x14, 0x2E, 0x03,
    0x27, 0x34, 0x65, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

#define HS_T0 0x105E34u
#define HS_T1 0x105EACu
#define HS_SCRATCH 0x3FFFF00u

static int hs_match(u32 addr, const u8 *want, u32 n)
{
    for (u32 i = 0; i < n; i++)
        if (DSB(addr + i) != want[i]) return 0;
    return 1;
}

/* 0x2DB58/0x2DBC4/0x2DCA0, 0x1E918/0x1E988/0x1E824 on the loaded image. The
 * config region with the tables and the decode buffer (DS_00105D88..+0x1C0),
 * the three name buffers, DS_00104528 and a scratch source are saved and
 * restored. */
static void check_hiscore(void)
{
    static u8 sreg[0x1C0], snb[0x164], sscr[0x40];
    const u32 nb = DS_001042C7;
    u32 left, size, i;
    memcpy(sreg, mem + DS_00105D88, sizeof sreg);
    memcpy(snb, mem + nb, sizeof snb);
    memcpy(sscr, mem + HS_SCRATCH, sizeof sscr);
    u32 s28 = DSD(DS_00104528);

    /* The image's descriptors and pointers (0x2D3FC, 0x2D478). */
    CHECK_EQ_INT((int)DSD(DS_0002D478), (int)HS_T0);

    /* 0x2DB58: the address, the bytes left and the record size. */
    left = size = 0xDEADu;
    CHECK_EQ_INT((int)hiscore_locate(0u, 0u, &left, &size), (int)HS_T0);
    CHECK_EQ_INT((int)left, 120);
    CHECK_EQ_INT((int)size, 12);
    CHECK_EQ_INT((int)hiscore_locate(9u, 0u, &left, &size), (int)(HS_T0 + 108u));
    CHECK_EQ_INT((int)left, 12);
    left = size = 0xDEADu;
    CHECK_EQ_INT((int)hiscore_locate(10u, 0u, &left, &size), 0);
    CHECK_EQ_INT((int)left, 0xDEAD);
    CHECK_EQ_INT((int)hiscore_locate(0u, 1u, &left, &size), (int)HS_T1);
    CHECK_EQ_INT((int)size, 28);
    CHECK_EQ_INT((int)hiscore_locate(1u, 1u, NULL, NULL), 0);
    CHECK_EQ_INT((int)hiscore_locate(0u, 2u, &left, &size), 0x105EC8);
    CHECK_EQ_INT((int)size, 5);
    CHECK_EQ_INT((int)hiscore_locate(0u, 3u, &left, &size), 0);

    /* 0x1E824 on zeroed tables with the original's DS_00104528 (0x142095: bit
     * 0x4000 clear, bit 0x2000 set, the audit not due: fields 0x27/0x26 are
     * below 2000/200) writes the original's bytes, blanks the three name
     * buffers and raises DS_00105DD8 bits 6 and 7 only. */
    mem_fill(HS_T0, 0, 153u);
    mem_fill(nb, 0x5A, sizeof snb);
    DSD(DS_00104528) = 0x142095u;
    DSB(DS_00105DD8) = 0x01u;
    hiscore_init();
    CHECK(hs_match(HS_T0, hs_orig, 153u), "0x1E824 writes the original's tables");
    CHECK_EQ_INT((int)DSB(DS_00105DD8), 0xC1);
    for (i = 0; i < 0x24u; i++) {
        CHECK_EQ_INT((int)DSB(nb + i), 0x20);
        CHECK_EQ_INT((int)DSB(DS_00104367 + i), 0x20);
        CHECK_EQ_INT((int)DSB(DS_00104367 + 0xA0u + i), 0x20);
    }
    CHECK_EQ_INT((int)DSB(nb + 0x24u), 0x5A);
    CHECK_EQ_INT((int)DSB(DS_00104367 + 0x24u), 0x5A);
    CHECK_EQ_INT((int)DSB(DS_00104367 - 1u), 0x5A);

    /* 0x2DBC4: the decode, the terminator and the return. */
    mem_fill(DS_00105EFC, 0x77, 0x2Cu);
    CHECK_EQ_INT((int)hiscore_read(9u, 0u), (int)DS_00105EFC);
    CHECK_EQ_INT((int)DSD(DS_00105EFC), 100);
    CHECK(memcmp(mem + DS_00105F00, "DUD         ", 13u) == 0, "record 9 reads DUD");
    CHECK_EQ_INT((int)hiscore_read(0u, 1u), (int)DS_00105EFC);
    CHECK_EQ_INT((int)DSD(DS_00105EFC), 500000);
    CHECK(memcmp(mem + DS_00105F00, "TEENY WEENY GAMES", 17u) == 0, "the champion");
    CHECK_EQ_INT((int)DSB(DS_00105F00 + 17u), 0x20);
    CHECK_EQ_INT((int)DSB(DS_00105F00 + 36u), 0);
    CHECK_EQ_INT((int)DSB(DS_00105F00 + 37u), 0x77);
    mem_fill(DS_00105EFC, 0x77, 0x2Cu);
    CHECK_EQ_INT((int)hiscore_read(10u, 0u), 0);
    CHECK_EQ_INT((int)DSD(DS_00105EFC), 0x77777777);

    /* A second 0x1E824 keeps what is there: a non-zero champion and records. */
    DSB(HS_T1 + 3u) = 0x21u;
    DSB(HS_T0 + 12u + 3u) = 0x81u;
    hiscore_init();
    CHECK_EQ_INT((int)DSB(HS_T1 + 3u), 0x21);
    CHECK_EQ_INT((int)DSB(HS_T0 + 12u + 3u), 0x81);

    /* 0x2DCA0: the insert moves the later records down one and drops the last;
     * a space packs 0 and advances, a NUL packs 0 and does not. */
    DSD(HS_SCRATCH) = 0x01020304u;
    memcpy(mem + HS_SCRATCH + 4u, "A BC\0D", 6u);
    DSB(HS_T0 + 107u) = 0x5Cu;                           /* record 8's last byte */
    DSB(DS_00105DD8) = 0u;
    CHECK_EQ_INT((int)hiscore_insert(1u, HS_SCRATCH, 0u), 1);
    CHECK(hs_match(HS_T0, hs_orig, 12u), "record 0 kept");
    CHECK_EQ_INT((int)DSB(HS_T0 + 12u), 1);
    CHECK_EQ_INT((int)DSB(HS_T0 + 15u), 4);
    CHECK_EQ_INT((int)DSW(HS_T0 + 16u), 0x0801);         /* A, space, B */
    CHECK_EQ_INT((int)DSW(HS_T0 + 18u), 0x0003);         /* C, NUL, NUL */
    CHECK_EQ_INT((int)DSW(HS_T0 + 20u), 0);
    CHECK_EQ_INT((int)DSB(HS_T0 + 24u + 3u), 0x81);      /* old record 1, moved */
    CHECK(hs_match(HS_T0 + 36u, hs_orig + 24u, 83u), "records 2..8 moved down");
    CHECK_EQ_INT((int)DSB(HS_T0 + 119u), 0x5C);          /* the move's last byte */
    CHECK_EQ_INT((int)DSB(DS_00105DD8), 0x40);
    CHECK_EQ_INT((int)hiscore_insert(10u, HS_SCRATCH, 0u), 0);
    CHECK_EQ_INT((int)hiscore_insert(0u, HS_SCRATCH, 3u), 0);
    /* The last record: nothing to move (left == size). */
    CHECK_EQ_INT((int)hiscore_insert(9u, HS_SCRATCH, 0u), 1);
    CHECK_EQ_INT((int)DSB(HS_T0 + 111u), 4);
    CHECK_EQ_INT((int)DSB(HS_T0 + 120u + 3u), 0x21);     /* table 1 untouched */
    /* Table 2 (0x105EC8, 3 value bytes, one name word): its dirty bit is
     * table + 6 = 8, bit 0 of DS_00105DD9. */
    {
        u8 s_c8[5];
        memcpy(s_c8, mem + 0x105EC8u, 5u);
        mem_fill(0x105EC8u, 0xEE, 5u);
        DSB(DS_00105DD8) = 0u;
        DSB(DS_00105DD8 + 1u) = 0u;
        DSB(0x105ECDu) = 0x3Cu;
        CHECK_EQ_INT((int)hiscore_insert(0u, HS_SCRATCH, 2u), 1);
        CHECK_EQ_INT((int)DSB(0x105EC8u), 0x02);
        CHECK_EQ_INT((int)DSB(0x105EC9u), 0x03);
        CHECK_EQ_INT((int)DSB(0x105ECAu), 0x04);
        CHECK_EQ_INT((int)DSW(0x105ECBu), 0x0801);
        CHECK_EQ_INT((int)DSB(0x105ECDu), 0x3C);         /* one record, no move */
        CHECK_EQ_INT((int)DSB(DS_00105DD8), 0);
        CHECK_EQ_INT((int)DSB(DS_00105DD8 + 1u), 0x01);
        memcpy(mem + 0x105EC8u, s_c8, 5u);
    }

    /* 0x1E988: field 0x27 >= 2000 and field 0x26 >= 200. */
    config_field_set(0x27u, 2000u);
    config_field_set(0x26u, 200u);
    CHECK_EQ_INT((int)hiscore_audit_reset_due(), 1);
    config_field_set(0x26u, 199u);
    CHECK_EQ_INT((int)hiscore_audit_reset_due(), 0);
    config_field_set(0x26u, 200u);
    config_field_set(0x27u, 1999u);
    CHECK_EQ_INT((int)hiscore_audit_reset_due(), 0);

    /* The audit due with bits 0x2000 and 0x4000 clear: 0x1E988 is not
     * consulted, the normal path keeps the edited table and both fields. */
    config_field_set(0x27u, 2000u);
    DSB(HS_T0 + 12u + 3u) = 0x81u;
    DSD(DS_00104528) = 0x142095u & ~0x2000u;
    hiscore_init();
    CHECK_EQ_INT((int)DSB(HS_T0 + 12u + 3u), 0x81);
    CHECK_EQ_INT((int)config_field_get(0x27u), 2000);
    CHECK_EQ_INT((int)config_field_get(0x26u), 200);

    /* Bit 0x2000 (set in 0x142095) with the audit due: fields 0x27/0x26
     * cleared, the defaults forced over the edited table, the champion kept
     * (bit 0x4000 clear). */
    DSD(DS_00104528) = 0x142095u;
    hiscore_init();
    CHECK(hs_match(HS_T0, hs_orig, 120u), "the forced defaults");
    CHECK_EQ_INT((int)config_field_get(0x27u), 0);
    CHECK_EQ_INT((int)config_field_get(0x26u), 0);
    CHECK_EQ_INT((int)DSB(HS_T1 + 3u), 0x21);
    /* Bit 0x2000 with the audit not due: the normal path keeps the table. */
    DSB(HS_T0 + 3u) = 0x99u;
    hiscore_init();
    CHECK_EQ_INT((int)DSB(HS_T0 + 3u), 0x99);

    /* Bit 0x4000: forced, the bit cleared into field 0x29, the champion
     * rewritten over a non-zero one. */
    DSD(DS_00104528) = 0x142095u | 0x4000u;
    hiscore_init();
    CHECK(hs_match(HS_T0, hs_orig, 153u), "0x4000 rewrites both tables");
    CHECK_EQ_INT((int)DSD(DS_00104528), 0x142095);
    CHECK_EQ_INT((int)config_field_get(0x29u), 0x142095);

    DSD(DS_00104528) = s28;
    memcpy(mem + HS_SCRATCH, sscr, sizeof sscr);
    memcpy(mem + nb, snb, sizeof snb);
    memcpy(mem + DS_00105D88, sreg, sizeof sreg);
}

/* 0x2DDE4 — record §49-R. hiscore_rank_probe against the fresh-CMOS table
 * (the same 0x1E824 recipe check_hiscore uses), on the real, descending
 * hs_orig values (500000, 400000, 350000, 300000, 250000, 200000, 90210,
 * 50000, 20000, 100 for records 0..9 of table 0; the single champion
 * record of table 1 is 500000 too). Proves the strict-win test (a byte
 * greater than the record's, unsigned), the tie-does-not-win test (an
 * exact match keeps scanning rather than stopping), and the off-the-table
 * -1 sentinel, against table 0 and table 1 both, plus the invalid-table
 * (>= 3) immediate -1. */
static void check_hiscore_rank(void)
{
    static u8 sreg[0x1C0];
    memcpy(sreg, mem + DS_00105D88, sizeof sreg);

    mem_fill(HS_T0, 0, 153u);
    DSD(DS_00104528) = 0x142095u;
    hiscore_init();
    CHECK(hs_match(HS_T0, hs_orig, 153u), "check_hiscore_rank's own fresh table");

    /* table 0: strict win at record 0 (no records scanned). */
    CHECK_EQ_INT((int)hiscore_rank_probe(999999u, 0u), 0);
    /* table 0: an exact tie with record 0 does not win; record 1 does. */
    CHECK_EQ_INT((int)hiscore_rank_probe(500000u, 0u), 1);
    /* table 0: strictly between records 0 and 1. */
    CHECK_EQ_INT((int)hiscore_rank_probe(450000u, 0u), 1);
    /* table 0: a tie with record 6, then a strict win over record 7. */
    CHECK_EQ_INT((int)hiscore_rank_probe(90210u, 0u), 7);
    /* table 0: at or below the last record (9, value 100), no record is
     * ever beaten and the probe returns -1. Below record 9's own value a
     * tie with record 9 first (0 < 100 does not win either), then the
     * table's own byte budget runs out (0x2DE71..0x2DE7C): the loop's
     * `left` accounting is table 0's own 120-byte budget, so this holds
     * regardless of what memory happens to sit past table 0's own bytes. */
    CHECK_EQ_INT((int)hiscore_rank_probe(100u, 0u), (int)0xFFFFFFFFu);
    CHECK_EQ_INT((int)hiscore_rank_probe(99u, 0u), (int)0xFFFFFFFFu);
    CHECK_EQ_INT((int)hiscore_rank_probe(0u, 0u), (int)0xFFFFFFFFu);

    /* table 1 (the one-record champion table, also 500000): a strict win
     * resolves in the first comparison, with no record-advance loop run at
     * all — the only table-1/2 case the real callers' own table = 0
     * argument (0x1ECC8/0x1EC38, record §49-R) makes relevant to reason
     * about; a losing or tying probe against table 1 or 2 is not tested
     * here since the raw's own record-advance loop then walks past that
     * table's own tiny byte budget into memory owned by the next table,
     * behaviour no shipped caller ever exercises. */
    CHECK_EQ_INT((int)hiscore_rank_probe(999999u, 1u), 0);

    /* table >= 3: 0x2DB58 itself returns 0 (rec 0 >= count 0), -1
     * immediately regardless of value. */
    CHECK_EQ_INT((int)hiscore_rank_probe(999999u, 3u), (int)0xFFFFFFFFu);

    memcpy(mem + DS_00105D88, sreg, sizeof sreg);
}

/* ---- test_config.c ---- */

/* The descriptor table lives in the loaded image (obj-0 VA 0x2D300). These tests
 * run after test_le() has loaded PRAGE.EXE into mem[]. */
int test_config(void)
{
    /* The table must be the real one; a missing load would otherwise read zeros
     * and silently pass. */
    CHECK_EQ_INT((int)DSD(DS_0002D300 + 0x29u * 4u), 0x1D980);

    /* Save the config region: the tests below seed and mutate it. */
    u8 saved[0x100];
    for (u32 i = 0; i < 0x100u; i++) saved[i] = DSB(DS_00105D88 + i);

    /* Field 0x29 has bitpos 102, width 8, no trailing byte: the getter reads
     * DS_00105DE1 indices 54,53,52,51, giving b54<<24 | b53<<16 | b52<<8 | b51. */
    for (u32 i = 0; i < 0x70u; i++)
        DSB(DS_00105DE1 + i) = (u8)i;
    CHECK_EQ_INT((int)config_field_get(0x29u), 0x36353433);

    /* Field 0x2A has bitpos 110, width 1: the low nibble of byte 55. */
    CHECK_EQ_INT((int)config_field_get(0x2Au), 55u & 0xFu);

    /* Field 0x35 has bitpos 131, width 4 (odd start). With a ramp, the value is
     * ((67 & 0xf) << 8 | 66) << 4 | (65 >> 4) = 0x3424. */
    CHECK_EQ_INT((int)config_field_get(0x35u), 0x3424);

    /* Field 0x00 has bitpos 0, width 2 and trailing index 1: the value is byte 0
     * (the even-start walk reads it whole) shifted up 8 and OR'd with
     * DS_00105DAF + 1. */
    DSB(DS_00105DAF + 1u) = 0xA1u;
    CHECK_EQ_INT((int)config_field_get(0x00u), 0xA1);

    /* Out-of-range fields return -1. */
    CHECK_EQ_INT((int)config_field_get(0x3Fu), -1);
    CHECK_EQ_INT((int)config_field_get(0x100u), -1);

    for (u32 i = 0; i < 0x100u; i++) DSB(DS_00105D88 + i) = saved[i];

    {
        /* set -> get round-trips for even/odd starts, widths 8 / 4 / 3 / 1 / 2 and
         * the trailing-byte case. Odd widths (3, and the synthesized 5 and 7) are
         * the shape where the getter's cursor-parity seed ((bitpos+width)&1) and the
         * setter's bitpos-parity branch disagree. */
        static const u32 saved_lo = 0x00105DAFu, saved_hi = 0x00105DE1u;
        u8 lo[0x20], hi[0x70];
        for (u32 i = 0; i < 0x20u; i++) lo[i] = DSB(saved_lo + i);
        for (u32 i = 0; i < 0x70u; i++) hi[i] = DSB(saved_hi + i);
        u8 saved_flags = DSB(DS_00105DD8);

        config_field_set(0x29u, 0x0000ABCDu);
        CHECK_EQ_INT((int)config_field_get(0x29u), 0xABCD);

        config_field_set(0x35u, 0x0000000Au);
        CHECK_EQ_INT((int)config_field_get(0x35u), 0x000A);

        config_field_set(0x00u, 0x0000080Fu);
        CHECK_EQ_INT((int)config_field_get(0x00u), 0x80F);

        config_field_set(0x2Au, 0x00000003u);
        CHECK_EQ_INT((int)config_field_get(0x2Au), 3);

        /* Width 3, both parities: field 0x03 has bitpos 6 (getter odd-seed, setter
         * even) and field 0x04 has bitpos 9 (getter even, setter odd). 11 bits. */
        config_field_set(0x03u, 0x000005A3u);
        CHECK_EQ_INT((int)config_field_get(0x03u), 0x5A3);
        config_field_set(0x04u, 0x000006B7u);
        CHECK_EQ_INT((int)config_field_get(0x04u), 0x6B7);

        /* The setter's odd branch ORs the field's low nibble into the byte at
         * DS_00105DE1 + (bitpos>>1) and must keep that byte's low nibble. Field
         * 0x35 has bitpos 131, so it packs into DS_00105DE1 + 65. */
        DSB(DS_00105DE1 + 65u) = 0x0Bu;
        config_field_set(0x35u, 0x0000000Au);
        CHECK_EQ_INT((int)(DSB(DS_00105DE1 + 65u) & 0x0Fu), 0x0B);

        /* Widths 5 and 7 do not occur in the shipped table. Synthesize descriptors
         * (the descriptor is just data; the same walk arithmetic runs) in unused
         * slots 0x3C/0x3D and restore the slots after. */
        u8 desc_saved[8];
        for (u32 i = 0; i < 8u; i++) desc_saved[i] = DSB(DS_0002D300 + 0xF0u + i);
        DSD(DS_0002D300 + 0x3Cu * 4u) = 0x00010500u;   /* width 5, bitpos 20 */
        DSD(DS_0002D300 + 0x3Du * 4u) = 0x00018A00u;   /* width 7, bitpos 40 */
        config_field_set(0x3Cu, 0x0000001Bu);
        CHECK_EQ_INT((int)config_field_get(0x3Cu), 0x1B);
        config_field_set(0x3Du, 0x0000006Du);
        CHECK_EQ_INT((int)config_field_get(0x3Du), 0x6D);
        for (u32 i = 0; i < 8u; i++) DSB(DS_0002D300 + 0xF0u + i) = desc_saved[i];

        /* The setter raises DS_00105DD8: |1 for a trailing byte, |6 always. */
        DSB(DS_00105DD8) = 0;
        config_field_set(0x00u, 0x10u);
        CHECK_EQ_INT((int)DSB(DS_00105DD8), 0x7);
        DSB(DS_00105DD8) = 0;
        config_field_set(0x29u, 0u);
        CHECK_EQ_INT((int)DSB(DS_00105DD8), 0x6);

        CHECK_EQ_INT((int)config_field_set(0x3Fu, 0u), -1);
        CHECK_EQ_INT((int)config_field_set(0x100u, 0u), -1);

        for (u32 i = 0; i < 0x20u; i++) DSB(saved_lo + i) = lo[i];
        for (u32 i = 0; i < 0x70u; i++) DSB(saved_hi + i) = hi[i];
        DSB(DS_00105DD8) = saved_flags;
    }

    {
        /* A zeroed config region fails the magic and the defaults path runs:
         * the magic is rewritten, DS_00105DA7 is set, and the four fields the
         * defaults writer touches carry the values from the obj-1 default table. */
        u8 saved[0x100];
        for (u32 i = 0; i < 0x100u; i++) saved[i] = DSB(DS_00105D88 + i);
        for (u32 i = 0; i < 0x100u; i++) DSB(DS_00105D88 + i) = 0;

        config_validate();
        CHECK_EQ_INT((int)DSD(DS_00105E30), (int)0x9C94D2C4u);
        CHECK_EQ_INT((int)DSB(DS_00105DA7), 1);

        /* The mismatch path raises the flag byte with |6 then |1; the setter may
         * add |1 again for a trailing byte, so the byte is 0x7 either way. */
        CHECK_EQ_INT((int)DSB(DS_00105DD8), 0x7);

        /* Field 0x35 and 0x37 default to 0xA0; field 0x2A keeps the top six bits
         * and gets 3 in the low two. */
        CHECK_EQ_INT((int)config_field_get(0x35u), 0xA0);
        CHECK_EQ_INT((int)config_field_get(0x37u), 0xA0);
        CHECK_EQ_INT((int)(config_field_get(0x2Au) & 0x3u), 3);

        /* Field 0x29's default is the menu table's '*' selection, walked from the
         * obj-1 table at 0xA2EB4. The shipped table must be nonzero or the
         * equality below is vacuous. */
        CHECK(config_menu_default_bits(0xA2EB4u) != 0u,
              "shipped menu table gives a nonzero default");
        CHECK_EQ_INT((int)config_field_get(0x29u),
                     (int)config_menu_default_bits(0xA2EB4u));

        for (u32 i = 0; i < 0x100u; i++) DSB(DS_00105D88 + i) = saved[i];
    }

    {
        /* The credit layer: free play, the credit counter and the suppression
         * flag DS_00104B1F. */
        u32 credits = DSD(DS_00105C00);
        u8 free_play = DSB(DS_00105D60);
        u8 suppress = DSB(DS_00104B1F);

        DSB(DS_00105D60) = 0;
        DSB(DS_00104B1F) = 0;
        DSD(DS_00105C00) = 5u;

        CHECK_EQ_INT((int)config_not_free_play(), 1);
        CHECK_EQ_INT((int)config_has_credit(), 1);
        CHECK_EQ_INT((int)config_credit_ready(), 1);

        /* 0x2CA48: takes one credit and reports success. */
        CHECK_EQ_INT((int)config_credit_take(), 1);
        CHECK_EQ_INT((int)DSD(DS_00105C00), 4);

        /* 0x2CA7C: the n > credits guard leaves the counter alone. */
        CHECK_EQ_INT((int)config_credit_spend(5u), 0);
        CHECK_EQ_INT((int)DSD(DS_00105C00), 4);
        CHECK_EQ_INT((int)config_credit_spend(4u), 1);
        CHECK_EQ_INT((int)DSD(DS_00105C00), 0);

        /* No credits and no free play: both predicates and the take fail. */
        CHECK_EQ_INT((int)config_has_credit(), 0);
        CHECK_EQ_INT((int)config_credit_take(), 0);
        CHECK_EQ_INT((int)config_credit_spend(1u), 0);

        /* DS_00105D60 (free play) short-circuits to success, counter untouched. */
        DSB(DS_00105D60) = 1;
        CHECK_EQ_INT((int)config_not_free_play(), 0);
        DSD(DS_00105C00) = 2u;
        CHECK_EQ_INT((int)config_credit_take(), 1);
        CHECK_EQ_INT((int)DSD(DS_00105C00), 2);
        CHECK_EQ_INT((int)config_credit_spend(9u), 1);
        CHECK_EQ_INT((int)DSD(DS_00105C00), 2);
        DSB(DS_00105D60) = 0;

        /* DS_00104B1F suppresses the debit while still reporting success. */
        DSD(DS_00105C00) = 3u;
        DSB(DS_00104B1F) = 1;
        CHECK_EQ_INT((int)config_credit_take(), 1);
        CHECK_EQ_INT((int)DSD(DS_00105C00), 3);
        CHECK_EQ_INT((int)config_credit_spend(1u), 1);
        CHECK_EQ_INT((int)DSD(DS_00105C00), 3);
        DSB(DS_00104B1F) = 0;

        /* 0x2C06C/0x2BF00: the overlay row writers. */
        config_set_credit_row(7u);
        CHECK_EQ_INT((int)DSB(DS_00105C05), 7);
        config_set_credit_row_init();
        CHECK_EQ_INT((int)DSB(DS_00105C05), 0x1D);

        DSD(DS_00105C00) = credits;
        DSB(DS_00105D60) = free_play;
        DSB(DS_00104B1F) = suppress;
    }
    check_hiscore();
    check_hiscore_rank();

    /* Record §K2.3 (2026-09-29-k2-k5-derivations.md): 0x2D4B4's loop jumps
     * back to its `inc eax` (0x2D4C1 -> 0x2D4BA) and returns EAX, so the result
     * is n + r + 1 for the smallest r with 2^r >= n + r + 1, not a power of
     * two. Every caller passes 0x26 (0x2D528, 0x2D8BD, 0x2DB0B, 0x2DB1F). */
    CHECK_EQ_INT((int)config_codeword_len(0u), 1);
    CHECK_EQ_INT((int)config_codeword_len(1u), 4);
    CHECK_EQ_INT((int)config_codeword_len(3u), 7);
    CHECK_EQ_INT((int)config_codeword_len(4u), 8);
    CHECK_EQ_INT((int)config_codeword_len(0x26u), 0x2D);
    return 0;
}

/* ---- test_anim.c ---- */

/* port/tests/test_anim.c — the animation-stream interpreter (Task 7).
 * Direct tests of 0x29F34/0x29DB8/0x2A408 plus the spawn and frame-timer walks
 * that consume them, and the Task 1 opcode-8 pin mirror. */









/* 0x29F34: the variable reader. Format reference F: < 0x40 indexes the ring
 * through rec+0x51; 0x40..0x45 are the record's own fields (0x40..0x43 and 0x45
 * sign-extended, 0x44 the pset-slot word); 0x46..0x4B the parent and 0x4C..0x51
 * the child. 0x29DB8 is the mirror. */
static void check_read_write_var(void)
{
    u32 rec = tf_anim_alloc_record();
    CHECK(rec != 0, "reader record");
    if (rec == 0) return;

    for (u32 i = 0; i < 0x40; i++) DSW(DS_00105B4C + i * 2u) = (u16)(0x1000u + i);
    DSB(rec + 0x51) = 0x10;
    CHECK_EQ_INT((int)anim_read_var(rec, 0x00),
                 (int)DSW(DS_00105B4C + ((0x00u + 0x10u) & 0x3fu) * 2u));
    CHECK_EQ_INT((int)anim_read_var(rec, 0x30),
                 (int)DSW(DS_00105B4C + ((0x30u + 0x10u) & 0x3fu) * 2u));

    DSB(rec + 0x52) = 0x80; CHECK_EQ_INT((int)anim_read_var(rec, 0x40), 0xff80);
    DSB(rec + 0x53) = 0x7f; CHECK_EQ_INT((int)anim_read_var(rec, 0x41), 0x007f);
    DSB(rec + 0x54) = 0xff; CHECK_EQ_INT((int)anim_read_var(rec, 0x42), 0xffff);
    DSB(rec + 0x55) = 0x01; CHECK_EQ_INT((int)anim_read_var(rec, 0x43), 0x0001);
    DSW(rec + 0x56) = 0xbeef; CHECK_EQ_INT((int)anim_read_var(rec, 0x44), 0xbeef);
    DSB(rec + 0x58) = 0x90; CHECK_EQ_INT((int)anim_read_var(rec, 0x45), 0xff90);

    u32 parent = actor_alloc(0);
    CHECK(parent != 0, "parent record");
    if (parent != 0) {
        DSB(rec + 0x4a) = (u8)actor_index(parent);
        DSB(parent + 0x52) = 0x85; DSB(parent + 0x53) = 0x02;
        DSB(parent + 0x54) = 0xfe; DSB(parent + 0x55) = 0x01;
        DSW(parent + 0x56) = 0x1234; DSB(parent + 0x58) = 0x80;
        CHECK_EQ_INT((int)anim_read_var(rec, 0x46), 0xff85);
        CHECK_EQ_INT((int)anim_read_var(rec, 0x47), 0x0002);
        CHECK_EQ_INT((int)anim_read_var(rec, 0x48), 0xfffe);
        CHECK_EQ_INT((int)anim_read_var(rec, 0x49), 0x0001);
        CHECK_EQ_INT((int)anim_read_var(rec, 0x4a), 0x1234);
        CHECK_EQ_INT((int)anim_read_var(rec, 0x4b), 0xff80);
    }

    u32 child = actor_alloc(0);
    CHECK(child != 0, "child record");
    if (child != 0) {
        DSB(rec + 0x4b) = (u8)actor_index(child);
        DSB(child + 0x52) = 0x90; DSB(child + 0x53) = 0x03;
        DSB(child + 0x54) = 0xfd; DSB(child + 0x55) = 0x02;
        DSW(child + 0x56) = 0x4321; DSB(child + 0x58) = 0x7f;
        CHECK_EQ_INT((int)anim_read_var(rec, 0x4c), 0xff90);
        CHECK_EQ_INT((int)anim_read_var(rec, 0x4d), 0x0003);
        CHECK_EQ_INT((int)anim_read_var(rec, 0x4e), 0xfffd);
        CHECK_EQ_INT((int)anim_read_var(rec, 0x4f), 0x0002);
        CHECK_EQ_INT((int)anim_read_var(rec, 0x50), 0x4321);
        CHECK_EQ_INT((int)anim_read_var(rec, 0x51), 0x007f);
    }

    anim_write_var(rec, 0x40, 0x1234);
    CHECK_EQ_INT((int)DSB(rec + 0x52), 0x34);
    anim_write_var(rec, 0x44, 0x1234);
    CHECK_EQ_INT((int)DSW(rec + 0x56), 0x0034);
    anim_write_var(rec, 0x05, 0xabcd);
    CHECK_EQ_INT((int)DSW(DS_00105B4C + ((0x05u + 0x10u) & 0x3fu) * 2u), 0xabcd);
    /* 0x29E6A: the parent/child byte stores take the operand byte at
     * DS_00105BE8, not the passed value; 0x4A/0x4B take the value. */
    if (parent != 0) {
        DSW(DS_00105BE8) = 0x11;
        anim_write_var(rec, 0x46, 0x99);
        CHECK_EQ_INT((int)DSB(parent + 0x52), 0x11);
        anim_write_var(rec, 0x4a, 0x55);
        CHECK_EQ_INT((int)DSW(parent + 0x56), 0x0055);
    }
    /* 0x7F is out of every selector range; the switch falls through without a
     * store. The ring entry it would collide with if mishandled is checked
     * unchanged. */
    DSW(DS_00105B4C + 0x0f * 2u) = 0x1234;
    anim_write_var(rec, 0x7f, 0xffff);
    CHECK_EQ_INT((int)DSW(DS_00105B4C + 0x0f * 2u), 0x1234);
}

/* 0x2A408: the literal reader, the two 0xD00 computed forms, the hflip fold,
 * and the real second title object's stream (descriptor 0x9AC94, pointer
 * 0x0E897A, which begins `40 CD` = word 0xCD40). */
static void check_next_sprite_id(void)
{
    u32 rec = tf_anim_alloc_record();
    CHECK(rec != 0, "id record");
    if (rec == 0) return;
    u16 *s = (u16 *)(mem + ANIM_SCRATCH);
    u32 pset = actor_pset(rec);

    /* literal: word & 0x8000 clear, returned as word & 0x7FFF. The disassembly
     * (0x2A4AF `mov ecx,eax`) does not move the cursor for this form. */
    DSW(rec + 0x28) = 0;
    s[0] = 0x1234; DSD(rec + 8) = ANIM_SCRATCH;
    CHECK_EQ_INT((int)anim_next_sprite_id(rec, pset), 0x1234);
    CHECK_EQ_INT((int)DSD(rec + 8), (int)ANIM_SCRATCH);

    /* hflip fold: the returned bit 0x8000 is (id & 0x8000) XOR (rec+0x28>>8 &
     * 0x40). A literal has its high bit clear, so setting the record flip sets
     * the returned bit. */
    s[0] = 0x1234; DSW(rec + 0x28) = 0x0000;
    CHECK_EQ_INT((int)anim_next_sprite_id(rec, pset), 0x1234);
    DSW(rec + 0x28) = 0x4000;
    CHECK_EQ_INT((int)anim_next_sprite_id(rec, pset), 0x9234);
    /* A computed id can carry the high bit: 0xCD40 + 0x8000 -> fold clears it
     * when the record flip is set. */
    DSB(rec + 0x52) = 0x00;
    s[0] = 0xcd40; s[1] = 0x8000; DSD(rec + 8) = ANIM_SCRATCH;
    DSW(rec + 0x28) = 0x4000;
    CHECK_EQ_INT((int)anim_next_sprite_id(rec, pset), 0x0000);
    DSW(rec + 0x28) = 0x0000;
    DSD(rec + 8) = ANIM_SCRATCH;
    CHECK_EQ_INT((int)anim_next_sprite_id(rec, pset), 0x8000);

    /* 0xD00 one-extra-word form: 0xCD40, (word >> 8 & 0x60) == 0x40, consumes
     * the operand word and returns read_var(rec, word & 0x7F) + that word. */
    DSW(rec + 0x28) = 0;
    DSB(rec + 0x52) = 0x10;
    s[0] = 0xcd40; s[1] = 0x0100; DSD(rec + 8) = ANIM_SCRATCH;
    CHECK_EQ_INT((int)anim_next_sprite_id(rec, pset), 0x0110);
    CHECK_EQ_INT((int)DSD(rec + 8), (int)(ANIM_SCRATCH + 2));

    /* 0xD00 two-extra-word form: 0x8D00, (word >> 8 & 0x60) == 0. Consumes the
     * table pointer dword and returns table[read_var(rec, word & 0x7F)]. */
    DSW(DS_00105B4C) = 2; DSB(rec + 0x51) = 0;
    u32 tab = ANIM_SCRATCH + 0x200;
    s[0] = 0x8d00; *(u32 *)(mem + ANIM_SCRATCH + 2) = tab;
    *(u16 *)(mem + tab + 2 * 2) = 0xbeef;
    DSD(rec + 8) = ANIM_SCRATCH;
    CHECK_EQ_INT((int)anim_next_sprite_id(rec, pset), 0xbeef);
    CHECK_EQ_INT((int)DSD(rec + 8), (int)(ANIM_SCRATCH + 4));

    /* A 0x8000 word that is not 0xD00 keeps the current pset id (0x2A4A7
     * `mov cx,[edx]; and ch,0x7f`). The brief's 0xD100/0xD200 literals are not
     * 0xD00 words: (0xD100 & 0x1F00) == 0x0100. */
    DSW(pset) = 0x1234; DSW(rec + 0x28) = 0;
    s[0] = 0xd100; DSD(rec + 8) = ANIM_SCRATCH;
    CHECK_EQ_INT((int)anim_next_sprite_id(rec, pset), 0x1234);
    CHECK_EQ_INT((int)DSD(rec + 8), (int)ANIM_SCRATCH);

    /* 0x2A408's flag-8 arm reads the id from the cursor itself. */
    DSW(rec + 0x28) = 0x0800; DSD(rec + 8) = 0x2222;
    CHECK_EQ_INT((int)anim_next_sprite_id(rec, pset), 0x2222);
}

/* The real asset: descriptor 0x9AC94's stream starts `40 CD 3D 02`, i.e.
 * word 0xCD40 (0xD00, one extra word, selector 0x40 -> rec+0x52) followed by
 * 0x023D. This closes the spec's open item on 0x29F34's enumeration with a
 * shipped asset rather than a synthetic one. */
static void check_real_stream(void)
{
    actors_reset();
    DSB(DS_00104B24) = 0;
    DSB(DS_00104B26) = 0;
    const u32 *desc = (const u32 *)(mem + 0x9AC94u);
    u32 rec = actor_spawn(desc, 0, 0xe4u, 0, 0);
    CHECK(rec != 0, "real-stream record");
    if (rec == 0) return;

    DSD(rec + 8) = 0x0e897au;          /* the stream head */
    DSB(rec + 0x52) = 0x10;            /* the 0x40 variable read */
    DSW(rec + 0x28) &= (u16)~0x4000u;
    u32 pset = actor_pset(rec);
    u32 id = anim_next_sprite_id(rec, pset);
    CHECK_EQ_INT((int)id, 0x024d);     /* 0x023D + rec+0x52 */
    CHECK_EQ_INT((int)DSD(rec + 8), 0x0e897cu);
}

/* The spawn/frame-timer walks: the literal word is left at the cursor by
 * 0x2A408, and the next walk advances the cursor by one word and loads it. */
static void check_walk(void)
{
    u16 *s = (u16 *)(mem + ANIM_SCRATCH);
    s[0] = 0x2c11; s[1] = 0x2c12;
    u32 rec = tf_anim_spawn_stream();
    CHECK(rec != 0, "walk record");
    if (rec == 0) return;
    u32 pset = actor_pset(rec);
    CHECK_EQ_INT((int)DSW(pset), 0x2c11);
    CHECK_EQ_INT((int)DSD(rec + 8), (int)ANIM_SCRATCH);

    DSD(rec + 0x20) = 0;                       /* expired timer */
    DSD(rec + 0x24) = 0x3f800000u;             /* 1.0f: frame_timer runs */
    DSW(rec + 0x2a) = 0;
    DSW(rec + 0x28) &= (u16)~0x0810u;
    actors_update();
    CHECK_EQ_INT((int)DSW(pset), 0x2c12);
    CHECK_EQ_INT((int)DSD(rec + 8), (int)(ANIM_SCRATCH + 2));
}

/* Opcode 8 is the in-window RNG consumer; the Task 1 pin replaced its 0x5D7DC
 * call with `mov eax, 0`, mirrored by actors_pin_anim_tick_zero. Command word
 * 0x88FF is opcode 8 with operand 0xFFFF. */
static void check_opcode8_pin(void)
{
    u16 *s = (u16 *)(mem + ANIM_SCRATCH);
    s[0] = 0x88ff; s[1] = 0;
    actors_pin_anim_tick_zero(1);
    rng_seed(0xabcd);
    u32 rec = tf_anim_spawn_stream();
    actors_pin_anim_tick_zero(0);
    CHECK(rec != 0, "opcode-8 record");
    if (rec == 0) return;
    union { float f; u32 u; } fu;
    fu.u = DSD(rec + 0x20);
    CHECK_EQ_INT((int)fu.f, 0);                /* pinned draw is exactly 0 */
    u32 pset = actor_pset(rec);
    CHECK_EQ_INT((int)DSW(pset), 0x01e1);      /* status 2 -> engine id 0x1E1 */
}

/* 0x12720: the animation opcode 0x11 target the globe's first presentation
 * stream reaches. The word at 0xE89A8 is 0xD100 — opcode 0x11, mode 0x4000 —
 * so anim_operand loads the code pointer 0x12720 into DS_00105BD4 and the
 * dispatcher's indirect call reaches it. It spawns the globe's fourth layer as
 * a child of DS_000F0A58: descriptor 0x9AC80 (stream 0x0E89F6, frame hold 7,
 * layer 0xE2). Driven from the real descriptor 0x9AC44 with the cursor at the
 * op-0x11 word, so the walk's own operand decode is what supplies the target.
 * The spawned record is identified as the one the active list did not hold
 * before the call: with the target skipped the list is unchanged and the
 * assertions below cannot pass on a record the test itself left behind. */
static void check_globe_opcode11_spawn(void)
{
    actors_reset();
    DSB(DS_00104B24) = 0;
    DSB(DS_00104B26) = 0;
    u32 parent = actor_spawn((const u32 *)(mem + 0x9AC44u),
                             0x2A00u, 0xE0u, 0x5A00u, 0u);
    CHECK(parent != 0, "globe first-layer record");
    if (parent == 0) return;
    u32 saved_a58 = DSD(DS_000F0A58);
    DSW(parent + 0x56) = 7;             /* a non-zero slot: the child's +0x4A is the parent's slot */
    DSD(DS_000F0A58) = parent;          /* 0x12720 reads this global */
    DSD(parent + 8) = 0x0E89A6u;        /* the walk reads the op-0x11 word at 0xE89A8 */
    DSD(parent + 0x20) = 0;             /* expired frame timer */
    DSD(parent + 0x24) = 0x3f800000u;   /* 1.0f: frame_timer runs */
    DSW(parent + 0x2a) = 0;
    DSW(parent + 0x28) &= (u16)~0x0810u;

    u32 before[64];
    int nbefore = 0;
    for (u32 r = actor_list_head(); r != 0 && nbefore < 64; r = actor_next(r))
        before[nbefore++] = r;

    actor_sync(parent);

    /* The record the walk spawned is the one the active list did not hold. */
    u32 child = 0;
    for (u32 r = actor_list_head(); r != 0; r = actor_next(r)) {
        int seen = 0;
        for (int i = 0; i < nbefore; i++) if (before[i] == r) { seen = 1; break; }
        if (!seen) child = r;
    }
    CHECK(child != 0, "the opcode-0x11 target spawned the fourth layer");
    if (child != 0) {
        union { float f; u32 u; } fu;
        CHECK_EQ_INT((int)DSB(child + 0x49), 0xE2);            /* layer 0xE2 */
        CHECK_EQ_INT((int)DSB(child + 0x4a), 7);               /* a5 = the parent slot */
        /* The 0x400 in a5 selects the parent-relative spawn: the descriptor's
         * own u16@8 (0x2000) plus the 0x400 parent bit. */
        CHECK_EQ_INT((int)DSW(child + 0x28), 0x2400);
        CHECK_EQ_INT((int)DSW(actor_pset(child)), 0x0235);     /* stream 0x0E89F6's first id */
        fu.u = DSD(child + 0x24);
        CHECK_EQ_INT((int)fu.f, 7);                            /* descriptor hold 7 */
        fu.u = DSD(child + 0x20);
        CHECK_EQ_INT((int)fu.f, 6);                            /* 7 - 1 */
    }
    DSD(DS_000F0A58) = saved_a58;
}

/* 0x2BCF4/0x2BC30: the stream-entry helpers also load the first id through
 * 0x2A408. 0x2BCF4 only re-points the cursor; 0x2BC30 also resets the cache and
 * the frame timer and pre-walks the commands. */
static void check_entry_helpers(void)
{
    u16 *s = (u16 *)(mem + ANIM_SCRATCH);
    u32 rec = tf_anim_alloc_record();
    CHECK(rec != 0, "entry record");
    if (rec == 0) return;
    DSW(rec + 0x56) = 0;
    u32 pset = actor_pset(rec);

    s[0] = 0x2c21;
    DSW(rec + 0x28) = 0x0014;              /* bits 0x2BCF4 clears */
    DSB(rec + 0x2b) = 0xff;
    actors_anim_seek(rec, ANIM_SCRATCH);
    CHECK_EQ_INT((int)DSW(pset), 0x2c21);
    CHECK_EQ_INT((int)DSD(rec + 8), (int)ANIM_SCRATCH);
    CHECK_EQ_INT((int)(DSW(rec + 0x28) & 0x0014), 0);

    s[0] = 0x2c22;
    DSB(rec + 0x50) = 0x55;
    DSB(rec + 0x61) = 0x66;
    DSB(rec + 0x2b) = 0xff;
    actors_anim_begin(rec, ANIM_SCRATCH, 0x40e00000u);   /* IEEE-754 7.0f bits */
    CHECK_EQ_INT((int)DSW(pset), 0x2c22);
    CHECK_EQ_INT((int)DSD(rec + 8), (int)ANIM_SCRATCH);
    CHECK_EQ_INT((int)DSB(rec + 0x50), 0);
    CHECK_EQ_INT((int)DSB(rec + 0x61), 0);
    CHECK_EQ_INT((int)(DSB(rec + 0x2b) & 0x04), 0);
    /* 0x2BC8B stores the raw dword argument, not a converted integer. */
    CHECK_EQ_INT((int)DSD(rec + 0x20), 0x40e00000);
    CHECK_EQ_INT((int)DSD(rec + 0x24), 0x40e00000);
}

/* The dispatcher driven through a stream (0x2BC30's pre-walk, EBX=0), covering
 * the opcodes the title streams actually reach: 0x12, 0x18 (both branches),
 * 0x00 and 0x01. The 0xCD40/0x8D00 cases above exercise 0x2A408, not the
 * dispatcher. */
static void check_dispatcher_streams(void)
{
    u16 *s = (u16 *)(mem + ANIM_SCRATCH);

    /* 0x12: operand 0x14 -> rec+0x2E = 0x140, rec+0x4E = 1, then literal id. */
    u32 rec = tf_anim_alloc_record();
    CHECK(rec != 0, "dispatch 0x12 record");
    if (rec != 0) {
        DSW(rec + 0x56) = 0;
        DSW(rec + 0x2e) = 0;
        s[0] = 0x9214; s[1] = 0x2c11;
        actors_anim_begin(rec, ANIM_SCRATCH, 0x40e00000u);
        CHECK_EQ_INT((int)DSW(rec + 0x2e), 0x0140);
        CHECK_EQ_INT((int)DSB(rec + 0x4e), 1);
        CHECK_EQ_INT((int)DSW(actor_pset(rec)), 0x2c11);
    }

    /* 0x18 fall-through: bound 0, so the counter (rec+0x52) increments once and
     * the cursor advances to the literal id. */
    rec = tf_anim_alloc_record();
    CHECK(rec != 0, "dispatch 0x18 fallthrough record");
    if (rec != 0) {
        DSW(rec + 0x56) = 0;
        DSB(rec + 0x52) = 0;
        s[0] = 0xb840; s[1] = 0x0000; s[2] = 0x0000; s[3] = 0x0000;
        s[4] = 0x2c11;                         /* cursor+8 after fall-through */
        actors_anim_begin(rec, ANIM_SCRATCH, 0x40e00000u);
        CHECK_EQ_INT((int)DSB(rec + 0x52), 1);
        CHECK_EQ_INT((int)DSW(actor_pset(rec)), 0x2c11);
    }

    /* 0x18 jump: bound 3 and the table dword points back at the stream, so the
     * counter counts up to the bound before falling through. */
    rec = tf_anim_alloc_record();
    CHECK(rec != 0, "dispatch 0x18 jump record");
    if (rec != 0) {
        DSW(rec + 0x56) = 0;
        DSB(rec + 0x52) = 0;
        s[0] = 0xb840; s[1] = 0x0003;
        *(u32 *)(mem + ANIM_SCRATCH + 4) = ANIM_SCRATCH;
        s[4] = 0x2c11;                         /* cursor+8 after fall-through */
        actors_anim_begin(rec, ANIM_SCRATCH, 0x40e00000u);
        CHECK_EQ_INT((int)DSB(rec + 0x52), 3);
        CHECK_EQ_INT((int)DSW(actor_pset(rec)), 0x2c11);
    }

    /* 0x00: set_dead (rec+0x28 0x08) + frame reset, returns 2. */
    rec = tf_anim_alloc_record();
    CHECK(rec != 0, "dispatch 0x00 record");
    if (rec != 0) {
        DSW(rec + 0x56) = 0;
        DSB(rec + 0x28) &= (u8)~0x08u;
        s[0] = 0x8000; s[1] = 0x2c11;
        actors_anim_begin(rec, ANIM_SCRATCH, 0x40e00000u);
        CHECK_EQ_INT((int)(DSB(rec + 0x28) & 0x08), 0x08);
        CHECK_EQ_INT((int)DSD(rec + 0x24), 0);
        CHECK_EQ_INT((int)DSD(rec + 0x20), 0);
    }

    /* 0x01: frame reset, returns 2. */
    rec = tf_anim_alloc_record();
    CHECK(rec != 0, "dispatch 0x01 record");
    if (rec != 0) {
        DSW(rec + 0x56) = 0;
        s[0] = 0x8100; s[1] = 0x2c11;
        actors_anim_begin(rec, ANIM_SCRATCH, 0x40e00000u);
        CHECK_EQ_INT((int)DSD(rec + 0x24), 0);
        CHECK_EQ_INT((int)DSD(rec + 0x20), 0);
    }
}

/* 0x37A58: the fighters' idle-animation tick, an opcode-0x10 target. The
 * character streams (the T-rex 0xE6DD2, the raptor 0xD2136) carry the word
 * 0xD000 (opcode 0x10, mode 0x4000), so anim_operand loads the dword 0x37A58
 * into DS_00105BD4 and the dispatcher's indirect call reaches it. It advances
 * rec+0x52 — the offset the 0xCD40 form selects the sprite id with — and on
 * rec+0x4C's expiry draws rng(2), flips rec+0x58 between +1 and 0xFF and
 * reseeds rec+0x4C to 3 * (rec+0x4D / 3). Before it was registered anim_indirect
 * returned NULL and the variable froze, so the idle animation held its first
 * sprite. */
static void check_idle_tick_37a58(void)
{
    u16 *s = (u16 *)(mem + ANIM_SCRATCH);
    u32 rec = tf_anim_alloc_record();
    CHECK(rec != 0, "idle-tick record");
    if (rec == 0) return;
    DSW(rec + 0x56) = 0;
    DSW(DS_00104B00) = 3;                /* not 6: the rng(3) arm is skipped */
    s[0] = 0x2c10;
    s[1] = 0xd000;                       /* opcode 0x10, mode 0x4000 */
    *(u32 *)(mem + ANIM_SCRATCH + 4) = 0x37a58u;
    s[4] = 0x2c11;                       /* the literal id after the command */
    DSW(rec + 0x28) = 0;
    DSW(rec + 0x2a) = 0;
    DSB(rec + 0x4b) = 0;
    DSB(rec + 0x4f) = 0;
    DSB(rec + 0x51) = 0;
    DSB(rec + 0x4d) = 0x1e;

    /* Tick A: the countdown has not expired. The variable advances by rec+0x58
     * and no rng is drawn. */
    DSB(rec + 0x4c) = 5;
    DSB(rec + 0x52) = 0x10;              /* seeded sentinel, not the post-state */
    DSB(rec + 0x58) = 1;
    DSD(rec + 8) = ANIM_SCRATCH;
    DSD(rec + 0x20) = 0;                 /* expired frame timer */
    DSD(rec + 0x24) = 0x3f800000u;       /* 1.0f: frame_timer runs */
    rng_seed(0xabcd);
    u32 lcg0 = DSD(DS_000EF6D8);
    actor_sync(rec);
    CHECK_EQ_INT((int)DSB(rec + 0x52), 0x11);        /* 0x10 + 1 */
    CHECK_EQ_INT((int)DSB(rec + 0x4c), 4);
    CHECK_EQ_INT((int)DSD(DS_000EF6D8), (int)lcg0);  /* no rng draw */

    /* Tick B: the countdown expires. rng(2) flips rec+0x58 and rec+0x4C is
     * reseeded to 3 * (rec+0x4D / 3). The expected flip is the same draw the
     * callback makes, so seed and draw it here. */
    rng_seed(0xabcd);
    u32 draw = rng_next(2u);
    DSB(rec + 0x4c) = 0;
    DSB(rec + 0x52) = 5;
    DSB(rec + 0x58) = 1;
    DSD(rec + 8) = ANIM_SCRATCH;
    DSD(rec + 0x20) = 0;
    DSD(rec + 0x24) = 0x3f800000u;
    DSW(rec + 0x28) = 0;
    DSW(rec + 0x2a) = 0;
    rng_seed(0xabcd);
    u32 lcg1 = DSD(DS_000EF6D8);
    actor_sync(rec);
    CHECK_EQ_INT((int)DSB(rec + 0x4c), 0x1e);        /* 3 * (0x1e / 3) */
    CHECK(DSD(DS_000EF6D8) != lcg1, "the expiry drew rng(2)");
    CHECK_EQ_INT((int)DSB(rec + 0x58), draw != 0u ? 1 : 0xff);
    CHECK_EQ_INT((int)DSB(rec + 0x52), draw != 0u ? 6 : 4);
}

int test_anim(void)
{
    int before = g_failures;
    if (res_count() != 69)
        CHECK(res_load_index("data/game/C", "data/game/C/INDEX") == 69,
              "index loaded");
    CHECK(DSD(DS_001014F4) != 0, "actor pool allocated");

    check_read_write_var();
    check_next_sprite_id();
    check_real_stream();
    check_walk();
    check_entry_helpers();
    check_dispatcher_streams();
    check_opcode8_pin();
    check_globe_opcode11_spawn();
    check_idle_tick_37a58();
    return g_failures - before;
}

/* ---- test_frontend.c ---- */

/* The front-end helpers 0x1C6D4 and 0x33904, and (under PR_FRONTEND_DUMP) the
 * state-2 selector driver. The driver calls game_init(), which may run once per
 * process, so it is env-gated exactly like test_title(): run_tests.c runs this
 * file alone when PR_FRONTEND_DUMP is set, and the helper checks still run at
 * the top of test_frontend() before the driver. */















/* The reference's LCG state when its state 6 runs. The attract's voice tick
 * (0x10F28, called from 0x11559 while DS_0009AD58 == 0) is the only consumer
 * before the title: it draws 26 values over the boot attract, and the port's
 * own attract reaches attract_step case 0xB's title handoff with
 * DS_000EF6D8 == 0x4308698B, i.e. seed 0xABCD advanced 26 steps. The title's
 * three draws are pinned to constants (no advance) and states 2..5 draw
 * nothing (only the pinned opcode-8 handler is reachable), so the reference's
 * state-6 entry is that same state. The driver enters at state 2, so it
 * re-seeds to it — the same pattern test_title_window uses for the pinned
 * title. The assertion below fails if the re-seed or the state-6 draws move. */
#define FRONTEND_RNG_AFTER_ATTRACT 0x4308698Bu

/* PORT: the driver's alignment seed for the frame counter DS_000EF6DC at its
 * state-2 entry. The driver enters at state 2 and skips the attract and the
 * title, so it seeds the counter as it seeds the LCG, to the port's own natural
 * boot count: the raw's only writer is 0x24C5C's per-iteration word `inc`
 * (0x24CCD..0x24CDB, 0 in the image), and the port's continuous boot run spends
 * 690 iterations in state 0 and 196 in the title state 1 (1 + 95 phase-1 steps
 * of 0x10 from 0x600 + the 0x3E688 fade), 886 before its first state-2 frame.
 * test_attract's PR_ATTRACT_DUMP run asserts both counts (690 at the title
 * entry, this value at the state-2 handoff). With this seed 0x1282C's
 * (DS_000EF6DC & 0x3F) == 0 gate spawns the grey flier at state-7 f = 91,
 * which captures 864/865 show (demo record §15).
 * TODO(verify): the original's live counter is unread (no live-RAM dump; demo
 * record §1.6). The frontend capture's timing only bounds it to 882..903, and
 * the mod-64 pin by the flier holds only if the port's modelled iteration count
 * from state 2 to f = 91 is the original's, including the loader-stall ticks
 * at 0x1B45F that game_flow.md's residual 1 records as un-derivable. */
#define FRONTEND_FRAMES_BEFORE_STATE2 886u

/* PORT: the driver's alignment seed for the attract cycle counter DS_000F0A5C
 * at its state-2 entry, the boot attract's post-state. 0x10E80 stores 4
 * (game_state_init), and the boot attract's one phase 2 wraps it to 0
 * (0x11089 `inc`, 0x110A1 `jl`, 0x110A5 store 0); the title and states 2..7
 * never write it. The driver skips the boot attract, so it seeds 0, and the
 * next attract (after the demo's exit) counts 1: phase 9 takes its 0x78 arm,
 * phase 0xA spawns the lightning actor 0x9AD30 and phase 0xB hands off to
 * state 6 with DS_000F0A72 = 5 (0x1150E), the second demo the capture shows
 * from 2385. Left at 4, it wraps to 0 and hands off to the title. */
#define FRONTEND_ATTRACT_CYCLE_AFTER_BOOT 0u

/* The cycle-2 dump: every frame the driver's run presents after the demo's
 * exit frame (loop 1970), written to <dump>/cycle2/frame_%04d.raw in order.
 * That is the loop's presented frame per iteration (the same rule as the
 * front-end dump) and, inside the iteration that plays the logos, each screen
 * 0x1C740 writes (movie_set_screen_hook: the entry/exit blanks and every
 * frame). A separate directory keeps these frames out of the front-end and
 * demo-fight windows, whose alignment would otherwise match the attract's
 * second cycle against the capture's first (frames 1..~560). */
static char fe_cyc2_dir[1300];
static int fe_cyc2_on;
static int fe_cyc2_n;
static int fe_cyc2_failed;

/* Writes the displayed screen (the aperture after the last gfx_present or
 * 0x52106, else DS_000E87A0) as RGB24 through gfx_dac; 1 when all 192000
 * bytes were written. */
static int fe_write_frame(const char *path)
{
    const u8 *fb = gfx_display();
    if (fb == NULL) fb = mem + DSD(DS_000E87A0);
    FILE *fr = fopen(path, "wb");
    int ok = fr != NULL;
    if (fr != NULL) {
        for (u32 b = 0; b < 320u * 200u; b++) {
            if (fwrite(gfx_dac[fb[b]], 1, 3, fr) != 3) {
                ok = 0;
                break;
            }
        }
        if (fclose(fr) != 0) ok = 0;
    }
    return ok;
}

static void fe_cyc2_dump(void)
{
    if (!fe_cyc2_on || fe_cyc2_failed) return;
    char path[1400];
    snprintf(path, sizeof path, "%s/frame_%04d.raw", fe_cyc2_dir, fe_cyc2_n);
    palette_dump_frame_marker(fe_cyc2_n);
    if (fe_write_frame(path)) fe_cyc2_n++;
    else fe_cyc2_failed = 1;
}

/* The loader's `- LOADING -` screens in the cycle-2 dump (res.c's seam, record
 * §45-A): the loop that drew each and the cycle-2 frame index it was written
 * at. fe_loop_i is the driver's current loop. */
static int fe_loop_i;
static int fe_ld_n;
static int fe_ld_loop[16], fe_ld_frame[16];

static void fe_cyc2_loader(void)
{
    if (fe_cyc2_on && !fe_cyc2_failed && fe_ld_n < 16) {
        fe_ld_loop[fe_ld_n] = fe_loop_i;
        fe_ld_frame[fe_ld_n] = fe_cyc2_n;
        fe_ld_n++;
    }
    fe_cyc2_dump();
}

/* 1 when INDEX entry e carries 0x1B3AC's loaded bit. */
static int fe_entry_read(u32 e)
{
    return (DSD(DSD(DS_001014E0) + e * 20u + 12u) & 0x20000000u) != 0u;
}

/* The driver's loop: FE_DEMO_LOOPS covers the first demo's 0x11BCC exit at
 * loop 1970 (the run's length before the cycle-2 dump); FE_LOOPS reaches the
 * second attract cycle's state-6 handoff (loop 2782), the whole second demo
 * (its exit to state 9 at loop 3684, the poll's f = 4571), the third attract
 * cycle's high-score screen (loops 3684..3984) and the third demo from its
 * state-6 entry (loop 3985). FE_LOOPS is a measurement window, not a raw
 * value: 4100 (3900 before record §47-A, 3500 before §44-A, 3300 before §41,
 * 3200 before §40, 3100 before §39, 2800 before §38) reaches past the
 * capture's last frame, 3616: capture 3546 is loop 3986's frame (cycle-2
 * 2194), and 70 capture frames at the 60.05/70.09 Hz ratio are about 60
 * loops, so 3616 lies near loop 4046. (The old estimate, loop 3865 from the
 * exit's capture frame 3406, missed that the capture drops the static
 * high-score screen's repeated frames.) */
#define FE_DEMO_LOOPS 2000
#define FE_LOOPS 4100

int test_frontend(void)
{
    int before = g_failures;

    /* 0x1C6D4: membership over nine resource addresses read from mem[]. The
     * raw dereferences its argument, so use a scratch linear address inside
     * mem[] (never a host pointer). */
    {
        static const u32 known[9] = {
            0x80995Cu, 0x80997Cu, 0x809984u, 0x80998Cu, 0x809994u,
            0x80999Cu, 0x8099A4u, 0x8099ACu, 0x8099CCu,
        };
        const u32 scratch = 0x3002000u;   /* scratch linear addr inside mem[] */
        u32 saved = DSD(scratch);
        for (u32 i = 0; i < 9u; i++) {
            DSD(scratch) = known[i];
            CHECK_EQ_INT((int)frontend_resource_known(scratch), 1);
        }
        DSD(scratch) = 0x809980u;         /* between two members */
        CHECK_EQ_INT((int)frontend_resource_known(scratch), 0);
        DSD(scratch) = 0x8099B0u;         /* past the last member */
        CHECK_EQ_INT((int)frontend_resource_known(scratch), 0);
        DSD(scratch) = saved;
    }

    /* 0x33904: the 0x10-stride table iterator at DS_00107608, bounded by
     * 0x107798 and stopping at a nonzero dword at +0x14. The table is runtime
     * state and empty before game_init(), so seed exactly one live entry and
     * restore the whole table afterwards. */
    {
        static u8 saved[0x190];
        tf_frontend_seed_list(0u, saved);
        CHECK_EQ_INT((int)frontend_list_next(0), (int)(DS_00107608 + 0x10u));
        CHECK_EQ_INT((int)frontend_list_next(DS_00107608 + 0x10u), 0);
        tf_frontend_restore_list(saved);
    }

    /* 0x11F28 / 0x11D04: the coin path. The mask table DS_0009ACBC lives in
     * the data object; test_le() maps PRAGE.EXE in the shared suite, but the
     * PR_FRONTEND_DUMP branch runs this file alone, so map it here to read the
     * shipped masks. An accepted coin debits one credit through 0x2CA7C, calls
     * 0x257A4 with the accepted mask (0x11D41) and returns early, so the
     * frame's state dispatch is skipped; a rejected poll leaves the credit
     * alone and runs the dispatch (state 9's countdown is the observable that
     * the dispatch ran or was skipped). State 8 calls 0x257A4 with 3 (0x11EB8).
     * Record §48-W. 0x257A4 rewrites the data object and, when the pools exist
     * (the unit suite; the isolated PR_FRONTEND_DUMP run reaches here before
     * game_init() and 0x2BAF4 returns at once), both pools and the 0xFA00-byte
     * copy [0xE87A0] -> [0xE87A4], pointed at a scratch area here. All of it
     * is saved and restored. */
    {
        const char *gdir = getenv("PR_GAME_DIR");
        char exe[560];
        if (gdir == NULL || gdir[0] == '\0') gdir = "data/game/C";
        snprintf(exe, sizeof exe, "%s/PRAGE.EXE", gdir);
        if (DSD(DS_0009ACBC) == 0u)
            CHECK(mem_load_le(exe, NULL) == 1,
                  "PRAGE.EXE maps for the coin mask table");
    }
    {
        static u8 s_data[0x10B0D0u - 0x80000u];
        static u8 s_rec[0xEBA0u], s_pset[0x4880u], s_scr[0x20000u];
        const u32 cw_src = 0x3D00000u, cw_dst = 0x3D10000u;
        const u32 rec_pool = DSD(DS_001014F4), pset_pool = DSD(DS_001014EC);
        tf_snap(s_data, 0x80000u, sizeof s_data);
        if (rec_pool != 0u) tf_snap(s_rec, rec_pool, sizeof s_rec);
        if (pset_pool != 0u) tf_snap(s_pset, pset_pool, sizeof s_pset);
        tf_snap(s_scr, cw_src, sizeof s_scr);
        DSD(DS_000E87A0) = cw_src;
        DSD(DS_000E87A4) = cw_dst;
        const u32 saved_c00 = DSD(DS_00105C00);
        const u32 saved_e4  = DSD(DS_001088E4);
        const u8  saved_1d  = DSB(DS_00104B1D);
        const u8  saved_1f  = DSB(DS_00104B1F);
        const u8  saved_60  = DSB(DS_00105D60);
        const u32 saved_dc  = DSD(DS_001082DC);
        const u16 saved_64  = DSW(DS_000F0A64);
        const u16 saved_6a  = DSW(DS_000F0A6A);
        const u16 saved_6c  = DSW(DS_000F0A6C);

        /* The masks are the shipped ones, or the checks below are vacuous. */
        CHECK_EQ_INT((int)DSD(DS_0009ACBC), 0x01000000);
        CHECK_EQ_INT((int)DSD(DS_0009ACBC + 4u), 0x00000100);

        DSB(DS_00104B1D) = 0;          /* coin poll enabled */
        DSB(DS_00104B1F) = 0;          /* debit not suppressed */
        DSB(DS_00105D60) = 0;          /* not free play */
        DSD(DS_00105C00) = 5u;
        DSD(DS_001082DC) = 0;          /* no localisation table: empty strings */

        /* Reject: no newly-pressed bit -> no debit, state 9's countdown runs. */
        DSD(DS_00104B00) = 3u;
        DSD(DS_00104AE4) = 0xDEADBEEFu;
        DSD(DS_001088E4) = 0u;
        DSW(DS_000F0A64) = 9; DSW(DS_000F0A6A) = 2; DSW(DS_000F0A6C) = 3;
        game_state_step();
        CHECK_EQ_INT((int)DSD(DS_00105C00), 5);
        CHECK_EQ_INT((int)DSW(DS_000F0A6A), 1);

        /* The reject leaves 0x257A4's outputs alone: the mode dword and the
         * hook keep their seeds. */
        CHECK_EQ_INT((int)DSD(DS_00104B00), 3);
        CHECK_EQ_INT((int)DSD(DS_00104AE4), (int)0xDEADBEEFu);

        /* Accept event 0: one credit debited, dispatch skipped (countdown held),
         * and 0x257A4(1): DS_00104B1F = the mask, the hook 0x4367C and mode
         * 0x1A with the return mode 0x10. */
        DSW(DS_00104B00) = 3u;
        DSD(DS_00104AE4) = 0xDEADBEEFu;
        DSW(DS_00104AFA) = 0x7777u;
        DSD(DS_001088E4) = DSD(DS_0009ACBC);
        DSW(DS_000F0A6A) = 2;
        game_state_step();
        CHECK_EQ_INT((int)DSD(DS_00105C00), 4);
        CHECK_EQ_INT((int)DSW(DS_000F0A6A), 2);
        CHECK_EQ_INT((int)DSB(DS_00104B1F), 1);
        CHECK_EQ_INT((int)DSD(DS_00104B00), 0x1A);
        CHECK_EQ_INT((int)DSD(DS_00104AE4), 0x4367C);
        CHECK_EQ_INT((int)DSW(DS_00104AFA), 0x10);

        /* Accept event 1: likewise, with the mask 2. 0x2CA7C debits only
         * while DS_00104B1F == 0, which 0x257A4 has just set, so re-arm it as
         * a fresh frame in mode 3 would find it. */
        DSB(DS_00104B1F) = 0;
        DSW(DS_00104B00) = 3u;
        DSD(DS_001088E4) = DSD(DS_0009ACBC + 4u);
        game_state_step();
        CHECK_EQ_INT((int)DSD(DS_00105C00), 3);
        CHECK_EQ_INT((int)DSB(DS_00104B1F), 2);
        CHECK_EQ_INT((int)DSD(DS_00104B00), 0x1A);

        /* Both events in one frame: both polls debit (DS_00104B1F is still 0
         * until 0x257A4 runs) and the mask is 1 | 2 (0x11D31 `or dl,2`). */
        DSB(DS_00104B1F) = 0;
        DSW(DS_00104B00) = 3u;
        DSD(DS_001088E4) = DSD(DS_0009ACBC) | DSD(DS_0009ACBC + 4u);
        DSW(DS_000F0A6A) = 2;
        game_state_step();
        CHECK_EQ_INT((int)DSD(DS_00105C00), 1);
        CHECK_EQ_INT((int)DSB(DS_00104B1F), 3);
        CHECK_EQ_INT((int)DSW(DS_000F0A6A), 2);

        /* State 8 (0x11EAC), no event: 0x257A4(3), then the shared tails. The
         * held pause chord (DS_001088D8 bytes +3 & 0x20, +1 & 0x10) makes
         * 0x10DB0 store state 4. It runs after 0x257A4 has cleared
         * DS_00104B1B (DS_00104B19 + 2), so it takes the arm that leaves
         * DS_000F0A6C alone; tails first would store 4 there too. */
        tf_put(s_data, 0x80000u, sizeof s_data);
        DSB(DS_00104B1D) = 0;
        DSB(DS_00105D60) = 0;
        DSD(DS_00105C00) = 5u;
        DSD(DS_001082DC) = 0;
        DSD(DS_000E87A0) = cw_src;
        DSD(DS_000E87A4) = cw_dst;
        DSD(DS_001088E4) = 0u;
        DSB(DS_00104B1F) = 0x77u;
        DSW(DS_00104B00) = 3u;
        DSD(DS_00104AE4) = 0xDEADBEEFu;
        DSW(DS_00104AFA) = 0x7777u;
        DSB(DS_000F0A71) = 0;
        DSD(DS_001088D8) = 0x20001000u;
        DSB(DS_00104B19 + 2u) = 0x77u;
        DSW(DS_000F0A64) = 8; DSW(DS_000F0A6A) = 2; DSW(DS_000F0A6C) = 0x7777u;
        game_state_step();
        CHECK_EQ_INT((int)DSB(DS_00104B1F), 3);
        CHECK_EQ_INT((int)DSD(DS_00104B00), 0x1A);
        CHECK_EQ_INT((int)DSD(DS_00104AE4), 0x4367C);
        CHECK_EQ_INT((int)DSW(DS_00104AFA), 0x10);
        CHECK_EQ_INT((int)DSW(DS_000F0A64), 4);
        CHECK_EQ_INT((int)DSB(DS_000F0A71), 1);
        CHECK_EQ_INT((int)DSW(DS_000F0A6C), 0x7777);

        DSD(DS_00105C00) = saved_c00; DSD(DS_001088E4) = saved_e4;
        DSB(DS_00104B1D) = saved_1d;  DSB(DS_00104B1F) = saved_1f;
        DSB(DS_00105D60) = saved_60;  DSD(DS_001082DC) = saved_dc;
        DSW(DS_000F0A64) = saved_64;  DSW(DS_000F0A6A) = saved_6a;
        DSW(DS_000F0A6C) = saved_6c;
        tf_put(s_data, 0x80000u, sizeof s_data);
        if (rec_pool != 0u) tf_put(s_rec, rec_pool, sizeof s_rec);
        if (pset_pool != 0u) tf_put(s_pset, pset_pool, sizeof s_pset);
        tf_put(s_scr, cw_src, sizeof s_scr);
    }

    /* 0x4F1D0 zeroes the two origin words; 0x4F1E4 writes DS_00104B15. They are
     * distinct raw functions and 4b-B conflated them. */
    {
        const u16 saved_3a = DSW(DS_00107A3A);
        const u16 saved_38 = DSW(DS_00107A38);
        const u8  saved_15 = DSB(DS_00104B15);
        DSW(DS_00107A3A) = 0x1234;
        DSW(DS_00107A38) = 0x5678;
        DSB(DS_00104B15) = 0x9A;
        frontend_origin_zero();
        CHECK_EQ_INT((int)DSW(DS_00107A3A), 0);
        CHECK_EQ_INT((int)DSW(DS_00107A38), 0);
        CHECK_EQ_INT((int)DSB(DS_00104B15), 0x9A);   /* 0x4F1D0 does NOT touch it */
        DSW(DS_00107A3A) = saved_3a; DSW(DS_00107A38) = saved_38;
        DSB(DS_00104B15) = saved_15;
    }

    /* The effect call sites 0x29B74 and 0x41578 are ported (record §42-E;
     * test_effects), their callers are not. 0x41578 is only direct-called:
     * no dword 0x00041578 exists in either object, so nothing may register
     * it. 0x29B74 is a DS_00104AE4 pointer and is registered by actors_init
     * (asserted in test_actors, which runs it; this driver may not). The
     * behavioral half (the ported state machine never arms DS_00104AE4 and
     * never leaves mode 3) is asserted in the state-5 block below. */
    {
        CHECK(fn_resolve(FN_00041578) == NULL,
              "0x41578 is direct-called only, not registered");
    }

    /* 0x12484: state 3 (the post-select presentation). Phase 0 re-spawns the
     * four corner rows, spawns a type-3 effect (0x13C70) for the live list entry
     * whose handle is 0x3E688, takes the DS_00104528 bit-1 branch and hands off
     * through 0x12658, which stores the first of its three actors at
     * DS_000F0A58. Phase 1 terminates into state 9 once the handoff actor's
     * offset reaches 0x1E00. Needs the actor/effect pools res_load_index
     * allocates; the isolated PR_FRONTEND_DUMP run has none before game_init(),
     * so it skips here and exercises state 3 through the driver instead. */
    if (DSD(DS_001014F4) != 0) {
        static u8 saved_list[0x190];
        const u8  saved_1d = DSB(DS_00104B1D);
        const u8  saved_29 = DSB(DS_00104528 + 1u);
        const u32 saved_40 = DSD(DS_000F0A40);
        const u32 saved_58 = DSD(DS_000F0A58);
        const u16 saved_64 = DSW(DS_000F0A64);
        const u16 saved_6a = DSW(DS_000F0A6A);
        const u16 saved_6c = DSW(DS_000F0A6C);
        const u8  saved_6f = DSB(DS_000F0A6F);
        const u16 saved_38 = DSW(DS_00107A38);
        const u16 saved_44 = DSW(DS_00107A44);
        const u8  saved_72 = DSB(DS_000F0A72);

        tf_frontend_seed_list(0x3E688u, saved_list);
        DSB(DS_00104B1D) = 0;
        DSW(DS_000F0A64) = 3;
        DSW(DS_000F0A6A) = 1;
        DSB(DS_000F0A6F) = 0;
        DSB(DS_00104528 + 1u) |= 2u;               /* take the DS_000F0A40 branch */
        DSD(DS_000F0A40) = 0;
        game_state_step();
        CHECK_EQ_INT((int)DSB(DS_000F0A6F), 1);
        CHECK(DSD(DS_000F0A40) != 0u, "phase 0 spawned through 0x2AE14");
        CHECK(effects_active() != 0, "phase 0 spawned a type-3 effect");

        {
            u32 cam = DSD(DS_000F0A58);
            DSD(cam + 0x1Cu) = 0x1E01u;            /* sentinel; terminate stores 0x1E00 */
            DSD(cam + 0x34u) = 0x10000u;           /* high word 1, so b = 1 >= |a| = 1 */
            DSB(DS_000F0A6F) = 1;
            game_state_step();
            CHECK_EQ_INT((int)DSW(DS_000F0A64), 9);
            CHECK_EQ_INT((int)DSW(DS_000F0A6C), 6);
            CHECK_EQ_INT((int)DSW(DS_000F0A6A), 0xF0);
            CHECK_EQ_INT((int)DSD(cam + 0x1Cu), 0x1E00);
        }

        tf_frontend_restore_list(saved_list);
        DSB(DS_00104B1D) = saved_1d;   DSB(DS_00104528 + 1u) = saved_29;
        DSD(DS_000F0A40) = saved_40;   DSD(DS_000F0A58) = saved_58;
        DSW(DS_000F0A64) = saved_64;   DSW(DS_000F0A6A) = saved_6a;
        DSW(DS_000F0A6C) = saved_6c;   DSB(DS_000F0A6F) = saved_6f;
        DSW(DS_00107A38) = saved_38;   DSW(DS_00107A44) = saved_44;
        DSB(DS_000F0A72) = saved_72;
    }

    /* 0x11578: state 4 (the match-up credit roll). Its phase counter is
     * DS_0009AD98, separate from the dispatch word DS_000F0A64. The 8/12/13
     * 0x2F4BC call counts are literal in the raw and are transcribed unrolled
     * rather than counted here; only the phase state is asserted. The phase-0
     * 0x2AE14 spawn needs the actor pool, so that one check is guarded (the
     * isolated PR_FRONTEND_DUMP run reaches here before game_init()). */
    {
        const u8  saved_1d = DSB(DS_00104B1D);
        const u8  saved_71 = DSB(DS_000F0A71);
        const u8  saved_58 = DSB(DS_0009AD58);
        const u8  saved_15 = DSB(DS_00104B15);
        const u8  saved_c5 = DSB(DS_00105C05);
        const u16 saved_64 = DSW(DS_000F0A64);
        const u16 saved_6a = DSW(DS_000F0A6A);
        const u16 saved_6c = DSW(DS_000F0A6C);
        const u16 saved_74 = DSW(DS_000F0A74);
        const u16 saved_76 = DSW(DS_000F0A76);
        const u16 saved_98 = DSW(DS_0009AD98);

        DSB(DS_00104B1D) = 1;          /* coin poll skipped */
        DSB(DS_000F0A71) = 1;          /* pause/continue tails skipped */
        DSB(DS_0009AD58) = 1;          /* overlay skipped */
        DSW(DS_000F0A64) = 4;

        /* Phase 0: the setup sequence, the 180-frame timer, continuation 1. */
        DSW(DS_0009AD98) = 0;
        DSW(DS_000F0A76) = 0;
        DSW(DS_000F0A74) = 0;
        game_state_step();
        CHECK_EQ_INT((int)DSW(DS_0009AD98), 4);
        CHECK_EQ_INT((int)DSW(DS_000F0A76), 0xB4);
        CHECK_EQ_INT((int)DSW(DS_000F0A74), 1);

        /* The phase-0 0x2AE14 descriptor is the data object's 0x9AD84 (raw
         * 0x115CA), not a literal 0. Every phase-0 spawn head-inserts, so the
         * first one (the 0x2AE14 actor) is the active list's tail and carries
         * the descriptor's first dword in its +8 record dword. */
        if (DSD(DS_001014F4) != 0) {
            u32 tail = 0;
            for (u32 rec = actor_list_head(); rec != 0u; rec = actor_next(rec))
                tail = rec;
            CHECK(tail != 0u, "phase 0 filled the actor list");
            if (tail != 0u)
                CHECK_EQ_INT((int)DSD(tail + 8u), (int)DSD(0x9AD84u));
        }

        /* Phase 4 with a non-zero timer decrements it and does not continue.
         * DS_000F0A74 is a sentinel that differs from both 4 and the natural
         * continuation values, so an always-continue bug is unambiguous. */
        DSW(DS_0009AD98) = 4;
        DSW(DS_000F0A76) = 2;
        DSW(DS_000F0A74) = 9;
        game_state_step();
        CHECK_EQ_INT((int)DSW(DS_000F0A76), 1);
        CHECK_EQ_INT((int)DSW(DS_0009AD98), 4);

        /* Phase 4 at zero: the raw tests the value before the decrement, so the
         * store wraps to 0xFFFF and the continuation fires this frame. */
        DSW(DS_000F0A76) = 0;
        DSW(DS_000F0A74) = 3;
        game_state_step();
        CHECK_EQ_INT((int)DSW(DS_0009AD98), 3);
        CHECK_EQ_INT((int)DSW(DS_000F0A76), 0xFFFF);

        /* Phase 3 hands to state 9 with the state-3 terminator values. The two
         * written globals start at sentinels so the checks test the stores. */
        DSW(DS_0009AD98) = 3;
        DSW(DS_000F0A6C) = 0x1234;
        DSW(DS_000F0A6A) = 0x1234;
        game_state_step();
        CHECK_EQ_INT((int)DSW(DS_000F0A64), 9);
        CHECK_EQ_INT((int)DSW(DS_000F0A6C), 0);
        CHECK_EQ_INT((int)DSW(DS_000F0A6A), 1);
        CHECK_EQ_INT((int)DSW(DS_0009AD98), 0);

        DSB(DS_00104B1D) = saved_1d;   DSB(DS_000F0A71) = saved_71;
        DSB(DS_0009AD58) = saved_58;
        DSB(DS_00104B15) = saved_15;   DSB(DS_00105C05) = saved_c5;
        DSW(DS_000F0A64) = saved_64;   DSW(DS_000F0A6A) = saved_6a;
        DSW(DS_000F0A6C) = saved_6c;   DSW(DS_000F0A74) = saved_74;
        DSW(DS_000F0A76) = saved_76;   DSW(DS_0009AD98) = saved_98;
    }

    /* 0x11D04 case 5 (state 5, the match-start block inline in the dispatch).
     * The body runs 0x2C3FC (voice cancel, skipped), 0x1EA08, 0x2C06C, 0x32970
     * (run clock, skipped), then stores the raw's five values. The case uses
     * break, so the three shared tails still run after the switch; the second
     * call below arms one of them to prove the case does not return early. */
    {
        const u8  saved_1d = DSB(DS_00104B1D);
        const u8  saved_71 = DSB(DS_000F0A71);
        const u8  saved_58 = DSB(DS_0009AD58);
        const u8  saved_15 = DSB(DS_00104B15);
        const u8  saved_60 = DSB(DS_00105D60);
        const u8  saved_19 = DSB(DS_00104B19 + 2u);
        const u8  saved_d8_1 = DSB(DS_001088D8 + 1u);
        const u8  saved_d8_3 = DSB(DS_001088D8 + 3u);
        const u8  saved_c5 = DSB(DS_00105C05);
        const u16 saved_64 = DSW(DS_000F0A64);
        const u16 saved_6a = DSW(DS_000F0A6A);
        const u16 saved_6c = DSW(DS_000F0A6C);
        const u8  saved_6f = DSB(DS_000F0A6F);
        const u8  saved_72 = DSB(DS_000F0A72);
        const u32 saved_ae8 = DSD(DS_00104AE8);
        const u32 saved_aec = DSD(DS_00104AEC);
        const u32 saved_ad0 = DSD(DS_00104AD0);
        const u32 saved_ae4 = DSD(DS_00104AE4);
        const u16 saved_b00 = DSW(DS_00104B00);
        u32 saved_row[7];
        for (u32 i = 0; i < 7u; i++)
            saved_row[i] = DSD(DS_00107A1C + i * 4u);

        DSB(DS_00104B1D) = 1;          /* coin poll skipped */
        DSB(DS_000F0A71) = 1;          /* pause/continue tails skipped */
        DSB(DS_0009AD58) = 1;          /* overlay skipped */

        /* Sentinels differ from every post-condition so a no-op case fails. */
        DSW(DS_000F0A64) = 5;
        DSW(DS_000F0A6C) = 0x1234;
        DSW(DS_000F0A6A) = 0x1234;
        DSB(DS_000F0A6F) = 0xAB;
        DSB(DS_000F0A72) = 0xCD;
        DSB(DS_00104B15) = 0x9A;
        DSB(DS_00105C05) = 0x9A;   /* sentinel; 0x2C06C stores 0x1D */
        /* Sentinels for 0x1EA08's prefix: 0x2BAF4 zeroes the three process
         * masks and 0x38B70 clears the 7-entry row table before 0x38B18 fills
         * slot 0 from the 0xA7B6C descriptor. */
        DSD(DS_00104AE8) = 0x12345678u;
        DSD(DS_00104AEC) = 0x12345678u;
        DSD(DS_00104AD0) = 0x12345678u;
        mem_fill(DS_00107A1C, 0, 28u);
        /* Task 8 sentinels: the deferred effect call sites must stay unarmed
         * through the ported path. 0xDEADBEEF is never a code address the port
         * arms, and mode 3 is the only mode the port dispatches. */
        DSD(DS_00104AE4) = 0xDEADBEEFu;
        DSW(DS_00104B00) = 3;
        game_state_step();
        CHECK_EQ_INT((int)DSW(DS_000F0A64), 9);
        CHECK_EQ_INT((int)DSW(DS_000F0A6C), 6);
        CHECK_EQ_INT((int)DSW(DS_000F0A6A), 0x12C);
        CHECK_EQ_INT((int)DSB(DS_000F0A6F), 0);
        CHECK_EQ_INT((int)DSB(DS_000F0A72), 0);
        CHECK_EQ_INT((int)DSB(DS_00104B15), 0);   /* 0x1EA08 ran 0x4F1E4 */
        CHECK_EQ_INT((int)DSB(DS_00105C05), 0x1D);  /* 0x2C06C ran */
        /* Task 8: the ported state-5 path does not arm the 0x29B74 handler
         * (its DS_00104AE4 stores are unported) and does not leave mode 3
         * (so the six call dword [0x104ae4] sites and the four 0x41578
         * sites stay unreachable). */
        CHECK(DSD(DS_00104AE4) == 0xDEADBEEFu,
              "state 5 does not arm the 0x29B74 handler");
        CHECK_EQ_INT((int)DSW(DS_00104B00), 3);
        /* 0x1EA08's prefix is observable only with the pool present (the
         * isolated PR_FRONTEND_DUMP run reaches here before game_init()). */
        if (DSD(DS_001014F4) != 0) {
            CHECK_EQ_INT((int)DSD(DS_00104AE8), 0);   /* 0x2BAF4 ran */
            CHECK_EQ_INT((int)DSD(DS_00104AEC), 0);
            CHECK_EQ_INT((int)DSD(DS_00104AD0), 0);
            CHECK(DSD(DS_00107A1C) != 0u, "0x1EA08 spawned the 0xA7B6C row");
            if (DSD(DS_00107A1C) != 0u) {
                u32 row = DSD(DS_00107A1C);
                CHECK_EQ_INT((int)DSD(row + 8u), (int)DSD(0xA7B6Cu));
            }
        }

        /* Fall-through proof: re-enter state 5 with the pause tail (0x10DB0)
         * armed and its extra branch disabled. The tail runs after the case and
         * overwrites the case's state 9 with state 4; DS_000F0A71 records that
         * it fired. A case that returned before the tails would leave 9. */
        DSW(DS_000F0A64) = 5;
        DSB(DS_000F0A71) = 0;
        DSB(DS_00104B19 + 2u) = 0;
        DSB(DS_001088D8 + 3u) |= 0x20u;
        DSB(DS_001088D8 + 1u) |= 0x10u;
        DSB(DS_00104B15) = 0x9A;   /* sentinel; 0x1EA08's 0x4F1E4 stores 0 */
        DSB(DS_00105C05) = 0x9A;   /* sentinel; 0x2C06C stores 0x1D */
        game_state_step();
        CHECK_EQ_INT((int)DSB(DS_00104B15), 0);     /* 0x1EA08 ran again */
        CHECK_EQ_INT((int)DSB(DS_00105C05), 0x1D);  /* 0x2C06C ran again */
        CHECK_EQ_INT((int)DSB(DS_000F0A71), 1);
        CHECK_EQ_INT((int)DSW(DS_000F0A64), 4);

        DSB(DS_00104B1D) = saved_1d;   DSB(DS_000F0A71) = saved_71;
        DSB(DS_0009AD58) = saved_58;   DSB(DS_00104B15) = saved_15;
        DSB(DS_00105D60) = saved_60;   DSB(DS_00104B19 + 2u) = saved_19;
        DSB(DS_001088D8 + 1u) = saved_d8_1;
        DSB(DS_001088D8 + 3u) = saved_d8_3;
        DSB(DS_00105C05) = saved_c5;
        DSW(DS_000F0A64) = saved_64;   DSW(DS_000F0A6A) = saved_6a;
        DSW(DS_000F0A6C) = saved_6c;   DSB(DS_000F0A6F) = saved_6f;
        DSB(DS_000F0A72) = saved_72;
        DSD(DS_00104AE8) = saved_ae8;  DSD(DS_00104AEC) = saved_aec;
        DSD(DS_00104AD0) = saved_ad0;
        DSD(DS_00104AE4) = saved_ae4;  DSW(DS_00104B00) = saved_b00;
        for (u32 i = 0; i < 7u; i++)
            DSD(DS_00107A1C + i * 4u) = saved_row[i];
    }

    /* 0x41350 / 0xA8A28: the palette-variant flag the T-rex's handle is
     * selected by. With the original's pre-state (DS_0010816A[0..1] = 0, the
     * BSS value the front-end dump driver must seed — record §7.1) side 0's
     * char equals side 1's, so variant 1 selects 0xA8A28[1] = 0x1BB9FCD8; the
     * driver's old 0xFF seed made variant 0 and 0x1BB9FD58. The variant sentinel
     * starts at 0, differing from the post-condition 1. */
    {
        const u8  saved_1d  = DSB(DS_00104B1D);
        const u8  saved_6a0 = DSB(DS_0010816A);
        const u8  saved_6a1 = DSB(DS_0010816A + 1u);
        const u8  saved_6e0 = DSB(DS_0010816E);
        const u8  saved_b34 = DSB(DS_00105B34);
        const u8  saved_b35 = DSB(DS_00105B34 + 1u);
        const u8  saved_idx = DSB(0x0010810Du);
        const u8  saved_63  = DSB(DS_001077B0 + 0x63u);
        const u32 saved_3c  = DSD(DS_001077B0 + 0x3Cu);
        const u8  saved_7f  = DSB(DS_001077B0 + 0x7Fu);
        const u8  saved_80  = DSB(DS_001077B0 + 0x80u);
        const u8  saved_82  = DSB(DS_001077B0 + 0x82u);
        const u8  saved_5b  = DSB(DS_001077B0 + 0x5Bu);
        const u16 saved_860 = DSW(DS_00108860);

        DSB(DS_00104B1D) = 0;          /* the char store runs */
        DSB(DS_0010816A) = 0;          /* the original's BSS pre-state */
        DSB(DS_0010816A + 1u) = 0;
        DSB(DS_00105B34) = 0;          /* sentinel; the post-condition is 1 */
        DSB(DS_00105B34 + 1u) = 0;
        fight_char_select(0u, 0u);     /* 0xC835A[0] = 0, the T-rex */
        CHECK_EQ_INT((int)DSB(DS_00105B34), 1);
        CHECK_EQ_INT((int)DSD(0xA8A28u + DSB(DS_00105B34) * 4u), 0x1BB9FCD8);

        DSB(DS_00104B1D) = saved_1d;
        DSB(DS_0010816A) = saved_6a0;  DSB(DS_0010816A + 1u) = saved_6a1;
        DSB(DS_0010816E) = saved_6e0;
        DSB(DS_00105B34) = saved_b34;  DSB(DS_00105B34 + 1u) = saved_b35;
        DSB(0x0010810Du) = saved_idx;
        DSB(DS_001077B0 + 0x63u) = saved_63;
        DSD(DS_001077B0 + 0x3Cu) = saved_3c;
        DSB(DS_001077B0 + 0x7Fu) = saved_7f;
        DSB(DS_001077B0 + 0x80u) = saved_80;
        DSB(DS_001077B0 + 0x82u) = saved_82;
        DSB(DS_001077B0 + 0x5Bu) = saved_5b;
        DSW(DS_00108860) = saved_860;
    }

    /* Record §3: the loader draw's palette flush. The raw's loader draw
     * (0x1C65C -> 0x1C470) and the master loop (0x25672) are the same
     * whole-list drain; the scope is what is on the list at each call. The raw's
     * palette_list_init (0x336C0) enqueues the initial 0x33734 record
     * { ptr = 0xBD470; first = 0; count = 1; flag = 0 } (0x336F6-0x3370A:
     * EBX = 0xBD470, EDX = 1, EAX = 0), so the loader draw's flush drains it
     * (the raw's drain also carries the glyph walk's font palette 0x80997C).
     * The port previously omitted the enqueue; the record's data word is zero,
     * so the upload is black (DAC[0]) — the same value the port's gfx_dac clear
     * leaves — but the record's presence is the raw-faithful observable. Seed
     * the base record with a sentinel that differs from the post-condition, so
     * the checks cannot pass on a never-written BSS zero. */
    {
        u32 saved[0xC0], saved_bd470 = DSD(DS_000BD470);   /* palette_list_init resets the whole list + ownership-table region and DS_000BD470; save it all. */
        const u32 saved_head = DSD(DS_00107798);
        for (u32 k = 0; k < 0xC0u; k++) saved[k] = DSD(DS_00107498 + k * 4u);
        DSD(DS_00107498 + 0u)  = 0xDEADBEEFu;
        DSD(DS_00107498 + 4u)  = 0xDEADBEEFu;
        DSD(DS_00107498 + 8u)  = 0xDEADBEEFu;
        DSD(DS_00107498 + 12u) = 0xDEADBEEFu;
        DSD(DS_00107798) = DS_00107498 + 0x40u;   /* a sentinel head */

        palette_list_init();                      /* 0x336C0 */
        CHECK_EQ_INT((int)DSD(DS_00107798), (int)(DS_00107498 + 0x10u));
        CHECK_EQ_INT((int)DSD(DS_00107498 + 0u),  (int)DS_000BD470);
        CHECK_EQ_INT((int)DSD(DS_00107498 + 4u),  0);
        CHECK_EQ_INT((int)DSD(DS_00107498 + 8u),  1);
        /* 0x3373F stores the flag's low byte only (todo-verify record §3):
         * the sentinel's upper three bytes survive, and 0x336C0 never clears
         * the list's +0xC dwords (it marks only +4 at 0x336E9). */
        CHECK_EQ_INT((int)DSD(DS_00107498 + 12u), (int)0xDEADBE00u);

        gfx_flush_palette();                      /* the loader draw's 0x1C470 */
        CHECK_EQ_INT((int)DSD(DS_00107498 + 4u), -1);   /* consumed */
        CHECK_EQ_INT((int)DSD(DS_00107798), (int)DS_00107498);

        for (u32 k = 0; k < 0xC0u; k++) DSD(DS_00107498 + k * 4u) = saved[k];
        DSD(DS_00107798) = saved_head;
        DSD(DS_000BD470) = saved_bd470;
    }

    const char *dump = getenv("PR_FRONTEND_DUMP");
    if (dump == NULL || dump[0] == '\0') {
        printf("test_frontend: PR_FRONTEND_DUMP unset, state-2 driver skipped\n");
        return g_failures - before;
    }

    /* The driver runs the real init and loop for a fixed window, as the title
     * driver does, because game_init() may run once per process.
     * PR_FRONTEND_DUMP names a directory to receive the hash log and, from the
     * state-3 entry on, one RGB24 frame per presented frame. The log covers the
     * whole window, through the demo, so two runs can be diffed; the frame dump
     * is capped by PR_FRONTEND_DUMP_FRAMES (default 1400).
     *
     * FE_DEMO_LOOPS (2000) and the 1400-frame cap are sized from the demo's
     * state-7 exit: state 3 is entered at loop frame 589, state 6 runs at loop
     * frame 1070 (dumped frame 481), and 0x11BCC's timer exit runs at loop frame
     * 1970, where the state drops to 0. game_frame (0x24C5C) runs before the
     * 0x25643 present in the same iteration, and the title/attract dump hooks
     * (flow.c, state_before) credit each presented frame to the state that
     * started its iteration; this driver does the same with state_in, and
     * keeps the post-state for the entry (loop 589 ends in state 3). So loop
     * frame 1970 (state 7 in, 0 out), the frame presented in the iteration
     * that starts in state 7 and exits it, is dumped, and 1971 on, which start
     * in state 0, are not in the state >= 3 window.
     * The dump run is loop frames 589..1970, i.e. dumped frames 0..1381 (1382
     * frames); the 1400 cap covers it and FE_DEMO_LOOPS clears the 1970
     * exit. The exit frame closes this dump: the loop runs on to FE_LOOPS
     * (3900) and writes every later presented frame to <dump>/cycle2 instead
     * (fe_cyc2_dump; record §36), the attract's second cycle, which the
     * capture shows from 1886 to its second demo at 2385. The per-frame
     * measurements and end-of-run reads below keep the FE_DEMO_LOOPS window. The front-end window is distinct [560..1884] (1325 frames: 517
     * clean, 801 splice, 3 transition, 2 unexplained), up to the capture's
     * first all-black frame after the demo, 1885 ([560..1880]/1321 with the
     * 1381-frame dump before 0x34E2C's reaction callback 0x3E3A8 (the T-rex's
     * reaction 0x2A at f = 962) and the loop-frame-1970 dump explained
     * 1881..1884; [560..1762]/1203 before 0x39040's
     * combo text 0x38D90/0x38FEC (the "2 HIT COMBO" at f = 860) explained
     * 1763..1880; [560..1749]/1190 before 0x34E2C's reaction callback 0x3C0A4
     * (the T-rex's reversed-facing attack through 0x3BF70 at f = 850)
     * explained 1750..1762; [560..1714]/1155 before the
     * worshipper landing target 0x4AC80 (the climb at f = 820 and 841)
     * explained 1715..1749; [560..1658]/1099 before the fighters' body
     * push 0x3BB90 (0x4FB20/0x3BAEC/0x3B9D8, game_frame's 0x2541D)
     * explained 1659..1714; [560..1562]/1003 before the effects
     * pass's per-entry prelude 0x4B69C (the worshippers' trample: 0x17D30,
     * 0x4B470 and case 6) explained 1563..1658; [560..1545]/986 before the effects
     * pass's worshipper fall/lie/climb cases 3..5 (0x49DB3/0x49E5A/0x49EC0)
     * explained 1546..1562; [560..1480]/921 before 0x349C8's
     * 0x34A8D command gate explained 1481..1545; [560..1477]/918 before the
     * 0x17CB0 projectile collision step explained 1478..1480; [560..842]/283 before the
     * demo-pose 0x3A43C/0x186C4 fix explained captures 843..850,
     * [560..850]/291 before the 0x3AD27 setter-operand fix explained
     * 851..857, [560..857]/298 before the 0x2A690 mode-1 x fix
     * explained 858, [560..858]/299 before the 0x49D2F walk-arrival
     * fix explained 859, [560..859]/300 before the 0x4A634 slot +0x42
     * reset explained 860..863, [560..863]/304 before state 6's 0x12750
     * node list and the frame-counter seed explained 864/865, and
     * [560..865]/306 before 0x36870's case-0 restart of its own record
     * (0x36A8C/0x36AA1) explained 866, and [560..866]/307 before 0x3BDDC's
     * 0x3C480 animation start with the full 0x18714 anchor path explained
     * 867..869, [560..869]/310 before the attack streams' 0xD000 target
     * 0x35E04 and its 0x3BC70 launch explained 870..879, and [560..879]/320
     * before 0x34E2C's reaction callback and the T-rex's 0x3E62C/0x3E524/
     * 0x3E4E4 leap explained 880..890, and [560..890]/331 before the
     * T-rex's +0x1C callback 0x3E4C4 explained 891, and [560..891]/332
     * before the knockback pose's handler 0x39CC8 explained 892..949, and
     * [560..949]/390 before the knockdown floor 0x347B8 explained
     * 950..991, and [560..991]/432 before the walk entry 0x35938
     * explained 992..997, and [560..997]/438 (0 transition) before the
     * worshipper arrival target 0x4AC18 explained 998..1357; its three
     * transition frames lie in that new span, and [560..1357]/798 before
     * the camera split arm's 0x18714 writes explained 1358..1410, and
     * [560..1410]/851 before the T-rex's reaction-0x20 callback 0x3D17C
     * and its 0x3D214/0x3D26C targets explained 1411..1477). The
     * [557..810]/254 text here was
     * stale drift, already flagged in Task 2's review and corrected here; Task
     * 3c's camera-offset fix does not touch it. The arena-backdrop fix (the
     * crowd actor 0's mountain layer, actor_spawn's per-type dispatch) extended
     * the window from [560..830]: the window is derived from the port's own
     * dump, so explaining captures 834..842 at 0 bytes necessarily grows it,
     * and the two new unexplained frames (832, 833) are pre-existing,
     * out-of-scope gaps with named owners, allowed by name in
     * tools/title_compare.py. The window's exhibition set spans port frames
     * 0..1033 (797 exhibited). */
    {
        const char *dir = getenv("PR_GAME_DIR");
        if (dir == NULL || dir[0] == '\0') dir = "data/game/C";
        game_set_game_dir(dir);
        game_init();
        actors_pin_anim_tick_zero(1);

        /* The attract runs before the reference's state 6; this driver skips it
         * by entering at state 2, so re-seed to the attract's post-state. */
        rng_seed(FRONTEND_RNG_AFTER_ATTRACT);
        /* Likewise the frame counter: the attract and the title run before the
         * reference's state 2 (the raw's word at 0xEF6DC). */
        DSW(DS_000EF6DC) = (u16)FRONTEND_FRAMES_BEFORE_STATE2;
        /* Likewise the attract cycle counter (the boot attract's post-state). */
        DSB(DS_000F0A5C) = (u8)FRONTEND_ATTRACT_CYCLE_AFTER_BOOT;

        /* Enter state 2 at phase 0, the entry game_state_title() leaves for. */
        DSW(DS_000F0A64) = 2;
        DSB(DS_000F0A6F) = 0;

        /* Seed the pick bytes so the alignment check below cannot pass on a
         * never-written BSS zero. DS_0010816A[1] must equal the original's BSS
         * value 0 at side 0's 0x41350 call: it is the other side's pick byte
         * then, and only char[0] == char[1] gives the T-rex variant 1 (record
         * §7.1). [0] is overwritten before the variant test, so 0xFF is a
         * sentinel that cannot pass as a never-written BSS zero. */
        DSB(DS_0010816A) = 0xFFu;
        DSB(DS_0010816A + 1u) = 0u;

        mkdir(dump, 0777);      /* ignore EEXIST; matches the frame-dump hook */

        char log_path[1200];
        snprintf(log_path, sizeof log_path, "%s/select.log", dump);
        FILE *log = fopen(log_path, "w");
        CHECK(log != NULL, "state-2 hash log opens");

        const char *cap_s = getenv("PR_FRONTEND_DUMP_FRAMES");
        long raw_cap = cap_s ? strtol(cap_s, NULL, 0) : 1400;
        int dumped = 0;
        int dump_failed = 0;

        u32 seen_entries = 0;
        int reached3 = 0;
        int seen6 = 0;
        u32 entry_lcg = 0;
        int dust_sampled = 0;
        u32 dust_actor[4] = { 0, 0, 0, 0 };
        u32 dust_anim[4] = { 0, 0, 0, 0 };
        u32 dust_type[4] = { 0, 0, 0, 0 };
        u32 dust_e21[4] = { 0xFF, 0xFF, 0xFF, 0xFF };   /* entry+0x21 */
        /* Task 6b: the state-7 fight's chain, sampled per loop frame. The
         * 0x3531C/0x350D0 machine must take slot 0's +0x52 out of 0 through
         * 0x0E; the 9/8 no-op entry (Task 3c) is where the demo-AI's aligned
         * command words leave it. A missing 0x35803 call leaves the counters
         * false/zero. */
        int s7_saw14 = 0, s7_saw09 = 0, s7_hit = 0, s7_last_change = 0;
        /* Task 4: the pose state 0x10/0x0A is measured but NOT asserted. The
         * original's T-rex reaches it at the 6th frame of its 9/8 hold
         * (0x3AAFC -> the 0x3A504/0x3A650/0x3A79C/0x3A8E8 pose family). Task 4
         * found the port could not (its derivation record §10.4). The port
         * reached it once fighter_pass_a's tail 0x193B0 -> 0x3B714 -> 0x3AAFC
         * was ported, and the printed pose10/pose0a were already 1 before the
         * per-slot hook 0x19020 was ported (demo-pose record §35: the dump is
         * byte-identical with it). Measured so the next task can see it,
         * exactly as s7_hit is. */
        int s7_saw10 = 0, s7_saw0a = 0;
        int s7_last = -1;              /* the last loop frame the state is 7 */
        int s7_saw42_40 = 0;           /* the +0x42 bit 6 arm was ever set */
        u8 s7_prev[4] = { 0, 0, 0, 0 };
        /* Task 3b: the first state-7 frame's LCG and the 2nd frame's command.
         * Task 3c: the camera screen offset DS_00100AB0 (0x18540/0x18350) at
         * the 1st frame. */
        u32 s7_entry_pre = 0, s7_entry_post = 0;
        int s7_pre_seen = 0, s7_post_seen = 0;
        u16 s7_cmd0_1072 = 0;
        u16 s7_cmd1_1071 = 0;
        u32 s7_cam0 = 0;
        /* Task 5 / record §7.0/§7.1: the Gate's second claim — the T-rex
         * character palette's DAC range, sampled from the palette table while
         * the state is 7. Sentinel 0 differs from the post-condition 142. */
        u32 s7_pal_start = 0, s7_pal_len = 0;
        /* Demo record §15: the loop frame after which the first type-0x01
         * actor (0x1282C's 0xBB254 flier) is live, and DS_0010150C then (the
         * loop has already advanced it past the frame's own f). Sentinels -1:
         * never seen. */
        int s7_flier_f = -1, s7_flier_i = -1;
        int s7_pal_sampled = 0;
        /* The cycle-2 dump (fe_cyc2_dump) and its samples: the loop the
         * cycle-2 dump starts at (-1 before the exit frame), DS_00107A54 after
         * loop 1971 (the logos' iteration: 0x11000 phase 0's actors_reset),
         * the first cycle-2 loop whose post-state has DS_00104AD0 bit 0 (the
         * lightning stream's 0x4F83C) and the first whose post-state is 6,
         * with DS_000F0A72 then. Sentinels -1 differ from every value
         * asserted below. */
        snprintf(fe_cyc2_dir, sizeof fe_cyc2_dir, "%s/cycle2", dump);
        mkdir(fe_cyc2_dir, 0777);
        fe_cyc2_on = 0;
        fe_cyc2_n = 0;
        fe_cyc2_failed = 0;
        movie_set_screen_hook(fe_cyc2_dump);
        res_set_screen_hook(fe_cyc2_loader);
        fe_ld_n = 0;
        /* Record §45-A: the sound banks' first reads. The first demo's
         * state-6 loop (1070) reads s16rexsd (60), s16sound (5) and s16cobsd
         * (36) through the two spawns' tails (bits 0..2, sampled after loops
         * 1069 and 1070); the second demo's (its handler runs in loop 2783)
         * s16konsd (54, after 2782 and 2783); loop 3557's voice 0x4D s16spisd
         * (64, after 3556 and 3557). -1: never sampled. */
        int sd1[2] = { -1, -1 }, sd2[2] = { -1, -1 }, sd64[2] = { -1, -1 };
        /* Record §47-A: the third demo's state-6 entry. Its first reads,
         * sampled after loops 3984 and 3985 (s16caves 22, s16dia 37, s16diash
         * 39, s16diasd 42, s16spi 61, s16spish 62 as bits 0..5), and the first
         * loop after the second demo's exit (3684) that starts in state 6,
         * with the cycle-2 index its presented frame is written at. -1: never
         * sampled. */
        int sd3[2] = { -1, -1 }, c2_s6c_i = -1, c2_s6c_f = -1;
        int c2_start = -1, c2_proj54 = -1, c2_flash_i = -1;
        int c2_state6_i = -1, c2_f0a72 = -1;
        /* The live-fighter count DS_001078FA after the second demo's 6 -> 7
         * loop (record §38). -1: never sampled. */
        int c2_state7_i = -1, c2_fa = -1;
        /* The first second-demo loop whose post-state has side 0 (the
         * raptor) in the block state +0x52 = 6 (0x1A7CC; record §38). */
        int c2_block_i = -1;
        /* The ape's (side 1) record stream and rec+0x52 after loop 3040 (the
         * poll's f = 3927). Sentinels that no stream takes. */
        u32 c2_ape_st = 0xFFFFFFFFu, c2_ape_52 = 0xFFu;
        /* The raptor's (side 0) record stream, slot state +0x52/+0x53/+0x54
         * and +0x18 hook after loop 3116 (the poll's f = 4003), and its
         * record stream after loop 3133 (f = 4020). Sentinels. */
        u32 c2_rap_st = 0xFFFFFFFFu, c2_rap_state = 0xFFFFFFFFu;
        u32 c2_rap_hook = 0xFFFFFFFFu, c2_rap_miss = 0xFFFFFFFFu;
        /* The raptor's record stream, slot +0x52/+0x53/+0x54/+0x41 and
         * DS_00100AB0 after loop 3293 (the poll's f = 4180). Sentinels. */
        u32 c2_ret_st = 0xFFFFFFFFu, c2_ret_state = 0xFFFFFFFFu;
        u32 c2_ret_ab0 = 0x12345678u;
        /* Record §44-A. The raptor's record stream, slot +0x52/+0x53/+0x54/
         * +0x57 and +0x0C/+0x18/+0x1C after loop 3422 (f = 4309) and loop
         * 3551 (f = 4438); its +0x4B child index, that child's stream and
         * +0x14 after loop 3431 (f = 4318) and 3557 (f = 4444); the first
         * type-0x0A node's actor +0x34 after loop 3538 (f = 4425), its phase
         * and stream after loop 3547, and the in-use node list's head after
         * loop 3631. Sentinels. */
        u32 c2_r25_st = 0xFFFFFFFFu, c2_r25_state = 0xFFFFFFFFu;
        u32 c2_r25_cb[3] = { 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu };
        u32 c2_r24_st = 0xFFFFFFFFu, c2_r24_state = 0xFFFFFFFFu;
        u32 c2_r24_cb[3] = { 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu };
        u32 c2_k25[3] = { 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu };
        u32 c2_k24[2] = { 0xFFFFFFFFu, 0xFFFFFFFFu };
        u32 c2_bl34 = 0xFFFFFFFFu, c2_bl_ph = 0xFFFFFFFFu;
        u32 c2_bl_st = 0xFFFFFFFFu, c2_bl_head = 0xFFFFFFFFu;
        /* The ape's record stream and slot +0x52/+0x53/+0x54 after loop 3567
         * (f = 4454), the frame after 0x151A0's 0x3B714. Sentinels. */
        u32 c2_hit_st = 0xFFFFFFFFu, c2_hit_state = 0xFFFFFFFFu;
        /* The picks, variant and handle as the first demo left them, read
         * where the driver's run used to end (loop FE_DEMO_LOOPS - 1); the
         * second demo's state 6 draws new picks. 0xFF/0 sentinels. */
        u32 end_pick0 = 0xFFu, end_pick1 = 0xFFu, end_variant = 0xFFu;
        u32 end_handle = 0;
        for (int i = 0; i < FE_LOOPS; i++) {
            /* The state-9 exit leaves DS_000F0A64 == 6 for the next iteration;
             * nothing draws between the hold and the state-6 handler, so this
             * is the state-6 entry's LCG state. */
            if (!seen6 && DSW(DS_000F0A64) == 6u) {
                entry_lcg = DSD(DS_000EF6D8);
                seen6 = 1;
            }
            /* Task 3b: the first state-7 frame's LCG, before and after its
             * game_loop. The state-7 entry frame leaves the LCG at the
             * original's 0x8612D6C5; the next frame draws the demo-AI picks and
             * the type-0 effect handler's four draws, so the original reaches
             * 0x10F7DB07. A port that skips the type-0 handler stops at
             * 0xB45CD1BB (two draws). */
            if (!s7_pre_seen && DSW(DS_000F0A64) == 7u) {
                s7_entry_pre = DSD(DS_000EF6D8);
                s7_pre_seen = 1;
            }
            DSB(DS_000A81A8) = 1;          /* exactly one game_loop iteration */
            u16 state_in = DSW(DS_000F0A64);   /* the state this frame starts in */
            fe_loop_i = i;
            game_loop();
            if (i == 1069 || i == 1070)
                sd1[i - 1069] = fe_entry_read(60u) | fe_entry_read(5u) << 1
                              | fe_entry_read(36u) << 2;
            if (i == 2782 || i == 2783) sd2[i - 2782] = fe_entry_read(54u);
            if (i == 3556 || i == 3557) sd64[i - 3556] = fe_entry_read(64u);
            if (i == 3984 || i == 3985)
                sd3[i - 3984] = fe_entry_read(22u) | fe_entry_read(37u) << 1
                              | fe_entry_read(39u) << 2 | fe_entry_read(42u) << 3
                              | fe_entry_read(61u) << 4 | fe_entry_read(62u) << 5;
            if (i > 3684 && c2_s6c_i < 0 && state_in == 6u) {
                c2_s6c_i = i;
                c2_s6c_f = fe_cyc2_n;
            }
            if (s7_pre_seen && !s7_post_seen) {
                s7_entry_post = DSD(DS_000EF6D8);
                s7_post_seen = 1;
            }
            if (i == 1072) s7_cmd0_1072 = DSW(DS_001088E0);
            if (i == 1071) s7_cmd1_1071 = DSW(DS_001088E2);
            if (i == 1071) s7_cam0 = DSD(0x00100AB0u);
            /* The dust entries exist from the state-6 frame on; sample them
             * before the state-7 frames advance their animations, and read the
             * actor's fields now — later state transitions run actors_reset
             * (0x2BAF4), which zeroes the record pool the pointers point into. */
            if (seen6 && !dust_sampled) {
                u32 n = 0;
                for (u32 e = DSD(DS_0010884C); e != DS_0010884C && n < 4u;
                     e = DSD(e), n++) {
                    u32 actor = DSD(e + 8u);
                    dust_actor[n] = actor;
                    dust_e21[n] = DSB(e + 0x21u);
                    if (actor != 0) {
                        dust_anim[n] = DSD(actor + 8u);
                        dust_type[n] = DSB(actor + 0x48u);
                    }
                }
                dust_sampled = 1;
            }
            /* The measurements below keep the first demo's window, loops
             * 0..FE_DEMO_LOOPS-1: the second demo (from loop 2782) must not
             * satisfy or overwrite them. */
            if (i < FE_DEMO_LOOPS && DSW(DS_000F0A64) == 3u) reached3 = 1;
            if (i < FE_DEMO_LOOPS && DSB(DS_000F0A6E) < 6u)
                seen_entries |= 1u << DSB(DS_000F0A6E);
            if (i == FE_DEMO_LOOPS - 1) {
                end_pick0 = DSB(DS_0010816A);
                end_pick1 = DSB(DS_0010816A + 1u);
                end_variant = DSB(DS_00105B34);
                end_handle = DSD(0xA8A28u + DSB(DS_00105B34) * 4u);
            }
            if (i < FE_DEMO_LOOPS && DSW(DS_000F0A64) == 7u) {
                /* The Gate's second claim: the T-rex character palette's DAC
                 * range. The palette table (DS_00107618, 0x10-byte entries
                 * {handle; rc; start; len}) is populated by the arena's spawn
                 * and drained at the state transitions, so sample the entry
                 * while the state is 7, before actors_reset (0x2BAF4) clears
                 * the pool. Record §1.2/§1.3: the entry is start=142 len=31 in
                 * both the original and the port. The variant selects the
                 * handle (0x1BB9FD58 variant 0 / 0x1BB9FCD8 variant 1, the
                 * driver's), not the range — so the range is the invariant
                 * claim 2 names. */
                if (!s7_pal_sampled) {
                    const u32 handle = DSD(0xA8A28u + DSB(DS_00105B34) * 4u);
                    for (u32 e = DS_00107618; e < DS_00107798; e += 0x10u) {
                        if (DSD(e) == handle) {
                            s7_pal_start = DSD(e + 8u);
                            s7_pal_len = DSD(e + 12u);
                        }
                    }
                    s7_pal_sampled = 1;
                }
                if (s7_flier_f < 0) {
                    for (u32 r = actor_list_head(); r != 0; r = actor_next(r)) {
                        if (DSB(r + 0x48u) == 0x01u) {
                            s7_flier_f = (int)DSD(DS_0010150C);
                            s7_flier_i = i;
                            break;
                        }
                    }
                }
                u8 s0 = DSB(DS_001077B0 + 0x52u);
                u8 s1 = DSB(DS_001077B0 + 0x94u + 0x52u);
                s7_last = i;
                if (s0 == 0x0Eu) s7_saw14 = 1;
                if (s0 == 9u) s7_saw09 = 1;
                if (s0 == 0x10u || s1 == 0x10u) s7_saw10 = 1;
                if (DSB(DS_001077B0 + 0x53u) == 0x0Au
                        || DSB(DS_001077B0 + 0x94u + 0x53u) == 0x0Au)
                    s7_saw0a = 1;
                if (DSB(DS_001077B0 + 0x7Cu) != 0u
                        || DSB(DS_001077B0 + 0x94u + 0x7Cu) != 0u) s7_hit = 1;
                if (i > 0 && (s0 != s7_prev[0] || s1 != s7_prev[1]))
                    s7_last_change = i;
                s7_prev[0] = s0; s7_prev[1] = s1;
                /* Task 3: record whether 0x349C8's +0x42 bit 6 (the 0x349E6
                 * arm) is ever set in the state-7 window. This measures only
                 * that bit; it does not settle 0x37178's reachability, whose
                 * other entry is 0x37A4F (0x36870's case-4 body). */
                if ((DSB(DS_001077B0 + 0x42u) & 0x40u) != 0u
                        || (DSB(DS_001077B0 + 0x94u + 0x42u) & 0x40u) != 0u)
                    s7_saw42_40 = 1;
            }
            if (log != NULL) {
                /* Hash DS_000E87A4 read after game_loop: after the swap that
                 * is the next draw buffer, not the just-presented one (which
                 * is DS_000E87A0, the one the dump below writes). */
                const u8 *fb = mem + DSD(DS_000E87A4);
                u32 h = 2166136261u;
                for (u32 b = 0; b < 320u * 200u; b++) h = (h ^ fb[b]) * 16777619u;
                fprintf(log, "%d %u %u\n", i, (unsigned)DSB(DS_000F0A6F), h);
            }
            /* Once state 3 is reached, write the just-presented frame as RGB24
             * (192000 bytes, 320x200) through gfx_dac, the same form the title
             * and attract hooks write. swap_buffers() has already run, so the
             * just-presented buffer is DS_000E87A0 (the hook reads DS_000E87A4
             * before the swap). A frame counts as dumped only when all 192000
             * bytes were written, so the count below cannot pass on a short or
             * missing file; one failure stops further attempts. */
            if (fe_cyc2_on) {
                fe_cyc2_dump();
            } else if ((state_in >= 3u || DSW(DS_000F0A64) >= 3u) &&
                       !dump_failed && dumped < (int)raw_cap) {
                char path[1300];
                snprintf(path, sizeof path, "%s/frame_%04d.raw", dump, dumped);
                palette_dump_frame_marker(dumped);
                int ok = fe_write_frame(path);
                CHECK(ok, "front-end frame writes to the dump");
                if (ok) dumped++;
                else dump_failed = 1;
            }
            /* The exit frame (an iteration that starts in state >= 3 and ends
             * below it: loop 1970) closes the front-end dump; every later
             * presented frame goes to the cycle-2 dump. */
            if (!fe_cyc2_on && state_in >= 3u && DSW(DS_000F0A64) < 3u) {
                fe_cyc2_on = 1;
                c2_start = i + 1;
            }
            if (i == 1971) c2_proj54 = (int)DSB(DS_00107A54);
            if (c2_start > 0 && i >= c2_start) {
                if (c2_flash_i < 0 && (DSB(DS_00104AD0) & 1u) != 0u)
                    c2_flash_i = i;
                if (c2_state6_i < 0 && DSW(DS_000F0A64) == 6u) {
                    c2_state6_i = i;
                    c2_f0a72 = (int)DSB(DS_000F0A72);
                }
                if (c2_state6_i >= 0 && c2_state7_i < 0
                        && DSW(DS_000F0A64) == 7u) {
                    c2_state7_i = i;
                    c2_fa = (int)DSB(DS_001078FA);
                }
                if (c2_state7_i >= 0 && c2_block_i < 0
                        && DSB(DS_001077B0 + 0x52u) == 6u)
                    c2_block_i = i;
                if (i == 3040) {
                    u32 rec1 = DSD(DS_001077B0 + 0x94u);
                    c2_ape_st = DSD(rec1 + 8u);
                    c2_ape_52 = DSB(rec1 + 0x52u);
                }
                if (i == 3116) {
                    u32 sl0 = DS_001077B0;
                    c2_rap_st = DSD(DSD(sl0) + 8u);
                    c2_rap_state = (u32)DSB(sl0 + 0x52u) << 16
                                 | (u32)DSB(sl0 + 0x53u) << 8
                                 | DSB(sl0 + 0x54u);
                    c2_rap_hook = DSD(sl0 + 0x18u);
                }
                if (i == 3133) c2_rap_miss = DSD(DSD(DS_001077B0) + 8u);
                if (i == 3422 || i == 3551) {
                    u32 sl0 = DS_001077B0;
                    u32 *cb = (i == 3422) ? c2_r25_cb : c2_r24_cb;
                    u32 stt = (u32)DSB(sl0 + 0x52u) << 24
                            | (u32)DSB(sl0 + 0x53u) << 16
                            | (u32)DSB(sl0 + 0x54u) << 8
                            | DSB(sl0 + 0x57u);
                    if (i == 3422) {
                        c2_r25_st = DSD(DSD(sl0) + 8u);
                        c2_r25_state = stt;
                    } else {
                        c2_r24_st = DSD(DSD(sl0) + 8u);
                        c2_r24_state = stt;
                    }
                    cb[0] = DSD(sl0 + 0x0Cu);
                    cb[1] = DSD(sl0 + 0x18u);
                    cb[2] = DSD(sl0 + 0x1Cu);
                }
                if (i == 3431 || i == 3557) {
                    u32 ix = DSB(DSD(DS_001077B0) + 0x4Bu);
                    u32 ch = DSD(DS_001014F4) + ix * 0x68u;
                    u32 *k = (i == 3431) ? c2_k25 : c2_k24;
                    k[0] = ix;
                    k[1] = DSD(ch + 8u);
                    if (i == 3431) c2_k25[2] = DSD(ch + 0x14u);
                }
                if (i == 3538) c2_bl34 = DSW(DSD(0x00104780u + 8u) + 0x34u);
                if (i == 3547) {
                    c2_bl_ph = DSB(0x00104780u + 0x0Cu);
                    c2_bl_st = DSD(DSD(0x00104780u + 8u) + 8u);
                }
                if (i == 3631) c2_bl_head = DSD(0x00104880u);
                if (i == 3567) {
                    u32 sl1 = DS_001077B0 + 0x94u;
                    c2_hit_st = DSD(DSD(sl1) + 8u);
                    c2_hit_state = (u32)DSB(sl1 + 0x52u) << 16
                                 | (u32)DSB(sl1 + 0x53u) << 8
                                 | DSB(sl1 + 0x54u);
                }
                if (i == 3293) {
                    u32 sl0 = DS_001077B0;
                    c2_ret_st = DSD(DSD(sl0) + 8u);
                    c2_ret_state = (u32)DSB(sl0 + 0x52u) << 24
                                 | (u32)DSB(sl0 + 0x53u) << 16
                                 | (u32)DSB(sl0 + 0x54u) << 8
                                 | DSB(sl0 + 0x41u);
                    c2_ret_ab0 = DSD(DS_00100AB0);
                }
            }
        }
        movie_set_screen_hook(NULL);
        res_set_screen_hook(NULL);
        printf("test_frontend: sound banks @1069/1070 %d/%d, @2782/2783 %d/%d, "
               "@3556/3557 %d/%d; loader screens %d:", sd1[0], sd1[1], sd2[0],
               sd2[1], sd64[0], sd64[1], fe_ld_n);
        for (int k = 0; k < fe_ld_n && k < 16; k++)
            printf(" %d@%d", fe_ld_loop[k], fe_ld_frame[k]);
        printf("\n");
        /* Record §47-A: the loader screens' cycle-2 indices, one per line, for
         * title_compare --attract2's three-frame splice (a catch-up present
         * after the loader's stall). */
        {
            char lpath[1400];
            snprintf(lpath, sizeof lpath, "%s/loader.txt", fe_cyc2_dir);
            FILE *lf = fopen(lpath, "w");
            int lok = lf != NULL;
            for (int k = 0; lok && k < fe_ld_n && k < 16; k++)
                if (fprintf(lf, "%d\n", fe_ld_frame[k]) < 0) lok = 0;
            if (lf != NULL && fclose(lf) != 0) lok = 0;
            CHECK(lok, "the loader-screen list writes to the cycle-2 dump");
        }
        fe_cyc2_on = 0;
        if (log != NULL) fclose(log);
        printf("test_frontend: cycle 2 from loop %d, %d frames; A54@1971 %d, "
               "4F83C@%d, state 6@%d (F0A72 %d), state 7@%d (78FA %d), "
               "block@%d, ape@3040 %05X/%u, raptor@3116 %05X/%06X/%05X, "
               "@3133 %05X, @3293 %05X/%08X/%08X\n", c2_start, fe_cyc2_n,
               c2_proj54,
               c2_flash_i, c2_state6_i, c2_f0a72, c2_state7_i, c2_fa,
               c2_block_i, (unsigned)c2_ape_st, (unsigned)c2_ape_52,
               (unsigned)c2_rap_st, (unsigned)c2_rap_state,
               (unsigned)c2_rap_hook, (unsigned)c2_rap_miss,
               (unsigned)c2_ret_st, (unsigned)c2_ret_state,
               (unsigned)c2_ret_ab0);
        printf("test_frontend: @3422 %05X/%08X/%X/%X/%X, @3431 %02X/%05X/%X, "
               "@3538 %04X, @3547 %u/%05X, @3551 %05X/%08X/%X/%X/%X, "
               "@3557 %02X/%05X, @3567 %05X/%06X, @3631 %X\n",
               (unsigned)c2_r25_st, (unsigned)c2_r25_state,
               (unsigned)c2_r25_cb[0], (unsigned)c2_r25_cb[1],
               (unsigned)c2_r25_cb[2], (unsigned)c2_k25[0],
               (unsigned)c2_k25[1], (unsigned)c2_k25[2], (unsigned)c2_bl34,
               (unsigned)c2_bl_ph, (unsigned)c2_bl_st, (unsigned)c2_r24_st,
               (unsigned)c2_r24_state, (unsigned)c2_r24_cb[0],
               (unsigned)c2_r24_cb[1], (unsigned)c2_r24_cb[2],
               (unsigned)c2_k24[0], (unsigned)c2_k24[1],
               (unsigned)c2_hit_st, (unsigned)c2_hit_state,
               (unsigned)c2_bl_head);

        /* The raw's timeline: six entries, each drawn then paused, then state 3.
         * Entry k is drawn on frame 1+93k; after the sixth, 0x1E pause frames
         * and the phase-3 handoff land state 3 on frame 589 (the arithmetic is
         * in docs/superpowers/plans/2026-09-19-frontend-input-derivations.md).
         * State 3 now runs its 0x12484 phases and hands off to state 9, so the
         * window is asserted to reach state 3, not to end in it; the state it
         * ends in is whatever 0x12658's actor timing produces. The demo adds
         * states 9/6/7: state 7 runs its 900-frame timer and 0x11BCC's exit
         * lands at loop frame 1970, an iteration that starts in state 7, so
         * its presented frame is dumped: loop frames 589..1970 (1382 frames,
         * dump 0..1381). The 1400 cap covers it; FE_DEMO_LOOPS clears the
         * 1970 exit, and the second demo's states 6/7 (loop 2782 on) go to the
         * cycle-2 dump, not here. */
        CHECK_EQ_INT((int)seen_entries, 0x3F);
        CHECK(reached3, "the window reaches state 3");
        CHECK_EQ_INT(dumped, (int)(raw_cap < 1382 ? raw_cap : 1382));

        /* The second attract cycle (derivation record §36). The cycle-2 dump
         * starts after the exit frame, at loop 1971, and holds 2095 frames:
         * loops 1971..3899 (1929) and, inside loop 1971, the 166 screens
         * 0x1C740 writes (TWI5: the entry blank, 121 frames, the exit blank;
         * TWG: the same with 41). A player that drops TWI5's 121st frame
         * writes 2094; one with no screen hook 1929. 0x2BAF4's 0x4F228 call
         * (0x2BBC4) clears the projection gate DS_00107A54 the demo's state 6
         * set (0x387E2), so it is 0 after loop 1971 (1 without it). The
         * lightning stream 0xE890A's 0x4F83C sets DS_00104AD0 bit 0 first at
         * loop 2547 (never without its registration). With the boot attract's
         * cycle counter (FRONTEND_ATTRACT_CYCLE_AFTER_BOOT), phase 0xB hands off
         * to state 6 at loop 2782 with DS_000F0A72 = 5 (0x1150E); with the
         * counter left at 4 it hands off to the title and never reaches 6.
         * Record §38: state 6's 0x20DF4 zeroes the live-fighter count
         * DS_001078FA through 0x34978 (0x20E42) before its two spawns, so the
         * second demo's 6 -> 7 loop (2783) leaves it at 2; without the reset
         * the first demo's 2 grows to 4 and 0x1958C/0x34D8C never run.
         * The raptor blocks the ape's punch at loop 2848 (f = 3735, the
         * DOSBox-X live-RAM poll's first +0x52 = 6 for side 0): 0x1AB5C's arm
         * calls 0x1A7CC; without it the raptor never blocks (-1).
         * Record §39: at loop 3040 (f = 3927) the ape has blocked since
         * f = 3915. 0x3B298 sets its +0x43 bit 0x20 and calls 0x1A734
         * (0x3B443), which restarts 0xC8F40[1] = 0xE3F5E at +0x52 = 0
         * whatever the bit was, as the poll's original does. Without it
         * the stream runs on at 0xE3F66 (rec+0x52 = 2).
         * Record §40: at loop 3116 (f = 4003) the raptor takes character 3's
         * reaction 0x23, whose callback 0x14E44 starts the grab stream
         * 0xD3026 (at 0xD3028 after the frame), state 9/7/0 and the +0x18
         * hook 0x14CC4; unregistered, the raptor stays in its stance. At loop
         * 3133 (f = 4020) the hook returns 1 (no grab) and restarts the
         * record on the miss stream 0xD3062 (at 0xD3068 after the frame),
         * as the poll's original does.
         * Record §41: at loop 3293 (f = 4180) the raptor's reaction stream
         * 0xD24F0 reaches its 0xD500 target 0x3C32C (the dword at 0xD24FE),
         * which clears the slot's +0x54 and runs 0x36870: the record
         * restarts on the stance 0xD2136, the slot is 0/0/0 with +0x41 = 0
         * and DS_00100AB0 = 0xFFFFFF00, as the poll's original. Unregistered,
         * the record runs on at 0xD2500 in 9/8/0.
         * Record §44-A: at loop 3422 (f = 4309) character 3's reaction-0x25
         * callback 0x15350 starts 0xD311A (0xD311C after the frame) in
         * 9/7/0 with +0x57 = 0 and the callbacks 0x152D4/0x15208/0x1527C, as
         * the poll's original; unregistered, the raptor stays in 9/0/0 with
         * +0x57 = 3. At loop 3431 (f = 4318) the stream's 0xD100 target
         * 0x153D8 has spawned 0xBB394's child (actor 0x6A, stream 0xD318E,
         * +0x14 = the raptor's slot). At loop 3538 (f = 4425) the blood
         * particle 0x2901C accepted from the hit effect's opcode-0x0C spawn
         * carries the parent's hflip (a5 = 0x4000, 0x2B4C4), so its +0x34
         * is +0x58 as in the original (the snapshot's actor 106); with the
         * old a5 = 0x40 it is -0x58. The process 0x2910C (entry 7) moves it
         * to phase 1 on 0xE8D14 (0xE8D16 after the frame) by loop 3547 and
         * has returned all seven nodes to the free list by loop 3631. At
         * loop 3551 (f = 4438) the reaction-0x24 callback 0x151C0 starts
         * 0xD3078 (0xD3082) in 9/7/0 with +0x0C = 0 and the hooks
         * 0x15160/0x151A0, and at loop 3557 (f = 4444) its stream's 0xD100
         * target 0x1543C has spawned 0xBB3A8's child (actor 0x6B, stream
         * 0xD30F4). At loop 3566 (f = 4453) its +0x1C callback 0x151A0 runs
         * 0x3B714 on the ape: after loop 3567 the ape is in 0x10/0x0A/2 on
         * 0xE4406, as the poll's original; unregistered, it is in 0x0E/0/0
         * on 0xE3AAC. */
        CHECK(!fe_cyc2_failed, "cycle-2 frames write to the dump");
        CHECK_EQ_INT(c2_start, 1971);
        CHECK_EQ_INT(fe_cyc2_n, 2308);
        /* Record §45-A. The sound banks' first reads: none before, all after
         * each loop (0x33E51's tails, 0x2C3FC(0x4D)). */
        CHECK_EQ_INT(sd1[0], 0);
        CHECK_EQ_INT(sd1[1], 7);
        CHECK_EQ_INT(sd2[0], 0);
        CHECK_EQ_INT(sd2[1], 1);
        CHECK_EQ_INT(sd64[0], 0);
        CHECK_EQ_INT(sd64[1], 1);
        /* Record §47-A. The high-score screen (state 5 at loop 3684, its
         * state-9 hold 0x12C) hands to state 6 at the end of loop 3984; loop
         * 3985 is the third demo's state-6 entry. It reads its six files
         * there, none before, and presents the frame after the last loader
         * screen (cycle-2 frame 2193, the middle of capture 3545's
         * three-frame splice 2192/2193/2194). */
        CHECK_EQ_INT(sd3[0], 0);
        CHECK_EQ_INT(sd3[1], 0x3F);
        CHECK_EQ_INT(c2_s6c_i, 3985);
        CHECK_EQ_INT(c2_s6c_f, 2193);
        /* The thirteen loader screens the cycle-2 dump holds, with the loop
         * that drew each: s16title (loop 1973), the second demo's state-6
         * entry (s16stone, s16kon, s16konsd, s16konsh in loop 2783), s16spisd
         * (loop 3557, capture 3257), s16hghsc (loop 3684) and the third
         * demo's state-6 entry (six screens in loop 3985, record §47-A).
         * 2295 presented frames (loops 1971..4099 and the logo player's 166
         * screens) + 13 = 2308. The first is this driver's, not the port's:
         * the original and the port's own boot both read s16title in the
         * boot attract's phase 2 (0x110D8's palette_acquire(0x396ED28), the
         * first entry-7 resolve; the headless boot reads it at f 3), but this
         * driver enters at state 2 and skips that attract, so its first
         * entry-7 resolve is cycle 2's phase 2 (record §6 of
         * 2026-09-29-e-open-derivations.md). Seeding entry 7's read bit as
         * the boot attract leaves it would drop this screen and shift the
         * cycle-2 dump by one frame (2308 -> 2307, and attract2's named
         * 2192/2193/2194 splice), an oracle-line move not made here. */
        {
            static const int ld_loop[13] = { 1973, 2783, 2783, 2783, 2783, 3557, 3684,
                                             3985, 3985, 3985, 3985, 3985, 3985 };
            static const int ld_frame[13] = { 168, 979, 980, 981, 982, 1757, 1885,
                                              2187, 2188, 2189, 2190, 2191, 2192 };
            CHECK_EQ_INT(fe_ld_n, 13);
            for (int k = 0; k < 13 && k < fe_ld_n; k++) {
                CHECK_EQ_INT(fe_ld_loop[k], ld_loop[k]);
                CHECK_EQ_INT(fe_ld_frame[k], ld_frame[k]);
            }
        }
        /* Capture 3257's screen: loop 3557's loader screen (cycle-2 frame
         * 1757) is the frame presented before it (1756) with the `- LOADING -`
         * text over it: every differing pixel is in the text's box, rows
         * 192..197, columns 0..85. */
        {
            char pa[1400], pb[1400];
            static u8 fa[192000], fb[192000];
            snprintf(pa, sizeof pa, "%s/frame_%04d.raw", fe_cyc2_dir, 1756);
            snprintf(pb, sizeof pb, "%s/frame_%04d.raw", fe_cyc2_dir, 1757);
            FILE *xa = fopen(pa, "rb"), *xb = fopen(pb, "rb");
            int ok = xa != NULL && xb != NULL
                  && fread(fa, 1, sizeof fa, xa) == sizeof fa
                  && fread(fb, 1, sizeof fb, xb) == sizeof fb;
            if (xa != NULL) fclose(xa);
            if (xb != NULL) fclose(xb);
            CHECK(ok, "cycle-2 frames 1756/1757 read back");
            int in_box = 0, out_box = 0;
            for (int px = 0; ok && px < 64000; px++) {
                if (memcmp(fa + px * 3, fb + px * 3, 3) == 0) continue;
                int y = px / 320, x = px % 320;
                if (y >= 192 && y <= 197 && x <= 85) in_box++;
                else out_box++;
            }
            CHECK(in_box > 0, "the loader text is over frame 1756");
            CHECK_EQ_INT(out_box, 0);
        }
        CHECK_EQ_INT(c2_proj54, 0);
        CHECK_EQ_INT(c2_flash_i, 2547);
        CHECK_EQ_INT(c2_state6_i, 2782);
        CHECK_EQ_INT(c2_f0a72, 5);
        CHECK_EQ_INT(c2_state7_i, 2783);
        CHECK_EQ_INT(c2_fa, 2);
        CHECK_EQ_INT(c2_block_i, 2848);
        CHECK_EQ_INT((int)c2_ape_st, 0x000E3F5E);
        CHECK_EQ_INT((int)c2_ape_52, 0);
        CHECK_EQ_INT((int)c2_rap_st, 0x000D3028);
        CHECK_EQ_INT((int)c2_rap_state, 0x090700);
        CHECK_EQ_INT((int)c2_rap_hook, 0x00014CC4);
        CHECK_EQ_INT((int)c2_rap_miss, 0x000D3068);
        CHECK_EQ_INT((int)c2_ret_st, 0x000D2136);
        CHECK_EQ_INT((int)c2_ret_state, 0x00000000);
        CHECK_EQ_INT((int)c2_ret_ab0, (int)0xFFFFFF00u);
        CHECK_EQ_INT((int)c2_r25_st, 0x000D311C);
        CHECK_EQ_INT((int)c2_r25_state, 0x09070000);
        CHECK_EQ_INT((int)c2_r25_cb[0], 0x000152D4);
        CHECK_EQ_INT((int)c2_r25_cb[1], 0x00015208);
        CHECK_EQ_INT((int)c2_r25_cb[2], 0x0001527C);
        CHECK_EQ_INT((int)c2_k25[0], 0x6A);
        CHECK_EQ_INT((int)c2_k25[1], 0x000D318E);
        CHECK_EQ_INT((int)c2_k25[2], (int)DS_001077B0);
        CHECK_EQ_INT((int)c2_bl34, 0x0058);
        CHECK_EQ_INT((int)c2_bl_ph, 1);
        CHECK_EQ_INT((int)c2_bl_st, 0x000E8D16);
        CHECK_EQ_INT((int)c2_r24_st, 0x000D3082);
        CHECK_EQ_INT((int)c2_r24_state, 0x09070000);
        CHECK_EQ_INT((int)c2_r24_cb[0], 0);
        CHECK_EQ_INT((int)c2_r24_cb[1], 0x00015160);
        CHECK_EQ_INT((int)c2_r24_cb[2], 0x000151A0);
        CHECK_EQ_INT((int)c2_k24[0], 0x6B);
        CHECK_EQ_INT((int)c2_k24[1], 0x000D30F4);
        CHECK_EQ_INT((int)c2_hit_st, 0x000E4406);
        CHECK_EQ_INT((int)c2_hit_state, 0x100A02);
        CHECK_EQ_INT((int)c2_bl_head, 0x00104880);

        /* Alignment: the driver's state-6 entry sits at the attract's
         * post-state, and the two picks drawn from it (plus the dust builder's
         * six intermediate draws) are the capture's characters, 0xC835A[0] = 0
         * and 0xC835A[3] = 3. A reverted re-seed leaves entry_lcg at the seed
         * (or 0); a dropped dust draw or a restored master-loop draw changes
         * the characters. */
        CHECK(seen6, "the driver reaches state 6");
        CHECK_EQ_INT((int)entry_lcg, (int)FRONTEND_RNG_AFTER_ATTRACT);
        CHECK_EQ_INT((int)end_pick0, 0);
        CHECK_EQ_INT((int)end_pick1, 3);
        /* Record §7.1: the seed's consequence — the T-rex's variant is 1 and
         * its handle 0x1BB9FCD8 (0xA8A28[1]). The old 0xFF seed left variant 0
         * and 0x1BB9FD58, so this fails under that mutation. */
        CHECK_EQ_INT((int)end_variant, 1);
        CHECK_EQ_INT((int)end_handle, 0x1BB9FCD8);
        /* The Gate's second claim: the T-rex's character palette lands at the
         * raw's DAC range (record §1.2/§1.3, entry 6: start=142 len=31 in both
         * trees). A palette table that assigns a different range — e.g. a
         * changed acquisition order, or a `count` from the wrong resource —
         * fails here. The handle 0x1BB9FD58 the spec's Verification names is
         * the pre-fix variant-0 handle at the same range; the driver's fixed
         * seed (variant 1) acquires the original's 0x1BB9FCD8 (record §1.4). */
        CHECK_EQ_INT((int)s7_pal_start, 142);
        CHECK_EQ_INT((int)s7_pal_len, 31);

        /* Task 6b/Task 3c: the state-7 fight. slot+0x52 enters the 0x34B14
         * no-op 0x0E and the 0x3531C/0x350D0 machine drives it to the 9/8
         * no-op (the demo-AI's aligned command word 0x4848, Task 3c). A missing
         * 0x35803 call leaves +0x52 at 0x0E. Task 4 corrected the residual: the
         * slot's exit from 9/8 to the pose state 0x10/0x0A is the
         * 0x1958C -> 0x193B0 -> 0x3B714 -> 0x3AAFC -> pose-family chain, which
         * the port now runs (0x19020 included, record §35). Task 1 §3.3's
         * "the animation cursor differs" is stale: the port's
         * raptor cursor now matches (0xD2316 at the 9/8 entry) and its
         * silhouette matches capture 834. The pose state is measured by
         * s7_saw10/s7_saw0a, not asserted (Task 4 record §10). */
        CHECK(s7_saw14, "state-7 slot 0's +0x52 enters the 0x0E no-op");
        CHECK(s7_saw09, "state-7 slot 0's +0x52 reaches the 9/8 no-op");
        printf("test_frontend: state-7 last +0x52 change at loop frame %d\n",
               s7_last_change);
        /* Task 3b: the entry draw count. Both trees hold LCG 0x8612D6C5 at the
         * first state-7 frame; the original draws six in that frame (the two
         * demo-AI picks plus the type-0 effect handler's four) and reaches
         * 0x10F7DB07. The port without the type-0 handler drew two and stopped
         * at 0xB45CD1BB, so the two CHECKs below fail under that mutation. */
        CHECK_EQ_INT((int)s7_entry_pre, (int)0x8612D6C5u);
        CHECK_EQ_INT((int)s7_entry_post, (int)0x10F7DB07u);
        /* Task 3c: the demo-AI block state and both command words. The camera
         * screen offset DS_00100AB0 (0x18540/0x18350) must be applied, or the
         * AI's ai_distance (0x187FC) sees the wrong fighter separation and picks
         * the wrong band: without it cmd1 at the 1st state-7 frame stays 0x0000
         * (s1 stays 0/0) and cmd0 at the 2nd stays 0x1010. With it, the AI
         * block's band/move/step pointer and both command words match the
         * original's: cmd1@1071 = 0x0002, cmd0@1072 = 0x4848. The camera offset
         * at the 1st state-7 frame is the original's measured 0x80 (0xCEB00
         * anchor 2 * 64); the sentinel is 0 (never written), so a skipped
         * 0x18350 fails the cam0 CHECK and a skipped 0x18540 the rest. */
        CHECK_EQ_INT((int)s7_cam0, 0x80);
        CHECK_EQ_INT((int)s7_cmd1_1071, 0x0002);
        CHECK_EQ_INT((int)s7_cmd0_1072, 0x4848);
        printf("test_frontend: task3b entry post-LCG %08x, cmd1@1071 %04x, "
               "cmd0@1072 %04x, cam0@1071 %08x, hit %d, last change %d, "
               "pose10 %d, pose0a %d\n",
               (unsigned)s7_entry_post, (unsigned)s7_cmd1_1071,
               (unsigned)s7_cmd0_1072, (unsigned)s7_cam0,
               s7_hit, s7_last_change, s7_saw10, s7_saw0a);
        /* The Gate's first claim: the fight reaches the state-7 900-frame timer
         * exit. State 7 is entered at loop 1070 and left at 1970 (the timer's
         * 0x11BCC arm), so its last frame is 1969; a fight that stalls earlier
         * (or never leaves) fails. The dump count above (1382, through the
         * exit frame 1970) is the same proof through the presented frames. */
        CHECK_EQ_INT(s7_last, 1969);
        /* Demo record §15: the grey flier of captures 864/865 is 0x1282C's
         * type-0x01 spawn. It is refused unless state 6's 0x20DF4 has built
         * the 0xF0A78 node list (0x12750), and the 0x3F gate opens only on
         * the frame counter's multiples of 64, which the seed above puts at
         * f = 91: loop frame 1097, dumped frame 508, the frame capture 864
         * shows (DS_0010150C reads 93 after it: the tick pair carries the
         * state-6 entry's read stall, res.c; 92 before record §45-A added the
         * spawns' sound-bank reads and re-derived the rate). Without 0x12750
         * no flier is ever live (-1); with the counter unseeded it spawns at
         * f = 81. */
        CHECK_EQ_INT(s7_flier_f, 93);
        CHECK_EQ_INT(s7_flier_i, 1097);
        /* The +0x42 bit 6 measurement: neither slot's bit is set in the
         * state-7 window (checked per frame above), so 0x349C8's 0x349E6 arm
         * is never taken. This asserts only that bit, not 0x37178's
         * reachability (its 0x37A4F entry is not measured here). */
        CHECK_EQ_INT(s7_saw42_40, 0);

        /* The dust descriptors the aligned stream picks. From the state-6
         * entry at FRONTEND_RNG_AFTER_ATTRACT, 0x49388's rng(0x64) draws are
         * 93, 80, 30, 25 in spawn order (P0's two iterations, then draw2, then
         * P1's two) -> 0xC9524 indices 0, 1, 3, 4 through the raw's thresholds
         * (0x493B0..0x493E4). The list is newest-first, so its order is
         * 4, 3, 1, 0; each actor's +8 is its descriptor's first dword
         * (0x2AE7D). A rng(side) picker (always index 4) fails on the second
         * entry. */
        {
            static const u32 order[4] = { 4u, 3u, 1u, 0u };
            /* The entry's +0x21 is the SIDE (0x49626; the raw reads the frame's
             * [ESP+0x8] after 0x2AE14's `RET 0x4` restores ESP). The two
             * side-1 entries (orders 4 and 3) carry 1, the side-0 ones 0.
             * 0x4AAD0 indexes DS_001088B2/DS_0010889E with this byte, so a
             * (u8)y write (the port's old bug) reads DS_001088CB = 4 for the
             * order-0 entry and misroutes it to 0x4B430; the effect frame then
             * draws three, not four. */
            static const u32 want_e21[4] = { 1u, 1u, 0u, 0u };
            for (u32 n = 0; n < 4u; n++) {
                u32 desc = DSD(DS_000C9524 + order[n] * 4u);
                CHECK(dust_actor[n] != 0, "dust entry carries a spawned actor");
                CHECK_EQ_INT((int)dust_anim[n], (int)(DSD(desc) + 2u));
                CHECK_EQ_INT((int)dust_type[n], (int)(0x20u + order[n]));
                CHECK_EQ_INT((int)dust_e21[n], (int)want_e21[n]);
            }
        }
        game_shutdown();
    }
    return g_failures - before;
}

/* The fallback determinism gate: two independent PR_FRONTEND_DUMP runs of the
 * states 3/4 window must write byte-identical frame-hash logs. game_init() may
 * run once per process, so the check re-invokes this binary twice (the two
 * invocations + diff pattern) and compares the two logs. The runner sets
 * PR_FRONTEND_DET to the dump root; the children see only PR_FRONTEND_DUMP. */
int test_frontend_determinism(const char *self)
{
    char root[1024], gdir[1024];
    const char *r = getenv("PR_FRONTEND_DET");
    const char *d = getenv("PR_GAME_DIR");
    snprintf(root, sizeof root, "%s",
             (r != NULL && r[0] != '\0') ? r : "/tmp/pr_frontend_det");
    snprintf(gdir, sizeof gdir, "%s",
             (d != NULL && d[0] != '\0') ? d : "data/game/C");

    /* The children must not inherit this mode or they recurse; drop it from the
     * environment before the re-invocations. root/gdir are copied first because
     * unsetenv invalidates the pointers getenv returned. */
    unsetenv("PR_FRONTEND_DET");
    mkdir(root, 0777);      /* the run1/run2 children only create their leaf */

    for (int k = 1; k <= 2; k++) {
        char cmd[4096], log[1300];
        snprintf(log, sizeof log, "%s/run%d.log", root, k);
        snprintf(cmd, sizeof cmd,
                 "PR_FRONTEND_DUMP='%s/run%d' PR_GAME_DIR='%s' '%s' >'%s' 2>&1",
                 root, k, gdir, self, log);
        CHECK(system(cmd) == 0, "front-end determinism run completes");
    }

    char p1[1300], p2[1300];
    snprintf(p1, sizeof p1, "%s/run1/select.log", root);
    snprintf(p2, sizeof p2, "%s/run2/select.log", root);
    FILE *f1 = fopen(p1, "rb");
    FILE *f2 = fopen(p2, "rb");
    CHECK(f1 != NULL && f2 != NULL, "front-end determinism logs open");
    if (f1 != NULL && f2 != NULL) {
        int same = 1;
        for (;;) {
            int a = fgetc(f1), b = fgetc(f2);
            if (a != b) { same = 0; break; }
            if (a == EOF) break;
        }
        CHECK(same, "two front-end runs' frame hashes are byte-identical");
    }
    if (f1 != NULL) fclose(f1);
    if (f2 != NULL) fclose(f2);
    return g_failures;
}

/* ---- test_attract.c ---- */

/* The small attract units that need no game_init(): the pause/continue tails
 * (0x10DB0/0x10E18), the state reset (0x10EE4), the config volumes (0x2C8F0
 * with eax = -2), the voice/rng scheduler (0x10F28) and the per-bit scene tick
 * (0x292AC). Every expected value is derived from the raw in
 * docs/superpowers/plans/2026-09-19-attract-derivations.md. */







/* The PR_ATTRACT_DUMP continuous-run driver (4d): the real init and master loop,
 * plus the shared title window. */






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

        /* 0x2C8F0(eax = -1), record §42-F: each field halved with `sar 1`
         * (0x2C910/0x2C92D), no 0x2A scale. Scale 0 is seeded so a scaled
         * arm would read 0; m 0xA1 -> 0x50, s 0x41 -> 0x20 (odd, so the
         * halving truncates). */
        config_field_set(0x2Au, 0u);
        config_field_set(0x35u, 0x00A1u);
        config_field_set(0x37u, 0x0041u);
        DSD(DS_000A2CB8) = 0xDEADBEEFu;
        DSD(DS_000A2CB4) = 0xDEADBEEFu;
        attract_config_volumes_unscaled();
        CHECK_EQ_INT((int)DSD(DS_000A2CB8), 0x50);
        CHECK_EQ_INT((int)DSD(DS_000A2CB4), 0x20);

        config_field_set(0x2Au, saved2a);
        config_field_set(0x35u, saved35);
        config_field_set(0x37u, saved37);
        DSD(DS_000A2CB8) = saved_b8;
        DSD(DS_000A2CB4) = saved_b4;
    }

    /* 0x10F28: two signed 16-bit countdowns. Each reload is rng_next(N) + N
     * with N = 0x2D / 0x3C; the second branch also consumes rng_next(2) for the
     * 0xBE/0xBF voice pick, so skipping it would shift the stream. */
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
         * rng_next(2) = 1 (the 0xBE pick; without it the next draw is 52 -> 0x70);
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

    /* 0x4F7F4/0x4F83C/0x33874: the attract palette drivers. The starter 0x4F83C
     * sets DS_00104AD0 bit 0, zeroes DS_001088F1 and enqueues DS_000C98A0[0]
     * through 0x33874 (EAX = the entry DS_000F0A48, EDX = the handle); the
     * advance 0x4F7F4 steps the counter and enqueues DS_000C98A0[counter],
     * clearing bit 0 once the counter reaches 10. The record's recipe seeds
     * DS_000F0A48 = 1, which 0x33874 would treat as a table entry and walk from
     * 0x11 to 0x107798; this test instead uses a scratch descriptor and
     * overrides the array entries with non-resolving sentinels, so 0x33874's
     * else branch runs with count 0 and no loader-presentation side effect. The
     * descriptor's len is 0x7FFFFFFF (signed >= any count) to keep that branch;
     * the raw's compare is signed (`jl`/`jle`). */
    {
        const u32 DESCRIPTOR = 0x3F00000u + 0x100u;
        const u32 HANDLE0 = 0xC98A0u;     /* the ten-handle array, no symbol */
        const u32 ENTRY_LO = 0x107778u;   /* ownership-table entries 22/23 */
        const u32 ENTRY_HI = 0x107788u;
        const u32 SENTINEL_HANDLE = 0xFFFFFFFFu;

        const u32 saved48 = DSD(DS_000F0A48);
        const u8  saved_f1 = DSB(DS_001088F1);
        const u32 saved_ad0 = DSD(DS_00104AD0);
        const u32 saved_h0 = DSD(HANDLE0);
        const u32 saved_h3 = DSD(HANDLE0 + 3u * 4u);
        const u32 saved_h9 = DSD(HANDLE0 + 9u * 4u);
        const u32 saved_head = DSD(DS_00107798);
        u32 saved_rec[4];
        u32 saved_desc[4];
        u32 saved_entry[8];
        for (u32 k = 0; k < 4u; k++) {
            saved_rec[k] = DSD(DS_00107498 + k * 4u);
            saved_desc[k] = DSD(DESCRIPTOR + k * 4u);
        }
        for (u32 k = 0; k < 8u; k++) saved_entry[k] = DSD(ENTRY_LO + k * 4u);

        /* A. 0x4F7F4 advances counter 3 -> 4 and enqueues handle[3]. */
        DSD(DESCRIPTOR + 0u) = 0xDEAD0000u;   /* handle sentinel */
        DSD(DESCRIPTOR + 4u) = 0;
        DSD(DESCRIPTOR + 8u) = 0x1234u;       /* start */
        DSD(DESCRIPTOR + 12u) = 0x7FFFFFFFu;  /* len: else branch */
        DSD(DS_000F0A48) = DESCRIPTOR;
        DSD(HANDLE0 + 3u * 4u) = SENTINEL_HANDLE;
        DSB(DS_001088F1) = 3;
        DSD(DS_00104AD0) = 0xFFFFu;
        DSD(DS_00107798) = DS_00107498;
        attract_palette_advance();
        CHECK_EQ_INT((int)DSB(DS_001088F1), 4);
        CHECK_EQ_INT((int)(DSD(DS_00104AD0) & 1u), 1);   /* still set */
        CHECK_EQ_INT((int)DSD(DESCRIPTOR), (int)SENTINEL_HANDLE);
        CHECK_EQ_INT((int)DSD(DS_00107498 + 0u), (int)SENTINEL_HANDLE);
        CHECK_EQ_INT((int)DSD(DS_00107498 + 4u), 0x1234);
        CHECK_EQ_INT((int)DSD(DS_00107498 + 8u), 0);
        CHECK_EQ_INT((int)DSD(DS_00107498 + 12u), 1);
        CHECK_EQ_INT((int)DSD(DS_00107798), (int)(DS_00107498 + 0x10u));

        /* B. counter 9 -> 10 clears bit 0 (and still enqueues handle[9]). */
        DSD(DESCRIPTOR + 0u) = 0xDEAD0000u;
        DSD(DESCRIPTOR + 12u) = 0x7FFFFFFFu;
        DSD(DS_000F0A48) = DESCRIPTOR;
        DSD(HANDLE0 + 9u * 4u) = SENTINEL_HANDLE;
        DSB(DS_001088F1) = 9;
        DSD(DS_00104AD0) = 0xFFFFu;
        DSD(DS_00107798) = DS_00107498;
        attract_palette_advance();
        CHECK_EQ_INT((int)DSB(DS_001088F1), 10);
        CHECK_EQ_INT((int)(DSD(DS_00104AD0) & 1u), 0);   /* cleared */
        CHECK_EQ_INT((int)DSD(DESCRIPTOR), (int)SENTINEL_HANDLE);
        CHECK_EQ_INT((int)DSD(DS_00107498 + 0u), (int)SENTINEL_HANDLE);

        /* C. entry 0 skips the body and clears bit 0. */
        DSD(DS_000F0A48) = 0;
        DSB(DS_001088F1) = 5;
        DSD(DS_00104AD0) = 0xFFFFu;
        DSD(DS_00107798) = DS_00107498;
        attract_palette_advance();
        CHECK_EQ_INT((int)DSB(DS_001088F1), 5);           /* unchanged */
        CHECK_EQ_INT((int)(DSD(DS_00104AD0) & 1u), 0);
        CHECK_EQ_INT((int)DSD(DS_00107798), (int)DS_00107498);   /* no enqueue */

        /* D. 0x4F83C starts: bit 0 set, counter 1, handle[0] enqueued. */
        DSD(DESCRIPTOR + 0u) = 0xDEAD0000u;
        DSD(DESCRIPTOR + 12u) = 0x7FFFFFFFu;
        DSD(DS_000F0A48) = DESCRIPTOR;
        DSD(HANDLE0) = SENTINEL_HANDLE;
        DSD(DS_00104AD0) = 0;
        DSB(DS_001088F1) = 7;
        DSD(DS_00107798) = DS_00107498;
        attract_palette_start();
        CHECK_EQ_INT((int)(DSD(DS_00104AD0) & 1u), 1);
        CHECK_EQ_INT((int)DSB(DS_001088F1), 1);
        CHECK_EQ_INT((int)DSD(DESCRIPTOR), (int)SENTINEL_HANDLE);
        CHECK_EQ_INT((int)DSD(DS_00107498 + 0u), (int)SENTINEL_HANDLE);
        CHECK_EQ_INT((int)DSD(DS_00107498 + 4u), 0x1234);
        CHECK_EQ_INT((int)DSD(DS_00107498 + 8u), 0);
        CHECK_EQ_INT((int)DSD(DS_00107498 + 12u), 1);

        /* E. entry 0: bit 0 is cleared at function level (0x4F883), counter 0. */
        DSD(DS_000F0A48) = 0;
        DSD(DS_00104AD0) = 0;
        DSB(DS_001088F1) = 7;
        DSD(DS_00107798) = DS_00107498;
        attract_palette_start();
        CHECK_EQ_INT((int)(DSD(DS_00104AD0) & 1u), 0);
        CHECK_EQ_INT((int)DSB(DS_001088F1), 0);
        CHECK_EQ_INT((int)DSD(DS_00107798), (int)DS_00107498);   /* no enqueue */

        /* E2. The lightning stream 0xE890A's opcode-0x11 targets (record §36),
         * registered by actors_init and called as anim_indirect calls them,
         * (rec, operand). 0x4F83C through its wrapper: D's post-state from an
         * operand the raw ignores. 0x10FC4 (`mov dword [eax+0x18],0`) clears
         * only the record's +0x18. actors_init registers both; the shared
         * suite's test_actors runs it, the isolated PR_ATTRACT_DUMP run does
         * not before this check (its pool is absent too). Outside that run
         * the gate must be open, so E2 cannot skip silently. */
        if (getenv("PR_ATTRACT_DUMP") == NULL)
            CHECK(DSD(DS_001014F4) != 0, "E2 runs in the shared suite");
        if (DSD(DS_001014F4) != 0) {
            typedef void (*anim_fn)(u32 rec, u32 arg);
            anim_fn f4f = (anim_fn)(void *)fn_resolve(0x4F83Cu);
            anim_fn f10 = (anim_fn)(void *)fn_resolve(0x10FC4u);
            CHECK(f4f != NULL, "0x4F83C is a registered stream target");
            CHECK(f10 != NULL, "0x10FC4 is a registered stream target");
            if (f4f != NULL) {
                DSD(DESCRIPTOR + 0u) = 0xDEAD0000u;
                DSD(DESCRIPTOR + 12u) = 0x7FFFFFFFu;
                DSD(DS_000F0A48) = DESCRIPTOR;
                DSD(HANDLE0) = SENTINEL_HANDLE;
                DSD(DS_00104AD0) = 0x80u;
                DSB(DS_001088F1) = 7;
                DSD(DS_00107798) = DS_00107498;
                f4f(0x3200000u, 0x1234u);
                CHECK_EQ_INT((int)DSD(DS_00104AD0), 0x81);
                CHECK_EQ_INT((int)DSB(DS_001088F1), 1);
                CHECK_EQ_INT((int)DSD(DS_00107498 + 0u), (int)SENTINEL_HANDLE);
                CHECK_EQ_INT((int)DSD(DS_00107798), (int)(DS_00107498 + 0x10u));
            }
            if (f10 != NULL) {
                const u32 rec = 0x3200000u;
                DSD(rec + 0x14u) = 0x11111111u;
                DSD(rec + 0x18u) = 0x22222222u;
                DSD(rec + 0x1Cu) = 0x33333333u;
                f10(rec, 0xFFFFu);
                CHECK_EQ_INT((int)DSD(rec + 0x18u), 0);
                CHECK_EQ_INT((int)DSD(rec + 0x14u), 0x11111111);
                CHECK_EQ_INT((int)DSD(rec + 0x1Cu), 0x33333333);
            }
        }

        /* F. 0x33874 else branch: handle differs -> re-point + enqueue. */
        DSD(ENTRY_LO + 0u)  = 0xDEAD0000u;
        DSD(ENTRY_LO + 4u)  = 0;
        DSD(ENTRY_LO + 8u)  = 0x2000u;
        DSD(ENTRY_LO + 12u) = 0x7FFFFFFFu;
        DSD(DS_00107798) = DS_00107498;
        palette_reflow(ENTRY_LO, 0xFEED0000u);
        CHECK_EQ_INT((int)DSD(ENTRY_LO), (int)0xFEED0000u);
        CHECK_EQ_INT((int)DSD(DS_00107498 + 0u), (int)0xFEED0000u);
        CHECK_EQ_INT((int)DSD(DS_00107498 + 4u), 0x2000);
        CHECK_EQ_INT((int)DSD(DS_00107498 + 8u), 0);
        CHECK_EQ_INT((int)DSD(DS_00107498 + 12u), 1);

        /* G. 0x33874 else branch: same handle -> no enqueue. */
        DSD(ENTRY_LO + 0u)  = 0xFEED0000u;
        DSD(ENTRY_LO + 12u) = 0x7FFFFFFFu;
        DSD(DS_00107798) = DS_00107498;
        palette_reflow(ENTRY_LO, 0xFEED0000u);
        CHECK_EQ_INT((int)DSD(DS_00107798), (int)DS_00107498);

        /* H. 0x33874 walk branch: len short -> move the next entry's start to
         * prev.start + prev.len and re-enqueue it. */
        DSD(ENTRY_LO + 0u)  = 0xDEAD0000u;
        DSD(ENTRY_LO + 4u)  = 0;
        DSD(ENTRY_LO + 8u)  = 0x2000u;
        DSD(ENTRY_LO + 12u) = 0xFFFFFFFFu;   /* signed -1: len < count */
        DSD(ENTRY_HI + 0u)  = 0xBEEF0000u;
        DSD(ENTRY_HI + 4u)  = 0;
        DSD(ENTRY_HI + 8u)  = 0x1000u;
        DSD(ENTRY_HI + 12u) = 0x10u;
        DSD(DS_00107798) = DS_00107498;
        palette_reflow(ENTRY_LO, SENTINEL_HANDLE);
        CHECK_EQ_INT((int)DSD(ENTRY_HI + 8u), 0x1FFF);   /* 0x2000 + (-1) */
        CHECK_EQ_INT((int)DSD(ENTRY_LO), (int)0xDEAD0000u);    /* unchanged */
        CHECK_EQ_INT((int)DSD(DS_00107498 + 0u), (int)0xBEEF0000u);
        CHECK_EQ_INT((int)DSD(DS_00107498 + 4u), 0x1FFF);
        CHECK_EQ_INT((int)DSD(DS_00107498 + 8u), 0x10);
        CHECK_EQ_INT((int)DSD(DS_00107498 + 12u), 1);

        /* restore */
        DSD(DS_000F0A48) = saved48;
        DSB(DS_001088F1) = saved_f1;
        DSD(DS_00104AD0) = saved_ad0;
        DSD(HANDLE0) = saved_h0;
        DSD(HANDLE0 + 3u * 4u) = saved_h3;
        DSD(HANDLE0 + 9u * 4u) = saved_h9;
        DSD(DS_00107798) = saved_head;
        for (u32 k = 0; k < 4u; k++) {
            DSD(DS_00107498 + k * 4u) = saved_rec[k];
            DSD(DESCRIPTOR + k * 4u) = saved_desc[k];
        }
        for (u32 k = 0; k < 8u; k++) DSD(ENTRY_LO + k * 4u) = saved_entry[k];
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
            /* The attract's LCG state at its handoff phase. The voice tick
             * 0x10F28 (called from 0x11559 while DS_0009AD58 == 0, set at
             * 0x11092) is the attract's only consumer: it draws 26 values over
             * the boot attract, so the state here is seed 0xABCD advanced 26
             * steps (0x4308698B). Sampled at the top of the case-0xB iteration,
             * before that iteration's tail tick; the front-end driver re-seeds
             * to this value (test_frontend.c). */
            u32 handoff_lcg = 0;
            int seen_handoff = 0;
            /* The handoff iteration ends with DS_000F0A64 == 1 but ran the
             * attract; the title entry is the next iteration. */
            while (DSW(DS_000F0A64) != 1 && guard++ < 200000) {
                u8 ph = DSB(DS_000F0A6F);
                if (!seen_handoff && ph == 0xBu) {
                    handoff_lcg = DSD(DS_000EF6D8);
                    seen_handoff = 1;
                }
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

            /* The 26-draw measurement, pinned: the observed handoff state is
             * the derived value and the seed advanced 26 steps. The sentinel 0
             * fails if the phase never ran. */
            CHECK(seen_handoff, "the attract reached its handoff phase");
            CHECK_EQ_INT((int)handoff_lcg, 0x4308698B);
            rng_seed(0xABCDu);
            for (u32 i = 0; i < 26u; i++) (void)rng_next(0u);
            CHECK_EQ_INT((int)DSD(DS_000EF6D8), (int)handoff_lcg);

            /* Demo record §15.3: the frame counter's boot counts that the
             * front-end driver's FRONTEND_FRAMES_BEFORE_STATE2 seed takes from
             * this run. 0x24C5C's word counter has counted the 690 attract
             * iterations here (the title entry is the next iteration); the
             * sentinel is the image's 0, which fails. */
            CHECK_EQ_INT((int)DSW(DS_000EF6DC), 690);

            /* The title window joins the same run (no second game_init()). */
            test_title_window(dump);

            /* Drive the title to its state-2 handoff (0x12459). The counter
             * then holds the iterations before the first state-2 frame, the
             * driver's seed. The title's 96-tick window plus its fade are
             * bounded well inside the guard. These iterations also dump the
             * title's last 100 frames into <dir>/title (196 frames, under the
             * hook's 200 cap); tools/attract_compare.py's output is unchanged
             * by them (compared before/after). */
            {
                int g2 = 0;
                while (DSW(DS_000F0A64) != 2 && g2++ < 2000) {
                    DSB(DS_000A81A8) = 1;
                    game_loop();
                }
                CHECK_EQ_INT((int)DSW(DS_000F0A64), 2);
                CHECK_EQ_INT((int)DSW(DS_000EF6DC),
                             (int)FRONTEND_FRAMES_BEFORE_STATE2);
            }
            game_shutdown();
        }
    }

    return g_failures - before;
}

/* ---- test_title.c ---- */

/* Task 10: the title oracle driver, now running on the continuous 4d boot. The
 * engine init and the master loop run headless, letting Task 9's PR_TITLE_DUMP
 * hook write one RGB24 frame per presented title frame. The pixel comparison
 * lives in tools/title_compare.py (`make title-oracle`); this file owns the
 * dump, its frame count and the entry invariants.
 *
 * The driver is the only test that calls game_init(), so it cannot share a
 * process with the unit suite: a second res_load_index() would exhaust the 64 MB
 * bump allocator. run_tests.c therefore runs it alone when PR_TITLE_DUMP is set,
 * and this file is a no-op in the normal suite. Since 4d, boot enters state 0
 * (attract), so this driver drives the state machine through the attract to the
 * title before its pinned 96-frame window; test_title_window() is the shared
 * window body that test_attract()'s PR_ATTRACT_DUMP run also calls. It reuses
 * the production game_loop() rather than calling game_frame()/render_list()
 * itself, because the master-loop body also flushes the palette, presents the
 * front buffer and swaps it — swap_buffers() is not exported, so only
 * game_loop() reproduces the frame the capture holds. */











/* The title window is defined in master-loop ticks, not in dumped frames: the
 * state-1 handler decrements DS_000F0A66 by 0x10 a tick (0x1230B seeds 0x600,
 * 0x1235x subtracts), so the window ends when it falls below 0x11 after 96
 * ticks. The master loop's tick gate (0x25643) presents only when
 * DS_0010150C == DS_00101508, but the loader's read re-syncs the pair
 * (0x1B45F/0x1B464: DS_0010150C = DS_00101508), so the gate passes on the load
 * frame and on every tick after it: the dump holds one frame per tick. */
#define TITLE_WINDOW_ITERS 96       /* ticks: state-1 entry to DS_000F0A66 < 0x11 */
#define TITLE_PRESENTED_FRAMES 96   /* the gate-passed ticks the dump holds */

static int count_raw(const char *dir)
{
    DIR *d = opendir(dir);
    if (d == NULL) return 0;
    int n = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        size_t l = strlen(e->d_name);
        if (l > 4 && strcmp(e->d_name + l - 4, ".raw") == 0) n++;
    }
    closedir(d);
    return n;
}

/* Drive the state machine until the title state is reached, then its 96-frame
 * window. `dump` is the dump root used for the frame-count invariant. Assumes
 * game_init() and actors_pin_anim_tick_zero(1) already ran. */
int test_title_window(const char *dump)
{
    int before = g_failures;

    /* Record k7-k12 §2: game_init's 0x1C0B1 call to 0x1D0BC ran. The image
     * game_init maps holds 0 at DS_000A2CB0 and at slot 0's +0x10
     * (DS_00102870), and nothing else writes them. */
    CHECK_EQ_INT((int)DSB(DS_000A2CB0), 1);
    CHECK(DSD(DS_00102870) != 0u, "0x1C0B1: slot 0 has its 0x8C00 buffer");

    /* The attract handoff iteration ends with DS_000F0A64 == 1 but ran the
     * attract; the title entry is the next iteration. A bounded drive keeps a
     * broken attract from hanging the run. */
    int guard = 0;
    while (DSW(DS_000F0A64) != 1 && guard++ < 200000) {
        DSB(DS_000A81A8) = 1;   /* exactly one game_loop iteration per call */
        game_loop();
    }
    CHECK_EQ_INT((int)DSW(DS_000F0A64), 1);
    CHECK(guard < 200000, "title state reached within the drive bound");

    /* The capture was made from the pinned original (tools/title_pin.py), which
     * replaces the title's three entry draws with the values the port's own LCG
     * produces from seed 0xABCD (12, 111, 0) and the anim opcode-8 draw with 0.
     * The pin decouples those draws from the RNG state the attract leaves, so
     * the driver re-seeds here to reproduce the pin: the three title draws are
     * again the first three from 0xABCD, exactly as in the pre-attract port.
     * Nothing consumes RNG between this seed and game_state_title's draws (the
     * coin poll, the update table and the scene tick do not draw). */
    rng_seed(0xABCDu);

    /* Record k7-k12 §2: the spin 0x256C5 models the ISR's counter pair
     * (game_isr_ticks), so the 0x500BB clock DS_00101500 advances in every
     * iteration. DS_00101508 alone is not compared: 0x52106 (gfx_screen_reset)
     * stores it, and not DS_00101500, inside the window. */
    u32 isr_still = 0, isr_ticks = 0;
    for (int i = 0; i < TITLE_WINDOW_ITERS; i++) {
        const u32 t00 = DSD(DS_00101500);
        DSB(DS_000A81A8) = 1;   /* exactly one game_loop iteration per call */
        game_loop();            /* update -> render -> present -> dump */
        if (DSD(DS_00101500) == t00) isr_still++;
        isr_ticks += DSD(DS_00101500) - t00;
        if (i == 0) {
            /* State-1 entry (spec DoD #2): the entry frame sets the title
             * countdown to 0x600 and leaves the state machine in state 1. */
            CHECK_EQ_INT((int)DSW(DS_000F0A64), 1);
            CHECK_EQ_INT((int)DSW(DS_000F0A66), 0x600);
        }
    }
    CHECK_EQ_INT((int)isr_still, 0);
    CHECK(isr_ticks >= (u32)TITLE_WINDOW_ITERS, "the clock ticks every iteration");

    /* The window's own boundary, not the tick count: the 96th tick is the one
     * where DS_000F0A66 has just fallen below 0x11. If the timing drifted, the
     * dump would cover the wrong window and this fails. */
    CHECK_EQ_INT((int)DSW(DS_000F0A64), 1);
    CHECK(DSW(DS_000F0A66) < 0x11,
          "window ends at DS_000F0A66 < 0x11 (state-1 entry predicate)");

    /* The window is state 1 for 96 ticks; the dump must hold exactly one RGB24
     * frame per *presented* tick (the hook's cap is 200, so the count is the
     * run's, not the cap's). The loader's re-sync makes the gate pass on the
     * load frame, so every tick in the window presents — see the constants
     * above. */
    {
        char sub[1200];
        snprintf(sub, sizeof sub, "%s/title", dump);
        CHECK_EQ_INT(count_raw(sub), TITLE_PRESENTED_FRAMES);
    }
    /* The window's end invariants: the entry spawned the logo and the phase ran
     * to the release point. A missing logo means the window compared a dead
     * scene. */
    CHECK(DSD(DS_000F0A58) != 0, "title logo record exists after the window");

    return g_failures - before;
}

int test_title(void)
{
    int before = g_failures;
    const char *dump = getenv("PR_TITLE_DUMP");
    if (dump == NULL || dump[0] == '\0') {
        printf("test_title: PR_TITLE_DUMP unset, title dump driver skipped\n");
        return 0;
    }
    const char *dir = getenv("PR_GAME_DIR");
    if (dir == NULL || dir[0] == '\0') dir = "data/game/C";

    game_set_game_dir(dir);
    game_init();
    /* The port's half of Task 1's fourth pin site: the original's anim-stream
     * opcode-8 draw is replaced by `mov eax, 0`, so the interpreter must store 0
     * instead of calling rng_next(). Inert if the title streams never reach it
     * (Task 7's hand-driven walk); the Task 10 report states the measured count.
     * The rng re-seed that reproduces the pin's three title draws lives in
     * test_title_window, after the attract. */
    actors_pin_anim_tick_zero(1);

    test_title_window(dump);
    game_shutdown();

    return g_failures - before;
}

/* ---- record §49-Y: the 0x2Exxx key layer, the timed screen and the menu
 * helpers ---------------------------------------------------------------- */

#define CH_KB      0x03D00000u   /* test-only key-config record (DS_00101514) */
#define CH_BUF_A   0x03D10000u   /* test-only 64000-byte frame buffers */
#define CH_BUF_B   0x03D20000u
#define CH_DEST    0x03D30000u   /* test-only text destination */
#define CH_REC     0x03D40000u   /* test-only option record */
#define CH_KEY_LATCH 0x00105F30u
#define CH_KEY_TIME  0x00105F2Cu
#define CH_KEY_WORD  0x00105F28u
#define CH_TICK      0x00101500u
#define CH_CODE      0x00107450u

static u32 ch_cell(s32 row, s32 col)
{
    return DSD(DS_00105F38 + (u32)row * 0xacu + (u32)col * 4u);
}

/* The glyph's sprite id and its palette-table entry: what a cell is made of. */
static u32 ch_sprite(s32 row, s32 col)
{
    u32 r = ch_cell(row, col);
    return r != 0u ? (u32)(DSW(actor_pset(r)) & 0x7fffu) : 0u;
}

static u32 ch_pal(s32 row, s32 col)
{
    u32 r = ch_cell(row, col);
    return r != 0u ? DSD(actor_pset(r) + 0x18u) : 0u;
}

/* The palette handle 0x2F5A0 picks for a font-0 glyph at `mode` (0x2F5A0's
 * switch over mode & 0xF000, read off actors.c). */
static u32 ch_handle(u32 mode)
{
    switch (mode & 0xF000u) {
    case 0x1000u: return 0x809984u;
    case 0x2000u: return 0x80998Cu;
    case 0x3000u: return 0x809994u;
    case 0x4000u: case 0xF000u: return 0x8099A4u;
    default:      return 0x80997Cu;
    }
}

/* A font-0 cell is `ch` at `mode`: sprite from the table at 0xBCD7C, palette
 * from the handle. */
static void ch_expect(s32 row, s32 col, u32 ch, u32 mode, const char *msg)
{
    CHECK(ch_cell(row, col) != 0u, msg);
    if (ch_cell(row, col) == 0u) return;
    CHECK_EQ_INT((int)ch_sprite(row, col), (int)DSW(0xBCD7Cu + ch * 4u));
    CHECK_EQ_INT((int)ch_pal(row, col), (int)palette_acquire(ch_handle(mode)));
}

static void ch_text_setup(void)
{
    if (res_count() != 69)
        CHECK(res_load_index("data/game/C", "data/game/C/INDEX") == 69,
              "index loaded");
    game_string_table_load("data/game/C");
    actors_reset();
    DSD(DS_00105F34) = 0;
}

static void ch_check_key_name(void)
{
    static const struct { u32 scan; u32 id; } named[] = {
        { 0x0E, 0x22A }, { 0x0F, 0x22B }, { 0x47, 0x220 }, { 0x48, 0x223 },
        { 0x49, 0x221 }, { 0x4B, 0x224 }, { 0x4D, 0x225 }, { 0x4F, 0x227 },
        { 0x50, 0x226 }, { 0x51, 0x222 }, { 0x52, 0x228 }, { 0x53, 0x229 },
    };
    for (u32 i = 0; i < sizeof named / sizeof named[0]; i++) {
        memset(mem + CH_DEST, 0xEE, 0x200);
        char want[0x100];
        strcpy(want, (const char *)game_string_get(named[i].id));
        CHECK(want[0] != 0, "the key-name string exists");
        CHECK_EQ_INT((int)config_key_name(named[i].scan << 8, 1u, CH_DEST), 1);
        CHECK(strcmp((const char *)(mem + CH_DEST), want) == 0,
              "raw flag set: the string as is");
        memset(mem + CH_DEST, 0xEE, 0x200);
        CHECK_EQ_INT((int)config_key_name(named[i].scan << 8, 0u, CH_DEST), 1);
        char wrapped[0x110];
        snprintf(wrapped, sizeof wrapped, "<%s>", want);
        CHECK(strcmp((const char *)(mem + CH_DEST), wrapped) == 0,
              "raw flag clear: the string wrapped in <>");
    }
    /* Distinct names: a swapped id would otherwise pass. */
    char a[0x100], b[0x100];
    strcpy(a, (const char *)game_string_get(0x223u));
    strcpy(b, (const char *)game_string_get(0x226u));
    CHECK(strcmp(a, b) != 0, "up and down names differ");

    static const struct { u32 scan; const char *text; } lit[] = {
        { 0x3D, "F3" }, { 0x3E, "F4" }, { 0x3F, "F5" }, { 0x40, "F6" },
        { 0x41, "F7" }, { 0x42, "F8" }, { 0x43, "F9" }, { 0x44, "F10" },
        { 0x4A, "-" },  { 0x4E, "+" },
    };
    for (u32 i = 0; i < sizeof lit / sizeof lit[0]; i++) {
        memset(mem + CH_DEST, 0xEE, 0x200);
        CHECK_EQ_INT((int)config_key_name(lit[i].scan << 8, 1u, CH_DEST), 1);
        CHECK(strcmp((const char *)(mem + CH_DEST), lit[i].text) == 0,
              "function/sign key literal from the data object");
    }

    /* An ascii key stands as itself: 'a', Esc (0x1B), and 0x7F; a scan code in
     * the high byte does not matter once the ascii byte qualifies. */
    memset(mem + CH_DEST, 0xEE, 0x200);
    CHECK_EQ_INT((int)config_key_name(0x1E61u, 1u, CH_DEST), 1);
    CHECK_EQ_INT((int)DSB(CH_DEST), 'a');
    CHECK_EQ_INT((int)DSB(CH_DEST + 1u), 0);
    memset(mem + CH_DEST, 0xEE, 0x200);
    CHECK_EQ_INT((int)config_key_name(0x1E61u, 0u, CH_DEST), 1);
    CHECK(strcmp((const char *)(mem + CH_DEST), "<a>") == 0, "ascii wrapped");
    memset(mem + CH_DEST, 0xEE, 0x200);
    CHECK_EQ_INT((int)config_key_name(0x011Bu, 1u, CH_DEST), 1);
    CHECK_EQ_INT((int)DSB(CH_DEST), 0x1B);
    memset(mem + CH_DEST, 0xEE, 0x200);
    CHECK_EQ_INT((int)config_key_name(0x487Fu, 1u, CH_DEST), 1);
    CHECK_EQ_INT((int)DSB(CH_DEST), 0x7F);

    /* Not ascii: Enter (0xD) and space (0x20) fall to their scan codes 0x1C and
     * 0x39, which have no name; 0x80 is above 0x7F so its scan code stands
     * (0x0E: the string 0x22A). Unnamed keys leave the destination alone. */
    static const u32 none[] = { 0x1C0Du, 0x3920u, 0x3B00u, 0x3C00u, 0x0100u,
                                0x0000u, 0x4500u, 0x4600u, 0x4C00u, 0x5400u,
                                0x0300u, 0x1000u };
    for (u32 i = 0; i < sizeof none / sizeof none[0]; i++) {
        memset(mem + CH_DEST, 0xEE, 0x200);
        CHECK_EQ_INT((int)config_key_name(none[i], 1u, CH_DEST), 0);
        CHECK_EQ_INT((int)DSB(CH_DEST), 0xEE);
    }
    memset(mem + CH_DEST, 0xEE, 0x200);
    CHECK_EQ_INT((int)config_key_name(0x0E80u, 1u, CH_DEST), 1);
    CHECK(strcmp((const char *)(mem + CH_DEST),
                 (const char *)game_string_get(0x22Au)) == 0, "0x80 uses its scan");
    /* Scan code 2 with no ascii is the ascii arm too (0x315A7 `cmp eax,0x47`
     * sees EAX = 2): "%c" of 0 is an empty string. */
    memset(mem + CH_DEST, 0xEE, 0x200);
    CHECK_EQ_INT((int)config_key_name(0x0200u, 1u, CH_DEST), 1);
    CHECK_EQ_INT((int)DSB(CH_DEST), 0);
    CHECK_EQ_INT((int)DSB(CH_DEST + 1u), 0);
}

static void ch_check_key_flags(void)
{
    u32 saved_kb = DSD(DS_00101514);
    u32 saved_tick = DSD(CH_TICK), saved_time = DSD(CH_KEY_TIME);
    u32 saved_latch = DSD(CH_KEY_LATCH);
    u8 saved_idle = DSB(0x00107414u);
    u8 saved_rec[0x300];
    memcpy(saved_rec, mem + CH_KB, sizeof saved_rec);

    DSD(DS_00101514) = CH_KB;
    memset(mem + CH_KB, 0, 0x300);
    for (u32 i = 0; i < 4; i++) {            /* player 1: 0x48 0x50 0x4B 0x4D */
        static const u8 p1[4] = { 0x48, 0x50, 0x4B, 0x4D };
        static const u8 p2[4] = { 0x11, 0x1F, 0x1E, 0x20 };
        DSB(CH_KB + 0x2DEu + i) = p1[i];
        DSB(CH_KB + 0x2E6u + i) = p2[i];
    }
    DSW(CH_KB + 0x2D4u) = 1u;
    DSW(CH_KB + 0x2D6u) = 1u;

    /* 0x2EB80: a latched key is returned as is; without one the idle timeout
     * (unsigned > 0x4B0) stores the flag byte. */
    DSD(CH_KEY_LATCH) = 0x48u;
    DSD(CH_TICK) = 5000u;
    DSD(CH_KEY_TIME) = 1u;
    DSB(0x00107414u) = 0x55u;
    CHECK_EQ_INT((int)config_key_latched(), 0x48);
    CHECK_EQ_INT((int)DSB(0x00107414u), 0x55);       /* untouched: a key is latched */
    DSD(CH_KEY_LATCH) = 0u;
    DSD(CH_KEY_TIME) = 5000u - 0x4B0u;               /* exactly 0x4B0: not over */
    CHECK_EQ_INT((int)config_key_latched(), 0);
    CHECK_EQ_INT((int)DSB(0x00107414u), 0x55);
    DSD(CH_KEY_TIME) = 5000u - 0x4B1u;               /* one over: the timeout */
    CHECK_EQ_INT((int)config_key_latched(), 0);
    CHECK_EQ_INT((int)DSB(0x00107414u), 0);
    DSB(0x00107414u) = 0x55u;
    DSD(CH_KEY_TIME) = 5001u;                        /* the clock behind: wraps huge */
    CHECK_EQ_INT((int)config_key_latched(), 0);
    CHECK_EQ_INT((int)DSB(0x00107414u), 0);

    /* Nothing latched, the timeout not due: 0, and the record pointer is kept. */
    DSD(CH_KEY_TIME) = 5000u;
    CHECK_EQ_INT((int)config_key_flags(0u), 0);
    CHECK_EQ_INT((int)DSD(DS_00101514), (int)CH_KB);

    /* Arrow scan codes with mask 0. Player 1's key matched with its word
     * non-zero, and player 2's with its own word. */
    static const struct { u8 scan; u32 bits; u32 p1; u32 p2; } arrows[] = {
        { 0x48, 0x80008000u, 0x2DE, 0x2E6 }, { 0x50, 0x40004000u, 0x2DF, 0x2E7 },
        { 0x4B, 0x20002000u, 0x2E0, 0x2E8 }, { 0x4D, 0x10001000u, 0x2E1, 0x2E9 },
    };
    for (u32 i = 0; i < 4; i++) {
        DSD(CH_KEY_LATCH) = arrows[i].scan;
        CHECK_EQ_INT((int)config_key_flags(0u), (int)arrows[i].bits);
        /* Player 1's word zero blocks the P1 match. */
        DSW(CH_KB + 0x2D4u) = 0u;
        CHECK_EQ_INT((int)config_key_flags(0u), 0);
        /* The scan code equals both players' keys: P1's word zero returns
         * before P2 is looked at. */
        u8 p2 = DSB(CH_KB + arrows[i].p2);
        DSB(CH_KB + arrows[i].p2) = arrows[i].scan;
        CHECK_EQ_INT((int)config_key_flags(0u), 0);
        DSW(CH_KB + 0x2D4u) = 1u;
        DSW(CH_KB + 0x2D6u) = 0u;
        CHECK_EQ_INT((int)config_key_flags(0u), 0);  /* P2's word zero blocks P2 */
        DSW(CH_KB + 0x2D6u) = 1u;
        CHECK_EQ_INT((int)config_key_flags(0u), (int)arrows[i].bits);
        DSB(CH_KB + arrows[i].p2) = p2;
        /* Only P2's key matches: P2's word decides. */
        u8 p1 = DSB(CH_KB + arrows[i].p1);
        DSB(CH_KB + arrows[i].p1) = 0x99u;
        DSB(CH_KB + arrows[i].p2) = arrows[i].scan;
        CHECK_EQ_INT((int)config_key_flags(0u), (int)arrows[i].bits);
        DSW(CH_KB + 0x2D6u) = 0u;
        CHECK_EQ_INT((int)config_key_flags(0u), 0);
        DSW(CH_KB + 0x2D6u) = 1u;
        /* Neither key matches: the bits are set regardless of the words. */
        DSB(CH_KB + arrows[i].p2) = 0x98u;
        DSW(CH_KB + 0x2D4u) = 0u;
        DSW(CH_KB + 0x2D6u) = 0u;
        CHECK_EQ_INT((int)config_key_flags(0u), (int)arrows[i].bits);
        DSW(CH_KB + 0x2D4u) = 1u;
        DSW(CH_KB + 0x2D6u) = 1u;
        DSB(CH_KB + arrows[i].p1) = p1;
        DSB(CH_KB + arrows[i].p2) = p2;
    }

    /* The mask gate: 0xF300F000 bits admit the arrows, others do not. */
    DSD(CH_KEY_LATCH) = 0x48u;
    CHECK_EQ_INT((int)config_key_flags(0x80000000u), (int)0x80008000u);
    CHECK_EQ_INT((int)config_key_flags(0x00000100u), 0);
    CHECK_EQ_INT((int)config_key_flags(0x0C000000u), 0);   /* outside 0xF300F000 */
    CHECK_EQ_INT((int)config_key_flags(0x01000000u), (int)0x80008000u);  /* bit 24 is in it */
    CHECK_EQ_INT((int)config_key_flags(0x02000000u), (int)0x80008000u);  /* and bit 25 */
    /* The menu drivers' own mask (0x2FA40/0x2FFC4 poll 0xC300C000). */
    DSD(CH_KEY_LATCH) = 0x50u;
    CHECK_EQ_INT((int)config_key_flags(0xC300C000u), 0x40004000);
    /* A code outside the arrows sets nothing. */
    DSD(CH_KEY_LATCH) = 0x49u;
    CHECK_EQ_INT((int)config_key_flags(0u), 0);
    /* Not even when a player's binding is 0x49 (inside the 0x48..0x50 table
     * span but on a default arm, 0x2ED9F). */
    {
        u8 p2up = DSB(CH_KB + 0x2E6u);
        DSB(CH_KB + 0x2E6u) = 0x49u;
        CHECK_EQ_INT((int)config_key_flags(0u), 0);
        DSB(CH_KB + 0x2E6u) = p2up;
    }
    DSD(CH_KEY_LATCH) = 0x4Au;
    CHECK_EQ_INT((int)config_key_flags(0u), 0);

    /* Enter and Esc. */
    DSD(CH_KEY_LATCH) = 0x0Du;
    CHECK_EQ_INT((int)config_key_flags(0u), 0x1000000);
    CHECK_EQ_INT((int)config_key_flags(0x01000000u), 0x1000000);
    CHECK_EQ_INT((int)config_key_flags(0x02000000u), 0);
    CHECK_EQ_INT((int)config_key_flags(0x80000000u), 0);
    DSD(CH_KEY_LATCH) = 0x1Bu;
    CHECK_EQ_INT((int)config_key_flags(0u), 0x2000000);
    CHECK_EQ_INT((int)config_key_flags(0x02000000u), 0x2000000);
    CHECK_EQ_INT((int)config_key_flags(0x01000000u), 0);
    CHECK_EQ_INT((int)config_key_flags(0x80000000u), 0);
    CHECK_EQ_INT((int)DSD(DS_00101514), (int)CH_KB);   /* stored back unchanged (0x2EDD1) */

    /* 0x2EDE0 / 0x2EEC8: the level word, the key flags on `flag`, the stamp
     * on a non-zero result, and the latch cleared by 0x2EEC8 alone. */
    u32 lvl = DSD(DS_000E1C34);
    DSD(DS_000E1C34) = 0x00000100u;
    DSD(CH_TICK) = 7777u;
    DSD(CH_KEY_LATCH) = 0x0Du;
    DSD(CH_KEY_TIME) = 1u;
    CHECK_EQ_INT((int)config_input_poll(0u, 0u), 0x100);       /* flag clear: level only */
    CHECK_EQ_INT((int)DSD(CH_KEY_TIME), 7777);
    DSD(CH_KEY_TIME) = 1u;
    CHECK_EQ_INT((int)config_input_poll(0u, 1u), 0x1000100);   /* level | Enter */
    CHECK_EQ_INT((int)DSD(CH_KEY_LATCH), 0x0D);
    DSD(DS_000E1C34) = 0u;
    DSD(CH_KEY_TIME) = 1u;
    CHECK_EQ_INT((int)config_input_poll(0u, 0u), 0);            /* zero: no stamp */
    CHECK_EQ_INT((int)DSD(CH_KEY_TIME), 1);
    CHECK_EQ_INT((int)config_input_poll_clear(0u, 1u), 0x1000000);
    CHECK_EQ_INT((int)DSD(CH_KEY_LATCH), 0);
    CHECK_EQ_INT((int)DSD(CH_KEY_TIME), 7777);
    DSD(CH_KEY_LATCH) = 0x33u;
    DSD(CH_KEY_TIME) = 1u;
    CHECK_EQ_INT((int)config_input_poll_clear(0u, 0u), 0);
    CHECK_EQ_INT((int)DSD(CH_KEY_LATCH), 0);                    /* cleared even on 0 */
    CHECK_EQ_INT((int)DSD(CH_KEY_TIME), 1);

    /* The menu drivers' mask 0xC300C000 with a pad level (0x50161's level word
     * and one-shot latch): the keyboard is read only on a non-zero flag, the
     * poll leaves the key latched, and the stamp follows a non-zero result. */
    u32 plat = DSD(DS_000E1C38);
    DSD(CH_TICK) = 700u;
    DSD(CH_KEY_TIME) = 5u;
    DSD(CH_KEY_LATCH) = 0x50u;
    DSD(DS_000E1C34) = 0u; DSD(DS_000E1C38) = 0u;
    CHECK_EQ_INT((int)config_input_poll(0xC300C000u, 0u), 0);   /* keyboard not read */
    CHECK_EQ_INT((int)DSD(CH_KEY_TIME), 5);
    CHECK_EQ_INT((int)config_input_poll(0xC300C000u, 1u), 0x40004000);
    CHECK_EQ_INT((int)DSD(CH_KEY_TIME), 700);
    CHECK_EQ_INT((int)DSD(CH_KEY_LATCH), 0x50);                 /* 0x2EDE0 leaves the key */
    DSD(CH_KEY_LATCH) = 0u;
    DSD(DS_000E1C34) = 0x80008000u; DSD(DS_000E1C38) = 0u;
    CHECK_EQ_INT((int)config_input_poll(0xC300C000u, 0u), (int)0x80008000u);
    CHECK_EQ_INT((int)config_input_poll(0xC300C000u, 0u), 0);   /* latched: one shot */
    DSD(CH_TICK) = 900u;
    DSD(CH_KEY_LATCH) = 0x50u;
    DSD(DS_000E1C34) = 0u; DSD(DS_000E1C38) = 0u;
    DSD(CH_KEY_TIME) = 5u;
    CHECK_EQ_INT((int)config_input_poll_clear(0xC300C000u, 1u), 0x40004000);
    CHECK_EQ_INT((int)DSD(CH_KEY_LATCH), 0);
    CHECK_EQ_INT((int)DSD(CH_KEY_TIME), 900);
    DSD(CH_KEY_TIME) = 5u;
    CHECK_EQ_INT((int)config_input_poll_clear(0xC300C000u, 1u), 0);
    CHECK_EQ_INT((int)DSD(CH_KEY_TIME), 5);
    DSD(DS_000E1C38) = plat;
    DSD(DS_000E1C34) = lvl;

    DSD(DS_00101514) = saved_kb;
    DSD(CH_TICK) = saved_tick;
    DSD(CH_KEY_TIME) = saved_time;
    DSD(CH_KEY_LATCH) = saved_latch;
    DSB(0x00107414u) = saved_idle;
    memcpy(mem + CH_KB, saved_rec, sizeof saved_rec);
}

static void ch_check_bar(void)
{
    ch_text_setup();
    /* value 200, no label: the mode steps at the raw's thresholds. Cell k is
     * i = 8k: 0x1000 through 128 (0x81 not passed), 0x2000 for 136..184, 0x3000
     * for 192 and 200 (> 0xBF, <= value), 0xF000 beyond the value. */
    config_bar_draw(200, 3, -1);
    for (s32 k = 0; k < 32; k++) {
        u32 i = (u32)k * 8u;
        u32 mode = i > 200u ? 0xF000u : i > 0xBFu ? 0x3000u
                 : i > 0x81u ? 0x2000u : 0x1000u;
        for (s32 r = 3; r < 6; r++)
            ch_expect(r, 5 + k, 0x13u, mode, "bar cell");
    }
    CHECK_EQ_INT((int)ch_cell(3, 4), 0);
    CHECK_EQ_INT((int)ch_cell(3, 37), 0);
    CHECK_EQ_INT((int)ch_cell(6, 5), 0);
    CHECK_EQ_INT((int)ch_cell(2, 5), 0);
    CHECK_EQ_INT((int)ch_cell(3, 0x19), ch_cell(3, 0x19));    /* bar cell, not a label */
    /* No label: nothing outside the bar rows. */
    CHECK_EQ_INT((int)DSW(DS_00105F34), 0);

    /* Clamping: 999 acts as 0xFF (no cell past the value), -5 as 0 (cell 0 keeps
     * 0x1000 because 0 > 0 is false). */
    actors_reset();
    config_bar_draw(999, 3, -1);
    ch_expect(3, 5 + 31, 0x13u, 0x3000u, "clamped high: last cell within the value");
    actors_reset();
    config_bar_draw(-5, 3, -1);
    ch_expect(3, 5, 0x13u, 0x1000u, "clamped low: cell 0 at 0x1000");
    ch_expect(3, 6, 0x13u, 0xF000u, "clamped low: cell 1 past the value");

    /* A label row >= 0 draws the value as a three-wide number at column 0x19:
     * the same cells as text_number_set with pad 2 and mode 0xC002. */
    actors_reset();
    config_bar_draw(100, 3, 8);
    u32 got[8][2];
    for (s32 c = 0; c < 8; c++) {
        got[c][0] = ch_sprite(8, 0x19 + c);
        got[c][1] = ch_pal(8, 0x19 + c);
    }
    actors_reset();
    text_number_set(0x19, 8, 100, 3, 2u, 0xC002u);
    for (s32 c = 0; c < 8; c++) {
        CHECK_EQ_INT((int)got[c][0], (int)ch_sprite(8, 0x19 + c));
        CHECK_EQ_INT((int)got[c][1], (int)ch_pal(8, 0x19 + c));
    }
    CHECK(got[0][0] != 0u && got[2][0] != 0u, "the label number was drawn");
    /* The label shows the clamped value: 999 prints as 255 (a 256 would differ
     * in the second digit), -5 prints as 0 (not "-5"). The numbers use the
     * class font, whose glyphs advance one or two columns, so eight columns
     * are compared. */
    static const struct { s32 in; s32 shown; } clamp[] = { { 999, 255 }, { -5, 0 } };
    for (u32 k = 0; k < 2; k++) {
        actors_reset();
        config_bar_draw(clamp[k].in, 3, 8);
        u32 gs[8];
        for (s32 c = 0; c < 8; c++) gs[c] = ch_sprite(8, 0x19 + c);
        actors_reset();
        text_number_set(0x19, 8, clamp[k].shown, 3, 2u, 0xC002u);
        u32 drawn = 0;
        for (s32 c = 0; c < 8; c++) {
            CHECK_EQ_INT((int)gs[c], (int)ch_sprite(8, 0x19 + c));
            if (gs[c] != 0u) drawn++;
        }
        CHECK(drawn >= 1u, "the clamped number was drawn");
    }
    /* A negative label row draws no number. */
    actors_reset();
    config_bar_draw(100, 3, -1);
    CHECK_EQ_INT((int)ch_cell(8, 0x19), 0);
    /* Label row 0 is a row, not "none". */
    actors_reset();
    config_bar_draw(100, 3, 0);
    CHECK(ch_cell(0, 0x19) != 0u, "label row 0 draws");
}

static void ch_check_option_row(void)
{
    ch_text_setup();
    memset(mem + CH_REC, 0, 0x40);
    DSW(CH_REC) = 2u;               /* which == 0 reads +0 */
    DSW(CH_REC + 0x12u) = 6u;       /* which != 0 reads +0x12 */

    /* which == 0, flag 0: heading 0x17 at (2,4) mode 0x2000; value 2 -> string
     * 0x22F at column 2 (base 0xA - 8), row 6, mode 0x4000; arrows at 1 and 19. */
    config_option_row(0u, CH_REC, 0u);
    const u8 *h = game_string_get(0x17u);
    ch_expect(4, 2, h[0], 0x2000u, "heading 0x17 first glyph");
    char s22f[0x100];
    strcpy(s22f, (const char *)game_string_get(0x22Fu));
    u32 len = (u32)strlen(s22f);
    ch_expect(6, 2, (u32)(u8)s22f[0], 0x4000u, "value 2 -> 0x22F");
    CHECK(ch_cell(6, (s32)(2u + len)) == 0u || (2u + len) == 19u,
          "value string ends at its length");
    ch_expect(6, 1, 0x3Cu, 0x4000u, "left arrow at base - 9");
    ch_expect(6, 19, 0x3Eu, 0x4000u, "right arrow at base + 9");
    CHECK_EQ_INT((int)DSW(DS_00105F34 + 2), (int)(2u + len));   /* the last 0x2F198 */

    /* flag != 0 moves the value string to mode 0x3000 (arrows stay 0x4000). */
    actors_reset();
    config_option_row(0u, CH_REC, 1u);
    ch_expect(6, 2, (u32)(u8)s22f[0], 0x3000u, "flag set: arrow mode 0x3000");
    ch_expect(6, 1, 0x3Cu, 0x4000u, "arrows stay 0x4000");

    /* which != 0: heading 0x16 at column 0x16, base 0x1E; value 6 -> 0x22E at
     * column 0x16, arrows at 0x15 and 0x27. */
    actors_reset();
    config_option_row(1u, CH_REC, 0u);
    h = game_string_get(0x16u);
    ch_expect(4, 0x16, h[0], 0x2000u, "heading 0x16");
    ch_expect(6, 0x16, (u32)(u8)game_string_get(0x22Eu)[0], 0x4000u, "value 6 -> 0x22E");
    ch_expect(6, 0x15, 0x3Cu, 0x4000u, "left arrow at 0x1E - 9");
    ch_expect(6, 0x27, 0x3Eu, 0x4000u, "right arrow at 0x1E + 9");

    /* The four values and their strings; odd values and 8 draw no value text. */
    static const struct { u16 v; u32 id; } vals[] = {
        { 0, 0x22D }, { 2, 0x22F }, { 4, 0x230 }, { 6, 0x22E },
    };
    for (u32 i = 0; i < 4; i++) {
        actors_reset();
        DSW(CH_REC) = vals[i].v;
        config_option_row(0u, CH_REC, 0u);
        char want[0x100];
        strcpy(want, (const char *)game_string_get(vals[i].id));
        u32 wl = (u32)strlen(want);
        for (u32 c = 0; c < wl && 2u + c < 19u; c++)
            if (want[c] != ' ')
                ch_expect(6, (s32)(2u + c), (u32)(u8)want[c], 0x4000u, "value text glyph");
        CHECK(ch_cell(6, 2 + (s32)wl) == 0u || 2u + wl >= 19u, "value text width");
    }
    static const u16 skip[] = { 1, 3, 5, 7, 8, 0x102 };
    for (u32 i = 0; i < sizeof skip / sizeof skip[0]; i++) {
        actors_reset();
        DSW(CH_REC) = skip[i];
        config_option_row(0u, CH_REC, 0u);
        CHECK_EQ_INT((int)ch_cell(6, 2), 0);
        ch_expect(6, 1, 0x3Cu, 0x4000u, "arrows drawn for any value");
    }
}

static void ch_check_code_row(void)
{
    ch_text_setup();
    u8 saved_rec[0x20];
    memcpy(saved_rec, mem + CH_CODE, sizeof saved_rec);
    u8 saved_tab[8];
    memcpy(saved_tab, mem + 0x00105EC8u, sizeof saved_tab);
    u8 saved_flags = DSB(DS_00105DD8);

    /* Flags byte zero: eight columns, the first text (+4) below the count
     * (+2), the second (+0xD) from the count on. */
    memset(mem + CH_CODE, 0, 0x20);
    DSB(CH_CODE + 2u) = 3u;
    memcpy(mem + CH_CODE + 4u, "ABCDEFGH", 8);
    memcpy(mem + CH_CODE + 0xDu, "12345678", 9);
    config_code_row(3, 5);
    for (s32 i = 0; i < 8; i++) {
        u32 ch = i < 3 ? (u32)"ABCDEFGH"[i] : (u32)"12345678"[i];
        ch_expect(5, 3 + i, ch, i < 3 ? 0x4000u : 0x2000u, "code column");
    }
    CHECK_EQ_INT((int)ch_cell(5, 2), 0);
    CHECK_EQ_INT((int)ch_cell(5, 11), 0);
    /* Count 8: every column from the first text; count 0: all from the second. */
    actors_reset();
    DSB(CH_CODE + 2u) = 8u;
    config_code_row(0, 7);
    ch_expect(7, 7, 'H', 0x4000u, "count 8: last column first text");
    actors_reset();
    DSB(CH_CODE + 2u) = 0u;
    config_code_row(0, 7);
    ch_expect(7, 0, '1', 0x2000u, "count 0: first column second text");
    /* The cursor is the last cell's: row 7, column 0 + 7 + 1. */
    CHECK_EQ_INT((int)DSW(DS_00105F34), 7);
    CHECK_EQ_INT((int)DSW(DS_00105F34 + 2), 8);

    /* Flags byte non-zero: the second text drawn as one string at mode 0x1000
     * and, with flag bit 1 clear, the entry filed in table 2 record 0. */
    actors_reset();
    memset(mem + 0x00105EC8u, 0xA5, 8);
    memset(mem + CH_CODE, 0, 0x20);
    DSB(CH_CODE + 3u) = 1u;
    memcpy(mem + CH_CODE + 0xDu, "XYZ12345", 9);
    DSD(DS_00105EFC) = 0xDEADBEEFu;                         /* sentinel: the filing must overwrite it */
    config_code_row(2, 4);
    ch_expect(4, 2, 'X', 0x1000u, "the string is drawn at mode 0x1000");
    ch_expect(4, 9, '5', 0x1000u, "eight columns");
    CHECK_EQ_INT((int)DSB(CH_CODE + 3u), 3);                /* bit 1 set */
    u32 rec = hiscore_read(0u, 2u);
    CHECK_EQ_INT((int)rec, 0x105EFC);
    CHECK_EQ_INT((int)DSD(DS_00105EFC), 12345);
    CHECK(strcmp((const char *)(mem + DS_00105F00), "XYZ") == 0,
          "the 3-character name is filed");
    /* Bit 1 set: draws, files nothing. */
    memcpy(mem + CH_CODE + 0xDu, "QRS99999", 9);
    config_code_row(2, 4);
    CHECK_EQ_INT((int)DSB(CH_CODE + 3u), 3);
    (void)hiscore_read(0u, 2u);
    CHECK_EQ_INT((int)DSD(DS_00105EFC), 12345);
    /* The digit walk stops at the first character outside '0'..'9' and takes at
     * most five (columns 3..7). */
    static const struct { const char *t; int n; } digits[] = {
        { "ABC12x45", 12 }, { "ABC1:345", 1 }, { "ABC1/345", 1 },
        { "ABCx1234", 0 }, { "ABC00007", 7 }, { "ABC99999", 99999 },
        { "ABC 1234", 0 },
    };
    for (u32 i = 0; i < sizeof digits / sizeof digits[0]; i++) {
        actors_reset();
        DSB(CH_CODE + 3u) = 1u;
        memcpy(mem + CH_CODE + 0xDu, digits[i].t, 9);
        config_code_row(0, 2);
        (void)hiscore_read(0u, 2u);
        CHECK_EQ_INT((int)DSD(DS_00105EFC), digits[i].n);
        CHECK(strncmp((const char *)(mem + DS_00105F00), digits[i].t, 3) == 0,
              "name kept");
    }
    /* A high-bit character is negative as a signed byte and stops the walk. */
    actors_reset();
    DSB(CH_CODE + 3u) = 1u;
    memcpy(mem + CH_CODE + 0xDu, "ABC1\xB1" "234", 9);
    config_code_row(0, 2);
    (void)hiscore_read(0u, 2u);
    CHECK_EQ_INT((int)DSD(DS_00105EFC), 1);

    memcpy(mem + CH_CODE, saved_rec, sizeof saved_rec);
    memcpy(mem + 0x00105EC8u, saved_tab, sizeof saved_tab);
    DSB(DS_00105DD8) = saved_flags;
}

static void ch_check_screen_wait(void)
{
    ch_text_setup();
    u32 s_a0 = DSD(DS_000E87A0), s_a4 = DSD(DS_000E87A4);
    u32 s_tick = DSD(CH_TICK), s_isr = DSD(DS_00101508);
    u16 s_word = DSW(0x000EF6DEu);
    u8 s_gate = DSB(0x00104B22u), s_full = DSB(DS_001014FC);
    u32 s_lat = DSD(CH_KEY_LATCH), s_time = DSD(CH_KEY_TIME), s_kw = DSD(CH_KEY_WORD);

    DSD(DS_000E87A0) = CH_BUF_A;
    DSD(DS_000E87A4) = CH_BUF_B;
    memset(mem + CH_BUF_A, 0x11, 64000);
    memset(mem + CH_BUF_B, 0x22, 64000);
    DSB(0x00104B22u) = 0u;
    DSB(DS_001014FC) = 0x01u;
    DSD(CH_KEY_LATCH) = 0x77u;
    input_clear();

    /* n == -1: one frame, no waiting. The latch is cleared, the back buffer is
     * presented, the buffers swap and the full-copy flag byte is cleared. */
    u32 tick0 = DSD(CH_TICK);
    config_screen_wait(-1);
    CHECK_EQ_INT((int)DSD(CH_KEY_LATCH), 0);
    CHECK_EQ_INT((int)DSD(DS_000E87A0), (int)CH_BUF_B);
    CHECK_EQ_INT((int)DSD(DS_000E87A4), (int)CH_BUF_A);
    CHECK_EQ_INT((int)DSB(DS_001014FC), 0);
    CHECK_EQ_INT((int)DSD(CH_TICK), (int)tick0);             /* no wait, no tick */
    const u8 *shown = gfx_display();
    CHECK(shown != NULL, "a frame was presented");
    if (shown != NULL) {
        CHECK_EQ_INT((int)shown[0], 0x22);
        CHECK_EQ_INT((int)shown[63999], 0x22);
    }

    /* n == -2: one pass. One tick elapses (ISR model), and the queue's last key
     * wins: 'a' then an extended key (ascii 0) latches the scan code. */
    input_push(0x1E, 'a');
    input_push(0x48, 0x00);
    DSD(CH_TICK) = 1000u;
    DSD(CH_KEY_TIME) = 0u;
    DSW(0x000EF6DEu) = 0xFFFFu;
    u32 isr0 = DSD(DS_00101508);
    config_screen_wait(-2);
    CHECK_EQ_INT((int)DSD(CH_KEY_LATCH), 0x48);
    CHECK_EQ_INT((int)DSD(CH_KEY_WORD), 0x4800);
    CHECK_EQ_INT((int)DSD(CH_TICK), 1001);
    CHECK_EQ_INT((int)DSD(CH_KEY_TIME), 1001);
    CHECK_EQ_INT((int)DSW(0x000EF6DEu), 0);                  /* wrapped 0xFFFF + 1 */
    CHECK_EQ_INT((int)DSD(DS_00101508), (int)(isr0 + 1u));
    CHECK(!input_has_key(), "the queue was drained");

    /* n == 0: two passes (0x2EB6E..0x2EB74 loops once more for ecx = -1), so
     * two ticks; a key in the queue is stamped at the first. */
    input_push(0x1E, 'a');
    DSD(CH_TICK) = 2000u;
    config_screen_wait(0);
    CHECK_EQ_INT((int)DSD(CH_TICK), 2002);
    CHECK_EQ_INT((int)DSD(CH_KEY_LATCH), 'a');
    CHECK_EQ_INT((int)DSD(CH_KEY_WORD), 0x1E61);
    CHECK_EQ_INT((int)DSD(CH_KEY_TIME), 2001);
    /* n == 1 is three passes. */
    DSD(CH_TICK) = 3000u;
    config_screen_wait(1);
    CHECK_EQ_INT((int)DSD(CH_TICK), 3003);
    CHECK_EQ_INT((int)DSD(CH_KEY_LATCH), 0);                 /* cleared at entry */

    DSD(DS_000E87A0) = s_a0;
    DSD(DS_000E87A4) = s_a4;
    DSD(CH_TICK) = s_tick;
    DSD(DS_00101508) = s_isr;
    DSW(0x000EF6DEu) = s_word;
    DSB(0x00104B22u) = s_gate;
    DSB(DS_001014FC) = s_full;
    DSD(CH_KEY_LATCH) = s_lat;
    DSD(CH_KEY_TIME) = s_time;
    DSD(CH_KEY_WORD) = s_kw;
}

/* Key-config record (0x1AE28/0x1AE20/0x1AEE0/0x1AF64; record §50-C). Every
 * destination is seeded with a sentinel that differs from the post-condition. */
static void ch_check_keycfg(void)
{
    enum { DSM = 0x001014ACu, RECN = 0x28 };
    u32 saved_kb = DSD(DS_00101514);
    u8 saved_rec[0x300], saved_mirror[0x28], saved_def[RECN];
    memcpy(saved_rec, mem + CH_KB, sizeof saved_rec);
    memcpy(saved_mirror, mem + DSM, sizeof saved_mirror);
    memcpy(saved_def, mem + 0x000A2C62u, RECN);
    DSD(DS_00101514) = CH_KB;

    u8 rec[RECN];
    memset(rec, 0, sizeof rec);
    rec[0] = 0x03u; rec[1] = 0xABu;          /* device 3, garbage high byte */
    rec[0x12] = 0x05u; rec[0x13] = 0xCDu;
    for (u32 i = 0; i < 8; i++) {
        rec[2 + 2 * i] = (u8)(0x10u + i);     /* ASCII (low) */
        rec[3 + 2 * i] = (u8)(0x80u + i);     /* scan (high) */
        rec[0x14 + 2 * i] = (u8)(0x20u + i);
        rec[0x15 + 2 * i] = (u8)(0x90u + i);
    }
    rec[0x24] = 0x64u; rec[0x25] = 0x77u; rec[0x26] = 0x65u; rec[0x27] = 0x78u;
    memcpy(mem + 0x2F00000u, rec, RECN);

    /* 0x1AE28 */
    memset(mem + CH_KB, 0xEE, 0x300);
    memset(mem + DSM, 0xEE, 0x28);
    config_keys_apply(0x2F00000u);
    CHECK_EQ_INT(DSW(CH_KB + 0x2D4u), 3);            /* high byte dropped: 0x1AE3B */
    CHECK_EQ_INT(DSW(CH_KB + 0x2D6u), 5);
    CHECK_EQ_INT(DSB(DSM), 3);
    CHECK_EQ_INT(DSB(DSM + 0x12u), 5);
    for (u32 i = 0; i < 8; i++) {
        CHECK_EQ_INT(DSB(CH_KB + 0x2DEu + i), 0x80 + (int)i);
        CHECK_EQ_INT(DSB(CH_KB + 0x2E6u + i), 0x90 + (int)i);
        CHECK_EQ_INT(DSB(DSM + 2u + 2u * i), 0x80 + (int)i);   /* scan mirror */
        CHECK_EQ_INT(DSB(DSM + 3u + 2u * i), 0x10 + (int)i);   /* ASCII mirror */
        CHECK_EQ_INT(DSB(DSM + 0x14u + 2u * i), 0x90 + (int)i);
        CHECK_EQ_INT(DSB(DSM + 0x15u + 2u * i), 0x20 + (int)i);
    }
    CHECK_EQ_INT(DSB(DSM + 0x24u), 0x64);
    CHECK_EQ_INT(DSB(DSM + 0x26u), 0x65);
    CHECK_EQ_INT(DSB(CH_KB + 0x2DDu), 0xEE);         /* neighbours untouched */
    CHECK_EQ_INT(DSB(CH_KB + 0x2D8u), 0xEE);
    CHECK_EQ_INT(DSB(CH_KB + 0x2EEu), 0xEE);
    CHECK_EQ_INT(DSB(DSM + 0x25u), 0xEE);

    /* 0x1AEE0: the pack is the inverse; the device and +0x24/+0x26 words are
     * zero-extended bytes. */
    memset(mem + 0x2F00100u, 0xEE, RECN);
    config_keys_pack(0x2F00100u);
    u8 want[RECN];
    memcpy(want, rec, RECN);
    want[1] = 0; want[0x13] = 0; want[0x25] = 0; want[0x27] = 0;
    CHECK(memcmp(mem + 0x2F00100u, want, RECN) == 0, "0x1AEE0 packs the mirror");

    /* 0x1AF64: only the low device byte and the high key bytes reach BIOS. */
    memset(mem + CH_KB, 0xEE, 0x300);
    config_keys_load(0x2F00000u);
    CHECK_EQ_INT(DSW(CH_KB + 0x2D4u), 3);
    CHECK_EQ_INT(DSW(CH_KB + 0x2D6u), 5);
    for (u32 i = 0; i < 8; i++) {
        CHECK_EQ_INT(DSB(CH_KB + 0x2DEu + i), 0x80 + (int)i);
        CHECK_EQ_INT(DSB(CH_KB + 0x2E6u + i), 0x90 + (int)i);
    }
    CHECK_EQ_INT(DSB(CH_KB + 0x2DDu), 0xEE);
    CHECK_EQ_INT(DSB(CH_KB + 0x2D8u), 0xEE);
    CHECK_EQ_INT(DSB(CH_KB + 0x2EEu), 0xEE);

    /* 0x1AE20: the default record at DS 0x22C62, bytes read from the shipped
     * image (Ghidra 0xA2C62): S X Z C U I N M and the arrow block. */
    static const u8 def[RECN] = {
        0x00,0x00,0x73,0x1F,0x78,0x2D,0x7A,0x2C,0x63,0x2E,0x75,0x16,0x69,0x17,
        0x6E,0x31,0x6D,0x32,0x00,0x00,0x00,0x48,0x00,0x50,0x00,0x4B,0x00,0x4D,
        0x00,0x47,0x00,0x49,0x00,0x4F,0x00,0x51,0x64,0x00,0x64,0x00 };
    memcpy(mem + 0x000A2C62u, def, RECN);
    memset(mem + CH_KB, 0xEE, 0x300);
    config_keys_apply_defaults();
    static const u8 p1[8] = { 0x1F,0x2D,0x2C,0x2E,0x16,0x17,0x31,0x32 };
    static const u8 p2[8] = { 0x48,0x50,0x4B,0x4D,0x47,0x49,0x4F,0x51 };
    CHECK_EQ_INT(DSW(CH_KB + 0x2D4u), 0);
    CHECK_EQ_INT(DSW(CH_KB + 0x2D6u), 0);
    CHECK(memcmp(mem + CH_KB + 0x2DEu, p1, 8) == 0, "0x1AE20 player 1 scans");
    CHECK(memcmp(mem + CH_KB + 0x2E6u, p2, 8) == 0, "0x1AE20 player 2 scans");
    CHECK_EQ_INT(DSB(DSM + 0x24u), 100);

    memcpy(mem + 0x000A2C62u, saved_def, RECN);
    memcpy(mem + DSM, saved_mirror, sizeof saved_mirror);
    memcpy(mem + CH_KB, saved_rec, sizeof saved_rec);
    DSD(DS_00101514) = saved_kb;
}

/* Record §K5 (2026-09-29-k2-k5-derivations.md): 0x2EA74 is `xor eax,eax;
 * mov eax,eax` with no `ret`, so it runs 0x2EA78 with EAX = 0: the latch is
 * cleared, one frame is presented (the buffers swap), and two passes wait a
 * tick each and drain the key queue. Every seed differs from its post-value. */
static void ch_check_screen_wait_zero(void)
{
    ch_text_setup();
    u32 s_a0 = DSD(DS_000E87A0), s_a4 = DSD(DS_000E87A4);
    u32 s_tick = DSD(CH_TICK), s_isr = DSD(DS_00101508);
    u16 s_word = DSW(0x000EF6DEu);
    u8 s_gate = DSB(0x00104B22u), s_full = DSB(DS_001014FC);
    u32 s_lat = DSD(CH_KEY_LATCH), s_time = DSD(CH_KEY_TIME), s_kw = DSD(CH_KEY_WORD);

    DSD(DS_000E87A0) = CH_BUF_A;
    DSD(DS_000E87A4) = CH_BUF_B;
    memset(mem + CH_BUF_A, 0x11, 64000);
    memset(mem + CH_BUF_B, 0x33, 64000);
    DSB(0x00104B22u) = 0u;
    DSB(DS_001014FC) = 0u;
    input_clear();

    /* No key: the latch 0x77 is cleared at 0x2EA85 and stays 0. */
    DSD(CH_KEY_LATCH) = 0x77u;
    DSD(CH_TICK) = 5000u;
    DSD(CH_KEY_TIME) = 0x1234u;
    config_screen_wait_zero();
    CHECK_EQ_INT((int)DSD(CH_KEY_LATCH), 0);
    CHECK_EQ_INT((int)DSD(CH_TICK), 5002);                   /* two passes */
    CHECK_EQ_INT((int)DSD(CH_KEY_TIME), 0x1234);             /* nothing stamped */
    CHECK_EQ_INT((int)DSD(DS_000E87A0), (int)CH_BUF_B);      /* 0x50188 swap */
    CHECK_EQ_INT((int)DSD(DS_000E87A4), (int)CH_BUF_A);
    const u8 *shown = gfx_display();
    CHECK(shown != NULL, "0x2EA74 presents a frame");
    if (shown != NULL) {
        CHECK_EQ_INT((int)shown[0], 0x33);
        CHECK_EQ_INT((int)shown[63999], 0x33);
    }

    /* A queued key is drained and stamped at the first pass. */
    input_push(0x1E, 'a');
    DSD(CH_TICK) = 6000u;
    DSD(CH_KEY_WORD) = 0u;
    config_screen_wait_zero();
    CHECK_EQ_INT((int)DSD(CH_TICK), 6002);
    CHECK_EQ_INT((int)DSD(CH_KEY_LATCH), 'a');
    CHECK_EQ_INT((int)DSD(CH_KEY_WORD), 0x1E61);
    CHECK_EQ_INT((int)DSD(CH_KEY_TIME), 6001);
    CHECK(!input_has_key(), "0x2EA74 drained the queue");

    DSD(DS_000E87A0) = s_a0;
    DSD(DS_000E87A4) = s_a4;
    DSD(CH_TICK) = s_tick;
    DSD(DS_00101508) = s_isr;
    DSW(0x000EF6DEu) = s_word;
    DSB(0x00104B22u) = s_gate;
    DSB(DS_001014FC) = s_full;
    DSD(CH_KEY_LATCH) = s_lat;
    DSD(CH_KEY_TIME) = s_time;
    DSD(CH_KEY_WORD) = s_kw;
}

/* 0x249F0 (record §50-D): the quit prompt. Strings 0x1F0/0x1F1 are the
 * localised yes/no letters; the seeds differ from every asserted result. */
static void ch_check_quit_prompt(void)
{
    ch_text_setup();
    const u8 yes = game_string_get(0x1F0u)[0];
    const u8 no = game_string_get(0x1F1u)[0];
    CHECK(yes >= 'A' && yes <= 'Z' && no >= 'A' && no <= 'Z' && yes != no,
          "the yes/no letters are distinct capitals");
    const u8 lo_yes = (u8)(yes + 0x20), lo_no = (u8)(no + 0x20);
    const u32 s_a0 = DSD(DS_000E87A0), s_a4 = DSD(DS_000E87A4);
    const u8 s_gate = DSB(DS_00104B22), s_quit = DSB(DS_000A81A8);
    const u8 s_da = DSB(DS_001028DA), s_d8 = DSB(DS_001028D8);
    const u32 s_c0 = DSD(DS_001028C0), s_c8 = DSD(DS_001028C8);
    const u8 s_full = DSB(DS_001014FC);
    const u32 s_lat = DSD(CH_KEY_LATCH);

    DSD(DS_001028C0) = 0;
    DSD(DS_001028C8) = 0;
    for (u32 pass = 0; pass < 4u; pass++) {
        /* pass 0: AL = 0, "no" (lower case) after a stray key.
         * pass 1: AL = 0, "yes" upper case.  pass 2: AL = 1, "yes" lower.
         * pass 3: AL = 1, "no". */
        const u32 hard = (pass >= 2u) ? 1u : 0u;
        const int want_quit = (pass == 1u || pass == 2u);
        DSD(DS_000E87A0) = CH_BUF_A;
        DSD(DS_000E87A4) = CH_BUF_B;
        DSB(DS_00104B22) = 0x77u;
        DSB(DS_000A81A8) = 0x5Au;
        DSB(DS_001028DA) = 0;
        DSB(DS_001028D8) = 0x55u;
        DSD(DS_00105F34) = 0;
        input_clear();
        if (pass == 0u) input_push(0x2D, 'x');            /* not yes, not no */
        u8 k = (pass == 0u) ? lo_no : (pass == 1u) ? yes : (pass == 2u) ? lo_yes : no;
        input_push(0x1E, k);
        game_quit_prompt(hard);
        CHECK_EQ_INT((int)DSB(DS_00104B22), 0);           /* seed 0x77 */
        CHECK_EQ_INT((int)DSB(DS_000A81A8), want_quit ? 1 : 0x5A);
        CHECK(!input_has_key(), "the prompt consumed its keys");
        CHECK_EQ_INT((int)DSB(DS_001028DA), 0);           /* resumed */
        CHECK_EQ_INT((int)DSB(DS_001028D8), 0);           /* seed 0x55 */
        CHECK_EQ_INT((int)DSW(DS_00105F34), 0xA);         /* the question's row */
        {
            /* the question drawn is 0x1EE (AL = 0) or 0x1EF, centred: the
             * cursor's column word is col + width (mode 0x1000: strlen). */
            int len = (int)strlen((const char *)game_string_get(hard ? 0x1EFu : 0x1EEu));
            CHECK_EQ_INT((int)DSW(DS_00105F34 + 2), ((0x2B - len) >> 1) + len);
        }
        CHECK_EQ_INT((int)DSD(DS_000E87A0), (int)CH_BUF_B);   /* one frame presented */
    }

    DSD(DS_000E87A0) = s_a0;
    DSD(DS_000E87A4) = s_a4;
    DSB(DS_00104B22) = s_gate;
    DSB(DS_000A81A8) = s_quit;
    DSB(DS_001028DA) = s_da;
    DSB(DS_001028D8) = s_d8;
    DSD(DS_001028C0) = s_c0;
    DSD(DS_001028C8) = s_c8;
    DSB(DS_001014FC) = s_full;
    DSD(CH_KEY_LATCH) = s_lat;
}

/* ---- the options ("service") menu, record 2026-09-29-k11-service-menu-derivations.md ---- */
typedef struct { u16 key; u32 pad; } sm_step_t;   /* key = (scan << 8) | ascii, 0 = none */
static const sm_step_t *sm_script;
static u32 sm_len, sm_frame, sm_last_a0, sm_extra;
static u32 sm_s_a0, sm_s_a4, sm_s_kb;
static u8 sm_s_gate;
/* Pad bits the script holds from an earlier frame: tf_menu_press clears the
 * latch DS_000E1C38 (every pressed bit is a new edge), and these go back into
 * it, as 0x500C4's `latch &= level` keeps a held bit latched. 0 by default. */
static u32 sm_held;
/* Called by sm_hook with the frame index at each new frame, before that
 * frame's step is applied: it sees the screen and state the previous pass
 * left. NULL by default; sm_end clears it. */
static void (*sm_probe)(u32 frame);

/* One scripted step per presented frame: config_screen_wait swaps DS_000E87A0
 * before its tick passes, so the first hook call after a swap is a new frame. */
static void sm_hook(void *ctx)
{
    (void)ctx;
    if (DSD(DS_000E87A0) == sm_last_a0) return;
    sm_last_a0 = DSD(DS_000E87A0);
    if (sm_probe != NULL) sm_probe(sm_frame);
    if (sm_frame < sm_len) {
        const sm_step_t *s = &sm_script[sm_frame];
        if (s->key != 0u) input_push((u8)(s->key >> 8), (u8)s->key);
        tf_menu_press(s->pad);
        DSD(DS_000E1C38) |= s->pad & sm_held;
    } else {
        /* Out of script: Esc, then alternately Enter, so any loop leaves. */
        sm_extra++;
        input_push(0x01, 0x1B);
        tf_menu_press((sm_extra & 1u) ? 0x2000000u : 0x1000000u);
        if (sm_extra > 600u) {
            printf("FAIL %s:%d: scripted menu loop never left\n", __FILE__, __LINE__);
            exit(1);
        }
    }
    sm_frame++;
}

/* The environment every options-menu test needs, with or without the hook. */
static void sm_env_begin(void)
{
    sm_s_a0 = DSD(DS_000E87A0); sm_s_a4 = DSD(DS_000E87A4);
    sm_s_kb = DSD(DS_00101514); sm_s_gate = DSB(DS_00104B22);
    DSD(DS_000E87A0) = CH_BUF_A;
    DSD(DS_000E87A4) = CH_BUF_B;
    DSB(DS_00104B22) = 0u;              /* not 1: the ISR tick model runs (0x1BDF8) */
    mem_fill(MT_LAYOUT, 0, 0x300u);
    DSD(DS_00101514) = MT_LAYOUT;
    input_clear();
    tf_menu_press(0u);
    DSD(CH_KEY_LATCH) = 0u;
    /* 0x2EB80's idle timeout (tick - DS_00105F2C > 0x4B0, `jbe` at 0x2EB9F)
     * clears DS_00107414 on a no-key poll; the suite's tick has run on, so
     * stamp the key time now. */
    DSD(CH_KEY_TIME) = DSD(CH_TICK);
}

static void sm_env_end(void)
{
    input_clear();
    tf_menu_press(0u);
    DSD(DS_000E87A0) = sm_s_a0; DSD(DS_000E87A4) = sm_s_a4;
    DSD(DS_00101514) = sm_s_kb; DSB(DS_00104B22) = sm_s_gate;
}

static void sm_begin_raw(const sm_step_t *s, u32 n)
{
    sm_env_begin();
    sm_script = s; sm_len = n; sm_frame = 0u; sm_extra = 0u;
    sm_last_a0 = DSD(DS_000E87A0);
    host_set_pump_hook(sm_hook, NULL);
}

/* Set by sm_check_seam once the hook has scripted its two frames. Without a
 * live hook a blocking menu never sees its exit key and the suite would hang,
 * so every scripted run stops the process instead. */
static int sm_seam_ok;

static void sm_begin(const sm_step_t *s, u32 n)
{
    if (!sm_seam_ok) {
        printf("FAIL %s:%d: the host pump hook is not live; a scripted menu would hang\n",
               __FILE__, __LINE__);
        exit(1);
    }
    sm_begin_raw(s, n);
}

static void sm_end(u32 frames, const char *what)
{
    host_set_pump_hook(NULL, NULL);
    sm_held = 0u;
    sm_probe = NULL;
    CHECK(sm_extra == 0u, what);
    CHECK_EQ_INT((int)sm_frame, (int)frames);
    sm_env_end();
}

/* The harness seam (record §K11.1): one scripted key per presented frame. */
static void sm_check_seam(void)
{
    static const sm_step_t keys[] = { { 0x011Bu, 0u }, { 0x1C0Du, 0u } };
    sm_begin_raw(keys, 2u);
    DSD(CH_KEY_LATCH) = 0x77u;
    config_screen_wait_zero();
    const u32 first = DSD(CH_KEY_LATCH);
    CHECK_EQ_INT((int)first, 0x1B);
    config_screen_wait_zero();
    CHECK_EQ_INT((int)DSD(CH_KEY_LATCH), 0x0D);
    sm_seam_ok = first == 0x1Bu && DSD(CH_KEY_LATCH) == 0x0Du && sm_frame == 2u && sm_extra == 0u;
    sm_end(2u, "the seam script used exactly its two frames");
}

/* Cycle 1 (record §K11.2): the START MENU mode setters, the two MAIN MENU
 * callbacks and the screen reset 0x2F99C. */
static void sm_check_shell(void)
{
    static const struct { u32 (*fn)(u32); u32 addr, entry, mode, ah; } st[] = {
        { svc_start_arcade_left,    0x2CBC4u, 0xBCCDCu, 0x2Du, 0u },
        { svc_start_arcade_right,   0x2CBDCu, 0xBCCECu, 0x2Eu, 0u },
        { svc_start_training_left,  0x2CBF4u, 0xBCCFCu, 0x28u, 1u },
        { svc_start_training_right, 0x2CC0Cu, 0xBCD0Cu, 0x29u, 1u },
        { svc_start_tug_of_war,     0x2CC24u, 0xBCD1Cu, 0x2Au, 2u },
        { svc_start_endurance,      0x2CC3Cu, 0xBCD2Cu, 0x2Bu, 3u },
        { svc_start_handicap,       0x2CC54u, 0xBCD3Cu, 0x2Cu, 4u },
    };
    const u16 s_mode = DSW(DS_00104B00), s_mode_hi = DSW(DS_00104B00 + 2u);
    const u8 s_sub = DSB(DS_00104B1D);
    svcmenu_register();
    svcmenu_register();                                   /* idempotent */
    for (u32 i = 0; i < sizeof st / sizeof st[0]; i++) {
        /* record §0.2: the table's +8 holds this callback */
        CHECK_EQ_INT((int)DSD(st[i].entry + 8u), (int)st[i].addr);
        CHECK(fn_resolve(st[i].addr) == (void (*)(void))st[i].fn, "registered");
        DSW(DS_00104B00) = 0xBEEFu; DSW(DS_00104B00 + 2u) = 0x7777u; DSB(DS_00104B1D) = 0xA5u;
        u32 r = st[i].fn(st[i].entry);
        CHECK_EQ_INT((int)DSW(DS_00104B00), (int)st[i].mode);
        CHECK_EQ_INT((int)DSW(DS_00104B00 + 2u), 0x7777);  /* a word store */
        CHECK_EQ_INT((int)DSB(DS_00104B1D), (int)st[i].ah);
        CHECK_EQ_INT((int)r, (int)((st[i].entry & 0xFFFF00FFu) | (st[i].ah << 8)));
    }
    CHECK(fn_resolve(0x2CB74u) == (void (*)(void))svc_start_menu, "0x2CB74 registered");
    CHECK(fn_resolve(0x2CB94u) == (void (*)(void))svc_options_menu, "0x2CB94 registered");

    /* 0x2F99C: the reset menu_title_draw opens with (record §K11.2). */
    ch_text_setup();
    text_cursor_set(5, 5, (const u8 *)"Z", 0u);
    CHECK(ch_cell(5, 5) != 0u, "the pre-reset cell");
    DSB(DS_00104B15) = 1u; DSW(DS_00107A3A) = 0x77u; DSW(DS_00107A38) = 0x66u; DSD(DS_00107A1C) = 0u;
    svc_screen_reset();
    CHECK_EQ_INT((int)ch_cell(5, 5), 0);
    CHECK_EQ_INT((int)DSB(DS_00104B15), 0);
    CHECK_EQ_INT((int)DSW(DS_00107A3A), 0);
    CHECK_EQ_INT((int)DSW(DS_00107A38), 0);
    CHECK(DSD(DS_00107A1C) != 0u, "the 0x9AD84 backdrop row is spawned");

    /* START: menu_step over the stock MAIN MENU, one pad press per call. No
     * hook: menu_step presents frames only at initialisation (0x2FFF1), and
     * the pad state is set here between calls. */
    sm_env_begin();
    mem_fill(DS_00107414, 0, 0x40u);
    DSW(DS_00104B00) = 0xBEEFu; DSB(DS_00104B1D) = 0xA5u;
    CHECK_EQ_INT((int)menu_step(0xBCBDCu, 0x10u, 4u), 0);            /* init */
    CHECK_EQ_INT((int)DSD(DS_0010741C), 0xBCBEC);
    tf_menu_press(0x1000000u);                                        /* Enter on "Start" */
    CHECK_EQ_INT((int)menu_step(0xBCBDCu, 0x10u, 4u), 0);
    CHECK_EQ_INT((int)DSD(DS_0010741C), 0xBCCDC);                     /* 0x2CB74 re-initialised */
    CHECK_EQ_INT((int)DSB(DS_00107414), 1);
    CHECK_EQ_INT((int)DSD(DS_00107418), 0);                           /* ECX = 0 at 0x2CB86 */
    tf_menu_press(0u);  (void)menu_step(0xBCBDCu, 0x10u, 4u);         /* redraw, release */
    tf_menu_press(0x40004000u); (void)menu_step(0xBCBDCu, 0x10u, 4u); /* Down */
    tf_menu_press(0u);  (void)menu_step(0xBCBDCu, 0x10u, 4u);
    tf_menu_press(0x40004000u); (void)menu_step(0xBCBDCu, 0x10u, 4u); /* Down */
    tf_menu_press(0u);  (void)menu_step(0xBCBDCu, 0x10u, 4u);
    tf_menu_press(0x1000000u);
    CHECK_EQ_INT((int)menu_step(0xBCBDCu, 0x10u, 4u), 0);             /* EAX = 0x000B01FC is not -5/-10 */
    CHECK_EQ_INT((int)DSW(DS_00104B00), 0x28);                        /* item 2: LEFT TRAINING, 0x2CBF4 */
    CHECK_EQ_INT((int)DSB(DS_00104B1D), 1);
    CHECK_EQ_INT((int)DSB(DS_00107414), 0);

    /* START then Esc: the nested state has flags 0 (DS_00107418), so Esc takes
     * 0x30449..0x30466: DS_0010742C (0) != DS_00107424 (7 items walked) gives
     * -5 with DS_00107414 = 0, and the next call re-initialises MAIN MENU. */
    mem_fill(DS_00107414, 0, 0x40u);
    tf_menu_press(0u);         (void)menu_step(0xBCBDCu, 0x10u, 4u);  /* init MAIN */
    tf_menu_press(0x1000000u); (void)menu_step(0xBCBDCu, 0x10u, 4u);  /* Start: nested init */
    tf_menu_press(0u);         (void)menu_step(0xBCBDCu, 0x10u, 4u);  /* redraw, release */
    tf_menu_press(0x2000000u);
    CHECK_EQ_INT((int)menu_step(0xBCBDCu, 0x10u, 4u), -5);            /* 0x30466 */
    CHECK_EQ_INT((int)DSB(DS_00107414), 0);                           /* 0x30456 */
    tf_menu_press(0u);
    CHECK_EQ_INT((int)menu_step(0xBCBDCu, 0x10u, 4u), 0);
    CHECK_EQ_INT((int)DSD(DS_0010741C), 0xBCBEC);                     /* MAIN MENU again */
    CHECK_EQ_INT((int)DSD(DS_00107418), 4);
    sm_env_end();

    /* GAME OPTIONS: 0x2CB94 = menu_run(0xBCC1C) (blocking), 0x1B084 (deferred), 0x2C304. */
    static const sm_step_t esc_now[] = { { 0u, 0x2000000u } };
    const u32 want_credits = ((config_field_get(0x29u) & 0xF0000u) >> 16) + 1u;
    DSD(DS_00105C00) = 0xDEADu;
    sm_begin(esc_now, 1u);
    CHECK_EQ_INT((int)svc_options_menu(0xBCBFCu), -1);                /* 0x2FD53..0x2FD60 */
    sm_end(1u, "GAME OPTIONS left on the first Esc");
    CHECK_EQ_INT((int)DSD(DS_00105C00), (int)want_credits);

    DSW(DS_00104B00) = s_mode; DSW(DS_00104B00 + 2u) = s_mode_hi; DSB(DS_00104B1D) = s_sub;
}

/* Cycle 2 (record §K11.3): the option rows 0x2CC74/0x2CD30, the editor
 * 0x2CF00, CONFIG OPTIONS 0x2CACC/0x33578, SOUND TEST 0x30EB4, MUSIC TEST
 * 0x30F54 and the voice wrappers 0x2C9CC/0x2C9E8. The CONFIG OPTIONS table is
 * 0xA2EB4; its record 0 is "CREDITS" (0x46), shift 0x10, 10 values, the index
 * drawn, list 0xA2CEC (all zero but entry 4, {0x8088C "*", 0}). 31 frames. */
#define SM_CONFIG_TABLE 0x000A2EB4u
#define SM_CONFIG_DEFAULTS 0x00142095u   /* 0x2CCD0(0xA2EB4): the '*' entries (record §K11.3) */
#define SM_VOICE_REC(id) (0x000BBDC8u + (u32)(id) * 12u)
#define SM_CFG_REC 0x03D41000u           /* test-only {table, table} record for 0x2CACC */
static void sm_check_options(void)
{
    ch_text_setup();
    /* (a) 0x2CD30 on record 0 */
    actors_reset();
    CHECK_EQ_INT((int)svc_option_row(SM_CONFIG_TABLE, 3u << 0x10, 6, 0x2000u, 0u), 0xA2EC8);
    ch_expect(6, 4, 'C', 0x2000u, "label at column 4");
    ch_expect(7, 5, '4', 0x2000u, "itoa(3 + 1) on row + 1, column 5");
    CHECK_EQ_INT((int)ch_cell(7, 6), 0);                       /* no text, no string */
    actors_reset();
    (void)svc_option_row(SM_CONFIG_TABLE, 4u << 0x10, 6, 0x2000u, 0u);
    ch_expect(6, 4, 'C', 0x2000u, "the label keeps the entry mode");
    ch_expect(7, 5, '5', 0x1000u, "the '*' default switches the mode to 0x1000");
    /* release = 1 (0x2CD7E): the same cells are released, not drawn */
    (void)svc_option_row(SM_CONFIG_TABLE, 4u << 0x10, 6, 0x2000u, 1u);
    CHECK_EQ_INT((int)ch_cell(6, 4), 0);
    CHECK_EQ_INT((int)ch_cell(7, 5), 0);
    /* a text and a string (record 3 "Difficulty", shift 4, value 9: "*" and
     * 0x1F8): the index "10", then the string after the empty text. */
    actors_reset();
    CHECK_EQ_INT((int)svc_option_row(SM_CONFIG_TABLE + 3u * 0x14u, 9u << 4, 9, 0x4000u, 0u),
                 (int)(SM_CONFIG_TABLE + 4u * 0x14u));
    ch_expect(10, 5, '1', 0x1000u, "two-digit index");
    ch_expect(10, 6, '0', 0x1000u, "two-digit index, second digit");
    ch_expect(10, 8, (u32)(u8)game_string_get(0x1F8u)[0], 0x1000u,
              "the string after the index and one blank");
    /* (b) out of range: value 15 + 1 > 10 */
    actors_reset();
    CHECK_EQ_INT((int)svc_option_row(SM_CONFIG_TABLE, 0xFu << 0x10, 6, 0x2000u, 0u), 0);
    CHECK_EQ_INT((int)ch_cell(6, 4), 0);
    /* 0x2CC74: seven rows 3..21 from record 0; from record 4 the terminator
     * ends the walk after six rows. */
    actors_reset();
    CHECK_EQ_INT((int)svc_option_rows(SM_CONFIG_TABLE, SM_CONFIG_DEFAULTS, 0, 0xF000u, 0u),
                 (int)(SM_CONFIG_TABLE + 7u * 0x14u));
    ch_expect(21, 4, (u32)(u8)game_string_get(0x1FEu)[0], 0xF000u, "row 21 is record 6");
    actors_reset();
    CHECK_EQ_INT((int)svc_option_rows(SM_CONFIG_TABLE, SM_CONFIG_DEFAULTS, 4, 0xF000u, 0u), 0);
    ch_expect(18, 4, 'R', 0xF000u, "record 9 on row 18");
    CHECK_EQ_INT((int)svc_option_rows(SM_CONFIG_TABLE, SM_CONFIG_DEFAULTS, -1, 0xF000u, 0u), 0);

    /* (c) 0x2CF00 alone on the CONFIG table: Left from CREDITS 0 wraps to 9 */
    static const sm_step_t left_enter[] = { { 0x4B00u, 0u }, { 0x1C0Du, 0u } };
    /* the four repeat words are put back by test_svcmenu */
    DSD(DS_000E1C3C) = 0x12345678u; DSW(DS_000E1C44) = 0x7777u;
    actors_reset();
    sm_begin(left_enter, 2u);
    CHECK_EQ_INT((int)svc_option_edit(SM_CONFIG_TABLE, 0x00000000u, 0x4000000u, 0u), (int)(9u << 16));
    sm_end(2u, "option editor: Left, Enter");
    CHECK_EQ_INT((int)DSD(DS_000E1C3C), (int)0xF000F000u);   /* 0x2CFE4..0x2CFF3 0x50146 */
    CHECK_EQ_INT((int)DSW(DS_000E1C44), 0xF);
    ch_expect(4, 5, '1', 0x2000u, "the edited row: index 10");
    ch_expect(4, 6, '0', 0x2000u, "the edited row: index 10, second digit");
    ch_expect(23, 9, 0x3Bu, 0x1000u, "10 records: the MORE marker, first glyph 0x3B");
    ch_expect(23, 16, 0x3Bu, 0x1000u, "the MORE marker, last glyph 0x3B");
    static const sm_step_t esc_only[] = { { 0x011Bu, 0u } };
    sm_begin(esc_only, 1u);
    CHECK_EQ_INT((int)svc_option_edit(SM_CONFIG_TABLE, 5u << 16, 0x4000000u, 1u), -1);
    sm_end(1u, "option editor: Esc cancels");
    sm_begin(esc_only, 1u);
    CHECK_EQ_INT((int)svc_option_edit(SM_CONFIG_TABLE, 5u << 16, 0x4000000u, 0u), (int)(5u << 16));
    sm_end(1u, "option editor: Esc keeps");
    /* Right from CREDITS 9 wraps to 0; then the mask key (0x2D038) puts the
     * entry bits back */
    static const sm_step_t right_enter[] = { { 0x4D00u, 0u }, { 0x1C0Du, 0u } };
    sm_begin(right_enter, 2u);
    CHECK_EQ_INT((int)svc_option_edit(SM_CONFIG_TABLE, 9u << 16, 0x4000000u, 0u), 0);
    sm_end(2u, "option editor: Right wraps");
    static const sm_step_t restore[] = { { 0x4D00u, 0u }, { 0u, 0x4000000u }, { 0x1C0Du, 0u } };
    sm_begin(restore, 3u);
    CHECK_EQ_INT((int)svc_option_edit(SM_CONFIG_TABLE, 5u << 16, 0x4000000u, 0u), (int)(5u << 16));
    sm_end(3u, "option editor: Right, restore, Enter");
    /* The Up/Down hold check 0x2D1BE..0x2D1CF: pad Up pressed on an earlier
     * frame and still held is latched, so 0x2EDE0(0xF300F000) has no Up edge
     * and the pass reaches the second poll, whose level has it: the latched
     * Left is skipped and CREDITS keeps 5. */
    static const sm_step_t held_up[] = { { 0x4B00u, 0x80000000u }, { 0x1C0Du, 0u } };
    sm_begin(held_up, 2u);
    sm_held = 0x80000000u;
    CHECK_EQ_INT((int)svc_option_edit(SM_CONFIG_TABLE, 5u << 16, 0x4000000u, 0u), (int)(5u << 16));
    sm_end(2u, "option editor: a Left under a held Up is skipped");

    /* (d) the voice wrappers, with no DIG driver and no sequence */
    const u32 s_5c = DSD(DS_00105D5C), s_c0 = DSD(DS_001028C0), s_c8 = DSD(DS_001028C8);
    const u32 s_cc = DSD(DS_001028CC), s_d4 = DSD(DS_001028D4);
    const u8 s_d9 = DSB(DS_001028D9), s_da = DSB(DS_001028DA), s_db = DSB(DS_001028DB);
    const u8 s_d7 = DSB(SM_VOICE_REC(0xD7u)), s_ea = DSB(SM_VOICE_REC(0xEAu));
    DSD(DS_001028C0) = 0u; DSD(DS_001028C8) = 0u;
    DSB(DS_001028DA) = 0u; DSB(DS_001028DB) = 0u;
    DSD(DS_00105D5C) = 0xDEADu; DSD(DS_001028D4) = 0xD4D4D4D4u;
    CHECK_EQ_INT((int)svc_play_tune(0u), 1);
    CHECK_EQ_INT((int)DSD(DS_00105D5C), 0x0D000008);           /* tune 0 = voice 0x1B, case 1 */
    CHECK_EQ_INT((int)DSD(DS_001028D4), 0x0D000008);           /* the stop (0x100) came first */
    /* The samples are case 2, which leaves no mark without a DIG driver:
     * 0xEA (sample 3) is retyped to case 1 for the test. */
    DSB(SM_VOICE_REC(0xEAu)) = 1u;
    DSD(DS_00105D5C) = 0xDEADu;
    (void)svc_play_sample(3u);
    CHECK_EQ_INT((int)DSD(DS_00105D5C), 0x1201A05D);

    /* SOUND TEST: Enter plays sample 0 (voice 0xD7, retyped), Esc (CL = 1)
     * leaves through sound_voice(0x100), which clears the song. */
    DSB(SM_VOICE_REC(0xD7u)) = 1u;
    DSD(DS_00105D5C) = 0xDEADu; DSD(DS_001028D4) = 0xD4D4D4D4u;
    static const sm_step_t enter_esc[] = { { 0x1C0Du, 0u }, { 0x011Bu, 0u } };
    sm_begin(enter_esc, 2u);
    CHECK_EQ_INT((int)svc_sound_test(0xBCC4Cu), 1);
    sm_end(2u, "SOUND TEST: Enter, Esc");
    CHECK_EQ_INT((int)DSD(DS_00105D5C), 0x02807ACC);
    CHECK_EQ_INT((int)DSD(DS_001028D4), 0);
    /* MUSIC TEST: Right, Enter (tune 1), Right, Enter (tune 2: the next edit
     * starts from the last result), Esc. */
    DSD(DS_00105D5C) = 0xDEADu; DSD(DS_001028D4) = 0xD4D4D4D4u;
    static const sm_step_t music[] = {
        { 0x4D00u, 0u }, { 0x1C0Du, 0u }, { 0x4D00u, 0u }, { 0x1C0Du, 0u }, { 0x011Bu, 0u } };
    sm_begin(music, 5u);
    CHECK_EQ_INT((int)svc_music_test(0xBCC5Cu), 1);
    sm_end(5u, "MUSIC TEST: Right, Enter, Right, Enter, Esc");
    CHECK_EQ_INT((int)DSD(DS_00105D5C), 0x0B800008);           /* tune 2 = voice 0x1D */
    CHECK_EQ_INT((int)DSD(DS_001028D4), 0);
    DSB(SM_VOICE_REC(0xD7u)) = s_d7; DSB(SM_VOICE_REC(0xEAu)) = s_ea;
    DSD(DS_00105D5C) = s_5c; DSD(DS_001028C0) = s_c0; DSD(DS_001028C8) = s_c8;
    DSD(DS_001028CC) = s_cc; DSD(DS_001028D4) = s_d4;
    DSB(DS_001028D9) = s_d9; DSB(DS_001028DA) = s_da; DSB(DS_001028DB) = s_db;

    /* (e) CONFIG OPTIONS end to end, and 0x2CACC's table argument. The raw
     * store 0x2FA01 makes DS_0010740C = 0x1D2D0, whose +4 is the table. The
     * test uses a scratch record whose +0 is MUSIC TEST's table 0xA3060
     * (record 0 "Music Tunes", shift 0), so taking the wrong dword fails a
     * check rather than walking the 0x32700 fallback. */
    const u32 s_740c = DSD(DS_0010740C);
    CHECK_EQ_INT((int)DSD(0x0001D2D4u), (int)SM_CONFIG_TABLE);       /* [0x1D2D0 + 4] */
    DSD(SM_CFG_REC) = 0x000A3060u; DSD(SM_CFG_REC + 4u) = SM_CONFIG_TABLE;
    DSD(DS_0010740C) = SM_CFG_REC;
    const u32 f0 = config_field_get(0x29u);
    const u32 v0 = (f0 & ~0x000F8000u) | (2u << 16);           /* bit 15 clear, CREDITS = 2 */
    config_field_set(0x29u, v0);
    sm_begin(right_enter, 2u);
    (void)svc_config_options_entry(0xBCC2Cu);
    sm_end(2u, "CONFIG OPTIONS: Right, Enter");
    CHECK_EQ_INT((int)config_field_get(0x29u), (int)(v0 + (1u << 16)));
    ch_expect(3, 4, 'C', 0x2000u, "0x2CACC hands [+4] = 0xA2EB4 (CREDITS), not [+0]");
    /* Bit 15 set: the defaults are written before the edit (0x335C3..0x335D3;
     * skipped, Right would wrap the seeded bit 15 to 0 and nothing would
     * default). Nine Downs scroll to record 9 "Restore Factory Default?"
     * (shift 0xF), Right sets bit 15, and the defaults are written after the
     * edit (0x33680..0x33695). The window is then records 3..9. */
    config_field_set(0x29u, v0 | 0x8000u);
    static const sm_step_t scroll[] = {
        { 0x5000u, 0u }, { 0x5000u, 0u }, { 0x5000u, 0u }, { 0x5000u, 0u }, { 0x5000u, 0u },
        { 0x5000u, 0u }, { 0x5000u, 0u }, { 0x5000u, 0u }, { 0x5000u, 0u },
        { 0x4D00u, 0u }, { 0x1C0Du, 0u } };
    sm_begin(scroll, 11u);
    (void)svc_config_options(SM_CONFIG_TABLE);
    sm_end(11u, "CONFIG OPTIONS: nine Downs, Right, Enter");
    CHECK_EQ_INT((int)config_field_get(0x29u), (int)SM_CONFIG_DEFAULTS);
    ch_expect(2, 9, 0x19u, 0x1000u, "scrolled: the upper marker, glyph 0x19");
    CHECK_EQ_INT((int)ch_cell(23, 9), 0);                      /* 9 - 3 = 6: the lower marker blanked */
    ch_expect(3, 4, 'D', 0xF000u, "row 3 is record 3, Difficulty");
    ch_expect(21, 4, 'R', 0x2000u, "record 9 highlighted on row 21");
    ch_expect(22, 5, 'Y', 0x2000u, "record 9 value 1: string 0x202");
    config_field_set(0x29u, f0);
    DSD(DS_0010740C) = s_740c;
}

/* Cycle 3 (record §K11.4): 0x2F464, the volume error 0x30728, ADJUST VOLUME
 * 0x30864, the handicap row 0x30FE8 and 2 PLAYER HANDICAP 0x31138. The
 * image's layout bytes: bar rows 5, 0xD, 0x13 and label rows 3, 0xB, 0x11
 * (0xBD444..0xBD449); handicap bar rows 4, 0xC and label rows 2, 0xA
 * (0xBD459..0xBD45C). 44 frames. */
#define SM_VOL_MUTED 0x000BD458u   /* the mute byte 0x30E25/0x30E49 store */
static void sm_check_volume(void)
{
    ch_text_setup();
    /* 0x2F464: "  123" at the cursor (row 3, column 10), width 5, pad 1. */
    actors_reset();
    DSW(DS_00105F34) = 3u; DSW(DS_00105F34 + 2u) = 10u;
    text_number_cont(123, 5, 1u, 0x1000u);
    CHECK_EQ_INT((int)ch_cell(3, 10), 0);                          /* ' ' draws no cell */
    CHECK_EQ_INT((int)ch_cell(3, 11), 0);
    ch_expect(3, 12, '1', 0x1000u, "digit 1");
    ch_expect(3, 13, '2', 0x1000u, "digit 2");
    ch_expect(3, 14, '3', 0x1000u, "digit 3");
    CHECK_EQ_INT((int)DSW(DS_00105F34 + 2u), 15);                  /* the cursor moved past 5 cells */
    CHECK_EQ_INT((int)DSW(DS_00105F34), 3);

    /* 0x30728: a pass without Esc, then Esc; 26 characters centred at column
     * (0x2B - 26) >> 1 = 8 on row 6 are released (0x3077E). */
    static const sm_step_t esc2[] = { { 0u, 0u }, { 0x011Bu, 0u } };
    actors_reset();
    sm_begin(esc2, 2u);
    svc_volume_error();
    sm_end(2u, "the volume error waits for Esc");
    CHECK_EQ_INT((int)ch_cell(6, 8), 0);
    CHECK_EQ_INT((int)ch_cell(6, 33), 0);

    /* The sound state the screens touch, with no sequence and no DIG driver. */
    const u32 s_5c = DSD(DS_00105D5C), s_c0 = DSD(DS_001028C0), s_c8 = DSD(DS_001028C8);
    const u32 s_cc = DSD(DS_001028CC), s_d4 = DSD(DS_001028D4);
    const u8 s_d9 = DSB(DS_001028D9), s_da = DSB(DS_001028DA), s_db = DSB(DS_001028DB);
    const u32 s_mv = DSD(DS_000A2CB8), s_sv = DSD(DS_000A2CB4);
    const u8 s_muted = DSB(SM_VOL_MUTED);
    DSD(DS_001028C0) = 0u; DSD(DS_001028C8) = 0u;
    DSB(DS_001028DA) = 0u; DSB(DS_001028DB) = 0u;

    /* 0x2C8F0(-1) returns field 0x35 as read (0x2C9B1 `mov eax,edx`). */
    config_field_set(0x35u, 0x40u);
    DSD(DS_000A2CB8) = 0x77u;
    CHECK_EQ_INT((int)attract_config_volumes_unscaled(), 0x40);
    CHECK_EQ_INT((int)DSD(DS_000A2CB8), 0x20);

    /* ADJUST VOLUME A: music 0x40, effects 0x80, voice 1 (field 0x2A = 9, bit
     * 3 kept). Up wraps 0 -> 2, Right makes the voice 2 (the effects volume is
     * scaled, (v * 2 / 3) >> 1), Down wraps 2 -> 0, Right steps music 0x40 to
     * 0x48 (unclamped, 0x30C04), Esc. */
    config_field_set(0x35u, 0x40u); config_field_set(0x37u, 0x80u); config_field_set(0x2Au, 9u);
    DSB(SM_VOL_MUTED) = 0x5Au;
    DSD(DS_00105D5C) = 0xDEADu; DSD(DS_001028D4) = 0xD4D4D4D4u;
    DSD(DS_000A2CB8) = 0x77u; DSD(DS_000A2CB4) = 0x77u;
    DSD(DS_000E1C3C) = 0x12345678u;
    actors_reset();
    static const sm_step_t vol_a[] = {
        { 0u, 0u }, { 0x4800u, 0u }, { 0x4D00u, 0u }, { 0x5000u, 0u }, { 0x4D00u, 0u },
        { 0x011Bu, 0u }, { 0u, 0u } };
    sm_begin(vol_a, 7u);
    CHECK_EQ_INT((int)svc_adjust_volume(0xBCC9Cu), 0);             /* 0x30EA8 */
    sm_end(7u, "ADJUST VOLUME: Up, Right, Down, Right, Esc");
    CHECK_EQ_INT((int)config_field_get(0x2Au), 0xA);               /* (f & ~3) | 2 */
    CHECK_EQ_INT((int)config_field_get(0x35u), 0x48);              /* 0x40 + 8 */
    CHECK_EQ_INT((int)DSD(DS_000A2CB8), 0x24);                     /* sel 0 last: 0x48 >> 1 */
    CHECK_EQ_INT((int)DSD(DS_000A2CB4), 0x2A);                     /* sel 2: (0x80 * 2 / 3) >> 1 */
    CHECK_EQ_INT((int)DSB(SM_VOL_MUTED), 0);                       /* the first pass unmutes */
    CHECK_EQ_INT((int)DSD(DS_00105D5C), 0x21);                     /* sound_voice(3), case 4 */
    CHECK_EQ_INT((int)DSD(DS_001028D4), 0);                        /* sound_voice(0x100) on the way out */
    CHECK_EQ_INT((int)DSD(DS_000E1C3C), (int)0xF000F000u);         /* 0x30B39..0x30B40 0x50146 */
    ch_expect(3, 16, 'G', 0x3000u, "GAME MUSIC selected");
    ch_expect(0x11, 15, 'A', 0x4000u, "ATTRACT RATIO not selected");
    ch_expect(0x16, 5, '2', 0xF000u, "voice level 2");
    ch_expect(0x16, 6, '/', 0xF000u, "the slash 0x80B60");
    ch_expect(0x16, 7, '3', 0xF000u, "of 3, through 0x2F464");
    ch_expect(0x13, 15, 0x13u, 0x1000u, "voice bar: (0x80 * 2) / 3 = 0x55 covers cell 10");
    ch_expect(0x13, 16, 0x13u, 0xF000u, "voice bar: cell 11 is past it");
    ch_expect(5, 14, 0x13u, 0x1000u, "music bar: 0x48 covers cell 9");
    ch_expect(5, 15, 0x13u, 0xF000u, "music bar: cell 10 is past it");

    /* B: Left takes music 0x40 to 0x38, Down, Right clamps effects 0xFC to
     * 0xFF (0x30BFC `jg`), Esc. */
    config_field_set(0x35u, 0x40u); config_field_set(0x37u, 0xFCu); config_field_set(0x2Au, 9u);
    DSD(DS_000A2CB8) = 0x77u; DSD(DS_000A2CB4) = 0x77u;
    actors_reset();
    static const sm_step_t vol_b[] = {
        { 0u, 0u }, { 0x4B00u, 0u }, { 0x5000u, 0u }, { 0x4D00u, 0u }, { 0x011Bu, 0u }, { 0u, 0u } };
    sm_begin(vol_b, 6u);
    (void)svc_adjust_volume(0xBCC9Cu);
    sm_end(6u, "ADJUST VOLUME: Left, Down, Right, Esc");
    CHECK_EQ_INT((int)config_field_get(0x35u), 0x38);
    CHECK_EQ_INT((int)config_field_get(0x37u), 0xFF);
    CHECK_EQ_INT((int)config_field_get(0x2Au), 9);
    CHECK_EQ_INT((int)DSD(DS_000A2CB8), 0x1C);                     /* 0x38 >> 1 */
    CHECK_EQ_INT((int)DSD(DS_000A2CB4), 0x7F);                     /* 0xFF >> 1 */
    ch_expect(0xB, 15, 'G', 0x3000u, "GAME SAMPLES selected");
    ch_expect(0x16, 5, '1', 0xF000u, "voice level 1, drawn before the loop");

    /* C: Left clamps music 5 to 0 (0x30BB9 `jl`), which mutes: voice 0x22 and
     * the byte 0xBD458 = 1 (0x30E07..0x30E25). */
    config_field_set(0x35u, 5u); config_field_set(0x37u, 0x80u);
    DSB(SM_VOL_MUTED) = 0u;
    DSD(DS_000A2CB8) = 0x77u;
    static const sm_step_t vol_c[] = { { 0u, 0u }, { 0x4B00u, 0u }, { 0x011Bu, 0u }, { 0u, 0u } };
    sm_begin(vol_c, 4u);
    (void)svc_adjust_volume(0xBCC9Cu);
    sm_end(4u, "ADJUST VOLUME: Left to 0, Esc");
    CHECK_EQ_INT((int)config_field_get(0x35u), 0);
    CHECK_EQ_INT((int)DSB(SM_VOL_MUTED), 1);
    CHECK_EQ_INT((int)DSD(DS_000A2CB8), 0);
    /* D: a pass with no key still redraws (EDI = 1 from 0x308BC): GAME MUSIC
     * turns 0x3000 and the mute byte is cleared through sound_voice(3). */
    config_field_set(0x35u, 0x40u);
    DSB(SM_VOL_MUTED) = 0x5Au;
    static const sm_step_t vol_d[] = { { 0u, 0u }, { 0u, 0u }, { 0x011Bu, 0u }, { 0u, 0u } };
    sm_begin(vol_d, 4u);
    (void)svc_adjust_volume(0xBCC9Cu);
    sm_end(4u, "ADJUST VOLUME: a pass without a key, Esc");
    CHECK_EQ_INT((int)DSB(SM_VOL_MUTED), 0);
    ch_expect(3, 16, 'G', 0x3000u, "the first pass highlights GAME MUSIC");

    DSD(DS_00105D5C) = s_5c; DSD(DS_001028C0) = s_c0; DSD(DS_001028C8) = s_c8;
    DSD(DS_001028CC) = s_cc; DSD(DS_001028D4) = s_d4;
    DSB(DS_001028D9) = s_d9; DSB(DS_001028DA) = s_da; DSB(DS_001028DB) = s_db;
    DSD(DS_000A2CB8) = s_mv; DSD(DS_000A2CB4) = s_sv;
    DSB(SM_VOL_MUTED) = s_muted;

    /* 0x30FE8: 100 on row 4. "    " is released from column 0x10 on rows 8
     * and 9, the number goes at column 0x12, and bar cell (100 - 0x32) / 5 =
     * 10 is 0xF000, the cells below 0x3000, above 0x1000. */
    actors_reset();
    text_cursor_set(0x10, 8, (const u8 *)"ZZ", 0u);
    svc_handicap_row(0x64u, 4);
    CHECK_EQ_INT((int)ch_cell(8, 0x10), 0);
    CHECK_EQ_INT((int)ch_cell(8, 0x11), 0);
    CHECK(ch_cell(8, 0x12) != 0u, "100: its first digit at column 0x12");
    ch_expect(4, 0xB, 0x13u, 0x3000u, "bar cell 0");
    ch_expect(5, 0x14, 0x13u, 0x3000u, "bar cell 9, row + 1");
    ch_expect(6, 0x15, 0x13u, 0xF000u, "bar cell 10 is the value, row + 2");
    ch_expect(4, 0x16, 0x13u, 0x1000u, "bar cell 11");
    ch_expect(4, 0x1F, 0x13u, 0x1000u, "bar cell 20, the last");
    CHECK_EQ_INT((int)ch_cell(4, 0x20), 0);
    /* Below 100 (0x10, clamped to 0x32): released from 0x12, drawn at 0x14. */
    actors_reset();
    text_cursor_set(0x12, 8, (const u8 *)"ZZ", 0u);
    svc_handicap_row(0x10u, 4);
    CHECK_EQ_INT((int)ch_cell(8, 0x12), 0);
    CHECK(ch_cell(8, 0x14) != 0u, "50: its first digit at column 0x14");
    ch_expect(4, 0xB, 0x13u, 0xF000u, "clamped low: cell 0 is the value");
    ch_expect(4, 0xC, 0x13u, 0x1000u, "clamped low: cell 1 above it");
    actors_reset();
    svc_handicap_row(0x200u, 4);
    ch_expect(4, 0x1F, 0x13u, 0xF000u, "clamped high: cell 20 is the value");
    ch_expect(4, 0x1E, 0x13u, 0x3000u, "clamped high: cell 19 below it");

    /* 2 PLAYER HANDICAP: the packed record's +0x24/+0x26 are the mirror bytes
     * DS_001014D0/DS_001014D2. A: Right clamps 0x93 to 0x96 (0x3132D `jg`),
     * Down, Left clamps 0x36 to 0x32 (0x312FF `jl`), Esc. */
    u8 s_keys[0x28];
    memcpy(s_keys, mem + DS_001014AC, sizeof s_keys);
    /* 0x1AE28 also writes the record at [DS_00101514] (+0x2D4..+0x2EF). The
     * harness points DS_00101514 at MT_LAYOUT while a screen runs, so the
     * suite's own record, seeded with 0xA5, must come out untouched. */
    const u32 kb = DSD(DS_00101514);
    u8 s_kb[0x1C], seed_kb[0x1C];
    memcpy(s_kb, mem + kb + 0x2D4u, sizeof s_kb);
    memset(seed_kb, 0xA5, sizeof seed_kb);
    memcpy(mem + kb + 0x2D4u, seed_kb, sizeof seed_kb);
    const u32 s_68 = DSD(DS_00107468), s_6c = DSD(DS_0010746C);
    DSB(DS_001014D0) = 0x93u; DSB(DS_001014D2) = 0x36u;
    DSD(DS_00107468) = 0xDEADBEEFu; DSD(DS_0010746C) = 0xDEADBEEFu;
    DSD(DS_000E1C3C) = 0x12345678u;
    actors_reset();
    static const sm_step_t hcp_a[] = {
        { 0u, 0u }, { 0x4D00u, 0u }, { 0x5000u, 0u }, { 0x4B00u, 0u }, { 0x011Bu, 0u }, { 0u, 0u } };
    sm_begin(hcp_a, 6u);
    CHECK_EQ_INT((int)svc_handicap(0xBCCACu), -1);                 /* EAX: 0x2EA78's -1 */
    sm_end(6u, "HANDICAP: Right, Down, Left, Esc");
    CHECK_EQ_INT((int)DSD(DS_00107468), 0x96);                     /* 0x313CE */
    CHECK_EQ_INT((int)DSD(DS_0010746C), 0x32);                     /* 0x313D7 */
    CHECK_EQ_INT((int)DSB(DS_001014D0), 0x96);                     /* 0x1AE28 applies +0x24 */
    CHECK_EQ_INT((int)DSB(DS_001014D2), 0x32);                     /* and +0x26 */
    CHECK_EQ_INT((int)DSD(DS_000E1C3C), (int)0xF000F000u);         /* 0x31284..0x3128B 0x50146 */
    ch_expect(2, 16, 'L', 0x4000u, "LEFT PLAYER not selected");
    ch_expect(0xA, 15, 'R', 0x3000u, "RIGHT PLAYER selected");
    ch_expect(4, 0x1F, 0x13u, 0xF000u, "left bar at 0x96");
    ch_expect(0xC, 0xB, 0x13u, 0xF000u, "right bar at 0x32");
    /* F: unclamped steps (0x31304 `lea ecx,[edx-5]`, 0x31335 `lea edx,[esi+5]`):
     * Left takes 0x64 to 0x5F, Down, Right takes 0x50 to 0x55, Esc. The bars
     * put 0xF000 on cell (v - 0x32) / 5: 9 (column 0x14) and 7 (0x12). */
    DSB(DS_001014D0) = 0x64u; DSB(DS_001014D2) = 0x50u;
    DSD(DS_00107468) = 0xDEADBEEFu; DSD(DS_0010746C) = 0xDEADBEEFu;
    static const sm_step_t hcp_f[] = {
        { 0u, 0u }, { 0x4B00u, 0u }, { 0x5000u, 0u }, { 0x4D00u, 0u }, { 0x011Bu, 0u }, { 0u, 0u } };
    sm_begin(hcp_f, 6u);
    (void)svc_handicap(0xBCCACu);
    sm_end(6u, "HANDICAP: Left, Down, Right, Esc");
    CHECK_EQ_INT((int)DSD(DS_00107468), 0x5F);
    CHECK_EQ_INT((int)DSD(DS_0010746C), 0x55);
    ch_expect(4, 0x14, 0x13u, 0xF000u, "left bar: 0x5F is cell 9");
    ch_expect(4, 0x15, 0x13u, 0x1000u, "left bar: cell 10 above it");
    ch_expect(0xC, 0x12, 0x13u, 0xF000u, "right bar: 0x55 is cell 7");
    ch_expect(0xC, 0x11, 0x13u, 0x3000u, "right bar: cell 6 below it");
    /* B: Left, then Enter puts the packed record's value back
     * (0x312B1..0x312CF), Esc. */
    DSB(DS_001014D0) = 0x64u; DSB(DS_001014D2) = 0x50u;
    DSD(DS_00107468) = 0xDEADBEEFu; DSD(DS_0010746C) = 0xDEADBEEFu;
    static const sm_step_t hcp_b[] = {
        { 0u, 0u }, { 0x4B00u, 0u }, { 0x1C0Du, 0u }, { 0x011Bu, 0u }, { 0u, 0u } };
    sm_begin(hcp_b, 5u);
    (void)svc_handicap(0xBCCACu);
    sm_end(5u, "HANDICAP: Left, Enter, Esc");
    CHECK_EQ_INT((int)DSD(DS_00107468), 0x64);
    CHECK_EQ_INT((int)DSD(DS_0010746C), 0x50);
    /* E: a pass with no key still redraws (EBP = menu_run's 0x10): LEFT
     * PLAYER turns 0x3000. */
    DSD(DS_00107468) = 0xDEADBEEFu;
    static const sm_step_t hcp_e[] = { { 0u, 0u }, { 0u, 0u }, { 0x011Bu, 0u }, { 0u, 0u } };
    sm_begin(hcp_e, 4u);
    (void)svc_handicap(0xBCCACu);
    sm_end(4u, "HANDICAP: a pass without a key, Esc");
    ch_expect(2, 16, 'L', 0x3000u, "the first pass highlights LEFT PLAYER");
    CHECK_EQ_INT((int)DSD(DS_00107468), 0x64);
    CHECK(memcmp(mem + kb + 0x2D4u, seed_kb, sizeof seed_kb) == 0,
          "the suite's key record at [DS_00101514] is untouched");
    memcpy(mem + kb + 0x2D4u, s_kb, sizeof s_kb);
    memcpy(mem + DS_001014AC, s_keys, sizeof s_keys);
    DSD(DS_00107468) = s_68; DSD(DS_0010746C) = s_6c;

    CHECK(fn_resolve(0x30864u) == (void (*)(void))svc_adjust_volume, "0x30864 registered");
    CHECK(fn_resolve(0x31138u) == (void (*)(void))svc_handicap, "0x31138 registered");
}

/* Cycle 4 (record §K11.5): the hex text 0x2EF48/0x2F48C, the stick 0x314A0,
 * the key names around it 0x319B0/0x31A78/0x31B94/0x31C78, MODIFY CONTROLS
 * 0x31F24 and TEST CONTROLS 0x32358. */
#define SM_CTRL_REC 0x03D42000u          /* test-only key-config record */

/* Fills `n` cells of row `row` from column `col` with 'Z' (mode 0). */
static void sm_fill_row(s32 col, s32 row, u32 n)
{
    u8 z[0x30];
    memset(z, 'Z', n);
    z[n] = 0u;
    text_cursor_set(col, row, z, 0u);
}

static void sm_check_controls(void)
{
    ch_text_setup();
    u8 *b = mem + CH_DEST;
    /* 0x2EF48: the digits from 0x2EF10, right-aligned; the pad is ' ' when ECX
     * is non-zero (0x2EF65 `test ecx,ecx`), else '0'; EAX = the digit count. */
    memset(b, 0xEE, 0x20);
    CHECK_EQ_INT((int)text_hex_format(0x2Au, b, 4, 1u), 2);
    CHECK(memcmp(b, "  2A", 4) == 0 && b[4] == 0, "space-padded hex");
    memset(b, 0xEE, 0x20);
    CHECK_EQ_INT((int)text_hex_format(0xBEEFu, b, 6, 0u), 4);      /* the digit count, not width - digits */
    CHECK(memcmp(b, "00BEEF", 6) == 0 && b[6] == 0, "zero-padded hex");
    memset(b, 0xEE, 0x20);
    CHECK_EQ_INT((int)text_hex_format(0x12345u, b, 3, 0u), 3);     /* the high digits are dropped */
    CHECK(memcmp(b, "345", 3) == 0 && b[3] == 0, "hex truncated to the width");
    memset(b, 0xEE, 0x20);
    CHECK_EQ_INT((int)text_hex_format(0u, b, 3, 0x100u), 1);       /* zero is one digit; the flag is all of ECX */
    CHECK(memcmp(b, "  0", 3) == 0 && b[3] == 0, "zero, space-padded");

    /* 0x2F48C: EAX col, EDX row, EBX value, ECX width, then the stack pad and
     * mode (`ret 8`). */
    actors_reset();
    text_hex_set(3, 10, 0xBEEFu, 6, 0u, 0x3000u);
    ch_expect(10, 3, '0', 0x3000u, "hex: the pad digit");
    ch_expect(10, 5, 'B', 0x3000u, "hex: B");
    ch_expect(10, 8, 'F', 0x3000u, "hex: the last digit");
    CHECK_EQ_INT((int)ch_cell(10, 9), 0);
    text_hex_set(3, 12, 0x2Au, 4, 1u, 0x1000u);
    CHECK_EQ_INT((int)ch_cell(12, 4), 0);                          /* ' ' draws no cell */
    ch_expect(12, 5, '2', 0x1000u, "space-padded hex: 2");
    ch_expect(12, 6, 'A', 0x1000u, "space-padded hex: A");

    /* 0x314A0: 3x3 cells two apart around (col, row); bit 4 (the centre)
     * moves to 3 on Left, 5 on Right, then >> 3 on Up, << 3 on Down. */
    actors_reset();
    sm_fill_row(9, 9, 1u);
    svc_stick_draw(10, 11, 0u);
    CHECK_EQ_INT((int)ch_cell(9, 9), 0);                           /* 5 cells released from column 8 */
    ch_expect(11, 10, '+', 0x4000u, "stick: no bit lights the centre");
    ch_expect(9, 8, '.', 0x4000u, "stick: the top left is unlit");
    ch_expect(13, 12, '.', 0x4000u, "stick: the bottom right is unlit");
    svc_stick_draw(10, 11, 0xA0000000u);
    ch_expect(9, 8, '+', 0x4000u, "stick: Up + Left lights the top left");
    ch_expect(11, 10, '.', 0x4000u, "stick: then the centre is unlit");
    ch_expect(13, 8, '.', 0x4000u, "stick: and the bottom left too");
    svc_stick_draw(10, 11, 0x5000u);
    ch_expect(13, 12, '+', 0x4000u, "stick: Down + Right lights the bottom right");
    ch_expect(9, 12, '.', 0x4000u, "stick: the top right is unlit");

    /* 0x319B0: string 0x22C (nine blanks) released from (col - 8, row + 8),
     * (col + 2, row + 8), (col - 8, row + 0xC), (col + 2, row + 0xC). */
    actors_reset();
    sm_fill_row(1, 0x13, 20u);
    sm_fill_row(1, 0x17, 20u);
    svc_buttons_clear(0xA, 0xB);
    CHECK(ch_cell(0x13, 1) != 0u, "buttons clear: column 1 is kept");
    CHECK_EQ_INT((int)ch_cell(0x13, 2), 0);
    CHECK_EQ_INT((int)ch_cell(0x13, 10), 0);
    CHECK(ch_cell(0x13, 11) != 0u, "buttons clear: column 11 is kept");
    CHECK_EQ_INT((int)ch_cell(0x13, 12), 0);
    CHECK_EQ_INT((int)ch_cell(0x13, 20), 0);
    CHECK_EQ_INT((int)ch_cell(0x17, 2), 0);
    CHECK(ch_cell(0x17, 11) != 0u, "buttons clear: row 0x17 column 11 is kept");
    CHECK_EQ_INT((int)ch_cell(0x17, 20), 0);

    /* 0x31A78: the four words from +0xA (side 0) or +0x1C, each "<name>"
     * centred on col - 5 / col + 5, rows row + 8 / row + 0xC. A key with no
     * name draws the buffer left by the one before. */
    mem_fill(SM_CTRL_REC, 0, 0x28u);
    DSW(SM_CTRL_REC + 0xAu) = 0x1E41u;                             /* 'A' */
    DSW(SM_CTRL_REC + 0xCu) = 0x1F53u;                             /* 'S' */
    DSW(SM_CTRL_REC + 0xEu) = 0x4800u;                             /* UP */
    DSW(SM_CTRL_REC + 0x1Cu) = 0x2C5Au;                            /* 'Z', side 1 */
    actors_reset();
    svc_buttons_draw(0u, SM_CTRL_REC, 0xA, 0xB);
    ch_expect(0x13, 4, '<', 0x4000u, "button 0: <A> centred on column 5");
    ch_expect(0x13, 5, 'A', 0x4000u, "button 0: A");
    ch_expect(0x13, 15, 'S', 0x4000u, "button 1: <S> centred on column 15");
    ch_expect(0x17, 4, 'U', 0x4000u, "button 2: <UP> from column 3, row + 0xC");
    ch_expect(0x17, 14, 'U', 0x4000u, "button 3 has no name: <UP> again from column 13");
    CHECK_EQ_INT((int)ch_cell(0x17, 12), 0);
    svc_buttons_draw(1u, SM_CTRL_REC, 0x1E, 0xB);
    ch_expect(0x13, 0x19, 'Z', 0x4000u, "side 1 reads +0x1C: <Z> centred on column 0x19");

    /* 0x31B94: blanks released across rows 8 and 0xE from column c - 3 and
     * down columns c - 3 and c + 3 from row 8 (c = 0xA, or 0x1E for side 1). */
    actors_reset();
    sm_fill_row(7, 8, 10u);
    sm_fill_row(7, 0xE, 10u);
    text_vertical_set(7, 8, (const u8 *)"ZZZZZZZZZZ", 0u);
    text_vertical_set(13, 8, (const u8 *)"ZZZZZZZZZZ", 0u);
    sm_fill_row(0x1B, 8, 1u);
    svc_dirs_clear(0u);
    CHECK_EQ_INT((int)ch_cell(8, 7), 0);
    CHECK_EQ_INT((int)ch_cell(8, 15), 0);
    CHECK(ch_cell(8, 16) != 0u, "dirs clear: row 8 column 16 is kept");
    CHECK_EQ_INT((int)ch_cell(0xE, 15), 0);
    CHECK(ch_cell(0xE, 16) != 0u, "dirs clear: row 0xE column 16 is kept");
    CHECK_EQ_INT((int)ch_cell(16, 7), 0);
    CHECK(ch_cell(17, 7) != 0u, "dirs clear: column 7 row 17 is kept");
    CHECK_EQ_INT((int)ch_cell(9, 13), 0);
    CHECK_EQ_INT((int)ch_cell(16, 13), 0);
    CHECK(ch_cell(17, 13) != 0u, "dirs clear: column 13 row 17 is kept");
    CHECK(ch_cell(8, 0x1B) != 0u, "dirs clear: side 0 leaves side 1");
    svc_dirs_clear(1u);
    CHECK_EQ_INT((int)ch_cell(8, 0x1B), 0);

    /* 0x31C78: the words from +2 (side 0) or +0x14: up "<name>" on row 8 and
     * down on row 0xE centred on c; left and right unwrapped, down columns
     * c - 3 and c + 3, centred on row 0xB. */
    mem_fill(SM_CTRL_REC, 0, 0x28u);
    DSW(SM_CTRL_REC + 2u) = 0x4800u;                               /* UP */
    DSW(SM_CTRL_REC + 4u) = 0x5000u;                               /* DOWN */
    DSW(SM_CTRL_REC + 6u) = 0x4B00u;                               /* LEFT */
    DSW(SM_CTRL_REC + 8u) = 0x4D00u;                               /* RGT */
    DSW(SM_CTRL_REC + 0x14u) = 0x2C5Au;                            /* 'Z', side 1 */
    actors_reset();
    sm_fill_row(7, 8, 1u);                                         /* where "<LEFT>" would start */
    sm_fill_row(13, 9, 1u);                                        /* where "<RGT>" would start */
    svc_dirs_draw(0u, SM_CTRL_REC);
    ch_expect(8, 7, 'Z', 0u, "left is unwrapped (EDX = 1): row 8 kept");
    ch_expect(9, 13, 'Z', 0u, "right is unwrapped: row 9 kept");
    ch_expect(8, 8, '<', 0x4000u, "up: <UP> centred on column 0xA, row 8");
    ch_expect(8, 9, 'U', 0x4000u, "up: U");
    ch_expect(0xE, 7, '<', 0x4000u, "down: <DOWN> from column 7, row 0xE");
    ch_expect(0xE, 8, 'D', 0x4000u, "down: D");
    ch_expect(9, 7, 'L', 0x4000u, "left: LEFT down column 7 from row 9");
    ch_expect(12, 7, 'T', 0x4000u, "left: T on row 12");
    ch_expect(10, 13, 'R', 0x4000u, "right: RGT down column 13 from row 0xA");
    ch_expect(12, 13, 'T', 0x4000u, "right: T on row 12");
    svc_dirs_draw(1u, SM_CTRL_REC);
    ch_expect(8, 0x1E, 'Z', 0x4000u, "side 1 reads +0x14: <Z> centred on column 0x1E");

    /* The screens: the packed record comes from the mirror DS_001014AC.., its
     * device words replaced by the BIOS record's (0x31F54..0x31F85). */
    u8 s_keys[0x28];
    memcpy(s_keys, mem + DS_001014AC, sizeof s_keys);
    const u32 s_410 = DSD(DS_00107410);
    memset(mem + DS_001014AC, 0, sizeof s_keys);
    static const u8 keys1[16] = { 0x48, 0, 0x50, 0, 0x4B, 0, 0x4D, 0,
                                  0x1E, 'A', 0x1F, 'S', 0x20, 'D', 0x21, 'F' };
    memcpy(mem + DS_001014AC + 2u, keys1, sizeof keys1);

    /* MODIFY A: BIOS devices 6 and 0 (the mirror says 4 and 2). Down wraps
     * player 1 from 6 to 0; Right picks player 2; frame 4's Down falls inside
     * the 0xC ticks after frame 1's (3 ticks a frame) and is dropped; frame
     * 5's Up is past them and wraps 0 to 6; Esc applies the record. */
    DSB(DS_001014AC) = 4u; DSB(DS_001014BE) = 2u;
    static const sm_step_t mod_a[] = {
        { 0x5000u, 0u }, { 0x4D00u, 0u }, { 0u, 0u }, { 0x5000u, 0u }, { 0x4800u, 0u },
        { 0x011Bu, 0u } };
    actors_reset();
    sm_begin(mod_a, 6u);
    DSW(MT_LAYOUT + 0x2D4u) = 6u; DSW(MT_LAYOUT + 0x2D6u) = 0u;
    CHECK_EQ_INT((int)svc_modify_controls(0xBCC6Cu), 0);            /* 0x3234C */
    sm_end(6u, "MODIFY CONTROLS: Down, Right, -, Down, Up, Esc");
    CHECK_EQ_INT((int)DSW(MT_LAYOUT + 0x2D4u), 0);                 /* 0x1AE28 writes the devices back */
    CHECK_EQ_INT((int)DSW(MT_LAYOUT + 0x2D6u), 6);
    CHECK_EQ_INT((int)DSB(DS_001014AC), 0);
    CHECK_EQ_INT((int)DSB(DS_001014BE), 6);
    ch_expect(6, 7, 'K', 0x4000u, "player 1 KEYBOARD, not selected");
    ch_expect(6, 0x16, 'K', 0x3000u, "player 2 KEYBOARD/JOYSTICK, selected");
    ch_expect(8, 9, 'U', 0x4000u, "player 1's up key <UP>");
    ch_expect(0x13, 5, 'A', 0x4000u, "player 1's first button <A>");
    ch_expect(0x11, 2, 'H', 0x4000u, "label HI QUICK at (col - 3, row - 1)");
    ch_expect(0x12, 5, 'X', 0x4000u, "HI QUICK marker");
    ch_expect(0xB, 0xA, '+', 0x4000u, "player 1 stick at rest");

    /* MODIFY B: BIOS devices 4 and 0. Right, then Down takes player 2 from 0
     * to 2, which player 1's 2 BUTTON JOYSTICK turns into 4 (0x3212D..
     * 0x3213C). Player 1's key names are cleared, and HI FIERCE and LO
     * FIERCE are blank for both 2 BUTTON JOYSTICKs (their cells are seeded). */
    static const sm_step_t mod_b[] = { { 0x4D00u, 0u }, { 0x5000u, 0u }, { 0x011Bu, 0u } };
    actors_reset();
    sm_fill_row(7, 8, 1u);
    sm_fill_row(2, 0x13, 1u);
    sm_fill_row(15, 0x12, 1u); sm_fill_row(35, 0x12, 1u);
    sm_fill_row(15, 0x16, 1u); sm_fill_row(35, 0x16, 1u);
    sm_begin(mod_b, 3u);
    DSW(MT_LAYOUT + 0x2D4u) = 4u; DSW(MT_LAYOUT + 0x2D6u) = 0u;
    (void)svc_modify_controls(0xBCC6Cu);
    sm_end(3u, "MODIFY CONTROLS: Right, Down, Esc");
    CHECK_EQ_INT((int)DSW(MT_LAYOUT + 0x2D4u), 4);
    CHECK_EQ_INT((int)DSW(MT_LAYOUT + 0x2D6u), 4);
    CHECK_EQ_INT((int)ch_cell(8, 7), 0);                           /* 0x31B94 for a joystick */
    CHECK_EQ_INT((int)ch_cell(0x13, 2), 0);                        /* 0x319B0 */
    CHECK_EQ_INT((int)ch_cell(0x12, 15), 0);                       /* 0x2000000 blank for device 4 */
    CHECK_EQ_INT((int)ch_cell(0x12, 35), 0);                       /* 0x200 */
    CHECK_EQ_INT((int)ch_cell(0x16, 15), 0);                       /* 0x8000000 */
    CHECK_EQ_INT((int)ch_cell(0x16, 35), 0);                       /* 0x800 */
    ch_expect(0x12, 25, 'X', 0x4000u, "player 2 HI QUICK marker");
    ch_expect(0x16, 5, 'X', 0x4000u, "player 1 LO QUICK marker");

    /* MODIFY C: BIOS devices 2 and 6. Right, Up takes player 2 from 6 to 4,
     * which player 1's 4 BUTTON JOYSTICK turns into 0 (0x320A3..0x320C8);
     * Left picks player 1 again. */
    static const sm_step_t mod_c[] = {
        { 0x4D00u, 0u }, { 0x4800u, 0u }, { 0x4B00u, 0u }, { 0x011Bu, 0u } };
    actors_reset();
    sm_begin(mod_c, 4u);
    DSW(MT_LAYOUT + 0x2D4u) = 2u; DSW(MT_LAYOUT + 0x2D6u) = 6u;
    (void)svc_modify_controls(0xBCC6Cu);
    sm_end(4u, "MODIFY CONTROLS: Right, Up, Left, Esc");
    CHECK_EQ_INT((int)DSW(MT_LAYOUT + 0x2D4u), 2);
    CHECK_EQ_INT((int)DSW(MT_LAYOUT + 0x2D6u), 0);
    ch_expect(6, 2, '4', 0x3000u, "player 1 4 BUTTON JOYSTICK, selected again");
    ch_expect(6, 0x1B, 'K', 0x4000u, "player 2 KEYBOARD, not selected");

    /* MODIFY D: BIOS devices 4 and 4. Up takes player 1 from 4 to 2, which
     * player 2's 2 BUTTON JOYSTICK turns into 0 (0x3208A..0x3209C). */
    static const sm_step_t mod_d[] = { { 0x4800u, 0u }, { 0x011Bu, 0u } };
    actors_reset();
    sm_begin(mod_d, 2u);
    DSW(MT_LAYOUT + 0x2D4u) = 4u; DSW(MT_LAYOUT + 0x2D6u) = 4u;
    (void)svc_modify_controls(0xBCC6Cu);
    sm_end(2u, "MODIFY CONTROLS: Up, Esc");
    CHECK_EQ_INT((int)DSW(MT_LAYOUT + 0x2D4u), 0);
    CHECK_EQ_INT((int)DSW(MT_LAYOUT + 0x2D6u), 4);
    ch_expect(0x12, 15, 'X', 0x4000u, "player 1 keyboard: HI FIERCE marker");
    CHECK_EQ_INT((int)ch_cell(0x12, 35), 0);

    /* MODIFY E: BIOS devices 6 and 0. Up steps player 1 from 6 to 4
     * (0x3207A `sub esi,2`); player 2's device 0 matches no limit arm
     * (0x32081..0x3208D), so 4 stands: "2 BUTTON JOYSTICK", HI FIERCE blank. */
    static const sm_step_t mod_e[] = { { 0x4800u, 0u }, { 0x011Bu, 0u } };
    actors_reset();
    sm_fill_row(15, 0x12, 1u);
    sm_begin(mod_e, 2u);
    DSW(MT_LAYOUT + 0x2D4u) = 6u; DSW(MT_LAYOUT + 0x2D6u) = 0u;
    (void)svc_modify_controls(0xBCC6Cu);
    sm_end(2u, "MODIFY CONTROLS: Up 6 -> 4, Esc");
    CHECK_EQ_INT((int)DSW(MT_LAYOUT + 0x2D4u), 4);
    CHECK_EQ_INT((int)DSW(MT_LAYOUT + 0x2D6u), 0);
    ch_expect(6, 2, '2', 0x3000u, "player 1 2 BUTTON JOYSTICK, selected");
    CHECK_EQ_INT((int)ch_cell(0x12, 15), 0);                       /* 0x2000000 blank for device 4 */

    /* MODIFY F: BIOS devices 0 and 0. Down steps player 1 from 0 to 2
     * (0x320F6 `add esi,2`); player 2's device 0 matches no limit arm
     * (0x320FD..0x3210D), so 2 stands: "4 BUTTON JOYSTICK", the direction
     * names drawn by the first pass are released. */
    static const sm_step_t mod_f[] = { { 0x5000u, 0u }, { 0x011Bu, 0u } };
    actors_reset();
    sm_begin(mod_f, 2u);
    DSW(MT_LAYOUT + 0x2D4u) = 0u; DSW(MT_LAYOUT + 0x2D6u) = 0u;
    (void)svc_modify_controls(0xBCC6Cu);
    sm_end(2u, "MODIFY CONTROLS: Down 0 -> 2, Esc");
    CHECK_EQ_INT((int)DSW(MT_LAYOUT + 0x2D4u), 2);
    CHECK_EQ_INT((int)DSW(MT_LAYOUT + 0x2D6u), 0);
    ch_expect(6, 2, '4', 0x3000u, "player 1 4 BUTTON JOYSTICK, selected");
    CHECK_EQ_INT((int)ch_cell(8, 9), 0);                           /* "<UP>" of the first pass released */

    /* TEST A (DS_00107410 bit 4 clear): the pad level of frame 1 (player 1
     * Up + Left and HI QUICK, player 2 Down) is shown by the next pass; the
     * Esc of frame 2 is the latched 0x1B (0x3254B). */
    DSB(DS_001014AC) = 4u; DSB(DS_001014BE) = 2u;
    DSD(DS_00107410) = 0xEFu;
    static const sm_step_t test_a[] = { { 0u, 0xA1004000u }, { 0x011Bu, 0u } };
    actors_reset();
    sm_begin(test_a, 2u);
    DSW(MT_LAYOUT + 0x2D4u) = 0u; DSW(MT_LAYOUT + 0x2D6u) = 4u;
    CHECK_EQ_INT((int)svc_test_controls(0xBCC8Cu), 0);              /* 0x32632 */
    sm_end(2u, "TEST CONTROLS: a pad frame, Esc");
    ch_expect(6, 7, 'K', 0x4000u, "TEST: player 1 KEYBOARD");
    ch_expect(6, 0x16, '2', 0x4000u, "TEST: player 2 2 BUTTON JOYSTICK");
    ch_expect(8, 9, 'U', 0x4000u, "TEST: player 1's up key");
    ch_expect(9, 8, '+', 0x4000u, "TEST: player 1 stick Up + Left");
    ch_expect(11, 10, '.', 0x4000u, "TEST: player 1 stick centre unlit");
    ch_expect(13, 0x1E, '+', 0x4000u, "TEST: player 2 stick Down");
    ch_expect(0x12, 5, 'O', 0x3000u, "TEST: HI QUICK held");
    ch_expect(0x12, 15, 'X', 0x3000u, "TEST: HI FIERCE not held");
    ch_expect(0x11, 2, 'H', 0x4000u, "TEST: label HI QUICK");
    CHECK_EQ_INT((int)ch_cell(7, 3), 0);                           /* no DIAGS row */
    CHECK_EQ_INT((int)DSB(DS_001014AC), 4);                        /* no 0x1AE28: the mirror keeps its 4 */

    /* TEST B (bit 4 set): the diagnostic rows, the pad word in hex under RAW
     * DATA, and the three extra entries at 0x31410 whose "string ids" are the
     * pointers 0x80B6C/0x80B70/0x80B74 (they decode as strings 0xAC, 0xB0,
     * 0xB4 through the group ring). Rows 4 and 6 are drawn first, so the
     * option rows overwrite them; player 1's device 8 has no text, which
     * leaves the address on row 6 visible. */
    DSD(DS_00107410) = 0x10u;
    static const sm_step_t test_b[] = { { 0u, 0x10002000u }, { 0x011Bu, 0u } };
    actors_reset();
    sm_fill_row(4, 0x13, 1u);                                      /* where player 1's "<A>" would start */
    sm_begin(test_b, 2u);
    DSW(MT_LAYOUT + 0x2D4u) = 8u; DSW(MT_LAYOUT + 0x2D6u) = 4u;
    (void)svc_test_controls(0xBCC8Cu);
    sm_end(2u, "TEST CONTROLS: diagnostics, a pad frame, Esc");
    ch_expect(4, 0xE, 'R', 0x4000u, "RAW DATA (0x80BAC + 11) past LEFT PLAYER");
    ch_expect(6, 3, 'F', 0x4000u, "the address FFE80000");
    ch_expect(6, 5, 'E', 0x4000u, "the address: E");
    ch_expect(6, 6, '8', 0x4000u, "the address: 8");
    ch_expect(7, 3, 'D', 0x4000u, "DIAGS (0x80BD4)");
    ch_expect(6, 0xE, '1', 0x3000u, "the pad word 10002000");
    ch_expect(6, 0x12, '2', 0x3000u, "the pad word: 2");
    ch_expect(6, 0x15, '0', 0x3000u, "the pad word: last digit");
    CHECK(ch_cell(7, 0xE) != 0u, "the raw data row is drawn");
    ch_expect(0xA, 0x1D, 'C', 0x4000u, "0x80B6C as a string id: string 0xAC");
    ch_expect(0xA, 0x21, 'V', 0x4000u, "0x80B74 as a string id: string 0xB4 Vertigo");
    ch_expect(0xB, 0x20, 'X', 0x3000u, "bit 8 marker over the stick");
    ch_expect(0xB, 0xC, '+', 0x4000u, "player 1 stick Right");
    ch_expect(0xB, 0x1C, '+', 0x4000u, "player 2 stick Left");
    ch_expect(0x13, 4, 'Z', 0u, "device 8: no button names (0x3248A..0x32499)");

    memcpy(mem + DS_001014AC, s_keys, sizeof s_keys);
    DSD(DS_00107410) = s_410;
    CHECK(fn_resolve(0x31F24u) == (void (*)(void))svc_modify_controls, "0x31F24 registered");
    CHECK(fn_resolve(0x32358u) == (void (*)(void))svc_test_controls, "0x32358 registered");
}

/* Cycle 5 (record §K11.6): the key slots 0x19C60/0x19D34, the raw key take
 * 0x2EBBC and CONFIGURE KEYBOARD 0x19DF0. */
#define SM_KEYS_REC 0x00100CACu          /* the record 0x19DF0 packs, edits and applies */

/* Seeds the mirror DS_001014AC.. with the key words `w[16]` in slot order,
 * through the ported 0x1AE28 of a test-only record. */
static void sm_seed_slots(const u16 *w)
{
    static const u8 off[16] = { 2, 8, 4, 6, 0xA, 0xC, 0xE, 0x10,
                                0x14, 0x1A, 0x16, 0x18, 0x1C, 0x1E, 0x20, 0x22 };
    mem_fill(SM_CTRL_REC, 0, 0x28u);
    for (u32 s = 0; s < 16u; s++) DSW(SM_CTRL_REC + off[s]) = w[s];
    config_keys_apply(SM_CTRL_REC);
}

static void sm_check_keyboard(void)
{
    static const u32 slot_addr[16] = {
        0x100CAEu, 0x100CB4u, 0x100CB0u, 0x100CB2u, 0x100CB6u, 0x100CB8u, 0x100CBAu, 0x100CBCu,
        0x100CC0u, 0x100CC6u, 0x100CC2u, 0x100CC4u, 0x100CC8u, 0x100CCAu, 0x100CCCu, 0x100CCEu,
    };
    u8 saved[0x34];                      /* the record and the name buffer 0x100CD4 */
    memcpy(saved, mem + 0x100CACu, sizeof saved);
    memset(mem + 0x100CACu, 0x5A, 0x28);
    for (u32 s = 0; s < 16u; s++) {
        svc_key_slot_set(s, (u16)(0x9100u + s));         /* bit 15 set: the getter zero-extends */
        CHECK_EQ_INT((int)DSW(slot_addr[s]), (int)(0x9100u + s));
        CHECK_EQ_INT((int)svc_key_slot_get(s), (int)(0x9100u + s));
    }
    svc_key_slot_set(16u, 0x7777u);                      /* above 15: nothing */
    svc_key_slot_set(0xFFFFFFFFu, 0x7777u);              /* 0x19C60 `ja`: unsigned */
    CHECK_EQ_INT((int)DSW(0x100CACu), 0x5A5A);           /* +0 is no slot */
    CHECK_EQ_INT((int)DSW(0x100CBEu), 0x5A5A);           /* 0x100CBE is no slot */
    for (u32 s = 0; s < 16u; s++) CHECK(DSW(slot_addr[s]) != 0x7777u, "slot 16 writes nothing");
    CHECK_EQ_INT((int)svc_key_slot_get(16u), 0);
    CHECK_EQ_INT((int)svc_key_slot_get(0xFFFFFFFFu), 0); /* 0x19D37 `ja`: unsigned */
    DSD(CH_KEY_WORD) = 0x1C0Du;
    CHECK_EQ_INT((int)svc_raw_key_take(), 0x1C0D);
    CHECK_EQ_INT((int)DSD(CH_KEY_WORD), 0);

    /* 0x19DF0. The record is packed from the mirror; the key loop takes one
     * slot at a time. */
    u8 s_keys[0x28];
    memcpy(s_keys, mem + DS_001014AC, sizeof s_keys);
    const u8 s_free = DSB(DS_00105D60), s_113 = DSB(DS_00108113);
    DSB(DS_00105D60) = 0u; DSB(DS_00108113) = 0u;

    /* KEYS A: "morland" into slots 0..6 (0x1A4A8..0x1A53B sets DS_00108113);
     * at slot 7, 'D' (0x2044: shift keeps the scan code 0x20 of slot 6's 'd'
     * 0x2064; the compare is & 0xFF00, up to slot - 1) is refused and F1 has
     * no name (0x3157C returns 0) and is refused; 'x' then replaces slot 7's
     * "<HOME>" in column 0xC; slot 8 takes UP although its own old word has
     * scan 0x48 (only the slots below are compared); slot 12's "<j>" is drawn
     * after the seven blanks at 0x80590 released its old "<HOME>"; Enter
     * keeps slot 13's 'k'. After slot 15 the record is applied (0x1A551). */
    static const u16 seed_a[16] = { 0x2C7Au, 0x2C7Au, 0x2C7Au, 0x2C7Au, 0x2C7Au, 0x2C7Au, 0x2C7Au, 0x4700u,
                                    0x4830u, 0x2C7Au, 0x2C7Au, 0x2C7Au, 0x4700u, 0x256Bu, 0x2C7Au, 0x2C7Au };
    static const sm_step_t keys_a[] = {
        { 0x326Du, 0u }, { 0x186Fu, 0u }, { 0x1372u, 0u }, { 0x266Cu, 0u }, { 0x1E61u, 0u },
        { 0x316Eu, 0u }, { 0x2064u, 0u }, { 0x2044u, 0u }, { 0x3B00u, 0u }, { 0x2D78u, 0u },
        { 0x4800u, 0u }, { 0x4D00u, 0u }, { 0x5000u, 0u }, { 0x4B00u, 0u }, { 0x246Au, 0u },
        { 0x1C0Du, 0u }, { 0x1769u, 0u }, { 0x1675u, 0u } };
    static const u16 want_a[16] = { 0x326Du, 0x186Fu, 0x1372u, 0x266Cu, 0x1E61u, 0x316Eu, 0x2064u, 0x2D78u,
                                    0x4800u, 0x4D00u, 0x5000u, 0x4B00u, 0x246Au, 0x256Bu, 0x1769u, 0x1675u };
    actors_reset();
    sm_begin(keys_a, 18u);
    sm_seed_slots(seed_a);
    CHECK_EQ_INT((int)svc_configure_keyboard(0xBCC7Cu), 0);         /* 0x1A556 */
    sm_end(18u, "CONFIGURE KEYBOARD: morland, two refusals, nine keys, Enter");
    for (u32 s = 0; s < 16u; s++) CHECK_EQ_INT((int)DSW(slot_addr[s]), (int)want_a[s]);
    CHECK_EQ_INT((int)DSB(DS_00108113), 1);                          /* "morland" */
    CHECK_EQ_INT((int)DSB(DS_001014AE), 0x32);                       /* 0x1AE28: slot 0's scan */
    CHECK_EQ_INT((int)DSB(DS_001014AC + 3u), 'm');                   /* and its ascii */
    CHECK_EQ_INT((int)DSB(MT_LAYOUT + 0x2DEu), 0x32);                /* the BIOS record too */
    CHECK_EQ_INT((int)DSB(MT_LAYOUT + 0x2E6u + 3u), 0x4D);           /* slot 9 is player 2's +0x1A */
    ch_expect(4, 2, 'L', 0x2000u, "LEFT PLAYER (0x17) at (2, 4)");
    ch_expect(4, 0x16, 'R', 0x2000u, "RIGHT PLAYER (0x16) at (0x16, 4)");
    ch_expect(7, 2, 'U', 0x1000u, "label UP (0xA2C2C)");
    ch_expect(9, 2, 'R', 0x1000u, "label RIGHT (0xA2C30) second");
    ch_expect(0x15, 0x16, 'L', 0x1000u, "player 2 label LO FIERCE (0xA2C48)");
    ch_expect(7, 0xC, '<', 0xF000u, "slot 0 <m> at (0xC, 7)");
    ch_expect(7, 0xE, '>', 0xF000u, "slot 0 <m>: three cells");
    ch_expect(0x15, 0xE, '>', 0xF000u, "slot 7 <x> in column 0xC (0x1A339 `jg` above 7)");
    CHECK_EQ_INT((int)ch_cell(0x15, 0xF), 0);                        /* slot 7's old <HOME> tail released */
    ch_expect(0x11, 0x20, '<', 0xF000u, "slot 13 redrawn after Enter");
    ch_expect(7, 0x21, 'U', 0xF000u, "slot 8 <UP> at (0x20, 7)");
    ch_expect(9, 0x21, 'R', 0xF000u, "slot 9 <RGT> on row 9");
    ch_expect(0xF, 0x22, '>', 0xF000u, "slot 12 <j>");
    CHECK_EQ_INT((int)ch_cell(0xF, 0x23), 0);                        /* the old <HOME>'s tail released */

    /* KEYS B: "spaten" into slots 0..5 (0x1A426..0x1A4A1 sets the FREE PLAY
     * flag DS_00105D60), then Esc on slot 6: EAX = 0 and no 0x1AE28, so the
     * mirror and the BIOS record keep their 'z'. */
    static const u16 seed_b[16] = { 0x2C7Au, 0x2C7Au, 0x2C7Au, 0x2C7Au, 0x2C7Au, 0x2C7Au, 0x4700u, 0x2C7Au,
                                    0x2C7Au, 0x5000u, 0x2C7Au, 0x2C7Au, 0x2C7Au, 0x2C7Au, 0x2C7Au, 0x2C7Au };
    static const sm_step_t keys_b[] = {
        { 0x1F73u, 0u }, { 0x1970u, 0u }, { 0x1E61u, 0u }, { 0x1474u, 0u }, { 0x1265u, 0u },
        { 0x316Eu, 0u }, { 0x011Bu, 0u } };
    static const u16 want_b[6] = { 0x1F73u, 0x1970u, 0x1E61u, 0x1474u, 0x1265u, 0x316Eu };
    actors_reset();
    sm_begin(keys_b, 7u);
    sm_seed_slots(seed_b);
    CHECK_EQ_INT((int)svc_configure_keyboard(0xBCC7Cu), 0);         /* 0x1A3A4 -> 0x1A556 */
    sm_end(7u, "CONFIGURE KEYBOARD: spaten, Esc");
    for (u32 s = 0; s < 6u; s++) CHECK_EQ_INT((int)DSW(slot_addr[s]), (int)want_b[s]);
    CHECK_EQ_INT((int)DSB(DS_00105D60), 1);                          /* "spaten": FREE PLAY */
    CHECK_EQ_INT((int)DSB(DS_001014AE), 0x2C);                       /* Esc: not applied */
    CHECK_EQ_INT((int)DSB(MT_LAYOUT + 0x2DEu), 0x2C);
    ch_expect(0x13, 0xD, 'H', 0x3000u, "slot 6's old <HOME> highlighted when Esc left");
    ch_expect(9, 0x21, 'D', 0xF000u, "slot 9 (0x100CC6) drawn at (0x20, 9): <DOWN>");

    memcpy(mem + DS_001014AC, s_keys, sizeof s_keys);
    DSB(DS_00105D60) = s_free; DSB(DS_00108113) = s_113;
    memcpy(mem + 0x100CACu, saved, sizeof saved);
    CHECK(fn_resolve(0x19DF0u) == (void (*)(void))svc_configure_keyboard, "0x19DF0 registered");
}

/* Cycle 6 (record §K11.7): STATISTICS pages 1 and 2, 0x328B8 0x32F54
 * 0x32F98 0x33458 0x33058 0x33230. 13 frames. */
#define SM_COLS(s, m) ((0x2B - text_width(game_string_get(s), (m))) >> 1)

/* Page 1's fields (0x32644, 0x326C4, 0x32F98 read them). */
static void sm_stats_seed(void)
{
    static const struct { u32 f, v; } seed[] = {
        { 0u, 7u }, { 3u, 0x10005u }, { 4u, 0x10000u }, { 5u, 0xFFFFu }, { 6u, 3u }, { 7u, 4u },
        { 8u, 2u }, { 9u, 0xFFFFu }, { 0xAu, 250u }, { 0xBu, 0u }, { 0xCu, 999u },
        { 0x12u, 3932580u }, { 0x13u, 600u },
    };
    for (u32 i = 0; i < sizeof seed / sizeof seed[0]; i++) {
        (void)config_field_set(seed[i].f, seed[i].v);
        CHECK_EQ_INT((int)config_field_get(seed[i].f), (int)seed[i].v);   /* the field holds it */
    }
}

/* Page 2's clear: fields 0..0x27 = 1 + f % 7 (every width holds 7), 0x28 = 5. */
static void sm_stats_seed_all(void)
{
    for (u32 f = 0; f < 0x28u; f++) (void)config_field_set(f, 1u + f % 7u);
    (void)config_field_set(0x28u, 5u);
    CHECK_EQ_INT((int)config_field_get(0x27u), 1 + 0x27 % 7);
    CHECK_EQ_INT((int)config_field_get(0x28u), 5);
}

static u32 sm_stats_seen;
/* Frames are numbered from 0: sm_probe(f) runs as frame f is presented,
 * before step f is applied, so it sees what the passes through step f - 1
 * left. A pass with the redraw flag clear draws nothing: a glyph planted at
 * probe 1 (after pass 0's first draw) is still there at probe 2 (0x330AD /
 * 0x332DC). */
static void sm_stats_plant(u32 frame)
{
    if (frame == 1u) text_glyph_at(2, 'Z', 20, 0xF000u);
    if (frame == 2u && ch_cell(20, 2) != 0u) sm_stats_seen |= 0x200u;
}

/* P2A (frames from 0, as above): probes 1..3 see the fields still seeded
 * (step 0's Esc alone, step 1's Enter alone and, at probe 3, step 2's
 * Esc-release wait clear nothing); probe 1 sees the hints and the nine rows
 * of pass 0's first draw (row 12 = 0xC is released by the exit, so it is
 * looked at here); probe 4 sees the fields cleared once step 3 released
 * Esc. */
static void sm_stats_probe(u32 frame)
{
    sm_stats_plant(frame);
    if (frame >= 1u && frame <= 3u && config_field_get(0u) == 1u && config_field_get(0x27u) == 1u + 0x27u % 7u)
        sm_stats_seen |= 1u << frame;
    if (frame == 1u && ch_cell(12, 0x20) == 0u)
        sm_stats_seen |= 0x400u;                             /* nine rows: a tenth draws field 0xFF's 65535 */
    if (frame == 1u) {
        ch_expect(0x18, SM_COLS(0x69u, 0x4000u), 'H', 0x4000u, "page 2 hint 0x69 on row 0x18");
        ch_expect(0x19, SM_COLS(0x6Au, 0x4000u), 'A', 0x4000u, "page 2 hint 0x6A on row 0x19");
        ch_expect(0x1A, SM_COLS(0xA0u, 0x4000u), 't', 0x4000u, "page 2 hint 0xA0 on row 0x1A");
        sm_stats_seen |= 0x100u;
    }
    if (frame == 4u && config_field_get(0u) == 0u) sm_stats_seen |= 1u << frame;
}

static void sm_check_stats(void)
{
    const u32 s_150c = DSD(DS_0010150C), s_e4 = DSD(DS_001014E4), s_e8 = DSD(DS_001014E8);
    u8 s_dac[256][3];
    memcpy(s_dac, gfx_dac, sizeof s_dac);
    ch_text_setup();

    /* (a) 0x328B8 at the cursor (row 4, column 5): 125 s is " 2" (width 5 -
     * 3, pad 1), ':' and "05" (width 2, pad 0). */
    text_glyph_at(5, 'Z', 4, 0xF000u);
    CHECK(ch_cell(4, 5) != 0u, "a seeded glyph under the minutes' pad");
    DSW(DS_00105F34) = 4u; DSW(DS_00105F34 + 2u) = 5u;
    svc_draw_mmss(125u, 5u);
    CHECK_EQ_INT((int)ch_cell(4, 5), 0);                         /* ' ' releases it */
    ch_expect(4, 6, '2', 0xF000u, "mm:ss minutes");
    ch_expect(4, 7, ':', 0xF000u, "mm:ss colon 0x80BDC");
    ch_expect(4, 8, '0', 0xF000u, "mm:ss seconds padded with '0'");
    ch_expect(4, 9, '5', 0xF000u, "mm:ss seconds");

    /* (b) 0x32F54: 0x2CA78 is `xor eax,eax; ret`, so the average is 0 with
     * any fields. */
    sm_stats_seed();
    CHECK_EQ_INT((int)config_credit_zero(), 0);
    CHECK_EQ_INT((int)svc_stats_avg(), 0);

    DSD(DS_001014E4) = CH_BUF_A; DSD(DS_001014E8) = CH_BUF_B;

    /* P1A: a new Esc with Enter held stays (0x330A7) and draws; a pass
     * with nothing draws nothing; a new Esc alone leaves (3 frames). The
     * draw opens with 0x2F99C, which empties every cell, so a glyph seeded
     * here would not survive to be a sentinel: the empty-cell checks below
     * (row 3/4 column 0x25, row 8 column 0x24, row 14 column 28) catch a
     * glyph the draw puts there (an extra digit, a pad drawn as '0'), not a
     * pad that fails to release one. */
    static const sm_step_t p1a[] = { { 0u, 0x3000000u }, { 0u, 0u }, { 0u, 0x2000000u } };
    actors_reset();
    sm_stats_seen = 0u;
    sm_begin(p1a, 3u);
    sm_probe = sm_stats_plant;
    svc_stats_page1();
    sm_end(3u, "STATISTICS page 1: Esc with Enter held stays, Esc leaves");
    CHECK_EQ_INT((int)sm_stats_seen, 0x200);                     /* no redraw on pass 2 */
    CHECK(ch_cell(0, SM_COLS(0x81u, 0x5002u)) != 0u, "the title 0x81");
    ch_expect(3, 4, 'I', 0xF000u, "Idle Mins (0x8F) on row 3");
    ch_expect(3, 0x24, '5', 0xF000u, "field 3 as is, & 0xFFFF");
    CHECK_EQ_INT((int)ch_cell(3, 0x25), 0);
    ch_expect(4, 4, '1', 0xF000u, "1 Player Mins (0x90) on row 4");
    ch_expect(4, 0x24, '7', 0xF000u, "field 0x12 / 60 = 65543, & 0xFFFF");
    CHECK_EQ_INT((int)ch_cell(4, 0x25), 0);
    ch_expect(5, 0x24, '1', 0xF000u, "field 0x13 / 60 = 10");
    ch_expect(5, 0x25, '0', 0xF000u, "field 0x13 / 60 = 10");
    ch_expect(6, 0x24, '4', 0xF000u, "field 0xA / 60 = 4");
    ch_expect(7, 4, 'C', 0xF000u, "Cont Game Mins (0x93) on row 7");
    ch_expect(7, 0x24, '1', 0xF000u, "field 0xC / 60 = 16");
    ch_expect(7, 0x25, '6', 0xF000u, "field 0xC / 60 = 16");
    /* 0x33458 from row 8: 250 / 2 = 125; 999 / 0 is 0; 3932580 / (2 + 3),
     * & 0xFFFF = 84; 600 / ((0xFFFF + 4) & 0xFFFF) = 200. */
    ch_expect(8, 4, 'A', 0xF000u, "Ave New 1 pl time (0x94) on row 8");
    CHECK_EQ_INT((int)ch_cell(8, 0x24), 0);
    ch_expect(8, 0x25, '2', 0xF000u, "row 8 2:05");
    ch_expect(8, 0x26, ':', 0xF000u, "row 8 2:05");
    ch_expect(8, 0x27, '0', 0xF000u, "row 8 2:05");
    ch_expect(8, 0x28, '5', 0xF000u, "row 8 2:05");
    ch_expect(9, 0x25, '0', 0xF000u, "row 9 0:00 (a zero sum)");
    ch_expect(9, 0x28, '0', 0xF000u, "row 9 0:00");
    ch_expect(10, 0x25, '1', 0xF000u, "row 10 1:24");
    ch_expect(10, 0x27, '2', 0xF000u, "row 10 1:24");
    ch_expect(10, 0x28, '4', 0xF000u, "row 10 1:24");
    ch_expect(11, 4, 'A', 0xF000u, "Ave 2 pl game time (0x97) on row 11");
    ch_expect(11, 0x25, '3', 0xF000u, "row 11 3:20");
    ch_expect(11, 0x27, '2', 0xF000u, "row 11 3:20");
    ch_expect(11, 0x28, '0', 0xF000u, "row 11 3:20");
    /* 0x32F98(4, 13): AVG TIME/COIN at (5, 14), Percentage Play at (4, 15). */
    ch_expect(14, 5, 'A', 0xF000u, "AVG TIME/COIN (0x8B) at (col + 1, row + 1)");
    CHECK_EQ_INT((int)ch_cell(14, 28), 0);
    ch_expect(14, 29, '0', 0xF000u, "0x328B8(0, 6): minutes in 3 cells");
    ch_expect(14, 30, ':', 0xF000u, "0x328B8(0, 6)");
    ch_expect(14, 31, '0', 0xF000u, "0x328B8(0, 6)");
    ch_expect(14, 32, '0', 0xF000u, "0x328B8(0, 6)");
    ch_expect(15, 4, 'P', 0xF000u, "Percentage Play (0x8A) at (col, row + 2)");
    ch_expect(15, 26, '3', 0xF000u, "100 * 0xFFFF / 0x30004 = 33");
    ch_expect(15, 27, '3', 0xF000u, "100 * 0xFFFF / 0x30004 = 33");
    ch_expect(0x1B, SM_COLS(0x209u, 0x1000u), 'P', 0x1000u, "PRESS ESCAPE KEY");
    ch_expect(0x1C, SM_COLS(0x82u, 0x1000u), 'f', 0x1000u, "for more stats (0x82)");

    /* P1B: the latched Enter leaves before any draw and releases "EEPROM
     * ERROR"'s twelve cells from (0x1B, 0xC) (1 frame). P1C: the latched Esc
     * (1 frame). */
    static const sm_step_t p1b[] = { { 0x1C0Du, 0u } };
    static const sm_step_t p1c[] = { { 0x011Bu, 0u } };
    for (u32 run = 0; run < 2u; run++) {
        actors_reset();
        text_glyph_at(2, 'Z', 20, 0xF000u);
        text_glyph_at(0x1B, 'Z', 0xC, 0xF000u); text_glyph_at(0x26, 'Z', 0xC, 0xF000u);
        text_glyph_at(0x27, 'Z', 0xC, 0xF000u);
        DSD(DS_0010150C) = 0x5A5A5A5Au;
        sm_begin(run == 0u ? p1b : p1c, 1u);
        svc_stats_page1();
        sm_end(1u, run == 0u ? "page 1 left on the latched Enter" : "page 1 left on the latched Esc");
        CHECK(ch_cell(20, 2) != 0u, "no draw before the exit");
        CHECK_EQ_INT((int)DSD(DS_0010150C), 0x5A5A5A5A);
        CHECK_EQ_INT((int)ch_cell(0xC, 0x1B), 0);
        CHECK_EQ_INT((int)ch_cell(0xC, 0x26), 0);
        CHECK(ch_cell(0xC, 0x27) != 0u, "only twelve cells released");
    }

    /* P2A (clear_ok = 1, Esc held, so never new): Esc alone, then Enter
     * alone, clear nothing; Esc + Enter waits for Esc's release, zeroes
     * fields 0..0x27 and redraws without the hints; the latched Enter leaves
     * (5 frames). */
    static const sm_step_t p2a[] = {
        { 0u, 0x2000000u }, { 0u, 0x1000000u }, { 0u, 0x3000000u }, { 0u, 0x1000000u }, { 0x1C0Du, 0u } };
    actors_reset();
    sm_stats_seed_all();
    text_glyph_at(0x20, 'Z', 12, 0xF000u);
    text_glyph_at(SM_COLS(0x69u, 0x4000u), 'Z', 0x18, 0xF000u);
    sm_stats_seen = 0u;
    sm_begin(p2a, 5u);
    sm_held = 0x2000000u;
    sm_probe = sm_stats_probe;
    svc_stats_page2(1u);
    sm_end(5u, "page 2: the Esc + Enter clear, then Enter");
    CHECK_EQ_INT((int)sm_stats_seen, 0x71E);
    for (u32 f = 0; f < 0x28u; f++) CHECK_EQ_INT((int)config_field_get(f), 0);
    CHECK_EQ_INT((int)config_field_get(0x28u), 5);              /* 0x332A5: up to 0x27 */
    CHECK(ch_cell(0, SM_COLS(0xAAu, 0x5002u)) != 0u, "the title 0xAA");
    /* Rows 3..5 are drawn; their modes 0xF000/0x4000 pick the same palette
     * (record §K11.7), so these checks cannot see the alternation. */
    ch_expect(3, 4, '1', 0xF000u, "1 player games (0xA1) on row 3");
    ch_expect(3, 0x20, '0', 0xF000u, "field 8 cleared, pad 3");
    ch_expect(4, 4, '2', 0x4000u, "2 player games (0xA2) on row 4");
    ch_expect(4, 0x20, '0', 0x4000u, "field 9 cleared on row 4");
    ch_expect(5, 4, '1', 0xF000u, "1 pl continues (0xA3) on row 5");
    ch_expect(11, 4, 'F', 0xF000u, "Final continues (0xA9) on row 11");
    ch_expect(11, 0x20, '0', 0xF000u, "field 0x11");
    CHECK_EQ_INT((int)ch_cell(0x18, SM_COLS(0x69u, 0x4000u)), 0); /* no hints after the clear */
    ch_expect(0x1C, SM_COLS(0x82u, 0x1000u), 'f', 0x1000u, "page 2's for more stats");

    /* P2B (clear_ok = 0): Esc + Enter neither clears nor leaves; a new Esc
     * leaves (2 frames). */
    static const sm_step_t p2b[] = { { 0u, 0x3000000u }, { 0u, 0x2000000u } };
    actors_reset();
    sm_stats_seed();
    text_glyph_at(SM_COLS(0x69u, 0x4000u), 'Z', 0x18, 0xF000u);
    sm_begin(p2b, 2u);
    svc_stats_page2(0u);
    sm_end(2u, "page 2 without the clear: Esc + Enter stays, Esc leaves");
    CHECK_EQ_INT((int)config_field_get(8u), 2);
    CHECK_EQ_INT((int)config_field_get(0u), 7);
    ch_expect(3, 0x20, '2', 0xF000u, "field 8");
    ch_expect(4, 0x20, '6', 0x4000u, "field 9 = 65535");
    ch_expect(4, 0x24, '5', 0x4000u, "field 9 = 65535");
    CHECK_EQ_INT((int)ch_cell(0x18, SM_COLS(0x69u, 0x4000u)), 0);   /* no hints */

    /* P2C (clear_ok = 1): the latched Esc leaves first (1 frame). */
    static const sm_step_t p2c[] = { { 0x011Bu, 0u } };
    actors_reset();
    text_glyph_at(2, 'Z', 20, 0xF000u);
    text_glyph_at(0x1B, 'Z', 0xC, 0xF000u); text_glyph_at(0x27, 'Z', 0xC, 0xF000u);
    sm_begin(p2c, 1u);
    svc_stats_page2(1u);
    sm_end(1u, "page 2 left on the latched Esc");
    CHECK(ch_cell(20, 2) != 0u, "page 2: no draw before the exit");
    CHECK_EQ_INT((int)ch_cell(0xC, 0x1B), 0);
    CHECK(ch_cell(0xC, 0x27) != 0u, "page 2: only twelve cells released");
    CHECK_EQ_INT((int)config_field_get(0u), 7);

    /* 0x32F98's zero-total arm (0x32FDC `je`, 0x32FEB): fields 3..5 all 0 give a
     * percentage of 0, drawn at the cursor after "Percentage Play" (row
     * 13 + 2, column 26); no frame. P1A drew "33" there with the fields
     * seeded. */
    actors_reset();
    for (u32 f = 3u; f <= 5u; f++) {
        (void)config_field_set(f, 0u);
        CHECK_EQ_INT((int)config_field_get(f), 0);               /* was 0x10005, 0x10000, 0xFFFF */
    }
    svc_stats_play(4, 13);
    ch_expect(15, 4, 'P', 0xF000u, "Percentage Play (0x8A) at (col, row + 2)");
    ch_expect(15, 26, '0', 0xF000u, "a zero total gives 0");
    CHECK_EQ_INT((int)ch_cell(15, 27), 0);

    memcpy(gfx_dac, s_dac, sizeof s_dac);
    DSD(DS_0010150C) = s_150c; DSD(DS_001014E4) = s_e4; DSD(DS_001014E8) = s_e8;
}

/* Cycle 7 (record §K11.8): the histograms and the STATISTICS entry, 0x2E218
 * 0x2E11C 0x2E248 0x2E5E4 0x32BDC 0x33560 0x2CAC0. 16 frames. */
#define SM_HBUF 0x039000C0u              /* 0x32BDC's frame at the port scratch SVC_HIST_TMP */
#define SM_HST  0x00105D64u              /* 0x2E248's state block */
#define SM_H0   0x00105ECDu              /* histogram 0's 20 counters (0x2D45C) */
#define SM_H1   0x00105EE1u              /* histogram 1's 20 (0x2D464) */
#define SM_H2   0x00105EF5u              /* histogram 2's 7 (0x2D46C) */

/* `head` followed by `n` copies of `bar`, into `out`. */
static const char *sm_hline(char *out, const char *head, char bar, u32 n)
{
    const size_t h = strlen(head);
    memcpy(out, head, h);
    memset(out + h, bar, n);
    out[h + n] = 0;
    return out;
}

/* The line `s` on screen from (row, col): a ' ' is an empty cell, and so is
 * the cell after the last. */
static void sm_row_is(s32 row, s32 col, const char *s, u32 mode, const char *msg)
{
    const s32 n = (s32)strlen(s);
    for (s32 k = 0; k < n; k++) {
        if (s[k] == ' ') CHECK_EQ_INT((int)ch_cell(row, col + k), 0);
        else ch_expect(row, col + k, (u8)s[k], mode, msg);
    }
    CHECK_EQ_INT((int)ch_cell(row, col + n), 0);
}

/* The scratch buffer holds `want` and its NUL. */
static void sm_buf_is(const char *want, const char *msg)
{
    CHECK(memcmp(mem + SM_HBUF, want, strlen(want) + 1u) == 0, msg);
}

/* The counters of record §K11.8: histogram 0 {0: 3, 2: 30, 5: 12, 9: 20,
 * 19: 7}, 1 {0: 1, 3: 2}, 2 {1: 255, 2..6: 1..5}; the byte past them is
 * a sentinel. Histogram 1's odd sum makes the median's (sum + 1) >> 1
 * matter: 2 is reached in bucket 3, 1 would be in bucket 0. */
static void sm_hist_seed(void)
{
    memset(mem + SM_H0, 0, 47);
    DSB(SM_H0 + 0u) = 3u; DSB(SM_H0 + 2u) = 30u; DSB(SM_H0 + 5u) = 12u;
    DSB(SM_H0 + 9u) = 20u; DSB(SM_H0 + 19u) = 7u;
    DSB(SM_H1 + 0u) = 1u; DSB(SM_H1 + 3u) = 2u;
    DSB(SM_H2 + 1u) = 255u;
    for (u32 i = 2u; i < 7u; i++) DSB(SM_H2 + i) = (u8)(i - 1u);
    DSB(DS_00105EFC) = 0x5Bu;
}

static u32 sm_hist_seen;
/* Run A (frames from 0; probe f sees what the passes through step f - 1
 * left): probe 2 page 2 with its hints, probes 3..5 the three histograms,
 * probe 6 the clear screen inside the release wait. */
static void sm_hist_probe(u32 frame)
{
    char l[64];
    if (frame == 2u) {
        ch_expect(0x18, SM_COLS(0x69u, 0x4000u), 'H', 0x4000u, "page 2 with clear_ok = 1 from 0x2CAC0");
        sm_hist_seen |= 1u << frame;
    } else if (frame == 3u) {
        ch_expect(0, 8, 'R', 0x1000u, "the title 0x238 at column (0x28 - 23) >> 1");
        CHECK_EQ_INT((int)ch_cell(0, 7), 0);
        sm_row_is(2, 2, sm_hline(l, "  0-  9:  3   4% ", 3, 2u), 0x2000u, "histogram 0, bucket 0");
        sm_row_is(3, 2, " 10- 19:  0   0% ", 0x2000u, "histogram 0, bucket 1");
        /* 41 cells from column 2: 0x2F830 cuts the string at 0x2A - 2 - 1 */
        sm_row_is(4, 2, sm_hline(l, " 20- 29: 30  41% ", 3, 22u), 0x2000u, "histogram 0, bucket 2");
        sm_row_is(7, 2, sm_hline(l, " 50- 59: 12  16% ", 3, 10u), 0x3000u, "the median bucket 5 in 0x3000");
        sm_row_is(21, 2, sm_hline(l, "190& UP:  7   9% ", 3, 6u), 0x2000u, "the last bucket");
        ch_expect(23, 15, 'M', 0x3000u, "MEDIAN: (0x83) on row 20 + 3");
        sm_row_is(23, 22, " 50- 59", 0x1000u, "the median's range, cut at its ':'");
        ch_expect(23, 3, 'T', 0x1000u, "TOTAL: (0x84)");
        sm_row_is(23, 11, "72", 0x1000u, "the total");
        ch_expect(0x1C, SM_COLS(0x87u, 0x1000u), 'f', 0x1000u, "for next histogram (0x87)");
        CHECK_EQ_INT((int)ch_cell(0x18, SM_COLS(0x69u, 0x4000u)), 0);   /* hints only on the last */
        sm_hist_seen |= 1u << frame;
    } else if (frame == 4u) {
        ch_expect(0, 8, 'M', 0x1000u, "the title 0x239");
        sm_row_is(2, 2, sm_hline(l, "  0- 14: 1  33% ", 3, 6u), 0x2000u, "histogram 1, bucket 0");
        sm_row_is(5, 2, sm_hline(l, " 45- 59: 2  66% ", 3, 13u), 0x3000u, "histogram 1, the median bucket 3");
        sm_row_is(21, 2, "285& UP: 0   0% ", 0x2000u, "histogram 1, the last bucket");
        sm_row_is(23, 22, " 45- 59", 0x1000u, "histogram 1's median range");
        sm_row_is(23, 11, "3", 0x1000u, "histogram 1's total");
        sm_hist_seen |= 1u << frame;
    } else if (frame == 5u) {
        ch_expect(0, 8, 'S', 0x1000u, "the title 0x23A");
        sm_row_is(2, 2, "   SAURON   0   0% ", 0x2000u, "histogram 2, column 0");
        sm_row_is(3, 2, sm_hline(l, " BLIZZARD 255  94% ", 3, 20u), 0x3000u, "histogram 2, the median column 1");
        sm_row_is(8, 2, "    CHAOS   5   1% ", 0x2000u, "histogram 2, column 6");
        CHECK_EQ_INT((int)ch_cell(10, 15), 0);                   /* no ':' in the line: no MEDIAN */
        ch_expect(10, 3, 'T', 0x1000u, "TOTAL: on row 7 + 3");
        sm_row_is(10, 11, "270", 0x1000u, "histogram 2's total");
        ch_expect(0x1C, SM_COLS(0x20Au, 0x1000u), 'T', 0x1000u, "TO EXIT MENU (0x20A) on the last");
        ch_expect(0x18, SM_COLS(0x69u, 0x4000u), 'H', 0x4000u, "hint 0x69 with a = 1");
        ch_expect(0x19, SM_COLS(0x6Au, 0x4000u), 'A', 0x4000u, "hint 0x6A");
        ch_expect(0x1A, SM_COLS(0x86u, 0x4000u), 't', 0x4000u, "hint 0x86");
        CHECK_EQ_INT((int)DSB(DS_00105DD8), 0);                  /* not cleared yet */
        sm_hist_seen |= 1u << frame;
    } else if (frame == 6u) {
        ch_expect(0xA, SM_COLS(0x88u, 0x4000u), 'C', 0x4000u, "CLEARING ALL HISTOGRAMS (0x88)");
        ch_expect(0x1C, SM_COLS(0x20Au, 0x1000u), 'T', 0x1000u, "the clear screen's TO EXIT MENU");
        CHECK_EQ_INT((int)ch_cell(3, 3), 0);                     /* the screen was reset */
        u32 nz = 0u;
        for (u32 i = 0u; i < 47u; i++) nz |= DSB(SM_H0 + i);
        CHECK_EQ_INT((int)nz, 0);                                /* all three cleared before the wait */
        CHECK_EQ_INT((int)DSB(DS_00105EFC), 0x5B);
        CHECK_EQ_INT((int)DSB(DS_00105DD8), 0x38);               /* bits 3, 4, 5 */
        sm_hist_seen |= 1u << frame;
    }
}

static void sm_check_hist(void)
{
    const u32 s_150c = DSD(DS_0010150C), s_e4 = DSD(DS_001014E4), s_e8 = DSD(DS_001014E8);
    u8 s_dac[256][3];
    memcpy(s_dac, gfx_dac, sizeof s_dac);
    u8 s_cnt[48], s_st[0x24], s_desc[0x70];
    memcpy(s_cnt, mem + SM_H0, sizeof s_cnt);                    /* 0x105ECD..0x105EFC */
    memcpy(s_st, mem + SM_HST, sizeof s_st);
    memcpy(s_desc, mem + 0x2D414u, sizeof s_desc);               /* 0x2D414..0x2D483 */
    ch_text_setup();
    char l[64];

    /* (a) 0x2E218: `jl`/`jge` signed; 10 digits at most. */
    static const struct { s32 v; u32 n; } dg[] = {
        { 0, 1 }, { 9, 1 }, { 10, 2 }, { 99, 2 }, { 100, 3 }, { 999999999, 9 },
        { 1000000000, 10 }, { 0x7FFFFFFF, 10 }, { -5, 1 },
    };
    for (u32 i = 0; i < sizeof dg / sizeof dg[0]; i++)
        CHECK_EQ_INT((int)audit_digits(dg[i].v), (int)dg[i].n);

    /* (b) 0x2E11C. */
    memset(mem + SM_H0, 0xAA, 47);
    DSB(DS_00105EFC) = 0x5Bu;                                    /* one past histogram 2 */
    DSB(DS_00105DD8) = 0x00u;
    CHECK_EQ_INT((int)audit_hist_clear(0u), 0);
    for (u32 i = 0; i < 20u; i++) CHECK_EQ_INT((int)DSB(SM_H0 + i), 0);   /* 20 bytes */
    CHECK_EQ_INT((int)DSB(SM_H1), 0xAA);                         /* histogram 1 untouched */
    CHECK_EQ_INT((int)DSB(DS_00105DD8), 0x08);                   /* bit 0 + 3 */
    CHECK_EQ_INT((int)audit_hist_clear(2u), 0);
    for (u32 i = 0; i < 7u; i++) CHECK_EQ_INT((int)DSB(SM_H2 + i), 0);    /* 7 bytes */
    CHECK_EQ_INT((int)DSB(DS_00105EFC), 0x5B);                   /* past the end */
    CHECK_EQ_INT((int)DSB(SM_H1 + 19u), 0xAA);
    CHECK_EQ_INT((int)DSB(DS_00105DD8), 0x28);                   /* + bit 5 */
    memset(mem + SM_H0, 0xAA, 47);
    CHECK_EQ_INT((int)audit_hist_clear(3u), -1);                 /* 0x2E123 `jb` */
    CHECK_EQ_INT((int)DSB(SM_H0), 0xAA);
    CHECK_EQ_INT((int)DSB(DS_00105DD8), 0x28);                   /* untouched */

    /* (c) 0x2E248 and 0x2E5E4 over the seeded counters (the lines are the
     * record's; no frame). */
    sm_hist_seed();
    memset(mem + SM_HST, 0xA5, sizeof s_st);
    memset(mem + SM_HBUF, 0xEE, 0x40);
    CHECK_EQ_INT((int)audit_hist_format(0u, SM_HBUF, 0x2Au, SM_HBUF + 0x30u, SM_HBUF + 0x34u, 0x32640u), 23);
    sm_buf_is("Round Time (in seconds)", "the title copied up to the NUL");
    CHECK_EQ_INT((int)DSD(SM_HBUF + 0x30u), 30);                 /* the largest */
    CHECK_EQ_INT((int)DSD(SM_HBUF + 0x34u), 5);                  /* the median: 36 reached in bucket 5 */
    static const u32 st0[9] = { 1u, 30u, 72u, 9u, 3u, 3u, 0u, 0x32640u, 1u };
    for (u32 k = 0; k < 9u; k++) CHECK_EQ_INT((int)DSD(SM_HST + 4u * k), (int)st0[k]);
    CHECK_EQ_INT((int)audit_hist_line(0u, SM_HBUF, 0x2A), 3);
    sm_buf_is(sm_hline(l, "  0-  9:  3   4% ", 3, 2u), "bucket 0");
    CHECK_EQ_INT((int)audit_hist_line(1u, SM_HBUF, 0x2A), 0);
    sm_buf_is(" 10- 19:  0   0% ", "bucket 1: a zero count, no bar");
    CHECK_EQ_INT((int)audit_hist_line(2u, SM_HBUF, 0x2A), 30);
    sm_buf_is(sm_hline(l, " 20- 29: 30  41% ", 3, 24u), "bucket 2: the full bar of 42 - 9 - 2 - 7");
    CHECK_EQ_INT((int)audit_hist_line(5u, SM_HBUF, 0x2A), 12);
    sm_buf_is(sm_hline(l, " 50- 59: 12  16% ", 3, 10u), "bucket 5");
    CHECK_EQ_INT((int)audit_hist_line(19u, SM_HBUF, 0x2A), 7);
    sm_buf_is(sm_hline(l, "190& UP:  7   9% ", 3, 6u), "the last bucket: & UP");
    memset(mem + SM_HBUF, 0xEE, 0x40);
    CHECK_EQ_INT((int)audit_hist_line(20u, SM_HBUF, 0x2A), -1);  /* past the 0x14 bytes */
    CHECK_EQ_INT((int)audit_hist_line(2u, SM_HBUF, 18), -1);     /* a bar of 0 */
    CHECK_EQ_INT((int)audit_hist_line(2u, 0u, 0x2A), -1);
    CHECK_EQ_INT((int)DSB(SM_HBUF), 0xEE);                       /* none of the three wrote */
    CHECK_EQ_INT((int)audit_hist_line(2u, SM_HBUF, 19), 30);
    sm_buf_is(" 20- 29: 30  41% \x03", "a bar of 1");
    /* The default bar string "#:" (label 0): L = 2, a half cell is ':'. */
    CHECK_EQ_INT((int)audit_hist_format(0u, SM_HBUF, 0x2Au, 0u, 0u, 0u), 23);
    CHECK_EQ_INT((int)DSD(SM_HST + 0x1Cu), 0x80B20);
    CHECK_EQ_INT((int)DSD(SM_HST + 0x20u), 2);
    CHECK_EQ_INT((int)audit_hist_line(0u, SM_HBUF, 0x2A), 3);
    sm_buf_is("  0-  9:  3   4% ##:", "two cells and a half");
    CHECK_EQ_INT((int)audit_hist_line(19u, SM_HBUF, 0x2A), 7);
    sm_buf_is("190& UP:  7   9% #####:", "five cells and a half");
    /* Histogram 1: the unit is at least 4 * L (the largest count is 2, so
     * a count of 1 is 25 / 4 cells, not 25 / 2). */
    CHECK_EQ_INT((int)audit_hist_format(1u, SM_HBUF, 0x2Au, SM_HBUF + 0x30u, SM_HBUF + 0x34u, 0x32640u), 23);
    CHECK_EQ_INT((int)DSD(SM_HBUF + 0x34u), 3);                  /* (3 + 1) >> 1 = 2, reached in bucket 3 */
    CHECK_EQ_INT((int)audit_hist_line(0u, SM_HBUF, 0x2A), 1);
    sm_buf_is(sm_hline(l, "  0- 14: 1  33% ", 3, 6u), "histogram 1, bucket 0");
    /* A median bucket that holds exactly half: {0: 2, 3: 1} stops at bucket
     * 0 (0x2E596 `jle`: 2 <= 2). */
    DSB(SM_H1 + 0u) = 2u; DSB(SM_H1 + 3u) = 1u;
    CHECK_EQ_INT((int)audit_hist_format(1u, SM_HBUF, 0x2Au, 0u, SM_HBUF + 0x34u, 0x32640u), 23);
    CHECK_EQ_INT((int)DSD(SM_HBUF + 0x34u), 0);
    DSB(SM_H1 + 0u) = 1u; DSB(SM_H1 + 3u) = 2u;
    /* Histogram 2: the tabbed template; +0x10/+0x14 are not written. */
    memset(mem + SM_HST, 0xA5, sizeof s_st);
    CHECK_EQ_INT((int)audit_hist_format(2u, SM_HBUF, 0x2Au, SM_HBUF + 0x30u, SM_HBUF + 0x34u, 0x32640u), 21);
    sm_buf_is("Selects Per Character:", "the title up to the tab");
    CHECK_EQ_INT((int)DSD(SM_HBUF + 0x34u), 1);
    static const u32 st2[9] = { 3u, 255u, 270u, 10u, 0xA5A5A5A5u, 0xA5A5A5A5u, 0x80A6Au, 0x32640u, 1u };
    for (u32 k = 0; k < 9u; k++) CHECK_EQ_INT((int)DSD(SM_HST + 4u * k), (int)st2[k]);
    CHECK_EQ_INT((int)audit_hist_line(0u, SM_HBUF, 0x2A), 0);
    sm_buf_is("   SAURON   0   0% ", "column 0, right-aligned in 10");
    CHECK_EQ_INT((int)audit_hist_line(1u, SM_HBUF, 0x2A), 255);
    sm_buf_is(sm_hline(l, " BLIZZARD 255  94% ", 3, 22u), "column 1, the widest");
    CHECK_EQ_INT((int)audit_hist_line(6u, SM_HBUF, 0x2A), 5);
    sm_buf_is("    CHAOS   5   1% ", "column 6, the last");
    CHECK_EQ_INT((int)audit_hist_line(7u, SM_HBUF, 0x2A), -1);   /* past the 7 bytes */

    /* (d) the refusals. */
    DSD(SM_HST) = 0x77u;
    CHECK_EQ_INT((int)audit_hist_format(3u, SM_HBUF, 0x2Au, 0u, 0u, 0u), -1);
    CHECK_EQ_INT((int)DSD(SM_HST), 0);                           /* 0x2E26A before the tests */
    memset(mem + SM_HBUF, 0xEE, 0x40);
    CHECK_EQ_INT((int)audit_hist_line(0u, SM_HBUF, 0x2A), -1);   /* no histogram prepared */
    CHECK_EQ_INT((int)DSB(SM_HBUF), 0xEE);
    CHECK_EQ_INT((int)audit_hist_format(0u, 0u, 0x2Au, 0u, 0u, 0u), -1);
    CHECK_EQ_INT((int)audit_hist_format(0u, SM_HBUF, 0u, 0u, 0u, 0u), -1);
    CHECK_EQ_INT((int)DSB(SM_HBUF), 0xEE);
    CHECK_EQ_INT((int)audit_hist_format(0u, SM_HBUF, 23u, 0u, 0u, 0u), -1);   /* the title fills it */
    CHECK_EQ_INT((int)DSB(SM_HBUF + 22u), ')');
    CHECK_EQ_INT((int)DSB(SM_HBUF + 23u), 0xEE);                 /* no NUL */
    CHECK_EQ_INT((int)audit_hist_format(0u, SM_HBUF, 24u, 0u, 0u, 0u), 23);
    CHECK_EQ_INT((int)DSB(SM_HBUF + 23u), 0);
    DSB(0x2D442u) = 6u;                                          /* histogram 2 with 6 columns: a 7th is left */
    CHECK_EQ_INT((int)audit_hist_format(2u, SM_HBUF, 0x2Au, 0u, 0u, 0u), -1);
    DSB(0x2D442u) = 8u;                                          /* 8: the template ends early */
    DSW(0x2D46Eu) = 8u;                                          /* (8 counters, or the sum refuses it) */
    CHECK_EQ_INT((int)audit_hist_format(2u, SM_HBUF, 0x2Au, 0u, 0u, 0u), -1);
    DSB(0x2D442u) = 7u;
    DSW(0x2D46Eu) = 6u;                                          /* 6 counters for 7 columns */
    CHECK_EQ_INT((int)audit_hist_format(2u, SM_HBUF, 0x2Au, 0u, 0u, 0u), -1);
    DSW(0x2D46Eu) = 7u;
    DSB(0x2D422u) = 1u;                                          /* histogram 0 with one bucket */
    CHECK_EQ_INT((int)audit_hist_format(0u, SM_HBUF, 0x2Au, 0u, 0u, 0u), -1);
    DSB(0x2D422u) = 0x14u;
    /* Unit-wide buckets from 1: every range is one value (no '-'), the
     * high digits are 0 and the last is '+'. */
    DSD(0x2D418u) = 1u; DSD(0x2D41Cu) = 1u;
    CHECK_EQ_INT((int)audit_hist_format(0u, SM_HBUF, 0x2Au, 0u, 0u, 0x32640u), 23);
    CHECK_EQ_INT((int)DSD(SM_HST + 0xCu), 5);
    CHECK_EQ_INT((int)DSD(SM_HST + 0x10u), 2);
    CHECK_EQ_INT((int)DSD(SM_HST + 0x14u), 0);
    CHECK_EQ_INT((int)audit_hist_line(1u, SM_HBUF, 0x2A), 0);
    sm_buf_is(" 1 :  0   0% ", "a one-value range");
    CHECK_EQ_INT((int)audit_hist_line(19u, SM_HBUF, 0x2A), 7);
    sm_buf_is(sm_hline(l, "19+:  7   9% ", 3, 7u), "no room for & UP: '+'");
    /* From 50: the first range is 0-49, the width grows to lo + 4. */
    DSD(0x2D418u) = 50u;
    CHECK_EQ_INT((int)audit_hist_format(0u, SM_HBUF, 0x2Au, 0u, 0u, 0x32640u), 23);
    CHECK_EQ_INT((int)DSD(SM_HST + 0xCu), 8);
    CHECK_EQ_INT((int)audit_hist_line(0u, SM_HBUF, 0x2A), 3);
    sm_buf_is(sm_hline(l, " 0-49 :  3   4% ", 3, 3u), "0-49");
    CHECK_EQ_INT((int)audit_hist_line(1u, SM_HBUF, 0x2A), 0);
    sm_buf_is("50    :  0   0% ", "lo == hi: no range");
    CHECK_EQ_INT((int)audit_hist_line(19u, SM_HBUF, 0x2A), 7);
    sm_buf_is(sm_hline(l, "68& UP:  7   9% ", 3, 6u), "& UP at (e + 1) >> 1 = 0");
    /* 100-wide buckets: four digits each side, and e = 11 - 6 - 4 = 1 puts
     * "& UP" one cell right. */
    DSD(0x2D418u) = 100u; DSD(0x2D41Cu) = 100u;
    CHECK_EQ_INT((int)audit_hist_format(0u, SM_HBUF, 0x2Au, 0u, 0u, 0x32640u), 23);
    CHECK_EQ_INT((int)DSD(SM_HST + 0xCu), 11);
    CHECK_EQ_INT((int)audit_hist_line(2u, SM_HBUF, 0x2A), 30);
    sm_buf_is(sm_hline(l, " 200- 299: 30  41% ", 3, 22u), "200-299");
    CHECK_EQ_INT((int)audit_hist_line(19u, SM_HBUF, 0x2A), 7);
    sm_buf_is(sm_hline(l, "1900 & UP:  7   9% ", 3, 5u), "& UP at (e + 1) >> 1 = 1");
    /* Eleven 10-wide buckets: the last range starts at 100, so the low
     * bound has 3 digits and the high 2; a bucket past the count (11 < the
     * 20 bytes) is still formatted, its 119 cut to its last 2 digits. */
    DSD(0x2D418u) = 10u; DSD(0x2D41Cu) = 10u; DSB(0x2D422u) = 11u;
    CHECK_EQ_INT((int)audit_hist_format(0u, SM_HBUF, 0x2Au, 0u, 0u, 0x32640u), 23);
    CHECK_EQ_INT((int)DSD(SM_HST + 8u), 65);                     /* bucket 19 is not summed */
    CHECK_EQ_INT((int)DSD(SM_HST + 0x10u), 3);
    CHECK_EQ_INT((int)DSD(SM_HST + 0x14u), 2);
    CHECK_EQ_INT((int)audit_hist_line(1u, SM_HBUF, 0x2A), 0);
    sm_buf_is(" 10-19 :  0   0% ", "a 3-digit low and a 2-digit high");
    CHECK_EQ_INT((int)audit_hist_line(10u, SM_HBUF, 0x2A), 0);
    sm_buf_is("100& UP:  0   0% ", "the last of eleven");
    CHECK_EQ_INT((int)audit_hist_line(11u, SM_HBUF, 0x2A), 0);
    sm_buf_is("110-19 :  0   0% ", "past the count, within the size");
    memcpy(mem + 0x2D414u, s_desc, sizeof s_desc);       /* the descriptors are back */

    DSD(DS_001014E4) = CH_BUF_A; DSD(DS_001014E8) = CH_BUF_B;
    CHECK(fn_resolve(0x2CAC0u) == (void (*)(void))svc_statistics_entry, "0x2CAC0 registered");

    /* Run A, 0x2CAC0 (7 frames): page 1 leaves on the latched Enter; page 2
     * draws (clear_ok = 1) and leaves on the latched Esc; histogram 0 goes
     * on at a new Esc with Enter held (h < 2); histogram 1 on the latched
     * Enter; on histogram 2 a new Esc with Enter held clears all three, and
     * the release wait leaves at the next new Esc. */
    static const sm_step_t ra[] = {
        { 0x1C0Du, 0u }, { 0u, 0u }, { 0x011Bu, 0u }, { 0u, 0x3000000u },
        { 0x1C0Du, 0u }, { 0u, 0x3000000u }, { 0u, 0x2000000u } };
    actors_reset();
    sm_hist_seed();
    DSB(DS_00105DD8) = 0u;
    sm_hist_seen = 0u;
    sm_begin(ra, 7u);
    sm_probe = sm_hist_probe;
    const u32 ret_a = svc_statistics_entry(0xBCC3Cu);
    sm_end(7u, "STATISTICS: page 1, page 2, three histograms and the clear");
    CHECK_EQ_INT((int)sm_hist_seen, 0x7C);
    CHECK_EQ_INT((int)ret_a, 0x2000000);                         /* the wait's Esc poll */
    ch_expect(0xA, SM_COLS(0x88u, 0x4000u), 'C', 0x4000u, "the clear screen stays");

    /* Run B, a = 0 (3 frames): the latched Esc, then a new Esc, go on; on
     * histogram 2 a new Esc with Enter held leaves without the clear. */
    static const sm_step_t rb[] = { { 0x011Bu, 0u }, { 0u, 0x2000000u }, { 0u, 0x3000000u } };
    actors_reset();
    sm_hist_seed();
    DSB(DS_00105DD8) = 0u;
    sm_begin(rb, 3u);
    const u32 ret_b = svc_stats_hist(0u);
    sm_end(3u, "histograms without the clear");
    CHECK_EQ_INT((int)ret_b, 2);                                 /* EAX = h at 0x32E76 */
    CHECK_EQ_INT((int)DSB(SM_H0 + 2u), 30);
    CHECK_EQ_INT((int)DSB(SM_H2 + 1u), 255);
    CHECK_EQ_INT((int)DSB(DS_00105DD8), 0);
    ch_expect(0, 8, 'S', 0x1000u, "histogram 2 left on screen");
    ch_expect(0x1C, SM_COLS(0x20Au, 0x1000u), 'T', 0x1000u, "TO EXIT MENU with a = 0");
    CHECK_EQ_INT((int)ch_cell(0x18, SM_COLS(0x69u, 0x4000u)), 0);   /* no hints with a = 0 */

    /* Run C, a = 1 (3 frames): the latched Enter and Esc go on; on histogram
     * 2 a new Esc without Enter leaves. */
    static const sm_step_t rc[] = { { 0x1C0Du, 0u }, { 0x011Bu, 0u }, { 0u, 0x2000000u } };
    actors_reset();
    sm_hist_seed();
    DSB(DS_00105DD8) = 0u;
    sm_begin(rc, 3u);
    const u32 ret_c = svc_stats_hist(1u);
    sm_end(3u, "histograms: a new Esc without Enter leaves");
    CHECK_EQ_INT((int)ret_c, 0x2000000);                         /* the 0x2EDE0(0, 1) word */
    CHECK_EQ_INT((int)DSB(SM_H0 + 2u), 30);
    CHECK_EQ_INT((int)DSB(DS_00105DD8), 0);
    ch_expect(0x18, SM_COLS(0x69u, 0x4000u), 'H', 0x4000u, "hint 0x69 with a = 1");

    /* Run D, a = 1 (3 frames): a new Esc and the latched Enter go on; the
     * latched Esc on histogram 2 ends the three. */
    static const sm_step_t rd[] = { { 0u, 0x2000000u }, { 0x1C0Du, 0u }, { 0x011Bu, 0u } };
    actors_reset();
    sm_hist_seed();
    DSB(DS_00105DD8) = 0u;
    sm_begin(rd, 3u);
    const u32 ret_d = svc_stats_hist(1u);
    sm_end(3u, "histograms: the latched Esc after the last leaves");
    CHECK_EQ_INT((int)ret_d, 0x1B);
    CHECK_EQ_INT((int)DSB(SM_H0 + 2u), 30);
    CHECK_EQ_INT((int)DSB(DS_00105DD8), 0);

    /* After a clear every count is 0: the percentage's zero-sum arm. */
    memset(mem + SM_H0, 0, 47);
    CHECK_EQ_INT((int)audit_hist_format(1u, SM_HBUF, 0x2Au, SM_HBUF + 0x30u, SM_HBUF + 0x34u, 0x32640u), 23);
    CHECK_EQ_INT((int)DSD(SM_HST + 8u), 0);
    CHECK_EQ_INT((int)audit_hist_line(0u, SM_HBUF, 0x2A), 0);
    sm_buf_is("  0- 14: 0   0% ", "a zero sum gives 0%");

    memcpy(mem + SM_H0, s_cnt, sizeof s_cnt);
    memcpy(mem + SM_HST, s_st, sizeof s_st);
    memcpy(gfx_dac, s_dac, sizeof s_dac);
    DSD(DS_0010150C) = s_150c; DSD(DS_001014E4) = s_e4; DSD(DS_001014E8) = s_e8;
}

/* Runs after test_cfg_helpers, whose one actors_init() it relies on. The
 * menu screens advance the tick model and the key state; those, the menu
 * state DS_00107414..DS_00107453 and the credits dword are put back. */
int test_svcmenu(void)
{
    int before = g_failures;
    u8 s_menu[0x40];
    memcpy(s_menu, mem + DS_00107414, sizeof s_menu);
    const u32 s_tick = DSD(CH_TICK), s_isr = DSD(DS_00101508);
    const u16 s_word = DSW(DS_000EF6DE);
    const u8 s_full = DSB(DS_001014FC);
    const u32 s_lat = DSD(CH_KEY_LATCH), s_time = DSD(CH_KEY_TIME), s_kw = DSD(CH_KEY_WORD);
    const u32 s_credits = DSD(DS_00105C00);
    u8 s_cfg[0x100];                     /* the config fields and their dirty byte DS_00105DD8 */
    memcpy(s_cfg, mem + DS_00105D88, sizeof s_cfg);
    const u32 s_rpt = DSD(DS_000E1C3C);  /* the repeat words 0x50146 writes */
    const u16 s_rpt40 = DSW(DS_000E1C40), s_rpt42 = DSW(DS_000E1C42), s_rpt44 = DSW(DS_000E1C44);

    sm_check_seam();
    sm_check_shell();
    sm_check_options();
    sm_check_volume();
    sm_check_controls();
    sm_check_keyboard();
    sm_check_stats();
    sm_check_hist();

    DSD(DS_000E1C3C) = s_rpt;
    DSW(DS_000E1C40) = s_rpt40; DSW(DS_000E1C42) = s_rpt42; DSW(DS_000E1C44) = s_rpt44;
    memcpy(mem + DS_00107414, s_menu, sizeof s_menu);
    DSD(CH_TICK) = s_tick; DSD(DS_00101508) = s_isr;
    DSW(DS_000EF6DE) = s_word;
    DSB(DS_001014FC) = s_full;
    DSD(CH_KEY_LATCH) = s_lat; DSD(CH_KEY_TIME) = s_time; DSD(CH_KEY_WORD) = s_kw;
    DSD(DS_00105C00) = s_credits;
    memcpy(mem + DS_00105D88, s_cfg, sizeof s_cfg);
    return g_failures - before;
}

int test_cfg_helpers(void)
{
    int before = g_failures;
    /* Once only: every actors_init() registers its handlers again and the
     * registration table has a fixed limit. */
    CHECK(actors_init() == 1, "actors_init validates the pools");
    ch_check_key_flags();
    ch_text_setup();
    ch_check_key_name();
    ch_check_bar();
    ch_check_option_row();
    ch_check_code_row();
    ch_check_screen_wait();
    ch_check_screen_wait_zero();
    ch_check_quit_prompt();
    ch_check_keycfg();
    return g_failures - before;
}

/* ---- record §51-A: 0x204F4 and mode 0x1E's states 0xF/0x10 ---------------- */

/* Addresses symbols.h has no name for (nameentry.c's own NE_* values). */
#define RA_COL    0x001044D0u   /* word: cursor column */
#define RA_ROW    0x001044D2u   /* word: cursor row */
#define RA_CNT    0x0010431Cu   /* byte: letters typed */
#define RA_LIM    0x0010431Du   /* byte: letters allowed */
#define RA_PAD0   0x001088E7u   /* side 0's pad byte */
#define RA_PAD1   0x001088E5u   /* side 1's pad byte */
#define RA_REP_H  0x001044E0u
#define RA_REP_V  0x001044DCu
/* A private actor pool: nameentry_reset and nameentry_step spawn into it, so
 * the image's own pool is never touched (test_fight.c's M1F_POOL recipe). */
#define RA_POOL   0x03F70000u
#define RA_PSET   0x03FA0000u
#define RA_DATA_LEN (0x0010B0D0u - 0x00080000u)
#define RA_POOL_LEN (ACTOR_POOL_RECORDS * ACTOR_REC_SIZE)
#define RA_PSET_LEN (ACTOR_POOL_RECORDS * PSET_SIZE)

static u8 *ra_buf;
static u8 ra_low[0x40];

static int ra_save(void)
{
    ra_buf = (u8 *)malloc(RA_DATA_LEN + RA_POOL_LEN + RA_PSET_LEN);
    if (ra_buf == NULL) return 0;
    tf_snap(ra_buf, 0x00080000u, RA_DATA_LEN);
    tf_snap(ra_buf + RA_DATA_LEN, RA_POOL, RA_POOL_LEN);
    tf_snap(ra_buf + RA_DATA_LEN + RA_POOL_LEN, RA_PSET, RA_PSET_LEN);
    tf_snap(ra_low, 0u, sizeof ra_low);
    return 1;
}

static void ra_restore(void)
{
    if (ra_buf == NULL) return;
    tf_put(ra_buf, 0x00080000u, RA_DATA_LEN);
    tf_put(ra_buf + RA_DATA_LEN, RA_POOL, RA_POOL_LEN);
    tf_put(ra_buf + RA_DATA_LEN + RA_POOL_LEN, RA_PSET, RA_PSET_LEN);
    tf_put(ra_low, 0u, sizeof ra_low);
    free(ra_buf);
    ra_buf = NULL;
}

/* The fresh-CMOS table 0 (hs_orig: 500000, 400000, 350000, 300000, 250000,
 * 200000, 90210, 50000, 20000, 100), a private actor pool, the game strings,
 * and 0x204F4's outputs at sentinels that differ from every post-condition.
 * hiscore_init leaves DS_00104367's 0x24 bytes blank (0x20). */
static void ra_env(void)
{
    DSD(DS_001014F4) = RA_POOL;
    DSD(DS_001014EC) = RA_PSET;
    actors_reset();
    game_string_table_load("data/game/C");
    mem_fill(HS_T0, 0, 153u);
    DSD(DS_00104528) = 0x142095u;
    hiscore_init();
    DSB(DS_00104367 + 0x24u) = 0x5Au;
    DSB(DS_0010431E) = 0x77u;
    mem_fill(DS_0010431F, 0x77, 0x24u);
    mem_fill(DS_00104343, 0x77, 0x24u);
    mem_fill(DS_00104394, 0x77, 0x25u);
    DSB(RA_CNT) = 0x77u;
    DSB(RA_LIM) = 0x77u;
    DSD(DS_00104390) = 0xDEADBEEFu;
    DSD(DS_001044D4) = 0x77777777u;
    DSD(DS_0010438C) = 0x77777777u;
    DSD(DS_001044CC) = 0x77777777u;
    DSD(DS_001044C4) = 0x77777777u;
    DSW(DS_001044D6) = 0x7777u;
    DSD(DS_00105EFC) = 0x77777777u;
    DSW(RA_COL) = 0x7777u;
    DSW(RA_ROW) = 0x7777u;
    DSB(DS_001044D8) = 0x77u;
    DSB(RA_PAD0) = 0u;
    DSB(RA_PAD1) = 0u;
    DSD(RA_REP_H) = 0u;
    DSD(RA_REP_V) = 0u;
    DSD(DS_00104B00) = 0xBEEF0000u | 0x1Eu;
}

/* 0x204F4 called directly: the candidate fill, the buffers, the geometry,
 * and the 0x2DBC4 read order (its last decode is left in DS_00105EFC). */
static void ra_check_arm(void)
{
    u32 i;

    /* rank 3, a blank candidate: factory record 3's name "MSG" (0xA7BC0 +
     * 3 * 0x2C) is copied with its NUL, the rest of the buffer untouched;
     * 3 letters at column 0x12, row 0x16. Records 3..8 then 0..2 are read,
     * so the last decode is record 2 (350000). */
    ra_env();
    nameentry_arm(3u, 0x12345u);
    CHECK(memcmp(mem + DS_00104367, "MSG", 4u) == 0, "0x204F4 fills the blank candidate from 0xA7BC0");
    CHECK_EQ_INT((int)DSB(DS_00104367 + 4u), 0x20);
    CHECK_EQ_INT((int)DSB(DS_00104367 + 0x24u), 0x5A);
    CHECK_EQ_INT((int)DSB(DS_0010431E), 1);
    CHECK_EQ_INT((int)DSB(DS_0010431F), 0x20);
    for (i = 1u; i < 0x24u; i++) CHECK_EQ_INT((int)DSB(DS_0010431F + i), 0);
    for (i = 0u; i < 0x24u; i++) CHECK_EQ_INT((int)DSB(DS_00104343 + i), 0);
    for (i = 0u; i < 0x24u; i++) CHECK_EQ_INT((int)DSB(DS_00104394 + i), 0x20);
    CHECK_EQ_INT((int)DSB(DS_00104394 + 0x24u), 0x77);
    CHECK_EQ_INT((int)DSB(RA_CNT), 0);
    CHECK_EQ_INT((int)DSD(DS_00104390), 0x12345);
    CHECK_EQ_INT((int)DSD(DS_001044D4), 0x77777700);   /* a byte store */
    CHECK_EQ_INT((int)DSD(DS_0010438C), 0x777702EE);   /* a word store */
    CHECK_EQ_INT((int)DSD(DS_001044CC), 0x12);
    CHECK_EQ_INT((int)DSD(DS_001044C4), 0x16);
    CHECK_EQ_INT((int)DSB(RA_LIM), 3);
    CHECK_EQ_INT((int)DSD(DS_00105EFC), 350000);

    /* rank 0, a candidate that is not blank: kept as is, DS_0010431E not
     * raised; 18 letters at column 2. Records 0..8 are read (last 20000). */
    ra_env();
    DSB(DS_00104367 + 7u) = 'Q';
    nameentry_arm(0u, 777u);
    CHECK_EQ_INT((int)DSB(DS_00104367), 0x20);
    CHECK_EQ_INT((int)DSB(DS_00104367 + 7u), 'Q');
    CHECK_EQ_INT((int)DSB(DS_0010431E), 0x77);
    CHECK_EQ_INT((int)DSD(DS_00104390), 777);
    CHECK_EQ_INT((int)DSD(DS_001044CC), 2);
    CHECK_EQ_INT((int)DSD(DS_001044C4), 0x16);
    CHECK_EQ_INT((int)DSB(RA_LIM), 0x12);
    CHECK_EQ_INT((int)DSD(DS_00105EFC), 20000);

    /* rank 9, blank: "DUD"; only the second loop runs (records 0..8). */
    ra_env();
    nameentry_arm(9u, 150u);
    CHECK(memcmp(mem + DS_00104367, "DUD", 4u) == 0, "rank 9's factory name");
    CHECK_EQ_INT((int)DSD(DS_00105EFC), 20000);

    /* 0x1EC38 arms the screen only for a qualifying score. */
    ra_env();
    CHECK_EQ_INT((int)hiscore_rank_single(999999u), 1);
    CHECK_EQ_INT((int)DSD(DS_00104390), 999999);
    CHECK_EQ_INT((int)DSB(RA_LIM), 0x12);
    CHECK_EQ_INT((int)DSW(DS_001044D6), 0);
    ra_env();
    CHECK_EQ_INT((int)hiscore_rank_single(0u), 0);
    CHECK_EQ_INT((int)DSD(DS_00104390), (int)0xDEADBEEFu);
    CHECK_EQ_INT((int)DSB(RA_LIM), 0x77);
    CHECK_EQ_INT((int)DSW(DS_001044D6), 0x7777);
}

/* States 0xF/0x10 (0x1EFA2/0x1F094): the other side's state (0xE/0xD), the
 * screen reset (cursor to column 0xB / row 6), 0x1EC38 on the other side's
 * score, then one priming 0x1F458 for that side: the timer 0x2EE the arm set
 * is counted down once, and that side's "right" moves the cursor to 0xE. */
static void ra_check_rearm(void)
{
    /* 0xF: side 1's 450000 ranks 1 (3 letters); side 1 presses right. */
    ra_env();
    DSB(DS_00104B25) = 0x0Fu;
    DSD(DS_001077EC) = 999999u;
    DSD(DS_00107880) = 450000u;
    DSB(RA_PAD1) = 0x10u;
    game_mode_1e_step();
    CHECK_EQ_INT((int)DSB(DS_00104B25), 0x0E);
    CHECK_EQ_INT((int)DSW(DS_001044D6), 1);
    CHECK_EQ_INT((int)DSD(DS_00104390), 450000);
    CHECK_EQ_INT((int)DSB(RA_LIM), 3);
    CHECK_EQ_INT((int)DSW(DS_0010438C), 0x2ED);
    CHECK_EQ_INT((int)DSW(RA_COL), 0xE);
    CHECK_EQ_INT((int)DSW(RA_ROW), 6);
    CHECK_EQ_INT((int)DSB(DS_001044D8), 0);
    CHECK_EQ_INT((int)DSD(DS_00104B00), (int)(0xBEEF0000u | 0x1Eu));

    /* 0xF polls side 1, not side 0: side 0's right leaves the column. */
    ra_env();
    DSB(DS_00104B25) = 0x0Fu;
    DSD(DS_00107880) = 450000u;
    DSB(RA_PAD0) = 0x10u;
    game_mode_1e_step();
    CHECK_EQ_INT((int)DSW(RA_COL), 0xB);

    /* 0xF with a non-qualifying side-1 score: the state and the reset still
     * happen and the entry is still primed (timer 0x100 -> 0xFF), but
     * nothing is armed. */
    ra_env();
    DSB(DS_00104B25) = 0x0Fu;
    DSD(DS_00107880) = 0u;
    DSW(DS_0010438C) = 0x100u;
    game_mode_1e_step();
    CHECK_EQ_INT((int)DSB(DS_00104B25), 0x0E);
    CHECK_EQ_INT((int)DSW(DS_001044D6), 0x7777);
    CHECK_EQ_INT((int)DSD(DS_00104390), (int)0xDEADBEEFu);
    CHECK_EQ_INT((int)DSW(DS_0010438C), 0xFF);
    CHECK_EQ_INT((int)DSW(RA_COL), 0xB);

    /* 0x10: side 0's 150000 ranks 6; side 0 presses right. */
    ra_env();
    DSB(DS_00104B25) = 0x10u;
    DSD(DS_001077EC) = 150000u;
    DSD(DS_00107880) = 999999u;
    DSB(RA_PAD0) = 0x10u;
    game_mode_1e_step();
    CHECK_EQ_INT((int)DSB(DS_00104B25), 0x0D);
    CHECK_EQ_INT((int)DSW(DS_001044D6), 6);
    CHECK_EQ_INT((int)DSD(DS_00104390), 150000);
    CHECK_EQ_INT((int)DSW(DS_0010438C), 0x2ED);
    CHECK_EQ_INT((int)DSW(RA_COL), 0xE);
    CHECK_EQ_INT((int)DSW(RA_ROW), 6);

    /* 0x10 polls side 0, not side 1. */
    ra_env();
    DSB(DS_00104B25) = 0x10u;
    DSD(DS_001077EC) = 150000u;
    DSB(RA_PAD1) = 0x10u;
    game_mode_1e_step();
    CHECK_EQ_INT((int)DSW(RA_COL), 0xB);
}

/* States 0/4/7 reset the screen (0x1EEEA/0x1F1E2/0x1F316) only on their
 * arming arm. */
static void ra_check_arming_reset(void)
{
    /* state 0: both qualify, side 0 wins (0xB), re-probed and armed. */
    ra_env();
    DSB(DS_00104B25) = 0u;
    DSD(DS_00104AD4) = 2u;
    DSB(DS_00104B1F) = 0u;
    DSD(DS_001077EC) = 999999u;
    DSD(DS_00107880) = 450000u;
    game_mode_1e_step();
    CHECK_EQ_INT((int)DSB(DS_00104B25), 0x0B);
    CHECK_EQ_INT((int)DSW(RA_COL), 0xB);
    CHECK_EQ_INT((int)DSW(RA_ROW), 6);
    CHECK_EQ_INT((int)DSB(DS_001044D8), 0);
    CHECK_EQ_INT((int)DSD(DS_00104390), 999999);
    CHECK_EQ_INT((int)DSB(RA_LIM), 0x12);

    /* state 0: the pair fails: no reset. */
    ra_env();
    DSB(DS_00104B25) = 0u;
    DSD(DS_00104AD4) = 2u;
    DSB(DS_00104B1F) = 0u;
    DSD(DS_001077EC) = 0u;
    DSD(DS_00107880) = 999999u;
    game_mode_1e_step();
    CHECK_EQ_INT((int)DSB(DS_00104B25), 4);
    CHECK_EQ_INT((int)DSW(RA_COL), 0x7777);

    /* state 4: qualifies and DS_00104AD4 != 0 arms state 5. */
    ra_env();
    DSB(DS_00104B25) = 4u;
    DSB(DS_00107813) = 0u;
    DSD(DS_00104AD4) = 1u;
    DSD(DS_001077EC) = 999999u;
    game_mode_1e_step();
    CHECK_EQ_INT((int)DSB(DS_00104B25), 5);
    CHECK_EQ_INT((int)DSW(RA_COL), 0xB);
    CHECK_EQ_INT((int)DSW(RA_ROW), 6);

    /* state 4: qualifies but the second gate fails: armed, not reset. */
    ra_env();
    DSB(DS_00104B25) = 4u;
    DSB(DS_00107813) = 0u;
    DSD(DS_00104AD4) = 0u;
    DSB(DS_00104B14) = 0u;
    DSD(DS_001077EC) = 999999u;
    game_mode_1e_step();
    CHECK_EQ_INT((int)DSB(DS_00104B25), 7);
    CHECK_EQ_INT((int)DSW(RA_COL), 0x7777);
    CHECK_EQ_INT((int)DSD(DS_00104390), 999999);

    /* state 7: side 1 qualifies and DS_00104AD4 != 1 arms state 8. */
    ra_env();
    DSB(DS_00104B25) = 7u;
    DSB(DS_001078A7) = 0u;
    DSD(DS_00104AD4) = 0u;
    DSD(DS_00107880) = 999999u;
    game_mode_1e_step();
    CHECK_EQ_INT((int)DSB(DS_00104B25), 8);
    CHECK_EQ_INT((int)DSW(RA_COL), 0xB);
    CHECK_EQ_INT((int)DSW(RA_ROW), 6);
}

/* ---- record §53-A: 0x20860 and its wiring in 0x24C5C's int 16h loop ------ */

#define NI_Q(i)   (DS_00104458 + 4u * (u32)(i))   /* the queue: entries 0..16 live */
#define NI_GUARD  0x0010449Cu                     /* the dword just past entry 16 */
#define NI_SENT(i) (0x5A5A0000u | (u32)(i))

/* ra_env, then the queue (17 entries and the guard dword past it) at
 * per-entry sentinels, the indices at `w`/`r`, DS_001044D8 at 0x55 and the
 * key latch at a sentinel. The raw's class table DS_00081C84 is the image's. */
static void ni_env(u32 w, u32 r)
{
    ra_env();
    for (u32 i = 0u; i <= 17u; i++) DSD(NI_Q(i)) = NI_SENT(i);
    DSD(DS_001044BC) = w;
    DSD(DS_001044C8) = r;
    DSB(DS_001044D8) = 0x55u;
    DSD(DS_00105F30) = 0x77777777u;
}

/* Every queue entry except `skip` still holds its sentinel (-1: all of them). */
static int ni_untouched(int skip)
{
    for (int i = 0; i <= 17; i++)
        if (i != skip && DSD(NI_Q(i)) != NI_SENT(i)) return 0;
    return 1;
}

/* One accepted key: stored at entry w + 1 (unmasked), the write index
 * becomes (w + 1) & 0xF, DS_001044D8 = 1, nothing else in the queue moves. */
static void ni_expect_store(u32 w, u32 r, u32 c, u32 code)
{
    ni_env(w, r);
    nameentry_key(c);
    CHECK_EQ_INT((long)DSD(NI_Q(w + 1u)), (long)code);
    CHECK(ni_untouched((int)(w + 1u)), "0x20860 writes only entry write + 1");
    CHECK_EQ_INT((int)DSD(DS_001044BC), (int)((w + 1u) & 0xFu));
    CHECK_EQ_INT((int)DSD(DS_001044C8), (int)r);
    CHECK_EQ_INT((int)DSB(DS_001044D8), 1);
}

/* 0x20860 called directly. */
static void ni_check_key(void)
{
    /* Letters of either case become 0..0x19 (0x653ED upper-cases, then -'A'). */
    ni_expect_store(3u, 0u, 'a', 0u);
    ni_expect_store(3u, 0u, 'A', 0u);
    ni_expect_store(3u, 0u, 'm', 12u);
    ni_expect_store(3u, 0u, 'z', 0x19u);
    ni_expect_store(3u, 0u, 'Z', 0x19u);
    /* Backspace, Enter and space. */
    ni_expect_store(3u, 0u, 8u, 0x1Bu);
    ni_expect_store(3u, 0u, 0xDu, 0x1Cu);
    ni_expect_store(3u, 0u, 0x20u, 0x1Au);
    /* The raw's index: write 15 stores at entry 16 (0x104498), not entry 0,
     * and wraps to 0; an empty queue (w == r) and w + 1 != r accept. */
    ni_expect_store(15u, 3u, 'b', 1u);
    ni_expect_store(7u, 7u, 'c', 2u);
    ni_expect_store(5u, 7u, 'd', 3u);

    /* Bytes whose class has neither bit 6 nor bit 7 are dropped before
     * DS_001044D8 is touched: the neighbours of the letter ranges, a digit,
     * ESC, other control bytes, 0x7F and 0x80.. (the class byte for 0xFF is
     * the table's entry 0, (u8)(0xFF + 1)). */
    {
        static const u8 drop[] = { '@', '[', '`', '{', '1', 0x1Bu, 0x09u, 0x0Au,
                                   0x7Fu, 0x80u, 0xE1u, 0xFFu };
        for (u32 k = 0u; k < sizeof drop; k++) {
            ni_env(3u, 0u);
            nameentry_key(drop[k]);
            CHECK(ni_untouched(-1), "0x20860 drops a byte of no letter class");
            CHECK_EQ_INT((int)DSD(DS_001044BC), 3);
            CHECK_EQ_INT((int)DSB(DS_001044D8), 0x55);
        }
    }

    /* A full queue ((w + 1) & 0xF == r) stores nothing but still sets
     * DS_001044D8, even for a byte of no letter class (0x20876 jumps past the
     * class test); w = 15, r = 0 is full through the mask. */
    {
        static const u32 full[][3] = { {4u, 5u, 'a'}, {4u, 5u, '1'}, {15u, 0u, 'a'} };
        for (u32 k = 0u; k < 3u; k++) {
            ni_env(full[k][0], full[k][1]);
            nameentry_key(full[k][2]);
            CHECK(ni_untouched(-1), "0x20860 stores nothing into a full queue");
            CHECK_EQ_INT((int)DSD(DS_001044BC), (int)full[k][0]);
            CHECK_EQ_INT((int)DSB(DS_001044D8), 1);
        }
    }
}

/* The wiring: game_frame's int 16h loop in mode 0x1E (0x24D08..0x24D6C). Mode
 * 0x1E's state byte is 1, the no-op state (0x1F452), so only the loop acts. */
static void ni_frame_env(void)
{
    DSB(DS_00104B25) = 1u;
    DSD(DS_00104AE8) = 0u;       /* no update-table entries */
    DSB(DS_00104B15) = 0u;       /* no demo-fight tail */
    DSB(0x00104B1Bu) = 0u;       /* no CPU command block (0x24C7C) */
    input_clear();
}

static void ni_check_frame(void)
{
    /* 'a' then an extended key (up, ascii 0): 'a' is queued, the extended
     * key only latches its scan code, and the host queue is drained. The mode
     * test is the word at DS_00104B00 (ra_env's dword is 0xBEEF001E). */
    ni_env(0u, 0u);
    ni_frame_env();
    input_push(0x1E, 'a');
    input_push(0x48, 0x00);
    game_frame();
    CHECK_EQ_INT((int)DSD(NI_Q(1)), 0);
    CHECK(ni_untouched(1), "the frame queues only the letter key");
    CHECK_EQ_INT((int)DSD(DS_001044BC), 1);
    CHECK_EQ_INT((int)DSB(DS_001044D8), 1);
    CHECK_EQ_INT((int)DSD(DS_00105F30), 0x48);
    CHECK(!input_has_key(), "mode 0x1E drains the int 16h queue");

    /* The next frame: 'Q', a digit (dropped by 0x20860) and backspace, in
     * order; the latch holds the last key's ascii byte. */
    input_push(0x10, 'Q');
    input_push(0x02, '1');
    input_push(0x0E, 0x08);
    game_frame();
    CHECK_EQ_INT((int)DSD(NI_Q(1)), 0);
    CHECK_EQ_INT((int)DSD(NI_Q(2)), 0x10);
    CHECK_EQ_INT((int)DSD(NI_Q(3)), 0x1B);
    CHECK_EQ_INT((int)DSD(NI_Q(4)), (int)NI_SENT(4));
    CHECK_EQ_INT((int)DSD(DS_001044BC), 3);
    CHECK_EQ_INT((int)DSD(DS_00105F30), 8);
    CHECK(!input_has_key(), "mode 0x1E drains the int 16h queue");

    /* ESC in mode 0x1E is 0x20860's too (dropped for its class), not the
     * quit: the quit flag stays clear and the key is consumed. */
    ni_env(0u, 0u);
    ni_frame_env();
    DSB(DS_000A81A8) = 0u;
    input_push(0x01, 0x1B);
    game_frame();
    CHECK(ni_untouched(-1), "ESC queues nothing");
    CHECK_EQ_INT((int)DSB(DS_001044D8), 0x55);
    CHECK_EQ_INT((int)DSD(DS_00105F30), 0x1B);
    CHECK_EQ_INT((int)DSB(DS_000A81A8), 0);
    CHECK(!input_has_key(), "mode 0x1E consumes ESC");

    /* Another mode (0x1C, an empty case) does not reach 0x20860. */
    ni_env(0u, 0u);
    ni_frame_env();
    DSD(DS_00104B00) = 0x001E001Cu;
    input_push(0x1E, 'a');
    game_frame();
    CHECK(ni_untouched(-1), "mode 0x1C does not queue letters");
    CHECK_EQ_INT((int)DSD(DS_001044BC), 0);
    CHECK_EQ_INT((int)DSB(DS_001044D8), 0x55);
    input_clear();
}

/* End to end: a typed key reaches the name through 0x1F458 (state 5 polls
 * side 0) in the same frame, and the pad cursor is turned off. */
static void ni_check_typing(void)
{
    ni_env(0u, 0u);
    nameentry_arm(3u, 0x12345u);          /* 3 letters */
    nameentry_reset();                    /* cursor (0xB, 6), queue empty */
    ni_frame_env();
    DSB(DS_00104B25) = 5u;
    input_push(0x32, 'm');
    game_frame();
    CHECK_EQ_INT((int)DSB(DS_00104B25), 5);
    CHECK_EQ_INT((int)DSD(DS_001044BC), 1);
    CHECK_EQ_INT((int)DSD(DS_001044C8), 1);   /* 0x1F458 took the letter */
    CHECK_EQ_INT((int)DSB(DS_001044D8), 1);
    CHECK_EQ_INT((int)DSB(RA_CNT), 1);
    CHECK_EQ_INT((int)DSB(DS_00104114 + 0x12u), 1);   /* cell 0 flies */
    CHECK_EQ_INT((int)DSB(DS_00104114 + 0x13u), 12);  /* 'M' */
    CHECK_EQ_INT((int)DSW(RA_COL), 0x1A);     /* 3 * (12 % 7) + 0xB */
    CHECK_EQ_INT((int)DSW(RA_ROW), 9);        /* 3 * (12 / 7) + 6 */
    input_clear();
}

int test_nameentry_input(void)
{
    int before = g_failures;
    if (!ra_save()) { CHECK(0, "the §53-A snapshot allocates"); return 1; }
    ni_check_key();
    ni_check_frame();
    ni_check_typing();
    ra_restore();
    return g_failures - before;
}

/* ---- record §55-A: the rest of 0x24C5C's int 16h keyboard loop ------------ */

#define KL_MODE(m) (0xBEEF0000u | (u32)(m))   /* the word is DS_00104B00's mode */
#define KL_CURSOR  0x77777777u                /* DS_00105F34's row and column words */

/* ra_env's pool and strings, then `mode` in the word DS_00104B00 (the high
 * word a sentinel, so a dword compare or store shows), and every output of
 * the loop's arms at a sentinel: the latch, the prompt byte DS_00104B22, the
 * quit flag, the text cursor, the sound pause bytes (DB 0x40: a toggle makes
 * it 0x41, never 1, so 0x1D220 does not reach 0x1CD9C), and the hook. No
 * sequence handle and no DIG driver, so the pause touches no AIL state. */
static void kl_env(u32 mode)
{
    if (res_count() != 69)
        CHECK(res_load_index("data/game/C", "data/game/C/INDEX") == 69,
              "index loaded");
    ra_env();
    DSD(DS_00104B00) = KL_MODE(mode);
    DSD(DS_00105F30) = 0x77777777u;
    DSB(DS_00104B22) = 0x77u;
    DSB(DS_000A81A8) = 0x5Au;
    DSD(DS_00105F34) = KL_CURSOR;
    DSD(DS_000E87A0) = CH_BUF_A;
    DSD(DS_000E87A4) = CH_BUF_B;
    DSD(DS_001028C0) = 0u;
    DSD(DS_001028C8) = 0u;
    DSB(DS_001028D9) = 0u;
    DSB(DS_001028DA) = 0u;
    DSB(DS_001028D8) = 0x55u;
    DSB(DS_001028DB) = 0x40u;
    DSD(DS_00104AE4) = 0x12345678u;
    for (s32 col = 0; col < 0x2B; col++) DSD(DS_00105F38 + 0xFu * 0xACu + (u32)col * 4u) = 0u;
    input_clear();
}

/* No glyph is left on the pause's row 0xF (0x2F280 released what 0x2F198 drew). */
static int kl_row_f_clear(void)
{
    for (s32 col = 0; col < 0x2B; col++)
        if (ch_cell(0xF, col) != 0u) return 0;
    return 1;
}

/* The column word text_cursor_set leaves for string `id` centred in mode
 * 0x1000 (ch_check_quit_prompt's arithmetic: col + width). */
static int kl_col(u32 id)
{
    int len = (int)strlen((const char *)game_string_get(id));
    return ((0x2B - len) >> 1) + len;
}

/* Nothing but the latch moved: no prompt, no pause, no quit, no mode store. */
static void kl_expect_quiet(u32 mode, u32 latch, const char *msg)
{
    CHECK(!input_has_key(), msg);
    CHECK_EQ_INT((long)DSD(DS_00104B00), (long)KL_MODE(mode));
    CHECK_EQ_INT((int)DSD(DS_00105F30), (int)latch);
    CHECK_EQ_INT((int)DSB(DS_00104B22), 0x77);
    CHECK_EQ_INT((int)DSB(DS_000A81A8), 0x5A);
    CHECK_EQ_INT((long)DSD(DS_00105F34), (long)KL_CURSOR);
    CHECK_EQ_INT((int)DSB(DS_001028D8), 0x55);
    CHECK_EQ_INT((int)DSB(DS_001028DB), 0x40);
}

/* The prompt ran on string `id` and closed: the byte dropped, the question's
 * row and column, the sound resumed, and the prompt's frame cleared the
 * latch (0x2EA85) after the loop stored the key. */
static void kl_expect_prompt(u32 id, int quit)
{
    CHECK(!input_has_key(), "the prompt consumed its keys");
    CHECK_EQ_INT((int)DSB(DS_00104B22), 0);
    CHECK_EQ_INT((int)DSB(DS_000A81A8), quit ? 1 : 0x5A);
    CHECK_EQ_INT((int)DSW(DS_00105F34), 0xA);
    CHECK_EQ_INT((int)DSW(DS_00105F34 + 2u), kl_col(id));
    CHECK_EQ_INT((int)DSB(DS_001028DA), 0);
    CHECK_EQ_INT((int)DSB(DS_001028D8), 0);
    CHECK_EQ_INT((int)DSD(DS_00105F30), 0);
}

/* The wiring: game_frame runs the loop in a mode with no arm (0x1C, an empty
 * case of the switch), where only the latch moves. */
static void kl_check_frame(void)
{
    kl_env(0x1Cu);
    ni_frame_env();
    input_push(0x1E, 'a');
    input_push(0x48, 0x00);
    game_frame();
    CHECK(!input_has_key(), "game_frame drains the int 16h queue in mode 0x1C");
    CHECK_EQ_INT((int)DSD(DS_00105F30), 0x48);
    CHECK_EQ_INT((long)DSD(DS_00104B00), (long)KL_MODE(0x1Cu));
}

/* Enter (ascii 0xD) in mode 3 stores the word 0x27 (0x24EE0); elsewhere it
 * does nothing, and the ascii bytes beside it do nothing in mode 3. */
static void kl_check_enter(void)
{
    const u32 lo_no = (u32)game_string_get(0x1F1u)[0] + 0x20u;

    kl_env(3u);
    input_push(0x1C, 0x0D);
    game_key_loop();
    CHECK(!input_has_key(), "Enter is consumed");
    CHECK_EQ_INT((long)DSD(DS_00104B00), (long)KL_MODE(0x27u));
    CHECK_EQ_INT((int)DSD(DS_00105F30), 0x0D);
    CHECK_EQ_INT((int)DSB(DS_00104B22), 0x77);
    CHECK_EQ_INT((int)DSB(DS_000A81A8), 0x5A);

    /* The mode is read per key: after Enter the ESC is mode 0x27's (no
     * prompt), so the 'n' behind it is only latched. */
    kl_env(3u);
    input_push(0x1C, 0x0D);
    input_push(0x01, 0x1B);
    input_push(0x31, (u8)lo_no);
    game_key_loop();
    kl_expect_quiet(0x27u, lo_no, "Enter, ESC, 'n' are consumed");

    kl_env(0x1Cu);
    input_push(0x1C, 0x0D);
    game_key_loop();
    kl_expect_quiet(0x1Cu, 0x0D, "Enter outside mode 3 is consumed");

    /* The neighbours of 0xD, 0x1B and 0x20 in mode 3. */
    static const u8 other[] = { 0x0C, 0x0E, 0x1A, 0x1C, 0x1F, 0x21 };
    for (u32 i = 0u; i < sizeof other; i++) {
        kl_env(3u);
        input_push(0x2C, other[i]);
        game_key_loop();
        kl_expect_quiet(3u, other[i], "a key of no arm is consumed");
    }
}

/* ESC (ascii 0x1B): mode 3 asks 0x1EE (AL = 0), mode 0x27 nothing, any other
 * mode 0x1EF (AL = 1). */
static void kl_check_esc(void)
{
    const u8 yes = game_string_get(0x1F0u)[0];
    const u8 no = game_string_get(0x1F1u)[0];
    CHECK(kl_col(0x1EEu) != kl_col(0x1EFu), "the two questions centre apart");

    kl_env(3u);
    input_push(0x01, 0x1B);
    input_push(0x15, yes);
    game_key_loop();
    kl_expect_prompt(0x1EEu, 1);

    kl_env(0x1Cu);
    input_push(0x01, 0x1B);
    input_push(0x31, no);
    game_key_loop();
    kl_expect_prompt(0x1EFu, 0);
    CHECK_EQ_INT((long)DSD(DS_00104B00), (long)KL_MODE(0x1Cu));

    /* AL = 1's yes: the port's longjmp stand-in is the same quit flag. */
    kl_env(0x04u);
    input_push(0x01, 0x1B);
    input_push(0x15, yes);
    game_key_loop();
    kl_expect_prompt(0x1EFu, 1);

    kl_env(0x27u);
    input_push(0x01, 0x1B);
    input_push(0x31, (u8)(no + 0x20));
    game_key_loop();
    kl_expect_quiet(0x27u, (u32)no + 0x20u, "ESC in mode 0x27 is consumed");
}

/* Space (ascii 0x20): the pause shows string 0x1E8 on row 0xF and waits for
 * another space; modes 3 and 0x27, and 0x17 under the hook 0x10E80, skip it.
 * The queue is space, 'n', space (then 'a' for a pause): a pause that took
 * any key would pause twice and end on 'a', a skipped pause latches 0x20. */
static void kl_check_pause(void)
{
    kl_env(0x1Cu);
    input_push(0x39, ' ');
    input_push(0x31, 'n');
    input_push(0x39, ' ');
    input_push(0x1E, 'a');
    game_key_loop();
    CHECK(!input_has_key(), "the pause consumed its keys");
    CHECK_EQ_INT((int)DSD(DS_00105F30), 'a');   /* the key after the pause */
    CHECK_EQ_INT((int)DSB(DS_00104B22), 0);
    CHECK_EQ_INT((int)DSB(DS_000A81A8), 0x5A);
    CHECK_EQ_INT((int)DSW(DS_00105F34), 0xF);
    CHECK_EQ_INT((int)DSW(DS_00105F34 + 2u), kl_col(0x1E8u));
    CHECK(kl_row_f_clear(), "the pause released its text");
    CHECK_EQ_INT((int)DSD(DS_000E87A0), (int)CH_BUF_B);   /* one frame presented */
    CHECK_EQ_INT((int)DSB(DS_001028DA), 0);      /* paused, then resumed */
    CHECK_EQ_INT((int)DSB(DS_001028D8), 0);      /* seed 0x55 */
    CHECK_EQ_INT((long)DSD(DS_00104B00), (long)KL_MODE(0x1Cu));

    /* Mode 0x17 pauses under any other hook, and another mode under the
     * hook 0x10E80. */
    for (u32 pass = 0u; pass < 2u; pass++) {
        kl_env(pass == 0u ? 0x17u : 0x1Cu);
        if (pass == 1u) DSD(DS_00104AE4) = FN_00010E80;
        input_push(0x39, ' ');
        input_push(0x31, 'n');
        input_push(0x39, ' ');
        input_push(0x1E, 'a');
        game_key_loop();
        CHECK(!input_has_key(), "the pause consumed its keys");
        CHECK_EQ_INT((int)DSB(DS_00104B22), 0);
        CHECK_EQ_INT((int)DSW(DS_00105F34), 0xF);
        CHECK_EQ_INT((int)DSD(DS_00105F30), 'a');
    }

    static const u32 skip[] = { 3u, 0x27u, 0x17u };
    for (u32 i = 0u; i < 3u; i++) {
        kl_env(skip[i]);
        if (skip[i] == 0x17u) DSD(DS_00104AE4) = FN_00010E80;
        input_push(0x39, ' ');
        input_push(0x31, 'n');
        input_push(0x39, ' ');
        game_key_loop();
        kl_expect_quiet(skip[i], 0x20u, "a skipped pause consumes its keys");
    }
}

/* The extended keys (ascii 0), in any mode, 0x1E included: 0x10 asks 0x1EE
 * (AL = 0), 0x1F toggles the sample pause, 0x32 the music pause, 0x24 (the
 * joystick calibration, host-owned) and any other scan code only latch. */
static void kl_check_extended(void)
{
    const u8 yes = game_string_get(0x1F0u)[0];

    kl_env(0x1Cu);
    input_push(0x10, 0x00);
    input_push(0x15, yes);
    game_key_loop();
    kl_expect_prompt(0x1EEu, 1);

    kl_env(0x1Cu);
    input_push(0x1F, 0x00);
    game_key_loop();
    CHECK_EQ_INT((int)DSB(DS_001028DB), 0x41);
    CHECK_EQ_INT((int)DSD(DS_00105F30), 0x1F);

    kl_env(0x1Eu);
    input_push(0x1F, 0x00);
    game_key_loop();
    CHECK_EQ_INT((int)DSB(DS_001028DB), 0x41);  /* mode 0x1E reaches the arm */

    /* 0x1D1B0 with the music paused: it only clears the byte (no song). */
    kl_env(0x1Cu);
    DSB(DS_001028DA) = 1u;
    input_push(0x32, 0x00);
    game_key_loop();
    CHECK_EQ_INT((int)DSB(DS_001028DA), 0);
    CHECK_EQ_INT((int)DSB(DS_001028DB), 0x40);

    /* An ascii key on those scan codes ('s', 'm', 'q') is no extended key;
     * the 'n' behind them would answer a prompt that 'q' wrongly opened. */
    kl_env(0x1Cu);
    DSB(DS_001028DA) = 1u;
    input_push(0x1F, 's');
    input_push(0x32, 'm');
    input_push(0x10, 'q');
    input_push(0x31, 'n');
    game_key_loop();
    CHECK_EQ_INT((int)DSB(DS_001028DA), 1);
    DSB(DS_001028DA) = 0u;
    kl_expect_quiet(0x1Cu, 'n', "ascii 's', 'm', 'q' only latch");

    static const u8 quiet[] = { 0x24, 0x48, 0x11, 0x20, 0x31 };
    for (u32 i = 0u; i < sizeof quiet; i++) {
        kl_env(3u);
        input_push(quiet[i], 0x00);
        game_key_loop();
        kl_expect_quiet(3u, quiet[i], "an extended key of no arm is consumed");
    }
}

int test_key_loop(void)
{
    int before = g_failures;
    if (!ra_save()) { CHECK(0, "the §55-A snapshot allocates"); return 1; }
    kl_check_frame();
    kl_check_enter();
    kl_check_esc();
    kl_check_pause();
    kl_check_extended();
    input_clear();
    ra_restore();
    return g_failures - before;
}

int test_mode1e_rearm(void)
{
    int before = g_failures;
    if (!ra_save()) { CHECK(0, "the §51-A snapshot allocates"); return 1; }
    ra_check_arm();
    ra_check_rearm();
    ra_check_arming_reset();
    ra_restore();
    return g_failures - before;
}

/* ---- record k7-k12 §4: the K12 voice sites ---------------------------------- */

/* Addresses symbols.h has no name for. */
#define VS_B1A    0x00104B1Au   /* byte: the current side (mode 0x22) */
#define VS_B29    0x00104529u   /* DS_00104528's second byte */
#define VS_MENU   0x03E31000u   /* a zeroed menu table: no items, no callback */

/* A private actor pool (the §51-A recipe), so no driver spawns into the
 * image's own pool. */
static void vs_pools(void)
{
    DSD(DS_001014F4) = RA_POOL;
    DSD(DS_001014EC) = RA_PSET;
    actors_reset();
}

/* game_state_step's gates that are not the row's: no coin poll (0x11D04's
 * DS_00104B1D test), both per-state tails return at once (DS_000F0A71), and
 * the overlay takes its attract return (0x2BF18). */
static void vs_state(u16 state)
{
    DSB(DS_00104B1D) = 1u;
    DSB(DS_000F0A71) = 1u;
    DSB(DS_0009AD58) = 1u;
    DSW(DS_000F0A64) = state;
}

/* Rows 8/9: 0x10DB0/0x10E18 with the gate open and DS_00104B19 byte 2 set. */
static void vs_pause_tail(void)
{
    DSB(DS_000F0A71) = 0u;
    DSB(DS_001088D8 + 3u) = 0x20u;
    DSB(DS_001088D8 + 1u) = 0x10u;
    DSB(DS_00104B19 + 2u) = 1u;
    frontend_pause_tail();
}
static void vs_continue_tail(void)
{
    DSB(DS_000F0A71) = 0u;
    DSB(DS_001088D8 + 3u) = 0x10u;
    DSB(DS_001088D8 + 1u) = 0x20u;
    DSB(DS_00104B19 + 2u) = 1u;
    frontend_continue_tail();
}
/* Row 10: 0x11000 case 0. The media dir is cleared so the two boot movies
 * fail at once (each would load its file into the bump heap), then set back
 * to test_flow's "data/game/C" (game_set_game_dir). */
static void vs_attract0(void)
{
    vs_pools();
    DSB(DS_000F0A6F) = 0u;
    attract_set_media_dir(NULL);
    attract_step();
    attract_set_media_dir("data/game/C");
}
/* Rows 27/29: 0x4367C, DS_00104B1D = 3 (0x4454C) or not. */
static void vs_hook_4367c_3(void)
{
    vs_pools();
    DSB(DS_00104B1D) = 3u;
    fight_hook_4367c();
}
static void vs_hook_4367c(void)
{
    vs_pools();
    DSB(DS_00104B1D) = 0u;
    fight_hook_4367c();
}
/* Row 44: 0x49C78 case 7 (0x49FC2): one entry of type 7, its actor's +0x3C
 * inside (0, 0x5400) and +0x1C bit 2 clear (0x49FCA..0x49FE1). */
static void vs_effects_7(void)
{
    u32 entry = FIGHT_RECS + 0x3000u;
    u32 rec = FIGHT_RECS + 0x3100u;
    (void)tf_demo_fixture();
    mem_fill(FIGHT_RECS, 0, 0x4000u);
    DSW(FIGHT_RECS + 0x56u) = 1;
    DSW(FIGHT_RECS + 0x100u + 0x56u) = 2;
    DSW(DS_00104B00) = 3;
    DSD(DS_0010884C) = entry;
    DSD(entry) = DS_0010884C;
    DSD(entry + 8u) = rec;
    DSB(entry + 0x1Cu) = 0u;
    DSB(entry + 0x1Eu) = 7u;
    DSB(entry + 0x21u) = 0u;
    DSB(rec + 0x48u) = 0x20u;
    DSD(rec + 0x3Cu) = 0x1000u;
    fight_effects_pass();
}
/* Rows 139/140: 0x257A4, both calls unconditional. */
static void vs_coin_divert(void) { vs_pools(); game_coin_divert(1u); }
/* Row 141: 0x28DA4, unconditional. */
static void vs_player_join(void)
{
    vs_pools();
    game_string_table_load("data/game/C");
    flow_player_join(0u);
}
/* Row 166: 0x25AE8, the first statement. */
static void vs_hook_25ae8(void)
{
    vs_pools();
    game_string_table_load("data/game/C");
    game_hook_25ae8();
}
/* Rows 172..174: state 4 (0x11578) phases 0, 1, 2. */
static void vs_state4_phase(u16 phase)
{
    vs_pools();
    vs_state(4u);
    DSW(DS_0009AD98) = phase;
    game_state_step();
}
static void vs_state4_0(void) { vs_state4_phase(0u); }
static void vs_state4_1(void) { vs_state4_phase(1u); }
static void vs_state4_2(void) { vs_state4_phase(2u); }
/* Row 175: state 6 (0x11A8C), the first statement. */
static void vs_state6(void)
{
    (void)tf_demo_fixture();
    vs_pools();
    vs_state(6u);
    game_state_step();
}
/* Rows 176/178/182: 0x1EEC0 states 0, 4 and 7 with a qualifying score (ra_env
 * seeds the fresh-CMOS table 0: 999999 beats record 0). */
static void vs_mode1e_0(void)
{
    ra_env();
    DSB(DS_00104B25) = 0u;
    DSD(DS_00104AD4) = 2u;
    DSB(DS_00104B1F) = 0u;
    DSD(DS_001077EC) = 999999u;
    DSD(DS_00107880) = 450000u;
    game_mode_1e_step();
}
static void vs_mode1e_4(void)
{
    ra_env();
    DSB(DS_00104B25) = 4u;
    DSB(DS_00107813) = 0u;
    DSD(DS_001077EC) = 999999u;
    DSD(DS_00104AD4) = 1u;                      /* != 0: advance (0x1F1B8) */
    game_mode_1e_step();
}
static void vs_mode1e_7(void)
{
    ra_env();
    DSB(DS_00104B25) = 7u;
    DSB(DS_001078A7) = 0u;
    DSD(DS_00107880) = 999999u;
    DSD(DS_00104AD4) = 0u;                      /* != 1: advance (0x1F2D6) */
    game_mode_1e_step();
}
/* Row 191: 0x26C8C -> 0x26D4C with DS_00104AC4 <= 0 (0x26D54); bit 1 of
 * DS_00104529 takes the actor arm, as test_fight.c's check_mode_22 does. */
static void vs_mode22(void)
{
    (void)tf_demo_fixture();
    vs_pools();
    DSB(VS_B1A) = 0u;
    DSD(DS_00104AC4) = 0u;
    DSB(VS_B29) = 2u;
    game_mode_22_step();
}
/* Row 194: state 5 (0x11DE8), the first statement. */
static void vs_state5(void)
{
    ra_env();
    vs_state(5u);
    game_state_step();
}
/* Row 195: state 7's timer exit (0x11BCC), pre-value 1 (0x11E72). */
static void vs_state7_exit(void)
{
    vs_state(7u);
    DSW(DS_000F0A6A) = 1u;
    DSW(DS_000F0A6C) = 0u;
    game_state_step();
}
/* Rows 196/197: 0x2FA40 and 0x2FFC4 (first call, DS_00107414 = 0) on a table
 * with no item and no callback; ESC ends menu_run with flags 0 (0x2FD4C). */
static void vs_menu_run(void)
{
    vs_pools();
    mem_fill(VS_MENU, 0, 0x100u);
    DSD(DS_00101514) = MT_LAYOUT;
    tf_menu_press(0x2000000u);
    (void)menu_run(VS_MENU, 0x10u, 0u);
}
static void vs_menu_step(void)
{
    vs_pools();
    mem_fill(VS_MENU, 0, 0x100u);
    DSD(DS_00101514) = MT_LAYOUT;
    DSB(DS_00107414) = 0u;
    (void)menu_step(VS_MENU, 0x10u, 0u);
}

/* Record k7-k12 §4, batch A: the 0x100 stop-alls and the case-0/6 sites.
 * Only wired ids are listed; a path's unwired voices (later batches) are
 * added to its row when they are wired. */
#define K12_A_ROWS 22
static const TfVoiceSite k12_a[] = {
    {   8u, vs_pause_tail,    1u, { 0x100u } },           /* 0x10E06 */
    {   9u, vs_continue_tail, 1u, { 0x100u } },           /* 0x10E6E */
    {  10u, vs_attract0,      1u, { 0x100u } },           /* 0x11024 */
    {  27u, vs_hook_4367c_3,  1u, { 0x100u } },           /* 0x44557 */
    {  29u, vs_hook_4367c,    1u, { 0x100u } },           /* 0x43696 */
    {  44u, vs_effects_7,     1u, { 0xDEu } },            /* 0x49FF1 */
    { 139u, vs_coin_divert,   2u, { 0x100u, 0x53u } },    /* 0x257AD, 0x25820 */
    { 140u, vs_coin_divert,   2u, { 0x100u, 0x53u } },    /* 0x257AD, 0x25820 */
    { 141u, vs_player_join,   1u, { 0x100u } },           /* 0x28DCA */
    { 166u, vs_hook_25ae8,    1u, { 0x100u } },           /* 0x25AF1 */
    { 172u, vs_state4_0,      1u, { 0x100u } },           /* 0x1159F */
    { 173u, vs_state4_1,      1u, { 0x100u } },           /* 0x116C4 */
    { 174u, vs_state4_2,      1u, { 0x100u } },           /* 0x11844 */
    { 175u, vs_state6,        1u, { 0x100u } },           /* 0x11A94 */
    { 176u, vs_mode1e_0,      1u, { 0x100u } },           /* 0x1EEF4 */
    { 178u, vs_mode1e_4,      1u, { 0x100u } },           /* 0x1F1EC */
    { 182u, vs_mode1e_7,      1u, { 0x100u } },           /* 0x1F307 */
    { 191u, vs_mode22,        1u, { 0x29u } },            /* 0x26DA9 */
    { 194u, vs_state5,        1u, { 0x100u } },           /* 0x11DF2 */
    { 195u, vs_state7_exit,   1u, { 0x100u } },           /* 0x11BF0 */
    { 196u, vs_menu_run,      1u, { 0x100u } },           /* 0x2FA6D */
    { 197u, vs_menu_step,     1u, { 0x100u } },           /* 0x2FFFD */
};

/* ---- record k7-k12 §5, batch B1: the match-flow music requests and stops -- */

#define VS_810D   0x0010810Du   /* byte: the winner side (DS_0010810A's top byte) */

/* The private pools and the string table: the rows below draw text. */
static void vs_text(void)
{
    vs_pools();
    game_string_table_load("data/game/C");
}
/* Row 130: 0x41578 on an empty front-end list (no darken), mode 0 for
 * 0x32A3C. */
static void vs_darken_marked(void)
{
    vs_pools();
    mem_fill(DS_00107608, 0, 0x190u);
    DSD(DS_00104ABC) = 0u;
    DSB(DS_00104B19) = 0u;
    frontend_darken_marked();
}
/* Row 134: 0x41C28 case 5 with DS_0010810E = 8 (0x420E8) and DS_00104B1F !=
 * 3 (0x420F9). Both camera-target records are the slots with +0x63 = 1, so
 * 0x418F4 skips both sides; DS_00108104[0] = 7 takes 0x4160C's text arm. */
static void vs_mode12_5(void)
{
    vs_text();
    DSB(DS_00104B25) = 5u;
    DSD(DS_00104AD4) = 0u;
    DSB(DS_0010810E) = 8u;
    DSB(DS_00104B1F) = 0u;
    DSD(DS_001077A8) = DS_001077B0;
    DSD(DS_001077A8 + 4u) = DS_001077B0 + 0x94u;
    DSB(DS_00107813) = 1u;
    DSB(DS_001078A7) = 1u;
    DSB(DS_00108104) = 7u;
    DSB(VS_B29) = 0u;
    game_mode_12_step();
}
/* Row 136: 0x28D80, the first statement. */
static void vs_char_hook_voice(void) { vs_pools(); frontend_char_screen_hook_voice(); }
/* Rows 137/138: 0x26998 and 0x270BC (check_mode_1a_hooks' seeds: stage 2,
 * no dust, no sound bank). */
static void vs_fighter_hook_seed(void)
{
    vs_pools();
    DSW(DS_00104AFC) = 2u;
    DSB(DS_00104B1E) = 0xFFu;
    DSB(DS_0010816A) = 1u;
    DSB(DS_0010816A + 1u) = 4u;
    DSB(DS_00104B14) = 1u;
    DSB(DS_0010780B) = 0x10u;
    DSB(DS_0010780B + 0x94u) = 0x77u;
}
static void vs_hook_26998(void)
{
    vs_fighter_hook_seed();
    DSB(DS_001078A7) = 0u;
    game_hook_26998();
}
static void vs_hook_270bc(void)
{
    vs_fighter_hook_seed();
    DSB(VS_810D) = 1u;
    game_hook_270bc();
}
/* Rows 142-144: 0x28130. -1 with DS_00104B16 = 3 draws nothing (0x28266);
 * 0 with the signed byte DS_001088F2 = 0 (below 1, 0x2816F); 2 draws 0x42. */
static void vs_result_text(u32 r, u8 b16, u8 f2)
{
    vs_text();
    DSD(DS_00104AD4) = r;
    DSB(DS_00104B16) = b16;
    DSB(DS_001088F2) = f2;
    DSB(DS_00107813) = 0u;
    DSB(DS_001078A7) = 0u;
    flow_match_result_text();
}
static void vs_result_m1(void) { vs_result_text(0xFFFFFFFFu, 3u, 0x40u); }
static void vs_result_0(void)  { vs_result_text(0u, 0x77u, 0u); }
static void vs_result_2(void)  { vs_result_text(2u, 0x77u, 0x40u); }
/* Rows 146/147: 0x272DC's second arm, 0x27347 0x27 then 0x27351 0x22 (the
 * side's DS_0010780A below 0x78, DS_00104B12's at or above; §1.3 seed). */
static void vs_arena_ko_b(void)
{
    DSB(VS_810D) = 0u;
    DSB(DS_0010780A) = 0x10u;                  /* side 0 below 0x78: first arm skipped */
    DSB(DS_00104B12) = 1u;
    DSB(DS_0010780A + 0x94u) = 0x78u;          /* side 1 at 0x78: second arm */
    flow_arena_ko_check();
}
/* Rows 148/149: 0x27A2C with no credit (0x42F60 returns 0 at 0x2C060), no
 * forced tick, the frame word's low six bits 0 (0x27ADF) and DS_00108110 = 0,
 * so the decrement goes negative (0x27AF5); 0x29B74 walks an empty list. */
static void vs_mode0e(void)
{
    vs_pools();
    DSB(DS_00105D60) = 0u;
    DSD(DS_00105C00) = 0u;
    DSB(DS_00105C04) = 0u;
    DSB(DS_00104B1F) = 0u;
    DSW(DS_000EF6DC) = 0x40u;
    DSB(DS_00108110) = 0u;
    mem_fill(DS_00107608, 0, 0x190u);
    game_mode_0e_step();
}
/* The match-end fixture of rows 150-153: tf_demo_fixture's inert prelude
 * (0x3C5CC..0x12DA8), a byte DS_00104B0C set, both slots' +4 records live,
 * the empty 0x4DBEC free list and 0x28130's -1/3 arm (the 0x24 voice, no
 * text). */
static void vs_match_end_seed(void)
{
    (void)tf_demo_fixture();
    vs_text();
    DSB(DS_00104B0C) = 1u;
    DSD(DS_001077B4) = actor_alloc(0);
    DSD(DS_001077B4 + 0x94u) = actor_alloc(0);
    DSD(DS_001083C4) = DS_001083C4;
    DSD(DS_001083C4 + 4u) = DS_001083C4;
    DSD(DS_00104AD4) = 0xFFFFFFFFu;
    DSB(DS_00104B16) = 3u;
    DSB(VS_B29) = 0u;
    DSD(DS_00105BF8) = 0u;                     /* 0x2C2B0 releases no cell */
    DSD(DS_001077EC) = 1211u;                  /* 0x2765C: no 0x41310 */
}
/* The replace arm's loser `s`: only character 3 is free (0x2716C), its
 * 0xA8628 entry is 0 (fn_resolve NULL: no entrance runs), and s's HUD
 * records (0x1D764's bar and stream, 0x1D838's badge slot) are live or 0. */
static void vs_replace_seed(u32 s)
{
    for (u32 c = 0; c < 7u; c++) DSB(DS_00104B02 + c) = 0x20u;
    DSB(DS_00104B02 + 3u) = 0x01u;
    DSD(DS_000A8628 + 3u * 4u) = 0u;
    DSB(DS_0010452C) = 3u;
    DSD(DS_001082D0) = 0u;
    DSD(DS_001028F0 + s * 4u) = actor_alloc(0);
    DSD(DS_001028F8 + s * 4u) = actor_alloc(0);
    DSD(DS_001028E0 + s * 4u) = 0u;
}
/* Row 150: 0x274FC's seventh round, 0x2759E 0x2A then 0x28130's 0x24. */
static void vs_mode0d_final(void)
{
    vs_match_end_seed();
    DSB(DS_00104B21) = 6u;
    DSB(VS_810D) = 0u;
    game_mode_0d_step();
}
/* Row 151: 0x274FC's replace arm, loser side 1; n = DS_00104B0A ^ 1 gives
 * 0x25 + (n != 0) (0x277A0 `setne`): the row runs b0a = 1 (n 0, 0x25),
 * vs_setne_check b0a = 0 (n 1, 0x26). */
static void vs_mode0d_replace_b0a(u8 b0a)
{
    vs_match_end_seed();
    vs_replace_seed(1u);
    DSB(DS_00104B21) = 0u;
    DSB(DS_00104B12) = 1u;
    DSB(DS_00104B0A) = b0a;
    game_mode_0d_step();
}
static void vs_mode0d_replace(void) { vs_mode0d_replace_b0a(1u); }
/* Row 152: 0x296B8's final arm, side 0's count 3 -> 4 (0x297A9). */
static void vs_mode32_final(void)
{
    vs_match_end_seed();
    DSB(DS_00104B09) = 0u;
    DSB(DS_00104AF0) = 3u;
    DSB(DS_00104AF1) = 1u;
    game_mode_32_step();
}
/* Row 153: 0x296B8's replace arm, side 1's count 0 -> 1 and the team byte
 * DS_00108134[4 + 1] = 3; the row runs b0a = 0 (n 1, 0x26, 0x29950
 * `setne`), vs_setne_check b0a = 1 (n 0, 0x25). */
static void vs_mode32_replace_b0a(u8 b0a)
{
    vs_match_end_seed();
    vs_replace_seed(1u);
    DSB(DS_00104B09) = 1u;
    DSB(DS_00104AF0) = 0u;
    DSB(DS_00104AF1) = 0u;
    DSB(DS_00108134 + 4u + 1u) = 3u;
    DSB(DS_00104B0A) = b0a;
    game_mode_32_step();
}
static void vs_mode32_replace(void) { vs_mode32_replace_b0a(0u); }
/* Rows 156-159: 0x29970 with side 0's +0x5A (0x29974) or side 1's
 * (DS_0010789E, 0x299AD) at 0x78 and the other at 0. 0x27C48 then runs with
 * DS_00104B1D = 3 (no round bonus, 0x27CAA..) and the win markers present
 * (0x256F4 spawns none). */
static void vs_round_over(u32 side)
{
    vs_pools();
    DSB(DS_0010780A) = side == 0u ? 0x78u : 0u;
    DSB(DS_0010789E) = side == 0u ? 0u : 0x78u;
    DSB(DS_00104B1D) = 3u;
    DSB(DS_00104AF2) = 0u;
    DSB(DS_00104AF3) = 0u;
    for (u32 k = 0; k < 4u; k++) {
        DSD(DS_00104A88 + k * 4u) = 0x5555u;
        DSD(DS_00104A98 + k * 4u) = 0x5555u;
    }
    flow_round_over_check();
}
static void vs_round_over_0(void) { vs_round_over(0u); }
static void vs_round_over_1(void) { vs_round_over(1u); }
/* Row 160: 0x29638 with DS_00104AFE = 1, so the decrement is <= 0 (0x2968A),
 * on tf_demo_fixture's inert passes (its effect list is empty). */
static void vs_mode33(void)
{
    (void)tf_demo_fixture();
    vs_pools();
    DSW(DS_00104AFE) = 1u;
    game_mode_33_step();
}

/* Record k7-k12 §5, batch B1: 21 wiring points, all case 1 or 5 (music
 * requests and stops). Rows 150/152 also log 0x28130's 0x24 (rows
 * 142-144's arm), which is wired in this batch. */
#define K12_B1_ROWS 21
static const TfVoiceSite k12_b1[] = {
    { 130u, vs_darken_marked,   1u, { 0x33u } },          /* 0x415DC */
    { 134u, vs_mode12_5,        1u, { 0x33u } },          /* 0x42112 */
    { 136u, vs_char_hook_voice, 1u, { 0x2Eu } },          /* 0x28D8B */
    { 137u, vs_hook_26998,      1u, { 0x28u } },          /* 0x26A2C */
    { 138u, vs_hook_270bc,      1u, { 0x25u } },          /* 0x270F9 */
    { 142u, vs_result_m1,       1u, { 0x24u } },          /* 0x282BB (-1) */
    { 143u, vs_result_0,        1u, { 0x24u } },          /* 0x2817F */
    { 144u, vs_result_2,        1u, { 0x24u } },          /* 0x282BB (2) */
    { 146u, vs_arena_ko_b,      2u, { 0x27u, 0x22u } },   /* 0x27347, 0x27351 */
    { 147u, vs_arena_ko_b,      2u, { 0x27u, 0x22u } },   /* 0x27347, 0x27351 */
    { 148u, vs_mode0e,          2u, { 0x27u, 0x22u } },   /* 0x27B01, 0x27B0D */
    { 149u, vs_mode0e,          2u, { 0x27u, 0x22u } },   /* 0x27B01, 0x27B0D */
    { 150u, vs_mode0d_final,    2u, { 0x2Au, 0x24u } },   /* 0x2759E, 0x282BB */
    { 151u, vs_mode0d_replace,  1u, { 0x25u } },          /* 0x277B0 */
    { 152u, vs_mode32_final,    2u, { 0x2Au, 0x24u } },   /* 0x297C4, 0x282BB */
    { 153u, vs_mode32_replace,  1u, { 0x26u } },          /* 0x29960 */
    { 156u, vs_round_over_0,    2u, { 0x27u, 0x22u } },   /* 0x29983, 0x29992 */
    { 157u, vs_round_over_0,    2u, { 0x27u, 0x22u } },   /* 0x29983, 0x29992 */
    { 158u, vs_round_over_1,    2u, { 0x27u, 0x22u } },   /* 0x299C1, 0x299CD */
    { 159u, vs_round_over_1,    2u, { 0x27u, 0x22u } },   /* 0x299C1, 0x299CD */
    { 160u, vs_mode33,          1u, { 0x2Bu } },          /* 0x29698 */
};

/* The checks below put back what tf_voice_sites does after each case, with
 * its own tf_voice_snap/tf_voice_put (test_fixtures.c): the data object, the
 * two actor pools DS_001014EC/DS_001014F4 name at entry (their pools are
 * fixed for the process), the aperture and the DAC; and they run with
 * DS_001028C8 = 0 (no DIG, no bank read). */
/* How many times `id` is in the voice log. */
static int vs_log_count(u32 id)
{
    int n = 0;
    for (u32 k = 0; k < sound_voice_log_count(); k++)
        if (sound_voice_log_at(k) == id) n++;
    return n;
}

/* Row 143's gate (0x28164..0x28178 for side 0, 0x281DA..0x281EE for side 1):
 * the 0x24 voice when the signed byte DS_001088F2 is below 1 (`sar eax,0x18;
 * cmp eax,1; jl`) or side r's own slot +0x63 is non-zero, and none when both
 * fail. 0xFF is -1 (a signed compare), 1 is not below 1, and the other side's
 * +0x63 is not read. */
static void vs_result_gate_check(void)
{
    static const struct { u32 r; u8 f2, own, other; int want; } g[6] = {
        { 0u, 0x40u, 0u, 1u, 0 }, { 0u, 0xFFu, 0u, 0u, 1 }, { 0u, 0x40u, 1u, 0u, 1 },
        { 1u, 0x01u, 0u, 1u, 0 }, { 1u, 0x00u, 0u, 0u, 1 }, { 1u, 0x40u, 1u, 0u, 1 },
    };
    for (u32 i = 0; i < 6u; i++) {
        tf_voice_snap();
        vs_text();
        DSD(DS_00104AD4) = g[i].r;
        DSB(DS_001088F2) = g[i].f2;
        DSB(DS_00107813 + g[i].r * 0x94u) = g[i].own;
        DSB(DS_00107813 + (g[i].r ^ 1u) * 0x94u) = g[i].other;
        flow_match_result_text();
        int n = vs_log_count(0x24u);
        if (n != g[i].want)
            fprintf(stderr, "row 143 gate case %u: %d 0x24 voices, want %d\n",
                    (unsigned)i, n, g[i].want);
        CHECK_EQ_INT(n, g[i].want);
        tf_voice_put();
    }
}

/* Rows 151/153's other setne outcome: the table rows run 0x274FC with n = 0
 * (0x25) and 0x296B8 with n = 1 (0x26); here 0x274FC with n = 1 posts 0x26
 * and not 0x25, and 0x296B8 with n = 0 posts 0x25 and not 0x26. */
static void vs_setne_check(void)
{
    static const struct { u32 row; void (*drive)(u8); u8 b0a; u32 want, not_; } c[2] = {
        { 151u, vs_mode0d_replace_b0a, 0u, 0x26u, 0x25u },
        { 153u, vs_mode32_replace_b0a, 1u, 0x25u, 0x26u },
    };
    for (u32 i = 0; i < 2u; i++) {
        tf_voice_snap();
        c[i].drive(c[i].b0a);
        int got = vs_log_count(c[i].want), bad = vs_log_count(c[i].not_);
        if (got != 1 || bad != 0)
            fprintf(stderr, "row %u setne case %u: 0x%X x%d, 0x%X x%d\n",
                    (unsigned)c[i].row, (unsigned)i, (unsigned)c[i].want, got,
                    (unsigned)c[i].not_, bad);
        CHECK_EQ_INT(got, 1);
        CHECK_EQ_INT(bad, 0);
        tf_voice_put();
    }
}

/* ---- record k7-k12 §6: batch B2, the flow and attract rows ---------------- */

#define VS2_R          (FIGHT_RECS + 0x400u)   /* a scratch camera-target record (m0f_seed's) */

/* Row 13: 0x11000 case 4 on the actor case 3 spawns (0x111AF, into
 * DS_000F0A50). +0x2C = 0x1100 gives 0x1000 after the 0x100 step, not above
 * 0x1000 (0x111E4 `jg`); DS_000F0A5C (a dword, 0x111EA) = 0 posts 0x54
 * (0x111F3), non-zero 0x56 (0x111FA). Both arms run, 0x54 first.
 * DS_0009AD58 = 1 keeps the 0x10F28 tail (rows 6/7) off. */
static void vs_attract_4(void)
{
    u32 rec;
    vs_pools();
    DSB(DS_0009AD58) = 1u;
    DSB(DS_000F0A6F) = 3u;
    DSD(DS_000F0A50) = 0u;
    attract_step();                             /* case 3: the spawn, phase 4 */
    rec = DSD(DS_000F0A50);
    if (rec == 0u) return;                      /* no voice: the row fails */
    DSW(rec + 0x2Cu) = 0x1100u;
    DSD(DS_000F0A5C) = 0u;
    attract_step();                             /* 0x54, phase 5 */
    DSB(DS_000F0A6F) = 4u;
    DSW(rec + 0x2Cu) = 0x1100u;
    DSD(DS_000F0A5C) = 1u;
    attract_step();                             /* 0x56 */
}
/* Row 164: 0x42CB4 with r = DS_00104AD4 = 0 and no credit press
 * (DS_001088E4 = 0: 0x42F60 returns 0), no forced tick (DS_00105C04 = 0),
 * DS_00104B1F = 0 (tick 0) on a frame with DS_000EF6DC & 0x3F = 0, and
 * DS_00108110 = 0 so the decrement goes negative (0x42E37); slot 0's think
 * gate DS_00107813 != 1 (0x42E65) takes the voice arm. DS_00104B1D = 1
 * ends it at the hook store (0x42ED9). */
static void vs_challenge_poll(void)
{
    vs_pools();
    DSD(DS_00104AD4) = 0u;
    DSD(DS_001088E4) = 0u;
    DSB(DS_00105C04) = 0u;
    DSB(DS_00104B1F) = 0u;
    DSW(DS_000EF6DC) = 0x40u;
    DSB(DS_00108110) = 0u;
    DSB(DS_00107813) = 0u;
    DSB(DS_00104B1D) = 1u;
    flow_challenge_poll();
}
/* Row 165: 0x424E8 case 0, check_mode_13_c's (j) arm: DS_00104AD4 = 3
 * (0x428B8 takes none of its three arms and writes through DS_001080F8,
 * here a zeroed scratch record), DS_00104B1D = DS_00104B1F = 3 (the side
 * tail draws nothing). */
static void vs_mode13_0(void)
{
    vs_pools();
    mem_fill(VS2_R, 0, 0x100u);
    DSD(DS_001080F8) = VS2_R;
    DSD(DS_00104AD4) = 3u;
    DSB(DS_00104B1F) = 3u;
    DSB(DS_00104B1D) = 3u;
    DSB(DS_00104B25) = 0u;
    game_mode_13_step();
}
/* Row 169: 0x277C0 with the countdown at 1 (0x27811 fires at 0), m0f_seed's
 * recipe: the winner side DS_0010810D = 0, the result side DS_00104AD4 = 0
 * whose DS_001077A8 entry is a scratch record (0x41310 writes its +0x3C). */
static void vs_mode0f(void)
{
    (void)tf_demo_fixture();
    mem_fill(VS2_R, 0, 0x100u);
    DSD(DS_001077A8) = VS2_R;
    DSB(VS_810D) = 0u;
    DSD(DS_00104AD4) = 0u;
    DSW(DS_00104AFE) = 1u;
    game_mode_0f_step();
}
/* Rows 180/181/184..187: 0x1EEB0 states 5, 8 and 0xB..0xE with the entry
 * finished: the screen reset (0x1ED2C: the three cursor actors, every cell
 * idle, so 0x1FFD0 leaves DS_001044AC = 0), the timer DS_0010438C = 0
 * (0x1F4AD), a rank (2) and three letters for 0x20710, side characters 0. */
static void vs_mode1e_done(u8 state)
{
    ra_env();
    nameentry_reset();
    DSW(DS_0010438C) = 0u;
    DSW(DS_001044D6) = 2u;
    DSB(RA_LIM) = 3u;
    DSB(DS_0010782A) = 0u;
    DSB(DS_0010782A + 0x94u) = 0u;
    DSB(DS_00104B25) = state;
    game_mode_1e_step();
}
static void vs_mode1e_5(void) { vs_mode1e_done(5u); }
static void vs_mode1e_8(void) { vs_mode1e_done(8u); }
/* Rows 186/187: the four raw pairs of the one port path (jump table 0x1EE6C:
 * state 0xB 0x1EF44, 0xC 0x1F034, 0xD 0x1F0C1, 0xE 0x1EFD3), each an E3 call
 * then an E2 call: B 0x1EF8D/0x1EF97, C 0x1F07F/0x1F089, D 0x1F109/0x1F113,
 * E 0x1F01F/0x1F029. Row 186's driver runs B and C, row 187's D and E. */
static void vs_mode1e_bc(void) { vs_mode1e_done(0x0Bu); vs_mode1e_done(0x0Cu); }
static void vs_mode1e_de(void) { vs_mode1e_done(0x0Du); vs_mode1e_done(0x0Eu); }
/* Rows 188/189: states 0xF/0x10, ra_check_rearm's recipe (the other side's
 * score qualifies; the priming 0x1F458 keeps running). */
static void vs_mode1e_0f(void)
{
    ra_env();
    DSB(DS_00104B25) = 0x0Fu;
    DSD(DS_001077EC) = 999999u;
    DSD(DS_00107880) = 450000u;
    game_mode_1e_step();
}
static void vs_mode1e_10(void)
{
    ra_env();
    DSB(DS_00104B25) = 0x10u;
    DSD(DS_001077EC) = 150000u;
    DSD(DS_00107880) = 999999u;
    game_mode_1e_step();
}
/* Row 190: 0x208F8 state 0, m1f_seed's recipe: side 0, character 0, the
 * palette-acquire table empty before the backdrop row's spawn. */
static void vs_mode1f_0(void)
{
    vs_pools();
    DSD(DS_00104AD4) = 0u;
    DSB(DS_0010782A) = 0u;
    mem_fill(DS_00107608, 0, 0x190u);
    DSB(DS_00104B25) = 0u;
    game_mode_1f_step();
}

/* Record k7-k12 §6, batch B2 (flow.c and attract.c): pure-state voices, all
 * case 1 or 5. The ids are the wired voices on each path in raw order, batch
 * A's included (rows 167/177/179/183/192 share batch A's drivers). */
#define K12_B2_GAME_ROWS 18
static const TfVoiceSite k12_b2_game[] = {
    {  13u, vs_attract_4,      2u, { 0x54u, 0x56u } },                /* 0x111FF */
    { 164u, vs_challenge_poll, 1u, { 0x2Du } },                       /* 0x42E7F */
    { 165u, vs_mode13_0,       1u, { 0x2Cu } },                       /* 0x4250D */
    { 167u, vs_hook_25ae8,     2u, { 0x100u, 0x3Du } },               /* 0x25B51 */
    { 169u, vs_mode0f,         1u, { 0x2Bu } },                       /* 0x27821 */
    { 177u, vs_mode1e_0,       2u, { 0x100u, 0xE1u } },               /* 0x1EEFE */
    { 179u, vs_mode1e_4,       2u, { 0x100u, 0xE1u } },               /* 0x1F1F6 */
    { 180u, vs_mode1e_5,       2u, { 0xE3u, 0xE2u } },                /* 0x1F249 */
    { 181u, vs_mode1e_5,       2u, { 0xE3u, 0xE2u } },                /* 0x1F253 */
    { 183u, vs_mode1e_7,       2u, { 0x100u, 0xE1u } },               /* 0x1F311 */
    { 184u, vs_mode1e_8,       2u, { 0xE3u, 0xE2u } },                /* 0x1F36C */
    { 185u, vs_mode1e_8,       2u, { 0xE3u, 0xE2u } },                /* 0x1F376 */
    { 186u, vs_mode1e_bc,      4u, { 0xE3u, 0xE2u, 0xE3u, 0xE2u } },  /* E3: 0x1EF8D (B) 0x1F07F (C) 0x1F109 (D) 0x1F01F (E) */
    { 187u, vs_mode1e_de,      4u, { 0xE3u, 0xE2u, 0xE3u, 0xE2u } },  /* E2: 0x1EF97 (B) 0x1F089 (C) 0x1F113 (D) 0x1F029 (E) */
    { 188u, vs_mode1e_0f,      1u, { 0xE1u } },                       /* 0x1EFA9 */
    { 189u, vs_mode1e_10,      1u, { 0xE1u } },                       /* 0x1F099 */
    { 190u, vs_mode1f_0,       1u, { 0x3Bu } },                       /* 0x20955 */
    { 192u, vs_mode22,         2u, { 0x29u, 0x22u } },                /* 0x26DB8 */
};

/* Rows 6/7: 0x10F28 with both countdowns at 1, so both expire in one call
 * (signed `<= 0` after the decrement, 0x10F3C/0x10F6F `jg`). 0xBD comes
 * before the rng_next(0x2D) reload (0x10F43, 0x10F4D); the second arm's
 * rng_next(2) picks 0xBE when non-zero, else 0xBF (0x10F76..0x10F86). Seed
 * 0xABCD draws 6 then 1 (0xBE); seed 3 draws 17 then 0 (0xBF). */
static void vc_voice_tick(u32 seed)
{
    DSW(DS_000F0A60) = 1u;
    DSW(DS_000F0A62) = 1u;
    rng_seed(seed);
    attract_voice_tick();
}
static void vc_voice_tick_be(void) { vc_voice_tick(0xABCDu); }
static void vc_voice_tick_bf(void) { vc_voice_tick(3u); }

/* Record k7-k12 §7, batch C (attract.c): the attract's sample voices, case 2
 * (the runner's DS_001028C8 = 0 logs them without a bank read). */
#define K12_C_GAME_ROWS 2
static const TfVoiceSite k12_c_game[] = {
    {   6u, vc_voice_tick_be, 2u, { 0xBDu, 0xBEu } },             /* 0x10F43 */
    {   7u, vc_voice_tick_bf, 2u, { 0xBDu, 0xBFu } },             /* 0x10F8B */
};

/* ---- record k7-k12 §11: batch D4, the name-entry and flow samples -------- */

#define VS4_DESC  0x000A7E44u   /* the name-entry marker descriptor (0x1EE20's) */

static u32 vs4_actor(void)
{
    return actor_spawn((const u32 *)(mem + VS4_DESC), 0x1111u, 0xFFu, 0x2222u, 0u);
}

/* The name-entry screen as 0x1ED2C leaves it (the three cursor actors, every
 * cell idle, DS_001044D8/C8/BC = 0, the cursor at column 0xB, row 6), with no
 * pad bit and no autorepeat, the timer running (DS_0010438C = 500, so
 * 0x1F4AD does not finish), rank 3, no letter typed of 3, the name at row 4
 * column 0x24, DS_00104529 = 0 (the text arms) and no blink frame. */
static void vs4_ne_env(void)
{
    vs_pools();
    game_string_table_load("data/game/C");
    nameentry_reset();
    DSB(RA_PAD0) = 0u;
    DSB(RA_PAD1) = 0u;
    DSD(RA_REP_H) = 0u;
    DSD(RA_REP_V) = 0u;
    DSW(DS_0010438C) = 500u;
    DSW(DS_001044D6) = 3u;
    DSB(RA_CNT) = 0u;
    DSB(RA_LIM) = 3u;
    DSB(VS_B29) = 0u;
    DSW(DS_000EF6DC) = 0x21u;
    DSD(DS_001044C4) = 4u;
    DSD(DS_001044CC) = 0x24u;
}

/* One letter cell (stride 0x14 at DS_00104114): +0 handle, +4 x, +8 y, +0xC
 * vel, +0xE acc, +0x10 target, +0x12 state, +0x13 letter. */
static void vs4_cell(u32 i, u32 h, u32 x, u32 y, u32 vel, u32 acc, u32 tgt,
                     u32 st, u32 ch)
{
    u32 c = DS_00104114 + 0x14u * i;
    DSD(c) = h;
    DSD(c + 4u) = x;
    DSD(c + 8u) = y;
    DSW(c + 0xCu) = (u16)vel;
    DSW(c + 0xEu) = (u16)acc;
    DSW(c + 0x10u) = (u16)tgt;
    DSB(c + 0x12u) = (u8)st;
    DSB(c + 0x13u) = (u8)ch;
}

/* Rows 198..205: 0x1FFD0 with one cell (two for row 201) in the state. */
static void vs4_cells_1(void)                  /* 0x20036: spawn */
{
    vs4_ne_env();
    vs4_cell(3u, 0x77u, 0x11u, 0x22u, 0x33u, 0x44u, 0x55u, 1u, 5u);
    nameentry_cells_step();
}
static void vs4_cells_2(void)                  /* 0x200F7: y 0x1F00 + 0x11E past 0x2000 */
{
    vs4_ne_env();
    vs4_cell(2u, vs4_actor(), 0x77u, 0x1F00u, 0xFEu, 0x20u, 0x2000u, 2u, 1u);
    nameentry_cells_step();
}
static void vs4_cells_3(void)                  /* 0x20183 on cell 1 (odd), then cell 2 (even) */
{
    vs4_ne_env();
    vs4_cell(1u, vs4_actor(), 0x77u, 0x1FF1u, 0x10u, 0x20u, 0x2000u, 3u, 1u);
    vs4_cell(2u, vs4_actor(), 0x77u, 0x1FF1u, 0x10u, 0x20u, 0x2000u, 3u, 1u);
    nameentry_cells_step();
}
static void vs4_cells_5(void)                  /* 0x2027D: x 0x5010 - 0x22 below 0x5000 */
{
    vs4_ne_env();
    vs4_cell(1u, vs4_actor(), 0x5010u, 0x77u, 0xFFDEu, 0x77u, 0x5000u, 5u, 1u);
    nameentry_cells_step();
}
static void vs4_cells_8(void)                  /* 0x203E0: the DEL letter 0x1B */
{
    vs4_ne_env();
    vs4_cell(4u, 0x77u, 0x77u, 0x77u, 0x77u, 0x77u, 0x77u, 8u, 0x1Bu);
    nameentry_cells_step();
}
static void vs4_cells_9(void)                  /* 0x20487 */
{
    vs4_ne_env();
    vs4_cell(7u, vs4_actor(), 0x1000u, 0x2000u, 0x77u, 0x77u, 0x77u, 9u, 1u);
    nameentry_cells_step();
}

/* Rows 206..217: 0x1F458 side 0 with one direction bit on the cursor
 * (column, row); 0x1FCF9 then returns (no face button, no queued letter). */
static void vs4_ne_dir(u8 mask, u16 col, u16 row)
{
    vs4_ne_env();
    DSW(RA_COL) = col;
    DSW(RA_ROW) = row;
    DSB(RA_PAD0) = mask;
    (void)nameentry_step(0u);
}
/* right: 0xB + 3 within 0x1D (0x34); 0x1D + 3 past 0x1D on row 6
 * (0x1F580) and 0x20 + 3 past 0x20 on row 0xF (0x1F560), both 0x39. */
static void vs4_ne_right_in(void)   { vs4_ne_dir(0x10u, 0xBu, 6u); }
static void vs4_ne_right_wrap(void)
{
    vs4_ne_dir(0x10u, 0x1Du, 6u);
    vs4_ne_dir(0x10u, 0x20u, 0xFu);
}
/* left: 0xB - 3 below 0xB (0x35); 0xE - 3 not below (0x38). */
static void vs4_ne_left_wrap(void)  { vs4_ne_dir(0x20u, 0xBu, 6u); }
static void vs4_ne_left_in(void)    { vs4_ne_dir(0x20u, 0xEu, 6u); }
/* up (0x80) and down (0x40): on the END column 0x20 (0x39 / 0x38), else
 * 0x37 / 0x36. */
static void vs4_ne_up_end(void)     { vs4_ne_dir(0x80u, 0x20u, 0xFu); }
static void vs4_ne_up(void)         { vs4_ne_dir(0x80u, 0xBu, 9u); }
static void vs4_ne_down_end(void)   { vs4_ne_dir(0x40u, 0x20u, 0xCu); }
static void vs4_ne_down(void)       { vs4_ne_dir(0x40u, 0xBu, 6u); }
/* Row 218: a face button (pad bit 0) picks the letter under the cursor
 * (column 0xB, row 6: 'A'), count 0 below the limit 3 (0x1FEA3). */
static void vs4_ne_pick(void)       { vs4_ne_dir(0x01u, 0xBu, 6u); }

/* Mode 0x12 (0x41C28): the seven portrait actors DS_001080C0[0..6] and the
 * background record DS_001080F4, all live pool records. */
static void vs4_mode12(u8 state)
{
    u32 k;
    vs_pools();
    game_string_table_load("data/game/C");
    for (k = 0; k < 7u; k++) DSD(DS_001080C0 + k * 4u) = vs4_actor();
    DSD(DS_001080F4) = vs4_actor();
    DSB(VS_B29) = 0u;
    DSD(DS_00104AD4) = 0u;
    DSB(DS_0010782A) = 0u;
    DSW(DS_00104AFC) = 0u;
    DSB(DS_00104B25) = state;
}
/* Row 131: state 1, stage 0 marked (DS_00108106 bit 7), not the current
 * stage 7, the counter n = 3 before its increment: 0x34 + 3. */
static void vs4_mode12_1(void)
{
    vs4_mode12(1u);
    DSB(DS_0010810F) = 0u;
    DSW(DS_00104AFC) = 7u;
    mem_fill(DS_00108106, 0, 7u);
    DSB(DS_00108106) = 0x80u;
    DSB(DS_00108112) = 3u;
    DSD(DS_00104AD4) = 2u;
    game_mode_12_step();
}
/* Row 132: state 3 with the background at y 0x2300 (0x41E7D). */
static void vs4_mode12_3(void)
{
    u32 bg;
    vs4_mode12(3u);
    bg = DSD(DS_001080F4);
    DSW(bg + 0x36u) = 0u;
    DSD(bg + 0x1Cu) = 0x2300u;
    game_mode_12_step();
}
/* How many times `id` is in the voice log since the runner's reset. */
static int vs4_log_count(u32 id)
{
    u32 i, n = sound_voice_log_count();
    int k = 0;
    for (i = 0; i < n; i++)
        if (sound_voice_log_at(i) == id) k++;
    return k;
}
/* Row 133: state 4, first DS_0010810E 6 -> 7 (0x420B0 `jne`: no 0xC7; the
 * pass ran: the count is 7 and the state 8), then 7 -> 8 (0x420AD). */
static void vs4_mode12_4(void)
{
    vs4_mode12(4u);
    DSB(DS_0010810E) = 6u;
    game_mode_12_step();
    CHECK_EQ_INT((int)DSB(DS_0010810E), 7);
    CHECK_EQ_INT((int)DSB(DS_00104B25), 8);
    CHECK_EQ_INT(vs4_log_count(0xC7u), 0);
    DSB(DS_00104B25) = 4u;
    DSB(DS_0010810E) = 7u;
    game_mode_12_step();
    /* Record k7-k12 §12.0: 8 -> 9 posts nothing (0x420AD `cmp eax,8; jne`
     * is an equality, not a `>= 8`): the one 0xC7 stays the 7 -> 8 pass's. */
    CHECK_EQ_INT(vs4_log_count(0xC7u), 1);
    DSB(DS_00104B25) = 4u;
    DSB(DS_0010810E) = 8u;
    game_mode_12_step();
    CHECK_EQ_INT((int)DSB(DS_0010810E), 9);
    CHECK_EQ_INT(vs4_log_count(0xC7u), 1);
}
/* Row 135: state 6, the background +0x2C = 0x1C0 so the step is 1
 * (0x42183..0x4219D), cycle byte 1. */
static void vs4_mode12_6(void)
{
    vs4_mode12(6u);
    DSW(DSD(DS_001080F4) + 0x2Cu) = 0x1C0u;
    DSB(DS_00107832) = 1u;
    game_mode_12_step();
}

/* The fight fixture with the private pool and the HUD bar records
 * DS_001028F0/F8[side] live (0x1D764 re-seeks them). */
static void vs4_fight(void)
{
    u32 s;
    (void)tf_demo_fixture();
    vs_pools();
    game_string_table_load("data/game/C");
    for (s = 0; s < 2u; s++) {
        DSD(DS_001028F0 + s * 4u) = vs4_actor();
        DSD(DS_001028F8 + s * 4u) = vs4_actor();
        DSW(DS_001077B0 + s * 0x94u + 0x8Cu) = 0u;
    }
    DSB(VS_B29) = 0u;
}
/* Row 145: 0x272DC with the winner slot 0 at 0x78 (0x27303). */
static void vs4_arena_ko(void)
{
    vs4_fight();
    DSB(VS_810D) = 0u;
    DSB(DS_0010780A) = 0x78u;
    flow_arena_ko_check();
}
/* Rows 154/155: modes 5 and 0x30, state 2. Mode 5 runs twice:
 * DS_00104B14 = 0 (0xD7), then 1 (0xD9). */
static void vs4_mode05_2(void)
{
    vs4_fight();
    DSB(DS_00104B14) = 0u;
    DSB(DS_00104B25) = 2u;
    game_mode_05_step();
    DSB(DS_00104B14) = 1u;
    DSB(DS_00104B25) = 2u;
    game_mode_05_step();
}
static void vs4_mode30_2(void)
{
    vs4_fight();
    DSB(DS_00104B14) = 0u;
    DSB(DS_00104B25) = 2u;
    game_mode_30_step();
}
/* Row 161: 0x4F4E8 on a tick multiple of DS_001088D0 = 1. First the
 * countdown byte DS_001088F2 = 0xB (above 10, 0x4F575 `jg`: no 0x52; the
 * pass ran: the byte is decremented to 0xA), then 5 (non-zero, <= 10). */
static void vs4_round_timer(void)
{
    vs_pools();
    game_string_table_load("data/game/C");
    DSB(DS_00105B3B) = 0u;
    DSD(DS_001088D0) = 1u;
    DSW(DS_00104AF4) = 7u;
    DSB(DS_001088F2) = 0x0Bu;
    flow_round_timer_step();
    CHECK_EQ_INT((int)DSB(DS_001088F2), 0x0A);
    CHECK_EQ_INT(vs4_log_count(0x52u), 0);
    DSB(DS_001088F2) = 5u;
    flow_round_timer_step();
    /* Record k7-k12 §12.0: the boundary 0xA (0x4F572 `cmp edx,0xa; jg`: 10 is
     * not above 10) and 0x80 (0x4F56F `sar edx,0x18`: -128, signed) post
     * 0x52 too. */
    CHECK_EQ_INT(vs4_log_count(0x52u), 1);
    DSB(DS_001088F2) = 0x0Au;
    flow_round_timer_step();
    CHECK_EQ_INT(vs4_log_count(0x52u), 2);
    DSB(DS_001088F2) = 0x80u;
    flow_round_timer_step();
    CHECK_EQ_INT(vs4_log_count(0x52u), 3);
}
/* Rows 162/163: 0x27FA8, outside mode 0xB, B1E == ADC, the win counts
 * equal. Row 162: both sides at 0x78 (a tie: 0x27C48 leaves DS_00104AD4 =
 * 2). Row 163: both at 0 with DS_001088F2 = 0 (0x28054), |A - B| = 0. */
static void vs4_round_end(u8 hp, u8 f2)
{
    vs4_fight();
    DSB(DS_0010780A) = hp;
    DSB(DS_0010789E) = hp;
    DSB(DS_001088F2) = f2;
    DSW(DS_00104B00) = 4u;
    DSB(DS_00104B1D) = 3u;
    DSB(DS_00104B1E) = 3u;
    DSD(DS_00104ADC) = 3u;
    DSB(DS_00104AF2) = 0u;
    DSB(DS_00104AF3) = 0u;
    DSB(DS_0010782A) = 0u;
    DSB(DS_001078BE) = 0u;
    flow_round_end_check();
}
static void vs4_round_end_ko(void)   { vs4_round_end(0x78u, 0x40u); }
static void vs4_round_end_time(void) { vs4_round_end(0u, 0u); }
/* Row 168: 0x28BD4, both +0x54 bytes 0 (0x28C0E..0x28C1E), m0a_seed's
 * fixture. */
static void vs4_mode0a(void)
{
    (void)tf_demo_fixture();
    DSD(DS_00104B00) = 0xAu;
    DSB(DS_00107804) = 0u;
    DSB(DS_00107898) = 0u;
    game_mode_0a_step();
}
/* Row 193: mode 0x23 state 2 (0x26AD6). */
static void vs4_mode23_2(void)
{
    vs_pools();
    DSB(VS_B29) = 0u;
    DSB(DS_00104B25) = 2u;
    game_mode_23_step();
}

/* Record k7-k12 §11, batch D4: 33 wiring points, all case 2 (0x4D case 3),
 * reached only in real play. One entry per wiring point; rows on one path
 * share a driver and list each other's ids. */
#define K12_D4_ROWS 33
static const TfVoiceSite k12_d4[] = {
    { 131u, vs4_mode12_1,       1u, { 0x37u } },                  /* 0x41D2B */
    { 132u, vs4_mode12_3,       1u, { 0x3Au } },                  /* 0x4207F */
    { 133u, vs4_mode12_4,       1u, { 0xC7u } },                  /* 0x420B7 */
    { 135u, vs4_mode12_6,       1u, { 0xBCu } },                  /* 0x42229 */
    { 145u, vs4_arena_ko,       1u, { 0xD3u } },                  /* 0x2730A */
    { 154u, vs4_mode05_2,       2u, { 0xD7u, 0xD9u } },           /* 0x25E39 */
    { 155u, vs4_mode30_2,       1u, { 0xD7u } },                  /* 0x294D5 */
    { 161u, vs4_round_timer,    1u, { 0x52u } },                  /* 0x4F581 */
    { 162u, vs4_round_end_ko,   1u, { 0xD3u } },                  /* 0x27FF9 */
    { 163u, vs4_round_end_time, 1u, { 0xD3u } },                  /* 0x2805F */
    { 168u, vs4_mode0a,         1u, { 0xD8u } },                  /* 0x28C2A */
    { 193u, vs4_mode23_2,       1u, { 0x60u } },                  /* 0x26B32 */
    { 198u, vs4_cells_1,        2u, { 0xB0u, 0x7Bu } },           /* 0x200B5 */
    { 199u, vs4_cells_1,        2u, { 0xB0u, 0x7Bu } },           /* 0x200BF */
    { 200u, vs4_cells_2,        1u, { 0x71u } },                  /* 0x2012D */
    { 201u, vs4_cells_3,        2u, { 0xE7u, 0xE8u } },           /* 0x201D2 */
    { 202u, vs4_cells_5,        2u, { 0x70u, 0x4Du } },           /* 0x202C5 */
    { 203u, vs4_cells_5,        2u, { 0x70u, 0x4Du } },           /* 0x202CF */
    { 204u, vs4_cells_8,        1u, { 0xE9u } },                  /* 0x2043F */
    { 205u, vs4_cells_9,        1u, { 0xE9u } },                  /* 0x204B5 */
    { 206u, vs4_ne_right_in,    2u, { 0xE6u, 0x34u } },           /* 0x1F52B */
    { 207u, vs4_ne_right_wrap,  4u, { 0xE6u, 0x39u, 0xE6u, 0x39u } }, /* 0x1F593 */
    { 208u, vs4_ne_right_in,    2u, { 0xE6u, 0x34u } },           /* 0x1F593 */
    { 209u, vs4_ne_left_in,     2u, { 0xE6u, 0x38u } },           /* 0x1F5FD */
    { 210u, vs4_ne_left_wrap,   2u, { 0xE6u, 0x35u } },           /* 0x1F644 */
    { 211u, vs4_ne_left_in,     2u, { 0xE6u, 0x38u } },           /* 0x1F644 */
    { 212u, vs4_ne_up,          2u, { 0xE6u, 0x37u } },           /* 0x1F6AE */
    { 213u, vs4_ne_up_end,      2u, { 0xE6u, 0x39u } },           /* 0x1F6E8 */
    { 214u, vs4_ne_up,          2u, { 0xE6u, 0x37u } },           /* 0x1F70A */
    { 215u, vs4_ne_down,        2u, { 0xE6u, 0x36u } },           /* 0x1F75F */
    { 216u, vs4_ne_down_end,    2u, { 0xE6u, 0x38u } },           /* 0x1F794 */
    { 217u, vs4_ne_down,        2u, { 0xE6u, 0x36u } },           /* 0x1F7B5 */
    { 218u, vs4_ne_pick,        1u, { 0xE9u } },                  /* 0x1FF31 */
};

int test_voice_sites(void)
{
    int before = g_failures;
    /* 22 wiring points, one entry each (rows 139/140 share one path). */
    CHECK_EQ_INT((int)(sizeof k12_a / sizeof k12_a[0]), K12_A_ROWS);
    tf_voice_sites(k12_a, (u32)(sizeof k12_a / sizeof k12_a[0]));
    CHECK_EQ_INT((int)(sizeof k12_b1 / sizeof k12_b1[0]), K12_B1_ROWS);
    tf_voice_sites(k12_b1, (u32)(sizeof k12_b1 / sizeof k12_b1[0]));
    vs_result_gate_check();
    vs_setne_check();
    CHECK_EQ_INT((int)(sizeof k12_b2_game / sizeof k12_b2_game[0]), K12_B2_GAME_ROWS);
    tf_voice_sites(k12_b2_game, (u32)(sizeof k12_b2_game / sizeof k12_b2_game[0]));
    CHECK_EQ_INT((int)(sizeof k12_c_game / sizeof k12_c_game[0]), K12_C_GAME_ROWS);
    tf_voice_sites(k12_c_game, (u32)(sizeof k12_c_game / sizeof k12_c_game[0]));
    CHECK_EQ_INT((int)(sizeof k12_d4 / sizeof k12_d4[0]), K12_D4_ROWS);
    tf_voice_sites(k12_d4, (u32)(sizeof k12_d4 / sizeof k12_d4[0]));
    return g_failures - before;
}
