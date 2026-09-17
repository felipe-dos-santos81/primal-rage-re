#include "platform/res.h"
#include "../mem.h"
#include "../symbols.h"
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#define RES_REC   0x14u
#define RES_HEAP  0x10B0D0u   /* first free offset above the data object */
#define RES_MAX   256u

static u32 g_heap = RES_HEAP;

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

/* Opens game_dir/<index entry name>. INDEX names are lowercase while the files
 * shipped on the CD are uppercase (S16TITLE.GRA); DOS is case-insensitive but a
 * POSIX filesystem is not, and macOS hides the mismatch. Try the exact name,
 * then scan the directory comparing case-insensitively, so the result does not
 * depend on the host filesystem's case behaviour. The 12-byte name is not
 * NUL-terminated when exactly 12 characters long. */
static FILE *res_open(const char *game_dir, u32 index)
{
    const char *name = res_name(index);
    size_t nlen = 0;
    while (nlen < 12 && name[nlen]) nlen++;

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
     * clears a block; the allocator itself does not zero. */
    u32 biggest = 0, missing = 0;
    for (u32 i = 0; i < DSD(DS_001014F0); i++) {
        u32 size = DSD(table + i * RES_REC + 12) & 0xFFFFFFu;
        u32 data = res_alloc(size);
        if (data == 0) return 0;
        DSD(table + i * RES_REC + 16) = data;
        if (size > biggest) biggest = size;

        FILE *rf = res_open(game_dir, i);
        if (!rf) { missing++; continue; }
        size_t got = fread(mem + data, 1, size, rf);
        fclose(rf);
        if (got != size) missing++;
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

static u32 res_table(void) { return DSD(DS_001014E0); }

const char *res_name(u32 index)   { return (const char *)(mem + res_table() + index * RES_REC); }
u32 res_size(u32 index)           { return DSD(res_table() + index * RES_REC + 12) & 0xFFFFFFu; }
u8  res_flags(u32 index)          { return (u8)(DSD(res_table() + index * RES_REC + 12) >> 24); }

u32 res_handle(u32 index, u32 offset) { return (index << 23) | (offset & 0x7FFFFFu); }

void *res_resolve(u32 handle)
{
    u32 index = handle >> 23;
    if (index >= DSD(DS_001014F0)) return NULL;
    u32 data = DSD(res_table() + index * RES_REC + 16);
    if (data == 0) return NULL;
    return mem + data + (handle & 0x7FFFFFu);
}
