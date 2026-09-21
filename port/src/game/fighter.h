/* port/src/game/fighter.h
 * The two per-frame fighter passes the arena frame calls between its projection
 * pairs (0x1958C, then 0x19068) plus the pure per-fighter helpers the HUD spine
 * and Task 4's think chain share. Both passes loop the two sides; neither is a
 * render. Addresses, gates, globals and the single RNG site are from
 * docs/superpowers/plans/2026-09-20-demo-fight-derivations.md §3.5 and §5.8.
 *
 * Task 4 adds the think/AI chain (0x1975C -> 0x3B464 -> 0x3B298 -> 0x3BDDC) to
 * this file. Its entry point is not declared yet: the arena frame's 0x264CC
 * call site carries a PORT skip until Task 4 lands (see fight.c). */
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

#endif /* PRAGE_GAME_FIGHTER_H */
