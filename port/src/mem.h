/* Flat 64 MB address space preserving the original LE linear addresses.
 * The original probes extended memory and keeps a block list; 0x1B120 loads
 * every INDEX resource eagerly, 41.31 MB shipped, so the port sizes its flat
 * space to hold them rather than emulating EMS. The data object lives at
 * 0x80000, so DS:0x0004 is 0x80004 and is written DSB(0x80004). The code
 * object range 0x10000..0x73B15 is reserved: the port never executes the
 * original code bytes (code stays reimplemented in C), but mem_load_le() may
 * populate them so cross-object fixups resolve and the image can be validated
 * against Ghidra. */
#ifndef PR_MEM_H
#define PR_MEM_H

#include "types.h"

#define MEM_SIZE 0x4000000u
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
 * implementing it, in both directions.
 * Sentinels: fn_resolve returns NULL for an address that was never registered,
 * and fn_origin returns 0 for a function that was never registered (0 is never
 * a legal code address; the code object starts at 0x10000). Callers must check
 * fn_resolve for NULL before making the indirect call. */
void  fn_register(u32 orig_addr, void (*fn)(void));
void (*fn_resolve(u32 orig_addr))(void);
u32   fn_origin(void (*fn)(void));

/* PORT: the miss log (record gameplay-u0 §U0.1). The original calls every code
 * pointer it loads; the port's callers skip an unregistered one silently, so a
 * table-reached function the port lacks leaves no trace. fn_resolve_from is
 * fn_resolve plus the caller's name: while the log is armed, each distinct
 * (non-zero unresolved address, caller) pair is recorded with a hit count.
 * Inert by default (nothing is recorded until fn_misslog_arm(1)); the drivers
 * arm it and report it, PR_FN_MISSLOG arms the windowed run. The macro routes
 * every `fn_resolve(x)` call through it with the calling function's name. */
void (*fn_resolve_from(u32 orig_addr, const char *ctx))(void);
#define fn_resolve(a) fn_resolve_from((a), __func__)

#define FN_MISSLOG_MAX 64u
void        fn_misslog_arm(int on);        /* 1 arms and clears, 0 disarms */
u32         fn_misslog_count(void);        /* distinct (address, caller) pairs */
u32         fn_misslog_addr(u32 i);
const char *fn_misslog_ctx(u32 i);
u32         fn_misslog_hits(u32 i);
u32         fn_misslog_dropped(void);      /* misses past FN_MISSLOG_MAX pairs */
/* 1 when some recorded pair has address `addr` (any caller). */
int         fn_misslog_has(u32 addr);
/* One `fn-miss <tag> 0xADDR <caller> hits=N` line per pair on stdout, then
 * `fn-miss <tag> distinct=N dropped=N`. */
void        fn_misslog_report(const char *tag);

#endif /* PR_MEM_H */
