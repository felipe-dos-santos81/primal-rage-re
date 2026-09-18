/* Task 10: the title oracle driver. It runs the real engine init and the master
 * loop for exactly the pinned 96-frame window, letting Task 9's PR_TITLE_DUMP
 * hook write one RGB24 frame per presented title frame. The pixel comparison
 * lives in tools/title_compare.py (`make title-oracle`); this file owns the
 * dump, its frame count and the entry invariants.
 *
 * The driver is the only test that calls game_init(), so it cannot share a
 * process with the unit suite: a second res_load_index() would exhaust the 64 MB
 * bump allocator. run_tests.c therefore runs it alone when PR_TITLE_DUMP is set,
 * and this file is a no-op in the normal suite. It reuses the production
 * game_loop() rather than calling game_frame()/render_list() itself, because the
 * master-loop body also flushes the palette, presents the front buffer and swaps
 * it — swap_buffers() is not exported, so only game_loop() reproduces the frame
 * the capture holds. */
#include "game/flow.h"
#include "game/actors.h"
#include "mem.h"
#include "symbols.h"
#include "test.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TITLE_WINDOW_FRAMES 96

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
     * No re-seed here: game_init()'s rng_seed(0xABCD) is the port half of the
     * other three sites and the measured path (Task 9 errata). */
    actors_pin_anim_tick_zero(1);

    for (int i = 0; i < TITLE_WINDOW_FRAMES; i++) {
        DSB(DS_000A81A8) = 1;   /* exactly one game_loop iteration per call */
        game_loop();            /* update -> render -> present -> dump */
        if (i == 0) {
            /* State-1 entry (spec DoD #2): the entry frame sets the title
             * countdown to 0x600 and leaves the state machine in state 1. */
            CHECK_EQ_INT((int)DSW(DS_000F0A64), 1);
            CHECK_EQ_INT((int)DSW(DS_000F0A66), 0x600);
        }
    }
    game_shutdown();

    /* The window's own boundary, not the frame count: the 96th presented frame
     * is the one where DS_000F0A66 has just fallen below 0x11. If the timing
     * drifted, the dump would cover the wrong window and this fails. */
    CHECK_EQ_INT((int)DSW(DS_000F0A64), 1);
    CHECK(DSW(DS_000F0A66) < 0x11,
          "window ends at DS_000F0A66 < 0x11 (state-1 entry predicate)");

    /* The window is state 1 for 96 frames; the dump must hold exactly one RGB24
     * frame per presented frame (the hook's cap is 200, so the count is the
     * run's, not the cap's). */
    {
        char sub[1200];
        snprintf(sub, sizeof sub, "%s/title", dump);
        CHECK_EQ_INT(count_raw(sub), TITLE_WINDOW_FRAMES);
    }
    /* Frame 96's invariants: the entry spawned the logo and the phase ran to the
     * release point. A missing logo means the window compared a dead scene. */
    CHECK(DSD(DS_000F0A58) != 0, "title logo record exists after the window");

    return g_failures - before;
}
