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
#include "game/menu.h"
#include "game/nameentry.h"
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

/* 0x1E75C — record §50-D. PORT: lock the paged data handle and return its data offset, or 0
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

/* 0x1E808 — record §50-D. PORT: clear the lock bit. Its second argument feeds 0x500BB (a DPMI
 * page-map query) and a write to [arg+0x10] that the string reader never reads;
 * the port omits both, which is the arm 0x474E4 reaches. */
static void string_unlock(u32 handle) { DSB(handle + 0x15) &= 0xFDu; }

/* 0x474E4 — record §49-Z. Decode string `id` from the localisation table at
 * `base` into `out` (capacity `outlen`). The table's +4 holds a linked list of
 * group offsets relative to `base`; each group is a run of one-byte-length-
 * prefixed entries and an entry's bytes are XORed with its plaintext length
 * byte. Returns the decoded length + 1 (0 for an empty entry) or `outlen` when
 * truncated. PORT: the original locks the DS_001082DC handle itself (0x474F2
 * 0x1E75C) and unlocks it at 0x475A5 (0x1E808); the port's only caller,
 * game_string_get, does both around this body, so `base` arrives locked. The
 * `outlen` compare is signed (0x47556 JGE) and the ids are the callers'
 * constants. */
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
 * DS_00104AFE runs out; 0x27B17, in 0x27A2C, calls it directly, record
 * §48-E). EAX is never read: 0x13DF0 comes first and the walk starts from
 * `xor eax,eax`. EBX/ECX/EDX are pushed and popped. */
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

/* ---- mode 0x12, the post-match "next opponent" screen (record §48-Z) ---- */

#define DS_00104529 0x00104529u   /* no symbols.h name: DS_00104528's second byte */
#define FN_000259CC 0x000259CCu   /* no symbols.h name: mode 0x11's hook */
#define DS_000C8704 0x000C8704u   /* no symbols.h name: state 0's sprite prompt */
#define DS_000C8718 0x000C8718u   /* no symbols.h name: shared prompt-row word */
#define DS_000C871C 0x000C871Cu   /* no symbols.h name: 0x417C4's sprite prompt */
#define DS_000C876C 0x000C876Cu   /* no symbols.h name: 0x4160c's sprite prompt */
#define DS_0000A7760 0x0000A7760u /* no symbols.h name: state 3's per-char actor table */
#define DS_0000A7808 0x0000A7808u /* no symbols.h name: 0x418F4's per-char actor table */
#define DS_000C8780 0x000C8780u   /* no symbols.h name: state 3's centre sprite */
#define DS_000C87A8 0x000C87A8u   /* no symbols.h name: state 3's opponent/stage sprite */
#define DS_000C8794 0x000C8794u   /* no symbols.h name: state 3's third sprite */
#define DS_000C86A4 0x000C86A4u   /* no symbols.h name: per-stage anim-stream table, stride 8 */
#define DS_000C86A8 0x000C86A8u   /* no symbols.h name: per-stage layout-width table, stride 8 */
#define DS_000C866C 0x000C866Cu   /* no symbols.h name: per-char name string-id table */
#define DS_000C8688 0x000C8688u   /* no symbols.h name: per-stage name string-id table */
#define DS_000C85F4 0x000C85F4u   /* no symbols.h name: state 6's per-slot anim-stream table */
#define DS_0000E9286 0x000E9286u  /* no symbols.h name: state 6's background anim stream */
#define DS_000C85A8 0x000C85A8u   /* no symbols.h name: 0x418F4/state 7's "confetti" sprite */
#define DS_000C8730 0x000C8730u   /* no symbols.h name: 0x418F4's fixed sprite (sprite layout) */
#define DS_000C8744 0x000C8744u   /* no symbols.h name: 0x418F4's fixed sprite (sprite layout) */
#define DS_000C8762 0x000C8762u   /* no symbols.h name: 0x418F4's per-side tally column (text layout) */
#define DS_000C8764 0x000C8764u   /* no symbols.h name: 0x418F4/state 3/7's per-side dword/column table */
#define DS_000C875C 0x000C875Cu   /* no symbols.h name: 0x418F4's per-side tally column (sprite layout) */
#define DS_000C875E 0x000C875Eu   /* no symbols.h name: 0x418F4/state 7's per-side score column (sprite layout) */
#define DS_000C85D8 0x000C85D8u   /* no symbols.h name: frontend_portrait_flash's stream table */
#define DS_000C8610 0x000C8610u   /* no symbols.h name: frontend_portrait_flash's palette table */
#define DS_000C85E6 0x000C85E6u   /* no symbols.h name: frontend_portrait_reveal's stream table */
#define DS_000C8648 0x000C8648u   /* no symbols.h name: frontend_portrait_reveal's palette table */
#define FN_00026978 0x00026978u   /* no symbols.h name: game_hook_26978 */

/* 0x414B4 — record §48-Z. Flash portrait slot `i`'s actor: mark its +0x29
 * dirty bit (0x8), re-seek its animation to DS_000C85D8[char*2] (the class
 * DS_00108106[i] & 0xF, or +7 when bit 0x40 is set, indexes the palette
 * table DS_000C8610), and reset its pset palette. EBX/ECX/EDX/ESI pushed and
 * popped. Callers: state 1's scan (0x41D13) and state 4 (0x42096), both in
 * 0x41C28. */
static void frontend_portrait_flash(u32 i)
{
    u32 flags = DSB(DS_00108106 + i);                       /* 0x414B8/0x414C5 */
    u32 cls = flags & 0xFu;                                 /* 0x414C0/0x414C3 */
    u32 bit6 = (flags & 0x40u) >> 6;                        /* 0x414CD..0x414E4 */
    u32 ch = DSB(DS_000C8664 + i);                          /* 0x414D0 */
    u32 rec = DSD(DS_001080C0 + ch * 4u);                   /* 0x414D9 */
    DSB(rec + 0x29u) = (u8)(DSB(rec + 0x29u) | 8u);         /* 0x414E0 */
    u32 stream = DSW(DS_000C85D8 + ch * 2u);                /* 0x414E7/0x414EF */
    actors_anim_seek(rec, stream);                          /* 0x414FC 0x2BCF4 */
    u32 col = bit6 != 0u ? cls + 7u : cls;                  /* 0x41501..0x4150A */
    u32 pal = DSD(DS_000C8610 + col * 4u);                  /* 0x4150A */
    actor_pset_palette(rec, 0u, pal);                       /* 0x4151C 0x2A17C */
}

/* 0x41528 — record §48-Z. 0x414B4 without the class/palette-column split:
 * mark the portrait's +0x29 dirty, re-seek its animation to
 * DS_000C85E6[char*2] and reset its pset palette to DS_000C8648[char].
 * EBX/ECX/EDX pushed and popped. Only caller: state 5's DS_0010810E < 8 arm,
 * 0x42159 (0x41C28). */
static void frontend_portrait_reveal(u32 slot)
{
    u32 ch = DSB(DS_000C8664 + slot);                       /* 0x4152D */
    u32 rec = DSD(DS_001080C0 + ch * 4u);                   /* 0x4153A/0x41552 */
    DSB(rec + 0x29u) = (u8)(DSB(rec + 0x29u) | 8u);         /* 0x41540 */
    u32 stream = DSW(DS_000C85E6 + ch * 2u);                /* 0x41544 */
    actors_anim_seek(rec, stream);                          /* 0x41558 0x2BCF4 */
    u32 pal = DSD(DS_000C8648 + ch * 4u);                   /* 0x4155D */
    actor_pset_palette(rec, 0u, pal);                       /* 0x4156F 0x2A17C */
}

/* 0x4660c — record §48-Z. Recompute the continue timer DS_001082D0 from the
 * difficulty row b = DS_0010452C and column `col` (the same table
 * flow_1082c8_init reads at column 0 for its default): the byte
 * DS_000C9388[b*7 + col], minus (DS_00104B11 - 1) when the continue count
 * DS_00104B11 > 1, clamped to >= 0. EBX/ECX/EDX pushed and popped. Only
 * caller: 0x416D4 (0x41780's 0x41778, and 0x41760's 0x41778 wait — actually
 * called from 0x416D4 at 0x416EC and from 0x41760 at 0x41778). */
static void flow_1082d0_from_column(u32 col)
{
    u32 count = DSB(DS_00104B11);                           /* 0x46613 */
    u32 b = DSB(DS_0010452C);                                /* 0x46620/0x46644 */
    u32 v = DSB(DS_000C9388 + col + b * 7u);                 /* 0x46633/0x46653 */
    s32 r = (count > 1u) ? (s32)v - (s32)(count - 1u) : (s32)v;   /* 0x4661C..0x4665A */
    if (r < 0) r = 0;                                          /* 0x46662/0x46664 */
    DSD(DS_001082D0) = (u32)r;                                 /* 0x46666 */
}

/* 0x4160c — record §48-Z. Draw the round-transition prompt (sprite
 * DS_000C876C at DS_000C8718's word when the sprite flag DS_00104529 bit 1
 * is set, else the strings 0xDD/0xDC erased and drawn at row 2), clear the
 * seven bytes DS_00108106..DS_0010810C, DS_00108111 = 0, DS_00104B23 = 6, a
 * 30-frame countdown (DS_00104AFE = 0x1E), increment the current side's
 * select-cycle field DS_00107832[r*0x94] and schedule state 8
 * (DS_00104B25 = 8). Callers: 0x416D4 (0x4174D) and state 5's
 * DS_00108104[r] == 7 arm, 0x4212C (0x41C28). */
static void frontend_mode12_advance(void)
{
    if ((DSB(DS_00104529) & 2u) != 0u) {                    /* 0x4160F/0x41616 */
        (void)actor_spawn((const u32 *)(mem + DS_000C876C), DSW(DS_000C8718),
                          0xFFu, 0x1200u, 0u);               /* 0x41618..0x41632 0x2AE14 */
    } else {
        text_cells_release(0, 2, game_string_get(0xDDu), 0x4000u);   /* 0x41639..0x41651 0x1C500, 0x2F280 */
        text_cursor_set(-1, 2, game_string_get(0xDCu), 0x4003u);     /* 0x41656..0x41671 0x1C500, 0x2F198 */
    }
    mem_fill(DS_00108106, 0u, 7u);                          /* 0x41676..0x41682 0x65490 */
    DSB(DS_00108111) = 0u;                                  /* 0x41689 */
    u32 r = DSD(DS_00104AD4);                               /* 0x4168F */
    u32 field = DS_00107832 + r * 0x94u;                    /* 0x41695..0x416A3 */
    DSB(DS_00104B23) = 6u;                                  /* 0x416A5 (BL) */
    DSW(DS_00104AFE) = 0x1Eu;                               /* 0x416B7 */
    DSB(field) = (u8)(DSB(field) + 1u);                     /* 0x416B0/0x416BE/0x416C2 */
    DSB(DS_00104B25) = 8u;                                  /* 0x416C9 (DH) */
}

/* 0x416D4 — record §48-Z. Mode 0x12's tally step, called only from state 5
 * of 0x41C28 (0x420FE). With the match result r == 2, frontend_darken_marked
 * and return. Otherwise: flow_1082d0_from_column(DS_00108104[r])
 * recomputes the continue timer; the dead stub 0x32BAC(char[r], char[r^1])
 * is a no-op (raw byte at 0x32BAC is 0xC3, RET — confirmed by `read_memory`,
 * 8 call sites across the image, none observable); then, when
 * DS_00108104[r] == 7, frontend_mode12_advance(), else
 * frontend_darken_marked(). EBX/EDX pushed and popped. */
static void frontend_mode12_darken_or_tally(void)
{
    if (DSD(DS_00104AD4) == 2u) {                           /* 0x416DC/0x416DF */
        frontend_darken_marked();                           /* 0x41755 0x41578 */
        return;
    }
    u32 r = DSD(DS_00104AD4);                               /* 0x416D6 */
    flow_1082d0_from_column((u32)DSB(DS_00108104 + r));     /* 0x416E1..0x416EC 0x4660C */
    /* PORT: 0x41733 0x32BAC(char[r], char[r^1]) — the dead stub (see above).
     * The original's EAX/EDX build (DS_0010782A[r^1] and DS_0010782A[r])
     * feeds only this no-op call, so the port drops the dead computation
     * too. */
    if (DSB(DS_00108104 + r) == 7u) {                       /* 0x41738..0x4174B */
        frontend_mode12_advance();                          /* 0x4174D 0x4160C */
        return;
    }
    frontend_darken_marked();                               /* 0x41755 0x41578 */
}

/* 0x41760 — record §48-Z. State 5's "challenger takes over" branch:
 * flow_1082d0_from_column(DS_00108104[r]), flow_stage_pick() (0x25848),
 * fight_char_select(r ^ 1, DS_00104AFC) (0x41350, the other side becomes the
 * current stage's character), then arm the hook game_hook_259cc, mode 0x17
 * and a 0x78-frame (120) countdown. EBX/ECX/EDX pushed and popped. Only
 * caller: state 5's DS_00108104[r] != 7 arm, 0x4213B (0x41C28). */
static void frontend_mode12_challenge_continue(void)
{
    u32 r = DSD(DS_00104AD4);                                /* 0x41763 */
    flow_1082d0_from_column((u32)DSB(DS_00108104 + r));      /* 0x41768..0x41778 0x4660C */
    flow_stage_pick();                                        /* 0x4177D 0x25848 */
    fight_char_select(r ^ 1u, DSW(DS_00104AFC));             /* 0x41784..0x41794 0x41350 */
    DSD(DS_00104AE4) = FN_000259CC;                          /* 0x417A5 */
    DSW(DS_001088EE) = 0u;                                   /* 0x4179E */
    DSW(DS_00104B00) = 0x17u;                                /* 0x417B0 */
    DSW(DS_00104AFE) = 0x78u;                                /* 0x417B7 */
}

/* 0x417C4 — record §48-Z. State 7's "no more continues" branch: draw the
 * game-over text (strings 0x1E/0x1F/0x20 at rows 8/0xA/0xC, col -1, mode 0)
 * or, with the sprite flag DS_00104529 bit 1, spawn the actor DS_000C871C at
 * DS_000C8718's word (mode 0x1E00). Then arm the hook game_hook_26978, mode
 * 0x17 and a 0xB4-frame (180) countdown. EBX/ECX/EDX pushed and popped.
 * Only caller: state 7's DS_00105B3A == 0 arm, 0x42439 (0x41C28). */
static void flow_no_continue_screen(void)
{
    if ((DSB(DS_00104529) & 2u) != 0u) {                    /* 0x417C7/0x417CE */
        (void)actor_spawn((const u32 *)(mem + DS_000C871C), DSW(DS_000C8718),
                          0xFFu, 0x1E00u, 0u);               /* 0x417D0..0x417EA 0x2AE14 */
    } else {
        text_cursor_set(-1, 8, game_string_get(0x1Eu), 0u);   /* 0x417F1..0x41809 0x1C500, 0x2F198 */
        text_cursor_set(-1, 0xA, game_string_get(0x1Fu), 0u); /* 0x4180E..0x41826 */
        text_cursor_set(-1, 0xC, game_string_get(0x20u), 0u); /* 0x4182B..0x41843 */
    }
    DSD(DS_00104AE4) = FN_00026978;                          /* 0x41854 */
    DSW(DS_001088EE) = 0u;                                   /* 0x4185A */
    DSW(DS_00104B00) = 0x17u;                                /* 0x41866 */
    DSW(DS_00104AFE) = 0xB4u;                                /* 0x4186D */
}

/* 0x418F4 — record §48-Z. State 5's scoreboard build: for i in {0, 1}, if
 * the side's camera-target record's +0x63 byte != 1, spawn the side's
 * character (DS_0000A7808[char]) at a per-side position, draw a 12- or
 * 19-glyph dash divider, two name lines (or one tally column) and a small
 * DS_00108104[i] tally, then draw the side's score (rec+0x3C) at column
 * DS_000C875E/DS_000C8764[i], row 0x1C. With the sprite flag DS_00104529 bit
 * 1 clear (the "text layout") the divider/columns come from
 * DS_000C8760/0xC875C..0xC8764's per-side tables and two DS_000C866C-style
 * name lines are text_cursor_set directly; with it set (the "sprite layout")
 * the divider/columns come from DS_000C8758/0xC875C/0xC875E and two fixed
 * sprites DS_000C8730/0xC8744 are spawned. On the second side (i != 0) the
 * spawned character's palette resets to handle 0x74. Last, when the side's
 * select-cycle field rec+0x82 is non-zero, that many copies of the sprite
 * DS_000C85A8 scroll in through DS_00108100/0x108102, `count` steps of
 * 0x800. EBP/ESI/EDI and a four-dword local frame are the raw's loop
 * scratch; i, off4 = i*4 and p2 = i*2 below play that role. Only caller:
 * state 5's top, 0x420ED (0x41C28). */
static void frontend_scoreboard_build(void)
{
    for (u32 i = 0u; i < 2u; i++) {
        u32 off4 = i * 4u, p2 = i * 2u;
        u32 rec = DSD(DS_001077A8 + off4);                  /* 0x4190E */
        if (DSB(rec + 0x63u) == 1u) continue;                /* 0x41921/0x41924 */
        u32 spawned;
        s32 col;
        if ((DSB(DS_00104529) & 2u) != 0u) {                 /* 0x4192A/0x41931 */
            u32 ch = DSB(rec + 0x7Au);
            spawned = actor_spawn((const u32 *)(mem + DSD(DS_0000A7808 + ch * 4u)),
                                  DSW(DS_000C8758 + p2), 0xFFu, 0x2900u, 0u);   /* 0x41937..0x41963 0x2AE14 */
            if (i != 0u) actor_pset_palette(spawned, 0u, 0x74u);   /* 0x41968..0x41973 0x2A17C */
            s32 base = (s32)(s16)DSW(DS_000C8758 + p2) / 512;      /* 0x41978..0x4198E */
            for (s32 c = base; c != base + 0x13; c++)               /* 0x41991..0x419B1 */
                text_glyph_at(c, 0x2D, 0x16, 0x1000u);               /* 0x419AA 0x2F174 */
            (void)actor_spawn((const u32 *)(mem + DS_000C8730), DSW(DS_000C8758 + p2),
                              0xFFu, 0x2E00u, 0u);              /* 0x419D1 0x2AE14 */
            (void)actor_spawn((const u32 *)(mem + DS_000C8744), DSW(DS_000C8758 + p2),
                              0xFFu, 0x3400u, 0u);              /* 0x419F4 0x2AE14 */
            text_number_set((s32)DSB(DS_000C875C + i), 0x18, (s32)DSB(DS_00108104 + i),
                            1, 1u, 0x4003u);                    /* 0x41A22 0x2F434 */
            col = (s32)DSB(DS_000C875E + i);
        } else {
            u32 ch = DSB(rec + 0x7Au);
            spawned = actor_spawn((const u32 *)(mem + DSD(DS_0000A7808 + ch * 4u)),
                                  (u32)((s32)DSD(DS_000C8764 + p2) >> 16), 0xFFu,
                                  0x2B00u, 0u);                 /* 0x41A4B..0x41A78 0x2AE14 */
            if (i != 0u) actor_pset_palette(spawned, 0u, 0x74u);   /* 0x41A7D..0x41A88 0x2A17C */
            s32 start = (s32)DSB(DS_000C8760 + i);              /* 0x41A91 */
            for (s32 c = start; c != start + 0xC; c++)          /* 0x41A9D..0x41AB6 */
                text_glyph_at(c, 0x2D, 0x17, 0x1000u);           /* 0x41AAF 0x2F174 */
            text_cursor_set(start, 0x18, game_string_get(0x4Cu), 0u);   /* 0x41AC2..0x41ACD */
            text_cursor_set(start, 0x19, game_string_get(0x4Du), 0u);   /* 0x41ADC..0x41AE7 */
            text_number_set((s32)DSB(DS_000C8762 + i), 0x1A, (s32)DSB(DS_00108104 + i),
                            1, 1u, 0x2000u);                      /* 0x41B15 0x2F434 */
            text_cursor_set(start, 0x1B, game_string_get(0x4Eu), 0u);   /* 0x41B24..0x41B2F */
            col = (s32)DSB(DS_000C8764 + i);
        }
        text_number_set(col, 0x1C, (s32)DSD(rec + 0x3Cu), 7, 1u, 0x2000u);   /* 0x41B58 0x2F434 */
        s32 count = (s32)DSB(rec + 0x82u);                        /* 0x41B61 */
        if (count > 0) {                                           /* 0x41B6A */
            s32 y = (count << 11) - 0x800;                         /* 0x41B6C..0x41B74 */
            do {
                DSW(DS_00108100) = DSW(DS_000C8518 + p2);          /* 0x41B7A */
                s32 base_y = (DSB(DS_00104529) & 2u) != 0u ? 0x2600 : 0x2800;   /* 0x41B88..0x41B98 */
                DSW(DS_00108102) = (u16)(base_y - y);              /* 0x41B9D */
                y -= 0x800;                                          /* 0x41BB1 */
                count--;                                              /* 0x41BC9 */
                (void)actor_spawn((const u32 *)(mem + DS_000C85A8), DSW(DS_00108100),
                                  0xFFu, DSW(DS_00108102), 0u);        /* 0x41BCA 0x2AE14 */
            } while (count > 0);                                      /* 0x41BCF/0x41BD1 */
        }
    }
}

#define FN_00027134 0x00027134u   /* no symbols.h name: game_hook_27134 */

/* 0x271E0 — record §48-Z. Reset the join/character-select scratch
 * (DS_00104B21, DS_00104B0C = 0; DS_00104B14 = 1; DS_00104B11 = 0),
 * flow_1082d0_from_column(4) (the continue timer default for this screen;
 * EDX survives the call, which pushes and pops it, so DS_00104AFC = the
 * stage word 7 loaded before the call), flow_side_char_random(r ^ 1)
 * (0x2716C, r reloaded after the call), then arm the hook game_hook_27134,
 * mode 0x17 and a 0x78-frame (120) countdown. EBX/ECX/EDX/ESI pushed and
 * popped. Only caller: state 7's DS_00105B3A != 0 arm, 0x423FC (0x41C28,
 * record §48-Z). */
static void flow_join_prompt_draw(void)
{
    DSB(DS_00104B21) = 0u;                                   /* 0x271F4 */
    DSB(DS_00104B14) = 1u;                                    /* 0x271FA */
    DSB(DS_00104B0C) = 0u;                                    /* 0x27200 */
    DSB(DS_00104B11) = 0u;                                    /* 0x27206 */
    flow_1082d0_from_column(4u);                              /* 0x27216 0x4660C */
    DSW(DS_00104AFC) = 7u;                                    /* 0x27220 */
    u32 r = DSD(DS_00104AD4);                                 /* 0x2721B */
    flow_side_char_random(r ^ 1u);                            /* 0x2722E 0x2716C */
    DSD(DS_00104AE4) = FN_00027134;                           /* 0x27233 */
    DSW(DS_00104AFE) = 0x78u;                                 /* 0x27239 */
    DSW(DS_001088EE) = 0u;                                    /* 0x27240 */
    DSW(DS_00104B00) = 0x17u;                                 /* 0x27247 */
}

/* 0x41C28 — record §48-Z. Mode 0x12's step, game_frame's case 0x12 (only
 * caller, 0x253BD). Nine states on the byte DS_00104B25 (a jump table at
 * 0x41C04; DS_00104B25 > 8 is a no-op). `rec` below is
 * DS_001077B0 + r*0x94, the raw's ECX, computed once per call and used by
 * states 3/6/7. */
void game_mode_12_step(void)
{
    if (DSB(DS_00104B25) > 8u) return;                       /* 0x41C36/0x41C38 */
    u32 r = DSD(DS_00104AD4);                                /* 0x41C3E */
    u32 rec = DS_001077B0 + r * 0x94u;                       /* 0x41C44..0x41C5E */

    switch (DSB(DS_00104B25)) {                              /* 0x41C61 */
    case 0u:                                                 /* 0x41C68 */
        if ((DSB(DS_00104529) & 2u) != 0u) {                 /* 0x41C68/0x41C6F */
            DSD(DS_001080BC) =
                actor_spawn((const u32 *)(mem + DS_000C8704), DSW(DS_000C8718),
                           0xFFu, 0x3600u, 0u);               /* 0x41C71..0x41C90 0x2AE14 */
        } else {
            text_cursor_set(-1, 0x1Bu, game_string_get(0x4Fu), 0x4003u);   /* 0x41C97..0x41CB2 0x1C500, 0x2F198 */
        }
        DSW(DS_00104AFE) = 0xFu;                             /* 0x41CC0 */
        DSB(DS_00104B25) = 8u;                               /* 0x41CC7 */
        DSB(DS_00104B23) = 1u;                               /* 0x41CCD */
        break;

    case 1u: {                                               /* 0x41CDD */
        u32 i = DSB(DS_0010810F);                            /* 0x41CDF */
        for (; i < 7u; i++) {                                /* 0x41CE5/0x41DB2 */
            if (i == DSW(DS_00104AFC)) continue;             /* 0x41CF6/0x41CF8 */
            if ((DSB(DS_00108106 + i) & 0x80u) == 0u) continue;   /* 0x41CFE/0x41D0B */
            frontend_portrait_flash(i);                      /* 0x41D13 0x414B4 */
            u32 n = DSB(DS_00108112);                         /* 0x41D1A */
            DSB(DS_00108112) = (u8)(n + 1u);                  /* 0x41D23 */
            /* PORT: 0x41D2B 0x2C3FC(n + 0x34, n) voice, not wired (record
             * §45-A). */
            if (DSD(DS_00104AD4) != 2u) {                     /* 0x41D30/0x41D37 */
                u32 cls = DSB(DS_00108106 + i) & 0x7Fu;       /* 0x41D39/0x41D3F */
                for (u32 side = 0u; side < 2u; side++) {      /* 0x41D4A..0x41D75 */
                    u32 tag = side * 0x40u;                   /* 0x41D68 */
                    u32 ch = DSB(DS_0010782A + side * 0x94u); /* 0x41D4D */
                    if (cls == (ch | tag))                    /* 0x41D5C/0x41D60 */
                        DSB(DS_00108104 + side) =
                            (u8)(DSB(DS_00108104 + side) + 1u);   /* 0x41D62 */
                }
            }
            DSB(DS_0010810F) = (u8)(i + 1u);                  /* 0x41D86 */
            DSB(DS_00104B23) = 1u;                            /* 0x41D8C */
            DSW(DS_00104AFE) = 8u;                            /* 0x41D93 */
            DSB(DS_00104B25) = 8u;                            /* 0x41D9C */
            DSB(DS_00108111) = (u8)(DSB(DS_00108111) + 1u);   /* 0x41DA2 */
            return;
        }
        if (DSD(DS_00104AD4) == 2u ||                         /* 0x41DC2/0x41DC5 */
            DSB(DS_00107813 + r * 0x94u) == 1u) {              /* 0x41DD5..0x41DE4 */
            frontend_darken_marked();                          /* 0x41DE6 0x41578 */
            return;
        }
        DSW(DS_00104AFE) = 0x1Eu;                              /* 0x41DFE */
        DSB(DS_00104B23) = 2u;                                  /* 0x41E05 */
        DSB(DS_00104B25) = 8u;                                   /* 0x41E0B */
        break;
    }

    case 2u:                                                  /* 0x41E1B */
        if ((DSB(DS_00104529) & 2u) != 0u) {                  /* 0x41E1B/0x41E22 */
            actor_set_dead(DSD(DS_001080BC));                        /* 0x41E24..0x41E29 0x2B150 */
        } else {
            text_cursor_hold_font2(-1, 0x1Bu, game_string_get(0x50u), 0x4000u);   /* 0x41E30..0x41E4B 0x1C500, 0x2F510 */
        }
        DSB(DS_00104B25) = 3u;                                 /* 0x41E57 */
        DSW(DSD(DS_001080F4) + 0x36u) = 0x40u;                 /* 0x41E5D */
        break;

    case 3u:                                                   /* 0x41E6D */
        /* PORT: the raw reads the dword at +0x34 and SARs it by 16
         * (0x41E72/0x41E78), which is exactly the signed word at +0x36
         * (little-endian: the dword's top 16 bits). The port reads that
         * word directly. */
        if ((s32)(s16)DSW(DSD(DS_001080F4) + 0x36u)
            + (s32)DSD(DSD(DS_001080F4) + 0x1Cu) < 0x2300)     /* 0x41E7D/0x41E83 */
            break;                                              /* not yet: nothing this frame */
        DSD(DSD(DS_001080F4) + 0x1Cu) = 0x2300u;                /* 0x41E89 */
        DSW(DSD(DS_001080F4) + 0x36u) = 0u;                     /* 0x41E90 */
        if ((DSB(DS_00104529) & 2u) != 0u) {                    /* 0x41EA2/0x41EA5 */
            u32 ch = DSB(rec + 0x7Au);
            s32 half = (s32)((DSB(DS_000C86A8 + (u32)DSW(DS_00104AFC) * 8u) + 0x58u) / 2u);   /* 0x41EB4..0x41ECB */
            s32 x = 0xA8 - half;                                /* 0x41ECD/0x41ED9 */
            u32 spawned = actor_spawn(
                (const u32 *)(mem + DSD(DS_0000A7760 + ch * 4u)),
                (u32)x << 6, 0xFFu, 0x200u, 0u);                /* 0x41EF1 0x2AE14 */
            if (DSD(DS_00104AD4) != 0u)                          /* 0x41EF6/0x41EFD */
                actor_pset_palette(spawned, 0u, 0x74u);           /* 0x41F06 0x2A17C */
            x += 0x50;                                           /* 0x41F12 */
            (void)actor_spawn((const u32 *)(mem + DS_000C8780),
                              (u32)x << 6, 0xFFu, 0x400u, 0u);     /* 0x41F24 0x2AE14 */
            s32 halfw = (s32)(DSB(DS_000C86A8 + (u32)DSW(DS_00104AFC) * 8u) / 2u) + 8;   /* 0x41F3F..0x41F4F */
            u32 rec2 = actor_spawn((const u32 *)(mem + DS_000C87A8),
                                   (u32)(x + halfw) << 6, 0xFFu, 0x400u, 0u);   /* 0x41F62 0x2AE14 */
            actors_anim_seek(rec2, DSD(DS_000C86A4 + (u32)DSW(DS_00104AFC) * 8u));   /* 0x41F81 0x2BCF4 */
            (void)actor_spawn((const u32 *)(mem + DS_000C8794), DSW(DS_000C8718),
                              0xFFu, 0xA00u, 0u);                  /* 0x41F96 0x2AE14 */
        } else {
            const u8 *s1 = game_string_get(DSD(DS_000C866C + (u32)DSB(rec + 0x7Au) * 4u));  /* 0x41FAE 0x1C500 */
            s32 w1 = text_width(s1, 0u);                          /* 0x41FB3 0x2F0F0 */
            const u8 *s2 = game_string_get(0xDBu);                /* 0x41FC1 0x1C500 */
            s32 w2 = text_width(s2, 0u);                          /* 0x41FC6 0x2F0F0 */
            const u8 *s3 = game_string_get(DSD(DS_000C8688 + (u32)DSW(DS_00104AFC) * 4u));  /* 0x41FDE 0x1C500 */
            s32 w3 = text_width(s3, 0u);                          /* 0x41FE3 0x2F0F0 */
            s32 col0 = 0x15 - (w1 + w2 + w3) / 2;                  /* 0x41FF3..0x41FFA */
            const u8 *s4 = game_string_get(DSD(DS_000C866C + (u32)DSB(rec + 0x7Au) * 4u));  /* 0x4200D 0x1C500 */
            text_cursor_set(col0, 2, s4, 0u);                     /* 0x42018 0x2F198 */
            text_cursor_next_line(game_string_get(0xDBu), 0u);    /* 0x42024/0x42029 0x1C500, 0x2F41C */
            text_cursor_next_line(
                game_string_get(DSD(DS_000C8688 + (u32)DSW(DS_00104AFC) * 4u)), 0u);   /* 0x4203F/0x42044 */
        }
        DSB(DS_0010810E) = 0u;                                   /* 0x42056 */
        DSB(DS_00108104 + r) = (u8)(DSB(DS_00108104 + r) + 1u);  /* 0x42060 */
        DSB(DS_00104B25) = 4u;                                    /* 0x4206C */
        DSB(DS_00108111) = (u8)(DSB(DS_00108111) + 1u);           /* 0x42079 */
        /* PORT: 0x4207F 0x2C3FC(0x3A) voice, not wired (record §45-A). */
        break;

    case 4u:                                                     /* 0x4208E */
        frontend_portrait_flash(DSW(DS_00104AFC));                /* 0x42096 0x414B4 */
        DSB(DS_0010810E) = (u8)(DSB(DS_0010810E) + 1u);           /* 0x420A5 */
        /* PORT: 0x420B7 0x2C3FC(0xC7) voice when DS_0010810E == 8, not wired
         * (record §45-A). */
        DSW(DS_00104AFE) = 4u;                                     /* 0x420C5 */
        DSB(DS_00104B23) = 5u;                                     /* 0x420CC */
        DSB(DS_00104B25) = 8u;                                     /* 0x420D2 */
        break;

    case 5u:                                                      /* 0x420E1 */
        if (DSB(DS_0010810E) < 8u) {                               /* 0x420E8/0x420EB */
            frontend_portrait_reveal(DSW(DS_00104AFC));            /* 0x42159 0x41528 */
            DSB(DS_00104B23) = 4u;                                  /* 0x4215E */
            DSW(DS_00104AFE) = 4u;                                   /* 0x42166 */
            DSB(DS_00104B25) = 8u;                                   /* 0x4216D */
            break;
        }
        frontend_scoreboard_build();                                /* 0x420ED 0x418F4 */
        if (DSB(DS_00104B1F) == 3u) {                                /* 0x420F9/0x420FC */
            frontend_mode12_darken_or_tally();                       /* 0x420FE 0x416D4 */
            break;
        }
        /* PORT: 0x4210D 0x2C3FC(0x33) voice, not wired (record §45-A). */
        if (DSB(DS_00108104 + r) == 7u) {                            /* 0x42127/0x4212A */
            frontend_mode12_advance();                                /* 0x4212C 0x4160C */
            break;
        }
        frontend_mode12_challenge_continue();                        /* 0x4213B 0x41760 */
        break;

    case 6u: {                                                      /* 0x4217D */
        s32 d = (s32)DSW(DSD(DS_001080F4) + 0x2Cu) - 0x180;          /* 0x42183/0x4218D */
        s32 step = d / 64;   /* 0x42195..0x4219D: the SAR/SHL/SBB/SAR sequence
                               * is a truncating (round-toward-zero) signed
                               * divide by 64, same idiom as elsewhere in this
                               * file; C's `/` matches it, a plain `>>6` would
                               * not for a negative `d`. */
        DSW(DS_00108100) = DSW(DS_000C8518 + r * 2u);                  /* 0x421A7/0x421B5 */
        u32 field = DS_00107832 + r * 0x94u;                            /* the raw's local ECX, same as `rec + 0x82` */
        u8 cyc = DSB(field);                                             /* 0x421C2/0x421D2 */
        s32 base_y = (DSB(DS_00104529) & 2u) != 0u ? 0x2600 : 0x2800;   /* 0x421AF..0x421D9 */
        DSW(DS_00108102) = (u16)(base_y - (s32)(cyc - 1) * 0x800);       /* 0x421E1/0x421E3 */
        DSW(DSD(DS_001080F4) + 0x34u) = (s16)(((s32)(u16)DSW(DS_00108100)
            - (s32)DSD(DSD(DS_001080F4) + 0x18u)) / step);               /* 0x421FC..0x42213 */
        DSW(DSD(DS_001080F4) + 0x36u) = (s16)(((s32)(u16)DSW(DS_00108102)
            - (s32)DSD(DSD(DS_001080F4) + 0x1Cu)) / step);               /* 0x42211..0x4221E */
        /* PORT: 0x42229 0x2C3FC(0xBC, remainder) voice, not wired (record
         * §45-A). */
        DSB(DS_00104B25) = 7u;                                            /* 0x42238 (DH) */
        actors_anim_begin(DSD(DS_001080F4), DS_0000E9286, 0x3F800000u);   /* 0x42245 0x2BC30 */
        for (u32 off = 0u; off != 0x1Cu; off += 4u)                       /* 0x4224A..0x4226C */
            actors_anim_begin(DSD(DS_001080C0 + off), DSD(DS_000C85F4 + off),
                              0x3F800000u);                                /* 0x4225E 0x2BC30 */
        break;
    }

    case 7u: {                                                          /* 0x4226E */
        s32 y = (s32)(s16)DSW(DSD(DS_001080F4) + 0x2Cu) - 0x40;          /* 0x4227C/0x4227F */
        DSW(DSD(DS_001080F4) + 0x2Cu) = (u16)y;                          /* 0x4227F */
        if (y > 0x180) break;                                            /* 0x42283/0x42289 */
        actor_set_dead(DSD(DS_001080F4));                                       /* 0x4228F 0x2B150 */
        (void)actor_spawn((const u32 *)(mem + DS_000C85A8), DSW(DS_00108100),
                          0xFFu, DSW(DS_00108102), 0u);                    /* 0x422B2 0x2AE14 */
        fighter_41310(r, 100000);                                          /* 0x422C1 0x41310 */
        u32 sc = (DSB(DS_00104529) & 2u) != 0u
               ? DSB(DS_000C875E + r) : DSB(DS_000C8764 + r);              /* 0x422DA..0x42314 */
        text_number_set((s32)sc, 0x1C, (s32)DSD(rec + 0x3Cu), 7, 1u, 0x2000u);   /* 0x42329 0x2F434 */
        if (DSB(DS_00104B1D) != 0u) {                                      /* 0x4232E/0x42335 */
            frontend_darken_marked();                                      /* 0x42337 0x41578 */
            break;
        }
        if (DSB(DS_00104B1F) == 3u) {                                      /* 0x42346/0x42350 */
            frontend_darken_marked();                                      /* 0x42352 0x41578 */
            break;
        }
        if (DSB(DS_0010452C) < 9u && DSB(DS_00108113) == 0u) {             /* 0x42368/0x4236B/0x42374 */
            flow_1082d0_from_column((u32)DSB(DS_00108104 + r));            /* 0x4238B 0x4660C */
            flow_stage_pick();                                              /* 0x42390 0x25848 */
            fight_char_select(r ^ 1u, DSW(DS_00104AFC));                    /* 0x423A7 0x41350 */
            DSD(DS_00104AE4) = FN_000259CC;                                  /* 0x423C4 */
            DSW(DS_001088EE) = 0u;                                           /* 0x423BD */
            DSW(DS_00104AFE) = 0x78u;                                        /* 0x423B6 */
            DSW(DS_00104B00) = 0x17u;                                        /* 0x423CA */
            break;
        }
        if (DSB(DS_00105B3A) == 0u) {                                       /* 0x423E1 */
            flow_no_continue_screen();                                       /* 0x42434 0x417C4 */
            break;
        }
        if (DSB(DS_001078A7) == 0u) DSB(DS_0010789F) = 0x3Cu;               /* 0x423EA/0x423F5 */
        else DSB(DS_0010780B) = 0x3Cu;                                       /* 0x423EC */
        flow_join_prompt_draw();                                             /* 0x423FC 0x271E0 */
        config_play_time_close(DSD(DS_00104ABC), DSB(DS_00104B19));          /* 0x4240E 0x32A3C */
        DSB(DS_00104B19) = 0u;                                                /* 0x4241A */
        /* PORT: 0x42420 longjmp(0x2DAE4, 0x10, 1) — the front-end quit path,
         * out of scope (spec §7). */
        break;
    }

    case 8u:                                                                /* 0x42443 */
        DSW(DS_00104AFE) = (u16)(DSW(DS_00104AFE) - 1u);                    /* 0x42449 */
        if ((s16)DSW(DS_00104AFE) < 1)                                       /* 0x42452 */
            DSB(DS_00104B25) = DSB(DS_00104B23);                             /* 0x42457/0x4245C */
        break;
    }
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

/* ---- the mode 0x17 countdown 0x4F318 and its skip test 0x4F790 (§46-G) ---- */

/* 0x4F778 — record §46-G. AL = (DS_001088E4 & the dword 0xC9898[n]) != 0
 * (`lea edx,[eax*4]; test [edx+0xc9898],eax; setnz al`); EDX is pushed and
 * popped. The three callers (0x27AC3, 0x42E01, 0x4F7D5) read AL only (`and
 * eax,0xff` or `test al,al`), so the rest of EAX (DS_001088E4's upper bytes)
 * is dropped. The two masks are 0x0F000000 and 0x00000F00. */
u32 frontend_buttons_pressed(u32 n)
{
    return (DSD(DS_000C9898 + n * 4u) & DSD(DS_001088E4)) != 0u
               ? 1u : 0u;                               /* 0x4F779..0x4F78B */
}

/* 0x4F790 — record §46-G. The countdown's skip test, AL only (every caller
 * masks or tests AL). 2 when the held bits DS_001088D8 cover either mask
 * 0xC9898[0]/[1] whole; else 1 when a bit of either is newly pressed
 * (DS_001088E4, 0xC9898[0] tested inline, [1] through 0x4F778); else 0. The
 * stores to DS_001088D8 at 0x4F7C1/0x4F7E6 write back the value just read.
 * EBX/ECX/EDX/EDI are pushed and popped. */
u32 frontend_skip_check(void)
{
    u32 held = DSD(DS_001088D8);                        /* 0x4F794 */
    for (u32 i = 0; i < 8u; i += 4u) {                  /* 0x4F79A/0x4F7AE/0x4F7B1 */
        u32 m = DSD(DS_000C9898 + i);                   /* 0x4F79E */
        if ((held & m) == m) {                          /* 0x4F7A4/0x4F7A6 */
            DSD(DS_001088D8) = held;                    /* 0x4F7E6 */
            return 2u;                                  /* 0x4F7AA */
        }
    }
    u32 mask0 = DSD(DS_000C9898);                       /* 0x4F7BB */
    DSD(DS_001088D8) = held;                            /* 0x4F7C1 */
    u32 r = (DSD(DS_001088E4) & mask0) != 0u ? 1u : 0u; /* 0x4F7B6/0x4F7C7/0x4F7C9 */
    if (r == 0u && frontend_buttons_pressed(1u) != 0u)  /* 0x4F7CE..0x4F7DC */
        r = 1u;                                         /* 0x4F7DE */
    DSD(DS_001088D8) = DSD(DS_001088D8);                /* 0x4F7E0/0x4F7E6 */
    return r;
}

/* 0x4F24C — record §48-X. The mode 0x15 handler (0x24C5C case 0x15, the jump
 * table 0x24B8C entry 0x253E0 `call 0x4f24c; jmp 0x2540F`; its only caller,
 * no dword in the image holds 0x4F24C). EBX/EDX are pushed and popped. The
 * body is 0x4F318's (and 0x4F2B0's) countdown byte for byte up to the
 * expiry: while the word DS_001088EE is non-zero it is decremented (0x4F27F
 * `mov ebx,edx; dec ebx`); at 0 the skip test 0x4F790 runs: 2 stores DX to
 * DS_00104AFE (still the 0 just tested: 0x4F790 pushes and pops EDX), 1
 * takes 0x3C off it. Then DS_00104AFE is decremented (0x4F28F `mov edx,eax;
 * dec edx`), and when its old value was <= 0 (signed, `test ax,ax; jg`) the
 * mode word DS_00104B00 takes the return-mode word DS_00104AFA (0x4F29E/
 * 0x4F2A4). Unlike 0x4F318 there is no DS_001088EE = 0xFFFF store and, unlike
 * 0x4F2B0, no DS_00104AE4 hook call. The two stores of mode 0x15 are 0x29B74
 * (0x29BBA; DS_001088EE = DS_00104AFE = 0x78, whose caller 0x27A2C then sets
 * DS_00104AFA = 0x1E, record §48-E) and 0x41578 (0x415F8; DS_00104AFE =
 * 0x78, DS_001088EE = 0, DS_00104AFA = 0x13, record §42-E). EAX (the old
 * countdown, or the return mode) is left as every case leaves it for the
 * shared tail 0x2540F; the port returns nothing, as for 0x4F318. */
void frontend_mode_15_step(void)
{
    u16 dx = DSW(DS_001088EE);                          /* 0x4F24E */
    if (dx == 0u) {                                     /* 0x4F255/0x4F258 */
        u32 r = frontend_skip_check() & 0xFFu;          /* 0x4F25A 0x4F790, 0x4F25F */
        if (r == 2u) DSW(DS_00104AFE) = dx;             /* 0x4F264/0x4F269 */
        if (r == 1u)                                    /* 0x4F270 */
            DSW(DS_00104AFE) = (u16)(DSW(DS_00104AFE) - 0x3Cu);  /* 0x4F275 */
    } else {
        DSW(DS_001088EE) = (u16)(dx - 1u);              /* 0x4F27F..0x4F282 */
    }
    u16 ax = DSW(DS_00104AFE);                          /* 0x4F289 */
    DSW(DS_00104AFE) = (u16)(ax - 1u);                  /* 0x4F28F..0x4F292 */
    if ((s16)ax > 0) return;                            /* 0x4F299/0x4F29C */
    DSW(DS_00104B00) = DSW(DS_00104AFA);                /* 0x4F29E/0x4F2A4 */
}

/* 0x4F318 — record §46-G. The mode 0x17 handler (0x24C5C case 0x17, the jump
 * table 0x24B8C entry 0x253EE; also called at 0x425E5 in 0x424E8). While the
 * word DS_001088EE is non-zero it is decremented; at 0 the skip test runs: 2
 * zeroes the countdown DS_00104AFE (DX, still 0: 0x4F790 pops EDX), 1 takes
 * 0x3C off it. Then DS_00104AFE is decremented, and when its old value was <=
 * 0 (signed, `test ax,ax; jg`) DS_001088EE = 0xFFFF and the DS_00104AE4 hook
 * runs. The mode is left to the hook (unlike mode 0x16's 0x4F2B0). */
void frontend_mode_17_step(void)
{
    u16 dx = DSW(DS_001088EE);                          /* 0x4F31A */
    if (dx == 0u) {                                     /* 0x4F321/0x4F324 */
        u32 r = frontend_skip_check() & 0xFFu;          /* 0x4F326 0x4F790, 0x4F32B */
        if (r == 2u) DSW(DS_00104AFE) = dx;             /* 0x4F330/0x4F335 */
        if (r == 1u)                                    /* 0x4F33C */
            DSW(DS_00104AFE) = (u16)(DSW(DS_00104AFE) - 0x3Cu);  /* 0x4F341 */
    } else {
        DSW(DS_001088EE) = (u16)(dx - 1u);              /* 0x4F34B..0x4F34E */
    }
    u16 ax = DSW(DS_00104AFE);                          /* 0x4F355 */
    DSW(DS_00104AFE) = (u16)(ax - 1u);                  /* 0x4F35B..0x4F35E */
    if ((s16)ax > 0) return;                            /* 0x4F365/0x4F368 */
    DSW(DS_001088EE) = 0xFFFFu;                         /* 0x4F36A */
    /* PORT: `call dword [0x104ae4]` through the registry, a miss skipped, as
     * in 0x4F9A0. Every value the image stores there is registered (records
     * §42-E, §43-B, §46-B, §46-F), and only 0x29D60/0x5D812 are no-ops. EAX
     * (the old countdown, <= 0) and EDX (EAX - 1, the new countdown, from
     * 0x4F35B/0x4F35D) are not passed: the registered hooks take no
     * arguments, as for 0x4F9A0. */
    void (*hook)(void) = fn_resolve(DSD(DS_00104AE4));
    if (hook != NULL) hook();                           /* 0x4F373 */
}

/* 0x4F2B0 — record §49-G. The mode 0x16 handler (0x24C5C case 0x16, the jump
 * table 0x24B8C entry 0x253E7; sole caller, per get_xrefs_to). The same
 * DS_001088EE/DS_00104AFE countdown and frontend_skip_check() (0x4F790) skip
 * test as 0x4F24C (mode 0x15) and 0x4F318 (mode 0x17), but its expiry arm
 * combines both siblings' instead of doing only one half: it runs the
 * DS_00104AE4 hook, as 0x4F318 does (every value the image stores there is
 * registered: records §42-E, §43-B, §46-B, §46-F, and only 0x29D60/0x5D812
 * are no-ops), *and* then takes DS_00104B00 = DS_00104AFA, as 0x4F24C does.
 * Unlike 0x4F318 there is no DS_001088EE = 0xFFFF rearm. EBX/EDX are pushed
 * and popped. As for 0x4F318/0x4F9A0, the registered hooks take no
 * arguments. Reached from mode 8/9's results countdown (game_mode_08_step/
 * game_mode_09_step, records §49-C/§48-Y, which install game_hook_25bbc and
 * set DS_00104B00 = 0x16 with DS_00104AFA = 0x30 or 5). */
void frontend_mode_16_step(void)
{
    u16 dx = DSW(DS_001088EE);                          /* 0x4F2B2 */
    if (dx == 0u) {                                     /* 0x4F2B9/0x4F2BC */
        u32 r = frontend_skip_check() & 0xFFu;          /* 0x4F2BE 0x4F790, 0x4F2C3 */
        if (r == 2u) DSW(DS_00104AFE) = dx;             /* 0x4F2CB/0x4F2CD */
        if (r == 1u)                                    /* 0x4F2D4/0x4F2D7 */
            DSW(DS_00104AFE) = (u16)(DSW(DS_00104AFE) - 0x3Cu);  /* 0x4F2D9 */
    } else {
        DSW(DS_001088EE) = (u16)(dx - 1u);              /* 0x4F2E3..0x4F2E6 */
    }
    u16 ax = DSW(DS_00104AFE);                          /* 0x4F2ED */
    DSW(DS_00104AFE) = (u16)(ax - 1u);                  /* 0x4F2F3..0x4F2F6 */
    if ((s16)ax > 0) return;                            /* 0x4F2FD/0x4F300 */
    void (*hook)(void) = fn_resolve(DSD(DS_00104AE4));
    if (hook != NULL) hook();                           /* 0x4F302 */
    DSW(DS_00104B00) = DSW(DS_00104AFA);                /* 0x4F308/0x4F30E */
}

/* ---- the mode 0x18/0x19 effects-gated hooks 0x4F6E8/0x4F704 (§49-I) ---- */

/* 0x4F6E8 — record §49-I. The mode 0x18 handler (0x24C5C case 0x18, the jump
 * table 0x24B8C entry 0x253F5; sole caller, per get_xrefs_to). Not a
 * countdown like 0x15/0x16/0x17/0x4F9C8's family: the sole gate is the
 * effects-in-flight count DS_0009AF3D (effects_active()'s backing byte,
 * `cmp byte [0x9af3d],0; jnz 0x4f703`). While it is non-zero the function is
 * a no-op (straight to the shared `ret`). At 0, the DS_00104AE4 hook runs
 * (the same registry as 0x4F318/0x4F2B0/0x4F9A0/0x4F9C8: every value the
 * image stores there is registered — records §42-E, §43-B, §46-B, §46-F —
 * and only 0x29D60/0x5D812 are no-ops; the registered hooks take no
 * arguments) and then DS_00104B00 takes the return-mode word DS_00104AFA, as
 * 0x4F2B0's/0x4F24C's expiry arms do. EAX/EDX carry no state the port needs:
 * the raw's AX = DS_00104AFA (0x4F6F7) is only the value the next
 * instruction stores back. */
void frontend_mode_18_step(void)
{
    if (DSB(DS_0009AF3D) != 0u) return;                 /* 0x4F6E8/0x4F6EF */
    void (*hook)(void) = fn_resolve(DSD(DS_00104AE4));
    if (hook != NULL) hook();                           /* 0x4F6F1 */
    DSW(DS_00104B00) = DSW(DS_00104AFA);                /* 0x4F6F7/0x4F6FD */
}

/* 0x4F704 — record §49-I. The mode 0x19 handler (0x24C5C case 0x19, the jump
 * table 0x24B8C entry 0x253FC; sole caller, per get_xrefs_to). Byte-for-byte
 * 0x4F6E8's gate and hook call (`cmp byte [0x9af3d],0; jnz 0x4f713; call
 * [0x104ae4]; ret`) but it omits the mode-transition store: unlike 0x18,
 * DS_00104B00 is left exactly as the hook (or the shared 0x2545C tail) set
 * it. The two are otherwise the same function, which is why they sit
 * together in the game_frame gap list. */
void frontend_mode_19_step(void)
{
    if (DSB(DS_0009AF3D) != 0u) return;                 /* 0x4F704/0x4F70B */
    void (*hook)(void) = fn_resolve(DSD(DS_00104AE4));
    if (hook != NULL) hook();                           /* 0x4F70D */
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
 * 0x28468, game_mode_08_step, record §49-C). The byte DS_001078FA = 0 and
 * DS_00104B13 = 0 (AH), DS_00104B1E
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

/* ---- the coin/start divert 0x257A4 and its callee 0x46594 (record §47-C) -- */

#define FN_0004367C 0x0004367Cu   /* no symbols.h name: fight_hook_4367c */
#define DS_00104B1B 0x00104B1Bu   /* no symbols.h name */

/* 0x46594 — record §47-C. Only caller 0x257F7 (0x257A4). With the byte
 * DS_00108173 != 0, DS_001082C8 = 7 (EDX) and DS_001082CC = 4 (EBX);
 * otherwise both = the byte 0xC93F8[b] (`xor eax,eax; mov al,[0x10452c]; mov
 * al,[eax+0xc93f8]; and eax,0xff`), b = DS_0010452C. Then DS_001082D0 = the
 * byte 0xC9388[7b] (`lea eax,[edx*8]; sub eax,edx`) and the 0x46504 latch
 * inline: DS_001082C0 = DS_001082C8, DS_001082C4 = DS_001082CC. EBX/EDX are
 * pushed and popped; EAX (DS_001082CC) is dead at 0x257FC. */
void flow_1082c8_init(void)
{
    if (DSB(DS_00108173) != 0u) {                       /* 0x46596/0x4659D */
        DSD(DS_001082C8) = 7u;                          /* 0x465A9 */
        DSD(DS_001082CC) = 4u;                          /* 0x465AF */
    } else {
        u32 v = DSB(DS_000C93F8 + DSB(DS_0010452C));    /* 0x465B7..0x465C4 */
        DSD(DS_001082CC) = v;                           /* 0x465C9 */
        DSD(DS_001082C8) = v;                           /* 0x465CE */
    }
    u32 b = DSB(DS_0010452C);                           /* 0x465D3/0x465D5 */
    DSD(DS_001082D0) = DSB(DS_000C9388 + b * 7u);       /* 0x465DB..0x465EF */
    DSD(DS_001082C0) = DSD(DS_001082C8);                /* 0x465F4/0x465F9 */
    DSD(DS_001082C4) = DSD(DS_001082CC);                /* 0x465FE/0x46603 */
}

/* 0x257A4 — record §47-C. The coin/start divert. EAX (`players`) is copied to
 * EDX (0x257A6), which 0x2C3FC and 0x2BAF4 push and pop, so the 0x257E0 byte
 * store DS_00104B1F = DL is the argument. The first 0x33C18 call runs with EDX
 * still the argument (it reads only EAX), the second after `xor edx,edx`;
 * EDX = 0 and ECX = 7 (0x257BB) survive 0x33C18/0x46594 into 0x65490, the
 * memset (EAX = dst, EDX = the fill dword, ECX = the count), which clears the
 * seven bytes DS_00104B02..0x104B08: the mode word's upper half and the four
 * bytes after it. Then the hook 0x4367C (EDX, 0x25806) and 0x4F980 arms mode
 * 0x1A with the return mode 0x10. Callers: 0x11D41 (0x11D04's coin arm,
 * EAX = the accepted mask), 0x11EB8 (0x11D04's state 8, EAX = 3), the eight
 * game-start modes 0x28..0x2F of 0x24C5C (0x24F5C..0x25194) and the
 * unentered stub 0x11CC8 (`jmp` at 0x11CD4). */
void game_coin_divert(u32 players)
{
    /* PORT: 0x257AD 0x2C3FC(0x100) voice, not wired (record §45-A). */
    actors_reset_al(0u);                                /* 0x257B2/0x257B4 0x2BAF4 */
    DSB(DS_00104B17) = 0u;                              /* 0x257C0 (AH) */
    DSB(DS_00104B19) = 0u;                              /* 0x257C6 */
    DSB(DS_00104B11) = 0u;                              /* 0x257CC */
    DSB(DS_00104B1B) = 0u;                              /* 0x257D2 */
    DSB(DS_00104B15) = 0u;                              /* 0x257D8 */
    DSB(DS_00104B1F) = (u8)players;                     /* 0x257E0 (DL) */
    fight_char_reset(0u);                               /* 0x257DE/0x257E6 0x33C18 */
    fight_char_reset(1u);                               /* 0x257EB/0x257F2 0x33C18 */
    flow_1082c8_init();                                 /* 0x257F7 0x46594 */
    mem_fill(DS_00104B02, 0u, 7u);                      /* 0x257FC/0x25801 0x65490 */
    DSD(DS_00104AE4) = FN_0004367C;                     /* 0x25806/0x25810 */
    frontend_wipe_arm(0x10u);                           /* 0x2580B/0x25816 0x4F980 */
    /* PORT: 0x25820 0x2C3FC(0x53) voice, not wired (record §45-A). Its EAX is
     * 0x257A4's return value, which no caller reads: 0x11EB8 is followed by
     * 0x10DB0, which loads AH first and never reads AL; 0x11D41 returns
     * through 0x11D04 into case 3's `jmp 0x2540F`, and the game-start cases
     * jump there too. In that tail 0x2A31C, 0x3BB90 and 0x12D48 write EAX (or
     * AL) before reading it, the mode tail 0x2545C loads AX and compares only
     * AX, and 0x255CC reloads EAX (0x25621) after 0x24C5C returns. */
}

/* ---- 0x33C18's other callers (record §48-Q) ------------------------------ */

#define FN_00028D80 0x00028D80u   /* no symbols.h name: frontend_char_screen_hook_voice */

/* 0x4651C — record §48-Q. The 0x46504 latch reversed: DS_001082C8 =
 * DS_001082C0 and DS_001082CC = DS_001082C4 (two dword copies through EAX).
 * Callers: 0x28E07 (0x28DA4) and 0x27E74 (0x27DC8, record §48-K). */
void flow_1082c8_restore(void)
{
    DSD(DS_001082C8) = DSD(DS_001082C0);                /* 0x4651C/0x46521 */
    DSD(DS_001082CC) = DSD(DS_001082C4);                /* 0x46526/0x4652B */
}

/* 0x292D4 — record §48-Q. EAX = the side (kept in EBX), EDX = the character
 * (DL). 0x33C18(side) pushes EDX, so DL is still the argument. Then
 * DS_00105B34[side] = AH = 0 (0x292DC `xor ah,ah` on 0x33C18's return),
 * DS_0010816A[side] = DL, and DS_00105B34[side] = 1 when the other side
 * (`xor al,1` on the side) holds the same character with its DS_00105B34
 * byte 0. Last the bytes DS_00104B12 = the side and DS_0010810D = the side ^ 1.
 * Only caller: 0x29871 (0x296B8). */
void flow_side_char_set(u32 side, u32 ch)
{
    u32 other = side ^ 1u;                              /* 0x292E4/0x292E6 */
    fight_char_reset(side);                             /* 0x292D7 0x33C18 */
    DSB(DS_00105B34 + side) = 0u;                       /* 0x292DC/0x292DE */
    DSB(DS_0010816A + side) = (u8)ch;                   /* 0x292E8 */
    if ((u8)ch == DSB(DS_0010816A + other)              /* 0x292EE `jnz` */
            && DSB(DS_00105B34 + other) == 0u)          /* 0x292F6 `jnz` */
        DSB(DS_00105B34 + side) = 1u;                   /* 0x292FF */
    DSB(DS_00104B12) = (u8)side;                        /* 0x29306 */
    DSB(DS_0010810D) = (u8)(side ^ 1u);                 /* 0x2930C/0x2930F */
}

/* 0x2716C — record §48-Q. EAX = the side (kept in ECX). 0x33C18(side), then
 * rng(7) (0x5D7DC with EDX = 0, which it pushes and pops, so `mov dl,al`
 * makes EDX the draw) until the draw's byte DS_00104B02[c] has bit 5 clear
 * (0x2718A `and bl,0x20`). That bit is set, DS_0010816A[side] = c, and
 * DS_00105B34[side] = DL ^ AL = 0 (0x271A2). Then the 0x292D4 tail: 1 when the
 * other side holds the same character with its DS_00105B34 byte 0, and
 * DS_00104B12 = the side, DS_0010810D = the side ^ 1. When all seven bytes
 * have bit 5 set the raw draws for ever; so does the port. Callers: 0x2705E
 * (0x26F58), 0x2722E (0x271E0) and 0x27700 (0x274FC). */
void flow_side_char_random(u32 side)
{
    u32 other = side ^ 1u;                              /* 0x271A4/0x271A6 */
    u32 c;
    fight_char_reset(side);                             /* 0x27171 0x33C18 */
    do {
        c = rng_next(7u);                               /* 0x27176..0x2717D 0x5D7DC */
    } while ((DSB(DS_00104B02 + c) & 0x20u) != 0u);     /* 0x27184..0x27193 */
    DSB(DS_00104B02 + c) = (u8)(DSB(DS_00104B02 + c) | 0x20u);  /* 0x27195 */
    DSB(DS_0010816A + side) = (u8)c;                    /* 0x2719C */
    DSB(DS_00105B34 + side) = 0u;                       /* 0x271A2/0x271A8 (DL ^ AL) */
    if (DSB(DS_0010816A + side) == DSB(DS_0010816A + other)    /* 0x271AE/0x271B4 */
            && DSB(DS_00105B34 + other) == 0u)          /* 0x271BC */
        DSB(DS_00105B34 + side) = 1u;                   /* 0x271C5 */
    DSB(DS_00104B12) = (u8)side;                        /* 0x271CC */
    DSB(DS_0010810D) = (u8)(side ^ 1u);                 /* 0x271D2/0x271D5 */
}

/* 0x28DA4 — record §48-Q. A player joins (modes 6 and 0xC, at 0x25269 and
 * 0x25353, with EAX = 0x28CC8's result - 1, the joining side; kept in ESI).
 * In raw order: 0x32970(0); DS_000F0A48, DS_00104AEC and DS_00104AE8 = 0
 * (EDX); the 0x100 voice; DS_00104ABC = (DS_00104B1F == 3) + 1; string 0x44
 * drawn by 0x2F510 at col -1, row 0xA (EDX, kept by 0x1C500) with mode 0x4000
 * (ECX); 0x4651C; 0x33C18(side); the byte +0x5B of the OTHER side's slot = 0
 * (0x28E0E `xor si,1`, 0x28E29); 0x32B00(DS_00104ABC, 1) and 0x2DAE4(0xD, 1).
 * Then DS_00104B24 = 1 (BL), the hook 0x28D80 (ESI, 0x28E4E), the mode word =
 * CX = 0x17 (set at 0x28E3A; 0x32B00 and 0x2DAE4 push and pop ECX),
 * DS_00104B17 = DH = 2, DS_00104B15 = BH = 0, and the words DS_00104AFE =
 * 0x78 and DS_001088EE = 0x1E. EBX/ECX/EDX/ESI are pushed and popped. */
void flow_player_join(u32 side)
{
    /* PORT: 0x28DAC 0x32970(EAX = 0), the run clock, is out of scope (spec
     * §7); the host clock owns wall time. */
    DSD(DS_000F0A48) = 0u;                              /* 0x28DB8 */
    DSD(DS_00104AEC) = 0u;                              /* 0x28DBE */
    DSD(DS_00104AE8) = 0u;                              /* 0x28DC4 */
    /* PORT: 0x28DCA 0x2C3FC(0x100) voice, not wired (record §45-A). */
    DSD(DS_00104ABC) = (DSB(DS_00104B1F) == 3u ? 1u : 0u) + 1u;   /* 0x28DCF..0x28DE7 */
    text_cursor_hold_font2(-1, 0xA, game_string_get(0x44u),
                           0x4000u);                    /* 0x28DF6 0x1C500, 0x28E02 0x2F510 */
    flow_1082c8_restore();                              /* 0x28E07 0x4651C */
    fight_char_reset(side);                             /* 0x28E12 0x33C18 */
    DSB(DS_0010780B + (side ^ 1u) * 0x94u) = 0u;        /* 0x28E0E/0x28E17..0x28E29 */
    config_play_time_snap(DSD(DS_00104ABC), 1u);        /* 0x28E30..0x28E3F 0x32B00 */
    /* PORT: 0x28E53 0x2DAE4(0xD, 1), the audit add, is deferred (spec §7),
     * as in 0x2D962 and 0x32A3C (config.c). */
    DSB(DS_00104B24) = 1u;                              /* 0x28E27/0x28E5A (BL) */
    DSD(DS_00104AE4) = FN_00028D80;                     /* 0x28E4E/0x28E60 */
    DSW(DS_00104B00) = 0x17u;                           /* 0x28E3A/0x28E66 (CX) */
    DSB(DS_00104B17) = 2u;                              /* 0x28E58/0x28E6D (DH) */
    DSB(DS_00104B15) = 0u;                              /* 0x28E73/0x28E7A (BH) */
    DSW(DS_00104AFE) = 0x78u;                           /* 0x28E75/0x28E85 */
    DSW(DS_001088EE) = 0x1Eu;                           /* 0x28E80/0x28E8C */
}

#define DS_000A89C4 0x000A89C4u   /* no symbols.h name: [char] string id (0xC6..) */
#define DS_000A89E0 0x000A89E0u   /* no symbols.h name: [char] string id (0xCD..) */
#define DS_000A89FC 0x000A89FCu   /* no symbols.h name: [char] string id (0xD4..) */
#define DS_000A8998 0x000A8998u   /* no symbols.h name: 0x274FC's descriptor */
#define DS_000A76D0 0x000A76D0u   /* no symbols.h name: the badge y word (0x980) */
#define DS_00104529 0x00104529u   /* no symbols.h name: DS_00104528's second byte */
#define DS_001077F1 0x001077F1u   /* no symbols.h name: slot 0's +0x41 */
#define DS_00107885 0x00107885u   /* no symbols.h name: slot 1's +0x41 */
#define DS_00108134 0x00108134u   /* no symbols.h name: DS_00108131's high byte */

/* ---- 0x28CC8, the join poll of modes 6 and 0xC (record §48-J) ------------ */

/* 0x28CC8 — record §48-J. Loops ECX = side 0..1 (EBX = side + 1) and skips a
 * side whose bit side + 1 is set in DS_00104B1F (0x28CD7 `test eax,ebx`, AL
 * zero-extended). The first side without its bit decides:
 * - no credit (0x2C060 == 0): 0x2C1C8(side, 0x1D) (EBX = 0 is not read), 0;
 * - its start mask 0x9ACBC[side] newly pressed in DS_001088E4: 0x2C2B0(side,
 *   0x1D), DS_00104B1F |= side + 1, then 0x2CA7C(1), and side + 1 (EBX);
 * - otherwise 0x2C178(side, 0x3A00 with the DS_00104529 bit 1, else 0x1D), 0.
 * With both bits set it returns 0 and draws nothing. The join stores
 * DS_00104B1F (0x28D13) before 0x2CA7C tests it (0x2CA93), so the spend never
 * debits: the credit is only required, by 0x2C060. EBX/ECX survive 0x2C060
 * (0x2CAA8 writes EAX only, 0x2CA2C pushes and pops EDX), 0x2C2B0 (pushes
 * and pops them) and 0x2CA7C (EAX only). EBX/ECX/EDX are pushed and popped.
 * Callers: 0x2525F (mode 6) and 0x25349 (mode 0xC), each passing the result
 * minus one to 0x28DA4. */
u32 flow_join_poll(void)
{
    for (u32 side = 0; side < 2u; side++) {             /* 0x28CCB, 0x28D56..0x28D5A */
        u32 bit = side + 1u;                            /* 0x28CCF */
        if ((DSB(DS_00104B1F) & bit) != 0u) continue;   /* 0x28CCD..0x28CD9 */
        if (config_credit_ready() == 0u) {              /* 0x28CDF 0x2C060 */
            prompt_insert_coin(side, 0x1D);             /* 0x28D42..0x28D4B 0x2C1C8 */
            return 0u;                                  /* 0x28D50 */
        }
        if ((DSD(DS_0009ACBC + side * 4u) & DSD(DS_001088E4)) != 0u) {  /* 0x28CE8..0x28CF4 */
            prompt_side_erase((s32)side, 0x1D);         /* 0x28CFD 0x2C2B0 */
            DSB(DS_00104B1F) = (u8)(DSB(DS_00104B1F) | bit);   /* 0x28D02..0x28D13 */
            (void)config_credit_spend(1u);              /* 0x28D0E/0x28D19 0x2CA7C */
            return bit;                                 /* 0x28D1E */
        }
        prompt_press_start(side, (DSB(DS_00104529) & 2u) != 0u
                                     ? 0x3A00 : 0x1D);  /* 0x28D24..0x28D3B 0x2C178 */
        return 0u;                                      /* 0x28D40 -> 0x28D60 */
    }
    return 0u;                                          /* 0x28D60 */
}

/* 0x28130 — record §48-Q. The match-result caption on the dword
 * DS_00104AD4 (0x2813C `jz` on -1, then the unsigned 0x28144 `jc`/`jbe` and
 * 0x28152 `jz` against 1 and 2):
 * - -1: on the byte DS_00104B16, 0 -> the string 0xA89C4[slot 0's +0x7A], 1
 *   -> 0xA89C4[slot 1's +0x7A], 2 -> 0x43, drawn by 0x2F510 at col -1, row 8
 *   (EDX, kept by 0x1C500) with mode 0x4000; above 2 nothing is drawn. Then
 *   the 0x24 voice (0x282B6);
 * - 0 / 1: the 0x24 voice when the signed byte DS_001088F2 (0x28164 `mov
 *   eax,[0x1088ef]; sar eax,0x18`) is < 1 or the side's slot +0x63 is
 *   non-zero; then 0xA89E0[c] at row 8 and 0xA89FC[c] at row 0xB, c = that
 *   side's +0x7A, both with mode 0x4000;
 * - 2: the string 0x42 at row 8, then the 0x24 voice;
 * - anything else: nothing.
 * EBX/ECX/EDX are pushed and popped. The `test edx,edx; jnz` at 0x2815E runs
 * with EDX = 0 and is never taken. Callers: 0x275A3 (0x274FC), 0x297C9
 * (0x296B8), 0x28599 (0x28468, mode 8, record §49-C) and 0x288C7 (0x28788,
 * mode 9, record §48-Y). */
void flow_match_result_text(void)
{
    u32 r = DSD(DS_00104AD4);                           /* 0x28133 */
    if (r == 0xFFFFFFFFu) {                             /* 0x28139/0x2813C */
        u32 b = DSB(DS_00104B16);                       /* 0x28257 */
        u32 id;
        if (b == 0u)                                    /* 0x28268 */
            id = DSD(DS_000A89C4 + (u32)DSB(DS_0010782A) * 4u);     /* 0x2826C..0x28278 */
        else if (b == 1u)                               /* 0x28260 `jbe` */
            id = DSD(DS_000A89C4 + (u32)DSB(DS_001078BE) * 4u);     /* 0x28281..0x2828D */
        else if (b == 2u)                               /* 0x28264 */
            id = 0x43u;                                 /* 0x28296 */
        else
            id = 0xFFFFFFFFu;                           /* 0x28266 `jmp 0x282B6` */
        if (id != 0xFFFFFFFFu)
            text_cursor_hold_font2(-1, 8, game_string_get(id),
                                   0x4000u);            /* 0x282A5 0x1C500, 0x282B1 0x2F510 */
        /* PORT: 0x282BB 0x2C3FC(0x24) voice, not wired (record §45-A). */
        return;
    }
    if (r <= 1u) {                                      /* 0x28147 `jc`, 0x28149 `jbe` */
        u32 c = r == 0u ? (u32)DSB(DS_0010782A)         /* 0x28186 */
                        : (u32)DSB(DS_001078BE);        /* 0x281FC */
        /* PORT: 0x2817F/0x281F5 0x2C3FC(0x24) voice, not wired (record
         * §45-A); its gate (the signed byte DS_001088F2 < 1, or the side's
         * slot +0x63 != 0, 0x28164..0x28178 / 0x281DA..0x281EE) reads only. */
        text_cursor_hold_font2(-1, 8, game_string_get(DSD(DS_000A89E0 + c * 4u)),
                               0x4000u);                /* 0x2819C 0x1C500, 0x281A8 0x2F510 */
        text_cursor_hold_font2(-1, 0xB, game_string_get(DSD(DS_000A89FC + c * 4u)),
                               0x4000u);                /* 0x281C5 0x1C500, 0x281D1 0x2F510 */
        return;
    }
    if (r == 2u) {                                      /* 0x2814F/0x28152 */
        text_cursor_hold_font2(-1, 8, game_string_get(0x42u),
                               0x4000u);                /* 0x28250 -> 0x282A5, 0x282B1 */
        /* PORT: 0x282BB 0x2C3FC(0x24) voice, not wired (record §45-A). */
    }
}

#define DS_00104890 0x00104890u   /* no symbols.h name: [side] 0x27254's slot copy (0x94) */
#define DS_001049B8 0x001049B8u   /* no symbols.h name: [side] 0x27254's record copy (0x68) */

/* 0x27254 — record §48-C. Both sides' snapshot (ECX = side, EDI = 0x104890 +
 * side * 0x94, ESI = 0x1049B8 + side * 0x68, EBP = side * 0x94; EBX/ECX/EDX/
 * ESI/EDI/EBP pushed and popped): 0x33ACC(side, EDI, ESI) copies the slot and
 * its record, 0x3C16C and 0x3C148 zero the record's motion words, and the
 * record's +0x24 dword = 0 (0x27295, EAX = the slot's record pointer, loaded
 * after the three calls). 0x33ACC clobbers only EAX/EDX (ECX/ESI/EDI pushed
 * and popped); 0x3C16C/0x3C148 push and pop EDX. The two copies are
 * contiguous: side 1's slot copy ends at 0x1049B8. Its only caller is
 * 0x2730F (0x272DC). */
void flow_match_snapshot(void)
{
    u32 c;
    for (c = 0; c < 2u; c++) {                          /* 0x27264, 0x27294..0x2729F */
        fighter_33acc(c, DS_00104890 + c * 0x94u,
                      DS_001049B8 + c * 0x68u);         /* 0x2726C..0x27272 0x33ACC */
        fighter_3c16c(c);                               /* 0x27279 0x3C16C */
        fighter_3c148(c);                               /* 0x27286 0x3C148 */
        DSD(DSD(DS_001077B0 + c * 0x94u) + 0x24u) = 0u; /* 0x2728B/0x27295 */
    }
}

/* 0x278B0 — record §48-C. The continue screen's opening (EBX/ECX/EDX pushed
 * and popped): DS_00104B1F = 0 and DS_00105C04 = 0 (AH, 0x278B5 `xor ah,ah`),
 * DS_00108110 = 0xF (DL); the string 0x41 by 0x2F198 at col -1, row 0xA
 * (EDX, kept by 0x1C500's push/pop), mode 0x1000 (ECX, loaded at 0x278B7;
 * 0x1C500 does not write it and its 0x474E4 pushes and pops it); then
 * 0x2F434(col -1, row 0xE, the signed byte DS_00108110, width 2, pad 1, mode
 * 0x4002) (0x278FD `mov ebx,[0x10810d]` / 0x27905 `sar ebx,0x18`: the dword's
 * top byte is 0x108110, just stored, so the countdown 15); and mode 0xE. Its
 * only caller is 0x27314 (0x272DC). */
void flow_continue_open(void)
{
    DSB(DS_00104B1F) = 0u;                              /* 0x278BC (AH) */
    DSB(DS_00108110) = 0xFu;                            /* 0x278C2 (DL) */
    DSB(DS_00105C04) = 0u;                              /* 0x278C8 (AH) */
    text_cursor_set(-1, 0xA, game_string_get(0x41u),
                    0x1000u);                           /* 0x278D8 0x1C500, 0x278E4 0x2F198 */
    text_number_set(-1, 0xE, (s32)(s8)DSB(DS_00108110), 2, 1u,
                    0x4002u);                           /* 0x278E9..0x27905, 0x27908 0x2F434 */
    DSW(DS_00104B00) = 0xEu;                            /* 0x2790D */
}

/* 0x272DC — record §48-C. Mode 0xC's round-end test (EDX pushed and popped;
 * EAX clobbered). The winner w = (s8)DS_0010810D (0x272DD `mov edx,[0x10810a]`
 * / 0x272E3 `sar edx,0x18`): when w's slot +0x5A byte (zero-extended, 0x272FB)
 * is at least 0x78 (0x27303 `jl`), the 0xD3 voice, 0x27254 and 0x278B0 (the
 * continue screen, mode 0xE). Otherwise, when the DS_00104B12 side's +0x5A is
 * at least 0x78 (0x27340 `jl`): the 0x27 and 0x22 voices, w's slot +0x41 |=
 * 0x10 (w re-read at 0x27356) and mode 0xD. Else nothing. Its only caller is
 * 0x274EC (0x27380). */
void flow_arena_ko_check(void)
{
    s32 w = (s32)(s8)DSB(DS_0010810D);                  /* 0x272DD/0x272E3 */
    if (DSB(DS_0010780A + (u32)w * 0x94u) >= 0x78u) {   /* 0x272F4..0x27303 */
        /* PORT: 0x2730A 0x2C3FC(0xD3) voice, not wired (record §45-A). */
        flow_match_snapshot();                          /* 0x2730F 0x27254 */
        flow_continue_open();                           /* 0x27314 0x278B0 */
        return;                                         /* 0x27319 */
    }
    if (DSB(DS_0010780A + (u32)DSB(DS_00104B12) * 0x94u) < 0x78u)
        return;                                         /* 0x2731B..0x27340 */
    /* PORT: 0x27347 0x2C3FC(0x27) and 0x27351 0x2C3FC(0x22) voices, not wired
     * (record §45-A). */
    w = (s32)(s8)DSB(DS_0010810D);                      /* 0x27356/0x2735C */
    DSB(DS_001077F1 + (u32)w * 0x94u) =
        (u8)(DSB(DS_001077F1 + (u32)w * 0x94u) | 0x10u);    /* 0x2736D */
    DSW(DS_00104B00) = 0xDu;                            /* 0x27375 */
}

/* 0x27380 — record §48-C. Mode 0xC's arena frame (0x24C5C case 0xC's no-join
 * arm, 0x2535D `call 0x27380`, then 0x25362 `jmp 0x2540F`; its only caller).
 * EBX/ECX/EDX are pushed and popped; EAX is clobbered, and 0x2540F's 0x2A31C
 * call does not read it. First it undoes the mode-0xC tail's blink (0x25487,
 * game_frame): when the DS_00104B12 slot's +0x41 bit 0 is set and its +0x42
 * bit 3 clear, and its record's pset word (DS_001014EC + +0x56 * 0x20) is the
 * 0x1E1 blink sprite with bit 15 masked (0x273CB `and dh,0x7f`, the compare
 * copy only), the word gets DS_00104AF6 back (the whole word). Then the arena
 * steps of 0x263F4 minus 0x49C78/0x1282C, with the projection block gated:
 * 0x3C5CC, 0x16D58 per side, the two position latches; only while the byte
 * DS_001078FA is 2 (0x27427 `cmp eax,2` on the zero-extended byte) 0x17FA0
 * twice, 0x17580, 0x1958C, 0x19068(0), 0x17FA0 twice and 0x1975C; then
 * 0x3CB68, 0x35658 per side, 0x12DA8, 0x1DA08 (the pulse countdowns),
 * 0x272DC (the round-end test, which may store mode 0xD or 0xE) and
 * DS_00104AEC |= 2. The four 0x17FA0 calls take the same arguments as the
 * first four of 0x263F4's six (0x27430..0x274C2); 0x263F4's third pair,
 * after its 0x1975C, has no counterpart here. */
void game_mode_0c_step(void)
{
    u32 slot = DS_001077B0 + (u32)DSB(DS_00104B12) * 0x94u; /* 0x27383..0x27399 */
    if ((DSB(slot + 0x41u) & 1u) != 0u                  /* 0x2739C/0x273A3 */
            && (DSB(slot + 0x42u) & 8u) == 0u) {        /* 0x273A5/0x273AC */
        u32 ps = DSD(DS_001014EC)
                 + (u32)DSW(DSD(slot) + 0x56u) * 0x20u; /* 0x273AE..0x273C6 */
        if ((DSW(ps) & 0x7FFFu) == 0x1E1u)              /* 0x273C8..0x273DA */
            DSW(ps) = DSW(DS_00104AF6);                 /* 0x273DC/0x273E3 */
    }
    fight_slot_clear();                                 /* 0x273E6 0x3C5CC */
    camera_screen_base(0, (s32)DSB(DS_0010782A));       /* 0x273EB..0x273F5 0x16D58 */
    camera_screen_base(1, (s32)DSB(DS_001078BE));       /* 0x273FA..0x27407 0x16D58 */
    DSD(DS_001077E8) = DSD(DS_001077E4);                /* 0x2740C/0x27411 */
    DSD(DS_0010787C) = DSD(DS_00107878);                /* 0x27416/0x2741B */
    if (DSB(DS_001078FA) == 2u) {                       /* 0x27420..0x2742A */
        camera_project(0, DS_00100B08, DS_00100B00, DS_00100B62,
                       DS_00100B60, DS_00100AF0);       /* 0x2744B 0x17FA0 */
        camera_project(1, DS_00100B0C, DS_00100B04, DS_00100B63,
                       DS_00100B61, DS_00100AF4);       /* 0x2746E 0x17FA0 */
        camera_decay();                                 /* 0x27473 0x17580 */
        fighter_pass_a();                               /* 0x27478 0x1958C */
        fighter_pass_b(0u);                             /* 0x2747F 0x19068 */
        camera_project(0, DS_00100B08, DS_00100B00, DS_00100B62,
                       DS_00100B60, DS_00100AF0);       /* 0x2749F 0x17FA0 */
        camera_project(1, DS_00100B0C, DS_00100B04, DS_00100B63,
                       DS_00100B61, DS_00100AF4);       /* 0x274C2 0x17FA0 */
        fighter_think();                                /* 0x274C7 0x1975C */
    }
    fight_slot_pass();                                  /* 0x274CC 0x3CB68 */
    fight_hud_pass(0u);                                 /* 0x274D3 0x35658 */
    fight_hud_pass(1u);                                 /* 0x274DD 0x35658 */
    camera_y_commit();                                  /* 0x274E2 0x12DA8 */
    fight_hud_pulse();                                  /* 0x274E7 0x1DA08 */
    flow_arena_ko_check();                              /* 0x274EC 0x272DC */
    DSB(DS_00104AEC) = (u8)(DSB(DS_00104AEC) | 2u);     /* 0x274F1 */
}

/* ---- mode 0xE, the continue screen 0x27A2C and its callees (record §48-E) - */

/* 0x42F60 — record §48-E. EAX = side (kept in EDX; EDX pushed and popped).
 * 0 when no credit is ready (0x2C060, which keeps EDX: 0x2CAA8 writes EAX
 * only and 0x2CA2C pushes and pops EDX) or when the side's start mask
 * 0x9ACBC[side] is not newly pressed in DS_001088E4 (`test [edx*4+0x9acbc],
 * eax`, the side used whole). Otherwise one credit is spent and
 * DS_00105C04 = 1, and the result is 1. The spend is chosen by 0x2CA78,
 * which is `xor eax,eax; ret` and always 0, so 0x42F92's 0x2CA7C(1) always
 * runs; the other arm (0x2CAA8, `inc eax; sar eax,1`, then 0x2CA48, which
 * does not read EAX) is dead. Both results are discarded. Callers: 0x27A37
 * (0x27A2C) and the unported 0x42CB4 (three sites). */
u32 flow_continue_poll(u32 side)
{
    if (config_credit_ready() == 0u) return 0u;         /* 0x42F63 0x2C060, 0x42F6A */
    if ((DSD(DS_0009ACBC + side * 4u) & DSD(DS_001088E4)) == 0u)
        return 0u;                                      /* 0x42F6C..0x42F78, 0x42FAA */
    if (config_credit_zero() != 0u) {                   /* 0x42F7A 0x2CA78, 0x42F81 */
        (void)config_not_free_play();                   /* 0x42F83 0x2CAA8 */
        (void)config_credit_take();                     /* 0x42F8B 0x2CA48 */
    } else {
        (void)config_credit_spend(1u);                  /* 0x42F92/0x42F97 0x2CA7C */
    }
    DSB(DS_00105C04) = 1u;                              /* 0x42F9C */
    return 1u;                                          /* 0x42FA3 */
}

/* 0x2791C — record §48-E. The continue taken (EBX/ECX/EDX/ESI/EDI pushed and
 * popped). The continue screen is erased: string 0x41's length (0x1C500,
 * then `repne scasb`) in cells at col -1 (centred by 0x2F388), row 0xA; the
 * "PRESS START" prompt through 0x2C088(col -1, row 0xC, the side (s8)
 * DS_0010810D); 0x1C cells at rows 0xE and 0xF (the countdown). Then both
 * sides' 0x27254 snapshots come back through 0x33B00(side, 0x104890 + side *
 * 0x94, 0x1049B8 + side * 0x68) (ECX = side, EDI/ESI the copies; 0x2F388 and
 * 0x33B00 push and pop ECX/ESI/EDI, 0x2C088 ECX/ESI). Then 0x1D764 on the
 * side (s8)DS_0010810D, that slot's byte +0x5B = DS_00104B0B,
 * 0x46534(DS_00104B12, -2) (`xor eax,eax; mov al`, EDX = 0xFFFFFFFE), the
 * side re-read, mode 0xC (the word, DX) and its slot's +0x41 &= 0xE7 (AH,
 * read before the mode store, written after it). Its only caller is 0x27A64
 * (0x27A2C). */
void flow_continue_take(void)
{
    s32 w;
    u8 f;
    s32 n = (s32)strlen((const char *)game_string_get(0x41u)); /* 0x27926 0x1C500, 0x2792B..0x2793B */
    text_cells_release_count(-1, 0xA, n);               /* 0x2792D..0x27943 0x2F388 */
    prompt_press_start_clear(-1, 0xC,
                             (u32)(s32)(s8)DSB(DS_0010810D));   /* 0x27948..0x27960 0x2C088 */
    text_cells_release_count(-1, 0xE, 0x1C);            /* 0x27965..0x27979 0x2F388 */
    text_cells_release_count(-1, 0xF, 0x1C);            /* 0x2797E..0x2798F 0x2F388 */
    for (u32 c = 0; c < 2u; c++)                        /* 0x2798D, 0x2799F..0x279AC */
        fighter_state_33b00(c, DS_00104890 + c * 0x94u,
                            DS_001049B8 + c * 0x68u);   /* 0x27994..0x2799A 0x33B00 */
    fight_hud_side_reset((u32)(s32)(s8)DSB(DS_0010810D));   /* 0x279AE..0x279B6 0x1D764 */
    w = (s32)(s8)DSB(DS_0010810D);                      /* 0x279BB/0x279C1 */
    DSB(DS_0010780B + (u32)w * 0x94u) = DSB(DS_00104B0B);   /* 0x279D2/0x279D7 */
    fighter_46534(DSB(DS_00104B12), -2);                /* 0x279DE..0x279EA 0x46534 */
    w = (s32)(s8)DSB(DS_0010810D);                      /* 0x279EF/0x279F5 */
    f = (u8)(DSB(DS_001077F1 + (u32)w * 0x94u) & 0xE7u);    /* 0x27A06/0x27A12 */
    DSW(DS_00104B00) = 0xCu;                            /* 0x27A15 (DX) */
    DSB(DS_001077F1 + (u32)w * 0x94u) = f;              /* 0x27A1C */
}

/* 0x27A2C — record §48-E. Mode 0xE's handler, the continue screen 0x278B0
 * opens (0x24C5C case 0xE, the table entry 0x25371 `call 0x27a2c`, then
 * 0x25376 `jmp 0x2540F`; its only caller). EBX/ECX/EDX are pushed and
 * popped. The side w = (s8)DS_0010810D (`mov eax,[0x10810a]; sar eax,0x18`).
 * - 0x42F60(w) non-zero (the side's start, with a credit spent): DS_00104B1F
 *   = (the byte DS_0010810D != 0) + 1 (`setne al; inc eax`), the 0x2DAE4
 *   audit, 0x2791C (the continue taken, mode 0xC) and 0x41310(w, 1), return.
 * - Otherwise a tick is forced when DS_00105C04 is set (DS_00108110 = 0xA,
 *   DS_00105C04 = 0 by DH after `xor dh,dh`, EAX = 1), else by a newly
 *   pressed button: 0x4F778(1) with the DS_00104B1F bit 0, else 0x4F778(0)
 *   with bit 1 (AL, `and eax,0xff`), else none (EAX = 0 from the `and`).
 * - On the frame word's DS_000EF6DC & 0x3F == 0 (the zero-extended word,
 *   0x27ACD..0x27ADF) or a forced tick, the byte DS_00108110 is decremented.
 *   Below 0 (signed, 0x27AF5 `jge`): the 0x27 and 0x22 voices, 0x29D60 (a
 *   bare `ret`), 0x29B74 (darken all, mode 0x15), DS_00104B25 = CH = 0 (0x27B0B
 *   `xor ch,ch`) and the word DS_00104AFA = DX = 0x1E (loaded at 0x27AFC for
 *   the first voice; 0x2C3FC pushes and pops EBX/EDX/EDI and never names ECX,
 *   0x29B74 pushes and pops EBX/ECX/EDX), return. Otherwise 0x2F434(col -1,
 *   row 0xE, the signed byte DS_00108110, width 2, pad 1, mode 0x4002)
 *   (0x27B41 `mov ebx,[0x10810d]` / 0x27B49 `sar ebx,0x18`: the dword's top
 *   byte is 0x108110, the new count).
 * - While the signed byte DS_00108110 is below 0xE (0x27B59 `cmp eax,0xe;
 *   jge`): with a credit (0x2C060) the "PRESS START" blink 0x2C0F4(col -1,
 *   row 0xC, w, ECX = 0: the string form); else the "INSERT 1 COIN" blink
 *   0x2C1D4(col 0xE, row 0xC) (EBX = 1 is not read: 0x2C1D4 loads EBX before
 *   any use). Then DS_00104AEC |= 2. */
void game_mode_0e_step(void)
{
    u32 tick;
    if (flow_continue_poll((u32)(s32)(s8)DSB(DS_0010810D)) != 0u) {  /* 0x27A2F..0x27A3E 0x42F60 */
        DSB(DS_00104B1F) = (u8)((DSB(DS_0010810D) != 0u ? 1u : 0u) + 1u);  /* 0x27A40..0x27A55 */
        /* PORT: 0x27A5F 0x2DAE4(0x11, 1), the audit add, is deferred (spec
         * §7), as in 0x28E53 (0x28DA4) and 0x2D962 (config.c). */
        flow_continue_take();                           /* 0x27A64 0x2791C */
        fighter_41310((u32)(s32)(s8)DSB(DS_0010810D), 1);   /* 0x27A69..0x27A76 0x41310 */
        return;                                         /* 0x27A7B */
    }
    if (DSB(DS_00105C04) != 0u) {                       /* 0x27A7F/0x27A86 */
        DSB(DS_00108110) = 0xAu;                        /* 0x27A88 */
        DSB(DS_00105C04) = 0u;                          /* 0x27A8F/0x27A96 (DH) */
        tick = 1u;                                      /* 0x27A91 */
    } else if ((DSB(DS_00104B1F) & 1u) != 0u) {         /* 0x27A9E..0x27AAA */
        tick = frontend_buttons_pressed(1u) & 0xFFu;    /* 0x27AAC 0x27AC3 0x4F778, 0x27AC8 */
    } else if ((DSB(DS_00104B1F) & 2u) != 0u) {         /* 0x27AB3..0x27ABF */
        tick = frontend_buttons_pressed(0u) & 0xFFu;    /* 0x27AC1 0x27AC3 0x4F778, 0x27AC8 */
    } else {
        tick = 0u;                                      /* 0x27ABF (EAX = 0) */
    }
    if (((u32)DSW(DS_000EF6DC) & 0x3Fu) == 0u || tick != 0u) {   /* 0x27ACD..0x27AE3 */
        s8 n = (s8)(DSB(DS_00108110) - 1u);             /* 0x27AE5/0x27AEB */
        DSB(DS_00108110) = (u8)n;                       /* 0x27AED */
        if (n < 0) {                                    /* 0x27AF3/0x27AF5 */
            /* PORT: 0x27B01 0x2C3FC(0x27, EDX = 0x1E) and 0x27B0D
             * 0x2C3FC(0x22) voices, not wired (record §45-A). */
            /* 0x27B12 0x29D60 is a ret-only no-op. */
            frontend_darken_all();                      /* 0x27B17 0x29B74 */
            DSB(DS_00104B25) = 0u;                      /* 0x27B1C (CH) */
            DSW(DS_00104AFA) = 0x1Eu;                   /* 0x27B22 (DX) */
            return;                                     /* 0x27B2C */
        }
        text_number_set(-1, 0xE, (s32)(s8)DSB(DS_00108110), 2, 1u,
                        0x4002u);                       /* 0x27B2D..0x27B49 0x27B4C 0x2F434 */
    }
    if ((s32)(s8)DSB(DS_00108110) < 0xE) {              /* 0x27B51..0x27B5C */
        if (config_credit_ready() != 0u)                /* 0x27B5E 0x2C060, 0x27B65 */
            prompt_press_start_blink(-1, 0xC, (u32)(s32)(s8)DSB(DS_0010810D),
                                     0u);               /* 0x27B67..0x27B7C 0x2C0F4 */
        else
            prompt_insert_coin_blink(0xE, 0xC);         /* 0x27B83..0x27B92 0x2C1D4 */
    }
    DSB(DS_00104AEC) = (u8)(DSB(DS_00104AEC) | 2u);     /* 0x27B97 */
}

/* 0x274FC — record §48-Q. Mode 0xD's handler (0x24C5C case 0xD, the table
 * entry 0x25367; its only caller). The arena frame's tail steps: 0x3C5CC,
 * 0x16D58 per side with the slot's +0x7A, the two position latches
 * (+0x34 -> +0x38), 0x35658 per side, 0x19068(1) and 0x12DA8. Then
 * DS_00104AEC |= 2, and nothing more while the byte DS_00104B0C is 0. Else
 * DS_00104B0C = 0 and the byte DS_00104B21 is incremented:
 * - reaching 7: the 0x2A voice, 0x28130, 0x2C2B0((s8)(DS_0010810D ^ 1),
 *   0x1D), then with DS_00104528's second byte bit 1 the actor 0xA8998 at
 *   (0x2A00, a3 0xFF, a4 0x1800, a5 0), else the strings 0x3F and 0x40 by
 *   0x2F198 at col -1, rows 0xB and 0xD, mode 0x2000. The winner w =
 *   (s8)DS_0010810D (0x27622 `mov ebx,[0x10810a]; sar ebx,0x18`) earns
 *   5000000 through 0x41310 when its slot +0x3C is a multiple of 100
 *   (unsigned DIV, 0x2764B `test dx,dx`), else 1000000 when the remainder is
 *   <= 10 (signed); 0x4DBEC; DS_000F0AFE = 3, mode 0xF, DS_00104AFE = 0x258
 *   and w's slot +0x41 |= 0x10;
 * - otherwise the loser s = DS_00104B12: its record's +0x48 = 0, 0x2B150 on
 *   its slot's +4 record, DS_001078FA - 1, 0x2716C(s), 0x46534(s, 1),
 *   0x33C18(s), the entrance 0xA8628[(s8)DS_0010816A[s]](s) (EAX = s),
 *   DS_000F0AFF = DS_0010810D, DS_000F0AFE = 0, 0x1D764(s), 0x1D838(s, that
 *   character, y = the word 0xA76D0), w's slot +0x41 &= 0xEF, DS_00104B0A ^=
 *   1 (0x25 + (the result != 0) is the voice) and mode 0xC (EDX, kept by
 *   0x2C3FC). */
void game_mode_0d_step(void)
{
    s32 w;
    u32 s, ch;
    u8 n;
    fight_slot_clear();                                 /* 0x274FF 0x3C5CC */
    camera_screen_base(0, (s32)DSB(DS_0010782A));       /* 0x27508 0x16D58 */
    camera_screen_base(1, (s32)DSB(DS_001078BE));       /* 0x2751A 0x16D58 */
    DSD(DS_001077E8) = DSD(DS_001077E4);                /* 0x27525/0x2752A */
    DSD(DS_0010787C) = DSD(DS_00107878);                /* 0x2752F/0x27534 */
    fight_hud_pass(0u);                                 /* 0x2753B 0x35658 */
    fight_hud_pass(1u);                                 /* 0x27545 0x35658 */
    fighter_pass_b(1u);                                 /* 0x2754F 0x19068 */
    camera_y_commit();                                  /* 0x27554 0x12DA8 */
    DSB(DS_00104AEC) = (u8)(DSB(DS_00104AEC) | 2u);     /* 0x27559..0x27568 */
    if (DSB(DS_00104B0C) == 0u) return;                 /* 0x2756E/0x27570 */
    n = (u8)(DSB(DS_00104B21) + 1u);                    /* 0x27576/0x27580 */
    DSB(DS_00104B0C) = 0u;                              /* 0x27582 (DH) */
    DSB(DS_00104B21) = n;                               /* 0x2758A */
    if (n == 7u) {                                      /* 0x27590 `jnz` */
        u32 rem;
        /* PORT: 0x2759E 0x2C3FC(0x2A) voice, not wired (record §45-A). */
        flow_match_result_text();                       /* 0x275A3 0x28130 */
        prompt_side_erase((s32)(s8)(DSB(DS_0010810D) ^ 1u),
                          0x1D);                        /* 0x275A8..0x275B7 0x2C2B0 */
        if ((DSB(DS_00104529) & 2u) != 0u) {            /* 0x275BC/0x275C3 */
            (void)actor_spawn((const u32 *)(mem + DS_000A8998), 0x2A00u,
                              0xFFu, 0x1800u, 0u);      /* 0x275C5..0x275DB 0x2AE14 */
        } else {
            text_cursor_set(-1, 0xB, game_string_get(0x3Fu),
                            0x2000u);                   /* 0x275F1 0x1C500, 0x275FD 0x2F198 */
            text_cursor_set(-1, 0xD, game_string_get(0x40u),
                            0x2000u);                   /* 0x27611 0x1C500, 0x2761D 0x2F198 */
        }
        w = (s32)(s8)DSB(DS_0010810D);                  /* 0x27622/0x27628 */
        rem = DSD(DS_001077EC + (u32)w * 0x94u) % 100u; /* 0x27642/0x27649 */
        if ((u16)rem == 0u)                             /* 0x2764B `jnz` */
            fighter_41310((u32)w, 5000000);             /* 0x27650 0x41310 */
        else if ((s32)(u16)rem <= 0xA)                  /* 0x2765C `jg` */
            fighter_41310((u32)w, 1000000);             /* 0x27661 0x41310 */
        fight_4dbec();                                  /* 0x2766D 0x4DBEC */
        w = (s32)(s8)DSB(DS_0010810D);                  /* 0x27672/0x27678 */
        DSB(DS_000F0AFE) = 3u;                          /* 0x27690 */
        DSW(DS_00104B00) = 0xFu;                        /* 0x27696 */
        n = (u8)(DSB(DS_001077F1 + (u32)w * 0x94u) | 0x10u);    /* 0x2769D/0x276A9 */
        DSW(DS_00104AFE) = 0x258u;                      /* 0x276AC */
        DSB(DS_001077F1 + (u32)w * 0x94u) = n;          /* 0x276B3 */
        return;
    }
    s = DSB(DS_00104B12);                               /* 0x276C0 */
    DSB(DSD(DS_001077B0 + s * 0x94u) + 0x48u) = 0u;     /* 0x276D4/0x276DB */
    actor_set_dead(DSD(DS_001077B4 + s * 0x94u));       /* 0x276DF/0x276E6 0x2B150 */
    DSB(DS_001078FA) = (u8)(DSB(DS_001078FA) - 1u);     /* 0x276EB..0x276FA */
    flow_side_char_random(DSB(DS_00104B12));            /* 0x27700 0x2716C */
    fighter_46534(DSB(DS_00104B12), 1);                 /* 0x27711 0x46534 */
    fight_char_reset(DSB(DS_00104B12));                 /* 0x2771D 0x33C18 */
    s = DSB(DS_00104B12);                               /* 0x27724 */
    ch = (u32)(s32)(s8)DSB(DS_0010816A + s);            /* 0x27729/0x2772F */
    {
        void (*fn)(u32) = (void (*)(u32))(void *)
            fn_resolve(DSD(DS_000A8628 + ch * 4u));
        if (fn) fn(s);                                  /* 0x27732 */
        /* PORT: all seven entries of 0xA8628 are ported and registered
         * (0x24568, record §46-C; 0x40CB0, 0x49150, 0x15A34, 0x45FE8,
         * 0x40E64 and 0x24804, record §48-V), so the guard only skips a
         * character byte outside 0..6, which the raw would call through. */
    }
    DSB(DS_000F0AFF) = DSB(DS_0010810D);                /* 0x27739/0x2773E */
    DSB(DS_000F0AFE) = 0u;                              /* 0x2774C (CL) */
    fight_hud_side_reset(DSB(DS_00104B12));             /* 0x27752 0x1D764 */
    s = DSB(DS_00104B12);                               /* 0x27759 */
    fight_hud_badge_spawn(s, (u32)(s32)(s8)DSB(DS_0010816A + s),
                          (u32)DSW(DS_000A76D0));       /* 0x27760..0x27770 0x1D838 */
    w = (s32)(s8)DSB(DS_0010810D);                      /* 0x27775/0x2777B */
    DSB(DS_001077F1 + (u32)w * 0x94u) =
        (u8)(DSB(DS_001077F1 + (u32)w * 0x94u) & 0xEFu);    /* 0x2778C */
    n = (u8)(DSB(DS_00104B0A) ^ 1u);                    /* 0x27794/0x27799 */
    DSB(DS_00104B0A) = n;                               /* 0x2779B */
    /* PORT: 0x277B0 0x2C3FC(0x25 + (n != 0)) voice, not wired (record §45-A);
     * it pushes and pops EDX, so the mode word below is its EDX = 0xC. */
    DSW(DS_00104B00) = 0xCu;                            /* 0x277AB/0x277B5 */
}

/* 0x296B8 — record §48-Q. Mode 0x32's handler (0x24C5C case 0x32, the table
 * entry 0x251B2; its only caller). The prelude of 0x274FC (0x296BB..0x29724),
 * then nothing more while DS_00104B0C is 0. Else DS_00104B0C = 0 and
 * DS_00104B21 + 1, and with s = DS_00104B09 (zero-extended into ECX; the
 * 0x29750 `cmp ecx,-1; jz 0x29919` is never taken) the loser's record +0x48 =
 * 0, 0x2B150 on its slot's +4 record, the side's win count DS_00104AF0[s] + 1
 * and DS_001078FA - 1:
 * - either count 4 (0x297A9/0x297B9): the 0x2A voice, 0x28130, both slots'
 *   +0x41 |= 0x10, DS_0010810D = 1 and the string 0x65 when DS_00104AF1 <
 *   DS_00104AF0 (unsigned, 0x297F0 `jnc`), else 0 and 0x66, by 0x2F198 at
 *   col -1, row 0xB, mode 0x2000; 0x4DBEC; DS_000F0AFE = 3, mode 0x33 and
 *   DS_00104AFE = 0x258;
 * - otherwise the next character c = (s8)DS_00108134[s * 4 + the count]
 *   (0x29865 `mov ecx,[ebx + eax*4 + 0x108131]; sar ecx,0x18`): 0x292D4(s,
 *   c), 0x46534(s, 1), 0x33C18(s), the entrance 0xA8628[c](s) (EBX = 0; it
 *   keeps ECX), the slot's +0x63 = 0 and +0x7A = c, DS_000F0AFE = 0,
 *   DS_000F0AFF = s ^ 1, 0x1D764(s), 0x1D838(s, c, the word 0xA76D0) and the
 *   other slot's +0x41 &= 0xEF.
 * Both of the latter end at 0x29919: (s8)DS_0010810D's slot +0x41 &= 0xEF,
 * DS_00104B0A ^= 1 (the voice 0x25 + (the result != 0)) and mode 0x31. */
void game_mode_32_step(void)
{
    s32 w;
    u32 s, ch;
    u8 n, wins;
    fight_slot_clear();                                 /* 0x296BB 0x3C5CC */
    camera_screen_base(0, (s32)DSB(DS_0010782A));       /* 0x296C4 0x16D58 */
    camera_screen_base(1, (s32)DSB(DS_001078BE));       /* 0x296D6 0x16D58 */
    DSD(DS_001077E8) = DSD(DS_001077E4);                /* 0x296E1/0x296E6 */
    DSD(DS_0010787C) = DSD(DS_00107878);                /* 0x296EB/0x296F0 */
    fight_hud_pass(0u);                                 /* 0x296F7 0x35658 */
    fight_hud_pass(1u);                                 /* 0x29701 0x35658 */
    fighter_pass_b(1u);                                 /* 0x2970B 0x19068 */
    camera_y_commit();                                  /* 0x29710 0x12DA8 */
    DSB(DS_00104AEC) = (u8)(DSB(DS_00104AEC) | 2u);     /* 0x29715..0x29724 */
    if (DSB(DS_00104B0C) == 0u) return;                 /* 0x2972A/0x2972C */
    n = (u8)(DSB(DS_00104B21) + 1u);                    /* 0x29732/0x29742 */
    s = DSB(DS_00104B09);                               /* 0x2973C */
    DSB(DS_00104B0C) = 0u;                              /* 0x29744 (DH) */
    DSB(DS_00104B21) = n;                               /* 0x2974A */
    DSB(DSD(DS_001077B0 + s * 0x94u) + 0x48u) = 0u;     /* 0x29767/0x2976E */
    actor_set_dead(DSD(DS_001077B4 + s * 0x94u));       /* 0x29772/0x29779 0x2B150 */
    s = DSB(DS_00104B09);                               /* 0x29780 */
    wins = (u8)(DSB(DS_00104AF0 + s) + 1u);             /* 0x29785/0x2978D */
    DSB(DS_00104AF0 + s) = wins;                        /* 0x29795 */
    DSB(DS_001078FA) = (u8)(DSB(DS_001078FA) - 1u);     /* 0x2978F..0x297A3 */
    if (DSB(DS_00104AF0) == 4u || DSB(DS_00104AF1) == 4u) {  /* 0x2979D..0x297B9 */
        u32 id;
        /* PORT: 0x297C4 0x2C3FC(0x2A) voice, not wired (record §45-A). */
        flow_match_result_text();                       /* 0x297C9 0x28130 */
        DSB(DS_001077F1) = (u8)(DSB(DS_001077F1) | 0x10u);  /* 0x297CE */
        DSB(DS_00107885) = (u8)(DSB(DS_00107885) | 0x10u);  /* 0x297D5..0x297E4 */
        if (DSB(DS_00104AF1) < DSB(DS_00104AF0)) {      /* 0x297EA/0x297F0 `jnc` */
            DSB(DS_0010810D) = 1u;                      /* 0x29803 (BL) */
            id = 0x65u;                                 /* 0x297F4 */
        } else {
            DSB(DS_0010810D) = 0u;                      /* 0x2981C (DH) */
            id = 0x66u;                                 /* 0x29810 */
        }
        text_cursor_set(-1, 0xB, game_string_get(id),
                        0x2000u);                       /* 0x29809/0x29822 0x1C500, 0x29833 0x2F198 */
        fight_4dbec();                                  /* 0x29838 0x4DBEC */
        DSB(DS_000F0AFE) = 3u;                          /* 0x29844 (BH) */
        DSW(DS_00104B00) = 0x33u;                       /* 0x2984F */
        DSW(DS_00104AFE) = 0x258u;                      /* 0x29856 */
        return;
    }
    ch = (u32)(s32)(s8)DSB(DS_00108134 + s * 4u + wins);    /* 0x29861..0x2986C */
    flow_side_char_set(s, ch);                          /* 0x29871 0x292D4 */
    fighter_46534(DSB(DS_00104B09), 1);                 /* 0x29882 0x46534 */
    fight_char_reset(DSB(DS_00104B09));                 /* 0x2988E 0x33C18 */
    s = DSB(DS_00104B09);                               /* 0x29895 */
    {
        void (*fn)(u32) = (void (*)(u32))(void *)
            fn_resolve(DSD(DS_000A8628 + ch * 4u));
        if (fn) fn(s);                                  /* 0x2989C */
        /* PORT: as in 0x274FC, all seven entrances are registered (records
         * §46-C and §48-V); the guard only skips a character outside 0..6. */
    }
    s = DSB(DS_00104B09);                               /* 0x298A3 */
    DSB(DS_00107813 + s * 0x94u) = 0u;                  /* 0x298B9/0x298BB (DL ^ BL) */
    DSB(DS_0010782A + s * 0x94u) = (u8)ch;              /* 0x298C2 (CL) */
    DSB(DS_000F0AFE) = 0u;                              /* 0x298CB/0x298CF (DH ^ BH) */
    DSB(DS_000F0AFF) = (u8)(s ^ 1u);                    /* 0x298C9..0x298D5 */
    fight_hud_side_reset(s);                            /* 0x298DE 0x1D764 */
    fight_hud_badge_spawn(DSB(DS_00104B09), ch,
                          (u32)DSW(DS_000A76D0));       /* 0x298E3..0x298F3 0x1D838 */
    s = (u32)DSB(DS_00104B09) ^ 1u;                     /* 0x298F8..0x29901 */
    DSB(DS_001077F1 + s * 0x94u) =
        (u8)(DSB(DS_001077F1 + s * 0x94u) & 0xEFu);     /* 0x29911 */
    w = (s32)(s8)DSB(DS_0010810D);                      /* 0x29919/0x2991F */
    DSB(DS_001077F1 + (u32)w * 0x94u) =
        (u8)(DSB(DS_001077F1 + (u32)w * 0x94u) & 0xEFu);    /* 0x29930..0x29940 */
    n = (u8)(DSB(DS_00104B0A) ^ 1u);                    /* 0x2993A/0x29947 */
    DSB(DS_00104B0A) = n;                               /* 0x2994A */
    /* PORT: 0x29960 0x2C3FC(0x25 + (n != 0)) voice, not wired (record
     * §45-A); its EDX (0x31) survives into the mode word. */
    DSW(DS_00104B00) = 0x31u;                           /* 0x2995B/0x29965 */
}

/* ---- mode 5, the round start (record §48-U) ------------------------------ */

#define DS_000A8898 0x000A8898u   /* no symbols.h name: 3 round-card descriptor pointers */
#define DS_000A88A4 0x000A88A4u   /* no symbols.h name: the round card (DS_00104B14) */
#define DS_000A88A8 0x000A88A8u   /* no symbols.h name: 3 round-card descriptor pointers (bit 1) */
#define DS_000A88B4 0x000A88B4u   /* no symbols.h name: the round card (DS_00104B14, bit 1) */
#define DS_000A8884 0x000A8884u   /* no symbols.h name: the fight card descriptor (bit 1) */
#define DS_000BB6A0 0x000BB6A0u   /* no symbols.h name: the fight card descriptor */
#define DS_000BB68C 0x000BB68Cu   /* no symbols.h name: the round-win marker descriptor */
#define DS_00080994 0x00080994u   /* no symbols.h name: 22 spaces */
#define DS_00080C80 0x00080C80u   /* no symbols.h name: the string "TT" */
#define DS_00080C84 0x00080C84u   /* no symbols.h name: the string "EE" */
#define DS_00080C88 0x00080C88u   /* no symbols.h name: the string "XX" */

/* 0x256F4 — record §48-U. The round-win markers: for k below the byte
 * DS_00104AF2 (unsigned; 0x256FA `jbe` skips at 0) a zero DS_00104A88[k] takes
 * the spawn 0xBB68C at (0x1700 + k * 0x380, a3 0xFF, 0x900, a5 = EDX = 0,
 * the value just tested); then for k below DS_00104AF3 a zero DS_00104A98[k]
 * takes 0xBB68C at (0x3940 - k * 0x380, 0xFF, 0x900, a5 = EBX = 0). The bound
 * is re-read each pass; nothing caps k at the four slots. EBX..EBP are pushed
 * and popped. Callers: 0x25DA6 (0x25C88), 0x27D02/0x27D80/0x27DA5/0x27DBA
 * (0x27C48, record §48-K) and 0x29446 (0x29328, game_mode_30_step). */
void flow_win_markers_spawn(void)
{
    u32 k;
    for (k = 0; k < (u32)DSB(DS_00104AF2); k++) {       /* 0x2573F..0x25748 */
        if (DSD(DS_00104A88 + k * 4u) == 0u)            /* 0x25714 */
            DSD(DS_00104A88 + k * 4u) = actor_spawn(
                (const u32 *)(mem + DS_000BB68C), 0x1700u + k * 0x380u,
                0xFFu, 0x900u, 0u);                     /* 0x25718..0x2572F 0x2AE14 */
    }
    for (k = 0; k < (u32)DSB(DS_00104AF3); k++) {       /* 0x2578F..0x25798 */
        if (DSD(DS_00104A98 + k * 4u) == 0u)            /* 0x25764 */
            DSD(DS_00104A98 + k * 4u) = actor_spawn(
                (const u32 *)(mem + DS_000BB68C), 0x3940u - k * 0x380u,
                0xFFu, 0x900u, 0u);                     /* 0x25768..0x2577F 0x2AE14 */
    }
}

/* 0x4F37C — record §48-U. The round-timer field at col 0x13, row 1, mode
 * 0x4002 through 0x2F198 (the cursor moves): with DS_00104B1D == 2 the
 * string "TT" (0x80C80) and DS_001088F2 = 0x1E; with 3 "EE" (0x80C84) and
 * DS_001088F2 = 0x1E; else with DS_00104B14 != 0 "XX" (0x80C88), DS_001088F2
 * untouched; else DS_001088F2 = 0x3C, the number 0x3C at width 2, pad 0 and
 * mode 0x4000 by 0x2F528, and DS_00104AEC |= 1. EBX/ECX/EDX are pushed and
 * popped. Callers: 0x25E9F (0x25C88), 0x29534 (0x29328, game_mode_30_step)
 * and 0x25AC7 (0x25A84, no entrance). */
void flow_round_timer_draw(void)
{
    u8 b1d = DSB(DS_00104B1D);                          /* 0x4F37F */
    if (b1d == 2u) {                                    /* 0x4F385 */
        DSB(DS_001088F2) = 0x1Eu;                       /* 0x4F39B (CH) */
        text_cursor_set(0x13, 1, mem + DS_00080C80, 0x4002u);   /* 0x4F3A6 0x2F198 */
        return;
    }
    if (b1d == 3u) {                                    /* 0x4F3AF */
        DSB(DS_001088F2) = 0x1Eu;                       /* 0x4F3C5 (CL) */
        text_cursor_set(0x13, 1, mem + DS_00080C84, 0x4002u);   /* 0x4F3D0 0x2F198 */
        return;
    }
    if (DSB(DS_00104B14) != 0u) {                       /* 0x4F3D9 */
        text_cursor_set(0x13, 1, mem + DS_00080C88, 0x4002u);   /* 0x4F3F6 0x2F198 */
        return;
    }
    DSB(DS_001088F2) = 0x3Cu;                           /* 0x4F417 (BL) */
    text_number_draw_font2(0x13, 1, 0x3C, 2, 0u, 0x4000u);  /* 0x4F3FF..0x4F422 0x2F528 */
    DSB(DS_00104AEC) = (u8)(DSB(DS_00104AEC) | 1u);     /* 0x4F427 */
}

/* 0x25C1C — record §48-U. 0x29D60 (a bare `ret`); DS_00104AD8 = 0 (EDX);
 * 0x1DC6C(1) when DS_00104B1D == 2, else 0x1D890(1); with DS_00104B1D != 3
 * and DS_00104B14 != 0, 0x1D810(DS_00104B12); DS_00104B20 = 1 and 0x20EF8.
 * EDX is pushed and popped. Callers: 0x25CBC (0x25C88) and 0x2935C (0x29328,
 * game_mode_30_step). */
void flow_round_hud_init(void)
{
    /* 0x25C1D 0x29D60 is a ret-only no-op. */
    DSD(DS_00104AD8) = 0u;                              /* 0x25C2A */
    if (DSB(DS_00104B1D) == 2u)                         /* 0x25C30 */
        fight_hud_spawn_b(1u);                          /* 0x25C3A 0x1DC6C */
    else
        fight_hud_spawn(1u);                            /* 0x25C46 0x1D890 */
    if (DSB(DS_00104B1D) != 3u && DSB(DS_00104B14) != 0u)   /* 0x25C4B/0x25C54 */
        fight_select_marker_release(DSB(DS_00104B12));  /* 0x25C64 0x1D810 */
    DSB(DS_00104B20) = 1u;                              /* 0x25C69 */
    fight_round_reset();                                /* 0x25C70 0x20EF8 */
}

/* 0x25C88 — record §48-U. Mode 5's handler (0x24C5C case 5, the table entry
 * 0x2524C; its only caller). 0x3CB68, then on the byte DS_00104B25 - 1
 * (0x25C97 `dec al`, `cmp al,3; ja`, the table 0x25C78):
 * - 1 (0x25CAE): DS_00104B1B = DS_00104B15 = 1 (BL), 0x25C1C, then the round
 *   card into DS_00104AC0 at (0x2A00, a3 0xFF, 0x3600, a5 0): with
 *   DS_00104B14 the descriptor [0xA88B4] (DS_00104529 bit 1) or [0xA88A4];
 *   else [0xA88A8 + 4i] (bit 1) or [0xA8898 + 4i] with i = 2 when the byte
 *   DS_00104B1E equals the dword DS_00104ADC, 0 when it is 1, else 1. The
 *   eight dwords DS_00104A88..DS_00104AA4 = 0, 0x256F4, DS_00104B25 = 4,
 *   DS_00104B23 = 2, the word DS_00104AFE = 0x3C;
 * - 2 (0x25DD3): the fight card 0xA8884 (bit 1 and DS_00104B14) or 0xBB6A0
 *   at (0x2A00, 0xFF, 0x1200, 0) into DS_00104ACC, DS_00104AE8 |= 0x40, the
 *   voice 0xD9 (DS_00104B14) or 0xD7, DS_00104B25 = 4, DS_00104AFE = 0x3C and
 *   DS_00104B23 = 3;
 * - 3 (0x25E67): 0x2F4BC(-1, 5, 0x80994, 0) (22 spaces), 0x2B150 on
 *   DS_00104AC0 and then DS_00104ACC, each zeroed after (EDX = 0 survives
 *   0x2B150); 0x4F37C; the bytes DS_001078FC/DS_001078FE = 0; DS_00104ABC =
 *   (DS_00104B1F == 3) + 1; 0x32970; 0x32B00(DS_00104ABC, 1) when
 *   DS_00104B1E == 1; 0x32B4C(DS_00104ABC, 1) when DS_00104B14 == 0. Then
 *   with DS_00104B14 == 0 mode 6; else mode 0xC (0x25F20, before the call),
 *   the entrance 0xA8628[(s8)DS_0010816A[s]](s) with s = DS_00104B12 (EAX),
 *   0x1D838(s, that character, the word 0xA76D0), DS_000F0AFE = 0 (BH) and
 *   DS_000F0AFF = DS_0010810D;
 * - 4 (0x25F81): the word DS_00104AFE - 1, and DS_00104B25 = DS_00104B23
 *   once it is <= 0 (signed, 0x25F93 `jg`);
 * - 0 or above 4: nothing.
 * Every path ends with DS_00104AEC |= 2. EBX..EDI are pushed and popped. */
void game_mode_05_step(void)
{
    u32 s, ch;
    fight_slot_pass();                                  /* 0x25C8D 0x3CB68 */
    switch (DSB(DS_00104B25)) {                         /* 0x25C92..0x25CA6 */
    case 1u: {
        u32 desc;
        DSB(DS_00104B1B) = 1u;                          /* 0x25CB0 (BL) */
        DSB(DS_00104B15) = 1u;                          /* 0x25CB6 (BL) */
        flow_round_hud_init();                          /* 0x25CBC 0x25C1C */
        if (DSB(DS_00104B14) != 0u) {                   /* 0x25CC1 */
            desc = (DSB(DS_00104529) & 2u) != 0u        /* 0x25CCA */
                 ? DSD(DS_000A88B4) : DSD(DS_000A88A4); /* 0x25CE4 / 0x25CFF */
        } else {
            u32 i, b = DSB(DS_00104B1E);                /* 0x25D06/0x25D0E */
            if (b == DSD(DS_00104ADC)) i = 2u;          /* 0x25D13/0x25D17 */
            else if (b == 1u) i = 0u;                   /* 0x25D1E/0x25D23 */
            else i = 1u;                                /* 0x25D27 */
            desc = (DSB(DS_00104529) & 2u) != 0u        /* 0x25D2C/0x25D35 */
                 ? DSD(DS_000A88A8 + i * 4u)            /* 0x25D4B */
                 : DSD(DS_000A8898 + i * 4u);           /* 0x25D64 */
        }
        DSD(DS_00104AC0) = actor_spawn((const u32 *)(mem + desc), 0x2A00u,
                                       0xFFu, 0x3600u, 0u);  /* 0x25D6A 0x2AE14 */
        DSD(DS_00104A88) = 0u;                          /* 0x25D76 (ESI) */
        DSD(DS_00104A88 + 4u) = 0u;                     /* 0x25D7C */
        DSD(DS_00104A88 + 8u) = 0u;                     /* 0x25D82 */
        DSD(DS_00104A88 + 0xCu) = 0u;                   /* 0x25D88 */
        DSD(DS_00104A98) = 0u;                          /* 0x25D8E */
        DSD(DS_00104A98 + 4u) = 0u;                     /* 0x25D94 */
        DSD(DS_00104A98 + 8u) = 0u;                     /* 0x25D9A */
        DSD(DS_00104A98 + 0xCu) = 0u;                   /* 0x25DA0 */
        flow_win_markers_spawn();                       /* 0x25DA6 0x256F4 */
        DSB(DS_00104B25) = 4u;                          /* 0x25DAF (AH) */
        DSB(DS_00104B23) = 2u;                          /* 0x25DBA (DL) */
        DSW(DS_00104AFE) = 0x3Cu;                       /* 0x25DC0 (AX) */
        break;
    }
    case 2u: {
        u32 desc = ((DSB(DS_00104529) & 2u) != 0u && DSB(DS_00104B14) != 0u)
                 ? DS_000A8884 : DS_000BB6A0;           /* 0x25DD3..0x25E0E */
        DSD(DS_00104ACC) = actor_spawn((const u32 *)(mem + desc), 0x2A00u,
                                       0xFFu, 0x1200u, 0u);  /* 0x25E13 0x2AE14 */
        DSB(DS_00104AE8) = (u8)(DSB(DS_00104AE8) | 0x40u);  /* 0x25E1D */
        /* PORT: 0x25E39 0x2C3FC(DS_00104B14 != 0 ? 0xD9 : 0xD7) voice, not
         * wired (record §45-A). */
        DSB(DS_00104B25) = 4u;                          /* 0x25E45 (DL) */
        DSW(DS_00104AFE) = 0x3Cu;                       /* 0x25E4D (DI) */
        DSB(DS_00104B23) = 3u;                          /* 0x25E54 (DH) */
        break;
    }
    case 3u:
        text_cursor_hold(-1, 5, mem + DS_00080994, 0u); /* 0x25E67..0x25E78 0x2F4BC */
        actor_set_dead(DSD(DS_00104AC0));               /* 0x25E7D/0x25E82 0x2B150 */
        DSD(DS_00104AC0) = 0u;                          /* 0x25E8E (EDX) */
        actor_set_dead(DSD(DS_00104ACC));               /* 0x25E89/0x25E94 0x2B150 */
        DSD(DS_00104ACC) = 0u;                          /* 0x25E99 (EDX, kept by 0x2B150) */
        flow_round_timer_draw();                        /* 0x25E9F 0x4F37C */
        DSB(DS_001078FC) = 0u;                          /* 0x25EA6 (AH) */
        DSB(DS_001078FE) = 0u;                          /* 0x25EAC (AH) */
        DSD(DS_00104ABC) = (DSB(DS_00104B1F) == 3u ? 1u : 0u) + 1u;   /* 0x25EB2..0x25EC5 */
        /* PORT: 0x25ECA 0x32970, the run clock, is out of scope (spec §7);
         * the host clock owns wall time. */
        if (DSB(DS_00104B1E) == 1u)                     /* 0x25ECF..0x25ED9 */
            config_play_time_snap(DSD(DS_00104ABC), 1u);    /* 0x25EE5 0x32B00 */
        if (DSB(DS_00104B14) == 0u)                     /* 0x25EEA */
            config_play_time_snap_b(DSD(DS_00104ABC), 1u);  /* 0x25EFD 0x32B4C */
        if (DSB(DS_00104B14) == 0u) {                   /* 0x25F02 */
            DSW(DS_00104B00) = 6u;                      /* 0x25F6B */
            break;
        }
        s = DSB(DS_00104B12);                           /* 0x25F0B/0x25F0D */
        ch = (u32)(s32)(s8)DSB(DS_0010816A + s);        /* 0x25F12/0x25F1D */
        DSW(DS_00104B00) = 0xCu;                        /* 0x25F18/0x25F20 (SI) */
        {
            void (*fn)(u32) = (void (*)(u32))(void *)
                fn_resolve(DSD(DS_000A8628 + ch * 4u));
            if (fn) fn(s);                              /* 0x25F27 */
            /* PORT: as in 0x274FC, only the registered entrances run; for
             * the others the call is a named gap. */
        }
        s = DSB(DS_00104B12);                           /* 0x25F2E/0x25F30 */
        fight_hud_badge_spawn(s, (u32)(s32)(s8)DSB(DS_0010816A + s),
                              (u32)DSW(DS_000A76D0));   /* 0x25F35..0x25F47 0x1D838 */
        DSB(DS_000F0AFE) = 0u;                          /* 0x25F53 (BH) */
        DSB(DS_000F0AFF) = DSB(DS_0010810D);            /* 0x25F4E/0x25F59 */
        break;
    case 4u: {
        u16 t = (u16)(DSW(DS_00104AFE) - 1u);           /* 0x25F81/0x25F88 */
        DSW(DS_00104AFE) = t;                           /* 0x25F89 */
        if ((s16)t <= 0)                                /* 0x25F90/0x25F93 `jg` */
            DSB(DS_00104B25) = DSB(DS_00104B23);        /* 0x25F95/0x25F9A */
        break;
    }
    default:
        break;                                          /* 0x25C9B `ja 0x25F9F` */
    }
    DSB(DS_00104AEC) = (u8)(DSB(DS_00104AEC) | 2u);     /* 0x25DC6/0x25E5A/0x25F5E/0x25F74/0x25F9F */
}

/* ---- modes 0x30/0x31/0x33, mode 0x32's continue/rematch chain (record
 * §49-J) -------------------------------------------------------------- */

/* 0x29328 — record §49-J. Mode 0x30's handler (0x24C5C case 0x30, the
 * table's own named-gap listing until now; its only caller). A near-twin of
 * mode 5's 0x25C88 (record §48-U): the same DS_00104B25 sub-state machine
 * (the jump table at 0x29318, AL - 1 in 0..3), with state 1's round-card
 * spawn/win-marker reset and state 4's DS_00104AFE countdown into
 * DS_00104B23 byte-for-byte the same code (0x25C88's descriptor tables
 * DS_000A8898/A88A4/A88A8/A88B4 and its 0x25C1C/0x256F4 callees are reused
 * unchanged). It differs in two places: state 2's voice is unconditionally
 * 0x2C3FC(0xD7, dl=3) here (0x25C88's picks 0xD9 with DS_00104B14 set), and
 * state 3 only advances the mode word (to 0x31, with DS_00104AF0/AF1 reset
 * to 0) when DS_00104B21 == 0; the raw's own `jnz` for a nonzero
 * DS_00104B21 skips straight to the shared 0x295B1 tail with neither
 * DS_00104B25 nor DS_00104B00 touched, so that state's cleanup (the two
 * actor_set_dead calls, flow_round_timer_draw, the byte resets) repeats
 * every frame it is entered — ported as observed, not "fixed"; nothing in
 * mode 0x32's own transition (game_mode_32_step, which lands here only via
 * mode 0x31) is seen to leave DS_00104B21 nonzero on the first pass through
 * this state, so the quirk is believed unreachable on the oracle path.
 * EBX..EDI are pushed and popped. */
void game_mode_30_step(void)
{
    fight_slot_pass();                                  /* 0x2932D 0x3CB68 */
    switch (DSB(DS_00104B25)) {                         /* 0x29332..0x29346 */
    case 1u: {
        u32 desc;
        DSB(DS_00104B1B) = 1u;                          /* 0x2934E/0x29350 */
        DSB(DS_00104B15) = 1u;                          /* 0x29356 */
        flow_round_hud_init();                          /* 0x2935C 0x25C1C */
        if (DSB(DS_00104B14) != 0u) {                   /* 0x29361/0x29368 */
            desc = (DSB(DS_00104529) & 2u) != 0u
                 ? DSD(DS_000A88B4) : DSD(DS_000A88A4);  /* 0x2936A..0x293A4 */
        } else {
            u32 i, b = DSB(DS_00104B1E);                /* 0x293A6..0x293AE */
            if (b == DSD(DS_00104ADC)) i = 2u;           /* 0x293B3/0x293B5 */
            else if (b == 1u) i = 0u;                    /* 0x293BE/0x293C1 */
            else i = 1u;                                 /* 0x293C7 */
            desc = (DSB(DS_00104529) & 2u) != 0u
                 ? DSD(DS_000A88A8 + i * 4u)              /* 0x293CC..0x293F1 */
                 : DSD(DS_000A8898 + i * 4u);             /* 0x293F3..0x2940A */
        }
        DSD(DS_00104AC0) = actor_spawn((const u32 *)(mem + desc), 0x2A00u,
                                       0xFFu, 0x3600u, 0u);  /* 0x2940A 0x2AE14 */
        DSD(DS_00104A88) = 0u;                          /* 0x29414/0x29416 */
        DSD(DS_00104A88 + 4u) = 0u;                     /* 0x2941C */
        DSD(DS_00104A88 + 8u) = 0u;                     /* 0x29422 */
        DSD(DS_00104A88 + 0xCu) = 0u;                   /* 0x29428 */
        DSD(DS_00104A98) = 0u;                          /* 0x2942E */
        DSD(DS_00104A98 + 4u) = 0u;                     /* 0x29434 */
        DSD(DS_00104A98 + 8u) = 0u;                     /* 0x2943A */
        DSD(DS_00104A98 + 0xCu) = 0u;                   /* 0x29440 */
        flow_win_markers_spawn();                       /* 0x29446 0x256F4 */
        DSB(DS_00104B25) = 4u;                          /* 0x2944B/0x29452 */
        DSB(DS_00104B23) = 2u;                          /* 0x29457/0x29460 */
        DSW(DS_00104AFE) = 0x3Cu;                       /* 0x2944D/0x29459 */
        break;
    }
    case 2u: {
        u32 desc = ((DSB(DS_00104529) & 2u) != 0u && DSB(DS_00104B14) != 0u)
                 ? DS_000A8884 : DS_000BB6A0;            /* 0x29473..0x294AE */
        DSD(DS_00104ACC) = actor_spawn((const u32 *)(mem + desc), 0x2A00u,
                                       0xFFu, 0x1200u, 0u);  /* 0x294B3 0x2AE14 */
        DSB(DS_00104AE8) = (u8)(DSB(DS_00104AE8) | 0x40u);  /* 0x294BD..0x294C9 */
        /* PORT: 0x294CE 0x2C3FC(0xD7, dl=3) voice, not wired (record §45-A). */
        DSW(DS_00104AFE) = 0x3Cu;                       /* 0x294C4/0x294DC */
        DSB(DS_00104B23) = 3u;                          /* 0x294D3/0x294E3 */
        DSB(DS_00104B25) = 4u;                          /* 0x294DA/0x294E9 */
        break;
    }
    case 3u:
        text_cursor_hold(-1, 5, mem + DS_00080994, 0u); /* 0x294FC..0x2950D 0x2F4BC */
        actor_set_dead(DSD(DS_00104AC0));               /* 0x29512/0x29517 0x2B150 */
        DSD(DS_00104AC0) = 0u;                          /* 0x2951C/0x29523 */
        actor_set_dead(DSD(DS_00104ACC));               /* 0x2951E/0x29529 0x2B150 */
        DSD(DS_00104ACC) = 0u;                          /* 0x2952E */
        flow_round_timer_draw();                        /* 0x29534 0x4F37C */
        DSB(DS_001078FC) = 0u;                          /* 0x29539/0x2953B */
        DSB(DS_001078FE) = 0u;                          /* 0x29541 */
        DSD(DS_00104ABC) = (DSB(DS_00104B1F) == 3u ? 1u : 0u) + 1u;  /* 0x29547..0x2955A */
        /* PORT: 0x2955F 0x32970, the run clock, is out of scope (spec §7);
         * the host clock owns wall time. */
        if (DSB(DS_00104B21) == 0u) {                   /* 0x29564..0x2956C */
            DSB(DS_00104AF0) = 0u;                      /* 0x29573 */
            DSB(DS_00104AF1) = 0u;                      /* 0x29579 */
            DSW(DS_00104B00) = 0x31u;                   /* 0x2957F */
            DSB(DS_00104AEC) = (u8)(DSB(DS_00104AEC) | 2u);
            return;
        }
        /* PORT/quirk: else the raw falls straight to the shared 0x295B1
         * tail without touching DS_00104B25 or the mode word — see the
         * header note above. */
        break;
    case 4u: {
        u16 t = (u16)(DSW(DS_00104AFE) - 1u);           /* 0x29593/0x2959A */
        DSW(DS_00104AFE) = t;                            /* 0x2959B */
        if ((s16)t <= 0)                                 /* 0x295A2/0x295A5 */
            DSB(DS_00104B25) = DSB(DS_00104B23);         /* 0x295A7/0x295AC */
        break;
    }
    default:
        break;                                           /* 0x29339 `ja` */
    }
    DSB(DS_00104AEC) = (u8)(DSB(DS_00104AEC) | 2u);     /* 0x295B1 */
}

/* 0x29970 — record §49-J. The round-over threshold check: for each side
 * with its +0x5A score byte (DS_0010780A/DS_001078BE, zero-extended; 0x78 =
 * 120) at or above 0x78, mode 0x32 and DS_00104B09 = the side, then
 * flow_round_winner. EBX/EDX are pushed and popped. Callers: the unported
 * 0x299A6/0x299DF (0x27C48, flow_round_winner, already documents these as
 * its own callers) — flow_round_over_check is 0x299E8's (mode 0x31,
 * game_mode_31_step) only caller. */
void flow_round_over_check(void)
{
    if (DSB(DS_0010780A) >= 0x78u) {                    /* 0x29974..0x2997C */
        /* PORT: 0x2997E 0x2C3FC(0x27) and 0x29988 0x2C3FC(0x22) voices, not
         * wired (record §45-A). */
        DSW(DS_00104B00) = 0x32u;                       /* 0x29999 */
        DSB(DS_00104B09) = 0u;                          /* 0x299A0 */
        flow_round_winner();                            /* 0x299A6 0x27C48 */
    }
    if (DSB(DS_001078BE) >= 0x78u) {                    /* 0x299AD..0x299B5 */
        /* PORT: 0x299B7 0x2C3FC(0x27) and 0x299C6 0x2C3FC(0x22) voices, not
         * wired (record §45-A). */
        DSW(DS_00104B00) = 0x32u;                       /* 0x299D2 */
        DSB(DS_00104B09) = 1u;                          /* 0x299D9 */
        flow_round_winner();                            /* 0x299DF 0x27C48 */
    }
}

/* 0x299E8 — record §49-J. Mode 0x31's handler (0x24C5C case 0x31, the
 * table's own named-gap listing until now; its only caller). A near-twin of
 * mode 0xC's 0x27380 (game_mode_0c_step, record §48-C): the identical
 * prelude (fight_slot_clear, camera_screen_base per side, the two latches),
 * the identical DS_001078FA == 2 gated projection block (the same six
 * 0x17FA0/0x17580/0x1958C/0x19068/0x1975C calls at the same literal
 * addresses), and the identical tail (fight_slot_pass, fight_hud_pass per
 * side, camera_y_commit, fight_hud_pulse). Two differences: the prelude
 * opens with the DS_00104B12 slot's frozen-pose undo — the same 0x1E1
 * blink-restore idiom 0x27380 itself runs on that same slot (record §48-C),
 * here unconditional at entry rather than gated by the mode-0xC tail — and
 * the tail ends with flow_round_over_check (0x29970) in place of
 * flow_arena_ko_check (0x272DC). EBX/ECX/EDX are pushed and popped. */
void game_mode_31_step(void)
{
    u32 slot = DS_001077B0 + (u32)DSB(DS_00104B12) * 0x94u;  /* 0x299ED..0x29A01 */
    if ((DSB(slot + 0x41u) & 1u) != 0u                  /* 0x29A04/0x29A0B */
            && (DSB(slot + 0x42u) & 8u) == 0u) {        /* 0x29A0D/0x29A14 */
        u32 ps = DSD(DS_001014EC)
                 + (u32)DSW(DSD(slot) + 0x56u) * 0x20u;  /* 0x29A16..0x29A2E */
        if ((DSW(ps) & 0x7FFFu) == 0x1E1u)               /* 0x29A30..0x29A42 */
            DSW(ps) = DSW(DS_00104AF6);                  /* 0x29A44/0x29A4B */
    }
    fight_slot_clear();                                 /* 0x29A4E 0x3C5CC */
    camera_screen_base(0, (s32)DSB(DS_0010782A));        /* 0x29A53..0x29A5D 0x16D58 */
    camera_screen_base(1, (s32)DSB(DS_001078BE));        /* 0x29A62..0x29A6F 0x16D58 */
    DSD(DS_001077E8) = DSD(DS_001077E4);                /* 0x29A74/0x29A79 */
    DSD(DS_0010787C) = DSD(DS_00107878);                /* 0x29A7E/0x29A83 */
    if (DSB(DS_001078FA) == 2u) {                        /* 0x29A88..0x29A92 */
        camera_project(0, DS_00100B08, DS_00100B00, DS_00100B62,
                       DS_00100B60, DS_00100AF0);        /* 0x29AB3 0x17FA0 */
        camera_project(1, DS_00100B0C, DS_00100B04, DS_00100B63,
                       DS_00100B61, DS_00100AF4);        /* 0x29AD6 0x17FA0 */
        camera_decay();                                  /* 0x29ADB 0x17580 */
        fighter_pass_a();                                /* 0x29AE0 0x1958C */
        fighter_pass_b(0u);                              /* 0x29AE7 0x19068 */
        camera_project(0, DS_00100B08, DS_00100B00, DS_00100B62,
                       DS_00100B60, DS_00100AF0);        /* 0x29B07 0x17FA0 */
        camera_project(1, DS_00100B0C, DS_00100B04, DS_00100B63,
                       DS_00100B61, DS_00100AF4);        /* 0x29B2A 0x17FA0 */
        fighter_think();                                 /* 0x29B2F 0x1975C */
    }
    fight_slot_pass();                                   /* 0x29B34 0x3CB68 */
    fight_hud_pass(0u);                                   /* 0x29B3B 0x35658 */
    fight_hud_pass(1u);                                    /* 0x29B45 0x35658 */
    camera_y_commit();                                      /* 0x29B4A 0x12DA8 */
    fight_hud_pulse();                                       /* 0x29B4F 0x1DA08 */
    flow_round_over_check();                                  /* 0x29B54 0x29970 */
    DSB(DS_00104AEC) = (u8)(DSB(DS_00104AEC) | 2u);            /* 0x29B59 */
}

/* 0x29638 — record §49-J. Mode 0x33's handler (0x24C5C case 0x33, the
 * table's own named-gap listing until now; its only caller). The match's
 * end wait: the two latches, fight_hud_pass per side, the crowd/ambience
 * walker 0x4DEF4 (named gap below), camera_y_commit, then the DS_00104AEC
 * bit-1 mark and the DS_00104AFE countdown (armed to 0x258 by mode 0x32's
 * own final-win arm, game_mode_32_step). On expiry: the voice 0x2B,
 * DS_00104B25 = 0 (mode 0x30's own sub-state, parked for mode 0x30's next
 * entry), mode 0x17 (frontend_mode_17_step) and the DS_00104AE4 hook
 * FN_00025AE8 (game_hook_25ae8's own header already names this call site).
 * ECX/EDX are pushed and popped. */
void game_mode_33_step(void)
{
    DSD(DS_001077E8) = DSD(DS_001077E4);                /* 0x2963A/0x2963F */
    DSD(DS_0010787C) = DSD(DS_00107878);                /* 0x29644/0x29649 */
    fight_hud_pass(0u);                                 /* 0x2964E 0x35658 */
    fight_hud_pass(1u);                                 /* 0x29655 0x35658 */
    /* PORT: 0x2965F 0x4DEF4, a 552-byte crowd/ambience state walker over the
     * DS_0010884C actor list (voices 0xCB/0xDC, volume ramps through
     * 0x2BC30), is a named gap: unrelated to this mode's own transition
     * logic and out of scope for this task (also called, still unported,
     * from mode 0xF's 0x277C0). */
    camera_y_commit();                                  /* 0x29664 0x12DA8 */
    DSB(DS_00104AEC) = (u8)(DSB(DS_00104AEC) | 2u);     /* 0x29676/0x2967A */
    {
        u16 t = (u16)(DSW(DS_00104AFE) - 1u);           /* 0x2966F/0x29679 */
        DSW(DS_00104AFE) = t;                            /* 0x29680 */
        if ((s16)t <= 0) {                               /* 0x29687/0x2968A */
            /* PORT: 0x2968C 0x2C3FC(0x2B) voice, not wired (record §45-A). */
            DSB(DS_00104B25) = 0u;                      /* 0x2969D */
            DSW(DS_00104B00) = 0x17u;                   /* 0x296A8 */
            DSD(DS_00104AE4) = FN_00025AE8;             /* 0x296AF */
        }
    }
}

/* ---- 0x26254, the fight frame, and its round end 0x27FA8 (record §48-K) -- */

#define DS_00104B1C 0x00104B1Cu   /* no symbols.h name: 0x27C48's bonus byte */
#define DS_001088EF 0x001088EFu   /* no symbols.h name: the dword whose top byte is DS_001088F2 */
#define DS_001077F2 0x001077F2u   /* no symbols.h name: slot 0's +0x42 */
#define DS_000A88B8 0x000A88B8u   /* no symbols.h name: 0x25FDC's descriptor */
#define DS_000A88E0 0x000A88E0u   /* no symbols.h name: 0x25FDC's descriptor (bit 1) */
#define DS_000A88CC 0x000A88CCu   /* no symbols.h name: 0x2604C's descriptor */
#define DS_000A88F4 0x000A88F4u   /* no symbols.h name: 0x2604C's descriptor (bit 1) */
#define DS_000BB6C8 0x000BB6C8u   /* no symbols.h name: 0x27ED8's descriptor */

#define DS_001081EC 0x001081ECu   /* no symbols.h name: the word 0x45B50's state 1 arms */

/* 0x4F4E8 — record §49-Z. The render table's bit-0 entry (DS_000A86C4[0],
 * the dword at 0xA86C4; 0x255CC's per-bit walk calls it while DS_00104AEC
 * bit 0 is set, which 0x4F37C sets and 0x27E6D clears), fn() with EAX unread;
 * EBX/ECX/EDX/ESI/EDI are pushed and popped. With DS_00105B3B non-zero it
 * draws each side's slot +0x3C dword as a width-6 number at row 7 (column 1,
 * then 0x23; mode 0x2000, pad 0) through 0x2F4D0. With it zero and the signed
 * DS_001088D0 at most 0x62 it runs on the frames whose tick word
 * DS_00104AF4 is a multiple of DS_001088D0 (the unsigned word, a signed
 * `idiv`): with the countdown byte DS_001088F2 (the top byte of the dword
 * DS_001088EF) non-zero, the byte is read signed, a value at most 10 plays the
 * voice 0x52 and takes mode 0x3000 (else 0x4000), the byte is decremented and
 * the new value is drawn at column 0x13, row 1, width 2, pad 0 through 0x2F528.
 * PORT: 0x4F577 0x2C3FC(0x52) voice, not wired (record §45-A). PORT: a zero
 * DS_001088D0 would fault the original's `idiv` (0x4F55A); the port returns
 * (the init writes (v & 0xF) * 5 + 0x1E, so it is never zero). */
void flow_round_timer_step(void)
{
    if (DSB(DS_00105B3B) != 0u) {                       /* 0x4F4ED */
        for (u32 i = 0; i < 2u; i++)                    /* 0x4F4FB..0x4F52A */
            text_number_draw((s32)(1u + i * 0x22u), 7,
                             (s32)DSD(DS_001077B0 + i * 0x94u + 0x3Cu), 6, 0u,
                             0x2000u);                  /* 0x4F510..0x4F51C 0x2F4D0 */
    }
    if (DSB(DS_00105B3B) != 0u) return;                 /* 0x4F52C/0x4F533 */
    if ((s32)DSD(DS_001088D0) > 0x62) return;           /* 0x4F539/0x4F540 */
    if (DSD(DS_001088D0) == 0u) return;
    if (((s32)DSW(DS_00104AF4) % (s32)DSD(DS_001088D0)) != 0)
        return;                                         /* 0x4F548..0x4F55E */
    if (DSB(DS_001088F2) == 0u) return;                 /* 0x4F560 */
    u32 mode;
    if (((s32)DSD(DS_001088EF) >> 24) <= 0xA)           /* 0x4F569..0x4F575 */
        mode = 0x3000u;                                 /* 0x4F57C */
    else
        mode = 0x4000u;                                 /* 0x4F588 */
    DSB(DS_001088F2) = (u8)(DSB(DS_001088F2) - 1u);     /* 0x4F59E..0x4F5A6 */
    text_number_draw_font2(0x13, 1, (s32)DSD(DS_001088EF) >> 24, 2, 0u,
                           mode);                       /* 0x4F5AE..0x4F5BC 0x2F528 */
}

/* 0x25FDC — record §48-K. EAX = the side (kept in ESI). With the
 * DS_00104529 bit 1 the actor 0xA88E0 with EDX = 0x2A00, ECX = 0xFF, EBX =
 * 0xE00, else 0xA88B8 with EBX = 0x1200 (actor_spawn's a2 = EDX, a3 = ECX,
 * a4 = EBX; a5 = the pushed 0) into DS_00104AB4; DS_00104B0D = 0 (DL), DS_00104AE9 |= 0x80, then 0x41310(side,
 * 0x4E20). EBX/ECX/EDX/ESI are pushed and popped. Callers: 0x27CC7 and
 * 0x27D67 (0x27C48), each with the winning side whose +0x5A is 0. */
void flow_round_bonus_a(u32 side)
{
    if ((DSB(DS_00104529) & 2u) != 0u)                  /* 0x25FE2/0x25FE9 */
        DSD(DS_00104AB4) = actor_spawn((const u32 *)(mem + DS_000A88E0),
                                       0x2A00u, 0xFFu, 0xE00u, 0u);   /* 0x25FEB..0x26019 0x2AE14 */
    else
        DSD(DS_00104AB4) = actor_spawn((const u32 *)(mem + DS_000A88B8),
                                       0x2A00u, 0xFFu, 0x1200u, 0u);  /* 0x26003..0x26019 0x2AE14 */
    DSB(DS_00104B0D) = 0u;                              /* 0x26023/0x26025 */
    /* PORT: 0x25FDC's caller sets DS_00104AE9 bit 0x80, switching on entry
     * 15 of the per-frame update table DS_000A8644 (0xA8680, dispatched
     * every frame at 0x24CEF, xref-confirmed, not a bare code pointer);
     * that entry, 0x260BC, is a named gap, so the bonus card it would
     * animate/kill each frame is skipped. Named gap, not a regression: no
     * ported path set this bit before, and no oracle path ends a round. */
    DSB(DS_00104AE9) = (u8)(DSB(DS_00104AE9) | 0x80u);  /* 0x2602B..0x26036 */
    fighter_41310(side, 0x4E20);                        /* 0x26034..0x26041 0x41310 */
}

/* 0x2604C — record §48-K. 0x25FDC's twin: 0xA88F4 (bit 1) or 0xA88CC at the
 * same places into DS_00104AB0, DS_00104B0F = 0, DS_00104AEA |= 1, then
 * 0x41310(side, 0x4E20). EBX/ECX/EDX/ESI are pushed and popped. Callers:
 * 0x27CFD and 0x27DA0 (0x27C48), and 0x2613E in the unported 0x260BC, on
 * the signed byte DS_00104B1C. */
void flow_round_bonus_b(u32 side)
{
    if ((DSB(DS_00104529) & 2u) != 0u)                  /* 0x26052/0x26059 */
        DSD(DS_00104AB0) = actor_spawn((const u32 *)(mem + DS_000A88F4),
                                       0x2A00u, 0xFFu, 0xE00u, 0u);   /* 0x2605B..0x26089 0x2AE14 */
    else
        DSD(DS_00104AB0) = actor_spawn((const u32 *)(mem + DS_000A88CC),
                                       0x2A00u, 0xFFu, 0x1200u, 0u);  /* 0x26073..0x26089 0x2AE14 */
    DSB(DS_00104B0F) = 0u;                              /* 0x26093/0x26095 */
    /* PORT: 0x2604C's caller sets DS_00104AEA bit 0, switching on entry 16
     * of the same update table DS_000A8644 — 0x26194, the unported twin of
     * 0x260BC, not documented anywhere else. Same skip, same inertness. */
    DSB(DS_00104AEA) = (u8)(DSB(DS_00104AEA) | 1u);     /* 0x2609B..0x260A6 */
    fighter_41310(side, 0x4E20);                        /* 0x260A4..0x260B1 0x41310 */
}

/* 0x27BA4 — record §48-K. The match result into the dword DS_00104AD4. With
 * the byte DS_00104B1E equal to the dword DS_00104ADC: 2 on equal round wins
 * DS_00104AF2/DS_00104AF3, else 1 when AF2 < AF3 (0x27BCF `setbe` after
 * the `jnz`), else 0. Otherwise, with B1E > 1 and B1E <= ADC (both signed
 * dword compares) and d = ADC - B1E: 0 when AF2 > AF3 + d, 1 when AF3 > AF2
 * + d; every other case -1. For a result 0 or 1 the word DS_00108860[r]
 * (zero-extended, below 0xC8) gains 4. EBX/ECX/EDX are pushed and popped.
 * Callers: the four 0x27C48 exits (0x27D07, 0x27D85, 0x27DAA, 0x27DBF). */
void flow_match_result_set(void)
{
    u32 adc = DSD(DS_00104ADC);                         /* 0x27BA9 */
    u32 b1e = DSB(DS_00104B1E);                         /* 0x27BAF */
    u32 w2 = DSB(DS_00104AF2), w3 = DSB(DS_00104AF3);
    s32 r;
    if (b1e == adc) {                                   /* 0x27BB5/0x27BB7 */
        if (w2 == w3) r = 2;                            /* 0x27BC4..0x27BC8 */
        else r = w2 < w3 ? 1 : 0;                       /* 0x27BCF/0x27BD2 */
    } else if ((s32)b1e <= 1 || (s32)b1e > (s32)adc) {  /* 0x27BDA..0x27BE1 */
        r = -1;                                         /* 0x27C0C */
    } else {
        u32 d = adc - b1e;                              /* 0x27BE3 */
        if ((s32)w2 > (s32)(w3 + d)) r = 0;             /* 0x27BF4..0x27BFB */
        else if ((s32)w3 > (s32)(w2 + d)) r = 1;        /* 0x27BFF..0x27C05 */
        else r = -1;                                    /* 0x27C0C */
    }
    if (r != -1 && r != 2) {                            /* 0x27C11..0x27C19 */
        u32 w = DSW(DS_00108860 + (u32)r * 2u);         /* 0x27C1B..0x27C24 */
        if ((s32)w < 0xC8)                              /* 0x27C2B/0x27C31 */
            DSW(DS_00108860 + (u32)r * 2u) = (u16)(w + 4u);   /* 0x27C33/0x27C36 */
    }
    DSD(DS_00104AD4) = (u32)r;                          /* 0x27C3D */
}

/* 0x27C48 — record §48-K. The round's winner on the two +0x5A bytes, A
 * (DS_0010780A) and B (DS_0010789E), compared unsigned. First DS_00104B13 =
 * 1 (AH) and DS_00104B1C = 0xFF (DL); AL = 0x78 - A.
 * - A < B: side 0 wins. With slot 0's +0x41 bit 3, slot 0's +0x5B = AL.
 *   DS_00104B16 = 0, DS_00104AF2 and slot 0's +0x7F incremented;
 * - B < A: side 1 wins. With slot 1's +0x41 bit 3, slot 1's +0x5B = AL,
 *   the value computed from A (0x27D21 stores the 0x27C69 AL, never
 *   recomputed from B). DS_00104B16 = 1, DS_00104AF3 and slot 1's +0x7F
 *   incremented;
 * - A == B: DS_00104B16 = 2 only.
 * For a winner, unless DS_00104B1D is 3 or 2: with the winner's +0x5A 0
 * (re-read from memory), 0x25FDC(winner), then with the signed byte
 * DS_001088F2 (0x27CCC `mov eax,[0x1088ef]; sar eax,0x18`) >= 0x32,
 * DS_00104B1C = the winner; with it non-zero and DS_001088F2 >= 0x32,
 * 0x2604C(winner). Every path ends 0x256F4, 0x27BA4. EBX/ECX/EDX are pushed
 * and popped. Callers: 0x27FC8, 0x280C4 and 0x280DC (0x27FA8), and
 * 0x299A6/0x299DF (0x29970, flow_round_over_check). */
void flow_round_winner(void)
{
    u8 a, b, al;
    u32 w;
    DSB(DS_00104B13) = 1u;                              /* 0x27C4B/0x27C4F */
    DSB(DS_00104B1C) = 0xFFu;                           /* 0x27C4D/0x27C55 */
    a = DSB(DS_0010780A);                               /* 0x27C5D */
    b = DSB(DS_0010789E);                               /* 0x27C63 */
    al = (u8)(0x78u - a);                               /* 0x27C5B/0x27C69 */
    if (a < b) {                                        /* 0x27C6B `jnc` */
        if ((DSB(DS_001077F1) & 8u) != 0u)              /* 0x27C73 */
            DSB(DS_0010780B) = al;                      /* 0x27C7C */
        DSB(DS_00104B16) = 0u;                          /* 0x27C89 */
        DSB(DS_00104AF2) = (u8)(DSB(DS_00104AF2) + 1u); /* 0x27C83..0x27C96 */
        DSB(DS_0010782F) = (u8)(DSB(DS_0010782F) + 1u); /* 0x27C90..0x27CA4 */
        w = 0u;
    } else if (b < a) {                                 /* 0x27D10 `jnc` */
        if ((DSB(DS_00107885) & 8u) != 0u)              /* 0x27D18 */
            DSB(DS_0010789F) = al;                      /* 0x27D21 */
        DSB(DS_00104B16) = 1u;                          /* 0x27D26/0x27D28 */
        DSB(DS_001078C3) = (u8)(DSB(DS_001078C3) + 1u); /* 0x27D2D..0x27D49 */
        DSB(DS_00104AF3) = (u8)(DSB(DS_00104AF3) + 1u); /* 0x27D33..0x27D3D */
        w = 1u;
    } else {
        DSB(DS_00104B16) = 2u;                          /* 0x27DB3 */
        w = 2u;
    }
    if (w != 2u && DSB(DS_00104B1D) != 3u && DSB(DS_00104B1D) != 2u) {  /* 0x27CAA..0x27CB6, 0x27D4F..0x27D57 */
        if (DSB(DS_0010780A + w * 0x94u) == 0u) {       /* 0x27CBC, 0x27D59 */
            flow_round_bonus_a(w);                      /* 0x27CC7, 0x27D67 0x25FDC */
            if ((s32)DSD(DS_001088EF) >> 24 >= 0x32)    /* 0x27CCC..0x27CD7, 0x27D6C..0x27D77 */
                DSB(DS_00104B1C) = (u8)w;               /* 0x27CDF (CL = 0), 0x27D79 */
        } else if ((s32)DSD(DS_001088EF) >> 24 >= 0x32) {   /* 0x27CEA..0x27CF5, 0x27D8E..0x27D99 */
            flow_round_bonus_b(w);                      /* 0x27CFD, 0x27DA0 0x2604C */
        }
    }
    flow_win_markers_spawn();                           /* 0x27D02/0x27D80/0x27DA5/0x27DBA 0x256F4 */
    flow_match_result_set();                            /* 0x27D07/0x27D85/0x27DAA/0x27DBF 0x27BA4 */
}

/* 0x27DC8 — record §48-K. The match's end. 0x32970(0); DS_00104ABC =
 * (DS_00104B1F == 3) + 1; 0x32B4C(that, EDX = 0); then 0x32BB0 on the
 * characters (EAX = slot 0's, EDX = slot 1's, or with DS_00104ABC == 1 the
 * side not flagged by slot 0's +0x63 and EDX = -1); 0x2B150 on DS_00104AC8
 * when non-zero (the dword is kept); 0x4F728(DS_00104AFC, 0x1D); 0x2C2B0
 * for both sides at row 0x1D (EDX = 0x1D survives 0x4F728, which pushes and
 * pops it, and is reloaded for side 1); DS_00104AE8 &= 0xFB, DS_00104AEC &=
 * 0xFE; 0x4651C; and unless DS_00104B16 == 2, 0x46534(DS_00104B16, -1) and
 * 0x46534(DS_00104B16 ^ 1, 1). EBX/EDX are pushed and popped. Callers:
 * 0x2800D and 0x280E3 (0x27FA8). */
void flow_match_end(void)
{
    u32 b;
    /* PORT: 0x27DCC 0x32970(EAX = 0), the run clock, is out of scope (spec
     * §7); the host clock owns wall time. */
    DSD(DS_00104ABC) = (DSB(DS_00104B1F) == 3u ? 1u : 0u) + 1u;   /* 0x27DD1..0x27DE6 */
    config_play_time_snap_b(DSD(DS_00104ABC), 0u);      /* 0x27DE4/0x27DEB 0x32B4C */
    /* PORT: 0x27DF0..0x27E28 0x32BB0(c0, c1) (or (c, -1) with DS_00104ABC
     * == 1): two 0x2DAE4(0x1B + c, 1) audit adds for the characters below 7,
     * deferred (spec §7) as in 0x2D962 and 0x32A3C (config.c). */
    if (DSD(DS_00104AC8) != 0u)                         /* 0x27E2D/0x27E35 */
        actor_set_dead(DSD(DS_00104AC8));               /* 0x27E39 0x2B150 */
    /* PORT: 0x27E4B 0x4F728 (EAX = DS_00104AFC, not read) is two voices, not
     * wired (record §45-A): 0xDF when DS_00104AD4 is neither -1 nor 3, the
     * winner's +0x63 is 0 and the signed byte DS_001088F2 > 0, else 0x23;
     * then 0x22. It reads only. */
    prompt_side_erase(0, 0x1D);                         /* 0x27E50/0x27E52 0x2C2B0 */
    prompt_side_erase(1, 0x1D);                         /* 0x27E57..0x27E61 0x2C2B0 */
    DSB(DS_00104AE8) = (u8)(DSB(DS_00104AE8) & 0xFBu);  /* 0x27E66 */
    DSB(DS_00104AEC) = (u8)(DSB(DS_00104AEC) & 0xFEu);  /* 0x27E6D */
    flow_1082c8_restore();                              /* 0x27E74 0x4651C */
    b = DSB(DS_00104B16);                               /* 0x27E79/0x27E7B */
    if (b != 2u) {                                      /* 0x27E80/0x27E83 */
        fighter_46534(b, -1);                           /* 0x27E85/0x27E8A 0x46534 */
        fighter_46534((DSB(DS_00104B16) ^ 1u) & 0xFFu, 1);   /* 0x27E8F..0x27EA0 0x46534 */
    }
}

/* 0x27ED8 — record §48-K. The round is over: for each side the slot's +0x40
 * dword &= 0xFFEFEFFF and 0x1D764(side); 0x2DAE4(0xF, 1); the actor 0xBB6C8
 * with EDX = 0x2A00, ECX = 0xFF, EBX = 0xF00 (a2, a3, a4; EBX/ECX set before
 * 0x2DAE4, which pushes and pops them; a5 = the pushed 0) into DS_00104AC8; 0x19820; DS_001088F2 = 0x1E
 * (AH). Then m = the larger +0x5A byte (unsigned, 0x27F52 `ja`); for m below
 * 0x78, q = (s8)DS_001088F2 * DS_001088D0 / (0x78 - m) (signed `imul`/`idiv`;
 * DS_001088F2 is the 0x1E just stored), and q = 1 for m >= 0x78 or q == 0.
 * The +0x5A bytes are read at 0x27F4D/0x27F41, after 0x1D764 zeroed both, so
 * m is 0 and q = 0x1E * DS_001088D0 / 0x78 on every path.
 * Last the mode word = 0xA and DS_00104AA8 = q (the 0x27EDC load of
 * DS_00104AA8 into ESI is overwritten on every path). EBX/ECX/EDX/ESI are
 * pushed and popped. Callers: 0x28003, 0x280BA and 0x280D2 (0x27FA8). */
void flow_round_over(void)
{
    u32 side, m, q;
    for (side = 0; side < 2u; side++) {                 /* 0x27EE6..0x27F09 */
        u32 slot = DS_001077B0 + side * 0x94u;
        DSD(slot + 0x40u) &= 0xFFEFEFFFu;               /* 0x27EE8..0x27EFB */
        fight_hud_side_reset(side);                     /* 0x27F01 0x1D764 */
    }
    /* PORT: 0x27F1F 0x2DAE4(0xF, 1), the audit add, is deferred (spec §7),
     * as in 0x2D962 and 0x32A3C (config.c). */
    DSD(DS_00104AC8) = actor_spawn((const u32 *)(mem + DS_000BB6C8),
                                   0x2A00u, 0xFFu, 0xF00u, 0u);   /* 0x27F24..0x27F35 0x2AE14 */
    fighter_19820();                                    /* 0x27F3A 0x19820 */
    DSB(DS_001088F2) = 0x1Eu;                           /* 0x27F3F/0x27F47 */
    m = DSB(DS_0010780A);                               /* 0x27F4D */
    if (!(m > (u32)DSB(DS_0010789E))) m = DSB(DS_0010789E);   /* 0x27F52..0x27F56 */
    q = 1u;                                             /* 0x27F8D */
    if (m < 0x78u) {                                    /* 0x27F5C..0x27F63 */
        s32 p = (s32)((u32)((s32)DSD(DS_001088EF) >> 24) * DSD(DS_001088D0));  /* 0x27F65..0x27F74 */
        s32 v = p / (s32)(0x78u - m);                   /* 0x27F77..0x27F85 */
        if (v != 0) q = (u32)v;                         /* 0x27F87..0x27F8B */
    }
    DSW(DS_00104B00) = 0x0Au;                           /* 0x27F92 */
    DSD(DS_00104AA8) = q;                               /* 0x27F9B */
}

/* 0x27FA8 — record §48-K. The round-end check the fight frame runs each
 * frame, on A = DS_0010780A and B = DS_0010789E (the +0x5A bytes):
 * - either at 0x78 or above: 0x27C48. Then, outside mode 0xB, with the
 *   byte DS_00104B1E equal to the dword DS_00104ADC and DS_00104AD4 == 2: the
 *   0xD3 voice, 0x39FF4 and 0x27ED8. Otherwise 0x27DC8, the word
 *   DS_00104AF8 = 0x258 (BX) and mode 9 for DS_00104AD4 in 0..2 (signed), else
 *   mode 8;
 * - both below: nothing while the signed byte DS_001088F2 is >= 1. At 0 or
 *   below the 0xD3 voice and 0x39FF4; then, with B1E == ADC and outside mode
 *   0xB, 0x27ED8 when DS_00104AF3 == DS_00104AF2 and |A - B| <= 3, else
 *   0x27C48 and 0x27ED8 when DS_00104AD4 == 2. The rest (B1E != ADC or mode
 *   0xB go through 0x27C48 at 0x280DC; AD4 != 2 falls through from 0x280C9):
 *   0x27DC8; unless DS_00104B16 == 2 the winner slot's +0x42 |= 0x80; the
 *   bytes DS_001078FC = DS_001078FE = 1 (BL), DS_000F0AFE = 2 (CL) and mode
 *   7 (DX). The mode word is compared as a zero-extended word
 *   (0x27FCF/0x2807C). EBX/ECX/EDX/EDI are pushed and popped. Callers:
 *   0x263AF (0x26254) and the unported 0x2669B (0x26540). */
void flow_round_end_check(void)
{
    u32 a = DSB(DS_0010780A), b = DSB(DS_0010789E);    /* 0x27FAE, 0x27FBA */
    if (a >= 0x78u || b >= 0x78u) {                     /* 0x27FB3/0x27FB6, 0x27FBF/0x27FC2 */
        s32 r;
        flow_round_winner();                            /* 0x27FC8 0x27C48 */
        if (DSW(DS_00104B00) != 0x0Bu                   /* 0x27FCF..0x27FD8 */
                && (u32)DSB(DS_00104B1E) == DSD(DS_00104ADC)   /* 0x27FDA..0x27FE9 */
                && DSD(DS_00104AD4) == 2u) {            /* 0x27FEB/0x27FF2 */
            /* PORT: 0x27FF9 0x2C3FC(0xD3) voice, not wired (record §45-A). */
            fighter_39ff4();                            /* 0x27FFE 0x39FF4 */
            flow_round_over();                          /* 0x28003 0x27ED8 */
            return;
        }
        flow_match_end();                               /* 0x2800D 0x27DC8 */
        DSW(DS_00104AF8) = 0x258u;                      /* 0x28012/0x2801D */
        r = (s32)DSD(DS_00104AD4);                      /* 0x28017 */
        DSW(DS_00104B00) = (r >= 0 && r <= 2) ? 0x09u : 0x08u;  /* 0x28024..0x2803B */
        return;
    }
    if ((s32)DSD(DS_001088EF) >> 24 >= 1) return;       /* 0x28049..0x28054 */
    /* PORT: 0x2805F 0x2C3FC(0xD3) voice, not wired (record §45-A). */
    fighter_39ff4();                                    /* 0x28064 0x39FF4 */
    if ((u32)DSB(DS_00104B1E) == DSD(DS_00104ADC)       /* 0x28069..0x28078 */
            && DSW(DS_00104B00) != 0x0Bu) {             /* 0x2807A..0x28085 */
        u32 d = a >= b ? a - b : b - a;                 /* 0x28087..0x2809C */
        if (DSB(DS_00104AF3) == DSB(DS_00104AF2) && (d & 0xFFu) <= 3u) {  /* 0x2809E..0x280B8 */
            flow_round_over();                          /* 0x280BA 0x27ED8 */
            return;
        }
        flow_round_winner();                            /* 0x280C4 0x27C48 */
        if (DSD(DS_00104AD4) == 2u) {                   /* 0x280C9/0x280D0 */
            flow_round_over();                          /* 0x280D2 0x27ED8 */
            return;
        }
    } else {
        flow_round_winner();                            /* 0x280DC 0x27C48 */
    }
    flow_match_end();                                   /* 0x280E3 0x27DC8 */
    {
        u32 w = DSB(DS_00104B16);                       /* 0x280E8 */
        if (w != 2u)                                    /* 0x280EE/0x280F1 */
            DSB(DS_001077F2 + w * 0x94u) = (u8)(DSB(DS_001077F2 + w * 0x94u) | 0x80u);  /* 0x280F3..0x28101 */
    }
    DSB(DS_001078FC) = 1u;                              /* 0x28109/0x28112 */
    DSB(DS_001078FE) = 1u;                              /* 0x28118 */
    DSB(DS_000F0AFE) = 2u;                              /* 0x2810B/0x2811E */
    DSW(DS_00104B00) = 0x07u;                           /* 0x2810D/0x28124 */
}

/* 0x28C38 — record §48-B. Mode `0xB`'s winner-pose tick (0x24C5C case 0xB:
 * `0x25287 call 0x26254`, `0x2528C call 0x28c38`, `0x25291 jmp 0x2540F`;
 * its only caller). EBX/EDX are pushed and popped. The running frame count
 * DS_00104AD8 (a signed dword, reset to 0 at 0x25C1C, record §48-U) is
 * incremented, then taken modulo DS_00104AA8 (`sar edx,0x1f; idiv ebx`, the
 * duration `flow_round_over` loads into it, record §48-K); a non-zero
 * signed remainder returns at once. Otherwise, per side, while that side's
 * +0x5A byte (DS_0010780A/DS_0010789E) is below 0x77, `0x392A0`(slot, 1, 0)
 * nudges its win pose (`fighter_392a0`, already ported). */
void flow_winner_pose_step(void)
{
    u32 count = DSD(DS_00104AD8) + 1u;                  /* 0x28C3A/0x28C40 */
    DSD(DS_00104AD8) = count;                           /* 0x28C47 */
    if ((s32)count % (s32)DSD(DS_00104AA8) != 0) return;   /* 0x28C41..0x28C56 */
    if (DSB(DS_0010780A) < 0x77u)                       /* 0x28C58..0x28C61 */
        fighter_392a0(DS_001077B0, 1, 0);               /* 0x28C63..0x28C6F */
    if (DSB(DS_0010789E) < 0x77u)                       /* 0x28C76..0x28C7F */
        fighter_392a0(DS_001077B0 + 0x94u, 1, 0);       /* 0x28C81..0x28C8D */
}

/* 0x26254 — record §48-K. The fight frame: mode 4's handler (0x24C5C case
 * 4, the table entry 0x25242 `call 0x26254; jmp 0x2540F`, which case 6
 * reaches too, at 0x2525D/0x25266) and the first call of case 0xB's
 * 0x25287. 0x3C5CC; 0x16D58 per side (EAX = side, EDX = the slot's
 * zero-extended +0x7A); the two position latches (+0x34 -> +0x38). Then,
 * only with DS_001078FA == 2 (0x2629D; else `jnz 0x26385`), the arena
 * frame's projection block, as in 0x263F4: 0x17FA0 for both sides (the
 * DS_00100B08/B00/B62/B60/AF0 set, pushed twice per call), 0x17580, 0x1958C,
 * 0x19068(0), the projections again, 0x1975C and the projections a third
 * time. Then always 0x3CB68, 0x35658(0), 0x35658(1), 0x49C78, 0x1282C,
 * 0x12DA8, 0x1DA08, 0x27FA8, DS_00104AEC |= 2 (AH), and with
 * DS_001078FA == 2 (re-read after 0x27FA8), the word DS_00108892
 * (zero-extended) >= 6 and slot 0's +0x53 == 0, 0x4E11C. The `jnz` at
 * 0x263E8 repeats 0x263E6's on the same flags and is never taken.
 * EBX/ECX/EDX are pushed and popped; every callee keeps them or they are
 * reloaded before a read. */
void game_mode_04_step(void)
{
    fight_slot_clear();                                 /* 0x26257 0x3C5CC */
    camera_screen_base(0, (s32)DSB(DS_0010782A));       /* 0x2625C..0x26266 0x16D58 */
    camera_screen_base(1, (s32)DSB(DS_001078BE));       /* 0x2626B..0x26278 0x16D58 */
    DSD(DS_001077E8) = DSD(DS_001077E4);                /* 0x2627D/0x26283 */
    DSD(DS_0010787C) = DSD(DS_00107878);                /* 0x26289/0x2628F */
    if (DSB(DS_001078FA) == 2u) {                       /* 0x26295..0x262A0 */
        camera_project(0, DS_00100B08, DS_00100B00, DS_00100B62,
                       DS_00100B60, DS_00100AF0);       /* 0x262A6..0x262C1 0x17FA0 */
        camera_project(1, DS_00100B0C, DS_00100B04, DS_00100B63,
                       DS_00100B61, DS_00100AF4);       /* 0x262C6..0x262E4 0x17FA0 */
        camera_decay();                                 /* 0x262E9 0x17580 */
        fighter_pass_a();                               /* 0x262EE 0x1958C */
        fighter_pass_b(0u);                             /* 0x262F3/0x262F5 0x19068 */
        camera_project(0, DS_00100B08, DS_00100B00, DS_00100B62,
                       DS_00100B60, DS_00100AF0);       /* 0x262FA..0x26315 0x17FA0 */
        camera_project(1, DS_00100B0C, DS_00100B04, DS_00100B63,
                       DS_00100B61, DS_00100AF4);       /* 0x2631A..0x26338 0x17FA0 */
        fighter_think();                                /* 0x2633D 0x1975C */
        camera_project(0, DS_00100B08, DS_00100B00, DS_00100B62,
                       DS_00100B60, DS_00100AF0);       /* 0x26342..0x2635D 0x17FA0 */
        camera_project(1, DS_00100B0C, DS_00100B04, DS_00100B63,
                       DS_00100B61, DS_00100AF4);       /* 0x26362..0x26380 0x17FA0 */
    }
    fight_slot_pass();                                  /* 0x26385 0x3CB68 */
    fight_hud_pass(0u);                                 /* 0x2638A/0x2638C 0x35658 */
    fight_hud_pass(1u);                                 /* 0x26391/0x26396 0x35658 */
    fight_effects_pass();                               /* 0x2639B 0x49C78 */
    camera_scene_step();                                /* 0x263A0 0x1282C + 0x263A5 0x12DA8 */
    fight_hud_pulse();                                  /* 0x263AA 0x1DA08 */
    flow_round_end_check();                             /* 0x263AF 0x27FA8 */
    DSB(DS_00104AEC) = (u8)(DSB(DS_00104AEC) | 2u);     /* 0x263B4..0x263C5 */
    if (DSB(DS_001078FA) == 2u                          /* 0x263BA..0x263CE */
            && (u32)DSW(DS_00108892) >= 6u              /* 0x263D0..0x263DC */
            && DSB(DS_00107803) == 0u)                  /* 0x263DE..0x263E8 */
        fight_mode25_enter();                           /* 0x263EA 0x4E11C */
}

/* 0x26540 — record §49-O. Mode 0x21's frame handler (one of game_frame's
 * named-gap fallthrough cases, `call 0x26540; jmp 0x2540F`). Byte-for-byte
 * 0x26254's (game_mode_04_step, above) preamble and gated projection block —
 * fight_slot_clear; camera_screen_base per side; the two position latches;
 * then, only with DS_001078FA == 2, camera_project per side, camera_decay,
 * fighter_pass_a, fighter_pass_b(0), camera_project per side again,
 * fighter_think, camera_project per side a third time; then fight_slot_pass,
 * fight_hud_pass(0)/(1) — identical to 0x26254 through here. It diverges
 * exactly where 0x26254 calls fight_effects_pass (0x49C78): 0x26540 calls
 * FUN_0004BF18 instead — fight_4bf18 (record §49-S), the attract loop's
 * volleyball mini-game's per-frame ball/entry driver; see its own header
 * comment in fight.c for the full 8-state switch (over the same singly-
 * linked DS_0010884C list fight_effects_pass walks, not a separate
 * doubly-linked one) and shared-tail derivation. Its own unported callee
 * FUN_0004A868 (case 1's else branch) is the same predicate this codebase
 * already treats as always false elsewhere (spec §7.4). After that,
 * exactly like 0x26254: camera_scene_step (0x1282C + 0x12DA8),
 * fight_hud_pulse (0x1DA08), flow_round_end_check (0x27FA8), then
 * DS_00104AEC |= 2. Unlike 0x26254 there is no closing DS_001078FA/
 * DS_00108892/DS_00107803 gate into fight_mode25_enter (0x4E11C): the raw
 * disassembly ends at the OR and a plain `pop edx; pop ecx; pop ebx; ret`
 * (0x266A0..0x266AA). EBX/ECX/EDX are pushed and popped exactly as
 * 0x26254's. */
void game_mode_21_step(void)
{
    fight_slot_clear();                                 /* 0x26543 0x3C5CC */
    camera_screen_base(0, (s32)DSB(DS_0010782A));       /* 0x26548..0x26552 0x16D58 */
    camera_screen_base(1, (s32)DSB(DS_001078BE));       /* 0x26557..0x26564 0x16D58 */
    DSD(DS_001077E8) = DSD(DS_001077E4);                /* 0x26569/0x2656F */
    DSD(DS_0010787C) = DSD(DS_00107878);                /* 0x26575/0x2657B */
    if (DSB(DS_001078FA) == 2u) {                       /* 0x26581..0x2658C */
        camera_project(0, DS_00100B08, DS_00100B00, DS_00100B62,
                       DS_00100B60, DS_00100AF0);       /* 0x26592..0x265AD 0x17FA0 */
        camera_project(1, DS_00100B0C, DS_00100B04, DS_00100B63,
                       DS_00100B61, DS_00100AF4);       /* 0x265B2..0x265D0 0x17FA0 */
        camera_decay();                                 /* 0x265D5 0x17580 */
        fighter_pass_a();                               /* 0x265DA 0x1958C */
        fighter_pass_b(0u);                             /* 0x265DF/0x265E1 0x19068 */
        camera_project(0, DS_00100B08, DS_00100B00, DS_00100B62,
                       DS_00100B60, DS_00100AF0);       /* 0x265E6..0x26601 0x17FA0 */
        camera_project(1, DS_00100B0C, DS_00100B04, DS_00100B63,
                       DS_00100B61, DS_00100AF4);       /* 0x26606..0x26624 0x17FA0 */
        fighter_think();                                /* 0x26629 0x1975C */
        camera_project(0, DS_00100B08, DS_00100B00, DS_00100B62,
                       DS_00100B60, DS_00100AF0);       /* 0x2662E..0x26649 0x17FA0 */
        camera_project(1, DS_00100B0C, DS_00100B04, DS_00100B63,
                       DS_00100B61, DS_00100AF4);       /* 0x2664E..0x2666C 0x17FA0 */
    }
    fight_slot_pass();                                  /* 0x26671 0x3CB68 */
    fight_hud_pass(0u);                                 /* 0x26676/0x26678 0x35658 */
    fight_hud_pass(1u);                                 /* 0x2667D/0x26682 0x35658 */
    fight_4bf18();                                       /* 0x26687 0x4BF18 (record §49-S) */
    camera_scene_step();                                /* 0x2668C 0x1282C + 0x26691 0x12DA8 */
    fight_hud_pulse();                                  /* 0x26696 0x1DA08 */
    flow_round_end_check();                             /* 0x2669B 0x27FA8 */
    DSB(DS_00104AEC) = (u8)(DSB(DS_00104AEC) | 2u);     /* 0x266A0 */
}

/* ---- mode 0x13, the challenge screen 0x424E8 and its callees (record §48-D) */

#define DS_000C8364 0x000C8364u   /* no symbols.h name: descriptor, id 0x351 */
#define DS_000C8378 0x000C8378u   /* no symbols.h name: descriptor, id 0x352 */
#define DS_000C838C 0x000C838Cu   /* no symbols.h name: descriptor, id 0x3B1 */
#define DS_000C83A0 0x000C83A0u   /* no symbols.h name: descriptor, id 0x34F (the count) */
#define DS_000C83B4 0x000C83B4u   /* no symbols.h name: descriptor, +0x10 = DS_000C83C4 */
#define DS_000C87BC 0x000C87BCu   /* no symbols.h name: descriptor, id 0x3F11 */
#define DS_000BB830 0x000BB830u   /* no symbols.h name: [char * 2] side 0's fighter descriptor */
#define DS_000BB834 0x000BB834u   /* no symbols.h name: [char * 2] side 1's fighter descriptor */
#define DS_000BB880 0x000BB880u   /* no symbols.h name: [char * 2] side 0's winner descriptor */
#define DS_000BB884 0x000BB884u   /* no symbols.h name: [char * 2] side 1's winner descriptor */
#define DS_000BB8D0 0x000BB8D0u   /* no symbols.h name: [char] the secondary actor (0x33D38's) */
#define DS_000C8312 0x000C8312u   /* no symbols.h name: [char] word x - 2 (side 0, loser/draw) */
#define DS_000C8320 0x000C8320u   /* no symbols.h name: [char] word x - 2 (side 1, loser/draw) */
#define DS_000C832E 0x000C832Eu   /* no symbols.h name: [char] word x - 2 (side 0, winner) */
#define DS_000C833C 0x000C833Cu   /* no symbols.h name: [char] word x - 2 (side 1, winner) */
#define DS_000C834A 0x000C834Au   /* no symbols.h name: [char] word a3 - 2 (the winner) */
#define DS_000E8816 0x000E8816u   /* no symbols.h name: the joined side's 6.0 stream */
#define FN_00028D68 0x00028D68u   /* no symbols.h name: frontend_char_screen_hook */

/* 0x20E90 — record §48-D. EAX = the stage (the zero-extended word
 * DS_00104AFC at its only caller, 0x4256A in 0x424E8; not clamped, unlike
 * 0x20DF4's). EDX is pushed and popped. The dwords DS_000F0AEC/DS_000F0AF0
 * and the words DS_000F0AFA/DS_000F0AF8 = 0 (EDX, zeroed twice), then
 * 0x38730 on the stage: 0x20DF4's 0x20E4C..0x20E63 stores and its 0x20E7F
 * call, without the rest. */
void flow_scroll_reset(u32 stage)
{
    DSD(DS_000F0AEC) = 0u;                              /* 0x20E93 */
    DSD(DS_000F0AF0) = 0u;                              /* 0x20E99 */
    DSW(DS_000F0AFA) = 0u;                              /* 0x20EA1 */
    DSW(DS_000F0AF8) = 0u;                              /* 0x20EA8 */
    render_scroll_setup(stage);                         /* 0x20EAF 0x38730 */
}

/* 0x29CBC — record §48-D. EAX = side, EDX = the character: the dword
 * 0xA8ADC[ch] when the byte DS_00105B34[side] is non-zero, else 0xA8AC0[ch]
 * (0x29CDC's twin on other tables). EBX is pushed and popped. Its callers
 * 0x42979, 0x42A13, 0x42AAD and 0x42AE3 are all in 0x428B8. */
static u32 flow_challenge_handle(u32 side, u32 ch)
{
    if (DSB(DS_00105B34 + side) != 0u)                  /* 0x29CBD..0x29CC8 */
        return DSD(DS_000A8ADC + ch * 4u);              /* 0x29CCA */
    return DSD(DS_000A8AC0 + ch * 4u);                  /* 0x29CD2 */
}

/* 0x428B8 — record §48-D. The challenge screen's actors (0x424E8's case 0,
 * 0x42533; its only caller). EBX/ECX/EDX/ESI/EDI are pushed and popped.
 * 0x29D60 (a bare `ret`); the dwords DS_001080DC/E0/E4/E8 = 0 (EDX) and the
 * count byte DS_00108110 = 9 (AH). Two 0x2AE14 spawns with a4 = EBX =
 * 0xFFFFC400 (above the screen, as 0x430E8's pair at a4 0): 0xC8364 (a2 =
 * EDX = 0, a3 0xF0, a5 = the pushed EDX = 0) into DS_001080B4 and 0xC8378
 * (a2 0x2A00, a3 0xF1, a5 = ESI = 0) into DS_001080B8. Then pb = the
 * second's +0x56 word | 0x400 (EDI, `xor edi,edi` first, so the upper half
 * is 0) and pa = the first's | 0x400 (ESI, the `or` at 0x4293E, which the
 * r = 0 arm skips and never reads). On the match result r = DS_00104AD4 (a
 * dword; `jc`/`jbe`/`jz` against 1 and 2, then 0x4295A `test eax,eax`):
 * - 0 (side 0 won): 0x29CBC(1, slot 1's character DS_001078BE) patches
 *   0xC83B4's +0x10 (DS_000C83C4) and three children of pb: 0xC83B4 (a2
 *   0x30, a3 0xE0, a4 0xDA) into DS_001080F8, 0xC838C (0x14, 0xF3, 0xF) into
 *   DS_001080E4 and 0xC83A0 (0x46, 0xF4, 0x34) into DS_001080DC; then with
 *   slot 0's think gate DS_00107813 == 0 (CH) DS_00104B19 = 0 (CH) and
 *   DS_00104B1F = 1, else DS_00104B1F &= 0xFD.
 * - 1: the mirror, children of pa: 0x29CBC(0, DS_0010782A), 0xC83B4 into
 *   DS_001080FC, 0xC838C into DS_001080E8 and 0xC83A0 (a2 0x4C) into
 *   DS_001080E0; with slot 1's DS_001078A7 == 0 DS_00104B19 = 0 (DH) and
 *   DS_00104B1F = 2 (BH), else DS_00104B1F &= 0xFE.
 * - 2: both sides' six, alternating pa/pb (0xC83C4 patched before each
 *   0xC83B4 spawn), and DS_00104B1F = 0 (DL).
 * - anything else: none.
 * The a3/a4 values survive 0x29CBC (pushes EBX, never names ECX) and are
 * reloaded after each 0x2AE14 (which keeps only ESI/EDI/EBP). Last, the
 * first record's +0x36 word = 0x100 and +0x5B = 0, the second's +0x36 =
 * 0x40 and +0x5B = 0; with r != 1 (re-read, 0x42BA0) DS_001080F8's record
 * gets +0x4E = 1 and +0x2E += 4 (the word, `add edx,4` on DX). For r
 * outside 0..2 that is whatever DS_001080F8 held before: nothing here
 * stores it. */
void flow_challenge_open(void)
{
    u32 r, pa, pb, rec;
    /* 0x428BD 0x29D60 is a ret-only no-op. */
    DSD(DS_001080DC) = 0u;                              /* 0x428D2 */
    DSD(DS_001080E0) = 0u;                              /* 0x428D8 */
    DSD(DS_001080E4) = 0u;                              /* 0x428DE */
    DSB(DS_00108110) = 9u;                              /* 0x428E4 (AH) */
    DSD(DS_001080E8) = 0u;                              /* 0x428F0 */
    DSD(DS_001080B4) = actor_spawn((const u32 *)(mem + DS_000C8364), 0u, 0xF0u,
                                   0xFFFFC400u, 0u);    /* 0x428F6 0x2AE14, 0x4290A */
    rec = actor_spawn((const u32 *)(mem + DS_000C8378), 0x2A00u, 0xF1u,
                      0xFFFFC400u, 0u);                 /* 0x42917 0x2AE14 */
    DSD(DS_001080B8) = rec;                             /* 0x42922 */
    pb = (u32)DSW(rec + 0x56u) | 0x400u;                /* 0x42915, 0x42927, 0x42930 */
    pa = (u32)DSW(DSD(DS_001080B4) + 0x56u) | 0x400u;   /* 0x4291C, 0x42935, 0x4293E */
    r = DSD(DS_00104AD4);                               /* 0x4292B */
    if (r == 0u) {                                      /* 0x4293C `jc`, 0x4295A */
        DSD(DS_000C83C4) = flow_challenge_handle(1u, DSB(DS_001078BE));  /* 0x42962..0x42984 0x29CBC */
        DSD(DS_001080F8) = actor_spawn((const u32 *)(mem + DS_000C83B4),
                                       0x30u, 0xE0u, 0xDAu, pb);    /* 0x4298E 0x2AE14, 0x429A3 */
        DSD(DS_001080E4) = actor_spawn((const u32 *)(mem + DS_000C838C),
                                       0x14u, 0xF3u, 0xFu, pb);     /* 0x429AD 0x2AE14, 0x429C2 */
        DSD(DS_001080DC) = actor_spawn((const u32 *)(mem + DS_000C83A0),
                                       0x46u, 0xF4u, 0x34u, pb);    /* 0x429CC 0x2AE14, 0x429D7 */
        if (DSB(DS_00107813) == 0u) {                   /* 0x429D1..0x429DE */
            DSB(DS_00104B19) = 0u;                      /* 0x429E2 (CH) */
            DSB(DS_00104B1F) = 1u;                      /* 0x429E8 (AH) */
        } else {
            DSB(DS_00104B1F) = (u8)(DSB(DS_00104B1F) & 0xFDu);  /* 0x429F3 */
        }
    } else if (r == 1u) {                               /* 0x42946 `jbe` */
        DSD(DS_000C83C4) = flow_challenge_handle(0u, DSB(DS_0010782A));  /* 0x429FF..0x42A1E 0x29CBC */
        DSD(DS_001080FC) = actor_spawn((const u32 *)(mem + DS_000C83B4),
                                       0x30u, 0xE0u, 0xDAu, pa);    /* 0x42A28 0x2AE14, 0x42A3D */
        DSD(DS_001080E8) = actor_spawn((const u32 *)(mem + DS_000C838C),
                                       0x14u, 0xF3u, 0xFu, pa);     /* 0x42A47 0x2AE14, 0x42A5C */
        DSD(DS_001080E0) = actor_spawn((const u32 *)(mem + DS_000C83A0),
                                       0x4Cu, 0xF4u, 0x34u, pa);    /* 0x42A66 0x2AE14, 0x42A71 */
        if (DSB(DS_001078A7) == 0u) {                   /* 0x42A6B..0x42A78 */
            DSB(DS_00104B19) = 0u;                      /* 0x42A7C (DH) */
            DSB(DS_00104B1F) = 2u;                      /* 0x42A82 (BH) */
        } else {
            DSB(DS_00104B1F) = (u8)(DSB(DS_00104B1F) & 0xFEu);  /* 0x42A8D */
        }
    } else if (r == 2u) {                               /* 0x4294F */
        DSD(DS_000C83C4) = flow_challenge_handle(0u, DSB(DS_0010782A));  /* 0x42A99..0x42AB8 0x29CBC */
        DSD(DS_001080FC) = actor_spawn((const u32 *)(mem + DS_000C83B4),
                                       0x30u, 0xE0u, 0xDAu, pa);    /* 0x42AC2 0x2AE14, 0x42ACC */
        DSD(DS_000C83C4) = flow_challenge_handle(1u, DSB(DS_001078BE));  /* 0x42AC7..0x42AEE 0x29CBC */
        DSD(DS_001080F8) = actor_spawn((const u32 *)(mem + DS_000C83B4),
                                       0x30u, 0xE0u, 0xDAu, pb);    /* 0x42AF8 0x2AE14, 0x42B0D */
        DSD(DS_001080E8) = actor_spawn((const u32 *)(mem + DS_000C838C),
                                       0x14u, 0xF3u, 0xFu, pa);     /* 0x42B17 0x2AE14, 0x42B2C */
        DSD(DS_001080E4) = actor_spawn((const u32 *)(mem + DS_000C838C),
                                       0x14u, 0xF3u, 0xFu, pb);     /* 0x42B36 0x2AE14, 0x42B4B */
        DSD(DS_001080E0) = actor_spawn((const u32 *)(mem + DS_000C83A0),
                                       0x4Cu, 0xF4u, 0x34u, pa);    /* 0x42B55 0x2AE14, 0x42B6A */
        DSD(DS_001080DC) = actor_spawn((const u32 *)(mem + DS_000C83A0),
                                       0x46u, 0xF4u, 0x34u, pb);    /* 0x42B74 0x2AE14, 0x42B7B */
        DSB(DS_00104B1F) = 0u;                          /* 0x42B80 (DL) */
    }
    rec = DSD(DS_001080B4);                             /* 0x42B86 */
    DSW(rec + 0x36u) = 0x100u;                          /* 0x42B8B */
    DSB(rec + 0x5Bu) = 0u;                              /* 0x42B91 */
    rec = DSD(DS_001080B8);                             /* 0x42B95 */
    DSW(rec + 0x36u) = 0x40u;                           /* 0x42B9A */
    DSB(rec + 0x5Bu) = 0u;                              /* 0x42BA6 */
    if (DSD(DS_00104AD4) != 1u) {                       /* 0x42BA0..0x42BAD */
        rec = DSD(DS_001080F8);                         /* 0x42BAF */
        DSB(rec + 0x4Eu) = 1u;                          /* 0x42BB8 */
        DSW(rec + 0x2Eu) = (u16)(DSW(rec + 0x2Eu) + 4u);    /* 0x42BB4/0x42BBC/0x42BBF */
    }
}

/* 0x42BCC — record §48-D. The drop (0x424E8's case 1, 0x42553; its only
 * caller). EBX/ECX/EDX/ESI are pushed and popped. When both records
 * DS_001080B4/B8 have their +0x5B byte set, DS_00104B25 is incremented and
 * nothing else. Otherwise, per record (EBX = 0, 4) whose +0x5B is 0: the
 * +0x36 word (the top half of the +0x34 dword) += 0x40; when the signed sum
 * of v = +0x34 >> 16 (`sar`) and the +0x1C dword is not negative (0x42C1B
 * `jl`), +0x1C = 0 and +0x36 = -(v / 3) (`sar edx,0x1f; idiv esi`, a
 * signed truncating divide, then `neg`); when that word is negative (0x42C4C
 * `test si,si; jge`) and its magnitude (+0x34 >> 16, negated) is at most
 * 0x40 (0x42C66 `jg`), +0x36 = 0 and +0x5B = 1. The `jge` at 0x42C51
 * repeats 0x42C4F's on the same flags and is never taken, so its
 * non-negative arm (0x42C5D) is dead. The record pointer is re-read from
 * DS_001080B4[EBX] before each group of accesses; nothing in between
 * stores it. */
void flow_challenge_drop(void)
{
    u32 o;
    if (DSB(DSD(DS_001080B4) + 0x5Bu) != 0u             /* 0x42BD0..0x42BD9 */
            && DSB(DSD(DS_001080B8) + 0x5Bu) != 0u) {   /* 0x42BDB..0x42BE4 */
        DSB(DS_00104B25) = (u8)(DSB(DS_00104B25) + 1u); /* 0x42BE6 */
        return;
    }
    for (o = 0; o != 8u; o += 4u) {                     /* 0x42BF1, 0x42C7E..0x42C84 */
        u32 rec = DSD(DS_001080B4 + o);                 /* 0x42BF3 */
        s32 v;
        if (DSB(rec + 0x5Bu) != 0u) continue;           /* 0x42BF9/0x42BFD */
        DSW(rec + 0x36u) = (u16)(DSW(rec + 0x36u) + 0x40u);     /* 0x42C03 */
        v = (s32)DSD(rec + 0x34u) >> 16;                /* 0x42C08..0x42C14 */
        if ((s32)((u32)v + DSD(rec + 0x1Cu)) < 0) continue;     /* 0x42C11, 0x42C17..0x42C1B */
        DSD(rec + 0x1Cu) = 0u;                          /* 0x42C1D */
        v = (s32)DSD(rec + 0x34u) >> 16;                /* 0x42C24..0x42C2D */
        DSW(rec + 0x36u) = (u16)(-(v / 3));             /* 0x42C30..0x42C3E */
        if ((s16)DSW(rec + 0x36u) >= 0) continue;       /* 0x42C42..0x42C4F */
        if (-((s32)DSD(rec + 0x34u) >> 16) > 0x40) continue;    /* 0x42C53..0x42C66 */
        DSW(rec + 0x36u) = 0u;                          /* 0x42C68/0x42C6E */
        DSB(rec + 0x5Bu) = 1u;                          /* 0x42C74/0x42C7A */
    }
}

/* 0x42724 — record §48-D. The challenge screen's fighters (0x424E8's case 2,
 * 0x4256F; its only caller). EBX/ECX/EDX/ESI/EDI are pushed and popped; the
 * frame's [esp] and [esp+4] hold the characters c0 = DS_0010782A and c1 =
 * DS_001078BE (bytes). On r = DS_00104AD4 (a dword, `jc`/`jbe`/`jz` against
 * 1 and 2, then 0x42757 `test eax,eax`), two 0x2AE14 spawns with a4 = EBX = 0
 * and a5 = 0 (pushed) into the slot records DS_001077B0 (side 0) and
 * DS_00107844 (side 1). Each x is a signed word read as `mov reg,[c*2 + T];
 * sar reg,0x10` (the word at T + 2 + c * 2):
 * - 0: 0xBB880[c0 * 2] at x 0xC832E[c0], a3 0xC834A[c0] (0x4275F pushes
 *   EAX = 0 first, so [esp + 4] is c0); then 0xBB834[c1 * 2] at x
 *   0xC8320[c1], a3 0xE00;
 * - 1: 0xBB830[c0 * 2] at x 0xC8312[c0], a3 0xE00; then 0xBB884[c1 * 2] at x
 *   0xC833C[c1] with a3 = the word 0xC834A[c0], not c1's: 0x427BA reads
 *   `[esi + 0xc834a]` with ESI = c0 * 2 (0x4278A, kept by 0x2AE14), while the
 *   0x427B6 `mov al,[esp+8]` (after the second push) is c1;
 * - 2: 0xBB830[c0 * 2] (x 0xC8312[c0], a3 0xE00) and 0xBB834[c1 * 2] (x
 *   0xC8320[c1], a3 0xE00);
 * - anything else: no spawn, the slot records kept.
 * Then 0x29BC8(0, DS_001077B0, c0) and 0x29BC8(1, DS_00107844, c1) (EAX =
 * side, EDX = the character, EBX = the record; ESI = 0 is loaded between
 * them and survives the second, which pushes ECX and whose 0x2A17C pushes
 * ECX/ESI). Per slot (ESI = 0, 0x94; EDI = 0): 0x2AE14(0xBB8D0[c], 0, 0, 0,
 * a5 = (the slot record's +0x56 word | 0x400) & 0xFFFF) into the slot's +4
 * with the child's +0x59 = 0xFE. c is c0 for BOTH slots: after the 0x42869
 * push, `mov al,[esp+4]` (0x4286E) is [esp] before it, c0, and 0x2AE14's
 * `ret 4` restores ESP each time. Last, DS_001080EC = DS_001077B0 and
 * DS_001080F0 = DS_00107844. */
void flow_challenge_fighters(void)
{
    u32 c0 = DSB(DS_0010782A);                          /* 0x4272C/0x42731 */
    u32 c1 = DSB(DS_001078BE);                          /* 0x42734/0x42739 */
    u32 r = DSD(DS_00104AD4);                           /* 0x4273D */
    u32 s;
    if (r == 0u) {                                      /* 0x42745 `jc`, 0x42757 */
        DSD(DS_001077B0) = actor_spawn(
            (const u32 *)(mem + DSD(DS_000BB880 + c0 * 8u)),
            (u32)((s32)DSD(DS_000C832E + c0 * 2u) >> 16),
            (u32)((s32)DSD(DS_000C834A + c0 * 2u) >> 16), 0u, 0u);  /* 0x4275F..0x4277A, 0x427F7 0x2AE14, 0x427FC */
        DSD(DS_00107844) = actor_spawn(
            (const u32 *)(mem + DSD(DS_000BB834 + c1 * 8u)),
            (u32)((s32)DSD(DS_000C8320 + c1 * 2u) >> 16),
            0xE00u, 0u, 0u);                            /* 0x42801..0x4281A, 0x42821 0x2AE14, 0x42826 */
    } else if (r == 1u) {                               /* 0x42747 `jbe` */
        DSD(DS_001077B0) = actor_spawn(
            (const u32 *)(mem + DSD(DS_000BB830 + c0 * 8u)),
            (u32)((s32)DSD(DS_000C8312 + c0 * 2u) >> 16),
            0xE00u, 0u, 0u);                            /* 0x42783..0x427A5, 0x427A8 0x2AE14, 0x427AD */
        DSD(DS_00107844) = actor_spawn(
            (const u32 *)(mem + DSD(DS_000BB884 + c1 * 8u)),
            (u32)((s32)DSD(DS_000C833C + c1 * 2u) >> 16),
            (u32)((s32)DSD(DS_000C834A + c0 * 2u) >> 16), 0u, 0u);  /* 0x427B2..0x427CF, 0x42821 0x2AE14, 0x42826 */
    } else if (r == 2u) {                               /* 0x4274C */
        DSD(DS_001077B0) = actor_spawn(
            (const u32 *)(mem + DSD(DS_000BB830 + c0 * 8u)),
            (u32)((s32)DSD(DS_000C8312 + c0 * 2u) >> 16),
            0xE00u, 0u, 0u);                            /* 0x427D8..0x427F0, 0x427F7 0x2AE14, 0x427FC */
        DSD(DS_00107844) = actor_spawn(
            (const u32 *)(mem + DSD(DS_000BB834 + c1 * 8u)),
            (u32)((s32)DSD(DS_000C8320 + c1 * 2u) >> 16),
            0xE00u, 0u, 0u);                            /* 0x42801..0x4281A, 0x42821 0x2AE14, 0x42826 */
    }
    fighter_29bc8(0u, DSD(DS_001077B0), c0);            /* 0x4282B..0x42838 0x29BC8 */
    fighter_29bc8(1u, DSD(DS_00107844), c1);            /* 0x4283D..0x42850 0x29BC8 */
    for (s = 0; s < 2u; s++) {                          /* 0x4284E, 0x42888..0x42898 */
        u32 slot = DS_001077B0 + s * 0x94u;
        u32 a5 = ((u32)DSW(DSD(slot) + 0x56u) | 0x400u) & 0xFFFFu;  /* 0x42857..0x42864 */
        u32 rec = actor_spawn((const u32 *)(mem + DSD(DS_000BB8D0 + c0 * 4u)),
                              0u, 0u, 0u, a5);          /* 0x4286A..0x42876, 0x4287D 0x2AE14 */
        DSD(slot + 4u) = rec;                           /* 0x42882 */
        DSB(rec + 0x59u) = 0xFEu;                       /* 0x4288E */
    }
    DSD(DS_001080EC) = DSD(DS_001077B0);                /* 0x4289A/0x4289F */
    DSD(DS_001080F0) = DSD(DS_00107844);                /* 0x428A4/0x428A9 */
}

/* 0x42FE0 — record §48-D. EAX = the side that takes the challenge (kept in
 * EBX); EBX/ECX/EDX/ESI/EDI are pushed and popped, so every register the
 * caller loads around the call survives it. Side 0: DS_001080FC's record
 * takes the 0xE8816 stream at 6.0 (0x2BC30, the pushed 0x40C00000); when
 * DS_001080E0 is non-zero, 0x2B150 on it and on DS_001080E8 and
 * DS_001080E0 = 0 (EDI); slot 0's byte +0x7F (DS_0010782F) = 0 (DH) and
 * DS_00104B1F |= 1. Any other side: the same with DS_001080F8, DS_001080DC/
 * DS_001080E4, slot 1's +0x7F (DS_001078C3, AH) and bit 2. Then 0x2C2B0
 * (side, 0x1C), DS_00104B19 = 1 (CH), 0x41310(side, 1) and 0x41310(side ^ 1,
 * 1) (0x430A9 `xor bl,ch`, CH = 1). Its only caller is 0x42CB4 (four
 * sites). */
void flow_challenge_join(u32 side)
{
    if (side == 0u) {                                   /* 0x42FE7/0x42FE9 */
        actors_anim_begin(DSD(DS_001080FC), DS_000E8816,
                          0x40C00000u);                 /* 0x42FEB..0x42FFA 0x2BC30 */
        if (DSD(DS_001080E0) != 0u) {                   /* 0x42FFF..0x43007 */
            actor_set_dead(DSD(DS_001080E0));           /* 0x4300B 0x2B150 */
            actor_set_dead(DSD(DS_001080E8));           /* 0x43017 0x2B150 */
            DSD(DS_001080E0) = 0u;                      /* 0x4301C (EDI) */
        }
        DSB(DS_0010782F) = 0u;                          /* 0x4302D (DH) */
        DSB(DS_00104B1F) = (u8)(DSB(DS_00104B1F) | 1u); /* 0x43022/0x4302A/0x43033 */
    } else {
        actors_anim_begin(DSD(DS_001080F8), DS_000E8816,
                          0x40C00000u);                 /* 0x4303B..0x4304A 0x2BC30 */
        if (DSD(DS_001080DC) != 0u) {                   /* 0x4304F..0x43057 */
            actor_set_dead(DSD(DS_001080DC));           /* 0x4305B 0x2B150 */
            actor_set_dead(DSD(DS_001080E4));           /* 0x43067 0x2B150 */
            DSD(DS_001080DC) = 0u;                      /* 0x4306C (ECX) */
        }
        DSB(DS_001078C3) = 0u;                          /* 0x4307D (AH) */
        DSB(DS_00104B1F) = (u8)(DSB(DS_00104B1F) | 2u); /* 0x43072/0x4307A/0x43083 */
    }
    prompt_side_erase((s32)side, 0x1C);                 /* 0x43089..0x43092 0x2C2B0 */
    DSB(DS_00104B19) = 1u;                              /* 0x4309E (CH) */
    fighter_41310(side, 1);                             /* 0x43097..0x430A4 0x41310 */
    fighter_41310(side ^ 1u, 1);                        /* 0x430A9..0x430B2 0x41310 */
}

/* 0x42CB4 — record §48-D. The challenge poll and its countdown (0x424E8's
 * cases 3 and 4, 0x425DE; its only caller). EBX/ECX/EDX/ESI/EDI/EBP are
 * pushed and popped; EAX is not read. On r = DS_00104AD4 (a dword, the
 * 0x42F60 side the loser's):
 * - 0: 0x42F60(1) non-zero: 0x42FE0(1), the hook DS_00104AE4 = 0x28D68, the
 *   words DS_001088EE = DS_00104AFE = 0x78 and DS_00104B25 = 5 (EBX, CX, DL
 *   loaded before the call, which keeps them), return;
 * - 1: 0x42F60(0) non-zero: 0x42FE0(0) and the same stores, return;
 * - 2: per side (EDX = 0, 1, kept by 0x42F60 and 0x42FE0) 0x42F60(side)
 *   non-zero: with DS_00104B1F non-zero 0x42FE0(side) and the stores,
 *   return; else 0x42FE0(side) only and the next side;
 * - otherwise (or no challenge) the countdown: a forced tick when
 *   DS_00105C04 is set (DS_00108110 = 0xA, DS_00105C04 = 0 by AL after `xor
 *   al,al`, EAX = 1), else a newly pressed button, 0x4F778(1) with the
 *   DS_00104B1F bit 0, else 0x4F778(0) with bit 1 (AL, `and eax,0xff`), else
 *   none (EAX = 0). On the frame word DS_000EF6DC & 0x3F == 0 (0x42E0B..
 *   0x42E1D) or a forced tick the byte DS_00108110 is decremented:
 *   - below 0 (signed, 0x42E37 `jge`): with r (EBP, re-read) == 2 or slot
 *     r's think gate DS_00107813 + r * 0x94 (`lea`/`add`/`shl`/`add` to r
 *     * 37, then `[eax*4 + 0x107813]`) == 1, 0x4F1E4 (EAX = 0) and 0x2BAF4
 *     (EAX = 1); else the 0x2D voice. Then on the byte DS_00104B1F (`jc`/
 *     `jbe`/`jz`): 1 clears bit 6 of DS_00104B02..DS_00104B08 (0x42E95
 *     loop, the store `[edx + 0x104b01]` after `inc edx`), 2 clears bit 7
 *     of the same seven. With DS_00104B1D != 0 the hook becomes 0x25AE8 and
 *     the mode word 0x17 (BX); else DS_00104B25 = 0 (CH, the zero just
 *     tested) and the mode word 0x1E (DX), return;
 *   - otherwise the count actors: the non-zero DS_001080E0 and then
 *     DS_001080DC each take +8 = 0xC82EC[count] and +0x28 |= 4, where the
 *     count is the signed byte DS_00108110 (0x42F0F/0x42F35 `mov eax,
 *     [0x10810d]; sar eax,0x18`: the dword's top byte).
 * EDX survives 0x42F60 (pushes it) and 0x42FE0; ECX/EDI/BH survive
 * 0x42FE0. */
void flow_challenge_poll(void)
{
    u32 r = DSD(DS_00104AD4);                           /* 0x42CBA */
    u32 tick;
    s8 n;
    if (r == 0u) {                                      /* 0x42CC2 `jc`, 0x42CD4 */
        if (flow_continue_poll(1u) != 0u) {             /* 0x42CDC..0x42CE8 0x42F60 */
            flow_challenge_join(1u);                    /* 0x42CEE..0x42CFF 0x42FE0 */
            DSD(DS_00104AE4) = FN_00028D68;             /* 0x42D04 (EBX) */
            DSW(DS_001088EE) = 0x78u;                   /* 0x42D0A (CX) */
            DSW(DS_00104AFE) = 0x78u;                   /* 0x42D11 (CX) */
            DSB(DS_00104B25) = 5u;                      /* 0x42D18 (DL) */
            return;                                     /* 0x42D1E -> 0x42F56 */
        }
    } else if (r == 1u) {                               /* 0x42CC4 `jbe` */
        if (flow_continue_poll(0u) != 0u) {             /* 0x42D23..0x42D2C 0x42F60 */
            flow_challenge_join(0u);                    /* 0x42D32..0x42D39 0x42FE0 */
            DSD(DS_00104AE4) = FN_00028D68;             /* 0x42D40 (EDX) */
            DSB(DS_00104B25) = 5u;                      /* 0x42D4B (AH) */
            DSW(DS_001088EE) = 0x78u;                   /* 0x42D51 (DX) */
            DSW(DS_00104AFE) = 0x78u;                   /* 0x42D58 (DX) */
            return;                                     /* 0x42D65 */
        }
    } else if (r == 2u) {                               /* 0x42CC9 */
        u32 side;
        for (side = 0; side < 2u; side++) {             /* 0x42D66, 0x42DB7..0x42DBB */
            if (flow_continue_poll(side) == 0u) continue;   /* 0x42D68..0x42D71 0x42F60 */
            if (DSB(DS_00104B1F) != 0u) {               /* 0x42D73/0x42D7A */
                flow_challenge_join(side);              /* 0x42D86..0x42D8A 0x42FE0 */
                DSD(DS_00104AE4) = FN_00028D68;         /* 0x42D8F (ECX) */
                DSW(DS_001088EE) = 0x78u;               /* 0x42D95 (DI) */
                DSW(DS_00104AFE) = 0x78u;               /* 0x42D9C (DI) */
                DSB(DS_00104B25) = 5u;                  /* 0x42DA3 (BH) */
                return;                                 /* 0x42DAF */
            }
            flow_challenge_join(side);                  /* 0x42DB0/0x42DB2 0x42FE0 */
        }
    }
    if (DSB(DS_00105C04) != 0u) {                       /* 0x42DBD/0x42DC4 */
        DSB(DS_00108110) = 0xAu;                        /* 0x42DCA (CH) */
        DSB(DS_00105C04) = 0u;                          /* 0x42DD0 (AL) */
        tick = 1u;                                      /* 0x42DD5 */
    } else if ((DSB(DS_00104B1F) & 1u) != 0u) {         /* 0x42DDC..0x42DE8 */
        tick = frontend_buttons_pressed(1u) & 0xFFu;    /* 0x42DEA 0x42E01 0x4F778, 0x42E06 */
    } else if ((DSB(DS_00104B1F) & 2u) != 0u) {         /* 0x42DF1..0x42DFD */
        tick = frontend_buttons_pressed(0u) & 0xFFu;    /* 0x42DFF 0x42E01 0x4F778, 0x42E06 */
    } else {
        tick = 0u;                                      /* 0x42DF8 (EAX = 0) */
    }
    if (((u32)DSW(DS_000EF6DC) & 0x3Fu) != 0u && tick == 0u)
        return;                                         /* 0x42E0B..0x42E21 */
    n = (s8)(DSB(DS_00108110) - 1u);                    /* 0x42E27/0x42E2D */
    DSB(DS_00108110) = (u8)n;                           /* 0x42E2F */
    if (n < 0) {                                        /* 0x42E35/0x42E37 */
        r = DSD(DS_00104AD4);                           /* 0x42E3D */
        if (r == 2u || DSB(DS_00107813 + r * 0x94u) == 1u) {    /* 0x42E43..0x42E65 */
            frontend_input_reset();                     /* 0x42E67/0x42E69 0x4F1E4 (eax = 0) */
            actors_reset();                             /* 0x42E6E/0x42E73 0x2BAF4 (eax = 1) */
        } else {
            /* PORT: 0x42E7F 0x2C3FC(0x2D) voice, not wired (record §45-A). */
        }
        if (DSB(DS_00104B1F) == 1u) {                   /* 0x42E84..0x42E8D `jbe` */
            u32 i;
            for (i = 0; i < 7u; i++)                    /* 0x42E95..0x42EAC */
                DSB(DS_00104B02 + i) = (u8)(DSB(DS_00104B02 + i) & 0xBFu);  /* 0x42E97..0x42EA1 */
        } else if (DSB(DS_00104B1F) == 2u) {            /* 0x42E8F/0x42E91 */
            u32 i;
            for (i = 0; i < 7u; i++)                    /* 0x42EAE..0x42EC3 */
                DSB(DS_00104B02 + i) = (u8)(DSB(DS_00104B02 + i) & 0x7Fu);  /* 0x42EB0..0x42EBA */
        }
        if (DSB(DS_00104B1D) != 0u) {                   /* 0x42EC5..0x42ECD */
            DSD(DS_00104AE4) = FN_00025AE8;             /* 0x42ED9 */
            DSW(DS_00104B00) = 0x17u;                   /* 0x42EDE (BX) */
            return;                                     /* 0x42EEB */
        }
        DSB(DS_00104B25) = 0u;                          /* 0x42EF1 (CH) */
        DSW(DS_00104B00) = 0x1Eu;                       /* 0x42EF7 (DX) */
        return;                                         /* 0x42F04 */
    }
    if (DSD(DS_001080E0) != 0u) {                       /* 0x42F05..0x42F0D */
        u32 rec = DSD(DS_001080E0);
        DSD(rec + 8u) = DSD(DS_000C82EC
                            + (u32)((s32)(s8)DSB(DS_00108110) * 4));   /* 0x42F0F..0x42F24 */
        DSB(rec + 0x28u) = (u8)(DSB(rec + 0x28u) | 4u);     /* 0x42F27 */
    }
    if (DSD(DS_001080DC) != 0u) {                       /* 0x42F2B..0x42F33 */
        u32 rec = DSD(DS_001080DC);
        DSD(rec + 8u) = DSD(DS_000C82EC
                            + (u32)((s32)(s8)DSB(DS_00108110) * 4));   /* 0x42F35..0x42F50 */
        DSB(rec + 0x28u) = (u8)(DSB(rec + 0x28u) | 4u);     /* 0x42F44..0x42F53 */
    }
}

/* 0x424E8 — record §48-D. Mode 0x13's handler, the challenge screen after a
 * match (0x24C5C case 0x13, the table entry 0x253C4 `call 0x424e8`, then
 * 0x253C9 `jmp 0x2540F`; its only caller). Mode 0x13 is the return mode
 * DS_00104AFA that mode 0x15 takes: stored at 0x289A8, 0x28AC1 and 0x28B5D
 * (0x28788, mode 9's handler, each with DS_00104B25 = 0, mode 0x17 and the
 * hook 0x29B74, which darkens into mode 0x15) and at 0x415EF (0x41578, with
 * DS_00104B25 = 0 at 0x415FF). EBX/ECX/EDX/ESI/EDI/EBP are pushed and
 * popped.
 * A jump table at 0x424D0 on the byte DS_00104B25 (`cmp al,5; ja`, `and
 * eax,0xff`; entries 0x42508, 0x42553, 0x4255D, 0x425C0, 0x425DE, 0x425E5):
 * - 0: the 0x2C voice; 0x2BAF4 (EAX = 1; ECX = EBX = 0); 0x4F1D0 (EAX = EDX
 *   = 0); 0x38B18(0xC87BC) with EDX = EBX = 0 (0x2BAF4 and 0x4F1D0 keep
 *   both); 0x428B8; then DS_00104B15 = 0 (DH) and DS_00104B25 + 1 (BL,
 *   read after 0x428B8, which does not store it);
 * - 1: 0x42BCC (the drop);
 * - 2: 0x29CFC (a `jmp 0x13DF0`, the effects clear), 0x20E90 on the stage
 *   word DS_00104AFC, 0x42724, 0x4B9AC; then 0x29B74's list walk with a
 *   filter: every 0x33904 entry whose +0 handle is not 0x3E708 (the `jz` at
 *   0x42590 repeats 0x4258E's) and that 0x1C6D4 rejects takes 0x13C70 (EAX
 *   = the entry, DL = 3, EBX = its +0 handle); DS_00104B25 + 1;
 * - 3: when the effects count DS_0009AF3D is 0, the call 0xC7F58[stage] and
 *   DS_00104B25 + 1; then as 4;
 * - 4: 0x42CB4 (the challenge poll), 0x33F08 (the health bars);
 * - 5: 0x4F318 (mode 0x17's countdown, with 0x42CB4's hook 0x28D68),
 *   0x33F08;
 * - above 5: nothing.
 * Then, on the byte DS_00104B1D (CL), each side (ECX = 0, 1) with no
 * reason to skip gets "PRESS START" (0x2C178 at row 0x3800 with the
 * DS_00104529 bit 1, else row 0x1C) when 0x2C060 reports a credit, else
 * "INSERT 1 COIN" (0x2C1C8 at row 0x1C; EBX = 1 is not read). The skips:
 * - 1: side bit side + 1 clear in the dword DS_00104AB8 (re-read per side,
 *   EBP; `test eax,ebp; jz`);
 * - other non-zero: that bit set in the byte DS_00104B1F (zero-extended);
 * - 0: that bit set in DS_00104B1F, or the side's think gate DS_00107813 +
 *   side * 0x94 (ESI) non-zero.
 * The loop registers ECX/ESI/EDI/EBP survive 0x2C060 (EAX only, and EDX
 * pushed), 0x2C178 (pushes EBX/ECX/ESI; its 0x2C0F4 pushes ESI/EDI) and
 * 0x2C1C8 (0x2C1D4 pushes ECX/ESI/EDI). No ported path reaches mode 0x13:
 * 0x28788 (mode 9) and 0x41578's callers 0x416D4 and 0x41C28 (mode 0x12)
 * are unported. */
void game_mode_13_step(void)
{
    u32 side;
    switch (DSB(DS_00104B25)) {                         /* 0x424EE..0x42500 */
    case 0u:
        /* PORT: 0x4250D 0x2C3FC(0x2C) voice, not wired (record §45-A); EDX
         * is game_frame's. */
        actors_reset();                                 /* 0x42512..0x4251B 0x2BAF4 (eax = 1) */
        frontend_origin_zero();                         /* 0x42520..0x42524 0x4F1D0 */
        frontend_spawn_row((const u32 *)(mem + DS_000C87BC), 0u, 0u);   /* 0x42529/0x4252E 0x38B18 */
        flow_challenge_open();                          /* 0x42533 0x428B8 */
        DSB(DS_00104B15) = 0u;                          /* 0x4253E/0x42542 (DH) */
        DSB(DS_00104B25) = (u8)(DSB(DS_00104B25) + 1u); /* 0x42538/0x42540/0x42548 */
        break;                                          /* 0x4254E */
    case 1u:
        flow_challenge_drop();                          /* 0x42553 0x42BCC */
        break;                                          /* 0x42558 */
    case 2u: {
        u32 e;
        effects_clear();                                /* 0x4255D 0x29CFC -> 0x13DF0 */
        flow_scroll_reset(DSW(DS_00104AFC));            /* 0x42562..0x4256A 0x20E90 */
        flow_challenge_fighters();                      /* 0x4256F 0x42724 */
        fight_challenge_crowd();                        /* 0x42574 0x4B9AC */
        for (e = frontend_list_next(0u); e != 0u;       /* 0x42579/0x4257B 0x33904 */
             e = frontend_list_next(e)) {               /* 0x425AB/0x425AD 0x33904, 0x425B4 */
            if (DSD(e) == 0x0003E708u) continue;        /* 0x42586..0x42590 */
            if (frontend_resource_known(e) != 0u) continue;     /* 0x42592..0x4259B 0x1C6D4 */
            (void)effects_spawn(e, 3u, DSD(e));         /* 0x4259D..0x425A6 0x13C70 */
        }
        DSB(DS_00104B25) = (u8)(DSB(DS_00104B25) + 1u); /* 0x425B8 */
        break;                                          /* 0x425BE */
    }
    case 3u:
        if (DSB(DS_0009AF3D) == 0u) {                   /* 0x425C0/0x425C7 */
            /* PORT: the call `[0xC7F58 + stage * 4]` (0x425D1) is not issued,
             * as 0x412A0's (fight_scene_props): the eight entries are 0x412EC
             * (0x412A0's own `ret`) and 0x5D812 (`xor eax,eax; ret`), nothing
             * stores the table, and 0x42CB4 does not read EAX. */
            DSB(DS_00104B25) = (u8)(DSB(DS_00104B25) + 1u);     /* 0x425D8 */
        }
        flow_challenge_poll();                          /* 0x425DE 0x42CB4 */
        fight_health_bars();                            /* 0x425E3 -> 0x425EA 0x33F08 */
        break;
    case 4u:
        flow_challenge_poll();                          /* 0x425DE 0x42CB4 */
        fight_health_bars();                            /* 0x425E3 -> 0x425EA 0x33F08 */
        break;
    case 5u:
        frontend_mode_17_step();                        /* 0x425E5 0x4F318 */
        fight_health_bars();                            /* 0x425EA 0x33F08 */
        break;
    default:
        break;                                          /* 0x424F5 `ja 0x425EF` */
    }
    if (DSB(DS_00104B1D) == 1u) {                       /* 0x425EF..0x425F8 */
        for (side = 0; side < 2u; side++) {             /* 0x425FF, 0x42649..0x42653 */
            if (((side + 1u) & DSD(DS_00104AB8)) == 0u) continue;   /* 0x42606..0x42611 */
            if (config_credit_ready() != 0u)            /* 0x42613 0x2C060, 0x4261A */
                prompt_press_start(side, (DSB(DS_00104529) & 2u) != 0u
                                             ? 0x3800 : 0x1C);  /* 0x4261C..0x42637 0x2C178 */
            else
                prompt_insert_coin(side, 0x1C);         /* 0x4263E..0x42644 0x2C1C8 */
        }
    } else if (DSB(DS_00104B1D) != 0u) {                /* 0x42655/0x42657 */
        for (side = 0; side < 2u; side++) {             /* 0x42663, 0x426A9..0x426B3 */
            if (((side + 1u) & DSB(DS_00104B1F)) != 0u) continue;   /* 0x42665..0x42671 */
            if (config_credit_ready() != 0u)            /* 0x42673 0x2C060, 0x4267A */
                prompt_press_start(side, (DSB(DS_00104529) & 2u) != 0u
                                             ? 0x3800 : 0x1C);  /* 0x4267C..0x42694 0x2C178 */
            else
                prompt_insert_coin(side, 0x1C);         /* 0x4269B..0x426A4 0x2C1C8 */
        }
    } else {
        for (side = 0; side < 2u; side++) {             /* 0x426B5, 0x4270F..0x42719 */
            if (((side + 1u) & DSB(DS_00104B1F)) != 0u) continue;   /* 0x426BE..0x426CB */
            if (DSB(DS_00107813 + side * 0x94u) != 0u) continue;    /* 0x426CD..0x426D4 */
            if (config_credit_ready() != 0u)            /* 0x426D6 0x2C060, 0x426DD */
                prompt_press_start(side, (DSB(DS_00104529) & 2u) != 0u
                                             ? 0x3800 : 0x1C);  /* 0x426DF..0x426FA 0x2C178 */
            else
                prompt_insert_coin(side, 0x1C);         /* 0x42701..0x4270A 0x2C1C8 */
        }
    }
}

/* ---- mode 9, the frame handler 0x28788 and its callees (record §48-Y) ---- */

#define FN_00029B74 0x00029B74u   /* no symbols.h name: frontend_darken_all */
#define FN_0004142C 0x0004142Cu   /* no symbols.h name: fight_hook_4142c */

/* 0x28978/0x28A9D/0x28B3C — record §48-Y. Three of 0x28788's four result-
 * dispatch arms share this close: DS_00104B25 = 0 (0x28989/0x28AB3/0x28B4D,
 * BH/DH/AL), the hook DS_00104AE4 = frontend_darken_all (0x2898F/0x28AB9/
 * 0x28B57), mode DS_00104B00 = 0x17 (0x28995/0x28ACE/0x28B71), return mode
 * DS_00104AFA = 0x13 (0x289A8/0x28AC1/0x28B5D), then
 * config_play_time_close(DS_00104ABC, DS_00104B19) (0x289AF/0x28AD5/0x28B78,
 * already ported, 0x32A3C). Each arm's own DS_00104B17 store happens before
 * this close and is not part of it. */
static void flow_results_darken_close(void)
{
    DSB(DS_00104B25) = 0u;
    DSD(DS_00104AE4) = FN_00029B74;
    DSW(DS_00104B00) = 0x17u;
    DSW(DS_00104AFA) = 0x13u;
    config_play_time_close(DSD(DS_00104ABC), DSB(DS_00104B19));
}

/* 0x286BC — record §48-Y. The post-match streak/handicap update; only
 * caller 0x288CC (0x28788). EBX/ECX/EDX are pushed and popped.
 * DS_00104AD4 == 2 (a draw, 0x286C5/0x286C8): DS_00108106[stage] = 0 (DH),
 * return. Otherwise EBX = DS_00104AD4 is the winner (0/1, 0x286DE); the
 * loser is winner ^ 1 (0x286F9). DS_00107813[winner] == 1 (0x286FC/0x286FF):
 * DS_00104B11 += 1 (0x28701/0x28709/0x28711), fighter_4660c(the byte
 * DS_00108104[loser]) recomputes DS_001082D0 (0x2870B..0x28717, 0x4660C),
 * DS_00108106[stage] = 0 (0x2871C..0x28726), return. Otherwise (0x28730):
 * DS_00104B11 = 0 and DS_00107830[winner] = 0 (0x28732/0x28738, the same
 * table 0x28788's own tail increments — here it is reset instead);
 * DS_00108106[stage] = (winner << 6) | 0x80 | DS_0010782A[winner]
 * (0x2873E..0x28755); DS_00107830[loser] = that byte XOR loser
 * (0x2875B..0x2876F); DS_00107813[loser] == 1 calls fighter_46534(loser, 1)
 * (0x28769..0x2877E). */
static void flow_match_streak_update(void)
{
    u32 result = DSD(DS_00104AD4);                       /* 0x286BF */
    if (result == 2u) {                                  /* 0x286C5/0x286C8 */
        DSB(DS_00108106 + DSW(DS_00104AFC)) = 0u;         /* 0x286CA..0x286D4 */
        return;
    }
    u32 winner = result;                                  /* 0x286DE */
    u32 loser = winner ^ 1u;                               /* 0x286F9 */
    if (DSB(DS_00107813 + winner * 0x94u) == 1u) {          /* 0x286F8..0x286FF */
        DSB(DS_00104B11) = (u8)(DSB(DS_00104B11) + 1u);       /* 0x28701/0x28709/0x28711 */
        fighter_4660c((u32)DSB(DS_00108104 + loser));          /* 0x2870B..0x28717 0x4660C */
        DSB(DS_00108106 + DSW(DS_00104AFC)) = 0u;                /* 0x2871C..0x28726 */
        return;
    }
    DSB(DS_00104B11) = 0u;                                    /* 0x28730/0x28732 */
    DSB(DS_00107830 + winner * 0x94u) = 0u;                     /* 0x28738 */
    {
        u8 flag = (u8)(((u8)winner << 6) | 0x80u
                        | DSB(DS_0010782A + winner * 0x94u));    /* 0x2873E..0x2874A */
        DSB(DS_00108106 + DSW(DS_00104AFC)) = flag;                /* 0x2874C..0x28755 */
        DSB(DS_00107830 + loser * 0x94u) = (u8)(flag ^ (u8)loser); /* 0x2875B..0x2876F */
    }
    if (DSB(DS_00107813 + loser * 0x94u) == 1u)                    /* 0x28769..0x28778 */
        fighter_46534(loser, 1);                                    /* 0x2877A..0x2877E */
}

void game_mode_09_step(void)
{
    fight_slot_clear();                                 /* 0x2878E 0x3C5CC */
    camera_screen_base(0, (s32)DSB(DS_0010782A));       /* 0x28793..0x287A2 0x16D58 */
    camera_screen_base(1, (s32)DSB(DS_001078BE));       /* 0x287A7..0x287B9 0x16D58 */
    DSD(DS_001077E8) = DSD(DS_001077E4);                /* 0x287C3/0x287CD */
    DSD(DS_0010787C) = DSD(DS_00107878);                /* 0x287D2/0x287DC */
    camera_project(0, DS_00100B08, DS_00100B00, DS_00100B62,
                   DS_00100B60, DS_00100AF0);            /* 0x287BE..0x287E3 0x17FA0 */
    camera_project(1, DS_00100B0C, DS_00100B04, DS_00100B63,
                   DS_00100B61, DS_00100AF4);            /* 0x287E8..0x28806 0x17FA0 */
    camera_decay();                                      /* 0x2880B 0x17580 */
    fighter_pass_a();                                    /* 0x28810 0x1958C */
    if (!(DSB(DS_001077F1) & 2u) && !(DSB(DS_00107885) & 2u))   /* 0x28815..0x28825 */
        fight_slot_pass();                               /* 0x28827 0x3CB68 */
    fight_hud_pass(0u);                                  /* 0x2882C/0x2882E 0x35658 */
    fight_hud_pass(1u);                                  /* 0x28833/0x28838 0x35658 */
    fighter_pass_b(1u);                                  /* 0x2883D/0x28842 0x19068 */
    fight_effects_pass();                                /* 0x28847 0x49C78 */
    camera_y_commit();                                   /* 0x2884C 0x12DA8 */

    u16 hold = DSW(DS_00104AF8);                          /* 0x28851 */
    if (hold != 0u) {                                     /* 0x28858/0x2885B */
        hold = (u16)(hold - 1u);                          /* 0x2885D/0x2885F */
        DSW(DS_00104AF8) = hold;                           /* 0x28861 */
        if (hold == 0u) {                                   /* 0x28868 */
            DSB(DS_001078FE) = 1u;                            /* 0x2886A/0x2887E */
            DSB(DS_001078FC) = 1u;                             /* 0x28884 */
            DSB(DS_001077F1) = (u8)(DSB(DS_001077F1) | 0x10u); /* 0x2886C/0x28878/0x28892 */
            DSB(DS_00107885) = (u8)(DSB(DS_00107885) | 0x10u); /* 0x28872/0x2887B/0x2888A */
            DSB(DS_000F0AFE) = 4u;                              /* 0x28890/0x28898 */
        }
    }
    DSB(DS_00104AEC) = (u8)(DSB(DS_00104AEC) | 2u);        /* 0x2889E */

    if (DSB(DS_000F0AFE) != 4u) return;                     /* 0x288A5..0x288AF */
    if (DSB(DS_001078FC) == 0u) return;                      /* 0x288B5..0x288BC */

    fight_effects_hold_all();                                 /* 0x288C2 0x4A708 */
    flow_match_result_text();                                 /* 0x288C7 0x28130 */
    flow_match_streak_update();                                /* 0x288CC 0x286BC */
    DSW(DS_00104AFE) = 0xF0u;                                   /* 0x288D1/0x288E1 */
    DSW(DS_001088EE) = 0x3Cu;                                    /* 0x288D6/0x288E8 */
    if (DSB(DS_00108173) != 0u) {                                 /* 0x288DB/0x288EF */
        DSD(DS_00104AD4) = 2u;                                      /* 0x288F3 */
        DSB(DS_00107813) = 0u;                                       /* 0x288FF */
        DSB(DS_001078A7) = 0u;                                        /* 0x28905 */
    }
    DSB(DS_00104B1B) = 0u;                                           /* 0x28913 */

    if (DSB(DS_00104B1D) == 0u) {                                     /* 0x28919/0x2891B */
        u32 idx = ((DSB(DS_00104B1F) == 3u) ? 1u : 0u) + 1u;           /* 0x2891D..0x2892F */
        DSD(DS_00104ABC) = idx;                                          /* 0x28932 */
        config_play_time_snap(idx, 0u);                                   /* 0x28937 0x32B00 */
    }

    u32 result = DSD(DS_00104AD4);                                       /* 0x2893C */
    if (result == 2u) {                                                    /* 0x28942/0x28945 */
        DSB(DS_00104B17) = 2u;                                               /* 0x2894D/0x28954 */
        if (DSB(DS_00107813) == 1u) DSD(DS_00104AD4) = 0u;                    /* 0x2894F..0x28961 */
        if (DSB(DS_001078A7) == 1u) DSD(DS_00104AD4) = 1u;                     /* 0x28967..0x28973 */
        flow_results_darken_close();                                           /* 0x28978..0x289AF */
        return;                                                                 /* 0x289E4 */
    }

    {
        u32 winner = result;                                                    /* 0x289E5 */
        u32 loser = winner ^ 1u;                                                  /* 0x289F5 */
        if (DSB(DS_00107813 + winner * 0x94u) == 1u) {                            /* 0x289F8..0x28A13 */
            u8 streak = (u8)(DSB(DS_00107830 + loser * 0x94u) + 1u);                /* 0x28A19/0x28A1F */
            DSB(DS_00107830 + loser * 0x94u) = streak;                               /* 0x28A21 */
            if (streak >= 3u && DSB(DS_00108104 + loser) < 6u) {                       /* 0x28A2E..0x28A3E */
                u8 flag = (u8)(((u8)winner << 6) | 0x80u
                                | DSB(DS_0010782A + winner * 0x94u));                    /* 0x28A54..0x28A5C */
                DSB(DS_00108106 + DSW(DS_00104AFC)) = flag;                                /* 0x28A6F */
                DSB(DS_00104B17) = 1u;                                                      /* 0x28A87 */
                DSB(DS_00107830 + loser * 0x94u) = (u8)(flag ^ (u8)loser);                   /* 0x28A85/0x28A8D */
            } else {
                DSB(DS_00104B17) = 2u;                                                        /* 0x28A96 */
            }
            flow_results_darken_close();                                                       /* 0x28A9D..0x28AD5 */
            return;                                                                              /* 0x28B0A */
        }

        /* PORT: 0x28B0D..0x28B25. The winner-think != 1 arm re-reads the
         * loser's DS_00107813 think-byte; when it is also 1, the raw calls
         * (val = DS_0010782A[loser], arm = 0) into 0x32BAC, this function's
         * proven no-op (see the header comment in flow.h); omitted. */

        if (DSW(DS_00104AFC) == 7u) {                                                  /* 0x28B2C..0x28B35 */
            fight_stage_marks_clear();                                                   /* 0x28B37 0x4246C */
            DSB(DS_00104B17) = 0u;                                                        /* 0x28B4D */
            flow_results_darken_close();                                                    /* 0x28B3C..0x28B78 */
            return;                                                                          /* 0x28BAD */
        }

        DSB(DS_00104B17) = 1u;                                                         /* 0x28BB5 */
        DSW(DS_00104B00) = 0x17u;                                                        /* 0x28BC0 */
        DSD(DS_00104AE4) = FN_0004142C;                                                    /* 0x28BC6 */
    }
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

/* ---- mode 8, the frame handler 0x28468 (record §49-C) -------------------- */

/* 0x28468 — record §49-C. See flow.h for the full derivation; a near-
 * identical, but slimmer, sibling of mode 9's game_mode_09_step (0x28788,
 * record §48-Y). FN_00025BBC (game_hook_25bbc) is the same hook constant
 * game_hook_259cc installs above. */
void game_mode_08_step(void)
{
    fight_slot_clear();                                 /* 0x2846D 0x3C5CC */
    camera_screen_base(0, (s32)DSB(DS_0010782A));       /* 0x28476..0x28481 0x16D58 */
    camera_screen_base(1, (s32)DSB(DS_001078BE));       /* 0x2848D..0x28498 0x16D58 */
    DSD(DS_001077E8) = DSD(DS_001077E4);                /* 0x284A2/0x284AA */
    DSD(DS_0010787C) = DSD(DS_00107878);                /* 0x284B0/0x284BB */
    camera_project(0, DS_00100B08, DS_00100B00, DS_00100B62,
                   DS_00100B60, DS_00100AF0);            /* 0x2849D..0x284C6 0x17FA0 */
    camera_project(1, DS_00100B0C, DS_00100B04, DS_00100B63,
                   DS_00100B61, DS_00100AF4);            /* 0x284CB..0x284E9 0x17FA0 */
    camera_decay();                                      /* 0x284EE 0x17580 */
    fighter_pass_a();                                    /* 0x284F3 0x1958C */
    /* PORT: unlike mode 9 (0x28815..0x28825), no gate guards this call. */
    fight_slot_pass();                                   /* 0x284F8 0x3CB68 */
    fight_hud_pass(0u);                                  /* 0x284FD/0x284FF 0x35658 */
    fight_hud_pass(1u);                                  /* 0x28504/0x28509 0x35658 */
    fighter_pass_b(1u);                                  /* 0x2850E/0x28513 0x19068 */
    fight_effects_pass();                                /* 0x28518 0x49C78 */
    camera_y_commit();                                   /* 0x2851D 0x12DA8 */

    u16 hold = DSW(DS_00104AF8);                          /* 0x28522 */
    if (hold != 0u) {                                     /* 0x28529/0x2852C */
        hold = (u16)(hold - 1u);                          /* 0x2852E/0x28530 */
        DSW(DS_00104AF8) = hold;                           /* 0x28532 */
        if (hold == 0u) {                                   /* 0x28539 */
            DSB(DS_001078FE) = 1u;                            /* 0x2853B/0x2854F */
            DSB(DS_001078FC) = 1u;                             /* 0x28555 */
            DSB(DS_001077F1) = (u8)(DSB(DS_001077F1) | 0x10u); /* 0x2853D/0x28549/0x28563 */
            DSB(DS_00107885) = (u8)(DSB(DS_00107885) | 0x10u); /* 0x28543/0x2854C/0x2855B */
            DSB(DS_000F0AFE) = 4u;                              /* 0x28561/0x28569 */
        }
    }
    DSB(DS_00104AEC) = (u8)(DSB(DS_00104AEC) | 2u);        /* 0x2856F */

    if (DSB(DS_001078FE) == 0u) return;                     /* 0x28576..0x2857D */
    if (DSB(DS_000F0AFE) != 4u) return;                      /* 0x28583..0x2858E */

    fight_effects_hold_all();                                 /* 0x28594 0x4A708 */
    flow_match_result_text();                                 /* 0x28599 0x28130 */
    u32 mode_sel = DSB(DS_00104B1D);                          /* 0x285A8 */
    DSW(DS_00104AFE) = 0xF0u;                                  /* 0x285AD */
    DSW(DS_001088EE) = 0x3Cu;                                   /* 0x285B4 */
    if (mode_sel == 3u) {                                        /* 0x285BB/0x285BD */
        DSD(DS_00104AE4) = FN_00025BBC;                            /* 0x285CB */
        DSB(DS_00104B25) = 1u;                                       /* 0x285D1 */
        DSW(DS_00104B00) = 0x16u;                                     /* 0x285DC */
        DSW(DS_00104AFA) = 0x30u;                                      /* 0x285E3 */
        return;                                                         /* 0x285EA -> 0x28616 */
    }
    DSD(DS_00104AE4) = FN_00025BBC;                                       /* 0x285F3 */
    DSB(DS_00104B25) = 1u;                                                 /* 0x285F9 */
    DSW(DS_00104AFA) = 5u;                                                  /* 0x28609 */
    DSW(DS_00104B00) = 0x16u;                                                /* 0x2860F */
}

/* ---- mode 0xA, the frame handler 0x28BD4 (record §49-D) ------------------ */

/* 0x28BD4 — record §49-D. See flow.h for the full derivation. Not a results-
 * screen sibling of modes 8/9 despite sharing their listing: only the tail
 * render/commit slice those two share (position latches, fight_hud_pass(0)/
 * (1), fighter_pass_b(1), fight_effects_pass, camera_y_commit), then, once
 * both sides' slot +0x54 byte read zero, a voice and an unconditional mode
 * advance to 0xB. */
void game_mode_0a_step(void)
{
    DSD(DS_001077E8) = DSD(DS_001077E4);                /* 0x28BDA/0x28BDF */
    DSD(DS_0010787C) = DSD(DS_00107878);                /* 0x28BE4/0x28BE9 */
    fight_hud_pass(0u);                                  /* 0x28BE9/0x28BEB 0x35658 */
    fight_hud_pass(1u);                                  /* 0x28BF0/0x28BF5 0x35658 */
    fighter_pass_b(1u);                                  /* 0x28BFA/0x28BFF 0x19068 */
    fight_effects_pass();                                /* 0x28C04 0x49C78 */
    camera_y_commit();                                   /* 0x28C09 0x12DA8 */

    if (DSB(DS_00107804) == 0u && DSB(DS_00107898) == 0u) {  /* 0x28C0E..0x28C1E */
        /* PORT: 0x28C2A 0x2C3FC(0xD8, EDX = 0xB) voice, not wired (record
         * §45-A). EDX (0xB) survives 0x2C3FC unconditionally (record §42-E.2)
         * and becomes the mode below (0x28C2F, `DAT_00104b00 = extraout_DX`
         * in the decompiled C). */
        DSW(DS_00104B00) = 0xBu;                         /* 0x28C2F */
    }
}

/* ---- mode 7, the frame handler 0x282C4 (record §49-E) -------------------- */

/* 0x282C4 — record §49-E. Mode 7's frame handler (0x24C5C case 7, the table
 * entry unrecorded in this file's own comments before this task — confirmed
 * as case 7 the same way mode 0xA's port confirmed its case: `get_xrefs_to
 * 0x282C4` lists exactly one call site with no register setup before the
 * bare `CALL`, the shared bare-dispatch-stub shape every 0x24C5C case has;
 * body to 0x28413, its only caller). The tail slice modes 8/9/0xA share
 * (the two position latches, then, unlike those three, fight_slot_pass
 * unconditionally rather than fighter_pass_a's preamble first — 0x282C4 has
 * none of fight_slot_clear/camera_screen_base/camera_project/camera_decay/
 * fighter_pass_a — fight_hud_pass(0)/(1), fighter_pass_b(1), fight_effects_pass
 * and camera_y_commit).
 *   Then, on the round winner DS_00104B16 (0/1 a side, 2 a draw — the byte
 * flow_round_winner sets, flow.c above):
 *   - DS_00104B16 == 2 (a draw) and the match result DS_00104AD4 in [0, 2)
 *     (0/1, i.e. a *decided* match despite a drawn round): with the *other*
 *     side's (result ^ 1) slot +0x54 byte (DS_00107804/DS_00107898's stride,
 *     the same field mode 0xA's port and actors.c's anim-opcode 0xD500 clear
 *     read) == 3, arm DS_00104AF8 = 0x258 and advance to mode 9, OR the
 *     match result's `AEC |= 2; AD4 = result` tail either way (0x2836F..
 *     0x283B8);
 *   - DS_00104B16 == 2 and the match result out of [0, 2) (< 0 or >= 2, i.e.
 *     undecided/a draw): both sides' +0x54 bytes == 3 and equal to each
 *     other arms DS_00104AF8 = 0x258 and, only when the result is exactly 2
 *     (a draw), mode 9 (result > 2 or < 0 instead advances to mode 8) before
 *     the same tail (0x283B9..0x283F8);
 *   - DS_00104B16 != 2 (a side, 0 or 1): the byte at that side's own slot
 *     +0x52 (DS_00107802's stride — the byte fighter_attack_consume's
 *     "attack state" trio DS_00107802/03/04 writes, fighter.h) == 0 arms
 *     DS_00104AF8 = 0x258 and advances to mode 9 when the match result is in
 *     [0, 2] (0/1/2), else mode 8 (0x28339..0x283F9); the byte != 0 falls
 *     straight to the tail with no mode change.
 *   The tail (0x28402, reached whenever no branch above already returned):
 * DS_00104AEC |= 2, DS_00104AD4 = the match result (reloaded, 0x2832B/
 * 0x28361 — unchanged from the dword at function entry on every path, since
 * nothing in this function writes DS_00104AD4 except the two explicit
 * `= result` stores on the direct-return arms above, themselves writing back
 * the same value they read). The very first load, `MOV EBX,[0x104AD4]`
 * (0x282C8), before any of the above calls, is never read again before both
 * reload sites (0x2832B, 0x28361) overwrite it — a dead compiler-emitted
 * load, omitted. EBX/ECX/EDX/EDI are pushed and popped. */
void game_mode_07_step(void)
{
    DSD(DS_001077E8) = DSD(DS_001077E4);                /* 0x282CE/0x282D3 */
    DSD(DS_0010787C) = DSD(DS_00107878);                /* 0x282D8/0x282DD */
    fight_slot_pass();                                   /* 0x282E2 0x3CB68 */
    fight_hud_pass(0u);                                  /* 0x282E7/0x282E9 0x35658 */
    fight_hud_pass(1u);                                  /* 0x282EE/0x282F3 0x35658 */
    fighter_pass_b(1u);                                  /* 0x282F8/0x282FD 0x19068 */
    fight_effects_pass();                                /* 0x28302 0x49C78 */
    camera_y_commit();                                   /* 0x28307 0x12DA8 */

    u32 winner = DSB(DS_00104B16);                       /* 0x2830E */
    s32 result;

    if (winner != 2u) {
        u32 attack = DSB(DS_00107802 + winner * 0x94u);  /* 0x28318..0x28324 */
        result = (s32)DSD(DS_00104AD4);                   /* 0x2832B */
        if (attack == 0u) {                                /* 0x28331/0x28333 */
            DSW(DS_00104AF8) = 0x258u;                       /* 0x28339 */
            if (result >= 0 && result <= 2)                  /* 0x28342..0x2834D */
                DSW(DS_00104B00) = 9u;                          /* 0x28353 */
            else
                DSW(DS_00104B00) = 8u;                          /* 0x283F9 */
        }
    } else {
        result = (s32)DSD(DS_00104AD4);                     /* 0x28361 */
        if (result >= 0 && result < 2) {
            u32 other = (u32)result ^ 1u;                     /* 0x2836F/0x28371 */
            if (DSB(DS_00107804 + other * 0x94u) == 3u) {      /* 0x28382/0x2838A */
                DSW(DS_00104AF8) = 0x258u;                       /* 0x2839A */
                DSW(DS_00104B00) = 9u;                            /* 0x283A1 */
                DSB(DS_00104AEC) = (u8)(DSB(DS_00104AEC) | 2u);    /* 0x283A7 */
                DSD(DS_00104AD4) = (u32)result;                    /* 0x283AE */
                return;                                              /* 0x283B8 */
            }
        } else if (DSB(DS_00107804) == 3u &&
                   DSB(DS_00107898) == DSB(DS_00107804)) {   /* 0x283B9..0x283CA */
            DSW(DS_00104AF8) = 0x258u;                          /* 0x283CC */
            DSW(DS_00104B00) = (result == 2) ? 9u : 8u;          /* 0x283D5..0x283DE/0x283F9 */
            DSB(DS_00104AEC) = (u8)(DSB(DS_00104AEC) | 2u);       /* 0x283E7 */
            DSD(DS_00104AD4) = (u32)result;                        /* 0x283EE */
            return;                                                  /* 0x283F8 */
        }
    }

    DSB(DS_00104AEC) = (u8)(DSB(DS_00104AEC) | 2u);        /* 0x28402 */
    DSD(DS_00104AD4) = (u32)result;                         /* 0x28409 */
}

/* ---- mode 0xF, the frame handler 0x277C0 (record §49-F) ------------------ */

/* 0x277C0 — record §49-F. Mode 0xF's frame handler (0x24C5C case 0xF,
 * confirmed the same way mode 7's 0x282C4 was above via `get_xrefs_to
 * 0x277C0`'s single bare-`CALL` site; body to 0x278AE, its only caller). The
 * two position latches, then fight_hud_pass(0)/(1) — not fighter_pass_b,
 * unlike modes 7/8/9/0xA's shared tail — then the active effects list's
 * idle-pose walker fight_effects_idle_pass (0x4DEF4, record §49-F, fight.c
 * above) and camera_y_commit.
 *   DS_00104AEC |= 2 unconditionally; the word DS_00104AFE (no zero-guard,
 * unlike mode 8/9's DS_00104AF8 countdown) decrements by one every call; at
 * or below 0 (signed 16-bit, 0x27811 `test dx,dx`/`jg`) it fires:
 *   - voice 0x2B (0x27821 0x2C3FC, deferred, record §45-A) with DL = 0; since
 *     0x2C3FC preserves EDX unconditionally (record §42-E.2, the same
 *     precedent mode 0xA's port used), DS_00104B1B = DL afterward is
 *     unconditionally 0 (0x27826);
 *   - the established winner-side macro DS_0010810D's slot's +0x41 bit 4 is
 *     cleared (DS_001077F1 + w*0x94, the same field's bit 4 modes 8/9 OR in
 *     on their countdown-arm arms; 0x2782C..0x27843);
 *   - fighter_41310(DS_00104AD4, 0x30D40) — the *match result*, not
 *     DS_0010810D, is this call's side argument (0x2784B..0x27855);
 *   - the deferred 0x32B94 (0x2786B): tests DS_00104B1F bit 0 and, when set,
 *     posts one audit entry through 0x2DAE4(0xE, 1) — the deferred audit
 *     idiom, spec §7 — before an unconditional bare `ret`; no memory-visible
 *     effect either way (confirmed by disassembling 0x32B94 in full: 8
 *     instructions, the only call is the conditional 0x2DAE4, EDX pushed and
 *     popped around it), so this call site is a no-op here, the same
 *     treatment already given the four calls onto the neighbouring 0x32BAC
 *     bare-`ret` stub in mode 9's port;
 *   - the deferred run-clock tick 0x32970(0) (0x27874, out of scope, spec
 *     §7; the host clock owns wall time);
 *   - config_play_time_close(1, DS_00104B19) (0x27879..0x27886 0x32A3C);
 *   - DS_00104B25 = 0, DS_00104AFA = 0x1F, DS_00104B00 = 0x17 and
 *     DS_00104AE4 = frontend_darken_all (0x29B74) — the same hook mode 9's
 *     flow_results_darken_close installs (0x2788B..0x278A4).
 * EBX/ECX/EDX/ESI are pushed and popped. */
void game_mode_0f_step(void)
{
    DSD(DS_001077E8) = DSD(DS_001077E4);                /* 0x277C4/0x277C9 */
    DSD(DS_0010787C) = DSD(DS_00107878);                /* 0x277CE/0x277D3 */
    fight_hud_pass(0u);                                  /* 0x277D8/0x277DA 0x35658 */
    fight_hud_pass(1u);                                  /* 0x277DF/0x277E4 0x35658 */
    fight_effects_idle_pass();                           /* 0x277E9 0x4DEF4 */
    camera_y_commit();                                    /* 0x277EE 0x12DA8 */

    DSB(DS_00104AEC) = (u8)(DSB(DS_00104AEC) | 2u);        /* 0x277F3/0x27800/0x27804 */
    s16 hold = (s16)(DSW(DS_00104AFE) - 1u);                /* 0x277F9/0x27803 */
    DSW(DS_00104AFE) = (u16)hold;                            /* 0x2780A */
    if (hold > 0) return;                                     /* 0x27811/0x27814 */

    /* PORT: 0x27821 0x2C3FC(0x2B) voice, not wired (record §45-A); its DL
     * argument is 0 and 0x2C3FC preserves EDX (record §42-E.2), so the store
     * below is unconditionally 0. */
    DSB(DS_00104B1B) = 0u;                                    /* 0x27826 */
    {
        s32 w = (s32)(s8)DSB(DS_0010810D);                     /* 0x2782C/0x27832 */
        DSB(DS_001077F1 + (u32)w * 0x94u) =
            (u8)(DSB(DS_001077F1 + (u32)w * 0x94u) & 0xEFu);     /* 0x27835..0x27843 */
    }
    fighter_41310(DSD(DS_00104AD4), 0x30D40);                  /* 0x2784B..0x27855 0x41310 */
    /* PORT: 0x2786B 0x32B94: tests DS_00104B1F bit 0; when set, posts a
     * deferred audit entry through 0x2DAE4(0xE, 1) (out of scope, spec §7)
     * before an unconditional bare `ret`; no memory-visible effect either
     * way — omitted, the same treatment already given the 0x32BAC-stub call
     * sites in mode 9's port. */
    /* PORT: 0x27874 0x32970(EAX = 0), the run clock, is out of scope (spec
     * §7); the host clock owns wall time. */
    config_play_time_close(1u, DSB(DS_00104B19));               /* 0x27879..0x27886 0x32A3C */
    DSB(DS_00104B25) = 0u;                                       /* 0x27890 */
    DSW(DS_00104AFA) = 0x1Fu;                                    /* 0x27896 */
    DSW(DS_00104B00) = 0x17u;                                    /* 0x2789D */
    DSD(DS_00104AE4) = FN_00029B74;                               /* 0x278A4 */
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

/* 0x12658 — record §50-E. The state-3 handoff spawner (derived in
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

/* ---- the character select's side prompts 0x2C088..0x2C1D4 (record §48-S) -- */

#define DS_000BAB5A 0x000BAB5Au   /* no symbols.h name: [side] the prompt sprite's x */
#define DS_000BAB60 0x000BAB60u   /* no symbols.h name: the prompt sprite's descriptor */
#define DS_000809C4 0x000809C4u   /* no symbols.h name: the string "1" */

/* 0x2C088 — record §48-S. EAX = col, EDX = row, EBX = side. With the
 * DS_00104529 bit 1 (the sprite prompts): the side's sprite DS_00105BF0[side],
 * when set, is marked dead (0x2B150) and the slot zeroed (ECX = 0), then
 * string 0x48's cells are released at the position 0x2C1D4 last drew
 * (DS_00105C06/DS_00105C07, `movzx`/`mov dl` after `xor edx,edx`), not at the
 * caller's. Otherwise at the caller's col/row. The mode is 0x1000 (ECX);
 * 0x1C500 pushes and pops EBX/EDX. */
void prompt_press_start_clear(s32 col, s32 row, u32 side)
{
    if ((DSB(DS_00104529) & 2u) != 0u) {                /* 0x2C08E */
        u32 rec = DSD(DS_00105BF0 + side * 4u);         /* 0x2C09E */
        if (rec != 0u) {                                /* 0x2C0A4 */
            actor_set_dead(rec);                        /* 0x2C0AC 0x2B150 */
            DSD(DS_00105BF0 + side * 4u) = 0u;          /* 0x2C0B1 */
        }
        const u8 *s = game_string_get(0x48u);           /* 0x2C0C3 0x1C500 */
        text_cells_release((s32)DSB(DS_00105C06), (s32)DSB(DS_00105C07),
                           s, 0x1000u);                 /* 0x2C0C8..0x2C0EA 0x2F280 */
        return;
    }
    text_cells_release(col, row, game_string_get(0x48u), 0x1000u);  /* 0x2C0E1/0x2C0EA */
}

/* 0x2C0F4 — record §48-S. EAX = col (or x), EDX = row (or y), EBX = side,
 * ECX = `sprite`. On the blink phase DS_000EF6DC & 0x1F (the word, 0x2C0FC):
 * - 0: with `sprite`, the prompt sprite 0xBAB60 is spawned at (a2 = col,
 *   a3 = 0xFF, a4 = row, a5 = ECX, which is the phase, 0) into
 *   DS_00105BF0[side]; otherwise string 0x48 is drawn by 0x2F198 at col/row
 *   with mode 0x1000. Either way DS_00105BF8 = strlen(string 0x48)
 *   (0x2C149..0x2C15F);
 * - 0x18: 0x2C088(col, row, side);
 * - otherwise nothing. */
void prompt_press_start_blink(s32 col, s32 row, u32 side, u32 sprite)
{
    u32 phase = (u32)DSW(DS_000EF6DC) & 0x1Fu;         /* 0x2C0FC..0x2C108 */
    if (phase != 0u) {                                  /* 0x2C10E */
        if (phase == 0x18u)                             /* 0x2C168 */
            prompt_press_start_clear(col, row, side);   /* 0x2C16F 0x2C088 */
        return;
    }
    if (sprite != 0u) {                                 /* 0x2C110 */
        DSD(DS_00105BF0 + side * 4u) = actor_spawn(
            (const u32 *)(mem + DS_000BAB60), (u32)col, 0xFFu, (u32)row,
            phase);                                     /* 0x2C119..0x2C128 0x2AE14 */
    } else {
        text_cursor_set(col, row, game_string_get(0x48u), 0x1000u);  /* 0x2C131..0x2C144 */
    }
    DSD(DS_00105BF8) = (u32)strlen((const char *)game_string_get(0x48u));  /* 0x2C149..0x2C15F */
}

/* 0x2C178 — record §48-S. EAX = side, EDX = row (or y). With the DS_00104529
 * bit 1: 0x2C0F4 with the x word 0xBAB5A[side] (`mov esi,[eax*2+0xbab58];
 * sar esi,0x10`, signed) and ECX = 1; otherwise with the col byte
 * 0xBAB58[side] (`movzx`) and ECX = 0. EBX = side in both. */
void prompt_press_start(u32 side, s32 row)
{
    if ((DSB(DS_00104529) & 2u) != 0u)                  /* 0x2C17B */
        prompt_press_start_blink((s16)DSW(DS_000BAB5A + side * 2u), row,
                                 side, 1u);             /* 0x2C184..0x2C1A4 */
    else
        prompt_press_start_blink((s32)DSB(DS_000BAB58 + side), row,
                                 side, 0u);             /* 0x2C197..0x2C1A4 */
}

/* 0x2C1D4 — record §48-S. EAX = col, EDX = row. On the blink phase
 * DS_000EF6DC & 0x1F:
 * - 0: the 0x14-byte stack buffer takes string 0x49, then the image string
 *   at 0x809C4 ("1"), then string 0x4B (a strcpy and two strcats,
 *   0x2C1F3..0x2C275); 0x2F198 draws it at col/row with mode 0x3000, then
 *   DS_00105C06 = col, DS_00105BF8 = its length and DS_00105C07 = row (bytes
 *   and a dword, 0x2C286..0x2C2A3);
 * - 0x18 (0x2C1AD, the code just after 0x2C178's `ret`): 0x2F388 releases
 *   DS_00105BF8 cells at col/row;
 * - otherwise nothing.
 * PORT: the raw's buffer is [esp..esp+0x13], with the saved col and row at
 * [esp+0x14]/[esp+0x18], and the copies are unbounded. The port bounds them
 * to the 0x14 bytes. ENGLISH.TXT's "INSERT " + "1" + " COIN" is 13 characters,
 * so the bound is not reached. */
void prompt_insert_coin_blink(s32 col, s32 row)
{
    u32 phase = (u32)DSW(DS_000EF6DC) & 0x1Fu;         /* 0x2C1E2..0x2C1EC */
    if (phase != 0u) {                                  /* 0x2C1F1 */
        if (phase == 0x18u)                             /* 0x2C1AD */
            text_cells_release_count(col, row, (s32)DSD(DS_00105BF8));  /* 0x2C1B2..0x2C1BC 0x2F388 */
        return;
    }
    char buf[0x14];
    snprintf(buf, sizeof buf, "%s", (const char *)game_string_get(0x49u));  /* 0x2C1FA */
    size_t n = strlen(buf);
    snprintf(buf + n, sizeof buf - n, "%s", (const char *)(mem + DS_000809C4));  /* 0x2C21B */
    n = strlen(buf);
    snprintf(buf + n, sizeof buf - n, "%s", (const char *)game_string_get(0x4Bu));  /* 0x2C24B */
    text_cursor_set(col, row, (const u8 *)buf, 0x3000u);  /* 0x2C276..0x2C281 0x2F198 */
    DSB(DS_00105C06) = (u8)col;                         /* 0x2C294 */
    DSD(DS_00105BF8) = (u32)strlen(buf);                /* 0x2C286..0x2C28F, 0x2C29D */
    DSB(DS_00105C07) = (u8)row;                         /* 0x2C2A3 */
}

/* 0x2C1C8 — record §48-S. EAX = side, EDX = row: EAX = the col byte
 * 0xBAB58[side] (`and eax,0xff`), then a `nop` falls through into 0x2C1D4. */
void prompt_insert_coin(u32 side, s32 row)
{
    prompt_insert_coin_blink((s32)DSB(DS_000BAB58 + side), row);  /* 0x2C1C8..0x2C1D3 */
}

/* 0x2C2B0 — record §48-T. EAX = side (kept in ESI), EDX = row (kept in ECX);
 * EBX/ECX/ESI are pushed and popped. The erase of `side`'s prompts, both
 * forms. With the DS_00104529 bit 1 (the sprite prompts): 0x2C088 with EAX =
 * the col byte 0xBAB58[side] (`xor eax,eax; mov al`), EDX = row and EBX =
 * side, then 0x2F388 releases DS_00105BF8 cells at DS_00105C06/DS_00105C07
 * (`xor`, then `mov al`/`mov dl`), the last "INSERT 1 COIN" position. Then,
 * either way, 0x2F388 releases DS_00105BF8 cells at the col byte and `row`.
 * 0x2C088 pushes and pops ECX/ESI, so the second call's EDX (`mov edx,ecx`)
 * and col are the caller's; DS_00105BF8 is reloaded before each 0x2F388.
 * The side is used whole in `[esi+0xBAB58]` and 0x2C088's `[eax*4+0x105BF0]`,
 * so a negative one reads below both tables. Callers: 0x275B7 (0x274FC, side
 * (s8)(DS_0010810D ^ 1) by `movsx`, row 0x1D), 0x28CFD (0x28CC8, record
 * §48-J), 0x27E52/0x27E61 (0x27DC8, sides 0 and 1, row 0x1D, record §48-K)
 * and the unported 0x26D4C and 0x42FE0. */
void prompt_side_erase(s32 side, s32 row)
{
    if ((DSB(DS_00104529) & 2u) != 0u) {                /* 0x2C2B7/0x2C2BE */
        prompt_press_start_clear((s32)DSB(DS_000BAB58 + (u32)side), row,
                                 (u32)side);            /* 0x2C2C0..0x2C2CA 0x2C088 */
        text_cells_release_count((s32)DSB(DS_00105C06), (s32)DSB(DS_00105C07),
                                 (s32)DSD(DS_00105BF8));    /* 0x2C2CF..0x2C2E4 0x2F388 */
    }
    text_cells_release_count((s32)DSB(DS_000BAB58 + (u32)side), row,
                             (s32)DSD(DS_00105BF8));    /* 0x2C2E9..0x2C2F9 0x2F388 */
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
 * callers are 0x11A30 (still unwired) and three in game_mode_1e_step's
 * states 2, 6 and 9 (0x1F140/0x1F278/0x1F39B, record §49-H, below), now
 * wired. */
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

/* ---- mode 0x1E, the frame handler 0x1EEB0 (record §49-H) ----------------- */

#define FN_000430E8 0x000430E8u   /* no symbols.h name: the versus-screen hook,
                                      fight.c's fight_hook_430e8 (record §46-B) */

/* 0x1EC38 — record §49-R. EAX = score (DS_001077EC or DS_00107880). Ranks it
 * against table 0 through hiscore_rank_probe (0x2DDE4, table = 0 both
 * `xor edx,edx` at 0x1EC3D/0x1EC50's own callers). A rank >= 10 (0x1EC75/
 * 0x1EC78, the low 16 bits of the probe's return, 0xFFFFFFFF included)
 * returns 0. A rank < 10 clears config field 0x26 (0x1ECB1/0x1ECB3
 * config_field_set — the real, unconditional store, not the deferred audit
 * add below), records the rank into DS_001044D6 (0x1ECBA) and returns 1.
 * PORT: 0x1EC44..0x1EC6B and 0x1EC7A..0x1EC9A each post one deferred
 * 0x2DAE4 audit add (field 0x27 always, clamped at 2000 through
 * config_field_set when its running total exceeds it; field 0x26 only on
 * the non-qualifying path, clamped at 200) — 0x2DAE4 is the deferred audit
 * idiom this codebase already treats as out of scope everywhere else
 * (spec §7); neither add changes this function's own return value or
 * DS_001044D6, so omitting them changes no state this port's own callers
 * observe. A rank < 10 first arms the name-entry screen through
 * nameentry_arm (0x204F4, record §51-A) with EAX = the rank's low word
 * (0x1EC70/0x1EC72 `xor eax,eax; mov ax,cx`) and EDX = the score
 * (0x1ECA5); ECX (the rank) survives it (0x204F5 push/0x2070A pop). */
u32 hiscore_rank_single(u32 score)
{
    u32 rank = hiscore_rank_probe(score, 0u);                  /* 0x1EC3F */
    /* PORT: 0x1EC44..0x1EC6B — field 0x27's deferred audit add; see header. */
    if ((u16)rank >= 10u) {                                    /* 0x1EC75/0x1EC78 */
        /* PORT: 0x1EC7A..0x1EC9A — field 0x26's deferred audit add; see
         * header. */
        return 0u;                                             /* 0x1EC9F */
    }
    nameentry_arm((u16)rank, score);                            /* 0x1ECA7 0x204F4 */
    (void)config_field_set(0x26u, 0u);                          /* 0x1ECB3 0x2DA0C */
    DSW(DS_001044D6) = (u16)rank;                               /* 0x1ECBA */
    return 1u;                                                  /* 0x1ECB8 */
}

/* 0x1ECC8 — record §49-R. Ranks both fighters' post-match scores
 * (DS_001077EC, DS_00107880 — the per-side records' own `+8` field) against
 * table 0 through hiscore_rank_probe, recording each rank into
 * DS_001044C0/DS_001044C2 for game_mode_1e_step's own state 0 to read back
 * (0x1ECD5/0x1ECFC). Side 0's rank >= 10 bails immediately (0x1ECE0/
 * 0x1ECE3/0x1ECE5). Otherwise side 1 is probed; the one pathological tie —
 * both sides land on rank 9, the table's last slot, which only one of them
 * can actually take — bails too (0x1ED02/0x1ED05/0x1ED0C/0x1ED0F/0x1ED11);
 * any other tie is not special-cased (this codebase transcribes the raw
 * exactly here, not a plausible-looking simplification). Otherwise side
 * 1's rank >= 10 bails (0x1ED1D/0x1ED20/0x1ED22); else returns 1
 * (0x1ED26). */
u32 hiscore_rank_pair(void)
{
    u32 r0 = hiscore_rank_probe(DSD(DS_001077EC), 0u);          /* 0x1ECD0 */
    DSW(DS_001044C0) = (u16)r0;                                  /* 0x1ECD5 */
    if ((u16)r0 >= 10u) return 0u;                               /* 0x1ECE0/0x1ECE3 */
    u32 r1 = hiscore_rank_probe(DSD(DS_00107880), 0u);           /* 0x1ECF0 */
    DSW(DS_001044C2) = (u16)r1;                                  /* 0x1ECFC */
    if ((u16)r1 == (u16)r0 && (u16)r0 == 9u) return 0u;          /* 0x1ED02/0x1ED05/0x1ED0C/0x1ED0F/0x1ED11 */
    if ((u16)r1 >= 10u) return 0u;                                /* 0x1ED1D/0x1ED20 */
    return 1u;                                                    /* 0x1ED26 */
}

/* 0x1EEB0 — record §49-H, states 0/4/7's real gate record §49-R, states
 * 5/8/0xB..0xE record §49-T. See flow.h for the full derivation: a 17-state
 * sub-machine on DS_00104B25, entered from mode 0x13's challenge poll with
 * DS_00104B25 = 0 when the post-match challenge window runs out unjoined.
 * States 2/3/6/9/0xA are pure bookkeeping and fully ported; states 0/4/7 run
 * their short-circuit-then-rank-probe gate for real (hiscore_rank_pair/
 * hiscore_rank_single above, record §49-R); states 5/8/0xB..0xE poll the
 * name-entry driver nameentry_step (0x1F458, record §49-T) and advance when
 * it reports done. Record §51-A wires the rest: the 0x1ED2C screen reset
 * (nameentry_reset) at states 0/4/7's arming points, 0x204F4 inside 0x1EC38,
 * and states 0xF/0x10, which re-arm the other side's entry. */
void game_mode_1e_step(void)
{
    switch (DSB(DS_00104B25)) {
    case 0x00u:
        if (DSD(DS_00104AD4) == 2u && DSB(DS_00104B1F) == 0u    /* 0x1EECF/0x1EED6/0x1EED8/0x1EEDF */
            && hiscore_rank_pair() != 0u) {                     /* 0x1EEE1/0x1EEE6/0x1EEE8 */
            nameentry_reset();                                  /* 0x1EEEA 0x1ED2C */
            /* PORT: 0x1EEEF/0x1EEF9 0x2C3FC voices (0x100, 0xE1), not
             * wired (record §45-A). */
            u32 s0 = DSD(DS_001077EC);                          /* 0x1EF03 */
            u32 s1 = DSD(DS_00107880);                          /* 0x1EF08 */
            if (s0 >= s1) {                                     /* 0x1EF0E/0x1EF10 (unsigned) */
                DSB(DS_00104B25) = 0x0Bu;                       /* 0x1EF12 */
                (void)hiscore_rank_single(s0);                  /* 0x1EF19 */
            } else {
                DSB(DS_00104B25) = 0x0Cu;                       /* 0x1EF27 */
                (void)hiscore_rank_single(s1);                  /* 0x1EF2D */
            }
            break;
        }
        DSB(DS_00104B25) = 4u;                                  /* 0x1EF37 */
        break;
    case 0x01u:
        /* 0x1EE6C's state-1 jump-table entry is the shared tail `ret`
         * itself (0x1F452): a genuine no-op, not an unported gap. */
        break;
    case 0x02u:
        frontend_input_reset();               /* 0x1F12F 0x4F1E4 */
        actors_reset();                        /* 0x1F13B 0x2BAF4 (eax=1) */
        frontend_match_start();                /* 0x1F140 0x1EA08 */
        DSW(DS_00104AFE) = 0x12Cu;             /* 0x1F145 */
        DSW(DS_00104B00) = 0x15u;              /* 0x1F14C */
        DSW(DS_00104AFA) = 0x1Eu;              /* 0x1F153 */
        DSB(DS_00104B25) = 3u;                 /* 0x1F15C */
        DSW(DS_001088EE) = 0u;                 /* 0x1F162 */
        break;
    case 0x03u:
        frontend_input_reset();               /* 0x1F175 0x4F1E4 */
        actors_reset();                        /* 0x1F181 0x2BAF4 (eax=1) */
        DSB(DS_00104B25) = 0u;                 /* 0x1F186 */
        DSW(DS_00104B00) = 0x14u;              /* 0x1F18C */
        break;
    case 0x04u:
        if (DSB(DS_00107813) != 0u) {                            /* 0x1F199/0x1F1A0 */
            DSB(DS_00104B25) = 7u;                                /* 0x1F2AA */
            break;
        }
        if (hiscore_rank_single(DSD(DS_001077EC)) != 0u) {         /* 0x1F1A6/0x1F1AB/0x1F1B0/0x1F1B2 */
            u32 advance = (DSD(DS_00104AD4) != 0u)                  /* 0x1F1B8/0x1F1BF */
                       || (DSB(DS_00104B14) != 0u && DSD(DS_00104ABC) == 1u); /* 0x1F1C1/0x1F1C8/0x1F1CE/0x1F1D5 */
            if (advance) {
                DSB(DS_00104B25) = 5u;                              /* 0x1F1DB */
                nameentry_reset();                                  /* 0x1F1E2 0x1ED2C */
                /* PORT: 0x1F1EC/0x1F1F6 0x2C3FC voices (0x100, 0xE1), not
                 * wired (record §45-A). */
                break;
            }
        }
        DSB(DS_00104B25) = 7u;                                     /* 0x1F2AA */
        break;
    case 0x05u:
        if (nameentry_step(0u) == 0u) {                          /* 0x1F203 0x1F458(0) */
            DSW(DS_00104AFE) = 0x8Cu;                           /* 0x1F221 */
            DSW(DS_001088EE) = 0u;                              /* 0x1F227 */
            DSW(DS_00104B00) = 0x15u;                           /* 0x1F230 */
            DSB(DS_00104B25) = 6u;                              /* 0x1F237 */
            DSW(DS_00104AFA) = 0x1Eu;                           /* 0x1F242 */
            /* PORT: 0x1F249 0x2C3FC(0xE3) and 0x1F253 0x2C3FC(0xE2) voices,
             * not wired (record §45-A). */
        }
        break;
    case 0x06u:
        frontend_input_reset();                                 /* 0x1F260 0x4F1E4 */
        actors_reset();                                          /* 0x1F26A 0x2BAF4 (eax=1) */
        if (DSD(DS_00104ABC) == 1u) {          /* 0x1F26F/0x1F276 */
            frontend_match_start();            /* 0x1F278 0x1EA08 */
            DSW(DS_00104AFE) = 0x12Cu;         /* 0x1F28E */
            DSW(DS_001088EE) = 0u;             /* 0x1F295 */
            DSW(DS_00104B00) = 0x15u;          /* 0x1F29C */
            DSW(DS_00104AFA) = 0x1Eu;          /* 0x1F2A3 */
        }
        DSB(DS_00104B25) = 7u;                 /* 0x1F2AA, both arms */
        break;
    case 0x07u:
        if (DSB(DS_001078A7) != 0u) {                            /* 0x1F2B7/0x1F2BE */
            DSB(DS_00104B25) = 0xAu;                              /* 0x1F3CC */
            break;
        }
        if (hiscore_rank_single(DSD(DS_00107880)) != 0u) {          /* 0x1F2C4/0x1F2C9/0x1F2CE/0x1F2D0 */
            u32 advance = (DSD(DS_00104AD4) != 1u)                   /* 0x1F2D6/0x1F2DC/0x1F2DF */
                       || (DSB(DS_00104B14) != 0u && DSD(DS_00104ABC) == 1u); /* 0x1F2E1/0x1F2E8/0x1F2EE/0x1F2F4 */
            if (advance) {
                DSB(DS_00104B25) = 8u;                               /* 0x1F301 */
                /* PORT: 0x1F307/0x1F311 0x2C3FC voices (0x100, 0xE1), not
                 * wired (record §45-A). */
                nameentry_reset();                                   /* 0x1F316 0x1ED2C */
                break;
            }
        }
        DSB(DS_00104B25) = 0xAu;                                     /* 0x1F3CC */
        break;
    case 0x08u:
        if (nameentry_step(1u) == 0u) {                          /* 0x1F326 0x1F458(1) */
            DSB(DS_00104B25) = 9u;                              /* 0x1F341 */
            DSW(DS_00104AFE) = 0x8Cu;                           /* 0x1F347 */
            DSW(DS_001088EE) = 0u;                              /* 0x1F34E */
            DSW(DS_00104B00) = 0x15u;                           /* 0x1F355 */
            DSW(DS_00104AFA) = 0x1Eu;                           /* 0x1F365 */
            /* PORT: 0x1F36C 0x2C3FC(0xE3) and 0x1F376 0x2C3FC(0xE2) voices,
             * not wired (record §45-A). */
        }
        break;
    case 0x09u:
        frontend_input_reset();               /* 0x1F383 0x4F1E4 */
        actors_reset();                        /* 0x1F38D 0x2BAF4 (eax=1) */
        if (DSD(DS_00104ABC) == 1u) {          /* 0x1F392/0x1F399 */
            frontend_match_start();            /* 0x1F39B 0x1EA08 */
            DSW(DS_00104AFE) = 0x12Cu;         /* 0x1F3B1 */
            DSW(DS_001088EE) = 0u;             /* 0x1F3B7 */
            DSW(DS_00104B00) = 0x15u;          /* 0x1F3BE */
            DSW(DS_00104AFA) = 0x1Eu;          /* 0x1F3C5 */
        }
        DSB(DS_00104B25) = 0xAu;               /* 0x1F3CC, both arms */
        break;
    case 0x0Au: {
        frontend_input_reset();                /* 0x1F3DB 0x4F1E4 */
        /* actors_reset leaves DS_00104B25 = 0: CH is zeroed at 0x1F3E5 and
         * survives the call, since 0x2BAF4 pushes ECX at entry and pops it
         * unmodified just before its own `ret` (Ghidra's own extraout_CH_00
         * for this store is a conservative decompiler artifact, not a real
         * clobber). */
        actors_reset();                        /* 0x1F3E7 0x2BAF4 (eax=1) */
        DSB(DS_00104B25) = 0u;                 /* 0x1F3F1 */
        if (DSB(DS_00104B1F) == 0u) {          /* 0x1F3EC/0x1F3F7/0x1F3F9 */
            u32 r = DSD(DS_00104AD4);          /* 0x1F3FB */
            if (r == 2u) {                     /* 0x1F401/0x1F404 */
                DSW(DS_00104B00) = 0x14u;      /* 0x1F42E */
                break;
            }
            if (DSB(DS_00107813 + r * 0x94u) == 1u) {   /* 0x1F406..0x1F423 */
                DSW(DS_00104B00) = 0x14u;      /* 0x1F42E */
                break;
            }
        }
        if (DSB(DS_00104B14) == 0u) {          /* 0x1F425/0x1F42C */
            DSD(DS_00104AE4) = FN_000430E8;    /* 0x1F447 */
            frontend_wipe_arm(0x11u);          /* 0x1F44D 0x4F980 */
        } else {
            DSW(DS_00104B00) = 0x14u;          /* 0x1F42E */
        }
        break;
    }
    case 0x0Bu:
    case 0x0Cu:
    case 0x0Du:
    case 0x0Eu: {
        /* 0x1EF44/0x1F034/0x1F0C1/0x1EFD3 poll 0x1F458 with the side 0/1/0/1. */
        static const u8 side_of[4] = { 0u, 1u, 0u, 1u };
        static const u8 next_of[4] = { 0x0Fu, 0x10u, 2u, 2u };
        u32 k = (u32)(DSB(DS_00104B25) - 0x0Bu);
        if (nameentry_step(side_of[k]) == 0u) {                  /* 0x1EF46/0x1F039/0x1F0C3/0x1EFD8 */
            DSW(DS_00104AFE) = 0x8Cu;                           /* 0x1EF6C/0x1F059/0x1F0E1/0x1EFFB */
            DSW(DS_001088EE) = 0u;                              /* 0x1EF73/0x1F060/0x1F0E8/0x1F002 */
            DSW(DS_00104B00) = 0x15u;                           /* 0x1EF86/0x1F067/0x1F0EE/0x1F009 */
            DSW(DS_00104AFA) = 0x1Eu;                           /* 0x1EF7F/0x1F06E/0x1F0F5/0x1F012 */
            DSB(DS_00104B25) = next_of[k];                      /* 0x1EF66/0x1F079/0x1F103/0x1F019 */
            /* PORT: 0x1EF8D/0x1F07F/0x1F109/0x1F01F 0x2C3FC(0xE3) and
             * 0x1EF97/0x1F089/0x1F10E/0x1F024 0x2C3FC(0xE2) voices, not
             * wired (record §45-A). */
        }
        break;
    }
    case 0x0Fu:
        /* Side 0 is done (state 0xB); arm side 1's entry (state 0xE). The
         * rank probe's own return is discarded (no `test al,al`), so the
         * entry runs even for a non-qualifying score. */
        /* PORT: 0x1EFA9 0x2C3FC(0xE1) voice, not wired (record §45-A); DL =
         * 0xE survives it (0x2C3FD push edx, popped on every exit). */
        DSB(DS_00104B25) = 0x0Eu;                               /* 0x1EFA7/0x1EFAE */
        nameentry_reset();                                      /* 0x1EFB4 0x1ED2C */
        (void)hiscore_rank_single(DSD(DS_00107880));            /* 0x1EFB9/0x1EFBE 0x1EC38 */
        (void)nameentry_step(1u);                               /* 0x1EFC3/0x1EFC8 0x1F458 */
        break;
    case 0x10u:
        /* Side 1 is done (state 0xC); arm side 0's entry (state 0xD). */
        /* PORT: 0x1F099 0x2C3FC(0xE1) voice, not wired (record §45-A). */
        DSB(DS_00104B25) = 0x0Du;                               /* 0x1F09E */
        nameentry_reset();                                      /* 0x1F0A5 0x1ED2C */
        (void)hiscore_rank_single(DSD(DS_001077EC));            /* 0x1F0AA/0x1F0AF 0x1EC38 */
        (void)nameentry_step(0u);                               /* 0x1F0B4/0x1F0B6 0x1F458 */
        break;
    }
}

/* ---- mode 0x1F, the frame handler 0x208F8 (record §49-J) ----------------- */

/* 0x208F8 — record §49-J. The mode 0x1F handler (0x24C5C case 0x1F, jump
 * table 0x24B8C entry; sole caller, per get_xrefs_to). No ported case stores
 * mode 0x1F today — it is reachable only from the still-unported match-end
 * path — but the raw table 0x24B8C dispatches here regardless, so the port
 * wires it the same as every other case.
 *
 * A 5-state sub-machine on DS_00104B25 (jump table 0x208E4; DL > 4 is a
 * no-op, 0x20927/0x20c00), gated by the per-side character id `charid`
 * (DS_0010782A[DS_00104AD4 * 0x94], read unconditionally at 0x20900..0x20921
 * regardless of state, as the decompilation's top-of-function `uVar1`
 * shows). This is the "roar" screen between the post-match challenge poll
 * and mode 0x1E's high-score flow: state 0 resets the screen and spawns a
 * fixed backdrop row (0x38B18(0xA7B80)) plus one effect (0x13C70) when
 * 0x33904's walk of the palette-acquire table (DS_00107618..DS_00107798,
 * the same table palette_acquire itself fills — record §47-C, §49-J.3)
 * finds any entry, which the backdrop spawn's own real handle typically
 * ensures; state 1 spawns two actors, the
 * fixed 0xA7ED8 descriptor and a character-indexed one (0xA80AC when the
 * sprite flag DS_00104529 bit 1 is set, else 0xA8090), both frame_bits =
 * (the first actor's own +0x56 word) | 0x400; state 2 waits for the first
 * actor's play position (+0x34 SAR 16, +0x1C) to reach 0x1E00, then locks it
 * there, blanks the second actor's +0x36 and hands off to mode 0x15 (a
 * 0x4B0-frame wait, or skip) with return mode 0x1F itself (DS_00104AFA); on
 * mode 0x15's return (state 3) a third, character-indexed actor (0xA80C8)
 * is spawned, the first actor is set to a fade-out anim (+0x36 = 0xFF00,
 * stream 0xE9206) and the second to its own character-indexed stream
 * (0xA7EBC), both at hold 1.0; state 4 counts the first actor's +0x2C down
 * by 0x80 a frame at a time, killing both actors (0x2B150) the frame it
 * hits exactly 0, while independently counting the third actor's +0x2C up
 * by 0x40 a frame at a time until it clamps at 0x1000 — at which point
 * either a character-indexed actor (0xA818C, bit 1 set) or a
 * character-indexed string (0xA80E4, bit 1 clear, drawn at col -1, row
 * 0x1A, mode 0x2000 through text_cursor_set/game_string_get) is emitted,
 * then mode 0x15 is armed again (0x384 frames) with return mode 0x1E — the
 * post-match challenge/high-score flow (record §49-H) — and DS_00104B25 is
 * reset to 0 for the next time mode 0x1F runs. */
void game_mode_1f_step(void)
{
    u32 charid = DSB(DS_0010782A + DSD(DS_00104AD4) * 0x94u);  /* 0x20900..0x20921 */

    switch (DSB(DS_00104B25)) {                                 /* 0x2091b/0x20931 */
    case 0x00u:
        frontend_input_reset();                                 /* 0x2093b 0x4F1E4 */
        actors_reset();                                         /* 0x20949 0x2BAF4 (eax=1) */
        /* PORT: 0x20955 0x2C3FC(0x3B, edx=0) voice, not wired (record
         * §45-A; spec §7). */
        frontend_spawn_row((const u32 *)(mem + 0xA7B80u), 0u, 0u); /* 0x2095f 0x38B18 */
        {
            u32 e = frontend_list_next(0u);                     /* 0x20966 0x33904 */
            if (e != 0u)
                (void)effects_spawn(e, 2u, 0x3E688u);           /* 0x20979 0x13C70 */
        }
        DSB(DS_00104B25) = (u8)(DSB(DS_00104B25) + 1u);         /* 0x2097e */
        break;
    case 0x01u: {
        u32 rec1 = actor_spawn((const u32 *)(mem + 0xA7ED8u),
                                0x2A00u, 0xF0u, 0x5A00u, 0u);   /* 0x2099e..0x209a3 0x2AE14 */
        DSD(DS_001044A0) = rec1;                                /* 0x209ae */
        u32 frame_bits = (u32)(u16)(DSW(rec1 + 0x56u) | 0x0400u);  /* 0x209b8/0x209dd..0x209e4 */
        u32 desc = (DSB(DS_00104529) & 2u) != 0u                /* 0x209b3/0x209b6 */
                       ? DSD(0xA80ACu + charid * 4u)             /* 0x209d4 */
                       : DSD(0xA8090u + charid * 4u);            /* 0x209f9 */
        u32 rec2 = actor_spawn((const u32 *)(mem + desc),
                                0u, 0xF2u, 0u, frame_bits);      /* 0x20a00 0x2AE14 */
        DSD(DS_001044A4) = rec2;                                /* 0x20a05 */
        DSW(rec1 + 0x36u) = 0xFFC0u;                             /* 0x20a17 */
        DSB(DS_00104B25) = (u8)(DSB(DS_00104B25) + 1u);         /* 0x20a0a/0x20a1d */
        break;
    }
    case 0x02u: {
        u32 rec1 = DSD(DS_001044A0);                             /* 0x20a2c */
        s32 pos = (s32)DSD(rec1 + 0x34u) >> 16;                  /* 0x20a31/0x20a37 */
        s32 frame = (s32)DSD(rec1 + 0x1Cu);                      /* 0x20a34 */
        if (pos + frame > 0x1E00) break;                         /* 0x20a3c/0x20a42 */
        DSD(rec1 + 0x1Cu) = 0x1E00u;                             /* 0x20a5c */
        DSW(rec1 + 0x36u) = 0u;                                  /* 0x20a65 */
        DSW(DS_00104AFE) = 0x4B0u;                               /* 0x20a6b */
        DSW(DS_001088EE) = 0x3Cu;                                /* 0x20a72 */
        DSW(DS_00104B00) = 0x15u;                                /* 0x20a79 */
        DSW(DS_00104AFA) = 0x1Fu;                                /* 0x20a80 */
        DSB(DS_00104B25) = (u8)(DSB(DS_00104B25) + 1u);          /* 0x20a63/0x20a87 */
        break;
    }
    case 0x03u: {
        u32 rec3 = actor_spawn((const u32 *)(mem + DSD(0xA80C8u + charid * 4u)),
                                0x2A00u, 0xF8u, 0x1A00u, 0u);   /* 0x20a9d..0x20ab3 0x2AE14 */
        DSD(DS_0010449C) = rec3;                                 /* 0x20ac3 */
        DSB(DS_00104B25) = (u8)(DSB(DS_00104B25) + 1u);          /* 0x20abd/0x20acf */
        u32 rec1 = DSD(DS_001044A0);                             /* 0x20aca */
        DSW(rec1 + 0x36u) = 0xFF00u;                             /* 0x20ada */
        actors_anim_begin(rec1, 0xE9206u, 0x3F800000u);          /* 0x20ae0 0x2BC30 */
        u32 rec2 = DSD(DS_001044A4);                             /* 0x20ae5 */
        actors_anim_begin(rec2, DSD(0xA7EBCu + charid * 4u),
                           0x3F800000u);                         /* 0x20af6 0x2BC30 */
        break;
    }
    case 0x04u: {
        u32 rec1 = DSD(DS_001044A0);                             /* 0x20b04 */
        if (rec1 != 0u) {                                        /* 0x20b0a/0x20b0c */
            u16 pos = (u16)(DSW(rec1 + 0x2Cu) - 0x80u);          /* 0x20b10..0x20b1a */
            DSW(rec1 + 0x2Cu) = pos;
            if (pos == 0u) {                                     /* 0x20b1e/0x20b21 */
                actor_set_dead(rec1);                             /* 0x20b25 0x2B150 */
                DSD(DS_001044A0) = 0u;                            /* 0x20b2a */
                u32 rec2 = DSD(DS_001044A4);                      /* 0x20b30 */
                if (rec2 != 0u) {                                 /* 0x20b36/0x20b38 */
                    actor_set_dead(rec2);                          /* 0x20b3e 0x2B150 */
                    DSD(DS_001044A4) = 0u;                        /* 0x20b43 */
                }
            }
        }
        u32 rec3 = DSD(DS_0010449C);                              /* 0x20b49 */
        u16 alpha = (u16)(DSW(rec3 + 0x2Cu) + 0x40u);            /* 0x20b4e..0x20b57 */
        DSW(rec3 + 0x2Cu) = alpha;                                /* 0x20b5a */
        if (alpha < 0x1000u) break;                               /* 0x20b5e/0x20b64 */
        DSW(rec3 + 0x2Cu) = 0x1000u;                              /* 0x20b6a */
        if ((DSB(DS_00104529) & 2u) != 0u) {                      /* 0x20b70/0x20b77 */
            (void)actor_spawn((const u32 *)(mem + DSD(0xA818Cu + charid * 4u)),
                               DSW(DS_000C8718), 0xFFu, 0x3400u, 0u);  /* 0x20b94..0x20b9b 0x2AE14 */
        } else {
            text_cursor_set(-1, 0x1A,
                             game_string_get(DSD(0xA80E4u + charid * 4u)),
                             0x2000u);                            /* 0x20bac..0x20bc4 0x1C500, 0x2F198 */
        }
        DSW(DS_00104AFE) = 0x384u;                                /* 0x20bdd */
        DSW(DS_001088EE) = 0x3Cu;                                 /* 0x20be4 */
        DSW(DS_00104AFA) = 0x1Eu;                                 /* 0x20beb */
        DSW(DS_00104B00) = 0x15u;                                 /* 0x20bf4 */
        DSB(DS_00104B25) = 0u;                                    /* 0x20bfa */
        break;
    }
    }
}

/* ---- modes 0x22/0x23/0x24, the fight-transition frames -------------------- */

/* 0x26D4C — record §49-L. Called unconditionally at the end of mode 0x22's
 * frame (0x26C8C, its sole caller per get_xrefs_to). Skips its whole body
 * when DS_00104AC4 > 0 (signed), falling straight to the epilogue (there is
 * no further camera_y_commit here — 0x26C8C already ran it before this
 * call). Otherwise:
 * - three text_cells_release_count(-1, 3, 0x2A), (-1, 5, 0x2A), (-1, 7,
 *   0x2A) clears (0x2F388);
 * - the run clock 0x32970(0) and two deferred voices 0x2C3FC(0x29) and
 *   0x2C3FC(0x22, edx=0x1D), out of scope (spec §7);
 * - prompt_side_erase(0, 0x1D) and prompt_side_erase(1, 0x1D) (0x2C2B0);
 * - the current side's slot byte DS_001077F1[side] |= 0x10 (the slot's
 *   +0x41, as flow.c's other +0x41 writers);
 * - DS_00104AFE = 0xB4, DS_00104AEC &= ~0x10, DS_00104B00 = 0x24;
 * - text_cursor_hold_font2(-1, 7, game_string_get(0x3C), 0x4000)
 *   unconditionally;
 * - pct = DS_0010780B[side] * 100 / 0x78 (signed IDIV; DS_0010780B[side] is
 *   the per-side byte 0x1DAE8/0x1DA84 also touch, record §49-M);
 * - with DS_00104529 bit 1 set: two background actors, 0xA8970 at (0x1400,
 *   0xFF, 0x1800, 0) and 0xA8984 at (0x3A00, 0xFF, 0x1800, 0), then
 *   text_number_set(0xF, 0xB, pct, 3, 1, 0x4003) (0x2F434), and the string
 *   path below is skipped;
 * - with the bit clear: game_string_get(0x3D), sprintf(buf, "%s%.03d%%",
 *   that string, pct) (0x65546, format read at data 0x809AC: "%s%.03d%", a
 *   bare trailing '%' the port writes as the equivalent "%s%03d%%"; the
 *   0x65546 formatter itself is not ported, as game_overlay_step's 0x2BF08
 *   already established for this codebase), then the raw's unbounded
 *   REPNE-SCASB manual strcat appends game_string_get(0x3E) to it (the port
 *   bounds it, as prompt_insert_coin_blink's 0x2C1FA already established),
 *   and text_cursor_set(-1, 3, buf, 0x2000) draws it.
 * EBX..EDI are pushed and popped. */
static void flow_26d4c(void)
{
    if ((s32)DSD(DS_00104AC4) > 0) return;                  /* 0x26D54/0x26D5B */

    text_cells_release_count(-1, 3, 0x2A);                  /* 0x26D70 0x2F388 */
    text_cells_release_count(-1, 5, 0x2A);                  /* 0x26D84 */
    text_cells_release_count(-1, 7, 0x2A);                  /* 0x26D98 */
    /* PORT: 0x26D9F 0x32970(0), the run clock, is out of scope (spec §7). */
    /* PORT: 0x26DA9 0x2C3FC(0x29) and 0x26DB8 0x2C3FC(0x22, edx=0x1D)
     * voices, not wired (record §45-A; spec §7). */
    prompt_side_erase(0, 0x1D);                             /* 0x26DBF 0x2C2B0 */
    prompt_side_erase(1, 0x1D);                             /* 0x26DCE */

    u32 side = DSB(DS_00104B1A);                            /* 0x26DD5 */
    DSB(DS_001077F1 + side * 0x94u) =
        (u8)(DSB(DS_001077F1 + side * 0x94u) | 0x10u);      /* 0x26DE9/0x26DF8 */
    DSW(DS_00104AFE) = 0xB4u;                               /* 0x26E0D */
    DSB(DS_00104AEC) = (u8)(DSB(DS_00104AEC) & 0xEFu);      /* 0x26E14 */
    DSW(DS_00104B00) = 0x24u;                               /* 0x26E24 */
    text_cursor_hold_font2(-1, 7, game_string_get(0x3Cu), 0x4000u);  /* 0x26E3C */

    u32 v = DSB(DS_0010780B + side * 0x94u);                /* 0x26E59 */
    s32 pct = (s32)(v * 100u) / 0x78;                       /* 0x26E7B */

    if ((DSB(DS_00104529) & 2u) != 0u) {                    /* 0x26E85/0x26E88 */
        (void)actor_spawn((const u32 *)(mem + 0xA8970u), 0x1400u, 0xFFu,
                          0x1800u, 0u);                     /* 0x26EA0 0x2AE14 */
        (void)actor_spawn((const u32 *)(mem + 0xA8984u), 0x3A00u, 0xFFu,
                          0x1800u, 0u);                     /* 0x26EBB */
        text_number_set(0xF, 0xB, pct, 3, 1u, 0x4003u);     /* 0x26EDB 0x2F434 */
    } else {
        char buf[0x40];
        snprintf(buf, sizeof buf, "%s%03d%%",
                (const char *)game_string_get(0x3Du), (int)pct);   /* 0x26F01 0x65546 */
        size_t n = strlen(buf);
        snprintf(buf + n, sizeof buf - n, "%s",
                (const char *)game_string_get(0x3Eu));      /* 0x26F1D..0x26F3B */
        text_cursor_set(-1, 3, (const u8 *)buf, 0x2000u);   /* 0x26F48 0x2F198 */
    }
}

/* 0x26C8C — record §49-L. The mode 0x22 handler (0x24C5C case 0x22, jump
 * table entry). fight_slot_clear, then the per-side camera preamble shared
 * with mode 0x24 (record §49-N): camera_screen_base(side, the class byte
 * DS_0010782A[side]), DS_001077E8[side] = DS_001077E4[side] (a straight
 * field copy), camera_project(side, out_a = &DS_00100B08[side], out_b =
 * &DS_00100B00[side], facing = &DS_00100B62[side], page_flag =
 * &DS_00100B60[side], index_out = &DS_00100AF0[side]). Then fight_slot_pass,
 * fight_hud_pass(side), camera_y_commit, fight_4d2d0 (the effects pass,
 * record §43-A), camera_impact_dust_spawn when DS_00104AC4 > 1 (signed,
 * record §49-L), and flow_26d4c unconditionally. DS_00104AEC |= 0x18.
 * EBX/ECX/EDX/ESI are pushed and popped. Sole caller per get_xrefs_to:
 * 0x24C5C's jump table (record §47-B.1). */
void game_mode_22_step(void)
{
    fight_slot_clear();                                     /* 0x26C90 0x3C5CC */
    u32 side = DSB(DS_00104B1A);                            /* 0x26C95 */
    u32 ch = DSB(DS_0010782A + side * 0x94u);                /* 0x26CAE */
    camera_screen_base((s32)side, (s32)ch);                 /* 0x26CB7 0x16D58 */
    DSD(DS_001077E8 + side * 0x94u) = DSD(DS_001077E4 + side * 0x94u);  /* 0x26CD1/0x26CD8 */
    camera_project(side,
                   DS_00100B08 + side * 4u,
                   DS_00100B00 + side * 4u,
                   DS_00100B62 + side,
                   DS_00100B60 + side,
                   DS_00100AF0 + side * 4u);                 /* 0x26D0D 0x17FA0 */
    fight_slot_pass();                                       /* 0x26D12 0x3CB68 */
    fight_hud_pass(DSB(DS_00104B1A));                        /* 0x26D1E 0x35658 */
    camera_y_commit();                                       /* 0x26D23 0x12DA8 */
    fight_4d2d0();                                            /* 0x26D28 0x4D2D0 */
    if ((s32)DSD(DS_00104AC4) > 1)                             /* 0x26D2D/0x26D34 */
        camera_impact_dust_spawn();                              /* 0x26D36 0x128D4 */
    flow_26d4c();                                                /* 0x26D3B 0x26D4C */
    DSB(DS_00104AEC) = (u8)(DSB(DS_00104AEC) | 0x18u);            /* 0x26D40 */
}

#define DS_000A8908 0x000A8908u   /* no symbols.h name: [character*12] challenge string id row 3 */
#define DS_000A890C 0x000A890Cu   /* no symbols.h name: [character*12] challenge string id row 5 */
#define DS_000A8910 0x000A8910u   /* no symbols.h name: [character*12] challenge string id row 7 */

/* 0x26A50 — record §49-M. The mode 0x23 handler (0x24C5C case 0x23, jump
 * table entry). A 4-state sub-machine on DS_00104B25 (jump table 0x26A40,
 * `dec al; cmp al,3; ja` sends anything outside 1..4 straight to the shared
 * tail): state 1 arms the round (DS_00104B1B/DS_00104B15 = 1, DS_00104AC4 =
 * 0x16, DS_00104AD8 = 0, fight_hud_spawn_side(1) (record §49-M), DS_00104B20
 * = 1, fight_round_reset, DS_00104AFE = 0x3C, DS_00104B23 = 2, DS_00104B25 =
 * 4); state 2 spawns the round card (0xA895C or 0xBB6B4 by DS_00104529 bit
 * 1) into DS_00104ACC and arms the voice/countdown (DS_00104B25 = 4,
 * DS_00104AFE = 0x3C, DS_00104B23 = 3); state 3 erases the "22 spaces"
 * cursor line (0x2F4BC), kills DS_00104ACC, sets DS_00104ABC =
 * (DS_00104B1F == 3) + 1, DS_00104AAC = 0x78, DS_00104B00 = 0x22, and (with
 * DS_00104529 bit 1 clear only) draws three per-character strings from the
 * 12-byte-stride table 0xA8908/0xA890C/0xA8910 at rows 3/5/7; state 4
 * decrements DS_00104AFE and, once it reaches 0 (signed), sets DS_00104B25
 * = DS_00104B23. Every path (including the outside-1..4 default and the
 * shared tail states 3/4 fall into) ends with DS_00104AEC |= 8.
 * EBX/ECX/EDX/ESI/EDI are pushed and popped. Sole caller per get_xrefs_to:
 * 0x24C5C's jump table (record §47-B.1). */
void game_mode_23_step(void)
{
    switch (DSB(DS_00104B25)) {                             /* 0x26A55..0x26A69 */
    case 1u:
        DSB(DS_00104B1B) = 1u;                              /* 0x26A78 */
        DSB(DS_00104B15) = 1u;                              /* 0x26A7E */
        DSD(DS_00104AC4) = 0x16u;                            /* 0x26A8B */
        /* 0x26A91 0x29D60 is a ret-only no-op. */
        DSD(DS_00104AD8) = 0u;                              /* 0x26A9B */
        fight_hud_spawn_side(1u);                            /* 0x26AA1 0x1DAE8 */
        DSB(DS_00104B20) = 1u;                               /* 0x26AAA */
        fight_round_reset();                                /* 0x26AAF 0x20EF8 */
        DSW(DS_00104AFE) = 0x3Cu;                            /* 0x26AB6 */
        DSB(DS_00104B23) = 2u;                               /* 0x26ABD */
        DSB(DS_00104B25) = 4u;                               /* 0x26AC3 */
        break;
    case 2u: {
        u32 desc = (DSB(DS_00104529) & 2u) != 0u
                 ? 0xA895Cu : 0xBB6B4u;                      /* 0x26AD6..0x26B08 */
        DSD(DS_00104ACC) = actor_spawn((const u32 *)(mem + desc), 0x2A00u,
                                       0xFFu, 0x1200u, 0u);  /* 0x26B0D 0x2AE14 */
        DSB(DS_00104AE8) = (u8)(DSB(DS_00104AE8) | 0x40u);   /* 0x26B2C */
        /* PORT: 0x26B32 0x2C3FC(0x60) voice, not wired (record §45-A). */
        DSB(DS_00104B25) = 4u;                               /* 0x26B37 */
        DSW(DS_00104AFE) = 0x3Cu;                            /* 0x26B3F */
        DSB(DS_00104B23) = 3u;                               /* 0x26B46 */
        break;
    }
    case 3u:
        text_cursor_hold(-1, 5, mem + DS_00080994, 0u);      /* 0x26B6A 0x2F4BC */
        actor_set_dead(DSD(DS_00104ACC));                    /* 0x26B74 0x2B150 */
        DSD(DS_00104ACC) = 0u;                               /* 0x26B82 */
        DSD(DS_00104ABC) = (DSB(DS_00104B1F) == 3u ? 1u : 0u) + 1u;  /* 0x26B94 */
        /* PORT: 0x26B99 0x32970, the run clock, is out of scope (spec §7). */
        DSD(DS_00104AAC) = 0x78u;                            /* 0x26BBE */
        DSW(DS_00104B00) = 0x22u;                            /* 0x26BD1 */
        if ((DSB(DS_00104529) & 2u) == 0u) {                 /* 0x26BD8/0x26BDB */
            u32 side = DSB(DS_00104B1A);                     /* 0x26B9E */
            u32 ch = DSB(DS_0010782A + side * 0x94u);        /* 0x26BC4 */
            u32 row = ch * 12u;                              /* 0x26BEA */
            text_cursor_set(-1, 3, game_string_get(DSD(DS_000A8908 + row)),
                            0x2000u);                        /* 0x26C0D */
            text_cursor_set(-1, 5, game_string_get(DSD(DS_000A890C + row)),
                            0x2000u);                        /* 0x26C2E */
            text_cursor_set(-1, 7, game_string_get(DSD(DS_000A8910 + row)),
                            0x2000u);                        /* 0x26C4F */
        }
        break;
    case 4u: {
        s32 t = (s32)(s16)(DSW(DS_00104AFE) - 1u);           /* 0x26C68 */
        DSW(DS_00104AFE) = (u16)t;                           /* 0x26C69 */
        if (t <= 0)                                          /* 0x26C73 `jg` */
            DSB(DS_00104B25) = DSB(DS_00104B23);             /* 0x26C7A */
        break;
    }
    default:
        break;
    }
    DSB(DS_00104AEC) = (u8)(DSB(DS_00104AEC) | 8u);          /* 0x26C7F..0x26C86 */
}

/* 0x26F58 — record §49-N. The mode 0x24 handler (0x24C5C case 0x24, jump
 * table entry). The same per-side camera preamble as mode 0x22 (record
 * §49-L; fight_slot_clear, camera_screen_base, the DS_001077E8/E4 field
 * copy, camera_project), then fight_slot_pass, fight_hud_pass(side) and
 * fight_4d2d0 — but no camera_impact_dust_spawn and no camera_y_commit at
 * this point. DS_00104AEC |= 8 and DS_00104AFE -= 1 (word); while the
 * result is > 0 (signed) the function skips straight to camera_y_commit.
 * Once it reaches 0 or below: reset the join scratch (DS_00104B21 = 0,
 * DS_00104B14 = 1, DS_00104B0C = 0, DS_00104B11 = 0),
 * flow_1082d0_from_column(4), DS_00104AFC = 7, flow_side_char_random(r ^ 1)
 * with r = DS_00104AD4 (as 0x271E0's own 0x2722E call), arm the hook
 * game_hook_27134 (DS_00104AE4 = 0x27134 — this is the store the hook's own
 * header comment cites as "stored at 0x27074 (0x26F58)"), DS_00104AFE =
 * 0x78, DS_00104B00 = 0x17, DS_001088EE = 0, config_play_time_close(mode =
 * DS_00104ABC, flag = DS_00104B19), DS_00104B19 = 0, and the deferred
 * 0x2DAE4 audit add (spec §7, per config_play_time_close's own header).
 * Either way, camera_y_commit runs last. EBX/ECX/EDX/ESI/EDI are pushed and
 * popped. Sole caller per get_xrefs_to: 0x24C5C's jump table (record
 * §47-B.1). */
void game_mode_24_step(void)
{
    fight_slot_clear();                                     /* 0x26F5D 0x3C5CC */
    u32 side = DSB(DS_00104B1A);                            /* 0x26F62 */
    u32 ch = DSB(DS_0010782A + side * 0x94u);                /* 0x26F7B */
    camera_screen_base((s32)side, (s32)ch);                 /* 0x26F84 0x16D58 */
    DSD(DS_001077E8 + side * 0x94u) = DSD(DS_001077E4 + side * 0x94u);  /* 0x26F9E/0x26FA5 */
    camera_project(side,
                   DS_00100B08 + side * 4u,
                   DS_00100B00 + side * 4u,
                   DS_00100B62 + side,
                   DS_00100B60 + side,
                   DS_00100AF0 + side * 4u);                 /* 0x26FDA 0x17FA0 */
    fight_slot_pass();                                       /* 0x26FDF 0x3CB68 */
    fight_hud_pass(DSB(DS_00104B1A));                        /* 0x26FEB 0x35658 */
    fight_4d2d0();                                           /* 0x26FF0 0x4D2D0 */

    DSB(DS_00104AEC) = (u8)(DSB(DS_00104AEC) | 8u);          /* 0x27002/0x27006 */
    s32 t = (s32)(s16)(DSW(DS_00104AFE) - 1u);               /* 0x27005 */
    DSW(DS_00104AFE) = (u16)t;                               /* 0x2700C */
    if (t <= 0) {                                            /* 0x27016 `jg` */
        DSB(DS_00104B21) = 0u;                               /* 0x2702D */
        DSB(DS_00104B14) = 1u;                               /* 0x27037 */
        DSB(DS_00104B0C) = 0u;                               /* 0x2703D */
        DSB(DS_00104B11) = 0u;                               /* 0x27043 */
        flow_1082d0_from_column(4u);                         /* 0x27049 0x4660C */
        u32 r = DSD(DS_00104AD4);                            /* 0x2704E */
        DSW(DS_00104AFC) = 7u;                               /* 0x27057 */
        flow_side_char_random(r ^ 1u);                       /* 0x2705E 0x2716C */
        DSW(DS_00104AFE) = 0x78u;                            /* 0x2706D */
        DSD(DS_00104AE4) = FN_00027134;                      /* 0x27074 */
        DSW(DS_00104B00) = 0x17u;                            /* 0x2707A */
        DSW(DS_001088EE) = 0u;                               /* 0x2708D */
        config_play_time_close(DSD(DS_00104ABC), DSB(DS_00104B19));  /* 0x27094 0x32A3C */
        DSB(DS_00104B19) = 0u;                               /* 0x270A3 */
        /* PORT: 0x270A9 0x2DAE4's play-time audit add is out of scope
         * (spec §7), per config_play_time_close's own header. */
    }
    camera_y_commit();                                       /* 0x270AE 0x12DA8 */
}

/* 0x266AC — record §49-P. Mode 0x25's per-frame state-0 body: case 0x25's
 * inline DS_00104B25 == 0 arm calls it, then (DS_001088BD == 8 and both round
 * flags DS_001078F0/DS_001078F1) advances the case's own DS_00104B25 to 1 and
 * calls game_mode_25_reveal (0x4EF8C). Gated on both round flags: the word
 * DS_001088A6 decrements (wrapping mod 0x10000); when it *was* 0 (before the
 * decrement), DS_00104B18 = 1 and it resets to 0xB4 (the 180-frame window
 * fight_384f8/hit_3d004 read). Then unconditionally: fight_slot_clear,
 * camera_screen_base per side, DS_001077E8 = DS_001077E4 and DS_0010787C =
 * DS_00107878 (two plain dword copies), camera_project per side, fight_slot_
 * pass, fight_384f8(0)/fight_384f8(1); then, only when DS_001078FA == 2,
 * three more camera_project pairs with fighter_pass_b(0) between the first
 * and second pair (re-syncing the camera after fight_384f8's slot latches,
 * the same idiom mode 0xC's game_mode_0c_step runs once per frame). Always
 * then: fight_4e67c (the mode's audience-effects pass), camera_dust_spawn,
 * fight_hud_pulse, DS_00104AEC |= 2, and a HUD-text tail: with (DS_000EF6DC &
 * 0x1F) == 0 and bit 0x20 clear, both strings 0x3B/0x3A are cell-released at
 * row 5 (their glyph count, centred); with bit 0x20 set, one of them (DS_
 * 001088BC selects which) is drawn centred at row 5 mode 0x2000 instead; then,
 * while DS_001088A6 is nonzero (an unsigned test, not signed), its value / 60
 * is drawn at col -1, row
 * 0xB, width 1, pad 0, mode 0x2000. EBX/ECX/EDX/EDI are pushed and popped.
 * Only caller: 0x252B2 (0x24C5C case 0x25). */
void game_mode_25_step(void)
{
    if (DSB(DS_001078F0) != 0u && DSB(DS_001078F1) != 0u) {  /* 0x266B0..0x266C0 */
        u16 old = DSW(DS_001088A6);                          /* 0x266C2 */
        DSW(DS_001088A6) = (u16)(old - 1u);                  /* 0x266C8..0x266CB */
        if (old == 0u) {                                     /* 0x266D2/0x266D5 */
            DSB(DS_00104B18) = 1u;                            /* 0x266D7 */
            DSW(DS_001088A6) = 0xB4u;                         /* 0x266DE */
        }
    }

    fight_slot_clear();                                      /* 0x266E7 0x3C5CC */
    camera_screen_base(0, (s32)DSB(DS_0010782A));            /* 0x266FB 0x16D58 */
    camera_screen_base(1, (s32)DSB(DS_001078BE));             /* 0x26712 0x16D58 */
    DSD(DS_001077E8) = DSD(DS_001077E4);                      /* 0x26726 */
    DSD(DS_0010787C) = DSD(DS_00107878);                       /* 0x26735 */
    camera_project(0, DS_00100B08, DS_00100B00, DS_00100B62,
                   DS_00100B60, DS_00100AF0);                  /* 0x2673C 0x17FA0 */
    camera_project(1, DS_00100B0C, DS_00100B04, DS_00100B63,
                   DS_00100B61, DS_00100AF4);                   /* 0x2675F 0x17FA0 */
    fight_slot_pass();                                          /* 0x26764 0x3CB68 */
    fight_384f8(0u);                                             /* 0x2676B */
    fight_384f8(1u);                                              /* 0x26775 */

    if (DSB(DS_001078FA) == 2u) {                                 /* 0x2677C..0x26784 */
        camera_project(0, DS_00100B08, DS_00100B00, DS_00100B62,
                       DS_00100B60, DS_00100AF0);                   /* 0x267A5 */
        camera_project(1, DS_00100B0C, DS_00100B04, DS_00100B63,
                       DS_00100B61, DS_00100AF4);                    /* 0x267C8 */
        fighter_pass_b(0u);                                          /* 0x267CF 0x19068 */
        camera_project(0, DS_00100B08, DS_00100B00, DS_00100B62,
                       DS_00100B60, DS_00100AF0);                     /* 0x267EF */
        camera_project(1, DS_00100B0C, DS_00100B04, DS_00100B63,
                       DS_00100B61, DS_00100AF4);                      /* 0x26812 */
        camera_project(0, DS_00100B08, DS_00100B00, DS_00100B62,
                       DS_00100B60, DS_00100AF0);                       /* 0x26832 */
        camera_project(1, DS_00100B0C, DS_00100B04, DS_00100B63,
                       DS_00100B61, DS_00100AF4);                        /* 0x26855 */
    }

    fight_4e67c();                                                      /* 0x2685A */
    camera_dust_spawn();                                                 /* 0x2685F 0x1282C */
    fight_hud_pulse();                                                    /* 0x26864 0x1DA08 */
    DSB(DS_00104AEC) = (u8)(DSB(DS_00104AEC) | 2u);                       /* 0x2687A */

    if ((DSW(DS_000EF6DC) & 0x1Fu) == 0u) {                               /* 0x26887 */
        if ((DSW(DS_000EF6DC) & 0x20u) == 0u) {                          /* 0x2689C */
            text_cells_release_count(-1, 5, (s32)strlen(
                (const char *)game_string_get(0x3Bu)));                  /* 0x266EB..0x26912 0x2F388 */
            text_cells_release_count(-1, 5, (s32)strlen(
                (const char *)game_string_get(0x3Au)));                  /* 0x26917..0x26939 0x2F388 */
        } else if (DSB(DS_001088BC) == 0u) {                              /* 0x2689E/0x268A5 */
            text_cursor_set(-1, 5, game_string_get(0x3Au), 0x2000u);     /* 0x268C9..0x268E4 0x1C500/0x2F198 */
        } else {
            text_cursor_set(-1, 5, game_string_get(0x3Bu), 0x2000u);     /* 0x268A7..0x268C2 */
        }
    }

    if (DSW(DS_001088A6) != 0u) {                                         /* 0x26940/0x26943 */
        /* PORT: 0x26940 TEST/JBE is an unsigned "!= 0" test (0x2694a's
         * MOV DX,BX zero-extends before the divide), not a signed
         * comparison; the earlier (s16) cast here was wrong and is fixed. */
        text_number_draw(-1, 0xB, (s32)DSW(DS_001088A6) / 60, 1, 0u,
                         0x2000u);                                       /* 0x26954..0x2696E 0x2F4D0 */
    }
}

/* 0x4EF8C — record §49-P. Mode 0x25's reveal: for every entry on the active
 * list DS_0010884C whose +0x1C bit 1 is set, node->+0x14 = DS_000F0AF0 -
 * 0x8180, its actor R's +0x34 = 0xFF80, node->+0x1C &= 0x7F, R->+0x28 |=
 * 0x4080, node->+0x1E = 8, and R begins table_C9544[(u16)(R->+0x48 - 0x20)]
 * at 3.0. Then DS_0010889A = 0xF0 (the state-1 arm's countdown), both cell-
 * released strings 0x3B/0x3A at row 5, fight_mode25_scorecard() and, per
 * DS_0010888C vs DS_00108891 (the two sides' final scorecard tallies), one
 * font-2 string at row 0xB mode 0x4000: 0x63 then 0x64 at row 0xE when equal;
 * else 0x65 (DS_0010888C <= DS_00108891, unsigned) or 0x66, then always 0x63
 * (sic — the raw redraws string id 0x63, not the winner's own follow-up id)
 * at row 0xE. EBX/ECX/EDX/ESI are pushed and popped. Only caller: the
 * unported 0x252E1 (0x24C5C case 0x25, DS_001088BD == 8 arm). */
void game_mode_25_reveal(void)
{
    DSW(DS_0010889A) = 0xF0u;                                /* 0x4EF90/0x4EF9A */
    u32 node = DSD(DS_0010884C);
    while (node != DS_0010884C) {
        u32 next = DSD(node);                                /* 0x4EFAE */
        if ((DSW(node + 0x1Cu) & 2u) != 0u) {                /* 0x4EFB0..0x4EFB9 */
            u32 rec = DSD(node + 8u);
            u32 idx = (u32)(u16)((u32)DSB(rec + 0x48u) - 0x20u);  /* 0x4EFBE..0x4EFEC */
            DSD(node + 0x14u) = DSD(DS_000F0AF0) - 0x8180u;   /* 0x4EFC3..0x4EFCF */
            DSW(rec + 0x34u) = 0xFF80u;                        /* 0x4EFD5 */
            DSB(node + 0x1Cu) = (u8)(DSB(node + 0x1Cu) & 0x7Fu);  /* 0x4EFDB */
            DSW(rec + 0x28u) = (u16)(DSW(rec + 0x28u) | 0x4080u); /* 0x4EFE2..0x4EFEF */
            DSB(node + 0x1Eu) = 8u;                             /* 0x4EFF5 */
            actors_anim_begin(rec, DSD(0x000C9544u + idx * 4u),
                              0x40400000u);                     /* 0x4EFFC..0x4F00B 0x2BC30, the same
                                                                   * 0xC9544 table fight.c's DS_000C9544
                                                                   * local #define names */
        }
        node = next;                                             /* 0x4F010/0x4F012 */
    }

    text_cells_release_count(-1, 5,
        (s32)strlen((const char *)game_string_get(0x3Bu)));       /* 0x4F02E..0x4F049 0x2F388 */
    text_cells_release_count(-1, 5,
        (s32)strlen((const char *)game_string_get(0x3Au)));       /* 0x4F04E..0x4F069 0x2F388 */
    fight_mode25_scorecard();                                       /* 0x4F06E 0x4EBB8 */

    if (DSB(DS_0010888C) == DSB(DS_00108891)) {                      /* 0x4F073..0x4F080 */
        text_cursor_hold_font2(-1, 0xB, game_string_get(0x63u),
                               0x4000u);                              /* 0x4F082..0x4F09D 0x1C500/0x2F510 */
        text_cursor_hold_font2(-1, 0xE, game_string_get(0x64u),
                               0x4000u);                              /* 0x4F0DC..0x4F0F2 */
    } else {
        u32 id = (DSB(DS_0010888C) <= DSB(DS_00108891)) ? 0x65u : 0x66u;  /* 0x4F0A9..0x4F0B7 */
        text_cursor_hold_font2(-1, 0xB, game_string_get(id), 0x4000u); /* 0x4F0BC..0x4F0D2 */
        text_cursor_hold_font2(-1, 0xE, game_string_get(0x63u),
                               0x4000u);                              /* 0x4F0D7..0x4F0F2 */
    }
}

/* 0x4F0FC — record §49-P. Mode 0x25's timeout exit: draws strings 0x67 (row
 * 0xB) and 0x68 (row 0xE) font-2 mode 0x4000, cell-releases text rows 6..10
 * (col -1, 0xE cells each — the round-card body 0x4EBB8 drew), then DS_
 * 00104B00 = 6 (back to the fight-select flow), DS_001088C0 = 0, DS_00104B15
 * = 1 and DS_00104AEC |= 1. Only caller: the unported 0x25303 (0x24C5C case
 * 0x25, DS_00104B25 == 1 arm, timer expiry). */
void game_mode_25_exit(void)
{
    text_cursor_hold_font2(-1, 0xB, game_string_get(0x67u), 0x4000u); /* 0x4F0FF..0x4F11A */
    text_cursor_hold_font2(-1, 0xE, game_string_get(0x68u), 0x4000u); /* 0x4F11F..0x4F13A */
    text_cells_release_count(-1, 6, 0xE);                            /* 0x4F13F..0x4F14E 0x2F388 */
    text_cells_release_count(-1, 7, 0xE);                             /* 0x4F153..0x4F162 */
    text_cells_release_count(-1, 8, 0xE);                              /* 0x4F167..0x4F176 */
    text_cells_release_count(-1, 9, 0xE);                               /* 0x4F17B..0x4F18A */
    text_cells_release_count(-1, 0xA, 0xE);                              /* 0x4F18F..0x4F19E */
    DSW(DS_00104B00) = 6u;                                                /* 0x4F1A3 */
    DSB(DS_001088C0) = 0u;                                                 /* 0x4F1B0 */
    DSB(DS_00104B15) = 1u;                                                  /* 0x4F1BE */
    DSB(DS_00104AEC) = (u8)(DSB(DS_00104AEC) | 1u);                          /* 0x4F1C4 */
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

/* 0x1CAB8 — record §50-D. EAX = the music volume. Unchanged from
 * DS_000A2CB8 it does nothing; otherwise it is stored (0x1CACB) and, when the
 * sequence plays (0x1CAD9 0x5DEED status 4; a zero DS_001028C0 reads as not
 * playing, 0x1CAD2), pushed to the device over 500 ms (0x1CAF6 0x5DECA). */
void sound_music_volume(u32 v)
{
    if (v == DSD(DS_000A2CB8)) return;                     /* 0x1CABD */
    DSD(DS_000A2CB8) = v;                                  /* 0x1CACB */
    if (snd_music_playing() == 1u)                         /* 0x1CAD2..0x1CAF1 */
        AIL_set_sequence_volume(s_sequence, (s32)DSD(DS_000A2CB8), 500);   /* 0x1CAF6..0x1CB09 */
}

/* 0x1CED4 — record §50-D. EAX = the SFX volume. Unchanged from DS_000A2CB4 it
 * does nothing; otherwise it is stored (0x1CEE1) and every slot whose 0x5DD03
 * status is 4 (playing) gets it through 0x5DCC5 (slots 0..3 at stride 0x18,
 * 0x1CF12/0x1CF15 `cmp esi,0x60`). PORT: the slot's handle is s_samples[i],
 * not a mem[] word (see the sound-module comment above). */
void sound_sfx_volume(u32 v)
{
    if (v == DSD(DS_000A2CB4)) return;                     /* 0x1CED9 */
    DSD(DS_000A2CB4) = v;                                  /* 0x1CEE1 */
    for (u32 off = 0; off != SND_SLOT_END; off += SND_SLOT_STRIDE) {   /* 0x1CEE6 0x1CF15 */
        if (snd_slot_status(off) != 4) continue;           /* 0x1CEEF 0x1CEF7 */
        AIL_set_sample_volume(s_samples[off / SND_SLOT_STRIDE],
                              (s32)DSD(DS_000A2CB4));      /* 0x1CEFC..0x1CF0A */
    }
}

/* 0x1D1B0 — record §50-D. The music pause toggle (key 0x32 at 0x24DBF; the
 * pause arm of 0x1D250, the resume arm of 0x1D270). With the music paused
 * (DS_001028DA == 1) it clears the byte (0x1D1C2) and, when a current song
 * byte (DS_001028D9) and song (DS_001028D4) are set and there is a sequence
 * handle, makes the song pending again (0x1D1E7). Otherwise, with a sequence
 * handle that plays (0x1CA40), it stops the sequence (0x1D20B 0x5DEAF); either
 * way it then sets the pause byte (0x1D213). No caller reads the result. */
void sound_music_pause_toggle(void)
{
    if (DSB(DS_001028DA) == 1u) {                          /* 0x1D1BB */
        DSB(DS_001028DA) = 0;                              /* 0x1D1C2 */
        if (DSB(DS_001028D9) == 0u) return;                /* 0x1D1C8 */
        u32 song = DSD(DS_001028D4);                       /* 0x1D1D1 */
        if (song == 0u) return;                            /* 0x1D1D7 */
        if (DSD(DS_001028C0) == 0u) return;                /* 0x1D1E1 */
        DSD(DS_001028CC) = song;                           /* 0x1D1E7 */
        return;
    }
    if (DSD(DS_001028C0) != 0u && snd_music_playing() != 0u)   /* 0x1D1F2 0x1D1FB 0x1D202 */
        AIL_stop_sequence(s_sequence);                     /* 0x1D20B 0x5DEAF */
    DSB(DS_001028DA) = 1;                                  /* 0x1D213 */
}

/* 0x1D220 — record §50-D. The sample pause toggle (key 0x1F at 0x24DC9):
 * flips DS_001028DB (0x1D220) and, when it is now 1, stops every sample
 * (0x1D231 -> 0x1CD9C, a tail jump). */
void sound_sample_pause_toggle(void)
{
    DSB(DS_001028DB) ^= 1u;                                /* 0x1D220 */
    if (DSB(DS_001028DB) == 1u) snd_samples_stop_all();    /* 0x1D22E 0x1D231 */
}

/* 0x1D250 — record §50-D. The pause entry of the quit prompt (0x249F0) and the
 * pause key (0x24E26): with the music not yet paused, pauses it (0x1D25C
 * 0x1D1B0) and records that this pause is the prompt's (DS_001028D8 = 1,
 * 0x1D261: DL is 1 across the call); then stops every sample (0x1D267). */
void sound_pause(void)
{
    if (DSB(DS_001028DA) == 0u) {                          /* 0x1D251 */
        sound_music_pause_toggle();                        /* 0x1D25C */
        DSB(DS_001028D8) = 1u;                             /* 0x1D261 */
    }
    snd_samples_stop_all();                                /* 0x1D267 0x1CD9C */
}

/* 0x1D270 — record §50-D. The resume of 0x1D250: when it paused the music
 * (DS_001028D8 == 1) it toggles the music back (0x1D27C 0x1D1B0) and clears
 * the byte (0x1D283). */
void sound_resume(void)
{
    if (DSB(DS_001028D8) != 1u) return;                    /* 0x1D272 0x1D27A */
    sound_music_pause_toggle();                            /* 0x1D27C */
    DSB(DS_001028D8) = 0;                                  /* 0x1D283 */
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

/* 0x249F0 — record §50-D. The quit prompt (0x24C5C's key 0x10 at 0x24DDF with
 * AL = 0, and its mode arms at 0x24EAD / 0x24EC5 with AL = 0 / 1). `hard_quit`
 * is AL: 0 asks string 0x1EE and a yes sets the quit flag DS_000A81A8, nonzero
 * asks string 0x1EF and a yes leaves through the longjmp quit. It raises the
 * prompt flag DS_00104B22 (0x24A01), pauses the sound (0x1D250), draws the
 * question centred on row 10 (0x24A1C..0x24A32 0x1C500 0x2F198), presents one
 * frame (0x24A37 0x2EA78 with -1) and reads keys (0x24A64 int 16h AH=0) until
 * the flag drops: the key's ascii byte, upper-cased (0x24A7B 0x653ED: 'a'..'z'
 * minus 0x20, signed compares), against the first bytes of strings 0x1F0
 * (yes) and 0x1F1 (no), each sign-extended (0x24A4B 0x24A58 movsx). Yes drops
 * the flag (0x24A89); no drops it (0x24ABE) and clears the question's cells
 * (0x24AE7 0x2F280). Any other key loops. It ends by resuming the sound
 * (0x24AF9 0x1D270).
 * PORT: 0x24A9C..0x24AB0 (0x1D270, then the resource free 0x1B084, a no-op
 * under flat mem[] (see game_init), then `jmp 0x65431`, longjmp(0x1044F4, 1))
 * is out of scope (spec §7); the port resumes the sound and ends the run
 * through the same quit flag the AL = 0 arm sets. The blocking key read is
 * input_get_key. */
void game_quit_prompt(u32 hard_quit)
{
    u32 id = (hard_quit & 0xFFu) == 0u ? 0x1EEu : 0x1EFu;  /* 0x24A0C..0x24A17 */
    DSB(DS_00104B22) = 1u;                                 /* 0x24A01 */
    sound_pause();                                         /* 0x24A07 0x1D250 */
    text_cursor_set(-1, 0xA, game_string_get(id), 0x1000u);   /* 0x24A1C..0x24A32 0x1C500 0x2F198 */
    config_screen_wait(-1);                                /* 0x24A37 0x2EA78 */
    s32 yes = (s8)game_string_get(0x1F0u)[0];              /* 0x24A41..0x24A4B */
    s32 no = (s8)game_string_get(0x1F1u)[0];               /* 0x24A4E..0x24A58 */
    do {
        s32 key = (s32)(input_get_key() & 0xFFu);          /* 0x24A64..0x24A76 int 16h AH=0 */
        if (key >= 0x61 && key <= 0x7A) key -= 0x20;       /* 0x24A7B 0x653ED */
        if (key == yes) {                                  /* 0x24A80 */
            DSB(DS_00104B22) = 0;                          /* 0x24A89 */
            if ((hard_quit & 0xFFu) == 0u) {               /* 0x24A91 */
                DSB(DS_000A81A8) = 1u;                     /* 0x24A93 */
            } else {
                sound_resume();                            /* 0x24A9C 0x1D270 */
                DSB(DS_000A81A8) = 1u;                     /* 0x24AA1..0x24AB0 */
                return;
            }
        } else if (key == no) {                            /* 0x24AB5 */
            DSB(DS_00104B22) = 0;                          /* 0x24ABE */
            text_cells_release(-1, 0xA, game_string_get(id), 0x1000u);   /* 0x24AC4..0x24AE7 0x1C500 0x2F280 */
        }
    } while (DSB(DS_00104B22) != 0u);                      /* 0x24AEC */
    sound_resume();                                        /* 0x24AF9 0x1D270 */
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
    DSD(DS_00101510) = 1;                /* PORT: 0x1ACA8 (DPMI 0400h CPU probe, result is never read again; record §50-C) -> ok */
    DSD(DS_00101514) = GAME_BIOS_BASE;   /* PORT: selector<<4 -> flat scratch */
    mem_fill(GAME_BIOS_BASE, 0, GAME_BIOS_LEN);
    DSD(DS_000A2CAC) = DSD(DS_00101514);
    /* PORT: no DPMI — the 0x109A0 region locks are no-ops. */
    /* PORT: 0x1B3AC resource-file setup is replaced by res_load_index(). */
    game_audio_init();      /* 0x1CF40: AIL_startup, prefs, handles, timer */
    /* PORT: extended-memory block list (0x1E2A0/0x1C0F0) unused under flat mem[]. */
    /* PORT: 0x4FB98 (int 10h set mode) — the SDL host owns the window. */

    if (int10h_query() != 0x13) { game_fatal("no VGA 320x200 mode"); return; }

    /* PORT: 0x10C30/0x10D34/0x10D0C (CD-ROM drive locate and restore) and the
     * DPMI locks 0x1ADAC/0x1ADE4 have no flat-memory or host equivalent (record
     * §50-E). */
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

    /* 0x20CF0-0x20CF9: the key-config record round trip. PORT: the raw packs
     * into a stack record; the port uses a scratch inside the BIOS block
     * (GAME_BIOS_LEN is 0x1000, the key config ends at +0x2ED). */
    config_keys_pack(GAME_BIOS_BASE + 0x800u);       /* 0x20CF2 */
    config_keys_load(GAME_BIOS_BASE + 0x800u);       /* 0x20CF9 */
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
    /* PORT: 0x1B084 (the config-file writer, deferred storage, record §50-C) and the memory frees are no-ops under flat mem[]. */
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

/* ---- modes 0x28-0x2F, the coin/start divert's eight game-start entries
 * (record §49-Q) -------------------------------------------------------- */

/* 0x24F09 — record §49-Q. Mode 0x28's handler (0x24C5C case 0x28, jump-table
 * entry 0x24F09, the table's own named-gap listing until now; its only
 * caller). See flow.h for the group derivation. Decodes config field 0x29
 * (0x2D974) into DS_00104528/DS_00105B3A/DS_0010452C/DS_001088D0, exactly as
 * game_state_init's 0x20C5D-0x20CC2 (record §46-F), sets the both-sides mask
 * DS_00104AB8 = 1 and diverts both players (0x257A4(3)). */
void game_mode_28_step(void)
{
    u32 v = config_field_get(0x29u);                     /* 0x24F09 0x2D974 */
    DSD(DS_00104528) = v;                                /* 0x24F1A */
    DSB(DS_00105B3A) = (u8)((v & 0x100u) >> 4);          /* 0x24F2C */
    DSD(DS_00104AB8) = 1u;                               /* 0x24F38 */
    DSD(DS_001088D0) = (v & 0xFu) * 5u + 0x1Eu;          /* 0x24F4C */
    DSB(DS_0010452C) = (u8)((v & 0xF0u) >> 4);           /* 0x24F56 */
    game_coin_divert(3u);                                /* 0x24F5C 0x257A4 */
}

/* 0x24F66 — record §49-Q. Mode 0x29's handler (0x24C5C case 0x29, jump-table
 * entry 0x24F66; its only caller). Identical to game_mode_28_step's field
 * decode and divert(3), differing only in DS_00104AB8 = 2. */
void game_mode_29_step(void)
{
    u32 v = config_field_get(0x29u);                     /* 0x24F66 0x2D974 */
    DSD(DS_00104528) = v;                                /* 0x24F75 */
    DSD(DS_00104AB8) = 2u;                               /* 0x24F7E */
    DSB(DS_00105B3A) = (u8)((v & 0x100u) >> 4);          /* 0x24F95 */
    DSB(DS_0010452C) = (u8)((v & 0xF0u) >> 4);           /* 0x24FA7 */
    DSD(DS_001088D0) = (v & 0xFu) * 5u + 0x1Eu;          /* 0x24FB4 */
    game_coin_divert(3u);                                /* 0x24FBA 0x257A4 */
}

/* 0x24FC4 — record §49-Q. Mode 0x2A's handler (0x24C5C case 0x2A, jump-table
 * entry 0x24FC4; its only caller). Identical field decode and divert(3),
 * differing only in DS_00104AB8 = 3. */
void game_mode_2a_step(void)
{
    u32 v = config_field_get(0x29u);                     /* 0x24FC4 0x2D974 */
    DSD(DS_00104528) = v;                                /* 0x24FD0 */
    DSB(DS_00105B3A) = (u8)((v & 0x100u) >> 4);          /* 0x24FE8 */
    DSB(DS_0010452C) = (u8)((v & 0xF0u) >> 4);           /* 0x24FF9 */
    DSD(DS_001088D0) = (v & 0xFu) * 5u + 0x1Eu;          /* 0x25007 */
    DSD(DS_00104AB8) = 3u;                               /* 0x2500E */
    game_coin_divert(3u);                                /* 0x25014 0x257A4 */
}

/* 0x25187 — record §49-Q. Mode 0x2B's handler (0x24C5C case 0x2B, jump-table
 * entry 0x25187; its only caller). The one case in the group with no config
 * field 0x29 read at all: DS_00104AB8 = 3 directly, then divert(3). */
void game_mode_2b_step(void)
{
    DSD(DS_00104AB8) = 3u;                               /* 0x2518E */
    game_coin_divert(3u);                                /* 0x25194 0x257A4 */
}

/* 0x2501E — record §49-Q. Mode 0x2C's handler (0x24C5C case 0x2C, jump-table
 * entry 0x2501E; its only caller). The field decode again, but with no
 * DS_00104AB8 store at all this time — it keeps whatever an earlier case (or
 * game_state_init's own DS_00104AB8 = 0 init, 0x10ED3) left there — then
 * divert(3). */
void game_mode_2c_step(void)
{
    u32 v = config_field_get(0x29u);                     /* 0x2501E 0x2D974 */
    DSD(DS_00104528) = v;                                /* 0x25028 */
    DSB(DS_00105B3A) = (u8)((v & 0x100u) >> 4);          /* 0x25042 */
    DSB(DS_0010452C) = (u8)((v & 0xF0u) >> 4);           /* 0x25054 */
    DSD(DS_001088D0) = (v & 0xFu) * 5u + 0x1Eu;          /* 0x25061 */
    game_coin_divert(3u);                                /* 0x25067 0x257A4 */
}

/* 0x25071 — record §49-Q. Mode 0x2D's handler (0x24C5C case 0x2D, jump-table
 * entry 0x25071; its only caller). The field decode, no DS_00104AB8 store,
 * then one credit spent (config_credit_spend, 0x2CA7C) before diverting side
 * 0 alone (0x257A4(1)). */
void game_mode_2d_step(void)
{
    u32 v = config_field_get(0x29u);                     /* 0x25071 0x2D974 */
    DSD(DS_00104528) = v;                                /* 0x2507B */
    DSB(DS_00105B3A) = (u8)((v & 0x100u) >> 4);          /* 0x25095 */
    DSB(DS_0010452C) = (u8)((v & 0xF0u) >> 4);           /* 0x250A7 */
    DSD(DS_001088D0) = (v & 0xFu) * 5u + 0x1Eu;          /* 0x250B4 */
    (void)config_credit_spend(1u);                       /* 0x250BA 0x2CA7C */
    game_coin_divert(1u);                                /* 0x250C4 0x257A4 */
}

/* 0x250CE — record §49-Q. Mode 0x2E's handler (0x24C5C case 0x2E, jump-table
 * entry 0x250CE; its only caller). Same shape as game_mode_2d_step (field
 * decode, no DS_00104AB8 store, config_credit_spend(1)), but diverts side 1
 * alone (0x257A4(2)) instead of side 0. */
void game_mode_2e_step(void)
{
    u32 v = config_field_get(0x29u);                     /* 0x250CE 0x2D974 */
    DSD(DS_00104528) = v;                                /* 0x250D8 */
    DSB(DS_00105B3A) = (u8)((v & 0x100u) >> 4);          /* 0x250F2 */
    DSB(DS_0010452C) = (u8)((v & 0xF0u) >> 4);           /* 0x25104 */
    DSD(DS_001088D0) = (v & 0xFu) * 5u + 0x1Eu;          /* 0x25111 */
    (void)config_credit_spend(1u);                       /* 0x25117 0x2CA7C */
    game_coin_divert(2u);                                /* 0x25121 0x257A4 */
}

/* 0x2512B — record §49-Q. Mode 0x2F's handler (0x24C5C case 0x2F, jump-table
 * entry 0x2512B; its only caller). Byte for byte the same operation as
 * game_mode_2e_step (field decode, no DS_00104AB8 store, config_credit_
 * spend(1), divert(2)) — the compiler emitted it a second time under a
 * separate case label rather than sharing 0x250CE's block; ported as
 * observed, not merged into one function. */
void game_mode_2f_step(void)
{
    u32 v = config_field_get(0x29u);                     /* 0x2512B 0x2D974 */
    DSD(DS_00104528) = v;                                /* 0x25137 */
    DSB(DS_00105B3A) = (u8)((v & 0x100u) >> 4);          /* 0x25149 */
    DSD(DS_001088D0) = (v & 0xFu) * 5u + 0x1Eu;          /* 0x25163 */
    DSB(DS_0010452C) = (u8)((v & 0xF0u) >> 4);           /* 0x2516D */
    (void)config_credit_spend(1u);                       /* 0x25173 0x2CA7C */
    game_coin_divert(2u);                                /* 0x2517D 0x257A4 */
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

    /* PORT: 0x24CFE..0x24EE7, the int 16h keyboard loop, is ported only for
     * mode 0x1E (record §53-A, below); elsewhere only its ESC quit arm is, in
     * game_loop. It runs before the switch and is one of the three ways out of
     * mode 3: Enter in mode 3 stores mode 0x27 (0x24EE0, the start menu),
     * 0x11D04's coin/start arm calls 0x257A4 (game_coin_divert), and so does
     * 0x11D04's state 8 (reached only when DS_00108173 is non-zero, which no
     * instruction stores) (records §47-B, §48-W). */
    /* 0x24D08..0x24D6C: while a key is queued (AH = 1, 0x24D0E), read it (AH =
     * 0, 0x24D2E), latch its ascii byte, or its scan code when the ascii byte is
     * 0 (0x24D3E..0x24D4D), and in mode 0x1E (the word compare `mov ax,
     * [0x104b00]; cmp eax,0x1e`, 0x24D54/0x24D5A) hand a non-zero ascii byte to
     * 0x20860 and loop (0x24D6C). The mode test is per key in the raw; no key
     * this arm handles changes the mode, so it is hoisted. Mode 0x1E's keys
     * therefore never reach game_loop's ESC arm, as in the raw, where ESC (ascii
     * 0x1B) goes to 0x20860 too.
     * PORT: an extended key (ascii 0) in mode 0x1E is read and latched but its
     * scan-code dispatch (0x24D8E..0x24DDF: 0x10 the quit prompt 0x249F0, 0x1F
     * 0x1D220, 0x24 0x5004A, 0x32 0x1D1B0) is not ported; it is dropped. */
    if (DSW(DS_00104B00) == 0x1Eu) {                       /* 0x24D54..0x24D5D */
        while (input_check_key() != 0) {                   /* 0x24D08..0x24D20 int 16h AH=1 */
            u32 key = input_get_key();                     /* 0x24D26..0x24D3B int 16h AH=0 */
            u32 bl = key & 0xFFu;
            DSD(DS_00105F30) = (bl != 0u ? key : key >> 8) & 0xFFu;   /* 0x24D3E..0x24D4D */
            if (bl != 0u) nameentry_key(bl);               /* 0x24D5F..0x24D67 0x20860 */
        }
    }

    /* 0x24EEC..0x24F01: the mode switch, on the word DS_00104B00 (`mov
     * ax,[0x104b00]; cmp ax,0x33; ja 0x2540F; and eax,0xffff; jmp
     * [eax*4+0x24B8C]`). Every case ends at 0x2540F, the 0x2A31C tail below.
     * Record §47-B lists every entry. No ported path the oracles exercise
     * leaves mode 3 (§47-B.2), so only case 3 runs outside the unit tests and
     * real input (an accepted coin/start event reaches 0x1A, record §48-W). */
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
    case 0x17u:
        frontend_mode_17_step();                       /* 0x253EE 0x4F318 */
        break;
    case 0x16u:
        frontend_mode_16_step();                       /* 0x253E7 0x4F2B0 (record §49-G) */
        break;                                         /* 0x253EC */
    case 0x15u:
        frontend_mode_15_step();                       /* 0x253E0 0x4F24C (record §48-X) */
        break;                                         /* 0x253E5 */
    case 0x18u:
        frontend_mode_18_step();                       /* 0x253F5 0x4F6E8 (record §49-I) */
        break;                                         /* 0x253FA */
    case 0x19u:
        frontend_mode_19_step();                       /* 0x253FC 0x4F704 (record §49-I) */
        break;                                         /* 0x25401 */
    case 0x10u:
        fight_mode_10_step();                          /* 0x25385 0x438B4 */
        break;
    case 0x0Du:
        game_mode_0d_step();                           /* 0x25367 0x274FC */
        break;
    case 0x32u:
        game_mode_32_step();                           /* 0x251B2 0x296B8 */
        break;
    case 0x04u:
        game_mode_04_step();                           /* 0x25242 0x26254 */
        break;                                         /* 0x25247 */
    case 0x05u:
        game_mode_05_step();                           /* 0x2524C 0x25C88 */
        break;
    case 0x06u:
        /* 0x25256 (record §48-J): with DS_00104B1D == 0, 0x28CC8's non-zero
         * result r joins side r - 1 through 0x28DA4 (`dec eax`). */
        if (DSB(DS_00104B1D) == 0u) {                  /* 0x25256/0x2525D */
            u32 r = flow_join_poll();                  /* 0x2525F 0x28CC8 */
            if (r != 0u) {                             /* 0x25264/0x25266 */
                flow_player_join(r - 1u);              /* 0x25268/0x25269 0x28DA4 */
                break;                                 /* 0x2526E */
            }
        }
        /* 0x2525D/0x25266 jump to case 4's entry 0x25242 (`call 0x26254;
         * jmp 0x2540F`), the fight frame (record §48-K). */
        game_mode_04_step();                           /* 0x25242 0x26254 */
        break;                                         /* 0x25247 */
    case 0x0Cu: {
        /* 0x25349 (record §48-J): 0x28CC8 with no DS_00104B1D test; a
         * non-zero result r joins side r - 1 through 0x28DA4. */
        u32 r = flow_join_poll();                      /* 0x25349 0x28CC8 */
        if (r != 0u) {                                 /* 0x2534E/0x25350 */
            flow_player_join(r - 1u);                  /* 0x25352/0x25353 0x28DA4 */
            break;                                     /* 0x25358 */
        }
        game_mode_0c_step();                           /* 0x2535D 0x27380 (record §48-C) */
        break;                                         /* 0x25362 */
    }
    case 0x0Eu:
        game_mode_0e_step();                           /* 0x25371 0x27A2C (record §48-E) */
        break;                                         /* 0x25376 */
    case 0x0Bu:
        /* 0x25287 (record §48-B): the fight frame, then the winner-pose
         * tick. */
        game_mode_04_step();                           /* 0x25287 0x26254 */
        flow_winner_pose_step();                       /* 0x2528C 0x28C38 */
        break;                                         /* 0x25291 */
    case 0x13u:
        game_mode_13_step();                           /* 0x253C4 0x424E8 (record §48-D) */
        break;                                         /* 0x253C9 */
    case 0x12u:
        game_mode_12_step();                           /* 0x253BD 0x41C28 (record §48-Z) */
        break;                                         /* 0x253C2 */
    case 0x09u:
        game_mode_09_step();                           /* 0x2533F 0x28788 (record §48-Y) */
        break;                                         /* 0x25344 */
    case 0x08u:
        game_mode_08_step();                           /* 0x25335 0x28468 (record §49-C) */
        break;                                         /* 0x2533A */
    case 0x0Au:
        game_mode_0a_step();                           /* 0x2527D 0x28BD4 (record §49-D) */
        break;                                         /* 0x25282 */
    case 0x1Eu:
        game_mode_1e_step();                           /* 0x253CB 0x1EEB0 (record §49-H) */
        break;
    case 0x1Fu:
        game_mode_1f_step();                           /* 0x253D2 0x208F8 (record §49-J) */
        break;
    case 0x30u:
        game_mode_30_step();                           /* 0x29328 (record §49-K) */
        break;
    case 0x31u:
        game_mode_31_step();                           /* 0x299E8 (record §49-K) */
        break;
    case 0x33u:
        game_mode_33_step();                           /* 0x29638 (record §49-K) */
        break;
    case 0x21u:
        game_mode_21_step();                           /* 0x26540 (record §49-O) */
        break;
    case 0x07u:
        game_mode_07_step();                           /* 0x25273 0x282C4 (record §49-E) */
        break;                                         /* 0x25278 */
    case 0x0Fu:
        game_mode_0f_step();                           /* 0x2537B 0x277C0 (record §49-F) */
        break;                                         /* 0x25380 */
    case 0x22u:
        game_mode_22_step();                           /* 0x2540F 0x26C8C (record §49-L) */
        break;
    case 0x23u:
        game_mode_23_step();                           /* 0x2540F 0x26A50 (record §49-M) */
        break;
    case 0x24u:
        game_mode_24_step();                           /* 0x2540F 0x26F58 (record §49-N) */
        break;
    case 0x28u:
        game_mode_28_step();                           /* 0x24F09 (record §49-Q) */
        break;
    case 0x29u:
        game_mode_29_step();                           /* 0x24F66 (record §49-Q) */
        break;
    case 0x2Au:
        game_mode_2a_step();                           /* 0x24FC4 (record §49-Q) */
        break;
    case 0x2Bu:
        game_mode_2b_step();                           /* 0x25187 (record §49-Q) */
        break;
    case 0x2Cu:
        game_mode_2c_step();                           /* 0x2501E (record §49-Q) */
        break;
    case 0x2Du:
        game_mode_2d_step();                           /* 0x25071 (record §49-Q) */
        break;
    case 0x2Eu:
        game_mode_2e_step();                           /* 0x250CE (record §49-Q) */
        break;
    case 0x2Fu:
        game_mode_2f_step();                           /* 0x2512B (record §49-Q) */
        break;
    case 0x25u: {
        /* 0x252A0..0x25312 — record §49-P. The inline DS_00104B25 sub-state
         * machine: 0 runs game_mode_25_step (0x266AC), then when DS_
         * 001088BD == 8 and both round flags DS_001078F0/DS_001078F1 are set,
         * game_mode_25_reveal (0x4EF8C) and DS_00104B25++; 1 decrements the
         * word DS_0010889A and, while it stays nonzero (an unsigned test),
         * runs fight_effects_pass (0x49C78), else game_mode_25_exit
         * (0x4F0FC); any
         * other DS_00104B25 value falls straight through to the break. */
        u8 sub = DSB(DS_00104B25);                          /* 0x252A0 */
        if (sub == 0u) {                                    /* 0x252A5/0x252A7 */
            game_mode_25_step();                             /* 0x252B2 0x266AC */
            if (DSB(DS_001088BD) == 8u                        /* 0x252B9/0x252BE */
                    && DSB(DS_001078F0) != 0u                   /* 0x252C7/0x252CE */
                    && DSB(DS_001078F1) != 0u) {                 /* 0x252D4/0x252DB */
                game_mode_25_reveal();                            /* 0x252E1 0x4EF8C */
                DSB(DS_00104B25) = (u8)(DSB(DS_00104B25) + 1u);    /* 0x252E6 */
            }
        } else if (sub == 1u) {                                /* 0x252A9/0x252AB */
            u16 t = (u16)(DSW(DS_0010889A) - 1u);                /* 0x252F1/0x252F7 */
            DSW(DS_0010889A) = t;                                 /* 0x252F8 */
            /* PORT: 0x252FE TEST/JA is an unsigned "!= 0" test, not the
             * signed comparison an earlier draft of this port used. */
            if (t != 0u) fight_effects_pass();                     /* 0x252FE/0x25301 0x49C78 */
            else game_mode_25_exit();                               /* 0x25303 0x4F0FC */
        }
        break;                                                      /* 0x2530C/0x25312 */
    }
    case 0x27u: {
        /* 0x251C6..0x25215 — record §49-X. The service menu: 0x50146 sets the
         * repeat mask 0xC000C000 with delays 0x1E/0xF, then the per-frame menu
         * 0x2FFC4 over the table 0xBCBDC (stride 0x10, flags 4 from the
         * `mov ecx,4` at 0x251D5). Results 0, -5 and -10 (0x251F5..0x251FC)
         * go on to 0x4F644; any other result is the longjmp(0x1044F4, 1) at
         * 0x25206. No ported path sets DS_00104B00 to 0x27 (the raw's setters
         * are in the unported 0x2CBxx callbacks), so this arm is not reached by
         * the front-end, demo-fight or attract runs.
         * PORT: 0x25206 0x65431 longjmp is out of scope (spec §7): the port
         * skips 0x4F644 and continues.
         * The record §47-B.1 note on the other cases follows. Case 0x17 is ported (0x4F318, record
         * §46-G) and dispatched above as frontend_mode_17_step, case 0x10
         * (0x438B4, record §47-M) as fight_mode_10_step, and cases 0xD
         * (0x274FC) and 0x32 (0x296B8, record §48-Q) as game_mode_0d_step and
         * game_mode_32_step, and case 5 (0x25C88, record §48-U) as
         * game_mode_05_step, and case 4 (0x26254, record §48-K) as
         * game_mode_04_step. Cases 6 and 0xC run 0x28CC8 (flow_join_poll)
         * and 0x28DA4 (flow_player_join) above (record §48-J); case 6's
         * other arm is case 4's 0x26254 (record §48-K), and case 0xC's,
         * 0x27380, is game_mode_0c_step (record §48-C). Case 0xE (0x27A2C,
         * record §48-E) is game_mode_0e_step, case 0x15 (0x4F24C, record
         * §48-X) is frontend_mode_15_step, case 0xB (0x28C38, record §48-B)
         * runs after 0x26254, case 0x13 (0x424E8, record §48-D) is
         * game_mode_13_step, case 9 (0x28788, record §48-Y) is
         * game_mode_09_step, case 8 (0x28468, record §49-C) is
         * game_mode_08_step, case 0xA (0x28BD4, record §49-D) is
         * game_mode_0a_step, case 0x12 (0x41C28, record §48-Z) is
         * game_mode_12_step, case 0x16 (0x4F2B0, record §49-G) is
         * frontend_mode_16_step, case 0x1E (0x1EEB0, record §49-H) is
         * game_mode_1e_step, cases 0x18/0x19 (0x4F6E8/0x4F704, record
         * §49-I) are frontend_mode_18_step/frontend_mode_19_step, case
         * 0x1F (0x208F8, record §49-J) is game_mode_1f_step, cases
         * 0x30/0x31/0x33 (0x29328/0x299E8/0x29638, record §49-K) are
         * game_mode_30_step/game_mode_31_step/game_mode_33_step, case
         * 0x21 (0x26540, record §49-O) is game_mode_21_step, case 7
         * (0x282C4, record §49-E) is game_mode_07_step, case 0xF
         * (0x277C0, record §49-F) is game_mode_0f_step, cases
         * 0x22/0x23/0x24 (0x26C8C/0x26A50/0x26F58, records §49-L/§49-M/
         * §49-N) are game_mode_22_step/game_mode_23_step/game_mode_24_step,
         * cases 0x28..0x2F (0x24F09/0x24F66/0x24FC4/0x25187/0x2501E/
         * 0x25071/0x250CE/0x2512B, record §49-Q) are game_mode_28_step
         * through game_mode_2f_step, each dispatched above, and case 0x25
         * (0x252A0's inline DS_00104B25 sub-state machine over 0x266AC/
         * 0x4EF8C/0x4F0FC, record §49-P) is game_mode_25_step/game_mode_25_
         * reveal/game_mode_25_exit, dispatched in its own `case 0x25u:`
         * block above (not this fallthrough). */
        input_repeat_set(0xC000C000u, 0x1Eu, 0xFu);          /* 0x251C6..0x251DA 0x50146 */
        {
            u32 r = menu_step(0xBCBDCu, 0x10u, 4u);           /* 0x251DF..0x251EE 0x2FFC4 */
            if (r == 0u || r == (u32)-5 || r == (u32)-10)     /* 0x251F3..0x251FC */
                input_state_update();                          /* 0x25210 0x4F644 */
        }
        break;
    }
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

/* 0x11F28 — record §47-M. One coin/start event: requires a credit (0x2C060),
 * then tests the event's mask in the DS_0009ACBC table against the
 * newly-pressed bits DS_001088E4, and debits one credit through 0x2CA7C.
 * Returns 1 when accepted (0x11F42/0x11F4C: 1 whatever 0x2CA7C returns).
 * Called at 0x11D15/0x11D28 and, with EAX = the side, by 0x43928 (0x43939)
 * and 0x43B24 (0x43B4E). */
u32 frontend_coin_poll(u32 code)
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
        if (accepted != 0u) {                          /* 0x11D34/0x11D36 */
            /* PORT: 0x11D38 runs 0x32970(eax = 0), the run-clock/tick update,
             * out of scope (spec §7). It pushes and pops EDX, so 0x11D3F's
             * `mov eax,edx` passes the accepted mask on (record §48-W). */
            game_coin_divert(accepted);                /* 0x11D41 0x257A4 */
            return;                                    /* 0x11D46..0x11D49 */
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
            /* PORT: 0x11EAC runs 0x32970(eax = 0), the run clock, out of scope
             * (spec §7); 0x11EB3 then loads EAX = 3. Only attract phase 0xB
             * (0x114D6) stores state 8, when DS_00108173 != 0, and no
             * instruction stores that byte (records §47-M.2, §48-W). */
            game_coin_divert(3u);                       /* 0x11EB8 0x257A4 */
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
