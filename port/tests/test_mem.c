#include "mem.h"
#include "symbols.h"
#include "test.h"

void fn_probe(void) { }
void fn_probe2(void) { }

int test_mem(void)
{
    int before = g_failures;

    CHECK_EQ_INT(MEM_SIZE, 0x4000000);
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

    /* Code addresses stored in data (process tables, the lock calls in main)
     * must survive a round trip through the flat address space. */
    {
        extern void fn_probe(void);
        extern void fn_probe2(void);
        CHECK(fn_resolve(FN_0002D62C) == NULL, "unregistered address resolves to NULL");
        fn_register(FN_0002D62C, fn_probe);
        CHECK(fn_resolve(FN_0002D62C) == fn_probe, "round trip");
        CHECK_EQ_INT(fn_origin(fn_probe), FN_0002D62C);
        CHECK(fn_resolve(0xDEAD) == NULL, "unknown address is NULL, not garbage");

        /* An unregistered function has origin 0, the sentinel callers test. */
        CHECK_EQ_INT(fn_origin(fn_probe2), 0);

        /* Same address registered twice: the first entry wins and the later
         * registration is ignored, not silently substituted. */
        fn_register(FN_000255CC, fn_probe);
        fn_register(FN_000255CC, fn_probe2);
        CHECK(fn_resolve(FN_000255CC) == fn_probe, "duplicate address keeps first registration");
    }

    return g_failures - before;
}
