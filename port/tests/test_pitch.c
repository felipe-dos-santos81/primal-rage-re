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

    return g_failures - before;
}
