# Combat/render fidelity — cycle 3 (design)

**Cycle 2** (`docs/superpowers/specs/2026-09-21-demo-fight-closure-design.md`,
plan `docs/superpowers/plans/2026-09-21-demo-fight-closure.md`) merged to `main`
at `a2d7ac3`. It ported the demo fight's closure path and recorded its Gate — the
demo window at **0 unexplained frames** — as **UNMET**, with the residual named
gaps. This cycle closes those residuals.

## Goal

The attract demo fight plays **behaviourally and render-faithfully to its own
end**: the fight runs to the 900-frame timer exit without stalling, and the arena
renders in the DAC banks the raw assigns.

The Gate has two crisp claims, each with a test:

1. **The fight runs to the 900-frame timer exit** (loop 1969) without stalling.
2. **The arena's character palette matches the raw's DAC range**, and the arena
   frame's byte-diff drops to the level the animation poses alone explain.

No threshold is fitted. Claim 2's "the level the poses alone explain" is a
measured quantity (the arena's residual once the palette matches, with the pose
difference identified and named), not a chosen number.

## What cycle 2 left open (the starting state)

Measured, with the evidence in the cycle-2 record:

- The demo window is capture frames `[831..3616]` (raw `3670..8409`), 2786
  frames: **0 clean / 0 splice / 0 transition / 2779 unexplained** (7 all-black
  frames excluded). First unexplained **832** — the lazy loader's `- LOADING -`
  screen.
- **The palette order.** `palette_acquire` (`0x33754`) assigns each palette its
  DAC range by acquisition order (`start = prev.start + prev.len`). The port's
  character palette `0x1BB9FD58` lands at range `start=142`; the original's
  differs, so the T-rex renders in the wrong bank while the backdrop matches.
  The port's `palette_acquire` body is a faithful port of the raw's
  search/reflow, so the divergence is in the **sequence of acquisitions**.
- **The fighter animation poses.** The raptor's silhouette has **IoU 0.522**
  against capture 834 and no port frame matches over 900; no translation
  improves it (best shift `(1,0)`, IoU 0.298; a forced flip is worse). It is a
  different animation/think frame, not a position or a flip. The T-rex's residual
  is 3-channel colour on an aligned silhouette — the palette above.
- **The fight's stall tail.** At loop 1400, s1 lands on `+0x52 = 9` with
  `+0x53 = 8` where the original is at `+0x52 = 4`. Closing it needs the unported
  `0x34B14` handlers (record §7.10/§11.3). The hit chain itself fires and lands
  hits (`+0x7C` 0/0 → 1/1).
- **The arena's residual.** 42667 bytes (22.2%, 15067 px) against capture 834,
  of which the raptor is 24709 and the T-rex 17741. The idle-animation tick
  (`0x37A58`) landed; the residual is the poses plus the palette.
- **831/832 is proven un-derivable** (Tasks 5a/5c): the port produces both held
  states, but the gate passes on the loader frame and the post-read ISR tick
  count is a host/emulator property (two live-RAM polls disagree, Δ=2 vs Δ=3).

## Scope

**In:** the palette acquisition order and the loader flush scope; the twelve
unported `0x34B14` `+0x52` handlers plus `0x349C8`'s sub-handlers; the fighter
animation/think state that selects the arena's sprites.

**Out:** the 831/832 held-frame presentation path. The post-read ISR ticks remain
un-derivable, and no host-timing value is admitted. The palette work may touch
the loader's flush scope (candidate (a) is confirmed) without claiming the
held-frame presentation — 831/832 stays a documented named gap unless the
derivation finds it falls out of the palette fix for free, in which case the
record says so with its evidence.

**Out:** the interactive match — the mode graph, the `0x257A4` coin divert,
`0x1EEB0`, `0x1F458`, the player screens and human input. It remains unowned.

## Architecture and ownership

**No new modules unless the derivation proves a genuinely new subsystem exists.**
The default is to extend the responsible existing owner: the palette order into
`actors.c` (`palette_acquire` and its callers), `platform/res.c` (the loader's
flush scope) and `platform/gfx.c` (the dirty list and `gfx_flush_palette`); the
`0x34B14` handlers into `fighter.c` (the `+0x52` dispatch and the per-fighter
state handlers) and `fight.c` (the health/HUD spine that calls `0x36E2C`); the
poses into whichever of `actors.c`/`fighter.c` already owns the animation/think
tick. The derivation task names each owner and its address *before* any porting,
and if it finds a real new subsystem it says so with the evidence rather than
inventing a module.

State, as always, lives in `mem[]` at its original linear address under its
`symbols.h` name. No long-lived C global shadows original state.

## The work

**Task 1 derives the whole cycle before anything is ported.** It establishes:

- the original's `palette_acquire` call sequence and order (from `0x2BAF4`,
  `0x2C06C`, the actor spawns and the fight's pset sync), versus the port's, and
  the DAC range each gives `0x1BB9FD58` — the exact point the sequences diverge;
- the twelve `0x34B14` handlers (`0x359E0` `+0x52=1`, `0x35C1C`/`0x35D20` `2`,
  `0x35F84` `4`, `0x36430` `5`, `0x399CC` `7`, `0x37464` `8`, `0x361C8` `12`,
  `0x36300` `13`, `0x36710` `17`, `0x33B00` `19`, `0x35E6C` `20`, `0x364FC`
  `21`), plus `0x349C8`'s `0x37178`/`0x37D18`, each body, its callees, and its
  `+0x52`/`+0x53`/`+0x54`/`+0x57` writes — and **their size**;
- the state that selects the raptor's sprite at capture 834 (which field, which
  stream, and where the port's animation/think state diverges);
- whether the loader's flush scope (candidate (a)) is separable from the 831/832
  held-frame gap, and whether fixing it moves the oracle at all.

This is the risk-burner. It bounds the handler tail before the sequence is
committed to, and every later task ports from it rather than re-deriving. On
plan-vs-raw conflict the raw wins, with the correction and its address recorded.

Then port **in the order render → behaviour → poses**, one task each:

1. **the palette order + the loader flush scope** — the render foundation, shared
   with the interactive match;
2. **the `0x34B14` handlers** — the combat tail that closes the stall;
3. **the poses** — the animation/think state.

Task 5 measures the Gate, records the cycle honestly, and confirms every
existing oracle claim is unmoved.

## Verification

**The Gate's two claims are each asserted.** Claim 1 (the fight reaches the timer
exit) is measured by a test at the raw's dispatch point, reporting the last
state-change frame and the timer exit; an env-gated probe is used only if the
test cannot reach the frame, and then the probe is the evidence of record. Claim
2 (the palette's DAC range) is an assertion on the palette table entry's `start`
and `len` for the T-rex's character palette (`0xA8A28[DS_00105B34]`): the
pre-fix variant-0 handle `0x1BB9FD58` and the original's variant-1 handle
`0x1BB9FCD8` land at the same range `start=142 len=31` (record §1.2/§1.3), and
the arena's byte-diff is measured before and after.

**Every existing oracle claim must stay unmoved.** The invariant is the claim, not
the window indices: title `54 clean, 55 splice, 2 transition, 0 unexplained` and
`54 clean, 57 splice, 0 unexplained`; attract `FIRST DIVERGENCE at capture frame
215`; front-end `0 unexplained`; smk `120/120` + `41/41`; the C-vs-Python
byte-exact claim. `make demo-oracle` is report-only. If any index moves, the
record says which and why.

Unit tests follow the existing rules: one `int test_X(void)` per file registered
in `test.h`/`run_tests.c`/`CMakeLists.txt`, only `CHECK`/`CHECK_EQ_INT`,
`game_init()` once per process and env-gated, and **assertions that can fail** —
seeded sentinels that differ from the post-conditions, each new assertion proved
by a mutation of the code it tests. No fitted constants anywhere: every value is
derived from the raw bytes or a capture with the address that proves it, or is a
named gap with its evidence.

## Risks and fallbacks

- **The handler tail's size** is the main risk. Task 1 bounds the twelve handlers
  and their callees before the sequence is committed to. If the closure is
  materially larger than a cycle, that is a re-scope conversation with the human,
  not a silent overrun.
- **The poses may be un-derivable.** If Task 1 cannot pin which state selects the
  sprite — or pins it but the port's animation/think state has no single
  derivable divergence — that is a **named gap with its evidence**, never a
  fitted value.
- **The palette fix may be a call-order change, not a code change.** The port's
  `palette_acquire` body is faithful; the divergence is the acquisition sequence.
  The fix therefore belongs at the callers, and Task 1 names them. If the
  divergence turns out to be a resource-identity difference instead (the port
  resolving a different handle), that is the same shape and is recorded.
- **The Gate's second claim depends on a measurement that does not exist yet.**
  "The level the poses alone explain" is measured by Task 5 after the palette and
  the poses land. If the poses prove un-derivable, the residual is reported with
  the pose named as a gap and the Gate is assessed honestly — the **Gate** is not
  declared met, and no pin or value is fitted to force it. (Claim 2's palette
  core is independent of this: it is MET with the pose residual named — see the
  Outcome.)
- **The window may still not read clean.** It will not: 831/832 stays
  un-derivable. This cycle's Gate is not the window's count; the record says so
  explicitly so the artifact cannot read as though the demo window were closed.

## Open questions for the derivation

These are Task 1's to answer with evidence, not assumptions to carry:

1. What is the original's `palette_acquire` call sequence, and at which call does
   it diverge from the port's — and what DAC range does `0x1BB9FD58` get in each?
2. What do the twelve `0x34B14` handlers do, how large is their closure, and which
   of them returns `+0x52` from 9 to 4 (closing the loop-1400 stall)?
3. What selects the raptor's sprite at capture 834, and is the port's divergence a
   single derivable state (a field, a tick, or a stream) or a subsystem?
4. Is the loader's flush scope (candidate (a)) separable from the 831/832
   held-frame gap, and does fixing it move the demo oracle at all?
5. Does the `0x34B14` tail consume RNG, and if so does it need a determinism
   answer before it can be ported?

## Outcome (recorded)

**The Gate is UNMET overall.** Its second claim holds; its first does not.

**Claim 2 — MET.** The T-rex's character palette is acquired at the raw's DAC
range: handle `0x1BB9FCD8` (variant 1) at **`start=142 len=31`** (the port's
ownership-table entry 6 at HEAD; the original's table, record §1.2, is identical
in order and range). The arena's byte-diff against capture 834 fell **42 667 B
(22.2 %, 15 067 px) → 18 294 B (9.5 %, 6 194 px)**; the T-rex region
6 363 → 1 773 px, the raptor region 8 704 → 4 421 px. The raptor's teal-mask
silhouette IoU rose **0.522 → 1.000** (frame 482; 0.974 at 483). The residual is
the pose state (claim 1's gap), named — not fitted.

**Claim 1 — UNMET.** The fight **reaches** the 900-frame timer exit
(`s7_last == 1969`, asserted in `port/tests/test_frontend.c`), but it does not
run to it without stalling: the slot state machine's last `+0x52` change is loop
frame **1072**, and the original's pose state `0x10`/`0x0A` is never entered
(`s7_saw10`/`s7_saw0a` both 0). The cause is the unported
`0x19020 → 0x193B0 → 0x3B714 → 0x3AAFC →` pose-family chain: `0x19020` is
unported, so `DS_00100AF8`/`AFC` stay 0 and `fighter_pass_a`'s tail never runs
(record §10). Its closure is **68 new functions / 10 467 B (true new
≥ 11 012 B)** — ~2.4× a task — so Task 4 landed no port and the subsystem is
handed to **cycle 4**.

**What the cycle landed.** The front-end dump driver's palette-variant seed
(`DS_0010816A[1]` `0xFF` → `0`; no engine change — `0x41350` is faithful); seven
faithful `+0x52` handlers (`0x35F84`/`0x36430`/`0x399CC`/`0x361C8`/`0x36300`/
`0x36710`/`0x364FC`) and the `0x3C148`/`0x3C16C` clears; the state-7 entry's four
missing RNG draws (the type-0 effect handler `0x4AAD0`/`0x4B144` and its wiring —
the entry LCG now reaches `0x10F7DB07`, matching); and the demo-AI block state
(`0x18540`/`0x18350` — the command words now match: `cmd0@1072 = 0x4848`,
`cmd1@1071 = 0x0002`). The loader flush scope got **no engine change**: the
record (§4) shows no faithful standalone change.

**Named gaps carried out of the cycle.** 831/832's held-frame presentation (the
post-read ISR ticks, a host property — un-derivable, so excluded, not fitted);
the unported `0x36870` 9→4 closer and `0x37178`/`0x37D18`; the five unported
`+0x52` handlers (`0x359E0`, `0x35C1C`/`0x35D20`, `0x37464`, `0x33B00`,
`0x35E6C`); and the `0x19020`/`0x3Fxxx` freeze subsystem (cycle 4).

**The interactive match remains UNOWNED** — the mode graph, the `0x257A4` coin
divert, `0x1EEB0`, `0x1F458`, the player screens and human input (the Scope "Out"
above stands).

**Every enforced oracle claim is unmoved**: title `54 clean, 55 splice, 2
transition, 0 unexplained` and `54 clean, 57 splice, 0 unexplained`; attract
`FIRST DIVERGENCE at capture frame 215`; front-end `[560..830]` / 271 frames
`0 unexplained`; smk `120/120` + `41/41`; the C-vs-Python `9866 writes`
byte-exact claim. `make verify` exits 0 with 0 warnings.
