"""Walk the chunk chain of a .GRA file.

A GRA file is a linked list of chunks: each starts with
    u16 type; char magic[2] ('43'); u32 next_chunk_offset (0 = last)
and the chunk body follows.

usage: gra_chunks.py FILE.GRA [FILE.GRA ...]
"""
import struct
import sys

for path in sys.argv[1:]:
    d = open(path, 'rb').read()
    print(f'== {path} ({len(d)} bytes)')
    off = 0
    n = 0
    while off < len(d) and n < 32:
        typ, = struct.unpack_from('<H', d, off)
        magic = d[off + 2:off + 4]
        nxt, = struct.unpack_from('<I', d, off + 4)
        if nxt and nxt <= off:
            print(f'   [off {off:#x}] type={typ} magic={magic!r} next={nxt:#x}  <-- BAD')
            break
        end = nxt if nxt else len(d)
        print(f'   [off {off:#x}] type={typ} magic={magic!r} next={nxt:#x} body={end - off - 8} bytes')
        if nxt == 0:
            break
        off = nxt
        n += 1
