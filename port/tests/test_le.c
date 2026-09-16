#include "mem.h"
#include "test.h"
#include <string.h>

#define EXE "data/game/C/PRAGE.EXE"

int test_le(void)
{
    int before = g_failures;

    CHECK(mem_load_le(EXE, NULL) == 1, "PRAGE.EXE loads");
    CHECK(mem_load_le("data/game/C/INDEX", NULL) == 0, "a non-LE file is rejected");

    /* The LE header and object table are already verified by tools/le_info.py:
     *   LE header 0x290A4, page size 0x1000, 213 pages,
     *   object 0 code  base 0x10000 size 0x63B15 100 pages
     *   object 1 data  base 0x80000 size 0x8B0D0 113 pages
     * After loading, the data object's string table must be present. These are
     * real bytes from the shipped file (port/decomp/prage.strings.csv lists
     * them at DS:0x0004, 0x0010, 0x001C), so they prove the page map and object
     * mapping are right before any fixup is applied. */
    CHECK(mem_in_range(DATA_BASE, 0x8B0D0), "data object fits");
    CHECK(memcmp(mem + DATA_BASE + 4, "SB16.DIG", 9) == 0,
          "DS:0x0004 is the SB16.DIG string");
    CHECK(memcmp(mem + DATA_BASE + 0x10, "SBPRO.DIG", 10) == 0,
          "DS:0x0010 is the SBPRO.DIG string");
    CHECK(memcmp(mem + DATA_BASE + 0x1C, "SBLASTER.DIG", 13) == 0,
          "DS:0x001C is the SBLASTER.DIG string");
    return g_failures - before;
}
