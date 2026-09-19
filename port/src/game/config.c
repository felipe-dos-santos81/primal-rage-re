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

    if (cursor & 1u) {                          /* odd: seed is the low nibble */
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

    config_storage_touch(1u, 0u);                  /* 0x2DACA */
    config_storage_touch(2u, 0u);                  /* 0x2DAD4 */
    return 0u;
}
