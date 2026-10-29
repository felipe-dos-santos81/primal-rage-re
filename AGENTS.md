# AGENTS.md — Primal Rage (DOS 1995) reverse engineering + SDL3 port

Faithful reimplementation of `PRAGE.EXE` in C over a flat `mem[]`, plus the RE
artefacts. Read `port/RE_GUIDE.md` (address conventions), `port/PORTING.md`
(the porting rules this file summarises), and `port/spec/game_flow.md` (the
frame loop and pixel path) before changing engine code.

## Commands

```bash
make help                  # the authoritative target list
make build                 # cmake -S port -B build && cmake --build build
make gate-check            # inventory the CAPTURE_DIRS paths; strict/partial modes; reports the declared U14 gap
make verify                # THE gate: gate-check + frames + oracle-required tests + all oracles + symbols.h idempotence
PR_ORACLE_REQUIRED=1 ./build/run_tests    # tests alone, with the byte-exact oracles enabled
make check frames=60       # headless run → frames/frame_*.ppm/.pal/.idx
make run                   # windowed
make title-oracle          # pixel-exact oracles; each skips without its capture
make attract-oracle smk-oracle frontend-oracle demo-oracle
make demo-fight-oracle     # ratchet on the demo fight's first unexplained frame (N pinned in the Makefile); in make verify
make attract2-oracle       # ratchet on the attract's second cycle after the demo (N pinned in the Makefile); in make verify
make k11-oracle            # K11 service-menu oracles (the walk and menuesc); in make verify; each skips without its data/k11-captures/<scenario>
make k11-capture scenario=walk   # DOSBox-X capture of the service menu (writes data/k11-captures/<scenario>)
make save-reload-oracle    # save-reload: boot 1's replay writes the captured CMOS byte-identically and boot 2 loads it (the five storage ranges at the pinned frame); in make verify; skips without data/k11-captures/save-reload
make gp-capture scenario=gp-pads  # DOSBox-X gameplay capture (frame-keyed injection, per-frame snapshot log); writes data/k11-captures/<scenario> (scenario names are gp-…)
make gp-replay scenario=gp-pads  # port replay of a gameplay capture (PR_GP_DUMP driver, .ipx frames + trace); in make verify on gp-pads, which skips without data/k11-captures/gp-pads
make gp-oracle             # gameplay oracle: frame + trace ratchets on data/k11-captures/gp-idle-loss (N values in the Makefile); in make verify; skips without the capture
make gp-charsel-oracle     # the same ratchets on data/k11-captures/gp-u5-charsel (the character-select walk; N 516 = how far the port's replay got, F 1513, the poll.log hash pinned); in make verify; skips without the capture
make gp-moves-oracle       # gameplay oracle: gp-u6-moves-b frame, trace and moves ratchets (N values in the Makefile); in make verify; skips without the capture
make gp-keys-oracle        # in-match keys: gp-keys-fight evidence + effects ratchet (GP_KEYS_MIN_EFFECTS and the poll.log sha256 in the Makefile); in make verify; skips without the capture
make gp-win-oracle         # U9: the win path under pokes, data/k11-captures/gp-u9-win: evidence (tools/gp_win.py check), frame/trace/milestone/win ratchets (GP_WIN_* in the Makefile); in make verify; skips without the capture
make gp-ending-oracle      # U10: the ending under pokes, data/k11-captures/gp-u10-ending (GP_ENDING_*); in make verify; skips without the capture
make gp-twop-oracle        # two-human gameplay oracle: gp-twop must be a two-human match (tools/gp_twop.py), then frame + trace + moves ratchets (N 612, F 1506 and moves N 1506 = the port script's end, the poll.log sha256 pinned in the Makefile); in make verify; skips without the capture
make gp-modes-oracle       # the same ratchets on the seven U8 captures data/k11-captures/gp-u8-* (the other START MENU rows and the attract start; GP_MODES_<ID>_* pins in the Makefile, an evidence check per capture, tools/gp_modes.py); in make verify; each skips without its capture
make gp-u12-oracle         # Gameplay U12: gp-u12-endurance's team pass and fight; in make verify; skips without data/k11-captures/gp-u12-endurance
make gp-u13-oracle         # Gameplay U13: gp-u13-ending2's non-character-0 ending; in make verify; skips without data/k11-captures/gp-u13-ending2
make gp-u14-oracle         # Gameplay U14: gp-u14-win-live's fought win; in make verify; skips without data/k11-captures/gp-u14-win-live
make gp-u15-oracle         # Gameplay U15: gp-u15-join's mid-round join; in make verify; skips without data/k11-captures/gp-u15-join
make gp-u16-oracle         # Gameplay U16: gp-u16-twop2's extended two-human match and frame evidence (finisher gap; §J8); in make verify; skips without data/k11-captures/gp-u16-twop2
make gp-u17-oracle         # host input truth U17: gp-u17-device2 must reach MODIFY CONTROLS and select device 2 with no pad (tools/gp_u17.py check: the S-record clear, the label band's screen proof, the poll.log sha256 and frame count); in make verify; skips without the capture; the frame/trace ratchets are empty (the wall-paced walk has no replay, record §H6.9)
make gp-u18-oracle         # endgame truth U18: TALON's ending (character 2, cell 1) on gp-u18-talon-ending; gp_win.py check + frame/trace/reach/WIN ratchets; in make verify; skips without the capture
make gp-u19-oracle         # endgame truth U19: VERTIGO's ending (character 3, cell 2); as gp-u18-oracle
make gp-u20-oracle         # endgame truth U20: CHAOS's ending (character 6, cell 4); as gp-u18-oracle
make gp-u21-oracle         # endgame truth U21: ARMADON's ending (character 4, cell 5); as gp-u18-oracle
make gp-u22-oracle         # endgame truth U22: DIABLO's ending (character 5, cell 6); as gp-u18-oracle
make gp-u23-oracle         # endgame truth U23: the U16 finisher reaction under the labelled poked-damage fallback on gp-u23-u16-finisher (gp_twop.py check --finisher without --allow-missing-reaction); in make verify; skips without the capture
make gp-u25-oracle         # state coverage U25: gp-u25-endurance-end's four-KO roster walk and match end (modes 0x32/0x33; record §D5); in make verify; skips without the capture
make gp-u26-oracle         # state coverage U26: gp-u26-side1-ending's side-1 tour and ARMADON ending (ad4=1; record §D5); in make verify; skips without the capture
make gp-draw-timeout-oracle # state coverage Task 7: gp-draw-timeout's timeout/health drawn round (record §D7); in make verify; skips without the capture
make coverage              # the PR_COVERAGE function census (build-cov + every driver with PR_HIT_TAG, then tools/coverage_census.py; NOT in make verify — it re-runs the drivers); COV_REASON="…" re-pins tools/coverage_baseline.txt
make gp-report scenario=gp-pads  # report-only comparison of a gp capture against its port replay (no ratchet, exit 0)
make audio-render          # FM music to a WAV (rendered audibility has no oracle)
make diff-verify           # differential verification: the original's bytes vs the port's C functions (skips without unicorn or capstone; in make verify)
make entry-triage          # E2: committed table must equal a fresh run (in make verify; skips without capstone or PRAGE.EXE; a caller-set PR_ORACLE_REQUIRED=1 makes that a failure, but verify does not export it globally)
```
- **A fresh git worktree needs the git-ignored test fixture copied in** (`port/tests/ghidra_data.bin`;
  `make re-oracle` regenerates it) before any `PR_ORACLE_REQUIRED=1` run or `make verify` — it lives
  in the main checkout and `git worktree add` does not bring it (track G lost three runs to this).

- **Run the binaries from the repo root.** Several tests default to the relative
  `data/game/C` (`PR_GAME_DIR` overrides it), and CMake's `add_test` sets the
  working directory to the repo root for the same reason.

- **The byte-exact oracles SKIP silently unless `PR_ORACLE_REQUIRED=1`** — the
  oracle fixtures are copies of the game's own bytes and are git-ignored. A run
  without it proves much less than it appears to. `make verify` sets it.
- `make verify`'s `--check` step **must precede** the unit suite: `test_fn_misslog`
  reads `frames/fn_miss.txt` (the run's `fn_resolve` miss log) from the CWD.
- There is no per-test filter: `run_tests` is one binary. The drivers that call
  `game_init()` are selected by env var and run alone (`PR_TITLE_DUMP`,
  `PR_ATTRACT_DUMP`, `PR_FRONTEND_DUMP`, `PR_FRONTEND_DET`, `PR_K11_DUMP`, `PR_GP_DUMP`,
  `PR_RESTART`).
- If a build invoked through `make` looks stale, `cmake --build build` is the
  reliable fallback.
- macOS host: no `timeout`. SDL audio open has failed before here and can fail on
  other hosts/runs (`-66681`, first observed in the 2026-09-16 audio report), but
  on this host it opens: `host_audio_open(49716,2) -> 1` (coreaudio; the windowed
  run prints `audio device open at 49716 Hz`) — rendered audibility still has no
  device-recording oracle, and `make audio-render` renders the FM path to a WAV
  (record `2026-10-15-reverse-j-derivations.md` §J2.5).
- Linux host: `run_tests` (and so `make verify`) runs under ASLR — no `setarch -R` needed.
  The `p45_check_p5_spawns` pool seed is consistent (the `0x48A20` arm assigns the pool base
  `DSD(0x1014F4) = Z_R1` before `c4r_pool()` builds the free list, so `actor_spawn`'s `pset`
  index stays in the pool), which retired the pre-existing ASLR-dependent out-of-bounds write
  (sub-project A, record `2026-10-16-reverse-a-derivations.md` §A2).
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

- **A live Ghidra MCP instance is usually available** (project `primal-rage`,
  `/PRAGE.EXE`); its addresses match the scheme above. Prefer it for data
  addresses, jump tables and cross-references.
- Ghidra headless + the exact import/re-export commands: `port/RE_GUIDE.md`.
  Needs `JAVA_HOME` (temurin-25) and the `ghidra-lx-loader` extension.
- `dosbox-x` for runtime ground truth (memory-file dumps, debugger breakpoints).
- Python 3 with `capstone` for ad-hoc disassembly, `Pillow` for `tools/gra_extract.py`.
- The differential harness (`make diff-verify`, `tools/diff_verify.py`) needs `unicorn` and `capstone` (`pip install -r tools/requirements-diff.txt`; pins `unicorn` 2.1.4 and `capstone` 5.0.7). Without either the step prints its skip line and `make verify` stays green. Its last line counts the functions VERIFIED, the mutants detected, the named gaps (`NAMED_GAP` rows: the original stops on the instruction the spec names) and, over the rows that have callees, those closed (VERIFIED with every callee VERIFIED by its own row — except the declared non-goal classes the closure rule excuses, record §J1; rows with no callee are counted apart, `41/126 rows with callees closed (23 have none)` after P6); a stubbed callee that is not is "unverified" in the table's callee column. At every recorded call it also compares the memory changed so far, and a stub poisons the registers its callee does not preserve. The original runs at the game's x87 control word `0x127F` (53-bit precision, round to nearest; `fldcw` at `0x72A83` of the word `0xF09B4`), which `run_original` seeds because unicorn resets it to `0x0000` (24-bit); `X87ControlWordTests` fails without the seed, and under it a C `double` sum stored to a `float` is the x87's (record `2026-10-03-reverse-p3-derivations.md` §P3.6, which names the limits: a control-word change outside the code object is not excluded). A stubbed predicate whose AL alone the original tests (`test al,al`) gets a case whose stub EAX is `0x100` and an `@eax`/`@al` mutant that only that case catches (`0x47688` h3, `0x48170` g4). The checklist for verifying a function (args from the callee's bytes, `pop`, `clobbers`, per-case stub EAX, pointer arguments as mem offsets, where the binding, the spec and the mutants go) is record `docs/superpowers/plans/2026-10-01-reverse-e3-derivations.md` §E3.10.
- `tools/` is the RE/oracle toolbox: `le_info.py`, `gra_render.py`,
  `gra_extract.py`, `gen_symbols.py`, `title_pin.py`, `title_compare.py`.
  Check the active plan before editing anything here.

## Porting rules (the ones that get missed)

- One C function per original function, header comment `/* 0xADDR — spec section */`.
- Mark deliberate deviations `/* PORT: ... */`; doubts `/* TODO(verify): ... */`.
  No other comment styles in `port/src`.
- Code addresses stored in data go through `fn_origin()` / `fn_resolve()`.
- A ported function that a differentially verified function stubs or records (a call-set callee,
  `E.Call` in `tools/diff_verify.py`) opens with `PR_SEAM(0xADDR, args...)` (void) or
  `PR_SEAM_RET(0xADDR, args...)` (`mem.h`): the harness's call seam, inert outside `build/diffrun`.
  The arguments are the C signature's, in order (a pointer into `mem[]` as its offset); the `E.Call`
  names the original's registers and stack slots in the same order. A callee with no argument opens with
  `PR_SEAM0(0xADDR)`; a buffer on the caller's stack that the callee cannot run on both sides is passed by
  value (its bytes as little-endian dwords, the `E.Call` naming `[reg]`, `[reg+N]`; record
  `2026-10-02-reverse-p2-derivations.md` §P2.7). An allow-mode callee (run on
  both sides and not recorded, such as `0x33950` and `0x339AC`) has no seam. A batch
  that stubs a callee it does not own seams it and leaves the differential row to the
  callee-row batch: P4+P5's `0x29C08` opens `fighter_29c08` with `PR_SEAM_RET` and
  `0x2AD40` opens `release_record` with `PR_SEAM`; both rows are open after C1's 20
  (record 2026-10-03-reverse-p4-p5 §P4.8). Record
  `2026-10-01-reverse-e3-derivations.md` §E3.3-§E3.5; the checklist is §E3.10.
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

## Unit testing: write fewer, better tests

Every test is code someone has to read, maintain, and wait on in `make verify`.
A test earns its place only if it can fail for a reason no other test already
covers. Optimize for distinct behaviors verified, not for test count or
coverage numbers. This section is how to choose what to write; the bullets above
stay binding (the mutation proof of a new assertion, the seeded sentinels, the
consolidation gate).

### Before writing any test

1. Read the existing tests for the code you're touching. If a behavior is
   already covered, do not cover it again. Extend or adjust the existing test
   instead of adding a new one beside it.
2. List the distinct behaviors you need to verify (one line each). If two items
   on the list would fail for the same underlying bug, merge them.
3. For each remaining item, ask: "If I deleted this test, what bug could slip
   through that the other tests would miss?" If you can't name one, don't
   write it.

### What counts as redundant

- Multiple inputs from the same equivalence class (e.g. testing 3, 5, and 7
  when any positive integer exercises the same path). Pick one representative
  plus the boundaries.
- The same logic tested at several layers. Test it once at the lowest layer
  that owns it; higher layers only test their own wiring and logic.
- Tests that differ only in input and expected values. Collapse them into one
  table-driven/parameterized test.
- A new test that is a strict subset of an existing, broader one.

### What not to test at all

- Trivial code with no logic: getters, setters, plain constructors, constants,
  simple delegation.
- The language, standard library, framework, or third-party dependencies.
- Implementation details: private helpers, internal call order, or mock
  interactions that merely restate the implementation. Test observable behavior
  through the public interface.
- Scenarios that the type system or compiler already makes impossible.

### What you should still test

- Each distinct branch or behavior of the public contract, once.
- Boundaries and edge cases (empty, zero, max, nil/null, off-by-one).
- Error paths that have distinct handling.
- A regression test for each bug you fix, targeting that specific bug.

### When changing existing code

- Update the tests that cover the changed behavior rather than adding parallel
  ones. Delete tests made obsolete by the change.
- Don't add tests for code you didn't change unless asked.

### Reporting

- When you finish, state briefly which behaviors you tested and any you
  deliberately skipped as redundant or trivial, so the reviewer can disagree
  if needed.

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
  capture's first attract. The attract prefix oracle itself (`make attract-oracle`)
  likewise pins the count of port attract frames the capture exhibits
  (`ATTRACT_MIN_PORT_EXH` = 173, sub-project A A1c), so a port that renders fewer
  attract frames fails; the window's interior coverage is otherwise not claimed.
- The front-end oracle's claim is narrow: it proves only that no content-bearing
  capture frame inside the window the port exhibits is unexplained and that the
  window reaches its pinned END (`FRONTEND_END`, sub-project A A1b), so a port
  that stops early fails. But the window START is still derived from the port's
  own dump and coverage *inside* the window is not ratcheted (a mid-window port
  frame no capture frame exhibits is ignored). Do not read a green oracle as
  "the frame is correct".
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
  port that never restarts. It now also covers every non-black port frame up to
  the final settled screen and pins the distinct exhibited count (`K11_WALK_MIN_EXH`
  = 36, `K11_MENUESC_MIN_EXH` = 184, `--min-exh`; sub-project A A1c), so a port
  that renders fewer frames fails.
  The gameplay oracle (`make gp-oracle`, in `make verify`) ratchets the first
  unexplained capture frame and the first differing trace frame of
  `data/k11-captures/gp-idle-loss`; its N values and provenance are in the
  Makefile (with the capture's `poll.log` sha256: another capture fails); it also pins the
  port-frame window reach (`GP_*_MIN_REACH`, one per enforced gp scenario; sub-project A A1a), so a
  port that renders fewer frames fails; it skips without the capture,
  even under `PR_ORACLE_REQUIRED`. The `PR_GP_DUMP` driver pins its own `fn_resolve` miss set per
  scenario (`test_platform.c`, record gameplay-ground-truth §G.24).
  `make gp-charsel-oracle` (in `make verify`) does the same for `data/k11-captures/gp-u5-charsel`
  (record gameplay-u5 §C5.18): N = 516 is the script's end (how far the port's replay got, not a
  divergence), F = 1513, the `poll.log` sha256 and frame count are pinned; it skips without the capture. Its claim is narrow the same way (record
  `…-gameplay-ground-truth-derivations.md` §G.16): no content-bearing capture
  frame from the window start up to N is unexplained and the traced fields
  agree below F; the window start is pinned too (`GP_IDLE_LOSS_MAX_START`), so a
  regressed port cannot slide it forward. It cannot detect a port that
  under-renders *inside* the window, and **the order of the port's frames and that every port frame
  appears are not claimed** (a named gap; a coverage count is only reported). Where
  the port's script ends before the capture the first unexplained frame is how far
  the port got, not a defect.
  `make gp-moves-oracle` does the same for `data/k11-captures/gp-u6-moves-b` and adds a
  third ratchet, `moves`, over the snapshot's move bytes (`c0 c1 r0 r1 s0_43`, record
  gameplay-u6 §U6.11); its claim is as narrow.
  `make gp-keys-oracle` (in `make verify`; record gameplay-u11 §K.10/§K.14 and
  track-G §G5) judges the 11 in-match
  key events of `data/k11-captures/gp-keys-fight` on state: at each event's capture frame the
  capture shows the raw-derived change (`evidence`) and the port's replay shows the same one
  (`effects`, ratchet `GP_KEYS_MIN_EFFECTS` = 11, `poll.log` sha256 pinned), judged on the latch,
  pause bytes, pad words, `mode`, `b1f`, `cred` and `rng`; and (track G task 5) ratchets the
  frames: every content-bearing capture frame of the pinned window (start ≤ `GP_KEYS_MAX_START`
  = 100) below `GP_KEYS_MIN_FIRST` = 1136 is explained (clean/splice/transition) against the
  port's replay — the pause, ABANDON and QUIT screens' first pixel claim — with capture 835
  (the QUIT event's scan-out catch-up) allowed by name ONLY at its pinned bytes (rows 0..80
  and 92..199 byte-bound to the port's current `f = 0x832`/`0x833` frames, the 11-row band
  pinned by sha256; that band holds a present state no port frame carries — the §K.10 item 2
  named gap stays its cause, not isolated); N = 1136 is the end of the port's script (the
  capture's X record; its 30-frame boot-movie tail is how far the port got, not claimed),
  the trace ratchets at `GP_KEYS_TRACE_MIN_FIRST` = 2164 and the capture identity is pinned
  (sha256 + `GP_KEYS_CAPTURE_FRAMES` = 1166, the same capture U11 pinned); the claim is narrow
  the same way — no under-render detection *inside* the window, order and coverage not claimed;
  it skips without the capture.
  `make gp-twop-oracle` (in `make verify`; record gameplay-u7 §T.8-§T.11) checks that
  `data/k11-captures/gp-twop` is a two-human match from P2's join in the character select (f=2C3, `cred 4 -> 4`)
  to the scenario's end (`tools/gp_twop.py check`, from the raw's command-word writers), then ratchets as
  `gp-oracle`: N = 612 and F = 1506 are the end of the port's script (how far the port got, not divergences), the
  window start 83 and the `poll.log` sha256 and frame count are pinned; the moves claim is pinned too (N 1506).
  Its claim is as narrow, and it says nothing about the two-player game past the scenario's end; it skips without
  the capture.
  `make gp-modes-oracle` (in `make verify`) applies the same two ratchets to the U8 captures `data/k11-captures/gp-u8-*` (the other START MENU rows and the attract start), each with `GP_MODES_<ID>_*` values and provenance in the Makefile and an evidence check (`tools/gp_modes.py`) that the capture reached its row; each skips without its capture. Record gameplay-u8 §U8.23 has the table of N, F and the window start per scenario and the named gaps: N and F are the end of the port's replay in all seven (RA reached its end once track P batch 2 ported `0x14EF8`/`0x14F50` and their streams' targets: N 726 -> 1072, F 1978 -> 2274, record 2026-10-02-reverse-p2 §P2.4; EN's end is its 300-frame team-select cut); nothing past them is covered by these U8 replays, ENDURANCE's fight is now covered through modes `0x30`/`0x31` by `gp-u12-oracle` (§J4), and the damaged-frame proof depends on the frame chosen.
  `make gp-win-oracle`/`gp-ending-oracle` (plan 2026-10-02-gameplay-u9-u10, record §W.9) reach the
  win and the ending under memory pokes (a `poke` step writes bytes in the spin; the port replays
  the capture's `W` records as `poke` lines at W.f + 1, the iteration that reads them, record §W.8). Their claim is as narrow, and what
  a poke replaced is not claimed: the KO by hits, the six earlier won matches, the final's death
  animations. The U9/U10 captures cover character 0; character 1's BLIZZARD ending is captured and pinned by `gp-u13-oracle` (§J5), while the U14 live-win remains a distinct gap (R1, §J6). Both frame ratchets now pass round 1's mode-8 entry and the mode-0x17→0x12 loader strip by name (track G §G6/§G7): the strips at U9 2209 and U10 2062 are the loader's `- LOADING -` presentation — the same 166 px six-row strip (rows 192..197, x 0..85, sha a886c0f3.., glyph fingerprint de1c1bb2..) — composed over the entry screen, a host-owned-audio model artifact allowed by name in `OVERLAY_ALLOWED`, not a named gap; the first unexplained is U9 2452 (the replay's post-script tail) and U10 5437 (the replay's post-script tail), and the pixel claims now reach U9's replay end (f=0xDAE, the X record) and U10's replay end (capture 4662; the replay itself runs on to f=0x26E1), so the results screens, the following rounds' fights and U10's final through the replay's end are under them; U9's trace and WIN fields now agree to its replay's end too (3503, `0 differing through 3502`; the milestone frames, mostly mode changes driven by replayed pokes, do not check which opponent is fought); U10's trace and WIN fields agree to the replay's end (f=0x26E1) since track P batch 2 ported `0x2381C` (record §W.16); the replay now *reaches* the once-missed `0x29C78`/`0x37DD4` (P7 and P6 ported them — record §P7.8, §P6.12), but the traced fields cannot see their per-hit death-stream effects, so the clean trace to f=0x26E1 does not claim the death streams or the mode-0xF content are reproduced and is no evidence of correctness past the frame ratchet's 5437. `gp-u10-ending` runs DOSBox-X with
  `memsize=64`, a harness value (the original ran out of memory in the poked final at 16 MB,
  record §W.13; the memory growth is not root-caused, and `memsize=64` claims nothing about the game's memory need). The mode-8 entry frames the ratchets used to stop at (U9 346, U10 331, gp-idle-loss 2064) hold one screen three consecutive scans and then put two presents inside one 70.09 Hz scan-out: track G §G6 isolated that as the loop's own catch-up (an iteration whose compute overruns an ISR tick fails the 0x25643 gate and skips its present; the re-opened presents land sub-tick apart) — a comparison-model limit, not a port divergence, the loader re-sync hypothesis tested and rejected. Those frames are allowed by name in `SCAN3_ALLOWED` with per-frame byte recipes (a pinned hash row strip that an earlier dump frame must hold + one unique adjacent pair binding the rows below; mutation-tested per §G5's discipline), the U9/U10 entry strips in `OVERLAY_ALLOWED` (the same discipline, §G7), and the ratchets moved up to U9 2452 / U10 5437 / idle-loss 7559 (the idle-loss and U9 ones are past the replay script's end — how far the port got — and U10's 5437 is the replay's post-script tail; the 4113–4134 death-animation run is explained by the span-writer carry fix, record §W1). A P batch that ports
  a target in a miss set re-measures and re-pins (U10: `0x29C78` P7; record §W.16;
  track P batch 3 ported `0x475EC`, record 2026-10-03-reverse-p3 §P3.9; batches 4+5 ported
  `0x400E0`/`0x21084` P4, `0x21044` P5 and `0x3DA50` P5: U9's set gains `0x213F0`/`0x213F4` P7 and
  `0x2BDA0` P6 (first frames 0x8ED, 0x91F, 0xD0C) and its TRACE pin rises 2150 -> 2364; U10's pins
  are unchanged; record 2026-10-03-reverse-p4-p5 §P4.7/§P5.7; batch 6 ported `0x2BDA0` P6: U9's
  set loses that row alone (distinct=6) and every U9 pin is unchanged and exact — the trace's first
  difference f=0x93C precedes the target's first frame 0xD0C — and U10's set loses `0x37DD4` alone
  (distinct=5; `0x29C78` P7 remains) with every U10 pin unchanged — record 2026-10-03-reverse-p6
  §P6.12; batch 7 ported `0x213F0`/`0x213F4` P7 (task 2): U9's set loses both rows but gains
  `0x36114` (task 3's D000 target, which the replay then reaches; distinct=5), TRACE rises
  2364 -> 2377 and WIN stays 3162 on the task-2 tree; task 3 ports `0x36114`, so the set loses
  that row too (distinct=4), and U9's
  TRACE and WIN pins rise 2364/3162 -> 3503 (MIN_FIRST 346 and MAX_START 100 unchanged and exact),
  and `0x29C78` P7: U10's set loses it (distinct=4) with every pin exact and unchanged — record
  2026-10-04-reverse-p7 §P7.8).
- **The endgame truth phase: all seven endings and the U16 finisher are captured; U14 R1 stays
  a named gap** (plan `docs/superpowers/plans/2026-10-23-endgame-truth.md`, record
  `2026-10-23-endgame-truth-derivations.md` §E2–§E11). Five ending captures clone U13's poked win
  path — `gp-u18-talon-ending` (TALON, character 2, cell 1), `gp-u19-vertigo-ending` (VERTIGO, 3,
  cell 2), `gp-u20-chaos-ending` (CHAOS, 6, cell 4), `gp-u21-armadon-ending` (ARMADON, 4, cell 5)
  and `gp-u22-diablo-ending` (DIABLO, 5, cell 6) — each pinned by `gp-u18-oracle`…`gp-u22-oracle`
  (all in `make verify`, each skipping only without its capture): `gp_win.py check` (30/30
  milestones, the mode-0x1F `ad4=0,c0=char` row under the character-name label), the
  frame/trace/reach ratchets, the WIN ratchet, the `poll.log` sha256 and frame count, plus each
  capture's measured per-frame allowances (`SCAN3_ALLOWED` three-present scans, the loader strip in
  `OVERLAY_ALLOWED`, ARMADON's `BANDS_ALLOWED` present-copy scan-outs and DIABLO's fade bands).
  `gp-u23-oracle` (record §E2) captures the U16 finisher reaction under the labelled poked-damage
  fallback: the live KO could not be reconciled with the narrow `0x36F10` gate window (six
  attempts, record §E2), so P2's damage byte `0x10789E` is poked to `0x77` and a real hit takes it
  to `0x78`; `gp_twop.py check --finisher` runs without `--allow-missing-reaction` and the capture
  reaches `r0=0x33`. Its moves ratchet records a measured port divergence: the replay leaves
  `r0=0x27` where the capture advances to `0x33` (the capture also holds `0x27` at `f=0x14F7`).
  **The claims are the ending content and the finisher reaction, not the fight**: the WIN_MENU
  path, the seven land marks, the `0x10789E` damage and the `DEATH_DONE` pokes are harness writes,
  so the KO by hits and the six earlier won matches are not claimed. U14's fought live win (R1) is
  a named gap: the fought-match-truth phase's one authorized attempt (2026-10-26) lost the game's
  match (P1 KO'd in both rounds, P2 VERTIGO `c1=3`/`afc=3`, `b0c` never 1, mode 0x12 never opened)
  and also failed its harness checks, so its artifact is preserved as
  `data/k11-captures/gp-u14-win-live.failed`; the published target is absent, all nine `GP_U14_*`
  pins (the eight plus `MAX_MISSING`) are empty, `GATE_NAMED_GAPS` keeps it declared, and
  `gp-u14-oracle` keeps its absent-capture skip. (The endgame-truth plan's Task 8 had declined a
  further attempt after gameplay truth II's two; this phase's attempt ran under its own go-ahead.)
  U13's post-script tail is
  claimed (record §E9): `port_script`'s opt-in `--end-to-last-s` runs the script to the capture's
  last `S` (f=10068), the 5891..5903 tail is byte-identical to the replay's own 4940..4952 frames,
  and `GP_U13_MIN_FIRST`/`MIN_REACH`/`TRACE`/`WIN` read 5904/4951/10069/10069. The game-set `b0c`
  is **not** a gap and needs no new capture: it is U10's `GP_ENDING_WIN_MIN_FIRST=9954` (the write
  at f=0x1602 in mode 0x0F, 190 frames after the mode-0x0F entry (`f=0x1544`) and 200 after the
  seventh poked final KO's `S` — record §E1.3); only the
  fought (no-poke) win is open (`live_death`'s `(0x09,1)` rule). The INT 8 tick item is done
  (record §E1.6/§E10): the install `0x66519..0x6652D` sets vector 8 to `0x6627E`, the PIT divisor
  is 19,887 (59.998 Hz), and `0x2D62C` is the counter the AIL callback `0x1BDF4` calls — the
  installer stays host-owned. Named gaps left by the phase: U14 R1, the 5120 band (a byte-pinned
  13-pixel allowance whose cause is INCONCLUSIVE — the state-coverage §D4 aperture read is invalid,
  the DOSBox-X memory file excluding the VGA aperture), the differential frontier, and the poked-win
  status above.
- **The input sampler and its readers are ported; the game logic has no joystick path.** `0x2D2F0`
  (`input_joystick_device`) is `xor eax,eax; ret` (raw `31 c0 c3`; it has its own differential
  row), so `0x4F644` stores 0 in `0x1088D4`/`0x1088DC`; its gate at `0x4F68B`
  (`test byte [0x1088d4],2; je`) never opens and the merge at `0x4F694`/`0x4F699` never runs
  (the row's `unhit_named` `0x4F694` says the gate is always taken). The original's only gameplay
  game-port sampling is the ISR sampler `0x1BBAC` (called at `0x1BE1C`; the key-config
  device words `+0x2D4/+0x2D6` select keyboard 0 or joystick 2/4/6, and its arms write the same
  key bitmap `+0x2D8/+0x2D9` that `0x500C4` reads). The host input truth phase ported it and its 12
  pure readers — the six keyboard readers `0x1B610`/`0x1B6A0`/`0x1B730`/`0x1B7C0`/`0x1B850`/`0x1B870`,
  the three game-port readers `0x1B890`/`0x1B8DC`/`0x1B908` and the two axis readers plus the
  sampler `0x4FF8F`/`0x4FFD8`/`0x1BBAC` (`port/src/platform/input.c`, record
  `2026-10-21-host-input-truth-derivations.md` §H2-§H4) — so the host fills the BIOS key-state
  table and the four axis words and the sampler writes the bytes; `0x1B890` is the both-player
  game-port row the pad's button map is judged by. The six PIT/game-port routines (`0x4FDF5`,
  `0x4FC05`, `0x4FCFD`, `0x4FBBB`, `0x5004A`, `0x50021`) and the 60 Hz AIL callback `0x1BDF4` stay
  host-owned (`tools/port_classification.txt`): `0x4FBBB`'s probe answer is `host_port_present()`,
  `0x5004A`/`0x50021`'s centre capture is `host_pad_center()`, and with no pad the sampler's
  presence block clears a joystick device word on its next tick (`0x1BCBF`'s P1-only arm;
  `0x1BC6F` is the both-player one) — record §H6.1/§H6.3. `game_boot_ports` models
  `0x20D0F..0x20DE3`'s boot clears (§H6.2).
- **The SDL gamepad is additive host input, not a ported path** (record §H5). `host_pad_open`/
  `host_pad_poll`/`host_pad_fill` (`host.c`) map the left stick to axes A, the right stick to axes B
  and the four face buttons to the game's 1/2/4/8 (grounding port bits 4..7; active low), gated by
  the device words — a pad connected while the config says keyboard (device 0) feeds nothing, and
  `host_pad_fill` leaves a keyboard player's axis words at `host_input_fill_axes`' centres. The SDL
  axis scale is **host policy** (`HOST_PAD_AXIS_SCALE 64`, declared, not fitted); the seam
  `host_set_pad_override` drives the whole model in `test_platform.c` with no SDL. The default
  device 0/0 path is byte-identical to the raw's bytes and no existing capture pin moved (§H4.5,
  §H6.7). `gp-u17-device2` pins the no-pad clear and the device-2 selection's screen proof
  (`make gp-u17-oracle`); its frame/trace ratchets are empty because the walk is wall-paced and
  the frame-keyed replay cannot follow it (§H6.9's named gap).
- **The movie audio needs no port.** Both shipped movies declare zero audio tracks, so no stream
  opens and the 13 deferred movie-audio rows are a zero-payload finding, not an implementation gap;
  `0x10678` is the ms reader (`[0x81E10]*4`, not a buffer size), record §H1. A report-only
  Alt-J/boot probe is optional and is not a `gp-*` fixture (record gameplay-truth-ii §J9).
- **The music truth phase: the in-match/menu music plays; the register stream is the claim**
  (plan `docs/superpowers/plans/2026-10-25-music-truth.md`, record
  `2026-10-25-music-truth-derivations.md` §M1–§M6). `0x1C930` is ported as
  `snd_music_start` (the pending arm at `0x1CF20`'s `0x1CF39`): `game_audio_init` stores
  the non-zero stand-in `1` at `DS_001028C0`/`DS_001028C4` (the install-succeeded form the
  DIG path already used at `0x1028C8`), so `snd_music_request` (`0x1CA14`) queues the
  game's own stage/menu song and the resolve copies the bank into the raw's `0x5100`
  buffer and starts it. The title bank starts **once** (`game_state_title_music_request`
  suppresses `title_music_start` when the attract's `0x54`/`0x56` request is pending or
  last started; its body is unchanged), and `seq_start` no longer resets the FM driver's
  voice-rotation cursor — the raw's `AIL_start_sequence` resets the sequence record only,
  so a second song continues the cursor (the capture's selmo first note is OPL channel 4
  after the title's 0..3); `seq_driver_reset()` is the test-only pristine seam. The
  register-stream oracle `tools/audio_compare.py` (`make audio-oracle`, in `make verify`)
  compares `PR_AUDIO_DUMP` against the 29 gameplay `gp-*` `.dro` traces, anchored at each
  section's first key-on, tolerance-free across ticks (sorted per-tick multisets for the
  declared intra-tick order model; the measured one-tick capture-label model; a skipped
  group must be empty and every port write matched once), skipping absent traces by name;
  the selmo section is per-tick exact in 24 of the 26 music traces
  (`gp-u8-attract-start`'s tick-57 jitter and `gp-u23-u16-finisher`'s capture tick split;
  `gp-u22-diablo-ending`'s title is a separate label-slack/stop named gap). The nine ending
  captures' loader `OVERLAY_ALLOWED` entries retired as clean matches and their
  `MAX_MISSING`/`MIN_REACH` pins were re-measured (record §M2.7). **Named gaps:** rendered
  audibility (the OPL core/mixer is an invented model); 16 of the 26 code-reachable banks
  are not requested or attributed in the measured cut windows, and 5 more (citys, himal,
  caves, sound+0x5B88, sound+0x3E64) are requested-only named gaps — only title,
  selmo+0x25C, selmo+0x08, beach+0x08 and jungl+0x08 are capture-matched; the later
  `0x31`/match sections the capture's cursor advanced past; and `0x1C930`/`0x1CF40` stay
  `UNROWED` (a row would fabricate the AIL handle and could not compare the hand-off). The
  PCM pin is **determinism, not audibility**: `make audio-render` renders the selected
  selmo (`0x0980025C`, volume `0x50`) at peak **2790**, sha256
  `530a94301f4be46f9a5c666c94516f8a1f7fca6f136d3be0562faed802c47cef` (two byte-identical
  runs; the title WAV is unchanged by Task 5 — `515b7c94…`/4,090 followed Task 3's
  voice-cursor correction from the Task 1 `df74acfb…`/3,402 baseline). The phase's required gate is **both**
  `PR_DIFF_FULLSCAN=1 GATE_STRICT=1 PR_PINS_EXACT=1 make verify` **and** `make audio-render`.
  Task 6 is declared, not taken (the optional service-menu language reload, the
  coin-countdown capture the no-new-capture constraint forbids, and the proof-completion
  housekeeping). The title/attract 9866-write oracle and every existing pin are unchanged.
- **The fought-match truth phase: the harness and two measurements, not the two claims** (plan
  `docs/superpowers/plans/2026-10-26-fought-match-truth.md`, record
  `2026-10-26-fought-match-truth-derivations.md` §F1–§F6; design
  `2026-10-25-fought-match-truth-design.md`, status **Partially implemented**). The harness gained
  the four fighter-slot state fields (`s0_30`/`s0_42`/`s1_30`/`s1_42` — the `+0x30` dword and the
  `+0x42` word) and stop/skip-on-mode; no game code changed. **U14 R1 stays open:** the one
  authorized capture (2026-10-26) lost the game's match (P1 KO'd both rounds, P2 VERTIGO
  `c1=3`/`afc=3`, `b0c` never 1, mode 0x12 never opened) and also failed its harness checks, so it
  is preserved as `data/k11-captures/gp-u14-win-live.failed`; the published target is absent and
  all nine `GP_U14_*` pins (the eight plus `MAX_MISSING`) are empty, so `gp-u14-oracle` skips and
  `GATE_NAMED_GAPS` keeps the path declared (the gate reads 34/35). **The two-human KO is
  game-evidence-only:** the first run and its CPU-confined recovery each produced the game's
  hand-dealt KO (`gp_twop.py check --ko`: `ko f=B9A` then `f=B91`, P1's round result
  `ad4=0xFFFFFFFF`, no pokes, `w2=1`/`w3=0`), but both failed the port-script pin under host
  starvation; the recovery's publish path replaced the first `.failed` artifact (the fix round's
  `publish()` now keeps a later failure under a numbered `.failed.N` suffix instead), so only the
  recovery's `data/k11-captures/gp-twop-ko.failed` survives and the first run's rows are the
  record's §F4.3–§F4.5. There is no published `gp-twop-ko`, no `gp-twop-ko-oracle`, no
  `GP_TWOP_KO_*` pin, no miss set and no gate entry. **U16 R2 stays open:** the recovery measured
  the `0x4844C` case-3 exit at the old marker's exit frame, `f=0x1533` (inferred: the `s0_54=2`
  set record is missing), as `s0_30=0x1180`, `s0_42=0x8000`, below the `0x1400`
  gate, against the port's set `0x202E` at `0x1532` and exit `0x1180` at `0x155C`. The deciding
  gate input is now measured by the `t9-r5` diagnostic (set `f=0x1531`, `y = 0x0EAE < 0x1400`; the
  clear is strict, `y < 0x1400`; claim-coverage §C5, live-truth §L4), and the `.failed` artifact is
  not that measurement. No fix, no
  oracle/miss set, no third capture. `gp-harness-smoke`, `gp-twop-ko`, `gp-u14-win-live.failed`,
  `gp-u24-u16-state` and its two failed runs are
  not in `CAPTURE_DIRS` and their `.dro` files are not in `tools/audio_compare.py`'s 29-trace
  inventory (unattributed; no audio claim). The phase adds no oracle, no capture path and no
  measured pin (the nine `GP_U14_*` are empty);
  `port_progress.py` stays `799 1203 66` / `759 759 100` and `make diff-verify` stays
  `1033/1033 functions VERIFIED; 14993/14993 mutants detected; 0 named gaps`. The phase's full
  gate stopped at the register-stream audio oracle under host load (`gp-u18-talon-ending`'s beach
  section, then `gp-u13-ending2`'s jungl section on a standalone retry: section-anchor frame/tick
  shifts; a standalone U18 run passed at 968 ticks and failed at 716, the dumps' register/value
  data identical and only their labels differing — 180 `W` frame labels one lower, two `W` tick
  labels 308→310 and one `S` `ticks` 2454→2456; no pin weakened); the remaining steps are green
  separately (record §F6.3).
- **The out-of-scope content modes `0x21` (volleyball), `0x25` (both-characters-4
  camera) and `0x0E` (continue screen) are declared, not captured** (state-coverage
  record `docs/superpowers/plans/2026-10-28-state-coverage-derivations.md` §D2). `DS_00104ABC =
  (DS_00104B1F == 3) + 1` — written at `flow.c:1632` (`flow_player_join`) and `flow.c:2416`
  (the round-end case-3 arm) — so `DS_00104ABC >= 2` iff `b1f == 3` (a two-human match), and both
  in-match entries read it: `0x21` stores the mode at `0x4BD98` behind `fight_4b470`'s eighth-hit
  tail (`+0x1F > 7` = eight worshipper tramples of one effects entry, `fight.c:3084`;
  `DS_00104B1D` not 2/3, `DS_00104AFC`/`DS_001088C1`/`DS_001088C5`/`DS_00108864` clear,
  `(DS_001088EF >> 24) > 1`, then `DS_00104AD8 <= 0`, `DS_00104ABC >= 2`, `fight_13134` false,
  the two `0xBAB88`/`0xBAB9C` actor spawns and the `0x1E` timer `DS_001088AA`), and `0x25` at
  `0x4E11C` behind `DS_001078FA == 2`, `DS_00108892 >= 6`, slot 0's `+0x53 == 0`,
  `DS_00104B1D` not 2/3, the camera within `±0x3300`, `DS_00104AD8 <= 0`, `DS_00104ABC >= 2`,
  both characters 4 (mirror-ARMADON) and `DS_001088C4 == 0`. `0x0E` opens on a KO and runs the
  countdown `DS_00108110` (starts `0xF`, reset to `0xA` on a forced tick, decremented every 64
  master ticks or on a press) and the `config_credit_ready` credit. A poked 1P scenario cannot
  open `0x21`/`0x25` (no poke writes `b1f`/`DS_00104ABC`), so none is built. The census rows
  `0x4BD98`/`0x4E11C`/`0x27A2C` are `NOT-HIT` (baseline l.696/714/255; the trample `0x4B470` is
  `HIT`); `0x0B` (the drawn winner pose) stays unexhibited (claim-coverage Task 11's `gp-draw`
  did not publish).
- **The proof debt is declared terminal** (state-coverage Task 13, record §D7; the measured
  counters, re-measured not copied): `make diff-verify` prints `1033/1033 functions VERIFIED;
  14993/14993 mutants detected; 0 named gaps; 825/845 rows with callees closed (188 have none);
  809/845 closed on full rows; 8 ported functions with no row`. **20 open rows over 19 distinct
  addresses** (`TERMINAL_CENSUS`: 15 AIL runtime thunks, `sound_buffers_alloc` `0x1D0BC`,
  `input_sampler` `0x1BBAC`, and the three runtime exit-flush rows `config_file_load`/`config_file_save`/
  `menu_fatal_error` on the unrowed `0x6C4CF`; `attract_config_volumes_unscaled` and
  `attract_config_volumes` are two specs at `0x2C8F0`), **13 rows are `WEAKER`**, and **8 portable
  functions are `UNROWED`** — the three lists are the harness's `TERMINAL_CENSUS`/`WEAKER_CENSUS`/
  `UNROWED_CENSUS` tables in `tools/tests/test_diff_verify.py`, keyed to the counter by their tests.
  The span census is **417 slots / 392 distinct / 385 writer slots (384 reachable + dead `0x57E3C`)
  plus the out-of-table `0x5D110`** (`0x81110[128] == 0x81310[0]`, the 386th target, reachable), and
  the E2 triage has **388 `span-writer` rows plus the supplement `0x5D004`** (`0x81110[127]`, the one
  writer slot `>= RUNTIME_BASE`, named in the supplement). **U17** (the wall-paced walk, no
  tick-keyed replay; its screens already covered by the K11 captures) and **U14** (the fought live
  win) are declared, neither run; U14's `live_death` rule anchors on the match-1 round-2 KO but
  `b0c`'s only setter (`0x37FF6` in `0x37EA0`) runs on the ending path, and the two re-scope options
  are (a) drop the `b0c` clause and claim "fought KOs, no pokes", or (b) move to the final — with the
  measured correction that `b0c`'s ending write is **not universal** (BLIZZARD U13 and ARMADON U21
  keep `b0c = 0`). The claim-coverage design's undefined **B2–B8 labels** are replaced by the census's
  measured NOT-HIT set (**102** addresses, §D7.5, §D8.3; Task 3's original pin
  was 110) and the declared modes; the census baseline is a
  separate pin (its own `tools/coverage_baseline.txt`, not the `tools/pin_baseline.sha256`
  capture-pin tripwire). One `sprite_blit_aperture` (`0x51ED8`) pool mismatch was noted (three reruns
  green, not reproduced). No row, class or universe is widened and no capture or pin changed.
- **The state coverage phase is implemented and gated** (spec
  `docs/superpowers/specs/2026-10-28-state-coverage-design.md`, record
  `docs/superpowers/plans/2026-10-28-state-coverage-derivations.md` §D1–§D8;
  two full `PR_DIFF_FULLSCAN=1 GATE_STRICT=1 PR_PINS_EXACT=1 make verify` runs
  plus `make audio-render`, green 2026-10-29). **The census** (`make coverage`,
  outside `verify`) reads **HIT 653 / NOT-HIT 102 / UNINSTRUMENTED 4** (sums to
  759; the 4 are `0x29D60`, `0x2EA30`, `0x2EA64`, `0x500BB`, pinned in
  `tools/coverage_baseline.txt`; Task 14 re-pinned it from the Task 3 baseline's
  `HIT 645 / NOT-HIT 110` — R17/§D8.3 — the eight `improved:` addresses measured
  as `gp-u25-endurance-end`'s `0x292D4`/`0x29638`/`0x296B8` and
  `gp-draw-timeout`'s `0x19820`/`0x19958`/`0x27ED8`/`0x28BD4`/`0x28C38`).
  **The exhibits:** ENDURANCE's team end
  (`gp-u25-endurance-end`, `af1` `0 → 4`, stop mode `0x17`, 1,360 frames), the
  side-1 tour (`gp-u26-side1-ending`, `ad4=1`/`b1f=2`/`c1=4`, **268 mode-`0x1F`
  frames byte-identical to `gp-u21`**), the timeout draw (`gp-draw-timeout`,
  `clock=0` from the `W` record, `b16` not asserted) and the name-entry exhibit
  (`gp-u27-name-entry`, sub-states `b25 00 → 04 → 05 → 06 → 07 → 0A`, three
  letters, "ABD" — an exhibit, not a gated oracle). **The U16 fix** (record
  §D3): the reaction's record-motion gap is a null table base — the stream
  reads the IVT at linear 0 — fixed by `bios_ivt_stand_in()` seeding
  `mem[1] = 0xCA`/`mem[3] = 0xF0`; only `GP_U23_TRACE_MIN_FIRST` and
  `GP_U23_MOVES_MIN_FIRST` re-pinned (`5449`/`5467 → 6050`). **The 5120 band**
  is **INCONCLUSIVE** (§D4): the DOSBox-X memory file excludes the VGA aperture,
  so the diagnostic's 14 bytes read `0xFF` and the band stays a declared gap.
  **The OPL pitch test** (§D6) renders 439.9915 Hz (measured 439.0000 Hz)
  within the 1.7586 Hz derived tolerance. The two-human KO stays
  game-evidence-only, and `gp-u27-name-entry` is an exhibit, not a gated
  oracle (its replay has the pinned miss set `k_miss_gp_u27_name_entry`, but
  there is no `gp-u27-oracle` and it is not in `make verify`).
- Captures are git-ignored. `data/` is git-ignored and read-only — never write to it.
- **The storage layer is ported (track S) and every capture ran on an all-zero `CMOS`.** The image
  `0x100CE4..0x1014DB` is loaded by `config_file_load` (`0x1AFE8`) and saved by `config_file_save`
  (`0x1B084`) through the `host_save_*` calls. Only the windowed run has a save path (SDL's per-user
  directory, or `--cmos FILE`); `--check`, `run_tests`, `diffrun` and every driver read the game dir's
  `CMOS` and drop the writes, the captures' condition, and `make verify` fails first if that file is not
  2040 zero bytes or absent. The run clock `0x32970` reads the tick `0x105D88`, which the port's modelled
  ISR ticks advance (`game_isr_ticks`, `config.c`'s key wait), not wall time (record §S.9). Named gaps
  (record §S.8, its first item now a declared terminal difference on `0x2EA68`'s row, §G2): `0x2EA68` returns where the original exits; a short `cmos` gives the defaults where the
  original exits; the third (no recording of the original writing or loading a real save) is closed
  by `data/k11-captures/save-reload` (record §J3.10; `make save-reload-oracle`, in `make verify`):
  boot 1 (the one writable mount, the 11 keys ending in CONFIG OPTIONS' `right` tap and two Escs)
  leaves the OPTIONS MENU, whose `menu_run` return calls `config_file_save` (`0x2CBAF`); the port's
  replay of that boot with a scratch `PR_K11_SAVE` path writes a file byte-identical to the
  capture's `CMOS`, and the port booted from a copy of that `CMOS` holds, at the pinned frame
  (`f=0x1EB`) and every later dumped frame, the five storage ranges (the `config` image, the kind-0
  counters and the three high-score tables, §J3.3) boot 2's `poll.log` holds at the pin;
  `data/game/C` is proven untouched (the capture's own `CHECK staging`). Two more named
  gaps (record §S.10 and §S.8's PLAUSIBLE items, declared terminal by
  state-coverage §D7.3): the slot-13..35 bookkeeping low
  bytes are wrong at boot (the port's `config_storage_touch` stops the encode at
  the first store outside the image, so the low-byte record is written nowhere,
  where the original exits; the raw defect that makes the slot step past the
  image is §S.10), and the four PLAUSIBLE items are now declared — (a) the
  original cannot itself save a slot byte of 13+ (the exit chain is save-free:
  `0x2EA68` is `mov eax,1; jmp 0x62003`, and `0x6C4CF`'s closure reaches only
  runtime call targets, so no boot loads slot 13..35); (c) the two "nothing else
  stores it" absolute-operand scans (the byte `0x2D490`; `0x105DA4`/`0x105DA5`)
  are already implicitly covered by the rows' whole-memory comparison (the
  explicit watch is optional and not taken); (d) the statistics screens are
  already enforced (the save-reload boot-2 comparison's five ranges); (e) the 39
  `config_field_get` consumers are bounded and audited.
- DOSBox captures at 70.09 Hz while the game ticks at 60.05 Hz, which is why the
  title/front-end oracles model a capture frame as a byte-offset splice of two
  adjacent port frames rather than a 1:1 match. (60.05 Hz is the capture model's
  observed iteration rate; the bytes' AIL period and PIT divisor give 59.998 Hz,
  0.09 % apart — record `2026-10-23-endgame-truth-derivations.md` §E1.6.)
- **The differential rows are being extended to every ported function** (track D,
  record `docs/superpowers/plans/2026-10-11-reverse-d-derivations.md`). C3g closed
  the callee frontier at 322 rows; track D rows the ported functions the callee
  scan never reached. Track S (record `2026-10-13-reverse-s-derivations.md`) then ported the storage layer
  (14 functions and the body of `0x2D4EC`), the run clock `0x32970` and the ISR tick `0x2D62C`, with a row each;
  track I (record `2026-10-13-reverse-i-derivations.md`) made five inlined functions real ones with rows
  (`0x2F920`, `0x11BCC`, `0x12CD4`, `0x1D018`, and the master init `0x2F9CC`, which now makes the original's
  twelve calls); track N (record `2026-10-14-reverse-n-derivations.md`) re-decided the twelve named gaps from
  the bytes: three got a weaker row, nine stay gaps with the instruction that stops each; track H (record
  `2026-10-14-reverse-h-derivations.md`) gave the emulator a machine model (`E.Machine`: BIOS `int 0x16` from a
  per-case key queue, `in` answers, the VGA DAC through a device model fed by the port's `PR_OUT`, the aperture
  against `gfx_aperture()`) and ran the keyboard and VGA rows unpoked; track F (record
  `2026-10-14-reverse-f-derivations.md`) extended it with a per-case file model (`Machine.files`: the runtime's
  fopen/fread/fwrite/fclose answer from the case's in-memory files and the closed file's final bytes mirror into
  `mem[]` for the comparison) and ran both save-file rows unpoked. Track G batch g2 (record
  `2026-10-14-reverse-g-derivations.md` §G2) modelled the runtime's exit chain for rows, gave the load row the
  short-read case l6 that declares the terminal difference (its exit block is now exercised), and rowed
   `0x2EA68` — which closed `config_image_store` on its callee; batch g3 (record §G3) then rowed ALL SIX named
   non-rows (the §C3g.7 reversal: `dpmi_clock_read` `0x500BB`, `res_resolve` `0x1B544`, `0x29D60`, the interrupt
   lock `0x2EA30`, `0x32BAC`, `0x2EA64` — the six together closed 77 callee rows, 639→716; each new row's claim
   states exactly what it compares, the reversal's wording rule). Track G g4 and track J j1 pinned it at
   `1007/1007 functions VERIFIED; 14900/14900 mutants detected; 1 named gaps;
   774/832 rows with callees closed (175 have none); 768/832 closed on full rows; 8 ported functions with no
   row` (`0x24C5C` has ten rows); host input truth re-pinned it at **`1019/1019 functions VERIFIED;
   14956/14956 mutants detected; 0 named gaps; 774/833 rows with callees closed (186 have none);
   768/833 closed on full rows; 8 ported functions with no row`** — +12 functions and +56 mutants (26 the
   six keyboard readers — two of them the F readers, 12 the three game-port readers, 18 the two axis readers and
   the sampler), +1 row with callees and +11 callee-less, and the named gap `host_1b890` (its `in al,dx` at
   `0x1B899`) became a row once track H's `E.Machine.in` answered it (record
   `2026-10-21-host-input-truth-derivations.md` §H2.6, §H3, §H4.5). The 2026-10-22 pixel-and-proof
   closure plan (record `2026-10-22-pixel-and-proof-closure-derivations.md` §P5-§P10) re-pinned it at
   **`1031/1031 functions VERIFIED; 14983/14983 mutants detected; 0 named gaps; 813/844 rows with
   callees closed (187 have none); 806/844 closed on full rows; 8 ported functions with no row (45
   FN_ ported-unrowed: 8 portable [5 registry-visible + 3 header-shape exceptions] + 37 runtime; the
   scan-visible subset is 42 = 5 + 37)`** — 12 rows (+11 with callees, `runtime_5d812` a leaf) closed
   28 of the 59 open rows (59 → 31), for 833→844 rows with callees, 774→813 closed and 768→806 full,
   +12 functions and +27 mutants: the frontend stub pair `0x5D812`/`0x28D68`, the fighter callbacks
   `0x3E484`/`0x29D04`/`0x3E4C4`, the actor entrances `0x24568`/`0x40CB0`/`0x24804`, the menu, loop
   and memset entries `0x2CB74`/`0x4F5C8`/`0x61A70`, and the `+0x10` hook `0x3A6D4` (the spec's
   `0x3D0BC` is alignment padding, four zero bytes, not a function — record §P9.2). Their harness pins
   are the counter string, the EAX-mask, real-name and mutant-name lists and the stub-clobber
   dictionary (whose `0x3A6D4: ("ebx", "ecx", "edx")` entry replaces the padding's `0x3D0BC`); the
   hook's ported function opens with `PR_SEAM(0x3A6D4u, slot, side)`, inert outside `build/diffrun`
   (`run_tests` passes, record §P9.4).
   The 2026-10-24 proof-completion phase (record
   `docs/superpowers/plans/2026-10-24-proof-completion-derivations.md` §P5-§P8) re-pinned the
   counter at **`1033/1033 functions VERIFIED; 14993/14993 mutants detected; 0 named gaps; 825/845
   rows with callees closed (188 have none); 809/845 closed on full rows; 8 ported functions with no
   row (45 FN_ ported-unrowed: 8 portable [5 registry-visible + 3 header-shape exceptions] + 37
   runtime; the scan-visible subset is 42 = 5 + 37)`** — +2 functions and +10 mutants (the two
   ported card-step rows, §P5); the rows that close are Task 5's twelve (§P6, one full sprintf row and
   eleven weak scan-stop rows) minus `config_file_save`'s reopen (§P7.3). It declared the **20 open rows** terminal with a per-row
   reason (`TERMINAL_CENSUS`, §P8.1): 15 AIL runtime thunks, `sound_buffers_alloc` (`0x1D0BC`),
   `input_sampler` (the four host-owned PIT/game-port probes), and the three runtime exit-flush rows
   `config_file_load`, `config_file_save` and `menu_fatal_error` (all on the unrowed `0x6C4CF`
   flush; `config_file_save` reopened by the short-write case). The 20 rows are 19 distinct
   addresses — `attract_config_volumes_unscaled` and `attract_config_volumes` are two specs at
   `0x2C8F0`. `WEAKER` reads **13** (the proof-completion phase's 15 minus `0x51ED8` and `0x1BE30`,
   which claim-coverage Task 6 closed, record §C1.10) and `UNROWED` stays **8**: `diff-verify` prints
   0 named gaps, not 8. The phase's second
   proof is the **span-writer census** (§P2-§P4): 417 slots, 392 distinct targets (six renderers, one
   bare `ret`, 385 writer slots) plus the reachable out-of-table `0x81110[128] = 0x81310[0] =
   0x5D110`, for **386 distinct writer targets, 385 reachable** (384 reachable slots + `0x5D110`) and
   one dead (`0x57E3C`); the minimal fixture proves **26/26 code-shape classes** and the exhaustive
   fixture **385/386 distinct targets**, including both-edges clipping (`0x5D580`/`0x582F4`) and the
   clipped 14/15 word tails. E2 triages **388 `span-writer` rows** (the 384 writer-slot targets plus
   the four renderers `0x5215C`/`0x57F80`/`0x57FFB`/`0x58CBD`), the one writer slot without a row being
   the residual `0x5D004` (`0x81110[127]`), named in the supplement because it is `>= RUNTIME_BASE`
   (claim-coverage §C1.5.1); `0x24B54` is live, `0x24B08` dead.
   The two new clauses count the explicit frozensets `tools/diff_verify.py`
   keeps: `WEAKER` (13 — the rowed functions whose row is not full: a code poke, a reachable block left out,
   a declared terminal difference, or a declared byte difference; §P6.3/§P7.4) and `UNROWED` (8 — the ported functions with no differential row at
   all, none open; record §D.12, §S.8, §I.5, §N.4, §G2, §G3); the mutant moved by 1 in track G
   g4 (`game_loop`'s recorded RNG stub, `@rng`). The counter's unrowed clause no longer prints only
   `UNROWED`'s literal: `ported_unrowed()` intersects `symbols.h`'s 1203 `FN_` addresses with the
   registry (`NonPortable.ported`) and subtracts every `SPECS` entry, splitting at `RUNTIME_BASE`:
   the scan-visible frontier is 42 (5 portable + 37 runtime, record §P10.2-§P10.4), and with the
   three `UNROWED_HEADER_SHAPE` exceptions (`0x1B120`, `0x20C10`, `0x4FBA2` — implemented, but named
   only inside a comment that does not start with `/* 0xADDR`) the declared portable figure is
   `UNROWED`'s 8 and the declared total 45 (8 + 37). The guard test
   (`test_the_unrowed_figure_measures_the_frontier`, record §P10.6) fails on a new `FN_` port without
   a row, on a stale declaration, and on an exception that became scan-visible. The closure rule
   (`closed_rows`, record §J1): a row counts closed
   when every callee is VERIFIED by its own row, or is an **allow-mode** callee outside the portable set — runtime
   ≥ 0x5D000, host-owned/deferred in `tools/port_classification.txt`, or an address the decomp index
   (`port/decomp/prage.functions.csv`) never lists as a function (E2's triage field) — *and* `port/src` does not
   implement it (no `fn_register`, no column-0 `/* 0xADDR` header; the port-code guard applies to all three
   classes) — while a recorded *stub* or `real` callee outside
   the set, and any ported function with no row, still keep the row open. The port-code check was
   load-bearing in track J: `0x29D04` and `0x3E484` (the E2-class handler entries, implemented and
   `fn_register`ed by `port/src/game/actors.c`, then unrowed) kept `fighter_19020`,
   `fighter_35050`, `fighter_think_side`, `fighter_pass_a` and `fighter_pass_a_al` open — without it
   the rule would have closed them on a row that can exist (716→774, +58). The 2026-10-22 plan gave
   all four E2-class entries rows (`0x28D68`, `0x3E484`, `0x29D04`, `0x3E4C4`), so no open row
   blocks on a port-code implementation now. The
   counter does not tell a full row from a weaker one: **13** of the portable rowed functions have a row
   that runs the original under code pokes, leaves reachable blocks out, declares a terminal
   difference, or declares a byte difference on some or all of its cases (which then compare nothing
   on the port's side, §G1's skip): the eleven the 2026-10-22 plan named that remain (`0x2CADC`, `game_loop`,
   `0x1AFE8`, `0x1B084`, `0x1C740`, `0x24C5C`, `0x2EA78`, `0x2EA74`, `0x2EA68`,
   `0x1B544`, `0x2EA30`), plus the two the phase added — `0x249F0` (its `0x24A9C` restart arm
   unexercised after the longjmp became scan-only, §P6.3) and `0x1D0BC` (its `0x1D10E`/`0x1D183`
   buffer-failure arms unhit, §P7.4). `0x1BE30` (of the twelve) and `0x51ED8` (the aperture row's
   declared renderer node-scratch `+0x14`/`+0x38` byte difference, §P7.4; the counter had counted it
   full) left `WEAKER` when claim-coverage Task 6 closed them (record §C1.10). The one-line reason for
   each of the 13 is the `WEAKER_CENSUS` table in `tools/tests/test_diff_verify.py` (the record's
   §P8.3 table predates the two closures). A closed row may rest on a weaker callee row
   (`attract_step` on `0x1C740`'s, `config_image_store` on `0x2EA68`'s).
   The still-open rows are measured, not asserted: after the closure rule **20 of the 845 rows stay
   open**, and every one is a declared terminal with a per-row reason (`TERMINAL_CENSUS`, record
   §P8.1) — 15 AIL runtime thunks, `sound_buffers_alloc` (`0x1D0BC`, blocked by `0x1C308`),
   `input_sampler` (the four host-owned PIT/game-port probes `0x4FBBB`/`0x4FC05`/`0x4FCFD`/
   `0x4FDF5`), and the three runtime exit-flush rows `config_file_load`, `config_file_save` and
   `menu_fatal_error` (all blocked by the unrowed `0x6C4CF`; `config_file_save` reopened by the
   short-write case §P7.3). The 20 rows are 19 distinct addresses
   (`attract_config_volumes_unscaled` and `attract_config_volumes` are two specs at `0x2C8F0`). No
   row blocks on a ported function in the portable set by accident: the 8
   `UNROWED` functions are callees of none (the runtime-region `0x65431`/`0x5DC0F` family is stubbed).
   §G3's 116-row census was the pre-rule reading (96 ≥0x5D000, 17 E2-class, none
   on the six); the rule closed 58 of the 116, host input truth's sampler row made 59 open (record §H4.5),
   the 2026-10-22 plan's 12 rows closed 28 of the 59, leaving 31, and the proof-completion phase closed
   eleven more (Task 5's twelve minus `config_file_save`'s reopen), leaving the 20 terminals above.
   A change that ports a function or touches a
  row's callees must keep that counter and every pin intact, or re-measure and
  record why they moved. The harness runs its rows on a process pool
  (`PR_DIFF_JOBS`, default the CPU count; 1 is the sequential loop), one job per function: `diffrun`
  loads the EXE once and forks a child for the function and for each of its mutants, and the original is
  emulated once and shared. A full `--self-check` is about half a minute and the differential step of `make verify` about three
  (it was about 33); the whole gate is about 26 minutes, most of it the oracles.
  `diffrun` finds a case's writes outside the image from the pages a write fault marked (they are
  read-only until written), not from a scan of the 63 MB; `PR_DIFF_FULLSCAN=1 make verify` runs the
  full scans instead (several times slower), the form to use before a push. Both printed the same bytes
  on every row and mutant when the tracking went in (990 functions, 15,715 bindings), and
  `RunnerScanTests` compares them on each gate. A binding that makes a system call write straight into
  a page of `mem[]` outside the image that was never written (a `read()` into it) would get `EFAULT`
  under the tracking: none does. `--self-check` does not see every pin: a stub's
  declared clobbers must agree across all specs and with the pinned dictionary, a
  read outside the image must be named, and a seam added to a helper that an
  existing mutant binding calls directly (`effects_list_unlink`, d4b) changes what
  catches that mutant. Only the harness suite (`make diff-verify`'s unittest) checks
  those; a batch that did not run it has not checked them (record §D.7, §D.8).
- **Every binding and every mutant must stay inside `mem[]`.** `DSB/DSW/DSD` have
  no bounds check, and a read outside the 64 MB array is host-dependent (an
  intermittent SIGBUS on macOS 27): an index-like sentinel (side, slot,
  character, a pointer field the function follows) keeps `base + index * size`
  in range, the "other side" `1 - side` included. A row whose original
  deliberately reads above the image names the read in `P45_OUTSIDE`
  (`tools/tests/test_diff_verify.py`).

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
  addresses) that have a column-0 `/* 0xADDR` header, an `fn_register` call, or
  a declared `PORT_PROGRESS_EXCEPTIONS` entry in `port/src` (the strict rule
  `tools/diff_verify.py`'s `NonPortable.ported` shares; the loose "anywhere in a
  comment" form reads prose mentions as implementations and is not used).
  Update the README title and its "N% of the original's D real functions"
  line after any merge that adds or removes a ported function.
  It also prints the adjusted (portable) figure and the class breakdown; the
  README title keeps the raw
  percentage. As of 2026-10-28 (claim coverage Task 4) the lines read `798 1203 66`,
  `759 759 100` and `ported 798 (759 portable + 39 runtime) host-owned 41
  deferred 12 replaced 37 runtime 315` (the classification-truth phase removed
  the `0x62003` false positive and declared the `replaced` class; record §C4):
  - **Raw:** 798 ported of 1203 real functions (66%). The 405 unported functions
    are not porting targets: 53 are classified host-owned or deferred — 41
    host-owned + 12 deferred (the rows of `tools/port_classification.txt` that
    are not ported, one evidence line each) — 37 are the `replaced` clean-room
    runtime replacement layer (the boot/movie/attract spine's runtime closure,
    every address >= 0x5D000, one evidence line each), and 315 are runtime code
    (>= 0x5D000: WATCOM libc, DOS/4GW glue, the AIL library) that the port
    serves from the host libc / clean-room decoder. The AIL runtime set within
    the 315 is named unreproducible (no call-graph rule yields the recon's
    56/10,824 B reading; record §C4).
  - **Portable:** 759 of 759 (100%). The denominator leaves out the 53
    host-owned/deferred functions (41 + 12), the 37 `replaced` addresses and
    everything else at or above 0x5D000; the
    other 39 ported functions (798 - 759) sit in that runtime region, so they
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
