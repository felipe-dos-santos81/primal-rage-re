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

/* Non-consuming presence test: 1 when a key is queued, else 0. */
int input_has_key(void);

/* int 16h AH=0: blocks (draining host_pump()) until a key is queued, then
 * dequeues and returns it packed as (scan << 8) | ascii. */
u16 input_get_key(void);

/* int 16h AH=1: peeks without dequeuing. Returns the packed key, or 0 when the
 * queue is empty (0x0000 is never a real BIOS key). */
int input_check_key(void);

/* Discards every queued key. */
void input_clear(void);

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

/* 0x2D2F0. The joystick accessor: a constant 0 in the shipped profile
 * (`xor eax,eax; ret`), so the joystick half of 0x4F644 is inert. */
u32 input_joystick_device(u32 selector);

/* 0x4F644. Builds DS_001088E4 (newly pressed) and DS_001088D8 (held) from the
 * level word and the two 0x50161 mask families. */
void input_state_update(void);

#endif /* PR_INPUT_H */
