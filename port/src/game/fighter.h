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

/* 0x18950. The move-connectivity query fighter_pass_a's winner gate makes: 1
 * when bit state(param_2) is set in the pair table row for characters
 * char(param_1)/char(param_2), at param_1's state record (the dword at
 * row + state(param_1)*8, or its +4 twin when state(param_2) >= 0x20). The row
 * is PTR_DAT_000a1290[char(param_2) + char(param_1)*10] (100 dwords at
 * 0xA1290). */
int fighter_connect_query(u32 param_1, u32 param_2);

/* 0x1958C. The first per-frame fighter pass. Gated on DS_001078FA == 2; per
 * side it calls 0x33950/0x19020/0x3AFC4, clears DS_00100AF8/AFC entries, then
 * picks a winner from the two 0x18950 reachabilities and, on an exact tie,
 * draws rng(2) at 0x19714. 0x19020/0x193B0 are named gaps (§7.6); the gates,
 * the flag stores and the RNG site are ported. */
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
 * 0x35838/0x2BC30 transitions. Its +0x42 bit 6/7 arms call 0x37178/0x37D18. */
void fighter_state_default(u32 side);

/* 0x37D18. The 0x349C8 +0x42 bit-7 arm's callee: set the slot's +0x52/+0x53/
 * +0x54 = 9/3/3, set +0x42 bit 2 (clearing bit 5), start the 0xC9238[char]
 * animation at 2.0, write the +0x74 timer = 0x309 and reset the 0x1078DC
 * approach-table pointer to 0xBD89C, then flag the other slot. */
void fighter_37d18(u32 slot, u32 rec);                   /* 0x37D18 */

/* 0x39A10. Write `value` to the +0x74 timer word of the slot named by the
 * record's +0x51 (0x107824 + side*0x94). Called by 0x37D18 and the pose chain. */
void fighter_39a10(u32 rec, u32 value);                  /* 0x39A10 */

/* 0x37178. The 0x349C8 +0x42 bit-6 arm's callee (the approach machine). */
void fighter_37178(u32 slot);                            /* 0x37178 */

/* 0x36870. The +0x54 machine 0x37178/0x379C4 call; its mode-0x25 arm runs
 * 0x385B0. */
void fighter_36870(u32 rec);                             /* 0x36870 */

/* 0x385B0. The mode-0x25 slot reset 0x36870 runs. */
void fighter_385b0(u32 rec);                             /* 0x385B0 */

/* 0x36638. Reset the slot's +0x43 bit 0x40 and restart the fighter's animation
 * per slot+0x54. Called by 0x349C8, 0x35838 and the 0x34B6C position branch. */
int fighter_state_36638(u32 slot, u32 rec);

/* 0x35D7C. The +0x52 == 3 handler: clear slot+0x53/+0x54, then, when the
 * 0x3CF38 hit chain reports no hit, arm slot+0x54 = 2, slot+0x53 = 4. */
void fighter_state_35d7c(u32 side);

/* The remaining 0x34B14 +0x52 handlers (record §2.2). One C function per
 * original; the entry addresses are the table's 0x34B14 entries. */
void fighter_state_35f84(u32 slot, u32 rec);             /* 0x35F84, +0x52=4 */
void fighter_state_361c8(u32 slot, u32 rec);             /* 0x361C8, +0x52=12 */
void fighter_state_36300(u32 slot, u32 rec);             /* 0x36300, +0x52=13 */
void fighter_state_36710(u32 slot, u32 rec);             /* 0x36710, +0x52=17 */
void fighter_state_399cc(u32 side);                      /* 0x399CC, +0x52=7 */
void fighter_state_36430(u32 slot, u32 rec, u32 side);   /* 0x36430, +0x52=5 */
void fighter_state_364fc(u32 slot, u32 rec, u32 side);   /* 0x364FC, +0x52=21 */

/* The five remaining 0x34B14 +0x52 handlers (record §1). One C function per
 * original; the entry addresses are the table's 0x34B14 entries. */
void fighter_state_359e0(u32 slot, u32 rec, u32 side);   /* 0x359E0, +0x52=1 */
int  fighter_state_35c1c(u32 slot, u32 rec);             /* 0x35C1C, +0x52=2 */
void fighter_state_35d20(u32 slot, u32 rec);             /* 0x35D20, +0x52=2 */
void fighter_state_37464(u32 side);                      /* 0x37464, +0x52=8 */
void fighter_state_33b00(u32 side, u32 src, u32 dst2);   /* 0x33B00, +0x52=19 */
void fighter_state_35e6c(u32 slot, u32 rec);             /* 0x35E6C, +0x52=20 */

/* 0x1A640. 0x1000 when this side's facing bit 0x4000 is clear and the command's
 * 0x1000 is set; 0x2000 in the mirror; else 0. The mirror of 0x1A5D4. */
int fighter_1a640(u32 side);

/* 0x3AAFC. The reaction applier / pose dispatcher 0x3B714 runs for the winner:
 * the facing latch, the 0x39834 pose driver, the 0x3A0FC effect spawn, and the
 * dispatch on the two reaction animation words and slot+0x54. EAX = slot, the
 * stack argument = the reaction byte. */
void fighter_reaction_apply(u32 slot, u32 reaction);

/* 0x3A280. The reaction predicate: 1 for a byte in 0x10..0x17 or 0x20..0x3F. */
int  fighter_3a280(u32 code);

/* 0x3B714. The reaction applier the winner's 0x193B0 runs: EAX = param_1 (the
 * other slot), EDX = param_2 (the winner's slot). It runs the 0x3C59C frame
 * gate, seeds the reaction state through 0x3B080/0x3AE9C, then dispatches
 * through 0x3AAFC (the reaction/pose applier) or 0x3AD98 (the effect spawn).
 * Its new callees 0x3B080/0x3AE9C and the gates 0x39EFC/0x3B038/0x3B6C4 are
 * exposed for the reaction tests. */
void fighter_reaction(u32 param_1, u32 param_2);         /* 0x3B714 */

/* 0x3AD98. The winner's reaction-effect spawn 0x3B714 runs when the command
 * dispatch returns non-zero: play the per-character voice (out of scope), spawn
 * the 0xBB0B0 effect actor at the 0x100AD8-derived offset and start the
 * 0xE8E08/22/3C stream selected by word[anim[2]], then nudge the slot's +0x5A
 * through 0x392A0. EAX = side, EDX = &anim. */
void fighter_3ad98(u32 side, const u32 anim[3]);         /* 0x3AD98 */
int  fighter_39efc(u32 side);                            /* 0x39EFC */
int  fighter_3b038(u32 side);                            /* 0x3B038 */
int  fighter_3b6c4(u32 side);                            /* 0x3B6C4 */
void fighter_3b080(u32 side, u32 param_2, u32 param_3, u32 param_4); /* 0x3B080 */
void fighter_3ae9c(u32 side, u8 param_2);                /* 0x3AE9C */

/* 0x193B0. The winner's per-frame body fighter_pass_a's tail runs for one side
 * when DS_00100AF8/DS_00100AFC survives and the side's +0x8A byte is set. EAX =
 * side. It runs the facing latch, the 0x3962C/0x396AC reaction gates or the
 * +0x84 count compare, and dispatches the winner's reaction through 0x3B714. */
void fighter_winner_body(u32 side);                      /* 0x193B0 */

/* 0x46534. Add `delta` to the per-side AI-difficulty accumulator at
 * DS_001082C8[side], clamp to [0, byte[0xC9408 + byte[0x10452C]]], then raise to
 * DS_001082D0. Called by 0x4F434. */
void fighter_46534(u32 side, s32 delta);

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

/* 0x3C59C. Test-and-set bit `bit` of DSD(DS_00107D50 + side*4): 0 the first
 * time (and sets it), 1 when already set. fight_hud_pass's preamble gate. */
int fighter_pass_flag(u32 bit, u32 side);

/* 0x3531C. The +0x53 dispatcher fight_hud_pass calls at 0x35803. Cases 0, 0xd
 * and >0xf run 0x350D0; 4 increments +0x56; 7 runs the +0x41 bit 7 / +0xC
 * callback and, for char 4 with a live +0x5F and +0x86 > 0x5A, 0x367DC; 8 runs
 * the 0xBDBE8/+0x88 gate, 0x34E20(+0x5F), clears +0x53 and calls the 0x3CF38
 * chain directly at 0x354BC; 10 runs the +0x10 callback. */
void fighter_state_3531c(u32 side);

/* 0x350D0. The core per-frame body 0x3531C's default runs: the +0x52/+0x53/
 * +0x54 state drive (0x3BDDC at 0x3520E), the direct 0x3CF38 call at 0x352A6,
 * and the 0x1DE64/0x34E2C reaction on a miss. */
void fighter_state_350d0(u32 side);

/* 0x39280. Clear slot+0x5D and the slot's +0x43 bit 2. */
void fighter_state_39280(u32 side);

/* 0x34DDC. 0 when slot+0x30 is above word[0xBD870 + char*2] and the record's
 * +0x36 is negative, else 1. */
int fighter_34ddc(u32 side);

/* 0x34E20. 1 when the reaction byte is below 0x18. */
int fighter_34e20(u32 reaction);

/* 0x39040. The per-side round/timer pass; its tail clears the +0x107D2C/
 * 0x107D20/0x107D24 words and the 0x107A80 table. */
void fighter_39040(u32 side);

/* 0x1DE64. The reaction picker: map the side's command word (or, with slot+0x63
 * clear, the 0x46460/0x4649C input scan — a named gap) through 0x1DDF4 to a
 * reaction code; 0xFF when nothing maps. */
u32 hit_reaction_pick(u32 side, u32 stance);

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
