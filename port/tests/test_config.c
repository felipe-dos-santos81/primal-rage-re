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
        /* set -> get round-trips for even/odd starts, widths 8 / 4 / 3 / 1 / 2 and
         * the trailing-byte case. Odd widths (3, and the synthesized 5 and 7) are
         * the shape where the getter's cursor-parity seed ((bitpos+width)&1) and the
         * setter's bitpos-parity branch disagree. */
        static const u32 saved_lo = 0x00105DAFu, saved_hi = 0x00105DE1u;
        u8 lo[0x20], hi[0x70];
        for (u32 i = 0; i < 0x20u; i++) lo[i] = DSB(saved_lo + i);
        for (u32 i = 0; i < 0x70u; i++) hi[i] = DSB(saved_hi + i);
        u8 saved_flags = DSB(DS_00105DD8);

        config_field_set(0x29u, 0x0000ABCDu);
        CHECK_EQ_INT((int)config_field_get(0x29u), 0xABCD);

        config_field_set(0x35u, 0x0000000Au);
        CHECK_EQ_INT((int)config_field_get(0x35u), 0x000A);

        config_field_set(0x00u, 0x0000080Fu);
        CHECK_EQ_INT((int)config_field_get(0x00u), 0x80F);

        config_field_set(0x2Au, 0x00000003u);
        CHECK_EQ_INT((int)config_field_get(0x2Au), 3);

        /* Width 3, both parities: field 0x03 has bitpos 6 (getter odd-seed, setter
         * even) and field 0x04 has bitpos 9 (getter even, setter odd). 11 bits. */
        config_field_set(0x03u, 0x000005A3u);
        CHECK_EQ_INT((int)config_field_get(0x03u), 0x5A3);
        config_field_set(0x04u, 0x000006B7u);
        CHECK_EQ_INT((int)config_field_get(0x04u), 0x6B7);

        /* The setter's odd branch ORs the field's low nibble into the byte at
         * DS_00105DE1 + (bitpos>>1) and must keep that byte's low nibble. Field
         * 0x35 has bitpos 131, so it packs into DS_00105DE1 + 65. */
        DSB(DS_00105DE1 + 65u) = 0x0Bu;
        config_field_set(0x35u, 0x0000000Au);
        CHECK_EQ_INT((int)(DSB(DS_00105DE1 + 65u) & 0x0Fu), 0x0B);

        /* Widths 5 and 7 do not occur in the shipped table. Synthesize descriptors
         * (the descriptor is just data; the same walk arithmetic runs) in unused
         * slots 0x3C/0x3D and restore the slots after. */
        u8 desc_saved[8];
        for (u32 i = 0; i < 8u; i++) desc_saved[i] = DSB(DS_0002D300 + 0xF0u + i);
        DSD(DS_0002D300 + 0x3Cu * 4u) = 0x00010500u;   /* width 5, bitpos 20 */
        DSD(DS_0002D300 + 0x3Du * 4u) = 0x00018A00u;   /* width 7, bitpos 40 */
        config_field_set(0x3Cu, 0x0000001Bu);
        CHECK_EQ_INT((int)config_field_get(0x3Cu), 0x1B);
        config_field_set(0x3Du, 0x0000006Du);
        CHECK_EQ_INT((int)config_field_get(0x3Du), 0x6D);
        for (u32 i = 0; i < 8u; i++) DSB(DS_0002D300 + 0xF0u + i) = desc_saved[i];

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
        DSB(DS_00105DD8) = saved_flags;
    }

    {
        /* A zeroed config region fails the magic and the defaults path runs:
         * the magic is rewritten, DS_00105DA7 is set, and the four fields the
         * defaults writer touches carry the values from the obj-1 default table. */
        u8 saved[0x100];
        for (u32 i = 0; i < 0x100u; i++) saved[i] = DSB(DS_00105D88 + i);
        for (u32 i = 0; i < 0x100u; i++) DSB(DS_00105D88 + i) = 0;

        config_validate();
        CHECK_EQ_INT((int)DSD(DS_00105E30), (int)0x9C94D2C4u);
        CHECK_EQ_INT((int)DSB(DS_00105DA7), 1);

        /* The mismatch path raises the flag byte with |6 then |1; the setter may
         * add |1 again for a trailing byte, so the byte is 0x7 either way. */
        CHECK_EQ_INT((int)DSB(DS_00105DD8), 0x7);

        /* Field 0x35 and 0x37 default to 0xA0; field 0x2A keeps the top six bits
         * and gets 3 in the low two. */
        CHECK_EQ_INT((int)config_field_get(0x35u), 0xA0);
        CHECK_EQ_INT((int)config_field_get(0x37u), 0xA0);
        CHECK_EQ_INT((int)(config_field_get(0x2Au) & 0x3u), 3);

        /* Field 0x29's default is the menu table's '*' selection, walked from the
         * obj-1 table at 0xA2EB4. The shipped table must be nonzero or the
         * equality below is vacuous. */
        CHECK(config_menu_default_bits(0xA2EB4u) != 0u,
              "shipped menu table gives a nonzero default");
        CHECK_EQ_INT((int)config_field_get(0x29u),
                     (int)config_menu_default_bits(0xA2EB4u));

        for (u32 i = 0; i < 0x100u; i++) DSB(DS_00105D88 + i) = saved[i];
    }

    {
        /* The credit layer: free play, the credit counter and the suppression
         * flag DS_00104B1F. */
        u32 credits = DSD(DS_00105C00);
        u8 free_play = DSB(DS_00105D60);
        u8 suppress = DSB(DS_00104B1F);

        DSB(DS_00105D60) = 0;
        DSB(DS_00104B1F) = 0;
        DSD(DS_00105C00) = 5u;

        CHECK_EQ_INT((int)config_not_free_play(), 1);
        CHECK_EQ_INT((int)config_has_credit(), 1);
        CHECK_EQ_INT((int)config_credit_ready(), 1);

        /* 0x2CA48: takes one credit and reports success. */
        CHECK_EQ_INT((int)config_credit_take(), 1);
        CHECK_EQ_INT((int)DSD(DS_00105C00), 4);

        /* 0x2CA7C: the n > credits guard leaves the counter alone. */
        CHECK_EQ_INT((int)config_credit_spend(5u), 0);
        CHECK_EQ_INT((int)DSD(DS_00105C00), 4);
        CHECK_EQ_INT((int)config_credit_spend(4u), 1);
        CHECK_EQ_INT((int)DSD(DS_00105C00), 0);

        /* No credits and no free play: both predicates and the take fail. */
        CHECK_EQ_INT((int)config_has_credit(), 0);
        CHECK_EQ_INT((int)config_credit_take(), 0);
        CHECK_EQ_INT((int)config_credit_spend(1u), 0);

        /* DS_00105D60 (free play) short-circuits to success, counter untouched. */
        DSB(DS_00105D60) = 1;
        CHECK_EQ_INT((int)config_not_free_play(), 0);
        DSD(DS_00105C00) = 2u;
        CHECK_EQ_INT((int)config_credit_take(), 1);
        CHECK_EQ_INT((int)DSD(DS_00105C00), 2);
        CHECK_EQ_INT((int)config_credit_spend(9u), 1);
        CHECK_EQ_INT((int)DSD(DS_00105C00), 2);
        DSB(DS_00105D60) = 0;

        /* DS_00104B1F suppresses the debit while still reporting success. */
        DSD(DS_00105C00) = 3u;
        DSB(DS_00104B1F) = 1;
        CHECK_EQ_INT((int)config_credit_take(), 1);
        CHECK_EQ_INT((int)DSD(DS_00105C00), 3);
        CHECK_EQ_INT((int)config_credit_spend(1u), 1);
        CHECK_EQ_INT((int)DSD(DS_00105C00), 3);
        DSB(DS_00104B1F) = 0;

        /* 0x2C06C/0x2BF00: the overlay row writers. */
        config_set_credit_row(7u);
        CHECK_EQ_INT((int)DSB(DS_00105C05), 7);
        config_set_credit_row_init();
        CHECK_EQ_INT((int)DSB(DS_00105C05), 0x1D);

        DSD(DS_00105C00) = credits;
        DSB(DS_00105D60) = free_play;
        DSB(DS_00104B1F) = suppress;
    }
    return 0;
}
