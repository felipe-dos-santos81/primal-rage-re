/* port/tests/test_effects.c */
#include "game/effects.h"
#include "game/flow.h"
#include "platform/gfx.h"
#include "mem.h"
#include "test.h"
#include "symbols.h"
#include "test_fixtures.h"

/* Scratch for a source record: mem[] above the heap, the base other tests use. */
#define EFFECTS_TEST_SRC 0x3F00000u

/* Zero the shared source scratch (entry count 0 at +0xC). */
static void effects_reset_source(u32 size)
{
    mem_fill(EFFECTS_TEST_SRC, 0, size);
}

/* Install a one-entry resource table at `tab` whose block is `blk`. */
static void effects_reset_restab(u32 tab, u32 blk)
{
    DSD(DS_001014E0) = tab;
    DSD(DS_001014F0) = 1;
    DSD(tab + 16) = blk;
}

/* Put the resource table back the way effects_reset_restab found it. */
static void effects_restore_restab(u32 saved_tab, u32 saved_n)
{
    DSD(DS_001014E0) = saved_tab;
    DSD(DS_001014F0) = saved_n;
}

/* Seed the six-dword resolved block the scroll tests copy. */
static void effects_seed_block6(u32 blk)
{
    for (int i = 0; i < 6; i++) DSD(blk + 4 + (u32)i * 4u) = 0x40u + (u32)i;
}

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
        /* The three new producers share effect_take_free, so they return 0 on
         * the unbuilt pool without reading source_rec or touching mem[]. */
        CHECK_EQ_INT((int)effects_spawn_darken(EFFECTS_TEST_SRC, 0u), 0);
        CHECK_EQ_INT((int)effects_spawn_pulse(EFFECTS_TEST_SRC, 0u), 0);
        CHECK_EQ_INT((int)effects_spawn_scroll(EFFECTS_TEST_SRC, 0, 1u, 1u), 0);
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
    tf_effects_fixture_begin();

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
        effects_reset_source(0x40u);          /* source record, entry count 0 at +0xC */
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
        effects_reset_restab(tab, blk);
        DSD(blk + 4) = 0x11111111u;
        DSD(blk + 8) = 0x22222222u;
        DSD(blk + 12) = 0x33333333u;
        effects_reset_source(0x40u);
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
            effects_reset_source(0x40u);
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

        effects_restore_restab(saved_tab, saved_n);
    }

    effects_clear();
    CHECK_EQ_INT(effects_active(), 0);

    {
        /* Full pool: 24 records come out, the 25th spawn finds the self-linked
         * free sentinel and must return 0 without touching either list. */
        u32 src = EFFECTS_TEST_SRC;
        tf_effects_fixture_begin();
        effects_reset_source(0x40u);
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
        tf_effects_fixture_begin();
        effects_reset_source(0x40u);          /* source count +0xC == 0 */
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
        tf_effects_fixture_begin();
        effects_reset_source(0x300u);
        DSD(src + 0x00) = 4u;             /* handle = index 0, offset 4 */
        DSD(src + 0x0C) = 2u;
        effects_reset_restab(tab, blk);
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
        effects_restore_restab(saved_tab, saved_n);
    }
    effects_clear();
    CHECK_EQ_INT(effects_active(), 0);

    {
        /* 0x13D4C's copy loop tests the source count signed
         * (0x13D9D mov ebp,[esi+0xc]; test/jle, file 0x66BF1): a high-bit-set
         * count is skipped, not iterated ~4e9 times. The record still builds. */
        u32 src = EFFECTS_TEST_SRC;
        tf_effects_fixture_begin();
        effects_reset_source(0x40u);
        DSD(src + 0x0C) = 0xFFFFFFFFu;
        u32 head = DSD(DS_000FCCE8);
        DSD(head + 0x10) = 0xDDDDDDDDu;
        u32 rec = effects_spawn_darken(src, 0u);
        CHECK(rec != 0, "0x13D4C non-positive count is skipped");
        CHECK_EQ_INT((int)rec, (int)head);
        CHECK_EQ_INT((int)DSD(rec + 0x10), (int)0xDDDDDDDDu);  /* no copy */
    }
    effects_clear();
    CHECK_EQ_INT(effects_active(), 0);

    {
        /* 0x1B544 can fail to resolve. The new producers keep the
         * effects_spawn PORT guard: the resolved copy is skipped rather than
         * dereferencing NULL, while the field writes and the list insert still
         * run. Darken: +0x10 untouched; pulse: white fill still runs; scroll:
         * both blocks untouched. */
        u32 src = EFFECTS_TEST_SRC;
        u32 saved_tab = DSD(DS_001014E0), saved_n = DSD(DS_001014F0);
        tf_effects_fixture_begin();
        effects_reset_source(0x40u);
        DSD(src + 0x00) = 4u;             /* handle = index 0, offset 4 */
        DSD(src + 0x0C) = 2u;
        DSD(DS_001014F0) = 0;             /* res_resolve(index 0) -> NULL */
        u32 head = DSD(DS_000FCCE8);
        DSD(head + 0x10) = 0xDDDDDDDDu;
        u32 rd = effects_spawn_darken(src, 0u);
        CHECK(rd != 0, "0x13D4C unresolved handle takes a record");
        CHECK_EQ_INT((int)rd, (int)head);
        CHECK_EQ_INT((int)DSD(rd + 0x10), (int)0xDDDDDDDDu);
        u32 head2 = DSD(DS_000FCCE8);
        DSD(head2 + 0x410) = 0xEEEEEEEEu;
        u32 rp = effects_spawn_pulse(src, 0u);
        CHECK(rp != 0, "0x13E28 unresolved handle takes a record");
        CHECK_EQ_INT((int)rp, (int)head2);
        CHECK_EQ_INT((int)DSD(rp + 0x10), 0x00FFFFFF);        /* white fill ran */
        CHECK_EQ_INT((int)DSD(rp + 0x410), (int)0xEEEEEEEEu); /* copy skipped */
        u32 head3 = DSD(DS_000FCCE8);
        DSD(head3 + 0x14) = 0xCCCCCCCCu;
        DSD(head3 + 0x414) = 0xBBBBBBBBu;
        u32 rs = effects_spawn_scroll(src, 0, 2u, 1u);
        CHECK(rs != 0, "0x13B3C unresolved handle takes a record");
        CHECK_EQ_INT((int)rs, (int)head3);
        CHECK_EQ_INT((int)DSD(rs + 0x14), (int)0xCCCCCCCCu);
        CHECK_EQ_INT((int)DSD(rs + 0x414), (int)0xBBBBBBBBu);
        effects_restore_restab(saved_tab, saved_n);
    }
    effects_clear();
    CHECK_EQ_INT(effects_active(), 0);

    {
        /* 0x13E28: type 6, +0x0F = 0x80, +0x0E = 1, +0x10 = 0xFFFFFF,
         * +0x410 = the resolved block, count +1. */
        u32 src = EFFECTS_TEST_SRC;
        u32 saved_tab = DSD(DS_001014E0), saved_n = DSD(DS_001014F0);
        u32 tab = src + 0x100u, blk = src + 0x200u, rec;
        tf_effects_fixture_begin();
        effects_reset_source(0x300u);
        DSD(src + 0x00) = 4u;             /* handle = index 0, offset 4 */
        DSD(src + 0x0C) = 2u;
        effects_reset_restab(tab, blk);
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
        effects_restore_restab(saved_tab, saved_n);
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
        tf_effects_fixture_begin();
        effects_reset_source(0x300u);
        DSD(src + 0x00) = 4u;             /* handle = index 0, offset 4 */
        DSD(src + 0x0C) = 257u;
        effects_reset_restab(tab, blk);
        DSD(blk + 4) = 0x11111111u;
        DSD(blk + 8) = 0x22222222u;
        rec = effects_spawn_pulse(src, 1u);
        CHECK(rec != 0, "0x13E28 takes a 257-entry record");
        CHECK_EQ_INT((int)DSD(rec + 0x10), 0x00FFFFFF);
        CHECK_EQ_INT((int)DSD(rec + 0x410), 0x22222222);
        effects_restore_restab(saved_tab, saved_n);
    }
    effects_clear();
    CHECK_EQ_INT(effects_active(), 0);

    {
        /* 0x13B3C: flag != 0 -> type 2, +0x0E = 1; the resolved block is
         * copied into BOTH +0x14 and +0x414, walked by the signed offset. */
        u32 src = EFFECTS_TEST_SRC;
        u32 saved_tab = DSD(DS_001014E0), saved_n = DSD(DS_001014F0);
        u32 tab = src + 0x100u, blk = src + 0x200u, rec;
        tf_effects_fixture_begin();
        effects_reset_source(0x300u);
        DSD(src + 0x0C) = 3u;
        effects_reset_restab(tab, blk);
        effects_seed_block6(blk);
        rec = effects_spawn_scroll(src, 0, 2u, 1u);
        CHECK(rec != 0, "0x13B3C takes a record");
        CHECK_EQ_INT((int)DSB(rec + 0x0C), 2);
        CHECK_EQ_INT((int)DSB(rec + 0x0E), 1);
        CHECK_EQ_INT((int)DSB(rec + 0x0F), 2);
        CHECK_EQ_INT((int)DSB(rec + 0x0D), 1);
        CHECK_EQ_INT((int)DSB(rec + 0x10), 0);
        CHECK_EQ_INT((int)DSD(rec + 0x14), 0x40);
        CHECK_EQ_INT((int)DSD(rec + 0x18), 0x41);
        CHECK_EQ_INT((int)DSD(rec + 0x414), 0x40);
        CHECK_EQ_INT((int)DSD(rec + 0x418), 0x41);
        effects_restore_restab(saved_tab, saved_n);
    }
    effects_clear();
    CHECK_EQ_INT(effects_active(), 0);

    {
        /* 0x13B3C negative-offset arm (0x13B97..0x13BEB): a do-while bounded by
         * the byte count runs count+1 times, so offset -1 / count 2 fills
         * +0x14..+0x1C descending from resolved[4] to resolved[2]. */
        u32 src = EFFECTS_TEST_SRC;
        u32 saved_tab = DSD(DS_001014E0), saved_n = DSD(DS_001014F0);
        u32 tab = src + 0x100u, blk = src + 0x200u, rec;
        tf_effects_fixture_begin();
        effects_reset_source(0x300u);
        effects_reset_restab(tab, blk);
        effects_seed_block6(blk);
        rec = effects_spawn_scroll(src, -1, 2u, 1u);
        CHECK(rec != 0, "0x13B3C negative offset");
        CHECK_EQ_INT((int)DSD(rec + 0x14), 0x41);
        CHECK_EQ_INT((int)DSD(rec + 0x18), 0x42);
        CHECK_EQ_INT((int)DSD(rec + 0x1C), 0x43);
        CHECK_EQ_INT((int)DSD(rec + 0x414), 0x41);
        CHECK_EQ_INT((int)DSD(rec + 0x418), 0x42);
        CHECK_EQ_INT((int)DSD(rec + 0x41C), 0x43);
        effects_clear();
        CHECK_EQ_INT(effects_active(), 0);

        /* offset == -0x80 (0x13B9E cmp eax,-0x80) starts at resolved[count]
         * instead of resolved[1 + count - offset]. */
        tf_effects_fixture_begin();
        effects_reset_source(0x300u);
        effects_reset_restab(tab, blk);
        effects_seed_block6(blk);
        rec = effects_spawn_scroll(src, -0x80, 2u, 1u);
        CHECK(rec != 0, "0x13B3C offset -0x80");
        CHECK_EQ_INT((int)DSD(rec + 0x14), 0);      /* resolved[0] */
        CHECK_EQ_INT((int)DSD(rec + 0x18), 0x40);   /* resolved[1] */
        CHECK_EQ_INT((int)DSD(rec + 0x1C), 0x41);   /* resolved[2] */

        /* The descending arm is a do-while, so count == 0 still writes index 0
         * once: sp = resolved + 1 + 0 - (-1) = resolved[2], stored at +0x14
         * and +0x414. */
        rec = effects_spawn_scroll(src, -1, 0u, 1u);
        CHECK(rec != 0, "0x13B3C negative offset count 0");
        CHECK_EQ_INT((int)DSD(rec + 0x14), 0x41);   /* resolved[2] */
        CHECK_EQ_INT((int)DSD(rec + 0x414), 0x41);

        /* flag == 0 -> type 0 and state byte +0x0E = 0 (the raw truth; such a
         * record never retires, and the raw does not bump DS_0009AF3D). */
        effects_clear();
        tf_effects_fixture_begin();
        effects_reset_source(0x300u);
        effects_reset_restab(tab, blk);
        rec = effects_spawn_scroll(src, 0, 0u, 0u);
        CHECK(rec != 0, "0x13B3C zero flag");
        CHECK_EQ_INT((int)DSB(rec + 0x0C), 0);
        CHECK_EQ_INT((int)DSB(rec + 0x0E), 0);
        CHECK_EQ_INT((int)DSB(rec + 0x0D), 0);
        CHECK_EQ_INT((int)DSB(rec + 0x0F), 0);
        CHECK_EQ_INT(effects_active(), 0);   /* raw never bumps the count */

        /* flag == 0 still copies the resolved block; only the type/state bytes
         * differ from the flag != 0 arm. */
        effects_seed_block6(blk);
        rec = effects_spawn_scroll(src, 0, 2u, 0u);
        CHECK(rec != 0, "0x13B3C zero flag with count 2");
        CHECK_EQ_INT((int)DSB(rec + 0x0C), 0);
        CHECK_EQ_INT((int)DSB(rec + 0x0E), 0);
        CHECK_EQ_INT((int)DSD(rec + 0x14), 0x40);
        CHECK_EQ_INT((int)DSD(rec + 0x18), 0x41);
        CHECK_EQ_INT((int)DSD(rec + 0x414), 0x40);
        CHECK_EQ_INT((int)DSD(rec + 0x418), 0x41);
        effects_restore_restab(saved_tab, saved_n);
    }
    effects_clear();
    CHECK_EQ_INT(effects_active(), 0);

    {
        /* End-to-end: 0x13D4C's type-4 record darkens its +0x10 block toward
         * zero and enqueues it; gfx_flush_palette drains the dirty list into
         * gfx_dac. Each step subtracts 8 from every colour lane, and the DAC
         * reader takes bits 2..7 of each lane, so a 0x40 lane becomes 0x38
         * after one step. */
        u32 src = EFFECTS_TEST_SRC;
        u32 saved_tab = DSD(DS_001014E0), saved_n = DSD(DS_001014F0);
        u32 tab = src + 0x100u, blk = src + 0x200u;
        palette_list_init();
        tf_effects_fixture_begin();
        effects_reset_source(0x300u);
        DSD(src + 0x00) = 4u;             /* handle = index 0, offset 4 */
        DSD(src + 0x08) = 0x40u;          /* first DAC index for the record */
        DSD(src + 0x0C) = 2u;
        effects_reset_restab(tab, blk);
        DSD(blk + 4) = 0x40404040u;
        DSD(blk + 8) = 0x40404040u;
        DSD(blk + 12) = 0x40404040u;
        u32 rec = effects_spawn_darken(src, 1u);
        CHECK(rec != 0, "end-to-end spawn");
        effects_step();                    /* state 1 -> 0, case-4 body runs */
        gfx_flush_palette();
        CHECK_EQ_INT(gfx_dac[0x40][0], 0x38);
        CHECK_EQ_INT(gfx_dac[0x40][1], 0x38);
        CHECK_EQ_INT(gfx_dac[0x40][2], 0x38);
        CHECK_EQ_INT(gfx_dac[0x41][0], 0x38);
        /* Cannot pass vacuously: 0x40 darkens 0x38, 0x30, ..., 0x00 across
         * steps 1..8, then step 9 sees every lane zero and retires the record.
         * The active count returning to 0 proves the step actually ran the
         * case-4 body and removed the record, not that the DAC was never
         * written. */
        for (int i = 0; i < 8; i++) effects_step();
        CHECK_EQ_INT(effects_active(), 0);
        effects_restore_restab(saved_tab, saved_n);
    }
    effects_clear();
    CHECK_EQ_INT(effects_active(), 0);

    {
        /* End-to-end type 6 (0x13E28). The first case-6 body (0x13996) is the
         * flag pass: 0x1399c test cl,cl / 0x139a8 call 0x33734 enqueues the
         * white +0x10 block and clears +0x0F (0x139ad). The next body darkens
         * toward the +0x410 target with the subtract-8 clamp (0x139f0 sub
         * edx,8 / 0x139f7 mov edx,eax) and enqueues at 0x13ab4.
         * gfx_flush_palette takes each lane at bits 2..7 and expands 6 -> 8:
         * white 0x00FFFFFF gives r/g/b = (0xFFFFFF >> 2/10/18) & 0x3F = 0x3F,
         * (0x3F << 2) | (0x3F >> 4) = 0xFF. One darken is 0xFF - 8 = 0xF7:
         * v = (0xF7 >> 2) & 0x3F = 0x3D, (0x3D << 2) | (0x3D >> 4) = 0xF7. */
        u32 src = EFFECTS_TEST_SRC;
        u32 saved_tab = DSD(DS_001014E0), saved_n = DSD(DS_001014F0);
        u32 tab = src + 0x100u, blk = src + 0x200u;
        palette_list_init();
        tf_effects_fixture_begin();
        effects_reset_source(0x300u);
        DSD(src + 0x00) = 4u;             /* handle = index 0, offset 4 */
        DSD(src + 0x08) = 0x40u;          /* first DAC index */
        DSD(src + 0x0C) = 1u;
        effects_reset_restab(tab, blk);
        /* handle 4 shifts the resolved base by 4, so the +0x410 target is
         * blk+8. The darken reads lanes at bits 0/8/16 only, so the target
         * must have a zero high byte (0x00404040) for the full-dword equality
         * test to ever succeed and let the record retire. */
        DSD(blk + 4) = 0x00404040u;
        DSD(blk + 8) = 0x00404040u;
        DSD(blk + 12) = 0x00404040u;
        u32 rec = effects_spawn_pulse(src, 1u);
        CHECK(rec != 0, "end-to-end pulse spawn");
        effects_step();                    /* state 1 -> 0, flag pass */
        gfx_flush_palette();
        CHECK_EQ_INT(gfx_dac[0x40][0], 0xFF);
        CHECK_EQ_INT(gfx_dac[0x40][1], 0xFF);
        CHECK_EQ_INT(gfx_dac[0x40][2], 0xFF);
        effects_step();                    /* darken once: 0xFF -> 0xF7 */
        gfx_flush_palette();
        CHECK_EQ_INT(gfx_dac[0x40][0], 0xF7);
        CHECK_EQ_INT(gfx_dac[0x40][1], 0xF7);
        CHECK_EQ_INT(gfx_dac[0x40][2], 0xF7);
        /* Cannot pass vacuously: 0xFF - 8*24 clamps to the 0x40 target, then
         * the all-equal pass takes the removal arm (0x13a92 test al,al /
         * 0x13a96..0x13aac) and retires the record. */
        for (int i = 0; i < 24; i++) effects_step();
        CHECK_EQ_INT(effects_active(), 0);
        effects_restore_restab(saved_tab, saved_n);
    }
    effects_clear();
    CHECK_EQ_INT(effects_active(), 0);

    {
        /* End-to-end type 2 (0x13B3C, flag != 0). The case-2 body (0x135a8)
         * rotates the +0x14 block by one dword and enqueues it through 0x33734
         * at 0x13abf. The positive-offset arm (0x135f1..0x13629) sets the
         * first DAC index to DSD(src+8) + (s8)offset (0x13627 add eax,esi) and
         * the count to DSB(rec+0x0F) (0x13624 mov dl,[edi+0xf]). gfx_flush
         * expands each 6-bit lane: 0x80 -> (0x80 >> 2) & 0x3F = 0x20 -> 0x82;
         * 0x40 -> (0x40 >> 2) & 0x3F = 0x10 -> 0x41. Step 1 rotates
         * [A,B] -> [B,A], so DAC[0x50] = 0x82 (B) and DAC[0x51] = 0x41 (A);
         * step 2 rotates back. */
        u32 src = EFFECTS_TEST_SRC;
        u32 saved_tab = DSD(DS_001014E0), saved_n = DSD(DS_001014F0);
        u32 tab = src + 0x100u, blk = src + 0x200u;
        palette_list_init();
        tf_effects_fixture_begin();
        effects_reset_source(0x300u);
        /* handle 0 (src+0x00 == 0) so resolved[1] = blk+4 = A. */
        DSD(src + 0x08) = 0x50u;          /* first DAC index */
        DSD(src + 0x0C) = 2u;
        effects_reset_restab(tab, blk);
        DSD(blk + 4) = 0x00000040u;       /* A = resolved[1] */
        DSD(blk + 8) = 0x00000080u;       /* B = resolved[2] */
        u32 rec = effects_spawn_scroll(src, 0, 2u, 1u);
        CHECK(rec != 0, "end-to-end scroll spawn");
        CHECK_EQ_INT((int)DSD(rec + 0x14), 0x40);
        CHECK_EQ_INT((int)DSD(rec + 0x18), 0x80);
        effects_step();                    /* state 1 -> 0, rotate + enqueue */
        gfx_flush_palette();
        CHECK_EQ_INT(gfx_dac[0x50][0], 0x82);   /* rotated-in B */
        CHECK_EQ_INT(gfx_dac[0x51][0], 0x41);   /* rotated A */
        /* Non-vacuous: the drain reset the dirty-list head, and the second
         * step re-rotates and re-drains with the opposite order. */
        CHECK_EQ_INT((int)DSD(DS_00107798), (int)DS_00107498);
        effects_step();
        gfx_flush_palette();
        CHECK_EQ_INT(gfx_dac[0x50][0], 0x41);
        CHECK_EQ_INT(gfx_dac[0x51][0], 0x82);
        effects_restore_restab(saved_tab, saved_n);
    }
    effects_clear();
    CHECK_EQ_INT(effects_active(), 0);

    {
        /* ---- the reachable fight-camera update: 0x1324C ------------------- */
        const u32 s_off = DSD(DS_000F0AF4), s_vel = DSD(DS_000F0AF6);
        const u32 s_e4 = DSD(DS_001088E4), s_d8 = DSD(DS_001088D8);
        const u32 s_dc = DSD(DS_001088DC), s_d4 = DSD(DS_001088D4);
        const u16 s_e0 = DSW(DS_001088E0), s_e2 = DSW(DS_001088E2);
        const u32 s_latch = DSD(DS_000E1C38), s_frame = DSD(DS_000EF6DC);
        const u8 s_bec = DSB(DS_00105BEC), s_bee = DSB(DS_00105BEE);
        const u32 s_tab = DSD(DS_000A8644);
        const u32 s_bt = DSD(DS_00104B00);
        const u32 s_lnext = DSD(DS_00105BCC), s_lprev = DSD(DS_00105BD0);

        /* 0x1324C input A (record 8.7): offset 0x10 + velocity -0x20 settles;
         * the offset zeroes and update-mask bit 0 clears, while the velocity
         * still takes its -0x20 step. */
        DSW(DS_000F0AF4) = 0x10u; DSW(DS_000F0AF6) = 0xFFE0u;
        DSB(DS_00104AE8) = 0x01;
        camera_shake_decay();
        CHECK_EQ_INT((int)DSW(DS_000F0AF4), 0);
        CHECK_EQ_INT((int)DSW(DS_000F0AF6), 0xFFE0);
        CHECK_EQ_INT((int)DSB(DS_00104AE8), 0x00);

        /* 0x1324C input B (record 8.7): a positive velocity does not settle; the
         * offset accumulates and the mask is untouched. */
        DSW(DS_000F0AF4) = 0x100u; DSW(DS_000F0AF6) = 0x20u;
        DSB(DS_00104AE8) = 0x01;
        camera_shake_decay();
        CHECK_EQ_INT((int)DSW(DS_000F0AF4), 0x120);
        CHECK_EQ_INT((int)DSW(DS_000F0AF6), 0);
        CHECK_EQ_INT((int)DSB(DS_00104AE8), 0x01);

        /* 0x1324C is update-table entry 0 and runs only when DS_00104AE8 bit 0
         * is set. The REAL dispatch (game_frame's run_process_table) must leave
         * the shake state alone with the bit clear and run the decay with it
         * set. The active-actor sentinel is self-linked so actors_update walks
         * nothing; every global game_frame touches here is restored below. */
        tf_effects_fixture_begin();
        CHECK(fn_resolve(FN_0001324C) != NULL,
              "0x1324C is registered for the update table");
        DSD(DS_000A8644) = FN_0001324C;      /* entry 0 */
        DSD(DS_00104B00) = 0;                /* skip the state machine */
        DSD(DS_00105BCC) = DS_00105BCC;
        DSD(DS_00105BD0) = DS_00105BCC;
        DSW(DS_000F0AF4) = 0x100u; DSW(DS_000F0AF6) = 0x20u;
        DSB(DS_00104AE8) = 0;
        game_frame();
        CHECK_EQ_INT((int)DSW(DS_000F0AF4), 0x100);  /* bit 0 clear: no run */
        CHECK_EQ_INT((int)DSW(DS_000F0AF6), 0x20);
        DSB(DS_00104AE8) = 1;
        game_frame();
        CHECK_EQ_INT((int)DSW(DS_000F0AF4), 0x120);  /* bit 0 set: ran */
        CHECK_EQ_INT((int)DSW(DS_000F0AF6), 0);

        DSD(DS_000F0AF4) = s_off; DSD(DS_000F0AF6) = s_vel;
        DSB(DS_00104AE8) = 0;
        DSD(DS_001088E4) = s_e4; DSD(DS_001088D8) = s_d8;
        DSD(DS_001088DC) = s_dc; DSD(DS_001088D4) = s_d4;
        DSW(DS_001088E0) = s_e0; DSW(DS_001088E2) = s_e2;
        DSD(DS_000E1C38) = s_latch; DSD(DS_000EF6DC) = s_frame;
        DSB(DS_00105BEC) = s_bec; DSB(DS_00105BEE) = s_bee;
        DSD(DS_000A8644) = s_tab; DSD(DS_00104B00) = s_bt;
        DSD(DS_00105BCC) = s_lnext; DSD(DS_00105BD0) = s_lprev;
    }

    {
        /* The PRESENTED palette, not just the dirty list: a spawned type-4
         * effect darkens its block and the render pass's gfx_flush_palette must
         * write gfx_dac, which is what gfx_present reads. Seed a sentinel that
         * differs from both the expected value and the untouched neighbour, so
         * the check cannot pass vacuously. */
        u32 src = EFFECTS_TEST_SRC;
        u32 saved_tab = DSD(DS_001014E0), saved_n = DSD(DS_001014F0);
        u32 tab = src + 0x100u, blk = src + 0x200u;
        palette_list_init();
        tf_effects_fixture_begin();
        effects_reset_source(0x300u);
        DSD(src + 0x00) = 4u;             /* handle = index 0, offset 4 */
        DSD(src + 0x08) = 0x40u;          /* first DAC index */
        DSD(src + 0x0C) = 2u;
        effects_reset_restab(tab, blk);
        DSD(blk + 4) = 0x40404040u;
        DSD(blk + 8) = 0x40404040u;
        DSD(blk + 12) = 0x40404040u;      /* resolved[2], the count-2 block */
        gfx_dac[0x40][0] = 0xAB; gfx_dac[0x40][1] = 0xAB; gfx_dac[0x40][2] = 0xAB;
        gfx_dac[0x41][0] = 0xCD; gfx_dac[0x41][1] = 0xCD; gfx_dac[0x41][2] = 0xCD;
        gfx_dac[0x50][0] = 0x77;          /* outside the enqueued 0x40..0x41 */
        u32 rec = effects_spawn_darken(src, 1u);
        CHECK(rec != 0, "presented-palette spawn");
        effects_step();
        gfx_flush_palette();
        CHECK_EQ_INT(gfx_dac[0x40][0], 0x38);
        CHECK_EQ_INT(gfx_dac[0x40][1], 0x38);
        CHECK_EQ_INT(gfx_dac[0x40][2], 0x38);
        CHECK_EQ_INT(gfx_dac[0x41][0], 0x38);
        CHECK_EQ_INT(gfx_dac[0x50][0], 0x77);
        effects_clear();
        effects_restore_restab(saved_tab, saved_n);
    }
    CHECK_EQ_INT(effects_active(), 0);

    return g_failures - before;
}
