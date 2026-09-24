# The demo fight's pose advance — the state-7 T-rex pose handler (design)

**Context.** Cycle 5 (arena backdrop) closed the demo's state-7 mountain layer:
port 482 ↔ capture 834 is byte-exact and captures 834..839 all match. The demo
oracle's report-only fallback now opens at capture **843 (raw 3750)** — the
first captured frame after the front-end window — where the record §6.3 names
**the T-rex's animation pose** as the residual, owned by the pose
selector/pose family and re-scoped with its size unknown. This cycle closes it.

## Goal

Close the demo's first unexplained frame (capture 843, the T-rex's pose) and
the state-7 divergence behind it, so the port's demo frames explain the
capture's **fight-1 window** — the whole first demo fight — and the first
unexplained content-bearing captured frame moves past it (to the Time Warner
logo at capture 1886).

## Acceptance

1. **The fight-1 window is byte-exact.** The capture's fight-1 window
   `[843..1884]` — **1042 content-bearing frames, no all-black artifact inside**
   — has **0 unexplained** under the oracle's existing model (its
   clean/splice/transition classifications, zero tolerance). The measured witness:
   the demo oracle's first unexplained content-bearing captured frame moves
   from **843** to **1886** (the Time Warner logo; 1885 is the capture's
   all-black artifact). Reported before and after. If the whole window is
   unreachable, the residual is **named with its evidence and its owner** —
   never a fitted number.
2. **Faithful port.** One C function per original, address-tagged, wired at its
   call site. `make verify` exits 0 with every enforced claim unmoved — title
   `54 clean, 55 splice, 2 transition, 0 unexplained` and
   `54 clean, 57 splice, 0`; attract `FIRST DIVERGENCE at capture frame 215`;
   front-end `[560..842]` / 283 / `2 unexplained (832, 833)` allowed by name;
   smk `120/120` + `41/41`; `oracle C-vs-Python: 9866 writes byte-exact`;
   `symbols.h` byte-identical. 0 warnings. A correct fix that necessarily moves
   a claim is absorbed **in the same commit** with its reason recorded (the
   arena-backdrop precedent).
3. **The bounded claim is enforced.** A new gated oracle,
   `make demo-fight-oracle` (in `make verify`), claims **0 unexplained in the
   fight window `[fe_b+1 .. first all-black artifact)`**, where `fe_b` is the
   front-end window's last frame (the same content alignment `--demo` already
   locates) and the window's end is the capture's own all-black artifact run
   (1885). It skips without the capture, like the front-end oracle. The full
   `--demo` window stays **report-only** (its tail — the logo, the title
   screens, fight 2 — is out of scope).
4. **Assertions** mutation-proven, in the existing per-area files.

## Evidence (measured before the design)

- **The divergence is the T-rex's body only.** Capture 843 ↔ port 490: the
  record §6.3's best-splice distance is 16 064 B; a direct compare is
  **16 101 B / 5793 px**, whose bounding box is the T-rex alone
  (`x 0..145`, `y 81..197`). The capture's T-rex is **upright, roaring**
  (blood at the mouth); the port's is **crouched, frozen where the pose entry
  left it** (port frames 483..490 are the same crouch the capture holds through
  841, while the capture rises at 842). Everything else — sky, mountains,
  temple, sea, ground, HUD, text, `CREDITS: 5` — matches.
- **The port enters the pose and freezes.** The suite's `s7_last_change` (the
  last loop frame where either slot's `+0x52` changes) is **1078**; the state-7
  entry is loop 1070/1071. `fighter_pose_start` (`0x39F40`,
  `port/src/game/fighter.c:545`) writes `+0x52=0x10`, `+0x53=0x0A`,
  `+0x10=0x39CC8`, `+0x54=2`; after that the port's `+0x52` never changes
  again.
- **`0x39CC8` — the pose handler — is unported.** `0x3531C`'s case 10
  (`fighter.c:2799`) resolves `slot+0x10` and calls it; `fn_resolve(0x39CC8)`
  returns NULL today, so `+0x58` stays 0 and the `0x39EFC` gate
  (`+0x53 == 0x0A && +0x10 == 0x39CC8 && +0x58 == 4`; ported,
  `fighter.c:4040`) never lets the ported `0x3B714` (`fighter_reaction`) exit
  the pose.
- **The handler's shape (Ghidra, linear addresses).** `0x39CC8..0x39EF4`
  (~556 B): call `0x33A10` (ported) then `0x35050` (**125 B, unported**); a
  5-entry jump table at `0x39CB4` switches on `slot+0x58`:
  case 0 → `+0x58=1`; case 1 → `0x39B30` (**388 B, unported**) then `+0x58=2`;
  case 2 → the `rec+0x36 < 0` gate, `0x39AC8` (**74 B, unported**), a
  `0x2BC30` (`actors_anim_begin`, ported) call; case 3 → `0x186D0`,
  `0x3C16C`, `0x1890C`, `0x188DC`, `0x2AE14` (mentioned/ported), `rec+0x43 =
  0x14`, and `0x2C3FC(0x6C)` (**the voice stub — stays a stub call**, the
  arena-backdrop precedent); case 4 → clear `rec+0x28` bit 0x20 and
  `+0x54=0`.
- **The window and the ratio.** The port's demo frames are `[490..1380]`
  (891); the capture's fight window is `[843..1884]` (1042 content frames) —
  1042/891 = 1.169 ≈ the capture's 70.09 Hz / the game's 60.05 Hz = 1.167. The
  port's dump already covers the whole fight.
- **Rough closure (project method, heuristic).** The four roots
  (`0x39CC8` ~556 B, `0x35050` 125 B, `0x39B30` 388 B, `0x39AC8` 74 B) plus
  their unported callees walk to roughly **~30 functions / ~3 KB** — near the
  gate's 4 KB line. The walk reaches the resource read/gate family (`0x1Exxx`,
  the known "read/gate model" divergence) through `0x39B30`'s
  `0x3C520`/`0x3C148`/`0x3C480` arms. Task 1 measures exactly.

## Scope

- **In:** the state-7 fight path's missing pieces — the pose handler `0x39CC8`
  and its unported callees first, then whatever Task 1's divergence trace finds
  behind it (the pose's exit into `0x3B714`, the raptor's machine, the hits,
  the demo AI's commands, the camera) — if the genuinely-new closure is under
  the size gate.
- **Out:**
  - Any piece whose genuinely-new closure is **≥ ~4 KB or ≥ ~20 new functions**
    — a follow-on cycle, named in Task 1.
  - The demo loop's other content — the Time Warner logo, the title/logo
    screens, fight 2 — the `--demo` tail stays report-only.
  - The state-9 hold's animation (fidelity-gaps §7.6), the loader's presented
    DAC state (§7.11, the front-end's 832/833), the attract-215 presented-DAC
    mechanism (demo-fight-closure §9.5), the interactive match (§7.12), the
    audio gaps (§7.13, the `0x2C3FC` voice calls stay stub calls), `0x38154` —
    each carries its owner and is not touched.
  - Any state-7 divergence Task 1 finds that is **not** the pose chain: named
    and re-scoped with its size, not silently pulled in.

## Architecture — derivation-first, size-gated

**Task 1 — the derivation record** (no porting code). One record,
`docs/superpowers/plans/2026-09-24-demo-pose-derivations.md`. Pins the
divergence by **runtime differential** — a temporary, reverted port trace of
both slots' `+0x52`/`+0x53`/`+0x58`/`+0x10`/`rec+0x52`/cursor over the state-7
window, compared to the original's live RAM under dosbox-x and to the
capture's pixels — plus Ghidra for the raw addresses; derives each missing
piece's body, written fields and call site, the porting plan, the size-gate
computation, the unit-test values, the named gaps, and the provenance. **If the
closure crosses the gate, the cycle stops after Task 1 and names the
follow-on.**

**Tasks 2–N — the port.** Each ports one piece from the record: one C function
per original, address-tagged, wired at the raw's call site, gated by a
raw-derived unit assertion with a mutation proof and the full ladder.

**Task N+1 — the outcome.** Re-measures the fight window and the demo oracle's
first unexplained; adds the gated `--demo-fight` oracle (Acceptance 3); updates
the gap inventory (`README.md`, `port/spec/game_flow.md`, the record's outcome,
this spec) — each gap closed or re-scoped with evidence.

**Homes.** `port/src/game/fighter.c` (the pose family), or wherever the
record's seam lands. SDL and file I/O stay in `host.c`/`main.c`.

**The size gate.** A group whose genuinely-new closure is **≥ ~4 KB or ≥ ~20 new
functions** becomes a follow-on cycle — measured in Task 1, re-scoped **before**
porting.

## Verification

- **The cycle's own gate:** the fight window `[843..1884]` has 0 unexplained,
  and the demo oracle's first unexplained content-bearing frame is 1886 —
  reported before and after.
- **The enforced bounded claim:** `make demo-fight-oracle` (in `make verify`)
  fails on any unexplained frame in the fight window; the window is derived
  from the front-end pass and the capture's all-black artifact.
- **Per task:** a raw-derived unit assertion with a **mutation proof** — the
  assertion must fail under a perturbation of the code it tests — and the
  ported function faithful: one C function per original, address-tagged, per
  `AGENTS.md`.
- **Assertions live in the existing per-area files** (`test_fight.c`,
  `test_game.c`) — no new test file.
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

Branch `demo-pose` off `main`, **in-place** (no worktree — a worktree lacks the
git-ignored `data/`, so the oracles would skip silently), run through
subagent-driven development with a review per task, the ledger at
`.superpowers/sdd/2026-09-24-demo-pose/`. Commit style `<area>: <what changed>`;
never `git add -A`.

## Risks

- **The trajectory may diverge beyond the pose handler.** The capture's
  divergence cascades (by capture 870 the whole scene differs), so Task 1 must
  trace the chain — the pose's exit into `0x3B714`, the raptor's machine, the
  hits, the AI, the camera — and the cycle ports what fits the gate, naming the
  rest.
- **The closure may reach the resource read/gate family** (`0x1Exxx`, the known
  "read/gate model" divergence) through the pose handler's callees and cross
  the gate → the cycle stops after Task 1 (the rule).
- **The voice stub `0x2C3FC(0x6C)`** — if the original's dispatcher consumes RNG
  or writes fight-visible state, the stub could break the alignment; Task 1
  pins this before the port.
- **The fix may move the front-end claim's tail** (port 488–489 ↔ captures
  840–842, currently splice-explained) → absorbed with its reason in the same
  commit (the precedent).
- **The fight window's end is capture-derived** (the first all-black artifact
  after the front-end window). If a later cycle changes what the port exhibits,
  the window is re-derived from the same rule, not hard-coded.
