/* Port of the 0x13xxx effect list: the free/active pools, the spawn (0x13C70),
 * the free-list build (0x13ADC), the clear (0x13DF0) and the per-entry teardown
 * (0x13420) over the two intrusive list primitives (0x249B0/0x249C0/0x249D0).
 * The lists are {next@+0; prev@+4} dwords holding mem[] offsets; the sentinels
 * are DS_000FCCE0 (active) and DS_000FCCE8 (free) and point at themselves when
 * empty. 24 records of stride 0x814 run DS_000F0B00..0xFC4CC. PORT: the
 * DS_0009AF3C interrupt lock is written for fidelity but is inert in the port's
 * single-threaded loop.
 *
 * The one reachable fight-camera update also lives here: the screen-shake decay
 * (0x1324C, update-table entry 0). It draws nothing; it maintains
 * DS_000F0AF4/DS_000F0AF6, which the unported y-stepper would add to the camera
 * y. 0x1324C is dormant in the shipped path (no store sets DS_00104AE8 bit 0);
 * it is registered so the existing update-table dispatch reaches it if bit 0 is
 * ever set. The rest of the camera/scene layer is deferred (see the section
 * below and port/spec/game_flow.md). */
#include "game/effects.h"
#include "../mem.h"
#include "../symbols.h"
#include "platform/gfx.h"
#include "platform/res.h"
#include <stddef.h>

/* 0x7CCF0 -> linear 0xFCCF0, the raw palette buffer 0x13420's case 4 enqueues
 * from. Not a named global in symbols.h. */
#define EFFECTS_PALETTE_BUF 0x000FCCF0u

/* ---- the splice lists (0x249B0/0x249C0/0x249D0) ------------------------- */

/* 0x249B0: insert rec immediately after `at`. */
void effects_list_insert_after(u32 at, u32 rec)
{
    u32 next = DSD(at);
    DSD(at) = rec;
    DSD(rec) = next;
    DSD(rec + 4) = at;
    DSD(next + 4) = rec;
}

/* 0x249C0: insert rec immediately before `at`. */
void effects_list_insert_before(u32 at, u32 rec)
{
    u32 prev = DSD(at + 4);
    DSD(at + 4) = rec;
    DSD(rec) = at;
    DSD(rec + 4) = prev;
    DSD(prev) = rec;
}

/* 0x249D0: unlink rec and zero its two link fields. */
void effects_list_unlink(u32 rec)
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
    effects_list_unlink(rec);
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

    effects_list_insert_after(DS_000FCCE8, rec);
}

/* ---- exported ----------------------------------------------------------- */

/* PORT: registers the original code addresses this module implements. Defined
 * with the camera/scene layer below; declared here because effects_init() is the
 * module's init (actors_reset calls it on every entry). */
static void camera_register(void);

/* 0x13ADC. */
void effects_init(void)
{
    camera_register();
    DSB(DS_0009AF3C) = 1;
    DSD(DS_000FCCE4) = DS_000FCCE0;
    DSD(DS_000FCCE0) = DS_000FCCE0;
    DSD(DS_000FCCEC) = DS_000FCCE8;
    DSD(DS_000FCCE8) = DS_000FCCE8;
    for (u32 rec = DS_000F0B00; rec < DS_000FCCE0; rec += EFFECTS_REC_SIZE)
        effects_list_insert_before(DS_000FCCE8, rec);
    DSB(DS_0009AF3C) = 0;
}

/* PORT: the four producers share one pop — take the free-list head under the
 * interrupt lock. Returns 0 when the pool is unbuilt or empty. The unbuilt
 * guard mirrors effects_clear's: with the sentinels zeroed (actors_reset
 * early-returns before effects_init) the free sentinel reads as rec=0 and
 * effects_list_unlink(0) would write mem[0]/mem[4], then link a phantom record onto the
 * active list. The original itself factors this pop out of line at 0x133D0
 * (uncalled); the port keeps one owner for the four producers. */
static u32 effect_take_free(void)
{
    if (DSD(DS_000FCCE8) == 0) return 0;
    u32 rec = DSD(DS_000FCCE8);
    /* PORT: the free sentinel points at itself when the list is empty. */
    if (rec == DS_000FCCE8) return 0;
    u8 saved = DSB(DS_0009AF3C);
    DSB(DS_0009AF3C) = 1;
    effects_list_unlink(rec);
    DSB(DS_0009AF3C) = saved;
    return rec;
}

/* 0x13C70. */
u32 effects_spawn(u32 source_rec, u32 byte_arg, u32 handle)
{
    u32 rec = effect_take_free();
    if (rec == 0) return 0;

    const u32 *resolved = (const u32 *)res_resolve(handle);
    DSB(rec + 0x0f) = 0x80;
    DSB(rec + 0x0c) = 3;
    DSD(rec + 8) = source_rec;
    DSB(rec + 0x0d) = (u8)byte_arg;

    /* The iteration bound is the SOURCE record's +0xC, read before any entry is
     * written; the original tests it signed (`test`/`jle`), so a non-positive
     * count is skipped. +0x10 holds zeros and +0x410 the resolved block's
     * dwords. PORT: 0x1B544 can fail to resolve; the original trusts the
     * handle, the port skips the copy rather than dereference NULL. */
    s32 count = (s32)DSD(source_rec + 0x0c);
    for (s32 i = 0; i < count; i++)
        DSD(rec + 0x10 + (u32)i * 4u) = 0;
    if (resolved != NULL) {
        for (s32 i = 0; i < count; i++)
            DSD(rec + 0x410 + (u32)i * 4u) = resolved[1 + i];
    }

    DSB(DS_0009AF3C) = 1;
    DSB(rec + 0x0e) = 1;
    effects_list_insert_after(DS_000FCCE0, rec);
    DSB(DS_0009AF3D) = (u8)(DSB(DS_0009AF3D) + 1);
    DSB(DS_0009AF3C) = 0;
    return rec;
}

/* 0x13D4C. Type 4: +0x10 holds the resolved block's colours for the case-4
 * step body to darken to zero. The handle is read from the source record. */
u32 effects_spawn_darken(u32 source_rec, u32 byte_arg)
{
    u32 rec = effect_take_free();
    if (rec == 0) return 0;
    /* PORT: the raw dereferences [source_rec] for the handle (0x13D86);
     * there is no handle register. */
    const u32 *resolved = (const u32 *)res_resolve(DSD(source_rec));
    DSB(rec + 0x0f) = 0;
    DSB(rec + 0x0c) = 4;
    DSD(rec + 8) = source_rec;
    DSB(rec + 0x0d) = (u8)byte_arg;
    s32 count = (s32)DSD(source_rec + 0x0c);
    if (resolved != NULL) {
        for (s32 i = 0; i < count; i++)
            DSD(rec + 0x10 + (u32)i * 4u) = resolved[1 + i];
    }
    DSB(DS_0009AF3C) = 1;
    DSB(rec + 0x0e) = 1;
    effects_list_insert_after(DS_000FCCE0, rec);
    DSB(DS_0009AF3D) = (u8)(DSB(DS_0009AF3D) + 1);
    DSB(DS_0009AF3C) = 0;
    return rec;
}

/* 0x13E28. Type 6: +0x10 starts white and the case-6 step body darkens it
 * toward the resolved block at +0x410. The handle is read from source_rec. */
u32 effects_spawn_pulse(u32 source_rec, u32 byte_arg)
{
    u32 rec = effect_take_free();
    if (rec == 0) return 0;
    /* PORT: the raw dereferences [source_rec] for the handle (0x13E66). */
    const u32 *resolved = (const u32 *)res_resolve(DSD(source_rec));
    DSB(rec + 0x0f) = 0x80;
    DSB(rec + 0x0c) = 6;
    DSD(rec + 8) = source_rec;
    DSB(rec + 0x0d) = (u8)byte_arg;
    s32 count = (s32)DSD(source_rec + 0x0c);
    /* PORT: two loops, not one - the raw (0x13E7D..0x13E96) fills all of +0x10
     * with white first, THEN (0x13E98..0x13EAB) copies the resolved block into
     * +0x410. The blocks overlap at +0x410 when count == 257 (white's i = 256,
     * resolved's j = 0), and the raw's ordering leaves the resolved value
     * there. A merged loop would leave white. */
    for (s32 i = 0; i < count; i++)
        DSD(rec + 0x10 + (u32)i * 4u) = 0x00FFFFFFu;
    /* PORT: the raw dereferences `resolved` unconditionally (faults on a failed
     * 0x1B544); the port skips the copy instead, as effects_spawn does. */
    if (resolved != NULL) {
        for (s32 i = 0; i < count; i++)
            DSD(rec + 0x410 + (u32)i * 4u) = resolved[1 + i];
    }
    DSB(DS_0009AF3C) = 1;
    DSB(rec + 0x0e) = 1;
    effects_list_insert_after(DS_000FCCE0, rec);
    DSB(DS_0009AF3D) = (u8)(DSB(DS_0009AF3D) + 1);
    DSB(DS_0009AF3C) = 0;
    return rec;
}

/* 0x13B3C. Types 0/2: the resolved block, walked by the signed `offset`, is
 * copied into BOTH +0x14 and +0x414. flag != 0 selects type 2 / +0x0E = 1;
 * flag == 0 selects type 0 / +0x0E = 0 - the raw truth, which never retires:
 * effects_step's type-0 branch skips while +0x0E is 0 and never reloads it.
 * The raw does NOT bump DS_0009AF3D (0x13B3C..0x13C6F writes only 0x1AF3C),
 * so this producer never counts as active. The handle is DSD(source_rec)
 * (0x13B85 reads [source_rec]); there is no handle register. */
u32 effects_spawn_scroll(u32 source_rec, s32 offset, u32 count, u32 flag)
{
    u32 rec = effect_take_free();
    if (rec == 0) return 0;
    const u32 *resolved = (const u32 *)res_resolve(DSD(source_rec));
    u8 n = (u8)count;
    s8 off = (s8)offset;
    u8 fl = (u8)flag;

    /* PORT: 0x1B544 can fail to resolve; the raw dereferences it
     * unconditionally (0x13B90 lea ebx,[eax+4]). The port skips the copy,
     * matching effects_spawn's existing guard. */
    if (resolved != NULL) {
        if (off < 0) {
            /* Descending arm (0x13B97..0x13BEB). The raw is a do-while bounded
             * by the byte count, so it runs count+1 times: destination
             * +0x14/+0x414 indices count down to 0 inclusive while the source
             * pointer descends. off == -0x80 is special-cased to start at
             * resolved[count] (0x13B9E cmp eax,-0x80); otherwise it starts at
             * resolved[1 + count - off] (0x13BBD add ebx,edx / 0x13BBF sub
             * ebx,eax). */
            const u32 *sp = (off == -0x80) ? resolved + n
                                           : resolved + 1 + (s32)n - (s32)off;
            for (s32 j = 0; j <= (s32)n; j++) {
                u32 v = *sp--;
                u32 idx = (u32)n - (u32)j;
                DSD(rec + 0x14 + idx * 4u) = v;
                DSD(rec + 0x414 + idx * 4u) = v;
            }
        } else {
            /* Forward arm (0x13BED..0x13C1B): `count` dwords from
             * resolved[1 + off] into +0x14[0..count) and +0x414[0..count). */
            const u32 *sp = resolved + 1 + (s32)off;
            for (u32 i = 0; i < n; i++) {
                u32 v = sp[i];
                DSD(rec + 0x14 + i * 4u) = v;
                DSD(rec + 0x414 + i * 4u) = v;
            }
        }
    }

    if (fl == 0) {
        DSB(rec + 0x0c) = 0;
        DSB(rec + 0x0e) = 0;
    } else {
        DSB(rec + 0x0c) = 2;
        DSB(rec + 0x0e) = 1;
    }
    DSD(rec + 8) = source_rec;
    DSB(rec + 0x0d) = fl;
    DSB(rec + 0x10) = (u8)offset;
    DSB(rec + 0x0f) = n;

    DSB(DS_0009AF3C) = 1;
    effects_list_insert_after(DS_000FCCE0, rec);
    DSB(DS_0009AF3C) = 0;
    return rec;
}

/* ---- 0x134C0: the per-frame effect step/age ----------------------------- */

/* The step moves colours packed as 0xRRGGBB in a dword (channels at byte shifts
 * 0/8/16). 0x1362e/0x137ee-b raise each channel by 8 but not past the target;
 * 0x13996/0x137ee-a lower it by 8, toward the target or to zero. */
static u32 effect_lighten(u32 cur, u32 tgt)
{
    u32 b = (cur & 0xffu) + 8u, g = ((cur >> 8) & 0xffu) + 8u,
        r = ((cur >> 16) & 0xffu) + 8u;
    u32 tb = tgt & 0xffu, tg = (tgt >> 8) & 0xffu, tr = (tgt >> 16) & 0xffu;
    if (b > tb) b = tb;
    if (g > tg) g = tg;
    if (r > tr) r = tr;
    return (b & 0xffu) | ((g & 0xffu) << 8) | ((r & 0xffu) << 16);
}

static u32 effect_darken_to(u32 cur, u32 tgt)
{
    s32 b = (s32)(cur & 0xffu) - 8, g = (s32)((cur >> 8) & 0xffu) - 8,
        r = (s32)((cur >> 16) & 0xffu) - 8;
    s32 tb = (s32)(tgt & 0xffu), tg = (s32)((tgt >> 8) & 0xffu), tr = (s32)((tgt >> 16) & 0xffu);
    if (b < tb) b = tb;
    if (g < tg) g = tg;
    if (r < tr) r = tr;
    return ((u32)b & 0xffu) | (((u32)g & 0xffu) << 8) | (((u32)r & 0xffu) << 16);
}

static u32 effect_darken(u32 cur)
{
    s32 b = (s32)(cur & 0xffu) - 8, g = (s32)((cur >> 8) & 0xffu) - 8,
        r = (s32)((cur >> 16) & 0xffu) - 8;
    if (b < 0) b = 0;
    if (g < 0) g = 0;
    if (r < 0) r = 0;
    return (u32)b | ((u32)g << 8) | ((u32)r << 16);
}

/* 0x134C0. */
void effects_step(void)
{
    if (DSB(DS_0009AF3C) != 0) return;
    u32 rec = DSD(DS_000FCCE0);
    while (rec != DS_000FCCE0) {
        u32 src = DSD(rec + 8);
        s32 count = (s32)DSD(src + 0x0c);
        u8 type = DSB(rec + 0x0c);
        int removed = 0;

        /* The state byte counts down from the spawn's DL (rec+0xD); only when it
         * wraps does the type body run. Type 0 has no reload and instead runs
         * the case-2 body once via the 0x13502 jump. */
        if (type == 0) {
            if (DSB(rec + 0x0e) == 0) { rec = DSD(rec); continue; }
            DSB(rec + 0x0e) = 0;
            type = 2;
        } else {
            u8 state = (u8)(DSB(rec + 0x0e) - 1);
            DSB(rec + 0x0e) = state;
            if (state != 0) { rec = DSD(rec); continue; }
            DSB(rec + 0x0e) = DSB(rec + 0x0d);
            if (type > 6) type = 6;     /* 0x13522: the >6 default body */
        }

        switch (type) {
        case 1: {
            u32 a = DSD(rec + 0x14);
            u32 v = (u32)(((a & 0xffu) + (u32)((s32)DSD(rec + 0x18) >> 16)) & 0xffu)
                  | (u32)((((a & 0xffffu) >> 8) + (u32)((s32)DSD(rec + 0x1a) >> 16)) & 0xffffu) << 8
                  | (u32)((((a >> 16) & 0xffu) + (u32)((s32)DSD(rec + 0x1c) >> 16)) & 0xffu) << 16;
            DSD(rec + 0x14) = v;
            palette_record(rec + 0x14, DSD(src + 8) + (u32)DSB(rec + 0x0f), 1, 0);
            break;
        }
        case 2:
            /* 0x135a8: rotate the +0x10 trail by one dword either way; the sign
             * of the block's first byte picks the direction and the palette
             * offset. */
            if ((s8)DSB(rec + 0x10) < 0) {
                u32 d = rec + 0x14, s = rec + 0x10, tmp = DSD(d);
                for (u32 i = 1; i < (u32)DSB(rec + 0x0f); i++) {
                    d -= 4; u32 v = DSD(s); s -= 4; DSD(d + 4) = v;
                }
                DSD(d) = tmp;
                palette_record(rec + 0x14,
                               (u32)((s32)DSD(src + 8) - (s32)(s8)DSB(rec + 0x10)),
                               (u32)DSB(rec + 0x0f), 0);
            } else {
                u32 d = rec + 0x14, s = rec + 0x18, tmp = DSD(d);
                for (u32 i = 1; i < (u32)DSB(rec + 0x0f); i++) {
                    d += 4; u32 v = DSD(s); s += 4; DSD(d - 4) = v;
                }
                DSD(d) = tmp;
                palette_record(rec + 0x14,
                               (u32)((s32)DSD(src + 8) + (s32)(s8)DSB(rec + 0x10)),
                               (u32)DSB(rec + 0x0f), 0);
            }
            break;
        case 3:
            if (DSB(rec + 0x0f) != 0) {
                palette_record(rec + 0x10, DSD(src + 8), (u32)count, 0);
                DSB(rec + 0x0f) = 0;
            } else {
                int anim = 1;
                for (s32 i = 0; i < count; i++) {
                    u32 at = rec + 0x10 + (u32)i * 4u;
                    u32 cur = DSD(at), tgt = DSD(rec + 0x410 + (u32)i * 4u);
                    if (cur != tgt) { DSD(at) = effect_lighten(cur, tgt); anim = 0; }
                }
                if (anim) removed = 1;
                else palette_record(rec + 0x10, DSD(src + 8), (u32)count, 0);
            }
            break;
        case 4: {
            int anim = 1;
            for (s32 i = 0; i < count; i++) {
                u32 at = rec + 0x10 + (u32)i * 4u;
                u32 cur = DSD(at);
                if (cur != 0) { DSD(at) = effect_darken(cur); anim = 0; }
            }
            if (anim) removed = 1;
            else palette_record(rec + 0x10, DSD(src + 8), (u32)count, 0);
            break;
        }
        case 5: {
            /* 0x137ee: two-phase pulse. Flag +0x11 selects which side runs;
             * each completed side drains the count without tearing down. */
            u8 n = DSB(rec + 0x0f);
            int anim = 1;
            /* The dispatch left ESI at rec+0x14 for this body (0x1352d), unlike
             * the 0x10 bases the other cases load. */
            if (DSB(rec + 0x11) != 0) {
                for (s32 i = 0; i < (s32)n; i++) {
                    u32 at = rec + 0x14 + (u32)i * 4u;
                    u32 cur = DSD(at);
                    if (cur != 0) { DSD(at) = effect_darken(cur); anim = 0; }
                }
                if (anim) {
                    DSB(DS_0009AF3D) = (u8)(DSB(DS_0009AF3D) - 1);
                    DSB(rec + 0x11) = 0;
                }
            } else {
                for (s32 i = 0; i < (s32)n; i++) {
                    u32 at = rec + 0x14 + (u32)i * 4u;
                    u32 cur = DSD(at), tgt = DSD(rec + 0x414 + (u32)i * 4u);
                    if (cur != tgt) { DSD(at) = effect_lighten(cur, tgt); anim = 0; }
                }
                if (anim) {
                    DSB(DS_0009AF3D) = (u8)(DSB(DS_0009AF3D) - 1);
                    DSB(rec + 0x11) = 1;
                }
            }
            palette_record(rec + 0x14, DSD(src + 8) + (u32)(s32)(s8)DSB(rec + 0x10),
                           (u32)n, 0);
            break;
        }
        case 6:
        default:
            if (DSB(rec + 0x0f) != 0) {
                palette_record(rec + 0x10, DSD(src + 8), (u32)count, 0);
                DSB(rec + 0x0f) = 0;
            } else {
                int anim = 1;
                for (s32 i = 0; i < count; i++) {
                    u32 at = rec + 0x10 + (u32)i * 4u;
                    u32 cur = DSD(at), tgt = DSD(rec + 0x410 + (u32)i * 4u);
                    if (cur != tgt) { DSD(at) = effect_darken_to(cur, tgt); anim = 0; }
                }
                if (anim) removed = 1;
                else palette_record(rec + 0x10, DSD(src + 8), (u32)count, 0);
            }
            break;
        }

        if (removed) {
            /* 0x249d0 unlinks and zeroes the links, so save the back-link first;
             * after the unlink it points at the record's original next. */
            u32 back = DSD(rec + 4);
            effects_list_unlink(rec);
            effects_list_insert_after(DS_000FCCE8, rec);
            DSB(DS_0009AF3D) = (u8)(DSB(DS_0009AF3D) - 1);
            rec = DSD(back);
        } else {
            rec = DSD(rec);
        }
    }
}

/* ---- the fight-camera/scene state updates ------------------------------- */

/* 0x1324C. Screen-shake decay, update-table entry 0. PORT: dormant in the
 * shipped path - no store sets DS_00104AE8 bit 0 (only clears and bits 2/5/6
 * are written). Registered so the existing update-table dispatch reaches it.
 *
 * PORT: the rest of the camera/scene layer (0x12CD4 the y-stepper, 0x1317C the
 * y-clamp, 0x13290/0x1333C the x-centering modes) is deliberately NOT ported.
 * Its only callers are the unported dispatcher chain 0x12D48 (modes 0x12DF0/
 * 0x12E3C) and 0x12DA8/0x131F8/0x13224, which no task in this plan owns; the
 * raw gives 0x12CD4 exactly one caller, 0x1317C at 0x131cb, and 0x1324C calls
 * nothing, so all four would be unreachable production surface. Deferred and
 * recorded as a gap in port/spec/game_flow.md. */
void camera_shake_decay(void)
{
    s16 vel = (s16)DSW(DS_000F0AF6);
    s16 off = (s16)DSW(DS_000F0AF4);
    s16 sum = (s16)(off + vel);
    if (vel < 0 && sum <= 0) {
        vel = 0;
        DSB(DS_00104AE8) &= 0xfe;
        sum = 0;
    }
    vel = (s16)(vel - 0x20);
    DSW(DS_000F0AF6) = (u16)vel;
    DSW(DS_000F0AF4) = (u16)sum;
}

/* PORT: one-time registration (the raw's update table is static data). */
static void camera_register(void)
{
    static int done;
    if (done) return;
    done = 1;
    fn_register(FN_0001324C, camera_shake_decay);
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
