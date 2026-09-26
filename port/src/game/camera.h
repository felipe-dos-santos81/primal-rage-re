/* port/src/game/camera.h
 * The fight camera state machine and the 16.16 projection. The raw derivations
 * are in docs/superpowers/plans/2026-09-20-demo-fight-derivations.md §1-§3.
 * The raw registers no camera init (neither process table contains a camera
 * function; 0x1324C is already effects.c's update-table entry 0), so this
 * module has no init and registers nothing. */
#ifndef PRAGE_GAME_CAMERA_H
#define PRAGE_GAME_CAMERA_H

#include "../types.h"

/* 0x17FA0. The per-frame projection for one side. The six arguments are the
 * raw's four registers plus two stack words (ret 8):
 *   side      EAX   0 / 1
 *   out_a     EDX   &DS_00100B08 (side 0) / &DS_00100B0C (side 1)
 *   out_b     EBX   &DS_00100B00 (side 0) / &DS_00100B04 (side 1)
 *   facing    ECX   &DS_00100B62 / &DS_00100B63 (record's +0x28 bit 0x4000)
 *   page_flag [esp] &DS_00100B60 / &DS_00100B61 (set 0, raised to 1 when the
 *                   0x164F4 tail succeeds)
 *   index_out [esp+8] &DS_00100AF0 / &DS_00100AF4
 * `out_a`/`out_b` are written ((record actor +4/+8 + 0x20) >> 6) minus the
 * sprite origin resolved through 0x16308; when 0x16308 cannot resolve the
 * paged resource (res_resolve NULL) the subtraction is skipped, which is how a
 * unit test pins the statically-determinable stage. The two "world anchor"
 * globals DS_00100AA0..DS_00100AAC are updated from the slot[0]/slot[1]
 * secondary pointers (DS_001077B8/DS_0010784C) regardless of `side`. The
 * 0x180C9/0x18108 tail fills DS_00100AC0/DS_00100AC8[side] and *page_flag
 * through 0x16AFC/0x164F4. */
void camera_project(u32 side, u32 out_a, u32 out_b, u32 facing, u32 page_flag,
                    u32 index_out);

/* 0x16734. The per-character sprite-id map 0x16AFC runs: switch on slot[side]'s
 * character, compare the actor's sprite id (word[actor] & 0x7FFF) against the
 * case's literal ids, return a code 0xD2..0xDA or -1. */
int camera_sprite_code(u32 side);

/* 0x16AFC. The first page-flag tail 0x17FA0 calls: write the side's screen box
 * to `out` (DS_00100A78 + side*4) and return 1, else zero it and return 0.
 * Uses the 0xCC300 frame table on a 0x16734 code, else the per-side countdown
 * (bit 2/3 of DS_00107EE0) and the 0x98688 scan cached in 0xFD120..0xFD138. */
int camera_page_tail_a(u32 side, u32 out);

/* 0x164F4. The 0x16AFC twin 0x17FA0 calls: skips 0x16734, uses bits 0/1 of
 * DS_00107EE0, scans the 0x96108 table and caches into 0xFD140..0xFD158.
 * Writes the side's screen box to `out` (DS_00100A90 + side*4). */
int camera_page_tail_b(u32 side, u32 out);

/* 0x17EEC. The per-character ground constant for character index `ch` (the
 * slot's +0x7A byte). Char 0 and char > 6 return word[0xE6DD0]. Shared with
 * 0x33F08, which selects from the same table. */
u32 camera_char_const(u32 ch);

/* 0x12D48. The per-frame mode switch on DS_000F0AFE:
 * 0 -> 0x12DF0, 1 -> 0x12E3C, 2 -> 0x13290, 3 -> 0x1333C, else nothing; then
 * DS_000F0AF0 = clamp(DS_000F0AF0, -0x5D00, +0x5D00). */
void camera_dispatch(void);

/* 0x12C70. The camera-x step seed: DS_000F0AFC = 0x400. 0x20DF4 (the state-6
 * fight reset) is its only caller (0x20E6A). */
void camera_step_seed(void);

/* 0x17580. The per-frame projection decay: four word countdowns, three globals
 * zeroed, four signed truncating multiplies by 0xF3D/0xD56, then the 0x140E4
 * box-overlap gate: when the boxes overlap, 0x170A0(0)/0x170A0(1) run for the
 * sides whose DS_00100B60/DS_00100B61 gate is set. */
void camera_decay(void);

/* 0x170A0. The unfreeze half's per-side body: clip the side's screen box
 * against the projected sprite, sync the three visible rectangles (0x181D0),
 * and write DS_00100AF8[side] = DS_00100B54. Guarded on the other side's two
 * hit-stun/recovery countdowns. */
void camera_unfreeze(u32 side);

/* 0x17CB0. The projectile collision step fighter_think (0x1975C) runs first:
 * zero DS_00100AD0/AD4, then test two live projectiles against each other
 * (0x17BC8, which bursts/kills both on a hit) or each side's live projectile
 * against the other fighter (0x176CC, which writes the overlap count
 * DS_00100B54 into DS_00100AD0[side]). */
void camera_projectile_step(void);

/* 0x17D30. The point (x, y) (the sign-extended words, world units; `tall` the
 * BX word) against both fighters' 0x100AC8 boxes through 0x1790C: bit 0 = side
 * 0 hit, bit 1 = side 1 hit. Writes the 0x100B10..0x100B54 sync/overlap
 * scratch and the 0x100BD3 plane; restores 0x100B00..0x100B0C and the two flip
 * bytes 0x100B62/0x100B63. The effects pass's per-entry prelude 0x4B69C calls
 * it with the worshipper's pset point. */
u32 camera_point_hit(s32 x, s32 y, u32 tall);

/* 0x140E4. 1 iff the two actors' screen boxes overlap. `actor0`/`actor1` are
 * the raw's actor-table indices (slot+0x56), not side numbers. Writes no
 * global; camera_decay's 0x17698 gate. */
int camera_box_overlap(u32 actor0, u32 actor1);

/* 0x16D58. Per-side screen base. `side` and `character` are the raw's AX/DX
 * 16-bit signed values; out of [0,1]x[0,0xA) writes nothing. */
void camera_screen_base(s32 side, s32 character);

/* 0x12750. Builds the type-0x01 node lists: the in-use sentinel 0xF0AE0 and the
 * free sentinel 0xF0A78 self-linked, then the eight 12-byte nodes
 * 0xF0A80..0xF0AD4 tail-appended to the free list. 0x127C0 (the 0xBB254 actor's
 * cb1) pops that list, so without it 0x1282C's spawn is always refused. 0x20DF4
 * (the state-6 fight reset) calls it at 0x20E33. */
void camera_dust_list_init(void);

/* 0x1282C then 0x12DA8, the pair 0x263F4 calls at the end of the frame: the
 * gated dust spawn (every 64th frame, then rng(7)&3 == 0) followed by the
 * selected-player-y commit and the camera-y clamp. */
void camera_scene_step(void);

#endif /* PRAGE_GAME_CAMERA_H */
