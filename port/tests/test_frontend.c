/* The front-end helpers 0x1C6D4 and 0x33904, and (under PR_FRONTEND_DUMP) the
 * state-2 selector driver. The driver calls game_init(), which may run once per
 * process, so it is env-gated exactly like test_title(): run_tests.c runs this
 * file alone when PR_FRONTEND_DUMP is set, and the helper checks still run at
 * the top of test_frontend() before the driver. */
#include "game/flow.h"
#include "game/actors.h"
#include "game/effects.h"
#include "game/fight.h"
#include "game/rng.h"
#include "platform/gfx.h"
#include "mem.h"
#include "symbols.h"
#include "test.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

/* The reference's LCG state when its state 6 runs. The attract's voice tick
 * (0x10F28, called from 0x11559 while DS_0009AD58 == 0) is the only consumer
 * before the title: it draws 26 values over the boot attract, and the port's
 * own attract reaches attract_step case 0xB's title handoff with
 * DS_000EF6D8 == 0x4308698B, i.e. seed 0xABCD advanced 26 steps. The title's
 * three draws are pinned to constants (no advance) and states 2..5 draw
 * nothing (only the pinned opcode-8 handler is reachable), so the reference's
 * state-6 entry is that same state. The driver enters at state 2, so it
 * re-seeds to it — the same pattern test_title_window uses for the pinned
 * title. The assertion below fails if the re-seed or the state-6 draws move. */
#define FRONTEND_RNG_AFTER_ATTRACT 0x4308698Bu

/* Seed exactly one live front-end list entry at DS_00107608 with `handle` in
 * its +0 dword. The iterator advances by 0x10 before its first test, so the
 * live dword at tbl+4 is skipped and the entry at tbl+0x10 wins. `saved` must
 * hold 0x190 bytes; restore_frontend_list() puts the whole table back. Shared
 * by the 0x33904 iterator check and the state-3 (0x12484) check. */
static void seed_frontend_list(u32 handle, u8 *saved)
{
    const u32 tbl = DS_00107608;
    for (u32 i = 0; i < 0x190u; i++) saved[i] = DSB(tbl + i);
    mem_fill(tbl, 0, 0x190u);
    DSD(tbl + 0x04u) = 1u;            /* tbl's own +4 is live */
    DSD(tbl + 0x10u) = handle;        /* the returned entry's +0 handle */
    DSD(tbl + 0x14u) = 1u;            /* entry at tbl+0x10 is live */
}

static void restore_frontend_list(const u8 *saved)
{
    const u32 tbl = DS_00107608;
    for (u32 i = 0; i < 0x190u; i++) DSB(tbl + i) = saved[i];
}

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
        seed_frontend_list(0u, saved);
        CHECK_EQ_INT((int)frontend_list_next(0), (int)(DS_00107608 + 0x10u));
        CHECK_EQ_INT((int)frontend_list_next(DS_00107608 + 0x10u), 0);
        restore_frontend_list(saved);
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

    /* Task 8: the effect call sites 0x29B74 (the DS_00104AE4 mode-0x17 handler)
     * and 0x41578 are deferred, not shipped. The raw reaches them only through
     * 0x24C5C's unported mode cases (0x12 and 0x16..0x1b) and the unported
     * match/fight chain; the port's DS_00104B00 is fixed at 3 by 0x10E80, so
     * wiring either would be a dispatch path nothing can reach (UNOWNED BY THIS
     * PLAN; see port/spec/game_flow.md). This pins that no unreachable handler
     * is registered: registering FN_00029B74 or FN_00041578 fails it. The
     * behavioral half (the ported state machine never arms DS_00104AE4 and
     * never leaves mode 3) is asserted in the state-5 block below. */
    {
        CHECK(fn_resolve(FN_00029B74) == NULL,
              "0x29B74 is deferred, not registered");
        CHECK(fn_resolve(FN_00041578) == NULL,
              "0x41578 is deferred, not registered");
    }

    /* 0x12484: state 3 (the post-select presentation). Phase 0 re-spawns the
     * four corner rows, spawns a type-3 effect (0x13C70) for the live list entry
     * whose handle is 0x3E688, takes the DS_00104528 bit-1 branch and hands off
     * through 0x12658, which stores the first of its three actors at
     * DS_000F0A58. Phase 1 terminates into state 9 once the handoff actor's
     * offset reaches 0x1E00. Needs the actor/effect pools res_load_index
     * allocates; the isolated PR_FRONTEND_DUMP run has none before game_init(),
     * so it skips here and exercises state 3 through the driver instead. */
    if (DSD(DS_001014F4) != 0) {
        static u8 saved_list[0x190];
        const u8  saved_1d = DSB(DS_00104B1D);
        const u8  saved_29 = DSB(DS_00104528 + 1u);
        const u32 saved_40 = DSD(DS_000F0A40);
        const u32 saved_58 = DSD(DS_000F0A58);
        const u16 saved_64 = DSW(DS_000F0A64);
        const u16 saved_6a = DSW(DS_000F0A6A);
        const u16 saved_6c = DSW(DS_000F0A6C);
        const u8  saved_6f = DSB(DS_000F0A6F);
        const u16 saved_38 = DSW(DS_00107A38);
        const u16 saved_44 = DSW(DS_00107A44);
        const u8  saved_72 = DSB(DS_000F0A72);

        seed_frontend_list(0x3E688u, saved_list);
        DSB(DS_00104B1D) = 0;
        DSW(DS_000F0A64) = 3;
        DSW(DS_000F0A6A) = 1;
        DSB(DS_000F0A6F) = 0;
        DSB(DS_00104528 + 1u) |= 2u;               /* take the DS_000F0A40 branch */
        DSD(DS_000F0A40) = 0;
        game_state_step();
        CHECK_EQ_INT((int)DSB(DS_000F0A6F), 1);
        CHECK(DSD(DS_000F0A40) != 0u, "phase 0 spawned through 0x2AE14");
        CHECK(effects_active() != 0, "phase 0 spawned a type-3 effect");

        {
            u32 cam = DSD(DS_000F0A58);
            DSD(cam + 0x1Cu) = 0x1E01u;            /* sentinel; terminate stores 0x1E00 */
            DSD(cam + 0x34u) = 0x10000u;           /* high word 1, so b = 1 >= |a| = 1 */
            DSB(DS_000F0A6F) = 1;
            game_state_step();
            CHECK_EQ_INT((int)DSW(DS_000F0A64), 9);
            CHECK_EQ_INT((int)DSW(DS_000F0A6C), 6);
            CHECK_EQ_INT((int)DSW(DS_000F0A6A), 0xF0);
            CHECK_EQ_INT((int)DSD(cam + 0x1Cu), 0x1E00);
        }

        restore_frontend_list(saved_list);
        DSB(DS_00104B1D) = saved_1d;   DSB(DS_00104528 + 1u) = saved_29;
        DSD(DS_000F0A40) = saved_40;   DSD(DS_000F0A58) = saved_58;
        DSW(DS_000F0A64) = saved_64;   DSW(DS_000F0A6A) = saved_6a;
        DSW(DS_000F0A6C) = saved_6c;   DSB(DS_000F0A6F) = saved_6f;
        DSW(DS_00107A38) = saved_38;   DSW(DS_00107A44) = saved_44;
        DSB(DS_000F0A72) = saved_72;
    }

    /* 0x11578: state 4 (the match-up credit roll). Its phase counter is
     * DS_0009AD98, separate from the dispatch word DS_000F0A64. The 8/12/13
     * 0x2F4BC call counts are literal in the raw and are transcribed unrolled
     * rather than counted here; only the phase state is asserted. The phase-0
     * 0x2AE14 spawn needs the actor pool, so that one check is guarded (the
     * isolated PR_FRONTEND_DUMP run reaches here before game_init()). */
    {
        const u8  saved_1d = DSB(DS_00104B1D);
        const u8  saved_71 = DSB(DS_000F0A71);
        const u8  saved_58 = DSB(DS_0009AD58);
        const u8  saved_15 = DSB(DS_00104B15);
        const u8  saved_c5 = DSB(DS_00105C05);
        const u16 saved_64 = DSW(DS_000F0A64);
        const u16 saved_6a = DSW(DS_000F0A6A);
        const u16 saved_6c = DSW(DS_000F0A6C);
        const u16 saved_74 = DSW(DS_000F0A74);
        const u16 saved_76 = DSW(DS_000F0A76);
        const u16 saved_98 = DSW(DS_0009AD98);

        DSB(DS_00104B1D) = 1;          /* coin poll skipped */
        DSB(DS_000F0A71) = 1;          /* pause/continue tails skipped */
        DSB(DS_0009AD58) = 1;          /* overlay skipped */
        DSW(DS_000F0A64) = 4;

        /* Phase 0: the setup sequence, the 180-frame timer, continuation 1. */
        DSW(DS_0009AD98) = 0;
        DSW(DS_000F0A76) = 0;
        DSW(DS_000F0A74) = 0;
        game_state_step();
        CHECK_EQ_INT((int)DSW(DS_0009AD98), 4);
        CHECK_EQ_INT((int)DSW(DS_000F0A76), 0xB4);
        CHECK_EQ_INT((int)DSW(DS_000F0A74), 1);

        /* The phase-0 0x2AE14 descriptor is the data object's 0x9AD84 (raw
         * 0x115CA), not a literal 0. Every phase-0 spawn head-inserts, so the
         * first one (the 0x2AE14 actor) is the active list's tail and carries
         * the descriptor's first dword in its +8 record dword. */
        if (DSD(DS_001014F4) != 0) {
            u32 tail = 0;
            for (u32 rec = actor_list_head(); rec != 0u; rec = actor_next(rec))
                tail = rec;
            CHECK(tail != 0u, "phase 0 filled the actor list");
            if (tail != 0u)
                CHECK_EQ_INT((int)DSD(tail + 8u), (int)DSD(0x9AD84u));
        }

        /* Phase 4 with a non-zero timer decrements it and does not continue.
         * DS_000F0A74 is a sentinel that differs from both 4 and the natural
         * continuation values, so an always-continue bug is unambiguous. */
        DSW(DS_0009AD98) = 4;
        DSW(DS_000F0A76) = 2;
        DSW(DS_000F0A74) = 9;
        game_state_step();
        CHECK_EQ_INT((int)DSW(DS_000F0A76), 1);
        CHECK_EQ_INT((int)DSW(DS_0009AD98), 4);

        /* Phase 4 at zero: the raw tests the value before the decrement, so the
         * store wraps to 0xFFFF and the continuation fires this frame. */
        DSW(DS_000F0A76) = 0;
        DSW(DS_000F0A74) = 3;
        game_state_step();
        CHECK_EQ_INT((int)DSW(DS_0009AD98), 3);
        CHECK_EQ_INT((int)DSW(DS_000F0A76), 0xFFFF);

        /* Phase 3 hands to state 9 with the state-3 terminator values. The two
         * written globals start at sentinels so the checks test the stores. */
        DSW(DS_0009AD98) = 3;
        DSW(DS_000F0A6C) = 0x1234;
        DSW(DS_000F0A6A) = 0x1234;
        game_state_step();
        CHECK_EQ_INT((int)DSW(DS_000F0A64), 9);
        CHECK_EQ_INT((int)DSW(DS_000F0A6C), 0);
        CHECK_EQ_INT((int)DSW(DS_000F0A6A), 1);
        CHECK_EQ_INT((int)DSW(DS_0009AD98), 0);

        DSB(DS_00104B1D) = saved_1d;   DSB(DS_000F0A71) = saved_71;
        DSB(DS_0009AD58) = saved_58;
        DSB(DS_00104B15) = saved_15;   DSB(DS_00105C05) = saved_c5;
        DSW(DS_000F0A64) = saved_64;   DSW(DS_000F0A6A) = saved_6a;
        DSW(DS_000F0A6C) = saved_6c;   DSW(DS_000F0A74) = saved_74;
        DSW(DS_000F0A76) = saved_76;   DSW(DS_0009AD98) = saved_98;
    }

    /* 0x11D04 case 5 (state 5, the match-start block inline in the dispatch).
     * The body runs 0x2C3FC (voice cancel, skipped), 0x1EA08, 0x2C06C, 0x32970
     * (run clock, skipped), then stores the raw's five values. The case uses
     * break, so the three shared tails still run after the switch; the second
     * call below arms one of them to prove the case does not return early. */
    {
        const u8  saved_1d = DSB(DS_00104B1D);
        const u8  saved_71 = DSB(DS_000F0A71);
        const u8  saved_58 = DSB(DS_0009AD58);
        const u8  saved_15 = DSB(DS_00104B15);
        const u8  saved_60 = DSB(DS_00105D60);
        const u8  saved_19 = DSB(DS_00104B19 + 2u);
        const u8  saved_d8_1 = DSB(DS_001088D8 + 1u);
        const u8  saved_d8_3 = DSB(DS_001088D8 + 3u);
        const u8  saved_c5 = DSB(DS_00105C05);
        const u16 saved_64 = DSW(DS_000F0A64);
        const u16 saved_6a = DSW(DS_000F0A6A);
        const u16 saved_6c = DSW(DS_000F0A6C);
        const u8  saved_6f = DSB(DS_000F0A6F);
        const u8  saved_72 = DSB(DS_000F0A72);
        const u32 saved_ae8 = DSD(DS_00104AE8);
        const u32 saved_aec = DSD(DS_00104AEC);
        const u32 saved_ad0 = DSD(DS_00104AD0);
        const u32 saved_ae4 = DSD(DS_00104AE4);
        const u16 saved_b00 = DSW(DS_00104B00);
        u32 saved_row[7];
        for (u32 i = 0; i < 7u; i++)
            saved_row[i] = DSD(DS_00107A1C + i * 4u);

        DSB(DS_00104B1D) = 1;          /* coin poll skipped */
        DSB(DS_000F0A71) = 1;          /* pause/continue tails skipped */
        DSB(DS_0009AD58) = 1;          /* overlay skipped */

        /* Sentinels differ from every post-condition so a no-op case fails. */
        DSW(DS_000F0A64) = 5;
        DSW(DS_000F0A6C) = 0x1234;
        DSW(DS_000F0A6A) = 0x1234;
        DSB(DS_000F0A6F) = 0xAB;
        DSB(DS_000F0A72) = 0xCD;
        DSB(DS_00104B15) = 0x9A;
        DSB(DS_00105C05) = 0x9A;   /* sentinel; 0x2C06C stores 0x1D */
        /* Sentinels for 0x1EA08's prefix: 0x2BAF4 zeroes the three process
         * masks and 0x38B70 clears the 7-entry row table before 0x38B18 fills
         * slot 0 from the 0xA7B6C descriptor. */
        DSD(DS_00104AE8) = 0x12345678u;
        DSD(DS_00104AEC) = 0x12345678u;
        DSD(DS_00104AD0) = 0x12345678u;
        mem_fill(DS_00107A1C, 0, 28u);
        /* Task 8 sentinels: the deferred effect call sites must stay unarmed
         * through the ported path. 0xDEADBEEF is never a code address the port
         * arms, and mode 3 is the only mode the port dispatches. */
        DSD(DS_00104AE4) = 0xDEADBEEFu;
        DSW(DS_00104B00) = 3;
        game_state_step();
        CHECK_EQ_INT((int)DSW(DS_000F0A64), 9);
        CHECK_EQ_INT((int)DSW(DS_000F0A6C), 6);
        CHECK_EQ_INT((int)DSW(DS_000F0A6A), 0x12C);
        CHECK_EQ_INT((int)DSB(DS_000F0A6F), 0);
        CHECK_EQ_INT((int)DSB(DS_000F0A72), 0);
        CHECK_EQ_INT((int)DSB(DS_00104B15), 0);   /* 0x1EA08 ran 0x4F1E4 */
        CHECK_EQ_INT((int)DSB(DS_00105C05), 0x1D);  /* 0x2C06C ran */
        /* Task 8: the ported state-5 path does not arm the deferred 0x29B74
         * handler and does not leave mode 3 (so the six call dword [0x104ae4]
         * sites and the four 0x41578 sites stay unreachable). */
        CHECK(DSD(DS_00104AE4) == 0xDEADBEEFu,
              "state 5 does not arm the deferred 0x29B74 handler");
        CHECK_EQ_INT((int)DSW(DS_00104B00), 3);
        /* 0x1EA08's prefix is observable only with the pool present (the
         * isolated PR_FRONTEND_DUMP run reaches here before game_init()). */
        if (DSD(DS_001014F4) != 0) {
            CHECK_EQ_INT((int)DSD(DS_00104AE8), 0);   /* 0x2BAF4 ran */
            CHECK_EQ_INT((int)DSD(DS_00104AEC), 0);
            CHECK_EQ_INT((int)DSD(DS_00104AD0), 0);
            CHECK(DSD(DS_00107A1C) != 0u, "0x1EA08 spawned the 0xA7B6C row");
            if (DSD(DS_00107A1C) != 0u) {
                u32 row = DSD(DS_00107A1C);
                CHECK_EQ_INT((int)DSD(row + 8u), (int)DSD(0xA7B6Cu));
            }
        }

        /* Fall-through proof: re-enter state 5 with the pause tail (0x10DB0)
         * armed and its extra branch disabled. The tail runs after the case and
         * overwrites the case's state 9 with state 4; DS_000F0A71 records that
         * it fired. A case that returned before the tails would leave 9. */
        DSW(DS_000F0A64) = 5;
        DSB(DS_000F0A71) = 0;
        DSB(DS_00104B19 + 2u) = 0;
        DSB(DS_001088D8 + 3u) |= 0x20u;
        DSB(DS_001088D8 + 1u) |= 0x10u;
        DSB(DS_00104B15) = 0x9A;   /* sentinel; 0x1EA08's 0x4F1E4 stores 0 */
        DSB(DS_00105C05) = 0x9A;   /* sentinel; 0x2C06C stores 0x1D */
        game_state_step();
        CHECK_EQ_INT((int)DSB(DS_00104B15), 0);     /* 0x1EA08 ran again */
        CHECK_EQ_INT((int)DSB(DS_00105C05), 0x1D);  /* 0x2C06C ran again */
        CHECK_EQ_INT((int)DSB(DS_000F0A71), 1);
        CHECK_EQ_INT((int)DSW(DS_000F0A64), 4);

        DSB(DS_00104B1D) = saved_1d;   DSB(DS_000F0A71) = saved_71;
        DSB(DS_0009AD58) = saved_58;   DSB(DS_00104B15) = saved_15;
        DSB(DS_00105D60) = saved_60;   DSB(DS_00104B19 + 2u) = saved_19;
        DSB(DS_001088D8 + 1u) = saved_d8_1;
        DSB(DS_001088D8 + 3u) = saved_d8_3;
        DSB(DS_00105C05) = saved_c5;
        DSW(DS_000F0A64) = saved_64;   DSW(DS_000F0A6A) = saved_6a;
        DSW(DS_000F0A6C) = saved_6c;   DSB(DS_000F0A6F) = saved_6f;
        DSB(DS_000F0A72) = saved_72;
        DSD(DS_00104AE8) = saved_ae8;  DSD(DS_00104AEC) = saved_aec;
        DSD(DS_00104AD0) = saved_ad0;
        DSD(DS_00104AE4) = saved_ae4;  DSW(DS_00104B00) = saved_b00;
        for (u32 i = 0; i < 7u; i++)
            DSD(DS_00107A1C + i * 4u) = saved_row[i];
    }

    /* 0x41350 / 0xA8A28: the palette-variant flag the T-rex's handle is
     * selected by. With the original's pre-state (DS_0010816A[0..1] = 0, the
     * BSS value the front-end dump driver must seed — record §7.1) side 0's
     * char equals side 1's, so variant 1 selects 0xA8A28[1] = 0x1BB9FCD8; the
     * driver's old 0xFF seed made variant 0 and 0x1BB9FD58. The variant sentinel
     * starts at 0, differing from the post-condition 1. */
    {
        const u8  saved_1d  = DSB(DS_00104B1D);
        const u8  saved_6a0 = DSB(DS_0010816A);
        const u8  saved_6a1 = DSB(DS_0010816A + 1u);
        const u8  saved_6e0 = DSB(DS_0010816E);
        const u8  saved_b34 = DSB(DS_00105B34);
        const u8  saved_b35 = DSB(DS_00105B34 + 1u);
        const u8  saved_idx = DSB(0x0010810Du);
        const u8  saved_63  = DSB(DS_001077B0 + 0x63u);
        const u32 saved_3c  = DSD(DS_001077B0 + 0x3Cu);
        const u8  saved_7f  = DSB(DS_001077B0 + 0x7Fu);
        const u8  saved_80  = DSB(DS_001077B0 + 0x80u);
        const u8  saved_82  = DSB(DS_001077B0 + 0x82u);
        const u8  saved_5b  = DSB(DS_001077B0 + 0x5Bu);
        const u16 saved_860 = DSW(DS_00108860);

        DSB(DS_00104B1D) = 0;          /* the char store runs */
        DSB(DS_0010816A) = 0;          /* the original's BSS pre-state */
        DSB(DS_0010816A + 1u) = 0;
        DSB(DS_00105B34) = 0;          /* sentinel; the post-condition is 1 */
        DSB(DS_00105B34 + 1u) = 0;
        fight_char_select(0u, 0u);     /* 0xC835A[0] = 0, the T-rex */
        CHECK_EQ_INT((int)DSB(DS_00105B34), 1);
        CHECK_EQ_INT((int)DSD(0xA8A28u + DSB(DS_00105B34) * 4u), 0x1BB9FCD8);

        DSB(DS_00104B1D) = saved_1d;
        DSB(DS_0010816A) = saved_6a0;  DSB(DS_0010816A + 1u) = saved_6a1;
        DSB(DS_0010816E) = saved_6e0;
        DSB(DS_00105B34) = saved_b34;  DSB(DS_00105B34 + 1u) = saved_b35;
        DSB(0x0010810Du) = saved_idx;
        DSB(DS_001077B0 + 0x63u) = saved_63;
        DSD(DS_001077B0 + 0x3Cu) = saved_3c;
        DSB(DS_001077B0 + 0x7Fu) = saved_7f;
        DSB(DS_001077B0 + 0x80u) = saved_80;
        DSB(DS_001077B0 + 0x82u) = saved_82;
        DSB(DS_001077B0 + 0x5Bu) = saved_5b;
        DSW(DS_00108860) = saved_860;
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
     * whole window, through the demo, so two runs can be diffed; the frame dump
     * is capped by PR_FRONTEND_DUMP_FRAMES (default 1400).
     *
     * The 2000-iteration loop and the 1400-frame cap are sized from the demo's
     * state-7 exit: state 3 is entered at loop frame 589, state 6 runs at loop
     * frame 1070 (dumped frame 481), and 0x11BCC's timer exit runs at loop frame
     * 1970, where the state drops to 0 and dumping stops. So the state>=3 dump
     * run is loop frames 589..1969, i.e. dumped frames 0..1380 (1381 frames); the
     * 1400 cap covers it and the 2000-frame loop clears the 1970 exit. The
     * front-end window is now distinct [557..810] (254 frames; the earlier
     * [557..813] moved with Task 9's re-capture), and it ends inside the state-9
     * hold — its last exhibited port frame is 258. */
    {
        const char *dir = getenv("PR_GAME_DIR");
        if (dir == NULL || dir[0] == '\0') dir = "data/game/C";
        game_set_game_dir(dir);
        game_init();
        actors_pin_anim_tick_zero(1);

        /* The attract runs before the reference's state 6; this driver skips it
         * by entering at state 2, so re-seed to the attract's post-state. */
        rng_seed(FRONTEND_RNG_AFTER_ATTRACT);

        /* Enter state 2 at phase 0, the entry game_state_title() leaves for. */
        DSW(DS_000F0A64) = 2;
        DSB(DS_000F0A6F) = 0;

        /* Seed the pick bytes so the alignment check below cannot pass on a
         * never-written BSS zero. DS_0010816A[1] must equal the original's BSS
         * value 0 at side 0's 0x41350 call: it is the other side's pick byte
         * then, and only char[0] == char[1] gives the T-rex variant 1 (record
         * §7.1). [0] is overwritten before the variant test, so 0xFF is a
         * sentinel that cannot pass as a never-written BSS zero. */
        DSB(DS_0010816A) = 0xFFu;
        DSB(DS_0010816A + 1u) = 0u;

        mkdir(dump, 0777);      /* ignore EEXIST; matches the frame-dump hook */

        char log_path[1200];
        snprintf(log_path, sizeof log_path, "%s/select.log", dump);
        FILE *log = fopen(log_path, "w");
        CHECK(log != NULL, "state-2 hash log opens");

        const char *cap_s = getenv("PR_FRONTEND_DUMP_FRAMES");
        long raw_cap = cap_s ? strtol(cap_s, NULL, 0) : 1400;
        int dumped = 0;
        int dump_failed = 0;

        u32 seen_entries = 0;
        int reached3 = 0;
        int seen6 = 0;
        u32 entry_lcg = 0;
        int dust_sampled = 0;
        u32 dust_actor[4] = { 0, 0, 0, 0 };
        u32 dust_anim[4] = { 0, 0, 0, 0 };
        u32 dust_type[4] = { 0, 0, 0, 0 };
        /* Task 6b: the state-7 fight's chain, sampled per loop frame. The
         * 0x3531C/0x350D0 machine must take slot 0's +0x52 out of 0 through
         * 0x0E to 3 (the 0x3520E -> 0x3BF0A drive) and resolve a hit; a
         * missing 0x35803 call leaves all four counters false/zero. */
        int s7_saw14 = 0, s7_saw3 = 0, s7_hit = 0, s7_last_change = 0;
        int s7_last = -1;              /* the last loop frame the state is 7 */
        u8 s7_prev[4] = { 0, 0, 0, 0 };
        for (int i = 0; i < 2000; i++) {
            /* The state-9 exit leaves DS_000F0A64 == 6 for the next iteration;
             * nothing draws between the hold and the state-6 handler, so this
             * is the state-6 entry's LCG state. */
            if (!seen6 && DSW(DS_000F0A64) == 6u) {
                entry_lcg = DSD(DS_000EF6D8);
                seen6 = 1;
            }
            DSB(DS_000A81A8) = 1;          /* exactly one game_loop iteration */
            game_loop();
            /* The dust entries exist from the state-6 frame on; sample them
             * before the state-7 frames advance their animations, and read the
             * actor's fields now — later state transitions run actors_reset
             * (0x2BAF4), which zeroes the record pool the pointers point into. */
            if (seen6 && !dust_sampled) {
                u32 n = 0;
                for (u32 e = DSD(DS_0010884C); e != DS_0010884C && n < 4u;
                     e = DSD(e), n++) {
                    u32 actor = DSD(e + 8u);
                    dust_actor[n] = actor;
                    if (actor != 0) {
                        dust_anim[n] = DSD(actor + 8u);
                        dust_type[n] = DSB(actor + 0x48u);
                    }
                }
                dust_sampled = 1;
            }
            if (DSW(DS_000F0A64) == 3u) reached3 = 1;
            if (DSB(DS_000F0A6E) < 6u) seen_entries |= 1u << DSB(DS_000F0A6E);
            if (DSW(DS_000F0A64) == 7u) {
                u8 s0 = DSB(DS_001077B0 + 0x52u);
                u8 s1 = DSB(DS_001077B0 + 0x94u + 0x52u);
                s7_last = i;
                if (s0 == 0x0Eu) s7_saw14 = 1;
                if (s0 == 3u) s7_saw3 = 1;
                if (DSB(DS_001077B0 + 0x7Cu) != 0u
                        || DSB(DS_001077B0 + 0x94u + 0x7Cu) != 0u) s7_hit = 1;
                if (i > 0 && (s0 != s7_prev[0] || s1 != s7_prev[1]))
                    s7_last_change = i;
                s7_prev[0] = s0; s7_prev[1] = s1;
            }
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
             * before the swap). A frame counts as dumped only when all 192000
             * bytes were written, so the count below cannot pass on a short or
             * missing file; one failure stops further attempts. */
            if (DSW(DS_000F0A64) >= 3u && !dump_failed &&
                dumped < (int)raw_cap) {
                const u8 *fb = gfx_display();
                if (fb == NULL) fb = mem + DSD(DS_000E87A0);
                char path[1300];
                snprintf(path, sizeof path, "%s/frame_%04d.raw", dump, dumped);
                FILE *fr = fopen(path, "wb");
                int ok = fr != NULL;
                if (fr != NULL) {
                    for (u32 b = 0; b < 320u * 200u; b++) {
                        if (fwrite(gfx_dac[fb[b]], 1, 3, fr) != 3) {
                            ok = 0;
                            break;
                        }
                    }
                    if (fclose(fr) != 0) ok = 0;
                }
                CHECK(ok, "front-end frame writes to the dump");
                if (ok) dumped++;
                else dump_failed = 1;
            }
        }
        if (log != NULL) fclose(log);

        /* The raw's timeline: six entries, each drawn then paused, then state 3.
         * Entry k is drawn on frame 1+93k; after the sixth, 0x1E pause frames
         * and the phase-3 handoff land state 3 on frame 589 (the arithmetic is
         * in docs/superpowers/plans/2026-09-19-frontend-input-derivations.md).
         * State 3 now runs its 0x12484 phases and hands off to state 9, so the
         * window is asserted to reach state 3, not to end in it; the state it
         * ends in is whatever 0x12658's actor timing produces. The demo adds
         * states 9/6/7: state 7 runs its 900-frame timer and 0x11BCC's exit
         * lands at loop frame 1970, so the state>=3 dump window is loop frames
         * 589..1969 (1381 frames, dump 0..1380). The 1400 cap covers it; the
         * 2000-frame loop clears the 1970 exit. */
        CHECK_EQ_INT((int)seen_entries, 0x3F);
        CHECK(reached3, "the window reaches state 3");
        CHECK_EQ_INT(dumped, (int)(raw_cap < 1381 ? raw_cap : 1381));

        /* Alignment: the driver's state-6 entry sits at the attract's
         * post-state, and the two picks drawn from it (plus the dust builder's
         * six intermediate draws) are the capture's characters, 0xC835A[0] = 0
         * and 0xC835A[3] = 3. A reverted re-seed leaves entry_lcg at the seed
         * (or 0); a dropped dust draw or a restored master-loop draw changes
         * the characters. */
        CHECK(seen6, "the driver reaches state 6");
        CHECK_EQ_INT((int)entry_lcg, (int)FRONTEND_RNG_AFTER_ATTRACT);
        CHECK_EQ_INT((int)DSB(DS_0010816A), 0);
        CHECK_EQ_INT((int)DSB(DS_0010816A + 1u), 3);
        /* Record §7.1: the seed's consequence — the T-rex's variant is 1 and
         * its handle 0x1BB9FCD8 (0xA8A28[1]). The old 0xFF seed left variant 0
         * and 0x1BB9FD58, so this fails under that mutation. */
        CHECK_EQ_INT((int)DSB(DS_00105B34), 1);
        CHECK_EQ_INT((int)DSD(0xA8A28u + DSB(DS_00105B34) * 4u), 0x1BB9FCD8);

        /* Task 6b: the state-7 fight. slot+0x52 enters the 0x34B14 no-op 0x0E
         * and the 0x3531C/0x350D0 machine drives it back to 3 (the 0x3BDDC
         * attack transition), and the 0x3CF38 chain resolves a hit (+0x7C).
         * A missing 0x35803 call leaves +0x52 at 0x0E and +0x7C at 0. */
        CHECK(s7_saw14, "state-7 slot 0's +0x52 enters the 0x0E no-op");
        CHECK(s7_saw3, "state-7 slot 0's +0x52 returns to 3");
        CHECK(s7_hit, "state-7 the 0x3CF38 chain resolves a hit");
        printf("test_frontend: state-7 last +0x52 change at loop frame %d\n",
               s7_last_change);
        /* The Gate's first claim: the fight reaches the state-7 900-frame timer
         * exit. State 7 is entered at loop 1070 and left at 1970 (the timer's
         * 0x11BCC arm), so its last frame is 1969; a fight that stalls earlier
         * (or never leaves) fails. The dump count above (1381) is the same
         * proof through the presented frames. */
        CHECK_EQ_INT(s7_last, 1969);

        /* The dust descriptors the aligned stream picks. From the state-6
         * entry at FRONTEND_RNG_AFTER_ATTRACT, 0x49388's rng(0x64) draws are
         * 93, 80, 30, 25 in spawn order (P0's two iterations, then draw2, then
         * P1's two) -> 0xC9524 indices 0, 1, 3, 4 through the raw's thresholds
         * (0x493B0..0x493E4). The list is newest-first, so its order is
         * 4, 3, 1, 0; each actor's +8 is its descriptor's first dword
         * (0x2AE7D). A rng(side) picker (always index 4) fails on the second
         * entry. */
        {
            static const u32 order[4] = { 4u, 3u, 1u, 0u };
            for (u32 n = 0; n < 4u; n++) {
                u32 desc = DSD(DS_000C9524 + order[n] * 4u);
                CHECK(dust_actor[n] != 0, "dust entry carries a spawned actor");
                CHECK_EQ_INT((int)dust_anim[n], (int)(DSD(desc) + 2u));
                CHECK_EQ_INT((int)dust_type[n], (int)(0x20u + order[n]));
            }
        }
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
    snprintf(root, sizeof root, "%s",
             (r != NULL && r[0] != '\0') ? r : "/tmp/pr_frontend_det");
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
