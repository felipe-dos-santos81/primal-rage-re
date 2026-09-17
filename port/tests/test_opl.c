#include <string.h>

#include "test.h"
#include "platform/audio/opl/opl.h"

static s16 buf_a[2048], buf_b[2048];

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

    return g_failures - before;
}
