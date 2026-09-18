/* Port of the actor record pool, its free/active lists and the state-begin
 * reset. Original addresses are named in comments:
 *   0x249B0 insert-after, 0x249C0 insert-before, 0x249D0 unlink,
 *   0x2AC80 alloc, 0x2BAF4 state-begin reset, 0x2AD40 release.
 * The lists are {next@+0; prev@+4} dwords holding mem[] offsets; the sentinels
 * are the mem[] offsets DS_00105B3C (free) and DS_00105BCC (active) and point at
 * themselves when the list is empty. The two pools are allocated by
 * res_load_index (platform/res.c), so actors_init only validates them. */
#include "game/actors.h"
#include "../mem.h"
#include "../symbols.h"
#include "platform/gfx.h"
#include "platform/render.h"

/* ---- the two splice lists (0x249B0/0x249C0/0x249D0) -------------------- */

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

/* 0x249D0: unlink rec. */
static void list_unlink(u32 rec)
{
    u32 prev = DSD(rec + 4);
    DSD(DSD(rec) + 4) = prev;
    DSD(prev) = DSD(rec);
    DSD(rec + 4) = 0;
    DSD(rec) = 0;
}

/* PORT: a list whose head is its own sentinel is empty; report that as 0. */
static u32 list_head(u32 sentinel)
{
    u32 next = DSD(sentinel);
    return next == sentinel ? 0u : next;
}

/* ---- pool geometry ------------------------------------------------------ */

static u32 pool_base(void) { return DSD(DS_001014F4); }

/* PORT: the spec §7 pool invariant — an offset outside [base, base+0xEBA0) or
 * not 0x68-aligned is not a record. */
static int in_pool(u32 rec)
{
    u32 base = pool_base();
    if (base == 0 || rec < base) return 0;
    if (rec >= base + ACTOR_POOL_RECORDS * ACTOR_REC_SIZE) return 0;
    return ((rec - base) % ACTOR_REC_SIZE) == 0;
}

/* ---- exported ----------------------------------------------------------- */

/* PORT: validates the two pools res_load_index already allocated. The offsets
 * are pointer-valued mem[] offsets, so consume them as mem + DSD(...). */
int actors_init(void)
{
    u32 pool = DSD(DS_001014F4);
    u32 pset = DSD(DS_001014EC);
    if (pool == 0 || !mem_in_range(pool, ACTOR_POOL_RECORDS * ACTOR_REC_SIZE))
        return 0;
    if (pset == 0 || !mem_in_range(pset, 0x4880u))
        return 0;
    return 1;
}

/* 0x2BAF4. The title entry (0x121E4) calls it with eax = 1, so this mirrors the
 * param_1 != 0 arm and skips the param_1 == 0 back-buffer copy. */
void actors_reset(void)
{
    /* PORT: the original assumes its loader ran; without the pool the free-list
     * rebuild would walk mem[0..0xEBA0]. Return when actors_init would fail. */
    if (pool_base() == 0) return;
    /* 0x2EA30(0x2700)/0x2EA30(): the original's interrupt-lock nesting counter
     * DAT_000BCD60. PORT: inert in the port's single-threaded loop; nothing
     * reads the counter. */
    DSD(DS_00104AE8) = 0;       /* process mask: update table */
    DSD(DS_00104AEC) = 0;       /* process mask: render table */
    DSD(DS_00100B4C) = 0;
    DSD(DS_00104AD0) = 0;
    /* PORT: 0x13DF0 frees the 0x13xxx effect-list at sentinels DS_000FCCE0/E8,
     * a subsystem (0x13ADC/0x13B3C) no task in this cycle reaches. */
    DSW(DS_00105BEA) = 0;
    DSB(DS_00105BED) = 0;
    mem_fill(DSD(DS_001014EC), 0, 0x4880u);   /* 0x61A70: pset pool */
    mem_fill(DSD(DS_001014F4), 0, 0xEBA0u);   /* 0x61A70: record pool */
    mem_fill(DS_00105B4C, 0, 0x80u);          /* 0x61A70: DS_00105B4C..BCC */
    DSD(DS_00105BD0) = DS_00105BCC;
    DSD(DS_00105BCC) = DS_00105BCC;
    DSD(DS_00105B40) = DS_00105B3C;
    DSD(DS_00105B3C) = DS_00105B3C;
    /* PORT: rebuild the free list in pool order; 0x249C0 appends at the tail so
     * the head is the pool base and allocation runs base, base+0x68, ... */
    u32 base = DSD(DS_001014F4);
    for (u32 rec = base; rec < base + ACTOR_POOL_RECORDS * ACTOR_REC_SIZE;
         rec += ACTOR_REC_SIZE)
        list_insert_before(DS_00105B3C, rec);
    /* PORT: 0x13ADC re-inits the 0x13xxx effect-list free pool at
     * DS_000FCCE8, out of scope for this cycle. */
    /* PORT: 0x4F228(0,0) zeroes the input/mouse state at DS_00107A38/3A and
     * DS_00107A54/55; input state is owned by platform/input.c. */
    DSD(DS_00105B44) = 0;
    DSD(DS_00105B48) = 0;
    render_list_init();                             /* 0x1C350 */
    /* PORT: 0x38B70 zeroes the mouse/input arrays at DS_00107A00/DS_00107A1C;
     * input state is owned by platform/input.c. */
    mem_fill(DS_00105F38, 0, 0x14D4u);              /* 0x2F920 */
    /* 0x2BAF4's param_1 != 0 arm: 0x52106 clears both offscreen buffers and
     * zeroes the tick counters. PORT: its VGA DAC clear is palette_list_init's
     * gfx_dac memset and its literal 0xA0000 aperture clear is never performed
     * (the port's memory map aliases that address to engine tables). The
     * param_1 == 0 arm (copy DS_000E87A0 into DS_000E87A4) is unreachable from
     * the title path and is not transcribed. */
    DSD(DS_00101508) = 0;
    DSD(DS_0010150C) = 0;
    mem_fill(DSD(DS_001014E8), 0, 0xFA00u);
    mem_fill(DSD(DS_001014E4), 0, 0xFA00u);
    palette_list_init();                            /* 0x336C0 */
    DSB(DS_00105BED) = 1;
    /* PORT: 0x2EA30() restores the lock state saved before the counter zeroing;
     * inert as above. */
}

/* 0x2AC80: pop the free-list head and link it at the head of the active list.
 * The spawn flag's 0x400 bit selects the tail instead (0x249C0); the title's
 * spawn passes 0, so this is the head insert. */
u32 actor_alloc(void)
{
    if (list_head(DS_00105B3C) == 0) return 0;
    u32 rec = DSD(DS_00105B3C);
    list_unlink(rec);
    list_insert_after(DS_00105BCC, rec);
    return rec;
}

/* 0x2AD40's list move: unlink the record from the active list and push it at
 * the free-list head (0x249B0). Its dead-bit and child-refcount housekeeping is
 * release logic owned by the spawn task. An offset outside the pool or not
 * 0x68-aligned is ignored, so a transcription bug leaks instead of corrupting
 * the heap (spec §7). */
void actor_free(u32 rec)
{
    if (!in_pool(rec)) return;
    /* PORT: not linked into any list (the original trusts the caller). 0x249D0
     * on such an offset would write mem[0]/mem[4]; skip it so a bad call leaks
     * instead of corrupting the low image. */
    if (DSD(rec) == 0 && DSD(rec + 4) == 0) return;
    list_unlink(rec);
    list_insert_after(DS_00105B3C, rec);
}

/* PORT: pool index -> record offset, or 0 past the end. */
u32 actor_record(u32 index)
{
    u32 base = pool_base();
    if (base == 0 || index >= ACTOR_POOL_RECORDS) return 0;
    return base + index * ACTOR_REC_SIZE;
}

/* PORT: record offset -> pool index, or -1u when it is not a record. */
u32 actor_index(u32 rec)
{
    if (!in_pool(rec)) return 0xFFFFFFFFu;
    return (rec - pool_base()) / ACTOR_REC_SIZE;
}

/* PORT: head of the active list, 0 when empty. */
u32 actor_list_head(void) { return list_head(DS_00105BCC); }

/* PORT: next active record after rec, 0 at the end of the list. */
u32 actor_next(u32 rec)
{
    if (!in_pool(rec)) return 0;
    u32 next = DSD(rec);
    return next == DS_00105BCC ? 0u : next;
}

/* PORT: the record's pset: pool base + slot index * 0x20. */
u32 actor_pset(u32 rec)
{
    if (!in_pool(rec)) return 0;
    return DSD(DS_001014EC) + (u32)DSW(rec + 0x56) * PSET_SIZE;
}
