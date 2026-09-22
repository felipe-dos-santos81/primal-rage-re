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
 * to 0x2A1FC (actor_sync) advances the fighter's record each frame. The rest of
 * the pass (0x33C78, 0x34038, 0x38D24, 0x3531C, 0x354F0, 0x186C4) is a named
 * gap (§7.8) and is skipped. */
void fight_hud_pass(u32 side);

/* 0x1A978. The per-side stance/command pass, reached from 0x34B6C. It calls the
 * command mapper 0x3B134 behind the 0x5f/0x62 gate and maintains the stance
 * bytes +0x61/+0x60/+0x54/+0x53. Its helpers 0x1A6AC/0x1A640/0x1A8F4 are gaps
 * (§7.11) and are skipped. Exposed because Task 4's 0x34B6C path shares it. */
void fight_stance_pass(u32 side);

/* 0x3B134. The command-word mapper: it writes word[DS_001088E0 + side*2].
 * `edx_arg` is the raw's EDX (the other slot's +0x5F stance byte on the
 * 0x1A978 path); `override` non-zero bypasses the rng(100) reaction gate.
 * `0x3AFC4`'s anim triple is selected here; the 0x8000 arm's 0x3BDDC consumer
 * is Task 4's and is a named gap (§7.16). Exposed because Task 4's 0x3B298
 * calls it. */
void fight_command_map(u32 side, u32 edx_arg, u32 override);

/* 0x49C78. The scene/effects pass. Cycle 1 ports the pass structure, the list
 * walk and the eight direct RNG call sites with their gates; the entry effect
 * bodies (0x4B69C/0x496DC/0x4A634/0x4987C/...) are a named gap (§7.4). When
 * the effect list at DS_0010884C is empty only the unconditional tail runs. */
void fight_effects_pass(void);

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
