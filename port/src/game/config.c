/* port/src/game/config.c */
#include "game/config.h"

#include "../mem.h"
#include "../symbols.h"

#include <stddef.h>
#include <string.h>

/* 0x2D974. The walk mirrors the raw exactly (spec §3): the descriptor gives a bit
 * position and a width; the value is assembled from the byte/nibble array ending
 * at DS_00105DE1 + ((bitpos + width) >> 1) and walking downward, with the final
 * unit depending on whether the width lands on a nibble boundary, then an optional
 * trailing byte from DS_00105DAF + (descriptor & 0x3f). */
u32 config_field_get(u32 field)
{
    if (field > 0x3Eu) return 0xFFFFFFFFu;

    u32 d = DSD(DS_0002D300 + field * 4u);
    u32 width  = ((d >> 14) & 7u) + 1u;
    u32 cursor = ((d >> 6) & 0xFFu) + width;   /* 0x13D9A1: bitpos + width */
    u32 idx    = cursor >> 1;                  /* 0x13D9A5: sar ecx,1 */
    u32 value;
    s32 ebx;

    if (cursor & 1u) {                          /* 0x13D9A7: odd start seeds the low nibble */
        value = DSB(DS_00105DE1 + idx) & 0x0Fu;
        ebx = (s32)idx;
        width -= 1u;
    } else {
        value = 0u;
        ebx = (s32)idx;
    }

    while (width != 0u) {
        ebx -= 1;                               /* 0x13D9C5: dec ebx before the read */
        if (width == 1u) {                      /* 0x13D9CB: final high nibble */
            u32 hi = (DSB(DS_00105DE1 + (u32)ebx) >> 4) & 0x0Fu;
            value = (value << 4) | hi;
            break;
        }
        value = (value << 8) | DSB(DS_00105DE1 + (u32)ebx);   /* 0x13D9E0 */
        width -= 2u;
    }

    u32 trail = d & 0x3Fu;                      /* 0x13D9F2 */
    if (trail != 0u)
        value = (value << 8) | DSB(DS_00105DAF + trail);
    return value;
}

/* 0x2D4EC. Maintains the EEPROM storage image at 0x80CE4, which the port does not
 * keep (no save/load I/O, spec §7). The setter's three calls are declared no-ops
 * so a later persistence cycle has the call sites already in place. */
static void config_storage_touch(u32 kind, u32 value)
{
    (void)kind;
    (void)value;
}

/* 0x2DA0C. Inverse of config_field_get (spec §3). */
u32 config_field_set(u32 field, u32 value)
{
    if (field > 0x3Eu) return 0xFFFFFFFFu;

    u32 d = DSD(DS_0002D300 + field * 4u);

    u32 trail = d & 0x3Fu;
    if (trail != 0u) {
        u8 flags = DSB(DS_00105DD8);
        DSB(DS_00105DAF + trail) = (u8)value;      /* 0x2DA34 */
        flags |= 1u;                               /* 0x2DA3B */
        value >>= 8;
        DSB(DS_00105DD8) = flags;
        config_storage_touch(0u, value);           /* 0x2DA49 */
    }

    DSB(DS_00105DD8) |= 6u;                        /* 0x2DA4E */

    u32 bitpos = (d >> 6) & 0xFFu;
    u32 width  = ((d >> 14) & 7u) + 1u;
    s32 ebx = (s32)bitpos >> 1;                    /* 0x2DA6B: sar ebx,1 */

    if (bitpos & 1u) {                             /* 0x2DA6D */
        u8 low = DSB(DS_00105DE1 + (u32)ebx) & 0x0Fu;
        ebx += 1;                                  /* 0x2DA8A: inc ebx */
        width -= 1u;
        /* 0x2DA90 writes at [ebx + 0x85de0], i.e. DS_00105DE1 + (bitpos>>1). */
        DSB(DS_00105DE0 + (u32)ebx) = (u8)(low | ((value & 0x0Fu) << 4));
        value >>= 4;
    }

    for (;;) {
        if (width == 0u) break;                    /* 0x2DA96 */
        if (width == 1u) {                         /* 0x2DA9A */
            u8 high = DSB(DS_00105DE1 + (u32)ebx) & 0xF0u;
            DSB(DS_00105DE1 + (u32)ebx) = (u8)(high | (value & 0x0Fu));
            break;
        }
        width -= 2u;
        DSB(DS_00105DE1 + (u32)ebx) = (u8)value;   /* 0x2DAB9 */
        ebx += 1;
        value >>= 8;
    }

    /* TODO(verify): the raw's edx at 0x2DACA/0x2DAD4 still holds the value shifted
     * out by the write loop, not 0. The 0u placeholders are safe only because
     * config_storage_touch is a no-op; a persistence cycle must re-derive both
     * arguments from those sites instead of trusting them. */
    config_storage_touch(1u, 0u);                  /* 0x2DACA */
    config_storage_touch(2u, 0u);                  /* 0x2DAD4 */
    return 0u;
}

/* 0x2CCD0. Records are a contiguous array of stride 0x14: [0] presence,
 * [4] shift (only cl is read; x86 masks the shift count), [8] count, [0x10]
 * entries pointer. Entries are stride 8 with [0] a char *; the inner loop stops
 * at the first '*' and sets that entry's index at the record's shift. */
u32 config_menu_default_bits(u32 table)
{
    u32 bits = 0u;
    u32 rec = table;
    while (DSD(rec) != 0u) {
        u32 shift   = DSD(rec + 4u) & 0x1Fu;       /* 0x2CCFA: cl only */
        u32 count   = DSD(rec + 8u);
        u32 entries = DSD(rec + 16u);
        u32 found   = 0u;
        for (u32 i = 0u; i < count && found == 0u; i++) {
            u32 str = DSD(entries + i * 8u);
            if (DSB(str) == (u8)'*') {             /* 0x2CCF3 */
                found = 1u;
                bits |= i << shift;                /* 0x2CD00 */
            }
        }
        rec += 0x14u;                              /* 0x2CD1A: not a pointer chase */
    }
    return bits;
}

/* 0x2CADC. Order and values are the raw's. The message draw 0x2F198, screen
 * setup 0x1AE20, storage write 0x2EA78 and cursor restore 0x2F280 are declared
 * no-ops (spec §4/§7), so only the config-field effect is ported. */
void config_set_defaults(void)
{
    u32 v29 = config_menu_default_bits(0xA2EB4u);  /* 0x2CAF8: obj-1 menu table */
    config_field_set(0x29u, v29);
    config_field_set(0x35u, 0xA0u);
    config_field_set(0x37u, 0xA0u);
    u32 v2a = config_field_get(0x2Au);
    config_field_set(0x2Au, (v2a & 0xFCu) | 3u);
}

/* 0x2D6F8, no-storage path. */
void config_validate(void)
{
    DSB(DS_00105DA5) = 0u;                         /* 0x2D70F */
    DSB(DS_00105DA4) = 0u;                         /* 0x2D715 */

    /* PORT: 0x2D638's storage load and the two 0x2E990 reads are declared
     * no-ops (spec §7); a storage-absent read reports -1, so there is no stored
     * image, the magic cannot match, and validate takes the defaults path. */
    s32 read1 = -1;
    s32 read2 = -1;

    /* 0x2D754: taken only when a read failed AND DS_0002D490 is clear. The
     * shipped image holds 4 there, so this never fires. */
    if ((read1 < 0 || read2 < 0) && DSB(DS_0002D490) == 0u)
        return;

    /* 0x2D76F: version arms. With both reads -1 neither runs; the raw's
     * stack-buffer copy into DS_00105DE1 has no stored image to copy. */
    if (read2 > read1)
        DSB(DS_00105DD8) |= 2u;                    /* 0x2D786 */
    else if (read1 > read2)
        DSB(DS_00105DD8) |= 4u;                    /* 0x2D7DC */

    /* 0x2D820: the four bytes at DS_00105E30 read as a little-endian u32. */
    if (DSD(DS_00105E30) != 0x9C94D2C4u || read1 < -1) {
        /* 0x2D83A: defaults. The two 0x61A70 calls clear the unported pset pool
         * (no-op); DS_00105E2F is cleared before the defaults writer runs. */
        DSB(DS_00105DD8) |= 6u;                    /* 0x2D84F */
        DSB(DS_00105DD8) |= 1u;                    /* 0x2D86F */
        DSB(DS_00105E2F) = 0u;                     /* 0x2D881 */
        config_set_defaults();                     /* 0x2D886 */
        DSD(DS_00105E30) = 0x9C94D2C4u;            /* 0x2D88D */
    }

    /* TODO(verify): the raw also performs three 0x2D4EC storage-maintenance calls
     * on this path (0x2D8A5, 0x2D8AF, 0x2D909); the port omits them because
     * storage is a no-op. A persistence cycle must add them rather than leave the
     * storage image under-maintained. */

    /* 0x2D912/0x2D919: high-score validate 0x2DE98 and 0x2DF8C, both deferred
     * no-ops (spec §6/§7); called unconditionally. */
    if (read1 >= 0 || DSB(DS_0002D490) != 0u) {
        DSB(DS_00105DA7) = 1u;                     /* 0x2D940 */
        (void)config_field_get(0x24u);             /* 0x2D946: result discarded */
        /* 0x2D962 calls 0x2DAE4(0x24) only when DS_00105DA4 + DS_00105DA5 != 0;
         * both are 0 on this path, and 0x2DAE4 is a deferred no-op (spec §7). */
    }
}

/* ---- high-score tables (record §46-A) ------------------------------------- */

/* 0x2DB58 — record §46-A. EAX = rec, EDX = table, ECX = size_out, EBX =
 * left_out. The descriptor is the 8-byte 0x2D3FC[table]: +0 the count, +4 the
 * value bytes, +6 the name bytes; the RAM block is [0x2D478 + 8*table]. */
u32 hiscore_locate(u32 rec, u32 table, u32 *left_out, u32 *size_out)
{
    if (table >= 3u) return 0u;                              /* 0x2DB5F `jc` */
    u32 desc = DS_0002D3FC + table * 8u;                     /* 0x2DB68 */
    u32 count = DSW(desc);                                   /* 0x2DB77 */
    if (rec >= count) return 0u;                             /* 0x2DB7A `jc` */
    u32 size = (u32)DSW(desc + 4u) + (u32)DSW(desc + 6u);    /* 0x2DB8D..0x2DB9B */
    if (left_out != NULL) *left_out = (count - rec) * size;  /* 0x2DBA4/0x2DBA7 */
    if (size_out != NULL) *size_out = size;                  /* 0x2DBAD */
    return rec * size + DSD(DS_0002D478 + table * 8u);       /* 0x2DBAF..0x2DBB9 */
}

/* 0x2DBC4 — record §46-A. EAX = rec, EDX = table. The value bytes are read
 * big-endian into DS_00105EFC, each name word gives three 5-bit characters
 * (+0x40, or a space for 0) at DS_00105F00, NUL-terminated. Returns 0x105EFC,
 * or 0 when 0x2DB58 finds no record. */
u32 hiscore_read(u32 rec, u32 table)
{
    u32 p = hiscore_locate(rec, table, NULL, NULL);          /* 0x2DBD1 */
    if (p == 0u) return 0u;                                  /* 0x2DBDA */
    u32 desc = DS_0002D3FC + table * 8u;                     /* 0x2DBE0 */
    u32 value = 0u;
    for (u32 n = DSW(desc + 4u); n != 0u; n--)               /* 0x2DBF5..0x2DC00 */
        value = (value << 8) | DSB(p++);
    u32 out = DS_00105F00;                                   /* 0x2DC02 */
    DSD(DS_00105EFC) = value;                                /* 0x2DC07 */
    for (u32 n = DSW(desc + 6u); n != 0u; n -= 2u) {         /* 0x2DC0C..0x2DC8D */
        u32 w = (u32)DSB(p) | ((u32)DSB(p + 1u) << 8);       /* 0x2DC25..0x2DC2D */
        p += 2u;
        for (u32 k = 0u; k < 3u; k++) {                      /* 0x2DC32/0x2DC54/0x2DC75 */
            DSB(out++) = (u8)((w & 0x1Fu) != 0u ? (w & 0x1Fu) + 0x40u : 0x20u);
            w >>= 5;
        }
    }
    DSB(out) = 0u;                                           /* 0x2DC94 */
    return DS_00105EFC;                                      /* 0x2DC8F */
}

/* 0x2DCA0 — record §46-A. EAX = rec, EDX = src (a u32 value, then the name),
 * EBX = table. Inserts the record: the records from `rec` move down one
 * (0x653A1, a memmove; the last drops), then the value is stored big-endian
 * and the name packed three characters to a word. Returns 1, or 0 when 0x2DB58
 * finds no record. */
u32 hiscore_insert(u32 rec, u32 src, u32 table)
{
    u32 left = 0u, size = 0u;
    u32 p = hiscore_locate(rec, table, &left, &size);        /* 0x2DCBB */
    if (p == 0u) return 0u;                                  /* 0x2DCC4 */
    u32 desc = DS_0002D3FC + table * 8u;                     /* 0x2DCCD */
    if ((u32)DSW(desc + 2u) > rec)                           /* 0x2DCE4 `jbe` */
        DSB(DS_00105DD8 + ((table + 6u) >> 3)) |=
            (u8)(1u << ((table + 6u) & 7u));                 /* 0x2DCE8..0x2DD00 */
    if ((s32)left > (s32)size)                               /* 0x2DD0A `jle` */
        memmove(mem + p + size, mem + p, left - size);       /* 0x2DD1B 0x653A1 */
    u32 nval = DSW(desc + 4u);
    u32 value = DSD(src);                                    /* 0x2DD26 */
    for (u32 n = nval; n != 0u; n--) {                       /* 0x2DD2F..0x2DD37 */
        DSB(p + n - 1u) = (u8)value;
        value >>= 8;
    }
    p += nval;                                               /* 0x2DD45 */
    u32 s = src + 4u;                                        /* 0x2DD47 */
    for (u32 n = DSW(desc + 6u); n != 0u; n -= 2u) {         /* 0x2DD4A..0x2DDB6 */
        u32 w = 0u;
        for (u32 k = 0u; k < 3u; k++) {                      /* 0x2DD52/0x2DD6E/0x2DD8B */
            s32 c = (s8)DSB(s);                              /* movsx */
            if (c != 0 && c != 0x20) {
                w |= ((u32)c & 0x1Fu) << (5u * k);
                s++;
            } else if (c == 0x20) {
                s++;
            }
        }
        DSB(p) = (u8)w;                                      /* 0x2DDA9 */
        DSB(p + 1u) = (u8)(w >> 8);                          /* 0x2DDB0 */
        p += 2u;
    }
    /* PORT: 0x2DDCD 0x2D4EC(table + 6), run when rec is below the stored count,
     * rewrites the storage image's check bytes; a no-op here, as in 0x2DA0C
     * (spec §7). */
    return 1u;                                               /* 0x2DDD2 */
}

/* ---- credit layer -------------------------------------------------------- */

/* 0x2CAA8. `cmp byte [0x85d60],0; sete al; and eax,0xff`. */
u32 config_not_free_play(void)
{
    return DSB(DS_00105D60) == 0u ? 1u : 0u;
}

/* 0x2CA2C. `mov edx,[0x85c00]; mov al,[0x85d60]; or eax,edx; setne al`. */
u32 config_has_credit(void)
{
    return (u32)((DSB(DS_00105D60) | DSD(DS_00105C00)) != 0u);
}

/* 0x2C060. `call 0x2caa8; jmp 0x2ca2c` — the 0x2CAA8 result is discarded. */
u32 config_credit_ready(void)
{
    (void)config_not_free_play();
    return config_has_credit();
}

/* 0x2CA48. Free play, then the zero-credit guard, then the suppressed debit. */
u32 config_credit_take(void)
{
    if (DSB(DS_00105D60) != 0u) return 1u;            /* 0x2CA51 */
    if (DSD(DS_00105C00) == 0u) return 0u;            /* 0x2CA5E */
    if (DSB(DS_00104B1F) == 0u) DSD(DS_00105C00)--;   /* 0x2CA69 */
    return 1u;
}

/* 0x2CA7C. `cmp eax,[0x85c00]; ja` is an unsigned guard. */
u32 config_credit_spend(u32 n)
{
    if (DSB(DS_00105D60) != 0u) return 1u;            /* 0x2CA85 */
    if (n > DSD(DS_00105C00)) return 0u;              /* 0x2CA91 */
    if (DSB(DS_00104B1F) == 0u) DSD(DS_00105C00) -= n;/* 0x2CA9C */
    return 1u;
}

/* 0x2C06C. `mov byte [0x85c05], al`. */
void config_set_credit_row(u8 row)
{
    DSB(DS_00105C05) = row;
}

/* 0x2BF00. `mov byte [0x85c05], 0x1d`. */
void config_set_credit_row_init(void)
{
    DSB(DS_00105C05) = 0x1Du;
}

/* 0x2C304 — record §46-F. `mov eax,0x29; call 0x2D974; and eax,0xf0000; sar
 * eax,0x10; inc eax; mov [0x105c00],eax`. */
void config_credits_init(void)
{
    DSD(DS_00105C00) = ((config_field_get(0x29u) & 0xF0000u) >> 16) + 1u;  /* 0x2C309..0x2C317 */
}

/* 0x32A3C — derivation record §42-E. EAX = the mode (only its low two bits
 * index, 0x32A43 `and edx,3`), EDX = the flag, kept in ECX (0x32A3F). Takes
 * the per-mode tick accumulator DS_0010746C[mode & 3] and zeroes it
 * (0x32A4F/0x32A56); mode 0 stops there (0x32A5F). */
void config_play_time_close(u32 mode, u32 flag)
{
    u32 idx = mode & 3u;                                /* 0x32A43 */
    /* PORT: 0x32A4A 0x32970(EAX = 0), the run clock, is out of scope (spec
     * §7); the host clock owns wall time. Its stores, for a later port:
     * DS_0010747C = DS_00105D88; the elapsed ticks added into
     * DS_0010746C[k + 1] for each set bit k of DS_00107494, into
     * DS_00107484 and into DS_00107488[DS_00107494]; at DS_00107484 >=
     * 0x3840 it zeroes it and, for each DS_00107488[i] > 0xE10 (i = 0..2),
     * keeps the remainder mod 0xE10 and posts 0x2DAE4(3 + i, quotient);
     * and DS_00107494 = AL = 0 (0x32A28), clearing the mode mask. It
     * preserves EDX (0x32972 push). */
    u32 ticks = DSD(DS_0010746C + idx * 4u);            /* 0x32A4F */
    DSD(DS_0010746C + idx * 4u) = 0u;                   /* 0x32A56 */
    (void)ticks;
    (void)flag;
    /* PORT: the audit adds through 0x2DAE4 (deferred no-op, spec §7) that
     * follow for mode != 0, with t = ticks / 0x3C (unsigned DIV):
     *   mode 1, flag == 0: (8, 1), (0xA, t), (0x12, t)          0x32A7B..
     *   mode 1, flag != 0: (6, 1), (0xB, 1), (0xC, t), (0x12, t) 0x32A91..
     *   mode 2/3:          (flag == 0 ? 9 : 7, 1), (0x13, t)     0x32AC4.. */
}

/* 0x32B00 — record §48-Q. EAX = the index (0x32B08 `shl eax,2`, no mask),
 * EDX = the arm; EBX/ECX are pushed and popped, EDX is not written. EDX != 0:
 * DS_00107478 (= DS_0010746C[3]) takes DS_0010746C[idx] (0x32B0F, stored at
 * 0x32B40). EDX == 0: t = (DS_0010746C[idx] - DS_00107478) / 0x3C (unsigned
 * DIV with EDX = 0, 0x32B17..0x32B24) is stored (0x32B2F), posted through
 * 0x2E934(1, t), then DS_00107478 is reloaded and stored back
 * (0x32B3A/0x32B40). Callers: 0x28E3F (0x28DA4, EDX = 1), 0x25EE5 (0x25C88)
 * and 0x28937 (0x28788). */
void config_play_time_snap(u32 idx, u32 arm)
{
    if (arm != 0u) {                                    /* 0x32B0B `jz` */
        DSD(DS_00107478) = DSD(DS_0010746C + idx * 4u); /* 0x32B0F/0x32B40 */
        return;
    }
    DSD(DS_00107478) = (DSD(DS_0010746C + idx * 4u)
                        - DSD(DS_00107478)) / 0x3Cu;    /* 0x32B17..0x32B2F */
    /* PORT: 0x32B35 0x2E934(1, t), the audit post, is deferred (spec §7).
     * Neither it nor its callees 0x2E180, 0x2E0A4 and 0x2E034 name 0x107478,
     * so the reload and store at 0x32B3A/0x32B40 leave the value as stored. */
}

/* 0x32B4C — record §48-U. 0x32B00 on DS_00107480 in place of DS_00107478,
 * posting to audit counter 0 (0x32B76 `xor eax,eax`) in place of 1: EAX = the
 * index (0x32B54 `shl eax,2`, no mask), EDX = the arm; EBX/ECX pushed and
 * popped. EDX != 0: DS_00107480 = DS_0010746C[idx] (0x32B5B, stored at
 * 0x32B89). EDX == 0: t = (DS_0010746C[idx] - DS_00107480) / 0x3C (unsigned
 * DIV with EDX = 0) is stored (0x32B78), posted through 0x2E934(0, t), and
 * DS_00107480 is reloaded and stored back (0x32B83/0x32B89). Callers:
 * 0x25EFD (0x25C88, EDX = 1) and 0x27DEB (0x27DC8, EDX = 0, record §48-K). */
void config_play_time_snap_b(u32 idx, u32 arm)
{
    if (arm != 0u) {                                    /* 0x32B57 `jz` */
        DSD(DS_00107480) = DSD(DS_0010746C + idx * 4u); /* 0x32B5B/0x32B89 */
        return;
    }
    DSD(DS_00107480) = (DSD(DS_0010746C + idx * 4u)
                        - DSD(DS_00107480)) / 0x3Cu;    /* 0x32B63..0x32B78 */
    /* PORT: 0x32B7E 0x2E934(0, t), the audit post, is deferred (spec §7);
     * as for 0x32B00, it names neither 0x107480 nor anything the reload at
     * 0x32B83 reads, so the value stays as stored. */
}
