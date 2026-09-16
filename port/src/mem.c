#include "mem.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

u8 mem[MEM_SIZE];

int mem_in_range(u32 addr, u32 len)
{
    if (len == 0) return 1;
    return addr < MEM_SIZE && len <= MEM_SIZE - addr;
}

void mem_fill(u32 addr, u8 value, u32 len)
{
    assert(mem_in_range(addr, len));
    for (u32 i = 0; i < len; i++) mem[addr + i] = value;
}

/* ---- LE loader --------------------------------------------------------- */

static u32 rd32(const u8 *p) { return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24); }
static u16 rd16(const u8 *p) { return (u16)((u32)p[0] | ((u32)p[1] << 8)); }

static long find_le(const u8 *d, size_t n)
{
    for (size_t i = 0; i + 0x84 <= n; i++) {
        if (d[i] != 'L' || d[i + 1] != 'E') continue;
        if (d[i + 2] != 0 || d[i + 3] != 0) continue;
        u16 cpu = rd16(d + i + 8), os = rd16(d + i + 10);
        u32 npages = rd32(d + i + 0x14), psz = rd32(d + i + 0x28);
        u32 nobj = rd32(d + i + 0x44), objtab = rd32(d + i + 0x40);
        if (cpu < 1 || cpu > 5 || os < 1 || os > 4) continue;
        if (psz != 0x200 && psz != 0x400 && psz != 0x1000 && psz != 0x2000 && psz != 0x4000) continue;
        if (nobj == 0 || nobj >= 64 || objtab >= 0x2000) continue;
        if (npages == 0 || npages >= 20000) continue;
        return (long)i;
    }
    return -1;
}

/* The bound-image base: the inner MZ header whose e_lfanew resolves to the LE
 * header. PRAGE.EXE is a DOS/4GW *bound* executable, so the LE header sits
 * 0x2A50 bytes into an embedded MZ image; page-data offsets are relative to
 * that image, not to the LE header. */
static long find_bound_base(const u8 *d, size_t n, long le)
{
    for (size_t i = 0; i + 0x40 <= n; i++) {
        if (d[i] != 'M' || d[i + 1] != 'Z') continue;
        u32 rel = rd32(d + i + 0x3C);
        if (rel != 0 && i + rel == (size_t)le) return (long)i;
    }
    return -1;
}

int mem_load_le(const char *exe_path, const char *object_bin_out)
{
    FILE *f = fopen(exe_path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return 0; }
    u8 *d = malloc((size_t)sz);
    if (!d) { fclose(f); return 0; }
    if (fread(d, 1, (size_t)sz, f) != (size_t)sz) { free(d); fclose(f); return 0; }
    fclose(f);

    long le = find_le(d, (size_t)sz);
    if (le < 0) { free(d); return 0; }

    u32 npages  = rd32(d + le + 0x14);
    u32 psz     = rd32(d + le + 0x28);
    u32 nobj    = rd32(d + le + 0x44);
    u32 objtab  = rd32(d + le + 0x40);
    u32 pagemap = rd32(d + le + 0x48);
    u32 pageoff = rd32(d + le + 0x80);

    /* PORT: tools/le_info.py read the LE header +0x80 field as a "data pages
     * offset relative to the LE header". In this DOS/4GW bound image it is
     * relative to the bound image base (the inner MZ), which find_bound_base()
     * locates; the LE header alone gives the wrong page data. */
    long bound = find_bound_base(d, (size_t)sz, le);
    u32 pagedata = (u32)((bound >= 0 ? (u32)bound : (u32)le) + pageoff);

    /* Page map: one 4-byte entry per page, indexed by (1-based page number - 1).
     * Entry 0 means "not present" and is zero-filled; a non-zero entry's high
     * word is the 1-based physical page number, stored at
     * pagedata + (phys - 1) * psz. TODO(verify): confirm this entry encoding
     * against the Ghidra byte-for-byte diff of the objects in Task 3. */
    for (u32 obj = 0; obj < nobj; obj++) {
        const u8 *e = d + le + objtab + obj * 24;
        u32 virt = rd32(e + 0);     /* object virtual size */
        u32 rel = rd32(e + 4);
        u32 pageidx = rd32(e + 12); /* 1-based number of the object's page 0 */
        u32 npg = rd32(e + 16);
        for (u32 p = 0; p < npg; p++) {
            u32 idx = pageidx - 1 + p;
            u32 entry = idx < npages ? rd32(d + le + pagemap + idx * 4) : 0;
            u32 dst = rel + p * psz;
            if (entry == 0) {
                if (mem_in_range(dst, psz)) mem_fill(dst, 0, psz);
                continue;
            }
            u32 phys = entry >> 16;
            if (phys == 0) phys = pageidx + p;
            if (phys == 0) continue;
            u32 src = pagedata + (phys - 1) * psz;
            u32 avail = src < (u32)sz ? (u32)sz - src : 0;
            if (avail > psz) avail = psz;
            if (avail == 0 || !mem_in_range(dst, avail)) continue;
            memcpy(mem + dst, d + src, avail);
            if (avail < psz && mem_in_range(dst, psz))
                mem_fill(dst + avail, 0, psz - avail);
        }
        /* Zero the object's BSS tail: the part of its virtual size beyond the
         * file-backed pages. Without this, repeated loads leak stale bytes from
         * whatever previously occupied mem[] (later tasks load files into
         * scratch mem[] space). */
        u32 mapped = npg * psz;
        if (virt > mapped && mem_in_range(rel + mapped, virt - mapped))
            mem_fill(rel + mapped, 0, virt - mapped);
    }

    if (object_bin_out) {
        size_t len = DATA_BASE + 0x8B0D0 - CODE_BASE;
        FILE *o = fopen(object_bin_out, "wb");
        int ok = o && fwrite(mem + CODE_BASE, 1, len, o) == len;
        if (o) ok = (fclose(o) == 0) && ok;
        if (!ok) { free(d); return 0; }
    }
    free(d);
    return 1;
}
