/* XMIDI sequencer -> OPL register writes. See sequencer.h for the contract and
 * the tick evidence, port/spec/audio.md for the event grammar.
 *
 * The original sent MIDI events to the loaded SBPRO2.MDI driver and the driver
 * wrote the OPL registers. This module stands in for that driver: it owns a
 * fixed 9-channel OPL voice pool, looks programs up in the FAT.OPL bank, and
 * writes the same register families the capture shows. Voice stealing and the
 * per-voice decode are the port's reconstruction, not the driver's code; the
 * parts the capture pins down (patch byte layout, tick, fnum/block) are noted
 * at their use.
 */
#include <stddef.h>

#include "sequencer.h"
#include "patches.h"
#include "opl/opl.h"

#define SEQ_OPL_CHANNELS 9
#define SEQ_MIDI_CHANNELS 16
#define SEQ_NOTE_FREE (-1)
#define SEQ_NO_PATCH 0xFFFFu

/* OPL2 operator register slot for each channel: 0x20+slot, 0x40+slot, ...
 * (channels 0-2 -> 0..2, 3-5 -> 8..10, 6-8 -> 16..18; op2 is slot+3). */
static const u8 OPL_SLOT[SEQ_OPL_CHANNELS] = { 0, 1, 2, 8, 9, 10, 16, 17, 18 };

/* MIDI note -> { block, fnum } for 49716 Hz, fnum <= 1023 with the highest
 * usable fnum (lowest block). Verified against the capture: note 84 ->
 * block 5 fnum 0x2B2 and note 47 -> block 2 fnum 0x3CF match prage_000.dro. */
static const u16 NOTE_TAB[128][2] = {
    {0x00,0x0AC}, {0x00,0x0B7}, {0x00,0x0C2}, {0x00,0x0CD}, {0x00,0x0D9}, {0x00,0x0E6}, {0x00,0x0F4}, {0x00,0x102},
    {0x00,0x112}, {0x00,0x122}, {0x00,0x133}, {0x00,0x146}, {0x00,0x159}, {0x00,0x16D}, {0x00,0x183}, {0x00,0x19A},
    {0x00,0x1B3}, {0x00,0x1CC}, {0x00,0x1E8}, {0x00,0x205}, {0x00,0x223}, {0x00,0x244}, {0x00,0x267}, {0x00,0x28B},
    {0x00,0x2B2}, {0x00,0x2DB}, {0x00,0x306}, {0x00,0x334}, {0x00,0x365}, {0x00,0x399}, {0x00,0x3CF}, {0x01,0x205},
    {0x01,0x223}, {0x01,0x244}, {0x01,0x267}, {0x01,0x28B}, {0x01,0x2B2}, {0x01,0x2DB}, {0x01,0x306}, {0x01,0x334},
    {0x01,0x365}, {0x01,0x399}, {0x01,0x3CF}, {0x02,0x205}, {0x02,0x223}, {0x02,0x244}, {0x02,0x267}, {0x02,0x28B},
    {0x02,0x2B2}, {0x02,0x2DB}, {0x02,0x306}, {0x02,0x334}, {0x02,0x365}, {0x02,0x399}, {0x02,0x3CF}, {0x03,0x205},
    {0x03,0x223}, {0x03,0x244}, {0x03,0x267}, {0x03,0x28B}, {0x03,0x2B2}, {0x03,0x2DB}, {0x03,0x306}, {0x03,0x334},
    {0x03,0x365}, {0x03,0x399}, {0x03,0x3CF}, {0x04,0x205}, {0x04,0x223}, {0x04,0x244}, {0x04,0x267}, {0x04,0x28B},
    {0x04,0x2B2}, {0x04,0x2DB}, {0x04,0x306}, {0x04,0x334}, {0x04,0x365}, {0x04,0x399}, {0x04,0x3CF}, {0x05,0x205},
    {0x05,0x223}, {0x05,0x244}, {0x05,0x267}, {0x05,0x28B}, {0x05,0x2B2}, {0x05,0x2DB}, {0x05,0x306}, {0x05,0x334},
    {0x05,0x365}, {0x05,0x399}, {0x05,0x3CF}, {0x06,0x205}, {0x06,0x223}, {0x06,0x244}, {0x06,0x267}, {0x06,0x28B},
    {0x06,0x2B2}, {0x06,0x2DB}, {0x06,0x306}, {0x06,0x334}, {0x06,0x365}, {0x06,0x399}, {0x06,0x3CF}, {0x07,0x205},
    {0x07,0x223}, {0x07,0x244}, {0x07,0x267}, {0x07,0x28B}, {0x07,0x2B2}, {0x07,0x2DB}, {0x07,0x306}, {0x07,0x334},
    {0x07,0x365}, {0x07,0x399}, {0x07,0x3CF}, {0x07,0x3FF}, {0x07,0x3FF}, {0x07,0x3FF}, {0x07,0x3FF}, {0x07,0x3FF},
    {0x07,0x3FF}, {0x07,0x3FF}, {0x07,0x3FF}, {0x07,0x3FF}, {0x07,0x3FF}, {0x07,0x3FF}, {0x07,0x3FF}, {0x07,0x3FF},
};

typedef struct {
    int midi;          /* MIDI channel that keyed this voice */
    int note;          /* MIDI note, or SEQ_NOTE_FREE */
    u32 release;       /* ticks left before auto key-off */
    u32 age;           /* allocation order, for stealing */
    u8 b0;             /* 0xB0 value without the key bit */
} seq_voice;

static struct {
    const u8 *evnt;
    u32 evnt_len;
    u32 pos;
    u32 wait;
    int loaded;
    int playing;
    u32 age;
    seq_voice voice[SEQ_OPL_CHANNELS];
    u8 program[SEQ_MIDI_CHANNELS];
    u8 bank[SEQ_MIDI_CHANNELS];
} S = {
    /* Voices start free, so the first halt() has nothing to key off and
     * seq_active_track() is 0 before the first load. */
    .voice = {
        [0] = { .note = SEQ_NOTE_FREE }, [1] = { .note = SEQ_NOTE_FREE },
        [2] = { .note = SEQ_NOTE_FREE }, [3] = { .note = SEQ_NOTE_FREE },
        [4] = { .note = SEQ_NOTE_FREE }, [5] = { .note = SEQ_NOTE_FREE },
        [6] = { .note = SEQ_NOTE_FREE }, [7] = { .note = SEQ_NOTE_FREE },
        [8] = { .note = SEQ_NOTE_FREE },
    },
};

static u32 rd_be32(const u8 *p)
{
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | (u32)p[3];
}

static int id4(const u8 *p, char a, char b, char c, char d)
{
    return p[0] == (u8)a && p[1] == (u8)b && p[2] == (u8)c && p[3] == (u8)d;
}

/* Bounded VLQ read from the event stream. */
static int read_vlq(u32 *out)
{
    u32 v = 0;
    for (int i = 0; i < 4 && S.pos < S.evnt_len; i++) {
        u8 c = S.evnt[S.pos++];
        v = (v << 7) | (c & 0x7f);
        if ((c & 0x80) == 0) {
            *out = v;
            return 1;
        }
    }
    return 0;
}

/* Applies a patch payload to an OPL channel. Payload layout (verified,
 * FORMATS.md): [3..7] = modulator 0x20/0x40/0x60/0x80/0xE0, [8] = 0xC0,
 * [9..13] = carrier 0x20/0x40/0x60/0x80/0xE0. Writes are ordered by register
 * family, matching the captured driver's per-note setup.
 *
 * The driver ORs 0x30 into 0xC0 (OPL3 left/right output bits) in the capture;
 * kept here. KNOWN DIVERGENCE: the driver also attenuates the carrier TL
 * (p[10]) by velocity, dominantly p[10] + 0x16 + ((127 - vel) >> 3) added to
 * the raw byte (KSL bits included); the residual is not a pure function of
 * velocity (patch 0x34 is +1, patch 0x74 is -1), so this port applies the patch
 * TL verbatim. See port/spec/audio.md "Known capture divergences". */
static void apply_patch(int opl_ch, u16 key)
{
    const u8 *p = patches_lookup(key);
    u8 base = OPL_SLOT[opl_ch];

    if (p == NULL)
        return;
    opl_write((u16)(0x20 + base), p[3]);
    opl_write((u16)(0x23 + base), p[9]);
    opl_write((u16)(0x40 + base), p[4]);
    opl_write((u16)(0x43 + base), p[10]);
    opl_write((u16)(0x60 + base), p[5]);
    opl_write((u16)(0x63 + base), p[11]);
    opl_write((u16)(0x80 + base), p[6]);
    opl_write((u16)(0x83 + base), p[12]);
    opl_write((u16)(0xE0 + base), p[7]);
    opl_write((u16)(0xE3 + base), p[13]);
    opl_write((u16)(0xC0 + opl_ch), (u8)(p[8] | 0x30));
}

static void key_off(int opl_ch)
{
    if (S.voice[opl_ch].note == SEQ_NOTE_FREE)
        return;
    opl_write((u16)(0xB0 + opl_ch), S.voice[opl_ch].b0);
    S.voice[opl_ch].note = SEQ_NOTE_FREE;
    S.voice[opl_ch].release = 0;
}

/* Releases the voice on `midi`/`note`, oldest first. */
static void key_off_note(int midi, int note)
{
    int best = -1;
    for (int v = 0; v < SEQ_OPL_CHANNELS; v++) {
        if (S.voice[v].note != note || S.voice[v].midi != midi)
            continue;
        if (best < 0 || S.voice[v].age < S.voice[best].age)
            best = v;
    }
    if (best >= 0)
        key_off(best);
}

static int alloc_voice(void)
{
    int best = -1;
    for (int v = 0; v < SEQ_OPL_CHANNELS; v++) {
        if (S.voice[v].note == SEQ_NOTE_FREE)
            return v;
        if (best < 0 || S.voice[v].age < S.voice[best].age)
            best = v;
    }
    /* PORT: all nine voices busy — steal the oldest. The original's exhaustion
     * policy (drop / steal / error) is not established; see mixer.h for the
     * same unknown on the sample path. */
    key_off(best);
    return best;
}

static void key_on(int midi, int note, int vel, u32 dur)
{
    u16 key;
    int v;
    (void)vel;

    if (midi < 0 || midi >= SEQ_MIDI_CHANNELS || note < 0 || note > 127)
        return;
    /* Channel 9 is percussion: the capture shows its notes looked up in the
     * FAT.OPL percussion bank (0x7F << 8 | note). */
    if (midi == 9)
        key = PATCH_KEY(PATCH_BANK_PERCUSSION, (u8)note);
    else
        key = PATCH_KEY(S.bank[midi], S.program[midi]);

    v = alloc_voice();
    apply_patch(v, key);
    S.voice[v].midi = midi;
    S.voice[v].note = note;
    S.voice[v].release = dur;
    S.voice[v].age = ++S.age;
    {
        u8 block = (u8)NOTE_TAB[note][0];
        u16 fnum = NOTE_TAB[note][1];
        S.voice[v].b0 = (u8)((block << 2) | ((fnum >> 8) & 0x03));
        opl_write((u16)(0xA0 + v), (u8)(fnum & 0xff));
        opl_write((u16)(0xB0 + v), (u8)(S.voice[v].b0 | 0x20));
    }
}

/* The single halt path. Every exit from the parser that stops playback routes
 * here, so no path can stop the stream while leaving OPL channels keyed on. */
static void halt(void)
{
    for (int v = 0; v < SEQ_OPL_CHANNELS; v++)
        key_off(v);
    S.playing = 0;
}

/* Processes the pending event group, then reads the delta that precedes the
 * next one. Mirrors the engine's per-tick event loop (port/spec/audio.md). */
static void process(void)
{
    while (S.pos < S.evnt_len) {
        u8 b = S.evnt[S.pos];
        u8 hi;

        if (b < 0x80) {
            S.pos++;
            S.wait = b;
            return;
        }
        S.pos++;
        if (b == 0xFF) {
            u8 type;
            u32 ln;
            if (S.pos >= S.evnt_len) { halt(); return; }
            type = S.evnt[S.pos++];
            if (!read_vlq(&ln)) { halt(); return; }
            if (type == 0x2F) {           /* XMIDI loop / end of sequence */
                /* TODO(verify): the RBRN loop range is not reproduced; the
                 * capture stayed linear over its window, so playback stops. */
                halt();
                return;
            }
            if (S.evnt_len - S.pos < ln) { halt(); return; }
            S.pos += ln;
        } else if (b == 0xF0 || b == 0xF7) {
            u32 ln;
            if (!read_vlq(&ln)) { halt(); return; }
            if (S.evnt_len - S.pos < ln) { halt(); return; }
            S.pos += ln;
        } else {
            u8 ch = (u8)(b & 0x0F);
            hi = (u8)(b & 0xF0);
            if (hi == 0x90) {
                u8 note, vel;
                u32 dur;
                if (S.evnt_len - S.pos < 2) { halt(); return; }
                note = S.evnt[S.pos++];
                vel = S.evnt[S.pos++];
                if (!read_vlq(&dur)) { halt(); return; }
                if (vel == 0)
                    key_off_note(ch, note);
                else
                    key_on(ch, note, vel, dur);
            } else if (hi == 0x80) {
                u8 note;
                if (S.evnt_len - S.pos < 2) { halt(); return; }
                note = S.evnt[S.pos];
                S.pos += 2;
                key_off_note(ch, note);
            } else if (hi == 0xB0) {
                u8 c, val;
                if (S.evnt_len - S.pos < 2) { halt(); return; }
                c = S.evnt[S.pos++];
                val = S.evnt[S.pos++];
                if (c == 0 && ch < SEQ_MIDI_CHANNELS)
                    S.bank[ch] = val;        /* bank select */
            } else if (hi == 0xC0) {
                if (S.pos >= S.evnt_len) { halt(); return; }
                if (ch < SEQ_MIDI_CHANNELS)
                    S.program[ch] = S.evnt[S.pos];
                S.pos++;
            } else if (hi == 0xA0 || hi == 0xE0) {
                if (S.evnt_len - S.pos < 2) { halt(); return; }
                S.pos += 2;
            } else if (hi == 0xD0) {
                if (S.pos >= S.evnt_len) { halt(); return; }
                S.pos++;
            } else {
                halt();                  /* unknown status: stop safely */
                return;
            }
        }
    }
    halt();
}

u32 seq_bank_size(const u8 *data)
{
    if (data == NULL)
        return 0;
    /* RIFF chunk size counts the bytes after the size field, so the container
     * is 8 + size bytes. Type-agnostic (FORM or CAT) so an outer XDIR container
     * can be sized too; seq_load is what insists on XMID. */
    if (id4(data, 'F', 'O', 'R', 'M') || id4(data, 'C', 'A', 'T', ' '))
        return 8u + rd_be32(data + 4);
    return 0;
}

int seq_load(const u8 *data, u32 len)
{
    const u8 *evnt = NULL;
    u32 evnt_len = 0;
    u32 i = 0;

    if (data == NULL || len < 12)
        return 0;

    while (i + 12 <= len) {
        if (id4(data + i, 'F', 'O', 'R', 'M') &&
            id4(data + i + 8, 'X', 'M', 'I', 'D')) {
            u32 fsz = rd_be32(data + i + 4);
            u32 end, p;
            if (fsz < 4 || fsz > len - (i + 8))
                return 0;
            end = i + 8 + fsz;              /* first byte past this FORM */
            p = i + 12;
            while (p + 8 <= end) {
                u32 csz = rd_be32(data + p + 4);
                if (csz > end - (p + 8))
                    return 0;
                if (id4(data + p, 'E', 'V', 'N', 'T') && csz > 0) {
                    evnt = data + p + 8;
                    evnt_len = csz;
                }
                p += 8 + csz + (csz & 1u);
            }
            break;
        }
        i++;
    }

    if (evnt == NULL)
        return 0;

    halt();                             /* release the previous bank's voices */
    S.evnt = evnt;
    S.evnt_len = evnt_len;
    S.pos = 0;
    S.wait = 0;
    S.loaded = 1;
    return 1;
}

void seq_start(void)
{
    if (!S.loaded)
        return;
    halt();                             /* release any voices from a prior run */
    S.pos = 0;
    S.wait = 0;
    S.playing = 1;
    S.age = 0;
    for (int v = 0; v < SEQ_OPL_CHANNELS; v++) {
        S.voice[v].note = SEQ_NOTE_FREE;
        S.voice[v].midi = 0;
        S.voice[v].release = 0;
        S.voice[v].age = 0;
        S.voice[v].b0 = 0;
    }
    for (int c = 0; c < SEQ_MIDI_CHANNELS; c++) {
        S.program[c] = 0;
        S.bank[c] = 0;
    }
    /* The captured driver's cached state opens with waveform-select enable
     * (0x01 = 0x20) then the OPL3-mode enable (0x105 = 0x01); the port writes
     * both, in that order, matching the capture. The port needs 0x01 because
     * patches write 0xE0. 0x105 does not change the port's output: every
     * apply_patch writes 0xC0 = patch | 0x30 (both output enables), which in
     * OPL3 mode is what gates each channel's mix. See port/spec/audio.md
     * "Known capture divergences". */
    opl_write(0x01, 0x20);
    opl_write(0x105, 0x01);
}

void seq_stop(void)
{
    halt();
}

void seq_tick(void)
{
    if (!S.playing)
        return;
    for (int v = 0; v < SEQ_OPL_CHANNELS; v++) {
        if (S.voice[v].note == SEQ_NOTE_FREE)
            continue;
        if (S.voice[v].release == 0 || --S.voice[v].release == 0)
            key_off(v);
    }
    if (S.wait > 0 && --S.wait > 0)
        return;
    process();
}

int seq_active_track(void)
{
    int n = 0;
    for (int v = 0; v < SEQ_OPL_CHANNELS; v++)
        if (S.voice[v].note != SEQ_NOTE_FREE)
            n++;
    return n;
}

int seq_playing(void)
{
    return S.playing;
}
