#include "test.h"
#include "platform/audio/mixer.h"

#define FRAMES 64

static s16 out[2 * FRAMES];

static int all_zero(const s16 *b, int n)
{
    for (int i = 0; i < n; i++)
        if (b[i] != 0)
            return 0;
    return 1;
}

static long sum_abs(const s16 *b, int n)
{
    long s = 0;
    for (int i = 0; i < n; i++)
        s += b[i] < 0 ? -(long)b[i] : (long)b[i];
    return s;
}

int test_mixer(void)
{
    int before = g_failures;

    /* Nothing playing: OPL reset with no register writes renders exact zeros
     * across the whole buffer (Task 4 property), and no voice is added. */
    mixer_reset();
    mixer_render(out, FRAMES, 44100);
    CHECK(all_zero(out, 2 * FRAMES), "reset mixer with no voices is exact silence");

    /* One voice: nonzero output. Tone at 1 kHz-ish, volume 128 (~0.5), looping,
     * rate == out_rate so no resampling is involved. */
    static s16 tone[8] = { 1000, -1000, 1000, -1000, 1000, -1000, 1000, -1000 };
    mixer_reset();
    mixer_add_sample(tone, 8, 44100, 128, 1);
    mixer_render(out, FRAMES, 44100);
    CHECK(!all_zero(out, 2 * FRAMES), "one voice produces non-silence");

    /* Two voices louder than one. Volume 128 with amplitude 1000 gives +/-500
     * per voice, so two voices sum to +/-1000: well inside s16, the comparison
     * cannot be explained by clipping. */
    long one = sum_abs(out, 2 * FRAMES);
    mixer_reset();
    mixer_add_sample(tone, 8, 44100, 128, 1);
    mixer_add_sample(tone, 8, 44100, 128, 1);
    mixer_render(out, FRAMES, 44100);
    long two = sum_abs(out, 2 * FRAMES);
    CHECK(two > one, "two voices together are louder than one");

    /* Stopped voices contribute nothing. */
    mixer_reset();
    mixer_add_sample(tone, 8, 44100, 256, 1);
    mixer_stop_samples();
    mixer_render(out, FRAMES, 44100);
    CHECK(all_zero(out, 2 * FRAMES), "stop_samples returns output to silence");

    /* Clipping saturates, never wraps. Four voices at unity of +/-30000 sum to
     * +/-120000, past both s16 limits. Every sample must be exactly a limit and
     * in the direction of the input sign. */
    static s16 loud[2] = { 30000, -30000 };
    mixer_reset();
    for (int i = 0; i < 4; i++)
        mixer_add_sample(loud, 2, 44100, 256, 1);
    mixer_render(out, FRAMES, 44100);
    int saw_pos = 0, saw_neg = 0, outside = 0;
    for (int i = 0; i < 2 * FRAMES; i++) {
        if (out[i] == 32767) saw_pos = 1;
        else if (out[i] == -32768) saw_neg = 1;
        else outside = 1;
    }
    CHECK(saw_pos && saw_neg, "clipping saturates both s16 limits");
    CHECK(!outside, "no sample wraps past the s16 limits");

    /* A voice whose rate differs from out_rate is resampled without reading out
     * of bounds. Two-sample buffer, 100x the output rate: an unclamped
     * nearest-neighbour read would step ~100 samples past the end on the first
     * frame. Canary values immediately after the buffer must survive. */
    static struct {
        s16 pcm[2];
        s16 canary[4];
    } g = { { 32000, 32000 }, { 111, 222, 333, 444 } };
    mixer_reset();
    mixer_add_sample(g.pcm, 2, 44100 * 100, 256, 0);
    mixer_render(out, FRAMES, 44100);
    CHECK_EQ_INT(g.canary[0], 111);
    CHECK_EQ_INT(g.canary[1], 222);
    CHECK_EQ_INT(g.canary[2], 333);
    CHECK_EQ_INT(g.canary[3], 444);
    CHECK_EQ_INT(g.pcm[0], 32000);
    CHECK_EQ_INT(g.pcm[1], 32000);

    return g_failures - before;
}
