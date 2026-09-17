/* Software mixer. See mixer.h for the invented-subsystem rationale, the voice
 * pool size, and the volume/resampling contract.
 *
 * Arithmetic: each output frame starts from the OPL core's stereo sample and
 * adds every active voice's sample scaled by its Q8 volume. The accumulator is
 * s32 so four full-scale voices plus OPL (up to ~17x full scale) cannot overflow
 * before saturation; the result is then saturated to the s16 limits. A wrap here
 * would be audible garbage, so saturation is explicit, never a cast.
 */
#include <stddef.h>

#include "mixer.h"
#include "opl/opl.h"

/* Four, matching the original's four AIL sample handles. TODO(verify): the
 * original's behaviour once all four are busy is unknown — drop, steal, or
 * error (see mixer.h); this port drops. */
#define MIXER_VOICES 4
#define MIXER_UNITY_VOLUME 256
#define MIXER_MAX_VOLUME 1024

/* MIXER_OPL_RATE (the OPL core's native rate) is exported by mixer.h so the
 * frame loop opens the device at exactly the rate this module renders; see that
 * comment. */

typedef struct {
    const s16 *pcm;
    const void *owner;   /* AIL sample handle; see mixer_stop_sample */
    u32 frames;
    int rate;
    int volume;
    int loop;
    int active;
    u32 idx;
    u32 frac;
    u32 step;
} mixer_voice;

static mixer_voice g_voices[MIXER_VOICES];

/* OPL resampling state: the core is generative, so it is rendered forward and
 * the frame nearest the phase is held in g_opl_last. */
static u32 g_opl_idx;
static u32 g_opl_frac;
static u32 g_opl_step;
static u32 g_opl_rendered;
static s16 g_opl_last[2];

/* 16.16 samples-per-output-frame. */
static u32 rate_step(int rate, u32 out_rate)
{
    return (u32)(((unsigned long long)(u32)rate << 16) / out_rate);
}

static s16 sat16(s32 v)
{
    if (v > 32767) return 32767;
    if (v < -32768) return -32768;
    return (s16)v;
}

void mixer_reset(void)
{
    for (int i = 0; i < MIXER_VOICES; i++)
        g_voices[i].active = 0;
    g_opl_idx = 0;
    g_opl_frac = 0;
    g_opl_step = 0;
    g_opl_rendered = 0;
    g_opl_last[0] = 0;
    g_opl_last[1] = 0;
    opl_reset();
}

void mixer_add_sample(const s16 *pcm, u32 frames, int rate, int volume, int loop,
                      const void *owner)
{
    if (pcm == NULL || frames == 0 || rate <= 0)
        return;
    if (volume < 0) volume = 0;
    if (volume > MIXER_MAX_VOLUME) volume = MIXER_MAX_VOLUME;
    for (int i = 0; i < MIXER_VOICES; i++) {
        if (g_voices[i].active) continue;
        mixer_voice *v = &g_voices[i];
        v->pcm = pcm;
        v->owner = owner;
        v->frames = frames;
        v->rate = rate;
        v->volume = volume;
        v->loop = loop;
        v->active = 1;
        v->idx = 0;
        v->frac = 0;
        v->step = 0;
        return;
    }
}

void mixer_stop_sample(const void *owner)
{
    for (int i = 0; i < MIXER_VOICES; i++)
        if (g_voices[i].active && g_voices[i].owner == owner)
            g_voices[i].active = 0;
}

void mixer_stop_samples(void)
{
    for (int i = 0; i < MIXER_VOICES; i++)
        g_voices[i].active = 0;
}

int mixer_active_voices(void)
{
    int n = 0;
    for (int i = 0; i < MIXER_VOICES; i++)
        if (g_voices[i].active) n++;
    return n;
}

/* Renders the OPL frame at the current phase into g_opl_last, then advances the
 * phase. Nearest neighbour: the frame read is floor(phase), and the core never
 * runs out, so the read is always valid. */
static void opl_advance(void)
{
    while (g_opl_rendered <= g_opl_idx) {
        opl_render(g_opl_last, 1);
        g_opl_rendered++;
    }
    g_opl_frac += g_opl_step;
    g_opl_idx += g_opl_frac >> 16;
    g_opl_frac &= 0xffff;
}

/* Nearest-neighbour read at the voice's phase, then phase advance. Wraps when
 * looping, stops the voice when one-shot; either way idx < frames at the read,
 * so the buffer is never overrun. */
static s16 voice_read(mixer_voice *v)
{
    if (v->idx >= v->frames) {
        if (v->loop) {
            v->idx %= v->frames;
        } else {
            v->active = 0;
            return 0;
        }
    }
    s16 s = v->pcm[v->idx];
    v->frac += v->step;
    v->idx += v->frac >> 16;
    v->frac &= 0xffff;
    return s;
}

void mixer_render(s16 *out, u32 frames, u32 out_rate)
{
    if (out == NULL || out_rate == 0) return;
    g_opl_step = rate_step(MIXER_OPL_RATE, out_rate);
    for (int i = 0; i < MIXER_VOICES; i++)
        if (g_voices[i].active)
            g_voices[i].step = rate_step(g_voices[i].rate, out_rate);
    for (u32 f = 0; f < frames; f++) {
        opl_advance();
        s32 l = g_opl_last[0];
        s32 r = g_opl_last[1];
        for (int i = 0; i < MIXER_VOICES; i++) {
            mixer_voice *v = &g_voices[i];
            if (!v->active) continue;
            s32 s = (s32)voice_read(v) * v->volume / MIXER_UNITY_VOLUME;
            l += s;
            r += s;
        }
        out[f * 2] = sat16(l);
        out[f * 2 + 1] = sat16(r);
    }
}
