/* The AIL (Miles) audio call surface the game makes.
 *
 * The original linked the Miles AIL 3.02 engine plus a runtime-loaded .DIG/.MDI
 * driver and crossed into them through real-mode/DPMI reflection. The port
 * cannot run those binaries, so every call the game makes into the sound-library
 * block is reimplemented here against the port's own modules:
 *
 *     sequencer.c   XMIDI event stream -> OPL register writes (music)
 *     samples.c     PCM decode + the one u8 -> s16 conversion
 *     mixer.c       sample voices + OPL -> stereo frames
 *
 * One C function per AIL call in port/spec/audio.md "AIL surface (Task 3)".
 * Signatures are the game's call-site signatures; return codes keep the
 * original's meaning (AIL_sequence_status 4 = playing / 2 = stopped, an
 * allocation failure returns 0 / -1). AIL names are the closest Miles AIL 3.02
 * matches and some are inferred (spec rows 14, 19, 24); the original address in
 * each function's header comment is the durable identity.
 *
 * PORT: no file I/O and no asset resolution here, on purpose. The caller
 * (game/flow.c) resolves DIG.INI / MDI.INI / FAT.OPL / the XMI bank through the
 * resource layer and passes bytes/handles in, so this file stays testable
 * without the game assets. SDL is never touched: mixed frames leave through
 * mixer_render() and the host's audio seam.
 *
 * PORT: fixed audio profile, no hardware probe. The original probes the card
 * and falls back SB16.DIG -> SBPRO.DIG -> SBLASTER.DIG. The port presents the
 * shipped SBPRO2-FM + SB16-sample profile unconditionally and succeeds on the
 * first driver file, so the probe/fallback path is removed while the later
 * calls behave as on the selected hardware.
 *
 * PORT: the original's real-mode/DPMI crossings into the loaded driver (AIL's
 * dispatcher 0x5d973) become direct C calls into sequencer/samples/mixer. Each
 * replacement is marked inside ail.c and listed in the Task 10 report.
 */
#ifndef PR_AIL_H
#define PR_AIL_H

#include "types.h"

/* Opaque handles. The originals are pointers into the AIL engine; the port
 * hands back pointers to its own static bookkeeping, so a non-NULL handle is
 * the success signal exactly as before. */
typedef struct AIL_DIG_DRIVER *HDIGDRIVER;
typedef struct AIL_MDI_DRIVER *HMDIDRIVER;
typedef struct AIL_SAMPLE *HSAMPLE;
typedef struct AIL_SEQUENCE *HSEQUENCE;

/* Timer handle. The original's is the byte offset into its timer table
 * (0, 4, ... 0x3c); the port returns the slot index. -1 means "no handle". */
typedef s32 HTIMER;
typedef void (*AIL_TIMER_CB)(void);

/* ---- engine lifecycle / preferences (spec rows 1-3) -------------------- */

/* 0x5d851 — spec audio.md "AIL surface" (row 1). Installs the 18 default
 * preferences. No args, no return. */
void AIL_startup(void);

/* 0x5d86a — spec audio.md "AIL surface" (row 2). Releases the drivers, the
 * timers, and every sample handle. */
void AIL_shutdown(void);

/* 0x5d87e — spec audio.md "AIL surface" (row 3). Swaps prefs[preference] and
 * returns the old value, or -1 for an out-of-range index. */
s32 AIL_set_preference(u32 preference, u32 value);

/* ---- timers (spec rows 4-7) -------------------------------------------- */

/* 0x5da12 — spec audio.md "AIL surface" (row 4). Returns a timer handle, or -1
 * when every slot is taken ("Out of timer handles"). */
HTIMER AIL_register_timer(AIL_TIMER_CB callback);

/* 0x5da87 — spec audio.md "AIL surface" (row 5). Stores 1000000/hz microseconds
 * as the period (0xfa = 250 Hz, 0x3c = 60 Hz at the game's call sites). */
void AIL_set_timer_frequency(HTIMER timer, u32 hz);

/* 0x5daa6 — spec audio.md "AIL surface" (row 6). Marks the timer running.
 * PORT: there is no PIT/ISR here, so the callback is never fired by this
 * module; the frame loop paces the game and calls seq_tick itself. */
void AIL_start_timer(HTIMER timer);

/* 0x5dadc — spec audio.md "AIL surface" (row 7). Frees the timer slot. */
void AIL_release_timer_handle(HTIMER timer);

/* ---- driver install (spec rows 8, 9, 25) ------------------------------- */

/* 0x5db7c — spec audio.md "AIL surface" (row 8). PORT: fixed audio profile, no
 * hardware probe — the original opens DIG.INI and parses DRIVER=/IO_ADDR/DMA_*;
 * the port returns the fixed DIG handle without opening anything. */
HDIGDRIVER AIL_install_DIG_INI(void);

/* 0x5db9e — spec audio.md "AIL surface" (row 9). PORT: fixed audio profile, no
 * hardware probe — the named file is never opened; the first install succeeds
 * so the SB16.DIG -> SBPRO.DIG -> SBLASTER.DIG fallback chain stops. */
HDIGDRIVER AIL_install_DIG_driver_file(const char *filename, const void *addr);

/* 0x5ddd0 — spec audio.md "AIL surface" (row 25). PORT: fixed audio profile, no
 * hardware probe — the MDI.INI open is replaced by the fixed MDI handle. */
HMDIDRIVER AIL_install_MDI_INI(void);

/* ---- sample handles (spec rows 10-24) ---------------------------------- */

/* 0x5dbcb — spec audio.md "AIL surface" (row 10). Finds a free slot on the
 * driver and returns it, or 0 when all four are taken ("Out of sample
 * handles"). */
HSAMPLE AIL_allocate_sample_handle(HDIGDRIVER driver);

/* 0x5dbf4 — spec audio.md "AIL surface" (row 11). Frees the sample slot. */
void AIL_release_sample_handle(HSAMPLE sample);

/* 0x5dc0f — spec audio.md "AIL surface" (row 12). Resets the sample (state 2,
 * volume/loop defaults, default rate 0x2b11 = 11025). */
void AIL_init_sample(HSAMPLE sample);

/* 0x5dc2a — spec audio.md "AIL surface" (row 13). Stores the sample buffer
 * reference and length; the bytes are referenced, never copied. */
void AIL_set_sample_address(HSAMPLE sample, const void *buf, u32 len);

/* 0x5dc4d — spec audio.md "AIL surface" (row 14). Stores the 0..3 format code
 * and flag from the sample record (name inferred). */
void AIL_set_sample_type(HSAMPLE sample, s32 format, u32 flag);

/* 0x5dc70 — spec audio.md "AIL surface" (row 15). Marks the sample playing
 * (state 4) and starts it. PORT: where the original issued driver call 0x401
 * (DMA start), the port converts the 8-bit sample and adds a mixer voice. */
void AIL_start_sample(HSAMPLE sample);

/* 0x5dc8b — spec audio.md "AIL surface" (row 16). Marks the sample stopped
 * (state 2) and stops this handle's voice only, matching the original's
 * per-handle stop (the port mixer's voice carries the handle as its owner). */
void AIL_stop_sample(HSAMPLE sample);

/* 0x5dca6 — spec audio.md "AIL surface" (row 17). Sets the sample rate. */
void AIL_set_sample_rate(HSAMPLE sample, u32 rate);

/* 0x5dcc5 — spec audio.md "AIL surface" (row 18). Sets the sample volume,
 * clamped to 0..0x7f. */
void AIL_set_sample_volume(HSAMPLE sample, s32 volume);

/* 0x5dce4 — spec audio.md "AIL surface" (row 19). Sets the loop count (the game
 * forces 0 = no loop; name inferred). */
void AIL_set_sample_loop_count(HSAMPLE sample, u32 count);

/* 0x5dd03 — spec audio.md "AIL surface" (row 20). Returns 2 stopped / 4 playing
 * (the game gates on != 4 and == 4), or 0 for a NULL/free handle. */
s32 AIL_sample_status(HSAMPLE sample);

/* 0x5dd2c — spec audio.md "AIL surface" (row 21. name TODO(verify)).
 * PORT: deferred stub — movie/Smacker streaming buffer sizing, sub-project 2b.
 * Returns 0. */
s32 AIL_sample_buffer_size(HDIGDRIVER driver, u32 rate, u32 len);

/* 0x5dd5d — spec audio.md "AIL surface" (row 22, name TODO(verify)).
 * PORT: deferred stub — movie/Smacker streaming, sub-project 2b. Returns -1
 * (idle), the original's "no half needs refilling". */
s32 AIL_stream_buffer_index(HSAMPLE sample);

/* 0x5dd86 — spec audio.md "AIL surface" (row 23, name TODO(verify)).
 * PORT: deferred stub — movie/Smacker streaming, sub-project 2b. No-op. */
void AIL_stream_feed(HSAMPLE sample, s32 half, const void *buf, u32 len);

/* 0x5ddad — spec audio.md "AIL surface" (row 24, name inferred).
 * PORT: deferred stub — movie/Smacker streaming, sub-project 2b. No-op. */
void AIL_register_sample_callback(HSAMPLE sample, u32 which, void (*cb)(HSAMPLE));

/* ---- sequences (spec rows 26-31) --------------------------------------- */

/* 0x5de1f — spec audio.md "AIL surface" (row 26). Allocates the single
 * sequence handle from the MDI driver, or 0 when it is already taken ("Out of
 * sequence handles"). */
HSEQUENCE AIL_allocate_sequence_handle(HMDIDRIVER driver);

/* 0x5de48 — spec audio.md "AIL surface" (row 27). Parses the FORM/CAT/XMID
 * bank at `data` and sets it up. Returns 1 on a usable bank, 0 on bad data
 * ("Invalid XMIDI sequence").
 * PORT: `sequence_num` keeps the original's meaning (the game passes 0); the
 * port ignores it and derives the bank length from the container
 * (seq_bank_size). No length argument is added. */
s32 AIL_init_sequence(HSEQUENCE sequence, const void *data, u32 sequence_num);

/* 0x5de79 — spec audio.md "AIL surface" (row 28). Silences and restarts the
 * sequence, marking it playing (state 4). */
void AIL_start_sequence(HSEQUENCE sequence);

/* 0x5deaf — spec audio.md "AIL surface" (row 29). All-notes-off and stop,
 * marking state 2. */
void AIL_stop_sequence(HSEQUENCE sequence);

/* 0x5deca — spec audio.md "AIL surface" (row 30). Sets the sequence target
 * volume and a fade time in ms. PORT: the port sequencer has no master-volume
 * stage, so the value is recorded but not applied; the fade is not modelled. */
void AIL_set_sequence_volume(HSEQUENCE sequence, s32 volume, u32 fade_ms);

/* 0x5deed — spec audio.md "AIL surface" (row 31). Returns 4 while the sequence
 * is playing, 2 once stopped or at the bank's end, 0 for a NULL/free handle. */
s32 AIL_sequence_status(HSEQUENCE sequence);

/* ---- no-ops (spec rows 32, 33) ----------------------------------------- */

/* 0x5dfdc — spec audio.md "AIL surface" (row 32, purpose unknown). Faithful
 * no-op: the original body is empty. Runs once when the driver refcount goes
 * 0 -> 1. */
void AIL_noop_5dfdc(void);

/* 0x5dfeb — spec audio.md "AIL surface" (row 33, purpose unknown). Faithful
 * no-op: the original body is empty. Runs once when the refcount goes 1 -> 0. */
void AIL_noop_5dfeb(void);

#endif /* PR_AIL_H */
