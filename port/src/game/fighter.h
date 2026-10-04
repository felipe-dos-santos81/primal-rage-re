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
void fighter_pose_start(u32 side, u32 edx, u32 ebx, u32 ecx, u32 frame);

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

/* 0x18460 (record §50-E). The anchor pre-dispatch 0x18540 calls: 1 when it
 * reaches its 0x18428 dispatch (which has no effect), 0 when a slot is missing,
 * the side's sprite id lies in the character's range or is 0x1E1. */
int fighter_18460(u32 side);

/* 0x3C570. Test-and-set bit `bit` of DS_00107EE0: 1 when it was already set,
 * else set it and return 0. The camera page tails test bits 0..3 and
 * fighter_pass_b bit 5; fight_slot_clear (0x3C5CC) clears the word per frame. */
int fighter_slot_flag(u32 bit);

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
 * draws rng(2) at 0x19714. 0x19020 (the slot +0x18 hook) and 0x193B0
 * (fighter_winner_body, wired at this pass's 0x1974D tail) are ported. The
 * gates, the flag stores and the RNG site are ported. */
void fighter_pass_a(void);

/* 0x19068. The second per-frame fighter pass, called with arg = 0 by the
 * arena frame. Gated on DS_00107802/DS_00107896 != 0x13; per side it runs the
 * hit-stun/timer update over DS_00100B58/B5A/B5C/B5E and the fighter record,
 * calling 0x3C570, 0x1922C (hit_stance_timer, record §49-B) and 0x3CF38
 * (hit_chain_resolve). No RNG. Fully ported. */
void fighter_pass_b(u32 arg);

/* 0x186D0. The slot position latch the game_frame tail (0x25438) calls per live
 * side, and the 0x2545C mode tail calls at 0x2551E/0x25559/0x2559E. With slot+0x42 bit 3 set it copies the fighter record's +0x18/+0x1C to
 * slot+0x2C/+0x30; otherwise the 0x18540/0x18350 screen-anchor path (both
 * ported, fighter.c) offsets them by DS_00100AB0/AB4[side]. A set slot+0x41 bit 7 then
 * latches slot+0x2C into slot+0x34. */
void fighter_slot_latch(u32 side);

/* 0x186C4. Latch side 0 then side 1 through 0x186D0 (fight_hud_pass's 0x35829
 * tail). */
void fighter_slot_latch_both(void);

/* 0x33EB4. The demo-fight fighter spawn entry state 6 calls after each
 * character pick. It picks the 0x4000/0 stack argument by side, reads the
 * per-side initial x from DS_000BDA38 (a dword load shifted right 16), and runs
 * 0x33C78: it stores the slot pointer into DS_001077A8[side], copies the picked
 * character (DS_0010816A[side]) into slot+0x7A, spawns the fighter and its
 * secondary actor through actors.c's 0x2AE14, assigns the character palette
 * (0x29BC8), and resets the slot fields, then runs 0x494A8's dust entry
 * (0x33E43, when DS_00104B14 == 0) and the sound-bank res_resolve tail
 * (0x33E51..0x33EA6, behind the 0x1CEBC gate); both are ported. */
void fighter_spawn(u32 side);

/* 0x34978. The fighter-slot reset 0x20DF4 calls at 0x20E42 before state 6
 * spawns the fighters: the two slot pointers DS_001077A8[0..1] (0x654C7, a
 * two-dword fill), the word DS_001078F6 and the live-fighter count
 * DS_001078FA, which 0x33C78 increments per spawn and 0x1958C/0x34D8C gate on
 * == 2. */
void fighter_slots_reset(void);

/* 0x1A6AC. EAX = slot, EDX = rec. 0x18B04 for rec+0x51's side, then with
 * slot+0x54 == 0 (1) and slot+0x43 bit 0x20 (0x10) clear, start the stream
 * 0xC8F40[char] (0xC8F90[char]) through 0x3C480 at 3.0 and set that bit
 * alone of the pair. */
void fighter_block_anim(u32 slot, u32 rec);

/* 0x1A7CC. The block start 0x1AB5C calls (0x1AC7A): +0x43 bit 1 off,
 * 0x18B04, +0x61/+0x60/+0x62 = 0/0x1E/1, +0x60 from the other side's move
 * record (0x3AFC4 triple[0] + 0xA), 0x1A6AC, the other side's 0x100CE0
 * counter and its 0xA2C4C percentage of +0x60, then +0x52/+0x53 = 6/1. */
void fighter_block_start(u32 side);

/* 0x1A8F4. EDX = rec (EAX unread). The block end: restart the stance stream
 * 0xC8F68[char] (0xC8FB8[char] when +0x54 == 1) at 3.0 through 0x2BC30, then
 * +0x43 &= 0xCF and +0x52/+0x53/+0x62/+0x60 = 9/0/0/0. */
void fighter_block_end(u32 rec);

/* 0x1A734. EAX = side; its only caller is 0x3B298 (0x3B443), right after it
 * set +0x43 bit 0x20 or 0x10. 0x18B04, +0x61 = 0x0C (capped at +0x60 when
 * 0x0C > (s8)+0x60 and +0x62 != 0), then, ungated, restart the block stream
 * at 3.0 through 0x3C480: bit 0x20 set gives +0x54 = 0 and 0xC8F40[char],
 * else bit 0x10 set gives +0x54 = 1 and 0xC8F90[char]. */
void fighter_block_hit(u32 side);

/* 0x1975C. The think step the arena frame calls at 0x264CC. It runs the
 * projectile collision step 0x17CB0 (camera.h), then, for each thrower whose
 * DS_00100AD0 overlap count exceeds 2, applies the projectile hit to the struck
 * side through the driver 0x3B464 and bursts the projectile (0x3B938). The raw
 * takes no argument; 0x3B464's projectile +0x48 == 8 arm runs 0x1922C and
 * 0x235C4 (demo-pose record §26, §41-C). */
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

/* 0x3A43C. The 0x3A504 pose family's per-frame handler: phase 0 arms +0x58;
 * phase 1 starts the self record's 0xC8FE0[char] stream at 3.0, re-anchors the
 * self record and snaps the self x to A[side] behind the B[side] and +0x90
 * gates. 0x3531C case 10 resolves it from slot+0x10; registered in actors_init.
 * EAX = slot (dead), EBX = side. */
void fighter_pose_3a43c(u32 slot, u32 side);

/* 0x3A6D4. The 0x3A79C pose family's per-frame handler: 0x3A43C's body with the
 * 0xC9008[char] stream, the 0x107D04/0x107D08 B/A words and +0x90 = 3. 0x3531C
 * case 10 resolves it from slot+0x10; registered in actors_init. EAX = slot
 * (dead), EBX = side. */
void fighter_pose_3a6d4(u32 slot, u32 side);

/* 0x3A588 (record gameplay-u6 §U6.3). The 0x3A650 pose family's per-frame
 * handler: 0x3A43C's body with the 0xC9030[char] stream, the 0x107D00/0x107D0C
 * B/A words and +0x90 = 2. 0x3531C case 10 resolves it from slot+0x10;
 * registered in actors_init. EAX = slot (dead), EBX = side. */
void fighter_pose_3a588(u32 slot, u32 side);

/* 0x39CC8. The 0x39F40 knockback pose's per-frame handler (slot+0x10, which
 * 0x39F40 stores at 0x39F8F): the slot +0x14 callback through 0x35050, then a
 * +0x58 machine. 0 arms; 1 launches through 0x39B30 (gravity, vertical and
 * signed horizontal speed from the DS_00107A60/68/70/78 words); 2 waits for the
 * fall and re-times it to the ground; 3 lands (the 0xBEDB0 stream, the 0xBB1DC
 * dust); 4 clears +0x54. 0x3531C case 10 resolves it from slot+0x10; registered
 * in actors_init. EAX = slot (dead), EBX = side. */
void fighter_39cc8(u32 slot, u32 side);

/* 0x347B8. The knockdown floor, an animation-opcode 0x15 target in the
 * characters' knockdown streams (the raptor's landing stream 0xD2ADA: the
 * D500 word at 0xD2B00, the dword 0x000347B8 at 0xD2B02; the dword occurs 53
 * times in the data): +0x74 = 0x29A, the record's speeds cleared,
 * state 9/0x0B/0, then (through the 0x340BC stun gate and 0x34168) the
 * character's floor stream at hold 3.0; game mode 7 may freeze the side
 * instead. EAX = rec. */
void fighter_347b8(u32 rec);

/* 0x340BC. The stun gate 0x347B8 asks: 1 when the side's word +0x8C <= 0,
 * neither slot's +0x63 is set and the side's +0x5A is in 0x55..0x77 and more
 * than 0x3C above the other side's. EAX = side. */
int  fighter_340bc(u32 side);

/* 0x346F8. The get-up, an animation-opcode 0x15 target after the floor
 * streams' lying loop (the raptor's 0xD28FC at 0xD2912; 13 data sites): the side's word +0x76 = word[0xBDBE6] + 1, then
 * 0x36870(rec). EAX = rec. */
void fighter_346f8(u32 rec);

/* 0x35938. The walk entry, an animation-opcode 0x15 target (14 data sites,
 * two per character; the T-rex's D500 word at 0xE6EE8): with
 * rec+0x14 set, state 1/0 (state 8 when +0x54 is 4), rec+8 seeked to the
 * literal sprite id of 0x35C1C's +0x43-selected frame table at the record's
 * frame rec+0x52, then rec+0x52 = 0, rec+0x20 = rec+0x24 = 0, rec+0x58 = 1,
 * rec+0x28 |= 0x804. EAX = rec. */
void fighter_35938(u32 rec);

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
 * dispatch returns non-zero: play the per-character voice (record k7-k12
 * §7), spawn the 0xBB0B0 effect actor at the 0x100AD8-derived offset and
 * start the 0xE8E08/22/3C stream selected by word[anim[2]], then nudge the
 * slot's +0x5A through 0x392A0. EAX = side, EDX = &anim. */
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

/* 0x3BB90. The fighters' body push the game_frame DS_00104B15 tail runs at
 * 0x2541D, before 0x12D48 (and the mode-0x21 tail at 0x25509). Clears DS_00107D30; with DS_001078FA == 2 and both
 * DS_001077A8 slots live it re-latches both slots (0x186D0), and unless either
 * slot's +0x42 bit 2 is set it copies their +0x2C/+0x30 to DS_000D3388..D3394,
 * sums the 0xBEEF8 character widths (halved for a side whose +0x54 is 2) into
 * DS_000D33A8, and when 0x4FB20's body distance is non-zero pushes the sides
 * apart by the penetration (0x3BAEC -> 0x3B9D8 per side -> 0x1883C). Returns
 * 1 when it pushed, else 0 (both callers ignore it). */
u32 fighter_body_push(void);

/* 0x46534. Add `delta` to the per-side AI-difficulty accumulator at
 * DS_001082C8[side], clamp to [0, byte[0xC9408 + byte[0x10452C]]], then raise to
 * DS_001082D0. Called by 0x4F434. */
void fighter_46534(u32 side, s32 delta);

/* 0x4660C — record §48-Y. Recomputes the ceiling DS_001082D0 fighter_46534
 * clamps up to. v = the caller's byte; b = DS_0010452C (the difficulty
 * index). t = the zero-extended byte 0xC9388[v + b*7] (0x46620/0x46626 or
 * 0x46644/0x4664A, both `lea eax,[ebx*8]; sub eax,ebx` = b*7). DS_00104B11
 * (already incremented by the caller) at or below 1 (0x46619/0x4661C):
 * DS_001082D0 = t. Above 1: DS_001082D0 = t - (DS_00104B11 - 1)
 * (0x46639/0x4663C), both clamped up to 0 (0x46660..0x46666). EBX/ECX/EDX are
 * pushed and popped. Only caller: 0x28717 (0x286BC, record §48-Y). */
void fighter_4660c(u32 v);

/* 0x41310. Add `delta` to the camera-target record DS_001077A8[side]'s +0x3C
 * (mode 3 excluded); a negative delta that would leave it <= 0 stores 0.
 * Exported for mode 0xD's 0x274FC (record §48-Q). */
void fighter_41310(u32 side, s32 delta);

/* 0x29BC8. EAX = side, EDX = the character, EBX = the record: the handle
 * 0xA8A98[ch][DS_00105B34[side]] set on the record's pset (0x2A17C, word 0).
 * Exported for 0x42724 (record §48-D). */
void fighter_29bc8(u32 side, u32 rec, u32 ch);

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

/* 0x38C5C. Clear side's combo text cells (rows 8, 9..14 and 0x10). */
void fighter_38c5c(u32 side);
/* 0x38D24. Count down the 0x107D18 combo-text timer; the step reaching zero
 * clears the text and redraws string 0xE5 on rows 6/7. */
void fighter_38d24(u32 side);
/* 0x38D90. Draw side's combo text: the hit count and "\x1bCOMBO" down col
 * side*0x25 + 2 from row 8, and (slot+0x63 clear) the 0x107D20 value and "%"
 * on row 0x10. */
void fighter_38d90(u32 side);
/* 0x38ED0. EAX = side, EDX = a 0x44-byte combo record: 1 (and, with slot+0x63
 * clear, the record's name strings on rows 6/7) when its 0x107A80 needs and a
 * hit-count threshold are met, else 0. */
u8 fighter_38ed0(u32 side, u32 rec);
/* 0x38FEC. Walk the character's combo records through 0x38ED0 until one
 * names a combo. */
void fighter_38fec(u32 side);

/* 0x39040. The per-side combo pass; its tail clears the +0x107D2C/
 * 0x107D20/0x107D24 words and the 0x107A80 table. */
void fighter_39040(u32 side);

/* 0x1DE64. The reaction picker: map the side's command word (or, with slot+0x63
 * clear, the 0x46460/0x4649C input scan, record §49-B) through 0x1DDF4 to a
 * reaction code; 0xFF when nothing maps. */
u32 hit_reaction_pick(u32 side, u32 stance);

/* 0x3BDDC. The attack/command consumer the mapper's 0x8000 arm calls behind
 * 0x3BDB0. Reads the side's command word DS_001088E0/E2; when bit 15 is set it
 * clears the record's +0x34/+0x43/+0x42, sets the slot's +0x5F to 0xFF and,
 * unless the 0x3CF38 chain hits, starts the record's 0xC8B30[char] animation
 * through 0x3C480 and writes the attack state (DS_00107802/03/04,
 * DS_00107D40 + side*4, DS_001078F8 + side, DS_001077FE + side*0x94). Returns
 * 1 on the transition, 0 when the slot's +0x40 bit 7 or the command's bit 15
 * rejects. */
int fighter_attack_consume(u32 side);

/* 0x18B04. The attacker/defender facing flag (see its header comment in
 * fighter.c); exported for fight_health_sync's case 0x12 (0x34D17). EAX =
 * side. */
void hit_facing_flag(u32 side);

/* 0x35E04. The animation-opcode 0x10 target in the characters' 0xC8B30
 * attack streams: with rec+0x14 set, hold 3.0f into rec+0x20/+0x24 and launch
 * the side through 0x3BC70 (state 4/0/2; gravity, vertical and signed
 * horizontal speed from the DS_00107D40 row). EAX = rec. */
void fighter_35e04(u32 rec);

/* 0x3E62C. The T-rex's reaction-0x2B callback (0x34E2C's *(u32*)0xA3884): the
 * 0xE7BDE stream at hold 3.0 through 0x3C4CC, the x re-anchor, slot +0x57 = 2,
 * state 9/7/2 and the +0x0C/+0x18/+0x1C callbacks 0x3E524/0x3E484/0x3E4C4.
 * EAX = slot, EDX = rec (the EBX side is unused). */
void fighter_3e62c(u32 slot, u32 rec, u32 side);

/* 0x3BF70. The forced attack, 0x34E2C's reaction callback 0x3F (*(u32*)0xA3A14
 * for the T-rex): unless the side's slot +0x40 bit 7 is set, the side's
 * record's (ctx[4], DSD(0x1077B0 + side*0x94)) +0x34/+0x43/+0x42 cleared,
 * slot +0x5F = 0xFF, DS_00107D40 + side*4 = the 0xBEFA0 row, the
 * 0xC8B30[char] attack at hold 2.0 through 0x3C4CC, state 3/4/2, slot +0x40
 * bit 7, DS_001078F8 + side = 1 and slot +0x4E = 0xFFFF when 0x1A570(side) is
 * non-zero, else 1. Returns 1, or 0 on the bit-7 reject.
 * EAX = slot, EDX = rec, EBX = side. */
int fighter_3bf70(u32 slot, u32 rec, u32 side);

/* 0x3C0A4. 0x34E2C's reaction callback 0x3E (*(u32*)0xA3A00 for the T-rex):
 * 0x3BF70, then on success slot +0x4E the other way (1 when 0x1A570(side) is
 * non-zero, else 0xFFFF). Returns 0x3BF70's result. EAX = slot, EDX = rec,
 * EBX = side. */
int fighter_3c0a4(u32 slot, u32 rec, u32 side);

/* 0x3D17C. The T-rex's reaction-0x20 callback (0x34E2C's *(u32*)0xA37A8):
 * unless the slot's +0x08 is set, the 0xE84C8 stream at hold 3.0 through
 * 0x3C4CC, state 0xB/6/0, the +0x0C/+0x18/+0x1C callbacks cleared, +0x64 =
 * +0x5F, +0x5F = 0xFF and the word 0x1080AC[rec+0x51] = 0x100. EAX = slot,
 * EDX = rec (the EBX side is unused). */
void fighter_3d17c(u32 slot, u32 rec, u32 side);

/* 0x3D214. The opcode-0x11 target of the reaction-0x20 stream (0xE84CC):
 * with rec+0x14 set, spawns the emitter 0xBB27C as the record's child (slot in
 * its +0x14, +0x59 = 2, +0x60 = 1; its index into the record's +0x4B). EAX =
 * rec. */
void fighter_3d214(u32 rec);

/* 0x3D26C. The opcode-0x11 target of the emitter's stream (0xE85A8): with the
 * emitter's +0x14 (the slot) set, spawns the projectile 0xBB268 beside the
 * slot's record into the slot's +0x08, at the horizontal speed of the word
 * 0x1080AC[side]. EAX = the emitter. */
void fighter_3d26c(u32 rec);

/* 0x3E524. The slot +0x0C callback 0x3531C case 7 runs each frame after
 * 0x3E62C: the +0x57 rise/fall/landing machine. EAX = slot, EDX = rec,
 * EBX = side. */
void fighter_3e524(u32 slot, u32 rec, u32 side);

/* 0x3E4E4. The animation-opcode 0x15 target in the 0xE7BDE stream: with
 * rec+0x14 set, restart at 0xE7BFA (hold 3.0), vertical speed 0x320, gravity
 * 0x23, slot +0x57 = 0. EAX = rec. */
void fighter_3e4e4(u32 rec);

/* 0x3E4C4. The slot +0x1C callback 0x3E62C arms, called by 0x193B0 at
 * 0x19505 with EAX = side: 0x3B714(slot[1-side], slot[side]). */
void fighter_3e4c4(u32 side);

/* 0x3E3A8. The T-rex's reaction-0x2A callback (0x34E2C's *(u32*)0xA3870):
 * ctx = 0x33950(side); ctx[4] on the 0xC8950[char] stream at hold 2.0 through
 * 0x3C4CC, ctx[2]'s state 9/7/0, the +0x0C/+0x18/+0x1C callbacks
 * 0x3E328/0x3E1D0/0x3E244, +0x57 = 0 and +0x41 bit 7. EBX = side (the EAX
 * slot and EDX rec are overwritten). */
void fighter_3e3a8(u32 slot, u32 rec, u32 side);

/* 0x3E328. The slot +0x0C callback 0x3531C case 7 runs each frame after
 * 0x3E3A8: ctx = 0x33950(side); ctx[2]'s +0x57 steps 0 -> 1 once +0x86 >> 16
 * exceeds 3, and 1 -> 2 with +0x8A = 0, the 0xE84B6 stream at hold 4.0 on
 * ctx[4] through 0x3C4CC, 0x3E0F0's child (+0x53 = 1) and +0x52 = 9. EBX =
 * side (the EAX slot and EDX rec are overwritten). */
void fighter_3e328(u32 slot, u32 rec, u32 side);

/* 0x18C14. The guarded-check walk: EAX = side, EDX = 16 flag bytes (2 skips a
 * check, 0 returns 1 when it holds, 1 when it fails; flag 0 is inverted and
 * rewritten to 4/3), EBX/ECX = two box tables (0 selects 0xA1818/0xA1822).
 * Returns 0 only when every check passes. */
void fighter_18bd4(u8 flags[16]);                        /* 0x18BD4 */
int fighter_18c14(u32 side, u8 flags[16], u32 box_a, u32 box_b);

/* 0x19020. 0x1958C's per-slot hook call (0x195B6): with the side's slot +0x18
 * set, DS_00100AF8[side] = (hook(side) == 0) ? 1 : 0. */
void fighter_19020(u32 side);

/* 0x3E484. The +0x18 hook 0x3E62C stores: 0x18C14 with flags 0 = 1, 1 = 0,
 * 8 = 0 and the default box tables. EAX = side. */
u32 fighter_3e484(u32 side);

/* 0x3F0F0 and 0x3F130 (record gameplay-u6 §U6.17): the 0xD100 targets of the
 * 0xE7B78 and 0xE7BBE streams (EAX = rec): the emitter 0xBB290 as the slot's
 * child, +0x2B bit 0 (0x3F130 also +0x50 = 2). */
void fighter_3f0f0(u32 rec);
void fighter_3f130(u32 rec);

/* 0x3F0A8 and its three callbacks (record gameplay-u6 §U6.13): the T-rex's
 * reaction-0x24 callback (slot, rec, side), the +0x18 hook 0x3EFE0 (fn(side),
 * EAX returned), the +0x1C callback 0x3F020 (fn(side)) and the per-frame
 * +0x0C callback 0x3F054 (slot, rec, side). */
void fighter_3f0a8(u32 slot, u32 rec, u32 side);
u32  fighter_3efe0(u32 side);
void fighter_3f020(u32 side);
void fighter_3f054(u32 slot, u32 rec, u32 side);

/* 0x3D1EC (record gameplay-u6 §U6.14): the T-rex's reaction-0x2D callback. */
void fighter_3d1ec(u32 slot, u32 rec, u32 side);

/* 0x3C048 (record gameplay-u6 §U6.15): every character's reaction-0x3D
 * callback; returns 0x3BF70's AL. */
int fighter_3c048(u32 slot, u32 rec, u32 side);

/* 0x3E1D0. The +0x18 hook 0x3E3A8 stores: 1 unless the slot's +0x86 >> 16 is
 * in 1..3, else 0x18C14 with flags 5 = 1, 1/4/7/8/0xD/0xE = 0 and the
 * 0xC75F5/0xC75FF box tables. EAX = side. */
u32 fighter_3e1d0(u32 side);

/* 0x1490C. The character-3 reaction-0x27 callback (*(u32*)0xA4734), called
 * by 0x34E2C at 0x35045 with (slot, rec, side): 0x14814 starts the reaction
 * stream and arms the slot (+0x0C 0x1461C, +0x18 0x145CC, +0x1C 0x145E4),
 * then the side's 0xFD11C byte = 1. */
void fighter_1490c(u32 slot, u32 rec, u32 side);

/* 0x14814. Character 3's reaction-0x22 callback (*(u32*)0xA46D0), same
 * (slot, rec, side) registers (EBX unread): starts the reaction stream, arms
 * the slot (+0x0C 0x1461C, +0x18 0x145CC, +0x1C 0x145E4) and zeroes the
 * side's 0xFD11C byte. */
void fighter_14814(u32 slot, u32 rec, u32 side);

/* 0x1461C. The slot +0x0C callback 0x14814 stores (0x3531C case 7), same
 * (slot, rec, side) registers: the +0x57 step machine 0..3. */
void fighter_1461c(u32 slot, u32 rec, u32 side);

/* 0x145CC. The +0x18 hook 0x14814 stores: returns 1. EAX = side. */
u32 fighter_145cc(u32 side);

/* 0x145E4. The +0x1C callback 0x14814 stores: 0x39834(ctx[1], ctx[2]'s
 * +0x5F) on 0x33950(side). EAX = side. */
void fighter_145e4(u32 side);

/* 0x14B90 (record §49-W). Character 3's reaction-0x26 callback
 * (*(u32*)0xA4720), 0x14814's twin, same (slot, rec, side) registers (EBX
 * unread): starts the reaction stream through 0x3C520 and arms the slot
 * (+0x0C 0x14988, +0x18 0x14938, +0x1C 0x14950). */
void fighter_14b90(u32 slot, u32 rec, u32 side);

/* 0x14C98 (record §49-W). Character 3's reaction-0x28 callback
 * (*(u32*)0xA4748), 0x1490C's twin: 0x14B90, then the side's 0xFD11A byte = 1. */
void fighter_14c98(u32 slot, u32 rec, u32 side);

/* 0x14988 (record §49-W). The slot +0x0C callback 0x14B90 stores (0x3531C case
 * 7), 0x1461C's twin: the +0x57 step machine 0..3, whose step 2 is 0x14A5C. */
void fighter_14988(u32 slot, u32 rec, u32 side);

/* 0x14938 (record §49-W). The +0x18 hook 0x14B90 stores: returns 1. EAX = side. */
u32 fighter_14938(u32 side);

/* 0x14950 (record §49-W). The +0x1C callback 0x14B90 stores: 0x39834(ctx[1],
 * ctx[2]'s +0x5F) on 0x33950(side). EAX = side. */
void fighter_14950(u32 side);

/* 0x14E44. Character 3's reaction-0x23 callback (*(u32*)0xA46E4), same
 * (slot, rec, side) registers (EBX unread): the 0xD3026 grab stream at 2.0,
 * state 9/7/0, +0x18 0x14CC4, +0x1C 0x14D7C and +0x42 bit 2. */
void fighter_14e44(u32 slot, u32 rec, u32 side);

/* 0x14D7C. The +0x1C throw 0x14E44 stores (0x193B0's 0x19505, EAX = side):
 * 0x18B04 for the other side, the 0x1088BF = 4 gate, both slots' +0x74 =
 * 0x309, 0x3C208 at the other character's 0x9AFA4 distance, 0x39834, the
 * other record on 0xC91C0[its char] at 3.0 and the other slot in 9/4 with
 * +0x41 bit 7. */
void fighter_14d7c(u32 side);

/* 0x3C208. Place the other side |dist| from `side` (0x1883C by the gap, or
 * at the wall through 0x3B8D8/0x3B90C/0x188DC), after latching both slots,
 * 0x18AF8's facing flags and clearing both records' +0x34/+0x42/+0x43.
 * EAX = side, EDX = dist. 10 call sites (0x14D7C's 0x14DDF among them). */
void fighter_3c208(u32 side, s32 dist);

/* 0x3B90C. The side's slot+0x2C plus `delta`, clamped to +/-DS_000BE018 (the
 * arena wall). EAX = side, EDX = delta; 0x3C208's two calls only. */
s32 fighter_3b90c(u32 side, s32 delta);
s32 ai_distance(void);
void fighter_18b44(u32 slot);
int fighter_189fc(u32 side);
int fighter_18a4c(u32 side);
void fighter_1883c(u32 side, u32 a, u32 b);
int fighter_3b8d8(u32 side, s32 delta);

/* 0x14CC4. The +0x18 hook 0x14E44 stores: 1 while the record's +0x61 is
 * clear; else the 0x18C14 checks and the 0x187FC range 0x1900..0x3200 decide
 * the grab, a miss (1) restarts the record on 0xD3062, and +0x61 is cleared.
 * EAX = side. */
u32 fighter_14cc4(u32 side);

/* 0x15350. Character 3's reaction-0x25 callback (*(u32*)0xA470C), same
 * (slot, rec, side) registers (EBX unread): nothing when 0x468D8(the other
 * side) holds; else the 0xD311A stream at 3.0, state 9/7/0 with +0x57 = 0,
 * +0x0C 0x152D4, +0x18 0x15208, +0x1C 0x1527C, +0x42 bit 2 and the side's
 * 0xFD118 byte = 0. */
void fighter_15350(u32 slot, u32 rec, u32 side);

/* 0x152D4. The slot +0x0C callback 0x15350 stores (0x3531C case 7), same
 * registers (EBX = side): at +0x57 == 1 with +0x88 above the 0x9AFF8 byte,
 * +0x57 = 2, 0x2BD44 on the record's +0x4B child and the 0xD315A stream. */
void fighter_152d4(u32 slot, u32 rec, u32 side);

/* 0x15208. The +0x18 hook 0x15350 stores: 0x18C14 (flags 1/4/7/8/0xD/0xE =
 * 0, 5/9 = 1, box tables 0x9AFFA/0x9B001), or 1 while the slot's +0x88 is
 * below the 0x9AFF9 byte. EAX = side. */
u32 fighter_15208(u32 side);

/* 0x1527C. The +0x1C callback 0x15350 stores (0x193B0's 0x19505, EAX =
 * side): 0x39834, 0x36D20 on the other slot, 0x188AC, the other slot's +0x43
 * bits 4/5 cleared and +0x57 = 1. */
void fighter_1527c(u32 side);

/* 0x151C0. Character 3's reaction-0x24 callback (*(u32*)0xA46F8), same
 * (slot, rec, side) registers (EBX unread): the 0xD3078 stream at 3.0,
 * state 9/7/0 with +0x57 = 0, +0x0C = 0, +0x18 0x15160, +0x1C 0x151A0 and
 * +0x42 bit 2. */
void fighter_151c0(u32 slot, u32 rec, u32 side);

/* 0x15160. The +0x18 hook 0x151C0 stores: 0x18C14 with flag 0 = 1, flags
 * 1/8 = 0 and the default box tables. EAX = side. */
u32 fighter_15160(u32 side);

/* 0x151A0. The +0x1C callback 0x151C0 stores: 0x3B714(the other slot, the
 * side's slot). EAX = side. */
void fighter_151a0(u32 side);

/* 0x3B938. Burst slot `slot`'s projectile (slot+0x08): restart it on the
 * burst stream (0xE1898 at 2.0 when its +0x48 is 4, else the per-character
 * 0xBDFC8/0xBDFF0 stream and hold), detach it (+0x48, +0x34/+0x36 and
 * slot+0x08 zeroed) and set slot+0x64 = 0xFF. Called by 0x1975C and 0x17BC8. */
void fighter_3b938(u32 slot);

/* 0x3A95C. The projectile-hit stagger 0x3B464 runs for the struck side:
 * re-anchor the record at its own x (0x188AC), put the slot in 0x10/0x0A/0
 * with no +0x10 handler, start the per-character 0xC8FE0 stream at 3.0 and
 * set slot+0x7E = byte[0xBECF8] + b. EAX = side, EDX = b. */
void fighter_3a95c(u32 side, u32 b);

/* 0x235C4. 0x3B464's projectile +0x48 == 8 arm (0x3B65E, its only caller):
 * snapshot the side's slot and record (0x33ACC into 0x104530/0x104658), the
 * 0x39834 pose driver with reaction 0x2A, then state 0x10/0x0A with the +0x10
 * handler 0x22BEC, +0x18/+0x1C cleared, and 0x22B28's freeze. EAX = side. */
void fighter_235c4(u32 side);

/* 0x22BEC. The +0x10 handler 0x235C4 stores (0x3531C case 10; EAX = slot,
 * EBX = side, only the side is read): the per-side 0x10474C tick, and the
 * +0x58 phases 1 (hold until the tick passes 0x78, at double speed while
 * 0x10476C[side] is set) and 2 (restore the 0x33ACC snapshot through 0x33B00
 * and re-arm 0x29D04 while the +0x14 callback is still pending). */
void fighter_22bec(u32 slot, u32 side);

/* 0x29D04. The slot +0x14 callback 0x22B28 and 0x22BEC store (EAX = slot):
 * 0 while a palette effect is live (DS_0009AF3D), else the side's character
 * palette through 0x2A17C and 1. */
u32 fighter_29d04(u32 slot);

/* 0x370F0. The 0xD000/0xD100 stream target of 21 stream sites (EAX = rec),
 * also called by 0x48AAC and 0x48D94: with both DS_001077A8 slots set, the
 * record's slot takes +0x54 = 3, +0x42 bit 2 and +0x52 = 0x0A; then either
 * the 0xBDC2C[char] stream at 1.0 (DS_00104B14 set) or the other slot's +0x42
 * bit 6, the record's +0x53 = 0 and DS_000F0AFE = 2. */
void fighter_370f0(u32 rec);

/* 0x3C358 (record §42-B). The hold state after a throw placement (EAX =
 * side): the side's slot 9/7 with +0x42 bit 2, the other slot 0x10/0x0A with
 * +0x54 and +0x0C cleared, both slot records' +0x34/+0x43/+0x42 and both
 * context records' +0x1C cleared. 6 call sites (0x3E244's 0x3E2D7 among
 * them). */
void fighter_3c358(u32 side);

/* 0x3E244 (record §42-B). The +0x1C callback 0x3E3A8 stores (0x193B0's
 * 0x19505, EAX = side): the flash pair, the side's record on 0xE843A and the
 * other's on 0xC90F8[its char] at 2.0, 0x3E0F0's child, 0x3C208 at the
 * other's 0xC759C distance, 0x18AF8, 0x39834, 0x3C358, both slots' +0x74 =
 * 0x309, then +0x57 = 2 and the other slot's +0x53 = 0x0F. */
void fighter_3e244(u32 side);

/* 0x36280 (record §42-B). The 0xD000 target of the 0xC90A8/0xC90D0 fall
 * streams (EAX = rec): with an owner slot, the landing (+0x58, 0x188AC,
 * +0x54, +0x42 bit 2, the record's +0x36/+0x24) and a 0xBB1DC spawn. */
void fighter_36280(u32 rec);
/* 0x3640C (record gameplay-u6 §U6.4): the 0xD000 target of seven streams.
 * EAX = rec: +0x52 = 0, hold 3.0, +0x4D = 0x14; the owner slot (rec+0x14),
 * when set, +0x52 = 5. */
void fighter_3640c(u32 rec);
/* 0x37DCC (record gameplay-u6 §U6.5): the 0xD100 target of seventeen
 * streams; sets the byte DS_001078FC = 1. */
void fighter_37dcc(void);

/* 0x48AAC and 0x48D94 (record §42-B). Character 2's two finisher +0x0C
 * callbacks (0x3531C case 7: EAX = slot, EDX = rec, EBX = side), stored by
 * 0x48BE0 and 0x48F54; each ends in 0x370F0 on the other record. */
void fighter_48aac(u32 slot, u32 rec, u32 side);
void fighter_48d94(u32 slot, u32 rec, u32 side);

/* 0x48BE0 and 0x48F54 (record §42-B). Character 2's finisher entries (the
 * 0xBDAE4/0xBDB00 tables), called by 0x379C4 through DS_001078E8 with EAX =
 * slot, EDX = rec: the record's stream at 2.0 and the slot 7/9/0 with the
 * +0x0C callback 0x48AAC/0x48D94; non-zero return. */
int fighter_48be0(u32 slot, u32 rec);
int fighter_48f54(u32 slot, u32 rec);

/* 0x37640, 0x37774 and 0x37898 (record gameplay-u0 §U0.4). The reaction
 * 0x32/0x33/0x34 callbacks of every character (0x3531C/0x34E2C: EAX = slot,
 * EDX = rec, EBX = side): with the other slot finishable (+0x42 bit 5, +0x43
 * bit 3) they arm the finisher: the 0x1078E4 stream, DS_001078E8 (0 /
 * 0xBDAE4[char] / 0xBDB00[char]), 0x37D18 or the +0x40 flags, the score
 * 0x41310(side, 50000). */
void fighter_37640(u32 slot, u32 rec, u32 side);
void fighter_37774(u32 slot, u32 rec, u32 side);
void fighter_37898(u32 slot, u32 rec, u32 side);

/* 0x45C10 (record gameplay-u0 §U0.5). Character 4's 0xBDAE4 finisher entry,
 * called by 0x379C4 through DS_001078E8 with EAX = slot, EDX = rec: the
 * record's stream 0xEB7A0 at 3.0 and the slot 7/9/0 with the +0x0C callback
 * 0x45B50; non-zero return. */
int fighter_45c10(u32 slot, u32 rec);

/* 0x22CE4. The second way into the 0x22BEC freeze (0x22E44's call): 0x33ACC,
 * 0x39834 with the other slot's +0x5F, the other slot's +0x57 = 2, the side's
 * slot in 0x10/0x0A with +0x10 0x22BEC and +0x5F = 0xFF, 0x22B28, then
 * 0x10476A[side ^ 1] = 1. EAX = side. */
void fighter_22ce4(u32 side);

/* 0x22D8C. The +0x18 hook 0x22F74 stores (0x19020, fn(side)): 1 while the
 * other slot is frozen or the side's 0x104750 tick is outside 3 (6 with the
 * slot's +0x76 zero)..0x10, else 0x18C14 on the 0xA8314/0xA8328 box tables. */
u32 fighter_22d8c(u32 side);

/* 0x22E44. The +0x1C callback 0x22F74 stores (0x193B0's 0x19505, fn(side)):
 * +0x57 = 2, the other side frozen through 0x22CE4, +0x74 = 0x29A, the
 * 0xBB3E4 projectile in 0x104728[side] and the update table's bit 5. */
void fighter_22e44(u32 side);

/* 0x22F14. The +0x0C callback 0x22F74 stores (0x3531C case 7; only EBX =
 * side is read): the 0x104750 tick, and +0x57 1 -> 2 past 0x10. */
void fighter_22f14(u32 slot, u32 rec, u32 side);

/* 0x22F74. Character 1's reaction-0x29 callback (*(u32*)0xA3D5C): arms the
 * slot with 0x22D8C/0x22E44/0x22F14, +0x57 = 1, state 9/7/0, zeroes the
 * 0x104750 tick and starts 0xE4952 at 3.0. Returns 1. */
int fighter_22f74(u32 slot, u32 rec, u32 side);

/* 0x22FE8. The update table's entry 5: retire each side's 0x104728
 * projectile on the 0xE8E66 stream once its gate allows, clearing bit 5 when
 * both are gone. */
void fighter_22fe8(void);

/* 0x2365C. Character 1's reaction-0x2A callback (*(u32*)0xA3D70): 0 while the
 * slot holds a projectile or the other slot is frozen (0x22BEC), else the
 * 0xE4996 stream at 3.0, state 0x0B/6/0 and +0x5F moved to +0x64; 1. */
int fighter_2365c(u32 slot, u32 rec, u32 side);

/* 0x230F0/0x23130/0x23178. Character 1's reaction callbacks 0x25/0x20/0x28
 * (*(u32*)0xA3D0C/0xA3CA8/0xA3D48): the 0xE483E/0xE4872/0xE48DC stream at 3.0
 * through 0x3C4CC and state 9/7/0 with +0x0C = 0 (0x230F0 starts the stream
 * before the stores, the other two after); 1. Record §43-C. */
int fighter_230f0(u32 slot, u32 rec, u32 side);
int fighter_23130(u32 slot, u32 rec, u32 side);
int fighter_23178(u32 slot, u32 rec, u32 side);

/* 0x231C0 (record gameplay-u6 §U6.16): character 1's reaction-0x27 callback. */
int fighter_231c0(u32 slot, u32 rec, u32 side);

/* 0x236D8. 0xE4996's 0xD100 target: spawns the 0xBB3BC child of rec with the
 * slot at +0x14. 0x2372C. That child stream's 0xD100 target (also called at
 * 0x2463E): spawns the 0xBB3D0 projectile into the slot's +0x08. §43-C. */
void fighter_236d8(u32 rec);
void fighter_2372c(u32 rec);

/* 0x24568. Entry 1 of the per-character table 0xA8628 (character 1's
 * entrance, EAX = side): the spawn 0x5000 beside DS_0010810D's fighter, the
 * 0xE453A stream, the 0x2372C projectile and state 9/3. 0x246D4. That stream's
 * 0xD500 target, the entrance's end: 0x3BCE0(side) restarts the side on its
 * stance stream DS_000C8B30[char] in state 3/4/2. §46-C. */
void fighter_24568(u32 side);
void fighter_246d4(u32 rec);
void fighter_3bce0(u32 side);

/* The other six entries of 0xA8628 (EAX = side), each with its entrance
 * stream's 0xD500 target (EAX = rec) and that target's callees. §48-V.
 * Character 0: 0x40CB0 (the 0x3000 placement, y = 0x4C00) and 0x40C34.
 * Character 2: 0x49150, 0x492A8 and 0x488B8 (also its reaction-0x22
 * callback, (slot, rec, side)). Character 3: 0x15A34 and 0x159A8.
 * Character 4: 0x45FE8 and 0x46138 (which calls 0x45878). Character 5:
 * 0x40E64, 0x40E14 and 0x3DFC0 (slot, rec). Character 6: 0x24804, its
 * direct callee 0x24754 (side), 0x24964 and 0x23530 (also its reaction-0x24
 * callback, (slot, rec, side)). */
void fighter_40cb0(u32 side);
void fighter_40c34(u32 rec);
void fighter_49150(u32 side);
void fighter_492a8(u32 rec);
void fighter_488b8(u32 slot, u32 rec, u32 side);
void fighter_15a34(u32 side);
void fighter_159a8(u32 rec);
void fighter_45fe8(u32 side);
void fighter_46138(u32 rec);
void fighter_40e64(u32 side);
void fighter_40e14(u32 rec);
void fighter_3dfc0(u32 slot, u32 rec);
void fighter_24804(u32 side);
void fighter_24754(u32 side);
void fighter_24964(u32 rec);
void fighter_23530(u32 slot, u32 rec, u32 side);

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
void hit_anim_start_b(u32 rec, u32 stream, u32 frame_bits); /* 0x3C4CC */
int  hit_reaction_allow(u32 side, u32 reaction);      /* 0x4CE70 */
int  hit_geometry(u32 side, u32 table, u32 idx);      /* 0x1DDF4 */
void hit_anchor_set(u32 side, u32 x, u32 y);          /* 0x188AC */
u32  hit_record_x(u32 side);                          /* 0x18714 */
void hit_anchor_x(u32 side, u32 x);                   /* 0x188DC */
void hit_anchor_y(u32 side, u32 y);                   /* 0x1890C */
void hit_sound(u32 ch);                               /* 0x32BAC */

/* 0x45AD0. Character 4's reaction-0x25 callback (*(u32*)0xA4C0C), the
 * (slot, rec, side) registers (EBX unread): the 0xEB64E curl stream at 2.0,
 * state 9/7/1 with +0x57 = 0, +0x0C 0x45A70, +0x18 0x459F4, +0x1C 0x45A34
 * and the record's +0x4C = 0x78. 0x45A70. Its +0x0C callback: the curl
 * holds while the command word has 0x600, the other slot's +0x42 bit 0x10
 * is clear and the +0x4C countdown stays positive; else the 0xEB692 stream
 * and +0x57 = 1. 0x459F4. The +0x18 hook (0x3E484's body). 0x45A34. The +0x1C
 * callback: 0x3B714(ctx[3], ctx[2]), the 0xEB692 stream and +0x57 = 1.
 * 0x459D0. The curl stream's 0xD100 target: rec+0x63 = 1. §48-A. */
void fighter_45ad0(u32 slot, u32 rec, u32 side);
void fighter_45a70(u32 slot, u32 rec, u32 side);
u32  fighter_459f4(u32 side);
void fighter_45a34(u32 side);
void fighter_459d0(u32 rec);
/* 0x45B18 (record gameplay-u0 §U0.7). The 0xD100 target at 0xEB70E: the
 * child 0xC934C spawned on the record (its index in +0x4B), then 0x37B54. */
void fighter_45b18(u32 rec);
/* Record §48-R. Character 4's reaction callbacks 0x44F64 (0x20), 0x450E8
 * (0x21), 0x455A0 (0x22), 0x44970 (0x23), 0x45878 (0x24), 0x44CFC (0x26) and
 * 0x44B10 (0x2D), the (slot, rec, side) registers of 0x34E2C's 0x35045 call;
 * the +0x0C callbacks they store (0x3531C case 7, same registers) 0x44E64,
 * 0x4505C, 0x452E4, 0x45454, 0x4579C and 0x44C88; the +0x18 hooks (0x19020,
 * fn(side), EAX returned) 0x44D78, 0x44FB4, 0x45158, 0x45640 and 0x44B38; the
 * +0x1C callbacks (0x193B0's 0x19505, fn(side)) 0x44DB8, 0x44FF4, 0x451EC,
 * 0x456A8 and 0x44BAC; their streams' targets (EAX = rec) 0x449B8, 0x44A64
 * (0xD000), 0x44E0C (0xD500), 0x45238 and 0x458D4 (0xD000); 0x4579C's
 * landing 0x45908 (EAX = rec); and 0x3A2A0, the pose knockback of three +0x1C
 * callbacks (EAX = side, EDX/EBX/ECX and a stack word for 0x39F40; returns
 * 0x3B298's verdict, 0 or 1). */
void fighter_44f64(u32 slot, u32 rec, u32 side);
void fighter_450e8(u32 slot, u32 rec, u32 side);
void fighter_455a0(u32 slot, u32 rec, u32 side);
void fighter_44970(u32 slot, u32 rec, u32 side);
void fighter_45878(u32 slot, u32 rec, u32 side);
void fighter_44cfc(u32 slot, u32 rec, u32 side);
void fighter_44b10(u32 slot, u32 rec, u32 side);
void fighter_44e64(u32 slot, u32 rec, u32 side);
void fighter_4505c(u32 slot, u32 rec, u32 side);
void fighter_452e4(u32 slot, u32 rec, u32 side);
void fighter_45454(u32 slot, u32 rec, u32 side);
void fighter_4579c(u32 slot, u32 rec, u32 side);
void fighter_44c88(u32 slot, u32 rec, u32 side);
u32  fighter_44d78(u32 side);
u32  fighter_44fb4(u32 side);
u32  fighter_45158(u32 side);
u32  fighter_45640(u32 side);
u32  fighter_44b38(u32 side);
void fighter_44db8(u32 side);
void fighter_44ff4(u32 side);
void fighter_451ec(u32 side);
void fighter_456a8(u32 side);
void fighter_44bac(u32 side);
void fighter_449b8(u32 rec);
void fighter_44a64(u32 rec);
void fighter_44e0c(u32 rec);
void fighter_45238(u32 rec);
void fighter_458d4(u32 rec);
void fighter_45908(u32 rec);
u32  fighter_3a2a0(u32 side, u32 edx, u32 ebx, u32 ecx, u32 word);
/* Record §48-P. The slot callbacks the entrance poses store: character 0's
 * (0x40C34, and its reaction-0x26/0x27 callback 0x3F3F4) +0x0C 0x3F360, +0x18
 * 0x3F1F0 and +0x1C 0x3F284; character 2's (0x488B8) +0x0C 0x487D4, +0x18
 * 0x48668 and +0x1C 0x486F8; character 6's (0x23530) +0x0C 0x233A8, +0x18
 * 0x23250 and +0x1C 0x232B4. Character 5's reaction-0x22 callback 0x3E064 and
 * the callbacks it stores, +0x0C 0x3DE54, +0x18 0x3DD14 and +0x1C 0x3DD84, and
 * the other slot's +0x10 handler 0x3D424 (0x3531C case 10, (slot, side)) and
 * +0x14 callback 0x3D3E4 (fn(slot), EAX returned) that 0x3DD84 stores. The
 * shapes are §48-R's: reaction and +0x0C callbacks (slot, rec, side), +0x18
 * hooks fn(side) with EAX returned, +0x1C callbacks fn(side). */
void fighter_3f3f4(u32 slot, u32 rec, u32 side);
void fighter_3f360(u32 slot, u32 rec, u32 side);
u32  fighter_3f1f0(u32 side);
void fighter_3f284(u32 side);
/* Record gameplay-u0 §U0.10. The twins of the 0x3F3F4 cluster: character
 * 6's reaction 0x22 (0x2201C: +0x0C 0x21F88, +0x18 0x21E10, +0x1C 0x21EA4)
 * and 0x27 (0x22294: 0x22200, 0x220F4, 0x22188), character 0's 0x2E
 * (0x3F650: 0x3F5BC, 0x3F4B8, 0x3F54C); and character 6's reaction 0x23
 * (0x210C4: 0x210A4, 0x20FA0, 0x20FE0). Setters and +0x0C are (slot, rec,
 * side), +0x18 hooks fn(side) with EAX returned, +0x1C fn(side). */
void fighter_2201c(u32 slot, u32 rec, u32 side);
void fighter_21f88(u32 slot, u32 rec, u32 side);
u32  fighter_21e10(u32 side);
void fighter_21ea4(u32 side);
void fighter_22294(u32 slot, u32 rec, u32 side);
void fighter_22200(u32 slot, u32 rec, u32 side);
u32  fighter_220f4(u32 side);
void fighter_22188(u32 side);
void fighter_3f650(u32 slot, u32 rec, u32 side);
void fighter_3f5bc(u32 slot, u32 rec, u32 side);
u32  fighter_3f4b8(u32 side);
void fighter_3f54c(u32 side);
void fighter_210c4(u32 slot, u32 rec, u32 side);
void fighter_210a4(u32 slot, u32 rec, u32 side);
u32  fighter_20fa0(u32 side);
void fighter_20fe0(u32 side);
void fighter_487d4(u32 slot, u32 rec, u32 side);
u32  fighter_48668(u32 side);
void fighter_486f8(u32 side);
void fighter_233a8(u32 slot, u32 rec, u32 side);
u32  fighter_23250(u32 side);
void fighter_232b4(u32 side);
void fighter_3e064(u32 slot, u32 rec, u32 side);
void fighter_3de54(u32 slot, u32 rec, u32 side);
u32  fighter_3dd14(u32 side);
void fighter_3dd84(u32 side);
void fighter_3d424(u32 slot, u32 side);
u32  fighter_3d3e4(u32 slot);
/* Record gameplay-u0 §U0.9. Character 5's reaction-0x20 cluster: 0x3D73C
 * (slot, rec, side) stores the +0x0C 0x3D674 (case 7), the +0x18 hook
 * 0x3D484 (fn(side), EAX returned) and the +0x1C 0x3D4DC (fn(side)), which
 * arms 0x3D424/0x3D3E4. Its reaction-0x26 cluster: 0x3DA10 stores 0x3D9E4,
 * 0x3D858 and 0x3D8AC, which arms the +0x10 handler 0x3D790 (case 10,
 * (slot, side) as 0x3D424) and 0x3D3E4. */
void fighter_3d73c(u32 slot, u32 rec, u32 side);
void fighter_3d674(u32 slot, u32 rec, u32 side);
u32  fighter_3d484(u32 side);
void fighter_3d4dc(u32 side);
void fighter_3da10(u32 slot, u32 rec, u32 side);
void fighter_3d9e4(u32 slot, u32 rec, u32 side);
u32  fighter_3d858(u32 side);
void fighter_3d8ac(u32 side);
void fighter_3d790(u32 slot, u32 side);
/* Record §49-U: the pose/animation slot callbacks 0x3EC20/0x3EF44/0x3FB88
 * store. 0x3EA24, 0x3EE00 and 0x3F9C8 are +0x0C callbacks (slot, rec, side);
 * 0x3E6A8 is a +0x10 handler, (slot, side) as 0x3D424. */
void fighter_3e6a8(u32 slot, u32 side);
void fighter_3ea24(u32 slot, u32 rec, u32 side);
void fighter_3ee00(u32 slot, u32 rec, u32 side);
void fighter_3f9c8(u32 slot, u32 rec, u32 side);
/* Record §50-A: the setups the pose callbacks above are stored by
 * (0x3EC20 called by 0x3E9A4; 0x3EF44/0x3FB88/0x3ECF8 reaction-row callbacks,
 * (slot, rec, side)), the +0x18 hooks 0x3E924/0x3ED78 (fn(side), EAX
 * returned) and the +0x1C callbacks 0x3E9A4/0x3EDB8/0x3FDD8 (fn(side)),
 * the +0x0C callback 0x40954, 0x3E8E4 (no reference), the confetti burst
 * 0x406B4 and the winner-pose initializer 0x36F10 (EAX = the other slot's
 * table entry, fight_health_sync's arm). */
void fighter_3ec20(u32 slot, u32 rec);
void fighter_3e9a4(u32 side);
void fighter_3e8e4(u32 side);
void fighter_3e800(u32 side);
u32  fighter_3e924(u32 side);
void fighter_3ecf8(u32 slot, u32 rec, u32 side);
u32  fighter_3ed78(u32 side);
void fighter_3edb8(u32 side);
void fighter_3ef44(u32 slot, u32 rec, u32 side);
void fighter_3fb88(u32 slot, u32 rec, u32 side);
void fighter_3fdd8(u32 side);
void fighter_40954(u32 slot, u32 rec, u32 side);
void fighter_406b4(void);
void fighter_36f10(u32 slot);
int  fighter_40bbc(u32 slot, u32 rec);
int  fighter_3ff08(u32 slot, u32 rec);
/* Record §52-A: the callbacks those setups store (the +0x18 hooks 0x3F7F4/
 * 0x3FD30/0x21A88/0x21B74, fn(side), EAX returned; the +0x1C callbacks
 * 0x3F85C/0x21B08/0x21BE8, fn(side); the +0x0C callback 0x3FEF8, (slot, rec,
 * side)), the reaction-row callbacks 0x3FFDC (char 5, 0x23; it calls 0x3FF08)
 * and 0x21C84/0x21D10 (char 1, 0x23/0x24), (slot, rec, side), their shared
 * 0x215B0 (EAX = side) and the confetti pool: the init 0x40608 and the
 * update-table entries 13 (0x40554) and 14 (0x407EC), fn(). */
u32  fighter_3f7f4(u32 side);
void fighter_3f85c(u32 side);
u32  fighter_3fd30(u32 side);
void fighter_3fef8(u32 slot, u32 rec, u32 side);
void fighter_3ffdc(u32 slot, u32 rec, u32 side);
void fighter_40608(void);
void fighter_407ec(void);
void fighter_40554(void);
void fighter_215b0(u32 side);
u32  fighter_21a88(u32 side);
void fighter_21b08(u32 side);
u32  fighter_21b74(u32 side);
void fighter_21be8(u32 side);
void fighter_21c84(u32 slot, u32 rec, u32 side);
void fighter_21d10(u32 slot, u32 rec, u32 side);
/* Record §54-A: the animation-opcode targets of those pose streams (EAX =
 * rec): 0x3FF90/0x3FEA8/0x40434 (0xD000, opcode 0x10), 0x3FC08/0x3FCB0
 * (0xD100, opcode 0x11), 0x40034/0x3F77C (0xD500, opcode 0x15); and 0x21694
 * (EAX = side, no reference), the twin of 0x3E8E4. */
void fighter_3ff90(u32 rec);
void fighter_3fea8(u32 rec);
void fighter_40034(u32 rec);
void fighter_3fc08(u32 rec);
void fighter_3fcb0(u32 rec);
void fighter_3f77c(u32 rec);
void fighter_40434(u32 rec);
void fighter_21694(u32 side);
/* And their callees no ported code shares: 0x3F184 (EAX = rec: 0x3F308 on
 * the record's side's slot, the 0xE7C98 landing and +0x57 = 3), 0x3F308 (EAX =
 * slot: the 0xBB308 spawn at the slot's record), 0x3C404 (EAX = side, EDX =
 * n: the signed 64ths between the other slot's +0x2C and this one's + n * 64,
 * facing-signed) and 0x3605C (EAX = side, one stack word: state 9/4/0, the
 * anchor, 0x3C148/0x3C16C and the 0xC8B58[char] stream at those bits). */
void fighter_3f184(u32 rec);
void fighter_3f308(u32 slot);

/* 0x468D8 (record §49-V). See fighter.c. */
int ai_pred_468d8(u32 side);

/* The 0x469A8 predicates 0x467DC and 0x4682C (record §K1.4 of
 * 2026-09-29-k1-k9-derivations.md). EAX = side: 1 when the slot's
 * +0x54 == 0, +0x53 == 0 and +0x52 == 1 and the 0x1A5D4 facing/command test
 * is zero (0x467DC) or non-zero (0x4682C), else 0. Exposed for their unit
 * test. */
int ai_pred_467dc(u32 side);
int ai_pred_4682c(u32 side);

/* 0x21458 (record §49-V). The slot +0x10 callback 0x21994 arms on the opposite
 * side's slot: a three-state (+0x58) knockdown-recovery step, (slot, rec, side)
 * as the reaction callbacks. Registered in actors_init. */
void fighter_21458(u32 slot, u32 rec, u32 side);
/* PORT: fighter_state_3531c's case-10 call passes (slot, side); this adapter is
 * what actors_init registers under 0x21458 (rec = DSD(slot), as at 0x35396). */
void fighter_21458_case10(u32 slot, u32 side);
/* 0x216EC (record §49-V). The slot +0x0C callback 0x21994 arms: the six-state
 * (+0x57) step, (slot, rec, side) as 0x3531C case 7 calls it. Registered in
 * actors_init. */
void fighter_216ec(u32 slot, u32 rec, u32 side);
/* 0x21994 (record §49-V). Arms the pair (the 0x216EC/0x21458 callbacks on the
 * slot and the other side's slot); returns 1. Its callers 0x21B43/0x21C33 are
 * in 0x21B08/0x21BE8 (record §52-A). */
u32 fighter_21994(u32 slot, u32 rec);
s32  fighter_3c404(u32 side, s32 n);
void fighter_3605c(u32 side, u32 frame_bits);

/* Record §48-C, exported for 0x27254 (flow.c). 0x33ACC: copy slot[side]'s 0x94
 * bytes to `dst` and its record's 0x68 bytes to `dst2` (EAX = side, EDX =
 * dst, EBX = dst2; ECX/ESI/EDI pushed and popped, EAX and EDX clobbered).
 * 0x3C16C: zero the side's record's +0x36/+0x44 words. 0x3C148: zero its
 * +0x34 word and +0x43/+0x42 bytes (both push and pop EDX). */
void fighter_33acc(u32 side, u32 dst, u32 dst2);
void fighter_3c16c(u32 side);
void fighter_3c148(u32 side);

/* 0x354F0 (record §49-A). The arena-wall clamp; fight_hud_pass (0x35658)
 * calls it side then 1-side (0x3581C/0x35824). Clamps the side's x
 * (slot+0x2C) to +/-DS_000BE018 through hit_anchor_x, drags the other side
 * by the same overshoot through fighter_1883c when the two are close and
 * the clamped side is mid-hitstun (slot+0x53==0xA) while the other is not
 * blocking (slot+0x54!=2), and zeroes the clamped side's motion via
 * fighter_3c148 when it is still driving into the wall. Skipped entirely
 * when slot+0x40 bit 0x40 is set. */
void fighter_wall_clamp(u32 side);

/* Record §48-K, the fight frame's round end. 0x39FF4: both fighters frozen
 * (the +0x4B child dropped or released, the slot's timers cleared, the stun
 * timer +0x8C ended through 0x34038, 0x1922C and a 0x39F40 pose on the
 * distance to the camera centre). 0x34038: the stun end on DS_001078FF's
 * side when `side`'s +0x8C word is 1. 0x38154: the mode-0x25 approach step
 * of `side` (DS_001078F0[side]). 0x19820: the two 0x100C20/0x100C28 lists,
 * DS_00100CA8/CA9 and DS_00104AE8 |= 4. */
void fighter_39ff4(void);
void fighter_34038(u32 side);
void fighter_38154(u32 side);
/* 0x384F8 — record §49-P. Mode 0x25's per-side combo/approach pass;
 * game_mode_25_step (0x266AC) calls it for each side. See fighter.c for the
 * full derivation. */
void fight_384f8(u32 side);
void fighter_19820(void);

/* 0x392A0. The winner-pose driver (see its header comment in fighter.c);
 * exported for 0x28C38 (record §48-B). EAX = slot, EDX = v, EBX = w. */
void fighter_392a0(u32 slot, s32 v, s32 w);

/* Record §49-Z. 0x39FB0: the pose 0x39F40 (-0x50, 0x64, 0xF, 0x14) on the
 * slot record's side after 0x18B04. EAX = slot. */
void fighter_39fb0(u32 slot);
/* 0x45B50: the slot +0x0C callback 0x45C10 stores (slot, rec, side). */
void fighter_45b50(u32 slot, u32 rec, u32 side);
/* 0x47A00: the throw setup 0x47B04's case 0 runs. EAX = side. */
void fighter_47a00(u32 side);
/* 0x47B04: the slot +0x0C callback 0x47BFC stores (slot, rec, side). */
void fighter_47b04(u32 slot, u32 rec, u32 side);
/* Record gameplay-u0 §U0.8. Character 2's reaction-0x26 callback 0x47BFC
 * (slot, rec, side): with the side's DS_00107D2C word >= 1, the slot 9/7/0,
 * +0x57 = 5 and the callbacks +0x0C 0x47B04, +0x18 0x478D4 (fn(side), EAX
 * returned: two 0x18C14 passes) and +0x1C 0x47984 (fn(side)). */
void fighter_47bfc(u32 slot, u32 rec, u32 side);
/* 0x23208 (record gameplay-u6 §U6.2). Character 1's reaction-0x26 callback
 * (slot, rec, side): the record on 0xE4900 at 3.0, the slot 9/7/0 with +0x0C =
 * 0, the voice 0x79. */
void fighter_23208(u32 slot, u32 rec, u32 side);
u32  fighter_478d4(u32 side);
void fighter_47984(u32 side);

/* Record §K8c: the update-table entries 4 (0x37C8C), 8 (0x34648), 9
 * (0x3800C), 11 (0x4F890), 12 (0x24150) and 17 (0x45D98), each fn() as
 * 0x24CEF calls it, registered in actors_init; and 0x29C20, entry 8's handle
 * pick (EAX = char, DL = flag). */
void fighter_37c8c(void);
void fighter_34648(void);
u32  fighter_29c20(u32 ch, u32 flag);
void fighter_3800c(void);
void fighter_4f890(void);
void fighter_24150(void);
void fighter_45d98(void);

/* Record §D8 (2026-09-29-e-wire-k8b-k8d-derivations.md): the animation-opcode
 * targets that arm entries 9, 12 and 17, each the dword after a 0xD100 word
 * in a character stream; EAX = the record (0x45D58 reads none). actors.c
 * registers their (rec, arg) wrappers. */
void fighter_37ea0(u32 rec);
void fighter_24078(u32 rec);
void fighter_45d58(void);
/* Record §C: 0x4F944, entry 11's arming: the count (clamped to 0x14 by a
 * signed compare) into DS_001088F0, the countdown 0, the reload word 0x10,
 * DS_00104AE9 |= 8. Callers 0x1467F, 0x149EB, 0x39195. */
void fighter_4f944(u32 v);

/* Track P batch 1 (record 2026-10-02-reverse-p1-derivations.md §P1.5): the
 * finisher entries 0x379C4 calls through DS_001078E8 as (slot, rec), testing
 * the whole EAX; registered in actors_init. */
int  fighter_1567c(u32 slot, u32 rec);
int  fighter_15908(u32 slot, u32 rec);
int  fighter_23ec0(u32 slot, u32 rec);
int  fighter_45d14(u32 slot, u32 rec);
u32  fighter_23bf8(u32 slot, u32 rec);
int  fighter_402fc(u32 slot, u32 rec);
/* The slot +0x0C callbacks the finisher entries store (record §P1.8/§P1.9),
 * as 0x3531C case 7 calls them (slot, rec, side); registered in actors_init. */
void fighter_15584(u32 slot, u32 rec, u32 side);
void fighter_1579c(u32 slot, u32 rec, u32 side);
void fighter_23d38(u32 slot, u32 rec, u32 side);
void fighter_23b68(u32 slot, u32 rec, u32 side);
void fighter_401d4(u32 slot, u32 rec, u32 side);
/* 0x38034 (record §P1.9): the call of 0x401D4's case 3, EAX = a side. */
void fighter_38034(u32 side);
/* The 0xD100 stream targets the finisher streams reach (record §P1.12), as
 * the animation dispatcher's opcode 0x11 calls them (EAX = rec, EDX = the
 * operand word); registered in actors_init through anim_code wrappers. */
void fighter_156d4(u32 rec);
void fighter_23ca4(u32 rec);
void fighter_23868(u32 rec, u32 arg);
void fighter_3f174(u32 rec);
/* Track P batch 2 (record 2026-10-02-reverse-p2-derivations.md §P2.3): the
 * guard-shaped move callbacks 0x34E2C calls as (slot, rec, side), and the
 * +0x0C callback 0x22A00 stores (0x3531C case 7, the same registers);
 * registered in actors_init. */
void fighter_237d0(u32 slot, u32 rec, u32 side);
void fighter_2381c(u32 slot, u32 rec, u32 side);
void fighter_3dadc(u32 slot, u32 rec, u32 side);
void fighter_3db34(u32 slot, u32 rec, u32 side);
void fighter_3d10c(u32 slot, u32 rec, u32 side);
void fighter_22a00(u32 slot, u32 rec, u32 side);
void fighter_229fc(u32 slot, u32 rec, u32 side);
/* §P2.4: character 3's reactions 0x20 and 0x21, and the 0xD000 targets of the
 * streams they start (EAX = rec; registered through anim_code wrappers). */
void fighter_14ef8(u32 slot, u32 rec, u32 side);
void fighter_14f50(u32 slot, u32 rec, u32 side);
void fighter_14fa8(u32 rec);
void fighter_14ff8(u32 rec);
void fighter_150ac(u32 rec);
/* §P2.5: the unconditional move callbacks (slot, rec, side). */
void fighter_15478(u32 slot, u32 rec, u32 side);
void fighter_3dcec(u32 slot, u32 rec, u32 side);
void fighter_21114(u32 slot, u32 rec, u32 side);
/* §P2.6: the move callbacks that arm the side's own slot (slot, rec, side). */
void fighter_21374(u32 slot, u32 rec, u32 side);
void fighter_22938(u32 slot, u32 rec, u32 side);
/* §P2.7: the slot +0x18 hooks they store, fn(side) as 0x19020 calls them. */
u32  fighter_2116c(u32 side);
u32  fighter_22510(u32 side);
/* §P2.8: the slot +0x1C callbacks they store, fn(side) as 0x193B0 calls them,
 * and 0x22588's callee 0x22404 (side); and three callees they stub, no
 * longer file-local so the harness's mutants can call them: 0x3C480, 0x18AF8
 * and 0x39834. */
void hit_anim_start_a(u32 rec, u32 stream, u32 frame_bits);
void fighter_18af8(void);
void fighter_39834(u32 side, s32 b);
void fighter_22404(u32 side);
void fighter_211f0(u32 side);
void fighter_22588(u32 side);
/* §P2.9: the slot +0x0C callbacks they store (slot, rec, side). */
void fighter_212cc(u32 slot, u32 rec, u32 side);
void fighter_22638(u32 slot, u32 rec, u32 side);
/* Track P batch 3 (record 2026-10-03-reverse-p3-derivations.md §P3.3): the
 * move callbacks 0x34E2C calls as (slot, rec, side), registered in
 * actors_init; and the callee 0x35838 they stub (no longer file-local, so
 * the harness's mutants can call it). */
void fighter_state_35838(u32 slot, u32 rec, u32 dirbits);
void fighter_475ec(u32 slot, u32 rec, u32 side);
void fighter_47608(u32 slot, u32 rec, u32 side);
void fighter_47624(u32 slot, u32 rec, u32 side);
void fighter_48964(u32 slot, u32 rec, u32 side);
void fighter_489a0(u32 slot, u32 rec, u32 side);
/* §P3.4: 0x47720 and its +0x0C callback (slot, rec, side), +0x18 hook
 * fn(side) and +0x1C callback fn(side); and the callee 0x3B298 (no longer
 * file-local, so the harness's mutants can call it). */
int fighter_command_dispatch(u32 side, u32 edx_arg);
void fighter_47720(u32 slot, u32 rec, u32 side);
void fighter_476fc(u32 slot, u32 rec, u32 side);
u32  fighter_47648(u32 side);
void fighter_47688(u32 side);
/* §P3.5: 0x47874 and its +0x0C callback (slot, rec, side), +0x14 callback
 * fn(slot), +0x18 hook fn(side) and +0x1C callback fn(side); and the callee
 * 0x3C190 (no longer file-local, so the harness's mutants can call it). */
void fighter_3c190(u32 side, u32 v);
void fighter_47874(u32 slot, u32 rec, u32 side);
void fighter_47830(u32 slot, u32 rec, u32 side);
u32  fighter_47798(u32 slot);
u32  fighter_477a8(u32 side);
void fighter_477e8(u32 side);
/* §P3.6: 0x47FCC and its +0x0C callback (slot, rec, side), +0x18 hook
 * fn(side) and +0x1C callback fn(side). */
void fighter_47fcc(u32 slot, u32 rec, u32 side);
u32  fighter_47cb0(u32 side);
void fighter_47d24(u32 side);
void fighter_47e9c(u32 slot, u32 rec, u32 side);
/* §P3.7: 0x48608 (slot, rec, side), its +0x18 hook fn(side) and +0x1C
 * callback fn(side), the +0x1C callback's callee 0x48170(side), and the
 * +0x10 handler 0x4811C (slot, rec, side) with its case-10 adapter; and the
 * callee 0x36D98 (no longer file-local, so the harness's mutants can call
 * it). */
void fighter_36d98(u32 slot);
void fighter_48608(u32 slot, u32 rec, u32 side);
u32  fighter_48054(u32 side);
void fighter_48170(u32 side);
void fighter_480b4(u32 side);
void fighter_4811c(u32 slot, u32 rec, u32 side);
void fighter_4811c_case10(u32 slot, u32 side);
/* §P3.8: 0x48608's +0x0C callback (slot, rec, side). */
void fighter_4844c(u32 slot, u32 rec, u32 side);

#endif /* PRAGE_GAME_FIGHTER_H */
