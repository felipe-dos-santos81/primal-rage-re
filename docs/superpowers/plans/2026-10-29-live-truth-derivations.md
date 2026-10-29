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
