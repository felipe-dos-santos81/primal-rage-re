#include "gfx.h"
#include "res.h"
#include "../host.h"
#include "../mem.h"
#include "../symbols.h"

u8 gfx_dac[256][3];

void gfx_flush_palette(void)
{
    u32 rec = DS_00107498;
    u32 head = DSD(0x107798);
    while (rec != head) {
        u32 ptr = DSD(rec + 0);
        u32 first = (u8)DSD(rec + 4);
        s32 count = (s32)DSD(rec + 8);
        u32 flag = DSD(rec + 12);

        if (first + count > 0x100) count = 0x100 - (s32)first;
        if ((u8)flag != 0) ptr = (u32)(uintptr_t)res_resolve(ptr) + 4;
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
    host_pump();
}
