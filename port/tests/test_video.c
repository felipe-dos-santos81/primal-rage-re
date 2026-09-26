/* test_video.c — the video suite.
 *
 * Consolidated from: test_smacker.c, test_movie.c.
 * Every assertion is carried verbatim; only the file's home and the
 * two cross-file static names (none) changed. */

#include "platform/smacker.h"
#include "test.h"
#include "game/movie.h"
#include "platform/gfx.h"
#include "mem.h"
#include "symbols.h"
#include "host.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>


/* ---- test_smacker.c ---- */

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

/* The capture holds the frames the original presents: a prefix of the decoded
 * sequence (settled by re-capture, commit 199cad1: TWI5 120 of 121, TWG 41 of
 * 41; TWI5's last payload frame is real and never presented). Absent capture:
 * skip unless PR_ORACLE_REQUIRED=1, then fail (the 2a gate). */
static void check_capture(const char *movie, u32 expected, u32 decoded)
{
    char dir[512];
    u32 n;
    snprintf(dir, sizeof(dir), "data/smk-captures/%s", movie);
    n = count_raw(dir);
    if (n == 0) {
        if (getenv("PR_ORACLE_REQUIRED") != NULL)
            CHECK(0, "PR_ORACLE_REQUIRED=1 but the smacker capture is missing");
        else
            printf("test_smacker: no capture at %s, prefix not checked\n", dir);
        return;
    }
    CHECK_EQ_INT(n, expected);                 /* the settled presented count */
    CHECK(decoded >= n, "every presented frame is decoded");
}

/* Decode every payload frame of `file` in `dir` into the caller-owned index
 * buffer, in place (SKIP and palette deltas depend on the previous frame).
 * When `dump_dir` is non-NULL, write every frame as 320x200 RGB24 through the
 * current palette (`frame_%04u.raw`). Reports the decoded count via `*written`
 * and returns the header frame count, or 0 when it will not open.
 *
 * The decoder never drops a frame; which of the decoded frames the original
 * presents is the player's rule (Task 7). The oracle compares the capture's
 * prefix with `smk_compare.py --frames <capture count>`. */
static u32 decode_movie(const char *dir, const char *file, const char *dump_dir,
                        u32 *written)
{
    char path[512];
    static u8 data[2 << 20];
    static u8 frame[320 * 200];
    static u8 rgb[320 * 200 * 3];
    static u8 dac[256][3];
    static SmkMovie m;
    size_t sz;
    u32 i, wh, n, count = 0;
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
        CHECK(smk_decode_frame(&m, frame), "frame decodes");
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
        count++;
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
    *written = count;
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

    /* Task 5: every frame of both movies decodes in bounds (no dropping) and
     * the palette ends populated. The decoder's count equals the header (TWI5
     * 121, TWG 41). The collapsed smk capture holds a prefix (settled by
     * re-capture, commit 199cad1): TWI5 120 of 121 whole frames - its 121st
     * payload frame is blitted but blanked by 0x52106 before a whole scanout
     * (record §36, front-end capture 2094) - and TWG 41 of 41. That
     * presentation rule is the player's (Task 7), not the test's; here the
     * oracle compares the captured prefix via `smk_compare.py --frames
     * <capture count>`. With PR_SMK_DUMP
     * set, write every decoded frame as RGB24. */
    const char *dump = getenv("PR_SMK_DUMP");
    char dump_sub[600];
    u32 decoded_twi5, decoded_twg;

    if (dump != NULL) {
        snprintf(dump_sub, sizeof(dump_sub), "%s/twi5", dump);
        mkdir(dump, 0777);
        mkdir(dump_sub, 0777);
    }
    CHECK_EQ_INT(decode_movie(dir, "TWI5.SMK", dump ? dump_sub : NULL,
                              &decoded_twi5), 121);
    CHECK_EQ_INT(decoded_twi5, 121);
    check_capture("twi5", 120, decoded_twi5);

    if (dump != NULL) {
        snprintf(dump_sub, sizeof(dump_sub), "%s/twg", dump);
        mkdir(dump_sub, 0777);
    }
    CHECK_EQ_INT(decode_movie(dir, "TWG.SMK", dump ? dump_sub : NULL,
                              &decoded_twg), 41);
    CHECK_EQ_INT(decoded_twg, 41);
    check_capture("twg", 41, decoded_twg);

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

/* ---- test_movie.c ---- */

/* The player owns the boot-logos presentation rule. The decoder decodes every
 * payload frame, and 0x1C740 blits every one (its loop runs uVar7 = 1..count,
 * the last frame included), then blanks the screen with 0x52106(0): TWI5
 * presents 121 (its 121st frame is on screen only until the exit blank, which
 * is why the collapsed smk capture holds 120 whole frames while the front-end
 * capture's 2094 splices frame 119 into it; record §36) and TWG 41.
 * `movie_frames_presented()` reports the count the last movie_play() actually
 * presented, so the rule is asserted on the real playback, not on a helper.
 * The screen hook sees every screen the player writes: the entry blank, each
 * frame, and the exit blank.
 * A headless gate must not pace against the wall clock: the suite never opens a
 * window, so the movie's real-time tick budget (TWI5 ~515 ticks) must not be
 * spent waiting. Pacing is re-enabled when a window is open (windowed run). */
static int g_movie_hook;
static int g_movie_hook_first_lit, g_movie_hook_second_lit;
static int g_movie_hook_last_lit;

/* 1 when the displayed screen holds a non-zero index or the DAC a non-zero
 * colour for index 0 (the blanks clear both). */
static int movie_screen_lit(void)
{
    const u8 *ap = gfx_aperture();
    for (u32 i = 0; i < 320u * 200u; i++)
        if (ap[i] != 0) return 1;
    return gfx_dac[0][0] | gfx_dac[0][1] | gfx_dac[0][2] ? 1 : 0;
}

static void movie_hook(void)
{
    int lit = movie_screen_lit();
    if (g_movie_hook == 0) g_movie_hook_first_lit = lit;
    if (g_movie_hook == 1) g_movie_hook_second_lit = lit;
    g_movie_hook_last_lit = lit;
    g_movie_hook++;
}

int test_movie(void)
{
    int before = g_failures;
    const char *dir = getenv("PR_GAME_DIR");
    if (dir == NULL) {
        printf("test_movie: PR_GAME_DIR unset, skipping\n");
        return 0;
    }

    /* 0x52106 at entry and exit: seed the tick counters, the DAC and the
     * aperture non-zero; the hook records the calls and whether the screen was
     * black on the first and last. */
    movie_set_screen_hook(movie_hook);
    DSD(DS_00101508) = 0x55u;
    DSD(DS_0010150C) = 0x66u;
    memset(gfx_dac, 0x2A, sizeof gfx_dac);
    memset(gfx_aperture(), 0x11, 320 * 200);
    g_movie_hook = 0;
    u32 ticks0 = host_tick_count();
    CHECK_EQ_INT(movie_play(dir, "twi5.smk"), 1);
    CHECK_EQ_INT((int)movie_frames_presented(), 121);
    CHECK((host_tick_count() - ticks0) < 100,
          "headless playback does not sleep on the VBlank clock");
    CHECK_EQ_INT(g_movie_hook, 123);
    CHECK_EQ_INT(g_movie_hook_first_lit, 0);
    CHECK_EQ_INT(g_movie_hook_second_lit, 1);
    CHECK_EQ_INT(g_movie_hook_last_lit, 0);
    CHECK_EQ_INT((int)DSD(DS_00101508), 0);
    CHECK_EQ_INT((int)DSD(DS_0010150C), 0);
    CHECK_EQ_INT(gfx_dac[200][1], 0);
    CHECK_EQ_INT((int)gfx_aperture()[1000], 0);

    g_movie_hook = 0;
    CHECK_EQ_INT(movie_play(dir, "twg.smk"), 1);
    CHECK_EQ_INT((int)movie_frames_presented(), 41);
    CHECK_EQ_INT(g_movie_hook, 43);

    /* A missing name is a clean skip: return 1, message, no crash, nothing
     * presented. The name is lowercase while the file is uppercase; the scan
     * inside res_load_file handles the case mismatch, so a missing name is a
     * genuine miss, not a case artefact. */
    g_movie_hook = 0;
    CHECK_EQ_INT(movie_play(dir, "no-such-movie.smk"), 1);
    CHECK_EQ_INT((int)movie_frames_presented(), 0);
    CHECK_EQ_INT(g_movie_hook, 1);          /* the entry blank only */

    /* Invalid arguments are a hard failure (0), never a crash. */
    g_movie_hook = 0;
    CHECK_EQ_INT(movie_play(NULL, "twi5.smk"), 0);
    CHECK_EQ_INT(movie_play(dir, NULL), 0);
    CHECK_EQ_INT(g_movie_hook, 0);
    movie_set_screen_hook(NULL);

    return g_failures - before;
}
