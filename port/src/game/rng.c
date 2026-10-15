#include "game/rng.h"
#include "mem.h"
#include "symbols.h"

void rng_seed(u32 s) { DSD(DS_000EF6D8) = s; }

u32 rng_next(u32 range)
{
    PR_SEAM_RET(0x5D7DCu, range);
    DSD(DS_000EF6D8) = DSD(DS_000EF6D8) * 0xB90D12B9u + 0x38CE051Fu;
    return (DSD(DS_000EF6D8) >> 16) * (range & 0xFFFFu) >> 16;
}

void rng_step(void) { (void)rng_next(0); }

/* The master loop 0x255CC's two draws (0x256AF/0x256B1 and 0x256D4/0x256D6: `mov eax,ebp` with
 * EBP = 0x7FFF at 0x255E4, then `call 0x5D7DC`). The pinned original (tools/title_pin.py,
 * PATCHES 0x78505/0x7852A) replaces both calls by a non-advancing `mov eax,0`, and the port's
 * real stream follows it: a real draw here would advance the LCG per iteration and per spin
 * pass, shifting every later consumption-site draw against the captures. The differential row
 * `game_loop` runs the raw's calls as recorded stubs on 0x5D7DC's own row, so the port models
 * them through the seam alone — build/diffrun records the call (EAX = `range`, as the raw's
 * `mov eax,ebp`) and stubs it; every other binary does nothing (PR_SEAM is inert outside
 * build/diffrun). The raw discards both returns (0x256BB/0x256C6 `mov eax,[0x10150C]`), so no
 * caller reads the stubbed value. */
void rng_pinned_draw(u32 range)
{
    PR_SEAM(0x5D7DCu, range);
}
