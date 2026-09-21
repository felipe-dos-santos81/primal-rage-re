# Spec: front-end chain (states 3, 4, 5) and the effect render path

**Status:** approved design, pending implementation plan
**Cycle:** front-end chain
**Branch base:** `main` at the time of writing
**Predecessor cycles:** 4b-A (EEPROM/config core), 4b-B (front-end input, credits and the
select state), 4d (attract), the OPL/MIDI-controller audio cycle.

## 1. Context

The port's state machine (`port/src/game/flow.c` `game_state_step`, the raw's `0x11D04`)
dispatches states 0 (attract), 1 (title) and 2 (the six-entry select carousel) for real.
States 3, 4 and 5 are now ported too (Tasks 3/4/6), so the carousel advances into the
match-up chain; states 6, 7, 8 and 9's semantics beyond the countdown handoff remain
no-op stubs and are the next cycle.

Two declared gaps were thought to block these states from working; both were
resolved or re-scoped during implementation:

* The `0x13xxx` effect **render** path was called unported. Task 7 found no missing
  draw exists — the palette path is the existing spawn → step → dirty list →
  `gfx_flush_palette` → `gfx_dac` chain.
* The call sites (`0x29B74`, `0x41578`) were called table-registering. Task 2's
  derivation refuted that; Task 8 found them unreachable from the ported path and
  deferred them (see §5).

## 2. Goal

Port states 3, 4 and 5 and the effect render path so the port advances
carousel → 3 → 4 → 5 → 9 → 6 unaided, with the effects these states spawn actually
appearing, and with the front-end region verified against a capture of the original.

The fight engine (states 6/7/8 — `0x11A8C`, `0x263F4`, `0x33F08`) is **out of scope**;
the cycle ends where state 6 begins and that stub stays declared.

## 3. Spike evidence (why the verification shape changed)

The starting assumption was that reaching states 3/4 in the original would require
scripted input. A bounded spike disproved that:

* **No input is needed.** The title state hands to state 2 by itself
  (`0x121A0`, decompiler `:1488`: `if (DAT_000f0a6f == 2 && DAT_0009af3d == 0)
  DAT_000f0a64 = 2`), and the select state always exits to state 3.
* **A longer passive capture already reaches the front-end.** Re-running the existing
  pinned capture at `--time-limit 120` instead of the Makefile's 45 produced **3708
  distinct post-logo frames** versus 587. The title window is only 111 of those, so
  roughly 3100 frames of content beyond the title are now in hand.
* **The run is not frame-timing deterministic.** `--verify-reproducible` on two
  identical 120 s runs: 659 shared distinct frames of ~3686, first divergence at
  distinct index 30 (raw 1586, during the boot sub-machine + title), longest common run
  80 frames. This is the same phenomenon the title oracle already absorbs as *splice*
  frames; the title oracle reports 54 clean / 55 splice / **0 unexplained**.

The residual risk is therefore **content** determinism, not input reachability: the title
needed four RNG/behaviour pins before it was stable, and the front-end will likely need
its own. That risk is front-loaded by sequencing (section 8).

## 4. Scope

**In:**

* State 3 — `0x12484`.
* State 4 — `0x11578`.
* State 5 — the match-start block inline in `0x11D04` case 5.
* The effect **render** path — the camera/scene layer (`0x1317C`, `0x1324C`, `0x13290`,
  `0x1333C`) and their `DS_000F0AEC`/`DS_000F0AF0`/`DS_000F0AF4` state. **There is no
  missing draw to port:** Task 2's derivation, re-checked against the bytes, found that
  none of the four functions draws anything — they are fight-camera state updaters whose
  state the existing render pass (`0x14328`) and the actor-pset sync already consume. The
  effect palette path is already complete (spawn → `effects_step` → palette dirty list →
  `gfx_flush_palette` → `gfx_dac` → `gfx_present`). `0x1324C` is update-table entry 0 and
  is **dormant**: no shipped store sets `DS_00104AE8` bit 0.
* The effect call sites `0x29B74` and `0x41578` — investigated and **deferred** as
  unreachable from the ported path (see §5).
* The capture/pin/oracle extension that verifies the above.

**Out:**

* The fight engine — state 6 (`0x11A8C`), state 7 (`0x263F4`, `0x33F08`), state 8
  (`0x32970`, `0x257A4`), and state 9's semantics beyond the countdown handoff it
  already performs.
* The `0x13EF0`/`0x13F68` string helpers (an "ASSHOLE"-table name matcher), which are
  not part of the effect path.
* Any restructuring of the existing state handlers, effects chain or process tables.

## 5. Architecture

Three layers, each extended where it already lives. No new module boundaries.

* **State handlers** — `port/src/game/flow.c`, beside `game_state_title` and
  `game_state_select`. `game_state_step`'s cases 3/4/5 replace their current
  `/* PORT: later states */` stubs. Each state stays a phase machine driven by its own
  `mem[]` counter — `DS_000F0A6F` for state 3, `DS_0009AD98` for state 4 — transcribed
  phase-for-phase from the raw. No restructuring, no invented abstractions.
* **Effect render** — `port/src/game/effects.{c,h}`, beside the existing producers and
  `effects_step`. The camera/scene functions maintain the camera state the render pass
  consumes; they draw nothing and touch no palette. The palette path itself is the
  existing spawn → step → palette dirty list → `gfx_flush_palette` → `gfx_dac` chain,
  which the master loop already runs before `gfx_present`. `0x1324C` is registered so the
  existing update-table dispatch (`run_process_table`) reaches it the way the raw does;
  no call site is added and no draw routine exists.
* **Effect call sites — deferred, not wired.** The plan said `0x29B74` and `0x41578`
  register into the port's existing `run_process_table` update/render tables. Task 2's
  derivation, re-checked against the bytes in Task 8, refuted that: `0x29B74` is the
  mode-`0x17` handler stored at `DS_00104AE4` and dispatched by six
  `call dword [0x104AE4]` sites, and `0x41578` is direct-called from four sites. Neither
  appears in either process table, and every one of those sites is reached only through
  `0x24C5C`'s unported mode cases (`0x12`, `0x16..0x1B`) and the unported match/fight
  chain. The port's `DS_00104B00` is fixed at 3 by `0x10E80`, so none is reachable from
  the ported states 3/4/5. Per the reachability ruling they are **deferred and unowned by
  this plan** — no dispatch path is shipped (see `port/spec/game_flow.md`).

**Tools.** The repo's standing rule is that existing `tools/` files are read-only. This
cycle carves out `tools/title_pin.py` and `tools/title_compare.py` by explicit approval:
a front-end oracle is the same alignment/splice machinery with a different window, and
duplicating it into parallel files would be worse. The shared modules (`smk_capture.py`,
`title_capture.py`'s staging and AVI decoding) are used as-is and not renamed.

## 6. Interfaces and new derivations

Real functions already ported and reusable: `0x2C3FC`, `0x4F1E4`, `0x2BAF4`, `0x2AE14`,
`0x33904`, `0x13C70`, `0x1C500`, `0x2F198`, `0x2C06C`, `0x1EA08`, `0x32970`,
`0x10DB0`/`0x10E18`/`0x2BF08` (the tails).

Entirely uncharacterized, and derived in this cycle before any porting:

* `0x2F4BC` — called 8, 12 and 13 times in state 4's phases 0/1/2.
* `0x12658` — called once at the end of state 3 phase 0.
* `0x1EA08` — called by state 5.
* `0x29B74` / `0x41578` — the effect call sites (they walk the front-end list with
  `0x33904` and call `0x13D4C` for records matching `0x3E688` or `0x88874B0`).
* The camera/scene functions `0x1317C`, `0x1324C`, `0x13290`, `0x1333C`.

Every derived value is cited to a disassembly address or a command with real output; no
value is transcribed from the decompiler without checking the bytes.

## 7. Data flow

* **State 3** — phase 0 runs `0x2BAF4`, four `0x38B18` calls, then walks the front-end
  list (`0x33904`) spawning a type-3 effect (`0x13C70`) for each record whose first dword
  is `0x3E688`; then either the `0x1C500`/`0x2F198` pair twice or a single `0x2AE14`
  spawn into `DS_000F0A40`; then `0x12658`. Later phases do timing arithmetic on
  `DS_00107A38`/`DS_00107A44` and hand to state 9.
* **State 4** — phases 0/1/2 each perform match setup (`0x2C3FC`, `0x4F1E4`, `0x2BAF4`,
  `0x2C06C`, `0x2AE14`, `N× 0x2F4BC` — 8, 12, 13), set the 180-frame timer
  `DS_000F0A76 = 0xB4` and the continuation phase in `DS_000F0A74`, then enter delay
  phase 4. Phase 4 counts the timer down and jumps to the continuation phase. Phase 3
  hands to state 9 with `DS_000F0A6C = 0`.
* **State 5** (inline in `0x11D04`) — `0x2C3FC`, `0x1EA08`, `0x2C06C`, `0x32970`, then
  `DS_000F0A72 = 0`, `DS_000F0A6C = 6`, `DS_000F0A64 = 9` and the three tails: match
  start, then the state-9 wait, then state 6.
* **Effects** — spawn → record → `effects_step` → palette dirty list → `gfx_flush_palette`
  → `gfx_dac` → `gfx_present`; the camera/scene layer only maintains camera state
  (`DS_000F0AEC`/`DS_000F0AF0`/`DS_000F0AF4`) that the existing render pass consumes.

## 8. Sequencing

1. **Capture + pin + oracle (de-risk, bounded).** Extend `title_pin` with the front-end
   behaviour pins, add a Makefile capture target that reuses `title_capture.py` unchanged
   (it already takes `--time-limit` and `--out`, so a longer front-end capture needs no
   tool edit), emit port **frames** for the front-end (not just the current `select.log`
   hashes), find the window by content-alignment, and wire a `frontend-oracle` target.
   This phase comes before any state code.
2. **Derivation record** for `0x2F4BC`, `0x12658`, `0x1EA08`, `0x29B74`, `0x41578` and
   the camera/scene functions.
3. **State 3**, then **state 4**, then **state 5**, each gated on the oracle advancing.
4. **Effect render path** (the camera/scene state updates; no draw exists). The effect
   call sites are investigated and deferred as unreachable (see §5).
5. **Docs** — `port/spec/game_flow.md` dispositions, README and the cycle report.

## 9. Verification

* **Primary:** a front-end pixel oracle against the extended capture, using the existing
  content-alignment plus clean/splice/unexplained classification, requiring **0
  unexplained** over the states 3/4 window.
* **Gated on the pin converging.** If the front-end cannot be made content-stable within
  phase 1, fall back to the `PR_FRONTEND_DUMP` determinism log extended through states
  3/4 (two runs byte-identical) and record the missing pixel oracle as a declared gap —
  the same disposition the select state carries today. The fallback is decided in phase
  1 and recorded, never discovered silently later.
* **Unit tests** for every newly derived helper and for the camera/scene functions.
* **Non-regression:** the title, attract and smacker oracles and the
  `oracle C-vs-Python` gate must be unchanged; `make verify` green with 0 warnings and
  `port/src/symbols.h` byte-identical.

## 10. Risks

* **Content non-determinism in the front-end.** The main risk; front-loaded by
  sequencing, with a free fallback.
* **Undocumented helpers.** `0x2F4BC` and `0x12658` are new ground; if either turns out
  to depend on state the port does not model, it is recorded as a named gap rather than
  guessed at.
* **Splice tolerance.** The title oracle's 54 clean / 55 splice split is the existing
  bar, not a strong pixel-exactness claim; the front-end oracle inherits that bar and the
  spec should not overclaim what it proves.

## 11. Open questions

* Which RNG draws the front-end consumes, and therefore how many pins `title_pin` needs
  (resolved in phase 1).
* Whether the front-end window is contiguous in the capture or interrupted by the
  attract loop cycling (resolved by content-alignment in phase 1).
