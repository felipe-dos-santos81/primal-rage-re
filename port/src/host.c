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
#include "mem.h"
#include "symbols.h"
#include "platform/input.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Original tick interval = 60 Hz. Task 13 pinned the rate from the engine's own
 * unit conversions (/ 0x3c = 60 at 0x32B00; timers wrap at 0xe10 = 3600 ticks =
 * 1 minute at 0x32970) and measured a live 60.05 Hz counter at physical
 * 0x2EBD88 (page offset 0xd88 == DAT_00105D88's, so very likely that counter).
 * PORT: no vector installs 0x2D62C. 0x1CF40 registers 0x1BDF4 as an AIL timer
 * callback (0x1CFED push 0x1bdf4; 0x1CFF2 AIL_register_timer 0x5DA12) at 60 Hz
 * (0x1CFFA push 0x3c; 0x1CFFF AIL_set_timer_frequency 0x5DA87) and starts it
 * (0x1D008 0x5DAA6); 0x1BDF4 calls 0x2D62C at 0x1BE28, only while the byte
 * DS_00104B22 is not 1 (0x1BDF8..0x1BE00). The dword at 0x1BF5C is the
 * 0x109A0 memory lock of 0x2D62C's code, not an install. g_tick is the host's
 * own pacing counter and ignores that gate. The game's counter DS_00105D88 is
 * not g_tick: the port's modelled ISR ticks advance it (flow.c game_isr_ticks,
 * config.c's key wait; 0x2D62C is config_isr_tick), and the run clock 0x32970
 * (config_run_clock) reads it (record 2026-10-13-reverse-s §S.9). See
 * port/spec/game_flow.md "Tick". */
#define HOST_TICK_NS 16666667ull

/* HOST_TICK_MAX_CATCHUP lives in host.h: the audio service shares the host
 * clock's catch-up bound, so it must be a single source of truth. */

/* PORT: window policy is the host's own (the original has no window). The
 * window opens at 1024x768 — 4:3, the aspect a DOS CRT showed the 320x200
 * frame at — and is user-resizable; present_pending() scales the frame to
 * fill whatever the current size is. */
#define HOST_WINDOW_W 1024
#define HOST_WINDOW_H 768

/* Set 1 BIOS scan codes for SDL_SCANCODE_A..Z, indexed - SDL_SCANCODE_A. */
static const u8 k_bios_letter[26] = {
    0x1E, 0x30, 0x2E, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17, 0x24, 0x25, 0x26,
    0x32, 0x31, 0x18, 0x19, 0x10, 0x13, 0x1F, 0x14, 0x16, 0x2F, 0x11, 0x2D,
    0x15, 0x2C
};

/* PORT: the host stands in for the IRQ1 key-state table and the ISR sampler
 * 0x1BBAC (host-owned; record §50-C). The sampler's device-0 arms read their
 * binding out of the key-config block at [DS_00101514]: 0x1B610/0x1B730 test
 * P1's eight scans at +0x2DE..+0x2E5 by position, 0x1B6A0/0x1B7C0 P2's at
 * +0x2E6..+0x2ED, and 0x1B850/0x1B870 the fixed +0x28F/+0x290 key-state bytes
 * (record gameplay-ground-truth §G.1.2; every bit captured in gp-pads, §G.7.2).
 * The block is the default record's scan list at 0xA2C62 after config_keys_apply
 * (0x1AE28) or the service menu's edited one, and the config image's mirror
 * (0x1014AC..) reaches it through the 0x20CF2/0x20CF9 pack/load pair at boot,
 * so a windowed run follows the configured binding.
 *
 * The set-1 scan -> SDL scancode map (record §H2). The config holds set-1
 * scans, so every one of the 16 slots must be sampled, not only the 26 letters
 * and 10 pad keys the first cut knew. 0 is SDL_SCANCODE_UNKNOWN. Two keys share
 * one set-1 code: the keypad's 0x47..0x53 and the navigation keys (the BIOS's
 * int 16h hands back the same scan for both), and the map takes the key the
 * default record binds (arrows / Home PgUp End PgDn), as the first cut did. */
static const SDL_Scancode k_scan_sdl[0x80] = {
    [0x01] = SDL_SCANCODE_ESCAPE,
    [0x02] = SDL_SCANCODE_1, [0x03] = SDL_SCANCODE_2, [0x04] = SDL_SCANCODE_3,
    [0x05] = SDL_SCANCODE_4, [0x06] = SDL_SCANCODE_5, [0x07] = SDL_SCANCODE_6,
    [0x08] = SDL_SCANCODE_7, [0x09] = SDL_SCANCODE_8, [0x0A] = SDL_SCANCODE_9,
    [0x0B] = SDL_SCANCODE_0, [0x0C] = SDL_SCANCODE_MINUS, [0x0D] = SDL_SCANCODE_EQUALS,
    [0x0E] = SDL_SCANCODE_BACKSPACE, [0x0F] = SDL_SCANCODE_TAB,
    [0x10] = SDL_SCANCODE_Q, [0x11] = SDL_SCANCODE_W, [0x12] = SDL_SCANCODE_E,
    [0x13] = SDL_SCANCODE_R, [0x14] = SDL_SCANCODE_T, [0x15] = SDL_SCANCODE_Y,
    [0x16] = SDL_SCANCODE_U, [0x17] = SDL_SCANCODE_I, [0x18] = SDL_SCANCODE_O,
    [0x19] = SDL_SCANCODE_P, [0x1A] = SDL_SCANCODE_LEFTBRACKET,
    [0x1B] = SDL_SCANCODE_RIGHTBRACKET, [0x1C] = SDL_SCANCODE_RETURN,
    [0x1D] = SDL_SCANCODE_LCTRL,
    [0x1E] = SDL_SCANCODE_A, [0x1F] = SDL_SCANCODE_S, [0x20] = SDL_SCANCODE_D,
    [0x21] = SDL_SCANCODE_F, [0x22] = SDL_SCANCODE_G, [0x23] = SDL_SCANCODE_H,
    [0x24] = SDL_SCANCODE_J, [0x25] = SDL_SCANCODE_K, [0x26] = SDL_SCANCODE_L,
    [0x27] = SDL_SCANCODE_SEMICOLON, [0x28] = SDL_SCANCODE_APOSTROPHE,
    [0x29] = SDL_SCANCODE_GRAVE, [0x2A] = SDL_SCANCODE_LSHIFT,
    [0x2B] = SDL_SCANCODE_BACKSLASH,
    [0x2C] = SDL_SCANCODE_Z, [0x2D] = SDL_SCANCODE_X, [0x2E] = SDL_SCANCODE_C,
    [0x2F] = SDL_SCANCODE_V, [0x30] = SDL_SCANCODE_B, [0x31] = SDL_SCANCODE_N,
    [0x32] = SDL_SCANCODE_M, [0x33] = SDL_SCANCODE_COMMA,
    [0x34] = SDL_SCANCODE_PERIOD, [0x35] = SDL_SCANCODE_SLASH,
    [0x36] = SDL_SCANCODE_RSHIFT, [0x37] = SDL_SCANCODE_KP_MULTIPLY,
    [0x38] = SDL_SCANCODE_LALT, [0x39] = SDL_SCANCODE_SPACE,
    [0x3A] = SDL_SCANCODE_CAPSLOCK,
    [0x3B] = SDL_SCANCODE_F1, [0x3C] = SDL_SCANCODE_F2, [0x3D] = SDL_SCANCODE_F3,
    [0x3E] = SDL_SCANCODE_F4, [0x3F] = SDL_SCANCODE_F5, [0x40] = SDL_SCANCODE_F6,
    [0x41] = SDL_SCANCODE_F7, [0x42] = SDL_SCANCODE_F8, [0x43] = SDL_SCANCODE_F9,
    [0x44] = SDL_SCANCODE_F10, [0x45] = SDL_SCANCODE_NUMLOCKCLEAR,
    [0x46] = SDL_SCANCODE_SCROLLLOCK,
    [0x47] = SDL_SCANCODE_HOME, [0x48] = SDL_SCANCODE_UP,
    [0x49] = SDL_SCANCODE_PAGEUP, [0x4A] = SDL_SCANCODE_KP_MINUS,
    [0x4B] = SDL_SCANCODE_LEFT, [0x4C] = SDL_SCANCODE_KP_5,
    [0x4D] = SDL_SCANCODE_RIGHT, [0x4E] = SDL_SCANCODE_KP_PLUS,
    [0x4F] = SDL_SCANCODE_END, [0x50] = SDL_SCANCODE_DOWN,
    [0x51] = SDL_SCANCODE_PAGEDOWN, [0x52] = SDL_SCANCODE_INSERT,
    [0x53] = SDL_SCANCODE_DELETE, [0x54] = SDL_SCANCODE_PRINTSCREEN,
    [0x56] = SDL_SCANCODE_NONUSBACKSLASH,
    [0x57] = SDL_SCANCODE_F11, [0x58] = SDL_SCANCODE_F12,
};

/* The key-config block's 16 scan slots in the sampler's own bit order: P1's
 * +0x2DE..+0x2E1 give 0x80 0x40 0x20 0x10 of +0x2D8 (0x1B610) and
 * +0x2E2..+0x2E5 give 1 2 4 8 (0x1B730); P2's +0x2E6..+0x2ED the same of
 * +0x2D9 (0x1B6A0/0x1B7C0), as the kb word (0x2D8 << 8) | 0x2D9 packs them. */
static const u16 k_cfg_bit[16] = {
    0x8000u, 0x4000u, 0x2000u, 0x1000u, 0x0100u, 0x0200u, 0x0400u, 0x0800u,
    0x0080u, 0x0040u, 0x0020u, 0x0010u, 0x0001u, 0x0002u, 0x0004u, 0x0008u
};

/* The game's default binding (the record at 0xA2C62: P1 S X Z C / U I N M, P2
 * the arrows / Home PgUp End PgDn) for a run whose block is absent or all
 * zero. 0x3B/0x3C (F1/F2) are outside the block: 0x1B850/0x1B870 read them at
 * the fixed +0x28F/+0x290. */
static const u8 k_default_scan[16] = {
    0x1F, 0x2D, 0x2C, 0x2E, 0x16, 0x17, 0x31, 0x32,
    0x48, 0x50, 0x4B, 0x4D, 0x47, 0x49, 0x4F, 0x51
};

/* The 16 scans a run samples: the configured block when one is filled, else the
 * default record's. NULL when DS_00101514 does not point into mem[]. */
static const u8 *host_key_scans(void)
{
    const u32 base = DSD(DS_00101514);
    if (base == 0u || base > MEM_SIZE - 0x2EEu) return NULL;
    const u8 *cfg = mem + base + 0x2DEu;
    u8 any = 0u;
    for (u32 i = 0u; i < 16u; i++) any |= cfg[i];     /* a zero table is no table */
    return any != 0u ? cfg : k_default_scan;
}

/* PORT: one set-1 scan's kb bit under the binding the sampler reads: the
 * configured scans when a run has filled the block (config_keys_apply, record
 * §50-C), else the game's default binding. Any other scan is no pad key. */
u16 host_kb_bit(u8 scan)
{
    const u32 base = DSD(DS_00101514);
    if (base != 0u && base <= MEM_SIZE - 0x2EEu) {
        const u8 *cfg = mem + base + 0x2DEu;
        u16 bits = 0u;
        u8 any = 0u;
        for (u32 i = 0u; i < 16u; i++) {
            any |= cfg[i];                       /* a zero table is no table */
            if (cfg[i] == scan) bits |= k_cfg_bit[i];
        }
        if (any != 0u) {
            if (scan == 0x3B) bits |= 0x0100u;   /* F1 (0x1B850, +0x28F) */
            if (scan == 0x3C) bits |= 0x0001u;   /* F2 (0x1B870, +0x290) */
            return bits;
        }
    }
    /* PORT: no configured block (no game_init ran, or its 16 scans are all
     * zero) falls back to the default binding; the raw's sampler would report
     * no pad key for a block whose slots are empty. The windowed run's block is
     * never empty: config_set_defaults (0x2CADC) applies the default record, or
     * the pack/load pair carries the image's own. */
    u16 bits = 0u;
    for (u32 i = 0u; i < 16u; i++)
        if (k_default_scan[i] == scan) bits |= k_cfg_bit[i];
    if (scan == 0x3B) bits |= 0x0100u;
    if (scan == 0x3C) bits |= 0x0001u;
    return bits;
}

static int g_key_bits_override_on;
static u16 g_key_bits_override;

void host_set_key_bits_override(u16 bits, int on)
{
    g_key_bits_override = bits;
    g_key_bits_override_on = on;
}

u16 host_key_bits(void)
{
    /* PORT: the test seam declared in host.h. */
    if (g_key_bits_override_on) return g_key_bits_override;
    const bool *st = SDL_GetKeyboardState(NULL);
    if (st == NULL) return 0u;
    const u8 *scans = host_key_scans();
    if (scans == NULL) return 0u;
    u16 bits = 0u;
    /* Every configured slot is sampled, whatever scan it names (record §H2):
     * the first cut walked the 26 letters and 10 pad keys, so a rebind outside
     * them was stored but never read. */
    for (u32 i = 0u; i < 16u; i++) {
        const u8 scan = scans[i];
        if (scan >= 0x80u) continue;                  /* no SDL key for it */
        const SDL_Scancode key = k_scan_sdl[scan];
        if (key != SDL_SCANCODE_UNKNOWN && st[key]) bits |= k_cfg_bit[i];
    }
    if (st[SDL_SCANCODE_F1]) bits |= 0x0100u;         /* 0x1B850 */
    if (st[SDL_SCANCODE_F2]) bits |= 0x0001u;         /* 0x1B870 */
    return bits;
}

/* One key-state entry: the RM.DRV int 9 handler (0x31B) stores the raw scan
 * byte at [base+0x254+(scan&0x7F)] -- the table is 128 entries, +0x254..+0x2D3,
 * with the config block's device words at +0x2D4. So the make byte has bit 7
 * clear and the break byte sets it; the six readers test bit 7. */
static void host_key_state_store(u8 *tbl, u8 scan, int down)
{
    const u8 make = (u8)(scan & 0x7Fu);
    tbl[make] = down ? make : (u8)(make | 0x80u);
}

/* PORT: fills the key-state table the ported readers (input_dirs_p1 .. input_f2)
 * read, from the same 16 bits host_key_bits() reports -- SDL's keyboard through
 * the configured binding, or the test seam's captured bitmap. The raw's ISR
 * writes only the key it saw; the host writes every bound slot and F1/F2 each
 * frame, so no read entry can keep a stale make. The default record's scans are
 * used when the block is absent or all zero (record §H2). */
void host_input_fill_keyboard(void)
{
    const u32 base = DSD(DS_00101514);
    if (base == 0u || base > MEM_SIZE - 0x2EEu) return;
    const u8 *scans = host_key_scans();
    if (scans == NULL) return;
    u8 *const tbl = mem + base + 0x254u;
    const u16 bits = host_key_bits();
    /* 0x1B850/0x1B870 read the fixed entries +0x28F/+0x290 (= +0x254+0x3B/0x3C). */
    host_key_state_store(tbl, 0x3Bu, (bits & 0x0100u) != 0u);
    host_key_state_store(tbl, 0x3Cu, (bits & 0x0001u) != 0u);
    for (u32 i = 0u; i < 16u; i++) {
        const u8 scan = scans[i];
        if (scan == 0u) continue;                     /* an unbound slot */
        int down = (bits & k_cfg_bit[i]) != 0u;
        /* One table entry can serve two readers: a slot bound to F1/F2 is the
         * entry 0x1B850/0x1B870 reads, so either source makes it. */
        if (scan == 0x3Bu) down |= (bits & 0x0100u) != 0u;
        if (scan == 0x3Cu) down |= (bits & 0x0001u) != 0u;
        host_key_state_store(tbl, scan, down);
    }
}

/* PORT: the game port's button byte (record §H3). The PC game port reads its
 * four button inputs in bits 4..7 of port 0x201, active low: a pressed button
 * grounds its bit. The host owns the port, so g_port_buttons is the host's
 * answer: host_pad_fill() (Task 5) writes the pad's byte each frame. The seam's
 * byte is kept apart, so pinning one never loses the pad's own (a fill writes
 * g_port_buttons and leaves the seam alone). With no pad the byte is 0 -- the
 * readers then see all four buttons low, consistent only with the device word
 * being cleared when no pad answers (0x4FBBB, Task 6). */
static u8 g_port_buttons;            /* the pad's byte (0 until Task 5's fill) */
static u8 g_port_buttons_override;   /* the seam's byte, read only while armed */
static int g_port_buttons_override_on;

void host_set_port_buttons_override(u8 buttons, int on)
{
    g_port_buttons_override = buttons;
    g_port_buttons_override_on = on;
}

u8 host_port_buttons(void)
{
    /* PORT: a pinned byte wins; else the pad's (0 with no pad, until Task 5's
     * host_pad_fill writes it). */
    if (g_port_buttons_override_on) return g_port_buttons_override;
    return g_port_buttons;
}

/* PORT: the four normalized axis words at rest (record 2026-10-21-host-input-
 * truth §H4). The raw's game-port routines 0x4FDF5/0x4FC05/0x4FCFD normalize
 * each timed axis and store it at 0xE1C1E/0xE1C20/0xE1C26/0xE1C28; the center
 * capture 0x5004A stores the rest values at 0xE1C22/0xE1C24/0xE1C2A/0xE1C2C.
 * With no pad every axis is at its center, so the ported readers 0x4FF8F/
 * 0x4FFD8 read the dead band. Task 5's host_pad_fill() writes the pad's
 * displacement over these words. */
void host_input_fill_axes(void)
{
    DSW(DS_000E1C1E) = DSW(DS_000E1C22);          /* P1 X: at its center */
    DSW(DS_000E1C20) = DSW(DS_000E1C24);          /* P1 Y */
    DSW(DS_000E1C26) = DSW(DS_000E1C2A);          /* P2 X */
    DSW(DS_000E1C28) = DSW(DS_000E1C2C);          /* P2 Y */
}

/* PORT: test seam, no raw counterpart (host.h): the four presence answers
 * below return this while it is armed, so the unit test can exercise the
 * sampler's device 2/4/6 arms with no pad attached. Off by default. */
static int g_port_present_override;      /* the seam's answer, read only while armed */
static int g_port_present_override_on;

void host_set_port_present_override(int present, int on)
{
    g_port_present_override = present;
    g_port_present_override_on = on;
}

/* PORT: the game port's presence (record §H4; the routines stay host-owned,
 * record §49-V). 0x4FDF5 polls all four axes, 0x4FC05 P1's two and 0x4FCFD
 * P2's; each returns nonzero when the port answered, and 0x4FBBB probes one
 * button mask (3 = game port 0's two buttons, 0xC = game port 1's). The
 * original times the PIT and reads port 0x201; the host owns that answer.
 * Task 4 answers "no port" (0) -- the no-pad state -- so the sampler clears a
 * joystick device word exactly as the original does when nothing answers; Task
 * 6 models the pad's answer and the boot probe. Each opens with its seam so the
 * sampler's differential row records the call (the case's stub answer is what
 * the port's sampler sees there). */
static int host_port_present(void)
{
    return g_port_present_override_on ? g_port_present_override : 0;   /* no pad: Task 6 */
}

int host_port_poll_both(void)
{
    PR_SEAM_RET0(0x4FDF5u);
    return host_port_present();
}

int host_port_poll_p1(void)
{
    PR_SEAM_RET0(0x4FC05u);
    return host_port_present();
}

int host_port_poll_p2(void)
{
    PR_SEAM_RET0(0x4FCFDu);
    return host_port_present();
}

int host_port_probe(u32 mask)
{
    PR_SEAM_RET(0x4FBBBu, mask);
    return host_port_present();
}

static u32 g_tick;
static uint64_t g_tick_base_ns;
static int g_tick_started;

static SDL_Window *g_window;
static SDL_Surface *g_scratch; /* RGB24 surface the game renders into */
static int g_sdl_video;
static int g_pending; /* a frame was submitted and awaits present */
static int g_w, g_h;
static int g_quit_requested; /* the user closed the window (SDL_EVENT_QUIT) */
static host_pump_hook_fn g_pump_hook; /* PORT: test seam (host.h); NULL in the game */
static void *g_pump_hook_ctx;

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
    /* The name-entry screen's DEL key (0x20860 maps ascii 8 to 0x1B). */
    case SDL_SCANCODE_BACKSPACE: *scan = 0x0E; *ascii = 0x08; return 1;
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
        /* The BIOS gives an Alt-letter as its scan code with ascii 0: 0x24C5C's
         * extended-key arms Alt-Q/S/J/M (record §55-A). */
        if ((k->mod & SDL_KMOD_ALT) != 0) return 1;
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
            /* PORT: the original has no window, so a close request is the host's
             * own quit signal and is recorded as one. Proxying it as an ESC key
             * failed: a movie drains the same queue and only aborts on ESC, so
             * the request was swallowed and the game ran on. */
            g_quit_requested = 1;
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
    /* SDL_GetWindowSurface is re-fetched every present: SDL3 invalidates the
     * surface on resize, so this is also the resize path. The 320x200 frame is
     * scaled to fill the window (nearest, to keep the pixels hard). */
    SDL_Rect dst = { 0, 0, win->w, win->h };
    SDL_BlitSurfaceScaled(g_scratch, NULL, win, &dst, SDL_SCALEMODE_NEAREST);
    SDL_UpdateWindowSurface(g_window);
}

int host_init(const char *title, int w, int h)
{
    if (g_sdl_video) return 1;
    if (w <= 0 || h <= 0) return 0;
    if (!SDL_Init(SDL_INIT_VIDEO)) return 0; /* no display: report, do not abort */
    g_sdl_video = 1;
    g_window = SDL_CreateWindow(title, HOST_WINDOW_W, HOST_WINDOW_H,
                                SDL_WINDOW_RESIZABLE);
    if (!g_window) { host_shutdown(); return 0; }
    g_scratch = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGB24);
    if (!g_scratch) { host_shutdown(); return 0; }
    g_w = w;
    g_h = h;
    return 1;
}

int host_has_window(void)
{
    /* g_window is NULL before host_init(), after host_shutdown(), and after a
     * failed host_init() (all failure paths call host_shutdown()). */
    return g_window != NULL;
}

void host_shutdown(void)
{
    host_audio_close(); /* one authoritative teardown, before SDL_Quit() */
    if (g_scratch) { SDL_DestroySurface(g_scratch); g_scratch = NULL; }
    if (g_window) { SDL_DestroyWindow(g_window); g_window = NULL; }
    if (g_sdl_video) { SDL_Quit(); g_sdl_video = 0; }
    g_pending = 0;
    g_w = g_h = 0;
    g_quit_requested = 0;
}

int host_quit_requested(void)
{
    return g_quit_requested;
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
    if (g_pump_hook != NULL) g_pump_hook(g_pump_hook_ctx);
}

void host_set_pump_hook(host_pump_hook_fn fn, void *ctx)
{
    g_pump_hook = fn;
    g_pump_hook_ctx = ctx;
}

static host_fault_hook_fn s_fault_hook;

host_fault_hook_fn host_set_fault_hook(host_fault_hook_fn hook)
{
    host_fault_hook_fn prev = s_fault_hook;
    s_fault_hook = hook;
    return prev;
}

_Noreturn void host_cpu_fault(u32 exc, u32 eip, const char *msg, int status)
{
    if (s_fault_hook != NULL) s_fault_hook(exc, eip);
    if (msg != NULL)
        fprintf(stderr, "%s\n", msg);
    else
        fprintf(stderr, "prageport: CPU exception %02Xh at %08X: the original "
                "leaves it to DOS/4GW's default handler, which ends the run\n",
                (unsigned)exc, (unsigned)eip);
    host_shutdown();
    exit(status);
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

static char s_save_path[1024];
static char s_save_fallback[1024];

/* A path that does not fit is no path, not another file. */
static void save_copy_path(char *dst, size_t max, const char *a, const char *b)
{
    if (!a || snprintf(dst, max, "%s%s", a, b) >= (int)max)
        dst[0] = '\0';
}

void host_save_set_path(const char *path)
{
    save_copy_path(s_save_path, sizeof s_save_path, path, "");
}

void host_save_set_fallback(const char *dir)
{
    save_copy_path(s_save_fallback, sizeof s_save_fallback, dir, "");
}

void host_save_set_default_path(void)
{
    char *pref = SDL_GetPrefPath(NULL, "primal-rage");   /* ends with a separator; NULL on failure */

    save_copy_path(s_save_path, sizeof s_save_path, pref, "cmos");
    SDL_free(pref);
}

/* The raw's fread(image, 1, 0x7F8, file) (0x1B017): a longer file gives its
 * first `len` bytes, a shorter one fails. */
static int save_read_exact(const char *path, u8 *dst, u32 len)
{
    FILE *f = fopen(path, "rb");
    size_t n;

    if (!f) return 0;
    n = fread(dst, 1, (size_t)len, f);
    fclose(f);
    return n == (size_t)len;
}

int host_save_read(u8 *dst, u32 len)
{
    char path[1100];
    FILE *f;

    if (s_save_path[0] && (f = fopen(s_save_path, "rb")) != NULL) {
        fclose(f);
        return save_read_exact(s_save_path, dst, len);
    }
    if (!s_save_fallback[0])
        return 0;
    snprintf(path, sizeof path, "%s/cmos", s_save_fallback);
    if (save_read_exact(path, dst, len))
        return 1;
    snprintf(path, sizeof path, "%s/CMOS", s_save_fallback);
    return save_read_exact(path, dst, len);
}

int host_save_writable(void)
{
    FILE *f;

    long end;

    if (!s_save_path[0] || (f = fopen(s_save_path, "ab")) == NULL)
        return 0;
    end = (fseek(f, 0, SEEK_END) == 0) ? ftell(f) : -1;
    fclose(f);
    if (end == 0) remove(s_save_path);   /* the probe leaves no empty save behind */
    return 1;
}

int host_save_write(const u8 *src, u32 len)
{
    return s_save_path[0] ? host_write_file(s_save_path, src, len) : 0;
}

/* Audio seam. This is the only place SDL's audio device exists; game-side audio
 * modules hand mixed stereo frames to host_audio_submit() and never see SDL.
 * The stream pulls from SDL's own queue (no callback), so host.c has no
 * reference to the mixer and every caller stays buildable and testable headless.
 *
 * PORT: fixed audio profile, no hardware probe. The seam asks SDL for the
 * default playback device at the caller's rate/channels; it never enumerates
 * devices or negotiates formats beyond what SDL needs to open.
 * PORT: the failure branches (NULL from SDL_OpenAudioDeviceStream, or a failed
 * SDL_ResumeAudioStreamDevice on a host with no audio device) are host
 * behaviour with no original counterpart (the original's device probe is the
 * AIL driver install, replaced by the fixed profile), so no raw or capture can
 * pin them; the suite never opens a real device and they stay unexercised
 * (record §2 of 2026-09-29-todo-verify-derivations.md). */
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
