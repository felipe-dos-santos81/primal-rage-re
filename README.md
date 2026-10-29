# Primal Rage (DOS, 1995) — Reverse Engineering 66% of Ghidra's 1203 indexed functions

![Primal Rage](docs/intro.jpg)

Reverse engineering of **Primal Rage**'s PC/DOS release (Time Warner
Interactive / Probe Software / Teeny Weeny Games, 1995), targeting `PRAGE.EXE`
and the `S16*.GRA` graphics set.

It is a **32-bit protected-mode WATCOM C/C++32 program** shipped as a
**Microsoft DOS/4GW *bound* Linear Executable (LE)**. There is no 16-bit real
mode game code: everything interesting lives in two LE objects (code + data).

## Game assets

The original game files are **not included** in this repository and are
required to build, run, or reverse-engineer anything here. Download them (e.g.
from [myabandonware](https://www.myabandonware.com/game/primal-rage-2vr)) and
place them under `data/game/C/` (`PRAGE.EXE`, `INDEX`, `S16*.GRA`, sound
drivers). `data/` is git-ignored; `data/game/` is read-only input. The capture
harness writes only its own capture directories under `data/` and must leave
`data/game/` untouched.

## Status

`PRAGE.EXE` and its `S16*.GRA` graphics set are fully decompiled
(`docs/FORMATS.md`, `port/decomp/`). The SDL3 port reimplements the engine in C
over a flat `mem[]` at the original addresses (`port/PORTING.md`) and covers the
whole game: boot, title, attract cycles, front end, service menu, character
select, one- and two-player matches, the win path, all seven endings, and the
game-over/high-score flows. The measured claims:

- **Porting — 66% raw, 100% portable.** 798 of Ghidra's 1203 indexed functions
  have a ported, header-commented counterpart (798 = 759 portable + 39
  runtime-region). The 405 not ported are 53 classified host-owned/deferred
  (41 + 12; `tools/port_classification.txt`), 37 replaced clean-room runtime
  (the boot/movie/attract spine) and 315 WATCOM libc / DOS/4GW / AIL runtime
  functions served by the host. Excluding the non-targets, **759/759 portable**
  functions are ported (`tools/port_progress.py`).
- **Differential.** `make diff-verify` runs the original's bytes in unicorn
  against the port's C functions from the same image, comparing every changed
  byte, the return register, the ordered calls and the block coverage:
  **1033/1033 functions VERIFIED**, **14993/14993 mutants detected**, **0 named
  gaps**. Of the 845 rows with callees, **825 are closed** and **809 are closed
  on full rows** (188 callee-less rows counted apart); **20 declared terminal
  rows** (19 addresses), **13 `WEAKER`**, **8 `UNROWED`** (45 `FN_`
  ported-unrowed = 8 portable + 37 runtime).
- **Span writers.** The independent census has **417 slots and 392 distinct
  targets** (386 writer targets, 385 reachable); **385/386 targets** and
  **26/26 code-shape classes** are proved. E2 reads **736 candidates** (732 by
  U0's rule), 388 span-writer.
- **Pixel and timing oracles.** The Smacker logo, title, attract (cycle 2),
  front end, demo fight, K11 service menu and 24 gameplay replays are compared
  against the original's captured frames. The gate inventory is **35 explicit
  capture paths**, **34/35 present** with only the declared U14 gap. These are
  **bounded-missing claims, not complete coverage**: an unsampled state or a
  same-count class substitution remains outside them.
- **Audio.** The register-stream oracle compares the port's OPL register writes
  tolerance-free against **29 `.dro` traces across 28 capture directories**
  (gp-pads holds two; the wall-paced device-2 walk skips by name), and
  `audio-render` pins the rendered WAV. **Rendered audibility remains a named
  gap** (no device-recording oracle).
- **Endgame.** All seven endings are pinned — SAURON and BLIZZARD by the earlier
  U10/U13 captures, TALON, VERTIGO, CHAOS, ARMADON and DIABLO by `gp-u18`…
  `gp-u22` — and the U16 finisher reaction by `gp-u23`, all under poked win
  paths. The claim is the **ending content and finisher reaction, not the KO by
  hits**: U23's moves ratchet records the port's `r0=0x27` where the capture
  reaches `0x33`.

Named gaps: the U14 fought live win (R1), the U16 live finisher, the 5120
band's unisolated cause, rendered audibility, and the bounded-missing limits.
The fought-match truth phase measured the harness and two game-side results
(U14's one authorized live win lost, the two-human KO as game evidence only,
the U16 case-3 exit) **without adding a claim**. The running detail and every
gap's evidence are in [`docs/PROGRESS.md`](docs/PROGRESS.md); the frame loop and
pixel path are in `port/spec/game_flow.md`.

## Layout

| Path | What |
|---|---|
| `docs/PROGRESS.md` | Running status: what's ported, verified, and every named gap |
| `port/` | SDL3 port: `RE_GUIDE.md` (address model), `PORTING.md` (rules), `spec/game_flow.md` |
| `port/src/` | The port: `game/`, `platform/` (gfx, sprite, render, res, input, `audio/`), `mem.c`, `host.c` |
| `port/decomp/` | Ghidra decompilation + function/string/symbol indexes (`DECOMPILATION.md`) |
| `docs/FORMATS.md` | Decoded on-disk formats (LE layout, `INDEX`, `GRA`) |
| `docs/THIRD_PARTY_LICENSES.md` | Vendored third-party code and licences |
| `docs/superpowers/` | Sub-project specs, plans, derivation records and reports |
| `tools/` | RE/oracle toolbox: `le_info.py`, `gra_render.py`, `gra_extract.py`, `gen_symbols.py`, the comparison tools |
| `_tools/ghidra_scripts/` | Ghidra headless scripts used to produce the artefacts |
| `data/game/C/` | Installed game (git-ignored); `data/game/CD/RAGECD.ISO` is the original CD |
| `Makefile` | `make help` — PORT (`build`/`test`/`check`/`run`/`verify`) and RE targets |

## Toolchain

* **Ghidra 12.2** with the
  [`ghidra-lx-loader`](https://github.com/yetmorecode/ghidra-lx-loader)
  extension (LE/LX loader, DOS/4GW aware). It maps object 0 to `0x10000` (code)
  and object 1 to `0x80000` (data) and applies the LE fixups, so no manual
  image extraction is needed.
* **JDK 25** (`temurin-25`) for Ghidra 12.2.
* **dosbox-x** for ground truth (memory layout, runtime behaviour, captures).
* Python 3 with `capstone` for ad-hoc disassembly and `Pillow` for
  `tools/gra_extract.py`.
* Python 3 with `unicorn` and `capstone` for the differential harness
  (`make diff-verify`; `pip install -r tools/requirements-diff.txt`). Without
  either, that step skips.

### Regenerate the decompilation

```bash
export JAVA_HOME=/path/to/temurin-25        # JDK 25
G="${GHIDRA_HOME:-$HOME/ghidra_12.2_DEV}/support/analyzeHeadless"

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

## Build and run

```bash
cmake -S port -B build && cmake --build build     # or: make build
./build/prageport --game-dir data/game/C          # windowed; ESC to quit
./build/prageport --game-dir data/game/C --check 60
```

The window opens at 1024x768 (4:3, the original's CRT aspect) and is
user-resizable; the 320x200 frame is scaled to fill it (nearest-neighbour, to
keep the pixels hard). This is host policy, not from the original — see the
`PORT:` note in `host.c`. The windowed run opens the audio device at the FM
core's native 49716 Hz stereo rate; if no device can be started it prints
SDL's reason and continues silently (there is no sound flag).
`SDL_AUDIODRIVER=dummy` exercises the open path on a headless machine.

`--check N` runs exactly N master-loop iterations with no window and no audio
device, writing `frame_NNNN.ppm` (RGB), `frame_NNNN.pal` (the DAC) and
`frame_NNNN.idx` (raw indices) per frame, and exits non-zero on an internal
assertion failure.

### Verify

```bash
make verify            # the gate (sets PR_ORACLE_REQUIRED=1)
```

`make verify` runs the headless `--check` smoke, the oracle-required tests, the
restart driver, the pixel/ratchet oracles, the register-stream audio oracle and
`audio-render`, the differential harness, the E2 triage, the tool unit tests
(including the docs-claim guard) and `symbols.h` idempotence. `make gate-check`
inventories the **35 explicit capture paths** first; the full checkout is
**34/35** with only the declared U14 gap. `GATE_STRICT=1` (the default) fails an
undeclared absence; `GATE_STRICT=0` tolerates missing captures so their skips
can run. `PR_PINS_EXACT` is strict by default, so an improvement requires a
re-pin.

The full gate needs `port/tests/ghidra_data.bin` (git-ignored; `make re-oracle`
regenerates it), `unicorn` and `capstone` (`pip install -r
tools/requirements-diff.txt`), and `data/game/C/CMOS` absent or 2040 zero
bytes; `make deps` checks them. The capture model (70.09 Hz captures vs the
60.05 Hz game) and the ratchets are described in `AGENTS.md`; each pin's
provenance is in the `Makefile`.

To produce the title/attract captures yourself:

```bash
make title-pin                                   # pin the four measured behaviour sites into /tmp/pr_title_pin/PRAGE.EXE
make title-capture                               # drive the pinned original under DOSBox-X (data/title-captures/title)
make frontend-capture                            # the front-end region (data/title-captures/frontend)
make title-oracle attract-oracle frontend-oracle # compare
```

## Third-party

The FM synthesiser is vendored: **opal 2.0.3**, MIT
(`https://github.com/RealBitdancer/opal`, commit `7e829f33`); its synthesis core
is by Shayde/Reality (Reality Adlib Tracker 2), public domain. It lives at
`port/src/platform/audio/opl/` with its licence at
`opl/LICENSE.opal.txt`; the files are byte-identical to upstream apart from a
provenance banner. See `docs/THIRD_PARTY_LICENSES.md`.
