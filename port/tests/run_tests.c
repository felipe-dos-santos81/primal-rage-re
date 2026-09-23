#include <stdio.h>
#include <stdlib.h>
#include "test.h"

int g_failures = 0;

/* The env-gated drivers each call game_init(), which may run only once per
 * process (a second res_load_index() would exhaust the 64 MB bump allocator),
 * so each runs alone with nothing else in this process. */
static const struct { const char *env; int (*fn)(void); } k_drivers[] = {
#define DRIVER_ROW(name, env) { env, name },
    TEST_DRIVERS(DRIVER_ROW)
#undef DRIVER_ROW
};

int main(int argc, char **argv)
{
    (void)argc;

    /* The front-end fallback determinism gate (Task 1): two independent
     * PR_FRONTEND_DUMP runs must produce byte-identical frame-hash logs. It
     * needs argv[0] and re-invokes this binary, so it comes first and runs
     * nothing else in this process. */
    const char *det = getenv("PR_FRONTEND_DET");
    if (det != NULL && det[0] != '\0') {
        test_frontend_determinism(argv[0]);
        printf(g_failures ? "FAILURES: %d\n" : "all checks passed\n", g_failures);
        return g_failures != 0;
    }

    /* The continuous attract/title driver, the title oracle driver and the
     * state-2 selector driver each call game_init() and so run alone (see the
     * table comment). test_attract()/test_frontend() run their unit checks
     * first; test_title() is a no-op without PR_TITLE_DUMP. */
    for (size_t i = 0; i < sizeof k_drivers / sizeof k_drivers[0]; i++) {
        const char *v = getenv(k_drivers[i].env);
        if (v != NULL && v[0] != '\0') {
            k_drivers[i].fn();
            printf(g_failures ? "FAILURES: %d\n" : "all checks passed\n", g_failures);
            return g_failures != 0;
        }
    }

#define CASE_RUN(name) name();
    TEST_CASES(CASE_RUN)
#undef CASE_RUN
    printf(g_failures ? "FAILURES: %d\n" : "all checks passed\n", g_failures);
    return g_failures != 0;
}
