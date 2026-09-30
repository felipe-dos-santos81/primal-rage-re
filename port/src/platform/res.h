/* Resource manager: the INDEX table at 0x1B120 and handle resolution at
 * 0x1B544. The 20-byte-per-entry table, its count and every resource payload
 * live in mem[] at the offsets the original uses. */
#ifndef PR_RES_H
#define PR_RES_H

#include "types.h"

/* The loader read-stall rate: payload bytes read per timer tick, derived from
 * the DOSBox-X live-RAM poll (record §9.6 as corrected by §45-A: 7297097 bytes
 * block 55 ticks). The port models 0x1B3AC's blocking read with this rate in
 * res.c; test_res.c pins the exact tick delta it produces, so the value cannot
 * drift unnoticed. */
#define RES_READ_BYTES_PER_TICK 132674u

/* Builds the entry table in mem[] the way 0x1B120 does and loads each
 * resource's bytes from disk. game_dir holds the INDEX-listed files;
 * index_path is the INDEX file itself. Returns the entry count, 0 on failure. */
int res_load_index(const char *game_dir, const char *index_path);

/* Loads game_dir/name into mem[] through the bump allocator, for files that
 * are not INDEX entries (the Smacker movies). The name is matched
 * case-insensitively like the INDEX path. On success writes the mem[] offset
 * and byte count and returns 1; returns 0 on any failure (missing file, read
 * error, does not fit mem[]) and leaves the outputs untouched. */
int res_load_file(const char *game_dir, const char *name, u32 *out_off, u32 *out_size);

/* The port's 0x1C308 (res.c's bump allocator): a mem[] offset, 0 if it does not fit. */
u32 res_block_alloc(u32 size);

u32 res_count(void);

/* 12-byte, zero-padded, not necessarily NUL-terminated. */
const char *res_name(u32 index);

u32 res_size(u32 index);
u8  res_flags(u32 index);

/* Packs a resource index (high bits) and byte offset (low 23 bits) the way
 * 0x1B544 expands a handle. */
u32 res_handle(u32 index, u32 offset);

/* handle -> pointer, matching 0x1B544. NULL when the index is out of range or
 * the entry has no data block. */
void *res_resolve(u32 handle);

/* PORT: the dump seam for the loader's `- LOADING -` screen. The hook runs on
 * a lazy resolve's presentation, right after the text is on the aperture
 * (gfx_aperture()); NULL (the default) disables it. */
void res_set_screen_hook(void (*hook)(void));

#endif /* PR_RES_H */
