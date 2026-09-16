/* Host presentation seam. The platform layer calls these to hand frames and
 * pump events to the real host; SDL lives only in host.c/main.c (Task 12).
 * Until Task 12 supplies host.c, the weak no-op definitions below keep the
 * core and the test suite linking without SDL. Task 12 replaces this file. */
#ifndef PR_HOST_H
#define PR_HOST_H

#include "types.h"

/* Hands a w*h RGB24 frame (3 bytes per pixel, row-major) to the host. */
void host_present_rgb(const u8 *rgb, int w, int h);

/* Runs one host event/OS pump tick. The original's VBlank spin maps here. */
void host_pump(void);

__attribute__((weak)) void host_present_rgb(const u8 *rgb, int w, int h)
{
    (void)rgb;
    (void)w;
    (void)h;
}

__attribute__((weak)) void host_pump(void) {}

#endif /* PR_HOST_H */
