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

    /* A spawn before effects_init must not walk mem[]: with the free sentinel
     * zeroed, rec reads as 0 and list_unlink(0) would write mem[0]/mem[4] and
     * then link the phantom record onto the active list. Mirrors the unbuilt-pool
     * guard effects_clear has; the count stays 0 and neither list is touched. */
    mem_fill(DS_000FCCE0, 0, 12u);
    mem_fill(DS_0009AF3D, 0, 1u);
    {
        u32 m0 = DSD(0), m4 = DSD(4);
        CHECK_EQ_INT((int)effects_spawn(EFFECTS_TEST_SRC, 0u, 0u), 0);
        CHECK_EQ_INT(effects_active(), 0);
        CHECK_EQ_INT((int)DSD(DS_000FCCE8), 0);
        CHECK_EQ_INT((int)DSD(0), (int)m0);
        CHECK_EQ_INT((int)DSD(4), (int)m4);
    }

    /* Clearing an uninitialised pool must be a safe no-op, not a walk from the
     * zeroed sentinel through mem[] — the same hazard actors_reset guards. */
    mem_fill(DS_000FCCE0, 0, 8u);
    effects_clear();
    CHECK_EQ_INT(effects_active(), 0);

    /* Building the free list makes records available; an empty list is
     * self-linked, so a spawn succeeds and becomes the one active effect. */
    effects_init();

    /* 0x13ADC link direction: both sentinels self-linked; 0x249C0 appends each
     * record before the free sentinel, so the free list runs pool order
     * (0xF0B00 next, 0xFC4CC last) and both ends point at the sentinel. */
    CHECK_EQ_INT((int)DSD(DS_000FCCE0), (int)DS_000FCCE0);
    CHECK_EQ_INT((int)DSD(DS_000FCCE4), (int)DS_000FCCE0);
    CHECK_EQ_INT((int)DSD(DS_000FCCE8), (int)DS_000F0B00);
    CHECK_EQ_INT((int)DSD(DS_000F0B00 + 4), (int)DS_000FCCE8);
    CHECK_EQ_INT((int)DSD(DS_000F0B00), (int)(DS_000F0B00 + EFFECTS_REC_SIZE));
    CHECK_EQ_INT((int)DSD(DS_000FCCE8 + 4), (int)(DS_000FCCE0 - EFFECTS_REC_SIZE));
    CHECK_EQ_INT((int)DSD(DS_000FCCE0 - EFFECTS_REC_SIZE), (int)DS_000FCCE8);

    {
        u32 src = EFFECTS_TEST_SRC;
        u32 rec;
        mem_fill(src, 0, 0x40u);          /* source record, entry count 0 at +0xC */
        rec = effects_spawn(src, 0x2Au, 0x419786Cu);
        CHECK(rec != 0, "spawn takes a record once the free list is built");
        CHECK_EQ_INT(effects_active(), 1);
        CHECK_EQ_INT((int)rec, (int)DS_000F0B00);   /* pops the free-list head */
        CHECK_EQ_INT((int)DSB(rec + 0x0D), 0x2A);
        CHECK_EQ_INT((int)DSB(rec + 0x0F), 0x80);
        CHECK_EQ_INT((int)DSD(rec + 0x08), (int)src);
        /* 0x249B0 head-inserts after the active sentinel: rec[0]=next=active,
         * rec[4]=prev=active, and the sentinel's next/prev both front rec. */
        CHECK_EQ_INT((int)DSD(DS_000FCCE0), (int)rec);
        CHECK_EQ_INT((int)DSD(DS_000FCCE4), (int)rec);
        CHECK_EQ_INT((int)DSD(rec), (int)DS_000FCCE0);
        CHECK_EQ_INT((int)DSD(rec + 4), (int)DS_000FCCE0);
        CHECK_EQ_INT((int)DSD(DS_000FCCE8), (int)(DS_000F0B00 + EFFECTS_REC_SIZE));
        /* A second spawn distinguishes insert-AFTER from insert-BEFORE: the new
         * record must front the sentinel and point back at the first, not land
         * behind it. */
        u32 rec2 = effects_spawn(src, 0u, 0u);
        CHECK(rec2 != 0, "second spawn");
        CHECK_EQ_INT(effects_active(), 2);
        CHECK_EQ_INT((int)rec2, (int)(DS_000F0B00 + EFFECTS_REC_SIZE));
        CHECK_EQ_INT((int)DSD(DS_000FCCE0), (int)rec2);   /* sentinel.next = head */
        CHECK_EQ_INT((int)DSD(DS_000FCCE4), (int)rec);    /* sentinel.prev = tail */
        CHECK_EQ_INT((int)DSD(rec2), (int)rec);           /* rec2.next = rec */
        CHECK_EQ_INT((int)DSD(rec2 + 4), (int)DS_000FCCE0);
        CHECK_EQ_INT((int)DSD(rec + 4), (int)rec2);       /* rec.prev = rec2 */
        CHECK_EQ_INT((int)DSD(rec), (int)DS_000FCCE0);
    }

    /* Clear returns the pool to empty and zeroes the active count, which is the
     * flag 0x121A0's phase-2 exit tests. */
    effects_clear();
    CHECK_EQ_INT(effects_active(), 0);
    CHECK_EQ_INT((int)DSD(DS_000FCCE0), (int)DS_000FCCE0);
    CHECK_EQ_INT((int)DSD(DS_000FCCE8), (int)DS_000F0B00);   /* record returned */
    CHECK_EQ_INT((int)DSD(DS_000F0B00 + 4), (int)DS_000FCCE8);
    CHECK_EQ_INT((int)DSD(DS_000F0B00), (int)(DS_000F0B00 + EFFECTS_REC_SIZE));

    {
        /* The entry count is the SOURCE record's +0xC; each entry is zeroed at
         * +0x10 and copied from the resolved handle's block at +0x410. A fake
         * one-entry resource table keeps this block self-contained. */
        u32 src = EFFECTS_TEST_SRC;
        u32 saved_tab = DSD(DS_001014E0), saved_n = DSD(DS_001014F0);
        u32 tab = src + 0x100u;
        u32 blk = src + 0x200u;
        u32 head = DSD(DS_000FCCE8);
        u32 rec;
        DSD(DS_001014E0) = tab;
        DSD(DS_001014F0) = 1;
        DSD(tab + 16) = blk;
        DSD(blk + 4) = 0x11111111u;
        DSD(blk + 8) = 0x22222222u;
        DSD(blk + 12) = 0x33333333u;
        mem_fill(src, 0, 0x40u);
        DSD(src + 0x0C) = 3u;
        /* Dirty the record's entry tables, including one slot PAST the count,
         * so the count-bounded zero/copy loops are proven, not vacuous. */
        DSD(head + 0x10) = 0xAAAAAAAAu;
        DSD(head + 0x14) = 0xAAAAAAABu;
        DSD(head + 0x18) = 0xAAAAAAACu;
        DSD(head + 0x1C) = 0xAAAAAAADu;
        DSD(head + 0x410) = 0xBBBBBBBBu;
        DSD(head + 0x414) = 0xBBBBBBBCu;
        DSD(head + 0x418) = 0xBBBBBBBDu;
        DSD(head + 0x41C) = 0xBBBBBBBEu;
        rec = effects_spawn(src, 0x2Bu, 0u);    /* res_handle(0, 0) */
        CHECK(rec != 0, "spawn with three entries");
        CHECK_EQ_INT((int)rec, (int)head);
        CHECK_EQ_INT(effects_active(), 1);
        CHECK_EQ_INT((int)DSB(rec + 0x0D), 0x2B);
        CHECK_EQ_INT((int)DSD(rec + 0x10), 0);
        CHECK_EQ_INT((int)DSD(rec + 0x14), 0);
        CHECK_EQ_INT((int)DSD(rec + 0x18), 0);
        CHECK_EQ_INT((int)DSD(rec + 0x1C), (int)0xAAAAAAADu);   /* past count kept */
        CHECK_EQ_INT((int)DSD(rec + 0x410), 0x11111111);
        CHECK_EQ_INT((int)DSD(rec + 0x414), 0x22222222);
        CHECK_EQ_INT((int)DSD(rec + 0x418), 0x33333333);
        CHECK_EQ_INT((int)DSD(rec + 0x41C), (int)0xBBBBBBBEu);  /* past count kept */
        /* active head insert again, with the record as the only active node. */
        CHECK_EQ_INT((int)DSD(DS_000FCCE0), (int)rec);
        CHECK_EQ_INT((int)DSD(DS_000FCCE4), (int)rec);
        CHECK_EQ_INT((int)DSD(rec), (int)DS_000FCCE0);
        CHECK_EQ_INT((int)DSD(rec + 4), (int)DS_000FCCE0);

        /* 0x13C70 tests the count signed (`test`/`jle`): a high-bit-set count is
         * non-positive and must be skipped, not iterated ~4e9 times. */
        {
            u32 rec3 = DSD(DS_000FCCE8);
            mem_fill(src, 0, 0x40u);
            DSD(src + 0x0C) = 0xFFFFFFFFu;
            u32 rneg = effects_spawn(src, 0u, 0u);
            CHECK(rneg != 0, "non-positive count is skipped");
            CHECK_EQ_INT((int)rneg, (int)rec3);
            CHECK_EQ_INT(effects_active(), 2);
        }

        /* 0x1B544 can fail to resolve. A positive count must not dereference the
         * NULL: the +0x10 zeroing still runs, the +0x410 copy is skipped. */
        {
            u32 rec4 = DSD(DS_000FCCE8);
            DSD(DS_001014F0) = 0;               /* res_resolve(index 0) -> NULL */
            DSD(src + 0x0C) = 3u;
            DSD(rec4 + 0x410) = 0xCCCCCCCCu;
            u32 rnull = effects_spawn(src, 0u, 0u);
            CHECK(rnull != 0, "spawn with an unresolved handle");
            CHECK_EQ_INT((int)rnull, (int)rec4);
            CHECK_EQ_INT((int)DSD(rnull + 0x10), 0);
            CHECK_EQ_INT((int)DSD(rnull + 0x410), (int)0xCCCCCCCCu);
            CHECK_EQ_INT(effects_active(), 3);
            DSD(DS_001014F0) = 1;
        }

        DSD(DS_001014E0) = saved_tab;
        DSD(DS_001014F0) = saved_n;
    }

    effects_clear();
    CHECK_EQ_INT(effects_active(), 0);

    {
        /* Full pool: 24 records come out, the 25th spawn finds the self-linked
         * free sentinel and must return 0 without touching either list. */
        u32 src = EFFECTS_TEST_SRC;
        effects_init();
        mem_fill(src, 0, 0x40u);
        for (u32 i = 0; i < 24u; i++)
            CHECK(effects_spawn(src, 0u, 0u) != 0, "pool has 24 records");
        CHECK_EQ_INT(effects_active(), 24);
        u32 ah = DSD(DS_000FCCE0), fh = DSD(DS_000FCCE8);
        CHECK_EQ_INT((int)fh, (int)DS_000FCCE8);   /* free list empty */
        CHECK_EQ_INT((int)effects_spawn(src, 0u, 0u), 0);
        CHECK_EQ_INT(effects_active(), 24);
        CHECK_EQ_INT((int)DSD(DS_000FCCE0), (int)ah);
        CHECK_EQ_INT((int)DSD(DS_000FCCE8), (int)fh);
    }

    effects_clear();
    CHECK_EQ_INT(effects_active(), 0);

    /* A second clear on an already-empty pool must not walk or corrupt the
     * self-linked sentinels. */
    effects_clear();
    CHECK_EQ_INT(effects_active(), 0);

    {
        /* 0x134C0 drains the list the spawn grew. The record's age counter
         * (rec+0xE) counts down from the byte arg stored at rec+0xD; the title
         * spawn passes 3 (flow.c 0x123EA), so the counter wraps every third
         * step and the type-3 body runs on steps 1 and 4. The first body is the
         * flag(0x80) palette pass; the second animates +0x10 toward +0x410 and,
         * with the source count (+0xC) zero, finds them equal immediately and
         * tears the record down. Four steps is exactly the raw's lifetime. */
        u32 src = EFFECTS_TEST_SRC;
        effects_init();
        mem_fill(src, 0, 0x40u);          /* source count +0xC == 0 */
        u32 rec = effects_spawn(src, 3u, 0u);
        CHECK_EQ_INT(effects_active(), 1);
        CHECK_EQ_INT((int)rec, (int)DS_000F0B00);
        effects_step();
        CHECK_EQ_INT(effects_active(), 1);   /* wrap 1: flag palette pass */
        effects_step();
        CHECK_EQ_INT(effects_active(), 1);
        effects_step();
        CHECK_EQ_INT(effects_active(), 1);
        effects_step();
        CHECK_EQ_INT(effects_active(), 0);   /* wrap 2: teardown drains it */

        /* The teardown unlinked from active and re-linked to the free list
         * exactly once: the active sentinel is self-linked and the record
         * appears in the free walk exactly once, with all 24 records back. */
        CHECK_EQ_INT((int)DSD(DS_000FCCE0), (int)DS_000FCCE0);
        int seen = 0, n = 0;
        for (u32 p = DSD(DS_000FCCE8); p != DS_000FCCE8; p = DSD(p)) {
            if (p == rec) seen++;
            n++;
        }
        CHECK_EQ_INT(seen, 1);
        CHECK_EQ_INT(n, 24);
    }

    {
        /* 0x13D4C: type 4, +0x0F = 0, +0x0E = 1, +0x10 = the resolved block's
         * dwords (no +0x410 fill), active count +1. */
        u32 src = EFFECTS_TEST_SRC;
        u32 saved_tab = DSD(DS_001014E0), saved_n = DSD(DS_001014F0);
        u32 tab = src + 0x100u, blk = src + 0x200u, rec;
        effects_init();
        mem_fill(src, 0, 0x300u);
        DSD(src + 0x00) = 4u;             /* handle = index 0, offset 4 */
        DSD(src + 0x0C) = 2u;
        DSD(DS_001014E0) = tab;
        DSD(DS_001014F0) = 1;
        DSD(tab + 16) = blk;
        DSD(blk + 4) = 0x40404040u;
        DSD(blk + 8) = 0x00707070u;
        DSD(blk + 12) = 0x0A0B0C0Du;
        rec = effects_spawn_darken(src, 0x2Cu);
        CHECK(rec != 0, "0x13D4C takes a record");
        CHECK_EQ_INT(effects_active(), 1);
        CHECK_EQ_INT((int)DSB(rec + 0x0C), 4);
        CHECK_EQ_INT((int)DSB(rec + 0x0F), 0);
        CHECK_EQ_INT((int)DSB(rec + 0x0E), 1);
        CHECK_EQ_INT((int)DSD(rec + 0x08), (int)src);
        CHECK_EQ_INT((int)DSB(rec + 0x0D), 0x2C);
        /* Handles beyond offset 0 prove the handle came from DSD(source_rec):
         * with offset 0 these would be blk+4/blk+8 instead. */
        CHECK_EQ_INT((int)DSD(rec + 0x10), 0x00707070);
        CHECK_EQ_INT((int)DSD(rec + 0x14), 0x0A0B0C0D);
        /* The record front-inserts into the active list like 0x13C70. */
        CHECK_EQ_INT((int)DSD(DS_000FCCE0), (int)rec);
        CHECK_EQ_INT((int)DSD(rec), (int)DS_000FCCE0);
        DSD(DS_001014E0) = saved_tab;
        DSD(DS_001014F0) = saved_n;
    }
    effects_clear();
    CHECK_EQ_INT(effects_active(), 0);

    {
        /* 0x13E28: type 6, +0x0F = 0x80, +0x0E = 1, +0x10 = 0xFFFFFF,
         * +0x410 = the resolved block, count +1. */
        u32 src = EFFECTS_TEST_SRC;
        u32 saved_tab = DSD(DS_001014E0), saved_n = DSD(DS_001014F0);
        u32 tab = src + 0x100u, blk = src + 0x200u, rec;
        effects_init();
        mem_fill(src, 0, 0x300u);
        DSD(src + 0x00) = 4u;             /* handle = index 0, offset 4 */
        DSD(src + 0x0C) = 2u;
        DSD(DS_001014E0) = tab;
        DSD(DS_001014F0) = 1;
        DSD(tab + 16) = blk;
        DSD(blk + 4) = 0x00102030u;
        DSD(blk + 8) = 0x00040506u;
        DSD(blk + 12) = 0x00070809u;
        rec = effects_spawn_pulse(src, 1u);
        CHECK(rec != 0, "0x13E28 takes a record");
        CHECK_EQ_INT(effects_active(), 1);
        CHECK_EQ_INT((int)DSB(rec + 0x0C), 6);
        CHECK_EQ_INT((int)DSB(rec + 0x0F), 0x80);
        CHECK_EQ_INT((int)DSB(rec + 0x0E), 1);
        CHECK_EQ_INT((int)DSD(rec + 0x08), (int)src);
        CHECK_EQ_INT((int)DSD(rec + 0x10), 0x00FFFFFF);
        CHECK_EQ_INT((int)DSD(rec + 0x14), 0x00FFFFFF);
        CHECK_EQ_INT((int)DSD(rec + 0x410), 0x00040506);
        CHECK_EQ_INT((int)DSD(rec + 0x414), 0x00070809);
        DSD(DS_001014E0) = saved_tab;
        DSD(DS_001014F0) = saved_n;
    }
    effects_clear();
    CHECK_EQ_INT(effects_active(), 0);
    {
        /* 0x13E28 at count == 257: the +0x10 white fill (i = 256 -> +0x410) and
         * the +0x410 resolved copy (j = 0 -> +0x410) overlap. The raw runs the
         * whole white loop before the resolved loop, so +0x410 ends as the
         * resolved value; a merged loop would leave 0x00FFFFFF there. */
        u32 src = EFFECTS_TEST_SRC;
        u32 saved_tab = DSD(DS_001014E0), saved_n = DSD(DS_001014F0);
        u32 tab = src + 0x100u, blk = src + 0x200u, rec;
        effects_init();
        mem_fill(src, 0, 0x300u);
        DSD(src + 0x00) = 4u;             /* handle = index 0, offset 4 */
        DSD(src + 0x0C) = 257u;
        DSD(DS_001014E0) = tab;
        DSD(DS_001014F0) = 1;
        DSD(tab + 16) = blk;
        DSD(blk + 4) = 0x11111111u;
        DSD(blk + 8) = 0x22222222u;
        rec = effects_spawn_pulse(src, 1u);
        CHECK(rec != 0, "0x13E28 takes a 257-entry record");
        CHECK_EQ_INT((int)DSD(rec + 0x10), 0x00FFFFFF);
        CHECK_EQ_INT((int)DSD(rec + 0x410), 0x22222222);
        DSD(DS_001014E0) = saved_tab;
        DSD(DS_001014F0) = saved_n;
    }
    effects_clear();
    CHECK_EQ_INT(effects_active(), 0);

    return g_failures - before;
}
