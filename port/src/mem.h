/* Flat 16 MB address space preserving the original LE linear addresses.
 * The data object lives at 0x80000, so DS:0x0004 is 0x80004 and is written
 * DSB(0x80004). The code object range 0x10000..0x73B15 is reserved: the port
 * never executes the original code bytes (code stays reimplemented in C), but
 * mem_load_le() may populate them so cross-object fixups resolve and the image
 * can be validated against Ghidra. */
#ifndef PR_MEM_H
#define PR_MEM_H

#include "types.h"

#define MEM_SIZE 0x1000000u
#define DATA_BASE 0x80000u
#define CODE_BASE 0x10000u
#define CODE_END  0x73B15u

extern u8 mem[MEM_SIZE];

#define DSB(o) (*(u8  *)(mem + (o)))
#define DSW(o) (*(u16 *)(mem + (o)))
#define DSD(o) (*(u32 *)(mem + (o)))
#define DSP(o) (*(void **)(mem + (o)))

int  mem_in_range(u32 addr, u32 len);
void mem_fill(u32 addr, u8 value, u32 len);

/* Loads the LE image of exe_path into mem[]: header, object table, page map,
 * then the page data, then every object's LE fixups. Returns 1 on success.
 * When object_bin_out is non-NULL, also dumps CODE_BASE..DATA_BASE+data_size
 * as raw bytes for diffing against Ghidra. */
int mem_load_le(const char *exe_path, const char *object_bin_out);

/* Applies the LE fixup records whose source lies in object_index (0-based
 * object table index: 0 = code, 1 = data) to mem[]. Returns 1 if every fixup
 * was applied, 0 if the file cannot be parsed or holds a fixup form this
 * loader does not implement. */
int mem_load_le_fixups(const char *exe_path, u32 object_index);

/* Original code addresses are stored in data, but the port reimplements code
 * in C. Registration maps an original linear code address to the C function
 * implementing it, in both directions. */
void  fn_register(u32 orig_addr, void (*fn)(void));
void (*fn_resolve(u32 orig_addr))(void);
u32   fn_origin(void (*fn)(void));

#endif /* PR_MEM_H */
