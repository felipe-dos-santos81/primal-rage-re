/* VGA DAC and frame presentation. gfx_flush_palette ports 0x1C470: it drains
 * the 4x u32 dirty-list at DS_00107498 whose records { ptr/handle; first index;
 * count; handle flag } describe palette ranges to push through the DAC. The
 * original writes the VGA ports 0x3C8/0x3C9; the port fills gfx_dac[][] instead
 * so gfx_present can convert 8-bit indices to RGB and hand a frame to the host
 * seam (host.h). */
#ifndef PR_GFX_H
#define PR_GFX_H

#include "types.h"

/* 256 DAC entries, { R, G, B }, each an 8-bit gun 0..255 (the original's
 * `word >> 2`); the real VGA DAC is 6-bit, but the port keeps the full byte. */
extern u8 gfx_dac[256][3];

/* Drains the dirty-list at DS_00107498 up to the head pointer DS_00107798, then
 * resets the head to the base. A record with a non-zero byte flag stores a
 * resource handle in [0] that res_resolve() expands; its colour words start 4
 * bytes past the resolved data. A record's range is clamped so first+count does
 * not pass 0x100. Consumed records are marked first = -1. */
void gfx_flush_palette(void);

/* Converts w*h bytes of palette indices through gfx_dac into RGB24 and hands the
 * frame to host_present_rgb(). */
void gfx_present(const u8 *indices, int w, int h);

/* Single mapping point for the original's `in(0x3DA) & 8` VBlank spin. */
void gfx_wait_vblank(void);

#endif /* PR_GFX_H */
