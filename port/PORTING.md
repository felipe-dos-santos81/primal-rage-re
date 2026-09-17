# Porting rules — Primal Rage (DOS, 1995) → SDL3

Faithful reimplementation of PRAGE.EXE. Read the spec in
`../docs/superpowers/specs/` and the runtime facts in `../port/spec/game_flow.md`.

## Memory model
* Original data lives in `mem[]` at its original linear address: `DSW(0x101514)`.
  Every global is read and written there; never shadow game state in a C global
  that outlives a function.
* Code is reimplemented in C. Code addresses stored in data go through
  `fn_origin()` / `fn_resolve()`.
* Writes outside `mem[]` are bugs; Debug builds assert.

## Porting a function
1. One C function per original function, header comment `/* 0xADDR — spec section */`.
2. Follow the spec; confirm against the decompilation wherever it says `likely`.
3. Mark deliberate deviations `/* PORT: ... */`; doubts `/* TODO(verify): ... */`.
4. No shadowing of original state: anything the original keeps in its data segment
   stays in `mem[]` at its original offset, never mirrored in a long-lived C global.
   Port-only bookkeeping for port-only behaviour is fine as `static` (e.g. the
   chosen title frame cycle in `game/flow.c`) — say so where it lives. Prefer
   `static`; a link-visible port-only value is acceptable only when the wrapper's
   interface has no handle to carry it (e.g. the single OPL chip, `g_opl` in
   `platform/audio/opl/opl.c`) and its PORT comment says so. It is still never
   original state and is never registered as a `mem[]` offset.
5. Do not reformat files owned by another module.
6. New subsystems stay data-in: no file I/O, no asset resolution and no SDL
   outside `host.c`/`main.c`. The caller resolves assets and passes bytes/handles
   in (the audio modules are the model), so each module is testable without the
   game's assets and headless.

## Build and checks
    cmake -S port -B build && cmake --build build
    PR_ORACLE_REQUIRED=1 ./build/run_tests
    ./build/prageport --game-dir data/game/C --check 60

The byte-exact oracles (`ghidra_data.bin`, `s16title_frame10.idx`) are copies of
the game's own bytes and are git-ignored, so the suite SKIPS them unless
`PR_ORACLE_REQUIRED` is set — a real verification run must set it. `make verify`
runs the whole ladder.
