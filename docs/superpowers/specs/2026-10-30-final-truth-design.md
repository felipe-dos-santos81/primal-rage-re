# Final Truth — design

**Status:** implemented through Task 8 — the terminal claim register is
`docs/superpowers/specs/2026-10-30-final-claims.md` (derivation record §F1–§F8);
the final gate is Task 9. Date: 2026-10-30.
**Predecessor:** Live Truth (`docs/superpowers/specs/2026-10-29-live-truth-design.md`).
**Plan:** `docs/superpowers/plans/2026-10-30-final-truth.md`.

## 1. Where the project stands

The port is complete on its declared target: **759/759 portable functions**
(798/1203 raw, 66%; the remainder is 53 host-owned/deferred, 37 replaced
runtime, 315 runtime). The 2026-10-29 Live Truth phase published the last
live captures: the KO-branch draw, the no-poke two-human KO, the continue
screen/countdown/expiry, the name-entry oracle, and the 74 audio matched
prefixes. The capture inventory is **44 paths, 43/44 present**, with only U14's fought
live win declared absent. The differential census is **1033/1033 functions
VERIFIED; 14993/14993 mutants detected; 0 named gaps; 825/845 rows with
callees closed; 810/845 full; 8 unrowed; 12 `WEAKER`**; the coverage census is
**HIT 675 / NOT-HIT 80 / UNINSTRUMENTED 4**.

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

**Task 3 outcome (2026-10-30, record §F3).** The split is implemented:
`gp_win.MATCH_WIN`/`live_match` has no `b0c` clause (the match result must be a
2-0 win for its side and the KO'd side's `+0x5A` must have been live), and
`gp_win.FINAL_WIN`/`live_final` requires `b0c=1` at or after the mode-`0x0F`
entry (the final path); no scenario is in `FINAL_WIN` yet. `gp_twop.py check
--win` adds the two-human fought match win (both rounds' live-to-`0x78`, the
round and match results, no time-out, an X record). The one authorized
`gp-u14-twop-win` run reached the game's two-round 2-0 win (`b1f=3`, no `W`,
round 1 `f=0xB1B`, match result `f=0x13A8`, mode `0x12` at `f=0x1693`) but
failed the `port script v2` check at `f=0xB1B` (`pad change ... unpinned`, no
`S(0xB1A)`; 15 of 5,820 snapshots missed by the capture model). It is preserved as
`data/k11-captures/gp-u14-twop-win.failed`; per the capture discipline it
publishes no oracle, pin, miss set, manifest entry or `CAPTURE_DIRS` path, and
R1 is terminal (the measured arcade damage race in §3.1's opening). The claim
is game evidence only, not a published capture.

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

**Outcome (Final truth Task 6, record §F6): `0x21` stays declared and the
candidate blocker is refuted; no capture.** The stream does not restore the
height (the six tumble streams `0xC9604[0..5]` are sprite-hold loops with no
motion opcode), but the engine does: `0x4B470` sets `actor+0x36 = 0x240`
(`0x4B52E`) and `motion_step` (`0x2A4FC`, `0x2A524..0x2A537`) adds `vy` to
`actor+0x1C` every iteration. The same-pass case-6 test/decrement runs first,
so the first step adds `0x230` and `h(n) = 568n - 8n²`: a 71-motion-step flight
(landing on the 72nd effects pass, apex `0x2760`, 157.5 px), with 35 passes of
`vy < 0` where `0x49F18` re-sets `entry+0x1C` bit 7. `0x4B470` re-launches on
every hit and the count is zeroed only on landing (`0x49FAF`) or a
spawn/rebuild (`0x49622`/`0x4D061`/`0x4E4E8`), so the count
**accumulates across a flight chain** and the eight-hit tail (`0x4B579`) is
mechanically reachable — the existing unit cases G/H/I/J of `test_fight.c` pin
every link but the composition. What is not derivable is a frame-exact route:
the trigger needs a stage-0 two-human match (the two-human pick is `rng(7)` at
`0x2587C`, 1-in-7, measured `m=0`/`b1f=3` at f=876 in `gp-twop` → 5; the
`0xA87C4[c] + rng(6) + 1` arm is the 1P pick), a crowd lie (four measured
`0x4B470` calls — `gp-idle-loss` twice and the demo twice — all first hits,
`+0x1F=1`, on distinct lying entries), and eight strikes each inside a 7-motion-step window
(`n = 65..71`) of the 71-step flight. With one capture and `.failed` publishing nothing, the phase
does not spend it on an underived script; the mode's pass `0x4BF18` and the
tail `0x4BD98` stay `NOT-HIT`. A port-side route search (a derived, frame-keyed
script) is the route to exhibit the mode, and it is a new task.

### 3.3 0x25 — the staggered route opened the mode; the residue was the count

**Outcome (Final truth Task 5, record §F5): `0x25` is published.** The one
authorized staggered capture `gp-bowling-2` (mirror-ARMADON move 0 five times
scheduled on both sides, 150 frames apart — the fifth never fires, 28 of 32
steps, so four fire — no poke; sha256 `54d4c7d5…`, 1,491 frames,
every session check ok) raised `DS_00108892` to `1/3/5/7` and opened the mode
at `f=0x832` — the first frame slot 0's `+0x53` was 0 with the count `>= 6`.
The measured `+0x53` lattice is `7`, `0x0A` (pose arming), `0x0B` (knockdown
floor), `0`, so the get-up's `0x35D6C` clear supplied the gate's zero and the
`0x367DC` 91-frame arm was never reached; the appended `s0_53` (`0x107803`) and
`camx` (`0xF0AF0`) settle the residue as **the count, not the camera**
(`camx` `-1433..4887`, `-606` at the entry, against the `±0x3300` gate; `0x104AD8`
is 0 in a fight and `0x104ABC = (b1f == 3) + 1`). `gp-bowling-oracle` is in
`make verify`/`CAPTURE_DIRS` with `k_miss_gp_bowling`, the `GP_BOWLING_*` pins
(N 1,422, trace/moves 2,159, start 97, reach 1,137, 4 missing, 1,491 frames)
and a capture-443 three-present scan-out allowance (rows 19..32 port 301, rows
33..199 port 302, the measured pair 301 pinned); the census re-pin moved
`0x4E11C` and the mode's own chain to HIT (`HIT 675 / NOT-HIT 80`). The claim
boundary: the counter/camera series are the checker's capture-side claims, the
frame claim reaches capture 1,421 (the entry + 70) and nothing past it, and the
later scoreboard is not claimed. The live-truth phase's two failed real-input
attempts (`b892=2`; last sample 5, +1/+2/+2) are preserved as
`gp-bowling.failed`/`gp-bowling-fix.failed` and still fail `mode 0x25 never
observed`.

*The derivation below is the pre-capture text and stays as the raw evidence;
its "a staggered route is a new, unmeasured task" conclusion is superseded by
the outcome above.*

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
DAC-injection diagnostic (the **six** derived W writes over the two records
that cover the four indices; the design's 56/24 was the pre-derivation estimate,
record §F2.2; sentinels for `0x65/0x32/0x4D/0x2E`, back-buffer reads at
`0xE87A0` and `0xE87A4`) distinguishes PASS-A (capture/pipeline present model) from
PASS-B (content divergence with a nameable renderer). The fade source is
pinned through `0x29B74`/`0x13D4C`/`0x134C0`; the specific list entry for
`0x32` is not yet pinned.

**Task 2 outcome (record §F2.5): PASS-A.** The one authorised diagnostic
`data/k11-captures/gp-u13-band-dac` shows the 13 band pixels as the injected
sentinels at capture frame 05130 and the S record at `f=0x1826` holds the port's
back-buffer indices (`0x32`×8/`0x2E`/`0x4D`×4), so the old capture-5120 blue
`00001c` is the capture/present-side class, not content. The exact
palette/scan-out transient is not named; the allowance `(5120,1,5,56b900b1…)`
is unchanged. The pre-capture text above is the raw evidence.


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
missing capture. **Superseded by the measured outcome (record §F7.2):** the 4
never-requested premise is wrong for two banks. `grave+0x0A8C` is requested by
the mode-0x0D `sound_voice(0x25 + (n != 0))` arm (the "dead" claim holds only
for a literal `0x26` constant), `grave+0x08` is the **claimed** 1,200-tick
ending section (`0x5A56E385`, id `0x2A`, reached by the mode-0x0D `n == 7` arm
in the poked endings — not gated by U14 R1), and `grave+0x05E4` is the
reset-only second of that pair. `sound+0x7714` is requested by three reaction
callbacks behind the shared `+0x42`/`+0x43` gate (the t9-r5 replay reaches
`r0=0x33` but the gate fails; the "99 sampled frames" is the exact
`s1_42 == 0x8830` count, 99 `S` rows), and `konsd+0x1CCB0` is gated by the
`0x24508` stream target. The corrected inventory is **12 claimed + 10 residue +
2 requested-but-unattributable (`0x0A8C`, `0x05E4`) + 2 never requested
(`0x7714`, `0x1CCB0`)**; a no-capture residue table (bank → first-diff tick and
cause class) is the smallest defensible closure for the 10.

### 3.6 U17, taken-continue, movie audio

U17's wall-paced walk has no tick in its I records; a tick-keyed replay would
need a new capture with a `tick=` column (the K11 driver is the precedent).
Its screens are already covered by K11, and the device-label band and clear
are pinned capture-side; the unique gain is binding the label to a port
frame. This is terminal for the phase (the tick-keyed replay is documented as
a closable-but-low-value option).

The taken-continue branch was a genuine small gap: `gp-u28-continue` proves
expiry only. `flow_continue_poll` returns 1 with a credit and a newly pressed
start, sets `b1f=side+1`, spends the credit, and stores mode `0x0C`
(`flow.c:1898-1910`, `1985`, `1961`). **Task 4 outcome (record §F4): closed.**
`gp-u29-continue-taken` takes the prompt at `f=0x1561` (mode `0x0E`→`0x0C`,
`cred` 4→3, `b1f` 1, the incumbent's `+0x5A` `0x78`→0) with no new poke, and
`gp-u29-continue-taken-oracle` is in `make verify`; the census improves
`0x2791C` and `0x33B00`. Movie audio is already closed: both Smacker files
declare zero audio tracks and no stream opens; it is a zero-payload terminal
finding (register R33).

### 3.7 The `0x2CADC` boot-defaults omission was a real visible gap

`config_set_defaults` (`0x2CADC`) makes ten raw calls; the port makes five
and omits `0x2F198` (draws the defaults message and stores the cursor words
`0x105F34/36`), `0x2EA78(0xB4)` (the wait that presents one frame and runs `n + 2` = **182**
tick passes; the plan's "180" was the pre-derivation label, record §F1.3) and
`0x2F280` (releases the cells). The omission is a deliberate `PORT` note: the
wait would present frames before the loop and move the frame-indexed oracles.
The message's pixels are real, the wait is real, and the row was one of the 13
WEAKER rows. **Task 1 outcome (record §F1): fixed.** The three calls are wired
with the data object's strings (not a literal), the `0x2CADC` row is the full
ten-call row, the title (96) and attract (690) dumps are byte-identical before
and after, no oracle was re-pinned, and `WEAKER` is 12 with 810/845 full. The
message's rendered pixels are still not gated (the present is outside every
capture's frame set); that is the residual claim (register R01).

### 3.8 Proof debt, storage, coverage, classification

The 20 open differential rows are 19 blocker addresses; 15 AIL rows plus
`0x1C308`, the four PIT/game-port probes and `0x6C4CF` are terminal, and the
only genuine opening (modelling the `0x6C4CF` exit walk via DOS `int 0x21`
AH=0x3E/0x40) is not worth the proof. **Task 1 closed `0x2CADC`**, so **12**
rows are WEAKER; the 8 UNROWED are all `<0x5D000` and stay declared (the specs
would compare constants, not game behaviour). The 4 UNINSTRUMENTED are
inlined one-liners with VERIFIED rows; instrumenting them is low value. The
coverage census is HIT 675 / NOT-HIT 80 / UNINSTRUMENTED 4 after Tasks 4–5. The
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

## 9. Terminal claims the phase issued

The register is `docs/superpowers/specs/2026-10-30-final-claims.md` (Task 8):
**34 rows — 6 closed, 1 gated, 19 terminal, 8 declared**, each with its
address/file:line or capture. The measured outcome differs from the design's
expectation, and the raw wins:

- **Closed:** `0x2CADC` (Task 1: the message/wait/release are wired, the row is
  full, WEAKER 13 → 12, 810/845 full, no oracle re-pin); 0x25 (Task 5:
  `gp-bowling-2` opened the mode at `f=0x832`, the count not the camera was the
  residue); the `0x0E` taken branch (Task 4: `gp-u29-continue-taken`, `0x2791C`
  and `0x33B00` HIT); the `0x0E` expiry (live truth Task 5); the movie-audio
  zero-payload finding (R33); and the carrier-TL latch (Task 7a, R34).
- **Terminal:** U14's arcade fought win (R1) with the measured damage race;
  5120 **PASS-A** (Task 2: the content is the port's, the old blue is the
  capture/present-side class; the exact transient is unnamed); rendered
  audibility (Opal 2.0.3/49,716 Hz vs DOSBox-X 48 kHz); the 10-row audio
  residue; U17's wall-paced replay; U16 R2's measured operand; the differential
  `1033/1033`, `14993/14993`, 0 named gaps, `825/845`, `810/845`, 8 unrowed,
  WEAKER 12, 20 open rows/19 addresses; coverage HIT 675 / NOT-HIT 80 /
  UNINSTRUMENTED 4; the storage items; the 53/37/315 classification; and the six
  oracle model limits (front-end/K11 window starts, gameplay order/coverage, U10
  death streams/mode-F, gp-keys 835, the label-shift flake, `memsize=64`).
- **Declared:** 0x21 (Task 6: the count is not limited to 1, but no frame-exact
  route is derivable; the two-human `.failed` win is game evidence only; the 2
  audio banks requested-but-unattributable and the 2 never requested; the U16
  finisher; the capture inventory; the music-truth Task 6 optional items; the
  poked 1P exhibits).
- **Gated:** the 12 whole-section-claimed audio banks and the 74 prefixes.

No row says "open". The two-human `gp-u14-twop-win` capture is game evidence
only; `gp-u14-win-live` stays absent and declared. A capture that failed a
session check publishes nothing.

## 10. Success criterion (definition of done)

1. The terminal claim register `docs/superpowers/specs/2026-10-30-final-claims.md`
   exists and every remaining named gap has a measured status; no row says
   "open" without a task result; the docs test is in `make verify`.
2. Every new capture is either published with an oracle/pins or preserved as
   `.failed` with no claim (`gp-u29-continue-taken` and `gp-bowling-2`
   published; `gp-u14-twop-win.failed` game evidence only, no pins).
3. Every new pin is mutation-proven; every existing pin is unchanged or
   re-measured with a recorded reason.
4. The two full gates and `make audio-render` are green on the final tree;
   `make gate-check` reads 43/44, `capture_manifest --check-pins` the digest
   `d3b6969f…`, `port_progress.py` `798 1203 66` / `759 759 100`,
   `diff-verify` `1033/1033` / `14993/14993` / `810/845` / WEAKER 12,
   coverage 675/80/4, `entry-triage` unchanged and `symbols.h` byte-identical.
5. No port implementation changed except the `0x2CADC` fix (Task 1); the audio
   TL re-derivation needed only the unit test (Task 7), and no new portable
   function exists.
6. Only `main` remains locally and remotely after the merge, unless the
   operator asks otherwise.
