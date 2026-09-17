/* Music sequencer: turns the game's sequenced music data (XMIDI) into timed OPL
 * register writes.
 *
 * The original played music through the Miles AIL engine plus the loaded
 * SBPRO2.MDI FM driver. The driver binaries cannot run here, so this module
 * reproduces the part of that path the register trace exposes: it decodes the
 * XMIDI event stream, maps program changes through the FAT.OPL patch bank, and
 * emits the note/patch register writes the driver emitted. See
 * port/spec/audio.md ("Music event grammar", "FAT.OPL patch bank") for the byte
 * evidence and FORMATS.md for the container.
 *
 * Data in, no I/O. `seq_load` takes the XMI bank bytes; the caller resolves them
 * through the resource/file layer (same discipline as samples_load). Nothing
 * here opens a file, reads a GRA, or touches SDL.
 *
 * Tick. `seq_tick` advances the stream by exactly one driver tick. The tick is
 * SEQ_TICK_MS long, grounded in the captured OPL trace: aligning the
 * S16TITLE.GRA bank's note-on positions against the captured key-on times in
 * data/audio-captures/prage_000.dro gives one XMIDI delta unit == 8.333 ms
 * (120 Hz) over the first 17 notes (<= 3 ms error over 5.6 s), while 60 Hz and
 * 250 Hz do not fit. The capture command and the fit are in
 * port/spec/audio.md. TODO(verify): the same rate as declared by the loaded
 * SBPRO2.MDI at driver offset +0x2e during its 0x300 init (port/decomp/prage.c
 * FUN_00065b7b); the capture is behavioural evidence, not that field.
 */
#ifndef PR_SEQUENCER_H
#define PR_SEQUENCER_H

#include "types.h"

#define SEQ_TICK_MS (1000.0 / 120.0)

/* Parses the XMIDI bank at `data` (length `len`) into playable state. Returns 1
 * on a bank with a usable EVNT chunk, 0 on anything else (NULL, empty, a
 * truncated FORM/CAT/XMID/EVNT). On 0 the previously loaded bank is left
 * unchanged. Does not start playback. */
int seq_load(const u8 *data, u32 len);

/* Advances the stream by exactly one driver tick (SEQ_TICK_MS) while playing:
 * releases expired notes, then, when the pending delta has elapsed, processes
 * the next event group and emits its register writes. No-op when not playing. */
void seq_tick(void);

/* Rewinds the loaded bank to its start and begins playback. No-op if nothing is
 * loaded or the load failed. */
void seq_start(void);

/* Keys off every sounding channel and stops. Safe to call when stopped. */
void seq_stop(void);

/* Number of OPL channels currently keyed on (0 when stopped, nothing loaded, or
 * after the last note's duration expires). This is the port's "is a track
 * sounding" observable; it is not an AIL sequence status code. */
int seq_active_track(void);

/* Whether the loaded bank is currently playing: 1 from seq_start until seq_stop
 * or the bank's end meta halts it, 0 otherwise. The AIL surface's
 * AIL_sequence_status reads this for the original's 4 (playing) / 2 (stopped)
 * codes; seq_active_track cannot stand in for it, because it is 0 between
 * notes. */
int seq_playing(void);

#endif /* PR_SEQUENCER_H */
