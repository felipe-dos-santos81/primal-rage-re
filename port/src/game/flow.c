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

/* 0x29B74 — derivation record §42-E. The DS_00104AE4 countdown handler (the
 * dispatchers 0x4F2B0/0x4F318/0x4F6E8/0x4F704/0x4F9A0/0x4F9C8 call it when
 * DS_00104AFE runs out). EAX is never read: 0x13DF0 comes first and the walk
 * starts from `xor eax,eax`. */
void frontend_darken_all(void)
{
    effects_clear();                                    /* 0x29B77 0x13DF0 */
    u32 e = frontend_list_next(0u);                     /* 0x29B7E */
    while (e != 0u) {                                   /* 0x29B85/0x29BA0 */
        (void)effects_spawn_darken(e, 3u);              /* 0x29B90 0x13D4C */
        e = frontend_list_next(e);                      /* 0x29B97 */
    }
    DSW(DS_001088EE) = 0x78u;                           /* 0x29BAC */
    DSW(DS_00104AFE) = 0x78u;                           /* 0x29BB3 */
    DSW(DS_00104B00) = 0x15u;                           /* 0x29BBA */
}

/* 0x41578 — derivation record §42-E. Darken only the list entries whose +0
 * resource handle is 0x0003E688 (s16slabs.gra, index 0) or 0x088874B0
 * (s16win.gra, index 0x11, offset 0x874B0), close the play-time audit, and arm
 * the next countdown. Direct-called only (0x41755 in 0x416D4; 0x41DE6,
 * 0x42337, 0x42352 in 0x41C28). */
void frontend_darken_marked(void)
{
    u32 e = frontend_list_next(0u);                     /* 0x4157E */
    while (e != 0u) {                                   /* 0x41585/0x415B2 */
        u32 h = DSD(e);                                 /* 0x41589 */
        if (h == 0x0003E688u || h == 0x088874B0u)       /* 0x4158B/0x41593 */
            (void)effects_spawn_darken(e, 2u);          /* 0x415A2 0x13D4C */
        e = frontend_list_next(e);                      /* 0x415A9 */
    }
    config_play_time_close(DSD(DS_00104ABC), DSB(DS_00104B19));  /* 0x415CD */
    /* PORT: 0x415DC 0x2C3FC(0x33, EDX = 0x78) voice, not wired (record §45-A).
     * EBX (0), ECX (0x13), EDX (0x78) and ESI (0x15) survive 0x32A3C (pushes
     * EBX/ECX/ESI) and 0x2C3FC (pushes EBX/EDX/EDI, never names ECX/ESI)
     * into the stores below. */
    DSW(DS_00104AFE) = 0x78u;                           /* 0x415E1 */
    DSW(DS_001088EE) = 0u;                              /* 0x415E8 */
    DSW(DS_00104AFA) = 0x13u;                           /* 0x415EF */
    DSW(DS_00104B00) = 0x15u;                           /* 0x415F8 */
    DSB(DS_00104B25) = 0u;                              /* 0x415FF (AH = 0) */
}

/* ---- the mode 0x1A/0x1B wipe and the DS_00104AE4 hook (record §43-B) ---- */

#define DS_000C98F4 0x000C98F4u   /* no symbols.h name: the wipe-in descriptor */
#define DS_000C9908 0x000C9908u   /* no symbols.h name: the wipe-out descriptor */
#define DS_000C991C 0x000C991Cu   /* no symbols.h name: 17 wipe-in sprite words */
#define DS_000C993E 0x000C993Eu   /* no symbols.h name: 17 wipe-out sprite words */

/* 0x4F980 — record §43-B. Arm mode 0x1A: the wipe counter DS_001088F5 = 0
 * (0x4F983), the return mode DS_00104AFA = AX (0x4F98E, a word) and the mode
 * DS_00104B00 = 0x1A (0x4F994, a word). EDX is pushed and popped. */
void frontend_wipe_arm(u32 ret_mode)
{
    DSB(DS_001088F5) = 0u;                              /* 0x4F983 */
    DSW(DS_00104AFA) = (u16)ret_mode;                   /* 0x4F98E */
    DSW(DS_00104B00) = 0x1Au;                           /* 0x4F994 */
}

/* 0x28D68 — record §43-B. A DS_00104AE4 hook itself, stored by 0x42CB4 (at
 * 0x42D04/0x42D40/0x42D8F) and by the unreferenced stub 0x42FB0 (at 0x42FBD;
 * no rel32 or dword enters it): the hook becomes 0x43738 (0x28D73) and 0x4F980
 * arms mode 0x1A with the return mode 0x10 (0x28D79). */
void frontend_char_screen_hook(void)
{
    DSD(DS_00104AE4) = FN_00043738;                     /* 0x28D73 */
    frontend_wipe_arm(0x10u);                           /* 0x28D79 0x4F980 */
}

/* 0x28D80 — record §43-B. 0x28D68 with the voice first; stored as the hook by
 * 0x28DA4 (0x28E4E/0x28E60). EDX (0x43738) survives 0x2C3FC, which pushes and
 * pops EBX, EDX and EDI (record §42-E.2). */
void frontend_char_screen_hook_voice(void)
{
    /* PORT: 0x28D8B 0x2C3FC(0x2E) voice, not wired (record §45-A). */
    DSD(DS_00104AE4) = FN_00043738;                     /* 0x28D95 */
    frontend_wipe_arm(0x10u);                           /* 0x28D9B 0x4F980 */
}

/* 0x4F9E4 — record §43-B. One wipe-in frame. The first frame (DS_000C98F0 ==
 * 0) spawns 0xC98F4 (a2 = EDX = 0, a3 = 0xFF, a4 = EBX = 0, a5 = the pushed
 * EDX = 0) into DS_000C98F0 and resyncs the frame counter DS_0010150C to the
 * tick DS_00101508. While the signed counter byte DS_001088F5 (0x4FA13 `mov
 * eax,[0x1088f2]; sar eax,0x18`) is <= 0x10 (`jle`), 0x10D70 gives the record
 * the sprite word 0xC991C[counter] (zero-extended, 0x4FA64), the counter is
 * incremented (stored at 0x4FA72, before the call) and 0 returns. Past 0x10
 * the record is killed, DS_000C98F0 = 0, 0x2BAF4 runs with EAX = 1, the render
 * gate DS_001088F4 = 1 and 1 returns. */
u32 frontend_wipe_in(void)
{
    if (DSD(DS_000C98F0) == 0u) {                       /* 0x4F9EE */
        DSD(DS_000C98F0) = actor_spawn((const u32 *)(mem + DS_000C98F4),
                                       0u, 0xFFu, 0u, 0u);  /* 0x4F9FF/0x4FA04 */
        DSD(DS_0010150C) = DSD(DS_00101508);            /* 0x4FA09/0x4FA0E */
    }
    s8 n = (s8)DSB(DS_001088F5);                        /* 0x4FA13/0x4FA18 */
    if ((s32)n > 0x10) {                                /* 0x4FA1B/0x4FA1E */
        actor_set_dead(DSD(DS_000C98F0));               /* 0x4FA25 0x2B150 */
        DSD(DS_000C98F0) = 0u;                          /* 0x4FA31 */
        actors_reset();                                 /* 0x4FA37 0x2BAF4 (eax = 1) */
        DSB(DS_001088F4) = 1u;                          /* 0x4FA3C */
        return 1u;                                      /* 0x4FA43 */
    }
    u32 rec = DSD(DS_000C98F0);                         /* 0x4FA52 */
    u32 word = DSW(DS_000C991C + (u32)((s32)n * 2));    /* 0x4FA5C/0x4FA64 */
    DSB(DS_001088F5) = (u8)(n + 1);                     /* 0x4FA6A/0x4FA72 */
    actor_pset_word_set(rec, word);                     /* 0x4FA79 0x10D70 */
    return 0u;                                          /* 0x4FA7E */
}

/* 0x4FA88 — record §43-B. One wipe-out frame: 0x4F9E4 with the descriptor
 * 0xC9908 and the words 0xC993E, the render gate DS_001088F4 cleared on the
 * spawn frame only (0x4FAB9, inside the DS_000C98F0 == 0 arm), and no 0x2BAF4
 * and no gate store when the counter passes 0x10. */
u32 frontend_wipe_out(void)
{
    if (DSD(DS_000C98F0) == 0u) {                       /* 0x4FA92 */
        DSD(DS_000C98F0) = actor_spawn((const u32 *)(mem + DS_000C9908),
                                       0u, 0xFFu, 0u, 0u);  /* 0x4FAA3/0x4FAA8 */
        DSD(DS_0010150C) = DSD(DS_00101508);            /* 0x4FAAD/0x4FAB2 */
        DSB(DS_001088F4) = 0u;                          /* 0x4FAB9 */
    }
    s8 n = (s8)DSB(DS_001088F5);                        /* 0x4FABF/0x4FAC4 */
    if ((s32)n > 0x10) {                                /* 0x4FAC7/0x4FACA */
        actor_set_dead(DSD(DS_000C98F0));               /* 0x4FAD1 0x2B150 */
        DSD(DS_000C98F0) = 0u;                          /* 0x4FADD */
        return 1u;                                      /* 0x4FAD8 */
    }
    u32 rec = DSD(DS_000C98F0);                         /* 0x4FAED */
    u32 word = DSW(DS_000C993E + (u32)((s32)n * 2));    /* 0x4FAF7/0x4FAFF */
    DSB(DS_001088F5) = (u8)(n + 1);                     /* 0x4FB05/0x4FB0D */
    actor_pset_word_set(rec, word);                     /* 0x4FB14 0x10D70 */
    return 0u;                                          /* 0x4FB19 */
}

/* 0x4F9A0 — record §43-B. The mode 0x1A handler (0x24C5C case 0x1A, the jump
 * table 0x24B8C entry 0x25403): once 0x4F9E4 reports the wipe done, the hook
 * runs, the counter DS_001088F5 = 0 (AH after `xor ah,ah`) and the mode
 * becomes 0x1B (a word). */
void frontend_mode_1a_step(void)
{
    if (frontend_wipe_in() == 0u) return;               /* 0x4F9A1/0x4F9A8 */
    /* PORT: `call dword [0x104ae4]` goes through the registry and a miss is
     * skipped. A miss is a no-op only for 0x29D60 (a bare `ret`, stored by
     * 0x43738/0x444C8) and 0x5D812 (`xor eax,eax; ret`, record §42-E.4). The
     * five hooks 0x4F980's own callers install just before arming mode 0x1A
     * (0x430E8, 0x4367C, 0x25BBC, 0x26998, 0x270BC) and 0x430C0, which
     * 0x430E8 installs for this call's 0x1B twin, are registered (record
     * §46-B), and so are the 7 other non-trivial values the image stores
     * there (record §46-F): 0x259CC, 0x10E80, 0x24B54, 0x27134, 0x4142C,
     * 0x25AE8 and 0x26978, mode 0x17's hooks.
     * TODO(verify): game_frame dispatches cases 0x1A/0x1B (record §47-B), so
     * a miss on any value but the two no-ops is a missing port, not a skip;
     * no unregistered value is known. The returned EAX is
     * dead: `xor ah,ah` and byte/word stores of AH/DX follow. */
    void (*hook)(void) = fn_resolve(DSD(DS_00104AE4));
    if (hook != NULL) hook();                           /* 0x4F9AA */
    DSB(DS_001088F5) = 0u;                              /* 0x4F9B7 */
    DSW(DS_00104B00) = 0x1Bu;                           /* 0x4F9BD */
}

/* 0x4F9C8 — record §43-B. The mode 0x1B handler (entry 0x2540A): once 0x4FA88
 * reports the wipe done, the hook runs again and the mode takes the word
 * DS_00104AFA that 0x4F980 saved. */
void frontend_mode_1b_step(void)
{
    if (frontend_wipe_out() == 0u) return;              /* 0x4F9C8/0x4F9CF */
    /* PORT: the registry call of 0x4F9A0, with the same misses: only
     * 0x29D60/0x5D812 are no-ops, and every other value the image stores is
     * registered (records §46-B, §46-F). EAX is overwritten by the 0x4F9D7
     * load. */
    void (*hook)(void) = fn_resolve(DSD(DS_00104AE4));
    if (hook != NULL) hook();                           /* 0x4F9D1 */
    DSW(DS_00104B00) = DSW(DS_00104AFA);                /* 0x4F9D7/0x4F9DD */
}

/* ---- the mode-0x1A hooks 0x25BBC/0x26998/0x270BC and their callees (§46-B) */

#define DS_000A87C4 0x000A87C4u   /* no symbols.h name: 7 stage-offset bytes */
#define DS_0010810D 0x0010810Du   /* no symbols.h name: the high byte of DS_0010810A */
/* 0x104B1A: the slot index 0x26998 sets and the mode-0x22/0x23 tail latches
 * (0x25552 `mov al,[0x104b1a]`); symbols.h has no name for it. */
#define DS_00104B1A 0x00104B1Au

/* 0x4F200 — record §46-B. EAX = v: DS_00107A55 = (u8)v (0x4F20A), and
 * DS_00107A54 = DH after `and eax,0xff; mov edx,eax; xor dh,ah`, which is 0
 * (0x4F20F); then 0x4F1D0 and 0x2BAF4 with EAX = 1. EDX is pushed and popped.
 * Only caller: 0x430F9 (0x430E8, EAX = 0). */
void flow_screen_reset(u32 v)
{
    DSB(DS_00107A55) = (u8)v;                           /* 0x4F20A */
    DSB(DS_00107A54) = 0u;                              /* 0x4F20F */
    frontend_origin_zero();                             /* 0x4F215 0x4F1D0 */
    actors_reset();                                     /* 0x4F21F 0x2BAF4 (eax = 1) */
}

/* 0x46504 — record §46-B. DS_001082C0 = DS_001082C8 and DS_001082C4 =
 * DS_001082CC (two dword copies through EAX). Callers: 0x25C0E (0x25BBC) and
 * 0x26A31 (0x26998). */
static void flow_1082c8_latch(void)
{
    DSD(DS_001082C0) = DSD(DS_001082C8);                /* 0x46504/0x46509 */
    DSD(DS_001082C4) = DSD(DS_001082CC);                /* 0x4650E/0x46513 */
}

/* 0x25848 — record §46-B. Picks the stage word DS_00104AFC on the byte
 * DS_00104B17 (0x2584E..0x25865: 0 -> 0x2586B, 2 -> return, 1 and above 2 ->
 * 0x258D2).
 * - 0: with DS_00104B1F == 3 the stage is rng(7) (0x2587C). Otherwise it is
 *   0xA87C4[c] + rng(6) + 1, where c is the signed byte at 0x108169 +
 *   DS_00104B1F (0x2588C `mov eax,[eax+0x108166]; sar eax,0x18`), stored as a
 *   word and reduced once by 7 when the word is >= 7 (0x258B0..0x258C6).
 * - 1 or above 2: a 4-byte frame holds [esp] = DS_0010782A and [esp+1] =
 *   DS_001078BE | 0x40. With n = the zero bytes of DS_00108106[0..6], n != 0
 *   picks the rng(n)-th zero entry (0x258FD..0x25925). Otherwise, or when that
 *   scan runs out, the first entry whose byte & 0x7F is neither frame byte
 *   (0x25927..0x2595B). Failing that, DS_00104AD4 == 2 steps the word by one,
 *   wrapping to 0 at 7 (0x25966..0x25981); otherwise the first entry whose byte
 *   & 0x7F equals [esp + (DS_00104AD4 ^ 1)] (0x2598F..0x259C1).
 * EAX is not a result. Callers: 0x430FE (0x430E8), 0x4177D, 0x418A8,
 * 0x42390. */
void flow_stage_pick(void)
{
    u8 m = DSB(DS_00104B17);                            /* 0x2584E */
    if (m == 0u) {                                      /* 0x25867 */
        u32 b1f = DSB(DS_00104B1F);                     /* 0x2586D */
        if (b1f == 3u) {                                /* 0x25872 */
            DSW(DS_00104AFC) = (u16)rng_next(7u);       /* 0x2587C/0x25881 */
            return;
        }
        s32 c = (s8)DSB(DS_00108169 + b1f);             /* 0x2588C/0x25892 */
        u32 dl = DSB(DS_000A87C4 + (u32)c);             /* 0x25897 */
        DSW(DS_00104AFC) = (u16)(rng_next(6u) + dl + 1u);   /* 0x258A2..0x258AA */
        u32 w = DSW(DS_00104AFC);                       /* 0x258B2 */
        if (w >= 7u) DSW(DS_00104AFC) = (u16)(w - 7u);  /* 0x258B8..0x258C6 */
        return;
    }
    if (m == 2u) return;                                /* 0x2585D/0x2585F */
    u8 fr[2];
    fr[0] = DSB(DS_0010782A);                           /* 0x258D2/0x258D7 */
    fr[1] = (u8)(DSB(DS_001078BE) | 0x40u);             /* 0x258DA..0x258E3 */
    u32 n = 0;
    for (u32 i = 0; i < 7u; i++)                        /* 0x258E9..0x258F7 */
        if (DSB(DS_00108106 + i) == 0u) n++;
    if (n != 0u) {                                      /* 0x258F9 */
        u32 r = rng_next(n);                            /* 0x258FD */
        for (u32 i = 0; i < 7u; i++) {                  /* 0x25904..0x25925 */
            if (DSB(DS_00108106 + i) != 0u) continue;
            if (--r == 0xFFFFFFFFu) {                   /* 0x2590D/0x2590E */
                DSW(DS_00104AFC) = (u16)i;              /* 0x25913 */
                return;
            }
        }
    }
    for (u32 i = 0; i < 7u; i++) {                      /* 0x25927..0x2595B */
        u32 a = DSB(DS_00108106 + i) & 0x7Fu;
        if (a != fr[0] && a != fr[1]) {                 /* 0x2593B/0x25945 */
            DSW(DS_00104AFC) = (u16)i;                  /* 0x25949 */
            return;
        }
    }
    if (DSD(DS_00104AD4) == 2u) {                       /* 0x2595D */
        u16 b = (u16)(DSW(DS_00104AFC) + 1u);           /* 0x25966..0x2596F */
        DSW(DS_00104AFC) = b;                           /* 0x25973 */
        if (b >= 7u) DSW(DS_00104AFC) = 0u;             /* 0x2597A/0x25981 */
        return;
    }
    u32 k = DSD(DS_00104AD4) ^ 1u;                      /* 0x2598F/0x25997 */
    /* PORT: the raw reads [esp + k] of its 4-byte frame. DS_00104AD4's
     * writers store only -1, 0, 1 or 2 (-1 at 0x25A0E and 0x27C3D), and 2
     * took the step above, so k is 1, 0 or -2. For -2 the raw reads [esp-2],
     * a leftover stack byte below the frame, and stores only when that byte
     * equals one of the two frame bytes. The port does not model the stack,
     * so it returns, leaving DS_00104AFC unchanged as the raw's no-match exit
     * (0x259C3) does. */
    if (k > 1u) return;
    for (u32 i = 0; i < 7u; i++) {                      /* 0x2599A..0x259C1 */
        if ((u32)(DSB(DS_00108106 + i) & 0x7Fu) == fr[k]) {   /* 0x259AB */
            DSW(DS_00104AFC) = (u16)i;                  /* 0x259AF */
            return;
        }
    }
}

/* 0x25BBC — record §46-B. A DS_00104AE4 hook, stored at 0x25A46/0x25A73
 * (before 0x4F980 arms mode 0x1A with 0x30/5) and at 0x285CB/0x285F3 (in
 * 0x28468). The byte DS_001078FA = 0 and DS_00104B13 = 0 (AH), DS_00104B1E
 * += 1, then 0x20DF4 on the stage word with EDX = 1, both fighters (0x33EB4
 * with EAX = 0 and 1), 0x4F714 on the stage word, 0x46504, and the hook
 * becomes 0x5D812 (the EDX 0x25C04 loaded, which 0x4F714's tail jump into
 * 0x2C3FC and 0x46504 leave alone). */
void game_hook_25bbc(void)
{
    u8 r = (u8)(DSB(DS_00104B1E) + 1u);                 /* 0x25BBF/0x25BCB */
    DSB(DS_001078FA) = 0u;                              /* 0x25BC5 */
    DSB(DS_00104B13) = 0u;                              /* 0x25BCD */
    DSB(DS_00104B1E) = r;                               /* 0x25BD3 */
    game_fight_reset(DSW(DS_00104AFC), 1u);             /* 0x25BE6 0x20DF4 */
    fighter_spawn(0u);                                  /* 0x25BED 0x33EB4 */
    fighter_spawn(1u);                                  /* 0x25BF7 0x33EB4 */
    /* PORT: 0x25C09 0x4F714(stage) — `mov ax,[eax*2+0xc9888]; and
     * eax,0xffff; jmp 0x2C3FC`, the stage's voice (0x20, 0x21, 0x1B, 0x1C,
     * 0x1E, 0x1D, 0x1F, 0x1F) — voice, not wired (record §45-A). */
    flow_1082c8_latch();                                /* 0x25C0E 0x46504 */
    DSD(DS_00104AE4) = FN_0005D812;                     /* 0x25C13 */
}

/* 0x26998 — record §46-B. A DS_00104AE4 hook, stored at 0x2698B (0x26978,
 * before 0x4F980 at 0x26991). The byte DS_001078FA = 0, DS_00104B1E += 1,
 * 0x20DF4 on the stage word with EDX = 1; then the side byte DS_00104B1A = 0
 * when DS_001078A7 != 0, else 1, and 0x33EB4 spawns that side. The side's
 * byte at 0x10780B + side * 0x94 grows by 2 (a byte add) and is capped at
 * 0x78 (0x26A16 `cmp edx,0x78; jle` on the zero-extended byte). Then the 0x28
 * voice, 0x46504, and the hook becomes 0x5D812. */
void game_hook_26998(void)
{
    u8 r = (u8)(DSB(DS_00104B1E) + 1u);                 /* 0x2699B/0x269A3 */
    DSB(DS_001078FA) = 0u;                              /* 0x269A5 */
    DSB(DS_00104B1E) = r;                               /* 0x269AB */
    game_fight_reset(DSW(DS_00104AFC), 1u);             /* 0x269BE 0x20DF4 */
    u32 side;
    if (DSB(DS_001078A7) != 0u) {                       /* 0x269C3 */
        DSB(DS_00104B1A) = 0u;                          /* 0x269D0 */
        side = 0u;                                      /* 0x269CE */
    } else {
        DSB(DS_00104B1A) = 1u;                          /* 0x269DF */
        side = 1u;                                      /* 0x269DA */
    }
    fighter_spawn(side);                                /* 0x269E5 0x33EB4 */
    u32 a = DS_0010780B + (u32)DSB(DS_00104B1A) * 0x94u;    /* 0x269EC..0x26A00 */
    u8 v = (u8)(DSB(a) + 2u);                           /* 0x26A03/0x26A0B */
    DSB(a) = v;                                         /* 0x26A10 */
    if (v > 0x78u) DSB(a) = 0x78u;                      /* 0x26A16/0x26A1B */
    /* PORT: 0x26A2C 0x2C3FC(0x28) voice, not wired (record §45-A). EDX
     * (0x5D812, 0x26A27) survives it and 0x46504. */
    flow_1082c8_latch();                                /* 0x26A31 0x46504 */
    DSD(DS_00104AE4) = FN_0005D812;                     /* 0x26A36 */
}

/* 0x270BC — record §46-B. A DS_00104AE4 hook, stored at 0x2715C (0x27134,
 * before 0x4F980 at 0x27162). The byte DS_001078FA = 0, DS_00104B1E += 1,
 * 0x20DF4 on the stage word with EDX = 1, then 0x33EB4 on the signed byte
 * DS_0010810D (0x270E5 `mov eax,[0x10810a]; sar eax,0x18`), the 0x25 voice,
 * DS_00104B0A = 0 (DH after `xor dh,dh`, which 0x2C3FC preserves), and
 * DS_00104B0B = the byte at 0x10780B + that side * 0x94. The hook becomes
 * 0x5D812. */
void game_hook_270bc(void)
{
    u8 r = (u8)(DSB(DS_00104B1E) + 1u);                 /* 0x270BD/0x270C5 */
    DSB(DS_001078FA) = 0u;                              /* 0x270C7 */
    DSB(DS_00104B1E) = r;                               /* 0x270CD */
    game_fight_reset(DSW(DS_00104AFC), 1u);             /* 0x270E0 0x20DF4 */
    fighter_spawn((u32)(s32)(s8)DSB(DS_0010810D));      /* 0x270E5..0x270ED 0x33EB4 */
    /* PORT: 0x270F9 0x2C3FC(0x25) voice, not wired (record §45-A). */
    DSB(DS_00104B0A) = 0u;                              /* 0x270FE */
    u32 side = (u32)(s32)(s8)DSB(DS_0010810D);          /* 0x27104/0x2710A */
    DSB(DS_00104B0B) = DSB(DS_0010780B + side * 0x94u); /* 0x2711B/0x27127 */
    DSD(DS_00104AE4) = FN_0005D812;                     /* 0x2712C */
}

/* ---- the remaining DS_00104AE4 values (record §46-F) --------------------- */

#define FN_00025BBC 0x00025BBCu   /* no symbols.h name: game_hook_25bbc */
#define FN_00026998 0x00026998u   /* no symbols.h name: game_hook_26998 */
#define FN_000270BC 0x000270BCu   /* no symbols.h name: game_hook_270bc */
#define FN_00024B54 0x00024B54u   /* no symbols.h name: game_hook_24b54 */

/* 0x259CC — record §46-F. A DS_00104AE4 hook, stored at 0x253AE (mode 0x11's
 * case 0x2538F), 0x417A5 (0x41760), 0x418D5 (0x41878, no Ghidra function)
 * and 0x423C4 (0x41C28), each with mode 0x17, whose 0x4F318 calls it when the countdown runs out. The stage's
 * byte DS_00108106[DS_00104AFC] = 0 (DL), DS_00104B1E = 0; then, around
 * 0x2D974(0x29), the bytes DS_00104AF3/DS_00104AF2 = 0 (DL), DS_00104B14 = 0
 * (CL) and DS_00104AD4 = -1 (EBX, 0x259D2); DS_00104AC8 = 0 and DS_00104ADC =
 * ((field & 0x300000) >> 20) * 2 + 1 (`sar eax,0x14; add eax,eax; inc eax`).
 * The hook becomes 0x25BBC and DS_00104B25 = 1 on both arms; with DS_00104B1D
 * == 3 (CH, read at 0x25A17) 0x4F980 arms mode 0x1A with 0x30 and then
 * DS_00104B14 = 1 (DL) and DS_00104B21 = 0 (CL, kept by 0x4F980, which pushes
 * only EDX); otherwise with 5. 0x2D974 pushes EBX/ECX/EDX/ESI. */
void game_hook_259cc(void)
{
    u32 stage = DSW(DS_00104AFC);                       /* 0x259D7 */
    DSB(DS_00108106 + stage) = 0u;                      /* 0x259E1 */
    DSB(DS_00104B1E) = 0u;                              /* 0x259EC */
    u32 v = config_field_get(0x29u) & 0x300000u;        /* 0x259F2 0x2D974, 0x259F7 */
    DSB(DS_00104AF3) = 0u;                              /* 0x259FC */
    DSB(DS_00104AF2) = 0u;                              /* 0x25A02 */
    DSB(DS_00104B14) = 0u;                              /* 0x25A08 */
    DSD(DS_00104AD4) = 0xFFFFFFFFu;                     /* 0x25A0E */
    u8 mode = DSB(DS_00104B1D);                         /* 0x25A17 */
    DSD(DS_00104AC8) = 0u;                              /* 0x25A22 */
    DSD(DS_00104ADC) = (v >> 20) * 2u + 1u;             /* 0x25A14..0x25A21, 0x25A28 */
    if (mode == 3u) {                                   /* 0x25A2D */
        DSB(DS_00104B25) = 1u;                          /* 0x25A3B */
        DSD(DS_00104AE4) = FN_00025BBC;                 /* 0x25A46 */
        frontend_wipe_arm(0x30u);                       /* 0x25A4C 0x4F980 */
        DSB(DS_00104B14) = 1u;                          /* 0x25A51 */
        DSB(DS_00104B21) = 0u;                          /* 0x25A57 */
        return;
    }
    DSB(DS_00104B25) = 1u;                              /* 0x25A69 */
    DSD(DS_00104AE4) = FN_00025BBC;                     /* 0x25A73 */
    frontend_wipe_arm(5u);                              /* 0x25A79 0x4F980 */
}

/* 0x26978 — record §46-F. A DS_00104AE4 hook, stored at 0x41854 (0x417C4)
 * with mode 0x17. DS_00104B25 = 1, the hook becomes 0x26998 and 0x4F980 arms
 * mode 0x1A with 0x23. */
void game_hook_26978(void)
{
    DSB(DS_00104B25) = 1u;                              /* 0x26980 */
    DSD(DS_00104AE4) = FN_00026998;                     /* 0x2698B */
    frontend_wipe_arm(0x23u);                           /* 0x26991 0x4F980 */
}

/* 0x27134 — record §46-F. A DS_00104AE4 hook, stored at 0x27074 (0x26F58)
 * and 0x27233 (0x271E0), each with mode 0x17. The bytes DS_00104B1E,
 * DS_00104AF3 and DS_00104AF2 = 0 (AH), DS_00104B25 = 1 (BL), the hook
 * becomes 0x270BC and 0x4F980 arms mode 0x1A with 5. */
void game_hook_27134(void)
{
    DSB(DS_00104B1E) = 0u;                              /* 0x2713F */
    DSB(DS_00104AF3) = 0u;                              /* 0x27145 */
    DSB(DS_00104AF2) = 0u;                              /* 0x2714B */
    DSB(DS_00104B25) = 1u;                              /* 0x27151 */
    DSD(DS_00104AE4) = FN_000270BC;                     /* 0x2715C */
    frontend_wipe_arm(5u);                              /* 0x27162 0x4F980 */
}

/* 0x24B54 — record §46-F. A DS_00104AE4 hook, stored only by 0x25AE8
 * (0x25B97, the DS_00104B1D != 0 arm) with mode 0x17. 0x4F1E4, 0x2BAF4 with
 * EAX = 1, the mode word DS_00104B00 = DX = 0x27 (loaded at 0x24B61 and kept
 * by 0x2BAF4, which pushes EDX), the bytes DS_00104B1D and DS_00104B1F = 0
 * (AH) and the dword DS_00104AB8 = 0. */
void game_hook_24b54(void)
{
    frontend_input_reset();                             /* 0x24B57 0x4F1E4 */
    actors_reset();                                     /* 0x24B66 0x2BAF4 (eax = 1) */
    DSW(DS_00104B00) = 0x27u;                           /* 0x24B6D */
    DSB(DS_00104B1D) = 0u;                              /* 0x24B74 */
    DSB(DS_00104B1F) = 0u;                              /* 0x24B7C */
    DSD(DS_00104AB8) = 0u;                              /* 0x24B82 */
}

/* 0x25AE8 — record §46-F. The mode-0x14 handler (0x24C5C case 0x14, table
 * entry 0x253D9 calls it) and a DS_00104AE4 hook, stored at 0x296AF (0x29638)
 * and 0x42ED9 (0x42CB4), each with mode 0x17. The 0x100 voice, the byte
 * DS_00104B14 = 0 (AH), 0x4F1E4, 0x2BAF4 with EAX = 1, DS_00104ABC =
 * (DS_00104B1F == 3) + 1 (`cmp eax,3; sete al; inc eax`), string 0x52 drawn
 * by 0x2F510 at col -1, row 0xE with mode 0x4000 (ECX, set at 0x25B22 and
 * kept by 0x1C500 and 0x474E4, which push it), 0x4246C and the 0x3D voice.
 * Then mode 0x17 with the countdown words DS_001088EE = DS_00104AFE = 0xB4,
 * and the hook becomes 0x10E80 when DS_00104B1D == 0, else 0x24B54. */
void game_hook_25ae8(void)
{
    /* PORT: 0x25AF1 0x2C3FC(0x100) voice, not wired (record §45-A). */
    DSB(DS_00104B14) = 0u;                              /* 0x25AF8 */
    frontend_input_reset();                             /* 0x25B00 0x4F1E4 */
    actors_reset();                                     /* 0x25B0A 0x2BAF4 (eax = 1) */
    DSD(DS_00104ABC) = (DSB(DS_00104B1F) == 3u ? 1u : 0u) + 1u;   /* 0x25B11..0x25B27 */
    text_cursor_hold_font2(-1, 0xE, game_string_get(0x52u),
                           0x4000u);                    /* 0x25B36 0x1C500, 0x25B42 0x2F510 */
    fight_stage_marks_clear();                        /* 0x25B47 0x4246C */
    /* PORT: 0x25B51 0x2C3FC(0x3D) voice, not wired (record §45-A). */
    if (DSB(DS_00104B1D) == 0u) {                       /* 0x25B56 */
        DSD(DS_00104AE4) = FN_00010E80;                 /* 0x25B6E */
        DSW(DS_001088EE) = 0xB4u;                       /* 0x25B74 */
        DSW(DS_00104AFE) = 0xB4u;                       /* 0x25B7B */
        DSW(DS_00104B00) = 0x17u;                       /* 0x25B82 */
        return;
    }
    DSD(DS_00104AE4) = FN_00024B54;                     /* 0x25B97 */
    DSW(DS_00104B00) = 0x17u;                           /* 0x25BA2 */
    DSW(DS_00104AFE) = 0xB4u;                           /* 0x25BA9 */
    DSW(DS_001088EE) = 0xB4u;                           /* 0x25BB0 */
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
        /* PORT: 0x2C3FC(0x100) voice, not wired (record §45-A). */
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
        /* PORT: 0x2C3FC(0x100) voice, not wired (record §45-A). */
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
        /* PORT: 0x2C3FC(0x100) voice, not wired (record §45-A). */
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

/* ---- 0x20DF4 the fight reset (record §46-B) ------------------------------ */

/* 0x2C390 — record §46-B. Self-links the two sentinels 0x105C0C (0x2C3A2/
 * 0x2C3A8) and 0x105C14 (0x2C3AE/0x2C3B4), then tail-appends the sixteen
 * 0x14-byte nodes 0x105C1C..0x105D48 to the 0x105C14 list (0x2C3C2 `mov
 * eax,0x105c14` / 0x2C3C7 `mov edx,ebx` / 0x2C3C9 `add ebx,0x14` / 0x2C3CC
 * `call 0x249c0`, while EBX < 0x105D5C). Callers: 0x20E2E (in 0x20DF4) and
 * 0x20ED5. EBX/ECX/EDX are pushed and popped. */
static void flow_list_105c0c_init(void)
{
    DSD(DS_00105C10) = DS_00105C0C;                     /* 0x2C3A2 */
    DSD(DS_00105C0C) = DS_00105C0C;                     /* 0x2C3A8 */
    DSD(DS_00105C18) = DS_00105C14;                     /* 0x2C3AE */
    DSD(DS_00105C14) = DS_00105C14;                     /* 0x2C3B4 */
    for (u32 node = DS_00105C1C; node < DS_00105D5C; node += 0x14u)  /* 0x2C3BA/0x2C3D1 */
        effects_list_insert_before(DS_00105C14, node);  /* 0x2C3CC 0x249C0 */
}

/* 0x2C074 — record §46-B. DS_00105BF4 = DS_00105BF0 = 0 (EDX pushed and
 * popped). Callers: 0x20E47 (in 0x20DF4) and 0x20EEE. */
static void flow_105bf0_clear(void)
{
    DSD(DS_00105BF4) = 0u;                              /* 0x2C077 */
    DSD(DS_00105BF0) = 0u;                              /* 0x2C07D */
}

/* 0x20DF4 — record §46-B. The fight reset. EAX = the stage (DS_00104AFC's
 * zero-extended word at every caller), clamped to 7 by a signed `cmp eax,7;
 * jl` (0x20E01/0x20E06) into EBX; EDX = `full`, read at 0x20E6F. EDX survives
 * every pre-branch callee (0x2C390/0x12750/0x49300/0x28E98/0x34978/0x2C074
 * push and pop it; 0x29B70 is a bare `ret`, 0x12C70 does not name it), so the
 * test reads the caller's EDX. EBX/ECX/ESI are pushed and popped; EAX is not
 * a result.
 * - 0x12750 builds the type-0x01 node lists without which 0x1282C's spawn is
 *   refused (demo record §15); 0x49300 self-links the fight-effect sentinel
 *   DS_0010884C that the 0x49C78 walk reads; 0x28E98 builds the type-0x0A/0x19
 *   lists (record §41-D); 0x34978 restarts the live-fighter count DS_001078FA
 *   (record §38).
 * - The full branch: 0x2BAF4 with EAX = 1 (actors_reset), 0x38730 and 0x412A0
 *   on the clamped stage (0x20E7D/0x20E84 `mov eax,ebx`).
 * Callers: 0x11AC4 (state 6), 0x25A95, 0x25BE6 (0x25BBC), 0x269BE (0x26998),
 * 0x270E0 (0x270BC) and 0x295E4. */
void game_fight_reset(u32 stage, u32 full)
{
    DSD(DS_000F0A48) = 0u;                              /* 0x20DFB */
    u32 s = (s32)stage < 7 ? stage : 7u;                /* 0x20DF7/0x20E01/0x20E06 */
    /* 0x20E0B 0x29B70 is a bare `ret`. */
    DSD(DS_00100B4C) = 0u;                              /* 0x20E16 */
    DSD(DS_00104AE8) = 0u;                              /* 0x20E1C */
    DSB(DS_001088EC) = 0u;                              /* 0x20E22 */
    DSB(DS_00104B15) = 0u;                              /* 0x20E28 */
    flow_list_105c0c_init();                            /* 0x20E2E 0x2C390 */
    camera_dust_list_init();                            /* 0x20E33 0x12750 */
    fight_list_init();                                  /* 0x20E38 0x49300 */
    actor_type_0a19_list_init();                        /* 0x20E3D 0x28E98 */
    fighter_slots_reset();                              /* 0x20E42 0x34978 */
    flow_105bf0_clear();                                /* 0x20E47 0x2C074 */
    DSD(DS_000F0AEC) = 0u;                              /* 0x20E4C */
    DSD(DS_000F0AF0) = 0u;                              /* 0x20E52 */
    DSW(DS_000F0AFA) = 0u;                              /* 0x20E5C */
    DSW(DS_000F0AF8) = 0u;                              /* 0x20E63 */
    camera_step_seed();                                 /* 0x20E6A 0x12C70 */
    if (full == 0u) return;                             /* 0x20E6F/0x20E71 */
    actors_reset();                                     /* 0x20E78 0x2BAF4 (eax = 1) */
    render_scroll_setup(s);                             /* 0x20E7F 0x38730 */
    fight_scene_props(s);                               /* 0x20E86 0x412A0 */
}

/* 0x11A8C. State 6: the demo-fight setup. It picks two random characters from
 * the shared RNG stream and arms the 900-frame state-7 timer.
 *
 * RNG DRAW ORDER (Task 7 pins the shared stream): draw1 = rng(7) at 0x11AAD also
 * sets DS_00104AFC and P0's character; draw2 = rng(6) at 0x11AE9 gives P1 the
 * character (draw1 + draw2) % 7. Exactly two draws, in that order. */
static void game_state_6(void)
{
    /* PORT: 0x2C3FC(0x100) voice, not wired (record §45-A). */
    /* 0x29D60 is a ret-only no-op. */
    config_set_credit_row(0x1Du);                       /* 0x11AA3 0x2C06C */

    u32 draw1 = rng_next(7u);                           /* 0x11AAD (draw 1) */
    DSW(DS_00104AFC) = (u16)draw1;                      /* 0x11AB4 */
    /* 0x11ABA..0x11AC1: EAX = the zero-extended word of the draw, EDX = 1, so
     * 0x20DF4 takes its full-reset branch (record §46-B). */
    game_fight_reset(draw1, 1u);                        /* 0x11AC4 0x20DF4 */
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

/* 0x1E918 — record §46-A. EAX = force. Writes the ten 0x2C-byte factory
 * records at 0xA7BBC into table 0 through 0x2DCA0: each record whose value
 * reads 0, or every record when forced. */
void hiscore_fill_defaults(u32 force)
{
    u32 src = 0xA7BBCu;                                         /* 0x1E920 */
    for (u32 i = 0u; i < 10u; i++) {                            /* 0x1E925..0x1E94C */
        u32 r = hiscore_read(i, 0u);                            /* 0x1E92D 0x2DBC4 */
        if (DSD(r) == 0u || force != 0u)                        /* 0x1E932/0x1E936 */
            (void)hiscore_insert(i, src, 0u);                   /* 0x1E940 0x2DCA0 */
        src += 0x2Cu;                                           /* 0x1E946 */
    }
}

/* 0x1E988 — record §46-A. AL = 1 when config field 0x27 >= 2000 and field
 * 0x26 >= 200 (both `jl`, signed). */
u32 hiscore_audit_reset_due(void)
{
    if ((s32)config_field_get(0x27u) < 0x7D0) return 0u;        /* 0x1E98D/0x1E992 */
    if ((s32)config_field_get(0x26u) < 0xC8) return 0u;         /* 0x1E99E/0x1E9A3 */
    return 1u;                                                  /* 0x1E9AA */
}

/* 0x1E824 — record §46-A. The high-score init, called from 0x20C10 at 0x20C84
 * after DS_00104528 is read. Blanks three 0x24-byte name buffers, then fills
 * the factory table 0 and, when table 1 is empty, the champion 0xA7D74. With
 * DS_00104529 bit 0x40, or bit 0x20 and 0x1E988, it clears fields 0x27/0x26,
 * forces the defaults and, only when bit 0x40 was set, clears it back into
 * field 0x29 and writes the champion over any value (bit 0x40 clear returns
 * at 0x1E8B9 -> 0x1E912 with the champion kept). */
void hiscore_init(void)
{
    for (u32 k = 0u; k < 2u; k++)                               /* 0x1E82B..0x1E856 */
        for (u32 i = 0u; i < 0x24u; i++)
            DSB(DS_00104367 + k * 0xA0u + i) = 0x20u;           /* 0x1E83B */
    for (u32 i = 0u; i < 0x24u; i++)                            /* 0x1E85E..0x1E86D */
        DSB(DS_001042C7 + i) = 0x20u;                           /* 0x1E85F */
    u8 ah = DSB(DS_00104528 + 1u);                              /* 0x1E86F */
    if ((ah & 0x40u) == 0u
        && ((ah & 0x20u) == 0u || hiscore_audit_reset_due() != 1u)) {   /* 0x1E875..0x1E88C */
        hiscore_fill_defaults(0u);                              /* 0x1E8F0 0x1E918 */
        u32 r = hiscore_read(0u, 1u);                           /* 0x1E8F7 0x2DBC4 */
        if (DSD(r) != 0u) return;                               /* 0x1E8FC/0x1E8FF */
    } else {
        (void)config_field_set(0x27u, 0u);                      /* 0x1E895 0x2DA0C */
        (void)config_field_set(0x26u, 0u);                      /* 0x1E8A1 0x2DA0C */
        hiscore_fill_defaults(1u);                              /* 0x1E8AB 0x1E918 */
        u8 dh = DSB(DS_00104528 + 1u);                          /* 0x1E8B0 */
        if ((dh & 0x40u) == 0u) return;                         /* 0x1E8B6/0x1E8B9 */
        DSB(DS_00104528 + 1u) = (u8)(dh & 0xBFu);               /* 0x1E8BD/0x1E8C0 */
        (void)config_field_set(0x29u, DSD(DS_00104528));        /* 0x1E8D1 0x2DA0C */
        (void)hiscore_read(0u, 1u);                             /* 0x1E8E2 0x2DBC4 */
    }
    (void)hiscore_insert(0u, 0xA7D74u, 1u);                     /* 0x1E90D 0x2DCA0 */
}

/* 0x1EA08 — record §46-A. The attract's high-score screen, called inline from
 * state 5. It resets the input latch and the actor pool, spawns the backdrop
 * row from the 0xA7B6C descriptor, draws the champion (table 1) on row 2 and
 * table 0's records 1..9 at the 0xA7B94 layout (row, rank column, name column,
 * value column), then spawns the champion's figure: the first of the ten
 * 0xA7DA0 strings whose first byte is the name's character 0x12 selects the
 * 0xA7DCC descriptor (0 when none matches or above 6). 0x1EA08's other four
 * callers (0x11A30 and three in FUN_0001EEB0, the match sub-state machine)
 * belong to the match cycle and stay unwired. */
void frontend_match_start(void)
{
    u8 name[0x24];

    frontend_input_reset();                                     /* 0x1EA11 (0x4F1E4) */
    actors_reset();                                             /* 0x1EA26 (0x2BAF4) */
    frontend_spawn_row((const u32 *)(mem + 0xA7B6Cu), 0u, 0u);  /* 0x1EA30 (0x38B18) */
    const u32 mode = 0x3000u;                                   /* 0x1EA35/0x1EA3C [esp+0x24] */
    u32 champ = hiscore_read(0u, 1u);                           /* 0x1EA4A 0x2DBC4 */
    text_number_draw(DSB(0xA7B95u), 2, 1, 2, 1u, mode);         /* 0x1EA66 0x2F4D0 (row = ECX 2) */
    for (u32 i = 0u; i < 0x12u; i++)                            /* 0x1EA74..0x1EA8D */
        name[i] = DSB(champ + 4u + i);
    name[0x12] = 0u;                                            /* 0x1EA98 */
    text_cursor_hold(DSB(0xA7B96u), 2, name, 0x2000u);          /* 0x1EAA8 0x2F4BC */
    text_number_draw(DSB(0xA7B97u), 2, (s32)DSD(champ), 7, 1u, 0x2000u);   /* 0x1EAC7 0x2F4D0 */

    u32 sel = 0u;                                               /* 0x1EAD0 */
    for (u32 i = 0u; i < 10u; i++) {                            /* 0x1EAD9..0x1EB02 */
        if (DSB(champ + 0x16u) == DSB(DSD(DS_000A7DA0 + i * 4u))) {  /* 0x1EADB..0x1EAE7 */
            sel = i;                                            /* 0x1EAED */
            break;
        }
    }
    if (sel > 6u) sel = 0u;                                     /* 0x1EB0A..0x1EB11 */

    for (u32 i = 1u; i < 10u; i++) {                            /* 0x1EB15..0x1EBF8 */
        u32 r = hiscore_read(i, 0u);                            /* 0x1EB2F 0x2DBC4 */
        s32 row = DSB(0xA7B94u + i * 4u);
        text_number_draw(DSB(0xA7B95u + i * 4u), row, (s32)(i + 1u), 2, 1u, mode);  /* 0x1EB5A */
        for (u32 k = 0u; k < 3u; k++)                           /* 0x1EB68..0x1EB81 */
            name[k] = DSB(r + 4u + k);
        name[3] = 0u;                                           /* 0x1EB92 */
        text_cursor_hold(DSB(0xA7B96u + i * 4u), row, name, mode);            /* 0x1EBA6 0x2F4BC */
        text_number_draw(DSB(0xA7B97u + i * 4u), row, (s32)DSD(r), 7, 1u, mode);   /* 0x1EBC7 */
        text_cursor_hold(DSB(0xA7B96u + i * 4u), row, name, mode);            /* 0x1EBE2 0x2F4BC */
    }

    u32 rec = actor_spawn((const u32 *)(mem + DSD(0xA7DCCu + sel * 4u)),   /* 0x1EC15 */
                          0x2A00u, 0xFFu, 0x1C80u, 0u);         /* 0x1EC1C 0x2AE14 */
    /* PORT: 0x1EC28 passes the spawn's EAX unchecked; the pool was reset at
     * 0x1EA26, so the spawn cannot fail, and the port does not index a pset
     * from a zero record. */
    if (rec != 0u)
        actor_pset_palette(rec, 0u, 0x105FD30u);                /* 0x1EC28 0x2A17C */
}

/* 0x10E80 — record §46-F. Initialise the game state: 0x20C10's last call
 * (0x20CE6), and a DS_00104AE4 hook, stored at 0x25B6E (0x25AE8, the
 * DS_00104B1D == 0 arm) with mode 0x17; 0x24C5C tests the hook against it at
 * 0x24E09. It enters state 0 (the 0x11000 attract sub-machine), which plays
 * the two boot logos in phase 0 (through movie_play) and hands off to title
 * state 1 when DS_000F0A5C wraps to 0. The row-1 credit value comes from
 * attract phase 2's 0x2C06C(1), not a stand-in. In order: 0x32970(0),
 * 0x4F1E4, 0x2BAF4 with EAX = 1, the mode word DS_00104B00 = DX = 3 (loaded
 * at 0x10E8B and kept by 0x4F1E4 and 0x2BAF4, which push EDX), the word
 * DS_000F0A64 = BX = 0, the byte DS_000F0A71 = 0 (AH), the dword DS_000F0A5C =
 * 4, the byte DS_000F0A6F = 0 (DL), 0x2C304, the dword DS_00104AB8 = 0 (EBX)
 * and the byte DS_00104B1F = 0 (DH). */
void game_state_init(void)
{
    /* PORT: 0x10E84 0x32970(EAX = 0), the run clock, is out of scope (spec
     * §7); the host clock owns wall time. */
    frontend_input_reset();                             /* 0x10E90 0x4F1E4 */
    actors_reset();                                     /* 0x10E9C 0x2BAF4 (eax = 1) */
    DSW(DS_00104B00) = 3u;                              /* 0x10EA1 */
    DSW(DS_000F0A64) = 0u;                              /* 0x10EA8 */
    DSB(DS_000F0A71) = 0u;                              /* 0x10EB6 */
    DSD(DS_000F0A5C) = 4u;                              /* 0x10EBC */
    DSB(DS_000F0A6F) = 0u;                              /* 0x10EC6 */
    config_credits_init();                              /* 0x10ECC 0x2C304 */
    DSD(DS_00104AB8) = 0u;                              /* 0x10ED3 */
    DSB(DS_00104B1F) = 0u;                              /* 0x10ED9 */
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
    /* 0x1CF8E: the DIG driver handle is stored at DS_001028C8, which every
     * sample path tests (0x1CEBC, 0x1CC28, 0x1CE70, 0x1CE04, 0x1CD9C).
     * PORT: the handle is the port's host object (ail.c) and does not fit a
     * mem[] dword, so the port stores 1 as its non-zero stand-in. Every reader
     * tests it against zero; the one call that passes it on, 0x5DBCB below,
     * the port makes with its own handle. 0x1D0BC, which zeroes it when no
     * sample buffer can be allocated, is not ported (see game_init). */
    DSD(DS_001028C8) = (dig != NULL) ? 1u : 0u;
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

/* ---- the sound module (0x1CA14..0x1D244) and the voice dispatcher 0x2C3FC --
 *
 * The four sample slots are 0x18-byte records at DS_00102860: +0x00 the AIL
 * sample handle (0x1CF40), +0x04 the queued resource handle (0x1CC28), +0x08
 * its loop byte, +0x0C the playing resource handle (0x1CB18), +0x10 the slot's
 * buffer (0x1D0BC), +0x14 the queue time (0x500BB). DS_001028C8 is the DIG
 * driver handle, DS_001028C0 the MDI sequence handle, DS_001028DB the sample
 * pause byte (0x1D220), DS_001028DA the music one.
 * PORT: the AIL handles are the port's host objects (flow.c's s_samples and
 * s_sequence, in 0x1CF40's allocation order), so a slot's +0x00 is not in
 * mem[]: slot i's handle is s_samples[i]. The port stores 1 for DS_001028C8
 * (game_audio_init) and leaves DS_001028C0/C4 at 0: its music is started by
 * s_music_request, not through DS_001028CC, so the dispatcher's music arms
 * (0x1CA14's store, 0x1CA40's status) stay inert, as without an MDI driver.
 * Named gap (spec §7): 0x1CC28's slot choice and 0x1CB18's start (the sample
 * copy into the slot's 0x1D0BC buffer, which the port does not allocate), so
 * no port path writes a slot's +0x04/+0x0C/+0x14. */

#define SND_SLOT_STRIDE 0x18u
#define SND_SLOT_END    0x60u
#define SND_VOICE_REC   0x0Cu

/* 0x1D238. Clears the music pause byte DS_001028DA (0x1D23A). */
static void snd_music_unpause(void)
{
    DSB(DS_001028DA) = 0;                                  /* 0x1D23A */
}

/* 0x1D244. Clears the sample pause byte DS_001028DB (0x1D246). */
static void snd_sample_unpause(void)
{
    DSB(DS_001028DB) = 0;                                  /* 0x1D246 */
}

/* 0x1CA14. EAX = the song handle, DL = its byte. Stores both as the current
 * song (DS_001028D4/DS_001028D9); unless the music is paused (DS_001028DA ==
 * 1) or there is no sequence handle (DS_001028C0), it becomes the pending
 * song DS_001028CC and AL = 1. */
static u32 snd_music_request(u32 song, u32 b)
{
    DSB(DS_001028D9) = (u8)b;                              /* 0x1CA14 */
    DSD(DS_001028D4) = song;                               /* 0x1CA22 */
    if (DSB(DS_001028DA) == 1u) return 0;                  /* 0x1CA27 */
    if (DSD(DS_001028C0) == 0u) return 0;                  /* 0x1CA2C */
    DSD(DS_001028CC) = song;                               /* 0x1CA35 */
    return 1;
}

/* 0x1CA40. AL = 1 when the sequence DS_001028C0 plays (0x5DEED status 4).
 * PORT: the sequence handle is s_sequence; DS_001028C0 is 0 in the port, so
 * the status arm runs only when a caller has stored one. */
static u32 snd_music_playing(void)
{
    if (DSD(DS_001028C0) == 0u) return 0;                  /* 0x1CA40 */
    return AIL_sequence_status(s_sequence) == 4 ? 1u : 0u; /* 0x5DEED */
}

/* 0x1CA6C. Clears the current song (DS_001028D4 = 0, DS_001028D9 = 0); when
 * the sequence plays, the pending song DS_001028CC = 0 and 0x5DEAF stops it
 * (AL = 1). The caller's EAX/EDX are passed to 0x1CA40, which reads neither. */
static u32 snd_music_stop(void)
{
    DSD(DS_001028D4) = 0;                                  /* 0x1CA7A */
    DSB(DS_001028D9) = 0;                                  /* 0x1CA80 */
    if (DSD(DS_001028C0) == 0u) return 0;                  /* 0x1CA86 */
    if (snd_music_playing() == 0u) return 0;               /* 0x1CA8A */
    DSD(DS_001028CC) = 0;                                  /* 0x1CAA1 */
    AIL_stop_sequence(s_sequence);                         /* 0x1CAA7 0x5DEAF */
    return 1;
}

struct AIL_SAMPLE *sound_slot_handle(u32 i)
{
    return (i < 4u) ? s_samples[i] : NULL;
}

/* 0x5DD03 on slot `off`'s handle (s_samples[off / 0x18]). */
static s32 snd_slot_status(u32 off)
{
    return AIL_sample_status(s_samples[off / SND_SLOT_STRIDE]);
}

/* 0x1CE70. AL = 1 when a slot plays the resource handle `h` (its +0x0C is
 * `h` and 0x5DD03 reports 4); a slot whose +0x0C is `h` but has stopped gets
 * +0x0C = 0 and the scan goes on. AL = 0 without a DIG driver. */
static u32 snd_sample_playing(u32 h)
{
    if (DSD(DS_001028C8) == 0u) return 0;                  /* 0x1CE78 */
    for (u32 off = 0; off < SND_SLOT_END; off += SND_SLOT_STRIDE) {
        if (DSD(DS_0010286C + off) != h) continue;         /* 0x1CE83 */
        if (snd_slot_status(off) == 4) return 1;           /* 0x1CE92/0x1CE9A */
        DSD(DS_0010286C + off) = 0;                        /* 0x1CEA5 */
    }
    return 0;
}

/* 0x1CE04. Stops the first slot playing `h`: a slot whose +0x0C is `h` and
 * whose 0x5DD03 status is not 2 is ended (0x5DC8B) and re-inited (0x5DC0F),
 * its +0x0C = 0, AL = 1. AL = 0 when none (or no DIG driver). */
static u32 snd_sample_stop(u32 h)
{
    if (DSD(DS_001028C8) == 0u) return 0;                  /* 0x1CE0C */
    for (u32 off = 0; off < SND_SLOT_END; off += SND_SLOT_STRIDE) {
        if (DSD(DS_0010286C + off) != h) continue;         /* 0x1CE17 */
        if (snd_slot_status(off) == 2) continue;           /* 0x1CE26/0x1CE2E */
        AIL_stop_sample(s_samples[off / SND_SLOT_STRIDE]); /* 0x1CE3A 0x5DC8B */
        AIL_init_sample(s_samples[off / SND_SLOT_STRIDE]); /* 0x1CE49 0x5DC0F */
        DSD(DS_0010286C + off) = 0;                        /* 0x1CE53 */
        return 1;
    }
    return 0;
}

/* 0x1CD9C. Without a DIG driver AL = 0. Otherwise every slot's +0x04 and
 * +0x0C are cleared and a slot whose status is not 2 is ended and re-inited;
 * AL = 1. */
static u32 snd_samples_stop_all(void)
{
    if (DSD(DS_001028C8) == 0u) return 0;                  /* 0x1CDA2 */
    for (u32 off = 0; off < SND_SLOT_END; off += SND_SLOT_STRIDE) {
        DSD(DS_00102864 + off) = 0;                        /* 0x1CDB5 */
        DSD(DS_0010286C + off) = 0;                        /* 0x1CDBC */
        if (snd_slot_status(off) == 2) continue;           /* 0x1CDC2/0x1CDCA */
        AIL_stop_sample(s_samples[off / SND_SLOT_STRIDE]); /* 0x1CDD6 0x5DC8B */
        AIL_init_sample(s_samples[off / SND_SLOT_STRIDE]); /* 0x1CDE5 0x5DC0F */
    }
    return 1;
}

/* 0x1CC28. EAX = the resource handle of a sample, DL = its loop byte. Without
 * a DIG driver (DS_001028C8) or while samples are paused (DS_001028DB) AL = 0
 * and nothing is read. Otherwise it reads the time (0x500BB) and resolves the
 * handle through 0x1B544 (0x1CC5D): the resolve is what reads a sound bank the
 * first time a voice names it (the loader's `- LOADING -` screen, record
 * §45-A). It then queues the sample on a slot (AL = 1).
 * PORT: the slot choice (0x1CC62..0x1CD8D: a free slot for a sample of at most
 * 0x6000 bytes, else slot 0 or the oldest, ended and re-inited, then +0x04 =
 * `h`, +0x08 = the loop byte, +0x14 = the time) is the named gap above; its
 * result, AL = 1, is kept. */
static u32 snd_sample_queue(u32 h, u32 loop)
{
    (void)loop;
    if (DSD(DS_001028C8) == 0u) return 0;                  /* 0x1CC37 */
    if (DSB(DS_001028DB) != 0u) return 0;                  /* 0x1CC44 */
    (void)res_resolve(h);                                  /* 0x1CC5D 0x1B544 */
    return 1;
}

/* 0x2C3FC — the voice dispatcher. EAX = the voice id (0 does nothing; 0x100
 * is id 0's record); the record is the 12-byte DS_000BBDC8[id]: +0 the case,
 * +4 a handle, +8 a byte. Cases (jump table 0x2C3E0): 0 nothing; 1 the handle
 * becomes the current voice DS_00105D5C and a music request (0x1CA14); 2 the
 * sample is queued unless it plays; 3 the paired samples of ids 0x46, 0x4D and
 * 0x5D; 4 a restart of the music and the samples; 5 the stops; 6 and above
 * nothing. AL = 1, or 0 for id 0, a case above 5 (case 6's 0x2C8E8 and the
 * `ja`), a playing sample in cases 2/3, or an unlisted case-3 id. EBX, EDX and
 * EDI are preserved; the callers read AL at most. */
u32 sound_voice(u32 id)
{
    if (id == 0u) return 0;                                /* 0x2C401 */
    if (id == 0x100u) id = 0;                              /* 0x2C409/0x2C410 */
    u32 rec = DS_000BBDC8 + id * SND_VOICE_REC;            /* 0x2C412..0x2C41B */
    u32 h = DSD(rec + 4u);
    u32 b = DSB(rec + 8u);
    switch (DSB(rec)) {                                    /* 0x2C422/0x2C42F */
    case 0:                                                /* 0x2C8DD */
        return 1;
    case 1:                                                /* 0x2C437 */
        DSD(DS_00105D5C) = h;                              /* 0x2C447 */
        snd_music_request(DSD(DS_00105D5C), b);            /* 0x2C463 0x1CA14 */
        return 1;
    case 2:                                                /* 0x2C473 */
        if (snd_sample_playing(h) != 0u) return 0;         /* 0x2C483/0x2C48A */
        snd_sample_queue(h, b);                            /* 0x2C4B6 0x1CC28 */
        return 1;
    case 3:                                                /* 0x2C4C6 */
        if (id == 0x46u) {                                 /* 0x2C4D7 */
            if (snd_sample_playing(0x2886158u) != 0u) return 0;   /* 0x2C4E5 */
            snd_sample_queue(0x28847C9u, 0);               /* 0x2C4F9 */
            snd_sample_queue(0x2886158u, 0);               /* 0x2C505 */
            return 1;
        }
        if (id == 0x4Du) {                                 /* 0x2C4CB */
            if (snd_sample_playing(0x1201D606u) != 0u) return 0;  /* 0x2C51A */
            snd_sample_queue(0x1201D606u, 0);              /* 0x2C52E */
            snd_sample_queue(0x2001513Cu, 0);              /* 0x2C53A */
            return 1;
        }
        if (id == 0x5Du) {                                 /* 0x2C4CD */
            if (snd_sample_playing(0x281A726u) != 0u) return 0;   /* 0x2C54F */
            snd_sample_queue(0x281A726u, 0);               /* 0x2C563 */
            snd_sample_queue(0x2819183u, 0);               /* 0x2C56F */
            return 1;
        }
        return 0;                                          /* 0x2C8E8 */
    case 4:                                                /* 0x2C89B */
        snd_music_unpause();                               /* 0x2C89B 0x1D238 */
        snd_sample_unpause();                              /* 0x2C8A0 0x1D244 */
        DSD(DS_00105D5C) = 0x21u;                          /* 0x2C8AC */
        snd_music_request(0x2803E64u, 0);                  /* 0x2C8B6 0x1CA14 */
        if (snd_sample_playing(0x180122FDu) == 0u) {       /* 0x2C8C0/0x2C8C7 */
            snd_samples_stop_all();                        /* 0x2C8C9 0x1CD9C */
            snd_sample_queue(0x180122FDu, 1);              /* 0x2C8D8 */
        }
        return 1;
    case 5: {                                              /* 0x2C57F */
        u32 cur = DSD(DS_00105D5C);
        switch (id) {
        case 0x00:                                         /* 0x2C694/0x2C69C */
            snd_music_stop();                              /* 0x1CA6C */
            snd_samples_stop_all();                        /* 0x2C6A1 0x1CD9C */
            break;
        case 0x22:                                         /* 0x2C6B1..0x2C6E2 */
            if ((cur >= 0x1Bu && cur <= 0x21u) || cur == 0x25u || cur == 0x26u)
                snd_music_stop();                          /* 0x2C6E8 */
            break;
        case 0x2B: if (cur == 0x2Au) snd_music_stop(); break;        /* 0x2C6F8 */
        case 0x2D: if (cur == 0x2Cu) snd_music_stop(); break;        /* 0x2C715 */
        case 0x2F:                                         /* 0x2C732 */
            if (cur == 0x2Eu || cur == 0x30u) snd_music_stop();
            break;
        case 0x33: if (cur == 0x32u) snd_music_stop(); break;        /* 0x2C756 */
        case 0x3C: if (cur == 0x3Bu) snd_music_stop(); break;        /* 0x2C773 */
        case 0x3F: snd_sample_stop(0x1800EBC9u); break;              /* 0x2C790 */
        case 0x41: snd_sample_stop(0x383B6F4u); break;               /* 0x2C7A5 */
        case 0x43: snd_sample_stop(0x3837440u); break;               /* 0x2C7BA */
        case 0x4C: snd_sample_stop(0x22008696u); break;              /* 0x2C7CF */
        case 0x4F: snd_sample_stop(0x1501053Cu); break;              /* 0x2C7E4 */
        case 0x55: if (cur == 0x54u) snd_music_stop(); break;        /* 0x2C7F9 */
        case 0x57: if (cur == 0x56u) snd_music_stop(); break;        /* 0x2C816 */
        case 0x5B: snd_sample_stop(0x1B01AF00u); break;              /* 0x2C82F */
        case 0xE0: if (cur == 0xDFu) snd_music_stop(); break;        /* 0x2C844 */
        case 0xE2:                                         /* 0x2C860 */
            if (cur == 0xE1u || cur == 0xE3u) snd_music_stop();
            break;
        case 0xF1: snd_sample_stop(0x22018405u); break;              /* 0x2C886 */
        default: break;                                    /* 0x2C890 */
        }
        return 1;
    }
    default:                                               /* 0x2C8E8, `ja` */
        return 0;
    }
}

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

    /* PORT: the DOS/4GW loader maps both LE objects before 0x1BEC4 runs. The
     * port reimplements the code object but the data object at DATA_BASE holds
     * the title descriptors and tables 0x121A0 reads; res_load_index only loads
     * the INDEX resources, so map the image here. The resource heap starts at
     * RES_HEAP (= the data object's end), so the two regions never overlap.
     * The image is mapped first, as the loader does: mapped after the chain
     * below, it overwrote that chain's stores with the image's zeros
     * (DS_00101504/10/14, DS_000A2CAC, 0x1CF40's DS_000A2CB1 and DS_001028C8;
     * record §45-A). */
    {
        char exe_path[512];
        snprintf(exe_path, sizeof exe_path, "%s/PRAGE.EXE", s_game_dir);
        if (!mem_load_le(exe_path, NULL)) {
            game_fatal("PRAGE.EXE image load failed");
            return;
        }
    }

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
    /* The actor and pset pools are the two allocations res_load_index performs
     * (0x1B120's DS_001014EC/DS_001014F4); actors_init validates them. */
    if (!actors_init()) {
        game_fatal("actor pool allocation missing");
        return;
    }
    surface_setup();        /* 0x51F45 */
    palette_list_init();    /* 0x336C0 */
    render_list_init();     /* 0x1C350 */
    render_projection_reset(0u);    /* 0x20C47 `xor eax,eax`, 0x20C49 0x4F228 */
    rng_seed(0xABCDu);      /* PORT: 0x20C10 seeds the LCG with a hardcoded 0xABCD. */
    /* PORT: 0x20C5D-0x20CC2: DS_00104528 = 0x2D974(0x29) and the three globals
     * derived from v. The master init 0x2F9CC (0x20C15) runs 0x13ADC
     * (effects_init, inside actors_init above) then 0x2D6F8 (config_validate)
     * before this block, so on a fresh image v is what the defaults path wrote. */
    config_validate();          /* 0x2F9CC's 0x2D6F8, before 0x20C5D */
    u32 v = config_field_get(0x29u);                   /* 0x20C68 */
    DSD(DS_00104528) = v;                              /* 0x20C6D */
    /* 0x20C84 0x1E824. The raw runs 0x47370 (the string table, loaded below
     * in the port) first; 0x1E824 reads neither it nor the globals derived
     * next. */
    hiscore_init();                                    /* 0x20C84 */
    DSB(DS_00105B3A) = (u8)((v & 0x100u) >> 4);        /* 0x20C9F */
    DSD(DS_001088D0) = (v & 0xFu) * 5u + 0x1Eu;        /* 0x20CB0 */
    DSB(DS_0010452C) = (u8)((v & 0xF0u) >> 4);         /* 0x20CC2 */
    /* PORT: 0x2BF08's captured inputs (docs/superpowers/plans/
     * 2026-09-18-bf08-overlay-diagnosis.md §2.3). DS_00105C00 is the live credit
     * counter the overlay renders as `<CREDITS string>:<n>`; the un-pinned title
     * capture shows 5, derived by 0x2C304's config read (config_credits_init,
     * which game_state_init calls at 0x10ECC, record §46-F). Its
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
        DSD(DS_001028C8) = 0;          /* 0x1D0A9, after the voices stop */
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

#define FN_000259CC 0x000259CCu   /* no symbols.h name: mode 0x11's hook */

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
    /* 0x24CCD..0x24CDB: the frame counter is a word (`mov di,[0xef6dc]` /
     * `inc edi` / `mov [0xef6dc],di`); 0xEF6DE is a separate global
     * (0x1BE21/0x5D808), so the increment must not carry into it. */
    DSW(DS_000EF6DC) = (u16)(DSW(DS_000EF6DC) + 1u);   /* 0x24CDB */
    run_process_table(DS_000A8644, DSD(DS_00104AE8));  /* update table */
    /* PORT: 0x24C5C's second 0x38990 per-frame service call is deferred. */

    /* PORT: 0x24CFE..0x24EE7, the int 16h keyboard loop, is not ported (only
     * its ESC quit arm, in game_loop). It runs before the switch and is one of
     * the two ways out of mode 3: Enter in mode 3 stores mode 0x27 (0x24EE0,
     * the start menu), and 0x11D04's coin/start arm calls the unported 0x257A4
     * (record §47-B). */

    /* 0x24EEC..0x24F01: the mode switch, on the word DS_00104B00 (`mov
     * ax,[0x104b00]; cmp ax,0x33; ja 0x2540F; and eax,0xffff; jmp
     * [eax*4+0x24B8C]`). Every case ends at 0x2540F, the 0x2A31C tail below.
     * Record §47-B lists every entry. No ported path the oracles exercise
     * leaves mode 3 (§47-B.2), so only case 3 runs outside the unit tests. */
    switch (DSW(DS_00104B00)) {
    case 0x01u:
    case 0x02u:
    case 0x20u:
        /* 0x2521A/0x25224/0x2522E call 0x29B70, a bare `ret`. */
        break;
    case 0x03u:
        game_state_step();                             /* 0x25238 0x11D04 */
        break;
    case 0x11u:
        /* 0x2538F: the countdowns DS_00104AFE = 0xF0 (BX) and DS_001088EE = 0
         * (CX), the hook 0x259CC (EDI) and mode 0x17 (SI), all words but the
         * hook. */
        DSW(DS_00104AFE) = 0xF0u;                      /* 0x253A0 */
        DSW(DS_001088EE) = 0u;                         /* 0x253A7 */
        DSD(DS_00104AE4) = FN_000259CC;                /* 0x253AE */
        DSW(DS_00104B00) = 0x17u;                      /* 0x253B4 */
        break;
    case 0x14u:
        game_hook_25ae8();                             /* 0x253D9 0x25AE8 */
        break;
    case 0x1Au:
        frontend_mode_1a_step();                       /* 0x25403 0x4F9A0 */
        break;
    case 0x1Bu:
        frontend_mode_1b_step();                       /* 0x2540A 0x4F9C8 */
        break;
    case 0x04u:
    case 0x05u:
    case 0x06u:
    case 0x07u:
    case 0x08u:
    case 0x09u:
    case 0x0Au:
    case 0x0Bu:
    case 0x0Cu:
    case 0x0Du:
    case 0x0Eu:
    case 0x0Fu:
    case 0x10u:
    case 0x12u:
    case 0x13u:
    case 0x15u:
    case 0x16u:
    case 0x17u:
    case 0x18u:
    case 0x19u:
    case 0x1Eu:
    case 0x1Fu:
    case 0x21u:
    case 0x22u:
    case 0x23u:
    case 0x24u:
    case 0x25u:
    case 0x27u:
    case 0x28u:
    case 0x29u:
    case 0x2Au:
    case 0x2Bu:
    case 0x2Cu:
    case 0x2Du:
    case 0x2Eu:
    case 0x2Fu:
    case 0x30u:
    case 0x31u:
    case 0x32u:
    case 0x33u:
        /* PORT: named gaps, each case's body unported (record §47-B.1 has
         * the entry and callees of every one):
         * 4 0x26254; 5 0x25C88; 6 0x28CC8/0x28DA4 else 0x26254; 7 0x282C4;
         * 8 0x28468; 9 0x28788; 0xA 0x28BD4; 0xB 0x26254 + 0x28C38;
         * 0xC 0x28CC8/0x28DA4 else 0x27380; 0xD 0x274FC; 0xE 0x27A2C;
         * 0xF 0x277C0; 0x10 0x438B4; 0x12 0x41C28; 0x13 0x424E8;
         * 0x15 0x4F24C; 0x16 0x4F2B0; 0x17 0x4F318; 0x18 0x4F6E8;
         * 0x19 0x4F704; 0x1E 0x1EEB0; 0x1F 0x208F8; 0x21 0x26540;
         * 0x22 0x26C8C; 0x23 0x26A50; 0x24 0x26F58; 0x25 inline (0x266AC,
         * 0x4EF8C, 0x4F0FC, 0x49C78); 0x27 inline (0x50146, the 0xBCBDC menu
         * 0x2FFC4, 0x65431 longjmp); 0x28..0x2F inline (0x2D974 field 0x29,
         * 0x2CA7C, 0x257A4); 0x30 0x29328; 0x31 0x299E8; 0x32 0x296B8;
         * 0x33 0x29638. Case 0x17's 0x4F318 is ported on branch gap7-mode17
         * (record §46-G) and is wired here when that branch merges. */
        break;
    case 0x00u:
    case 0x1Cu:
    case 0x1Du:
    case 0x26u:
        break;                                         /* table entries 0x2540F */
    default:
        break;                                         /* 0x24EF6 ja 0x2540F */
    }

    /* 0x24C5C's tail calls 0x2A31C here (Format reference I): walk the active
     * list and sync each record's pset before the render table composites. */
    actors_update();                                   /* 0x2A31C */

    /* 0x24C5C's DS_00104B15 tail (0x25414): the demo fight's post-update. It is
     * armed by state 6 and cleared by the 0x11BCC timer exit. The per-side loop
     * gates on the camera-target table DS_001077A8[side], which 0x33C78 (the
     * fighter spawn) fills. */
    if (DSB(DS_00104B15) != 0) {                       /* 0x25414 */
        (void)fighter_body_push();                     /* 0x2541D 0x3BB90 */
        camera_dispatch();                             /* 0x25422 0x12D48 */
        for (u32 side = 0; side < 2u; side++) {        /* 0x2542D */
            if (DSD(DS_001077A8 + side * 4u) == 0) continue;
            fighter_slot_latch(side);                  /* 0x25438 0x186D0 */
            actor_pset_point(DSD(DS_001077B0 + side * 0x94u));  /* 0x25443 */
        }
        fight_health_bars();                           /* 0x25457 0x33F08 */
    }

    /* 0x2545C: the mode tail (record §42-D), on the word DS_00104B00 (`cmp
     * ax,0x21` / `jc` / `jbe`, `cmp ax,0x23` / `jbe`, `cmp ax,0x25` / `jz`).
     * Every arm ends 0x24C5C. The port's modes (3, and 0x15 once 0x29B74
     * stores it) take none. */
    switch (DSW(DS_00104B00)) {
    case 0x0Cu: {
        /* 0x25487: while the frame word's bit 1 is set (two frames on, two
         * off: frames 2 and 3 mod 4) and the DS_00104B12 slot's +0x41 bit 0
         * is set, its record's pset word is saved to DS_00104AF6 and replaced
         * by 0x1E1 with bit 15 kept. */
        u32 slot = DS_001077B0 + (u32)DSB(DS_00104B12) * 0x94u;     /* 0x25491..0x254A7 */
        if ((DSB(slot + 0x41u) & 1u) == 0u) break;                  /* 0x254AA */
        if ((DSW(DS_000EF6DC) & 2u) == 0u) break;                   /* 0x254B7..0x254C9 */
        u32 ps = DSD(DS_001014EC) + (u32)DSW(DSD(slot) + 0x56u) * 0x20u; /* 0x254CF..0x254E9 */
        DSW(DS_00104AF6) = DSW(ps);                                 /* 0x254EB/0x254EE */
        DSW(ps) = (u16)((DSW(ps) & 0x8000u) | 0x1E1u);              /* 0x254F5..0x25500 */
        break;
    }
    case 0x21u:
        /* 0x25509: the body push, the held pair camera, both slots latched
         * (unconditionally, unlike the 0x25414 tail) with each non-zero
         * record's pset synced, then the health bars. */
        (void)fighter_body_push();                     /* 0x25509 0x3BB90 */
        camera_pair_hold(1u);                          /* 0x25513 0x12FD8 */
        for (u32 side = 0; side < 2u; side++) {        /* 0x2551C..0x2553E */
            fighter_slot_latch(side);                  /* 0x2551E 0x186D0 */
            u32 rec = DSD(DS_001077B0 + side * 0x94u); /* 0x25523 */
            if (rec != 0u) actor_pset_point(rec);      /* 0x2552F 0x2A690 */
        }
        fight_health_bars();                           /* 0x25540 0x33F08 */
        break;
    case 0x22u:
    case 0x23u: {
        /* 0x2554B: the camera dispatch and the DS_00104B1A slot alone. */
        camera_dispatch();                             /* 0x2554B 0x12D48 */
        u32 side = DSB(DS_00104B1A);                   /* 0x25552 */
        fighter_slot_latch(side);                      /* 0x25559 0x186D0 */
        u32 rec = DSD(DS_001077B0 + side * 0x94u);     /* 0x25575 */
        if (rec != 0u) actor_pset_point(rec);          /* 0x25581 0x2A690 */
        fight_health_bars();                           /* 0x25586/0x255C0 0x33F08 */
        break;
    }
    case 0x25u:
        /* 0x25591: as 0x21 without the body push, the camera on the centre. */
        camera_pair_hold(0u);                          /* 0x25593 0x12FD8 */
        for (u32 side = 0; side < 2u; side++) {        /* 0x2559C..0x255BE */
            fighter_slot_latch(side);                  /* 0x2559E 0x186D0 */
            u32 rec = DSD(DS_001077B0 + side * 0x94u); /* 0x255A3 */
            if (rec != 0u) actor_pset_point(rec);      /* 0x255AF 0x2A690 */
        }
        fight_health_bars();                           /* 0x255C0 0x33F08 */
        break;
    default:
        break;
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
            /* PORT: 0x2C3FC(0x100 / ecx = 0x12C) voice/sample cancel, not wired
             * (record §45-A). */
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
                /* PORT: 0x29D60 is a ret-only no-op; 0x2C3FC(0x100) voice, not wired
                 * (record §45-A). */
            } else {
                fight_arena_frame();                    /* 0x11E8F 0x263F4 */
                fight_health_bars();                    /* 0x11E94 0x33F08 */
            }
            break;
        case 8:
            /* PORT: 0x11EAC runs 0x32970(0) (the run clock, spec §7) and
             * 0x257A4(3), the unported game-start divert that leaves mode 3
             * for 0x1A (record §47-B), then the shared tails below. Only
             * attract phase 0xB stores state 8, when DS_00108173 != 0, and
             * no ported code writes that byte. */
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
