/* port/src/game/config.h */
#ifndef PRAGE_GAME_CONFIG_H
#define PRAGE_GAME_CONFIG_H

#include "../types.h"

/* The EEPROM/config data layer (0x2Dxxx). The original keeps 63 packed fields in
 * the byte region DS_00105D88..(+0x1000) and describes each field's bit position
 * and width in the obj-0 descriptor table at 0x2D300. The port reads both out of
 * mem[]; no table or value is transcribed. The storage image (0x80CE4) and the
 * save/load I/O are declared no-ops this cycle (spec §7). */

/* 0x2D974. Reads the packed field `field` (0x00..0x3E) out of the config byte
 * region. Returns 0xFFFFFFFF when field > 0x3E. */
u32 config_field_get(u32 field);

/* 0x2DA0C. Writes `value` into the packed field `field` (0x00..0x3E), preserving
 * nibbles the field does not own and raising DS_00105DD8 (|1 for a trailing byte,
 * |6 always). Returns 0, or 0xFFFFFFFF when field > 0x3E. The storage-image
 * maintenance (0x2D4EC) is a no-op this cycle. */
u32 config_field_set(u32 field, u32 value);

/* 0x2CCD0. Walks the obj-1 menu-descriptor table `table` (records are a
 * contiguous array of stride 0x14: presence, shift, count, entries pointer) and
 * returns the bitfield of each record's first entry whose string starts with '*'
 * (the default selection), placed at that record's shift. */
u32 config_menu_default_bits(u32 table);

/* 0x2CADC. Writes the default config fields: 0x29 from the menu table, 0x35 and
 * 0x37 = 0xA0, 0x2A's low two bits = 3. The message draw (0x2F198), screen setup
 * (0x1AE20), storage write (0x2EA78) and cursor restore (0x2F280) are declared
 * no-ops (spec §4/§7). */
void config_set_defaults(void);

/* 0x2D6F8. Validates the stored config against the magic at DS_00105E30 and runs
 * the defaults path when it fails. With the storage layer stubbed (spec §7) the
 * stored image is absent, so the magic never matches and this always takes the
 * defaults path, as on a fresh machine. */
void config_validate(void);

/* ---- credit layer (0x2Cxxx) ---------------------------------------------
 * The credit counter DS_00105C00, the FREE PLAY flag DS_00105D60 and the
 * debit-suppression flag DS_00104B1F. Read and written where the raw does. */

/* 0x2CAA8. 1 when the machine is not in free play. */
u32 config_not_free_play(void);

/* 0x2CA2C. 1 when a credit is available or free play is on. */
u32 config_has_credit(void);

/* 0x2C060. Calls 0x2CAA8 then tail-jumps to 0x2CA2C, discarding the 0x2CAA8
 * result, so this is config_has_credit(). */
u32 config_credit_ready(void);

/* 0x2CA48. Free play -> 1; no credits -> 0; otherwise decrement one credit
 * (unless DS_00104B1F suppresses it) and -> 1. */
u32 config_credit_take(void);

/* 0x2CA7C(n). Free play -> 1; n > credits -> 0; otherwise subtract n (unless
 * DS_00104B1F suppresses it) and -> 1. */
u32 config_credit_spend(u32 n);

/* 0x2C06C. Writes the overlay's text row DS_00105C05. */
void config_set_credit_row(u8 row);

/* 0x2BF00. The init-time row write: 0x1D. One caller, 0x20CCC in 0x20C10. */
void config_set_credit_row_init(void);

#endif /* PRAGE_GAME_CONFIG_H */
