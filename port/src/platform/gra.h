/* `.GRA` graphics container: a linked list of chunks. gra_open walks the chain
 * and reports each chunk's type and body range; gra_decode_palette and
 * gra_decode_frame decode the type-5 palette bank and the type-2/6 sprites.
 * The 8-byte header and every payload layout are documented in ../FORMATS.md.
 *
 * Every offset in GraChunk is FILE-RELATIVE: the struct carries no file base,
 * so callers must add the offset the image was loaded at — a chunk body starts
 * at mem[file_off + body_off]. */
#ifndef PR_GRA_H
#define PR_GRA_H

#include "types.h"

typedef struct {
    u32 off;
    u16 type;
    u32 body_off;
    u32 body_len;
} GraChunk;

/* Walks the chunk chain in the file image at mem[file_off .. file_off+file_len).
 * The header is u16 type, char magic[2] == "43", u32 next (absolute file
 * offset, 0 = last); body_off = off + 8 and body_len runs to `next` or EOF.
 * Fills up to `max` entries and stores the number found in *count. Returns 1 on
 * a well-formed chain, 0 on bad magic or a `next` that is out of range or does
 * not advance. A chain longer than `max` is truncated: the walk stops at `max`
 * and still returns 1 with *count == max (not an error). */
int gra_open(u32 file_off, u32 file_len, GraChunk *out, int max, int *count);

/* Decodes the type-5 palette bank in chunks (the file image lives at
 * mem[file_off ..]). The bank is concatenated { u32 count; u32 colour[count] }
 * records; each colour emits one RGB triple with R = bits[2..9], G =
 * bits[10..17], B = bits[18..25] (the packing FUN_0001C470 flushes to the DAC).
 * Writes 3 * (*count) bytes into rgb_out, whose capacity in u8 values is
 * rgb_cap. Returns 1 on success, 0 if there is no type-5 chunk, a record
 * overruns the body, or the bank does not fit in rgb_cap (nothing past the
 * capacity is ever written). Like gra_open, *count is 0 on every failure, so a
 * caller that checks the return can never read a partial bank. */
int gra_decode_palette(u32 file_off, const GraChunk *chunks, int chunk_count,
                       u8 *rgb_out, u32 rgb_cap, int *count);

/* Decodes frame `frame` of the type-6 descriptor table — 12-byte records
 * { u16 width; u16 height; s16 x; s16 y; u32 pixel_handle } — into dst as
 * width*height 8-bit palette indices, row-major. The handle resolves through
 * res_resolve() to the chunk-2 RLE blob; transparent runs write index 0,
 * matching the Python oracle's model. Returns the number of RLE bytes consumed
 * (a caller can require it to equal the next sprite's offset), or -1 for an
 * out-of-range frame, a zero/negative-dimension sentinel, an unroutable handle,
 * or a run malformed in the source (source exhausted before the image is
 * filled, a literal overrunning the blob, or a zero-advance token). A run
 * longer than the remaining row is clipped to the row, matching the original's
 * raster pass, and does not fail. */
int gra_decode_frame(u32 file_off, const GraChunk *chunks, int chunk_count,
                     int frame, u8 *dst, u32 dst_len);

/* Decodes the RLE blob at `blob` (length `len` bytes) as a `w` x `h` index
 * image, row-major and packed at `w`, into dst (capacity dst_len). Same decoder,
 * semantics and return value as gra_decode_frame (RLE bytes consumed, or -1);
 * it takes the blob and its length directly so a caller can decode a sprite
 * reached by handle rather than by a frame index. `w`/`h` must be positive. */
int gra_decode_frame_at(const u8 *blob, u32 len, int w, int h, u8 *dst, u32 cap);

/* Handle-addressed sprite descriptors: the original's 12-byte records
 * { s16 width; s16 height; s16 xorg; s16 yorg; u32 pixel_handle } reached
 * through the static handle table at DS_000A8B30 instead of a GRA file's own
 * type-6 chunk. 0x14268 masks the sprite id to 0x7FFF, indexes the table by
 * whole dwords, and calls 0x1B544 (the port's res_resolve) on the entry; a
 * handle whose entry is zero is not a sprite. The table is 18,443 entries,
 * indices 0..18442 — its end is not inferable from the data, so the mask is
 * the only bound. */
typedef struct {
    s16 width, height, xorg, yorg;
    u32 pixel_handle;
} GraSprite;

/* Opens descriptor `desc_handle` (a table entry, not a sprite id) into *out.
 * Returns 1 and fills *out, or 0 if desc_handle does not resolve. */
int gra_sprite_open(u32 desc_handle, GraSprite *out);

/* Looks sprite id `sprite_id` up in the DS_000A8B30 handle table (masked to
 * 0x7FFF as 0x14268 does), stores the descriptor handle in *desc_handle when
 * non-NULL, and opens it into *out. Returns 1, or 0 for a zero table entry or
 * an unresolvable descriptor. */
int gra_sprite_lookup(u32 sprite_id, GraSprite *out, u32 *desc_handle);

/* Resolves pixel_handle into *out (the RLE blob, as gra_decode_frame would
 * consume it). Returns 1, or 0 if the handle does not resolve. */
int gra_sprite_pixels(u32 pixel_handle, const u8 **out);

#endif /* PR_GRA_H */
