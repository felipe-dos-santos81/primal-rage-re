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
 *   page_flag [esp] &DS_00100B60 / &DS_00100B61 (set 0; the 0x16AFC/0x164F4
 *                   tail that would raise it is an unported gap §7.2)
 *   index_out [esp+8] &DS_00100AF0 / &DS_00100AF4
 * `out_a`/`out_b` are written ((record actor +4/+8 + 0x20) >> 6) minus the
 * sprite origin resolved through 0x16308; when 0x16308 cannot resolve the
 * paged resource (res_resolve NULL) the subtraction is skipped, which is how a
 * unit test pins the statically-determinable stage. The two "world anchor"
 * globals DS_00100AA0..DS_00100AAC are updated from the slot[0]/slot[1]
 * secondary pointers (DS_001077B8/DS_0010784C) regardless of `side`. */
void camera_project(u32 side, u32 out_a, u32 out_b, u32 facing, u32 page_flag,
                    u32 index_out);

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
 * zeroed, four signed truncating multiplies by 0xF3D/0xD56, then the
 * 0x140E4/0x170A0 tail (unported gap). */
void camera_decay(void);

/* 0x16D58. Per-side screen base. `side` and `character` are the raw's AX/DX
 * 16-bit signed values; out of [0,1]x[0,0xA) writes nothing. */
void camera_screen_base(s32 side, s32 character);

/* 0x1282C then 0x12DA8, the pair 0x263F4 calls at the end of the frame: the
 * gated dust spawn (every 64th frame, then rng(7)&3 == 0) followed by the
 * selected-player-y commit and the camera-y clamp. */
void camera_scene_step(void);

#endif /* PRAGE_GAME_CAMERA_H */
