/* Port of the actor record pool, its free/active lists, the state-begin
 * reset and the spawn. Original addresses are named in comments:
 *   0x249B0 insert-after, 0x249C0 insert-before, 0x249D0 unlink,
 *   0x2AC80 alloc, 0x2BAF4 state-begin reset, 0x2AD40 release, 0x2AE14 spawn,
 *   0x33754 palette acquire.
 * The lists are {next@+0; prev@+4} dwords holding mem[] offsets; the sentinels
 * are the mem[] offsets DS_00105B3C (free) and DS_00105BCC (active) and point at
 * themselves when the list is empty. The two pools are allocated by
 * res_load_index (platform/res.c), so actors_init only validates them. */
#include "game/actors.h"
#include "../mem.h"
#include "../symbols.h"
#include "platform/gfx.h"
#include "platform/render.h"
#include "platform/res.h"

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
    /* PORT: 0x52106 stores its param_1 into both tick counters; the call site
     * (0x2BBEA) zeroes EAX first, so param_1 is provably 0. */
    DSD(DS_00101508) = 0;
    DSD(DS_0010150C) = 0;
    mem_fill(DSD(DS_001014E8), 0, 0xFA00u);
    mem_fill(DSD(DS_001014E4), 0, 0xFA00u);
    palette_list_init();                            /* 0x336C0 */
    DSB(DS_00105BED) = 1;
    /* PORT: 0x2EA30() restores the lock state saved before the counter zeroing;
     * inert as above. */
}

/* 0x2AC80: pop the free-list head and link it into the active list. The flag is
 * the caller's ECX; its 0x400 bit selects the tail insert (0x249C0) over the
 * head insert (0x249B0). 0x2AE14 passes its arg 3 (ECX), so actor_spawn threads
 * a3 here. */
u32 actor_alloc(u32 flag)
{
    if (list_head(DS_00105B3C) == 0) return 0;
    u32 rec = DSD(DS_00105B3C);
    list_unlink(rec);
    if (flag & 0x400u)
        /* TODO(verify): transcribed but unreached by this cycle's only caller:
         * the title spawns pass arg 3 = 0xE0/0xE4/0xFF (and 2 via 0x38B18),
         * none of which sets 0x400. */
        list_insert_before(DS_00105BCC, rec);
    else
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

/* ---- spawn support ------------------------------------------------------ */

/* 0x33754: acquire a reference to palette resource `handle` in the 24-entry
 * table at DS_00107618 (0x10-byte entries { handle; refcount; start; len }) and
 * enqueue its DAC range on the dirty list via palette_record (0x33734). Returns
 * the entry's mem[] offset (the original's EAX, stored in pset+0x18); 0 when the
 * table is full. 0x1B544 is the port's res_resolve. The original's 0x62003 on a
 * full table is a fatal error; the port returns 0 rather than exiting. */
static u32 palette_acquire(u32 handle)
{
    const u32 *res = res_resolve(handle);           /* 0x1B544 */
    u32 count = res ? *res : 0;
    u32 e = DS_00107618;
    while (e < DS_00107798) {                       /* search an existing entry */
        if (DSD(e) == handle) {
            DSD(e + 4) = DSD(e + 4) + 1;
            return e;
        }
        e += 0x10;
    }
    u32 start = 1;
    e = DS_00107618;
    while (e < DS_00107798 && DSD(e) != 0) {        /* first free entry */
        start = DSD(e + 8) + DSD(e + 12);
        e += 0x10;
    }
    if (e >= DS_00107798) return 0;
    DSD(e + 0) = handle;
    DSD(e + 4) = 1;
    DSD(e + 8) = start;
    DSD(e + 12) = count;
    palette_record(handle, start, count, 1);
    u32 prev = e;
    for (u32 p = e + 0x10; p < DS_00107798; p += 0x10) {
        if (DSD(p) == 0) continue;
        u32 next_start = DSD(prev + 8) + DSD(prev + 12);
        if (next_start == DSD(p + 8)) break;
        DSD(p + 8) = next_start;
        palette_record(DSD(p), next_start, DSD(p + 12), 1);
        prev = p;
    }
    return e;
}

/* PORT: deferred to Task 7. 0x2B2A0 is the animation-opcode dispatcher (1623
 * bytes). This stub cannot consume the stream, so it returns non-zero and not 2
 * to stop spawn's initial walk instead of spinning; it is NOT oracle-correct. */
static u32 spawn_anim_opcode(u32 rec, u32 index)
{
    (void)rec; (void)index;
    return 1;
}

/* PORT: deferred to Task 7. 0x2A408 is the literal sprite-id reader; until it
 * lands pset+0x00 is written from here and is NOT oracle-correct. */
static u32 spawn_anim_id(u32 rec, u32 pset)
{
    (void)rec; (void)pset;
    return 0;
}

/* PORT: deferred to Task 6. 0x2A820 writes the pset position/layer from the
 * record; Task 6 replaces this stub with the pset sync. */
static void spawn_pset_sync(u32 rec, u32 pset)
{
    (void)rec; (void)pset;
}

/* PORT: deferred to Task 6. 0x2A620 is the mode-1 cursor helper. */
static void spawn_mode1_cursor(u32 rec, u32 pset)
{
    (void)rec; (void)pset;
}

/* 0x2AE14. The register arguments are pinned by disassembly in
 * docs/superpowers/plans/2026-09-17-actor-system-args.md: EAX=desc, EDX=a2,
 * ECX=a3, EBX=a4, and the flags word a5 on the stack. a3 is also the 0x2AC80
 * alloc flag (its ECX at the call). */
u32 actor_spawn(const u32 *desc, u32 a2, u32 a3, u32 a4, u32 a5)
{
    const u8 *dp = (const u8 *)desc;
    u32 rec = actor_alloc(a3);
    if (rec == 0) return 0;

    u32 index = (rec - pool_base()) / ACTOR_REC_SIZE;
    DSW(rec + 0x56) = (u16)index;
    u32 pset = DSD(DS_001014EC) + (index & 0xffffu) * PSET_SIZE;

    DSD(rec + 0x08) = desc[0];
    u8 frame = dp[5];
    union { float f; u32 u; } fu;
    fu.f = (float)frame;
    DSD(rec + 0x20) = fu.u;
    DSD(rec + 0x24) = fu.u;
    if ((fu.u & 0x7fffffffu) != 0) {
        fu.f = fu.f - 1.0f;
        DSD(rec + 0x20) = fu.u;
    }
    DSB(rec + 0x48) = dp[4];
    DSW(rec + 0x2a) = 0;
    DSB(rec + 0x4a) = (u8)a5 & 0x7fu;
    DSW(rec + 0x2c) = *(const u16 *)(dp + 0x0c);
    s16 extent = *(const s16 *)(dp + 0x0a);
    DSB(rec + 0x51) = 0;
    DSB(rec + 0x55) = 0;
    DSB(rec + 0x4b) = 0;
    DSW(rec + 0x38) = 0;
    DSB(rec + 0x4f) = 0;
    DSB(rec + 0x59) = 0;
    DSB(rec + 0x4e) = 0;
    DSB(rec + 0x43) = 0;
    DSW(rec + 0x44) = 0;
    DSB(rec + 0x60) = 0;
    DSB(rec + 0x61) = 0;
    DSW(rec + 0x40) = (u16)(extent << 6);
    DSB(rec + 0x50) = DSB(rec + 0x51);
    DSB(rec + 0x54) = DSB(rec + 0x55);
    DSB(rec + 0x53) = DSB(rec + 0x55);
    DSB(rec + 0x52) = DSB(rec + 0x55);
    DSW(rec + 0x36) = DSW(rec + 0x38);
    DSW(rec + 0x34) = DSW(rec + 0x38);
    DSW(rec + 0x2e) = *(const u16 *)(dp + 0x06);
    DSB(rec + 0x5a) = (u8)(a5 >> 0x10);
    DSB(rec + 0x5f) = 1;

    u8 layer = (u8)a3;
    if ((a5 & 0x400u) == 0) {
        DSD(rec + 0x18) = a2;
        DSD(rec + 0x1c) = a4;
        DSW(rec + 0x28) = (u16)((*(const u16 *)(dp + 0x08) & 0xffc3u)
                                | (((a5 >> 8) & 0x44u) << 8));
        DSB(rec + 0x2b) |= 0x80;
        if ((DSW(rec + 0x28) >> 8 & 0x20) != 0)
            DSB(rec + 0x49) = layer;
        else
            DSW(rec + 0x32) = (u16)a3;
    } else {
        u32 parent = (u32)DSB(rec + 0x4a) * ACTOR_REC_SIZE + DSD(DS_001014F4);
        DSW(rec + 0x34) = (u16)a2;
        DSW(rec + 0x36) = (u16)a4;
        DSB(rec + 0x5a) = DSB(parent + 0x5a);
        DSB(parent + 0x4f) = (u8)(DSB(parent + 0x4f) + 1);
        DSW(rec + 0x28) = (u16)((*(const u16 *)(dp + 0x08) & 0xffc3u)
              | ((((a5 >> 8) & 0x40u) ^ ((DSW(parent + 0x28) >> 8) & 0x40u)) << 8)
              | 0x400u);
        layer = a3 != 0 ? (u8)a3 : DSB(parent + 0x49);
        DSB(rec + 0x49) = layer;
    }

    /* 0x2AFFA: initial animation-stream walk. The dispatcher and the literal
     * reader are Task 7 stubs, so the walk stops on the first opcode word and
     * the sprite id comes from the stub. */
    u32 id = 0;
    int have_id = 0;
    if ((DSW(rec + 0x28) >> 8 & 8) == 0) {
        DSD(rec + 0x08) = DSD(rec + 0x08) - 2;
        u32 status = 0;
        do {
            u32 p = DSD(rec + 0x08) + 2;
            DSD(rec + 0x08) = p;
            if ((DSW(p) >> 8 & 0x80) == 0) break;
            status = spawn_anim_opcode(rec, index);
        } while (status == 0);
        if (status == 2) { id = 0x1e1u; have_id = 1; }   /* Task 7 owns this */
    }
    if (!have_id) id = spawn_anim_id(rec, pset);
    DSW(pset + 0x00) = (u16)id;

    DSW(pset + 0x02) = (u16)(DSW(rec + 0x2e)
                             | (DSB(rec + 0x5f) != 0 ? 0x800u : 0u));

    u32 hdl = *(const u32 *)(dp + 0x10);
    DSD(pset + 0x18) = hdl != 0 ? palette_acquire(hdl) : 0u;

    DSB(rec + 0x2b) |= 0x20;
    spawn_pset_sync(rec, pset);                     /* Task 6 stub */
    DSB(rec + 0x2b) &= 0xdf;
    if ((DSW(rec + 0x28) >> 8 & 0x10) != 0) {
        DSD(pset + 0x14) = DSD(pset + 0x08);
        spawn_mode1_cursor(rec, pset);              /* Task 6 stub */
    }

    /* 0x2B0D4: the per-type render check (DS_000BB9DC + rec+0x48 * 0xC). Case
     * 0x00 is 0x5D812 `xor eax,eax; ret`, so the check returns 0 and the record
     * takes the visible path. */
    switch (DSB(rec + 0x48)) {
    case 0x00:
        DSB(rec + 0x2b) |= 0x40;
        if ((a5 & 0x400u) == 0) DSB(rec + 0x4a) = 0;
        render_list_insert(pset);                   /* 0x1C390 + 0x1C3A0 */
        return rec;
    default:
        /* PORT: per-type render check case 0x%02x is not ported. No title object
         * reaches it: 0x9AC30 and 0x9AC94 both carry desc+0x04 = 0x00. Mirror
         * the check's non-zero return (mark the record dead, return 0). */
        DSB(rec + 0x48) = 0;
        DSW(rec + 0x28) |= 8;
        return 0;
    }
}
