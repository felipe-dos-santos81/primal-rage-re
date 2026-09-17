/* SDL3 host. The only module besides main.c that touches SDL. Everything the
 * engine needs from the outside world goes through here: the window, the event
 * pump, the tick clock, frame presentation, raw file I/O, and audio output.
 *
 * Tick model. The original advances DAT_00105D88 from a 60 Hz interrupt handler
 * while loop code separately busy-polls VBlank. The port has no ISR: host_pump()
 * drains events and advances g_tick by the number of nominal 60 Hz intervals
 * elapsed since the last call, and host_wait_vblank() sleeps to the next
 * boundary. gfx_wait_vblank() calls host_pump() on every busy-wait, so the
 * counter keeps pace regardless of how often it is polled.
 *
 * PORT/host infra: g_tick and the g_* window/event state are host
 * infrastructure (window, clock, transient input), not game state; the game's
 * own counters live in mem[]. g_tick is the host's model of DAT_00105D88. */
#include "host.h"
#include "platform/input.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/* Original tick interval = 60 Hz. Task 13 pinned the rate from the engine's own
 * unit conversions (/ 0x3c = 60 at 0x32B00; timers wrap at 0xe10 = 3600 ticks =
 * 1 minute at 0x32970) and measured a live 60.05 Hz counter at physical
 * 0x2EBD88 (page offset 0xd88 == DAT_00105D88's, so very likely that counter).
 * TODO(verify): the interrupt vector that installs 0x2D62C is still unknown —
 * it has no static install site. See port/spec/game_flow.md "Tick". */
#define HOST_TICK_NS 16666667ull

/* HOST_TICK_MAX_CATCHUP lives in host.h: the audio service shares the host
 * clock's catch-up bound, so it must be a single source of truth. */

/* Set 1 BIOS scan codes for SDL_SCANCODE_A..Z, indexed - SDL_SCANCODE_A. */
static const u8 k_bios_letter[26] = {
    0x1E, 0x30, 0x2E, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17, 0x24, 0x25, 0x26,
    0x32, 0x31, 0x18, 0x19, 0x10, 0x13, 0x1F, 0x14, 0x16, 0x2F, 0x11, 0x2D,
    0x15, 0x2C
};

static u32 g_tick;
static uint64_t g_tick_base_ns;
static int g_tick_started;

static SDL_Window *g_window;
static SDL_Surface *g_scratch; /* RGB24 surface the game renders into */
static int g_sdl_video;
static int g_pending; /* a frame was submitted and awaits present */
static int g_w, g_h;

/* Monotonic, so a wall-clock step (NTP, manual set) cannot skew the tick base.
 * CLOCK_MONOTONIC is POSIX; the port already targets a POSIX host. */
static uint64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

/* PORT: BIOS int 16h gives the game scan+ASCII. SDL gives its own scancodes and
 * keycodes, so map the keys this game uses. US layout assumed; only the keys
 * named in the plan (arrows, space, enter, escape, letters) are mapped, not a
 * full set-1 table. Task 14 may extend this if the menus need more. */
static int translate_key(const SDL_KeyboardEvent *k, u8 *scan, u8 *ascii)
{
    *ascii = 0;
    switch (k->scancode) {
    case SDL_SCANCODE_ESCAPE: *scan = 0x01; *ascii = 0x1B; return 1;
    case SDL_SCANCODE_RETURN: *scan = 0x1C; *ascii = 0x0D; return 1;
    case SDL_SCANCODE_SPACE:  *scan = 0x39; *ascii = ' ';  return 1;
    case SDL_SCANCODE_UP:     *scan = 0x48; return 1;
    case SDL_SCANCODE_DOWN:   *scan = 0x50; return 1;
    case SDL_SCANCODE_LEFT:   *scan = 0x4B; return 1;
    case SDL_SCANCODE_RIGHT:  *scan = 0x4D; return 1;
    default: break;
    }
    if (k->scancode >= SDL_SCANCODE_A && k->scancode <= SDL_SCANCODE_Z) {
        int i = (int)(k->scancode - SDL_SCANCODE_A);
        int upper = ((k->mod & SDL_KMOD_SHIFT) != 0) !=
                    ((k->mod & SDL_KMOD_CAPS) != 0);
        *scan = k_bios_letter[i];
        *ascii = (u8)((upper ? 'A' : 'a') + i);
        return 1;
    }
    return 0;
}

static void drain_events(void)
{
    if (!g_sdl_video) return;
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_EVENT_QUIT:
            /* PORT: closing the window maps onto ESC so the loop leaves through
             * the same path as a keyed exit. */
            input_push(0x01, 0x1B);
            break;
        case SDL_EVENT_KEY_DOWN: {
            u8 scan, ascii;
            if (translate_key(&e.key, &scan, &ascii)) input_push(scan, ascii);
            break;
        }
        default: break;
        }
    }
}

static void present_pending(void)
{
    g_pending = 0;
    if (!g_window || !g_scratch) return;
    SDL_Surface *win = SDL_GetWindowSurface(g_window);
    if (!win) return;
    SDL_BlitSurface(g_scratch, NULL, win, NULL); /* converts RGB24 to window fmt */
    SDL_UpdateWindowSurface(g_window);
}

int host_init(const char *title, int w, int h)
{
    if (g_sdl_video) return 1;
    if (w <= 0 || h <= 0) return 0;
    if (!SDL_Init(SDL_INIT_VIDEO)) return 0; /* no display: report, do not abort */
    g_sdl_video = 1;
    g_window = SDL_CreateWindow(title, w, h, 0);
    if (!g_window) { host_shutdown(); return 0; }
    g_scratch = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGB24);
    if (!g_scratch) { host_shutdown(); return 0; }
    g_w = w;
    g_h = h;
    return 1;
}

void host_shutdown(void)
{
    host_audio_close(); /* one authoritative teardown, before SDL_Quit() */
    if (g_scratch) { SDL_DestroySurface(g_scratch); g_scratch = NULL; }
    if (g_window) { SDL_DestroyWindow(g_window); g_window = NULL; }
    if (g_sdl_video) { SDL_Quit(); g_sdl_video = 0; }
    g_pending = 0;
    g_w = g_h = 0;
}

void host_present_rgb(const u8 *rgb, int w, int h)
{
    if (!g_scratch || w != g_w || h != g_h) return; /* headless / mismatch */
    u8 *dst = (u8 *)g_scratch->pixels;
    for (int y = 0; y < h; y++)
        memcpy(dst + (size_t)y * (size_t)g_scratch->pitch,
               rgb + (size_t)y * (size_t)w * 3u, (size_t)w * 3u);
    g_pending = 1;
}

void host_pump(void)
{
    drain_events();
    present_pending();

    if (!g_tick_started) { g_tick_base_ns = now_ns(); g_tick_started = 1; }
    uint64_t now = now_ns();
    uint64_t missed = (now - g_tick_base_ns) / HOST_TICK_NS;
    if (missed > HOST_TICK_MAX_CATCHUP) {
        /* Long stall: advance a bounded amount and rebase, dropping the rest. */
        g_tick += HOST_TICK_MAX_CATCHUP;
        g_tick_base_ns = now;
    } else {
        g_tick += (u32)missed;
        g_tick_base_ns += missed * HOST_TICK_NS;
    }
}

void host_wait_vblank(void)
{
    if (g_tick_started) {
        uint64_t now = now_ns();
        uint64_t elapsed = now >= g_tick_base_ns ? now - g_tick_base_ns : 0;
        if (elapsed < HOST_TICK_NS) {
            uint64_t remain_ms = (HOST_TICK_NS - elapsed + 999999ull) / 1000000ull;
            SDL_Delay((Uint32)remain_ms); /* safe before SDL video init */
        }
    }
    host_pump();
}

u32 host_tick_count(void)
{
    return g_tick;
}

int host_read_file(const char *path, u8 *dst, u32 max, u32 *len_out)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    long sz;
    if (fseek(f, 0, SEEK_END) != 0 || (sz = ftell(f)) < 0 ||
        (uint64_t)sz > (uint64_t)max || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return 0;
    }
    size_t n = fread(dst, 1, (size_t)sz, f);
    fclose(f);
    if (n != (size_t)sz) return 0;
    if (len_out) *len_out = (u32)n;
    return 1;
}

int host_write_file(const char *path, const u8 *src, u32 len)
{
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    size_t n = fwrite(src, 1, (size_t)len, f);
    int ok = (n == (size_t)len);
    if (fclose(f) != 0) ok = 0;
    return ok;
}

/* Audio seam. This is the only place SDL's audio device exists; game-side audio
 * modules hand mixed stereo frames to host_audio_submit() and never see SDL.
 * The stream pulls from SDL's own queue (no callback), so host.c has no
 * reference to the mixer and every caller stays buildable and testable headless.
 *
 * PORT: fixed audio profile, no hardware probe. The seam asks SDL for the
 * default playback device at the caller's rate/channels; it never enumerates
 * devices or negotiates formats beyond what SDL needs to open.
 * TODO(verify): the failure branches (NULL from SDL_OpenAudioDeviceStream, or a
 * failed SDL_ResumeAudioStreamDevice on a host with no audio device) are not
 * exercised by the suite, which never opens a real device. */
static SDL_AudioStream *g_audio;
static int g_audio_rate;     /* > 0 iff the seam is open */
static int g_audio_channels;

/* Why the last host_audio_open() failed. SDL's own error distinguishes "no
 * device" from "device present but the stream could not start"; without it the
 * caller can only report the former. Captured before any teardown, which can
 * reset SDL's error state. */
static char g_audio_error[256];

const char *host_audio_error(void)
{
    return g_audio_error;
}

static void audio_fail(const char *what)
{
    snprintf(g_audio_error, sizeof g_audio_error, "%s: %s", what, SDL_GetError());
}

int host_audio_open(int rate, int channels)
{
    g_audio_error[0] = '\0';
    /* Guard before any SDL call, exactly like host_init(): an impossible profile
     * reports failure instead of letting SDL negotiate something unexpected. */
    if (rate <= 0 || channels <= 0) {
        snprintf(g_audio_error, sizeof g_audio_error,
                 "invalid profile (%d Hz, %d channels)", rate, channels);
        return 0;
    }
    if (g_audio) host_audio_close();
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        audio_fail("SDL_InitSubSystem(SDL_INIT_AUDIO)");
        return 0;
    }

    SDL_AudioSpec spec;
    spec.format = SDL_AUDIO_S16;
    spec.channels = channels;
    spec.freq = rate;
    SDL_AudioStream *s = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
    if (!s) {
        audio_fail("SDL_OpenAudioDeviceStream");
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return 0;
    }

    g_audio = s;
    g_audio_rate = rate;
    g_audio_channels = channels;
    /* SDL_OpenAudioDeviceStream leaves the stream paused; without this it renders
     * nothing even though submit succeeds. Resume now or tear down and fail. */
    if (!SDL_ResumeAudioStreamDevice(s)) {
        audio_fail("SDL_ResumeAudioStreamDevice");
        host_audio_close();
        return 0;
    }
    return 1;
}

void host_audio_close(void)
{
    if (g_audio) { SDL_DestroyAudioStream(g_audio); g_audio = NULL; }
    /* Only quit the subsystem if we initialised it (rate marks that). */
    if (g_audio_rate) SDL_QuitSubSystem(SDL_INIT_AUDIO);
    g_audio_rate = 0;
    g_audio_channels = 0;
}

void host_audio_submit(const s16 *frames, int frame_count)
{
    if (!g_audio || !frames || frame_count <= 0) return;
    /* Bound frame_count before multiplying: on a 32-bit size_t target the
     * product below could wrap past the INT32_MAX byte cap SDL takes. */
    if (frame_count > INT32_MAX / (g_audio_channels * (int)sizeof(s16))) return;
    size_t bytes = (size_t)frame_count * (size_t)g_audio_channels * sizeof(s16);
    SDL_PutAudioStreamData(g_audio, frames, (int)bytes);
}

u32 host_audio_rate(void)
{
    return (u32)g_audio_rate;
}
