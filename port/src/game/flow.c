/* Port of the top-level game flow. Original addresses are named in comments:
 *   0x1BEC4 game main, 0x20C10 init wrapper, 0x255CC master loop,
 *   0x24C5C per-frame update, 0x11D04 state machine, 0x51F45 surface setup,
 *   0x336C0 palette dirty-list init, 0x1BE30 teardown.
 * Scope (Task 14): the title-screen path is live. Every call owned by a later
 * sub-project is stubbed where it is reached and named in a PORT comment. */
#include "game/flow.h"
#include "mem.h"
#include "symbols.h"
#include "platform/res.h"
#include "platform/gra.h"
#include "platform/gfx.h"
#include "platform/input.h"
#include "platform/audio/ail.h"
#include "platform/audio/mixer.h"
#include "platform/audio/samples.h"
#include "platform/audio/sequencer.h"
#include "host.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- port-local scratch and constants ---------------------------------- */

/* PORT: the original keeps BIOS/real-mode state in segment memory addressed
 * through DAT_00101514 (= DPMI selector << 4) — keyboard shift flags at +0x2d8,
 * joystick config at +0x2d4. The port has no real-mode segment; it points that
 * base into flat mem[] above the resource heap (which ends near 0x2A8BFD7) and
 * leaves it zeroed, so those reads become defined host state, not live BIOS. */
#define GAME_BIOS_BASE   0x3000000u
#define GAME_BIOS_LEN    0x1000u
/* Packed palette words for the title's dirty-list record. */
#define GAME_PAL_SCRATCH (GAME_BIOS_BASE + 0x1000u)

/* s16title.gra is resource index 7 in the shipped INDEX. */
#define TITLE_RES 7u

/* s16sound.gra is resource index 5; the one located PCM sample (Task 1) lives
 * in it as a RIFF/WAVE blob. */
#define SOUND_RES 5u

/* The original title is a composite drawn through the process-table task
 * system (0x2AE14 spawns tasks; the sprite blitter fills DAT_000E87A4). PORT:
 * the port renders selected full-screen S16TITLE.GRA frames instead; the
 * logo/menu sprite overlay is deferred to the menus sub-project. The frame set
 * {10,12,13,18} is derived from the asset — S16TITLE has exactly four 320x200
 * descriptors — but rendering them full-screen is the port's choice, not the
 * original's composite. TITLE_HOLD_FRAMES is a port rate: the original advances
 * its animation through task timers. */
static const int TITLE_FRAMES[] = { 10, 12, 13, 18 };
#define TITLE_FRAME_COUNT ((int)(sizeof TITLE_FRAMES / sizeof TITLE_FRAMES[0]))
#define TITLE_HOLD_FRAMES 8   /* PORT: title-image rate (original: task timers) */

static const char *s_game_dir;

static GraChunk s_title_chunks[8];
static u32 s_title_off;
static int s_title_chunk_n;
static int s_title_ready;
static int s_title_idx;
static int s_title_hold;

/* Music pacing. The sequencer is driven by the host's 60 Hz tick clock (the
 * original's PIT ISR), one host tick = 2 XMIDI ticks (Task 8: 8.333 ms), and a
 * device frame wants MIXER_OPL_RATE/60 samples. game_audio_service derives both
 * the tick count and the sample count from the same measured host tick delta,
 * so a slow loop iteration cannot slow the music relative to wall time. The
 * catch-up is clamped to the host clock's own bound (HOST_TICK_MAX_CATCHUP —
 * the intervals host_pump() will replay before rebasing), not a second tuning
 * constant: a stall the host clock itself replays keeps the music's wall-clock
 * time, and past that bound both drop the lost time together. The clamp also
 * bounds the render, so one iteration can never submit an unbounded burst. */
#define GAME_AUDIO_TICKS_PER_HOST_TICK 2u
/* Largest service burst: one host tick is 49716/60 = 828.6 frames (828 or 829),
 * so a max-clamped service is HOST_TICK_MAX_CATCHUP of those. */
#define AUDIO_FRAMES_MAX (((MIXER_OPL_RATE + 59) / 60) * HOST_TICK_MAX_CATCHUP)

/* Audio state (see the audio section below). PORT: port-only bookkeeping — the
 * original keeps the sequence handle in DAT_001028c0 and the pending-song
 * handle in DAT_001028cc, both in mem[]; none of this is a mem[] offset. */
static HSEQUENCE s_sequence;
/* The four sample handles the original keeps at DAT_00102860 (spec audio.md
 * "AIL surface" row 10). The announcer is queued on slot 0. */
static HSAMPLE s_samples[4];
/* The announcer request: the original's FUN_0002c3fc case 2/3 resolves the
 * sample bytes and FUN_0001cc28 queues them; the master loop's 0x1CF20 -> the
 * per-slot FUN_0001cb18 then sets the handle up and calls AIL_start_sample.
 * PORT: the runtime sound table (DAT_000bbdc8) maps an id to a resource only at
 * runtime and is not extracted (see title_music_bank), so the port binds the one
 * located sample (S16SOUND.GRA, Task 1) directly. s_pending_sample.pcm borrows
 * the resource bytes in mem[]; they outlive the voice. */
static SampleVoice s_pending_sample;
static int s_sample_request;     /* a state asked for a sample; 0x1CF20 plays it */
static int s_music_request;      /* a state asked for music; 0x1CF20 starts it */
static u32 s_audio_ticks;        /* seq_tick() calls driven since start */
static u32 s_audio_frac;         /* sub-host-tick sample remainder, /60 */
static u32 s_last_host_tick;     /* host clock at the last service call */
static int s_music_notes;        /* sticky: a note has been keyed */
static s16 s_audio_buf[AUDIO_FRAMES_MAX * 2];

/* ---- small ported helpers ---------------------------------------------- */

/* PORT: 0x4FBA2 is a BIOS `int 10h` mode query. In the port the SDL host owns
 * the window and always reports the VGA 320x200 mode the game gates on. */
static u32 int10h_query(void) { return 0x13u; }

/* PORT: 0x1D290 prints an error and exits the process; the port does the same
 * rather than unwinding (the init chain has no clean recovery). */
static void game_fatal(const char *what)
{
    fprintf(stderr, "Primal Rage: fatal init error: %s\n", what);
    exit(1);
}

/* 0x336C0: resets the palette dirty-list head and marks every record unused. */
static void palette_list_init(void)
{
    DSD(DS_00107798) = DS_00107498;
    for (u32 i = 0; i < 0x180; i += 0x10) DSD(DS_0010749C + i) = 0xFFFFFFFFu;
    DSD(DS_000BD470) = 0;
    /* PORT: 0x336C0 then enqueues the initial palette via 0x33734. With no VGA
     * DAC to reset, the port just clears gfx_dac. */
    memset(gfx_dac, 0, sizeof gfx_dac);
}

/* 0x33734: appends a raw-pointer palette record { ptr; first; count; flag }. */
static void palette_record(u32 ptr, u32 first, u32 count, u32 flag)
{
    u32 head = DSD(DS_00107798);
    DSD(head + 0) = ptr;
    DSD(head + 4) = first;
    DSD(head + 8) = count;
    DSD(head + 12) = flag;
    DSD(DS_00107798) = head + 16;
}

/* 0x51F45: binds the two offscreen buffers and builds the 200-entry scanline
 * offset table (0, 0x140, 0x280, ...). */
static void surface_setup(void)
{
    DSD(DS_000E87A0) = DSD(DS_001014E4);
    DSD(DS_000E87A4) = DSD(DS_001014E8);
    for (u32 i = 0; i < 200; i++) DSD(DS_001088F8 + i * 4) = i * 0x140u;
}

/* 0x50188: swaps the front/back offscreen buffers. */
static void swap_buffers(void)
{
    u32 t = DSD(DS_000E87A0);
    DSD(DS_000E87A0) = DSD(DS_000E87A4);
    DSD(DS_000E87A4) = t;
}

/* The engine's extension seam: a 32-entry table of original code addresses
 * gated by a bitmask. Each live entry is resolved through the port's function
 * registration table and called. */
static void run_process_table(u32 table, u32 mask)
{
    for (int i = 0; mask != 0; i++, mask >>= 1) {
        if (!(mask & 1)) continue;
        void (*fn)(void) = fn_resolve(DSD(table + (u32)i * 4));
        if (fn) fn();
    }
}

/* PORT: 0x500C4 samples the BIOS shift flags at DAT_00101514+0x2d8. The port
 * has no real-mode BIOS; keyboard input arrives through host_pump() ->
 * input_push(). */
static void input_pump(void) { host_pump(); }

/* ---- title state (state 1, 0x121A0) ------------------------------------ */

static void title_load(void)
{
    void *base = res_resolve(res_handle(TITLE_RES, 0));
    if (!base) { DSB(DS_000A81A8) = 1; return; }
    u32 off = (u32)((const u8 *)base - mem);
    int n = 0;
    if (!gra_open(off, res_size(TITLE_RES), s_title_chunks, 8, &n)) {
        DSB(DS_000A81A8) = 1;
        return;
    }

    /* PORT: the whole type-5 palette bank is flattened; the original selects a
     * sub-palette per sprite. Only the first 256 entries are pushed. */
    static u8 pal[256 * 3 * 4];
    int pc = 0;
    if (!gra_decode_palette(off, s_title_chunks, n, pal, sizeof pal, &pc)) {
        DSB(DS_000A81A8) = 1;
        return;
    }
    int use = pc < 256 ? pc : 256;
    for (int i = 0; i < use; i++)
        DSD(GAME_PAL_SCRATCH + (u32)i * 4) =
            ((u32)pal[i * 3 + 0] << 2) | ((u32)pal[i * 3 + 1] << 10) |
            ((u32)pal[i * 3 + 2] << 18);
    palette_record(GAME_PAL_SCRATCH, 0, (u32)use, 0);

    s_title_off = off;
    s_title_chunk_n = n;
    s_title_idx = 0;
    s_title_hold = 0;
    s_title_ready = 1;
}

/* FUN_0002c3fc (case 2/3) -> FUN_0001cc28 at the title state's first entry:
 * resolves the announcer's bytes and queues them. The resource is scanned for
 * the RIFF/WAVE blob (the same shape samples_load parses) rather than trusting a
 * fixed offset; samples_load bounds every chunk against the bytes it is handed,
 * so a truncated or corrupt blob is rejected instead of over-read. */
static void game_sample_request(void)
{
    const u8 *base = (const u8 *)res_resolve(res_handle(SOUND_RES, 0));
    if (base == NULL) return;
    u32 size = res_size(SOUND_RES);
    for (u32 i = 0; i + 12 <= size; i++) {
        if (memcmp(base + i, "RIFF", 4) != 0) continue;
        if (memcmp(base + i + 8, "WAVE", 4) != 0) continue;
        if (samples_load(base + i, size - i, &s_pending_sample)) {
            s_sample_request = 1;
            return;
        }
    }
}

static void game_state_title(void)
{
    if (!s_title_ready) {
        title_load();
        if (!s_title_ready) return;
        /* 0x121a0 (state 1's first entry) calls FUN_0002c3fc(0x41)/(0x43); its
         * case 1 requests the title music, which the master loop's 0x1CF20 then
         * loads and starts — the request is made here, started by the frame
         * path, not on a port-side timer. PORT: the port requests the S16TITLE
         * bank directly instead of the runtime sound table's handle. */
        s_music_request = 1;
        /* 0x121A0's first entry also calls FUN_0002C3FC(0x41)/(0x43); the port
         * queues the located announcer sample here and lets the master loop's
         * 0x1CF20 play it (the original's request/play split, not collapsed). */
        game_sample_request();
    }
    /* Redraw the current image into the draw buffer every frame, matching the
     * original: 0x255CC swaps buffers every presented tick, so a buffer that is
     * not redrawn this frame is presented blank on the next. TITLE_HOLD_FRAMES
     * only slows which image is current; it must never skip the redraw. */
    u8 *dst = mem + DSD(DS_000E87A4);
    int consumed = gra_decode_frame(s_title_off, s_title_chunks, s_title_chunk_n,
                                    TITLE_FRAMES[s_title_idx], dst, 320u * 200u);
    if (consumed < 0) return;   /* keep the previous image */
    if (++s_title_hold >= TITLE_HOLD_FRAMES) {
        s_title_hold = 0;
        s_title_idx = (s_title_idx + 1) % TITLE_FRAME_COUNT;
    }
    DSD(DS_001014FC) = 1;       /* signals the full-screen copy in game_loop */
}

/* 0x10E80: initialise the game state. */
static void game_state_init(void)
{
    /* The original enters state 0 (the 0x11000 attract sub-machine), which then
     * transitions to title state 1. PORT: the attract sub-machine is deferred,
     * so the port enters state 1 directly. The index is a chosen, likely value
     * from static evidence (see port/spec/game_flow.md "Title state"), not a
     * runtime reading — no scriptable DOSBox-X debugger was available.
     * 0x24C5C drives 0x11D04 only in case 3 of switch(DAT_00104B00), so the
     * port selects that mode; the original derives the value in 0x10E80's
     * register handoff. */
    DSD(DS_00104B00) = 3;
    DSW(DS_000F0A64) = 1;
    DSB(DS_000F0A71) = 0;
    DSB(DS_000F0A5C) = 4;
    DSB(DS_000F0A6F) = 0;
}

/* ---- audio: the init chain's AIL calls and the frame-loop music service --- */

/* 0x1CF40: installs the AIL profile and allocates the game's audio handles.
 * PORT: fixed audio profile, no hardware probe (ail.c). The four sample handles
 * and the single sequence handle live in ail.c; only the sequence is kept here
 * because the title state needs it to start music. The 60 Hz timer is registered
 * and started as in the original, but its callback is never fired: the port has
 * no PIT/ISR and the frame loop owns pacing (see game_audio_service). */
void game_audio_init(void)
{
    /* TODO(verify): the original's FUN_0001cf40 gates the DIG install and its
     * preferences behind param_2 and the MDI install behind param_1
     * (prage.c:8703,8719); the port installs both unconditionally. The shipped
     * init calls it with both nonzero, so the shipped behaviour is equal. */
    mixer_reset();
    AIL_startup();
    DSB(DS_000A2CB1) = 1;
    /* PORT: the original's master SFX volume (DAT_000a2cb4) is an EEPROM/options
     * value owned by sub-project 4, and game_audio_init already carries the
     * shipped enable flag above. The shipped EXE data segment holds 0x7f (full)
     * at this address, so the port installs that default rather than playing the
     * announcer at the zeroed mem[] value. */
    DSD(DS_000A2CB4) = 0x7f;
    AIL_set_preference(4, 4);
    AIL_set_preference(1, 0x2b11);  /* 11025 Hz sample rate */
    AIL_set_preference(3, 0x14);
    HDIGDRIVER dig = AIL_install_DIG_INI();
    if (dig != NULL) {
        for (int i = 0; i < 4; i++) {
            s_samples[i] = AIL_allocate_sample_handle(dig);
            if (s_samples[i] != NULL) AIL_init_sample(s_samples[i]);
        }
    }
    AIL_set_preference(0xb, 1);
    HMDIDRIVER mdi = AIL_install_MDI_INI();
    if (mdi != NULL) s_sequence = AIL_allocate_sequence_handle(mdi);
    HTIMER timer = AIL_register_timer(NULL);
    AIL_set_timer_frequency(timer, 0x3c);   /* the original's 60 Hz game tick */
    AIL_start_timer(timer);
    s_last_host_tick = host_tick_count();
}

/* Locates the title music bank in S16TITLE.GRA through the resource layer: the
 * first FORM/XMID container in the resource, the same scan tools/opl_seq.py and
 * seq_load use. The original passes a runtime sound-table handle (the title
 * 0x121a0 calls FUN_0002c3fc(0x41), whose case 1 requests it); Task 9's capture
 * spans this S16TITLE bank end to end.
 * TODO(verify): the sound-table id -> resource handle mapping is not extracted
 * (DAT_000bbdc8 is zero in PRAGE.EXE and populated at runtime), so the port
 * binds the title state to the bank directly. Returns NULL on a bank whose
 * declared FORM size runs past the loaded resource. */
static const u8 *title_music_bank(void)
{
    const u8 *base = (const u8 *)res_resolve(res_handle(TITLE_RES, 0));
    if (base == NULL) return NULL;
    return game_music_bank_find(base, res_size(TITLE_RES));
}

/* Scans `base`/`size` for the first FORM/XMID container (the same scan
 * tools/opl_seq.py and seq_load use) and validates its declared FORM size
 * against the range. Exposed for a unit test: this check is the only guard
 * against a corrupt bank, because seq_load is handed a length derived from the
 * same declared size (seq_bank_size), making its own bound tautological.
 * Returns NULL when the container is absent or its size runs past the range. */
const u8 *game_music_bank_find(const u8 *base, u32 size)
{
    if (base == NULL || size < 12) return NULL;
    for (u32 i = 0; i + 12 <= size; i++) {
        if (memcmp(base + i, "FORM", 4) != 0) continue;
        if (memcmp(base + i + 8, "XMID", 4) != 0) continue;
        u32 fsz = ((u32)base[i + 4] << 24) | ((u32)base[i + 5] << 16) |
                  ((u32)base[i + 6] << 8) | (u32)base[i + 7];
        /* Task 10 carry-forward: seq_bank_size() trusts this declared size, so
         * reject a bank that would run past the loaded resource before seq_load
         * can read it — a corrupt asset must never cause an over-read. Compare
         * against the remaining bytes, never `i + 8 + fsz`: that u32 sum wraps
         * for a crafted FORM placed at i >= 248 with fsz near 0xFFFFFF00.
         * `i + 12 <= size` makes `size - (i + 8)` underflow-free. */
        if (fsz < 4 || fsz > size - (i + 8u)) return NULL;
        return base + i;
    }
    return NULL;
}

/* 0x1C930 (reached from 0x1CF20): loads and starts the pending song. */
static void title_music_start(void)
{
    const u8 *bank = title_music_bank();
    if (bank == NULL || s_sequence == NULL) return;
    if (!AIL_init_sequence(s_sequence, bank, 0)) return;
    AIL_set_sequence_volume(s_sequence, (s32)DSD(DS_000A2CB8), 500);
    AIL_start_sequence(s_sequence);
}

/* 0x1CF20: the master loop's per-frame audio service (0x255CC). Starts the
 * pending music, advances the sequencer, then renders and submits one frame of
 * mixed stereo audio. PORT: no PIT/ISR — the music tick is driven here from the
 * host's measured 60 Hz tick delta. Task 8 measured one XMIDI tick = 8.333 ms
 * (120 Hz) for the shipped profile, so two sequencer ticks per host tick; both
 * the tick count and the sample count derive from that one delta, and a stalled
 * frame is clamped to the host clock's own catch-up bound, so the music tracks
 * wall time without bursting. With no
 * device (host_audio_rate() == 0, e.g. --check) the sequencer still advances
 * but nothing is rendered or submitted. */
/* FUN_0001cb18 (reached from 0x1CF20): sets a queued sample up on its handle and
 * starts it, in the original's call order. The port has the one announcer slot
 * rather than the original's per-slot loop over four. */
static void game_sample_play(void)
{
    HSAMPLE h = s_samples[0];
    if (h == NULL || s_pending_sample.pcm == NULL) return;
    AIL_init_sample(h);
    AIL_set_sample_address(h, s_pending_sample.pcm, s_pending_sample.frames);
    AIL_set_sample_volume(h, (s32)DSD(DS_000A2CB4));
    AIL_set_sample_rate(h, s_pending_sample.rate);
    AIL_set_sample_type(h, 0, 0);
    AIL_set_sample_loop_count(h, 0);   /* the original forces 0 = one-shot */
    AIL_start_sample(h);
}

void game_audio_service(void)
{
    /* 0x1CF20 plays queued samples before it starts the pending song. */
    if (s_sample_request) {
        s_sample_request = 0;
        game_sample_play();
    }
    if (s_music_request) {
        s_music_request = 0;
        title_music_start();
    }

    /* Both the sequencer tick count and the audio frame count come from the
     * same measured host tick delta, so they stay matched and a slow iteration
     * cannot change the tempo. A stall is clamped to the host clock's own
     * catch-up bound, so time the host clock drops is dropped here too and a
     * stall it replays keeps the music's wall-clock time. */
    u32 now = host_tick_count();
    u32 elapsed = now - s_last_host_tick;
    s_last_host_tick = now;
    if (elapsed > HOST_TICK_MAX_CATCHUP) elapsed = HOST_TICK_MAX_CATCHUP;

    u32 ticks = elapsed * GAME_AUDIO_TICKS_PER_HOST_TICK;
    for (u32 i = 0; i < ticks; i++) seq_tick();
    s_audio_ticks += ticks;
    if (seq_active_track() > 0) s_music_notes = 1;

    u32 rate = host_audio_rate();
    if (rate == 0) return;
    s_audio_frac += rate * elapsed;     /* samples due, scaled by 60 */
    u32 n = s_audio_frac / 60u;
    s_audio_frac %= 60u;
    if (n > AUDIO_FRAMES_MAX) n = AUDIO_FRAMES_MAX;
    if (n == 0) return;
    mixer_render(s_audio_buf, n, rate);
    host_audio_submit(s_audio_buf, (int)n);
}

u32 game_audio_ticks(void) { return s_audio_ticks; }
int game_music_notes_seen(void) { return s_music_notes; }

/* ---- the exported flow -------------------------------------------------- */

void game_set_game_dir(const char *dir) { s_game_dir = dir; }

void game_init(void)
{
    char index_path[512];
    snprintf(index_path, sizeof index_path, "%s/INDEX", s_game_dir);

    /* 0x1BEC4 init chain, in order. */
    /* PORT: the argc==2 argv probe (0x623B0, "-f") has no host equivalent. */
    DSD(DS_00101504) = int10h_query();   /* 0x4FBA2 */
    DSD(DS_00101510) = 1;                /* PORT: 0x1ACA8 memory detect -> ok */
    DSD(DS_00101514) = GAME_BIOS_BASE;   /* PORT: selector<<4 -> flat scratch */
    mem_fill(GAME_BIOS_BASE, 0, GAME_BIOS_LEN);
    DSD(DS_000A2CAC) = DSD(DS_00101514);
    /* PORT: no DPMI — the 0x109A0 region locks are no-ops. */
    /* PORT: 0x1B3AC resource-file setup is replaced by res_load_index(). */
    game_audio_init();      /* 0x1CF40: AIL_startup, prefs, handles, timer */
    /* PORT: extended-memory block list (0x1E2A0/0x1C0F0) unused under flat mem[]. */
    /* PORT: 0x4FB98 (int 10h set mode) — the SDL host owns the window. */

    if (int10h_query() != 0x13) { game_fatal("no VGA 320x200 mode"); return; }

    /* PORT: DPMI locks 0x10C30/0x10D34/0x1ADAC/0x1ADE4/0x10D0C are no-ops. */
    if (res_load_index(s_game_dir, index_path) <= 0) {
        game_fatal("resource INDEX load failed");
        return;
    }
    surface_setup();        /* 0x51F45 */
    palette_list_init();    /* 0x336C0 */
    /* PORT: 0x5004A joystick init — the port reads int 16h keyboard only. */
    /* PORT: 0x1D0BC allocates the MIDI sequence buffer and the four sample
     * buffers. The port references the XMIDI bank's resource bytes directly
     * (sequencer.c) and samples.c allocates each handle's conversion buffer on
     * AIL_start_sample, so no init-time work buffers are needed. */
    /* PORT: 0x47370 EEPROM read (menus/EEPROM, sub-project 4). */

    game_state_init();      /* 0x20C10's FUN_00010E80 */
}

/* 0x1BE30 teardown. */
void game_shutdown(void)
{
    /* 0x1D018 clears its enable flag, stops the sequence and the sample
     * voices, then calls AIL_shutdown. AIL_shutdown releases the sample and
     * sequence handles itself, so the explicit stop mirrors the original's
     * ordering; clearing DS_000A2CB1 makes game_audio_init() idempotent. */
    if (DSB(DS_000A2CB1)) {
        DSB(DS_000A2CB1) = 0;
        AIL_stop_sequence(s_sequence);
        AIL_shutdown();     /* 0x5d86a */
    }
    /* PORT: 0x1B084 (resource free) and the memory frees are no-ops under flat mem[]. */
}

int game_main(void)
{
    if (!s_game_dir) {
        fprintf(stderr, "game_main: no game dir set\n");
        return 1;
    }
    game_init();            /* 0x1BEC4 init chain */
    game_loop();            /* 0x20C10 -> 0x255CC */
    game_shutdown();        /* 0x1BE30 teardown */
    return 0;
}

void game_loop(void)
{
    DSD(DS_00101508) = 0;
    DSD(DS_0010150C) = 0;
    do {
        input_pump();                        /* 0x500C4 */
        /* PORT: 0x292AC and 0x389C4/0x38A38 (DS_00107A54 != 0) deferred
         * (menus / fight engine). */
        game_frame();                        /* 0x24C5C */
        run_process_table(DS_000A86C4, DSD(DS_00104AEC));  /* render table */
        DSD(DS_00104AF4)++;
        /* PORT: 0x134C0 deferred (scene/narrative). */

        /* The original copies DAT_000E87A4 to the literal VGA aperture 0xA0000
         * here (0x255CC full copy when DS_001014FC != 0, else the 0x501A3
         * dirty-dword blit). PORT: the aperture rule — never write mem[0xA0000];
         * present the index buffer through gfx_present(), which converts via
         * gfx_dac to RGB and hands it to the host. */
        gfx_flush_palette();                 /* 0x1C470 */
        gfx_present(mem + DSD(DS_000E87A4), 320, 200);
        DSD(DS_001014FC) = 0;
        swap_buffers();                      /* 0x50188 */

        /* 0x255CC's tail calls 0x1CF20 here: play pending samples, start/drive
         * the music, render one frame of audio. */
        game_audio_service();

        /* PORT: the original paces on the tick counter pair DS_00101508/150C;
         * the port waits one 60 Hz host retrace. */
        host_wait_vblank();

        if (input_check_key() == 0x011B) {   /* ESC: scan 0x01, ASCII 0x1B */
            DSB(DS_000A81A8) = 1;
            input_clear();
        }
    } while (DSB(DS_000A81A8) == 0);
}

void game_frame(void)
{
    /* PORT: 0x24C5C calls 0x4F644 (unless DAT_00104B00 == 0x27); it is a
     * per-mode input/wait helper owned by no ported sub-project yet. */
    /* PORT: the two 0x94-byte player records at DS_001077E0 and 0x24C5C's
     * int 16h input loop belong to the fight engine (sub-project 5). */
    DSD(DS_000EF6DC)++;                                /* frame counter */
    run_process_table(DS_000A8644, DSD(DS_00104AE8));  /* update table */
    /* PORT: 0x24C5C's second 0x38990 per-frame service call is deferred. */

    /* The original reaches the state machine 0x11D04 only in case 3 of
     * switch(DAT_00104B00) (0x24C5C). The other modes (login/attract/fight and
     * diagnostics) are deferred to sub-projects 4/5. */
    switch (DSD(DS_00104B00)) {
    case 3:
        game_state_step();                             /* 0x11D04 */
        break;
    default:
        /* PORT: 0x24C5C's case 1/2/4..0x33 modes drive menus, attract, fight
         * and diagnostics; deferred to sub-projects 4/5. */
        break;
    }
}

void game_state_step(void)
{
    if (DSB(DS_00104B1D) == 0) {
        /* PORT: 0x11F28 menu-input poll (menus, sub-project 4). */
    }

    if (DSW(DS_000F0A64) < 10) {
        s16 sVar1 = (s16)(DSW(DS_000F0A6A) - 1);
        switch (DSW(DS_000F0A64)) {
        case 1:
            game_state_title();   /* ported title/attract screen */
            break;
        case 2:
        case 3:
        case 4:
            /* PORT: menus / character-select (sub-project 4). */
            break;
        case 5:
            /* PORT: match-start setup then state 6 (fight engine, sub-project 5). */
            break;
        case 6:
        case 7:
        case 8:
            /* PORT: fight engine (sub-project 5). */
            break;
        case 9:
            DSW(DS_000F0A6A) = (u16)sVar1;
            if (sVar1 == 0) {
                DSW(DS_000F0A64) = DSW(DS_000F0A6C);
                /* PORT: 0x10EE4/0x29D60 transition helpers (menus). */
            }
            break;
        default:
            break;
        }
    } else {
        /* PORT: 0x11000 attract sub-machine (state 0 / >=10), deferred. */
    }
    /* PORT: the trailing 0x10DB0/0x10E18/0x2BF08 present+transition helpers
     * (menus) are deferred. */
}
