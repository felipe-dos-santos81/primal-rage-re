/* Port of the 0x13xxx effect list: the free/active pools, the spawn (0x13C70),
 * the free-list build (0x13ADC), the clear (0x13DF0) and the per-entry teardown
 * (0x13420) over the two intrusive list primitives (0x249B0/0x249C0/0x249D0).
 * The lists are {next@+0; prev@+4} dwords holding mem[] offsets; the sentinels
 * are DS_000FCCE0 (active) and DS_000FCCE8 (free) and point at themselves when
 * empty. 24 records of stride 0x814 run DS_000F0B00..0xFC4CC. PORT: the
 * DS_0009AF3C interrupt lock is written for fidelity but is inert in the port's
 * single-threaded loop. */
#include "game/effects.h"
#include "../mem.h"
#include "../symbols.h"
#include "platform/gfx.h"
#include "platform/res.h"

/* 0x7CCF0 -> linear 0xFCCF0, the raw palette buffer 0x13420's case 4 enqueues
 * from. Not a named global in symbols.h. */
#define EFFECTS_PALETTE_BUF 0x000FCCF0u

/* ---- the splice lists (0x249B0/0x249C0/0x249D0) ------------------------- */

/* 0x249B0: insert rec immediately after `at`. */
static void list_insert_after(u32 at, u32 rec)
{
    u32 next = DSD(at);
    DSD(at) = rec;
    DSD(rec) = next;
    DSD(rec + 4) = at;
    DSD(next + 4) = rec;
}

/* 0x249C0: insert rec immediately before `at`. */
static void list_insert_before(u32 at, u32 rec)
{
    u32 prev = DSD(at + 4);
    DSD(at + 4) = rec;
    DSD(rec) = at;
    DSD(rec + 4) = prev;
    DSD(prev) = rec;
}

/* 0x249D0: unlink rec and zero its two link fields. */
static void list_unlink(u32 rec)
{
    u32 prev = DSD(rec + 4);
    DSD(DSD(rec) + 4) = prev;
    DSD(prev) = DSD(rec);
    DSD(rec + 4) = 0;
    DSD(rec) = 0;
}

/* 0x13420. Unlink `rec` from the active list, run its type dispatch, then push
 * it at the front of the free list (0x249B0 insert-after). The dispatch maps the
 * record's type to a palette dirty-list append: types 0/2/3/5 use the source
 * record's own fields through 0x33714 (flag 1), type 1 the record's +0x10 block
 * through 0x33734, type 4 the raw 0xFCCF0 buffer. PORT: 0x33714 is 0x33734 with
 * the flag byte set, so both are palette_record() with the flag argument. */
static void effect_teardown(u32 rec)
{
    u8 saved = DSB(DS_0009AF3C);
    DSB(DS_0009AF3C) = 1;
    list_unlink(rec);
    DSB(DS_0009AF3C) = saved;

    u32 src = DSD(rec + 8);
    switch (DSB(rec + 0x0c)) {
    case 0: case 2: case 3: case 5:
        palette_record(DSD(src), DSD(src + 8), DSD(src + 0x0c), 1);
        break;
    case 1:
        palette_record(rec + 0x10, DSD(src + 8) + (u32)DSB(rec + 0x0f), 1, 0);
        break;
    case 4:
        palette_record(EFFECTS_PALETTE_BUF, DSD(src + 8), DSD(src + 0x0c), 0);
        break;
    default:
        break;
    }

    list_insert_after(DS_000FCCE8, rec);
}

/* ---- exported ----------------------------------------------------------- */

/* 0x13ADC. */
void effects_init(void)
{
    DSB(DS_0009AF3C) = 1;
    DSD(DS_000FCCE4) = DS_000FCCE0;
    DSD(DS_000FCCE0) = DS_000FCCE0;
    DSD(DS_000FCCEC) = DS_000FCCE8;
    DSD(DS_000FCCE8) = DS_000FCCE8;
    for (u32 rec = DS_000F0B00; rec < DS_000FCCE0; rec += EFFECTS_REC_SIZE)
        list_insert_before(DS_000FCCE8, rec);
    DSB(DS_0009AF3C) = 0;
}

/* 0x13C70. */
u32 effects_spawn(u32 source_rec, u32 byte_arg, u32 handle)
{
    u32 rec = DSD(DS_000FCCE8);
    if (rec == DS_000FCCE8) return 0;       /* empty free list */
    u8 saved = DSB(DS_0009AF3C);
    DSB(DS_0009AF3C) = 1;
    list_unlink(rec);
    DSB(DS_0009AF3C) = saved;

    const u32 *resolved = (const u32 *)res_resolve(handle);
    DSB(rec + 0x0f) = 0x80;
    DSB(rec + 0x0c) = 3;
    DSD(rec + 8) = source_rec;
    DSB(rec + 0x0d) = (u8)byte_arg;

    /* The iteration bound is the SOURCE record's +0xC, read before any entry is
     * written; +0x10 holds zeros and +0x410 the resolved block's dwords. */
    u32 count = DSD(source_rec + 0x0c);
    for (u32 i = 0; i < count; i++)
        DSD(rec + 0x10 + i * 4u) = 0;
    for (u32 i = 0; i < count; i++)
        DSD(rec + 0x410 + i * 4u) = resolved[1 + i];

    DSB(DS_0009AF3C) = 1;
    DSB(rec + 0x0e) = 1;
    list_insert_after(DS_000FCCE0, rec);
    DSB(DS_0009AF3D) = (u8)(DSB(DS_0009AF3D) + 1);
    DSB(DS_0009AF3C) = 0;
    return rec;
}

/* 0x13DF0. */
void effects_clear(void)
{
    /* PORT: the original assumes the pool was built; without it the sentinel is
     * zeroed and the walk would start at mem[0]. A built list always has a
     * non-zero active next (itself when empty, a record otherwise). Mirror
     * actors_reset's pool guard. */
    if (DSD(DS_000FCCE0) == 0) return;
    DSB(DS_0009AF3C) = 1;
    u32 node = DSD(DS_000FCCE0);
    while (node != DS_000FCCE0) {
        u32 next = DSD(node);
        effect_teardown(node);
        node = next;
    }
    DSB(DS_0009AF3D) = 0;
    DSB(DS_0009AF3C) = 0;
}

/* DS_0009AF3D. */
int effects_active(void) { return (int)DSB(DS_0009AF3D); }
