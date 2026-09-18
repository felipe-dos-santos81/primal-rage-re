/* port/src/game/actors.h */
#ifndef PRAGE_GAME_ACTORS_H
#define PRAGE_GAME_ACTORS_H

#include "../types.h"

#define ACTOR_REC_SIZE 0x68u
#define ACTOR_POOL_RECORDS 580u
#define PSET_SIZE 0x20u

/* Validates the two pools res_load_index already allocated. Returns 0 when
 * either base is zero or outside mem[]. */
int  actors_init(void);
void actors_reset(void);
/* Pops the free-list head and links it into the active list. `flag` is the
 * original's EAX at the 0x2AC80 call (copied to ECX by 0x2AC84); its 0x400 bit
 * selects the tail insert (0x249C0) over the head insert (0x249B0). 0x2AE14
 * supplies the low 16 bits of its arg 5. */
u32  actor_alloc(u32 flag);
/* 0x2AE14. `desc` points at a descriptor in mem[]; the four register arguments
 * are a2=EDX, a3=ECX, a4=EBX and a5=the stack word, pinned by disassembly in
 * docs/superpowers/plans/2026-09-17-actor-system-args.md. Returns the record's
 * mem[] offset, or 0 on pool exhaustion. */
u32  actor_spawn(const u32 *desc, u32 a2, u32 a3, u32 a4, u32 a5);
void actor_free(u32 rec);
u32  actor_record(u32 index);
u32  actor_index(u32 rec);
u32  actor_list_head(void);
u32  actor_next(u32 rec);
u32  actor_pset(u32 rec);
/* 0x2A31C. Walks the active list and syncs each record (0x2A1FC). */
void actors_update(void);

/* ---- animation-stream interpreter -------------------------------------- */

/* 0x2A408. Read the record's next sprite id from its animation stream. The
 * second argument is the record's pset (pinned by disassembly: every caller
 * passes the pset at pset+0 = DS_001014EC + slot*0x20, read for the
 * `word & 0x8000` keep-current-id case). Literal words (bit 0x8000 clear) are
 * returned as-is; a 0xD00 computed word reads a variable and either adds the
 * following word or uses it to index a table. The returned bit 0x8000 is the
 * stream's bit XOR the record's `rec+0x28 >> 8 & 0x40` flip. */
u32  anim_next_sprite_id(u32 rec, u32 pset);
/* 0x29F34. Read an animation variable: `op & 0x7F` selects the 0x40-word ring
 * at DS_00105B4C (< 0x40), the record's own bytes (0x40..0x45), the parent
 * rec+0x4A's bytes (0x46..0x4B) or the child rec+0x4B's bytes (0x4C..0x51). */
u32  anim_read_var(u32 rec, u8 op);
/* 0x29DB8. Write an animation variable (the mirror of anim_read_var). */
void anim_write_var(u32 rec, u8 op, u32 value);
/* 0x2BC30. Point an existing record at `stream`, reset its animation cursor and
 * cache, pre-walk the commands, then load the first sprite id. */
void actors_anim_begin(u32 rec, u32 stream, u32 frame);
/* 0x2BCF4. Point a record at `stream` and load its first sprite id. */
void actors_anim_seek(u32 rec, u32 stream);
/* TEST-ONLY. Task 1's fourth pin replaced the opcode-8 call to 0x5D7DC with
 * `mov eax, 0`; the port draws `on ? 0 : rng_next(range)` at that one call site.
 * Set by the Task 10 title driver; nothing else calls it. */
void actors_pin_anim_tick_zero(int on);

#endif /* PRAGE_GAME_ACTORS_H */
