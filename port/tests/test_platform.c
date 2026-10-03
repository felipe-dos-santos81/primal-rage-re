/* test_platform.c — the platform suite.
 *
 * Consolidated from: test_mem.c, test_le.c, test_res.c, test_gra.c, test_gfx.c, test_sprite.c, test_render.c, test_input.c, test_host.c, test_rng.c, test_text.c.
 * Every assertion is carried verbatim; only the file's home and the
 * two cross-file static names (none) changed. */

#include "mem.h"
#include "symbols.h"
#include "test.h"
#include "test_fixtures.h"
#include "platform/res.h"
#include "platform/gfx.h"
#include "game/flow.h"
#include "platform/gra.h"
#include "platform/sprite.h"
#include "platform/render.h"
#include "platform/input.h"
#include "host.h"
#include "game/rng.h"
#include "game/actors.h"
#include "game/menu.h"
#include "game/config.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <setjmp.h>
#include <dirent.h>
#include <strings.h>
#include <stdint.h>
#include <time.h>


/* ---- test_mem.c ---- */

void fn_probe(void) { }
void fn_probe2(void) { }

int test_mem(void)
{
    int before = g_failures;

    CHECK_EQ_INT(MEM_SIZE, 0x4000000);
    CHECK(mem_in_range(0x80000, 0x8B0D0), "data object range is inside mem[]");
    CHECK(!mem_in_range(MEM_SIZE - 4, 8), "writes past the end are rejected");

    /* DSB offsets include the 0x80000 base: DAT_00080004 is DS:0x0004. */
    DSB(0x80004) = 0xAB;
    CHECK_EQ_INT(mem[0x80004], 0xAB);
    CHECK_EQ_INT(DSB(0x80004), 0xAB);

    DSD(0x80010) = 0x11223344u;
    CHECK_EQ_INT(DSD(0x80010), 0x11223344u);

    mem_fill(0x90000, 0x5A, 4);
    CHECK_EQ_INT(DSB(0x90000), 0x5A);
    CHECK_EQ_INT(DSB(0x90003), 0x5A);
    CHECK_EQ_INT(DSB(0x90004), 0x00);

    /* The macros must be usable as lvalues, not just rvalues. */
    DSW(0x90008) = 0x1234;
    CHECK_EQ_INT(DSW(0x90008), 0x1234);

    /* Code addresses stored in data (process tables, the lock calls in main)
     * must survive a round trip through the flat address space. */
    {
        extern void fn_probe(void);
        extern void fn_probe2(void);
        CHECK(fn_resolve(FN_0002D62C) == NULL, "unregistered address resolves to NULL");
        fn_register(FN_0002D62C, fn_probe);
        CHECK(fn_resolve(FN_0002D62C) == fn_probe, "round trip");
        CHECK_EQ_INT(fn_origin(fn_probe), FN_0002D62C);
        CHECK(fn_resolve(0xDEAD) == NULL, "unknown address is NULL, not garbage");

        /* An unregistered function has origin 0, the sentinel callers test. */
        CHECK_EQ_INT(fn_origin(fn_probe2), 0);

        /* Same address registered twice: the first entry wins and the later
         * registration is ignored, not silently substituted. */
        fn_register(FN_000255CC, fn_probe);
        fn_register(FN_000255CC, fn_probe2);
        CHECK(fn_resolve(FN_000255CC) == fn_probe, "duplicate address keeps first registration");
    }

    return g_failures - before;
}

/* The miss log (record gameplay-u0 §U0.1/§U0.2). Its pinned known-set: every
 * stock driver run, and the --check run, records exactly these (address,
 * caller) pairs. 0x5D812 is the runtime's `xor eax,eax; ret` stub
 * (0x5D812..0x5D814), deliberately unregistered (actors.c, flow.c): its
 * return is discarded at both sites, so the skip is equivalent. The
 * PR_FRONTEND_DUMP driver adds its own unit probe 0x41578 (test_frontend's
 * "direct-called only, not registered" check, which runs in the same
 * process before game_init()). Measured with every driver (title, attract,
 * frontend, restart, K11 walk/menuesc/idle/diags/de) and --check 5/60/820. */
typedef struct { u32 addr; const char *ctx; } fnm_pair;
static const fnm_pair k_miss_known[] = {
    { 0x5D812u, "actor_spawn" },
    { 0x5D812u, "set_dead" },
};
static const fnm_pair k_miss_frontend[] = {
    { 0x41578u, "test_frontend" },
};

/* The gp replay's own known-set (record gameplay-ground-truth §G.24, U4
 * review 1), per scenario: the PR_GP_DUMP driver replays a capture's script and
 * the named scenario decides what may be missed, on top of the 0x5D812 pair
 * above. gp-pads (the MAIN MENU) records only that base set. gp-idle-loss
 * (measured on the merged base, the full replay to f = 0x207F) adds two
 * pairs, each classified from the raw in §G.24:
 *   0x29D60 frontend_mode_1b_step: a bare `ret` (one byte, 0x29D60), f = 0x293;
 *   0x5D812 frontend_mode_1b_step: the runtime stub again, f = 0x77A.
 * Ported since (record gameplay-u6 §U6.7): 0x3640C (anim_indirect, U4's f = 0x173A;
 * unreached since the 0x37B03 fix, record gameplay-u5 §C5.3/§C5.12), 0x23208
 * (hit_reaction_apply, f = 0x824), 0x37DCC (anim_indirect, reached only once
 * 0x3A588 is ported) and 0x3A588 (fighter_state_3531c, from f = 0x8E7; U4's pre-fix f = 0x927).
 * A scenario with no entry here may miss only the base pair. */
static const fnm_pair k_miss_gp_idle_loss[] = {
    { 0x29D60u, "frontend_mode_1b_step" },
    { 0x5D812u, "frontend_mode_1b_step" },
};

/* gp-u5-charsel (record gameplay-u5 §C5.6/§C5.15): the character-select walk,
 * cut at the first frame of mode 6, adds the two hooks of the wipes it passes,
 * each classified in §G.24: 0x29D60, a bare `ret` (the wipe's end into mode
 * 0x10), and 0x5D812, the runtime stub (the wipe into mode 5). */
static const fnm_pair k_miss_gp_charsel[] = {
    { 0x29D60u, "frontend_mode_1b_step" },
    { 0x5D812u, "frontend_mode_1b_step" },
};

/* gp-u6-moves (plan gameplay-u6b, record gameplay-u6 §U6.12; the prefix match
 * in test_fn_misslog_driver also selects the re-capture gp-u6-moves-b and the
 * dry-run script gp-u6-moves-dry, record §U6.22) passes the same
 * MAIN MENU, START MENU and character-select wipes as gp-idle-loss, so it
 * records the same two harmless pairs (record §G.24): the bare `ret` 0x29D60
 * and the runtime stub 0x5D812, both from frontend_mode_1b_step. */
static const fnm_pair k_miss_gp_u6_moves[] = {
    { 0x29D60u, "frontend_mode_1b_step" },
    { 0x5D812u, "frontend_mode_1b_step" },
};

/* gp-keys-fight (record 2026-10-01-gameplay-u11 §K.12): the gp-idle-loss path
 * to round 1, the in-match keys, the join and the 0x24AB0 restart, measured
 * on its full replay; each pair is one of §G.24's classified misses. */
static const fnm_pair k_miss_gp_keys_fight[] = {
    { 0x29D60u, "frontend_mode_1b_step" },
    { 0x5D812u, "frontend_mode_1b_step" },
};

/* The gp-twop replay's own pairs (gameplay U7, record
 * 2026-10-01-gameplay-u7-derivations.md §T.9: measured on the full replay of
 * data/k11-captures/gp-twop to its X record; each classified there from the
 * raw), the two hooks of the wipes it passes, as gp-u5-charsel's:
 *   0x29D60 frontend_mode_1b_step: the bare `ret` (record §G.24);
 *   0x5D812 frontend_mode_1b_step: the runtime stub (record §G.24).
 * A scenario with no entry here may miss only the base pair. */
static const fnm_pair k_miss_gp_twop[] = {
    { 0x29D60u, "frontend_mode_1b_step" },
    { 0x5D812u, "frontend_mode_1b_step" },
};

/* Gameplay U8 (record 2026-10-01-gameplay-u8-derivations.md §U8.16): one set
 * per U8 scenario (an exact name), each pair measured on the full replay of its
 * capture and classified from the raw in the record.
 * gp-u8-right-arcade (START MENU row 1, b1f = 2), to its X record (f = 0x8E1):
 *   0x29D60 frontend_mode_1b_step: the bare `ret` (record §G.24);
 *   0x5D812 frontend_mode_1b_step: the runtime stub (record §G.24).
 * The move callbacks it reaches, 0x14EF8 and 0x14F50 (the dwords 0xA46A8 and
 * 0xA46BC, character 3's reactions 0x20/0x21, called by hit_reaction_apply at
 * 0x35045), and their streams' 0xD000 targets 0x14FA8, 0x14FF8 and 0x150AC
 * (anim_indirect) are ported by track P batch 2 (record
 * 2026-10-02-reverse-p2 §P2.4), so none of them is a miss. */
static const fnm_pair k_miss_gp_u8_right_arcade[] = {
    { 0x29D60u, "frontend_mode_1b_step" },
    { 0x5D812u, "frontend_mode_1b_step" },
};

/* gp-u8-left-training (START MENU row 2, b1d = 1, b1f = 3; record §U8.17),
 * to its X record (f = 0x921): the two hooks of the wipes it passes, as
 * gp-u5-charsel's: 0x29D60, the bare `ret`, and 0x5D812, the runtime stub
 * (record §G.24), both from frontend_mode_1b_step. */
static const fnm_pair k_miss_gp_u8_left_training[] = {
    { 0x29D60u, "frontend_mode_1b_step" },
    { 0x5D812u, "frontend_mode_1b_step" },
};

/* gp-u8-right-training (START MENU row 3, b1d = 1, b1f = 3; record §U8.18),
 * to its X record (f = 0x961): the same two wipe hooks (record §G.24), the
 * bare `ret` 0x29D60 and the runtime stub 0x5D812, from frontend_mode_1b_step. */
static const fnm_pair k_miss_gp_u8_right_training[] = {
    { 0x29D60u, "frontend_mode_1b_step" },
    { 0x5D812u, "frontend_mode_1b_step" },
};

/* gp-u8-tug-of-war (START MENU row 4, b1d = 2, b1f = 3; record §U8.19), to
 * its X record (f = 0x9A1): the same two wipe hooks (record §G.24), the bare
 * `ret` 0x29D60 and the runtime stub 0x5D812, from frontend_mode_1b_step. */
static const fnm_pair k_miss_gp_u8_tug_of_war[] = {
    { 0x29D60u, "frontend_mode_1b_step" },
    { 0x5D812u, "frontend_mode_1b_step" },
};

/* gp-u8-handicap (START MENU row 6, b1d = 4, b1f = 3; record §U8.20), to its
 * X record (f = 0x8E1): the same two wipe hooks (record §G.24), the bare `ret`
 * 0x29D60 and the runtime stub 0x5D812, from frontend_mode_1b_step. */
static const fnm_pair k_miss_gp_u8_handicap[] = {
    { 0x29D60u, "frontend_mode_1b_step" },
    { 0x5D812u, "frontend_mode_1b_step" },
};

/* gp-u8-endurance (START MENU row 5, b1d = 3, b1f = 3; record §U8.21), to its
 * X record (f = 0x495, 300 frames into the team select, decision D3): only the
 * bare `ret` 0x29D60 (record §G.24) from frontend_mode_1b_step; the replay
 * never leaves mode 0x10, as the §U8.3 preview. */
static const fnm_pair k_miss_gp_u8_endurance[] = {
    { 0x29D60u, "frontend_mode_1b_step" },
};

/* gp-u8-attract-start (P1's F1 in mode 3, the pad arm: b1d = 0, b1f = 1;
 * record §U8.22), to its X record (f = 0x7E1): the same two wipe hooks
 * (record §G.24), the bare `ret` 0x29D60 and the runtime stub 0x5D812, from
 * frontend_mode_1b_step. */
static const fnm_pair k_miss_gp_u8_attract_start[] = {
    { 0x29D60u, "frontend_mode_1b_step" },
    { 0x5D812u, "frontend_mode_1b_step" },
};

/* gp-u9-win (plan gameplay-u9-u10, record 2026-10-02-gameplay-u9-u10-derivations.md
 * §W.12), measured on its full replay to its X record (f = 0xDAE): the two wipe hooks
 * of §G.24 (0x29D60, a bare `ret`, from f = 0x28D; 0x5D812, the runtime stub, from
 * f = 0x405), and the E2 animation targets the CPU's CHAOS reaches after the poked KOs
 * (unported P-track rows of reverse-e2-triage.md, anim-target class): 0x400E0 (dword 0xD2816) at
 * f = 0x59C in mode 8, 0x21044 (dword 0xE1606) at f = 0x860 and 0x21084 (dword
 * 0xE162C) at f = 0x874 in mode 9. */
static const fnm_pair k_miss_gp_u9_win[] = {
    { 0x29D60u, "frontend_mode_1b_step" },
    { 0x5D812u, "frontend_mode_1b_step" },
    { 0x400E0u, "anim_indirect" },
    { 0x21044u, "anim_indirect" },
    { 0x21084u, "anim_indirect" },
};

/* gp-u10-ending (plan gameplay-u9-u10, record 2026-10-02-gameplay-u9-u10-derivations.md
 * §W.14), measured on its full replay to its X record (f = 0x26E1): the two wipe hooks of
 * §G.24 (0x29D60, a bare `ret`, from f = 0x286; 0x5D812, the runtime stub, from f = 0x3FE),
 * the death-animation stream's targets 0x37DD4 (E2 anim-target row, dword 0xD2BCE) at
 * f = 0x14FB in mode 0xC and 0x29C78 (outside E2, record reverse-p1 §P1.2, dword 0xD2BDA)
 * at f = 0x14FE in mode 0xD, and 0x3DA50 (E2 anim-target row, dword 0xD4BEC) at f = 0x155D
 * in mode 0xF. CHAOS's reaction-0x25 callback 0x2381C (track P batch 2) and character 2's
 * reaction-0x0B callback 0x475EC (the dword 0xA4004, from f = 0x1594 in mode 0xF; track P
 * batch 3, record 2026-10-03-reverse-p3 §P3.9) are ported, so neither is a miss. */
static const fnm_pair k_miss_gp_u10_ending[] = {
    { 0x29D60u, "frontend_mode_1b_step" },
    { 0x5D812u, "frontend_mode_1b_step" },
    { 0x37DD4u, "anim_indirect" },
    { 0x29C78u, "anim_indirect" },
    { 0x3DA50u, "anim_indirect" },
};

/* The scenario named by the first line of PR_GP_SCRIPT ("# gp port script v2:
 * scenario <name>[ (cut at N)]"), and whether the script was cut (--end): a
 * cut replay ends before some misses, so it may record a subset. */
static void fnm_gp_scenario(char *name, size_t n, int *cut)
{
    const char *path = getenv("PR_GP_SCRIPT");
    char line[256] = "";
    name[0] = '\0';
    *cut = 0;
    if (path != NULL) {
        FILE *f = fopen(path, "r");
        if (f != NULL) {
            if (fgets(line, sizeof line, f) == NULL) line[0] = '\0';
            fclose(f);
        }
    }
    const char *k = strstr(line, "scenario ");
    if (k == NULL) return;
    k += 9;
    size_t i = 0;
    while (k[i] != '\0' && k[i] != ' ' && k[i] != '\n' && i + 1 < n) { name[i] = k[i]; i++; }
    name[i] = '\0';
    *cut = strstr(line, "(cut at") != NULL;
}

static int fnm_in(const fnm_pair *t, size_t len, u32 addr, const char *ctx)
{
    for (size_t i = 0; i < len; i++)
        if (t[i].addr == addr && strcmp(t[i].ctx, ctx) == 0) return 1;
    return 0;
}

#define FNM_N(t) (sizeof (t) / sizeof (t)[0])

/* The gp miss sets keyed by scenario name (the name fnm_gp_scenario reads):
 * prefix 0 matches the name exactly, 1 every name that starts with it
 * (gp-idle-loss also selects gp-idle-loss-run2; gp-u6-moves also selects
 * gp-u6-moves-b and gp-u6-moves-dry). At most one entry may match a name. */
typedef struct { const char *name; int prefix; const fnm_pair *rows; size_t n; } gp_set;
static const gp_set k_gp_sets[] = {
    { "gp-idle-loss", 1, k_miss_gp_idle_loss, FNM_N(k_miss_gp_idle_loss) },
    { "gp-u5-charsel", 0, k_miss_gp_charsel, FNM_N(k_miss_gp_charsel) },
    { "gp-u6-moves", 1, k_miss_gp_u6_moves, FNM_N(k_miss_gp_u6_moves) },
    { "gp-keys-fight", 0, k_miss_gp_keys_fight, FNM_N(k_miss_gp_keys_fight) },
    { "gp-twop", 0, k_miss_gp_twop, FNM_N(k_miss_gp_twop) },
    { "gp-u8-right-arcade", 0, k_miss_gp_u8_right_arcade, FNM_N(k_miss_gp_u8_right_arcade) },
    { "gp-u8-left-training", 0, k_miss_gp_u8_left_training, FNM_N(k_miss_gp_u8_left_training) },
    { "gp-u8-right-training", 0, k_miss_gp_u8_right_training, FNM_N(k_miss_gp_u8_right_training) },
    { "gp-u8-tug-of-war", 0, k_miss_gp_u8_tug_of_war, FNM_N(k_miss_gp_u8_tug_of_war) },
    { "gp-u8-handicap", 0, k_miss_gp_u8_handicap, FNM_N(k_miss_gp_u8_handicap) },
    { "gp-u8-endurance", 0, k_miss_gp_u8_endurance, FNM_N(k_miss_gp_u8_endurance) },
    { "gp-u8-attract-start", 0, k_miss_gp_u8_attract_start, FNM_N(k_miss_gp_u8_attract_start) },
    { "gp-u9-win", 0, k_miss_gp_u9_win, FNM_N(k_miss_gp_u9_win) },
    { "gp-u10-ending", 0, k_miss_gp_u10_ending, FNM_N(k_miss_gp_u10_ending) },
};

/* The one entry of k_gp_sets that matches the scenario name, or NULL (a
 * scenario with no entry may miss only the base pair). */
static const gp_set *fnm_gp_set(const char *name)
{
    const gp_set *hit = NULL;
    int matches = 0;
    for (size_t i = 0; i < FNM_N(k_gp_sets); i++) {
        const gp_set *e = &k_gp_sets[i];
        size_t len = strlen(e->name);
        if (e->prefix ? strncmp(name, e->name, len) == 0 : strcmp(name, e->name) == 0) {
            if (hit == NULL) hit = e;
            matches++;
        }
    }
    if (matches > 1) printf("fn-miss: %d gp miss sets match scenario %s\n", matches, name);
    CHECK(matches <= 1, "at most one gp miss set matches the scenario name");
    return matches == 1 ? hit : NULL;
}

static int fnm_known(u32 addr, const char *ctx, int frontend, const gp_set *sc)
{
    if (fnm_in(k_miss_known, FNM_N(k_miss_known), addr, ctx)) return 1;
    if (frontend && fnm_in(k_miss_frontend, FNM_N(k_miss_frontend), addr, ctx)) return 1;
    return sc != NULL && fnm_in(sc->rows, sc->n, addr, ctx);
}

int test_fn_misslog_driver(const char *env)
{
    int before = g_failures;
    int frontend = strcmp(env, "PR_FRONTEND_DUMP") == 0;
    const gp_set *sc = NULL;
    int cut = 0;
    if (strcmp(env, "PR_GP_DUMP") == 0) {
        char name[64];
        fnm_gp_scenario(name, sizeof name, &cut);
        sc = fnm_gp_set(name);
    }
    u32 want = (u32)FNM_N(k_miss_known) +
               (frontend ? (u32)FNM_N(k_miss_frontend) : 0u) +
               (sc != NULL ? (u32)sc->n : 0u);
    CHECK_EQ_INT(fn_misslog_dropped(), 0);
    if (cut) CHECK(fn_misslog_count() <= want, "a cut gp replay records no more than the pinned set");
    else CHECK_EQ_INT(fn_misslog_count(), want);
    for (u32 i = 0; i < fn_misslog_count(); i++)
        if (!fnm_known(fn_misslog_addr(i), fn_misslog_ctx(i), frontend, sc)) {
            printf("fn-miss %s: unexpected 0x%05X from %s\n", env,
                   (unsigned)fn_misslog_addr(i), fn_misslog_ctx(i));
            CHECK(0, "the driver's miss log holds only its pinned known-set");
        }
    return g_failures - before;
}

/* A second caller, so a pair differs from another by its caller only. */
static void (*fnm_probe_ctx(u32 addr))(void) { return fn_resolve(addr); }

int test_fn_misslog(void)
{
    int before = g_failures;
    /* Unregistered sentinels: above the code object, never registered. */
    const u32 a = 0x00FEDC10u, b = 0x00FEDC20u;
    CHECK((fn_resolve)(a) == NULL && (fn_resolve)(b) == NULL,
          "the sentinels are unregistered");

    /* Disarmed: nothing is recorded (the arm just cleared the log). */
    fn_misslog_arm(1);
    fn_misslog_arm(0);
    CHECK(fn_resolve(a) == NULL, "a miss still resolves to NULL");
    CHECK_EQ_INT(fn_misslog_count(), 0);

    /* Armed: 0 and a registered address are not misses. */
    fn_misslog_arm(1);
    CHECK(fn_resolve(0) == NULL, "0 resolves to NULL");
    CHECK(fn_resolve(FN_0002D62C) == fn_probe, "registered by test_mem");
    CHECK_EQ_INT(fn_misslog_count(), 0);

    /* One miss: its address, its caller and one hit. */
    CHECK(fn_resolve(a) == NULL, "the seeded miss resolves to NULL");
    CHECK_EQ_INT(fn_misslog_count(), 1);
    CHECK_EQ_INT(fn_misslog_addr(0), a);
    CHECK(strcmp(fn_misslog_ctx(0), "test_fn_misslog") == 0, "the caller is recorded");
    CHECK_EQ_INT(fn_misslog_hits(0), 1);
    CHECK(fn_misslog_has(a) && !fn_misslog_has(b), "has() names the recorded address");

    /* The same pair again counts a hit; another caller is another pair. */
    (void)fn_resolve(a);
    CHECK_EQ_INT(fn_misslog_count(), 1);
    CHECK_EQ_INT(fn_misslog_hits(0), 2);
    (void)fnm_probe_ctx(a);
    CHECK_EQ_INT(fn_misslog_count(), 2);
    CHECK(strcmp(fn_misslog_ctx(1), "fnm_probe_ctx") == 0, "the second caller is recorded");
    CHECK_EQ_INT(fn_misslog_hits(1), 1);
    (void)fn_resolve(b);
    CHECK_EQ_INT(fn_misslog_count(), 3);
    CHECK_EQ_INT(fn_misslog_addr(2), b);

    /* Past FN_MISSLOG_MAX pairs a miss is counted as dropped, not stored. */
    for (u32 i = 3; i < FN_MISSLOG_MAX; i++) (void)fn_resolve(0x00FEE000u + i * 4u);
    CHECK_EQ_INT(fn_misslog_count(), FN_MISSLOG_MAX);
    CHECK_EQ_INT(fn_misslog_dropped(), 0);
    (void)fn_resolve(0x00FEF000u);
    CHECK_EQ_INT(fn_misslog_count(), FN_MISSLOG_MAX);
    CHECK_EQ_INT(fn_misslog_dropped(), 1);

    /* Re-arming clears both. */
    fn_misslog_arm(1);
    CHECK_EQ_INT(fn_misslog_count(), 0);
    CHECK_EQ_INT(fn_misslog_dropped(), 0);
    fn_misslog_arm(0);

    /* The --check run's log (main.c writes it after the run; `make verify`
     * runs --check before this suite): exactly the pinned known-set. */
    FILE *f = fopen("frames/fn_miss.txt", "r");
    if (f == NULL) {
        printf("test_fn_misslog: frames/fn_miss.txt absent, --check log not compared\n");
    } else {
        char line[256];
        int n = 0;
        while (fgets(line, sizeof line, f) != NULL) {
            unsigned addr = 0;
            char ctx[128];
            if (sscanf(line, "0x%x %127s", &addr, ctx) != 2) {
                printf("fn_miss.txt: unexpected line %s", line);
                CHECK(0, "every --check log line is a recorded pair");
                continue;
            }
            n++;
            if (!fnm_known((u32)addr, ctx, 0, NULL)) {
                printf("fn_miss.txt: unexpected 0x%05X from %s\n", addr, ctx);
                CHECK(0, "the --check miss log holds only the pinned known-set");
            }
        }
        fclose(f);
        CHECK_EQ_INT(n, (int)(sizeof k_miss_known / sizeof k_miss_known[0]));
    }
    return g_failures - before;
}

/* ---- the call seam (mem.h PR_SEAM; record 2026-10-01-reverse-e3 §E3.3) ---- */

static u32 s_seam_n, s_seam_addr[4], s_seam_nargs[4], s_seam_args[4][6];
static int s_seam_stub;                 /* the probe's answer: 1 stubs the call, 0 runs the body */

static int seam_probe(u32 addr, u32 nargs, const u32 *args, u32 *eax)
{
    if (s_seam_n < 4) {
        s_seam_addr[s_seam_n] = addr;
        s_seam_nargs[s_seam_n] = nargs;
        for (u32 i = 0; i < nargs && i < 6; i++) s_seam_args[s_seam_n][i] = args[i];
    }
    s_seam_n++;
    *eax = 0x5Au;
    return s_seam_stub;
}

/* Record 2026-10-02-reverse-p2 §P2.7: registering one (address, function)
 * pair more times than the table holds (FN_TABLE_MAX = 1300) adds it once;
 * before the fix the 1301st call aborted the run. The skip matches on the
 * pair, not on either half: a second function for the same address is still
 * appended (fn_origin finds it, fn_resolve keeps the first), and the same
 * function at a second address is too (an address-only skip drops the first,
 * a function-only skip the second). */
static void fnreg_probe_a(void) {}
static void fnreg_probe_b(void) {}

int test_fn_register_repeats(void)
{
    int before = g_failures;
    for (int i = 0; i < 1301; i++) fn_register(0xF00F8u, fnreg_probe_a);
    CHECK(fn_resolve(0xF00F8u) == fnreg_probe_a, "the repeated pair resolves");
    fn_register(0xF00F8u, fnreg_probe_b);
    CHECK(fn_resolve(0xF00F8u) == fnreg_probe_a, "the first pair for an address wins");
    CHECK(fn_origin(fnreg_probe_b) == 0xF00F8u, "a second function for an address is appended, not skipped");
    fn_register(0xF00FCu, fnreg_probe_a);
    CHECK(fn_resolve(0xF00FCu) == fnreg_probe_a, "the same function at a second address is appended, not skipped");
    return g_failures - before;
}

int test_call_seam(void)
{
    int before = g_failures;
    const u32 rec = 0x03F00000u;              /* above the image, inside mem[] */
    const u32 miss = 0x00FEDC30u;             /* above the code object, never registered */

    /* Stubbed: the hook gets the original address and the C arguments in order, and the body
     * does not run (the 0xA5 sentinels at +0x0C and +0x24, which the body zeroes and sets to the
     * frame, survive). */
    mem_fill(rec, 0xA5u, 0x68u);
    pr_seam = seam_probe;
    s_seam_n = 0u;
    s_seam_stub = 1;
    actors_anim_begin(rec, 0x000EB58Cu, 0x40400000u);
    CHECK_EQ_INT(s_seam_n, 1);
    CHECK_EQ_INT(s_seam_addr[0], 0x2BC30);
    CHECK_EQ_INT(s_seam_nargs[0], 3);
    CHECK(s_seam_args[0][0] == rec && s_seam_args[0][1] == 0x000EB58Cu
          && s_seam_args[0][2] == 0x40400000u, "the arguments in C order");
    CHECK_EQ_INT(DSD(rec + 0x0Cu), 0xA5A5A5A5u);
    CHECK_EQ_INT(DSD(rec + 0x24u), 0xA5A5A5A5u);

    /* PR_SEAM_RET returns the hook's EAX when it stubs, and the body's value when it runs
     * (sound_voice(0) returns 0 at 0x2C401). */
    s_seam_n = 0u;
    sound_voice_log_reset();
    CHECK_EQ_INT(sound_voice(0u), 0x5A);
    CHECK_EQ_INT((int)sound_voice_log_count(), 0);       /* the stubbed call leaves no log entry */
    s_seam_stub = 0;
    CHECK_EQ_INT(sound_voice(0u), 0);
    CHECK_EQ_INT((int)sound_voice_log_count(), 1);       /* the body ran and logged id 0 ... */
    sound_voice_log_reset();                             /* ... which no later test may see */
    CHECK_EQ_INT(s_seam_n, 2);
    CHECK(s_seam_addr[0] == 0x2C3FCu && s_seam_addr[1] == 0x2C3FCu, "both calls are seen");
    CHECK(s_seam_nargs[1] == 1u && s_seam_args[1][0] == 0u, "with the voice id");

    /* actor_spawn reports desc as a mem[] offset when it lies in mem[], and a sentinel no spec
     * can name when it does not (0x2F5A0 passes a C stack array). */
    {
        static const u32 c_desc[4] = { 0u, 0u, 0u, 0u };     /* not in mem[] */
        s_seam_n = 0u;
        s_seam_stub = 1;
        CHECK_EQ_INT((int)actor_spawn((const u32 *)(mem + 0x9AD08u), 1u, 2u, 3u, 4u), 0x5A);
        CHECK_EQ_INT((int)actor_spawn(c_desc, 1u, 2u, 3u, 4u), 0x5A);
        CHECK_EQ_INT(s_seam_n, 2);
        CHECK(s_seam_addr[0] == 0x2AE14u && s_seam_nargs[0] == 5u && s_seam_args[0][0] == 0x9AD08u
              && s_seam_args[0][1] == 1u && s_seam_args[0][4] == 4u, "a desc in mem[] is its offset");
        CHECK(s_seam_args[1][0] == 0xFFFFFFFFu && s_seam_args[1][3] == 3u, "a C array desc is the sentinel");
    }

    /* fn_resolve: an unregistered address is seen with no arguments; a registered one (test_mem's
     * FN_0002D62C) and 0 are not. */
    s_seam_n = 0u;
    s_seam_stub = 1;
    fn_misslog_arm(1);                       /* the report must not depend on the miss log, nor change it */
    CHECK(fn_resolve(miss) == NULL, "the miss still resolves to NULL");
    CHECK(fn_resolve(FN_0002D62C) == fn_probe, "registered by test_mem");
    CHECK(fn_resolve(0u) == NULL, "0 resolves to NULL");
    CHECK_EQ_INT(s_seam_n, 1);
    CHECK(s_seam_addr[0] == miss && s_seam_nargs[0] == 0u, "the miss, with no arguments");
    CHECK(fn_resolve(miss) == NULL, "the repeated miss (the log's early-return path)");
    CHECK_EQ_INT(s_seam_n, 2);                           /* reported on the repeat too, not only the first insert */
    CHECK_EQ_INT((int)fn_misslog_count(), 1);            /* one pair, two hits: the hook adds none and hides none */
    CHECK(fn_misslog_addr(0) == miss, "the log holds the miss");
    CHECK_EQ_INT((int)fn_misslog_hits(0), 2);
    fn_misslog_arm(0);

    pr_seam = NULL;
    CHECK_EQ_INT((int)sound_voice_log_count(), 0);       /* nothing this test did is left in the voice log */
    return g_failures - before;
}

/* ---- test_le.c ---- */

#define EXE "data/game/C/PRAGE.EXE"

int test_le(void)
{
    int before = g_failures;

    CHECK(mem_load_le(EXE, NULL) == 1, "PRAGE.EXE loads");
    CHECK(mem_load_le("data/game/C/INDEX", NULL) == 0, "a non-LE file is rejected");

    /* The LE header and object table are already verified by tools/le_info.py:
     *   LE header 0x290A4, page size 0x1000, 213 pages,
     *   object 0 code  base 0x10000 size 0x63B15 100 pages
     *   object 1 data  base 0x80000 size 0x8B0D0 113 pages
     * After loading, the data object's string table must be present. These are
     * real bytes from the shipped file (port/decomp/prage.strings.csv lists
     * them at DS:0x0004, 0x0010, 0x001C), so they prove the page map and object
     * mapping are right before any fixup is applied. */
    CHECK(mem_in_range(DATA_BASE, 0x8B0D0), "data object fits");
    CHECK(memcmp(mem + DATA_BASE + 4, "SB16.DIG", 9) == 0,
          "DS:0x0004 is the SB16.DIG string");
    CHECK(memcmp(mem + DATA_BASE + 0x10, "SBPRO.DIG", 10) == 0,
          "DS:0x0010 is the SBPRO.DIG string");
    CHECK(memcmp(mem + DATA_BASE + 0x1C, "SBLASTER.DIG", 13) == 0,
          "DS:0x001C is the SBLASTER.DIG string");

    /* The data object's virtual size (0x8B0D0) exceeds its file-backed pages
     * (113 * 0x1000 == 0x71000). Poison that BSS tail, reload, and require the
     * loader to have zeroed it: this fails if the tail is left to whatever
     * mem[] held before, which a zero-initialised global would disguise. */
    {
        u32 mapped = 113u * 0x1000u;
        mem_fill(DATA_BASE + mapped, 0xFF, 0x8B0D0 - mapped);
        CHECK(mem_load_le(EXE, NULL) == 1, "reload after poisoning the BSS tail");
        int dirty = 0;
        for (u32 i = 0; i < 0x8B0D0 - mapped; i++)
            if (mem[DATA_BASE + mapped + i] != 0) dirty = 1;
        CHECK(!dirty, "data object BSS tail is zero-filled, not stale");
    }
    /* mem[] must equal Ghidra's fixup-applied image of the data object.
     * The oracle is not committed (see Task 3 Step 1), so it is required only
     * when PR_ORACLE_REQUIRED=1 and otherwise skipped with a notice. */
    {
        FILE *g = fopen("port/tests/ghidra_data.bin", "rb");
        if (!g && getenv("PR_ORACLE_REQUIRED")) {
            CHECK(0, "PR_ORACLE_REQUIRED=1 but the Ghidra oracle is missing");
        } else if (!g) {
            printf("SKIP data-object oracle (generate it per Task 3 Step 1, "
                   "or set PR_ORACLE_REQUIRED=1 to require it)\n");
        } else {
            static u8 oracle[0x8B0D0];
            size_t got = fread(oracle, 1, sizeof oracle, g);
            fclose(g);
            CHECK_EQ_INT(got, sizeof oracle);
            int diff = -1;
            for (size_t i = 0; i < sizeof oracle && diff < 0; i++)
                if (mem[DATA_BASE + i] != oracle[i]) diff = (int)i;
            CHECK(diff < 0, "data object matches the Ghidra image");
            if (diff >= 0) printf("  first difference at DS:0x%05x\n", diff);
        }
    }

    return g_failures - before;
}

/* ---- test_res.c ---- */

/* The loader's dump seam (record §45-A): the calls, the text pixels already on
 * the aperture at each call, and DS_00101508 then (the read's stall comes
 * after the hook). */
static int res_hook_n;
static u32 res_hook_px, res_hook_1508;
static void res_hook(void)
{
    const u8 *ap = gfx_aperture();
    res_hook_n++;
    res_hook_px = 0;
    for (u32 b = 192u * 320u; b < 198u * 320u; b++) if (ap[b] != 0u) res_hook_px++;
    res_hook_1508 = DSD(DS_00101508);
}

/* named-gaps C (record 2026-09-30-named-gaps-c-derivations.md §C.1): res.c's
 * PORT: allocation-failure seam. Armed with n, the n-th request from then on
 * returns 0 and leaves the heap where it was (the mem_in_range arm of
 * res_alloc), and the seam disarms itself; n = 0 disarms. The heap ends where
 * it started: every request that succeeds here is size 0 (res_block_alloc(0)
 * aligns the heap and does not advance it), and the one sized request fails. */
static void check_res_fail_seam(void)
{
    const u32 a = res_block_alloc(0u);
    CHECK(a != 0u, "the bump heap has room");

    res_fail_alloc_nth(3u);
    CHECK_EQ_INT((int)res_fail_alloc_left(), 3);
    CHECK_EQ_INT((int)res_block_alloc(0u), (int)a);       /* request 1 succeeds */
    CHECK_EQ_INT((int)res_fail_alloc_left(), 2);
    CHECK_EQ_INT((int)res_block_alloc(0u), (int)a);       /* request 2 succeeds */
    CHECK_EQ_INT((int)res_block_alloc(0x100u), 0);        /* request 3 fails */
    CHECK_EQ_INT((int)res_fail_alloc_left(), 0);          /* one-shot: disarmed */
    CHECK_EQ_INT((int)res_block_alloc(0u), (int)a);       /* the failure took nothing */

    res_fail_alloc_nth(1u);
    res_fail_alloc_nth(0u);                               /* n = 0 disarms */
    CHECK_EQ_INT((int)res_block_alloc(0u), (int)a);
    CHECK_EQ_INT((int)res_fail_alloc_left(), 0);          /* an unarmed request leaves it */
}

int test_res(void)
{
    int before = g_failures;

    u32 n = res_load_index("data/game/C", "data/game/C/INDEX");
    /* docs/FORMATS.md: the shipped C/INDEX has 69 entries. */
    CHECK_EQ_INT(n, 69);
    CHECK_EQ_INT(res_count(), 69);

    /* The table pointer and count live where 0x1B120 puts them. */
    CHECK(mem_in_range(DSD(DS_001014E0), 69 * 20), "table pointer is in range");
    CHECK_EQ_INT(DSD(DS_001014F0), 69);

    /* Every entry got a data block, and DS_001014F8 is the largest size. */
    u32 biggest = 0, have_data = 0;
    for (u32 i = 0; i < res_count(); i++) {
        if (DSD(DSD(DS_001014E0) + i * 20 + 16) != 0) have_data++;
        if (res_size(i) > biggest) biggest = res_size(i);
    }
    CHECK_EQ_INT(have_data, 69);
    CHECK_EQ_INT(DSD(DS_001014F8), biggest);

    /* Names are 12-byte zero-padded ASCII, so compare the first NUL only. */
    int found = 0;
    for (u32 i = 0; i < res_count(); i++) {
        const char *name = res_name(i);
        if (strncmp(name, "s16title.gra", 12) == 0 ||
            strncmp(name, "S16TITLE.GRA", 12) == 0) {
            found = 1;
            /* The size field must equal the on-disk file size. */
            char path[256];
            snprintf(path, sizeof path, "data/game/C/%.*s", 12, name);
            FILE *f = fopen(path, "rb");
            CHECK(f != NULL, "resource file opens");
            if (f) {
                fseek(f, 0, SEEK_END);
                CHECK_EQ_INT(res_size(i), ftell(f));
                fclose(f);
            }
            /* Its bytes were actually read, and a GRA begins with the chunk
             * header: u16 type = 2, then the magic "43". */
            u8 *data = res_resolve(res_handle(i, 0));
            CHECK(data != NULL, "handle resolves to the resource data");
            if (data) {
                CHECK_EQ_INT(data[0], 2);
                CHECK_EQ_INT(data[1], 0);
                CHECK_EQ_INT(data[2], '4');
                CHECK_EQ_INT(data[3], '3');
                /* Offset arithmetic: handle low 23 bits index into the data. */
                CHECK_EQ_INT(res_resolve(res_handle(i, 4)), data + 4);
            }
        }
    }
    CHECK(found, "s16title.gra is in the index");

    /* The loader's residency state (0x1B544/0x1B3AC). The shipped INDEX
     * preloads only s16fonts (1) and s16statu (2); every other entry is read
     * on first resolve, which is when 0x1B3AC presents the `- LOADING -`
     * screen and sets the master loop's full-copy flag DS_001014FC. The flag
     * is seeded to 0 before each step so a missing presentation cannot pass on
     * a stale value. */
    {
        u32 table = DSD(DS_001014E0);
        /* The INDEX's own preload byte: 0x01 vs 0x02. */
        CHECK_EQ_INT((int)(res_flags(1) & 1u), 1);
        CHECK_EQ_INT((int)(res_flags(2) & 1u), 1);
        CHECK_EQ_INT((int)(res_flags(0) & 1u), 0);

        /* A preloaded entry is marked read by the init walk (0x1B47A) and its
         * resolve presents nothing (0x1B569's resident arm). */
        CHECK((DSD(table + 1u * 20u + 12u) & 0x20000000u) != 0u,
              "preloaded entry marked read at init");
        DSD(DS_001014FC) = 0;
        CHECK(res_resolve(res_handle(1u, 0)) != NULL, "s16fonts resolves");
        CHECK_EQ_INT((int)DSD(DS_001014FC), 0);

        /* A lazy entry stays unread until its first resolve: that resolve
         * presents (0x1B3AC's head) and marks it read (0x1B47A). The read's
         * stall advances DS_00101508 by bytes/132674 (records 9.6, 45-A) and its tail
         * re-syncs DS_0010150C to it (0x1B45F/0x1B464), so the master loop's
         * gate (0x25643) passes on the load frame. Seed the pair to different
         * sentinels: a missing stall leaves 1508 at 0x5678, a missing re-sync
         * leaves 150C at 0x1234. */
        CHECK((DSD(table + 0u * 20u + 12u) & 0x20000000u) == 0u,
              "lazy entry unread after init");
        DSD(DS_001014FC) = 0;
        DSD(DS_00101508) = 0x5678;
        DSD(DS_00101500) = 0x9ABCu;
        DSD(DS_0010150C) = 0x1234;
        CHECK(res_resolve(res_handle(0u, 0)) != NULL, "s16slabs resolves");
        CHECK_EQ_INT((int)DSD(DS_001014FC), 1);
        /* The exact delta, not merely "advanced": the stall is
         * ceil(res_size(0)/RES_READ_BYTES_PER_TICK) ticks (res.c). The rate is
         * asserted against its literal first — the delta expression below is
         * rate-relative, so without this a changed rate would move both sides
         * together and the pin would be vacuous. res_size(0) is read from the
         * shipped INDEX, so the assertion tracks the real payload. */
        CHECK_EQ_INT((int)RES_READ_BYTES_PER_TICK, 132674);
        CHECK_EQ_INT((int)DSD(DS_00101508),
                     0x5678 + (int)((res_size(0u) + RES_READ_BYTES_PER_TICK - 1u)
                                    / RES_READ_BYTES_PER_TICK));
        CHECK_EQ_INT((int)DSD(DS_00101500),
                     0x9ABC + (int)((res_size(0u) + RES_READ_BYTES_PER_TICK - 1u)
                                    / RES_READ_BYTES_PER_TICK));   /* the ISR's 0x1BE16 pair */
        CHECK_EQ_INT((int)DSD(DS_0010150C), (int)DSD(DS_00101508));
        CHECK((DSD(table + 0u * 20u + 12u) & 0x20000000u) != 0u,
              "lazy entry marked read by its first resolve");

        /* The second resolve presents nothing (0x1B57F/0x1B585). */
        DSD(DS_001014FC) = 0;
        CHECK(res_resolve(res_handle(0u, 4)) != NULL, "s16slabs resolves again");
        CHECK_EQ_INT((int)DSD(DS_001014FC), 0);
    }

    /* The loader's presentation pixels. String 489 decodes to `- LOADING -`
     * and 0x1C65C/0x1C5E8 blit it at (0,192) — the expression 0x1B3AC uses is
     * game_string_get(0x1E9) with seeds (0, 0xE6). The composite buffer is
     * pointed at a zeroed scratch and the row-192 offset built, so the first
     * resolve of a fresh lazy entry (index 3 s16glife) draws where the check
     * can see it. The capture's overlay is 166 non-zero pixels at rows
     * 192..197, columns 0..85; the box is asserted, not just the count, so a
     * string-id, font or position regression cannot pass. */
    game_string_table_load("data/game/C");
    CHECK(strcmp((const char *)game_string_get(0x1E9u), "- LOADING -") == 0,
          "string 489 decodes to - LOADING -");
    {
        /* PORT: the loader's text blit is 0x51ED8, whose destination base is the
         * literal VGA aperture 0xA0000 (`add edi, 0xa0000`), not the back buffer
         * the renderer's blit 0x51E5C uses (`add edi, [0x687a4]`). So the text
         * must land in gfx_aperture() and must NOT touch DS_000E87A4. */
        u8 *scratch = gfx_aperture();
        u32 saved_base = DSD(DS_000E87A4);
        u32 saved_row = DSD(DS_001088F8 + 192u * 4u);
        const u32 backbuf = 0x3F60000u;   /* above the resource heap */
        memset(scratch, 0, 0xFA00u);
        mem_fill(backbuf, 0xAAu, 0xFA00u);   /* a sentinel, not the 0 post-state */
        DSD(DS_000E87A4) = backbuf;
        DSD(DS_001088F8 + 192u * 4u) = 192u * 0x140u;
        DSD(DS_001014FC) = 0;
        CHECK(res_resolve(res_handle(3u, 0)) != NULL, "the fresh lazy resolve draws");
        CHECK_EQ_INT((int)DSD(DS_001014FC), 1);
        u32 pixels = 0, rowlo = 200u, rowhi = 0u, collo = 320u, colhi = 0u;
        for (u32 y = 192u; y < 198u; y++) {
            for (u32 x = 0u; x < 320u; x++) {
                if (scratch[y * 320u + x] != 0u) {
                    pixels++;
                    if (y < rowlo) rowlo = y;
                    if (y > rowhi) rowhi = y;
                    if (x < collo) collo = x;
                    if (x > colhi) colhi = x;
                }
            }
        }
        CHECK_EQ_INT((int)pixels, 166);
        CHECK_EQ_INT((int)rowlo, 192);
        CHECK_EQ_INT((int)rowhi, 197);
        CHECK_EQ_INT((int)collo, 0);
        CHECK_EQ_INT((int)colhi, 85);
        /* The back buffer keeps its sentinel: 0x51ED8 never writes it. */
        CHECK_EQ_INT((int)mem[backbuf + 192u * 320u], 0xAA);
        CHECK_EQ_INT((int)mem[backbuf + 197u * 320u + 85u], 0xAA);
        DSD(DS_000E87A4) = saved_base;
        DSD(DS_001088F8 + 192u * 4u) = saved_row;
    }

    /* The dump seam: a first read calls the hook once, with the loader's 166
     * text pixels already on the aperture and before the stall advances
     * DS_00101508; a second resolve, or a NULL hook, calls nothing. Every
     * byte it touches is restored: the data object (the tick pair,
     * DS_001014FC, the row pointer, the font palette's records), entries 4
     * and 6's +0xC, the aperture and the DAC. */
    {
        static u8 sv_data[0x8B0D0], sv_ap[320u * 200u], sv_dac[256][3];
        u32 table = DSD(DS_001014E0);
        u32 sv_e4 = DSD(table + 4u * 20u + 12u), sv_e6 = DSD(table + 6u * 20u + 12u);
        memcpy(sv_data, mem + DATA_BASE, sizeof sv_data);
        memcpy(sv_ap, gfx_aperture(), sizeof sv_ap);
        memcpy(sv_dac, gfx_dac, sizeof sv_dac);
        DSD(DS_001088F8 + 192u * 4u) = 192u * 0x140u;   /* as the pixel test above */
        CHECK((DSD(table + 4u * 20u + 12u) & 0x20000000u) == 0u, "s16jap unread");
        CHECK((DSD(table + 6u * 20u + 12u) & 0x20000000u) == 0u, "s16snd2 unread");
        memset(gfx_aperture(), 0, 320u * 200u);
        res_hook_n = 0;
        res_hook_px = 0x7777u;
        res_hook_1508 = 0x7777u;
        DSD(DS_00101508) = 0x5678u;
        res_set_screen_hook(res_hook);
        CHECK(res_resolve(res_handle(4u, 0)) != NULL, "s16jap resolves");
        CHECK_EQ_INT(res_hook_n, 1);
        CHECK_EQ_INT((int)res_hook_px, 166);
        CHECK_EQ_INT((int)res_hook_1508, 0x5678);
        CHECK(DSD(DS_00101508) != 0x5678u, "the stall follows the hook");
        CHECK(res_resolve(res_handle(4u, 8)) != NULL, "s16jap resolves again");
        CHECK_EQ_INT(res_hook_n, 1);
        res_set_screen_hook(NULL);
        CHECK(res_resolve(res_handle(6u, 0)) != NULL, "s16snd2 resolves unhooked");
        CHECK_EQ_INT(res_hook_n, 1);
        CHECK((DSD(table + 6u * 20u + 12u) & 0x20000000u) != 0u, "s16snd2 read");
        DSD(table + 4u * 20u + 12u) = sv_e4;
        DSD(table + 6u * 20u + 12u) = sv_e6;
        memcpy(gfx_dac, sv_dac, sizeof sv_dac);
        memcpy(gfx_aperture(), sv_ap, sizeof sv_ap);
        memcpy(mem + DATA_BASE, sv_data, sizeof sv_data);
    }

    CHECK(res_resolve(0xFFFFFFFFu) == NULL, "an out-of-range handle resolves to NULL");

    /* The Smacker movies are not in INDEX, so they load by name. The shipped
     * files are uppercase (TWI5.SMK) and the callers use lowercase, so this
     * exercises the case-insensitive scan. */
    u32 off = 0, size = 0;
    CHECK_EQ_INT(res_load_file("data/game/C", "twi5.smk", &off, &size), 1);
    CHECK_EQ_INT(size, 1208576);
    CHECK(mem_in_range(off, size), "movie bytes got a block in mem[]");
    CHECK_EQ_INT(mem[off], 'S');
    CHECK_EQ_INT(mem[off + 1], 'M');
    CHECK_EQ_INT(mem[off + 2], 'K');
    CHECK_EQ_INT(res_load_file("data/game/C", "twg.smk", &off, &size), 1);
    CHECK_EQ_INT(size, 31048);

    /* A missing file fails without touching either output. */
    u32 miss_off = 0xDEADBEEFu, miss_size = 0xFEEDFACEu;
    CHECK_EQ_INT(res_load_file("data/game/C", "nope.smk", &miss_off, &miss_size), 0);
    CHECK_EQ_INT(miss_off, 0xDEADBEEFu);
    CHECK_EQ_INT(miss_size, 0xFEEDFACEu);

    /* Size boundaries, both rejected with the outputs untouched. A sparse file
     * gives the length with no large fixture on disk. 128 MiB is above MEM_SIZE,
     * so the allocator's bound is hit before any read. 4 GiB + 1 is the u32
     * truncation case that used to pass that bound (the low 32 bits are 0) and
     * then over-read past mem[]; the size is now rejected before allocating, so
     * no read happens — if that guard regresses this test faults rather than
     * silently corrupting the flat space. */
    char big_path[] = "/tmp/pr_resbig_XXXXXX";
    int big_fd = mkstemp(big_path);
    CHECK(big_fd >= 0, "sparse fixture created");
    if (big_fd >= 0) {
        u32 big_off = 0xDEADBEEFu, big_size = 0xFEEDFACEu;
        CHECK_EQ_INT(ftruncate(big_fd, 0x8000000), 0);
        CHECK_EQ_INT(res_load_file("/tmp", big_path + 5, &big_off, &big_size), 0);
        CHECK_EQ_INT(big_off, 0xDEADBEEFu);
        CHECK_EQ_INT(big_size, 0xFEEDFACEu);

        big_off = 0xDEADBEEFu, big_size = 0xFEEDFACEu;
        CHECK_EQ_INT(ftruncate(big_fd, 0x100000000L), 0);
        CHECK_EQ_INT(res_load_file("/tmp", big_path + 5, &big_off, &big_size), 0);
        CHECK_EQ_INT(big_off, 0xDEADBEEFu);
        CHECK_EQ_INT(big_size, 0xFEEDFACEu);

        close(big_fd);
        unlink(big_path);
    }

    check_res_fail_seam();

    return g_failures - before;
}

/* ---- test_gra.c ---- */

/* Scratch base for GRA images. The plan sketched 0xE00000, but that is NOT
 * clear of the data: after res_load_index the resource heap runs from
 * 0x10B0D0 up to about 0x2A8C548 (~44.6 MB), so a GRA loaded at 0xE00000
 * would land on top of loaded resources. Start above the heap instead. */
#define SCRATCH 0x3000000u

static int load_at(const char *path, u32 at, u32 *len_out)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (!mem_in_range(at, (u32)sz)) { fclose(f); return 0; }
    size_t got = fread(mem + at, 1, (size_t)sz, f);
    fclose(f);
    *len_out = (u32)sz;
    return got == (size_t)sz;
}

static void check_chain(const char *name, int want_count, const u16 *types,
                        const u32 *lens)
{
    char path[256];
    snprintf(path, sizeof path, "data/game/C/%s", name);
    u32 len = 0;
    CHECK(load_at(path, SCRATCH, &len), "gra file loads");
    GraChunk chunks[8];
    int count = -1;
    CHECK(gra_open(SCRATCH, len, chunks, 8, &count), "valid chain accepted");
    CHECK_EQ_INT(count, want_count);
    for (int i = 0; i < want_count && i < count; i++) {
        CHECK_EQ_INT(chunks[i].type, types[i]);
        CHECK_EQ_INT(chunks[i].body_len, lens[i]);
        CHECK_EQ_INT(chunks[i].body_off, chunks[i].off + 8);
    }
}

static void put_header(u32 at, u16 type, const char magic[2], u32 next)
{
    DSW(at) = type;
    DSB(at + 2) = (u8)magic[0];
    DSB(at + 3) = (u8)magic[1];
    DSD(at + 4) = next;
}

static void check_gra_sprites(void)
{
    /* The sprite table itself is static in the data object and already
     * resident, but resolving its handles needs the resource INDEX. test_res
     * loads it earlier in the run_tests order; load it here too (guarded, so
     * the bump allocator is never asked for it twice) to make this test
     * independent of that ordering. Needs platform/res.h. */
    if (DSD(DS_001014F0) == 0)
        CHECK(res_load_index("data/game/C", "data/game/C/INDEX") > 0,
              "resource index loads");

    /* The first entry must resolve, and its descriptor must be self-consistent.
     * Its header values are read from the shipped asset (s16statu.gra): 30x27
     * with X pivot 15 and Y pivot 13, positive height => RLE, not the raw
     * marker. */
    GraSprite s;
    u32 dh = 0;
    CHECK_EQ_INT(gra_sprite_lookup(0, &s, &dh), 1);
    CHECK(dh != 0, "sprite 0 has a descriptor handle");
    CHECK_EQ_INT(s.width, 30);
    CHECK_EQ_INT(s.height, 27);
    CHECK_EQ_INT(s.xorg, 15);
    CHECK_EQ_INT(s.yorg, 13);
    const u8 *px = NULL;
    CHECK_EQ_INT(gra_sprite_pixels(s.pixel_handle, &px), 1);
    CHECK(px != NULL, "pixel handle resolves");

    /* A garbage handle must be rejected. */
    CHECK_EQ_INT(gra_sprite_open(0x7FFFFFFFu, &s), 0);
    CHECK_EQ_INT(gra_sprite_pixels(0x7FFFFFFFu, &px), 0);

    /* The table is the documented 18,443 entries, indices 0..18442. The port
     * masks the id to 0x7FFF exactly as 0x14268 does and applies no other
     * bound. The table's end is NOT discoverable from the data — the dwords
     * after it are nonzero but are not handles (the first all-zero dword is at
     * index 18535, and 18534 reads 0x128D) — so the length is pinned from the
     * disassembly, not inferred. */
    int n = 0;
    for (u32 i = 0; i < 18443u; i++)
        if (DSD(DS_000A8B30 + i * 4u) != 0) n++;
    CHECK_EQ_INT(n, 18443);
}

int test_gra(void)
{
    int before = g_failures;

    check_gra_sprites();

    check_chain("S16FONTS.GRA", 3, (const u16[]){2, 5, 6},
                (const u32[]){39244, 144, 4644});
    check_chain("S16CAGE.GRA", 2, (const u16[]){2, 6},
                (const u32[]){187712, 324});
    check_chain("S16TITLE.GRA", 3, (const u16[]){2, 5, 6},
                (const u32[]){1501656, 2928, 1380});
    check_chain("S16COBSD.GRA", 1, (const u16[]){2},
                (const u32[]){145427});

    /* Malformed chains must be rejected rather than walked out of bounds. */
    GraChunk c[4];
    int n = -1;
    mem_fill(SCRATCH, 0, 64);
    put_header(SCRATCH, 2, "44", 0);
    CHECK_EQ_INT(gra_open(SCRATCH, 64, c, 4, &n), 0);   /* bad magic */

    put_header(SCRATCH, 2, "43", 16);
    put_header(SCRATCH + 16, 6, "43", 16);
    CHECK_EQ_INT(gra_open(SCRATCH, 64, c, 4, &n), 0);   /* next not increasing */

    put_header(SCRATCH, 2, "43", 1000);
    CHECK_EQ_INT(gra_open(SCRATCH, 64, c, 4, &n), 0);   /* next past file */

    put_header(SCRATCH, 2, "43", 0);
    CHECK_EQ_INT(gra_open(SCRATCH, 4, c, 4, &n), 0);    /* header truncated */

    put_header(SCRATCH, 2, "43", 4);
    CHECK_EQ_INT(gra_open(SCRATCH, 64, c, 4, &n), 0);   /* next inside header */
    CHECK_EQ_INT(n, 0);                                 /* count zeroed on failure */

    CHECK_EQ_INT(gra_open(MEM_SIZE - 4, 64, c, 4, &n), 0); /* file off out of mem */
    CHECK_EQ_INT(n, 0);

    /* `max` caps output; the walk stops without overrunning `out`. */
    u32 len = 0;
    CHECK(load_at("data/game/C/S16FONTS.GRA", SCRATCH, &len), "gra loads for cap test");
    CHECK_EQ_INT(gra_open(SCRATCH, len, c, 2, &n), 1);
    CHECK_EQ_INT(n, 2);

    /* Palette bank: S16FONTS is 27 colours over a 144-byte type-5 body, and
     * the first colour word 0x0090D0F0 packs to (0x3C, 0x34, 0x24). */
    {
        u8 pal[4096];
        int cols = -1;
        CHECK(load_at("data/game/C/S16FONTS.GRA", SCRATCH, &len), "fonts loads");
        CHECK(gra_open(SCRATCH, len, c, 4, &n), "fonts chain");
        CHECK(gra_decode_palette(SCRATCH, c, n, pal, sizeof pal, &cols), "fonts palette decodes");
        CHECK_EQ_INT(cols, 27);
        CHECK_EQ_INT(pal[0], 0x3C);
        CHECK_EQ_INT(pal[1], 0x34);
        CHECK_EQ_INT(pal[2], 0x24);

        CHECK(load_at("data/game/C/S16TITLE.GRA", SCRATCH, &len), "title loads");
        CHECK(gra_open(SCRATCH, len, c, 4, &n), "title chain");
        CHECK(gra_decode_palette(SCRATCH, c, n, pal, sizeof pal, &cols), "title palette decodes");
        CHECK_EQ_INT(cols, 720);
    }

    /* The largest bank in the shipped set is S16ATTRC's 1292 colours (3876 B).
     * An exactly-sized buffer succeeds; an undersized buffer is rejected with
     * *count left 0, never written past its capacity. */
    {
        u8 big[1292 * 3];
        u8 tiny[3 * 8];
        int cols = -1;
        CHECK(load_at("data/game/C/S16ATTRC.GRA", SCRATCH, &len), "attrc loads");
        CHECK(gra_open(SCRATCH, len, c, 4, &n), "attrc chain");
        CHECK(gra_decode_palette(SCRATCH, c, n, big, sizeof big, &cols), "largest bank fits");
        CHECK_EQ_INT(cols, 1292);
        CHECK_EQ_INT(gra_decode_palette(SCRATCH, c, n, tiny, sizeof tiny, &cols), 0);
        CHECK_EQ_INT(cols, 0);
    }

    /* A single frame: FONTS frame 0 is 13x7, consumes 92 RLE bytes and has
     * 88 of 91 pixels opaque (Task 8's measured index model). Transparent
     * pixels are written as index 0, so counting non-zero bytes is the
     * opaque count — the reconciliation of indices vs. the RGB PPM oracle. */
    {
        u8 px[64 * 64];
        CHECK(load_at("data/game/C/S16FONTS.GRA", SCRATCH, &len), "fonts loads again");
        CHECK(gra_open(SCRATCH, len, c, 4, &n), "fonts chain again");
        int used = gra_decode_frame(SCRATCH, c, n, 0, px, sizeof px);
        CHECK_EQ_INT(used, 92);
        int opaque = 0;
        for (int i = 0; i < 13 * 7; i++) if (px[i]) opaque++;
        CHECK_EQ_INT(opaque, 88);

        /* Sentinel records, by *record index* (the earlier byte offsets 864 and
         * 624 were being misread as indices and only hit the out-of-range
         * guard). S16FONTS has 5 zero-dimension records and S16TITLE has 1
         * negative-dimension record (-320x-200 at index 72); each must be
         * rejected by the sentinel guard. Removing `(s16)w <= 0 || (s16)h <= 0`
         * makes the zero-dimension ones return >= 0, so these tests fail. */
        static const int zero_dim[] = {52, 232, 263, 328, 330};
        CHECK(load_at("data/game/C/S16FONTS.GRA", SCRATCH, &len), "fonts loads a third time");
        CHECK(gra_open(SCRATCH, len, c, 4, &n), "fonts chain a third time");
        for (unsigned k = 0; k < sizeof zero_dim / sizeof zero_dim[0]; k++)
            CHECK_EQ_INT(gra_decode_frame(SCRATCH, c, n, zero_dim[k], px, sizeof px), -1);
        CHECK(load_at("data/game/C/S16TITLE.GRA", SCRATCH, &len), "title loads again");
        CHECK(gra_open(SCRATCH, len, c, 4, &n), "title chain again");
        CHECK_EQ_INT(gra_decode_frame(SCRATCH, c, n, 72, px, sizeof px), -1);
    }

    /* Primary assertion: exact consumption across all 69 shipped files. Every
     * positive-dimension descriptor with a strictly greater sprite offset in
     * its file must RLE-decode to exactly that next offset. This is the count
     * Task 8 measured in Python: 18,201 of 18,202, the one miss being
     * S16TITLE frame 113 (74x167 at 0x349D7). */
    {
        static u8 dst[800 * 700];
        int exact = 0, total = 0, zero = 0, neg = 0, nonext = 0, errors = 0;
        int miss_seen = 0;
        DIR *d = opendir("data/game/C");
        CHECK(d != NULL, "game dir opens");
        struct dirent *ent;
        while (d && (ent = readdir(d)) != NULL) {
            size_t l = strlen(ent->d_name);
            if (l < 4 || strcasecmp(ent->d_name + l - 4, ".GRA") != 0) continue;
            char path[300];
            snprintf(path, sizeof path, "data/game/C/%s", ent->d_name);
            if (!load_at(path, SCRATCH, &len) || !gra_open(SCRATCH, len, c, 4, &n) || n <= 0)
                continue;
            int k6 = -1;
            for (int i = 0; i < n; i++) if (c[i].type == 6) k6 = i;
            if (k6 < 0) continue;
            u32 recs = c[k6].body_len / 12;
            for (u32 i = 0; i < recs; i++) {
                u32 at = SCRATCH + c[k6].body_off + i * 12;
                u16 w = DSW(at), h = DSW(at + 2);
                u32 off = DSD(at + 8) & 0x7FFFFFu;
                if (w == 0 || (w & 0x8000) || (h & 0x8000)) {
                    if (w == 0) zero++; else neg++;
                    CHECK_EQ_INT(gra_decode_frame(SCRATCH, c, n, (int)i, dst,
                                                  sizeof dst), -1);
                    continue;
                }
                u32 next = 0;
                for (u32 j = 0; j < recs; j++) {
                    u32 oj = DSD(SCRATCH + c[k6].body_off + j * 12 + 8) & 0x7FFFFFu;
                    if (oj > off && (next == 0 || oj < next)) next = oj;
                }
                if (next == 0) { nonext++; continue; }
                total++;
                int used = gra_decode_frame(SCRATCH, c, n, (int)i, dst, sizeof dst);
                if (used < 0) { errors++; continue; }
                if ((u32)used == next - off) exact++;
                else if (strcasecmp(ent->d_name, "S16TITLE.GRA") == 0 &&
                         w == 74 && h == 167 && off == 0x349D7) miss_seen = 1;
            }
        }
        if (d) closedir(d);
        CHECK_EQ_INT(zero, 17);
        CHECK_EQ_INT(neg, 10);
        CHECK_EQ_INT(nonext, 60);
        CHECK_EQ_INT(errors, 0);
        CHECK_EQ_INT(total, 18202);
        CHECK_EQ_INT(exact, 18201);
        CHECK(miss_seen, "the single mismatch is the documented S16TITLE record");
    }

    return g_failures - before;
}

/* ---- test_gfx.c ---- */

/* Scratch colour buffer for raw-pointer records, clear of the resource heap
 * (0x10B0D0 .. ~0x2A8C548), same as the GRA test. */
#define SCRATCH 0x3000000u
#define REC   0x107498u
#define HEAD  0x107798u

/* Builds exactly one dirty record and points the head at its end. */
static void put_record(u32 ptr, u32 first, u32 count, u32 flag)
{
    DSD(REC + 0) = ptr;
    DSD(REC + 4) = first;
    DSD(REC + 8) = count;
    DSD(REC + 12) = flag;
    DSD(HEAD) = REC + 16;
}

/* The 6-bit VGA DAC channel expanded to the 8-bit value gfx_dac holds, exactly
 * as gfx_flush_palette does (and as a VGA/DOSBox renders it). */
static u8 exp8(u8 v6) { return (u8)((v6 << 2) | (v6 >> 4)); }

int test_gfx(void)
{
    int before = g_failures;

    /* Raw-pointer record: R/G/B field order, first-index placement, count
     * discrimination (only the addressed entries are written), head reset, and
     * consumed marking. */
    put_record(SCRATCH, 5, 1, 0);
    u32 word = (0x10u << 2) | (0x20u << 10) | (0x30u << 18);
    DSD(SCRATCH) = word;
    DSD(SCRATCH + 4) = (0x11u << 2) | (0x22u << 10) | (0x33u << 18);
    gfx_flush_palette();
    CHECK_EQ_INT(gfx_dac[5][0], exp8(0x10));
    CHECK_EQ_INT(gfx_dac[5][1], exp8(0x20));
    CHECK_EQ_INT(gfx_dac[5][2], exp8(0x30));
    CHECK(gfx_dac[4][0] == 0, "only the addressed DAC entry was written");
    CHECK_EQ_INT(DSD(HEAD), REC);            /* head reset to base */
    CHECK_EQ_INT(DSD(REC + 4), 0xFFFFFFFFu); /* record marked consumed */

    /* count=2 writes both addressed entries, and nothing past count. */
    put_record(SCRATCH, 5, 2, 0);
    DSD(SCRATCH) = word;
    DSD(SCRATCH + 4) = (0x11u << 2) | (0x22u << 10) | (0x33u << 18);
    gfx_flush_palette();
    CHECK_EQ_INT(gfx_dac[5][0], exp8(0x10));
    CHECK_EQ_INT(gfx_dac[6][0], exp8(0x11));
    CHECK_EQ_INT(gfx_dac[6][1], exp8(0x22));
    CHECK_EQ_INT(gfx_dac[6][2], exp8(0x33));
    CHECK(gfx_dac[7][0] == 0, "count bounds the write");

    /* 6-bit VGA truncation: a channel byte >= 0x40 must be masked to 6 bits
     * before the display expansion. (0x50 & 0x3F) = 0x10 -> exp8(0x10) = 0x41;
     * without the mask the full 8-bit channel exp8(0x50) = 0x45, so this pins
     * the truncation. */
    put_record(SCRATCH, 0x30, 1, 0);
    DSD(SCRATCH) = 0x50u << 2;
    gfx_flush_palette();
    CHECK_EQ_INT(gfx_dac[0x30][0], exp8(0x50u & 0x3Fu));
    CHECK(gfx_dac[0x30][0] != exp8(0x50u),
          "the 6-bit VGA truncation is applied");

    /* No count clamp (record §3 of 2026-09-29-todo-verify-derivations.md):
     * 0x1C48B..0x1C49F clamps first + the flag word [+0xC], not the count, so
     * first 0xFE + count 8 with flag 0 runs all eight writes, and the DAC's
     * 8-bit write index wraps: entries 0 and 1 take the 3rd and 4th words. */
    gfx_dac[0][0] = 0;
    gfx_dac[1][0] = 0;
    put_record(SCRATCH, 0xFE, 8, 0);
    for (int i = 0; i < 8; i++) DSD(SCRATCH + (u32)i * 4) = (u32)(0x10 + i) << 2;
    gfx_flush_palette();
    CHECK_EQ_INT(gfx_dac[0xFE][0], exp8(0x10));
    CHECK_EQ_INT(gfx_dac[0xFF][0], exp8(0x11));
    CHECK_EQ_INT(gfx_dac[0][0], exp8(0x12));   /* the index wraps (0x1C4D6) */
    CHECK_EQ_INT(gfx_dac[1][0], exp8(0x13));

    /* Record §3: the write loop is a do-while (0x1C4D6 `dec esi; jg`), so a
     * zero count still writes one entry. */
    gfx_dac[0x40][0] = 0;
    put_record(SCRATCH, 0x40, 0, 0);
    DSD(SCRATCH) = 0x15u << 2;
    gfx_flush_palette();
    CHECK_EQ_INT(gfx_dac[0x40][0], exp8(0x15));

    /* Record §3: first + the flag dword above 0x100 (signed, 0x1C497 `jle`)
     * subtracts the excess from the flag dword itself (0x1C49F), never from
     * the count: first 0x100 + flag 0x200 leaves flag 0, and the count-1
     * write still lands, at DAC index (u8)0x100 = 0 (0x1C4AA `out dx,al`). */
    put_record(SCRATCH, 0x100, 1, 0x200);
    DSD(SCRATCH) = 0x16u << 2;
    gfx_flush_palette();
    CHECK_EQ_INT(DSD(REC + 12), 0);
    CHECK_EQ_INT(gfx_dac[0][0], exp8(0x16));

    /* Record §3: 0x33734/0x33714 store only the flag's low byte
     * (`mov byte [eax-4],0/1`, 0x3373F/0x3371F); the upper three bytes keep
     * what the record slot held. */
    DSD(HEAD) = REC;
    DSD(REC + 12) = 0xAABBCC00u;
    palette_record(SCRATCH, 0x10, 1, 1);
    CHECK_EQ_INT(DSD(REC + 12), 0xAABBCC01u);
    DSD(HEAD) = REC;

    /* Record §K1.7 (2026-09-29-k1-k9-derivations.md): 0x33714 is the append
     * with the constant flag byte 1 (0x3371F), EBX -> +0, EAX -> +4,
     * EDX -> +8 (0x33723..0x33729), head += 0x10 (0x3372C). Sentinels differ
     * from every post-value. */
    DSD(REC + 0) = 0xDEADBEEFu;
    DSD(REC + 4) = 0xDEADBEEFu;
    DSD(REC + 8) = 0xDEADBEEFu;
    DSD(REC + 12) = 0xAABBCC00u;
    palette_record_flagged(SCRATCH, 0x10, 3);
    CHECK_EQ_INT(DSD(REC + 0), SCRATCH);
    CHECK_EQ_INT(DSD(REC + 4), 0x10);
    CHECK_EQ_INT(DSD(REC + 8), 3);
    CHECK_EQ_INT(DSD(REC + 12), 0xAABBCC01u);
    CHECK_EQ_INT(DSD(HEAD), REC + 16);
    DSD(HEAD) = REC;

    /* Handle path: a non-zero flag byte in [3] makes [0] a resource handle;
     * gfx_flush_palette resolves it and skips the bank's u32 colour count at
     * +4. Re-basing the resolved host pointer instead of its offset reads the
     * wrong address and fails these assertions. */
    {
        u32 ridx = 0;
        while (ridx < res_count() &&
               !(res_resolve(res_handle(ridx, 0)) && res_size(ridx) >= 16))
            ridx++;
        CHECK(ridx < res_count(), "a resource large enough for the handle path");
        if (ridx < res_count()) {
            u32 off = (u32)((const u8 *)res_resolve(res_handle(ridx, 0)) - mem);
            DSD(off + 4) = (0x21u << 2) | (0x22u << 10) | (0x23u << 18);
            DSD(off + 8) = (0x31u << 2) | (0x32u << 10) | (0x33u << 18);
            put_record(res_handle(ridx, 0), 0x10, 2, 1);
            gfx_flush_palette();
            CHECK_EQ_INT(gfx_dac[0x10][0], exp8(0x21));
            CHECK_EQ_INT(gfx_dac[0x10][1], exp8(0x22));
            CHECK_EQ_INT(gfx_dac[0x10][2], exp8(0x23));
            CHECK_EQ_INT(gfx_dac[0x11][0], exp8(0x31));
            CHECK_EQ_INT(gfx_dac[0x11][1], exp8(0x32));
            CHECK_EQ_INT(gfx_dac[0x11][2], exp8(0x33));
        }
    }

    /* Unresolvable handle (index 0x1FF is past the 69-entry table): the record
     * is skipped, not read through NULL, and still marked consumed. */
    gfx_dac[0x20][0] = 0;
    put_record(0xFFFFFFFFu, 0x20, 1, 1);
    gfx_flush_palette();
    CHECK_EQ_INT(gfx_dac[0x20][0], 0);
    CHECK_EQ_INT(DSD(REC + 4), 0xFFFFFFFFu);
    CHECK_EQ_INT(DSD(HEAD), REC);

    /* 0x52106 (gfx_screen_reset): both tick counters take EAX, the two
     * offscreen buffers and the aperture are filled with the dword EAX
     * (0x51F72 with EDX = EAX), and the DAC is blacked (AL = 0 at 0x5213D).
     * Buffers at scratch addresses with 0xAB sentinels, one byte past each
     * 0xFA00-byte fill; the DAC and the aperture seeded non-zero. */
    {
        const u32 buf_a = SCRATCH + 0x10000u, buf_b = SCRATCH + 0x20000u;
        const u32 s_e8 = DSD(DS_001014E8), s_e4 = DSD(DS_001014E4);
        const u32 s_08 = DSD(DS_00101508), s_0c = DSD(DS_0010150C);
        static u8 s_dac[256][3], s_ap[320 * 200];
        memcpy(s_dac, gfx_dac, sizeof s_dac);
        memcpy(s_ap, gfx_aperture(), sizeof s_ap);

        DSD(DS_001014E8) = buf_a;
        DSD(DS_001014E4) = buf_b;
        mem_fill(buf_a, 0xAB, 0xFA04u);
        mem_fill(buf_b, 0xAB, 0xFA04u);
        DSD(DS_00101508) = 0x1111u;
        DSD(DS_0010150C) = 0x2222u;
        memset(gfx_dac, 0x3F, sizeof gfx_dac);
        memset(gfx_aperture(), 0x77, 320 * 200);

        gfx_screen_reset(0x12345678u);
        u32 ap0, apl;
        memcpy(&ap0, gfx_aperture(), 4);
        memcpy(&apl, gfx_aperture() + 0xF9FCu, 4);
        CHECK_EQ_INT((int)DSD(DS_00101508), 0x12345678);
        CHECK_EQ_INT((int)DSD(DS_0010150C), 0x12345678);
        CHECK_EQ_INT((int)DSD(buf_a), 0x12345678);
        CHECK_EQ_INT((int)DSD(buf_a + 0xF9FCu), 0x12345678);
        CHECK_EQ_INT((int)DSB(buf_a + 0xFA00u), 0xAB);
        CHECK_EQ_INT((int)DSD(buf_b), 0x12345678);
        CHECK_EQ_INT((int)DSD(buf_b + 0xF9FCu), 0x12345678);
        CHECK_EQ_INT((int)DSB(buf_b + 0xFA00u), 0xAB);
        CHECK_EQ_INT((int)ap0, 0x12345678);
        CHECK_EQ_INT((int)apl, 0x12345678);
        CHECK_EQ_INT(gfx_dac[0][0], 0);
        CHECK_EQ_INT(gfx_dac[128][1], 0);
        CHECK_EQ_INT(gfx_dac[255][2], 0);

        /* The ported callers (0x2BAF4, 0x1C740) pass 0: the screen and both
         * buffers go black. */
        gfx_dac[7][0] = 0x3F;
        gfx_screen_reset(0u);
        CHECK_EQ_INT((int)DSD(DS_00101508), 0);
        CHECK_EQ_INT((int)DSD(DS_0010150C), 0);
        CHECK_EQ_INT((int)DSD(buf_a + 0x8000u), 0);
        CHECK_EQ_INT((int)DSD(buf_b + 0x8000u), 0);
        CHECK_EQ_INT((int)gfx_aperture()[0x8000u], 0);
        CHECK_EQ_INT(gfx_dac[7][0], 0);

        DSD(DS_001014E8) = s_e8;
        DSD(DS_001014E4) = s_e4;
        DSD(DS_00101508) = s_08;
        DSD(DS_0010150C) = s_0c;
        memcpy(gfx_dac, s_dac, sizeof s_dac);
        memcpy(gfx_aperture(), s_ap, sizeof s_ap);
    }

    /* Record §K2.4 (2026-09-29-k2-k5-derivations.md): 0x51F72 stores the dword
     * EDX over 0xC8 passes of 0x140 bytes from EAX on (0x51F73, 0x520F7), so
     * exactly 0xFA00 bytes. Four 0xAB sentinel bytes on each side. */
    {
        const u32 buf = SCRATCH + 0x30000u, base = buf + 4u;
        mem_fill(buf, 0xAB, 0xFA08u);
        gfx_fill_screen(base, 0xCAFEF00Du);
        u32 bad = 0u;
        for (u32 i = 0; i < 0xFA00u; i += 4u)
            if (DSD(base + i) != 0xCAFEF00Du) bad++;
        CHECK_EQ_INT((int)bad, 0);
        CHECK_EQ_INT((int)DSD(base), (int)0xCAFEF00Du);
        CHECK_EQ_INT((int)DSD(base + 0xF9FCu), (int)0xCAFEF00Du);
        CHECK_EQ_INT((int)DSD(buf), (int)0xABABABABu);
        CHECK_EQ_INT((int)DSD(base + 0xFA00u), (int)0xABABABABu);
    }

    return g_failures - before;
}

/* ---- test_sprite.c ---- */

static void check_node_build(void)
{
    SpriteNode n;
    memset(&n, 0xAA, sizeof n);          /* poison, to catch unwritten fields */

    sprite_node_build(&n, 0);
    CHECK_EQ_INT(n.rows, 0);
    CHECK_EQ_INT(n.width, 0);
    CHECK_EQ_INT(n.xorg, 0);
    CHECK_EQ_INT(n.yorg, 0);

    /* A real RLE sprite, with its header values read from the shipped assets
     * (s16rad.gra): width 15, height 107, X pivot 8, Y pivot 53. The pivots are
     * NOT centred, which is what makes the hflip assertion below non-vacuous. */
    SpriteNode a, b;
    sprite_node_build(&a, 0x0001u);
    CHECK_EQ_INT(a.type & 0x01, 1);
    CHECK_EQ_INT(a.type & 0x02, 0);
    CHECK_EQ_INT(a.type & 0x08, 0);
    CHECK_EQ_INT(a.width, 15);
    CHECK_EQ_INT(a.rows, 107);
    CHECK_EQ_INT(a.xorg, 8);
    CHECK_EQ_INT(a.yorg, 53);
    const u8 *px = NULL;
    CHECK_EQ_INT(gra_sprite_pixels(a.pixel_handle, &px), 1);

    /* The same id with the hflip bit must set type bit 3 and mirror the X
     * pivot as width - xorg - 1 == 15 - 8 - 1 == 6, changing nothing else. */
    sprite_node_build(&b, 0x0001u | 0x8000u);
    CHECK_EQ_INT(b.type & 0x08, 8);
    CHECK_EQ_INT(b.xorg, 6);
    CHECK_EQ_INT(b.width, 15);
    CHECK_EQ_INT(b.rows, 107);
    CHECK_EQ_INT(b.yorg, 53);

    /* A negative-height header selects the raw base and negates both
     * dimensions. The first such entry is id 0x2BDF (s16caves.gra), whose
     * 12-byte record is { width -975; height -53; xorg 0; yorg 0 }, so
     * rows = -height = 53 and width = -width = 975, and type base is 2.
     * (The plan wrote this pair swapped; verified against the asset record at
     * s16caves.gra offset 0x477D4 and against 0x14268's own width/height use.) */
    SpriteNode r;
    sprite_node_build(&r, 0x2BDFu);
    CHECK_EQ_INT(r.type & 0x02, 2);
    CHECK_EQ_INT(r.type & 0x01, 0);
    CHECK_EQ_INT(r.rows, 53);
    CHECK_EQ_INT(r.width, 975);
}

/* The port now has THREE independent decoders of the same RLE: the new span
 * renderer, gra_decode_frame (sub-project 1, verified against the Python
 * oracle), and tools/gra_render.py. They must agree byte-for-byte on real
 * assets. This is the compositor's primary oracle and needs no emulator.
 *
 * The gra decoder packs a sprite's rows at `width`; the renderer advances by
 * `stride` (0x140, exactly 0x5D218's row pitch), so the comparison is row by
 * row at the renderer's pitch — a whole-buffer memcmp would trip on the row
 * advance alone, not on pixels. `bank` is the source-index offset the blitter
 * computes (sprite_bank -> sprite_bank_offset), so 0 is identity. */
static void check_rle_cross(void)
{
    static u8 expect[320 * 200];
    static u8 got[320 * 200];
    int checked = 0, literals = 0, fills = 0, transparents = 0;

    for (u32 id = 0; id < 0x7FFFu && checked < 32; id++) {
        GraSprite g; u32 dh = 0;
        if (!gra_sprite_lookup(id, &g, &dh)) break;
        if (g.height <= 0 || g.width <= 0) continue;        /* RLE base only */
        if ((int)g.width > 320 || (int)g.height > 200) continue;

        const u8 *px = NULL;
        if (!gra_sprite_pixels(g.pixel_handle, &px)) continue;

        /* The blob length gra_decode_frame derives internally: from the
         * handle's byte offset to the end of its resource block. */
        u32 idx = g.pixel_handle >> 23;
        u32 off = g.pixel_handle & 0x7FFFFFu;
        if (idx >= res_count() || off >= res_size(idx)) continue;
        u32 len = res_size(idx) - off;

        memset(expect, 0, sizeof expect);
        int n = gra_decode_frame_at(px, len, g.width, g.height, expect,
                                    sizeof expect);
        if (n < 0) continue;

        memset(got, 0, sizeof got);
        u8 bank = 0;                       /* offset 0: identity */
        CHECK_EQ_INT(sprite_render_rle(px, got, g.width, g.height, 320, bank), 0);

        int same = 1;
        for (int r = 0; r < g.height && same; r++)
            if (memcmp(expect + (size_t)r * (size_t)g.width,
                       got + (size_t)r * 320u, (size_t)g.width) != 0)
                same = 0;
        CHECK(same, "span renderer == gra decoder");

        /* Count control-byte classes so the sample cannot pass vacuously. */
        for (const u8 *p = px; p < px + n; ) {
            u8 b = *p++;
            if (b < 0x80) { literals++; p += b; }
            else if (b < 0xC0) { fills++; p += 1; }
            else transparents++;
        }
        checked++;
    }
    CHECK(checked >= 8, "cross-checked at least 8 sprites");
    CHECK(literals > 0 && fills > 0, "sample covers literal and fill runs");
    CHECK(transparents > 0, "sample covers transparent runs");
}

/* The two generated tables live in a region Ghidra never decompiled, so
 * gen_symbols.py emits no DS_ symbols for them; the addresses are literals and
 * are inside the loaded data object. */
#define BANK_TABLE   0x00081310u
#define COLOUR_TABLE 0x00081314u

/* Scratch for a fake 0x33754 palette-table entry (16 bytes: handle, refcount,
 * start, len). Clear of the resource heap and of the other tests' scratch. */
#define PAL_ENTRY    0x3F00000u

static void check_bank_and_colour(void)
{
    /* BANK_TABLE[n] == (n-1) replicated, for every n in 1..255. These are
     * static generated table facts, so they are pinned exactly. */
    for (u32 n = 1; n < 256; n++)
        CHECK(DSD(BANK_TABLE + n * 4u) == (n - 1u) * 0x01010101u,
              "bank table entry is (n-1) replicated");
    /* [0] is the stale code pointer, deliberately not replicated. */
    CHECK(DSD(BANK_TABLE) == 0x0005D110u, "bank[0] is the stale pointer");

    /* COLOUR_TABLE[n] == n replicated, for every n. */
    for (u32 n = 0; n < 256; n++)
        CHECK(DSD(COLOUR_TABLE + n * 4u) == n * 0x01010101u,
              "colour table entry is n replicated");

    /* The bank *byte* mapping: (b-1), except that byte 0 is no offset. */
    CHECK_EQ_INT(sprite_bank_offset(1), 0);
    CHECK_EQ_INT(sprite_bank_offset(2), 1);
    CHECK_EQ_INT(sprite_bank_offset(255), 254);
    CHECK_EQ_INT(sprite_bank_offset(0), 0);

    /* sprite_bank interprets its argument as a 0x33754 palette-table entry
     * ({handle; refcount; start; len}), not a resource handle: the bank is the
     * low byte of the entry's `start` field at +8. Build entries in scratch
     * memory. (The previous version of this check resolved a sprite descriptor
     * handle through res_resolve and read its byte 8; that encoded the
     * sprite_bank bug the fix removes.) */
    u32 entry = PAL_ENTRY;
    DSD(entry + 8) = 0x41u;
    CHECK_EQ_INT(sprite_bank(entry), 0x40);       /* sprite_bank_offset(0x41) */
    DSB(entry + 8) = 0;                            /* start 0 => no offset */
    CHECK_EQ_INT(sprite_bank(entry), 0);
    CHECK_EQ_INT(sprite_bank(0), 0);               /* null palette entry */

    /* A non-zero bank must actually shift the drawn pixels. This is the path
     * the cross-check in Task 3 cannot cover: it renders at bank offset 0, so
     * the fill-colour `+ bank` add is otherwise untested. Row: literal 2, fill
     * 3 (colour index 7) -- at offset 2 every drawn byte is +2. */
    static const u8 row[9] = { 0x02, 0x0A, 0x0B, 0x83, 0x07, 0,0,0,0 };
    u8 out[8]; memset(out, 0xEE, sizeof out);
    CHECK_EQ_INT(sprite_render_rle(row, out, 5, 1, 8, 2), 0);
    CHECK_EQ_INT(out[0], 0x0C);   /* 0x0A + 2 */
    CHECK_EQ_INT(out[1], 0x0D);   /* 0x0B + 2 */
    CHECK_EQ_INT(out[2], 0x09);   /* colour index 7, zero-offset byte 7, + 2 */
    CHECK_EQ_INT(out[3], 0x09);
    CHECK_EQ_INT(out[4], 0x09);
    /* No overrun into the row padding. */
    CHECK_EQ_INT(out[5], 0xEE);
}

static void check_raw_copy(void)
{
    /* A 4x2 raw fixture with the bank offset applied byte-wise. */
    const u8 src[8] = { 1,2,3,4, 5,6,7,8 };
    /* dst must fit two stride-16 rows: the brief's dst[16] would write row 1
     * at dst[16..19], past the end, clobbering the adjacent stack. */
    u8 dst[32]; memset(dst, 0xEE, sizeof dst);
    CHECK_EQ_INT(sprite_render_raw(src, dst, 4, 2, 16, 3, 0,0,0), 0);
    for (int i = 0; i < 4; i++) CHECK_EQ_INT(dst[i], src[i] + 3);
    for (int i = 0; i < 4; i++) CHECK_EQ_INT(dst[16 + i], src[4 + i] + 3);
    /* The row gap is untouched. */
    for (int i = 4; i < 16; i++) CHECK_EQ_INT(dst[i], 0xEE);

    /* Overflow wraps byte-wise, not into the next pixel. */
    const u8 hi[2] = { 0xFE, 0xFF };
    u8 d2[2] = { 0, 0 };
    CHECK_EQ_INT(sprite_render_raw(hi, d2, 2, 1, 2, 4, 0,0,0), 0);
    CHECK_EQ_INT(d2[0], 0x02);
    CHECK_EQ_INT(d2[1], 0x03);
}

/* Clipped raw (0x58CBD, type 0x12): clip_t whole source rows and clip_l source
 * columns are skipped, and `vis = width - clip_l - clip_r` bytes per drawn row
 * are copied from the window origin. Fixture: 6-wide, 3-row raw, clip_l 1,
 * clip_r 2 (vis 3), clip_t 1 (2 drawn rows), stride 8. Hand-computed: the
 * skipped row 0 and the clipped columns/gap must stay 0xEE, and each drawn
 * row's bytes land at dst[0..2]. A no-clip copy of `width` bytes would draw
 * row 0 and overwrite the dst[3..7] padding -- both caught below. */
static void check_raw_clipped(void)
{
    u8 src[18];
    for (int i = 0; i < 18; i++) src[i] = (u8)(10 + i);
    /* row0 = 10..15 (skipped), row1 = 16..21, row2 = 22..27. */
    u8 dst[2 * 8]; memset(dst, 0xEE, sizeof dst);

    CHECK_EQ_INT(sprite_render_raw(src, dst, 6, 3, 8, 0, 1, 2, 1), 0);
    /* src = row1 + clip_l = index 6+1 = 7 -> 17,18,19. */
    CHECK_EQ_INT(dst[0], 17);
    CHECK_EQ_INT(dst[1], 18);
    CHECK_EQ_INT(dst[2], 19);
    for (int i = 3; i < 8; i++) CHECK_EQ_INT(dst[i], 0xEE);
    /* src advances a whole width to row2 + clip_l = index 12+1 = 13. */
    CHECK_EQ_INT(dst[8], 23);
    CHECK_EQ_INT(dst[9], 24);
    CHECK_EQ_INT(dst[10], 25);
    for (int i = 11; i < 16; i++) CHECK_EQ_INT(dst[i], 0xEE);

    /* vis <= 0 draws nothing; rows - clip_t <= 0 draws nothing. */
    memset(dst, 0xEE, sizeof dst);
    CHECK_EQ_INT(sprite_render_raw(src, dst, 6, 3, 8, 0, 3, 3, 0), 0);
    CHECK_EQ_INT(sprite_render_raw(src, dst, 6, 3, 8, 0, 0, 0, 3), 0);
    for (int i = 0; i < (int)sizeof dst; i++) CHECK_EQ_INT(dst[i], 0xEE);
}

/* 0x5D28F / 0x57FFB semantics. Fixture: a 6-wide, 3-row RLE sprite whose row
 * is [literal 2][transparent 1][fill 3], built by repeating the 9-byte ROW so
 * the three stream rows are byte-identical; stride 16, bank 0 (identity). The
 * blob's trailing zero bytes are no-op literals, so a source desync of a few
 * bytes would hide here -- the distinct-payload AB stream below pins source
 * consumption instead. Destination indexing is window-relative: dst[0] is the
 * window's first visible column (render_list clamped node.x to the clip edge),
 * so the expected rows below are the visible window written from dst[0]. */
static void check_clipped_case(const u8 *blob, int clip_l, int clip_r,
                               const u8 *expect, const char *what)
{
    u8 dst[3 * 16];
    memset(dst, 0xEE, sizeof dst);
    CHECK_EQ_INT(sprite_render_rle_clipped(blob, dst, 6, 3, 16, 0,
                                           clip_l, clip_r, 0, 0), 0);
    /* Every stream row is checked, so a per-row source desync (rather than
     * clipping) would also trip this. */
    int same = 1;
    for (int r = 0; r < 3 && same; r++)
        for (int c = 0; c < 6; c++)
            if (dst[r * 16 + c] != expect[c]) same = 0;
    CHECK(same, what);
}

static void check_rle_clipped(void)
{
    static const u8 ROW[9] = { 0x02, 0x0A, 0x0B, 0xC1, 0x00, 0x83, 0x07,0,0 };
    u8 blob[27];
    for (int i = 0; i < 3; i++) memcpy(blob + i * 9, ROW, sizeof ROW);

    /* 0xEE marks a column the window does not cover (dst untouched). */
    static const u8 e_none[6]   = { 0x0A,0x0B,0xEE,0x07,0x07,0x07 };
    static const u8 e_left[6]   = { 0x0B,0xEE,0x07,0x07,0x07,0xEE };
    static const u8 e_ltrans[6] = { 0xEE,0x07,0x07,0x07,0xEE,0xEE };
    static const u8 e_right[6]  = { 0x0A,0x0B,0xEE,0x07,0xEE,0xEE };
    static const u8 e_both[6]   = { 0xEE,0x07,0xEE,0xEE,0xEE,0xEE };
    static const u8 e_spans[6]  = { 0x07,0xEE,0xEE,0xEE,0xEE,0xEE };

    check_clipped_case(blob, 0, 0, e_none,  "clipped: no clip");
    check_clipped_case(blob, 1, 0, e_left,  "clipped: left cuts mid-literal");
    check_clipped_case(blob, 2, 0, e_ltrans,"clipped: left cuts transparent");
    check_clipped_case(blob, 0, 2, e_right, "clipped: right cuts fill run");
    check_clipped_case(blob, 2, 2, e_both,  "clipped: left and right");
    check_clipped_case(blob, 4, 1, e_spans, "clipped: window inside fill run");

    /* clip_t: whole rows of stream consumed without drawing, so the visible
     * rows shift up by one and the last destination slot stays untouched. */
    u8 dst[3 * 16]; memset(dst, 0xEE, sizeof dst);
    CHECK_EQ_INT(sprite_render_rle_clipped(blob, dst, 6, 3, 16, 0, 0,0,1,0), 0);
    for (int c = 0; c < 6; c++) {
        CHECK_EQ_INT(dst[c], e_none[c]);
        CHECK_EQ_INT(dst[16 + c], e_none[c]);
        CHECK_EQ_INT(dst[32 + c], 0xEE);
    }

    /* Source consumption, on rows with DISTINCT payloads: clipping must still
     * consume each whole row's stream, or row B decodes out of sync. */
    static const u8 AB[12] = {
        0x02,0x11,0x12, 0xC1, 0x83,0x05,
        0x02,0x21,0x22, 0xC1, 0x83,0x06,
    };
    static const u8 e_a[6] = { 0x12,0xEE,0x05,0x05,0x05,0xEE };
    static const u8 e_b[6] = { 0x22,0xEE,0x06,0x06,0x06,0xEE };
    memset(dst, 0xEE, sizeof dst);
    CHECK_EQ_INT(sprite_render_rle_clipped(AB, dst, 6, 2, 16, 0, 1,0,0,0), 0);
    int same = 1;
    for (int c = 0; c < 6; c++) {
        if (dst[c] != e_a[c]) same = 0;
        if (dst[16 + c] != e_b[c]) same = 0;
    }
    CHECK(same, "clipped: L=1 consumes each whole row's stream");

    /* vis <= 0: the whole stream is still consumed, nothing is drawn. */
    memset(dst, 0xEE, sizeof dst);
    CHECK_EQ_INT(sprite_render_rle_clipped(blob, dst, 6, 3, 16, 0, 3,3,0,0), 0);
    same = 1;
    for (int i = 0; i < (int)sizeof dst; i++) if (dst[i] != 0xEE) same = 0;
    CHECK(same, "clipped: zero-width window draws nothing");
}

/* The unified rle_row must reduce to the old unclipped row exactly, including
 * at its boundaries: a run that exactly reaches width, and literal/fill/
 * transparent runs that overshoot it (the original clips those to the row but
 * still consumes the whole run from the source). */
static void check_rle_row_edges(void)
{
    /* literal 2 then fill 3 exactly fills the 5-pixel row. */
    static const u8 exact[5] = { 0x02, 0x0A, 0x0B, 0x83, 0x07 };
    u8 out[6]; memset(out, 0xEE, sizeof out);
    CHECK_EQ_INT(sprite_render_rle(exact, out, 5, 1, 6, 0), 0);
    CHECK_EQ_INT(out[0], 0x0A);
    CHECK_EQ_INT(out[1], 0x0B);
    for (int i = 2; i < 5; i++) CHECK_EQ_INT(out[i], 0x07);
    CHECK_EQ_INT(out[5], 0xEE);

    /* literal 9 on a 4-wide row: 4 drawn, the full 9 consumed. */
    static const u8 lo[10] = { 0x09, 1,2,3,4,5,6,7,8,9 };
    u8 l4[4]; memset(l4, 0xEE, sizeof l4);
    CHECK_EQ_INT(sprite_render_rle(lo, l4, 4, 1, 4, 0), 0);
    for (int i = 0; i < 4; i++) CHECK_EQ_INT(l4[i], i + 1);

    /* fill 10 on a 4-wide row: clipped to 4. */
    static const u8 fo[2] = { 0x8A, 0x03 };
    u8 f4[4]; memset(f4, 0xEE, sizeof f4);
    CHECK_EQ_INT(sprite_render_rle(fo, f4, 4, 1, 4, 0), 0);
    for (int i = 0; i < 4; i++) CHECK_EQ_INT(f4[i], 0x03);

    /* transparent 10 on a 4-wide row: nothing drawn. */
    static const u8 to[1] = { 0xCA };
    u8 t4[4]; memset(t4, 0xEE, sizeof t4);
    CHECK_EQ_INT(sprite_render_rle(to, t4, 4, 1, 4, 0), 0);
    for (int i = 0; i < 4; i++) CHECK_EQ_INT(t4[i], 0xEE);
}

/* 0x57F80 (hflip RLE) and 0x57FFB (hflip + clipped RLE): the same row decoder
 * with mirror set must reverse the visible columns. The row fixture is
 * [literal 2][transparent 1][fill 3], so the plain row is 0A 0B __ 07 07 07
 * and its mirror is 07 07 07 __ 0B 0A -- the transparent gap lands on the
 * opposite side and the two non-uniform runs swap ends, so this is not a
 * palindromic pass. */
static void check_rle_mirror(void)
{
    const u8 src[9] = { 0x02, 0x0A, 0x0B, 0xC1, 0x00, 0x83, 0x07,0,0 };
    u8 plain[6], mir[6];
    memset(plain, 0xEE, sizeof plain); memset(mir, 0xEE, sizeof mir);
    CHECK_EQ_INT(sprite_render_rle(src, plain, 6, 1, 6, 1), 0);
    /* The clipped entry with mirror=1 and no clip must reverse the columns. */
    CHECK_EQ_INT(sprite_render_rle_clipped(src, mir, 6, 1, 6, 1,
                                          0, 0, 0, /*mirror=*/1), 0);
    for (int i = 0; i < 6; i++) CHECK_EQ_INT(mir[i], plain[5 - i]);
}

/* Mirrored + clipped RLE (0x57FFB, type 0x19): the visible window is reversed,
 * not the whole `width`. The earlier mirror test uses clip_l = 0 / vis = width,
 * so its formula degenerates to vis-1-c and a bug ignoring the window passes.
 * Here clip_l = 1, clip_r = 2 (vis = 3) and the row is
 * [literal 2][transparent 1][fill 3].
 *
 * The mirrored source window is [clip_r, clip_r + vis), not [clip_l, clip_l +
 * vis). 0x57FFB's entry computes vis = width - clip_l - clip_r and walks the
 * destination backward from dst + vis - 1 (0x58001/0x58008); its three source
 * paths all start the source at the row-relative column clip_r: 0x58090
 * (clip_l != 0, clip_r == 0) reads from the row start with no skip (0x58093),
 * while 0x581D0 (clip_l == 0, clip_r != 0) and 0x582F4 (both) load clip_r into
 * the skip counter (0x581D0/0x582F4 `MOV EDX,[EBP+0x2c]`) and consume that many
 * source columns before drawing vis. The geometric reason: mirroring maps screen
 * column s to source width-1-(s-L), so the screen's right overhang clip_r is the
 * source's left overhang. The window is columns 2,3,4 = {transparent, 0x07,
 * 0x07}; reversed it is {0x07, 0x07, transparent}. The old [clip_l, clip_l+vis)
 * expectation (columns 1,2,3) is corrected here with the addresses above; it is
 * the source of the port's triangle-rendered first fighter
 * (port/src/platform/sprite.c rle_row). */
static void check_rle_mirror_clip(void)
{
    const u8 src[9] = { 0x02, 0x0A, 0x0B, 0xC1, 0x00, 0x83, 0x07,0,0 };
    u8 dst[8]; memset(dst, 0xEE, sizeof dst);
    CHECK_EQ_INT(sprite_render_rle_clipped(src, dst, 6, 1, 8, 0,
                                           1, 2, 0, /*mirror=*/1), 0);
    CHECK_EQ_INT(dst[0], 0x07);
    CHECK_EQ_INT(dst[1], 0x07);
    CHECK_EQ_INT(dst[2], 0xEE);          /* window column 2 is transparent */
    for (int i = 3; i < 8; i++) CHECK_EQ_INT(dst[i], 0xEE);
}

static void check_shear(void)
{
    /* The shear reads `vis` bytes from `src + sh`, where `sh` can be positive
     * (up to +2 in the cases below), so the fixture must carry slack past the
     * last row: 6 columns x 4 rows = 24 bytes for a 3-row image. A 6x3 buffer
     * would read out of bounds on the +2 case. */
    u8 src[6 * 4];
    for (int i = 0; i < 24; i++) src[i] = (u8)(10 + i);
    u8 dst[6 * 3]; memset(dst, 0xEE, sizeof dst);

    /* Zero the table explicitly first: this test must not depend on whatever
     * the loaded data object happens to hold at DS_00107900. With the table
     * zero the shear is zero and this is a plain copy. */
    DSW(DS_00107900 + 0) = 0;
    DSW(DS_00107900 + 2) = 0;
    DSW(DS_00107900 + 4) = 0;
    CHECK_EQ_INT(sprite_render_shear(src, dst, 6, 3, 6, 0, 0,0,0), 0);
    for (int i = 0; i < 18; i++) CHECK_EQ_INT(dst[i], src[i]);

    /* A non-zero ramp shears row r by ((tab[r] - tab[0]) >> 5), arithmetic. */
    DSW(DS_00107900 + 0) = 0;
    DSW(DS_00107900 + 2) = 32;      /* row 1: (32-0)>>5 = 1 */
    DSW(DS_00107900 + 4) = 64;      /* row 2: (64-0)>>5 = 2 */
    memset(dst, 0xEE, sizeof dst);
    CHECK_EQ_INT(sprite_render_shear(src, dst, 6, 3, 6, 0, 0,0,0), 0);
    for (int i = 0; i < 6; i++) CHECK_EQ_INT(dst[i], src[i]);           /* row 0 */
    for (int i = 0; i < 6; i++) CHECK_EQ_INT(dst[6+i], src[6 + i + 1]); /* row 1 */
    for (int i = 0; i < 6; i++) CHECK_EQ_INT(dst[12+i], src[12 + i + 2]);/* row 2 */

    /* Negative shear truncates toward -infinity: -33 >> 5 == -2. */
    DSW(DS_00107900 + 2) = (u16)0xFFDFu;   /* -33 */
    memset(dst, 0xEE, sizeof dst);
    CHECK_EQ_INT(sprite_render_shear(src, dst, 6, 3, 6, 0, 0,0,0), 0);
    /* (i16)0xFFDF == -33, (-33 - 0) >> 5 == -2 */
    for (int i = 0; i < 6; i++) CHECK_EQ_INT(dst[6+i], src[6 + i - 2]);

    /* Reset the table so later tests are unaffected. */
    DSW(DS_00107900 + 0) = 0;
    DSW(DS_00107900 + 2) = 0;
    DSW(DS_00107900 + 4) = 0;
}

/* Clipped shear (0x5215C, type 0x16) with clip_t > 0 settles the table index:
 * the disassembly indexes DS_00107900 by the DRAWN row, not the image row (the
 * original zeroes node->+0x3C before the loop and increments it only for drawn
 * rows). Fixture: 6-wide, 4-row, clip_l 1, clip_r 2 (vis 3), clip_t 1 (3 drawn
 * rows), stride 8, ramp tab = {0,32,64,96}: drawn-row shears are
 * 0, (32-0)>>5=1, (64-0)>>5=2. The image-row form would give 1,2,3, so row 0
 * discriminates (it would write src index 8, not 7). */
static void check_shear_clipped(void)
{
    u8 src[30];
    for (int i = 0; i < 30; i++) src[i] = (u8)(10 + i);
    u8 dst[3 * 8]; memset(dst, 0xEE, sizeof dst);

    DSW(DS_00107900 + 0) = 0;
    DSW(DS_00107900 + 2) = 32;
    DSW(DS_00107900 + 4) = 64;
    DSW(DS_00107900 + 6) = 96;

    CHECK_EQ_INT(sprite_render_shear(src, dst, 6, 4, 8, 0, 1, 2, 1), 0);
    /* src = clip_t*width + clip_l = 6+1 = 7 (value 17). Drawn row 0: sh 0. */
    CHECK_EQ_INT(dst[0], 17);
    CHECK_EQ_INT(dst[1], 18);
    CHECK_EQ_INT(dst[2], 19);
    for (int i = 3; i < 8; i++) CHECK_EQ_INT(dst[i], 0xEE);
    /* src advances to 13 (value 23). Drawn row 1: sh 1 -> indices 14,15,16. */
    CHECK_EQ_INT(dst[8], 24);
    CHECK_EQ_INT(dst[9], 25);
    CHECK_EQ_INT(dst[10], 26);
    for (int i = 11; i < 16; i++) CHECK_EQ_INT(dst[i], 0xEE);
    /* src advances to 19 (value 29). Drawn row 2: sh 2 -> indices 21,22,23. */
    CHECK_EQ_INT(dst[16], 31);
    CHECK_EQ_INT(dst[17], 32);
    CHECK_EQ_INT(dst[18], 33);
    for (int i = 19; i < 24; i++) CHECK_EQ_INT(dst[i], 0xEE);

    DSW(DS_00107900 + 0) = 0;
    DSW(DS_00107900 + 2) = 0;
    DSW(DS_00107900 + 4) = 0;
    DSW(DS_00107900 + 6) = 0;
}

/* Dispatch pinning: compare the blitter's whole back-buffer output against the
 * raster of the renderer the original's PTR_LAB_00080C8C selects. Comparing
 * output (not the call site) makes a swap between renderer classes -- or wrong
 * clip arguments into a shared renderer -- change the compared bytes. */
#define BLIT_BUF (320 * 200)

static u8 blit_pristine[BLIT_BUF];

enum {
    REF_RLE,
    REF_RAW,
    REF_RLE_MIRROR,
    REF_RLE_CLIP,
    REF_RLE_MIRROR_CLIP,
    REF_SHEAR
};

static void blit_ref(u8 *out, int which, const u8 *src, int w, int rows,
                     u8 bank, int x, int y, int L, int R, int T)
{
    memcpy(out, blit_pristine, BLIT_BUF);
    u8 *dst = out + DSD(DS_001088F8 + (u32)y * 4u) + (u32)x;
    switch (which) {
    case REF_RLE:
        sprite_render_rle(src, dst, w, rows, 320, bank); break;
    case REF_RAW:
        sprite_render_raw(src, dst, w, rows, 320, bank, L,R,T); break;
    case REF_RLE_MIRROR:
        sprite_render_rle_clipped(src, dst, w, rows, 320, bank, 0,0,0,1); break;
    case REF_RLE_CLIP:
        sprite_render_rle_clipped(src, dst, w, rows, 320, bank, L,R,T,0); break;
    case REF_RLE_MIRROR_CLIP:
        sprite_render_rle_clipped(src, dst, w, rows, 320, bank, L,R,T,1); break;
    case REF_SHEAR:
        sprite_render_shear(src, dst, w, rows, 320, bank, L,R,T); break;
    }
}

static void blit_node(SpriteNode *n, u32 type, u32 pixels, u32 pal,
                      int w, int rows, int L, int R, int T)
{
    memset(n, 0, sizeof *n);
    n->type = type; n->pixel_handle = pixels; n->pal_ptr = pal;
    n->width = w; n->rows = rows;
    n->clip_l = L; n->clip_r = R; n->clip_t = T;
}

/* Blit one node against the starting buffer and capture the whole back buffer. */
static void blit_run(u8 *out, SpriteNode *n, u32 icon)
{
    memcpy(mem + icon, blit_pristine, BLIT_BUF);
    sprite_blit(n);
    memcpy(out, mem + icon, BLIT_BUF);
}

static void check_blit_dispatch(void)
{
    /* A zero-size node must return without touching the buffer. Snapshot first:
     * `CHECK(1, "no crash")` would assert nothing, and a test that asserts
     * nothing is not a test. */
    u32 icon = DSD(DS_000E87A4);
    static u8 pre[320 * 200];
    memcpy(pre, mem + icon, sizeof pre);
    SpriteNode n; memset(&n, 0, sizeof n);
    sprite_blit(&n);
    CHECK(memcmp(mem + icon, pre, sizeof pre) == 0,
          "a zero-size node blits nothing");

    /* The blitter restores +0x14 and +0x30 after the call. */
    GraSprite g; u32 dh = 0;
    CHECK_EQ_INT(gra_sprite_lookup(0x2C11u, &g, &dh), 1);
    u32 pal = PAL_ENTRY;            /* fake 0x33754 palette-table entry */
    DSB(pal + 8) = 1;               /* start 1 => bank offset 0 */
    memset(&n, 0, sizeof n);
    sprite_node_build(&n, 0x2C11u);
    n.pal_ptr = pal;                 /* any resolvable pointer with a bank byte */
    n.x = 0; n.y = 0;
    n.rows = 2; n.width = 2;        /* clamp so the fixture is small */
    n.clip_t = 1; n.clip_b = 0;
    int rows_before = n.rows, top_before = n.clip_t;
    sprite_blit(&n);
    CHECK_EQ_INT(n.rows, rows_before);
    CHECK_EQ_INT(n.clip_t, top_before);

    /* RAW+HFLIP (type 10) is a no-op in the original and must stay one. */
    SpriteNode r; memset(&r, 0, sizeof r);
    r.type = 0x0Au; r.rows = 4; r.width = 4;
    r.pixel_handle = n.pixel_handle; r.pal_ptr = pal;
    u8 *back = mem + DSD(DS_000E87A4);
    u8 before = back[DSD(DS_001088F8) + 0];
    sprite_blit(&r);
    CHECK_EQ_INT(back[DSD(DS_001088F8) + 0], before);

    /* ---- Pin every live dispatch class by its whole-buffer output raster. */
    {
        const u8 *rle = NULL;
        u32 icon2 = DSD(DS_000E87A4);
        u32 pix = n.pixel_handle;
        u8 bank = (u8)sprite_bank(pal);
        SpriteNode s;
        static u8 got[BLIT_BUF], got2[BLIT_BUF], ref[BLIT_BUF], ref2[BLIT_BUF];

        memcpy(blit_pristine, mem + icon2, BLIT_BUF);

        u32 idx = pix >> 23, off = pix & 0x7FFFFFu;
        u32 blen = (idx < (u32)res_count() && off < res_size(idx))
                       ? res_size(idx) - off : 0;
        CHECK(blen >= 40, "0x2C11 blob is long enough for the fixtures");
        CHECK_EQ_INT(gra_sprite_pixels(pix, &rle), 1);

        /* 0x01 -> unclipped RLE (0x5D218). */
        blit_node(&s, 0x01, pix, pal, 9, 1, 0,0,0);
        blit_run(got, &s, icon2);
        blit_ref(ref, REF_RLE, rle, 9, 1, bank, 0, 0, 0,0,0);
        CHECK(memcmp(got, ref, BLIT_BUF) == 0,
              "0x01 routes to the RLE renderer");
        /* The real blob's control bytes must make RLE differ from a byte copy,
         * or this case could pass a raw/RLE swap. */
        blit_ref(ref2, REF_RAW, rle, 9, 1, bank, 0, 0, 0,0,0);
        CHECK(memcmp(ref, ref2, BLIT_BUF) != 0,
              "0x2C11 blob: RLE raster != raw copy");

        /* 0x02 raw (0x58CBD); 0x12 (RAW|CLIP) shares the same entry, so its
         * raster must equal 0x02's. */
        blit_node(&s, 0x02, pix, pal, 9, 3, 0,0,0);
        blit_run(got, &s, icon2);
        blit_ref(ref, REF_RAW, rle, 9, 3, bank, 0, 0, 0,0,0);
        CHECK(memcmp(got, ref, BLIT_BUF) == 0,
              "0x02 routes to the raw renderer");
        blit_node(&s, 0x12, pix, pal, 9, 3, 0,0,0);
        blit_run(got2, &s, icon2);
        CHECK(memcmp(got2, ref, BLIT_BUF) == 0,
              "0x12 routes to the raw renderer");
        CHECK(memcmp(got, got2, BLIT_BUF) == 0,
              "0x12 shares 0x58CBD with 0x02");

        /* 0x12 with real overhangs must use the clip-aware raw path: its raster
         * matches sprite_render_raw at the node's clip args and differs from the
         * no-clip raster. This pins the Critical fix. */
        blit_node(&s, 0x12, pix, pal, 9, 3, 2,1,1);
        blit_run(got2, &s, icon2);
        blit_ref(ref2, REF_RAW, rle, 9, 3, bank, 0, 0, 2,1,1);
        CHECK(memcmp(got2, ref2, BLIT_BUF) == 0,
              "0x12 with clip routes to clipped raw");
        CHECK(memcmp(ref2, ref, BLIT_BUF) != 0,
              "0x12's clip arguments change the raster");

        /* 0x11 -> clipped RLE, mirror off (0x5D28F). */
        blit_node(&s, 0x11, pix, pal, 9, 1, 2,1,0);
        blit_run(got, &s, icon2);
        blit_ref(ref, REF_RLE_CLIP, rle, 9, 1, bank, 0, 0, 2,1,0);
        CHECK(memcmp(got, ref, BLIT_BUF) == 0,
              "0x11 routes to clipped RLE");

        /* 0x09 (mirror, no clip; 0x57F80) vs 0x19 (mirror+clip; 0x57FFB):
         * same renderer, different arguments -- the rasters must differ. */
        blit_node(&s, 0x09, pix, pal, 9, 1, 0,0,0);
        blit_run(got, &s, icon2);
        blit_ref(ref, REF_RLE_MIRROR, rle, 9, 1, bank, 0, 0, 0,0,0);
        CHECK(memcmp(got, ref, BLIT_BUF) == 0,
              "0x09 routes to mirrored RLE");
        blit_node(&s, 0x19, pix, pal, 9, 1, 2,1,0);
        blit_run(got2, &s, icon2);
        blit_ref(ref2, REF_RLE_MIRROR_CLIP, rle, 9, 1, bank, 0, 0, 2,1,0);
        CHECK(memcmp(got2, ref2, BLIT_BUF) == 0,
              "0x19 routes to mirrored clipped RLE");
        CHECK(memcmp(ref, ref2, BLIT_BUF) != 0,
              "0x19's clip arguments change the raster");
        /* The mirror itself must change the raster, distinguishing 0x09 from
         * the unmirrored RLE cases. */
        blit_ref(ref2, REF_RLE, rle, 9, 1, bank, 0, 0, 0,0,0);
        CHECK(memcmp(ref, ref2, BLIT_BUF) != 0,
              "0x09's mirror changes the raster");

        /* Mode-1 shear (0x04/0x14 unclipped, 0x06/0x16 clipped; 0x5215C). A
         * non-zero DS_00107900 makes the raster differ from a plain copy. */
        DSW(DS_00107900 + 0) = 0;
        DSW(DS_00107900 + 2) = 32;
        DSW(DS_00107900 + 4) = 64;

        blit_node(&s, 0x04, pix, pal, 9, 3, 0,0,0);
        blit_run(got, &s, icon2);
        blit_ref(ref, REF_SHEAR, rle, 9, 3, bank, 0, 0, 0,0,0);
        CHECK(memcmp(got, ref, BLIT_BUF) == 0,
              "0x04 routes to the shear renderer");
        blit_node(&s, 0x14, pix, pal, 9, 3, 0,0,0);
        blit_run(got2, &s, icon2);
        CHECK(memcmp(got2, ref, BLIT_BUF) == 0,
              "0x14 shares the shear raster with 0x04");
        blit_ref(ref2, REF_RAW, rle, 9, 3, bank, 0, 0, 0,0,0);
        CHECK(memcmp(ref, ref2, BLIT_BUF) != 0,
              "non-zero shear table makes 0x04 differ from a copy");

        blit_node(&s, 0x06, pix, pal, 9, 3, 2,1,1);
        blit_run(got, &s, icon2);
        blit_ref(ref, REF_SHEAR, rle, 9, 3, bank, 0, 0, 2,1,1);
        CHECK(memcmp(got, ref, BLIT_BUF) == 0,
              "0x06 routes to clipped shear");
        blit_node(&s, 0x16, pix, pal, 9, 3, 2,1,1);
        blit_run(got2, &s, icon2);
        CHECK(memcmp(got2, ref, BLIT_BUF) == 0,
              "0x16 shares the shear raster with 0x06");

        DSW(DS_00107900 + 0) = 0;
        DSW(DS_00107900 + 2) = 0;
        DSW(DS_00107900 + 4) = 0;
    }
}

/* 1 when all `len` bytes at p equal v. */
static int k1_all(const u8 *p, u8 v, size_t len)
{
    for (size_t i = 0; i < len; i++)
        if (p[i] != v) return 0;
    return 1;
}

/* Record §K1.6 (2026-09-29-k1-k9-derivations.md): 0x51ED8 is 0x51E5C's span
 * blit with the aperture as its base (0x51F1B `add edi,0xa0000`) that does
 * not save/restore the node's +0x14: it stores rows - clip_b there
 * (0x51F23..0x51F2F) and keeps it. Both buffers are seeded 0xEE. */
static void check_k1_blit_aperture(void)
{
    enum { SCREEN = 320 * 200 };
    u8 *ap = gfx_aperture();
    u8 *back = mem + DSD(DS_000E87A4);
    static u8 s_ap[SCREEN], s_back[SCREEN], got[SCREEN];
    memcpy(s_ap, ap, SCREEN);
    memcpy(s_back, back, SCREEN);

    /* A zero-width node returns before the store (0x51EDF). */
    SpriteNode z; memset(&z, 0, sizeof z);
    z.rows = 5; z.clip_b = 2;
    sprite_blit_aperture(&z);
    CHECK_EQ_INT(z.rows, 5);

    u32 pal = PAL_ENTRY;            /* fake 0x33754 palette-table entry */
    DSB(pal + 8) = 1;               /* start 1 => bank offset 0 */
    SpriteNode n; memset(&n, 0, sizeof n);
    sprite_node_build(&n, 0x2C11u);
    n.pal_ptr = pal;
    n.x = 0; n.y = 0;
    n.rows = 4; n.clip_b = 1;
    SpriteNode m = n;

    /* 0x51E5C into the back buffer: rows restored (0x51ECB), aperture
     * untouched. */
    memset(back, 0xEE, SCREEN);
    memset(ap, 0xEE, SCREEN);
    sprite_blit(&m);
    CHECK_EQ_INT(m.rows, 4);
    CHECK(k1_all(ap, 0xEE, SCREEN), "0x51E5C does not draw to the aperture");
    CHECK(!k1_all(back, 0xEE, SCREEN), "0x51E5C drew into the back buffer");
    memcpy(got, back, SCREEN);

    /* 0x51ED8: the same pixels in the aperture, the back buffer untouched,
     * and the node keeps rows - clip_b = 3. */
    memset(back, 0xEE, SCREEN);
    sprite_blit_aperture(&n);
    CHECK_EQ_INT(n.rows, 3);
    CHECK(memcmp(ap, got, SCREEN) == 0, "0x51ED8 draws 0x51E5C's pixels");
    CHECK(k1_all(back, 0xEE, SCREEN), "0x51ED8 does not draw to the back buffer");

    memcpy(ap, s_ap, SCREEN);
    memcpy(back, s_back, SCREEN);
}

int test_sprite(void)
{
    check_node_build();
    check_rle_cross();
    check_bank_and_colour();
    check_raw_copy();
    check_raw_clipped();
    check_rle_clipped();
    check_rle_row_edges();
    check_rle_mirror();
    check_rle_mirror_clip();
    check_shear();
    check_shear_clipped();
    check_blit_dispatch();
    check_k1_blit_aperture();
    return 0;
}

/* ---- test_render.c ---- */

#define RSCRATCH 0x3F00000u

static void check_list_order(void)
{
    render_list_init();
    CHECK_EQ_INT(render_list_count(), 0);

    /* Three hand-built psets in layer order 5, 1, 3 (insertion order).
     * RSCRATCH = 0x3F00000u, a free region near the top of mem[]: below
     * MEM_SIZE (0x4000000, so 0x04000000 and up are out-of-bounds writes) and
     * above test_gra.c's SCRATCH (0x3000000), which whole .GRA files are
     * loaded into. */
    u32 p1 = RSCRATCH + 0x00u, p2 = RSCRATCH + 0x20u, p3 = RSCRATCH + 0x40u;
    DSW(p1 + 0x0E) = 5; DSW(p2 + 0x0E) = 1; DSW(p3 + 0x0E) = 3;
    CHECK_EQ_INT(render_list_insert(p1), 1);
    CHECK_EQ_INT(render_list_insert(p2), 1);
    CHECK_EQ_INT(render_list_insert(p3), 1);
    render_list_sort();

    CHECK_EQ_INT(render_list_count(), 3);
    u32 n = render_list_head();
    CHECK_EQ_INT(DSD(n + 4), p2); n = DSD(n);
    CHECK_EQ_INT(DSD(n + 4), p3); n = DSD(n);
    CHECK_EQ_INT(DSD(n + 4), p1);

    /* Insertion kept the list ordered, so the sort above was a no-op. Disorder
     * a layer in place (as animation does after insertion) and sort again: a
     * single bubble step would leave p1 in the middle, so this pins a full
     * reorder. */
    DSW(p1 + 0x0E) = 0;
    render_list_sort();
    n = render_list_head();
    CHECK_EQ_INT(DSD(n + 4), p1); n = DSD(n);
    CHECK_EQ_INT(DSD(n + 4), p2); n = DSD(n);
    CHECK_EQ_INT(DSD(n + 4), p3);

    /* Stability: equal layers keep insertion order. */
    render_list_init();
    u32 q1 = RSCRATCH + 0x100u, q2 = RSCRATCH + 0x120u;
    DSW(q1 + 0x0E) = 2; DSW(q2 + 0x0E) = 2;
    CHECK_EQ_INT(render_list_insert(q1), 1);
    CHECK_EQ_INT(render_list_insert(q2), 1);
    render_list_sort();
    n = render_list_head();
    CHECK_EQ_INT(DSD(n + 4), q1); n = DSD(n);
    CHECK_EQ_INT(DSD(n + 4), q2);

    /* Remove returns the node to the free-list and keeps the list sound. */
    render_list_remove(q1);
    CHECK_EQ_INT(render_list_count(), 1);
    CHECK_EQ_INT(DSD(render_list_head() + 4), q2);

    /* Exhaustion: 580 nodes, the 581st insert fails without corrupting. */
    render_list_init();
    for (int i = 0; i < 580; i++)
        CHECK_EQ_INT(render_list_insert(RSCRATCH + 0x200u + (u32)i * 0x20u), 1);
    CHECK_EQ_INT(render_list_insert(RSCRATCH + 0x30000u), 0);
    CHECK_EQ_INT(render_list_count(), 580);
}

static void check_proj_rounding(void)
{
    /* 0x14328's projection: seed +0x800, then for a negative sum add a further
     * 0xFFF (the sbb-side borrow correction), so the result rounds half toward
     * +infinity, NOT half away from zero. The negative cases are the ones a
     * plain >>12 gets wrong, and -2048 distinguishes this form from the
     * +0x1000 variant (which yields -1949). */
    CHECK_EQ_INT(render_proj_x(0), 0);
    CHECK_EQ_INT(render_proj_x(4096), 3901);
    CHECK_EQ_INT(render_proj_x(-4096), -3900);   /* plain >>12 gives -3901 */
    CHECK_EQ_INT(render_proj_x(-1), 0);          /* plain >>12 gives -1 */
    CHECK_EQ_INT(render_proj_x(-2048), -1950);   /* the +0x1000 form gives -1949 */
    CHECK_EQ_INT(render_proj_x(1), (3901 + 0x800) >> 12);
    CHECK_EQ_INT(render_proj_y(4096), 3414);
    CHECK_EQ_INT(render_proj_y(-4096), -3413);

    /* Exhaustive small-range self-consistency pin: p = v*3901 + 0x800; if
     * p < 0, p += 0xFFF; result = p >> 12. The explicit values above, above all
     * -2048, are the discriminators; this loop keeps the idiom honest. */
    for (int v = -8192; v <= 8192; v++) {
        int p = v * 3901 + 0x800;
        if (p < 0) p += 0xFFF;
        CHECK_EQ_INT(render_proj_x(v), p >> 12);
    }
}

static void check_offscreen_skip(void)
{
    render_list_init();

    /* A pset whose sprite is entirely off-screen must be skipped and leave the
     * back buffer untouched. Layer 3 takes the un-shifted coordinate path. */
    u32 back = DSD(DS_000E87A4);
    static u8 copy[320 * 200];
    memcpy(copy, mem + back, sizeof copy);

    u32 p = RSCRATCH + 0x400u;
    DSW(p + 0x00) = 0x2C11u;              /* s16attrc.gra, 9x8 RLE */
    DSD(p + 0x04) = -400;                 /* projected x is far negative */
    DSD(p + 0x08) = 0;
    DSW(p + 0x0E) = 3;
    DSD(p + 0x18) = RSCRATCH + 0x600u;    /* palette pointer; bank byte below */
    DSB(RSCRATCH + 0x600u + 8u) = 1;      /* bank 1 => offset 0 */
    CHECK_EQ_INT(render_list_insert(p), 1);
    render_list();
    CHECK(memcmp(mem + back, copy, sizeof copy) == 0,
          "an off-screen sprite composites nothing");

    /* The same sprite moved on-screen must change the buffer, so the check
     * above cannot pass vacuously. Choose pset x so the projected position is
     * 0: x = proj_x(pset_x) - xorg, so pset_x = the value whose projection
     * equals xorg. Search a small range rather than inverting the projection. */
    GraSprite g; u32 dh = 0;
    CHECK_EQ_INT(gra_sprite_lookup(0x2C11u, &g, &dh), 1);
    int px = 0;
    for (int v = 0; v < 4096; v++)
        if (render_proj_x(v) >= (int)g.xorg) { px = v; break; }
    DSD(p + 0x04) = px;
    render_list();
    CHECK(memcmp(mem + back, copy, sizeof copy) != 0,
          "an on-screen sprite changes the buffer");
}

/* The mode-1/mode-2 y rules. With the five mode globals zero, a layer-3 entry
 * keeps its projected y, a layer-1 entry is forced to y = -camera.y and gets
 * type |= 4, and a following layer-2 entry takes y = last_mode1_y - rows. Since
 * last_mode1_y is 0, the layer-2 node is top-clipped away entirely. Each of the
 * three outcomes is pinned by comparing the composited buffer against nodes
 * built directly and blitted by hand. */
static void check_layer_modes(void)
{
    DSW(DS_00107A3E) = 0; DSW(DS_00107A4E) = 0;
    DSW(DS_00107A3A) = 0; DSW(DS_00107A38) = 0;
    DSD(DS_000F0AEC) = 0;
    /* A non-zero shear ramp makes mode-1's type bit change the raster, so a
     * layer-1 entry cannot pass by accumulating into the raw renderer. Zero the
     * whole ramp first: sprite_render_shear reads one entry per image row, and
     * this test must not depend on whatever the loaded data object holds. */
    for (u32 i = 0; i < 64; i++) DSW(DS_00107900 + i * 2u) = 0;
    DSW(DS_00107900 + 2) = 32;            /* row 1 shifts by 1 */
    DSW(DS_00107900 + 4) = 64;            /* row 2 shifts by 2 */

    render_list_init();
    u32 back = DSD(DS_000E87A4);
    static u8 pristine[320 * 200];
    static u8 got[320 * 200];
    memcpy(pristine, mem + back, sizeof pristine);

    /* Layer 1: the raw sprite 0x2BDF (975x53, pivots 0). Mode-1 forces y to
     * -camera.y == 0 and sets type bit 2, turning raw (2) into clipped shear
     * (0x16) once the 975-wide sprite overhangs the 320-wide clip. Its pset y is
     * set to a value that would be off-screen if the mode-1 rewrite did not
     * happen, so dropping the rewrite is visible. */
    u32 p1 = RSCRATCH + 0x800u;
    DSW(p1 + 0x00) = 0x2BDFu;
    DSD(p1 + 0x04) = 0;                   /* layer <= 2: unshifted -> x = 0 */
    DSD(p1 + 0x08) = 7 * 64;              /* discarded by the mode-1 rewrite */
    DSW(p1 + 0x0E) = 1;
    DSD(p1 + 0x18) = 0;
    CHECK_EQ_INT(render_list_insert(p1), 1);

    /* Layer 2: the 9x8 RLE sprite 0x2C11, following layer 1. Its y is
     * last_mode1_y - rows = 0 - 8 = -8, so it is entirely top-clipped and never
     * blitted. Its pset y makes the fallback branch (y = proj_y(py) - yorg) put
     * it on screen, so ignoring last_mode1_y would draw extra pixels. */
    u32 p2 = RSCRATCH + 0x840u;
    DSW(p2 + 0x00) = 0x2C11u;
    DSD(p2 + 0x04) = 0;                   /* x = proj_x(0) - xorg = -4 */
    DSD(p2 + 0x08) = 144;                 /* proj_y(144) = 120: a visible fallback */
    DSW(p2 + 0x0E) = 2;
    DSD(p2 + 0x18) = 0;
    CHECK_EQ_INT(render_list_insert(p2), 1);

    /* Layer 3: the un-shifted mode path. The stored pset coords are pre-shifted
     * (>>6), so 8<<6 and 144<<6 give px = 8, py = 144 and x = proj_x(8) - 4 = 4,
     * y = proj_y(144) - 4 = 116, both fully on-screen with no clipping. */
    u32 p3 = RSCRATCH + 0x880u;
    DSW(p3 + 0x00) = 0x2C11u;
    DSD(p3 + 0x04) = 8u << 6;
    DSD(p3 + 0x08) = 144u << 6;
    DSW(p3 + 0x0E) = 3;
    DSD(p3 + 0x18) = 0;
    CHECK_EQ_INT(render_list_insert(p3), 1);

    render_list();
    memcpy(got, mem + back, sizeof got);

    /* Rebuild the same composite from directly-built nodes: layer 1 at y = 0
     * with type 0x16 and clip_r 655; layer 3 at (4, 116), type 1, no clip.
     * Layer 2 contributes nothing. */
    memcpy(mem + back, pristine, sizeof pristine);
    SpriteNode n;
    sprite_node_build(&n, 0x2BDFu);
    n.pal_ptr = 0;
    n.type |= 4u | 0x10u;
    n.x = 0; n.y = 0;
    n.clip_l = 0; n.clip_r = 655; n.clip_t = 0; n.clip_b = 0;
    sprite_blit(&n);

    sprite_node_build(&n, 0x2C11u);
    n.pal_ptr = 0;
    n.x = 4; n.y = 116;
    n.clip_l = n.clip_r = n.clip_t = n.clip_b = 0;
    sprite_blit(&n);

    CHECK(memcmp(got, mem + back, sizeof got) == 0,
          "layer modes composite as directly-built nodes");

    /* Restore the shear ramp so later tests start from zero. */
    for (u32 i = 0; i < 64; i++) DSW(DS_00107900 + i * 2u) = 0;
}

/* The expected buffer: the back buffer as it is now, plus `bank`'s rendering of
 * the sprite at (x, y). Writing into a copy of the live back buffer (rather
 * than into a zeroed one) is what makes the comparison valid — the sprite has
 * transparent runs and the buffer has existing content underneath. */
static void expect_composite(u8 *out, u32 icon, int x, int y,
                             const u8 *px, const GraSprite *g, u8 bank)
{
    memcpy(out, mem + icon, 320 * 200);
    CHECK_EQ_INT(sprite_render_rle(px, out + (u32)y * 320u + (u32)x,
                                   g->width, g->height, 320, bank), 0);
}

static void check_end_to_end(void)
{
    render_list_init();

    GraSprite g; u32 dh = 0;
    CHECK_EQ_INT(gra_sprite_lookup(0x2C11u, &g, &dh), 1);
    const u8 *px = NULL;
    CHECK_EQ_INT(gra_sprite_pixels(g.pixel_handle, &px), 1);

    /* pset+0x18 is a 0x33754 palette-table entry ({handle; refcount; start;
     * len}), not a resource handle: sprite_bank reads the low byte of the
     * entry's `start` field at +8. Build two entries in scratch with different
     * start bytes, so the two layers' pixels are distinguishable. (The previous
     * version of this check treated pset+0x18 as a resolvable resource handle
     * and searched handles for differing bank bytes; that encoded the
     * sprite_bank bug the fix removes.) */
    u32 pal_a = RSCRATCH + 0x1000u, pal_b = RSCRATCH + 0x1010u;
    DSB(pal_a + 8) = 1;                 /* start 1 => bank offset 0 */
    DSB(pal_b + 8) = 3;                 /* start 3 => bank offset 2 */
    u8 bank_a = (u8)sprite_bank(pal_a);
    u8 bank_b = (u8)sprite_bank(pal_b);
    CHECK(bank_a != bank_b, "the two palette entries give different banks");

    /* Layer > 2 psets store their coordinates pre-shifted by 6 (render_list
     * decodes them with >> 6), so choose the pset x/y whose projections equal
     * the pivots and land the sprite at (0, 0). */
    int pset_x = 0, pset_y = 0;
    for (int v = 0; v < 4096; v++) {
        if (render_proj_x(v) >= (int)g.xorg) { pset_x = v; break; }
    }
    for (int v = 0; v < 4096; v++) {
        if (render_proj_y(v) >= (int)g.yorg) { pset_y = v; break; }
    }
    int sx = render_proj_x(pset_x) - (int)g.xorg;
    int sy = render_proj_y(pset_y) - (int)g.yorg;

    u32 pa = RSCRATCH + 0xA00u, pb = RSCRATCH + 0xA20u;
    DSW(pa + 0x00) = 0x2C11u; DSD(pa + 0x04) = pset_x << 6;   /* 9x8 RLE, s16attrc */
    DSD(pa + 0x08) = pset_y << 6;  DSW(pa + 0x0E) = 3;
    DSD(pa + 0x18) = pal_a;
    DSW(pb + 0x00) = 0x2C11u; DSD(pb + 0x04) = pset_x << 6;   /* same sprite, other bank */
    DSD(pb + 0x08) = pset_y << 6;  DSW(pb + 0x0E) = 4;
    DSD(pb + 0x18) = pal_b;

    static u8 expect[320 * 200];
    u32 back = DSD(DS_000E87A4);

    /* Layer 4 (bank_b) is higher, so it wins where they overlap. */
    expect_composite(expect, back, sx, sy, px, &g, bank_b);
    CHECK_EQ_INT(render_list_insert(pa), 1);
    CHECK_EQ_INT(render_list_insert(pb), 1);
    render_list_sort();
    render_list();
    CHECK(memcmp(mem + back, expect, sizeof expect) == 0,
          "the higher layer's pixels win");

    /* Swap the layers: bank_a must now win. Two-sided by construction —
     * if neither pset composited, neither assertion holds. */
    DSW(pa + 0x0E) = 4; DSW(pb + 0x0E) = 3;
    expect_composite(expect, back, sx, sy, px, &g, bank_a);
    render_list_sort();
    render_list();
    CHECK(memcmp(mem + back, expect, sizeof expect) == 0,
          "swapping the layers swaps the winner");
}

/* 0x389C4 / 0x38A38: the attract scroll/zoom projection. Seed the inputs, run
 * the raw arithmetic, and assert exact words (no tolerance). Every global the
 * two functions touch is saved and restored, including the shear-table
 * entries. The expected values come from the raw disassembly recorded in
 * docs/superpowers/plans/2026-09-19-attract-derivations.md: the raw divides by
 * 2^n through MSVC's `sar`/`shl`/`sbb` idiom, which truncates toward zero
 * (add 2^n-1 when negative), so a plain arithmetic shift is wrong for negative
 * inputs. */
static void check_scroll_projection(void)
{
    u16 s38 = DSW(DS_00107A38), s3a = DSW(DS_00107A3A);
    u16 s3c = DSW(DS_00107A3C), s3e = DSW(DS_00107A3E);
    u16 s40 = DSW(DS_00107A40), s42 = DSW(DS_00107A42);
    u16 s46 = DSW(DS_00107A44 + 2), s48 = DSW(DS_00107A48);
    u16 s4a = DSW(DS_00107A4A), s4c = DSW(DS_00107A4C);
    u16 s4e = DSW(DS_00107A4E), s50 = DSW(DS_00107A50);
    u16 s52 = DSW(DS_00107A52);
    u32 sf0 = DSD(DS_000F0AF0);
    u16 tbl[0x40];
    for (u32 i = 0; i < 0x40; i++) tbl[i] = DSW(DS_00107900 + i * 2u);

    /* Case A (0x389C4, DS_00107A3C < 1): DS_00107A38 = 0x0400 / 64 = 0x0010,
     * then DS_00107A4C = DS_00107A4E = 0x00AB. */
    DSW(DS_00107A3C) = 0;
    DSW(DS_00107A48) = 0x0400;
    DSW(DS_00107A4E) = 0x00AB;
    render_scroll_edge();
    CHECK_EQ_INT((int)DSW(DS_00107A38), 0x0010);
    CHECK_EQ_INT((int)DSW(DS_00107A4C), 0x00AB);

    /* Case B (0x389C4, DS_00107A3C >= 1, early return): the subtrahend is the
     * s16 high word of DSD(DS_00107A3A), i.e. DS_00107A3C = 65, so
     * x = 0 - 65 = -65. The raw's signed /64 gives (-65 + 63) >> 6 = -1 =
     * 0xFFFF; a plain >> 6 would floor to -2 = 0xFFFE. DS_00107A4A (10) >
     * DS_00107A4C (5), the `ja` early return, so 4C stays 5. */
    DSW(DS_00107A3C) = 65;
    DSW(DS_00107A48) = 0;
    DSW(DS_00107A4A) = 10;
    DSW(DS_00107A4C) = 5;
    render_scroll_edge();
    CHECK_EQ_INT((int)DSW(DS_00107A38), 0xFFFF);
    CHECK_EQ_INT((int)DSW(DS_00107A4C), 5);

    /* Case B' (0x389C4, DS_00107A3C >= 1, no early return): DS_00107A4A (3) <=
     * DS_00107A4C (5), so DS_00107A4C = 3. Same x and same 0xFFFF. */
    DSW(DS_00107A4A) = 3;
    DSW(DS_00107A4C) = 5;
    render_scroll_edge();
    CHECK_EQ_INT((int)DSW(DS_00107A38), 0xFFFF);
    CHECK_EQ_INT((int)DSW(DS_00107A4C), 3);

    /* Case C (0x38A38): stride = 0x100 << 8 = 0x10000, step = 0x10000 / 3 =
     * 21845. DS_00107A52 = 4 rows (indices 3,2,1,0), and each table entry is
     * the raw's truncating /256 followed by the arithmetic `sar edx,1`:
     *   i=3 row 0x10000 -> (256)>>1 = 0x0080
     *   i=2 row 0x0AAAB -> (170)>>1 = 0x0055
     *   i=1 row 0x05556 ->  (85)>>1 = 0x002A
     *   i=0 row 0x00001 ->   (0)>>1 = 0x0000
     * filled downward.
     * edge = 0xEE + 3 = 0xF1; rows i=3 (0xF1) and i=2 (0xF0) skip the
     * DS_00107A3E store because their low 16 exceed 0xEF. i=1 (0xEF) writes
     * (0x2A + 0x2B00) / 32 = 0x0159; i=0 (0xEE) writes 0x0158.
     * Tail: after the loop row = 0x10000 - 4*21845 = -21844; minus
     * DS_00107A42 (2) * 21845 = -65534; /256 = -255 = 0xFF01. Then
     * ((-255 >> 1) + 0x41) / 32 = (-128 + 65) / 32 = -1 = 0xFFFF. */
    DSD(DS_000F0AF0) = 0x100u;
    DSW(DS_00107A40) = 3;
    DSW(DS_00107A52) = 4;
    DSW(DS_00107A4C) = 0x00EE;
    DSW(DS_00107A42) = 2;
    DSW(DS_00107A50) = 0x0041;
    DSW(DS_00107A3E) = 0xDEAD;
    render_scroll_fill();
    CHECK_EQ_INT((int)DSW(DS_00107900 + 0), 0x0000);
    CHECK_EQ_INT((int)DSW(DS_00107900 + 2), 0x002A);
    CHECK_EQ_INT((int)DSW(DS_00107900 + 4), 0x0055);
    CHECK_EQ_INT((int)DSW(DS_00107900 + 6), 0x0080);
    CHECK_EQ_INT((int)DSW(DS_00107A3E), 0x0158);
    CHECK_EQ_INT((int)DSW(DS_00107A44 + 2), 0xFF01);
    CHECK_EQ_INT((int)DSW(DS_00107A3A), 0xFFFF);

    /* Case D (0x38A38, every row skips the DS_00107A3E store): one row with
     * edge = 0x100 > 0xEF, so the seeded sentinel must survive. The row value
     * is (0x10000 / 256) >> 1 = 0x0080. */
    DSD(DS_000F0AF0) = 0x100u;
    DSW(DS_00107A40) = 3;
    DSW(DS_00107A52) = 1;
    DSW(DS_00107A4C) = 0x0100;
    DSW(DS_00107A42) = 0;
    DSW(DS_00107A50) = 0x0041;
    DSW(DS_00107A3E) = 0xDEAD;
    render_scroll_fill();
    CHECK_EQ_INT((int)DSW(DS_00107900 + 0), 0x0080);
    CHECK_EQ_INT((int)DSW(DS_00107A3E), 0xDEAD);

    /* Case E: a single row with a NONZERO t (t = (0x10000 / 256) >> 1 = 0x80)
     * and edge = 0 within range writes the shifted t, (0x80 + 0x2B00) / 32 =
     * 0x015C. Case C only observes t == 0 (its last row overwrites the earlier
     * nonzero store), so this pins the shifted-vs-unshifted store. */
    DSD(DS_000F0AF0) = 0x100u;
    DSW(DS_00107A40) = 3;
    DSW(DS_00107A52) = 1;
    DSW(DS_00107A4C) = 0;
    DSW(DS_00107A42) = 0;
    DSW(DS_00107A50) = 0x0041;
    DSW(DS_00107A3E) = 0xDEAD;
    render_scroll_fill();
    CHECK_EQ_INT((int)DSW(DS_00107900 + 0), 0x0080);
    CHECK_EQ_INT((int)DSW(DS_00107A3E), 0x015C);

    DSW(DS_00107A38) = s38; DSW(DS_00107A3A) = s3a;
    DSW(DS_00107A3C) = s3c; DSW(DS_00107A3E) = s3e;
    DSW(DS_00107A40) = s40; DSW(DS_00107A42) = s42;
    DSW(DS_00107A44 + 2) = s46; DSW(DS_00107A48) = s48;
    DSW(DS_00107A4A) = s4a; DSW(DS_00107A4C) = s4c;
    DSW(DS_00107A4E) = s4e; DSW(DS_00107A50) = s50;
    DSW(DS_00107A52) = s52;
    DSD(DS_000F0AF0) = sf0;
    for (u32 i = 0; i < 0x40; i++) DSW(DS_00107900 + i * 2u) = tbl[i];
}

/* 0x4F228 (render_projection_reset): DS_00107A54 = 0 (AH zeroed at 0x4F22A),
 * DS_00107A55 = the caller's AL, and the words DS_00107A3A/DS_00107A38 = 0.
 * Every target seeded with a value its post-condition differs from, and the
 * neighbours DS_00107A53/56, DS_00107A3C and DS_00107A36 kept. */
static void check_projection_reset(void)
{
    u8 s53 = DSB(DS_00107A54 - 1u), s54 = DSB(DS_00107A54);
    u8 s55 = DSB(DS_00107A55), s56 = DSB(DS_00107A55 + 1u);
    u16 s36 = DSW(DS_00107A38 - 2u), s38 = DSW(DS_00107A38);
    u16 s3a = DSW(DS_00107A3A), s3c = DSW(DS_00107A3C);

    DSB(DS_00107A54 - 1u) = 0x5Au;
    DSB(DS_00107A54) = 1u;
    DSB(DS_00107A55) = 0xEEu;
    DSB(DS_00107A55 + 1u) = 0xA5u;
    DSW(DS_00107A38 - 2u) = 0x6666u;
    DSW(DS_00107A38) = 0x4321u;
    DSW(DS_00107A3A) = 0x1234u;
    DSW(DS_00107A3C) = 0x7777u;
    render_projection_reset(0x3Cu);
    CHECK_EQ_INT((int)DSB(DS_00107A54), 0);
    CHECK_EQ_INT((int)DSB(DS_00107A55), 0x3C);
    CHECK_EQ_INT((int)DSW(DS_00107A3A), 0);
    CHECK_EQ_INT((int)DSW(DS_00107A38), 0);
    CHECK_EQ_INT((int)DSB(DS_00107A54 - 1u), 0x5A);
    CHECK_EQ_INT((int)DSB(DS_00107A55 + 1u), 0xA5);
    CHECK_EQ_INT((int)DSW(DS_00107A38 - 2u), 0x6666);
    CHECK_EQ_INT((int)DSW(DS_00107A3C), 0x7777);

    DSB(DS_00107A54 - 1u) = s53; DSB(DS_00107A54) = s54;
    DSB(DS_00107A55) = s55; DSB(DS_00107A55 + 1u) = s56;
    DSW(DS_00107A38 - 2u) = s36; DSW(DS_00107A38) = s38;
    DSW(DS_00107A3A) = s3a; DSW(DS_00107A3C) = s3c;
}

/* 0x38990 (render_scroll_track, record §K3.1): DS_00107A3C = the low word of
 * DS_000F0AEC with `and al,0xc0` (0x38997), and DS_00107A4A = the dword
 * DS_000F0AEC through the raw's truncating /64 plus the zero-extended word
 * DS_00107A4E, low 16 bits stored (0x389BC). The four neighbouring words are
 * seeded and must survive (both stores are word stores). Every sentinel differs
 * from its post-condition. */
static void check_scroll_track(void)
{
    u32 sf0 = DSD(DS_000F0AEC);
    u16 s3a = DSW(DS_00107A3A), s3c = DSW(DS_00107A3C), s3e = DSW(DS_00107A3E);
    u16 s48 = DSW(DS_00107A48), s4a = DSW(DS_00107A4A), s4c = DSW(DS_00107A4C);
    u16 s4e = DSW(DS_00107A4E);

    DSW(DS_00107A3A) = 0x1111u; DSW(DS_00107A3E) = 0x2222u;
    DSW(DS_00107A48) = 0x3333u; DSW(DS_00107A4C) = 0x4444u;

    /* (a) 0x00012345: 0x2345 & 0xFFC0 = 0x2340; the dword (not the word)
     * divides, 0x12345 / 64 = 0x48D, + 0x10 = 0x49D. */
    DSD(DS_000F0AEC) = 0x00012345u;
    DSW(DS_00107A4E) = 0x0010u;
    DSW(DS_00107A3C) = 0x7777u; DSW(DS_00107A4A) = 0x7777u;
    render_scroll_track();
    CHECK_EQ_INT((int)DSW(DS_00107A3C), 0x2340);
    CHECK_EQ_INT((int)DSW(DS_00107A4A), 0x049D);

    /* (b) -127 (0xFFFFFF81): 0xFF81 & 0xFFC0 = 0xFF80; the `sar`/`shl`/`sbb`
     * divide truncates toward zero, -127 / 64 = -1, + 0x10 = 0x000F (a
     * flooring >> 6 would give -2 + 0x10 = 0x000E). */
    DSD(DS_000F0AEC) = 0xFFFFFF81u;
    DSW(DS_00107A3C) = 0x7777u; DSW(DS_00107A4A) = 0x7777u;
    render_scroll_track();
    CHECK_EQ_INT((int)DSW(DS_00107A3C), 0xFF80);
    CHECK_EQ_INT((int)DSW(DS_00107A4A), 0x000F);

    /* (c) 0x0000FFFF with DS_00107A4E = 0xFFF0: only the low byte is masked
     * (0xFFC0), and 0x3FF + 0xFFF0 = 0x103EF stores its low word 0x03EF. */
    DSD(DS_000F0AEC) = 0x0000FFFFu;
    DSW(DS_00107A4E) = 0xFFF0u;
    DSW(DS_00107A3C) = 0x7777u; DSW(DS_00107A4A) = 0x7777u;
    render_scroll_track();
    CHECK_EQ_INT((int)DSW(DS_00107A3C), 0xFFC0);
    CHECK_EQ_INT((int)DSW(DS_00107A4A), 0x03EF);

    CHECK_EQ_INT((int)DSW(DS_00107A3A), 0x1111);
    CHECK_EQ_INT((int)DSW(DS_00107A3E), 0x2222);
    CHECK_EQ_INT((int)DSW(DS_00107A48), 0x3333);
    CHECK_EQ_INT((int)DSW(DS_00107A4C), 0x4444);
    CHECK_EQ_INT((int)DSW(DS_00107A4E), 0xFFF0);
    CHECK_EQ_INT((int)DSD(DS_000F0AEC), 0x0000FFFF);

    DSD(DS_000F0AEC) = sf0;
    DSW(DS_00107A3A) = s3a; DSW(DS_00107A3C) = s3c; DSW(DS_00107A3E) = s3e;
    DSW(DS_00107A48) = s48; DSW(DS_00107A4A) = s4a; DSW(DS_00107A4C) = s4c;
    DSW(DS_00107A4E) = s4e;
}

int test_render(void)
{
    check_list_order();
    check_proj_rounding();
    check_offscreen_skip();
    check_layer_modes();
    check_end_to_end();
    check_scroll_projection();
    check_scroll_track();
    check_projection_reset();
    return 0;
}

/* ---- test_input.c ---- */

/* PORT: scratch linear address inside mem[] the test points the BIOS base
 * (DS_00101514) at, matching game_init()'s GAME_BIOS_BASE. A host pointer would
 * break the mem[] linear-address invariant. */
#define TEST_KEY_BASE 0x3000000u

/* Dequeues one key, failing instead of blocking forever: input_get_key() spins
 * on host_pump() while empty, and the headless suite has no window to produce a
 * key, so a missing key must fail rather than hang. */
static int take(void)
{
    CHECK(input_has_key(), "key present before input_get_key");
    if (!input_has_key()) return -1;
    return (int)input_get_key();
}

int test_input(void)
{
    int before = g_failures;

    input_clear();
    CHECK(!input_has_key(), "cleared queue is empty");
    CHECK_EQ_INT(input_check_key(), 0);

    /* FIFO order and (scan << 8) | ascii packing; check_key() peeks. */
    input_push(0x1E, 'a');
    input_push(0x10, 'Q');
    CHECK(input_has_key(), "has_key after pushes");
    CHECK_EQ_INT(input_check_key(), (0x1E << 8) | 'a');
    CHECK(input_has_key(), "check_key did not consume");
    CHECK_EQ_INT(take(), (0x1E << 8) | 'a');
    CHECK_EQ_INT(take(), (0x10 << 8) | 'Q');
    CHECK(!input_has_key(), "queue drained in FIFO order");

    /* Packing edges: scan 0, ascii 0, both 0xFF. */
    input_push(0x00, 'x');
    input_push(0x48, 0x00);
    input_push(0xFF, 0xFF);
    CHECK_EQ_INT(take(), 0x0078);
    CHECK_EQ_INT(take(), 0x4800);
    CHECK_EQ_INT(take(), 0xFFFF);

    /* input_clear() empties a non-empty queue. */
    input_push(0x1E, 0x1E);
    input_clear();
    CHECK(!input_has_key(), "input_clear() empties the queue");
    CHECK_EQ_INT(input_check_key(), 0);

    /* Overflow drops the OLDEST entry: pushing 0..99 into the 64-entry ring
     * leaves exactly 36..99, in order. Draining the whole window and asserting
     * each value proves which entries survived, not merely that the count
     * capped (a "drop newest" bug would leave 0..63 and fail on the first). */
    input_clear();
    for (int i = 0; i < 100; i++) input_push((u8)i, (u8)(i ^ 0x5A));
    for (int i = 36; i < 100; i++)
        CHECK_EQ_INT(take(), ((i & 0xFF) << 8) | (i ^ 0x5A));
    CHECK(!input_has_key(), "ring held exactly 64 entries");

    {
        /* The input bitfield. 0x50161 is a level/latch selector over the raw
         * level word DAT_000E1C34; 0x4F644 turns it into the two masks the game
         * reads. Host bits enter through the key bitmap at
         * DS_00101514 + 0x2d8/0x2d9, which the test drives directly. */
        u32 saved_base = DSD(DS_00101514);
        u32 saved_30 = DSD(DS_000E1C30);
        u32 saved_34 = DSD(DS_000E1C34);
        u32 saved_38 = DSD(DS_000E1C38);
        u32 saved_3c = DSD(DS_000E1C3C);
        u16 saved_40 = DSW(DS_000E1C40);
        u8 saved_e4[4], saved_d8[4];

        mem_fill(TEST_KEY_BASE, 0, 0x400);
        DSD(DS_00101514) = TEST_KEY_BASE;
        for (u32 i = 0; i < 4u; i++) {
            saved_e4[i] = DSB(DS_001088E4 + i);
            saved_d8[i] = DSB(DS_001088D8 + i);
        }
        DSD(DS_000E1C30) = 0; DSD(DS_000E1C34) = 0; DSD(DS_000E1C38) = 0;
        DSD(DS_000E1C3C) = 0; DSW(DS_000E1C40) = 0;

        /* 0x2D2F0 is a constant 0 (`xor eax,eax; ret`). */
        CHECK_EQ_INT((int)input_joystick_device(0xFFu), 0);

        /* 0x500C4: level word = (byte[+0x2d8] << 24) | (byte[+0x2d9] << 8).
         * The debounce holds a bit's previous level for one frame, so a press
         * needs two samples to appear. */
        DSB(TEST_KEY_BASE + 0x2d9) = 0x01;           /* mask 0x00000100 */
        CHECK_EQ_INT((int)input_pump(), 0);          /* change is debounced */
        CHECK_EQ_INT((int)input_pump(), 0x00000100); /* second sample holds it */

        /* 0x4F644: with the joystick accessor 0 the merge never fires, so
         * DS_001088E4 is the newly-pressed bits and DS_001088D8 the held bits.
         * The press is visible in E4 for exactly one frame. */
        input_state_update();
        CHECK_EQ_INT((int)DSD(DS_001088E4), 0x00000100);
        CHECK_EQ_INT((int)DSD(DS_001088D8), 0x00000100);

        input_state_update();
        CHECK_EQ_INT((int)DSD(DS_001088E4), 0);          /* no longer new */
        CHECK_EQ_INT((int)DSD(DS_001088D8), 0x00000100); /* still held */

        /* Release: the debounce holds the level one more frame, then drops it;
         * both masks clear. E4 is the newly-pressed (rising) edge, so it stays
         * clear through a release. */
        DSB(TEST_KEY_BASE + 0x2d9) = 0;
        CHECK_EQ_INT((int)input_pump(), 0x00000100);  /* still held this frame */
        CHECK_EQ_INT((int)input_pump(), 0);           /* dropped on the second */
        input_state_update();
        CHECK_EQ_INT((int)DSD(DS_001088E4), 0);
        CHECK_EQ_INT((int)DSD(DS_001088D8), 0);

        /* 0x50161 applies its mask to the latch and leaves the unmasked bits of
         * the level word alone. */
        DSD(DS_000E1C34) = 0x0F000000u;
        DSD(DS_000E1C38) = 0;
        CHECK_EQ_INT((int)input_select_bits(0x01000000u), 0x0F000000);
        CHECK_EQ_INT((int)DSD(DS_000E1C38), 0x01000000);
        CHECK_EQ_INT((int)input_select_bits(0x10000000u), 0x0F000000);
        CHECK_EQ_INT((int)DSD(DS_000E1C38), 0x01000000);   /* bit 28 not live yet */
        DSD(DS_000E1C34) = 0x1F000000u;                    /* now bit 28 is live */
        CHECK_EQ_INT((int)input_select_bits(0x10000000u), 0x1F000000);
        CHECK_EQ_INT((int)DSD(DS_000E1C38), 0x11000000);   /* latched, not held */

        DSD(DS_00101514) = saved_base;
        DSD(DS_000E1C30) = saved_30; DSD(DS_000E1C34) = saved_34;
        DSD(DS_000E1C38) = saved_38; DSD(DS_000E1C3C) = saved_3c;
        DSW(DS_000E1C40) = saved_40;
        for (u32 i = 0; i < 4u; i++) {
            DSB(DS_001088E4 + i) = saved_e4[i];
            DSB(DS_001088D8 + i) = saved_d8[i];
        }
    }

    return g_failures - before;
}

/* ---- test_host.c ---- */

static unsigned long long now_ns(void)
{
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (unsigned long long)ts.tv_sec * 1000000000ull +
           (unsigned long long)ts.tv_nsec;
}

/* Record named-gaps-b §B.4: the CPU-fault end of the run (host_cpu_fault). */
static jmp_buf hf_jb;
static volatile u32 hf_exc, hf_eip;
static void hf_hook(u32 exc, u32 eip) { hf_exc = exc; hf_eip = eip; longjmp(hf_jb, 1); }

static void hf_check_hook(void)
{
    volatile int landed = 0;
    host_fault_hook_fn prev = host_set_fault_hook(hf_hook);
    hf_exc = 0x5Au; hf_eip = 0x5A5Au;
    if (setjmp(hf_jb) == 0) host_cpu_fault(0x0Eu, 0x32578u, "unused", 3); else landed = 1;
    CHECK_EQ_INT(landed, 1);
    CHECK_EQ_INT((long)hf_exc, 0x0E);
    CHECK_EQ_INT((long)hf_eip, 0x32578);
    CHECK(host_set_fault_hook(prev) == hf_hook, "the hook is handed back");
}

static void hf_check_exit(void)
{
    int fds[2];
    CHECK(pipe(fds) == 0, "pipe");
    fflush(stdout); fflush(stderr);          /* the child must not re-flush our buffers */
    pid_t pid = fork();
    if (pid == 0) {
        dup2(fds[1], 2); close(fds[0]);
        host_cpu_fault(0x00u, 0x334E0u, NULL, 7);
    }
    close(fds[1]);
    char buf[256];
    ssize_t n = read(fds[0], buf, sizeof buf - 1);
    close(fds[0]);
    buf[n > 0 ? n : 0] = '\0';
    int st = 0;
    waitpid(pid, &st, 0);
    CHECK(WIFEXITED(st), "the fault ends the process");
    CHECK_EQ_INT(WEXITSTATUS(st), 7);
    CHECK(strstr(buf, "00h") != NULL && strstr(buf, "000334E0") != NULL,
          "the NULL-message line names the exception and the address");
}

int test_host(void)
{
    int before = g_failures;

    /* The default binding (record gameplay-ground-truth §G.1.2; every bit
     * captured in gp-pads, §G.7.2): a set-1 scan's kb bit. */
    {
        static const u8 scans[18] = { 0x1F, 0x2D, 0x2C, 0x2E, 0x16, 0x17, 0x31, 0x32, 0x3B,
                                      0x48, 0x50, 0x4B, 0x4D, 0x47, 0x49, 0x4F, 0x51, 0x3C };
        static const u16 bits[18] = { 0x8000, 0x4000, 0x2000, 0x1000, 0x0100, 0x0200, 0x0400, 0x0800, 0x0100,
                                      0x0080, 0x0040, 0x0020, 0x0010, 0x0001, 0x0002, 0x0004, 0x0008, 0x0001 };
        for (int i = 0; i < 18; i++) CHECK_EQ_INT(host_kb_bit(scans[i]), bits[i]);
        CHECK_EQ_INT(host_kb_bit(0x10), 0);     /* Q: Alt-Q's letter is no pad key */
        CHECK_EQ_INT(host_kb_bit(0x38), 0);     /* Alt: read by nothing (record u11 §K.2) */
        CHECK_EQ_INT(host_kb_bit(0x06), 0);     /* '5': the stale "coin" binding */
    }

    /* host_pump()/host_present_rgb()/host_shutdown() before host_init(): the
     * suite runs headless with no window, so all three must be safe no-ops. */
    host_shutdown();
    host_pump();
    host_present_rgb((const u8 *)"\x01\x02\x03", 1, 1);

    /* host_init() reports failure instead of aborting. w <= 0 fails the guard
     * before SDL is touched, so the suite never opens a window. */
    CHECK_EQ_INT(host_init("pr-test", 0, 0), 0);
    host_shutdown();

    /* The real host advances the tick from a clock; the weak no-op left
     * host_tick_count() pinned at 0. host_wait_vblank() sleeps to the next 60 Hz
     * boundary then pumps, so a bounded loop must see the tick move. */
    u32 t0 = host_tick_count();
    unsigned long long deadline = now_ns() + 500ull * 1000000ull;
    while (host_tick_count() == t0 && now_ns() < deadline) host_wait_vblank();
    CHECK(host_tick_count() > t0, "host_wait_vblank advances host_tick_count");
    CHECK(host_tick_count() >= t0, "tick does not go backwards");

    /* A pump with no elapsed interval must not invent ticks (the catch-up clamp
     * must preserve normal pacing). */
    u32 tprev = host_tick_count();
    host_pump();
    CHECK_EQ_INT((int)host_tick_count(), (int)tprev);

    /* File round-trip, over-max rejection, and a missing file. The over-max read
     * must fail cleanly and leave the destination untouched. */
    const char *path = "port_host_test.tmp";
    const u8 out[5] = { 'p', 'r', 'a', 'g', 'e' };
    u8 in[8];
    u32 n = 0;
    CHECK(host_write_file(path, out, sizeof out), "host_write_file succeeds");
    CHECK(host_read_file(path, in, sizeof in, &n), "host_read_file succeeds");
    CHECK_EQ_INT((int)n, (int)sizeof out);
    CHECK(in[0] == 'p' && in[4] == 'e', "file contents round-trip");
    for (size_t i = 0; i < sizeof in; i++) in[i] = 0xAB;
    CHECK(!host_read_file(path, in, 4, &n), "file larger than max fails");
    CHECK(in[0] == 0xAB && in[7] == 0xAB, "over-max read writes nothing");
    CHECK(!host_read_file("no/such/prage/file", in, sizeof in, &n),
          "missing file fails");

    /* A sparse file whose low 32 bits fit `max` must still be rejected: the
     * guard compares the real 64-bit size, not a truncated u32. The old
     * narrowing bug passed the guard here and ran fread(size) into `in`. */
    const char *big = "port_host_big.tmp";
    FILE *bf = fopen(big, "wb");
    if (bf) {
        if (fseek(bf, (long)(4294967296ll + 2 - 1), SEEK_SET) != 0 ||
            fputc(0, bf) == EOF) {
            fclose(bf);
        } else {
            fclose(bf);
            for (size_t i = 0; i < sizeof in; i++) in[i] = 0xCD;
            CHECK(!host_read_file(big, in, 4, &n),
                  "file whose low 32 bits fit max is still rejected");
            CHECK(in[0] == 0xCD && in[7] == 0xCD,
                  "sparse over-size read writes nothing");
        }
        remove(big);
    }
    remove(path);

    /* Audio seam (Task 5). The suite must never open a real device, so every
     * assertion here stays on the closed/no-device path: submit is a no-op and
     * open() of an impossible profile fails through the precondition guard
     * before SDL is touched. */
    host_audio_close();                                  /* before any open */
    CHECK_EQ_INT((int)host_audio_rate(), 0);
    const s16 audio[4] = { 0, 0, 0, 0 };
    host_audio_submit(audio, 2);                         /* no device: no-op */
    host_audio_submit(NULL, 2);
    host_audio_submit(audio, 0);
    host_audio_submit(audio, -1);                        /* negative count */
    CHECK_EQ_INT((int)host_audio_rate(), 0);             /* submits kept seam closed */
    host_audio_close();                                  /* safe after no-op submits */
    CHECK_EQ_INT((int)host_audio_rate(), 0);
    CHECK_EQ_INT(host_audio_open(0, 2), 0);              /* rate <= 0 */
    CHECK_EQ_INT(host_audio_open(44100, 0), 0);          /* channels <= 0 */
    CHECK_EQ_INT(host_audio_open(-44100, -2), 0);
    CHECK_EQ_INT((int)host_audio_rate(), 0);             /* still closed */
    host_audio_close();
    host_audio_close();                                  /* idempotent */
    CHECK_EQ_INT((int)host_audio_rate(), 0);

    /* host_shutdown() tears the audio seam down first, so afterwards every
     * audio entry point must still be a safe no-op reading rate 0. */
    host_shutdown();
    host_audio_submit(audio, 2);
    host_audio_close();
    CHECK_EQ_INT((int)host_audio_rate(), 0);

    /* The K11 key-bits seam (named-gaps A record §A.3): the driver holds the
     * key bitmap game_loop copies from host_key_bits(). The sentinel 0x1234 is
     * no SDL keyboard state a headless run can report. */
    host_set_key_bits_override(0x1234u, 1);
    CHECK_EQ_INT((int)host_key_bits(), 0x1234);
    host_set_key_bits_override(0x1234u, 0);
    CHECK(host_key_bits() != 0x1234u, "the key-bits override is off again");

    hf_check_hook();
    hf_check_exit();
    return g_failures - before;
}

/* ---- test_rng.c ---- */

int test_rng(void)
{
    /* The recurrence's own outputs, computed independently from
     * state = state*0xB90D12B9 + 0x38CE051F seeded 0xABCD. */
    static const u32 ranges[] = { 0x5Au, 0x7Eu, 2u, 0xFFFFu, 0x10000u, 0u, 0x7FFFFFFFu };
    static const u32 expect[] = { 12u, 111u, 0u, 29617u, 0u, 0u, 11010u };

    rng_seed(0xABCDu);
    CHECK_EQ_INT((int)DSD(DS_000EF6D8), 0xABCD);
    for (unsigned i = 0; i < sizeof ranges / sizeof ranges[0]; i++)
        CHECK_EQ_INT((int)rng_next(ranges[i]), (int)expect[i]);

    /* A range wider than 16 bits is masked, and 0 must not shift undefinedly. */
    rng_seed(0xABCDu);
    CHECK_EQ_INT((int)rng_next(0x7FFFFFFFu), 9158);
    CHECK_EQ_INT((int)rng_next(0u), 0);

    /* rng_step advances the state and discards the value. */
    rng_seed(0xABCDu);
    u32 before = DSD(DS_000EF6D8);
    rng_step();
    CHECK(DSD(DS_000EF6D8) != before, "rng_step advanced the state");
    CHECK_EQ_INT((int)DSD(DS_000EF6D8), (int)(0xABCDu * 0xB90D12B9u + 0x38CE051Fu));

    /* The three title draws, in order, are what Task 1 pins the original to:
     * the immediates 12, 111, 0 in tools/title_pin.py must equal these. */
    rng_seed(0xABCDu);
    CHECK_EQ_INT((int)rng_next(0x5Au), 12);
    CHECK_EQ_INT((int)rng_next(0x7Eu), 111);
    CHECK_EQ_INT((int)rng_next(2u), 0);
    return 0;
}

/* ---- test_text.c ---- */

/* port/tests/test_text.c — the text renderer (Task 8b): 0x2F830 and its
 * 0x2F5A0 glyph emitter. The raw disassembly shows text is not a framebuffer
 * blit: 0x2F5A0 releases the cell's old record through 0x2AD40 and spawns each
 * non-space glyph as an actor through 0x2AE14, storing the record offset in the
 * 43-wide cell grid at DS_00105F38. The glyph's pixels therefore arrive later
 * through the actor display list (0x1C390), so the assertions below check the
 * grid, the spawned glyph's pset sprite id and position, and the cursor extent,
 * all hand-computed from the disassembly and the loaded font tables at
 * DS 0x3D048 / 0x3D1EC / 0x3CD7C / 0x3D38D. */






static u32 grid(s32 row, s32 col)
{
    return DSD(DS_00105F38 + (u32)row * 0xacu + (u32)col * 4u);
}

/* The mode-0 glyph's sprite id in cell (row, col), or 0 for an empty cell. */
static u32 grid_sprite(s32 row, s32 col)
{
    u32 r = grid(row, col);
    return r != 0 ? (u32)(DSW(actor_pset(r)) & 0x7fffu) : 0u;
}

/* 0x2EFD4/0x2F4D0/0x2F20C/0x2F314 (record §33). Mode-0 sprite ids come from
 * the font table at 0xBCD7C: '0'..'9' = 0x3F45..0x3F4E, 'A' 0x3F56, 'B'
 * 0x3F57, 'C' 0x3F58, the 0x1B HIT glyph 0x3F30. Every destination byte, cell
 * and cursor word is seeded with a sentinel that differs from its result. */
static void check_text_vertical_number(void)
{
    u8 d[8];

    /* 0x2EFD4. Pad 3 copies the digits alone and terminates at dest[len]. */
    memset(d, 0xEE, sizeof d);
    CHECK_EQ_INT(text_number_format(2, d, 2, 3u), 1);
    CHECK_EQ_INT(d[0], '2');
    CHECK_EQ_INT(d[1], 0);
    CHECK_EQ_INT(d[2], 0xEE);
    /* Pad 1 right-justifies with spaces, pad 0 with '0', pad 2 left-justifies;
     * all terminate at dest[width]. */
    memset(d, 0xEE, sizeof d);
    CHECK_EQ_INT(text_number_format(7, d, 3, 1u), 1);
    CHECK_EQ_INT(memcmp(d, "  7", 4), 0);
    CHECK_EQ_INT(d[4], 0xEE);
    memset(d, 0xEE, sizeof d);
    CHECK_EQ_INT(text_number_format(7, d, 3, 0u), 1);
    CHECK_EQ_INT(memcmp(d, "007", 4), 0);
    memset(d, 0xEE, sizeof d);
    CHECK_EQ_INT(text_number_format(7, d, 3, 2u), 1);
    CHECK_EQ_INT(memcmp(d, "7  ", 4), 0);
    /* The sign is one of the digits: '0' fills in front of it. */
    memset(d, 0xEE, sizeof d);
    CHECK_EQ_INT(text_number_format(-5, d, 3, 0u), 2);
    CHECK_EQ_INT(memcmp(d, "0-5", 4), 0);
    /* width <= len keeps the last `width` characters, whatever the pad. */
    memset(d, 0xEE, sizeof d);
    CHECK_EQ_INT(text_number_format(12345, d, 3, 1u), 5);
    CHECK_EQ_INT(memcmp(d, "345", 4), 0);
    memset(d, 0xEE, sizeof d);
    CHECK_EQ_INT(text_number_format(42, d, 2, 3u), 2);
    CHECK_EQ_INT(memcmp(d, "42", 3), 0);
    memset(d, 0xEE, sizeof d);
    CHECK_EQ_INT(text_number_format(42, d, 2, 4u), 2);  /* width == len */
    CHECK_EQ_INT(memcmp(d, "42", 3), 0);
    /* A pad above 3 writes the terminator alone. */
    memset(d, 0xEE, sizeof d);
    CHECK_EQ_INT(text_number_format(7, d, 3, 4u), 1);
    CHECK_EQ_INT(d[0], 0xEE);
    CHECK_EQ_INT(d[2], 0xEE);
    CHECK_EQ_INT(d[3], 0);

    /* 0x2F4D0: 0x2EFD4 then 0x2F198, the cursor restored. Width 3, pad 1:
     * "  7" leaves cols 4/5 empty and puts '7' at col 6. */
    actors_reset();
    DSD(DS_00105F34) = 0x12345678u;
    text_number_draw(4, 5, 7, 3, 1u, 0u);
    CHECK_EQ_INT((int)grid(5, 4), 0);
    CHECK_EQ_INT((int)grid(5, 5), 0);
    CHECK_EQ_INT((int)grid_sprite(5, 6), 0x3f4c);
    CHECK_EQ_INT((int)DSD(DS_00105F34), 0x12345678);
    /* Pad 3, width 2: "2" at the column itself. */
    actors_reset();
    text_number_draw(4, 5, 2, 2, 3u, 0u);
    CHECK_EQ_INT((int)grid_sprite(5, 4), 0x3f47);
    CHECK_EQ_INT((int)grid(5, 5), 0);

    /* 0x2F20C lays the string down a column (the vertical byte 1) and sets the
     * cursor to {row, col + count}. */
    actors_reset();
    DSD(DS_00105F34) = 0x7777u | (0x7777u << 16);
    text_vertical_set(5, 3, (const u8 *)"\x1b" "CB", 0u);
    CHECK_EQ_INT((int)grid_sprite(3, 5), 0x3f30);
    CHECK_EQ_INT((int)grid_sprite(4, 5), 0x3f58);
    CHECK_EQ_INT((int)grid_sprite(5, 5), 0x3f57);
    CHECK_EQ_INT((int)grid(3, 6), 0);
    CHECK_EQ_INT((int)DSW(DS_00105F34), 3);
    CHECK_EQ_INT((int)DSW(DS_00105F34 + 2), 8);    /* col 5 + 3 glyphs */
    /* col -1 centres by width: (0x2b - 3) >> 1 = 20. */
    actors_reset();
    text_vertical_set(-1, 2, (const u8 *)"ABC", 0u);
    CHECK_EQ_INT((int)grid_sprite(2, 20), 0x3f56);
    CHECK_EQ_INT((int)grid_sprite(4, 20), 0x3f58);
    CHECK_EQ_INT((int)DSW(DS_00105F34 + 2), 23);
    /* row -1 reuses the cursor: row = low word, col = high word. */
    actors_reset();
    DSW(DS_00105F34) = 6;
    DSW(DS_00105F34 + 2) = 9;
    text_vertical_set(0, -1, (const u8 *)"A", 0u);
    CHECK_EQ_INT((int)grid_sprite(6, 9), 0x3f56);
    CHECK_EQ_INT((int)DSW(DS_00105F34), 6);
    CHECK_EQ_INT((int)DSW(DS_00105F34 + 2), 10);

    /* 0x2F314 releases strlen(s) cells down the column, not across. */
    actors_reset();
    text_vertical_set(3, 9, (const u8 *)"ABC", 0u);
    text_cursor_set(4, 9, (const u8 *)"A", 0u);
    text_vertical_set(3, 12, (const u8 *)"A", 0u);
    u32 keep = grid(12, 3);
    CHECK(keep != 0, "the cell below the run");
    text_cells_release_vertical(3, 9, (const u8 *)"   ");
    CHECK_EQ_INT((int)grid(9, 3), 0);
    CHECK_EQ_INT((int)grid(10, 3), 0);
    CHECK_EQ_INT((int)grid(11, 3), 0);
    CHECK_EQ_INT((int)grid(12, 3), (int)keep);
    CHECK(grid(9, 4) != 0, "the neighbouring column is kept");
    /* It stops after the cell of a row above 0x1E: from row 0x1E a count of 3
     * releases row 0x1E and the dword past the grid's last row (row 0x1F), and
     * leaves the record planted at row 0x20 alone. */
    actors_reset();
    {
        u32 past = DS_00105F38 + 0x1fu * 0xacu + 3u * 4u;
        u32 past2 = past + 0xacu;
        u32 sv = DSD(past), sv2 = DSD(past2);
        u32 above = actor_spawn((const u32 *)(mem + 0x9AC30u), 0x4840u,
                                0xE0u, 0x1B00u, 0u);
        u32 last = actor_spawn((const u32 *)(mem + 0x9AC30u), 0x4840u,
                               0xE0u, 0x1B00u, 0u);
        u32 planted = actor_spawn((const u32 *)(mem + 0x9AC30u), 0x4840u,
                                  0xE0u, 0x1B00u, 0u);
        u32 planted2 = actor_spawn((const u32 *)(mem + 0x9AC30u), 0x4840u,
                                   0xE0u, 0x1B00u, 0u);
        CHECK(above != 0 && last != 0 && planted != 0 && planted2 != 0,
              "the planted records");
        DSD(DS_00105F38 + 0x1du * 0xacu + 3u * 4u) = above;
        DSD(DS_00105F38 + 0x1eu * 0xacu + 3u * 4u) = last;
        DSD(past) = planted;
        DSD(past2) = planted2;
        text_cells_release_vertical(3, 0x1e, (const u8 *)"abc");
        CHECK_EQ_INT((int)grid(0x1e, 3), 0);
        CHECK_EQ_INT((int)grid(0x1d, 3), (int)above);
        CHECK_EQ_INT((int)DSD(past), 0);
        CHECK_EQ_INT((int)DSD(past2), (int)planted2);
        DSD(past) = sv;
        DSD(past2) = sv2;
    }
    actors_reset();
}

/* ---- the menu cluster 0x2EB80..0x30000 (record §49-X) -------------------- */

#define MT_STR    0x3E30000u   /* a fabricated ENGLISH.TXT image (9 groups of 0x800) */
#define MT_HANDLE 0x3E2F000u   /* its 0x1E75C-style handle: +8 base, +0xC len, +0x15 flags */
#define MT_TABLE  0x3E2E000u   /* the menu tables */
#define MT_STRUCT 0x3E2C000u   /* the structure DS_0010740C points at */
#define MT_BUF_A  0x3E00000u   /* 64000-byte frame buffers 0x2EA78 draws/presents */
#define MT_BUF_B  0x3E10000u
#define MT_GROUP  0x800u
#define MT_FN0    0xF1A00u     /* fake code addresses for the registered callbacks */
#define MT_FN1    0xF1A10u
#define MT_FN2    0xF1A20u
#define MT_FN3    0xF1A30u
#define MT_FN_NONE 0xF1AFFu    /* never registered */

typedef struct { u32 n, arg, ret; } mt_cb_t;
static mt_cb_t mt_cbs[4];
static u32 mt_args0[16];
static u32 mt_hook_at, mt_hook_bits, mt_hook_ret_at, mt_hook_ret;

static u32 mt_cb_common(u32 which, u32 arg)
{
    mt_cbs[which].n++;
    mt_cbs[which].arg = arg;
    if (which == 0) {
        if (mt_cbs[0].n < 16u) mt_args0[mt_cbs[0].n] = arg;
        if (mt_cbs[0].n == mt_hook_at) tf_menu_press(mt_hook_bits);
        if (mt_cbs[0].n == mt_hook_ret_at) mt_cbs[0].ret = mt_hook_ret;
    }
    return mt_cbs[which].ret;
}
static u32 mt_cb0(u32 a) { return mt_cb_common(0, a); }
static u32 mt_cb1(u32 a) { return mt_cb_common(1, a); }
static u32 mt_cb2(u32 a) { return mt_cb_common(2, a); }
static u32 mt_cb3(u32 a) { return mt_cb_common(3, a); }

static void mt_cbs_reset(void)
{
    memset(mt_cbs, 0, sizeof mt_cbs);
    memset(mt_args0, 0, sizeof mt_args0);
    mt_hook_at = mt_hook_ret_at = 0xFFFFFFFFu;
    mt_hook_bits = mt_hook_ret = 0;
}

static void mt_strings_install(void)
{
    static const struct { u32 id; const char *s; } ent[] = {
        { 0x209u, "ESC TO EXIT" }, { 0x20Au, "MENU HELP" },
        { 0x210u, "TITLE" }, { 0x211u, "ITEM A" }, { 0x212u, "ITEM B" },
        { 0x213u, "X2" }, { 0x214u, "?HIDE" }, { 0x215u, "\nLOW" },
        { 0x216u, "\v3ROW" }, { 0x217u, "?" }, { 0x218u, "A_B_C" },
        { 0x219u, "ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZ" },
    };
    for (u32 g = 0; g < 9u; g++) {
        u32 p = MT_STR + g * MT_GROUP;
        DSD(p) = 0;
        DSD(p + 4u) = (g + 1u) * MT_GROUP;              /* 0x4752F: the next group */
        p += 8u;
        for (u32 j = 0; j < 0x40u; j++) {
            const char *s = "";
            if (g == 8u)
                for (u32 k = 0; k < sizeof ent / sizeof ent[0]; k++)
                    if (ent[k].id == 0x200u + j) s = ent[k].s;
            u32 len = (u32)strlen(s);
            DSB(p++) = (u8)len;                          /* 0x4754D */
            for (u32 i = 0; i < len; i++) DSB(p++) = (u8)((u8)s[i] ^ (u8)len);
        }
    }
    DSD(MT_HANDLE + 8u) = MT_STR;
    DSD(MT_HANDLE + 0xCu) = 0x10000u;
    DSB(MT_HANDLE + 0x15u) = 0;
    DSD(DS_001082DC) = MT_HANDLE;
}

static void mt_entry(u32 t, u32 i, u32 id, u32 id2, u32 cb, u32 yoff)
{
    u32 e = t + i * 0x10u;
    DSD(e) = id;
    DSD(e + 4u) = id2;
    DSD(e + 8u) = cb;
    DSD(e + 0xCu) = yoff;
}

/* The mode-0 font's sprite id for `ch` (table 0xBCD7C) and the class font's
 * (mode & 3 == 2: table 0xBD048 indexed through the class byte 0xBD390). */
static u32 mt_glyph(u8 ch) { return DSW(0xBCD7Cu + (u32)ch * 4u); }
static u32 mt_glyph2(u8 ch)
{
    s32 k = (s8)DSB(0xBD390u + ch);
    return k < 0 ? 0u : (u32)DSW(0xBD048u + (u32)k * 4u);
}
/* The palette handle (0x33754's table entry +0) of the glyph in a cell: the
 * font palette its mode selects (0x2F5A0). */
#define MT_PAL_1000 0x809984u
#define MT_PAL_2000 0x80998Cu
#define MT_PAL_4000 0x8099A4u
#define MT_PAL_F000 0x8099A4u
static u32 mt_pal(s32 row, s32 col)
{
    u32 r = grid(row, col);
    u32 e = r != 0 ? DSD(actor_pset(r) + 0x18u) : 0u;
    return e != 0 ? DSD(e) : 0u;
}

static void mt_menu_reset(void)
{
    mem_fill(DS_00107414, 0, 0x40u);
    mt_cbs_reset();
    tf_menu_press(0);
    DSD(DS_00105F30) = 0;
}

/* 0x2FE40, 0x2F940, 0x2FE84 (record §49-X.3..6). */
static void check_menu_draw(void)
{
    const u32 s_struct = DSD(DS_0010740C);
    u8 s_dbg[4];
    memcpy(s_dbg, mem + 0xBCD5Cu, 4u);

    /* 0x2FE40: index -1, past the end and a '?' item give 0. */
    mt_strings_install();
    const u32 T = MT_TABLE;
    mem_fill(T, 0, 0x100u);
    mt_entry(T, 0, 0x211u, 0, 0, 0);
    mt_entry(T, 1, 0x214u, 0, 0, 0);                 /* "?HIDE" */
    mt_entry(T, 2, 0x212u, 0, 0, 0);
    CHECK_EQ_INT((int)menu_entry_find(T, 0x10u, -1), 0);
    CHECK_EQ_INT((int)menu_entry_find(T, 0x10u, 0), (int)T);
    CHECK_EQ_INT((int)menu_entry_find(T, 0x10u, 1), 0);
    CHECK_EQ_INT((int)menu_entry_find(T, 0x10u, 2), (int)(T + 0x20u));
    CHECK_EQ_INT((int)menu_entry_find(T, 0x10u, 3), 0);
    CHECK_EQ_INT((int)menu_entry_find(T, 0x10u, 9), 0);
    CHECK_EQ_INT((int)menu_entry_find(T, 0x20u, 1), (int)(T + 0x20u));   /* the stride */

    /* 0x2F940: "OS:   " on `row`, "MAIN: " on `row + 1`, each followed by the
     * string 0x2F41C is handed. */
    actors_reset();
    DSD(DS_0010740C) = MT_STRUCT;
    DSD(MT_STRUCT + 0xCu) = MT_STRUCT + 0x40u;
    DSB(MT_STRUCT + 0x40u) = 'Y'; DSB(MT_STRUCT + 0x41u) = 0;
    DSB(0xBCD5Cu) = 'Z'; DSB(0xBCD5Du) = 'X'; DSB(0xBCD5Eu) = 0;
    menu_debug_lines(3u);
    CHECK_EQ_INT((int)grid_sprite(3, 4), (int)mt_glyph('O'));
    CHECK_EQ_INT((int)grid_sprite(3, 5), (int)mt_glyph('S'));
    CHECK_EQ_INT((int)grid_sprite(3, 10), (int)mt_glyph('Z'));
    CHECK_EQ_INT((int)grid_sprite(3, 11), (int)mt_glyph('X'));
    CHECK_EQ_INT((int)grid_sprite(4, 4), (int)mt_glyph('M'));
    CHECK_EQ_INT((int)grid_sprite(4, 10), (int)mt_glyph('Y'));
    CHECK_EQ_INT((int)grid(4, 11), 0);
    CHECK_EQ_INT((int)grid(2, 4), 0);
    CHECK_EQ_INT((int)grid(5, 4), 0);

    /* 0x2FE84: resets the actors, the input latch and the origin words, spawns
     * the backdrop row and centres the title (class font, mode | 2) with the
     * two instruction lines below. */
    mt_strings_install();
    mem_fill(T, 0, 0x100u);
    mt_entry(T, 0, 0x210u, 0, 0, 0);                 /* "TITLE" */
    actors_reset();
    text_cursor_set(5, 5, (const u8 *)"Z", 0u);      /* must not survive the reset */
    CHECK(grid(5, 5) != 0, "the pre-reset cell");
    DSB(DS_00104B15) = 1;
    DSW(DS_00107A3A) = 0x77; DSW(DS_00107A38) = 0x66;
    DSD(DS_00107A1C) = 0;
    menu_title_draw(T, 0x5000u, 0x1000u, 0u);
    CHECK_EQ_INT((int)grid(5, 5), 0);
    CHECK_EQ_INT((int)DSB(DS_00104B15), 0);
    CHECK_EQ_INT((int)DSW(DS_00107A3A), 0);
    CHECK_EQ_INT((int)DSW(DS_00107A38), 0);
    CHECK(DSD(DS_00107A1C) != 0, "the backdrop row is spawned");
    s32 col = (0x2b - text_width((const u8 *)"TITLE", 0x5002u)) >> 1;
    CHECK(col > 0 && mt_glyph2('T') != 0, "the title column");
    CHECK_EQ_INT((int)grid_sprite(0, col), (int)mt_glyph2('T'));
    CHECK_EQ_INT((int)grid(0, col - 1), 0);
    CHECK_EQ_INT((int)grid_sprite(0x1B, 16), (int)mt_glyph('E'));    /* "ESC TO EXIT" */
    CHECK_EQ_INT((int)grid_sprite(0x1C, 17), (int)mt_glyph('M'));    /* "MENU HELP" */
    CHECK_EQ_INT((int)grid(0x1B, 15), 0);
    /* Flags bit 2 drops the instruction lines; bit 0 adds the debug lines. */
    menu_title_draw(T, 0x5000u, 0x1000u, 4u);
    CHECK_EQ_INT((int)grid(0x1B, 16), 0);
    CHECK_EQ_INT((int)grid(0x1C, 17), 0);
    CHECK_EQ_INT((int)grid(0x1B, 4), 0);
    menu_title_draw(T, 0x5000u, 0x1000u, 5u);
    CHECK_EQ_INT((int)grid_sprite(0x1B, 4), (int)mt_glyph('O'));
    CHECK_EQ_INT((int)grid_sprite(0x1C, 4), (int)mt_glyph('M'));
    CHECK_EQ_INT((int)grid(0x1B, 16), 0);
    /* The second string follows after a space. */
    mt_entry(T, 0, 0x212u, 0x213u, 0, 0);            /* "ITEM B" + "X2" */
    menu_title_draw(T, 0x5000u, 0x1000u, 4u);
    col = (0x2b - text_width((const u8 *)"ITEM B X2", 0x5002u)) >> 1;
    s32 xcol = col + text_width((const u8 *)"ITEM B ", 0x5002u);
    CHECK_EQ_INT((int)grid_sprite(0, col), (int)mt_glyph2('I'));
    CHECK_EQ_INT((int)grid_sprite(0, xcol), (int)mt_glyph2('X'));
    CHECK_EQ_INT((int)grid(0, xcol - 1), 0);         /* the joining space */
    /* The '?', '\v' + digit and '\n' prefixes are stripped, in that order. */
    static const struct { u32 id; const char *shown; } pre[] = {
        { 0x214u, "HIDE" }, { 0x216u, "ROW" }, { 0x215u, "LOW" } };
    for (u32 i = 0; i < 3u; i++) {
        mt_entry(T, 0, pre[i].id, 0, 0, 0);
        menu_title_draw(T, 0x5000u, 0x1000u, 4u);
        col = (0x2b - text_width((const u8 *)pre[i].shown, 0x5002u)) >> 1;
        CHECK_EQ_INT((int)grid_sprite(0, col), (int)mt_glyph2((u8)pre[i].shown[0]));
    }
    /* Past 0x28 columns in the class font the '_' turn into spaces and the
     * plain font is used, centred on the 42 characters copied. */
    CHECK(mt_glyph('_') != 0, "the plain font has an underscore");
    {
        u8 want[0x30];
        snprintf((char *)want, sizeof want, "A_B_C %s",
                 "ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZ");
        want[0x2A] = 0;
        CHECK(text_width(want, 0x5002u) > 0x28, "the fixture is wide enough");
        mt_entry(T, 0, 0x218u, 0x219u, 0, 0);
        menu_title_draw(T, 0x5000u, 0x1000u, 4u);
        CHECK_EQ_INT((int)grid_sprite(0, 0), (int)mt_glyph('A'));
        CHECK_EQ_INT((int)grid(0, 1), 0);
        CHECK_EQ_INT((int)grid_sprite(0, 2), (int)mt_glyph('B'));
        CHECK_EQ_INT((int)grid(0, 3), 0);
        CHECK_EQ_INT((int)grid_sprite(0, 4), (int)mt_glyph('C'));
        CHECK_EQ_INT((int)grid(0, 5), 0);
        CHECK_EQ_INT((int)grid_sprite(0, 6), (int)mt_glyph('A'));
        /* Narrow enough: the class font keeps the string as it is. */
        mt_entry(T, 0, 0x218u, 0, 0, 0);
        menu_title_draw(T, 0x5000u, 0x1000u, 4u);
        col = (0x2b - text_width((const u8 *)"A_B_C", 0x5002u)) >> 1;
        CHECK_EQ_INT((int)grid_sprite(0, col), (int)mt_glyph2('A'));
    }

    actors_reset();
    DSD(DS_0010740C) = s_struct;
    memcpy(mem + 0xBCD5Cu, s_dbg, 4u);
}

/* 0x2FFC4 (record §49-X.7). */
/* The idle timeout's soft restart (record named-gaps-b §B.3) lands here. */
static jmp_buf ms_jb;

static void check_menu_step(void)
{
    const u32 T = MT_TABLE + 0x200u;
    const u32 s_lay = DSD(DS_00101514), s_ticks = DSD(DS_00101500),
              s_stamp = DSD(DS_00105F2C);
    mem_fill(T, 0, 0x100u);
    mt_entry(T, 0, 0x210u, 0, MT_FN0, 0);            /* the title */
    mt_entry(T, 1, 0x211u, 0, MT_FN1, 0);            /* "ITEM A" */
    mt_entry(T, 2, 0x212u, 0x213u, MT_FN2, 1);       /* "ITEM B" + "X2", one row down */
    mt_entry(T, 3, 0x214u, 0, MT_FN3, 0);            /* "?HIDE" */
    mt_entry(T, 4, 0x215u, 0, MT_FN3, 2);            /* "\nLOW", two rows down */
    mt_strings_install();
    fn_register(MT_FN0, (void (*)(void))mt_cb0);
    fn_register(MT_FN1, (void (*)(void))mt_cb1);
    fn_register(MT_FN2, (void (*)(void))mt_cb2);
    fn_register(MT_FN3, (void (*)(void))mt_cb3);
    mt_menu_reset();
    actors_reset();
    DSD(DS_00101514) = MT_LAYOUT;
    DSW(MT_LAYOUT + 0x2D4u) = 1; DSW(MT_LAYOUT + 0x2D6u) = 1;
    DSB(MT_LAYOUT + 0x2DEu) = 0x48; DSB(MT_LAYOUT + 0x2DFu) = 0x50;
    DSB(MT_LAYOUT + 0x2E0u) = 0x4B; DSB(MT_LAYOUT + 0x2E1u) = 0x4D;
    DSB(MT_LAYOUT + 0x2E6u) = 0x11; DSB(MT_LAYOUT + 0x2E7u) = 0x12;
    DSB(MT_LAYOUT + 0x2E8u) = 0x13; DSB(MT_LAYOUT + 0x2E9u) = 0x14;
    DSD(DS_00101500) = 1000;
    DSD(DS_00105F2C) = 5;
    const u32 pal_hi = MT_PAL_2000, pal_lo = MT_PAL_F000;
    const u32 A = T + 0x10u, B = T + 0x20u, C = T + 0x40u;

    /* First call: initialises, draws everything, calls the table callback with 0
     * (the redraw) and again with the selected entry. */
    CHECK_EQ_INT((int)menu_step(T, 0x10u, 4u), 0);
    CHECK_EQ_INT((int)DSB(DS_00107414), 1);
    CHECK_EQ_INT((int)DSD(DS_00107418), 4);
    CHECK_EQ_INT((int)DSD(DS_00105F2C), 1000);
    CHECK_EQ_INT((int)DSD(DS_00101500), 1002);       /* 0x2FFF1 0x2EA74: two tick waits (§K5) */
    CHECK_EQ_INT((int)DSD(DS_0010741C), (int)A);
    CHECK_EQ_INT((int)DSD(DS_0010744C), (int)MT_FN0);
    CHECK_EQ_INT((int)DSD(DS_00107424), 4);          /* the hidden item counts */
    CHECK_EQ_INT((int)DSD(DS_00107428), 0);
    CHECK_EQ_INT((int)DSD(DS_00107430), (int)A);
    CHECK_EQ_INT((int)DSD(DS_0010743C), 0);
    CHECK_EQ_INT((int)DSD(DS_00107444), 12);         /* rows 5, 7, 11 drawn, then + 1 */
    CHECK_EQ_INT((int)DSD(DS_00107440), 5);
    CHECK_EQ_INT((int)mt_cbs[0].n, 2);
    CHECK_EQ_INT((int)mt_args0[1], 0);
    CHECK_EQ_INT((int)mt_args0[2], (int)A);
    CHECK_EQ_INT((int)grid_sprite(5, 4), (int)mt_glyph('I'));
    CHECK_EQ_INT((int)grid_sprite(5, 9), (int)mt_glyph('A'));
    CHECK_EQ_INT((int)grid(5, 8), 0);
    CHECK_EQ_INT((int)grid(6, 4), 0);
    CHECK_EQ_INT((int)grid_sprite(7, 4), (int)mt_glyph('I'));
    CHECK_EQ_INT((int)grid_sprite(7, 9), (int)mt_glyph('B'));
    CHECK_EQ_INT((int)grid_sprite(7, 11), (int)mt_glyph('X'));   /* strlen("ITEM B") + 5 */
    CHECK_EQ_INT((int)grid_sprite(7, 12), (int)mt_glyph('2'));
    CHECK_EQ_INT((int)grid(8, 4), 0);
    CHECK_EQ_INT((int)grid(9, 4), 0);
    CHECK_EQ_INT((int)grid(10, 4), 0);
    CHECK_EQ_INT((int)grid_sprite(11, 4), (int)mt_glyph('L'));
    CHECK_EQ_INT((int)grid(0, 4), 0);
    CHECK(grid(0x1B, 16) == 0, "flags bit 2: no instruction lines");
    CHECK_EQ_INT((int)mt_pal(5, 4), (int)pal_hi);    /* the selected item */
    CHECK_EQ_INT((int)mt_pal(7, 4), (int)pal_lo);
    CHECK_EQ_INT((int)mt_pal(7, 11), (int)pal_lo);
    CHECK_EQ_INT((int)mt_pal(11, 4), (int)pal_lo);
    /* No input: nothing moves, the callback runs with the selection, and 0x4B0
     * idle ticks are not a timeout. */
    DSD(DS_00101500) = 1000 + 0x4B0;
    CHECK_EQ_INT((int)menu_step(T, 0x10u, 0u), 0);   /* flags of later calls are ignored */
    CHECK_EQ_INT((int)DSD(DS_00107418), 4);
    CHECK_EQ_INT((int)DSB(DS_00107414), 1);
    CHECK_EQ_INT((int)mt_cbs[0].n, 3);
    CHECK_EQ_INT((int)mt_args0[3], (int)A);
    CHECK_EQ_INT((int)DSD(DS_0010743C), 0);

    /* Down: the next visible item, a redraw asked for, the clock stamped. */
    DSD(DS_00101500) = 2000;
    tf_menu_press(0x40004000u);
    CHECK_EQ_INT((int)menu_step(T, 0x10u, 4u), 0);
    CHECK_EQ_INT((int)DSD(DS_0010742C), 1);
    CHECK_EQ_INT((int)DSD(DS_00107434), (int)B);
    CHECK_EQ_INT((int)DSD(DS_0010743C), 1);
    CHECK_EQ_INT((int)DSD(DS_00105F2C), 2000);
    CHECK_EQ_INT((int)DSD(DS_00107430), (int)A);     /* not yet redrawn */
    CHECK_EQ_INT((int)mt_pal(5, 4), (int)pal_hi);
    /* The redraw unhighlights the old item and highlights the new one, both
     * strings. */
    CHECK_EQ_INT((int)menu_step(T, 0x10u, 4u), 0);
    CHECK_EQ_INT((int)DSD(DS_00107430), (int)B);
    CHECK_EQ_INT((int)DSD(DS_00107428), 1);
    CHECK_EQ_INT((int)DSD(DS_0010743C), 0);
    CHECK_EQ_INT((int)mt_pal(5, 4), (int)pal_lo);
    CHECK_EQ_INT((int)mt_pal(7, 4), (int)pal_hi);
    CHECK_EQ_INT((int)mt_pal(7, 11), (int)pal_hi);
    CHECK_EQ_INT((int)mt_pal(11, 4), (int)pal_lo);
    CHECK_EQ_INT((int)mt_cbs[0].n, 6);               /* +2 (redraw 0, sel) after the +1 above */
    CHECK_EQ_INT((int)mt_args0[5], 0);
    CHECK_EQ_INT((int)mt_args0[6], (int)B);
    /* Down again skips the hidden item; then wraps to the first. */
    tf_menu_press(0x40004000u);
    (void)menu_step(T, 0x10u, 4u);
    CHECK_EQ_INT((int)DSD(DS_0010742C), 3);
    CHECK_EQ_INT((int)DSD(DS_00107434), (int)C);
    CHECK_EQ_INT((int)DSD(DS_00107448), 1);          /* the wrap fuse untouched */
    (void)menu_step(T, 0x10u, 4u);
    CHECK_EQ_INT((int)DSD(DS_00107430), (int)C);
    tf_menu_press(0x40004000u);
    (void)menu_step(T, 0x10u, 4u);
    CHECK_EQ_INT((int)DSD(DS_0010742C), 0);
    CHECK_EQ_INT((int)DSD(DS_00107434), (int)A);
    CHECK_EQ_INT((int)DSD(DS_00107448), 0);          /* one wrap spent */
    /* Up wraps to the last visible item, and beats Down. */
    (void)menu_step(T, 0x10u, 4u);
    tf_menu_press(0x80008000u);
    (void)menu_step(T, 0x10u, 4u);
    CHECK_EQ_INT((int)DSD(DS_0010742C), 3);
    (void)menu_step(T, 0x10u, 4u);
    tf_menu_press(0xC000C000u);
    (void)menu_step(T, 0x10u, 4u);
    CHECK_EQ_INT((int)DSD(DS_0010742C), 1);          /* 3 -> 2 (hidden) -> 1 */
    CHECK_EQ_INT((int)DSD(DS_00107434), (int)B);
    (void)menu_step(T, 0x10u, 4u);                   /* redraw: the selection is B */
    CHECK_EQ_INT((int)DSD(DS_00107430), (int)B);

    /* The keyboard reaches the same code: Down through the layout's binding, and
     * a disabled binding is ignored. */
    tf_menu_press(0);
    DSD(DS_00105F30) = 0x50;
    DSD(DS_00101500) = 3000;
    (void)menu_step(T, 0x10u, 4u);
    CHECK_EQ_INT((int)DSD(DS_0010742C), 3);
    CHECK_EQ_INT((int)DSD(DS_00105F30), 0);          /* 0x2EEC8 consumed the key */
    CHECK_EQ_INT((int)DSD(DS_00105F2C), 3000);
    (void)menu_step(T, 0x10u, 4u);
    DSW(MT_LAYOUT + 0x2D4u) = 0;
    DSD(DS_00105F30) = 0x48;
    DSD(DS_00101500) = 3100;
    (void)menu_step(T, 0x10u, 4u);
    CHECK_EQ_INT((int)DSD(DS_0010742C), 3);          /* unmoved */
    CHECK_EQ_INT((int)DSD(DS_00105F2C), 3000);       /* and not stamped */
    DSW(MT_LAYOUT + 0x2D4u) = 1;
    CHECK_EQ_INT((int)DSB(DS_00107414), 1);

    /* Idle past 0x4B0 ticks clears the active flag (0x2EBA8) and soft-restarts
     * (0x2EBB3, record named-gaps-b §B.3). */
    DSD(DS_00101500) = 3000 + 0x4B1;
    {
        jmp_buf *const prev = game_restart_arm(&ms_jb);
        volatile int landed = 0;
        if (setjmp(ms_jb) == 0) (void)menu_step(T, 0x10u, 4u); else landed = 1;
        (void)game_restart_arm(prev);
        CHECK_EQ_INT(landed, 1);
    }
    CHECK_EQ_INT((int)DSB(DS_00107414), 0);

    /* Enter runs the item's callback: -5 leaves with a redraw asked for, -10
     * leaves, any other result returns 0; the title is redrawn for the item. */
    DSD(DS_00101500) = 4000;
    DSD(DS_00105F2C) = 4000;
    (void)menu_step(T, 0x10u, 4u);                   /* re-initialises */
    CHECK_EQ_INT((int)DSB(DS_00107414), 1);
    CHECK_EQ_INT((int)DSD(DS_0010742C), 0);
    tf_menu_press(0x40004000u);
    (void)menu_step(T, 0x10u, 4u);
    (void)menu_step(T, 0x10u, 4u);                   /* selection B */
    CHECK_EQ_INT((int)DSD(DS_00107430), (int)B);
    CHECK(grid(5, 4) != 0, "the items are on screen before Enter");
    mt_cbs[2].ret = (u32)-5;
    tf_menu_press(0x1000000u);
    CHECK_EQ_INT((int)menu_step(T, 0x10u, 4u), -5);
    CHECK_EQ_INT((int)mt_cbs[2].n, 1);
    CHECK_EQ_INT((int)mt_cbs[2].arg, (int)B);
    CHECK_EQ_INT((int)DSB(DS_00107414), 0);
    CHECK_EQ_INT((int)DSD(DS_0010743C), 1);
    CHECK_EQ_INT((int)DSD(DS_00107428), -2);
    CHECK_EQ_INT((int)DSD(DS_00107448), -5);
    CHECK_EQ_INT((int)grid(5, 4), 0);                /* 0x2FE84 reset the screen */
    {
        s32 col = (0x2b - text_width((const u8 *)"ITEM B X2", 0x5002u)) >> 1;
        CHECK_EQ_INT((int)grid_sprite(0, col), (int)mt_glyph2('I'));
    }
    /* -10 leaves without asking for a redraw. */
    (void)menu_step(T, 0x10u, 4u);
    tf_menu_press(0x40004000u);
    (void)menu_step(T, 0x10u, 4u);
    (void)menu_step(T, 0x10u, 4u);
    mt_cbs[2].ret = (u32)-10;
    tf_menu_press(0x1000000u);
    CHECK_EQ_INT((int)menu_step(T, 0x10u, 4u), -10);
    CHECK_EQ_INT((int)DSB(DS_00107414), 0);
    CHECK_EQ_INT((int)DSD(DS_0010743C), 0);
    CHECK_EQ_INT((int)DSD(DS_00107448), -10);
    /* Another result is not a way out; neither is a callback nobody registered. */
    (void)menu_step(T, 0x10u, 4u);
    tf_menu_press(0x40004000u);
    (void)menu_step(T, 0x10u, 4u);
    (void)menu_step(T, 0x10u, 4u);
    mt_cbs[2].ret = 7;
    tf_menu_press(0x1000000u);
    CHECK_EQ_INT((int)menu_step(T, 0x10u, 4u), 0);
    CHECK_EQ_INT((int)DSD(DS_00107448), 7);
    CHECK_EQ_INT((int)DSB(DS_00107414), 0);
    DSD(B + 8u) = MT_FN_NONE;
    (void)menu_step(T, 0x10u, 4u);
    tf_menu_press(0x40004000u);
    (void)menu_step(T, 0x10u, 4u);
    (void)menu_step(T, 0x10u, 4u);
    tf_menu_press(0x1000000u);
    CHECK_EQ_INT((int)menu_step(T, 0x10u, 4u), 0);
    CHECK_EQ_INT((int)DSD(DS_00107448), 0);
    DSD(B + 8u) = MT_FN2;

    /* Esc: with flags bit 2 it leaves with -1 and a redraw asked for; without,
     * -5, or 0 when the selection index equals the item count. */
    (void)menu_step(T, 0x10u, 4u);
    tf_menu_press(0x2000000u);
    CHECK_EQ_INT((int)menu_step(T, 0x10u, 4u), -1);
    CHECK_EQ_INT((int)DSB(DS_00107414), 0);
    CHECK_EQ_INT((int)DSD(DS_0010743C), 1);
    (void)menu_step(T, 0x10u, 0u);
    CHECK_EQ_INT((int)DSD(DS_00107418), 0);
    CHECK_EQ_INT((int)grid_sprite(0x1B, 16), (int)mt_glyph('E'));   /* flags 0: the help lines */
    tf_menu_press(0x2000000u);
    CHECK_EQ_INT((int)menu_step(T, 0x10u, 0u), -5);
    CHECK_EQ_INT((int)DSB(DS_00107414), 0);
    (void)menu_step(T, 0x10u, 0u);
    DSD(DS_0010742C) = DSD(DS_00107424);
    tf_menu_press(0x2000000u);
    CHECK_EQ_INT((int)menu_step(T, 0x10u, 0u), 0);
    CHECK_EQ_INT((int)DSB(DS_00107414), 0);
    /* The Esc key itself is dead: 0x2EEC8 has consumed it before 0x2EB80 looks. */
    (void)menu_step(T, 0x10u, 4u);
    DSD(DS_00105F30) = 0x1B;
    CHECK_EQ_INT((int)menu_step(T, 0x10u, 4u), -1);   /* via the 0x2EBF0 Esc bit, flags 4 */
    CHECK_EQ_INT((int)DSD(DS_00105F30), 0);

    /* A nonzero table callback result is returned before any key is handled. */
    (void)menu_step(T, 0x10u, 4u);
    mt_cbs[0].ret = 9;
    tf_menu_press(0x2000000u);
    CHECK_EQ_INT((int)menu_step(T, 0x10u, 4u), 9);
    CHECK_EQ_INT((int)DSB(DS_00107414), 1);
    CHECK_EQ_INT((int)DSD(DS_00107448), 9);
    mt_cbs[0].ret = 0;
    tf_menu_press(0);

    /* Flags bit 0 draws the 0x305FC widget after a step that did not leave. */
    DSB(DS_00107414) = 0;
    actors_reset();
    memcpy(mem + DS_00107454, "12345678", 8u);
    DSB(DS_00107450 + 2u) = 8; DSB(DS_00107453) = 0;
    (void)menu_step(T, 0x10u, 5u);
    CHECK_EQ_INT((int)grid_sprite(2, 0x11), (int)mt_glyph('1'));
    CHECK_EQ_INT((int)grid_sprite(2, 0x18), (int)mt_glyph('8'));
    DSB(DS_00107414) = 0;
    actors_reset();
    (void)menu_step(T, 0x10u, 4u);
    CHECK_EQ_INT((int)grid(2, 0x11), 0);
    mem_fill(DS_00107450, 0, 0x20u);

    /* A leading "\v3" moves the first row only until the first redraw resets it
     * to 5 (0x300F9), and a hidden first item with nothing after it drops the
     * title. */
    mem_fill(T, 0, 0x100u);
    mt_entry(T, 0, 0x210u, 0, 0, 0);
    mt_entry(T, 1, 0x216u, 0, 0, 0);                 /* "\v3ROW" */
    DSB(DS_00107414) = 0;
    actors_reset();
    (void)menu_step(T, 0x10u, 4u);
    CHECK_EQ_INT((int)DSD(DS_00107440), 5);
    CHECK_EQ_INT((int)grid_sprite(5, 4), (int)mt_glyph(0x0B));   /* the redraw keeps the "\v3" */
    CHECK_EQ_INT((int)grid_sprite(5, 6), (int)mt_glyph('R'));
    CHECK_EQ_INT((int)grid(8, 4), 0);
    {
        s32 col = (0x2b - text_width((const u8 *)"TITLE", 0x5002u)) >> 1;
        CHECK_EQ_INT((int)grid_sprite(0, col), (int)mt_glyph2('T'));
    }
    mem_fill(T, 0, 0x100u);
    mt_entry(T, 0, 0x210u, 0, 0, 0);
    mt_entry(T, 1, 0x217u, 0, 0, 0);                 /* "?" */
    mt_entry(T, 2, 0x217u, 0, 0, 0);
    DSB(DS_00107414) = 0;
    actors_reset();
    (void)menu_step(T, 0x10u, 4u);
    {
        s32 col = (0x2b - text_width((const u8 *)"TITLE", 0x5002u)) >> 1;
        CHECK_EQ_INT((int)grid(0, col), 0);          /* no title: entry 0 unused */
    }
    /* A menu with nothing visible: Down finds nothing after two wraps and 0x2EA68
     * is reached (the port returns 0). */
    DSD(DS_00107448) = 0x12345678u;
    tf_menu_press(0x40004000u);
    CHECK_EQ_INT((int)menu_step(T, 0x10u, 4u), 0);
    CHECK_EQ_INT((int)DSD(DS_00107448), -1);
    tf_menu_press(0x80008000u);
    DSD(DS_00107448) = 0x12345678u;
    CHECK_EQ_INT((int)menu_step(T, 0x10u, 4u), 0);
    CHECK_EQ_INT((int)DSD(DS_00107448), -1);

    mt_menu_reset();
    mem_fill(DS_00107414, 0, 0x40u);
    actors_reset();
    DSD(DS_00101514) = s_lay; DSD(DS_00101500) = s_ticks; DSD(DS_00105F2C) = s_stamp;
    tf_menu_press(0);
}

/* 0x2FA40 (record §49-X.7). The callbacks are the script: the input state
 * changes inside the table callback, which the loop calls once per poll. */
static void check_menu_run(void)
{
    const u32 T = MT_TABLE + 0x400u;
    const u32 s_lay = DSD(DS_00101514), s_ticks = DSD(DS_00101500);
    mem_fill(T, 0, 0x100u);
    mt_entry(T, 0, 0x210u, 0, MT_FN0, 0);
    mt_entry(T, 1, 0x211u, 0, MT_FN1, 0);
    mt_entry(T, 2, 0x212u, 0x213u, MT_FN2, 1);
    mt_entry(T, 3, 0x214u, 0, MT_FN3, 0);
    mt_entry(T, 4, 0x215u, 0, MT_FN3, 2);
    mt_strings_install();
    fn_register(MT_FN0, (void (*)(void))mt_cb0);
    fn_register(MT_FN1, (void (*)(void))mt_cb1);
    fn_register(MT_FN2, (void (*)(void))mt_cb2);
    fn_register(MT_FN3, (void (*)(void))mt_cb3);
    mt_menu_reset();
    actors_reset();
    DSD(DS_00101514) = MT_LAYOUT;
    DSD(DS_00101500) = 100;
    const u32 pal_hi = MT_PAL_2000, pal_lo = MT_PAL_F000;
    const u32 A = T + 0x10u, B = T + 0x20u, C = T + 0x40u;

    /* Down moves to item B, whose entry the next callback call receives; the
     * callback's nonzero result is the return value. */
    mt_cbs_reset();
    tf_menu_press(0x40004000u);
    mt_hook_ret_at = 3; mt_hook_ret = 0x77;
    CHECK_EQ_INT((int)menu_run(T, 0x10u, 4u), 0x77);
    /* §K5: 0x2EA74 at entry (0x2FA61) and after the one completed poll pass
     * (0x2FE2F), two ticks each. */
    CHECK_EQ_INT((int)DSD(DS_00101500), 104);
    CHECK_EQ_INT((int)mt_cbs[0].n, 3);
    CHECK_EQ_INT((int)mt_args0[1], 0);               /* the header redraw */
    CHECK_EQ_INT((int)mt_args0[2], (int)A);
    CHECK_EQ_INT((int)mt_args0[3], (int)B);
    CHECK_EQ_INT((int)grid_sprite(5, 4), (int)mt_glyph('I'));
    CHECK_EQ_INT((int)grid_sprite(7, 11), (int)mt_glyph('X'));
    CHECK_EQ_INT((int)grid_sprite(11, 4), (int)mt_glyph('L'));
    CHECK_EQ_INT((int)grid(9, 4), 0);
    CHECK_EQ_INT((int)mt_pal(5, 4), (int)pal_lo);
    CHECK_EQ_INT((int)mt_pal(7, 4), (int)pal_hi);
    CHECK_EQ_INT((int)mt_pal(7, 11), (int)pal_hi);
    CHECK_EQ_INT((int)mt_pal(11, 4), (int)pal_lo);
    CHECK_EQ_INT((int)grid(0x1B, 16), 0);            /* flags bit 2: no help lines */

    /* Esc without flags bit 2: -1 (the index never equals the count). */
    mt_cbs_reset();
    tf_menu_press(0x2000000u);
    DSD(DS_00101500) = 200;
    CHECK_EQ_INT((int)menu_run(T, 0x10u, 0u), -1);
    CHECK_EQ_INT((int)mt_cbs[0].n, 2);
    CHECK_EQ_INT((int)DSD(DS_00101500), 202);        /* only the entry 0x2EA74 */
    /* Esc with flags bit 2 acts as Down. */
    mt_cbs_reset();
    tf_menu_press(0x2000000u);
    mt_hook_ret_at = 3; mt_hook_ret = 0x55;
    CHECK_EQ_INT((int)menu_run(T, 0x10u, 4u), 0x55);
    CHECK_EQ_INT((int)mt_args0[2], (int)A);
    CHECK_EQ_INT((int)mt_args0[3], (int)B);
    /* Down twice: past the hidden item to C. */
    mt_cbs_reset();
    tf_menu_press(0x40004000u);
    mt_hook_at = 2; mt_hook_bits = 0x40004000u;
    mt_hook_ret_at = 4; mt_hook_ret = 0x56;
    CHECK_EQ_INT((int)menu_run(T, 0x10u, 4u), 0x56);
    CHECK_EQ_INT((int)mt_args0[3], (int)B);
    CHECK_EQ_INT((int)mt_args0[4], (int)C);
    /* Up from the first item wraps to the last visible one. */
    mt_cbs_reset();
    tf_menu_press(0x80008000u);
    mt_hook_ret_at = 3; mt_hook_ret = 0x57;
    CHECK_EQ_INT((int)menu_run(T, 0x10u, 4u), 0x57);
    CHECK_EQ_INT((int)mt_args0[3], (int)C);
    /* Enter: the item's callback runs, then the header is drawn again (the
     * table callback is called with 0 once more) before it goes on. */
    mt_cbs_reset();
    tf_menu_press(0x1000000u);
    mt_hook_ret_at = 5; mt_hook_ret = 0x66;
    CHECK_EQ_INT((int)menu_run(T, 0x10u, 4u), 0x66);
    CHECK_EQ_INT((int)mt_cbs[1].n, 1);
    CHECK_EQ_INT((int)mt_cbs[1].arg, (int)A);
    CHECK_EQ_INT((int)mt_args0[3], 0);
    CHECK_EQ_INT((int)mt_args0[4], (int)A);
    CHECK_EQ_INT((int)mt_args0[5], (int)A);
    {
        s32 col = (0x2b - text_width((const u8 *)"TITLE", 0x5002u)) >> 1;
        CHECK_EQ_INT((int)grid_sprite(0, col), (int)mt_glyph2('T'));   /* the header is back */
    }
    CHECK_EQ_INT((int)grid_sprite(5, 4), (int)mt_glyph('I'));
    /* The debug flag draws the widget on every poll. */
    memcpy(mem + DS_00107454, "12345678", 8u);
    DSB(DS_00107450 + 2u) = 8; DSB(DS_00107453) = 0;
    mt_cbs_reset();
    tf_menu_press(0);
    mt_hook_ret_at = 3; mt_hook_ret = 0x58;
    CHECK_EQ_INT((int)menu_run(T, 0x10u, 5u), 0x58);
    CHECK_EQ_INT((int)grid_sprite(2, 0x11), (int)mt_glyph('1'));
    mem_fill(DS_00107450, 0, 0x20u);

    /* "\v3" puts the first item on row 8 (no reset here, unlike 0x2FFC4). */
    mem_fill(T, 0, 0x100u);
    mt_entry(T, 0, 0x210u, 0, MT_FN0, 0);
    mt_entry(T, 1, 0x216u, 0, MT_FN1, 0);
    mt_cbs_reset();
    mt_hook_ret_at = 2; mt_hook_ret = 0x59;
    CHECK_EQ_INT((int)menu_run(T, 0x10u, 4u), 0x59);
    CHECK_EQ_INT((int)grid_sprite(8, 4), (int)mt_glyph(0x0B));
    CHECK_EQ_INT((int)grid_sprite(8, 6), (int)mt_glyph('R'));
    CHECK_EQ_INT((int)grid(5, 4), 0);
    /* A hidden first item with no text after the '?' drops the title; a menu
     * with nothing visible reaches 0x2EA68 on the second wrap and the port
     * returns 0. */
    mem_fill(T, 0, 0x100u);
    mt_entry(T, 0, 0x210u, 0, 0, 0);
    mt_entry(T, 1, 0x217u, 0, 0, 0);
    mt_entry(T, 2, 0x217u, 0, 0, 0);
    mt_cbs_reset();
    tf_menu_press(0x40004000u);
    CHECK_EQ_INT((int)menu_run(T, 0x10u, 4u), 0);
    {
        s32 col = (0x2b - text_width((const u8 *)"TITLE", 0x5002u)) >> 1;
        CHECK_EQ_INT((int)grid(0, col), 0);
    }
    tf_menu_press(0x80008000u);
    CHECK_EQ_INT((int)menu_run(T, 0x10u, 4u), 0);

    mt_menu_reset();
    actors_reset();
    DSD(DS_00101514) = s_lay; DSD(DS_00101500) = s_ticks;
    tf_menu_press(0);
}

static void check_menu(void)
{
    static u8 spal[0x180];
    const u32 s_str = DSD(DS_001082DC);
    /* Earlier checks fill the 24-entry palette table (0x33754); the menu checks
     * compare palette entries, so they start from an empty one. */
    memcpy(spal, mem + DS_00107618, sizeof spal);
    mem_fill(DS_00107618, 0, sizeof spal);
    const u32 s_lvl = DSD(DS_000E1C34), s_lat = DSD(DS_000E1C38);
    /* Record §K5: the menus now run 0x2EA78 (0x2EA74), which presents the back
     * buffer, swaps the pair and spins on the ISR model (gate byte not 1). */
    const u32 s_raw = DSD(DS_000E1C30), s_a0 = DSD(DS_000E87A0),
              s_a4 = DSD(DS_000E87A4), s_08 = DSD(DS_00101508);
    const u16 s_word = DSW(0x000EF6DEu);
    const u8 s_gate = DSB(DS_00104B22);
    DSD(DS_000E87A0) = MT_BUF_A;
    DSD(DS_000E87A4) = MT_BUF_B;
    DSB(DS_00104B22) = 0u;
    check_menu_draw();
    check_menu_step();
    check_menu_run();
    menu_fatal_error(0x80B54u);
    memcpy(mem + DS_00107618, spal, sizeof spal);
    DSD(DS_001082DC) = s_str;
    DSD(DS_000E1C34) = s_lvl;
    DSD(DS_000E1C38) = s_lat;
    DSD(DS_000E1C30) = s_raw;
    DSD(DS_000E87A0) = s_a0;
    DSD(DS_000E87A4) = s_a4;
    DSD(DS_00101508) = s_08;
    DSW(0x000EF6DEu) = s_word;
    DSB(DS_00104B22) = s_gate;
    actors_reset();
}

int test_text(void)
{
    int before = g_failures;
    if (res_count() != 69)
        CHECK(res_load_index("data/game/C", "data/game/C/INDEX") == 69,
              "index loaded");
    CHECK(actors_init() == 1, "actors_init validates the pools");

    /* Mode 3 maps by class: 'A' -> class 10 (table entry sprite 0x3FDD, width
     * 16), 'I' -> class 18 (sprite 0x3FE5, width 8). 0x2F5A0 advances the
     * running column by the glyph width, so "AI" lands at columns 0 and 2; the
     * cursor is advanced by 0x2F830's return (the glyph count), not the pixel
     * width, so its high word is 0 + 2. */
    actors_reset();
    DSD(DS_00105F34) = 0;
    text_cursor_set(0, 0, (const u8 *)"AI", 3);
    u32 ga = grid(0, 0), gi = grid(0, 2);
    CHECK(ga != 0, "A glyph spawned at col 0");
    CHECK(gi != 0, "I glyph spawned at col 2");
    CHECK_EQ_INT((int)grid(0, 1), 0);              /* col 1 skipped: width 16 */
    if (ga != 0) {
        CHECK_EQ_INT((int)(DSW(actor_pset(ga)) & 0x7fffu), 0x3fdd);
        CHECK_EQ_INT((int)DSD(ga + 0x18), 0);      /* x = col 0 * 0x200 */
        CHECK_EQ_INT((int)DSD(ga + 0x1c), 0);      /* y = row 0 * 0x200 */
        CHECK_EQ_INT((int)DSW(ga + 0x40), 0x2000); /* extent 0x80 << 6 */
    }
    if (gi != 0) {
        CHECK_EQ_INT((int)(DSW(actor_pset(gi)) & 0x7fffu), 0x3fe5);
        CHECK_EQ_INT((int)DSD(gi + 0x18), 0x400);  /* x = col 2 * 0x200 */
        CHECK_EQ_INT((int)DSD(gi + 0x1c), 0);
    }
    CHECK_EQ_INT((int)DSW(DS_00105F34), 0);
    CHECK_EQ_INT((int)DSW(DS_00105F34 + 2), 2);

    /* Mode 0 indexes the char table at DS 0x3CD7C (all width 8, advance 1). A
     * space is counted and advances the column but spawns no actor. */
    actors_reset();
    DSD(DS_00105F34) = 0;
    text_cursor_set(0, 0, (const u8 *)"A B", 0);
    u32 g0 = grid(0, 0), g1 = grid(0, 1), g2 = grid(0, 2);
    CHECK(g0 != 0 && g2 != 0, "A and B glyphs");
    CHECK_EQ_INT((int)g1, 0);                      /* space: no actor */
    if (g0 != 0)
        CHECK_EQ_INT((int)(DSW(actor_pset(g0)) & 0x7fffu), 0x3f56);
    if (g2 != 0)
        CHECK_EQ_INT((int)(DSW(actor_pset(g2)) & 0x7fffu), 0x3f57);
    CHECK_EQ_INT((int)DSW(DS_00105F34 + 2), 3);    /* glyph count includes space */

    /* All-spaces takes 0x2F830's early arm: it clears the width-long run of
     * cells through 0x2F280 (releasing their records) and returns 0. */
    actors_reset();
    DSD(DS_00105F34) = 0;
    u32 r0 = actor_spawn((const u32 *)(mem + 0x9AC30u), 0x4840u, 0xE0u,
                         0x1B00u, 0u);
    u32 r1 = actor_spawn((const u32 *)(mem + 0x9AC30u), 0x4840u, 0xE0u,
                         0x1B00u, 0u);
    CHECK(r0 != 0 && r1 != 0, "pre-placed cells");
    DSD(DS_00105F38) = r0;
    DSD(DS_00105F38 + 4u) = r1;
    text_cursor_set(0, 0, (const u8 *)"  ", 0);
    CHECK_EQ_INT((int)DSD(DS_00105F38), 0);
    CHECK_EQ_INT((int)DSD(DS_00105F38 + 4u), 0);
    if (r0 != 0) CHECK_EQ_INT((int)(DSW(r0 + 0x28) & 8u), 8);
    CHECK_EQ_INT((int)DSW(DS_00105F34 + 2), 0);    /* extent 0 */

    /* An occupied cell is released through 0x2AD40 and re-spawned as the glyph:
     * the cell's pset sprite id changes from the pre-placed descriptor's
     * 0x2C11 to the glyph's 0x3F56. */
    actors_reset();
    DSD(DS_00105F34) = 0;
    u32 old = actor_spawn((const u32 *)(mem + 0x9AC30u), 0x4840u, 0xE0u,
                          0x1B00u, 0u);
    CHECK(old != 0, "old cell");
    DSD(DS_00105F38) = old;
    if (old != 0) CHECK_EQ_INT((int)(DSW(actor_pset(old)) & 0x7fffu), 0x2c11);
    text_cursor_set(0, 0, (const u8 *)"A", 0);
    u32 neu = grid(0, 0);
    CHECK(neu != 0, "cell re-spawned");
    if (neu != 0)
        CHECK_EQ_INT((int)(DSW(actor_pset(neu)) & 0x7fffu), 0x3f56);

    /* A negative class (mode 3 '"') makes 0x2F5A0 return 1, so 0x2F830 aborts
     * with 0 and emits nothing. */
    actors_reset();
    DSD(DS_00105F34) = 0;
    text_cursor_set(0, 0, (const u8 *)"\"", 3);
    CHECK_EQ_INT((int)grid(0, 0), 0);
    CHECK_EQ_INT((int)DSW(DS_00105F34 + 2), 0);

    check_text_vertical_number();
    check_menu();

    return g_failures - before;
}
