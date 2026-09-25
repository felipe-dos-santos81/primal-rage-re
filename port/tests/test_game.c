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
#include "platform/render.h"
#include "platform/res.h"
#include "test.h"
#include "game/effects.h"
#include "test_fixtures.h"
#include "game/config.h"
#include "game/fight.h"
#include "game/attract.h"
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

    /* A mode other than 3 must not run the state machine at all. */
    DSD(DS_00104B00) = 7;
    game_frame();

    check_title_overlay();

    /* Task 11: the init chain's audio calls and the title state's music request
     * drive the sequencer with no device open (the suite never opens one).
     * The title state above asked for music; the master-loop service inherits
     * that request, loads the S16TITLE bank and ticks it. */
    game_set_game_dir("data/game/C");
    game_audio_init();
    /* The init chain must load the FM patch bank (FAT.OPL): the sequencer maps
     * every program change through it, and without it a key-on carries no
     * operator setup, so the OPL core renders silence for the whole run (the
     * live title was silent until this load existed). This test runs before the
     * audio tests, so the count is 0 here unless the init path loaded it. */
    CHECK_EQ_INT(patches_count(), 181);
    CHECK_EQ_INT((int)game_audio_ticks(), 0);
    /* Task 12: the title state queued the announcer sample (S16SOUND.GRA's
     * RIFF/WAVE blob) through the game's own request path; the master loop's
     * audio service (0x1CF20 -> 0x1CB18) must play it. The title bank's first
     * note is at XMIDI tick 59, so this single-tick render is before the FM
     * sounds and any non-silence is the sample's, not the music's. */
    host_wait_vblank();
    game_audio_service();
    CHECK(mixer_active_voices() > 0,
          "announcer sample became an active mixer voice");
    {
        static s16 abuf[4096 * 2];
        mixer_render(abuf, 4096, MIXER_OPL_RATE);
        int nz = 0;
        for (int i = 0; i < 4096 * 2; i++) if (abuf[i]) { nz = 1; break; }
        CHECK(nz, "mixer rendered non-silence with the announcer sample active");
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
    game_shutdown();                         /* release handles for later tests */
    CHECK_EQ_INT((int)DSB(DS_000A2CB1), 0);  /* teardown clears the enable flag */

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

    /* A fresh reset frees every record and leaves both lists empty. */
    actors_reset();
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

    return g_failures - before;
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
     * shipped masks. An accepted coin debits one credit through 0x2CA7C and
     * returns early, so the frame's state dispatch is skipped; a rejected poll
     * leaves the credit alone and runs the dispatch (state 9's countdown is the
     * observable that the dispatch ran or was skipped). */
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
        DSD(DS_001088E4) = 0u;
        DSW(DS_000F0A64) = 9; DSW(DS_000F0A6A) = 2; DSW(DS_000F0A6C) = 3;
        game_state_step();
        CHECK_EQ_INT((int)DSD(DS_00105C00), 5);
        CHECK_EQ_INT((int)DSW(DS_000F0A6A), 1);

        /* Accept event 0: one credit debited, dispatch skipped (countdown held). */
        DSD(DS_001088E4) = DSD(DS_0009ACBC);
        DSW(DS_000F0A6A) = 2;
        game_state_step();
        CHECK_EQ_INT((int)DSD(DS_00105C00), 4);
        CHECK_EQ_INT((int)DSW(DS_000F0A6A), 2);

        /* Accept event 1: likewise. */
        DSD(DS_001088E4) = DSD(DS_0009ACBC + 4u);
        game_state_step();
        CHECK_EQ_INT((int)DSD(DS_00105C00), 3);

        DSD(DS_00105C00) = saved_c00; DSD(DS_001088E4) = saved_e4;
        DSB(DS_00104B1D) = saved_1d;  DSB(DS_00104B1F) = saved_1f;
        DSB(DS_00105D60) = saved_60;  DSD(DS_001082DC) = saved_dc;
        DSW(DS_000F0A64) = saved_64;  DSW(DS_000F0A6A) = saved_6a;
        DSW(DS_000F0A6C) = saved_6c;
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

    /* Task 8: the effect call sites 0x29B74 (the DS_00104AE4 mode-0x17 handler)
     * and 0x41578 are deferred, not shipped. The raw reaches them only through
     * 0x24C5C's unported mode cases (0x12 and 0x16..0x1b) and the unported
     * match/fight chain; the port's DS_00104B00 is fixed at 3 by 0x10E80, so
     * wiring either would be a dispatch path nothing can reach (UNOWNED BY THIS
     * PLAN; see port/spec/game_flow.md). This pins that no unreachable handler
     * is registered: registering FN_00029B74 or FN_00041578 fails it. The
     * behavioral half (the ported state machine never arms DS_00104AE4 and
     * never leaves mode 3) is asserted in the state-5 block below. */
    {
        CHECK(fn_resolve(FN_00029B74) == NULL,
              "0x29B74 is deferred, not registered");
        CHECK(fn_resolve(FN_00041578) == NULL,
              "0x41578 is deferred, not registered");
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
        /* Task 8: the ported state-5 path does not arm the deferred 0x29B74
         * handler and does not leave mode 3 (so the six call dword [0x104ae4]
         * sites and the four 0x41578 sites stay unreachable). */
        CHECK(DSD(DS_00104AE4) == 0xDEADBEEFu,
              "state 5 does not arm the deferred 0x29B74 handler");
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
        CHECK_EQ_INT((int)DSD(DS_00107498 + 12u), 0);

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
     * The 2000-iteration loop and the 1400-frame cap are sized from the demo's
     * state-7 exit: state 3 is entered at loop frame 589, state 6 runs at loop
     * frame 1070 (dumped frame 481), and 0x11BCC's timer exit runs at loop frame
     * 1970, where the state drops to 0 and dumping stops. So the state>=3 dump
     * run is loop frames 589..1969, i.e. dumped frames 0..1380 (1381 frames); the
     * 1400 cap covers it and the 2000-frame loop clears the 1970 exit. The
     * front-end window is distinct [560..890] (331 frames: 142 clean, 185
     * splice, 0 transition, 2 unexplained; [560..842]/283 before the
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
     * 0x3E4E4 leap explained 880..890). The
     * [557..810]/254 text here was
     * stale drift, already flagged in Task 2's review and corrected here; Task
     * 3c's camera-offset fix does not touch it. The arena-backdrop fix (the
     * crowd actor 0's mountain layer, actor_spawn's per-type dispatch) extended
     * the window from [560..830]: the window is derived from the port's own
     * dump, so explaining captures 834..842 at 0 bytes necessarily grows it,
     * and the two new unexplained frames (832, 833) are pre-existing,
     * out-of-scope gaps with named owners, allowed by name in
     * tools/title_compare.py. The window's exhibition set spans port frames
     * 0..530 (294 exhibited). */
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
         * (0x3AAFC -> the 0x3A504/0x3A650/0x3A79C/0x3A8E8 pose family), but the
         * port cannot: 0x19020 (fighter_pass_a's per-slot hook, fighter.c:287)
         * is unported, so DS_00100AF8/AFC stay 0 and fighter_pass_a's tail never
         * runs 0x193B0 -> 0x3B714 -> 0x3AAFC. The divergence is a subsystem (68
         * new funcs / 10467 B, Task 4's derivation record §10.4), so the
         * pose state is a named gap, not a fitted value. Measured so the next
         * task can see it, exactly as s7_hit is. */
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
        for (int i = 0; i < 2000; i++) {
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
            game_loop();
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
            if (DSW(DS_000F0A64) == 3u) reached3 = 1;
            if (DSB(DS_000F0A6E) < 6u) seen_entries |= 1u << DSB(DS_000F0A6E);
            if (DSW(DS_000F0A64) == 7u) {
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
                /* Hash the presented index buffer without reading pixels. */
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
            if (DSW(DS_000F0A64) >= 3u && !dump_failed &&
                dumped < (int)raw_cap) {
                const u8 *fb = gfx_display();
                if (fb == NULL) fb = mem + DSD(DS_000E87A0);
                char path[1300];
                snprintf(path, sizeof path, "%s/frame_%04d.raw", dump, dumped);
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
                CHECK(ok, "front-end frame writes to the dump");
                if (ok) dumped++;
                else dump_failed = 1;
            }
        }
        if (log != NULL) fclose(log);

        /* The raw's timeline: six entries, each drawn then paused, then state 3.
         * Entry k is drawn on frame 1+93k; after the sixth, 0x1E pause frames
         * and the phase-3 handoff land state 3 on frame 589 (the arithmetic is
         * in docs/superpowers/plans/2026-09-19-frontend-input-derivations.md).
         * State 3 now runs its 0x12484 phases and hands off to state 9, so the
         * window is asserted to reach state 3, not to end in it; the state it
         * ends in is whatever 0x12658's actor timing produces. The demo adds
         * states 9/6/7: state 7 runs its 900-frame timer and 0x11BCC's exit
         * lands at loop frame 1970, so the state>=3 dump window is loop frames
         * 589..1969 (1381 frames, dump 0..1380). The 1400 cap covers it; the
         * 2000-frame loop clears the 1970 exit. */
        CHECK_EQ_INT((int)seen_entries, 0x3F);
        CHECK(reached3, "the window reaches state 3");
        CHECK_EQ_INT(dumped, (int)(raw_cap < 1381 ? raw_cap : 1381));

        /* Alignment: the driver's state-6 entry sits at the attract's
         * post-state, and the two picks drawn from it (plus the dust builder's
         * six intermediate draws) are the capture's characters, 0xC835A[0] = 0
         * and 0xC835A[3] = 3. A reverted re-seed leaves entry_lcg at the seed
         * (or 0); a dropped dust draw or a restored master-loop draw changes
         * the characters. */
        CHECK(seen6, "the driver reaches state 6");
        CHECK_EQ_INT((int)entry_lcg, (int)FRONTEND_RNG_AFTER_ATTRACT);
        CHECK_EQ_INT((int)DSB(DS_0010816A), 0);
        CHECK_EQ_INT((int)DSB(DS_0010816A + 1u), 3);
        /* Record §7.1: the seed's consequence — the T-rex's variant is 1 and
         * its handle 0x1BB9FCD8 (0xA8A28[1]). The old 0xFF seed left variant 0
         * and 0x1BB9FD58, so this fails under that mutation. */
        CHECK_EQ_INT((int)DSB(DS_00105B34), 1);
        CHECK_EQ_INT((int)DSD(0xA8A28u + DSB(DS_00105B34) * 4u), 0x1BB9FCD8);
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
         * 0x19020 -> 0x193B0 -> 0x3B714 -> 0x3AAFC -> pose-family chain, which
         * the port cannot run (0x19020 is unported, so DS_00100AF8/AFC stay 0).
         * Task 1 §3.3's "the animation cursor differs" is stale: the port's
         * raptor cursor now matches (0xD2316 at the 9/8 entry) and its
         * silhouette matches capture 834. The pose state is measured by
         * s7_saw10/s7_saw0a and is a named gap (Task 4 record §10). */
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
         * (or never leaves) fails. The dump count above (1381) is the same
         * proof through the presented frames. */
        CHECK_EQ_INT(s7_last, 1969);
        /* Demo record §15: the grey flier of captures 864/865 is 0x1282C's
         * type-0x01 spawn. It is refused unless state 6's 0x20DF4 has built
         * the 0xF0A78 node list (0x12750), and the 0x3F gate opens only on
         * the frame counter's multiples of 64, which the seed above puts at
         * f = 91: loop frame 1097, dumped frame 508, the frame capture 864
         * shows (DS_0010150C reads 92 after it). Without 0x12750 no flier is
         * ever live (-1); with the counter unseeded it spawns at f = 81. */
        CHECK_EQ_INT(s7_flier_f, 92);
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

    for (int i = 0; i < TITLE_WINDOW_ITERS; i++) {
        DSB(DS_000A81A8) = 1;   /* exactly one game_loop iteration per call */
        game_loop();            /* update -> render -> present -> dump */
        if (i == 0) {
            /* State-1 entry (spec DoD #2): the entry frame sets the title
             * countdown to 0x600 and leaves the state machine in state 1. */
            CHECK_EQ_INT((int)DSW(DS_000F0A64), 1);
            CHECK_EQ_INT((int)DSW(DS_000F0A66), 0x600);
        }
    }

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
