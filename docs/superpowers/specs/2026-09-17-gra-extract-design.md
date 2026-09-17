# Design: `tools/gra_extract.py` — extract the original S16 sprites to PNG

A standalone extraction tool. It turns the shipped `S16*.GRA` graphics into
one RGBA PNG per sprite plus a JSON manifest, so the originals can be used as
reference material (for example to recreate them with generative AI) and so a
future override loader in the port can key a replacement image back to the
sprite it replaces. The tool lives in `tools/`, shares no code with `port/`,
and changes nothing under `port/`.

The override loader itself is **out of scope**; this spec only fixes the
output contract it will consume.

## 1. What is already known

* `FORMATS.md` documents the GRA container (8-byte chunk chain), the type-6
  descriptor table (`{u16 w; u16 h; i16 x; i16 y; u32 pixel_handle}`), the
  type-5 palette bank (`{u32 count; u32 colour[count]}` records, colour word
  `R = bits 2..9, G = 10..17, B = 18..25`) and the type-2 sprite RLE (literal /
  repeat / transparent runs). All are **verified**.
* `tools/gra_render.py` is the independent Python decoder and the C port's
  oracle. Its `chunks`, `parse_frames`, `parse_palette` and `decode_sprite`
  functions are reused as-is; the extractor is a second consumer of the same
  decoder, not a third implementation.
* The CD ships three graphics sets. They are RAM-tier installs, not
  resolutions: `S16` is the superset and is byte-identical to the installed
  `data/game/C` set the port reads (checked for `S16KON.GRA`).

  | Set | GRA files | Descriptors | Files with a palette |
  |---|---|---|---|
  | `RAGE.S04` | 416 | 8,650 | 38 |
  | `RAGE.S08` | 190 | 12,133 | 29 |
  | `RAGE.S16` | 69 | 18,289 | 30 |

* Sprites index a **sub-palette**, not the whole bank, and the index is
  **1-based**: across 3,700 sampled sprites no opaque pixel carries index 0,
  and the highest index equals the record size exactly (`S16FONTS` uses
  `1..7` against a 7-colour record, `S16TRB` `1..9` against 9, `S16CONTI`
  `1..24` against 24, `S16KON` `1..31` against its four 31-colour records).
  So `colour = record[index - 1]`. Character files carry records of
  `[31, 31, 31, 31, 9, 9]`, one 31-colour record per colour variant. Which record applies is a
  runtime, per-actor choice (`docs/superpowers/specs/2026-09-17-sprite-compositor-design.md`,
  the bank offset). A static extractor cannot know it; it can only choose a
  plausible default and record the choice.
* Ten descriptors carry negative dimensions. For every one of them
  `|w| * |h|` equals the distance to the next pixel blob, so they are
  **raw, uncompressed 8-bit bitmaps**: eight `975 x {53,62,64,79}` stage
  backdrops and two `320 x 200` screens (`S16TITLE` 72, `S16SLABS` 36).
  Seventeen descriptors have `w == 0` and address nothing.
* Seven files have no type-6 table and carry no graphics: `S16*SD.GRA`,
  `S16SOUND.GRA`, `S16SND2.GRA`.

## 2. Scope

**In:** enumerate every descriptor of every GRA in a directory; decode RLE
and raw sprites; choose and apply a palette; write RGBA PNGs and a manifest;
a `make re-extract` target; tests.

**Out:** the port-side override loader; any HD / upscaled asset pipeline;
sprite sheets or animation grouping; the `S04`/`S08` sets (the tool will
read them if pointed at them, but nothing is verified for them — seven
`S04*SH.GRA` files have a different chunk layout and are rejected by
`chunks()`).

## 3. Interface

```
tools/gra_extract.py GAME_DIR [OUT_DIR] [--only GLOB] [--palette-record N]
                     [--palette-from FILE.GRA]
```

| Argument | Meaning |
|---|---|
| `GAME_DIR` | directory of `*.GRA` files; `data/game/C` in the Makefile target |
| `OUT_DIR` | default `extracted/` (added to `.gitignore` by the plan) |
| `--only GLOB` | restrict to matching file names, e.g. `S16KON*` |
| `--palette-record N` | force sub-palette record `N` for every sprite of every file that has a bank |
| `--palette-from FILE` | force the palette bank of `FILE` for every file processed (overrides borrowing) |

Dependencies: Python 3.12 and **Pillow** (already installed; noted in
`README.md`). No new packaging files.

## 4. Output contract

```
OUT_DIR/
  manifest.json
  S16KON/0000.png ... 0861.png
  S16TITLE/0072.png            (raw 320x200 screen)
  ...
```

* One directory per GRA stem; one `NNNN.png` per descriptor index, zero-padded
  to four digits (largest table is `S16TRB` at 1,099 records).
* PNGs are **RGBA**. Transparent runs are `(0,0,0,0)`. Every opaque pixel is
  alpha 255, including opaque index 0 — RGBA is used precisely so that
  transparency never collides with a palette index.
* `manifest.json`:

```json
{
  "source_dir": "data/game/C",
  "files": {
    "S16KON": {
      "gra": "S16KON.GRA",
      "resource_index": 49,
      "palette_source": "S16KON",
      "palette_records": [31, 31, 31, 31, 9, 9],
      "skipped": false,
      "sprites": [
        {"index": 0, "png": "S16KON/0000.png", "kind": "rle",
         "width": 83, "height": 73, "x": 35, "y": 54,
         "pixel_offset": 1131291, "handle": "0x1891431b",
         "palette_record": 0, "max_index": 31, "rle_bytes": 3296}
      ]
    }
  }
}
```

  * `kind` is `rle`, `raw` or `empty` (`w == 0` or `h == 0`; no `png`).
  * `resource_index` is the file's position in `INDEX` (the `handle >> 23`
    field), read from `GAME_DIR/INDEX` when present, else `null`.
  * `palette_source` names the stem whose bank was used, or `"greyscale"`.
  * `palette_record` is the record chosen for this sprite, `"flat"` when no
    record fits, or `null` when greyscale. `out_of_palette` (present only
    when non-zero) counts opaque pixels whose index has no colour.
  * Files with no descriptor table appear with `"skipped": true` and a
    `"reason"`.

The pair `(gra stem, index)` is the override key. `handle` and
`pixel_offset` are carried so the port can be cross-checked against its
`DS_000A8B30` handle table without re-parsing.

## 5. Palette rule

Applied per sprite, in this order:

1. **Bank.** Use the file's own type-5 bank. If the file has none, borrow
   from the shortest-named file with a bank that shares this stem's first
   three letters (`KONSH` → `KON`, `COBSH` → `COB` not `COBFT`, `ESTKO` →
   `ESTIL`). If none matches (`CAGE`, `GLIFE`, `RAD`), use a 256-step
   greyscale ramp. `--palette-from` replaces this step.
2. **Record.** Within the bank, take the first record whose `count` is at
   least the sprite's highest used index (`max_index <= count`). If no record
   is large enough, fall back to the bank flattened in order and set
   `palette_record` to `"flat"`. `--palette-record N` replaces this step.
3. **Colour.** `index → record.colour[index - 1]` (1-based, see section 1),
   decoded with the verified `R/G/B` bit fields. Index 0 opaque or an index
   past the record is flagged magenta and counted in the manifest as
   `out_of_palette`. Raw blobs use the same rule. Greyscale maps
   `index → (index, index, index)`.

This is a heuristic and is labelled as such in the manifest. It is right
whenever a sprite's sub-palette is the first that fits, which is the common
case for stage, font and UI files with one dominant record; for character
files it yields colour variant 0 for every sprite. The lossless information
(indices via `gra_render.py --indices`, the full bank) is never discarded, so
a later, verified rule can re-render without re-extraction.

## 6. Structure

`tools/gra_extract.py`, roughly 250 lines, with these units:

| Unit | Does | Depends on |
|---|---|---|
| `load_index(path)` | `INDEX` → `{stem: resource_index}` | `struct` |
| `decode_raw(d, w, h, off)` | rows of `(index, True)` for a raw blob; verifies `off + w*h <= len(d)` | — |
| `choose_bank(stem, banks, args)` | palette-rule step 1 | — |
| `choose_record(records, max_index, args)` | palette-rule step 2 | — |
| `to_rgba(rows, w, h, colours)` | bytes for `Image.frombytes("RGBA", …)` | Pillow |
| `extract_file(path, out_dir, ctx)` | one GRA → PNGs + manifest entry | all of the above, `gra_render` |
| `main(argv)` | args, enumerate, write `manifest.json`, print a summary | — |

Sentinel detection is by sign: a descriptor whose `w` or `h` reads as
negative (`>= 0x8000` unsigned) is raw with `|w|`, `|h|`. `parse_frames`
returns them unsigned today; the extractor normalises before use.

Errors in one sprite (truncated RLE, offset out of range) are recorded in
that sprite's manifest entry as `"error"` and do not stop the run; the exit
status is non-zero if any error occurred.

## 7. Testing

Tests live in `tools/tests/test_gra_extract.py` and run under
`python3 -m unittest`; the Makefile gains `re-extract-test`.

| Test | Proves |
|---|---|
| synthetic GRA fixture (built in the test: chunk 2 with one RLE sprite using literal, repeat and transparent runs; one raw 4×3 sentinel; one `w == 0` record; chunk 5 with records `[3, 8]`; chunk 6) | end-to-end: PNG dimensions, alpha 0 exactly on transparent runs, opaque index 0 stays opaque, raw decode, `empty` kind, record choice (`max_index 2` → record 0, `max_index 5` → record 1) |
| borrowing | `KONSH`-style stem with no bank borrows the shortest matching prefix; no match → greyscale and `palette_source == "greyscale"` |
| flags | `--palette-record` and `--palette-from` override the rule |
| cross-check (requires `data/game/C`; skipped with a message when absent, required under `PR_ORACLE_REQUIRED=1`) | for `S16TITLE` frame 10 (indices `1..63`, record 0 has 63 colours) the PNG equals, pixel for pixel, the RGB computed from `gra_render.py --indices` and record 0 with the 1-based rule, alpha 255 everywhere (the frame is fully opaque) |
| raw sentinels (same gate) | all ten negative-dimension descriptors decode with `|w|*|h|` bytes and produce PNGs of `|w| x |h|` |

`make re-extract` runs the tool on `data/game/C` into `extracted/` and
prints the count of PNGs written, sprites skipped and errors.

## 8. Risks and open items

* **The record choice is a guess** for any sprite whose sub-palette is not
  the first that fits. The 1-based mapping is strongly evidenced but not
  read from the decompilation; `FORMATS.md` records it as `likely`. Mitigated by recording the choice and by keeping the
  lossless data reachable; not solved here.
* **Pure-Python decode speed.** ~44 MB of RLE across 18k sprites; expected
  around a minute. Acceptable for a batch tool; no numpy fast path unless it
  proves too slow.
* **`resource_index` is informational.** The port maps `handle >> 23`
  through the runtime `INDEX` table; the extractor reads the on-disk `INDEX`
  and assumes the same ordering, which `FORMATS.md` states.
