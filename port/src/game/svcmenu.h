/* port/src/game/svcmenu.h */
#ifndef PRAGE_GAME_SVCMENU_H
#define PRAGE_GAME_SVCMENU_H

#include "../types.h"

/* The options ("service") menu behind the menu tables 0xBCBDC (MAIN MENU),
 * 0xBCC1C (OPTIONS MENU) and 0xBCCCC (START MENU): their table callbacks and
 * the screens they open (record 2026-09-29-k11-service-menu-derivations.md).
 * A table callback is `u32 cb(u32 entry)`, reached through fn_resolve from
 * menu_step/menu_run (menu.h); `entry` is the selected item's table address. */

/* Registers the table callbacks with fn_register. Idempotent; game_init calls
 * it once. */
void svcmenu_register(void);

/* MAIN MENU "Start": menu_step over the START MENU 0xBCCCC — 0x2CB74. */
u32 svc_start_menu(u32 entry);
/* MAIN MENU "GAME OPTIONS": the blocking OPTIONS MENU 0xBCC1C — 0x2CB94. */
u32 svc_options_menu(u32 entry);
/* START MENU "LEFT PLAYER ARCADE": mode 0x2D, sub-mode 0 — 0x2CBC4. */
u32 svc_start_arcade_left(u32 entry);
/* START MENU "RIGHT PLAYER ARCADE": mode 0x2E, sub-mode 0 — 0x2CBDC. */
u32 svc_start_arcade_right(u32 entry);
/* START MENU "LEFT PLAYER TRAINING": mode 0x28, sub-mode 1 — 0x2CBF4. */
u32 svc_start_training_left(u32 entry);
/* START MENU "RIGHT PLAYER TRAINING": mode 0x29, sub-mode 1 — 0x2CC0C. */
u32 svc_start_training_right(u32 entry);
/* START MENU "TUG OF WAR": mode 0x2A, sub-mode 2 — 0x2CC24. */
u32 svc_start_tug_of_war(u32 entry);
/* START MENU "ENDURANCE": mode 0x2B, sub-mode 3 — 0x2CC3C. */
u32 svc_start_endurance(u32 entry);
/* START MENU "Start 2 PLAYER HANDICAP": mode 0x2C, sub-mode 4 — 0x2CC54. */
u32 svc_start_handicap(u32 entry);
/* The screen reset every options screen opens with — 0x2F99C. */
void svc_screen_reset(void);

/* An option table is an array of 0x14-byte records ended by a zero +0: +0 the
 * label string id, +4 the value's bit shift, +8 the value count, +0xC a byte
 * (draw the value's 1-based index), +0x10 the value list of 8-byte
 * {char *text, u32 string id} entries (record §K11.3). */

/* Draws (release = 0) or releases (release = 1) the option rows from record
 * `first` on rows 3, 6, .. 0x15; 0 for a negative `first`, an empty record or
 * a row that returns 0, else the last row's result — 0x2CC74. */
s32 svc_option_rows(u32 table, u32 bits, s32 first, u32 mode, u32 release);
/* One option row: the label at (4, row), the value's index, text and string
 * on row + 1 from column 5. Returns the next record, or 0 when the record is
 * empty or the value is out of range — 0x2CD30. */
u32 svc_option_row(u32 opt, u32 bits, s32 row, u32 mode, u32 release);
/* The blocking editor over a table's bit-packed values: Up/Down pick a row,
 * Left/Right cycle its value, a `mask` key restores the entry bits. Enter
 * returns the bits; Esc returns -1 when `esc_cancels`, else the bits
 * — 0x2CF00. */
u32 svc_option_edit(u32 table, u32 bits, u32 mask, u8 esc_cancels);
/* OPTIONS MENU "CONFIG OPTIONS": the table at [[DS_0010740C] + 4] — 0x2CACC. */
u32 svc_config_options_entry(u32 entry);
/* The CONFIG OPTIONS screen: edits config field 0x29 through `table`
 * — 0x33578. */
u32 svc_config_options(u32 table);
/* OPTIONS MENU "SOUND TEST": plays each picked sample — 0x30EB4. */
u32 svc_sound_test(u32 entry);
/* OPTIONS MENU "MUSIC TEST": plays each picked tune — 0x30F54. */
u32 svc_music_test(u32 entry);
/* Stops the voices, then plays tune `i` — 0x2C9CC. */
u32 svc_play_tune(u32 i);
/* Stops the voices, then plays sample `i` — 0x2C9E8. */
u32 svc_play_sample(u32 i);

/* "%i" of `value` fitted to `width` by `pad` (text_number_format), drawn at
 * the text cursor DS_00105F34 in `mode`; the cursor moves past it
 * — 0x2F464. */
void text_number_cont(s32 value, s32 width, u32 pad, u32 mode);
/* "ERROR SETTING VOLUME LEVEL" on row 6 until Esc, then released
 * — 0x30728. */
void svc_volume_error(void);
/* OPTIONS MENU "ADJUST VOLUME": the music, effects and voice levels in
 * config fields 0x35, 0x37 and 0x2A bits 0..1; returns 0 — 0x30864. */
u32 svc_adjust_volume(u32 entry);
/* One handicap row: `value` clamped to 0x32..0x96, its number on row + 4 and
 * a 21-cell bar on rows `row`..`row` + 2 — 0x30FE8. */
void svc_handicap_row(u32 value, s32 row);
/* OPTIONS MENU "2 PLAYER HANDICAP": edits the key-config record's +0x24/+0x26
 * and stores them in DS_00107468/DS_0010746C on Esc — 0x31138. */
u32 svc_handicap(u32 entry);

/* `value` in hex (the digits at 0x2EF10), right-aligned in `width` cells of
 * `buf` with a NUL at buf[width]; high digits that do not fit are dropped and
 * the cells left of the digits are ' ' when `space_pad` is non-zero, else
 * '0'. Returns the digit count — 0x2EF48. */
s32 text_hex_format(u32 value, u8 *buf, s32 width, u32 space_pad);
/* text_hex_format into a 0x14-byte buffer drawn at (col, row) in `mode`
 * (`pad` and `mode` are the two stack arguments) — 0x2F48C. */
void text_hex_set(s32 col, s32 row, u32 value, s32 width, u32 pad, u32 mode);
/* A 3x3 stick indicator of '+' (lit) and '.' cells two apart around (col,
 * row); the direction bits of `bits` pick the lit cell — 0x314A0. */
void svc_stick_draw(s32 col, s32 row, u32 bits);
/* Releases the four button-name rows around (col, row) (string 0x22C from
 * col - 8 and col + 2 on rows row + 8 and row + 0xC) — 0x319B0. */
void svc_buttons_clear(s32 col, s32 row);
/* Draws the four button keys of the key-config record `rec` (words +0xA.. for
 * side 0, +0x1C.. for side 1) as "<name>" centred on col - 5 / col + 5, rows
 * row + 8 / row + 0xC — 0x31A78. */
void svc_buttons_draw(u32 side, u32 rec, s32 col, s32 row);
/* Releases the four direction-name places of side 0 (centre column 0xA) or 1
 * (0x1E) — 0x31B94. */
void svc_dirs_clear(u32 side);
/* Draws the four direction keys of `rec` (words +2.. for side 0, +0x14.. for
 * side 1) around (0xA or 0x1E, 0xB): up and down wrapped across rows 8 and
 * 0xE, left and right unwrapped down columns c - 3 and c + 3 — 0x31C78. */
void svc_dirs_draw(u32 side, u32 rec);
/* OPTIONS MENU "MODIFY CONTROLS": Left/Right pick a player, Up/Down cycle
 * its device (0 KEYBOARD, 2 4 BUTTON JOYSTICK, 4 2 BUTTON JOYSTICK, 6
 * KEYBOARD/JOYSTICK); Esc applies the key-config record; returns 0
 * — 0x31F24. */
u32 svc_modify_controls(u32 entry);
/* OPTIONS MENU "TEST CONTROLS": shows the live pad sticks and buttons (and,
 * with DS_00107410 bit 4, the diagnostic rows) until the latched Esc; returns
 * 0 — 0x32358. */
u32 svc_test_controls(u32 entry);

#endif /* PRAGE_GAME_SVCMENU_H */
