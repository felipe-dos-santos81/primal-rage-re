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

#endif /* PRAGE_GAME_SVCMENU_H */
