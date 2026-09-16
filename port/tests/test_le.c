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

    /* The data object's virtual size (0x8B0D0) exceeds its file-backed pages
     * (113 * 0x1000 == 0x71000). Poison that BSS tail, reload, and require the
     * loader to have zeroed it: this fails if the tail is left to whatever
     * mem[] held before, which a zero-initialised global would disguise. */
    {
        u32 mapped = 113u * 0x1000u;
        mem_fill(DATA_BASE + mapped, 0xFF, 0x8B0D0 - mapped);
        CHECK(mem_load_le(EXE, NULL) == 1, "reload after poisoning the BSS tail");
        int dirty = 0;
        for (u32 i = 0; i < 0x8B0D0 - mapped; i++)
            if (mem[DATA_BASE + mapped + i] != 0) dirty = 1;
        CHECK(!dirty, "data object BSS tail is zero-filled, not stale");
    }
    return g_failures - before;
}
