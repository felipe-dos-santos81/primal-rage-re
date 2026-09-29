/* port/src/game/nameentry.h */
#ifndef PRAGE_GAME_NAMEENTRY_H
#define PRAGE_GAME_NAMEENTRY_H

#include "../types.h"

/* The high-score name-entry screen (record §49-T). Its state lives in mem[] at
 * the original addresses: the letter-cell records DS_00104114 (18 x 0x14), the
 * name buffer DS_00104343, the cursor words DS_001044D0 (column) and
 * DS_001044D2 (row), and the cursor/backdrop actors DS_001044B0/B4/B8. The
 * writers of the geometry words (DS_001044C4/CC/D6, DS_0010431A/C/D) are
 * nameentry_arm (0x204F4, record §51-A) and 0x1EC38 (flow.c, record §49-R); the
 * keyboard-queue writer (DS_00104458, DS_001044BC, DS_001044D8) is
 * nameentry_key (0x20860, record §53-A), fed by game_frame's int 16h loop. */

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

/* 0x204F4. EAX = `rank` (0..9, low word), EDX = `score`; called by 0x1EC38 for a
 * qualifying score. Reads table 0's records into a frame-local table the raw
 * never reads back, fills the blank candidate name DS_00104367 with factory
 * record `rank`'s name (0xA7BC0 + rank * 0x2C) and raises DS_0010431E, clears
 * the name buffers DS_0010431F/DS_00104343, blanks DS_00104394, stores the
 * score at DS_00104390, and sets the timer (0x2EE) and the entry geometry: rank
 * 0 allows 18 letters at column 2, any other rank 3 letters at column 0x12,
 * both on row 0x16. */
void nameentry_arm(u32 rank, u32 score);

/* 0x20710. Finalises the entry for `side` (0/1): filters the name, stores it in
 * the score records (0x2DCA0) and redraws the rank line, then retires the three
 * actors DS_001044B8/B4/B0. */
void nameentry_finish(u8 side);

/* 0x1F458. The per-frame name-entry driver for `side`: ticks the cells, counts
 * the timeout down, moves the cursor, takes a letter and (once the entry is
 * finished) finalises it. Returns 0 when finished, 1 while still running. */
u32 nameentry_step(u8 side);

/* 0x20860. EAX = `c`, a key's ascii byte (the caller 0x24C5C passes only a
 * non-zero one, in mode 0x1E). Queues its letter code for 0x1F458: backspace
 * -> DEL 0x1B, Enter -> END 0x1C, space -> 0x1A, a letter (either case) -> 0..0x19;
 * any other byte is dropped. A full queue drops the key too. Unless the byte was
 * dropped for its class, sets DS_001044D8, which turns the pad cursor off. */
void nameentry_key(u32 c);

#endif
