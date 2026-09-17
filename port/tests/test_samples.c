#include "platform/audio/samples.h"
#include "test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The shipped sample Task 1 located: a RIFF/WAVE blob at file offset 0x24b13 in
 * S16SOUND.GRA (port/spec/audio.md "Samples"). It is an 8-bit unsigned mono PCM
 * WAV at 11025 Hz with 19327 data bytes; the fmt+data prefix is 44 + 19327. The
 * file's RIFF size field also counts a trailing LIST/INFO and fact chunk, so
 * only the prefix is read here — the loader must bound itself by the length it
 * is given, not by that field. */
#define SND_GRA "data/game/C/S16SOUND.GRA"
#define SAMPLE_OFF 0x24b13u
#define SAMPLE_LEN (44u + 19327u)

static u8 g_blob[SAMPLE_LEN];

static int read_blob(void)
{
    FILE *f = fopen(SND_GRA, "rb");
    if (!f) return 0;
    if (fseek(f, (long)SAMPLE_OFF, SEEK_SET) != 0) { fclose(f); return 0; }
    size_t got = fread(g_blob, 1, sizeof g_blob, f);
    fclose(f);
    return got == sizeof g_blob;
}

/* A rejected load must never leave a half-filled descriptor. */
static void check_zeroed(const SampleVoice *v, const char *msg)
{
    CHECK(v->pcm == NULL && v->frames == 0 && v->rate == 0 && v->channels == 0,
          msg);
}

int test_samples(void)
{
    int before = g_failures;
    SampleVoice v;

    /* Zero-length sample: rejected regardless of any asset. */
    memset(&v, 0xAB, sizeof v);
    CHECK_EQ_INT(samples_load(NULL, 0, &v), 0);
    check_zeroed(&v, "zero-length load leaves out zeroed");

    /* A non-WAV buffer: same. */
    {
        static const u8 junk[64] = { 'N', 'O', 'T', 'W', 'A', 'V', 'E', '!' };
        memset(&v, 0xAB, sizeof v);
        CHECK_EQ_INT(samples_load(junk, sizeof junk, &v), 0);
        check_zeroed(&v, "non-WAV buffer rejected, out zeroed");
        CHECK_EQ_INT(samples_load(junk, sizeof junk, NULL), 0);
        CHECK_EQ_INT(samples_load(NULL, 16, &v), 0);
    }

    if (!read_blob()) {
        if (getenv("PR_ORACLE_REQUIRED")) {
            CHECK(0, "PR_ORACLE_REQUIRED=1 but S16SOUND.GRA sample blob is missing");
        } else {
            printf("SKIP sample loader real-data checks (need " SND_GRA
                   " @0x%x, %u bytes)\n", SAMPLE_OFF, (unsigned)SAMPLE_LEN);
        }
        return g_failures - before;
    }

    /* Confirm the bytes we are about to rely on are the located blob. */
    CHECK(memcmp(g_blob, "RIFF", 4) == 0 && memcmp(g_blob + 8, "WAVE", 4) == 0,
          "located bytes are a RIFF/WAVE blob");

    /* 1. Valid sample: real rate, channels and frame count. */
    CHECK_EQ_INT(samples_load(g_blob, SAMPLE_LEN, &v), 1);
    CHECK_EQ_INT(v.rate, 11025);
    CHECK_EQ_INT(v.channels, 1);
    CHECK_EQ_INT(v.frames, 19327);
    CHECK(v.pcm == g_blob + 44, "pcm references the caller's bytes, not a copy");

    /* 2. Truncated input: every prefix shorter than the declared PCM (and every
     *    header cut) is rejected, never read past `len`. */
    memset(&v, 0xAB, sizeof v);
    CHECK_EQ_INT(samples_load(g_blob, SAMPLE_LEN - 1, &v), 0);
    check_zeroed(&v, "sample truncated at its end rejected");
    CHECK_EQ_INT(samples_load(g_blob, 44 + 100, &v), 0);
    check_zeroed(&v, "PCM cut to 100 bytes rejected");
    CHECK_EQ_INT(samples_load(g_blob, 20, &v), 0);
    check_zeroed(&v, "header cut mid-fmt rejected");
    CHECK_EQ_INT(samples_load(g_blob, 4, &v), 0);

    /* 3. Unknown format rejected: unsupported tag, bit depth, channel count,
     *    and an empty data chunk. Each edits a copy of the real header. */
    {
        static u8 bad[SAMPLE_LEN];
        memcpy(bad, g_blob, sizeof bad);
        bad[20] = 3; bad[21] = 0;           /* fmt tag 3 = IEEE float */
        memset(&v, 0xAB, sizeof v);
        CHECK_EQ_INT(samples_load(bad, sizeof bad, &v), 0);
        check_zeroed(&v, "non-PCM format tag rejected");

        memcpy(bad, g_blob, sizeof bad);
        bad[34] = 16; bad[35] = 0;          /* 16-bit PCM */
        CHECK_EQ_INT(samples_load(bad, sizeof bad, &v), 0);
        check_zeroed(&v, "16-bit sample rejected");

        memcpy(bad, g_blob, sizeof bad);
        bad[22] = 2; bad[23] = 0;           /* 2 channels */
        CHECK_EQ_INT(samples_load(bad, sizeof bad, &v), 0);
        check_zeroed(&v, "stereo sample rejected");

        memcpy(bad, g_blob, sizeof bad);
        bad[40] = 0; bad[41] = 0; bad[42] = 0; bad[43] = 0;  /* data size 0 */
        CHECK_EQ_INT(samples_load(bad, sizeof bad, &v), 0);
        check_zeroed(&v, "zero-length data chunk rejected");
    }

    return g_failures - before;
}
