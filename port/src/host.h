/* SDL3 host: window, event pump, tick clock, frame presentation, file I/O.
 * SDL headers are included only in host.c and main.c; every other module talks
 * to the host through these declarations. host_pump() is safe before host_init()
 * (and without any display), so the test suite runs headless. */
#ifndef PR_HOST_H
#define PR_HOST_H

#include "types.h"

/* PORT: a host stall (resume from sleep, NTP step, debugger pause) must not
 * cost one loop iteration per missed interval. Beyond this many intervals the
 * port skips the lost time instead of replaying it. 30 intervals = 0.5 s.
 * Exported so the audio service shares this bound instead of inventing a
 * second catch-up policy. */
#define HOST_TICK_MAX_CATCHUP 30u

/* Opens a `w`x`h` window titled `title` and initialises SDL video. Returns 1 on
 * success, 0 on failure (no display, no video driver); never aborts. On failure
 * the rest of the host stays a safe no-op: host_pump() still advances the tick
 * and host_present_rgb() does nothing. */
int  host_init(const char *title, int w, int h);

/* 1 while a window is open. Returns 0 before host_init(), after
 * host_shutdown(), and when host_init() failed to open one. Headless callers
 * (the test suite, `--check`) use this to skip real-time waits: with no window
 * there is nothing to display, so pacing on the wall clock would only burn
 * verification time. No effect on any existing call. */
int  host_has_window(void);

/* Closes the window and shuts SDL down. Safe to call more than once, and after
 * a failed host_init(). */
void host_shutdown(void);

/* Runs one host tick: drains SDL events (translating key-down events to
 * input_push()), presents a submitted frame if one is ready, then advances the
 * host tick clock by however many nominal 60 Hz retrace intervals have elapsed.
 * Safe before host_init() and with no window. */
void host_pump(void);

/* 1 once the user has closed the window (SDL_EVENT_QUIT), else 0. The window
 * close is the host's own quit signal, not a game key: the port used to push an
 * ESC into the int 16h queue, but a movie drains that queue and only aborts on
 * ESC, so the request was swallowed and the game kept running. The loops that
 * own the game's quit flag read this instead. Cleared by host_shutdown(). */
int  host_quit_requested(void);

/* Sleeps to the next 60 Hz tick boundary, then runs host_pump(). Maps the
 * original's `in(0x3DA) & 8` VBlank spin. Safe with no window (SDL_Delay before
 * SDL video init is fine). */
void host_wait_vblank(void);

/* PORT: test seam, no raw counterpart. When set, host_pump() calls fn(ctx)
 * once at the end of every call. The blocking menu loops reach the host only
 * through config_screen_wait's host_wait_vblank(); the unit tests script one
 * input per presented frame through this hook. NULL (the default) disables it,
 * and nothing outside the tests sets it. */
typedef void (*host_pump_hook_fn)(void *ctx);
void host_set_pump_hook(host_pump_hook_fn fn, void *ctx);

/* PORT: the end of the run on a CPU exception the original leaves to
 * DOS/4GW's default handler (no program handler: record named-gaps-b §B.4).
 * With a hook installed (tests) the hook is called and must not return.
 * Otherwise prints `msg`, or when NULL a line naming `exc` and `eip`, to
 * stderr, shuts the host down and exits with `status`. */
_Noreturn void host_cpu_fault(u32 exc, u32 eip, const char *msg, int status);
typedef void (*host_fault_hook_fn)(u32 exc, u32 eip);
/* PORT: test seam; returns the previous hook. */
host_fault_hook_fn host_set_fault_hook(host_fault_hook_fn hook);

/* Hands a w*h RGB24 frame (3 bytes per pixel, row-major) to the host. The frame
 * is presented by the next host_pump(). A no-op when no window is open. */
void host_present_rgb(const u8 *rgb, int w, int h);

/* Host tick counter, the model for the original's DAT_00105D88. Only the host
 * advances it; callers read it. 0 until the first host_pump(). */
u32  host_tick_count(void);

/* Reads up to `max` bytes from `path` into `dst`, setting `*len_out` to the
 * count read. Returns 1 on success, 0 on failure (missing file, or file larger
 * than `max`). */
int  host_read_file(const char *path, u8 *dst, u32 max, u32 *len_out);

/* Writes `len` bytes from `src` to `path` (truncating it). Returns 1 when every
 * byte was written, else 0. */
int  host_write_file(const char *path, const u8 *src, u32 len);

/* The save file (the original's "cmos", 0xA2C5C). PORT: the original opens it
 * in the current directory; here the caller names a save path (NULL = none)
 * and a read-only fallback directory, the game dir (NULL = none). */
void host_save_set_path(const char *path);
void host_save_set_fallback(const char *dir);
/* Sets the save path to "cmos" in SDL's per-user directory for this app
 * (SDL_GetPrefPath, which creates it); no save path when SDL has none. */
void host_save_set_default_path(void);
/* Reads `len` bytes: from the save path when that file exists (a shorter file
 * is a failed read; a longer one gives its first `len` bytes, as the raw's
 * fread does), else from `cmos` or `CMOS` in the fallback directory. Returns 1
 * on success, else 0. */
int  host_save_read(u8 *dst, u32 len);
/* 1 when the save path can be opened for writing (it never truncates and
 * leaves no empty file behind). */
int  host_save_writable(void);
/* Writes `len` bytes to the save path only. Returns 1 when every byte was
 * written, else 0 (no save path included). */
int  host_save_write(const u8 *src, u32 len);

/* Opens the default playback device for `channels`-channel s16 frames at `rate`
 * Hz, matching opl_render()'s stereo-interleaved output. Returns 1 on success,
 * 0 on failure (non-positive rate/channels, or no audio device); never aborts,
 * and safe to call headless. */
int  host_audio_open(int rate, int channels);

/* Closes the audio seam and releases the device. Idempotent, and safe after
 * host_shutdown(); the seam is then a no-op and host_audio_rate() reads 0. */
void host_audio_close(void);

/* Queues `frame_count` interleaved s16 frames for playback. A no-op when no
 * device is open, or when `frames` is NULL / `frame_count` <= 0. */
void host_audio_submit(const s16 *frames, int frame_count);

/* Device sample rate in Hz, or 0 when no device is open. */
u32  host_audio_rate(void);

/* Human-readable reason the last host_audio_open() failed (SDL's own error
 * where one applies), so the caller can report the real failure instead of
 * assuming "no device". Empty before any open attempt. */
const char *host_audio_error(void);

/* PORT: the 16 input bits the game's bitfield carries, packed as the two key
 * bytes at DAT_00101514 + 0x2d8/0x2d9 (kb word (+0x2D8 << 8) | +0x2D9): the
 * SDL keys pressed, through host_kb_bit. */
u16 host_key_bits(void);

/* PORT: one set-1 scan's kb bit under the binding the game's sampler 0x1BBAC
 * reads (the key-config block at [DS_00101514], +0x2DE..+0x2ED by position,
 * record gameplay-ground-truth §G.1.2); the game's default binding (P1
 * S X Z C / U I N M / F1, P2 the arrows / Home PgUp End PgDn / F2) when no
 * run has filled the block; 0 for any other scan. */
u16 host_kb_bit(u8 scan);

/* PORT: fills the key-state table at [DS_00101514]+0x254 the six ported
 * keyboard readers (input_dirs_p1 .. input_f2) sample, from the same 16 bits
 * host_key_bits() reports (SDL's keyboard through the configured binding, or
 * host_set_key_bits_override's captured bitmap). Every configured slot is
 * written, whatever scan it names (record 2026-10-21-host-input-truth §H2), and
 * the fixed F1/F2 entries +0x28F/+0x290; the default record's scans stand in
 * when the block is absent or all zero. A no-op without a usable base. */
void host_input_fill_keyboard(void);

/* PORT: test seam, no raw counterpart. With `on` set, host_key_bits() returns
 * `bits` instead of the SDL keyboard state; the K11 oracle driver holds the key
 * bitmap a capture's poll log recorded through it (named-gaps A record §A.3).
 * Off by default; nothing outside the tests sets it. */
void host_set_key_bits_override(u16 bits, int on);

/* PORT: the PC game port's button byte, port 0x201's bits 4..7, the host's
 * answer to the three ported readers 0x1B890/0x1B8DC/0x1B908 (record
 * 2026-10-21-host-input-truth §H3). A game port's button inputs are active low:
 * a pressed button grounds its bit, and the readers set their game bit when the
 * port bit is CLEAR. The pad model fills it in host_pad_fill(); with no pad it
 * answers 0. That 0 would read as all four buttons held, so no game-port arm
 * may sample it: the device word that selects one is cleared when no pad
 * answers (0x4FBBB, host_port_present, §H6) before the sampler's arms run. */
u8 host_port_buttons(void);

/* PORT: fills the four normalized axis words (0xE1C1E/0xE1C20/0xE1C26/0xE1C28)
 * the ported axis readers 0x4FF8F/0x4FFD8 sample (record 2026-10-21-host-input-
 * truth §H4). The no-pad state is each word at its calibrated center
 * (0xE1C22/0xE1C24/0xE1C2A/0xE1C2C), so every axis reads its dead band. Task 5's
 * host_pad_fill writes the pad's displacement over it. */
void host_input_fill_axes(void);

/* ---- PORT: the SDL gamepad model (record 2026-10-21-host-input-truth §H5) ----
 * The host owns the game port: the pad is not part of the raw's game logic
 * (0x2D2F0 is `xor eax,eax; ret`), and the sampler's device 2/4/6 arms read
 * the host's game-port byte and the four normalized axis words. The host keeps
 * one SDL gamepad's state and, each sampler tick, writes the axis words for the
 * players whose device word selects a joystick arm and the active-low button
 * byte. Device 0 ignores the pad: its arm reads the keyboard table only. A
 * joystick arm replaces that player's keyboard direction/button nibble (device
 * 6 keeps only the keyboard *button* scans; the sampler's arms do the select).
 *
 * host_pad_open() opens the first SDL gamepad in host_init (a failure leaves
 * the pad absent and never aborts); host_pad_close() is host_shutdown's
 * teardown. host_pad_poll() samples the pad and input_sampler() calls it once
 * per tick. host_pad_fill() writes the game's axis words and port byte from the
 * sampled state: the four axes are X1 Y1 X2 Y2 (axes A = the first stick, axes
 * B = the second), the four game buttons are 1/2/4/8, mapped from the face
 * buttons south/east/west/north. The SDL int16 axis is scaled to the raw's
 * normalized unit by HOST_PAD_AXIS_SCALE (a declared host policy, host.c): full
 * deflection is 0x1FF, so the raw's ±0x1E dead band is about 6% of travel.
 * With no pad the fill leaves the axis words at their centers and the game-port
 * byte at 0; a joystick device word is cleared on the next tick by the
 * sampler's presence block (host_port_present, §H6), so that 0 is never read by
 * an arm. */
void host_pad_open(void);
void host_pad_close(void);
void host_pad_poll(void);
void host_pad_fill(void);

/* PORT: the centre capture, 0x5004A (record 2026-10-21-host-input-truth §H6).
 * The raw waits until each axis reads >= +0x1E and stores the reading as that
 * axis's center (0xE1C1E.. -> 0xE1C22..), skipping a player's pair whose poll
 * (0x4FC05/0x4FCFD) answers 0; its 0x50021 PIT-period word 0xE1C1C is stored
 * once, only while it is 0. The host's reading is the axis word itself, whose
 * rest value is the center (a pad at rest is its own center), so a capture
 * with no pad moves nothing. Called at pad connect (host_pad_open), from
 * game_init's 0x1C0AC, and on Alt-J (0x24DD3's and 0x2EB4D's 0x5004A). */
void host_pad_center(void);

/* PORT: test seam, no raw counterpart. While armed, host_pad_poll() does
 * nothing and host_pad_fill() reads `axes` (four s8 normalized displacements,
 * X1 Y1 X2 Y2) and `buttons` (four game buttons, nonzero = pressed) instead of
 * the SDL pad; `present` is the pad's presence. (NULL, NULL, 0) disarms it.
 * Off by default; nothing outside the tests sets it. */
void host_set_pad_override(const u8 *axes, const u8 *buttons, int present);

/* PORT: the game port's presence (record 2026-10-21-host-input-truth §H6). The
 * raw's 0x4FBBB(probe) waits for the game port's axis one-shot bits to clear,
 * triggers them with `out 0x201,0` and waits again: a fan-out answers 1, a
 * quiet port 0. The host owns that answer, so 1 means the pad is attached and
 * 0 means no pad -- the state the sampler clears a joystick device word in
 * (0x1BC6F the both-player arm, 0x1BCB4/0x1BCBF the P1-only arm's two words,
 * 0x1BCE6 the P2-only arm). The `probe` mask is the raw's argument: 3 is
 * P1's axis pair (port bits 0/1) and 0xC P2's (bits 2/3); one pad supplies
 * both sticks, so a present pad answers any axis mask. The raw tests the mask
 * through BL alone (`test bl,al`), so a mask whose low byte is 0 tests no bit
 * at all and answers 1 whatever the hardware (0x4FBCD/0x4FBEE; 0x4FBBB(0x100)
 * too). host_set_port_present_override still wins. */
int host_port_present(int probe);

/* PORT: the host's answer to the ISR sampler's presence block (0x1BBAC's
 * 0x4FDF5/0x4FC05/0x4FCFD polls and 0x4FBBB probe; the routines stay
 * host-owned, record §49-V). 1 means the game port answered, 0 means it did NOT
 * (the sampler then clears the device word: 0x1BC6F the both-player arm,
 * 0x1BCB4/0x1BCBF the P1-only arm's two words, 0x1BCE6 the P2-only arm). Each
 * answers through host_port_present() with the mask its raw routine reads
 * (0xF/3/0xC). The seam pins each answer for the differential rows. */
int host_port_poll_both(void);
int host_port_poll_p1(void);
int host_port_poll_p2(void);
int host_port_probe(u32 mask);

/* PORT: test seam, no raw counterpart. While armed, the four presence answers
 * above return `present` instead of the pad's; the unit test uses it to
 * exercise the sampler's device 2/4/6 arms with no pad attached. Off by
 * default; nothing outside the tests sets it. */
void host_set_port_present_override(int present, int on);

/* PORT: test seam, no raw counterpart. With `on` set, host_port_buttons()
 * answers `buttons` instead of the pad's byte; the differential rows and the
 * unit test pin the byte through it, and Task 5's fill keeps writing the pad's
 * own byte (`g_port_buttons`) beside it. Off by default (the pad's byte is 0
 * with no pad); the test disarms it again. */
void host_set_port_buttons_override(u8 buttons, int on);

#endif /* PR_HOST_H */
