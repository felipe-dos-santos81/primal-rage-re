# Actor System and Title (sub-project 4a-ii) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Port Primal Rage's actor system — the 0x68-byte actor pool, `0x2AE14` spawn, the pset sync, the animation-stream interpreter and the real `0x121A0` title state — and prove the title composite pixel-exact against a pinned capture of the original.

**Architecture:** Two new units. `game/rng.{c,h}` owns the `0x5D7DC` LCG. `game/actors.{c,h}` owns the pool, its free/active lists, spawn, the state-begin reset, the pset sync and the animation interpreter; it never touches the framebuffer and never sorts the display list (4a-i owns both). `game/flow.c` gains a transcription of `0x121A0` in place of the fake full-screen title, and calls `actors_update()` and one RNG advance where the original's master loop does. The oracle is a capture-only patched copy of `PRAGE.EXE` plus a new `make title-oracle`.

**Tech Stack:** C99, the repo's `mem[]` model, `platform/res` handles, the `port/tests` harness (`test.h` + `run_tests.c`), Python 3 + DOSBox-X video capture + ffmpeg (already used by `tools/smk_capture.py`).

## Global Constraints

These apply to every task. They are the spec's project-wide rules, copied verbatim.

- **Never write `mem[0xA0000]`.** It is data-object offset `0x20000` (live pointer tables), not the VGA aperture. The port composites into `mem + DSD(DS_000E87A4)` and presents through `gfx_present`.
- **No SDL outside `port/src/host.c` and `port/src/main.c`.**
- **`data/` is read-only.** Never write a file under `data/`. The patched binary lives in a temporary directory.
- **Comments only** `/* PORT: ... */` (a deviation from the original) and `/* TODO(verify): ... */` (an unproven doubt). No other comment conventions in `port/src` implementation code.
- **No new dependencies.** C99 + the existing tree + the Python tools already present.
- **Original engine state lives in `mem[]`**; pointer-valued globals store `mem[]` offsets, consumed as `mem + DSD(...)`.
- **Comparisons have no tolerance.** A pixel mismatch is a failure. No tolerance parameter exists.
- **Build must produce zero warnings**, and `make verify` must stay green, at the end of every task.
- **Test harness:** `port/tests/test.h` + `run_tests.c`. Conventions, verified: exactly one `int test_X(void)` entry point per test file, declared in `test.h`, called from `run_tests.c`; the only assertion macros are `CHECK(cond, msg)` and `CHECK_EQ_INT(a, b)`; and `port/CMakeLists.txt` lists test sources and core sources **explicitly** (no globbing), so a new file must be added to both `add_library(prage_core ...)` (line 13) and `add_executable(run_tests ...)` (line 39).
- **Commit style:** `<area>: <what changed>` in the imperative, lowercase area (e.g. `actors: pool alloc and free`).
- **Build/test invocations:** `make`, `make test` (needs `PR_GAME_DIR=data/game/C`), `make verify`, `make check`.
- **No `git add -A` in this worktree.** Other plans commit to the same branch.

---

## Format reference (derived from `PRAGE.EXE`; verified by disassembly)

Every fact below was established by instruction-level disassembly or from the
fixup-applied image, and the decompilation line ranges are the transcription
loci. Addresses are DOS/4GW virtual.

### A. The RNG — `0x5D7DC` (`prage.c:38892`), verified byte-for-byte

Disassembly at file offset `0xB0630` (42 bytes, Ghidra's size):

```
53              push ebx
52              push edx
25 ff ff 00 00  and  eax, 0xffff        ; range & 0xffff
8b d8           mov  ebx, eax
a1 d8 f6 06 00  mov  eax, [0x0006f6d8]  ; DS_000EF6D8 - 0x80000
ba b9 12 0d b9  mov  edx, 0xB90D12B9    ; -0x46F2ED47
f7 e2           mul  edx                ; edx:eax = eax * edx, low in eax
05 1f 05 ce 38  add  eax, 0x38CE051F
a3 d8 f6 06 00  mov  [0x0006f6d8], eax
c1 e8 10        shr  eax, 0x10
f7 e3           mul  ebx
c1 e8 10        shr  eax, 0x10
5a 5b c3        pop edx; pop ebx; ret
```

So: `state = state * 0xB90D12B9 + 0x38CE051F` (u32 wrap), return
`((state >> 16) * (range & 0xffff)) >> 16`. Seed `0xABCD`, hardcoded at
`0x20C10` (`prage.c:11428`).

**The object-0 code mapping is `file = va + 0x52E54`**, verified three ways:
`0x5D7DC + 0x52E54 = 0xB0630`; the byte at `0xB062F` is the previous function's
`ret` (`c3`); and the 42-byte body ends `5a 5b c3` (`pop edx; pop ebx; ret`),
matching the `53 52` prologue. A neighbouring function confirms it:
`0x5D808 + 0x52E54 = 0xB065C`. Every file offset in this plan is that conversion.

Expected sequence from seed `0xABCD` (consecutive calls, each row one call):

| range | returned |
|---|---|
| `0x5A` | 12 |
| `0x7E` | 111 |
| `2` | 0 |
| `0xFFFF` | 29617 |
| `0x10000` | 0 (masked to 0) |
| `0` | 0 (no undefined shift) |
| `0x7FFFFFFF` | 11010 (masked to `0xFFFF`) |

### A2. The pin — the three title draws are patched to immediates

`0x121A0` draws three values on entry and uses them for the logo's start X, speed
and gravity. Pin them by replacing each `call 0x5D7DC` (5 bytes, `e8 rel32`) with
`mov eax, imm32` (5 bytes, `b8 imm32`) **in place**: same length, no code cave, no
relocation, and the call is simply not taken.

| file offset | VA | range (preceding `mov eax, imm`) | patched bytes | value |
|---|---|---|---|---|
| `0x650E9` | `0x12295` | `0x5A` (at `0x650E4`) | `b8 0c 00 00 00` | 12 |
| `0x650F5` | `0x122A1` | `0x7E` (at `0x650F0`) | `b8 6f 00 00 00` | 111 |
| `0x6510B` | `0x122B7` | `2` (at `0x65106`) | `b8 00 00 00 00` | 0 |
| `0x7E289` | `0x2B435` | the anim-stream opcode-8 handler's own range | `b8 00 00 00 00` | 0 |

Those first three are the **only** `0x5D7DC` call sites inside `0x121A0` (verified by
scanning every one of the 122 `e8` sites in the image that target `0x5D7DC`).

The fourth is required and was found only by measurement: `FUN_0002B2A0`'s opcode 8
(`0x2B435`, inside the animation-stream interpreter) reads `rand(range)` into the
record's float frame timer `rec+0x20`, and the second title object's stream
(`0x0E897A`, descriptor `0x9AC94`) contains ten opcode-8 words in its first `0x600`
bytes. So the **RNG state at title entry is consumed in-window**, and the value of
that state is exactly what the master loop's unbounded, host-timed spin
(`while (DS_0010150C - 1 == DS_00101508) 0x5D7DC();`) makes run-to-run variable. Two
identical captures shared only 4 of 110 window frames until this site was pinned.

An optional belt-and-braces site, to be applied only if the gate still shows drift:
`0x7852A` (`e8 01 81 03 00` -> `90 90 90 90 90`) NOPs the spin's call, making the
original's per-iteration draw count one, as the port's `rng_step()` already is. It is
not needed once `0x7E289` is pinned, because opcode 8 is the only in-window consumer.

**Why not a constant-returning stub of `0x5D7DC`** (the plan's first draft): it
zeroes the three entry draws, so the logo's start X, speed *and* gravity are 0 and
the logo never moves — the 96-frame window would compare a near-static image. The
correct rule is narrower: pin the draws whose **values are consumed**, and let the
rest of the LCG run for real.

Resulting title state: `iVar1 = 12`, `iVar2 = 111 * 0x40 + 0x280 = 7744` (`0x1E40`,
not negated because `iVar3 = 0`), `DS_00107A50 = 0x2420`, `DS_00107A3A = 0x121`,
`logo+0x34 = -81`, `logo+0x36 = 8`, `logo+0x2C = 0xAA`.

### B. Actor record — 0x68 bytes, pool base `DS_001014F4`

Only the fields this cycle touches are named; the rest are transcribed as they
are reached. Offsets used by the tasks below:

| off | width | meaning |
|---|---|---|
| `+0x08` | u32 | script/animation-stream pointer (desc `+0x00`) |
| `+0x10` | u32 | stream base for the `0x4F` table lookup (`0x2AA70`) |
| `+0x18` | i32 | world x (pset source) |
| `+0x1C` | i32 | world y |
| `+0x20`, `+0x24` | float | frame timer and its increment |
| `+0x28` | u16 | flags (`0x08` dead bit, `0x40`, `0x80`, `0x200`, `0x400`, high-byte `0x0100`/`0x0400`/`0x0800`/`0x1000`/`0x2000`/`0x4000`) |
| `+0x2A` | u16 | high-byte op flags (`0x08` in high byte ⇒ draw the engine's own id) |
| `+0x2B` | u8 | visibility bits; `\|= 0x18` when the record is on-screen |
| `+0x2C` | u16 | pset `+0x0C` copy |
| `+0x2E` | u16 | pset/anim id |
| `+0x30` | i32 | pset `+0x02` alternate source |
| `+0x32` | i16 | x velocity (`>>16` is the integer step) |
| `+0x34` | i16 | y velocity |
| `+0x36` | i16 | gravity |
| `+0x38` | i16 | gravity accumulator step |
| `+0x3C` | u32 | last pset x (written by `0x2A690`/`0x2A820`) |
| `+0x40` | u16 | extent, used for the on-screen test in `0x2A820` |
| `+0x42` | u8 | bounce/limit accumulator |
| `+0x43`, `+0x44` | u8 / i16 | fall-limiter count and step |
| `+0x46` | u16 | mode-1 shear table entry cache (`0x2A690`) |
| `+0x48` | u8 | actor type id, indexes the per-type render table `DS_000BB9DC` (stride `0xC`) |
| `+0x49` | u8 | pset layer accumulator |
| `+0x4A` | u8 | parent slot (record index) |
| `+0x4B` | u8 | child slot (record index) |
| `+0x4E` | u8 | pset-id hold counter; when it hits 0 the pset id switches to `+0x30` |
| `+0x4F` | u32 | child ref-count (`>>24` indexes the stream byte table) |
| `+0x51` | u8 | animation sequence cursor mod 0x40 (stream byte index) |
| `+0x52..0x55` | u8×4 | four one-byte anim variables |
| `+0x56` | u16 | pset slot index (allocated by spawn) |
| `+0x59` | i8 | pset sort-bias added to the layer |
| `+0x5A` | u8 | pset layer base (spawn descriptor `+0x41`, byte) |
| `+0x5F` | u8 | nonzero ⇒ pset `+0x02` ORs `0x800` (hflip) |
| `+0x60`, `+0x61` | u8 | mode-1 flags; `+0x61` selects the shear ramp entry |

Write order in `0x2AE14` is listed in the spawn task.

### C. The pool and its two lists

* Pool: `DS_001014F4`, `0xEBA0` bytes = 580 records of `0x68`. **Already
  allocated** by `res_load_index` (`port/src/platform/res.c:121`), as is the
  pset pool `DS_001014EC` (`0x4880` = 580 × `0x20`, `res.c:120`). Both are the
  original's own immediates.
* Free list: sentinels `DS_00105B3C`/`DS_00105B40`, each `{next@+0; prev@+4}`.
* Active list: sentinels `DS_00105BCC`/`DS_00105BD0`, same shape.
* Splice helpers: `0x249B0` (link at head of its list), `0x249C0` (push to
  free-list head), `0x249D0` (pop free-list head) — all `prage.c:11867-11930`.
* Pool index of a record is `(off - DSD(DS_001014F4)) / 0x68`, and `+0x56`
  caches it as a u16.

### D. Spawn descriptor (`0x2AE14`, `prage.c:15885-16048`)

| desc off | width | record field |
|---|---|---|
| `+0x00` | dword | `+0x08` script/stream pointer |
| `+0x04` | byte | `+0x48` actor type id (indexes `DS_000BB9DC`) |
| `+0x05` | byte | `+0x20` and `+0x24` as float, then `+0x20 -= 1.0` if nonzero |
| `+0x06` | word | `+0x2E` pset/anim id |
| `+0x08` | word | `+0x28` flags, masked `0xFFC3` |
| `+0x0A` | word | `+0x40` (`<< 6`) |
| `+0x0C` | word | `+0x2C` |
| `+0x10` | dword | nonzero ⇒ `0x33754()` supplies the pset `+0x18` palette |

Title descriptors (`0x9AC30`, `0x9AC94`, `0x9AC1C`, `0x9ACA8`) are in the spec
§4.2 and are re-read from the data object; `0x9AC30` and `0x9AC94` carry the
palette handles `0x4197C6C` and `0x419776C` (resource 8 = `S16ATTRC.GRA`).

### E. pset — 0x20 bytes, base `DS_001014EC`, slot index from `rec+0x56`

| off | width | meaning |
|---|---|---|
| `+0x00` | u16 | sprite id (`0x1E1` when the per-type check returned 2) |
| `+0x02` | u16 | `rec+0x2E \| (rec+0x5F ? 0x800 : 0)` |
| `+0x04` | i32 | x |
| `+0x08` | i32 | y |
| `+0x0C` | u16 | copied from `rec+0x2C` |
| `+0x0E` | u16 | layer |
| `+0x18` | u32 | palette handle |

`0x2A148` writes `+0x02` from `rec+0x2E`; `0x2A17C` writes it from its argument
and manages the `+0x18` palette through `0x33864`/`0x33754` (the port's
`palette_record`/dirty-list path).

### F. Animation stream encoding (`0x29DB8`, `0x29F34`, `0x2A408`)

Two readers over one stream:

* **Literal reader** `0x2A408` (`prage.c:15429`): if the record flag
  `+0x28 >> 8 & 8` is set the id comes from `rec+0x08` itself; else the stream
  word at `rec+0x08` is read and the pointer advances. `word & 0x8000` selects a
  computed id, dispatched on `(word & 0x1F00) == 0xD00`:
  * `(word >> 8 & 0x60) == 0x40` ⇒ one extra word consumed, id =
    `0x29F34(rec, word & 0x7F)` plus the next stream word;
  * otherwise two extra words consumed, id =
    `table[0x29F34(rec, word & 0x7F)]` (the `0x4F` table).
  The final hflip bit is `(id & 0x8000) != 0` XOR `(rec+0x28 >> 8 & 0x40) != 0`.
* **Variable reader** `0x29F34` (`prage.c:15179`): `byte & 0x7F` selects the
  source — `< 0x40` ⇒ `DS_00105B4C[(byte + rec+0x51) & 0x3F]` (the 0x40-entry
  word ring); `0x40..0x45` ⇒ `rec+0x52..0x58` (sign-extended for `0x40..0x43`,
  `0x45`); `0x46..0x4B` ⇒ the same fields on the record at slot `rec+0x4A`;
  `0x4C..0x51` ⇒ the same fields on the record at slot `rec+0x4B`.
* **Writer** `0x29DB8` is the mirror image of `0x29F34` plus the ring, and is
  how the stream's opcodes store values. Neither reader nor writer is reached
  with a computed index by the title's first two objects unless the stream asks
  for it; transcribe both (they are 306 and 303 bytes) rather than stubbing.

Stream pointer arithmetic in the record: `rec+0x08` is advanced word-wise by
`0x2A408` and by `0x2BC30`/`0x2BCF4` (the anim-entry helpers).

### G. The title state `0x121A0` (`prage.c:1392-1493`)

`DAT_000F0A6F` phases. All immediates below are the port's transcription target;
`DS_000F0A58` is the logo record, `DS_000F0A54` the second object.

Phase 0: `0x2C3FC(0x41)`, `0x2C3FC(0x43)` (case-5 voice cancels), `0x4F1E4()`,
`0x2BAF4()`, `0x38910()`; mode-1 branch on `DAT_00104528[1] & 2` — clear ⇒
`0x1C500()` + `0x2F198()`, set ⇒ `0x2AE14(0x9AE3C)`; four
`0x38B18(0x9AC1C)` with `(edx,ebx)` = `(0,0)`, `(0x2A,0)`, `(0,0x1E)`,
`(0x2A,0x1E)`; three `0x5D7DC` draws with ranges `0x5A`, `0x7E`, `2`, the third
negating `iVar2 = iVar2*0x40 + 0x280` when nonzero; `DS_00107A50 = iVar2/2 +
0x1500`; `DS_000F0A58 = 0x2AE14(0x9AC30)`, `DS_000F0A66 = 0x600`, then on that
record: `+0x34 = -iVar2/0x5F`, `+0x2C = 0xAA`, `+0x36 = (iVar1 << 6)/0x5F`;
`DS_000F0A54 = 0x2AE14(0x9AC94)`; `DS_000F0A6F++`.

Phase `DS_000F0A6F < 2`: `DS_000F0A66 -= 0x10`; if `< 0x11` ⇒ `0x1C500()`,
`0x2F280()`, `0x2B150(logo)`, `0x2B150(second)`, `DS_000F0A54 = 0x2AE14(0x9ACA8)`,
then the `0x33904` walk (`prage.c:20878`) calling `0x13C70` (`prage.c:2697`) on
records whose first dword is `&DAT_0003E688`, and `DS_000F0A6F++`; then
`DS_00107A50 += (i16)((logo+0x32 >> 16) / 2)`; `logo+0x2C = 0x40000 /
DS_000F0A66`.

Exit (`DS_000F0A6F == 2 && DS_0009AF3D == 0`): `DS_000F0A6F = 0`,
`DS_000F0A64 = 2`. Always: `DS_00107A3A = (i16)(DS_00107A50 >> 5)`.

With the pin (Task 1) all three draws are fixed, so `iVar1 = 12`,
`iVar2 = 7744` (`0x1E40`, not negated because `iVar3 = 0`),
`DS_00107A50 = 0x2420`, `DS_00107A3A = 0x121`,
`logo+0x34 = -81`, `logo+0x36 = 8`, `logo+0x2C = 0xAA`. The non-zero `iVar1` is
what makes the logo **move** (gravity `8` per frame), which is why this window
exercises the anim interpreter and the pset sync at all.

### H. pset layer select and support (`0x2F0F0`, `0x2F198`, `0x2F280`, `0x2F4BC`)

`0x2F0F0` (`prage.c:18923`, 105 B) selects the pset cursor; `0x2F198`
(`prage.c:18974`, 115 B) clears/advances the pset layer; `0x2F280`
(`prage.c:19028`, 146 B) writes the layer set; `0x2F4BC` (`prage.c:19185`,
20 B) is a one-liner. All four operate on `DS_001014EC` slots through
`DS_00105Bxx` cursors. Transcribe them from their loci.

### I. Master-loop ordering (`0x255CC`, `prage.c:~12040`)

Verified body: `0x500C4` → `0x292AC` → optional `0x389C4`/`0x38A38` →
`0x24C5C` (whose tail calls `0x2A31C`) → the render table at
`PTR_FUN_000a86c4` gated on `_DAT_00104AEC` → `DAT_00104AF4++` → `0x134C0` →
**conditional on `DS_0010150C == DS_00101508`**: `0x1C3FC` (sort), `0x14328`
(composite, when `DS_001088F4 == 0`), `0x1C470` (palette), then the full copy or
`0x501A3`, then `0x50188` (swap) → **`0x5D7DC` once** → `0x1CF20` (audio) →
`DS_0010150C++` → `while (DS_0010150C - 1 == DS_00101508) 0x5D7DC();`.

So in the port: `actors_update()` goes at the **end of `game_frame()`**, and
`rng_step()` goes in `game_loop()` **after `swap_buffers()` and before
`game_audio_service()`**.

`DAT_00105BEC` decrements once per `0x2A31C` and wraps to `DS_00105BEE - 1`.

### J. Decompilation index (transcription loci)

| function | `port/decomp/prage.c` |
|---|---|
| `0x13C70` | 2697 |
| `0x1C500` | 8005 |
| `0x249B0` / `0x249C0` / `0x249D0` | 11867 / 11884 / 11901 |
| `0x29DB8` | 15090 |
| `0x29F34` | 15179 |
| `0x2A148` | 15251 |
| `0x2A17C` | 15272 |
| `0x2A1FC` | 15308 |
| `0x2A31C` | 15372 |
| `0x2A39C` | 15404 |
| `0x2A408` | 15429 |
| `0x2A4FC` | 15474 |
| `0x2A620` | 15544 |
| `0x2A690` | 15570 |
| `0x2A820` | 15642 |
| `0x2AA70` | 15723 |
| `0x2AC80` | 15817 |
| `0x2AD40` | 15849 |
| `0x2AE14` | 15885 |
| `0x2B150` | 16049 |
| `0x2BAF4` | 16432 |
| `0x2B2A0` (anim opcode dispatcher) | 16077 |
| `0x2BC30` / `0x2BCF4` | 16501 / 16545 |
| `0x2BF08` | 16666 |
| `0x2EA30` | 18424 |
| `0x2F0F0` | 18923 |
| `0x2F198` | 18974 |
| `0x2F280` | 19028 |
| `0x33754` | 20751 |
| `0x33904` | 20878 |
| `0x38910` | 23689 |
| `0x38B18` | 23798 |
| `0x4F1E4` | 35700 |
| `0x5D7DC` | 38892 |

---

### Task 1: The pin — a three-site capture copy

**Files:**
- Create: `tools/title_pin.py`
- Create: `tools/tests/test_title_pin.py`
- Modify: `Makefile` (new target `title-pin`, `.PHONY` list)

**Interfaces:**
- Consumes: nothing.
- Produces: `title_pin.py --src <exe> --out <exe>`; the copy has the three
  `0x5D7DC` calls inside `0x121A0` replaced by `mov eax, imm32` (Format reference
  A2) and is otherwise byte-identical. The port mirrors it by seeding `0xABCD` at
  title entry and taking the three real draws (Tasks 3 and 9).

The pin fixes the title's three entry draws to the values the port's own LCG
produces from seed `0xABCD`, so neither side needs any RNG-state reasoning and the
logo keeps its motion. Format reference A2 records why a constant-returning stub of
`0x5D7DC` — this task's first draft — was wrong.

- [ ] **Step 1: Write the failing test**

```python
# tools/tests/test_title_pin.py
import os, subprocess, sys, tempfile, unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TOOL = os.path.join(ROOT, "tools", "title_pin.py")
EXE = os.path.join(ROOT, "data", "game", "C", "PRAGE.EXE")

# Format reference A2. The three title draws carry the ranges the port's LCG
# must reproduce; the opcode-8 site is the in-window consumer (value 0 both sides).
DRAW_SITES = [
    (0x650E9, bytes.fromhex("e842b50400"), bytes.fromhex("b80c000000"), 0x5A),
    (0x650F5, bytes.fromhex("e836b50400"), bytes.fromhex("b86f000000"), 0x7E),
    (0x6510B, bytes.fromhex("e820b50400"), bytes.fromhex("b800000000"), 2),
]
PATCH_SITES = DRAW_SITES + [
    (0x7E289, bytes.fromhex("e8a2230300"), bytes.fromhex("b800000000"), None),
]

def lcg_draws():
    s, out = 0xABCD, []
    for _off, _orig, _repl, rng in DRAW_SITES:
        s = (s * 0xB90D12B9 + 0x38CE051F) & 0xFFFFFFFF
        out.append(((s >> 16) * (rng & 0xFFFF)) >> 16)
    return out

class TitlePinTest(unittest.TestCase):
    def run_tool(self, src, out):
        return subprocess.run([sys.executable, TOOL, "--src", src, "--out", out],
                              capture_output=True, text=True)

    def test_patches_all_three_sites_and_nothing_else(self):
        with tempfile.TemporaryDirectory() as d:
            out = os.path.join(d, "PRAGE_PIN.EXE")
            r = self.run_tool(EXE, out)
            self.assertEqual(r.returncode, 0, r.stderr)
            with open(EXE, "rb") as fh: a = fh.read()
            with open(out, "rb") as fh: b = fh.read()
            self.assertEqual(len(a), len(b))
            expect = bytearray(a)
            for off, _orig, repl, _rng in PATCH_SITES:
                expect[off:off + 5] = repl
            self.assertEqual(b, bytes(expect))

    def test_patch_sites_hold_the_original_calls(self):
        with open(EXE, "rb") as fh: a = fh.read()
        for off, orig, _repl, _rng in PATCH_SITES:
            self.assertEqual(a[off:off + 5], orig, hex(off))

    def test_patched_values_are_the_lcg_results_for_their_ranges(self):
        # The immediates must equal rng_seed(0xABCD) + the real draws, or the
        # port (which takes the real draws) would diverge on the entry frame.
        vals = lcg_draws()
        self.assertEqual(vals, [12, 111, 0])
        for (_off, _orig, repl, _rng), v in zip(DRAW_SITES, vals):
            self.assertEqual(int.from_bytes(repl[1:], "little"), v)

    def test_refuses_an_already_patched_file(self):
        with tempfile.TemporaryDirectory() as d:
            once = os.path.join(d, "one.exe")
            twice = os.path.join(d, "two.exe")
            self.assertEqual(self.run_tool(EXE, once).returncode, 0)
            r = self.run_tool(once, twice)
            self.assertNotEqual(r.returncode, 0)
            self.assertIn("site", r.stderr)
            self.assertFalse(os.path.exists(twice))

    def test_refuses_a_wrong_file_and_writes_nothing(self):
        with tempfile.TemporaryDirectory() as d:
            bad = os.path.join(d, "bad.exe")
            out = os.path.join(d, "out.exe")
            with open(bad, "wb") as fh: fh.write(b"\x00" * 4096)
            r = self.run_tool(bad, out)
            self.assertNotEqual(r.returncode, 0)
            self.assertFalse(os.path.exists(out))

    def test_refuses_an_out_under_data(self):
        # data/ is git-ignored and this repo has no remote: overwriting it
        # would destroy the only copy of the game.
        with tempfile.TemporaryDirectory() as d:
            out = os.path.join(ROOT, "data", "game", "C", "PRAGE_PIN.EXE")
            r = self.run_tool(EXE, out)
            self.assertNotEqual(r.returncode, 0)
            self.assertFalse(os.path.exists(out))

    def test_refuses_out_equal_to_src(self):
        with tempfile.TemporaryDirectory() as d:
            victim = os.path.join(d, "PRAGE.EXE")
            shutil.copyfile(EXE, victim)
            with open(victim, "rb") as fh: before = fh.read()
            r = self.run_tool(victim, victim)
            self.assertNotEqual(r.returncode, 0)
            with open(victim, "rb") as fh: self.assertEqual(fh.read(), before)

    def test_never_writes_under_data(self):
        with tempfile.TemporaryDirectory() as d:
            out = os.path.join(d, "PRAGE_PIN.EXE")
            self.run_tool(EXE, out)
            for root, _, files in os.walk(os.path.join(ROOT, "data")):
                for f in files:
                    if f.endswith("_PIN.EXE"):
                        self.fail(f"pin wrote {os.path.join(root, f)}")

if __name__ == "__main__":
    unittest.main()
```

`import shutil` is needed for the `out == src` test.

- [ ] **Step 2: Run it to verify it fails**

Run: `python3 -m unittest tools.tests.test_title_pin -v`
Expected: FAIL — `title_pin.py` does not exist.

- [ ] **Step 3: Write the tool**

```python
#!/usr/bin/env python3
"""Patch a COPY of PRAGE.EXE to pin the RNG draws the title consumes.

0x121A0 draws three values on entry and uses them for the logo's start X, speed
and gravity. Each `call 0x5D7DC` is replaced in place by `mov eax, imm32` holding
the value the port's own LCG (seed 0xABCD) produces for that call's range, so the
port reproduces the same three values by seeding and taking the real draws and the
logo keeps its motion. A fourth site pins the anim stream's opcode-8 handler
(0x2B2A0) to 0, because that handler is the only in-window RNG consumer and its
value would otherwise depend on the master loop's unbounded, host-timed spin.

Fails closed: every patch site's original bytes are verified before anything is
written, so a wrong, truncated or already-patched binary aborts and writes nothing.
Refuses to write over the source or anywhere under the repo's data/.
Usage: title_pin.py --src data/game/C/PRAGE.EXE --out /tmp/pr_title_pin/PRAGE.EXE"""
import argparse, os

# (file offset, original 5 bytes, replacement). Format reference A2.
PATCHES = [
    (0x650E9, bytes.fromhex("e842b50400"), bytes.fromhex("b80c000000")),  # 12
    (0x650F5, bytes.fromhex("e836b50400"), bytes.fromhex("b86f000000")),  # 111
    (0x6510B, bytes.fromhex("e820b50400"), bytes.fromhex("b800000000")),  # 0
    (0x7E289, bytes.fromhex("e8a2230300"), bytes.fromhex("b800000000")),  # opcode 8
]
DATA_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "data")

# The guard must be case-insensitive and filesystem-accurate: this platform's
# APFS resolves Data/ and data/ to the same directory, so a string-wise
# comparison lets `--out <root>/Data/...` write into the real, unrecoverable
# data/. Compare casefolded resolved paths and, where both exist, require that
# the output's directory is not the same file as data/.
def guard(src, out):
    fold = lambda p: os.path.realpath(p).casefold()
    if fold(out) == fold(src):
        raise SystemExit("title_pin: refusing to write over the source: %s" % out)
    data = fold(DATA_DIR)
    if fold(out) == data or fold(out).startswith(data + os.sep):
        raise SystemExit("title_pin: refusing to write under data/: %s" % out)
    parent = os.path.dirname(fold(out))
    try:
        if os.path.exists(parent) and os.path.samefile(parent, DATA_DIR):
            raise SystemExit("title_pin: refusing to write under data/: %s" % out)
    except OSError:
        pass

def patch(src, out):
    guard(src, out)
    with open(src, "rb") as f:
        img = bytearray(f.read())
    for off, orig, _repl in PATCHES:
        if img[off:off + len(orig)] != orig:
            raise SystemExit("title_pin: site mismatch at 0x%X -- wrong or "
                             "already-patched binary, nothing written" % off)
    for off, _orig, repl in PATCHES:
        img[off:off + len(repl)] = repl
    d = os.path.dirname(os.path.abspath(out))
    os.makedirs(d, exist_ok=True)
    tmp = out + ".part"
    with open(tmp, "wb") as f:
        f.write(img)
    os.replace(tmp, out)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    patch(a.src, a.out)
    print("title_pin: wrote %s (3 title draws -> 12, 111, 0)" % a.out)

if __name__ == "__main__":
    main()
```

- [ ] **Step 4: Run the test to verify it passes**

Run: `python3 -m unittest tools.tests.test_title_pin -v`
Expected: PASS (8 tests), output pristine.

- [ ] **Step 5: Add the Makefile target**

Add `title-pin` to `.PHONY` (line 31) and, next to `re-original`:

```make
TITLE_PIN_DIR = /tmp/pr_title_pin

title-pin: ## Build the pinned copy of PRAGE.EXE for the title oracle (writes /tmp only)
	$(PYTHON) tools/title_pin.py --src $(GAME_DIR)/PRAGE.EXE --out $(TITLE_PIN_DIR)/PRAGE.EXE
```

- [ ] **Step 6: Verify and commit**

Run: `make title-pin && python3 -m unittest tools.tests.test_title_pin` — expect
the file to be written under `/tmp/pr_title_pin/` and PASS.
Also confirm `git status --short` shows no change under `data/`.

```bash
git add tools/title_pin.py tools/tests/test_title_pin.py Makefile
git commit -m "tools: pin the RNG draws the title consumes in a capture-only PRAGE.EXE copy"
```

---

### Task 2: The title capture, and the reproducibility gate

**Files:**
- Create: `tools/title_capture.py`
- Modify: `Makefile` (target `title-capture`, `.PHONY`)
- Modify: `.gitignore` (ignore `data/title-captures/`)

**Interfaces:**
- Consumes: `tools/title_pin.py` (Task 1); `tools/smk_capture.py`'s `run_dosbox`,
  `read_avi_frames`, `align` and `ffprobe_fps` — **imported**, not copied.
- Produces: `data/title-captures/title/frame_%04d.raw` — the pinned original's
  post-logo run as 320×200 RGB24, collapsed to **one file per distinct consecutive
  frame**, plus `window.txt` recording, per written frame, the raw capture index it
  came from. The title window inside it is located by Task 10's content alignment,
  not by this task.

**Why one file per distinct game frame, not per capture frame** (the plan's second
draft assumed capture index == game frame index, which is false): the game's logic
runs at 60 Hz while mode 13h is captured at 70.09 Hz, so a game frame is displayed
for one or two capture frames. Consecutive identical capture frames are holds of a
single game frame and must be collapsed; otherwise no frame-for-frame comparison is
possible.

**Why the window is located by content, not by a fixed rule** (the plan's second
draft said "the first frame after the second logo", which is wrong): `FUN_00011000`
— the boot sub-machine, `prage.c:775` — runs its own countdown machine (180-, 64-
and 240-frame waits) between the two logos and state 1. In the pinned capture the
title window is the contiguous RNG-sensitive block at raw ≈2200, localised by
diffing a pinned run against an unpinned one (`0x121A0` is the only title-state RNG
consumer and the boot machine reaches the RNG only through a countdown re-seed).
Its exact start is then pinned by content match against the port's `frame_0000.raw`
(Task 10's `PR_TITLE_DUMP`) — so **the port's dump is an input to the anchor, not a
consumer of it.** The tool must support `--port-anchor DIR` (align the port's frames
against the capture and emit one capture frame per port frame) and must never write
a guessed anchor.

**The gate is re-derived in Task 10, and this ordering is deliberate.** A
port-free window locator is circular: the only thing that identifies the first
`DS_000F0A66 == 0x600` frame exactly is the port's own `frame_0000.raw`. An earlier
draft tried to locate the window from the pinned-vs-unpinned RNG-sensitivity diff;
pinning the in-window consumers (Format reference A2, sites 4 and 5) removed that
sensitivity, so the locator silently fell back to a boot-region window and produced
a meaningless verdict. So this task captures the run, collapses holds, records the
evidence and provides `--port-anchor`; **Task 10 pins the window by content and
proves determinism by requiring the port to match two independent captures** — a
strictly stronger statement than a port-free byte-identity gate, and the only
non-circular one.

Whole-AVI byte-identity is not achievable and is not what the oracle needs:
measured on two 45 s pinned runs, the boot text/movie region carries sampling
jitter (they differ from raw frame 31).

- [ ] **Step 1: Write the failing test**

There is no offline unit test for DOSBox-X itself; the tool's `--verify-reproducible`
mode is the gate, and running it is this task's proof.

- [ ] **Step 2: Write the tool**

Reuse `smk_capture` by import — its `run_dosbox` already performs the correct
invocation (`dosbox-x -defaultconf -fastlaunch -nopromptfolder -nogui -nomenu
-time-limit <n> -set "sdl fullscreen=false" -set "dosbox captures=<avi>" -c 'MOUNT C
<dir> -ro' -c 'IMGMOUNT D <iso> -t iso' -c C: -c 'DX-CAPTURE /V PRAGE.EXE -f' -c
EXIT`), so the tool must not re-implement it or modify `smk_capture.py`.

Staging is required and must not copy the assets: build `/tmp/pr_title_pin/C/` as a
symlink for every entry of `data/game/C` except `PRAGE.EXE`, which is the real
pinned copy from Task 1, plus `/tmp/pr_title_pin/CD/RAGECD.ISO` symlinked to
`data/game/CD/RAGECD.ISO`. That layout is what `run_dosbox`'s
`dirname(game_dir)/CD/RAGECD.ISO` derivation needs.

```python
#!/usr/bin/env python3
"""Capture the pinned original's title window as 320x200 RGB24 frames.

Runs the Task 1 pinned copy in DOSBox-X, records the whole boot (two Smacker logos,
then FUN_00011000's boot sub-machine, then the title), and writes one frame per
distinct game frame over the title window.

The window start is pinned by content, never guessed: without --port-anchor the
tool writes the RNG-sensitive block (pinned-vs-unpinned diff) and reports it; with
--port-anchor it aligns the port's frames and emits the matched capture frames, one
per port frame.

--verify-reproducible captures twice and requires the distinct-frame sequences to
be byte-identical; a mismatch voids the pin.
Reuses smk_capture.run_dosbox/read_avi_frames/align (2b settled the invocation).
Usage: title_capture.py --out data/title-captures/title [--port-anchor DIR]
                        [--verify-reproducible]"""
```

The tool's contract, in order:

1. Refuse to run unless `/tmp/pr_title_pin/PRAGE.EXE` exists; build the staging
   directory (symlinks only, never a copy); never write under `data/game/`.
2. Capture the whole run (`--time-limit`, default 45 s) to a temp AVI dir; decode to
   320×200 RGB24 with `read_avi_frames`.
3. Emit the whole region after the second logo, collapsed to its distinct
   consecutive frames, and (when `--port-anchor DIR` is given) align the port's
   frames against the capture with `smk_capture.align` and emit one capture frame per
   port frame, for Task 10. Never guess a window: with no `--port-anchor`, emit the
   whole region.
4. Write the window collapsed to distinct consecutive frames as
   `frame_%04d.raw` (exactly 192000 bytes each), recording each one's raw capture
   index in `window.txt`. If the window cannot be identified, print the candidate
   blocks with their indices and counts and stop without writing.
5. `--verify-reproducible`: run 2–4 twice, require the distinct-frame sequences to
   be byte-identical, and on failure report the first differing frame index.

- [ ] **Step 3: Run the capture and report the evidence**

Run: `make title-pin && python3 tools/title_capture.py --out data/title-captures/title --time-limit 45`

Expected: `frame_0000.raw` … exist, 192000 bytes each, and `window.txt` names the raw
capture index of each. Report: the emitted frame count, the raw range they cover,
both boot movies' last-presented raw indices, and — since the RNG-sensitivity
locator is dead by construction (Format reference A2 pins the consumers) — the fact
that the emitted region covers the whole post-logo run, so Task 10's content
alignment can find the title window inside it. Do not claim the title window has
been located; this task does not locate it.

- [ ] **Step 4: Confirm the pin is the only intended variance source**

Run: `python3 tools/title_capture.py --out data/title-captures/title --verify-reproducible`
This is a **diagnostic, not a gate**: report the two runs' frame counts, how many
frames are shared, and where they diverge. Expected: the boot/movie region diverges
(sampling jitter) and the post-title region agrees. A divergence *inside* the title
composite is a finding for Task 1 — report it with the raw indices instead of
concluding anything, and do not treat the diagnostic's exit status as this task's
verdict. Task 10 owns the real gate.

- [ ] **Step 5: Wire the Makefile target and commit**

```make
TITLE_CAPTURES = data/title-captures

title-capture: title-pin ## Capture the pinned original run for the title oracle (writes data/title-captures/)
	$(PYTHON) tools/title_capture.py --out $(TITLE_CAPTURES)/title --time-limit 45
```

Add `data/title-captures/` to `.gitignore` (captures are the original's bytes).

```bash
git add tools/title_capture.py Makefile .gitignore
git commit -m "tools: capture the pinned original run for the title oracle"
```

---

### Task 3: `game/rng.{c,h}` — the LCG, and the loop wiring

**Files:**
- Create: `port/src/game/rng.h`, `port/src/game/rng.c`
- Create: `port/tests/test_rng.c`
- Modify: `port/src/game/flow.c` (`game_init`, `game_loop`)
- Modify: `port/tests/test.h`, `port/tests/run_tests.c`, `port/CMakeLists.txt` (lines 13 and 39)

**Interfaces:**
- Consumes: `mem.h` (`DSD`), `symbols.h` (`DS_000EF6D8`).
- Produces:
  ```c
  void rng_seed(u32 s);        /* store s in DS_000EF6D8 */
  u32  rng_next(u32 range);    /* advance, return ((state>>16)*(range&0xffff))>>16 */
  void rng_step(void);         /* advance and discard (the master loop's own draw) */
  ```

There is **no stub or pin-side instrument in the port.** Task 1 pins the original's
three title draws to the values this LCG produces from seed `0xABCD`, so the port
reproduces them honestly: `rng_seed(0xABCD)` runs at title entry (Task 9, inside
`0x121A0`'s phase 0, before the three draws) and `rng_next` is called three times in
the transcribed order with ranges `0x5A`, `0x7E`, `2`. Task 3 seeds only in
`game_init` (mirroring `0x20C10`); the title-entry re-seed belongs to Task 9.

- [ ] **Step 1: Write the failing test**

```c
/* port/tests/test_rng.c */
#include "test.h"
#include "game/rng.h"
#include "mem.h"
#include "symbols.h"

int test_rng(void)
{
    /* The recurrence's own outputs, computed independently from
     * state = state*0xB90D12B9 + 0x38CE051F seeded 0xABCD. */
    static const u32 ranges[] = { 0x5Au, 0x7Eu, 2u, 0xFFFFu, 0x10000u, 0u, 0x7FFFFFFFu };
    static const u32 expect[] = { 12u, 111u, 0u, 29617u, 0u, 0u, 11010u };

    rng_seed(0xABCDu);
    CHECK_EQ_INT((int)DSD(DS_000EF6D8), 0xABCD);
    for (unsigned i = 0; i < sizeof ranges / sizeof ranges[0]; i++)
        CHECK_EQ_INT((int)rng_next(ranges[i]), (int)expect[i]);

    /* A range wider than 16 bits is masked, and 0 must not shift undefinedly.
     * These are FRESH-seed draws, not members of the ordered sequence above. */
    rng_seed(0xABCDu);
    CHECK_EQ_INT((int)rng_next(0x7FFFFFFFu), 9158);
    CHECK_EQ_INT((int)rng_next(0u), 0);

    /* rng_step advances the state and discards the value. */
    rng_seed(0xABCDu);
    u32 before = DSD(DS_000EF6D8);
    rng_step();
    CHECK(DSD(DS_000EF6D8) != before, "rng_step advanced the state");
    CHECK_EQ_INT((int)DSD(DS_000EF6D8), (int)(0xABCDu * 0xB90D12B9u + 0x38CE051Fu));

    /* The three title draws, in order, are what Task 1 pins the original to:
     * the immediates 12, 111, 0 in tools/title_pin.py must equal these. */
    rng_seed(0xABCDu);
    CHECK_EQ_INT((int)rng_next(0x5Au), 12);
    CHECK_EQ_INT((int)rng_next(0x7Eu), 111);
    CHECK_EQ_INT((int)rng_next(2u), 0);
    return 0;
}
```

- [ ] **Step 2: Run it to verify it fails**

Run: `PR_GAME_DIR=data/game/C make test`
Expected: FAIL to build — `game/rng.h` not found.

- [ ] **Step 3: Write the module**

```c
/* port/src/game/rng.h */
#ifndef PRAGE_GAME_RNG_H
#define PRAGE_GAME_RNG_H

#include "../types.h"

/* 0x5D7DC: state = state*0xB90D12B9 + 0x38CE051F, return
 * ((state >> 16) * (range & 0xffff)) >> 16. Seed 0xABCD from 0x20C10. */
void rng_seed(u32 s);
u32  rng_next(u32 range);
void rng_step(void);

#endif
```

```c
/* port/src/game/rng.c */
#include "game/rng.h"
#include "mem.h"
#include "symbols.h"

void rng_seed(u32 s) { DSD(DS_000EF6D8) = s; }

u32 rng_next(u32 range)
{
    DSD(DS_000EF6D8) = DSD(DS_000EF6D8) * 0xB90D12B9u + 0x38CE051Fu;
    return (DSD(DS_000EF6D8) >> 16) * (range & 0xFFFFu) >> 16;
}

void rng_step(void) { (void)rng_next(0); }
```

- [ ] **Step 4: Wire it into the flow**

In `port/src/game/flow.c`: add `#include "game/rng.h"`, then

* `game_init()`, next to `render_list_init()` (line 490): `rng_seed(0xABCDu);`
  with a `/* PORT: 0x20C10 seeds the LCG with a hardcoded 0xABCD. */` note.
* `game_loop()`: after `swap_buffers();` (line 551) and before
  `game_audio_service();`, add `rng_step();` with the ordering comment from
  Format reference I (`/* 0x255CC calls 0x5D7DC once per iteration, after the
  present and swap and before 0x1CF20. */`).

- [ ] **Step 5: Register the test and run it**

`port/tests/test.h`: add `int test_rng(void);`
`port/tests/run_tests.c`: add `RUN(test_rng);` in the existing pattern.
`port/CMakeLists.txt` line 13: add `src/game/rng.c`; line 39: add
`tests/test_rng.c`.

Run: `PR_GAME_DIR=data/game/C make test`
Expected: PASS.

- [ ] **Step 6: Run the full ladder and commit**

Run: `make verify`
Expected: exit 0 (the RNG step changes no pixels this cycle — no display list
entries exist yet).

```bash
git add port/src/game/rng.h port/src/game/rng.c port/tests/test_rng.c \
        port/tests/test.h port/tests/run_tests.c port/CMakeLists.txt \
        port/src/game/flow.c
git commit -m "rng: the 0x5D7DC LCG, seeded at init, stepped once per iteration"
```

---

### Task 4: `game/actors.{c,h}` — the pool, its lists, and the state-begin reset

**Files:**
- Create: `port/src/game/actors.h`, `port/src/game/actors.c`
- Create: `port/tests/test_actors.c`
- Modify: `port/tests/test.h`, `port/tests/run_tests.c`, `port/CMakeLists.txt`

**Interfaces:**
- Consumes: `mem.h`, `symbols.h` (`DS_001014F4`, `DS_001014EC`, `DS_00105B3C`,
  `DS_00105B40`, `DS_00105BCC`, `DS_00105BD0`), `platform/res.h` (`res_resolve`
  for the `0x33754` palette path in Task 5).
- Produces:
  ```c
  void   actors_init(void);          /* validates the pool res.c already allocated */
  void   actors_reset(void);         /* 0x2BAF4 */
  u32    actor_alloc(void);          /* 0x2AC80; record offset, or 0 */
  void   actor_free(u32 rec);        /* 0x249C0 */
  u32    actor_record(u32 index);    /* pool index -> record offset, or 0 */
  u32    actor_index(u32 rec);       /* record offset -> pool index, or -1u */
  u32    actor_list_head(void);      /* DSD(DS_00105BCC), 0 when empty */
  u32    actor_next(u32 rec);        /* rec of the next active record, 0 at end */
  u32    actor_pset(u32 rec);        /* DS_001014EC + DSW(rec+0x56)*0x20 */
  ```

**Do NOT add `res_alloc` to `res.h`.** The spec's §5.3 proposed it, but
`res_load_index` already performs exactly the original's two allocations —
`DS_001014EC` at `0x4880` and `DS_001014F4` at `0xEBA0`
(`port/src/platform/res.c:120-121`) — and a second allocator entry point would
allocate the pools twice. `actors_init()` validates what exists. This is a
deliberate, reportable deviation from the spec, taken on evidence.

- [ ] **Step 1: Write the failing test**

```c
/* port/tests/test_actors.c */
#include "test.h"
#include "game/actors.h"
#include "platform/res.h"
#include "mem.h"
#include "symbols.h"
#include <string.h>

int test_actors(void)
{
    /* The pool and pset bases come from res_load_index, so a run that reached
     * here has them; assert the shape the rest of the cycle depends on. */
    CHECK(res_load_index("data/game/C", "data/game/C/INDEX") > 0, "index loaded");
    CHECK(DSD(DS_001014F4) != 0, "actor pool allocated by res_load_index");
    CHECK(DSD(DS_001014EC) != 0, "pset pool allocated by res_load_index");
    actors_init();

    /* A fresh reset frees every record and leaves both lists empty. */
    actors_reset();
    CHECK_EQ_INT((int)actor_list_head(), 0);
    CHECK(actor_alloc() != 0, "alloc after reset returns a record");

    /* Allocating every record then exhausting returns 0, never a duplicate. */
    memset(mem + DSD(DS_001014F4), 0, 0xEBA0);
    actors_reset();
    u32 n = 0, first = actor_alloc();
    CHECK(first != 0, "first alloc");
    for (n = 1; n < 580; n++) {
        u32 r = actor_alloc();
        CHECK(r != 0, "alloc within the pool");
        if (r == first) { CHECK(0, "alloc returned the same record twice"); break; }
    }
    CHECK_EQ_INT((int)actor_alloc(), 0);   /* exhaustion */

    /* Free then realloc: the freed record is the one handed back (0x249D0
     * pops the free-list head that 0x249C0 pushed). */
    actors_reset();
    u32 a = actor_alloc(), b = actor_alloc();
    actor_free(a);
    CHECK_EQ_INT((int)actor_alloc(), (int)a);

    /* An out-of-pool or misaligned offset is ignored, not linked. */
    actors_reset();
    u32 c = actor_alloc();
    actor_free(0x1234u);                       /* outside the pool */
    actor_free(c + 1u);                        /* misaligned */
    CHECK_EQ_INT((int)actor_alloc(), (int)c + 0x68u);

    /* reset() zeroes both process masks (0x2A31C's gates) and rebuilds the
     * lists: after it, allocation order restarts at the pool base. */
    DSD(DS_00104AE8) = 0xFFFFFFFFu;
    DSD(DS_00104AEC) = 0xFFFFFFFFu;
    actors_reset();
    CHECK_EQ_INT((int)DSD(DS_00104AE8), 0);
    CHECK_EQ_INT((int)DSD(DS_00104AEC), 0);
    CHECK_EQ_INT((int)actor_alloc(), (int)DSD(DS_001014F4));
    return 0;
}
```

- [ ] **Step 2: Run it to verify it fails**

Run: `PR_GAME_DIR=data/game/C make test`
Expected: FAIL to build — `game/actors.h` not found.

- [ ] **Step 3: Write the pool half of the module**

Transcribe `0x249B0`/`0x249C0`/`0x249D0` (`prage.c:11867-11930`), `0x2AC80`
(`prage.c:15817-15845`) and `0x2BAF4` (`prage.c:16432-16497`) into
`port/src/game/actors.c`. The lists are `{next@+0; prev@+4}` dwords holding
`mem[]` offsets (0 = the sentinel's own address, i.e. empty); the sentinels are
the addresses `DS_00105B3C` (free) and `DS_00105BCC` (active).

```c
/* port/src/game/actors.h */
#ifndef PRAGE_GAME_ACTORS_H
#define PRAGE_GAME_ACTORS_H

#include "../types.h"

#define ACTOR_REC_SIZE 0x68u
#define ACTOR_POOL_RECORDS 580u
#define PSET_SIZE 0x20u

void actors_init(void);
void actors_reset(void);
u32  actor_alloc(void);
void actor_free(u32 rec);
u32  actor_record(u32 index);
u32  actor_index(u32 rec);
u32  actor_list_head(void);
u32  actor_next(u32 rec);
u32  actor_pset(u32 rec);

#endif
```

`actors_reset()` must, in the original's order: `0x2EA30(param_1)` (transcribe
or `/* PORT: */`-name it if it is inert, recording why), zero
`DS_00104AE8`/`DS_00104AEC`, `0x13DF0()`, clear `DS_00105BEA`/`DS_00105BED`,
three `0x61A70()` calls, re-point the four sentinels at themselves, free every
record, `0x13ADC()`, `0x4F228()`, set `DS_00105B44`/`DS_00105B48` through
`0x1C350()`, `0x38B70()`, `0x2F920()`, then seed the back buffer from
`DS_000E87A0` (16,000 dwords + tail) unless called from the `0x2BAF4` variant
that calls `0x52106`/`0x336C0` instead, then `DS_00105BED = 1` and `0x2EA30()`.
Each helper that no task in this cycle reaches gets a `/* PORT: <addr> <name>:
<why inert/out-of-scope> */` line, per the spec's invariant 3.

- [ ] **Step 4: Register and run**

`port/tests/test.h`: `int test_actors(void);`; `run_tests.c`: `RUN(test_actors);`;
CMake lines 13/39: `src/game/actors.c`, `tests/test_actors.c`.

Run: `PR_GAME_DIR=data/game/C make test`
Expected: PASS.

- [ ] **Step 5: Run the ladder and commit**

Run: `make verify`
Expected: exit 0.

```bash
git add port/src/game/actors.h port/src/game/actors.c port/tests/test_actors.c \
        port/tests/test.h port/tests/run_tests.c port/CMakeLists.txt
git commit -m "actors: pool records, free/active lists and the state-begin reset"
```

---

### Task 5: Spawn `0x2AE14`, after pinning its register arguments

**Files:**
- Modify: `port/src/game/actors.c`, `port/src/game/actors.h`
- Modify: `port/tests/test_actors.c`
- Create: `docs/superpowers/plans/2026-09-17-actor-system-args.md` (the pinned binding)

**Interfaces:**
- Consumes: Task 4.
- Produces:
  ```c
  u32 actor_spawn(const u32 *desc, u32 a2, u32 a3, u32 a4, u32 a5); /* 0x2AE14 */
  ```

`__regparm3` hides the register arguments in the decompilation. **Pin them by
disassembly before writing the function, not by inference**: the four title call
sites in Format reference G are the test cases. Method: dump the call sites with
`llvm-objdump -D -b binary -m i386` over the object-0 range (file offset
`va + 0x52E54`), read the `mov ecx/edx/ebx/eax, ...` immediates preceding each
`call`, and record, per call site: which register holds the descriptor and what
each remaining argument is. Write the result into
`docs/superpowers/plans/2026-09-17-actor-system-args.md`, and encode it as
`/* PORT: */` comments at each ported call site.

- [ ] **Step 1: Pin the binding by disassembly**

Produce the doc: for each of `0x121A0`'s four `0x2AE14` call sites plus
`0x38B18`'s (`prage.c:23821`), the register values and the resulting argument
order. Include the raw bytes and the disassembly line, so the review can check
the claim rather than the conclusion.

- [ ] **Step 2: Write the failing test**

Extend `test_actors.c` with a `check_actor_spawn()` sub-test:
descriptor `0x9AC30` read from the data object (it is resident in `mem[]`),
spawned with the pinned arguments. Assert the record fields the descriptor
controls: `rec+0x2E`, `rec+0x48`, `rec+0x40`, `rec+0x2C`, `rec+0x28 & 0xC3 == 0`,
`rec+0x56` pset index, and that the pset's `+0x02` carries `rec+0x2E` and (when
`rec+0x5F != 0`) `0x800`. Then assert `actor_spawn` returns 0 when the pool is
exhausted and that the pool is unchanged.

- [ ] **Step 3: Run it to verify it fails**

Run: `PR_GAME_DIR=data/game/C make test`
Expected: FAIL — `actor_spawn` undefined.

- [ ] **Step 4: Transcribe `0x2AE14` and its support**

Transcribe `0x2AE14` (`prage.c:15885-16048`) with the write order listed in
Format reference D, plus the helpers it calls that this cycle reaches:
`0x2AD40` (`prage.c:15849`), `0x2EA30` (`prage.c:18424`), `0x33754`
(`prage.c:20751`, the pset `+0x18` palette source), and the pset palette
enqueue through `0x33864` — in the port that is `palette_record()` in `flow.c`,
which is `static`; give `actors.c` its own `0x33864` transcription writing the
same dirty-list shape at `DS_00107798`, or make `palette_record` shared. Prefer
sharing: move it from `flow.c` to `platform/gfx.{c,h}` (which already owns
`gfx_dac` and `gfx_flush_palette`) and call it from both, so the palette queue
has one owner.

`0x2AE14` ends by calling the per-type render check
`(**(code **)(&DS_000BB9DC + rec+0x48 * 0xC))()` — transcribe the indirection as
a switch on `rec+0x48` over the cases the title reaches (descriptor `+0x04` is
`0x01` for `0x9AC30`, `0x10` for `0x9AC94`), and `/* PORT: */`-name every case
left unported with the reason. When the check returns 2 the pset sprite id
becomes `0x1E1`.

- [ ] **Step 5: Run, then the ladder, then commit**

Run: `PR_GAME_DIR=data/game/C make test` then `make verify` — both expected to
pass.

```bash
git add port/src/game/actors.c port/src/game/actors.h port/tests/test_actors.c \
        port/src/platform/gfx.c port/src/platform/gfx.h port/src/game/flow.c \
        docs/superpowers/plans/2026-09-17-actor-system-args.md
git commit -m "actors: spawn 0x2AE14 with the register arguments pinned by disassembly"
```

---

### Task 6: The pset sync — `0x2A31C` → `0x2A1FC` → `0x2A820`

**Files:**
- Modify: `port/src/game/actors.c`, `port/src/game/actors.h`, `port/tests/test_actors.c`

**Interfaces:**
- Consumes: Tasks 4–5.
- Produces:
  ```c
  void actors_update(void);   /* 0x2A31C: walk the active list, sync each record */
  ```

- [ ] **Step 1: Write the failing test**

Extend `test_actors.c` with `check_pset_sync()`: spawn a descriptor, set
`rec+0x18`/`rec+0x1C`, `rec+0x2C`, `rec+0x59`, `rec+0x5A`, then `actors_update()`
and assert the pset at `actor_pset(rec)`: `+0x04`/`+0x08` equal the record's
position (through the `0x2A690` path), `+0x0E` carries the layer the record's
`+0x59`/`+0x5A` produce, `+0x0C` equals `rec+0x2C`, and `rec+0x3C` mirrors the
written pset x. Then assert a record with `+0x28 & 4` (the `0x2A39C` path) takes
its sprite id from the stream reader, and that a record whose `+0x28 & 0x18`
tests put it off-screen does not get `+0x2B |= 0x18`.

- [ ] **Step 2: Run it to verify it fails**

Run: `PR_GAME_DIR=data/game/C make test` — FAIL, `actors_update` undefined.

- [ ] **Step 3: Transcribe the sync**

`0x2A31C` (`prage.c:15372-15400`): decrement `DS_00105BEC`, wrap to
`DS_00105BEE - 1`; walk the active list from `DS_00105BCC`; for each record
whose `+0x28 & 2` is clear, either call `0x2A1FC` or clear `+0x28 & 1`.
`0x2A1FC` (`prage.c:15308-15368`) and `0x2A820` (`prage.c:15642-15719`) are the
position/layer writers; `0x2A690` (`prage.c:15570-15638`) writes the pset and
the mode-1 shear cache through `DS_00107900`; `0x2A620` (`prage.c:15544`) is the
mode-1 cursor helper. `0x2A4FC` (`prage.c:15474`) and `0x2A39C`
(`prage.c:15404`) are the motion and anim-id paths `0x2A1FC` reaches. Transcribe
each; a `0x2B150` (`prage.c:16049`) call is the dead bit — transcribe it too
(63 callers).

- [ ] **Step 4: Run, ladder, commit**

Run: `PR_GAME_DIR=data/game/C make test`, then `make verify`.

```bash
git add port/src/game/actors.c port/src/game/actors.h port/tests/test_actors.c
git commit -m "actors: pset sync 0x2A31C/0x2A1FC/0x2A820 with motion and the shear cache"
```

---

### Task 7: The animation-stream interpreter — `0x2A408`, `0x29F34`, `0x29DB8`

**Files:**
- Modify: `port/src/game/actors.c`, `port/src/game/actors.h`, `port/tests/test_actors.c`
- Create: `port/tests/test_anim.c` (and its `test.h`/`run_tests.c`/CMake entries) if the test file outgrows `test_actors.c`; splitting is preferred once it does.

**Interfaces:**
- Consumes: Tasks 4–6; `game/rng.h` (`rng_next`), because the anim stream's opcode 8
  draws a value into the record's frame timer `rec+0x20`.
- Produces:
  ```c
  u32  anim_next_sprite_id(u32 rec, const u16 *stream);  /* 0x2A408 */
  u32  anim_read_var(u32 rec, u8 op);                    /* 0x29F34 */
  void anim_write_var(u32 rec, u8 op, u32 value);        /* 0x29DB8 */
  void actors_pin_anim_tick_zero(int on);                /* oracle mirror, see below */
  ```

`actors_pin_anim_tick_zero(on)` is the port's half of Task 1's fourth pin site: the
original's opcode-8 handler has its `0x5D7DC` call replaced by `mov eax, 0`, so the
port must make the opcode-8 tick draw 0 as well (`on ? 0 : rng_next(range)` at that
one call site). It is a TEST-ONLY seam set by the title oracle driver (Task 10) and
documented where it is defined; nothing else calls it, and it is the only way to
reproduce the pinned original's opcode-8 behaviour without a nondeterministic RNG.

- [ ] **Step 1: Write the failing test**

`check_anim()`: build a record in the pool, point `rec+0x08` at a synthetic
stream in a scratch `mem[]` region, and check, per Format reference F:
a literal opcode advances the pointer by one word and returns
`word & 0x7FFF`; `word = 0xD100` with `(word >> 8 & 0x60) == 0x40` consumes one
extra word and returns `0x29F34(rec, 0x00) + next`; `word = 0xD200` consumes two
extra words and returns `table[0x29F34(rec, 0x00)]`; the hflip bit is
`(id & 0x8000) != 0` XOR `(rec+0x28 >> 8 & 0x40) != 0`; and the `0x40..0x51`
opcode ranges read the four one-byte variables, the `+0x4A` parent record and
the `+0x4B` child record. Assert `0x29F34` sign-extends `0x40..0x43` and `0x45`
and masks `< 0x40` through the `DS_00105B4C` ring.

Include the second title object's real stream (`0x9AC94`'s pointer `0x0E897A`,
which begins `40 CD`) as a case: `0x40` is the `rec+0x52` variable read; assert
the returned id and the pointer's advance, so the spec's open item on
`0x29F34`'s enumeration is closed by a real asset, not by a synthetic one.

- [ ] **Step 2: Run it to verify it fails** — FAIL, undefined.

- [ ] **Step 3: Transcribe the readers and writer**

`0x2A408` (`prage.c:15429-15470`), `0x29F34` (`prage.c:15179-15247`),
`0x29DB8` (`prage.c:15090-15175`), plus the anim-entry helpers `0x2BC30`
(`prage.c:16501`) and `0x2BCF4` (`prage.c:16545`) that also consume the stream, plus
the opcode dispatcher `0x2B2A0` (`prage.c:16077`, 1623 bytes) — opcode 8 inside it
is the in-window RNG consumer the pin mirrors.

- [ ] **Step 4: Run, ladder, commit**

Run: `PR_GAME_DIR=data/game/C make test`, then `make verify`.

```bash
git add port/src/game/actors.c port/src/game/actors.h port/tests/test_anim.c \
        port/tests/test_actors.c port/tests/test.h port/tests/run_tests.c port/CMakeLists.txt
git commit -m "actors: animation-stream interpreter 0x2A408/0x29F34/0x29DB8"
```

---

### Task 8: pset layer select — `0x2F0F0`, `0x2F198`, `0x2F280`, `0x2F4BC`

**Files:**
- Modify: `port/src/game/actors.c`, `port/src/game/actors.h`, `port/tests/test_actors.c`

**Interfaces:**
- Consumes: Task 6.
- Produces: `void pset_layer_select(u32 which);` — one entry point over the four
  originals, keyed by the caller's address, since all four are pset-cursor
  writes the title calls in sequence.

- [ ] **Step 1: Write the failing test**

`check_pset_layer()`: after `actors_reset()`, call `pset_layer_select()` for each
of the four and assert the pset pool's layer cursor moved as the corresponding
original does (`0x2F198` clears, `0x2F280` writes the layer set), reading the
expected values from a hand-computed table derived from the disassembly of the
four loci.

- [ ] **Step 2: Run it to verify it fails** — FAIL.

- [ ] **Step 3: Transcribe**

`0x2F0F0` (`prage.c:18923`), `0x2F198` (`prage.c:18974`), `0x2F280`
(`prage.c:19028`), `0x2F4BC` (`prage.c:19185`).

- [ ] **Step 4: Run, ladder, commit**

```bash
git add port/src/game/actors.c port/src/game/actors.h port/tests/test_actors.c
git commit -m "actors: pset layer select 0x2F0F0/0x2F198/0x2F280/0x2F4BC"
```

---

### Task 9: The title state `0x121A0`, and the fake title's removal

**Files:**
- Modify: `port/src/game/flow.c`
- Modify: `port/tests/test_flow.c`, `port/tests/test_gfx.c`
- Modify: `Makefile` (the `clean` target's reference-file list)
- Delete reference: `port/tests/s16title_frame10.idx` (untracked; the `clean` entry goes with it)

**Interfaces:**
- Consumes: Tasks 3–8.
- Produces: `game_state_title()` becomes the `0x121A0` transcription; a
  `PR_TITLE_DUMP` dump hook (mirroring 2b's `PR_SMK_DUMP`) in
  `port/src/game/movie.c`'s neighbourhood — put it in `flow.c` next to the state,
  writing `frame_%04d.raw` RGB24 from `mem + DSD(DS_000E87A4)` via `gfx_dac`,
  one file per presented frame, only while `DS_000F0A66 >= 0x600` and for at
  most 96 frames.

`test_flow.c:91-123` asserts the *fake* title animates and enqueues a palette;
`test_gfx.c:102-125` compares `frames/frame_0001.idx` against S16TITLE frame 10.
Both encode the fake. Replace them: `test_flow.c` keeps the "state 1 runs and
enqueues a palette" assertions (they remain true of the real composite), and
`test_gfx.c`'s S16TITLE comparison is **deleted** along with the `clean` entry —
the real title composite is covered by Task 10's oracle, which is a stronger
proof of the same property. Say so in the commit message.

- [ ] **Step 1: Write the failing test**

Extend `test_flow.c`: with the real title, after a few frames
`DSD(DS_001014F4)` holds allocated records (`actor_list_head() != 0`), the psets
carry a palette handle, and `DS_000F0A66` decreases by `0x10` per frame from
`0x600`. Assert that the title-entry re-seed makes the three draws land where
Task 1 pinned them: after the entry frame `DS_00107A50 == 0x2420` and
`DS_00107A3A == 0x121`, per Format reference G.

- [ ] **Step 2: Run it to verify it fails** — FAIL: the fake leaves no records.

- [ ] **Step 3: Transcribe `0x121A0` and delete the fake**

Delete `TITLE_FRAMES`, `TITLE_FRAME_COUNT`, `TITLE_HOLD_FRAMES`,
`s_title_chunks`, `s_title_off`, `s_title_chunk_n`, `s_title_ready`,
`s_title_idx`, `s_title_hold`, `title_load`, and the `gra_decode_frame` call.
Keep `s_music_request`/`game_sample_request` with a corrected `/* PORT: */`
comment: the original's `0x2C3FC(0x41)`/`(0x43)` are case-5 voice cancels (both
table records are case 5; their handles `0x383B6F4`/`0x3837440` point into
`s16title.gra`), so the port's music/sample requests are a port choice standing
in for the deferred attract-state trigger, not a transcription of those calls.
Correct the same claim at `flow.c:233-241` and the `DAT_000BBDC8` comment at
`flow.c:333-337` (it is a static table in the EXE, stride 12, byte 0 = case,
dword +4 = handle).

`0x2BF08`, `0x10DB0`/`0x10E18` and the `0x33904`/`0x13C70` walk: transcribe the
walk (it is reached inside the pinned window when `DS_000F0A66 < 0x11`), and
`/* PORT: <addr>: <the spec §7 hypothesis and the frame-32/64/96 signature> */`
the two deferred helpers rather than silently skipping them.

- [ ] **Step 4: Run, then the ladder**

Run: `PR_GAME_DIR=data/game/C make test` — PASS. Then `make check frames=120`
and confirm `frames/` fills and the composite is plausibly the title (the
oracle in Task 10 is the proof; this step is a smoke check that the state runs
without a crash or a blank buffer).

- [ ] **Step 5: Commit**

```bash
git add port/src/game/flow.c port/tests/test_flow.c port/tests/test_gfx.c Makefile
git commit -m "title: transcribe 0x121A0 and delete the fake full-screen title"
```

---

### Task 10: `make title-oracle` — the pixel-exact gate

**Files:**
- Create: `tools/title_compare.py`
- Modify: `Makefile` (target `title-oracle`, `.PHONY`, `verify` prerequisites)
- Modify: `port/tests/test_title.c` (and the harness/CMake entries) — the dump driver

**Interfaces:**
- Consumes: Tasks 1, 2, 9 (`PR_TITLE_DUMP`).
- Produces: `make title-oracle`, a sibling of `smk-oracle`: skip when the capture
  is absent, fail when `PR_ORACLE_REQUIRED=1` and absent.

The window is exactly the spec's DoD #2: every frame from state-1 entry until
`DS_000F0A66` reaches `0x11`, 96 frames, zero tolerance. Index-for-index is valid
here because Task 2 collapses the capture to one file per distinct game frame;
`title_compare.py` otherwise mirrors `smk_compare.py`, including its env-gate
semantics, and takes no tolerance argument. A mismatch must be reported with both
the frame index and, from Task 2's `window.txt`, the raw capture frame it came

**This task owns the determinism proof that Task 2 could not produce.** The window
is pinned by aligning the port's `frame_0000.raw` into the capture (that is why
`--port-anchor` exists), and determinism is proven by requiring the port to match
**two independent captures** — not by a port-free byte-identity gate, which is
circular (Format reference and Task 2 record the failed attempt). So the oracle step
runs the capture twice and compares the port against both aligned windows; a
disagreement between the two captures inside the aligned window is reported as a
pin finding, with the raw indices of the first divergence.
from, so a collapse error (a genuinely repeated game frame merged) is diagnosable
rather than silent.

- [ ] **Step 1: Write the dump driver**

`port/tests/test_title.c`: `test_title()` runs `game_init()`, then
`actors_pin_anim_tick_zero(1)` (the port's half of the pin — Task 1 patches the
original's opcode-8 draw to 0, so the oracle run must make the same draw 0), then
drives `game_frame()`/`render_list()` for 96 frames with `PR_TITLE_DUMP` set. The
title entry frame itself calls `rng_seed(0xABCDu)` (Task 9) before its three draws,
which is the port-side half of the other three pin sites. The driver must reuse the
same presentation conversion as `gfx_present` (the palette in `gfx_dac`), so the
dumped RGB24 equals what the capture holds.

- [ ] **Step 2: Write the comparator**

```python
#!/usr/bin/env python3
"""Pixel-exact comparison of the port's title frames against the capture.
Absent capture: skip (exit 0) unless PR_ORACLE_REQUIRED=1, then fail.
Usage: title_compare.py --capture DIR --port DIR --frames 96"""
```

Same shape as `smk_compare.py`, including the env-gate semantics verbatim, with
`frame_%04d.raw` on both sides and a zero-byte-tolerance comparison. Report the
first differing byte offset, as `smk_compare.py` does.

- [ ] **Step 3: Wire the Makefile target**

```make
title-oracle: build ## Pixel-exact title oracle (skips without data/title-captures)
	@echo "== title oracle (pixel-exact, 96 frames) =="
	@if [ -d $(TITLE_CAPTURES)/title ]; then \
		rm -rf $(TITLE_DUMP); \
		PR_TITLE_DUMP=$(TITLE_DUMP) PR_GAME_DIR=$(GAME_DIR) ./$(BUILD_DIR)/run_tests; \
	else \
		echo "title-oracle: no capture at $(TITLE_CAPTURES)/, frames not compared"; \
	fi
	@$(PYTHON) tools/title_compare.py --capture $(TITLE_CAPTURES)/title --port $(TITLE_DUMP)/title --frames 96
```

Add `TITLE_DUMP = /tmp/pr_title_dump`, and add `title-oracle` to the `verify`
target right after `smk-oracle`.

- [ ] **Step 4: Run it and fix what it finds**

Run: `make title-pin && python3 tools/title_capture.py --out data/title-captures/title --port-anchor /tmp/pr_title_dump/title && make title-oracle`
Expected: `title_compare: 96/96 frames match`.

A mismatch is localised evidence: the first differing frame and byte offset name
the stage (entry frame ⇒ spawn/descriptor mapping; later frames ⇒ the anim
interpreter or the pset sync; a uniform colour shift ⇒ the pset palette enqueue;
drift exactly at frames 32/64/96 ⇒ the `0x2BF08` hypothesis, take its named
fallback). Fix the transcription, not the comparison, and re-run Task 2's
reproducibility gate if the capture is suspected.

- [ ] **Step 5: Run the full ladder and commit**

Run: `make verify`
Expected: exit 0, including `title-oracle` with `PR_ORACLE_REQUIRED=1`.

```bash
git add tools/title_compare.py Makefile port/tests/test_title.c \
        port/tests/test.h port/tests/run_tests.c port/CMakeLists.txt
git commit -m "title: pixel-exact oracle over the 96-frame pinned window"
```

---

### Task 11: The cycle report, and the docs the change invalidates

**Files:**
- Create: `docs/superpowers/plans/2026-09-17-actor-system-report.md`
- Modify: `port/RE_GUIDE.md`, `port/spec/game_flow.md`, `README.md`

- [ ] **Step 1: Write the report**

Sections: what landed; the pin as implemented (the three patched draw sites and
their file offsets, why the first draft's constant-returning stub was wrong — it
zeroed the logo's start X, speed *and* gravity — and why the port needs no
instrument: `rng_seed(0xABCD)` at title entry plus the real draws); the capture
contract (one file per distinct game frame, why a capture index is not a game-frame
index at 60 Hz logic / 70.09 Hz mode 13h, why the window is located by content and
not by "after the second logo" — `FUN_00011000`'s boot sub-machine sits between
them) and the Task 2 gate result; the oracle result (frames matched, the exact
command); the register-argument binding and where the evidence sits; the spec
deviations taken with their evidence — **`res_alloc` not added to `res.h`** (the
pools are already allocated at `res.c:120-121`), the `rng_step` addition, the
capture-api changes from the second draft; every `/* PORT: */` label introduced;
every deferred op with its reason; open items (the `0x2BF08` hypothesis' verdict,
`DS_00107900`'s producer, bank byte 0, `DS_00107A3E`/`3A`/`38`); and the residuals
carried into 4b/4c/4d.

- [ ] **Step 2: Update the docs**

`port/RE_GUIDE.md`: the pipeline row now says the actor system is ported; note
the oracle. `port/spec/game_flow.md`: replace the fake-title description with
the `0x121A0` phases and the pin. `README.md`: the sub-project table gains
4a-ii, and the verification section gains `make title-oracle` / `make title-pin`.

- [ ] **Step 3: Final ladder, ledger, and commit**

Run: `make verify` (must be exit 0) and
`python3 tools/gen_symbols.py port/decomp port/src/symbols.h` then
`git diff --quiet -- port/src/symbols.h`.

Record the cycle in `.superpowers/sdd/2026-09-17-actor-system/progress.md`
(created by the SDD run) with the deferred minors and residuals.

```bash
git add docs/superpowers/plans/2026-09-17-actor-system-report.md port/RE_GUIDE.md \
        port/spec/game_flow.md README.md
git commit -m "docs: sub-project 4a-ii report and the docs the title change invalidates"
```

---

## Self-review notes (run before execution)

1. **Spec coverage.** §2 in-scope items each map to a task: RNG → Task 3; pool +
   reset → Task 4; spawn → Task 5; pset sync → Task 6; anim interpreter →
   Task 7; the 4a-i insert call → inside Task 5's spawn emit; title → Task 9;
   oracle → Tasks 1, 2, 10; the `flow.c` comment corrections → Task 9; the
   `DAT_000BBDC8` correction → Task 9. Spec §5.3 (`res_alloc`) is deliberately
   not implemented — see Task 4's note.
2. **Placeholders.** None: every code step carries its code; every
   transcription step names a `prage.c` line range and the Format reference
   section that fixes its semantics.
3. **Type consistency.** `rng_next(u32)`, `rng_step(void)`,
   `actor_spawn(const u32*, u32, u32, u32, u32)`, `actors_update(void)`,
   `actor_pset(u32)`, `anim_next_sprite_id(u32, const u16*)` are used with the
   same signatures in every task that names them.

## Execution handoff

**1. Subagent-Driven (recommended)** — a fresh subagent per task, review between
tasks, fast iteration.

**2. Inline Execution** — execute in this session with `executing-plans`, batch
with checkpoints.

Task 1 and Task 2 are the spec's rung-settling tasks and should be reviewed
before any actor code is written.
