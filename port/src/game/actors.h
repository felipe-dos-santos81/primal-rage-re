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

#endif /* PRAGE_GAME_ACTORS_H */
