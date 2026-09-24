# The demo arena backdrop — the state-7 arena's missing horizon layer (design)

**Context.** Cycle 4 (pose/freeze) ported the freeze's two halves and the
`0x17FA0` page-flag tail; the fighters now byte-match capture 834, but the demo
oracle still takes its `res is None` fallback because the 482/834 witness
carries an **18 294 B (9.5 %) / 6 194 px** residual — a dark mountain/rock
silhouette on the horizon. Cycle 4 re-scoped that residual to the
demo-fight-closure subsystem (`platform/render.c`'s scroll/zoom path,
`0x38730`/`0x387F4`/`0x38890`/`0x38A38`), and the README's gap inventory carries
it as "the demo fight's arena backdrop" (record §11.4; fidelity-gaps §7).

## Goal

Close the demo's state-7 arena render gap so the port's arena frame byte-matches
the capture.

## Acceptance

1. **Byte-exact.** Port frame 482 byte-matches capture 834 — 0 of 192 000 RGB
   bytes differ (0 px). The cycle reports the diff before and after. If
   byte-exactness is unreachable, the residual is **named with its evidence and
   its owner** — never a fitted number.
2. **Faithful port.** One C function per original, address-tagged, wired at its
   call site. `make verify` exits 0 with every enforced claim unmoved — title
   `54 clean, 55 splice, 2 transition, 0 unexplained` and
   `54 clean, 57 splice, 0`; attract `FIRST DIVERGENCE at capture frame 215`;
   front-end `[560..830]`/271 `0 unexplained`; smk `120/120` + `41/41`;
   `oracle C-vs-Python: 9866 writes byte-exact`; `symbols.h` byte-identical.
   0 warnings.
3. **The demo oracle's fallback is a consequence, not a gate** — it is
   report-only and always exits 0; if the frame matches, its window should
   align, but the cycle does not assert it.
4. **Assertions** mutation-proven, in the existing per-area files.

## Evidence (measured before the design)

- The gap is a **dark mountain/rock silhouette** on the horizon (y ≈ 85–140,
  right and centre); the fighters, temple, text and HUD all match.
- The port **does** spawn and draw the two scene actors: layer 2 = sky (sprite
  id 11232, 549×213 at y = 0), layer 1 = sea (id 11233, 975×64 at y = 137); the
  demo's scene index is 0 (`render_scroll_setup(0)`); scene 0's first-actor
  descriptor (`0xBDE50`) carries the handle `0x0A838B44` — the
  combat-fidelity record's "arena backdrop"
  (`2026-09-22-combat-fidelity-derivations.md` §1.2, table idx 1).
- **Correction (raw wins).** Cycle 4's claim that "the port never renders the
  arena backdrop" is **false** — the backdrop actor *is* drawn. The cause is
  subtler: a wrong sprite/frame, a missing sub-layer, or a missing draw. Task 1
  pins which.

## Scope

- **In:** whatever Task 1's runtime differential + Ghidra pins as the missing
  layer, if its genuinely-new closure is under the size gate.
- **Out:**
  - Any piece whose genuinely-new closure is **≥ ~4 KB or ≥ ~20 new functions**
    — a follow-on cycle, named in Task 1.
  - The state-9 hold's animation (fidelity-gaps §7.6), the interactive match
    (§7.12), the audio gaps (§7.13), 831/832's held-frame presentation (§7.11)
    — each carries its owner and is not touched.
  - Any other state-7 arena divergence the derivation finds that is **not** the
    backdrop layer: named and re-scoped with its size, not silently pulled in.

## Architecture — derivation-first, size-gated

**Task 1 — the derivation record** (no porting code). One record,
`docs/superpowers/plans/2026-09-24-arena-backdrop-derivations.md`. Pins the
missing layer by **runtime differential** — a temporary, reverted port dump of
the render list + the scene actors/psets at frame 482, compared to the
original's live RAM under dosbox-x — plus Ghidra; derives the raw addresses, the
body and written fields, the porting plan, and the unit-test values; applies the
size gate; and records the Evidence correction above.

**Tasks 2–N — the port.** Each ports one piece from the record: one C function
per original, address-tagged, gated by a raw-derived unit assertion with a
mutation proof and the full ladder.

**Task N+1 — the outcome.** Re-measures the 482/834 byte-exact; updates the gap
inventory (`README.md`, `port/spec/game_flow.md`, the record's outcome, this
spec) — each gap closed or re-scoped with evidence.

**Homes.** `platform/render.c` (the scroll/zoom path and the render list), or
wherever the record's seam lands. SDL and file I/O stay in `host.c`/`main.c`.

**The size gate.** A group whose genuinely-new closure is **≥ ~4 KB or ≥ ~20 new
functions** becomes a follow-on cycle — measured in Task 1, re-scoped **before**
porting.

## Verification

- **The cycle's own gate:** port frame 482 ↔ capture 834 byte-exact
  (0 / 192 000 B), reported before and after.
- **Per task:** a raw-derived unit assertion with a **mutation proof** — the
  assertion must fail under a perturbation of the code it tests — and the ported
  function faithful: one C function per original, address-tagged, per
  `AGENTS.md`.
- **Assertions live in the existing per-area files** (`test_fight.c`,
  `test_render.c`) — no new test file.
- **Per-task ladder:** `cmake --build build && ./build/run_tests && make verify`.
- **If a correct fix moves an enforced claim:** the task **halts** and reports
  the old value, the new value, and the raw evidence that the new one is more
  faithful. If justified, the claim is updated **in the same commit** with the
  reason recorded. The default is no move.
- **Never ship a fitted constant.** Every value is derived from the raw bytes or
  a capture, with the address that proves it. A value that cannot be pinned is a
  **named gap with its evidence**.
- **On any plan-vs-raw conflict the raw wins** — record the correction and its
  address.

## Workflow

Branch `arena-backdrop` off `main`, **in-place** (no worktree — a worktree lacks
the git-ignored `data/`, so the oracles would skip silently), run through
subagent-driven development with a review per task, the ledger at
`.superpowers/sdd/2026-09-24-arena-backdrop/`. Commit style `<area>: <what
changed>`; never `git add -A`.

## Risks

- **The layer may be a whole subsystem.** If Task 1 finds the backdrop needs
  more than the size gate allows, the cycle stops after Task 1 and names the
  follow-on — the derivation is still the cycle's deliverable.
- **The cause may be a data/sprite divergence, not code.** If the missing layer
  is a wrong sprite frame or a bad resource resolve, the fix is in the
  animation/resolve path, not a new draw; Task 1 names the actual owner.
- **Byte-exactness at 482/834 may be unreachable** (e.g. a host-timing or palette
  property). The residual is then named with its evidence, not fitted.
- **The enforced oracles should not move** (the arena is after the front-end
  window), but a shared-path change would show in the front-end window first.
