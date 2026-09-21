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

int test_scaffold(void);
int test_mem(void);
int test_le(void);
int test_res(void);
int test_gra(void);
int test_gfx(void);
int test_input(void);
int test_host(void);
int test_flow(void);
int test_opl(void);
int test_pitch(void);
int test_samples(void);
int test_mixer(void);
int test_sequencer(void);
int test_ail(void);
int test_smacker(void);
int test_movie(void);
int test_sprite(void);
int test_render(void);
int test_rng(void);
int test_actors(void);
int test_effects(void);
int test_fight(void);
int test_config(void);
int test_anim(void);
int test_text(void);
int test_title(void);
/* The title window driver on an already-initialised game: drive the state
 * machine until the title state is reached (post-attract), then its 96-frame
 * window. Shared by test_title() (standalone, PR_TITLE_DUMP) and test_attract()'s
 * continuous PR_ATTRACT_DUMP run, because game_init() may run once per process. */
int test_title_window(const char *dump);
int test_frontend(void);
/* The Task 1 fallback determinism gate: re-invoke this binary twice with
 * PR_FRONTEND_DUMP and require the two frame-hash logs byte-identical. `self` is
 * argv[0]; PR_FRONTEND_DET names the dump root. */
int test_frontend_determinism(const char *self);
int test_attract(void);

#endif /* PR_TEST_H */
