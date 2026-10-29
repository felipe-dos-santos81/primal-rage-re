# Live Truth — design

**Status:** Proposed; the design gate is open (no capture, code change or pin has
run for this phase). The implementation plan is
`docs/superpowers/plans/2026-10-29-live-truth.md`; the derivation record is
written during execution as
`docs/superpowers/plans/2026-10-29-live-truth-derivations.md`.

**Date:** 2026-10-29

**Baseline:** `main` at `0b458148` (`Merge branch 'state-coverage'`). The
portable port is complete (`759/759`), the differential frontier is declared
terminal, and the state-coverage phase is implemented and gated. The remaining
honest work is the set of live/unexhibited branches and the two claims the last
record already measured but did not close.

## 1. Why this phase

The port no longer has a porting target. What remains is **truth**: content the
game can reach but no capture exhibits, and claims a later record measured but
never folded into the authoritative narrative. Four of those branches are
already scaffolded by earlier phases (`gp-draw`, `gp-twop-ko`,
`gp-u24-u16-state`, `gp-u27-name-entry`); two can be derived from raw bytes in
this phase (`0x0E`, `0x25`); one is a decision this phase must take rather than
keep deferring (`0x21`); and two are measurement/record tasks that need no new
capture at all (U16 R2 and U14's fought-KO evidence).

The phase is deliberately **capture + record**, not port work. No new portable
function is in scope. If a capture proves a port divergence, the task records
the first unexplained frame and stops; it does not fit a patch.

## 2. What the recon measured

The recon was read-only, on `main`; every value below is re-measured by the
task that consumes it. "Measured" means a file/`poll.log`/binary read today;
"inferred" means a reading that the task must prove.

### 2.1 `gp-draw` — the KO-branch drawn match (`0x0A`/`0x0B`)

- **No `data/k11-captures/gp-draw` exists** (measured; the directory is
  absent), and no `gp-draw` oracle, `CAPTURE_DIRS` entry or miss set exists.
  The state-coverage record §D2.7 says the same; `gp-draw-timeout` is the
  *timeout* draw, a different claim.
- The scenario and checker are already merged: `SCENARIOS['gp-draw']`
  (`tools/gp_session.py:572-602`) pokes `0x10789E` (round 1 P2 KO),
  `0x10780A` (round 2 P1 KO) and both (round 3, `KO_BOTH`), because a 1-1
  match leaves `AD4=-1`; the draw needs `B1E==ADC==3` and `w2==w3` at round 3
  (`0x27BA4`, `flow.c:2934-2950`). The stop is mode `0x0B` 60 frames in.
- `tools/gp_draw.py check` already reads `AD4==2`, `B1E==ADC`, `w2==w3` and
  requires mode `0x0A` then `0x0B`; the timeout check is the existing
  `--timeout` variant.
- **Missing:** a capture, a `gp-draw-oracle`, `GP_DRAW_*` pins, a
  `CAPTURE_DIRS` row, a `k_miss_gp_draw` row (prefix 0, so it does not also
  match `gp-draw-timeout`), a manifest entry, a pin-baseline reason, and the
  `.dro` decision. `gp-draw-timeout` is the model for all of them
  (`Makefile:1304-1334`).

### 2.2 `gp-twop-ko` — the two-human KO

- The surviving artifact is `data/k11-captures/gp-twop-ko.failed` (the
  recovery run). **The game's KO happened:** `S f=0B91 ... s1_5a=78
  w3=00 ad4=FFFFFFFF b0c=00`; `gp_twop.py check --ko` passes
  (`ko f=B91; round result f=B91`), and the loser's `0x78` came from a live
  `0x75` (measured poll rows).
- **One session check fails:** `check=FAIL port script v2 (key 16 (p1.b2)
  consumption at f=596 unpinned)` (`session.txt:15`). The poller missed
  `S(0x595)` after a 23-frame stall (`poll.log` S jumps `0x593→0x596`), so a
  BIOS word could not be pinned. This is host starvation, not the game.
- The guard/preflight named by claim-coverage Task 9 is already on `main`
  (`tools/gp_capture.py:89-140`, `capture_manifest.py:405-408`): load above
  the core count refuses the run, a missed `S(c-1)` fails fast, and a backlog
  skip fails the run. A third attempt is therefore **one guarded attempt**,
  not a certainty; a failure is preserved as `.failed.N`.
- The port replay's reconstructed script reaches a KO and the post-KO screen
  inside the capture's window (measured read-only); the exact agreement of a
  gap-free script is inferred. The miss set is the base pair
  `0x29D60`/`0x5D812` (`frontend_mode_1b_step`), plus a `k_gp_sets` row.

### 2.3 U16 R2 — already measured; fold it into the record

- `data/k11-captures/gp-u24-u16-state-t9-r5` holds the set record the earlier
  `.failed` artifact missed: `f=1531 s0_54=02 s0_30=00000EAE s0_42=8000`,
  exit `f=1532 s0_54=00 s0_30=00001180`; the reader prints
  `gate input y(1531): present: s0_30=0xEAE (below 0x1400)` (measured today;
  the state-coverage record §D3.4 already tabulates the same row).
- The gate is **strict**: `0x48599 cmp edx,[eax+0x30]; 0x4859C jle` clears
  only when `y < 0x1400`; equality does not clear. The clear belongs to the
  previous iteration's latched y, not the exit frame's.
- Therefore R2 is not a capture task. The claim-coverage record's §C5 is
  missing, and before this task `AGENTS.md` said the gate input was unmeasured (it is
  now measured). The task
  writes the measured value and the strict-inequality correction; no port
  change and no pin.

### 2.4 `0x0E` — the continue/toll screen

- The only open path is `flow_continue_open` (`0x278B0`) ←
  `flow_arena_ko_check` (`0x272DC`) ← `game_mode_0c_step` (`0x274EC`); it
  opens only inside **mode `0x0C`**. The KO must be on the side named by
  `DS_0010810D` (the incumbent/human side), or the code takes the winner-pose
  `0x0D` arm instead (`flow.c:1813-1827`). The state-coverage declaration's
  "opens on a KO" omitted this side rule (raw wins).
- Boot credits are **5** (`cred=00000005` in every gp poll log); one start
  spends one, so **4 remain** after an arcade start. No credit write is
  needed for the prompt. The countdown starts at `0x0F` and decrements every
  64 frames (`DS_000EF6DC & 0x3F`), so expiry is ~1,024 frames; the expiry
  path is mode `0x15` with `b25=0`.
- A capture can clone `gp-u10-ending`'s mode-`0x0C` route, poke
  `KO_P1 = 0x10780A=0x78` at the round entry, and stop on `0x0E` (prompt) or
  `0x15` (expiry). The poke is a labelled harness write; the claim is the
  screen/countdown, not the KO.

### 2.5 `0x25` — the bowling easter egg, not a camera

- The state-coverage Task 4 declaration called `0x25` "both-characters-4
  camera". The raw entry `fight_mode25_enter` (`0x4E11C`,
  `fight.c:5578-5624`) draws **string `0x61` at row 6** via
  `text_cursor_hold_font2(-1, 6, game_string_get(0x61), 0x4000)`; the recon
  read the family's strings as the bowling banner/scoreboard. The task
  re-reads `game_string_get(0x61)` (the table is encoded) and uses the decoded
  text before naming the mode; the string read is not a fitted claim.
- The gates are: `DS_001078FA==2` (live fighters), slot 0 `+0x53==0`,
  `DS_00104B1D ∉ {2,3}`, camera `|0xF0AF0|<=0x3300`, `DS_00104AD8<=0`,
  `DS_00104ABC>=2` (two humans), **both characters 4** (`0x10782A`,
  `0x1078BE`) and `DS_001088C4==0`. The **input** is
  `DS_00108892>=6`: `fighter_45158` increments it while the other slot is
  char 4 with `+0x5F==0x22`; both players must perform that reaction move.
- The two-human route is `gp-twop`'s join, P1 five rights to cell 5 (class 4
  ARMADON), P2's default cell 5 (class 4), then both perform move 0
  (`gp_moves.py steps --char 4 --facing 0/1 --moves 0`). The capture stops on
  mode `0x25`. This is the family's first execution evidence; the census rows
  are all `NOT-HIT` today.

### 2.6 `0x21` — volleyball stays declared

- The gate is `fight_4b470`'s eighth-hit tail: eight worshipper tramples of
  one effects entry (`+0x1F > 7`), `DS_00104B1D ∉ {2,3}`, `DS_00104AFC`,
  `DS_001088C1`, `DS_001088C5`, `DS_00108864` clear, `(DS_001088EF >> 24) > 1`,
  then `DS_00104AD8<=0`, `DS_00104ABC>=2` and `fight_13134` false; the entry
  spawns `0xBAB88`/`0xBAB9C` and sets `DS_001088AA=0x1E`.
- The `b1d` byte is a **mode** byte (arcade 0), not a player count; the
  `b1f==3` join route satisfies `DS_00104ABC>=2`. The real blockers are the
  eight-trample chain (the counter resets on landing; no capture has
  `+0x1F>=2`) and the 1-in-7 stage RNG. A poke can force `+0x1F=8`, but the
  state-coverage design §9 forbids a poked 1P scenario; reversing that is an
  operator decision, not this phase's call.
- **Decision:** keep `0x21` declared with the measured blockers; if the
  operator reverses §9, a later poked exhibit is the route. The citations
  (`fight.c`, `flow.c` lines) and the evidence for each blocker are in the record's
  §L9 (`docs/superpowers/plans/2026-10-29-live-truth-derivations.md`).

### 2.7 U14 R1 — the fought win stays open; the KO report closes the rule gap

- `live_death` anchors on the match-1 round-2 KO `(0x09,1)` and then demands
  `b0c=1`. `DS_00104B0C`'s only setter is `0x37FF6` in `0x37EA0`, the
  remains stream's `0xD100` opcode, which runs on the **ending** path; the
  U14 scenario stops at mode `0x12` in match 1. The rule can never pass on
  that capture, whatever the fight does.
- Option (a) is the smallest honest claim: replace the two-human clause with
  `b1f==1` (arcade) and keep the damage/KO clauses; run it read-only on
  `gp-u14-win-live.failed` (`ko f=898`, `w3=2`, `b0c` never 1, no `X`/`W`).
  It is **game evidence only**: the artifact has no `X` and failed four
  session checks (end frame, port script, unscripted input, stopped at the
  end), so it adds no oracle and no pins.
- Option (b) (move to the final) is rejected for this phase: the only route
  uses the land pokes, and `b0c=1` there appears with all seven final KOs
  poked, so it does not discriminate a fought KO. Cost and claim are worse.
- **Decision:** no new U14 capture without a go-ahead; `GP_U14_*` stays empty
  and the declared gap stays; T8 records the game-evidence-only KO and the
  option-(b) rejection.

### 2.8 The 5120 band

- The state-coverage §D4 diagnostic is **INCONCLUSIVE**: the DOSBox-X memory
  file is guest RAM (`MemBase`), not the VGA aperture (`vgamem`), so the 14
  bytes read `0xFF`. A port-side read of `DS_000E87A4` or a hook at `0x501A3`
  reproduces the port's own compositor and is not evidence.
- A falsifiable next experiment exists: poke the ending fade's palette source
  for the band's index at f=1825, with the dumped DAC's colour as the control;
  if the 8 band pixels turn the injected colour, the band is a DAC/compose
  timing artifact (pipeline, no port fix); if they stay `00001c`, the band is a
  content divergence. A second poke of `DS_000E87A4`'s rows 1..4 distinguishes
  a current-present sample from an older present.
- **Decision:** optional task T10. If the fade palette source cannot be pinned
  from the raw bytes, the task declares the instrument gap and leaves the band
  where §D4 left it; it does not fit a cause.

### 2.9 Audio and `gp-u27`

- The derived-k rotation anchor is already implemented
  (`tools/audio_compare.py:775-815`; claim-coverage §C3), with a null test that
  rejects a shifted copy and a shuffled capture
  (`tools/tests/test_audio_compare.py:528-566`). The `0x31` bank is anchored in
  23 of 25 occurrences today. What remains is a **tool-only** claim of each
  diff-allowed section's already-computed matched prefix; the 4
  never-requested banks need new captures and stay out of scope. Optional T11.
- `gp-u27-name-entry` is a complete capture/replay: the replay passes the miss
  set, the finaliser's 258 mode-`0x1E` port frames are byte-identical to the
  stored capture frames, and the measured pins are `MIN_FIRST 5871`,
  `MAX_START 81`, `MIN_REACH 4990`, `MAX_MISSING 38`, trace/moves `9402`,
  sha `5e2c5824…`, 5,885 frames. The only frame needing a named allowance is
  capture 334 (the mode-8 entry three-present scan-out) with a measured
  recipe. Promoting it is a bounded oracle task (T7), not an exhibit.

## 3. Approaches

**A — Live truth (recommended).** Publish the four scoped captures
(`gp-draw`, `gp-twop-ko`, `gp-u28-continue`, `gp-bowling`), promote `gp-u27`,
fold the measured U16 R2 value into the record, close the U14 rule gap as
game evidence, re-declare `0x21`, and run the optional 5120/audio experiments
only if their derivation lands. This closes every reachable unexhibited
content mode except the two-human-only volleyball and the model-limit
residuals.

**B — Model truth.** Spend the phase on the 5120 band, the audio banks and the
differential frontier. The differential frontier is already declared terminal;
the audio banks need new captures or a new request record; the 5120 is one
pixel anomaly. The claim value per unit cost is lower than A, and A's captures
also flip a meaningful set of census rows.

**C — Finalization.** No new RE; run the full gate, refresh docs, package the
port. Premature: four reachable content modes and a measured gate value are
still open, and the phase's captures are the evidence that would make a
"complete" claim honest.

**Choice: A.** The captures are the strongest remaining evidence, the harness
is ready, and the phase keeps B/C as named follow-ups.

## 4. Goals

- Publish `gp-draw`, `gp-twop-ko`, `gp-u28-continue` and `gp-bowling` with
  oracles, pins, miss sets and gate entries — or preserve a failed run and
  leave the gap.
- Promote `gp-u27-name-entry` to a gated `gp-u27-oracle`.
- Record the measured U16 R2 gate operand (`y(0x1531)=0x0EAE < 0x1400`) and
  the strict-inequality correction.
- Record U14's fought-KO game evidence and keep the fought win declared.
- Re-declare `0x21` with the measured eight-trample/stage blockers.
- Optionally decide the 5120 band with the DAC injection, and claim the audio
  diff-allowed prefixes.
- Keep every existing pin, counter and capture claim intact; the full gate
  green twice plus `audio-render`.

## 5. Non-goals and anti-goals

- **No U14 fought-win capture** without a new explicit go-ahead; no poked win
  substituted for it.
- **No poked `0x21` volleyball** unless the operator reverses
  state-coverage design §9.
- **No new portable function**, no port-code change, no pin widening. A
  proven port divergence is recorded for a follow-up, not patched.
- **No rendered-audibility claim**; the PCM pin stays determinism.
- **No new capture for the 4 never-requested audio banks**; they stay named.
- **No re-litigation of the terminal differential rows** or the declared
  storage/`config_field_get` items.

Anti-goals: a green gate that hides a host-load failure; a "published" capture
that failed its session checks; a pin fitted to the port's current output; a
claimed mode whose trigger was never measured.

## 6. Claims and non-claims

**Will claim:** the KO-draw round-over/winner-pose pixels (`gp-draw`, poked
KOs named); the two-human KO window (`gp-twop-ko`, the KO is the game's own
damage path, no KO pokes); the continue screen and its 15→expiry countdown
(`gp-u28-continue`, the KO poked); the `0x25` entry/content (`gp-bowling`, both
characters 4, the reaction move is real input); the name-entry entry/finaliser
(`gp-u27`); the measured U16 gate value; the U14 fought-KO game evidence.

**Will not claim:** the fought win; the volleyball content; the 5120 cause
unless the injection decides it; rendered audibility; the final-content
meaning of string `0x61` beyond the decoded text the task re-reads; any pixel
outside the new oracles' windows; the two-human KO's result screens if the
oracle window cannot cover them.

## 7. Risks and stop rules

- **Host load.** The two-human KO failed twice under starvation. The preflight
  must pass (load ≤ cores) and the run is one guarded attempt. A gap, a skipped
  BIOS word, or a failed session check preserves `.failed.N`; no pin.
- **Stop mode drift.** If a capture does not stop on the declared mode, the
  run is not publishable; re-derive the stop before a second attempt.
- **Replay window.** An oracle is added only if the port's replay reaches the
  claimed frames. If not, the capture is game evidence only, with the gap
  named and no pins.
- **Raw wins.** `0x25` is not a camera; the `0x0E` side rule and the U16
  strict inequality are corrections to earlier prose. The derivation record
  carries each correction with its address.
- **5120.** If the palette source is not pinned, declare the instrument gap.
  No fitted cause, no `BANDS_ALLOWED` move.
- **Audio.** If the prefix claim cannot be falsified (a prefix one shorter
  fails), leave the sections named.
- **U14.** No capture without a go-ahead; the `.failed` artifact is not a
  capture and adds no pins.
- **Time.** The full gate is ~2 h per run after the new oracles; if the host
  is loaded, the gate is deferred, not weakened.

## 8. What is left to the builder

- Exact scenario step timings, frame gaps, stop delays and the host schedule
  for the four captures; the specific pad edges for the `0x25` move.
- The measured oracle pins (never predicted); the `gp_continue.py` /
  `gp_bowling.py` predicate fields; the `gp_u27.py` evidence fields.
- Whether the `gp-draw` replay window covers both `0x0A` and `0x0B`; whether
  the two-human KO replay's window covers the KO and the result screen.
- The 5120 fade palette source and the injection's positive control.
- The audio prefix pin format and the per-section null test.
- The final `CAPTURE_DIRS` inventory and the census `improved:` set after the
  new captures.

## 9. Task outline

1. Baseline and the derivation record.
2. `gp-draw` — the KO-branch drawn match (`0x0A`/`0x0B`).
3. `gp-twop-ko` — the two-human KO.
4. U16 R2 — record the measured gate operand.
5. `gp-u28-continue` — the continue screen (`0x0E`).
6. `gp-bowling` — the `0x25` easter egg.
7. `gp-u27` — the name-entry oracle.
8. U14 R1 — the game-evidence-only KO report.
9. `0x21` — the volleyball declaration.
10. 5120 — the DAC-injection experiment (optional).
11. Audio — the diff-allowed prefix claim (optional).
12. Docs, counters and the coverage re-pin.
13. Full gate.

Captures 2, 3, 5, 6 and 10 run **serially** on the DOSBox-X host; the offline
tasks (4, 7, 8, 9, 11) may run in parallel worktrees and are cherry-picked. The
gate is last.
