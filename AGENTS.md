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
make demo-fight-oracle     # ratchet on the demo fight's first unexplained frame (N pinned in the Makefile); in make verify
make attract2-oracle       # ratchet on the attract's second cycle after the demo (N pinned in the Makefile); in make verify
make k11-oracle            # K11 service-menu oracles (the walk and menuesc); in make verify; each skips without its data/k11-captures/<scenario>
make k11-capture scenario=walk   # DOSBox-X capture of the service menu (writes data/k11-captures/<scenario>)
make gp-capture scenario=gp-pads  # DOSBox-X gameplay capture (frame-keyed injection, per-frame snapshot log); writes data/k11-captures/<scenario> (scenario names are gp-…)
make gp-replay scenario=gp-pads  # port replay of a gameplay capture (PR_GP_DUMP driver, .ipx frames + trace); in make verify on gp-pads, which skips without data/k11-captures/gp-pads
make gp-oracle             # gameplay oracle: frame + trace ratchets on data/k11-captures/gp-idle-loss (N values in the Makefile); in make verify; skips without the capture
make gp-charsel-oracle     # the same ratchets on data/k11-captures/gp-u5-charsel (the character-select walk; N 516 = how far the port's replay got, F 1513, the poll.log hash pinned); in make verify; skips without the capture
make gp-moves-oracle       # gameplay oracle: gp-u6-moves-b frame, trace and moves ratchets (N values in the Makefile); in make verify; skips without the capture
make gp-report scenario=gp-pads  # report-only comparison of a gp capture against its port replay (no ratchet, exit 0)
make audio-render          # FM music to a WAV (the windowed run is silent here)
make diff-verify           # differential verification: the original's bytes vs the port's C functions (skips without unicorn or capstone; in make verify)
make entry-triage          # E2: triage of the non-Ghidra entry candidates; the committed table must equal a fresh run (in make verify; skips without capstone or PRAGE.EXE; fails under PR_ORACLE_REQUIRED=1)
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
  `PR_ATTRACT_DUMP`, `PR_FRONTEND_DUMP`, `PR_FRONTEND_DET`, `PR_K11_DUMP`, `PR_GP_DUMP`,
  `PR_RESTART`).
- If a build invoked through `make` looks stale, `cmake --build build` is the
  reliable fallback.
- macOS host: no `timeout`; SDL audio cannot open here (`-66681`), so the
  windowed run is silent — use `make audio-render` to hear the FM path.
- `make gp-capture` fails (`check=FAIL unscripted input`) on any key word or pad bit the harness did
  not inject, so nobody may type into the DOSBox-X window while it runs; audit an existing capture
  with `python3 tools/gp_capture.py check-input data/k11-captures/<scenario>`. The check detects
  stray typing but does not prove its absence: it cannot see a stray press of a pad key inside that
  key's scripted hold window, input before the first or after the last S record, a stray key-up of a
  non-pad key, or a pad tap between two S records.

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
- The differential harness (`make diff-verify`, `tools/diff_verify.py`) needs `unicorn` and `capstone` (`pip install -r tools/requirements-diff.txt`; pins `unicorn` 2.1.4 and `capstone` 5.0.7). Without either the step prints its skip line and `make verify` stays green.
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

- **One area file per subsystem**, each holding several `int test_X(void)`
  functions: `test_platform.c`, `test_game.c`, `test_fight.c`, `test_audio.c`,
  `test_video.c`, plus the shared `test_fixtures.{h,c}`. **Register a test once**
  — add `X(test_foo)` to `TEST_CASES` in `port/tests/test.h`; the declarations,
  the run order and the build all follow from that one line (CMake globs
  `tests/test_*.c` with `CONFIGURE_DEPENDS` — the one deliberate exception to
  the repo's no-globbing rule, and the registry is what makes it safe). Env-gated
  drivers go in `TEST_DRIVERS` with their env var; each runs alone before the
  unit cases.
- **Consolidate, don't proliferate.** Add a `test_X` function to the area file
  that owns the code rather than creating a new file, and prefer fewer, larger
  area files over many small ones. **Consolidating must not change an
  assertion.** The `CHECK`/`CHECK_EQ_INT` suite total is the gate: it is
  identical before and after, and `make verify` stays green.
- Shared test fixtures live in `port/tests/test_fixtures.{h,c}`. A fixture moved
  there keeps its body byte-for-byte; only its home and its callers change.
- Only `CHECK(cond,msg)` and `CHECK_EQ_INT(a,b)`.
- Every env-gated driver and `--check` run arms `fn_resolve`'s miss log and
  must record exactly the pinned known-set (`k_miss_known` in
  `test_platform.c`, record gameplay-u0 §U0.2; `PR_FRONTEND_DUMP` also records
  its own unit probe `0x41578`). A port that makes a driver reach a new
  unregistered code pointer fails it: register the target or pin the miss
  with its evidence. The `PR_FRONTEND_DET` parent is not armed (its two
  `PR_FRONTEND_DUMP` children are), and the windowed `PR_FN_MISSLOG` report
  prints only when `game_main` returns, not on an `exit()` path.
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
  `PR_ORACLE_REQUIRED`); the title/attract/smk ones skip without captures. The
  demo-fight ratchet (`make demo-fight-oracle`, in `make verify`) is enforced the
  same way and skips without the capture; it fails if the first unexplained
  fight-window frame moves earlier than its pinned N (raise N when it improves).
  When the fight window is empty (the front-end window reaches the capture's
  first all-black frame), N = that frame + 1 is the exact pin, and a
  front-end window shrink that leaves a fight frame unexplained still fails
  the ratchet. The current N and its provenance are in the Makefile.
  The attract cycle-2 ratchet (`make attract2-oracle`; in `make verify` as
  `attract2-compare` on the demo-fight run's dump) is enforced the same way.
  It classifies the capture after the demo (from its first all-black frame) against
  the driver's separate `cycle2/` dump only. That dump holds the frames presented
  after the exit frame, the logo player's screens included. Keep them out of
  the top-level dump, or the front-end window matches them against the
  capture's first attract.
- The front-end oracle's claim is narrow: it proves only that no content-bearing
  capture frame inside the window the port exhibits is unexplained. It **cannot**
  detect a port that under-renders, and its window is derived from the port's own
  dump. Do not read a green oracle as "the frame is correct".
  The K11 oracle (`make k11-oracle`) is narrow the same way: its window START
  comes from the port's dump and its END is the capture's last non-black frame;
  it proves that no content-bearing capture frame in that window is
  unexplained (two mid-draw frames allowed by name) and that every settled port
  screen appears in the capture, from injected keys, not the keyboard
  controller (record 2026-09-30-named-gaps-a §A.10). It also compares the
  `menuesc` soft-restart capture (same narrow claim, record named-gaps-b §B.12);
  that comparison ratchets its window END at 388 (`K11_OPEN_END` in
  `tools/k11_compare.py`, a measured value like the demo-fight N: raise it when
  the window grows), because an END taken from the port's own dump passes a
  port that never restarts.
  The gameplay oracle (`make gp-oracle`, in `make verify`) ratchets the first
  unexplained capture frame and the first differing trace frame of
  `data/k11-captures/gp-idle-loss`; its N values and provenance are in the
  Makefile (with the capture's `poll.log` sha256: another capture fails); it skips without the capture,
  even under `PR_ORACLE_REQUIRED`. The `PR_GP_DUMP` driver pins its own `fn_resolve` miss set per
  scenario (`test_platform.c`, record gameplay-ground-truth §G.24).
  `make gp-charsel-oracle` (in `make verify`) does the same for `data/k11-captures/gp-u5-charsel`
  (record gameplay-u5 §C5.18): N = 516 is the script's end (how far the port's replay got, not a
  divergence), F = 1513, the `poll.log` sha256 and frame count are pinned; it skips without the capture. Its claim is narrow the same way (record
  `…-gameplay-ground-truth-derivations.md` §G.16): no content-bearing capture
  frame from the window start up to N is unexplained and the traced fields
  agree below F; the window start is pinned too (`GP_IDLE_LOSS_MAX_START`), so a
  regressed port cannot slide it forward. It cannot detect a port that
  under-renders, and **the order of the port's frames and that every port frame
  appears are not claimed** (a named gap; a coverage count is only reported). Where
  the port's script ends before the capture the first unexplained frame is how far
  the port got, not a defect.
  `make gp-moves-oracle` does the same for `data/k11-captures/gp-u6-moves-b` and adds a
  third ratchet, `moves`, over the snapshot's move bytes (`c0 c1 r0 r1 s0_43`, record
  gameplay-u6 §U6.11); its claim is as narrow.
- Captures are git-ignored. `data/` is git-ignored and read-only — never write to it.
- DOSBox captures at 70.09 Hz while the game ticks at 60.05 Hz, which is why the
  title/front-end oracles model a capture frame as a byte-offset splice of two
  adjacent port frames rather than a 1:1 match.

## Workflow and docs

- Sub-project specs, plans, derivation records and reports live in
  `docs/superpowers/`. A derivation record (`*-derivations.md`) is the
  authoritative raw-byte source for the tasks that implement from it.
- `docs/PROGRESS.md` is the running, per-task status narrative (what's ported,
  verified, and every named gap). Append a paragraph there, not to `README.md`,
  which stays a short pointer.
- **Keep `README.md`'s title percentage current.** It reads
  `— Reverse Engineering NN%`. Run `python3 tools/port_progress.py`: it prints
  `ported total percent`, counting only real functions (`symbols.h` `FN_`
  addresses) that have a `/* 0xADDR` header or an `fn_register` in `port/src`.
  Update the README title and its "N% of the original's D real functions"
  line after any merge that adds or removes a ported function.
  It also prints the adjusted (portable) figure; the README title keeps the raw
  percentage. As of 2026-09-30 the two lines read `771 1203 64` and
  `731 731 100`:
  - **Raw:** 771 ported of 1203 real functions (64%). The 432 unported functions
    are not porting targets: 81 are host-owned or deferred (the rows of
    `tools/port_classification.txt` that are not ported, one evidence line each)
    and 351 are runtime code (>= 0x5D000: WATCOM libc, DOS/4GW glue) that the
    port serves from the host libc.
  - **Portable:** 731 of 731 (100%). The denominator leaves out the 81
    host-owned/deferred functions and everything at or above 0x5D000; the
    other 40 ported functions (771 - 731) sit in that runtime region, so they
    count in the raw figure only.
  `--unported` lists what is left (addr, size, callers, callees), largest
  first; the `runtime` rows are not targets.
- Multi-task work runs under subagent-driven development with a git-ignored
  ledger at `.superpowers/sdd/<plan-basename>/progress.md`. `make clean` keeps
  `.superpowers/` deliberately — it is the recovery map, not build output.
- Commit style: `<area>: <what changed>`. **Never `git add -A`** — stage named files.
- Only commit when asked.
- **The E2 target list is a gate** (`docs/superpowers/plans/2026-10-01-reverse-e2-triage.md`; `make entry-triage`, in
  `make verify`, fails when it differs from a fresh run). A commit that ports a target (a new `/* 0xADDR` header or
  `fn_register`) regenerates the table in the same commit (decision D3):
  `./build/diffrun --exe data/game/C/PRAGE.EXE --image-out /tmp/e2img && python3 tools/entry_triage.py --image /tmp/e2img --live docs/superpowers/plans/2026-10-01-reverse-e2-live-functions.txt --out docs/superpowers/plans/2026-10-01-reverse-e2-triage.md`.
  A conflict in that file is resolved by regenerating it after the rebase, never by merging lines by hand.

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
