#include "platform/gfx.h"
#include "mem.h"
#include "test.h"

/* Scratch colour buffer. The plan sketched 0xE00000, but that address lies
 * inside the resource heap (0x10B0D0 .. ~0x2A8C548) once res_load_index() has
 * run, so the flush test would scribble on loaded resources. Use 0x3000000,
 * the same clear-of-heap scratch the GRA test uses. */
#define SCRATCH 0x3000000u

int test_gfx(void)
{
    int before = g_failures;

    /* A palette record with one colour, written directly into the dirty list. */
    DSD(0x107498 + 0) = SCRATCH;        /* colour pointer (scratch) */
    DSD(0x107498 + 4) = 5;              /* first DAC index */
    DSD(0x107498 + 8) = 1;              /* count */
    DSD(0x107498 + 12) = 0;             /* not a resource handle */
    DSD(0x107798) = 0x107498 + 16;      /* head pointer marks the end */

    /* R=0x40 -> bits2..9, G=0x80 -> bits10..17, B=0xC0 -> bits18..25 */
    u32 word = (0x40u << 2) | (0x80u << 10) | (0xC0u << 18);
    DSD(SCRATCH) = word;
    gfx_flush_palette();

    CHECK_EQ_INT(gfx_dac[5][0], 0x40);
    CHECK_EQ_INT(gfx_dac[5][1], 0x80);
    CHECK_EQ_INT(gfx_dac[5][2], 0xC0);
    CHECK(gfx_dac[4][0] == 0, "only the addressed DAC entry was written");
    CHECK_EQ_INT(DSD(0x107798), 0x107498);   /* head reset to base */
    return g_failures - before;
}
