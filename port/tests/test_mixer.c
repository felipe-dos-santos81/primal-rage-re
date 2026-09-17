#include "test.h"
#include "platform/audio/mixer.h"

#define FRAMES 64

static s16 out[2 * FRAMES];

/* Voice owners for the per-voice stop test; any distinct addresses work. */
static int owner_a, owner_b;

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
    mixer_add_sample(tone, 8, 44100, 128, 1, &owner_a);
    mixer_render(out, FRAMES, 44100);
    CHECK(!all_zero(out, 2 * FRAMES), "one voice produces non-silence");

    /* Two voices louder than one. Volume 128 with amplitude 1000 gives +/-500
     * per voice, so two voices sum to +/-1000: well inside s16, the comparison
     * cannot be explained by clipping. */
    long one = sum_abs(out, 2 * FRAMES);
    mixer_reset();
    mixer_add_sample(tone, 8, 44100, 128, 1, &owner_a);
    mixer_add_sample(tone, 8, 44100, 128, 1, &owner_a);
    mixer_render(out, FRAMES, 44100);
    long two = sum_abs(out, 2 * FRAMES);
    CHECK(two > one, "two voices together are louder than one");

    /* Stopped voices contribute nothing. */
    mixer_reset();
    mixer_add_sample(tone, 8, 44100, 256, 1, &owner_a);
    mixer_stop_samples();
    mixer_render(out, FRAMES, 44100);
    CHECK(all_zero(out, 2 * FRAMES), "stop_samples returns output to silence");

    /* Clipping saturates, never wraps. Four voices at unity of +/-30000 sum to
     * +/-120000, past both s16 limits. Every sample must be exactly a limit and
     * in the direction of the input sign. */
    static s16 loud[2] = { 30000, -30000 };
    mixer_reset();
    for (int i = 0; i < 4; i++)
        mixer_add_sample(loud, 2, 44100, 256, 1, &owner_a);
    mixer_render(out, FRAMES, 44100);
    int saw_pos = 0, saw_neg = 0, outside = 0;
    for (int i = 0; i < 2 * FRAMES; i++) {
        if (out[i] == 32767) saw_pos = 1;
        else if (out[i] == -32768) saw_neg = 1;
        else outside = 1;
    }
    CHECK(saw_pos && saw_neg, "clipping saturates both s16 limits");
    CHECK(!outside, "no sample wraps past the s16 limits");

    /* A voice whose rate differs from out_rate is resampled without reading
     * outside its buffer. Six-sample ramp at twice out_rate: the 16.16 phase
     * advances exactly two samples per output frame, so a one-shot voice reads
     * ramp[0], ramp[2], ramp[4] and then, at the fourth frame, has idx == 6 ==
     * frames. Expected output is exactly those three values followed by silence
     * — asserted per frame, so an unclamped read of ramp[6] would change frame 4
     * and fail. (The negative control needs the over-read to be deterministic:
     * temporarily stripping the idx >= frames guard and padding the array is
     * what confirms this test can fail.) */
    static s16 ramp[6] = { 100, 200, 300, 400, 500, 600 };
    mixer_reset();
    mixer_add_sample(ramp, 6, 44100 * 2, 256, 0, &owner_a);
    mixer_render(out, 6, 44100);
    CHECK_EQ_INT(out[0], 100);
    CHECK_EQ_INT(out[1], 100);
    CHECK_EQ_INT(out[2], 300);
    CHECK_EQ_INT(out[3], 300);
    CHECK_EQ_INT(out[4], 500);
    CHECK_EQ_INT(out[5], 500);
    CHECK_EQ_INT(out[6], 0);
    CHECK_EQ_INT(out[7], 0);
    CHECK_EQ_INT(out[8], 0);
    CHECK_EQ_INT(out[9], 0);
    CHECK_EQ_INT(out[10], 0);
    CHECK_EQ_INT(out[11], 0);

    /* Per-voice stop: stopping one owner leaves the other sounding; a stop for
     * an owner with no active voice changes nothing; stopping the last returns
     * to exact silence (OPL was reset and never written in this block). */
    {
        static s16 a[2] = { 1000, -1000 };
        static s16 b[2] = { 2000, -2000 };
        mixer_reset();
        mixer_add_sample(a, 2, 44100, 256, 1, &owner_a);
        mixer_add_sample(b, 2, 44100, 256, 1, &owner_b);
        mixer_stop_sample(&owner_a);
        mixer_render(out, FRAMES, 44100);
        CHECK(!all_zero(out, 2 * FRAMES),
              "stopping one owner leaves the other voice sounding");

        mixer_stop_sample(&owner_a);   /* already stopped: stale-safe */
        mixer_render(out, FRAMES, 44100);
        CHECK(!all_zero(out, 2 * FRAMES),
              "a stop for an inactive owner leaves the live voice alone");

        mixer_stop_sample(&owner_b);
        mixer_render(out, FRAMES, 44100);
        CHECK(all_zero(out, 2 * FRAMES),
              "stopping the last sample voice is exact silence");
    }

    return g_failures - before;
}
