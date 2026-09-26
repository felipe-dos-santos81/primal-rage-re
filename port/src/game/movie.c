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

/* PORT: the dump seam. 0x1C740 writes the screen inside one master-loop
 * iteration (its own VBlank-gated blits, not the 0x25643 present), so a driver
 * that dumps once per iteration cannot see those screens. When set, the hook
 * runs after each screen the player writes: the entry and exit blanks
 * (0x52106) and each frame's present. NULL (the default) does nothing. */
static void (*s_screen_hook)(void);

void movie_set_screen_hook(void (*hook)(void)) { s_screen_hook = hook; }

static void movie_screen_changed(void)
{
    if (s_screen_hook != NULL) s_screen_hook();
}

/* 0x1C740 — game_flow "Boot logos". 0x52106(0) at entry blanks the screen and
 * zeroes the tick counters; the open/decode loop runs every frame
 * uVar7 = 1..piVar3[3]: palette (0x65340, after a VBlank spin), decode
 * (0x64130), blit of the frame's dirty rectangles (0x64ED8/0x50D23), advance
 * (0x643CC) unless it is the last, and the per-frame wait (0x65240); the
 * key test (0x62756/0x50161) leaves the loop. The close (0x63CE8) is followed
 * by 0x52106(0) again, inside the opened arm. So the last frame is blitted too
 * and then blanked: capture 1886 is the blank spliced into TWI5 frame 0, and
 * capture 2094 is TWI5 frame 119 spliced into frame 120 (the 121st) before
 * the exit blank (capture 2095).
 * PORT: the raw's DS_000A81A8 and 0x62756 skip tests at entry are not
 * modelled (the port's loop exit flag and key poll); ESC leaves the loop. */
int movie_play(const char *game_dir, const char *name)
{
    s_presented = 0;
    s_pace = 0;
    if (game_dir == NULL || name == NULL || name[0] == '\0') return 0;

    gfx_screen_reset(0u);                          /* 0x1C74D 0x52106 */
    movie_screen_changed();

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

    for (u32 i = 0; i < n; i++) {
        if (!smk_decode_frame(&m, draw)) return 0;
        smk_palette_to(&m, gfx_dac);
        movie_present(w, h);
        s_presented++;
        movie_screen_changed();

        movie_pace(delay);
        host_pump();
        /* Same quit arm as game_loop(): drain the queue, so a key queued before
         * the ESC cannot pin the head and hide it (input.h INPUT_ESC). A window
         * close must stop the movie too: the loop that honours it does not run
         * while a movie plays. */
        if (input_drain_esc() || host_quit_requested()) break;
    }
    gfx_screen_reset(0u);                          /* 0x1C873 0x52106 */
    movie_screen_changed();
    return 1;
}
