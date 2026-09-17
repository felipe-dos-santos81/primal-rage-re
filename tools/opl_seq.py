"""Independent Python oracle for the music sequencer's OPL register stream.

Reads one shipped music bank (an XMIDI `FORM ... XMID` bundle, either the GRA
file itself or the bare bank) plus the `FAT.OPL` patch bank and emits, for each
port tick, the register writes the port's sequencer would make:

    tick reg value        three columns, millisecond-free

in exactly the format `tools/opl_trace.py` uses, so the two streams (Python
oracle, captured original) and the C sequencer's stream can be diffed pairwise.

This is a from-scratch decoder: it shares no code with the C port and does not
import it. It implements the same published contract (port/spec/audio.md "Music
event grammar" / "FAT.OPL patch bank" and sequencer.h) so that a byte difference
between it and the C stream is a bug in one of them, not a data difference.

usage:
  opl_seq.py <music> [--trace]          # "tick reg value" lines (default)
  opl_seq.py <music> --info             # summary only
  opl_seq.py <music> --trace --patches PATH
  opl_seq.py --self-test
"""
import os
import signal
import sys

# OPL2 operator slot for each sequencer channel (0x20+slot, 0x40+slot, ...).
OPL_SLOT = (0, 1, 2, 8, 9, 10, 16, 17, 18)

# MIDI note -> (block, fnum) for a 49716 Hz OPL clock, fnum <= 1023 at the
# lowest block. Transcribed from the port's verified table (sequencer.c); it is
# data pinned against the capture, not port logic.
NOTE_TAB = (
    (0, 0x0AC), (0, 0x0B7), (0, 0x0C2), (0, 0x0CD), (0, 0x0D9), (0, 0x0E6),
    (0, 0x0F4), (0, 0x102), (0, 0x112), (0, 0x122), (0, 0x133), (0, 0x146),
    (0, 0x159), (0, 0x16D), (0, 0x183), (0, 0x19A), (0, 0x1B3), (0, 0x1CC),
    (0, 0x1E8), (0, 0x205), (0, 0x223), (0, 0x244), (0, 0x267), (0, 0x28B),
    (0, 0x2B2), (0, 0x2DB), (0, 0x306), (0, 0x334), (0, 0x365), (0, 0x399),
    (0, 0x3CF),
    (1, 0x205), (1, 0x223), (1, 0x244), (1, 0x267), (1, 0x28B), (1, 0x2B2),
    (1, 0x2DB), (1, 0x306), (1, 0x334), (1, 0x365), (1, 0x399), (1, 0x3CF),
    (2, 0x205), (2, 0x223), (2, 0x244), (2, 0x267), (2, 0x28B), (2, 0x2B2),
    (2, 0x2DB), (2, 0x306), (2, 0x334), (2, 0x365), (2, 0x399), (2, 0x3CF),
    (3, 0x205), (3, 0x223), (3, 0x244), (3, 0x267), (3, 0x28B), (3, 0x2B2),
    (3, 0x2DB), (3, 0x306), (3, 0x334), (3, 0x365), (3, 0x399), (3, 0x3CF),
    (4, 0x205), (4, 0x223), (4, 0x244), (4, 0x267), (4, 0x28B), (4, 0x2B2),
    (4, 0x2DB), (4, 0x306), (4, 0x334), (4, 0x365), (4, 0x399), (4, 0x3CF),
    (5, 0x205), (5, 0x223), (5, 0x244), (5, 0x267), (5, 0x28B), (5, 0x2B2),
    (5, 0x2DB), (5, 0x306), (5, 0x334), (5, 0x365), (5, 0x399), (5, 0x3CF),
    (6, 0x205), (6, 0x223), (6, 0x244), (6, 0x267), (6, 0x28B), (6, 0x2B2),
    (6, 0x2DB), (6, 0x306), (6, 0x334), (6, 0x365), (6, 0x399), (6, 0x3CF),
    (7, 0x205), (7, 0x223), (7, 0x244), (7, 0x267), (7, 0x28B), (7, 0x2B2),
    (7, 0x2DB), (7, 0x306), (7, 0x334), (7, 0x365), (7, 0x399), (7, 0x3CF),
    (7, 0x3FF), (7, 0x3FF), (7, 0x3FF), (7, 0x3FF), (7, 0x3FF), (7, 0x3FF),
    (7, 0x3FF), (7, 0x3FF), (7, 0x3FF), (7, 0x3FF), (7, 0x3FF), (7, 0x3FF),
    (7, 0x3FF),
)


def load_patches(data):
    """FAT.OPL bytes -> {key: 14-byte payload}, matching patches.c's bounds."""
    entries, off = {}, 0
    while off + 6 <= len(data):
        key = data[off] | (data[off + 1] << 8)
        if key == 0xFFFF:
            off += 2
            break
        entries[key] = data[off + 2] | (data[off + 3] << 8) | \
            (data[off + 4] << 16) | (data[off + 5] << 24)
        off += 6
    for key, pos in entries.items():
        entries[key] = data[pos:pos + 14]
    return entries


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


class Sequencer:
    """The port's sequencer contract, decoded independently.

    `tick` tags every write with the number of completed seq_tick calls; writes
    made by start() carry tick 0, matching the test driver.
    """

    FREE = -1

    def __init__(self, evnt, patches):
        self.e = evnt
        self.patches = patches
        self.out = []
        self.tick = 0
        self.reset()

    def reset(self):
        self.pos = 0
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
        best = None
        for v in range(9):
            if self.voice[v].get('note') == note and self.voice[v].get('midi') == midi:
                if best is None or self.voice[v]['age'] < self.voice[best]['age']:
                    best = v
        if best is not None:
            self.key_off(best)

    def alloc_voice(self):
        best = None
        for v in range(9):
            if self.voice[v]['note'] == self.FREE:
                return v
            if best is None or self.voice[v].get('age', 0) < self.voice[best].get('age', 0):
                best = v
        # All nine busy: steal the oldest (PORT decision; original unverified).
        self.key_off(best)
        return best

    def apply_patch(self, ch, key):
        p = self.patches.get(key)
        if p is None or len(p) < 14:
            return
        base = OPL_SLOT[ch]
        self.write(0x20 + base, p[3])
        self.write(0x23 + base, p[9])
        self.write(0x40 + base, p[4])
        self.write(0x43 + base, p[10])
        self.write(0x60 + base, p[5])
        self.write(0x63 + base, p[11])
        self.write(0x80 + base, p[6])
        self.write(0x83 + base, p[12])
        self.write(0xE0 + base, p[7])
        self.write(0xE3 + base, p[13])
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

    def read_vlq(self):
        val = 0
        for _ in range(4):
            if self.pos >= len(self.e):
                return False, 0
            c = self.e[self.pos]
            self.pos += 1
            val = (val << 7) | (c & 0x7F)
            if not (c & 0x80):
                return True, val
        return False, 0

    def process(self):
        e = self.e
        n = len(e)
        while self.pos < n:
            b = e[self.pos]
            if b < 0x80:
                self.pos += 1
                self.wait = b
                return
            self.pos += 1
            if b == 0xFF:
                if self.pos >= n:
                    self.halt()
                    return
                typ = e[self.pos]
                self.pos += 1
                ok, ln = self.read_vlq()
                if not ok:
                    self.halt()
                    return
                if typ == 0x2F:                 # loop / end; RBRN un-modelled
                    self.halt()
                    return
                if n - self.pos < ln:
                    self.halt()
                    return
                self.pos += ln
            elif b in (0xF0, 0xF7):
                ok, ln = self.read_vlq()
                if not ok:
                    self.halt()
                    return
                if n - self.pos < ln:
                    self.halt()
                    return
                self.pos += ln
            else:
                ch, hi = b & 0x0F, b & 0xF0
                if hi == 0x90:
                    if n - self.pos < 2:
                        self.halt()
                        return
                    note, vel = e[self.pos], e[self.pos + 1]
                    self.pos += 2
                    ok, dur = self.read_vlq()
                    if not ok:
                        self.halt()
                        return
                    if vel == 0:
                        self.key_off_note(ch, note)
                    else:
                        self.key_on(ch, note, vel, dur)
                elif hi == 0x80:
                    if n - self.pos < 2:
                        self.halt()
                        return
                    note = e[self.pos]
                    self.pos += 2
                    self.key_off_note(ch, note)
                elif hi == 0xB0:
                    if n - self.pos < 2:
                        self.halt()
                        return
                    c, val = e[self.pos], e[self.pos + 1]
                    self.pos += 2
                    if c == 0:
                        self.bank[ch] = val
                elif hi == 0xC0:
                    if self.pos >= n:
                        self.halt()
                        return
                    self.program[ch] = e[self.pos]
                    self.pos += 1
                elif hi in (0xA0, 0xE0):
                    if n - self.pos < 2:
                        self.halt()
                        return
                    self.pos += 2
                elif hi == 0xD0:
                    if self.pos >= n:
                        self.halt()
                        return
                    self.pos += 1
                else:
                    self.halt()
                    return
        self.halt()

    def start(self):
        self.halt()
        self.reset()
        self.playing = True
        self.write(0x01, 0x20)      # waveform-select enable
        self.write(0x105, 0x01)     # capture's OPL3-mode enable (output-neutral)

    def tick_once(self):
        if not self.playing:
            return
        for v in range(9):
            vc = self.voice[v]
            if vc['note'] == self.FREE:
                continue
            if vc['release'] == 0 or vc['release'] - 1 == 0:
                self.key_off(v)
            else:
                vc['release'] -= 1
        if self.wait > 0 and self.wait - 1 > 0:
            self.wait -= 1
            return
        self.process()

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
    # A one-note synthetic bank: program 0 -> patch 0 -> note 60 for 2 ticks.
    ev = bytes([0xC0, 0x00,                     # program change ch0
                0x90, 0x3C, 0x7F, 0x02,         # note on 60 vel 127 dur 2
                0xFF, 0x2F, 0x00])              # end of track
    body = b'XMID' + b'EVNT' + len(ev).to_bytes(4, 'big') + ev
    form = b'FORM' + (len(body)).to_bytes(4, 'big') + body
    patches = {}
    patches[0] = bytes([0x0E, 0x00, 0x00, 1, 2, 3, 4, 5, 0x0E, 6, 7, 8, 9, 10])
    seq = Sequencer(find_evnt(form), patches)
    out = seq.run(20)
    assert seq.playing is False, 'track ended with FF 2F'
    # start writes + note-on patch/operator writes + key-off.
    kinds = [r for _, r, _ in out]
    assert kinds[0] == 0x01 and kinds[1] == 0x105, out
    assert 0xB0 in kinds, out
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
    if not patches:
        print('%s: no patch bank' % patches_path, file=sys.stderr)
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
