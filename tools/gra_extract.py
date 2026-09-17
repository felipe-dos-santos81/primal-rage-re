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


def palette_records(body):
    """type-5 body -> list of records, each [(r,g,b), ...], in file order."""
    recs, pos = [], 0
    while pos + 4 <= len(body):
        count = struct.unpack_from('<I', body, pos)[0]
        end = pos + 4 + 4 * count
        if end > len(body):
            raise ValueError("palette record at %#x overruns the body" % pos)
        recs.append(parse_palette(body[pos:end]))
        pos = end
    return recs


def _family(stem):
    """'S16KONSH' -> 'KON': drop the set prefix (S04/S08/S16), keep three letters."""
    if len(stem) > 3 and stem[0] == 'S' and stem[1:3].isdigit():
        stem = stem[3:]
    return stem[:3]


def choose_bank(stem, banks, force_from=None):
    """Spec rule step 1 -> (source_stem or 'greyscale', records or None)."""
    if force_from is not None:
        return force_from, banks[force_from]
    if stem in banks:
        return stem, banks[stem]
    cands = [s for s in banks if s != stem and _family(stem) == _family(s)]
    if cands:
        best = min(cands, key=lambda s: (len(s), s))
        return best, banks[best]
    return 'greyscale', None


def choose_record(records, max_index, force=None):
    """Spec rule step 2 -> (record id | 'flat' | None, colour list).
    Colour lists are indexed 1-based by the caller (to_rgba), except GREY."""
    if records is None:
        return None, GREY
    if force is not None:
        return force, records[force]
    for i, rec in enumerate(records):
        if max_index <= len(rec):
            return i, rec
    return 'flat', [c for rec in records for c in rec]


def to_rgba(rows, colours):
    """rows of (index, opaque) -> (RGBA bytes, count of opaque pixels without a colour).
    Palette records are 1-based (index 1 = record[0]); GREY is the identity."""
    one_based = colours is not GREY
    out = bytearray()
    bad = 0
    for row in rows:
        for idx, opaque in row:
            if not opaque:
                out += b'\0\0\0\0'
                continue
            k = idx - 1 if one_based else idx
            if 0 <= k < len(colours):
                out += bytes(colours[k]) + b'\xff'
            else:
                out += bytes(MAGENTA) + b'\xff'
                bad += 1
    return bytes(out), bad
