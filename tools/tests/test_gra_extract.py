"""Tests for tools/gra_extract.py.

The fixture is a synthetic GRA built in memory with the exact chunk layout
FORMATS.md documents: chunk 2 (pixel blobs) -> chunk 5 (palette bank,
optional) -> chunk 6 (12-byte descriptors).  Oracle tests at the bottom need
data/game/C and are skipped (or, under PR_ORACLE_REQUIRED, fail) without it.
"""
import os
import struct
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS = os.path.dirname(HERE)
sys.path.insert(0, TOOLS)

import gra_extract  # noqa: E402
from gra_extract import Desc, read_descriptors, decode_raw  # noqa: E402


def colour_word(r, g, b):
    """Pack an RGB triple the way chunk 5 stores it (R bits 2..9, G 10..17, B 18..25)."""
    return (r << 2) | (g << 10) | (b << 18)


def chunk(typ, body, nxt):
    return struct.pack('<H2sI', typ, b'43', nxt) + body


def build_gra(blobs, records, descs):
    """blobs: list of bytes (pixel blobs, laid out in order in chunk 2).
    records: list of [(r,g,b), ...] palette records, or None for no chunk 5.
    descs: list of (w, h, x, y, blob_index_or_None); w/h are the raw u16 fields.
    Returns the file bytes."""
    body2 = b''.join(blobs)
    offs, pos = [], 8
    for b in blobs:
        offs.append(pos)
        pos += len(b)
    end2 = 8 + len(body2)
    if records is not None:
        body5 = b''.join(struct.pack('<I', len(rec)) +
                         b''.join(struct.pack('<I', colour_word(*c)) for c in rec)
                         for rec in records)
        end5 = end2 + 8 + len(body5)
    else:
        body5, end5 = None, end2
    body6 = b''.join(struct.pack('<HHhhI', w, h, x, y, offs[bi] if bi is not None else 0)
                     for (w, h, x, y, bi) in descs)
    out = chunk(2, body2, end2)
    if body5 is not None:
        out += chunk(5, body5, end5)
    out += chunk(6, body6, 0)
    return out


# Sprite A: 4x3 RLE using all three run types; max opaque index 7; one opaque index 0.
#   row 0: literal 2 (5, 0), transparent 2
#   row 1: repeat 4 of colour 2
#   row 2: literal 1 (7), transparent 1, repeat 2 of colour 1
SPRITE_A = bytes([0x02, 0x05, 0x00, 0xC2,
                  0x84, 0x02,
                  0x01, 0x07, 0xC1, 0x82, 0x01])
# Raw B: 4x3 uncompressed, indices 1..3.
RAW_B = bytes([1, 2, 3, 1, 1, 2, 3, 1, 1, 2, 3, 1])
RECORDS = [[(10, 20, 30), (40, 50, 60), (70, 80, 90)],                  # 3 colours
           [(i, i * 2, i * 3) for i in range(1, 9)]]                    # 8 colours
DESCS = [(4, 3, 1, 2, 0),                    # A: rle
         (0xFFFC, 0xFFFD, 0, 0, 1),          # B: raw (-4 x -3)
         (0, 0, 30, 27, None)]               # C: empty


def fixture():
    return build_gra([SPRITE_A, RAW_B], RECORDS, DESCS)


class DescriptorTests(unittest.TestCase):
    def body6(self, d):
        from gra_render import chunks
        return next(d[o:e] for t, o, e in chunks(d) if t == 6)

    def test_classifies_rle_raw_empty(self):
        descs = read_descriptors(self.body6(fixture()))
        self.assertEqual([x.kind for x in descs], ['rle', 'raw', 'empty'])
        self.assertEqual(descs[0], Desc(0, 'rle', 4, 3, 1, 2, 8, 8))
        self.assertEqual((descs[1].width, descs[1].height), (4, 3))
        self.assertEqual(descs[1].offset, 8 + len(SPRITE_A))
        self.assertEqual((descs[2].width, descs[2].height, descs[2].x, descs[2].y), (0, 0, 30, 27))

    def test_handle_keeps_resource_bits(self):
        d = build_gra([SPRITE_A], RECORDS, [(4, 3, 0, 0, 0)])
        body = bytearray(self.body6(d))
        struct.pack_into('<I', body, 8, (49 << 23) | 8)
        desc = read_descriptors(bytes(body))[0]
        self.assertEqual(desc.handle, (49 << 23) | 8)
        self.assertEqual(desc.offset, 8)

    def test_rejects_bad_table_length(self):
        with self.assertRaises(ValueError):
            read_descriptors(b'\0' * 13)


class RawDecodeTests(unittest.TestCase):
    def test_decodes_rows_all_opaque(self):
        rows = decode_raw(RAW_B, 4, 3, 0)
        self.assertEqual(len(rows), 3)
        self.assertEqual(rows[0], [(1, True), (2, True), (3, True), (1, True)])
        self.assertEqual(rows[2], [(1, True), (2, True), (3, True), (1, True)])

    def test_rejects_truncated_blob(self):
        with self.assertRaises(ValueError):
            decode_raw(RAW_B, 4, 4, 0)


if __name__ == '__main__':
    unittest.main()
