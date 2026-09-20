"""Independent Python oracle for the music sequencer's OPL register stream.

Reads one shipped music bank (an XMIDI `FORM ... XMID` bundle, either the GRA
file itself or the bare bank) plus the `FAT.OPL` patch bank and emits, for each
port tick, the register writes the port's sequencer would make:

    tick reg value        three columns, millisecond-free

in exactly the format `tools/opl_trace.py` uses, so the two streams (Python
oracle, captured original) and the C sequencer's stream can be diffed pairwise.

Independence. This is a pure-Python 3 second implementation of the published
contract (port/spec/audio.md "Music event grammar" / "FAT.OPL patch bank" and
sequencer.h); it imports no port code. It is deliberately not a transliteration
of sequencer.c:

  * the decoder is split in two — `decode_events` tokenises the EVNT stream
    without touching voice state, and `Sequencer` replays the tokens — where
    sequencer.c interleaves parsing and register emission in one position-
    mutating loop;
  * the integer note -> (block, fnum) table is COMPUTED from the OPL clock
    (49716 Hz) and the MIDI pitch formula and pinned to capture-verified
    anchors. The driver's fine-step fnum table (SBPRO2.MDI 0x7fd, 192 signed
    words) is read from the driver image, not computed: its generator deviates
    from the formula by one at a handful of fine steps, and the C stream is
    compared byte-for-byte against that literal;
  * controller state and the family re-apply loop mirror `midi_control` /
    `fam_apply`, but voice selection and patch application are table /
    comprehension driven rather than a branch-for-branch copy of the port's
    helpers.

What independence does and does not buy. Agreement with the C stream is real
evidence that the *implementation* of the shared contract is faithful — it
catches transcription and coding errors, including in the note table. It is NOT
evidence that the contract matches the original driver: the reconstruction
choices (18-voice OPL3 pool, oldest-steal, per-note whole-patch re-apply) are the
port's and are unverified against the capture (spec "Known capture divergences").

usage:
  opl_seq.py <music> [--trace]          # "tick reg value" lines (default)
  opl_seq.py <music> --info             # summary only
  opl_seq.py <music> --trace --patches PATH
  opl_seq.py --capture-anchors <music> <capture.dro>   # re-derive note anchors
  opl_seq.py --self-test
"""
import os
import signal
import sys

# OPL clock the port pins its note table to (Hz). sequencer.c NOTE_TAB was
# verified against the capture at this rate; deriving the table from it here
# turns a copied literal into an independently computed value.
OPL_CLOCK = 49716.0

# OPL3 operator register offset for each sequencer channel, including the
# second register set's 0x100 (SBPRO2.MDI 0xc37 -> 0xc5b/0xc7f: channels 0-8 are
# 0,1,2,8,9,10,16,17,18, channels 9-17 repeat them in bank 1).
OPL_SLOT = (0, 1, 2, 8, 9, 10, 16, 17, 18,
            0x100, 0x101, 0x102, 0x108, 0x109, 0x10a, 0x110, 0x111, 0x112)
OPL_BANK = 9        # channels per OPL3 register set
OPL_CHANNELS = len(OPL_SLOT)


def ch_reg(ch, family):
    """Channel-family register (0xC0/0xA0/0xB0) for an OPL3 channel.

    The driver (SBPRO2.MDI 0xca3/0xcb5) addresses these by channel index within
    its bank: 0x100*bank + (ch mod 9), unlike the operator families."""
    return family + (ch % OPL_BANK) + (0x100 if ch >= OPL_BANK else 0)

# PATCH_BYTES payload -> register, in the order the port writes them: modulator
# fields then carrier fields (carrier at slot+3), each family in register order.
PATCH_BYTES = 14
PATCH_MAX = 256

# Family bits (SBPRO2.MDI 0x3184 tests and clears them in the order
# AMVIB, TL, EG, WAVE, CONN, FREQ; this table keeps that write order).
FAM_FREQ, FAM_CONN, FAM_WAVE, FAM_EG, FAM_TL, FAM_AMVIB = 0x01, 0x08, 0x10, 0x20, 0x40, 0x80
FAM_ALL = 0xF9
FAMILY_WRITES = (
    (FAM_AMVIB, ((0x20, 3), (0x23, 9))),      # AM/VIB/EG/KSR/MULT
    (FAM_TL, ((0x40, 4), (0x43, 10))),        # KSL/TL
    (FAM_EG, ((0x60, 5), (0x63, 11),          # attack/decay
              (0x80, 6), (0x83, 12))),        # sustain/release
    (FAM_WAVE, ((0xE0, 7), (0xE3, 13))),      # waveform select
)

# The driver's 0x7fd fine-step fnum table: 12 semitones x 16 steps, signed.
PITCH_TBL_OFFSET = 0x7FD
PITCH_TBL_WORDS = 192
# The straightforward controller stores; the value (mask) is None for the
# controllers whose raw handler returns before the re-apply loop.
CTRL_STORES = {6: ('bend_scale', None), 7: ('volume', FAM_TL),
               11: ('expression', FAM_TL), 1: ('mod', FAM_AMVIB),
               10: ('pan', FAM_CONN), 64: ('sustain', None)}


def load_pitch_table(path):
    """SBPRO2.MDI -> 192 signed words at 0x7fd, or None if the image is short."""
    data = read(path)
    if len(data) < PITCH_TBL_OFFSET + 2 * PITCH_TBL_WORDS:
        return None
    return tuple(int.from_bytes(
        data[PITCH_TBL_OFFSET + 2 * i:PITCH_TBL_OFFSET + 2 * i + 2],
        'little', signed=True) for i in range(PITCH_TBL_WORDS))


def computed_pitch_table():
    """Fine-step table derived from the OPL clock (fallback when the driver
    image is absent; the driver's own table is authoritative when present)."""
    table = []
    for n in range(PITCH_TBL_WORDS):
        sem, step = divmod(n, 16)
        note = 84 + sem + step / 16.0
        freq = 440.0 * 2.0 ** ((note - 69) / 12.0)
        ref = int(round(freq * 2.0 ** 15 / OPL_CLOCK))
        table.append(ref if ref <= 1023 else ref // 2 - 1024)
    return tuple(table)


def note_to_block_fnum(note):
    """MIDI note -> (block, fnum): lowest block with fnum <= 1023, rounded."""
    freq = 440.0 * 2.0 ** ((note - 69) / 12.0)
    for block in range(8):
        fnum = int(round(freq * 2 ** (20 - block) / OPL_CLOCK))
        if fnum <= 1023:
            return block, fnum
    return 7, 1023


NOTE_TAB = tuple(note_to_block_fnum(n) for n in range(128))

# Values the computed table must reproduce. 84 and 79 are CAPTURE-DERIVED: the
# title bank's first two melodic note-ons, paired to prage_000.dro key-on pairs
# by note-on time, not copied from sequencer.c's NOTE_TAB literal. `python3
# tools/opl_seq.py --capture-anchors <music> <capture.dro>` re-derives them and
# fails if the formula disagrees. 0, 31 and 127 are computed-table boundary
# points (lowest entry, octave, clamp) and are NOT capture claims. This table is
# the driver's melodic table; percussion selects it by its patch's base byte
# instead (see Sequencer.key_on, matching sequencer.c).
NOTE_ANCHORS = {0: (0, 0x0AC), 31: (1, 0x205), 79: (5, 0x205),
                84: (5, 0x2B2), 127: (7, 0x3FF)}

# The capture-derived subset, re-checked by --capture-anchors.
NOTE_CAPTURE = (84, 79)


def load_patches(data):
    """FAT.OPL bytes -> {key: 14-byte payload}, or None if the bank is rejected.

    Rejects exactly what patches.c rejects: a truncated table, no entries, more
    than PATCH_MAX entries, or a payload offset below the table end or less than
    PATCH_BYTES from the buffer end. (Short payload slices are no longer returned
    for apply-time skipping.) First key wins when a key repeats, as
    patches_lookup's first-match scan does.
    """
    if data is None or len(data) < 8:
        return None
    entries = []
    off, n = 0, len(data)
    while True:
        if off + 6 > n:
            return None
        key = data[off] | (data[off + 1] << 8)
        if key == 0xFFFF:
            off += 2
            break
        if len(entries) >= PATCH_MAX:
            return None
        entries.append((key, data[off + 2] | (data[off + 3] << 8) |
                        (data[off + 4] << 16) | (data[off + 5] << 24)))
        off += 6
    if not entries:
        return None
    table_end = off
    patches = {}
    for key, pos in entries:
        if pos < table_end or pos > n or n - pos < PATCH_BYTES:
            return None
        patches.setdefault(key, data[pos:pos + PATCH_BYTES])
    return patches


def find_evnt(buf):
    """First FORM XMID in `buf` -> its EVNT chunk, or None."""
    i = 0
    while i + 12 <= len(buf):
        if buf[i:i + 4] == b'FORM' and buf[i + 8:i + 12] == b'XMID':
            size = int.from_bytes(buf[i + 4:i + 8], 'big')
            end = i + 8 + size
            if size < 4 or end > len(buf):
                return None
            p = i + 12
            while p + 8 <= end:
                csz = int.from_bytes(buf[p + 4:p + 8], 'big')
                if csz > end - (p + 8):
                    return None
                if buf[p:p + 4] == b'EVNT' and csz > 0:
                    return buf[p + 8:p + 8 + csz]
                p += 8 + csz + (csz & 1)
            return None
        i += 1
    return None


def decode_events(evnt):
    """EVNT bytes -> token list, stopping at the first fatal byte.

    Tokens are ('delta', ticks) between event groups; ('note', ch, note, vel,
    dur) / ('off', ch, note) / ('ctrl', ch, num, val) / ('bend', ch, lsb, msb) /
    ('program', ch, prog) / ('ignore',) for events; and a final ('halt', why).
    Tokenising is pure: it reads no voice/program state and emits nothing, so
    the replay in `Sequencer` is what decides each write's tick.
    """
    toks = []
    n = len(evnt)
    pos = 0

    def vlq():
        nonlocal pos
        val = 0
        for _ in range(4):
            if pos >= n:
                return None
            c = evnt[pos]
            pos += 1
            val = (val << 7) | (c & 0x7F)
            if not (c & 0x80):
                return val
        return None

    def halt(why):
        toks.append(('halt', why))
        return toks

    while pos < n:
        b = evnt[pos]
        if b < 0x80:
            pos += 1
            toks.append(('delta', b))
            continue
        pos += 1
        if b == 0xFF:
            if pos >= n:
                return halt('meta type past end')
            typ = evnt[pos]
            pos += 1
            ln = vlq()
            if ln is None:
                return halt('meta length past end')
            if typ == 0x2F:
                return halt('end of track')
            if n - pos < ln:
                return halt('meta body past end')
            pos += ln
        elif b in (0xF0, 0xF7):
            ln = vlq()
            if ln is None or n - pos < ln:
                return halt('sysex body past end')
            pos += ln
        else:
            ch, hi = b & 0x0F, b & 0xF0
            if hi == 0x90:
                if n - pos < 2:
                    return halt('note past end')
                note, vel = evnt[pos], evnt[pos + 1]
                pos += 2
                dur = vlq()
                if dur is None:
                    return halt('note duration past end')
                toks.append(('note', ch, note, vel, dur))
            elif hi == 0x80:
                if n - pos < 2:
                    return halt('note-off past end')
                toks.append(('off', ch, evnt[pos]))
                pos += 2
            elif hi == 0xB0:
                if n - pos < 2:
                    return halt('control past end')
                toks.append(('ctrl', ch, evnt[pos], evnt[pos + 1]))
                pos += 2
            elif hi == 0xC0:
                if pos >= n:
                    return halt('program past end')
                toks.append(('program', ch, evnt[pos]))
                pos += 1
            elif hi == 0xE0:
                if n - pos < 2:
                    return halt('bend past end')
                toks.append(('bend', ch, evnt[pos], evnt[pos + 1]))
                pos += 2
            elif hi == 0xA0:
                if n - pos < 2:
                    return halt('aftertouch past end')
                toks.append(('ignore',))
                pos += 2
            elif hi == 0xD0:
                if pos >= n:
                    return halt('channel-pressure past end')
                toks.append(('ignore',))
                pos += 1
            else:
                return halt('unknown status %#04x' % b)
    return halt('end of stream')


class Sequencer:
    """Replays `decode_events` tokens against an 18-voice OPL3 pool.

    `tick` tags every write with the number of completed tick_once calls; writes
    made by start() carry tick 0, matching the test driver.
    """

    FREE = -1

    def __init__(self, evnt, patches, pitch_tbl=None):
        self.tokens = decode_events(evnt)
        self.patches = patches
        self.pitch_tbl = pitch_tbl if pitch_tbl is not None else computed_pitch_table()
        self.out = []
        self.note_log = []          # (tick, midi, note) when track_notes is set
        self.track_notes = False
        self.reset()

    def reset(self):
        self.tick = 0
        self.idx = 0
        self.wait = 0
        self.playing = False
        self.age = 0
        self.next = OPL_CHANNELS - 1     # driver's reset cursor 0xffff
        self.voice = [{'note': self.FREE} for _ in range(OPL_CHANNELS)]
        self.program = [0] * 16
        self.bank = [0] * 16
        # Per-MIDI-channel controller state (the driver's 0x18f9/0x1909/0x1919/
        # 0x1929/0x1939/0x1949/0x1959/0x1969 arrays). The wheel opens centred and
        # expression full; the rest are BSS zero.
        self.wheel_lsb = [0] * 16
        self.wheel_msb = [0x40] * 16
        self.bend_scale = [0] * 16
        self.volume = [0] * 16
        self.expression = [0x7F] * 16
        self.pan = [0] * 16
        self.mod = [0] * 16
        self.sustain = [0] * 16

    def write(self, reg, val):
        self.out.append((self.tick, reg, val))

    def halt(self):
        for v in range(OPL_CHANNELS):
            self.key_off(v)
        self.playing = False

    def key_off(self, v):
        if self.voice[v]['note'] == self.FREE:
            return
        self.write(ch_reg(v, 0xB0), self.voice[v]['b0'])
        self.voice[v] = {'note': self.FREE}

    def key_off_note(self, midi, note):
        matches = [v for v in range(OPL_CHANNELS)
                   if self.voice[v].get('note') == note
                   and self.voice[v].get('midi') == midi]
        if matches:
            self.key_off(min(matches, key=lambda v: self.voice[v]['age']))

    def alloc_voice(self):
        # SBPRO2.MDI's melodic allocator (0x3095-0x30d8): a monotonic rotation
        # cursor over the 18 slot-owner bytes [0x1a49] takes the next free slot
        # after the cursor, wrapping at 18, so successive notes rotate
        # 0,1,2,... rather than reusing the lowest free channel. key_off leaves
        # the cursor untouched (0x3162). Mirrors sequencer.c's alloc_voice.
        v = self.next
        for _ in range(OPL_CHANNELS):
            v = (v + 1) % OPL_CHANNELS
            if self.voice[v]['note'] == self.FREE:
                self.next = v
                return v
        victim = min(range(OPL_CHANNELS), key=lambda v: self.voice[v]['age'])
        # PORT: all 18 busy — steal the oldest (the driver steals by quietest
        # voice at 0x36f6; not reached in the compared window).
        self.key_off(victim)
        self.next = victim
        return victim

    def apply(self, v, mask):
        """Re-apply the register families selected by `mask` from the voice's
        cached patch, mirroring fam_apply (SBPRO2.MDI 0x3184)."""
        p = self.voice[v].get('patch')
        base = OPL_SLOT[v]
        if p is not None:
            for bit, writes in FAMILY_WRITES:
                if mask & bit:
                    for reg, i in writes:
                        self.write(reg + base, p[i])
            if mask & FAM_CONN:
                self.write(ch_reg(v, 0xC0), p[8] | 0x30)
        if mask & FAM_FREQ:
            # 0x35fa reads the wheel from the voice's own MIDI channel, so a
            # key-on uses the live wheel just as a controller re-apply does.
            ch = self.voice[v].get('midi', 0)
            bend = self.channel_bend(ch) if 0 <= ch < 16 else 0
            block, val = self.block_fnum(self.voice[v]['index'], bend)
            b0 = (block << 2) | ((val >> 8) & 0x03)
            self.voice[v]['b0'] = b0
            self.write(ch_reg(v, 0xA0), val & 0xFF)
            self.write(ch_reg(v, 0xB0), b0 | 0x20)

    def bend_of(self, wheel14, scale):
        """0x360f-0x3625: the driver keeps only the low 16 bits of the imul."""
        val = ((wheel14 - 0x2000) >> 5) * scale
        return ((val + 0x8000) & 0xFFFF) - 0x8000

    def channel_bend(self, ch):
        return self.bend_of((self.wheel_msb[ch] << 7) | self.wheel_lsb[ch],
                            self.bend_scale[ch])

    def block_fnum(self, index, bend):
        """0x35fa: the driver's folded fine-step lookup -> (block, fnum)."""
        folded = index - 0x18
        while True:
            folded += 0xc
            if folded >= 0:
                break
        while folded > 0x5f:
            folded -= 0xc
        # 0x3646/0x3648 are 16-bit adds; wrap before the 0x364e shift.
        fine = (bend + (folded << 8) + 8) & 0xFFFF
        if fine >= 0x8000:
            fine -= 0x10000
        fine >>= 4
        while fine < 0:
            fine += 0xc0
        while fine > 0x5ff:
            fine -= 0xc0
        idx2 = fine >> 4
        sem, octave = idx2 % 12, idx2 // 12
        val = self.pitch_tbl[16 * sem + (fine & 0xf)]
        block = octave - 1 + (1 if val < 0 else 0)
        if block < 0:
            block += 1
            val >>= 1
        return block, val

    def control(self, status, a, b):
        """0x3b54: store the controller, then re-apply the affected family for
        active voices on the same MIDI channel."""
        hi, ch = status & 0xF0, status & 0x0F
        if ch >= 16:
            return
        mask = None
        if hi == 0xE0:
            self.wheel_lsb[ch], self.wheel_msb[ch] = a, b
            mask = FAM_FREQ
        elif hi == 0xB0:
            if a == 121:
                self.sustain[ch] = 0
                self.mod[ch] = 0
                self.expression[ch] = 0x7F
                self.wheel_lsb[ch], self.wheel_msb[ch] = 0, 0x40
                mask = FAM_FREQ | FAM_TL | FAM_AMVIB
            else:
                field, mask = CTRL_STORES.get(a, (None, None))
                if field is not None:
                    getattr(self, field)[ch] = b
        if mask is None:
            return
        for v, voice in enumerate(self.voice):
            if voice.get('note') == self.FREE or voice.get('midi') != ch:
                continue
            self.apply(v, mask)

    def key_on(self, midi, note, vel, dur):
        if not (0 <= midi < 16 and 0 <= note <= 127):
            return
        if self.track_notes:
            self.note_log.append((self.tick, midi, note))
        if midi == 9:
            key = (0x7F << 8) | (note & 0xFF)
        else:
            key = (self.bank[midi] << 8) | self.program[midi]
        v = self.alloc_voice()
        # The driver keys a note through its patch's base byte, not its MIDI
        # pitch (SBPRO2.MDI 0x35fa-0x36a6): melodic adds the base to the note
        # (base 0 in every melodic FAT.OPL entry, so this is the note);
        # percussion uses the base alone. Mirrors sequencer.c's key_on.
        p = self.patches.get(key)
        idx = note
        if midi == 9 and p is not None:
            idx = p[2]
        idx = max(0, min(idx, 127))
        self.voice[v] = {'midi': midi, 'note': note, 'release': dur,
                         'age': self.age, 'b0': 0, 'patch': p, 'index': idx}
        self.age += 1
        self.apply(v, FAM_ALL)

    def _advance(self):
        """Consume tokens until a delta is read or the stream halts."""
        while self.idx < len(self.tokens):
            tok = self.tokens[self.idx]
            self.idx += 1
            kind = tok[0]
            if kind == 'delta':
                self.wait = tok[1]
                return
            if kind == 'halt':
                self.halt()
                return
            if kind == 'note':
                _, ch, note, vel, dur = tok
                if vel == 0:
                    self.key_off_note(ch, note)
                else:
                    self.key_on(ch, note, vel, dur)
            elif kind == 'off':
                _, ch, note = tok
                self.key_off_note(ch, note)
            elif kind == 'ctrl':
                _, ch, num, val = tok
                if num == 0:
                    self.bank[ch] = val
                else:
                    self.control(0xB0 | ch, num, val)
            elif kind == 'bend':
                _, ch, lsb, msb = tok
                self.control(0xE0 | ch, lsb, msb)
            elif kind == 'program':
                _, ch, prog = tok
                self.program[ch] = prog
            # 'ignore': cosmetically decoded, no register effect
        self.halt()

    def tick_once(self):
        if not self.playing:
            return
        for v in range(OPL_CHANNELS):
            vc = self.voice[v]
            if vc['note'] == self.FREE:
                continue
            if vc['release'] <= 1:
                self.key_off(v)
            else:
                vc['release'] -= 1
        if self.wait > 0:
            self.wait -= 1
            if self.wait > 0:
                return
        self._advance()

    def start(self):
        self.halt()
        self.reset()
        self.playing = True
        self.write(0x01, 0x20)      # waveform-select enable
        self.write(0x105, 0x01)     # capture's OPL3-mode enable (output-neutral)

    def run(self, ticks):
        self.start()
        for _ in range(ticks):
            self.tick += 1
            self.tick_once()
        return self.out


def emit(events, out):
    for tick, reg, val in events:
        out.write('%8d %#06x %#04x\n' % (tick, reg, val))


def read(path):
    with open(path, 'rb') as f:
        return f.read()


def cmd_info(music, patches_path, evnt, events):
    print('%s: %d EVNT bytes, patch bank %s, %d register writes over %d ticks'
          % (music, len(evnt), patches_path, len(events),
             events[-1][0] if events else 0))
    if events:
        print('  first: tick=%d reg=%#04x val=%#04x' % events[0])
        print('  last:  tick=%d reg=%#04x val=%#04x' % events[-1])


def self_test():
    for note, want in NOTE_ANCHORS.items():
        assert NOTE_TAB[note] == want, (note, NOTE_TAB[note], want)

    # Malformed patch banks are rejected whole, matching patches.c.
    assert load_patches(None) is None
    assert load_patches(b'') is None
    assert load_patches(b'\x00' * 12) is None
    bad_offset = bytes([0x00, 0x00, 0x40, 0x00, 0x00, 0x00,
                        0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00])
    assert load_patches(bad_offset) is None, 'payload offset past buffer rejected'

    # A one-note synthetic bank: program 0 -> patch 0 -> note 60 for 2 ticks.
    ev = bytes([0xC0, 0x00,                     # program change ch0
                0x90, 0x3C, 0x7F, 0x02,         # note on 60 vel 127 dur 2
                0xFF, 0x2F, 0x00])              # end of track
    body = b'XMID' + b'EVNT' + len(ev).to_bytes(4, 'big') + ev
    form = b'FORM' + (len(body)).to_bytes(4, 'big') + body
    patches = {0: bytes([0x0E, 0x00, 0x00, 1, 2, 3, 4, 5, 0x0E, 6, 7, 8, 9, 10])}
    seq = Sequencer(find_evnt(form), patches)
    out = seq.run(20)
    assert seq.playing is False, 'track ended with FF 2F'
    # start writes + note-on patch/operator writes + key-off.
    regs = [r for _, r, _ in out]
    assert regs[0] == 0x01 and regs[1] == 0x105, out
    assert 0xB0 in regs, out
    print('self-test ok: %d events' % len(out))


def capture_keyons(events):
    """A DRO event stream -> [(ms, block, fnum)] for 0xB0..0xB8 key-ons.

    A key-on's fnum low byte is the most recent 0xA0+ch write on the same chip;
    block/fnum-high come from the 0xB0 value (bit 5 is the key bit)."""
    last_a = {}
    out = []
    for ms, reg, val in events:
        if 0xA0 <= reg <= 0xA8:
            last_a[reg - 0xA0] = val
        elif 0xB0 <= reg <= 0xB8 and (val & 0x20):
            ch = reg - 0xB0
            out.append((ms, (val >> 2) & 7,
                        ((val & 3) << 8) | last_a.get(ch, 0)))
    return out


def cmd_capture_anchors(music, capture):
    """Re-derive NOTE_CAPTURE from the capture; nonzero exit on any mismatch.

    Pairs the title bank's note-ons to the capture's key-ons by note-on time
    (port tick -> ms), not by position, so it does not assume the port's voice
    or allocation order. Percussion (MIDI channel 9) is skipped: its fnum
    mapping is the documented divergence (audio.md item 6)."""
    import opl_trace
    keyons = capture_keyons(opl_trace.parse(read(capture))[1])
    patches = load_patches(read(os.path.join(
        os.path.dirname(os.path.abspath(music)), 'FAT.OPL')))
    if patches is None:
        print('no usable patch bank beside %s' % music, file=sys.stderr)
        return 1
    seq = Sequencer(find_evnt(read(music)), patches)
    seq.track_notes = True
    seq.run(100000)

    checked = set()
    for tick, midi, note in seq.note_log:
        if midi == 9 or note not in NOTE_CAPTURE or note in checked:
            continue
        ms = (tick - 60) * 1000 // 120
        hits = [k for k in keyons if abs(k[0] - ms) <= 3]
        if len(hits) != 1:
            print('note %d: %d capture key-ons near ms %d, expected 1'
                  % (note, len(hits), ms), file=sys.stderr)
            return 1
        _, block, fnum = hits[0]
        want = note_to_block_fnum(note)
        if (block, fnum) != want:
            print('note %d: capture block %d fnum %#05x != formula %r'
                  % (note, block, fnum, want), file=sys.stderr)
            return 1
        checked.add(note)
        print('anchor note %d = block %d fnum %#05x (capture ms %d, formula agrees)'
              % (note, block, fnum, ms))
    if checked != set(NOTE_CAPTURE):
        print('capture anchors not all found: %r'
              % sorted(set(NOTE_CAPTURE) - checked), file=sys.stderr)
        return 1
    print('capture anchors verified: %s'
          % ', '.join(str(n) for n in NOTE_CAPTURE))
    return 0


def main(argv):
    if not argv or argv[0] in ('-h', '--help'):
        print(__doc__.strip())
        return 0
    if argv[0] == '--self-test':
        self_test()
        return 0
    if argv[0] == '--capture-anchors':
        if len(argv) != 3:
            print('usage: opl_seq.py --capture-anchors <music> <capture.dro>',
                  file=sys.stderr)
            return 2
        return cmd_capture_anchors(argv[1], argv[2])

    music = None
    patches_path = None
    info = False
    i = 0
    while i < len(argv):
        a = argv[i]
        if a in ('--trace',):
            pass
        elif a == '--info':
            info = True
        elif a == '--patches':
            if i + 1 >= len(argv):
                print('--patches needs a path', file=sys.stderr)
                return 2
            i += 1
            patches_path = argv[i]
        elif a.startswith('-'):
            print('unknown option %s' % a, file=sys.stderr)
            return 2
        else:
            music = a
        i += 1
    if music is None:
        print('usage: opl_seq.py <music> [--trace|--info] [--patches PATH]',
              file=sys.stderr)
        return 2
    if patches_path is None:
        patches_path = os.path.join(os.path.dirname(os.path.abspath(music)), 'FAT.OPL')

    evnt = find_evnt(read(music))
    if evnt is None:
        print('%s: no FORM XMID / EVNT chunk' % music, file=sys.stderr)
        return 1
    patches = load_patches(read(patches_path))
    if patches is None:
        print('%s: no usable patch bank (missing, empty, truncated, or bad offset)'
              % patches_path, file=sys.stderr)
        return 1

    pitch_tbl = load_pitch_table(os.path.join(
        os.path.dirname(os.path.abspath(music)), 'SBPRO2.MDI'))
    if pitch_tbl is None:
        print('SBPRO2.MDI pitch table missing beside %s' % music, file=sys.stderr)
        return 1

    seq = Sequencer(evnt, patches, pitch_tbl)
    events = seq.run(100000)
    if info:
        cmd_info(music, patches_path, evnt, events)
    else:
        emit(events, sys.stdout)
    return 0


if __name__ == '__main__':
    signal.signal(signal.SIGPIPE, signal.SIG_DFL)
    sys.exit(main(sys.argv[1:]))
