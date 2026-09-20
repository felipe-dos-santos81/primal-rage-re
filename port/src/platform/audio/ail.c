/* The AIL (Miles) call surface. See ail.h for the contract, the fixed-profile
 * and I/O-free rules, and the real-mode-boundary replacement note.
 *
 * All state here is port-only bookkeeping (PORTING.md:20), so it is static: the
 * game's mem[] holds none of it. The original kept the same state inside the
 * AIL engine (prefs at DAT_00108d64, four sample handles, one sequence handle),
 * which the port cannot run.
 *
 * PORT: no real-mode/DPMI crossing remains. In the original every one of these
 * calls eventually reached the loaded .DIG/.MDI driver through AIL's dispatcher
 * (0x5d973, `swi 0x31`). Here they call sequencer/samples/mixer directly. The
 * sample-start call is the clearest replacement: the original's driver call
 * 0x401 (DMA start) becomes samples_to_s16() + mixer_add_sample().
 */
#include <stddef.h>
#include <stdlib.h>

#include "ail.h"
#include "mixer.h"
#include "samples.h"
#include "sequencer.h"

/* The 18 default preferences AIL_startup installs, prefs 0..0x11 (spec audio.md
 * "AIL surface" row 1). Shipped defaults the game relies on: pref 1 = 1,
 * pref 10 (0xa) = 0x78 = 120, pref 11 (0xb) = 8. These are only the defaults;
 * FUN_0001cf40 later overrides some through AIL_set_preference — pref 1 =
 * 0x2b11 (11025 Hz), pref 3 = 0x14, pref 4 = 4, pref 0xb = 1, pref 7 = 1
 * (spec row 3). */
#define AIL_PREF_COUNT 0x12
#define AIL_MAX_TIMERS 16
#define AIL_MAX_SAMPLES 4      /* spec row 10: FUN_0001cf40 allocates 0x60/0x18 */
#define AIL_MAX_SEQUENCES 1    /* spec row 26: the single sequence handle */

static const u32 AIL_PREF_DEFAULTS[AIL_PREF_COUNT] = {
    200, 1, 0x8000, 100, 0x10, 100, 0x28f, 0, 0,
    1, 0x78, 8, 0x7f, 1, 0, 2, 1, 1
};

static u32 g_prefs[AIL_PREF_COUNT];

struct AIL_DIG_DRIVER { int fixed; };
struct AIL_MDI_DRIVER { int fixed; };

struct AIL_SAMPLE {
    int used;
    s32 state;        /* 2 stopped, 4 playing (spec rows 20, 31) */
    const u8 *addr;   /* borrowed sample bytes */
    u32 len;          /* sample length in frames (8-bit mono) */
    s32 format;       /* spec row 14: 0..3 code from the record's flags */
    u32 flag;
    u32 rate;         /* spec row 17: 0x2b11 = 11025 */
    s32 volume;       /* 0..0x7f (spec row 18) */
    u32 loop;         /* spec row 19: 0 = one-shot */
    s16 *conv;        /* owned s16 conversion buffer, mixer references it */
    u32 conv_cap;     /* frames `conv` can hold */
};

struct AIL_SEQUENCE {
    int used;
    int loaded;
    s32 state;        /* spec row 31: 2 stopped, 4 playing */
    s32 volume;       /* spec row 30: recorded, not applied */
};

typedef struct {
    int used;
    AIL_TIMER_CB cb;
    u32 period_us;
    int running;
} ail_timer;

/* The fixed-profile driver handles. Both are valid forever: the port presents
 * the shipped profile unconditionally instead of probing (ail.h). */
static struct AIL_DIG_DRIVER g_dig_driver = { 1 };
static struct AIL_MDI_DRIVER g_mdi_driver = { 1 };

static ail_timer g_timers[AIL_MAX_TIMERS];
static struct AIL_SAMPLE g_samples[AIL_MAX_SAMPLES];
static struct AIL_SEQUENCE g_sequences[AIL_MAX_SEQUENCES];

/* ---- engine lifecycle / preferences ------------------------------------ */

/* 0x5d851 — spec audio.md "AIL surface" (row 1). */
void AIL_startup(void)
{
    for (int i = 0; i < AIL_PREF_COUNT; i++)
        g_prefs[i] = AIL_PREF_DEFAULTS[i];
}

/* 0x5d86a — spec audio.md "AIL surface" (row 2). */
void AIL_shutdown(void)
{
    for (int i = 0; i < AIL_MAX_SAMPLES; i++)
        AIL_release_sample_handle(&g_samples[i]);
    for (int i = 0; i < AIL_MAX_TIMERS; i++) {
        g_timers[i].used = 0;
        g_timers[i].running = 0;
    }
    seq_stop();
    for (int i = 0; i < AIL_MAX_SEQUENCES; i++) {
        g_sequences[i].used = 0;
        g_sequences[i].state = 0;
        g_sequences[i].loaded = 0;
    }
}

/* 0x5d87e — spec audio.md "AIL surface" (row 3). */
s32 AIL_set_preference(u32 preference, u32 value)
{
    if (preference >= AIL_PREF_COUNT)
        return -1;
    s32 old = (s32)g_prefs[preference];
    g_prefs[preference] = value;
    return old;
}

/* ---- timers ------------------------------------------------------------ */

/* 0x5da12 — spec audio.md "AIL surface" (row 4). */
HTIMER AIL_register_timer(AIL_TIMER_CB callback)
{
    for (int i = 0; i < AIL_MAX_TIMERS; i++) {
        if (g_timers[i].used)
            continue;
        g_timers[i].used = 1;
        g_timers[i].cb = callback;
        g_timers[i].period_us = 0;
        g_timers[i].running = 0;
        return (HTIMER)i;
    }
    return -1;    /* "Out of timer handles" */
}

/* 0x5da87 — spec audio.md "AIL surface" (row 5). */
void AIL_set_timer_frequency(HTIMER timer, u32 hz)
{
    if (timer < 0 || timer >= AIL_MAX_TIMERS)
        return;
    if (hz == 0)
        return;
    g_timers[timer].period_us = 1000000u / hz;
}

/* 0x5daa6 — spec audio.md "AIL surface" (row 6).
 * PORT: no PIT/ISR; the callback is stored but never fired here. */
void AIL_start_timer(HTIMER timer)
{
    if (timer < 0 || timer >= AIL_MAX_TIMERS)
        return;
    g_timers[timer].running = 1;
}

/* 0x5dadc — spec audio.md "AIL surface" (row 7). */
void AIL_release_timer_handle(HTIMER timer)
{
    if (timer < 0 || timer >= AIL_MAX_TIMERS)
        return;
    g_timers[timer].used = 0;
    g_timers[timer].running = 0;
}

/* ---- driver install ---------------------------------------------------- */

/* 0x5db7c — spec audio.md "AIL surface" (row 8).
 * PORT: fixed audio profile, no hardware probe. */
HDIGDRIVER AIL_install_DIG_INI(void)
{
    return &g_dig_driver;
}

/* 0x5db9e — spec audio.md "AIL surface" (row 9).
 * PORT: fixed audio profile, no hardware probe — the file is never opened, so
 * SB16.DIG (and any fallback name) succeeds; the fallback chain stops on the
 * first call exactly as it would on the shipped SB16. */
HDIGDRIVER AIL_install_DIG_driver_file(const char *filename, const void *addr)
{
    (void)filename;
    (void)addr;
    return &g_dig_driver;
}

/* 0x5ddd0 — spec audio.md "AIL surface" (row 25).
 * PORT: fixed audio profile, no hardware probe. */
HMDIDRIVER AIL_install_MDI_INI(void)
{
    return &g_mdi_driver;
}

/* ---- sample handles ---------------------------------------------------- */

/* Grows (never shrinks) the handle's conversion buffer to hold its current
 * sample. A voice may still reference the old buffer, so this handle's voices
 * are stopped before the old buffer is freed. Returns NULL when there is no
 * sample or the allocation fails. */
static s16 *sample_buffer(HSAMPLE s)
{
    if (s->len == 0)
        return NULL;
    if (s->conv != NULL && s->conv_cap >= s->len)
        return s->conv;
    if (s->conv != NULL)
        mixer_stop_sample(s);
    s16 *grown = (s16 *)malloc((size_t)s->len * sizeof(s16));
    if (grown == NULL)
        return NULL;
    free(s->conv);
    s->conv = grown;
    s->conv_cap = s->len;
    return s->conv;
}

/* 0x5dbcb — spec audio.md "AIL surface" (row 10). */
HSAMPLE AIL_allocate_sample_handle(HDIGDRIVER driver)
{
    (void)driver;
    for (int i = 0; i < AIL_MAX_SAMPLES; i++) {
        if (g_samples[i].used)
            continue;
        g_samples[i].used = 1;
        g_samples[i].state = 2;   /* spec row 10: allocate inits the sample */
        g_samples[i].addr = NULL;
        g_samples[i].len = 0;
        g_samples[i].format = 0;
        g_samples[i].flag = 0;
        g_samples[i].rate = 11025;
        g_samples[i].volume = 0x7f;
        g_samples[i].loop = 1;
        g_samples[i].conv = NULL;
        g_samples[i].conv_cap = 0;
        return &g_samples[i];
    }
    return NULL;    /* "Out of sample handles" */
}

/* 0x5dbf4 — spec audio.md "AIL surface" (row 11). */
void AIL_release_sample_handle(HSAMPLE sample)
{
    if (sample == NULL || !sample->used)
        return;
    mixer_stop_sample(sample);
    free(sample->conv);
    sample->conv = NULL;
    sample->conv_cap = 0;
    sample->used = 0;
    sample->state = 0;
}

/* 0x5dc0f — spec audio.md "AIL surface" (row 12). */
void AIL_init_sample(HSAMPLE sample)
{
    if (sample == NULL || !sample->used)
        return;
    sample->state = 2;
    sample->rate = 11025;
    sample->volume = 0x7f;
    sample->loop = 1;
    sample->format = 0;
    sample->flag = 0;
    /* addr/len are the game's to set (row 13) and are kept. */
}

/* 0x5dc2a — spec audio.md "AIL surface" (row 13). */
void AIL_set_sample_address(HSAMPLE sample, const void *buf, u32 len)
{
    if (sample == NULL || !sample->used)
        return;
    sample->addr = (const u8 *)buf;
    sample->len = len;
}

/* 0x5dc4d — spec audio.md "AIL surface" (row 14). */
void AIL_set_sample_type(HSAMPLE sample, s32 format, u32 flag)
{
    if (sample == NULL || !sample->used)
        return;
    sample->format = format;
    sample->flag = flag;
}

/* 0x5dc70 — spec audio.md "AIL surface" (row 15).
 * PORT: replaces the original's driver call 0x401 (DMA start) with the
 * conversion + mixer add. */
void AIL_start_sample(HSAMPLE sample)
{
    if (sample == NULL || !sample->used)
        return;
    s16 *buf = sample_buffer(sample);
    mixer_stop_sample(sample);   /* a restart must not stack a second voice */
    if (buf != NULL && sample->addr != NULL) {
        /* PORT: sample->format (row 14) is recorded but never applied. The only
         * shipped sample is 8-bit unsigned mono (samples.h) and samples_load
         * rejects every other shape, so this conversion is unconditional. */
        u32 frames = samples_to_s16(sample->addr, sample->len, buf);
        int rate = sample->rate != 0 ? (int)sample->rate : 11025;
        /* 0..0x7f -> Q8 with 0x7f (the game's full volume) at unity 256, not
         * 254: the mixer's unity is 256 (mixer.h). */
        int volume = (int)sample->volume * 256 / 0x7f;
        if (mixer_add_sample(buf, frames, rate, volume, sample->loop != 0,
                             sample))
            sample->state = 4;
        return;
    }
    /* No sample bytes (or no buffer): no voice was added, so report stopped
     * rather than playing with nothing behind it. */
    sample->state = 2;
}

/* 0x5dc8b — spec audio.md "AIL surface" (row 16). */
void AIL_stop_sample(HSAMPLE sample)
{
    if (sample == NULL || !sample->used)
        return;
    sample->state = 2;
    mixer_stop_sample(sample);   /* this handle's voice only (row 16) */
}

/* 0x5dca6 — spec audio.md "AIL surface" (row 17). */
void AIL_set_sample_rate(HSAMPLE sample, u32 rate)
{
    if (sample == NULL || !sample->used)
        return;
    sample->rate = rate;
}

/* 0x5dcc5 — spec audio.md "AIL surface" (row 18). */
void AIL_set_sample_volume(HSAMPLE sample, s32 volume)
{
    if (sample == NULL || !sample->used)
        return;
    if (volume < 0)
        volume = 0;
    if (volume > 0x7f)
        volume = 0x7f;
    sample->volume = volume;
}

/* 0x5dce4 — spec audio.md "AIL surface" (row 19). */
void AIL_set_sample_loop_count(HSAMPLE sample, u32 count)
{
    if (sample == NULL || !sample->used)
        return;
    sample->loop = count;
}

/* 0x5dd03 — spec audio.md "AIL surface" (row 20). */
s32 AIL_sample_status(HSAMPLE sample)
{
    if (sample == NULL || !sample->used)
        return 0;
    return sample->state;
}

/* Deferred movie/Smacker streaming surface (spec rows 21-24). Owner:
 * sub-project 2b (Smacker video), which uses the same streaming path the
 * original's FUN_00010034/FUN_000102b8 drive. Safe inert stubs. */

/* 0x5dd2c — spec audio.md "AIL surface" (row 21, name TODO(verify)). */
s32 AIL_sample_buffer_size(HDIGDRIVER driver, u32 rate, u32 len)
{
    (void)driver;
    (void)rate;
    (void)len;
    return 0;
}

/* 0x5dd5d — spec audio.md "AIL surface" (row 22, name TODO(verify)). */
s32 AIL_stream_buffer_index(HSAMPLE sample)
{
    (void)sample;
    return -1;    /* original: -1 when neither streaming half needs refilling */
}

/* 0x5dd86 — spec audio.md "AIL surface" (row 23, name TODO(verify)). */
void AIL_stream_feed(HSAMPLE sample, s32 half, const void *buf, u32 len)
{
    (void)sample;
    (void)half;
    (void)buf;
    (void)len;
}

/* 0x5ddad — spec audio.md "AIL surface" (row 24, name inferred). */
void AIL_register_sample_callback(HSAMPLE sample, u32 which, void (*cb)(HSAMPLE))
{
    (void)sample;
    (void)which;
    (void)cb;
}

/* ---- sequences --------------------------------------------------------- */

/* 0x5de1f — spec audio.md "AIL surface" (row 26). */
HSEQUENCE AIL_allocate_sequence_handle(HMDIDRIVER driver)
{
    (void)driver;
    for (int i = 0; i < AIL_MAX_SEQUENCES; i++) {
        if (g_sequences[i].used)
            continue;
        g_sequences[i].used = 1;
        g_sequences[i].loaded = 0;
        g_sequences[i].state = 0;
        g_sequences[i].volume = 0;
        return &g_sequences[i];
    }
    return NULL;    /* "Out of sequence handles" */
}

/* 0x5de48 — spec audio.md "AIL surface" (row 27).
 * PORT: the third argument keeps the original's meaning (the sequence number,
 * which the game passes as 0); the bank length seq_load needs is derived from
 * the XMIDI container itself (seq_bank_size), not passed in. */
s32 AIL_init_sequence(HSEQUENCE sequence, const void *data, u32 sequence_num)
{
    (void)sequence_num;
    if (sequence == NULL || !sequence->used)
        return 0;
    if (!seq_load((const u8 *)data, seq_bank_size((const u8 *)data))) {
        /* Bad data: clear `loaded` so a later AIL_start_sequence cannot restart
         * the previous bank. seq_load returns before halt(), so an already
         * playing bank keeps sounding through the engine; the status below
         * reports that truthfully instead of claiming stopped. TODO(verify): the
         * original's behaviour on a failed re-init of a playing handle (stop vs
         * keep playing) is unproven. */
        sequence->loaded = 0;
        sequence->state = seq_playing() ? 4 : 2;
        return 0;
    }
    sequence->loaded = 1;
    sequence->state = 2;
    return 1;
}

/* 0x5de79 — spec audio.md "AIL surface" (row 28). */
void AIL_start_sequence(HSEQUENCE sequence)
{
    if (sequence == NULL || !sequence->used || !sequence->loaded)
        return;
    seq_start();
    sequence->state = 4;
}

/* 0x5deaf — spec audio.md "AIL surface" (row 29). */
void AIL_stop_sequence(HSEQUENCE sequence)
{
    if (sequence == NULL || !sequence->used)
        return;
    seq_stop();
    sequence->state = 2;
}

/* 0x5deca — spec audio.md "AIL surface" (row 30).
 * PORT: the value feeds the sequencer's sequence-volume input, which the
 * original's CC7 arm uses to scale the driver's volume before dispatch
 * (prage.c:49121). The 500 ms fade is still not modelled (the port applies the
 * target immediately). */
void AIL_set_sequence_volume(HSEQUENCE sequence, s32 volume, u32 fade_ms)
{
    (void)fade_ms;
    if (sequence == NULL || !sequence->used)
        return;
    sequence->volume = volume;
    seq_set_sequence_volume((u8)(volume < 0 ? 0 : volume > 0x7f ? 0x7f : volume));
}

/* 0x5deed — spec audio.md "AIL surface" (row 31). */
s32 AIL_sequence_status(HSEQUENCE sequence)
{
    if (sequence == NULL || !sequence->used)
        return 0;
    return seq_playing() ? 4 : 2;
}

/* ---- no-ops ------------------------------------------------------------ */

/* 0x5dfdc — spec audio.md "AIL surface" (row 32). */
void AIL_noop_5dfdc(void)
{
}

/* 0x5dfeb — spec audio.md "AIL surface" (row 33). */
void AIL_noop_5dfeb(void)
{
}
