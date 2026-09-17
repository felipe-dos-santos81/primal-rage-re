#include "platform/gra.h"
#include "platform/res.h"
#include "../mem.h"
#include "../symbols.h"
#include <string.h>

int gra_open(u32 file_off, u32 file_len, GraChunk *out, int max, int *count)
{
    *count = 0;
    if (file_len < 8 || !mem_in_range(file_off, file_len)) return 0;
    int n = 0;
    u32 off = 0;
    while (n < max) {
        if (off + 8 > file_len) return 0;
        u16 type = DSW(file_off + off);
        char m0 = (char)DSB(file_off + off + 2);
        char m1 = (char)DSB(file_off + off + 3);
        if (m0 != '4' || m1 != '3') return 0;
        u32 next = DSD(file_off + off + 4);
        /* Require `next` to clear this chunk's header: a value in
         * (off, off+8) would give body_len a ~0xFFFFFFFF underflow. */
        if (next != 0 && (next > file_len || next < off + 8)) return 0;
        out[n].off = off;
        out[n].type = type;
        out[n].body_off = off + 8;
        out[n].body_len = (next ? next : file_len) - (off + 8);
        n++;
        if (next == 0) break;
        off = next;
    }
    /* A chain longer than `max` stops here with *count == max and still returns
     * 1 (truncated, not an error): the caller sees at most `max` chunks. */
    *count = n;
    return 1;
}

/* RLE-decodes one sprite from src: `height` rows of `width` pixels, decoded
 * back to back. Dispatch bit 7 first, then bit 6 (FORMATS.md):
 *   b & 0x80 == 0            literal run  (b & 0x7F) pixels, one colour byte each
 *   b & 0x80 != 0, b&0x40==0 repeat run   (b & 0x3F) pixels, one colour byte
 *   b & 0x80 != 0, b&0x40!=0 transparent  (b & 0x3F) pixels, index 0
 * Returns the bytes consumed, or -1 if `w*h` exceeds `dst_len`, the source is
 * exhausted before the sprite is filled, a literal's payload overruns `src_len`,
 * or a token would advance zero pixels (0x00 / 0x80 / 0xC0). A run longer than
 * the remaining row is clipped to the row (the original's raster pass does the
 * same) and does not fail.
 * PORT: transparent runs write index 0, conflating them with palette entry 0.
 * Lossless for the shipped assets: over all 113,266,036 sprite pixel slots
 * (w*h of every positive-dimension descriptor), 0 of the 49,423,356 opaque
 * pixels carry index 0, so no opaque pixel is lost. Not lossless in principle —
 * a sprite using entry 0 opaque cannot be told apart from a transparent run.
 * PORT: a zero-advance token (0x00 / 0x80 / 0xC0) returns -1; the original's
 * measure loop would not terminate. None occurs in the 69 shipped files. */
static int rle_decode(const u8 *src, u32 src_len, u16 w, u16 h,
                      u8 *dst, u32 dst_len)
{
    if ((u32)w * (u32)h > dst_len) return -1;
    u32 pos = 0;
    for (u32 row = 0; row < h; row++) {
        u32 x = 0;
        u8 *out = dst + row * w;
        while (x < w) {
            if (pos >= src_len) return -1;
            u8 b = src[pos++];
            if (b & 0x80) {
                u32 c = b & 0x3Fu;
                if (c == 0) return -1;
                if (b & 0x40) {
                    for (u32 i = 0; i < c && x + i < w; i++) out[x + i] = 0;
                } else {
                    if (pos >= src_len) return -1;
                    u8 v = src[pos++];
                    for (u32 i = 0; i < c && x + i < w; i++) out[x + i] = v;
                }
                x += c;
            } else {
                u32 c = b & 0x7Fu;
                if (c == 0 || pos + c > src_len) return -1;
                for (u32 i = 0; i < c && x + i < w; i++) out[x + i] = src[pos + i];
                pos += c;
                x += c;
            }
        }
    }
    return (int)pos;
}

/* Shared decode core for gra_decode_frame (frame-indexed) and
 * gra_decode_frame_at (blob-addressed). A zero or "negative" dimension
 * (0xFEC0 = -320, 0xFC31 = -975, ...) reads as a full-screen blit/clear
 * sentinel, not a sprite, and is rejected here; the frame path sign-extends the
 * header's u16 so the original's (s16) test is reproduced.
 * TODO(verify): the sentinel meaning is documented, not proven. */
static int decode_frame_core(const u8 *blob, u32 len, int w, int h,
                             u8 *dst, u32 cap)
{
    if (blob == NULL || dst == NULL || w <= 0 || h <= 0) return -1;
    return rle_decode(blob, len, (u16)w, (u16)h, dst, cap);
}

int gra_decode_palette(u32 file_off, const GraChunk *chunks, int chunk_count,
                       u8 *rgb_out, u32 rgb_cap, int *count)
{
    /* TODO(verify): the bank is returned flat; which record / DAC base index a
     * given sprite selects is still open, so no sub-palette split is applied. */
    *count = 0;
    const GraChunk *c5 = 0;
    for (int i = 0; i < chunk_count; i++)
        if (chunks[i].type == 5) { c5 = &chunks[i]; break; }
    if (!c5) return 0;

    u32 p = file_off + c5->body_off;
    u32 end = p + c5->body_len;
    u32 cnt = 0;
    while (p < end) {
        if (p + 4 > end) return 0;
        u32 n = DSD(p);
        p += 4;
        if (n > (end - p) / 4) return 0;
        /* Reject before writing when this record would not fit. The invariant
         * 3*cnt <= rgb_cap holds on entry, so this never writes past rgb_cap. */
        if (n > (rgb_cap - cnt * 3) / 3) return 0;
        for (u32 i = 0; i < n; i++) {
            u32 word = DSD(p);
            p += 4;
            rgb_out[cnt * 3 + 0] = (u8)((word >> 2) & 0xFF);
            rgb_out[cnt * 3 + 1] = (u8)((word >> 10) & 0xFF);
            rgb_out[cnt * 3 + 2] = (u8)((word >> 18) & 0xFF);
            cnt++;
        }
    }
    *count = (int)cnt;
    return 1;
}

int gra_decode_frame(u32 file_off, const GraChunk *chunks, int chunk_count,
                     int frame, u8 *dst, u32 dst_len)
{
    const GraChunk *c6 = 0;
    for (int i = 0; i < chunk_count; i++)
        if (chunks[i].type == 6) { c6 = &chunks[i]; break; }
    if (!c6 || frame < 0) return -1;

    u32 recs = c6->body_len / 12;
    if ((u32)frame >= recs) return -1;
    u32 at = file_off + c6->body_off + (u32)frame * 12;
    if (!mem_in_range(at, 12)) return -1;

    u16 w = DSW(at), h = DSW(at + 2);
    u32 handle = DSD(at + 8);
    /* TODO(verify): the s16 x/y anchor is not read here; its meaning (sprite
     * origin vs. bounding-box corner) is likely, not proven. */

    /* PORT: resolves the pixel handle through res_resolve (the port's resource
     * heap) instead of the original's EMS block walk at 0x1B544. The bytes are
     * identical, and res_size() gives rle_decode a length the original lacks. */
    u32 idx = handle >> 23;
    u32 off = handle & 0x7FFFFFu;
    if (idx >= res_count()) return -1;
    u32 size = res_size(idx);
    if (off >= size) return -1;
    u8 *src = res_resolve(res_handle(idx, off));
    if (!src) return -1;

    /* PORT: returns RLE bytes consumed (the original writes into a caller
     * buffer and signals nothing); the exact-consumption test needs it. */
    return decode_frame_core(src, size - off, (int)(s16)w, (int)(s16)h,
                             dst, dst_len);
}

int gra_decode_frame_at(const u8 *blob, u32 len, int w, int h, u8 *dst, u32 cap)
{
    return decode_frame_core(blob, len, w, h, dst, cap);
}

int gra_sprite_lookup(u32 sprite_id, GraSprite *out, u32 *desc_handle)
{
    u32 h = DSD(DS_000A8B30 + (sprite_id & 0x7FFFu) * 4u);
    if (h == 0) return 0;
    if (desc_handle) *desc_handle = h;
    return gra_sprite_open(h, out);
}

int gra_sprite_open(u32 desc_handle, GraSprite *out)
{
    const u8 *p = (const u8 *)res_resolve(desc_handle);
    if (p == NULL || out == NULL) return 0;
    memcpy(&out->width,  p + 0, 2);
    memcpy(&out->height, p + 2, 2);
    memcpy(&out->xorg,   p + 4, 2);
    memcpy(&out->yorg,   p + 6, 2);
    out->pixel_handle = (u32)p[8] | ((u32)p[9] << 8) | ((u32)p[10] << 16) |
                        ((u32)p[11] << 24);
    return 1;
}

int gra_sprite_pixels(u32 pixel_handle, const u8 **out)
{
    const u8 *p = (const u8 *)res_resolve(pixel_handle);
    if (p == NULL) return 0;
    if (out) *out = p;
    return 1;
}
