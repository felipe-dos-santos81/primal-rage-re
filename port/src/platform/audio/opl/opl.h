/* Private OPL FM synthesiser wrapper.
 *
 * Vendored core: opal 2.0.3, MIT, https://github.com/RealBitdancer/opal
 * (commit 7e829f3334e2a0b6aadc370c28c66acc83032d42). Its synthesis core is by
 * Shayde/Reality (Reality Adlib Tracker 2) and is public domain; the full
 * licence text is in LICENSE.opal.txt beside this file. Only this header is
 * reachable from outside the directory; the core's opal.h stays private.
 *
 * The original game drives one OPL stream through its AIL/MDI FM driver. The
 * captured register trace is dual-OPL2 (second register set present), so the
 * wrapper accepts the full OPL3 register range: 0x000-0x0FF first set,
 * 0x100-0x1FF second set (0x105 bit 0 enables OPL3 mode).
 */
#ifndef PR_OPL_H
#define PR_OPL_H

#include "types.h"

/* Initialises the chip to its power-on state. Must be called before the first
 * write or render. Deterministic: the same call leaves the same state. */
void opl_reset(void);

/* Writes one chip register. `reg` spans 0x000-0x1FF (both register sets). */
void opl_write(u16 reg, u8 value);

/* Renders `frames` stereo frames into `out` as interleaved s16 samples (2 per
 * frame: left, right). The core is synchronous; output depends only on the
 * register writes made so far, so the same writes produce the same samples. */
void opl_render(s16 *out, u32 frames);

/* Test seam: register writes since the last opl_reset. Lets callers assert that
 * a stopped sequencer emits no further writes. Not used by the game path. */
u32 opl_write_count(void);

/* Test seam: the first OPL_TRACE_MAX writes since the last opl_reset, in order,
 * so the sequencer oracle can diff the exact (reg, value) stream. Index i is
 * valid for i < opl_write_count() (writes past the cap are dropped and flagged
 * by opl_trace_overflow()). Not used by the game path. */
#define OPL_TRACE_MAX 16384u
u16 opl_trace_reg(u32 i);
u8 opl_trace_val(u32 i);
int opl_trace_overflow(void);

#endif /* PR_OPL_H */
