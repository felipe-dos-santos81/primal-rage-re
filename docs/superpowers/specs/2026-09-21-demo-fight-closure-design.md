# Demo fight, cycle 2 — closure (design)

**Cycle 1** (`docs/superpowers/specs/2026-09-20-demo-fight-design.md`, plan
`docs/superpowers/plans/2026-09-20-demo-fight-motion.md`) merged to `main` at
`c75e55c`. It ported the attract demo fight's motion and render path, and the
demo now advances, spawns fighters and animates. Its Gate — a report-only pixel
oracle proving the demo window clean up to the frame the original first lands a
hit — is **unmet by construction**, and its record says so. This cycle closes
it.

## Goal

The attract demo fight plays **end-to-end and pixel-faithfully**: `make
demo-oracle` reports the demo window with **0 unexplained frames**, from its
opening frame through the 900-frame fight, the timer exit and the continue
sequence.

The per-task measurable invariant is the oracle's **first-unexplained frame**. It
must advance monotonically toward the window's end. That is the progress metric
cycle 1 lacked: its window diverged at its first frame, so its artifact read
identically whether the ported code was correct or broken.

## What cycle 1 left open (the starting state)

Measured, with the evidence in the cycle-1 record:

- The demo window is capture frames `[811..3759]` (2949 frames), and
  `make demo-oracle` reports **0 clean / 0 splice / 0 transition / 2942
  unexplained**, first unexplained **811**.
- **Frame 811 is the state-9 hold's globe render**, not a landing hit. The
  port's state-9 output does not match the original's. The capture's demo fight
  does not begin until capture **836** (25 frames after the divergence).
- The port's **state-7 arena render** is also incomplete — the `0x3C88C` gap,
  measured as a broken globe. Re-anchoring the window at the state-6/7 entry
  (dumped 481) still yields 0 exhibited frames, so the window cannot be made
  discriminating by moving its start.
- The fight **stalls** at the unported `0x3CF38` hit-detection chain, which
  cycle 1 declared a cycle-2 gap.
- The state-6 determinism pins are **fitted to the port**: they replace
  `call 0x5D7DC` with `mov eax,4` (the port's own LCG output), so no oracle
  result can support the claim that the port's state-6 RNG handling is faithful.
  Worse, the `call`→`mov` pin does not advance the reference's LCG while the
  port's `rng_next(7)`/`rng_next(6)` do, so the pinned capture's stream is
  **offset by two draws** after state 6.
- Task 4's think chain (`fighter_think` → `0x3B464` → `0x3B298` →
  `fight_command_map`) is **dead for the demo's slots**: every writer of
  `slot+0x64` in the image sets `0xFF`, so it is dead in the raw too, not merely
  in the port. The demo's AI comes entirely from the command generator
  (`0x47208`).

## Scope

**In:** the state-9 globe render; the state-7 arena render gap (`0x3C88C`); the
`0x3CF38` hit chain (collision, damage, health, hit reactions); the timer exit
and continue sequence; the RNG nondeterminism's source; and marking the think
chain unexercised.

**Out:** the interactive match — the mode graph, the `0x257A4` coin divert,
`0x1EEB0`, `0x1F458`, the player screens and the human input mapping — and
anything past the demo window. The dead think chain stays in the tree, marked,
owned by the interactive match.

**Removed from this cycle:** window re-anchoring. Porting state 9's render makes
the window's opening frame clean, so no window change, no new reference and no
re-anchoring task. Cycle 1's spec named re-anchoring as cycle 2's; this
supersedes it, and the reason is recorded rather than the task silently dropped.

## Architecture and ownership

**No new modules unless the derivation proves a genuinely new subsystem exists.**
The default is to extend the responsible existing owner: the hit chain into
`fight.c` (the arena frame and the HUD/health path) and `fighter.c` (per-fighter
damage and reactions); the globe render into whichever existing pass already
produces that display list. The derivation task names each owner and its address
*before* any porting, and if it finds a real new subsystem it says so with the
evidence rather than inventing a module.

State, as always, lives in `mem[]` at its original linear address under its
`symbols.h` name. No long-lived C global shadows original state.

## The work

**Task 1 derives the whole cycle before anything is ported.** It establishes:

- the state-9 globe render, its producer and its display-list path;
- the `0x3C88C` gap and what state-7 arena render is missing;
- the full `0x3CF38` hit chain, its callees, and **its size**;
- the timer exit and the continue sequence;
- the source of the RNG nondeterminism at the state-6 picks.

This is the risk-burner. It bounds the hit chain before the sequence is
committed to, and every later task ports from it rather than re-deriving. It also
corrects two known record errors it re-reads that ground for anyway: Amendment
5's capture numbers (it says 839/28; the correct post-recapture values are
**836/25**) and the plan's stale front-end window figures (`[557..813]`/257;
reality `[557..810]`/254).

Then port **in frame order**, one task each, so the first-unexplained boundary
moves:

1. **state 9's globe** — the window's opening frame;
2. **the state-7 arena render** — the `0x3C88C` gap;
3. **the hit chain** — collision, damage, health, reactions;
4. **the timer exit and continue sequence** — the window's end.

The think-chain marker rides along with the derivation task's record update: a
`/* PORT: ... */` note and a record entry stating it is unexercised by the demo,
with the `slot+0x64` evidence, owned by the interactive match. Its unit tests
remain its only evidence, and the record says so.

## Verification

**The oracle is the gate.** Each task must advance the first-unexplained frame
and must not move any existing oracle's claim.

The RNG work replaces the state-6 constant pins with a **source-level pin** —
pinning the nondeterminism where it originates (the seed, or an earlier
uninitialised or timing-dependent read) so the whole stream stays aligned and the
port's own LCG carries every downstream draw. The oracle then *validates* the
state-6 picks instead of being forced to them, and the two-draw offset
disappears rather than being papered over.

**Outcome (Task 2, recorded).** The source-level pin landed: the two master-loop
draws `0x256B1`/`0x256D6` are pinned to a non-advancing `mov eax,0` and the
port's `game_loop` stopped drawing at the body site; the reference is now
deterministic and the oracle claims held after the re-capture (the derived
indices moved). **The pin alone does not align the streams**, and Task 2's gate
is therefore unmet: the reference's state-6 entry is the seed + 26 (the attract's
voice-tick draws, which the front-end driver skips by entering at state 2) and
the port is a further six draws behind inside state 6 (the dust builder
`0x494A8` draws three values per loop iteration between `0x11AAD` and `0x11AE9`;
`fighter_spawn_slot` skips it). The capture's picks (`0`/`3`) and the port's
(`0`/`6`) confirm both offsets. The fix is a human decision: reproduce the
attract's draws in the driver (or re-seed to its post-state) and issue the dust
builder's draws in the port, or pin the attract's three draw sites too.
`port/spec/game_flow.md` carries the evidence and addresses.

**The invariant is the oracle's claim, not its window indices.** A capture is
host-timed and provably unstable — repeat capture of the same tree yielded 587 vs
588 distinct frames — so the window's derived indices move whenever the reference
is regenerated, as they did when cycle 1's pins landed (`257`→`254` frames).
Cycle 1's "oracles that must not move" wording conflated the claim with the
indices and cost real confusion. Here the claim (`0 unexplained`, and the
attract/title/smk classifications) is what must not move; the indices are
reported as derived.

Reworking the pins **will** change the reference's output once more, so expect
one more recorded reference change. It is a consequence of the pin fix, not a
regression, and the record must say which numbers moved and why.

Unit tests follow the existing rules: one `int test_X(void)` per file registered
in `test.h`/`run_tests.c`/`CMakeLists.txt`, only `CHECK`/`CHECK_EQ_INT`,
`game_init()` once per process and env-gated, and **assertions that can fail** —
seeded sentinels that differ from the post-conditions, each new assertion proved
by a mutation of the code it tests. No fitted constants anywhere: every value is
derived from the raw bytes or a capture with the address that proves it, or is a
named gap with its evidence. On plan-vs-raw conflict the raw wins, with the
correction and its address recorded.

## Risks and fallbacks

- **The hit chain's size** is the main risk. Task 1 bounds it before the sequence
  is committed to. If it is far larger than assumed, that is a re-scope
  conversation, not a silent overrun.
- **The state-9 globe's owner may be unclear.** If it cannot be pinned to an
  existing pass, that is an escalation with evidence, not an invented module.
- **The RNG source may not be pinnable.** Source-fixing was chosen over extending
  the pin mechanism. If Task 1 shows the nondeterminism has no single pinnable
  origin, that is an escalation back to the human rather than a quiet fallback to
  the advance-and-overwrite pin.
- **The source pin may not align the streams (realized in Task 2).** The pin
  removes the master loop's host-timed draws, but the oracle's comparison path
  differs from the reference's: the front-end driver skips the attract (26
  draws) and the port skips the dust builder's draws (six per spawn). Task 2
  reported this with the evidence instead of pinning the consumption sites; the
  alignment fix is a human decision (`port/spec/game_flow.md`, "Cycle 2 replaced
  the two state-6 pins…").
- **The fight may not converge on a first try.** The hit chain's RNG consumption
  is new and may itself need a determinism answer. Task 1's derivation covers it;
  if a new nondeterministic site appears mid-cycle it is handled the same way —
  derived, then pinned at its source, never fitted.
- **The window may still not read clean when all four porting tasks land.** If it
  does not, the residual frames are reported as named gaps with their evidence
  and the Gate is assessed honestly — it is not declared met, and no pin or value
  is fitted to force it. Cycle 1's failure was not that its Gate went unmet; it
  was that the record nearly read as though it had.

## Open questions for the derivation

These are Task 1's to answer with evidence, not assumptions to carry:

1. What produces the state-9 globe, and does the port reach it at the right
   frame with the right state?
2. What exactly is missing at `0x3C88C`, and is the state-7 arena render
   otherwise complete?
3. How large is the `0x3CF38` chain, and does it consume RNG?
4. What does the timer exit run, and what does the continue sequence do?
5. Where does the state-6 RNG nondeterminism originate, and is it pinnable at
   that single origin?
