# Design: Primal Rage (DOS) → SDL3 port — sub-project 4a-iii: title-path residuals

This spec covers one cycle: closing the residuals the shipped title path
actually reaches, so that the title oracle's coverage claim becomes true. Base
is `main` at `769b272` (4a-ii merged as `a7b515d`, plus the FAT.OPL audio fix).

## 1. Context

Sub-project 4a-ii ported the actor system and the real title state and proved it
against two captures of the pinned original (`make title-oracle`, tear-aware,
zero pixel tolerance). Its report (`docs/superpowers/plans/2026-09-17-actor-system-report.md`)
records residuals, and two of them undermine the proof rather than merely
extending it:

* **`0x2BF08` was inerted in the capture.** `tools/title_pin.py` carries a fifth
  site — a scope decision, not a behaviour pin — that rewrites `0x2BF08`'s first
  byte (`53` → `c3`, a `ret`). The oracle therefore proves the ported subset of
  the original, not the original. The report is explicit that `0x2BF08`'s own
  behaviour is unproven.
* **`0x13C70` is reached once inside the window and is deferred.** At
  `flow.c:417` (original `0x123EA`) the title's phase-1 arm walks retired nodes
  typed `0x3E688` and must call it. The port does nothing there.

`0x13C70` is not cosmetic: it increments `DAT_0009AF3D`, and `0x121A0`'s phase-2
exit at `flow.c:432` tests `DSB(DS_0009AF3D) == 0`. With `0x13C70` unported the
flag stays 0, so the phase-2 exit currently fires for the wrong reason.

### 1.1 Why streamed Smacker audio (2b-ii) is not in this cycle

2b-ii was proposed as the next cycle. Its premise does not hold and this was
established during brainstorming:

* Both shipped movies are silent. `tools/smk_info.py` reports
  `flags = 0` and `audio_descriptors = [(0,0) × 7]`, `audio_frame_flags = []`
  on `TWI5.SMK` and `TWG.SMK`; the CD copies are byte-identical. The 2b design
  doc already recorded this ("The movies are silent").
* No ported caller drives the stream. The four AIL streaming stubs are the RAD
  Smacker sound adapter, and all callers of `FUN_000102b8` live in
  sub-projects 4/5.

Building it now would be machinery proven only at unit level against a
synthetic fixture, with no shipped bytes and no caller — the failure mode this
cycle exists to correct. It is dropped and revisited when a caller exists.

### 1.2 Decisions taken during brainstorming

| # | Decision |
|---|---|
| 1 | **2b-ii dropped.** Build streamed Smacker audio when a ported caller (4/5) drives it. |
| 2 | **Scope = the title-reachable residuals only**: `0x13C70` + its minimal subsystem slice, and `0x2BF08`. |
| 3 | **Oracle reference = the true un-pinned original.** Remove the fifth `title_pin.py` site, re-pin, re-capture; the ported-subset reference is retired. |
| 4 | **Structure = oracle-truth first.** Establish the reference before building on it. |
| 5 | **Falsifiability rule.** Every ported item either moves the oracle and is proven by it, or is declared an explicit coverage gap with a unit-level proof. No silent unproven code. |
| 6 | **Probe first for `0x13C70`.** Determine whether porting it moves the composite, and if it does not, declare the gap rather than claim proof. |

### 1.3 Re-scope during execution (amended so the record is self-consistent)

Two changes happened after execution started; this section records them and §2/§8
are amended accordingly.

* **`0x2BF08` re-scope.** The first draft removed the inert pin only to measure
  drift and ported `0x2BF08` conditionally. The un-pinned capture instead
  diverged totally (all 587/590 frames unexplained), falsifying the
  frames-32/64/96 prediction (Decision 4). The human re-scoped to port the
  overlay in-cycle and seed its captured inputs. The `DS_00105C05` row seed is
  **`1`**, not the diagnosis's `7`: one text row is `20/3` px, so text row `1`
  places the glyph at screen rows 7–12; the port dump is byte-identical to the
  capture there.
* **`0x134C0` moved into scope.** Task 6's spawn wiring left `DS_0009AF3D` with
  no writer but the spawn's `+1` (`effects_clear` only zeroes it), and the
  title's state-2 exit tests that count against zero. The title therefore
  stalled past frame 95 where it previously progressed. The human elected to
  port the step/age `0x134C0` in-cycle rather than carry the stall, so it is no
  longer a non-goal or a carried residual.

## 2. Scope

### In scope

* The `0x13xxx` effect-list slice the in-window call needs: spawn `0x13C70`,
  teardown `0x13DF0`, the list sentinels `DS_000FCCE0`/`DS_000FCCE8`, the lock
  `DAT_0009AF3C` and the active count `DAT_0009AF3D`, over the two intrusive
  list primitives `0x249B0` (link) and `0x249D0` (unlink). Plus the per-frame
  step/age `0x134C0` (§1.3).
* `0x2BF08`, the `0x11D04` tail's message/text tick, in `flow.c`.
* Removing the fifth `tools/title_pin.py` site, re-capturing twice, and
  re-deriving the oracle reference and its determinism proof.
* Unit tests, negative controls and the falsifiability record.

### Non-goals

* The `0x13xxx` **render** path beyond what the spawned record needs to exist.
  If the effect is never drawn in-window, that is a declared gap (§6).
* `0x11000`'s attract sub-machine (4d), `0x38A38`/`0x389C4`, `0x292AC`,
  `0x4F644`, `0x10DB0`/`0x10E18`, and the player/fight-engine paths.
* Any input model. `0x10DB0`/`0x10E18` gate on input-state bits the pinned
  no-input run never sets, so they stay deferred to 4b.
* Streamed Smacker audio (2b-ii, §1.1).
* Menus / EEPROM (4), the fight engine (5).

## 3. Architecture

Three pieces, each where its invariant belongs.

**`port/src/game/effects.{c,h}` (new)** — the `0x13xxx` effect list. Owns the
sentinels, the lock, the active count, `effects_spawn` (`0x13C70`) and
`effects_clear` (`0x13DF0`). This is the module the deferred marker in
`actors_reset` (`actors.c:103`) has been waiting for; `actors_reset` calls
`effects_clear` instead of describing it.

**List primitives.** `0x249B0` and `0x249D0` are 15- and 31-byte intrusive
doubly-linked-list link/unlink primitives with 33 callers each in the original —
general engine furniture, not effect-specific. They go in a small shared helper
(`platform/list.{c,h}` or the nearest existing owner) only if a second caller
appears in this cycle; otherwise they stay private to `effects.c` and the
promotion is deferred until one does. `effects.c` is the only consumer the
window reaches.

**`flow.c`** — `0x2BF08` is implemented at its existing PORT marker
(`flow.c:863-875`), in the `0x11D04` tail. Four of its six callees are already
ported: the string reader `FUN_0001C500` (`string_decode`/`game_string_get`)
and the text cursor trio `FUN_0002F4BC`/`FUN_0002F198`/`FUN_0002F280`
(`text_cursor_hold`/`text_cursor_set`/`text_cells_release`). Only the query
`FUN_0002CAA8` and the `sprintf` `FUN_00065546` are new, and neither is a
module — no new file is justified.

**`0x1B544`** — the effect record's table copy reads a sprite/document header
through this resolver. It is the same handle→address mapping `res_resolve`
already provides (`(index << 23) | (offset & 0x7FFFFF)`), plus load-on-demand
branches the shipped assets do not exercise. Use the existing resource contract;
add only what the effect record reads.

## 4. Data flow and the falsifiable link

`0x13C70` unlinks a free effect record from the `DS_000FCCE8` list, holds
`DAT_0009AF3C` across the walk, then initialises it: `+0xF = 0x80`, `+0xC`
(byte) `= 3`, `+0x8` = a pointer to the source record (the `ECX` argument),
`+0xD` = the byte argument, `+0xE = 1`. It reads the entry count from the
**source** record's `+0xC`, zeroes `+0x10 + 4*i` for that many entries, and
copies that many dwords of the header table returned by `FUN_0001B544()` into
`+0x410 + 4*i`. It then links the record onto the active list (`FUN_000249B0`),
increments `DAT_0009AF3D`, and releases the lock. `0x13DF0` walks
`DS_000FCCE0`, tears each entry down, and zeroes `DAT_0009AF3D`.

The falsifiable link is the count: because `0x121A0`'s phase-2 exit tests
`DS_0009AF3D == 0`, a faithful `0x13C70` changes the state machine, which the
title oracle must then either accept or reject. If the phase-2 path is not
reached inside the pinned window, the state change is real but invisible, and
§6's gap rule applies.

`0x2BF08` is gated in this order by `DAT_0009AD58 != 0` (early return),
`FUN_0002CAA8()`, `DAT_000EF6DC & 0x1F`/`& 0x20` (frame phase), and the latch
`DAT_00105C04`; the message id `DAT_00105C00` selects the string branch. Its
observable signature was already written down in the 4a-ii report: it reaches
the aperture only if a message is active, so **drift at exactly frames
32/64/96** is the prediction that proves it does.

## 5. The oracle change

`tools/title_pin.py`'s fifth site is removed and the pin regenerated. The
un-pinned `PRAGE.EXE` is captured twice from DOSBox-X exactly as 4a-ii did
(`make title-pin`, then two independent captures under `data/title-captures/`),
which becomes the reference. The tear-aware, zero-tolerance model, the
`MIN_SHARED_CLEAN = 1` non-vacuity guard, per-frame length validation and the
required-and-incomplete non-zero exit are unchanged.

This is the risky step and it is deliberately first: the re-capture may show
drift the inert pin was hiding, in which case the cycle stops and re-scopes
before any code is written.

The 4a-ii reference numbers (capture 1: 53 clean / 56 splice / 1 transition /
0 unexplained, 95/96; capture 2: 53 / 57 / 0 / 0, 94/96; determinism 36 agree /
0 disagree) describe the **pinned** original and are superseded, not carried.

## 6. Testing and Definition of Done

* **Oracle:** `make title-oracle` green against the un-pinned captures (both),
  determinism proof intact, required-and-incomplete path still exits non-zero.
* **Drift measurement:** record where, if anywhere, the port drifts after
  un-pinning. Frames 32/64/96 only ⇒ port `0x2BF08` and re-verify. Anything
  wider ⇒ stop and re-scope (Decision 4).
* **Unit tests:** effect spawn/teardown and the link/unlink primitives,
  including empty-list sentinel self-linking and unlink on an already-unlinked
  node; every branch of `0x2BF08`, including the early return and the latch.
* **Negative controls:** one test or assertion per ported item that fails if the
  item is stubbed back out.
* **Falsifiability record:** for each of `0x13C70` and `0x2BF08`, either the
  oracle evidence that it moves the composite, or an explicit statement in the
  report that it does not, with the unit-level proof that carries it instead
  (Decision 5, Decision 6).
* **Ladder:** `make verify` exit 0, 0 warnings, `symbols.h` byte-identical, no
  file under `data/` modified.

## 7. Invariants

* An empty list is self-linked sentinels, never NULL.
* Unlink is safe on an already-unlinked node; the lock flag is restored on
  every exit path.
* Every record table copy is bounded by the count declared at the **source**
  record's `+0xC`, read before any entry is written; `0x13C70` writes no further
  than that count times the record's own stride.
* `title_pin` fails closed on a site mismatch with equal-length replacement
  bytes; a re-capture that does not reproduce determinism fails rather than
  weakening the model.

## 8. Residuals carried out of this cycle

* `0x11000` attract sub-machine, `0x38A38` (`DS_00107900`'s producer),
  `0x389C4`, `0x292AC`, `0x4F644` — 4b/4d.
* `0x10DB0`/`0x10E18` — input-gated; deferred to 4b with the existing evidence.
* The `0x13xxx` render path, if §6 declares a gap for it.
* 4a-i's bank byte 0 and the `DS_00107A3E`/`3A`/`38` projection load width.
* The 2a OPL register-stream divergence (`make verify` reports first difference
  at C write 2: `reg=0x20 val=0x00` vs the capture's `reg=0xb0 val=0x2b`, a
  key-on), and the `0x2D974` save/config pin — 4b/2a-ii.
* Streamed Smacker audio (2b-ii) — until a caller exists.

## 9. Verification

Proof for this cycle is the re-pinned title oracle plus the falsifiability
record. It will **not** be a claim that the port matches the original for any
path the window does not reach: `0x11000`, the `0x13xxx` render path beyond the
spawned record, the input-gated tail, the attract sub-machine or the mode-1
sprite branch. Each is named above rather than implied green.
