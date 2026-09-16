#!/usr/bin/env python3
"""Render a frame from a Primal Rage `.GRA` graphics file to a PPM.

A `.GRA` is an 8-byte-header chunk chain (see FORMATS.md).  This tool is an
*independent* decoder: it shares no code with `port/` and re-derives the layout
from the file bytes so it can act as the oracle for the C decoder.

    gra_render.py FILE.GRA CHUNK_INDEX OUT.ppm [--palette CHUNK] [--frame N]

CHUNK_INDEX selects a chunk in the chain (0 = the first, usually type 2).
Frame selection uses the type-6 descriptor table:

    type 6  = u32 count? no: N x 12-byte records, body_len/12 of them
              record = { u16 width, u16 height, s16 x, s16 y, u32 pixel_handle }
              pixel_handle low 23 bits = file offset of an RLE blob in chunk 2.
    type 5  = a bank of palettes: repeated { u32 count; count x u32 colours },
              colour word: R = bits[2..9], G = bits[10..17], B = bits[18..25].
    type 2  = the RLE pixel blobs, addressed only by the type-6 handles.

Sprite RLE (one control byte, then optional data, per token):
    b & 0x80 == 0            literal run: (b & 0x7f) pixels, each own colour byte
    b & 0x80 != 0, b&0x40==0 repeat run:  (b & 0x3f) pixels, one colour byte
    b & 0x40 != 0            transparent: (b & 0x3f) pixels, no data
Rows are decoded back to back, `height` rows of `width` pixels, no row marker.
Transparent pixels are written as the background colour (black).
"""
import argparse
import struct
import sys


def chunks(d):
    """Walk the chunk chain: returns [(type, body_off, body_end)]."""
    out = []
    off = 0
    while True:
        if off + 8 > len(d):
            raise ValueError("truncated chunk header at %#x" % off)
        typ = struct.unpack_from('<H', d, off)[0]
        if d[off + 2:off + 4] != b'43':
            raise ValueError("bad magic at %#x" % off)
        nxt = struct.unpack_from('<I', d, off + 4)[0]
        if nxt and (nxt <= off + 8 or nxt > len(d)):
            raise ValueError("bad next=%#x at %#x" % (nxt, off))
        end = nxt if nxt else len(d)
        out.append((typ, off + 8, end))
        if not nxt:
            break
        off = nxt
    return out


def parse_palette(body):
    """A bank of { u32 count; count u32 colours }; returns flat [(r,g,b)]."""
    pal, pos = [], 0
    while pos + 4 <= len(body):
        count = struct.unpack_from('<I', body, pos)[0]
        pos += 4
        if count * 4 > len(body) - pos:
            raise ValueError("palette count %d overruns body" % count)
        for _ in range(count):
            w = struct.unpack_from('<I', body, pos)[0]
            pos += 4
            pal.append(((w >> 2) & 0xff, (w >> 10) & 0xff, (w >> 18) & 0xff))
    return pal


def parse_frames(body):
    """type-6 body -> [(width, height, x, y, pixel_file_offset)]."""
    if len(body) % 12:
        raise ValueError("type-6 body %d is not a multiple of 12" % len(body))
    frames = []
    for i in range(0, len(body), 12):
        w, h, x, y, handle = struct.unpack_from('<HHHHI', body, i)
        sx = x - 0x10000 if x & 0x8000 else x
        sy = y - 0x10000 if y & 0x8000 else y
        frames.append((w, h, sx, sy, handle & 0x7fffff))
    return frames


def decode_sprite(d, w, h, pos):
    """RLE-decode one sprite to rows of (index, opaque) pairs."""
    rows = []
    for _ in range(h):
        row = [(0, False)] * w
        x = 0
        while x < w:
            if pos >= len(d):
                raise ValueError("RLE truncated inside a row")
            b = d[pos]
            pos += 1
            if b & 0x80 == 0:
                c = b & 0x7f
                for i in range(c):
                    if x + i < w:
                        row[x + i] = (d[pos + i], True)
                pos += c
                x += c
            elif b & 0x40 == 0:
                c = b & 0x3f
                v = d[pos]
                pos += 1
                for i in range(c):
                    if x + i < w:
                        row[x + i] = (v, True)
                x += c
            else:
                x += b & 0x3f
        rows.append(row)
    return rows, pos


def render(rows, w, h, palette):
    def rgb(idx, opaque):
        if not opaque:
            return (0, 0, 0)
        if idx < len(palette):
            return palette[idx]
        return (255, 0, 255)  # index outside the palette: flag it loudly

    buf = bytearray()
    for row in rows:
        for idx, opaque in row:
            buf += bytes(rgb(idx, opaque))
    return b'P6\n%d %d\n255\n' % (w, h) + bytes(buf)


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('gra')
    ap.add_argument('chunk', type=int)
    ap.add_argument('out')
    ap.add_argument('--palette', type=int, default=None,
                    help='chunk index supplying the palette (default: first type-5)')
    ap.add_argument('--frame', type=int, default=0, help='descriptor index (default 0)')
    args = ap.parse_args(argv)

    d = open(args.gra, 'rb').read()
    ch = chunks(d)
    if not 0 <= args.chunk < len(ch):
        sys.exit("chunk index %d out of range (have %d)" % (args.chunk, len(ch)))
    typ, body_off, body_end = ch[args.chunk]
    body = d[body_off:body_end]

    # palette
    pal_chunk = args.palette
    if pal_chunk is None:
        pal_chunk = next((i for i, (t, _, _) in enumerate(ch) if t == 5), None)
    if pal_chunk is not None:
        palette = parse_palette(d[ch[pal_chunk][1]:ch[pal_chunk][2]])
        if palette:
            print("palette: chunk %d, %d colours" % (pal_chunk, len(palette)),
                  file=sys.stderr)
    else:
        palette = [(i, i, i) for i in range(256)]
        print("palette: none in file, using greyscale", file=sys.stderr)

    if typ == 5:
        # render the palette bank as a swatch strip
        n = len(palette)
        cols, rows = min(n, 256) or 1, (n + 255) // 256 or 1
        data = bytearray()
        for y in range(rows):
            for x in range(cols):
                i = y * 256 + x
                data += bytes(palette[i] if i < n else (0, 0, 0))
        open(args.out, 'wb').write(b'P6\n%d %d\n255\n' % (cols, rows) + bytes(data))
        print("wrote %s (%dx%d palette swatches, type 5)" % (args.out, cols, rows),
              file=sys.stderr)
        return

    # type 2 (pixels) or type 6 (descriptors): find the descriptor table
    frames_body = next((d[o:e] for t, o, e in ch if t == 6), None)
    if frames_body is None:
        sys.exit("no type-6 descriptor table: cannot locate sprites")
    frames = parse_frames(frames_body)
    if not 0 <= args.frame < len(frames):
        sys.exit("frame %d out of range (have %d)" % (args.frame, len(frames)))
    w, h, x, y, off = frames[args.frame]
    if not (0 <= off < len(d)):
        sys.exit("frame %d pixel offset %#x out of range" % (args.frame, off))
    print("frame %d: %dx%d origin (%d,%d) pixels @ %#x" % (args.frame, w, h, x, y, off),
          file=sys.stderr)
    rows, used = decode_sprite(d, w, h, off)
    open(args.out, 'wb').write(render(rows, w, h, palette))
    print("wrote %s (%dx%d, %d RLE bytes from %#x..%#x)"
          % (args.out, w, h, used - off, off, used), file=sys.stderr)


if __name__ == '__main__':
    main(sys.argv[1:])
