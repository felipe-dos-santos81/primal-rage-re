# File formats — Primal Rage (DOS)

Everything marked **verified** was confirmed against the shipped files and/or
the Ghidra decompilation.

## `PRAGE.EXE` — DOS/4GW *bound* Linear Executable (verified)

MS-DOS MZ executable whose real image is an embedded **LE** ("Linear
Executable") loaded by **DOS/4GW**; the game itself is 32-bit protected-mode
WATCOM C/C++32 code.

```
[MZ stub + DOS/4GW loader]
[LE header            @ 0x290A4]
[LE tables            (object table, page map, fixups ...)]
[embedded page data   (located via the LE object page map)]
```

| Field | Value |
|---|---|
| LE header | `0x290A4` |
| CPU / OS | `2` / `1` (DOS) |
| Page size / count | `0x1000` / `213` |
| Entry | object 1 offset `0x5245C` (linear **`0x6245C`**) |
| Object 0 (code) | linear `0x10000`, size `0x63B15`, 100 pages, `read|exec|preload|32-bit` |
| Object 1 (data) | linear `0x80000`, size `0x8B0D0`, 113 pages, `read|write|preload|32-bit` |
| Data-pages offset | `0x3C800` (relative to the embedded MZ image base at file `0x26654`, so page data begins at `0x62E54`) |

The embedded page data is not simply `LE_header + data_pages_offset`: the
`0x3C800` field is relative to the **embedded MZ image base** at file `0x26654`
(the LE header at `0x290A4` sits `0x2A50` bytes into it), which is why
`mem.c`'s `find_bound_base()` locates the inner MZ rather than adding the field
to the LE header. DOS/4GW then resolves the pages through the object page map.
**Use the
[`ghidra-lx-loader`](https://github.com/yetmorecode/ghidra-lx-loader)** — it
places the objects at their LE rel bases and applies the fixups, so absolute
data references in the decompilation are already correct.

## `INDEX` — resource index (verified)

`data/game/C/INDEX` is a flat array of 20-byte records, one per resource:

```c
struct IndexEntry {       // 20 bytes, on disk
    char name[12];        // ASCII, zero padded
    uint32_t size_flags;  // bits 0..23 = size, bits 24..31 = flags
    uint32_t reserved;    // 0 on disk; holds the loaded pointer at runtime
};
```

* `size` (low 24 bits) equals the on-disk file size of the named file.
* `flags` byte observed as `0x01` and `0x02` (meaning TBD).
* The shipped `C/INDEX` has **69 entries** (`s16*.gra`, plus `*.txt`).

Runtime model (from the decompilation):

* `FUN_0001B120` reads the whole file, computes `count = filesize / 0x14` and
  keeps a 20-byte entry per resource. It tests `entry.flags & 0x1000000`
  (bit 24), uses `entry.size` (`& 0xFFFFFF`) for sizing, and fills
  `entry+0x10` with the loaded data pointer.
* `FUN_0001B544` resolves a *resource handle* into a pointer:
  `entry = table + (handle >> 23) * 0x14; pointer = entry.data + (handle & 0x7FFFFF)`.
  I.e. a handle packs a resource index (high bits) and a byte offset (low 23 bits).
* File/memory helpers live around `0x61C60`–`0x62xxx`.

## `S16*.GRA` — graphics (chunk format and payloads verified)

Each `.GRA` is a **linked list of chunks**:

```c
struct GraChunk {       // 8-byte header, then the body
    uint16_t type;
    char     magic[2];  // '4','3'  (0x34 0x33)
    uint32_t next;      // absolute file offset of the next chunk, 0 = last
};
```

Chunk types seen:

| Type | Where | Computed layout |
|---|---|---|
| `2` | always first | RLE pixel blobs, addressed by the type-6 handles (largest body) |
| `5` | files with palettes | palette bank: repeated `{ u32 count; u32 colour[count] }` |
| `6` | last chunk | frame descriptor table: `body_len / 12` 12-byte records |

Examples (`tools/gra_chunks.py`):

```
S16FONTS.GRA : type 2 @0x0 (39244) -> type 5 @0x9954 (144) -> type 6 @0x99EC (4644)
S16CAGE.GRA  : type 2 @0x0 (187712) -> type 6 @0x2DD48 (324)
S16TITLE.GRA : type 2 @0x0 (1501656) -> type 5 @0x16E9E0 (2928) -> type 6 @0x16F558 (1380)
S16COBSD.GRA : type 2 @0x0 (145427)  (single chunk; `next` = 0)
```

### Decoded payload layout

All three types are now decoded. Every claim below is **verified** against the
decompilation and independently against the shipped bytes by
`tools/gra_render.py`. Across all 69 `.GRA` files, **18,201 of 18,202**
descriptors that carry positive dimensions and a next-sprite offset RLE-decode
to *exactly* that next offset (17 zero-dimension records and 10
negative-dimension sentinels are excluded — see "Still open").

**Type 6 — frame descriptor table (verified).**
`body_len / 12` records, each 12 bytes:

```c
struct GraFrame {        // on disk, 12 bytes
    uint16_t width;      // pixels
    uint16_t height;     // rows
    int16_t  x;          // signed origin / hotspot
    int16_t  y;
    uint32_t pixels;     // resource handle: (index << 23) | file_offset
};
```

`s16rad.gra` chunk 6 is 468 B = 39 records; record 0 is
`{122, 107, 122, 0, 0x0F80028A}` — width 122, height 107, pixel offset
`0x28A` into chunk 2 of resource index 31 (`s16rad.gra`). The in-game consumer
is `FUN_0001c528` (`port/decomp/prage.c`), which resolves the handle and reads
`[0]`..`[2]`; the static table `DAT_000a8b30` is a list of **18,443
consecutive resource handles before the first non-handle**, 18,442 of which
point into a chunk-6 body and **every one of those is 12-byte aligned** (scan
the data object at offset `0xA8B30 - 0x80000`). The `x`/`y` anchor reading is
**verified** by signed values (`-3`, `-35`, `-219`, …); the exact meaning of
each as sprite origin is **likely**.

**Type 2 — RLE pixel data (verified).**
The blobs are 8-bit palette-index bitmaps with per-sprite RLE. A row is
`width` pixels; there are `height` rows, decoded back to back with no row
marker. Dispatch is in order — bit 7 first, then bit 6 — and the conditions are
complete, so a byte in `0x00..0x7F` is always a literal and never a
transparent run:

| Control byte | Meaning |
|---|---|
| `b & 0x80 == 0` | literal run of `b & 0x7F` pixels, each followed by its own colour byte |
| `b & 0x80 != 0 && b & 0x40 == 0` | repeat run of `b & 0x3F` pixels, one colour byte follows |
| `b & 0x80 != 0 && b & 0x40 != 0` | transparent run of `b & 0x3F` pixels, no data |

The in-game decoder is `FUN_00041030` (0x41030): its first pass measures each
row (`iVar9 -= bVar2 & 0x7f` / `& 0x3f`, skipping `1+count` bytes for literals
and `2` for repeats — i.e. the literal payload *is* the per-pixel colour), and
its second pass rasterises the same tokens into a fixed 1 bpp opacity mask
(`0x26` = 38 bytes = 304 bits per row). The blobs are **independent**, not
delta-coded: `tools/gra_render.py` decodes descriptors standalone to exactly
`next_offset - offset` bytes (18,201/18,202 across all 69 files, exclusions
above), so the "delta relative to the previous frame" note in the brief is
**not** observed.

**Type 5 — palette bank (verified).**
Concatenated `{ u32 count; count × u32 colour }` records, no outer count:
parse until the body ends (the parse consumes the whole body exactly —
`S16FONTS` 27 colours / 144 B, `S16TITLE` 720 / 2928 B, `S16BEACH` 90 / 368 B).
The in-game consumer is `FUN_00033754`, which resolves a handle and reads
`count = *ptr` to build a palette record, and `FUN_0001c470` (0x1c470), the
VBlank-gated DAC flush, which for a handle takes `FUN_0001b544() + 4` and emits
`count` colours with

```c
r = (word >> 2)  & 0xFF;   // bits  2.. 9
g = (word >> 10) & 0xFF;   // bits 10..17
b = (word >> 18) & 0xFF;   // bits 18..25
```

which is exactly the packing of the chunk-5 words (they form descending
shading ramps, e.g. `S16FONTS` palette 0 = `0090d0f0 0070b0d0 … 00001010`).
A scan of the data object finds resource handles pointing into chunk-5 bodies
for **27** of the 30 `.GRA` files that contain a type-5 chunk — e.g.
`s16title` (19 chunk-5 handles), `s16jap` (47) and `s16beach` (12), while
`s16fonts` has none — so the bank is a real resource, not a stray table.

**Still open (marked likely, not promoted):**

* Which chunk-5 sub-palette (and which DAC base index) a given sprite uses —
  the per-record `count/colour` groups are read as one flat palette by
  `gra_render.py`; the game likely selects a sub-palette per sprite.
* The type-6 `x`/`y` fields as sprite origin vs. bounding-box corner is
  **likely** (evidence: signed small values, both signs present).
* 10 descriptors carry "negative" dimensions: 2 are `(-320, -200)` (the
  320×200 screen, in `s16title` and `s16slabs`) and 8 are `(-975, h)` with
  `h ∈ {-53, -62, -64, -79}` (in `s16beach`, `s16caves`, `s16citys`,
  `s16grave`, `s16himal`, `s16jungl`, `s16stone`, `s16volcn`). These read as
  full-screen blit / clear sentinels. A further 17 records have `width == 0`
  (5 of them in `s16fonts`). Both families are excluded from the
  exact-consumption count above and are not decoded here.
* The exact `x`/`y` semantics are characterised above, not proven (see the
  Type 6 note).

**Renderer scope (what the oracle can and cannot pin).**
`tools/gra_render.py` flattens the whole chunk-5 palette bank into one palette
and maps transparent / index 0 to black; the game selects a sub-palette per
sprite, so byte-for-byte agreement is **not** a general property. The C port
(`port/src/platform/gra.c`) therefore takes the **exact-consumption property**
(a decoded frame consumes exactly `next_sprite_offset - frame_offset` bytes) as
its primary acceptance — 18,201/18,202 across all 69 files, the single miss
being a `S16TITLE` 74×167 descriptor at `0x349d7`. For the four full-screen,
fully-opaque `S16TITLE` frames `{10,12,13,18}` (0 transparent pixels, no opaque
index 0, indices ≤ 63) the port's index buffers and RGB PPMs are additionally
**byte-identical** to `gra_render.py` — an exact comparison valid for that
subset only, not a claim about palettes or frames in general.

Run `tools/gra_render.py FILE.GRA 0 out.ppm --frame N` to reproduce any frame
(the first `--palette`-less run uses the file's first type-5 chunk, otherwise a
greyscale ramp); add `--indices OUT.idx` to emit the raw index buffer, which is
what the byte-exact index comparison uses.

## `FAT.OPL` / `FAT.AD` — FM patch bank (verified)

The two patch banks the Miles FM drivers load. Layout, confirmed from the bytes
and cross-checked against the captured OPL trace (Task 8):

```
table:    { u16 key (LE); u32 payload_offset (LE) }   step 6, terminator key 0xFFFF
          key = (bank << 8) | program
          bank 0x00 = melodic programs 0x00..0x7F
          bank 0x7F = percussion, keyed by MIDI note (0x23..0x57 shipped)
payload:  14 bytes at payload_offset:
          [0]  0x0E  (entry marker)   [1] 0x00   [2] key/percussion base
          [3..7]   modulator: 0x20, 0x40, 0x60, 0x80, 0xE0
          [8]      0xC0 feedback/connection
          [9..13]  carrier:   0x20, 0x40, 0x60, 0x80, 0xE0
```

`FAT.OPL` is 3622 bytes: 181 records (0x0000..0x007F then 0x7F23..0x7F57),
table `0x000..0x43D`, the `0xFFFF` terminator at `0x43E`, payloads from `0x440`
at `0xE` bytes each. The first four payload offsets `0x440`, `0x44E`, `0x45C`,
`0x46A` confirm the `0xE` step (the design document's earlier "`0x400`" read
`FAT.OPL` bytes `40 04` as big-endian; the little-endian u32 offset is
`0x440`).

Payload decode is **verified** against the capture: for program 0x7A the
capture writes modulator `0x20=0x0E, 0x40=0x00, 0x60=0xF6, 0x80=0x00` and
carrier `0x20=0xC0, 0x60=0x1F, 0x80=0x02, 0xE0=0x03`, `0xC0=0x3E`, matching
payload `0e 00 00 0e 00 f6 00 00 0e c0 00 1f 02 03` byte-for-byte (the driver
ORs `0x30` into `0xC0` — OPL3 left/right output — and scales the carrier TL by
velocity; see `port/spec/audio.md`).

The command that showed the table and payloads:

```sh
python3 -c "d=open('data/game/C/FAT.OPL','rb').read(); \
print([ (hex(int.from_bytes(d[6*n:6*n+2],'little')), hex(int.from_bytes(d[6*n+2:6*n+6],'little'))) for n in range(5)])"
# [(0x0, 0x440), (0x1, 0x44e), (0x2, 0x45c), (0x3, 0x46a), (0x4, 0x478)]
```

## XMIDI music banks (verified)

Each level GRA, `S16SOUND`/`S16SND2`/`S16TITLE`/`S16SELMO`, and `PRAGE.EXE`
(`0xC87C4`) carry an XMIDI bundle (Task 1). Container:

```
FORM XDIR { INFO }        directory
CAT  XMID { FORM XMID { TIMB, RBRN, EVNT } }
```

* `TIMB` — 16-bit little-endian `(bank << 8) | program` keys: the timbres the
  sequence uses (e.g. `S16TITLE` begins `0017 0000 0049 0058 …`).
* `EVNT` — the event stream; grammar in `port/spec/audio.md` ("Music event
  grammar"). `FORM`/chunk sizes are **big-endian** IFF sizes; data fields
  (offsets, TIMB keys) are little-endian.

## Sample data (`S16SOUND.GRA`) — RIFF/WAVE PCM (verified)

The one PCM sample the installed set contains is a standard **RIFF/WAVE** blob
inside `S16SOUND.GRA` at file offset `0x24b13` (a scan of every installed file
found `WAVE` only there and in the installer's own `SETSOUND.EXE`):

```
RIFF@0x24b13  WAVE
fmt : audioformat=1 (PCM), channels=1, rate=11025 Hz,
      byterate=11025, align=1, bits=8
data: 19327 bytes   (8-bit unsigned mono; byte 128 is centre)
```

`verified` (cmd: the full RIFF/WAVE scan is in `port/spec/audio.md` "Samples").
The rate matches the AIL preference the game sets at init (`FUN_0005d87e(1,0x2b11)` in
`FUN_0001cf40`). The declared RIFF size is **19432** because a trailing
`LIST`/`INFO` (and `fact`) chunk follows the PCM, so the container size field
must not be trusted; the port's parser walks chunks and bounds each against the
buffer. The conversion from 8-bit unsigned to the mixer's s16 exists exactly
once (`samples_to_s16`, `port/src/platform/audio/samples.c`).

Other in-play sound effects are likely stored as headerless AIL sample blocks
inside the level GRAs / `S16SND2.GRA` and were not located
(`TODO(verify)`, `port/spec/audio.md` "Samples").

## `TWI5.SMK` / `TWG.SMK` — Smacker video (verified)

`data/game/C/TWI5.SMK` (1,208,576 B) and `TWG.SMK` (31,048 B) are **SMK2**,
320×200, 121 and 41 frames, **silent**. Both reconcile to the byte
(`data_off + Σ(frame_size & ~3) == filesize`) and are byte-identical to the CD
copies (`RAGE.S16/TWI5.SMK`, `RAGE.S16/TWG.SMK`). Layout:

| Offset | Size | Field |
|---|---|---|
| `0x00` | 4 | magic `"SMK2"` |
| `0x04` / `0x08` | 4 | width / height = 320 / 200 |
| `0x0C` | 4 | frame count = 121 (`TWI5`) / 41 (`TWG`) |
| `0x10` | 4 | `pts_inc` signed: −7100 (`TWI5`) / −14200 (`TWG`); `|pts_inc| × 10 us` = the frame period |
| `0x14` | 4 | flags (0; bit0 ring frame, bit1 Y-interlace, bit2 Y-double) |
| `0x18` | 28 | unused audio data |
| `0x34` | 4 | `treesize` (40763 / 5835) |
| `0x38` | 16 | four tree sizes (mmap, mclr, full, type): `(31472, 2760, 109136, 1984)` / `(1088, 1824, 17328, 1488)` |
| `0x48` | 28 | 7 × (u24 rate + u8 flag) audio descriptors — all zero ⇒ silent |
| `0x64` | 4 | padding |
| `0x68` | `4*frames` | `frame_size[i]`; low 2 bits are flags (bit0 = keyframe — clear in both) |
| | `frames` | `frame_flags[i]`; bit0 = palette update, bits 1..7 = audio track |
| | `treesize` | tree bitstream (LSB-first) |
| | `Σ(frame_size & ~3)` | frame payloads, concatenated |

The **fixed profile** (what the port's decoder accepts): all four Huffman trees
present; MONO / FULL / SKIP / FILL 4×4 blocks with the 64-entry run table;
palette update on frames `0` (and TWI5 `101`, `102`), whose chunk starts with a
length byte in 4-byte units; no keyframe bits, no audio. Everything else is
rejected by name. The tree/palette/block decode is **verified** by pixel-exact
agreement against DOSBox-X frames (RGB24, palette included): TWI5 120/120, TWG
41/41. `tools/smk_info.py` proves the layout; `tools/smk_capture.py` captures
the original; `tools/smk_compare.py` is the oracle. See
`docs/superpowers/plans/2026-09-17-smacker-video-report.md` and the format
reference in `docs/superpowers/plans/2026-09-17-smacker-video.md`.

## Other files (not yet analysed)

* `PR.BMP`, `IMAGES.IMJ`, `INSTALL.EXE`, `RAMDTCT.EXE` — installer/CD assets.
* `RAGE.S04`, `RAGE.S08`, `RAGE.S16` (CD) — the same game at 320×200 /
  640×400 / 640×480 (`S04`, `S08`, `S16` are the three graphics sets; the CD
  `PRAGE.EXE` differs from the installed one only in the graphics prefix).
* `RAGE.SND` — AIL driver directory (no music or sample payload); see
  `port/spec/audio.md` "`RAGE.SND` on the CD is a driver directory".
* `DIG.INI`, `MDI.INI`, `*.DIG`, `*.MDI`, `RM.DRV`, `FAT.AD` —
  Miles/AIL sound driver set (third-party). `FAT.OPL`/`FAT.AD` are the patch
  banks — see the `FAT.OPL` section above.
