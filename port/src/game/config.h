/* port/src/game/config.h */
#ifndef PRAGE_GAME_CONFIG_H
#define PRAGE_GAME_CONFIG_H

#include "../types.h"

/* The EEPROM/config data layer (0x2Dxxx). The original keeps 63 packed fields in
 * the byte region DS_00105D88..(+0x1000) and describes each field's bit position
 * and width in the obj-0 descriptor table at 0x2D300. The port reads both out of
 * mem[]; no table or value is transcribed. The storage image (DS 0x80CE4,
 * linear 0x100CE4) and its save file are the storage section below (record
 * 2026-10-13-reverse-s). */

/* 0x2D974. Reads the packed field `field` (0x00..0x3E) out of the config byte
 * region. Returns 0xFFFFFFFF when field > 0x3E. */
u32 config_field_get(u32 field);

/* 0x2DA0C. Writes `value` into the packed field `field` (0x00..0x3E), preserving
 * nibbles the field does not own and raising DS_00105DD8 (|1 for a trailing byte,
 * |6 always). Returns 0, or 0xFFFFFFFF when field > 0x3E. Each call re-encodes
 * the image's records through 0x2D4EC (config_storage_touch). */
u32 config_field_set(u32 field, u32 value);

/* 0x2CCD0. Walks the obj-1 menu-descriptor table `table` (records are a
 * contiguous array of stride 0x14: presence, shift, count, entries pointer) and
 * returns the bitfield of each record's first entry whose string starts with '*'
 * (the default selection), placed at that record's shift. */
u32 config_menu_default_bits(u32 table);

/* 0x2CADC. Draws the data object's defaults message at 0x809C8
 * (0x2F198(-1, 0xE, ..., 0xB000)), writes the default config fields: 0x29 from
 * the menu table, 0x35 and 0x37 = 0xA0, 0x2A's low two bits = 3, applies the
 * key-config defaults (0x1AE20, config_keys_apply_defaults, record §50-C),
 * waits 0xB4 (0x2EA78, config_screen_wait, record §49-Y) and releases the
 * message cells through the 0x809F0 copy (0x2F280). Record §F1. */
void config_set_defaults(void);

/* 0x2D638. The reader of an older image layout (a 0x51-byte record at cell 0
 * with a copy at cell 0x59, the magic at +0x4D): when either copy holds the
 * magic, the 5-byte record at cell 0x195 is read to 0x105DDC, from where
 * hiscore_storage_load's defaults pass copies it to table 2. */
void config_storage_load(void);

/* 0x2D6F8. Reads the two copies of the field buffer from the image (the
 * greater result wins and the other copy is repaired), then validates it
 * against the magic at DS_00105E30: without the magic the defaults path runs,
 * as over the zero image the captures ran on; with it the trailing-byte record
 * is read from its slot. The high-score and audit tables follow
 * (hiscore_storage_load, config_storage_validate). */
void config_validate(void);

/* Record §K2.3, 0x2D4B4: n + r + 1 for the smallest r with 2^r >= n + r + 1
 * (a SEC-DED codeword length). Its callers: 0x2D4EC, 0x2D6F8's stored-image
 * arm and 0x2DAE4. */
u32 config_codeword_len(u32 n);

/* ---- the storage image (record 2026-10-13-reverse-s §S.1, §S.5) ------------
 * 0x7F8 bytes at 0x100CE4..0x1014DB ([0x2D48C]). A record of `len` bytes is a
 * codeword of config_codeword_len(len) cells, one byte per two-byte cell: the
 * data at the positions that are neither 0 nor a power of two, position 2^j
 * the XOR of the data whose position has bit j, position 0 the XOR of all. */

/* 0x2E990. Decodes the `len`-byte record at cell `cell` into `dst` (a pointer:
 * the raw's callers pass globals and their own frames) and returns 1 when its
 * check bytes agree, -1 otherwise; the bytes are stored either way. */
s32 config_storage_read(u32 cell, u8 *dst, u32 len);

/* 0x2D498. Stores the byte `value` at `addr` when 0x100CE4 <= addr < 0x1014DC;
 * outside, the fatal exit 0x2EA68 (menu_fatal_error, which returns in the port). */
void config_image_store(u32 addr, u32 value);

/* 0x2D4EC. Encodes the record of `kind` (0..8, the table 0x2D444: 0 the config
 * fields' trailing bytes at the slot [0x105E2F], 1 and 2 the two copies of the
 * 0x53-byte field buffer, 3..5 the audit tables, 6..8 the high-score tables)
 * from its source bytes into the image. It takes EAX only: 0x2D4F5 `mov
 * edx,eax` overwrites EDX before any read (record §26 of
 * 2026-09-29-todo-verify-derivations.md). */
void config_storage_touch(u32 kind);

/* ---- high-score tables (0x2DB58/0x2DBC4/0x2DCA0; record §46-A) ----------
 * Three packed tables described by the obj-0 descriptors at 0x2D3FC and kept
 * in the data object at [0x2D478 + 8*table]: 0 = the ten scores (0x105E34),
 * 1 = the champion (0x105EAC), 2 = a 5-byte block (0x105EC8). */

/* 0x2DB58. The address of record `rec` of `table`, or 0 past the count or the
 * three tables. When non-NULL, *left_out = (count - rec) * record size (EBX)
 * and *size_out = the record size (ECX). */
u32 hiscore_locate(u32 rec, u32 table, u32 *left_out, u32 *size_out);

/* 0x2DBC4. Decodes record `rec` of `table` into DS_00105EFC (the value) and
 * DS_00105F00 (the name, NUL-terminated); returns 0x105EFC, or 0. */
u32 hiscore_read(u32 rec, u32 table);

/* 0x2DCA0. Inserts the record at `src` (u32 value, then the name) as record
 * `rec` of `table`, moving the later records down one; returns 1, or 0. */
u32 hiscore_insert(u32 rec, u32 src, u32 table);

/* 0x2DE98. The three high-score tables (kinds 6..8) at boot: each block is
 * zeroed, then read from the image when `defaults` is 0; with `defaults` set
 * table 2 takes the five bytes at 0x105DDC. A table that was not read, or
 * whose read failed, is encoded into the image again. */
void hiscore_storage_load(u32 defaults);

/* 0x2DF8C. The audit tables (kinds 3..5) at boot: each is read from the image
 * when `defaults` is 0, else zeroed; a table that was not read, or whose read
 * failed, is encoded into the image again. */
void config_storage_validate(u32 defaults);

/* 0x2DDE4 — record §49-R. Ranks `value` against `table`'s records (via
 * 0x2DB58) as an insertion index: `value` packed big-endian to the
 * descriptor's own value-byte width (the same packing hiscore_insert's
 * value store uses) is compared record by record: a strict win (the first
 * differing byte is greater, unsigned) returns the count of records
 * scanned before it; a tie or a loss advances to the next record; running
 * past the table's own byte budget, or an invalid/empty table (0x2DB58
 * returns 0), returns 0xFFFFFFFF. */
u32 hiscore_rank_probe(u32 value, u32 table);

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

/* 0x2CA78. `xor eax,eax; ret`, always 0 (0x42F60 calls it; record §48-E). */
u32 config_credit_zero(void);

/* 0x2CA7C(n). Free play -> 1; n > credits -> 0; otherwise subtract n (unless
 * DS_00104B1F suppresses it) and -> 1. */
u32 config_credit_spend(u32 n);

/* 0x2C06C. Writes the overlay's text row DS_00105C05. */
void config_set_credit_row(u8 row);

/* 0x2BF00. The init-time row write: 0x1D. One caller, 0x20CCC in 0x20C10. */
void config_set_credit_row_init(void);

/* 0x2C304 (record §46-F). DS_00105C00 = ((0x2D974(0x29) & 0xF0000) >> 16) +
 * 1. Callers: 0x10ECC (0x10E80, game_state_init) and 0x2CBB4 (svcmenu.c). */
void config_credits_init(void);

/* 0x2D62C (record 2026-10-13-reverse-s §S.9). `inc dword [0x105D88]`: the tick
 * the timer ISR 0x1BDF4 adds at 0x1BE28, 60 a second, which the run clock
 * reads. */
void config_isr_tick(void);

/* 0x32970 (record 2026-10-13-reverse-s §S.9). The run clock. EAX = the new
 * mode mask (its low byte is kept, 0x32A25). The ticks since its last call
 * (DS_00105D88 - DS_0010747C) are added to DS_00107470[k] for each set bit k
 * of the old mask byte DS_00107494, to the total DS_00107484 and to the minute
 * counter DS_00107488[old mask]. When the total reaches 0x3840 it is zeroed
 * and each of the three counters DS_00107488[0..2] above 0xE10 keeps its
 * remainder and posts its minutes through audit_add(3 + i, minutes). Returns
 * 0x107488 (0x32A2D); no caller reads it. */
u32 config_run_clock(u32 mask);

/* 0x32A3C. The play-time audit close: runs the clock (0x32A4A), takes and
 * zeroes the per-mode tick accumulator DS_0010746C[mode & 3] and, for a mode
 * other than 0, posts the game count and the seconds through audit_add
 * (0x2DAE4). Callers: 0x41578, 0x26F58, 0x277C0,
 * 0x28788 and 0x41C28 (all ported) and the dead 0x2861C region. */
void config_play_time_close(u32 mode, u32 flag);

/* 0x32B00 (record §48-Q). `arm` != 0: DS_00107478 = DS_0010746C[idx]; 0:
 * DS_00107478 = (DS_0010746C[idx] - DS_00107478) / 0x3C, posted through
 * audit_post(1, that) (0x2E934). Ported callers: 0x28DA4 (flow_player_join)
 * and 0x25EE5 (0x25C88, game_mode_05_step, record §48-U). */
void config_play_time_snap(u32 idx, u32 arm);

/* 0x32B4C (record §48-U). 0x32B00 on DS_00107480: `arm` != 0 stores
 * DS_0010746C[idx]; 0 stores (DS_0010746C[idx] - DS_00107480) / 0x3C and
 * posts it through audit_post(0, t) (0x2E934). Ported callers: 0x25EFD and
 * 0x27DEB. */
void config_play_time_snap_b(u32 idx, u32 arm);

/* ---- the audit adds (track S batch sb, record §S.6) ---------------------- */

/* 0x2DAE4. Adds `n` to config field `field` (0x2D974 then 0x2DA0C) and
 * returns the sum; a field that reads negative (an index above 0x3E reads
 * -1) is returned as read and not written. Field 3 also steps the slot byte
 * DS_00105E2F when the sum changes at or above bit 8. */
s32 audit_add(u32 field, u32 n);

/* 0x32B94. audit_add(0xE, 1) when bit 0 of `flags` (the raw's AL) is set.
 * Caller: 0x2786B (0x277C0) with the byte DS_00104B1F. */
void audit_add_if_bit0(u32 flags);

/* 0x32BB0. One audit_add(0x1B + c, 1) per character at or below 6
 * (unsigned); a first character above 6 skips the second too. Caller:
 * 0x27E28 (0x27DC8), `c1` = -1 with one player. */
void audit_characters(u32 c0, u32 c1);

/* ---- timed screen, key latch and menu helpers (record §49-Y) ------------ */

/* 0x2EA78. Builds and presents one frame without the game logic, then waits
 * n + 2 ticks (none for n == -1) while draining the BIOS key queue into the
 * latch DS_00105F30. Callers: 0x24C5C, 0x249F0, 0x2CADC, 0x1A38C, 0x31FBF, and
 * 0x2EA74 by falling through (record §K5). Wired from 0x24C5C and 0x249F0
 * (EAX = -1), through 0x2EA74, and from config_set_defaults (0x2CADC,
 * n = 0xB4, record §F1). */
void config_screen_wait(s32 n);

/* Record §K5, 0x2EA74: `xor eax,eax; mov eax,eax` with no `ret`, falling
 * into 0x2EA78: config_screen_wait(0), one frame and two tick waits. Ported
 * callers: 0x2FA40 (0x2FA61, 0x2FE2F) and 0x2FFC4 (0x2FFF1). */
void config_screen_wait_zero(void);

/* 0x2EB80. The latched key DS_00105F30, or 0; over 0x4B0 idle ticks the idle
 * timeout stores DS_00107414 = 0 and soft-restarts (game_restart_longjmp,
 * 0x65431; record named-gaps-b §B.3). */
u32 config_key_latched(void);

/* 0x2EBF0. The latched key as the game's key-bit word (arrow pairs
 * 0x80008000/0x40004000/0x20002000/0x10001000, Enter 0x1000000, Esc
 * 0x2000000) under `mask`. Callers: 0x2EDE0, 0x2EEC8. */
u32 config_key_flags(u32 mask);

/* 0x2EDE0. 0x50161(mask), OR 0x2EBF0(mask) when `flag`; a non-zero result
 * stamps DS_00105F2C. 0x2FA40 and eighteen more callers. */
u32 config_input_poll(u32 mask, u8 flag);

/* 0x2EEC8. 0x2EDE0 that also clears the latch DS_00105F30. Caller 0x2FFC4. */
u32 config_input_poll_clear(u32 mask, u8 flag);

/* 0x305FC. Draws the code-entry record DS_00107450 at (col, row); once its
 * flags byte is set, files the entered name and number in high-score table 2,
 * record 0. Callers 0x2FA40 and 0x2FFC4. */
void config_code_row(s32 col, s32 row);

/* 0x30788. Clamps `value` to 0..0xFF, draws it as a number (when label_row is
 * not negative) and a 32-cell bar on three rows from `row`. */
void config_bar_draw(s32 value, s32 row, s32 label_row);

/* 0x31E28. Draws an option row: heading string 0x17 or 0x16, the value's text
 * (0/2/4/6) and the '<' '>' arrows. `p` is the record, `flag` the arrow mode. */
void config_option_row(u32 which, u32 p, u8 flag);

/* 0x3157C. Writes the name of the key word `key` at `dest` and returns 1, or
 * returns 0 when the key has none; `raw` == 0 wraps the name as "<name>". */
u32 config_key_name(u32 key, u8 raw, u32 dest);

/* ---- key-config record (0x1AE20/0x1AE28/0x1AEE0/0x1AF64; record §50-C) ---- */
/* 0x1AE28. Applies the 0x28-byte record at `rec`: device words and scan codes
 * into the BIOS record at DS_00101514 (+0x2D4/+0x2D6, +0x2DE.., +0x2E6..) and
 * the byte mirror at 0x1014AC.. in the data object. */
void config_keys_apply(u32 rec);

/* 0x1AE20. config_keys_apply of the default record at DS 0x22C62. */
void config_keys_apply_defaults(void);

/* 0x1AEE0. Packs the 0x1014AC.. mirror into a record at `rec`. */
void config_keys_pack(u32 rec);

/* 0x1AF64. Loads the BIOS key-config (+0x2D4.. ) from the record at `rec`. */
void config_keys_load(u32 rec);

/* 0x1AFE8 — record 2026-10-13-reverse-s §S.7. Loads the stored image (0x7F8 bytes at 0x100CE4) from
 * the save file through host_save_read and checks its XOR byte. Returns 0, or 1 after filling the image
 * with 0xFF (no file, a file of another length, a bad checksum). Only AL is the raw's result. */
u32 config_file_load(void);

/* 0x1B084 — record 2026-10-13-reverse-s §S.7. Saves the stored image: when host_save_writable, runs
 * config_storage_touch(1) and (2), stores the XOR byte 0x1014DB and writes the 0x7F8 bytes through
 * host_save_write. Returns 0, or 1 when nothing was opened or the write failed. Only AL is the raw's
 * result, and no caller reads it. */
u32 config_file_save(void);

#endif /* PRAGE_GAME_CONFIG_H */
