# Design: Primal Rage (DOS) → SDL3 port — sub-project 1, engine core

Date: 2026-09-16
Status: approved design, pending user review of this document

Part of a five-spec decomposition (see "Sub-project split" below). This spec
covers **sub-project 1 only**.

## Context

`PRAGE.EXE` is 32-bit protected-mode WATCOM C/C++32 code (1995), shipped as a
DOS/4GW *bound* Linear Executable. `port/decomp/` holds a complete Ghidra
decompilation (1352 functions, 0 errors) produced with the
`ghidra-lx-loader`. `FORMATS.md` covers the LE container and the `INDEX`
resource table; `port/spec/game_flow.md` covers entry/startup.

The target is a **faithful reimplementation**, the same model as the sibling
project `test-drive-sdl3/tdport`: original data files are read at runtime,
behaviour and integer arithmetic match the original, and SDL appears only in
`host.c` and `main.c`.

End goal of the overall project: a playable SDL3 port. End state of this
sub-project: **boot → title screen on real assets**, with the engine's real
frame loop running.

## Sub-project split

| # | Sub-project | Content |
|---|---|---|
| **1** | **Engine core** (this spec) | flat memory model, LE data loader, resource manager, frame loop, timing, GRA decode, palette + present, input |
| 2 | Smacker video | `twi5.smk`, `twg.smk` decoder + present |
| 3 | Audio | faithful AIL/Miles API reimplementation over SDL audio (`.DIG`/`.MDI`/`FAT.OPL`) |
| 4 | Menus / character select | input FSM, localisation text, settings, EEPROM high-score |
| 5 | Fight engine | animation interpreter, stage backgrounds, fighter state, demo AI |

Ordering follows data dependency: 1 is the dependency root (and retires the
two riskiest unknowns, the frame loop and the GRA type-2 pixel encoding); 5 is
the largest unknown but cannot start without 1.

Deferred but in scope for the overall project: the full attract sequence
(title, menus, character select, self-running demo fight) and faithful audio.

## Section 1 — Scope

### Definition of done

1. `main` → `0x1BEC4` init chain → real loop → title screen → clean exit,
   driven by the reconstructed engine, reading `data/game/C/` original files.
2. `INDEX` parsed; resource handles resolve (`entry = table + (handle>>23)*0x14`,
   `ptr = entry.data + (handle & 0x7FFFFF)`), matching `FUN_0001B544`.
3. GRA chunk types 2/5/6 decoded; title-screen pixels and palette match a
   dosbox-x screenshot **3-way**: port output, independent Python decoder, and
   the emulator.
4. The reconstructed loop is the original's: `0x255CC` → `0x24C5C` →
   `0x11D04`, paced by the original tick source, exit path through `0x1BE30`.
5. `--check` non-windowed mode: fixed number of frames, writes
   `frame_NNNN.ppm`, non-zero exit if an assertion fails.
6. `ESC` exits cleanly through the ported teardown path (`0x1BE30`); whether
   the original handles `ESC` at that point, or the port forces the exit, is
   resolved in the plan (see "Entry route for this sub-project").

### Non-goals (own sub-projects)

Audio and AIL (3); Smacker (2); menus, character select, localisation,
settings, EEPROM (4); fight engine, animation, stages and demo AI (5).

Not silently omitted: every call on the loop path that belongs to a later
sub-project is stubbed with an explicit `/* PORT: <sub-project> */` comment and
returns immediately. Nothing is left out by accident.

### Entry route for this sub-project

The real boot order runs Smacker logos (sub-project 2) and menus (sub-project 4)
before the title screen, and neither exists yet. So SP1 reaches the title screen
by driving the state machine directly: set `DAT_000F0A64` to the title state
and let the loop run it, with the sub-project-2/4 states stubbed.

Identifying **which** state index is the title screen is therefore a plan task
(read the `0x11D04` case bodies; confirm in dosbox-x by reading `DAT_000F0A64`
while the title is displayed).

`ESC` at the title may or may not be handled by the original. If it is not, the
port's exit is a deliberate deviation marked `/* PORT: exit at title */`
rather than a claimed original behaviour.

Main's other init callees are not yet classified — `0x47370` (369 B, also
called by `0x20C10`), `0x1CF40`, `0x1E2A0`, `0x336C0`, `0x5004A`. The plan
covers them in the init-chain task rather than assuming they are unimportant.

## Section 2 — Runtime model (verified)

Confirmed this session by reading the decompilation and the call graph.

```
main 0x1BEC4
 │  init: video query 0x4FBA2, hardware probe, printf 0x62734,
 │        DPMI region locks 0x109A0, screen surface 0x51F45
 │        (0, 200, &DAT_001088F8, 0x140, -1, 1, 0)  → 320x200 offscreen buffer
 │        at 0x1088F8; globals DAT_000E87A0 / DAT_000E87A4
 └─ 0x20C10  (483 B, 21 callees — startup orchestrator)
     └─ 0x255CC   ★ MASTER FRAME LOOP (293 B)
         do {
           FUN_000500C4();                     input / OS pump
           FUN_000292AC();
           if (DAT_00107A54) { FUN_000389C4(); FUN_00038A38(); }
           FUN_00024C5C();                     per-frame update
           if (_DAT_00104AEC) {                process table 2 (render)
             for each set bit: (**(code **)((int)&PTR_FUN_000a86c4 + i*4))();
           }
           DAT_00104AF4++;                     frame counter
           FUN_000134C0();
           frame pacing on DAT_0010150C / DAT_00101508
         } while (...);
         └─ 0x24C5C   per-frame update
             DAT_000EF6DC++;                                     frame counter
             2 player records, base DAT_001077E0, stride 0x94
             if (_DAT_00104AE8) {                process table 1 (update)
               for each set bit: (**(code **)((int)&PTR_FUN_000a8644 + i*4))();
             }
             └─ 0x11D04  ★ STATE MACHINE (546 B)
                 switch (DAT_000F0A64)  cases 1..9, transitions via
                 DAT_000F0A64 / DAT_000F0A6C / DAT_000F0A6A / DAT_000F0A6F /
                 DAT_000F0A72; per-state work 0x121A0, 0x11F6C, 0x12484,
                 0x11578, ...
                  └─ 0x11000  ★ per-state render/play dispatch (13-case switch)
                      └─ 0x1C740  animation/FLIC-style player, blits DAT_000E87A4
                          via FUN_00065340 after an `in(0x3DA) & 8` wait.
                          (NOT the frame loop's present — corrected in Task 13.)
```


**Process tables.** Two 32-entry callback tables, `0x80` bytes apart, each
gated by a `u32` bitmask: `PTR_FUN_000a8644` + `_DAT_00104AE8` (update, walked
in `0x24C5C`) and `PTR_FUN_000a86c4` + `_DAT_00104AEC` (render, walked in
`0x255CC`). Entries are 4-byte function pointers. This is the engine's process
scheduler, and it is **the** extensibility seam of the original.

**Timing.** `DAT_00105D88` is the tick counter, incremented by the 9-byte
handler `0x2D62C` (`DAT_00105D88++`). `main` locks `0x2D62C`'s code page and
the `DAT_00105D88` data page (`0x109A0(&DAT_00105D88, 0x1000)`), and also
`0x1B610` (+`0x4000`) and `0x62451` (+`0x1000`) — i.e. the ISR code plus the
shared data it touches. Frame pacing is a counter pair: `DAT_00101508`
(asynchronously advanced) and `DAT_0010150C` (loop-local), compared in
`0x255CC`. `0x1C740` separately busy-polls VBlank (`in(0x3DA) & 8`) around the
palette/present work. **So frame sync is ISR-tick + VBlank-poll, not a pure
software timer.** The exact interrupt vector is not yet pinned down and is a
plan task (see Risks).

**DPMI region lock/unlock** (resolves `game_flow.md` open question 3):
`0x109A0(ptr, size)` → `0x10830(ptr, ptr+size)`; the unlock twin is
`0x109CA` → `0x108E8`. Both thunk `0x61564`. 36 vs 29 callers. `main` locks 8
regions. In the port these become no-ops behind `mem_lock()` / `mem_unlock()`
(documented `/* PORT: no DPMI */`).

**Palette** (verified): `0x1C470` flushes a palette dirty-list to the VGA DAC.
Records are 4×`u32` at `DAT_00107498`, head pointer `DAT_00107798`, reset to
base after flush:

| field | meaning |
|---|---|
| `[0]` | colour pointer, or resource handle when `[3] != 0` (resolved by `0x1B544`, **+4** to skip a 4-byte header) |
| `[1]` | first DAC index (low byte) |
| `[2]` | colour count |
| `[3]` | non-zero → `[0]` is a resource handle |

Colour words are 8-bit guns packed R = bits[2..9], G = bits[10..17],
B = bits[18..25]. The function is VBlank-gated.

**Framebuffer write path — resolved, with a rule that constrains the memory
model.** An earlier note here claimed `0xA0000` had zero references and left the
path unknown; that was a **false negative** — the address appears as literal
immediates (`mov ebx,0xa0000` at `0x501A3`, `mov edi,0xa0000` in `0x255CC`),
which a symbol search misses. The game copies its 320x200 buffer to a constant
`0xA0000`: no VBE LFB, no banked `int 10h AX=4F05`, mode 13h.

That address is **not** data-object memory, and the port must not treat it as
such. The LE data object is *declared* at `0x80000` spanning through `0xA0000`,
and a flat mapping would put `mem[0xA0000]` at data-object offset `0x20000` —
which holds live state (`PTR_DAT_000A1290`, LUTs at `0xA1420`, and 1435 fixups).
But DOS/4GW relocates the image above 1 MB: a memory dump of the running game
shows `0xA0000..0xAFA00` as `0xFF` while the file's unique LUT bytes
(`00 80 c0 e0 f0 f8 fc fe`) sit at physical `0x287420` and the pointer table at
`0x287290` — a uniform relocation base of `0x266000`.

**Rule (bind Task 14 and later):** absolute addresses inside the VGA/BIOS window
`0xA0000`–`0xFFFFF` refer to **hardware**, not to the data object, because the
original's image is relocated above 1 MB. Screen and aperture writes must never
target `mem[0xA0000]`; the port keeps its frame buffer outside the mapped data
object and presents it through `gfx_present()`. `gfx_present()` needs no change
for this reason, but any ported code that copies to `0xA0000` must be redirected
to the port's frame buffer under a `/* PORT: ... */` marker.

## Section 3 — Address and memory model

Single flat buffer, original addresses preserved (32-bit flat, so no
segment:offset reconstruction is needed):

```c
static u8 mem[0x4000000];          /* 64 MB, sized to hold the resource set */
```

* Data object at its LE base `0x80000`, so `DAT_00107874` is `mem+0x107874`.
* Code object range `0x10000`–`0x73B15` is **reserved**; the port never
  *executes* those bytes (code is reimplemented in C), but the loader may
  populate the range so cross-object fixups resolve and the image can be
  validated against Ghidra.
* **64 MB, revised during implementation (was 16 MB).** `0x1B120` loads every
  `INDEX` resource eagerly; the shipped set is 69 entries totalling 41.31 MB
  (largest `s16rex.gra`, 3.64 MB), which is why the original probes extended
  memory and keeps a memory-block list. Sizing the flat space to fit is
  preferable to emulating EMS. All original addresses are unchanged — only the
  ceiling grows.
* Room to spare rather than the exact end: no allocation-size-dependent bounds
  arithmetic, and a stray original pointer stays inside the buffer.
* Writes outside `mem[]` are a bug; debug builds bounds-assert.

Accessors (offset **includes** the `0x80000` base):

```c
#define DSB(o) (*(u8  *)(mem + (o)))
#define DSW(o) (*(u16 *)(mem + (o)))
#define DSD(o) (*(u32 *)(mem + (o)))
#define DSP(o) (*(void **)(mem + (o)))
```

**Loading.** `mem[]` is initialised from the original LE image, **not**
zero-filled — original data tables hold absolute pointers to other globals, so
a raw file copy would produce wrong pointers:

```c
void mem_load_le(const char *exe_path);   /* MZ → LE header → page map → objects → apply fixups */
```

~100 lines of C. **What already exists and what does not:** `tools/le_info.py`
has verified the header fields, the object table and the page-map offset, so
that parsing is ported, not re-derived. It does **not** walk the page map,
extract the objects or apply fixups (its line-64 comment about writing a flat
image is stale — no code does it). Those parts are new code.

Oracle for the new parts: the `ghidra-lx-loader` applies the fixups when it
loads the file, so **Ghidra's mapped memory is the reference image**.
`_tools/ghidra_scripts/DumpBytes.java` already dumps hex bytes at given
addresses from that image, so `mem_load_le()` is validated by comparing `mem[]`
against a `DumpBytes` dump of the data object.

The code object is skipped (its range stays reserved). No generated asset
blobs: the port runs on original data.

**Code addresses in data.** The original stores function addresses in data
(e.g. `main` passes `FUN_0002D62C` and `FUN_0001B610` plus lengths to the
region lock, and the process tables hold 32 code pointers each). Those
addresses need stable identity without executable bytes in `mem[]`:

```c
intptr_t fn_resolve(u32 orig_addr);   /* original address → C function pointer, else 0 */
u32      fn_origin(void *c_fn);       /* reverse, for storing into mem[] */
```

Writes of a function address into `mem[]` go through `fn_origin()`; indirect
calls read through `fn_resolve()`. Known limitation: breaks if the original
does arithmetic on code addresses. Revisit if sub-project 5's jump tables need
it.

**State ownership.** All original globals live in `mem[]` at their original
offsets and are never shadowed by long-lived C globals. `symbols.h` names
them, generated by `tools/gen_symbols.py` from `port/decomp/*.csv`. Offsets
without a name are written raw with a trailing comment (tdport rule).

**Integer widths.** `u8/s8/u16/s16/u32/s32` exactly as the original register
widths, with explicit casts at every truncating step; signed shift is
`(s16)x >> n`. Carry/borrow-dependent arithmetic is emulated explicitly. This
sub-project needs little of it, but the rule is adopted now so later
sub-projects do not have to retrofit.

## Section 4 — Module layout

Sibling `tdport` layout. SDL is included **only** in `host.c` and `main.c`.

```
port/CMakeLists.txt
port/PORTING.md              rules, adapted from tdport (mem[] discipline, fn_resolve)
port/src/main.c              args, --game-dir, --check, host_init, game_main
port/src/types.h             u8/s8/u16/s16/u32/s32
port/src/mem.h/.c            mem[], DSB/DSW/DSD/DSP, fn_resolve, mem_load_le
port/src/host.h/.c           SDL3 window/present, vblank wait, key queue, files
port/src/symbols.h           GENERATED from ../decomp/*.csv
port/src/platform/gfx.*      dac[256][3], palette flush, present, blit/fill
port/src/platform/res.*      INDEX parse, handle resolve, DOS block alloc, file load
port/src/platform/gra.*      GRA chunk walk + type 2/5/6 decode
port/src/platform/input.*    int 16h keyboard, int 33h mouse
port/src/game/flow.*         0x1BEC4 init chain, the loop, state machine, title screen
```

Modules own one job each and talk through `host.h` / their own headers:

| module | responsibility | depends on |
|---|---|---|
| `mem` | flat address space, LE load, code-address mapping | — |
| `host` | SDL3, files, ticks, present calls | SDL3 |
| `platform/res` | `INDEX`, handle resolution, allocation | `mem`, `host` |
| `platform/gra` | GRA decode | `res`, `mem` |
| `platform/gfx` | palette, present, blits | `host`, `mem` |
| `platform/input` | keyboard/mouse | `host` |
| `game/flow` | init chain, loop, state machine, title | all of the above |

## Section 5 — Pixel path

**Palette.** Ported, not reinvented: `0x1C470` becomes `gfx_flush_palette()`,
its `out(0x3C8/0x3C9, …)` writing `dac[256][3]` instead of ports. The
dirty-list semantics, the `+4` resource offset and the VBlank gate are all
preserved. `present_frame()` converts through `dac`.

**Frame sync.** `host_wait_vblank()` is the single mapping point for the
original's `in(0x3DA) & 8` spin, and it calls `host_pump()` (tdport rule:
every original busy-wait pumps the host). The original tick rate and pacing
counter pair are preserved rather than replaced by a fixed timestep.

**Present.** One `present_frame()` seam, so sub-projects 2–5 never touch it.
The discovery task above determines what it must do; candidates are VBE LFB
pointer, banked `4F05` writes, or `rep movs` to a stored pointer. Until then,
the port renders the offscreen buffer to the window directly.

**GRA decode, two tracks.** Neither is trusted alone:

1. *Authoritative* — find the in-game consumer of chunk 2: the callers of
   `0x1B544` that blit, paired with chunk 5's `count`-entry table. Working
   hypothesis for the model: chunk 5 = per-frame `(offset, size)` pairs into
   chunk 2, chunk 6 = palette (consistent with the `+4` header skip in
   `0x1C470`, and with chunk 6 being last).
2. *Oracle* — independent decoder in `tools/gra_render.py`; hypothesis →
   render → pixel-diff against a dosbox-x screenshot of the same screen.

`platform/gra.*` is shared by sub-projects 1–5 and mirrors the Python tool.

## Section 6 — Verification

Cheapest sufficient proof, in order:

1. Constructs: each file compiles warning-free
   (`-Wall -Wextra -Wno-unused-parameter -fno-strict-aliasing`);
   single-file check with `-fsyntax-only`.
2. Static: LE header/object-table fields cross-checked against
   `tools/le_info.py`; the loaded `mem[]` image cross-checked against a
   `DumpBytes.java` dump of Ghidra's fixup-applied memory.
3. Three-way asset agreement: `tools/gra_render.py` ≡ `platform/gra.*` ≡
   dosbox-x screenshot (pixels and DAC values).
4. `--check` self-comparison: fixed frame count, PPM out, per-frame asserts.
5. Debug builds bounds-assert every `mem[]` access.
6. Deviations marked `/* PORT: … */`; doubts marked `/* TODO(verify): … */`
   and listed in the sub-project report.

No test framework; assertion-based self-checks plus the PPM comparison, as in
`tdport`.

## Section 7 — Risks

| # | Risk | Mitigation |
|---|---|---|
| 1 | Exact interrupt vector for the tick ISR is unidentified; if the loop's pacing depends on ISR-side state beyond the counter, the host tick model changes. | Plan task 1 pins the vector (installer search + dosbox-x breakpoint) before any porting of the loop. |
| 2 | GRA chunk 2 may be delta/compressed across frames rather than standalone bitmaps. | Decode frames in order; if the title screen needs sequential state, it is still in scope; if it needs the fight engine, that part moves to sub-project 5 and is stubbed. |
| 3 | The `int 10h` mode `0x13` gate (320x200) versus the installed `S16` (640x480) asset set is still contradictory — unresolved for Task 14. (The framebuffer write path itself is now **resolved**: a constant `0xA0000`, which is the VGA aperture because DOS/4GW relocates the image above 1 MB — see the memory-model rule above.) | Named discovery task for the resolution question; `present_frame()` isolates it. |
| 4 | `fn_resolve` breaks if the original does arithmetic on code addresses. | Documented limitation; revisit when sub-project 5's jump tables need it. |
| 5 | LE fixups may include intra-object references that are mis-applied. | `mem_load_le()` asserts + the three-way agreement in verification step 3. |

## Section 8 — Docs this sub-project updates

* `port/spec/game_flow.md` — loop, state machine, tick, and region-lock
  findings; open questions 1 and 3 resolved, 2 and 4 remain.
* `FORMATS.md` — GRA chunk 2/5/6 once decoded.
* `README.md` / `port/RE_GUIDE.md` — status and landmarks
  (`0x255CC`, `0x24C5C`, `0x11D04`, `0x11000`, `0x1C740`, `0x1C470`, `0x51F45`).
