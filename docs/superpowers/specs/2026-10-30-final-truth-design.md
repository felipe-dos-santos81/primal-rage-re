# Final Truth — design

**Status:** proposed, not implemented. Date: 2026-10-30.
**Predecessor:** Live Truth (`docs/superpowers/specs/2026-10-29-live-truth-design.md`).
**Plan:** `docs/superpowers/plans/2026-10-30-final-truth.md`.

## 1. Where the project stands

The port is complete on its declared target: **759/759 portable functions**
(798/1203 raw, 66%; the remainder is 53 host-owned/deferred, 37 replaced
runtime, 315 runtime). The 2026-10-29 Live Truth phase published the last
live captures: the KO-branch draw, the no-poke two-human KO, the continue
screen/countdown/expiry, the name-entry oracle, and the 74 audio matched
prefixes. The capture inventory is **41/42 present**, with only U14's fought
live win declared absent. The differential census is **1033/1033 functions
VERIFIED; 14993/14993 mutants detected; 0 named gaps; 825/845 rows with
callees closed; 809/845 full; 8 unrowed**; the coverage census is **HIT 657 /
NOT-HIT 98 / UNINSTRUMENTED 4**.

What is left is not porting. It is the **live-gap layer**: a small number of
real behavioural/fidelity differences, a few unexhibited content branches, and
a set of proof/model limits that are currently declared in prose but not
collected in one terminal claim. The Live Truth Task 13 gate is still the
outstanding gate debt.

## 2. Goal and success criterion

**Goal:** close the last real behavioural gaps, and convert every remaining
named gap into either a gate-enforced claim or a terminal declaration with a
measured blocker. No new portable functions; no fitted constants; no
weakening of an existing pin.

**Success criterion:** one authoritative terminal claim register in which
every remaining gap has a measured status — closed by a fix/exhibit/oracle, or
declared terminal with its blocker and evidence — and two consecutive full
gates plus `make audio-render` are green with every existing capture path,
counter and measured pin unchanged except where a task re-measured it and
recorded why. A failed capture is preserved as `.failed`, publishes no pins,
and leaves its row declared.

## 3. What the design investigation measured

The investigation was read-only (no captures, no builds). The citations are
the authoritative files; hypotheses are labelled.

### 3.1 U14 / `b0c` — the rule is unsatisfiable, and the blocker is not the ending

`DS_00104B0C` has one non-zero writer, `0x37FF6` in `0x37EA0` (the `0xD100`
opcode `0x11`), and its consumer is `game_mode_0d_step` (`0x274FC`,
`port/src/game/flow.c:2048-2067`). Mode `0x0D` is entered from the **final's**
seven-opponent arena (`flow_arena_ko_check` `0x272DC` via `game_mode_0c_step`
`0x27380`; `game_flow.md:601-604`), not from match 1's win, which runs
`0x09 → 0x17 → 0x12`. The previous design's "ending path" wording is
therefore a plan-vs-raw correction: the setter is the final's `0x0C→0x0D`
chain.

The `gp-u14-win-live.failed` artifact is a fought arcade KO only: it has
9,471 S rows, zero `b0c=1`, no mode `0x0C/0x0D/0x0F`, and four session-check
failures (end frame, port script, unscripted input, stopped at end). It is
game evidence, not a capture. A match-1 fought win can never pass
`live_death` as written, however well it is played. The honest closure is:
(a) correct the rule and the docs, (b) publish a **two-human fought match
win** — `gp-twop-ko` already produced a round-1 no-poke KO of side 1 and a
round-2 extension is the practical route — and (c) declare U14's arcade
fought win (R1) terminal with the measured damage race (the AI landed 10 hits
to the script's 2 in round 1; 9 to 8 in round 2) and the `0x0C→0x0D` chain.

### 3.2 0x21 — the eight-trample chain and the `+0x1C` re-hit question

The "worshipper trample" is an **effects entry** (type 0, `fight_dust_build`
`0x494A8`; actor `0x2AE14`), not a player move. `fight_4b69c` (`0x4B69C`)
increments that entry's own `+0x1F` when its `+0x1C` bit 7 is set and the
actor hits a fighter; the entry resets `+0x1F` on spawn/rebuild
(`0x49622`, `0x4D061`, `0x4E4E8`) and on landing (`0x49FAF`). Each counted
hit clears bit 7 (`0x4B544`), sets `+0x36=0x240`, and bit 7 returns only in
case 6 while `+0x36<0` (`0x49F16/0x49F18`). The candidate structural blocker
is that `0x4B470` never restores the actor's height `+0x1C`; the only writers
are the grab arms (`0x4B8E8`, `0x4DA19`), whose type-8 release tramples
without counting. If that is true, the counter can only reach 1 per spawn and
the gate is unreachable; this must be derived from the animation stream
interpreter before any capture. The stage word (`DS_00104AFC`) is the
two-human `rng_next(7)` at `0x2587C` (1-in-7 per match); the gate also
requires `b1d∉{2,3}`, `DS_00104AD8<=0`, `DS_00104ABC>=2` and
`fight_13134==0`. A poked 1P exhibit is barred by state-coverage §9; a poked
two-human exhibit would need a new operator reversal, not this phase.

### 3.3 0x25 — the one-sided route is dead; the residue is the count window

`fighter_45158` (`0x45158`) uses `ctx[3]` as the **other** slot and increments
only when that slot's `+0x5F==0x22` and `+0x7A==4`. The increment hook is
installed only by that slot's own `0x455A0`, which also writes that slot's
`+0x53=7` at `0x455C3`. A one-sided route (P2 performs, P1 idle) therefore
produces **zero** increments; the prior symmetric route's symmetry is implied
by the raw. The `+0x53` clear in `0x367DC` (`fighter.c:3680-3695`) fires at
the reaction's ~91st frame when `+0x88>90`; because `+0x53==7` for frames
1..90, the only frame where the entry gate's `+0x53==0` and an increment can
coexist is the clear frame itself, and `b892` must already be ≥6 there. The
first real-input attempt reached 2; the corrected attempt sampled 5 as its
last value. The count, not the `+0x53` gate, is the remaining blocker. The
camera word `DS_000F0AF0`
(|x|≤0x3300) is the one gate input never sampled. A staggered route (4–5
attempts, 150-frame spacing) is a new, unmeasured task; one authorized run
with `s0_53`/`camx` S fields would settle whether the count or the camera is
the residue. Without that run, the declared gap is the count/camera residue,
not "structurally impossible."

### 3.4 5120 — the index bytes differ, and the experiment is decidable

The port frame at f=1826 holds indices `0x32`×8, `0x2E`×1, `0x4D`×4 (DAC
`000000`/`100000`/`00000c`); capture 5120 holds `00001c`×8 + black×5. The
capture's 8 pixels are blue, which no DAC state of the port's `0x32` red-only
fade trajectory can produce (`5100-5108` fade `140000→0c0000→040000→000000`).
This is a **different presented index state**, not a fade/palette timing
offset. A per-pixel/per-row holder search over the existing U13 dump can
decide whether any port frame holds those exact 13 values; if none does, the
DAC-injection diagnostic (56 derived W writes over the 24 active palette
records, sentinels for `0x65/0x32/0x4D/0x2E`, back-buffer reads at `0xE87A0`
and `0xE87A4`) distinguishes PASS-A (capture/pipeline present model) from
PASS-B (content divergence with a nameable renderer). The fade source is
pinned through `0x29B74`/`0x13D4C`/`0x134C0`; the specific list entry for
`0x32` is not yet pinned.

### 3.5 Audio — the game-owned claim is the register stream, not the PCM

Exact WAV equality is structurally impossible: the port vendors Opal 2.0.3
and mixes at 49,716 Hz stereo with a Q8/4-voice mixer; DOSBox-X runs its own
OPL core at 48 kHz with DC-bias correction and a different voice allocation.
The `.dro` traces already prove the original's register writes, and the 74
diff-allowed prefixes/29,656 ticks pin the matched leading runs. A feature
oracle (onsets/pitch) would validate Opal/DOSBox-X, not Primal Rage. The
honest outcome is a terminal declaration of rendered audibility, plus (if
cheap) a narrow **register-to-render** test that compares the port's OPL stem
to `opl_render` of `PR_AUDIO_DUMP` — determinism, not audibility.

The 10 requested-unclaimed banks are all already present in existing traces;
the whole-section mismatch is the driver's voice-rotation/tail model, not a
missing capture. The 4 never-requested banks: `grave+0x0A8C` has no static
request site (evidence of absence), `grave+0x08` is the fought/final path
(U14 R1), `sound+0x7714` is gated by the `+0x42`/`+0x43` pose (true for 99
sampled frames in `gp-u24-u16-state-t9-r5`, which can be replayed without a
capture), and `konsd+0x1CCB0` is gated by the `0x24508` stream target. A
no-capture residue table (bank → first-diff tick and cause class) is the
smallest defensible closure for the 10.

### 3.6 U17, taken-continue, movie audio

U17's wall-paced walk has no tick in its I records; a tick-keyed replay would
need a new capture with a `tick=` column (the K11 driver is the precedent).
Its screens are already covered by K11, and the device-label band and clear
are pinned capture-side; the unique gain is binding the label to a port
frame. This is terminal for the phase (the tick-keyed replay is documented as
a closable-but-low-value option).

The taken-continue branch is a genuine small gap: `gp-u28-continue` proves
expiry only. `flow_continue_poll` returns 1 with a credit and a newly pressed
start, sets `b1f=side+1`, spends the credit, and stores mode `0x0C`
(`flow.c:1898-1910`, `1985`, `1961`). One clone of the U28 route plus a
`p1.start` at the prompt is feasible with no poke; it should flip `0x2791C`
from NOT-HIT and add a real oracle. Movie audio is already closed: both
Smacker files declare zero audio tracks and no stream opens; it is a
zero-payload terminal finding.

### 3.7 The `0x2CADC` boot-defaults omission is a real visible gap

`config_set_defaults` (`0x2CADC`) makes ten raw calls; the port makes five
and omits `0x2F198` (draws the defaults message and stores the cursor words
`0x105F34/36`), `0x2EA78(0xB4)` (the 180-tick wait that presents frames) and
`0x2F280` (releases the cells). The omission is a deliberate `PORT` note: the
wait would present frames before the loop and move the frame-indexed oracles.
The message's pixels are real, the wait is real, and the row is one of the 13
WEAKER rows. It is the only non-error-path behavioural WEAKER row. The phase
should derive whether the message can be drawn and the wait modelled/re-pinned
without invalidating the oracle model; if not, it is a terminal deviation with
a measured reason. This is a fidelity fix, not a proof-debt row.

### 3.8 Proof debt, storage, coverage, classification

The 20 open differential rows are 19 blocker addresses; 13 AIL rows plus
`0x1C308`, the four PIT/game-port probes and `0x6C4CF` are terminal, and the
only genuine opening (modelling the `0x6C4CF` exit walk via DOS `int 0x21`
AH=0x3E/0x40) is not worth the proof. The 13 WEAKER rows are 12 inherent plus
`0x2CADC`; the 8 UNROWED are all `<0x5D000` and stay declared (the specs
would compare constants, not game behaviour). The 4 UNINSTRUMENTED are
inlined one-liners with VERIFIED rows; instrumenting them is low value. The
storage items (`0x2EA68`, short CMOS, slot 13..35 low bytes, the four
PLAUSIBLE items) are terminal with their measured reasons. The 53/37/315
classification is already the measured denominator; it needs a terminal
statement in the claim register, not a task.

## 4. Approaches considered

- **A. Close every gap with captures.** Attempt 0x21 and 0x25 with poked or
  blind routes, a U14 arcade win, a 5120 DAC capture, a U17 replay, and new
  audio captures. Rejected: the highest-risk captures (0x21/0x25/U14 arcade)
  have failed or are underived, and the audio/PCM and U17 results would not
  support a stronger claim than the terminal evidence already supports.
- **B. Terminal register only.** Write the claim register and stop. Rejected:
  it leaves the two real fidelity gaps (`0x2CADC`, 5120), the taken-continue
  branch, and the corrected U14 rule unaddressed even though they are
  closable with existing data or one small capture.
- **C. Fidelity fixes + one-attempt exhibits + terminal register.** Fix the
  real visible gap (`0x2CADC` if the frame model can be reconciled), decide
  5120 with a holder search then one diagnostic, correct and publish the U14
  two-human fought win, capture the taken-continue branch, and give 0x25 one
  derived staggered attempt; 0x21 gets a static derivation and a capture only
  if the derivation shows a re-hit window; audio gets the TL re-derivation and
  the no-capture bank residue table; everything else is terminal. **Chosen.**
- **D. Content modes first.** Spend the phase on 0x21/0x25 before the
  fidelity and terminal work. Rejected: the 0x21 route is underived and the
  0x25 route needs a new count window, so a content-first phase can end with
  no new claim and the real gaps still open.

## 5. Scope

**In scope:** Gate-0 (the outstanding Live Truth Task 13); `0x2CADC`; 5120;
U14 rule + two-human fought win; `0x0E` taken-continue; 0x25 derivation + one
staggered capture; 0x21 static derivation + conditional capture; audio TL
re-derivation, bank residue table, rendered-audibility terminal; the terminal
claim register; docs; the final two-gate run.

**Non-goals:** new portable functions; porting AIL, `0x6C4CF` or `0x1C308`;
poked 1P 0x21/0x25 exhibits; new audio-bank captures for the 10; a U17
tick-keyed replay; a 5120 aperture memory-file read; changing an existing pin
except by re-measurement; fitting a tolerance or a cause.

## 6. Evidence discipline

- Every new pin is measured from a capture or the raw bytes, with the address
  or frame that proves it; every new pin fails one step in the failing
  direction.
- A capture that fails a session check is preserved as `.failed` and adds no
  replay, oracle, pin, miss set, manifest entry or census improvement.
- A terminal declaration names the instruction/address that stops the claim,
  not a plausible cause. If a cause is not derived, it is not fitted.
- No poked 1P 0x21/0x25 exhibit; a poked two-human exhibit needs an explicit
  operator reversal, which this design does not take.
- The `0x2CADC` fix is conditional: if the wait cannot be modelled without
  invalidating the frame-indexed oracle model, the omission is declared
  terminal with the measured frame shift, not half-fixed.
- The full gate is two consecutive `PR_DIFF_FULLSCAN=1 GATE_STRICT=1
  PR_PINS_EXACT=1 make verify` runs plus `make audio-render`, on the final
  tree, after the fix wave.

## 7. Tasks (summary; details in the plan)

0. **Gate-0** — finish the pending Live Truth Task 13 sequence; fix any
   failure before new work; record the baseline.
1. **`0x2CADC`** — derive and, if the frame model reconciles, implement the
   message/wait/release; re-row without the nop pokes; re-pin affected
   oracles; otherwise terminal deviation.
2. **5120** — per-pixel holder search; if no holder, one DAC-injection
   diagnostic; fix or declare.
3. **U14** — correct the `b0c` derivation; drop the unsatisfiable clause;
   one two-human fought match win capture + oracle; declare R1 terminal.
4. **`0x0E` taken-continue** — one capture with a start press; oracle; census
   `0x2791C` HIT.
5. **0x25** — derive the count/camera residue; one staggered capture with
   `s0_53`/`camx`; publish or terminal.
6. **0x21** — static `+0x1C` re-hit derivation; terminal, or one two-human
   capture only if a window is derived.
7. **Audio** — TL re-derivation; no-capture bank residue table; t9-r5 replay
   for `0xE4`; rendered-audibility terminal (optional narrow
   register-to-render test).
8. **Terminal claim register** — one table for every remaining gap; docs;
   no new claims.
9. **Final gate** — two full gates + audio-render; counters; merge.

## 8. Risks and mitigations

- **Capture failures** (0x25/0x21/U14): derive first; one authorized run per
  route; abort to a preserved `.failed`; the terminal declaration is the
  fallback, not a second run.
- **Host-load audio flake**: the register-stream oracle is load-sensitive;
  run captures when load ≤ cores; the final gate is standalone; no pin is
  weakened for a flake.
- **`0x2CADC` frame shift**: the fix may move every frame-indexed oracle. The
  task has an explicit abort: if the wait cannot be modelled without a full
  re-pin, revert and declare the omission terminal with the measured shift.
- **0x21 derivation shows a window**: the phase authorizes one capture only
  after the static derivation; if the capture fails, the row is terminal with
  the `.failed` artifact and the derivation.
- **0x25 derivation shows the camera is the blocker**: the `camx` field
  measures it; if the camera is outside ±0x3300, the route is wrong, not the
  count. The task records which gate failed.
- **Scope creep**: the terminal register is the deliverable; a gap that
  cannot be closed in the phase is declared, not carried as an open task.

## 9. Terminal claims the phase expects to issue

The register is built in Task 8; the expected terminal set is U14's arcade
fought win (R1), 0x21 if the `+0x1C` derivation closes it, 0x25 if the
staggered run fails, rendered audibility, the 10/4 audio-bank residue, U17's
wall-paced replay, the differential 20/13/8, the 4 UNINSTRUMENTED/98 NOT-HIT,
the storage items, movie audio (zero-payload), and the 53/37/315
classification. `0x2CADC` and 5120 are closed if the conditional tasks
succeed; otherwise they join the terminal set with their measured reasons.

## 10. Success criterion (definition of done)

1. The terminal claim register exists and every remaining named gap has a
   measured status; no row says "open" without a task result.
2. Every new capture is either published with an oracle/pins or preserved as
   `.failed` with no claim.
3. Every new pin is mutation-proven; every existing pin is unchanged or
   re-measured with a recorded reason.
4. The two full gates and `make audio-render` are green on the final tree;
   `make gate-check`, `capture_manifest --check-pins`, `port_progress.py`,
   `diff-verify`, `entry-triage` and `symbols.h` read the recorded values.
5. No port implementation changed except the conditional `0x2CADC` fix and
   the audio TL fix (if the re-derivation requires it); no new portable
   function.
6. Only `main` remains locally and remotely after the merge, unless the
   operator asks otherwise.
