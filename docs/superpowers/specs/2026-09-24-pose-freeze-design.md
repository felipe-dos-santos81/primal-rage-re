# The pose/freeze subsystem — the demo's pose entry (design)

**Context.** Cycle 3 measured the demo fight's freeze and named its owner: the
port's `fighter_pass_a` (`0x1958C`) never runs its winner tail because
`DS_00100AF8`/`AFC` stay 0, so `0x193B0` → `0x3B714` → `0x3AAFC` → the pose family
never runs and the fighters hold at the 9/8 entry (cycle-3 record §10). Cycle 3's
size gate routed the closure to this cycle (68 new funcs / 13 131 B), and the
demo oracle's `res is None` was re-scoped here as the freeze's symptom
(cycle-3 record §6.2, fidelity-gaps record §7.9).

## Goal

Port the freeze's remaining layer so the demo's pose state runs, and move the
demo observable.

## Acceptance

1. **Faithful port.** One C function per original, address-tagged, wired at its
   call site. `make verify` exits 0 with every enforced claim unmoved — title
   `54 clean, 55 splice, 2 transition, 0 unexplained` and
   `54 clean, 57 splice, 0 unexplained`; attract `FIRST DIVERGENCE at capture
   frame 215`; front-end `[560..830]`/271 `0 unexplained`; smk `120/120` +
   `41/41`; `oracle C-vs-Python: 9866 writes byte-exact`; `symbols.h`
   byte-identical. 0 warnings.
2. **The demo observable.** The demo oracle leaves the `res is None` fallback
   (`tools/title_compare.py:484`) and reports a real clean/splice/unexplained
   breakdown. Concrete witness: the port's fight frames byte-match the capture —
   today port 482 ↔ capture 834 at an 18 294-B diff (9.5 %), entirely the two
   fighters (left 1 549 px + right 4 645 px). The first unexplained is **recorded
   with its evidence**, not asserted to advance: it may remain the 831/832 entry,
   whose held-frame presentation is un-derivable and out of scope. The later
   capture frames stay unexplained — the port dump is bounded by state 7's
   900-frame timer (`test_frontend.c:738`, dump 0..1380) while the oracle window
   is `[831..3616]` — and are **named as such**, not treated as a failure.
3. **Assertions** mutation-proven, in the existing per-area files.

## Scope

The union of the two halves cycle-3's record §10.4 measured (the corrected
figures): **65 new funcs / 11 020 B** by the record's named-comment heuristic,
**68 / 13 131 B** counting the three named-but-unported roots.

| half | roots | new |
|---|---|---|
| unfreeze | `0x140E4` (388 B), `0x170A0` (1 248 B) | 32 / 5 332 B |
| pose-entry | `0x193B0` (475 B), `0x3B714` (449 B), `0x3AAFC` (667 B), `0x3A79C` | 53 / 7 607 B |

**In:** the union; plus `0x18950` (152 B, 0 callees) **if** the record shows its
two queries at `fighter.c:328` gate the demo's winner — they decide which side's
`AF8`/`AFC` survives, hence which side runs `0x193B0`.

**Out:**

- `0x19020` (70 B) — the corrected union excludes it: its `AF8` writes are
  guarded by `slot+0x18`, which the demo never sets, so it writes nothing.
- `0x38154` (368 B) — the `S+0x54 == 5` arm of `0x36638`/`0x385B0`; no in-scope
  path reaches it (cycle-3 record §6; `fighter.c:1248`).
- The `0x3Fxxx` closer script (`0x3FD30`/`0x3FF08`/`0x3FFDC`) — never runs.
- 831/832's held-frame presentation (un-derivable; a host property).
- Any blocker whose genuinely-new closure exceeds the size gate (≥ ~4 KB or
  ≥ ~20 new funcs) — a follow-on cycle.

## Architecture — five tasks

**Task 1 — the derivation record** (no porting code). One record,
`docs/superpowers/plans/2026-09-24-pose-freeze-derivations.md`, in the cycle-3
shape: per half, the owner chain, each function's body/callees/written fields,
the values with their proving addresses, the closure size against the size gate,
the porting plan, and the unit-test values. Plus three resolutions:

- **The winner gate** — whether `0x18950(0,1)`/`(1,0)` returns non-zero on the
  demo's frames (the character-table bits it reads), and whether the port's
  take-both-as-0 assumption at `fighter.c:328` changes the winner. If it does,
  `0x18950` is in-scope (Task 4).
- **The `0x19020` no-op** — confirm from the raw that `slot+0x18 == 0` on the
  demo's frames, so the current stub stays a named gap.
- **The measurement plan** — how Task 5 establishes the 482/834 match and the
  oracle's fallback exit.

**Task 2 — the unfreeze half.** Port `0x140E4` + `0x170A0` + their new callees
(one C function per original, address-tagged), wired into `camera_decay` at
`camera.c:373`. Assertion: seed `DS_00100B54`/`B60`/`B61`, call `camera_decay`,
assert `AF8`/`AFC` per the raw. Mutation proof. Ladder.

**Task 3 — the pose-entry chain.** Port `0x193B0` + `0x3B714` + `0x3AAFC` + the
pose family (`0x3A504`/`0x3A650`/`0x3A79C`/`0x3A8E8`) + their new callees, wired
into `fighter_pass_a`'s tail at `fighter.c:364`. Assertion: seed the winner flag
and the slot, call `fighter_pass_a`, assert the pose state (`+0x52`/`+0x54`).
Mutation proof. Ladder.

**Task 4 — the winner gate** (conditional on Task 1's resolution). Port `0x18950`
(+ any other stub the record names), wired at `fighter.c:328`. Assertion: seed the
character table, assert the query's return. Mutation proof. Ladder. If Task 1
finds it does not gate the demo, this task is dropped and the stub stays a named
gap.

**Task 5 — the demo measurement, the record and the docs.** Measure
`make demo-oracle` and the 482/834 diff; assert the observable; update the gap
inventory (`README.md`, `port/spec/game_flow.md`, the record's outcome) — each
gap closed or re-scoped with evidence, the residual divergence (if any) named
with its owner. The full ladder.

**Homes.** The unfreeze half → `port/src/game/camera.c` (the `camera_decay`
chain, which uses camera.c's projection helpers `0x17EEC`/`0x1B544`). The pose
half + `0x18950` → `port/src/game/fighter.c` (tightly coupled to its statics;
`fighter.c` is already 3 426 lines, so a new `pose.c` is reconsidered only if the
record finds a clean seam). SDL and file I/O stay in `host.c`/`main.c`.

**The size gate:** a group whose genuinely-new closure is **≥ ~4 KB or ≥ ~20 new
functions** becomes a follow-on cycle — measured in Task 1, re-scoped **before**
porting. The union is already 68 / 13 131 B, so the gate's purpose here is to
catch a *further* expansion (the winner gate, or a half that grows past the
record's measurement).

## Verification

- **Per task:** a raw-derived unit assertion with a **mutation proof** — the
  assertion must fail under a perturbation of the code it tests — and the ported
  function faithful: one C function per original, address-tagged, per `AGENTS.md`.
- **Assertions live in the existing per-area files** (`test_fight.c`,
  `test_frontend.c`) — no new `test_gaps.c`.
- **Per-task ladder:** `cmake --build build && ./build/run_tests && make verify`.
- **Task 5 measures the observable:** `make demo-oracle` must leave the
  `res is None` fallback; the port's fight frames byte-match the capture
  (witness: port 482 ↔ capture 834). It is a **report**, not a gate — the demo
  oracle is report-only and is not promoted this cycle.
- **If a correct fix moves an enforced claim:** the task **halts** and reports the
  old value, the new value, and the raw evidence that the new one is more
  faithful. If justified, the claim is updated **in the same commit** with the
  reason recorded. The default is no move.
- **Never ship a fitted constant.** Every value is derived from the raw bytes or
  a capture, with the address that proves it. A value that cannot be pinned is a
  **named gap with its evidence**.
- **On any plan-vs-raw conflict the raw wins** — record the correction and its
  address.
- **The record's gap list** ends with every gap closed or re-scoped with
  evidence — no gap silently dropped, no claim stronger than its evidence.

## Workflow

Branch `pose-freeze` off `main`, **in-place** (no worktree — a worktree lacks the
git-ignored `data/`, so the oracles would skip silently), run through
subagent-driven development with a review per task, the ledger at
`.superpowers/sdd/2026-09-24-pose-freeze/`. Commit style `<area>: <what
changed>`; never `git add -A`.

## Risks

- **The observable may not move as far as the freeze.** The 482/834 diff is
  entirely the fighters, which is strong evidence the freeze is the only
  divergence in that frame — but the path carries other stubs (`0x18950`,
  `0x38154`). Task 1 resolves the winner gate; if a blocking stub's closure
  exceeds the size gate it becomes a follow-on and the observable is reported
  short with its evidence.
- **The union may be larger than measured.** The named-comment heuristic
  undercounts (cycle-3 record §10.4's caveat); Task 1 re-measures true-new before
  porting.
- **The two halves are coupled.** Neither alone moves the observable (`AF8` gates
  the tail; `0x193B0` is what the tail runs), so a mid-cycle session death costs
  progress but not correctness — each task's gate stands alone.
- **The enforced oracles should not move** (the freeze is after the front-end
  window), but the front-end window's held frames (port 313..480 = capture 830)
  share the dump; a timing change would show there first.

## Outcome (Tasks 5–6)

All six tasks landed. **The demo observable did not move** — acceptance point 2
is **UNMET**, and its residual is named with its owner. Record:
`../plans/2026-09-24-pose-freeze-derivations.md` §10 and §11.

* **Acceptance 1 (faithful port) — MET.** The two halves (Tasks 2–3), `0x18950`
  (Task 4) and the `0x17FA0` page-flag tail (Task 6) are ported one C function
  per original, address-tagged, wired at their call sites; assertions
  mutation-proven in `test_fight.c`. `make verify` exits 0 with 0 warnings and
  every enforced claim unmoved: title `54 clean, 55 splice, 2 transition, 0
  unexplained` and `54 clean, 57 splice, 0`; attract `FIRST DIVERGENCE at capture
  frame 215`; front-end `[560..830]`/271 `0 unexplained`; smk `120/120` +
  `41/41`; `oracle C-vs-Python: 9866 writes byte-exact`; `symbols.h`
  byte-identical.
* **Acceptance 2 (the demo observable) — UNMET.** `make demo-oracle` still takes
  the `res is None` fallback (`tools/title_compare.py:484`): `0/1068` port frames
  exhibited, `0 clean / 0 splice / 0 transition / 2779 unexplained`, first
  unexplained capture 832 (raw 3671). The 482/834 witness is unchanged at
  **18 294 B (9.5 %) / 6 194 px** (left 1 549 + right 4 645). **Task 6
  (`bfd22cb`) ported the residual `0x17FA0` page-flag/visibility tail**
  (`0x16AFC` 601 B + `0x164F4` 547 B, plus `0x16734`/`0x164C0`): it now fires
  (`camera_page_tail_b(1)`, ~18 state-7 frames), `B61 = 1`, `camera_unfreeze(1)`
  writes `AF8[1] = 5/3/2`, `0x193B0` runs, and **892 port frames change from
  frame 489**. The fighters now byte-match at 482/834; the remaining residual is
  the **arena backdrop** (a missing mountain silhouette) in `platform/render.c`
  (`0x38730`/`0x387F4`/`0x38890`/`0x38A38`). **Owner:** the demo-fight-closure
  subsystem. Record §7.11, §11.
* **The winner gate (Task 4).** The record's odd-record dword `0x0000FF00` was
  corrected to `0x00FF0000`: both demo queries return 0, the position compare
  picks side 0 (T-rex) — matching cycle-3 §10.2 — and the §7.1 "conflict" is
  resolved. `0x18950` does not gate the demo; the port is a faithfulness fix.
* **The size gate.** The union (68 f / 13 131 B) was already over the gate and
  ratified as this cycle's scope; the winner gate (1 f / 152 B) and the Task-6
  tail (4 f / 2 154 B) are under it; no group grew past the record's measurement.
  No follow-on cycle is created.
* **Claim policy.** No enforced claim moved, so the claim-move clause is not
  exercised.
