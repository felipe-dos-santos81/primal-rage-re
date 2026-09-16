"""Report the LE/DOS4GW layout of PRAGE.EXE (and the companion INDEX file).

PRAGE.EXE is a Microsoft DOS/4GW *bound* executable: an MZ stub with the LE
header embedded at a non-zero offset. This tool locates the LE header, prints
the object table, page map and the on-disk page-data ranges, and decodes INDEX.

usage:
  le_info.py PRAGE.EXE
  le_info.py --index INDEX
"""
import struct
import sys

OBJ_FLAGS = [(0x0001, 'read'), (0x0002, 'write'), (0x0004, 'exec'), (0x0008, 'resource'),
             (0x0010, 'discardable'), (0x0020, 'shared'), (0x0040, 'preload'),
             (0x1000, '16:16 alias'), (0x2000, '32-bit'), (0x4000, 'big')]


def find_le(d):
    """Return the file offset of the first plausible LE header."""
    i = d.find(b'LE')
    while i >= 0:
        try:
            bo, wo = d[i + 2], d[i + 3]
            cpu, os = struct.unpack_from('<HH', d, i + 8)
            npages = struct.unpack_from('<I', d, i + 0x14)[0]
            psz = struct.unpack_from('<I', d, i + 0x28)[0]
            nobj = struct.unpack_from('<I', d, i + 0x44)[0]
            objtab = struct.unpack_from('<I', d, i + 0x40)[0]
            if (bo == 0 and wo == 0 and cpu in (1, 2, 3, 4, 5) and os in (1, 2, 3, 4)
                    and psz in (0x200, 0x400, 0x1000, 0x2000, 0x4000)
                    and 0 < nobj < 64 and objtab < 0x2000 and 0 < npages < 20000):
                return i
        except Exception:
            pass
        i = d.find(b'LE', i + 1)
    return None


def le_info(path):
    d = open(path, 'rb').read()
    le = find_le(d)
    if le is None:
        print('no LE header found')
        return
    f = lambda o: struct.unpack_from('<I', d, le + o)[0]
    h = lambda o: struct.unpack_from('<H', d, le + o)[0]
    npages, psz = f(0x14), f(0x28)
    nobj, objtab = f(0x44), f(0x40)
    print(f'file            : {path} ({len(d)} bytes)')
    print(f'LE header offset: {le:#x}')
    print(f'cpu/os          : {h(8)}/{h(10)}  pages={npages} pagesize={psz:#x}')
    print(f'eip             : {f(0x1C):#x}  (object {f(0x18)})')
    print(f'esp             : {f(0x24):#x}  (object {f(0x20)})')
    print(f'object table    : {objtab:#x}  count={nobj}  page map={f(0x48):#x}')
    print('objects:')
    for i in range(nobj):
        o = le + objtab + i * 24
        virt, rel, flags, pageidx, npg, _ = struct.unpack_from('<IIIIII', d, o)
        fl = '|'.join(n for b, n in OBJ_FLAGS if flags & b)
        print(f'  [{i}] virt={virt:#x} base={rel:#x} pages={npg} pageidx={pageidx} flags={flags:#x} ({fl})')
    pg = struct.unpack_from('<I', d, le + 0x80)[0]
    print(f'data pages off  : {pg:#x} (relative to the LE header; DOS/4GW bound)')
    # write the flat image used for analysis: objects at their rel bases
    return le


def index_info(path):
    d = open(path, 'rb').read()
    print(f'{path}: {len(d)} bytes, {len(d)//20} entries')
    print('name            offset?      size      flags')
    for e in range(0, len(d) - 20 + 1, 20):
        name = d[e:e + 12].split(b'\0')[0].decode('latin1')
        a, b = struct.unpack_from('<II', d, e + 12)
        size, flags = (a & 0xFFFFFF, a >> 24) if a >> 24 else (a & 0xFFFFFF, a >> 24)
        print(f'{name:14s} {a:#010x}  {size:8d}  {flags:#x}')


if __name__ == '__main__':
    if sys.argv[1] == '--index':
        index_info(sys.argv[2])
    else:
        le_info(sys.argv[1])
