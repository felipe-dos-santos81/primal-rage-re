#include "game/movie.h"

#include "host.h"
#include "mem.h"
#include "symbols.h"
#include "platform/gfx.h"
#include "platform/input.h"
#include "platform/res.h"
#include "platform/smacker.h"

#include <stdio.h>
#include <string.h>

/* PORT: the shipped fixed profile — both logos are 320x200 (the decoder also
 * accepts smaller synthetic grids for its own tests, hence the bound below). */
#define MOVIE_W 320u
#define MOVIE_H 200u
#define MOVIE_PIXELS (MOVIE_W * MOVIE_H)

/* PORT: int 16h packs a key as (scan << 8) | ascii; ESC is scan 0x01/ascii
 * 0x1B, the same code game_loop() tests. */
#define MOVIE_ESC 0x011Bu

static u32 s_presented;   /* frames presented by the last movie_play() */
static u32 s_pace;        /* sub-tick pacing remainder, in us*60 */

u32 movie_frames_presented(void) { return s_presented; }

/* PORT: 0x1C740 blits DAT_000E87A4 to the screen aperture once per frame; the
 * port's replacement is gfx_present() on the same buffer. The movie loop does
 * not own the double-buffer swap (0x255CC does), and must not: the decoder is
 * in-place (SKIP and delta blocks reference the previous frame), so every frame
 * decodes into the same buffer and swapping would lose that state. Headless
 * --check reaches the same call with no window; host_present_rgb() is a no-op. */
static void movie_present(u32 w, u32 h)
{
    gfx_present(mem + DSD(DS_000E87A4), (int)w, (int)h);
}

/* PORT: pace one frame by smk_frame_delay_us, converted to whole 60 Hz host
 * ticks, carrying the sub-tick remainder so a long movie does not drift. With
 * no window open (the test suite, `--check`) there is nothing to display, so
 * the real-time wait is skipped and the run stays fast and deterministic; the
 * frame is still decoded and presented through the same seam. */
static void movie_pace(u32 delay_us)
{
    s_pace += delay_us * 60u;
    u32 ticks = s_pace / 1000000u;
    s_pace %= 1000000u;
    if (!host_has_window()) return;
    while (ticks-- != 0) host_wait_vblank();
}

int movie_play(const char *game_dir, const char *name)
{
    s_presented = 0;
    s_pace = 0;
    if (game_dir == NULL || name == NULL || name[0] == '\0') return 0;

    u32 off = 0, size = 0;
    if (!res_load_file(game_dir, name, &off, &size)) {
        fprintf(stderr, "movie: %s: not found, skipping\n", name);
        return 1;
    }

    static SmkMovie m;
    if (!smk_open(mem + off, size, &m)) {
        fprintf(stderr, "movie: %s: rejected, skipping\n", name);
        return 1;
    }

    u32 n = smk_frames(&m);
    u32 w = smk_width(&m), h = smk_height(&m);
    u32 wh = w * h;
    u32 delay = smk_frame_delay_us(&m);
    if (wh == 0 || wh > MOVIE_PIXELS) {
        fprintf(stderr, "movie: %s: %ux%u unsupported, skipping\n", name, w, h);
        return 1;
    }

    u8 *draw = mem + DSD(DS_000E87A4);
    static u8 last[MOVIE_PIXELS];

    for (u32 i = 0; i < n; i++) {
        int final = (n > 1u && i + 1u == n);
        if (final) {
            /* The original decodes every payload frame but shows the final one
             * only when it is a ring/hold frame — the image already on screen.
             * Decode it into `last`, seeded from the current draw buffer so its
             * deltas reference the right previous frame, then keep the drawn
             * image only when the final frame is a hold.
             * TODO(verify): the original's 0x1C740 player-loop semantics are
             * unproven (no working scriptable DOSBox-X debugger); this
             * fixed-profile rule reproduces the settled capture counts (TWI5
             * 120 of 121, TWG 41 of 41) via the trailing ring/hold evidence in
             * the task-5 ledger. */
            memcpy(last, draw, wh);
            if (!smk_decode_frame(&m, last)) return 0;
            if (memcmp(last, draw, wh) == 0) {
                smk_palette_to(&m, gfx_dac);
                movie_present(w, h);
                s_presented++;
            }
        } else {
            if (!smk_decode_frame(&m, draw)) return 0;
            smk_palette_to(&m, gfx_dac);
            movie_present(w, h);
            s_presented++;
        }

        movie_pace(delay);
        host_pump();
        if (input_check_key() == MOVIE_ESC) {
            input_clear();
            break;
        }
    }
    return 1;
}
