#include "platform/res.h"
#include "../game/actors.h"
#include "../game/flow.h"
#include "../mem.h"
#include "../symbols.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define RES_REC   0x14u
#define RES_HEAP  0x10B0D0u   /* first free offset above the data object */
#define RES_MAX   256u

/* The entry's +0xC dword: the low 24 bits are the payload size, the high byte
 * is the loader's flag byte. 0x1B210 preloads an entry only when 0x1000000 is
 * set (the shipped INDEX sets it for s16fonts and s16statu); 0x1B3AC sets
 * 0x20000000 after an entry is read (0x1B47A), so 0x1B544's test pair
 * distinguishes resident / already-read / first read. */
#define RES_FLAG_PRELOAD 0x01000000u
#define RES_FLAG_LOADED  0x20000000u

static u32 g_heap = RES_HEAP;

/* PORT: the loader's read stalls the master loop. 0x1B3AC reads the entry's
 * payload (0x61C60) between the loader's text draw and the tick re-sync
 * (0x1B45F/0x1B464), and while it blocks the timer ISR 0x1BDF4 advances the tick
 * DS_00101508 alone, so the master loop's gate (0x25643) fails for the read's
 * duration. The port's payloads are resident, so the duration is modelled from
 * the bytes read at the rate measured from the DOSBox-X live-RAM poll (record
 * §9.6): the demo's state-6 entry reads s16beach (233128) + s16rex (3812084) +
 * s16cob (2438316) = 6483528 bytes and blocks 55 ticks, i.e. 117882 bytes/tick.
 * The two 9->6 entries measured 55 and 56 ticks; the 7->6 entry, which reads a
 * smaller set, 27. Rounded up per read. This is a *derived* rate, not a fitted
 * per-frame constant. The macro lives in res.h so test_res.c pins the exact
 * tick delta it produces. */

/* 0x1B3AC's presentation head (0x1B3B8-0x1B3F8). `draw` is the original's BL:
 * 0 from the init walk's call (0x1B250), 1 from the lazy resolve (0x1B5E9).
 * With draw set, string 489 is read through 0x1C500 and blitted at (0,192)
 * through 0x1C65C; either way the master loop's full-copy flag DS_001014FC is
 * set (0x1B3F8). `index` is the entry being read, for the read-stall model.
 *
 * The read's tail re-syncs the master loop's tick gate: 0x1B3AC ends with
 * `MOV EAX,[0x101508]` / `MOV [0x10150C],EAX` (0x1B45F/0x1B464), so
 * DS_0010150C is set equal to DS_00101508 once the read completes and the gate
 * (0x25643) passes on the load frame. The port's payloads are resident, so its
 * read is atomic: it models the ISR ticks the read consumes by advancing
 * DS_00101508 (the stall above) and then applies the same re-sync. Without it
 * the gate would fail for the stall's ticks and the loader's frame — which the
 * capture holds (the attract's 498-byte frame at capture 1, the title's first
 * frames) — would never be presented. */
static void res_load_present(u32 draw, u32 index)
{
    if (draw != 0u) {
        text_blit_string(game_string_get(0x1E9u), 0, 0xE6);   /* 0x1B3EA/0x1B3EF */
        /* PORT: the read's stall, advanced on the tick counter the gate reads. */
        u32 size = res_size(index);
        DSD(DS_00101508) += (size + RES_READ_BYTES_PER_TICK - 1u) / RES_READ_BYTES_PER_TICK;
        DSD(DS_0010150C) = DSD(DS_00101508);   /* 0x1B45F/0x1B464 */
    }
    DSD(DS_001014FC) = 1u;                                    /* 0x1B3F8 */
}

/* PORT: replaces FUN_0001C308. Bump allocator; the original is a real block
 * allocator with free(). Substituted until a task needs to release memory. */
static u32 res_alloc(u32 size)
{
    u32 at = (g_heap + 3u) & ~3u;
    if (!mem_in_range(at, size)) return 0;
    g_heap = at + size;
    return at;
}

u32 res_count(void) { return DSD(DS_001014F0); }

/* Opens game_dir/<name>. INDEX names are lowercase while the files shipped on
 * the CD are uppercase (S16TITLE.GRA); DOS is case-insensitive but a POSIX
 * filesystem is not, and macOS hides the mismatch. Try the exact name, then
 * scan the directory comparing case-insensitively, so the result does not
 * depend on the host filesystem's case behaviour. */
static FILE *res_open_name(const char *game_dir, const char *name, size_t nlen)
{
    char path[512];
    snprintf(path, sizeof path, "%.*s/%.*s", 400, game_dir, (int)nlen, name);
    FILE *f = fopen(path, "rb");
    if (f) return f;

    DIR *d = opendir(game_dir);
    if (!d) return NULL;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (strlen(ent->d_name) == nlen &&
            strncasecmp(ent->d_name, name, nlen) == 0) {
            snprintf(path, sizeof path, "%.*s/%s", 400, game_dir, ent->d_name);
            f = fopen(path, "rb");
            break;
        }
    }
    closedir(d);
    return f;
}

/* Index entries hold a 12-byte name that is not NUL-terminated when exactly 12
 * characters long, so its length is measured before opening. */
static FILE *res_open(const char *game_dir, u32 index)
{
    const char *name = res_name(index);
    size_t nlen = 0;
    while (nlen < 12 && name[nlen]) nlen++;
    return res_open_name(game_dir, name, nlen);
}

/* game_dir is the directory holding the INDEX-listed files (data/game/C);
 * index_path is the INDEX file itself. */
int res_load_index(const char *game_dir, const char *index_path)
{
    FILE *f = fopen(index_path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    /* The original calls FUN_0001D290 (fatal) when the size is not a multiple
     * of 0x14. The port reports failure instead of dying. */
    if (sz <= 0 || sz % (long)RES_REC) { fclose(f); return 0; }
    if (sz / (long)RES_REC > (long)RES_MAX) { fclose(f); return 0; }

    u32 table = res_alloc((u32)sz);
    if (table == 0) { fclose(f); return 0; }
    if (fread(mem + table, 1, (size_t)sz, f) != (size_t)sz) { fclose(f); return 0; }
    fclose(f);

    DSD(DS_001014E0) = table;
    DSD(DS_001014F0) = (u32)(sz / (long)RES_REC);

    /* Walk the entries exactly as 0x1B120 does, and additionally read each
     * resource's bytes from disk: the original reads them through 0x1B3AC,
     * which the port replaces with a direct file read into the block. A file
     * that cannot be read is counted, not fatal — the CD sets contain files the
     * installed directory may not have. The unread block only *reads* as zeroed
     * because `mem[]` starts zeroed and this bump allocator never reuses or
     * clears a block; the allocator itself does not zero.
     * PORT: the original reads a payload at its first resolve (0x1B3AC), the
     * port reads all of them here. Nothing can observe the read timing — a
     * payload is reachable only through res_resolve — but the loader's flags
     * are kept, so 0x1B544's first resolve of a non-preloaded entry still runs
     * the presentation. Only the preloaded entries (0x1B210) mark themselves
     * read at init; the rest stay unread-flagged and draw on first resolve. */
    u32 biggest = 0, missing = 0;
    for (u32 i = 0; i < DSD(DS_001014F0); i++) {
        u32 size = DSD(table + i * RES_REC + 12) & 0xFFFFFFu;
        u32 data = res_alloc(size);
        if (data == 0) return 0;
        DSD(table + i * RES_REC + 16) = data;
        if (size > biggest) biggest = size;

        FILE *rf = res_open(game_dir, i);
        int read_ok = 0;
        if (rf != NULL) {
            size_t got = fread(mem + data, 1, size, rf);
            fclose(rf);
            read_ok = (got == size);
        }
        if (!read_ok) missing++;
        if (DSD(table + i * RES_REC + 12) & RES_FLAG_PRELOAD) {
            /* PORT: 0x1B47A marks the entry read *after* 0x1B3AC's read; the
             * init call passes EDX = 1, so a failed preload read is 0x1D290's
             * fatal in the original, not a marked-but-empty entry. The port
             * counts the failure and continues (the installed file set may be
             * partial), so it marks only what it read. The 0x1B3F8 full-copy
             * flag is stored before the read in the original, so the
             * presentation runs either way; a preloaded entry is answered by
             * the resident bit regardless, making the difference unobservable
             * on a complete install. */
            if (read_ok) DSD(table + i * RES_REC + 12) |= RES_FLAG_LOADED;
            res_load_present(0u, (u32)i);                      /* 0x1B250 (BL=0) */
        }
    }
    DSD(DS_001014F8) = biggest;
    if (missing) fprintf(stderr, "res: %u of %u resources not read\n",
                         missing, DSD(DS_001014F0));
    /* 0x1B120 allocates four buffers after the entry walk (disassembly
     * 0x1B2A0-0x1B3A9): DS_001014E8 and DS_001014E4 at 0xFA00 each,
     * DS_001014EC at 0x4880 and DS_001014F4 at 0xEBA0. The sizes are the
     * immediates loaded into edx before each FUN_0001C308 call; the allocation
     * type in eax (0x41, retried as 1) does not affect the bump allocator.
     * E8/E4 are the pair 0x51F45 later assigns to DAT_000E87A0/DAT_000E87A4;
     * their contents are set up by the ported title path, not this loader, so
     * allocate all four and leave them zeroed. */
    DSD(DS_001014E8) = res_alloc(0xFA00u);
    DSD(DS_001014E4) = res_alloc(0xFA00u);
    DSD(DS_001014EC) = res_alloc(0x4880u);
    DSD(DS_001014F4) = res_alloc(0xEBA0u);
    return (int)DSD(DS_001014F0);
}

int res_load_file(const char *game_dir, const char *name, u32 *out_off, u32 *out_size)
{
    if (!game_dir || !name || !out_off || !out_size) return 0;
    size_t nlen = strlen(name);
    if (nlen == 0) return 0;

    FILE *f = res_open_name(game_dir, name, nlen);
    if (!f) return 0;

    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return 0; }
    long sz = ftell(f);
    /* Reject a size that is not a positive u32 before allocating. The size is
     * then the single u32 used for both the allocation and the read: were the
     * read left as the long, a >4 GiB file truncated for res_alloc could pass
     * mem_in_range and then fread the full size past mem[]. */
    if (sz <= 0 || sz > 0xFFFFFFFFL || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return 0;
    }
    u32 size = (u32)sz;

    u32 off = res_alloc(size);
    if (off == 0) { fclose(f); return 0; }
    if (fread(mem + off, 1, size, f) != size) { fclose(f); return 0; }
    fclose(f);

    *out_off = off;
    *out_size = size;
    return 1;
}

static u32 res_table(void) { return DSD(DS_001014E0); }

const char *res_name(u32 index)   { return (const char *)(mem + res_table() + index * RES_REC); }
u32 res_size(u32 index)           { return DSD(res_table() + index * RES_REC + 12) & 0xFFFFFFu; }
u8  res_flags(u32 index)          { return (u8)(DSD(res_table() + index * RES_REC + 12) >> 24); }

u32 res_handle(u32 index, u32 offset) { return (index << 23) | (offset & 0x7FFFFFu); }

void *res_resolve(u32 handle)
{
    u32 index = handle >> 23;
    if (index >= DSD(DS_001014F0)) return NULL;
    u32 entry = res_table() + index * RES_REC;
    u32 flags = DSD(entry + 12);
    u32 data = DSD(entry + 16);
    if (data == 0) return NULL;
    /* 0x1B569: the resident test. 0x1B57F/0x1B585: an entry whose loaded flag
     * is already set returns without presenting — the port stores the payload
     * offset where the original stores its block descriptor, so the `data != 0`
     * guard above is the port's form of 0x1B57F's `[block+8] != 0`.
     * 0x1B5E0: the first resolve of a lazy entry presents the loader screen
     * (0x1B3AC's head) and marks the entry read (0x1B47A).
     * PORT: 0x1B5A6's allocate arm (0x1E774) and 0x1B5D4's fatal have no port
     * analogue (the payload block is allocated at init), and 0x1B5C2's arm — a
     * set 0x40000000 in the entry's +0xC bypasses 0x1B5D4 and forces the load —
     * is not modelled: an instruction search finds no store of 0x40000000 to an
     * entry's +0xC anywhere in the code object (the INDEX carries 0x01/0x02
     * only), so no shipped path sets it. */
    if ((flags & (RES_FLAG_PRELOAD | RES_FLAG_LOADED)) == 0u) {
        /* PORT: the original presents first (0x1B3EA/0x1B3EF) and marks the
         * entry read after the file read (0x1B47A). The port's payload is
         * already resident, so it marks first: the presentation's own palette
         * flush (0x1C470 -> 0x1B544) can name the entry being read only if a
         * dirty-list record was enqueued before its resolve, which the
         * original's enqueue order (palette_record runs after res_resolve)
         * rules out. Marking first makes such a re-entrant resolve return
         * instead of presenting the same entry again. */
        DSD(entry + 12) = flags | RES_FLAG_LOADED;    /* 0x1B47A */
        res_load_present(1u, index);                  /* 0x1B5E0-0x1B5E9 (BL=1) */
    }
    return mem + data + (handle & 0x7FFFFFu);
}
