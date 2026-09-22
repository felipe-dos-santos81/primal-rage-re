#include "gfx.h"
#include "res.h"
#include "../host.h"
#include "../mem.h"
#include "../symbols.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

u8 gfx_dac[256][3];

/* 0x336C0: resets the palette ownership table and dirty list. The raw's first
 * action (0x336C3) is mem_fill(0x87618, 0, 0x180) — it clears the 24-entry
 * palette ownership table at DS_00107618 that palette_acquire records into;
 * without it a later actors_reset leaves those entries live, so a palette
 * acquired before the reset is found again and never re-enqueued, leaving the
 * DAC cleared (black). Pre-attract the table was empty at the title, so the
 * omission was invisible; the boot attract fills it. PORT: flow.c's game_init
 * and game/actors.c's actors_reset() (0x2BAF4) share this owner; the original's
 * trailing 0x33734 initial-palette enqueue has no VGA DAC to reset, so the port
 * just clears gfx_dac. */
void palette_list_init(void)
{
    mem_fill(DS_00107618, 0, 0x180u);   /* 0x336CF: the ownership table */
    DSD(DS_00107798) = DS_00107498;
    for (u32 i = 0; i < 0x180; i += 0x10) DSD(DS_0010749C + i) = 0xFFFFFFFFu;
    DSD(DS_000BD470) = 0;
    memset(gfx_dac, 0, sizeof gfx_dac);
}

/* 0x33734: appends a raw-pointer palette record { ptr; first; count; flag } at
 * the DS_00107798 head and advances the head. PORT: moved from flow.c so the
 * dirty-list writer has one owner; flow.c's init enqueue and game/actors.c's
 * 0x33754 palette acquire both call it.
 * PORT: the original never bounds the head; 0x1C470 drains the list every
 * frame, and the drain interval's record count is bounded by measurement, not
 * assumption. A temporary counter over the three faithful drivers gives a
 * maximum of 10 records between drains (the demo run; 7 in the attract and
 * title runs) against the 24 records that fit in
 * DS_00107498..DS_00107618 (the ownership table at DS_00107618 is next), so a
 * faithful path never reaches the bound. The test drivers drive game_frame()
 * and effects_step() without the loop's per-frame drain, which is what made
 * the head overflow before this guard (58 records between drains, 4 past the
 * list); they now drain at their fixture boundaries, and this guard is the
 * safety net for any future drain-less caller: a record that would write past
 * the list is dropped *loudly* — the original would corrupt the ownership table
 * and the globals above it — and the palette is re-recorded by the next
 * acquire. */
void palette_record(u32 ptr, u32 first, u32 count, u32 flag)
{
    u32 head = DSD(DS_00107798);
    /* PORT: an uninitialized head reads 0 and a stale one can sit outside the
     * record region; recover to the base exactly as gfx_flush_palette does, so
     * the two guards agree. */
    if (head < DS_00107498 || head > DS_00107618) {
        DSD(DS_00107798) = DS_00107498;
        head = DS_00107498;
    }
    if (head + 0x10u > DS_00107618) {
        fprintf(stderr, "gfx: palette dirty list full, dropping record "
                        "(ptr=%08x first=%x count=%x flag=%x)\n",
                ptr, first, count, flag);
        return;
    }
    DSD(head + 0) = ptr;
    DSD(head + 4) = first;
    DSD(head + 8) = count;
    DSD(head + 12) = flag;
    DSD(DS_00107798) = head + 16;
}

void gfx_flush_palette(void)
{
    u32 rec = DS_00107498;
    u32 head = DSD(0x107798);
    /* PORT: the head is valid only inside the record region (DS_00107498 up to
     * the ownership table at DS_00107618). A list that was never initialized
     * reads 0 and a stale head can sit outside it; either would walk past the
     * table. Treat such a head as empty. */
    if (head < DS_00107498 || head > DS_00107618) {
        DSD(DS_00107798) = DS_00107498;
        head = DS_00107498;
    }
    /* PORT: maps the original's `in(0x3DA) & 8` VBlank spin, run before the
     * dirty-list drain in FUN_0001C470, onto gfx_wait_vblank() -> host_pump(). */
    if (rec != head) gfx_wait_vblank();
    while (rec != head) {
        u32 ptr = DSD(rec + 0);
        u32 first = (u8)DSD(rec + 4);
        s32 count = (s32)DSD(rec + 8);
        u32 flag = DSD(rec + 12);

        if (first + count > 0x100) count = 0x100 - (s32)first;
        if ((u8)flag != 0) {
            /* PORT: the original walks its extended-memory block list to find
             * the handle's data; res_resolve() is the port's handle resolver.
             * It returns a host pointer, so subtract mem to get back the linear
             * offset DSD() expects, then skip the bank's u32 colour count. */
            const u8 *rp = res_resolve(ptr);
            if (!rp) {
                /* PORT: the original trusts the handle; the port skips a record
                 * whose handle does not resolve instead of reading from NULL. */
                DSD(rec + 4) = 0xFFFFFFFFu;
                rec += 16;
                continue;
            }
            ptr = (u32)(rp - mem) + 4;
        }
        /* PORT: the original writes the VGA DAC ports 0x3C8/0x3C9 (0x1C470:
         * `shr eax,2; out 0x3c9` twice more). The three channels sit at word
         * shifts 2/10/18, so the original's `out 0x3C9` — a 6-bit port — reads
         * the low 6 bits of each 8-bit channel and the VGA expands 6-bit to its
         * 8-bit display value as (v << 2) | (v >> 4). gfx_dac is the port's
         * model of that *displayed* 8-bit RGB (the same unit smk_palette_to
         * writes), so the port must truncate to 6 bits and expand. Reading the
         * full 8-bit channel without the truncation, and without the expansion,
         * is what rendered every game palette wrong; smk_palette_to already
         * supplies display values, so this path is the one that needed it. */
        for (s32 i = 0; i < count; i++) {
            u32 word = DSD(ptr + (u32)i * 4);
            u8 index = (u8)(first + (u32)i);
            u8 r = (u8)((word >> 2) & 0x3Fu), g = (u8)((word >> 10) & 0x3Fu);
            u8 b = (u8)((word >> 18) & 0x3Fu);
            gfx_dac[index][0] = (u8)((r << 2) | (r >> 4));
            gfx_dac[index][1] = (u8)((g << 2) | (g >> 4));
            gfx_dac[index][2] = (u8)((b << 2) | (b >> 4));
        }
        DSD(rec + 4) = 0xFFFFFFFFu;   /* the original marks the record consumed */
        rec += 16;
    }
    DSD(0x107798) = DS_00107498;
}

/* PORT: the original writes the game's 320x200 index buffer to the *literal*
 * address 0xA0000 (mov edi/ebx, 0xa0000) — a 64000-byte dword copy in the
 * master loop (0x255CC), or the dirty-dword blit (0x501A3), from
 * DAT_000E87A4. Task 13 verified that immediate is covered by no LE fixup, so
 * it is NOT a data-object address; the "VGA mode-13h aperture" reading is
 * inferred from that plus the mode-0x13 gate (spec: verified-with-caveat).
 * The port maps the LE data object flat at DATA_BASE 0x80000, so mem[0xA0000]
 * would alias
 * data-object offset 0x20000 — live engine tables (PTR_DAT_000A1290, the LUTs
 * at 0xA1420, …), never a screen. gfx_present() therefore must NOT write mem[]:
 * it converts the same indices through the DAC to RGB and hands the frame to
 * the host window. No behavioural change required. */
void gfx_present(const u8 *indices, int w, int h)
{
    static u8 rgb[320 * 200 * 3];
    if (w <= 0 || h <= 0 || (u32)w * (u32)h > sizeof rgb / 3) return;
    int n = w * h;
    for (int i = 0; i < n; i++) {
        const u8 *c = gfx_dac[indices[i]];
        rgb[i * 3 + 0] = c[0];
        rgb[i * 3 + 1] = c[1];
        rgb[i * 3 + 2] = c[2];
    }
    host_present_rgb(rgb, w, h);
}

void gfx_wait_vblank(void)
{
    /* PORT: the original spins on VGA status port 0x3DA bit 3; the port maps
     * the wait onto a host tick so the SDL host (Task 12) can pump events. */
    host_pump();
}
