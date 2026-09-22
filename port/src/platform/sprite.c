#include "platform/sprite.h"
#include "platform/gra.h"
#include "platform/res.h"
#include "../mem.h"
#include "../symbols.h"
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
    /* PORT: pal_ptr is a 0x33754 palette-table entry ({handle @+0; refcount
     * @+4; start @+8; len @+12} at DS_00107618), not a resource handle: 0x33754
     * returns the entry address (prage.c:20782/20811), spawn stores it in
     * pset+0x18, and 0x14328 copies it into the node. The original's "bank at
     * [+8]" is therefore the low byte of the entry's `start` — the DAC offset
     * palette_record uploaded the palette at. Entry 0 is the null palette. */
    if (pal_ptr == 0) return 0;
    return sprite_bank_offset(DSB(pal_ptr + 8u));
}

static void copy_run(u8 *dst, const u8 *src, int n, u8 bank)
{
    for (int i = 0; i < n; i++) dst[i] = (u8)(src[i] + bank);
}

/* PORT: the single row decoder. It walks one row of `width` pixels of RLE
 * control bytes, returns the advanced source, and stores a run only where the
 * row's visible window [src_l, src_l + vis) covers it, window-relative at
 * dst[mirror ? vis-1-(c-src_l) : c-src_l]. `src_l` is the window's first source
 * column: `clip_l` for the plain clipped path, and `clip_r` for the mirrored one
 * (see the 0x57FFB note below). The unclipped renderer passes clip_l = 0,
 * vis = width, so this reduces to a plain sequential row (and the
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
 * rewinds esi by the overhang for the literal case to reach the same endpoint).
 *
 * PORT: 0x57F80 (hflip RLE) sets the row destination once to dst + width - 1;
 * 0x57FFB (hflip + clipped RLE) uses dst + vis - 1; both then walk it backward,
 * so the mirror is a property of the destination pointer. The port keeps the row
 * base at mem + ...y... + x (dst[0] is the window's first visible column, as
 * above) and mirrors the column inside the row via vis - 1 - (col - src_l).
 * Observable result is identical; only the pointer arithmetic differs.
 *
 * PORT: the mirrored source window starts at `clip_r`, not `clip_l`. 0x57FFB
 * computes vis = width - clip_l - clip_r (0x58001) and its three source paths
 * all begin at the row-relative column clip_r: 0x58090 (clip_l != 0, clip_r ==
 * 0) reads from the row start with no skip (0x58093), while 0x581D0 (clip_l ==
 * 0, clip_r != 0) and 0x582F4 (both) load clip_r into the skip counter (`MOV
 * EDX,[EBP+0x2c]` at 0x581D0/0x582F4) and consume that many source columns
 * before drawing vis. Mirroring maps screen column s to source
 * width-1-(s-(X-clip_l)), so the screen's right overhang clip_r is the source's
 * left overhang: the window is [clip_r, width-clip_l) = [clip_r, clip_r+vis).
 * Using clip_l here instead renders the wrong slice (the first fighter's torso
 * as a diagonal triangle); the window is `width - clip_l - vis` == clip_r. */
static const u8 *rle_row(u8 *dst, const u8 *src, int width, u8 bank,
                         int mirror, int clip_l, int vis)
{
    int src_l = mirror ? width - clip_l - vis : clip_l;
    int col = 0;
    while (col < width) {
        u8 b = *src++;
        int n, lo, hi;
        if (b < 0x80) {
            n = b;
            if (dst != NULL) {
                lo = (col > src_l) ? col : src_l;
                hi = (col + n < src_l + vis) ? col + n : src_l + vis;
                for (int c = lo; c < hi; c++) {
                    int d = c - src_l;
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
                lo = (col > src_l) ? col : src_l;
                hi = (col + n < src_l + vis) ? col + n : src_l + vis;
                for (int c = lo; c < hi; c++) {
                    int d = c - src_l;
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

/* PORT: 0x58CBD. Clip-aware raw copy, shared by type 0x02 (no clip) and 0x12
 * (RAW|CLIP). vis = width - clip_l - clip_r; clip_t whole source rows are
 * skipped (with their clip_l columns), then each drawn row copies `vis` bytes
 * in bulk with the bank offset added byte-wise (copy_run), advancing src by a
 * whole `width` (the L and R overhangs) and dst by stride. */
int sprite_render_raw(const u8 *src, u8 *dst, int width, int rows,
                      int stride, u8 bank,
                      int clip_l, int clip_r, int clip_t)
{
    int vis = width - clip_l - clip_r;
    if (src == NULL || dst == NULL || width <= 0 || rows <= 0) return -1;
    if (vis <= 0 || rows - clip_t <= 0) return 0;
    src += clip_t * width + clip_l;
    for (int r = 0; r < rows - clip_t; r++) {
        copy_run(dst, src, vis, bank);
        src += width;
        dst += stride;
    }
    return 0;
}

/* PORT: 0x5215C. The shear table DS_00107900 is signed 16-bit; the (s16) cast
 * is load-bearing (see sprite.h). `ref` is tab[0]; the index is the drawn row
 * (the original counts drawn rows in node->+0x3C, zeroed before the loop, so
 * the first row after the clip_t skip uses tab[0]). src is the window origin
 * once, then advances a whole row per iteration. */
int sprite_render_shear(const u8 *src, u8 *dst, int width, int rows,
                        int stride, u8 bank,
                        int clip_l, int clip_r, int clip_t)
{
    int vis = width - clip_l - clip_r;
    if (src == NULL || dst == NULL || width <= 0 || rows <= 0) return -1;
    if (vis <= 0 || rows - clip_t <= 0) return 0;
    src += clip_t * width + clip_l;
    for (int r = 0; r < rows - clip_t; r++) {
        int ref = (s16)DSW(DS_00107900);
        int sh  = ((int)(s16)DSW(DS_00107900 + r * 2) - ref) >> 5;
        copy_run(dst, src + sh, vis, bank);
        src += width;
        dst += stride;
    }
    return 0;
}

/* PORT: 0x51E5C and 0x51ED8 are the same span blit with different destination
 * bases: 0x51E5C does `add edi, [0x687a4]` (DS_000E87A4, the back buffer the
 * renderer composites into) while 0x51ED8 does `add edi, 0xa0000` (the literal
 * VGA aperture the loader's text draws into). `base` is that destination. */
void sprite_blit_at(SpriteNode *n, u8 *base)
{
    if (n == NULL || n->width == 0 || n->rows == 0) return;
    const u8 *src = NULL;
    if (!gra_sprite_pixels(n->pixel_handle, &src)) return;

    u8  bank = (u8)sprite_bank(n->pal_ptr);
    u8 *dst  = base + DSD(DS_001088F8 + (u32)n->y * 4u) + (u32)n->x;

    /* PORT: 0x51E5C stores rows - clip_b into the node for the call and
     * restores +0x14/+0x30 afterwards. The port keeps the node intact and
     * passes the reduced row count, so a node can be re-blitted and the
     * renderers never see clip_b. */
    int rows = n->rows - n->clip_b;
    int L = n->clip_l, R = n->clip_r, T = n->clip_t;
    int w = n->width;

    /* PORT: the ten live entries of PTR_LAB_00080C8C, as a switch so the
     * mapping is explicit rather than inferred from bit combinations.
     * Everything else is the original's no-op stub, including RAW+HFLIP
     * (type 0x0A), which the original leaves unimplemented. */
    switch (n->type & 0x1Fu) {
    case 0x01: sprite_render_rle(src, dst, w, rows, 320, bank); break;
    case 0x02: sprite_render_raw(src, dst, w, rows, 320, bank, 0, 0, 0); break;
    case 0x04: case 0x06:
        sprite_render_shear(src, dst, w, rows, 320, bank, L, R, T); break;
    case 0x09:
        sprite_render_rle_clipped(src, dst, w, rows, 320, bank, 0, 0, 0, 1); break;
    case 0x11:
        sprite_render_rle_clipped(src, dst, w, rows, 320, bank, L, R, T, 0); break;
    case 0x12:
        sprite_render_raw(src, dst, w, rows, 320, bank, L, R, T); break;
    case 0x14: case 0x16:
        sprite_render_shear(src, dst, w, rows, 320, bank, L, R, T); break;
    case 0x19:
        sprite_render_rle_clipped(src, dst, w, rows, 320, bank, L, R, T, 1); break;
    default: break;                       /* PORT: stub slot: no-op */
    }
}

/* 0x51E5C: the renderer's span blit, compositing into the back buffer. */
void sprite_blit(SpriteNode *n)
{
    sprite_blit_at(n, mem + DSD(DS_000E87A4));
}
