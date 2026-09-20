#include <stdio.h>
#include <stdlib.h>
#include "test.h"

int g_failures = 0;

int main(void)
{
    /* The 4d continuous-run driver calls game_init() and drives the attract and
     * the title in one process, so it runs alone for the same reason as
     * PR_TITLE_DUMP below. test_attract() still runs its unit checks first. */
    if (getenv("PR_ATTRACT_DUMP") != NULL) {
        test_attract();
        printf(g_failures ? "FAILURES: %d\n" : "all checks passed\n", g_failures);
        return g_failures != 0;
    }

    /* The title oracle driver calls game_init() and so needs a fresh mem[]; it
     * cannot share the process with the unit suite (a second res_load_index()
     * would exhaust the 64 MB bump allocator). When PR_TITLE_DUMP asks for the
     * dump, run only that driver. It now drives the state-0 attract first to
     * reach the post-attract title window. */
    if (getenv("PR_TITLE_DUMP") != NULL) {
        test_title();
        printf(g_failures ? "FAILURES: %d\n" : "all checks passed\n", g_failures);
        return g_failures != 0;
    }

    /* The state-2 selector driver also calls game_init(), so it must run alone
     * for the same reason as PR_TITLE_DUMP above. test_frontend() still runs
     * its helper unit checks before the gated driver. */
    if (getenv("PR_FRONTEND_DUMP") != NULL) {
        test_frontend();
        printf(g_failures ? "FAILURES: %d\n" : "all checks passed\n", g_failures);
        return g_failures != 0;
    }

    test_scaffold();
    test_mem();
    test_le();
    test_res();
    test_gra();
    test_gfx();
    test_input();
    test_host();
    test_flow();
    test_opl();
    test_pitch();
    test_samples();
    test_mixer();
    test_sequencer();
    test_ail();
    test_smacker();
    test_movie();
    test_sprite();
    test_render();
    test_rng();
    test_actors();
    test_effects();
    test_config();
    test_frontend();
    test_attract();
    test_anim();
    test_text();
    test_title();       /* no-op unless PR_TITLE_DUMP is set */
    printf(g_failures ? "FAILURES: %d\n" : "all checks passed\n", g_failures);
    return g_failures != 0;
}
