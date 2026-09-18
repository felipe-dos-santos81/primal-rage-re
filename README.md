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
| `port/` | **SDL3 port** (engine core, sub-project 1) + **audio/AIL** (sub-project 2a) + **Smacker video** (sub-project 2b-i) + **sprite compositor** (sub-project 4a-i) + **actor system and title** (sub-project 4a-ii) — `cmake -S port -B build` |
| `port/src/platform/audio/` | AIL surface, XMIDI sequencer, FAT.OPL, samples, mixer, vendored OPL core |
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
| `tools/gra_extract.py` | Extract every S16 sprite to RGBA PNG + `manifest.json` (`make re-extract`) |
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
* Python 3 with `capstone` for ad-hoc disassembly checks and `Pillow` for `tools/gra_extract.py`.

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
verified; the `S16*.GRA` chunk types 2/5/6 are decoded (`FORMATS.md`) — the
per-sprite sub-palette/DAC base and the descriptor `x`/`y` anchor remain
`likely`, not proven.
The entry/startup (`0x624D4`), the game main (`0x1BEC4`), the real frame loop
(`0x255CC` → `0x24C5C` → `0x11D04`), the tick (60.05 Hz measured) and the
literal-`0xA0000` framebuffer write path are pinned — see `port/spec/game_flow.md`.

**SDL3 port — sub-project 1, engine core: boot → title screen, running.** The
port reimplements the engine in C over a flat `mem[]` that holds the original
LE data image at its original addresses, reads the original `data/game/C/`
assets at runtime, and SDL3 appears only in `host.c`/`main.c`. Ported and
verified: the LE loader + fixups (byte-exact against Ghidra's image), the
`INDEX` resource manager, `fn_resolve` code-address mapping, GRA decode
(exact-consumption primary — 18,201/18,202 descriptors across all 69 files — and
byte-identical to `tools/gra_render.py` for the four full-screen `S16TITLE`
frames `{10,12,13,18}` only; the palette bank is flattened and the sub-palette
choice is `likely`), the palette flush (`0x1C470`), the process-table scheduler,
and the `0x255CC`/`0x24C5C`/`0x11D04` loop running the title state. The port's
title renders those four asset frames full-screen; the original's task-system
composite is not reproduced, and the emulator comparison is unusable (Task 15).
**Audio — sub-project 2a, AIL/Miles, running.** The port runs the game's own
audio path with no DOS driver: the `0x1CF40` AIL init completes, the title/
attract XMIDI bank is decoded and sequenced into OPL register writes through a
vendored FM core, one located announcer sample plays through the game's own
sample request/play call path, and mixed stereo frames reach SDL audio when a
device opens. The FM data path is proven byte-exact against an independent
Python decoder (9340 OPL register writes, zero differences); the original's OPL
trace was captured and governs the comparison, which the port does **not** match
structurally (the driver's reconstruction semantics are unverified). The windowed
run is silent on hosts where SDL audio cannot start — on this machine `-66681`.
See `docs/superpowers/plans/2026-09-16-audio-ail-port-report.md`.
**Video — sub-project 2b-i, Smacker logos, running.** The port decodes the two
boot movies (`twi5.smk`, `twg.smk`) with an in-repo, clean-room SMK2 decoder and
plays them on the boot path before the title screen. Both movies' presented
frames are proven **pixel-exact** against frames captured from the original in
DOSBox-X (RGB24, palette included): TWI5 120/120, TWG 41/41; the container
layout reconciles byte-exactly on both files. The decoder is allocation-free and
fixed-profile (rejects everything else by name); the player owns the
presentation rule, which is content-based and carries a `TODO(verify)` on the
original's `0x1C740` loop. See
`docs/superpowers/plans/2026-09-17-smacker-video-report.md`.
**Sprite compositor — sub-project 4a-i, ported.** The engine composites a
display list into the back buffer the way `0x14328` does: the display node and
builder (`0x14268`), the span blitter (`0x51E5C`), all six reachable renderers
(RLE, clipped, mirrored, raw, mode-1 shear), the 580-node list with sorted
insert and sort, and the projection/clip driver. The RLE renderer is proven
byte-identical to sub-project 1's already-verified decoder on 32 real sprites;
the clipped/mirrored/shear renderers are proven by hand-computed tests, not by
an emulator (the DOSBox title oracle is 4a-ii's). `game_loop` calls the
compositor in the original's order — a proven no-op until 4a-ii populates the
list. See `docs/superpowers/plans/2026-09-17-sprite-compositor-report.md`.

**Actor system and title — sub-project 4a-ii, ported.** The engine runs its own
actor pool and real title state: the `0x68`-byte records and their lists, spawn
`0x2AE14`, the pset sync, the motion step, the animation-stream interpreter
(`0x2A408`/`0x29F34`/`0x29DB8` and the 47-opcode dispatcher `0x2B2A0`), the text
grid and glyph renderer, the `0x5D7DC` LCG, and `0x121A0` with its `ENGLISH.TXT`
caption. The title composite is proven **pixel-exact** against two independent
captures of the pinned original over the 96-frame window (`make title-oracle`):
the capture is modelled as a byte-offset splice of two adjacent port frames
because the original updates the aperture at `0x255CC` with no retrace wait
while DX-CAPTURE samples at 70.09 Hz, and every captured frame is explained with
zero pixel tolerance and zero unexplained frames. The oracle found and forced
the fix of three defects (the `0x33754` palette-table entry, the 6-bit VGA DAC,
the 16.16 actor velocity). One confirmed finding contradicts the earlier RE: the
in-window opcode-8 count is 0, so pin site 4 is inert and the `task-2-anchor-re.md`
account of the title's nondeterminism is wrong. See
`docs/superpowers/plans/2026-09-17-actor-system-report.md`.

Streamed Smacker audio (2b-ii), menus/EEPROM (4), the fight engine (5) and the
deferred attract/effect subsystem (`0x11000`, `0x13C70`, `0x38A38`, `0x2BF08`)
remain (`/* PORT: */` markers).

### Build and run

```bash
cmake -S port -B build && cmake --build build     # or: make build
./build/prageport --game-dir data/game/C          # windowed; ESC to quit
./build/prageport --game-dir data/game/C --check 60
```

The windowed run opens the audio device with the window, at the FM core's
native 49716 Hz stereo rate; if no device can be started it prints SDL's reason
and continues silently (audio is otherwise unconditional — there is no sound
flag). A headless machine can run with `SDL_AUDIODRIVER=dummy` to exercise the
open path without a device.

`--check N` is a headless mode: it runs exactly N master-loop iterations with no
window and no audio device, and writes `frame_NNNN.ppm` (RGB), `frame_NNNN.pal`
(the DAC) and `frame_NNNN.idx` (raw indices) per frame, exiting non-zero on an
internal assertion failure. It also asserts the announcer became a live voice
rendering non-silence and that the sequencer keyed notes.

### Verify

```bash
PR_ORACLE_REQUIRED=1 ./build/run_tests            # or: make verify
```

`PR_ORACLE_REQUIRED=1` is required for a real verification run: the byte-exact
Ghidra/Smacker/title oracles are git-ignored (they are copies of the game's own
bytes), so without it the suite skips those comparisons. `make verify` runs the
full ladder in order — a `--check 60` headless smoke run, then the
oracle-required test suite, then `make smk-oracle`, `make title-oracle`,
the GRA-extract oracle tests, and finally `symbols.h` idempotence.

The title oracle needs the pinned capture and the port dump:

```bash
make title-pin                                   # patch /tmp/pr_title_pin/PRAGE.EXE (writes /tmp only)
make title-oracle                                # align the port dump into data/title-captures/* and compare
```

With two independent captures under `data/title-captures/`, `make title-oracle`
proves both the pixel match (every captured frame is a byte-offset splice of two
adjacent port frames, zero tolerance) and determinism (the captures agree on the
clean samples). With `PR_ORACLE_REQUIRED=1` and fewer than two captures it fails
rather than reporting the proof incomplete.

### Third-party

The FM synthesiser is vendored: **opal 2.0.3**, MIT
(`https://github.com/RealBitdancer/opal`, commit `7e829f33`), whose synthesis
core is by Shayde/Reality (Reality Adlib Tracker 2), public domain. It lives at
`port/src/platform/audio/opl/` with its licence at
`opl/LICENSE.opal.txt`; the files are byte-identical to upstream apart from a
provenance banner. See `THIRD_PARTY_LICENSES.md`.
