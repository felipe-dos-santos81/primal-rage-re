#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "host.h"
#include "platform/input.h"

static void usage(const char *argv0)
{
    printf("usage: %s --game-dir DIR [--check N]\n", argv0);
}

/* Placeholder run until game/flow (Task 14) supplies game_main(): open the real
 * SDL window and pump until ESC (or window close, which the host maps to ESC).
 * This is the Task 12 proof that the SDL host is live, not the final loop. */
static int run_windowed(const char *game_dir)
{
    if (!host_init("Primal Rage", 320, 200)) {
        fprintf(stderr, "prageport: no window/display available\n");
        return 1;
    }
    printf("prageport 0.0.1 game-dir=%s\n", game_dir);
    for (;;) {
        host_pump();
        if (input_check_key() == 0x011B) { /* ESC: scan 0x01, ASCII 0x1B */
            input_clear();
            break;
        }
        host_wait_vblank();
    }
    host_shutdown();
    return 0;
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
