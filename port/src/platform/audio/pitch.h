#ifndef PR_PITCH_H
#define PR_PITCH_H
#include "types.h"

/* PORT: the driver's note-frequency routine (SBPRO2.MDI 0x35fa-0x36a6).
 * `index` is note+base (melodic) or base (percussion); `bend` is
 * pitch_bend_of(). Writes the 0xA0/0xB0 payload. bend == 0 reproduces the
 * retired NOTE_TAB except at indices 0-11, 16, 18 and 108-127, where the
 * driver's fold and negative-fnum carry differ (see the Task-1 record 3b). */
s32  pitch_bend_of(int wheel14, int scale);
void pitch_lookup(int index, s32 bend, u8 *a0, u8 *b0);
#endif /* PR_PITCH_H */
