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
#include <string.h>

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

    return g_failures - before;
}
