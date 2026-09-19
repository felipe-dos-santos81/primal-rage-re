/* port/src/game/effects.h */
#ifndef PRAGE_GAME_EFFECTS_H
#define PRAGE_GAME_EFFECTS_H

#include "../types.h"

/* The 0x13xxx effect list. 24 records of stride 0x814 run 0xF0B00..0xFC4CC;
 * the active and free sentinels are DS_000FCCE0 and DS_000FCCE8, each a
 * {next@+0; prev@+4} dword pair pointing at itself when its list is empty. The
 * interrupt lock DS_0009AF3C and the active count DS_0009AF3D live at 0x9AF3C.
 * All of these are generated in symbols.h. */
#define EFFECTS_REC_SIZE 0x814u

/* 0x13ADC. Self-links both sentinels, then tail-appends the 24 records to the
 * free list (0x249C0 insert-before, so 0xF0B00 stays the head). */
void effects_init(void);

/* 0x13C70. Pops the free-list head, fills it from `source_rec` and head-inserts
 * it into the active list; DS_0009AF3D++. Returns the record's mem[] offset, or
 * 0 when the free list is empty. The register arguments are EAX = source_rec,
 * DL = byte_arg (stored at rec+0xD), EBX = handle (resolved through 0x1B544);
 * the entry count and the copied block come from `source_rec`'s +0xC. */
u32 effects_spawn(u32 source_rec, u32 byte_arg, u32 handle);

/* 0x13D4C. Pops a free record, fills it as type 4 (darken-to-zero over the
 * resolved block) and head-inserts it into the active list. Same contract as
 * effects_spawn: returns the record offset, or 0 when the pool is
 * unbuilt/empty. The handle is DSD(source_rec) (the raw reads [source_rec]). */
u32 effects_spawn_darken(u32 source_rec, u32 byte_arg);

/* 0x13E28. Pops a free record, fills it as type 6 (darken-toward-target over
 * the resolved block) and head-inserts it into the active list. Same contract
 * as effects_spawn: returns the record offset, or 0 when the pool is
 * unbuilt/empty. The handle is DSD(source_rec) (the raw reads [source_rec]). */
u32 effects_spawn_pulse(u32 source_rec, u32 byte_arg);

/* 0x134C0. Ages every active record one frame. Each record's state byte
 * (rec+0xE) counts down from the byte at rec+0xD; when it wraps the record's
 * type body runs, animating its +0x10 block (through 0x33734's palette append)
 * and dropping it to the free list once finished, draining DS_0009AF3D each
 * time. Called from the master loop after DS_00104AF4++ and before the palette
 * flush. A no-op while the lock is set. */
void effects_step(void);

/* 0x13DF0. Tears down every active record (0x13420) and zeroes the active count
 * and the lock. A no-op when the pool was never initialised (the zeroed active
 * sentinel). */
void effects_clear(void);

/* DS_0009AF3D, the active-record count. */
int effects_active(void);

#endif /* PRAGE_GAME_EFFECTS_H */
