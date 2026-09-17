#include "platform/audio/sequencer.h"
#include "platform/audio/patches.h"
#include "platform/audio/mixer.h"
#include "platform/audio/opl/opl.h"
#include "test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Builds a minimal FORM/XMID/EVNT XMI bank around `ev`, so the halt paths can
 * be exercised without assets. Layout matches what seq_load walks. */
static u32 wr_be32(u8 *p, u32 v)
{
    p[0] = (u8)(v >> 24); p[1] = (u8)(v >> 16); p[2] = (u8)(v >> 8); p[3] = (u8)v;
    return 4;
}

static u32 build_xmi(u8 *buf, const u8 *ev, u32 n)
{
    u32 chunk = 8 + n + (n & 1u);
    u32 fsz = 4 + chunk;
    u32 len = 8 + fsz;

    buf[0] = 'F'; buf[1] = 'O'; buf[2] = 'R'; buf[3] = 'M';
    wr_be32(buf + 4, fsz);
    buf[8] = 'X'; buf[9] = 'M'; buf[10] = 'I'; buf[11] = 'D';
    buf[12] = 'E'; buf[13] = 'V'; buf[14] = 'N'; buf[15] = 'T';
    wr_be32(buf + 16, n);
    for (u32 i = 0; i < n; i++)
        buf[20 + i] = ev[i];
    if (n & 1u)
        buf[20 + n] = 0;
    return len;
}

/* The shipped title music bank: a FORM XDIR / CAT XMID container at file offset
 * 221446 (0x36106) in S16TITLE.GRA (port/spec/audio.md "Data locations"). The
 * XMID FORM it holds is at 0x36128. This is a shallow smoke test: it proves the
 * sequencer accepts the real bank, ticks, and drives the OPL core; the
 * byte-exact comparison against the capture is Task 9's job. */
#define TITLE_GRA "data/game/C/S16TITLE.GRA"
#define TITLE_XMI_OFF 221446u
#define FAT_OPL "data/game/C/FAT.OPL"

static u8 *read_file(const char *path, u32 *len)
{
    FILE *f = fopen(path, "rb");
    long n;
    u8 *buf;

    *len = 0;
    if (!f)
        return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    n = ftell(f);
    if (n <= 0) { fclose(f); return NULL; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }
    buf = (u8 *)malloc((size_t)n);
    if (!buf) { fclose(f); return NULL; }
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) { free(buf); fclose(f); return NULL; }
    fclose(f);
    *len = (u32)n;
    return buf;
}

static int any_nonzero(const s16 *b, int n)
{
    for (int i = 0; i < n; i++)
        if (b[i] != 0)
            return 1;
    return 0;
}

/* --- Task 9 oracle comparison ------------------------------------------- */

#define OPL_SEQ_PY "tools/opl_seq.py"
#define OPL_TRACE_PY "tools/opl_trace.py"
#define CAPTURE_DRO "data/audio-captures/prage_000.dro"

/* One normalised (tick, reg, value) write. */
typedef struct {
    u32 tick;
    u16 reg;
    u8 val;
} ev_t;

/* Runs `cmd` and parses its "tick reg value" lines (opl_seq.py / opl_trace.py
 * share this format). Fills up to `cap` events; `total` always gets the full
 * count. Returns 0 on success. */
static int read_ev_stream(const char *cmd, ev_t *out, int cap, u32 *total)
{
    FILE *p = popen(cmd, "r");
    char line[128];
    int n = 0;

    *total = 0;
    if (p == NULL)
        return -1;
    while (fgets(line, sizeof line, p) != NULL) {
        unsigned t, r, v;
        if (sscanf(line, "%u %x %x", &t, &r, &v) != 3)
            continue;
        if (n < cap) {
            out[n].tick = t;
            out[n].reg = (u16)r;
            out[n].val = (u8)v;
        }
        n++;
    }
    (void)pclose(p);
    *total = (u32)n;
    return 0;
}

static int ev_eq(const ev_t *a, const ev_t *b)
{
    return a->tick == b->tick && a->reg == b->reg && a->val == b->val;
}

/* Registers the port deliberately does not reproduce byte-for-byte against the
 * capture (port/spec/audio.md "Known capture divergences"): the OPL rhythm
 * register 0xBD, and the carrier TL family whose driver velocity offset is not
 * a pure function of velocity. */
static int documented_excluded(u16 reg)
{
    static const u8 carrier_tl[9] = { 0x43, 0x44, 0x45, 0x4B, 0x4C, 0x4D, 0x53, 0x54, 0x55 };
    u8 lo = (u8)(reg & 0xFF);

    if (lo == 0xBD)
        return 1;
    for (int i = 0; i < 9; i++)
        if (lo == carrier_tl[i])
            return 1;
    return 0;
}

/* Runs the C sequencer for `ticks` ticks, tagging each write with the tick it
 * was made on (seq_start's writes are tick 0). */
static int capture_c_stream(ev_t *out, int cap, u32 ticks)
{
    u32 prev, tick;
    int n = 0;

    opl_reset();
    seq_start();
    for (u32 k = 0; k < opl_write_count() && n < cap; k++) {
        out[n].tick = 0;
        out[n].reg = opl_trace_reg(k);
        out[n].val = opl_trace_val(k);
        n++;
    }
    prev = opl_write_count();
    for (tick = 1; tick <= ticks; tick++) {
        seq_tick();
        for (u32 k = prev; k < opl_write_count() && n < cap; k++) {
            out[n].tick = tick;
            out[n].reg = opl_trace_reg(k);
            out[n].val = opl_trace_val(k);
            n++;
        }
        prev = opl_write_count();
    }
    return n;
}

int test_sequencer(void)
{
    int before = g_failures;
    u8 *gra = NULL, *fat = NULL;
    u32 gra_len = 0, fat_len = 0;
    const u8 *xmi;
    u32 xmi_len;

    /* Rejections that need no asset: NULL/empty and a non-XMIDI buffer. These
     * also hold before any successful load, which must stay rejected. */
    {
        static const u8 junk[24] = { 'N', 'O', 'T', 'A', 'M', 'U', 'S', 'I',
                                     'C', 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
        CHECK_EQ_INT(seq_load(NULL, 0), 0);
        CHECK_EQ_INT(seq_load(junk, sizeof junk), 0);
        CHECK_EQ_INT(seq_load(NULL, 100), 0);
    }
    CHECK_EQ_INT(patches_load(NULL, 0), 0);
    {
        static const u8 junk[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
        CHECK_EQ_INT(patches_load(junk, sizeof junk), 0);
    }
    CHECK_EQ_INT(patches_count(), 0);

    /* 0. Halt invariants (synthetic banks, no assets): every stop path must
     *    release keyed voices, so seq_active_track() reaches 0 and no further
     *    OPL writes follow. Regression: the error paths only cleared the
     *    playing flag, leaving channels keyed on forever. */
    {
        u8 bank[64];
        u32 len;

        /* 0a. A parse overrun (truncated 0x90) after a note keys off. */
        {
            static const u8 ev[] = { 0x90, 0x30, 0x40, 0x7f, 0x05, 0x90, 0x33 };
            opl_reset();
            len = build_xmi(bank, ev, sizeof ev);
            CHECK_EQ_INT(seq_load(bank, len), 1);
            seq_start();
            seq_tick();
            CHECK_EQ_INT(seq_active_track(), 1);
            for (int i = 0; i < 10 && seq_active_track() > 0; i++)
                seq_tick();
            CHECK_EQ_INT(seq_active_track(), 0);
            {
                u32 w = opl_write_count();
                for (int i = 0; i < 10; i++)
                    seq_tick();
                CHECK_EQ_INT(opl_write_count(), w);
            }
        }

        /* 0b. An unknown status byte after a note keys off. */
        {
            static const u8 ev[] = { 0x90, 0x30, 0x40, 0x7f, 0x05, 0xf1 };
            opl_reset();
            len = build_xmi(bank, ev, sizeof ev);
            CHECK_EQ_INT(seq_load(bank, len), 1);
            seq_start();
            seq_tick();
            CHECK_EQ_INT(seq_active_track(), 1);
            for (int i = 0; i < 10 && seq_active_track() > 0; i++)
                seq_tick();
            CHECK_EQ_INT(seq_active_track(), 0);
        }

        /* 0c. Loading a new bank releases the previous bank's keyed voices and
         *     leaves no stale release for the next tick. */
        {
            static const u8 a[] = { 0x90, 0x30, 0x40, 0x7f, 0x05 };
            static const u8 b[] = { 0x20, 0x90, 0x40, 0x40, 0x7f, 0x05 };
            u8 bank_b[64];
            u32 len_b;

            opl_reset();
            len = build_xmi(bank, a, sizeof a);
            len_b = build_xmi(bank_b, b, sizeof b);
            CHECK_EQ_INT(seq_load(bank, len), 1);
            seq_start();
            seq_tick();
            CHECK_EQ_INT(seq_active_track(), 1);
            {
                u32 before = opl_write_count();
                CHECK_EQ_INT(seq_load(bank_b, len_b), 1);
                CHECK_EQ_INT(seq_active_track(), 0);
                CHECK(opl_write_count() > before, "load keys off previous voices");
            }
            {
                u32 w = opl_write_count();
                seq_tick();
                seq_tick();
                CHECK_EQ_INT(opl_write_count(), w);
                CHECK_EQ_INT(seq_active_track(), 0);
            }
        }

        /* 0d. Restarting while a note sounds releases it too. */
        {
            static const u8 a[] = { 0x90, 0x30, 0x40, 0x7f, 0x05 };
            opl_reset();
            len = build_xmi(bank, a, sizeof a);
            CHECK_EQ_INT(seq_load(bank, len), 1);
            seq_start();
            seq_tick();
            CHECK_EQ_INT(seq_active_track(), 1);
            {
                u32 before = opl_write_count();
                seq_start();
                CHECK_EQ_INT(seq_active_track(), 0);
                CHECK(opl_write_count() > before, "restart keys off sounding notes");
            }
        }
    }

    gra = read_file(TITLE_GRA, &gra_len);
    fat = read_file(FAT_OPL, &fat_len);
    if (gra == NULL || fat == NULL || gra_len <= TITLE_XMI_OFF) {
        free(gra);
        free(fat);
        if (getenv("PR_ORACLE_REQUIRED")) {
            CHECK(0, "PR_ORACLE_REQUIRED=1 but S16TITLE.GRA/FAT.OPL is missing");
        } else {
            printf("SKIP sequencer real-data checks — including the governing "
                   "C-vs-Python byte gate (need untracked " TITLE_GRA
                   " @%u and " FAT_OPL ")\n", TITLE_XMI_OFF);
        }
        return g_failures - before;
    }

    xmi = gra + TITLE_XMI_OFF;
    xmi_len = gra_len - TITLE_XMI_OFF;

    /* The recorded offset really is the XMI bank container. */
    CHECK(memcmp(xmi, "FORM", 4) == 0, "recorded offset is a FORM chunk");

    /* The music path loads the patch bank first (AIL init), so the sequencer's
     * program changes resolve. Without it a key-on carries no operator setup. */
    CHECK_EQ_INT(patches_load(fat, fat_len), 1);
    CHECK_EQ_INT(patches_count(), 181);

    /* 1. Real bank loads; nothing is sounding before start. */
    CHECK_EQ_INT(seq_load(xmi, xmi_len), 1);
    CHECK_EQ_INT(seq_active_track(), 0);

    /* 2. Truncated forms of the same bank are rejected and leave the loaded
     *    bank playable (the loader keeps its state on failure). */
    CHECK_EQ_INT(seq_load(xmi, 32), 0);
    CHECK_EQ_INT(seq_load(xmi, 64), 0);

    /* 3. Ticking the real bank keys notes on and reaches the OPL core. The OPL
     *    core advances only when rendered, so render a little each tick and
     *    watch for non-silence: it can appear only if register writes landed. */
    mixer_reset();
    seq_start();
    {
        static s16 out[2 * 64];
        int heard = 0, saw_active = 0;
        for (int i = 0; i < 400; i++) {
            seq_tick();
            if (seq_active_track() > 0)
                saw_active = 1;
            for (int k = 0; k < 2 * 64; k++)
                out[k] = 0;
            mixer_render(out, 64, 44100);
            if (any_nonzero(out, 2 * 64))
                heard = 1;
        }
        CHECK(saw_active, "ticking the title bank keys notes on");
        CHECK(heard, "sequencer output reaches the OPL core");
    }

    /* 4. Stop silences: no active voices and ticking further stays silent. */
    seq_stop();
    CHECK_EQ_INT(seq_active_track(), 0);
    for (int i = 0; i < 200; i++)
        seq_tick();
    CHECK_EQ_INT(seq_active_track(), 0);

    /* 5. FAT.OPL decodes (loaded above): melodic and percussion keys resolve
     *    to their payloads, absent keys do not. */
    {
        const u8 *mel = patches_lookup(PATCH_KEY(PATCH_BANK_MELODIC, 0));
        const u8 *drum = patches_lookup(PATCH_KEY(PATCH_BANK_PERCUSSION, 0x2d));
        CHECK(mel != NULL && mel[0] == 0x0e, "melodic patch payload decodes");
        CHECK(drum != NULL && drum[0] == 0x0e, "percussion patch payload decodes");
        CHECK(patches_lookup(0x1234u) == NULL, "absent patch key resolves to NULL");
    }

    /* 6. A truncated bank is rejected without discarding the loaded one. */
    CHECK_EQ_INT(patches_load(fat, 100), 0);
    CHECK_EQ_INT(patches_count(), 181);

    /* 7. Task 9 oracle: the C register stream must equal tools/opl_seq.py's
     *    byte for byte — same tick, register, value and order. Governing
     *    oracle, no tolerance. */
    {
        static ev_t c_ev[OPL_TRACE_MAX];
        static ev_t py_ev[OPL_TRACE_MAX];
        static char cmd[256];
        u32 py_total = 0;
        int c_n;

        CHECK_EQ_INT(patches_load(fat, fat_len), 1);
        CHECK_EQ_INT(seq_load(xmi, xmi_len), 1);
        c_n = capture_c_stream(c_ev, (int)OPL_TRACE_MAX, 4096);
        CHECK(!opl_trace_overflow(), "C register stream fits the trace seam");

        snprintf(cmd, sizeof cmd, "python3 %s %s --trace", OPL_SEQ_PY, TITLE_GRA);
        CHECK_EQ_INT(read_ev_stream(cmd, py_ev, (int)OPL_TRACE_MAX, &py_total), 0);
        CHECK_EQ_INT((long)c_n, (long)py_total);
        if (c_n == (int)py_total) {
            for (int i = 0; i < c_n; i++) {
                if (!ev_eq(&c_ev[i], &py_ev[i])) {
                    printf("ORACLE C-vs-Python first difference at write %d: "
                           "C tick=%u reg=%#04x val=%#04x, python tick=%u reg=%#04x val=%#04x\n",
                           i, c_ev[i].tick, c_ev[i].reg, c_ev[i].val,
                           py_ev[i].tick, py_ev[i].reg, py_ev[i].val);
                    CHECK(0, "C register stream equals the Python oracle byte-for-byte");
                    break;
                }
            }
        } else {
            printf("ORACLE C-vs-Python count differs: C=%d python=%u\n", c_n, py_total);
        }
        if (c_n == (int)py_total) {
            int bad = 0;
            for (int i = 0; i < c_n; i++)
                if (!ev_eq(&c_ev[i], &py_ev[i]))
                    bad = 1;
            if (!bad)
                printf("oracle C-vs-Python: %d writes byte-exact\n", c_n);
        }

        /* 8. Capture oracle (informational). The capture is the real driver,
         *    which the port reconstructs rather than reproduces: its
         *    cached-state init block, per-patch operator application and
         *    channel reuse are not modelled. Compare under the documented
         *    normalisation (capture ms -> port tick at 120 Hz, starting at the
         *    first key-on; the carrier-TL and 0xBD divergences excluded) and
         *    print the first remaining difference for the Task 9 report. It
         *    does not fail the suite on a known divergence. */
        {
            static ev_t cap_ev[OPL_TRACE_MAX];
            u32 cap_total = 0, w = 0, pyi = 0;
            u32 first_key = 0;
            int diff = -1;

            snprintf(cmd, sizeof cmd, "python3 %s %s", OPL_TRACE_PY, CAPTURE_DRO);
            if (read_ev_stream(cmd, cap_ev, (int)OPL_TRACE_MAX, &cap_total) != 0 || cap_total == 0) {
                /* Two causes read alike here: the capture may be absent, or it
                 * is present but nothing decoded (no python3 on PATH, or
                 * opl_trace.py failed). Say which, so the reader is not sent to
                 * the wrong place. */
                FILE *cf = fopen(CAPTURE_DRO, "rb");
                if (cf == NULL) {
                    printf("SKIP capture oracle — capture file missing: %s "
                           "(see \"Recorded capture\" in port/spec/audio.md)\n",
                           CAPTURE_DRO);
                } else {
                    fclose(cf);
                    printf("SKIP capture oracle — %s present but no stream "
                           "decoded: is python3 on PATH and " OPL_TRACE_PY
                           " runnable?\n", CAPTURE_DRO);
                }
            } else {
                for (u32 i = 0; i < cap_total; i++)
                    if (cap_ev[i].reg >= 0xB0 && cap_ev[i].reg <= 0xB8 &&
                        (cap_ev[i].val & 0x20)) {
                        first_key = i;
                        break;
                    }
                for (u32 i = first_key; i < cap_total; i++) {
                    if (documented_excluded(cap_ev[i].reg))
                        continue;
                    cap_ev[w].tick = (cap_ev[i].tick * 120u + 500u) / 1000u + 60u;
                    cap_ev[w].reg = cap_ev[i].reg;
                    cap_ev[w].val = cap_ev[i].val;
                    w++;
                }
                /* Lockstep walk, skipping the port's tick-0 init and the
                 * documented-excluded registers on both sides. Stop at the
                 * first difference or when either stream is exhausted: a stream
                 * that merely ended must not read as "all matched" — an
                 * uncompared capture tail is a real result, not a pass. */
                {
                    u32 ci = 0, c_tail = 0;
                    for (;;) {
                        while (ci < (u32)c_n &&
                               (c_ev[ci].tick == 0 || documented_excluded(c_ev[ci].reg)))
                            ci++;
                        if (ci >= (u32)c_n || pyi >= w)
                            break;
                        if (!ev_eq(&c_ev[ci], &cap_ev[pyi])) {
                            diff = (int)ci;
                            break;
                        }
                        ci++;
                        pyi++;
                    }
                    for (u32 k = ci; k < (u32)c_n; k++)
                        if (c_ev[k].tick != 0 && !documented_excluded(c_ev[k].reg))
                            c_tail++;
                    if (diff >= 0)
                        printf("capture oracle first difference at C write %d: "
                               "C tick=%u reg=%#04x val=%#04x vs capture tick=%u reg=%#04x val=%#04x "
                               "(C %d writes, capture %u normalised)\n",
                               diff, c_ev[diff].tick, c_ev[diff].reg, c_ev[diff].val,
                               cap_ev[pyi].tick, cap_ev[pyi].reg, cap_ev[pyi].val, c_n, w);
                    else if (c_tail == 0 && pyi == w)
                        printf("capture oracle: %u writes normalised vs C, all "
                               "compared and matched\n", w);
                    else
                        printf("capture oracle: %u compared/matched, %u C-only, "
                               "%u capture-only tail (not byte-exact; C %d writes, "
                               "capture %u normalised)\n",
                               pyi, c_tail, w - pyi, c_n, w);
                }
            }
        }
    }

    free(gra);
    free(fat);
    return g_failures - before;
}
