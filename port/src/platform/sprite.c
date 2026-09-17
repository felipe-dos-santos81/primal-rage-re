#include "platform/sprite.h"
#include "platform/gra.h"
#include "platform/res.h"
#include "../mem.h"
#include <stddef.h>

void sprite_node_build(SpriteNode *n, u32 sprite_id)
{
    if (n == NULL) return;
    if (sprite_id == 0) {
        n->rows = n->width = n->xorg = n->yorg = 0;
        return;
    }
    GraSprite g;
    u32 dh = 0;
    if (!gra_sprite_lookup(sprite_id, &g, &dh)) {
        n->rows = n->width = n->xorg = n->yorg = 0;
        return;
    }
    n->pal_ptr = 0;
    n->clip_l = n->clip_r = n->clip_t = n->clip_b = 0;
    n->stride = n->row = 0;
    if (g.height < 0) {
        n->rows  = -(int)g.height;
        n->width = -(int)g.width;
        n->type  = 2u;
    } else {
        n->rows  = (int)g.height;
        n->width = (int)g.width;
        n->type  = 1u;
    }
    /* PORT: 0x14268 leaves +0x00/+0x04/+0x1C unset; render_list and the caller
     * fill them. The hflip id bit is masked off before the table lookup. */
    n->pixel_handle = g.pixel_handle;
    n->xorg = g.xorg;
    n->yorg = g.yorg;
    if (sprite_id & 0x8000u) {
        n->type |= 8u;
        n->xorg = n->width - n->xorg - 1;
    }
}

u8 sprite_bank_offset(u8 bank_byte)
{
    return (bank_byte == 0u) ? 0u : (u8)(bank_byte - 1u);
}

u32 sprite_bank(u32 pal_ptr)
{
    const u8 *p = (const u8 *)res_resolve(pal_ptr);
    return sprite_bank_offset((p != NULL) ? p[8] : 0u);
}

static void copy_run(u8 *dst, const u8 *src, int n, u8 bank)
{
    for (int i = 0; i < n; i++) dst[i] = (u8)(src[i] + bank);
}

/* PORT: the single row decoder. It walks one row of `width` pixels of RLE
 * control bytes, returns the advanced source, and stores a run only where the
 * row's visible window [clip_l, clip_l + vis) covers it, window-relative at
 * dst[mirror ? vis-1-(c-clip_l) : c-clip_l]. The unclipped renderer passes
 * clip_l = 0, vis = width, so this reduces to a plain sequential row (and the
 * original's "runs longer than the row are clipped to it" falls out of the
 * intersection). `dst == NULL` walks exactly the same control bytes and stores
 * nothing, which is how the clipped renderer consumes rows it must not draw
 * (0x5D28F's top skip and vis <= 0); the source advance is identical either way,
 * so the following rows stay in sync. A run is advanced past by its full length
 * (never the visible count), so a run overshooting the row still ends the loop.
 *
 * PORT: 0x5D28F (clipped RLE) and 0x57FFB (clipped RLE + hflip). The original is
 * a six-way nest of straddle branches (line/right/both x span-crosses-edge); the
 * port instead intersects each decoded run with the window, which is provably
 * equivalent -- every original branch draws exactly the window-covered part of
 * the straddling run and skips the rest. dst[0] is the window's first visible
 * column: the blitter computes dst = mem + ... + node.x and render_list has
 * clamped node.x inward to the clip edge, so the window is written sequentially
 * from dst[0] and dst is never indexed by the original column. Clipped-off run
 * portions are still walked, so the whole row's stream is consumed (the original
 * rewinds esi by the overhang for the literal case to reach the same endpoint). */
static const u8 *rle_row(u8 *dst, const u8 *src, int width, u8 bank,
                         int mirror, int clip_l, int vis)
{
    int col = 0;
    while (col < width) {
        u8 b = *src++;
        int n, lo, hi;
        if (b < 0x80) {
            n = b;
            if (dst != NULL) {
                lo = (col > clip_l) ? col : clip_l;
                hi = (col + n < clip_l + vis) ? col + n : clip_l + vis;
                for (int c = lo; c < hi; c++) {
                    int d = c - clip_l;
                    if (mirror) d = vis - 1 - d;
                    dst[d] = (u8)(src[c - col] + bank);
                }
            }
            src += b;              /* PORT: the run length, not the visible count */
        } else if (b < 0xC0) {
            n = b & 0x3F;
            /* PORT: DS_00081314[n] == n*0x01010101; the byte-wise model is the
             * low byte after the original's dword add, i.e. n + bank. The
             * address is spelled literally because the table sits in a region
             * Ghidra never decompiled, so gen_symbols.py does not emit it. */
            u8 colour = (u8)(DSD(0x00081314u + (u32)(*src) * 4u) + (u32)bank);
            src++;
            if (dst != NULL) {
                lo = (col > clip_l) ? col : clip_l;
                hi = (col + n < clip_l + vis) ? col + n : clip_l + vis;
                for (int c = lo; c < hi; c++) {
                    int d = c - clip_l;
                    if (mirror) d = vis - 1 - d;
                    dst[d] = colour;
                }
            }
        } else {
            n = b & 0x3F;
            /* PORT: transparent runs touch neither destination nor source. */
        }
        col += n;
    }
    return src;
}

int sprite_render_rle(const u8 *src, u8 *dst, int width, int rows,
                      int stride, u8 bank)
{
    if (src == NULL || dst == NULL || width <= 0 || rows <= 0) return -1;
    for (int r = 0; r < rows; r++) {
        src = rle_row(dst, src, width, bank, 0, 0, width);
        dst += stride;
    }
    return 0;
}

int sprite_render_rle_clipped(const u8 *src, u8 *dst, int width, int rows,
                              int stride, u8 bank,
                              int clip_l, int clip_r, int clip_t, int mirror)
{
    if (src == NULL || dst == NULL || width <= 0 || rows <= 0) return -1;
    int vis = width - clip_l - clip_r;
    if (vis <= 0) {
        /* PORT: 0x5D28F's L>=W / R>=W path still walks every row so the source
         * pointer ends where the unclipped renderer would leave it. */
        for (int r = 0; r < rows; r++)
            src = rle_row(NULL, src, width, bank, mirror, 0, width);
        return 0;
    }
    int skip = clip_t;
    if (skip < 0) skip = 0;
    if (skip > rows) skip = rows;
    for (int r = 0; r < skip; r++)
        src = rle_row(NULL, src, width, bank, mirror, 0, width);
    for (int r = skip; r < rows; r++) {
        src = rle_row(dst, src, width, bank, mirror, clip_l, vis);
        dst += stride;
    }
    return 0;
}

/* PORT: 0x58CBD. Bulk-copies `width` bytes per row with the bank offset added
 * byte-wise (copy_run), so src advances only by width*rows and dst by stride. */
int sprite_render_raw(const u8 *src, u8 *dst, int width, int rows,
                      int stride, u8 bank)
{
    if (src == NULL || dst == NULL || width <= 0 || rows <= 0) return -1;
    for (int r = 0; r < rows; r++) {
        copy_run(dst, src, width, bank);
        src += width;
        dst += stride;
    }
    return 0;
}
