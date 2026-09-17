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

static void game_state_title(void)
{
    if (!s_title_ready) {
        title_load();
        if (!s_title_ready) return;
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

/* ---- the exported flow -------------------------------------------------- */

void game_set_game_dir(const char *dir) { s_game_dir = dir; }

int game_main(void)
{
    char index_path[512];
    if (!s_game_dir) {
        fprintf(stderr, "game_main: no game dir set\n");
        return 1;
    }
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
    /* PORT: audio/AIL (sub-project 3): 0x1CF40. */
    /* PORT: extended-memory block list (0x1E2A0/0x1C0F0) unused under flat mem[]. */
    /* PORT: 0x4FB98 (int 10h set mode) — the SDL host owns the window. */

    if (int10h_query() != 0x13) { game_fatal("no VGA 320x200 mode"); return 0; }

    /* PORT: DPMI locks 0x10C30/0x10D34/0x1ADAC/0x1ADE4/0x10D0C are no-ops. */
    if (res_load_index(s_game_dir, index_path) <= 0) {
        game_fatal("resource INDEX load failed");
        return 0;
    }
    surface_setup();        /* 0x51F45 */
    palette_list_init();    /* 0x336C0 */
    /* PORT: 0x5004A joystick init — the port reads int 16h keyboard only. */
    /* PORT: 0x1D0BC MIDI-memory setup (audio, sub-project 3). */
    /* PORT: 0x47370 EEPROM read (menus/EEPROM, sub-project 4). */

    game_state_init();      /* 0x20C10's FUN_00010E80 */
    game_loop();            /* 0x20C10 -> 0x255CC */
    /* 0x1BE30 teardown. PORT: 0x1D018 (audio shutdown, sub-project 3),
     * 0x1B084 (resource free) and the memory frees are no-ops under flat mem[]. */
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
