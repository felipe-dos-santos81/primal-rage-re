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

#endif /* PR_SPRITE_H */
