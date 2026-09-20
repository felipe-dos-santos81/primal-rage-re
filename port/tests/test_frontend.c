/* The front-end helpers 0x1C6D4 and 0x33904, and (under PR_FRONTEND_DUMP) the
 * state-2 selector driver. The driver calls game_init(), which may run once per
 * process, so it is env-gated exactly like test_title(): run_tests.c runs this
 * file alone when PR_FRONTEND_DUMP is set, and the helper checks still run at
 * the top of test_frontend() before the driver. */
#include "game/flow.h"
#include "game/actors.h"
#include "platform/gfx.h"
#include "mem.h"
#include "symbols.h"
#include "test.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

int test_frontend(void)
{
    int before = g_failures;

    /* 0x1C6D4: membership over nine resource addresses read from mem[]. The
     * raw dereferences its argument, so use a scratch linear address inside
     * mem[] (never a host pointer). */
    {
        static const u32 known[9] = {
            0x80995Cu, 0x80997Cu, 0x809984u, 0x80998Cu, 0x809994u,
            0x80999Cu, 0x8099A4u, 0x8099ACu, 0x8099CCu,
        };
        const u32 scratch = 0x3002000u;   /* scratch linear addr inside mem[] */
        u32 saved = DSD(scratch);
        for (u32 i = 0; i < 9u; i++) {
            DSD(scratch) = known[i];
            CHECK_EQ_INT((int)frontend_resource_known(scratch), 1);
        }
        DSD(scratch) = 0x809980u;         /* between two members */
        CHECK_EQ_INT((int)frontend_resource_known(scratch), 0);
        DSD(scratch) = 0x8099B0u;         /* past the last member */
        CHECK_EQ_INT((int)frontend_resource_known(scratch), 0);
        DSD(scratch) = saved;
    }

    /* 0x33904: the 0x10-stride table iterator at DS_00107608, bounded by
     * 0x107798 and stopping at a nonzero dword at +0x14. The table is runtime
     * state and empty before game_init(), so seed exactly one live entry and
     * restore the whole table afterwards. */
    {
        static u8 saved[0x190];
        const u32 tbl = DS_00107608;
        for (u32 i = 0; i < 0x190u; i++) saved[i] = DSB(tbl + i);
        mem_fill(tbl, 0, 0x190u);
        DSD(tbl + 0x04u) = 1u;            /* tbl's own +4 is live */
        DSD(tbl + 0x14u) = 1u;            /* entry at tbl+0x10 is live */
        /* The iterator advances by 0x10 before its first test, so the live
         * dword at tbl+4 is skipped and the entry at tbl+0x10 wins. */
        CHECK_EQ_INT((int)frontend_list_next(0), (int)(tbl + 0x10u));
        CHECK_EQ_INT((int)frontend_list_next(tbl + 0x10u), 0);
        for (u32 i = 0; i < 0x190u; i++) DSB(tbl + i) = saved[i];
    }

    /* 0x11F28 / 0x11D04: the coin path. The mask table DS_0009ACBC lives in
     * the data object; test_le() maps PRAGE.EXE in the shared suite, but the
     * PR_FRONTEND_DUMP branch runs this file alone, so map it here to read the
     * shipped masks. An accepted coin debits one credit through 0x2CA7C and
     * returns early, so the frame's state dispatch is skipped; a rejected poll
     * leaves the credit alone and runs the dispatch (state 9's countdown is the
     * observable that the dispatch ran or was skipped). */
    {
        const char *gdir = getenv("PR_GAME_DIR");
        char exe[560];
        if (gdir == NULL || gdir[0] == '\0') gdir = "data/game/C";
        snprintf(exe, sizeof exe, "%s/PRAGE.EXE", gdir);
        if (DSD(DS_0009ACBC) == 0u)
            CHECK(mem_load_le(exe, NULL) == 1,
                  "PRAGE.EXE maps for the coin mask table");
    }
    {
        const u32 saved_c00 = DSD(DS_00105C00);
        const u32 saved_e4  = DSD(DS_001088E4);
        const u8  saved_1d  = DSB(DS_00104B1D);
        const u8  saved_1f  = DSB(DS_00104B1F);
        const u8  saved_60  = DSB(DS_00105D60);
        const u32 saved_dc  = DSD(DS_001082DC);
        const u16 saved_64  = DSW(DS_000F0A64);
        const u16 saved_6a  = DSW(DS_000F0A6A);
        const u16 saved_6c  = DSW(DS_000F0A6C);

        /* The masks are the shipped ones, or the checks below are vacuous. */
        CHECK_EQ_INT((int)DSD(DS_0009ACBC), 0x01000000);
        CHECK_EQ_INT((int)DSD(DS_0009ACBC + 4u), 0x00000100);

        DSB(DS_00104B1D) = 0;          /* coin poll enabled */
        DSB(DS_00104B1F) = 0;          /* debit not suppressed */
        DSB(DS_00105D60) = 0;          /* not free play */
        DSD(DS_00105C00) = 5u;
        DSD(DS_001082DC) = 0;          /* no localisation table: empty strings */

        /* Reject: no newly-pressed bit -> no debit, state 9's countdown runs. */
        DSD(DS_001088E4) = 0u;
        DSW(DS_000F0A64) = 9; DSW(DS_000F0A6A) = 2; DSW(DS_000F0A6C) = 3;
        game_state_step();
        CHECK_EQ_INT((int)DSD(DS_00105C00), 5);
        CHECK_EQ_INT((int)DSW(DS_000F0A6A), 1);

        /* Accept event 0: one credit debited, dispatch skipped (countdown held). */
        DSD(DS_001088E4) = DSD(DS_0009ACBC);
        DSW(DS_000F0A6A) = 2;
        game_state_step();
        CHECK_EQ_INT((int)DSD(DS_00105C00), 4);
        CHECK_EQ_INT((int)DSW(DS_000F0A6A), 2);

        /* Accept event 1: likewise. */
        DSD(DS_001088E4) = DSD(DS_0009ACBC + 4u);
        game_state_step();
        CHECK_EQ_INT((int)DSD(DS_00105C00), 3);

        DSD(DS_00105C00) = saved_c00; DSD(DS_001088E4) = saved_e4;
        DSB(DS_00104B1D) = saved_1d;  DSB(DS_00104B1F) = saved_1f;
        DSB(DS_00105D60) = saved_60;  DSD(DS_001082DC) = saved_dc;
        DSW(DS_000F0A64) = saved_64;  DSW(DS_000F0A6A) = saved_6a;
        DSW(DS_000F0A6C) = saved_6c;
    }

    /* 0x4F1D0 zeroes the two origin words; 0x4F1E4 writes DS_00104B15. They are
     * distinct raw functions and 4b-B conflated them. */
    {
        const u16 saved_3a = DSW(DS_00107A3A);
        const u16 saved_38 = DSW(DS_00107A38);
        const u8  saved_15 = DSB(DS_00104B15);
        DSW(DS_00107A3A) = 0x1234;
        DSW(DS_00107A38) = 0x5678;
        DSB(DS_00104B15) = 0x9A;
        frontend_origin_zero();
        CHECK_EQ_INT((int)DSW(DS_00107A3A), 0);
        CHECK_EQ_INT((int)DSW(DS_00107A38), 0);
        CHECK_EQ_INT((int)DSB(DS_00104B15), 0x9A);   /* 0x4F1D0 does NOT touch it */
        DSW(DS_00107A3A) = saved_3a; DSW(DS_00107A38) = saved_38;
        DSB(DS_00104B15) = saved_15;
    }

    const char *dump = getenv("PR_FRONTEND_DUMP");
    if (dump == NULL || dump[0] == '\0') {
        printf("test_frontend: PR_FRONTEND_DUMP unset, state-2 driver skipped\n");
        return g_failures - before;
    }

    /* The driver runs the real init and loop for a fixed window, as the title
     * driver does, because game_init() may run once per process.
     * PR_FRONTEND_DUMP names a directory to receive the hash log and, from the
     * state-3 entry on, one RGB24 frame per presented frame. The log covers the
     * whole window, through states 3/4, so two runs can be diffed; the frame
     * dump is capped by PR_FRONTEND_DUMP_FRAMES (default 300). */
    {
        const char *dir = getenv("PR_GAME_DIR");
        if (dir == NULL || dir[0] == '\0') dir = "data/game/C";
        game_set_game_dir(dir);
        game_init();
        actors_pin_anim_tick_zero(1);

        /* Enter state 2 at phase 0, the entry game_state_title() leaves for. */
        DSW(DS_000F0A64) = 2;
        DSB(DS_000F0A6F) = 0;

        mkdir(dump, 0777);      /* ignore EEXIST; matches the frame-dump hook */

        char log_path[1200];
        snprintf(log_path, sizeof log_path, "%s/select.log", dump);
        FILE *log = fopen(log_path, "w");
        CHECK(log != NULL, "state-2 hash log opens");

        const char *cap_s = getenv("PR_FRONTEND_DUMP_FRAMES");
        long raw_cap = cap_s ? strtol(cap_s, NULL, 0) : 300;
        int dumped = 0;

        u32 seen_entries = 0;
        for (int i = 0; i < 900; i++) {
            DSB(DS_000A81A8) = 1;          /* exactly one game_loop iteration */
            game_loop();
            if (DSB(DS_000F0A6E) < 6u) seen_entries |= 1u << DSB(DS_000F0A6E);
            if (log != NULL) {
                /* Hash the presented index buffer without reading pixels. */
                const u8 *fb = mem + DSD(DS_000E87A4);
                u32 h = 2166136261u;
                for (u32 b = 0; b < 320u * 200u; b++) h = (h ^ fb[b]) * 16777619u;
                fprintf(log, "%d %u %u\n", i, (unsigned)DSB(DS_000F0A6F), h);
            }
            /* Once state 3 is reached, write the just-presented frame as RGB24
             * (192000 bytes, 320x200) through gfx_dac, the same form the title
             * and attract hooks write. swap_buffers() has already run, so the
             * just-presented buffer is DS_000E87A0 (the hook reads DS_000E87A4
             * before the swap). */
            if (DSW(DS_000F0A64) >= 3u && dumped < (int)raw_cap) {
                const u8 *fb = mem + DSD(DS_000E87A0);
                char path[1300];
                snprintf(path, sizeof path, "%s/frame_%04d.raw", dump, dumped);
                FILE *fr = fopen(path, "wb");
                if (fr != NULL) {
                    for (u32 b = 0; b < 320u * 200u; b++) {
                        const u8 *rgb = gfx_dac[fb[b]];
                        fwrite(rgb, 1, 3, fr);
                    }
                    fclose(fr);
                }
                dumped++;
            }
        }
        if (log != NULL) fclose(log);

        /* The raw's timeline: six entries, each drawn then paused, then state 3.
         * Entry k is drawn on frame 1+93k; after the sixth, 0x1E pause frames
         * and the phase-3 handoff land state 3 on frame 589 (the arithmetic is
         * in docs/superpowers/plans/2026-09-19-frontend-input-derivations.md).
         * The 900-frame window leaves margin through states 3/4. */
        CHECK_EQ_INT((int)seen_entries, 0x3F);
        CHECK_EQ_INT((int)DSW(DS_000F0A64), 3);
        CHECK_EQ_INT(dumped, (int)(raw_cap < (900 - 589) ? raw_cap : (900 - 589)));
        game_shutdown();
    }
    return g_failures - before;
}

/* The fallback determinism gate: two independent PR_FRONTEND_DUMP runs of the
 * states 3/4 window must write byte-identical frame-hash logs. game_init() may
 * run once per process, so the check re-invokes this binary twice (the two
 * invocations + diff pattern) and compares the two logs. The runner sets
 * PR_FRONTEND_DET to the dump root; the children see only PR_FRONTEND_DUMP. */
int test_frontend_determinism(const char *self)
{
    char root[1024], gdir[1024];
    const char *r = getenv("PR_FRONTEND_DET");
    const char *d = getenv("PR_GAME_DIR");
    snprintf(root, sizeof root, "%s", r != NULL ? r : "/tmp/pr_frontend_det");
    snprintf(gdir, sizeof gdir, "%s",
             (d != NULL && d[0] != '\0') ? d : "data/game/C");

    /* The children must not inherit this mode or they recurse; drop it from the
     * environment before the re-invocations. root/gdir are copied first because
     * unsetenv invalidates the pointers getenv returned. */
    unsetenv("PR_FRONTEND_DET");
    mkdir(root, 0777);      /* the run1/run2 children only create their leaf */

    for (int k = 1; k <= 2; k++) {
        char cmd[4096], log[1300];
        snprintf(log, sizeof log, "%s/run%d.log", root, k);
        snprintf(cmd, sizeof cmd,
                 "PR_FRONTEND_DUMP='%s/run%d' PR_GAME_DIR='%s' '%s' >'%s' 2>&1",
                 root, k, gdir, self, log);
        CHECK(system(cmd) == 0, "front-end determinism run completes");
    }

    char p1[1300], p2[1300];
    snprintf(p1, sizeof p1, "%s/run1/select.log", root);
    snprintf(p2, sizeof p2, "%s/run2/select.log", root);
    FILE *f1 = fopen(p1, "rb");
    FILE *f2 = fopen(p2, "rb");
    CHECK(f1 != NULL && f2 != NULL, "front-end determinism logs open");
    if (f1 != NULL && f2 != NULL) {
        int same = 1;
        for (;;) {
            int a = fgetc(f1), b = fgetc(f2);
            if (a != b) { same = 0; break; }
            if (a == EOF) break;
        }
        CHECK(same, "two front-end runs' frame hashes are byte-identical");
    }
    if (f1 != NULL) fclose(f1);
    if (f2 != NULL) fclose(f2);
    return g_failures;
}
