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

/* 0x11D04: switch(DS_000F0A64). Only the title and select states are ported;
 * the other cases carry PORT markers naming the sub-project that owns them. */
void game_state_step(void);

/* 0x33904: the fixed 0x10-stride list iterator at DS_00107608..DS_00107798.
 * Returns the first entry whose +4 dword is non-zero, or 0 at the end. Exposed
 * for a unit test. */
u32 frontend_list_next(u32 node);

/* 0x1C6D4: membership test over the nine resource addresses the raw's
 * cmp/jb/jbe tree accepts. `rec` is a linear address in mem[]; the raw
 * dereferences it. Exposed for a unit test. */
u32 frontend_resource_known(u32 rec);

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

/* PORT: 0x2BF08, the message line 0x11D04's tail draws after every state's
 * function. EAX/EDX/ECX are ignored (the original stores two of them and reads
 * neither). Draws the FREE-PLAY line (DS_00105D60 != 0), `CREDITS:<n>`
 * (DS_00105C00 != 0) or the INSERT-COINS blink through the ported text calls;
 * the message branch is ungated by DS_000EF6DC & 0x1f. Exposed for tests. */
void game_overlay_step(void);

/* PORT: Task 10's title dump hook, the counterpart of 2b's PR_SMK_DUMP. With
 * PR_TITLE_DUMP set, each presented title frame is written as RGB24
 * <dir>/title/frame_%04d.raw, capped by PR_TITLE_DUMP_FRAMES (default 200).
 * game_loop() calls it after each present; exported so the Task 10 driver can
 * dump the frames it drives itself. No-op when PR_TITLE_DUMP is unset or the
 * title state has not entered. */
void game_title_dump_frame(void);

/* PORT: the title's localisation reader, 0x47370 + 0x1C500 + 0x474E4 over the
 * ENGLISH.TXT table. `game_string_table_load(dir)` reads <dir>/ENGLISH.TXT once
 * into mem[] (idempotent); `game_string_get(id)` decodes string `id` into the
 * original's DS_00102760 buffer through the 0x1E75C/0x1E808 handle and returns
 * it (an empty string when the table is absent or the id is empty). Exposed for
 * a unit test; 0x121A0 uses it for string 0x15 ("THE FUTURE..."). */
void game_string_table_load(const char *dir);
const u8 *game_string_get(u32 id);

/* 0x4F1D0. Zeroes the two origin words DS_00107A3A/DS_00107A38. Distinct from
 * 0x4F1E4 (title_input_reset). Exposed so attract.c (0x11000 phase 0/1) and
 * game_state_select share it. */
void frontend_origin_zero(void);

#endif /* PR_GAME_FLOW_H */
