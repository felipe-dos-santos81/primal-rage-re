#include "mem.h"
#include "test.h"

int test_mem(void)
{
    int before = g_failures;

    CHECK_EQ_INT(MEM_SIZE, 0x1000000);
    CHECK(mem_in_range(0x80000, 0x8B0D0), "data object range is inside mem[]");
    CHECK(!mem_in_range(MEM_SIZE - 4, 8), "writes past the end are rejected");

    /* DSB offsets include the 0x80000 base: DAT_00080004 is DS:0x0004. */
    DSB(0x80004) = 0xAB;
    CHECK_EQ_INT(mem[0x80004], 0xAB);
    CHECK_EQ_INT(DSB(0x80004), 0xAB);

    DSD(0x80010) = 0x11223344u;
    CHECK_EQ_INT(DSD(0x80010), 0x11223344u);

    mem_fill(0x90000, 0x5A, 4);
    CHECK_EQ_INT(DSB(0x90000), 0x5A);
    CHECK_EQ_INT(DSB(0x90003), 0x5A);
    CHECK_EQ_INT(DSB(0x90004), 0x00);

    /* The macros must be usable as lvalues, not just rvalues. */
    DSW(0x90008) = 0x1234;
    CHECK_EQ_INT(DSW(0x90008), 0x1234);

    return g_failures - before;
}
