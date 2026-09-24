/* test_audio.c — the audio suite.
 *
 * Consolidated from: test_opl.c, test_pitch.c, test_samples.c, test_mixer.c, test_sequencer.c, test_ail.c.
 * Every assertion is carried verbatim; only the file's home and the
 * two cross-file static names (all_zero/out) changed. */

#include "test.h"
#include "platform/audio/opl/opl.h"
#include "platform/audio/pitch.h"
#include "platform/audio/samples.h"
#include "platform/audio/mixer.h"
#include "platform/audio/sequencer.h"
#include "platform/audio/patches.h"
#include "platform/audio/ail.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>


/* ---- test_opl.c ---- */

static s16 buf_a[2048], buf_b[2048];

/* The driver's note setup: waveform enable, operator/channel registers with the
 * output-enable write 0xC0 = patch | 0x30, then key-on. `with_105` inserts the
 * capture's OPL3-mode enable (0x105 = 0x01) right after 0x01 = 0x20. */
static void note_setup(int with_105, s16 *out)
{
    opl_reset();
    opl_write(0x01, 0x20);
    if (with_105)
        opl_write(0x105, 0x01);
    opl_write(0x20, 0x01);
    opl_write(0x40, 0x10);
    opl_write(0x60, 0xF0);
    opl_write(0x80, 0x77);
    opl_write(0xE0, 0x00);
    opl_write(0xC0, 0x30);
    opl_write(0xA0, 0x98);
    opl_write(0xB0, 0x31);
    opl_render(out, 1024);
}

int test_opl(void)
{
    int before = g_failures;

    opl_reset();
    opl_render(buf_a, 1024);
    int silent = 1;
    for (int i = 0; i < 2048; i++)
        if (buf_a[i] != 0)
            silent = 0;
    CHECK(silent, "a reset core with no register writes renders silence");

    opl_reset();
    opl_write(0x20, 0x01);
    opl_write(0x40, 0x10);
    opl_write(0x60, 0xF0);
    opl_write(0x80, 0x77);
    opl_write(0xA0, 0x98);
    opl_write(0xB0, 0x31);
    opl_render(buf_a, 1024);
    int any = 0;
    for (int i = 0; i < 2048; i++)
        if (buf_a[i] != 0)
            any = 1;
    CHECK(any, "a key-on write sequence produces audio");

    opl_reset();
    opl_write(0x20, 0x01);
    opl_write(0x40, 0x10);
    opl_write(0x60, 0xF0);
    opl_write(0x80, 0x77);
    opl_write(0xA0, 0x98);
    opl_write(0xB0, 0x31);
    opl_render(buf_b, 1024);
    CHECK(memcmp(buf_a, buf_b, sizeof buf_a) == 0, "the core is deterministic");

    /* Regression: the spec once claimed 0x105 = 0x01 silences the core. It does
     * not, given the 0xC0 output-enable write every real note setup has; the
     * old probe omitted that write and attributed the resulting OPL3-mode
     * silence to 0x105. */
    note_setup(0, buf_a);
    note_setup(1, buf_b);
    {
        int n = 0;
        for (int i = 0; i < 2048; i++)
            if (buf_b[i] != 0)
                n++;
        CHECK(n > 0, "0x105 = 1 with 0xC0 enable still renders audio");
        CHECK(memcmp(buf_a, buf_b, sizeof buf_a) == 0,
              "0x105 = 1 is output-neutral given the 0xC0 enable");
    }

    return g_failures - before;
}

/* ---- test_pitch.c ---- */

int test_pitch(void)
{
    int before = g_failures;

    /* Capture-pinned anchors from the driver routine. b0 is the payload
     * without the key-on bit; a key-on ORs 0x20. Note 84 (melodic) and 79
     * (the negative-fnum carry) land on block 5; percussion base 54 on
     * block 2. */
    {
        u8 a0, b0;
        pitch_lookup(84, 0, &a0, &b0);
        CHECK_EQ_INT(a0, 0xB2);
        CHECK_EQ_INT(b0, 0x16);
        pitch_lookup(79, 0, &a0, &b0);
        CHECK_EQ_INT(a0, 0x05);
        CHECK_EQ_INT(b0, 0x16);
        pitch_lookup(54, 0, &a0, &b0);
        CHECK_EQ_INT(a0, 0xCF);
        CHECK_EQ_INT(b0, 0x0B);
    }

    /* A centred wheel (0x2000) is zero at any scale. */
    CHECK_EQ_INT(pitch_bend_of(0x2000, 12), 0);
    /* A full-up wheel with scale 1 is (0x1fff >> 5) * 1 = 0xff. */
    CHECK_EQ_INT(pitch_bend_of(0x3fff, 1), 0xFF);
    /* The product is 16-bit: the driver's imul result is used from `ax` only,
     * so 255 * 255 wraps to the signed 16-bit value. */
    CHECK_EQ_INT(pitch_bend_of(0x3fff, 255), -511);
    /* The 0x3646/0x3648 additions are 16-bit too, so the sum wraps before the
     * 0x364e shift; 127 folds to bx 91, and 91*256 + 8 + 16000 overflows. */
    {
        u8 a0, b0;
        pitch_lookup(127, 16000, &a0, &b0);
        CHECK_EQ_INT(a0, 0xDA);
        CHECK_EQ_INT(b0, 0x01);
    }

    /* Folded low indices 0-6 with a centred wheel compute block -1 (one below
     * the lowest legal block), so the `block < 0` carry runs (block++ and
     * v >>= 1): index 0's v 0x02B2 halves to 0x0159, index 6's 0x03CF to
     * 0x01E7, both lifted to block 0. */
    {
        u8 a0, b0;
        pitch_lookup(0, 0, &a0, &b0);
        CHECK_EQ_INT(a0, 0x59);
        CHECK_EQ_INT(b0, 0x01);
        pitch_lookup(6, 0, &a0, &b0);
        CHECK_EQ_INT(a0, 0xE7);
        CHECK_EQ_INT(b0, 0x01);
    }
    /* A negative bend drives `ax` below 0; the 0xc0 fold wraps it (it is not
     * a clamp), so the index lands above centre. Index 0, bend -1000: ax -62
     * -> 130. */
    {
        u8 a0, b0;
        pitch_lookup(0, -1000, &a0, &b0);
        CHECK_EQ_INT(a0, 0x27);
        CHECK_EQ_INT(b0, 0x02);
    }

    return g_failures - before;
}

/* ---- test_samples.c ---- */

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

/* ---- test_mixer.c ---- */

#define FRAMES 64

static s16 mixer_out[2 * FRAMES];

/* Voice owners for the per-voice stop test; any distinct addresses work. */
static int owner_a, owner_b;

static int mixer_all_zero(const s16 *b, int n)
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
    mixer_render(mixer_out, FRAMES, 44100);
    CHECK(mixer_all_zero(mixer_out, 2 * FRAMES), "reset mixer with no voices is exact silence");

    /* One voice: nonzero output. Tone at 1 kHz-ish, volume 128 (~0.5), looping,
     * rate == out_rate so no resampling is involved. */
    static s16 tone[8] = { 1000, -1000, 1000, -1000, 1000, -1000, 1000, -1000 };
    mixer_reset();
    mixer_add_sample(tone, 8, 44100, 128, 1, &owner_a);
    mixer_render(mixer_out, FRAMES, 44100);
    CHECK(!mixer_all_zero(mixer_out, 2 * FRAMES), "one voice produces non-silence");

    /* Two voices louder than one. Volume 128 with amplitude 1000 gives +/-500
     * per voice, so two voices sum to +/-1000: well inside s16, the comparison
     * cannot be explained by clipping. */
    long one = sum_abs(mixer_out, 2 * FRAMES);
    mixer_reset();
    mixer_add_sample(tone, 8, 44100, 128, 1, &owner_a);
    mixer_add_sample(tone, 8, 44100, 128, 1, &owner_a);
    mixer_render(mixer_out, FRAMES, 44100);
    long two = sum_abs(mixer_out, 2 * FRAMES);
    CHECK(two > one, "two voices together are louder than one");

    /* Stopped voices contribute nothing. */
    mixer_reset();
    mixer_add_sample(tone, 8, 44100, 256, 1, &owner_a);
    mixer_stop_samples();
    mixer_render(mixer_out, FRAMES, 44100);
    CHECK(mixer_all_zero(mixer_out, 2 * FRAMES), "stop_samples returns output to silence");

    /* Clipping saturates, never wraps. Four voices at unity of +/-30000 sum to
     * +/-120000, past both s16 limits. Every sample must be exactly a limit and
     * in the direction of the input sign. */
    static s16 loud[2] = { 30000, -30000 };
    mixer_reset();
    for (int i = 0; i < 4; i++)
        mixer_add_sample(loud, 2, 44100, 256, 1, &owner_a);
    mixer_render(mixer_out, FRAMES, 44100);
    int saw_pos = 0, saw_neg = 0, outside = 0;
    for (int i = 0; i < 2 * FRAMES; i++) {
        if (mixer_out[i] == 32767) saw_pos = 1;
        else if (mixer_out[i] == -32768) saw_neg = 1;
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
    mixer_render(mixer_out, 6, 44100);
    CHECK_EQ_INT(mixer_out[0], 100);
    CHECK_EQ_INT(mixer_out[1], 100);
    CHECK_EQ_INT(mixer_out[2], 300);
    CHECK_EQ_INT(mixer_out[3], 300);
    CHECK_EQ_INT(mixer_out[4], 500);
    CHECK_EQ_INT(mixer_out[5], 500);
    CHECK_EQ_INT(mixer_out[6], 0);
    CHECK_EQ_INT(mixer_out[7], 0);
    CHECK_EQ_INT(mixer_out[8], 0);
    CHECK_EQ_INT(mixer_out[9], 0);
    CHECK_EQ_INT(mixer_out[10], 0);
    CHECK_EQ_INT(mixer_out[11], 0);

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
        mixer_render(mixer_out, FRAMES, 44100);
        CHECK(!mixer_all_zero(mixer_out, 2 * FRAMES),
              "stopping one owner leaves the other voice sounding");

        mixer_stop_sample(&owner_a);   /* already stopped: stale-safe */
        mixer_render(mixer_out, FRAMES, 44100);
        CHECK(!mixer_all_zero(mixer_out, 2 * FRAMES),
              "a stop for an inactive owner leaves the live voice alone");

        mixer_stop_sample(&owner_b);
        mixer_render(mixer_out, FRAMES, 44100);
        CHECK(mixer_all_zero(mixer_out, 2 * FRAMES),
              "stopping the last sample voice is exact silence");
    }

    /* A rate whose exact 16.16 step is 2^32 must saturate, not wrap: wrapping
     * gives step 0, pinning the voice on its first sample forever. rate 2^17 at
     * out_rate 2 gives step 2^32, so a u32 truncation would make it 0. */
    {
        static s16 pcm[4] = { 100, 200, 300, 400 };
        mixer_reset();
        CHECK(mixer_add_sample(pcm, 4, 1 << 17, 256, 0, &owner_a) == 1,
              "a voice starts before the wrap check");
        CHECK_EQ_INT(mixer_active_voices(), 1);
        mixer_render(mixer_out, 8, 2);
        CHECK_EQ_INT(mixer_active_voices(), 0);
    }

    return g_failures - before;
}

/* ---- test_sequencer.c ---- */

/* Builds a minimal FORM/XMID/EVNT XMI bank around `ev`, so the halt paths can
 * be exercised without assets. Layout matches what seq_load walks. */
static u32 wr_be32(u8 *p, u32 v)
{
    p[0] = (u8)(v >> 24); p[1] = (u8)(v >> 16); p[2] = (u8)(v >> 8); p[3] = (u8)v;
    return 4;
}

static u32 build_xmi(u8 *buf, const u8 *ev, u32 n)
{
    u32 chunk = 8 + n + (n & 1u);
    u32 fsz = 4 + chunk;
    u32 len = 8 + fsz;

    buf[0] = 'F'; buf[1] = 'O'; buf[2] = 'R'; buf[3] = 'M';
    wr_be32(buf + 4, fsz);
    buf[8] = 'X'; buf[9] = 'M'; buf[10] = 'I'; buf[11] = 'D';
    buf[12] = 'E'; buf[13] = 'V'; buf[14] = 'N'; buf[15] = 'T';
    wr_be32(buf + 16, n);
    for (u32 i = 0; i < n; i++)
        buf[20 + i] = ev[i];
    if (n & 1u)
        buf[20 + n] = 0;
    return len;
}

/* The shipped title music bank: a FORM XDIR / CAT XMID container at file offset
 * 221446 (0x36106) in S16TITLE.GRA (port/spec/audio.md "Data locations"). The
 * XMID FORM it holds is at 0x36128. This is a shallow smoke test: it proves the
 * sequencer accepts the real bank, ticks, and drives the OPL core; the
 * byte-exact comparison against the capture is Task 9's job. */
#define TITLE_GRA "data/game/C/S16TITLE.GRA"
#define TITLE_XMI_OFF 221446u
#define FAT_OPL "data/game/C/FAT.OPL"

static u8 *read_file(const char *path, u32 *len)
{
    FILE *f = fopen(path, "rb");
    long n;
    u8 *buf;

    *len = 0;
    if (!f)
        return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    n = ftell(f);
    if (n <= 0) { fclose(f); return NULL; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }
    buf = (u8 *)malloc((size_t)n);
    if (!buf) { fclose(f); return NULL; }
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) { free(buf); fclose(f); return NULL; }
    fclose(f);
    *len = (u32)n;
    return buf;
}

static int any_nonzero(const s16 *b, int n)
{
    for (int i = 0; i < n; i++)
        if (b[i] != 0)
            return 1;
    return 0;
}

/* --- Task 9 oracle comparison ------------------------------------------- */

#define OPL_SEQ_PY "tools/opl_seq.py"
#define OPL_TRACE_PY "tools/opl_trace.py"
#define CAPTURE_DRO "data/audio-captures/prage_000.dro"

/* One normalised (tick, reg, value, attr) write. `attr` is the per-write
 * attribution the OPL trace seam records (the MIDI channel a voice write
 * belongs to, or 0xFF when unattributed). Streams decoded from a file carry no
 * attribution, so read_ev_stream leaves it 0xFF. */
typedef struct {
    u32 tick;
    u16 reg;
    u8 val;
    u8 attr;
} ev_t;

/* Runs `cmd` and parses its "tick reg value" lines (opl_seq.py / opl_trace.py
 * share this format). Fills up to `cap` events; `total` always gets the full
 * count. Returns 0 on success. The third-party streams carry no attribution;
 * ev_t.attr stays 0xFF. */
static int read_ev_stream(const char *cmd, ev_t *out, int cap, u32 *total)
{
    FILE *p = popen(cmd, "r");
    char line[128];
    int n = 0;

    *total = 0;
    if (p == NULL)
        return -1;
    while (fgets(line, sizeof line, p) != NULL) {
        unsigned t, r, v;
        if (sscanf(line, "%u %x %x", &t, &r, &v) != 3)
            continue;
        if (n < cap) {
            out[n].tick = t;
            out[n].reg = (u16)r;
            out[n].val = (u8)v;
            out[n].attr = 0xFF;
        }
        n++;
    }
    (void)pclose(p);
    *total = (u32)n;
    return 0;
}

static int ev_eq(const ev_t *a, const ev_t *b)
{
    return a->tick == b->tick && a->reg == b->reg && a->val == b->val;
}

/* Registers the port deliberately does not reproduce against the capture
 * (port/spec/audio.md "Known capture divergences"). `attr` is the write's
 * per-MIDI-channel attribution from the OPL trace seam (0xFF unattributed).
 *
 * The carrier-TL law IS derived and implemented: `att = ((~p10) & 0x3f) * V /
 * 0x7f` with `V = scale7(scale7(cc7_eff, cc11), VELCURVE[vel >> 3])`, where
 * `cc7_eff = (seqvol * cc7) / 0x7f` is the engine's sequence-volume scaling
 * (prage.c:49121). It is exact on every non-residual channel (630/630 steady
 * carrier rows) and the ungated modulator path is verbatim (664/664).
 *
 * The residual is the engine's per-channel volume on MIDI channels 1 and 4 —
 * the only channels whose CC7 moves. Round 3 proved it not pinnable: the engine
 * has one sequence volume and one timer tick, yet ch4 and ch9 written at the
 * same tick imply 0x50 vs 0x54; the only per-channel volume array writer
 * (ctrl 83) is never sent. Reproducing those rows needs a fitted constant,
 * which this repo forbids, so the exclusion names exactly those rows instead of
 * the whole TL family: carrier-TL registers (low byte, both banks:
 * 0x43/0x44/0x45/0x4B/0x4C/0x4D/0x53/0x54/0x55) on channel 1 or 4 only.
 * Modulator TL (0x40..0x42, 0x48..0x4A, 0x50..0x52) and carrier TL on every
 * other channel are compared. 0xBD stays excluded unconditionally. */
static int documented_excluded(u16 reg, u8 attr)
{
    u8 lo = (u8)(reg & 0xFF);

    if (lo == 0xBD)
        return 1;
    switch (lo) {
    case 0x43: case 0x44: case 0x45:
    case 0x4B: case 0x4C: case 0x4D:
    case 0x53: case 0x54: case 0x55:
        return attr == 1 || attr == 4;
    default:
        return 0;
    }
}

/* The DRO capture records a register write only when it changes that register's
 * value: every captured register's value sequence has no two consecutive equal
 * values (0x20/0x21/0x24/0x41/0x120/0x122/0x125 all measured
 * consecutive-same=0). The shipped SBPRO2.MDI writes every family
 * unconditionally (Ghidra: FUN_0000_3184 gates on mask 0xf9, writer
 * FUN_0000_2ad6 — no shadow anywhere), so the port is faithful to the bytes and
 * the capture is the lossy side: DOSBox-X's capture path drops an unchanged
 * write. The oracle therefore compares state-change trajectories rather than
 * write counts — both streams drop a write whose value equals the last value
 * kept for that register, from the OPL power-on value 0. The recording artefact
 * cancels on both sides; a real value or ordering divergence still shows.
 * Indexed by the full 9-bit register so the second OPL2 bank (0x1E0-0x1F5) does
 * not alias the first (0xE0-0xF5). */
#define OPL_SHADOW_REGS 0x200

/* Runs the C sequencer for `ticks` ticks, tagging each write with the tick it
 * was made on (seq_start's writes are tick 0). */
static int capture_c_stream(ev_t *out, int cap, u32 ticks)
{
    u32 prev, tick;
    int n = 0;

    opl_reset();
    seq_start();
    for (u32 k = 0; k < opl_write_count() && n < cap; k++) {
        out[n].tick = 0;
        out[n].reg = opl_trace_reg(k);
        out[n].val = opl_trace_val(k);
        out[n].attr = opl_trace_attr(k);
        n++;
    }
    prev = opl_write_count();
    for (tick = 1; tick <= ticks; tick++) {
        seq_tick();
        for (u32 k = prev; k < opl_write_count() && n < cap; k++) {
            out[n].tick = tick;
            out[n].reg = opl_trace_reg(k);
            out[n].val = opl_trace_val(k);
            out[n].attr = opl_trace_attr(k);
            n++;
        }
        prev = opl_write_count();
    }
    return n;
}

int test_sequencer(void)
{
    int before = g_failures;
    u8 *gra = NULL, *fat = NULL;
    u32 gra_len = 0, fat_len = 0;
    const u8 *xmi;
    u32 xmi_len;

    /* Rejections that need no asset: NULL/empty and a non-XMIDI buffer. These
     * also hold before any successful load, which must stay rejected. */
    {
        static const u8 junk[24] = { 'N', 'O', 'T', 'A', 'M', 'U', 'S', 'I',
                                     'C', 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
        CHECK_EQ_INT(seq_load(NULL, 0), 0);
        CHECK_EQ_INT(seq_load(junk, sizeof junk), 0);
        CHECK_EQ_INT(seq_load(NULL, 100), 0);
    }
    /* A failed load must leave the loaded bank unchanged. The count is not
     * necessarily 0 here: the game's own init path (game_audio_init) loads
     * FAT.OPL, and test_flow runs before this test. */
    {
        int was = patches_count();
        CHECK_EQ_INT(patches_load(NULL, 0), 0);
        {
            static const u8 junk[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
            CHECK_EQ_INT(patches_load(junk, sizeof junk), 0);
        }
        CHECK_EQ_INT(patches_count(), was);
    }

    /* The narrowed capture-exclusion predicate (no assets): the carrier-TL
     * registers are excluded only for the residual channels 1 and 4; the
     * modulator TL registers are never excluded, nor is carrier TL under any
     * other attribution. 0xBD is excluded unconditionally. */
    {
        static const u16 ctl[9] = { 0x43, 0x44, 0x45, 0x4B, 0x4C, 0x4D, 0x53, 0x54, 0x55 };
        static const u16 mtl[9] = { 0x40, 0x41, 0x42, 0x48, 0x49, 0x4A, 0x50, 0x51, 0x52 };
        static const u8 attrs[5] = { 0, 1, 2, 4, 0xFF };

        CHECK_EQ_INT(documented_excluded(0xBD, 0xFF), 1);
        CHECK_EQ_INT(documented_excluded(0xBD, 0), 1);
        CHECK_EQ_INT(documented_excluded(0x1BD, 4), 1);
        for (int b = 0; b < 2; b++) {
            u16 bank = (u16)(b ? 0x100 : 0);
            for (int i = 0; i < 9; i++) {
                for (int a = 0; a < 5; a++) {
                    int want = (attrs[a] == 1 || attrs[a] == 4);
                    CHECK_EQ_INT(documented_excluded((u16)(ctl[i] | bank), attrs[a]), want);
                }
                for (int a = 0; a < 5; a++)
                    CHECK_EQ_INT(documented_excluded((u16)(mtl[i] | bank), attrs[a]), 0);
            }
        }
    }

    /* 0. Halt invariants (synthetic banks, no assets): every stop path must
     *    release keyed voices, so seq_active_track() reaches 0 and no further
     *    OPL writes follow. Regression: the error paths only cleared the
     *    playing flag, leaving channels keyed on forever. */
    {
        u8 bank[64];
        u32 len;

        /* 0a. A parse overrun (truncated 0x90) after a note keys off. */
        {
            static const u8 ev[] = { 0x90, 0x30, 0x40, 0x7f, 0x05, 0x90, 0x33 };
            opl_reset();
            len = build_xmi(bank, ev, sizeof ev);
            CHECK_EQ_INT(seq_load(bank, len), 1);
            seq_start();
            seq_tick();
            CHECK_EQ_INT(seq_active_track(), 1);
            for (int i = 0; i < 10 && seq_active_track() > 0; i++)
                seq_tick();
            CHECK_EQ_INT(seq_active_track(), 0);
            {
                u32 w = opl_write_count();
                for (int i = 0; i < 10; i++)
                    seq_tick();
                CHECK_EQ_INT(opl_write_count(), w);
            }
        }

        /* 0b. An unknown status byte after a note keys off. */
        {
            static const u8 ev[] = { 0x90, 0x30, 0x40, 0x7f, 0x05, 0xf1 };
            opl_reset();
            len = build_xmi(bank, ev, sizeof ev);
            CHECK_EQ_INT(seq_load(bank, len), 1);
            seq_start();
            seq_tick();
            CHECK_EQ_INT(seq_active_track(), 1);
            for (int i = 0; i < 10 && seq_active_track() > 0; i++)
                seq_tick();
            CHECK_EQ_INT(seq_active_track(), 0);
        }

        /* 0c. Loading a new bank releases the previous bank's keyed voices and
         *     leaves no stale release for the next tick. */
        {
            static const u8 a[] = { 0x90, 0x30, 0x40, 0x7f, 0x05 };
            static const u8 b[] = { 0x20, 0x90, 0x40, 0x40, 0x7f, 0x05 };
            u8 bank_b[64];
            u32 len_b;

            opl_reset();
            len = build_xmi(bank, a, sizeof a);
            len_b = build_xmi(bank_b, b, sizeof b);
            CHECK_EQ_INT(seq_load(bank, len), 1);
            seq_start();
            seq_tick();
            CHECK_EQ_INT(seq_active_track(), 1);
            {
                u32 before = opl_write_count();
                CHECK_EQ_INT(seq_load(bank_b, len_b), 1);
                CHECK_EQ_INT(seq_active_track(), 0);
                CHECK(opl_write_count() > before, "load keys off previous voices");
            }
            {
                u32 w = opl_write_count();
                seq_tick();
                seq_tick();
                CHECK_EQ_INT(opl_write_count(), w);
                CHECK_EQ_INT(seq_active_track(), 0);
            }
        }

        /* 0d. Restarting while a note sounds releases it too. */
        {
            static const u8 a[] = { 0x90, 0x30, 0x40, 0x7f, 0x05 };
            opl_reset();
            len = build_xmi(bank, a, sizeof a);
            CHECK_EQ_INT(seq_load(bank, len), 1);
            seq_start();
            seq_tick();
            CHECK_EQ_INT(seq_active_track(), 1);
            {
                u32 before = opl_write_count();
                seq_start();
                CHECK_EQ_INT(seq_active_track(), 0);
                CHECK(opl_write_count() > before, "restart keys off sounding notes");
            }
        }
        /* 0e. Controllers: a bend re-applies only the A0/B0 family, for active
         *     voices of that channel only, and carries the bent value, not the
         *     centred key-on value. A bend on a channel with no active voice
         *     writes nothing. Each zero-delta event group is processed on its
         *     own tick, so advance one tick per group. */
        {
            static const u8 ev[] = { 0xB0, 0x06, 0x02,                  /* bend scale ch0 */
                                     0x00, 0x90, 0x30, 0x40, 0x7f, 0x00,/* note on ch0 */
                                     0xE0, 0x00, 0x30,                  /* bend ch0 */
                                     0x00, 0xE1, 0x00, 0x30,            /* bend ch1 */
                                     0x00, 0xB0, 0x07, 0x70 };          /* volume ch0 */
            u32 start, i;
            u16 key_a0 = 0, bend_a0 = 0;
            int saw_key = 0, saw_bend = 0, saw_tl = 0;

            opl_reset();
            len = build_xmi(bank, ev, sizeof ev);
            CHECK_EQ_INT(seq_load(bank, len), 1);
            seq_start();
            seq_tick();                       /* tick 1: bend scale */
            start = opl_write_count();
            seq_tick();                       /* tick 2: note on, centred wheel */
            for (i = start; i < opl_write_count(); i++)
                if ((opl_trace_reg(i) & 0xf0) == 0xA0) {
                    key_a0 = opl_trace_val(i);
                    saw_key = 1;
                }
            CHECK(saw_key, "key-on wrote the frequency low byte");
            start = opl_write_count();
            seq_tick();                       /* tick 3: bend ch0 re-applies */
            for (i = start; i < opl_write_count(); i++)
                if ((opl_trace_reg(i) & 0xf0) == 0xA0) {
                    bend_a0 = opl_trace_val(i);
                    saw_bend = 1;
                }
            CHECK(saw_bend, "bend re-applied the frequency family");
            CHECK(bend_a0 != key_a0, "bend carried the bent value, not the centred one");
            start = opl_write_count();
            seq_tick();                       /* tick 4: bend ch1, no active voice */
            CHECK_EQ_INT(opl_write_count(), start);
            start = opl_write_count();
            seq_tick();                       /* tick 5: volume ch0 re-applies TL */
            for (i = start; i < opl_write_count(); i++)
                if ((opl_trace_reg(i) & 0xf0) == 0x40) saw_tl = 1;
            CHECK(saw_tl, "volume re-applied the TL family");
        }

        /* 0f. A note keyed after a bend on its channel sounds bent: key-on
         *     reaches the frequency routine (0x3085 -> 0x3184), which reads the
         *     live wheel from the voice's channel ([si+0x14c1]). */
        {
            static const u8 ev[] = { 0xB0, 0x06, 0x02,                   /* bend scale ch0 */
                                     0x00, 0x90, 0x30, 0x40, 0x7f, 0x00, /* note on ch0 */
                                     0xE0, 0x00, 0x30,                   /* bend ch0 */
                                     0x00, 0x90, 0x30, 0x40, 0x7f };     /* note on ch0 again */
            u32 start, i;
            u16 cent_a0 = 0, bend_a0 = 0, new_a0 = 0;
            int saw_cent = 0, saw_bend = 0, saw_new = 0;

            opl_reset();
            len = build_xmi(bank, ev, sizeof ev);
            CHECK_EQ_INT(seq_load(bank, len), 1);
            seq_start();
            seq_tick();                       /* tick 1: bend scale */
            start = opl_write_count();
            seq_tick();                       /* tick 2: note on, centred wheel */
            for (i = start; i < opl_write_count(); i++)
                if ((opl_trace_reg(i) & 0xf0) == 0xA0) {
                    cent_a0 = opl_trace_val(i);
                    saw_cent = 1;
                }
            start = opl_write_count();
            seq_tick();                       /* tick 3: bend ch0 */
            for (i = start; i < opl_write_count(); i++)
                if ((opl_trace_reg(i) & 0xf0) == 0xA0) {
                    bend_a0 = opl_trace_val(i);
                    saw_bend = 1;
                }
            start = opl_write_count();
            seq_tick();                       /* tick 4: second note on ch0 */
            for (i = start; i < opl_write_count(); i++)
                if ((opl_trace_reg(i) & 0xf0) == 0xA0) {
                    new_a0 = opl_trace_val(i);
                    saw_new = 1;
                }
            CHECK(saw_cent && saw_bend && saw_new, "key/bend/key all wrote a frequency low byte");
            CHECK(new_a0 == bend_a0, "a note keyed after the bend uses the live wheel");
            CHECK(new_a0 != cent_a0, "the bent key-on differs from the centred one");
        }
    }

    gra = read_file(TITLE_GRA, &gra_len);
    fat = read_file(FAT_OPL, &fat_len);
    if (gra == NULL || fat == NULL || gra_len <= TITLE_XMI_OFF) {
        free(gra);
        free(fat);
        if (getenv("PR_ORACLE_REQUIRED")) {
            CHECK(0, "PR_ORACLE_REQUIRED=1 but S16TITLE.GRA/FAT.OPL is missing");
        } else {
            printf("SKIP sequencer real-data checks — including the governing "
                   "C-vs-Python byte gate (need untracked " TITLE_GRA
                   " @%u and " FAT_OPL ")\n", TITLE_XMI_OFF);
        }
        return g_failures - before;
    }

    xmi = gra + TITLE_XMI_OFF;
    xmi_len = gra_len - TITLE_XMI_OFF;

    /* The recorded offset really is the XMI bank container. */
    CHECK(memcmp(xmi, "FORM", 4) == 0, "recorded offset is a FORM chunk");

    /* The music path loads the patch bank first (AIL init), so the sequencer's
     * program changes resolve. Without it a key-on carries no operator setup. */
    CHECK_EQ_INT(patches_load(fat, fat_len), 1);
    CHECK_EQ_INT(patches_count(), 181);

    /* 1. Real bank loads; nothing is sounding before start. */
    CHECK_EQ_INT(seq_load(xmi, xmi_len), 1);
    CHECK_EQ_INT(seq_active_track(), 0);

    /* 2. Truncated forms of the same bank are rejected and leave the loaded
     *    bank playable (the loader keeps its state on failure). */
    CHECK_EQ_INT(seq_load(xmi, 32), 0);
    CHECK_EQ_INT(seq_load(xmi, 64), 0);

    /* 3. Ticking the real bank keys notes on and reaches the OPL core. The OPL
     *    core advances only when rendered, so render a little each tick and
     *    watch for non-silence: it can appear only if register writes landed. */
    mixer_reset();
    seq_start();
    {
        static s16 out[2 * 64];
        int heard = 0, saw_active = 0;
        for (int i = 0; i < 400; i++) {
            seq_tick();
            if (seq_active_track() > 0)
                saw_active = 1;
            for (int k = 0; k < 2 * 64; k++)
                out[k] = 0;
            mixer_render(out, 64, 44100);
            if (any_nonzero(out, 2 * 64))
                heard = 1;
        }
        CHECK(saw_active, "ticking the title bank keys notes on");
        CHECK(heard, "sequencer output reaches the OPL core");
    }

    /* 4. Stop silences: no active voices and ticking further stays silent. */
    seq_stop();
    CHECK_EQ_INT(seq_active_track(), 0);
    for (int i = 0; i < 200; i++)
        seq_tick();
    CHECK_EQ_INT(seq_active_track(), 0);

    /* 5. FAT.OPL decodes (loaded above): melodic and percussion keys resolve
     *    to their payloads, absent keys do not. */
    {
        const u8 *mel = patches_lookup(PATCH_KEY(PATCH_BANK_MELODIC, 0));
        const u8 *drum = patches_lookup(PATCH_KEY(PATCH_BANK_PERCUSSION, 0x2d));
        CHECK(mel != NULL && mel[0] == 0x0e, "melodic patch payload decodes");
        CHECK(drum != NULL && drum[0] == 0x0e, "percussion patch payload decodes");
        CHECK(patches_lookup(0x1234u) == NULL, "absent patch key resolves to NULL");
    }

    /* 6. A truncated bank is rejected without discarding the loaded one. */
    CHECK_EQ_INT(patches_load(fat, 100), 0);
    CHECK_EQ_INT(patches_count(), 181);

    /* 7. Task 9 oracle: the C register stream must equal tools/opl_seq.py's
     *    byte for byte — same tick, register, value and order. Governing
     *    oracle, no tolerance. */
    {
        static ev_t c_ev[OPL_TRACE_MAX];
        static ev_t py_ev[OPL_TRACE_MAX];
        static char cmd[256];
        u32 py_total = 0;
        int c_n;

        CHECK_EQ_INT(patches_load(fat, fat_len), 1);
        CHECK_EQ_INT(seq_load(xmi, xmi_len), 1);
        /* The capture ran with the engine's AIL sequence volume below the image
         * default; 0x54 is the value the steady-CC7 channels imply. Both the C
         * stream and the Python oracle are driven with the same explicit input
         * so the byte gate compares the sequencer, not the volume setting. */
        seq_set_sequence_volume(0x54);
        c_n = capture_c_stream(c_ev, (int)OPL_TRACE_MAX, 4096);
        CHECK(!opl_trace_overflow(), "C register stream fits the trace seam");

        snprintf(cmd, sizeof cmd, "python3 %s %s --trace --seqvol %u",
                 OPL_SEQ_PY, TITLE_GRA, 0x54u);
        CHECK_EQ_INT(read_ev_stream(cmd, py_ev, (int)OPL_TRACE_MAX, &py_total), 0);
        CHECK_EQ_INT((long)c_n, (long)py_total);
        if (c_n == (int)py_total) {
            for (int i = 0; i < c_n; i++) {
                if (!ev_eq(&c_ev[i], &py_ev[i])) {
                    printf("ORACLE C-vs-Python first difference at write %d: "
                           "C tick=%u reg=%#04x val=%#04x, python tick=%u reg=%#04x val=%#04x\n",
                           i, c_ev[i].tick, c_ev[i].reg, c_ev[i].val,
                           py_ev[i].tick, py_ev[i].reg, py_ev[i].val);
                    CHECK(0, "C register stream equals the Python oracle byte-for-byte");
                    break;
                }
            }
        } else {
            printf("ORACLE C-vs-Python count differs: C=%d python=%u\n", c_n, py_total);
        }
        if (c_n == (int)py_total) {
            int bad = 0;
            for (int i = 0; i < c_n; i++)
                if (!ev_eq(&c_ev[i], &py_ev[i]))
                    bad = 1;
            if (!bad)
                printf("oracle C-vs-Python: %d writes byte-exact\n", c_n);
        }

        /* 7b. Percussion note -> fnum (spec divergence 6). The driver's
         *     note-on path (SBPRO2.MDI 0x35fa-0x36a6) builds the fnum index
         *     from the patch base byte ([di+2], stored at 0x3aac-0x3ac1), not
         *     from the MIDI note: melodic adds the base to the note, percussion
         *     uses the base alone. The title's first key-on is MIDI 47 on
         *     channel 9; its 0x7F-bank patch base is 54, so the capture keys
         *     block 2 fnum 0x3CF (0xA0=0xCF, 0xB0=0x2B). The melodic
         *     pitch_lookup(47, 0) would give block 2 fnum 0x28B (0xB0=0x2A), so
         *     this fails before the fix. */
        {
            const u8 *drum = patches_lookup(PATCH_KEY(PATCH_BANK_PERCUSSION, 47));
            int fk = -1;
            CHECK(drum != NULL && drum[2] == 54,
                  "percussion note 47 patch base byte is 54");
            for (int i = 0; i < c_n; i++) {
                if (c_ev[i].reg >= 0xB0 && c_ev[i].reg <= 0xB8 &&
                    (c_ev[i].val & 0x20)) {
                    fk = i;
                    break;
                }
            }
            CHECK(fk > 0, "the title stream keys its first note on");
            if (fk > 0) {
                CHECK_EQ_INT(c_ev[fk].reg, 0xB0);
                CHECK_EQ_INT(c_ev[fk].val, 0x2B);
                CHECK_EQ_INT(c_ev[fk - 1].reg, 0xA0);
                CHECK_EQ_INT(c_ev[fk - 1].val, 0xCF);
            }
        }

        /* 7c. Channel assignment (spec divergence 7). The driver's melodic
         *     allocator (SBPRO2.MDI 0x3095-0x30d8) walks a rotation cursor
         *     [0x1408] over the 18 slot-owner bytes [0x1a49]: each note takes
         *     the next free slot after the cursor (wrapping at 18), not the
         *     lowest free channel, and freeing a voice does not move the cursor
         *     back. The capture reflects it: its first four key-ons are OPL
         *     ch0, ch1, ch2, ch3. The port must not reuse ch0. */
        {
            int got[4];
            int n = 0;
            for (int i = 0; i < c_n && n < 4; i++) {
                if (c_ev[i].reg >= 0xB0 && c_ev[i].reg <= 0xB8 &&
                    (c_ev[i].val & 0x20)) {
                    got[n] = (int)(c_ev[i].reg - 0xB0);
                    n++;
                }
            }
            CHECK_EQ_INT(n, 4);
            for (int i = 0; i < n; i++)
                CHECK_EQ_INT(got[i], i);
        }

        /* 8. Capture oracle (informational). The capture is the real driver,
         *    which the port reconstructs rather than reproduces: its
         *    cached-state init block and per-patch operator application are
         *    not modelled. Both streams are reduced by the
         *    same rule: drop everything before the stream's first key-on
         *    (0xB0..0xB8 with the key bit), map capture ms -> port tick at
         *    120 Hz, drop the documented-excluded registers, and drop a write
         *    whose value equals the last kept value for that register (the
         *    capture records write-on-change only — see the note above
         *    documented_excluded). The driver
         *    folds its tick-0 reset and the first note's patch into one block
         *    (spec divergence 4), so the first note's operator/C0/A0 preamble
         *    has no separately comparable capture writes; anchoring both
         *    streams at the first key-on discards it symmetrically instead of
         *    discarding it on the capture only. Print the first remaining
         *    difference for the Task 9 report. It does not fail the suite on a
         *    known divergence. */
        {
            static ev_t cap_ev[OPL_TRACE_MAX];
            u32 cap_total = 0, w = 0, pyi = 0;
            u32 first_key = 0, first_key_c = 0;
            u8 cap_last[OPL_SHADOW_REGS] = { 0 };
            u8 anchor_state[OPL_SHADOW_REGS] = { 0 };
            int diff = -1;

            snprintf(cmd, sizeof cmd, "python3 %s %s", OPL_TRACE_PY, CAPTURE_DRO);
            if (read_ev_stream(cmd, cap_ev, (int)OPL_TRACE_MAX, &cap_total) != 0 || cap_total == 0) {
                /* Two causes read alike here: the capture may be absent, or it
                 * is present but nothing decoded (no python3 on PATH, or
                 * opl_trace.py failed). Say which, so the reader is not sent to
                 * the wrong place. */
                FILE *cf = fopen(CAPTURE_DRO, "rb");
                if (cf == NULL) {
                    printf("SKIP capture oracle — capture file missing: %s "
                           "(see \"Recorded capture\" in port/spec/audio.md)\n",
                           CAPTURE_DRO);
                } else {
                    fclose(cf);
                    printf("SKIP capture oracle — %s present but no stream "
                           "decoded: is python3 on PATH and " OPL_TRACE_PY
                           " runnable?\n", CAPTURE_DRO);
                }
            } else {
                for (u32 i = 0; i < cap_total; i++)
                    if (cap_ev[i].reg >= 0xB0 && cap_ev[i].reg <= 0xB8 &&
                        (cap_ev[i].val & 0x20)) {
                        first_key = i;
                        break;
                    }
                /* Anchor shadow. The driver's tick-0 block is a full 18-voice
                 * cached-state init (147 writes, of which 145 the port does not
                 * model — spec divergence 4) and its first key-on is the last
                 * of them, so the anchor drops them from the capture stream.
                 * They were written to the chip all the same: the capture enters
                 * the compared window with a post-init shadow, the port with
                 * its power-on shadow, and a register the port first writes at
                 * its unchanged init value is then emitted by the port and
                 * suppressed by the driver. Seed both sides' collapse shadow
                 * from the capture's pre-anchor register state so the
                 * comparison starts from one hardware state and only
                 * post-anchor changes are compared. */
                for (u32 i = 0; i < first_key; i++) {
                    u16 sh = (u16)(cap_ev[i].reg & (OPL_SHADOW_REGS - 1));
                    anchor_state[sh] = (u8)cap_ev[i].val;
                }
                memcpy(cap_last, anchor_state, sizeof cap_last);
                {
                    /* The capture records no MIDI channel, so a captured write
                     * inherits the attribution of the port's write at the same
                     * (tick, register): the port knows the voice's channel, and
                     * one register is written at most once per tick. Unmatched
                     * capture writes stay unattributed (0xFF) and are compared.
                     * The capture ticks are non-decreasing, so a cursor over the
                     * port stream keeps this linear. */
                    u32 c_scan = 0;
                    for (u32 i = first_key; i < cap_total; i++) {
                        u16 sh = (u16)(cap_ev[i].reg & (OPL_SHADOW_REGS - 1));
                        u32 ct = (cap_ev[i].tick * 120u + 500u) / 1000u + 60u;
                        u8 attr = 0xFF;

                        while (c_scan < (u32)c_n && c_ev[c_scan].tick < ct)
                            c_scan++;
                        for (u32 k = c_scan; k < (u32)c_n && c_ev[k].tick == ct; k++)
                            if (c_ev[k].reg == cap_ev[i].reg) {
                                attr = c_ev[k].attr;
                                break;
                            }
                        if (documented_excluded(cap_ev[i].reg, attr))
                            continue;
                        /* Symmetric reduction, capture side: drop a write whose
                         * value equals the last kept value for that register. */
                        if ((u8)cap_ev[i].val == cap_last[sh])
                            continue;
                        cap_last[sh] = (u8)cap_ev[i].val;
                        cap_ev[w].tick = ct;
                        cap_ev[w].reg = cap_ev[i].reg;
                        cap_ev[w].val = cap_ev[i].val;
                        cap_ev[w].attr = attr;
                        w++;
                    }
                }
                /* The port is reduced by the same rule as the capture: start
                 * at its first key-on, skip the documented-excluded registers,
                 * and drop a write whose value equals the last kept value for
                 * that register.
                 * Stop at the first difference or when either stream is
                 * exhausted: a stream that merely ended must not read as "all
                 * matched" — an uncompared capture tail is a real result, not a
                 * pass. */
                {
                    u8 c_last[OPL_SHADOW_REGS];
                    u32 ci = 0, c_tail = 0, cw = 0;
                    memcpy(c_last, anchor_state, sizeof c_last);
                    for (u32 i = 0; i < (u32)c_n; i++)
                        if (c_ev[i].reg >= 0xB0 && c_ev[i].reg <= 0xB8 &&
                            (c_ev[i].val & 0x20)) {
                            first_key_c = i;
                            break;
                        }
                    for (u32 i = first_key_c; i < (u32)c_n; i++) {
                        u16 sh = (u16)(c_ev[i].reg & (OPL_SHADOW_REGS - 1));
                        if (c_ev[i].tick == 0 ||
                            documented_excluded(c_ev[i].reg, c_ev[i].attr))
                            continue;
                        if ((u8)c_ev[i].val == c_last[sh])
                            continue;
                        c_last[sh] = (u8)c_ev[i].val;
                        c_ev[cw++] = c_ev[i];
                    }
                    for (;;) {
                        if (ci >= cw || pyi >= w)
                            break;
                        if (!ev_eq(&c_ev[ci], &cap_ev[pyi])) {
                            diff = (int)ci;
                            break;
                        }
                        ci++;
                        pyi++;
                    }
                    c_tail = cw - pyi;
                    if (diff >= 0)
                        printf("capture oracle first difference at C write %d: "
                               "C tick=%u reg=%#04x val=%#04x vs capture tick=%u reg=%#04x val=%#04x "
                               "(C %d writes, capture %u normalised)\n",
                               diff, c_ev[diff].tick, c_ev[diff].reg, c_ev[diff].val,
                               cap_ev[pyi].tick, cap_ev[pyi].reg, cap_ev[pyi].val, c_n, w);
                    else if (c_tail == 0 && pyi == w)
                        printf("capture oracle: %u writes normalised vs C, all "
                               "compared and matched\n", w);
                    else
                        printf("capture oracle: %u compared/matched, %u C-only, "
                               "%u capture-only tail (not byte-exact; C %d writes, "
                               "capture %u normalised)\n",
                               pyi, c_tail, w - pyi, c_n, w);
                }
            }
        }
    }

    /* 9. Optional headless audio render (PR_AUDIO_WAV=<path>). On hosts where
     *    SDL audio cannot open, the FM output is inaudible in the windowed run;
     *    this plays the title bank through the sequencer + OPL core + mixer and
     *    writes a 16-bit stereo WAV at the OPL rate, so the music can be
     *    listened to in any player. Duration via PR_AUDIO_WAV_SECONDS (default
     *    12). The sequencer is paced by the rendered audio, not by wall time:
     *    the driver runs at 2 ticks per 60 Hz frame = 120 Hz, so each
     *    MIXER_OPL_RATE/120 rendered frames advance one tick. */
    {
        const char *wav = getenv("PR_AUDIO_WAV");
        if (wav != NULL) {
            u32 seconds = 12;
            const char *sec = getenv("PR_AUDIO_WAV_SECONDS");
            u32 rate = MIXER_OPL_RATE;
            u32 total, done = 0, acc = 0;
            s16 chunk[2 * 1024];
            FILE *f;

            if (sec != NULL && atoi(sec) > 0) seconds = (u32)atoi(sec);
            total = seconds * rate;
            f = fopen(wav, "wb");
            if (f == NULL) {
                CHECK(0, "PR_AUDIO_WAV: cannot open output file");
            } else {
                u8 hdr[44];
                u32 data_bytes = total * 4u;   /* stereo, 16-bit */
                u32 riff = 36u + data_bytes;
                u32 brate = rate * 4u;
                u32 i;

                for (i = 0; i < 4; i++) hdr[i] = "RIFF"[i];
                hdr[4] = (u8)riff; hdr[5] = (u8)(riff >> 8);
                hdr[6] = (u8)(riff >> 16); hdr[7] = (u8)(riff >> 24);
                for (i = 0; i < 4; i++) hdr[8 + i] = "WAVE"[i];
                for (i = 0; i < 4; i++) hdr[12 + i] = "fmt "[i];
                hdr[16] = 16; hdr[17] = 0; hdr[18] = 0; hdr[19] = 0;
                hdr[20] = 1; hdr[21] = 0;      /* PCM */
                hdr[22] = 2; hdr[23] = 0;      /* 2 channels */
                hdr[24] = (u8)rate; hdr[25] = (u8)(rate >> 8);
                hdr[26] = (u8)(rate >> 16); hdr[27] = (u8)(rate >> 24);
                hdr[28] = (u8)brate; hdr[29] = (u8)(brate >> 8);
                hdr[30] = (u8)(brate >> 16); hdr[31] = (u8)(brate >> 24);
                hdr[32] = 4; hdr[33] = 0;      /* block align */
                hdr[34] = 16; hdr[35] = 0;     /* bits per sample */
                for (i = 0; i < 4; i++) hdr[36 + i] = "data"[i];
                hdr[40] = (u8)data_bytes; hdr[41] = (u8)(data_bytes >> 8);
                hdr[42] = (u8)(data_bytes >> 16); hdr[43] = (u8)(data_bytes >> 24);
                fwrite(hdr, 1, sizeof hdr, f);

                opl_reset();
                mixer_reset();
                seq_start();
                while (done < total) {
                    u32 n = total - done;
                    if (n > 1024u) n = 1024u;
                    mixer_render(chunk, n, rate);
                    fwrite(chunk, 2, (size_t)n * 2u, f);
                    done += n;
                    acc += n * 120u;            /* sequencer ticks per second */
                    while (acc >= rate) {
                        acc -= rate;
                        seq_tick();
                    }
                }
                fclose(f);
                printf("PR_AUDIO_WAV: wrote %s (%u s, %u Hz)\n", wav,
                       (unsigned)seconds, (unsigned)rate);
            }
        }
    }

    free(gra);
    free(fat);
    return g_failures - before;
}

/* ---- test_ail.c ---- */

/* Shallow checks for the AIL call surface (Task 10). The end-to-end wiring is
 * Task 11's and the announcer sample is Task 12's; this file only pins the
 * surface's observable contracts: the fixed-profile short-circuit, the
 * preference/timer handle semantics, that music start/stop drives the
 * sequencer, and that a sample-play call converts its bytes and reaches the
 * mixer. No audio device is opened. */








/* A minimal XMIDI bank: FORM/XMID with one EVNT chunk holding a note-on (with a
 * 0x10-tick delta before the end meta) so the sequence stays playing for a
 * tick, then the FF 2F end. Sizes are big-endian, as the container demands. */
static const u8 k_bank[] = {
    'F', 'O', 'R', 'M', 0x00, 0x00, 0x00, 0x14,
    'X', 'M', 'I', 'D',
    'E', 'V', 'N', 'T', 0x00, 0x00, 0x00, 0x08,
    0x90, 0x3C, 0x64, 0x7F,       /* note on, ch0, note 0x3C, vel 0x64, dur 127 */
    0x10,                         /* delta: 16 ticks before the next event */
    0xFF, 0x2F, 0x00              /* XMIDI end */
};

static s16 ail_out[2 * 64];

static int ail_all_zero(const s16 *b, int n)
{
    for (int i = 0; i < n; i++)
        if (b[i] != 0)
            return 0;
    return 1;
}

static void timer_cb(void) {}

int test_ail(void)
{
    int before = g_failures;

    /* 1. Defaults and the preference swap semantics (spec row 3): setting
     *    returns the old value, and an ail_out-of-range index is rejected. */
    AIL_startup();
    CHECK_EQ_INT(AIL_set_preference(4, 2), 0x10);
    CHECK_EQ_INT(AIL_set_preference(0x12, 1), -1);

    /* 2. Fixed profile, no hardware probe (spec rows 8, 9, 25): the fallback
     *    chain's names all succeed, and so does a name that cannot exist,
     *    proving no driver file is probed or opened. */
    HDIGDRIVER dig = AIL_install_DIG_INI();
    CHECK(dig != NULL, "install_DIG_INI returns a fixed-profile handle");
    CHECK(AIL_install_DIG_driver_file("SB16.DIG", NULL) != NULL,
          "fixed profile: SB16.DIG install succeeds");
    CHECK(AIL_install_DIG_driver_file("SBPRO.DIG", NULL) != NULL,
          "fixed profile: SBPRO.DIG install succeeds");
    CHECK(AIL_install_DIG_driver_file("SBLASTER.DIG", NULL) != NULL,
          "fixed profile: SBLASTER.DIG install succeeds");
    CHECK(AIL_install_DIG_driver_file("NO_SUCH.DRV", NULL) != NULL,
          "fixed profile: no driver-file probe (bogus name succeeds)");
    HMDIDRIVER mdi = AIL_install_MDI_INI();
    CHECK(mdi != NULL, "install_MDI_INI returns a fixed-profile handle");

    /* 3. Timer handles (spec rows 4-7): distinct slots, a released slot is
     *    reused. The callback is stored, never fired (no ISR in the port). */
    HTIMER t0 = AIL_register_timer(timer_cb);
    HTIMER t1 = AIL_register_timer(timer_cb);
    CHECK(t0 >= 0 && t1 >= 0 && t1 != t0, "timer handles are distinct slots");
    AIL_set_timer_frequency(t0, 0x3c);
    AIL_start_timer(t0);
    AIL_release_timer_handle(t0);
    CHECK(AIL_register_timer(timer_cb) == t0, "a released timer slot is reused");
    AIL_release_timer_handle(t1);

    /* 4. Music start/stop drive the sequencer (spec rows 27-29, 31). Start
     *    emits the sequencer's opening register writes and reports playing (4);
     *    one tick keys the bank's note; stop halts it and reports 2; no further
     *    tick writes anything. */
    HSEQUENCE seq = AIL_allocate_sequence_handle(mdi);
    CHECK(seq != NULL, "allocate_sequence_handle returns the sequence");
    CHECK(AIL_allocate_sequence_handle(mdi) == NULL,
          "the single sequence handle is exhausted on a second allocate");
    CHECK_EQ_INT(AIL_init_sequence(seq, k_bank, 0), 1);
    opl_reset();
    AIL_start_sequence(seq);
    u32 w0 = opl_write_count();
    CHECK(w0 >= 2, "start_sequence emits the sequencer opening writes");
    CHECK_EQ_INT(AIL_sequence_status(seq), 4);
    seq_tick();
    CHECK_EQ_INT(seq_active_track(), 1);
    CHECK(opl_write_count() > w0, "a tick emits the note's register writes");
    CHECK_EQ_INT(AIL_sequence_status(seq), 4);
    AIL_stop_sequence(seq);
    CHECK_EQ_INT(seq_active_track(), 0);
    CHECK_EQ_INT(AIL_sequence_status(seq), 2);
    {
        u32 w1 = opl_write_count();
        for (int i = 0; i < 8; i++)
            seq_tick();
        CHECK_EQ_INT((int)opl_write_count(), (int)w1);
    }

    /* 4b. Natural end (spec row 31): the bank's FF 2F end meta halts playback,
     *     so AIL_sequence_status reports 2 with no AIL_stop_sequence call. */
    AIL_start_sequence(seq);
    CHECK_EQ_INT(AIL_sequence_status(seq), 4);
    for (int i = 0; i < 40; i++)
        seq_tick();
    CHECK_EQ_INT(AIL_sequence_status(seq), 2);

    /* 4c. A failed re-init clears `loaded`: a later start_sequence must not
     *     restart the previously loaded bank. */
    {
        static const u8 bad_bank[4] = { 0 };
        CHECK_EQ_INT(AIL_init_sequence(seq, bad_bank, 0), 0);
        AIL_start_sequence(seq);
        CHECK_EQ_INT(AIL_sequence_status(seq), 2);
    }

    /* 5. The 8-bit -> s16 conversion is exact and lives once, in samples.c. */
    {
        static const u8 pcm8[3] = { 0, 128, 255 };
        s16 conv[3];
        CHECK_EQ_INT(samples_to_s16(pcm8, 3, conv), 3);
        CHECK_EQ_INT(conv[0], -32768);
        CHECK_EQ_INT(conv[1], 0);
        CHECK_EQ_INT(conv[2], 32512);
        CHECK_EQ_INT(samples_to_s16(NULL, 3, conv), 0);
        CHECK_EQ_INT(samples_to_s16(pcm8, 3, NULL), 0);
    }

    /* 6. The sample-play path: four handles, then exhaustion; start marks
     *    playing and adds a mixer voice of the converted bytes, so rendering
     *    is non-silent; stop returns to stopped. */
    {
        HSAMPLE hs[4];
        for (int i = 0; i < 4; i++) {
            hs[i] = AIL_allocate_sample_handle(dig);
            CHECK(hs[i] != NULL, "one of the four sample handles allocates");
        }
        CHECK(AIL_allocate_sample_handle(dig) == NULL,
              "a fifth sample handle is refused (four-handle pool)");

        static const u8 pcm[4] = { 0, 0, 0, 0 };   /* converted: all -32768 */
        AIL_init_sample(hs[0]);
        CHECK_EQ_INT(AIL_sample_status(hs[0]), 2);
        AIL_set_sample_address(hs[0], pcm, 4);
        AIL_set_sample_type(hs[0], 0, 0);
        AIL_set_sample_rate(hs[0], 11025);
        AIL_set_sample_volume(hs[0], 0x7f);
        AIL_set_sample_loop_count(hs[0], 0);

        /* hs[1] plays a tone so that stopping hs[0] can be shown not to stop it
         * (the original stops one handle, not every sample voice). */
        static const u8 tone8[4] = { 200, 56, 200, 56 };
        AIL_init_sample(hs[1]);
        AIL_set_sample_address(hs[1], tone8, 4);
        AIL_set_sample_rate(hs[1], 11025);
        AIL_set_sample_volume(hs[1], 0x7f);
        AIL_set_sample_loop_count(hs[1], 1);

        mixer_reset();                 /* silence OPL so only the voices are heard */
        AIL_start_sample(hs[0]);
        CHECK_EQ_INT(AIL_sample_status(hs[0]), 4);
        AIL_start_sample(hs[1]);
        CHECK_EQ_INT(AIL_sample_status(hs[1]), 4);
        mixer_render(ail_out, 64, 44100);
        CHECK(!ail_all_zero(ail_out, 2 * 64),
              "start_sample adds a mixer voice of the converted sample");

        AIL_stop_sample(hs[0]);
        CHECK_EQ_INT(AIL_sample_status(hs[0]), 2);
        CHECK_EQ_INT(AIL_sample_status(hs[1]), 4);
        mixer_render(ail_out, 64, 44100);
        CHECK(!ail_all_zero(ail_out, 2 * 64),
              "stop_sample stops one handle's voice, not every sample voice");

        AIL_stop_sample(hs[1]);
        CHECK_EQ_INT(AIL_sample_status(hs[1]), 2);
        mixer_render(ail_out, 64, 44100);
        CHECK(ail_all_zero(ail_out, 2 * 64), "stopping the last sample voice is silence");

        /* Volume 0x7f maps to the mixer's unity (256), not 254, so the
         * converted byte passes through unchanged: 200 -> (200-128)<<8. */
        static const u8 one8[1] = { 200 };
        mixer_reset();
        AIL_init_sample(hs[0]);
        AIL_set_sample_address(hs[0], one8, 1);
        AIL_set_sample_rate(hs[0], 44100);   /* == out_rate: no resampling */
        AIL_set_sample_volume(hs[0], 0x7f);
        AIL_set_sample_loop_count(hs[0], 0);
        AIL_start_sample(hs[0]);
        mixer_render(ail_out, 1, 44100);
        CHECK_EQ_INT(ail_out[0], 18432);
        CHECK_EQ_INT(ail_out[1], 18432);
        AIL_stop_sample(hs[0]);
        /* A start with no sample bytes adds no voice, so status stays stopped
         * instead of reporting playing with nothing behind it. */
        AIL_set_sample_address(hs[0], NULL, 0);
        AIL_start_sample(hs[0]);
        CHECK_EQ_INT(AIL_sample_status(hs[0]), 2);
        AIL_release_sample_handle(hs[0]);
        AIL_release_sample_handle(hs[1]);

        /* An uninitialised handle reports 0, not a stale status. */
        CHECK_EQ_INT(AIL_sample_status(NULL), 0);
    }

    /* 7. Shutdown releases everything and the pool is reusable. */
    AIL_shutdown();
    CHECK_EQ_INT(AIL_sample_status(NULL), 0);
    CHECK(AIL_allocate_sample_handle(dig) != NULL,
          "sample handles are reusable after shutdown");

    return g_failures - before;
}
