/* port/src/game/attract.h
 * The small attract/boot units: the per-bit scene tick, the state reset, the
 * voice/rng scheduler, the per-state pause/continue tails and the
 * config-derived volumes. The 0x11000 phase machine lives here too but is
 * declared in the task that ports it. */
#ifndef PRAGE_GAME_ATTRACT_H
#define PRAGE_GAME_ATTRACT_H

#include "../types.h"

/* 0x292AC. For each set bit of DS_00104AD0, call the original function at
 * DS_000A8744 + i*4 with i the bit's byte offset. */
void attract_scene_tick(void);

/* 0x4F7F4. The attract palette advance: while the palette entry DS_000F0A48 is
 * non-zero, enqueue the next handle DS_000C98A0[counter] through 0x33874 and
 * step the counter; clear DS_00104AD0 bit 0 once the counter reaches 10. */
void attract_palette_advance(void);

/* 0x4F83C. The attract palette start (a distinct code-pointer target, RET at
 * 0x4F88C): set DS_00104AD0 bit 0, zero the counter, and while the entry is
 * non-zero enqueue DS_000C98A0[0] and set the counter to 1; clear bit 0 at
 * function level when the counter is not < 10. */
void attract_palette_start(void);

/* 0x10EE4. Reset the state machine to attract: the clock helper 0x32970(0),
 * DS_00104B15 = 0 (0x4F1E4), actors_reset() (0x2BAF4 with eax = 1), then
 * DS_00104B00 = 3, DS_000F0A64 = 0, DS_000F0A71 = 0, DS_000F0A6F = 0. */
void attract_state_reset(void);

/* 0x10F28. The attract's voice scheduler: two signed 16-bit countdowns
 * DS_000F0A60/DS_000F0A62 that reload as rng_next(0x2D)+0x2D and
 * rng_next(0x3C)+0x3C. Each expiry draws 0x2C3FC (voice, out of scope). */
void attract_voice_tick(void);

/* 0x10DB0. The per-state pause tail: when DS_000F0A71 == 0 and the two input
 * bits (DS_001088D8 byte 3 bit 0x20, byte 1 bit 0x10) are set, latch
 * DS_000F0A71 and move to state 4. */
void frontend_pause_tail(void);

/* 0x10E18. The per-state continue tail: same gate with byte 3 bit 0x10 and
 * byte 1 bit 0x20; moves to state 5. */
void frontend_continue_tail(void);

/* 0x2C8F0 with eax = -2. Recompute the music/SFX volumes DS_000A2CB8 and
 * DS_000A2CB4 from config fields 0x2A (scale), 0x35 and 0x37. */
void attract_config_volumes(void);

/* 0x11000. The 13-phase attract sub-machine. DS_000F0A6F selects the phase
 * through the jump table at 0x10FCC (0 -> 0x1101F ... 0xC -> 0x11531); a value
 * > 0xC skips straight to the 0x11550 tail. Every phase falls through to that
 * tail, which runs the 0x10F28 voice scheduler while DS_0009AD58 == 0. */
void attract_step(void);

/* PORT: the game directory the phase-0 boot-logo movies (0x1C740 handed
 * mem + 0x80038 / +0x80044, the fixed names "twi5.smk" and "twg.smk") are
 * played from. Set from flow.c's game_set_game_dir(). */
void attract_set_media_dir(const char *dir);

#endif /* PRAGE_GAME_ATTRACT_H */
