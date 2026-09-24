# Closing the small fidelity gaps (design)

**Context.** Cycle 3 closed the demo fight's palette and combat tail and left a
named-gap inventory (the cycle-3 derivation record `§6` and `§10`). This cycle
closes the small ones — the gaps that are off the demo path or oracle-neutral.
It is a maintenance cycle: nothing observable is expected to move, so the
evidence standard, not the oracle, is what proves the work.

**Revised by the design grilling** (rounds 1–3, 2026-09-22). The scope lost two
items and gained two; the decisions below are the grilled ones.

## Goal

Close the in-scope named fidelity gaps — moving no enforced oracle claim — so the
port's fight and attract paths are faithful and every named gap in the record is
either closed or re-scoped with evidence.

## Scope

**In:**

1. **The five unported `+0x52` handlers** — `0x359E0` (state 1), `0x35C1C` +
   `0x35D20` (state 2), `0x37464` (state 8), `0x33B00` (state 19), `0x35E6C`
   (state 20). Cycle 3 ported seven of the twelve and measured that none of the
   twelve is on the port's demo path (record §2.2/§2.3); these five are the
   remainder, dispatched by the ported `fight_health_sync` and reachable in a
   real match.
2. **`0x349C8`'s bit-6/7 deep callees** — `0x36870`'s `0x385B0` and `0x37D18`'s
   `0x39A10`. Cycle 3 transcribed these at the call level (record §2.2) and named
   their bodies a gap (§6.4).
3. **The loader flush scope** — `platform/res.c`'s `res_load_present`,
   `platform/gfx.c`'s `gfx_flush_palette`/`palette_record` and `game/flow.c`'s
   ordering. Record §4.2 finds it separable as a code change; §4.3 measures it
   oracle-neutral, so it is fidelity-only.
4. **The attract/scene palette drivers** — `0x4F7F4`, `0x4F83C` and `0x33874`.
   The closure measured ~3 functions / ~300 B of palette work (the other ~21
   "new" members are shared resource/config code), so it **fits this cycle**, not
   a follow-on.

**Out:**

- The pose/freeze subsystem (cycle 4's `0x19020` chain).
- 831/832's held-frame presentation (un-derivable; a host property).
- The interactive match.
- The audio gaps.
- **The demo oracle's `res is None` — re-scoped to cycle 4.** The exploration
  proved it is the freeze's symptom, not a tooling gap: of the port's 398
  distinct demo-frame hashes exactly **one** appears in the whole 3617-frame
  capture, at capture 830, which `capture_lo = 831` excludes. No window, slice or
  load-path change can produce an alignment the content does not contain. The
  oracle's report-only fallback is correct behaviour and is not repaired.
- **The `0x13xxx` call sites — re-scoped to the interactive match.** The whole
  `0x13xxx` effect set is already ported; what is deferred is two **caller**
  functions, `0x29B74` (81 B) and `0x41578` (146 B), whose only reachability is
  `0x24C5C`'s unported modes and the menu/match chain (`0x277C0`, `0x28788`,
  `0x27A2C`, `0x416D4`, `0x41C28`, the input handlers). Porting them alone ships
  unreachable code, which the repo's rules forbid.

## Architecture — six tasks

**Task 1 — the derivation record** (no porting code). One record,
`docs/superpowers/plans/2026-09-22-fidelity-gaps-derivations.md`, in the cycle-3
shape: per gap, the owner chain, the values with their proving addresses, the
**closure size against the size gate**, the porting plan, and the unit-test
values. Plus three investigations this cycle's scope turned on:

- **The 169-tick static hold** — the port holds the front-end's last frame
  byte-static for 169 ticks (dump 312–480, loop 901–1069) before state 7 begins;
  the capture does not. Establish whether it is the load/stall model's expected
  behaviour or a real divergence, and if the latter name it as a gap.
- **`0x4F83C`'s shape** — whether it is a distinct code-pointer target or a tail
  of `0x4F7F4`.
- **The attract oracle's divergence at capture 215** — whether it is
  palette-related, so the drivers' port is measured against the right thing.

**Tasks 2–5 — one gap each**, provisional **smallest-first** order (the five
handlers; the deep callees; the loader flush scope; the attract palettes), with
Task 1's record holding final authority to reorder. Each ports one C function per
original with its address tag, adds a raw-derived unit assertion with a mutation
proof, runs the ladder, and updates the record.

**Task 6 — the record, the Gate, and the stale documentation.** The gap inventory
updated — every gap closed or re-scoped with evidence — and `README.md`,
`port/spec/game_flow.md` and the spec's Outcome. Plus three stale claims
corrected with their evidence: `README.md:198-199` contradicting `:331`; the
camera-chain deferral framing (`game_flow.md:327-341`, `README.md:283-284`), now
that `port/src/game/camera.c` ports that chain; and `0x13B3C` described as
deferred when it is dead (zero callers anywhere in the image).

**The size gate:** a group whose genuinely-new closure is **≥ ~4 KB or ≥ ~20 new
functions** becomes a follow-on cycle — measured in Task 1, re-scoped **before**
porting.

**The representation rule for `0x4F83C`:** a distinct named C function with its
address tag, the way the port already treats code-pointer targets (the `+0x52`
handlers, `attract_scene_tick`) — the rule's spirit is one unit per original unit
of behaviour, and this is a distinct behaviour with a distinct address.

## Verification

- **Per gap:** a raw-derived unit assertion with a **mutation proof** — the
  assertion must fail under a perturbation of the code it tests — and the ported
  function faithful: one C function per original, address-tagged, per `AGENTS.md`.
- **Assertions live in the existing per-area files** (`test_fight.c` for the
  handlers, `test_effects.c` for the effects, `test_frontend.c` for the
  palette/flush) — no new `test_gaps.c`.
- **Whole cycle:** `make verify` exits 0 with every enforced oracle claim
  unmoved — title `54 clean, 55 splice, 2 transition, 0 unexplained` and
  `54 clean, 57 splice, 0 unexplained`; attract `FIRST DIVERGENCE at capture
  frame 215`; front-end `[560..830]`/271 `0 unexplained`; smk `120/120` +
  `41/41`; the C-vs-Python byte-exact claim; `symbols.h` idempotent. 0 warnings.
- **Where a measurement exists** (the arena's byte-diff, the attract palette's
  DAC range) it is **reported, never required to move**.
- **If a correct fix moves an enforced claim:** the task **halts** and reports the
  old value, the new value, and the raw evidence that the new one is more
  faithful. If justified, the claim is updated **in the same commit** with the
  reason recorded. The default remains that no enforced claim moves.
- **The record's gap list** ends with every gap closed or re-scoped with
  evidence — no gap silently dropped, no claim stronger than its evidence.

## Workflow

Branch `fidelity-gaps` off `main`, **in-place** (no worktree — a worktree lacks
the git-ignored `data/`, so the oracles would skip silently), run through
subagent-driven development with a review per task, the ledger at
`.superpowers/sdd/2026-09-22-fidelity-gaps/`.

## Risks

- **The attract/scene palette drivers may be larger than measured.** The size
  gate catches it before porting; it becomes a follow-on cycle.
- **The five handlers are off the demo path** (record §2.3), so their correctness
  rests on the raw and the assertions — not on an oracle move.
- **The 169-tick hold may be a real divergence** the load model cannot explain;
  Task 1 names it and Task 6 records it, without forcing a fix into this cycle.
- **The deep callees' bodies are un-derived** until Task 1; their closure may
  re-scope that task.

## Outcome (recorded, Task 6)

**All four in-scope gaps are closed; no enforced oracle claim moved; every
out-of-scope gap is carried with its owner.** No single group triggered the size
gate; the **branch total crosses it** (20 f / 4 675 B).

* **The five `+0x52` handlers** — ported, dispatched, 31 `CHECK_EQ_INT` with seven
  mutation proofs.
* **`0x349C8`'s bit-6/7 deep callees** — ported at the raw's sites, with the
  minimal caller chain (5 functions / ~1 741 B) **ratified by the human** so the
  callees stay reachable; 15 `CHECK_EQ_INT`, five mutation proofs.
* **The loader flush scope** — the record's "scoped flush" was **refuted by the
  raw** (`0x1C470` is a whole-list drain; the port's flush was already faithful);
  the real gap, the missing initial record `{0xBD470, 0, 1, 0}`, shipped and
  asserted.
* **The attract/scene palette drivers** — `0x4F7F4`/`0x4F83C`/`0x33874` ported;
  `0x4F83C` ships as an **accepted exception** (test-only; its callers are
  unported; mandated by the representation rule, Q10); the attract claim is
  unmoved (215).

**The 169-tick hold** is a **real divergence** (the state-9 screen's animation
stops ~169 ticks early), not the load/stall model; oracle-neutral, carried
forward. **The size gate:** no single group triggered it (the four groups were
each task-sized — 10 f/2 223 B, 2 f/418 B, 0, 3 f/293 B — and Task 3's ratified
caller chain 5 f/1 741 B), but the **branch total 20 f / 4 675 B crosses the
≥ ~4 KB line**. The chain was **ratified by the human** (the callees' call sites
live in those callers; the repo forbids unreachable code), so the gate's purpose
— catching a subsystem-sized group before porting — was served.
**No enforced claim moved**: title `54 clean, 55 splice, 2 transition, 0
unexplained` + `54 clean, 57 splice, 0 unexplained`; attract `FIRST DIVERGENCE at
capture frame 215`; front-end `[560..830]`/271 `0 unexplained`; smk `120/120` +
`41/41`; C-vs-Python `9866 writes byte-exact`; `symbols.h` idempotent; 0 warnings.
**Carried with owners:** the freeze and the demo oracle's `res is None` → cycle 4;
the `0x13xxx` call sites → the interactive match; 831/832 (un-derivable); the
interactive match (unowned); the audio gaps (the audio sub-project, 2b-ii); the
state-9 hold; `0x38154`; the flush-scope-vs-gate concern. The full inventory is
the record §7.
