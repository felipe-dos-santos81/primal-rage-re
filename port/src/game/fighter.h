/* port/src/game/fighter.h
 * The two per-frame fighter passes the arena frame calls between its projection
 * pairs (0x1958C, then 0x19068) plus the pure per-fighter helpers the HUD spine
 * and Task 4's think chain share. Both passes loop the two sides; neither is a
 * render. Addresses, gates, globals and the single RNG site are from
 * docs/superpowers/plans/2026-09-20-demo-fight-derivations.md §3.5 and §5.8.
 *
 * Task 4 adds the think/AI chain (0x1975C -> 0x3B464 -> 0x3B298 -> 0x3B134 ->
 * 0x3BDDC) here. The arena frame's 0x264CC slot calls fighter_think();
 * 0x3BDDC is exposed because fight.c's 0x3B134 calls back into it. */
#ifndef PRAGE_GAME_FIGHTER_H
#define PRAGE_GAME_FIGHTER_H

#include "../types.h"

/* 0x33A10. Fill the six-dword side context: out[0]=1-side, out[1]=side,
 * out[2]=&slot[1-side], out[3]=&slot[side], out[4]=rec_other, out[5]=rec_self. */
void fighter_ctx_swap(u32 out[6], u32 side);

/* 0x33950. The mirror of fighter_ctx_swap: out[0]=side, out[1]=1-side,
 * out[2]=&slot[side], out[3]=&slot[1-side], out[4]=rec_self, out[5]=rec_other. */
void fighter_ctx_same(u32 out[6], u32 side);

/* 0x3AFC4. Select the three animation pointer bases for `slot_char`'s character
 * and sub-index `edx` (0..0x3F): out[0]=0xDE114+11*c, out[1]=0xA3528+20*c,
 * out[2]=0xA6728+6*c with c = (char<<6)+edx. Out-of-range `edx` is the raw's
 * 0x62003 error path; the port leaves the triple zero (0x62003 is out of scope). */
void fighter_anim_triple(u32 out[3], u32 slot_char, s32 edx);

/* 0x1AB10. The fighter-state gate: 1 when slot[self]+0x54 and +0x53 are both
 * <= 1 (unsigned bytes). */
int fighter_state_ok(u32 side);

/* 0x1A570. The "actor bit 15 clear" predicate:
 * (word[actor] & 0x8000) == 0 for slot[side]'s actor record. */
int fighter_actor_bit15_clear(u32 side);

/* 0x1958C. The first per-frame fighter pass. Gated on DS_001078FA == 2; per
 * side it calls 0x33950/0x19020/0x3AFC4, clears DS_00100AF8/AFC entries, then
 * picks a winner from the two 0x18950 reachabilities and, on an exact tie,
 * draws rng(2) at 0x19714. 0x19020/0x18950/0x193B0 are named gaps (§7.6); the
 * gates, the flag stores and the RNG site are ported. */
void fighter_pass_a(void);

/* 0x19068. The second per-frame fighter pass, called with arg = 0 by the
 * arena frame. Gated on DS_00107802/DS_00107896 != 0x13; per side it runs the
 * hit-stun/timer update over DS_00100B58/B5A/B5C/B5E and the fighter record,
 * calling 0x3C570, 0x1922C and 0x3CF38. No RNG. 0x1922C/0x3CF38 are named gaps
 * (§7.6); the gates, the timer arithmetic and the record float store are
 * ported. */
void fighter_pass_b(u32 arg);

/* 0x186D0. The slot position latch the game_frame tail (0x25438) calls per live
 * side. With slot+0x42 bit 3 set it copies the fighter record's +0x18/+0x1C to
 * slot+0x2C/+0x30; otherwise the 0x18540/0x18350 screen-anchor path (named gaps
 * §6.3) offsets them by DS_00100AB0/AB4[side]. A set slot+0x41 bit 7 then
 * latches slot+0x2C into slot+0x34. */
void fighter_slot_latch(u32 side);

/* 0x33EB4. The demo-fight fighter spawn entry state 6 calls after each
 * character pick. It picks the 0x4000/0 stack argument by side, reads the
 * per-side initial x from DS_000BDA38 (a dword load shifted right 16), and runs
 * 0x33C78: it stores the slot pointer into DS_001077A8[side], copies the picked
 * character (DS_0010816A[side]) into slot+0x7A, spawns the fighter and its
 * secondary actor through actors.c's 0x2AE14, assigns the character palette
 * (0x29BC8), and resets the slot fields. 0x494A8's dust entry and the
 * res_resolve tail are named gaps (§10.4/§10.5). */
void fighter_spawn(u32 side);

/* 0x1975C. The think step the arena frame calls at 0x264CC. It iterates the two
 * sides and, for each whose DS_00100AD0 count exceeds 2, runs the per-fighter
 * think driver 0x3B464. The raw takes no argument (the brief's u8 side is a
 * correction); the driver, the command dispatch and the command consumer are
 * ported, and the unported branch targets are named gaps (§7.12). */
void fighter_think(void);

/* 0x47208. One side's CPU-AI command word for this frame: classify the slot
 * state (0x469A8), manage the move selection (0x470F8 -> 0x46F4C, which draws
 * rng(0x64)), and map the selected move step through 0x3C6E8 into
 * DS_001088E0/E2. Returns early (command word 0) when slot+0x63 is clear or
 * DS_00105B39 is set. */
void fighter_command_generate(u32 side);

/* 0x461DC. Write each side's command word into the 0x14-word input ring at
 * DS_00108270 (+0x28 per side) and advance DS_001082D4. */
void fighter_input_ring_update(void);

/* 0x24C73. game_frame's per-side command block (`DS_00104B26 == 0 &&
 * DS_00104B19+2 != 0`): fill both sides' command words (slot+0x41 bit 0x10
 * zeroes a side instead) and run 0x461DC. */
void fighter_command_block(void);

/* 0x349C8. The +0x52 == 0 (and >0x15) default handler of fight_health_sync's
 * dispatch: the 0x365C8/0x36638 gates, the 0x3BDDC consumer and the
 * 0x35838/0x2BC30 transitions. Its +0x42 bit 6/7 arms are named gaps (§7.10). */
void fighter_state_default(u32 side);

/* 0x36638. Reset the slot's +0x43 bit 0x40 and restart the fighter's animation
 * per slot+0x54. Called by 0x349C8, 0x35838 and the 0x34B6C position branch. */
int fighter_state_36638(u32 slot, u32 rec);

/* 0x35D7C. The +0x52 == 3 handler: clear slot+0x53/+0x54, then, when the
 * 0x3CF38 hit chain reports no hit, arm slot+0x54 = 2, slot+0x53 = 4. */
void fighter_state_35d7c(u32 side);

/* 0x3C88C. The per-slot attack-frame state machine fight_slot_pass runs 2 x 32
 * times per arena frame. Reads DS_00107ED8 (slot index), DS_00107EDC (side) and
 * DS_00107EE4 (facing); phase 0 arms the hitbox (word[0x107D58 + side*0x40 +
 * i*2] = 8) and phases 2..8 decrement/advance it. 0x3C758's return semantics and
 * the 0x3C800 displacement are transcribed, not unit-pinned (§6.3). */
void hit_slot_step(void);

/* 0x3CF38. The hit chain for `side`: scan the armed hitboxes, validate against
 * the target's stance and hit-stun, drive the reaction and consume the hitbox.
 * Returns 1 on a resolved hit, 0 otherwise. RNG-free (§3.8). */
int hit_chain_resolve(u32 side);

/* 0x3BDDC. The attack/command consumer the mapper's 0x8000 arm calls behind
 * 0x3BDB0. Reads the side's command word DS_001088E0/E2; when bit 15 is set it
 * clears the record's +0x34/+0x43/+0x42, sets the slot's +0x5F to 0xFF and
 * writes the attack state (DS_00107802/03/04, DS_00107D40 + side*4,
 * DS_001078F8 + side, DS_001077FE + side*0x94). Returns 1 on the transition,
 * 0 when the slot's +0x40 bit 7 or the command's bit 15 rejects. Its 0x3CF38
 * gate and 0x3C480 continuation are named gaps (§7.6/§7.16). */
int fighter_attack_consume(u32 side);

/* The machine's and chain's per-function fixtures (record §7.1-§7.5, §7.7-§7.9
 * and §7.11) exercise these directly. */
u32  hit_frame_desc(u32 side, u32 i);                 /* 0x3C600 */
void hit_slot_seed(u32 side, u32 value, u32 i);       /* 0x3C6A8 */
s32  hit_scan(u32 side);                              /* 0x3CD44 */
int  hit_stance_ok(u32 side, u32 i);                  /* 0x3CCEC */
int  hit_reaction_drive(u32 side, u32 i);             /* 0x3CE58 */
int  hit_gate(u32 side, u32 i);                       /* 0x3CE24 */
int  hit_immunity(u32 side, u32 i);                   /* 0x3CD94 */
u16  hit_reaction_a(u32 side);                        /* 0x3CBC4 */
u16  hit_reaction_b(u32 side);                        /* 0x3CC58 */
void hit_flash_pair(u32 side);                        /* 0x34D8C */
void hit_reaction_apply(u32 side, u32 reaction);      /* 0x34E2C */
void hit_anim_ctx(u32 out[6], u32 rec);               /* 0x339AC */
int  hit_reaction_allow(u32 side, u32 reaction);      /* 0x4CE70 */
int  hit_geometry(u32 side, u32 table, u32 idx);      /* 0x1DDF4 */
void hit_anchor_set(u32 side, u32 x, u32 y);          /* 0x188AC */
void hit_anchor_x(u32 side, u32 x);                   /* 0x188DC */
void hit_anchor_y(u32 side, u32 y);                   /* 0x1890C */
void hit_sound(u32 ch);                               /* 0x32BAC */

#endif /* PRAGE_GAME_FIGHTER_H */
