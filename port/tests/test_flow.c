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
 * consumes no draw, and the first unexplained demo frame (capture 814) is inside
 * this hold, so a draw here would be a determinism site and not a render gap.
 * The two LCG-state assertions fail if case 9 ever draws. */
static void check_state9_countdown(void)
{
    const u32 saved_rng = DSD(DS_000EF6D8);
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

    /* 0x11A8C: the live state 6. The old "a deferred state is a harmless no-op"
     * premise is retired — state 6 now draws the shared RNG, runs 0x41350 for
     * both players, spawns the HUD path and arms the 900-frame timer. It is
     * exercised last, after the title-overlay and audio assertions, so its text
     * and overlay writes cannot corrupt them. */
    DSD(DS_00104B00) = 3;
    DSB(DS_00104B1D) = 1;                    /* skip the deferred menu poll */
    DSB(DS_00104B15) = 0;
    DSB(DS_00104B19 + 2u) = 0;
    DSW(DS_001082CC) = 0;
    DSW(DS_000F0A6A) = 0;
    DSW(DS_000F0A72) = 5;
    DSW(DS_000F0A6C) = 0;
    DSB(DS_000F0A6F) = 0xFF;
    DSW(DS_000F0A64) = 6;
    rng_seed(0xABCDu);
    game_frame();
    CHECK_EQ_INT((int)DSB(DS_00104B15), 1);
    CHECK_EQ_INT((int)DSW(DS_001082CC), 3);
    CHECK_EQ_INT((int)DSW(DS_000F0A64), 7);
    CHECK_EQ_INT((int)DSW(DS_000F0A6A), 900);
    CHECK_EQ_INT((int)DSW(DS_000F0A6C), 5);
    CHECK_EQ_INT((int)DSB(DS_000F0A6F), 0);

    return g_failures - before;
}
