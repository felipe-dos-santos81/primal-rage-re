# Closing the small fidelity gaps (design)

**Context.** Cycle 3 closed the demo fight's palette and combat tail and left a
named-gap inventory (the cycle-3 derivation record `§6` and `§10`). This cycle
closes the small ones — the gaps that are off the demo path or oracle-neutral —
and repairs the demo oracle's blind spot. It is a maintenance cycle: nothing
observable is expected to move, so the evidence standard, not the oracle, is
what proves the work.

## Goal

Close the five named fidelity gaps and fix the demo oracle's `res is None`
fallback, **moving no enforced oracle claim** — so the port's fight, effects and
attract paths are faithful and every named gap in the record is either closed or
re-scoped with evidence.

## Scope

**In:**

1. **The five unported `+0x52` handlers** — `0x359E0` (state 1), `0x35C1C` +
   `0x35D20` (state 2), `0x37464` (state 8), `0x33B00` (state 19), `0x35E6C`
   (state 20). Cycle 3 ported seven of the twelve and measured that none of the
   twelve is on the port's demo path (record §2.2/§2.3); these five are the
   remainder, reachable in a real match.
2. **`0x349C8`'s bit-6/7 deep callees** — `0x36870`'s `0x385B0` and `0x37D18`'s
   `0x39A10`. Cycle 3 transcribed these at the call level (record §2.2) and named
   their bodies as a gap (§6.4).
3. **The loader flush scope** — `platform/res.c`'s `res_load_present`,
   `platform/gfx.c`'s `gfx_flush_palette`/`palette_record` and `game/flow.c`'s
   ordering. Record §4.2 finds it separable as a code change; §4.3 measures it
   oracle-neutral (1381/1381 byte-identical dumped frames), so it is a
   fidelity-only change.
4. **The `0x13xxx` effect call sites** — the render path itself is no longer a
   gap (cycle 2: no draw was missing); its effect call sites are deferred.
5. **The attract/scene palette drivers** — `0x4F7F4` and `0x4F83C`, a cycle-2
   declared gap.
6. **The demo oracle's `res is None` fallback** —
   `tools/title_compare.py:264-267` (no captured frame exhibits any port frame →
   no window) and its report-only branch at `:484-514`. The derivation
   establishes whether this is a tooling gap or the freeze's symptom; only a
   tooling part is fixed.

**Out:** the pose/freeze subsystem (cycle 4's `0x19020` chain); 831/832's
held-frame presentation (un-derivable, a host property); the interactive match;
the audio gaps.

**Recorded, not forced:** if the oracle's `res is None` proves to be the freeze's
symptom rather than a tooling gap, the demo alignment cannot move until cycle 4.
That outcome is written into the record as a named gap; no pin is fitted to
force it.

## Architecture — eight tasks

**Task 1 — the derivation record** (no porting code). All five gaps plus the
oracle question, in the cycle-3 record's shape: per gap, the owner chain, the
values with their proving addresses, the **closure size against the size gate**,
the porting plan, and the unit-test values. Plus a section establishing *why*
`res is None` fires and whether a tooling-only fix exists. A value that cannot be
pinned is a named gap with its evidence, never invented.

**Tasks 2–6 — one gap each**, in the order the derivation's sizes recommend:
the five handlers; `0x349C8`'s deep callees; the loader flush scope; the
`0x13xxx` call sites; the attract/scene palette drivers. Each ports one C
function per original with its address tag, adds a raw-derived unit assertion
with a mutation proof, runs the ladder, and updates the record.

**Task 7 — the demo oracle**, if Task 1 finds a tooling fix. No engine change.

**Task 8 — the record and the Gate.** The gap inventory updated — every gap
closed or re-scoped with evidence — and `README.md`, `port/spec/game_flow.md`
and the spec's Outcome.

**The size gate applies per group.** Any group the derivation sizes at or above
a task becomes a follow-on cycle, the way cycle 3 re-scoped the poses —
re-scoped **before** porting, not after.

## Verification

- **Per gap:** a raw-derived unit assertion with a **mutation proof** — the
  assertion must fail under a perturbation of the code it tests — and the ported
  function faithful: one C function per original, address-tagged, per `AGENTS.md`.
- **Whole cycle:** `make verify` exits 0 with every enforced oracle claim
  unmoved — title `54 clean, 55 splice, 2 transition, 0 unexplained` and
  `54 clean, 57 splice, 0 unexplained`; attract `FIRST DIVERGENCE at capture
  frame 215`; front-end `[560..830]`/271 `0 unexplained`; smk `120/120` +
  `41/41`; the C-vs-Python byte-exact claim; `symbols.h` idempotent. 0 warnings.
- **Where a measurement exists** (the arena's byte-diff, the attract palette's
  DAC range) it is **reported, never required to move**.
- **The record's gap list** ends with every gap closed or re-scoped with
  evidence — no gap silently dropped, no claim stronger than its evidence.
- **The demo oracle:** its behaviour changes only if Task 1 finds a tooling fix;
  the claim that changes is documented in the record.

## Risks

- **The attract/scene palette drivers may be a subsystem.** The size gate catches
  it before porting; it becomes a follow-on cycle.
- **The demo oracle's `res is None` may be the freeze's symptom.** Then it is
  recorded, not fixed here, and cycle 4 owns it.
- **The five handlers are off the demo path** (record §2.3), so their
  correctness rests on the raw and the assertions — not on an oracle move.
- **The `0x13xxx` call sites' shape is un-derived** until Task 1; their closure
  may re-scope the task.
