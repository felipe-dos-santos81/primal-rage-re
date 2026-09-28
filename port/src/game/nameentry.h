/* port/src/game/nameentry.h */
#ifndef PRAGE_GAME_NAMEENTRY_H
#define PRAGE_GAME_NAMEENTRY_H

#include "../types.h"

/* The high-score name-entry screen (record §49-T). Its state lives in mem[] at
 * the original addresses: the letter-cell records DS_00104114 (18 x 0x14), the
 * name buffer DS_00104343, the cursor words DS_001044D0 (column) and
 * DS_001044D2 (row), and the cursor/backdrop actors DS_001044B0/B4/B8. The
 * writers of the geometry words (DS_001044C4/CC/D6, DS_0010431A/C/D) belong to
 * the unported 0x204F4 and 0x1EC38 (record §49-R), and the keyboard-queue writer
 * DS_001044BC to the unported 0x20860; see §49-T.7 for the named gaps. */

/* 0x13F68. EAX = the bad-word `word`, EDX = the name string `str`. A fuzzy
 * search of `str` for `word` (repeated letters collapse, spaces are skipped, up
 * to DS_000FD0F0 unmatched letters may intervene): a match is overwritten with
 * '*' and the search resumes after it. Returns 1 when a match was blanked, 0
 * otherwise. */
u32 name_filter_word(u32 word, u32 str);

/* 0x13EF0. EAX = the NUL-terminated string at `str`. Runs 0x13F68 over every
 * word of the pointer table at 0x9AF40 (until the entry naming an empty string),
 * then strips the leading spaces in place. Returns the accumulated match flag
 * sign-extended from a byte (the raw's `sar eax,0x18` of the stack byte). */
u32 name_filter(u32 str);

/* 0x1ED2C. The name-entry screen reset: the cursor to column 0xB / row 6, the
 * actors cleared, four backdrop rows (0xA7B80) and the three cursor/marker
 * actors DS_001044B8/B4/B0 spawned, the 18 letter cells cleared and the queue
 * and input-disable words zeroed. */
void nameentry_reset(void);

/* 0x1FFD0. One tick of the 18 letter cells at DS_00104114: each non-zero state
 * (1..9) advances the flying-letter animation and the last (8) commits the
 * letter to the name buffer and the screen. Sets DS_001044AC to 1 when any cell
 * is live. */
void nameentry_cells_step(void);

/* 0x20710. Finalises the entry for `side` (0/1): filters the name, stores it in
 * the score records (0x2DCA0) and redraws the rank line, then retires the three
 * actors DS_001044B8/B4/B0. */
void nameentry_finish(u8 side);

/* 0x1F458. The per-frame name-entry driver for `side`: ticks the cells, counts
 * the timeout down, moves the cursor, takes a letter and (once the entry is
 * finished) finalises it. Returns 0 when finished, 1 while still running. */
u32 nameentry_step(u8 side);

#endif
