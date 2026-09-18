/* port/src/game/rng.h */
#ifndef PRAGE_GAME_RNG_H
#define PRAGE_GAME_RNG_H

#include "../types.h"

/* 0x5D7DC: state = state*0xB90D12B9 + 0x38CE051F, return
 * ((state >> 16) * (range & 0xffff)) >> 16. Seed 0xABCD from 0x20C10. */
void rng_seed(u32 s);
u32  rng_next(u32 range);
void rng_step(void);

#endif /* PRAGE_GAME_RNG_H */
