#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "host.h"
#include "mem.h"
#include "symbols.h"
#include "platform/gfx.h"
#include "platform/audio/ail.h"
#include "platform/audio/mixer.h"
#include "game/flow.h"

static void usage(const char *argv0)
{
    printf("usage: %s --game-dir DIR [--check N]\n", argv0);
}

/* Task 14: open the SDL host, then hand control to the ported game flow
 * (init chain -> frame loop -> title state), which runs until ESC. The audio
 * device is opened here, with the window, so --check stays genuinely headless
 * (it never reaches run_windowed). A failed open leaves host_audio_rate() 0 and
 * the flow tick-runs the sequencer without rendering. */
static int run_windowed(const char *game_dir)
{
    if (!host_init("Primal Rage", 320, 200)) {
        fprintf(stderr, "prageport: no window/display available\n");
        return 1;
    }
    if (host_audio_open(MIXER_OPL_RATE, 2))
        printf("prageport: audio device open at %u Hz\n", host_audio_rate());
    else
        printf("prageport: audio unavailable (%s); running silent\n",
               host_audio_error());
    printf("prageport 0.0.1 game-dir=%s\n", game_dir);
    game_set_game_dir(game_dir);
    int rc = game_main();
    host_shutdown();
    return rc;
}

/* ---- Task 15: headless `--check N` -------------------------------------- */

#define CHECK_W 320
#define CHECK_H 200

/* PORT: the boot attract runs ~690 frames before the title. A `--check` request
 * at or above this bound is the `make verify` path (verify_frames = 820) and is
 * asserting that the run crosses into the title; if a future attract
 * lengthening pushes the title past the request, every title assertion below
 * would be skipped silently, so fail loudly instead. `make check`'s short
 * default stays below the bound and requires only the attract smoke render. */
#define CHECK_TITLE_REACH_FRAMES 700

/* Writes `len` bytes to `path`. host_write_file() reports success but leaves a
 * partial file behind if a mid-write failure occurs, so a failed capture removes
 * the artifact rather than leaving a truncated one for a comparison to read.
 * Returns 1 on success. */
static int write_exact(const char *path, const u8 *src, u32 len)
{
    if (host_write_file(path, src, len)) return 1;
    remove(path);
    fprintf(stderr, "prageport: --check could not write %s\n", path);
    return 0;
}

/* Dumps one captured frame as three artifacts under frames/:
 *   frames/frame_NNNN.ppm  binary P6 RGB, converted through the DAC exactly
 *                          as gfx_present() does (gfx_dac[idx][0..2]; 8-bit
 *                          guns).
 *   frames/frame_NNNN.pal  the 256-entry DAC, 3 bytes per entry.
 *   frames/frame_NNNN.idx  the raw 320x200 palette indices the decoder
 *                          produced.
 * The .idx is what makes the comparison against tools/gra_render.py
 * byte-exact: two different indices can share an RGB value, so the PPM alone
 * cannot prove the index buffers agree. `expect_drawn` gates the blank check:
 * the state-0 attract legitimately starts on a blank buffer before its scene
 * loads, so only the drawn title/select states are asserted non-blank. Returns
 * the failed-assertion count. */
static int capture_frame(int n, const u8 *indices, int expect_drawn)
{
    int fail = 0;
    char base[40];
    snprintf(base, sizeof base, "frames/frame_%04d", n);
    char path[48];

    static u8 ppm[32 + CHECK_W * CHECK_H * 3];
    int hdr = snprintf((char *)ppm, 32, "P6\n%d %d\n255\n", CHECK_W, CHECK_H);
    for (int i = 0; i < CHECK_W * CHECK_H; i++) {
        const u8 *c = gfx_dac[indices[i]];
        ppm[hdr + i * 3 + 0] = c[0];
        ppm[hdr + i * 3 + 1] = c[1];
        ppm[hdr + i * 3 + 2] = c[2];
    }
    snprintf(path, sizeof path, "%s.ppm", base);
    if (!write_exact(path, ppm, (u32)hdr + CHECK_W * CHECK_H * 3)) fail++;

    snprintf(path, sizeof path, "%s.pal", base);
    if (!write_exact(path, (const u8 *)gfx_dac, (u32)sizeof gfx_dac)) fail++;

    snprintf(path, sizeof path, "%s.idx", base);
    if (!write_exact(path, indices, CHECK_W * CHECK_H)) fail++;

    /* Internal assertion: a drawn presented buffer is a drawn image, not the
     * never-drawn second buffer (the blank-every-other-frame bug Task 14
     * fixed). --check must fail rather than emit a blank capture. */
    if (expect_drawn) {
        int nonzero = 0;
        for (int i = 0; i < CHECK_W * CHECK_H; i++) if (indices[i]) nonzero++;
        if (nonzero <= 1000) {
            fprintf(stderr, "prageport: --check frame %d is blank (%d pixels)\n",
                    n, nonzero);
            fail++;
        }
    }
    return fail;
}

static u32 buf_hash(const u8 *p)
{
    u32 h = 2166136261u;
    for (int i = 0; i < CHECK_W * CHECK_H; i++) { h ^= p[i]; h *= 16777619u; }
    return h;
}

/* Record §K7 (k7-k12 derivations §0.7.5): the attract's phase 2 queues the
 * looping s16title samples 0x40 (handle 0x0383B6F4) and 0x42 (0x03837440),
 * loop byte 1, at 0x11160/0x1116C; 0x1CF20 -> 0x1CB18 starts them, and the
 * title's first entry stops them (0x121CE voice 0x41, 0x121D8 voice 0x43).
 * 1 when a slot holds `h` as playing (+0x0C and AIL status 4). No device is
 * open, so the mixer renders on the virtual clock (record named-gaps-d §D.1):
 * a one-shot ends at its length, and these loops (loop count 0, 0x1CBE1)
 * stay live until the title stops them. */
static int attract_loop_playing(u32 h)
{
    for (u32 i = 0; i < 4u; i++)
        if (DSD(DS_0010286C + i * 0x18u) == h &&
            AIL_sample_status(sound_slot_handle(i)) == 4)
            return 1;
    return 0;
}

static const u32 attract_loops[2] = { 0x0383B6F4u, 0x03837440u };

/* PORT: the original has no headless mode. The port runs the real master loop
 * one frame at a time without opening a window: game_init() runs the init chain
 * once, then each game_loop() call advances exactly one frame because the loop
 * stops as soon as the quit flag DS_000A81A8 is set (preset here). The init is
 * split from game_main() so its teardown cannot stop the music between frames,
 * and no device is opened. host_init() is never called, so SDL opens no window
 * and needs no display, and the captured content is driven by the loop's own
 * frame counter, not by wall time. The presented buffer after the call is at
 * DS_000E87A0 (the loop presents DS_000E87A4, then 0x50188 swaps).
 * Boot enters state 0 (attract), so the title facts are asserted relative to the
 * title entry rather than a fixed frame; a run shorter than the attract is a
 * valid attract-only smoke test and skips them. Runs exactly `frames`
 * iterations and returns the accumulated failed-assertion count. */
static int run_check(const char *game_dir, int frames)
{
    mkdir("frames", 0755);             /* ignore EEXIST; capture_frame needs it */
    game_set_game_dir(game_dir);
    int fail = 0, distinct = 0;
    int title_entry = 0, title_frames = 0, probed = 0;
    int loop_before_title[2] = { 0, 0 };
    u32 last_hash = 0;
    u32 audio0 = game_audio_ticks();
    u32 host0 = host_tick_count();
    game_init();                       /* init chain once; runs the audio init */
    for (int i = 1; i <= frames; i++) {
        DSB(DS_000A81A8) = 1;            /* one loop iteration per call */
        game_loop();                     /* frame i */
        u16 st = DSW(DS_000F0A64);
        if (st == 0)
            for (int k = 0; k < 2; k++)
                loop_before_title[k] = attract_loop_playing(attract_loops[k]);
        const u8 *presented = gfx_display();
        if (presented == NULL) presented = mem + DSD(DS_000E87A0);
        /* State 0 is the attract (blank until its scene loads); states 1..9 are
         * drawn screens and must not be blank. */
        fail += capture_frame(i, presented, st != 0 && st <= 9);
        if (st == 1) {
            if (title_entry == 0) title_entry = i;
            title_frames++;
            u32 h = buf_hash(presented);
            if (h != last_hash) { distinct++; last_hash = h; }
        }
        /* The title's first entry stopped the attract's 0x40 and 0x42
         * loops, which played on the last attract frame. */
        if (title_entry != 0 && !probed && i == title_entry + 2) {
            for (int k = 0; k < 2; k++) {
                if (!loop_before_title[k]) {
                    fprintf(stderr, "prageport: --check the attract's loop "
                                    "0x%08X was not playing before the title\n",
                            (unsigned)attract_loops[k]);
                    fail++;
                }
                if (attract_loop_playing(attract_loops[k])) {
                    fprintf(stderr, "prageport: --check the title did not stop "
                                    "the attract's loop 0x%08X\n",
                            (unsigned)attract_loops[k]);
                    fail++;
                }
            }
            probed = 1;
        }
    }

    /* PORT: a run at or above the attract bound claims to reach the title; if
     * it did not, fail so a lengthened attract cannot silently skip every title
     * assertion below. */
    if (frames >= CHECK_TITLE_REACH_FRAMES && title_entry == 0) {
        fprintf(stderr,
                "prageport: --check %d frames never reached the title "
                "(attract longer than %d?)\n",
                frames, CHECK_TITLE_REACH_FRAMES);
        fail++;
    }

    /* A hold pair is TITLE_HOLD_FRAMES = 8, so only the title window (>= 9
     * frames) can assert that the title animates. */
    if (title_frames >= 9 && distinct < 2) {
        fprintf(stderr, "prageport: --check title did not animate (%d image(s))\n",
                distinct);
        fail++;
    }
    int dac_set = 0;
    for (int i = 0; i < 256 && !dac_set; i++)
        if (gfx_dac[i][0] || gfx_dac[i][1] || gfx_dac[i][2]) dac_set = 1;
    if (frames >= 1 && !dac_set) {
        fprintf(stderr, "prageport: --check title palette never reached gfx_dac\n");
        fail++;
    }

    /* Task 11: the frame loop must drive the sequencer with no device, paced by
     * the host's 60 Hz clock, not by the loop-iteration count. The service
     * derives two XMIDI ticks per measured host tick, so ticks can never outrun
     * the observed host delta. A genuine stall longer than the host clock's own
     * catch-up bound is clamped for both, so the sequencer may legitimately
     * trail the delta; the only failure is a silent stall to zero (the service
     * never ran), which a non-trivial observed delta makes unambiguous. */
    u32 host_delta = host_tick_count() - host0;
    u32 ticks = game_audio_ticks() - audio0;
    if (ticks > 2u * host_delta) {
        fprintf(stderr,
                "prageport: --check sequencer outran the host clock (%u > %u)\n",
                (unsigned)ticks, (unsigned)(2u * host_delta));
        fail++;
    }
    if (host_delta > 1u && ticks == 0) {
        fprintf(stderr,
                "prageport: --check sequencer stalled at zero over %u host "
                "tick(s)\n",
                (unsigned)host_delta);
        fail++;
    }
    /* The title bank's first note is at XMIDI tick 59 (Task 8) = frame 30 at two
     * ticks/frame; a title window past that must have keyed a note, proving the
     * music bank loaded and sounded, not merely that the service ticked. */
    if (title_frames >= 60 && !game_music_notes_seen()) {
        fprintf(stderr, "prageport: --check title music keyed no notes\n");
        fail++;
    }
    game_shutdown();                   /* 0x1BE30 teardown */
    return fail > 255 ? 255 : fail;
}

int main(int argc, char **argv)
{
    const char *game_dir = "data/game/C";
    int check_frames = 0;
    int have_check = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--game-dir") == 0 && i + 1 < argc) {
            game_dir = argv[++i];
        } else if (strcmp(argv[i], "--check") == 0 && i + 1 < argc) {
            check_frames = atoi(argv[++i]);
            have_check = 1;
        } else if (strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        } else {
            usage(argv[0]);
            return 2;
        }
    }
    if (have_check) {
        if (check_frames < 1) {
            fprintf(stderr, "prageport: --check needs a frame count >= 1\n");
            usage(argv[0]);
            return 2;
        }
        printf("prageport 0.0.1 game-dir=%s --check %d (headless)\n",
               game_dir, check_frames);
        int failures = run_check(game_dir, check_frames);
        if (failures)
            fprintf(stderr, "prageport: --check %d assertion failure(s)\n", failures);
        return failures;
    }
    return run_windowed(game_dir);
}
