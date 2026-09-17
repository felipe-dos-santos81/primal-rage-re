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


from gra_extract import (palette_records, choose_bank, choose_record,  # noqa: E402
                         to_rgba, GREY, MAGENTA)


class PaletteBankTests(unittest.TestCase):
    def body5(self, d):
        from gra_render import chunks
        return next(d[o:e] for t, o, e in chunks(d) if t == 5)

    def test_splits_records_and_decodes_words(self):
        recs = palette_records(self.body5(fixture()))
        self.assertEqual([len(r) for r in recs], [3, 8])
        self.assertEqual(recs[0][0], (10, 20, 30))
        self.assertEqual(recs[1][7], (8, 16, 24))

    def test_rejects_overrunning_count(self):
        with self.assertRaises(ValueError):
            palette_records(struct.pack('<I', 5) + b'\0' * 8)


class ChooseBankTests(unittest.TestCase):
    BANKS = {'KON': [[(1, 1, 1)]], 'COB': [[(2, 2, 2)]], 'COBFT': [[(3, 3, 3)]],
             'ESTIL': [[(4, 4, 4)]]}

    def test_own_bank_wins(self):
        self.assertEqual(choose_bank('KON', self.BANKS), ('KON', self.BANKS['KON']))

    def test_borrows_shortest_three_letter_match(self):
        self.assertEqual(choose_bank('KONSH', self.BANKS)[0], 'KON')
        self.assertEqual(choose_bank('COBSH', self.BANKS)[0], 'COB')
        self.assertEqual(choose_bank('ESTKO', self.BANKS)[0], 'ESTIL')

    def test_set_prefix_is_ignored(self):
        banks = {'S16KON': [[(1, 1, 1)]], 'S16COB': [[(2, 2, 2)]]}
        self.assertEqual(choose_bank('S16KONSH', banks)[0], 'S16KON')
        self.assertEqual(choose_bank('S16TALSH', banks), ('greyscale', None))

    def test_no_match_is_greyscale(self):
        self.assertEqual(choose_bank('CAGE', self.BANKS), ('greyscale', None))

    def test_force_from_overrides(self):
        self.assertEqual(choose_bank('KON', self.BANKS, force_from='COB'), ('COB', self.BANKS['COB']))


class ChooseRecordTests(unittest.TestCase):
    def test_first_record_that_fits_one_based(self):
        rid, cols = choose_record(RECORDS, 3)
        self.assertEqual((rid, cols), (0, RECORDS[0]))
        rid, cols = choose_record(RECORDS, 4)
        self.assertEqual((rid, cols), (1, RECORDS[1]))

    def test_flat_when_nothing_fits(self):
        rid, cols = choose_record(RECORDS, 9)
        self.assertEqual(rid, 'flat')
        self.assertEqual(cols, RECORDS[0] + RECORDS[1])

    def test_greyscale_when_no_bank(self):
        self.assertEqual(choose_record(None, 200), (None, GREY))

    def test_force_overrides(self):
        self.assertEqual(choose_record(RECORDS, 1, force=1), (1, RECORDS[1]))


class ToRgbaTests(unittest.TestCase):
    def test_transparent_opaque_and_flagged(self):
        rows = [[(1, True), (0, False), (0, True), (9, True)]]
        data, bad = to_rgba(rows, RECORDS[1])
        self.assertEqual(bad, 2)
        self.assertEqual(data[0:4], bytes((1, 2, 3, 255)))       # index 1 -> record[0]
        self.assertEqual(data[4:8], bytes((0, 0, 0, 0)))          # transparent
        self.assertEqual(data[8:12], bytes(MAGENTA) + b'\xff')   # opaque index 0
        self.assertEqual(data[12:16], bytes(MAGENTA) + b'\xff')  # past the record

    def test_greyscale_is_zero_based(self):
        data, bad = to_rgba([[(0, True), (255, True)]], GREY)
        self.assertEqual(bad, 0)
        self.assertEqual(data, bytes((0, 0, 0, 255, 255, 255, 255, 255)))


if __name__ == '__main__':
    unittest.main()


import json  # noqa: E402
import tempfile  # noqa: E402
from PIL import Image  # noqa: E402
from gra_extract import load_index, extract_file, main  # noqa: E402


def write_fixture_dir(tmp, files):
    """files: {'S16FOO.GRA': bytes}. Also writes an INDEX naming them in order."""
    for name, data in files.items():
        with open(os.path.join(tmp, name), 'wb') as f:
            f.write(data)
    with open(os.path.join(tmp, 'INDEX'), 'wb') as f:
        for name in files:
            f.write(struct.pack('<12sII', name.lower().encode(), len(files[name]) | 0x01000000, 0))


class Args:
    palette_record = None
    palette_from = None


def broken_fixture():
    """One RLE descriptor whose pixel offset lies past the end of the file."""
    from gra_render import chunks
    d = bytearray(build_gra([SPRITE_A], RECORDS, [(4, 3, 0, 0, 0)]))
    _, o, _ = next(c for c in chunks(bytes(d)) if c[0] == 6)
    struct.pack_into('<I', d, o + 8, 0x7FFFFF)
    return bytes(d)


class ExtractFileTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.dir = self.tmp.name
        write_fixture_dir(self.dir, {'S16FOO.GRA': fixture()})
        self.out = os.path.join(self.dir, 'out')

    def tearDown(self):
        self.tmp.cleanup()

    def run_one(self, banks=None, args=Args()):
        path = os.path.join(self.dir, 'S16FOO.GRA')
        if banks is None:
            from gra_render import chunks
            d = open(path, 'rb').read()
            banks = {'S16FOO': palette_records(next(d[o:e] for t, o, e in chunks(d) if t == 5))}
        return extract_file(path, self.out, banks, load_index(os.path.join(self.dir, 'INDEX')), args)

    def test_manifest_entry(self):
        e = self.run_one()
        self.assertEqual(e['gra'], 'S16FOO.GRA')
        self.assertEqual(e['resource_index'], 0)
        self.assertEqual(e['palette_source'], 'S16FOO')
        self.assertEqual(e['palette_records'], [3, 8])
        self.assertFalse(e['skipped'])
        a, b, c = e['sprites']
        self.assertEqual(a['kind'], 'rle')
        self.assertEqual((a['width'], a['height'], a['x'], a['y']), (4, 3, 1, 2))
        self.assertEqual(a['pixel_offset'], 8)
        self.assertEqual(a['handle'], '0x00000008')
        self.assertEqual(a['palette_record'], 1)
        self.assertEqual(a['max_index'], 7)
        self.assertEqual(a['rle_bytes'], len(SPRITE_A))
        self.assertEqual(a['out_of_palette'], 1)
        self.assertEqual(a['png'], 'S16FOO/0000.png')
        self.assertEqual(b['kind'], 'raw')
        self.assertEqual(b['palette_record'], 0)
        self.assertNotIn('rle_bytes', b)
        self.assertNotIn('out_of_palette', b)
        self.assertEqual(c['kind'], 'empty')
        self.assertNotIn('png', c)

    def test_png_pixels(self):
        self.run_one()
        im = Image.open(os.path.join(self.out, 'S16FOO', '0000.png'))
        self.assertEqual((im.mode, im.size), ('RGBA', (4, 3)))
        px = im.load()
        self.assertEqual(px[0, 0], (5, 10, 15, 255))       # index 5 -> record1[4]
        self.assertEqual(px[1, 0], (255, 0, 255, 255))     # opaque index 0, flagged
        self.assertEqual(px[2, 0], (0, 0, 0, 0))           # transparent run
        self.assertEqual(px[3, 0], (0, 0, 0, 0))
        self.assertEqual(px[0, 1], (2, 4, 6, 255))         # repeat colour 2
        self.assertEqual(px[1, 2], (0, 0, 0, 0))           # transparent 1
        self.assertEqual(px[3, 2], (1, 2, 3, 255))         # repeat colour 1
        raw = Image.open(os.path.join(self.out, 'S16FOO', '0001.png'))
        self.assertEqual(raw.size, (4, 3))
        self.assertEqual(raw.load()[2, 0], (70, 80, 90, 255))   # index 3 -> record0[2]
        self.assertFalse(os.path.exists(os.path.join(self.out, 'S16FOO', '0002.png')))

    def test_greyscale_when_no_bank_anywhere(self):
        e = self.run_one(banks={})
        self.assertEqual(e['palette_source'], 'greyscale')
        self.assertIsNone(e['palette_records'])
        self.assertIsNone(e['sprites'][0]['palette_record'])
        im = Image.open(os.path.join(self.out, 'S16FOO', '0001.png'))
        self.assertEqual(im.load()[2, 0], (3, 3, 3, 255))

    def test_forced_record(self):
        class A(Args):
            palette_record = 0
        e = self.run_one(args=A())
        self.assertEqual(e['sprites'][0]['palette_record'], 0)
        self.assertEqual(e['sprites'][0]['out_of_palette'], 3)   # 5, 0 and 7 miss a 3-colour record

    def test_sprite_error_is_recorded_not_raised(self):
        with open(os.path.join(self.dir, 'S16FOO.GRA'), 'wb') as f:
            f.write(broken_fixture())
        e = self.run_one(banks={})
        self.assertIn('error', e['sprites'][0])
        self.assertNotIn('png', e['sprites'][0])

    def test_file_without_descriptors_is_skipped(self):
        with open(os.path.join(self.dir, 'S16FOO.GRA'), 'wb') as f:
            f.write(chunk(2, b'\1\2\3', 0))
        e = self.run_one(banks={})
        self.assertTrue(e['skipped'])
        self.assertIn('reason', e)
        self.assertEqual(e['sprites'], [])


class MainTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.dir = self.tmp.name
        # KON has a bank; KONSH has none and must borrow it; SND has no descriptors.
        write_fixture_dir(self.dir, {
            'S16KON.GRA': fixture(),
            'S16KONSH.GRA': build_gra([SPRITE_A], None, [(4, 3, 0, 0, 0)]),
            'S16SND.GRA': chunk(2, b'\1\2\3', 0),
        })
        self.out = os.path.join(self.dir, 'out')

    def tearDown(self):
        self.tmp.cleanup()

    def manifest(self):
        return json.load(open(os.path.join(self.out, 'manifest.json')))

    def test_end_to_end(self):
        rc = main([self.dir, self.out])
        self.assertEqual(rc, 0)
        m = self.manifest()
        self.assertEqual(m['source_dir'], self.dir)
        self.assertEqual(sorted(m['files']), ['S16KON', 'S16KONSH', 'S16SND'])
        self.assertEqual(m['files']['S16KONSH']['palette_source'], 'S16KON')
        self.assertEqual(m['files']['S16KONSH']['resource_index'], 1)
        self.assertTrue(m['files']['S16SND']['skipped'])
        self.assertTrue(os.path.exists(os.path.join(self.out, 'S16KONSH', '0000.png')))

    def test_only_filter(self):
        main([self.dir, self.out, '--only', 'S16KONSH*'])
        self.assertEqual(list(self.manifest()['files']), ['S16KONSH'])

    def test_palette_from(self):
        main([self.dir, self.out, '--only', 'S16KONSH*', '--palette-from', 'S16KON.GRA'])
        self.assertEqual(self.manifest()['files']['S16KONSH']['palette_source'], 'S16KON')

    def test_error_sets_exit_status(self):
        with open(os.path.join(self.dir, 'S16KON.GRA'), 'wb') as f:
            f.write(broken_fixture())
        self.assertEqual(main([self.dir, self.out]), 1)
