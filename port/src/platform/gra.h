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
 * not advance. */
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
 * out-of-range frame, a zero/negative-dimension sentinel, an unroutable handle
 * or a malformed run. */
int gra_decode_frame(u32 file_off, const GraChunk *chunks, int chunk_count,
                     int frame, u8 *dst, u32 dst_len);

#endif /* PR_GRA_H */
