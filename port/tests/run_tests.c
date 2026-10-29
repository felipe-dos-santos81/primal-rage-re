#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "test.h"
#include "mem.h"

int g_failures = 0;

#ifdef PR_COVERAGE
/* PORT: the coverage hit dump (record state-coverage §D1.9), written beside the
 * miss log when PR_HIT_TAG names a tag. Returns 1 on a write failure. */
static int write_hits(const char *tag)
{
    char path[64];
    snprintf(path, sizeof path, "coverage/%s.hits", tag);
    mkdir("coverage", 0755);           /* ignore EEXIST; Task 3 git-ignores it */
    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "run_tests: could not write %s\n", path);
        return 1;
    }
    fn_hit_report_f(f, tag);
    return fclose(f) != 0;
}
#endif

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
            /* PORT: the miss log (record gameplay-u0 §U0.1) is armed for the
             * whole driver process, reported, and checked against the pinned
             * known-set of that driver. */
            fn_misslog_arm(1);
#ifdef PR_COVERAGE
            const char *hit_tag = getenv("PR_HIT_TAG");
            int hit = hit_tag != NULL && hit_tag[0] != '\0';
            if (hit) fn_hit_arm(1);
#endif
            k_drivers[i].fn();
            fn_misslog_report(k_drivers[i].env);
            test_fn_misslog_driver(k_drivers[i].env);
#ifdef PR_COVERAGE
            if (hit) g_failures += write_hits(hit_tag);
#endif
            fn_misslog_arm(0);
#ifdef PR_COVERAGE
            if (hit) fn_hit_arm(0);
#endif
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
