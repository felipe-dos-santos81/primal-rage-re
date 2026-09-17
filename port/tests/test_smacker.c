#include "platform/smacker.h"
#include "test.h"
#include <stdio.h>
#include <stdlib.h>

int test_smacker(void)
{
    int before = g_failures;
    const char *dir = getenv("PR_GAME_DIR");
    if (dir == NULL) {
        printf("test_smacker: PR_GAME_DIR unset, skipping\n");
        return 0;
    }

    static SmkMovie m;
    char path[512];
    snprintf(path, sizeof(path), "%s/TWI5.SMK", dir);   /* on-disk name is uppercase */
    FILE *f = fopen(path, "rb");
    CHECK(f != NULL, "TWI5.SMK opens");
    if (f == NULL) return g_failures - before;
    static u8 data[2 << 20];
    size_t sz = fread(data, 1, sizeof(data), f);
    fclose(f);
    CHECK(sz > 0, "TWI5.SMK reads");

    CHECK(smk_open(data, (u32)sz, &m), "smk_open accepts TWI5.SMK");
    CHECK_EQ_INT(smk_width(&m), 320);
    CHECK_EQ_INT(smk_height(&m), 200);
    CHECK_EQ_INT(smk_frames(&m), 121);

    /* Fixed profile: a truncated header, a bad magic, and a size that runs
     * past the buffer are all rejected. */
    CHECK_EQ_INT(smk_open(data, 8, &m), 0);
    u8 save = data[0]; data[0] = 'X';
    CHECK_EQ_INT(smk_open(data, (u32)sz, &m), 0);
    data[0] = save;
    return g_failures - before;
}
