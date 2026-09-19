/* port/src/game/config.c */
#include "game/config.h"

#include "../mem.h"
#include "../symbols.h"

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
