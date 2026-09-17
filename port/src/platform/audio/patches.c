/* FAT.OPL / FAT.AD patch-bank decoder. See patches.h for the layout. */
#include <stddef.h>

#include "patches.h"

#define PATCH_MAX 256

typedef struct {
    u16 key;
    u32 off;
    u8 data[PATCH_BYTES];
} patch_entry;

static patch_entry g_patches[PATCH_MAX];
static int g_count;

static u16 rd16(const u8 *p)
{
    return (u16)((u16)p[0] | ((u16)p[1] << 8));
}

static u32 rd32(const u8 *p)
{
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

int patches_load(const u8 *data, u32 len)
{
    patch_entry tmp[PATCH_MAX];
    int n = 0;
    u32 p = 0;
    u32 table_end;

    if (data == NULL || len < 8)
        return 0;

    /* Table: (u16 key, u32 offset) stepping 6 until the 0xFFFF terminator. */
    for (;;) {
        if (p + 6 > len)
            return 0;
        if (rd16(data + p) == 0xFFFFu) {
            p += 2;                      /* terminator is only the key word */
            break;
        }
        if (n >= PATCH_MAX)
            return 0;
        tmp[n].key = rd16(data + p);
        tmp[n].off = rd32(data + p + 2);
        n++;
        p += 6;
    }
    if (n == 0)
        return 0;
    table_end = p;

    /* Payloads sit after the table and inside the buffer. */
    for (int i = 0; i < n; i++) {
        u32 off = tmp[i].off;
        if (off < table_end || off > len || len - off < PATCH_BYTES)
            return 0;
        for (int k = 0; k < PATCH_BYTES; k++)
            tmp[i].data[k] = data[off + k];
    }

    for (int i = 0; i < n; i++) {
        g_patches[i].key = tmp[i].key;
        g_patches[i].off = tmp[i].off;
        for (int k = 0; k < PATCH_BYTES; k++)
            g_patches[i].data[k] = tmp[i].data[k];
    }
    g_count = n;
    return 1;
}

int patches_count(void)
{
    return g_count;
}

const u8 *patches_lookup(u16 key)
{
    for (int i = 0; i < g_count; i++)
        if (g_patches[i].key == key)
            return g_patches[i].data;
    return NULL;
}
