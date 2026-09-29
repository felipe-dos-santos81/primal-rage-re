/* port/src/game/fight.h
 * The demo arena frame (0x263F4) and the HUD/health spine (0x35658 -> 0x34B6C
 * -> 0x1A978 -> 0x3B134) plus the scene/effects pass 0x49C78. Addresses, gates
 * and arithmetic are from docs/superpowers/plans/2026-09-20-demo-fight-derivations.md
 * §3-§5. The module registers nothing; the arena frame is called from state 7
 * (0x11E8F, flow.c) and the HUD spine is reached only through it. */
#ifndef PRAGE_GAME_FIGHT_H
#define PRAGE_GAME_FIGHT_H

#include "../types.h"

/* 0x263F4. The demo arena frame, in the raw's exact call order:
 * 0x3C5CC, 0x16D58 twice, the two position latches, 0x17FA0 twice, 0x17580,
 * 0x1958C, 0x19068, 0x17FA0 twice, 0x1975C, 0x17FA0 twice, 0x3CB68, 0x35658
 * twice, 0x49C78, 0x1282C, 0x12DA8. The six 0x17FA0 calls are not redundant.
 * The 0x1975C think step is fighter_think() (fighter.h); everything else is
 * ported here or is a named gap. */
void fight_arena_frame(void);

/* 0x35658. The per-side HUD/health pass. The arena frame calls it twice. Its
 * load-bearing spine is 0x35658 -> 0x34B6C -> 0x1A978 -> 0x3B134: the command
 * word DS_001088E0/E2 is re-derived after the think step, and its 0x35813 call
 * to 0x2A1FC (actor_sync) advances the fighter's record each frame. The
 * 0x357EE stun end 0x34038 (record §48-K), the 0x357F5 combo-text timer
 * 0x38D24, the 0x35803 state machine 0x3531C and the 0x35829 0x186C4 re-latch
 * of both slots run. The mode-4 arm (0x35792..0x357DB, gated on
 * DS_001088E0[side] bit 0) calls fighter_spawn (0x33C78) and the arena-wall
 * clamp (0x3581C/0x35824, fighter_wall_clamp, 0x354F0) are both ported
 * (record §49-A); the §7.8 gap is closed. */
void fight_hud_pass(u32 side);

/* 0x1A978. The per-side stance/command pass, reached from 0x34B6C. It calls the
 * command mapper 0x3B134 behind the 0x5f/0x62 gate and maintains the stance
 * bytes +0x61/+0x60/+0x54/+0x53. It calls 0x1A6AC and 0x1A8F4 (fighter.c,
 * ported in record §38) and the already-ported 0x1A640. With 0x1A734, which
 * 0x3B298 calls (record §39), the demo-fight record's §7.11 is closed. It is
 * the +0x52 == 6 (block) handler. Exposed because Task 4's 0x34B6C path
 * shares it. */
void fight_stance_pass(u32 side);

/* 0x3B134. The command-word mapper: it writes word[DS_001088E0 + side*2].
 * `edx_arg` is the raw's EDX (the other slot's +0x5F stance byte on the
 * 0x1A978 path); `override` non-zero bypasses the rng(100) reaction gate.
 * `0x3AFC4`'s anim triple is selected here; the 0x8000 arm calls the 0x3BDDC
 * consumer (fighter_attack_consume, 0x3B28C), which starts the 0xC8B30 attack
 * stream through 0x3C480 (demo-pose record §17). Exposed because Task 4's
 * 0x3B298 calls it. */
void fight_command_map(u32 side, u32 edx_arg, u32 override);

/* 0x49C78. The scene/effects pass. Cycle 1 ports the pass structure, the list
 * walk and the eight direct RNG call sites with their gates; types 0/>0xE, 1..7,
 * 8's gate, 9..12 (record §42-D), 13/14's draws and the per-entry prelude
 * 0x4B69C (the trample, demo-pose record §29) are ported; the type-8 held body,
 * the case-13/14 bodies, the mode-9 block (the only reader of the frame locals
 * types 9 and 11 write) are a named gap (§7.4), though their helpers 0x496DC
 * and 0x4A928 are ported (record §49-Z) without a call site; the tail's
 * 0x4987C (DS_001088BF 1..4) is ported (record §43-A). When the effect list
 * at DS_0010884C is empty only the unconditional tail runs, which includes
 * 0x4A634's slot +0x42 bit 0/1 reset. */
void fight_effects_pass(void);

/* 0x4A708 — record §48-Y. Walks the effects list DS_0010884C (the same walk
 * fight_effects_pass opens) and, for every entry whose type (entry+0x1E) is
 * not 6, zeroes the actor's +0x38/+0x34/+0x36 velocity words and holds it:
 * fight_4b3f0(entry, index, 1) when DS_00104B16 == entry+0x21, else
 * fight_4b430(entry, index, 1) (their own headers name both call sites).
 * Callers: 0x28788 (mode 9, record §48-Y) and 0x28594 (mode 8, record
 * §49-C) — the same function both call, not two near-identical siblings;
 * `get_xrefs_to 0x4A708` lists both call sites. */
void fight_effects_hold_all(void);

/* 0x4DEF4 — record §49-F. The active effects list's idle-pose walker: a
 * background-flourish voice/countdown rearm (the DS_001088B0/DS_001088BB
 * pair fight_4dbec also arms), then, per DS_0010884C entry, a 5-way dispatch
 * on entry+0x1E whose states 1..4 each gate on fight_4a868 (0x4A868, record
 * §K4.3); see fight.c for the complete derivation. Callers: mode 0xF's
 * 0x277C0 (game_mode_0f_step, flow.c) and mode 0x33's 0x29638, whose call
 * 0x2965F is not wired yet (ledger §E-3). */
void fight_effects_idle_pass(void);

/* Record §K4.1, 0x4A868: the effect entry's proximity gate. With R = the
 * entry's actor (+8), 1 when |0x2BE00(R) - entry+0x14| <= |2 * (R's dword
 * +0x32 >> 16)| (both signed, `setle`), else 0. Callers: 0x4A361 (the
 * case-14 body of fight_effects_pass, a named gap, K13), 0x4BFDE
 * (fight_4bf18) and 0x4DF8E/0x4DFFA/0x4E066/0x4E0D0 (fight_effects_idle_pass). */
u32 fight_4a868(u32 entry);

/* 0x4AC18. The worshipper streams' 0xD500 target (opcode 0x15, mode 0x4000;
 * the dword 0x0004AC18 at 24 sites in 0xEE09E..0xEF62E, the first after the
 * 0xD500 word at 0xEE09C). EAX = the actor record: with its +0x14 fight-effect
 * entry (0x49617 stores it) non-zero, it calls 0x4AC38(entry, (u32)(u8)
 * rec+0x48 - 0x20), which zeroes the actor's +0x34/+0x36/+0x38, clears its
 * +0x29 bit 6 and sets bit 4, returns the entry to type 0 and begins the
 * 0xC9544[index] stream with the hold 5.0. EDX is pushed and overwritten
 * (0x4AC19/0x4AC1A) before any read. */
void fight_4ac18(u32 rec);

/* 0x4AC80. The worshipper landing streams' 0xD500 target (opcode 0x15, mode
 * 0x4000; the dword 0x0004AC80 at 6 sites in 0xEE3BC..0xEF608, the first after
 * the 0xD500 word at 0xEE3BA). EAX = the actor record; with its +0x14 entry
 * non-zero it ends the landing: by the entry's +0x1C bit 5 and DS_001088C5 the
 * worshipper walks beside or is held by the DS_00108868 record, releases the
 * DS_00108864 entry, is held through 0x4B3F0/0x4B430 (modes 8/9/0x17), or
 * climbs (type 5, the 0xC95EC stream). EDX is pushed and overwritten
 * (0x4AC82/0x4AC8B) before any read. */
void fight_4ac80(u32 rec);

/* 0x3C5CC. Zeroes the three slot-pass words. The arena frame's first call;
 * exposed because 0x3C570's bit test reads DS_00107EE0 and a test may seed it. */
void fight_slot_clear(void);

/* 0x49300. State 6's fight-effect list init: self-links the DS_001083C4 and
 * DS_0010884C sentinels and seeds DS_001088CC/CB from DS_00104AFC. Called by the
 * unported 0x20DF4 at 0x11AC4; ported because 0x49C78's list walk needs
 * DS_0010884C to point at itself when the list is empty. */
void fight_list_init(void);

/* 0x2C320. The scene's crowd actors: `n = DSW(0xBBD98 + scene*2)` records in
 * the table at `0xBBDA8[scene]`, each spawning 0x2AE14 with the descriptor
 * `0xBB9D8[[e+0xA]*3]`. Only caller is 0x412A0. */
void fight_scene_crowd(u32 scene);

/* 0x412A0. The scene's prop actors: the 12-byte triples of `0xC82CC[scene]`
 * (until a zero first dword), then 0x2C320(scene). The `0x20DF4` branch's third
 * call (0x20E86) with EAX = the clamped scene index; the tail `0xC7F58[scene]()`
 * is a no-op target and is not issued (see the .c). */
void fight_scene_props(u32 scene);

/* 0x43818. The setup 0x43738 and 0x444C8 share before they fill each side's
 * character entries: 0x4F228(0), 0x2BAF4(1), the spawns 0xC885C (a2 0, a3
 * 0xE0, a4 0) and 0xC87F8 twice (a2 0x1500/0x3F00, a3 0xE2, a4 0x3900; the
 * records go to DS_0010814C/DS_00108150), then the palette acquires 0x98EC50C,
 * 0x98EC514, 0x8099AC and 0x809984 in that order. Its callers are 0x24C5C-mode
 * code the port does not reach. */
void fight_char_screen_setup(void);

/* 0x1D810. Mark the side's marker record DS_001028E0[side] dead (0x2B150) and
 * zero the slot; a zero slot is left alone. */
void fight_select_marker_release(u32 side);

/* 0x1D7B8. The 0x1D810 release, then spawn the class's marker 0xA78B0[cls] at
 * x = side ? 0x4200 : 0x200, a3 0xFD, the caller's y (EBX) into
 * DS_001028E0[side]. */
void fight_select_marker_spawn(u32 side, u32 cls, u32 y);

/* Record §48-Q. 0x1D838: the 0x1D810 release inline, then spawn the
 * character's badge 0xA7760[ch] at x = side ? 0x4200 : 0x200, a3 0xFD, the
 * caller's y into DS_001028E0[side]. 0x1D764: DS_0010290C[side] = 0, the
 * slot's +0x5A = 0 and +0x42 bit 4 cleared, the 0x1D2F0 bar draw at 0
 * (record §48-U), and DS_001028F8[side] begins the stream 0xE904C at 1.0. 0x4DBEC: the
 * winner's crowd (up to 28 fight-effect entries run to DS_0010810D's
 * fighter), called by modes 0xD and 0x32 at a match's end. */
void fight_hud_badge_spawn(u32 side, u32 ch, u32 y);
void fight_hud_side_reset(u32 side);
void fight_4dbec(void);
/* 0x4B9AC — record §48-D. The challenge screen's crowd (mode 0x13's case 2):
 * per side up to min(slot +0x81 * 2, 10) fight-effect entries, each a
 * 0xC9524[rng(6)] actor in a lane, animated by the side's result. */
void fight_challenge_crowd(void);

/* Record §48-C. 0x1DA08: per side, the word DS_00102908[side] counts down;
 * at 0 or below the record DS_001028F8[side] restarts the stream 0xE9050 at
 * 4.0 and the word is reloaded with max((0x78 - the slot's +0x5A) >> 1, 0xC).
 * Called by mode 0xC's arena frame 0x27380 and mode 4/6's fight frame
 * 0x26254 (record §48-K), mode 0x21's 0x26540 (game_mode_21_step, record
 * §49-O), mode 0x25's 0x266AC (game_mode_25_step, record §49-P) and mode
 * 0x31's 0x299E8 (game_mode_31_step, record §49-J). */
void fight_hud_pulse(void);

/* 0x43964. Spawn the side's entry 0xC8870[side] at the character's
 * (0xC8898, 0xC88A6) into DS_00108154[side] and its panel 0xC8878[side] into
 * DS_0010815C[side], re-pointed at 0xC88DC[ch] with 0xC88F8[ch]/0xC8908[ch]
 * as its pset word and palette. The character is (s8)DS_00108166[side]. */
void fight_char_entry_spawn(u32 side);

/* 0x43A08. Spawn the character class's side actor 0xBB938[class][side] into
 * DS_0010813C[side] (+0x4D = 0x1E; side 0 with the a5 0x4000 flip), the marker
 * (0x1D7B8 with y 0x1800), and re-point DS_0010814C[side] at 0x32B with +0x29
 * bit 3. */
void fight_char_select_actor(u32 side);

/* 0x43738. The character screen's entry (the DS_00104AE4 hook 0x28D68/0x28D80
 * install; registered): the shared prologue and 0x43818, then 0x43964/0x43A08
 * for each side whose bit side + 1 is set in DS_00104B1F, the countdown
 * DS_0010816C (5 or 0xF by DS_00108173) drawn at col 0x13 row 1, and the hook
 * reset to 0x29D60. */
void fight_char_screen_open(void);

/* 0x444C8. 0x43738 with both sides filled unconditionally and no countdown.
 * Only 0x4454C calls it (0x4462B). */
void fight_char_screen_open_both(void);

/* Record §47-M. 0x43928: each side whose DS_00108170 byte is 0 is polled by
 * 0x11F28; an accepted side sets its bit side + 1 in DS_00104B1F and its byte
 * to 1. 0x438B4: the mode 0x10 handler (0x24C5C at 0x25385), on the byte
 * DS_00108174: 0 is the character select's per-frame pass 0x43B24, or
 * 0x44798 with DS_00104B1D == 3 (record §48-S), 1 is the join test, 0x4F790
 * and the countdown DS_0010816C that ends in DS_00108174 = DS_00108172. */
void fight_char_join(void);
void fight_mode_10_step(void);

/* Record §48-S. 0x43B24, the character select's per-frame pass, and its
 * callees: 0x432A0 the unjoined side's prompt; 0x432EC/0x433DC erase and
 * 0x43464/0x435AC blink the joined side's text; 0x43EA0 the portrait
 * highlight; 0x43FBC the side's fighter; 0x43D0C the confirm button (0..3);
 * 0x4248C drops a side's stage marks; 0x43D60 the confirm; 0x43AAC the
 * countdown step (1 when it ran out). */
void fight_char_prompt(u32 side);
void fight_char_text_clear(u32 side, u32 flag);
void fight_char_opp_text_clear(u32 side, u32 flag);
void fight_char_text_blink(u32 side);
void fight_char_opp_text_blink(u32 side);
void fight_char_portrait(u32 side);
void fight_char_fighter(u32 side);
u32  fight_char_button(u32 side);
void fight_stage_mark_drop(u32 cls, u32 side);
void fight_char_confirm(u32 side);
u32  fight_char_countdown(void);
void fight_char_select_pass(void);
/* Record §48-S. 0x44798, the DS_00104B1D == 3 pass (up to four class picks
 * per side in DS_00108134, a versus arm when both side bytes are 3), and its
 * callees: 0x44638 the joined side's text; 0x4418C/0x442A0 the portrait and
 * fighter (skipped for a zero record); 0x4434C the pick toggle; 0x44054 and
 * 0x4408C drop and add a pick tag in DS_00108114. */
void fight_char_team_text_blink(u32 side);
void fight_char_team_portrait(u32 side);
void fight_char_team_fighter(u32 side);
void fight_char_team_tag_drop(u32 side, u32 slot);
void fight_char_team_tag_add(u32 side, u32 slot);
void fight_char_team_pick(u32 side);
void fight_char_team_pass(void);

/* 0x494A8. The dust/effect entry builder the fighter spawn (0x33C78) calls at
 * 0x33E43 when DS_00104B14 == 0. Each iteration moves one node from the free
 * fight-effect list (DS_001083C4) to the active one (DS_0010884C), picks a
 * descriptor (0xC9524), spawns the dust actor and fills the entry. It issues
 * three RNG draws per iteration (0x49388, rng(0x1800), rng(step)) over
 * slot+0x81 iterations — state 6's six intermediate draws. The entry's type-0
 * processing (0x4AAD0) is a named gap (§7.4); the spawned actor renders. */
void fight_dust_build(u32 side);
/* 0x4CF20 — record §49-Z. 0x494A8's DS_00104AFA == 0x23 arm: the six-entry
 * dust builder (slot +0x81 = 6). EAX = side. */
void fight_4cf20(u32 side);
/* 0x496DC — record §49-Z. The case-13 body's spawner: `count` new type-0x0E
 * entries around `entry`'s actor. EAX = entry, EDX = count. The port has no
 * call site (the case-13 body is the named gap, spec §7.4). */
void fight_496dc(u32 entry, s32 count);
/* 0x4A928 — record §49-Z. The mode-9 block's side survey: DS_001088C6..CA,
 * DS_00108858/5C/70/7C. The port has no call site (the mode-9 block is a
 * named gap, spec §7.4). */
void fight_4a928(void);

/* 0x41350. The per-side character select state 6 calls for both players. It
 * runs 0x33C18 (the slot field reset), stores the character index (0xC835A[char])
 * into DS_0010816A[side] unless DS_00104B1D == 1, sets the slot's +0x63 think
 * gate, and mirrors DS_0010810D. `char_index` is the raw's DX. */
void fight_char_select(u32 side, u32 char_index);

/* 0x1D890. The HUD spawn. Per side it zeroes four HUD bytes (DS_0010780E/
 * DS_0010290C/DS_0010780A/DS_0010290E); with AL != 0 (record §48-U) it also
 * spawns the side's two bar records (DS_001028F0/DS_001028E8, drawn at 0 by
 * 0x1D2F0/0x1D464), the 0xBB664/0xBB678 records (DS_001028F8/DS_00102900)
 * and the character badge (0x1D838), and sets DS_00104AEC bit 1. State 6
 * calls it with EAX = 0; mode 5's 0x25C1C with 1. 0x1DC6C (fight_hud_spawn_b,
 * DS_00104B1D == 2) differs only in side 1's first descriptor, 0xA7678. */
void fight_hud_spawn(u32 enable);
void fight_hud_spawn_b(u32 enable);

/* 0x1DAE8 (record §49-M). The single-side twin of 0x1D890/0x1DC6C, scoped to
 * DS_00104B1A: only the health-bar and 0xBB664 records (no second bar, no
 * 0xBB678), then the badge; sets DS_00104AEC bit 3 (not bit 1). Only caller:
 * mode 0x23's state 1 (0x26A50, unported before this cycle). */
void fight_hud_spawn_side(u32 enable);

/* Record §48-U. 0x1D2F0: 0x10D70 on DS_001028F0[side] with the word
 * 0xC9960[v] or 0xC9A52[v] (v clamped to 0..0x78; the table by DS_00104B1D
 * == 2 and the side, else by the slot's +0x41 bit 3). 0x1D464: 0x10D70 on
 * DS_001028E8[side] with 0xC9B44[v] (v clamped to 0..0x44). */
void fight_hud_bar_set(s32 v, u32 side);
void fight_hud_bar2_set(s32 v, u32 side);

/* 0x1D540 (record §49-V). The render-table bit 1 handler: steps the two HUD
 * meters DS_0010290C/E toward the slots' +0x5A/+0x5D targets, runs the +0x5D/
 * +0x5E counters, and clears DS_00104AEC bit 1. Registered as a render-table
 * entry in actors_init. */
void fight_hud_meter_step(void);

/* 0x20EF8 (record §48-U). The per-round reset of both sides' slot words
 * +0x74/+0x76/+0x78/+0x84/+0x8C, their 0x38BEC scratch (the 0x107D18..
 * words and 64 bytes at 0x107A80), the 0xFD148/0xFD158 bytes, DS_001078F2,
 * DS_00100B5E, DS_00100B50 (-1), 0x46670's 0x1081F0 block and DS_00100C1D. */
void fight_round_reset(void);

/* 0x3CB68. The 2 x 32 slot pass (0x3C88C per slot); DS_00107EDC ends at 2 and
 * DS_00107ED8 at 0x20. Called by 0x263F4 (fight.c), mode 5's 0x25C88 and
 * mode 4's 0x26254 (record §48-K). */
void fight_slot_pass(void);

/* 0x33F08. The two-side health-bar pass, called by state 7 (0x11E94) and the
 * game_frame tail (0x25457). Per side it selects the character constant
 * (0x17EEC's table), writes the health sprite id into the secondary actor's
 * pset+8 from the slot+0x24 table, sets the pset+0x29 bit 0x40 from actor bit
 * 15, and advances the secondary actor's animation (0x2A408). The slot+0x24
 * table is a named gap (§7.9). */
void fight_health_bars(void);

/* 0x4CB18 (record §42-C). The launch of a worshipper entry: the 0xC9604[si]
 * tumble stream at 3.0, +0x34 from the fighters' distance (over 0x38 when
 * `flag`, else 0x70; negated when 0x1A570(side)), +0x36 = 0 (flag) or
 * (0x3BC0 - height) / 0x16, type 6, +0x1C bit 7 cleared. Called by 0x4B470's
 * eighth-hit tail and by 0x4C60C (record §43-A). */
void fight_4cb18(u32 entry, u32 index, u32 flag, u32 side);

/* 0x4D898 (record §42-C). The mode-0x22 grab arm 0x4D7A4 calls: 1 (trample)
 * unless fighter `hit - 1` is in the character's grab move (0xC97F2[ch], or
 * 0xA / 0xB by character); an accepted move grabs as 0x4B788 does, feeds
 * DS_00104B1A's slot +0x5B and returns 0. EAX = hit, EDX = entry, EBX = si. */
int fight_4d898(u32 hit, u32 entry, u32 index);

/* 0x4D7A4 (record §42-C). The mode-0x22 effects pass's per-entry prelude (the
 * twin of 0x4B69C, through the grab arm 0x4D898). Its only caller is the
 * mode-0x22 pass 0x4D2D0 (record §43-A). */
void fight_4d7a4(u32 entry, u32 index);

/* 0x4987C (record §43-A). EAX = side, EDX = count, EBX = kind: `count` new
 * fight-effect entries off the free list, kind 0 dust walkers (0x4B144), kind
 * 1/2 type-7 flyers. Callers: the effects tail 0x4A616 and 0x4D2D0. */
void fight_4987c(u32 side, s32 count, u32 kind);

/* 0x4D2D0 (record §43-A). The effects pass of modes 0x22 and 0x24; its
 * callers 0x26C8C/0x26F58 (the mode frames) are not ported. */
void fight_4d2d0(void);

/* 0x4CC0C (record §43-A). The volleyball game's end screen. Callers: 0x4C784
 * and the mode-0x21 pass 0x4BF18 (fight_4bf18, record §49-S). */
void fight_4cc0c(void);

/* 0x4C784 (record §43-A). Fighter `side` eats the volleyball DS_00108864. */
void fight_4c784(u32 side);

/* 0x4C60C (record §43-A). The volleyball's per-entry hit test (EAX = entry,
 * EDX = si); its one caller is the mode-0x21 pass 0x4BF18 (fight_4bf18,
 * record §49-S). */
void fight_4c60c(u32 entry, u32 index);

/* 0x4BF18 — record §49-S. The attract loop's volleyball mini-game's
 * per-frame driver, called by mode 0x21's frame handler (0x26540/
 * game_mode_21_step, flow.c) in place of fight_effects_pass; walks the
 * SAME singly-linked effects list DS_0010884C that pass walks. See fight.c's
 * own header comment for the full 8-state switch and shared-tail
 * derivation. */
void fight_4bf18(void);

/* Record §46-B. DS_00104AE4 hooks, registered in actors_init: 0x430E8 (the
 * versus screen, installing 0x430C0), 0x430C0 (draws "VS") and 0x4367C (the
 * character screen after the coin divert, 0x43738 or 0x4454C). */
void fight_hook_430e8(void);
void fight_hook_430c0(void);
void fight_hook_4367c(void);
/* Record §46-F. 0x4142C, a DS_00104AE4 hook (stored by 0x28788 with mode
 * 0x17): the screen of mode 0x12, with 0x413C8's eight spawns. 0x4246C: the
 * seven stage bytes DS_00108106 and the count DS_00108111 = 0. */
void fight_hook_4142c(void);
void fight_stage_marks_clear(void);
/* 0x33C18 (record §47-C). The slot `side`'s +0x7F/+0x80/+0x82/+0x5B bytes
 * and +0x3C dword = 0, and the word DS_00108860[side] = 100. */
void fight_char_reset(u32 side);
/* Record §48-K. 0x1DA08: see the declaration above (record §48-C); this
 * batch adds it as a second caller. 0x4E11C: the fight frame's gated entry to
 * mode 0x25 (both characters 4, the camera centre DS_000F0AF0 within
 * +-0x3300); else the word DS_00108892 counts down. 0x4E350(fresh): the ten
 * DS_0010839C entries and their 0xC9524 actors; 0x4E27C: the active list's
 * actors re-faced to the camera centre. */
void fight_mode25_enter(void);
void fight_mode25_spawn(u32 fresh);
void fight_mode25_face(void);

/* 0x4E67C — record §49-P. Mode 0x25's per-frame audience-effects pass;
 * game_mode_25_step (0x266AC) calls it unconditionally. See fight.c for the
 * full derivation. */
void fight_4e67c(void);
/* 0x4EBB8 — record §49-P. The mode-0x25 round-card body: the decorative
 * glyph grid and, per side, the two tallies' glyph/number rendering and the
 * final scorecard byte DS_0010888C[side * 5]. Called by fight_4e67c and by
 * game_mode_25_reveal (0x4EF8C). See fight.c for the full derivation. */
void fight_mode25_scorecard(void);

/* 0x4DBB4 (record §49-W). slot+0x5B += amount * 120 / 100 (signed, truncating),
 * capped at 0x78. 0x12BB8 (camera_dust_burst) calls it with 1; its other
 * caller is unported. */
void fight_slot_5b_add(u32 slot, s32 amount);

#endif /* PRAGE_GAME_FIGHT_H */
