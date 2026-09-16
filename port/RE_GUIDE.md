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
| Data-pages offset | `0x3C800` (relative to the LE header) |

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
| `0x10034` | sound-driver init (`SB16.DIG`/`SBPRO.DIG`/`SBLASTER.DIG`) |
| `0x2C3FC` | most-called function (206 callers) — core engine helper |
| `0x255CC` | master frame loop |
| `0x24C5C` | per-frame update (frame counter, player records, update process table) |
| `0x11D04` | state machine `switch(DAT_000F0A64)` |
| `0x11000` | per-state render/play dispatch (13 cases) |
| `0x1C740` | present / flip (VBlank-gated) |
| `0x1C470` | palette dirty-list flush → VGA DAC |
| `0x2D62C` | tick handler `DAT_00105D88++` (vector TBD) |
| `0x51F45` | 320x200 screen-surface setup (`&DAT_001088F8`) |
| `0x1C500` | small leaf called 181× — likely a getter/accessor |
| `0x2BC30`, `0x2AE14`, `0x2F198` | very hot code (150–190 callers) |
| `0x5D7DC` | 113 callers — likely allocator/memory helper |
| `0x5D87E`, `0x5D973`, `0x5DB9E` | sound driver API used by `0x10034` |
| `0x6FB28` | largest function (4979 bytes) |

The code is dense from roughly `0x10000`–`0x39000` (engine/utilities) and
`0x3A000`–`0x6A000` (game logic), with libraries at the high end
(`0x5C000`+ looks like the WATCOM runtime / DOS4GW glue). A proper subsystem
split is still to be written (see "Next steps").

## Next steps

1. Subsystem split by call graph (cluster by caller/callee) and write
   `port/spec/<subsystem>.md` files, following the format in the sibling
   `test-drive-sdl3/port/RE_GUIDE.md`. ~~Identify the main loop~~ — done:
   `0x1BEC4` → `0x20C10` → `0x255CC` (loop) → `0x24C5C` → `0x11D04` (state
   machine) → `0x11000` → `0x1C740` (present); see `spec/game_flow.md`.
2. Decode `S16*.GRA` fully (sprite/tile container) — see `FORMATS.md`.
3. Name the hot core functions (`0x2C3FC`, `0x2BC30`, `0x1C500`, `0x2AE14`).
4. Pin the tick interrupt vector and the framebuffer write path (the present
   path does not reference `0xA0000` directly).
5. The SDL3 port work lives in `docs/superpowers/specs/` and, from sub-project
   1 onward, in a sibling `port/` CMake project.
