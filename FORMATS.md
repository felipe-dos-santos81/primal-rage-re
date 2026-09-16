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
| Data-pages offset | `0x3C800` (from the LE header) |

The embedded page data is not simply `LE_header + data_pages_offset`; DOS/4GW
resolves it through the object page map. **Use the
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

## `S16*.GRA` — graphics (chunk format verified, payload TBD)

Each `.GRA` is a **linked list of chunks**:

```c
struct GraChunk {       // 8-byte header, then the body
    uint16_t type;
    char     magic[2];  // '4','3'  (0x34 0x33)
    uint32_t next;      // absolute file offset of the next chunk, 0 = last
};
```

Chunk types seen:

| Type | Where | Notes |
|---|---|---|
| `2` | always first | pixel/bitmap data (largest body) |
| `5` | FONTS, TITLE, JAP | frame/animation descriptor table |
| `6` | last chunk | trailing table / palette |

Examples (`tools/gra_chunks.py`):

```
S16FONTS.GRA : type 2 @0x0 (39244) -> type 5 @0x9954 (144) -> type 6 @0x99EC (4644)
S16CAGE.GRA  : type 2 @0x0 (187712) -> type 6 @0x2DD48 (324)
S16TITLE.GRA : type 2 @0x0 (1501656) -> type 5 @0x16E9E0 (2928) -> type 6 @0x16F558 (1380)
S16COBSD.GRA : type 2 @0x0 (145427)  (single chunk; `next` = 0)
```

**Open:** the pixel encoding of chunk type 2 (bit depth, sprite frames /
palette indexing / inter-frame deltas) and the exact layout of chunks 5 and 6.
`S16FONTS` chunk 5 begins with `u16 version=5, '43', u32 next, u32 count=7`
followed by 7 descending value pairs — probably a per-font colour table.

## Other files (not yet analysed)

* `PR.BMP`, `IMAGES.IMJ`, `INSTALL.EXE`, `RAMDTCT.EXE` — installer/CD assets.
* `RAGE.S04`, `RAGE.S08`, `RAGE.S16` (CD) — the same game at 320×200 /
  640×400 / 640×480 (`S04`, `S08`, `S16` are the three graphics sets; the CD
  `PRAGE.EXE` differs from the installed one only in the graphics prefix).
* `RAGE.SND` — audio.
* `twi5.smk`, `twg.smk` — Smacker video (logos / intro).
* `DIG.INI`, `MDI.INI`, `*.DIG`, `*.MDI`, `RM.DRV`, `FAT.OPL`, `FAT.AD` —
  Miles/AIL sound driver set (third-party).
