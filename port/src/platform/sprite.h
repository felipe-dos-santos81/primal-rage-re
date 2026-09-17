/* Display node for the sprite compositor. 0x14268 builds a SpriteNode from a
 * 16-bit sprite id: it masks the id to 0x7FFF, resolves the descriptor through
 * the handle table (gra_sprite_lookup), encodes the header's dimension sign and
 * the id's hflip bit into the type field, and mirrors the X pivot on hflip. The
 * span blitter 0x51E5C consumes a finished node; it is added with the renderers
 * in later tasks. The node's screen position, palette pointer and clip fields
 * are NOT set by the builder — render_list and the caller fill them. */
#ifndef PR_SPRITE_H
#define PR_SPRITE_H

#include "types.h"

typedef struct {
    int x, y, xorg, yorg;
    u32 type;
    int rows, width;
    u32 pal_ptr;
    u32 pixel_handle;
    int clip_l, clip_r, clip_t, clip_b;
    int stride, row;
} SpriteNode;

/* PORT: 0x14268. Fills the size, pivot and type fields of *n from the
 * descriptor for `sprite_id`. `sprite_id == 0` and any id whose descriptor does
 * not resolve zero rows/width/xorg/yorg and leave the rest untouched. The
 * hflip bit 0x8000 sets type bit 3 and mirrors the X pivot as
 * width - xorg - 1; a negative header height selects the raw base (type bit 2)
 * and negates both dimensions. */
void sprite_node_build(SpriteNode *n, u32 sprite_id);

/* The bank byte's effect on the source index. The original indexes a table at
 * DS_00081310 by the palette descriptor's bank byte; entries 1..255 hold
 * (b-1) replicated across the dword, so the byte-level effect is a plain -1.
 * Entry 0 is NOT replicated: it holds the stale code pointer 0x0005D110, whose
 * bytes differ, so the original's dword add would give each pixel of a 4-pixel
 * group a different offset — position-dependent garbage and a latent defect,
 * not a palette operation. A byte-wise port cannot reproduce an
 * alignment-dependent dword carry, so bank byte 0 maps to no offset. */
u8 sprite_bank_offset(u8 bank_byte);

/* PORT: 0x51E5C is its only original caller. Resolves `pal_ptr` through
 * res_resolve and maps the palette descriptor's bank byte through
 * sprite_bank_offset, giving the source-index offset sprite_render_rle
 * consumes. A pointer that does not resolve yields no offset. */
u32 sprite_bank(u32 pal_ptr);

/* RLE-renders `rows` rows of `width` pixels from src into dst. Each destination
 * row starts `stride` bytes after the previous one (the original's 0x140 row
 * pitch), so the caller offsets dst to the sprite's screen x and the renderer
 * advances the rest of the row itself; `bank` is the source-index offset the
 * blitter computes via sprite_bank_offset (0 = identity, Task 4). Transparent
 * runs leave dst untouched, and runs longer than the remaining row are clipped
 * to it. Returns 0, or -1 if src/dst is NULL or a dimension is non-positive. */
int sprite_render_rle(const u8 *src, u8 *dst, int width, int rows,
                      int stride, u8 bank);

/* PORT: 0x58CBD. Raw (uncompressed) copy renderer: `rows` rows of `width`
 * bytes are copied from src to dst with the bank offset added byte-wise to
 * every pixel, advancing dst by `stride` per row and src by `width`. The
 * original maps both the unclipped raw type 0x02 and the clipped type 0x12
 * (RAW|CLIP) here, so clipping is the composite driver's job (destination
 * offset and reduced row count), never this renderer's — hence no clip
 * arguments. Returns 0, or -1 if src/dst is NULL or a dimension is
 * non-positive. */
int sprite_render_raw(const u8 *src, u8 *dst, int width, int rows,
                      int stride, u8 bank);

#endif /* PR_SPRITE_H */
