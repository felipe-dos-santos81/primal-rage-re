/* The front-end helpers 0x1C6D4 and 0x33904, and (under PR_FRONTEND_DUMP) the
 * state-2 selector driver. The driver calls game_init(), which may run once per
 * process, so it is env-gated exactly like test_title(): run_tests.c runs this
 * file alone when PR_FRONTEND_DUMP is set, and the helper checks still run at
 * the top of test_frontend() before the driver. */
#include "game/flow.h"
#include "game/actors.h"
#include "mem.h"
#include "symbols.h"
#include "test.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

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
        const u32 tbl = DS_00107608;
        for (u32 i = 0; i < 0x190u; i++) saved[i] = DSB(tbl + i);
        mem_fill(tbl, 0, 0x190u);
        DSD(tbl + 0x14u) = 1u;            /* entry at tbl+0x10 is live */
        CHECK_EQ_INT((int)frontend_list_next(0), (int)(tbl + 0x10u));
        CHECK_EQ_INT((int)frontend_list_next(tbl + 0x10u), 0);
        for (u32 i = 0; i < 0x190u; i++) DSB(tbl + i) = saved[i];
    }

    const char *dump = getenv("PR_FRONTEND_DUMP");
    if (dump == NULL || dump[0] == '\0') {
        printf("test_frontend: PR_FRONTEND_DUMP unset, state-2 driver skipped\n");
        return g_failures - before;
    }

    /* The driver runs the real init and loop for a fixed window, as the title
     * driver does, because game_init() may run once per process.
     * PR_FRONTEND_DUMP names a directory to receive the hash log. */
    {
        const char *dir = getenv("PR_GAME_DIR");
        if (dir == NULL || dir[0] == '\0') dir = "data/game/C";
        game_set_game_dir(dir);
        game_init();
        actors_pin_anim_tick_zero(1);

        /* Enter state 2 at phase 0, the entry game_state_title() leaves for. */
        DSW(DS_000F0A64) = 2;
        DSB(DS_000F0A6F) = 0;

        char log_path[1200];
        snprintf(log_path, sizeof log_path, "%s/select.log", dump);
        FILE *log = fopen(log_path, "w");
        CHECK(log != NULL, "state-2 hash log opens");

        u32 seen_entries = 0;
        for (int i = 0; i < 640; i++) {
            DSB(DS_000A81A8) = 1;          /* exactly one game_loop iteration */
            game_loop();
            if (DSB(DS_000F0A6E) < 6u) seen_entries |= 1u << DSB(DS_000F0A6E);
            if (log != NULL) {
                /* Hash the presented index buffer without reading pixels. */
                const u8 *fb = mem + DSD(DS_000E87A4);
                u32 h = 2166136261u;
                for (u32 b = 0; b < 320u * 200u; b++) h = (h ^ fb[b]) * 16777619u;
                fprintf(log, "%d %u %u\n", i, (unsigned)DSB(DS_000F0A6F), h);
            }
        }
        if (log != NULL) fclose(log);

        /* The raw's timeline: six entries, each drawn then paused, then state 3.
         * Entry k is drawn on frame 1+93k; after the sixth, 0x1E pause frames
         * and the phase-3 handoff land state 3 on frame 589 (the arithmetic is
         * in docs/superpowers/plans/2026-09-19-frontend-input-derivations.md).
         * The 640-frame window leaves margin. */
        CHECK_EQ_INT((int)seen_entries, 0x3F);
        CHECK_EQ_INT((int)DSW(DS_000F0A64), 3);
        game_shutdown();
    }
    return g_failures - before;
}
