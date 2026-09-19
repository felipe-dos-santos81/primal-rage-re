#include "input.h"
#include "../host.h"
#include "../mem.h"
#include "../symbols.h"

static u16 g_keys[INPUT_QUEUE_CAP];
static int g_head;  /* index of the oldest queued key */
static int g_count; /* entries currently queued, 0..INPUT_QUEUE_CAP */

void input_clear(void)
{
    g_head = 0;
    g_count = 0;
}

void input_push(u8 scan, u8 ascii)
{
    if (g_count == INPUT_QUEUE_CAP) {
        /* PORT: the full BIOS buffer stops accepting keys; the port instead
         * folds the drop onto the oldest entry so recent input survives. */
        g_head = (g_head + 1) % INPUT_QUEUE_CAP;
        g_count--;
    }
    int tail = (g_head + g_count) % INPUT_QUEUE_CAP;
    g_keys[tail] = (u16)(((u16)scan << 8) | (u16)ascii);
    g_count++;
}

int input_has_key(void)
{
    return g_count > 0;
}

u16 input_get_key(void)
{
    while (g_count == 0) host_pump();
    u16 key = g_keys[g_head];
    g_head = (g_head + 1) % INPUT_QUEUE_CAP;
    g_count--;
    return key;
}

int input_check_key(void)
{
    return g_count > 0 ? (int)g_keys[g_head] : 0;
}

/* 0x2D2F0. `xor eax,eax; ret`. */
u32 input_joystick_device(u32 selector)
{
    (void)selector;
    return 0u;
}

/* 0x50161. Level/latch bit selector. */
u32 input_select_bits(u32 mask)
{
    u32 level = DSD(DS_000E1C34);
    if (mask != 0u) {
        u32 latch = DSD(DS_000E1C38);
        u32 edge = (latch ^ level) & mask;      /* 0x50174/0x50176 */
        DSD(DS_000E1C38) = latch | edge;        /* 0x50178 */
        level = (level & ~mask) | edge;         /* 0x5017E/0x50180/0x50182 */
    }
    return level;
}

/* 0x500C4. The key bytes are combined as (byte[+0x2d8] << 24) |
 * (byte[+0x2d9] << 8), so the level word only ever occupies the 0xFF00FF00
 * positions. The new level keeps the previous value for every bit that changed
 * this frame - the raw's one-frame debounce. */
u32 input_pump(void)
{
    const u8 *k = mem + DSD(DS_00101514);
    u32 cur = ((u32)k[0x2d8] << 24) | ((u32)k[0x2d9] << 8);
    u32 changed = DSD(DS_000E1C30) ^ cur;
    DSD(DS_000E1C30) = cur;
    u32 level = DSD(DS_000E1C34);
    u32 newlevel = (~changed & cur) | (changed & level);
    DSD(DS_000E1C34) = newlevel;
    DSD(DS_000E1C38) &= newlevel;

    u32 rpt = DSD(DS_000E1C3C);
    if ((changed & level & rpt) != 0u || (newlevel & rpt) == 0u) {
        DSW(DS_000E1C40) = DSW(DS_000E1C42);            /* 0x50130 */
    } else if (--DSW(DS_000E1C40) == 0u) {              /* 0x5010F */
        DSW(DS_000E1C40) = DSW(DS_000E1C44);            /* 0x50118 */
        DSD(DS_000E1C38) &= ~rpt;                       /* 0x50128 */
    }
    return DSD(DS_000E1C34);
}

/* 0x4F644. The first selector uses the 0xFF00FF00 family (the bits the level
 * word can carry) and yields the newly-pressed mask; the second uses the
 * complementary family and keeps only the level half, yielding the held mask.
 * Both share the DAT_000E1C38 latch, in this order. */
void input_state_update(void)
{
    DSD(DS_001088E4) = input_select_bits(0xFF00FF00u);              /* 0x4F64A */
    DSD(DS_001088D8) = input_select_bits(0x00FF00FFu) & 0xFF00FF00u;/* 0x4F659 */
    DSD(DS_001088DC) = input_joystick_device(0xFFu);                /* 0x4F66D */
    DSD(DS_001088D4) = input_joystick_device(0xFFFFFF00u) & 0xFFu;  /* 0x4F67C */
    if ((DSB(DS_001088D4) & 2u) != 0u)                              /* 0x4F68B */
        DSD(DS_001088E4) |= DSD(DS_001088D8);
    DSW(DS_001088E0) = (u16)(((DSD(DS_001088E4) & 0xFF000000u) >> 24) |
                             ((DSD(DS_001088D8) & 0xFF000000u) >> 16));
    DSW(DS_001088E2) = (u16)(((DSD(DS_001088E4) & 0x0000FF00u) >> 8) |
                             (DSD(DS_001088D8) & 0x0000FF00u));
}
