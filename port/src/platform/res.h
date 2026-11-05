/* Resource manager: the INDEX table at 0x1B120 and handle resolution at
 * 0x1B544. The 20-byte-per-entry table, its count and every resource payload
 * live in mem[] at the offsets the original uses. */
#ifndef PR_RES_H
#define PR_RES_H

#include "types.h"
#include <stddef.h>

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

/* PORT: the allocation-failure test seam (record
 * 2026-09-30-named-gaps-c-derivations.md §C.1), not original state (the
 * pattern of res_set_screen_hook). res_fail_alloc_nth(n) makes the n-th
 * bump-allocator request from now on return 0 without moving the heap, as a
 * block that does not fit does, and then disarms; every request counts
 * (res_load_index's, res_load_file's and res_block_alloc's, size 0 included).
 * n = 0 disarms. res_fail_alloc_left() is the requests left until the failure
 * (0 = disarmed). Nothing in port/src arms it. */
void res_fail_alloc_nth(u32 n);
u32 res_fail_alloc_left(void);

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

/* PORT: the release's game-folder check (plan 2026-11-05-github-release),
 * no original counterpart. Checks that game_dir holds every file the port
 * loads at boot, in this order: PRAGE.EXE and INDEX with the exact case
 * game_init opens them; each INDEX record's name, matched as res_load_index
 * matches it; FAT.OPL, TWI5.SMK and TWG.SMK, matched as res_load_file matches
 * them; english.txt, upper case then lower case, as game_string_table_load
 * opens it. Reads nothing into mem[]. `name` (n > 0) is always NUL-terminated:
 * the missing or wrong-case name, "" for the other results. */
#define RES_DIR_OK          0
#define RES_DIR_NO_EXE      1  /* <dir>/PRAGE.EXE does not open in any case */
#define RES_DIR_NO_INDEX    2  /* <dir>/INDEX does not open in any case, or its size is 0 or not a multiple of 0x14 */
#define RES_DIR_WRONG_CASE  3  /* PRAGE.EXE or INDEX exists only in another case; `name` = the on-disk name */
#define RES_DIR_MISSING     4  /* a required file is absent; `name` = the first missing name */
int res_dir_check(const char *game_dir, char *name, size_t n);

#endif /* PR_RES_H */
