/* Sample loading: turns the shipped PCM bytes into a descriptor the sample-play
 * AIL path can hand to the mixer.
 *
 * The shipped data is a RIFF/WAVE blob inside S16SOUND.GRA (port/spec/audio.md
 * "Samples"): WAVE_FORMAT_PCM, mono, 11025 Hz, 8-bit unsigned. Although the
 * driver profile installs SB16.DIG (16-bit DMA), the payload itself is 8-bit
 * (verified: the `fmt ` chunk reads tag 1, 1 channel, 0x2b11 Hz, 8 bits). The
 * parser therefore accepts exactly that one shape and rejects everything else.
 *
 * PORT: parsing the RIFF container is a port invention. The original loaded
 * samples through the Miles AIL sample subsystem (AIL_set_sample_address /
 * AIL_set_sample_type / AIL_set_sample_rate, port/spec/audio.md "AIL surface"
 * rows 13, 14, 17) with the driver owning the container format. This port reads
 * the one RIFF blob Task 1 located; it does not reproduce AIL's loader.
 *
 * pcm: unsigned 8-bit mono PCM borrowed directly from the `data` passed to
 * samples_load — never copied — so the caller must keep `data` valid and
 * unchanged for as long as it uses the descriptor (until the voice that
 * references it stops and its mixer pool slot is reused; see
 * mixer_add_sample). Mono-to-stereo duplication is the mixer's job; this type
 * stays mono.
 */
#ifndef PR_SAMPLES_H
#define PR_SAMPLES_H

#include "types.h"

typedef struct {
    const u8 *pcm;  /* borrowed 8-bit unsigned mono PCM; byte 128 is centre */
    u32 frames;     /* PCM frames (== bytes, 8-bit mono) */
    u32 rate;       /* frames per second */
    int channels;   /* 1 = mono (the only accepted shape) */
} SampleVoice;

/* Parses the RIFF/WAVE blob at `data` (length `len`) into `out`. Returns 1 on a
 * supported sample, 0 on anything else. On 0, `*out` is left zeroed, never
 * half-filled. Reads are bounded by `len`: a chunk whose declared body runs past
 * the buffer is rejected rather than followed. out == NULL also returns 0. */
int samples_load(const u8 *data, u32 len, SampleVoice *out);

/* The one 8-bit-unsigned -> s16 conversion in the port. The sample-play path
 * (AIL_start_sample) calls it with its own buffer; the mixer only ever sees
 * s16. `pcm` is 8-bit unsigned mono, byte 128 is centre; `out` is caller-owned
 * and must hold `frames` s16. Returns the number of frames written (0 when
 * either pointer is NULL). */
u32 samples_to_s16(const u8 *pcm, u32 frames, s16 *out);

#endif /* PR_SAMPLES_H */
