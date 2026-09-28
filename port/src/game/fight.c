/* Port of the demo arena frame 0x263F4 and the HUD/health spine
 * 0x35658 -> 0x34B6C -> 0x1A978 -> 0x3B134, plus the scene/effects pass
 * 0x49C78. Addresses, gates and arithmetic are from
 * docs/superpowers/plans/2026-09-20-demo-fight-derivations.md §3-§5. The module
 * registers nothing; the arena frame is the state-7 body's first call
 * (0x11E8F). The 0x1975C think step is fighter_think() (fighter.c). */
#include "game/fight.h"
#include "game/fighter.h"
#include "game/camera.h"
#include "game/actors.h"
#include "game/attract.h"
#include "game/config.h"
#include "game/effects.h"
#include "game/flow.h"
#include "game/rng.h"
#include "../mem.h"
#include "../symbols.h"
#include "platform/render.h"

#include <string.h>

/* 0x10810D: the mode-3 single-player slot index, written by 0x41350. Ghidra
 * emits it only as the `ram0x0010810d` form, so gen_symbols.py has no DS_ name
 * (camera.c's CAMERA_SLOT_INDEX3 is the same address). */
#define FIGHT_SLOT_INDEX 0x0010810Du

/* ---- 0x3C5CC the frame's first call ------------------------------------ */

void fight_slot_clear(void)
{
    DSD(DS_00107EE0) = 0;                       /* 0x3C5CF */
    DSD(DS_00107D50) = 0;                       /* 0x3C5D5 */
    DSD(DS_00107D54) = 0;                       /* 0x3C5DB */
}

/* ---- 0x49300 the fight-effect list init --------------------------------- */

/* 0x49300, called from state 6's 0x20DF4 at 0x11AC4. It self-links the
 * 0x1083C4 and 0x10884C {next@+0; prev@+4} sentinels (inserting each 0x24-stride
 * node into the 0x1083C4 list with 0x249C0) and seeds DS_001088CC/CB from the
 * draw1 value DS_00104AFC. The port skipped 0x20DF4 as a named gap, but the
 * 0x49C78 walk at 0x49CAF (fight_effects_pass) reads DS_0010884C as its head and
 * loops until it returns to the sentinel, so without this init the uninitialised
 * zero head walks address 0 forever. Ported here because 0x49300 is that walk's
 * liveness precondition; 0x20DF4's other resets stay declared gaps. */
void fight_list_init(void)
{
    DSD(DS_00108880) = 0x1500u;                 /* 0x49312 */
    DSD(DS_00108850) = DS_0010884C;            /* 0x49318 */
    DSD(DS_0010884C) = DS_0010884C;            /* 0x4931E */
    DSD(DS_001083C8) = DS_001083C4;            /* 0x49324 */
    DSD(DS_001083C4) = DS_001083C4;            /* 0x4932F */
    for (u32 node = 0x1083CCu; node < DS_0010884C; node += 0x24u)
        effects_list_insert_before(DS_001083C4, node);   /* 0x49347 0x249C0 */
    if ((u32)DSW(DS_00104AFC) == 7u) {          /* 0x4935C */
        DSB(DS_001088CC) = 0;                   /* 0x49363 */
        DSB(DS_001088CB) = 0;                   /* 0x49369 */
        return;
    }
    DSB(DS_001088CC) = 2u;                      /* 0x49377 */
    DSB(DS_001088CB) = 4u;                      /* 0x4937D */
}

/* ---- 0x412A0/0x2C320 the scene's props and crowd ------------------------ */

/* The crowd's descriptor-index table at 0xBB9D8 (stride 0xC, the first dword
 * used); symbols.h emits no name for it. */
#define DS_000BB9D8 0x000BB9D8u

/* 0x2C320. The scene's crowd actors. `n = DSW(0xBBD98 + scene*2)`; for each
 * 12-byte record in the table at `0xBBDA8[scene]` it spawns
 * 0x2AE14(desc = 0xBB9D8[[e+0xA]*3], a2 = [e], a3 = (s16)[e+6], a4 = (s16)[e+4],
 * a5 = ([e+0xB] << 16) | word[e+8]) and stores the table at DS_00105C08. The
 * only caller is 0x412A0 (0x412D8). Scene 0's table is 0xBBC18 with 3 records. */
void fight_scene_crowd(u32 scene)
{
    u16 n = DSW(DS_000BBD98 + scene * 2u);          /* 0x2C325 */
    if (n == 0u) return;                            /* 0x2C32D */
    u32 table = DSD(DS_000BBDA8 + scene * 4u);      /* 0x2C332 */
    DSD(DS_00105C08) = table;                       /* 0x2C339 */
    for (u32 i = 0; i < (u32)n; i++) {              /* 0x2C340 */
        u32 e = table + i * 0xCu;
        u32 a5 = ((u32)DSB(e + 0xbu) << 16) | DSW(e + 8u);        /* 0x2C347 */
        u32 desc = DSD(DS_000BB9D8 + (u32)DSB(e + 0xau) * 0xCu);  /* 0x2C370 */
        (void)actor_spawn((const u32 *)(mem + desc), DSD(e),
                          (u32)(s32)(s16)DSW(e + 6u),   /* a3 = ECX */
                          (u32)(s32)(s16)DSW(e + 4u),   /* a4 = EBX */
                          a5);                          /* 0x2C379 */
    }
}

/* 0x412A0. The scene's prop actors, the `0x20DF4` branch's third call
 * (0x20E86, EAX = the clamped scene index). For each 12-byte triple in the
 * table at `0xC82CC[scene]` — until a zero first dword — it spawns
 * 0x2AE14(desc = [e], a2 = [e+4], a3 = (s16)[e+8], a4 = 0, a5 = 0), then
 * 0x2C320(scene).
 *
 * PORT: the tail call `0xC7F58[scene]()` (0x412DD) is not issued: the eight
 * table entries are no-op targets — 0x412EC is 0x412A0's own `RET` and 0x5D812
 * is `xor eax,eax; ret` (both in the fixed image) — so it spawns nothing.
 * Scene 0's table is 0xC7F78 (5 triples) and its descriptors at 0xC77EC,
 * 0xC7800, 0xC7814, 0xC7828, 0xC783C carry sprite ids 0x2EF..0x2F3, which
 * 0xA8B30 resolves to s16beach descriptor indices 1, 0, 2, 4 (the temple), 3. */
void fight_scene_props(u32 scene)
{
    u32 table = DSD(DS_000C82CC + scene * 4u);      /* 0x412A8 */
    if (DSD(table) != 0u) {                         /* 0x412B3 */
        u32 e = table;
        for (;;) {
            (void)actor_spawn((const u32 *)(mem + DSD(e)), DSD(e + 4u),
                              (u32)(s32)(s16)DSW(e + 8u), 0u, 0u);  /* 0x412C7 */
            e += 0xCu;                              /* 0x412CF */
            if (DSD(e) == 0u) break;                /* 0x412D2 (EBP == 0) */
        }
    }
    fight_scene_crowd(scene);                       /* 0x412D8 */
}

/* ---- 0x43818 the per-side character screen's setup ---------------------- */

/* 0x43818 — demo-pose record §41-D. The setup 0x43738 (at 0x43778) and
 * 0x444C8 (at 0x44508) run before they fill each side's character entries
 * (0x43964/0x43A08). EAX = 0 into 0x4F228 (0x4381B), then 0x2BAF4 with EAX = 1
 * (actors_reset's arm), the ret-only 0x29D60, three 0x2AE14 spawns and four
 * 0x33754 acquires. ECX = 0xE0 (0x4381D) and EBX = 0 (0x4382C) reach the first
 * spawn intact: 0x4F228 never writes ECX, 0x2BAF4 pushes and pops both, and
 * 0x29D60 is a bare `ret`. Each spawn pushes a5 = 0 and 0x2AE14 pops it (`ret 4`,
 * 0x2B14A). The EAX left by the last acquire is dead: both callers overwrite
 * EAX next (0x4377F, 0x4450F). Its callers fight_char_screen_open (0x43738)
 * and fight_char_screen_open_both (0x444C8) are 0x24C5C-mode code no driver
 * reaches (record §42-F). */
#define DS_000C885C 0x000C885Cu   /* no symbols.h name: the first descriptor */
#define DS_000C87F8 0x000C87F8u   /* no symbols.h name: the per-side descriptor */
void fight_char_screen_setup(void)
{
    render_projection_reset(0u);                        /* 0x43822 0x4F228 */
    actors_reset();                                     /* 0x4382E 0x2BAF4 */
    /* 0x43833 0x29D60 is a ret-only no-op. */
    (void)actor_spawn((const u32 *)(mem + DS_000C885C),
                      0u, 0xE0u, 0u, 0u);               /* 0x43841 0x2AE14 */
    DSD(DS_0010814C) = actor_spawn((const u32 *)(mem + DS_000C87F8),
                                   0x1500u, 0xE2u, 0x3900u, 0u);  /* 0x4385C/0x43872 */
    DSD(DS_00108150) = actor_spawn((const u32 *)(mem + DS_000C87F8),
                                   0x3F00u, 0xE2u, 0x3900u, 0u);  /* 0x4387C/0x43881 */
    (void)palette_acquire(0x098EC50Cu);                 /* 0x4388B 0x33754 */
    (void)palette_acquire(0x098EC514u);                 /* 0x43895 0x33754 */
    (void)palette_acquire(0x008099ACu);                 /* 0x4389F 0x33754 */
    (void)palette_acquire(0x00809984u);                 /* 0x438A9 0x33754 */
}

/* ---- 0x43738/0x444C8 the character screen's entry (record §42-F) -------- */

#define DS_000C8870 0x000C8870u   /* no symbols.h name: [side] entry descriptor */
#define DS_000C8878 0x000C8878u   /* no symbols.h name: [side] panel descriptor */
#define DS_000C88DC 0x000C88DCu   /* no symbols.h name: [char] panel sprite id */
#define DS_000C88F8 0x000C88F8u   /* no symbols.h name: [char] pset word */
#define DS_000C8908 0x000C8908u   /* no symbols.h name: [char] palette handle */
#define DS_000BB938 0x000BB938u   /* no symbols.h name: [class][side] descriptor */
#define DS_000A78B0 0x000A78B0u   /* no symbols.h name: [class] marker descriptor */
#define DS_00104B1B 0x00104B1Bu   /* no symbols.h name */

/* 0x1D810 — record §42-F. EAX = side. When DS_001028E0[side] holds a record,
 * 0x2B150 marks it dead and the slot is zeroed (ECX = 0 at 0x1D826; 0x2B150
 * pushes and pops EBX, ECX and EDX). */
void fight_select_marker_release(u32 side)
{
    u32 rec = DSD(DS_001028E0 + side * 4u);             /* 0x1D81A */
    if (rec == 0u) return;                              /* 0x1D822 */
    actor_set_dead(rec);                                /* 0x1D828 0x2B150 */
    DSD(DS_001028E0 + side * 4u) = 0u;                  /* 0x1D82D */
}

/* 0x1D7B8 — record §42-F. EAX = side, EDX = class, EBX = the y (the caller's
 * a4, passed through: 0x1D7B8 never writes EBX, and 0x2B150 preserves it).
 * The 0x1D810 release, then 0x2AE14(0xA78B0[class], a2 = side ? 0x4200 :
 * 0x200, a3 = 0xFD, a4 = y, a5 = 0) into DS_001028E0[side]. */
void fight_select_marker_spawn(u32 side, u32 cls, u32 y)
{
    u32 rec = DSD(DS_001028E0 + side * 4u);             /* 0x1D7C7 */
    if (rec != 0u) {                                    /* 0x1D7CD */
        actor_set_dead(rec);                            /* 0x1D7D5 0x2B150 */
        DSD(DS_001028E0 + side * 4u) = 0u;              /* 0x1D7DA (EBP = 0) */
    }
    DSD(DS_001028E0 + side * 4u) = actor_spawn(
        (const u32 *)(mem + DSD(DS_000A78B0 + cls * 4u)),
        side != 0u ? 0x4200u : 0x200u, 0xFDu, y, 0u);   /* 0x1D7FE/0x1D803 */
}

#define DS_000A7760 0x000A7760u   /* no symbols.h name: [char] badge descriptor */
#define DS_000E904C 0x000E904Cu   /* no symbols.h name: the 0x1D764 stream */

/* 0x1D838 — record §48-Q. EAX = side (ESI), EDX = the character (ECX, saved
 * before 0x1D840 reuses EDX as side * 4), EBX = the y (passed through). As
 * 0x1D7B8: a record in DS_001028E0[side] is marked dead (0x2B150, which
 * pushes and pops EDX) and the slot zeroed (EBP = 0); then 0x2AE14(0xA7760[
 * character], a2 = side ? 0x4200 : 0x200, a3 = 0xFD, a4 = y, a5 = 0) into
 * DS_001028E0[side]. Callers: 0x27770 (0x274FC) and 0x298F3 (0x296B8) with
 * y = the word at 0xA76D0 (0x980), 0x1D9D5 (0x1D890), 0x1DDBF (0x1DC6C) and
 * 0x25F47 (0x25C88) (record §48-U), and the unported 0x1DBFA (0x1DAE8). */
void fight_hud_badge_spawn(u32 side, u32 ch, u32 y)
{
    u32 rec = DSD(DS_001028E0 + side * 4u);             /* 0x1D840/0x1D847 */
    if (rec != 0u) {                                    /* 0x1D84D */
        actor_set_dead(rec);                            /* 0x1D855 0x2B150 */
        DSD(DS_001028E0 + side * 4u) = 0u;              /* 0x1D85A (EBP = 0) */
    }
    DSD(DS_001028E0 + side * 4u) = actor_spawn(
        (const u32 *)(mem + DSD(DS_000A7760 + ch * 4u)),
        side != 0u ? 0x4200u : 0x200u, 0xFDu, y, 0u);   /* 0x1D860..0x1D883 */
}

/* 0x1D764 — record §48-Q. EAX = side (EBX). DS_0010290C[side] = 0 (DL), the
 * slot's +0x5A = 0 and +0x42 bit 4 cleared (read at 0x1D77B before the +0x5A
 * store, written at 0x1D78E), 0x1D2F0(0, side), then the record
 * DS_001028F8[side] begins the stream 0xE904C at 1.0 (0x3F800000 pushed,
 * 0x2BC30). EBX/ECX/EDX are pushed and popped. Callers: 0x27752 (0x274FC),
 * 0x298DE (0x296B8), 0x279B6 (0x2791C, record §48-E) and 0x27F01
 * (0x27ED8, record §48-K). */
void fight_hud_side_reset(u32 side)
{
    u32 slot = DS_001077B0 + side * 0x94u;              /* 0x1D767..0x1D773 */
    u8 cl;
    DSB(DS_0010290C + side) = 0u;                       /* 0x1D775 */
    cl = DSB(slot + 0x42u);                             /* 0x1D77B */
    DSB(slot + 0x5Au) = 0u;                             /* 0x1D782 */
    DSB(slot + 0x42u) = (u8)(cl & 0xEFu);               /* 0x1D789/0x1D78E */
    fight_hud_bar_set(0, side);                         /* 0x1D797 0x1D2F0 (record §48-U) */
    actors_anim_begin(DSD(DS_001028F8 + side * 4u), DS_000E904C,
                      0x3F800000u);                     /* 0x1D79C..0x1D7AD 0x2BC30 */
}

#define DS_00102908 0x00102908u   /* no symbols.h name: [side] the pulse countdown word */
#define DS_000E9050 0x000E9050u   /* no symbols.h name: the 0x1DA08 stream */

/* 0x1DA08 — record §48-C. The two sides' pulse countdowns (EBX = side * 2,
 * ESI = side * 0x94, ECX = side * 4; EBX/ECX/EDX/ESI pushed and popped, EAX
 * clobbered). Per side the word DS_00102908[side] is decremented (`mov dx`,
 * `dec edx`, and only DX is stored, so the word wraps mod 0x10000); while it
 * stays above 0 (signed, 0x1DA21 `test dx,dx`, 0x1DA24 `jg`) nothing else
 * runs. Otherwise the record DS_001028F8[side] begins the stream 0xE9050 at
 * 4.0 (0x40800000 pushed, 0x2BC30, which pushes and pops EBX/ECX/ESI and
 * returns `ret 4`), and the word is reloaded with (0x78 - the slot's +0x5A
 * byte) >> 1 (0x1DA3D..0x1DA4C, `sar`: signed), raised to 0xC when that is
 * below 0xC (0x1DA55 reads the dword at 0x102906 + side * 2 and `sar`s it by
 * 0x10, which is the signed word just stored; 0x1DA5E `cmp`, 0x1DA61 `jge`).
 * Callers: 0x274E7 (0x27380, record §48-C) and 0x263AA (0x26254, record
 * §48-K), and the unported 0x26696 (0x26540), 0x26864 (0x266AC) and 0x29B4F
 * (0x299E8). */
void fight_hud_pulse(void)
{
    u32 side;
    for (side = 0; side < 2u; side++) {                 /* 0x1DA6C..0x1DA7B */
        u16 w = (u16)(DSW(DS_00102908 + side * 2u) - 1u);   /* 0x1DA12/0x1DA19 */
        DSW(DS_00102908 + side * 2u) = w;               /* 0x1DA1A */
        if ((s16)w > 0) continue;                       /* 0x1DA21/0x1DA24 `jg` */
        actors_anim_begin(DSD(DS_001028F8 + side * 4u), DS_000E9050,
                          0x40800000u);                 /* 0x1DA26..0x1DA36 0x2BC30 */
        DSW(DS_00102908 + side * 2u) = (u16)((0x78 -
            (s32)DSB(DS_0010780A + side * 0x94u)) >> 1);    /* 0x1DA3B..0x1DA4E */
        if ((s16)DSW(DS_00102908 + side * 2u) < 0xC)    /* 0x1DA55..0x1DA61 `jge` */
            DSW(DS_00102908 + side * 2u) = 0xCu;        /* 0x1DA63 */
    }
}

/* 0x43964 — record §42-F. EAX = side; the character is the signed byte
 * DS_00108166[side] (0x43972 reads the dword at 0x108163 + side, 0x4397C `sar
 * esi,0x18`). Two spawns, a5 = 0 (each `push 0` popped by 0x2AE14's `ret 4`):
 * 0xC8870[side] at (0xC8898[ch], a3 0xFF, 0xC88A6[ch]) into DS_00108154[side],
 * and 0xC8878[side] at (0, a3 0xFE, 0) into DS_0010815C[side]. The second is
 * re-pointed at the sprite id 0xC88DC[ch] (0x2BCF4) and given the pset word
 * 0xC88F8[ch] and the palette 0xC8908[ch] (0x2A17C). The words are
 * zero-extended (`xor ebx,ebx`/`xor edx,edx` before each `mov bx`/`mov dx`). */
void fight_char_entry_spawn(u32 side)
{
    s32 ch = (s8)DSB(DS_00108166 + side);               /* 0x43972/0x4397C */
    DSD(DS_00108154 + side * 4u) = actor_spawn(
        (const u32 *)(mem + DSD(DS_000C8870 + side * 4u)),
        DSW(DS_000C8898 + (u32)(ch * 2)), 0xFFu,
        DSW(DS_000C88A6 + (u32)(ch * 2)), 0u);          /* 0x43996/0x439A6 */
    DSD(DS_0010815C + side * 4u) = actor_spawn(
        (const u32 *)(mem + DSD(DS_000C8878 + side * 4u)),
        0u, 0xFEu, 0u, 0u);                             /* 0x439B4/0x439B9 */
    actors_anim_seek(DSD(DS_0010815C + side * 4u),
                     DSD(DS_000C88DC + (u32)(ch * 4)));  /* 0x439D7 0x2BCF4 */
    actor_pset_palette(DSD(DS_0010815C + side * 4u),
                       DSW(DS_000C88F8 + (u32)(ch * 2)),
                       DSD(DS_000C8908 + (u32)(ch * 4)));  /* 0x439FD 0x2A17C */
}

/* 0x43A08 — record §42-F. EAX = side, the character as in 0x43964 and its
 * class the signed byte 0xC8882[ch] (0x43A19, 0x43A51 `movsx ebp,cl`). The
 * spawn 0xBB938[class * 2 + side] at (0xC88B4[side], a3 0xFF, 0xC88B8[side])
 * with a5 = side ? 0 : 0x4000 (0x43A21..0x43A3B) into DS_0010813C[side], its
 * +0x4D = 0x1E (0x43A7D); then 0x1D7B8(side, class) with EBX = 0x1800
 * (0x43A78), DS_0010814C[side]'s +0x29 |= 8 (0x43A8F) and that record
 * re-pointed at the sprite id 0x32B (0x43A9E 0x2BCF4). */
void fight_char_select_actor(u32 side)
{
    s32 ch = (s8)DSB(DS_00108166 + side);               /* 0x43A10/0x43A16 */
    s32 cls = (s8)DSB(DS_000C8882 + (u32)ch);           /* 0x43A19/0x43A51 */
    u32 rec = actor_spawn(
        (const u32 *)(mem + DSD(DS_000BB938 + side * 4u + (u32)(cls * 8))),
        DSW(DS_000C88B4 + side * 2u), 0xFFu,
        DSW(DS_000C88B8 + side * 2u),
        side == 0u ? 0x4000u : 0u);                     /* 0x43A60 0x2AE14 */
    DSD(DS_0010813C + side * 4u) = rec;                 /* 0x43A65 */
    DSB(rec + 0x4Du) = 0x1Eu;                           /* 0x43A7D */
    fight_select_marker_spawn(side, (u32)cls, 0x1800u); /* 0x43A84 0x1D7B8 */
    DSB(DSD(DS_0010814C + side * 4u) + 0x29u) |= 8u;    /* 0x43A8F */
    actors_anim_seek(DSD(DS_0010814C + side * 4u), 0x32Bu);  /* 0x43A9E 0x2BCF4 */
}

/* 0x43738 — record §42-F. The character screen's entry: the 0x104AE4 frame
 * hook 0x28D68/0x28D80 install, also called directly at 0x4372E. The shared
 * prologue (0x2C3FC voice, 0x13DF0, 0x2C8F0(-1), DS_00104B1B/DS_00104B24 = 0,
 * 0x1D810 for both sides, 0x43818), then per side: when DS_00104B1F has the
 * bit side + 1 (0x4377F `lea eax,[edx+1]`, 0x43788 `test ecx,eax`: 1 for
 * side 0, 2 for side 1) DS_00108170[side] = 1, 0x43964 and 0x43A08, else
 * DS_00108170[side] = 0; either way DS_00108144[side] = 0 (EBX = 0 from
 * 0x43771 advanced by 4 before the 0x437B2 store at EBX + 0x108140). The
 * countdown DS_0010816C = DS_00108173 ? 5 : 0xF is drawn by 0x2F528 at col
 * 0x13, row 1, width 2, pad 0, mode 0x4000 (the value is the signed word,
 * 0x437EE/0x437F6 `sar ebx,0x10`). Last, the hook becomes 0x29D60 and
 * DS_00108174 = 0. EDX (0 from 0x43765) and EBX survive 0x1D810 and 0x43818,
 * which push and pop them. */
void fight_char_screen_open(void)
{
    /* PORT: 0x43741 0x2C3FC(0x30) voice, not wired (record §45-A). */
    effects_clear();                                    /* 0x43746 0x13DF0 */
    attract_config_volumes_unscaled();                  /* 0x43750 0x2C8F0(-1) */
    DSB(DS_00104B1B) = 0u;                              /* 0x43757 */
    DSB(DS_00104B24) = 0u;                              /* 0x4375D */
    fight_select_marker_release(0u);                    /* 0x43767 0x1D810 */
    fight_select_marker_release(1u);                    /* 0x43773 0x1D810 */
    fight_char_screen_setup();                          /* 0x43778 0x43818 */
    for (u32 side = 0; side < 2u; side++) {             /* 0x437B1..0x437BB */
        if ((DSB(DS_00104B1F) & (side + 1u)) != 0u) {   /* 0x43782..0x4378A */
            DSB(DS_00108170 + side) = 1u;               /* 0x43790 */
            fight_char_entry_spawn(side);               /* 0x43796 0x43964 */
            fight_char_select_actor(side);              /* 0x4379D 0x43A08 */
        } else {
            DSB(DS_00108170 + side) = 0u;               /* 0x437A6 */
        }
        DSD(DS_00108144 + side * 4u) = 0u;              /* 0x437B2 */
    }
    DSW(DS_0010816C) = DSB(DS_00108173) != 0u ? 5u : 0xFu;  /* 0x437C6/0x437D1 */
    text_number_draw_font2(0x13, 1, (s16)DSW(DS_0010816C), 2,
                           0u, 0x4000u);                /* 0x437FE 0x2F528 */
    DSD(DS_00104AE4) = FN_00029D60;                     /* 0x43805 (a bare `ret`) */
    DSB(DS_00108174) = 0u;                              /* 0x4380B */
}

/* 0x444C8 — record §42-F. 0x43738's prologue (0x444CC..0x44508), then both
 * sides unconditionally: DS_00108170[side] = 1, 0x43964, 0x43A08 and
 * DS_00108144[side] = 0 (EBX advanced by 4 at 0x4451E before the 0x44529
 * store). No countdown draw; DS_00108174 = 0 and the hook becomes 0x29D60.
 * Its only caller is 0x4462B (in 0x4454C). */
void fight_char_screen_open_both(void)
{
    /* PORT: 0x444D1 0x2C3FC(0x30) voice, not wired (record §45-A). */
    effects_clear();                                    /* 0x444D6 0x13DF0 */
    attract_config_volumes_unscaled();                  /* 0x444E0 0x2C8F0(-1) */
    DSB(DS_00104B1B) = 0u;                              /* 0x444E7 */
    DSB(DS_00104B24) = 0u;                              /* 0x444ED */
    fight_select_marker_release(0u);                    /* 0x444F7 0x1D810 */
    fight_select_marker_release(1u);                    /* 0x44503 0x1D810 */
    fight_char_screen_setup();                          /* 0x44508 0x43818 */
    for (u32 side = 0; side < 2u; side++) {             /* 0x44528..0x44532 */
        DSB(DS_00108170 + side) = 1u;                   /* 0x44511 */
        fight_char_entry_spawn(side);                   /* 0x44517 0x43964 */
        fight_char_select_actor(side);                  /* 0x44521 0x43A08 */
        DSD(DS_00108144 + side * 4u) = 0u;              /* 0x44529 */
    }
    DSB(DS_00108174) = 0u;                              /* 0x4453B */
    DSD(DS_00104AE4) = FN_00029D60;                     /* 0x44541 */
}

/* ---- mode 0x10: the handler 0x438B4 and its join test 0x43928 (§47-M) ---- */

/* 0x43928 — record §47-M. For each side 0..1 (EDX, 0x4392C..0x4395E) whose
 * byte DS_00108170[side] is 0, 0x11F28 (the coin/start poll, EAX = side) is
 * run; when it accepts, DS_00104B1F |= side + 1 (0x43942..0x4394E, AL = DL +
 * 1 or'd into BL) and DS_00108170[side] = 1 (BH, 0x43954). A side whose byte
 * is non-zero is not polled. EBX/EDX are pushed and popped; EAX (the last
 * 0x11F28 result, or the side) is not read by 0x438B4's `call 0x4f790`. */
void fight_char_join(void)
{
    for (u32 side = 0; side < 2u; side++) {             /* 0x4392C/0x4395A/0x4395B */
        if (DSB(DS_00108170 + side) != 0u) continue;    /* 0x4392E/0x43935 */
        if (frontend_coin_poll(side) == 0u) continue;   /* 0x43939/0x4393E */
        DSB(DS_00104B1F) = (u8)(DSB(DS_00104B1F) | (side + 1u));  /* 0x43942..0x4394E */
        DSB(DS_00108170 + side) = 1u;                   /* 0x43954 (BH) */
    }
}

/* 0x438B4 — record §47-M. The mode 0x10 handler (0x24C5C case 0x10, the jump
 * table 0x24B8C entry 0x25385, its only caller). It branches on the byte
 * DS_00108174 (`test al,al; jbe` is == 0), with two copies of the same body
 * split on DS_00104B1D == 3:
 * - 0: 0x44798 (DS_00104B1D == 3) or 0x43B24 (otherwise), the character
 *   select's per-frame pass (record §48-S);
 * - 1: 0x43928, then 0x4F790's AL. When AL is non-zero, DS_00108174 takes the
 *   byte DS_00108172 (0x4391C/0x43921). When it is 0, the DS_00104B1D == 3 copy
 *   returns; the other decrements the word DS_0010816C (`dec edx` on the
 *   loaded DX, stored as a word) and takes DS_00108172 once the new value is
 *   <= 0 (signed, `test dx,dx; jg`);
 * - anything else: return.
 * EDX is pushed and popped; the returned EAX is dead (record §47-C.2: the
 * 0x2540F tail writes EAX before reading it). */
void fight_mode_10_step(void)
{
    u32 sub = DSB(DS_00108174);                         /* 0x438BE / 0x438E4 */
    if (DSB(DS_00104B1D) == 3u) {                       /* 0x438B5/0x438BC */
        if (sub == 0u) {                                /* 0x438C3/0x438C5 */
            fight_char_team_pass();                     /* 0x438CD 0x44798 */
            return;
        }
        if (sub != 1u) return;                          /* 0x438C7/0x438C9 */
        fight_char_join();                              /* 0x438D4 0x43928 */
        if ((frontend_skip_check() & 0xFFu) == 0u)      /* 0x438D9 0x4F790, 0x438DE */
            return;                                     /* 0x438E0 jz 0x43926 */
    } else {
        if (sub == 0u) {                                /* 0x438E9/0x438EB */
            fight_char_select_pass();                   /* 0x438F3 0x43B24 */
            return;
        }
        if (sub != 1u) return;                          /* 0x438ED/0x438EF */
        fight_char_join();                              /* 0x438FA 0x43928 */
        if ((frontend_skip_check() & 0xFFu) == 0u) {    /* 0x438FF 0x4F790, 0x43904 */
            u16 dx = (u16)(DSW(DS_0010816C) - 1u);      /* 0x43908/0x4390F */
            DSW(DS_0010816C) = dx;                      /* 0x43910 */
            if ((s16)dx > 0) return;                    /* 0x43917/0x4391A */
        }
    }
    DSB(DS_00108174) = DSB(DS_00108172);                /* 0x4391C/0x43921 */
}

/* ---- mode 0x10 sub-state 0: the character select's pass 0x43B24 (§48-S) - */

#define DS_00104529 0x00104529u   /* no symbols.h name: DS_00104528's second byte */
#define DS_000C8934 0x000C8934u   /* no symbols.h name: the joined side's prompt sprite */
#define DS_000C8948 0x000C8948u   /* no symbols.h name: [side] that sprite's x word */
#define DS_000C888A 0x000C888Au   /* no symbols.h name: [char] the cursor voice word */
#define DS_000BB988 0x000BB988u   /* no symbols.h name: [class] the select stream */
#define DS_000BB9B0 0x000BB9B0u   /* no symbols.h name: [class] the select palette */
#define FN_000430E8 0x000430E8u   /* no symbols.h name: the versus-screen hook */

/* 0x432A0 — record §48-S. EAX = side (kept in ECX). The unjoined side's
 * prompt: when 0x2C060 reports a credit, 0x2C178 with EDX = 0x3700 (the
 * DS_00104529 bit 1, the sprite prompt's y) or 0x1B (the text row); with no
 * credit, 0x2C1C8 with EDX = 0x1B (EBX = 0 is not read). */
void fight_char_prompt(u32 side)
{
    if (config_credit_ready() != 0u) {                  /* 0x432A5/0x432AA */
        if ((DSB(DS_00104529) & 2u) != 0u)              /* 0x432AE */
            prompt_press_start(side, 0x3700);           /* 0x432B7..0x432BE 0x2C178 */
        else
            prompt_press_start(side, 0x1B);             /* 0x432C7..0x432CE 0x2C178 */
        return;
    }
    prompt_insert_coin(side, 0x1B);                     /* 0x432D7..0x432E0 0x2C1C8 */
}

/* 0x432EC — record §48-S. EAX = side, EDX = `flag`. With `flag`, the side's
 * prompt sprite DS_00108144[side], when set, is marked dead (0x2B150; the
 * slot is not zeroed) and 0x2C088 runs with col = row = 0 and EBX = side.
 * Then strings 0x36, 0x37, 0x38 are released at the col word 0xC894C[side],
 * rows r - 2, r - 1 and r, and string 0x39 at r + 1 when it is at least 2
 * long, with mode 0x1000. r is 0x1B, or 0x1C when string 0x39 is shorter
 * than 2 (0x65624 is strlen; the compare is signed). */
void fight_char_text_clear(u32 side, u32 flag)
{
    s32 row = 0x1B;                                     /* 0x432F3 */
    if (flag != 0u) {                                   /* 0x432F8 */
        u32 rec = DSD(DS_00108144 + side * 4u);         /* 0x432FF */
        if (rec != 0u) actor_set_dead(rec);             /* 0x43305/0x4330B 0x2B150 */
        prompt_press_start_clear(0, 0, side);           /* 0x43310..0x43316 0x2C088 */
    }
    if ((s32)strlen((const char *)game_string_get(0x39u)) < 2) row = 0x1C;  /* 0x4331B..0x4332F */
    s32 col = (s32)DSW(DS_000C894C + side * 2u);        /* 0x43349 */
    text_cells_release(col, row - 2, game_string_get(0x36u), 0x1000u);  /* 0x43330..0x43354 */
    text_cells_release(col, row - 1, game_string_get(0x37u), 0x1000u);  /* 0x43359..0x43376 */
    text_cells_release(col, row, game_string_get(0x38u), 0x1000u);      /* 0x4337B..0x43397 */
    if ((s32)strlen((const char *)game_string_get(0x39u)) >= 2)          /* 0x4339C..0x433AE */
        text_cells_release(col, row + 1, game_string_get(0x39u), 0x1000u);  /* 0x433B0..0x433CF */
}

/* 0x433DC — record §48-S. EAX = side, EDX = `flag`: 0x432EC's `flag` arm,
 * then strings 0x235 and 0x236 released at the col word 0xC894C[side], rows
 * 0x1B and 0x1C, mode 0x1000. */
void fight_char_opp_text_clear(u32 side, u32 flag)
{
    if (flag != 0u) {                                   /* 0x433E2 */
        u32 rec = DSD(DS_00108144 + side * 4u);         /* 0x433E9 */
        if (rec != 0u) actor_set_dead(rec);             /* 0x433EF/0x433F5 0x2B150 */
        prompt_press_start_clear(0, 0, side);           /* 0x433FA..0x43400 0x2C088 */
    }
    s32 col = (s32)DSW(DS_000C894C + side * 2u);        /* 0x43422 */
    text_cells_release(col, 0x1B, game_string_get(0x235u), 0x1000u);  /* 0x43405..0x4342D */
    text_cells_release(col, 0x1C, game_string_get(0x236u), 0x1000u);  /* 0x43432..0x43457 */
}

/* 0x43464 — record §48-S. EAX = side. The joined side's blinking text, on the
 * phase DS_000EF6DC & 0x1F:
 * - 0: with the DS_00104529 bit 1, 0x432EC(side, 0) and the sprite 0xC8934
 *   at (a2 = 0xC8948[side], a3 0xFF, a4 0x3700, a5 0) into
 *   DS_00108144[side]; otherwise strings 0x36..0x38 (and 0x39 when at least
 *   2 long) drawn by 0x2F198 where 0x432EC releases them;
 * - 0x18: 0x432EC(side, DS_00104528 & 0x200). */
void fight_char_text_blink(u32 side)
{
    u32 phase = (u32)DSW(DS_000EF6DC) & 0x1Fu;         /* 0x4346C..0x4347B */
    if (phase != 0u) {                                  /* 0x43480 */
        if (phase == 0x18u)                             /* 0x4358B */
            fight_char_text_clear(side, DSD(DS_00104528) & 0x200u);  /* 0x43590..0x4359E */
        return;
    }
    if ((DSB(DS_00104529) & 2u) != 0u) {                /* 0x43486 */
        fight_char_text_clear(side, 0u);                /* 0x4348F..0x43493 0x432EC */
        DSD(DS_00108144 + side * 4u) = actor_spawn(
            (const u32 *)(mem + DS_000C8934),
            DSW(DS_000C8948 + side * 2u), 0xFFu, 0x3700u, 0u);  /* 0x43498..0x434BC */
        return;
    }
    s32 row = 0x1B;                                     /* 0x43474 */
    if ((s32)strlen((const char *)game_string_get(0x39u)) < 2) row = 0x1C;  /* 0x434C8..0x434DC */
    s32 col = (s32)DSW(DS_000C894C + side * 2u);        /* 0x434F5 */
    text_cursor_set(col, row - 2, game_string_get(0x36u), 0x1000u);  /* 0x434E1..0x43500 */
    text_cursor_set(col, row - 1, game_string_get(0x37u), 0x1000u);  /* 0x43505..0x43522 */
    text_cursor_set(col, row, game_string_get(0x38u), 0x1000u);      /* 0x43527..0x43543 */
    if ((s32)strlen((const char *)game_string_get(0x39u)) >= 2)      /* 0x43548..0x4355A */
        text_cursor_set(col, row + 1, game_string_get(0x39u), 0x1000u);  /* 0x4355C..0x4357F */
}

/* 0x435AC — record §48-S. EAX = side. 0x43464's twin for the side that is
 * not DS_00104AB8's with DS_00104B1D == 1: on phase 0 the same sprite (after
 * 0x432EC(side, 0)) or strings 0x235/0x236 drawn at rows 0x1B/0x1C; on phase
 * 0x18, 0x433DC(side, DS_00104528 & 0x200). */
void fight_char_opp_text_blink(u32 side)
{
    u32 phase = (u32)DSW(DS_000EF6DC) & 0x1Fu;         /* 0x435B3..0x435BD */
    if (phase != 0u) {                                  /* 0x435C2 */
        if (phase == 0x18u)                             /* 0x4365D */
            fight_char_opp_text_clear(side, DSD(DS_00104528) & 0x200u);  /* 0x43662..0x43670 */
        return;
    }
    if ((DSB(DS_00104529) & 2u) != 0u) {                /* 0x435C8/0x435D5 */
        fight_char_text_clear(side, 0u);                /* 0x435DF..0x435E8 0x432EC */
        DSD(DS_00108144 + side * 4u) = actor_spawn(
            (const u32 *)(mem + DS_000C8934),
            DSW(DS_000C8948 + side * 2u), 0xFFu, 0x3700u, 0u);  /* 0x435ED..0x43602 */
        return;
    }
    s32 col = (s32)DSW(DS_000C894C + side * 2u);        /* 0x43621 */
    text_cursor_set(col, 0x1B, game_string_get(0x235u), 0x1000u);  /* 0x4360B..0x4362C */
    text_cursor_set(col, 0x1C, game_string_get(0x236u), 0x1000u);  /* 0x43631..0x43652 */
}

/* 0x43EA0 — record §48-S. EAX = side (ECX), the other side EAX ^ 1. When the
 * other side's byte DS_00108170 is 1: with both cursors DS_00108166 equal, the
 * side's entry DS_00108154[side] takes +8 = 0xC88D4[side] and the other's
 * +8 = 0xC88CC[other] and +0x28 |= 4; otherwise the side's +8 =
 * 0xC88CC[side]. Then, with ch the side's signed cursor, the entry's +0x18 =
 * 0xC8898[ch] and +0x1C = 0xC88A6[ch] (zero-extended words stored as dwords),
 * +0x28 |= 4, the panel DS_0010815C[side] re-pointed at 0xC88DC[ch] (0x2BCF4)
 * with the pset word 0xC88F8[ch] and palette 0xC8908[ch] (0x2A17C), and the
 * voice 0xC888A[ch]. */
void fight_char_portrait(u32 side)
{
    u32 other = side ^ 1u;                              /* 0x43EA6 */
    if (DSB(DS_00108170 + other) == 1u) {               /* 0x43EAA/0x43EB0 */
        if (DSB(DS_00108166 + side) == DSB(DS_00108166 + other)) {  /* 0x43EB5..0x43ECA */
            DSD(DSD(DS_00108154 + side * 4u) + 8u) =
                DSD(DS_000C88D4 + side * 4u);           /* 0x43ECC..0x43ED8 */
            DSD(DSD(DS_00108154 + other * 4u) + 8u) =
                DSD(DS_000C88CC + other * 4u);          /* 0x43EDB..0x43EE9 */
            DSB(DSD(DS_00108154 + other * 4u) + 0x28u) |= 4u;  /* 0x43EEC/0x43EF3 */
        } else {
            DSD(DSD(DS_00108154 + side * 4u) + 8u) =
                DSD(DS_000C88CC + side * 4u);           /* 0x43EF9..0x43F05 */
        }
    }
    s32 ch = (s8)DSB(DS_00108166 + side);               /* 0x43F08/0x43F0E */
    DSD(DSD(DS_00108154 + side * 4u) + 0x18u) =
        DSW(DS_000C8898 + (u32)(ch * 2));               /* 0x43F11..0x43F25 */
    DSD(DSD(DS_00108154 + side * 4u) + 0x1Cu) =
        DSW(DS_000C88A6 + (u32)(ch * 2));               /* 0x43F28..0x43F45 */
    DSB(DSD(DS_00108154 + side * 4u) + 0x28u) |= 4u;    /* 0x43F48/0x43F4F */
    actors_anim_seek(DSD(DS_0010815C + side * 4u),
                     DSD(DS_000C88DC + (u32)(ch * 4)));  /* 0x43F53..0x43F6A 0x2BCF4 */
    actor_pset_palette(DSD(DS_0010815C + side * 4u),
                       DSW(DS_000C88F8 + (u32)(ch * 2)),
                       DSD(DS_000C8908 + (u32)(ch * 4)));  /* 0x43F6F..0x43F96 0x2A17C */
    /* PORT: 0x43FB1 0x2C3FC(0xC888A[ch]) voice, not wired (record §45-A). */
}

/* 0x43FBC — record §48-S. EAX = side. With ch the side's signed cursor and
 * cls the signed byte 0xC8882[ch], the side's fighter DS_0010813C[side] takes
 * +0x18 = 0xC88B4[side] and +0x1C = 0xC88B8[side] (zero-extended words stored
 * as dwords), the stream 0xBB988[cls] at 3.0 (0x2BC30, `push 0x40400000`),
 * the pset word 0x1C with the palette 0xBB9B0[cls] (0x2A17C); then the marker
 * 0x1D7B8(side, cls) with EBX = 0x1800, and the fighter's +0x2C word =
 * 0xC8924[cls]. */
void fight_char_fighter(u32 side)
{
    s32 ch = (s8)DSB(DS_00108166 + side);               /* 0x43FC2/0x43FD1 */
    s32 cls = (s8)DSB(DS_000C8882 + (u32)ch);           /* 0x43FDC/0x43FF6 */
    DSD(DSD(DS_0010813C + side * 4u) + 0x18u) =
        DSW(DS_000C88B4 + side * 2u);                   /* 0x43FC8..0x43FE2 */
    DSD(DSD(DS_0010813C + side * 4u) + 0x1Cu) =
        DSW(DS_000C88B8 + side * 2u);                   /* 0x43FE5..0x43FF9 */
    actors_anim_begin(DSD(DS_0010813C + side * 4u),
                      DSD(DS_000BB988 + (u32)(cls * 4)),
                      0x40400000u);                     /* 0x43FFC..0x4400F 0x2BC30 */
    actor_pset_palette(DSD(DS_0010813C + side * 4u), 0x1Cu,
                       DSD(DS_000BB9B0 + (u32)(cls * 4)));  /* 0x44014..0x44027 0x2A17C */
    fight_select_marker_spawn(side, (u32)cls, 0x1800u); /* 0x4402C..0x44035 0x1D7B8 */
    DSW(DSD(DS_0010813C + side * 4u) + 0x2Cu) =
        DSW(DS_000C8924 + (u32)(cls * 2));              /* 0x4403A..0x44049 */
}

/* 0x43D0C — record §48-S. EAX = side. The confirm button in the command word
 * DS_001088E0[side]: 1 for bit 0x200, else 2 for 0x400, else 3 for 0x800,
 * else 0 (the `and eax,0xffff` result, 0x43D51). */
u32 fight_char_button(u32 side)
{
    u16 w = DSW(DS_001088E0 + side * 2u);               /* 0x43D0F */
    if ((w & 0x200u) != 0u) return 1u;                  /* 0x43D16..0x43D23 */
    if ((w & 0x400u) != 0u) return 2u;                  /* 0x43D2A..0x43D3E */
    if ((w & 0x800u) != 0u) return 3u;                  /* 0x43D45..0x43D58 */
    return 0u;                                          /* 0x43D5D */
}

/* 0x4248C — record §48-S. EAX = cls, EDX = side. Each of the seven stage
 * bytes DS_00108106 whose low 7 bits equal cls | side << 6 (`shl dl,6`, `or
 * ch,dl`) becomes (b & 0x7F) ^ that value, i.e. 0, and the byte count
 * DS_00108111 is decremented once per match (0x4249F..0x424C6). */
void fight_stage_mark_drop(u32 cls, u32 side)
{
    u8 count = DSB(DS_00108111);                        /* 0x4248E */
    u8 key = (u8)((u8)cls | (u8)(side << 6));           /* 0x42494..0x4249D */
    for (u32 i = 0; i < 7u; i++) {                      /* 0x424C0..0x424C4 */
        u8 b = (u8)(DSB(DS_00108106 + i) & 0x7Fu);      /* 0x4249F/0x424A5 */
        if (b == key) {                                 /* 0x424B2 */
            count--;                                    /* 0x424B8 */
            DSB(DS_00108106 + i) = (u8)(b ^ key);       /* 0x424B6/0x424BA */
        }
    }
    DSB(DS_00108111) = count;                           /* 0x424C6 */
}

/* 0x43D60 — record §48-S. EAX = side (EBX). The confirm: the class
 * DS_0010816A[side] = 0xC8882[cursor], the side byte = 2 (AH), the fighter
 * slot byte DS_00107813 + side * 0x94 = 0 (`lea`/`shl` to side * 37, `[eax*4
 * + 0x107813]`), and DS_00105B34[side] = 0x43D0C(side). When the other side
 * already has that class (its DS_0010816E byte, or, while that byte is
 * 0xFF, its DS_0010816A byte with its side byte 2) with the same button,
 * DS_00105B34[side] becomes 1 if the other's is 0, else 0. Unless the side's
 * DS_0010816E byte is not 0xFF and already equals the class (0x43E43 jz
 * 0x43E69): a non-0xFF previous class runs 0x33C18 first, then 0x4248C(class,
 * side) and DS_0010816E[side] = the class. Last, the audit count, the entry
 * DS_00108154[side] marked dead (0x2B150), 0x432EC(side, 0) and the 0x6C
 * voice. */
void fight_char_confirm(u32 side)
{
    s32 ch = (s8)DSB(DS_00108166 + side);               /* 0x43D65/0x43D6B */
    DSB(DS_0010816A + side) = DSB(DS_000C8882 + (u32)ch);  /* 0x43D6E/0x43D74 */
    DSB(DS_00108170 + side) = 2u;                       /* 0x43D7A/0x43D7E */
    DSB(DS_00107813 + side * 0x94u) = 0u;               /* 0x43D84..0x43D94 */
    u32 btn = fight_char_button(side);                  /* 0x43D9D 0x43D0C */
    DSB(DS_00105B34 + side) = (u8)btn;                  /* 0x43DA4 */
    u32 other = side ^ 1u;                              /* 0x43DAA/0x43DAC */
    int clash;
    if ((s8)DSB(DS_0010816E + other) != -1) {           /* 0x43DAE..0x43DBA */
        clash = DSB(DS_0010816A + side) == DSB(DS_0010816E + other)  /* 0x43DBC/0x43DC2 */
             && (u32)DSB(DS_00105B34 + other) == btn;   /* 0x43DCA..0x43DD2 */
    } else {
        clash = DSB(DS_00108170 + other) == 2u          /* 0x43DE8..0x43DF0 */
             && DSB(DS_0010816A + other) == DSB(DS_0010816A + side)  /* 0x43DF5/0x43DFB */
             && (u32)DSB(DS_00105B34 + other) == btn;   /* 0x43E03..0x43E0B */
    }
    if (clash)
        DSB(DS_00105B34 + side) = DSB(DS_00105B34 + other) == 0u ? 1u : 0u;  /* 0x43DD6..0x43E23 */
    if ((s8)DSB(DS_0010816E + side) == -1
        || DSB(DS_0010816E + side) != DSB(DS_0010816A + side)) {  /* 0x43E29..0x43E43 */
        if ((s8)DSB(DS_0010816E + side) != -1)          /* 0x43E32 */
            fight_char_reset(side);                     /* 0x43E45/0x43E47 0x33C18 */
        fight_stage_mark_drop(DSB(DS_0010816A + side), side);  /* 0x43E4C..0x43E58 0x4248C */
        DSB(DS_0010816E + side) = DSB(DS_0010816A + side);     /* 0x43E5D/0x43E63 */
    }
    /* PORT: 0x43E77 0x2E934(2, (s8)DS_0010816A[side]), the character-pick
     * audit count (0x2E180/0x2E034 into the config region, 0x2D4EC), is a
     * named gap with the other audit adds (spec §7, record §48-S). */
    actor_set_dead(DSD(DS_00108154 + side * 4u));       /* 0x43E7C/0x43E83 0x2B150 */
    fight_char_text_clear(side, 0u);                    /* 0x43E88..0x43E8C 0x432EC */
    /* PORT: 0x43E96 0x2C3FC(0x6C) voice, not wired (record §45-A). */
}

/* 0x43AAC — record §48-S. The countdown step 0x43B24 runs every 64th frame:
 * the word DS_0010816C is decremented (`dec edx`, stored as a word). While
 * it stays > 0 (signed) it is redrawn by 0x2F528 at col 0x13, row 1, width 2,
 * pad 0, mode 0x4000 (EBX = `[0x10816a] sar 0x10`, the signed word) and AL =
 * 0 is returned. Otherwise each side whose byte is 1 is confirmed (0x43D60),
 * the hook becomes 0x430E8, 0x4F980 arms mode 0x1A returning to 0x11, and AL
 * = 1. */
u32 fight_char_countdown(void)
{
    u16 dx = (u16)(DSW(DS_0010816C) - 1u);              /* 0x43AAF/0x43AB6 */
    DSW(DS_0010816C) = dx;                              /* 0x43AB7 */
    if ((s16)dx > 0) {                                  /* 0x43ABE/0x43AC1 */
        text_number_draw_font2(0x13, 1, (s16)DSW(DS_0010816C), 2,
                               0u, 0x4000u);            /* 0x43AF9..0x43B18 0x2F528 */
        return 0u;                                      /* 0x43B1D */
    }
    for (u32 side = 0; side < 2u; side++)               /* 0x43AC3..0x43ADC */
        if (DSB(DS_00108170 + side) == 1u)              /* 0x43AC5..0x43ACF */
            fight_char_confirm(side);                   /* 0x43AD3 0x43D60 */
    DSD(DS_00104AE4) = FN_000430E8;                     /* 0x43AE8 */
    frontend_wipe_arm(0x11u);                           /* 0x43AEE 0x4F980 */
    return 1u;                                          /* 0x43AF3 */
}

/* 0x43B24 — record §48-S. The character select's per-frame pass (mode 0x10,
 * sub-state 0, DS_00104B1D != 3). Its only caller is 0x438F3 (0x438B4). For
 * side 0..1 (EDX; EBX = side * 2), on the side byte DS_00108170:
 * - 0: the poll 0x11F28(side); accepted, 0x43964, 0x43A08, DS_00104B1F |=
 *   side + 1 and the byte = 1; refused, the prompt 0x432A0;
 * - 1: 0x435AC with DS_00104B1D == 1 and side + 1 != the dword
 *   DS_00104AB8, else 0x43464; then the stick and confirm below;
 * - 2: when the other side's byte is 0 or 2, the hook 0x430E8 and 0x4F980
 *   (0x11), and the pass returns at once; otherwise the next side;
 * - above 2: the stick and confirm only.
 * The stick is bits 4..7 of the command word DS_001088E0[side] (`and al,0xf0`
 * with AH cleared): 0x10 moves the cursor DS_00108166[side] right while it is
 * < 6, 0x20 left while it is > 0, 0x40 down 4 while it is < 4 and then clamps
 * a cursor >= 7 to 6, 0x80 up 4 while it is >= 4, any other non-zero value
 * leaves it (all signed, re-read each time); every non-zero value then runs
 * 0x43EA0 and 0x43FBC. Bit 0 of the word confirms through 0x43D60. After the
 * loop, a set DS_00105C04 resets the countdown DS_0010816C to 0xF and is
 * cleared, and 0x43AAC runs when DS_000EF6DC & 0x3F == 0 (its AL is not
 * read). */
void fight_char_select_pass(void)
{
    for (u32 side = 0; side < 2u; side++) {             /* 0x43B27..0x43CCD */
        u8 b = DSB(DS_00108170 + side);                 /* 0x43B2B */
        if (b == 0u) {                                  /* 0x43B33/0x43B44 */
            if (frontend_coin_poll(side) != 0u) {       /* 0x43B4E/0x43B55 */
                fight_char_entry_spawn(side);           /* 0x43B59 0x43964 */
                fight_char_select_actor(side);          /* 0x43B60 0x43A08 */
                DSB(DS_00104B1F) = (u8)(DSB(DS_00104B1F) | (side + 1u));  /* 0x43B65..0x43B71 */
                DSB(DS_00108170 + side) = 1u;           /* 0x43B77 */
            } else {
                fight_char_prompt(side);                /* 0x43B85 0x432A0 */
            }
            continue;                                   /* 0x43B7E/0x43B8A */
        }
        if (b == 1u) {                                  /* 0x43B35 */
            if (DSB(DS_00104B1D) == 1u
                && side + 1u != DSD(DS_00104AB8))       /* 0x43B8F..0x43BA3 */
                fight_char_opp_text_blink(side);        /* 0x43BA7 0x435AC */
            else
                fight_char_text_blink(side);            /* 0x43BB0 0x43464 */
        } else if (b == 2u) {                           /* 0x43B37/0x43B39 */
            u8 o = DSB(DS_00108170 + (side ^ 1u));      /* 0x43BF0..0x43BF4 */
            if (o != 0u && o != 2u) continue;           /* 0x43BFA..0x43C08 */
            DSD(DS_00104AE4) = FN_000430E8;             /* 0x43C0E/0x43C18 */
            frontend_wipe_arm(0x11u);                   /* 0x43C1E 0x4F980 */
            return;                                     /* 0x43C23 */
        }
        u32 stick = (u32)DSW(DS_001088E0 + side * 2u) & 0xF0u;  /* 0x43BB5..0x43BC2 */
        if (stick != 0u) {                              /* 0x43BC5/0x43BC7 */
            if (stick == 0x10u) {                       /* 0x43C27/0x43C75 */
                if ((s8)DSB(DS_00108166 + side) < 6)    /* 0x43C7E */
                    DSB(DS_00108166 + side)++;          /* 0x43C83 */
            } else if (stick == 0x20u) {                /* 0x43BD3/0x43C8B */
                s8 c = (s8)DSB(DS_00108166 + side);
                if (c > 0) DSB(DS_00108166 + side) = (u8)(c - 1);  /* 0x43C91..0x43C99 */
            } else if (stick == 0x40u) {                /* 0x43BE3/0x43C49 */
                if ((s8)DSB(DS_00108166 + side) < 4)    /* 0x43C52 */
                    DSB(DS_00108166 + side) = (u8)(DSB(DS_00108166 + side) + 4u);  /* 0x43C57 */
                if ((s8)DSB(DS_00108166 + side) >= 7)   /* 0x43C5E..0x43C6A */
                    DSB(DS_00108166 + side) = 6u;       /* 0x43C6C */
            } else if (stick == 0x80u) {                /* 0x43BE9/0x43C32 */
                if ((s8)DSB(DS_00108166 + side) >= 4)   /* 0x43C3B */
                    DSB(DS_00108166 + side) = (u8)(DSB(DS_00108166 + side) - 4u);  /* 0x43C40 */
            }
            fight_char_portrait(side);                  /* 0x43CA1 0x43EA0 */
            fight_char_fighter(side);                   /* 0x43CA8 0x43FBC */
        }
        if ((DSW(DS_001088E0 + side * 2u) & 1u) != 0u)  /* 0x43CAD..0x43CBD */
            fight_char_confirm(side);                   /* 0x43CC1 0x43D60 */
    }
    if (DSB(DS_00105C04) != 0u) {                       /* 0x43CD3 */
        DSW(DS_0010816C) = 0xFu;                        /* 0x43CE3 */
        DSB(DS_00105C04) = 0u;                          /* 0x43CEA */
    }
    if ((DSW(DS_000EF6DC) & 0x3Fu) == 0u)               /* 0x43CF0..0x43CFF */
        (void)fight_char_countdown();                   /* 0x43D01 0x43AAC */
}

/* ---- mode 0x10 sub-state 0 with DS_00104B1D == 3: 0x44798 (§48-S) ------- */

/* 0x44638 — record §48-S. EAX = side (ESI). 0x43464's twin in the
 * DS_00104B1D == 3 pass. Phase 0 of DS_000EF6DC & 0x1F: the sprite form is
 * 0x43464's; the text form draws strings 0x36, 0x37, 0x38 at rows r - 2..r
 * (r = 0x1B, or 0x1C when string 0x39 is shorter than 2), then r is
 * incremented when string 0x39 is at least 2 long and string 0x39 is drawn
 * at r + 1 whatever its length (0x4473B `jl` skips only the `inc edi`).
 * Phase 0x18: 0x432EC(side, DS_00104528 & 0x200). */
void fight_char_team_text_blink(u32 side)
{
    u32 phase = (u32)DSW(DS_000EF6DC) & 0x1Fu;         /* 0x44643..0x44652 */
    if (phase != 0u) {                                  /* 0x44657 */
        if (phase == 0x18u)                             /* 0x44774 */
            fight_char_text_clear(side, DSD(DS_00104528) & 0x200u);  /* 0x44779..0x44787 */
        return;
    }
    if ((DSB(DS_00104529) & 2u) != 0u) {                /* 0x4465D */
        fight_char_text_clear(side, 0u);                /* 0x44666..0x4466A 0x432EC */
        DSD(DS_00108144 + side * 4u) = actor_spawn(
            (const u32 *)(mem + DS_000C8934),
            DSW(DS_000C8948 + side * 2u), 0xFFu, 0x3700u, 0u);  /* 0x4466F..0x44693 */
        return;
    }
    s32 row = 0x1B;                                     /* 0x4464B */
    if ((s32)strlen((const char *)game_string_get(0x39u)) < 2) row = 0x1C;  /* 0x4469F..0x446B3 */
    s32 col = (s32)DSW(DS_000C894C + side * 2u);        /* 0x446C9 */
    text_cursor_set(col, row - 2, game_string_get(0x36u), 0x1000u);  /* 0x446B8..0x446DC */
    text_cursor_set(col, row - 1, game_string_get(0x37u), 0x1000u);  /* 0x446E1..0x44705 */
    text_cursor_set(col, row, game_string_get(0x38u), 0x1000u);      /* 0x4470A..0x44727 */
    if ((s32)strlen((const char *)game_string_get(0x39u)) >= 2)      /* 0x4472C..0x4473E */
        row++;                                          /* 0x44740 */
    text_cursor_set(col, row + 1, game_string_get(0x39u), 0x1000u);  /* 0x44741..0x44765 */
}

/* 0x4418C — record §48-S. EAX = side (ECX), the other side EDX ^ 1. 0x43EA0
 * without its other-side byte test: nothing when the side's entry
 * DS_00108154[side] is 0 (0x4419F); otherwise the highlight on the two
 * cursors alone, then 0x43EA0's entry, panel and voice updates. */
void fight_char_team_portrait(u32 side)
{
    u32 other = side ^ 1u;                              /* 0x44191/0x4419C */
    if (DSD(DS_00108154 + side * 4u) == 0u) return;     /* 0x44196..0x441A1 */
    if (DSB(DS_00108166 + side) == DSB(DS_00108166 + other)) {  /* 0x441A7..0x441B3 */
        DSD(DSD(DS_00108154 + side * 4u) + 8u) =
            DSD(DS_000C88D4 + side * 4u);               /* 0x441B5..0x441C1 */
        DSD(DSD(DS_00108154 + other * 4u) + 8u) =
            DSD(DS_000C88CC + other * 4u);              /* 0x441C4..0x441D7 */
        DSB(DSD(DS_00108154 + other * 4u) + 0x28u) |= 4u;  /* 0x441DA/0x441E0 */
    } else {
        DSD(DSD(DS_00108154 + side * 4u) + 8u) =
            DSD(DS_000C88CC + side * 4u);               /* 0x441E6..0x441F2 */
    }
    s32 ch = (s8)DSB(DS_00108166 + side);               /* 0x441F5/0x441FB */
    DSD(DSD(DS_00108154 + side * 4u) + 0x18u) =
        DSW(DS_000C8898 + (u32)(ch * 2));               /* 0x441FE..0x4420F */
    DSD(DSD(DS_00108154 + side * 4u) + 0x1Cu) =
        DSW(DS_000C88A6 + (u32)(ch * 2));               /* 0x44212..0x4422F */
    DSB(DSD(DS_00108154 + side * 4u) + 0x28u) |= 4u;    /* 0x44232/0x44239 */
    actors_anim_seek(DSD(DS_0010815C + side * 4u),
                     DSD(DS_000C88DC + (u32)(ch * 4)));  /* 0x4423D..0x44254 0x2BCF4 */
    actor_pset_palette(DSD(DS_0010815C + side * 4u),
                       DSW(DS_000C88F8 + (u32)(ch * 2)),
                       DSD(DS_000C8908 + (u32)(ch * 4)));  /* 0x44259..0x4427A 0x2A17C */
    /* PORT: 0x44295 0x2C3FC(0xC888A[ch]) voice, not wired (record §45-A). */
}

/* 0x442A0 — record §48-S. EAX = side. 0x43FBC, skipped when the side's
 * fighter DS_0010813C[side] is 0 (0x442AE). */
void fight_char_team_fighter(u32 side)
{
    if (DSD(DS_0010813C + side * 4u) == 0u) return;     /* 0x442AE/0x442B5 */
    s32 ch = (s8)DSB(DS_00108166 + side);               /* 0x442BB/0x442C9 */
    s32 cls = (s8)DSB(DS_000C8882 + (u32)ch);           /* 0x442D4/0x442F0 */
    DSD(DSD(DS_0010813C + side * 4u) + 0x18u) =
        DSW(DS_000C88B4 + side * 2u);                   /* 0x442C1..0x442DA */
    DSD(DSD(DS_0010813C + side * 4u) + 0x1Cu) =
        DSW(DS_000C88B8 + side * 2u);                   /* 0x442DD..0x442F3 */
    actors_anim_begin(DSD(DS_0010813C + side * 4u),
                      DSD(DS_000BB988 + (u32)(cls * 4)),
                      0x40400000u);                     /* 0x442F6..0x44308 0x2BC30 */
    actor_pset_palette(DSD(DS_0010813C + side * 4u), 0x1Cu,
                       DSD(DS_000BB9B0 + (u32)(cls * 4)));  /* 0x4430D..0x4431F 0x2A17C */
    fight_select_marker_spawn(side, (u32)cls, 0x1800u); /* 0x44324..0x4432D 0x1D7B8 */
    DSW(DSD(DS_0010813C + side * 4u) + 0x2Cu) =
        DSW(DS_000C8924 + (u32)(cls * 2));              /* 0x44332..0x44340 */
}

/* 0x44054 — record §48-S. EAX = side, EDX = slot: the pick tag
 * DS_00108114[side * 4 + slot], when set, is released through 0x2AD40 (EDX =
 * its pset) and the slot zeroed (ECX = 0). */
void fight_char_team_tag_drop(u32 side, u32 slot)
{
    u32 a = DS_00108114 + side * 16u + slot * 4u;       /* 0x44056..0x4405C */
    u32 rec = DSD(a);                                   /* 0x4405F */
    if (rec == 0u) return;                              /* 0x44065 */
    actor_release(rec, DSD(DS_001014EC) + (u32)DSW(rec + 0x56u) * 0x20u);  /* 0x44069..0x4407E 0x2AD40 */
    DSD(a) = 0u;                                        /* 0x44083 */
}

/* 0x4408C — record §48-S. EAX = side (ESI), EDX = slot (EDI). When the tag
 * slot DS_00108114[side * 4 + slot] is empty, a tag is spawned from a
 * descriptor built on the stack: sprite 0x3F34 with the palette 0x809994
 * for side 0, 0x3F33 with 0x80998C for side 1; the bytes +4/+5 and the word
 * +6 = 0, +8 = 0x2A00, +0xA = 0x80, +0xC = 0x1000. It goes at the side's
 * cursor's portrait (0xC8898[ch] + 0x180 for side 0, + 0xB00 for side 1;
 * 0xC88A6[ch] + 0x180), a3 0xFE, a5 0 (`push esi` with ESI = 0, or `push
 * 0`), and a non-zero record is stored in the slot.
 * PORT: the raw leaves the descriptor's word +0xE unwritten (stack garbage);
 * 0x2AE14 reads +0, +4, +5, +6, +8, +0xA, +0xC and +0x10 only, so the port's
 * zero there is not observable. */
void fight_char_team_tag_add(u32 side, u32 slot)
{
    u32 a = DS_00108114 + side * 16u + slot * 4u;       /* 0x44097..0x4409C */
    if (DSD(a) != 0u) return;                           /* 0x4409C/0x440A4 */
    u32 desc[5];
    desc[0] = side == 0u ? 0x3F34u : 0x3F33u;           /* 0x440AE/0x440BA, 0x440D4 */
    desc[1] = 0u;                                       /* 0x440DE..0x440EE */
    desc[2] = 0x2A00u | (0x80u << 16);                  /* 0x440D9, 0x440FC */
    desc[3] = 0x1000u;                                  /* 0x440F7 */
    desc[4] = side == 0u ? 0x809994u : 0x80998Cu;       /* 0x440B3/0x440BF, 0x440CE */
    s32 ch = (s8)DSB(DS_00108166 + side);               /* 0x4410B/0x44141 */
    u32 x = (u32)DSW(DS_000C8898 + (u32)(ch * 2))
          + (side == 0u ? 0x180u : 0xB00u);             /* 0x44120/0x44132, 0x44156/0x44168 */
    u32 y = (u32)DSW(DS_000C88A6 + (u32)(ch * 2)) + 0x180u;  /* 0x44118/0x4412C */
    u32 rec = actor_spawn(desc, x, 0xFEu, y, 0u);       /* 0x4416E 0x2AE14 */
    if (rec != 0u) DSD(a) = rec;                        /* 0x44173..0x4417A */
}

/* 0x4434C — record §48-S. EAX = side (ECX). The DS_00104B1D == 3 confirm:
 * a pick of up to four classes per side in DS_00108134[side * 4 + k] (0xFF
 * empty). The class is the signed byte 0xC8882[cursor] (`[eax+0xc887f] sar
 * 0x18`). When one of the four already holds it (signed compare), its tag
 * is dropped (0x44054) and the byte becomes 0xFF. Otherwise the first 0xFF
 * slot takes the class, the side byte becomes 2 and the tag is added
 * (0x4408C); then, when no slot is 0xFF any more, the side byte becomes 3,
 * the entry DS_00108154[side] is marked dead and zeroed and 0x432EC(side, 0)
 * runs. Both ways end in 0x43D60's tail: the fighter slot byte 0x107813 +
 * side * 0x94 = 0; unless DS_0010816E[side] is not 0xFF and equals
 * DS_0010816A[side] (which this pass does not write), 0x33C18 for a
 * non-0xFF DS_0010816E, 0x4248C(DS_0010816A[side], side) and DS_0010816E =
 * DS_0010816A; then the 0x6C voice. */
void fight_char_team_pick(u32 side)
{
    s32 ch = (s8)DSB(DS_00108166 + side);               /* 0x4435D/0x44363 */
    s32 cls = (s8)DSB(DS_000C8882 + (u32)ch);           /* 0x44366/0x4436C */
    u32 base = DS_00108134 + side * 4u;
    s32 found = -1;
    for (u32 k = 0; k < 4u; k++) {                      /* 0x44381..0x4439F */
        if ((s8)DSB(base + k) == cls) { found = (s32)k; break; }  /* 0x44383..0x4438E */
    }
    if (found != -1) {                                  /* 0x443A1..0x443AE */
        fight_char_team_tag_drop(side, (u32)found);     /* 0x443B0..0x443B4 0x44054 */
        DSB(base + (u32)found) = 0xFFu;                 /* 0x443B9 */
    } else {
        s32 k0 = -1;
        for (u32 k = 0; k < 4u; k++)                    /* 0x443C6..0x443E6 */
            if ((s8)DSB(base + k) == -1) { k0 = (s32)k; break; }
        if (k0 != -1) {                                 /* 0x443E8 */
            DSB(base + (u32)k0) = (u8)cls;              /* 0x443ED/0x443F2 */
            DSB(DS_00108170 + side) = 2u;               /* 0x443F0/0x443F9 */
            fight_char_team_tag_add(side, (u32)k0);     /* 0x443FF..0x44403 0x4408C */
        }
        s32 k1 = -1;
        for (u32 k = 0; k < 4u; k++)                    /* 0x44408..0x4442D */
            if ((s8)DSB(base + k) == -1) { k1 = (s32)k; break; }
        if (k1 == -1) {                                 /* 0x4442F */
            DSB(DS_00108170 + side) = 3u;               /* 0x44434/0x4443F */
            actor_set_dead(DSD(DS_00108154 + side * 4u));   /* 0x44436..0x44445 0x2B150 */
            DSD(DS_00108154 + side * 4u) = 0u;          /* 0x4444E (EBP = 0) */
            fight_char_text_clear(side, 0u);            /* 0x4444A..0x44455 0x432EC */
        }
    }
    DSB(DS_00107813 + side * 0x94u) = 0u;               /* 0x4445A..0x4446A */
    if ((s8)DSB(DS_0010816E + side) == -1
        || DSB(DS_0010816E + side) != DSB(DS_0010816A + side)) {  /* 0x44471..0x4448B */
        if ((s8)DSB(DS_0010816E + side) != -1)          /* 0x4447A */
            fight_char_reset(side);                     /* 0x4448D/0x4448F 0x33C18 */
        fight_stage_mark_drop(DSB(DS_0010816A + side), side);  /* 0x44494..0x444A0 0x4248C */
        DSB(DS_0010816E + side) = DSB(DS_0010816A + side);     /* 0x444A5/0x444AB */
    }
    /* PORT: 0x444B6 0x2C3FC(0x6C) voice, not wired (record §45-A). */
}

/* 0x44798 — record §48-S. The character select's per-frame pass with
 * DS_00104B1D == 3 (mode 0x10, sub-state 0). Its only caller is 0x438CD
 * (0x438B4); the dead copy's `jbe` at 0x44943 also names it. For side 0..1
 * on the side byte DS_00108170:
 * - 3: when the other side's byte is 3 too, DS_0010816A = DS_00108134 (side
 *   0's first pick), DS_00105B34 = 0, DS_0010816B = DS_00108138 (side 1's),
 *   DS_00105B35 = (the two are equal), the hook 0x430E8 and 0x4F980(0x11),
 *   and the pass returns; otherwise the next side;
 * - 1: 0x44638, then the stick and confirm;
 * - anything else (0, 2, above 3): the stick and confirm.
 * The stick is 0x43B24's, followed by 0x4418C and 0x442A0; bit 0 confirms
 * through 0x4434C. After the loop a set DS_00105C04 resets the countdown
 * DS_0010816C to 0xF and is cleared; there is no countdown step. */
void fight_char_team_pass(void)
{
    for (u32 side = 0; side < 2u; side++) {             /* 0x4479B..0x44914 */
        u8 b = DSB(DS_00108170 + side);                 /* 0x4479F */
        if (b == 3u) {                                  /* 0x447AF/0x447C5..0x447CD */
            if (DSB(DS_00108170 + (side ^ 1u)) != 3u) continue;  /* 0x447D3..0x447E5 */
            DSB(DS_0010816A) = DSB(DS_00108134);        /* 0x447EB/0x447F0 */
            DSB(DS_00105B34) = 0u;                      /* 0x447F7 */
            DSB(DS_0010816B) = DSB(DS_00108138);        /* 0x447FD/0x44802 */
            DSB(DS_00105B35) = DSB(DS_0010816A) == DSB(DS_0010816B) ? 1u : 0u;  /* 0x44807..0x4481A */
            DSD(DS_00104AE4) = FN_000430E8;             /* 0x44815/0x44824 */
            frontend_wipe_arm(0x11u);                   /* 0x4482A 0x4F980 */
            return;                                     /* 0x4482F */
        }
        if (b == 1u)                                    /* 0x447B8/0x447BA */
            fight_char_team_text_blink(side);           /* 0x447BE 0x44638 */
        u32 stick = (u32)DSW(DS_001088E0 + side * 2u) & 0xF0u;  /* 0x44833..0x44840 */
        if (stick != 0u) {                              /* 0x44843/0x44845 */
            if (stick == 0x10u) {                       /* 0x4486E/0x448BC */
                if ((s8)DSB(DS_00108166 + side) < 6)    /* 0x448C5 */
                    DSB(DS_00108166 + side)++;          /* 0x448CA */
            } else if (stick == 0x20u) {                /* 0x44851/0x448D2 */
                s8 c = (s8)DSB(DS_00108166 + side);
                if (c > 0) DSB(DS_00108166 + side) = (u8)(c - 1);  /* 0x448D8..0x448E0 */
            } else if (stick == 0x40u) {                /* 0x44861/0x44890 */
                if ((s8)DSB(DS_00108166 + side) < 4)    /* 0x44899 */
                    DSB(DS_00108166 + side) = (u8)(DSB(DS_00108166 + side) + 4u);  /* 0x4489E */
                if ((s8)DSB(DS_00108166 + side) >= 7)   /* 0x448A5..0x448B1 */
                    DSB(DS_00108166 + side) = 6u;       /* 0x448B3 */
            } else if (stick == 0x80u) {                /* 0x44867/0x44879 */
                if ((s8)DSB(DS_00108166 + side) >= 4)   /* 0x44882 */
                    DSB(DS_00108166 + side) = (u8)(DSB(DS_00108166 + side) - 4u);  /* 0x44887 */
            }
            fight_char_team_portrait(side);             /* 0x448E8 0x4418C */
            fight_char_team_fighter(side);              /* 0x448EF 0x442A0 */
        }
        if ((DSW(DS_001088E0 + side * 2u) & 1u) != 0u)  /* 0x448F4..0x44904 */
            fight_char_team_pick(side);                 /* 0x44908 0x4434C */
    }
    if (DSB(DS_00105C04) != 0u) {                       /* 0x4491A */
        DSW(DS_0010816C) = 0xFu;                        /* 0x4492A */
        DSB(DS_00105C04) = 0u;                          /* 0x44931 */
    }
}

/* ---- the mode-0x1A hooks 0x430E8/0x4367C, 0x430C0 and 0x4454C (§46-B) -- */

#define DS_000C8364 0x000C8364u   /* no symbols.h name: descriptor, id 0x351 */
#define DS_000C8378 0x000C8378u   /* no symbols.h name: descriptor, id 0x352 */
#define DS_000C84E0 0x000C84E0u   /* no symbols.h name: 7 descriptor pointers */
#define DS_000C84FC 0x000C84FCu   /* no symbols.h name: 7 descriptor pointers */
#define DS_000C87BC 0x000C87BCu   /* no symbols.h name: descriptor, id 0x3F11 */
#define DS_00080C04 0x00080C04u   /* no symbols.h name: the string "VS" */
#define DS_0010816D 0x0010816Du   /* no symbols.h name: 0x10816E - 1, the loops' base */
#define DS_00108165 0x00108165u   /* no symbols.h name: 0x108166 - 1, the loops' base */
#define DS_00105B33 0x00105B33u   /* no symbols.h name: 0x105B34 - 1, the loops' base */
#define FN_000430C0 0x000430C0u   /* no symbols.h name: the hook 0x430E8 installs */

/* 0x430C0 — record §46-B. A DS_00104AE4 hook, stored by 0x430E8 (0x43284).
 * 0x29D60 (a bare `ret`), then 0x2F198 draws "VS" (0x80C04) at col 0x13, row
 * 2 with mode 0x4002. EBX/ECX/EDX are pushed and popped. */
void fight_hook_430c0(void)
{
    text_cursor_set(0x13, 2, mem + DS_00080C04, 0x4002u);   /* 0x430DC 0x2F198 */
}

/* 0x430E8 — record §46-B. A DS_00104AE4 hook, stored at 0x1F447 (0x1EEB0),
 * 0x43AE8 (0x43AAC), 0x43C18 (0x43B24) and 0x44824 (0x44798), each just
 * before 0x4F980 arms mode 0x1A. The versus screen:
 * - the 0x31 voice, 0x4F200(0), 0x25848; with DS_00108173 != 0 the bytes
 *   DS_00107813 and DS_001078A7 become 1;
 * - with DS_00104B1D == 1, DS_00104B1F = DS_00104AB8 (0x43125/0x4312A); then
 *   unless DS_00104B1F == 3, 0x41350 with EAX = (DS_00104B1F - 1) ^ 1 (0x4313D
 *   `dec eax` and 0x43145 `xor al,bl` with BL = 1, or 0x4315F `xor al,1`) and
 *   EDX = the stage word;
 * - six 0x2AE14 spawns: 0xC8364 (a2 0, a3 0xF0) into DS_001080B4, 0xC8378
 *   (a2 0x2A00, a3 0xF1) into DS_001080B8; then, with pa/pb the two records'
 *   +0x56 words, 0xC84FC[c0] and 0xC84FC[c1] (a2 0xF, a3 0xF4, a4 7, a5 pa |
 *   0x400 / pb | 0x400) and 0xC84E0[c0] (a2 0x9B, a3 0xE0, a4 0x36, a5 pa |
 *   0x4400) and 0xC84E0[c1] (a2 0xD, a5 pb | 0x400), where c0/c1 are the
 *   signed bytes DS_0010816A/DS_0010816B (`mov eax,[0x108167]` or
 *   `[0x108168]`; `sar eax,0x18`);
 * - 0x4F1D0, 0x38B18(0xC87BC) with EDX = EBX = 0 (0x43254/0x4325A, kept by
 *   0x4F1D0, which pushes EDX and names no other), the last spawn's +0x2E
 *   word += 4 and +0x4E = 1, the hook becomes 0x430C0, then the 0x2D and 0x2F
 *   voices. */
void fight_hook_430e8(void)
{
    /* PORT: 0x430F2 0x2C3FC(0x31) voice, not wired (record §45-A). */
    flow_screen_reset(0u);                              /* 0x430F9 0x4F200 */
    flow_stage_pick();                                  /* 0x430FE 0x25848 */
    if (DSB(DS_00108173) != 0u) {                       /* 0x43103 */
        DSB(DS_00107813) = 1u;                          /* 0x4310E */
        DSB(DS_001078A7) = 1u;                          /* 0x43114 */
    }
    if (DSB(DS_00104B1D) == 1u)                         /* 0x4311A/0x43120 */
        DSB(DS_00104B1F) = DSB(DS_00104AB8);            /* 0x43125/0x4312A */
    u32 b1f = DSB(DS_00104B1F);                         /* 0x43131/0x4314B */
    if (b1f != 3u)                                      /* 0x43136/0x43150 */
        fight_char_select((b1f - 1u) ^ 1u, DSW(DS_00104AFC));   /* 0x43161 0x41350 */
    u32 a = actor_spawn((const u32 *)(mem + DS_000C8364), 0u, 0xF0u, 0u, 0u);  /* 0x43176 */
    DSD(DS_001080B4) = a;                               /* 0x43187 */
    u32 b = actor_spawn((const u32 *)(mem + DS_000C8378), 0x2A00u, 0xF1u, 0u, 0u);  /* 0x43193 */
    DSD(DS_001080B8) = b;                               /* 0x4319E */
    u32 pa = DSW(DSD(DS_001080B4) + 0x56u);             /* 0x43198/0x431A3 */
    u32 pb = DSW(b + 0x56u);                            /* 0x431AF */
    u32 c0 = (u32)(s32)(s8)DSB(DS_0010816A);            /* 0x431BE/0x431C8 */
    u32 c1 = (u32)(s32)(s8)DSB(DS_0010816B);            /* 0x431EB/0x431F5 */
    actor_spawn((const u32 *)(mem + DSD(DS_000C84FC + c0 * 4u)),
                0xFu, 0xF4u, 7u, pa | 0x400u);          /* 0x431DC */
    actor_spawn((const u32 *)(mem + DSD(DS_000C84FC + c1 * 4u)),
                0xFu, 0xF4u, 7u, pb | 0x400u);          /* 0x43205 */
    actor_spawn((const u32 *)(mem + DSD(DS_000C84E0 + c0 * 4u)),
                0x9Bu, 0xE0u, 0x36u, pa | 0x4400u);     /* 0x43229 */
    u32 r = actor_spawn((const u32 *)(mem + DSD(DS_000C84E0 + c1 * 4u)),
                        0xDu, 0xE0u, 0x36u, pb | 0x400u);   /* 0x4324D/0x43252 */
    frontend_origin_zero();                             /* 0x4325C 0x4F1D0 */
    frontend_spawn_row((const u32 *)(mem + DS_000C87BC), 0u, 0u);  /* 0x43266 0x38B18 */
    DSW(r + 0x2Eu) = (u16)(DSW(r + 0x2Eu) + 4u);        /* 0x4326B..0x43277 */
    DSB(r + 0x4Eu) = 1u;                                /* 0x43280 */
    DSD(DS_00104AE4) = FN_000430C0;                     /* 0x43284 */
    /* PORT: 0x4328A 0x2C3FC(0x2D) and 0x43294 0x2C3FC(0x2F) voices, not wired
     * (record §45-A). */
}

/* 0x4454C — record §46-B. 0x4367C's DS_00104B1D == 3 arm (only caller
 * 0x43688). The 0x100 voice; then with DS_00108173 != 0 the per-side bytes as
 * in 0x4367C's first arm (0x10816E/F = 0xFF, 0x108166/7 = 0x108168/9,
 * 0x105B34/5 = 0) and the 0x108169/0x108168 step (each wraps to 0 at 7, the
 * second only on the first's wrap). Otherwise, per side s: 0x10816E[s] = 0xFF,
 * 0x108166[s] = 0xC8880[s], 0x105B34[s] = 0, the four bytes 0x108134[s*4..] =
 * 0xFF and the four dwords 0x108114[s*16..] = 0 (0x445F5..0x4460B, EAX
 * pre-incremented by 4 before the 0x44603 store), and 0x108164[s] = 0 (ESI
 * incremented before the 0x44616 store). Last, the 0x2E voice and 0x444C8. */
static void fight_4454c(void)
{
    /* PORT: 0x44557 0x2C3FC(0x100) voice, not wired (record §45-A). */
    if (DSB(DS_00108173) != 0u) {                       /* 0x4455C */
        for (u32 i = 1; i <= 2u; i++) {                 /* 0x44565..0x44586 */
            DSB(DS_0010816D + i) = 0xFFu;               /* 0x44568 */
            DSB(DS_00108165 + i) = DSB(DS_00108167 + i);    /* 0x4456F/0x44575 */
            DSB(DS_00105B33 + i) = 0u;                  /* 0x4457D */
        }
        u8 c = (u8)(DSB(DS_00108169) + 1u);             /* 0x44588/0x44590 */
        DSB(DS_00108169) = c;                           /* 0x44594 */
        if (c >= 7u) {                                  /* 0x4459A */
            u8 d = (u8)(DSB(DS_00108168) + 1u);         /* 0x445A3/0x445AB */
            DSB(DS_00108169) = 0u;                      /* 0x445AD */
            DSB(DS_00108168) = d;                       /* 0x445B5 */
            if (d >= 7u) DSB(DS_00108168) = 0u;         /* 0x445BB/0x445C0 */
        }
    } else {
        for (u32 s = 0; s < 2u; s++) {                  /* 0x445D1..0x4461F */
            DSB(DS_0010816E + s) = 0xFFu;               /* 0x445D1 */
            DSB(DS_00108166 + s) = DSB(DS_000C8880 + s);    /* 0x445D8/0x445DE */
            DSB(DS_00105B34 + s) = 0u;                  /* 0x445E8 */
            for (u32 k = 0; k < 4u; k++) {              /* 0x445F5..0x4460B */
                DSB(DS_00108134 + s * 4u + k) = 0xFFu;  /* 0x445FA */
                DSD(DS_00108114 + s * 16u + k * 4u) = 0u;   /* 0x44603 */
            }
            DSB(DS_00108164 + s) = 0u;                  /* 0x44616 */
        }
    }
    /* PORT: 0x44626 0x2C3FC(0x2E) voice, not wired (record §45-A). */
    fight_char_screen_open_both();                      /* 0x4462B 0x444C8 */
}

/* 0x4367C — record §46-B. A DS_00104AE4 hook, stored at 0x25810 (0x257A4,
 * the coin divert, before 0x4F980 at 0x25816). With DS_00104B1D == 3 it is
 * 0x4454C alone. Otherwise the 0x100 voice; with DS_00108173 != 0 the
 * per-side bytes (for EAX = 1, 2: 0x10816D[EAX] = 0xFF, 0x108165[EAX] =
 * 0x108167[EAX], 0x105B33[EAX] = 0) and the 0x108169/0x108168 step; else
 * the same loop with 0x108165[EAX] = 0xC887F[EAX]. Last, the 0x2E voice and
 * 0x43738 (called directly at 0x4372E). */
void fight_hook_4367c(void)
{
    if (DSB(DS_00104B1D) == 3u) {                       /* 0x4367F */
        fight_4454c();                                  /* 0x43688 0x4454C */
        return;
    }
    /* PORT: 0x43696 0x2C3FC(0x100) voice, not wired (record §45-A). */
    if (DSB(DS_00108173) != 0u) {                       /* 0x4369B */
        for (u32 i = 1; i <= 2u; i++) {                 /* 0x436A6..0x436C6 */
            u8 v = DSB(DS_00108167 + i);                /* 0x436A9 */
            DSB(DS_0010816D + i) = 0xFFu;               /* 0x436B1 */
            DSB(DS_00108165 + i) = v;                   /* 0x436B7 */
            DSB(DS_00105B33 + i) = 0u;                  /* 0x436BD */
        }
        u8 c = (u8)(DSB(DS_00108169) + 1u);             /* 0x436C8/0x436D0 */
        DSB(DS_00108169) = c;                           /* 0x436D4 */
        if (c >= 7u) {                                  /* 0x436DA */
            DSB(DS_00108169) = 0u;                      /* 0x436E1 */
            u8 d = (u8)(DSB(DS_00108168) + 1u);         /* 0x436E6 */
            DSB(DS_00108168) = d;
            if (d >= 7u) DSB(DS_00108168) = 0u;         /* 0x436F3/0x436F8 */
        }
    } else {
        for (u32 i = 1; i <= 2u; i++) {                 /* 0x43702..0x43722 */
            DSB(DS_0010816D + i) = 0xFFu;               /* 0x43707 */
            DSB(DS_00108165 + i) = DSB(DS_000C887F + i);    /* 0x4370D/0x43719 */
            DSB(DS_00105B33 + i) = 0u;                  /* 0x43713 */
        }
    }
    /* PORT: 0x43729 0x2C3FC(0x2E) voice, not wired (record §45-A). */
    fight_char_screen_open();                           /* 0x4372E 0x43738 */
}

/* ---- the DS_00104AE4 hook 0x4142C and its callees (record §46-F) -------- */

#define DS_000C86F0 0x000C86F0u   /* no symbols.h name: descriptor, id 0x2BEF */
#define DS_000C86DC 0x000C86DCu   /* no symbols.h name: descriptor, id 0x3F12 */
#define DS_000C85BC 0x000C85BCu   /* no symbols.h name: 7 descriptor pointers */
#define DS_00108105 0x00108105u   /* no symbols.h name: the byte after DS_00108104 */

/* 0x413C8 — record §46-F. Only caller 0x41456 (0x4142C). 0x2AE14 spawns
 * 0xC86DC (a2 0x2A00, a3 0xE0, a4 0x1B00, a5 0) into DS_001080F4; then for
 * i = 0..6 (ESI += 4 until 0x1C) 0xC85BC[i] with a2 = a4 = 0 (EDI), a3 0xE2
 * and a5 = the +0x56 word of DS_001080F4's record | 0x400 (reloaded each
 * pass, `or ah,4; and eax,0xffff`), stored at DS_001080C0[i] (`mov
 * [esi+0x1080bc],eax` after the add). */
static void fight_413c8(void)
{
    DSD(DS_001080F4) = actor_spawn((const u32 *)(mem + DS_000C86DC),
                                   0x2A00u, 0xE0u, 0x1B00u, 0u);  /* 0x413E3/0x413EA */
    for (u32 i = 0; i < 7u; i++) {                      /* 0x413F1..0x41423 */
        u32 p = (DSW(DSD(DS_001080F4) + 0x56u) | 0x400u) & 0xFFFFu;  /* 0x413F1..0x41402 */
        DSD(DS_001080C0 + i * 4u) =
            actor_spawn((const u32 *)(mem + DSD(DS_000C85BC + i * 4u)),
                        0u, 0xE2u, 0u, p);              /* 0x41415/0x4141A */
    }
}

/* 0x4246C — record §46-F. 0x65490 (a byte fill: EAX = dst, DL = the byte,
 * ECX = the count) zeroes the seven stage bytes DS_00108106..0x10810C, then
 * the byte DS_00108111 = 0 (AH). Callers: 0x25B47 (0x25AE8) and the unported
 * 0x28B37 (0x28788). */
void fight_stage_marks_clear(void)
{
    for (u32 i = 0; i < 7u; i++)                        /* 0x4247A 0x65490 */
        DSB(DS_00108106 + i) = 0u;
    DSB(DS_00108111) = 0u;                              /* 0x42481 */
}

/* 0x4142C — record §46-F. A DS_00104AE4 hook, stored only at 0x28BC6
 * (0x28788) with mode 0x17. DS_00104AE8 = 0, 0x4F1E4, 0x2BAF4 with EAX = 1,
 * 0x38B18(0xC86F0) with EDX = EBX = 0 (EDX zeroed at 0x4142F and kept by the
 * two calls, which push it; EBX at 0x41445), 0x413C8; then the bytes
 * DS_0010810F, DS_00108111, DS_00108104 and DS_00108105 = 0 (AH), the 0x32
 * voice, DS_00108112 = DL = 0 (0x2C3FC pushes EDX), DS_00104B25 = CL = 8, the
 * mode word DS_00104B00 = 0x12, the countdown word DS_00104AFE = 0x1E and
 * DS_00104B23 = CH = 0. */
void fight_hook_4142c(void)
{
    DSD(DS_00104AE8) = 0u;                              /* 0x41435 */
    frontend_input_reset();                             /* 0x4143B 0x4F1E4 */
    actors_reset();                                     /* 0x41447 0x2BAF4 (eax = 1) */
    frontend_spawn_row((const u32 *)(mem + DS_000C86F0), 0u, 0u);  /* 0x41451 0x38B18 */
    fight_413c8();                                      /* 0x41456 0x413C8 */
    DSB(DS_0010810F) = 0u;                              /* 0x41464 */
    DSB(DS_00108111) = 0u;                              /* 0x4146A */
    DSB(DS_00108104) = 0u;                              /* 0x41470 */
    DSB(DS_00108105) = 0u;                              /* 0x41476 */
    /* PORT: 0x41483 0x2C3FC(0x32) voice, not wired (record §45-A). */
    DSB(DS_00108112) = 0u;                              /* 0x41488 */
    DSB(DS_00104B25) = 8u;                              /* 0x4148E */
    DSW(DS_00104B00) = 0x12u;                           /* 0x41494 */
    DSW(DS_00104AFE) = 0x1Eu;                           /* 0x414A2 */
    DSB(DS_00104B23) = 0u;                              /* 0x414A9 */
}

/* ---- 0x494A8 the dust builder (state 6's fighter spawn) ----------------- */

/* 0x49388. The dust descriptor picker. The draw's range is the raw's
 * 0x4938b..0x493ab: `MOV DX,[0x104B00]; CMP EDX,3; JNZ 0x4939E;
 * MOV EAX,0x64; JMP 0x493AB; 0x4939E MOV AX,[EAX*2+0x108860]; AND EAX,0xffff;
 * 0x493AB CALL 0x5D7DC` — the caller's EAX (the side, set at 0x495AE) indexes
 * the table word DS_00108860[side] (0x33C50 seeds it 100), except in mode 3
 * (the demo), where the range is 0x64. One draw; the value maps through the
 * raw's thresholds to a 0xC9524 index. */
static u32 fight_dust_pick(u32 side)
{
    u32 range = (DSW(DS_00104B00) == 3u)
              ? 0x64u
              : (u32)DSW(DS_00108860 + side * 2u);
    u32 v = rng_next(range);                    /* 0x493AB */
    if (v < 0x1eu) return 4;
    if (v < 0x32u) return 3;
    if (v < 0x46u) return 5;
    if (v < 0x55u) return 1;
    if (v < 0x5fu) return 0;
    return 2;
}

/* 0x29CDC. The value written into the picked descriptor's +0x10: the per-side
 * table DS_000A8AF8 (when DS_00105B34[side] == 0) or DS_000A8B14, indexed by
 * the slot's character byte. */
static u32 fight_dust_value(u32 side, u32 ch)
{
    if (DSB(DS_00105B34 + side) == 0u) return DSD(DS_000A8AF8 + ch * 4u);
    return DSD(DS_000A8B14 + ch * 4u);
}

/* 0x496AC. The clamp the dust actor's +0x2C receives (and the effects pass's
 * cases 3 and 5, 0x49DBC/0x49EC9). The compares are signed (0x496AD `cmp
 * eax,0xb00` / `jl`, 0x496BB `cmp eax,0x400` / `jg`). The argument is the y:
 * 0x49629/0x49642 read `[ESP+0x4]` after 0x2AE14's `RET 0x4` (0x2B14A) has
 * popped the 0x49603 `PUSH 0x0`, so ESP is back at the frame base and
 * `[ESP+0x4]` is the value 0x49605 wrote (`ESP=S-4`, `[ESP+8]` = EBX = the y of
 * 0x49601). The step lives at `[ESP]` (0x495F9) and is not read here. The
 * original's entries confirm it: `entry+0x1a` = 0x0a49/0x0848/0x0a80/0x082d
 * and `actor+0x2c` = 0xc5b/0xd5c/0xc40/0xd69 = the raw's `(0xb00-y)>>1 + 0xc00`. */
static u16 fight_dust_clamp(s32 v)
{
    if (v >= 0xb00) return 0xc00u;
    if (v <= 0x400) return 0xf80u;
    return (u16)(((0xb00 - v) >> 1) + 0xc00);
}

/* 0x494A8. The dust/effect entry builder. 0x33C78 calls it at 0x33E43 when
 * DS_00104B14 == 0; each iteration moves one node from the free fight-effect
 * list (DS_001083C4, built by 0x49300) to the active one (DS_0010884C), picks a
 * descriptor (0xC9524[0x49388]), spawns the dust actor (0x2AE14) and fills the
 * entry's fields. The loop bound is slot+0x81 (0x4967F) and each iteration
 * draws THREE values — 0x49388's, rng(0x1800) (0x495DF) and rng(step)
 * (0x495FC). State 6's stream needs exactly those: the demo's slot+0x81 is 2,
 * so six draws land between the picks at 0x11AAD and 0x11AE9. The entry's
 * +0x1E type is 0, whose 0x49C78 handler (0x4AAD0) is the unported dust
 * behaviour (§7.4); the actor it spawns is an ordinary pool actor and renders. */
void fight_dust_build(u32 side)
{
    /* PORT: 0x494A8's DS_00104AFA == 0x23 arm calls 0x4CF20, a six-entry
     * variant of this builder with its own draws. Not reached in the demo (the
     * reference's state-6 draws are this arm's 3 x slot+0x81) and a named gap. */
    if (DSB(DS_00104AFA) == 0x23u) return;

    DSB(DS_001088AE + side) = 0;                /* 0x494F2 */
    DSB(DS_001088C4) = 0;                       /* 0x494FE */
    DSB(DS_001088A2 + side) = 0;                /* 0x49504 */
    DSB(DS_001088A4 + side) = 0;                /* 0x4950A */
    DSB(DS_001088B2 + side) = 0;                /* 0x49516 */
    DSB(DS_001088BF) = 0;                       /* 0x49524 */
    DSB(DS_001088A8 + side) = 0xffu;            /* 0x4952A */
    DSB(DS_0010889E + side) = 0;                /* 0x49532 */
    DSB(DS_001088B6 + side) = 0;                /* 0x49538 */
    DSW(DS_00108892) = 0;                       /* 0x49546 */

    u32 slot = DS_001077B0 + side * 0x94u;
    u32 n = (u32)DSB(slot + 0x81u);             /* 0x49540 */
    u32 step = (n != 0u) ? (0x300u / n) : 0x300u;   /* 0x49555/0x49563 */
    u32 offset = 0;                             /* 0x49578 */
    for (u32 i = 0; i < n; i++) {               /* 0x4967F */
        u32 entry = DSD(DS_001083C4);           /* 0x49581 */
        if (entry == DS_001083C4) return;       /* 0x4959C: the pool is empty */
        effects_list_unlink(entry);             /* 0x49595 0x249D0 */
        effects_list_insert_after(DS_0010884C, entry);   /* 0x495A9 0x249B0 */
        u32 desc = DSD(DS_000C9524 + fight_dust_pick(side) * 4u);  /* 0x495B9 */
        DSD(desc + 0x10u) = fight_dust_value(side, (u32)DSB(slot + 0x7Au)); /* 0x495CC */
        u32 rec = DSD(slot);                    /* 0x495CF/0x495E6 */
        u32 x = (u32)((s32)DSD(rec + 0x18u) - 0xc00 + (s32)rng_next(0x1800u)); /* 0x495E4 */
        u32 y = offset + (u32)((s32)DSD(rec + 0x30u) >> 16) + 0x400u
                + rng_next(step);               /* 0x49601 */
        u32 actor = actor_spawn((const u32 *)(mem + desc), x, y, 0u, 0u);  /* 0x4960F */
        DSD(entry + 8u) = actor;                /* 0x49614 */
        DSD(actor + 0x14u) = entry;             /* 0x49617 */
        DSB(entry + 0x1Eu) = 0;                 /* 0x4961A */
        DSB(entry + 0x1Fu) = 0;                 /* 0x49622 */
        /* 0x49626. The raw reads `[ESP+0x8]` at 0x4961E, after the
         * 0x4960F 0x2AE14 call. 0x2AE14 ends `RET 0x4`, so it pops the
         * 0x49603 `PUSH 0x0` and ESP returns to the frame base; `[ESP+0x8]`
         * is then the side stored at 0x494B1, NOT the y stored at 0x49605
         * (which is one slot lower). The original's entries carry +0x21 =
         * 1,1,0,0 (the side), and 0x4AAD0 indexes the per-side
         * DS_001088B2/DS_0010889E tables with it. */
        DSB(entry + 0x21u) = (u8)side;          /* 0x49626 */
        DSD(entry + 0x0Cu) = slot;              /* 0x4962D */
        DSW(actor + 0x2Cu) = fight_dust_clamp((s32)y); /* 0x49638 */
        DSW(entry + 0x1Cu) = 0;                 /* 0x4963C */
        DSD(entry + 0x10u) = 0;                 /* 0x49646 */
        DSW(entry + 0x1Au) = (u16)y;            /* 0x4964D */
        if (DSB(rec + 0x51u) != 0u) {           /* 0x49651 */
            DSW(actor + 0x2Eu) += 4;            /* 0x4965C */
            DSB(actor + 0x4Eu) = 1;             /* 0x49664 */
        }
        offset += step;                         /* 0x49674 */
    }
    DSB(DS_001088C3) = 0;                       /* 0x49693 */
    DSB(DS_001088C1) = 0;                       /* 0x4969B */
}

/* ---- 0x33C18 the character select's slot reset -------------------------- */

/* 0x33C18 — record §47-C. Clear the per-side character fields and reset the
 * 0x108860 word to 100 (EBX). The slot is 0x1077B0 + side * 0x94 (`shl 3; add;
 * shl 2; add; shl 2`); EDX = EAX indexes the word. Only EAX is read (EBX/EDX
 * are pushed and popped). Ported callers: 0x41354 (0x41350) and 0x257E6/
 * 0x257F2 (0x257A4). */
void fight_char_reset(u32 side)
{
    u32 slot = DS_001077B0 + side * 0x94u;
    DSB(slot + 0x7Fu) = 0;                      /* 0x33C2E */
    DSB(slot + 0x80u) = 0;                      /* 0x33C32 */
    DSD(slot + 0x3Cu) = 0;                      /* 0x33C39 */
    DSB(slot + 0x82u) = 0;                      /* 0x33C40 */
    DSB(slot + 0x5Bu) = 0;                      /* 0x33C4C */
    DSW(DS_00108860 + side * 2u) = 100u;        /* 0x33C50 */
}

/* ---- 0x41350 the per-side character select ------------------------------ */

void fight_char_select(u32 side, u32 char_index)
{
    fight_char_reset(side);                                 /* 0x41354 */
    if (DSB(DS_00104B1D) != 1u)                             /* 0x41359 */
        DSB(DS_0010816A + side) =
            DSB(DS_000C835A + (char_index & 0xFFFFu));      /* 0x41367 */
    DSB(DS_001077B0 + side * 0x94u + 0x63u) = 1u;           /* 0x41385 */
    DSB(DS_0010816E + side) = 0xFFu;                        /* 0x41390 */
    DSB(DS_00105B34 + side) = 0u;                           /* 0x41398 */
    if (DSB(DS_0010816A + side) == DSB(DS_0010816A + (side ^ 1u))
            && DSB(DS_00105B34 + (side ^ 1u)) == 0u)
        DSB(DS_00105B34 + side) = 1u;                       /* 0x413B5 */
    DSB(FIGHT_SLOT_INDEX) = (u8)(side ^ 1u);                /* 0x413BF */
}

/* ---- 0x1D890 the HUD spawn ---------------------------------------------- */

#define DS_000A7674 0x000A7674u   /* no symbols.h name: 0x1DC6C's side-1 bar descriptor - 4 */
#define DS_000A767C 0x000A767Cu   /* no symbols.h name: [side] bar descriptor */
#define DS_000A7684 0x000A7684u   /* no symbols.h name: [side] second bar descriptor */
#define DS_000A768C 0x000A768Cu   /* no symbols.h name: [side] bar x word */
#define DS_000A7690 0x000A7690u   /* no symbols.h name: [side] bar y word */
#define DS_000A7694 0x000A7694u   /* no symbols.h name: [side] second bar x word */
#define DS_000A7698 0x000A7698u   /* no symbols.h name: [side] second bar y word */
#define DS_000A7628 0x000A7628u   /* no symbols.h name: [side] 0xBB664 x word */
#define DS_000A762C 0x000A762Cu   /* no symbols.h name: [side] 0xBB664 y word */
#define DS_000A7630 0x000A7630u   /* no symbols.h name: [side] 0xBB678 x word */
#define DS_000A7634 0x000A7634u   /* no symbols.h name: [side] 0xBB678 y word */
#define DS_000A76D0 0x000A76D0u   /* no symbols.h name: the badge y word (0x980) */
#define DS_000BB664 0x000BB664u   /* no symbols.h name: the HUD descriptor (stream 0xE904C) */
#define DS_000BB678 0x000BB678u   /* no symbols.h name: the HUD descriptor (stream 0xE906A) */
#define DS_000C9960 0x000C9960u   /* no symbols.h name: 0x79 bar sprite words (0x2DF9..) */
#define DS_000C9A52 0x000C9A52u   /* no symbols.h name: 0x79 bar sprite words (0x46F9..) */
#define DS_000C9B44 0x000C9B44u   /* no symbols.h name: 0x45 bar sprite words (0x2E71..) */

/* 0x1D2F0 — record §48-U. EAX = the value v (ECX), EDX = the side (EBX). v is
 * clamped to 0..0x78 (0x1D2F9 `jle`, 0x1D305 `jge`; below 0 `xor ecx,eax`
 * gives 0). One 0x10D70 on the pushed record DS_001028F0[side] with AX = a
 * word of 0xC9960[v] or 0xC9A52[v]: with DS_00104B1D == 2 side 0 takes
 * 0xC9960 and side 1 0xC9A52; otherwise both take 0xC9A52 when the slot's
 * +0x41 bit 3 is set (0x1D3AA/0x1D400), else 0xC9960. The EBX/ECX/EDX loads
 * before each call (1 / 0x5000 or 0x6000 / 4 or 0x17) are dead: 0x10D70
 * reads only AX and its stack record. Callers: 0x1D797 (0x1D764), 0x1D941
 * (0x1D890), 0x1DD2C (0x1DC6C), and the unported 0x1D5A8 (0x1D540). */
void fight_hud_bar_set(s32 v, u32 side)
{
    u32 c = v > 0x78 ? 0x78u : (v < 0 ? 0u : (u32)v);  /* 0x1D2F9..0x1D309 */
    u32 flag8 = DSB(DS_001077B0 + side * 0x94u + 0x41u) & 8u;   /* 0x1D30B..0x1D319 */
    u32 table;
    if (DSB(DS_00104B1D) == 2u)                         /* 0x1D33D `jnz` */
        table = side == 0u ? DS_000C9960 : DS_000C9A52; /* 0x1D35D / 0x1D385 */
    else
        table = flag8 != 0u ? DS_000C9A52 : DS_000C9960;    /* 0x1D3AA..0x1D442 */
    actor_pset_word_set(DSD(DS_001028F0 + side * 4u),
                        DSW(table + c * 2u));           /* 0x10D70 */
}

/* 0x1D464 — record §48-U. EAX = the value v, EDX = the side. v is clamped to
 * 0..0x44 (0x1D466 `jle`, 0x1D472 `jge`, below 0 `xor eax,eax`), then 0x10D70
 * on the pushed record DS_001028E8[side] with AX = the word 0xC9B44[v]. The
 * EBX/ECX/EDX loads (3, 0x7000, 9 or 0x17) are dead as in 0x1D2F0. Callers:
 * 0x1D94A (0x1D890), 0x1DD35 (0x1DC6C) and the unported 0x1D5E1 (0x1D540). */
void fight_hud_bar2_set(s32 v, u32 side)
{
    u32 c = v > 0x44 ? 0x44u : (v < 0 ? 0u : (u32)v);  /* 0x1D466..0x1D476 */
    actor_pset_word_set(DSD(DS_001028E8 + side * 4u),
                        DSW(DS_000C9B44 + c * 2u));     /* 0x1D48B/0x1D4AA, 0x1D4C6 */
}

/* 0x1D890 — record §48-U (the EAX != 0 arm). Per side (ESI; EBP = side * 4,
 * EDI = side * 2): the four HUD bytes are zeroed; with the byte AL non-zero,
 * a = side ? 0x4000 : 0 is every spawn's a5 and
 * - 0xA767C[side] at (0xA768C[side], a3 0xFF, 0xA7690[side]) into
 *   DS_001028F0[side], 0xA7684[side] at (0xA7694[side], 0xFF, 0xA7698[side])
 *   into DS_001028E8[side];
 * - 0x1D2F0(0, side) and 0x1D464(0, side);
 * - 0xBB664 at (0xA7628[side], 0xFF, 0xA762C[side]) into DS_001028F8[side],
 *   0xBB678 at (0xA7630[side], 0xFF, 0xA7634[side]) into DS_00102900[side];
 * - DS_001028E0[side] = 0 (EBX, no 0x2B150 on the old record), then 0x1D838
 *   (side, the slot's +0x7A, the word 0xA76D0) and DS_00104AEC |= 2.
 * The words are zero-extended. Callers: 0x11B1E (state 6, EAX = 0), 0x25C46
 * (0x25C1C, EAX = 1) and 0x25AC2 (0x25A84, no entrance). */
void fight_hud_spawn(u32 enable)
{
    for (u32 side = 0; side < 2u; side++) {                 /* 0x1D8A6 loop */
        DSB(DS_0010780E + side * 0x94u) = 0;                /* 0x1D8AB */
        DSB(DS_0010290C + side) = 0;                        /* 0x1D8B1 */
        DSB(DS_0010780A + side * 0x94u) = 0;                /* 0x1D8B7 */
        DSB(DS_0010290E + side) = 0;                        /* 0x1D8C1 */
        if ((u8)enable != 0u) {                             /* 0x1D8C7 `test dl,dl` */
            u32 a = side != 0u ? 0x4000u : 0u;              /* 0x1D8CF..0x1D8E1 */
            DSD(DS_001028F0 + side * 4u) = actor_spawn(
                (const u32 *)(mem + DSD(DS_000A767C + side * 4u)),
                DSW(DS_000A768C + side * 2u), 0xFFu,
                DSW(DS_000A7690 + side * 2u), a);           /* 0x1D8F1..0x1D912 */
            DSD(DS_001028E8 + side * 4u) = actor_spawn(
                (const u32 *)(mem + DSD(DS_000A7684 + side * 4u)),
                DSW(DS_000A7694 + side * 2u), 0xFFu,
                DSW(DS_000A7698 + side * 2u), a);           /* 0x1D919..0x1D939 */
            fight_hud_bar_set(0, side);                     /* 0x1D941 0x1D2F0 */
            fight_hud_bar2_set(0, side);                    /* 0x1D94A 0x1D464 */
            DSD(DS_001028F8 + side * 4u) = actor_spawn(
                (const u32 *)(mem + DS_000BB664),
                DSW(DS_000A7628 + side * 2u), 0xFFu,
                DSW(DS_000A762C + side * 2u), a);           /* 0x1D96F..0x1D992 */
            DSD(DS_00102900 + side * 4u) = actor_spawn(
                (const u32 *)(mem + DS_000BB678),
                DSW(DS_000A7630 + side * 2u), 0xFFu,
                DSW(DS_000A7634 + side * 2u), a);           /* 0x1D9A0..0x1D9B7 */
            DSD(DS_001028E0 + side * 4u) = 0u;              /* 0x1D9C0 (EBX) */
            fight_hud_badge_spawn(side, DSB(DS_0010782A + side * 0x94u),
                                  DSW(DS_000A76D0));        /* 0x1D9C6..0x1D9D5 0x1D838 */
            DSB(DS_00104AEC) = (u8)(DSB(DS_00104AEC) | 2u); /* 0x1D9DA */
        }
    }
}

/* 0x1DC6C — record §48-U. 0x1D890 instruction for instruction but for the
 * first record's descriptor (0x1DCBC `test esi,esi`): side 0 takes
 * 0xA767C[0] (as 0x1D890) and side 1 0xA7674[1] = 0xA7678 (0x1D890's is
 * 0xA7680). In the image 0xA767C/0xA7680 both hold 0xA7638 and 0xA7678 holds
 * 0xA7660, the descriptor whose first sprite word is 0xC9A52's. The stack
 * slots of a and the slot offset are swapped; nothing else differs. Callers:
 * 0x25C3A (0x25C1C, EAX = 1, with DS_00104B1D == 2) and 0x25AB6 (0x25A84, no
 * entrance). */
void fight_hud_spawn_b(u32 enable)
{
    for (u32 side = 0; side < 2u; side++) {                 /* 0x1DC83 loop */
        DSB(DS_0010780E + side * 0x94u) = 0;                /* 0x1DC89 */
        DSB(DS_0010290C + side) = 0;                        /* 0x1DC8F */
        DSB(DS_0010780A + side * 0x94u) = 0;                /* 0x1DC95 */
        DSB(DS_0010290E + side) = 0;                        /* 0x1DC9F */
        if ((u8)enable != 0u) {                             /* 0x1DCA5 `test dl,dl` */
            u32 a = side != 0u ? 0x4000u : 0u;              /* 0x1DCAD..0x1DCBA */
            u32 d = side == 0u ? DSD(DS_000A767C)
                               : DSD(DS_000A7674 + side * 4u);  /* 0x1DCBC..0x1DCC8 */
            DSD(DS_001028F0 + side * 4u) = actor_spawn(
                (const u32 *)(mem + d),
                DSW(DS_000A768C + side * 2u), 0xFFu,
                DSW(DS_000A7690 + side * 2u), a);           /* 0x1DCE3..0x1DCFD */
            DSD(DS_001028E8 + side * 4u) = actor_spawn(
                (const u32 *)(mem + DSD(DS_000A7684 + side * 4u)),
                DSW(DS_000A7694 + side * 2u), 0xFFu,
                DSW(DS_000A7698 + side * 2u), a);           /* 0x1DD04..0x1DD24 */
            fight_hud_bar_set(0, side);                     /* 0x1DD2C 0x1D2F0 */
            fight_hud_bar2_set(0, side);                    /* 0x1DD35 0x1D464 */
            DSD(DS_001028F8 + side * 4u) = actor_spawn(
                (const u32 *)(mem + DS_000BB664),
                DSW(DS_000A7628 + side * 2u), 0xFFu,
                DSW(DS_000A762C + side * 2u), a);           /* 0x1DD59..0x1DD7B */
            DSD(DS_00102900 + side * 4u) = actor_spawn(
                (const u32 *)(mem + DS_000BB678),
                DSW(DS_000A7630 + side * 2u), 0xFFu,
                DSW(DS_000A7634 + side * 2u), a);           /* 0x1DD89..0x1DDA0 */
            DSD(DS_001028E0 + side * 4u) = 0u;              /* 0x1DDAA (EBX) */
            fight_hud_badge_spawn(side, DSB(DS_0010782A + side * 0x94u),
                                  DSW(DS_000A76D0));        /* 0x1DDB0..0x1DDBF 0x1D838 */
            DSB(DS_00104AEC) = (u8)(DSB(DS_00104AEC) | 2u); /* 0x1DDC4 */
        }
    }
}

/* ---- 0x20EF8 the round reset --------------------------------------------- */

/* 0x38BEC — record §48-U. EAX = the side. The words 0x107D2C, 0x107D20,
 * 0x107D18, 0x107D1C and 0x107D24 [side] = 0 (BX), then the 64 bytes
 * 0x107A80 + side * 0x40 .. + 0x3F = 0 (0x38C22 `inc eax` before the store at
 * [eax + 0x107A7F]). EBX/EDX are pushed and popped. Only caller: 0x20F07. */
static void fight_side_scratch_clear(u32 side)
{
    DSW(DS_00107D2C + side * 2u) = 0u;                  /* 0x38BF5 */
    DSW(DS_00107D20 + side * 2u) = 0u;                  /* 0x38BFD */
    DSW(DS_00107D18 + side * 2u) = 0u;                  /* 0x38C05 */
    DSW(DS_00107D1C + side * 2u) = 0u;                  /* 0x38C0D */
    DSW(DS_00107D24 + side * 2u) = 0u;                  /* 0x38C15 */
    for (u32 i = 0; i < 0x40u; i++)                     /* 0x38C22..0x38C2D */
        DSB(DS_00107A80 + side * 0x40u + i) = 0u;       /* 0x38C25 [eax+0x107A7F] */
}

/* 0x46670 — record §48-U. For EAX = 0x40 and 0x80 (0x46673 `add eax,0x40`
 * before the stores, 0x4668C `cmp eax,0x80`): the byte [EAX + 0x1081DE] = 0
 * and the dwords [EAX + 0x1081B0] and [EAX + 0x1081B4] = 0. So 0x10821E,
 * 0x10825E, 0x1081F0, 0x1081F4, 0x108230 and 0x108234. Only caller: 0x20F8C. */
static void fight_1081f0_clear(void)
{
    for (u32 o = 0; o <= 0x40u; o += 0x40u) {           /* 0x46673..0x46691 */
        DSB(DS_0010821E + o) = 0u;                      /* 0x46678 [eax+0x1081DE] */
        DSD(DS_001081F0 + o) = 0u;                      /* 0x46680 [eax+0x1081B0] */
        DSD(DS_001081F4 + o) = 0u;                      /* 0x46686 [eax+0x1081B4] */
    }
}

/* 0x20EF8 — record §48-U. Per side s (ECX; ESI, EBX and EDX advance to
 * (s + 1) * 2, * 4 and * 0x94 after 0x38BEC(s) and before the stores): the
 * slot's +0x74, +0x76, +0x78, +0x8C = 0 and +0x84 = AX = 0 (AH and AL were
 * zeroed at 0x20F22/0x20F31; 0x38BEC leaves EAX at s * 0x40 + 0x40), +0x76
 * again = AX; the bytes DS_001078F2[s] and DS_00100B5E[s] = 0; the bytes
 * 0xFD158/0xFD159/0xFD15A/0xFD15B + s * 4 = 0 and the dword 0xFD148 + s * 4 =
 * 0; the word DS_00100B50[s] = 0xFFFF (DI). Then 0x46670 and the byte
 * DS_00100C1D = 0. EBX..EDI are pushed and popped. Callers: 0x25C70
 * (0x25C1C) and the unported 0x26AAF (0x26A50). */
void fight_round_reset(void)
{
    for (u32 s = 0; s < 2u; s++) {                      /* 0x20F05..0x20F86 */
        u32 slot = DS_001077B0 + s * 0x94u;
        fight_side_scratch_clear(s);                    /* 0x20F07 0x38BEC */
        DSW(slot + 0x74u) = 0u;                         /* 0x20F1B */
        DSW(slot + 0x76u) = 0u;                         /* 0x20F24 */
        DSB(DS_001078F2 + s) = 0u;                      /* 0x20F2B */
        DSW(slot + 0x78u) = 0u;                         /* 0x20F33 */
        DSB(DS_000FD158 + s * 4u) = 0u;                 /* 0x20F3A */
        DSB(DS_000FD159 + s * 4u) = 0u;                 /* 0x20F42 */
        DSD(DS_000FD148 + s * 4u) = 0u;                 /* 0x20F48 */
        DSB(DS_000FD15A + s * 4u) = 0u;                 /* 0x20F4E */
        DSB(DS_000FD15B + s * 4u) = 0u;                 /* 0x20F56 */
        DSW(slot + 0x84u) = 0u;                         /* 0x20F5C (AX) */
        DSB(DS_00100B5E + s) = 0u;                      /* 0x20F63 */
        DSW(slot + 0x8Cu) = 0u;                         /* 0x20F69 */
        DSW(slot + 0x76u) = 0u;                         /* 0x20F75 (AX) */
        DSW(DS_00100B50 + s * 2u) = 0xFFFFu;            /* 0x20F70/0x20F7C (DI) */
    }
    fight_1081f0_clear();                               /* 0x20F8C 0x46670 */
    DSB(DS_00100C1D) = 0u;                              /* 0x20F93 */
}

/* ---- 0x33F08 the health-bar pass ---------------------------------------- */

void fight_health_bars(void)
{
    u32 table = DSD(DS_001014EC);                           /* 0x33F0D */
    for (u32 side = 0; side < 2u; side++) {                 /* 0x33F13 */
        u32 rec = DSD(DS_001077A8 + side * 4u);             /* 0x33F15 */
        if (rec == 0) continue;                             /* 0x33F1D */
        u32 fighter = DSD(rec);                             /* 0x33F76 */
        u32 actorword = DSW(table
            + (u32)(u16)DSW(fighter + 0x56u) * 0x20u);      /* 0x33F87 */
        s32 s = (s32)(actorword & 0x7FFFu)
              - (s32)camera_char_const(DSB(rec + 0x7Au));   /* 0x33F8E */
        u32 pset = DSD(rec + 4u);                           /* 0x33FA8 */
        if ((DSB(rec + 0x41u) & 0x20u) == 0 && s >= 0 && s < 0x4B1) {
            DSD(pset + 8u) = (u32)DSW(DSD(rec + 0x24u) + (u32)s * 2u);  /* 0x33FC4 */
        } else {
            DSD(pset + 8u) = 0x1E1u;                        /* 0x33FAB */
        }
        if ((actorword & 0x8000u) != 0)                     /* 0x33FDE */
            DSB(pset + 0x29u) |= 0x40u;                     /* 0x33FEC */
        else
            DSB(pset + 0x29u) &= 0xBFu;                     /* 0x33FF5 */
        {
            u32 secondary = DSD(rec + 4u);                  /* 0x33FF9 */
            u32 spset = table
                + (u32)(u16)DSW(secondary + 0x56u) * 0x20u; /* 0x34005 */
            DSW(spset) = (u16)anim_next_sprite_id(secondary, spset);
        }
    }
}

/* ---- 0x3CB68 the 2x32 slot pass ---------------------------------------- */

void fight_slot_pass(void)
{
    /* 0x3CB6B saves DS_00107ED8 and 0x3CB71 zeroes DS_00107EDC; the outer loop
     * increments DS_00107EDC to 2 and the inner runs 0x20 times per side. */
    DSD(DS_00107EDC) = 0;                       /* 0x3CB71 */
    for (u32 side = 0; side < 2u; side++) {
        DSB(DS_00107EE4) = (u8)fighter_actor_bit15_clear(side);   /* 0x3CB89 */
        for (u32 i = 0; i < 0x20u; i++) {
            DSD(DS_00107ED8) = i;               /* 0x3CB96 */
            hit_slot_step();                    /* 0x3CB9B 0x3C88C */
        }
        DSD(DS_00107EDC) = side + 1u;           /* 0x3CBA8 */
    }
    DSD(DS_00107EDC) = 2;                       /* 0x3CBAE */
    DSD(DS_00107ED8) = 0x20u;                   /* 0x3CBB9 */
}

/* ---- 0x1A978 the stance/command pass ----------------------------------- */

void fight_stance_pass(u32 side)
{
    u32 ctx[6];
    fighter_ctx_swap(ctx, side);                /* 0x1A984 0x33A10 */
    u32 self = ctx[3], other = ctx[2];

    /* 0x1A989: other+0x5F != -1 and self+0x62 != 0 arm the command mapper. */
    {
        u8 st = (u8)DSB(other + 0x5fu);
        if (st != 0xFFu && DSB(self + 0x62u) != 0)
            fight_command_map(ctx[1], (u32)st, 0u);  /* 0x1A9AE 0x3B134 */
    }

    /* 0x1A9B3: self +0x63 blocks the +0x61 stance-timer block. */
    if (DSB(self + 0x63u) == 0) {
        DSB(self + 0x61u) = (u8)(DSB(self + 0x61u) - 1u);   /* 0x1A9C2 */
        if ((s8)DSB(self + 0x61u) <= 0) {                    /* 0x1A9CD */
            DSB(self + 0x61u) = 0;                           /* 0x1A9CF */
            /* 0x1A9D2: bit 0x4000 of the side command drives +0x54. */
            if ((DSW(DS_001088E0 + ctx[1] * 2u) & 0x4000u) != 0) {
                if (DSB(self + 0x54u) != 1) {
                    DSB(self + 0x54u) = 1;                   /* 0x1A9F0 */
                    fighter_block_anim(self, ctx[5]);        /* 0x1AA24 0x1A6AC */
                }
            } else if (DSB(self + 0x54u) != 0) {
                DSB(self + 0x54u) = 0;                       /* 0x1AA18 */
                fighter_block_anim(self, ctx[5]);            /* 0x1AA24 0x1A6AC */
            }
        }
    }

    /* 0x1AA29: the +0x62 attack window and the +0x60 timer. */
    if (DSB(self + 0x62u) != 0) {
        DSB(self + 0x60u) = (u8)(DSB(self + 0x60u) - 1u);    /* 0x1AA35 */
        if (((s32)DSD(self + 0x5du) >> 24) < 1) {            /* 0x1AA4A */
            /* 0x1AA5F `cmp dx,[other+0x84]; jne 0x1AA6F`: the store runs
             * when the words are equal (record §38 corrects `!=`). */
            if (DSW(self + 0x86u) == DSW(other + 0x84u))     /* 0x1AA5F */
                DSB(other + 0x8au) = 0;                      /* 0x1AA68 */
            DSB(self + 0x62u) = 0;                           /* 0x1AA73 */
            DSB(self + 0x60u) = 0x28;                        /* 0x1AA7B */
            DSB(self + 0x53u) = 0;                           /* 0x1AA83 */
        }
    } else {
        DSB(self + 0x60u) = (u8)(DSB(self + 0x60u) - 1u);    /* 0x1AA8C */
        {
            u16 cmd = DSW(DS_001088E0 + ctx[1] * 2u);        /* 0x1AAA2 */
            if (fighter_1a640(ctx[1]) == 0                   /* 0x1AA93/0x1AA9A */
                    || (cmd & 0x8000u) != 0u                 /* 0x1AAAB/0x1AAB3 */
                    || (cmd & 0x000Fu) != 0u                 /* 0x1AABE/0x1AAC5 */
                    || (((s32)DSD(self + 0x5du) >> 24) < 1   /* 0x1AAD1 */
                        && DSB(other + 0x5fu) == 0xFFu       /* 0x1AAE2 */
                        && DSB(other + 0x64u) == 0xFFu))     /* 0x1AAF5 */
                fighter_block_end(ctx[5]);                   /* 0x1AB04 0x1A8F4 */
        }
    }
}

/* ---- 0x3BDB0 the attack-readiness gate --------------------------------- */

static int fight_attack_ready(u32 side)
{
    u32 ctx[6];
    fighter_ctx_same(ctx, side);                /* 0x3BDB8 0x33950 */
    u32 self = ctx[2];
    return DSB(self + 0x53u) == 0 && DSB(self + 0x54u) != 2;
}

/* ---- 0x3B134 the command-word mapper ----------------------------------- */

void fight_command_map(u32 side, u32 edx_arg, u32 override)
{
    u32 ctx[6];
    fighter_ctx_swap(ctx, side);                             /* 0x3B149 */
    u32 anim[3];
    fighter_anim_triple(anim, ctx[0], (s32)edx_arg);         /* 0x3B155 */

    /* 0x3B168 gate A: slot[side]+0x63. */
    if (DSB(DS_001077B0 + side * 0x94u + 0x63u) == 0) return;
    /* 0x3B17A gate B: 0x1AB10. */
    if (!fighter_state_ok(ctx[1])) return;

    /* 0x3B18C: the rng(100) reaction roll against a per-character/controller
     * threshold; `override` bypasses it (0x3B1AD). */
    {
        u32 draw = rng_next(0x64u);                          /* 0x3B18C */
        u32 c8 = DSD(DS_001082C8 + side * 4u);               /* 0x3B191 */
        u32 idx = (u32)DSB(DS_0010452C) << 4;                /* 0x3B19A */
        s32 thr = (s32)DSD(DS_000BEDF2 + idx + c8 * 2u) >> 16;
        if ((s32)draw > thr && (override & 0xffu) == 0) return;
    }

    u32 self = ctx[3], other = ctx[2];
    /* 0x3B1BC: the facing-derived base from the two slot copies' +0x2C. */
    u32 base = ((s32)DSD(self + 0x2cu) > (s32)DSD(other + 0x2cu)) ? 0x1000u
                                                                  : 0x2000u;

    /* 0x3B1D8: the other slot's stance selects 0x5000/0x6000. */
    if (DSB(other + 0x64u) != 0xFFu && DSD(other + 0x08u) != 0) {
        s16 cx = (s16)DSW(DSD(other + 0x08u) + 0x34u);       /* 0x3B1FC */
        DSW(DS_001088E0 + side * 2u) = (u16)(cx < 0 ? 0x6000u : 0x5000u);
        return;
    }

    /* 0x3B220: the animation pointer's +2 bits select base/base|0x4000/0x8000. */
    {
        u8 b = DSB(anim[2] + 2u);
        if ((b & 1u) == 0) {
            DSW(DS_001088E0 + side * 2u) = (u16)base;
            return;
        }
        if ((b & 2u) == 0) {
            DSW(DS_001088E0 + side * 2u) = (u16)(base | 0x4000u);
            return;
        }
        DSW(DS_001088E0 + side * 2u) = 0x8000u;              /* 0x3B26F */
        if (fight_attack_ready(side))
            (void)fighter_attack_consume(side);              /* 0x3B28C */
    }
}

/* ---- 0x34B6C the health sync ------------------------------------------- */

/* 0x36E2C. The position gate that decides between the 0x34B97 position branch
 * and the 0x34BF4 +0x52 table: 1 when the slot's +0x63 is armable and
 * (DS_00104B1D == 3 || DS_00104B14 == 0 || slot+0x63 != 0), +0x42 bit 0x10 is
 * set, +0x52 is one of {0,1,5,6,7} and +0x54 <= 1. */
static int fight_position_gate(u32 rec)
{
    u8 st;
    if (!(DSB(DS_00104B1D) == 3u || DSB(DS_00104B14) == 0u
          || DSB(rec + 0x63u) != 0u))
        return 0;
    if ((DSB(rec + 0x42u) & 0x10u) == 0u) return 0;
    st = DSB(rec + 0x52u);
    if (!(st == 0u || st == 1u || st == 5u || st == 6u || st == 7u)) return 0;
    return DSB(rec + 0x54u) <= 1u;
}

static void fight_health_sync(u32 side)
{
    u32 rec = DSD(DS_001077A8 + side * 4u);     /* 0x34B73 */
    if (rec == 0) return;                       /* 0x34B7C */
    if (DSD(rec) == 0) return;                  /* 0x34B82/0x34B86 */

    if (fight_position_gate(rec) != 0) {        /* 0x34B8E 0x36E2C */
        u32 fighter = DSD(rec);
        s32 x = ((DSW(fighter + 0x28u) >> 8 & 0x40u) != 0u)
              ? (s32)DSD(fighter + 0x18u) + 0x3000
              : (s32)DSD(fighter + 0x18u) - 0x3000;
        if (x < (s32)DSD(DS_000BE018) && x > -(s32)DSD(DS_000BE018)) {
            /* PORT: 0x34BE8 0x36F10 — the in-range arm. It is unreachable:
             * slot+0x42 bit 0x10 is set only inside 0x36F10 itself (0x36FD4
             * `| 0x820` is bit 0x20, not 0x10; no writer sets bit 0x10), so the
             * gate can never return 1 without this call having already run.
             * Named gap (§7.10). */
        } else {
            DSB(rec + 0x43u) |= 0x40u;          /* 0x34BD1 */
            (void)fighter_state_36638(rec, fighter);   /* 0x34BDE */
        }
        return;
    }

    switch (DSB(rec + 0x52u)) {                 /* 0x34BF4 table 0x34B14 */
    case 0u:                                    /* 0x34C08 -> 0x349C8 */
        fighter_state_default(side);
        break;
    case 1u:                                    /* 0x34C15 -> 0x359E0 */
        fighter_state_359e0(rec, DSD(rec), side);
        break;
    case 2u:                                    /* 0x34C26 -> 0x35C1C/0x35D20 */
        if (fighter_state_35c1c(rec, DSD(rec)) != 0)
            fighter_state_35d20(rec, DSD(rec));
        break;
    case 3u:                                    /* 0x34C46 -> 0x35D7C */
        fighter_state_35d7c(side);
        break;
    case 4u:                                    /* 0x34C53 -> 0x35F84 */
        fighter_state_35f84(rec, DSD(rec));
        break;
    case 5u:                                    /* 0x34C62 -> 0x36430 */
        fighter_state_36430(rec, DSD(rec), side);
        break;
    case 6u:                                    /* 0x34C73 -> 0x1A978 */
        fight_stance_pass(side);
        break;
    case 7u:                                    /* 0x34C80 -> 0x399CC */
        fighter_state_399cc(side);
        break;
    case 8u:                                    /* 0x34C8D -> 0x37464 */
        fighter_state_37464(side);
        break;
    /* 9,10,11,14,15,16 -> 0x34D83, the epilogue no-op. */
    case 9u: case 10u: case 11u: case 14u: case 15u: case 16u:
        break;
    case 12u:                                   /* 0x34C9A -> 0x361C8 */
        fighter_state_361c8(rec, DSD(rec));
        break;
    case 13u:                                   /* 0x34CA9 -> 0x36300 */
        fighter_state_36300(rec, DSD(rec));
        break;
    case 17u:                                   /* 0x34CB8 -> 0x36710 */
        fighter_state_36710(rec, DSD(rec));
        break;
    case 18u: /* PORT: 0x34CC7 inline cmd gate + 0x3BDDC -> 0x18B04 (§7.10) */ break;
    case 19u:                                   /* 0x34D22 inline + 0x33B00 */
        {
            s16 v = (s16)(DSW(rec + 0x8Eu) - 1u);       /* 0x34D22 */
            DSW(rec + 0x8Eu) = (u16)v;                  /* 0x34D2A */
            if (v < 0) {
                for (u32 i = 0; i < 2u; i++) {
                    u32 s = DS_001077B0 + i * 0x94u;
                    if (DSB(s + 0x52u) == 0x13u)        /* 0x34D3F */
                        fighter_state_33b00(i,
                            0x00107BD0u + i * 0x94u,    /* 0x34D48 */
                            0x00107B00u + i * 0x68u);   /* 0x34D4D */
                }
            }
        }
        break;
    case 20u:                                   /* 0x34D69 -> 0x35E6C */
        fighter_state_35e6c(rec, DSD(rec));
        break;
    case 21u:                                   /* 0x34D78 -> 0x364FC */
        fighter_state_364fc(rec, DSD(rec), side);
        break;
    default:                                    /* >0x15 -> 0x34C08 0x349C8 */
        fighter_state_default(side);
        break;
    }
}

/* ---- 0x35658 the HUD/health pass --------------------------------------- */

void fight_hud_pass(u32 side)
{
    u32 slot = DS_001077B0 + side * 0x94u;
    u32 rec;

    /* 0x35661..0x35773: the per-side preamble. 0x3C59C(3, side) is a
     * test-and-set fight_slot_clear zeroes each arena frame, so it runs once
     * per side; it maintains the slot+0x88/+0x8C/+0x90/+0x92 timers and
     * slot+0x78 that 0x350D0 and 0x3531C case 8 read. */
    if (fighter_pass_flag(3u, side) == 0) {                 /* 0x356BE */
        DSW(slot + 0x88u) = (u16)(DSW(slot + 0x88u) + 1u);  /* 0x356D6 */
        {
            s16 c = (s16)DSW(slot + 0x8Cu);                 /* 0x356CF */
            if (c > 0)      DSW(slot + 0x8Cu) = (u16)(c - 1);   /* 0x356E5 */
            else if (c < 0) DSW(slot + 0x8Cu) = 0;          /* 0x356F2 */
        }
        if ((s32)DSD(slot + 0x90u) >> 16 < 0x270F)          /* 0x35713 */
            DSW(slot + 0x92u) = (u16)(DSW(slot + 0x92u) + 1u);  /* 0x3571B */
    }
    {
        u16 cap = DSW(DS_000BDBEC);                         /* 0x35733 */
        u16 v = DSW(slot + 0x78u);                          /* 0x3573A */
        if ((s16)v > (s16)cap)      DSW(slot + 0x78u) = 0;  /* 0x35748 */
        else if ((s16)v > 0)        DSW(slot + 0x78u) = (u16)((s16)v - 1); /* 0x35757 */
    }
    DSB(DS_001078FB) = (u8)((((DSB(DS_001088D4) & 4u) != 0u) ? 0u : 1u) + 2u); /* 0x35770 */

    rec = DSD(DS_001077A8 + side * 4u);                     /* 0x35775 */
    if (rec == 0) return;                                   /* 0x3577E */

    if (DSW(DS_00104B00) == 4 && (DSB(rec + 0x43u) & 0x80u) == 0) {
        /* 0x35792..0x357DB (record §49-A). Bit 0 of DS_001088E0[side] gates
         * a respawn whose register setup (0x357AB..0x357D6: EDX = the s32
         * DS_000BDA38[side] shifted right 16, ECX = DS_000BD898, EBX(stack)
         * = 0x4000 for side 0 else 0, EBX(reg) = 0) is byte-for-byte
         * fighter_spawn's own wrapper (0x33EB4..0x33ED7), so the port calls
         * it directly instead of re-deriving 0x33C78's args. When the bit is
         * clear, the raw writes a local stack scratch word (0x357DD) that is
         * never read again before the function returns; the port skips it. */
        if ((DSW(DS_001088E0 + side * 2u) & 1u) != 0u)      /* 0x35798..0x357A9 */
            fighter_spawn(side);                             /* 0x357AB..0x357D6 0x33C78 */
        return;                                              /* 0x357DB/0x357E2 -> 0x3582E */
    }

    /* 0x357E4: rec is the camera-target record; *rec is the fighter record.
     * `[eax+0x28] = [*rec+0x18]` (eax=rec), and 0x35810's `[*rec+0x28] |= 1`
     * uses the same ebx (= *rec), which the callee-saved register holds across
     * the calls. */
    {
        u32 fighter = DSD(rec);                 /* 0x357E4 ebx = [eax] */
        DSD(rec + 0x28u) = DSD(fighter + 0x18u);          /* 0x357E6/0x357E9 */
        fighter_34038(side);                    /* 0x357EC/0x357EE 0x34038 (record §48-K) */
        fighter_38d24(side);                    /* 0x357F5 0x38D24 */
        fight_health_sync(side);                /* 0x357FC 0x34B6C */
        fighter_state_3531c(side);              /* 0x35803 0x3531C */
        DSB(fighter + 0x28u) = (u8)(DSB(fighter + 0x28u) | 1u); /* 0x35808..0x35810 */
        actor_sync(fighter);                    /* 0x35813 0x2A1FC */
    }
    fighter_wall_clamp(side);                    /* 0x3581C 0x354F0 (record §49-A) */
    fighter_wall_clamp(1u - side);               /* 0x35824 0x354F0 (record §49-A) */
    fighter_slot_latch_both();                  /* 0x35829 0x186C4 */
}

/* ---- 0x49C78 the scene/effects pass ------------------------------------ */

/* 0x2BE00. The actor's +4 field for a fighter record (a raw position term the
 * effect bodies use as a sign test). */
static s32 fight_2be00(u32 rec)
{
    u32 actor = DSD(DS_001014EC) + (u32)DSW(rec + 0x56u) * 0x20u;
    return (s32)DSD(actor + 4u);
}

/* 0x493F0. The midpoint of the two fighters' 0x2BE00 values (arithmetic shift:
 * floor-division by two after the difference is made non-negative). */
static u32 fight_midpoint(void)
{
    s32 a = fight_2be00(DSD(DS_001077B0));
    s32 b = fight_2be00(DSD(DS_00107844));
    s32 d = a - b;
    if (d >= 0) return (u32)((d >> 1) + b);
    return (u32)(((-d) >> 1) + a);
}

/* The fighter record cases 13/14 gate on: slot[ (s8)(DS_001088C6 >> 24) ]. */
static u32 fight_case_rec(void)
{
    s32 idx = (s32)DSD(DS_001088C6) >> 24;
    return DSD(DS_001077B0 + (u32)idx * 0x94u);
}

/* ---- the type-0 effect handler 0x4AAD0 and its callees ------------------ */

/* The 0xC95xx per-dust-descriptor stream/flag tables. The index is the entry's
 * `si` = (u16)(actor+0x48 - 0x20) (0x49CE1/0x49CE7). Ghidra emits them as
 * DAT_000c95xx, so gen_symbols.py has no DS_ name. */
#define DS_000C955C 0x000C955Cu
#define DS_000C958C 0x000C958Cu
#define DS_000C95BC 0x000C95BCu
#define DS_000C95D4 0x000C95D4u
#define DS_000C9754 0x000C9754u

/* 0x2BE1C. The difference of two fighter records' 0x2BE00 position terms. */
static s32 fight_2be1c(u32 rec_a, u32 rec_b)
{
    return fight_2be00(rec_a) - fight_2be00(rec_b);
}

/* 0x2BE4C. `(rec+0x44 >> 16) * 2 + value - 0x2A00` (0x2BE4C..0x2BE5B). */
static s32 fight_2be4c(u32 rec, s32 value)
{
    return (((s32)DSD(rec + 0x44u) >> 16) * 2) + value - 0x2A00;
}

/* 0x4B3F0. When the 0xC955C[index] stream is set, point the entry's actor at it
 * and set the entry's type to 8. EAX = entry, EDX = index, EBX = the flag: the
 * actor's +0x55 byte becomes 1 when EBX is non-zero (0x4B3FA `test ebx,ebx`,
 * 0x4B401), else 0 (0x4B40A). EBX = 1 at 0x4AC80's call 0x4AE32 (0x4AE2B)
 * and FUN_0004a708's call 0x4A75C (0x4A751); the other callers zero it:
 * 0x4AAD0's 0x4AB1A/0x4AB6F (0x4AB18/0x4AB6D), 0x4BFA7 (0x4BFA5), 0x4C5D3
 * (0x4C5D1) and 0x4D27A (0x4D278). */
static int fight_4b3f0(u32 entry, u32 index, u32 flag)
{
    u32 stream = DSD(DS_000C955C + index * 4u);     /* 0x4B3F0 */
    if (stream == 0u) return 0;                     /* 0x4B3F8 */
    u32 actor = DSD(entry + 8u);
    DSB(actor + 0x55u) = (flag != 0u) ? 1u : 0u;    /* 0x4B3FA..0x4B40A */
    DSB(entry + 0x1Eu) = 8u;                        /* 0x4B40E */
    actors_anim_begin(actor, stream, 0x40400000u);  /* 0x4B421 */
    return 1;                                       /* 0x4B426 */
}

/* 0x4B430. As 0x4B3F0 but over the 0xC958C table (the flag test at 0x4B43A).
 * EBX = 1 at 0x4AC80's call 0x4AE48 (0x4AE41), 0x49C78's call 0x4A14F
 * (0x4A143) and FUN_0004a708's call 0x4A76E (0x4A763); the other callers zero
 * it: 0x4AAD0's 0x4AAFE/0x4AB99 (0x4AAFC/0x4AB97), 0x4BFC3 (0x4BFC1), 0x4C5E7
 * (0x4C5E5) and 0x4D2A0 (0x4D29E). */
static int fight_4b430(u32 entry, u32 index, u32 flag)
{
    u32 stream = DSD(DS_000C958C + index * 4u);     /* 0x4B430 */
    if (stream == 0u) return 0;                     /* 0x4B438 */
    u32 actor = DSD(entry + 8u);
    DSB(actor + 0x55u) = (flag != 0u) ? 1u : 0u;    /* 0x4B43A..0x4B44A */
    DSB(entry + 0x1Eu) = 8u;                        /* 0x4B44E */
    actors_anim_begin(actor, stream, 0x40400000u);  /* 0x4B461 */
    return 1;                                       /* 0x4B466 */
}

/* 0x4BD4C. When the 0xC9754[index] stream is set, zero the actor's +0x34/+0x36/
 * +0x38, point it at the stream, then set the entry's type to 8. */
static int fight_4bd4c(u32 entry, u32 index)
{
    u32 stream = DSD(DS_000C9754 + index * 4u);     /* 0x4BD52 */
    if (stream == 0u) return 0;                     /* 0x4BD59 */
    u32 actor = DSD(entry + 8u);
    DSW(actor + 0x38u) = 0;                         /* 0x4BD5E */
    DSW(actor + 0x34u) = 0;                         /* 0x4BD67 */
    DSW(actor + 0x36u) = 0;                         /* 0x4BD70 */
    actors_anim_begin(actor, stream, 0x40800000u);  /* 0x4BD84 */
    DSB(entry + 0x1Eu) = 8u;                        /* 0x4BD8E */
    return 1;                                       /* 0x4BD89 */
}

/* 0x4B5A8. The `DS_001088B6[char] != 0 || (+0x52 == 7 && +0x53 == 2)` gate: on
 * true it retargets the actor at the 0xC95BC[index] stream, sets the entry's
 * +0x1A/+0x14 and type 3, and decrements DS_001088B6[char]. The raw returns the
 * flag in AL; the caller tests AL only. */
static int fight_4b5a8(u32 entry, u32 index)
{
    u32 slot = DSD(entry + 0xCu);
    u32 rec = DSD(slot);
    u8 ch = DSB(rec + 0x51u);                       /* 0x4B5B1 */
    if (DSB(DS_001088B6 + ch) == 0u
            && !(DSB(slot + 0x52u) == 7u && DSB(slot + 0x53u) == 2u))
        return 0;                                   /* 0x4B694 */
    u32 actor = DSD(entry + 8u);
    actors_anim_begin(actor, DSD(DS_000C95BC + index * 4u), 0x40400000u); /* 0x4B5E6 */
    DSW(entry + 0x1Au) = DSW(actor + 0x32u);        /* 0x4B5F2 */
    DSW(actor + 0x38u) = 0xFFC0u;                   /* 0x4B5F9 */
    s32 v;
    if (((DSW(rec + 0x28u) >> 8) & 0x40u) == 0u) {  /* 0x4B612 */
        DSW(actor + 0x34u) = 0xFFC0u;               /* 0x4B632 */
        v = fight_2be00(rec) - 0x1800;              /* 0x4B642 */
    } else {
        DSW(actor + 0x34u) = 0x0040u;               /* 0x4B617 */
        v = fight_2be00(rec) + 0x1800;              /* 0x4B627 */
    }
    DSD(entry + 0x14u) = (u32)fight_2be4c(actor, v);    /* 0x4B650 */
    DSB(entry + 0x1Eu) = 3u;                        /* 0x4B656 */
    u8 n = DSB(slot + 0x81u);
    if (DSB(DS_001088B6 + ch) > n)                  /* 0x4B671 */
        DSB(DS_001088B6 + ch) = n;                  /* 0x4B675 */
    DSB(DS_001088B6 + ch) -= 1u;                    /* 0x4B68B */
    return 1;                                       /* 0x4B691 */
}

/* The 0xC9544 per-descriptor arrival-stream table (0x4AC68 `mov edx,[ebx*4 +
 * 0xc9544]`); Ghidra emits it as DAT_000c9544, so gen_symbols.py has no DS_
 * name. Entry 0 is 0xEE02C, the type-0x20 worshipper's spawn stream. */
#define DS_000C9544 0x000C9544u

/* The 0xC9634 landing- and 0xC95EC rising-stream tables of the effects pass's
 * cases 3 and 4 (0x49E15 `mov edx,[eax*4 + 0xc9634]`, 0x49E74 `mov edx,[edx*4 +
 * 0xc95ec]`); Ghidra emits them as DAT_, so gen_symbols.py has no DS_ name. */
#define DS_000C9634 0x000C9634u
#define DS_000C95EC 0x000C95ECu

/* 0x4AC38. The arrival: the entry's actor stops (the +0x34/+0x36/+0x38 words
 * zeroed), clears its hflip (+0x29 &= 0xBF) and sets +0x29 bit 0x10, the entry
 * returns to type 0 and the actor is pointed at the 0xC9544[index] stream with
 * the hold 5.0 (`push 0x40a00000`, 0x4AC72). EAX = entry, EDX = index. */
static void fight_4ac38(u32 entry, u32 index)
{
    u32 actor = DSD(entry + 8u);
    DSB(actor + 0x29u) &= 0xBFu;                    /* 0x4AC3E */
    DSB(actor + 0x29u) |= 0x10u;                    /* 0x4AC45 */
    DSW(actor + 0x34u) = 0;                         /* 0x4AC4C */
    DSW(actor + 0x36u) = 0;                         /* 0x4AC55 */
    DSW(actor + 0x38u) = 0;                         /* 0x4AC5E */
    DSB(entry + 0x1Eu) = 0;                         /* 0x4AC64 */
    actors_anim_begin(actor, DSD(DS_000C9544 + index * 4u), 0x40A00000u); /* 0x4AC77 */
}

/* 0x4AC18 — demo-pose record §23. The worshipper streams' opcode-0x15 target:
 * the arrival 0x4AC38 for the actor's own +0x14 entry, indexed by the actor's
 * +0x48 descriptor byte less 0x20 (0x4AC25 `mov dl,[eax+0x48]`, 0x4AC28 `sub
 * edx,0x20`, a 32-bit index, unlike the effects pass's u16 `si`). */
void fight_4ac18(u32 rec)
{
    u32 entry = DSD(rec + 0x14u);                   /* 0x4AC1A */
    if (entry == 0u) return;                        /* 0x4AC1F */
    fight_4ac38(entry, (u32)DSB(rec + 0x48u) - 0x20u); /* 0x4AC25..0x4AC2D */
}

/* The 0xC95D4 walk- and 0xC958C/0xC955C held-stream tables are defined above;
 * 0x4AC80 also reads DS_00108868 (a record, written only by 0x4BD98 at
 * 0x4BEA3) and DS_00108864 (an entry, written by 0x4BD98/0x4CD98/0x4C784
 * and by 0x4AC80 itself at 0x4ADE1). Ghidra also lists 0x27C24/0x27C36
 * (0x27BA4) against DS_00108864, but those are the words `[eax+0x108860]`
 * with EAX = 0 or 2, DS_00108860[0..1] (record §48-K). */

/* 0x4AC80 — demo-pose record §31. The worshipper landing streams' opcode-0x15
 * target (the dword 0x0004AC80 after the 0xD500 word at 0xEE3BA, 0xEE6FA,
 * 0xEEAB6, 0xEEEA8, 0xEF25E and 0xEF606). EAX = the record; its +0x14 entry,
 * when non-zero, is ECX and the index is the 32-bit (u8)+0x48 - 0x20 (0x4AC98..
 * 0x4ACB1). The entry's actor (+8) loses its hflip and gains +0x29 bit 4. Then
 * by the entry's +0x1C bit 5 and DS_001088C5: set and non-zero, the actor walks
 * to 0x1740 beside the DS_00108868 record (type 1, the 0xC95D4 stream) or, when
 * it already stands there, is held (type 8, the 0xC958C stream, +0x55 = 1);
 * set and zero, the DS_00108864 entry is released (type 4, bit 5 cleared) and
 * DS_00108864 zeroed; clear, in modes 8/9/0x17 the actor stops and, by
 * DS_00104B16 against the entry's +0x21 (0x4AE26), 0x4B3F0 (equal) or 0x4B430
 * holds it with EBX = 1; otherwise it climbs as the effects pass's case 4 does
 * (the 0xC95EC stream, type 5, +0x1C &= 0x3F). */
void fight_4ac80(u32 rec)
{
    u32 entry = DSD(rec + 0x14u);                   /* 0x4AC8B */
    if (entry == 0u) return;                        /* 0x4AC90 */
    u32 index = (u32)DSB(rec + 0x48u) - 0x20u;      /* 0x4AC98..0x4ACB1 */
    DSB(DSD(entry + 8u) + 0x29u) &= 0xBFu;          /* 0x4ACA0 */
    DSB(DSD(entry + 8u) + 0x29u) |= 0x10u;          /* 0x4ACA7 */
    u32 bit5 = (u32)DSB(entry + 0x1Cu) & 0x20u;     /* 0x4ACBC..0x4ACC4 */
    if (DSB(DS_001088C5) != 0u && bit5 != 0u) {     /* 0x4ACB6/0x4ACC9 */
        s32 pos = fight_2be00(DSD(entry + 8u));     /* 0x4ACD2 */
        s32 base = fight_2be00(DSD(DS_00108868));   /* 0x4ACE0 */
        /* 0x4ACED..0x4AD27: the `sub`/`add` wrap in 32 bits before the signed
         * compares, so the bands are computed in u32. */
        s32 lo = (s32)((u32)base - 0x1740u);        /* 0x4ACED */
        s32 mid_lo = (s32)((u32)base - 0xBA0u);     /* 0x4AD02 */
        s32 hi = (s32)((u32)base + 0x1740u);        /* 0x4AD19 */
        s32 mid_hi = (s32)((u32)base + 0xBA0u);     /* 0x4AD27 */
        s32 target = 0;                             /* 0x4ACF3 */
        if (pos < lo)                               /* 0x4ACFA */
            target = lo;                            /* 0x4ACFC */
        else if (pos > mid_lo && pos < base)        /* 0x4AD0A/0x4AD0E */
            target = lo;                            /* 0x4AD10 */
        else if (pos > hi)                          /* 0x4AD21 */
            target = hi;                            /* 0x4AD37 */
        else if (pos < mid_hi && pos > base)        /* 0x4AD2F/0x4AD35 */
            target = hi;                            /* 0x4AD37 */
        u32 stream;
        if (target != 0) {                          /* 0x4AD3B */
            DSD(entry + 0x14u) = (u32)target;       /* 0x4AD3D */
            DSB(entry + 0x1Eu) = 1u;                /* 0x4AD43 */
            if (fight_2be00(DSD(entry + 8u)) < (s32)DSD(entry + 0x14u)) { /* 0x4AD4F */
                DSW(DSD(entry + 8u) + 0x34u) = 0x0040u;     /* 0x4AD54 */
                DSB(DSD(entry + 8u) + 0x29u) &= 0xBFu;      /* 0x4AD5D */
            } else {
                DSW(DSD(entry + 8u) + 0x34u) = 0xFFC0u;     /* 0x4AD66 */
                DSB(DSD(entry + 8u) + 0x29u) |= 0x40u;      /* 0x4AD6F */
            }
            stream = DSD(DS_000C95D4 + index * 4u); /* 0x4AD73 */
        } else {
            /* PORT: 0x4AD81 0x2C3FC(0xC8) voice, not wired (record §45-A). */
            DSB(entry + 0x1Eu) = 8u;                /* 0x4AD89 */
            DSB(DSD(entry + 8u) + 0x55u) = 1u;      /* 0x4AD8D */
            stream = DSD(DS_000C958C + index * 4u); /* 0x4AD91 */
        }
        actors_anim_begin(DSD(entry + 8u), stream, 0x40400000u); /* 0x4ADA0 */
        DSW(DSD(entry + 8u) + 0x36u) = 0;           /* 0x4ADA8 */
        return;
    }
    if (bit5 != 0u) {                               /* 0x4ADC5 */
        u32 held = DSD(DS_00108864);                /* 0x4ADC7 */
        u8 bh = DSB(held + 0x1Cu);                  /* 0x4ADCC */
        DSW(held + 0x18u) = 0;                      /* 0x4ADCF */
        DSB(held + 0x1Cu) = (u8)(bh & 0xDFu);       /* 0x4ADD5/0x4ADD8 */
        DSB(held + 0x1Eu) = 4u;                     /* 0x4ADDD */
        DSD(DS_00108864) = 0;                       /* 0x4ADE1 */
        return;
    }
    u16 mode = DSW(DS_00104B00);                    /* 0x4ADF1 */
    if (mode == 8u || mode == 9u || mode == 0x17u) {    /* 0x4ADF7..0x4AE04 */
        DSW(DSD(entry + 8u) + 0x38u) = 0;           /* 0x4AE09 */
        DSW(DSD(entry + 8u) + 0x34u) = 0;           /* 0x4AE12 */
        DSW(DSD(entry + 8u) + 0x36u) = 0;           /* 0x4AE1B */
        if (DSB(DS_00104B16) == DSB(entry + 0x21u)) /* 0x4AE26 */
            (void)fight_4b3f0(entry, index, 1u);    /* 0x4AE32 */
        else
            (void)fight_4b430(entry, index, 1u);    /* 0x4AE48 */
        return;
    }
    actors_anim_begin(DSD(entry + 8u), DSD(DS_000C95EC + index * 4u),
                      0x40400000u);                 /* 0x4AE66 */
    DSW(rec + 0x38u) = 0x0040u;                     /* 0x4AE6B */
    if ((DSW(DSD(DSD(entry + 0xCu)) + 0x28u) & 0x4000u) != 0u) /* 0x4AE84 */
        DSW(rec + 0x34u) = 0xFFC0u;                 /* 0x4AE86 */
    else
        DSW(rec + 0x34u) = 0x0040u;                 /* 0x4AE8E */
    DSB(entry + 0x1Eu) = 5u;                        /* 0x4AE97 */
    DSB(entry + 0x1Cu) = (u8)(DSB(entry + 0x1Cu) & 0x3Fu); /* 0x4AE9B/0x4AE9E */
}

/* 0x4B144. The type-0 entry's distance resolution: it walks the entry's actor
 * toward DS_00108874 (the midpoint), drawing rng(2)/rng(0x1200) 1-3 times on
 * the way, then sets the entry's +0x14 and type 1 and retargets the actor at
 * the 0xC95D4[index] stream. The sign/`0x600`/`0x1800` arms are the raw's
 * 0x4B164..0x4B24D. */
static void fight_4b144(u32 entry, u32 index)
{
    u32 rec = DSD(DSD(entry + 0xCu));               /* 0x4B14C */
    s32 pos = fight_2be00(rec);                     /* 0x4B151 */
    s32 mid = (s32)DSD(DS_00108874);                /* 0x4B156 */
    s32 ecx = pos - 0x600;                          /* 0x4B15E */
    s32 result;
    if ((u32)pos < (u32)mid) {                      /* 0x4B166 JNC */
        if (mid > 0x2A00) {                         /* 0x4B172 JLE */
            if (rng_next(2u) != 0u) {               /* 0x4B179 */
                ecx -= (s32)rng_next(0x1200u);      /* 0x4B187 */
                if ((u32)ecx > (u32)mid)            /* 0x4B196 JBE */
                    result = mid - (s32)rng_next(0x1200u);   /* 0x4B1A1 */
                else
                    result = ecx;
                goto lab;
            }
        }
        result = (pos + 0x600) + (s32)rng_next(0x1200u);    /* 0x4B1C0 */
        if ((u32)result > (u32)mid)                 /* 0x4B1CD JBE */
            result = mid - (s32)rng_next(0x1200u);  /* 0x4B1D8 */
    } else {
        if (mid < 0x2A00) {                         /* 0x4B1F2 JGE */
            if (rng_next(2u) != 0u) {               /* 0x4B1F9 */
                ecx -= (s32)rng_next(0x1200u);      /* 0x4B207 */
                if ((u32)ecx < (u32)mid)            /* 0x4B218 JNC */
                    result = mid + (s32)rng_next(0x1200u);   /* 0x4B21F */
                else
                    result = ecx;
                goto lab;
            }
        }
        result = (pos - 0x600) - (s32)rng_next(0x1200u);    /* 0x4B237 */
        if ((u32)result < (u32)mid)                 /* 0x4B246 JNC */
            result = mid + (s32)rng_next(0x1200u);  /* 0x4B24D */
    }
lab:
    {
        u32 actor = DSD(entry + 8u);                /* 0x4B258 */
        DSD(entry + 0x14u) = (u32)fight_2be4c(actor, result);   /* 0x4B268 */
        DSB(entry + 0x1Eu) = 1u;                    /* 0x4B264 */
        if ((s32)DSD(actor + 0x18u) < (s32)DSD(entry + 0x14u)) {    /* 0x4B271 */
            DSW(actor + 0x34u) = 0x0080u;           /* 0x4B276 */
            DSB(actor + 0x29u) &= (u8)~0x40u;       /* 0x4B27F */
        } else {
            DSW(actor + 0x34u) = 0xFF80u;           /* 0x4B285 */
            DSB(actor + 0x29u) |= 0x40u;            /* 0x4B28E */
        }
        actors_anim_begin(actor, DSD(DS_000C95D4 + index * 4u), 0x40400000u); /* 0x4B2A1 */
    }
}

/* 0x4AAD0. The type-0 (and >0xE) fight-effect handler. It gates on the slot's
 * +0x54/+0x42, DS_001088C2, 0x4B5A8 and the 0x1088B2/0x10889E tables, then
 * takes the 0x2BE00/0x2BE1C distance test into 0x4B144 (the four draws the
 * state-7 entry was missing). `entry` is EAX/ECX, `index` is EDX/ESI
 * (0x49D1E/0x4AAD5/0x4AAD7). */
static void fight_4aad0(u32 entry, u32 index)
{
    if (DSW(DS_00104B00) == 9u) {                   /* 0x4AAE1 */
        DSB(entry + 0x1Eu) = 9u;                    /* 0x4AAE6 */
        return;
    }
    u32 slot = DSD(entry + 0xCu);                   /* 0x4AAEF */
    if (DSB(slot + 0x54u) == 3u) {                  /* 0x4AAF2 */
        if (fight_4b430(entry, index, 0u) != 0) return; /* 0x4AAFE */
    }
    if (DSB(slot + 0x54u) == 4u) {                  /* 0x4AB0E */
        if (fight_4b3f0(entry, index, 0u) != 0) return; /* 0x4AB1A */
    }
    if (DSB(DS_001088C2) != 0u) {                   /* 0x4AB27 */
        if (fight_4bd4c(entry, index) != 0) return; /* 0x4AB34 */
    }
    if (fight_4b5a8(entry, index) != 0) return;     /* 0x4AB45 */
    if ((DSB(slot + 0x42u) & 2u) != 0u
            || DSB(DS_001088B2 + (u32)DSB(entry + 0x21u)) != 0u) {
        if (fight_4b3f0(entry, index, 0u) != 0) return; /* 0x4AB6F */
    }
    if ((DSB(slot + 0x42u) & 1u) != 0u
            || DSB(DS_0010889E + (u32)DSB(entry + 0x21u)) != 0u) {
        if (fight_4b430(entry, index, 0u) != 0) return; /* 0x4AB99 */
    }
    {
        s32 d = fight_2be1c(DSD(entry + 8u), DSD(slot));    /* 0x4ABAE */
        s32 e_actor = (s32)DSD(DS_00108874) - fight_2be00(DSD(entry + 8u));
        s32 e_rec = (s32)DSD(DS_00108874) - fight_2be00(DSD(slot));
        s32 ad = (d < 0) ? -d : d;
        if (ad >= 0x1800 || ad <= 0x600                     /* 0x4ABE1/0x4ABEF */
                || (e_actor > 0 && e_rec < 0)               /* 0x4ABFD */
                || (e_actor < 0 && e_rec > 0))              /* 0x4ABF7 */
            fight_4b144(entry, index);                      /* 0x4AC0B */
    }
}

/* 0x4A634. The effects pass's per-frame flag reset, called unconditionally at
 * 0x4A591. For each side (EDX = side, EBX = side * 0x94): when the slot's +0x42
 * bit 1 is set, the side's 0x1088A8 reaction byte picks the crowd voice, which
 * draws rng(3) for 0x20..0x3F, or rng(2) and, when that is non-zero, rng(2)
 * again for 0x10..0x17. Then it clears +0x42 bits 0/1 (`and al,0xfc`) and the
 * side's 0x10889E/0x1088B2 bytes (0x4A6EE/0x4A6F4 store at +0x10889D/+0x1088B1
 * after `inc edx`). */
static void fight_4a634(void)
{
    u32 side;
    for (side = 0; side < 2u; side++) {                     /* 0x4A6FA */
        u32 slot = DS_001077B0 + side * 0x94u;              /* 0x4A6E8 */
        if ((DSB(slot + 0x42u) & 2u) != 0u) {               /* 0x4A640 */
            u32 r = (u32)DSB(DS_001088A8 + side);           /* 0x4A64F */
            if (r >= 0x20u && r <= 0x3Fu) {                 /* 0x4A655/0x4A65A */
                (void)rng_next(3u);                         /* 0x4A664 */
                /* PORT: 0x4A6D2 0x2C3FC(0xCD/0xCE/0xCF by the draw) voice, not wired
                 * (record §45-A). */
            } else if (r >= 0x10u && r <= 0x17u) {          /* 0x4A692/0x4A697 */
                if (rng_next(2u) != 0u) {                   /* 0x4A69E */
                    (void)rng_next(2u);                     /* 0x4A6A9 */
                    /* PORT: 0x4A6B7/0x4A6C8 0x2C3FC(0xC9 or 0xCA) and 0x4A6D2
                     * 0x2C3FC(0xDA or 0xDB) voices, not wired (record §45-A). */
                }
            }
        }
        DSB(slot + 0x42u) &= 0xFCu;                         /* 0x4A6DD/0x4A6E0 */
        DSB(DS_0010889E + side) = 0;                        /* 0x4A6EE */
        DSB(DS_001088B2 + side) = 0;                        /* 0x4A6F4 */
    }
}

/* The trample tables of 0x4B470 and the grab-move byte of 0x4B788, all DAT_ in
 * Ghidra (no DS_ name from gen_symbols.py): 0xC9604[si] the tumble stream
 * (0x4B4A6 `mov edx,[ebx + 0xc9604]`), 0xBB920[si] the shadow actor's descriptor
 * (0x4B4CA `mov eax,[ebx + 0xbb920]`), 0xC97F2[ch] the fighter's grab move
 * (0x4B7EF `cmp al,[esi + 0xc97f2]`), and the byte 0x1088F2 0x4B564 reads as
 * `mov eax,[0x1088ef]` / `sar eax,0x18`. */
#define DS_000C9604 0x000C9604u
#define DS_000BB920 0x000BB920u
#define DS_000C97F2 0x000C97F2u
#define DS_001088EF 0x001088EFu

/* The grab arms' tables (record §42-C), all DAT_/PTR_ in Ghidra (no DS_ name
 * from gen_symbols.py): the worshipper's placement offsets, read as the top
 * byte of the unaligned dwords 0xC977D/0xC977E + ch * 2 (0x4B896/0x4B8D1 then
 * `sar eax,0x18`), the per-character held-stream tables 0xC97AC[ch][si]
 * (0x4B92E/0x4B938), the fighter's hold stream 0xC9790[ch] (0x4B95B) and its
 * hold 0xC97C8[ch] (0x4B964 `push`); 0x104B1A is the mode-0x22 side byte
 * (camera.c's CAMERA_MODE22_SIDE) whose slot +0x5B 0x4D898 feeds. */
#define DS_000C977D 0x000C977Du
#define DS_000C977E 0x000C977Eu
#define DS_000C97AC 0x000C97ACu
#define DS_000C9790 0x000C9790u
#define DS_000C97C8 0x000C97C8u
#define DS_00104B1A 0x00104B1Au

/* 0x4B788 — demo-pose record §29, §42-C. Fighter side `hit - 1` touching the
 * entry: 1 (the caller tramples it) when DS_00105B3A > 1, when the other side's
 * slot +0x54 is 3, or when the side's slot +0x5F is not its character's grab
 * move 0xC97F2[ch]; else 0 — the grab arm, which returns 0 without grabbing
 * when the entry is already held (+0x1C bit 6), the fighter record's +0x52 is
 * out of [0xC97E4[ch], 0xC97EB[ch]] (signed bytes) or its +0x4B is set, and
 * otherwise grabs: +0x1C bit 6, the shadow killed, the worshipper turned to
 * the fighter's facing and placed at its (0xC977D, 0xC977E)[ch] offsets (x
 * mirrored by the facing, << 6), stopped, type 8 on the 0xC97AC[ch][si] stream
 * at hold 0, linked to the fighter (+0x4A, 0x2BD20), and the fighter put on
 * 0xC9790[ch] at 0xC97C8[ch]. EAX = hit (1/2), EDX = entry, EBX = si. */
static int fight_4b788(u32 hit, u32 entry, u32 index)
{
    if ((u32)DSB(DS_00105B3A) > 1u) return 1;                   /* 0x4B7A0 */
    u32 s = hit - 1u;                                           /* 0x4B7A9 */
    u32 slot = DS_001077B0 + s * 0x94u;                         /* 0x4B7C4 */
    if (DSB(DS_001077B0 + (s ^ 1u) * 0x94u + 0x54u) == 3u)      /* 0x4B7C0/0x4B7DC */
        return 1;
    s32 ch = (s32)(s8)DSB(slot + 0x7Au);                        /* 0x4B7E9 */
    if (DSB(slot + 0x5Fu) != DSB(DS_000C97F2 + (u32)ch))        /* 0x4B7F5 */
        return 1;
    if ((DSB(entry + 0x1Cu) & 0x40u) != 0u) return 0;           /* 0x4B810 */
    u32 fr = DSD(slot);                                         /* 0x4B816 */
    s8 st = (s8)DSB(fr + 0x52u);
    if (st < (s8)DSB(DS_000C97E4 + (u32)ch)) return 0;          /* 0x4B821 */
    if (st > (s8)DSB(DS_000C97EB + (u32)ch)) return 0;          /* 0x4B82D */
    if (DSB(fr + 0x4Bu) != 0u) return 0;                        /* 0x4B837 */
    DSB(entry + 0x1Cu) = (u8)(DSB(entry + 0x1Cu) | 0x40u);      /* 0x4B845 */
    if (DSD(entry + 0x10u) != 0u) {                             /* 0x4B84A */
        actor_set_dead(DSD(entry + 0x10u));                     /* 0x4B84E 0x2B150 */
        DSD(entry + 0x10u) = 0;                                 /* 0x4B853 */
    }
    int unflipped = (DSW(DSD(slot) + 0x28u) & 0x4000u) == 0u;   /* 0x4B86A: DH */
    if (!unflipped)
        DSB(DSD(entry + 8u) + 0x29u) |= 0x40u;                  /* 0x4B871 */
    else
        DSB(DSD(entry + 8u) + 0x29u) &= 0xBFu;                  /* 0x4B87C */
    DSB(DSD(entry + 8u) + 0x29u) &= 0xEFu;                      /* 0x4B883 */
    DSB(entry + 0x20u) = (u8)(hit - 1u);                        /* 0x4B88C */
    {
        s32 ox = ((s32)DSD(DS_000C977D + (u32)ch * 2u) >> 24) * 64; /* 0x4B896..0x4B8A5 */
        u32 fx = DSD(DSD(slot) + 0x18u);
        DSD(DSD(entry + 8u) + 0x18u) = unflipped ? fx - (u32)ox   /* 0x4B8A8 */
                                                 : (u32)ox + fx;  /* 0x4B8C9 */
    }
    {
        s32 oy = ((s32)DSD(DS_000C977E + (u32)ch * 2u) >> 24) * 64; /* 0x4B8D1..0x4B8E0 */
        DSD(DSD(entry + 8u) + 0x1Cu) = (u32)oy + DSD(DSD(slot) + 0x1Cu); /* 0x4B8E8 */
    }
    DSW(DSD(entry + 8u) + 0x32u) = DSW(DSD(slot) + 0x32u);      /* 0x4B8F4 */
    DSW(DSD(entry + 8u) + 0x2Cu) =
        fight_dust_clamp((s32)DSD(DSD(entry + 8u) + 0x30u) >> 16); /* 0x4B901 0x496AC */
    DSW(DSD(entry + 8u) + 0x38u) = 0;                           /* 0x4B910 */
    DSW(DSD(entry + 8u) + 0x36u) = DSW(DSD(entry + 8u) + 0x38u); /* 0x4B91D */
    DSW(DSD(entry + 8u) + 0x34u) = DSW(DSD(entry + 8u) + 0x36u); /* 0x4B926 */
    DSB(entry + 0x1Eu) = 8u;                                    /* 0x4B92A */
    actors_anim_begin(DSD(entry + 8u),
                      DSD(DSD(DS_000C97AC + (u32)ch * 4u) + index * 4u), 0u); /* 0x4B93D */
    DSB(DSD(entry + 8u) + 0x4Au) = DSB(DSD(slot) + 0x56u);      /* 0x4B94A */
    (void)actors_link_held(DSD(entry + 8u), DSW(DSD(entry + 8u) + 0x56u)); /* 0x4B956 */
    actors_anim_begin(DSD(slot), DSD(DS_000C9790 + (u32)ch * 4u),
                      DSD(DS_000C97C8 + (u32)ch * 4u));         /* 0x4B96B */
    /* PORT: 0x4B98F 0x2C3FC(0xD4 for (u8)+0x48 - 0x20 < 3, else 0xD5) — voice,
     * not wired (record §45-A). */
    return 0;                                                   /* 0x4B994 */
}

/* 0x13134 — demo-pose record §42-C. 1 when both fighter records' x (+0x18 of
 * DS_001077B0's and DS_00107844's record) are below -0x3300, or both above
 * 0x3300 (signed); else 0. Its only caller is 0x4BD98 (0x4BDBA). */
static int fight_13134(void)
{
    s32 x0 = (s32)DSD(DSD(DS_001077B0) + 0x18u);
    s32 x1 = (s32)DSD(DSD(DS_00107844) + 0x18u);
    if (x0 < -0x3300 && x1 < -0x3300) return 1;                 /* 0x13140/0x1314E */
    if (x0 > 0x3300 && x1 > 0x3300) return 1;                   /* 0x13162/0x13170 */
    return 0;                                                   /* 0x13178 */
}

/* 0x4BD98 — demo-pose record §42-C. The eighth-hit start, EAX = entry: unless
 * DS_00104AD8 > 0 (signed), DS_00104ABC < 2 (unsigned) or 0x13134, the entry
 * becomes DS_00108864 as type 6 with +0x1C bit 5, the mode becomes 0x21 with
 * DS_001088C1/DS_001088C5 = 1 and DS_001088AA = 0x1E, the 0x1088A0/0x108898/
 * 0x1088AC words and 0x10889C/0x10889D bytes clear, DS_00104AEC loses bit 0,
 * and two actors are spawned from 0xBAB88/0xBAB9C at x = the fighters' +0x34
 * midpoint (DS_001077E4, DS_00107878) less DS_00108854, depth DS_00108880 -
 * 0x3140, layer word DS_000BD898 (into DS_00108868/DS_0010886C); the second
 * takes the 0xEF66A stream at 1.0, and both get +0x36 = 0x1A4. */
static void fight_4bd98(u32 entry)
{
    if ((s32)DSD(DS_00104AD8) > 0) return;                      /* 0x4BDA7 */
    if (DSD(DS_00104ABC) < 2u) return;                          /* 0x4BDB4 */
    if (fight_13134() != 0) return;                             /* 0x4BDC1 */
    DSD(DS_00108864) = entry;                                   /* 0x4BDD3 */
    DSB(entry + 0x1Eu) = 6u;                                    /* 0x4BDD9 */
    DSB(DS_001088C1) = 1u;                                      /* 0x4BDE1 */
    DSW(DS_00104B00) = 0x21u;                                   /* 0x4BDE7 */
    DSB(DS_001088C5) = 1u;                                      /* 0x4BDEE */
    DSW(DS_001088AA) = 0x1Eu;                                   /* 0x4BDF4 */
    DSW(DS_001088A0) = 0;                                       /* 0x4BDFB */
    DSW(DS_00108898) = 0;                                       /* 0x4BE02 */
    DSB(DS_0010889D) = 0;                                       /* 0x4BE19 */
    DSB(DS_0010889C) = 0;                                       /* 0x4BE21 */
    DSB(DS_00104AEC) = (u8)(DSB(DS_00104AEC) & 0xFEu);          /* 0x4BE27 */
    DSW(DS_001088AC) = 0;                                       /* 0x4BE31 */
    DSB(entry + 0x1Cu) = (u8)(DSB(entry + 0x1Cu) | 0x20u);      /* 0x4BE3C */
    {
        /* 0x4BE3F `jge`: the signed compare; the `sar` halves the difference. */
        s32 a = (s32)DSD(DS_001077E4), b = (s32)DSD(DS_00107878);
        u32 mid = (a < b)
            ? (u32)a + (u32)((s32)((u32)b - (u32)a) >> 1)       /* 0x4BE43..0x4BE4F */
            : (u32)b + (u32)((s32)((u32)a - (u32)b) >> 1);      /* 0x4BE53..0x4BE59 */
        DSD(DS_00108884) = mid;                                 /* 0x4BE5C */
        DSD(DS_00108884) = mid - DSD(DS_00108854);              /* 0x4BE8A */
    }
    DSD(DS_00108868) = actor_spawn((const u32 *)(mem + 0x000BAB88u),
                                   DSD(DS_00108884), (u32)DSW(DS_000BD898),
                                   DSD(DS_00108880) - 0x3140u, 0u); /* 0x4BE90/0x4BEA3 */
    u32 b2 = actor_spawn((const u32 *)(mem + 0x000BAB9Cu),
                         DSD(DS_00108884), (u32)DSW(DS_000BD898),
                         DSD(DS_00108880) - 0x3140u, 0u);       /* 0x4BEBC */
    DSD(DS_0010886C) = b2;                                      /* 0x4BECB */
    actors_anim_begin(b2, 0x000EF66Au, 0x3F800000u);            /* 0x4BED0 */
    DSW(DSD(DS_00108868) + 0x36u) = 0x01A4u;                    /* 0x4BEDA */
    DSW(DSD(DS_0010886C) + 0x36u) = 0x01A4u;                    /* 0x4BEE5 */
}

/* 0x4CB18 — demo-pose record §42-C. The launch: the entry's actor takes the
 * 0xC9604[si] tumble stream at 3.0; +0x34 = the fighters' +0x34 distance
 * (|DS_001077E4 - DS_00107878|, at most 0x3F00), negated when 0x1A570(side)
 * holds, over 0x38 (flag) or 0x70; +0x36 = 0 (flag) or (0x3BC0 - the actor's
 * +0x1C) / 0x16 (signed `idiv`s); type 6 and +0x1C bit 7 cleared. EAX = entry,
 * EDX = si, EBX = flag, ECX = side. Callers: 0x4B470 (0x4B59E, flag 0, side
 * +0x20) and 0x4C60C (0x4C760 flag 1, 0x4C774 flag 0; record §43-A). */
void fight_4cb18(u32 entry, u32 index, u32 flag, u32 side)
{
    /* PORT: 0x4CB3E 0x2C3FC(0xD1 for (u8)+0x48 - 0x20 < 3, else 0xD0) — voice,
     * not wired (record §45-A). */
    actors_anim_begin(DSD(entry + 8u), DSD(DS_000C9604 + index * 4u),
                      0x40400000u);                             /* 0x4CB52 */
    s32 d = (s32)(DSD(DS_001077E4) - DSD(DS_00107878));         /* 0x4CB63 */
    if (d < 0) d = (s32)(0u - (u32)d);                          /* 0x4CB6B */
    if (d > 0x3F00) d = 0x3F00;                                 /* 0x4CB75 */
    s32 h = (s32)(0x3BC0u - DSD(DSD(entry + 8u) + 0x1Cu));      /* 0x4CB84/0x4CB90 */
    if (fighter_actor_bit15_clear(side) != 0)                   /* 0x4CB8B 0x1A570 */
        d = (s32)(0u - (u32)d);                                 /* 0x4CB9A/0x4CBA3 */
    DSW(DSD(entry + 8u) + 0x34u) =
        (u16)(d / (flag != 0u ? 0x38 : 0x70));                  /* 0x4CBB5/0x4CBCC */
    if (flag != 0u)
        DSW(DSD(entry + 8u) + 0x36u) = 0;                       /* 0x4CBDC */
    else
        DSW(DSD(entry + 8u) + 0x36u) = (u16)(h / 0x16);         /* 0x4CBF0 */
    DSB(entry + 0x1Eu) = 6u;                                    /* 0x4CBFE */
    DSB(entry + 0x1Cu) = (u8)(DSB(entry + 0x1Cu) & 0x7Fu);      /* 0x4CC05 */
}

/* 0x4AF04 — demo-pose record §42-C. Whether side `side`'s fighter still plays
 * its grab hold: 1 when the slot's character (+0x7A, unsigned) is above 6, or
 * when the fighter record's stream cursor (+8) lies in the character's range
 * (jump table 0x4AEE8; each range starts at that character's 0xC9790 hold
 * stream); else 0. EAX = side (AL, zero-extended at 0x4AF09). */
static int fight_4af04(u32 side)
{
    u32 slot = DS_001077B0 + (side & 0xFFu) * 0x94u;           /* 0x4AF0B..0x4AF19 */
    u8 ch = DSB(slot + 0x7Au);                                  /* 0x4AF1C */
    u32 lo, hi;
    if (ch > 6u) return 1;                                      /* 0x4AF25 */
    switch (ch) {                                               /* 0x4AF31 */
    case 0:  lo = 0x000E7B02u; hi = 0x000E7B50u; break;         /* 0x4AF39 */
    case 1:  lo = 0x000E4744u; hi = 0x000E47FCu; break;         /* 0x4AF7F */
    case 2:  lo = 0x000ED7AEu; hi = 0x000ED80Cu; break;         /* 0x4AFC1 */
    case 3:  lo = 0x000D2DECu; hi = 0x000D2E06u; break;         /* 0x4AFE0 */
    case 4:  lo = 0x000EB38Au; hi = 0x000EB3E4u; break;         /* 0x4AFFF */
    case 5:  lo = 0x000D4A3Cu; hi = 0x000D4A8Au; break;         /* 0x4AF5C */
    default: lo = 0x000E137Au; hi = 0x000E1432u; break;         /* 0x4AFA2 */
    }
    u32 cur = DSD(DSD(slot) + 8u);
    if (cur < lo || cur > hi) return 0;                         /* `jc`/`jbe` */
    return 1;                                                   /* 0x4B01E */
}

/* 0x4B470 — demo-pose record §29, §42-C. The trample: the entry's actor takes the
 * 0xC9604[si] tumble stream at 3.0, gets a shadow actor (0x2AE14 from
 * 0xBB920[si] at its x and y) when +0x10 has none, is thrown (+0x34 = ±0x80, away
 * from the hitter on the first hit (0x1A570 of the +0x20 side), else reversed
 * from its current +0x34; not in mode 0x22) with +0x36 = 0x240, and the entry
 * becomes type 6 with +0x1C bit 7 cleared. Then the eighth-hit tail: with
 * DS_00104B1D not 2/3, DS_00104AFC, DS_001088C1, DS_001088C5 and DS_00108864
 * clear, [0x1088EF] >> 24 > 1 and +0x1F > 7, 0x4BD98 then 0x4CB18 (the
 * launch). EAX = entry, EDX = si. */
static void fight_4b470(u32 entry, u32 index)
{
    /* PORT: 0x4B497 0x2C3FC(0xD1 for si < 3, else 0xD0) — voice, not wired
     * (record §45-A). */
    u32 rec = DSD(entry + 8u);
    actors_anim_begin(rec, DSD(DS_000C9604 + index * 4u), 0x40400000u); /* 0x4B4B1 */
    if (DSD(entry + 0x10u) == 0u) {                             /* 0x4B4BB */
        u32 sh = actor_spawn((const u32 *)(mem + DSD(DS_000BB920 + index * 4u)),
                             DSD(rec + 0x18u),
                             (u32)((s32)DSD(rec + 0x30u) >> 16), 0u, 0u); /* 0x4B4D2 */
        DSD(entry + 0x10u) = sh;                                /* 0x4B4D7 */
    }
    if (DSW(DS_00104B00) != 0x22u) {                            /* 0x4B4E5 */
        if (DSB(entry + 0x1Fu) == 1u) {                         /* 0x4B4EF */
            if (fighter_actor_bit15_clear((u32)DSB(entry + 0x20u)) != 0) /* 0x4B4F6 */
                DSW(rec + 0x34u) = 0xFF80u;                     /* 0x4B525 */
            else
                DSW(rec + 0x34u) = 0x0080u;                     /* 0x4B507 */
        } else if (((s32)DSD(rec + 0x32u) >> 16) == -0x80) {    /* 0x4B518 */
            DSW(rec + 0x34u) = 0x0080u;                         /* 0x4B51D */
        } else {
            DSW(rec + 0x34u) = 0xFF80u;                         /* 0x4B525 */
        }
    }
    DSW(rec + 0x36u) = 0x0240u;                                 /* 0x4B52E */
    DSB(entry + 0x1Eu) = 6u;                                    /* 0x4B537 */
    DSB(entry + 0x1Cu) = (u8)(DSB(entry + 0x1Cu) & 0x7Fu);      /* 0x4B544 */
    if (DSB(DS_00104B1D) == 3u || DSB(DS_00104B1D) == 2u) return;  /* 0x4B547/0x4B54C */
    if (DSW(DS_00104AFC) != 0u) return;                         /* 0x4B559 */
    if (DSB(DS_001088C1) != 0u) return;                         /* 0x4B562 */
    if (((s32)DSD(DS_001088EF) >> 24) <= 1) return;             /* 0x4B56F */
    if ((u32)DSB(entry + 0x1Fu) <= 7u) return;                  /* 0x4B579 */
    if (DSB(DS_001088C5) != 0u) return;                         /* 0x4B582 */
    if (DSD(DS_00108864) != 0u) return;                         /* 0x4B58C */
    /* The eighth hit (§42-C): 0x4BD98, then 0x4CB18 whether or not 0x4BD98
     * passed its gates. EBX is 0x4B584's DS_00108864, zero here (0x4BD98
     * pushes and pops it); ECX is the +0x20 side byte (0x4B597/0x4B59B). */
    fight_4bd98(entry);                                         /* 0x4B590 */
    fight_4cb18(entry, index, 0u, (u32)DSB(entry + 0x20u));     /* 0x4B59E */
}

/* 0x4B69C — demo-pose record §29. The effects pass's per-entry prelude
 * (0x49CFE, before the type dispatch; EAX = entry, EDX = si): an entry whose
 * +0x1C bit 7 is set (a lying or falling worshipper) tests its actor's pset
 * point (the words at pset +4/+8) against both fighters (0x17D30, BX = 0).
 * Both sides hit counts as side 0. When 0x4B788 lets it through, +0x20 = the
 * hitter side, +0x1F counts the hit, a held actor (+0x4A) is released, and
 * 0x4B470 tramples it. */
static void fight_4b69c(u32 entry, u32 index)
{
    if ((DSB(entry + 0x1Cu) & 0x80u) == 0u) return;              /* 0x4B6B3 */
    u32 rec = DSD(entry + 8u);
    u32 ps = DSD(DS_001014EC) + ((u32)DSW(rec + 0x56u) << 5);  /* 0x4B6BC..0x4B6CD */
    u32 hit = camera_point_hit((s32)(s16)DSW(ps + 4u),
                               (s32)(s16)DSW(ps + 8u), 0u);     /* 0x4B6E2 */
    if (hit == 0u) return;                                      /* 0x4B6EC */
    if ((s32)hit > 2) hit = 1u;                                 /* 0x4B6F5 */
    if (fight_4b788(hit, entry, index) == 0) return;            /* 0x4B705 */
    DSB(entry + 0x20u) = (u8)(hit - 1u);                        /* 0x4B713 */
    DSB(entry + 0x1Fu) = (u8)(DSB(entry + 0x1Fu) + 1u);         /* 0x4B716 */
    u32 k = DSB(rec + 0x4Au);
    if (k != 0u) {                                              /* 0x4B720 */
        DSB(DSD(DS_001014F4) + k * 0x68u + 0x4Bu) = 0;          /* 0x4B73B */
        DSB(rec + 0x2Au) = (u8)(DSB(rec + 0x2Au) & 0xF7u);      /* 0x4B743 */
        DSB(rec + 0x29u) = (u8)(DSB(rec + 0x29u) & 0xBFu);      /* 0x4B74A */
        DSB(rec + 0x4Au) = 0;                                   /* 0x4B751 */
        DSB(entry + 0x1Cu) = (u8)(DSB(entry + 0x1Cu) & 0xBFu);  /* 0x4B760 */
        DSB(DS_001088AE + (u32)DSB(entry + 0x21u)) =
            (u8)(DSB(DS_001088AE + (u32)DSB(entry + 0x21u)) + 1u); /* 0x4B763 */
        DSB(DS_001088B2 + (u32)DSB(entry + 0x21u)) = 1u;        /* 0x4B76E */
    }
    fight_4b470(entry, index);                                  /* 0x4B779 */
}

/* The bytes 0x1088C7/0x1088C8/0x1088C9 the mode-9 worshipper types read (the
 * last as `mov al,[0x1088c9]` in 0x4B2AC, and as the high byte of the dword
 * DS_001088C6 elsewhere); symbols.h has no name for them. */
#define DS_001088C7 0x001088C7u
#define DS_001088C8 0x001088C8u
#define DS_001088C9 0x001088C9u

/* 0x4A7D4 — demo-pose record §42-D. The type-11 arrival test (its one caller is
 * 0x4A196): 1 when |actor+0x18 - entry+0x14| <= |2 * the actor's +0x34 word|
 * (read as `[+0x32] >> 16`, doubled by `add eax,eax`), signed (0x4A7FA `setle`).
 * EAX = entry; EDX and EBX are pushed and popped. */
static int fight_4a7d4(u32 entry)
{
    u32 actor = DSD(entry + 8u);
    s32 d = (s32)(DSD(actor + 0x18u) - DSD(entry + 0x14u));    /* 0x4A7DF */
    if (d < 0) d = (s32)(0u - (u32)d);                          /* 0x4A7E5 */
    s32 v = ((s32)DSD(actor + 0x32u) >> 16) * 2;                /* 0x4A7ED/0x4A7F0 */
    if (v < 0) v = -v;                                          /* 0x4A7F6 */
    return d <= v;                                              /* 0x4A7FA */
}

/* 0x4B2AC — demo-pose record §42-D. The type-10 walk (its one caller is
 * 0x4A163; EAX = entry, EDX = si). The side s = (s8)(DS_001088C9 ^ 1) gives
 * DS_00108878 = slot[s]+0x81 - (DS_001088CC + slot[s]+0x3C / [0xC9520]),
 * capped at 7 (signed `jl`). The target x is DS_00108870 when the entry's +0x21
 * equals (s8)DS_001088C9 (zero-extended against sign-extended), else
 * DS_0010887C, plus rng(0xC00) when DS_001088C6 is non-zero. A zero target, or
 * one closer than the actor's step (the +0x34 word's magnitude), stops the
 * actor (+0x34 = 0). Otherwise +0x14 = 0x2BE4C(actor, target), the actor
 * faces and walks toward it at 0x80 and takes the 0xC95D4[si] stream at 3.0.
 * The 0x4B35A hflip clear precedes the distance test. */
static void fight_4b2ac(u32 entry, u32 index)
{
    s32 s = (s32)(s8)(u8)(DSB(DS_001088C9) ^ 1u);               /* 0x4B2B5..0x4B2BC */
    u32 slot = DS_001077B0 + (u32)s * 0x94u;                    /* 0x4B2BF..0x4B2D3 */
    u32 q = DSD(slot + 0x3Cu) / DSD(DS_000C9520);               /* 0x4B2D8/0x4B2DE */
    u32 n = (u32)DSB(slot + 0x81u) - ((u32)DSB(DS_001088CC) + q);  /* 0x4B2E0..0x4B2F2 */
    DSD(DS_00108878) = n;                                       /* 0x4B2F4 */
    if ((s32)n >= 7) n = 7u;                                    /* 0x4B2F9/0x4B2FE */
    DSD(DS_00108878) = n;                                       /* 0x4B309 */

    u32 actor = DSD(entry + 8u);                                /* 0x4B316 */
    u32 target;
    if ((s32)(u32)DSB(entry + 0x21u) == ((s32)DSD(DS_001088C6) >> 24)) { /* 0x4B319 */
        target = DSD(DS_00108870);                              /* 0x4B31D */
        if (target == 0u) {                                     /* 0x4B323 */
            DSW(actor + 0x34u) = 0;                             /* 0x4B3E2 */
            return;
        }
    } else {
        target = DSD(DS_0010887C);                              /* 0x4B32F */
        if (target == 0u) {                                     /* 0x4B335 */
            DSW(actor + 0x34u) = 0;                             /* 0x4B3E2 */
            return;
        }
        if (DSB(DS_001088C6) != 0u)                             /* 0x4B33D */
            target = rng_next(0xC00u) + DSD(DS_0010887C);       /* 0x4B34B/0x4B350 */
    }
    DSB(actor + 0x29u) &= 0xBFu;                                /* 0x4B35D */
    s32 d = (s32)(DSD(actor + 0x18u) - target);                 /* 0x4B367 */
    if (d < 0) d = (s32)(0u - (u32)d);                          /* 0x4B36F */
    s32 step = (s32)DSD(actor + 0x32u) >> 16;                   /* 0x4B37F/0x4B389 */
    if ((s16)DSW(actor + 0x34u) < 0) step = -step;              /* 0x4B378/0x4B385 */
    if (d < step) {                                             /* 0x4B38F `jl` */
        DSW(actor + 0x34u) = 0;                                 /* 0x4B3DF/0x4B3E2 */
        return;
    }
    DSD(entry + 0x14u) = (u32)fight_2be4c(actor, (s32)target); /* 0x4B39A/0x4B39F */
    if ((s32)DSD(entry + 0x14u) > (s32)DSD(actor + 0x18u)) {    /* 0x4B3A8 `jle` */
        DSW(actor + 0x34u) = 0x0080u;                           /* 0x4B3AD */
        DSB(actor + 0x29u) &= 0xBFu;                            /* 0x4B3B6 */
    } else {
        DSW(actor + 0x34u) = 0xFF80u;                           /* 0x4B3BC */
        DSB(actor + 0x29u) |= 0x40u;                            /* 0x4B3C5 */
    }
    actors_anim_begin(actor, DSD(DS_000C95D4 + index * 4u), 0x40400000u); /* 0x4B3D8 */
}

/* 0x4D898 — demo-pose record §42-C. The mode-0x22 grab arm (0x4B788's twin,
 * without its DS_00105B3A and other-side +0x54 gates): 1 (the caller
 * tramples) unless the side's slot +0x5F is 0xC97F2[ch], or 0xA for
 * characters 0, 4 and 5, or 0xB for characters 1 and 6 (ch the signed slot
 * +0x7A). An accepted move returns 0 without grabbing on +0x1C bit 6, +0x52
 * out of [0xC97E4[ch], 0xC97EB[ch]] or +0x4B set; else it grabs exactly as
 * 0x4B788 does, then feeds DS_00104B1A's slot +0x5B (capped at 0x78): by the
 * actor's (u8)+0x48 - 0x20 (jump table 0x4D880: 0x10, 0xE, 0x15, 0xC, 0xA,
 * else 0xD) times 120 / 100 for the 0xC97F2 move, 1 for the 0xA/0xB move.
 * Returns 0. EAX = hit (1/2), EDX = entry, EBX = si. */
int fight_4d898(u32 hit, u32 entry, u32 index)
{
    u32 slot = DS_001077B0 + (hit - 1u) * 0x94u;                /* 0x4D8A6..0x4D8BF */
    u8 mv = DSB(slot + 0x5Fu);                                  /* 0x4D8C8 */
    s32 ch = (s32)(s8)DSB(slot + 0x7Au);                        /* 0x4D8C1, 0x4D8CF `sar` */
    if (mv != DSB(DS_000C97F2 + (u32)ch)) {                     /* 0x4D8DA */
        int alt = 0;
        if ((ch == 0 || ch == 5 || ch == 4) && mv == 0x0Au)     /* 0x4D8E2..0x4D8F9 */
            alt = 1;
        if (!alt) {
            if (ch != 1 && ch != 6) return 1;                   /* 0x4D905/0x4D90D */
            if (mv != 0x0Bu) return 1;                          /* 0x4D91C */
        }
    }
    if ((DSB(entry + 0x1Cu) & 0x40u) != 0u) return 0;           /* 0x4D92F */
    u32 fr = DSD(slot);                                         /* 0x4D939 */
    s8 st = (s8)DSB(fr + 0x52u);
    if (st < (s8)DSB(DS_000C97E4 + (u32)ch)) return 0;          /* 0x4D947 */
    if (st > (s8)DSB(DS_000C97EB + (u32)ch)) return 0;          /* 0x4D953 */
    if (DSB(fr + 0x4Bu) != 0u) return 0;                        /* 0x4D95D */
    DSB(entry + 0x1Cu) = (u8)(DSB(entry + 0x1Cu) | 0x40u);      /* 0x4D963 */
    if (DSD(entry + 0x10u) != 0u) {                             /* 0x4D96C */
        actor_set_dead(DSD(entry + 0x10u));                     /* 0x4D970 0x2B150 */
        DSD(entry + 0x10u) = 0;                                 /* 0x4D975 */
    }
    int unflipped = (DSW(DSD(slot) + 0x28u) & 0x4000u) == 0u;   /* 0x4D98C: DL */
    if (!unflipped)
        DSB(DSD(entry + 8u) + 0x29u) |= 0x40u;                  /* 0x4D993 */
    else
        DSB(DSD(entry + 8u) + 0x29u) &= 0xBFu;                  /* 0x4D99E */
    DSB(DSD(entry + 8u) + 0x29u) &= 0xEFu;                      /* 0x4D9A5 */
    DSB(entry + 0x20u) = (u8)(hit - 1u);                        /* 0x4D9AE */
    {
        s32 ox = ((s32)DSD(DS_000C977D + (u32)ch * 2u) >> 24) * 64; /* 0x4D9BC..0x4D9CB */
        u32 fx = DSD(DSD(slot) + 0x18u);
        DSD(DSD(entry + 8u) + 0x18u) = unflipped ? fx - (u32)ox   /* 0x4D9D5 */
                                                 : (u32)ox + fx;  /* 0x4D9F8 */
    }
    {
        s32 oy = ((s32)DSD(DS_000C977E + (u32)ch * 2u) >> 24) * 64; /* 0x4DA02..0x4DA11 */
        DSD(DSD(entry + 8u) + 0x1Cu) = (u32)oy + DSD(DSD(slot) + 0x1Cu); /* 0x4DA19 */
    }
    DSW(DSD(entry + 8u) + 0x32u) = DSW(DSD(slot) + 0x32u);      /* 0x4DA25 */
    DSW(DSD(entry + 8u) + 0x2Cu) =
        fight_dust_clamp((s32)DSD(DSD(entry + 8u) + 0x30u) >> 16); /* 0x4DA32 0x496AC */
    DSW(DSD(entry + 8u) + 0x38u) = 0;                           /* 0x4DA41 */
    DSW(DSD(entry + 8u) + 0x36u) = DSW(DSD(entry + 8u) + 0x38u); /* 0x4DA4E */
    DSW(DSD(entry + 8u) + 0x34u) = DSW(DSD(entry + 8u) + 0x36u); /* 0x4DA55 */
    DSB(entry + 0x1Eu) = 8u;                                    /* 0x4DA59 */
    actors_anim_begin(DSD(entry + 8u),
                      DSD(DSD(DS_000C97AC + (u32)ch * 4u) + index * 4u), 0u); /* 0x4DA6C */
    DSB(DSD(entry + 8u) + 0x4Au) = DSB(DSD(slot) + 0x56u);      /* 0x4DA79 */
    (void)actors_link_held(DSD(entry + 8u), DSW(DSD(entry + 8u) + 0x56u)); /* 0x4DA85 */
    actors_anim_begin(DSD(slot), DSD(DS_000C9790 + (u32)ch * 4u),
                      DSD(DS_000C97C8 + (u32)ch * 4u));         /* 0x4DA9A */
    /* PORT: 0x4DABE 0x2C3FC(0xD4 for (u8)+0x48 - 0x20 < 3, else 0xD5) — voice,
     * not wired (record §45-A). */
    {
        u32 dst = DS_001077B0 + (u32)DSB(DS_00104B1A) * 0x94u;  /* 0x4DB0B/0x4DB64 */
        u32 add;
        if (mv == DSB(DS_000C97F2 + (u32)ch)) {                 /* 0x4DACE */
            u8 w;
            switch ((u8)(DSB(DSD(entry + 8u) + 0x48u) - 0x20u)) { /* 0x4DAE0 0x4D880 */
            case 0:  w = 0x10u; break;                          /* 0x4DAF3 */
            case 1:  w = 0x0Eu; break;                          /* 0x4DAF7 */
            case 2:  w = 0x15u; break;                          /* 0x4DAFB */
            case 3:  w = 0x0Cu; break;                          /* 0x4DAFF */
            case 4:  w = 0x0Au; break;                          /* 0x4DB03 */
            default: w = 0x0Du; break;                          /* 0x4DB07 */
            }
            add = (u32)w * 120u / 100u;                         /* 0x4DB2F..0x4DB45 */
        } else {
            add = 1u;                                           /* 0x4DB83 */
        }
        if ((s32)((u32)DSB(dst + 0x5Bu) + add) > 0x78)          /* 0x4DB50/0x4DB8D */
            DSB(dst + 0x5Bu) = 0x78u;                           /* 0x4DB55/0x4DB92 */
        else
            DSB(dst + 0x5Bu) = (u8)(DSB(dst + 0x5Bu) + add);    /* 0x4DB5B/0x4DB9A */
    }
    return 0;                                                   /* 0x4DB5E/0x4DB9D */
}

/* 0x4D7A4 — demo-pose record §42-C. 0x4B69C's twin in the mode-0x22 effects
 * pass 0x4D2D0 (its only caller, 0x4D323, in mode 0x22 only; record §43-A):
 * an entry with +0x1C bit 7 tests its actor's pset point against the
 * fighters (0x17D30, BX = 0), both sides counting as side 0; when 0x4D898 lets
 * it through, +0x20 = the side, +0x1F counts, a held actor (+0x4A) is released
 * (without 0x4B69C's DS_001088B2 store) and 0x4B470 tramples it. EAX = entry,
 * EDX = si. */
void fight_4d7a4(u32 entry, u32 index)
{
    if ((DSB(entry + 0x1Cu) & 0x80u) == 0u) return;              /* 0x4D7BB */
    u32 rec = DSD(entry + 8u);
    u32 ps = DSD(DS_001014EC) + ((u32)DSW(rec + 0x56u) << 5);  /* 0x4D7C1..0x4D7D3 */
    u32 hit = camera_point_hit((s32)(s16)DSW(ps + 4u),
                               (s32)(s16)DSW(ps + 8u), 0u);     /* 0x4D7E8 */
    if (hit == 0u) return;                                      /* 0x4D7F2 */
    if ((s32)hit > 2) hit = 1u;                                 /* 0x4D7FB */
    if (fight_4d898(hit, entry, index) == 0) return;            /* 0x4D80B */
    DSB(entry + 0x20u) = (u8)(hit - 1u);                        /* 0x4D819 */
    DSB(entry + 0x1Fu) = (u8)(DSB(entry + 0x1Fu) + 1u);         /* 0x4D81C */
    u32 k = DSB(DSD(entry + 8u) + 0x4Au);
    if (k != 0u) {                                              /* 0x4D826 */
        DSB(DSD(DS_001014F4) + k * 0x68u + 0x4Bu) = 0;          /* 0x4D841 */
        DSB(DSD(entry + 8u) + 0x2Au) &= 0xF7u;                  /* 0x4D849 */
        DSB(DSD(entry + 8u) + 0x29u) &= 0xBFu;                  /* 0x4D850 */
        DSB(DSD(entry + 8u) + 0x4Au) = 0;                       /* 0x4D857 */
        DSB(entry + 0x1Cu) = (u8)(DSB(entry + 0x1Cu) & 0xBFu);  /* 0x4D866 */
        DSB(DS_001088AE + (u32)DSB(entry + 0x21u)) =
            (u8)(DSB(DS_001088AE + (u32)DSB(entry + 0x21u)) + 1u); /* 0x4D869 */
    }
    fight_4b470(entry, index);                                  /* 0x4D873 */
}

/* The mode-0x22 pass's and the volleyball's tables (record §43-A), DAT_ in
 * Ghidra (no DS_ name from gen_symbols.py): 0xC9724[si] the other landing
 * stream of 0x4D2D0's case 3 (0x4D4C0 `mov edx,[edx*4 + 0xc9724]`) and the
 * descriptor 0x4C784 spawns at the eaten ball (0x4C7F3 `mov eax,0xc976c`). */
#define DS_000C9724 0x000C9724u
#define DS_000C976C 0x000C976Cu

/* 0x4987C — demo-pose record §43-A. EAX = side, EDX = count, EBX = kind.
 * `count` entries (signed; none when <= 0) move from the free fight-effect
 * list DS_001083C4 to DS_0010884C (it returns when the free list runs dry).
 * Kind 0 (0x4D2D0's refill): the dust descriptor 0xC9524[0x49388(side)] with
 * +0x10 = 0x29CDC(side, slot +0x7A). Kind 1/2 (the effects tail 0x4A616's
 * flyers): 0xC9538[kind] for one entry, else 0xC953C[i] (kind 1) or
 * 0xC953C[i ^ 1] (kind 2), the first entry's +0x1C bit 0 set. The x is 0x5780
 * to one side of the side's fighter through 0x2BE4C (in mode 0x22 the side is
 * rng(2)'s, else away from the other fighter), 0x780 further out for a pair's
 * first flyer; the y is the layer word DS_000BD898 (less 0x200 outside mode
 * 0x22) for the flyers, the fighter's y + 0x400 + rng(0x300) for kind 0.
 * The spawned actor's +0x2C is 0x496AC(y); a character-marked fighter
 * (+0x51) or mode 0x22 gives it +0x2E += 4 and +0x4E = 1. Kind 0 then walks
 * it through 0x4B144; the flyers take +0x34 = +-0xC0 (bit 0) or +-0x100
 * toward the screen, face left when moving left, and become type 7 with
 * +0x1C bit 4. */
void fight_4987c(u32 side, s32 count, u32 kind)
{
    u32 slot = DS_001077B0 + side * 0x94u;                      /* 0x4988D..0x498A7 */
    u32 other = DS_001077B0 + (side ^ 1u) * 0x94u;              /* 0x498B3..0x498CE */
    s32 i;
    if (count <= 0) return;                                     /* 0x498AB `jle` */
    for (i = 0; i < count; i++) {                               /* 0x49C1A `jl` */
        u32 entry = DSD(DS_001083C4);                           /* 0x498E3 */
        if (entry == DS_001083C4) return;                       /* 0x498E9/0x49900 */
        effects_list_unlink(entry);                             /* 0x498F7 0x249D0 */
        effects_list_insert_after(DS_0010884C, entry);          /* 0x4990D 0x249B0 */
        DSW(entry + 0x1Cu) = 0;                                 /* 0x49912 */
        u32 desc;
        if (kind != 0u) {
            if (count == 1) {                                   /* 0x4991C */
                desc = DSD(DS_000C9538 + kind * 4u);            /* 0x49927 */
            } else {
                if (kind == 1u)                                 /* 0x49933 */
                    desc = DSD(DS_000C953C + (u32)i * 4u);      /* 0x4993C */
                else
                    desc = DSD(DS_000C953C + ((u32)i ^ 1u) * 4u); /* 0x49948 */
                if (i == 0)                                     /* 0x49953 */
                    DSB(entry + 0x1Cu) = (u8)(DSB(entry + 0x1Cu) | 1u); /* 0x49957 */
            }
        } else {
            desc = DSD(DS_000C9524 + fight_dust_pick(side) * 4u);   /* 0x49961/0x49966 */
            DSD(desc + 0x10u) =
                fight_dust_value(side, (u32)DSB(slot + 0x7Au));    /* 0x4997E/0x49987 */
        }

        u32 srec = DSD(slot);                                   /* 0x4999F */
        int pair = (kind != 0u && count == 2 && i == 0);        /* 0x499D1..0x499DE */
        s32 x;
        int flag;                                               /* [ESP+0x28] */
        if (DSW(DS_00104B00) == 0x22u) {                        /* 0x49992 */
            s32 fx = fight_2be00(srec);                         /* 0x499A5 */
            if (rng_next(2u) != 0u) {                           /* 0x499B1 */
                x = fight_2be4c(srec, (s32)((u32)fx - 0x5780u));        /* 0x499BE/0x499CA */
                if (pair) x = (s32)((u32)x - 0x780u);           /* 0x499E0 */
                flag = 1;                                       /* 0x499E6 */
            } else {
                x = fight_2be4c(srec, (s32)((u32)fx + 0x5780u));        /* 0x499F7/0x49A03 */
                if (pair) x = (s32)((u32)x + 0x780u);           /* 0x49A25 */
                flag = 0;                                       /* 0x49AB8 */
            }
        } else {
            s32 ox = fight_2be00(DSD(other));                   /* 0x49A3A */
            s32 fx = fight_2be00(srec);                         /* 0x49A4B */
            if (fx < ox) {                                      /* 0x49A52 `jge` */
                x = fight_2be4c(srec, (s32)((u32)fx - 0x5780u));        /* 0x49A54/0x49A64 */
                if (pair) x = (s32)((u32)x - 0x780u);           /* 0x49A7A */
                flag = 1;                                       /* 0x49A80 */
            } else {
                x = fight_2be4c(srec, (s32)((u32)fx + 0x5780u));        /* 0x49A8A/0x49A9A */
                if (pair) x = (s32)((u32)x + 0x780u);           /* 0x49AB0 */
                flag = 0;                                       /* 0x49AB8 */
            }
        }

        s32 y;                                                  /* [ESP+0x20] */
        if (kind != 0u) {
            if (DSW(DS_00104B00) == 0x22u)                      /* 0x49AC8 */
                y = (s32)(u32)DSW(DS_000BD898);                 /* 0x49AE2 */
            else
                y = (s32)(u32)DSW(DS_000BD898) - 0x200;         /* 0x49ACF/0x49AD5 */
        } else {
            y = ((s32)DSD(srec + 0x30u) >> 16) + 0x400
                + (s32)rng_next(0x300u);                        /* 0x49AF7..0x49B0D */
        }

        u32 actor = actor_spawn((const u32 *)(mem + desc), (u32)x, (u32)y,
                                0u, 0u);                        /* 0x49B1F 0x2AE14 */
        DSD(entry + 8u) = actor;                                /* 0x49B26 */
        u32 index = (u32)(u16)((u32)DSB(actor + 0x48u) - 0x20u);    /* 0x49B29/0x49B69 */
        DSD(actor + 0x14u) = entry;                             /* 0x49B2C */
        DSD(entry + 0xCu) = slot;                               /* 0x49B33 */
        DSB(entry + 0x21u) = (u8)side;                          /* 0x49B3C */
        DSB(entry + 0x1Fu) = 0;                                 /* 0x49B43 */
        DSW(actor + 0x2Cu) = fight_dust_clamp(y);               /* 0x49B46/0x49B4E */
        DSB(actor + 0x29u) = (u8)(DSB(actor + 0x29u) | 0x10u);  /* 0x49B55 */
        DSD(entry + 0x10u) = 0;                                 /* 0x49B5D */
        if (DSB(DSD(slot) + 0x51u) != 0u
                || DSW(DS_00104B00) == 0x22u) {                 /* 0x49B6E/0x49B7C/0x49B82 */
            DSW(actor + 0x2Eu) = (u16)(DSW(actor + 0x2Eu) + 4u);    /* 0x49B87 */
            DSB(actor + 0x4Eu) = 1u;                            /* 0x49B8F */
        }
        if (kind != 0u) {
            if ((DSB(entry + 0x1Cu) & 1u) != 0u)                /* 0x49B9B..0x49BA8 */
                DSW(actor + 0x34u) = flag ? 0x00C0u : 0xFF40u;  /* 0x49BC2 */
            else
                DSW(actor + 0x34u) = flag ? 0x0100u : 0xFF00u;  /* 0x49BDE */
            if (!flag)                                          /* 0x49BE2 */
                DSB(actor + 0x29u) = (u8)(DSB(actor + 0x29u) | 0x40u);  /* 0x49BEC */
            DSB(entry + 0x1Eu) = 7u;                            /* 0x49BF3 */
            DSB(entry + 0x1Cu) = (u8)(DSB(entry + 0x1Cu) | 0x10u);  /* 0x49BF9 */
        } else {
            fight_4b144(entry, index);                          /* 0x49C05 */
        }
    }
}

/* 0x4DBEC — record §48-Q. The winner's crowd at a match's end: its callers
 * are mode 0xD's 0x274FC (0x2766D) and mode 0x32's 0x296B8 (0x29838), each
 * on its final-round arm. w = DS_0010810D (zero-extended, 0x4DC0E). The byte
 * DS_001088BB = 0 (AH), the word DS_001088B0 = rng(0x14) + 0x78, and the
 * frame [esp+0x18] = pos = 0x2BE00(w's fighter) with the bounds pos -+ 0x5780
 * ([esp+0x10]/[esp+0x14]). Then up to 0x1C entries (0x4DECC `jl`), each:
 * - the flag [esp+0x1C] toggles (0 on entry, so 1 first) and the lane
 *   [esp+0xC] steps 1, 2, 0, 1, ... (reset to 0 past 2, signed);
 * - a node moves from the free list DS_001083C4 to DS_0010884C, or the
 *   function returns when the free list is empty (0x4DC89..0x4DCA6);
 * - the descriptor 0xC9524[0x49388(w)] with +0x10 = 0x29CDC(w, slot +0x7A);
 * - rng(2) non-zero: x = 0x2BE4C(fighter, pos - 0x5780), the target t = pos
 *   - lane * 0xA00 - rng(0xA00), or rng(0xA00) when that is negative
 *   (signed); EBP = 0. Zero: x = 0x2BE4C(fighter, pos + 0x5780), t = lane *
 *   0xA00 + pos + rng(0xA00), or 0x5400 - rng(0xA00) when that exceeds 0x5400
 *   (signed); EBP = 1. t goes to the entry's +0x14;
 * - the entry's type +0x1E: 4 when rng(2) is non-zero and the flag is 1,
 *   else the lane + 1 (the second rng(2) is drawn either way);
 * - y = (fighter +0x30 >> 16) - 0x80 - rng(0x200) with the flag set, else
 *   + 0x80 + rng(0x200);
 * - 0x2AE14(desc, x, a3 = y, a4 = 0, a5 = 0) into the entry's +8, and 0x4987C's
 *   field stores: the actor's +0x14 = the entry, the entry's +0xC = the
 *   slot, +0x21 = w, +0x1F = 0, the actor's +0x2C = 0x496AC(y), +0x29 |=
 *   0x10, the entry's +0x1C word and +0x10 dword = 0; a character-marked
 *   fighter (+0x51) gives +0x2E += 4 and +0x4E = 1;
 * - the actor's +0x34 = -0x80 (EBP set; +0x29 |= 0x40, the flags still
 *   those of 0x4DE8A `test ebp,ebp`) or 0x80, and the 0xC95D4[si] stream at
 *   3.0, si = (u16)(the actor's +0x48 - 0x20). */
void fight_4dbec(void)
{
    u32 w = DSB(FIGHT_SLOT_INDEX);                              /* 0x4DC0E */
    u32 slot = DS_001077B0 + w * 0x94u;                         /* 0x4DC1A..0x4DC32 */
    s32 pos, left, right;
    u32 flag = 0u, i;                                           /* 0x4DC43 (DL = 0) */
    s32 lane = 0;                                               /* 0x4DC4D */
    DSB(DS_001088BB) = 0u;                                      /* 0x4DBF7 (AH) */
    DSW(DS_001088B0) = (u16)(rng_next(0x14u) + 0x78u);          /* 0x4DBFD..0x4DC14 */
    pos = fight_2be00(DSD(slot));                               /* 0x4DC36/0x4DC3E 0x2BE00 */
    left = (s32)((u32)pos - 0x5780u);                           /* 0x4DC55/0x4DC60 */
    right = (s32)((u32)pos + 0x5780u);                          /* 0x4DC5B/0x4DC64 */
    for (i = 0; i < 0x1Cu; i++) {                               /* 0x4DEC3..0x4DECF */
        u32 entry, desc, actor, index;
        s32 x, t, y;
        int away;
        flag = flag == 0u ? 1u : 0u;                            /* 0x4DC68..0x4DC76 */
        lane++;                                                 /* 0x4DC75 */
        if (lane > 2) lane = 0;                                 /* 0x4DC7E `jle`, 0x4DC85 */
        entry = DSD(DS_001083C4);                               /* 0x4DC89 */
        if (entry == DS_001083C4) return;                       /* 0x4DC8F/0x4DCA6 */
        effects_list_unlink(entry);                             /* 0x4DC9D 0x249D0 */
        effects_list_insert_after(DS_0010884C, entry);          /* 0x4DCB9 0x249B0 */
        desc = DSD(DS_000C9524 + fight_dust_pick(w) * 4u);      /* 0x4DCC5/0x4DCCA */
        DSD(desc + 0x10u) =
            fight_dust_value(w, (u32)DSB(slot + 0x7Au));        /* 0x4DCE4/0x4DCEC */
        if (rng_next(2u) != 0u) {                               /* 0x4DCF4/0x4DCFB */
            x = fight_2be4c(DSD(DS_001077B0 + w * 0x94u), left);    /* 0x4DD14 */
            t = (s32)((u32)pos - (u32)lane * 0xA00u);           /* 0x4DCAC, 0x4DD1B..0x4DD21 */
            t = (s32)((u32)t - rng_next(0xA00u));               /* 0x4DD28/0x4DD2D */
            away = 0;                                           /* 0x4DD2F */
            if (t < 0) t = (s32)rng_next(0xA00u);               /* 0x4DD35 `jge`, 0x4DD3C */
        } else {
            x = fight_2be4c(DSD(DS_001077B0 + w * 0x94u), right);   /* 0x4DD58 */
            t = (s32)((u32)lane * 0xA00u + (u32)pos);           /* 0x4DD5F */
            t = (s32)(rng_next(0xA00u) + (u32)t);               /* 0x4DD68/0x4DD6D */
            away = 1;                                           /* 0x4DD6F */
            if (t > 0x5400)                                     /* 0x4DD74 `jle` */
                t = (s32)(0x5400u - rng_next(0xA00u));          /* 0x4DD80..0x4DD8C */
        }
        DSD(entry + 0x14u) = (u32)t;                            /* 0x4DD8E */
        if (rng_next(2u) != 0u && flag == 1u)                   /* 0x4DD96..0x4DDA8 */
            DSB(entry + 0x1Eu) = 4u;                            /* 0x4DDAA */
        else
            DSB(entry + 0x1Eu) = (u8)(lane + 1);                /* 0x4DDB0..0x4DDB6 */
        y = (s32)DSD(DSD(DS_001077B0 + w * 0x94u) + 0x30u) >> 16;   /* 0x4DDCD..0x4DDD6 */
        if (flag != 0u) {                                       /* 0x4DDB9 */
            y -= 0x80;                                          /* 0x4DDDE */
            y = (s32)((u32)y - rng_next(0x200u));               /* 0x4DDE4/0x4DDE9 */
        } else {
            y += 0x80;                                          /* 0x4DE0B */
            y = (s32)((u32)y + rng_next(0x200u));               /* 0x4DE11/0x4DE16 */
        }
        actor = actor_spawn((const u32 *)(mem + desc), (u32)x, (u32)y,
                            0u, 0u);                            /* 0x4DE18..0x4DE22 0x2AE14 */
        DSD(entry + 8u) = actor;                                /* 0x4DE29 */
        index = (u32)(u16)((u32)DSB(actor + 0x48u) - 0x20u);    /* 0x4DE2C..0x4DE34, 0x4DE74 */
        DSD(actor + 0x14u) = entry;                             /* 0x4DE31 */
        DSD(entry + 0xCu) = slot;                               /* 0x4DE3A */
        DSB(entry + 0x21u) = (u8)w;                             /* 0x4DE42 */
        DSB(entry + 0x1Fu) = 0u;                                /* 0x4DE47 */
        DSW(actor + 0x2Cu) = fight_dust_clamp(y);               /* 0x4DE4B/0x4DE53 0x496AC */
        DSB(actor + 0x29u) = (u8)(DSB(actor + 0x29u) | 0x10u);  /* 0x4DE5A */
        DSW(entry + 0x1Cu) = 0u;                                /* 0x4DE5E */
        DSD(entry + 0x10u) = 0u;                                /* 0x4DE68 */
        if (DSB(DSD(slot) + 0x51u) != 0u) {                     /* 0x4DE6F..0x4DE79 */
            DSW(actor + 0x2Eu) = (u16)(DSW(actor + 0x2Eu) + 4u);    /* 0x4DE7E */
            DSB(actor + 0x4Eu) = 1u;                            /* 0x4DE86 */
        }
        DSW(actor + 0x34u) = away ? 0xFF80u : 0x0080u;          /* 0x4DE8A..0x4DE9D */
        if (away)                                               /* 0x4DEA1 `jz` */
            DSB(actor + 0x29u) = (u8)(DSB(actor + 0x29u) | 0x40u);  /* 0x4DEA6 */
        actors_anim_begin(actor, DSD(DS_000C95D4 + index * 4u),
                          0x40400000u);                         /* 0x4DEAF..0x4DEBE 0x2BC30 */
    }
}

#define DS_000C9574 0x000C9574u   /* no symbols.h name: [si] the winner's lane-1 stream */
#define DS_000C95A4 0x000C95A4u   /* no symbols.h name: [si] the loser's lane-1 stream (rng) */
#define DS_000C961C 0x000C961Cu   /* no symbols.h name: [si] the loser's lane-1 stream */
#define DS_000C964C 0x000C964Cu   /* no symbols.h name: [si] the loser's lane-0 stream (outer) */
#define DS_000C9664 0x000C9664u   /* no symbols.h name: [si] the loser's lane-0 stream (middle) */
#define DS_000C97FA 0x000C97FAu   /* no symbols.h name: [lane] the crowd's y word */
#define DS_000C9800 0x000C9800u   /* no symbols.h name: [lane] the crowd's y word (stages 0, 6) */
#define DS_000C9806 0x000C9806u   /* no symbols.h name: [lane] the crowd's y range word */

/* 0x4B9AC — record §48-D. The challenge screen's crowd (0x424E8's case 2,
 * 0x42574; its only caller), 0x494A8's sibling. EBX/ECX/EDX/ESI/EDI/EBP are
 * pushed and popped; the frame holds [esp+0x10] the side, [esp+0x1C]/[esp+
 * 0x24] its slot, [esp+4] the leader (DS_001080F0 for side 1, else
 * DS_001080EC: 0x42724's slot records), [esp+0x20] the x base (0x5E0 for
 * side 1, else -0x22E0), [esp+8]/[esp+0xC] the bounds base + 0x9AA and base +
 * 0x1354, [esp+0x28] n = the slot's +0x81 byte * 2, at most 0xA (`jle`),
 * [esp+0x14] the count i and [esp+0x2C] the lane k (0 per side; its word
 * steps 0, 1, 2, 0, ...: `inc`, and 0 past 2). While the word n is above i
 * (signed, 0x4BD17 `jg`), each entry:
 * - the y word 0xC9800[k] on stage (DS_00104AFC) 0 or 6, else 0xC97FA[k],
 *   and the range word 0xC9806[k] (both zero-extended);
 * - a node from the free list DS_001083C4 to DS_0010884C, or the whole
 *   function returns when the list is empty (0x4BA9F..0x4BAB6: the
 *   sentinel gives EDI = 0; a zero head is unlinked first, then returns);
 * - the descriptor 0xC9524[rng(6)] with +0x10 = 0x29CDC(side, slot +0x7A);
 * - x = base + rng(0x1D00) (EBP), y = the y word + rng(range) (0x4BB0F
 *   `lea`); 0x2AE14(desc, x, a3 = y, a4 = 0, a5 = 0) into the entry's +8;
 *   the actor's +0x2C = 0x496AC(its +0x30 >> 16) and +0x14 = the entry;
 *   the entry's +0x1E/+0x1F = 0 and +0xC = the slot;
 * - si = (u16)(the actor's +0x48 byte - 0x20) (DH cleared, DL loaded, then
 *   `sub edx,0x20`, used as DX or `and edx,0xffff`); the actor's +0x29 |=
 *   0x40 when its +0x18 is above the leader's (signed, 0x4BB5E `jle`);
 * - on k (the word, `cmp cx,1`): 0: the side DS_00104AD4 (the winner, a
 *   dword compare) takes 0xC9724[si] at 5.0; else x below the first bound
 *   (signed) 0xC964C[si] at 2.0, below the second 0xC9664[si] at 2.0, else
 *   0xC964C[si] at 2.0 and +0x29 |= 0x40 after it. 1: the winner +0x55 = 1
 *   and 0xC9574[si] at 3.0; else rng(2) non-zero +0x55 = 1 and 0xC95A4[si]
 *   at 3.0, zero 0xC961C[si] at 2.0. 2: +0x55 = 1 and 0xC955C[si] (the
 *   winner) or 0xC958C[si] at 3.0. Every stream goes through 0x2BC30 with
 *   the pushed float;
 * - the entry's +0x21 = the side; side 1's actor +0x2E += 4 and +0x4E = 1.
 * ECX (the range) and EBX (the y word) survive 0x29CDC (pushes EBX) and
 * 0x5D7DC (pushes EBX/EDX); EDX (si) survives the rng(2) draw. */
void fight_challenge_crowd(void)
{
    u32 side;
    for (side = 0; side < 2u; side++) {                         /* 0x4B9BA..0x4B9C9, 0x4BD1F..0x4BD39 */
        u32 slot = DS_001077B0 + side * 0x94u;                  /* 0x4B9BC, 0x4BD27 */
        u32 lead = side != 0u ? DSD(DS_001080F0)
                              : DSD(DS_001080EC);               /* 0x4B9DF..0x4B9F3 */
        s32 base = side != 0u ? 0x5E0 : -0x22E0;                /* 0x4B9F7..0x4BA0B */
        u32 n = (u32)DSB(slot + 0x81u) * 2u;                    /* 0x4BA0F..0x4BA17 */
        u32 k = 0u;                                             /* 0x4B9D7 */
        s32 far, near, i;
        if ((s32)n > 0xA) n = 0xAu;                             /* 0x4BA19..0x4BA1E */
        far = base + 0x1354;                                    /* 0x4BA23..0x4BA31 */
        near = base + 0x9AA;                                    /* 0x4BA35..0x4BA44 */
        for (i = 0; (s32)(u16)n > i; i++) {                     /* 0x4BA40, 0x4BD08..0x4BD19 */
            u32 stage = DSW(DS_00104AFC);                       /* 0x4BA4D */
            u32 y0, range, entry, desc, actor, index, stream, bits;
            s32 x;
            if (stage == 0u || stage == 6u)                     /* 0x4BA54..0x4BA61 */
                y0 = DSW(DS_000C9800 + k * 2u);                 /* 0x4BA63..0x4BA6A */
            else
                y0 = DSW(DS_000C97FA + k * 2u);                 /* 0x4BA74..0x4BA7B */
            range = DSW(DS_000C9806 + k * 2u);                  /* 0x4BA88..0x4BA97 */
            entry = DSD(DS_001083C4);                           /* 0x4BA91 */
            if (entry == DS_001083C4)                           /* 0x4BA9F/0x4BAA5 */
                entry = 0u;                                     /* 0x4BAA7 */
            else
                effects_list_unlink(entry);                     /* 0x4BAAB/0x4BAAD 0x249D0 */
            if (entry == 0u) return;                            /* 0x4BAB2..0x4BAB6 */
            effects_list_insert_after(DS_0010884C, entry);      /* 0x4BABC..0x4BAC3 0x249B0 */
            desc = DSD(DS_000C9524 + rng_next(6u) * 4u);        /* 0x4BAC8..0x4BAD9 0x5D7DC */
            DSD(desc + 0x10u) = fight_dust_value(side, (u32)DSB(slot + 0x7Au));  /* 0x4BADC..0x4BAF1 0x29CDC */
            x = base + (s32)rng_next(0x1D00u);                  /* 0x4BAF4..0x4BB02 0x5D7DC */
            actor = actor_spawn((const u32 *)(mem + desc), (u32)x,
                                y0 + rng_next(range), 0u, 0u);  /* 0x4BB04..0x4BB0F 0x5D7DC, 0x4BB18 0x2AE14 */
            DSD(entry + 8u) = actor;                            /* 0x4BB1D */
            DSW(actor + 0x2Cu) =
                fight_dust_clamp((s32)DSD(actor + 0x30u) >> 16);    /* 0x4BB20..0x4BB2E 0x496AC */
            DSD(actor + 0x14u) = entry;                         /* 0x4BB32/0x4BB35 */
            DSB(entry + 0x1Eu) = 0u;                            /* 0x4BB38 */
            DSB(entry + 0x1Fu) = 0u;                            /* 0x4BB40 */
            DSD(entry + 0x0Cu) = slot;                          /* 0x4BB3C/0x4BB44 */
            index = (u32)(u16)((u32)DSB(actor + 0x48u) - 0x20u);    /* 0x4BB4E..0x4BB59 */
            if ((s32)DSD(actor + 0x18u) > (s32)DSD(lead + 0x18u))   /* 0x4BB47..0x4BB5E */
                DSB(actor + 0x29u) = (u8)(DSB(actor + 0x29u) | 0x40u);  /* 0x4BB60 */
            if (k == 0u) {                                      /* 0x4BB68 `jc`, 0x4BB83 */
                if (side == DSD(DS_00104AD4)) {                 /* 0x4BB8C..0x4BB98 */
                    actors_anim_begin(actor, DSD(DS_000C9724 + index * 4u),
                                      0x40A00000u);             /* 0x4BB9A..0x4BBA9, 0x4BCCC 0x2BC30 */
                } else if (x < near) {                          /* 0x4BBB3/0x4BBB7 */
                    actors_anim_begin(actor, DSD(DS_000C964C + index * 4u),
                                      0x40000000u);             /* 0x4BBB9..0x4BBC7, 0x4BCCC 0x2BC30 */
                } else if (x < far) {                           /* 0x4BBD3/0x4BBD7 */
                    actors_anim_begin(actor, DSD(DS_000C9664 + index * 4u),
                                      0x40000000u);             /* 0x4BBD9..0x4BBE8, 0x4BCCC 0x2BC30 */
                } else {
                    actors_anim_begin(actor, DSD(DS_000C964C + index * 4u),
                                      0x40000000u);             /* 0x4BBF2..0x4BC07 0x2BC30 */
                    DSB(actor + 0x29u) = (u8)(DSB(actor + 0x29u) | 0x40u);  /* 0x4BC0C/0x4BC0F */
                }
            } else if (k == 1u) {                               /* 0x4BB6E `jbe` */
                if (side == DSD(DS_00104AD4)) {                 /* 0x4BC18..0x4BC22 */
                    DSB(actor + 0x55u) = 1u;                    /* 0x4BC27 */
                    stream = DSD(DS_000C9574 + index * 4u);     /* 0x4BC2B..0x4BC30 */
                    bits = 0x40400000u;                         /* 0x4BC3A */
                } else if (rng_next(2u) != 0u) {                /* 0x4BC44..0x4BC50 0x5D7DC */
                    DSB(actor + 0x55u) = 1u;                    /* 0x4BC55 */
                    stream = DSD(DS_000C95A4 + index * 4u);     /* 0x4BC59..0x4BC5E */
                    bits = 0x40400000u;                         /* 0x4BC68 */
                } else {
                    stream = DSD(DS_000C961C + index * 4u);     /* 0x4BC74..0x4BC7D */
                    bits = 0x40000000u;                         /* 0x4BC6F */
                }
                actors_anim_begin(actor, stream, bits);         /* 0x4BCCC 0x2BC30 */
            } else if (k == 2u) {                               /* 0x4BB74/0x4BB78 */
                DSB(actor + 0x55u) = 1u;                        /* 0x4BC9A/0x4BCB8 */
                if (side == DSD(DS_00104AD4))                   /* 0x4BC86..0x4BC90 */
                    stream = DSD(DS_000C955C + index * 4u);     /* 0x4BC9E..0x4BCA7 */
                else
                    stream = DSD(DS_000C958C + index * 4u);     /* 0x4BCBC..0x4BCC5 */
                actors_anim_begin(actor, stream, 0x40400000u);  /* 0x4BC95/0x4BCB3, 0x4BCCC 0x2BC30 */
            }
            DSB(entry + 0x21u) = (u8)side;                      /* 0x4BCD1..0x4BCD9 */
            if (side != 0u) {                                   /* 0x4BCDC/0x4BCDE */
                DSW(actor + 0x2Eu) = (u16)(DSW(actor + 0x2Eu) + 4u);    /* 0x4BCE0..0x4BCE3 */
                DSB(actor + 0x4Eu) = 1u;                        /* 0x4BCE8..0x4BCEB */
            }
            k = (u32)(u16)(k + 1u);                             /* 0x4BCEF..0x4BCF9 */
            if (k > 2u) k = 0u;                                 /* 0x4BCFD..0x4BD04 */
        }
    }
}

/* 0x4D108 — demo-pose record §43-A. EAX = entry, EDX = si. One time in 0x3C
 * (rng(0x3C) == 0) the actor rises: the 0xC95BC[si] stream at 3.0, the
 * entry's +0x1A = the actor's y word +0x32, +0x38 = -0x20, type 3; AL = 1.
 * Else AL = 0 (0x4D149 `xor al,al`). */
static int fight_4d108(u32 entry, u32 index)
{
    if (rng_next(0x3Cu) != 0u) return 0;                        /* 0x4D110/0x4D117 */
    actors_anim_begin(DSD(entry + 8u), DSD(DS_000C95BC + index * 4u),
                      0x40400000u);                             /* 0x4D128 */
    DSW(entry + 0x1Au) = DSW(DSD(entry + 8u) + 0x32u);          /* 0x4D134 */
    DSW(DSD(entry + 8u) + 0x38u) = 0xFFE0u;                     /* 0x4D13B */
    DSB(entry + 0x1Eu) = 3u;                                    /* 0x4D143 */
    return 1;                                                   /* 0x4D141 */
}

/* 0x4D150 — demo-pose record §43-A. EAX = entry, EDX = si. One time in 0x3C
 * the actor walks: to rng(4) * 0xC00 to the right (rng(2) non-zero) or left
 * of DS_00104B1A's fighter (0x2BE00), through 0x2BE4C into +0x14, type 1,
 * +0x34 = 0x80 with the hflip cleared when the target is right of the
 * actor's x (signed), else -0x80 with it set, and the 0xC95D4[si] stream at
 * 3.0; returns 1. Else 0. */
static int fight_4d150(u32 entry, u32 index)
{
    if (rng_next(0x3Cu) != 0u) return 0;                        /* 0x4D15C/0x4D163 */
    u32 off;
    if (rng_next(2u) != 0u)                                     /* 0x4D170 */
        off = rng_next(4u) * 0xC00u;                            /* 0x4D17E..0x4D18C */
    else
        off = 0u - rng_next(4u) * 0xC00u;                       /* 0x4D196..0x4D1A7 */
    u32 rec = DSD(DS_001077B0 + (u32)DSB(DS_00104B1A) * 0x94u); /* 0x4D1AB..0x4D1BF */
    s32 target = fight_2be4c(DSD(entry + 8u),
                             (s32)((u32)fight_2be00(rec) + off));   /* 0x4D1C6..0x4D1D1 */
    DSB(entry + 0x1Eu) = 1u;                                    /* 0x4D1D6 */
    DSD(entry + 0x14u) = (u32)target;                           /* 0x4D1DA */
    u32 actor = DSD(entry + 8u);
    if ((s32)DSD(entry + 0x14u) > (s32)DSD(actor + 0x18u)) {    /* 0x4D1E3 `jle` */
        DSW(actor + 0x34u) = 0x0080u;                           /* 0x4D1E8 */
        DSB(actor + 0x29u) = (u8)(DSB(actor + 0x29u) & 0xBFu);  /* 0x4D1F1 */
    } else {
        DSW(actor + 0x34u) = 0xFF80u;                           /* 0x4D1F7 */
        DSB(actor + 0x29u) = (u8)(DSB(actor + 0x29u) | 0x40u);  /* 0x4D200 */
    }
    actors_anim_begin(actor, DSD(DS_000C95D4 + index * 4u), 0x40400000u); /* 0x4D213 */
    return 1;                                                   /* 0x4D218 */
}

/* 0x4D224 — demo-pose record §43-A. The mode-0x22 pass's handler for types 0,
 * 7 and above 8 (EAX = entry, EDX = si): with DS_001088C2 set, 0x4BD4C may
 * take it; else the rise 0x4D108, the walk 0x4D150, then (on the slot's +0x42
 * bit 1 or DS_001088B2[+0x21]) 0x4B3F0, then (on +0x42 bit 0 or
 * DS_0010889E[+0x21]) 0x4B430, each with EBX = 0; the first that acts ends it. */
static void fight_4d224(u32 entry, u32 index)
{
    if (DSB(DS_001088C2) != 0u) {                               /* 0x4D22B */
        if (fight_4bd4c(entry, index) != 0) return;             /* 0x4D236/0x4D23D */
    }
    if (fight_4d108(entry, index) != 0) return;                 /* 0x4D247/0x4D24E */
    if (fight_4d150(entry, index) != 0) return;                 /* 0x4D254/0x4D25B */
    if ((DSB(DSD(entry + 0xCu) + 0x42u) & 2u) != 0u
            || DSB(DS_001088B2 + (u32)DSB(entry + 0x21u)) != 0u) {  /* 0x4D260/0x4D26B */
        if (fight_4b3f0(entry, index, 0u) != 0) return;         /* 0x4D27A/0x4D281 */
    }
    if ((DSB(DSD(entry + 0xCu) + 0x42u) & 1u) != 0u
            || DSB(DS_0010889E + (u32)DSB(entry + 0x21u)) != 0u)    /* 0x4D286/0x4D291 */
        (void)fight_4b430(entry, index, 0u);                    /* 0x4D2A0 */
}

/* 0x4D2D0 — demo-pose record §43-A. The effects pass of modes 0x22 and 0x24
 * (called at 0x26D28 in 0x26C8C and 0x26FF0 in 0x26F58, neither ported). It
 * walks DS_0010884C counting the entries into the word of the side
 * DS_00104B1A; in mode 0x22 each entry first runs the prelude 0x4D7A4. While
 * DS_00104AC4 <= 1 (signed) an entry that is not type 6 and has neither
 * +0x1C bit 2 nor bit 6 stops (+0x38/+0x34/+0x36 zeroed, +0x1C bit 2, the
 * arrival 0x4AC38 for its actor's +0x14 entry) and becomes type 8. Otherwise
 * the type (jump table 0x4D2AC): 0, 7 and above 8 0x4D224; 1 the walk's
 * arrival (0x49C78's case 1); 2 the wait; 3 the fall, landing on 0xC9634[si]
 * at 2.0 or (rng(2) zero) 0xC9724[si] at 5.0; 4 the lie, which on expiry
 * re-arms (rng(2)), walks within +-0x4D00 (rng(2), the direction rng(2)) as
 * type 4 with the timer rng(0x1E) + 0x3C, or climbs (0xC95EC[si], +0x38 =
 * 0x20, type 5); 5 the climb; 6 the tumble, whose landing also stops the
 * actor as above while DS_00104AC4 <= 1; 8 nothing. After the walk
 * DS_001088C2 = 0, and when the side's slot +0x81 exceeds its count (as a
 * signed word) 0x4987C(side, the difference, 0) refills the list. */
void fight_4d2d0(void)
{
    u16 cnt[2] = { 0u, 0u };                                    /* 0x4D2E1/0x4D2E6 */
    u32 entry = DSD(DS_0010884C);                               /* 0x4D2DB */
    if (entry != DS_0010884C) {                                 /* 0x4D2EA */
        for (;;) {
            u32 next = DSD(entry);                              /* 0x4D2F6 */
            u32 actor = DSD(entry + 8u);                        /* 0x4D2F8: ECX */
            u32 index = (u32)(u16)((u32)DSB(actor + 0x48u) - 0x20u);   /* 0x4D304/0x4D310 */
            u32 side = DSB(DS_00104B1A);                        /* 0x4D2FF */
            /* PORT: 0x4D313 `inc word [esp + eax*2]` in a 4-byte frame. The
             * only writers of DS_00104B1A (0x269D0, 0x269DF) store 0 or 1; a
             * larger side would count into the raw's saved registers, which
             * the port does not model. */
            if (side < 2u) cnt[side] = (u16)(cnt[side] + 1u);   /* 0x4D313 */
            if (DSW(DS_00104B00) == 0x22u)                      /* 0x4D317 */
                fight_4d7a4(entry, index);                      /* 0x4D323 */
            if ((s32)DSD(DS_00104AC4) <= 1
                    && DSB(entry + 0x1Eu) != 6u
                    && (DSB(entry + 0x1Cu) & 0x44u) == 0u) {    /* 0x4D32F/0x4D339/0x4D348 */
                DSW(DSD(entry + 8u) + 0x38u) = 0;               /* 0x4D34D */
                DSW(DSD(entry + 8u) + 0x34u) = 0;               /* 0x4D356 */
                DSW(DSD(entry + 8u) + 0x36u) = 0;               /* 0x4D35F */
                DSB(entry + 0x1Cu) = (u8)(DSB(entry + 0x1Cu) | 4u);     /* 0x4D36E */
                {
                    u32 own = DSD(DSD(entry + 8u) + 0x14u);     /* 0x4D371 */
                    if (own != 0u)                              /* 0x4D376 */
                        fight_4ac38(own, (u32)DSB(DSD(entry + 8u) + 0x48u)
                                         - 0x20u);              /* 0x4D37A..0x4D382 */
                }
                DSB(entry + 0x1Eu) = 8u;                        /* 0x4D387 */
            } else {
                switch (DSB(entry + 0x1Eu)) {                   /* 0x4D390..0x4D39C */
                case 1: {
                    if (DSB(DS_001088C2) != 0u) {               /* 0x4D3B5 */
                        if (fight_4bd4c(entry, index) != 0) break;      /* 0x4D3C5/0x4D3CC */
                    }
                    s32 d = (s32)(DSD(DSD(entry + 8u) + 0x18u)
                                  - DSD(entry + 0x14u));        /* 0x4D3D8/0x4D3DB */
                    if (d < 0) d = (s32)(0u - (u32)d);          /* 0x4D3DF/0x4D3E3 */
                    u32 a = DSD(entry + 8u);
                    s32 step = (s32)DSD(a + 0x32u) >> 16;       /* 0x4D3F3/0x4D3FD */
                    if ((s16)DSW(a + 0x34u) < 0)                /* 0x4D3EC */
                        step = (s32)(0u - (u32)step);           /* 0x4D3F9 */
                    if (d > step) break;                        /* 0x4D405 `jg` */
                    fight_4ac38(entry, index);                  /* 0x4D412 */
                    break;
                }
                case 2: {
                    u16 t = (u16)(DSW(entry + 0x18u) - 1u);     /* 0x4D420 */
                    DSW(entry + 0x18u) = t;                     /* 0x4D421 */
                    if ((s16)t > 0) break;                      /* 0x4D428 `jg` */
                    fight_4ac38(entry, index);                  /* 0x4D435 */
                    break;
                }
                case 3:
                    DSW(DSD(entry + 8u) + 0x2Cu) =
                        fight_dust_clamp((s32)DSD(DSD(entry + 8u) + 0x30u) >> 16); /* 0x4D448/0x4D450 */
                    if (((s32)DSD(actor + 0x30u) >> 16)
                            > (s32)(u32)DSW(DS_000BD898))       /* 0x4D463 `jg` */
                        break;
                    {
                        u32 bit = (fight_2be1c(DSD(entry + 8u), DSD(DSD(entry + 0xCu))) > 0)
                                ? 0x4000u : 0u;                 /* 0x4D476..0x4D486 */
                        DSW(actor + 0x28u) = (u16)(DSW(actor + 0x28u) | bit); /* 0x4D490 */
                    }
                    if (rng_next(2u) != 0u)                     /* 0x4D499 */
                        actors_anim_begin(DSD(entry + 8u), DSD(DS_000C9634 + index * 4u),
                                          0x40000000u);         /* 0x4D4A7/0x4D4CC */
                    else
                        actors_anim_begin(DSD(entry + 8u), DSD(DS_000C9724 + index * 4u),
                                          0x40A00000u);         /* 0x4D4C0/0x4D4CC */
                    DSW(actor + 0x38u) = 0;                     /* 0x4D4D1 */
                    DSW(actor + 0x34u) = 0;                     /* 0x4D4DC */
                    DSW(entry + 0x18u) = (u16)(rng_next(0x3Cu) + 0x3Cu);   /* 0x4D4E2/0x4D4F6 */
                    DSB(entry + 0x1Eu) = 4u;                    /* 0x4D4EF */
                    DSB(entry + 0x1Cu) = (u8)(DSB(entry + 0x1Cu) | 0x80u); /* 0x4D4FA */
                    break;
                case 4: {
                    u16 t = (u16)(DSW(entry + 0x18u) - 1u);     /* 0x4D506 */
                    DSW(entry + 0x18u) = t;                     /* 0x4D507 */
                    if ((s16)t > 0) break;                      /* 0x4D50E `jg` */
                    if (rng_next(2u) != 0u) {                   /* 0x4D519 */
                        DSW(entry + 0x18u) = (u16)(rng_next(0x3Cu) + 0x3Cu); /* 0x4D527/0x4D531 */
                        break;
                    }
                    if (rng_next(2u) != 0u) {                   /* 0x4D53F */
                        s32 ax = (s32)DSD(DSD(entry + 8u) + 0x18u);     /* 0x4D54F */
                        if (ax > -0x4D00 && ax < 0x4D00) {      /* 0x4D558 `jle`/0x4D560 `jge` */
                            if (rng_next(2u) != 0u) {           /* 0x4D567 */
                                DSW(DSD(entry + 8u) + 0x34u) = 0x0080u; /* 0x4D573 */
                                DSB(DSD(entry + 8u) + 0x29u) =
                                    (u8)(DSB(DSD(entry + 8u) + 0x29u) & 0xBFu); /* 0x4D57C */
                            } else {
                                DSW(DSD(entry + 8u) + 0x34u) = 0xFF80u; /* 0x4D585 */
                                DSB(DSD(entry + 8u) + 0x29u) =
                                    (u8)(DSB(DSD(entry + 8u) + 0x29u) | 0x40u); /* 0x4D58E */
                            }
                            actors_anim_begin(DSD(entry + 8u), DSD(DS_000C95D4 + index * 4u),
                                              0x40400000u);     /* 0x4D5A6 */
                            DSB(entry + 0x1Eu) = 4u;            /* 0x4D5B0 */
                            DSW(entry + 0x18u) = (u16)(rng_next(0x1Eu) + 0x3Cu); /* 0x4D5B4/0x4D5BE */
                            break;
                        }
                    }
                    actors_anim_begin(DSD(entry + 8u), DSD(DS_000C95EC + index * 4u),
                                      0x40400000u);             /* 0x4D5DB */
                    DSW(actor + 0x38u) = 0x0020u;               /* 0x4D5E0 */
                    DSB(entry + 0x1Eu) = 5u;                    /* 0x4D5E9 */
                    DSB(entry + 0x1Cu) = (u8)(DSB(entry + 0x1Cu) & 0x7Fu); /* 0x4D5ED/0x4D5F0 */
                    break;
                }
                case 5:
                    DSW(DSD(entry + 8u) + 0x2Cu) =
                        fight_dust_clamp((s32)DSD(DSD(entry + 8u) + 0x30u) >> 16); /* 0x4D601/0x4D609 */
                    if ((s16)DSW(actor + 0x32u) < (s16)DSW(entry + 0x1Au)) /* 0x4D611 `jl` */
                        break;
                    actors_anim_begin(DSD(entry + 8u), DSD(DS_000C9544 + index * 4u),
                                      0x40400000u);             /* 0x4D62F */
                    DSW(actor + 0x38u) = 0;                     /* 0x4D634 */
                    DSW(actor + 0x34u) = 0;                     /* 0x4D63A */
                    DSB(entry + 0x1Eu) = 0;                     /* 0x4D640 */
                    break;
                case 6:
                    if ((s16)DSW(actor + 0x36u) < 0)            /* 0x4D649 `jge` */
                        DSB(entry + 0x1Cu) = (u8)(DSB(entry + 0x1Cu) | 0x80u); /* 0x4D650 */
                    if (DSD(entry + 0x10u) != 0u) {             /* 0x4D657 */
                        DSD(DSD(entry + 0x10u) + 0x18u) = DSD(DSD(entry + 8u) + 0x18u); /* 0x4D661 */
                        DSW(DSD(entry + 0x10u) + 0x32u) = DSW(DSD(entry + 8u) + 0x32u); /* 0x4D66E */
                    }
                    if ((s32)((u32)((s32)DSD(actor + 0x34u) >> 16)
                              + DSD(actor + 0x1Cu)) > 0) {      /* 0x4D672..0x4D67F */
                        DSW(actor + 0x36u) = (u16)(DSW(actor + 0x36u) - 0x10u); /* 0x4D742 */
                        break;
                    }
                    if (DSD(entry + 0x10u) != 0u) {             /* 0x4D688 */
                        actor_set_dead(DSD(entry + 0x10u));     /* 0x4D68E 0x2B150 */
                        DSD(entry + 0x10u) = 0;                 /* 0x4D693 */
                    }
                    if ((s32)DSD(DS_00104AC4) <= 1) {           /* 0x4D69A `jg` */
                        DSW(DSD(entry + 8u) + 0x38u) = 0;       /* 0x4D6A6 */
                        DSW(DSD(entry + 8u) + 0x34u) = 0;       /* 0x4D6AF */
                        DSW(DSD(entry + 8u) + 0x36u) = 0;       /* 0x4D6B8 */
                        DSB(entry + 0x1Cu) = (u8)(DSB(entry + 0x1Cu) | 4u); /* 0x4D6C7 */
                        {
                            u32 own = DSD(DSD(entry + 8u) + 0x14u);     /* 0x4D6CA */
                            if (own != 0u)                      /* 0x4D6CF */
                                fight_4ac38(own, (u32)DSB(DSD(entry + 8u) + 0x48u)
                                                 - 0x20u);      /* 0x4D6D3..0x4D6E0 */
                        }
                        DSB(entry + 0x1Eu) = 8u;                /* 0x4D6E5 */
                    }
                    {
                        u32 st = DSD(DS_000C973C + index * 4u); /* 0x4D6F1 */
                        if (st != 0u) {                         /* 0x4D6F7 */
                            actors_anim_begin(DSD(entry + 8u), st, 0x40000000u); /* 0x4D705 */
                            DSB(entry + 0x1Eu) = 8u;            /* 0x4D70A */
                        } else {
                            actors_anim_begin(DSD(entry + 8u), DSD(DS_000C9544 + index * 4u),
                                              0x40400000u);     /* 0x4D71E */
                            DSB(entry + 0x1Eu) = 4u;            /* 0x4D723 */
                        }
                    }
                    DSD(actor + 0x1Cu) = 0;                     /* 0x4D727 */
                    DSW(actor + 0x36u) = 0;                     /* 0x4D72E */
                    DSW(actor + 0x34u) = DSW(actor + 0x36u);    /* 0x4D734/0x4D738 */
                    DSB(entry + 0x1Fu) = 0;                     /* 0x4D73C */
                    break;
                case 8:
                    break;                                      /* 0x4D747 */
                default:
                    fight_4d224(entry, index);                  /* 0x4D3AB: 0, 7, > 8 */
                    break;
                }
            }
            entry = next;                                       /* 0x4D747 */
            if (entry == DS_0010884C) break;                    /* 0x4D749 */
        }
    }
    DSB(DS_001088C2) = 0;                                       /* 0x4D75F */
    {
        u32 side = DSB(DS_00104B1A);                            /* 0x4D759 */
        /* PORT: as at 0x4D313, a side above 1 would read the raw's saved
         * registers (0x4D77E); the port does not model it and skips the
         * refill. */
        if (side < 2u) {
            u8 want = DSB(DS_001077B0 + side * 0x94u + 0x81u);  /* 0x4D777 */
            u16 d = (u16)((u32)want - (u32)cnt[side]);          /* 0x4D77E..0x4D784 */
            if ((s16)d > 0)                                     /* 0x4D786 `jle` */
                fight_4987c(side, (s32)(s16)d, 0u);             /* 0x4D78B..0x4D792 */
        }
    }
}

/* 0x4CC0C — demo-pose record §43-A. The volleyball game's end (the mode-0x21
 * worshipper game 0x4BD98 starts): both 0x4BD98 spawns (DS_00108868/
 * DS_0010886C) get +0x36 = -0x1A4, DS_001088A0 = 0x3C, and the score screen
 * is drawn through 0x1C500 + 0x2F510 (the class font, cursor kept) or 0x2F4BC:
 * strings 0x57 (10, 8), 0x58 (0x13, 4), 0x59 (1, 7), 0x5A (0x26, 7), 0x5B
 * (0x13, 1), 0x5C (7, 8), 0x5D (0x16, 8), then centred on rows 6 and 9 0x5E
 * "VOLLEYBALL GAME" and 0x5F "TIED" when DS_0010889C equals DS_0010889D,
 * else 0x16 "RIGHT PLAYER" (DS_0010889C below) or 0x17 "LEFT PLAYER", and
 * 0x5E. Called by 0x4C784 and by the unported 0x4BF18 (0x4C356, 0x4C429). */
void fight_4cc0c(void)
{
    DSW(DSD(DS_00108868) + 0x36u) = 0xFE5Cu;                    /* 0x4CC19 */
    DSW(DS_001088A0) = 0x003Cu;                                 /* 0x4CC29 */
    DSW(DSD(DS_0010886C) + 0x36u) = 0xFE5Cu;                    /* 0x4CC30 */
    text_cursor_hold_font2(0x0A, 8, game_string_get(0x57u), 0x5000u);  /* 0x4CC40/0x4CC4C */
    text_cursor_hold_font2(0x13, 4, game_string_get(0x58u), 0u);       /* 0x4CC5B/0x4CC69 */
    text_cursor_hold(1, 7, game_string_get(0x59u), 0u);                /* 0x4CC78/0x4CC86 0x2F4BC */
    text_cursor_hold(0x26, 7, game_string_get(0x5Au), 0u);             /* 0x4CC95/0x4CCA3 0x2F4BC */
    text_cursor_hold_font2(0x13, 1, game_string_get(0x5Bu), 0x5000u);  /* 0x4CCB7/0x4CCC3 */
    text_cursor_hold_font2(7, 8, game_string_get(0x5Cu), 0x5000u);     /* 0x4CCD7/0x4CCE3 */
    text_cursor_hold_font2(0x16, 8, game_string_get(0x5Du), 0x5000u);  /* 0x4CCF7/0x4CD03 */
    {
        u8 a = DSB(DS_0010889C), b = DSB(DS_0010889D);          /* 0x4CD08/0x4CD0D */
        if (a == b) {                                           /* 0x4CD13 */
            text_cursor_hold_font2(-1, 6, game_string_get(0x5Eu), 0x4000u); /* 0x4CD26/0x4CD32 */
            text_cursor_hold_font2(-1, 9, game_string_get(0x5Fu), 0x4000u); /* 0x4CD80/0x4CD8C */
        } else {
            text_cursor_hold_font2(-1, 6, game_string_get(a <= b ? 0x16u : 0x17u),
                                   0x4000u);                    /* 0x4CD3E `setbe`, 0x4CD6C */
            text_cursor_hold_font2(-1, 9, game_string_get(0x5Eu), 0x4000u); /* 0x4CD80/0x4CD8C */
        }
    }
}

/* 0x4C784 — demo-pose record §43-A. EAX = side: fighter `side` ate the
 * volleyball, the DS_00108864 entry. A 0xC976C actor (the 0xEF65A stream at
 * 3.0) spawns at the ball (its +0x18/+0x1C, y = DS_000BD898 - 0x100, flags
 * 0x4000 unless the fighter faces left); the ball's shadow and actor die. The
 * other side scores (DS_0010889C[other] += 1); the DS_0010886C actor turns on
 * 0xEF680 (DS_00108884 right of the other fighter) or 0xEF6AC at 3.0;
 * DS_001088AC = 0x69 and DS_00108898 = 2 (side 0) or 1. At three points the
 * ball entry's +0x1F clears, 0x4CC0C ends the game unless DS_001088A0 is
 * running, and DS_00108864 = 0. Otherwise string 0x56 "BALL EATEN!" is
 * centred on row 6 and a new ball entry from the free list (dust descriptor
 * of the other side, +0x10 = 0x29CDC(side, the side's slot +0x7A)) spawns
 * 0x2580 beyond DS_00108868 on the side's side, walks (type 1, +0x1C bit 5)
 * to 0x1740 from it and becomes DS_00108864, owned by the eaten ball's side
 * (+0x21, +0x0C). The voices are PORT notes. */
void fight_4c784(u32 side)
{
    u32 hs = DSB(DSD(DS_00108864) + 0x21u);                     /* 0x4C78F..0x4C79E */
    u32 flip = ((DSW(DSD(DS_001077B0 + side * 0x94u) + 0x28u) & 0x4000u) != 0u)
             ? 0u : 0x4000u;                                    /* 0x4C7B0..0x4C7D0 */
    u32 spit = actor_spawn((const u32 *)(mem + DS_000C976C),
                           DSD(DSD(DSD(DS_00108864) + 8u) + 0x18u),
                           (u32)DSW(DS_000BD898) - 0x100u,
                           DSD(DSD(DSD(DS_00108864) + 8u) + 0x1Cu),
                           flip);                               /* 0x4C7D7..0x4C7F8 0x2AE14 */
    actors_anim_begin(spit, 0x000EF65Au, 0x40400000u);          /* 0x4C7FD..0x4C807 */
    if (DSD(DSD(DS_00108864) + 0x10u) != 0u) {                  /* 0x4C811/0x4C814 */
        actor_set_dead(DSD(DSD(DS_00108864) + 0x10u));          /* 0x4C81A 0x2B150 */
        DSD(DSD(DS_00108864) + 0x10u) = 0;                      /* 0x4C824 */
    }
    actor_set_dead(DSD(DSD(DS_00108864) + 8u));                 /* 0x4C833 0x2B150 */
    /* PORT: 0x4C85C 0x2C3FC(0xD4 when the ball's (u8)+0x48 - 0x20 < 3, else
     * 0xD5), 0x4C866 0x2C3FC(0xD6) and 0x4C875 0x2C3FC(0xCE) — voices, out of
     * scope (spec §7). */
    u32 other = side ^ 1u;                                      /* 0x4C86B/0x4C872 */
    DSB(DS_0010889C + other) = (u8)(DSB(DS_0010889C + other) + 1u); /* 0x4C886..0x4C897 */
    actors_anim_begin(DSD(DS_0010886C),
                      ((s32)DSD(DS_00108884)
                       > (s32)DSD(DSD(DS_001077B0 + other * 0x94u) + 0x18u))
                      ? 0x000EF680u : 0x000EF6ACu, 0x40400000u); /* 0x4C8A3 `jle`, 0x4C8BE */
    DSW(DS_001088AC) = 0x0069u;                                 /* 0x4C8CC */
    DSW(DS_00108898) = (u16)((other != 0u) ? 2u : 1u);          /* 0x4C8C3..0x4C8DE */
    if (DSB(DS_0010889C + other) >= 3u) {                       /* 0x4C8E8..0x4C8F6 `jl` */
        u16 running = DSW(DS_001088A0);                         /* 0x4C8FD */
        DSB(DSD(DS_00108864) + 0x1Fu) = 0;                      /* 0x4C904 */
        if (running == 0u)                                      /* 0x4C908 */
            fight_4cc0c();                                      /* 0x4C90D */
        DSD(DS_00108864) = 0;                                   /* 0x4C914 */
        return;
    }
    text_cursor_hold_font2(-1, 6, game_string_get(0x56u), 0x4000u);    /* 0x4C91F..0x4C93A */
    u32 entry = DSD(DS_001083C4);                               /* 0x4C93F */
    if (entry == DS_001083C4) return;                           /* 0x4C945..0x4C95C */
    effects_list_unlink(entry);                                 /* 0x4C953 0x249D0 */
    effects_list_insert_after(DS_0010884C, entry);              /* 0x4C96C 0x249B0 */
    u32 desc = DSD(DS_000C9524 + fight_dust_pick(other) * 4u);  /* 0x4C973/0x4C97F */
    DSD(desc + 0x10u) = fight_dust_value(side,
        (u32)DSB(DSD(DS_001077A8 + side * 4u) + 0x7Au));        /* 0x4C978..0x4C996 */
    s32 base = fight_2be00(DSD(DS_00108868));                   /* 0x4C99E */
    s32 ox = fight_2be00(DSD(DS_001077B0 + other * 0x94u));     /* 0x4C9BA */
    s32 sx = fight_2be00(DSD(DS_001077B0 + side * 0x94u));      /* 0x4C9D6 */
    s32 at, target;
    if (sx < ox) {                                              /* 0x4C9DB `jge` */
        at = (s32)((u32)fight_2be00(DSD(DS_00108868)) - 0x2580u);   /* 0x4C9E4/0x4C9E9 */
        target = (s32)((u32)base + 0x1740u);                    /* 0x4C9EF */
    } else {
        at = (s32)((u32)fight_2be00(DSD(DS_00108868)) + 0x2580u);   /* 0x4C9FC/0x4CA01 */
        target = (s32)((u32)base - 0x1740u);                    /* 0x4CA07 */
    }
    u32 x = (u32)fight_2be4c(DSD(DS_00108868), at);             /* 0x4CA15 */
    u32 y = (u32)DSW(DS_000BD898);                              /* 0x4CA22 */
    u32 actor = actor_spawn((const u32 *)(mem + desc), x, y, 0u, 0u); /* 0x4CA2D 0x2AE14 */
    DSD(entry + 8u) = actor;                                    /* 0x4CA34 */
    u32 index = (u32)(u16)((u32)DSB(actor + 0x48u) - 0x20u);    /* 0x4CA37/0x4CA95 */
    DSD(actor + 0x14u) = entry;                                 /* 0x4CA40 */
    DSB(entry + 0x1Fu) = 0;                                     /* 0x4CA59 */
    DSB(entry + 0x21u) = (u8)hs;                                /* 0x4CA63 */
    DSD(entry + 0xCu) = DS_001077B0 + hs * 0x94u;               /* 0x4CA43..0x4CA68 */
    DSW(actor + 0x2Cu) = fight_dust_clamp((s32)y);              /* 0x4CA6B/0x4CA73 */
    DSW(actor + 0x28u) = 0;                                     /* 0x4CA7A */
    DSW(entry + 0x1Cu) = 0;                                     /* 0x4CA80 */
    DSD(entry + 0x10u) = 0;                                     /* 0x4CA89 */
    if (DSB(DSD(DSD(entry + 0xCu)) + 0x51u) != 0u) {            /* 0x4CA92/0x4CA9A */
        DSW(actor + 0x2Eu) = (u16)(DSW(actor + 0x2Eu) + 4u);    /* 0x4CA9F */
        DSB(actor + 0x4Eu) = 1u;                                /* 0x4CAA7 */
    }
    DSB(entry + 0x1Eu) = 1u;                                    /* 0x4CAAE */
    DSD(DS_00108864) = entry;                                   /* 0x4CAB2 */
    DSD(entry + 0x14u) = (u32)target;                           /* 0x4CABB */
    DSB(entry + 0x1Cu) = (u8)(DSB(entry + 0x1Cu) | 0x20u);      /* 0x4CAC4 */
    if (fight_2be00(actor) < (s32)DSD(entry + 0x14u)) {         /* 0x4CAC7/0x4CACF `jge` */
        DSW(actor + 0x34u) = 0x0080u;                           /* 0x4CAD4 */
        DSB(actor + 0x29u) = (u8)(DSB(actor + 0x29u) & 0xBFu);  /* 0x4CADD */
    } else {
        DSW(actor + 0x34u) = 0xFF80u;                           /* 0x4CAE6 */
        DSB(actor + 0x29u) = (u8)(DSB(actor + 0x29u) | 0x40u);  /* 0x4CAEF */
    }
    actors_anim_begin(actor, DSD(DS_000C95D4 + index * 4u), 0x40400000u); /* 0x4CB07 */
}

/* 0x4C60C — demo-pose record §43-A. The volleyball's per-entry test (EAX =
 * entry, EDX = si; its one caller is 0x4BF18 at 0x4C21B, the mode-0x21 pass,
 * not ported): the actor's pset point against the fighters (0x17D30, BX = 0;
 * both sides count as side 0). A fighter of character 0, 3 or 5 whose slot
 * +0x5F is 0, or of character 2 whose +0x5F is 1, eats it (0x4C784); a
 * character above 6 does nothing; otherwise the ball is struck: DS_00108898 =
 * 0, +0x20 = the side, +0x1C bit 3 cleared, +0x1F counts, a held actor
 * (+0x4A) is released as 0x4B69C releases (without DS_001088B2), and 0x4CB18
 * launches it with the flag 1 (then +0x1C bit 3 set) when the side's +0x5F is
 * 0xC..0xF, else 0. */
void fight_4c60c(u32 entry, u32 index)
{
    u32 rec = DSD(entry + 8u);
    u32 ps = DSD(DS_001014EC) + ((u32)DSW(rec + 0x56u) << 5);  /* 0x4C618..0x4C62C */
    u32 hit = camera_point_hit((s32)(s16)DSW(ps + 4u),
                               (s32)(s16)DSW(ps + 8u), 0u);     /* 0x4C62F..0x4C641 */
    if (hit == 0u) return;                                      /* 0x4C64A */
    if ((s32)hit > 2) hit = 1u;                                 /* 0x4C653 `jle` */
    u32 side = hit - 1u;                                        /* 0x4C65A */
    u32 slot = DS_001077B0 + side * 0x94u;                      /* 0x4C65D..0x4C673 */
    u8 ch = DSB(slot + 0x7Au);                                  /* 0x4C675 */
    int struck = 0;                                             /* ECX, 0x4C61B */
    if (ch <= 6u) {                                             /* 0x4C67D `ja` */
        switch (ch) {                                           /* 0x4C687 0x4C5F0 */
        case 0: case 3: case 5:                                 /* 0x4C68E */
            if (DSB(slot + 0x5Fu) == 0u)
                fight_4c784(side);                              /* 0x4C697 */
            else
                struck = 1;                                     /* 0x4C6B0 */
            break;
        case 2:                                                 /* 0x4C69E */
            if (DSB(slot + 0x5Fu) == 1u)
                fight_4c784(side);                              /* 0x4C6A9 */
            else
                struck = 1;                                     /* 0x4C6B0 */
            break;
        default:                                                /* 1, 4, 6: 0x4C6B0 */
            struck = 1;
            break;
        }
    }
    if (!struck) return;                                        /* 0x4C6B7 */
    DSW(DS_00108898) = 0;                                       /* 0x4C6C6 */
    DSB(entry + 0x20u) = (u8)side;                              /* 0x4C6CD */
    DSB(entry + 0x1Cu) = (u8)(DSB(entry + 0x1Cu) & 0xF7u);      /* 0x4C6D6 */
    DSB(entry + 0x1Fu) = (u8)(DSB(entry + 0x1Fu) + 1u);         /* 0x4C6DE */
    {
        u32 k = DSB(DSD(entry + 8u) + 0x4Au);
        if (k != 0u) {                                          /* 0x4C6E5 */
            DSB(DSD(DS_001014F4) + k * 0x68u + 0x4Bu) = 0;      /* 0x4C702 */
            DSB(DSD(entry + 8u) + 0x2Au) &= 0xF7u;              /* 0x4C70A */
            DSB(DSD(entry + 8u) + 0x29u) &= 0xBFu;              /* 0x4C711 */
            DSB(DSD(entry + 8u) + 0x4Au) = 0;                   /* 0x4C718 */
            DSB(entry + 0x1Cu) = (u8)(DSB(entry + 0x1Cu) & 0xBFu);  /* 0x4C727 */
            DSB(DS_001088AE + (u32)DSB(entry + 0x21u)) =
                (u8)(DSB(DS_001088AE + (u32)DSB(entry + 0x21u)) + 1u); /* 0x4C72A */
        }
    }
    {
        u8 mv = DSB(DS_001077B0 + side * 0x94u + 0x5Fu);        /* 0x4C730..0x4C741 */
        if (mv >= 0x0Cu && mv <= 0x0Fu) {                       /* 0x4C74D/0x4C752 */
            fight_4cb18(entry, index, 1u, side);                /* 0x4C760 */
            DSB(entry + 0x1Cu) = (u8)(DSB(entry + 0x1Cu) | 0x08u);  /* 0x4C765 */
        } else {
            fight_4cb18(entry, index, 0u, side);                /* 0x4C774 */
        }
    }
}

void fight_effects_pass(void)
{
    /* The raw's frame locals the worshipper types write: [ESP+0xC] (type 9's
     * count, 0x4A128), [ESP+0x14] (type 11 sets it, 0x4A17A) and [ESP+0x18]
     * (type 11 clears it, 0x4A1E2). Mode 9 initialises them (0x49C8E..0x49CA6:
     * [ESP+0x18] = 1, [ESP+0x14] = 0, [ESP+0xC] = EDX with DX = 0); only the
     * mode-9 block reads them (0x4A549 as a word, 0x4A567, 0x4A56E). */
    /* PORT: outside mode 9 the raw leaves them unset; they start at 0 here, as
     * nothing reads them there. [ESP+0xC]'s high word (the caller's EDX high
     * half) is 0: the mode-9 block compares only AX. The prelude's per-side
     * words [ESP]/[ESP+2], the count [ESP+8] and case 14's [ESP+0x10] stay with
     * their named gaps. */
    u32 loc_idle = 0;                           /* [ESP+0xC] */
    u8 loc_walk = 0;                            /* [ESP+0x14] */
    u8 loc_still = 0;                           /* [ESP+0x18] */
    if (DSW(DS_00104B00) == 9u) {               /* 0x49C89 */
        loc_still = 1u;                         /* 0x49C94 */
        loc_walk = 0;                           /* 0x49C98 */
        loc_idle = 0;                           /* 0x49CA6 */
    }

    DSD(DS_00108874) = fight_midpoint();        /* 0x49CAA 0x493F0 */

    {
        u32 head = DSD(DS_0010884C);            /* 0x49CAF */
        if (head != DS_0010884C) {              /* 0x49CC5: empty list skips */
            for (;;) {
                u32 entry = head;
                u32 next = DSD(entry);          /* 0x49CD3 */
                u32 rec = DSD(entry + 8u);      /* 0x49CD8 */
                /* PORT: 0x49CDD..0x49CF8. The per-side counters (the frame
                 * locals only the mode-9 block reads) are a named gap (§7.4).
                 * The `si` the prelude and the handlers index with is
                 * (u16)(rec+0x48 - 0x20) (0x49CE1/0x49CF2). */
                u32 index = (u32)(u16)((u32)DSB(rec + 0x48u) - 0x20u);
                fight_4b69c(entry, index);      /* 0x49CFE */

                /* 0x49D03: AL = +0x1E; CMP AL,0xE; JA 0x49D1E. The jump table
                 * at 0x49C2C sends type 0 to the same 0x49D1E (0x4AAD0), so
                 * both type 0 and >0xE take it; types 1 (0x49D2F), 2
                 * (0x49D90), 3..7 (0x49DB3, 0x49E5A, 0x49EC0, 0x49F11,
                 * 0x49FC2), 8's gate (0x4A08A) and 9..12 (0x4A115, 0x4A131,
                 * 0x4A17A, 0x4A1EB; record §42-D) are ported. The type is read
                 * after the prelude, which can make it 6. */
                u8 type = DSB(entry + 0x1Eu);
                if (type == 0u || type > 0xEu) {
                    fight_4aad0(entry, index);  /* 0x49D1E */
                } else {
                switch (type) {
                case 1: {
                    /* 0x49D2F: the walk's arrival test. The actor stops once
                     * |actor+0x18 - entry+0x14| is within one step, the
                     * magnitude of the velocity word +0x34 (read as the high
                     * word of the dword at +0x32, 0x49D67/0x49D71 `mov eax,
                     * [eax+0x32]` then `sar eax,0x10`, negated when the word is
                     * negative: 0x49D60 `cmp word [eax+0x34],0`). The compare
                     * is signed (0x49D79 `jg`). */
                    if (DSB(DS_001088C2) != 0u) {           /* 0x49D2F */
                        if (fight_4bd4c(entry, index) != 0) break;  /* 0x49D3F */
                    }
                    s32 d = (s32)DSD(rec + 0x18u) - (s32)DSD(entry + 0x14u); /* 0x49D55 */
                    if (d < 0) d = -d;                      /* 0x49D5B */
                    s32 step = (s32)DSD(rec + 0x32u) >> 16; /* 0x49D6A/0x49D74 */
                    if ((s16)DSW(rec + 0x34u) < 0) step = -step;    /* 0x49D6D */
                    if (d <= step)                          /* 0x49D79 */
                        fight_4ac38(entry, index);          /* 0x49D86 */
                    break;
                }
                case 2: {
                    /* 0x49D90: the wait. The entry's +0x18 word counts down
                     * (signed, 0x49D99 `test bx,bx` / `jg`); at zero or below
                     * the actor arrives (0x4AC38). 0x4E5A4 sets the type
                     * (0x4E672), and 0x4DBEC (0x4DDB6, a register store) in
                     * modes 0x0D/0x32. */
                    u16 t = (u16)(DSW(entry + 0x18u) - 1u); /* 0x49D94 */
                    DSW(entry + 0x18u) = t;                 /* 0x49D95 */
                    if ((s16)t > 0) break;                  /* 0x49D9C */
                    fight_4ac38(entry, index);              /* 0x49DA9 */
                    break;
                }
                case 3:
                    /* 0x49DB3: the fall. The actor's +0x2C follows its y
                     * (0x496AC); once the y is at or below the zero-extended
                     * word DS_000BD898 (0x49DCD `mov ax,[0xbd898]` after `xor
                     * eax,eax`, 0x49DD6 signed `cmp`/0x49DD8 `jg`) it lands:
                     * hflip when 0x2BE1C > 0 (an OR only, 0x49E0B), the
                     * 0xC9634[si] stream at 2.0 (`push 0x40000000`), the
                     * velocities +0x38/+0x34 zeroed, the entry's +0x18 word
                     * timer = rng(0x3C) + 0x3C, +0x1C |= 0x80 and type 4. */
                    DSW(rec + 0x2Cu) = fight_dust_clamp((s32)DSD(rec + 0x30u) >> 16); /* 0x49DC4 */
                    if ((s32)DSD(rec + 0x30u) >> 16
                            > (s32)(u32)DSW(DS_000BD898))           /* 0x49DD8 */
                        break;
                    if (fight_2be1c(rec, DSD(DSD(entry + 0xCu))) > 0) /* 0x49DE6 */
                        DSW(rec + 0x28u) = (u16)(DSW(rec + 0x28u) | 0x4000u); /* 0x49E0D */
                    actors_anim_begin(rec, DSD(DS_000C9634 + index * 4u),
                                      0x40000000u);         /* 0x49E24 */
                    DSW(rec + 0x38u) = 0;                   /* 0x49E29 */
                    DSW(rec + 0x34u) = 0;                   /* 0x49E34 */
                    DSW(entry + 0x18u) = (u16)(rng_next(0x3Cu) + 0x3Cu); /* 0x49E3A 0x49E4E */
                    DSB(entry + 0x1Eu) = 4u;                /* 0x49E47 */
                    DSB(entry + 0x1Cu) = (u8)(DSB(entry + 0x1Cu) | 0x80u); /* 0x49E52 */
                    break;
                case 4: {
                    /* 0x49E5A: the lie. The entry's +0x18 word counts down
                     * (signed, 0x49E66 `jg`); at zero the actor takes the
                     * 0xC95EC[si] stream at 3.0, sets +0x38 = 0x40 and +0x34 =
                     * -0x40 when the fighter record's +0x28 word has bit 0x4000
                     * (0x49E94/0x49E96), else 0x40; +0x1C loses bit 0x80 and
                     * the entry becomes type 5. */
                    u16 t = (u16)(DSW(entry + 0x18u) - 1u);  /* 0x49E5E */
                    DSW(entry + 0x18u) = t;                  /* 0x49E5F */
                    if ((s16)t > 0) break;                   /* 0x49E66 */
                    actors_anim_begin(rec, DSD(DS_000C95EC + index * 4u),
                                      0x40400000u);          /* 0x49E80 */
                    DSW(rec + 0x38u) = 0x0040u;              /* 0x49E85 */
                    if ((DSW(DSD(DSD(entry + 0xCu)) + 0x28u) & 0x4000u) != 0u) /* 0x49E96 */
                        DSW(rec + 0x34u) = 0xFFC0u;          /* 0x49EA0 */
                    else
                        DSW(rec + 0x34u) = 0x0040u;          /* 0x49EA8 */
                    DSB(entry + 0x1Eu) = 5u;                 /* 0x49EB1 */
                    DSB(entry + 0x1Cu) = (u8)(DSB(entry + 0x1Cu) & 0x7Fu); /* 0x49EB8 */
                    break;
                }
                case 5:
                    /* 0x49EC0: the climb. +0x2C follows the y (0x496AC); once
                     * the y word +0x32 is back at the entry's +0x1A (signed,
                     * 0x49EDD `jl`), the actor takes the 0xC9544[si] stream at
                     * 3.0, stops (+0x38/+0x34 zeroed) and the entry returns to
                     * type 0. */
                    DSW(rec + 0x2Cu) = fight_dust_clamp((s32)DSD(rec + 0x30u) >> 16); /* 0x49ED1 */
                    if ((s16)DSW(rec + 0x32u) < (s16)DSW(entry + 0x1Au)) /* 0x49ED9 */
                        break;
                    actors_anim_begin(rec, DSD(DS_000C9544 + index * 4u),
                                      0x40400000u);          /* 0x49EF7 */
                    DSW(rec + 0x38u) = 0;                    /* 0x49EFC */
                    DSW(rec + 0x34u) = 0;                    /* 0x49F02 */
                    DSB(entry + 0x1Eu) = 0;                  /* 0x49F08 */
                    break;
                case 6: {
                    /* 0x49F11: the tumble 0x4B470 starts. While the actor's
                     * +0x36 word is negative (falling) the entry's +0x1C bit 7
                     * is set again, so 0x4B69C can re-hit it. The shadow
                     * actor (+0x10) follows the x dword and the y word +0x32.
                     * While the height +0x1C plus the +0x36 step (the high word
                     * of the dword at +0x34, 0x49F3A/0x49F40) stays positive,
                     * +0x36 falls by 0x10 a frame; at or below zero the shadow
                     * dies and the actor lands on the 0xC973C[si] stream at 2.0
                     * as type 8 (or, when that entry is 0, the 0xC9544[si]
                     * stream at 3.0 as type 4), with +0x1C, +0x36, +0x34 and the
                     * entry's hit count +0x1F zeroed. */
                    if ((s16)DSW(rec + 0x36u) < 0)           /* 0x49F16 */
                        DSB(entry + 0x1Cu) = (u8)(DSB(entry + 0x1Cu) | 0x80u); /* 0x49F18 */
                    u32 sh = DSD(entry + 0x10u);
                    if (sh != 0u) {                          /* 0x49F21 */
                        DSD(sh + 0x18u) = DSD(rec + 0x18u);  /* 0x49F29 */
                        DSW(sh + 0x32u) = DSW(rec + 0x32u);  /* 0x49F36 */
                    }
                    if (((s32)DSD(rec + 0x34u) >> 16)
                            + (s32)DSD(rec + 0x1Cu) > 0) {   /* 0x49F47 */
                        DSW(rec + 0x36u) = (u16)(DSW(rec + 0x36u) - 0x10u); /* 0x49FB8 */
                        break;
                    }
                    if (DSD(entry + 0x10u) != 0u) {          /* 0x49F4E */
                        actor_set_dead(DSD(entry + 0x10u));  /* 0x49F52 0x2B150 */
                        DSD(entry + 0x10u) = 0;              /* 0x49F57 */
                    }
                    {
                        u32 st = DSD(DS_000C973C + index * 4u); /* 0x49F66 */
                        if (st != 0u) {
                            actors_anim_begin(rec, st, 0x40000000u); /* 0x49F78 */
                            DSB(entry + 0x1Eu) = 8u;         /* 0x49F7D */
                        } else {
                            actors_anim_begin(rec, DSD(DS_000C9544 + index * 4u),
                                              0x40400000u);  /* 0x49F91 */
                            DSB(entry + 0x1Eu) = 4u;         /* 0x49F96 */
                        }
                    }
                    DSD(rec + 0x1Cu) = 0;                    /* 0x49F9A */
                    DSW(rec + 0x36u) = 0;                    /* 0x49FA1 */
                    DSW(rec + 0x34u) = 0;                    /* 0x49FAB */
                    DSB(entry + 0x1Fu) = 0;                  /* 0x49FAF */
                    break;
                }
                case 7: {
                    /* 0x49FC2: the flight across the screen (0x4987C sets the
                     * type, 0x49BF3). x = the actor's +0x3C dword. Inside
                     * (0, 0x5400) the entry's +0x1C bit 2 latches once with
                     * the voice. Inside [0, 0x5400] with +0x1C bit 0, the +0x34
                     * word (read as `[+0x32] >> 16`) steers: negative and above
                     * -0x100 it snaps to -0x100, at or below it falls by 1;
                     * non-negative and at most 0x100 it rises by 1, above it
                     * snaps to 0x100. Then at speed -0x100 with x < -0x300, or
                     * 0x100 with x > 0x5700 (the re-read speed; all signed),
                     * the actor's +0x28 byte gains bit 7. */
                    u32 actor = DSD(entry + 8u);
                    s32 x = (s32)DSD(actor + 0x3Cu);         /* 0x49FC5 */
                    if (x > 0 && x < 0x5400
                            && (DSB(entry + 0x1Cu) & 4u) == 0u) {   /* 0x49FCA..0x49FE1 */
                        DSB(entry + 0x1Cu) = (u8)(DSB(entry + 0x1Cu) | 4u); /* 0x49FEE */
                        /* PORT: 0x49FF1 0x2C3FC(0xDE) voice, not wired
                         * (record §45-A). */
                    }
                    if (x >= 0 && x <= 0x5400
                            && (DSB(entry + 0x1Cu) & 1u) != 0u) {   /* 0x49FF6..0x4A00F */
                        s32 v = (s32)DSD(actor + 0x32u) >> 16;      /* 0x4A014 */
                        if (v < 0) {                                /* 0x4A01C */
                            if (v > -0x100)                         /* 0x4A023 `jle` */
                                DSW(actor + 0x34u) = 0xFF00u;       /* 0x4A025 */
                            else
                                DSW(actor + 0x34u) = (u16)(DSW(actor + 0x34u) - 1u); /* 0x4A02D */
                        } else if (v > 0x100) {                     /* 0x4A038 `jle` */
                            DSW(actor + 0x34u) = 0x0100u;           /* 0x4A03A */
                        } else {
                            DSW(actor + 0x34u) = (u16)(DSW(actor + 0x34u) + 1u); /* 0x4A042 */
                        }
                    }
                    {
                        s32 v = (s32)DSD(actor + 0x32u) >> 16;      /* 0x4A049 */
                        if ((v == -0x100 && x < -0x300)             /* 0x4A04F/0x4A056 */
                                || (v == 0x100 && x > 0x5700))      /* 0x4A067/0x4A072 */
                            DSB(actor + 0x28u) = (u8)(DSB(actor + 0x28u) | 0x80u); /* 0x4A081 */
                    }
                    break;
                }
                case 8: {
                    /* 0x4A08A: the held worshipper (§42-C). Without +0x1C bit
                     * 6 (set by the grab arms 0x4B788 and 0x4D898) it does
                     * nothing; nor while the holder (side +0x20) still plays
                     * its hold (0x4AF04) or the actor has no +0x4A link.
                     * Otherwise it is released as 0x4B69C releases (the
                     * holder's +0x4B, +0x2A bit 3, +0x29 bit 6, +0x4A, the
                     * entry's bit 6, DS_001088AE counts and DS_001088B2 = 1)
                     * and 0x4B470 tramples it with EDX = EDI = si, without
                     * counting +0x1F. */
                    if ((DSB(entry + 0x1Cu) & 0x40u) == 0u)  /* 0x4A097 */
                        break;
                    if (fight_4af04((u32)DSB(entry + 0x20u)) != 0) /* 0x4A0A2 */
                        break;
                    u32 k = DSB(DSD(entry + 8u) + 0x4Au);    /* 0x4A0B2 */
                    if (k == 0u) break;                      /* 0x4A0B7 */
                    DSB(DSD(DS_001014F4) + k * 0x68u + 0x4Bu) = 0; /* 0x4A0CD */
                    DSB(DSD(entry + 8u) + 0x2Au) &= 0xF7u;   /* 0x4A0D5 */
                    DSB(DSD(entry + 8u) + 0x29u) &= 0xBFu;   /* 0x4A0DC */
                    DSB(DSD(entry + 8u) + 0x4Au) = 0;        /* 0x4A0E3 */
                    DSB(entry + 0x1Cu) = (u8)(DSB(entry + 0x1Cu) & 0xBFu); /* 0x4A0F2 */
                    DSB(DS_001088AE + (u32)DSB(entry + 0x21u)) =
                        (u8)(DSB(DS_001088AE + (u32)DSB(entry + 0x21u)) + 1u); /* 0x4A0F5 */
                    DSB(DS_001088B2 + (u32)DSB(entry + 0x21u)) = 1u; /* 0x4A100 */
                    fight_4b470(entry, index);               /* 0x4A10B */
                    break;
                }
                case 9:
                    /* 0x4A115: mode 9's idle (0x4AAD0 sets the type in mode 9,
                     * 0x4AAE6). While the word DS_001088B4 (set by the mode-9
                     * block, 0x4A559) is non-zero the entry becomes type 10;
                     * else it counts into [ESP+0xC] (a dword `inc`). */
                    if (DSW(DS_001088B4) != 0u)              /* 0x4A115 */
                        DSB(entry + 0x1Eu) = 0x0Au;          /* 0x4A11F */
                    else
                        loc_idle++;                          /* 0x4A128 */
                    break;
                case 10:
                    /* 0x4A131: with DS_001088C7 or DS_001088C8 set (0x4A928's
                     * flags), 0x4B430 may hold the actor (EBX = 1, 0x4A143);
                     * otherwise (or when it declines) the 0x4B2AC walk, type
                     * 11 and +0x1C bit 7 cleared. */
                    if (DSB(DS_001088C7) != 0u || DSB(DS_001088C8) != 0u) { /* 0x4A131/0x4A13A */
                        if (fight_4b430(entry, index, 1u) != 0) break;      /* 0x4A14F */
                    }
                    fight_4b2ac(entry, index);               /* 0x4A163 */
                    DSB(entry + 0x1Eu) = 0x0Bu;              /* 0x4A16B */
                    DSB(entry + 0x1Cu) = (u8)(DSB(entry + 0x1Cu) & 0x7Fu); /* 0x4A16F/0x4A172 */
                    break;
                case 11: {
                    /* 0x4A17A: the walk. [ESP+0x14] = 1; +0x2C follows the y
                     * (0x496AC); on arrival (0x4A7D4) the actor stops
                     * (+0x38/+0x34/+0x36 zeroed) and takes the 0xC9544[si]
                     * stream at 5.0 (EDX still holds si * 4 from 0x49D0F:
                     * 0x496AC and 0x4A7D4 push and pop it); still moving
                     * (+0x34 non-zero), [ESP+0x18] = 0. The type stays 11. */
                    loc_walk = 1u;                           /* 0x4A17A */
                    DSW(rec + 0x2Cu) = fight_dust_clamp((s32)DSD(rec + 0x30u) >> 16); /* 0x4A188/0x4A190 */
                    if (fight_4a7d4(entry) != 0) {           /* 0x4A196 */
                        DSW(rec + 0x38u) = 0;                /* 0x4A1A2 */
                        DSW(rec + 0x34u) = 0;                /* 0x4A1AB */
                        DSW(rec + 0x36u) = 0;                /* 0x4A1B4 */
                        actors_anim_begin(rec, DSD(DS_000C9544 + index * 4u),
                                          0x40A00000u);      /* 0x4A1C8 */
                    } else if (DSW(rec + 0x34u) != 0u) {     /* 0x4A1D5 */
                        loc_still = 0;                       /* 0x4A1E2 */
                    }
                    break;
                }
                case 12:
                    /* 0x4A1EB: the scatter (the mode-9 block sets every
                     * entry's type to 12, 0x4A583). By DS_001088CA the actor
                     * faces and walks left (zero: +0x29 |= 0x40, +0x34 =
                     * -0x80) or right (+0x29 &= 0xBF, +0x34 = 0x80), takes
                     * the 0xC95D4[si] stream at 3.0, and the entry becomes
                     * type 14 when DS_001088C6 is non-zero, else 13. */
                    if (DSB(DS_001088CA) == 0u) {            /* 0x4A1EB */
                        DSB(rec + 0x29u) = (u8)(DSB(rec + 0x29u) | 0x40u); /* 0x4A1F7 */
                        DSW(rec + 0x34u) = 0xFF80u;          /* 0x4A1FE */
                    } else {
                        DSB(rec + 0x29u) = (u8)(DSB(rec + 0x29u) & 0xBFu); /* 0x4A209 */
                        DSW(rec + 0x34u) = 0x0080u;          /* 0x4A210 */
                    }
                    actors_anim_begin(rec, DSD(DS_000C95D4 + index * 4u),
                                      0x40400000u);          /* 0x4A22A */
                    DSB(entry + 0x1Eu) = (DSB(DS_001088C6) != 0u)
                                       ? 0x0Eu : 0x0Du;      /* 0x4A22F..0x4A241 */
                    break;
                case 13: {
                    /* PORT: 0x4A24A..0x4A2F4. The case-13 body (0x2BE1C,
                     * 0x2BC30, the rec +0x3c/+0x2a/+0x32 gates, 0x2B150) is
                     * the named gap (§7.4); the 0x2BE00 arm is the draw gate. */
                    s32 r = fight_2be00(fight_case_rec());   /* 0x4A2F5 */
                    if (r > 0) (void)rng_next(0xC00u);       /* 0x4A305 */
                    else       (void)rng_next(0xC00u);       /* 0x4A315 */
                    break;
                }
                case 14: {
                    /* PORT: 0x4A346..0x4A412. The case-14 body (0x4A868,
                     * 0x2BC30, the rec +0x2a/+0x3c/+0x32 gates) is the named
                     * gap (§7.4); the 0x2BE00 arm is the draw gate. */
                    s32 r = fight_2be00(fight_case_rec());   /* 0x4A413 */
                    if (r > 0) (void)rng_next(0xC00u);       /* 0x4A439 */
                    else       (void)rng_next(0xC00u);       /* 0x4A449 */
                    break;
                }
                default:
                    break;
                }
                }

                head = next;                    /* 0x4A468 */
                if (head == DS_0010884C) break; /* 0x4A470 */
            }
        }
    }

    /* 0x4A476: the mode-9 block owns the 0x4A4C5/0x4A4F7 draws and the
     * DS_001088B0/0x1088C9/0x1088B4 traffic. The demo runs mode 3 and gates the
     * whole block out (0x4A481 `jne 0x4A591`), so those two sites are not
     * issued in cycle 1. */
    /* PORT: 0x4A487..0x4A58F (mode 9 only) — named gap (§7.4). It is the only
     * reader of the three frame locals above. */
    (void)loc_idle;
    (void)loc_walk;
    (void)loc_still;

    fight_4a634();                              /* 0x4A591 */
    DSB(DS_001088C2) = 0;                       /* 0x4A5A0 */

    /* 0x4A5A6: the tail behind the DS_001088BF 1..4 gate, for modes other
     * than 7/8/9. DS_001088BF is not cleared on that early-skip path. The
     * jump table 0x49C68 (0x4A5DC, 0x4A5E3, 0x4A5F4, 0x4A605) gives
     * 0x4987C(rng(2), count, kind) the (count, kind) pairs (1, 1), (1, 2),
     * (2, 1) and (2, 2) for DS_001088BF 1..4 (record §43-A). */
    if (DSW(DS_00104B00) != 9 && DSW(DS_00104B00) != 8
            && DSW(DS_00104B00) != 7) {
        u8 bh = (u8)DSB(DS_001088BF);
        if (bh != 0) {
            if ((u8)(bh - 1u) <= 3u) {
                s32 count = (bh <= 2u) ? 1 : 2;             /* EDX */
                u32 kind = ((bh & 1u) != 0u) ? 1u : 2u;     /* EBX */
                u32 side = rng_next(2u);                    /* 0x4A611 */
                fight_4987c(side, count, kind);             /* 0x4A616 */
            }
            DSB(DS_001088BF) = 0;               /* 0x4A61B */
        }
    }
    DSB(DS_00104AEC) = (u8)(DSB(DS_00104AEC) & 0x7Fu);   /* 0x4A623 */
}

/* ---- 0x263F4 the arena frame ------------------------------------------- */

void fight_arena_frame(void)
{
    fight_slot_clear();                                        /* 0x263F7 */
    camera_screen_base(0, (s32)DSB(DS_0010782A));              /* 0x2640B */
    camera_screen_base(1, (s32)DSB(DS_001078BE));              /* 0x26422 */

    /* 0x2642C/0x2643A: the two previous-position latches. */
    DSD(DS_001077E8) = DSD(DS_001077E4);
    DSD(DS_0010787C) = DSD(DS_00107878);

    camera_project(0, DS_00100B08, DS_00100B00, DS_00100B62,
                   DS_00100B60, DS_00100AF0);                  /* 0x26450 */
    camera_project(1, DS_00100B0C, DS_00100B04, DS_00100B63,
                   DS_00100B61, DS_00100AF4);                  /* 0x26473 */
    camera_decay();                                            /* 0x26478 0x17580 */
    fighter_pass_a();                                          /* 0x2647D 0x1958C */
    fighter_pass_b(0);                                         /* 0x26484 0x19068 */

    camera_project(0, DS_00100B08, DS_00100B00, DS_00100B62,
                   DS_00100B60, DS_00100AF0);                  /* 0x264A4 */
    camera_project(1, DS_00100B0C, DS_00100B04, DS_00100B63,
                   DS_00100B61, DS_00100AF4);                  /* 0x264C7 */

    fighter_think();                                           /* 0x264CC 0x1975C */

    camera_project(0, DS_00100B08, DS_00100B00, DS_00100B62,
                   DS_00100B60, DS_00100AF0);                  /* 0x264EC */
    camera_project(1, DS_00100B0C, DS_00100B04, DS_00100B63,
                   DS_00100B61, DS_00100AF4);                  /* 0x2650F */

    fight_slot_pass();                                         /* 0x26514 0x3CB68 */
    fight_hud_pass(0);                                         /* 0x2651B 0x35658 */
    fight_hud_pass(1);                                         /* 0x26525 0x35658 */
    fight_effects_pass();                                      /* 0x2652A 0x49C78 */
    camera_scene_step();                       /* 0x2652F 0x1282C + 0x26534 0x12DA8 */
}

/* ---- 0x4E11C, the fight frame's entry to mode 0x25 (record §48-K) ------- */

#define DS_000C980C 0x000C980Cu   /* no symbols.h name: 0x4E350's x step (0x100) */

/* 0x4E27C — record §48-K. For each entry on the active list DS_0010884C
 * (none when the head is the sentinel; the next pointer is read before the
 * call), on its actor R (+8): with the signed DS_000F0AF0 > 0 the entry's
 * +0x14 = DS_000F0AF0 - 0x8180, R's +0x34 = 0xFF80 and +0x29 |= 0x40; else
 * +0x14 = DS_000F0AF0 + 0x8180, +0x34 = 0x80 and +0x29 &= 0xBF. Then R
 * begins 0xC95D4[(u16)(R+0x48 - 0x20)] at 3.0 (the index is the low word of
 * ECX = EDX - 0x20, EDX's low byte R+0x48 and DH = 0). EBX/ECX/EDX are pushed
 * and popped; 0x2BC30 keeps EBX (the next entry). Only caller: 0x4E361
 * (0x4E350 with AL != 0). */
void fight_mode25_face(void)
{
    u32 node = DSD(DS_0010884C);                        /* 0x4E27F */
    if (node == DS_0010884C) return;                    /* 0x4E284/0x4E289 */
    do {
        u32 next = DSD(node);                           /* 0x4E28E */
        u32 idx = (u32)(u16)((u32)DSB(DSD(node + 8u) + 0x48u) - 0x20u);  /* 0x4E28B..0x4E29D */
        s32 cx = (s32)DSD(DS_000F0AF0);                 /* 0x4E297 */
        if (cx > 0) {                                   /* 0x4E2A0/0x4E2A2 */
            DSD(node + 0x14u) = (u32)cx - 0x8180u;      /* 0x4E2A4/0x4E2AA */
            DSW(DSD(node + 8u) + 0x34u) = 0xFF80u;      /* 0x4E2AD/0x4E2B0 */
            DSB(DSD(node + 8u) + 0x29u) = (u8)(DSB(DSD(node + 8u) + 0x29u) | 0x40u);  /* 0x4E2B6/0x4E2B9 */
        } else {
            DSD(node + 0x14u) = (u32)cx + 0x8180u;      /* 0x4E2BF/0x4E2C5 */
            DSW(DSD(node + 8u) + 0x34u) = 0x0080u;      /* 0x4E2C8/0x4E2CB */
            DSB(DSD(node + 8u) + 0x29u) = (u8)(DSB(DSD(node + 8u) + 0x29u) & 0xBFu);  /* 0x4E2D1/0x4E2D4 */
        }
        actors_anim_begin(DSD(node + 8u), DSD(DS_000C95D4 + idx * 4u),
                          0x40400000u);                 /* 0x4E2D8..0x4E2EC 0x2BC30 */
        node = next;                                    /* 0x4E2F1 */
    } while (node != DS_0010884C);                      /* 0x4E2F3/0x4E2F9 */
}

/* 0x4E350 — record §48-K. AL = `fresh` (kept at [ESP+0xC]). With it, 0x4E27C
 * first. DS_001088B9 = DS_001088B8 = 0 (DL). Then for i = 0..9 (EDI = i * 4):
 * - without `fresh` and with DS_0010839C[i] set, that entry E is kept;
 * - else E is the free list DS_001083C4's head (the function returns at
 *   once when the list is empty), unlinked (0x249D0) and put after
 *   DS_0010884C (0x249B0), and the descriptor D = 0xC9524[rng(6)] gets +0x10
 *   = 0x29CDC(0, slot 0's +0x7A);
 * - E's +0x1A = i; with `fresh` or DS_0010839C[i] still 0, D is spawned
 *   with EDX = DS_000F0AF0 - 0x3900, ECX = y and EBX = 0 (actor_spawn's a2,
 *   a3, a4; a5 = the pushed 0), y = W + j * k by the jump table 0x4E300 (W
 *   the zero-extended word DS_000BD898, k the dword 0xC980C; j = 0, 1, -1,
 *   2, 0, -2, -3, -1, 1, 3 for i = 0..9); E's +8 = the actor R and R's +0x14
 *   = E;
 * - E's +0x14 = 0x162F, 0xFCA (i 1..2), 0x965 (3..5) or 0x300 (6..9) by the
 *   table 0x4E328; +0x1F = +0x21 = 0 (AH), +0x0C = 0x1077B0, R's +0x2C =
 *   0xA00, R's +0x29 |= 0x10, E's +0x1C word = 0, +0x10 = 0; with slot 0's
 *   record's +0x51 non-zero R's +0x2E += 4 and +0x4E = 1; R's +0x29 &= 0xBF;
 *   E's +0x1C |= 2;
 * - with `fresh` or DS_0010839C[i] still 0: E's +0x1E = 0, R's +0x34 = 0x100,
 *   R begins 0xC95D4[(u16)(R+0x48 - 0x20)] at 3.0 (EDX = EAX after `mov
 *   al,[eax+0x48]; xor ah,ah`, less 0x20, `and edx,0xffff`) and
 *   DS_0010839C[i] = E; else E's +0x1E = 1.
 * EBX..EBP are pushed and popped. Callers: 0x4E271 (0x4E11C, AL = 1), and the
 * unported 0x4E987 (0x4E67C). */
void fight_mode25_spawn(u32 fresh)
{
    u32 i, node, desc = 0u;
    int fr = (u8)fresh != 0u;                           /* 0x4E359/0x4E35D */
    if (fr) fight_mode25_face();                        /* 0x4E361 0x4E27C */
    DSB(DS_001088B9) = 0u;                              /* 0x4E36C */
    DSB(DS_001088B8) = 0u;                              /* 0x4E372 */
    for (i = 0; i < 10u; i++) {                         /* 0x4E384..0x4E592 */
        u32 y, k, w, ebp, rec, b48;
        if (!fr && DSD(DS_0010839C + i * 4u) != 0u) {   /* 0x4E384..0x4E393 */
            node = DSD(DS_0010839C + i * 4u);           /* 0x4E395 */
        } else {
            node = DSD(DS_001083C4);                    /* 0x4E399 */
            if (node == DS_001083C4) node = 0u;         /* 0x4E39F..0x4E3A7 */
            else effects_list_unlink(node);             /* 0x4E3AB/0x4E3AD 0x249D0 */
            if (node == 0u) return;                     /* 0x4E3B4/0x4E3B6 */
            effects_list_insert_after(DS_0010884C, node);   /* 0x4E3BC/0x4E3C1 0x249B0 */
            desc = DSD(DS_000C9524 + rng_next(6u) * 4u);    /* 0x4E3C6..0x4E3D7 0x5D7DC */
            DSD(desc + 0x10u) = fight_dust_value(0u, (u32)DSB(DS_001077B0 + 0x7Au));  /* 0x4E3DB..0x4E3EE 0x29CDC */
        }
        DSW(node + 0x1Au) = (u16)i;                     /* 0x4E3F1/0x4E3FB */
        k = DSD(DS_000C980C);                           /* 0x4E40E/0x4E41E */
        w = DSW(DS_000BD898);                           /* 0x4E42F.. */
        switch (i) {                                    /* 0x4E426 jmp [0x4E300 + i * 4] */
        case 1: case 8: y = w + k; break;               /* 0x4E438 */
        case 2: case 7: y = w - k; break;               /* 0x4E463 */
        case 3:         y = 2u * k + w; break;          /* 0x4E44A */
        case 5:         y = w - 2u * k; break;          /* 0x4E476 */
        case 6:         y = w - 3u * k; break;          /* 0x4E484 */
        case 9:         y = w + 3u * k; break;          /* 0x4E456 */
        default:        y = w; break;                   /* 0x4E42D (i 0, 4) */
        }
        if (fr || DSD(DS_0010839C + i * 4u) == 0u) {    /* 0x4E48F..0x4E49D */
            rec = actor_spawn((const u32 *)(mem + desc),
                              DSD(DS_000F0AF0) - 0x3900u, y, 0u, 0u);   /* 0x4E3F5/0x4E403, 0x4E49F..0x4E4A7 0x2AE14 */
            DSD(node + 8u) = rec;                       /* 0x4E4AC */
            DSD(rec + 0x14u) = node;                    /* 0x4E4AF */
        }
        switch (i) {                                    /* 0x4E4B9 jmp [0x4E328 + i * 4] */
        case 0:                 ebp = 0x162Fu; break;   /* 0x4E4C0 */
        case 1: case 2:         ebp = 0x0FCAu; break;   /* 0x4E4C7 */
        case 3: case 4: case 5: ebp = 0x0965u; break;   /* 0x4E4CE */
        default:                ebp = 0x0300u; break;   /* 0x4E4D5 (i 6..9) */
        }
        rec = DSD(node + 8u);                           /* 0x4E4DA */
        DSD(node + 0x14u) = ebp;                        /* 0x4E4DD */
        b48 = DSB(rec + 0x48u);                         /* 0x4E4E0 */
        DSB(node + 0x1Fu) = 0u;                         /* 0x4E4E5 */
        DSB(node + 0x21u) = 0u;                         /* 0x4E4E8 */
        DSD(node + 0x0Cu) = DS_001077B0;                /* 0x4E4F0 */
        DSW(DSD(node + 8u) + 0x2Cu) = 0x0A00u;          /* 0x4E4ED/0x4E4F7 */
        DSB(DSD(node + 8u) + 0x29u) = (u8)(DSB(DSD(node + 8u) + 0x29u) | 0x10u);   /* 0x4E4FD/0x4E500 */
        DSW(node + 0x1Cu) = 0u;                         /* 0x4E504 */
        DSD(node + 0x10u) = 0u;                         /* 0x4E50D */
        if (DSB(DSD(DS_001077B0) + 0x51u) != 0u) {      /* 0x4E50A..0x4E51E */
            DSW(DSD(node + 8u) + 0x2Eu) = (u16)(DSW(DSD(node + 8u) + 0x2Eu) + 4u);  /* 0x4E520/0x4E523 */
            DSB(DSD(node + 8u) + 0x4Eu) = 1u;           /* 0x4E528/0x4E52B */
        }
        DSB(DSD(node + 8u) + 0x29u) = (u8)(DSB(DSD(node + 8u) + 0x29u) & 0xBFu);   /* 0x4E52F/0x4E532 */
        DSB(node + 0x1Cu) = (u8)(DSB(node + 0x1Cu) | 2u);   /* 0x4E536..0x4E540 */
        if (fr || DSD(DS_0010839C + i * 4u) == 0u) {    /* 0x4E53C..0x4E54E */
            DSB(node + 0x1Eu) = 0u;                     /* 0x4E550 */
            DSW(DSD(node + 8u) + 0x34u) = 0x0100u;      /* 0x4E554/0x4E55C */
            actors_anim_begin(DSD(node + 8u),
                              DSD(DS_000C95D4 + (u32)(u16)(b48 - 0x20u) * 4u),
                              0x40400000u);             /* 0x4E519, 0x4E557..0x4E572 0x2BC30 */
            DSD(DS_0010839C + i * 4u) = node;           /* 0x4E577 */
        } else {
            DSB(node + 0x1Eu) = 1u;                     /* 0x4E57F */
        }
    }
}

/* 0x4E11C — record §48-K. The fight frame's entry to mode 0x25, on its gate
 * (DS_001078FA == 2, the word DS_00108892 >= 6, slot 0's +0x53 == 0):
 * - DS_00104B1D 3 or 2: the word DS_00108892 - 1 only (0x4E131);
 * - unless the signed DS_000F0AF0 is within [-0x3300, 0x3300], the signed
 *   DS_00104AD8 <= 0, DS_00104ABC >= 2 (unsigned, 0x4E163 `jc`), both
 *   characters are 4 and DS_001088C4 == 0: the same decrement (0x4E189);
 * - else, in raw order: DS_00104B25 = 0 (DH, the DS_001088C4 byte just
 *   tested), DS_001088C4 = 1, the words DS_001088A6 = 0xB4 (SI) and
 *   DS_0010889A = 0 (DI), DS_00108892 = 0 (DX), DS_00104B15 = 0, DS_001088C0
 *   = 1 (CH), DS_00104AEC &= 0xFE (AL, read at 0x4E19D), DS_001088BC = 0,
 *   the mode word = 0x25 (BX), DS_00108896 = 0 (CX), 0x34D8C(0),
 *   DS_001088BD = DS_001088BE = 0 (DH survives 0x34D8C, which pushes and
 *   pops EDX), DS_00108884 = DS_000F0AF0; for side 0..1 0x38154(side) and
 *   the five bytes DS_00108888 + side * 5 .. + 4 = 0 (EDX = 5 * (side + 1),
 *   CL = 0; 0x38154 pushes and pops EBX/ECX/EDX); the string 0x61 by 0x2F510
 *   at col -1, row 6 (EDX, kept by 0x1C500) with mode 0x4000 (ECX; EBX = 2,
 *   the loop's end, is not read by 0x1C500); 0x4E350(1).
 * EBX/ECX/EDX/ESI/EDI are pushed and popped. Only caller: 0x263EA
 * (0x26254). */
void fight_mode25_enter(void)
{
    u8 b1d = DSB(DS_00104B1D);                          /* 0x4E121 */
    s32 cx;
    u32 side;
    if (b1d == 3u || b1d == 2u) {                       /* 0x4E127..0x4E12F */
        DSW(DS_00108892) = (u16)(DSW(DS_00108892) - 1u);   /* 0x4E131 */
        return;                                         /* 0x4E138 */
    }
    cx = (s32)DSD(DS_000F0AF0);                         /* 0x4E13D */
    if (cx < -0x3300 || cx > 0x3300                     /* 0x4E143..0x4E151 */
            || (s32)DSD(DS_00104AD8) > 0                /* 0x4E153/0x4E15A */
            || DSD(DS_00104ABC) < 2u                    /* 0x4E15C/0x4E163 */
            || DSB(DS_0010782A) != 4u                   /* 0x4E165..0x4E170 */
            || DSB(DS_001078BE) != 4u                   /* 0x4E172..0x4E17D */
            || DSB(DS_001088C4) != 0u) {                /* 0x4E17F..0x4E187 */
        DSW(DS_00108892) = (u16)(DSW(DS_00108892) - 1u);   /* 0x4E189 */
        return;
    }
    DSB(DS_00104B25) = 0u;                              /* 0x4E1A2 */
    DSB(DS_001088C4) = 1u;                              /* 0x4E1AE */
    DSW(DS_001088A6) = 0xB4u;                           /* 0x4E1B6 */
    DSW(DS_0010889A) = 0u;                              /* 0x4E1BD */
    DSW(DS_00108892) = 0u;                              /* 0x4E1CB */
    DSB(DS_00104B15) = 0u;                              /* 0x4E1D2 */
    DSB(DS_001088C0) = 1u;                              /* 0x4E1D8 */
    DSB(DS_00104AEC) = (u8)(DSB(DS_00104AEC) & 0xFEu);  /* 0x4E19D/0x4E1B4/0x4E1DE */
    DSB(DS_001088BC) = 0u;                              /* 0x4E1E3 */
    DSW(DS_00104B00) = 0x25u;                           /* 0x4E1C6/0x4E1E9 */
    DSW(DS_00108896) = 0u;                              /* 0x4E1F4 */
    hit_flash_pair(0u);                                 /* 0x4E1F2/0x4E1FB 0x34D8C */
    DSB(DS_001088BD) = 0u;                              /* 0x4E200 */
    DSB(DS_001088BE) = 0u;                              /* 0x4E206 */
    DSD(DS_00108884) = DSD(DS_000F0AF0);                /* 0x4E20C/0x4E214 */
    for (side = 0; side < 2u; side++) {                 /* 0x4E212..0x4E24A */
        u32 e = DS_00108888 + side * 5u;                /* 0x4E21E EDX = 5 * (side + 1), + 0x108883 */
        fighter_38154(side);                            /* 0x4E21C/0x4E223 0x38154 */
        DSB(e + 4u) = 0u;                               /* 0x4E228 */
        DSB(e + 3u) = 0u;                               /* 0x4E22E */
        DSB(e + 2u) = 0u;                               /* 0x4E234 */
        DSB(e + 1u) = 0u;                               /* 0x4E23A */
        DSB(e) = 0u;                                    /* 0x4E241 */
    }
    text_cursor_hold_font2(-1, 6, game_string_get(0x61u), 0x4000u);  /* 0x4E24C..0x4E267 0x1C500, 0x2F510 */
    fight_mode25_spawn(1u);                             /* 0x4E26C/0x4E271 0x4E350 */
}
