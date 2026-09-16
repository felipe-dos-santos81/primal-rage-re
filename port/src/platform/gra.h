/* `.GRA` graphics container: a linked list of chunks. This module only walks
 * the chain and reports each chunk's type and body range; the payloads are
 * decoded by later tasks. The 8-byte header layout is documented in
 * ../FORMATS.md. */
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

#endif /* PR_GRA_H */
