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

#endif /* PRAGE_GAME_SVCMENU_H */
