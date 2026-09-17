#!/usr/bin/env python3
"""Extract every sprite of a Primal Rage `.GRA` set to RGBA PNGs + a manifest.

    gra_extract.py GAME_DIR [OUT_DIR] [--only GLOB] [--palette-record N]
                   [--palette-from FILE.GRA]

Reuses the verified parsers in gra_render.py (chunk chain, type-6 descriptor
table, type-5 palette bank, sprite RLE) and adds:

  * descriptor classification: positive dims = RLE sprite, negative dims =
    raw uncompressed |w| x |h| bitmap, zero dims = empty (no image);
  * the palette rule (design spec section 5): the file's own bank, else the
    shortest-named file with a bank sharing the first three letters of the
    stem, else greyscale; the first record with max_index <= count, else the
    bank flattened; colour = record[index - 1] (indices are 1-based);
  * RGBA PNG per descriptor (transparent runs alpha 0) and manifest.json.

Output: OUT_DIR/<STEM>/<NNNN>.png and OUT_DIR/manifest.json, keyed by GRA
stem and descriptor index.
"""
import os
import struct
import sys
from collections import namedtuple

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gra_render import chunks, parse_frames, parse_palette, decode_sprite  # noqa: E402

Desc = namedtuple('Desc', 'index kind width height x y offset handle')
GREY = [(i, i, i) for i in range(256)]
MAGENTA = (255, 0, 255)


def _signed16(v):
    return v - 0x10000 if v & 0x8000 else v


def read_descriptors(body):
    """type-6 body -> [Desc]; dims normalised, kind classified, handle kept whole."""
    if len(body) % 12:
        raise ValueError("type-6 body %d is not a multiple of 12" % len(body))
    frames = parse_frames(body)
    out = []
    for i, (w_u, h_u, x, y, off) in enumerate(frames):
        handle = struct.unpack_from('<I', body, i * 12 + 8)[0]
        w, h = _signed16(w_u), _signed16(h_u)
        if w == 0 or h == 0:
            kind, w, h = 'empty', 0, 0
        elif w < 0 or h < 0:
            kind, w, h = 'raw', abs(w), abs(h)
        else:
            kind = 'rle'
        out.append(Desc(i, kind, w, h, x, y, off, handle))
    return out


def decode_raw(d, w, h, off):
    """Raw blob: w*h index bytes, row-major, all opaque -> rows of (index, True)."""
    if off + w * h > len(d):
        raise ValueError("raw blob %dx%d at %#x overruns the file" % (w, h, off))
    return [[(d[off + r * w + c], True) for c in range(w)] for r in range(h)]
