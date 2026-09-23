# AGENTS.md — Primal Rage (DOS 1995) reverse engineering + SDL3 port

Faithful reimplementation of `PRAGE.EXE` in C over a flat `mem[]`, plus the RE
artefacts. Read `port/RE_GUIDE.md` (address conventions), `port/PORTING.md`
(the porting rules this file summarises), and `port/spec/game_flow.md` (the
frame loop and pixel path) before changing engine code.

## Commands

```bash
make help                  # the authoritative target list
make build                 # cmake -S port -B build && cmake --build build
make verify                # THE gate: frames + oracle-required tests + all oracles + symbols.h idempotence
PR_ORACLE_REQUIRED=1 ./build/run_tests    # tests alone, with the byte-exact oracles enabled
make check frames=60       # headless run → frames/frame_*.ppm/.pal/.idx
make run                   # windowed
make title-oracle          # pixel-exact oracles; each skips without its capture
make attract-oracle smk-oracle frontend-oracle demo-oracle
make audio-render          # FM music to a WAV (the windowed run is silent here)
```

- **Run the binaries from the repo root.** Several tests default to the relative
  `data/game/C` (`PR_GAME_DIR` overrides it), and CMake's `add_test` sets the
  working directory to the repo root for the same reason.

- **The byte-exact oracles SKIP silently unless `PR_ORACLE_REQUIRED=1`** — the
  oracle fixtures are copies of the game's own bytes and are git-ignored. A run
  without it proves much less than it appears to. `make verify` sets it.
- `make verify`'s `--check` step **must precede** the tests that read
  `frames/frame_*.idx`.
- There is no per-test filter: `run_tests` is one binary. The drivers that call
  `game_init()` are selected by env var and run alone (`PR_TITLE_DUMP`,
  `PR_ATTRACT_DUMP`, `PR_FRONTEND_DUMP`, `PR_FRONTEND_DET`).
- If a build invoked through `make` looks stale, `cmake --build build` is the
  reliable fallback.
- macOS host: no `timeout`; SDL audio cannot open here (`-66681`), so the
  windowed run is silent — use `make audio-render` to hear the FM path.

## Address model (get this wrong and everything downstream is wrong)

- **Ghidra address == linear address == object base + offset.** No segment:offset.
  Code object `0x10000`–`0x73B14`; data object `0x80000`–`0x10B0CF`.
- A global Ghidra calls `DAT_0008xxxx` is at **DS offset `0xxxx`** (subtract
  `0x80000`). `mem.h` defines `DATA_BASE 0x80000`, `CODE_BASE 0x10000`,
  `MEM_SIZE 0x4000000`.
- All original state lives in `mem[]` at its original linear address, accessed as
  `DSB/DSW/DSD(addr)`. **Never shadow original state in a long-lived C global.**
- Raw-file disassembly (no Ghidra): obj-0 code file offset = `VA + 0x52E54`;
  obj-1 data file offset = `VA + 0x46E54`. **But those bytes are pre-fixup**: the
  LE fixup records rewrite the referenced data addresses (each record writes
  `base(target_object) + target_offset` — see `mem_load_le_fixups` in `mem.c`).
  So a raw-file disassembly shows displacements that are *not* the runtime
  addresses — **use Ghidra (fixups applied) for any data address**, or replicate
  `mem_load_le` + `mem_load_le_fixups`, or you will read the wrong global and
  conclude the wrong thing.
- The `.image` overlay block is analysis noise: 1207 of 1352 functions are real
  code; ignore `FUN_.image__*`.
- **Aperture rule:** never write `mem[0xA0000]`. Composite into
  `mem + DSD(DS_000E87A4)` and present through `gfx_present()`.

## Tooling for RE

- **A live Ghidra MCP instance is usually available** (project `rage`,
  `/PRAGE.EXE`); its addresses match the scheme above. Prefer it for data
  addresses, jump tables and cross-references.
- Ghidra headless + the exact import/re-export commands: `port/RE_GUIDE.md`.
  Needs `JAVA_HOME` (temurin-25) and the `ghidra-lx-loader` extension.
- `dosbox-x` for runtime ground truth (memory-file dumps, debugger breakpoints).
- Python 3 with `capstone` for ad-hoc disassembly, `Pillow` for `tools/gra_extract.py`.
- `tools/` is the RE/oracle toolbox: `le_info.py`, `gra_render.py`,
  `gra_extract.py`, `gen_symbols.py`, `title_pin.py`, `title_compare.py`.
  Check the active plan before editing anything here.

## Porting rules (the ones that get missed)

- One C function per original function, header comment `/* 0xADDR — spec section */`.
- Mark deliberate deviations `/* PORT: ... */`; doubts `/* TODO(verify): ... */`.
  No other comment styles in `port/src`.
- Code addresses stored in data go through `fn_origin()` / `fn_resolve()`.
- SDL and file/asset I/O live **only** in `port/src/host.c` and `main.c`. New
  subsystems are data-in: the caller resolves assets and passes bytes/handles.
- Do not reformat files owned by another module.
- **`port/src/symbols.h` is generated** by `tools/gen_symbols.py` and `make verify`
  fails if it does not regenerate byte-identically. Never hand-edit it; where the
  generator emits no name, use a local `#define` with the raw address.

## Tests

- One `int test_X(void)` per file. **Register it once** — add `X(test_foo)` to
  `TEST_CASES` in `port/tests/test.h`; the declarations, the run order and the
  build all follow from that one line (CMake globs `tests/test_*.c` with
  `CONFIGURE_DEPENDS` — the one deliberate exception to the repo's no-globbing
  rule, and the registry is what makes it safe). Env-gated drivers go in
  `TEST_DRIVERS` with their env var; each runs alone before the unit cases.
- Shared test fixtures live in `port/tests/test_fixtures.{h,c}`. A fixture moved
  there keeps its body byte-for-byte; only its home and its callers change.
- **Consolidating tests must not change an assertion.** The
  `CHECK`/`CHECK_EQ_INT` count per file is the gate: it is identical before and
  after, and `make verify` stays green.
- Only `CHECK(cond,msg)` and `CHECK_EQ_INT(a,b)`.
- **`game_init()` may run only once per process** (a second resource load
  exhausts the bump allocator). Any test calling it must be env-gated, and
  `run_tests.c` must run that driver alone.
- **Assertions must be able to fail.** Seed sentinels that differ from the
  post-conditions, prove a new assertion fails under a mutation of the code it
  tests, and never assert an unseeded BSS-zero (it cannot distinguish "wrote
  zero" from "never touched it"). This repo has shipped several such defects.

## Evidence discipline (non-negotiable here)

- **Never ship a fitted constant.** Every value is derived from the raw bytes or
  a capture, with the address that proves it. A value that cannot be pinned is a
  **named gap with its evidence** — never a plausible-looking number.
- **On any plan-vs-raw conflict the raw wins.** Record the correction and the
  address, in the derivation record and the report.
- The byte-exact oracle lines are the regression gate and must not move; run
  `make verify` after any change that can affect rendering, timing or RNG. The
  **front-end oracle is the enforced one** (it does not need
  `PR_ORACLE_REQUIRED`); the title/attract/smk ones skip without captures.
- The front-end oracle's claim is narrow: it proves only that no content-bearing
  capture frame inside the window the port exhibits is unexplained. It **cannot**
  detect a port that under-renders, and its window is derived from the port's own
  dump. Do not read a green oracle as "the frame is correct".
- Captures are git-ignored. `data/` is git-ignored and read-only — never write to it.
- DOSBox captures at 70.09 Hz while the game ticks at 60.05 Hz, which is why the
  title/front-end oracles model a capture frame as a byte-offset splice of two
  adjacent port frames rather than a 1:1 match.

## Workflow and docs

- Sub-project specs, plans, derivation records and reports live in
  `docs/superpowers/`. A derivation record (`*-derivations.md`) is the
  authoritative raw-byte source for the tasks that implement from it.
- Multi-task work runs under subagent-driven development with a git-ignored
  ledger at `.superpowers/sdd/<plan-basename>/progress.md`. `make clean` keeps
  `.superpowers/` deliberately — it is the recovery map, not build output.
- Commit style: `<area>: <what changed>`. **Never `git add -A`** — stage named files.
- Only commit when asked.

## Layout

| Path | What |
|---|---|
| `port/src/` | the port: `game/` (flow, actors, camera, fighter, fight, attract, effects, rng), `platform/` (gfx, sprite, render, res, input, `audio/`), `mem.c`, `host.c`, `main.c` |
| `port/tests/` | the assertion suite (one file per area) |
| `port/decomp/` | Ghidra decompilation + function/call/string indexes (read-only source of truth) |
| `port/spec/` | `game_flow.md` — the frame loop, state machine, pixel path |
| `tools/`, `_tools/` | RE/oracle tooling, Ghidra headless scripts |
| `data/` | the installed game (git-ignored, read-only) |
| `docs/superpowers/` | specs, plans, derivation records, reports |
