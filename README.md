# Primal Rage (DOS, 1995) — Reverse Engineering 66%

![Primal Rage](docs/intro.jpg)

Reverse engineering of **Primal Rage**'s PC/DOS release (Time Warner
Interactive / Probe Software / Teeny Weeny Games, 1995), targeting `PRAGE.EXE`
and the `S16*.GRA` graphics set, plus an SDL3 port that reimplements the engine
in C over a flat `mem[]` at the original addresses.

The original is a **32-bit protected-mode WATCOM C/C++32 program** shipped as a
Microsoft DOS/4GW *bound* Linear Executable (LE): two LE objects (code + data),
no 16-bit real-mode game code.

## Quick start

```bash
make deps                 # check the toolchain and game assets
make build                # cmake -S port -B build && cmake --build build
make run                  # windowed port
make check frames=60      # headless frames -> frames/frame_*.ppm/.pal/.idx
make verify               # the full gate
```

The original game files are **not included**. Download them (e.g. from
[myabandonware](https://www.myabandonware.com/game/primal-rage-2vr)) and place
them under `data/game/C/` (`PRAGE.EXE`, `INDEX`, `S16*.GRA`, sound drivers).
`data/` is git-ignored and `data/game/` is read-only; the capture harness writes
only its own directories under `data/`.

## Status

The port covers the whole game: boot, title, attract cycles, front end, service
menu, character select, one- and two-player matches, the win path, all seven
endings, and the game-over/high-score flows. The measured claims:

- **Port:** 798 of 1,203 indexed functions (**66%** raw); **759/759 portable
  (100%)**. The 405 not ported are 53 classified host-owned/deferred (41
  host-owned + 12 deferred; `tools/port_classification.txt`), 37 replaced
  clean-room runtime and 315 WATCOM/DOS4GW/AIL runtime functions served by the
  host (`tools/port_progress.py`).
- **Differential:** `make diff-verify` runs the original's bytes against the
  port's C functions: **1033/1033 functions**, **14993/14993 mutants**, **0 named
  gaps**; **825/845** callee rows closed, **810/845** full; **20 terminal rows**
  (19 addresses), **12 `WEAKER`**, **8 `UNROWED`**.
- **Oracles:** **44 explicit capture paths**, **43/44 present** (only the
  declared U14 gap); the Smacker logo, title/attract, front end, demo, K11
  service menu and the gameplay replays are compared against the original's
  frames. These are bounded-missing claims, not complete coverage.
- **Span census:** the independent census has 417 slots / 386 writer targets /
  26 code-shape classes proved (385/386 writer targets); E2 reads 736 candidates
  (732 by U0's rule), 388 span-writer.
- **Audio:** the register-stream oracle compares the port's OPL writes
  tolerance-free against 29 `.dro` traces across 28 capture directories (the
  29th, `gp-u17-device2`, skips by name); the 26
  code-reachable banks split **12 claimed / 10 residue / 2 unattributable / 2
  never-requested**, with **74 diff-allowed prefixes / 29,656 ticks** matched.
  Rendered audibility is terminal (the OPL core/mixer is an invented model); the
  game-owned claim is the register stream.
- **Endgame:** all seven endings and the U16 finisher reaction are pinned under
  poked win paths. The claim is the ending content and finisher reaction, not
  the KO by hits; the U23 moves ratchet records the port's `r0=0x27` vs the
  capture's `0x33`.

The single source of truth for every remaining gap is the **terminal claim
register**:
[`docs/superpowers/specs/2026-10-30-final-claims.md`](docs/superpowers/specs/2026-10-30-final-claims.md).
It has 34 rows, each closed, gated, terminal or declared, with its address,
file:line or capture. Named there: U14's fought arcade win (R1) is **terminal**
(the two-human 2-0 win is game evidence only in
`gp-u14-twop-win.failed`); `0x21` volleyball is **declared** (the count is not
limited to 1, but no frame-exact route is derivable); the 5120 band is
**PASS-A terminal** (the content is the port's; the old blue is the
capture/present-side class); the 20 terminal differential rows, the 12 `WEAKER`
and 8 `UNROWED`, the 4 `UNINSTRUMENTED` census addresses, the storage items,
the zero-payload movie audio (R33), the 53/37/315 classification, and the
oracle window/model limits. `0x25` bowling and the taken continue are **closed**
(gated captures), and `0x2CADC`'s boot-defaults omission is **fixed**.

The full gate is two consecutive
`PR_DIFF_FULLSCAN=1 GATE_STRICT=1 PR_PINS_EXACT=1 make verify` runs plus
`make audio-render`; the two runs and both audio renders passed on the gated
tree (`b3a710cc`, merged as `43b79b30`; recorded in `docs/PROGRESS.md`).

## Layout

| Path | What |
|---|---|
| `docs/PROGRESS.md` | Running status and per-task history |
| `docs/superpowers/specs/2026-10-30-final-claims.md` | Terminal claim register: every gap's measured status and evidence |
| `port/` | SDL3 port: `RE_GUIDE.md` (address model), `PORTING.md` (rules), `spec/game_flow.md` |
| `port/src/` | The port: `game/`, `platform/` (gfx, sprite, render, res, input, `audio/`), `mem.c`, `host.c` |
| `port/decomp/` | Ghidra decompilation + function/string/symbol indexes |
| `docs/FORMATS.md` | Decoded on-disk formats (LE layout, `INDEX`, `GRA`) |
| `docs/THIRD_PARTY_LICENSES.md` | Vendored third-party code and licences |
| `docs/superpowers/` | Sub-project specs, plans, derivation records and reports |
| `tools/` | RE/oracle toolbox: `le_info.py`, `gra_render.py`, `gra_extract.py`, `gen_symbols.py`, the comparison tools |
| `_tools/ghidra_scripts/` | Ghidra headless scripts used to produce the artefacts |
| `data/game/C/` | Installed game (git-ignored); `data/game/CD/RAGECD.ISO` is the original CD |
| `Makefile` | `make help` (quick) / `make help-all` (every target) |

## Build and run

```bash
cmake -S port -B build && cmake --build build     # or: make build
./build/prageport --game-dir data/game/C          # windowed; ESC to quit
./build/prageport --game-dir data/game/C --check 60
```

The window opens at 1024x768 (4:3, the original's CRT aspect) and is
user-resizable; the 320x200 frame is scaled to fill it with nearest-neighbour
pixels (host policy, not the original — see the `PORT:` note in `host.c`). The
windowed run opens the FM core's native 49716 Hz stereo; if no device opens it
prints SDL's reason and continues silently. `SDL_AUDIODRIVER=dummy` exercises
the open path headlessly.

`--check N` runs exactly N master-loop iterations with no window or audio,
writing `frame_NNNN.ppm` (RGB), `frame_NNNN.pal` (DAC) and `frame_NNNN.idx`
(raw indices) per frame, and exits non-zero on an internal assertion failure.

## Verify

```bash
make verify            # the gate (sets PR_ORACLE_REQUIRED=1)
make gate-check        # capture inventory, manifest, pin baseline, preflight
```

`make verify` runs the headless `--check` smoke, the oracle-required tests, the
pixel/ratchet oracles, the register-stream audio oracle and `audio-render`, the
differential harness, the E2 triage, the tool unit tests (including the
final-claims register guard) and `symbols.h` idempotence. `make gate-check`
inventories the 44 explicit capture paths first; the full checkout is 43/44
with only the declared U14 gap. `GATE_STRICT=1` (the default) fails an
undeclared absence; `PR_PINS_EXACT` is strict by default, so an improvement
requires a re-pin.

The full gate needs `port/tests/ghidra_data.bin` (git-ignored; `make re-oracle`
regenerates it), `unicorn` and `capstone`
(`pip install -r tools/requirements-diff.txt`), and `data/game/C/CMOS` absent
or 2040 zero bytes; `make deps` checks them. The capture model (70.09 Hz
captures vs the 60.05 Hz game) and the ratchets are described in `AGENTS.md`;
each pin's provenance is in the `Makefile`.

To reproduce the title/attract captures, see `make title-pin`,
`make title-capture`, `make frontend-capture`, then the oracle targets.

## Toolchain

* **Ghidra 12.2** with the
  [`ghidra-lx-loader`](https://github.com/yetmorecode/ghidra-lx-loader)
  extension (LE/LX loader, DOS/4GW aware). It maps object 0 to `0x10000` (code)
  and object 1 to `0x80000` (data) and applies the LE fixups. Import and export
  commands are in `port/RE_GUIDE.md`.
* **JDK 25** (`temurin-25`) for Ghidra 12.2.
* **dosbox-x** for ground truth (memory layout, runtime behaviour, captures).
* Python 3 with `capstone` for ad-hoc disassembly and `Pillow` for
  `tools/gra_extract.py`; `unicorn` + `capstone` for the differential harness.

## Third-party

The FM synthesiser is vendored: **opal 2.0.3**, MIT
(`https://github.com/RealBitdancer/opal`, commit `7e829f33`); its synthesis core
is by Shayde/Reality (Reality Adlib Tracker 2), public domain. It lives at
`port/src/platform/audio/opl/` with its licence at
`opl/LICENSE.opal.txt`; the files are byte-identical to upstream apart from a
provenance banner. See `docs/THIRD_PARTY_LICENSES.md`.
