#include <string.h>

#include "test.h"
#include "platform/audio/opl/opl.h"

static s16 buf_a[2048], buf_b[2048];

/* The driver's note setup: waveform enable, operator/channel registers with the
 * output-enable write 0xC0 = patch | 0x30, then key-on. `with_105` inserts the
 * capture's OPL3-mode enable (0x105 = 0x01) right after 0x01 = 0x20. */
static void note_setup(int with_105, s16 *out)
{
    opl_reset();
    opl_write(0x01, 0x20);
    if (with_105)
        opl_write(0x105, 0x01);
    opl_write(0x20, 0x01);
    opl_write(0x40, 0x10);
    opl_write(0x60, 0xF0);
    opl_write(0x80, 0x77);
    opl_write(0xE0, 0x00);
    opl_write(0xC0, 0x30);
    opl_write(0xA0, 0x98);
    opl_write(0xB0, 0x31);
    opl_render(out, 1024);
}

int test_opl(void)
{
    int before = g_failures;

    opl_reset();
    opl_render(buf_a, 1024);
    int silent = 1;
    for (int i = 0; i < 2048; i++)
        if (buf_a[i] != 0)
            silent = 0;
    CHECK(silent, "a reset core with no register writes renders silence");

    opl_reset();
    opl_write(0x20, 0x01);
    opl_write(0x40, 0x10);
    opl_write(0x60, 0xF0);
    opl_write(0x80, 0x77);
    opl_write(0xA0, 0x98);
    opl_write(0xB0, 0x31);
    opl_render(buf_a, 1024);
    int any = 0;
    for (int i = 0; i < 2048; i++)
        if (buf_a[i] != 0)
            any = 1;
    CHECK(any, "a key-on write sequence produces audio");

    opl_reset();
    opl_write(0x20, 0x01);
    opl_write(0x40, 0x10);
    opl_write(0x60, 0xF0);
    opl_write(0x80, 0x77);
    opl_write(0xA0, 0x98);
    opl_write(0xB0, 0x31);
    opl_render(buf_b, 1024);
    CHECK(memcmp(buf_a, buf_b, sizeof buf_a) == 0, "the core is deterministic");

    /* Regression: the spec once claimed 0x105 = 0x01 silences the core. It does
     * not, given the 0xC0 output-enable write every real note setup has; the
     * old probe omitted that write and attributed the resulting OPL3-mode
     * silence to 0x105. */
    note_setup(0, buf_a);
    note_setup(1, buf_b);
    {
        int n = 0;
        for (int i = 0; i < 2048; i++)
            if (buf_b[i] != 0)
                n++;
        CHECK(n > 0, "0x105 = 1 with 0xC0 enable still renders audio");
        CHECK(memcmp(buf_a, buf_b, sizeof buf_a) == 0,
              "0x105 = 1 is output-neutral given the 0xC0 enable");
    }

    return g_failures - before;
}
