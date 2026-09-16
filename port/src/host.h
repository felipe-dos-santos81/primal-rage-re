/* SDL3 host: window, event pump, tick clock, frame presentation, file I/O.
 * SDL headers are included only in host.c and main.c; every other module talks
 * to the host through these declarations. host_pump() is safe before host_init()
 * (and without any display), so the test suite runs headless. */
#ifndef PR_HOST_H
#define PR_HOST_H

#include "types.h"

/* Opens a `w`x`h` window titled `title` and initialises SDL video. Returns 1 on
 * success, 0 on failure (no display, no video driver); never aborts. On failure
 * the rest of the host stays a safe no-op: host_pump() still advances the tick
 * and host_present_rgb() does nothing. */
int  host_init(const char *title, int w, int h);

/* Closes the window and shuts SDL down. Safe to call more than once, and after
 * a failed host_init(). */
void host_shutdown(void);

/* Runs one host tick: drains SDL events (translating key-down events to
 * input_push()), presents a submitted frame if one is ready, then advances the
 * host tick clock by however many nominal 60 Hz retrace intervals have elapsed.
 * Safe before host_init() and with no window. */
void host_pump(void);

/* Sleeps to the next 60 Hz tick boundary, then runs host_pump(). Maps the
 * original's `in(0x3DA) & 8` VBlank spin. Safe with no window (SDL_Delay before
 * SDL video init is fine). */
void host_wait_vblank(void);

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

#endif /* PR_HOST_H */
