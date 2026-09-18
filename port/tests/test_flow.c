#include "game/flow.h"
#include "game/actors.h"
#include "host.h"
#include "mem.h"
#include "symbols.h"
#include "platform/gfx.h"
#include "platform/audio/ail.h"
#include "platform/audio/mixer.h"
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

int test_flow(void)
{
    int before = g_failures;

    check_bank_guard();

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
    u32 frame0 = DSD(DS_000EF6DC);

    /* Task 9: the real title. 0x121A0's entry frame resets and repopulates the
     * actor pools; Format reference G pins the entry-frame invariants: the
     * three entry draws (re-seeded 0xABCD) give DS_00107A50 = 0x2420 and
     * DS_00107A3A = 0x121, and DS_000F0A66 = 0x600. */
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

    /* A deferred (non-title) state must be a harmless no-op, not a crash. */
    DSW(DS_000F0A64) = 6;
    game_frame();
    CHECK_EQ_INT(DSB(DS_000A81A8), 0);

    /* A mode other than 3 must not run the state machine at all. */
    DSD(DS_00104B00) = 7;
    game_frame();

    /* Task 11: the init chain's audio calls and the title state's music request
     * drive the sequencer with no device open (the suite never opens one).
     * The title state above asked for music; the master-loop service inherits
     * that request, loads the S16TITLE bank and ticks it. */
    game_audio_init();
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

    return g_failures - before;
}
