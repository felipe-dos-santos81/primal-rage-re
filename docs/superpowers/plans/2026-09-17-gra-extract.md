# GRA sprite extractor (`tools/gra_extract.py`) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A standalone Python tool that turns every `S16*.GRA` sprite into an RGBA PNG plus one `manifest.json`, keyed by GRA stem and descriptor index.

**Architecture:** `tools/gra_extract.py` reuses the verified parsers in `tools/gra_render.py` (`chunks`, `parse_frames`, `parse_palette`, `decode_sprite`) and adds descriptor classification (RLE / raw / empty), the per-sprite palette rule (bank → record → 1-based colour), Pillow PNG output and the manifest. Nothing under `port/` changes. Tests are `unittest` modules under `tools/tests/`, driven by a synthetic in-memory GRA fixture plus two oracle tests that need the shipped assets.

**Tech Stack:** Python 3.12, Pillow 12 (installed), `unittest`, Make.

**Spec:** `docs/superpowers/specs/2026-09-17-gra-extract-design.md`

## Global Constraints

- Repo: `/Users/felipe.dos.santos/code/mine/primal-rage-reverse`. Tools under `tools/`, tests under `tools/tests/`. `data/` is read-only and untracked.
- **Decoupled from the game:** `tools/gra_extract.py` imports nothing from `port/`; it imports `gra_render` from its own directory.
- **One decoder:** chunk walking, descriptor parsing, palette word decoding and RLE decoding come from `gra_render.py` unmodified. Do not re-implement them.
- **Dependencies:** Python 3.12 and Pillow only. No `pyproject.toml`, no `requirements.txt`, no numpy.
- **Palette rule (spec §5), verbatim:** bank = own type-5 bank, else the shortest-named file with a bank sharing the stem's first three letters, else greyscale; record = first record with `max_index <= count`, else `"flat"`; colour = `record[index - 1]`; index 0 opaque or index past the record → magenta `(255, 0, 255)` and counted as `out_of_palette`.
- **Output (spec §4):** `OUT_DIR/<STEM>/<NNNN>.png` (RGBA, 4-digit zero-padded index), `OUT_DIR/manifest.json`. `OUT_DIR` defaults to `extracted/` and is git-ignored.
- **Tests run as** `python3 -m unittest tools.tests.test_gra_extract -v` from the repo root (namespace packages: no `__init__.py` files). Oracle tests skip when `data/game/C/S16TITLE.GRA` is absent and **fail** instead when `PR_ORACLE_REQUIRED` is set.
- **Style:** match `tools/gra_render.py` (argparse, docstring header, stderr for progress). Every task ends with the unit tests green and a commit.

---

## File structure

| File | Responsibility |
|---|---|
| `tools/gra_extract.py` (create) | the tool: descriptor classification, raw decode, palette rule, RGBA conversion, per-file extraction, manifest, CLI |
| `tools/tests/test_gra_extract.py` (create) | `build_gra` fixture builder + unit tests + gated oracle tests |
| `Makefile` (modify) | `re-extract`, `re-extract-test` targets |
| `.gitignore` (modify) | `extracted/` |
| `README.md` (modify) | one table row for the tool, Pillow in the toolchain list |
| `FORMATS.md` (modify) | the 1-based sub-palette index finding, marked `likely` |

Public names in `tools/gra_extract.py`, fixed across tasks:

```python
Desc = namedtuple('Desc', 'index kind width height x y offset handle')   # kind: 'rle' | 'raw' | 'empty'
GREY = [(i, i, i) for i in range(256)]
MAGENTA = (255, 0, 255)
def read_descriptors(body: bytes) -> list[Desc]
def decode_raw(d: bytes, w: int, h: int, off: int) -> list[list[tuple[int, bool]]]
def palette_records(body: bytes) -> list[list[tuple[int, int, int]]]
def choose_bank(stem: str, banks: dict, force_from: str | None = None) -> tuple[str, list | None]
def choose_record(records: list | None, max_index: int, force: int | None = None) -> tuple[int | str | None, list]
def to_rgba(rows, colours) -> tuple[bytes, int]          # (rgba bytes, out_of_palette count)
def load_index(path: str) -> dict[str, int]              # upper-case stem -> resource index
def extract_file(path: str, out_dir: str, banks: dict, index_map: dict, args) -> dict
def main(argv) -> int
```

---

### Task 1: Descriptor classification and raw decode, with the fixture builder

**Files:**
- Create: `tools/gra_extract.py`
- Create: `tools/tests/test_gra_extract.py`

**Interfaces:**
- Consumes: `gra_render.chunks(d)`, `gra_render.parse_frames(body)`, `gra_render.decode_sprite(d, w, h, pos)`.
- Produces: `Desc`, `read_descriptors(body)`, `decode_raw(d, w, h, off)`; the test helper `build_gra(blobs, records, descs)` used by every later test.

- [ ] **Step 1: Write the fixture builder and the failing tests**

`tools/tests/test_gra_extract.py`:

```python
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
RAW_B = bytes([1, 2, 3, 1, 2, 3, 1, 2, 3, 1, 2, 3])
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
```

- [ ] **Step 2: Run the tests to verify they fail**

Run: `python3 -m unittest tools.tests.test_gra_extract -v`
Expected: `ModuleNotFoundError: No module named 'gra_extract'`

- [ ] **Step 3: Write the minimal implementation**

`tools/gra_extract.py`:

```python
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
```

- [ ] **Step 4: Run the tests to verify they pass**

Run: `python3 -m unittest tools.tests.test_gra_extract -v`
Expected: 5 tests, `OK`

- [ ] **Step 5: Commit**

```bash
git add tools/gra_extract.py tools/tests/test_gra_extract.py
git commit -m "tools: gra_extract — descriptor classification, raw decode, fixture builder

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 2: Palette rule and RGBA conversion

**Files:**
- Modify: `tools/gra_extract.py`
- Modify: `tools/tests/test_gra_extract.py`

**Interfaces:**
- Consumes: `gra_render.parse_palette(body)` (one record's bytes → `[(r,g,b)]`), `GREY`, `MAGENTA`.
- Produces: `palette_records(body)`, `choose_bank(stem, banks, force_from=None)`, `choose_record(records, max_index, force=None)`, `to_rgba(rows, colours)`.

- [ ] **Step 1: Write the failing tests**

Append to `tools/tests/test_gra_extract.py` (above `if __name__ == '__main__':`):

```python
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
```

- [ ] **Step 2: Run the tests to verify they fail**

Run: `python3 -m unittest tools.tests.test_gra_extract -v`
Expected: `ImportError: cannot import name 'palette_records'`

- [ ] **Step 3: Write the minimal implementation**

Append to `tools/gra_extract.py`:

```python
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
```

- [ ] **Step 4: Run the tests to verify they pass**

Run: `python3 -m unittest tools.tests.test_gra_extract -v`
Expected: 18 tests, `OK`

- [ ] **Step 5: Commit**

```bash
git add tools/gra_extract.py tools/tests/test_gra_extract.py
git commit -m "tools: gra_extract — palette bank/record rule and RGBA conversion

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 3: Per-file extraction, manifest, CLI, Make target

**Files:**
- Modify: `tools/gra_extract.py`
- Modify: `tools/tests/test_gra_extract.py`
- Modify: `Makefile` (the `.PHONY` line and the `re-render` block near line 118)
- Modify: `.gitignore`
- Modify: `README.md` (the `tools/gra_render.py` row in the Layout table, and the Toolchain list)

**Interfaces:**
- Consumes: everything from Tasks 1–2; `PIL.Image.frombytes('RGBA', (w, h), data)`.
- Produces: `load_index(path)`, `extract_file(path, out_dir, banks, index_map, args)`, `main(argv)`; the manifest schema of spec §4.

- [ ] **Step 1: Write the failing tests**

Append to `tools/tests/test_gra_extract.py`:

```python
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
```

- [ ] **Step 2: Run the tests to verify they fail**

Run: `python3 -m unittest tools.tests.test_gra_extract -v`
Expected: `ImportError: cannot import name 'load_index'`

- [ ] **Step 3: Write the implementation**

Append to `tools/gra_extract.py`:

```python
import argparse  # noqa: E402
import fnmatch  # noqa: E402
import glob  # noqa: E402
import json  # noqa: E402

from PIL import Image  # noqa: E402


def load_index(path):
    """INDEX (20-byte records) -> {'S16KON': 49, ...}; {} when the file is absent."""
    if not os.path.isfile(path):
        return {}
    d = open(path, 'rb').read()
    out = {}
    for i in range(0, len(d) - len(d) % 20, 20):
        name = d[i:i + 12].split(b'\0')[0].decode('ascii', 'replace')
        stem = os.path.splitext(name)[0].upper()
        out[stem] = i // 20
    return out


def _stem(path):
    return os.path.splitext(os.path.basename(path))[0].upper()


def _bank_of(d):
    """The file's palette records, or None if it has no type-5 chunk."""
    for t, o, e in chunks(d):
        if t == 5:
            return palette_records(d[o:e])
    return None


def extract_file(path, out_dir, banks, index_map, args):
    """One GRA -> PNGs under out_dir/<STEM>/ and the manifest entry for it."""
    stem = _stem(path)
    d = open(path, 'rb').read()
    entry = {'gra': os.path.basename(path),
             'resource_index': index_map.get(stem),
             'palette_source': None, 'palette_records': None,
             'skipped': False, 'sprites': []}
    try:
        ch = chunks(d)
    except ValueError as e:
        entry.update(skipped=True, reason='bad chunk chain: %s' % e)
        return entry
    body6 = next((d[o:e] for t, o, e in ch if t == 6), None)
    if body6 is None:
        entry.update(skipped=True, reason='no type-6 descriptor table (not graphics)')
        return entry
    force_from = _stem(args.palette_from) if args.palette_from else None
    source, records = choose_bank(stem, banks, force_from)
    entry['palette_source'] = source
    entry['palette_records'] = [len(r) for r in records] if records is not None else None

    sub = os.path.join(out_dir, stem)
    os.makedirs(sub, exist_ok=True)
    for desc in read_descriptors(body6):
        s = {'index': desc.index, 'kind': desc.kind,
             'width': desc.width, 'height': desc.height, 'x': desc.x, 'y': desc.y,
             'pixel_offset': desc.offset, 'handle': '0x%08x' % desc.handle}
        entry['sprites'].append(s)
        if desc.kind == 'empty':
            continue
        try:
            if desc.kind == 'raw':
                rows = decode_raw(d, desc.width, desc.height, desc.offset)
            else:
                if not 0 <= desc.offset < len(d):
                    raise ValueError("pixel offset %#x out of range" % desc.offset)
                rows, used = decode_sprite(d, desc.width, desc.height, desc.offset)
                s['rle_bytes'] = used - desc.offset
        except (ValueError, IndexError) as e:
            s['error'] = str(e)
            continue
        max_index = max((v for row in rows for v, o in row if o), default=0)
        rid, colours = choose_record(records, max_index, args.palette_record)
        data, bad = to_rgba(rows, colours)
        png = '%s/%04d.png' % (stem, desc.index)
        Image.frombytes('RGBA', (desc.width, desc.height), data).save(os.path.join(out_dir, png))
        s.update(png=png, palette_record=rid, max_index=max_index)
        if bad:
            s['out_of_palette'] = bad
    return entry


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('game_dir')
    ap.add_argument('out_dir', nargs='?', default='extracted')
    ap.add_argument('--only', default=None, help='glob on the GRA file name, e.g. S16KON*')
    ap.add_argument('--palette-record', type=int, default=None,
                    help='force this sub-palette record for every sprite of every file with a bank')
    ap.add_argument('--palette-from', default=None,
                    help='force the palette bank of this GRA (name or path) for every file')
    args = ap.parse_args(argv)

    files = sorted(glob.glob(os.path.join(args.game_dir, '*.GRA')) +
                   glob.glob(os.path.join(args.game_dir, '*.gra')))
    if not files:
        print("no .GRA files in %s" % args.game_dir, file=sys.stderr)
        return 1
    banks = {}
    for f in files:
        try:
            recs = _bank_of(open(f, 'rb').read())
        except ValueError:
            recs = None
        if recs is not None:
            banks[_stem(f)] = recs
    if args.palette_from and _stem(args.palette_from) not in banks:
        print("--palette-from %s: no palette bank in that file" % args.palette_from, file=sys.stderr)
        return 1
    index_map = load_index(os.path.join(args.game_dir, 'INDEX'))
    if args.only:
        files = [f for f in files if fnmatch.fnmatch(os.path.basename(f), args.only)]

    os.makedirs(args.out_dir, exist_ok=True)
    manifest = {'source_dir': args.game_dir, 'files': {}}
    written = {'rle': 0, 'raw': 0}
    empty = skipped = errors = 0
    for f in files:
        print("extracting %s ..." % os.path.basename(f), file=sys.stderr)
        entry = extract_file(f, args.out_dir, banks, index_map, args)
        manifest['files'][_stem(f)] = entry
        if entry['skipped']:
            skipped += 1
            continue
        for s in entry['sprites']:
            if 'error' in s:
                errors += 1
            elif s['kind'] == 'empty':
                empty += 1
            else:
                written[s['kind']] += 1
    with open(os.path.join(args.out_dir, 'manifest.json'), 'w') as fp:
        json.dump(manifest, fp, indent=1)
    print("wrote %d PNGs (%d rle, %d raw), %d empty descriptors, %d files skipped, %d errors -> %s"
          % (written['rle'] + written['raw'], written['rle'], written['raw'],
             empty, skipped, errors, args.out_dir), file=sys.stderr)
    return 1 if errors else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
```

- [ ] **Step 4: Run the tests to verify they pass**

Run: `python3 -m unittest tools.tests.test_gra_extract -v`
Expected: 28 tests, `OK`

- [ ] **Step 5: Wire the Makefile, .gitignore and README**

`.gitignore`: add a line `extracted/` after `*.dro`.

`Makefile`: add `re-extract re-extract-test` to the `.PHONY` line (the `re-info re-gra re-render re-symbols re-cluster \` line), and add after the `re-render` target:

```make
re-extract: ## Extract every S16 sprite to extracted/ (RGBA PNG per descriptor + manifest.json)
	$(PYTHON) tools/gra_extract.py $(GAME_DIR) extracted

re-extract-test: ## Unit + oracle tests for the extractor (oracle needs data/game/C)
	PR_GAME_DIR=$(GAME_DIR) $(PYTHON) -m unittest tools.tests.test_gra_extract -v
```

`README.md`: after the `tools/gra_render.py` row add

```
| `tools/gra_extract.py` | Extract every S16 sprite to RGBA PNG + `manifest.json` (`make re-extract`) |
```

and change the toolchain bullet `* Python 3 with \`capstone\` for ad-hoc disassembly checks.` to
`* Python 3 with \`capstone\` for ad-hoc disassembly checks and \`Pillow\` for \`tools/gra_extract.py\`.`

Run: `make re-extract-test` — expected 28 tests `OK`. Run `make help | grep extract` — expected both targets listed.

- [ ] **Step 6: Commit**

```bash
git add tools/gra_extract.py tools/tests/test_gra_extract.py Makefile .gitignore README.md
git commit -m "tools: gra_extract — per-file extraction, manifest, CLI, make re-extract

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

### Task 4: Oracle tests on the shipped set, full run, format note

**Files:**
- Modify: `tools/tests/test_gra_extract.py`
- Modify: `FORMATS.md` (the "Still open" list under `S16*.GRA`)

**Interfaces:**
- Consumes: `extract_file`, `palette_records`, `read_descriptors`, `decode_raw`; `gra_render.main` for the index oracle.

- [ ] **Step 1: Write the gated oracle tests**

Append to `tools/tests/test_gra_extract.py`:

```python
import glob  # noqa: E402

GAME_DIR = os.environ.get('PR_GAME_DIR', os.path.join(os.path.dirname(TOOLS), 'data', 'game', 'C'))
HAVE_GAME = os.path.isfile(os.path.join(GAME_DIR, 'S16TITLE.GRA'))
ORACLE_REQUIRED = 'PR_ORACLE_REQUIRED' in os.environ


@unittest.skipUnless(HAVE_GAME or ORACLE_REQUIRED, 'no game assets at %s' % GAME_DIR)
class OracleTests(unittest.TestCase):
    def setUp(self):
        self.assertTrue(HAVE_GAME, 'PR_ORACLE_REQUIRED set but %s has no S16TITLE.GRA' % GAME_DIR)
        self.tmp = tempfile.TemporaryDirectory()
        self.out = self.tmp.name

    def tearDown(self):
        self.tmp.cleanup()

    def test_title_frame_10_matches_index_oracle(self):
        import gra_render
        from gra_render import chunks
        path = os.path.join(GAME_DIR, 'S16TITLE.GRA')
        ppm = os.path.join(self.out, 'o.ppm')
        idx = os.path.join(self.out, 'o.idx')
        gra_render.main([path, '0', ppm, '--frame', '10', '--indices', idx])
        indices = open(idx, 'rb').read()
        d = open(path, 'rb').read()
        bank = palette_records(next(d[o:e] for t, o, e in chunks(d) if t == 5))
        rec0 = bank[0]
        self.assertEqual(len(rec0), 63)
        self.assertEqual((min(indices), max(indices)), (1, 63))
        expected = b''.join(bytes(rec0[i - 1]) + b'\xff' for i in indices)

        entry = extract_file(path, self.out, {'S16TITLE': bank}, {}, Args())
        s = entry['sprites'][10]
        self.assertEqual((s['kind'], s['width'], s['height'], s['palette_record']), ('rle', 320, 200, 0))
        im = Image.open(os.path.join(self.out, s['png']))
        self.assertEqual(im.mode, 'RGBA')
        self.assertEqual(im.tobytes(), expected)

    def test_all_ten_raw_sentinels_decode(self):
        from gra_render import chunks
        found = []
        for f in sorted(glob.glob(os.path.join(GAME_DIR, 'S16*.GRA'))):
            d = open(f, 'rb').read()
            body6 = next((d[o:e] for t, o, e in chunks(d) if t == 6), None)
            if body6 is None:
                continue
            descs = read_descriptors(body6)
            offs = sorted(x.offset for x in descs if x.kind != 'empty')
            for x in descs:
                if x.kind != 'raw':
                    continue
                nxt = next((o for o in offs if o > x.offset), len(d))
                self.assertEqual(nxt - x.offset, x.width * x.height, (f, x.index))
                rows = decode_raw(d, x.width, x.height, x.offset)
                self.assertEqual((len(rows), len(rows[0])), (x.height, x.width))
                found.append((os.path.basename(f), x.index, x.width, x.height))
        self.assertEqual(len(found), 10, found)
        self.assertIn(('S16TITLE.GRA', 72, 320, 200), found)
        self.assertIn(('S16SLABS.GRA', 36, 320, 200), found)
        self.assertEqual(sum(1 for _, _, w, _ in found if w == 975), 8)
```

- [ ] **Step 2: Run the oracle tests**

Run: `PR_ORACLE_REQUIRED=1 python3 -m unittest tools.tests.test_gra_extract.OracleTests -v`
Expected: 2 tests, `OK`. Then `PR_GAME_DIR=/nonexistent python3 -m unittest tools.tests.test_gra_extract.OracleTests -v` — expected: 2 skipped.

- [ ] **Step 3: Run the full extraction and record the counts**

Run: `time make re-extract`
Expected summary line (stderr) of the form
`wrote 18262 PNGs (18252 rle, 10 raw), 17 empty descriptors, 7 files skipped, 0 errors -> extracted`
— the exact PNG count is whatever the run prints; it must be `18289 - 17 empty - errors`, `raw` must be 10, `skipped` must be 7 and `errors` must be 0. If errors are non-zero, list them (`python3 -c "import json; m=json.load(open('extracted/manifest.json')); print([(k,s['index'],s['error']) for k,v in m['files'].items() for s in v['sprites'] if 'error' in s])"`) and fix the cause before continuing. Spot-check three PNGs by eye: `extracted/S16KON/0000.png` (a character sprite in colour variant 0), `extracted/S16TITLE/0072.png` (a 320×200 raw screen), `extracted/S16CAGE/0000.png` (greyscale).

- [ ] **Step 4: Record the finding in FORMATS.md**

In `FORMATS.md`, under `S16*.GRA` → "Still open", replace the first bullet

```
* Which chunk-5 sub-palette (and which DAC base index) a given sprite uses —
  the per-record `count/colour` groups are read as one flat palette by
  `gra_render.py`; the game likely selects a sub-palette per sprite.
```

with

```
* Which chunk-5 sub-palette (and which DAC base index) a given sprite uses —
  the per-record `count/colour` groups are read as one flat palette by
  `gra_render.py`; the game selects a sub-palette per sprite at runtime
  (the compositor's bank offset). **Likely, from the bytes:** sprite indices
  are **1-based into the record** — across 3,700 sampled sprites no opaque
  pixel carries index 0 and the highest index equals the record size
  (`S16FONTS` `1..7` vs a 7-colour record, `S16TRB` `1..9` vs 9, `S16CONTI`
  `1..24` vs 24, `S16KON` `1..31` vs its 31-colour records), so
  `colour = record[index - 1]`. `tools/gra_extract.py` renders with that rule
  and picks the first record that fits; not read from the decompilation.
```

- [ ] **Step 5: Run everything and commit**

Run: `make re-extract-test` (expected 30 tests `OK`), then `make verify` to confirm nothing in the port ladder was touched (expected `all checks passed`).

```bash
git add tools/tests/test_gra_extract.py FORMATS.md
git commit -m "tools: gra_extract — oracle tests on the shipped set; FORMATS: 1-based sub-palette index

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>"
```

---

## Self-review against the spec

- §2 scope: enumeration, RLE + raw decode, palette rule, PNG + manifest, `make re-extract`, tests — Tasks 1–4. Out-of-scope items are not touched.
- §3 interface: `GAME_DIR`, optional `OUT_DIR`, `--only`, `--palette-record`, `--palette-from` — Task 3 `main`. Pillow only — no packaging files added.
- §4 output: directory per stem, `%04d.png`, RGBA with alpha 0 on transparent runs, manifest fields `gra`, `resource_index`, `palette_source`, `palette_records`, `skipped`/`reason`, per-sprite `index kind width height x y pixel_offset handle png palette_record max_index rle_bytes out_of_palette error` — Task 3 tests assert each.
- §5 palette rule: bank / record / colour steps and both flags — Task 2 units, Task 3 wiring; greyscale is zero-based, records 1-based.
- §6 units: every listed function exists with the listed name; `parse_frames` reused, handle re-read from the raw record.
- §7 tests: fixture with the three run types, raw sentinel, empty record, records `[3, 8]`; borrowing; flags; gated cross-check on `S16TITLE` 10; ten raw sentinels — all present. `make re-extract` summary — Task 3/4.
- §8: the 1-based note lands in `FORMATS.md` as `likely` (Task 4).
