/* Shallow checks for the AIL call surface (Task 10). The end-to-end wiring is
 * Task 11's and the announcer sample is Task 12's; this file only pins the
 * surface's observable contracts: the fixed-profile short-circuit, the
 * preference/timer handle semantics, that music start/stop drives the
 * sequencer, and that a sample-play call converts its bytes and reaches the
 * mixer. No audio device is opened. */
#include "test.h"

#include "platform/audio/ail.h"
#include "platform/audio/mixer.h"
#include "platform/audio/opl/opl.h"
#include "platform/audio/samples.h"
#include "platform/audio/sequencer.h"

/* A minimal XMIDI bank: FORM/XMID with one EVNT chunk holding a note-on (with a
 * 0x10-tick delta before the end meta) so the sequence stays playing for a
 * tick, then the FF 2F end. Sizes are big-endian, as the container demands. */
static const u8 k_bank[] = {
    'F', 'O', 'R', 'M', 0x00, 0x00, 0x00, 0x14,
    'X', 'M', 'I', 'D',
    'E', 'V', 'N', 'T', 0x00, 0x00, 0x00, 0x08,
    0x90, 0x3C, 0x64, 0x7F,       /* note on, ch0, note 0x3C, vel 0x64, dur 127 */
    0x10,                         /* delta: 16 ticks before the next event */
    0xFF, 0x2F, 0x00              /* XMIDI end */
};

static s16 out[2 * 64];

static int all_zero(const s16 *b, int n)
{
    for (int i = 0; i < n; i++)
        if (b[i] != 0)
            return 0;
    return 1;
}

static void timer_cb(void) {}

int test_ail(void)
{
    int before = g_failures;

    /* 1. Defaults and the preference swap semantics (spec row 3): setting
     *    returns the old value, and an out-of-range index is rejected. */
    AIL_startup();
    CHECK_EQ_INT(AIL_set_preference(4, 2), 0x10);
    CHECK_EQ_INT(AIL_set_preference(0x12, 1), -1);

    /* 2. Fixed profile, no hardware probe (spec rows 8, 9, 25): the fallback
     *    chain's names all succeed, and so does a name that cannot exist,
     *    proving no driver file is probed or opened. */
    HDIGDRIVER dig = AIL_install_DIG_INI();
    CHECK(dig != NULL, "install_DIG_INI returns a fixed-profile handle");
    CHECK(AIL_install_DIG_driver_file("SB16.DIG", NULL) != NULL,
          "fixed profile: SB16.DIG install succeeds");
    CHECK(AIL_install_DIG_driver_file("SBPRO.DIG", NULL) != NULL,
          "fixed profile: SBPRO.DIG install succeeds");
    CHECK(AIL_install_DIG_driver_file("SBLASTER.DIG", NULL) != NULL,
          "fixed profile: SBLASTER.DIG install succeeds");
    CHECK(AIL_install_DIG_driver_file("NO_SUCH.DRV", NULL) != NULL,
          "fixed profile: no driver-file probe (bogus name succeeds)");
    HMDIDRIVER mdi = AIL_install_MDI_INI();
    CHECK(mdi != NULL, "install_MDI_INI returns a fixed-profile handle");

    /* 3. Timer handles (spec rows 4-7): distinct slots, a released slot is
     *    reused. The callback is stored, never fired (no ISR in the port). */
    HTIMER t0 = AIL_register_timer(timer_cb);
    HTIMER t1 = AIL_register_timer(timer_cb);
    CHECK(t0 >= 0 && t1 >= 0 && t1 != t0, "timer handles are distinct slots");
    AIL_set_timer_frequency(t0, 0x3c);
    AIL_start_timer(t0);
    AIL_release_timer_handle(t0);
    CHECK(AIL_register_timer(timer_cb) == t0, "a released timer slot is reused");
    AIL_release_timer_handle(t1);

    /* 4. Music start/stop drive the sequencer (spec rows 27-29, 31). Start
     *    emits the sequencer's opening register writes and reports playing (4);
     *    one tick keys the bank's note; stop halts it and reports 2; no further
     *    tick writes anything. */
    HSEQUENCE seq = AIL_allocate_sequence_handle(mdi);
    CHECK(seq != NULL, "allocate_sequence_handle returns the sequence");
    CHECK(AIL_allocate_sequence_handle(mdi) == NULL,
          "the single sequence handle is exhausted on a second allocate");
    CHECK_EQ_INT(AIL_init_sequence(seq, k_bank, 0), 1);
    opl_reset();
    AIL_start_sequence(seq);
    u32 w0 = opl_write_count();
    CHECK(w0 >= 2, "start_sequence emits the sequencer opening writes");
    CHECK_EQ_INT(AIL_sequence_status(seq), 4);
    seq_tick();
    CHECK_EQ_INT(seq_active_track(), 1);
    CHECK(opl_write_count() > w0, "a tick emits the note's register writes");
    CHECK_EQ_INT(AIL_sequence_status(seq), 4);
    AIL_stop_sequence(seq);
    CHECK_EQ_INT(seq_active_track(), 0);
    CHECK_EQ_INT(AIL_sequence_status(seq), 2);
    {
        u32 w1 = opl_write_count();
        for (int i = 0; i < 8; i++)
            seq_tick();
        CHECK_EQ_INT((int)opl_write_count(), (int)w1);
    }

    /* 4b. Natural end (spec row 31): the bank's FF 2F end meta halts playback,
     *     so AIL_sequence_status reports 2 with no AIL_stop_sequence call. */
    AIL_start_sequence(seq);
    CHECK_EQ_INT(AIL_sequence_status(seq), 4);
    for (int i = 0; i < 40; i++)
        seq_tick();
    CHECK_EQ_INT(AIL_sequence_status(seq), 2);

    /* 4c. A failed re-init clears `loaded`: a later start_sequence must not
     *     restart the previously loaded bank. */
    {
        static const u8 bad_bank[4] = { 0 };
        CHECK_EQ_INT(AIL_init_sequence(seq, bad_bank, 0), 0);
        AIL_start_sequence(seq);
        CHECK_EQ_INT(AIL_sequence_status(seq), 2);
    }

    /* 5. The 8-bit -> s16 conversion is exact and lives once, in samples.c. */
    {
        static const u8 pcm8[3] = { 0, 128, 255 };
        s16 conv[3];
        CHECK_EQ_INT(samples_to_s16(pcm8, 3, conv), 3);
        CHECK_EQ_INT(conv[0], -32768);
        CHECK_EQ_INT(conv[1], 0);
        CHECK_EQ_INT(conv[2], 32512);
        CHECK_EQ_INT(samples_to_s16(NULL, 3, conv), 0);
        CHECK_EQ_INT(samples_to_s16(pcm8, 3, NULL), 0);
    }

    /* 6. The sample-play path: four handles, then exhaustion; start marks
     *    playing and adds a mixer voice of the converted bytes, so rendering
     *    is non-silent; stop returns to stopped. */
    {
        HSAMPLE hs[4];
        for (int i = 0; i < 4; i++) {
            hs[i] = AIL_allocate_sample_handle(dig);
            CHECK(hs[i] != NULL, "one of the four sample handles allocates");
        }
        CHECK(AIL_allocate_sample_handle(dig) == NULL,
              "a fifth sample handle is refused (four-handle pool)");

        static const u8 pcm[4] = { 0, 0, 0, 0 };   /* converted: all -32768 */
        AIL_init_sample(hs[0]);
        CHECK_EQ_INT(AIL_sample_status(hs[0]), 2);
        AIL_set_sample_address(hs[0], pcm, 4);
        AIL_set_sample_type(hs[0], 0, 0);
        AIL_set_sample_rate(hs[0], 11025);
        AIL_set_sample_volume(hs[0], 0x7f);
        AIL_set_sample_loop_count(hs[0], 0);

        /* hs[1] plays a tone so that stopping hs[0] can be shown not to stop it
         * (the original stops one handle, not every sample voice). */
        static const u8 tone8[4] = { 200, 56, 200, 56 };
        AIL_init_sample(hs[1]);
        AIL_set_sample_address(hs[1], tone8, 4);
        AIL_set_sample_rate(hs[1], 11025);
        AIL_set_sample_volume(hs[1], 0x7f);
        AIL_set_sample_loop_count(hs[1], 1);

        mixer_reset();                 /* silence OPL so only the voices are heard */
        AIL_start_sample(hs[0]);
        CHECK_EQ_INT(AIL_sample_status(hs[0]), 4);
        AIL_start_sample(hs[1]);
        CHECK_EQ_INT(AIL_sample_status(hs[1]), 4);
        mixer_render(out, 64, 44100);
        CHECK(!all_zero(out, 2 * 64),
              "start_sample adds a mixer voice of the converted sample");

        AIL_stop_sample(hs[0]);
        CHECK_EQ_INT(AIL_sample_status(hs[0]), 2);
        CHECK_EQ_INT(AIL_sample_status(hs[1]), 4);
        mixer_render(out, 64, 44100);
        CHECK(!all_zero(out, 2 * 64),
              "stop_sample stops one handle's voice, not every sample voice");

        AIL_stop_sample(hs[1]);
        CHECK_EQ_INT(AIL_sample_status(hs[1]), 2);
        mixer_render(out, 64, 44100);
        CHECK(all_zero(out, 2 * 64), "stopping the last sample voice is silence");

        /* Volume 0x7f maps to the mixer's unity (256), not 254, so the
         * converted byte passes through unchanged: 200 -> (200-128)<<8. */
        static const u8 one8[1] = { 200 };
        mixer_reset();
        AIL_init_sample(hs[0]);
        AIL_set_sample_address(hs[0], one8, 1);
        AIL_set_sample_rate(hs[0], 44100);   /* == out_rate: no resampling */
        AIL_set_sample_volume(hs[0], 0x7f);
        AIL_set_sample_loop_count(hs[0], 0);
        AIL_start_sample(hs[0]);
        mixer_render(out, 1, 44100);
        CHECK_EQ_INT(out[0], 18432);
        CHECK_EQ_INT(out[1], 18432);
        AIL_stop_sample(hs[0]);
        AIL_release_sample_handle(hs[0]);
        AIL_release_sample_handle(hs[1]);

        /* An uninitialised handle reports 0, not a stale status. */
        CHECK_EQ_INT(AIL_sample_status(NULL), 0);
    }

    /* 7. Shutdown releases everything and the pool is reusable. */
    AIL_shutdown();
    CHECK_EQ_INT(AIL_sample_status(NULL), 0);
    CHECK(AIL_allocate_sample_handle(dig) != NULL,
          "sample handles are reusable after shutdown");

    return g_failures - before;
}
