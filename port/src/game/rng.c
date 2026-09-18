#include "game/rng.h"
#include "mem.h"
#include "symbols.h"

void rng_seed(u32 s) { DSD(DS_000EF6D8) = s; }

u32 rng_next(u32 range)
{
    DSD(DS_000EF6D8) = DSD(DS_000EF6D8) * 0xB90D12B9u + 0x38CE051Fu;
    return (DSD(DS_000EF6D8) >> 16) * (range & 0xFFFFu) >> 16;
}

void rng_step(void) { (void)rng_next(0); }
