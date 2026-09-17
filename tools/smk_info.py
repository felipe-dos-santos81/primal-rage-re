#!/usr/bin/env python3
"""Smacker container inventory: proves the layout and enumerates the fixed
profile (tree presence, block types, keyframes). Read-only."""
import struct, sys

SMK_PAL = bytes([
    0x00,0x04,0x08,0x0C,0x10,0x14,0x18,0x1C,0x20,0x24,0x28,0x2C,0x30,0x34,0x38,0x3C,
    0x41,0x45,0x49,0x4D,0x51,0x55,0x59,0x5D,0x61,0x65,0x69,0x6D,0x71,0x75,0x79,0x7D,
    0x82,0x86,0x8A,0x8E,0x92,0x96,0x9A,0x9E,0xA2,0xA6,0xAA,0xAE,0xB2,0xB6,0xBA,0xBE,
    0xC3,0xC7,0xCB,0xCF,0xD3,0xD7,0xDB,0xDF,0xE3,0xE7,0xEB,0xEF,0xF3,0xF7,0xFB,0xFF])

def u32(b, o): return struct.unpack_from('<I', b, o)[0]

def parse(path):
    b = open(path, 'rb').read()
    magic = b[:4]
    w, h, frames, pts, flags = struct.unpack_from('<IIIII', b, 4)
    assert magic in (b'SMK2', b'SMK4'), magic
    treesize = u32(b, 0x34)
    trees = struct.unpack_from('<4I', b, 0x38)          # mmap, mclr, full, type
    audio = []
    for i in range(7):
        v = u32(b, 0x48 + 4*i)                          # u24 rate + u8 flag, LE
        audio.append((v & 0xFFFFFF, v >> 24))
    tbl = 0x68
    sizes = [u32(b, tbl + 4*i) for i in range(frames)]
    foff = tbl + 4*frames
    fflags = b[foff:foff+frames]
    trees_off = foff + frames
    data = trees_off + treesize
    total = sum(s & ~3 for s in sizes)
    assert data + total == len(b), (data + total, len(b))
    return dict(path=path, w=w, h=h, frames=frames, pts=pts, flags=flags,
                treesize=treesize, trees=trees, audio=audio, sizes=sizes,
                fflags=fflags, tbl=tbl, trees_off=trees_off, data=data)

def main():
    for path in sys.argv[1:]:
        m = parse(path)
        key = [i for i, s in enumerate(m['sizes']) if s & 1]
        pal = [i for i, f in enumerate(m['fflags']) if f & 1]
        aud = [i for i, f in enumerate(m['fflags']) if f & 0xFE]
        print(f"{path}: {m['w']}x{m['h']} frames={m['frames']} pts_inc={m['pts']}")
        print(f"  treesize={m['treesize']} tree_sizes(mmap,mclr,full,type)={m['trees']}")
        print(f"  audio_descriptors={m['audio']}  audio_frame_flags={aud}")
        print(f"  keyframes(frame_size bit0)={key}")
        print(f"  palette_change_frames={len(pal)} first={pal[:8]}")
        print(f"  layout: tbl=0x{m['tbl']:x} trees=0x{m['trees_off']:x} data=0x{m['data']:x} end={m['data']+sum(s & ~3 for s in m['sizes'])}")

if __name__ == '__main__':
    main()
