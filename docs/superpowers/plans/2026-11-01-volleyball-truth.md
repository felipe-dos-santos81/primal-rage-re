# Plan: Volleyball truth — the mode-`0x21` route search and exhibit

**Design:** `docs/superpowers/specs/2026-11-01-volleyball-truth-design.md`.
**Record:** `docs/superpowers/plans/2026-11-01-volleyball-truth-derivations.md`.
**Register:** R05 (`docs/superpowers/specs/2026-10-30-final-claims.md`).
**Base:** `edbc35de`.
**Status:** implemented and gated (2026-11-01); the measured results are in the
derivation record §2–§3.

## Global constraints

- No fitted constants. Every stage, lie, count and flight value comes from the
  raw bytes or a measured run, with the address or frame in the record.
- No poke may write `DS_00104B1F`/`DS_00104ABC` (R30). The search uses real
  inputs only; a port dump is not a capture and publishes nothing.
- `port/src` comments: only `/* PORT: ... */` and `/* TODO(verify): ... */`.
- Pick up a capture only after the port-side route is found; one guarded run,
  never a typed-into DOSBox-X window, and a failed run is `.failed` evidence.
- The `V`/`Vg` observer is inert without `PR_GP_VOLLEY`; the search tool is
  not in `make verify`. The one deliberate in-gate use is
  `gp-u30-volleyball-oracle`, which sets `PR_GP_VOLLEY=1` on its own replay so
  the checker reads the port's `+0x1F` count and `T`/`Vg` entry/return stores;
  the default output paths and the preexisting T values are unchanged, and the
  existing oracles and pins must not move.
- Commit style `<area>: <what changed>`; stage named files; only commit when
  asked.

## Task 1 — Correct the mode-`0x21` spec and add route B

**Files:** `port/spec/volleyball.md` only (plus the phase record §1).
**Requirements:** apply the preflight's corrections with the exact addresses:

1. `+0x10` is the **shadow actor** (spawned at `0x4B4B6..0x4B4D7`), not a held
   actor; the held link is `actor+0x4A` + `+0x1C` bit 6.
2. `+0x34`/`+0x36` in §4/§5 are the **actor's** vx/vy integer words (the entry
   is 0x24 bytes; the actor is `entry+0x08`), not entry fields.
3. `0x4B5A8` is the crowd **walk-in spawner** (type 3, target
   `fighter ± 0x1800`, `+0x34 = ±0x40`); the lie is the case-3 landing
   `0x49E47`/`0x49E52` (type 4, `+0x1C |= 0x80`, `rng(0x3C)+0x3C` timer). The
   gate is `DS_001088B6[side] != 0` (the `0x3907B` combo budget, `2+` hits) or
   `slot+0x52==7 && slot+0x53==2` (the dizzy state `0x36BC8`).
4. Add `0x4B788`'s grab gate: the counted trample needs it to return 1
   (fighter `+0x5F != 0xC97F2[ch] = 0x2D`, other `+0x54 != 3`,
   `DS_00105B3A <= 1`); its return-0 arm is the grab and does not count.
5. Correct the `+0x1F` zero list: `0x49622`, `0x4D061`, `0x49FAF`, `0x4C1E8`,
   `0x4C27F`, `0x4C349`, `0x4C904`, `0x4CAA9`; `0x4E4E5` (not `0x4E4E8`) is the
   ending audience's `+0x1F`, `0x4E4E8` is `+0x21`.
6. Add the route-B section: the mid-round join sets `m=2` (`0x28E6D`) and
   `flow_stage_pick` returns (`0x2585D/0x2585F`), so the 1P stage survives; the
   1P pick is `0xA87C4[c] + rng(6) + 1` reduced by 7 when `>= 7`, base
   `{0,4,2,3,6,1,5}`; character 0 cannot give 0 and characters 1..6 can. Keep
   route A as the fallback.
7. Mark the derived/measured boundary: the practical hit window is `T+65..T+71`
   from the 56 px box; the raw latch opens at `T+37`; the "two fighters
   ~142 px apart" is the 2 px/frame × 71 flight.
8. Correct §10's measured boundary: `gp-idle-loss` is 8,173 frames; the two
   tramples are at trace `f=0x8BE` (hex; the record's 2,238 is decimal), entries
   `0x108414`/`0x108438`, type 4/`+0x1C=0x80`, first hits; the demo's stage 0 is
   the boot default (`0x20C10`), not a pick.

**Acceptance:** the spec's tables match the raw/decomp citations above; no
claim changes; `python3 -m unittest tools.tests.test_final_claims` passes.
No code, no capture.

## Task 2 — Search instrumentation

**Files:** `port/src/host.c`, `host.h`, `port/tests/test_game.c`,
`tools/vb_search.py` (new), `Makefile` (a `vb-search` help line if useful).
**Requirements:**

1. A no-pace seam: `host_set_vblank_fast(int)` (host.c/h) makes
   `host_wait_vblank` skip the `SDL_Delay` but still `host_pump()`. The GP
   driver sets it when `PR_GP_FAST=1`. The default is unchanged.
2. A volley observer: when `PR_GP_VOLLEY=1`, the GP driver emits, before each
   `T` line, a `V` line per active effects entry walking `DS_0010884C`
   (`next@+0`), with `e`, type `+0x1E`, flags `+0x1C`, count `+0x1F`, and the
   actor's (`entry+0x08`) x `+0x18`, height `+0x1C`, depth `+0x30`,
   vx `+0x34`, vy `+0x36`; plus a `Vg` line with
   `DS_001088B6[0..1]`, `DS_00108864`, `DS_00104ABC`, `DS_0010889C/9D`,
   `DS_001088AA`, `DS_001088C1/C5`. Inert with the env unset.
3. `tools/vb_search.py`: reads a base port-script v2, applies a parameter grid
   (inserted keys, walk/press frames), runs `./build/run_tests` with
   `PR_GP_DUMP`, `PR_GP_SCRIPT`, `PR_GP_FAST=1`, `PR_GP_VOLLEY=1` in parallel
   over per-variant `/tmp` dirs, and ranks the traces by stage `afc`, `b1f`,
   the lie frame (`+0x1C` bit 7), max `+0x1F`, and `mode == 0x21`; it prints a
   ranked table and the winning script path.
**Acceptance:** replaying `gp-idle-loss` with `PR_GP_VOLLEY=1` prints the two
entries `0x108414`/`0x108438` at `f=0x8BE` with `+0x1F=1`; `PR_GP_FAST=1`
cuts the replay wall time to CPU-bound; `make gp-oracle` and the unit suite
still pass; the default paths are byte-identical.

## Task 3 — Find the route

**Files:** `tools/vb_search.py` invocations; the winning script under
`.superpowers/sdd/2026-11-01-volleyball-truth/`; the phase record.
**Requirements:**

1. Base: the `gp-u15-join` port script (boot → START MENU → LEFT PLAYER
   ARCADE → 1P fight → P2 join). Insert the 1P character-select cursor and
   confirm steps; try the seven cursor positions and a small select-timing
   grid until `DS_00104AFC == 0` at the vs/fight.
2. At the 1P fight, press P2 start; complete the join's select/vs; verify the
   stage survives (`afc == 0`) and `b1f == 3`, `DS_00104ABC == 2`.
3. Land a 2-hit P1 combo on P2; read the `V` line for a side-0 entry with
   `+0x1E==4`, `+0x1C` bit 7, at the landing; if the combo does not lie it,
   try the dizzy path and the other side.
4. Position the fighters: the first trample (the `+0x1F` 0→1), then the
   passive rally; tune the second fighter's distance and the first hit's
   direction until `+0x1F` reaches 8 and `mode == 0x21` (`0x4BD98`).
5. Record the winning script, the frame of each increment, the entry, the
   stage/join frames, and the best state if no route is found. Timebox the
   grid; the record keeps the measured bound.
**Acceptance:** a deterministic port script reaches `+0x1F == 8` and mode
`0x21`, with the court/score/return visible in the replay; or the record
states the measured best state and the exhausted grid.

## Task 4 — Capture and publish (conditional on Task 3)

**Files:** `tools/gp_session.py`, `tools/gp_capture.py` (scenario fields),
`tools/gp_volley.py` (new checker), `tools/tests/test_gp_volley.py`,
`Makefile`, `tools/capture_manifest.json`, `port/tests/test_platform.c` (miss
set), `tools/audio_compare.py` only if the capture joins the audio inventory
(it should not).
**Requirements:** clone `gp-bowling-2`/`gp-twop`; add the scenario with the
found script; add the `V`-line observables to `SNAP_FIELDS` and
`gp_trace_line`; one guarded capture with `stop_mode 0x21`; the checker asserts
the entry, `b1f=3`, the stage, `+0x1F` 8, the mode, a point and the return;
the oracle pins (frame, trace/moves, start, reach, missing, sha, frames), the
miss set, the manifest/`CAPTURE_DIRS` entry and the pin baseline re-stamp. A
failed run is `.failed`, publishes nothing, and Task 3's record says so.
**Acceptance:** the capture passes every session check; the checker and oracle
pass; every pin fails its one-step mutation; the manifest and gate-check agree.

## Task 5 — Close the phase

**Files:** the register, `AGENTS.md`, `docs/PROGRESS.md`, `port/RE_GUIDE.md`,
the phase record, `tools/tests/test_final_claims.py` if a token moves.
**Requirements:** R05 becomes `closed` with the capture/oracle evidence if Task
4 published one; otherwise it stays `declared` with the measured search bound
and the record. Re-measure the counters that move (coverage census if the
capture hits the mode functions; pin baseline; capture inventory). Run
`make verify` (and `make audio-render` if a capture/oracle was added).
**Acceptance:** the docs tests pass; `make verify` exits 0; the register has no
open row; the README title percentage is unchanged unless a ported function
was added.
