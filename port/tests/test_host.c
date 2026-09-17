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

    /* A pump with no elapsed interval must not invent ticks (the catch-up clamp
     * must preserve normal pacing). */
    u32 tprev = host_tick_count();
    host_pump();
    CHECK_EQ_INT((int)host_tick_count(), (int)tprev);

    /* File round-trip, over-max rejection, and a missing file. The over-max read
     * must fail cleanly and leave the destination untouched. */
    const char *path = "port_host_test.tmp";
    const u8 out[5] = { 'p', 'r', 'a', 'g', 'e' };
    u8 in[8];
    u32 n = 0;
    CHECK(host_write_file(path, out, sizeof out), "host_write_file succeeds");
    CHECK(host_read_file(path, in, sizeof in, &n), "host_read_file succeeds");
    CHECK_EQ_INT((int)n, (int)sizeof out);
    CHECK(in[0] == 'p' && in[4] == 'e', "file contents round-trip");
    for (size_t i = 0; i < sizeof in; i++) in[i] = 0xAB;
    CHECK(!host_read_file(path, in, 4, &n), "file larger than max fails");
    CHECK(in[0] == 0xAB && in[7] == 0xAB, "over-max read writes nothing");
    CHECK(!host_read_file("no/such/prage/file", in, sizeof in, &n),
          "missing file fails");

    /* A sparse file whose low 32 bits fit `max` must still be rejected: the
     * guard compares the real 64-bit size, not a truncated u32. The old
     * narrowing bug passed the guard here and ran fread(size) into `in`. */
    const char *big = "port_host_big.tmp";
    FILE *bf = fopen(big, "wb");
    if (bf) {
        if (fseek(bf, (long)(4294967296ll + 2 - 1), SEEK_SET) != 0 ||
            fputc(0, bf) == EOF) {
            fclose(bf);
        } else {
            fclose(bf);
            for (size_t i = 0; i < sizeof in; i++) in[i] = 0xCD;
            CHECK(!host_read_file(big, in, 4, &n),
                  "file whose low 32 bits fit max is still rejected");
            CHECK(in[0] == 0xCD && in[7] == 0xCD,
                  "sparse over-size read writes nothing");
        }
        remove(big);
    }
    remove(path);

    /* Audio seam (Task 5). The suite must never open a real device, so every
     * assertion here stays on the closed/no-device path: submit is a no-op and
     * open() of an impossible profile fails through the precondition guard
     * before SDL is touched. */
    host_audio_close();                                  /* before any open */
    CHECK_EQ_INT((int)host_audio_rate(), 0);
    const s16 audio[4] = { 0, 0, 0, 0 };
    host_audio_submit(audio, 2);                         /* no device: no-op */
    host_audio_submit(NULL, 2);
    host_audio_submit(audio, 0);
    host_audio_submit(audio, -1);                        /* negative count */
    CHECK_EQ_INT((int)host_audio_rate(), 0);             /* submits kept seam closed */
    host_audio_close();                                  /* safe after no-op submits */
    CHECK_EQ_INT((int)host_audio_rate(), 0);
    CHECK_EQ_INT(host_audio_open(0, 2), 0);              /* rate <= 0 */
    CHECK_EQ_INT(host_audio_open(44100, 0), 0);          /* channels <= 0 */
    CHECK_EQ_INT(host_audio_open(-44100, -2), 0);
    CHECK_EQ_INT((int)host_audio_rate(), 0);             /* still closed */
    host_audio_close();
    host_audio_close();                                  /* idempotent */
    CHECK_EQ_INT((int)host_audio_rate(), 0);

    /* host_shutdown() tears the audio seam down first, so afterwards every
     * audio entry point must still be a safe no-op reading rate 0. */
    host_shutdown();
    host_audio_submit(audio, 2);
    host_audio_close();
    CHECK_EQ_INT((int)host_audio_rate(), 0);

    return g_failures - before;
}
