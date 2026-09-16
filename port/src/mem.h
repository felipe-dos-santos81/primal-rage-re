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
 * then the page data. Fixups are applied by mem_load_le_fixups() (Task 3).
 * When object_bin_out is non-NULL, also dumps CODE_BASE..DATA_BASE+data_size
 * as raw bytes for diffing against Ghidra. Returns 1 on success. */
int mem_load_le(const char *exe_path, const char *object_bin_out);

#endif /* PR_MEM_H */
