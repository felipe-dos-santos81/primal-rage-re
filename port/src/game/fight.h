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
 * 0x357F5 combo-text timer 0x38D24, the 0x35803 state machine 0x3531C and the
 * 0x35829 0x186C4 re-latch of both slots run; the rest of the pass (0x33C78,
 * 0x34038, 0x354F0) is a named gap (§7.8) and is skipped. */
void fight_hud_pass(u32 side);

/* 0x1A978. The per-side stance/command pass, reached from 0x34B6C. It calls the
 * command mapper 0x3B134 behind the 0x5f/0x62 gate and maintains the stance
 * bytes +0x61/+0x60/+0x54/+0x53. Its helpers 0x1A6AC/0x1A640/0x1A8F4 are gaps
 * (§7.11) and are skipped. Exposed because Task 4's 0x34B6C path shares it. */
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
 * walk and the eight direct RNG call sites with their gates; types 0/>0xE, 1,
 * 3..6, 8's gate, 13/14's draws and the per-entry prelude 0x4B69C (the
 * trample, demo-pose record §29) are ported, the other entry effect bodies
 * (types 2, 7, 9..12, 0x496DC/0x4987C/...) are a named gap (§7.4). When the effect
 * list at DS_0010884C is empty only the unconditional tail runs, which includes
 * 0x4A634's slot +0x42 bit 0/1 reset. */
void fight_effects_pass(void);

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

/* 0x494A8. The dust/effect entry builder the fighter spawn (0x33C78) calls at
 * 0x33E43 when DS_00104B14 == 0. Each iteration moves one node from the free
 * fight-effect list (DS_001083C4) to the active one (DS_0010884C), picks a
 * descriptor (0xC9524), spawns the dust actor and fills the entry. It issues
 * three RNG draws per iteration (0x49388, rng(0x1800), rng(step)) over
 * slot+0x81 iterations — state 6's six intermediate draws. The entry's type-0
 * processing (0x4AAD0) is a named gap (§7.4); the spawned actor renders. */
void fight_dust_build(u32 side);

/* 0x41350. The per-side character select state 6 calls for both players. It
 * runs 0x33C18 (the slot field reset), stores the character index (0xC835A[char])
 * into DS_0010816A[side] unless DS_00104B1D == 1, sets the slot's +0x63 think
 * gate, and mirrors DS_0010810D. `char_index` is the raw's DX. */
void fight_char_select(u32 side, u32 char_index);

/* 0x1D890. The HUD spawn. State 6 calls it with EAX = 0, for which the raw only
 * zeroes four per-side HUD bytes (DS_0010780E/DS_0010290C/DS_0010780A/
 * DS_0010290E) and returns; the EAX != 0 arm (0x1D8CF..0x1D9DA) spawns the HUD
 * actors and is cycle 2's (§10.6). */
void fight_hud_spawn(u32 enable);

/* 0x33F08. The two-side health-bar pass, called by state 7 (0x11E94) and the
 * game_frame tail (0x25457). Per side it selects the character constant
 * (0x17EEC's table), writes the health sprite id into the secondary actor's
 * pset+8 from the slot+0x24 table, sets the pset+0x29 bit 0x40 from actor bit
 * 15, and advances the secondary actor's animation (0x2A408). The slot+0x24
 * table is a named gap (§7.9). */
void fight_health_bars(void);

#endif /* PRAGE_GAME_FIGHT_H */
