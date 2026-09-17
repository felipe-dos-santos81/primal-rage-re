# Smacker video (sub-project 2b-i) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Boot-time Smacker logos (`twi5.smk`, `twg.smk`) decode and present pixel-exact on the existing 320×200 path, driven by the game's own player call sites.

**Architecture:** One allocation-free decoder module (`platform/smacker.{c,h}`) parses the SMK2 container and decodes frames into a caller-owned index buffer; one thin player module (`game/movie.{c,h}`) opens the file by name, paces frames, writes the palette into `gfx_dac`, and lets the existing `gfx_present`/`swap_buffers` tail present it. No SDL, no `mem[0xA0000]`. The streamed-audio machinery is a **separate plan (2b-ii)**; it is out of scope here.

**Tech Stack:** C11 (host port), Python 3 (oracle tooling), DOSBox-X (original-frame capture), CMake/Make, the project's `test.h` assert harness.

## Global Constraints

- Repo: `/Users/felipe.dos.santos/code/mine/primal-rage-reverse`. Port sources under `port/src`, tests under `port/tests`, tools under `tools/`.
- **Licence:** clean-room reimplementation of the public Smacker format. Do **not** copy code from FFmpeg (`libavcodec/smacker.c`, `libavformat/smacker.c`), libsmacker, or ScummVM — all are LGPL/GPL. Use the format description in this plan and the captured oracle. The port has no licence file; vendoring nothing.
- **Aperture rule:** never write `mem[0xA0000]`. The decoder writes only caller-owned buffers; `gfx_dac` is the palette target.
- **No dynamic allocation** in `smacker.{c,h}`; it is caller-owned and fixed-capacity (reject what does not fit).
- **Bounds first:** every declared size is validated against the real buffer before use (`seq_bank_size` lesson).
- **SDL** stays in `port/src/host.c` / `port/src/main.c` only.
- **Style:** no comments except `/* PORT: ... */` for deviations and `/* TODO(verify): ... */` for doubts. Match surrounding code.
- Fixed profile: accept only what `data/game/C/TWI5.SMK` and `TWG.SMK` use; reject everything else with a named reason.
- Verification ladder (must stay green end-to-end after every task): `make verify` (build 0 warnings + headless frames + `PR_ORACLE_REQUIRED=1 ./build/run_tests` + `symbols.h` byte-identical).
- Captures and generated oracles are git-ignored, never committed. `data/` is read-only.
- Every code-writing task ends with `make verify` green and a commit.

---

## Format reference (derived from the two shipped files; verified)

Both movies are `SMK2`, 320×200, no audio. The container layout below reconciles each file to the byte (`delta = 0`).

### Header

| Offset | Size | Field |
|---|---|---|
| `0x00` | 4 | magic `"SMK2"` (accept `"SMK4"` only if a real file needs it; none does) |
| `0x04` | 4 | width (u32 LE) = 320 |
| `0x08` | 4 | height = 200 |
| `0x0C` | 4 | frame count = 121 (`TWI5`) / 41 (`TWG`) |
| `0x10` | 4 | `pts_inc` (signed; `TWI5` −7100, `TWG` −14200) — pacing is an **open item**, see Task 1 |
| `0x14` | 4 | flags (0 here; bit0 ring frame, bit1 Y-interlace, bit2 Y-double) |
| `0x18` | 28 | unused audio-related data (skipped) |
| `0x34` | 4 | `treesize` — length of the tree bitstream (TWI5 40763, TWG 5835) |
| `0x38` | 16 | four tree sizes (mmap, mclr, full, type) = TWI5 `(31472, 2760, 109136, 1984)`, TWG `(1088, 1824, 17328, 1488)` |
| `0x48` | 28 | 7 × (u24 rate + u8 flag) audio descriptors — **all zero ⇒ silent** |
| `0x64` | 4 | padding |
| `0x68` | `4*frames` | `frame_size[i]`; low 2 bits are flags (bit0 = keyframe) |
| | `frames` | `frame_flags[i]`; bit0 = palette update, bits 1..7 = audio track present |
| | `treesize` | tree bitstream |
| | `sum(frame_size[i] & ~3)` | frame payloads, concatenated in order |

### Frame payload

`frame_size[i] & ~3` bytes. If `frame_flags[i] & 1`, a variable-length **palette update** comes first and the remainder is the video bitstream; otherwise the video bitstream is the whole payload and the palette is unchanged.

**Palette state** is 256 entries × 3 bytes of already-expanded guns. A palette update is a run-length stream: read a control byte `t`:
- `t & 0x80` → skip `(t & 0x7f) + 1` entries (leave them unchanged);
- else `t & 0x40` → copy `(t & 0x3f) + 1` entries from `old[off]` (next byte = `off`, entries, not bytes); reject if `off + j > 0x100`;
- else → new entry: R = `smk_pal[t & 0x3f]`, G = `smk_pal[next & 0x3f]`, B = `smk_pal[next2 & 0x3f]`.
Continue until 256 entries are accounted for. `smk_pal[64]` is the standard 6-bit→8-bit table:

```
00 04 08 0C 10 14 18 1C 20 24 28 2C 30 34 38 3C
41 45 49 4D 51 55 59 5D 61 65 69 6D 71 75 79 7D
82 86 8A 8E 92 96 9A 9E A2 A6 AA AE B2 B6 BA BE
C3 C7 CB CF D3 D7 DB DF E3 E7 EB EF F3 F7 FB FF
```

### Video bitstream

Bit order is **LSB-first** within each byte (read bit 0 of the byte first). The stream decodes `(width>>2) * (height>>2)` 4×4 blocks in raster order. For each block, decode `type` from the **type tree**, then:

- `run = block_runs[(type >> 2) & 0x3f]` (table below) — applies to `run` consecutive blocks;
- `type & 3`:
  - `0 MONO` — decode `clr` (mclr tree) and `map` (mmap tree); `hi = clr >> 8`, `lo = clr & 0xff`; for each of the 4 rows: for columns 0..3 pick `hi` if the corresponding `map` bit (bit `4*row+col`) is set else `lo`; `map` is 8 bits (rows 2–3 use 0 ⇒ `lo`).
  - `1 FULL` — for each of the 4 rows, decode two **full-tree** codes: row bytes `[2k] = code & 0xff`, `[2k+1] = code >> 8`.
  - `2 SKIP` — advance `run` blocks, leaving the previous frame's content.
  - `3 FILL` — fill 4×4 with index `(type >> 8) & 0xff`.

`block_runs[64]` = `1,2,…,57,58,59,128,256,512,1024,2048`.

### Trees (in `treesize` bytes, LSB-first)

For each of the four trees in order (mmap, mclr, full, type):
1. one bit: present? if 0, the tree is a single `0` code and its size is skipped;
2. if present, decode a "big tree":
   - two small byte-trees (low, high): for each, one bit present? then a recursive tree of `(byte value, bit-length)` leaves built by: bit=0 ⇒ leaf (next 8 bits = value), bit=1 ⇒ two children; then one skip bit;
   - three 16-bit escape values;
   - decode `(size+3)>>2` values from the big tree using the two byte-trees as sub-codes and the escapes to mark the three "last" slots; then one skip bit;
   - a code value is either a leaf (byte) or a node offset (`0x80000000 | child_index`); traversal consumes one bit per node (1 = right/offset).
3. `last[]` slots default to the current length when unset.

---

## File structure

| File | Responsibility |
|---|---|
| `tools/smk_info.py` (create) | Container parser/inventory; the RE artefact that proves the layout and enumerates the fixed profile. |
| `tools/smk_capture.py` (create) | Turn a captured original frame sequence into a comparable raw form. |
| `tools/smk_compare.py` (create) | Compare port frames against captured frames, pixel-exact; the oracle gate. |
| `port/src/platform/smacker.h` (create) | The decoder's public interface. |
| `port/src/platform/smacker.c` (create) | Container parse, tree decode, block decode, palette. |
| `port/src/game/movie.h` / `.c` (create) | Player glue: open by name, pace, present; ports `0x1C740`/`0x10C30`/`0x11000` case 0. |
| `port/src/platform/res.{c,h}` (modify) | Add `res_load_file` (name-based), reusing the existing scan. |
| `port/src/game/flow.c` (modify) | Call the movie player at the boot sites engine core left stubbed. |
| `port/tests/test_smacker.c` (create) | Decoder unit tests + fixed-profile rejection + oracle compare. |
| `port/tests/test_movie.c` (create) | Player unit tests (open-by-name, pacing, palette plumbing). |
| `port/tests/run_tests.c` (modify) | Register the new suites. |
| `Makefile` (modify) | Add the frame-oracle gate to `verify`. |

---

## Task 0: Branch and baseline

**Files:** none (workflow).

- [ ] **Step 1: Create the branch from current `main`**

```bash
cd /Users/felipe.dos.santos/code/mine/primal-rage-reverse
git status --short          # must be clean
git switch -c smacker-video
git log --oneline -1        # expect aeda053 or 1a35da8
```

- [ ] **Step 2: Confirm the baseline is green**

Run: `make verify`
Expected: build 0 warnings; `all checks passed` twice; oracle `9340 writes byte-exact`; `symbols.h` byte-identical.

- [ ] **Step 3: Commit nothing**

The branch starts at the current `main`; no commit for Task 0.

---

## Task 1: Container parser and fixed-profile inventory

**Files:**
- Create: `tools/smk_info.py`
- Output (git-ignored): `/tmp/smk_inventory.txt` (also recorded in the task report; not committed)

**Interfaces:**
- Produces: the fixed-profile inventory (which tree present-flags are set, which block types occur, keyframe layout) that Task 5's decoder must support, and the pacing evidence for Task 7.

- [ ] **Step 1: Write `tools/smk_info.py`**

```python
#!/usr/bin/env python3
"""Smacker container inventory: proves the layout and enumerates the fixed
profile (tree presence, block types, keyframes). Read-only."""
import struct, sys

SMK_PAL = bytes([
    0x00,0x04,0x08,0x0C,0x10,0x14,0x18,0x1C,0x20,0x24,0x28,0x2C,0x30,0x34,0x38,0x3C,
    0x41,0x45,0x49,0x4D,0x51,0x55,0x59,0x5D,0x61,0x65,0x69,0x6D,0x71,0x75,0x79,0x7D,
    0x82,0x86,0x8A,0x8E,0x92,0x96,0x9A,0x9E,0xA2,0xA6,0xAA,0xAE,0xB2,0xB6,0xBA,0xBE,
    0xC3,0xC7,0xCB,0xCF,0xD3,0xD7,0xDB,0xDF,0xE3,0xE7,0xEB,0xEF,0xF3,0xF7,0xFB,0xFF])

def u32(b, o): return struct.unpack_from('<I', b, o)[0]

def parse(path):
    b = open(path, 'rb').read()
    magic = b[:4]
    w, h, frames, pts, flags = struct.unpack_from('<IIIII', b, 4)
    assert magic in (b'SMK2', b'SMK4'), magic
    treesize = u32(b, 0x34)
    trees = struct.unpack_from('<4I', b, 0x38)          # mmap, mclr, full, type
    audio = [struct.unpack_from('<I', b, 0x48 + 4*i)[0] for i in range(7)]
    tbl = 0x68
    sizes = [u32(b, tbl + 4*i) for i in range(frames)]
    foff = tbl + 4*frames
    fflags = b[foff:foff+frames]
    trees_off = foff + frames
    data = trees_off + treesize
    total = sum(s & ~3 for s in sizes)
    assert data + total == len(b), (data + total, len(b))
    return dict(path=path, w=w, h=h, frames=frames, pts=pts, flags=flags,
                treesize=treesize, trees=trees, audio=audio, sizes=sizes,
                fflags=fflags, tbl=tbl, trees_off=trees_off, data=data)

def main():
    for path in sys.argv[1:]:
        m = parse(path)
        key = [i for i, s in enumerate(m['sizes']) if s & 1]
        pal = [i for i, f in enumerate(m['fflags']) if f & 1]
        aud = [i for i, f in enumerate(m['fflags']) if f & 0xFE]
        print(f"{path}: {m['w']}x{m['h']} frames={m['frames']} pts_inc={m['pts']}")
        print(f"  treesize={m['treesize']} tree_sizes(mmap,mclr,full,type)={m['trees']}")
        print(f"  audio_descriptors={m['audio']}  audio_frame_flags={aud}")
        print(f"  keyframes(frame_size bit0)={key}")
        print(f"  palette_change_frames={len(pal)} first={pal[:8]}")
        print(f"  layout: tbl=0x{m['tbl']:x} trees=0x{m['trees_off']:x} data=0x{m['data']:x} end={m['data']+sum(s & ~3 for s in m['sizes'])}")

if __name__ == '__main__':
    main()
```

- [ ] **Step 2: Run it on both files and record the inventory**

Run: `python3 tools/smk_info.py data/game/C/TWI5.SMK data/game/C/TWG.SMK | tee /tmp/smk_inventory.txt`
Expected: both files `assert` successfully (layout proven); `audio_frame_flags=[]` (silent); record `palette_change_frames` and `keyframes` for Task 5. This is the **fixed profile**: the decoder must support exactly the observed tree-presence bits, block types, and palette-update forms.

- [ ] **Step 3: Settle the pacing open item**

Run the original in DOSBox-X (Task 2 harness) once, and measure the wall-clock duration of `TWG` (41 frames) and `TWI5` (121 frames). Record frames/second. Reconcile against `pts_inc`: `TWI5` −7100 and `TWG` −14200 suggest two different rates; the measured value is the truth and becomes the decoder's `smk_frame_delay_us` contract. Record the decision in the task report.

- [ ] **Step 4: Commit**

```bash
git add tools/smk_info.py
git commit -m "smacker: container inventory tool (layout verified on both movies)"
```

---

## Task 2: Original-frame capture oracle

**Files:**
- Create: `tools/smk_capture.py`
- Create: `tools/smk_compare.py`
- Output (git-ignored): `data/smk-captures/<movie>/frame_%04d.png` or `.raw`

**Interfaces:**
- Produces: `smk_compare.py --capture <dir> --port <dir>` exit 0 iff every frame matches pixel-exact; skips (exit 0 with a message) when the capture is absent; fails when `PR_ORACLE_REQUIRED=1` and the capture is absent — the 2a oracle-gate semantics.

- [ ] **Step 1: Determine a working DOSBox-X capture mechanism and write it into `smk_capture.py`**

Try, in order, and keep the first that yields per-frame images of the logos at 320×200:
1. DOSBox-X video capture (`-c "CAPTURE /V"` or the `capture` config) to AVI, frames extracted with `ffmpeg`.
2. DOSBox-X screenshots (its screenshot hotkey via `-c` autoexec or the `screenshot` command) on an interval.
3. If neither automates frame-indexed capture, run the original, capture the AVI, and index frames by matching against the port's own first-pass output to align indices (documented in the tool).

`tools/smk_capture.py` normalises the chosen artefact into `<dir>/frame_%04d.raw` (320×200 palette indices) plus a `palette.txt`, so comparison is index-based or RGB-based per the capture's fidelity. Record the chosen mechanism and its limits in the tool docstring.

- [ ] **Step 2: Write `tools/smk_compare.py`**

```python
#!/usr/bin/env python3
"""Pixel-exact comparison of port frames against captured original frames.
Absent capture: skip (exit 0) unless PR_ORACLE_REQUIRED=1, then fail.
Usage: smk_compare.py --capture DIR --port DIR [--frames N]"""
import argparse, os, sys

def load_raw(path):
    with open(path, 'rb') as f:
        return f.read()

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--capture', required=True)
    ap.add_argument('--port', required=True)
    ap.add_argument('--frames', type=int, default=0)
    a = ap.parse_args()
    required = os.environ.get('PR_ORACLE_REQUIRED') == '1'
    if not os.path.isdir(a.capture):
        print(f"smk_compare: no capture at {a.capture} "
              f"({'FAIL (required)' if required else 'skipped'})")
        return 1 if required else 0
    n = a.frames or len([f for f in os.listdir(a.capture) if f.endswith('.raw')])
    bad = 0
    for i in range(n):
        cf = os.path.join(a.capture, 'frame_%04d.raw' % i)
        pf = os.path.join(a.port, 'frame_%04d.raw' % i)
        if not (os.path.exists(cf) and os.path.exists(pf)):
            print(f"frame {i}: missing ({cf} or {pf})"); bad += 1; continue
        c, p = load_raw(cf), load_raw(pf)
        if len(c) != len(p) or c != p:
            d = next((k for k in range(min(len(c), len(p))) if c[k] != p[k]), None)
            print(f"frame {i}: MISMATCH len {len(c)}/{len(p)} first diff at {d}")
            bad += 1
    print(f"smk_compare: {n - bad}/{n} frames match")
    return 1 if bad else 0

if __name__ == '__main__':
    sys.exit(main())
```

- [ ] **Step 3: Prove the tool can fail (negative control)**

Corrupt one captured frame copy, run `smk_compare.py` against it, and confirm it exits 1 with a first-diff report. Restore.

- [ ] **Step 4: Commit**

```bash
git add tools/smk_capture.py tools/smk_compare.py
git commit -m "smacker: frame-capture oracle tooling"
```

---

## Task 3: Decoder interface and container validation

**Files:**
- Create: `port/src/platform/smacker.h`
- Create: `port/src/platform/smacker.c`
- Create: `port/tests/test_smacker.c`
- Modify: `port/tests/run_tests.c` (register `test_smacker`)
- Modify: `port/CMakeLists.txt` (add the sources; follow how `platform/audio/*` is listed)

**Interfaces:**
- Consumes: nothing.
- Produces (exact, later tasks rely on these):

```c
/* smacker.h */
#define SMK_TREE_WORDS 65536
typedef struct SmkMovie {
    const u8 *data; u32 len;
    u32 width, height, frames, frame_delay_us;
    u32 table_off, flags_off, trees_off, data_off, treesize;
    u32 tree_size[4];            /* header order: mmap, mclr, full, type */
    s32 *tree[4];                /* into `words` */
    s32 *last[4];                /* into `words` */
    u8  pal[768];                /* current 256-entry RGB table */
    u32 next_frame;              /* frame cursor */
    s32 words[SMK_TREE_WORDS];   /* tree value arena */
} SmkMovie;

int  smk_open(const u8 *data, u32 len, SmkMovie *out);  /* 0 = reject */
u32  smk_width(const SmkMovie *m);
u32  smk_height(const SmkMovie *m);
u32  smk_frames(const SmkMovie *m);
u32  smk_frame_delay_us(const SmkMovie *m);
int  smk_decode_frame(SmkMovie *m, u8 *frame);          /* 1 ok, 0 reject */
void smk_palette_to(const SmkMovie *m, u8 dac[256][3]);
```

`SmkMovie` is caller-owned, so its definition is exposed in `smacker.h` (like `SampleVoice`), not hidden. `movie.c` owns one `static SmkMovie`.

- [ ] **Step 1: Write the failing test** (`port/tests/test_smacker.c`)

```c
#include "test.h"
#include "platform/smacker.h"

int test_smacker(void)
{
    int before = g_failures;
    const char *dir = getenv("PR_GAME_DIR");
    if (dir == NULL) { printf("test_smacker: PR_GAME_DIR unset, skipping\n"); return 0; }

    static SmkMovie m;
    char path[512];
    snprintf(path, sizeof(path), "%s/TWI5.SMK", dir);   /* on-disk name is uppercase */
    FILE *f = fopen(path, "rb");
    CHECK(f != NULL, "TWI5.SMK opens");
    if (f == NULL) return g_failures - before;
    static u8 data[2 << 20];
    size_t sz = fread(data, 1, sizeof(data), f);
    fclose(f);
    CHECK(sz > 0, "TWI5.SMK reads");

    CHECK(smk_open(data, (u32)sz, &m), "smk_open accepts TWI5.SMK");
    CHECK_EQ_INT(smk_width(&m), 320);
    CHECK_EQ_INT(smk_height(&m), 200);
    CHECK_EQ_INT(smk_frames(&m), 121);

    /* Fixed profile: a truncated header, a bad magic, and a size that runs
     * past the buffer are all rejected. */
    CHECK_EQ_INT(smk_open(data, 8, &m), 0);
    u8 save = data[0]; data[0] = 'X';
    CHECK_EQ_INT(smk_open(data, (u32)sz, &m), 0);
    data[0] = save;
    return g_failures - before;
}
```

- [ ] **Step 2: Run it and watch it fail**

Run: `PR_GAME_DIR=data/game/C make test 2>&1 | tail`
Expected: link/compile failure (`smk_open` undefined).

- [ ] **Step 3: Implement `smk_open` header validation**

Parse the header, then validate: magic, `width`/`height` non-zero and ≤ 32768, `frames` ≤ 0xFFFFFF and non-zero, `treesize` and `frame_size[i]` within the buffer, table/flags/trees/data offsets as in the Format reference, and `data_off + sum(frame_size & ~3) == len`. Reject with a specific reason (write it to `stderr`) on any violation. Copy the four tree sizes; do not yet decode trees.

- [ ] **Step 4: Run the tests**

Run: `PR_GAME_DIR=data/game/C make test 2>&1 | tail`
Expected: `all checks passed` (with `PR_GAME_DIR` set) and skipped without it.

- [ ] **Step 5: Verify the full ladder**

Run: `make verify`
Expected: green (the new suite skips without `PR_GAME_DIR`, so `verify` must export it — add `PR_GAME_DIR=$(GAME_DIR)` to the `test` target so the decoder suites run under `verify`).

- [ ] **Step 6: Commit**

```bash
git add port/src/platform/smacker.h port/src/platform/smacker.c port/tests/test_smacker.c port/tests/run_tests.c port/CMakeLists.txt Makefile
git commit -m "smacker: container parse and validation"
```

---

## Task 4: Tree bitstream decode

**Files:**
- Modify: `port/src/platform/smacker.c` (`smk_open` gains tree decode)
- Modify: `port/tests/test_smacker.c`

**Interfaces:**
- Produces: `m.tree[4]` / `m.last[4]` (into `m.words`) populated by `smk_open`, and an internal `smk_get_code()` used by Task 5.

- [ ] **Step 1: Add the failing assertion to `test_smacker.c`**

```c
    /* The largest tree (FULL, 109136 -> 27285 values) must fit the arena and
     * decode without error; a movie whose tree sizes exceed SMK_TREE_WORDS is
     * rejected. */
    CHECK_EQ_INT(smk_open(data, sz, &m), 1);
    CHECK(m.tree[0] != NULL && m.tree[3] != NULL, "all four trees decoded");
```

- [ ] **Step 2: Run and watch it fail** — the fields do not exist yet.

- [ ] **Step 3: Implement the LSB-first bit reader and tree decode**

Implement in `smacker.c` (original code, per the Format reference):
- a bit reader with explicit `left()` checks and a sticky error flag;
- the small byte-tree builder recording each leaf's `(value, length, path)` in read order, plus a decode that consumes one bit at a time and matches by `(length, path)`;
- the big-tree decoder producing `NODE | left_subtree_size` nodes in preorder with three escape-marked `last` slots, matching the header's `(size+3)>>2` value count;
- the four present-flags and per-tree `skip` bits.

Reject on: any read past the trees bitstream, value count exceeding the arena, or recursion depth > 27 (small) / 500 (big).

- [ ] **Step 4: Run the tests** — `PR_GAME_DIR=data/game/C make test`; expect pass, and confirm the decoded arena offsets are within `words`.
- [ ] **Step 5: Negative control** — temporarily allow the value count to exceed the arena and confirm the reject fires (test must fail); revert.
- [ ] **Step 6: `make verify` then commit**

```bash
git commit -am "smacker: LSB-first tree bitstream decode"
```

---

## Task 5: Block decode, palette, and `smk_decode_frame`

**Files:**
- Modify: `port/src/platform/smacker.c`
- Modify: `port/tests/test_smacker.c`

**Interfaces:**
- Produces: `smk_decode_frame(m, frame)` and `smk_palette_to(m, dac)` as specified in Task 3.

- [ ] **Step 1: Add the failing per-frame assertions**

```c
    /* Decode every frame; each must succeed, stay in bounds, and the palette
     * must be a valid 256-entry table. */
    static u8 frame[320 * 200];
    static u8 dac[256][3];
    for (u32 i = 0; i < smk_frames(&m); i++)
        CHECK(smk_decode_frame(&m, frame), "frame decodes");
    smk_palette_to(&m, dac);
    int nonzero = 0;
    for (int c = 0; c < 256; c++)
        for (int k = 0; k < 3; k++) nonzero += dac[c][k] != 0;
    CHECK(nonzero > 0, "palette is populated");

    /* Past the last frame the decoder must reject, never re-read. */
    CHECK_EQ_INT(smk_decode_frame(&m, frame), 0);
```

- [ ] **Step 2: Run and watch it fail.**

- [ ] **Step 3: Implement**

- palette update parser (skip/copy/new) over the frame payload when `frame_flags & 1`, rejecting an out-of-range copy offset;
- the block loop: `type` from the type tree, `run` from `block_runs`, `MONO`/`FULL`/`SKIP`/`FILL` per the Format reference, writing into the caller's `frame` (the previous frame is already there, so `SKIP` and deltas work in place);
- bounds: every block write stays inside `width*height`, every code read is checked;
- `smk_palette_to` copies `m.pal` into `dac`.

- [ ] **Step 4: Run the tests** — `PR_GAME_DIR=data/game/C make test`; expect `all checks passed`.

- [ ] **Step 5: Add a `PR_SMK_DUMP=<dir>` dump path to the test and compare against the capture** — when the variable is set, the test decodes the movie and writes `frame_%04d.raw` (320×200 bytes) to the directory. Run `PR_SMK_DUMP=/tmp/port_twg PR_GAME_DIR=data/game/C make test`, then `python3 tools/smk_compare.py --capture data/smk-captures/twg --port /tmp/port_twg`. Expect exact match; if the palette model differs, resolve before proceeding (record the finding).

- [ ] **Step 6: Negative control** — strip the `run` bound so a run can exceed the block count; confirm the test fails; revert.

- [ ] **Step 7: `make verify` then commit**

```bash
git commit -am "smacker: block decode and palette; frames match the capture"
```

---

## Task 6: Name-based file loader in `res`

**Files:**
- Modify: `port/src/platform/res.h`, `port/src/platform/res.c`
- Modify: `port/tests/test_res.c` (or wherever `res_*` is tested)

**Interfaces:**
- Produces: `int res_load_file(const char *game_dir, const char *name, u32 *out_off, u32 *out_size);` returning 1/0, writing a `mem[]` offset and byte count. Reuses the existing case-insensitive scan; no duplicated directory logic.

- [ ] **Step 1: Write the failing test** — load `"twi5.smk"` (lowercase, file is uppercase) and assert size 1208576; load `"nope.smk"` and assert 0 and untouched outputs.
- [ ] **Step 2: Run and watch it fail.**
- [ ] **Step 3: Refactor `res_open` to a name-based core and add `res_load_file`** — read the whole file into `mem[]` through the existing bump allocator, reject if it does not fit.
- [ ] **Step 4: Run; `make verify`; commit**

```bash
git commit -am "res: name-based file loader (movies are not in INDEX)"
```

---

## Task 7: Movie player and boot wiring

**Files:**
- Create: `port/src/game/movie.h`, `port/src/game/movie.c`
- Modify: `port/src/game/flow.c` (the `0x11000` / `0x1C740` sites)
- Create: `port/tests/test_movie.c`
- Modify: `port/tests/run_tests.c`, `port/CMakeLists.txt`

**Interfaces:**
- Consumes: `smk_*`, `res_load_file`, `gfx_dac`, `gfx_present`, `swap_buffers`, `host_tick_count`, `host_wait_vblank`.
- Produces: `int movie_play(const char *game_dir, const char *name);` returning 1 when the movie played to the end (or was skipped), 0 only on a hard failure that the caller should report. It validates `game_dir`/`name`, loads the file, opens it, loops frames at `smk_frame_delay_us` converted to host ticks, writes `smk_palette_to(m, gfx_dac)` each frame, and returns on end/ESC.

- [ ] **Step 1: Write the failing test** (`test_movie.c`) — with `PR_GAME_DIR` set, assert `movie_play` returns 1 for `twi5.smk`; assert it returns 1 (skip) for a missing name, and emits a message rather than crashing.
- [ ] **Step 2: Run and watch it fail.**
- [ ] **Step 3: Implement `movie.c`**; the headless `--check` path must not open a window, so `movie_play` renders through the same present seam the loop uses.
- [ ] **Step 4: Wire the boot order** — call `movie_play(game_dir, "twi5.smk")` then `movie_play(game_dir, "twg.smk")` at the site `FUN_00011000` case 0 reaches (`0x1C740` twice), replacing the engine-core sub-project-2 stub.
- [ ] **Step 5: Run `--check`** — `./build/prageport --game-dir data/game/C --check 120` exits 0 and the headless frame gate still passes; confirm no `mem[0xA0000]` write (the aperture rule) by inspection.
- [ ] **Step 6: `make verify`; commit**

```bash
git commit -am "movie: play the Smacker logos on the boot path"
```

---

## Task 8: Frame-oracle gate in `make verify`

**Files:**
- Modify: `Makefile` (add an `smk-oracle` stage before/after the existing oracle stage)
- Modify: `port/tests/test_smacker.c` (an end-to-end frame dump when `PR_SMK_DUMP` is set)

**Interfaces:**
- Consumes: the Task 2 tools and capture.
- Produces: a `verify` stage with the same semantics as the audio oracle — capture absent ⇒ skip, `PR_ORACLE_REQUIRED=1` ⇒ fail.

- [ ] **Step 1: Extend the Task 5 `PR_SMK_DUMP` path to cover both movies** (it currently dumps the single movie the test drives).
- [ ] **Step 2: Generate and compare both** — `PR_SMK_DUMP=/tmp/port_pr PR_GAME_DIR=data/game/C make test`, then `python3 tools/smk_compare.py --capture data/smk-captures/twi5 --port /tmp/port_pr` and the same for `twg`.
- [ ] **Step 3: Wire into `verify`** with the skip/required semantics, and add the capture directory to `.gitignore`.
- [ ] **Step 4: Negative control** — flip one byte in a captured frame, confirm `verify` fails, revert.
- [ ] **Step 5: `make verify`; commit**

```bash
git commit -am "smacker: pixel-exact frame oracle in make verify"
```

---

## Task 9: Docs and sub-project report

**Files:**
- Create: `docs/superpowers/plans/2026-09-17-smacker-video-report.md`
- Modify: `README.md`, `FORMATS.md`, `port/RE_GUIDE.md`, `port/spec/game_flow.md` (the Smacker lines), and the engine-core report's sub-project-2 entry (mark it superseded, as 2a did for sub-project 3)

**Interfaces:**
- Consumes: every prior task.
- Produces: the report recording, in the 2a shape — what is proven (frames pixel-exact against the capture; pacing; boot order), what is not (streamed audio is a separate plan), every `/* PORT: */` deviation, every `/* TODO(verify): */`, the fixed-profile inventory, and the verification ladder verbatim.

- [ ] **Step 1: Write the report** and update the docs above.
- [ ] **Step 2: Reproduce the ladder** — clean out-of-tree build 0 warnings; `PR_ORACLE_REQUIRED=1 make verify` green; `symbols.h` byte-identical; the frame oracle reporting N/N frames.
- [ ] **Step 3: Commit**, then move to the **2b-ii (streamed audio)** plan, which this plan deliberately does not cover.

```bash
git commit -am "docs: smacker video cycle report and updated status"
```

---

## Self-review notes (for the implementer)

- **Spec coverage:** Video decode (Tasks 3–5), player + boot order (Task 7), oracle (Tasks 2, 8), `res_load_file` (Task 6), docs (Task 9). Streamed audio is intentionally absent — it is plan 2b-ii.
- **Open items carried from the spec:** pacing (Task 1 Step 3), fixed-profile inventory (Task 1 Step 2), open-by-name call site (Task 6/7), palette semantics (Task 5 Step 5), `0x1C740` shared path (Task 7 Step 5).
- **Do not guess:** if the tree/block decode disagrees with the capture, stop and fix the decoder — never tune the comparison, never approximate.

