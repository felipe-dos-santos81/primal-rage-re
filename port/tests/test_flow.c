#include "game/flow.h"
#include "mem.h"
#include "symbols.h"
#include "platform/gfx.h"
#include "platform/res.h"
#include "test.h"

static int g_called;

static void probe_task(void) { g_called++; }

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

    DSW(DS_000F0A64) = 1;      /* title state */
    DSB(DS_000A81A8) = 0;      /* not quitting */
    DSB(DS_00104B1D) = 1;      /* skip the deferred menu poll */
    u32 frame0 = DSD(DS_000EF6DC);

    game_frame();

    CHECK(g_called == 1, "game_frame runs the update process table");
    CHECK_EQ_INT(DSD(DS_000EF6DC), (long)frame0 + 1);
    CHECK_EQ_INT(DSD(DS_001014FC), 1);   /* title asked for a full present */
    DSD(DS_00104AE8) = 0;

    /* The title frame was decoded into the back buffer: the shipped title art
     * is not all index 0. */
    u8 *fb = mem + DSD(DS_000E87A4);
    u32 nonzero = 0;
    for (u32 i = 0; i < 320u * 200u; i++) if (fb[i]) nonzero++;
    CHECK(nonzero > 1000, "title frame decoded into the draw buffer");

    /* The title enqueued a palette record; flushing it fills the DAC. */
    gfx_dac[1][0] = gfx_dac[1][1] = gfx_dac[1][2] = 0;
    gfx_flush_palette();
    CHECK(gfx_dac[1][0] || gfx_dac[1][1] || gfx_dac[1][2],
          "title palette reached gfx_dac");

    /* A deferred (non-title) state must be a harmless no-op, not a crash. */
    DSW(DS_000F0A64) = 6;
    game_frame();
    CHECK_EQ_INT(DSB(DS_000A81A8), 0);

    return g_failures - before;
}
