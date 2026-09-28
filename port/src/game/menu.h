/* port/src/game/menu.h */
#ifndef PRAGE_GAME_MENU_H
#define PRAGE_GAME_MENU_H

#include "../types.h"

/* The text menu cluster 0x2EB80..0x30000 (record §49-X): the two menu drivers
 * 0x2FA40 (blocking) and 0x2FFC4 (one step per frame) and their helpers. A menu
 * is a table of 0x10-byte entries at `table` (stride `stride`): entry 0 is the
 * title (+0 string id, +4 second string id, +8 the menu-level callback), entries
 * 1.. are the items (+0 string id, 0 ends the list; +4 second string id; +8 the
 * item's callback; +0xC a signed row offset). A string starting with '?' hides
 * its item. Menu state lives in mem[] at DS_00107414..DS_0010744C (0x2FFC4) and
 * the debug widget at DS_00107450 (0x305FC).
 *
 * Callbacks are code addresses stored in the table: they run through fn_resolve
 * as `u32 cb(u32 entry)` and are skipped (result 0) when the address was never
 * registered. The stock tables at 0xBCBDC/0xBCC1C/0xBCCCC point at 0x2CB74,
 * 0x2CB94, 0x2CACC and 0x2CAC0, which are not ported (see record §49-X.8). */

/* 0x2EA68. The fatal-error exit `mov eax,1; jmp 0x62003` with the message
 * `msg` (0x80B54 "Null Menu" in the menu code). PORT: 0x62003 is the runtime's
 * error exit (spec §7); the port records nothing and returns. */
void menu_fatal_error(u32 msg);

/* 0x2EB80. The pending key (DS_00105F30) when nonzero, else 0 while the idle
 * clock DS_00101500 - DS_00105F2C is within 0x4B0 ticks. PORT: past 0x4B0 the
 * raw clears DS_00107414 and longjmps (0x65431, spec §7); the port clears
 * DS_00107414 and returns 0. */
u32 menu_key_or_timeout(void);

/* 0x2EBF0. EAX = `mask`. Decodes the pending key into direction/Enter/Esc bits:
 * 0x80008000 up, 0x40004000 down, 0x20002000 left, 0x10001000 right, 0x1000000
 * Enter (key 0xD), 0x2000000 Esc (key 0x1B). */
u32 menu_key_decode(u32 mask);

/* 0x2EDE0. EAX = `mask`, DL = `keys`. The pad bits 0x50161 reports for `mask`,
 * ORed with the keyboard bits of 0x2EBF0 when `keys` is nonzero; a nonzero
 * result stamps the idle clock DS_00105F2C. */
u32 menu_input_poll(u32 mask, u32 keys);

/* 0x2EEC8. 0x2EDE0's twin that first clears the pending key DS_00105F30. */
u32 menu_input_poll_clear(u32 mask, u32 keys);

/* 0x2FE40. The address of entry `idx` of the table at `base`, or 0 when
 * `idx` is negative, the list ends before it, or its string starts with '?'. */
u32 menu_entry_find(u32 base, u32 stride, s32 idx);

/* 0x2FE84. Redraws the menu title screen for the entry at `entry`: resets the
 * actors, spawns the backdrop row and centres the entry's two strings on row 0
 * (`mode_a`) with the two instruction lines 0x209/0x20A on rows 0x1B/0x1C
 * (`mode_b`) unless flags bit 2 is set; flags bit 0 adds the 0x2F940 lines. */
void menu_title_draw(u32 entry, u32 mode_a, u32 mode_b, u32 flags);

/* 0x2F940. The two debug lines at rows `row` and `row + 1`. */
void menu_debug_lines(u32 row);

/* 0x305FC. The debug entry widget at DS_00107450 drawn at (`col`, `row`); once
 * its byte DS_00107453 is set it also files the typed value as high-score
 * record 0 of table 2. */
void menu_debug_widget(s32 col, s32 row);

/* 0x2FA40. The blocking menu: EAX = `table`, EDX = `stride`, ECX = `flags`
 * (bit 0 debug overlay, bit 2 Esc acts as down and skips the instruction
 * lines). Returns the menu callback's nonzero result, or on Esc (flags bit 2
 * clear) -1. PORT: the raw busy-polls the pad and keyboard until one of those
 * happens; in the port it returns only when the input state makes it. */
u32 menu_run(u32 table, u32 stride, u32 flags);

/* 0x2FFC4. The per-frame menu: same table shape as 0x2FA40 with its state in
 * DS_00107414..DS_0010744C. Returns 0 while the menu stays open; -1/-5/-10
 * (0xFFFFFFFF/0xFFFFFFFB/0xFFFFFFF6) leave it, or a callback's nonzero
 * result. */
u32 menu_step(u32 table, u32 stride, u32 flags);

#endif /* PRAGE_GAME_MENU_H */
