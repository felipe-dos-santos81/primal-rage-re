# Demo fight — attract states 6/7 (design)

**Status: cycle 1 DONE_WITH_CONCERNS; cycle 2 not started.** Cycle 1's declared
success criterion — the demo window clean from its start up to the first landing
hit — was **not met**. The measured bound differs from this design's declared one
and is corrected in place below: the first unexplained demo-window frame is the
**state-9 hold's globe render** (capture 811, closest port frame 264, 205
differing bytes in rows 98..144), **not** the first landing hit, so the window
diverges at its first frame (0 clean). The two state-6 character picks are pinned;
the state-9 hold itself has no pinnable site. Cycle 1's real outcome is "the demo
moves and its divergence is measured and named", not "the window is clean to a
hit". See `port/spec/game_flow.md`'s demo-fight section and
`.superpowers/sdd/2026-09-20-demo-fight-motion/task-9-report.md`.

## Context

The port has the engine core, the OPL/audio stack, the sprite compositor, the
actor system, the title (state 1), the boot logos and attract machine (state 0,
`0x11000`), the front-end input and select carousel (state 2), the config/EEPROM
core, the effects producers, the MIDI controllers, and — from the last cycle —
the front-end chain (states 3, 4, 5) with an enforced pixel oracle.

`port/spec/game_flow.md` and `README.md` both frame the remaining work as "the
fight engine (states 6/7/8)". This cycle starts by correcting that framing,
because the reconnaissance that opened this design found the docs wrong in a way
that changes the scope.

## The correction this cycle starts from

**States 6/7/8 are not the interactive player match.** They are the attract-mode
CPU-vs-CPU demo fight and the continue sequence that follows it.

- State 6 (`0x11A8C`, 317 B, 1 caller) calls the RNG (`0x5D7DC`) and feeds the
  results to `0x41350(player, char)` for **both** players — it picks two *random*
  characters, ignoring the carousel — then sets `DS_00104B15 = 1`, spawns the HUD
  bars (`0x1D890`), sets `DS_001082CC = 3`, runs the `(DS_00104528+1) & 2` branch,
  and ends at `DS_000F0A64 = 7`, `DS_000F0A6A = 900`, `DS_000F0A6C = DS_000F0A72`,
  `DS_000F0A6F = 0`.
- State 7 (`0x263F4`, 329 B) runs the shared arena frame and reads **no input**.
  State 8 is the run clock (`0x32970`) plus the `0x257A4` coin divert.
- On timeout `0x11BCC` (41 B) restores `DS_000F0A64 = DS_000F0A6C`, whose value is
  in {0, 4, 5} — the attract/title/menu loop.

**The interactive match is a different machine: `DS_00104B00` (a screen *mode*),
not `DS_000F0A64` (the state).** The entry is coin → `0x257A4` → mode `0x10`
(`0x438B4`, player entry/select) → mode `0x13` (`0x424E8`, VS + spawn) → mode
`0x1E` (`0x1EEB0`, the match/round flow), with the round screens in modes `5`–`9`,
`0xB`, `0xC`, `0x14`, `0x15`, `0x21`–`0x25`, `0x30`–`0x33`.

Two supporting facts, both verified during this design:

- **No state `>= 10` is ever assigned.** Every assignment to `DS_000F0A64` in the
  image is 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, or `DS_000F0A6C`. So the
  `if (DS_000F0A64 < 10)` guard is always true and the `>= 10 → 0x11000` arm is
  unreachable; `0x11000` is reached only by state 0 falling through the switch.
  The port already encodes exactly this.
- **The demo never ends a round.** The round-end check `0x27FA8` (392 B) has
  exactly **two** callers — `0x26254` (mode 4/0xB) and `0x26540` (mode 0x21) —
  and `0x27FA8` is not in either's transitive callers from the state-7 path.
  `0x263F4` calls neither, and `0x49C78` is not a caller. So during the demo
  health can fall but no KO transition fires; the demo runs out state 6's
  900-frame timer and exits through `0x11BCC`.

## Scope

**In scope — states 6 and 7, the attract demo fight.** This is the scope of the
whole demo-fight design; the next section splits it into two cycles, and cycle 1
delivers everything here except the combat resolution.

- State 6 (`0x11A8C`): the demo setup, including the two RNG character picks, the
  HUD spawn, the `DS_00104528` branch, and the four stores.
- State 7: `0x263F4` (the arena frame) plus `0x33F08` (health bars), then the
  three shared tails.
- `0x24C5C`'s `DS_00104B15 != 0` tail, which state 6 arms.
- The pipeline those require: fight camera and projection, the arena frame
  `0x263F4`, fighter render and think/AI, collision and damage, HUD/health, and
  the state-6 timeout path through `0x11BCC`.

**Out of scope, each recorded as unowned by this cycle.**

- State 8 and the `0x257A4` coin divert, and the whole `DS_00104B00` mode graph:
  modes `0x10`, `0x13`, `0x1E`, and the round screens `5`–`9`, `0xB`, `0xC`,
  `0x14`, `0x15`, `0x21`–`0x25`, `0x30`–`0x33` — including their arena-frame
  variants `0x26254`, `0x26540` and `0x28788`, which are mode handlers the demo
  path never reaches.
- `0x1EEB0` (the match/round flow, 1448 B), `0x1F458` (the input-grid menu,
  2897 B), and `0x1EA08`'s remaining four call sites (`0x11A42` in `0x11A30`;
  `0x1F140`/`0x1F278`/`0x1F39B` in `0x1EEB0`).
- The player entry/select/VS/continue screens, the human input mapping, and the
  two-player path.
- `0x27FA8` and the KO/win screens, because the demo path does not reach them
  (see above).

## Cycle split

The demo fight is large enough to be two cycles, cut between **motion** and
**combat**. The cut point is forced by the oracle: it cannot converge until the
fighters both move and land hits, and a partial port can only satisfy the first
half.

**Cycle 1 — motion and render (the immediate cycle).** The derivation, the fight
camera and projection, the arena and fighter render, the think/AI chain, the
state 6/7 handlers, the 900-frame timer and the `0x11BCC` exit, and the
`game_frame` tail minus its combat calls. The demo visibly moves and animates.
Its oracle is **report-only**, proving the window clean up to its measured
divergence point. Cycle 1's declared point was the first landing hit; the
measurement found it is earlier — the state-9 hold's globe render — so the bound
the cycle records is that measured frame, not the assumed hit. This is the
front-end chain's Task 1 → Task 5 shape: a provisional gap, opened honestly.

**Cycle 2 — combat and closure.** Collision, damage, the health bars, and the
oracle closure: the pins settle, the window converges, `frontend`-style
enforcement moves the demo window into the ladder as a real gate.

Cycle 2's design decisions are already made here (module layout, determinism
strategy, oracle machinery), so cycle 2 needs no new design pass — only a plan
drawn from the stages below.

## Architecture

Three new modules, with the state cases wired in `flow.c` beside the existing
`game_state_3`/`game_state_4` handlers.

| Module | Owns | Why separate |
|---|---|---|
| `port/src/game/camera.c` | `0x12D48` dispatcher, `0x12CD4`, `0x1317C`, `0x12DA8`, `0x1282C`, and the projection `0x17FA0`/`0x17580`/`0x16D58` | A self-contained state machine with its own globals. **This retires the last cycle's camera-chain deferral**: `0x12CD4`/`0x1317C`/`0x13290`/`0x1333C` and the `0x12D48` dispatcher were deferred as unreachable, and the demo fight is the caller that makes them reachable. |
| `port/src/game/fighter.c` | think/AI `0x1975C`, `0x3B464`, `0x3B298`, `0x3B134`, `0x3BDDC`, `0x18C14`, `0x1A978`; combat `0x3BB90`, `0x3BAEC`, `0x4FB20`, `0x34038`, `0x186D0`, `0x2A690` | The simulation layer: per-fighter behaviour and hit resolution. |
| `port/src/game/fight.c` | the arena frame `0x263F4`; the HUD/health path `0x35658`, `0x1D890`, `0x33F08`; and `0x11A8C`'s body | The orchestration layer that sequences a round. The sibling arena frames `0x26254` (mode 4/0xB), `0x26540` (mode 0x21) and `0x28788` (mode 9) are **mode handlers and stay out of scope** with the mode graph; the demo path reaches only `0x263F4`. |

**Integration.** `game_frame` (the port's `0x24C5C`) gains the raw's
`DS_00104B15 != 0` tail at the **end of the function, after the existing
`actors_update()` (`0x2A31C`)** — the raw's mode switch runs first, then
`0x2A31C`, then this block. In the raw's order: `0x3BB90` (collision), `0x12D48`
(camera dispatch), then for each live entry of the two player records `0x186D0` +
`0x2A690`, then `0x33F08` (health bars). The `switch (DS_00104B00)` keeps only
`case 3`, as today.

**Reused as-is:** `actors.c` (actor pool, the 47-opcode animation interpreter,
pset sync), `render.c`/`sprite.c`, `effects.c`, `gfx.c`, `config.c`, `input.c`,
`text`, and `rng.c` — which already reproduces `0x5D7DC` byte-exactly and whose
draws `attract.c` already sequences in the raw's order to keep the shared stream
faithful.

## Data flow

One demo frame, in the raw's order from `0x263F4`:

1. `0x3C5CC`, then `0x16D58` twice.
2. Latch previous positions for both fighters: `DS_001077E8 = DS_001077E4`,
   `DS_0010787C = DS_00107878`.
3. Project both: `0x17FA0(&DS_00100B60, &DS_00100AF0)`,
   `0x17FA0(&DS_00100B61, &DS_00100AF4)`.
4. `0x17580`.
5. `0x1958C` (fighter 1 update), `0x19068` (fighter 2 update).
6. Re-project both.
7. `0x1975C` — the think/control step.
8. Re-project both again.
9. `0x3CB68`; `0x35658` twice (HUD/health); `0x49C78` (the scene/effects pass).
10. `0x1282C`, `0x12DA8` (camera).

Then `flow.c`'s three existing tails (`0x10DB0`, `0x10E18`, `0x2BF08`), then the
`game_frame` tail described above.

**Health and damage.** Collision `0x3BB90` selects each fighter's hit radius from
`DS_000BEEF8[fighter*4]` (halved when `+0x54 == 2`) and tests overlap with
`0x4FB20`; `0x3BAEC`/`0x3B9D8` apply the hit to `DS_0010780A` (P1) and
`DS_0010789E` (P2); `0x33F08` draws the bars. No KO transition fires, per the
correction above.

**Lifecycle.** State 6 (one frame) → arms `DS_000F0A6A = 900` and enters state 7 →
state 7 runs the arena frame each tick → when the timer expires, `0x11BCC` restores
`DS_000F0A64 = DS_000F0A6C` and clears `DS_00104B19.byte2` and `DS_00104B15` →
control returns to the attract loop.

## Verification

Reuse the existing capture and oracle machinery; take no new capture.

- **One dump run, two windows.** `PR_FRONTEND_DUMP` runs once with the frame count
  raised so the dump covers the state-6 entry plus the demo's up-to-900 frames
  (it currently stops at 900). `tools/title_compare.py --frontend` is invoked
  twice over that dump — once for the existing front-end window and once for a
  new demo window — as two separate ladder targets, so a demo regression and a
  front-end regression stay independently diagnosable.
- **The demo window is report-only in cycle 1 and enforced in cycle 2.** Cycle 1
  cannot converge it: the measured divergence is the state-9 hold's globe render,
  before state 6, and closing the window also needs cycle 2's combat. Cycle 1's
  target therefore reports and records the measured divergence frame (capture
  811), and the declared expectation is *clean from the window start up to that
  frame* — which the measurement refutes, since the frame is the window's first.
  Cycle 2 closes the remainder and the same target becomes a ladder gate.
  The front-end chain shipped exactly this shape one cycle ago.
- **Pins.** The demo's non-determinism goes in the existing `tools/title_pin.py`
  `PATCHES` table: the two character picks through `0x41350`, plus any per-frame
  AI draw that proves unpinned. The table keeps its fail-closed byte verification.
  `rng.c` already tracks the generator and `attract.c` already tracks the stream
  order, so this is expected to be a small number of sites — but that is not
  assumed. Every pin is regenerated against a fresh capture after it is added,
  exactly as the front-end cycle required. **Cycle 1's outcome: the two state-6
  character picks were pinned** (draw1 at file `0x64901`, draw2 at `0x6493D`,
  both to the port's LCG value 4) — they are one-time draws whose values depend
  on the master loop's host-timed spin, and the unpinned capture's fighters did
  not match the port's. The state-9 hold's divergence is a render gap with no
  pinnable site, and the per-committed-move generator draw at `0x47063` is not
  constant-pinnable (cycle 2). See the status note and `port/spec/game_flow.md`.
- **Determinism.** The two-run `PR_FRONTEND_DET` gate extends over the demo
  window.
- **Unit proofs.** `0x4FB20` exhaustively as pure geometry; the `0x17FA0`
  projection arithmetic; the `0x3BB90` hit-radius table selection; the
  think/command mapping (`0x3B134`, `0x3B298`) over hand-built input masks; and
  every store in the state 6/7 handlers, with sentinels that differ from the
  post-conditions. Assertions that cannot fail are defects: three shipped in the
  last cycle before being caught, and one of them was caught only by mutation
  testing.
- **The oracle's claim stays narrow.** It proves that no content-bearing capture
  frame inside the port-exhibited window is unexplained. It cannot detect a port
  that under-renders. That limitation is stated in the spec, `README.md` and the
  tool, exactly as the front-end oracle's limitation is stated today.

## Risks

1. **Demo determinism.** The demo's trajectory depends on the AI's per-frame RNG
   draws matching the original's count and order. If they do not, the demo
   diverges progressively and pinning the character picks does not help. If it
   cannot be pinned honestly, it becomes a declared gap — never a fitted value.
   Cycle 1's finding sharpens this: the generator's `rng(0x64)` at `0x47063`
   executes once per committed move with a different value each time, so the
   constant-replacement pin shape cannot align it; cycle 2 must settle how the
   demo's stream is kept in step.
2. **`0x49C78` is 2492 bytes** with an unexplored callee set, the largest single
   unknown in the slice. It gets its own derivation before anyone ports it, and
   its unpinnable parts become named gaps.
3. **16.16 projection drift.** One rounding difference in `0x17FA0` drifts the
   whole arena across the window, so the projection is unit-proven before
   anything renders.
4. **Frame timing.** The capture samples at 70.09 Hz against the port's 60 Hz, so
   the aligner's splice classification carries the window, as it does for the
   title and front-end oracles.
5. **Scale, and the interim gap.** ~55 named functions across seven stages. The
   cycle split above cuts that in half and gives cycle 1 a bounded, measured
   divergence point instead of a long stretch with no converging signal. The cost
   is deliberate: cycle 1 ships a report-only oracle and a declared gap at its
   measured divergence frame (the state-9 hold, not a landing hit). If cycle 1's
   layers do not all land, the same
   fallback applies again within the cycle — converge what can be converged,
   declare the rest.

## Sequencing

Cycle 1 opens the oracle provisionally and **does not close it**; cycle 2 closes
it. The stages below are the two cycles' task order.

**Cycle 1 — motion and render.**

1. Derivation: `0x49C78`, the fighter chains and the think chain — the record the
   later tasks implement from, with unit-test values, and the answer to open
   question 2 (how often the AI consumes RNG).
2. Camera and projection — unit-proven first; the bellwether.
3. Arena and fighter render — first partial oracle signal.
4. The think/AI chain — the fighters actually act, and the demo moves.
5. State 6/7 handlers, the `game_frame` tail minus its combat calls, the
   900-frame timer and the `0x11BCC` exit.
6. The oracle opened provisionally: raise the dump length, add the demo window as
   a report-only target, add the pins that the demo's motion needs (none were
   justified — see the cycle-1 outcome), and record the measured divergence frame
   as the declared bound. The measured frame is the state-9 hold's globe render,
   not the assumed first landing hit.

**Cycle 2 — combat and closure.**

7. Collision (`0x3BB90`, `0x4FB20`, `0x3BAEC`, `0x3B9D8`), damage, the health bars
   (`0x33F08`, `0x35658`, `0x1D890`) and the tail's combat calls.
8. Oracle closure: any remaining pins, the window converges to zero unexplained,
   and the demo window joins the ladder as an enforced gate.

## Success criteria

**Cycle 1 — partially met; the declared criterion is refuted and the bound
corrected (DONE_WITH_CONCERNS).** The demo visibly moves and animates (64 distinct
state-7 images over loop frames 1071..1969, versus one before). The declared
criterion — clean from the window start up to the first landing hit — is **not**
met: the window diverges at its first frame (capture 811), which the measurement
shows is the state-9 hold's globe render, **not** a landing hit, so that measured
frame is recorded as the declared bound instead. The two state-6 character picks
are pinned (`0x64901`/`0x6493D`); the state-9 hold itself has no pinnable site.
Every helper has unit proof that fails under a mutation of the code it tests;
every unported piece is a named gap carrying its address; no fitted constant
ships; `make verify` is green with the title, attract, smacker, `oracle
C-vs-Python: 9866 writes byte-exact` gates unmoved and the front-end gate still
`0 unexplained` (its window indices moved with the re-capture — see the spec).

**Cycle 2.** The demo window reports zero unexplained content-bearing capture
frames and is enforced in the `make verify` ladder, with cycle 1's declared bound
retired rather than narrowed.

## Gaps this cycle leaves

Recorded in `port/spec/game_flow.md` and `README.md`, named with addresses:
state 8 and the `0x257A4` coin divert; the whole `DS_00104B00` mode graph; the
player entry/select/VS/continue screens; `0x1EEB0`; `0x1F458`; `0x1EA08`'s four
remaining call sites and the un-ported half of `0x1EA08` itself; the human input
mapping and the two-player path; `0x27FA8` and the KO/win screens; and whatever
inside `0x49C78` resists pinning.

## Open questions

1. **Which modes the coin divert actually sequences** (`0x10` → `0x11` → … →
   `0x13`): inferred from the mode setters during reconnaissance, not enumerated.
   It does not affect this cycle — the coin path is out of scope — but the next
   cycle must settle it before designing the mode graph.
2. **Whether the demo's AI consumes RNG every frame or only on decisions.** This
   determines how many pins the demo needs. The derivation task settles it before
   any pinning work starts.
3. **Whether `0x49C78`'s internals are pinnable at all**, or whether parts of it
   become permanent declared gaps.
