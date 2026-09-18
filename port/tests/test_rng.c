/* port/tests/test_rng.c */
#include "test.h"
#include "game/rng.h"
#include "mem.h"
#include "symbols.h"

int test_rng(void)
{
    /* The recurrence's own outputs, computed independently from
     * state = state*0xB90D12B9 + 0x38CE051F seeded 0xABCD. */
    static const u32 ranges[] = { 0x5Au, 0x7Eu, 2u, 0xFFFFu, 0x10000u, 0u, 0x7FFFFFFFu };
    static const u32 expect[] = { 12u, 111u, 0u, 29617u, 0u, 0u, 11010u };

    rng_seed(0xABCDu);
    CHECK_EQ_INT((int)DSD(DS_000EF6D8), 0xABCD);
    for (unsigned i = 0; i < sizeof ranges / sizeof ranges[0]; i++)
        CHECK_EQ_INT((int)rng_next(ranges[i]), (int)expect[i]);

    /* A range wider than 16 bits is masked, and 0 must not shift undefinedly. */
    rng_seed(0xABCDu);
    CHECK_EQ_INT((int)rng_next(0x7FFFFFFFu), 9158);
    CHECK_EQ_INT((int)rng_next(0u), 0);

    /* rng_step advances the state and discards the value. */
    rng_seed(0xABCDu);
    u32 before = DSD(DS_000EF6D8);
    rng_step();
    CHECK(DSD(DS_000EF6D8) != before, "rng_step advanced the state");
    CHECK_EQ_INT((int)DSD(DS_000EF6D8), (int)(0xABCDu * 0xB90D12B9u + 0x38CE051Fu));

    /* The three title draws, in order, are what Task 1 pins the original to:
     * the immediates 12, 111, 0 in tools/title_pin.py must equal these. */
    rng_seed(0xABCDu);
    CHECK_EQ_INT((int)rng_next(0x5Au), 12);
    CHECK_EQ_INT((int)rng_next(0x7Eu), 111);
    CHECK_EQ_INT((int)rng_next(2u), 0);
    return 0;
}
