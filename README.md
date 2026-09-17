# Primal Rage (DOS, 1995) — reverse engineering

Reverse engineering of the PC/DOS release of **Primal Rage** (Time Warner
Interactive / Probe Software / Teeny Weeny Games, 1995), targeting
`PRAGE.EXE` and the `S16*.GRA` graphics set.

It is a **32-bit protected-mode WATCOM C/C++32 program** shipped as a
**Microsoft DOS/4GW *bound* Linear Executable (LE)**. There is no 16-bit real
mode game code: everything interesting lives in two LE objects (code + data).

## Layout

| Path | What |
|---|---|
| `data/game/C/` | Installed game (`PRAGE.EXE`, `INDEX`, `S16*.GRA`, sound drivers) |
| `data/game/CD/RAGECD.ISO` | Original CD (`/Volumes/RAGECD` when mounted: `RAGE.S04`, `RAGE.S08`, `RAGE.S16`, `RAGE.SND`) |
| `port/` | **SDL3 engine-core port** (sub-project 1) — `cmake -S port -B build` |
| `port/RE_GUIDE.md` | Address conventions, DOS/4GW layout, toolchain, landmarks |
| `port/spec/game_flow.md` | Entry, frame loop, state machine, tick, pixel path |
| `port/PORTING.md` | Porting rules (memory model, `mem[]` discipline, `fn_resolve`) |
| `port/DECOMPILATION.md` | Exact dependencies and steps used to produce `port/decomp/` |
| `port/decomp/prage.c` | Ghidra decompilation of every function (via the lx-loader) |
| `port/decomp/prage.functions.csv` | Function index: entry, size, callers, callees |
| `port/decomp/prage.strings.csv` | Defined strings with cross-references |
| `port/decomp/prage.symbols.csv` | User-defined / imported symbols |
| `FORMATS.md` | Decoded on-disk formats (LE layout, `INDEX`, `GRA`) |
| `docs/superpowers/` | Sub-project specs, plans and the engine-core report |
| `tools/le_info.py` | Dump the LE header/objects and decode `INDEX` |
| `tools/gra_render.py` | Independent GRA decoder + frame oracle (`--indices`, `--frame`) |
| `tools/gen_symbols.py` | Generate `port/src/symbols.h` from the decompilation |
| `_tools/ghidra_scripts/` | Ghidra headless scripts used to produce the artefacts |
| `Makefile` | `make help` — PORT (`build`/`test`/`check`/`run`/`verify`) and RE targets |

## Toolchain

* **Ghidra 12.2** with the [`ghidra-lx-loader`](https://github.com/yetmorecode/ghidra-lx-loader)
  extension (LE/LX loader, DOS/4GW aware). The loader maps object 0 to
  `0x10000` (code) and object 1 to `0x80000` (data) and applies the LE fixups,
  so no manual image extraction is needed. It is installed at
  `~/ghidra_12.2_DEV/Extensions/Ghidra/ghidra-lx-loader` and
  `~/Library/ghidra/ghidra_12.2_DEV/Extensions/ghidra-lx-loader`.
* **JDK 25** (`temurin-25`) for Ghidra 12.2.
* **dosbox-x** for ground truth (memory layout, runtime behaviour).
* Python 3 with `capstone` for ad-hoc disassembly checks.

### Regenerate the decompilation

```bash
export JAVA_HOME=/Library/Java/JavaVirtualMachines/temurin-25.jdk/Contents/Home
G=/Users/felipe.dos.santos/ghidra_12.2_DEV/support/analyzeHeadless

# one-shot import + analysis (the loader auto-detects the LE)
$G _tools/ghidra_proj prage -import data/game/C/PRAGE.EXE -overwrite \
   -scriptPath _tools/ghidra_scripts

# make .object1 executable, create the entry function, export everything
$G _tools/ghidra_proj prage -process PRAGE.EXE \
   -scriptPath _tools/ghidra_scripts \
   -postScript FixupProgram.java \
   -postScript ExportDecomp.java port/decomp/prage.c 90 \
   -postScript ExportMeta.java port/decomp/
```

## Status

**Reverse engineering.** `PRAGE.EXE` loads and analyses cleanly: **~1350
functions** decompiled, 0 failures. LE layout and the `INDEX` container are
verified; the `S16*.GRA` chunk types 2/5/6 are fully decoded (`FORMATS.md`).
The entry/startup (`0x624D4`), the game main (`0x1BEC4`), the real frame loop
(`0x255CC` → `0x24C5C` → `0x11D04`), the tick (60.05 Hz measured) and the
literal-`0xA0000` framebuffer write path are pinned — see `port/spec/game_flow.md`.

**SDL3 port — sub-project 1, engine core: boot → title screen, running.** The
port reimplements the engine in C over a flat `mem[]` that holds the original
LE data image at its original addresses, reads the original `data/game/C/`
assets at runtime, and SDL3 appears only in `host.c`/`main.c`. Ported and
verified: the LE loader + fixups (byte-exact against Ghidra's image), the
`INDEX` resource manager, `fn_resolve` code-address mapping, GRA decode
(byte-exact against `tools/gra_render.py`), the palette flush (`0x1C470`), the
process-table scheduler, and the `0x255CC`/`0x24C5C`/`0x11D04` loop running the
title state. Audio, Smacker, menus/EEPROM and the fight engine are stubbed at
their call sites and are the remaining sub-projects (`/* PORT: */` markers).

### Build and run

```bash
cmake -S port -B build && cmake --build build     # or: make build
./build/prageport --game-dir data/game/C          # windowed; ESC to quit
./build/prageport --game-dir data/game/C --check 60
```

`--check N` is a headless mode: it runs exactly N master-loop iterations with no
window and writes `frame_NNNN.ppm` (RGB), `frame_NNNN.pal` (the DAC) and
`frame_NNNN.idx` (raw indices) per frame, exiting non-zero on an internal
assertion failure.

### Verify

```bash
PR_ORACLE_REQUIRED=1 ./build/run_tests            # or: make verify
```

`PR_ORACLE_REQUIRED=1` is required for a real verification run: the byte-exact
Ghidra/title-screen oracles are git-ignored (they are copies of the game's own
bytes), so without it the suite skips those comparisons. `make verify` runs the
full ladder (oracle-required tests, `--check`, `symbols.h` idempotence).
