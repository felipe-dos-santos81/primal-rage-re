# Live Truth — derivation record

**Phase:** Live Truth. **Design:** `docs/superpowers/specs/2026-10-29-live-truth-design.md`
(status Proposed). **Plan:** `docs/superpowers/plans/2026-10-29-live-truth.md`.
**Baseline:** `main` at `0b458148` (`Merge branch 'state-coverage'`), full SHA
`0b45814842650de0b8f70fceeeab55a8d761fda4`. Branch `live-truth`.

**Phase rules.** No port code changes: the phase is capture and record, and no
new portable function is in scope. A capture that proves a port divergence is
recorded at its first unexplained frame and stops; it is not patched. The design
is proposed, and as of this record nothing of it has been built, captured or
pinned.

## §L1 Baseline

### L1.1 Commands and measured output

Measured on 2026-10-29 at `0b458148`, before any change in this phase.

```
$ python3 tools/port_progress.py
798 1203 66
759 759 100
ported 798 (759 portable + 39 runtime)  host-owned 41  deferred 12  replaced 37  runtime 315

$ make gate-check
gate: captures 37/38 present, 1 missing (strict=1, pins_exact=1)
gate: declared gap (absent): data/k11-captures/gp-u14-win-live
capture_manifest: 37 present checked, 1 declared skipped, 0 problems
capture_manifest: pin baseline ok (digest eed6eff69cedef33d57c590862ad3c4a3a0fd1ce1ec59a6d32f844cbca2f3579)
capture_manifest: verify preflight ok

$ python3 tools/capture_manifest.py --check-pins
capture_manifest: pin baseline ok (digest eed6eff69cedef33d57c590862ad3c4a3a0fd1ce1ec59a6d32f844cbca2f3579)

$ python3 tools/coverage_census.py --check-instrumented
check-instrumented: every .c column-0 header is instrumented

$ git status --short --branch
## live-truth
?? docs/superpowers/plans/2026-10-29-live-truth.md
?? docs/superpowers/specs/2026-10-29-live-truth-design.md

$ git log --oneline -3
0b458148 Merge branch 'state-coverage'
06d7071c docs: correct the mem[0..3] reader note
f82c73fa port: keep the render free list empty after a drain

$ git rev-parse HEAD
0b45814842650de0b8f70fceeeab55a8d761fda4
```

### L1.2 Counters

- **Raw:** 798 ported of 1203 real functions (66%).
- **Portable:** 759 of 759 (100%).
- **Gate:** captures 37/38 present; the one missing is the declared U14 gap
  `gp-u14-win-live` (absent, `.failed` artifact preserved).
- **Pins:** 37 present checked, 1 declared skipped, 0 problems (the
  `make gate-check` line above, which runs the manifest check).
- **Census instrumentation:** every `.c` column-0 header is instrumented.
- **Differential counter:** not re-measured in Task 1 (Task 12 re-measures
  it). The last recorded line, from `AGENTS.md`, is
  `1033/1033 functions VERIFIED; 14993/14993 mutants detected; 0 named gaps;
  825/845 rows with callees closed (188 have none); 809/845 closed on full rows;
  8 ported functions with no row`.
- **Coverage census:** not re-run in Task 1. The last recorded line, from
  `AGENTS.md`, is HIT 653 / NOT-HIT 102 / UNINSTRUMENTED 4 (sums to 759).

### L1.3 Capture inventory

`ls data/k11-captures` at baseline (48 entries; `.failed*` entries are failed
runs and carry no claim):

```
de  diags  gp-draw-timeout  gp-harness-smoke  gp-idle-loss  gp-idle-loss-run2
gp-keys-fight  gp-pads  gp-twop  gp-twop-ko.failed  gp-u10-ending  gp-u12-endurance
gp-u13-band  gp-u13-ending2  gp-u14-win-live.failed  gp-u15-join  gp-u16-twop2
gp-u17-device2  gp-u18-talon-ending  gp-u19-vertigo-ending  gp-u20-chaos-ending
gp-u21-armadon-ending  gp-u22-diablo-ending  gp-u23-u16-finisher
gp-u24-u16-state.failed  gp-u24-u16-state.failed.run1  gp-u24-u16-state-t9-r3
gp-u24-u16-state-t9-r4  gp-u24-u16-state-t9-r5  gp-u25-endurance-end
gp-u25-endurance-end.failed  gp-u26-side1-ending  gp-u27-name-entry  gp-u5-charsel
gp-u6-moves  gp-u6-moves-b  gp-u8-attract-start  gp-u8-endurance  gp-u8-handicap
gp-u8-left-training  gp-u8-right-arcade  gp-u8-right-training  gp-u8-tug-of-war
gp-u9-win  idle  menuesc  save-reload  walk
```

Not present, as the design measured: `gp-draw` (no directory),
`gp-u28-continue`, `gp-bowling`, and `gp-u14-win-live` (the `.failed` artifact
only). `gp-twop-ko` exists only as `gp-twop-ko.failed`.

### L1.4 Recon corrections

These are the corrections the phase starts from. Re-measured in this phase's
Task 1: the baseline commands of L1.1 and the U16 set/exit rows of item 1
(`poll.log` lines 5658-5659 of `gp-u24-u16-state-t9-r5`, read in the correction
pass, not in the first Task 1 run). Carried from the design's §2.3-§2.7 and not
re-measured here: the `0x25` bowling string, the `0x0E` side rule, and the U14
`b0c` rule (items 2-4); Tasks 4, 6, 8 and 9 re-measure them. None is a pin.

1. **U16 R2 is measured.** The set record the earlier `.failed` artifact missed
   is in `data/k11-captures/gp-u24-u16-state-t9-r5`. It reads
   `f=1531 s0_54=02 s0_30=00000EAE s0_42=8000`, with the exit at
   `f=1532 s0_54=00 s0_30=00001180`. The gate input is `y(1531)=0x0EAE`, below
   `0x1400`. The gate at `0x48599`/`0x4859C` is strict (`jle` after `cmp`), so
   equality does not clear it. This is a record task (ruling R3), with no capture
   and no pin.
2. **`0x25` is the bowling easter egg, not a camera.** `fight_mode25_enter`
   (`0x4E11C`, `fight.c:5578-5624`) draws string `0x61` at row 6. The
   state-coverage Task 4 label "both-characters-4 camera" is wrong. The gates
   and the two-human route stay as the design records them, but the mode is
   named by the decoded string, which Task 6 reads.
3. **`0x0E` needs the KO on `DS_0010810D`'s side.** `0x0E` opens only inside
   mode `0x0C`, through `flow_continue_open` (`0x278B0`) ← `flow_arena_ko_check`
   (`0x272DC`) ← `game_mode_0c_step` (`0x274EC`). The KO must be on the side
   named by `DS_0010810D` (`flow.c:1813-1827`); otherwise the winner-pose `0x0D`
   arm runs. The state-coverage declaration ("opens on a KO") omitted this rule.
4. **U14's `b0c` is unsatisfiable on match 1.** `live_death` demands `b0c=1`,
   but `DS_00104B0C`'s only setter is `0x37FF6` in `0x37EA0` (the remains
   stream's `0xD100` opcode), which runs on the ending path. The U14 scenario
   stops at mode `0x12` in match 1, so the clause cannot pass on that capture.
   The `.failed` artifact is game evidence only (ruling R5).

### L1.5 Ledger

The phase ledger is `.superpowers/sdd/2026-10-29-live-truth/progress.md`
(git-ignored). Its preflight table and task list were written before this task
and are kept; Task 1's result is appended there. Rulings: R1-R5 are the brief's
rulings (`task-1-brief.md`; R1 distinct draw claims, R2 the two-human KO is the
game's own damage path, R3 U16 R2 is a record task, R4 `0x21` stays declared,
R5 U14's fought win stays open). R6-R8 are the orchestrator's ledger rulings
(capture serialization, measured pins win, optional-task stop rules). The plan
lists only R1-R5; R6-R8 live in the git-ignored ledger.

## §L4 U16 R2: the measured gate operand (Task 4)

The measurement, its strict reading and the verdict. The full evidence (the
reader output, the raw bytes, the §D3.4 comparison and the `.failed` artifact's
rows) is in `docs/superpowers/plans/2026-10-27-claim-coverage-derivations.md`
§C5; this section records the phase's reading of it. No capture, no port change,
no pin.

### L4.1 Measured rows

`python3 tools/gp_u16_state.py read --capture data/k11-captures/gp-u24-u16-state-t9-r5`
(run today; `session.txt`: `pad_bios=0`, 11 of 11 checks `ok`):

- set: `f=0x1531 s0_54=0x2 s0_30=0xEAE s0_42=0x8000`
- exit: `f=0x1532 s0_54=0x0 s0_30=0x1180 s0_42=0x8000`
- gate input `y(0x1531) = 0x0EAE`, below `0x1400`

### L4.2 The gate and the strict reading

- The raw gate is `0x48599 cmp edx,[eax+0x30]` then `0x4859C jle 0x48601`, with
  `edx` the high word of the dword at `0xBD882 + 2*char`. The run's character is
  measured: `c0=02` on the `f=0x1531` row (`t9-r5` line 5658), and the table at
  `0xBD884 + 2*2` reads `0x1400`. `jle` returns when the threshold is `<=` the
  latched y, so the clear is **`y < 0x1400`, strictly**: equality does not clear.
- The gate reads the previous iteration's latched y, so the clear at `f=0x1532` reads
  `y(0x1531)`.
- The claim-coverage plan's `y <= 0x1400` wording is the plan's old reading, superseded
  by this strict one (claim-coverage §C5.2; the plan is not edited). The port's gate, `port/src/game/fighter.c:16326-16327`, is
  already strict.

### L4.3 The pre-fix value and the class

The state-coverage record's aligned `0x1531` row (§D3.4) gives the original's
`0x0EAE` and the pre-fix port's `0x202E`. The difference is `0x1180`, the record-motion
offset the table reports on every row from `0x152C`. The pre-fix port's `0x202E` is
above the threshold, so its gate did not clear. The IVT stand-in fix
(`bios_ivt_stand_in()`, `port/src/game/flow.c:6996-7000`; state-coverage §D3.7)
removed that class, and the U23 replay's exit moved to `0x1533`. This phase did not
re-run that replay.

### L4.4 The `.failed` artifact

`data/k11-captures/gp-u24-u16-state.failed` has no `s0_54=2` set record and no row
at `f=0x1532`, so its set frame is not measured and its `0x1180` exit is inferred. It
is not a measurement. `t9-r5` is the measurement.

### L4.5 Verdict

The deciding gate input is measured and is below the gate. The original's clear is
the gate, read on the strict inequality. No port change follows, no pin moves, and
no capture is taken. Whether the finisher gap R2 is closed is not decided here.

## §L9 The 0x21 volleyball declaration (Task 9)

**Decision: `0x21` stays declared, not captured.** No capture, code or pin. The gate
chain is re-derived from the bytes below and each blocker is cited to where it is
measured or read. The census rows stay `NOT-HIT`: `0x4BD98` (baseline l.696) and the
mode handler `0x26540` (l.240).

### L9.1 The store and its gates (`fight_4bd98`, `0x4BD98`)

`port/src/game/fight.c:2940-2977`:

- `fight.c:2943` `DS_00104AD8 > 0` returns (`0x4BDA7`).
- `fight.c:2944` `DS_00104ABC < 2` returns (`0x4BDB4`). `DS_00104ABC = (b1f == 3) + 1`
  is written at `flow.c:1632` (`flow_player_join`, `0x28DCF`) and `flow.c:2416` (the
  round-end case-3 arm, `0x25EB2`), so the gate is a two-human match only.
- `fight.c:2945` `fight_13134() != 0` returns (`0x4BDC1`). `fight.c:2921-2934`: both
  fighters' x beyond `±0x3300` on the same side.
- Store body, `fight.c:2946-2976`: `DS_00108864 = entry`; `entry+0x1E = 6`;
  `DS_001088C1 = 1`; the mode word `DS_00104B00 = 0x21` (`fight.c:2949`, `0x4BDE7`);
  `DS_001088C5 = 1`; the `0x1E` timer `DS_001088AA` (`fight.c:2951`, `0x4BDF4`); the
  two actor spawns `0xBAB88` (`fight.c:2968`) and `0xBAB9C` (`fight.c:2971`) at the
  fighters' midpoint, both `+0x36 = 0x1A4` (`fight.c:2976-2977`).

### L9.2 The caller tail (`fight_4b470`, `0x4B470`)

`fight.c:3080-3091`, the eighth-hit tail, returns unless every gate passes:
`3080` `DS_00104B1D` 2 or 3 (`0x4B547`); `3081` `DS_00104AFC != 0` (`0x4B559`, the
**stage word must be 0**); `3082` `DS_001088C1 != 0`; `3083`
`(DS_001088EF >> 24) <= 1`; `3084` `entry+0x1F <= 7` (`0x4B579`); `3085`
`DS_001088C5 != 0`; `3086` `DS_00108864 != 0`. Then `3090` `fight_4bd98(entry)`
(`0x4B590`) and `3091` `fight_4cb18` (`0x4B59E`).

`+0x1F` is the entry's hit count. The `0x4B69C` prelude (`fight.c:3101-3117`) tests
`+0x1C` bit 7 (`3104`), lets the hit through `fight_4b788` (`3111`), and increments
`+0x1F` at `fight.c:3113` (`0x4B716`).

### L9.3 The blockers

**(a) `b1d` is a mode byte, not a player count.** Arcade is `b1d = 0`; Endurance is
`b1d = 3` (`docs/superpowers/plans/2026-10-01-gameplay-u8-derivations.md` l.197,
l.248). The caller gate `fight.c:3080` excludes 2 and 3, so `0x21` needs an arcade
(`b1d` 0 or 1) match. The arcade rows (`b1d = 0`) are the ones that poll for a second
player (gameplay-u8 l.258-259).

**(b) The stage is 1 in 7.** The gate at `fight.c:3081` needs `DS_00104AFC == 0`. On
the `DS_00104B17 == 0` path of `flow_stage_pick` (`flow.c:1343-1350`), `b1f == 3`
picks the stage as `rng_next(7u)` (`flow.c:1350`, `0x2587C`), so the stage word is
0 with probability 1/7 per pick. I did not trace which of the four callers of
`0x25848` runs at an arcade match's setup, so the 1-in-7 is per pick on that path.

**(c) The trample counter resets on landing.** `+0x1F` is zeroed when an entry spawns
(`fight.c:1482`, `0x49622`) and when a tumbling entry lands in the case-6 arm
(`fight.c:4942`, `0x49FAF`). The re-hit needs `+0x1C` bit 7, which that arm sets only
while `+0x36 < 0` (`fight.c:4912-4913`). So eight hits must land on one entry during
its fall; a landing ends the count. Two other writers also increment `+0x1F`
(`fight.c:3455`, `0x4D81C`; `fight.c:4320`, `0x4C6DE`). This record does not trace
whether they reach the `0x4B590` tail.

**(d) No capture measures a count of two or more.** Every `poll.log` S row carries
`ent=`, which is entry 1's pointer (`DS_0010741C`, `tools/gp_session.py:44`), not a
per-entry count. No field records any entry's `+0x1F`, so the count is not measured in
any capture. The census is the
measured fact: `0x4BD98` is `NOT-HIT` (baseline l.696), so no run passed the chain to
the store. The trample `0x4B69C` is `HIT` (l.692).

**(e) The two-human gate.** `DS_00104ABC >= 2` needs `b1f == 3`, which only a joined
second player sets (`flow.c:1632`, `2416`). A 1P scenario cannot open it, and no poke
writes `b1f` or `DS_00104ABC` (AGENTS.md, the `0x21`/`0x25` paragraph).

**(f) A poke is not the route.** A poke could set `+0x1F = 8`. The state-coverage
design §9 bullet "The two-human-only gates cannot be poked open" states the stop rule:
"Do not build one; declare the modes with their gates". This phase keeps that rule.

### L9.4 The decision

- `0x21` stays declared with these citations. No new claim; the census rows stay
  `NOT-HIT`.
- A later poked exhibit needs an explicit operator reversal of state-coverage design §9.
- A real two-human chain needs a measured sequence this phase does not have: eight
  hits on one airborne entry, the stage word 0, `b1d` 0 or 1, `b1f == 3`,
  `DS_00104AD8 <= 0` and `fight_13134` false.
