/* port/src/game/config.c */
#include "game/config.h"

#include "game/actors.h"
#include "game/flow.h"
#include "platform/gfx.h"
#include "platform/input.h"
#include "platform/render.h"
#include "../host.h"
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
 * no-ops (spec §4/§7), so only the config-field effect is ported. PORT: 0x2EA78
 * is ported (config_screen_wait, record §49-Y) but left unwired here: this
 * runs inside game_init, where the raw's 0x2CB55 0x2EA78(0xB4) would present
 * frames and wait 180 ticks before the loop starts, moving the frame-indexed
 * oracles. */
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
     * no-ops (spec §7; 0x2D638 and 0x2DE98 stay deferred, record §49-Y); a
     * storage-absent read reports -1, so there is no stored image, the magic
     * cannot match, and validate takes the defaults path. */
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

/* 0x2CA78 — record §48-E. `xor eax,eax; ret`: the return-0 tail that 0x2CA48
 * (0x2CA5E `je`) and 0x2CA7C (0x2CA91 `ja`) jump to, also called as a
 * function by 0x42F60 (0x42F7A), which makes that function's 0x2CA48 arm dead,
 * and by the unported 0x32F54 (0x32F56). Always 0. */
u32 config_credit_zero(void)
{
    return 0u;                                         /* 0x2CA78 */
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

/* ---- timed screen, key latch and menu helpers (record §49-Y) -------------- */

/* Key-layer globals of the 0x2Exxx cluster. The symbols generator names none
 * of them, so the raw addresses are local. */
#define CFG_KEY_WORD     0x00105F28u   /* 0x2EB3F: the last BIOS key word */
#define CFG_KEY_TIME     0x00105F2Cu   /* 0x2EB57: 0x500BB's tick at that key */
#define CFG_KEY_LATCH    0x00105F30u   /* 0x2EB66: ascii, or the scan code */
#define CFG_TICK_ISR     0x00101500u   /* 0x500BB `mov eax,[0x101500]` */
#define CFG_ISR_GATE     0x00104B22u   /* 0x1BDF8: the ISR runs unless this is 1 */
#define CFG_FRAME_WORD   0x000EF6DEu   /* 0x2EAF2: the word the ISR increments */
#define CFG_IDLE_FLAG    0x00107414u   /* 0x2EBA8 */
#define CFG_CODE_BASE    0x00107450u   /* 0x30608: the code-entry record */
#define CFG_HISCORE_TMP  0x03900000u   /* PORT: the raw's stack record, see below */

/* 0x50188 (PORT: flow.c owns the master loop's copy of these two stores and
 * keeps it static; 0x2EAD6 needs the same swap). */
static void cfg_swap_buffers(void)
{
    u32 t = DSD(DS_000E87A0);
    DSD(DS_000E87A0) = DSD(DS_000E87A4);
    DSD(DS_000E87A4) = t;
}

/* 0x2EA78 — record §49-Y. EAX = the tick count n (signed, kept in ECX). One
 * frame is built and presented without the game logic: 0x2A31C, the display
 * list sort 0x1C3FC and draw 0x14328, then the full copy of the back buffer
 * (0x655FF; the flag byte DS_001014FC is cleared) when DS_001014FC is set, else
 * the dirty-dword blit 0x501A3; then the buffer swap 0x50188 and the palette
 * flush 0x1C470. Then, for ECX = n down to -1 inclusive (n + 2 passes for
 * n >= 0, none for n == -1: 0x2EAE0 `cmp ecx,-1; jz`, 0x2EB6E..0x2EB74
 * `mov eax,ecx; dec ecx; cmp eax,-1; jg`), each pass waits for the word
 * DS_000EF6DE to change (0x2EAF2..0x2EB0A, serving the audio 0x1CF20 while it
 * waits), pumps the key bitmap (0x500C4) and drains the BIOS queue
 * (0x2EB11..0x2EB6C): each key stores DS_00105F28 = the key word and
 * DS_00105F2C = 0x500BB, and DS_00105F30 = the ascii byte, or the scan code
 * when the ascii byte is 0 (0x2EB5C..0x2EB66); the last key wins. Callers:
 * 0x24C5C (0x24E4B), 0x249F0 (0x24A3C) with EAX = -1, 0x2CADC (0x2CB5F,
 * EAX = 0xB4), 0x1A38C and 0x31FBF. */
void config_screen_wait(s32 n)
{
    s32 ecx = n;                                            /* 0x2EA7C */
    DSD(CFG_KEY_LATCH) = 0u;                                /* 0x2EA85 */
    actors_update();                                        /* 0x2EA8B */
    render_list_sort();                                     /* 0x2EA9A */
    /* PORT: 0x14328 is called with the camera at 0xBCD64 (clip at +8, EBX =
     * 0xBCD6C); its bytes equal the master loop's DS_000A87CC record that
     * render_list() draws with, and 0x2A31C/0x1C3FC preserve EBX/EDX. */
    render_list();                                          /* 0x2EAA4 */
    const u8 *src = mem + DSD(DS_000E87A4);                 /* 0x2EABC */
    if (DSB(DS_001014FC) != 0u)                             /* 0x2EAA9 */
        DSB(DS_001014FC) = 0u;                              /* 0x2EAC9 */
    /* PORT: the full copy 0x2EAC2 and the dirty blit 0x2EAD1 are the one
     * gfx_present (the aperture rule). The raw flushes the palette (0x2EADB)
     * after the copy; the host converts through gfx_dac at present time, so
     * the flush runs first here and the frame shows the palette the hardware
     * would already have loaded. */
    cfg_swap_buffers();                                     /* 0x2EAD6 */
    gfx_flush_palette();                                    /* 0x2EADB */
    gfx_present(src, 320, 200);                             /* 0x2EAC2/0x2EAD1 */
    if (ecx == -1) return;                                  /* 0x2EAE0 */
    for (;;) {
        u16 seen = DSW(CFG_FRAME_WORD);                     /* 0x2EAF2 */
        while (DSW(CFG_FRAME_WORD) == seen) {               /* 0x2EAFB..0x2EB03 */
            game_audio_service();                           /* 0x2EB05 0x1CF20 */
            /* PORT: the timer ISR 0x1BDF4 is modelled as one tick per
             * retrace, as the master loop's spin does: when the gate byte is
             * not 1 (0x1BDF8..0x1BE00) it adds 1 to DS_00101508 and
             * DS_00101500 (0x1BE0E..0x1BE16) and to the word DS_000EF6DE
             * (0x1BE21). Its calls 0x1BBAC and 0x2D62C are not ported. */
            if (DSB(CFG_ISR_GATE) != 1u) {
                DSD(DS_00101508)++;
                DSD(CFG_TICK_ISR)++;
                DSW(CFG_FRAME_WORD) = (u16)(DSW(CFG_FRAME_WORD) + 1u);
            }
            host_wait_vblank();
        }
        (void)input_pump();                                 /* 0x2EB0C 0x500C4 */
        for (;;) {
            u32 key = (u32)input_check_key() & 0xFFFFu;     /* 0x2EB11..0x2EB22 */
            if (key == 0u) break;                           /* 0x2EB27 */
            key = (u32)input_get_key();                     /* 0x2EB29..0x2EB3A */
            DSD(CFG_KEY_WORD) = key;                        /* 0x2EB3F */
            /* PORT: 0x2EB45 key word 0x2400 calls 0x5004A (the joystick
             * calibration); the port reads the keyboard only, as game_init's
             * 0x5004A call is left out in flow.c. */
            DSD(CFG_KEY_TIME) = DSD(CFG_TICK_ISR);          /* 0x2EB52/0x2EB57 */
            u32 latch = key;
            if ((latch & 0xFFu) == 0u) latch >>= 8;         /* 0x2EB5C..0x2EB61 */
            DSD(CFG_KEY_LATCH) = latch & 0xFFu;             /* 0x2EB64/0x2EB66 */
        }
        s32 prev = ecx;                                     /* 0x2EB6E */
        ecx--;                                              /* 0x2EB70 */
        if (!(prev > -1)) break;                            /* 0x2EB71/0x2EB74 */
    }
}

/* 0x2EB80 — record §49-Y. Returns the latched key DS_00105F30 when it is
 * non-zero (0x2EB81..0x2EB8E). Otherwise 0x500BB minus DS_00105F2C is tested
 * unsigned against 0x4B0 (0x2EB94..0x2EB9F `jbe`): at or under, 0 (0x2EBB8);
 * over, the idle timeout stores DS_00107414 = 0 (0x2EBA8) and calls
 * longjmp(0x1044F4, 1) (0x2EBAE..0x2EBB3, 0x65431). PORT: the longjmp quit
 * path is not modelled (spec §7); the store is kept and 0 is returned in its
 * place. Callers: 0x2EBF0 (0x2EBFD), 0x2FFC4 (0x303D9), 0x33058 (0x3306B),
 * 0x33230 (0x33247) and three unlisted sites. */
u32 config_key_latched(void)
{
    u32 latch = DSD(CFG_KEY_LATCH);                         /* 0x2EB81 */
    if (latch != 0u) return latch;                          /* 0x2EB87 */
    if (DSD(CFG_TICK_ISR) - DSD(CFG_KEY_TIME) > 0x4B0u) {   /* 0x2EB94..0x2EB9F */
        DSB(CFG_IDLE_FLAG) = 0u;                            /* 0x2EBA8 */
        /* PORT: 0x2EBB3 jmp 0x65431, longjmp(0x1044F4, 1). */
    }
    return 0u;                                              /* 0x2EBB8 */
}

/* One arrow-key arm of 0x2EBF0: `code` is the latched key, `off` the byte
 * index (0x2DE + n) of the first player's key and off + 8 the second's, `bits`
 * the pair of direction bits. Returns the bits to set, or 0. The first
 * player's arm is skipped while the word at +0x2D4 is 0, the second's while
 * the word at +0x2D6 is 0 (0x2EC6A, 0x2EC85 and the same pairs below). */
static u32 cfg_dir_bits(u32 kb, u32 off, u32 code, u32 bits)
{
    u32 k1 = DSB(kb + off);                                 /* 0x2EC45 */
    u32 k2 = DSB(kb + off + 8u);                            /* 0x2EC52 */
    if (k1 != code && k2 != code) return bits;              /* 0x2EC50/0x2EC5B */
    if (k1 == code && DSW(kb + 0x2D4u) == 0u) return 0u;    /* 0x2EC6A */
    if (k2 == code && DSW(kb + 0x2D6u) == 0u) return 0u;    /* 0x2EC85 */
    return bits;                                            /* 0x2EC93 */
}

/* 0x2EBF0 — record §49-Y. EAX = the mask (kept in EBX). Turns the latched key
 * (0x2EB80) into the game's key-bit word. 0 when nothing is latched
 * (0x2EC04). With mask 0 or any of 0xF300F000 set (0x2EC19..0x2EC2B: the
 * second test is a subset of the first), the arrow scan codes 0x48, 0x50,
 * 0x4B and 0x4D (jump table 0x2EBCC, 0x2EC31..0x2EC3D) map through the
 * key-config record at DS_00101514 (bytes +0x2DE..+0x2E1 and +0x2E6..+0x2E9,
 * words +0x2D4/+0x2D6) to 0x80008000, 0x40004000, 0x20002000 and 0x10001000.
 * The right arrow (0x2ED97) also zeroes the code when neither player's key
 * matches. Then Enter (0xD) gives 0x1000000 when the mask is 0 or has bit 24
 * (0x2EDA3..0x2EDB0), and Esc (0x1B) 0x2000000 when the mask is 0 or has
 * bit 25 (0x2EDBC..0x2EDC9). The DS_00101514 pointer is loaded and stored back
 * unchanged (0x2EBF5, 0x2EDD1). Callers: 0x2EDE0 (0x2EDFB), 0x2EEC8 (0x2EEE3). */
u32 config_key_flags(u32 mask)
{
    u32 kb = DSD(DS_00101514);                              /* 0x2EBF5 */
    u32 code = config_key_latched();                        /* 0x2EBFD */
    u32 out = 0u;                                           /* 0x2EC02 */
    if (code == 0u) {                                       /* 0x2EC06 */
        DSD(DS_00101514) = kb;                              /* 0x2EDD1 */
        return 0u;
    }
    if (mask == 0u || (mask & 0xF300F000u) != 0u) {         /* 0x2EC19..0x2EC2B */
        switch (code) {                                     /* 0x2EC31..0x2EC3D */
        case 0x48u: out |= cfg_dir_bits(kb, 0x2DEu, code, 0x80008000u); break;
        case 0x50u: out |= cfg_dir_bits(kb, 0x2DFu, code, 0x40004000u); break;
        case 0x4Bu: out |= cfg_dir_bits(kb, 0x2E0u, code, 0x20002000u); break;
        case 0x4Du: {
            u32 bits = cfg_dir_bits(kb, 0x2E1u, code, 0x10001000u);
            out |= bits;
            /* 0x2ED97: only the neither-key-matches arm zeroes EAX. */
            if (DSB(kb + 0x2E1u) != code && DSB(kb + 0x2E9u) != code) code = 0u;
            break;
        }
        default: break;                                     /* 0x2ED9F */
        }
    }
    if ((mask == 0u || (mask & 0x1000000u) != 0u) && code == 0xDu) {
        out |= 0x1000000u;                                  /* 0x2EDB0 */
        code = 0u;                                          /* 0x2EDB6 */
    }
    if ((mask == 0u || (mask & 0x2000000u) != 0u) && code == 0x1Bu)
        out |= 0x2000000u;                                  /* 0x2EDC9 */
    DSD(DS_00101514) = kb;                                  /* 0x2EDD1 */
    return out;                                             /* 0x2EDCF */
}

/* 0x2EDE0 — record §49-Y. EAX = the mask, DL = the flag. EDX = 0x50161(mask)
 * (0x2EDEA); with the flag set, EDX |= 0x2EBF0(mask) (0x2EDF9..0x2EE00). A
 * non-zero result stamps DS_00105F2C = 0x500BB (0x2EE04..0x2EE0B). Returns
 * EDX. Callers: 0x2FA40 (0x2FD29) and eighteen more. */
u32 config_input_poll(u32 mask, u8 flag)
{
    u32 r = input_select_bits(mask);                        /* 0x2EDEA */
    if (flag != 0u) r |= config_key_flags(mask);            /* 0x2EDF3..0x2EE00 */
    if (r != 0u) DSD(CFG_KEY_TIME) = DSD(CFG_TICK_ISR);     /* 0x2EE04..0x2EE0B */
    return r;                                               /* 0x2EE10 */
}

/* 0x2EEC8 — record §49-Y. 0x2EDE0 that also clears the latch: the key flags
 * (0x2EEE3) are taken first, then DS_00105F30 = 0 (0x2EEEC), then the stamp
 * on a non-zero result (0x2EEF2..0x2EEFB). Caller: 0x2FFC4 (0x303D2). */
u32 config_input_poll_clear(u32 mask, u8 flag)
{
    u32 r = input_select_bits(mask);                        /* 0x2EED2 */
    if (flag != 0u) r |= config_key_flags(mask);            /* 0x2EEDB..0x2EEE8 */
    DSD(CFG_KEY_LATCH) = 0u;                                /* 0x2EEEC */
    if (r != 0u) DSD(CFG_KEY_TIME) = DSD(CFG_TICK_ISR);     /* 0x2EEF2..0x2EEFB */
    return r;                                               /* 0x2EF00 */
}

/* 0x305FC — record §49-Y. EAX = the column, EDX = the row; the record is
 * DS_00107450: byte +2 the count, byte +3 the flags, bytes +4.. the first
 * text, bytes +0xD.. the second (eight columns each). With the flags byte
 * non-zero (0x3061D), the second text is drawn as one string at mode 0x1000
 * (0x30625..0x30638); flag bit 1 set returns (0x30647..0x30649). Otherwise
 * the first three characters (0x3064F..0x30667) are kept as a NUL-terminated
 * name, the decimal digits from the fourth character on are summed into a
 * number while each is signed-inside '0'..'9' (0x30673..0x306A6, at most to
 * index 7), the pair is inserted into the high-score table 2 at record 0
 * through 0x2DCA0 (0x306B2) and the flag bit 1 is set (0x306BB). With the flags
 * byte zero, the eight columns are drawn one character each (0x306CC..0x30710):
 * the first text's character at mode 0x4000 while the column index is below
 * the count byte (signed word compare, 0x306E8 `jl`), else the second text's
 * at 0x2000. Callers: 0x2FA40 (0x2FE2A) and 0x2FFC4 (0x305E2). */
void config_code_row(s32 col, s32 row)
{
    if (DSB(CFG_CODE_BASE + 3u) != 0u) {                    /* 0x3061D */
        u32 text = CFG_CODE_BASE + 0xDu;                    /* 0x30625..0x30631 */
        text_cursor_set(col, row, mem + text, 0x1000u);     /* 0x30638 */
        if ((DSB(CFG_CODE_BASE + 3u) & 2u) != 0u) return;   /* 0x3063D..0x30649 */
        /* PORT: the raw builds { u32 number; char name[4] } on its stack and
         * passes ESP to 0x2DCA0, which reads mem[]; the record lives at the
         * port scratch CFG_HISCORE_TMP instead. */
        u32 name = CFG_HISCORE_TMP + 4u;
        for (u32 i = 0u; i < 3u; i++)
            DSB(name + i) = DSB(text + i);                  /* 0x3064F..0x30665 */
        DSB(name + 3u) = 0u;                                /* 0x3066F */
        u32 num = 0u;                                       /* 0x30673 */
        for (s16 i = 3; i < 8; i++) {                       /* 0x3069E..0x306A4 */
            s32 c = (s8)DSB(text + (u32)i);                 /* 0x3067D..0x30685 */
            if (c < 0x30 || c > 0x39) break;                /* 0x30688/0x30690 */
            num = num * 10u + (u32)(c - 0x30);              /* 0x30695..0x3069C */
        }
        DSD(CFG_HISCORE_TMP) = num;                         /* 0x306AD */
        (void)hiscore_insert(0u, CFG_HISCORE_TMP, 2u);      /* 0x306B2 0x2DCA0 */
        DSB(CFG_CODE_BASE + 3u) |= 2u;                      /* 0x306BB */
        return;
    }
    u8 one[2];
    one[1] = 0u;                                            /* 0x30619 */
    for (s16 i = 0; i < 8; i++) {                           /* 0x3070A..0x30710 */
        u32 mode = 0x4000u;                                 /* 0x306E3 */
        one[0] = DSB(CFG_CODE_BASE + 4u + (u32)i);          /* 0x306D3 */
        if (i >= (s16)DSB(CFG_CODE_BASE + 2u)) {            /* 0x306E8/0x306EB */
            one[0] = DSB(CFG_CODE_BASE + 0xDu + (u32)i);    /* 0x306ED */
            mode = 0x2000u;                                 /* 0x306F0 */
        }
        text_cursor_set(col + i, row, one, mode);           /* 0x30704 */
    }
}

/* 0x30788 — record §49-Y. EAX = the value (clamped to 0..0xFF, signed),
 * EDX = the first row, EBX = the label row. A non-negative label row draws the
 * value as a three-wide number at column 0x19, mode 0xC002 (0x307B9..0x307CE
 * 0x2F434). Then a 32-cell bar of the glyph 0x13 fills columns 5..0x24 on
 * three rows (row, row + 1, row + 2), one call each per cell, for the cell
 * index i = 0, 8, .. 0xF8: mode 0xF000 when i > value (0x307F2), else 0x3000
 * when i > 0xBF (0x307FF), else 0x2000 when i > 0x81 (0x3080E), else the
 * previous mode (0x1000 to begin with). Callers: three unlisted sites in
 * 0x30C9E..0x30DA3. */
void config_bar_draw(s32 value, s32 row, s32 label_row)
{
    if (value < 0) value = 0;                               /* 0x3079B..0x3079F */
    if (value > 0xFF) value = 0xFF;                         /* 0x307A3..0x307AD */
    if (label_row >= 0)                                     /* 0x307B5 `jl` */
        text_number_set(0x19, label_row, value, 3, 2u, 0xC002u);   /* 0x307CE */
    u32 mode = 0x1000u;                                     /* 0x307D8 */
    s32 col = 5;                                            /* 0x307E5 */
    for (s32 i = 0; i <= 0xFF; i += 8) {                    /* 0x3084B, 0x30854 */
        if (i > value) mode = 0xF000u;                      /* 0x307F2 */
        else if (i > 0xBF) mode = 0x3000u;                  /* 0x307FF */
        else if (i > 0x81) mode = 0x2000u;                  /* 0x3080E */
        text_glyph_at(col, 0x13, row, mode);                /* 0x30828 */
        text_glyph_at(col, 0x13, row + 1, mode);            /* 0x3083A */
        text_glyph_at(col, 0x13, row + 2, mode);            /* 0x3084E */
        col++;                                              /* 0x30853 */
    }
}

/* 0x31E28 — record §49-Y. EAX = which, EDX = the record pointer, BL = the
 * flag. The flag picks the arrow-row mode: non-zero 0x3000, else 0x4000
 * (0x31E33..0x31E3E). which == 0 draws string 0x17 at column 2, row 4, mode
 * 0x2000 (0x31E47..0x31E72) and reads the word at +0; else string 0x16 at
 * column 0x16 (0x31E74..0x31E9D) and the word at +0x12; the arrow row's base
 * column EBP is 0xA or 0x1E. A value of 0, 2, 4 or 6 (jump table 0x31E0C; odd
 * values and above 6 skip, 0x31EA7 `ja`) draws string 0x22D, 0x22F, 0x230 or
 * 0x22E at column EBP - 8, row 6 in the arrow mode (0x31EB6..0x31EE8). Then
 * '<' (0x3C) at column EBP - 9 and '>' (0x3E) at column EBP + 9, row 6, mode
 * 0x4000 (0x31EED..0x31F16). Callers: 0x3216C, 0x32182, 0x32190, 0x32438,
 * 0x32446 (unlisted). */
void config_option_row(u32 which, u32 p, u8 flag)
{
    u32 arrow = (flag != 0u) ? 0x3000u : 0x4000u;           /* 0x31E33..0x31E3E */
    u32 value, base;
    if (which == 0u) {                                      /* 0x31E43 */
        value = DSW(p);                                     /* 0x31E56 */
        text_cursor_set(2, 4, game_string_get(0x17u), 0x2000u);   /* 0x31E61 0x31EA2 */
        base = 0xAu;                                        /* 0x31E6D */
    } else {
        value = DSW(p + 0x12u);                             /* 0x31E83 */
        text_cursor_set(0x16, 4, game_string_get(0x16u), 0x2000u);   /* 0x31E91 0x31EA2 */
        base = 0x1Eu;                                       /* 0x31E9D */
    }
    u32 id = 0u;
    switch (value) {                                        /* 0x31EA7..0x31EBC */
    case 0u: id = 0x22Du; break;                            /* 0x31EC3 */
    case 2u: id = 0x22Fu; break;                            /* 0x31ED1 */
    case 4u: id = 0x230u; break;                            /* 0x31ED8 */
    case 6u: id = 0x22Eu; break;                            /* 0x31ECA */
    default: break;                                         /* 0x31EED */
    }
    if (id != 0u)
        text_cursor_set((s32)base - 8, 6, game_string_get(id), arrow);   /* 0x31EDD 0x31EE8 */
    text_glyph_at((s32)base - 9, 0x3C, 6, 0x4000u);         /* 0x31F02 */
    text_glyph_at((s32)base + 9, 0x3E, 6, 0x4000u);         /* 0x31F16 */
}

/* 0x3157C — record §49-Y. EAX = the key word (ascii low byte, scan code high
 * byte), DL = the raw flag, EBX = the destination text. Writes the key's name
 * and returns 1, or returns 0 for a key with none. The ascii byte 1..0x7F
 * other than 0xD and 0x20 (0x31591..0x315A0) stands as itself (class 2,
 * 0x31696: "%c" at 0x80B78); otherwise the scan code selects: 0xE and 0xF
 * strings 0x22A and 0x22B; 0x47 0x220; 0x48 0x223; 0x49 0x221; 0x4B 0x224;
 * 0x4D 0x225; 0x4F 0x227; 0x50 0x226; 0x51 0x222; 0x52 0x228; 0x53 0x229;
 * 0x3D..0x43 the texts at 0x80B7C..0x80B94 ("F3".."F9"); 0x44 the dword at
 * 0x80B98 ("F10"); 0x4A the word at 0x80B9C ("-"); 0x4E the word at 0x80BA0
 * ("+"). With the raw flag zero the name is then wrapped as "<%s>" (0x80BA4,
 * 0x316AC..0x316E0). The rest, including the scan codes 0..1, 3..0xD, 0x10..
 * 0x3C, 0x45, 0x46, 0x4C and above 0x53, return 0. PORT: the raw formats
 * into a 12-byte stack buffer; the wrap here uses a larger one. Callers:
 * 0x31A78 (0x31AC4) and the unlisted sites 0x19DE6..0x1A3EC and
 * 0x31D08..0x31DC0. */
u32 config_key_name(u32 key, u8 raw, u32 dest)
{
    u32 ch = key & 0xFFu;                                   /* 0x3158B */
    u32 code = (u32)((s32)key >> 8);                        /* 0x31588 */
    if (ch != 0u && ch <= 0x7Fu && ch != 0xDu && ch != 0x20u)
        code = 2u;                                          /* 0x31591..0x315A2 */
    const u8 *str = NULL;
    u32 lit = 0u, lit_len = 0u;
    switch (code) {                                         /* 0x315A7..0x31994 */
    case 2u:
        DSB(dest) = (u8)ch;                                 /* 0x31696 %c */
        DSB(dest + 1u) = 0u;
        break;
    case 0xEu:  str = game_string_get(0x22Au); break;       /* 0x318A9 */
    case 0xFu:  str = game_string_get(0x22Bu); break;       /* 0x318D6 */
    case 0x47u: str = game_string_get(0x220u); break;       /* 0x316EA */
    case 0x48u: str = game_string_get(0x223u); break;       /* 0x3176E */
    case 0x49u: str = game_string_get(0x221u); break;       /* 0x31714 */
    case 0x4Bu: str = game_string_get(0x224u); break;       /* 0x3179B */
    case 0x4Du: str = game_string_get(0x225u); break;       /* 0x317C8 */
    case 0x4Fu: str = game_string_get(0x227u); break;       /* 0x31822 */
    case 0x50u: str = game_string_get(0x226u); break;       /* 0x317F5 */
    case 0x51u: str = game_string_get(0x222u); break;       /* 0x31741 */
    case 0x52u: str = game_string_get(0x228u); break;       /* 0x3184F */
    case 0x53u: str = game_string_get(0x229u); break;       /* 0x3187C */
    case 0x3Du: lit = 0x80B7Cu; lit_len = 3u; break;        /* 0x31903 */
    case 0x3Eu: lit = 0x80B80u; lit_len = 3u; break;        /* 0x31912 */
    case 0x3Fu: lit = 0x80B84u; lit_len = 3u; break;        /* 0x31921 */
    case 0x40u: lit = 0x80B88u; lit_len = 3u; break;        /* 0x31930 */
    case 0x41u: lit = 0x80B8Cu; lit_len = 3u; break;        /* 0x3193F */
    case 0x42u: lit = 0x80B90u; lit_len = 3u; break;        /* 0x3194E */
    case 0x43u: lit = 0x80B94u; lit_len = 3u; break;        /* 0x3195D */
    case 0x44u: lit = 0x80B98u; lit_len = 4u; break;        /* 0x3196C */
    case 0x4Au: lit = 0x80B9Cu; lit_len = 2u; break;        /* 0x31978 */
    case 0x4Eu: lit = 0x80BA0u; lit_len = 2u; break;       /* 0x31986 */
    default: return 0u;                                     /* 0x31645/0x31994 */
    }
    if (str != NULL) {
        u32 n = 0u;                                         /* 0x316F9..0x3170F */
        do { DSB(dest + n) = str[n]; } while (str[n++] != 0u);
    } else if (lit_len != 0u) {
        for (u32 i = 0u; i < lit_len; i++) DSB(dest + i) = DSB(lit + i);
    }
    if (raw == 0u) {                                        /* 0x316AC..0x316AE */
        u8 tmp[0x120];
        u32 n = 0u;
        tmp[n++] = '<';                                     /* 0x316BB "<%s>" */
        for (u32 i = 0u; DSB(dest + i) != 0u && n < sizeof tmp - 2u; i++)
            tmp[n++] = DSB(dest + i);
        tmp[n++] = '>';
        tmp[n] = 0u;
        for (u32 i = 0u; i <= n; i++) DSB(dest + i) = tmp[i];   /* 0x316C8..0x316DC */
    }
    return 1u;                                              /* 0x316E1 */
}
