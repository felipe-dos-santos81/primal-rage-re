#ifndef PR_TEST_H
#define PR_TEST_H

#include <stdio.h>

extern int g_failures;

#define CHECK(cond, msg)                                                       \
    do {                                                                       \
        if (!(cond)) {                                                         \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, (msg));             \
            g_failures++;                                                      \
        }                                                                      \
    } while (0)

#define CHECK_EQ_INT(a, b)                                                     \
    do {                                                                       \
        long _a = (long)(a), _b = (long)(b);                                   \
        if (_a != _b) {                                                        \
            printf("FAIL %s:%d: %ld != %ld\n", __FILE__, __LINE__, _a, _b);    \
            g_failures++;                                                      \
        }                                                                      \
    } while (0)

/* One line per unit test; adding a test is one line here. The functions live
 * in the per-area files (test_platform.c, test_game.c, test_fight.c,
 * test_audio.c, test_video.c) — add to the area that owns the code. */
#define TEST_CASES(X)   \
    X(test_mem)         \
    X(test_le)          \
    X(test_res)         \
    X(test_gra)         \
    X(test_gfx)         \
    X(test_input)       \
    X(test_host)        \
    X(test_flow)        \
    X(test_opl)         \
    X(test_pitch)       \
    X(test_samples)     \
    X(test_mixer)       \
    X(test_sequencer)   \
    X(test_ail)         \
    X(test_smacker)     \
    X(test_movie)       \
    X(test_sprite)      \
    X(test_render)      \
    X(test_rng)         \
    X(test_actors)      \
    X(test_effects)     \
    X(test_fight)       \
    X(test_config)      \
    X(test_frontend)    \
    X(test_attract)     \
    X(test_anim)        \
    X(test_text)        \
    X(test_title)

/* One line per env-gated driver; each runs alone, before the unit cases. Every
 * entry is an int(void); the front-end determinism gate needs argv[0], so it is
 * declared and called separately (test_frontend_determinism below). */
#define TEST_DRIVERS(X)                       \
    X(test_attract,  "PR_ATTRACT_DUMP")       \
    X(test_title,    "PR_TITLE_DUMP")         \
    X(test_frontend, "PR_FRONTEND_DUMP")

#define TEST_CASE_DECLARE(name) int name(void);
TEST_CASES(TEST_CASE_DECLARE)
#undef TEST_CASE_DECLARE

#define TEST_DRIVER_DECLARE(name, env) int name(void);
TEST_DRIVERS(TEST_DRIVER_DECLARE)
#undef TEST_DRIVER_DECLARE

/* The title window driver on an already-initialised game: drive the state
 * machine until the title state is reached (post-attract), then its 96-frame
 * window. Shared by test_title() (standalone, PR_TITLE_DUMP) and test_attract()'s
 * continuous PR_ATTRACT_DUMP run, because game_init() may run once per process. */
int test_title_window(const char *dump);
/* The Task 1 fallback determinism gate: re-invoke this binary twice with
 * PR_FRONTEND_DUMP and require the two frame-hash logs byte-identical. `self` is
 * argv[0]; PR_FRONTEND_DET names the dump root. */
int test_frontend_determinism(const char *self);

#endif /* PR_TEST_H */
