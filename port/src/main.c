#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "host.h"
#include "game/flow.h"

static void usage(const char *argv0)
{
    printf("usage: %s --game-dir DIR [--check N]\n", argv0);
}

/* Task 14: open the SDL host, then hand control to the ported game flow
 * (init chain -> frame loop -> title state), which runs until ESC. */
static int run_windowed(const char *game_dir)
{
    if (!host_init("Primal Rage", 320, 200)) {
        fprintf(stderr, "prageport: no window/display available\n");
        return 1;
    }
    printf("prageport 0.0.1 game-dir=%s\n", game_dir);
    game_set_game_dir(game_dir);
    int rc = game_main();
    host_shutdown();
    return rc;
}

int main(int argc, char **argv)
{
    const char *game_dir = "data/game/C";
    int check_frames = 0;
    int have_check = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--game-dir") == 0 && i + 1 < argc) {
            game_dir = argv[++i];
        } else if (strcmp(argv[i], "--check") == 0 && i + 1 < argc) {
            check_frames = atoi(argv[++i]);
            have_check = 1;
        } else if (strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else {
            usage(argv[0]);
            return 2;
        }
    }
    if (have_check) {
        /* TODO(verify): Task 15 runs N frames headlessly and writes
         * frame_NNNN.ppm plus a .pal sidecar; Task 12 only accepts the flag and
         * skips host_init() so no window opens. */
        printf("prageport 0.0.1 game-dir=%s --check %d (headless; frame capture "
               "lands in Task 15)\n", game_dir, check_frames);
        return 0;
    }
    return run_windowed(game_dir);
}
