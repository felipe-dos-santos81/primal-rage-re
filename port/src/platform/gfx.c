#include "gfx.h"
#include "res.h"
#include "../host.h"
#include "../mem.h"
#include "../symbols.h"
#include <stddef.h>

u8 gfx_dac[256][3];

void gfx_flush_palette(void)
{
    u32 rec = DS_00107498;
    u32 head = DSD(0x107798);
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
        /* PORT: the original writes the VGA DAC ports 0x3C8/0x3C9; the port
         * targets gfx_dac[] instead so gfx_present can convert indices to RGB. */
        for (s32 i = 0; i < count; i++) {
            u32 word = DSD(ptr + (u32)i * 4);
            u8 index = (u8)(first + (u32)i);
            gfx_dac[index][0] = (u8)(word >> 2);
            gfx_dac[index][1] = (u8)((word >> 10) & 0xFF);
            gfx_dac[index][2] = (u8)((word >> 18) & 0xFF);
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
