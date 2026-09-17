#include "game/flow.h"
#include "mem.h"
#include "symbols.h"
#include "platform/gfx.h"
#include "platform/audio/ail.h"
#include "platform/res.h"
#include "test.h"

static int g_called;

static void probe_task(void) { g_called++; }

static u32 buf_hash(const u8 *p)
{
    u32 h = 2166136261u;
    for (u32 k = 0; k < 320u * 200u; k++) { h ^= p[k]; h *= 16777619u; }
    return h;
}

static u32 buf_nonzero(const u8 *p)
{
    u32 n = 0;
    for (u32 k = 0; k < 320u * 200u; k++) if (p[k]) n++;
    return n;
}

/* game_loop() presents mem[DS_000E87A4] and then swaps the two buffers. The
 * presentation test below must reproduce that ordering, otherwise it cannot see
 * a buffer that is presented but never redrawn. */
static void swap_like_loop(void)
{
    u32 t = DSD(DS_000E87A0);
    DSD(DS_000E87A0) = DSD(DS_000E87A4);
    DSD(DS_000E87A4) = t;
}

int test_flow(void)
{
    int before = g_failures;

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

    game_frame();

    CHECK(g_called == 1, "game_frame runs the update process table");
    CHECK_EQ_INT(DSD(DS_000EF6DC), (long)frame0 + 1);
    DSD(DS_00104AE8) = 0;

    /* Presentation test: over consecutive frames the buffer game_loop presents
     * must always be a drawn image. This fails if a hold frame skips the redraw
     * while the loop still swaps (the old blank-every-other-frame bug). */
    u32 last_hash = 0;
    int distinct = 0;
    for (int i = 0; i < 20; i++) {
        game_frame();
        const u8 *presented = mem + DSD(DS_000E87A4);
        u32 nz = buf_nonzero(presented);
        CHECK(nz > 1000, "presented buffer is a drawn image, not blank");
        u32 h = buf_hash(presented);
        if (h != last_hash) { distinct++; last_hash = h; }
        DSD(DS_001014FC) = 0;
        swap_like_loop();
    }
    CHECK(distinct >= 2, "title animates across presented buffers");

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
    game_audio_service();                    /* starts the pending title music */
    CHECK_EQ_INT((int)game_audio_ticks(), 2);
    for (int i = 0; i < 40; i++)
        game_audio_service();
    CHECK(game_music_notes_seen(), "title music keys notes without a device");
    AIL_shutdown();                          /* release handles for later tests */

    return g_failures - before;
}
