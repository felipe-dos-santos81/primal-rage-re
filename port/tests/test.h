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
 * test_audio.c, test_video.c) — add to the area that owns the code.
 * test_restart must stay last: its rs_check_resume and rs_check_resume_tail
 * run game_init_resume() on the unit process (game_state_init: mode 3,
 * attract state 0, a fresh actor pool; the RNG seed; the volumes), which
 * would change the state any test after it starts from. */
#define TEST_CASES(X)   \
    X(test_mem)         \
    X(test_fn_misslog)  \
    X(test_fn_register_repeats) \
    X(test_call_seam)   \
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
    X(test_movie_blit)  \
    X(test_movie_exits) \
    X(test_sprite)      \
    X(test_render)      \
    X(test_rng)         \
    X(test_actors)      \
    X(test_effects)     \
    X(test_fight)       \
    X(test_pose_pool)   \
    X(test_config)      \
    X(test_cfg_helpers) \
    X(test_svcmenu)     \
    X(test_frontend)    \
    X(test_attract)     \
    X(test_anim)        \
    X(test_text)        \
    X(test_title)       \
    X(test_mode1e_rearm) \
    X(test_nameentry_input) \
    X(test_key_loop)     \
    X(test_voice_sites) \
    X(test_fight_voice_sites) \
    X(test_table_reached) \
    X(test_u6_idle_loss_callbacks) \
    X(test_u6b_3c048) \
    X(test_u6b_3d1ec) \
    X(test_u6b_3f0a8) \
    X(test_u6b_3f0f0) \
    X(test_u6b_231c0) \
    X(test_p1_finishers) \
    X(test_p1_callbacks) \
    X(test_p1_anim_targets) \
    X(test_p2_guarded) \
    X(test_p2_reactions_3) \
    X(test_p2_unconditional) \
    X(test_p2_arming) \
    X(test_p2_hooks) \
    X(test_p2_1c) \
    X(test_p2_0c) \
    X(test_p3_simple) \
    X(test_p3_47720) \
    X(test_p3_47874) \
    X(test_virtual_clock) \
    X(test_gp_poke_script) \
    X(test_restart)

/* One line per env-gated driver; each runs alone, before the unit cases. Every
 * entry is an int(void); the front-end determinism gate needs argv[0], so it is
 * declared and called separately (test_frontend_determinism below). */
#define TEST_DRIVERS(X)                       \
    X(test_attract,  "PR_ATTRACT_DUMP")       \
    X(test_title,    "PR_TITLE_DUMP")         \
    X(test_frontend, "PR_FRONTEND_DUMP")      \
    X(test_k11_oracle, "PR_K11_DUMP")    \
    X(test_gp_replay, "PR_GP_DUMP")      \
    X(test_restart_drive, "PR_RESTART")

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
/* The miss log's driver gate (record gameplay-u0 §U0.2): after the driver
 * selected by `env` ran with the log armed, the recorded (address, caller)
 * pairs must be exactly that driver's pinned known-set. */
int test_fn_misslog_driver(const char *env);

#endif /* PR_TEST_H */
