#include "platform/res.h"
#include "../mem.h"
#include "../symbols.h"
#include <stdio.h>
#include <string.h>

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
     * that cannot be read leaves the block zeroed and is counted, not fatal —
     * the CD sets contain files the installed directory may not have. */
    u32 biggest = 0, missing = 0;
    char path[512];
    for (u32 i = 0; i < DSD(DS_001014F0); i++) {
        u32 size = DSD(table + i * RES_REC + 12) & 0xFFFFFFu;
        u32 data = res_alloc(size);
        if (data == 0) return 0;
        DSD(table + i * RES_REC + 16) = data;
        if (size > biggest) biggest = size;

        /* The name field is 12 bytes and not NUL-terminated when exactly 12
         * characters long, so bound it explicitly. */
        snprintf(path, sizeof path, "%.*s/%.12s", 400, game_dir, res_name(i));
        FILE *rf = fopen(path, "rb");
        if (!rf) { missing++; continue; }
        size_t got = fread(mem + data, 1, size, rf);
        fclose(rf);
        if (got != size) missing++;
    }
    DSD(DS_001014F8) = biggest;
    if (missing) fprintf(stderr, "res: %u of %u resources not read\n",
                         missing, DSD(DS_001014F0));
    /* 0x1B120 also allocates the pair DS_001014E8 / DS_001014E4 here; they are
     * the buffers 0x51F45 later assigns to DAT_000E87A0/DAT_000E87A4. Their
     * contents are set up by the ported 0x51F45/title path, not by this loader,
     * so allocate them and leave them zeroed. */
    DSD(DS_001014E8) = res_alloc(0x1000);
    DSD(DS_001014E4) = res_alloc(0x1000);
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
