# Reverse-engineering guide — Primal Rage (DOS)

Conventions, the DOS/4GW memory model, and the toolchain. Read together with
`../README.md` and `../FORMATS.md`.

## Target

`PRAGE.EXE` is a **32-bit protected-mode** program:

* Compiler/runtime: **WATCOM C/C++32** (the `WATCOM C/C++32 Run-Time system`
  copyright string is in the code object).
* Container: Microsoft **DOS/4GW *bound* Linear Executable (LE)** — an MZ stub
  followed by an embedded LE image (`file(1)`: *LE for MS-DOS, DOS4GW DOS
  extender (embedded)*).
* No real-mode game code. The MZ stub only loads DOS/4GW, which maps the two
  LE objects and jumps to the protected-mode entry.

### Verified LE facts (`tools/le_info.py data/game/C/PRAGE.EXE`)

| Item | Value |
|---|---|
| LE header file offset | `0x290A4` |
| CPU / OS | 2 (i286+) / 1 (DOS) |
| Pages | 213 × `0x1000` |
| Entry (`eip`, obj 1) | `0x5245C` |
| Stack (`esp`, obj 2) | `0x8B0D0` |
| Object 0 (code) | base `0x10000`, virtual size `0x63B15`, 100 pages, flags `read|exec|preload|32-bit` |
| Object 1 (data) | base `0x80000`, virtual size `0x8B0D0`, 113 pages, flags `read|write|preload|32-bit` |
| Data-pages offset | `0x3C800` (relative to the embedded MZ image base at file `0x26654`, not to the LE header; page data begins at `0x62E54`) |

## Address conventions

* **Ghidra address = linear address = LE object base + object offset.** There
  is no segment:offset split; WATCOM uses flat 32-bit addressing.
* A function Ghidra calls `FUN_0001c500` is at **code-object offset `0x1C500`**
  and runs at linear `0x1C500`. Code object spans `0x10000`–`0x73B14`.
* A global `DAT_0008xxxx` is at **DS offset `0x0xxxx`** in the data object:
  subtract `0x80000`. E.g. `DAT_00080004` = `DS:0004` (start of the string
  table). Data object spans `0x80000`–`0x10B0CF`.
* The `.image` block is the unmodified file mapped as an overlay; its
  `FUN_.image__*` symbols are analysis noise — **ignore them** (1207 of 1352
  functions are real code).

### Entry / startup

* LE entry `0x6245C` is `jmp 0x624D4` (2 bytes, immediately followed by the
  `WATCOM C/C++32 Run-Time system` copyright string in rodata).
* `0x624D4` is the WATCOM/DOS4GW startup (`_start`): it saves the register
  state, uses `int 21h`/`int 31h` (DOS/DPMI reflection) to read the PSP
  command tail and environment, builds `argc/argv`, then calls the C `main`.
* Interrupts seen in the decompilation: `int 21h` (DOS/DPMI), `int 31h` (DPMI),
  `int 10h` (BIOS video), `int 33h` (mouse), `int 16h` (keyboard), plus
  direct port I/O for the sound drivers.

## What the game reads at runtime

* `INDEX` — resource index (`name → size|flags`); see `FORMATS.md`.
* `S16*.GRA` — graphics sets, loaded by name from the current directory.
* `\RAGE.S16` — CD/relative path string (data object).
* `english.txt`, `french.txt`, `german.txt`, `italian.txt`, `spanish.txt`,
  `portugal.txt` — localisation text.
* `DIG.INI`, `MDI.INI`, `RM.DRV`, `SB16.DIG` / `SBPRO.DIG` / `SBLASTER.DIG`,
  `FAT.OPL`, `FAT.AD` — Miles/AIL sound-driver set. `FUN_00010034` inits the
  driver and falls back through `SB16.DIG → SBPRO.DIG → SBLASTER.DIG`.
* `twi5.smk`, `twg.smk` — Smacker video (logos/intro).

## Toolchain

* Ghidra 12.2 + [`ghidra-lx-loader`](https://github.com/yetmorecode/ghidra-lx-loader).
  The loader maps the objects at their LE rel bases and applies the LE fixups,
  so the decompilation already has correct absolute data references.
* `dosbox-x` for ground truth. Because DOS4GW relocates the image, the fastest
  way to learn a runtime address is to run the game with a `memory file`
  (`[dosbox] memory file = /tmp/prage.mem`) and search the dump, or to set a
  breakpoint in the debugger. The file↔memory mapping has a constant delta per
  LE object (measured in this session: code object `file + 0x19E1AC`,
  data object `file + 0x19F1AC`, with the image loaded around `0x200000`).

### Useful commands

```bash
G=/Users/felipe.dos.santos/ghidra_12.2_DEV/support/analyzeHeadless
export JAVA_HOME=/Library/Java/JavaVirtualMachines/temurin-25.jdk/Contents/Home

# import + full analysis
$G _tools/ghidra_proj prage -import data/game/C/PRAGE.EXE -overwrite -scriptPath _tools/ghidra_scripts

# fix perms/entry and re-export
$G _tools/ghidra_proj prage -process PRAGE.EXE -scriptPath _tools/ghidra_scripts \
   -postScript FixupProgram.java \
   -postScript ExportDecomp.java port/decomp/prage.c 90 \
   -postScript ExportMeta.java port/decomp/

python3 tools/le_info.py data/game/C/PRAGE.EXE
python3 tools/le_info.py --index data/game/C/INDEX
```

## Landmarks found so far

| Address | Meaning |
|---|---|
| `0x6245C` | LE entry → `jmp 0x624D4` |
| `0x624D4` | WATCOM/DOS4GW `_start` (PSP/env parsing, builds `argc/argv`) |
| `0x6C435` | CRT `main` wrapper → `0x1BEC4` |
| `0x1BEC4` | **game main** (init chain + dispatch) |
| `0x20C10` | startup orchestrator → master loop |
| `0x1BE30` | teardown |
| `0x10034` | sound-driver init (`SB16.DIG`/`SBPRO.DIG`/`SBLASTER.DIG`) |
| `0x1B120` | `INDEX` loader / resource-table builder |
| `0x1B544` | resource-handle → pointer (30 callers) |
| `0x1C0F0` | extended-memory probe + memory-block list (EMS; unused by the flat port) |
| `0x2C3FC` | most-called function (206 callers) — core engine helper |
| `0x255CC` | master frame loop |
| `0x24C5C` | per-frame update (frame counter, player records, update process table) |
| `0x11D04` | state machine `switch(DAT_000F0A64)` |
| `0x11000` | per-state render/play dispatch (13 cases) |
| `0x1C740` | VBlank-gated animation/FLIC-style blit of `DAT_000E87A4` (**not** the screen write) |
| `0x1C470` | palette dirty-list flush → VGA DAC |
| `0x2D62C` | tick handler `DAT_00105D88++`; rate 60.05 Hz measured, **interrupt vector unresolved** |
| `0x51F45` | 320x200 double-buffer + scanline-table setup (`&DAT_001088F8`) |
| `0x50188` | swap back/fore buffers `DAT_000E87A0` ↔ `DAT_000E87A4` |
| `0x501A3` | dirty-dword blit `DAT_000E87A4` → literal aperture `0xA0000` |
| `0xA0000` | screen-write target, a **literal immediate** (no LE fixup) — literal-target and not-a-data-object are verified; the "VGA mode-13h aperture" reading is inferred (see `spec/game_flow.md`, verified-with-caveat). Hardware, never `mem[0xA0000]` |
| `PTR_FUN_000A8644` / `_DAT_00104AE8` | update process table / bitmask |
| `PTR_FUN_000A86C4` / `_DAT_00104AEC` | render process table / bitmask |
| `0x1C500` | small leaf called 181× — likely a getter/accessor |
| `0x2BC30`, `0x2AE14`, `0x2F198` | very hot code (150–190 callers) |
| `0x5D7DC` | **not audio**: the Watcom C runtime `rand()` — a 32-bit LCG `seed = seed*0xB90D12B9 + 0x38CE051F`. 33 game callers, 0 AIL callers (spec `audio.md` "AIL surface"). Earlier "allocator/memory helper" was wrong |
| `0x5D87E`, `0x5D973`, `0x5DB9E` | sound driver API used by `0x10034`; `0x5D973` is AIL's driver dispatcher (`swi 0x31`), not game-called |
| `0x1CF40` | AIL init (sub-project 2a): `AIL_startup`, prefs, 4 sample handles, sequence handle, 60 Hz timer |
| `0x1CF20` | master-loop audio service: play queued samples (`0x1CB18`), start pending song (`0x1C930`), advance/sequence + render |
| `0x1C930` | load and start the pending song (`AIL_init_sequence`/`set_volume`/`start_sequence`) |
| `0x5D851`–`0x5DFFF` | AIL public-API thunk block (33 AIL + 3 non-AIL; the game→audio boundary) |
| `0x5DE48` | AIL XMIDI loader (`FORM`/`CAT`/`XMID` container parse) |
| `0x6FB28` | largest function (4979 bytes) |

The code is dense from roughly `0x10000`–`0x39000` (engine/utilities) and
`0x3A000`–`0x6A000` (game logic), with libraries at the high end
(`0x5C000`+ looks like the WATCOM runtime / DOS4GW glue). The subsystem split is
in `port/spec/`; `game_flow.md` covers the loop, state machine and frame path.

## Next steps

1. Subsystem split by call graph. ~~Identify the main loop~~ — done:
   `0x1BEC4` → `0x20C10` → `0x255CC` (loop) → `0x24C5C` → `0x11D04` (state
   machine) → `0x11000`; the frame write/present path is the literal `0xA0000`
   copy (`0x255CC` full copy / `0x501A3` dirty blit) from `DAT_000E87A4`. See
   `spec/game_flow.md`.
2. ~~Decode `S16*.GRA` fully~~ — done: chunk types 2/5/6 decoded (Task 8),
   implemented in `port/src/platform/gra.c`. The port matches
   `tools/gra_render.py` by **exact consumption** (18,201/18,202 descriptors
   across all 69 files) and byte-for-byte only for the four full-screen
   `S16TITLE` frames `{10,12,13,18}`; the per-sprite sub-palette/DAC base and
   the descriptor `x`/`y` anchor stay `likely`, not proven. See `FORMATS.md`.
3. Name the hot core functions (`0x2C3FC`, `0x2BC30`, `0x1C500`, `0x2AE14`).
4. Pin the tick **interrupt vector** (the rate is measured at 60.05 Hz; the
   framebuffer write path has since been resolved as a literal `0xA0000`).
5. The SDL3 port lives in `port/`: engine core (sub-project 1) and audio/AIL
   (sub-project 2a, report at
   `../docs/superpowers/plans/2026-09-16-audio-ail-port-report.md`). Smacker
   (2b), menus/EEPROM (4) and the fight engine (5) remain; the run-time AIL
   sound-id table (`DAT_000bbdc8`) is still unextracted.
