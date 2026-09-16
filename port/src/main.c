#include <stdio.h>
#include <string.h>

static void usage(const char *argv0)
{
    printf("usage: %s --game-dir DIR [--check N]\n", argv0);
}

int main(int argc, char **argv)
{
    const char *game_dir = "data/game/C";
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--game-dir") == 0 && i + 1 < argc) {
            game_dir = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else {
            usage(argv[0]);
            return 2;
        }
    }
    printf("prageport 0.0.1 game-dir=%s\n", game_dir);
    return 0;
}
