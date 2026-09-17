/* Top-level game flow: the init chain (0x1BEC4), the master frame loop
 * (0x255CC), the per-frame update (0x24C5C) and the state machine (0x11D04).
 * This is the Task 14 seam that turns the platform modules into a running game:
 * the title-screen path is live; every callee owned by a later sub-project is
 * stubbed with a PORT marker in flow.c naming the sub-project. */
#ifndef PR_GAME_FLOW_H
#define PR_GAME_FLOW_H

#include "types.h"

/* The port of the original's game main, 0x1BEC4. Runs the init chain, then the
 * frame loop until it quits, then the teardown of 0x1BE30. Returns the process
 * exit code (always 0; the original's fatal path exits the process instead).
 * The SDL host must already be initialised by the caller (main.c). */
int game_main(void);

/* 0x1BEC4's init chain and 0x1BE30's teardown, split out of game_main() so a
 * headless `--check` can run the init once and then drive game_loop() frame by
 * frame without the teardown stopping the music between calls. game_init()
 * requires game_set_game_dir() first. game_main() is game_init() ->
 * game_loop() -> game_shutdown(). */
void game_init(void);
void game_shutdown(void);

/* Tells game_main() which directory holds the INDEX-listed resources
 * (data/game/C). Must be called before game_main(). */
void game_set_game_dir(const char *dir);

/* 0x255CC: the master frame loop (input pump -> frame -> render -> present ->
 * pacing) until the quit flag DS_000A81A8 is set. Exposed for tests. */
void game_loop(void);

/* 0x24C5C: one per-frame update — frame counter, the two 0x94-byte player
 * records, the update process table (DS_000A8644 / DS_00104AE8), then the
 * state machine in the DAT_00104B00 == 3 mode only (the original's other modes
 * are deferred). Exposed for tests. */
void game_frame(void);

/* 0x11D04: switch(DS_000F0A64). Only the title state is ported; the other
 * cases carry PORT markers naming the sub-project that owns them. */
void game_state_step(void);

/* 0x1CF40: the init chain's audio calls — AIL_startup, the shipped preferences,
 * four sample handles, the sequence handle and the 60 Hz timer slot. Called by
 * game_main(); exported so tests can run it without the full init chain (which
 * reloads the resource index). No device is opened here. */
void game_audio_init(void);

/* 0x1CF20: the master loop's per-frame audio service — starts pending music,
 * advances the sequencer two ticks (120 Hz; the loop is 60 Hz) and, when a
 * device is open, renders and submits one frame of mixed stereo audio. Exposed
 * for tests; game_loop() calls it. */
void game_audio_service(void);

/* PORT: seq_tick() calls the audio service has driven since start. --check
 * asserts it advanced, proving the sequencer ran headless (no device). */
u32 game_audio_ticks(void);

/* PORT: 1 once the music has keyed a note; sticky. --check asserts it so a run
 * that loaded a bank but never sounded one fails rather than passing silently. */
int game_music_notes_seen(void);

/* Scans `base`/`size` for the first FORM/XMID container and returns it, or NULL
 * when it is absent or its declared FORM size runs past the range. Exposed for
 * a unit test (the size guard is the only protection against an over-read on a
 * corrupt bank — see flow.c). */
const u8 *game_music_bank_find(const u8 *base, u32 size);

#endif /* PR_GAME_FLOW_H */
