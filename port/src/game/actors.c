/* Port of the actor record pool, its free/active lists, the state-begin
 * reset and the spawn. Original addresses are named in comments:
 *   0x249B0 insert-after, 0x249C0 insert-before, 0x249D0 unlink,
 *   0x2AC80 alloc, 0x2BAF4 state-begin reset, 0x2AD40 release, 0x2AE14 spawn,
 *   0x33754 palette acquire, 0x29F34/0x29DB8 anim variable read/write,
 *   0x2A408 sprite-id reader, 0x2B8F8 operand fetch, 0x2B2A0 opcode dispatcher,
 *   0x2BC30/0x2BCF4 animation entry.
 * The lists are {next@+0; prev@+4} dwords holding mem[] offsets; the sentinels
 * are the mem[] offsets DS_00105B3C (free) and DS_00105BCC (active) and point at
 * themselves when the list is empty. The two pools are allocated by
 * res_load_index (platform/res.c), so actors_init only validates them. */
#include "game/actors.h"
#include "game/effects.h"
#include "game/fighter.h"
#include "game/fight.h"
#include "game/rng.h"
#include "../mem.h"
#include "../symbols.h"
#include "platform/gfx.h"
#include "platform/render.h"
#include "platform/res.h"
#include "platform/sprite.h"
#include <string.h>

/* symbols.h emits no name for the type-0x2D teardown's counter at 0x108398. */
#define DS_00108398 0x00108398u

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

static void anim_code_10FA8(u32 rec, u32 arg);
static void anim_code_12720(u32 rec, u32 arg);
static void anim_code_37A58(u32 rec, u32 arg);
static void anim_code_39A34(u32 rec, u32 arg);
static void anim_code_36870(u32 rec, u32 arg);
static void anim_code_35E04(u32 rec, u32 arg);
static void anim_code_3E4E4(u32 rec, u32 arg);
static void anim_code_347B8(u32 rec, u32 arg);
static void anim_code_346F8(u32 rec, u32 arg);
static void anim_code_35938(u32 rec, u32 arg);
static void anim_code_4AC18(u32 rec, u32 arg);
static void anim_code_4AC80(u32 rec, u32 arg);
static void anim_code_3D214(u32 rec, u32 arg);
static void anim_code_3D26C(u32 rec, u32 arg);

/* The 0xBB9D8 type table's two callback halves (cb1 at 0xBB9DC, cb2 at
 * 0xBB9E0). actor_spawn's tail (0x2B0D4) calls cb1 with (rec, slot) and tests
 * the returned AL as a whole byte; set_dead (0x2B150) calls cb2 with rec and
 * discards its return. */
typedef u8 (*actor_type_cb1)(u32 rec, u32 slot);
typedef void (*actor_type_cb2)(u32 rec);

static u8   actor_type_127C0(u32 rec, u32 slot);
static u8   actor_type_198E8(u32 rec, u32 slot);
static u8   actor_type_28F64(u32 rec, u32 slot);
static u8   actor_type_2901C(u32 rec, u32 slot);
static u8   actor_type_48CD8(u32 rec, u32 slot);
static u8   actor_type_412F0(u32 rec, u32 slot);
static u8   actor_type_412FC(u32 rec, u32 slot);
static void actor_type_12800(u32 rec);
static void actor_type_19928(u32 rec);
static void actor_type_290D0(u32 rec);
static void actor_type_3B9C4(u32 rec);
static void actor_type_3D784(u32 rec);
static void actor_type_3FC90(u32 rec);
static void actor_type_40684(u32 rec);
static void actor_type_48D3C(u32 rec);
static void actor_type_49444(u32 rec);

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
    /* PORT: the animation code pointers this module implements. anim_indirect
     * (0x2B2A0 opcodes 0x10/0x11/0x15) calls them through fn_resolve; an
     * unregistered target is skipped. */
    fn_register(0x10FA8u, (void (*)(void))anim_code_10FA8);
    fn_register(0x12720u, (void (*)(void))anim_code_12720);
    fn_register(0x37A58u, (void (*)(void))anim_code_37A58);
    /* PORT: the roar stream's 0xD100/0xD500 targets (opcodes 0x11/0x15, mode
     * 0x4000): the frame-hold scaler 0x39A34 and the already-ported 0x36870
     * +0x54 machine. */
    fn_register(0x39A34u, (void (*)(void))anim_code_39A34);
    fn_register(0x36870u, (void (*)(void))anim_code_36870);
    /* PORT: the characters' 0xC8B30 attack streams' 0xD000 target (opcode
     * 0x10, mode 0x4000, dword 0x00035E04), the attack's launch. */
    fn_register(0x35E04u, (void (*)(void))anim_code_35E04);
    /* PORT: the T-rex's reaction-0x2B stream 0xE7BDE's 0xD500 target (opcode
     * 0x15, mode 0x4000, dword 0x0003E4E4), its leap. */
    fn_register(0x3E4E4u, (void (*)(void))anim_code_3E4E4);
    /* PORT: 0x34E2C's reaction callback 0x3E62C (*(u32*)0xA3884) and the slot
     * +0x0C callback it stores, 0x3E524 (0x3531C case 7); both take the raw's
     * (slot, rec, side) registers. */
    fn_register(0x3E62Cu, (void (*)(void))fighter_3e62c);
    fn_register(0x3E524u, (void (*)(void))fighter_3e524);
    /* PORT: 0x34E2C's reaction callback 0x3D17C (*(u32*)0xA37A8, the T-rex's
     * reaction 0x20), same (slot, rec, side) registers. */
    fn_register(0x3D17Cu, (void (*)(void))fighter_3d17c);
    /* PORT: its stream 0xE84C8's 0xD100 target 0x3D214 (the emitter) and the
     * emitter stream 0xE8598's 0xD100 target 0x3D26C (the projectile), opcode
     * 0x11, mode 0x4000. */
    fn_register(0x3D214u, (void (*)(void))anim_code_3D214);
    fn_register(0x3D26Cu, (void (*)(void))anim_code_3D26C);
    /* PORT: the +0x1C callback 0x3E62C also stores, 0x3E4C4, called by
     * 0x193B0 at 0x19505 as fn(side). */
    fn_register(0x3E4C4u, (void (*)(void))fighter_3e4c4);
    /* PORT: the state-7 pose handler 0x3531C case 10 resolves from the slot's
     * +0x10 (the 0x3A504 setter writes it; the raw reaches it only through
     * that indirect call). */
    fn_register(0x3A43Cu, (void (*)(void))fighter_pose_3a43c);
    /* PORT: the knockback pose's handler 0x39CC8, which the setter 0x39F40
     * stores in slot+0x10 at 0x39F8F; same case-10 shape as 0x3A43C. */
    fn_register(0x39CC8u, (void (*)(void))fighter_39cc8);
    /* PORT: the landing streams' 0xD500 target 0x347B8 (the knockdown floor)
     * and the floor streams' 0xD500 target 0x346F8 (the get-up), opcode 0x15,
     * mode 0x4000. */
    fn_register(0x347B8u, (void (*)(void))anim_code_347B8);
    fn_register(0x346F8u, (void (*)(void))anim_code_346F8);
    /* PORT: the walk entry 0x35938, the 0xD500 target of 14 stream sites
     * (the T-rex's at 0xE6EE8), opcode 0x15, mode 0x4000. */
    fn_register(0x35938u, (void (*)(void))anim_code_35938);
    /* PORT: the worshipper streams' 0xD500 target 0x4AC18 (24 sites in
     * 0xEE09E..0xEF62E; the first after the 0xD500 word at 0xEE09C), opcode
     * 0x15, mode 0x4000: the arrival 0x4AC38 for the actor's +0x14 entry. */
    fn_register(0x4AC18u, (void (*)(void))anim_code_4AC18);
    /* PORT: the worshipper landing streams' 0xD500 target 0x4AC80 (6 sites in
     * 0xEE3BC..0xEF608; the first after the 0xD500 word at 0xEE3BA), opcode
     * 0x15, mode 0x4000: the climb, walk or hold for the actor's +0x14 entry. */
    fn_register(0x4AC80u, (void (*)(void))anim_code_4AC80);
    /* The 16 non-stub entries of the type table's callback halves. The other
     * entries hold the stub 0x5D812, which stays unregistered: the spawn
     * dispatch's fn_resolve miss keeps the raw's identity test for it. */
    fn_register(0x127C0u, (void (*)(void))actor_type_127C0);
    fn_register(0x12800u, (void (*)(void))actor_type_12800);
    fn_register(0x198E8u, (void (*)(void))actor_type_198E8);
    fn_register(0x19928u, (void (*)(void))actor_type_19928);
    fn_register(0x28F64u, (void (*)(void))actor_type_28F64);
    fn_register(0x2901Cu, (void (*)(void))actor_type_2901C);
    fn_register(0x290D0u, (void (*)(void))actor_type_290D0);
    fn_register(0x3B9C4u, (void (*)(void))actor_type_3B9C4);
    fn_register(0x3D784u, (void (*)(void))actor_type_3D784);
    fn_register(0x3FC90u, (void (*)(void))actor_type_3FC90);
    fn_register(0x40684u, (void (*)(void))actor_type_40684);
    fn_register(0x412F0u, (void (*)(void))actor_type_412F0);
    fn_register(0x412FCu, (void (*)(void))actor_type_412FC);
    fn_register(0x48CD8u, (void (*)(void))actor_type_48CD8);
    fn_register(0x48D3Cu, (void (*)(void))actor_type_48D3C);
    fn_register(0x49444u, (void (*)(void))actor_type_49444);
    return 1;
}

/* 0x38B70. Zeroes the three actor cursor globals DS_000BDFBC/C0/C4 and two
 * 7-dword arrays: DS_00107A00 (input latches) and DS_00107A1C, the front-end
 * row table that 0x38B18 (frontend_spawn_row) fills. The raw calls it from
 * 0x2BAF4 (actors_reset) at 0x2BBDA and from 0x20C10 (game_init) at 0x20C3D;
 * without the 0x2BAF4 call the row table only ever accumulates, so every spawn
 * after the seventh is dropped. 0x654C7's count is in dwords (7 each). */
void actor_cursor_reset(void)
{
    DSD(DS_000BDFBC) = 0;
    DSD(DS_000BDFC0) = 0;
    DSD(DS_000BDFC4) = 0;
    mem_fill(0x107A00u, 0, 28u);
    mem_fill(DS_00107A1C, 0, 28u);
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
    effects_clear();                            /* 0x2BB2B (0x13DF0) */
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
    effects_init();                             /* 0x2BBB8 (0x13ADC) */
    /* PORT: 0x4F228(0,0) zeroes the input/mouse state at DS_00107A38/3A and
     * DS_00107A54/55; input state is owned by platform/input.c. */
    DSD(DS_00105B44) = 0;
    DSD(DS_00105B48) = 0;
    render_list_init();                             /* 0x1C350 */
    actor_cursor_reset();                           /* 0x38B70 */
    mem_fill(DS_00105F38, 0, 0x14D4u);              /* 0x2F920 */
    /* 0x2BAF4's param_1 != 0 arm: 0x52106 clears both offscreen buffers, blacks
     * the DAC and clears the screen aperture, and zeroes the tick counters.
     * PORT: the VGA DAC clear is palette_list_init's gfx_dac memset; the literal
     * 0xA0000 clear (0x5214C-0x52151 `mov eax,0xa0000; call 0x51f72`) is
     * gfx_aperture(), the port's model of that screen. The param_1 == 0 arm
     * (copy DS_000E87A0 into DS_000E87A4) is unreachable from the title path and
     * is not transcribed. */
    /* PORT: 0x52106 stores its param_1 into both tick counters; the call site
     * (0x2BBEA) zeroes EAX first, so param_1 is provably 0. */
    DSD(DS_00101508) = 0;
    DSD(DS_0010150C) = 0;
    mem_fill(DSD(DS_001014E8), 0, 0xFA00u);
    mem_fill(DSD(DS_001014E4), 0, 0xFA00u);
    memset(gfx_aperture(), 0, 0xFA00u);          /* 0x5214C 0x51F72 */
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
        /* 0x2ACB6..0x2ACD7: `xor cl,cl ; and ch,4` leaves CX = flag & 0x400,
         * and non-zero calls 0x249C0 (insert before the sentinel: the tail).
         * Reached in the demo by 0x3D214's child spawn (a5 = rec+0x56 |
         * 0x400, demo-pose record §25/§26). */
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
u32 palette_acquire(u32 handle)
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

/* 0x33874. Re-point the palette-table entry `descriptor` at `handle` and reflow
 * the entries after it. The two callers are the attract drivers
 * (0x4F81F/0x4F872): EAX = the descriptor (an entry of the table at
 * DS_00107618, `{handle; rc; start; len}`), EDX = the handle. 0x1B544
 * (res_resolve) pushes EDX at entry and pops it at every return, so the raw's
 * EDX is still the handle after the call. The raw compares the entry's len
 * against the resolved resource's leading colour count (`[EAX]`): when len is
 * short (0x3388D `jl`), it walks the table from descriptor+0x10 to the table
 * end (0x107798) and, while an entry's start is below the previous entry's end,
 * moves the start to that end and re-enqueues the walked entry's own tuple
 * `{p.handle; p.start; p.len; 1}` (raw 0x338DC `MOV EBX,[EAX]` — the entry's
 * handle, not the `handle` argument); otherwise, when the handle differs from
 * the entry's, it re-points the entry and enqueues
 * `{handle; entry.start; count; 1}`. PORT: a failed resolve (NULL)
 * is treated as count 0, as gfx_flush_palette does, rather than the raw's
 * deref. */
void palette_reflow(u32 descriptor, u32 handle)
{
    const u32 *res = res_resolve(handle);           /* 0x1B544 */
    u32 count = res ? *res : 0;
    if ((s32)DSD(descriptor + 0x0Cu) < (s32)count) {        /* 0x3388B 0x3388D */
        u32 prev = descriptor;
        for (u32 p = descriptor + 0x10u; p < DS_00107798; p += 0x10u) {
            u32 next_start = DSD(prev + 0x08u) + DSD(prev + 0x0Cu);
            if ((s32)next_start <= (s32)DSD(p + 0x08u)) break;   /* 0x338D1 */
            DSD(p + 0x08u) = next_start;                        /* 0x338D3 */
            palette_record(DSD(p), next_start, DSD(p + 0x0Cu), 1);
            prev = p;                                           /* 0x338E8 */
        }
    } else if (handle != DSD(descriptor)) {                     /* 0x33897/0x33899 */
        DSD(descriptor) = handle;                               /* 0x3389B */
        palette_record(handle, DSD(descriptor + 0x08u), count, 1);
    }
}

/* ---- animation-stream interpreter (0x29F34/0x29DB8/0x2A408/0x2B8F8/0x2B2A0) */

static void set_dead(u32 rec);

/* 0x29F34. Read one animation variable. `op & 0x7F` selects: < 0x40 the
 * 0x40-word ring at DS_00105B4C indexed by rec+0x51; 0x40..0x45 the record's
 * own fields (0x40..0x43 and 0x45 sign-extended, 0x44 the pset-slot word);
 * 0x46..0x4B the same fields on the parent at rec+0x4A; 0x4C..0x51 on the
 * child at rec+0x4B. The disassembly passes the selector in EDX's low byte
 * (0x29F38 `xor dh,dh` / `and dl,0x7f`), not the decompiler's ABI reading. */
u32 anim_read_var(u32 rec, u8 op)
{
    u32 o = (u32)(op & 0x7fu);
    if (o < 0x40u)
        return DSW(DS_00105B4C + ((o + (u32)DSB(rec + 0x51)) & 0x3fu) * 2u);
    switch (o) {
    case 0x40: return (u32)(u16)(s8)DSB(rec + 0x52);
    case 0x41: return (u32)(u16)(s8)DSB(rec + 0x53);
    case 0x42: return (u32)(u16)(s8)DSB(rec + 0x54);
    case 0x43: return (u32)(u16)(s8)DSB(rec + 0x55);
    case 0x44: return DSW(rec + 0x56);
    case 0x45: return (u32)(u16)(s8)DSB(rec + 0x58);
    default: break;
    }
    u32 base;
    if (o <= 0x4bu)
        base = DSD(DS_001014F4) + (u32)DSB(rec + 0x4a) * ACTOR_REC_SIZE;
    else if (o <= 0x51u)
        base = DSD(DS_001014F4) + (u32)DSB(rec + 0x4b) * ACTOR_REC_SIZE;
    else
        return 0;
    switch (o) {
    case 0x46: case 0x4c: return (u32)(u16)(s8)DSB(base + 0x52);
    case 0x47: case 0x4d: return (u32)(u16)(s8)DSB(base + 0x53);
    case 0x48: case 0x4e: return (u32)(u16)(s8)DSB(base + 0x54);
    case 0x49: case 0x4f: return (u32)(u16)(s8)DSB(base + 0x55);
    case 0x4a: case 0x50: return DSW(base + 0x56);
    default:              return (u32)(u16)(s8)DSB(base + 0x58); /* 0x4B/0x51 */
    }
}

/* 0x29DB8. Write one animation variable; `value` is the original's EBX
 * (0x29DBC `mov eax, ebx`), the selector is EDX. The parent/child byte stores
 * (0x46..0x49, 0x4C..0x4F) save DS_00105BE8, the operand byte, not `value`
 * (0x29E6A/0x29E75); 0x4A/0x4B/0x50/0x51 take the value. */
void anim_write_var(u32 rec, u8 op, u32 value)
{
    u32 o = (u32)(op & 0x7fu);
    if (o < 0x40u) {
        DSW(DS_00105B4C + ((o + (u32)DSB(rec + 0x51)) & 0x3fu) * 2u) = (u16)value;
        return;
    }
    switch (o) {
    case 0x40: DSB(rec + 0x52) = (u8)value; return;
    case 0x41: DSB(rec + 0x53) = (u8)value; return;
    case 0x42: DSB(rec + 0x54) = (u8)value; return;
    case 0x43: DSB(rec + 0x55) = (u8)value; return;
    case 0x44: DSW(rec + 0x56) = (u16)(value & 0xffu); return;
    case 0x45: DSB(rec + 0x58) = (u8)value; return;
    default: break;
    }
    u32 base;
    if (o <= 0x4bu)
        base = DSD(DS_001014F4) + (u32)DSB(rec + 0x4a) * ACTOR_REC_SIZE;
    else if (o <= 0x51u)
        base = DSD(DS_001014F4) + (u32)DSB(rec + 0x4b) * ACTOR_REC_SIZE;
    else
        return;
    switch (o) {
    case 0x46: case 0x4c: DSB(base + 0x52) = (u8)DSW(DS_00105BE8); return;
    case 0x47: case 0x4d: DSB(base + 0x53) = (u8)DSW(DS_00105BE8); return;
    case 0x48: case 0x4e: DSB(base + 0x54) = (u8)DSW(DS_00105BE8); return;
    case 0x49: case 0x4f: DSB(base + 0x55) = (u8)DSW(DS_00105BE8); return;
    case 0x4a: case 0x50: DSW(base + 0x56) = (u16)(value & 0xffu); return;
    default:              DSB(base + 0x58) = (u8)value; return; /* 0x4B/0x51 */
    }
}

/* 0x2B8F8. Fetch the operand that follows an opcode word. The command word's
 * bits 0x2000/0x4000 choose the form: 0 none, a literal sign-extended byte;
 * 0x2000 a variable read; 0x4000 a variable read plus an inline dword that
 * becomes the stream base DS_00105BD4; 0x6000 a variable read whose index is
 * scaled by the next word's high nibble and dereferenced against rec+0x0C.
 * The decompiler drops the `*2`/`*4` scalings (0x2BA78/0x2BAA3/0x2BAC9); this
 * transcribes the raw arithmetic. */
static u32 anim_operand(u32 rec)
{
    u32 p = DSD(rec + 8);
    u16 cw = DSW(p);
    u16 mode = (u16)(cw & 0x6000u);
    DSW(DS_00105BE6) = mode;
    u8 op = (u8)DSW(DS_00105BE4);
    u8 ob = (u8)(cw & 0xffu);
    u32 cx;
    if (op == 0x1fu) {
        u32 np = p + 2;
        DSD(rec + 8) = np;
        DSW(DS_00105BE4) = ob;
        cx = DSW(np);
        if (mode == 0) {
            /* 0x2B932 -> 0x2BAE1 stores the operand word before returning. */
            DSW(DS_00105BE8) = (u16)cx;
            return cx;
        }
    } else {
        DSW(DS_00105BE8) = ob;
        if (mode == 0)
            /* 0x2BAE1 `and eax,0xffff`: the raw yields the 16-bit 0xFFxx, not
             * a 32-bit sign-extension. */
            return ob > 0x7fu ? (0xff00u | (u32)ob) : (u32)ob;
        cx = ob;
    }
    /* 0x2B96A */
    DSW(DS_00105BE8) = (u16)cx;
    u32 v = anim_read_var(rec, (u8)cx);
    u16 mode2 = DSW(DS_00105BE6);
    if (mode2 == 0x2000u) return v & 0xffffu;
    u32 p2 = DSD(rec + 8) + 2;
    u32 np = p2 + 2;
    DSD(rec + 8) = np;
    if (mode2 == 0x4000u) {
        DSD(DS_00105BD4) = DSD(p2);
        return v & 0xffffu;
    }
    u32 base = DSD(rec + 0x0c);
    DSD(DS_00105BD4) = base;
    u16 sel = (u16)(DSW(np) & 0xf000u);
    if (sel == 0x1000u)
        v = (v & 0xffffu) * 2u;
    else if (sel == 0x2000u)
        v = (v & 0xffffu) * 2u + 1u;
    else if (sel == 0x4000u) {
        base += (v & 0xffffu) * 2u;
        DSD(DS_00105BD4) = base;
        return DSW(base);
    } else if (sel == 0x5000u) {
        base += (v & 0xffffu) * 4u;
        DSD(DS_00105BD4) = base;
        return DSW(base);
    } else if (sel == 0x6000u) {
        base += (v & 0xffffu) * 4u + 2u;
        DSD(DS_00105BD4) = base;
        return DSW(base);
    } else {
        v &= 0xffffu;
    }
    u8 b = DSB(base + v);
    return b > 0x7fu ? ((0xff00u | (u32)b) & 0xffffu) : (u32)b;
}

/* 0x2A408. The sprite-id reader. `pset` is the second register argument; every
 * caller (0x2A39C, 0x2AE14, 0x2BC30, 0x2BCF4, 0x2BD44, 0x33F08) builds
 * DS_001014EC + slot*0x20 in EDX before the call and the function reads it at
 * 0x2A4A7 (`mov cx,[edx]`), which is the keep-current-id arm. */
u32 anim_next_sprite_id(u32 rec, u32 pset)
{
    u32 res;
    if ((DSW(rec + 0x28) >> 8 & 8u) != 0) {
        res = DSW(rec + 8);
    } else {
        u32 p = DSD(rec + 8);
        u16 word = DSW(p);
        res = word;
        if ((word & 0x8000u) != 0) {
            if ((word & 0x1f00u) == 0xd00u) {
                u32 e = p + 2;
                DSD(rec + 8) = e;
                if (((word >> 8) & 0x60u) == 0x40u) {
                    u32 rv = anim_read_var(rec, (u8)(word & 0x7fu));
                    res = (u32)DSW(e) + rv;
                } else {
                    DSD(rec + 8) = p + 4;
                    u32 rv = anim_read_var(rec, (u8)(word & 0x7fu));
                    u32 tab = DSD(p + 2);
                    res = DSW(tab + (rv & 0xffffu) * 2u);
                }
            } else {
                res = DSW(pset) & 0x7fffu;
            }
        }
    }
    u32 clearbit = (res & 0x8000u) == 0 ? 1u : 0u;
    u32 hflip    = ((DSW(rec + 0x28) >> 8) & 0x40u) == 0 ? 1u : 0u;
    if (clearbit == hflip) return res & 0x7fffu;
    return (res & 0x7fffu) | 0x8000u;
}

/* PORT: 0x2B2A0's opcodes 0x10/0x11/0x15 call the code pointer at
 * DS_00105BD4 through the original's indirect `call`. The port routes that
 * through fn_resolve and skips it until a later task registers the target.
 * The two title streams never dispatch these opcodes: the reached scripts are
 * `[id 0x023D][0x18 hold 0x1B][0x8100 end]` at 0x0E897A and
 * `[literal ids][0xCD40 id][0x18 hold 0x2F][ids][0x8100 end]` at 0x0E9116,
 * and the 0x10/0x11 words in that data region belong to other actors'
 * animation tables (referenced from 0xA17DC/0xA80xx/0xBB2xx). See
 * .superpowers/sdd/2026-09-17-actor-system/task-7-report.md. */
typedef void (*anim_code_fn)(u32 rec, u32 arg);
static void anim_indirect(u32 rec, u32 arg)
{
    anim_code_fn fn = (anim_code_fn)(void *)fn_resolve(DSD(DS_00105BD4));
    if (fn) fn(rec, arg);
}

/* 0x10FA8. The animation opcode 0x11 target reached on the logo streams after
 * sprite 0x01FD: spawns the RAGE continuation actor — descriptor 0x9AD08
 * (animation stream 0x0E88E6), layer 0xE4. The original (PRAGE.EXE obj0
 * file 0x63DFC) is `push ebx/ecx/edx; push 0; mov ecx,0xE4; mov eax,0x1AD08;
 * xor ebx,ebx; xor edx,edx; call 0x2AE14` — i.e. actor_spawn(desc, edx=0,
 * ecx=0xE4, ebx=0, [esp]=0) — and ignores the rec/arg the dispatcher passes.
 * 0x1AD08 is an LE-fixed data pointer, so it loads as mem + 0x9AD08. */
static void anim_code_10FA8(u32 rec, u32 arg)
{
    (void)rec;
    (void)arg;
    actor_spawn((const u32 *)(mem + 0x9AD08u), 0, 0xE4, 0, 0);
}

/* 0x12720. The animation opcode 0x11 target reached on the globe's first
 * presentation stream: the word at 0xE89A8 is `0xD100` (opcode 0x11, mode
 * 0x4000), so `anim_operand` loads the code pointer 0x12720 into
 * DS_00105BD4 and the dispatcher's indirect call reaches here. Spawns the
 * globe's fourth layer: a child of DS_000F0A58 (a5 = its pset slot | 0x400),
 * descriptor 0x9AC80 (stream 0x0E89F6, frame hold 7, layer 0xE2). The original
 * ignores the rec/arg the dispatcher passes. */
static void anim_code_12720(u32 rec, u32 arg)
{
    (void)rec;
    (void)arg;
    u32 first = DSD(DS_000F0A58);
    u32 a5 = ((u32)DSW(first + 0x56u) | 0x400u) & 0xFFFFu;
    actor_spawn((const u32 *)(mem + 0x9AC80u), 0u, 0xE2u, 0u, a5);
}

/* 0x37A58. The fighters' idle-animation tick, reached as an animation opcode
 * 0x10 target: the word 0xD000 in the character streams (0xE6DD2 the T-rex,
 * 0xD2136 the raptor) loads this address into DS_00105BD4 and the dispatcher's
 * indirect call reaches here. It advances the stream variable rec+0x52 by
 * rec+0x58 — the offset the 0xCD40 "id = next word + rec+0x52" form selects
 * with — draws rng(2) and flips rec+0x58 between +1 and 0xFF when rec+0x4C
 * expires, and in mode 6 with the variable back at 0 draws rng(3) and restarts
 * the character's idle stream from 0xBDAB8[char] (0 for the raptor, so it
 * skips). The dispatcher passed EAX=rec; the arg is ignored. */
#define DS_000BDAB8 0x000BDAB8u
static void anim_code_37A58(u32 rec, u32 arg)
{
    (void)arg;
    u8 c = (u8)(DSB(rec + 0x4cu) - 1u);
    DSB(rec + 0x4cu) = c;
    if ((s8)c < 0) {                                        /* 0x37A67 */
        DSB(rec + 0x58u) = (rng_next(2u) != 0) ? 1u : 0xffu; /* 0x37A6E/0x37A77 */
        DSB(rec + 0x4cu) = (u8)((DSB(rec + 0x4du) / 3u) * 3u); /* 0x37A98 */
    }
    u8 v = (u8)(DSB(rec + 0x52u) + DSB(rec + 0x58u));
    DSB(rec + 0x52u) = v;                                   /* 0x37AAB */
    if (DSW(DS_00104B00) == 6u && v == 0u) {                /* 0x37AAE */
        u32 slot = DSD(DS_001077A8 + (u32)DSB(rec + 0x51u) * 4u);
        if (slot == 0) return;                              /* 0x37AC5 -> ret */
        if (DSB(slot + 0x54u) == 0u && rng_next(3u) == 0u) { /* 0x37ACF/0x37AD6 */
            u32 stream = DSD(DS_000BDAB8 + (u32)DSB(slot + 0x7au) * 4u);
            if (stream != 0) actors_anim_begin(rec, stream, 0x40400000u);
        }
    }
    if ((s32)(s8)DSB(rec + 0x4fu) >= (s32)DSB(rec + 0x4du))
        DSB(rec + 0x52u) = 0;                               /* 0x37B12 */
    if ((s8)DSB(rec + 0x52u) < 0)
        DSB(rec + 0x52u) = (u8)(DSB(rec + 0x4du) - 1u);     /* 0x37B21 */
    u8 child = DSB(rec + 0x4bu);
    if (child != 0u)                                        /* 0x37B29 */
        DSB(DSD(DS_001014F4) + (u32)child * ACTOR_REC_SIZE + 0x52u)
            = DSB(rec + 0x52u);
}

/* 0x39A34. The roar stream's first opcode target (0xD100 = opcode 0x11, mode
 * 0x4000, operand 0x000A): rescale the record's animation frame hold rec+0x24
 * to the linked record's signed +0x7E over the opcode's zero-extended word
 * operand. The linked record is rec+0x14, the record's owner slot (the 0x3A504
 * setter writes its +0x7E). EAX = rec, EDX = operand. */
static void anim_code_39A34(u32 rec, u32 arg)
{
    u32 linked = DSD(rec + 0x14u);                          /* 0x39A3A */
    if (linked == 0u) return;                               /* 0x39A3F */
    {
        union { float f; u32 u; } fu;
        fu.f = (float)(s16)(s8)DSB(linked + 0x7Eu)          /* 0x39A41/0x39A52 */
             / (float)(u16)arg;                             /* 0x39A4C/0x39A59 */
        DSD(rec + 0x24u) = fu.u;                            /* 0x39A5B */
    }
}

/* 0x36870 — the animation-opcode target shape. PORT: anim_indirect calls every
 * code pointer as (rec, arg); the raw 0x36870 takes only EAX = rec (its first
 * use of EDX at 0x36876 is `XOR EDX,EDX`, and it returns with a plain RET), so
 * this wrapper drops the operand and calls fighter_36870(rec) unchanged. */
static void anim_code_36870(u32 rec, u32 arg)
{
    (void)arg;
    fighter_36870(rec);
}

/* 0x35E04 — the animation-opcode target shape. PORT: anim_indirect calls every
 * code pointer as (rec, arg); the raw 0x35E04 takes EAX = rec and does not
 * read EDX (it pushes EBX and EDX at 0x35E04/0x35E05 and loads EDX from
 * rec+0x14 at 0x35E06 before any read), so this wrapper drops the operand
 * and calls fighter_35e04(rec) unchanged. */
static void anim_code_35E04(u32 rec, u32 arg)
{
    (void)arg;
    fighter_35e04(rec);
}

/* 0x3E4E4 — the animation-opcode target shape. PORT: anim_indirect calls every
 * code pointer as (rec, arg); the raw 0x3E4E4 takes EAX = rec and does not
 * read EDX (it pushes EBX, ECX and EDX at 0x3E4E4..0x3E4E6 and loads EDX
 * with the stream at 0x3E4F0 before any read), so this wrapper drops the
 * operand and calls fighter_3e4e4(rec) unchanged. */
static void anim_code_3E4E4(u32 rec, u32 arg)
{
    (void)arg;
    fighter_3e4e4(rec);
}

/* 0x347B8 — the animation-opcode target shape. PORT: anim_indirect calls every
 * code pointer as (rec, arg); the raw 0x347B8 takes EAX = rec and does not
 * read EDX (it pushes EBX, ECX, EDX and ESI at 0x347B8..0x347BB and zeroes
 * EDX at 0x347CE before any read), so this wrapper drops the operand and
 * calls fighter_347b8(rec) unchanged. */
static void anim_code_347B8(u32 rec, u32 arg)
{
    (void)arg;
    fighter_347b8(rec);
}

/* 0x346F8 — the animation-opcode target shape. PORT: anim_indirect calls every
 * code pointer as (rec, arg); the raw 0x346F8 takes EAX = rec and does not
 * read EDX (it pushes EBX and EDX at 0x346F8/0x346F9 and overwrites EDX with
 * EAX at 0x346FD before any read), so this wrapper drops the operand and
 * calls fighter_346f8(rec) unchanged. */
static void anim_code_346F8(u32 rec, u32 arg)
{
    (void)arg;
    fighter_346f8(rec);
}

/* 0x35938 — the animation-opcode target shape. PORT: anim_indirect calls every
 * code pointer as (rec, arg); the raw 0x35938 takes EAX = rec and does not
 * read EDX (it pushes EBX, ECX and EDX at 0x35938..0x3593A and writes DL at
 * 0x35948 or EDX at 0x3596F/0x35993 before any read), so this wrapper drops
 * the operand and calls fighter_35938(rec) unchanged. */
static void anim_code_35938(u32 rec, u32 arg)
{
    (void)arg;
    fighter_35938(rec);
}

/* 0x4AC18 — the animation-opcode target shape. PORT: anim_indirect calls every
 * code pointer as (rec, arg); the raw 0x4AC18 takes EAX = rec and does not
 * read EDX (it pushes EBX and EDX at 0x4AC18/0x4AC19 and loads EDX from
 * rec+0x14 at 0x4AC1A before any read), so this wrapper drops the operand
 * and calls fight_4ac18(rec) unchanged. */
static void anim_code_4AC18(u32 rec, u32 arg)
{
    (void)arg;
    fight_4ac18(rec);
}

/* 0x4AC80 — the animation-opcode target shape. PORT: anim_indirect calls every
 * code pointer as (rec, arg); the raw 0x4AC80 takes EAX = rec and does not
 * read EDX (it pushes EDX at 0x4AC82 and loads EDX from rec+0x14 at 0x4AC8B
 * before any read), so this wrapper drops the operand and calls
 * fight_4ac80(rec) unchanged. */
static void anim_code_4AC80(u32 rec, u32 arg)
{
    (void)arg;
    fight_4ac80(rec);
}

/* 0x3D214 — the animation-opcode target shape. PORT: anim_indirect calls every
 * code pointer as (rec, arg); the raw 0x3D214 takes EAX = rec and does not
 * read EDX (it pushes EBX, ECX, EDX, ESI and EDI at 0x3D214..0x3D218 and
 * zeroes EDX at 0x3D233 before any read), so this wrapper drops the operand
 * and calls fighter_3d214(rec) unchanged. */
static void anim_code_3D214(u32 rec, u32 arg)
{
    (void)arg;
    fighter_3d214(rec);
}

/* 0x3D26C — the animation-opcode target shape. PORT: anim_indirect calls every
 * code pointer as (rec, arg); the raw 0x3D26C takes EAX = rec and does not
 * read EDX (it pushes EBX, ECX, EDX, ESI, EDI and EBP at 0x3D26C..0x3D271 and
 * zeroes EDX at 0x3D282 before any read), so this wrapper drops the operand
 * and calls fighter_3d26c(rec) unchanged. */
static void anim_code_3D26C(u32 rec, u32 arg)
{
    (void)arg;
    fighter_3d26c(rec);
}

/* PORT: TEST-ONLY, see actors.h. The opcode-8 draw is `on ? 0 : rng_next()`. */
static int anim_tick_zero;
void actors_pin_anim_tick_zero(int on) { anim_tick_zero = on; }

/* 0x2B2A0. The animation-opcode dispatcher (1623 bytes). The opcode is the
 * command word's high byte & 0x1F, or, for the 0x1F prefix, the operand byte
 * that anim_operand stores back into DS_00105BE4. `flag` is the original's EBX
 * on entry: 0x2AE14 passes 1, 0x2AA70 and 0x2BC30 pass 0; only opcode 0 reads
 * it. Returns 0 to keep walking, 1 to stop (opcode 0x0D) and 2 for death. */
static u32 spawn_anim_opcode(u32 rec, u32 index, u32 flag)
{
    u32 p = DSD(rec + 8);
    DSW(DS_00105BE4) = (u16)((DSW(p) >> 8) & 0x1fu);
    if ((u8)DSW(DS_00105BE4) == 0x0du) return 1;
    u32 value = anim_operand(rec);
    u8 op = (u8)DSW(DS_00105BE4);
    u16 ax = (u16)value;
    union { float f; u32 u; } fu;

    switch (op) {
    case 0x00:                                      /* 0x2B304 */
        (void)flag;                                 /* 0x2EA64 is a `ret` */
        set_dead(rec);
        /* fallthrough */
    case 0x01:                                      /* 0x2B314 */
        DSD(rec + 0x24) = 0;
        DSD(rec + 0x20) = DSD(rec + 0x24);
        return 2;
    case 0x02:                                      /* 0x2B330 */
        DSB(rec + 0x61) = (u8)value;
        return 0;
    case 0x03:                                      /* 0x2B343 */
        DSD(rec + 8) = DSD(DS_00105BD4) - 2u;
        return 0;
    case 0x04: {                                    /* 0x2B35A */
        u8 c = (u8)(DSB(rec + 0x50) + 1u);
        DSB(rec + 0x50) = c;
        if ((u32)c <= (u32)DSW(DS_00105BE8))
            DSD(rec + 8) = DSD(DS_00105BD4) - 2u;
        else
            DSB(rec + 0x50) = 0;
        return 0;
    }
    case 0x05:                                      /* 0x2B382 */
        if (ax != 0) return 0;
        DSD(rec + 0x20) = 0;
        DSB(rec + 0x28) |= 0x10;
        return 2;
    case 0x06: {                                    /* 0x2B3B2 */
        u32 edx = DSD(rec + 8) + 2u;
        DSD(rec + 8) = edx;
        u16 n1 = (u16)((u32)DSW(edx) + 1u);
        u16 cx = ax;
        if (cx != 0 && n1 >= cx) {
            u32 t = edx + ((u32)cx * 4u - 2u);
            DSD(rec + 8) = t;
            DSD(rec + 8) = DSD(t) - 2u;
        } else {
            DSD(rec + 8) = edx + ((u32)n1 * 4u);
        }
        return 0;
    }
    case 0x07:                                      /* 0x2B40F */
        fu.f = (float)(u32)ax;
        DSD(rec + 0x20) = fu.u;
        return 2;
    case 0x08:                                      /* 0x2B42E, the pin site */
        fu.f = (float)(u32)(anim_tick_zero ? 0u : rng_next(ax));
        DSD(rec + 0x20) = fu.u;
        return 2;
    case 0x09:                                      /* 0x2B387 */
        return 0;
    case 0x0a:                                      /* 0x2B452 */
        fu.f = (float)(s32)(s16)ax;
        { union { float f; u32 u; } b; b.u = DSD(rec + 0x24); fu.f += b.f; }
        DSD(rec + 0x24) = fu.u;
        return 0;
    case 0x0b:                                      /* 0x2B468 */
        fu.f = (float)(u32)ax;
        DSD(rec + 0x24) = fu.u;
        return 0;
    case 0x0c: {                                    /* 0x2B484 */
        u16 var = DSW(DS_00105BE8);
        u32 a5 = 0;
        if (var > 0) a5 = (var == 1) ? 0x400u : 0u;
        u32 e = DSD(rec + 8);
        u32 w0 = e + 2;
        e += 4;
        DSD(rec + 8) = e;
        s16 di = (s16)DSW(w0);
        s16 bx = (s16)DSW(e);
        u32 a2, a3, a4;
        if (a5 == 0x400u) {
            /* 0x2B4EB: a2 = (s16)word[p+2], a4 = (s16)word[p+4], a5 gets the
             * parent slot (`add eax,edx` with EDX = the pset index). */
            a2 = (u32)(s32)di;
            a3 = 0;
            a4 = (u32)(s32)bx;
            a5 = (u32)(index + 0x400u);
        } else {
            a5 = (u32)((DSW(rec + 0x28) >> 8) & 0x40u);
            a2 = (u32)((s32)di + (s32)DSD(rec + 0x18));
            a3 = (u32)((s32)DSD(rec + 0x30) >> 16);
            a4 = (u32)((s32)bx + (s32)DSD(rec + 0x1c));
        }
        DSD(DS_00105BD8) = rec;
        u32 child = actor_spawn((const u32 *)(mem + DSD(DS_00105BD4)),
                                a2, a3, a4, a5);
        if (child != 0) {
            DSW(child + 0x2a) |= (u16)((DSW(DS_000EF6DC) & 1u) | 4u);
            DSB(child + 0x51) = DSB(rec + 0x51);
        }
        return 0;
    }
    case 0x0d:                                      /* 0x2B52F */
    case 0x0e:                                      /* 0x2B534 */
        anim_write_var(rec, (u8)ax, 0);
        return 0;
    case 0x0f: {                                    /* 0x2B538 */
        if (DSW(DS_00105BE6) == 0) DSD(rec + 8) += 2u;
        u32 v = DSW(DSD(rec + 8));
        anim_write_var(rec, (u8)ax, v);
        return 0;
    }
    case 0x10:                                      /* 0x2B56B */
        anim_indirect(rec, index);
        return 0;
    case 0x11: {                                    /* 0x2B57F */
        u32 e = DSD(rec + 8) + 2u;
        DSD(rec + 8) = e;
        anim_indirect(rec, DSW(e));
        return 0;
    }
    case 0x12:                                      /* 0x2B5A4 */
        DSB(rec + 0x4e) = 1;
        DSW(rec + 0x2e) = (u16)(value << 4);
        return 0;
    case 0x13:                                      /* 0x2B5BB */
        DSB(rec + 0x59) = (u8)value;
        return 0;
    case 0x14:                                      /* 0x2B5CE */
        DSB(rec + 0x29) ^= 0x40;
        return 0;
    case 0x15:                                      /* 0x2B5E3 */
        /* 0x2B5E3 sets ECX=2 before the call and copies it to EAX after
         * (`mov eax,ecx`), so the walk sees 2, not 0. */
        anim_indirect(rec, index);
        return 2;
    case 0x16:                                      /* 0x2B5FA */
        DSB(rec + 0x29) &= (u8)~0x02u;
        return 0;
    case 0x17:                                      /* 0x2B60F */
        anim_write_var(rec, (u8)DSW(DS_00105BE8), (u16)(value + 1u));
        return 0;
    case 0x18: {                                    /* 0x2B638 */
        u16 cx = (u16)(value + 1u);
        anim_write_var(rec, (u8)DSW(DS_00105BE8), cx);
        u32 edi = DSD(rec + 8) + 2u;
        DSD(rec + 8) = edi;
        u16 d = DSW(edi);
        if ((u32)d > (u32)cx) {
            u32 a = edi + 2u;
            DSD(rec + 8) = a;
            DSD(rec + 8) = DSD(a) - 2u;
        } else {
            DSD(rec + 8) = edi + 4u;
        }
        return 0;
    }
    case 0x19: {                                    /* 0x2B68C */
        u16 cx = (u16)(value - 1u);
        anim_write_var(rec, (u8)DSW(DS_00105BE8), cx);
        u32 ebx = DSD(rec + 8) + 2u;
        DSD(rec + 8) = ebx;
        u16 d = DSW(ebx);
        if ((s32)(s16)cx < (s32)d) {
            DSD(rec + 8) = ebx + 4u;
        } else {
            u32 a = ebx + 2u;
            DSD(rec + 8) = a;
            DSD(rec + 8) = DSD(a) - 2u;
        }
        return 0;
    }
    case 0x1a:                                      /* 0x2B6E8 */
        DSD(rec + 0x0c) = DSD(DS_00105BD4);
        return 0;
    case 0x1b:                                      /* 0x2B6FC */
        DSD(rec + 0x0c) = 0;
        return 0;
    case 0x1c: {                                    /* 0x2B70F */
        DSD(rec + 0x10) = DSD(DS_00105BD4);
        u32 pb = DSD(rec + 0x10) + (u32)
                 ((s32)DSD(rec + 0x4f) >> 24);
        fu.f = (float)(u32)DSB(pb);
        DSD(rec + 0x20) = fu.u;
        DSD(rec + 0x24) = fu.u;
        DSB(rec + 0x2b) |= 0x04;
        return 0;
    }
    case 0x1d:                                      /* 0x2B749 */
        DSD(rec + 0x10) = 0;
        return 0;
    case 0x1e:                                      /* 0x2B75C */
        DSB(rec + 0x2b) &= (u8)~0x04u;
        return 0;
    case 0x1f:                                      /* 0x2B771 */
        /* fallthrough: 0x2EA64 is a `ret` */
    case 0x20:                                      /* 0x2B776 */
        if ((DSW(rec + 0x28) >> 8 & 0x40u) != 0)
            DSD(rec + 0x18) = (u32)((s32)DSD(rec + 0x18)
                                    - (s32)(s16)ax * 64);
        else
            DSD(rec + 0x18) = (u32)((s32)DSD(rec + 0x18)
                                    + (s32)(s16)ax * 64);
        return 0;
    case 0x21:                                      /* 0x2B7B8 */
        DSD(rec + 0x1c) = (u32)((s32)DSD(rec + 0x1c)
                                + (s32)(s16)ax * 64);
        return 0;
    case 0x22:                                      /* 0x2B7D0 */
        DSW(rec + 0x32) = (u16)(DSW(rec + 0x32)
                                + (u16)((value << 6) & 0xffffu));
        return 0;
    case 0x25:                                      /* 0x2B7E9 */
        DSW(rec + 0x34) = (u16)(DSW(rec + 0x34) + ax);
        return 0;
    case 0x26:                                      /* 0x2B7FF */
        DSW(rec + 0x36) = (u16)(DSW(rec + 0x36) + ax);
        return 0;
    case 0x27:                                      /* 0x2B815 */
        DSW(rec + 0x38) = (u16)(DSW(rec + 0x38) + ax);
        return 0;
    case 0x28:                                      /* 0x2B825 */
        if ((DSW(rec + 0x28) >> 8 & 0x40u) != 0)
            DSW(rec + 0x34) = (u16)(-value);
        else
            DSW(rec + 0x34) = ax;
        return 0;
    case 0x29: DSW(rec + 0x36) = ax; return 0;      /* 0x2B850 */
    case 0x2a: DSW(rec + 0x38) = ax; return 0;      /* 0x2B860 */
    case 0x2b: DSW(rec + 0x2c) = ax; return 0;      /* 0x2B870 */
    case 0x2c:                                      /* 0x2B880 */
        DSW(rec + 0x2c) = (u16)(DSW(rec + 0x2c) + ax);
        return 0;
    case 0x2d: {                                    /* 0x2B896 */
        /* 0x2B89F reads a dword at pset+0x0A then `sar 0x10` (0x2B8A3): the
         * compared value is the sign-extended word at pset+0x0C. */
        s32 pv = (s32)(s16)DSW(DSD(DS_001014EC)
                               + index * PSET_SIZE + 0x0c);
        if (pv < (s32)ax) {
            DSD(rec + 8) += 2u;
            DSD(rec + 8) = DSD(DSD(rec + 8)) - 2u;
        } else {
            DSD(rec + 8) += 4u;
        }
        return 0;
    }
    case 0x2e:                                      /* 0x2B8D2 */
        /* PORT: 0x2B2A0's opcode 0x2E calls 0x2C3FC (1268-byte effect/voice
         * subsystem), out of this cycle's scope. Not silently dropped: the call
         * is documented here and listed in the Task 7 report. */
        return 0;
    default:                                        /* 0x2B8E8 */
        /* PORT: table opcodes 0x23 and 0x24 both point at 0x2B8E8, as does
         * every opcode above 0x2E. The only effect there is the inert
         * 0x2EA64 (`ret`), so these are named rather than silently skipped. */
        return 0;
    }
}

/* 0x2BC30. Point a record at `stream`, reset its animation cursor and cache,
 * pre-walk its commands, then load the first sprite id. `frame_bits` is the
 * original's third stack argument, stored verbatim into rec+0x24/rec+0x20
 * (0x2BC8B `mov [ecx+0x24], eax`). The original callers pass IEEE-754 float
 * bit patterns (0x40400000 at 0x12C13/0x14D5E, 0x3F800000 at 0x154C0,
 * 0x40000000 at 0x155BF), so the port passes and stores those dwords raw. */
void actors_anim_begin(u32 rec, u32 stream, u32 frame_bits)
{
    DSD(rec + 0x0c) = 0;
    DSD(rec + 0x10) = 0;
    DSB(rec + 0x52) = 0;
    DSB(rec + 0x50) = 0;
    DSB(rec + 0x61) = 0;
    DSD(rec + 8) = stream;
    DSW(rec + 0x28) &= 0xf7ebu;
    DSB(rec + 0x2b) &= (u8)~0x04u;
    DSD(rec + 0x24) = frame_bits;
    DSD(rec + 0x20) = frame_bits;
    for (;;) {
        if (((DSW(DSD(rec + 8)) >> 8) & 0x80u) == 0) break;
        u32 st = spawn_anim_opcode(rec, DSW(rec + 0x56), 0);
        if (st != 0) {
            if (st != 1) DSD(rec + 8) += 2u;
            break;
        }
        DSD(rec + 8) += 2u;
    }
    u32 pset = DSD(DS_001014EC) + (u32)DSW(rec + 0x56) * PSET_SIZE;
    DSW(pset) = (u16)anim_next_sprite_id(rec, pset);
}

/* 0x2BCF4. Point a record at `stream` and load its first sprite id. */
void actors_anim_seek(u32 rec, u32 stream)
{
    DSD(rec + 8) = stream;
    DSB(rec + 0x28) &= (u8)~0x14u;
    u32 pset = DSD(DS_001014EC) + (u32)DSW(rec + 0x56) * PSET_SIZE;
    DSW(pset) = (u16)anim_next_sprite_id(rec, pset);
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
 * into rec+0x3C. Exported for the game_frame tail's per-fighter sync (0x25443). */
void actor_pset_point(u32 rec)
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
            /* 0x2A733 `mov eax,[ebx+0x44]` / 0x2A739 `sar eax,0x10`: the
             * signed word at +0x46, the ramp entry just stored (0x2A72F). */
            x = DSD(rec + 0x18) + 0x2a00u
                - (u32)(((s32)DSD(rec + 0x44) >> 16) * 2);
        } else if (DSW(rec + 0x34) == 0) {
            x = DSD(rec + 0x18) + 0x2a00u
                - (u32)((s32)DSD(DS_00107A44) >> 16);
        } else {
            /* 0x2A6E0 `mov eax,[ebx+0x32]` / 0x2A6E9 `sar eax,0x10`: the
             * signed word at +0x34, the one 0x2A6D9 gates on. */
            s32 p = ((s32)DSD(DS_00107A44) >> 16) * ((s32)DSD(rec + 0x32) >> 16);
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
        /* PORT: 0x2A7C5 `and eax,0xffff` zero-extends the layer before
         * 0x2A7CA's signed `jle` clamp, so signed and unsigned coincide and
         * the clamp is correct. */
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

/* 0x2BE5C. The mode-1 sibling of 0x2A690: rec+0x1C takes the pset y and the
 * record's 16.16 vertical position; when rec+0x28 bit 12 is set the mode-1 arm
 * runs (pset+0x14 = pset+8, 0x2A620 mode1_cursor, the 0x107900 ramp entry into
 * rec+0x46, rec+0x18 from pset+4 and the 16.16 rec+0x44), otherwise rec+0x18
 * comes from pset+4 and DS_000F0AF0. Clears rec+0x29 bit 5. EAX = rec on entry
 * (the raw's register argument; the function returns void). Called
 * by the type-0x19/0x0A cb1 tails (0x28FB5/0x2906D), which set rec+0x29 bit 4
 * (rec+0x28 bit 12) just before the call, so the mode-1 arm is the one the
 * dispatch reaches. Exposed for its unit test. */
void actor_mode1_pset(u32 rec)
{
    u32 pset = actor_pset(rec);
    DSD(rec + 0x1c) = (DSD(DS_000F0AEC) + 0x3bc0u - DSD(pset + 8))
                    - (u32)((s32)DSD(rec + 0x30) >> 16);
    if ((DSW(rec + 0x28) & 0x1000u) != 0) {
        DSD(pset + 0x14) = DSD(pset + 8);
        mode1_cursor(rec, pset);
        DSW(rec + 0x46) = DSW(DS_00107900
                             + (u32)((s32)DSD(rec + 0x61) >> 24) * 2u);
        DSD(rec + 0x18) = DSD(pset + 4)
                        + (u32)((s32)DSD(rec + 0x44) >> 16) * 2u
                        - 0x2a00u;
    } else {
        DSD(rec + 0x18) = DSD(DS_000F0AF0) + (DSD(pset + 4) - 0x2a00u);
    }
    DSB(rec + 0x29) &= (u8)~0x20u;
}

/* 0x2A820. The pset position/layer writer: 0x2A690 for a free record, the
 * parent-relative form for a child, and the on-screen visibility test. */
static void pset_write(u32 rec, u32 pset)
{
    if ((DSW(rec + 0x28) >> 8 & 0x20u) == 0) {
        if ((DSW(rec + 0x28) >> 8 & 0x04u) == 0) {
            actor_pset_point(rec);
        } else {
            u32 parent = DSD(DS_001014F4)
                       + (u32)DSB(rec + 0x4a) * ACTOR_REC_SIZE;
            if ((DSW(parent + 0x28) & 8u) != 0) goto dead;
            u32 pp = DSD(DS_001014EC)
                   + (u32)DSW(parent + 0x56) * PSET_SIZE;
            u32 x = DSD(pp + 4) + (u32)(((s32)DSD(rec + 0x32) >> 16) * 64);
            DSD(pset + 4) = x;
            DSD(DS_00105BDC) = x;
            if ((DSW(rec + 0x28) & 0x40u) == 0)
                DSD(pset + 8) = DSD(pp + 8)
                    + (u32)(((s32)DSD(rec + 0x34) >> 16) * 64);
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
                + (u32)(((s32)DSD(rec + 0x32) >> 16) * 64);
            DSD(rec + 0x1c) = DSD(parent + 0x1c)
                + (u32)(((s32)DSD(rec + 0x34) >> 16) * 64);
            DSW(rec + 0x2c) = DSW(parent + 0x2c);
        }
        DSD(pset + 4) = DSD(rec + 0x18);
        DSD(DS_00105BDC) = DSD(rec + 0x18);
        DSD(pset + 8) = DSD(rec + 0x1c);
        DSD(DS_00105BE0) = DSD(rec + 0x1c);
        DSW(pset + 0x0c) = DSW(rec + 0x2c);
        /* PORT: 0x2A8D9 `mov dx,cx` zero-extends the layer before 0x2A8DE's
         * signed `jle` clamp, so signed and unsigned coincide and the clamp is
         * correct. */
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
    /* PORT: the original integrates the x velocity as `sar dword [eax+0x32],16`
     * and the y velocity as `sar dword [eax+0x34],16` — the 16.16 fixed-point
     * integer step (0x2A516/0x2A524), NOT the low 16-bit word. 0x121A0 stores
     * the logo's -iVar2/0x5F at +0x34 and (iVar1<<6)/0x5F at +0x36, so reading
     * +0x32/+0x34 as words would move the logo by (0,-81) instead of the
     * original's (-81,+8). pset_write already reads the dwords the same way. */
    DSD(rec + 0x18) = (u32)((s32)DSD(rec + 0x18) + ((s32)DSD(rec + 0x32) >> 16));
    DSD(rec + 0x1c) = (u32)((s32)DSD(rec + 0x1c) + ((s32)DSD(rec + 0x34) >> 16));
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

/* 0x2A39C. Clear the +0x28 0x04 bit and write pset+0 from the reader, or-ing
 * the parent's hflip into bit 0x8000. */
static void anim_id_path(u32 rec, u32 slot)
{
    DSB(rec + 0x28) &= 0xfb;
    u32 pset = DSD(DS_001014EC) + slot * PSET_SIZE;
    u32 id = anim_next_sprite_id(rec, pset);
    if (DSB(rec + 0x4a) != 0) {
        u32 parent = DSD(DS_001014F4)
                   + (u32)DSB(rec + 0x4a) * ACTOR_REC_SIZE;
        if ((DSW(parent + 0x28) >> 8 & 0x40u) == 0) id &= 0x7fffu;
        else                                        id |= 0x8000u;
    }
    DSW(pset) = (u16)id;
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
            status = spawn_anim_opcode(rec, slot, 0);   /* 0x2AA70 passes EBX=0 */
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

/* 0x2A17C. EAX=rec, EDX=word, EBX=handle (pinned by the raw: 0x2A17E copies EAX
 * to ECX and 0x2A192 copies EDX to EAX before the pset+2 OR). The spawn's
 * 0x29BC8 passes word 0 and the character's palette handle, so an existing
 * entry (the descriptor's) is released before the new one is acquired. A zero
 * handle returns at 0x2A1AC leaving pset+0x18 unchanged: the 0x2A1F5 store is
 * dead, reached only from 0x2A1E6's `test ebx,ebx / je` inside the guarded
 * acquire path. */
void actor_pset_palette(u32 rec, u32 word, u32 handle)
{
    u32 pset = actor_pset(rec);                             /* 0x2A182..0x2A190 */
    DSW(pset + 0x02u) = (u16)(word | (DSB(rec + 0x5fu) != 0 ? 0x800u : 0u));
    if (handle == 0) return;                                /* 0x2A1AA/0x2A1AC */
    u32 old = DSD(pset + 0x18u);                            /* 0x2A1BF */
    if (old != 0) {                                         /* 0x2A1C2 */
        palette_release(old);                               /* 0x2A1C8 0x33864 */
        DSD(pset + 0x18u) = 0;                              /* 0x2A1CD */
    }
    DSD(pset + 0x18u) = palette_acquire(handle);            /* 0x2A1EA/0x2A1EF */
}

/* 0x2B150. Set the dead bit (0x28 0x08), release the pset palette and unlink
 * the pset from the render list. 63 callers in the original; the port reaches
 * it from the sync path (0x2A1FC's release_record) and from the type-0x20..0x25
 * teardown (0x49444). */
static void set_dead(u32 rec)
{
    DSB(rec + 0x28) |= 0x08;
    if ((DSW(rec + 0x2a) >> 8 & 0x40u) != 0) {
        /* 0x2B185: cb2 = DS_000BB9E0[type * 0xC], called with EAX = rec; its
         * return is discarded. The stub 0x5D812 is unregistered, so its
         * fn_resolve miss is skipped, and 0x2B18B clears rec+0x2b 0x40 either
         * way. */
        actor_type_cb2 cb2 = (actor_type_cb2)(void *)fn_resolve(
            DSD(DS_000BB9E0 + (u32)DSB(rec + 0x48) * 0xCu));
        /* PORT: the raw calls the table entry unconditionally; the stub
         * 0x5D812 is `XOR EAX,EAX; RET` and its return is discarded, so a
         * fn_resolve miss (the stub) is skipped — same state either way. */
        if (cb2 != NULL) cb2(rec);
        DSB(rec + 0x2b) &= (u8)~0x40u;
    }
    u32 pset = actor_pset(rec);
    if (DSD(pset + 0x18) != 0) {
        palette_release(DSD(pset + 0x18));
        DSD(pset + 0x18) = 0;
    }
    render_list_remove(pset);                   /* 0x1C458 + 0x1C3D0 */
}

/* Exposed for 0x121A0's phase-1 retirement of the logo and the second object
 * (the same 0x2B150 the sync path reaches internally). */
void actor_set_dead(u32 rec) { set_dead(rec); }

/* ---- the per-type callbacks (0xBB9DC cb1 / 0xBB9E0 cb2) ----------------
 * The 16 non-stub entries of the type table. Every body is the raw's; the
 * data addresses are Ghidra's (fixup-applied) values. The five cb1s that pop
 * a list (0x127C0, 0x198E8, 0x28F64, 0x2901C, 0x48CD8) return 0xFF when it
 * is empty, else they link the popped node at rec+0x14 and re-insert it at
 * the destination's head (0x249B0 insert-after; 0x249C0 is insert-before). */

/* 0x127C0. Type 0x01: pop the 0xF0A78 head, insert it at the 0xF0AE0 head. */
static u8 actor_type_127C0(u32 rec, u32 slot)
{
    (void)slot;
    u32 rec2 = list_head(DS_000F0A78);
    if (rec2 == 0) return 0xff;
    list_unlink(rec2);
    DSD(rec2 + 8) = rec;
    DSD(rec + 0x14) = rec2;
    list_insert_after(DS_000F0AE0, rec2);
    return 0;
}

/* 0x12800. Type 0x01's teardown: return the rec+0x14 node to 0xF0A78. */
static void actor_type_12800(u32 rec)
{
    u32 rec2 = DSD(rec + 0x14);
    if (rec2 == 0) return;
    list_unlink(rec2);
    list_insert_after(DS_000F0A78, DSD(rec + 0x14));
    DSD(rec + 0x14) = 0;
}

/* 0x198E8. Types 0x06/0x26/0x27/0x28: pop 0x100C20, insert at 0x100C28. */
static u8 actor_type_198E8(u32 rec, u32 slot)
{
    (void)slot;
    u32 rec2 = list_head(DS_00100C20);
    if (rec2 == 0) return 0xff;
    list_unlink(rec2);
    DSD(rec2 + 8) = rec;
    DSD(rec + 0x14) = rec2;
    list_insert_after(DS_00100C28, rec2);
    return 0;
}

/* 0x19928. Types 0x06/0x26/0x27/0x28's teardown: return the node to 0x100C20
 * and clear the type. */
static void actor_type_19928(u32 rec)
{
    u32 rec2 = DSD(rec + 0x14);
    if (rec2 == 0) return;
    list_unlink(rec2);
    list_insert_after(DS_00100C20, DSD(rec + 0x14));
    DSD(rec + 0x14) = 0;
    DSB(rec + 0x48) = 0;
}

/* 0x28F64. Type 0x19: pop 0x104888, insert at 0x104880, then 0x2BE5C and
 * the two rng draws: rec+0x34 takes rng(0x20)+0x20 (negated when rec+0x28 bit
 * 14 is set, with rec+0x29 0x40 set) and rec+0x36 takes rng(0x80)+0xC0. */
static u8 actor_type_28F64(u32 rec, u32 slot)
{
    (void)slot;
    u32 rec2 = list_head(DS_00104888);
    if (rec2 == 0) return 0xff;
    list_unlink(rec2);
    list_insert_after(DS_00104880, rec2);
    DSB(rec2 + 0x0c) = 0;
    DSW(rec + 0x32) = DSW(DS_000BD898);
    DSD(rec2 + 8) = rec;
    DSB(rec + 0x29) |= 0x10;
    DSD(rec + 0x14) = rec2;
    actor_mode1_pset(rec);                              /* 0x2BE5C */
    u32 ecx = rng_next(0x20u) + 0x20u;
    u32 eax = rng_next(0x80u) + 0xc0u;
    if ((DSW(rec + 0x28) & 0x4000u) != 0) {
        DSW(rec + 0x34) = (u16)(0u - ecx);
        DSB(rec + 0x29) |= 0x40;
    } else {
        DSW(rec + 0x34) = (u16)ecx;
    }
    DSW(rec + 0x44) = 0x0c;
    DSW(rec + 0x36) = (u16)eax;
    DSB(DS_00104AE8) |= 0x80;
    return 0;
}

/* 0x2901C. Type 0x0A: the same head; both draws are rng(0x80) (no +0x20) and
 * the bit-14 polarity is reversed: set takes CX, clear negates and sets
 * rec+0x29 0x40. */
static u8 actor_type_2901C(u32 rec, u32 slot)
{
    (void)slot;
    u32 rec2 = list_head(DS_00104888);
    if (rec2 == 0) return 0xff;
    list_unlink(rec2);
    list_insert_after(DS_00104880, rec2);
    DSB(rec2 + 0x0c) = 0;
    DSW(rec + 0x32) = DSW(DS_000BD898);
    DSD(rec2 + 8) = rec;
    DSB(rec + 0x29) |= 0x10;
    DSD(rec + 0x14) = rec2;
    actor_mode1_pset(rec);                              /* 0x2BE5C */
    u32 ecx = rng_next(0x80u);
    u32 eax = rng_next(0x80u) + 0xc0u;
    if ((DSW(rec + 0x28) & 0x4000u) != 0) {
        DSW(rec + 0x34) = (u16)ecx;
    } else {
        DSW(rec + 0x34) = (u16)(0u - ecx);
        DSB(rec + 0x29) |= 0x40;
    }
    DSW(rec + 0x44) = 0x0c;
    DSW(rec + 0x36) = (u16)eax;
    DSB(DS_00104AE8) |= 0x80;
    return 0;
}

/* 0x290D0. Types 0x0A/0x19's teardown: return the node to 0x104888. */
static void actor_type_290D0(u32 rec)
{
    u32 rec2 = DSD(rec + 0x14);
    if (rec2 == 0) return;
    list_unlink(rec2);
    list_insert_after(DS_00104888, DSD(rec + 0x14));
    DSD(rec + 0x14) = 0;
}

/* 0x48CD8. Type 0x2D: pop 0x1082E0, insert at 0x108368, then the 0x104AE8
 * bit 1 and the 0x108398 counter. */
static u8 actor_type_48CD8(u32 rec, u32 slot)
{
    (void)slot;
    u32 rec2 = list_head(DS_001082E0);
    if (rec2 == 0) return 0xff;
    list_unlink(rec2);
    list_insert_after(DS_00108368, rec2);
    DSB(rec2 + 0x0c) = 0;
    DSD(rec2 + 8) = rec;
    DSD(rec + 0x14) = rec2;
    DSB(DS_00104AE8) |= 0x02;
    DSB(DS_00108398) = (u8)(DSB(DS_00108398) + 1u);
    return 0;
}

/* 0x48D3C. Type 0x2D's teardown: return the node to 0x1082E0, decrement the
 * 0x108398 counter and clear the 0x104AE8 bit 1 on its 0xFF underflow. */
static void actor_type_48D3C(u32 rec)
{
    u32 rec2 = DSD(rec + 0x14);
    if (rec2 == 0) return;
    list_unlink(rec2);
    list_insert_after(DS_001082E0, DSD(rec + 0x14));
    u8 c = (u8)(DSB(DS_00108398) - 1u);
    DSD(rec + 0x14) = 0;
    DSB(DS_00108398) = c;
    if (c == 0xffu) DSB(DS_00104AE8) &= (u8)~0x02u;
}

/* 0x3B9C4. Types 0x02/0x03/0x04/0x05/0x08's teardown: zero the node's +8 and
 * +0x64. */
static void actor_type_3B9C4(u32 rec)
{
    u32 rec2 = DSD(rec + 0x14);
    if (rec2 == 0) return;
    DSD(rec2 + 8) = 0;
    DSB(rec2 + 0x64) = 0xff;
}

/* 0x3D784. Type 0x09's teardown: `mov eax,0x4f; jmp 0x2c3fc`, the 1268-byte
 * voice dispatcher the port carries as an out-of-scope stub. cb2's return is
 * discarded. */
static void actor_type_3D784(u32 rec)
{
    (void)rec;
    /* PORT: 0x2C3FC(0x4F) voice, out of scope (spec §7). */
}

/* 0x3FC90. Type 0x10's teardown: clear the 0x108080 table entry named by the
 * node's +0x51. */
static void actor_type_3FC90(u32 rec)
{
    u32 rec2 = DSD(rec + 0x14);
    if (rec2 == 0) return;
    DSD(DS_00108080 + (u32)DSB(rec2 + 0x51) * 4u) = 0;
}

/* 0x40684. Type 0x1A's teardown: return the node to 0x107EF8 and clear the
 * type. */
static void actor_type_40684(u32 rec)
{
    u32 rec2 = DSD(rec + 0x14);
    if (rec2 == 0) return;
    list_unlink(rec2);
    list_insert_after(DS_00107EF8, DSD(rec + 0x14));
    DSD(rec + 0x14) = 0;
    DSB(rec + 0x48) = 0;
}

/* 0x412F0. Type 0x16: rec+0x34 = 0x200, visible. */
static u8 actor_type_412F0(u32 rec, u32 slot)
{
    (void)slot;
    DSW(rec + 0x34) = 0x200;
    return 0;
}

/* 0x412FC. Type 0x1B: rec+0x34 = 0x140, visible. */
static u8 actor_type_412FC(u32 rec, u32 slot)
{
    (void)slot;
    DSW(rec + 0x34) = 0x140;
    return 0;
}

/* 0x49444. Types 0x20..0x25's teardown: clear the 0x10839C entry named by the
 * node's 16.16 +0x18 when its +0x1C bit 1 is set, retire the node's +0x10
 * child, then return the node to 0x1083C4. */
static void actor_type_49444(u32 rec)
{
    u32 rec2 = DSD(rec + 0x14);
    if (rec2 == 0) return;
    if ((DSW(rec2 + 0x1c) & 2u) != 0)
        DSD(DS_0010839C
            + (u32)((s32)DSD(rec2 + 0x18) >> 16) * 4u) = 0;
    if (DSD(rec2 + 0x10) != 0) {
        set_dead(DSD(rec2 + 0x10));                     /* 0x2B150 */
        DSD(rec2 + 0x10) = 0;
    }
    list_unlink(DSD(rec + 0x14));
    list_insert_after(DS_001083C4, DSD(rec + 0x14));
    DSD(rec + 0x14) = 0;
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

/* 0x2A1FC. Per-record sync: timer, anim-id, pset-id hold, motion then write.
 * Exposed for 0x35658's 0x35813 call (the state-7 arena's fighter sync). */
void actor_sync(u32 rec)
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
                actor_sync(rec);
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

    /* 0x2AFFA: initial animation-stream walk. `spawn_anim_opcode` consumes the
     * leading command words; when it returns 2 (the 0x1F prefix, opcode 0 or
     * 1) the engine's own id 0x1E1 replaces the stream id, else
     * `anim_next_sprite_id` reads the literal/computed id. */
    u32 id = 0;
    int have_id = 0;
    if ((DSW(rec + 0x28) >> 8 & 8) == 0) {
        DSD(rec + 0x08) = DSD(rec + 0x08) - 2;
        u32 status = 0;
        do {
            u32 p = DSD(rec + 0x08) + 2;
            DSD(rec + 0x08) = p;
            if ((DSW(p) >> 8 & 0x80) == 0) break;
            status = spawn_anim_opcode(rec, index, 1);   /* 0x2AE14 passes EBX=1 */
        } while (status == 0);
        if (status == 2) { id = 0x1e1u; have_id = 1; }   /* status 2 -> 0x1E1 */
    }
    if (!have_id) id = anim_next_sprite_id(rec, pset);
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

    /* 0x2B0D4: the per-type render check calls cb1 = DS_000BB9DC[type * 0xC]
     * with (rec, slot) and tests the returned AL as a whole byte: non-zero
     * marks the record dead and clears its type. Every non-stub entry the
     * table holds is registered in actors_init, so the else arm is the stub's
     * alone. */
    u32 cb = DSD(DS_000BB9DC + (u32)DSB(rec + 0x48) * 0xCu);
    actor_type_cb1 cb1 = (actor_type_cb1)(void *)fn_resolve(cb);
    u8 visible;
    if (cb1 != NULL)
        visible = cb1(rec, index) == 0;
    else
        /* PORT: the raw calls the table entry unconditionally; the stub
         * 0x5D812 is `XOR EAX,EAX; RET`, so the identity test stands in for
         * the call and its AL == 0 (visible). */
        visible = cb == FN_0005D812;
    if (!visible) {
        DSB(rec + 0x48) = 0;
        DSW(rec + 0x28) |= 8;
        return 0;
    }
    DSB(rec + 0x2b) |= 0x40;
    if ((a5 & 0x400u) == 0) DSB(rec + 0x4a) = 0;
    render_list_insert(pset);                   /* 0x1C390 + 0x1C3A0 */
    return rec;
}

/* ---- text renderer and record grid -------------------------------------
 * (0x2F0F0, 0x2F198, 0x2F280, 0x2F4BC, 0x2F5A0, 0x2F830)
 *
 * PORT: plan Format reference H names this group "pset layer select and
 * support". The shipped machine is not a pset layer writer: 0x2F0F0 measures a
 * display string, 0x2F198/0x2F4BC move the text cursor at DS_00105F34, and
 * 0x2F830 lays the string out through 0x2F5A0, which spawns each non-space
 * glyph as an actor (0x2AE14) into the grid; 0x2F280 clears a run of cells in
 * the 31x43 actor-record pointer grid at
 * DS_00105F38 (which actors_reset zeroes via 0x2F920), releasing each record
 * through 0x2AD40. The layer at pset+0x0E is written only by the sync:
 * 0x2A690 uses (s8)rec+0x59 + 0xF0 (0x2A7DC `movsx dx,[ebx+0x59]`; 0x2A7E1
 * `add edx,0xf0`; 0x2A7D6 `mov [esi+0xe],dx`), 0x2A820 uses
 * rec+0x49 + (s8)rec+0x59 (0x2A8CD `mov cl,[ebx+0x49]`; 0x2A8D0
 * `movsx ax,[ebx+0x59]`; 0x2A8D7 `add ecx,eax`; 0x2A8EB `mov [esi+0xe],ax`),
 * and 0x2AD40 zeroes it. rec+0x5A is written by spawn from a5's high 16
 * (0x2AF26 `mov al,[esp+0x10]`; 0x2AF2A `mov [ecx+0x5a],al`) but is never read
 * by the layer path, so Format references B and H overstate it. */

/* 0x2F0F0. EAX = byte string, EDX = mode; pinned at the three internal call
 * sites (0x2F1CD/0x2F243/0x2F28A are `mov eax,ebx; mov edx,ecx`). Mode & 3 in
 * {0,1} returns strlen (the `repne scasb` path). Modes 2/3 sum a per-character
 * class weight: class = (s8)(dword at 0xBD38D + c >> 24) = DSB(0xBD390 + c); a
 * negative class is skipped; otherwise the width byte at table + class*4 + 2
 * counts 1 when it is 8, else 2. The tables live in the original data object
 * (DS 0x3D048 / 0x3D1EC). */
int text_width(const u8 *s, u32 mode)
{
    mode &= 3u;
    const u32 table = (mode == 2u) ? 0xbd048u : 0xbd1ecu;
    if (mode != 2u && mode != 3u)
        return (int)strlen((const char *)s);
    int n = 0;
    for (;;) {
        u8 c = *s++;
        if (c == 0) return n;
        s32 cls = (s8)DSB(0xbd390u + c);
        if (cls < 0) continue;
        u8 w = DSB(table + (u32)cls * 4u + 2u);
        n += (w == 8u) ? 1 : 2;
    }
}

/* 0x2F5A0. Emit or replace one glyph cell. The descriptor built for 0x2AE14 is
 * 20 bytes: {sprite id u16; 0; 0; flags 0x2A00; extent 0x80; 0x1000;
 * palette handle}. The 0x2A00 high byte carries bit 8, so 0x2AE14's initial
 * animation-stream walk is skipped and 0x2A408 keeps the descriptor's literal
 * sprite id rather than dereferencing it as a stream. The glyph's pixels are
 * therefore produced by the actor renderer (0x1C390), not written here. */
u8 text_glyph_emit(s32 ch, s32 *col, s32 *row, u32 mode, u32 vertical)
{
    u32 c = (u32)ch & 0xffu;
    u32 cls = mode & 3u;
    u32 mhi = mode & 0xf000u;
    u32 table;
    u32 palette;

    if (cls == 2u) {
        table = 0xbd048u;
        palette = (mhi == 0x3000u) ? 0x8099ccu : 0x8099acu;
    } else if (cls == 3u) {
        table = 0xbd1ecu;
        palette = 0x80995cu;
    } else {
        table = 0xbcd7cu;
        switch (mhi) {
        case 0x1000u: case 0x9000u: palette = 0x809984u; break;
        case 0x2000u: case 0xa000u: palette = 0x80998cu; break;
        case 0x3000u: case 0xb000u: palette = 0x809994u; break;
        case 0x4000u: case 0x5000u: case 0xc000u: case 0xd000u:
        case 0xf000u: palette = 0x8099a4u; break;
        default: palette = 0x80997cu; break;
        }
    }

    u32 width, height, sprite = 0;
    if (cls == 2u || cls == 3u) {
        /* 0x2F6BC: SAR 0x18 of the dword at 0xBD38D + c, i.e. its top byte. */
        s32 k = (s8)DSB(0xbd390u + c);
        if (k < 0) return 1;
        u32 e = table + (u32)k * 4u;
        sprite = DSW(e);
        width = DSB(e + 2u);
        height = DSB(e + 3u);
    } else if (c > 0x2eb4u && c < 0x2ec6u) {
        /* 0x2F6E0: fixed 8x8, table read skipped, so the surviving low word
         * (the character) is the sprite id. PORT: unreachable — ESI was masked
         * to 0xFF at 0x2F5AC, so ESI can never reach 0x2EB5. Transcribed. */
        sprite = c;
        width = 8u;
        height = 8u;
    } else {
        u32 e = table + c * 4u;
        sprite = DSW(e);
        width = DSB(e + 2u);
        height = DSB(e + 3u);
    }

    u32 off = (u32)*row * 0xacu + (u32)*col * 4u;
    u32 rec = DSD(DS_00105F38 + off);
    if (rec != 0) {
        release_record(rec, actor_pset(rec));      /* 0x2AD40 */
        DSD(DS_00105F38 + off) = 0;
    }

    if (c != 0x20u) {
        u32 desc[5];
        desc[0] = sprite;                          /* dp+0x00 */
        desc[1] = 0;                               /* dp+0x04 frame, dp+0x05 */
        desc[2] = 0x2a00u | (0x0080u << 16);       /* dp+0x08 flags, dp+0x0A extent */
        desc[3] = 0x1000u;                         /* dp+0x0C */
        desc[4] = palette;                         /* dp+0x10 palette handle */
        /* 0x2F7A2: a2 = col * 0x200, a3 = 0xFF, a4 = row * 0x200, a5 = 0. */
        u32 spawned = actor_spawn(desc, (u32)*col * 0x200u, 0xffu,
                                  (u32)*row * 0x200u, 0u);
        if (spawned == 0) return 1;
        DSD(DS_00105F38 + off) = spawned;
    }

    if (vertical != 0) {
        *row += (height == 0x10u) ? 2 : 1;
    } else {
        *col += (width == 0x10u) ? 2 : 1;
    }
    return 0;
}

/* ---- the loader's direct glyph path (0x1C5E8, 0x1C65C) ------------------
 *
 * PORT: the resource loader's `- LOADING -` screen. 0x1C5E8/0x1C65C are the
 * text system's other glyph path: instead of spawning a glyph actor (0x2F5A0)
 * they build a display node from the font table and blit it directly. The
 * font table is the same 0xBCD7C 4-byte {u16 sprite id; u8 width; u8 height}
 * table text_glyph_emit reads. */

/* 0x1C5E8. EAX = the character, EDX = &col, EBX = &row. Builds the display
 * node from the font entry, acquires the font palette 0x80997C (0x33754),
 * blits through 0x51ED8 and advances *col by the entry's width byte (0x10
 * advances 0xF, else 8). The original's return is the palette entry pointer;
 * no caller reads it. */
static void text_blit_glyph(u32 ch, s32 *col, s32 *row)
{
    u32 e = 0xbcd7cu + (ch & 0xffu) * 4u;
    SpriteNode n;
    sprite_node_build(&n, DSW(e));                    /* 0x1C5FB/0x1C60B */
    n.pal_ptr = palette_acquire(0x80997cu);           /* 0x1C610/0x1C61C */
    n.x = *col;                                       /* 0x1C630 */
    n.y = *row;                                       /* 0x1C635 */
    sprite_blit_at(&n, gfx_aperture());               /* 0x1C63D 0x51ED8 */
    *col += (DSB(e + 2u) == 0x10u) ? 0xfu : 8;        /* 0x1C642/0x1C647 */
}

/* 0x1C65C. EAX = the string, EDX = the x seed, EBX = the y seed. The seeds are
 * 12-bit fixed point: (v*0xF3D + 0x800)/0x1000 and (v*0xD56 + 0x800)/0x1000
 * (the original's SAR/SBB sequence is a truncating division, so the C
 * division is the same operation). 0x1B3AC's (EDX=0, EBX=0xE6) gives
 * (0, 192). Walks the string through 0x1C5E8 and flushes the palette dirty
 * list (0x1C470) when the string is non-empty. */
void text_blit_string(const u8 *s, s32 x, s32 y)
{
    s32 col = (x * 0xf3d + 0x800) / 0x1000;           /* 0x1C66A-0x1C680 */
    s32 row = (y * 0xd56 + 0x800) / 0x1000;           /* 0x1C683-0x1C69C */
    if (*s == 0) return;                              /* 0x1C6A3-0x1C6AA */
    while (*s != 0) {
        text_blit_glyph((u32)*s, &col, &row);         /* 0x1C6BD */
        s++;
    }
    gfx_flush_palette();                              /* 0x1C6C6 */
}

/* 0x2F830. `vertical` is the stack byte; 0x2F198 passes 0 and 0x2F20C passes 1.
 * The write to the caller's string is the original's own truncation at the line
 * limit (0x2A for horizontal, 0x1E for vertical). */
s32 text_render(const u8 *s, u32 mode, s32 row, s32 col, u32 vertical)
{
    u8 *m = (u8 *)s;   /* 0x2F830 writes the terminator into param_1 */

    if (*s == 0) return 0;                                 /* 0x2F849 */

    u32 all_spaces = 1;
    for (const u8 *p = s; *p != 0; p++) {                  /* 0x2F852-0x2F86C */
        if (*p != 0x20u) { all_spaces = 0; break; }
    }
    if (all_spaces) {
        text_cells_release(col, row, s, mode);             /* 0x2F882 -> 0x2F280 */
        return 0;
    }

    s32 w = text_width(s, mode);                           /* 0x2F898 -> 0x2F0F0 */
    if (vertical == 1u) {
        if (w > 0x1e) m[0x1d] = 0;
        if (w + row >= 0x1f) { s32 k = 0x1e - row; m[k - 1] = 0; }
    } else {
        if (w > 0x2a) m[0x29] = 0;
        if (w + col >= 0x2b) { s32 k = 0x2a - col; m[k - 1] = 0; }
    }

    s32 count = 0;
    for (;;) {
        u8 ch = *s;
        if (ch == 0) return count;                         /* 0x2F8E4 */
        if (text_glyph_emit((s32)ch, &col, &row, mode, vertical) != 0)
            return 0;                                      /* 0x2F903 */
        count++;
        s++;
    }
}

/* 0x2F198. EAX = col (-1 centers), EDX = row (-1 reuses the cursor), EBX =
 * string, ECX = mode. Emits the string through 0x2F830 and writes
 * {row, col + glyph count} as two words at DS_00105F34. The title's call
 * (0x1223F) passes EAX=-1, EDX=4, EBX=0x1C500's result, ECX=0x1000. */
void text_cursor_set(s32 col, s32 row, const u8 *s, u32 mode)
{
    if (row == -1) {
        /* 0x2F1B4/0x2F1BA: reload both cursor words and sign-extend them. */
        col = (s16)DSW(DS_00105F34 + 2);
        row = (s16)DSW(DS_00105F34);
    } else if (col == -1) {
        s32 w = text_width(s, mode);
        col = (0x2b - w) >> 1;
        if (col < 0) col = 0;
    }
    s32 extent = text_render(s, mode, row, col, 0u);   /* 0x2F198 passes 0 */
    DSW(DS_00105F34) = (u16)row;
    DSW(DS_00105F34 + 2) = (u16)(col + extent);
}

/* 0x2F280. Same register shape as 0x2F198. Clears `text_width(s, mode)`
 * consecutive cells of DS_00105F38, walking col 0..0x2a then wrapping to the
 * next row, and releases every non-empty record through 0x2AD40. */
void text_cells_release(s32 col, s32 row, const u8 *s, u32 mode)
{
    s32 count = text_width(s, mode);
    if (col < 0) {
        col = (0x2b - count) >> 1;
        if (col < 0) {
            col = 0;
            count = 0x2a;
        }
    }
    if (count <= 0) return;
    u32 rowbase = (u32)row * 0xacu;
    for (s32 i = 0; i < count; i++) {
        u32 idx = (u32)col * 4u + rowbase;
        u32 rec = DSD(DS_00105F38 + idx);
        if (rec != 0) {
            release_record(rec, actor_pset(rec));   /* 0x2AD40 */
            DSD(DS_00105F38 + idx) = 0;
        }
        if (col < 0x2b) col++;
        else { rowbase += 0xacu; col = 0; }
    }
}

/* 0x2F4BC (`push esi; mov esi,[0x85f34]; call 0x2F198; mov [0x85f34],esi`).
 * 0x2F198 with the cursor saved and restored. Its eight callers are not in
 * this cycle, so the port takes the arguments explicitly. */
void text_cursor_hold(s32 col, s32 row, const u8 *s, u32 mode)
{
    u32 save = DSD(DS_00105F34);
    text_cursor_set(col, row, s, mode);
    DSD(DS_00105F34) = save;
}
