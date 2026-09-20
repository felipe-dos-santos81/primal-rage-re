/* XMIDI sequencer -> OPL register writes. See sequencer.h for the contract and
 * the tick evidence, port/spec/audio.md for the event grammar.
 *
 * The original sent MIDI events to the loaded SBPRO2.MDI driver and the driver
 * wrote the OPL registers. This module stands in for that driver: it owns a
 * fixed 18-channel OPL voice pool, looks programs up in the FAT.OPL bank, and
 * writes the same register families the capture shows. Voice stealing and the
 * per-voice decode are the port's reconstruction, not the driver's code; the
 * parts the capture pins down (patch byte layout, tick, fnum/block) are noted
 * at their use. The channel pool is the driver's 18 OPL3 operator pairs.
 */
#include <stddef.h>

#include "sequencer.h"
#include "patches.h"
#include "pitch.h"
#include "opl/opl.h"

#define SEQ_OPL_CHANNELS 18
#define SEQ_OPL_BANK 9
#define SEQ_MIDI_CHANNELS 16
#define SEQ_NOTE_FREE (-1)
#define SEQ_NO_PATCH 0xFFFFu

/* PORT: register-family mask for the driver's applier (SBPRO2.MDI 0x3184),
 * which tests [v+0x1539] in the order 0x80, 0x40, 0x20, 0x10, 0x08, 0x01 and
 * clears each bit after writing that family. */
#define FAM_FREQ  0x01
#define FAM_CONN  0x08
#define FAM_WAVE  0x10
#define FAM_EG    0x20
#define FAM_TL    0x40
#define FAM_AMVIB 0x80
#define FAM_ALL   (FAM_FREQ | FAM_CONN | FAM_WAVE | FAM_EG | FAM_TL | FAM_AMVIB)

/* PORT: the driver's key-on velocity-level table (SBPRO2.MDI 0xc27), indexed
 * by the MIDI velocity >> 3. Stored on the voice as [v+0x1511] at note-on and
 * folded into the TL channel level (0x31b8). */
static const u8 SEQ_VEL_LEVEL[16] = {
    0x52, 0x55, 0x58, 0x5b, 0x5e, 0x61, 0x64, 0x67,
    0x6a, 0x6d, 0x70, 0x73, 0x76, 0x79, 0x7c, 0x7f,
};

/* PORT: operator register offset for each OPL3 channel, including the second
 * register set's 0x100 (SBPRO2.MDI 0xc37 -> 0xc5b/0xc7f: channels 0-8 are
 * {0,1,2,8,9,10,16,17,18}, channels 9-17 repeat them in bank 1; op2 is +3). */
static const u16 OPL_SLOT[SEQ_OPL_CHANNELS] = {
    0x00, 0x01, 0x02, 0x08, 0x09, 0x0a, 0x10, 0x11, 0x12,
    0x100, 0x101, 0x102, 0x108, 0x109, 0x10a, 0x110, 0x111, 0x112,
};

/* PORT: register low byte for the channel families (0xC0/0xA0/0xB0). The
 * driver (SBPRO2.MDI 0xca3/0xcb5) uses the channel index within its bank, not
 * the operator slot: 0x100*bank + (ch mod 9). */
static u16 ch_reg(int ch, u16 family)
{
    return (u16)(family + (ch % SEQ_OPL_BANK) + (ch >= SEQ_OPL_BANK ? 0x100 : 0));
}

typedef struct {
    int midi;          /* MIDI channel that keyed this voice */
    int note;          /* MIDI note, or SEQ_NOTE_FREE */
    u32 release;       /* ticks left before auto key-off */
    u32 age;           /* allocation order, for stealing */
    u8 b0;             /* 0xB0 value without the key bit */
    u8 level;          /* key-on velocity level [v+0x1511], from 0xc27 */
    const u8 *patch;   /* cached patch payload, for controller re-applies */
    int index;         /* cached fnum table index (note+base / base) */
} seq_voice;

static struct {
    const u8 *evnt;
    u32 evnt_len;
    u32 pos;
    u32 wait;
    int loaded;
    int playing;
    u32 age;
    u32 next;          /* rotation cursor: the last slot tried (driver 0xffff) */
    u8 seqvol;         /* AIL sequence volume (engine input), 0..0x7f */
    seq_voice voice[SEQ_OPL_CHANNELS];
    u8 program[SEQ_MIDI_CHANNELS];
    u8 bank[SEQ_MIDI_CHANNELS];
    /* PORT: the driver's per-MIDI-channel controller state, indexed by channel
     * (0x1909 volume, 0x1919 pan, 0x1929/0x1939 wheel LSB/MSB, 0x1949
     * expression, 0x1959 mod, 0x1969 sustain, 0x18f9 bend scale). */
    u8 wheel_lsb[SEQ_MIDI_CHANNELS];
    u8 wheel_msb[SEQ_MIDI_CHANNELS];
    u8 bend_scale[SEQ_MIDI_CHANNELS];
    u8 volume[SEQ_MIDI_CHANNELS];
    u8 expression[SEQ_MIDI_CHANNELS];
    u8 pan[SEQ_MIDI_CHANNELS];
    u8 mod[SEQ_MIDI_CHANNELS];
    u8 sustain[SEQ_MIDI_CHANNELS];
} S = {
    /* Voices start free, so the first halt() has nothing to key off and
     * seq_active_track() is 0 before the first load. `next` seeds the driver's
     * 0xffff rotation cursor: the first trial wraps to slot 0. `seqvol` opens at
     * the image's AIL sequence-volume default (DAT_00108d94, 0x7f). */
    .next = SEQ_OPL_CHANNELS - 1,
    .seqvol = 0x7f,
    .voice = {
        [0] = { .note = SEQ_NOTE_FREE }, [1] = { .note = SEQ_NOTE_FREE },
        [2] = { .note = SEQ_NOTE_FREE }, [3] = { .note = SEQ_NOTE_FREE },
        [4] = { .note = SEQ_NOTE_FREE }, [5] = { .note = SEQ_NOTE_FREE },
        [6] = { .note = SEQ_NOTE_FREE }, [7] = { .note = SEQ_NOTE_FREE },
        [8] = { .note = SEQ_NOTE_FREE }, [9] = { .note = SEQ_NOTE_FREE },
        [10] = { .note = SEQ_NOTE_FREE }, [11] = { .note = SEQ_NOTE_FREE },
        [12] = { .note = SEQ_NOTE_FREE }, [13] = { .note = SEQ_NOTE_FREE },
        [14] = { .note = SEQ_NOTE_FREE }, [15] = { .note = SEQ_NOTE_FREE },
        [16] = { .note = SEQ_NOTE_FREE }, [17] = { .note = SEQ_NOTE_FREE },
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

/* PORT: the driver's `local_a` staircase (SBPRO2.MDI 0x31b0-0x31c2): each
 * stage computes `(a*b*2) >> 8` and then the `cmp al,1; sbb al,0xff` idiom,
 * i.e. `+1` when the shifted product is non-zero. Used twice for the channel
 * level and again by the per-operator TL fold. */
static u8 scale7(u8 a, u8 b)
{
    u32 t = ((u32)a * b * 2u) >> 8;
    return (u8)(t == 0 ? 0 : t + 1);
}

/* Applies the register families selected by `mask` to an OPL channel, from
 * the voice's cached patch payload. Payload layout (verified, FORMATS.md):
 * [3..7] = modulator 0x20/0x40/0x60/0x80/0xE0, [8] = 0xC0, [9..13] = carrier
 * 0x20/0x40/0x60/0x80/0xE0. Writes are ordered by register family, matching
 * the captured driver's per-note setup; a key-on passes FAM_ALL, which emits
 * the full 0x20..0xC0 preamble before the 0xA0/0xB0 frequency pair.
 *
 * The driver ORs 0x30 into 0xC0 (OPL3 left/right output bits) in the capture;
 * kept here, with the pan controller selecting between 0x30/0x20/0x10. */
static void fam_apply(int opl_ch, u8 mask)
{
    const u8 *p = S.voice[opl_ch].patch;
    u16 base = OPL_SLOT[opl_ch];
    int midi = S.voice[opl_ch].midi;
    int known = midi >= 0 && midi < SEQ_MIDI_CHANNELS;
    u8 ch = (u8)midi;

    /* PORT: attribute every write this apply makes to the voice's MIDI channel
     * until it returns, then clear it. The function has a single exit, so no
     * write can inherit a stale channel. The oracle uses this to exclude only
     * the carrier-TL rows of the residual channels (1 and 4). */
    opl_set_write_attr((u8)midi);

    if (p != NULL) {
        if (mask & FAM_AMVIB) {
            /* PORT: SBPRO2.MDI 0x3409-0x345f. Controller 1 >= 0x40 ORs bit
             * 0x40 into both 0x20 bytes; the patch bits are otherwise verbatim. */
            u8 am = (known && S.mod[ch] >= 0x40) ? 0x40 : 0;
            opl_write((u16)(0x20 + base), (u8)(p[3] | am));
            opl_write((u16)(0x23 + base), (u8)(p[9] | am));
        }
        if (mask & FAM_TL) {
            /* PORT: SBPRO2.MDI 0x319a-0x31c4 (channel level) and
             * 0x346a-0x34d3 (per-operator fold). The engine scales a received
             * CC7 by the AIL sequence volume before dispatch (prage.c:49121);
             * `level` then folds in the channel expression and the per-voice
             * key-on velocity level with the driver's staircase. Gate bit 0 is
             * the modulator, bit 1 the carrier; the normal-voice gate byte
             * [v+0x1629] is (p[8]&1)|2, so the carrier is always gated. Every
             * shipped payload opens 0x000e (type 0), so the type-3 gate
             * [v+0x18e5] is not reachable and is not modelled. */
            u8 cc7 = known ? (u8)(((u32)S.seqvol * S.volume[ch]) / 0x7f) : 0;
            u8 expr = known ? S.expression[ch] : 0x7f;
            u8 level = scale7(scale7(cc7, expr), S.voice[opl_ch].level);
            u8 gate = (u8)((p[8] & 1) | 2);
            u8 att;

            att = (u8)((~p[4]) & 0x3f);
            if (gate & 1)
                att = (u8)(((u32)att * level) / 0x7f);
            opl_write((u16)(0x40 + base), (u8)(((~att) & 0x3f) | (p[4] & 0xc0)));
            att = (u8)((~p[10]) & 0x3f);
            if (gate & 2)
                att = (u8)(((u32)att * level) / 0x7f);
            opl_write((u16)(0x43 + base), (u8)(((~att) & 0x3f) | (p[10] & 0xc0)));
        }
        if (mask & FAM_EG) {
            opl_write((u16)(0x60 + base), p[5]);
            opl_write((u16)(0x63 + base), p[11]);
            opl_write((u16)(0x80 + base), p[6]);
            opl_write((u16)(0x83 + base), p[12]);
        }
        if (mask & FAM_WAVE) {
            opl_write((u16)(0xE0 + base), p[7]);
            opl_write((u16)(0xE3 + base), p[13]);
        }
        if (mask & FAM_CONN) {
            /* PORT: SBPRO2.MDI 0x3578-0x35bf. Base 0x30; pan < 0x1c -> 0x20,
             * pan > 99 -> 0x10, else 0x30. */
            u8 pan = known ? S.pan[ch] : 0x40;
            u8 bits = (pan < 0x1c) ? 0x20 : (pan > 99 ? 0x10 : 0x30);
            opl_write(ch_reg(opl_ch, 0xC0), (u8)((p[8] & 0x0f) | bits));
        }
    }
    if (mask & FAM_FREQ) {
        u8 a0, b0;
        int ch = S.voice[opl_ch].midi;
        s32 bend = 0;
        /* PORT: 0x35fa reads the wheel unconditionally from the voice's own
         * MIDI channel ([si+0x14c1] -> [bx+0x1939]/[bx+0x1929]); a key-on
         * reaches the same routine, so it sounds the live wheel too. */
        if (ch >= 0 && ch < SEQ_MIDI_CHANNELS)
            bend = pitch_bend_of((S.wheel_msb[ch] << 7) | S.wheel_lsb[ch],
                                 S.bend_scale[ch]);
        pitch_lookup(S.voice[opl_ch].index, bend, &a0, &b0);
        S.voice[opl_ch].b0 = b0;
        opl_write(ch_reg(opl_ch, 0xA0), a0);
        opl_write(ch_reg(opl_ch, 0xB0), (u8)(b0 | 0x20));
    }
    opl_set_write_attr(0xFF);
}

/* PORT: 0x3b54, the channel-controller handler. Stores the controller, then
 * re-applies the affected register family for every active voice on the same
 * MIDI channel (the raw's 0x3c53 loop). The no-re-apply controllers (6, 64,
 * 112, 113, 114, 123) return before the loop, matching the raw's dispatch
 * table; 112/113/114 store state (patch-bank/percussion/patch-flag) the port's
 * AIL-layer bank model has no separate field for, so they are inert here. */
static void midi_control(u8 status, u8 a, u8 b)
{
    u8 hi = (u8)(status & 0xf0);
    int ch = status & 0x0f;
    u8 mask;

    if (ch >= SEQ_MIDI_CHANNELS)
        return;
    if (hi == 0xe0) {
        S.wheel_lsb[ch] = a;
        S.wheel_msb[ch] = b;
        mask = FAM_FREQ;
    } else if (hi == 0xb0) {
        switch (a) {
        case 6:  S.bend_scale[ch] = b; return;
        case 7:  S.volume[ch] = b; mask = FAM_TL; break;
        case 11: S.expression[ch] = b; mask = FAM_TL; break;
        case 1:  S.mod[ch] = b; mask = FAM_AMVIB; break;
        case 10: S.pan[ch] = b; mask = FAM_CONN; break;
        /* TODO(verify): ctrl 64 also calls 0x3b1e(ch) when b < 0x40; that
         * sustain-release helper is a Task 5 gap (spec §10). */
        case 64: S.sustain[ch] = b; return;
        /* PORT: 0x3cc0-0x3ce2. The reset does not touch volume, pan or bend
         * scale; it also calls 0x3b1e(ch), a Task 5 gap. */
        case 121:
            S.sustain[ch] = 0;
            S.mod[ch] = 0;
            S.expression[ch] = 0x7f;
            S.wheel_lsb[ch] = 0;
            S.wheel_msb[ch] = 0x40;
            mask = (u8)(FAM_AMVIB | FAM_TL | FAM_FREQ);
            break;
        case 123: return;
        case 112: return;
        case 113: return;
        case 114: return;
        default:  return;
        }
    } else {
        return;
    }
    for (int v = 0; v < SEQ_OPL_CHANNELS; v++)
        if (S.voice[v].note != SEQ_NOTE_FREE && S.voice[v].midi == ch)
            fam_apply(v, mask);
}

static void key_off(int opl_ch)
{
    if (S.voice[opl_ch].note == SEQ_NOTE_FREE)
        return;
    opl_set_write_attr((u8)S.voice[opl_ch].midi);
    opl_write(ch_reg(opl_ch, 0xB0), S.voice[opl_ch].b0);
    opl_set_write_attr(0xFF);
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
    /* PORT: SBPRO2.MDI's melodic allocator (0x3095-0x30d8) walks a monotonic
     * rotation cursor [0x1408] over the 18 slot-owner bytes [0x1a49]: it takes
     * the next free slot after the cursor, wrapping at 18, so successive notes
     * rotate 0,1,2,... instead of reusing the lowest free channel. key_off
     * leaves `next` untouched (0x3162), so freeing ch0 does not pull the next
     * note back to it. */
    int v = (int)S.next;
    for (int i = 0; i < SEQ_OPL_CHANNELS; i++) {
        v = (v + 1) % SEQ_OPL_CHANNELS;
        if (S.voice[v].note == SEQ_NOTE_FREE) {
            S.next = (u32)v;
            return v;
        }
    }
    /* PORT: all 18 voices busy — the driver steals by quietest voice at
     * 0x36f6; the port keeps its oldest-voice policy (see spec divergences).
     * (Not reached in the compared window; see the channel-assignment doc.) */
    {
        int best = 0;
        for (int i = 1; i < SEQ_OPL_CHANNELS; i++)
            if (S.voice[i].age < S.voice[best].age)
                best = i;
        key_off(best);
        S.next = (u32)best;
        return best;
    }
}

static void key_on(int midi, int note, int vel, u32 dur)
{
    const u8 *p;
    u16 key;
    int idx;
    int v;

    if (midi < 0 || midi >= SEQ_MIDI_CHANNELS || note < 0 || note > 127)
        return;
    /* Channel 9 is percussion: the capture shows its notes looked up in the
     * FAT.OPL percussion bank (0x7F << 8 | note). */
    if (midi == 9)
        key = PATCH_KEY(PATCH_BANK_PERCUSSION, (u8)note);
    else
        key = PATCH_KEY(S.bank[midi], S.program[midi]);

    v = alloc_voice();

    /* PORT: the driver does not key a note at its MIDI pitch. Its note-on
     * path (SBPRO2.MDI 0x35fa-0x36a6) builds the fnum table index from the
     * patch's base byte ([di+2], stored at 0x3aac-0x3ac1): melodic adds the
     * base to the note ([si+0x14d5]=note, [si+0x14fd]=base), while percussion
     * uses the base alone ([si+0x14d5]=base, [si+0x14fd]=0), so a drum's
     * 0x7F-bank patch base byte selects the table entry. Every melodic
     * FAT.OPL entry in the shipped bank has base 0, so the melodic sum reduces
     * to the note for the compared window; pitch_lookup() folds the index and
     * selects the fnum/block. The payload pointer and index are cached on the
     * voice so a later controller re-apply (0x3184) can rebuild the writes
     * without the key. */
    p = patches_lookup(key);
    idx = note;
    if (p != NULL)
        idx = (midi == 9) ? (int)p[2] : note + (int)p[2];
    if (idx > 127)
        idx = 127;
    S.voice[v].patch = p;
    S.voice[v].index = idx;
    S.voice[v].midi = midi;
    S.voice[v].note = note;
    S.voice[v].level = SEQ_VEL_LEVEL[(vel >> 3) & 0x0f];
    S.voice[v].release = dur;
    S.voice[v].age = ++S.age;

    fam_apply(v, FAM_ALL);
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
                /* ctrl 0 is the AIL-layer bank select, not a 0x3b54 handler;
                 * every other controller goes to the driver's dispatch. */
                if (c == 0 && ch < SEQ_MIDI_CHANNELS)
                    S.bank[ch] = val;
                else
                    midi_control(b, c, val);
            } else if (hi == 0xC0) {
                if (S.pos >= S.evnt_len) { halt(); return; }
                if (ch < SEQ_MIDI_CHANNELS)
                    S.program[ch] = S.evnt[S.pos];
                S.pos++;
            } else if (hi == 0xE0) {
                u8 lsb, msb;
                if (S.evnt_len - S.pos < 2) { halt(); return; }
                lsb = S.evnt[S.pos++];
                msb = S.evnt[S.pos++];
                midi_control(b, lsb, msb);
            } else if (hi == 0xA0) {
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

/* Precondition: `data` points to at least 8 readable bytes (the "FORM"/"CAT "
 * id plus the big-endian size). The AIL surface carries no length (see ail.h),
 * so the caller owns this bound; game_music_bank_find validates the declared
 * size against the loaded resource before seq_load is handed the result. */
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

/* PORT: engine input — the AIL sequence volume. The original's sequencer scales
 * a received CC7 by this before handing it to the driver (prage.c:49121:
 * `param_4 = (seq[0xd] * param_4) / 0x7f`), and the driver's TL law consumes
 * that scaled value. It is not a driver constant: DAT_00108d94 seeds it at
 * sequence setup and AIL_set_sequence_volume sets it (flow.c's music volume).
 * Clamped to 0..0x7f; persists across seq_load/seq_start (it is engine state,
 * not sequence data). */
void seq_set_sequence_volume(u8 volume)
{
    S.seqvol = volume > 0x7f ? 0x7f : volume;
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
    S.next = SEQ_OPL_CHANNELS - 1;   /* driver's reset cursor 0xffff */
    for (int v = 0; v < SEQ_OPL_CHANNELS; v++) {
        S.voice[v].note = SEQ_NOTE_FREE;
        S.voice[v].midi = 0;
        S.voice[v].release = 0;
        S.voice[v].age = 0;
        S.voice[v].b0 = 0;
        S.voice[v].level = 0;
        S.voice[v].patch = NULL;
        S.voice[v].index = 0;
    }
    for (int c = 0; c < SEQ_MIDI_CHANNELS; c++) {
        S.program[c] = 0;
        S.bank[c] = 0;
        S.wheel_lsb[c] = 0;
        S.wheel_msb[c] = 0x40;
        S.bend_scale[c] = 0;
        S.volume[c] = 0;
        S.expression[c] = 0x7f;
        /* PORT: the driver's reset (0x3cc0) does not touch pan, and BSS-zero
         * would give 0x24, but the capture's tick-0 0xC0 = 0x34 needs
         * [ch+0x1919] in [0x1c, 99]. The exact default is not readable from the
         * DRO; 0x40 (MIDI centre) is a documented choice inside that evidenced
         * band and yields the captured 0x30 bits. The title also sends CC10 =
         * 0x40 on every channel at tick 0, so the default is not exercised. */
        S.pan[c] = 0x40;
        S.mod[c] = 0;
        S.sustain[c] = 0;
    }
    /* The captured driver's cached state opens with waveform-select enable
     * (0x01 = 0x20) then the OPL3-mode enable (0x105 = 0x01); the port writes
     * both, in that order, matching the capture. The port needs 0x01 because
     * patches write 0xE0. 0x105 does not change the port's output: every
     * fam_apply writes 0xC0 = patch | bits with bits in {0x10, 0x20, 0x30}
     * (the default pan 0x40 gives 0x30, both output enables; the pan
     * controller narrows that to one side but never clears both). Those bits
     * are what gate each channel's mix in OPL3 mode. See port/spec/audio.md
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
