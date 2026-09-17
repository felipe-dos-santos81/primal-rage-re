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
  * the note -> (block, fnum) table is COMPUTED from the OPL clock (49716 Hz)
    and the MIDI pitch formula and pinned to capture-verified anchors, where
    sequencer.c carries the table as a copied literal;
  * voice selection and patch application are table / comprehension driven
    rather than a branch-for-branch copy of the port's helpers.

What independence does and does not buy. Agreement with the C stream is real
evidence that the *implementation* of the shared contract is faithful — it
catches transcription and coding errors, including in the note table. It is NOT
evidence that the contract matches the original driver: the reconstruction
choices (nine-voice pool, oldest-steal, per-note whole-patch re-apply) are the
port's and are unverified against the capture (spec "Known capture divergences").

usage:
  opl_seq.py <music> [--trace]          # "tick reg value" lines (default)
  opl_seq.py <music> --info             # summary only
  opl_seq.py <music> --trace --patches PATH
  opl_seq.py --self-test
"""
import os
import signal
import sys

# OPL clock the port pins its note table to (Hz). sequencer.c NOTE_TAB was
# verified against the capture at this rate; deriving the table from it here
# turns a copied literal into an independently computed value.
OPL_CLOCK = 49716.0

# OPL2 operator slot for each sequencer channel (0x20+slot, 0x40+slot, ...).
OPL_SLOT = (0, 1, 2, 8, 9, 10, 16, 17, 18)

# PATCH_BYTES payload -> register, in the order the port writes them: modulator
# fields then carrier fields (carrier at slot+3), each family in register order.
PATCH_BYTES = 14
PATCH_MAX = 256
PATCH_WRITES = (
    (0x20, 3), (0x23, 9),      # AM/VIB/EG/KSR/MULT
    (0x40, 4), (0x43, 10),     # KSL/TL
    (0x60, 5), (0x63, 11),     # attack/decay
    (0x80, 6), (0x83, 12),     # sustain/release
    (0xE0, 7), (0xE3, 13),     # waveform select
)


def note_to_block_fnum(note):
    """MIDI note -> (block, fnum): lowest block with fnum <= 1023, rounded."""
    freq = 440.0 * 2.0 ** ((note - 69) / 12.0)
    for block in range(8):
        fnum = int(round(freq * 2 ** (20 - block) / OPL_CLOCK))
        if fnum <= 1023:
            return block, fnum
    return 7, 1023


NOTE_TAB = tuple(note_to_block_fnum(n) for n in range(128))

# Notes on which the port's literal table is pinned to the capture (sequencer.c
# NOTE_TAB comment, Task 8). The computed table must reproduce these.
NOTE_ANCHORS = {0: (0, 0x0AC), 31: (1, 0x205), 84: (5, 0x2B2), 127: (7, 0x3FF)}


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
    dur) / ('off', ch, note) / ('ctrl', ch, num, val) / ('program', ch, prog) /
    ('ignore',) for events; and a final ('halt', why). Tokenising is pure: it
    reads no voice/program state and emits nothing, so the replay in `Sequencer`
    is what decides each write's tick.
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
            elif hi in (0xA0, 0xE0):
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
    """Replays `decode_events` tokens against a nine-voice OPL pool.

    `tick` tags every write with the number of completed tick_once calls; writes
    made by start() carry tick 0, matching the test driver.
    """

    FREE = -1

    def __init__(self, evnt, patches):
        self.tokens = decode_events(evnt)
        self.patches = patches
        self.out = []
        self.reset()

    def reset(self):
        self.tick = 0
        self.idx = 0
        self.wait = 0
        self.playing = False
        self.age = 0
        self.voice = [{'note': self.FREE} for _ in range(9)]
        self.program = [0] * 16
        self.bank = [0] * 16

    def write(self, reg, val):
        self.out.append((self.tick, reg, val))

    def halt(self):
        for v in range(9):
            self.key_off(v)
        self.playing = False

    def key_off(self, v):
        if self.voice[v]['note'] == self.FREE:
            return
        self.write(0xB0 + v, self.voice[v]['b0'])
        self.voice[v] = {'note': self.FREE}

    def key_off_note(self, midi, note):
        matches = [v for v in range(9)
                   if self.voice[v].get('note') == note
                   and self.voice[v].get('midi') == midi]
        if matches:
            self.key_off(min(matches, key=lambda v: self.voice[v]['age']))

    def alloc_voice(self):
        for v in range(9):
            if self.voice[v]['note'] == self.FREE:
                return v
        victim = min(range(9), key=lambda v: self.voice[v]['age'])
        # PORT: all nine busy — steal the oldest (original's exhaustion policy
        # unverified; see mixer.h for the same unknown on the sample path).
        self.key_off(victim)
        return victim

    def apply_patch(self, ch, key):
        p = self.patches.get(key)
        if p is None:
            return
        base = OPL_SLOT[ch]
        for reg, i in PATCH_WRITES:
            self.write(reg + base, p[i])
        self.write(0xC0 + ch, p[8] | 0x30)

    def key_on(self, midi, note, vel, dur):
        if not (0 <= midi < 16 and 0 <= note <= 127):
            return
        if midi == 9:
            key = (0x7F << 8) | (note & 0xFF)
        else:
            key = (self.bank[midi] << 8) | self.program[midi]
        v = self.alloc_voice()
        self.apply_patch(v, key)
        block, fnum = NOTE_TAB[note]
        b0 = (block << 2) | ((fnum >> 8) & 0x03)
        self.voice[v] = {'midi': midi, 'note': note, 'release': dur,
                         'age': self.age, 'b0': b0}
        self.age += 1
        self.write(0xA0 + v, fnum & 0xFF)
        self.write(0xB0 + v, b0 | 0x20)

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
            elif kind == 'program':
                _, ch, prog = tok
                self.program[ch] = prog
            # 'ignore': cosmetically decoded, no register effect
        self.halt()

    def tick_once(self):
        if not self.playing:
            return
        for v in range(9):
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


def main(argv):
    if not argv or argv[0] in ('-h', '--help'):
        print(__doc__.strip())
        return 0
    if argv[0] == '--self-test':
        self_test()
        return 0

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

    seq = Sequencer(evnt, patches)
    events = seq.run(100000)
    if info:
        cmd_info(music, patches_path, evnt, events)
    else:
        emit(events, sys.stdout)
    return 0


if __name__ == '__main__':
    signal.signal(signal.SIGPIPE, signal.SIG_DFL)
    sys.exit(main(sys.argv[1:]))
