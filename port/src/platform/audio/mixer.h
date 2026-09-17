/* Software mixer: sums the OPL FM core's stereo output and the active sample
 * voices into interleaved stereo frames.
 *
 * PORT: this whole subsystem is invented. In the original the SB16.DIG driver
 * mixed sample voices in hardware and the SBPRO2.MDI FM driver drove a separate
 * OPL chip; game code never summed audio (port/spec/audio.md "AIL surface").
 * A single software mixer is the port's replacement for both. Its arithmetic —
 * per-voice Q8 volume, nearest-neighbour resampling, saturating s16 output — has
 * no original counterpart.
 *
 * Voice pool: four, matching the four AIL sample handles the original allocates
 * (port/spec/audio.md "AIL surface" row 10: FUN_0001cf40 allocates 0x60 == 4 *
 * 0x18 bytes). A fifth concurrent add is dropped.
 *
 * TODO(verify): the original's behaviour on voice exhaustion is NOT established.
 * Row 10 shows only the four-handle allocation; it does not say whether the
 * driver dropped the add, stole a playing handle (AIL note stealing), or
 * errored (row 10 does record an "Out of sample handles" return, but not
 * whether any game call site reaches it). This port drops; that policy is a
 * guess, not a reproduction. What would settle it: the S16.DIG / SBPRO.DIG /
 * SBLASTER.DIG driver decompilation ("Out of sample handles" path), or a game
 * call site that adds a fifth sample while four are active.
 *
 * Volume: Q8 fixed point, clamped to [0, 1024]. 256 is unity gain, 0 is silent,
 * negative values clamp to 0; above 256 amplifies and may saturate.
 *
 * pcm: mono s16, `frames` samples. A stereo render duplicates it to both
 * channels. `loop` nonzero loops at the end; zero stops the voice there. The
 * mixer references this buffer, never copies it, so the caller must keep `pcm`
 * valid and unchanged for as long as the voice is active — until it stops on
 * its own, is stopped, or its pool slot is reused.
 *
 * Resampling: one policy for every source, the OPL core included — nearest
 * neighbour off a 16.16 phase accumulator. A voice whose rate differs from
 * out_rate never reads outside its own buffer.
 */
#ifndef PR_MIXER_H
#define PR_MIXER_H

#include "types.h"

/* The rate the mixer renders at, and therefore the rate the frame loop opens the
 * audio device with: the OPL core's native sample rate. PORT: mirrored from
 * OPAL_OPL3_SAMPLE_RATE (49716) in opl/opal/opal.h. That header stays private to
 * opl.c, so the core's rate is exported here rather than through opl.h; the
 * frame loop must not duplicate it. TODO(verify): if the vendored core rate ever
 * changes, this constant must change with it. */
#define MIXER_OPL_RATE 49716

/* Clears every voice and resets the OPL core, so "nothing playing" is exact
 * silence even with no register writes. */
void mixer_reset(void);

/* Starts a voice. Ignores pcm == NULL, frames == 0 or rate <= 0. `volume` is
 * Q8 (see above); `loop` selects one-shot vs looping. `owner` identifies the
 * caller's sample handle, so mixer_stop_sample(owner) can stop just this
 * caller's voices and never another's. Dropped when all four voices are busy —
 * a port choice; the original's exhaustion policy is unverified (see TODO
 * above). */
void mixer_add_sample(const s16 *pcm, u32 frames, int rate, int volume, int loop,
                      const void *owner);

/* Stops every active voice owned by `owner`. A voice whose `owner` slot was
 * reused after the original ended on its own is left alone: matching on the
 * owner pointer is stale-safe, unlike a stored voice index. */
void mixer_stop_sample(const void *owner);

/* Stops every active voice. */
void mixer_stop_samples(void);

/* Renders `frames` stereo frames into `out` as interleaved s16 (left, right).
 * out == NULL or out_rate == 0 fills nothing and renders nothing. */
void mixer_render(s16 *out, u32 frames, u32 out_rate);

#endif /* PR_MIXER_H */
