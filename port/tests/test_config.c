/* port/tests/test_config.c */
#include "test.h"

#include "game/config.h"
#include "mem.h"
#include "symbols.h"

/* The descriptor table lives in the loaded image (obj-0 VA 0x2D300). These tests
 * run after test_le() has loaded PRAGE.EXE into mem[]. */
int test_config(void)
{
    /* The table must be the real one; a missing load would otherwise read zeros
     * and silently pass. */
    CHECK_EQ_INT((int)DSD(DS_0002D300 + 0x29u * 4u), 0x1D980);

    /* Save the config region: the tests below seed and mutate it. */
    u8 saved[0x100];
    for (u32 i = 0; i < 0x100u; i++) saved[i] = DSB(DS_00105D88 + i);

    /* Field 0x29 has bitpos 102, width 8, no trailing byte: the getter reads
     * DS_00105DE1 indices 54,53,52,51, giving b54<<24 | b53<<16 | b52<<8 | b51. */
    for (u32 i = 0; i < 0x70u; i++)
        DSB(DS_00105DE1 + i) = (u8)i;
    CHECK_EQ_INT((int)config_field_get(0x29u), 0x36353433);

    /* Field 0x2A has bitpos 110, width 1: the low nibble of byte 55. */
    CHECK_EQ_INT((int)config_field_get(0x2Au), 55u & 0xFu);

    /* Field 0x35 has bitpos 131, width 4 (odd start). With a ramp, the value is
     * ((67 & 0xf) << 8 | 66) << 4 | (65 >> 4) = 0x3424. */
    CHECK_EQ_INT((int)config_field_get(0x35u), 0x3424);

    /* Field 0x00 has bitpos 0, width 2 and trailing index 1: the value is byte 0
     * (the even-start walk reads it whole) shifted up 8 and OR'd with
     * DS_00105DAF + 1. */
    DSB(DS_00105DAF + 1u) = 0xA1u;
    CHECK_EQ_INT((int)config_field_get(0x00u), 0xA1);

    /* Out-of-range fields return -1. */
    CHECK_EQ_INT((int)config_field_get(0x3Fu), -1);
    CHECK_EQ_INT((int)config_field_get(0x100u), -1);

    for (u32 i = 0; i < 0x100u; i++) DSB(DS_00105D88 + i) = saved[i];

    {
        /* set -> get round-trips for even/odd starts, width 8 / width 4 / width 1
         * and the trailing-byte case. These are the fields the consumers use plus
         * the boundary shapes of the walk. */
        static const u32 saved_lo = 0x00105DAFu, saved_hi = 0x00105DE1u;
        u8 lo[0x20], hi[0x70];
        for (u32 i = 0; i < 0x20u; i++) lo[i] = DSB(saved_lo + i);
        for (u32 i = 0; i < 0x70u; i++) hi[i] = DSB(saved_hi + i);

        config_field_set(0x29u, 0x0000ABCDu);
        CHECK_EQ_INT((int)config_field_get(0x29u), 0xABCD);

        config_field_set(0x35u, 0x0000000Au);
        CHECK_EQ_INT((int)config_field_get(0x35u), 0x000A);

        config_field_set(0x00u, 0x0000080Fu);
        CHECK_EQ_INT((int)config_field_get(0x00u), 0x80F);

        config_field_set(0x2Au, 0x00000003u);
        CHECK_EQ_INT((int)config_field_get(0x2Au), 3);

        /* The setter raises DS_00105DD8: |1 for a trailing byte, |6 always. */
        DSB(DS_00105DD8) = 0;
        config_field_set(0x00u, 0x10u);
        CHECK_EQ_INT((int)DSB(DS_00105DD8), 0x7);
        DSB(DS_00105DD8) = 0;
        config_field_set(0x29u, 0u);
        CHECK_EQ_INT((int)DSB(DS_00105DD8), 0x6);

        CHECK_EQ_INT((int)config_field_set(0x3Fu, 0u), -1);
        CHECK_EQ_INT((int)config_field_set(0x100u, 0u), -1);

        for (u32 i = 0; i < 0x20u; i++) DSB(saved_lo + i) = lo[i];
        for (u32 i = 0; i < 0x70u; i++) DSB(saved_hi + i) = hi[i];
    }
    return 0;
}
