/* port/tests/test_effects.c */
#include "game/effects.h"
#include "mem.h"
#include "test.h"
#include "symbols.h"

/* Scratch for a source record: mem[] above the heap, the base other tests use. */
#define EFFECTS_TEST_SRC 0x3F00000u

int test_effects(void)
{
    int before = g_failures;

    /* Clearing an uninitialised pool must be a safe no-op, not a walk from the
     * zeroed sentinel through mem[] — the same hazard actors_reset guards. */
    mem_fill(DS_000FCCE0, 0, 8u);
    effects_clear();
    CHECK_EQ_INT(effects_active(), 0);

    /* Building the free list makes records available; an empty list is
     * self-linked, so a spawn succeeds and becomes the one active effect. */
    effects_init();
    {
        u32 src = EFFECTS_TEST_SRC;
        u32 rec;
        mem_fill(src, 0, 0x40u);          /* source record, entry count 0 at +0xC */
        rec = effects_spawn(src, 0x2Au, 0x419786Cu);
        CHECK(rec != 0, "spawn takes a record once the free list is built");
        CHECK_EQ_INT(effects_active(), 1);
        CHECK_EQ_INT((int)DSB(rec + 0x0D), 0x2A);
        CHECK_EQ_INT((int)DSB(rec + 0x0F), 0x80);
        CHECK_EQ_INT((int)DSD(rec + 0x08), (int)src);

        /* The entry count is the SOURCE record's +0xC; each entry is zeroed at
         * +0x10 and copied from the resolved handle's block at +0x410. A fake
         * one-entry resource table keeps this block self-contained. */
        u32 saved_tab = DSD(DS_001014E0), saved_n = DSD(DS_001014F0);
        u32 tab = src + 0x100u;
        u32 blk = src + 0x200u;
        DSD(DS_001014E0) = tab;
        DSD(DS_001014F0) = 1;
        DSD(tab + 16) = blk;
        DSD(blk + 4) = 0x11111111u;
        DSD(blk + 8) = 0x22222222u;
        DSD(blk + 12) = 0x33333333u;
        mem_fill(src, 0, 0x40u);
        DSD(src + 0x0C) = 3u;
        rec = effects_spawn(src, 0x2Bu, 0u);    /* res_handle(0, 0) */
        CHECK(rec != 0, "spawn with three entries");
        CHECK_EQ_INT(effects_active(), 2);
        CHECK_EQ_INT((int)DSD(rec + 0x10), 0);
        CHECK_EQ_INT((int)DSD(rec + 0x14), 0);
        CHECK_EQ_INT((int)DSD(rec + 0x18), 0);
        CHECK_EQ_INT((int)DSD(rec + 0x410), 0x11111111);
        CHECK_EQ_INT((int)DSD(rec + 0x414), 0x22222222);
        CHECK_EQ_INT((int)DSD(rec + 0x418), 0x33333333);
        DSD(DS_001014E0) = saved_tab;
        DSD(DS_001014F0) = saved_n;
    }

    /* Clear returns the pool to empty and zeroes the active count, which is the
     * flag 0x121A0's phase-2 exit tests. */
    effects_clear();
    CHECK_EQ_INT(effects_active(), 0);

    /* A second clear on an already-empty pool must not walk or corrupt the
     * self-linked sentinels. */
    effects_clear();
    CHECK_EQ_INT(effects_active(), 0);

    return g_failures - before;
}
