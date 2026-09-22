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
#include "game/flow.h"
#include "game/actors.h"
#include "game/rng.h"
#include "mem.h"
#include "symbols.h"
#include "test.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
