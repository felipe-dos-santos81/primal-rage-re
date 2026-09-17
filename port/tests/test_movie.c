#include "game/movie.h"
#include "host.h"
#include "test.h"
#include <stdio.h>
#include <stdlib.h>

/* The player owns the boot-logos presentation rule. The decoder decodes every
 * payload frame; the original presents TWI5 120 of 121 (its 121st payload frame
 * is real and never shown) and TWG 41 of 41 (its 41st is a hold of the 40th, so
 * the held image is displayed). `movie_frames_presented()` reports the count the
 * last movie_play() actually presented, so the rule is asserted on the real
 * playback, not on a helper.
 * A headless gate must not pace against the wall clock: the suite never opens a
 * window, so the movie's real-time tick budget (TWI5 ~515 ticks) must not be
 * spent waiting. Pacing is re-enabled when a window is open (windowed run). */
int test_movie(void)
{
    int before = g_failures;
    const char *dir = getenv("PR_GAME_DIR");
    if (dir == NULL) {
        printf("test_movie: PR_GAME_DIR unset, skipping\n");
        return 0;
    }

    u32 ticks0 = host_tick_count();
    CHECK_EQ_INT(movie_play(dir, "twi5.smk"), 1);
    CHECK_EQ_INT((int)movie_frames_presented(), 120);
    CHECK((host_tick_count() - ticks0) < 100,
          "headless playback does not sleep on the VBlank clock");

    CHECK_EQ_INT(movie_play(dir, "twg.smk"), 1);
    CHECK_EQ_INT((int)movie_frames_presented(), 41);

    /* A missing name is a clean skip: return 1, message, no crash, nothing
     * presented. The name is lowercase while the file is uppercase; the scan
     * inside res_load_file handles the case mismatch, so a missing name is a
     * genuine miss, not a case artefact. */
    CHECK_EQ_INT(movie_play(dir, "no-such-movie.smk"), 1);
    CHECK_EQ_INT((int)movie_frames_presented(), 0);

    /* Invalid arguments are a hard failure (0), never a crash. */
    CHECK_EQ_INT(movie_play(NULL, "twi5.smk"), 0);
    CHECK_EQ_INT(movie_play(dir, NULL), 0);

    return g_failures - before;
}
