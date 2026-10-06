/* port/src/game/actors.h */
#ifndef PRAGE_GAME_ACTORS_H
#define PRAGE_GAME_ACTORS_H

#include "../types.h"

#define ACTOR_REC_SIZE 0x68u
#define ACTOR_POOL_RECORDS 580u
#define PSET_SIZE 0x20u

/* Validates the two pools res_load_index already allocated. Returns 0 when
 * either base is zero or outside mem[]. */
int  actors_init(void);
/* 0x2BAF4 with EAX = 1 (the 34 callers other than 0x257A4). */
void actors_reset(void);
/* 0x2BAF4 with AL = al: non-zero runs 0x52106/0x336C0, zero copies the
 * 0xFA00-byte buffer [0xE87A0] into [0xE87A4] instead (record §47-C). */
void actors_reset_al(u32 al);
/* 0x38B70. Zeroes the actor cursors and the two 7-dword arrays DS_00107A00 and
 * the front-end row table DS_00107A1C; actors_reset calls it (0x2BBDA). */
void actor_cursor_reset(void);
/* Pops the free-list head and links it into the active list. `flag` is the
 * original's EAX at the 0x2AC80 call (copied to ECX by 0x2AC84); its 0x400 bit
 * selects the tail insert (0x249C0) over the head insert (0x249B0). 0x2AE14
 * supplies the low 16 bits of its arg 5. */
u32  actor_alloc(u32 flag);

/* 0x2A820 / 0x2A620 (C2b): 0x2AE14's pset writer and mode-1 cursor, exported
 * for its differential row's mutants. */
void pset_write(u32 rec, u32 pset);

/* 0x2B8F8 (C2b): the operand fetch 0x2B2A0 calls, exported for its row's
 * mutants (0x29DB8 is already declared above the actors_init section). */
u32 anim_operand(u32 rec);
void mode1_cursor(u32 rec, u32 pset);
/* 0x2AE14. `desc` points at a descriptor in mem[]; the four register arguments
 * are a2=EDX, a3=ECX, a4=EBX and a5=the stack word, pinned by disassembly in
 * docs/superpowers/plans/2026-09-17-actor-system-args.md. Returns the record's
 * mem[] offset, or 0 on pool exhaustion. */
u32  actor_spawn(const u32 *desc, u32 a2, u32 a3, u32 a4, u32 a5);
void actor_free(u32 rec);
u32  actor_record(u32 index);
u32  actor_index(u32 rec);
u32  actor_list_head(void);
u32  actor_next(u32 rec);
u32  actor_pset(u32 rec);
/* 0x2A690. The free-record pset point writer: pset+0x10/0x14 take pset+4/8,
 * pset+4/8 take the record's world x/y, the layer at pset+0x0E and rec+0x3C are
 * written. 0x2A820's free-record arm calls the same body; the game_frame tail
 * (0x25443) calls it per live fighter. */
void actor_pset_point(u32 rec);
/* 0x2A1FC. Per-record sync: the frame timer, the animation-id hold and motion,
 * then the pset write. 0x2A31C calls it for each active record; 0x35658 calls it
 * on the fighter at 0x35813. Exposed for fight_hud_pass. */
void actor_sync(u32 rec);
/* 0x2A31C. Walks the active list and syncs each record (0x2A1FC). */
void actors_update(void);
/* 0x2A17C. Point a record's pset word +2 at `word` OR the 0x800 sprite bit when
 * the record's +0x5F is non-zero, and set the pset's palette entry at +0x18 to
 * `handle`'s acquired entry: an existing entry is released first (0x33864). A
 * zero handle returns at 0x2A1AC leaving +0x18 unchanged (the 0x2A1F5 store is
 * dead). 0x29BC8 (the fighter's character palette) passes word 0. */
void actor_pset_palette(u32 rec, u32 word, u32 handle);
/* 0x2A148. Store `flag` in rec+0x5F and set the pset's +2 word to rec+0x2E
 * with the 0x800 sprite bit while that byte is non-zero. */
void actor_pset_flag_5f(u32 rec, u8 flag);
/* 0x2B150. Mark `rec` dead (rec+0x28 |= 8), release its pset palette and unlink
 * the pset from the render list. Exported for its own C3b row; the port's other
 * callers go through actor_set_dead below. */
void set_dead(u32 rec);
/* 0x2B150. The wrapper the port's non-actor modules call: 0x121A0's phase 1
 * uses it on the logo and the second object when DS_000F0A66 <= 0x10. */
void actor_set_dead(u32 rec);
/* 0x49444 (record §49-P). Types 0x20..0x25's teardown: clear the 0x10839C
 * entry named by the node's 16.16 +0x18 when its +0x1C bit 1 is set, retire
 * the node's +0x10 child, then return the node to 0x1083C4. Exported for
 * fight_4e67c's direct call (0x4E8EE, not a stored callback). */
void actor_type_49444(u32 rec);
/* 0x10D70. Clear rec+0x28 bit 2 and write `word` to the record's pset +0, its
 * bit 15 taken from rec+0x28 bit 14. The wipe steps 0x4F9E4/0x4FA88 call it
 * (record §43-B), and so do 0x1D2F0/0x1D464 (fight.c, record §48-U) and
 * 0x1DA84 (fight.c, 0x1DAE0, record §49-M). */
void actor_pset_word_set(u32 rec, u32 word);
/* 0x2BE5C. The mode-1 pset/position updater the type-0x19/0x0A cb1 tails call
 * (0x28FB5/0x2906D): rec+0x1C from the pset y and the 16.16 vertical position,
 * then the bit-12 mode-1 arm (pset+0x14, 0x2A620, the 0x107900 ramp entry,
 * rec+0x18 from pset+4 and the 16.16 rec+0x44) or the DS_000F0AF0 form, and
 * clears rec+0x29 bit 5. Exposed for its unit test. */
void actor_mode1_pset(u32 rec);
/* 0x28E98. Self-link the type-0x0A/0x19 sentinels 0x104880 (in use) and
 * 0x104888 (free) and append the sixteen nodes 0x104780..0x104870 to the free
 * list in address order. State 6's reset 0x20DF4 calls it at 0x20E3D; without
 * it 0x28F64/0x2901C find the free list empty and refuse the spawn. */
void actor_type_0a19_list_init(void);
/* 0x48C4C (record §42-B). Self-link the type-0x2D sentinels 0x1082E0 (free)
 * and 0x108368 (in use), append the eight nodes 0x1082E8..0x108358 to the
 * free list in address order and clear the bytes 0x108396..0x108398 (the
 * last is the type-0x2D count). Its one caller is 0x48D94's +0x57 == 1 arm. */
void actor_type_2d_list_init(void);

/* 0x33754. Acquire a reference to palette resource `handle` in the table at
 * DS_00107618 and enqueue its DAC range via palette_record (0x33734). Returns
 * the entry's mem[] offset (stored in pset+0x18), or 0 on a full table.
 * Exposed for the attract phase-2 palette registration (0x11000). */
u32 palette_acquire(u32 handle);

/* 0x33874. Re-point the palette-table entry `descriptor` at `handle` (the
 * attract drivers pass DS_000F0A48 and DS_000C98A0[counter]) and reflow the
 * entries after it. When the entry's len is short of the resolved resource's
 * colour count, the table is walked from descriptor+0x10 and each overlapping
 * entry's start is moved to the previous end and re-enqueued; otherwise the
 * entry is re-pointed and its range re-enqueued. Sibling of palette_acquire
 * (0x33754) and palette_release (0x33864). */
void palette_reflow(u32 descriptor, u32 handle);

/* ---- animation-stream interpreter -------------------------------------- */

/* 0x2A408. Read the record's next sprite id from its animation stream. The
 * second argument is the record's pset (pinned by disassembly: every caller
 * passes the pset at pset+0 = DS_001014EC + slot*0x20, read for the
 * `word & 0x8000` keep-current-id case). Literal words (bit 0x8000 clear) are
 * returned as-is; a 0xD00 computed word reads a variable and either adds the
 * following word or uses it to index a table. The returned bit 0x8000 is the
 * stream's bit XOR the record's `rec+0x28 >> 8 & 0x40` flip. */
u32  anim_next_sprite_id(u32 rec, u32 pset);
u32  spawn_anim_opcode(u32 rec, u32 index, u32 flag);
void palette_release(u32 entry);
/* 0x29F34. Read an animation variable: `op & 0x7F` selects the 0x40-word ring
 * at DS_00105B4C (< 0x40), the record's own bytes (0x40..0x45), the parent
 * rec+0x4A's bytes (0x46..0x4B) or the child rec+0x4B's bytes (0x4C..0x51). */
u32  anim_read_var(u32 rec, u32 op);
/* 0x29DB8. Write an animation variable (the mirror of anim_read_var). */
void anim_write_var(u32 rec, u8 op, u32 value);
/* 0x2BC30. Point an existing record at `stream`, reset its animation cursor and
 * cache, pre-walk the commands, then load the first sprite id. `frame_bits` is
 * the original's third stack argument stored verbatim into rec+0x24/rec+0x20;
 * the callers pass IEEE-754 float bit patterns. */
void actors_anim_begin(u32 rec, u32 stream, u32 frame_bits);
/* 0x2BCF4. Point a record at `stream` and load its first sprite id. */
void actors_anim_seek(u32 rec, u32 stream);
void release_record(u32 rec, u32 pset);
/* 0x2BD20. Store `v`'s low byte at +0x4B of the pool record `rec`'s +0x4A byte
 * names (the holder's link to the held record); returns 0. */
u32 actors_link_held(u32 rec, u32 v);
/* TEST-ONLY. Task 1's fourth pin replaced the opcode-8 call to 0x5D7DC with
 * `mov eax, 0`; the port draws `on ? 0 : rng_next(range)` at that one call site.
 * Set by the Task 10 title driver; nothing else calls it. */
void actors_pin_anim_tick_zero(int on);

/* ---- text cursor and record grid ---------------------------------------- */

/* PORT: plan Format reference H calls the four functions below "pset layer
 * select and support". The shipped machine is a display-string / text-cursor /
 * actor-record-grid group, not a pset layer writer; the pset layer at
 * pset+0x0E comes only from the sync (rec+0x59 alone in 0x2A690, rec+0x49 +
 * rec+0x59 in 0x2A820). See the group comment in actors.c. */
/* 0x2F0F0. EAX = byte string, EDX = mode. Mode & 3 in {0,1} returns strlen;
 * modes 2/3 sum a per-character class weight from the data-object tables at
 * DS 0x3D048 / 0x3D1EC / 0x3D38D. */
int  text_width(const u8 *s, u32 mode);
/* 0x2F198. EAX = col (-1 centers), EDX = row (-1 reuses the cursor), EBX =
 * string, ECX = mode. Writes the two-word cursor at DS_00105F34. */
void text_cursor_set(s32 col, s32 row, const u8 *s, u32 mode);
/* 0x2F41C (record §48-Z). 0x2F198 with col = 0, row = -1 (the cursor's row). */
void text_cursor_next_line(const u8 *s, u32 mode);
/* 0x2F280. Same register shape as 0x2F198: clears `text_width` consecutive
 * cells of the actor-record grid at DS_00105F38 and releases each record. */
void text_cells_release(s32 col, s32 row, const u8 *s, u32 mode);
/* 0x2AD40 (record §48-S): release `rec` (EDX = its pset): unlink it to the
 * free list, zero the pset's +4/+8/+0xC/+0xE and mark it dead. */
void actor_release(u32 rec, u32 pset);
/* 0x2F388 (record §48-S). 0x2F280 with the cell count `count` given (EBX)
 * rather than measured from a string. */
void text_cells_release_count(s32 col, s32 row, s32 count);
/* 0x2F4BC. 0x2F198 with the cursor saved and restored afterwards. */
void text_cursor_hold(s32 col, s32 row, const u8 *s, u32 mode);
/* 0x2F510. 0x2F4BC with the mode ORed with 2 (the class font). */
void text_cursor_hold_font2(s32 col, s32 row, const u8 *s, u32 mode);
/* 0x2F20C. 0x2F198's vertical twin (0x2F830 with the stack byte 1); the cursor
 * gets {row, col + glyph count}. */
void text_vertical_set(s32 col, s32 row, const u8 *s, u32 mode);
/* 0x2F314. EAX = col, EDX = row, EBX = string: releases strlen(s) cells going
 * down one row per cell, stopping after a row above 0x1E. */
void text_cells_release_vertical(s32 col, s32 row, const u8 *s);
/* 0x2EFD4 (with 0x2EF24). "%i" of `value` into `dest`, fitted to `width` by
 * `pad` (0 '0'-left, 1 ' '-left, 2 ' '-right, 3 none); returns the digit
 * count. */
s32 text_number_format(s32 value, u8 *dest, s32 width, u32 pad);
/* 0x2F4D0. EAX = col, EDX = row, EBX = value, ECX = width, stack pad and mode:
 * 0x2EFD4 then 0x2F198 with the cursor saved and restored. */
void text_number_draw(s32 col, s32 row, s32 value, s32 width, u32 pad, u32 mode);
/* 0x2F434 (record §48-C). 0x2F4D0 without the cursor save: 0x2EFD4 then
 * 0x2F198, so the cursor moves. */
void text_number_set(s32 col, s32 row, s32 value, s32 width, u32 pad, u32 mode);
/* 0x2F528. 0x2F4D0 with the mode ORed with 2 (the 0xBD048 class font). */
void text_number_draw_font2(s32 col, s32 row, s32 value, s32 width, u32 pad,
                            u32 mode);
/* 0x2F830. EAX = string, EDX = mode, ECX = row, EBX = col, and a stack byte
 * `vertical` (0x2F198 passes 0; 0x2F20C passes 1). All-spaces clears the run
 * through 0x2F280; otherwise it lays each character out through 0x2F5A0 and
 * returns the number of glyph cells emitted (0 on empty/all-space or a glyph
 * abort). It truncates the caller's string at the line limit. */
s32 text_render(const u8 *s, u32 mode, s32 row, s32 col, u32 vertical);
/* 0x2F5A0. EAX = character, EDX = &col, EBX = &row, ECX = mode, and the same
 * stack byte `vertical`. Releases the addressed cell's record (0x2AD40), spawns
 * a non-space glyph as an actor (0x2AE14) and advances *col (vertical 0) or
 * *row (vertical 1) by the glyph's width. Returns 1 when a negative class or a
 * full pool aborts the string, 0 otherwise. */
u8 text_glyph_emit(s32 ch, s32 *col, s32 *row, u32 mode, u32 vertical);
/* 0x2F174 (record §48-Z). 0x2F5A0 at a fixed (col, row); the glyph advance is
 * discarded. */
void text_glyph_at(s32 col, s32 ch, s32 row, u32 mode);
/* 0x1C65C. EAX = the string, EDX = the x seed, EBX = the y seed (12-bit fixed
 * point, truncating /0x1000 after +0x800). Blits each glyph directly through
 * 0x1C5E8 (the font table 0xBCD7C, no glyph actor) and flushes the palette
 * dirty list. The resource loader calls it for string 489 at (0,192). */
void text_blit_string(const u8 *s, s32 x, s32 y);

/* 0x37B54 (demo-pose record §46-D): the other side's slot record's +0x53 = 1.
 * Exported for 0x45D98 (record §K8c.6). EAX = rec. */
void fighter_37b54(u32 rec);

#endif /* PRAGE_GAME_ACTORS_H */
