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
int test_config(void);
int test_anim(void);
int test_text(void);
int test_title(void);
int test_frontend(void);
int test_attract(void);

#endif /* PR_TEST_H */
