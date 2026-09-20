#include "platform/audio/pitch.h"
#include "test.h"

int test_pitch(void)
{
    int before = g_failures;

    /* Capture-pinned anchors from the driver routine. b0 is the payload
     * without the key-on bit; a key-on ORs 0x20. Note 84 (melodic) and 79
     * (the negative-fnum carry) land on block 5; percussion base 54 on
     * block 2. */
    {
        u8 a0, b0;
        pitch_lookup(84, 0, &a0, &b0);
        CHECK_EQ_INT(a0, 0xB2);
        CHECK_EQ_INT(b0, 0x16);
        pitch_lookup(79, 0, &a0, &b0);
        CHECK_EQ_INT(a0, 0x05);
        CHECK_EQ_INT(b0, 0x16);
        pitch_lookup(54, 0, &a0, &b0);
        CHECK_EQ_INT(a0, 0xCF);
        CHECK_EQ_INT(b0, 0x0B);
    }

    /* A centred wheel (0x2000) is zero at any scale. */
    CHECK_EQ_INT(pitch_bend_of(0x2000, 12), 0);
    /* A full-up wheel with scale 1 is (0x1fff >> 5) * 1 = 0xff. */
    CHECK_EQ_INT(pitch_bend_of(0x3fff, 1), 0xFF);
    /* The product is 16-bit: the driver's imul result is used from `ax` only,
     * so 255 * 255 wraps to the signed 16-bit value. */
    CHECK_EQ_INT(pitch_bend_of(0x3fff, 255), -511);
    /* The 0x3646/0x3648 additions are 16-bit too, so the sum wraps before the
     * 0x364e shift; 127 folds to bx 91, and 91*256 + 8 + 16000 overflows. */
    {
        u8 a0, b0;
        pitch_lookup(127, 16000, &a0, &b0);
        CHECK_EQ_INT(a0, 0xDA);
        CHECK_EQ_INT(b0, 0x01);
    }

    return g_failures - before;
}
