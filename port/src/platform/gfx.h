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

/* PORT: 0x336C0. Resets the palette dirty-list head, marks every record unused
 * and clears the DAC. Owned here because gfx_flush_palette() owns the same
 * dirty-list; flow.c's game_init and actors_reset() (0x2BAF4) both call it. */
void palette_list_init(void);

/* PORT: 0x33734. Appends a raw palette dirty-list record
 * { ptr; first; count; flag } at the DS_00107798 head and advances the head.
 * Moved here from flow.c so the dirty list has one owner: flow.c's game_init
 * enqueue and actors.c's 0x33754 palette acquire both call it. */
void palette_record(u32 ptr, u32 first, u32 count, u32 flag);

/* PORT: the VGA aperture (0xA0000), the 320x200 screen. Two original writers
 * target it directly: the loader's text blit (0x51ED8, `add edi, 0xa0000`) and
 * the master loop's copy (0x25680) — the renderer's blit (0x51E5C) targets the
 * back buffer DS_000E87A4 instead. gfx_present models the copy: it writes the
 * index buffer into the aperture, then converts the aperture through gfx_dac.
 * The loader's text blit draws into this buffer (see sprite_blit_at). */
u8 *gfx_aperture(void);

/* Writes w*h bytes of palette indices into the aperture (when 320x200) and hands
 * the aperture converted through gfx_dac to host_present_rgb(). */
void gfx_present(const u8 *indices, int w, int h);

/* PORT: the aperture after the last gfx_present(), or NULL before the first.
 * Models the VGA aperture, which holds its content when the master loop's
 * gate fails. The dump drivers present this so a held frame is the last
 * presented one, not the freshly zeroed back buffer. */
const u8 *gfx_display(void);

/* Single mapping point for the original's `in(0x3DA) & 8` VBlank spin. */
void gfx_wait_vblank(void);

#endif /* PR_GFX_H */
