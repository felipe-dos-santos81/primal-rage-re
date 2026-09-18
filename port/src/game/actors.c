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

/* 0x2AC80: pop the free-list head and link it into the active list. The
 * argument is EAX at entry (0x2AC84 `mov ecx, eax` copies it before the
 * `xor cl,cl` / `and ch,4` test); its 0x400 bit selects the tail insert (0x249C0)
 * over the head insert (0x249B0). 0x2AE14 supplies EAX = the low 16 bits of its
 * arg 5 (`0x2AE37 mov ax, [esp+0x28]`), so actor_spawn threads a5 here. */
u32 actor_alloc(u32 flag)
{
    if (list_head(DS_00105B3C) == 0) return 0;
    u32 rec = DSD(DS_00105B3C);
    list_unlink(rec);
    if (flag & 0x400u)
        /* TODO(verify): transcribed but unreached by this cycle's only caller:
         * every title 0x2AE14 call passes a5 = 0, so the low 16 bits — and thus
         * bit 0x400 — are clear. */
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

/* ---- pset sync (0x2A31C -> 0x2A1FC -> 0x2A820) -------------------------- */

static void set_dead(u32 rec);

/* 0x2A620. The mode-1 shear cursor: derive rec+0x64 from the current y, or
 * from pset+0x14 (the previous frame's x) when rec+0x1c is zero. */
static void mode1_cursor(u32 rec, u32 pset)
{
    s32 v;
    if (DSD(rec + 0x1c) == 0)
        v = (s32)DSD(pset + 0x14);
    else
        v = (s32)DSD(DS_000F0AEC) + 0x3bc0 - ((s32)DSD(rec + 0x30) >> 16);
    v >>= 6;
    if ((u16)v < DSW(DS_00107A4C)) {        /* unsigned: 0x2A645 jae */
        DSB(rec + 0x64) = 0xff;
        return;
    }
    DSB(rec + 0x64) = (u8)((u8)v - (u8)DSW(DS_00107A4C));
    if (((s32)DSD(rec + 0x61) >> 24) >= 0x80)
        DSB(rec + 0x64) = 0x7f;
}

/* 0x2A690. Write pset+0x10/0x14 (previous x/y), pset+0x04/0x08 and the layer at
 * pset+0x0E, cache the mode-1 shear ramp entry at rec+0x46, and mirror the x
 * into rec+0x3C. */
static void pset_point(u32 rec)
{
    u32 pset = actor_pset(rec);
    u32 x;
    if ((DSW(rec + 0x28) >> 8 & 0x10u) == 0) {
        x = DSD(rec + 0x18) - DSD(DS_000F0AF0) + 0x2a00u;
    } else {
        if (DSW(rec + 0x38) != 0) mode1_cursor(rec, pset);
        if ((s8)DSB(rec + 0x64) >= 0) {
            s32 idx = (s32)DSD(rec + 0x61) >> 24;    /* byte at rec+0x64 */
            DSW(rec + 0x46) = DSW(DS_00107900 + (u32)idx * 2u);
            x = DSD(rec + 0x18) + 0x2a00u
                - (u32)(((s32)(s16)DSW(rec + 0x44)) * 2);
        } else if (DSW(rec + 0x34) == 0) {
            x = DSD(rec + 0x18) + 0x2a00u
                - (u32)((s32)DSD(DS_00107A44) >> 16);
        } else {
            s32 p = ((s32)DSD(DS_00107A44) >> 16) * (s32)(s16)DSW(rec + 0x32);
            x = DSD(rec + 0x18) + 0x2a00u - (u32)(p / 256);
        }
    }
    DSD(pset + 0x10) = DSD(pset + 4);
    DSD(pset + 0x14) = DSD(pset + 8);
    s32 ysrc;
    if ((DSW(rec + 0x28) & 0x40u) == 0)
        ysrc = ((s32)DSD(rec + 0x30) >> 16) + (s32)DSD(rec + 0x1c);
    else
        ysrc = (s32)DSD(rec + 0x30) >> 16;
    s32 y = (s32)DSD(DS_000F0AEC) + 0x3bc0 - ysrc;
    DSD(DS_00105BE0) = (u32)y;
    DSD(pset + 4) = x;
    DSD(pset + 8) = (u32)y;
    if ((s16)DSW(rec + 0x32) > 0) {
        /* TODO(verify): 0x2A7CA's clamp is `and eax,0xffff; cmp eax,0xff; jle`
         * (signed), so values 0x8000..0xffff are not clamped. The decompiler
         * reads it unsigned. Transcribed signed. */
        s32 layer = 0xf0 - ((s32)DSD(rec + 0x30) >> 22)
                  + ((s32)DSD(rec + 0x56) >> 24);
        u16 u = (u16)layer;
        if ((s32)u > 0xff) u = 0xff;
        DSW(pset + 0x0e) = u;
    } else {
        s32 layer = (s32)(s8)DSB(rec + 0x59) + 0xf0;
        if (layer > 0xff) layer = 0xff;
        DSW(pset + 0x0e) = (u16)layer;
    }
    DSW(pset + 0x0c) = DSW(rec + 0x2c);
    DSD(rec + 0x3c) = DSD(pset + 4);
    DSD(DS_00105BDC) = x;
}

/* 0x2A820. The pset position/layer writer: 0x2A690 for a free record, the
 * parent-relative form for a child, and the on-screen visibility test. */
static void pset_write(u32 rec, u32 pset)
{
    if ((DSW(rec + 0x28) >> 8 & 0x20u) == 0) {
        if ((DSW(rec + 0x28) >> 8 & 0x04u) == 0) {
            pset_point(rec);
        } else {
            u32 parent = DSD(DS_001014F4)
                       + (u32)DSB(rec + 0x4a) * ACTOR_REC_SIZE;
            if ((DSW(parent + 0x28) & 8u) != 0) goto dead;
            u32 pp = DSD(DS_001014EC)
                   + (u32)DSW(parent + 0x56) * PSET_SIZE;
            u32 x = DSD(pp + 4) + (u32)(((s32)DSD(rec + 0x32) >> 16) << 6);
            DSD(pset + 4) = x;
            DSD(DS_00105BDC) = x;
            if ((DSW(rec + 0x28) & 0x40u) == 0)
                DSD(pset + 8) = DSD(pp + 8)
                    + (u32)(((s32)DSD(rec + 0x34) >> 16) << 6);
            else
                DSD(pset + 8) = (u32)((s32)DSD(DS_000F0AEC) + 0x3bc0
                    - ((s32)DSD(parent + 0x30) >> 16));
            if ((DSW(parent + 0x28) & 8u) != 0) goto dead;
            u8 lyr = (u8)(DSB(pp + 0x0e) + DSB(rec + 0x59));
            DSB(rec + 0x49) = lyr;
            DSW(pset + 0x0e) = lyr;
            DSW(pset + 0x0c) = DSW(rec + 0x2c);
            DSD(rec + 0x3c) = DSD(pset + 4);
        }
    } else {
        if ((DSW(rec + 0x28) >> 8 & 0x04u) != 0) {
            u32 parent = DSD(DS_001014F4)
                       + (u32)DSB(rec + 0x4a) * ACTOR_REC_SIZE;
            if ((DSW(parent + 0x28) & 8u) != 0) goto dead;
            DSD(rec + 0x18) = DSD(parent + 0x18)
                + (u32)(((s32)DSD(rec + 0x32) >> 16) << 6);
            DSD(rec + 0x1c) = DSD(parent + 0x1c)
                + (u32)(((s32)DSD(rec + 0x34) >> 16) << 6);
            DSW(rec + 0x2c) = DSW(parent + 0x2c);
        }
        DSD(pset + 4) = DSD(rec + 0x18);
        DSD(DS_00105BDC) = DSD(rec + 0x18);
        DSD(pset + 8) = DSD(rec + 0x1c);
        DSD(DS_00105BE0) = DSD(rec + 0x1c);
        DSW(pset + 0x0c) = DSW(rec + 0x2c);
        /* TODO(verify): 0x2A8DE's clamp is signed (`cmp edx,0xff; jle`), so a
         * low word of 0x8000..0xffff is not clamped; the decompiler reads it
         * unsigned. Transcribed signed. */
        u16 layer = (u16)((s32)DSB(rec + 0x49) + (s32)(s8)DSB(rec + 0x59));
        if ((s32)layer > 0xff) layer = 0xff;
        DSW(pset + 0x0e) = layer;
    }
    if ((DSW(rec + 0x28) >> 8 & 1u) != 0) return;
    u32 extent = DSW(rec + 0x40);
    if ((s32)(0u - extent) < (s32)DSD(DS_00105BDC) &&
        (s32)DSD(DS_00105BDC) < (s32)(extent + 0x5400u) &&
        (s32)(0u - extent) < (s32)DSD(DS_00105BE0) &&
        (s32)DSD(DS_00105BE0) < (s32)(extent + 0x3bc0u) &&
        DSW(rec + 0x2c) != 0) {
        DSB(rec + 0x2b) |= 0x18;
        return;
    }
    if ((DSW(rec + 0x28) & 0x80u) == 0) return;
    if ((DSW(rec + 0x2a) >> 8 & 8u) == 0) return;
dead:
    set_dead(rec);
}

/* 0x2A4FC. Integrate the record's velocity and gravity. */
static void motion_step(u32 rec)
{
    if ((DSW(rec + 0x2a) >> 8 & 0x80u) == 0) return;
    DSD(rec + 0x18) = (u32)((s32)DSD(rec + 0x18) + (s32)(s16)DSW(rec + 0x32));
    DSD(rec + 0x1c) = (u32)((s32)DSD(rec + 0x1c) + (s32)(s16)DSW(rec + 0x34));
    s16 sv = (s16)DSW(rec + 0x34);
    DSW(rec + 0x32) = (u16)(DSW(rec + 0x32) + DSW(rec + 0x38));
    if (sv < 0)
        DSW(rec + 0x34) = (u16)(sv - (s16)DSB(rec + 0x42));
    else if (sv > 0)
        DSW(rec + 0x34) = (u16)(sv + (s16)DSB(rec + 0x42));
    if (DSB(rec + 0x43) != 0) {
        s32 v = ((s16)DSW(rec + 0x34) < 0)
              ? -(s32)((s32)DSD(rec + 0x32) >> 16)
              :  (s32)((s32)DSD(rec + 0x32) >> 16);
        s32 b = (s32)DSB(rec + 0x43);
        s16 cur = (s16)DSW(rec + 0x34);
        if (v > b) {
            if (cur > 0) DSW(rec + 0x34) = (u16)(cur - (s16)b);
            else         DSW(rec + 0x34) = (u16)(cur + (s16)b);
        } else {
            DSW(rec + 0x34) = 0;
            DSB(rec + 0x43) = 0;
        }
    }
    if (DSW(rec + 0x44) == 0) return;
    s16 g = (s16)DSW(rec + 0x44);
    if ((DSW(rec + 0x28) & 0x20u) != 0 || (s16)DSW(rec + 0x36) >= 0 ||
        (s32)DSD(rec + 0x1c) > 0) {
        DSW(rec + 0x36) = (u16)((s16)DSW(rec + 0x36) - g);
        return;
    }
    if ((DSW(rec + 0x28) & 0x80u) != 0) {
        set_dead(rec);
    } else {
        DSW(rec + 0x36) = 0;
        DSW(rec + 0x44) = 0;
        DSD(rec + 0x1c) = 0;
    }
}

/* 0x2A39C. Clear the +0x28 0x04 bit and write pset+0 from the Task 7 reader,
 * or-ing the parent's hflip into bit 0x8000. */
static void anim_id_path(u32 rec, u32 slot)
{
    DSB(rec + 0x28) &= 0xfb;
    u32 pset = DSD(DS_001014EC) + slot * PSET_SIZE;
    u32 id = spawn_anim_id(rec, pset);          /* PORT: deferred to Task 7 */
    if (DSB(rec + 0x4a) != 0) {
        u32 parent = DSD(DS_001014F4)
                   + (u32)DSB(rec + 0x4a) * ACTOR_REC_SIZE;
        if ((DSW(parent + 0x28) >> 8 & 0x40u) == 0) id &= 0x7fffu;
        else                                        id |= 0x8000u;
    }
    DSW(pset) = (u16)id;
}

/* PORT: deferred to Task 7. 0x29F34 is the animation-stream variable reader
 * (Task 7's anim_read_var); frame_timer's +0x28 0x10 probe returns early while
 * this returns 0, so opcode-conditional delays are not yet oracle-correct. */
static u32 anim_read_var(u32 rec, u8 op)
{
    (void)rec; (void)op;
    return 0;
}

/* 0x2AA70. The frame timer: consume stream opcodes until the dispatcher stops,
 * then take the sprite id through 0x2A39C. The child chain is drained before
 * the walk and a dead ancestor stops it. */
static void frame_timer(u32 rec, u32 slot)
{
    if ((DSW(rec + 0x28) & 0x810u) != 0 || (DSW(rec + 0x2a) & 4u) != 0) {
        if ((DSW(rec + 0x28) >> 8 & 8u) != 0) return;
        if ((DSW(rec + 0x28) & 0x10u) != 0) {
            if ((u16)anim_read_var(rec, (u8)DSW(DSD(rec + 8))) == 0) return;
            DSB(rec + 0x28) &= 0xef;
        }
        if ((DSW(rec + 0x2a) & 4u) != 0 &&
            (((u32)DSW(DS_000EF6DC) ^ (DSW(rec + 0x2a) & 1u)) & 1u) == 0)
            return;
    }
    if ((DSW(rec + 0x2a) >> 8 & 2u) != 0) {
        union { float f; u32 u; } fu;
        fu.f = (float)(u32)DSB(DS_00105BEC);
        DSD(rec + 0x20) = fu.u;
    }
    union { float f; u32 u; } fu;
    fu.u = DSD(rec + 0x20);
    float old = fu.f;
    fu.f = old - 1.0f;
    DSD(rec + 0x20) = fu.u;
    if (old > 0.0f) {                           /* 0x2AC47 */
        if ((DSW(rec + 0x2a) >> 8 & 1u) == 0) return;
        if (DSD(DS_00104AE0) != 0) return;
        DSW(DSD(DS_001014EC) + slot * PSET_SIZE) = 0x1e1;
        return;
    }
    for (;;) {                                  /* 0x2AB4A */
        DSB(rec + 0x63) = (u8)(DSB(rec + 0x63) + 1);
        if (DSB(rec + 0x4b) != 0) {
            for (;;) {
                u32 child = DSD(DS_001014F4)
                          + (u32)DSB(rec + 0x4b) * ACTOR_REC_SIZE;
                if ((DSW(child + 0x28) & 8u) == 0) {
                    DSB(child + 0x2a) |= 8;
                    frame_timer(child, DSB(rec + 0x4b));
                    DSD(child + 0x20) = 0;
                    break;
                }
                DSB(child + 0x2a) &= 0xf7;
                u8 next = DSB(child + 0x4b);
                DSB(rec + 0x4b) = next;
                if (next == 0) goto stream_walk;
                DSB(child + 0x4b) = 0;
            }
        }
stream_walk:;
        u32 status = 0;
        for (;;) {
            u32 p = DSD(rec + 8) + 2;
            DSD(rec + 8) = p;
            if ((DSW(p) >> 8 & 0x80u) == 0) break;
            status = spawn_anim_opcode(rec, slot);  /* PORT: Task 7 stub */
            if (status != 0) break;
        }
        if (status == 2) return;
        anim_id_path(rec, slot);
        if ((DSW(rec + 0x2a) >> 8 & 4u) != 0) {
            u32 p = DSD(rec + 0x10) + (u32)((s32)DSD(rec + 0x4f) >> 24);
            union { float f; u32 u; } dv;
            dv.f = (float)(u32)DSB(p);
            DSD(rec + 0x24) = dv.u;
        }
        union { float f; u32 u; } a, b;
        a.u = DSD(rec + 0x24);
        b.u = DSD(rec + 0x20);
        a.f = a.f + b.f;
        DSD(rec + 0x20) = a.u;
        if ((DSW(rec + 0x2a) & 8u) != 0) return;
        union { float f; u32 u; } step;
        step.u = DSD(rec + 0x24);
        if (!(step.f > 0.0f)) return;           /* rec+0x24 <= 0 */
        b.u = DSD(rec + 0x20);
        if (!(b.f < 0.0f)) return;              /* rec+0x20 >= 0 */
        /* else reloop */
    }
}

/* 0x33864: drop a palette-table reference; clear the handle at 0 at zero. */
static void palette_release(u32 entry)
{
    u32 ref = DSD(entry + 4);
    DSD(entry + 4) = ref - 1;
    if (ref - 1 == 0) DSD(entry) = 0;
}

/* 0x2B150. Set the dead bit (0x28 0x08), release the pset palette and unlink
 * the pset from the render list. 63 callers in the original; only the sync
 * path reaches it in this cycle. */
static void set_dead(u32 rec)
{
    DSB(rec + 0x28) |= 0x08;
    if ((DSW(rec + 0x2a) >> 8 & 0x40u) != 0) {
        /* PORT: 0x2B150 calls the per-type teardown at DS_000BB9E0 + type*0xC
         * and then clears rec+0x2b 0x40. No title record's code-object path is
         * reimplemented and no address is registered for fn_resolve, so the
         * callback and its clear are not executed here. */
    }
    u32 pset = actor_pset(rec);
    if (DSD(pset + 0x18) != 0) {
        palette_release(DSD(pset + 0x18));
        DSD(pset + 0x18) = 0;
    }
    render_list_remove(pset);                   /* 0x1C458 + 0x1C3D0 */
}

/* 0x2AD40. The release path: drop the child, decrement the parent refcount,
 * return the record to the free list and zero its pset. */
static void release_record(u32 rec, u32 pset)
{
    if (!in_pool(rec)) return;                  /* PORT: spec §7 invariant */
    if ((DSW(rec + 0x2a) & 8u) != 0) return;
    if (DSB(rec + 0x4f) != 0) return;
    if (DSB(rec + 0x4b) != 0) {
        u32 child = DSD(DS_001014F4)
                  + (u32)DSB(rec + 0x4b) * ACTOR_REC_SIZE;
        DSB(child + 0x2a) &= 0xf7;
        set_dead(child);
    }
    if ((DSW(rec + 0x28) >> 8 & 4u) != 0) {
        u32 parent = DSD(DS_001014F4)
                   + (u32)DSB(rec + 0x4a) * ACTOR_REC_SIZE;
        DSB(parent + 0x4f) = (u8)(DSB(parent + 0x4f) - 1);
    }
    list_unlink(rec);                           /* 0x249D0 */
    list_insert_after(DS_00105B3C, rec);        /* 0x249B0 */
    DSD(pset + 8) = 0;
    DSW(pset + 0x0c) = 0;
    DSD(pset + 4) = DSD(pset + 8);
    DSW(pset + 0x0e) = DSW(pset + 0x0c);
    set_dead(rec);
    DSB(rec + 0x4b) = 0;
    DSB(rec + 0x4a) = DSB(rec + 0x4b);
}

/* 0x2A1FC. Per-record sync: timer, anim-id, pset-id hold, motion then write. */
static void sync_record(u32 rec)
{
    u32 slot = DSW(rec + 0x56);
    u32 pset = DSD(DS_001014EC) + slot * PSET_SIZE;
    if (DSB(DS_00104B26) != 0) {
        pset_write(rec, pset);
        return;
    }
    if ((DSD(rec + 0x24) & 0x7fffffffu) != 0 ||
        (((DSW(rec + 0x2a) >> 8) & 2u) != 0 && DSB(DS_00105BEE) != 0))
        frame_timer(rec, slot);
    DSB(rec + 0x2a) &= 0xfb;
    if ((DSW(rec + 0x28) & 4u) != 0) anim_id_path(rec, slot);
    if (DSB(rec + 0x4e) != 0) {
        u8 c = (u8)(DSB(rec + 0x4e) - 1);
        DSB(rec + 0x4e) = c;
        u16 base = DSB(rec + 0x5f) != 0 ? 0x800u : 0u;
        if (c == 0) DSW(pset + 2) = DSW(rec + 0x2e) | base;
        else        DSW(pset + 2) = DSW(rec + 0x30) | base;
    }
    if ((DSW(rec + 0x28) & 8u) == 0) {
        if ((DSW(rec + 0x28) & 0x200u) == 0) motion_step(rec);
        if ((DSW(rec + 0x28) & 8u) == 0) pset_write(rec, pset);
    }
    if ((DSW(rec + 0x28) & 8u) != 0) release_record(rec, pset);
}

/* 0x2A31C. The per-frame actor update: decrement the 0x4F-table frame counter,
 * then walk the active list and sync each record. */
void actors_update(void)
{
    if (DSB(DS_00104B24) != 0) return;
    if (DSB(DS_00104B26) == 0) {
        u8 c = (u8)(DSB(DS_00105BEC) - 1);
        DSB(DS_00105BEC) = c;
        if (c == 0xff) DSB(DS_00105BEC) = (u8)(DSB(DS_00105BEE) - 1);
    }
    u32 rec = list_head(DS_00105BCC);
    while (rec != 0) {
        u32 next = actor_next(rec);
        if ((DSW(rec + 0x28) & 2u) == 0) {
            if ((DSW(rec + 0x28) & 1u) != 0)
                DSB(rec + 0x28) &= 0xfe;
            else
                sync_record(rec);
        }
        rec = next;
    }
}

/* 0x2AE14. The register arguments are pinned by disassembly in
 * docs/superpowers/plans/2026-09-17-actor-system-args.md: EAX=desc, EDX=a2,
 * ECX=a3, EBX=a4, and the flags word a5 on the stack. a5's low 16 bits are also
 * the 0x2AC80 alloc flag (EAX at that call). */
u32 actor_spawn(const u32 *desc, u32 a2, u32 a3, u32 a4, u32 a5)
{
    const u8 *dp = (const u8 *)desc;
    u32 rec = actor_alloc(a5);
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
    DSW(rec + 0x40) = (u16)(extent * 64);
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
    pset_write(rec, pset);                          /* 0x2A820 */
    DSB(rec + 0x2b) &= 0xdf;
    if ((DSW(rec + 0x28) >> 8 & 0x10) != 0) {
        DSD(pset + 0x14) = DSD(pset + 0x08);
        mode1_cursor(rec, pset);                    /* 0x2A620 */
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
        /* PORT: the per-type render check case for this record's type is not
         * ported. No title object reaches it: 0x9AC30 and 0x9AC94 both carry
         * desc+0x04 = 0x00. Mirror the check's non-zero return (mark the record
         * dead, return 0). */
        DSB(rec + 0x48) = 0;
        DSW(rec + 0x28) |= 8;
        return 0;
    }
}
