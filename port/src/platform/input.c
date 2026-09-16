#include "input.h"
#include "../host.h"

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
