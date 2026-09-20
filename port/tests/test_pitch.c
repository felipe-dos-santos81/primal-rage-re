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

    /* Folded low indices 0-6 with a centred wheel compute block -1 (one below
     * the lowest legal block), so the `block < 0` carry runs (block++ and
     * v >>= 1): index 0's v 0x02B2 halves to 0x0159, index 6's 0x03CF to
     * 0x01E7, both lifted to block 0. */
    {
        u8 a0, b0;
        pitch_lookup(0, 0, &a0, &b0);
        CHECK_EQ_INT(a0, 0x59);
        CHECK_EQ_INT(b0, 0x01);
        pitch_lookup(6, 0, &a0, &b0);
        CHECK_EQ_INT(a0, 0xE7);
        CHECK_EQ_INT(b0, 0x01);
    }
    /* A negative bend drives `ax` below 0; the 0xc0 fold wraps it (it is not
     * a clamp), so the index lands above centre. Index 0, bend -1000: ax -62
     * -> 130. */
    {
        u8 a0, b0;
        pitch_lookup(0, -1000, &a0, &b0);
        CHECK_EQ_INT(a0, 0x27);
        CHECK_EQ_INT(b0, 0x02);
    }

    return g_failures - before;
}
