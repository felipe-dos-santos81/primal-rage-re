#include "mem.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

u8 mem[MEM_SIZE];

/* The registration table is port infrastructure, not game state: game state
 * lives in mem[] at its original offsets, but the mapping from original code
 * addresses to C functions only exists in the port. */
#define FN_TABLE_MAX 512
static struct { u32 addr; void (*fn)(void); } fn_table[FN_TABLE_MAX];
static u32 fn_table_len;

void fn_register(u32 orig_addr, void (*fn)(void))
{
    assert(fn_table_len < FN_TABLE_MAX);
    fn_table[fn_table_len].addr = orig_addr;
    fn_table[fn_table_len].fn = fn;
    fn_table_len++;
}

void (*fn_resolve(u32 orig_addr))(void)
{
    for (u32 i = 0; i < fn_table_len; i++)
        if (fn_table[i].addr == orig_addr) return fn_table[i].fn;
    return NULL;
}

u32 fn_origin(void (*fn)(void))
{
    for (u32 i = 0; i < fn_table_len; i++)
        if (fn_table[i].fn == fn) return fn_table[i].addr;
    return 0;
}

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

static u8 *slurp(const char *path, long *out_size)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return NULL; }
    u8 *d = malloc((size_t)sz);
    if (!d) { fclose(f); return NULL; }
    if (fread(d, 1, (size_t)sz, f) != (size_t)sz) { free(d); fclose(f); return NULL; }
    fclose(f);
    *out_size = sz;
    return d;
}

int mem_load_le(const char *exe_path, const char *object_bin_out)
{
    long sz;
    u8 *d = slurp(exe_path, &sz);
    if (!d) return 0;

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
     * pagedata + (phys - 1) * psz. Confirmed byte-for-byte: the loaded image
     * matches Ghidra's lx-loader (Task 3 oracle) for all 0x8B0D0 data bytes. */
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

    for (u32 obj = 0; obj < nobj; obj++) {
        if (!mem_load_le_fixups(exe_path, obj)) { free(d); return 0; }
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

int mem_load_le_fixups(const char *exe_path, u32 object_index)
{
    long sz;
    u8 *d = slurp(exe_path, &sz);
    if (!d) return 0;

    long le = find_le(d, (size_t)sz);
    if (le < 0) { free(d); return 0; }

    u32 npages = rd32(d + le + 0x14);
    u32 psz    = rd32(d + le + 0x28);
    u32 last   = rd32(d + le + 0x2c);
    u32 nobj   = rd32(d + le + 0x44);
    u32 objtab = rd32(d + le + 0x40);
    u32 fpt    = rd32(d + le + 0x68); /* fixup page table */
    u32 frt    = rd32(d + le + 0x6c); /* fixup record table */

    if (object_index >= nobj) { free(d); return 0; }
    if ((long)(le + fpt) + (long)(npages + 1) * 4 > sz || (long)(le + frt) > sz) {
        free(d);
        return 0;
    }

    const u8 *e = d + le + objtab + object_index * 24;
    u32 rel = rd32(e + 4), pageidx = rd32(e + 12), npg = rd32(e + 16);

    /* The fixup page table maps logical page -> [begin,end) in the record
     * table, so a page's records are contiguous. PRAGE.EXE only carries the
     * internal 32-bit offset form (source type 7, target type 0): the 32-bit
     * word at the source offset becomes target_base + target_offset, the same
     * absolute linear address Ghidra's lx-loader computes. Every other form is
     * reported (return 0), never skipped silently. */
    for (u32 p = 0; p < npg; p++) {
        u32 page = pageidx + p;
        long begin = (long)rd32(d + le + fpt + (page - 1) * 4);
        long end   = (long)rd32(d + le + fpt + page * 4);
        if (end < begin || (long)(le + frt + end) > sz) { free(d); return 0; }

        /* Ghidra gives the last page of the last object only lastPageSize
         * bytes, so fixups there clip at that length rather than at pageSize. */
        u32 pagelen = (object_index + 1 == nobj && p + 1 == npg) ? last : psz;

        for (long cur = 0; cur < end - begin; ) {
            const u8 *r = d + le + frt + begin + cur;
            u8 src = r[0], tf = r[1];
            /* Accept only the exact encodings this walker models, so an
             * unrecognised record is reported (return 0), never misparsed: the
             * source byte must be a plain 32-bit offset (0x07) -- no alias
             * (0x10), source list (0x20) or 0x40 source-location-size bit; the
             * target byte must be an internal reference (low 2 bits clear) with
             * only the 32-bit target offset (0x10) and 16-bit object (0x40)
             * flags this walker reads. Additive (0x04/0x20) and chaining (0x08)
             * records resize the record and are likewise rejected. */
            if (src != 0x07 || (tf & ~(u8)0x50) != 0) {
                free(d);
                return 0;
            }
            s32 src_off = (s16)rd16(r + 2);
            const u8 *q = r + 4;
            u32 target_obj;
            if (tf & 0x40) { target_obj = rd16(q); q += 2; } else { target_obj = *q++; }
            s32 target_off;
            if (tf & 0x10) { target_off = (s32)rd32(q); q += 4; }
            else { target_off = (s16)rd16(q); q += 2; if (target_off < 0) target_off += 0x10000; }
            if (target_obj < 1 || target_obj > nobj) { free(d); return 0; }

            u32 base = rd32(d + le + objtab + (target_obj - 1) * 24 + 4);
            if (base == 0) base = target_obj * 0x100000;
            u32 value = base + (u32)target_off;

            for (u32 k = 0; k < 4; k++) {
                s32 o = src_off + (s32)k;
                if (o < 0 || (u32)o >= pagelen) continue;
                u32 dst = rel + p * psz + (u32)o;
                if (mem_in_range(dst, 1)) mem[dst] = (u8)(value >> (8 * k));
            }
            cur += (long)(q - r);
        }
    }
    free(d);
    return 1;
}
