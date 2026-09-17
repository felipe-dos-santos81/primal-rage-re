"""Decode a DOSBox-X raw-OPL capture (DBRAWOPL / DRO v2) into an ordered stream.

The capture is produced by DOSBox-X's internal shell command

    DX-CAPTURE /O <program.exe> [args]

which calls CAPTURE_StartOPL() (DOSBox-X src/shell/shell_cmds.cpp, CMD_DXCAPTURE)
and then runs <program>. Recording actually begins at the guest's first FM
note-on (Adlib::Capture::DoWrite: reg 0xB0-0xB8 with bit 0x20, or percussion
0xBD), so a file is only produced if the program plays FM. The emulator writes
"<program>_NNN.dro" into the `captures` directory.

File layout (DOSBox-X src/hardware/adlib.cpp, struct RawHeader + Capture,
packed, little-endian):

    0x00  char[8]  id            "DBRAWOPL"
    0x08  u16      versionHigh   (2 for DRO v2)
    0x0a  u16      versionLow    (0)
    0x0c  u32      commands      number of cmd/data pairs that follow
    0x10  u32      milliseconds  total captured span
    0x14  u8       hardware      0=OPL2, 1=dual-OPL2, 2=OPL3
    0x15  u8       format        0 = cmd/data interleaved
    0x16  u8       compression   0 = none
    0x17  u8       delay256      raw code: delay of (data+1) ms
    0x18  u8       delayShift8   raw code: delay of (data+1)*256 ms
    0x19  u8       tableSize     bytes of the register table
    0x1a  u8[]     table[tableSize]   raw-code -> register number
    then command bytes: repeated (rawCode, value) pairs. A rawCode equal to
    delay256 / delayShift8 is a delay command; otherwise `table[rawCode & 0x7f]`
    is the register and bit 0x80 selects the OPL3 second register set (0x100+).

`tick` is milliseconds accumulated from the capture's own delay commands, never
wall-clock, so the stream is reproducible across hosts.

usage:
  opl_trace.py FILE.dro              # "tick reg value" lines
  opl_trace.py --info FILE.dro       # header + summary only
  opl_trace.py --json FILE.dro       # JSON array of [tick, reg, value]
  opl_trace.py --self-test           # built-in round-trip check
"""
import json
import signal
import struct
import sys

HEADER = struct.Struct('<8sHHIIBBBBBB')   # id, verHi, verLo, cmds, ms, hw, fmt, comp, d256, dsh8, tsize
assert HEADER.size == 0x1a, HEADER.size

HW = {0: 'OPL2', 1: 'dual-OPL2', 2: 'OPL3'}


def parse(data):
    """Return (header_dict, [(tick_ms, reg, value), ...])."""
    if len(data) < HEADER.size or data[:8] != b'DBRAWOPL':
        raise ValueError('not a DBRAWOPL capture')
    (magic, ver_hi, ver_lo, commands, ms, hw, fmt, comp,
     delay256, delay_shift8, table_size) = HEADER.unpack_from(data, 0)
    if ver_hi != 2:
        raise ValueError('unsupported DBRAWOPL version %d.%d (this tool decodes v2 '
                         'captures produced by DX-CAPTURE /O)' % (ver_hi, ver_lo))
    if fmt != 0:
        raise ValueError('unsupported DRO format %d (only interleaved cmd/data)' % fmt)
    if comp != 0:
        raise ValueError('unsupported DRO compression %d' % comp)

    off = HEADER.size
    table = data[off:off + table_size]
    if len(table) != table_size:
        raise ValueError('truncated conversion table')
    off += table_size

    events = []
    tick = 0
    end = len(data)
    while off + 1 < end:
        code, value = data[off], data[off + 1]
        off += 2
        if code == delay256:
            tick += value + 1
        elif code == delay_shift8:
            tick += (value + 1) * 256
        else:
            idx = code & 0x7f
            if idx >= table_size:
                raise ValueError('raw code %#x outside table (%d entries)' % (code, table_size))
            reg = table[idx]
            if code & 0x80:
                reg |= 0x100
            events.append((tick, reg, value))

    header = {
        'version': '%d.%d' % (ver_hi, ver_lo),
        'hardware': HW.get(hw, 'unknown(%d)' % hw),
        'commands': commands,
        'milliseconds': ms,
        'table_size': table_size,
        'events': len(events),
    }
    return header, events


def cmd_info(path):
    data = open(path, 'rb').read()
    header, events = parse(data)
    print('%s: %s, %s, %d cmd/data pairs, %d ms, table=%d bytes, %d register writes'
          % (path, header['version'], header['hardware'], header['commands'],
             header['milliseconds'], header['table_size'], header['events']))
    if events:
        regs = sorted({r for _, r, _ in events})
        print('  first: tick=%d reg=%#04x val=%#04x' % events[0])
        print('  last:  tick=%d reg=%#04x val=%#04x' % events[-1])
        print('  registers: %s' % ' '.join('%#04x' % r for r in regs[:32]) +
              (' ...' if len(regs) > 32 else ''))
        print('  span: %d ms' % events[-1][0])


def cmd_dump(path, as_json):
    _, events = parse(open(path, 'rb').read())
    if as_json:
        print(json.dumps(events))
    else:
        for tick, reg, val in events:
            print('%8d %#06x %#04x' % (tick, reg, val))


def self_test():
    # minimal v2 file: table maps raw 0->0x20, 1->0x40; delay256=2, delayShift8=3
    table = bytes([0x20, 0x40])
    body = bytes([0x00, 0x01,      # reg 0x20 = 0x01
                  0x02, 0x09,      # delay (9+1)=10 ms
                  0x01, 0x77,      # reg 0x40 = 0x77
                  0x03, 0x00,      # delay (0+1)*256 = 256 ms
                  0x80, 0xAB])     # second set: reg 0x100|0x20, val 0xAB
    data = HEADER.pack(b'DBRAWOPL', 2, 0, 5, 266, 2, 0, 0, 2, 3, len(table)) + table + body
    header, events = parse(data)
    assert header['hardware'] == 'OPL3', header
    assert events == [(0, 0x20, 0x01),
                      (10, 0x40, 0x77),
                      (266, 0x120, 0xAB)], events
    print('self-test ok: %d events' % len(events))


def main(argv):
    if not argv or argv[0] in ('-h', '--help'):
        print(__doc__.strip())
        return 0
    if argv[0] == '--self-test':
        self_test()
        return 0
    as_json = False
    info = False
    args = []
    for a in argv:
        if a == '--json':
            as_json = True
        elif a == '--info':
            info = True
        else:
            args.append(a)
    if len(args) != 1:
        print('usage: opl_trace.py [--info|--json] FILE.dro', file=sys.stderr)
        return 2
    if info:
        cmd_info(args[0])
    else:
        cmd_dump(args[0], as_json)
    return 0


if __name__ == '__main__':
    signal.signal(signal.SIGPIPE, signal.SIG_DFL)  # die quietly when piped to head
    sys.exit(main(sys.argv[1:]))
