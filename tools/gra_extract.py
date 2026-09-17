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
