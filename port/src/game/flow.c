/* Port of the top-level game flow. Original addresses are named in comments:
 *   0x1BEC4 game main, 0x20C10 init wrapper, 0x255CC master loop,
 *   0x24C5C per-frame update, 0x11D04 state machine, 0x51F45 surface setup,
 *   0x336C0 palette dirty-list init, 0x1BE30 teardown.
 * Scope (Task 14): the title-screen path is live. Every call owned by a later
 * sub-project is stubbed where it is reached and named in a PORT comment. */
#include "game/flow.h"
#include "game/actors.h"
#include "game/movie.h"
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
 * frame_%04d.raw. */
static int s_title_dump_n = -1;

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

/* PORT: 0x500C4 samples the BIOS shift flags at DAT_00101514+0x2d8. The port
 * has no real-mode BIOS; keyboard input arrives through host_pump() ->
 * input_push(). */
static void input_pump(void) { host_pump(); }

/* ---- title state (state 1, 0x121A0) ------------------------------------ */

/* PORT: 0x4F1E4. Two 0x2EA30 interrupt-lock calls bracket the write; 0x2EA30
 * is inert in the port's single-threaded loop (actors_reset documents the
 * same). */
static void title_input_reset(void)
{
    DSB(DS_00104B15) = 0;                       /* 0x4F1F1 */
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
 * skips instead of touching the word before the table. */
static void title_spawn_row(const u32 *desc, u32 a2, u32 a3)
{
    int slot = 0;
    while (slot < 7 && DSD(DS_00107A1C + (u32)slot * 4u) != 0) slot++;
    if (slot >= 7) return;
    DSD(DS_00107A1C + (u32)slot * 4u) =
        actor_spawn(desc, a2 << 3, 2u, a3 << 3, 0u);   /* 0x38B5D */
}

/* PORT: 0x33904. Iterate the fixed 0x10-stride table at
 * DS_00107608..DS_00107798 (the raw immediates 0x87608/0x87798 are
 * DS-relative), returning the first entry whose +4 word is non-zero, or 0 at
 * the end. */
static u32 title_retire_next(u32 node)
{
    u32 e = node ? node : DS_00107608;
    for (;;) {
        e += 0x10u;
        if (e >= DS_00107798) return 0;
        if (DSD(e + 4) != 0) return e;
    }
}

/* PORT: Task 10's dump hook, the title counterpart of 2b's PR_SMK_DUMP. With
 * PR_TITLE_DUMP set, each presented title frame is written as
 * <dir>/title/frame_%04d.raw RGB24, converted through the live gfx_dac exactly
 * as gfx_present does. PR_TITLE_DUMP_FRAMES caps the run (default 200). There
 * is no phase predicate: Task 10 locates its 96-frame window by content
 * alignment. Exported so the Task 10 driver can dump the frames it drives. */
void game_title_dump_frame(void)
{
    const char *dir = getenv("PR_TITLE_DUMP");
    if (dir == NULL || s_title_dump_n < 0) return;
    const char *cap_s = getenv("PR_TITLE_DUMP_FRAMES");
    long cap = cap_s ? strtol(cap_s, NULL, 0) : 200;
    if (s_title_dump_n >= cap) return;

    char sub[1024];
    snprintf(sub, sizeof sub, "%s/title", dir);
    mkdir(dir, 0777);       /* ignore EEXIST; the same pattern main.c uses */
    mkdir(sub, 0777);
    char path[1200];
    snprintf(path, sizeof path, "%s/frame_%04d.raw", sub, s_title_dump_n);
    FILE *f = fopen(path, "wb");
    if (f != NULL) {
        const u8 *idx = mem + DSD(DS_000E87A4);
        for (u32 i = 0; i < 320u * 200u; i++) {
            const u8 *rgb = gfx_dac[idx[i]];
            fwrite(rgb, 1, 3, f);
        }
        fclose(f);
    }
    s_title_dump_n++;
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
        title_input_reset();                    /* 0x121D9 (0x4F1E4) */
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
        title_spawn_row((const u32 *)(mem + 0x9AC1Cu), 0u, 0u);
        title_spawn_row((const u32 *)(mem + 0x9AC1Cu), 0x2Au, 0u);
        title_spawn_row((const u32 *)(mem + 0x9AC1Cu), 0u, 0x1Eu);
        title_spawn_row((const u32 *)(mem + 0x9AC1Cu), 0x2Au, 0x1Eu);
        /* The EXE's only seed store is 0x20C62 in 0x20C10 (before
         * 0x2D974(0x29)); 0x121A0 itself does not re-seed. The port's
         * game_init() mirrors 0x20C10, and nothing consumes RNG between that
         * seed and these three draws (measured: production --check prints
         * DS_000EF6D8 == 0xABCD here), so the draws are the first three from
         * 0xABCD and land on the Task 1 pin (12, 111, 0) with no title-entry
         * re-seed. No PORT marker: this is the original's own RNG state. */
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
            for (u32 node = title_retire_next(0); node != 0;
                 node = title_retire_next(node)) {              /* 0x123CB */
                if (DSD(node) == 0x3E688u) {                    /* 0x123D6 */
                    /* PORT: 0x13C70 (called 0x123EA) is the 0x13xxx
                     * effect/spawn subsystem, out of this cycle (spec §11). Its
                     * only in-window call is here, on nodes typed &0x3E688. */
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
    /* PORT: FUN_00011000 case 0 (the attract sub-machine's entry) plays the two
     * boot logos through two 0x1C740 calls before it assigns title state 1. The
     * rest of the attract sub-machine is still deferred, so the port plays the
     * logos here and then enters state 1 directly, as before. The names are
     * lowercase while the on-disk files are uppercase; res_load_file's scan
     * matches case-insensitively. A missing or rejected movie is skipped, never
     * fatal. */
    if (!movie_play(s_game_dir, "twi5.smk"))
        fprintf(stderr, "flow: twi5.smk playback failed\n");
    if (!movie_play(s_game_dir, "twg.smk"))
        fprintf(stderr, "flow: twg.smk playback failed\n");
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
    /* PORT: 0x20C5D-0x20CC2: DS_00104528 = 0x2D974(0x29). On the shipped image
     * table32[0x29] (va 0x2D3A4) = 0x1D980 -> count 7, index 102, so 0x2D974
     * returns the four bytes at DS_00105DE0+52..55, all zero. The port pins that
     * 0 (the 0x2D974 save/config record subsystem is out of this cycle) and the
     * three globals 0x20C10 derives from it, so 0x121A0 takes the text branch:
     * DS_00105B3A = (v & 0x100) >> 4, DS_001088D0 = (v & 0xf)*5 + 0x1e,
     * DS_0010452C = (v & 0xf0) >> 4, with v = 0. */
    DSD(DS_00104528) = 0;       /* 0x20C6D */
    DSB(DS_00105B3A) = 0;       /* 0x20C9F */
    DSD(DS_001088D0) = 30;      /* 0x20CB0 */
    DSB(DS_0010452C) = 0;       /* 0x20CC2 */
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
        render_list_sort();                  /* 0x1C3FC */
        render_list();                       /* 0x14328 */
        DSD(DS_00104AF4)++;
        /* PORT: 0x134C0 deferred (scene/narrative). */

        /* The original copies DAT_000E87A4 to the literal VGA aperture 0xA0000
         * here (0x255CC full copy when DS_001014FC != 0, else the 0x501A3
         * dirty-dword blit). PORT: the aperture rule — never write mem[0xA0000];
         * present the index buffer through gfx_present(), which converts via
         * gfx_dac to RGB and hands it to the host. */
        gfx_flush_palette();                 /* 0x1C470 */
        gfx_present(mem + DSD(DS_000E87A4), 320, 200);
        /* PORT: Task 10's PR_TITLE_DUMP hook — one RGB24 file per presented
         * title frame, read before the swap replaces DS_000E87A4 with the back
         * buffer. */
        if (DSW(DS_000F0A64) == 1) game_title_dump_frame();
        DSD(DS_001014FC) = 0;
        swap_buffers();                      /* 0x50188 */

        /* 0x255CC calls 0x5D7DC once per iteration, after the present and swap
         * and before 0x1CF20. */
        rng_step();

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

    /* 0x24C5C's tail calls 0x2A31C here (Format reference I): walk the active
     * list and sync each record's pset before the render table composites. */
    actors_update();                                   /* 0x2A31C */
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
            game_state_title();   /* 0x121A0 */
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
    /* PORT: 0x10DB0 and 0x10E18 (0x11D04's tail, run after every state's
     * function, including 0x121A0): both gate on DS_000F0A71 == 0 and two bits
     * of the input state DS_001088D8, then latch DS_000F0A71. With no input
     * those bits stay zero and neither branch is taken (spec §7). Deferred to
     * 4b with that evidence.
     * PORT: 0x2BF08 (same tail, every state): early-returns unless
     * (DS_000EF6DC & 0x1F) == 0, i.e. frames 32/64/96 inside the pinned window;
     * on those it runs the 0x1C500 -> 0x474E4 string-cursor tick and pset
     * housekeeping. Spec §7 hypothesis: none of it reaches DS_000E87A4 unless a
     * message is active, and DS_00105C00 is set only from 0x11F28 on menu
     * input. Detectable signature: falsified iff the Task 10 oracle drifts at
     * exactly frames 32, 64, 96 and nowhere else; the named fallback absorbs
     * 0x2BF08 and the 0x1C500/0x474E4/0x1E75C chain. */
}
