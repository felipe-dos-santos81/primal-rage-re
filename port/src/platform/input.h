/* int 16h keyboard service. The original calls the BIOS with swi(0x16): AH=1
 * to peek, then AH=0 to read (FUN_00024c5c, FUN_000249f0). The port keeps the
 * same BIOS-style FIFO and lets the host feed it through input_push().
 * PORT: the ring is C state in input.c, not an offset in mem[]. int 16h is a
 * BIOS service, so its buffer belongs to the BIOS/host, not the game image:
 * the port has no real-mode segment 0x40, and no byte of the shipped code or
 * data object describes this queue. The player's keystrokes are transient host
 * input, not the game's saved state the mem[] rule protects. host.c (Task 12)
 * is the only producer and shares this file's state directly. */
#ifndef PR_INPUT_H
#define PR_INPUT_H

#include "types.h"

/* BIOS-style ring capacity, in keys. */
#define INPUT_QUEUE_CAP 64

/* Enqueues one key: `scan` is the scan code, `ascii` the ASCII byte (0 for an
 * extended key). When full, the oldest entry is dropped. */
void input_push(u8 scan, u8 ascii);

/* Non-consuming presence test: 1 when a key is queued, else 0. The movie
 * player reads it as 0x62756, the WATCOM runtime's kbhit (int 21h AH=0Bh on
 * the BIOS buffer; record named-gaps-f §F.2). */
int input_has_key(void);

/* int 16h AH=0: blocks (draining host_pump()) until a key is queued, then
 * dequeues and returns it packed as (scan << 8) | ascii. PORT: returns 0
 * without a key once the host has asked to quit (a window close). */
u16 input_get_key(void);

/* int 16h AH=1: peeks without dequeuing. Returns the packed key, or 0 when the
 * queue is empty (0x0000 is never a real BIOS key). */
int input_check_key(void);

/* Discards every queued key. */
void input_clear(void);

/* int 16h packs a key as (scan << 8) | ascii; ESC is scan 0x01 / ascii 0x1B,
 * the key 0x249F0 turns into the quit flag DAT_000A81A8. */
#define INPUT_ESC 0x011Bu

/* ---- game input bitfield (0x500C4 / 0x50161 / 0x4F644) ------------------
 * The original keeps a debounced key level in DAT_000E1C34, a hold latch in
 * DAT_000E1C38 and a repeat mask in DAT_000E1C3C, sampled from the key bitmap
 * at DAT_00101514 + 0x2d8/0x2d9, and turns it into the two masks the game
 * reads: DS_001088E4 (newly pressed) and DS_001088D8 (held). The host fills the
 * bitmap; everything below is the raw's arithmetic. */

/* 0x500C4. Samples the host key bitmap, applies the raw's one-frame debounce
 * and repeat-timer logic, and returns the debounced level word. */
u32 input_pump(void);

/* 0x50161. Reports `level & ~mask` OR'd with the bits of `mask` newly set
 * since the last call, latching them into DAT_000E1C38 as it goes. */
u32 input_select_bits(u32 mask);

/* 0x50146 (record §49-X). Sets the repeat mask DS_000E1C3C to `mask` and the
 * repeat timer words: DS_000E1C42 (the reload value) and DS_000E1C40 (the live
 * counter) to `first`, DS_000E1C44 (the reload after the first repeat) to
 * `next`. */
void input_repeat_set(u32 mask, u32 first, u32 next);

/* 0x2D2F0. The joystick accessor: a constant 0 in the shipped profile
 * (`xor eax,eax; ret`), so the joystick half of 0x4F644 is inert. */
u32 input_joystick_device(u32 selector);

/* 0x4F644. Builds DS_001088E4 (newly pressed) and DS_001088D8 (held) from the
 * level word and the two 0x50161 mask families. */
void input_state_update(void);

/* ---- the keyboard readers (record 2026-10-21-host-input-truth §H2) --------
 * The six functions the ISR sampler 0x1BBAC's device-0 arms call. Each reads
 * the key-config block at DAT_00101514: the 16 scan slots +0x2DE..+0x2ED by
 * position and the key-state table at +0x254 indexed by those scans. The table
 * is the real-mode driver's (RM.DRV:0x31B stores the raw scan byte at
 * [base+0x254+(scan&0x7F)]: bit 7 clear on make, set on break), so a reader
 * sets its game bit when the entry's bit 7 is CLEAR. The host fills the table
 * from SDL in host_input_fill_keyboard(). */

/* 0x1B610. P1's directions: slots +0x2DE..+0x2E1 -> 0x80/0x40/0x20/0x10. */
u8 input_dirs_p1(void);

/* 0x1B6A0. P2's directions: slots +0x2E6..+0x2E9 -> 0x80/0x40/0x20/0x10. */
u8 input_dirs_p2(void);

/* 0x1B730. P1's buttons: slots +0x2E2..+0x2E5 -> 1/2/4/8. */
u8 input_buttons_p1(void);

/* 0x1B7C0. P2's buttons: slots +0x2EA..+0x2ED -> 1/2/4/8. */
u8 input_buttons_p2(void);

/* 0x1B850. F1's fixed table entry +0x28F (= +0x254+0x3B) -> bit 0. */
u8 input_f1(void);

/* 0x1B870. F2's fixed table entry +0x290 (= +0x254+0x3C) -> bit 0. */
u8 input_f2(void);

/* ---- the game-port button readers (record 2026-10-21-host-input-truth §H3)
 * The three functions 0x1BBAC's device 2/4/6 arms call. They read the PC game
 * port's button byte (port 0x201, bits 4..7), whose inputs are ACTIVE LOW: a
 * pressed button grounds its bit, so a reader sets its game bit when the port
 * bit is CLEAR. 0x1B890 maps bits 4..7 -> 1/2/4/8 (both players); 0x1B8DC maps
 * bits 4/5 -> 1/4 (P1); 0x1B908 maps bits 6/7 -> 1/4 (P2). The host supplies
 * the byte through host_port_buttons(). */

/* 0x1B890. Both players: game-port bits 4..7 -> 1/2/4/8. */
u8 input_port_buttons_both(void);

/* 0x1B8DC. P1: game-port bits 4/5 -> 1/4. */
u8 input_port_buttons_p1(void);

/* 0x1B908. P2: game-port bits 6/7 -> 1/4. */
u8 input_port_buttons_p2(void);

/* ---- the axis readers (record 2026-10-21-host-input-truth §H4) ------------
 * 0x1BBAC's device 2/4/6 arms read the four normalized axis words and their
 * calibrated centers: `(axis - center) >= 0x1E -> 0x10`, `<= -0x1E -> 0x20`
 * for X and `0x40/0x80` for Y, a +-0x1D dead band around the center and both
 * axes independent (8-way). The subtraction is 16-bit and wraps (the raw's
 * `sub cx,`); the host's pad model fills the axis words (host_input_fill_axes,
 * host_pad_fill). */

/* 0x4FF8F. Player 1's axes: 0xE1C1E/0xE1C20 against 0xE1C22/0xE1C24. */
u8 input_axes_a(void);

/* 0x4FFD8. Player 2's axes: 0xE1C26/0xE1C28 against 0xE1C2A/0xE1C2C. */
u8 input_axes_b(void);

/* ---- the ISR input sampler (record 2026-10-21-host-input-truth §H4) -------
 * 0x1BBAC, the original's 60 Hz interrupt sampler (called from 0x1BDF4 at
 * 0x1BE1C). It reads the two device words +0x2D4/+0x2D6 from the key-config
 * block, checks the game port's presence when either selects an arm (the
 * host-owned 0x4FDF5/0x4FBBB/0x4FC05/0x4FCFD, including the device-word
 * clears), then builds P1's byte +0x2D8 and P2's +0x2D9 from the selected arm:
 *
 *   dev | P1 (+0x2D4)                                  | P2 (+0x2D6)
 *   0   | directions 0x1B610, buttons 0x1B730, F1        | 0x1B6A0/0x1B7C0, F2
 *   2   | axes A 0x4FF8F, game-port 0x1B890, F1          | axes A, 0x1B890, F2
 *   4   | axes A, game-port P1 0x1B8DC, F1               | axes B 0x4FFD8, 0x1B908, F2
 *   6   | axes A, keyboard buttons 0x1B730, F1           | axes B, 0x1B7C0, F2
 *
 * Any other device leaves that player's byte untouched. F1/F2 are OR'd into
 * bit 0 of their player's byte in every arm (0x1BD53/0x1BDE0). */
void input_sampler(void);

#endif /* PR_INPUT_H */
