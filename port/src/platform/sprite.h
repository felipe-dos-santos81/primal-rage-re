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

/* PORT: 0x51E5C. The span blitter: resolves the node's pixel handle and palette
 * bank, computes the destination from the current back buffer
 * (mem + DS_000E87A4 + DS_001088F8[y] + x) and dispatches on `n->type & 0x1F` to
 * the renderer the original's PTR_LAB_00080C8C selects. `rows - clip_b` rows are
 * drawn (clip_b is consumed here, never passed to a renderer); the node itself
 * is left intact, so it can be blitted again. A zero-size node, an unresolvable
 * pixel handle, or any type outside the ten live entries (including RAW+HFLIP,
 * type 0x0A, which the original leaves a stub) draws nothing. */
void sprite_blit(SpriteNode *n);

/* RLE-renders `rows` rows of `width` pixels from src into dst. Each destination
 * row starts `stride` bytes after the previous one (the original's 0x140 row
 * pitch), so the caller offsets dst to the sprite's screen x and the renderer
 * advances the rest of the row itself; `bank` is the source-index offset the
 * blitter computes via sprite_bank_offset (0 = identity, Task 4). Transparent
 * runs leave dst untouched, and runs longer than the remaining row are clipped
 * to it. Returns 0, or -1 if src/dst is NULL or a dimension is non-positive. */
int sprite_render_rle(const u8 *src, u8 *dst, int width, int rows,
                      int stride, u8 bank);

/* PORT: 0x5D28F (clipped) and 0x57FFB (clipped+hflip). RLE-renders the
 * `width`-pixel rows inside a clip window. `rows` has already had clip_b
 * subtracted by the blitter, so there is no clip_b here; `clip_t` whole rows at
 * the top are consumed without drawing and `clip_l`/`clip_r` columns each side
 * are the window overhangs. The visible window is `width - clip_l - clip_r`
 * columns wide and is written sequentially from dst[0] -- the blitter's dst
 * already points at the window's first visible column, because render_list
 * clamped node.x inward to the clip edge. `mirror` reverses the window
 * horizontally (0x57FFB). The whole row's stream is always consumed, even when
 * nothing is visible (vis <= 0), so later rows stay in sync. Returns 0, or -1
 * if src/dst is NULL or width/rows is non-positive. */
int sprite_render_rle_clipped(const u8 *src, u8 *dst, int width, int rows,
                              int stride, u8 bank,
                              int clip_l, int clip_r, int clip_t, int mirror);

/* PORT: 0x58CBD. Clip-aware raw (uncompressed) copy renderer, shared by the
 * unclipped raw type 0x02 (called with clip_l = clip_r = clip_t = 0) and the
 * clipped type 0x12 (RAW|CLIP). `vis = width - clip_l - clip_r` bytes are
 * copied per drawn row from src + clip_t*width + clip_l, with the bank offset
 * added byte-wise to every pixel; src advances a whole `width` per row (the
 * clip_l/clip_r overhangs) and dst by `stride`. The original maps both types to
 * 0x58CBD and the renderer itself reads the node's clip fields. Returns 0
 * (including the vis <= 0 / rows - clip_t <= 0 no-draw paths), or -1 if
 * src/dst is NULL or a dimension is non-positive. */
int sprite_render_raw(const u8 *src, u8 *dst, int width, int rows,
                      int stride, u8 bank,
                      int clip_l, int clip_r, int clip_t);

/* PORT: 0x5215C. Mode-1 shear renderer for dispatch types 0x04 (unclipped) and
 * 0x06 (mode-1 | clip). The original stores signed 16-bit values one per image
 * row at DS_00107900; the r-th drawn row (r = 0 after the clip_t skip, matching
 * the original's node->+0x3C counter) is shifted horizontally by
 * ((s16)tab[r] - (s16)tab[0]) >> 5, an arithmetic shift, so a negative
 * shear truncates toward -infinity (the shear cast is load-bearing: reading the
 * entry as unsigned makes the shift amount a large positive offset). `rows` has
 * already had clip_b subtracted by the blitter, so there is no clip_b; clip_t
 * whole rows are skipped without drawing and clip_l/clip_r columns each side are
 * the window overhangs. The visible window is `width - clip_l - clip_r` columns
 * written sequentially from dst[0], and `vis` bytes are read from `src + sh`,
 * where src is the window origin (row clip_t, column clip_l) advanced by width
 * per row -- so the fixture needs slack past the last row for a positive sh.
 * Returns 0 (including the vis <= 0 / rows - clip_t <= 0 no-draw paths), or -1
 * if src/dst is NULL or width/rows is non-positive. */
int sprite_render_shear(const u8 *src, u8 *dst, int width, int rows,
                        int stride, u8 bank,
                        int clip_l, int clip_r, int clip_t);

#endif /* PR_SPRITE_H */
