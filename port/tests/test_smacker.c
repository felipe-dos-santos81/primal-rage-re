#include "platform/smacker.h"
#include "test.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static u32 count_raw(const char *dir)
{
    DIR *d = opendir(dir);
    struct dirent *e;
    u32 n = 0;
    if (d == NULL) return 0;
    while ((e = readdir(d)) != NULL) {
        size_t l = strlen(e->d_name);
        if (l > 4 && strcmp(e->d_name + l - 4, ".raw") == 0) n++;
    }
    closedir(d);
    return n;
}

/* The oracle's presented-frame count, from Task 2's capture directory. Absent
 * capture: skip unless PR_ORACLE_REQUIRED=1, then fail (the 2a gate). */
static void check_capture(const char *movie, u32 presented)
{
    char dir[512];
    u32 n;
    snprintf(dir, sizeof(dir), "data/smk-captures/%s", movie);
    n = count_raw(dir);
    if (n == 0) {
        if (getenv("PR_ORACLE_REQUIRED") != NULL)
            CHECK(0, "PR_ORACLE_REQUIRED=1 but the smacker capture is missing");
        else
            printf("test_smacker: no capture at %s, presented count not checked\n", dir);
        return;
    }
    CHECK_EQ_INT(presented, n);
}

/* Decode every payload frame of `file` in `dir` into the caller-owned index
 * buffer, in place (SKIP and palette deltas depend on the previous frame).
 * When `dump_dir` is non-NULL, write each presented frame as 320x200 RGB24
 * through the current palette (`frame_%04u.raw`). Reports the presented count
 * via `*presented`. Returns the header frame count, or 0 when it will not open.
 *
 * The original never presents a trailing ring frame that changes the image; a
 * trailing no-op hold is the image it does show, so it is presented (TWG's last
 * frames are 8-byte SKIP holds; TWI5's last frame is new content). */
static u32 decode_movie(const char *dir, const char *file, const char *dump_dir,
                        u32 *presented)
{
    char path[512];
    static u8 data[2 << 20];
    static u8 frame[320 * 200];
    static u8 prev[320 * 200];
    static u8 rgb[320 * 200 * 3];
    static u8 dac[256][3];
    static SmkMovie m;
    size_t sz;
    u32 i, wh, n, written = 0;
    FILE *f;

    snprintf(path, sizeof(path), "%s/%s", dir, file);
    f = fopen(path, "rb");
    CHECK(f != NULL, "movie opens");
    if (f == NULL) return 0;
    sz = fread(data, 1, sizeof(data), f);
    fclose(f);
    CHECK(sz > 0, "movie reads");
    CHECK(smk_open(data, (u32)sz, &m), "smk_open accepts the movie");

    wh = m.width * m.height;
    n = smk_frames(&m);
    for (i = 0; i < n; i++) {
        int ring = (i + 1 == n) && (i > 0);   /* trailing ring/hold frame */
        int hold;
        CHECK(smk_decode_frame(&m, frame), "frame decodes");
        hold = ring && memcmp(frame, prev, wh) == 0;
        if (!ring || hold) {
            if (dump_dir != NULL) {
                u32 p;
                smk_palette_to(&m, dac);
                for (p = 0; p < wh; p++) {
                    rgb[p * 3 + 0] = dac[frame[p]][0];
                    rgb[p * 3 + 1] = dac[frame[p]][1];
                    rgb[p * 3 + 2] = dac[frame[p]][2];
                }
                snprintf(path, sizeof(path), "%s/frame_%04u.raw", dump_dir, i);
                f = fopen(path, "wb");
                CHECK(f != NULL, "dump frame opens");
                if (f != NULL) {
                    fwrite(rgb, 1, (size_t)wh * 3, f);
                    fclose(f);
                }
            }
            written++;
        }
        memcpy(prev, frame, wh);
    }

    /* Past the last frame the decoder must reject, never re-read. */
    CHECK_EQ_INT(smk_decode_frame(&m, frame), 0);

    smk_palette_to(&m, dac);
    {
        int nonzero = 0, c, k;
        for (c = 0; c < 256; c++)
            for (k = 0; k < 3; k++) nonzero += dac[c][k] != 0;
        CHECK(nonzero > 0, "palette is populated");
    }
    *presented = written;
    return n;
}

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

    /* Task 4: all four trees (mmap, mclr, full, type) decode into the arena,
     * in order, and every tree/last pointer stays inside `words`. */
    CHECK(m.tree[0] != NULL && m.tree[1] != NULL &&
          m.tree[2] != NULL && m.tree[3] != NULL, "all four trees decoded");
    for (int i = 0; i < 4; i++) {
        CHECK(m.tree[i] != NULL && m.tree[i] >= m.words &&
              m.tree[i] < m.words + SMK_TREE_WORDS, "tree pointer in arena");
        for (int k = 0; k < 3; k++)
            CHECK(m.last[i][k] != NULL && m.last[i][k] >= m.words &&
                  m.last[i][k] < m.words + SMK_TREE_WORDS, "last pointer in arena");
        if (i > 0)
            CHECK(m.tree[i - 1] < m.tree[i], "trees laid out in order");
    }

    /* Fixed profile: a truncated header, a bad magic, and a size that runs
     * past the buffer are all rejected. */
    CHECK_EQ_INT(smk_open(data, 8, &m), 0);
    u8 save = data[0]; data[0] = 'X';
    CHECK_EQ_INT(smk_open(data, (u32)sz, &m), 0);
    data[0] = save;

    /* A frame_size entry whose payload runs past the buffer is rejected: the
     * table starts at 0x68, so corrupt frame 0 to an oversized value. */
    u8 save_size[4];
    for (int i = 0; i < 4; i++) { save_size[i] = data[0x68 + i]; data[0x68 + i] = 0; }
    data[0x68] = 0xFF; data[0x69] = 0xFF; data[0x6A] = 0xFF; data[0x6B] = 0x7F;
    CHECK_EQ_INT(smk_open(data, (u32)sz, &m), 0);
    for (int i = 0; i < 4; i++) data[0x68 + i] = save_size[i];

    /* A tree value count too large for the arena (SMK_TREE_WORDS) is rejected:
     * each of the four tree_size fields is set to 0x7FFFFFFF, which the
     * container check does not look at but the tree decode must. */
    u8 save_ts[16];
    for (int i = 0; i < 16; i++) { save_ts[i] = data[0x38 + i]; data[0x38 + i] = 0xFF; }
    for (int i = 0; i < 4; i++) data[0x38 + 4 * i + 3] = 0x7F;
    m.width = 0xDEADBEEFu;
    CHECK_EQ_INT(smk_open(data, (u32)sz, &m), 0);
    CHECK_EQ_INT(m.width, 0xDEADBEEFu);   /* *out untouched on rejection */
    for (int i = 0; i < 16; i++) data[0x38 + i] = save_ts[i];

    /* Task 5: every frame of both movies decodes in bounds, the palette ends
     * populated, and the presented counts are the capture's (TWI5 120 of the
     * header's 121 - the trailing ring frame is decoded but not presented;
     * TWG 41). With PR_SMK_DUMP set, write the RGB oracle frames too. */
    const char *dump = getenv("PR_SMK_DUMP");
    char dump_sub[600];
    u32 presented_twi5, presented_twg;

    if (dump != NULL) {
        snprintf(dump_sub, sizeof(dump_sub), "%s/twi5", dump);
        mkdir(dump, 0777);
        mkdir(dump_sub, 0777);
    }
    CHECK_EQ_INT(decode_movie(dir, "TWI5.SMK", dump ? dump_sub : NULL,
                              &presented_twi5), 121);
    CHECK_EQ_INT(presented_twi5, 120);
    check_capture("twi5", presented_twi5);

    if (dump != NULL) {
        snprintf(dump_sub, sizeof(dump_sub), "%s/twg", dump);
        mkdir(dump_sub, 0777);
    }
    CHECK_EQ_INT(decode_movie(dir, "TWG.SMK", dump ? dump_sub : NULL,
                              &presented_twg), 41);
    CHECK_EQ_INT(presented_twg, 41);
    check_capture("twg", presented_twg);

    /* Bounds: a 4x4 movie has a single 4x4 block, so decoding must never touch
     * a pixel past the first 16 even though every run in these trees is sized
     * for the 320x200 grid. The run bound is what keeps that true. */
    data[0x04] = 4; data[0x05] = 0; data[0x06] = 0; data[0x07] = 0;
    data[0x08] = 4; data[0x09] = 0; data[0x0A] = 0; data[0x0B] = 0;
    static SmkMovie small;
    CHECK(smk_open(data, (u32)sz, &small), "smk_open accepts the 4x4 variant");
    {
        static u8 sbuf[320 * 200];
        u32 f, k, over = 0;
        for (f = 0; f < smk_frames(&small); f++) {
            memset(sbuf, 0xAA, sizeof(sbuf));
            CHECK(smk_decode_frame(&small, sbuf), "4x4 frame decodes");
            for (k = 16; k < sizeof(sbuf); k++)
                if (sbuf[k] != 0xAA) over++;
        }
        CHECK_EQ_INT(over, 0);
    }
    data[0x04] = 0x40; data[0x05] = 0x01; data[0x06] = 0; data[0x07] = 0;
    data[0x08] = 0xC8; data[0x09] = 0; data[0x0A] = 0; data[0x0B] = 0;

    return g_failures - before;
}
