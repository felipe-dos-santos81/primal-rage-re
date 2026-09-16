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
4. No `static` game state.
5. Do not reformat files owned by another module.

## Build and checks
    cmake -S port -B build && cmake --build build
    ./build/run_tests
