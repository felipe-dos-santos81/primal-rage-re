#include "host.h"
#include "test.h"
#include <stdio.h>
#include <time.h>

static unsigned long long now_ns(void)
{
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (unsigned long long)ts.tv_sec * 1000000000ull +
           (unsigned long long)ts.tv_nsec;
}

int test_host(void)
{
    int before = g_failures;

    /* host_pump()/host_present_rgb()/host_shutdown() before host_init(): the
     * suite runs headless with no window, so all three must be safe no-ops. */
    host_shutdown();
    host_pump();
    host_present_rgb((const u8 *)"\x01\x02\x03", 1, 1);

    /* host_init() reports failure instead of aborting. w <= 0 fails the guard
     * before SDL is touched, so the suite never opens a window. */
    CHECK_EQ_INT(host_init("pr-test", 0, 0), 0);
    host_shutdown();

    /* The real host advances the tick from a clock; the weak no-op left
     * host_tick_count() pinned at 0. host_wait_vblank() sleeps to the next 60 Hz
     * boundary then pumps, so a bounded loop must see the tick move. */
    u32 t0 = host_tick_count();
    unsigned long long deadline = now_ns() + 500ull * 1000000ull;
    while (host_tick_count() == t0 && now_ns() < deadline) host_wait_vblank();
    CHECK(host_tick_count() > t0, "host_wait_vblank advances host_tick_count");
    CHECK(host_tick_count() >= t0, "tick does not go backwards");

    /* File round-trip, over-max rejection, and a missing file. */
    const char *path = "port_host_test.tmp";
    const u8 out[5] = { 'p', 'r', 'a', 'g', 'e' };
    u8 in[8];
    u32 n = 0;
    CHECK(host_write_file(path, out, sizeof out), "host_write_file succeeds");
    CHECK(host_read_file(path, in, sizeof in, &n), "host_read_file succeeds");
    CHECK_EQ_INT((int)n, (int)sizeof out);
    CHECK(in[0] == 'p' && in[4] == 'e', "file contents round-trip");
    CHECK(!host_read_file(path, in, 4, &n), "file larger than max fails");
    CHECK(!host_read_file("no/such/prage/file", in, sizeof in, &n),
          "missing file fails");
    remove(path);

    return g_failures - before;
}
