/* Port of the top-level game flow. Original addresses are named in comments:
 *   0x1BEC4 game main, 0x20C10 init wrapper, 0x255CC master loop,
 *   0x24C5C per-frame update, 0x11D04 state machine, 0x51F45 surface setup,
 *   0x336C0 palette dirty-list init, 0x1BE30 teardown.
 * Scope (Task 14): the title-screen path is live. Every call owned by a later
 * sub-project is stubbed where it is reached and named in a PORT comment. */
#include "game/flow.h"
#include "game/actors.h"
#include "game/attract.h"
#include "game/camera.h"
#include "game/config.h"
#include "game/effects.h"
#include "game/fight.h"
#include "game/fighter.h"
#include "game/rng.h"
#include "mem.h"
#include "symbols.h"
#include "platform/res.h"
#include "platform/render.h"
#include "platform/gfx.h"
#include "platform/input.h"
#include "platform/audio/ail.h"
#include "platform/audio/mixer.h"
#include "platform/audio/patches.h"
#include "platform/audio/samples.h"
#include "platform/audio/sequencer.h"
#include "host.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* ---- port-local scratch and constants ---------------------------------- */

/* PORT: the original keeps BIOS/real-mode state in segment memory addressed
 * through DAT_00101514 (= DPMI selector << 4) — keyboard shift flags at +0x2d8,
 * joystick config at +0x2d4. The port has no real-mode segment; it points that
 * base into flat mem[] above the resource heap (which ends near 0x2A8BFD7) and
 * leaves it zeroed, so those reads become defined host state, not live BIOS. */
#define GAME_BIOS_BASE   0x3000000u
#define GAME_BIOS_LEN    0x1000u

/* s16title.gra is resource index 7 in the shipped INDEX. */
#define TITLE_RES 7u

/* s16sound.gra is resource index 5; the one located PCM sample (Task 1) lives
 * in it as a RIFF/WAVE blob. */
#define SOUND_RES 5u

/* PORT: scratch for the localisation table (0x47370's 0x1C308 block). It must
 * sit above the resource heap AND game_state_init's later movie loads (TWI5.SMK
 * is 1.2 MB, allocated by res_load_file after this loader, pushing the heap to
 * ~0x2BC0000), so it lives near the top of mem[]: 0x20 bytes for the 0x1E75C
 * handle and 0x2000 for the file (ENGLISH.TXT is 6953 bytes shipped). The
 * original keeps the handle in DAT_001082DC and the size in DAT_001082D8. */
#define STRING_HANDLE 0x3800000u
#define STRING_DATA   0x3800020u
#define STRING_CAP    0x2000u

static const char *s_game_dir;
static int s_string_table_loaded;

/* PORT: Task 10's dump hook bookkeeping. s_title_dump_n < 0 until 0x121A0's
 * entry frame arms it; each presented title frame then writes one
 * frame_%04d.raw. s_attract_dump_n counts the state-0 4d attract frames from
 * the first presented frame. */
static int s_title_dump_n = -1;
static int s_attract_dump_n = 0;

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

/* PORT: 0x33734 palette_record now lives in platform/gfx.c (gfx.h), the
 * dirty-list owner; this file's init enqueue below calls the shared definition. */

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

/* PORT: 0x500C4 is now the real sampler in platform/input.c (declared through
 * the platform/input.h include above). The old placeholder here only called
 * host_pump(); the SDL event pump still runs once per frame because
 * host_wait_vblank() (the loop tail below) invokes host_pump(), so the
 * event-pump half 0x500C4 never had is unaffected. */

/* ---- title state (state 1, 0x121A0) ------------------------------------ */

/* PORT: 0x4F1E4. Two 0x2EA30 interrupt-lock calls bracket the write; 0x2EA30
 * is inert in the port's single-threaded loop (actors_reset documents the
 * same). Exported so attract.c's 0x11000 phase 0/2 and 0x10EE4 reuse the one
 * body instead of inlining the byte store. */
void frontend_input_reset(void)
{
    DSB(DS_00104B15) = 0;                       /* 0x4F1F1 */
}

/* 0x4F1D0. `xor edx,edx; mov word [0x87A3A],dx; mov word [0x87A38],dx`. The
 * origin zeroing only; 0x4F1E4 (title_input_reset) is a different function. */
void frontend_origin_zero(void)
{
    DSW(DS_00107A3A) = 0;                       /* 0x4F1D3 */
    DSW(DS_00107A38) = 0;                       /* 0x4F1DA */
}

/* PORT: 0x38910. Called with eax = 0 from 0x121F9. 0x4F1D0 zeroes the two
 * cursor words; the mode-1 cursor words copy DS_00107A4E; the two table words
 * come from the data object's fixed-up tables DS_000BDE0C / DS_000BDDFC. */
static void title_origin_reset(u32 idx)
{
    DSW(DS_00107A3A) = 0;                       /* 0x4F1D3 (0x4F1D0) */
    DSW(DS_00107A38) = 0;                       /* 0x4F1DA (0x4F1D0) */
    DSW(DS_00107A4A) = DSW(DS_00107A4E);        /* 0x38925 */
    DSW(DS_00107A4C) = DSW(DS_00107A4E);        /* 0x3892B */
    DSW(DS_00107A50) = DSW(DS_000BDE0C + idx * 2u);   /* 0x38938 */
    DSW(DS_00107A40) = DSW(DS_000BDDFC + idx * 2u);   /* 0x38948 */
    DSW(DS_00107A3A) = (u16)(DSW(DS_00107A50) >> 5);  /* 0x3895C */
    DSW(DS_00107A38) = (u16)(DSW(DS_00107A48) >> 6);  /* 0x3897D */
}

/* PORT: 0x1E75C. Lock the paged data handle and return its data offset, or 0
 * when it is locked or empty. The handle is {base @+8; len @+0xc; flags @+0x15}
 * (0x1E6D8/0x1E75C/0x1E808's block header); the port builds it in mem[] and the
 * "lock" is an inert single-threaded flag. */
static u32 string_lock(u32 handle)
{
    if ((DSB(handle + 0x15) & 1u) == 0 && DSD(handle + 0xc) != 0) {
        DSB(handle + 0x15) |= 2u;
        return DSD(handle + 8);
    }
    return 0;
}

/* PORT: 0x1E808. Clear the lock bit. Its second argument feeds 0x500BB (a DPMI
 * page-map query) and a write to [arg+0x10] that the string reader never reads;
 * the port omits both, which is the arm 0x474E4 reaches. */
static void string_unlock(u32 handle) { DSB(handle + 0x15) &= 0xFDu; }

/* PORT: 0x474E4. Decode string `id` from the localisation table at `base` into
 * `out` (capacity `outlen`). The table's +4 holds a linked list of group offsets
 * relative to `base`; each group is a run of one-byte-length-prefixed entries
 * and an entry's bytes are XORed with its plaintext length byte. Returns the
 * decoded length (0 for an empty entry) or `outlen` when truncated. */
static u32 string_decode(u32 base, u32 id, u8 *out, u32 outlen)
{
    u32 off = 0;
    for (u32 g = id / 0x40u; g != 0; g--)
        off = DSD(base + off + 4u);             /* 0x4752F */
    u32 p = base + off + 8u;                    /* 0x47535 */
    for (u32 i = id % 0x40u; i != 0; i--)
        p += (u32)DSB(p) + 1u;                  /* 0x47544 */
    u32 len = DSB(p);                           /* 0x4754D */
    p += 1u;
    if (len < outlen) {
        for (u32 i = 0; i < len; i++)
            out[i] = (u8)(DSB(p + i) ^ (u8)len);   /* 0x47564 */
        out[len] = 0;                              /* 0x47572 */
        return len != 0 ? len + 1u : 0u;
    }
    for (u32 i = 0; i + 1u < outlen; i++)
        out[i] = (u8)(DSB(p + i) ^ (u8)len);       /* 0x47591 */
    out[outlen - 1u] = 0;                          /* 0x4759E */
    return outlen;
}

/* PORT: 0x47370. The original loads the loaded-config language file (index 0 =
 * english.txt, selected by the DS_00104528 byte 0x20C10 extracts) through the
 * 0x1C308/0x1E6D8 paged-memory manager and the 0x61xxx DOS file I/O, neither of
 * which the port has. The port replaces both with a direct stdio read into
 * STRING_DATA and a small handle at STRING_HANDLE, which is what 0x1E75C/
 * 0x1E808/0x474E4 operate on. Idempotent; a missing file leaves the table
 * empty (0x1C500 then returns ""). */
void game_string_table_load(const char *dir)
{
    if (s_string_table_loaded) return;
    s_string_table_loaded = 1;
    if (dir == NULL) return;
    char path[512];
    snprintf(path, sizeof path, "%s/ENGLISH.TXT", dir);
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        snprintf(path, sizeof path, "%s/english.txt", dir);
        f = fopen(path, "rb");
    }
    if (f == NULL) return;
    size_t n = fread(mem + STRING_DATA, 1, STRING_CAP, f);
    fclose(f);
    DSD(STRING_HANDLE + 8) = STRING_DATA;       /* base */
    DSD(STRING_HANDLE + 0xc) = (u32)n;          /* len */
    DSB(STRING_HANDLE + 0x15) = 0;              /* flags */
    DSD(DS_001082DC) = STRING_HANDLE;           /* 0x4738E */
    DSD(DS_001082D8) = (u32)n;                  /* 0x473A3 */
}

/* PORT: 0x1C500 + 0x474E4. The original's EAX = string id, EDX = DS_00102760,
 * EBX = 0x100; 0x1C500 zeroes the first byte when 0x474E4 reports no string. */
const u8 *game_string_get(u32 id)
{
    u32 base = string_lock(DSD(DS_001082DC));
    if (base != 0) {
        string_decode(base, id, mem + DS_00102760, 0x100u);
        string_unlock(DSD(DS_001082DC));
    } else {
        DSB(DS_00102760) = 0;                   /* 0x1C517 */
    }
    return (const u8 *)(mem + DS_00102760);
}

/* 0x1C500(0x15) -> 0x2F198: the title's caption. */
static const u8 *title_string(void) { return game_string_get(0x15u); }

/* PORT: 0x38B18 spawns `desc` into the first free slot of the 7-entry table at
 * DS_00107A1C as actor_spawn(desc, a2 << 3, 2, a3 << 3, 0); the argument
 * binding is pinned by disassembly (docs/superpowers/plans/2026-09-17-actor-system-args.md §2).
 * The original writes at index -1 when the table is already full; the port
 * skips instead of touching the word before the table. Shared by the title
 * state, the 0x11F6C selector and the 0x11000 attract machine. */
void frontend_spawn_row(const u32 *desc, u32 a2, u32 a3)
{
    int slot = 0;
    while (slot < 7 && DSD(DS_00107A1C + (u32)slot * 4u) != 0) slot++;
    if (slot >= 7) return;
    DSD(DS_00107A1C + (u32)slot * 4u) =
        actor_spawn(desc, a2 << 3, 2u, a3 << 3, 0u);   /* 0x38B5D */
}

/* PORT: 0x33904. Iterate the fixed 0x10-stride table at
 * DS_00107608..DS_00107798 (the raw immediates 0x87608/0x87798 are
 * DS-relative), returning the first entry whose +4 dword is non-zero, or 0 at
 * the end. Exposed for a unit test. */
u32 frontend_list_next(u32 node)
{
    u32 e = node ? node : DS_00107608;
    for (;;) {
        e += 0x10u;
        if (e >= DS_00107798) return 0;
        if (DSD(e + 4) != 0) return e;
    }
}

/* 0x1C6D4: membership test. The raw dereferences `rec` (`mov eax,[eax]`) then
 * accepts exactly the nine resource addresses its cmp/jb/jbe tree selects and
 * rejects everything else. Exposed for a unit test. */
u32 frontend_resource_known(u32 rec)
{
    u32 v = DSD(rec);
    switch (v) {
    case 0x80995Cu: case 0x80997Cu: case 0x809984u: case 0x80998Cu:
    case 0x809994u: case 0x80999Cu: case 0x8099A4u: case 0x8099ACu:
    case 0x8099CCu:
        return 1u;
    default:
        return 0u;
    }
}

/* 0x11F6C: the six-entry selector. Phase 0 draws the first entry then falls
 * into phase 1 (no jump between 0x11FD4 and 0x11FDA); phase 1 draws an entry;
 * phase 4 pauses on DS_000F0A68; phase 2 advances the entry and leaves for
 * state 3 past the sixth; phase 3 hands off. The jump table at 0x11F58 fixes
 * the boundaries: 0 -> 0x11F89, 1 -> 0x11FDA, 2 -> 0x1211C, 3 -> 0x12159,
 * 4 -> 0x1217B, default (ja) -> 0x1219A. */
static void game_state_select(void)
{
    /* 0x487BC is a link-time offset below the data object's base; the loader
     * fixes it to mem+0xC87BC. Config row 1, phase 0 and phase 1. */
    const u32 *desc = (const u32 *)(mem + 0xC87BCu);

    switch (DSB(DS_000F0A6F)) {
    case 0:
    case 1: {
        u32 n = DSB(DS_000F0A6E);
        if (DSB(DS_000F0A6F) == 0) {
            /* 0x29D60 is a ret-only no-op. */
            actors_reset();                         /* 0x2BAF4 (eax = 1) */
            frontend_origin_zero();                 /* 0x4F1D0 */
            frontend_spawn_row(desc, 0u, 0u);       /* 0x38B18 */
            config_set_credit_row(1u);              /* 0x2C06C (eax = 1) */
            DSB(DS_000F0A6E) = 0;
            DSD(DS_000F0A44) = 0;
            if ((DSB(DS_00104528 + 1) & 2u) != 0u) DSD(DS_000F0A40) = 0;
            n = 0;
        }
        actors_reset();                             /* 0x2BAF4 (eax = 1) */
        frontend_origin_zero();                     /* 0x4F1D0 */
        frontend_spawn_row(desc, 0u, 0u);           /* 0x38B18 */
        DSD(DS_000F0A44) = actor_spawn(             /* 0x2AE14 */
            (const u32 *)(mem + DSD(0x9AEE0u + 12u * n)),   /* 0x9AEE0 */
            0u, 0xE0u + n, 0x600u, 0u);
        for (u32 node = frontend_list_next(0); node != 0;
             node = frontend_list_next(node)) {
            if (!frontend_resource_known(node))
                effects_spawn_pulse(node, 1u);      /* 0x13E28 (edx = 1) */
        }
        if ((DSB(DS_00104528 + 1) & 2u) != 0u) {
            DSD(DS_000F0A40) = actor_spawn(         /* 0x2AE14 */
                (const u32 *)(mem + DSD(0x9AEC8u + 4u * n)),   /* 0x9AEC8 */
                0x2A00u, 0xFFu, 0x3400u, 0u);
        } else {
            text_cursor_set(-1, 0x18,               /* 0x1C500 + 0x2F198 */
                game_string_get(DSD(0x9AEE4u + 12u * n)), 0x4003u);  /* 0x9AEE4 */
            u32 id2 = DSD(0x9AEE8u + 12u * n);
            if (id2 != 0)
                text_cursor_set(-1, 0x1b,           /* 0x1C500 + 0x2F198 */
                    game_string_get(id2), 0x4003u);
        }
        DSB(DS_000F0A70) = 2;
        DSB(DS_000F0A6F) = 4;
        DSW(DS_000F0A68) = 0x5Au;
        return;
    }
    case 2:
        break;      /* 0x1211C: advance, handled after the switch */
    case 3:
        actor_set_dead(DSD(DS_000F0A44));           /* 0x2B150 (edx = 3) */
        DSW(DS_000F0A64) = 3;                       /* 0x1216A (literal 3) */
        DSB(DS_000F0A6F) = 0;
        return;
    case 4: {
        /* 0x1217B stores count-1 but tests the ORIGINAL value (`mov ax,[count];
         * dec; mov [count],bx; test ax,ax; jg`), so it advances only when the
         * original is <= 0. `--count < 1` would fire one frame early. */
        s16 v = (s16)DSW(DS_000F0A68);
        DSW(DS_000F0A68) = (u16)(v - 1);
        if (v <= 0)
            DSB(DS_000F0A6F) = DSB(DS_000F0A70);
        return;
    }
    default:
        return;
    }

    /* 0x1211C: advance the entry, or leave the carousel past the sixth. */
    DSB(DS_000F0A6E)++;
    if (DSB(DS_000F0A6E) == 6u) {
        DSB(DS_000F0A70) = 3;
        DSB(DS_000F0A6F) = 4;
        DSW(DS_000F0A68) = 0x1Eu;
        return;
    }
    DSB(DS_000F0A6F) = 1;
}

/* PORT: the frame-dump hook shared by the title (Task 10) and attract (4d)
 * drivers, the counterpart of 2b's PR_SMK_DUMP. Writes the presented index
 * buffer as <dir>/<sub>/frame_%04d.raw RGB24, converted through the live
 * gfx_dac exactly as gfx_present does. The caller owns the frame counter and
 * the cap. */
static void game_dump_frame(const char *dir, const char *sub, int n)
{
    char subdir[1200];
    snprintf(subdir, sizeof subdir, "%s/%s", dir, sub);
    mkdir(dir, 0777);       /* ignore EEXIST; the same pattern main.c uses */
    mkdir(subdir, 0777);
    char path[1300];
    snprintf(path, sizeof path, "%s/frame_%04d.raw", subdir, n);
    FILE *f = fopen(path, "wb");
    if (f != NULL) {
        const u8 *idx = gfx_display();
        if (idx == NULL) idx = mem + DSD(DS_000E87A4);
        for (u32 i = 0; i < 320u * 200u; i++) {
            const u8 *rgb = gfx_dac[idx[i]];
            fwrite(rgb, 1, 3, f);
        }
        fclose(f);
    }
}

/* PORT: Task 10's title dump hook. With PR_TITLE_DUMP set — or PR_ATTRACT_DUMP
 * set, so one continuous 4d run dumps the title beside the attract — each
 * presented title frame is written as <dir>/title/frame_%04d.raw.
 * PR_TITLE_DUMP_FRAMES caps the run (default 200). There is no phase predicate:
 * Task 10 locates its 96-frame window by content alignment. Exported so the
 * Task 10 driver can dump the frames it drives. */
void game_title_dump_frame(void)
{
    const char *dir = getenv("PR_TITLE_DUMP");
    if (dir == NULL || dir[0] == '\0') dir = getenv("PR_ATTRACT_DUMP");
    if (dir == NULL || dir[0] == '\0' || s_title_dump_n < 0) return;
    const char *cap_s = getenv("PR_TITLE_DUMP_FRAMES");
    long cap = cap_s ? strtol(cap_s, NULL, 0) : 200;
    if (s_title_dump_n >= cap) return;

    game_dump_frame(dir, "title", s_title_dump_n);
    s_title_dump_n++;
}

/* PORT: the 4d attract dump hook, the state-0 counterpart of
 * game_title_dump_frame. With PR_ATTRACT_DUMP set, each presented attract
 * frame is written as <dir>/attract/frame_%04d.raw RGB24 through the same
 * gfx_dac conversion; PR_ATTRACT_DUMP_FRAMES caps the run (default 4096; the
 * one boot cycle to the title is well under it). The capture is post-logo, so
 * phase 0's logo frames are dumped too and the comparator simply finds no
 * capture for them. */
void game_attract_dump_frame(void)
{
    const char *dir = getenv("PR_ATTRACT_DUMP");
    if (dir == NULL || dir[0] == '\0') return;
    const char *cap_s = getenv("PR_ATTRACT_DUMP_FRAMES");
    long cap = cap_s ? strtol(cap_s, NULL, 0) : 4096;
    if (s_attract_dump_n >= cap) return;

    game_dump_frame(dir, "attract", s_attract_dump_n);
    s_attract_dump_n++;
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

/* 0x121A0: the title state. Phase counter DS_000F0A6F. */
static void game_state_title(void)
{
    if (DSB(DS_000F0A6F) == 0) {
        /* 0x121C9/0x121D3: FUN_0002C3FC(0x41)/(0x43) are case-5 voice cancels,
         * not a case-1 music request (their static table records are case 5,
         * their handles 0x383B6F4/0x3837440 point into s16title.gra). PORT: the
         * port requests the S16TITLE bank and queues the located announcer
         * sample here instead; that is a port choice standing in for the
         * deferred attract-state trigger (0x11000), not a transcription of
         * those calls. */
        s_music_request = 1;
        game_sample_request();
        frontend_input_reset();                 /* 0x121D9 (0x4F1E4) */
        actors_reset();                         /* 0x121E4 (0x2BAF4, eax = 1) */
        DSW(DS_00107A48) = 0;                   /* 0x121F2 */
        title_origin_reset(0);                  /* 0x121F9 (0x38910) */
        if ((DSB(DS_00104528 + 1) & 2u) == 0) {
            /* 0x12224/0x12238: text branch. DS_00104528 is pinned to 0 in
             * game_init (0x2D974(0x29) = 0 on the shipped image), so this is
             * the shipped path and the one Task 8b's renderer serves. */
            text_cursor_set(-1, 4, title_string(), 0x1000u);      /* 0x1223F */
        } else {
            /* 0x1221D: mode-1 sprite branch, unreachable on the shipped
             * profile. Arguments pinned at 0x12207-0x12218. */
            actor_spawn((const u32 *)(mem + 0x9AE3Cu), 0x2A00u, 0xFFu, 0xC00u, 0u);
        }
        /* 0x1224F/0x12262/0x12275/0x1228B: four 0x38B18(0x9AC1C) rows. */
        frontend_spawn_row((const u32 *)(mem + 0x9AC1Cu), 0u, 0u);
        frontend_spawn_row((const u32 *)(mem + 0x9AC1Cu), 0x2Au, 0u);
        frontend_spawn_row((const u32 *)(mem + 0x9AC1Cu), 0u, 0x1Eu);
        frontend_spawn_row((const u32 *)(mem + 0x9AC1Cu), 0x2Au, 0x1Eu);
        /* The EXE's only seed store is 0x20C62 in 0x20C10 (before
         * 0x2D974(0x29)); 0x121A0 itself does not re-seed. Since 4d boot runs
         * the state-0 attract first, which consumes the shared RNG stream, so
         * these draws are no longer the first three from 0xABCD. The capture was
         * made from the pinned original (tools/title_pin.py), whose three draws
         * are hardcoded to the values the port's LCG produces from 0xABCD (12,
         * 111, 0); the title driver reproduces that pin with rng_seed(0xABCD)
         * immediately before the title entry (test_title_window). No re-seed
         * here: this is the original's own RNG state. */
        int iVar1 = (int)rng_next(0x5Au);                   /* 0x12295 */
        int iVar2 = (int)rng_next(0x7Eu) * 0x40 + 0x280;    /* 0x122A1 */
        int iVar3 = (int)rng_next(2u);                      /* 0x122B7 */
        if (iVar3 != 0) iVar2 = -iVar2;                     /* 0x122C0 */
        DSW(DS_00107A50) = (u16)((iVar2 / 2) + 0x1500);     /* 0x122E6 */
        u32 logo = actor_spawn((const u32 *)(mem + 0x9AC30u),
                               (u32)iVar2 + 0x2A00u, 0xE0u,
                               0x1E00u - (u32)(iVar1 << 6), 0u);   /* 0x122F1 */
        DSD(DS_000F0A58) = logo;                            /* 0x122FD */
        DSW(DS_000F0A66) = 0x600;                           /* 0x1230B */
        DSW(logo + 0x34) = (u16)(s16)(-iVar2 / 0x5F);       /* 0x1231A */
        DSW(logo + 0x2C) = 0xAA;                            /* 0x12327 */
        DSW(logo + 0x36) = (u16)(s16)((iVar1 << 6) / 0x5F); /* 0x12331 */
        DSD(DS_000F0A54) = actor_spawn((const u32 *)(mem + 0x9AC94u),
                                       0u, 0xE4u, 0u, 0u);   /* 0x1233F */
        DSB(DS_000F0A6F)++;                                 /* 0x1234A */
        s_title_dump_n = 0;     /* arm the Task 10 dump at title entry */
    } else if (DSB(DS_000F0A6F) == 1) {
        u16 t = (u16)(DSW(DS_000F0A66) - 0x10u);
        DSW(DS_000F0A66) = t;                               /* 0x1236B */
        if (t <= 0x10u) {                                   /* 0x12375 (jg) */
            text_cells_release(-1, 4, title_string(), 0x1000u);   /* 0x12396 */
            actor_set_dead(DSD(DS_000F0A58));               /* 0x123A5 (0x2B150) */
            actor_set_dead(DSD(DS_000F0A54));               /* 0x123B1 (0x2B150) */
            DSD(DS_000F0A54) = actor_spawn((const u32 *)(mem + 0x9ACA8u),
                                           0u, 0xE4u, 0u, 0u);  /* 0x123BF */
            for (u32 node = frontend_list_next(0); node != 0;
                 node = frontend_list_next(node)) {             /* 0x123CB */
                if (DSD(node) == 0x3E688u) {                    /* 0x123D6 */
                    /* 0x123DE-0x123EA: EBX = 0x419786C, DL = 3, EAX = node. */
                    effects_spawn(node, 3u, 0x419786Cu);        /* 0x123EA */
                }
            }
            DSB(DS_000F0A6F)++;                                 /* 0x123FC */
        }
        /* 0x12402: DS_00107A50 += (rec58+0x32 >> 16) / 2, and
         * 0x12429: rec58+0x2C = 0x40000 / DS_000F0A66. */
        u32 logo = DSD(DS_000F0A58);
        if (logo != 0) {
            DSW(DS_00107A50) = (u16)(DSW(DS_00107A50) +
                                     (u16)(((s32)DSD(logo + 0x32) >> 16) / 2));
            DSW(logo + 0x2C) = (u16)(0x40000u / (u32)DSW(DS_000F0A66));
        }
    } else if (DSB(DS_000F0A6F) == 2 && DSB(DS_0009AF3D) == 0) {
        DSB(DS_000F0A6F) = 0;       /* 0x12453 (0x1244E) */
        DSW(DS_000F0A64) = 2;       /* 0x12459 */
    }
    DSW(DS_00107A3A) = (u16)(DSW(DS_00107A50) >> 5);   /* 0x12476 */
}

/* PORT: 0x12658. The state-3 handoff spawner (derived in
 * docs/superpowers/plans/2026-09-20-frontend-chain-derivations.md §2). It
 * spawns three actors from the 0x9AC44 descriptor run, stores the first at
 * DS_000F0A58 and copies the third's +0x56 byte into the record before it
 * (0x126AC/0x126D2). Then it walks the front-end list and spawns a type-3
 * effect for every live entry that is neither handle 0x3E688 nor a known
 * resource. Called only from game_state_3 phase 0. */
static void game_state_3_handoff(void)
{
    u32 first = actor_spawn((const u32 *)(mem + 0x9AC44u),
                            0x2A00u, 0xE0u, 0x5A00u, 0u);       /* 0x12672 */
    DSW(first + 0x36u) = 0xFFC0u;                               /* 0x12677 */
    DSD(DS_000F0A58) = first;                                   /* 0x1267D */
    u32 a5 = (DSW(first + 0x56u) & 0xFFFFu) | 0x400u;           /* 0x12686 */
    u32 second = actor_spawn((const u32 *)(mem + 0x9AC58u),
                             0u, 0xE2u, 0u, a5);                /* 0x1269D */
    DSB(first + 0x4Bu) = DSB(second + 0x56u);                   /* 0x126AC */
    a5 = (DSW(first + 0x56u) & 0xFFFFu) | 0x400u;               /* 0x126B3 */
    u32 third = actor_spawn((const u32 *)(mem + 0x9AC6Cu),
                            0u, 0xE2u, 0u, a5);                 /* 0x126CA */
    DSB(second + 0x4Bu) = DSB(third + 0x56u);                   /* 0x126D2 */
    for (u32 rec = frontend_list_next(0); rec != 0;             /* 0x126D7 */
         rec = frontend_list_next(rec)) {
        if (DSD(rec) == 0x3E688u) continue;                     /* 0x126E8 */
        if (frontend_resource_known(rec) != 0u) continue;       /* 0x126F3 */
        effects_spawn(rec, 6u, DSD(rec));                       /* 0x126FE */
    }
    DSW(DS_00107A44) = 0;                                       /* 0x12712 */
}

/* PORT: 0x12484. State 3, the post-select presentation. Phase 0
 * (DS_000F0A6F == 0) re-spawns the four corner rows, spawns a type-3 effect for
 * the live list entry whose handle is 0x3E688, then takes the DS_00104528 bit-1
 * branch (text rows or an actor through 0x2AE14) and hands off through 0x12658.
 * Phase 1 tracks the handoff actor's offset and terminates into state 9 when it
 * reaches 0x1E00. `cam` is the record 0x12658 stores at DS_000F0A58. */
static void game_state_3(void)
{
    if (DSB(DS_000F0A6F) == 0) {
        actors_reset();                                             /* 0x2BAF4 */
        frontend_spawn_row((const u32 *)(mem + 0x9AC1Cu), 0u, 0u);       /* 0x124A9 */
        frontend_spawn_row((const u32 *)(mem + 0x9AC1Cu), 0x2Au, 0u);    /* 0x124B9 */
        frontend_spawn_row((const u32 *)(mem + 0x9AC1Cu), 0u, 0x1Eu);    /* 0x124CC */
        frontend_spawn_row((const u32 *)(mem + 0x9AC1Cu), 0x2Au, 0x1Eu); /* 0x124DF */
        for (u32 node = frontend_list_next(0); node != 0;          /* 0x124F7 */
             node = frontend_list_next(node)) {
            if (DSD(node) == 0x3E688u)                             /* 0x12504 */
                effects_spawn(node, 2u, DSD(node));                /* 0x12515 */
        }
        if ((DSB(DS_00104528 + 1u) & 2u) == 0u) {                  /* 0x12527 */
            text_cursor_set(-1, 0x18, game_string_get(0x13u),
                            0x4003u);                              /* 0x12561 */
            text_cursor_set(-1, 0x1B, game_string_get(0x14u),
                            0x4003u);                              /* 0x12581 */
        } else {
            DSD(DS_000F0A40) = actor_spawn(                        /* 0x12546 */
                (const u32 *)(mem + 0x9AEB4u), 0x2A00u, 0xFFu, 0x3400u, 0u);
        }
        game_state_3_handoff();                                    /* 0x12592 */
        DSB(DS_000F0A6F)++;                                        /* 0x12597 */
        return;
    }
    if (DSB(DS_000F0A6F) == 1) {
        u32 cam = DSD(DS_000F0A58);                                /* 0x125BF */
        DSW(DS_00107A38) = (u16)(DSW(DS_00107A44) >> 6);           /* 0x125B9 */
        s32 v = 0x5A00 - (s32)DSD(cam + 0x1Cu);                    /* 0x125CE */
        if (v < 0) v = -v;                                         /* 0x125D4 */
        DSW(DS_00107A44) = (u16)v;                                 /* 0x125D6 */
        s32 a = (s32)DSD(cam + 0x1Cu) - 0x1E00;                    /* 0x125E8 */
        s32 b = (s16)DSW(cam + 0x36u) < 0                          /* 0x125EE */
                    ? -(s32)((s32)DSD(cam + 0x34u) >> 16)          /* 0x125F9 */
                    :  (s32)((s32)DSD(cam + 0x34u) >> 16);         /* 0x12600 */
        if (a < 0) a = -a;                                         /* 0x12607 */
        if (a <= b) {                                              /* 0x12609 */
            DSD(cam + 0x24u) = 0x40C00000u;                        /* 0x12621 */
            DSW(DS_000F0A6C) = 6;                                  /* 0x12628 */
            DSD(cam + 0x1Cu) = 0x1E00u;                            /* 0x1262F */
            DSW(DS_000F0A64) = 9;                                  /* 0x12636 */
            DSW(cam + 0x36u) = 0;                                  /* 0x1263D */
            DSW(DS_000F0A6A) = 0xF0;                               /* 0x12645 */
            DSB(DS_000F0A72) = 0;                                  /* 0x1264C */
        }
    }
}

/* PORT: 0x11578. State 4, the match-up sequence. Its phase counter is
 * DS_0009AD98 (separate from the dispatch word DS_000F0A64). Cases 0/1/2 are
 * the unrolled credit-roll pages of 8, 12 and 13 0x2F4BC calls — the counts
 * are literal in the raw, not a loop bound. Each spawns the 0x9AD84
 * descriptor, arms the 180-frame timer DS_000F0A76 and sets its continuation
 * phase DS_000F0A74, then enters phase 4. Phase 4 counts the timer down and
 * continues when the pre-decrement value is zero. Phase 3 hands to state 9.
 * 0x2C06C is called in case 0 only, as the raw does. */
static void game_state_4(void)
{
    switch (DSW(DS_0009AD98)) {
    case 0:
        /* PORT: 0x2C3FC(0x100) voice, out of scope (spec §7). */
        frontend_input_reset();                                 /* 0x4F1E4 (eax = 0) */
        actors_reset();                                         /* 0x2BAF4 (eax = 1) */
        config_set_credit_row(0x1du);                           /* 0x2C06C (eax = 0x1D) */
        (void)actor_spawn((const u32 *)(mem + 0x9AD84u),        /* 0x2AE14 */
                          0u, 0xE0u, 0u, 0u);
        text_cursor_hold(-1, 1, (const u8 *)(mem + 0x8005Cu), 0x2000u);   /* 0x2F4BC */
        text_cursor_hold(-1, 3, (const u8 *)(mem + 0x80070u), 0x2000u);
        text_cursor_hold(2, 7, (const u8 *)(mem + 0x80090u), 0u);
        text_cursor_hold(2, 9, (const u8 *)(mem + 0x800BCu), 0u);
        text_cursor_hold(2, 0xB, (const u8 *)(mem + 0x800E8u), 0x1000u);
        text_cursor_hold(2, 0xD, (const u8 *)(mem + 0x8010Cu), 0x1000u);
        text_cursor_hold(2, 0xF, (const u8 *)(mem + 0x80130u), 0x2000u);
        text_cursor_hold(2, 0x11, (const u8 *)(mem + 0x80154u), 0x2000u);
        DSW(DS_000F0A76) = 0xB4;                                /* 0x116A5 */
        DSW(DS_000F0A74) = 1;                                   /* 0x116AC */
        DSW(DS_0009AD98) = 4;                                   /* 0x116B2 */
        return;
    case 1:
        /* PORT: 0x2C3FC(0x100) voice, out of scope (spec §7). */
        frontend_input_reset();                                 /* 0x4F1E4 (eax = 0) */
        actors_reset();                                         /* 0x2BAF4 (eax = 1) */
        (void)actor_spawn((const u32 *)(mem + 0x9AD84u),        /* 0x2AE14 */
                          0u, 0xE0u, 0u, 0u);
        text_cursor_hold(-1, 1, (const u8 *)(mem + 0x8005Cu), 0x2000u);   /* 0x2F4BC */
        text_cursor_hold(-1, 3, (const u8 *)(mem + 0x8017Cu), 0x2000u);
        text_cursor_hold(2, 5, (const u8 *)(mem + 0x80198u), 0u);
        text_cursor_hold(2, 7, (const u8 *)(mem + 0x801BCu), 0u);
        text_cursor_hold(2, 9, (const u8 *)(mem + 0x801E4u), 0x1000u);
        text_cursor_hold(2, 0xB, (const u8 *)(mem + 0x80208u), 0x1000u);
        text_cursor_hold(2, 0xD, (const u8 *)(mem + 0x80228u), 0x1000u);
        text_cursor_hold(2, 0xF, (const u8 *)(mem + 0x8024Cu), 0x1000u);
        text_cursor_hold(2, 0x11, (const u8 *)(mem + 0x8026Cu), 0x1000u);
        text_cursor_hold(2, 0x13, (const u8 *)(mem + 0x80290u), 0x3000u);
        text_cursor_hold(2, 0x15, (const u8 *)(mem + 0x802B4u), 0x3000u);
        text_cursor_hold(2, 0x17, (const u8 *)(mem + 0x802D8u), 0x3000u);
        DSW(DS_0009AD98) = 4;                                   /* 0x11824 */
        DSW(DS_000F0A76) = 0xB4;                                /* 0x1182B */
        DSW(DS_000F0A74) = 2;                                   /* 0x11832 */
        return;
    case 2:
        /* PORT: 0x2C3FC(0x100) voice, out of scope (spec §7). */
        frontend_input_reset();                                 /* 0x4F1E4 (eax = 0) */
        actors_reset();                                         /* 0x2BAF4 (eax = 1) */
        (void)actor_spawn((const u32 *)(mem + 0x9AD84u),        /* 0x2AE14 */
                          0u, 0xE0u, 0u, 0u);
        text_cursor_hold(-1, 1, (const u8 *)(mem + 0x802FCu), 0x2000u);   /* 0x2F4BC */
        text_cursor_hold(-1, 3, (const u8 *)(mem + 0x80310u), 0x2000u);
        text_cursor_hold(2, 5, (const u8 *)(mem + 0x80328u), 0u);
        text_cursor_hold(2, 7, (const u8 *)(mem + 0x8034Cu), 0x1000u);
        text_cursor_hold(2, 9, (const u8 *)(mem + 0x8036Cu), 0x1000u);
        text_cursor_hold(2, 0xB, (const u8 *)(mem + 0x80394u), 0x1000u);
        text_cursor_hold(2, 0xD, (const u8 *)(mem + 0x803B4u), 0x1000u);
        text_cursor_hold(2, 0xF, (const u8 *)(mem + 0x803DCu), 0x2000u);
        text_cursor_hold(2, 0x11, (const u8 *)(mem + 0x803FCu), 0x2000u);
        text_cursor_hold(2, 0x13, (const u8 *)(mem + 0x80420u), 0x3000u);
        text_cursor_hold(2, 0x15, (const u8 *)(mem + 0x80444u), 0x3000u);
        text_cursor_hold(2, 0x17, (const u8 *)(mem + 0x80464u), 0x3000u);
        text_cursor_hold(2, 0x19, (const u8 *)(mem + 0x80488u), 0x3000u);
        DSW(DS_000F0A76) = 0xB4;                                /* 0x119C0 */
        DSW(DS_000F0A74) = 3;                                   /* 0x119C7 */
        DSW(DS_0009AD98) = 4;                                   /* 0x119CD */
        return;
    case 3:
        DSW(DS_000F0A6C) = 0;                                   /* 0x119E6 */
        DSW(DS_000F0A64) = 9;                                   /* 0x119ED */
        DSW(DS_000F0A6A) = 1;                                   /* 0x119F4 */
        DSW(DS_0009AD98) = 0;                                   /* 0x119FB */
        return;
    case 4: {
        /* PORT: the raw reads DS_000F0A76 before the decrement and continues
         * on the frame the pre-decrement value is zero (`mov ax,[...]; test
         * ax,ax; ja`), not after the store wraps. */
        u16 old = DSW(DS_000F0A76);                             /* 0x11A08 */
        DSW(DS_000F0A76) = (u16)(old - 1u);                     /* 0x11A11 */
        if (old == 0u) DSW(DS_0009AD98) = DSW(DS_000F0A74);     /* 0x11A1D */
        return;
    }
    default:
        return;
    }
}

/* 0x11A8C. State 6: the demo-fight setup. It picks two random characters from
 * the shared RNG stream and arms the 900-frame state-7 timer.
 *
 * RNG DRAW ORDER (Task 7 pins the shared stream): draw1 = rng(7) at 0x11AAD also
 * sets DS_00104AFC and P0's character; draw2 = rng(6) at 0x11AE9 gives P1 the
 * character (draw1 + draw2) % 7. Exactly two draws, in that order. */
static void game_state_6(void)
{
    /* PORT: 0x2C3FC(0x100) voice, out of scope (spec §7). */
    /* 0x29D60 is a ret-only no-op. */
    config_set_credit_row(0x1Du);                       /* 0x11AA3 0x2C06C */

    u32 draw1 = rng_next(7u);                           /* 0x11AAD (draw 1) */
    DSW(DS_00104AFC) = (u16)draw1;                      /* 0x11AB4 */
    /* PORT: 0x11AC4 0x20DF4(eax=draw1, edx=1) — a 155-byte reset. Its eight
     * pre-branch calls are 0x29B70, 0x2C390, 0x12750, 0x49300, 0x28E98, 0x34978,
     * 0x2C074 and 0x12C70, and 0x20E5C/0x20E63 also write the words
     * DS_000F0AFA/DS_000F0AF8. Of these 0x12750 and 0x49300 are ported here.
     * 0x12750 (camera_dust_list_init) builds the type-0x01 node lists without
     * which 0x1282C's spawn is refused (demo record §15). 0x49300
     * (fight_list_init) is the liveness precondition — it self-links the
     * fight-effect list sentinel DS_0010884C that the 0x49C78 walk reads, so a
     * zero head would walk address 0 forever. 0x12C70 (camera_step_seed) is
     * called at its raw position below (0x20E6A); the rest, including the two
     * word stores DS_000F0AFA/DS_000F0AF8, stay a named gap (record §6.10) —
     * both stores are BSS-zero, so the port is net-faithful for them. */
    camera_dust_list_init();                            /* 0x11AC4 0x12750 (0x20E33) */
    fight_list_init();                                  /* 0x11AC4 0x49300 */
    /* 0x20E4C/0x20E52: the reset zeroes the two camera words the projection
     * reads — 0x38A38's stride is DS_000F0AF0 << 8 and 0x2A620's shear base is
     * DS_000F0AEC — before the 0x38730 call below. */
    DSD(DS_000F0AEC) = 0;
    DSD(DS_000F0AF0) = 0;
    /* 0x20E6A 0x12C70: the camera-x step seed (DS_000F0AFC = 0x400), at the
     * raw's position before the EDX branch below. camera_x_commit reads it as
     * the step. */
    camera_step_seed();                                 /* 0x20E6A 0x12C70 */
    /* 0x20E78 0x2BAF4(EAX=1): the branch's first call. It clears the actor and
     * pset pools, the render list and the process masks, zeroes the two
     * offscreen buffers and blacks the DAC (0x52106/0x336C0), which releases the
     * attract's presentation actors and their held text before the fight's own
     * actors spawn. The port's actors_reset() ports the param_1 != 0 arm.
     * The branch's other two calls follow below: 0x38730 and 0x412A0. */
    actors_reset();                                     /* 0x11AC4 0x2BAF4 */
    /* 0x20E7F 0x38730(eax=draw1): the attract projection setup. Its argument is
     * 0x20DF4's clamped EAX (0x20DF7 MOV EBX,EAX; 0x20E01/0x20E06 clamp to 7;
     * 0x20E7D MOV EAX,EBX), which 0x11AC1 loaded with the state-6 draw. */
    render_scroll_setup(draw1);                         /* 0x11AC4 0x38730 */
    /* 0x20E86 0x412A0(eax=draw1): the branch's third call — the scene's prop
     * actors (0xC82CC[draw1]) and the crowd (0x2C320), which 0x412A0's tail
     * runs. */
    fight_scene_props(draw1);                           /* 0x11AC4 0x412A0 */
    fight_char_select(0u, draw1);                       /* 0x11ACD 0x41350 */
    fighter_spawn(0u);                                  /* 0x11AD9 0x33EB4 */
    /* 0x11AE3: EDX after 0x33EB4 is the caller's 7 — 0x33EB4 push/pops EDX and
     * its `ret 4` consumes only the stack argument, so this is the constant, not
     * a live callee return (record §10.8). */
    DSD(DS_001082C8) = 7u;                              /* 0x11AE3 */

    u32 draw2 = rng_next(6u);                           /* 0x11AE9 (draw 2) */
    u32 p1 = draw1 + draw2;                             /* 0x11AEE */
    if (p1 >= 7u) p1 -= 7u;                             /* 0x11AF6 (single sub 7) */
    fight_char_select(1u, p1);                          /* 0x11AFE 0x41350 */
    fighter_spawn(1u);                                  /* 0x11B08 0x33EB4 */

    DSB(DS_00104B15) = 1;                               /* 0x11B14 */
    /* 0x11B1E 0x1D890(0): EAX is 0, so only the four per-side HUD bytes are
     * zeroed; the HUD actor arm is cycle 2's (§10.6). DL=1 is preserved (0x1D890
     * push/pops EDX), so the store below is the constant 1. */
    fight_hud_spawn(0u);                                /* 0x11B1E 0x1D890 */
    DSB(DS_00104B19 + 2u) = 1u;                         /* 0x11B23 */
    DSW(DS_001082CC) = 3u;                              /* 0x11B2F */

    if ((DSB(DS_00104528 + 1u) & 2u) == 0u) {           /* 0x11B35 */
        text_cursor_set(-1, 2, game_string_get(6u), 0x2000u);   /* 0x11B49/55 */
        text_cursor_set(-1, 4, game_string_get(7u), 0x2000u);   /* 0x11B69/75 */
        text_cursor_set(-1, 6, game_string_get(8u), 0x2000u);   /* 0x11B89/95 */
    }

    DSW(DS_000F0A64) = 7;                               /* 0x11BAB */
    DSW(DS_000F0A6A) = 900;                             /* 0x11BB2 */
    DSW(DS_000F0A6C) = DSW(DS_000F0A72);                /* 0x11BB9 */
    DSB(DS_000F0A6F) = 0;                               /* 0x11BBF */
}

/* PORT: 0x1EA08. The match-start builder, called inline from state 5. It resets
 * the input latch and the actor pool, spawns the roster row from the 0xA7B6C
 * descriptor, then builds the per-character rows and spawns the selected
 * character's actor. The build reads its strings and its selection key from the
 * paged resource reader 0x2DBC4/0x2DB58 (not modelled) and formats them through
 * 0x2F4D0 (0x2EFD4, also not modelled), so only the three calls before the
 * resource read are transcribed. Task 6 wires only this state-5 call site;
 * 0x1EA08's other four callers (0x11A30 and three in FUN_0001EEB0, the match
 * sub-state machine) belong to the match cycle and stay unwired. */
static void frontend_match_start(void)
{
    frontend_input_reset();                                     /* 0x1EA11 (0x4F1E4) */
    actors_reset();                                             /* 0x1EA26 (0x2BAF4) */
    frontend_spawn_row((const u32 *)(mem + 0xA7B6Cu), 0u, 0u);  /* 0x1EA30 (0x38B18) */
    /* PORT: 0x1EA4A onward is the resource-driven roster build: 0x2DBC4 returns
     * a string blob whose [esi+0x16] selects the character descriptor from
     * 0xA7DCC (the guard at 0x1EB0A keeps indices 0..6), and 0x2F4D0/0x2F4BC
     * draw the formatted rows before 0x2AE14/0x2A17C spawn the selected actor.
     * The port does not model the paged resource reader 0x2DB58/0x2DBC4 or the
     * 0x2EFD4 formatter, so the blob, the `local` index, the string draws and
     * the actor spawn are a declared gap
     * (docs/superpowers/plans/2026-09-20-frontend-chain-derivations.md §7.1,
     * §7.2); spawning descriptor 0 would be a fitted constant. */
}

/* 0x10E80: initialise the game state. */
static void game_state_init(void)
{
    /* 0x10E80 enters state 0 (the 0x11000 attract sub-machine), which plays the
     * two boot logos in phase 0 (through movie_play) and hands off to title
     * state 1 when DS_000F0A5C wraps to 0. DS_00104B00 is the port's selected
     * mode; the original derives the value in 0x10E80's register handoff. The
     * row-1 credit value comes from attract phase 2's 0x2C06C(1), not a
     * stand-in. */
    DSD(DS_00104B00) = 3;
    DSW(DS_000F0A64) = 0;
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
    /* The original's MDI install loads the driver's FM patch bank (FAT.OPL).
     * Without it every key-on carries no operator setup — the sequencer maps
     * each program change through this bank (sequencer.c) — so the OPL core
     * renders silence for the whole run. The bytes come through the resource
     * layer like every other asset; a missing or short bank leaves the game
     * silent, which is what the original does with no driver bank. */
    {
        u32 bank_off = 0, bank_len = 0;
        if (res_load_file(s_game_dir, "FAT.OPL", &bank_off, &bank_len))
            patches_load(mem + bank_off, bank_len);
    }
    HTIMER timer = AIL_register_timer(NULL);
    AIL_set_timer_frequency(timer, 0x3c);   /* the original's 60 Hz game tick */
    AIL_start_timer(timer);
    s_last_host_tick = host_tick_count();
}

/* Locates the title music bank in S16TITLE.GRA through the resource layer: the
 * first FORM/XMID container in the resource, the same scan tools/opl_seq.py and
 * seq_load use. Task 9's capture spans this S16TITLE bank end to end. (The
 * original's 0x121a0 FUN_0002c3fc(0x41)/(0x43) are case 5 voice cancels, not a
 * case-1 music request.)
 * TODO(verify): the sound-table id -> resource handle mapping is not extracted
 * (DAT_000BBDC8 is a static table in PRAGE.EXE, stride 12, byte 0 = case,
 * dword +4 = handle), so the port binds the title state to the bank directly.
 * Returns NULL on a bank whose declared FORM size runs past the loaded
 * resource. */
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
    /* The original gates this on the per-slot flag DAT_00102868[slot] == 1
     * (prage.c:8469-8471): only a flagged slot gets loop count 0. The port has
     * no per-slot flag and always forces 0 (one-shot). TODO(verify): the flag's
     * source and the original's non-1 behaviour are unmodelled; the shipped data
     * presumably carries 1, which is why the port matches the spec. */
    AIL_set_sample_loop_count(h, 0);
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

void game_set_game_dir(const char *dir)
{
    s_game_dir = dir;
    /* The attract machine's phase-0 boot logos (0x1C740) play from the same
     * directory. */
    attract_set_media_dir(dir);
}

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
    /* PORT: the DOS/4GW loader maps both LE objects before 0x1BEC4 runs. The
     * port reimplements the code object but the data object at DATA_BASE holds
     * the title descriptors and tables 0x121A0 reads; res_load_index only loads
     * the INDEX resources, so map the image here. The resource heap starts at
     * RES_HEAP (= the data object's end), so the two regions never overlap. */
    {
        char exe_path[512];
        snprintf(exe_path, sizeof exe_path, "%s/PRAGE.EXE", s_game_dir);
        if (!mem_load_le(exe_path, NULL)) {
            game_fatal("PRAGE.EXE image load failed");
            return;
        }
    }
    if (res_load_index(s_game_dir, index_path) <= 0) {
        game_fatal("resource INDEX load failed");
        return;
    }
    /* The actor and pset pools are the two allocations res_load_index performs
     * (0x1B120's DS_001014EC/DS_001014F4); actors_init validates them. */
    if (!actors_init()) {
        game_fatal("actor pool allocation missing");
        return;
    }
    surface_setup();        /* 0x51F45 */
    palette_list_init();    /* 0x336C0 */
    render_list_init();     /* 0x1C350 */
    rng_seed(0xABCDu);      /* PORT: 0x20C10 seeds the LCG with a hardcoded 0xABCD. */
    /* PORT: 0x20C5D-0x20CC2: DS_00104528 = 0x2D974(0x29) and the three globals
     * derived from v. The master init 0x2F9CC (0x20C15) runs 0x13ADC
     * (effects_init, inside actors_init above) then 0x2D6F8 (config_validate)
     * before this block, so on a fresh image v is what the defaults path wrote. */
    config_validate();          /* 0x2F9CC's 0x2D6F8, before 0x20C5D */
    u32 v = config_field_get(0x29u);                   /* 0x20C68 */
    DSD(DS_00104528) = v;                              /* 0x20C6D */
    DSB(DS_00105B3A) = (u8)((v & 0x100u) >> 4);        /* 0x20C9F */
    DSD(DS_001088D0) = (v & 0xFu) * 5u + 0x1Eu;        /* 0x20CB0 */
    DSB(DS_0010452C) = (u8)((v & 0xF0u) >> 4);         /* 0x20CC2 */
    /* PORT: 0x2C304, called from 0x10E80 at 0x10ECC. The port's
     * game_state_init() transcribes only 0x10E80's state handoff, not this
     * credit initializer, so the derivation lives here with the config reads it
     * depends on: DS_00105C00 = ((0x2D974(0x29) & 0xF0000) >> 16) + 1. */
    DSD(DS_00105C00) = ((config_field_get(0x29u) & 0xF0000u) >> 16) + 1u;
    /* PORT: 0x2BF08's captured inputs (docs/superpowers/plans/
     * 2026-09-18-bf08-overlay-diagnosis.md §2.3). DS_00105C00 is the live credit
     * counter the overlay renders as `<CREDITS string>:<n>`; the un-pinned title
     * capture shows 5, now derived by 0x2C304's config read above. Its
     * decrementers (0x2CA48/0x2CA7C via the title input handler 0x11F28) are
     * input-driven and wired through game_state_step's coin poll. The
     * renderer scales a text row by 20/3 px (0x200 >> 6, projected by
     * render_proj_y's 3414/4096), so text row 1 lands on the captured screen
     * rows 7-12.
     * TODO(verify): reproduces the captured no-input window only; credit
     * countdown under input is not yet covered by an oracle. */
    /* 0x20CCC: the init chain writes the overlay row to 0x1D. */
    config_set_credit_row_init();
    /* PORT: 0x5004A joystick init — the port reads int 16h keyboard only. */
    /* PORT: 0x1D0BC allocates the MIDI sequence buffer and the four sample
     * buffers. The port references the XMIDI bank's resource bytes directly
     * (sequencer.c) and samples.c allocates each handle's conversion buffer on
     * AIL_start_sample, so no init-time work buffers are needed. */
    /* 0x47370 loads the localisation table (0x20C10 calls it before 0x10E80);
     * the port reads ENGLISH.TXT directly rather than the DOS memory/file
     * managers. 0x121A0's caption comes from it. */
    game_string_table_load(s_game_dir);

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
    game_loop_begin();      /* 0x255D4/0x255DA loop prologue */
    game_loop();            /* 0x20C10 -> 0x255CC */
    game_shutdown();        /* 0x1BE30 teardown */
    return 0;
}

/* 0x255D4/0x255DA: the master loop's prologue zeroes the tick pair before the
 * first iteration. PORT: split out because the port's game_loop() is the loop
 * body and is driven one iteration at a time by the check/test drivers. */
void game_loop_begin(void)
{
    DSD(DS_00101508) = 0;
    DSD(DS_0010150C) = 0;
}

void game_loop(void)
{
    /* PORT: the raw's 0x255CC prologue (0x255D4/0x255DA) zeroes the tick pair
     * once, at the loop's entry. The port's game_loop() is called once per frame
     * by the test/check drivers, so the zeroing lives in game_loop_begin()
     * (called once by game_main) instead of here. */
    do {
        {
            /* PORT: the host fills the key bitmap 0x500C4 samples; the binding
             * is host.c's table (host_key_bits()). */
            u16 bits = host_key_bits();
            u8 *k = mem + DSD(DS_00101514);
            k[0x2d8] = (u8)(bits >> 8);
            k[0x2d9] = (u8)bits;
        }
        input_pump();                        /* 0x500C4 */
        /* 0x255F3: the per-bit scene tick runs unconditionally; the two
         * scroll/zoom projection calls at 0x25601/0x25606 are gated by
         * DS_00107A54 (the raw's `cmp byte [0x107a54],0; je`). */
        attract_scene_tick();                /* 0x292AC */
        if (DSB(DS_00107A54) != 0u) {
            render_scroll_edge();            /* 0x389C4 */
            render_scroll_fill();            /* 0x38A38 */
        }
        /* PORT: the state that dispatches this frame; the attract handoff sets
         * state 1 inside game_frame, so the presented frame must be attributed
         * to the state that produced it. */
        const u16 state_before = DSW(DS_000F0A64);
        game_frame();                        /* 0x24C5C */
        run_process_table(DS_000A86C4, DSD(DS_00104AEC));  /* render table */
        DSD(DS_00104AF4)++;                  /* 0x2563A */
        effects_step();                      /* 0x134C0 (0x2563E) */

        /* 0x25643: the tick gate. The original presents only when the frame
         * counter DS_0010150C has caught the ISR tick DS_00101508; the sort,
         * render, flush, copy and swap all sit inside it (0x25650-0x256AA). The
         * port's state-6 resource reads advance DS_00101508 (res.c), so the
         * loader frame's gate fails and the loop catches up exactly as the raw
         * does — see record §9.6. */
        if (DSD(DS_0010150C) == DSD(DS_00101508)) {          /* 0x25643 */
            render_list_sort();              /* 0x25650 0x1C3FC */
            if (DSB(DS_001088F4) == 0) render_list();        /* 0x2566D 0x14328 */
            /* The original copies DAT_000E87A4 to the literal VGA aperture
             * 0xA0000 here (0x25680 full copy when DS_001014FC != 0, else the
             * 0x501A3 dirty-dword blit). PORT: the aperture rule — never write
             * mem[0xA0000]; present the index buffer through gfx_present(),
             * which converts via gfx_dac to RGB and hands it to the host. */
            gfx_flush_palette();             /* 0x25672 0x1C470 */
            gfx_present(mem + DSD(DS_000E87A4), 320, 200);   /* 0x25680 the copy */
            /* PORT: Task 10's PR_TITLE_DUMP hook and 4d's PR_ATTRACT_DUMP hook —
             * one RGB24 file per *presented* frame, read after the copy and before
             * the swap, so the stream contains only the frames the original
             * presents (a gate-failed frame dumps nothing). The frame is
             * attributed to the state that dispatched it: the attract's phase-0xB
             * handoff sets state 1 inside game_frame, so it is still an attract
             * frame. */
            if (state_before == 0) game_attract_dump_frame();
            else if (state_before == 1) game_title_dump_frame();
            DSD(DS_001014FC) = 0;            /* 0x2569D */
            swap_buffers();                  /* 0x256AA 0x50188 */
        }
        DSD(DS_0010150C)++;                  /* 0x256C0 */

        /* PORT: 0x256B1 (the master loop's body draw, `rng(0x7FFF)`) is pinned to
         * a non-advancing `mov eax,0` in the pinned original (tools/title_pin.py),
         * and the port draws nothing here, so both streams carry only the
         * consumption-site draws and stay in step. The original's spin draw
         * (0x256D6) is pinned the same way; the port's spin advances the ISR tick
         * below and draws nothing. */

        /* 0x255CC's tail calls 0x1CF20 here: play pending samples, start/drive
         * the music, render one frame of audio. */
        game_audio_service();

        /* 0x256C5: the original spins while DS_0010150C-1 == DS_00101508, i.e.
         * until the timer ISR (0x1BDF4) advances DS_00101508 to the frame
         * counter. PORT: the spin is where the host retrace is waited, and the
         * ISR's increment is modelled as one tick per retrace; when the loop is
         * behind (a resource-read stall), the spin does not run and the loop
         * catches up without waiting — matching the raw. */
        while (DSD(DS_0010150C) - 1u == DSD(DS_00101508)) {  /* 0x256C5 */
            DSD(DS_00101508)++;
            host_wait_vblank();
        }

        /* PORT: the original reads int 16h inside 0x24C5C's keyboard loop and
         * quits from 0x249F0; the port ports only that quit arm and tests it
         * here, so the test drains the queue (input_drain_esc) — the BIOS
         * queue's head advances only on a read. A window close is the host's
         * own request rather than a key. */
        if (input_drain_esc() || host_quit_requested()) {  /* ESC: 0x011B */
            DSB(DS_000A81A8) = 1;
        }
    } while (DSB(DS_000A81A8) == 0);
}

void game_frame(void)
{
    /* PORT: 0x24C5C calls 0x4F644 at 0x24C6E (unless DAT_00104B00 == 0x27) as
     * its first action, before the frame counter and the process tables. */
    if (DSW(DS_00104B00) != 0x27u) input_state_update();   /* 0x4F644 (0x24C6E) */

    /* 0x24C73: the per-side CPU-AI command block. It runs while
     * DS_00104B26 == 0 (BSS, read-only in the image) and DS_00104B19+2 != 0
     * (armed by state 6), and fills DS_001088E0/E2 via 0x47208. The original's
     * int 16h input loop and the 0x94-byte player records it consumes are the
     * interactive match's and stay a gap; this block is the demo's AI. */
    fighter_command_block();                           /* 0x24C73 */
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
         * and diagnostics; deferred to sub-projects 4/5. The effect call sites
         * 0x29B74 (the DS_00104AE4 mode-0x17 handler, six `call [0x104ae4]`
         * sites) and 0x41578 (four direct calls) live only in those unported
         * modes — 0x24C5C cases 0x12 and 0x16..0x1b — plus the unported
         * match/fight chain, so they are deferred with them (UNOWNED BY THIS
         * PLAN; see port/spec/game_flow.md). DS_00104B00 is fixed at 3 by
         * 0x10E80, so no reachable path enters those cases; nothing is wired. */
        break;
    }

    /* 0x24C5C's tail calls 0x2A31C here (Format reference I): walk the active
     * list and sync each record's pset before the render table composites. */
    actors_update();                                   /* 0x2A31C */

    /* 0x24C5C's DS_00104B15 tail (0x25414): the demo fight's post-update. It is
     * armed by state 6 and cleared by the 0x11BCC timer exit. The per-side loop
     * gates on the camera-target table DS_001077A8[side], which 0x33C78 (the
     * unported fighter spawn) fills. */
    if (DSB(DS_00104B15) != 0) {                       /* 0x25414 */
        /* PORT: 0x3BB90 is cycle 2's; see the spec's cycle split. */
        camera_dispatch();                             /* 0x25422 0x12D48 */
        for (u32 side = 0; side < 2u; side++) {        /* 0x2542D */
            if (DSD(DS_001077A8 + side * 4u) == 0) continue;
            fighter_slot_latch(side);                  /* 0x25438 0x186D0 */
            actor_pset_point(DSD(DS_001077B0 + side * 0x94u));  /* 0x25443 */
        }
        fight_health_bars();                           /* 0x25457 0x33F08 */
    }
}

/* PORT: 0x11F28. One coin/start event: requires a credit (0x2C060), then tests
 * the event's mask in the DS_0009ACBC table against the newly-pressed bits
 * DS_001088E4, and debits one credit through 0x2CA7C. Returns 1 when accepted. */
static u32 frontend_coin_poll(u32 code)
{
    if (config_credit_ready() == 0u) return 0u;
    if ((DSD(DS_0009ACBC + code * 4u) & DSD(DS_001088E4)) == 0u) return 0u;
    (void)config_credit_spend(1u);
    return 1u;
}

void game_state_step(void)
{
    if (DSB(DS_00104B1D) == 0) {
        u32 accepted = 0u;
        if (frontend_coin_poll(0u)) accepted |= 1u;    /* 0x11D15 */
        if (frontend_coin_poll(1u)) accepted |= 2u;    /* 0x11D28 */
        if (accepted != 0u) {
            /* PORT: the raw then calls 0x32970(eax=0) and 0x257a4(eax=accepted)
             * and returns from 0x11D04, so the state dispatch below is skipped
             * for that frame. Both divert handlers are unported (out of scope). */
            return;
        }
    }

    if (DSW(DS_000F0A64) < 10) {
        s16 sVar1 = (s16)(DSW(DS_000F0A6A) - 1);
        switch (DSW(DS_000F0A64)) {
        case 1:
            game_state_title();   /* 0x121A0 */
            break;
        case 2:
            game_state_select();   /* 0x11F6C */
            break;
        case 3:
            game_state_3();   /* 0x12484 */
            break;
        case 4:
            game_state_4();   /* 0x11578 */
            break;
        case 5:
            /* PORT: 0x2C3FC(0x100 / ecx = 0x12C) voice/sample cancel, out of
             * scope (spec §7). */
            frontend_match_start();                     /* 0x1EA08 */
            config_set_credit_row(0x1Du);               /* 0x2C06C */
            /* PORT: 0x32970(eax = 0), the run-clock/tick update, is out of scope
             * (spec §7); the host clock owns wall time. */
            DSB(DS_000F0A6F) = 0;                       /* 0x11E11 */
            DSB(DS_000F0A72) = 0;                       /* 0x11E17 */
            DSW(DS_000F0A6A) = 0x12C;                   /* 0x11E1D */
            DSW(DS_000F0A6C) = 6;                       /* 0x11E2E */
            DSW(DS_000F0A64) = 9;                       /* 0x11E35 */
            break;
        case 6:
            game_state_6();                             /* 0x11A8C */
            break;
        case 7:
            /* 0x11D62 computes sVar1 = (u16)timer - 1 before the switch; case 7
             * stores it and exits when the new value is 0, so the exit frame is
             * the one whose PRE value is 1. */
            DSW(DS_000F0A6A) = (u16)sVar1;              /* 0x11E67 */
            if (sVar1 == 0) {                           /* 0x11E72 (jge) */
                /* 0x11BCC the timer exit. */
                DSB(DS_00104B19 + 2u) = 0;              /* 0x11BCE */
                DSB(DS_00104B15) = 0;                   /* 0x11BD4 */
                DSW(DS_000F0A64) = DSW(DS_000F0A6C);    /* 0x11BE0 */
                /* PORT: 0x29D60 is a ret-only no-op; 0x2C3FC(0x100) voice, out
                 * of scope (spec §7). */
            } else {
                fight_arena_frame();                    /* 0x11E8F 0x263F4 */
                fight_health_bars();                    /* 0x11E94 0x33F08 */
            }
            break;
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
            /* 0x11CDC[0] -> 0x11D70: state 0 runs the attract sub-machine. */
            attract_step();     /* 0x11000 */
            break;
        }
    } else {
        /* 0x11D54 (`ja 0x11D70`): states > 9 run the attract sub-machine. */
        attract_step();         /* 0x11000 */
    }
    /* 0x11D8D/0x11D92/0x11D97: every state's jump-table target ends with the
     * two tails then the overlay, in that order (state 0's 0x11D70 too). */
    frontend_pause_tail();      /* 0x10DB0 */
    frontend_continue_tail();   /* 0x10E18 */
    game_overlay_step();        /* 0x2BF08 */
}

/* PORT: 0x2BF08. Raw disassembly fixes the branch order and the misstated
 * arguments (prage.c drops them): 0x45 FREE PLAY, 0x46 CREDITS, 0x47 INSERT
 * COINS. `sprintf(buf, "%s:%d", ...)` becomes snprintf; the 0x65546 formatter
 * itself is not ported. The message branch is ungated by DS_000EF6DC & 0x1f —
 * that gate belongs only to the DS_00105C00 == 0 fallback. */
void game_overlay_step(void)
{
    s32 row = (s32)DSB(DS_00105C05);

    if (DSB(DS_0009AD58) != 0) return;                  /* 0x2BF18: attract */

    if (DSB(DS_00105D60) != 0) {                        /* 0x2BF27: FREE PLAY */
        const u8 *s = game_string_get(0x45u);
        if ((DSB(DS_000EF6DC) & 0x20u) != 0)
            text_cursor_hold(-1, row, s, 0u);           /* 0x2BF53 */
        else
            text_cells_release(-1, row, s, 0u);         /* 0x2C049 */
        DSB(DS_00105C04) = 0;                           /* 0x2C04E */
        return;
    }

    if (DSD(DS_00105C00) != 0) {                        /* 0x2BF7D: CREDITS */
        char buf[64];
        snprintf(buf, sizeof buf, "%s:%d", game_string_get(0x46u),
                 (int)DSD(DS_00105C00));
        text_cursor_set(-1, row, (const u8 *)buf, 0u);  /* 0x2BFBA */
        DSB(DS_00105C04) = 0;                           /* 0x2BFC1 */
        return;
    }

    /* 0x2BFCE: DS_00105C00 == 0 fallback (the blink). */
    if ((DSB(DS_000EF6DC) & 0x1fu) != 0 && DSB(DS_00105C04) == 0) {
        DSB(DS_00105C04) = 0;
        return;                                         /* 0x2BFE6 -> 0x2C04E */
    }
    {
        const u8 *s = game_string_get(0x47u);
        if ((DSB(DS_000EF6DC) & 0x20u) != 0)
            text_cursor_hold(0xd, row, s, 0u);          /* 0x2C017 */
        else
            text_cells_release(0xd, row, s, 0u);        /* 0x2C049 */
        DSB(DS_00105C04) = 0;                           /* 0x2C04E */
    }
}
