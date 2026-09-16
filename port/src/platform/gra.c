#include "platform/gra.h"
#include "platform/res.h"
#include "../mem.h"

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
    *count = n;
    return 1;
}

/* RLE-decodes one sprite from src: `height` rows of `width` pixels, decoded
 * back to back. Dispatch bit 7 first, then bit 6 (FORMATS.md):
 *   b & 0x80 == 0            literal run  (b & 0x7F) pixels, one colour byte each
 *   b & 0x80 != 0, b&0x40==0 repeat run   (b & 0x3F) pixels, one colour byte
 *   b & 0x80 != 0, b&0x40!=0 transparent  (b & 0x3F) pixels, index 0
 * Returns the bytes consumed, or -1 if a run overruns the row, src, or dst, or
 * if a token would advance zero pixels (which the original's grammar excludes
 * but which would otherwise spin). */
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

int gra_decode_palette(u32 file_off, const GraChunk *chunks, int chunk_count,
                       u8 *rgb_out, int *count)
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
    int cnt = 0;
    while (p < end) {
        if (p + 4 > end) return 0;
        u32 n = DSD(p);
        p += 4;
        if (n > (end - p) / 4) return 0;
        for (u32 i = 0; i < n; i++) {
            u32 word = DSD(p);
            p += 4;
            rgb_out[cnt * 3 + 0] = (u8)((word >> 2) & 0xFF);
            rgb_out[cnt * 3 + 1] = (u8)((word >> 10) & 0xFF);
            rgb_out[cnt * 3 + 2] = (u8)((word >> 18) & 0xFF);
            cnt++;
        }
        *count = cnt;
    }
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

    /* Zero-dimension and "negative"-dimension records (0xFEC0 = -320,
     * 0xFC31 = -975, ...) read as full-screen blit/clear sentinels, not
     * sprites. TODO(verify): the sentinel meaning is documented, not proven. */
    if ((s16)w <= 0 || (s16)h <= 0) return -1;

    u32 idx = handle >> 23;
    u32 off = handle & 0x7FFFFFu;
    if (idx >= res_count()) return -1;
    u32 size = res_size(idx);
    if (off >= size) return -1;
    u8 *src = res_resolve(res_handle(idx, off));
    if (!src) return -1;

    return rle_decode(src, size - off, w, h, dst, dst_len);
}
