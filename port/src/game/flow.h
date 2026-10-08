/* Top-level game flow: the init chain (0x1BEC4), the master frame loop
 * (0x255CC), the per-frame update (0x24C5C) and the state machine (0x11D04).
 * This is the Task 14 seam that turns the platform modules into a running game:
 * states 0/1/2 and the front-end states 3/4/5 are live; every callee owned by a
 * later sub-project is stubbed with a PORT marker in flow.c naming it. */
#ifndef PR_GAME_FLOW_H
#define PR_GAME_FLOW_H

#include "types.h"

#include <setjmp.h>

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

/* 0x20C24..0x20DE3 — record named-gaps-b §B.2. The post-setjmp tail of
 * 0x20C10: game_init runs it last, and a 0x65431 longjmp re-runs it from
 * game_loop's restart point. */
void game_init_resume(void);

/* 0x65431 — record named-gaps-b §B.1. WATCOM longjmp(0x1044F4, 1): the soft
 * restart. Lands at the armed restart point (game_loop's; 0x20C1F's setjmp in
 * the raw); never returns. */
_Noreturn void game_restart_longjmp(void);

/* PORT: arms `jb` as the restart point and returns the previous one (NULL
 * when none). The raw's jmp_buf is the 0x2C-byte register save at DS
 * 0x1044F4 (0x653FC), which only 0x65431 reads. */
jmp_buf *game_restart_arm(jmp_buf *jb);

/* PORT: test seam, no raw counterpart: how many 0x65431 restarts have landed
 * at game_loop()'s restart point since boot. */
u32 game_restart_landings(void);

/* Tells game_main() which directory holds the INDEX-listed resources
 * (data/game/C). Must be called before game_main(). */
void game_set_game_dir(const char *dir);

/* 0x255D4/0x255DA: the master loop's prologue — zeroes the tick pair
 * DS_00101508/DS_0010150C. Called once by game_main(); the per-frame drivers
 * that call game_loop() directly start from the BSS-zero pair. */
void game_loop_begin(void);

/* 0x255CC: the master frame loop (input pump -> frame -> render -> present ->
 * pacing) until the quit flag DS_000A81A8 is set. Exposed for tests. */
void game_loop(void);

/* PORT: exactly one game_loop() iteration, for the headless drivers (--check
 * and the test drivers). The loop also stops on the quit flag DS_000A81A8, as
 * the raw's 0x256DD does; the step brake leaves that flag untouched, because
 * the movie player's entry test 0x1C75F reads it inside the iteration (record
 * named-gaps-f §F.2). */
void game_loop_step(void);

/* 0x24C5C: one per-frame update — frame counter, the two 0x94-byte player
 * records, the update process table (DS_000A8644 / DS_00104AE8), then the mode
 * switch on the word DS_00104B00 (jump table 0x24B8C, record §47-B: mode 3 runs
 * the state machine; all 52 entries are wired, all-gaps ledger §D), the
 * DS_00104B15 tail, and the 0x2545C mode tail
 * (modes 0x0C, 0x21..0x23 and 0x25; record §42-D). Exposed for tests. */
void game_frame(void);

/* 0x24CFE..0x24EE7 (record §55-A): game_frame's int 16h keyboard loop, split
 * out so a test can drive it without the mode switch. Until the queue is
 * empty, each key sets the latch DS_00105F30; in mode 0x1E a non-zero ascii
 * byte goes to 0x20860; otherwise Enter in mode 3 stores mode 0x27, ESC opens
 * the quit prompt 0x249F0 (AL = 0 in mode 3, none in mode 0x27, else AL = 1),
 * space pauses (not in modes 3/0x27, nor in 0x17 under the hook 0x10E80), and
 * the extended keys 0x10/0x1F/0x24/0x32 run 0x249F0(0)/0x1D220/0x5004A (not
 * ported)/0x1D1B0. The prompt and the pause block on input_get_key. */
void game_key_loop(void);

/* 0x11D04: switch(DS_000F0A64). States 0/1/2 and the front-end states 3/4/5 are
 * ported; 6/7/8 (the fight engine) and state 9's semantics beyond the countdown
 * handoff carry PORT markers naming the sub-project that owns them. The coin
 * arm and state 8 call 0x257A4 (game_coin_divert, record §48-W). */
void game_state_step(void);

/* 0x33904: the fixed 0x10-stride list iterator at DS_00107608..DS_00107798.
 * Returns the first entry whose +4 dword is non-zero, or 0 at the end. Exposed
 * for a unit test. */
u32 frontend_list_next(u32 node);

/* 0x1C6D4: membership test over the nine resource addresses the raw's
 * cmp/jb/jbe tree accepts. `rec` is a linear address in mem[]; the raw
 * dereferences it. Exposed for a unit test. */
u32 frontend_resource_known(u32 rec);

/* 0x29B74: the DS_00104AE4 countdown handler. Clears the effects, spawns a
 * 0x13D4C darken (byte 3) for every live 0x33904 list entry, then
 * DS_001088EE = DS_00104AFE = 0x78 and DS_00104B00 = 0x15. Registered in
 * actors_init for its ported stores (0x277C0, 0x28788) and its six ported
 * `call [0x104ae4]` dispatchers (record §42-E); 0x27A2C (record §48-E) also
 * calls it directly. */
void frontend_darken_all(void);

/* 0x41578: spawns a 0x13D4C darken (byte 2) for each live list entry whose +0
 * handle is 0x3E688 or 0x88874B0, runs 0x32A3C, then DS_00104AFE = 0x78,
 * DS_001088EE = 0, DS_00104AFA = 0x13, DS_00104B00 = 0x15, DS_00104B25 = 0.
 * Direct-called only (record §42-E): 0x41755, 0x41DE6, 0x42337 and 0x42352,
 * all ported. */
void frontend_darken_marked(void);

/* Record §43-B: the mode 0x1A/0x1B wipe and the DS_00104AE4 hook. None is
 * reached on an oracle path: 0x4F980's eleven callers and the two hooks'
 * storers (0x42CB4, 0x28DA4) are ported, but none runs without input
 * (all-gaps ledger §D), so they are unit-tested.
 * 0x4F980: DS_001088F5 = 0, DS_00104AFA = ret_mode, DS_00104B00 = 0x1A. */
void frontend_wipe_arm(u32 ret_mode);
/* 0x28D68/0x28D80: DS_00104AE4 hooks; the hook becomes 0x43738 and 0x4F980
 * arms mode 0x1A returning to 0x10 (0x28D80 plays the 0x2E voice first).
 * Registered in actors_init. */
void frontend_char_screen_hook(void);
void frontend_char_screen_hook_voice(void);
/* 0x4F9E4/0x4FA88: one wipe-in / wipe-out frame over the actor DS_000C98F0;
 * 1 when the 17-frame sprite run is done, else 0. */
u32 frontend_wipe_in(void);
u32 frontend_wipe_out(void);
/* 0x4F9A0/0x4F9C8: the mode 0x1A/0x1B handlers (0x24C5C at 0x25403/0x2540A).
 * Each runs its wipe; when it is done, the DS_00104AE4 hook, then mode 0x1B /
 * the saved DS_00104AFA. */
void frontend_mode_1a_step(void);
void frontend_mode_1b_step(void);
/* Record §46-G. 0x4F778: AL = DS_001088E4 & 0xC9898[n] != 0. 0x4F790: the
 * skip test (2 held, 1 pressed, 0 none). 0x4F318: the mode 0x17 handler, a
 * countdown that runs the DS_00104AE4 hook (0x24C5C at 0x253EE). */
u32 frontend_buttons_pressed(u32 n);
u32 frontend_skip_check(void);
void frontend_mode_17_step(void);
/* Record §48-X. 0x4F24C: the mode 0x15 handler (0x24C5C at 0x253E0), the
 * same countdown; when it runs out the mode takes DS_00104AFA. */
void frontend_mode_15_step(void);
/* Record §49-G. 0x4F2B0: the mode 0x16 handler (0x24C5C at 0x253E7), the
 * same countdown; when it runs out it runs the DS_00104AE4 hook (as 0x4F318
 * does) and then takes DS_00104B00 = DS_00104AFA (as 0x4F24C does) — both
 * siblings' expiry actions in one. */
void frontend_mode_16_step(void);
/* Record §49-I. 0x4F6E8/0x4F704: the mode 0x18/0x19 handlers (0x24C5C at
 * 0x253F5/0x253FC), gated on the effects-in-flight count DS_0009AF3D rather
 * than a countdown; each runs the DS_00104AE4 hook when it is 0, and 0x18
 * also stores DS_00104B00 = DS_00104AFA (0x19 does not). */
void frontend_mode_18_step(void);
void frontend_mode_19_step(void);
/* 0x11F28: one coin/start poll for the event (side) `code`; 1 when a credit
 * is ready and the event's mask 0x9ACBC[code] is newly pressed (one credit is
 * spent through 0x2CA7C), else 0. Also 0x43928's poll (record §47-M). */
u32 frontend_coin_poll(u32 code);
/* Record §48-S. The unjoined side's blinking prompt on the character screen
 * (0x432A0's callees; also called by 0x28CC8, record §48-J, and by 0x2C2B0
 * since record §48-T, by 0x27A2C and 0x2791C since record §48-E, and by
 * 0x424E8, record §48-D).
 * 0x2C178: "PRESS START" (string 0x48, or the 0xBAB60 sprite with the
 * DS_00104529 bit 1) for `side` through 0x2C0F4, which draws on the
 * blink phase DS_000EF6DC & 0x1F == 0 and erases through 0x2C088 on phase
 * 0x18. 0x2C1C8 (falling into 0x2C1D4): "INSERT 1 COIN" (strings 0x49, the
 * image string 0x809C4 and 0x4B) on the same phases, erased by 0x2F388. */
void prompt_press_start_clear(s32 col, s32 row, u32 side);
void prompt_press_start_blink(s32 col, s32 row, u32 side, u32 sprite);
void prompt_press_start(u32 side, s32 row);
void prompt_insert_coin_blink(s32 col, s32 row);
void prompt_insert_coin(u32 side, s32 row);
/* 0x2C2B0 (record §48-T): erase `side`'s prompts at `row` outright — with the
 * DS_00104529 bit 1, 0x2C088 and DS_00105BF8 cells at the last "INSERT 1
 * COIN" position; then, either way, DS_00105BF8 cells at the side's col byte
 * 0xBAB58[side]. */
void prompt_side_erase(s32 side, s32 row);

/* 0x1CF40: the init chain's audio calls — AIL_startup, the shipped preferences,
 * four sample handles, the sequence handle and the 60 Hz timer slot. Called by
 * game_main(); exported so tests can run it without the full init chain (which
 * reloads the resource index). No device is opened here. */
void game_audio_init(void);
/* 0x1D0BC (record k7-k12 §0.7.1): the MIDI buffer and the four sample-slot
 * buffers (+0x10), once. Returns AL (0 when DS_000A2CB0 was already set). */
u32 sound_buffers_alloc(void);
/* 0x1CB18 (record k7-k12 §0.7.4): start slot's queued sample; called by 0x1CF20. */
void sound_sample_start(u32 slot);
/* PORT: the timer ISR 0x1BDF4's counter pair, n ticks (0x1BE0E..0x1BE16). */
void game_isr_ticks(u32 n);

/* 0x5D808: DS_000EF6DE = 0, the ISR's frame word (0x20C10's tail). */
void game_isr_word_reset(void);

/* 0x2C3FC: the voice dispatcher over the 12-byte records at DS_000BBDC8
 * (flow.c). Returns AL: 1, or 0 for id 0, a record case above 5, a playing
 * sample in cases 2/3 or an unlisted case-3 id. */
u32 sound_voice(u32 id);

/* 0x1CA14/0x1CA6C/0x1CC28/0x1CD9C/0x1CE04/0x1CE70/0x1D238/0x1D244 (C2b):
 * the voice dispatcher's audio callees, exported for its differential row's mutants. */
u32 snd_music_request(u32 song, u32 b);
/* 0x1CA40 (C3b): the sequence-playing predicate, exported for the voice rows. */
u32 snd_music_playing(void);
u32 snd_music_stop(void);
u32 snd_sample_queue(u32 h, u32 loop);
u32 snd_samples_stop_all(void);
u32 snd_sample_playing(u32 h);
u32 snd_sample_stop(u32 h);
void snd_music_unpause(void);
void snd_sample_unpause(void);

/* PORT: test seam (record k7-k12 §4), not original state. sound_voice logs
 * the id of every entry, the first SOUND_VOICE_LOG_CAP of them, since the
 * last reset. sound_voice_log_count is the number of entries since the reset
 * (it counts past the cap); sound_voice_log_at(i) is the i-th id, or
 * 0xFFFFFFFF past the count or the cap. */
#define SOUND_VOICE_LOG_CAP 16u
void sound_voice_log_reset(void);
u32 sound_voice_log_count(void);
u32 sound_voice_log_at(u32 i);

/* Record §K6.1, 0x4F714: sound_voice(word 0xC9888[stage]), a tail jump. */
u32 sound_voice_stage(u32 stage);

/* Record §K6.2, 0x4F728: voice 0xDF (the result DS_00104AD4 not -1/3, its
 * slot's +0x63 clear, the signed DS_001088F2 > 0) or 0x23, then 0x22. */
void sound_voice_match_end(void);

/* PORT: slot i's AIL sample handle (0..3), which the original keeps at
 * DS_00102860 + i*0x18 and the port in flow.c; NULL out of range. Exposed so
 * a unit test can set the status the sound module's slot scans read. */
struct AIL_SAMPLE *sound_slot_handle(u32 i);

/* 0x1CAB8 / 0x1CED4 (record §50-D). Set the music / SFX volume word
 * (DS_000A2CB8 / DS_000A2CB4) and push it to a playing sequence / to each
 * playing sample slot; a value equal to the stored one does nothing. */
void sound_music_volume(u32 v);
void sound_sfx_volume(u32 v);

/* 0x1D1B0, 0x1D220, 0x1D250, 0x1D270 (record §50-D): the music pause toggle,
 * the sample pause toggle, and the pause / resume pair the quit prompt uses. */
void sound_music_pause_toggle(void);
void sound_sample_pause_toggle(void);
void sound_pause(void);
void sound_resume(void);

/* 0x249F0 (record §50-D). The quit prompt; `hard_quit` is AL (0 = the quit
 * flag, nonzero = ABANDON CONQUEST, whose yes soft-restarts through
 * game_restart_longjmp, record named-gaps-b §B.3).
 * Blocks on input_get_key. Called by game_key_loop (ESC, the extended key
 * 0x10; record §55-A). */
void game_quit_prompt(u32 hard_quit);

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

/* PORT: 4d's attract dump hook, the state-0 counterpart of
 * game_title_dump_frame. With PR_ATTRACT_DUMP set, each presented attract frame
 * is written as <dir>/attract/frame_%04d.raw RGB24, capped by
 * PR_ATTRACT_DUMP_FRAMES (default 4096). game_loop() calls it after each present
 * while DS_000F0A64 == 0; exported so the 4d driver can document the same run. */
void game_attract_dump_frame(void);

/* PORT: the title's localisation reader, 0x47370 + 0x1C500 + 0x474E4 over the
 * ENGLISH.TXT table. `game_string_table_load(dir)` reads <dir>/ENGLISH.TXT once
 * into mem[] (idempotent); `game_string_get(id)` decodes string `id` into the
 * original's DS_00102760 buffer through the 0x1E75C/0x1E808 handle and returns
 * it (an empty string when the table is absent or the id is empty). Exposed for
 * a unit test; 0x121A0 uses it for string 0x15 ("THE FUTURE..."). */
void game_string_table_load(const char *dir);
const u8 *game_string_get(u32 id);
u32  string_lock(u32 handle);                    /* 0x1E75C */
void string_unlock(u32 handle);                  /* 0x1E808 */
u32  string_decode(u32 id, u8 *out, u32 outlen); /* 0x474E4 */

/* 0x4F1D0. Zeroes the two origin words DS_00107A3A/DS_00107A38. Distinct from
 * 0x4F1E4 (frontend_input_reset). Exposed so attract.c (0x11000 phase 0/1) and
 * game_state_select share it. */
void frontend_origin_zero(void);

/* 0x4F1E4. Clears DS_00104B15 (the front-end input latch); the raw's two
 * 0x2EA30 interrupt-lock calls are inert in the port. Exposed so attract.c's
 * 0x11000 phase 0/2 and 0x10EE4 reuse the one body. */
void frontend_input_reset(void);

/* 0x38B18. Spawns `desc` through actor_spawn(desc, a2 << 3, 2, a3 << 3, 0) into
 * the first free slot of the 7-entry table at DS_00107A1C. The raw argument
 * binding is eax = desc, edx = a2, ebx = a3 (pinned by disassembly in
 * docs/superpowers/plans/2026-09-17-actor-system-args.md §2); the 0x11000
 * attract phase 2 passes edx = ebx = 0. Shared by the title state, the 0x11F6C
 * selector and the attract machine. */
void frontend_spawn_row(const u32 *desc, u32 a2, u32 a3);

/* Record §46-B. 0x20DF4: the fight reset (state 6 and the three hooks
 * below). `stage` is clamped to 7 for the full branch; `full` (the raw's EDX)
 * selects 0x2BAF4/0x38730/0x412A0. */
void game_fight_reset(u32 stage, u32 full);
/* Record §K2.1, 0x29B70: a bare `ret`. Called by 0x20DF4 (0x20E0B) and by
 * game_frame for modes 1, 2 and 0x20 (0x2521A/0x25224/0x2522E). */
void game_null_step(void);
/* Record §K2.2, 0x32968: a bare `ret`. Called by game_init (0x20CC7). */
void game_init_null(void);
/* 0x4F200: DS_00107A55 = (u8)v, DS_00107A54 = 0, 0x4F1D0, 0x2BAF4(1). */
void flow_screen_reset(u32 v);
/* 0x25848: picks the stage word DS_00104AFC (on DS_00104B17). */
void flow_stage_pick(void);
/* 0x25BBC/0x26998/0x270BC: DS_00104AE4 hooks their storers install before
 * 0x4F980 arms mode 0x1A; each resets the fight, spawns fighters and leaves
 * the hook 0x5D812. Registered in actors_init. */
void game_hook_25bbc(void);
void game_hook_26998(void);
void game_hook_270bc(void);
/* Record §46-F. The seven remaining DS_00104AE4 values, registered in
 * actors_init: 0x259CC, 0x26978 and 0x27134 arm mode 0x1A with the hooks
 * above; 0x24B54 and 0x10E80 (game_state_init, also 0x20C10's last call)
 * reset to modes 0x27 and 3; 0x25AE8 (also mode 0x14's handler) sets mode 0x17
 * with the hook 0x10E80 or 0x24B54. */
void game_hook_259cc(void);
void game_hook_26978(void);
void game_hook_27134(void);
void game_hook_24b54(void);
void game_hook_25ae8(void);
void game_state_init(void);

/* 0x1E918, 0x1E988, 0x1E824 and 0x1EA08: the high-score defaults, the audit
 * reset test, the high-score init and the attract's high-score screen (record
 * §46-A). */
void hiscore_fill_defaults(u32 force);
u32  hiscore_audit_reset_due(void);
void hiscore_init(void);
void frontend_match_start(void);

/* Record §47-C. 0x257A4: the coin/start divert (`players` is the raw's EAX,
 * stored as DS_00104B1F): 0x2BAF4(0), the byte resets, 0x33C18 per side,
 * 0x46594, the 7-byte clear of DS_00104B02, the hook 0x4367C and mode 0x1A
 * with the return mode 0x10. Called by game_state_step's coin arm (the
 * accepted mask) and state 8 (3) (record §48-W), and by the game-start modes
 * 0x28..0x2F (game_mode_28_step..game_mode_2f_step, record §49-Q).
 * 0x46594: the DS_001082C8/CC/D0 values from the byte DS_0010452C (or 7/4
 * when DS_00108173 != 0), latched into DS_001082C0/C4. */
void game_coin_divert(u32 players);
void flow_1082c8_init(void);

/* Record §48-Q, 0x33C18's other callers. 0x4651C: DS_001082C8/CC from
 * DS_001082C0/C4. 0x292D4: the side's character set to `ch` (0x33C18 first).
 * 0x2716C: the same with an unused random character (bit 5 of
 * DS_00104B02[c]). 0x28DA4: a player joins (modes 6/0xC), mode 0x17 with the
 * hook 0x28D80. */
void flow_1082c8_restore(void);
void flow_side_char_set(u32 side, u32 ch);
void flow_side_char_random(u32 side);
void flow_player_join(u32 side);
/* Record §48-J. 0x28CC8: the join poll of modes 6 and 0xC. The first side
 * whose bit side + 1 is clear in DS_00104B1F gets "INSERT 1 COIN" (no credit)
 * or "PRESS START" at row 0x1D (y 0x3A00 for the sprite prompt), or, on its
 * newly pressed start mask 0x9ACBC[side], joins: the bit is set and side + 1
 * returned (the 0x2CA7C spend then never debits). 0 otherwise. game_frame's
 * cases 6 and 0xC pass a non-zero result minus one to 0x28DA4. */
u32 flow_join_poll(void);
/* 0x28130: the match-result caption on DS_00104AD4 (and DS_00104B16).
 * 0x274FC/0x296B8: the handlers of modes 0xD and 0x32, dispatched by
 * game_frame: the arena frame's tail steps, then on DS_00104B0C the next
 * opponent (modes 0xC / 0x31) or the match's end (modes 0xF / 0x33). */
void flow_match_result_text(void);
void game_mode_0d_step(void);
void game_mode_32_step(void);
/* Record §48-C, mode 0xC. 0x27380, game_frame's case 0xC with no join: the
 * mode-0xC tail's blink undone, the arena frame's steps (the projection block
 * only with DS_001078FA == 2), the pulse countdowns 0x1DA08, 0x272DC and
 * DS_00104AEC |= 2. 0x272DC: the winner (s8)DS_0010810D's slot +0x5A at 0x78
 * gives 0x27254 and 0x278B0 (mode 0xE); else the DS_00104B12 side's gives
 * the winner's +0x41 bit 4 and mode 0xD. 0x27254: both sides' slot/record
 * copies to 0x104890/0x1049B8 and their motion zeroed. 0x278B0: the
 * continue screen, the string 0x41 and the countdown 15, mode 0xE. */
void game_mode_0c_step(void);
void flow_arena_ko_check(void);
void flow_match_snapshot(void);
void flow_continue_open(void);
/* Record §48-E, mode 0xE (the continue screen). 0x27A2C, game_frame's case
 * 0xE: the side (s8)DS_0010810D's start with a credit (0x42F60) takes the
 * continue (DS_00104B1F = side + 1, 0x2791C, 0x41310(side, 1)); otherwise
 * the countdown DS_00108110 ticks every 0x40 frames or on a forced tick
 * (DS_00105C04: back to 0xA; a newly pressed button of the joined side), is
 * redrawn on row 0xE, and below 0 darkens everything (0x29B74, mode 0x15)
 * with DS_00104AFA = 0x1E; below 0xE the side's "PRESS START" (with a
 * credit) or "INSERT 1 COIN" blinks on row 0xC; DS_00104AEC |= 2. 0x42F60:
 * 1 with a credit and the side's start mask 0x9ACBC[side] newly pressed (one
 * credit spent, DS_00105C04 = 1), else 0. 0x2791C: the continue screen
 * erased, both 0x27254 snapshots restored (0x33B00), 0x1D764 on the side,
 * its +0x5B = DS_00104B0B, 0x46534(DS_00104B12, -2), mode 0xC and its +0x41
 * &= 0xE7. */
void game_mode_0e_step(void);
u32 flow_continue_poll(u32 side);
void flow_continue_take(void);

/* Record §48-U, mode 5 (the round start). 0x25C88, dispatched by game_frame:
 * 0x3CB68, then on DS_00104B25 1 the HUD (0x25C1C), the round card and the
 * win markers (0x256F4); 2 the fight card; 3 their release, the timer field
 * (0x4F37C) and mode 6, or mode 0xC with the side DS_00104B12's entrance and
 * badge; 4 the DS_00104AFE countdown into DS_00104B23. 0x25C1C: the HUD
 * spawn (0x1DC6C or 0x1D890 with 1), 0x1D810 and 0x20EF8. 0x256F4: the
 * 0xBB68C markers for DS_00104AF2/DS_00104AF3 wins. 0x4F37C: "TT"/"EE"/"XX"
 * or the number 60 at col 0x13, row 1. */
void game_mode_05_step(void);
/* Record §K8a, 0x25FAC: update-table entry 6 (DS_00104AE8 bit 0x40, armed
 * by modes 5/0x23/0x30 with the card DS_00104ACC): the card's word +0x2C grows
 * by 0x100 a frame (a byte increment of +0x2D) until it reaches 0x1000, where
 * it is clamped and the entry disarms itself. */
void flow_card_ramp_step(void);
void flow_round_hud_init(void);
void flow_win_markers_spawn(void);
void flow_round_timer_draw(void);
/* 0x4F4E8 — record §49-Z. The render table's bit-0 entry: the two sides' slot
 * +0x3C numbers when DS_00105B3B is set, else the round-timer countdown
 * redraw on the frames the tick word divides by DS_001088D0. */
void flow_round_timer_step(void);
/* 0x4F5C8 — record gameplay-u0 §U0.6. The render table's bit-4 entry: on the
 * frames the tick word divides by DS_001088D0, the count DS_00104AC4 down by
 * one (while non-zero), redrawn at column 0x13 row 1, and 0x4DBB4(the
 * DS_00104B1A slot, 1). */
void flow_bonus_count_step(void);

/* Record §49-J, modes 0x30/0x31/0x33 (mode 0x32's continue/rematch chain).
 * 0x29328 (mode 0x30): a near-twin of mode 5's 0x25C88, the same
 * DS_00104B25 sub-state machine (1 the round card and win markers, 2 the
 * fight card, 3 their release into mode 0x31 once DS_00104B21 == 0, 4 the
 * DS_00104AFE countdown into DS_00104B23). 0x299E8 (mode 0x31): a near-twin
 * of mode 0xC's 0x27380 — the same prelude, gated projection block and
 * tail — plus the DS_00104B12 slot's frozen-pose undo at entry and
 * flow_round_over_check (0x29970) in place of flow_arena_ko_check. 0x29638
 * (mode 0x33): the match's end wait (with the idle-pose walker 0x4DEF4 at
 * 0x2965F, record §W) into mode 0x17 with the hook FN_00025AE8, armed by
 * mode 0x32's own game_mode_32_step. */
void game_mode_30_step(void);
void flow_round_over_check(void);
void game_mode_31_step(void);
void game_mode_33_step(void);

/* Record §48-K, mode 4 (the fight frame). 0x26254, dispatched by game_frame
 * for mode 4 and for mode 6 without a join: the arena frame's steps (the
 * projection block only with DS_001078FA == 2), then 0x1DA08, the round-end
 * check 0x27FA8, DS_00104AEC |= 2 and the gated 0x4E11C. 0x27FA8: on a +0x5A
 * byte at 0x78 (or the timer byte DS_001088F2 run out) the winner 0x27C48,
 * then 0x27ED8 (mode 0xA) or 0x27DC8 and mode 9/8/7. 0x27C48: the winner's
 * counts, the bonuses 0x25FDC/0x2604C, 0x256F4 and 0x27BA4 (the match result
 * DS_00104AD4). 0x27DC8: the match's end. 0x27ED8: the round over. */
void game_mode_04_step(void);
void flow_round_end_check(void);
void flow_round_winner(void);
void flow_round_bonus_a(u32 side);
void flow_round_bonus_b(u32 side);
/* Record §B8 (2026-09-29-e-wire-k8b-k8d-derivations.md), 0x260BC and
 * 0x26194: update-table entries 15 (DS_00104AE9 bit 0x80) and 16
 * (DS_00104AEA bit 0x01), armed by flow_round_bonus_a/_b. Each grows its
 * bonus card (DS_00104AB4 / DS_00104AB0) in, holds it 0x3C frames, starts
 * its exit stream and shrinks it out, then kills it and disarms itself;
 * entry 15's hold end also spawns the second card through 0x2604C when
 * 0x27C48 named a side in DS_00104B1C. */
void flow_bonus_card_a_step(void);
void flow_bonus_card_b_step(void);
void flow_match_result_set(void);
void flow_match_end(void);
void flow_round_over(void);

/* 0x28C38 — record §48-B. Mode 0xB's winner-pose tick, run after
 * game_mode_04_step: every DS_00104AA8 frames (DS_00104AD8's own count),
 * 0x392A0(slot, 1, 0) on each side below 0x77 at its +0x5A byte. */
void flow_winner_pose_step(void);

/* Record §48-Z, mode 0x12 (the post-match "next opponent" screen).
 * game_frame's case 0x12 (0x253BD) calls 0x41C28, a 9-state machine on the
 * byte DS_00104B25 (a jump table at 0x41C04): 0 the prompt (sprite or text),
 * 1 a one-slot-per-call scan of the seven portraits DS_00108106 for a
 * DS_0010810F cursor with bit 0x80 set, flashing it (0x414B4) and tallying
 * DS_0010782A's match against DS_00108104; 2 kills the state-0 prompt; 3 the
 * "P1'S CHARACTER / VS / STAGE" banner once the background record's y offset
 * clears 0x2300; 4 flashes the current stage's portrait (0x41528) and counts
 * DS_0010810E; 5 the scoreboard build (0x418F4) then dispatches on
 * DS_00104B1F/DS_00108104[r]: 0x416D4 (mode 0x12's tally step, called only
 * here), 0x4160c (frontend_mode12_advance) or 0x41760
 * (frontend_mode12_challenge_continue); 6 sets up the credits-reel scroll
 * velocity and starts the scoreboard actors' animations; 7 scrolls the
 * background until it passes y = -0x180, awards 100000 points (0x41310) and
 * draws the updated score, then dispatches to mode 0x17 (continue), 0x417C4
 * (flow_no_continue_screen) or the join-prompt draw + audit close + the
 * audit add 0x2DAE4(0x10, 1) (a call, deferred); 8 a countdown (DS_00104AFE)
 * that restores DS_00104B25 from DS_00104B23 at zero. 0x416D4's dead stub
 * call 0x32BAC is 0xC3 (a bare RET)
 * confirmed by raw `read_memory` — its 8 call sites across the image
 * (including 0x41733) are all no-ops. */
void game_mode_12_step(void);

/* Record §48-D, mode 0x13 (the challenge screen after a match). 0x424E8,
 * game_frame's case 0x13, steps DS_00104B25: 0 the actors (0x428B8), 1 the
 * drop (0x42BCC), 2 the fighters (0x42724), the crowd (0x4B9AC) and a
 * filtered palette fade, 3/4 the challenge poll (0x42CB4) and the health
 * bars, 5 mode 0x17's countdown; then each side's "PRESS START"/"INSERT 1
 * COIN" on DS_00104B1D. 0x20E90: the camera words zeroed and 0x38730 on the
 * stage. 0x428B8: the two dropping records DS_001080B4/B8 and, on the match
 * result DS_00104AD4, the loser's prompt and count actors. 0x42BCC: both
 * records fall and bounce until landed (+0x5B), then DS_00104B25 + 1.
 * 0x42724: both slot records respawned for the result, their palettes and
 * secondary actors. 0x42CB4: the loser's start with a credit takes the
 * challenge (0x42FE0, the hook 0x28D68, DS_00104B25 = 5); else the count
 * DS_00108110 ticks, and below 0 leaves for mode 0x1E (or 0x17 with the
 * hook 0x25AE8). 0x42FE0: the side's prompt actor on the 0xE8816 stream,
 * its count actors killed, its prompts erased, 0x41310 on both sides. */
void game_mode_13_step(void);

/* 0x28788 — record §48-Y. Mode 9's frame handler (0x24C5C case 9, the table
 * entry 0x2533F `call 0x28788; jmp 0x2540F`, body to 0x28BD2; its only
 * caller). A slimmer sibling of the fight frame 0x26254 (game_mode_04_step,
 * record §48-K) runs unconditionally (fight_slot_clear, camera_screen_base
 * per side, the position latches, camera_project per side, camera_decay,
 * fighter_pass_a; unlike 0x26254 there is no DS_001078FA == 2 gate and no
 * second/third projection round); fight_slot_pass runs only when neither
 * side's +0x41 byte has bit 1 set; then fight_hud_pass(0)/(1),
 * fighter_pass_b(1) (note the argument: 1, not 0x26254's 0), fight_effects_pass
 * and, of camera_scene_step's pair, only camera_y_commit (0x12DA8; 0x1282C is
 * not called here). The word DS_00104AF8 counts down; reaching 0 this frame
 * sets DS_001078FE = DS_001078FC = 1, ORs 0x10 into both sides' +0x41 byte
 * and sets DS_000F0AFE = 4; either way DS_00104AEC |= 2. Only with
 * DS_000F0AFE == 4 and DS_001078FC != 0 does the results state run: it holds
 * every effects-list entry (fight_effects_hold_all), draws the result caption
 * (flow_match_result_text) and updates the streak/handicap bookkeeping
 * (flow_match_streak_update); rearms the countdowns; with DS_00108173 set,
 * forces a draw and clears both sides' think bytes; with DS_00104B1D == 0,
 * snapshots the play-time index and calls config_play_time_snap(idx, 0). It
 * then dispatches on the match result DS_00104AD4: a draw (2) always darkens
 * into mode 0x17/return-mode 0x13 (flow_results_darken_close); a decided
 * result (0/1) whose winner's DS_00107813 think-byte is 1 bumps the loser's
 * win-streak byte and, once it reaches 3 with the loser's DS_00108104 stat
 * below 6, records a combined flag byte before the same darken/close;
 * otherwise (the winner's think-byte isn't 1) the stage word DS_00104AFC ==
 * 7 clears the stage marks (fight_stage_marks_clear) before the same
 * darken/close, and any other stage only installs the mode-0x12 hook
 * fight_hook_4142c (0x4142C) under mode 0x17, with no return-mode store and
 * no darken/close. Four call sites inside 0x28788 (0x289D9, 0x28AFF,
 * 0x28B25, 0x28BA2) target 0x32BAC, which `read_memory` shows is a single
 * byte, 0xC3 (`ret`) — the tail instruction of the unrelated FUN_00032B94,
 * not a function of its own (Ghidra's own function table agrees:
 * body_start == body_end == 0x32BAC). A CALL onto a bare `ret` pushes and
 * immediately pops the same return address, so it is a proven no-op
 * (`get_xrefs_to 0x32BAC` lists eight such call sites across the image, not
 * only this function's four); the port omits the register-only computations
 * that feed them. */
void game_mode_09_step(void);

/* 0x28468 — record §49-C. Mode 8's frame handler (0x24C5C case 8, the table
 * entry 0x25335 `call 0x28468; jmp 0x2540F`, body to 0x2861B; its only
 * caller). A near-identical sibling of mode 9's 0x28788 (game_mode_09_step,
 * record §48-Y, above): the same unconditional preamble (fight_slot_clear,
 * camera_screen_base per side, the position latches, camera_project per
 * side, camera_decay, fighter_pass_a), but fight_slot_pass (0x284F8, 0x3CB68)
 * runs unconditionally here — mode 9 gates it on neither side's +0x41 byte
 * having bit 1 set (0x28815..0x28825); mode 8 has no such gate. Then
 * fight_hud_pass(0)/(1), fighter_pass_b(1), fight_effects_pass and
 * camera_y_commit, exactly as mode 9. The word DS_00104AF8 counts down the
 * same way, arming DS_001078FE = DS_001078FC = 1, OR-ing 0x10 into both
 * sides' +0x41 byte and setting DS_000F0AFE = 4 on reaching 0; either way
 * DS_00104AEC |= 2. The results gate reads different bytes than mode 9's:
 * DS_001078FE (not DS_001078FC) == 0 (0x28576..0x2857D), then DS_000F0AFE !=
 * 4 (0x28583..0x2858E), either returning before the results state.
 * The results state is much slimmer than mode 9's: it holds every
 * effects-list entry (fight_effects_hold_all, 0x28594 0x4A708 — the same
 * function mode 9 calls, not a separate sibling; `get_xrefs_to 0x4A708`
 * confirms 0x28594 as one of its call sites) and draws the result caption
 * (flow_match_result_text, 0x28599 0x28130 — likewise the same function).
 * Unlike mode 9, mode 8 never reads or writes the match result DS_00104AD4:
 * there is no win/lose/draw dispatch, no flow_match_streak_update, no
 * DS_00108173 draw force and no config_play_time_snap/close. It only rearms
 * the countdown words (DS_00104AFE = 0xF0, DS_001088EE = 0x3C, the same
 * constants mode 9 uses) and, on the byte DS_00104B1D read into AL
 * (0x285A8), installs the hook game_hook_25bbc (DS_00104AE4 = 0x25BBC) and
 * sets DS_00104B25 = 1 and mode DS_00104B00 = 0x16 unconditionally; only the
 * return mode DS_00104AFA differs: 0x30 when DS_00104B1D == 3, else 5. No
 * call site in 0x28468 targets the bare-`ret` stub 0x32BAC (game_mode_09_
 * step's finding, above): all fifteen CALL instructions in this function
 * were checked and each targets a real, already-ported callee.
 * EBX/ECX/EDX/ESI/EDI are pushed and popped. */
void game_mode_08_step(void);

/* 0x28BD4 — record §49-D. Mode 0xA's frame handler (0x24C5C case 0xA, the
 * table entry 0x2527D `call 0x28BD4; jmp 0x2540F`, body to 0x28C37; its only
 * caller, confirmed both from the decompiled switch at 0x24C5C — `case 10:`
 * is the sole caller of FUN_00028bd4 — and from `get_xrefs_to 0x28BD4`,
 * which lists exactly that one call site). Unlike modes 8/9 (game_mode_08_
 * step, game_mode_09_step, above) this is not a results-screen sibling: its
 * only 23 instructions are the tail slice those two share — the position
 * latches (DS_001077E8 = DS_001077E4, DS_0010787C = DS_00107878), then
 * fight_hud_pass(0)/(1), fighter_pass_b(1), fight_effects_pass and
 * camera_y_commit — with none of their fight_slot_clear/camera_screen_base/
 * camera_project/camera_decay/fighter_pass_a preamble, no DS_00104AF8
 * countdown, and no match-result dispatch. EAX/EDX go in as whatever the
 * jump-table dispatch stub at 0x2527D left them (no register is set up
 * before the bare `call 0x28BD4`), and 0x28BD4 never reads them: the
 * `__regparm3(undefined4,undefined4)` signature Ghidra infers is spurious;
 * the port takes no parameters (confirmed against the raw disassembly, not
 * the decompiled C, which also mis-shows the first fight_hud_pass call as
 * `FUN_00035658(param_2)` — the raw shows `xor eax,eax; call 0x35658` and
 * `mov eax,1; call 0x35658`, i.e. fight_hud_pass(0) then fight_hud_pass(1),
 * matching the modes 8/9 idiom exactly).
 *   With both sides' slot +0x54 byte (DS_00107804, DS_00107898 — the same
 * per-side field anim-opcode 0xD500 clears, actors.c's anim_code_3C32C)
 * zero (0x28C0E..0x28C1E), it plays voice 0xD8 and advances the mode:
 * `mov eax,0xd8; mov edx,0xb; call 0x2C3FC; mov [DS_00104B00],dx` (0x28C20..
 * 0x28C2F).
 *   The voice is wired (0x28C20/0x28C2A, record k7-k12 §11).
 * EDX (0xB) survives 0x2C3FC, which pushes and pops EBX, EDX and EDI (record
 * §42-E.2, confirmed here by reading 0x2C3FC's own disassembly: every exit
 * path — including the two "can't play" early-outs at 0x2C8E8/0x2C8EA — ends
 * `pop edi; pop edx; pop ebx; ret`, so DX after the call is unconditionally
 * the 0xB written in before it, not a status flag), and becomes the mode
 * below: `DAT_00104b00 = extraout_DX` in the decompiled C is exactly this
 * preserved 0xB, which Ghidra cannot trace through the call and so renders
 * as an untracked "extraout" register. So this branch unconditionally sets
 * mode 0xB (flow_winner_pose_step's mode, run after game_mode_04_step,
 * record §48-B) once the voice call returns, regardless of whether 0x2C3FC
 * itself actually played anything; when either side's byte is still
 * nonzero, the mode is left at 0xA (this handler runs again next frame). */
void game_mode_0a_step(void);

/* 0x1EC38 — record §49-R. Ranks a fighter's post-match score against
 * table 0 through config.h's hiscore_rank_probe (0x2DDE4): a rank < 10
 * clears config field 0x26 and records the rank in DS_001044D6, returning
 * 1, after arming the name-entry screen through nameentry_arm (0x204F4,
 * record §51-A); a rank >= 10 returns 0. The deferred field-0x27/0x26 audit
 * adds (0x2DAE4) are out of scope — see flow.c's header comment on this
 * function. */
u32 hiscore_rank_single(u32 score);

/* 0x1ECC8 — record §49-R. Ranks both fighters' post-match scores
 * (DS_001077EC, DS_00107880) against table 0 through hiscore_rank_probe,
 * recording each side's rank into DS_001044C0/DS_001044C2. Returns 1 only
 * when both rank < 10 and are not both exactly 9 (the table's single last
 * slot, which only one side can take); see flow.c's header comment for the
 * full derivation. */
u32 hiscore_rank_pair(void);

/* 0x1EEB0 — record §49-H. Mode 0x1E's frame handler (0x24C5C's named-gap
 * case 0x1E, table entry `call 0x1EEB0; jmp 0x2540F`; get_xrefs_to 0x1EEB0
 * confirms that one call site). It is entered with DS_00104B25 = 0 as the
 * return mode DS_00104AFA that mode 0x13's challenge poll (flow_
 * challenge_poll, record §48-D) takes when the post-match challenge window
 * runs out with no one joining (0x42EF1/0x42EF7).
 *   A 17-state sub-machine (jump table 0x1EE6C, states 0..0x10 on the byte
 *   DS_00104B25; AL > 0x10 and state 1 both land on the shared tail `ret`
 *   0x1F452, so state 1 is a genuine no-op, not an unported gap).
 *   Half the states are pure bookkeeping this port wires in full: state 2
 *   (frontend_input_reset, actors_reset, frontend_match_start, then
 *   DS_00104AFE = 0x12C, DS_00104B00 = 0x15, DS_00104AFA = 0x1E, DS_00104B25
 *   = 3, DS_001088EE = 0); state 3 (frontend_input_reset, actors_reset,
 *   DS_00104B25 = 0, DS_00104B00 = 0x14); state 6 and its side-1 mirror
 *   state 9 (frontend_input_reset, actors_reset, then — only when
 *   DS_00104ABC == 1 — the same frontend_match_start/DS_00104AFE/
 *   DS_00104B00/DS_00104AFA quad as state 2, before DS_00104B25 = 7 (state
 *   6) or 0xA (state 9) unconditionally either way); and state 0xA
 *   (frontend_input_reset, actors_reset — which leaves DS_00104B25 = 0, the
 *   CH zeroed at 0x1F3E5 and proven to survive the call since 0x2BAF4
 *   pushes and pops ECX whole — then, unless DS_00104B1F != 0 short-
 *   circuits past it, a match-result/think-byte check (DS_00104AD4 == 2, or
 *   DS_00107813 + DS_00104AD4 * 0x94 == 1) that on either hit sets
 *   DS_00104B00 = 0x14 and returns; otherwise, on DS_00104B14 == 0, installs
 *   the hook FN_000430E8 (fight.c's fight_hook_430e8, the versus-screen
 *   hook, record §46-B — already documented there as stored "at 0x1F447
 *   (0x1EEB0)") and arms the mode-0x1A wipe with return mode 0x11
 *   (frontend_wipe_arm); else DS_00104B00 = 0x14 too).
 *   States 0, 4 and 7's high-score rank probe (record §49-R) is now real:
 *   0x1ECC8 (hiscore_rank_pair, state 0) and 0x1EC38 (hiscore_rank_single,
 *   states 4/7) rank DS_001077EC/DS_00107880 — each fighter's post-match
 *   score, the +8 field of the per-side records at DS_001077E4/
 *   DS_00107878 — against table 0 through hiscore_rank_probe (0x2DDE4,
 *   config.h/.c); a rank < 0xA is a high-score-table slot. State 0's own
 *   short-circuit (DS_00104AD4 == 2 && DS_00104B1F == 0) now continues
 *   into the real gate: when hiscore_rank_pair() is true, it picks the
 *   higher of the two scores (unsigned; a tie favours side 0), stores
 *   DS_00104B25 = 0xB or 0xC and calls hiscore_rank_single on the winner
 *   (its own return discarded, matching the raw); either a false
 *   short-circuit term or a false gate lands on DS_00104B25 = 4. State 4's
 *   short-circuit (DS_00107813 == 0) continues into hiscore_rank_single on
 *   DS_001077EC; a true result is followed by a second, independent gate —
 *   DS_00104AD4 != 0, or (DS_00104B14 != 0 && DS_00104ABC == 1) — that
 *   arms DS_00104B25 = 5 only when it too holds; any false lands on
 *   DS_00104B25 = 7. State 7 mirrors state 4 exactly (DS_001078A7,
 *   DS_00107880, DS_00104AD4 != 1, arming state 8, else state 0xA).
 *   Past the real gate the name-entry screen is ported too (record §49-T):
 *   0x1F458 is nameentry_step (game/nameentry.h) with its callees 0x1FFD0,
 *   0x20710, 0x1ED2C and the name filter 0x13EF0/0x13F68; states
 *   5/8/0xB..0xE poll it (params 0 or 1 select the side) and advance when it
 *   reports done. Record §51-A wires the rest: 0x1ED2C (nameentry_reset)
 *   at the state 0/4/7 arming points, 0x204F4 (nameentry_arm) inside
 *   0x1EC38, and states 0xF/0x10, which store DS_00104B25 = 0xE/0xD, reset
 *   the screen, re-probe the other side's score through 0x1EC38 (return
 *   discarded) and prime nameentry_step for that side (return discarded).
 *   Two established gaps recur throughout, left as the same PORT: notes
 *   this codebase already uses elsewhere: every 0x2C3FC voice call (record
 *   §45-A) and, inside 0x1EC38, 0x2DAE4 (the deferred audit no-op, spec
 *   §7). EBX/ECX/EDX/ESI/EDI are pushed and popped. */
void game_mode_1e_step(void);

/* 0x208F8 — record §49-J. The mode 0x1F handler: the "roar" screen between
 * the post-match challenge poll and mode 0x1E's high-score flow. See flow.c
 * for the full derivation. */
void game_mode_1f_step(void);

/* 0x26540 — record §49-O. Mode 0x21's frame handler (0x24C5C case 0x21,
 * dispatched by `call 0x26540; jmp 0x2540F`). Byte-for-byte the fight frame 0x26254's (game_mode_04_step,
 * record §48-K) preamble and gated projection block: fight_slot_clear,
 * camera_screen_base per side, the two position latches, then, only with
 * DS_001078FA == 2, camera_project per side, camera_decay, fighter_pass_a,
 * fighter_pass_b(0), camera_project per side again, fighter_think
 * (fighter.c's 0x1975C), camera_project per side a third time. Then
 * fight_slot_pass, fight_hud_pass(0)/(1) — identical to 0x26254 up to here.
 * It diverges where 0x26254 calls fight_effects_pass (0x49C78): 0x26540 calls
 * fight_4bf18 instead (0x4BF18, record §49-S) — the attract loop's
 * volleyball mini-game's per-frame ball/entry driver, an 8-state switch over
 * the SAME singly-linked DS_0010884C list fight_effects_pass walks (not a
 * separate doubly-linked one); see fight.c's own header comment for the full
 * derivation. After that, exactly like 0x26254: camera_scene_step (0x1282C +
 * 0x12DA8), fight_hud_pulse
 * (0x1DA08), flow_round_end_check (0x27FA8), DS_00104AEC |= 2. Unlike
 * 0x26254 there is no closing DS_001078FA/DS_00108892/DS_00107803 gate into
 * fight_mode25_enter (0x4E11C): the raw ends at the OR and a plain
 * `pop edx; pop ecx; pop ebx; ret`. */
void game_mode_21_step(void);

/* 0x282C4 — record §49-E. Mode 7's frame handler (0x24C5C case 7, its only
 * caller, confirmed via `get_xrefs_to 0x282C4`'s single bare-`CALL` site —
 * the same dispatch-stub shape used to confirm mode 0xA's case, above). Not
 * a results-screen sibling of modes 8/9 either: no fight_slot_clear/
 * camera_screen_base/camera_project/camera_decay/fighter_pass_a preamble,
 * just fight_slot_pass unconditionally, then the shared tail (the two
 * position latches, fight_hud_pass(0)/(1), fighter_pass_b(1),
 * fight_effects_pass, camera_y_commit). See flow.c's header comment on
 * game_mode_07_step for the full branch-by-branch derivation of what
 * follows: a three-way dispatch on the round winner DS_00104B16 (0/1 a
 * side, 2 a draw) and the match result DS_00104AD4, each testing a
 * per-side "attack state" byte (DS_00107802's stride, entry +0x52, or
 * DS_00107804/DS_00107898, +0x54) before optionally arming DS_00104AF8 =
 * 0x258 and advancing to mode 8 or 9; every path ends DS_00104AEC |= 2,
 * DS_00104AD4 = the (reloaded, unchanged) match result. EBX/ECX/EDX/EDI are
 * pushed and popped. */
void game_mode_07_step(void);

/* 0x277C0 — record §49-F. Mode 0xF's frame handler (0x24C5C case 0xF, its
 * only caller, confirmed via `get_xrefs_to 0x277C0`'s single bare-`CALL`
 * site). Unlike modes 7/8/9/0xA's shared tail this calls fight_hud_pass(0)/
 * (1) but not fighter_pass_b; in its place, the active effects list's
 * idle-pose walker fight_effects_idle_pass (0x4DEF4, fight.c/fight.h,
 * record §49-F). See flow.c's header comment on game_mode_0f_step for the
 * full derivation: a DS_00104AFE countdown (no zero-guard, unlike modes
 * 8/9's DS_00104AF8) that, on reaching zero or below, plays voice 0x2B
 * (record k7-k12 §6), clears a bit on DS_0010810D's (the established winner-side macro)
 * slot, awards fighter_41310(DS_00104AD4, 0x30D40), runs the deferred
 * 0x32B94/0x32970 (audit/run-clock, spec §7, both no-ops here) and
 * config_play_time_close(1, DS_00104B19) — one of the two live callers
 * fighter.h/config.h's own comments named as still unported before this
 * task — then installs frontend_darken_all (0x29B74) under mode 0x17, the
 * same hook mode 9's results-close path installs. EBX/ECX/EDX/ESI are
 * pushed and popped. */
void game_mode_0f_step(void);

/* 0x26C8C — record §49-L. The mode 0x22 handler: fight_slot_clear, the
 * per-side camera preamble shared with mode 0x24, fight_slot_pass,
 * fight_hud_pass, camera_y_commit, fight_4d2d0, a gated
 * camera_impact_dust_spawn and the unconditional tail 0x26D4C (flow_26d4c,
 * static in flow.c). See flow.c for the full derivation. */
void game_mode_22_step(void);

/* 0x26A50 — record §49-M. The mode 0x23 handler: a 4-state sub-machine on
 * DS_00104B25 that arms a round card, spawns it, draws the per-character
 * challenge strings and counts down to mode 0x22. See flow.c for the full
 * derivation. */
void game_mode_23_step(void);

/* 0x26F58 — record §49-N. The mode 0x24 handler: the same camera preamble as
 * mode 0x22 (no dust spawn), then a countdown that on expiry resets the join
 * scratch, re-arms game_hook_27134 and closes the play-time accounting. See
 * flow.c for the full derivation. */
void game_mode_24_step(void);

/* 0x266AC — record §49-P. Mode 0x25's per-frame state-0 body: case 0x25's
 * own inline DS_00104B25 == 0 arm calls it. See flow.c for the full
 * derivation. */
void game_mode_25_step(void);
/* 0x4EF8C — record §49-P. Mode 0x25's reveal, run once when DS_001088BD
 * reaches 8 with both round flags set: settles the active audience-effect
 * list, draws the round-card body and the winner strings. See flow.c for the
 * full derivation. */
void game_mode_25_reveal(void);
/* 0x4F0FC — record §49-P. Mode 0x25's timeout exit: draws the closing
 * strings, releases the round-card cells and returns to mode 6. See flow.c
 * for the full derivation. */
void game_mode_25_exit(void);

/* Record §49-Q, the coin/start divert's eight game-start entries, cases
 * 0x28..0x2F of 0x24C5C (the jump table at 0x24B8C, entries 0x24F09/0x24F66/
 * 0x24FC4/0x25187/0x2501E/0x25071/0x250CE/0x2512B for 0x28/0x29/0x2A/0x2B/
 * 0x2C/0x2D/0x2E/0x2F respectively — confirmed by reading those eight table
 * dwords over the Ghidra bridge, not by disassembly position, since 0x24C5C
 * dispatches through a jump table). Each is a short, straight-line block with
 * no branch and no loop, either decoding config field 0x29 (0x2D974) the same
 * way game_state_init's 0x20C5D-0x20CC2 already does (record §46-F) — DS_
 * 00104528 = the raw value v, DS_00105B3A = bit 8 of v shifted to bit 4,
 * DS_0010452C = the nibble (v&0xF0)>>4, DS_001088D0 = (v&0xF)*5+0x1E — then
 * calling game_coin_divert (0x257A4, record §47-C) with a per-case players
 * mask (1 = side 0, 2 = side 1, 3 = both), optionally preceded by
 * config_credit_spend (0x2CA7C) and optionally preceded by setting the dword
 * DS_00104AB8 to a per-case constant. Case 0x2B skips the config-field decode
 * entirely (DS_00104AB8 = 3, straight to the divert); cases 0x2C-0x2F skip
 * the DS_00104AB8 store entirely (it keeps whatever value an earlier case, or
 * game_state_init's own DS_00104AB8 = 0 init at 0x10ED3, left there); cases
 * 0x2D-0x2F additionally spend one credit before the divert. Cases 0x2E and
 * 0x2F are, byte for byte, the same operation (config_credit_spend(1) then
 * game_coin_divert(2)) compiled twice under separate case labels — ported as
 * two separate functions since that is what the jump table's two distinct
 * entries are, not "fixed" into one. See flow.c for the full per-case
 * derivation and addresses. */
void game_mode_28_step(void);
void game_mode_29_step(void);
void game_mode_2a_step(void);
void game_mode_2b_step(void);
void game_mode_2c_step(void);
void game_mode_2d_step(void);
void game_mode_2e_step(void);
void game_mode_2f_step(void);

void flow_scroll_reset(u32 stage);
void flow_challenge_open(void);
void flow_challenge_drop(void);
void flow_challenge_fighters(void);
void flow_challenge_poll(void);
void flow_challenge_join(u32 side);

#endif /* PR_GAME_FLOW_H */
