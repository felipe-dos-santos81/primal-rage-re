#include "platform/res.h"
#include "mem.h"
#include "symbols.h"
#include "test.h"
#include <stdio.h>
#include <string.h>

int test_res(void)
{
    int before = g_failures;

    u32 n = res_load_index("data/game/C", "data/game/C/INDEX");
    /* port/FORMATS.md: the shipped C/INDEX has 69 entries. */
    CHECK_EQ_INT(n, 69);
    CHECK_EQ_INT(res_count(), 69);

    /* The table pointer and count live where 0x1B120 puts them. */
    CHECK(mem_in_range(DSD(DS_001014E0), 69 * 20), "table pointer is in range");
    CHECK_EQ_INT(DSD(DS_001014F0), 69);

    /* Every entry got a data block, and DS_001014F8 is the largest size. */
    u32 biggest = 0, have_data = 0;
    for (u32 i = 0; i < res_count(); i++) {
        if (DSD(DSD(DS_001014E0) + i * 20 + 16) != 0) have_data++;
        if (res_size(i) > biggest) biggest = res_size(i);
    }
    CHECK_EQ_INT(have_data, 69);
    CHECK_EQ_INT(DSD(DS_001014F8), biggest);

    /* Names are 12-byte zero-padded ASCII, so compare the first NUL only. */
    int found = 0;
    for (u32 i = 0; i < res_count(); i++) {
        const char *name = res_name(i);
        if (strncmp(name, "s16title.gra", 12) == 0 ||
            strncmp(name, "S16TITLE.GRA", 12) == 0) {
            found = 1;
            /* The size field must equal the on-disk file size. */
            char path[256];
            snprintf(path, sizeof path, "data/game/C/%.*s", 12, name);
            FILE *f = fopen(path, "rb");
            CHECK(f != NULL, "resource file opens");
            if (f) {
                fseek(f, 0, SEEK_END);
                CHECK_EQ_INT(res_size(i), ftell(f));
                fclose(f);
            }
            /* Its bytes were actually read, and a GRA begins with the chunk
             * header: u16 type = 2, then the magic "43". */
            u8 *data = res_resolve(res_handle(i, 0));
            CHECK(data != NULL, "handle resolves to the resource data");
            if (data) {
                CHECK_EQ_INT(data[0], 2);
                CHECK_EQ_INT(data[1], 0);
                CHECK_EQ_INT(data[2], '4');
                CHECK_EQ_INT(data[3], '3');
                /* Offset arithmetic: handle low 23 bits index into the data. */
                CHECK_EQ_INT(res_resolve(res_handle(i, 4)), data + 4);
            }
        }
    }
    CHECK(found, "s16title.gra is in the index");

    CHECK(res_resolve(0xFFFFFFFFu) == NULL, "an out-of-range handle resolves to NULL");

    /* The Smacker movies are not in INDEX, so they load by name. The shipped
     * files are uppercase (TWI5.SMK) and the callers use lowercase, so this
     * exercises the case-insensitive scan. */
    u32 off = 0, size = 0;
    CHECK_EQ_INT(res_load_file("data/game/C", "twi5.smk", &off, &size), 1);
    CHECK_EQ_INT(size, 1208576);
    CHECK(mem_in_range(off, size), "movie bytes got a block in mem[]");
    CHECK_EQ_INT(mem[off], 'S');
    CHECK_EQ_INT(mem[off + 1], 'M');
    CHECK_EQ_INT(mem[off + 2], 'K');
    CHECK_EQ_INT(res_load_file("data/game/C", "twg.smk", &off, &size), 1);
    CHECK_EQ_INT(size, 31048);

    /* A missing file fails without touching either output. */
    u32 miss_off = 0xDEADBEEFu, miss_size = 0xFEEDFACEu;
    CHECK_EQ_INT(res_load_file("data/game/C", "nope.smk", &miss_off, &miss_size), 0);
    CHECK_EQ_INT(miss_off, 0xDEADBEEFu);
    CHECK_EQ_INT(miss_size, 0xFEEDFACEu);

    return g_failures - before;
}
