/* FM patch bank: turns the shipped FAT.OPL bytes into the instrument state the
 * sequencer applies on a program change.
 *
 * Layout (verified against the bytes and against the captured OPL trace; full
 * detail in FORMATS.md):
 *
 *   table:   repeated { u16 key (LE); u32 payload offset (LE) } stepping 6,
 *            terminated by key == 0xFFFF. key = (bank << 8) | program; bank 0
 *            is melodic (programs 0..0x7F), bank 0x7F is percussion (keyed by
 *            MIDI note, 0x23..0x57 in the shipped bank).
 *   payload: 14 bytes at the offset:
 *              [0]=0x0E  [1]=0x00  [2]=key/percussion base
 *              [3..7]  = modulator   0x20,0x40,0x60,0x80,0xE0
 *              [8]     = 0xC0 feedback/connection
 *              [9..13] = carrier     0x20,0x40,0x60,0x80,0xE0
 *
 * `patches_load` does no I/O: the caller resolves the FAT.OPL bytes through the
 * file/resource layer (the game reads it directly from the install directory,
 * port/spec/audio.md "AIL surface" row 25) and passes them in, exactly as
 * samples_load does.
 */
#ifndef PR_PATCHES_H
#define PR_PATCHES_H

#include "types.h"

#define PATCH_BYTES 14
#define PATCH_BANK_MELODIC 0x00u
#define PATCH_BANK_PERCUSSION 0x7fu
#define PATCH_KEY(bank, program) ((u16)(((u16)(bank) << 8) | (u8)(program)))

/* Decodes the bank at `data` (length `len`). Returns 1 on a well-formed bank,
 * 0 otherwise; on 0 the previously loaded bank is left unchanged. Reads are
 * bounded by `len`: a record or payload that runs past the buffer is rejected
 * rather than followed. */
int patches_load(const u8 *data, u32 len);

/* Entries in the loaded bank (0 before the first successful load). */
int patches_count(void);

/* The 14-byte payload for `key`, or NULL when the key is absent or no bank is
 * loaded. The pointer is owned by the loader and stays valid until the next
 * patches_load (this bank is copied in, so the caller need not keep `data`). */
const u8 *patches_lookup(u16 key);

#endif /* PR_PATCHES_H */
