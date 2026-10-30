# Live Truth — derivation record

**Phase:** Live Truth. **Design:** `docs/superpowers/specs/2026-10-29-live-truth-design.md`
(status implemented through Task 12a; Task 12b and the Task 13 full gate pending).
**Plan:** `docs/superpowers/plans/2026-10-29-live-truth.md`.
**Baseline:** `main` at `0b458148` (`Merge branch 'state-coverage'`), full SHA
`0b45814842650de0b8f70fceeeab55a8d761fda4`. Branch `live-truth`.

**Phase rules.** No port code changes: the phase is capture and record, and no
new portable function is in scope. A capture that proves a port divergence is
recorded at its first unexplained frame and stops; it is not patched. §L1 is the
baseline at which nothing of the phase had been built, captured or pinned. The
later sections record the phase's measured changes and declarations.

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

## §L2 The KO-branch drawn match (Task 2)

**Decision: `gp-draw` is published and gated by `gp-draw-oracle`; the KO-branch
`0x0B` winner pose is exhibited.** This is distinct from `gp-draw-timeout`: the
KO capture has `ad4=2`, while the timeout capture has `ad4=0xFFFFFFFF`. The three
KO pokes are harness writes, not fought KOs; U14's fought-live-win gap is unchanged.
No port code or existing pin changed.

### L2.1 One guarded capture and its evidence

Exactly one `make gp-capture scenario=gp-draw` ran. It wrote 2,132 frames (raw
1377..4939) in 71.2 s, with 2,955 S records (`f=0x0005..0x0B9E`), four W records
and one X record. All session checks passed: base, 7/7 steps and the mode-`0x0B`
stop, end frame, mode `0x27` after Enter, `kb == raw`, 2,132/2,132 frames, port
script v2, no unscripted input, 4/4 pokes with no race, zero skipped BIOS words,
and the SIGTERM stop at `f=0x0B9E`. `gp_capture.py check-input` also passes.

The three poke steps are round 1 `0x10789E = 0x78` (P2 KO), round 2
`0x10780A = 0x78` (P1 KO), and round 3 both addresses (the drawn round). The
deciding S record is:

```
f=0B02 mode=000A ad4=00000002 b1e=03 adc=00000003 w2=01 w3=01
f=0B26 mode=000B ad4=00000002 b1e=03 adc=00000003 w2=01 w3=01
```

Thus `python3 tools/gp_draw.py check --capture data/k11-captures/gp-draw` exits
0: `ad4=2`, `b1e=adc=3`, `w2=w3=1`, and mode `0x0A` precedes `0x0B`. The capture
identity is 2,132 frames and poll.log sha256
`d99571d3db0bf2135ad10d3a0f08d5c5bbdebdea392fad2821e7ce0901299885`.

### L2.2 Replay and miss set

The first replay reached the script end but failed only the not-yet-registered
miss-set assertion. Its measured log is `distinct=4 dropped=0`: the global
`0x5D812` `actor_spawn`/`set_dead` rows plus `0x29D60` and `0x5D812` from
`frontend_mode_1b_step` (one hit each). `k_miss_gp_draw` pins the latter pair;
its `k_gp_sets` row uses **prefix 0**, so `gp-draw` and `gp-draw-timeout` cannot
both match. The oracle replay then passes with the same four rows.

The 1,686-frame port dump reaches mode `0x0A` at port 1589 and `0x0B` at port
1681, and ends at `f=0x0B62` (2914). Trace and moves have zero differences
through 2914, so both pins are 2915.

### L2.3 Capture 346 and the measured frame window

The initial report measured window start **100**, first unexplained **346**, and
12 missing classes. Capture 346 (raw 2827) is the round-1 mode-8 entry's
two-segment scan-out, not a port content divergence:

- rows 0..2 equal port 218, the only earlier holder;
- rows 3..199 equal port 221;
- the vacuous-middle adjacent pair `(220,221)` is unique in the 1,686-frame dump;
- ports 219 and 220 are unexhibited;
- the top-row sha256 is
  `f1839ec475ffc5228cb673caafb0ac274e3221299b50e1be804ae5da887aef1b`.

`SCAN3_ALLOWED['gp-draw']` pins exactly `(346,3,3,top_sha)`. With that measured
recipe, the first unexplained frame is **2064 (raw 4871)**. Capture 2064..2131
is the harness's 60-game-frame `STOP_TAIL` poll after the shared script end:
the capture's X record and the generated port script both end at `f=0x0B62`
(2914), and `f=0x0B9E` is the later SIGTERM. The replay has no frames for those
68 capture frames; they are not claimed. Captures 2058..2063 exhibit ports
1681..1685, so the window below 2064 includes
both mode `0x0A` and mode `0x0B`, and reach **1685** is the port dump's last
frame. The 11 canonical non-black missing classes through that reach are
`[9, 11, 92, 129, 132, 219, 220, 809, 1019, 1083, 1526]`.

| Pin | Measured value |
|---|---:|
| `GP_DRAW_MIN_FIRST` | 2064 |
| `GP_DRAW_TRACE_MIN_FIRST` | 2915 |
| `GP_DRAW_MOVES_MIN_FIRST` | 2915 |
| `GP_DRAW_MAX_START` | 100 |
| `GP_DRAW_MIN_REACH` | 1685 |
| `GP_DRAW_MAX_MISSING` | 11 |
| `GP_DRAW_CAPTURE_FRAMES` | 2132 |
| `GP_DRAW_CAPTURE_SHA256` | `d99571d3db0bf2135ad10d3a0f08d5c5bbdebdea392fad2821e7ce0901299885` |

The frame claim remains narrow: no content-bearing capture frame below 2064 is
unexplained, with capture 346 admitted only by its byte recipe; port-frame order
and complete interior coverage are not claimed, and nothing in the harness tail
is claimed.

`ad4`, `w2`, `w3` and `b1e` are not in `gp_session.TRACE_FIELDS`, so the trace
ratchet does not compare the port's draw result; only the capture-side
`gp_draw.py check` reads that result. The out-of-band port trace at
`f=0x0B02`/`f=0x0B26` also shows `ad4=2`, `w2=w3=1` and `b1e=3`, but that is
outside the enforced claim. The two captures' `0x0B` screens share the
winner-pose content, so the new claim is the KO branch (`ad4=2` from `0x27BA4`),
not new pixels.

### L2.4 Failing-direction proofs

Each `GP_DRAW_*` mutation below was a command-line override of
`make gp-draw-oracle`; every run failed as required and the file kept its
measured value. The exact-pin environment (`PR_PINS_EXACT=1`) makes the
lower-`MIN_REACH` and raised-`MAX_MISSING` mutations fail too.

| Mutation | Measured failure |
|---|---|
| `MIN_FIRST 2064 -> 2065` | `first unexplained 2064 < ratchet N 2065` |
| `TRACE_MIN_FIRST 2915 -> 2916` | `N 2916 > end 2915: N is unreachable` |
| `MOVES_MIN_FIRST 2915 -> 2916` | the same moves-side unreachable-N failure |
| `MAX_START 100 -> 99` | `window starts at capture 100 ... > pinned start 99` |
| `MIN_REACH 1685 -> 1684` | `window reach port 1685 > pinned reach 1684; re-pin to 1685` |
| `MAX_MISSING 11 -> 12` | `11 missing class(es) < pinned 12; re-pin to 11` |
| capture SHA's final hex digit `5 -> 6` | capture identity mismatch (2,132 frames) |
| `CAPTURE_FRAMES 2132 -> 2131` | capture identity mismatch (2,132 actual frames) |

A separate real-oracle proof changed the capture-346 top hash's final hex digit,
then restored it. The run failed with `allowed capture 346 ... is not its named
three-frame scan-out` and `first unexplained 346 < ratchet N 2064`.

### L2.5 Audio, manifest and gate

`prage_000.dro` exists, but is deliberately **outside**
`tools/audio_compare.py`'s 29-trace inventory: no section was measured and no
audio claim or section pin is fabricated. The regenerated manifest has 40
entries. `make gate-check` reads **39/40 captures present, one missing**, with
39 present checked and the same declared U14 absence. The comment-only
attribution correction re-pinned the reason as `gp-draw tail attribution
correction; comment only`; the digest remains
`b4b2d3b19322a9d24646820825c6e9cea8bfab6db21eac502b1de8a2fc602013` because no
marked pin region or pin value changed.

## §L3 The published two-human KO window (Task 3)

**Decision: `gp-twop-ko` is published and gated by `gp-twop-ko-oracle`.** The
capture is a two-human match and the KO is the game's own damage path: there
are no `W` records and no KO/death pokes. The oracle asserts the KO and round
result in the measured window; it does not claim a fought match win or that the
result naming is a fought win. U14's fought-live-win gap is unchanged. No port
code or game byte changed.

### L3.1 One guarded capture and the game-dealt KO

Exactly one `make gp-capture scenario=gp-twop-ko` ran after the preflight passed
(load 17.97 on 20 cores and a writable capture root). It wrote 2,565 frames
(raw 1427..5251) in 76.3 s, with 3,184 S records (`f=0x0005..0x0C8B`) and 23
missed frames. All 11 session checks passed: base; 105/166 steps with no skip
and the mode-`0x08` stop; end frame; mode `0x27` after Enter; `kb == raw`;
2,565/2,565 frames; port script v2; no unscripted input; 0/0 pokes; zero BIOS
words skipped by the 14-entry backlog guard; and the SIGTERM stop at
`f=0x0C8C`. `gp_capture.py check-input` is clean.

The two-human check passes from P2's join at `f=0x294`: 2,479 S records through
the X record at `f=0x0C50`, both sides on pad words throughout, and 493/29
fight presses. The KO check exits 0:

```
gp_twop: gp-twop-ko: hand-dealt KO of side 0: ko f=B63; round result f=B63; match result f=-; death-done f=-
gp_twop: gp-twop-ko: hand-dealt KO: ok
```

The deciding live transition is P2's `s1_5a` **`0x73` at `f=0x0B30` (still
`0x73` at `f=0x0B5F`) to `0x78` at the mode-8 record `f=0x0B63`**. Earlier
mode-6 records show the game's damage progression `0x00 → 0x12 → 0x19 → 0x20
→ 0x30 → 0x37 → 0x3E → 0x4E → 0x55 → 0x5C → 0x6C → 0x73`; P1's `s0_5a` stays
zero. At the result, `w2=1`, `w3=0`, `ad4=0xFFFFFFFF` and `b0c=0`. There is no
match-result or death-done record in this round-1 window. Capture identity is
2,565 frames and poll.log sha256
`15705a3f95c9c9333abcc8ca3c751a2fb0d5525b1556132bdee740eb2807a0ee`.

### L3.2 Replay and miss set

The first `make gp-replay scenario=gp-twop-ko` reached the script end but failed
only the not-yet-registered miss-set assertion. Its measured log was
`distinct=4 dropped=0`: the global `0x5D812` `actor_spawn`/`set_dead` rows plus
`0x29D60` and `0x5D812` from `frontend_mode_1b_step`, one hit each.
`k_miss_gp_twop_ko` pins the latter pair and its `k_gp_sets` row is exact-name
(prefix 0). The oracle replay then passes with the same four rows.

The 2,104-frame port dump (indices 0..2103) has its last presented frame at
`f=0x0C4F`; `f=0x0C50` is the trace/X record. It reaches the mode-8 KO/result
transition and the following mode-`0x16` screen, so the KO and result window
are both inside the replay. Trace and moves each compare 2,870 frames (22
without a capture snapshot are not compared) and have zero differences through
`f=0x0C50` (3152).

### L3.3 Measured scan-outs and frame window

The initial report's first unexplained frame was 272, before the KO, so that
raw report was not a publishable KO window. Byte analysis identified four
scan-out frames; each allowance binds its bytes to the current dump rather
than widening the general two-frame model:

- **272 (raw 2422):** the byte layout also matches ports 31/32/33, but that is
  the earlier occurrence, exhibited cleanly by captures 106/107/108. The
  timeline instead has capture 271 clean at port 172, capture 273 a
  `(177,178)` splice and capture 274 clean at 178, placing this scan-out at
  **175/176/177** (rows 0..13/14..57/58..199; the top's earlier holders are
  31 and 175). The layout repeats again at 222/223/224, so the existing
  unique-pair rule cannot choose one. A fifth, optional `SCAN3_ALLOWED` field
  pins measured occurrence **B=176**; the pair must still bind the same rows,
  and ordinary four-field entries keep their unique-pair requirement. The new
  unit test failed first on the missing five-field support, then passed,
  including rejection of a non-binding pin.
- **1286 (raw 3881):** rows 0..163 port 1042, rows 166..199 port 1043, with
  rows 164..165 the capture-pinned band; (1042,1043) is unique.
- **1291 (raw 3886):** rows 0..109 port 1046, rows 133..199 port 1047, with
  rows 110..132 the capture-pinned band; (1046,1047) is unique.
- **2286 (raw 4906):** rows 0..50 port 1896, rows 51..193 port 1899 and rows
  194..199 port 1900; (1899,1900) is unique and port 1896 is the only earlier
  holder of the top segment.

The two bands have no whole-present binding and their causes are not isolated;
their named bytes and bound segments are the allowance's limit. With those
recipes, the first unexplained capture is **2525 (raw 5184)**. Capture
2525..2564 is the harness's post-X `STOP_AT_END` tail: the port script and
trace end at `f=0x0C50` (the last presented frame is `f=0x0C4F`), while the
original polls on to `f=0x0C8C`. Nothing in that tail is claimed. Captures
76..2524 include the KO and result window. The window start is **76 (raw
1727)**, reach is **2103**, and the seven canonical non-black missing classes
are `[9, 11, 216, 222, 260, 1897, 1898]`.

| Pin | Measured value |
|---|---:|
| `GP_TWOP_KO_MIN_FIRST` | 2525 |
| `GP_TWOP_KO_TRACE_MIN_FIRST` | 3153 |
| `GP_TWOP_KO_MOVES_MIN_FIRST` | 3153 |
| `GP_TWOP_KO_MAX_START` | 76 |
| `GP_TWOP_KO_MIN_REACH` | 2103 |
| `GP_TWOP_KO_MAX_MISSING` | 7 |
| `GP_TWOP_KO_CAPTURE_FRAMES` | 2565 |
| `GP_TWOP_KO_CAPTURE_SHA256` | `15705a3f95c9c9333abcc8ca3c751a2fb0d5525b1556132bdee740eb2807a0ee` |

The frame claim remains narrow: no content-bearing capture frame below 2525 is
unexplained except the four named byte recipes; port-frame order and complete
interior coverage are not claimed. Trace/moves and `MIN_REACH` extend through
the KO record and result window, not through the harness tail.

### L3.4 Failing-direction proofs

Each `GP_TWOP_KO_*` mutation below was a command-line override of
`make gp-twop-ko-oracle`; every run failed as required and the Makefile kept
its measured value. The restored oracle is green.

| Mutation | Measured failure |
|---|---|
| `MIN_FIRST 2525 -> 2526` | `first unexplained 2525 < ratchet N 2526` |
| `TRACE_MIN_FIRST 3153 -> 3154` | `N 3154 > end 3153: N is unreachable` |
| `MOVES_MIN_FIRST 3153 -> 3154` | the same moves-side unreachable-N failure |
| `MAX_START 76 -> 75` | `window starts at capture 76 ... > pinned start 75` |
| `MIN_REACH 2103 -> 2104` | `window reach port 2103 < pinned reach 2104` |
| `MAX_MISSING 7 -> 6` | `7 missing class(es) > pinned 6` |
| `MAX_MISSING 7 -> 8` with `PR_PINS_EXACT=1` | `7 missing class(es) < pinned 8; re-pin to 7` |
| capture SHA's final hex digit `e -> f` | capture identity mismatch (2,565 frames) |
| `CAPTURE_FRAMES 2565 -> 2564` | capture identity mismatch (2,565 actual frames) |

`tools.tests.test_gp_compare` passes all 92 tests, and
`tools.tests.test_gp_twop` passes all 54 in the oracle.
`PR_PINS_EXACT=1 make gp-twop-ko-oracle` passes with the exact miss set and all
eight pins.

### L3.5 Audio, manifest and gate

`prage_000.dro` exists, but is deliberately **outside**
`tools/audio_compare.py`'s 29-trace inventory: no section was measured and no
audio claim or section pin is fabricated. The regenerated manifest has 41
entries. `make gate-check` reads **40/41 captures present, one missing**, with
40 present checked and the same declared U14 absence. The measured pin-baseline
reason is `twop-ko scan-out occurrence correction; measured pair 176/177 and
seven missing classes`, digest
`095baa1a398032d98895858e9eb840f5410f846efb6ab13237e24b1386a03f17`.

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

## §L5 The continue screen, countdown and expiry (Task 5)

**Decision: `gp-u28-continue` is published and `gp-u28-continue-oracle` is in
`make verify`.** One guarded capture passed every session check and the new
`tools/gp_continue.py check`. The claim is the continue screen's credit-ready
PRESS START prompt, its complete countdown and its expiry. The three KOs are
labelled harness pokes; neither a fought KO nor a taken continue is claimed.

### L5.1 Raw route, the side rule and the measured countdown

`flow_arena_ko_check` (`0x272DC`, `flow.c:1810-1827`) reads the incumbent side
`w = (s8)DS_0010810D`. If **that side's** `+0x5A` is at least `0x78`, it calls
`flow_continue_open` (`0x278B0`) and opens mode `0x0E`; if only the other side is
KO'd, it takes the mode-`0x0D` winner-pose arm. This is the side rule the
state-coverage declaration's "opens on a KO" omitted. The capture verifies
`w10d=0`; it does not assume the brief's side.

`SCENARIOS['gp-u28-continue']` reuses a new tuple sliced from
`gp-u10-ending`'s LEFT PLAYER ARCADE route through the two match-1 KOs, then
applies `KO_P1` (`0x10780A=0x78`) ten frames into the first mode-`0x0C` entry.
`WIN_MENU`, `KO_P1` and `gp-u10-ending` are not mutated. Boot's five credits leave
four after the arcade start, so there is no credit poke. The default 16 MB guest
memory is sufficient: the scenario stops before the fourth-final OOM that
justifies U10's `memsize=64` (§W.13), and the harness's memsize whitelist stays
unchanged. `stop_mode=0x15`, `stop_delay=0` puts the whole countdown in the
replay script.

The checker and the capture's S/W records measure:

| Claim | Capture evidence |
|---|---|
| labelled incumbent KO | `W f=0x14BF addr=0010780A now=78`; mode `0x0E` opens at `f=0x14C0` with `w10d=0` |
| no taken continue | `b1f=0` throughout mode `0x0E`; `flow_continue_take` (`0x2791C`) is not called |
| credits unchanged | `cred=4` throughout the screen and at expiry; no credit write |
| complete countdown | `cnt=0x0F` at `f=0x14C0`, then `0x0E@0x1500`, `0x0D@0x1540`, …, `0x01@0x1840`, `0x00@0x1880`: every value 15→0, 64 master frames apart |
| PRESS START path | first at `cnt=0x0D`, `f=0x1540`: the raw gate is `cnt < 0x0E` (`flow.c:2016`), and `cred=4` selects `prompt_press_start_blink` (`0x2C0F4`), not INSERT COIN |
| expiry | `f=0x18C2 mode=0x15 b25=0 cnt=0xFF` (`0x27AE5..0x27B1C`) |

The first PRESS START value is **13, not 14**: `0x27B59 cmp eax,0xE; jge` skips
the prompt at 14. A test asserting the raw threshold failed against the initial
checker and passed after the correction. `gp_continue.py path` compares only
`gp_trace_line` fields (`mode`, `w10d`, `cnt`, `cred`, `b1f`, `b25`) and reproduces
**17/17 milestones** at the capture's frames. The prompt's pixels are the frame
oracle's claim, not a state-field invention.

The task brief predicted that `0x2791C` should improve in the census. The raw
call graph says otherwise: `0x2791C` is `flow_continue_take`, and this capture
deliberately expires without taking the continue. Only `0x27A2C` and `0x278B0`
should become HIT in Task 12's re-measurement; `0x2791C` stays NOT-HIT with its
reason named here. No coverage baseline is changed in this task.

### L5.2 The one capture and acceptance

Command, run exactly once with host load 5.82 on 20 CPUs:

```
make gp-capture scenario=gp-u28-continue
```

The published capture is `data/k11-captures/gp-u28-continue`: **4,621 frames**
(raw 1387..8985), 6,369 snapshots (`f=0x0005..0x18FB`, 22 game frames missed by
the documented poller), wall 128.9 s, and poll.log sha256
`96645e99a3718f5c387c6613d2faddaa8221ad936ea8c80f82a3dd602e51246e`.
All session checks passed: base; 7/7 steps fired and stopped on mode `0x15`; end
frame reached; mode `0x27` after Enter; `kb == raw`; 4,621/4,621 frames written;
port script v2; no unscripted input; 4/4 pokes written with zero races; zero
skipped BIOS words; stopped at the end. `check-input` independently prints
`check=ok no unscripted input`. The X record and generated script end at
`f=0x18C2` (6338); the later SIGTERM is `f=0x18FC` after the harness's
`STOP_TAIL=60` poll. No `.failed` artifact and no second capture exists.

### L5.3 Replay and miss set

The first replay reached the script end and failed only the expected
not-yet-registered miss-set assertion. Its measured rows were the global
`0x5D812 actor_spawn` (3,863 hits) / `set_dead` (3,258) pair and
`0x29D60`/`0x5D812` from `frontend_mode_1b_step` (1/3 hits, `distinct=4`).
`k_miss_gp_u28_continue` and its exact-name `k_gp_sets` row pin the two hook
pairs; the replay then passes. The dump has **3,844 presented frames**. Port
3549 is the first mode-`0x0E` frame (`f=0x14C0`); port 3843 is the last
(`f=0x18C0`, mode `0x15`). Trace and moves have zero differences through 6338.

### L5.4 Frame measurement and pin proofs

The initial frame report stopped at capture **336** (raw 2825). Its bytes are the
round-1 mode-8 entry's three-present scan-out, not a continue-screen divergence:

- rows 0..1 equal port 216, the only earlier holder;
- rows 2..9 equal port 218 and rows 10..199 equal port 219;
- `(218,219)` is the dump's unique adjacent-pair bind; port 217 is unexhibited;
- top sha256 is
  `2d6fb6ad829baae61c65a844fb44f05cb51393322afdce4ab8117960e1306ddd`.

Only `SCAN3_ALLOWED['gp-u28-continue']` gained that measured recipe. With it,
the first unexplained frame is capture **4596** (raw 8922), in the post-script
STOP_TAIL; all prompt/countdown/expiry content is below it. The final report has
window start **96** (raw 1738), reach **3843**, zero order failures over 4,485
classified frames, and **17** canonical non-black missing classes:
`[9, 29, 31, 91, 127, 217, 1039, 1040, 1103, 1104, 1525, 1792, 1793, 2005,
2006, 2093, 3362]`.

Measured pins:

| Pin | Value |
|---|---:|
| `GP_U28_MIN_FIRST` | 4596 |
| `GP_U28_TRACE_MIN_FIRST` / `GP_U28_MOVES_MIN_FIRST` | 6339 / 6339 |
| `GP_U28_MAX_START` | 96 |
| `GP_U28_MIN_REACH` | 3843 |
| `GP_U28_MAX_MISSING` | 17 |
| `GP_U28_CAPTURE_FRAMES` | 4621 |
| `GP_U28_CAPTURE_SHA256` | `96645e99a3718f5c387c6613d2faddaa8221ad936ea8c80f82a3dd602e51246e` |

Every pin was moved one step in its failing direction and restored:

| Mutation | Observed failure |
|---|---|
| `MIN_FIRST 4596 -> 4597` | `first unexplained 4596 < ratchet N 4597` |
| `TRACE_MIN_FIRST 6339 -> 6340` | `N 6340 > end 6339: N is unreachable` |
| `MOVES_MIN_FIRST 6339 -> 6340` | the same unreachable-N failure on moves |
| `MAX_START 96 -> 95` | `window starts at capture 96 (raw 1738) > pinned start 95` |
| `MIN_REACH 3843 -> 3842` | `window reach port 3843 > pinned reach 3842; re-pin to 3843` |
| `MAX_MISSING 17 -> 18` with `PR_PINS_EXACT=1` | `17 missing class(es) < pinned 18; re-pin to 17` |
| capture SHA final digit `e -> f` | capture identity mismatch |
| `CAPTURE_FRAMES 4621 -> 4620` | capture identity mismatch (4,621 actual frames) |

The real capture-336 top hash was also changed by one hex digit and restored;
the comparison failed with `allowed capture 336 ... not its named three-frame
scan-out` and `first unexplained 336 < ratchet N 4596`. The synthetic recipe
unit test rejects one mutated bound byte. Logs are in the git-ignored ledger as
`task-5-pin-*.log` and `task-5-scan3-hash-mutation.log`.

### L5.5 Audio, manifest and verification

`prage_000.dro` is deliberately **outside** `tools/audio_compare.py`'s 29-trace
inventory: no audio section was measured, so no section minimum or audio claim
is fabricated. `tools/audio_compare.py` is unchanged. The regenerated manifest
has **42 entries**. `make gate-check` reads **41/42 captures present, one
missing**, with 41 present checked and the same declared U14 absence. The
measured pin-baseline reason is `live truth Task 5: measured gp-u28-continue
oracle and capture-336 scan-out`, digest
`adffb4c97366215f2e4e3ed0e5a07b56c3eb275f156c7c492f7a3b94cc2a9bf7`.

`PR_PINS_EXACT=1 make gp-u28-continue-oracle` is green, including the evidence
checker, replay/miss set, frame/order/coverage, trace/moves and the 17-milestone
path. The affected Python suites (`test_gp_continue`, `test_gp_compare`,
`test_gp_session`, `test_gp_capture`, `test_capture_manifest`) pass **263 tests**.
No game or port implementation file changed, no other capture or oracle pin
moved, and the full `make verify` remains Task 13.

## §L6 The mode-0x25 bowling route: measured failures and a structural blocker (Task 6)

**Decision: no bowling capture, oracle, pin or census claim is published.** The
first one-attempt route reached `b892 = 2`; the review-corrected three-attempt
route's last sampled value was **5**. Neither opened mode `0x25`. The artifacts
are preserved as `data/k11-captures/gp-bowling.failed` and
`data/k11-captures/gp-bowling-fix.failed`. The current symmetric route is
structurally blocked by slot 0's `+0x53`, as derived below. **No fourth execution
of that route is authorized and no more captures are to be run.** A staggered
route would be a new, unmeasured task, not a continuation of this one.

### L6.1 The decoded banner, gates and move sequence

The shipped localisation table was loaded through `game_string_table_load` and
read through the game's own `game_string_get` in a debugger at
`check_localised_string` (`test_game.c:83`). The measured result is

```
game_string_get(0x61) -> "BOWLING!"
```

The state-coverage label "both-characters-4 camera" is therefore wrong; the
entry is the bowling easter egg. `fight_mode25_enter` (`0x4E11C`,
`fight.c:5578-5624`) draws that string at column -1, row 6, mode `0x4000`.

The complete entry gates are `DS_001078FA == 2`, `DS_00108892 >= 6`, slot 0's
`+0x53 == 0` (`flow.c:3228-3231`), then `DS_00104B1D` not 2/3, signed camera
`DS_000F0AF0` within `[-0x3300,0x3300]`, signed `DS_00104AD8 <= 0`, unsigned
`DS_00104ABC >= 2`, `DS_0010782A == DS_001078BE == 4`, and
`DS_001088C4 == 0` (`fight.c:5581-5597`). `fighter_45158` calls `0x18C14`
with flag 0 = 1, flags 1 and 8 = 0, and every other flag 2; flag 5's
`hit_geometry` is therefore **skipped**, not part of this trigger. The zero
result requires the `DS_00100AF8[side]` latch above zero (flag 0's inverted
form), the other record's `+0x74 == 0` and unsigned `+0x76 <= 1`, and its
`+0x42` bit 3 clear (`fighter.c:8211-8226`, `8271-8276`). `fighter_19020`
writes that latch to 1 after a zero hook result (`fighter.c:8338-8351`). Only
then, outside mode `0x25` and with the other slot at character 4 /
`+0x5F == 0x22`, does `fighter_45158` increment the word
(`fighter.c:9839-9842`). The reaction callback is character 4's `0x455A0`
(`fighter.c:10004-10032`).

That callback also creates the structural blocker. It installs the increment
hook at slot `+0x18 = 0x45158` and, in the same activation, writes slot `+0x53 =
7` at `0x455C3` and `+0x52 = 9` at `0x455C7` (`fighter.c:10010-10028`). The
caller gate requires slot 0's `+0x53 == 0` (`flow.c:3228-3231`, `0x263DE`), but
fighter states 9 and 10 are epilogue no-ops (`fight.c:2258-2260`), so they do not
clear `+0x53`. It therefore stays 7 on every frame on which the reaction-`0x22`
increment hook is active. The symmetric route cannot open `0x25`, regardless of
whether the sampled counter stopped at 5 or reached 6 and was rejected. A
staggered route would change the state coupling and is a new, unmeasured task.

The S fields do not include the four remaining unmeasured gate inputs: camera
`DS_000F0AF0`, `DS_00104AD8`, slot 0's `DS_00107803` (`+0x53`), or
`DS_00104ABC`. They were not added to `SNAP_FIELDS`. The `+0x53` blocker above
is a raw store/control-flow derivation, not a claim that its value was sampled;
the mode entry would prove that the raw passed all four gates when the count
reached 6, not what the bytes were on earlier frames.

`gp_moves.py list --char 4` decodes move 0 as reaction `0x22`, callback
`0x455A0`, phases `20000/08000/00000/3`, the repeated
`20000/08000/00000/3`, `04000/00000/08000/15`, and
`80500/04A00/28000/15`. With the generator's four-frame harness step, P1's
facing-0 presses are right (hold 4), b0+b2 (hold 12), down (4), left (4); P2's
facing-1 mirror is left, b0+b2, down, right. The scenario clones only
`gp-twop`'s route object by tuple prefix (it does not mutate `gp-twop`), joins
P2, moves P1 right five times from cell 0 to cell 5, leaves P2 on default cell
5, and confirms both class-4 fighters. The corrected route restores `gp-twop`'s
separate 30-frame walk-in, then plays the complete move three times on both
sides. The first failed attempt showed both `+0x52` bytes back at 0 at
`f=0x636`, 139 frames after its move's spin at `f=0x5AB`; attempts therefore
start 150 frames apart (142 frames after each attempt's last direction step),
the next 30-frame harness gap.

`SNAP_FIELDS` and the port test's T record append `b892` (`0x108892`, word),
`b8c4` (`0x1088C4`), `b8a6` (`0x1088A6`, word), `b8bd` (`0x1088BD`), `b415`
(`0x104B15`) and `b8c0` (`0x1088C0`), in that order. `game_mode_04_step` runs
`fighter_pass_a` before `fight_mode25_enter`; an increment crossing 6 and the
entry's `b892 = 0` reset (`0x4E1CB`) can share one iteration, so a frame-boundary
snapshot need not show that peak. The third pair's increments themselves remain
visible. The corrected checker requires at least three rising edges into the
callback animation state `s*_52 = 9` per side (not an attempt count),
non-decreasing visible counter growth to 4 except for one `-1` immediately after
a value >=6
(the `0x4E131`/`0x4E189` rejection arm), then the entry's `b892 = 0`, `b1f=3`,
`b1d` not 2/3, `c0=c1=4`, `b25=0`, `b8c4` 0 -> 1, `b8a6=0xB4`, `b8bd=b415=0`,
and `b8c0=1`. The raw gate is evidenced by the entry stores plus that growth,
never by requiring an impossible sampled 6. Any `W` record rejects the trigger.
S/T carry no text row, so the string is the decoder evidence above and the banner
is a frame claim, not an invented poll field.

### L6.2 The first failed capture and its exact checks

`make gp-capture scenario=gp-bowling` ran once for 90.7 s. It wrote **3,512
frames** and **4,161 S records** (`f=0x0005..0x104A`) to
`gp-bowling.failed`; all 15 scripted actions fired. The route reached the fight
with `b1f=3`, `b1d=0`, `c0=c1=4`, and both `r0/r1` applied `0x22` from
`f=0x05B5`. But `b892` changed only at `f=0x05C2`, from 0 to **2**, and stayed
2 through the last record; `b8c4` stayed 0. The last-snapshot rising-edge count
is **2 per side** for `s0_52`/`s1_52` reaching 9. This is callback-animation
state evidence (about two edges per move execution), not an attempt count. The
mode set is `{03,05,06,10,11,17,1A,1B,27}` — no `0x25`.

The capture's session verdicts are:

```
check=ok steps fired 15/15 (0 skipped)
check=FAIL end frame reached
check=FAIL port script v2 (the scenario end (X record) was not reached)
check=ok no unscripted input
check=ok pokes written 0/0, 0 raced
check=ok capture pins: 0 BIOS words skipped (backlog over the 14-entry cap)
check=FAIL stopped at the end (SIGTERM at f=-, rc=0)
```

The independent input audit passes (`check=ok no unscripted input`). The new
checker exits 1 with the exact line

```
gp_bowling: gp-bowling.failed: evidence: FAIL: mode 0x25 never observed
```

There is no X record and no accepted replay from this attempt. The initial
commit's subject was amended to `gameplay: measure the 0x25 bowling route`
before the review-fix work, so the history does not claim a capture that never
happened.

### L6.3 Review fixes and the corrected failed capture

The review identified two independent defects: the one-attempt scenario could
not supply three qualifying frames, and the checker required a sampled 6 that
the entry/reset makes unobservable. The corrected scenario and checker above
were test-driven: the new success fixture has only visible `b892 = 4` and passes;
a drop below 4, a decrease other than the one permitted rejection, or fewer than
three state-9 rising edges on either side fails. The edge detector uses the last
snapshot before the current frame, not necessarily `f-1`. `gp-bowling-fix` shares
the corrected immutable steps tuple; `gp-bowling.failed` remains the original
evidence.

`make gp-capture scenario=gp-bowling-fix` then ran **once**, for 90.6 s. It
wrote **3,434 frames** and **4,159 S records** (`f=0x0005..0x104A`) to
`gp-bowling-fix.failed`. All 24 actions fired, with no poke and no unscripted
input. Both reactions appeared at `f=0x05F4`. The last-snapshot rising-edge count
is **6 per side** for `s0_52`/`s1_52` reaching 9, again about two per move
execution and not an attempt count. The counter measured

```
f=05F9 b892=1    f=069C b892=3    f=0732 b892=5
```

Thus the three qualifying frames contributed +1, +2 and +2, with **5 the last
sampled value**. The trace cannot distinguish "stopped at 5" from "reached 6 and
was rejected": the `0x4E131`/`0x4E189` rejection arm decrements in the same
iteration, so either history has the same frame-boundary samples. No mode entry
occurred in either case. Independently, the `+0x53 = 7` coupling makes the entry
gate structurally closed on this symmetric route. `b8c4` stayed 0 and the mode
set remained `{03,05,06,10,11,17,1A,1B,27}`. The inner camera `DS_000F0AF0`,
`DS_00104AD8`, `DS_00104ABC` and `DS_00107803` values were not sampled, as §L6.1
declares. The checker exits 1 with

```
gp_bowling: gp-bowling-fix.failed: evidence: FAIL: mode 0x25 never observed
```

The session checks again failed only the end-dependent trio:

```
check=FAIL end frame reached
check=FAIL port script v2 (the scenario end (X record) was not reached)
check=FAIL stopped at the end (SIGTERM at f=-, rc=0)
```

Base, all 24/24 steps, the mode-`0x27` arm, `kb == raw`, 3434/3434 frames, no
unscripted input, 0/0 pokes with no race, and zero skipped BIOS words passed;
the independent input audit also passes. There was no replay, miss-set
measurement, ratchet, `k_miss_gp_bowling*`, `GP_BOWLING*` pin, `CAPTURE_DIRS`
entry, manifest or pin-baseline change, audio-inventory addition, or census
improvement. Neither failed `.dro` has a measured section or audio claim. No
game or port implementation code changed; the only C change remains the test
driver's appended T-record fields. Mode `0x25` stays declared. No further capture
was made, and no fourth execution of the current symmetric route is authorized.

### L6.4 Structural-blocker review-fix report (2026-10-30)

The review withdrew any inference that the symmetric route was one execution
short. The checker now permits exactly one `-1` immediately after a counter value
>=6, labels the `s*_52 = 9` rising edges as callback-animation state rather than
attempts, detects those edges against the last earlier snapshot, and retains all
entry-store, real-input and no-`W` checks. A real-capture-shaped `0,1,3,5,6,5`
fixture passes with a valid entry and still fails with `mode 0x25 never observed`
when the entry is removed; a sparse-snapshot fixture catches the old `f-1`
lookup. The new tests failed before the checker fix, then `test_gp_bowling`
passed **14 tests** and the combined `test_gp_bowling`/`test_gp_session` run
passed **102 tests**. Both preserved artifacts still exit 1 with the exact
`evidence: FAIL: mode 0x25 never observed` verdict. No capture, replay, oracle,
pin, `CAPTURE_DIRS` or manifest entry, `SNAP_FIELDS` field or port code was added;
both `.failed` artifacts remain untouched.

## §L7 The name-entry gated oracle (Task 7)

**Decision: `gp-u27-name-entry` is now a gated oracle in `make verify`.** No capture or
port code changed, and the existing `k_miss_gp_u27_name_entry`/`k_gp_sets` row was already
sufficient (the replay reports the same four rows, `distinct=4 dropped=0`). The capture is
`data/k11-captures/gp-u27-name-entry`: 9,417 S records, 5,885 frames, poll.log sha256
`5e2c5824ee009d814c1929663110c22b24d658e4b0ea0c440a4b3327eeb79d04`. The fresh replay dumps
4,991 frames and stops in mode 3 at `f=0x24B9`.

### L7.1 Capture and port path checks

`tools/gp_u27.py check` reads these measured S records (the name dword is little-endian
memory order, so `0x20444241` is the bytes `41 42 44 20`, or `"ABD "`):

| Claim | Measured records |
|---|---|
| mode-`0x1E` sub-states | `b25 00@0x217E -> 04@0x217F -> 05@0x2183 -> 06@0x22CE -> 07@0x23FC -> 0A@0x23FD` |
| three letter counts | `ncnt 1@0x2193, 2@0x21B0, 3@0x21CE` |
| three letters and finaliser | first `name=00000041@0x220D`, `00004241@0x2226`, `00444241@0x2240`, `20444241@0x2243` |
| score | `sc0=005217A0` on every checked row |
| return | `mode=3`, `b25=0`, final name and score at `f=0x24B9` |

The `06` and `07` rows are mode-`0x1E` re-entry samples, not the name entry's first writes of
those `b25` values. The name entry's writes are not a global first-transition claim: the
capture's first `b25=06` and `b25=07` records are `f=0x0CA0` and `f=0x0CA1`, both in mode
`0x12`, before the name entry opens (`poll.log` lines 3250-3251), and the entry's own `06`
and `07` writes occur during mode `0x15`. The checker therefore pins the exact records rather
than inventing a global first-transition rule. `path` compares only the six
`b25` values at the same capture frames; the port trace does not carry `ncnt`, `cell0` or
`name`. On the real capture/replay both commands exit 0: all capture rows pass and all
`6/6` port sub-states reproduce. `tools/tests/test_gp_u27.py` runs six tests covering the
sub-state sequence, all three count/name stages, the padded finaliser, and capture/path
failure cases. It erred on the missing module before implementation and passes after it.

### L7.2 Capture 334 allowance

Without an allowance the first unexplained content frame is capture **334 (raw 2837)**, the
round-1 mode-8 entry's three-present scan-out. On the 4,991-frame dump:

- rows 0..2 equal port **219** (`f=0x47C`), the only holder before the pair;
- rows 3..62 equal port **221** (`f=0x47E`);
- rows 63..199 equal port **222** (`f=0x47F`);
- `(221,222)` is the dump's unique adjacent-pair bind, and port 220 (`f=0x47D`) is
  unexhibited.

The measured top-row sha256 is
**`f1839ec475ffc5228cb673caafb0ac274e3221299b50e1be804ae5da887aef1b`**.
The recon's `3f1839ec...` prefix was wrong; the measurement wins.
`SCAN3_ALLOWED['gp-u27-name-entry'] = ((334,3,63,f1839ec...),)` is the resulting recipe.
The new unit test pins that tuple and fails a one-byte middle-frame mutation. A second,
real-dump proof changed one byte at index `3*320+17` of a hard-linked scratch copy's
`frame_00221.ipx`: the comparator returned 1, reported `no unique adjacent port pair`, and
moved the first unexplained frame back to 334. The original dump was not modified.

### L7.3 Measured pins and finaliser coverage

All values were re-measured on the fresh replay; the recon values were otherwise correct.
With capture 334 allowed by name, the comparator reports 3,438 clean, 2,320 splice, eight
transition, one scan3-allowed and one unexplained frame, with 23 all-black frames and zero
order failures.

| Pin | Measured value | Evidence |
|---|---:|---|
| `GP_U27_MIN_FIRST` | **5871** | first unexplained capture 5871 (raw 13662), the post-script tail |
| `GP_U27_MAX_START` | **81** | window starts at capture 81 (raw 1746) |
| `GP_U27_MIN_REACH` | **4990** | highest exhibited port index, the 4,991-frame dump's last frame |
| `GP_U27_MAX_MISSING` | **38** | 38 canonical non-black missing classes through port 4990 |
| `GP_U27_TRACE_MIN_FIRST` | **9402** | 0 differing through 9401 (`f=0x24B9`) |
| `GP_U27_MOVES_MIN_FIRST` | **9402** | 0 differing through 9401 |
| `GP_U27_CAPTURE_FRAMES` | **5885** | stored capture frames |
| `GP_U27_CAPTURE_SHA256` | **`5e2c5824...b79d04`** | full value above |

The frame window includes mode `0x1E` and the finaliser. The replay dump holds
**178** mode-`0x1E` port frames. The Task 7 record's **258 scene-total figure and
its mode split are withdrawn as not re-derivable**. The reproducible finaliser
claim is the oracle's: all **303 content-bearing capture frames 5311..5616** are
explained (255 clean and 48 splice). The oracle keeps the usual narrow
existential/splice and bounded-missing claims, not full port-frame order or
coverage.

### L7.4 Failing-direction proofs

Each real-comparator run below used the same fresh dump and returned **1**; the logs are in
the git-ignored Task 7 ledger directory. No pin was weakened or left changed.

| Mutation | Measured failure |
|---|---|
| `MIN_FIRST 5871 -> 5872` | `first unexplained 5871 < ratchet N 5872` |
| `MAX_START 81 -> 80` | `window starts at capture 81 ... > pinned start 80` |
| `MIN_REACH 4990 -> 4991` | `window reach port 4990 < pinned reach 4991` |
| `MAX_MISSING 38 -> 37` | `38 missing class(es) > pinned 37` |
| `TRACE_MIN_FIRST 9402 -> 9403` | `N 9403 > end 9402: N is unreachable` |
| `MOVES_MIN_FIRST 9402 -> 9403` | `N 9403 > end 9402: N is unreachable` |
| capture SHA's last hex digit `4 -> 5` | capture identity mismatch; re-capture invalidates the pins |
| `CAPTURE_FRAMES 5885 -> 5884` | capture identity mismatch (5,885 actual frames) |
| one bound byte of real port 221 | capture 334 loses its unique pair and is unexplained |

A Makefile-level `GP_U27_MIN_FIRST=5872` run also failed with
`first unexplained 5871 < ratchet N 5872`; restoring the file's measured 5871 and rerunning
`make gp-u27-oracle GP_WIN_KEEP=1` exited 0 with every pin exact, the evidence and path
checks green, and the existing miss set unchanged. The Python run
`python3 -m unittest tools.tests.test_gp_compare tools.tests.test_gp_u27` is
`Ran 97 tests ... OK`. The manifest now reads **38 present checked, 1 declared skipped**;
the declared absence remains U14. The measured pin-baseline reason is
`live truth Task 7: measured gp-u27 name-entry pins and capture-334 scan-out`, digest
`74896d14dc0075f38d7ea4e8a201f8db9eb41fc89394d5b75b590ce27c560308`.

## §L8 The one-human fought KO: game evidence only (Task 8)

**Decision: the failed U14 artifact is game evidence for a fought KO, not a capture, and
U14 R1 stays declared.** `gp_twop.py check --ko` required `b1f == 3` (a two-human match).
`--arcade` replaces that clause with a one-human arcade round (`b1f` 0 or 1 before the X
record, the KO's own record `b1f == 1`, no join). The WIN-field, no-KO-poke,
live-to-`0x78`, no-timeout and result-naming conditions are unchanged. No oracle, pin,
capture or port code moved.

### L8.1 Predicate and tests

`tools/gp_twop.py` `ko(lines, winner, arcade)` and `--arcade` (requires `--ko`).
`tools/tests/test_gp_twop.py` (TestKO): a one-human log passes `--arcade` and fails the
two-human claim; a two-human log fails `--arcade` (`b1f=3`); a KO poke fails `--arcade`;
the CLI passes a one-human KO. RED first: the new tests erred (no `arcade` parameter, no
`--arcade` flag). The `test_gp_twop` run is `Ran 54 tests ... OK` after the change.

### L8.2 The read-only run on the artifact

```
$ python3 tools/gp_twop.py check --ko --arcade --winner 1 --capture data/k11-captures/gp-u14-win-live.failed
gp_twop: gp-u14-win-live.failed: hand-dealt KO of side 1: ko f=898; round result f=898; match result f=F9F; death-done f=-
gp_twop: gp-u14-win-live.failed: hand-dealt KO: ok
(exit 0)
$ python3 tools/gp_twop.py check --ko --winner 1 --capture data/k11-captures/gp-u14-win-live.failed
gp_twop: gp-u14-win-live.failed: FAIL: b1f never reaches 3 (no S record has both sides human)
gp_twop: gp-u14-win-live.failed: hand-dealt KO: FAIL
(exit 1)
```

The `--winner 1` is the side that won: the KO'd fighter is side 0 (`s0_5a`).
The S rows, read from `poll.log`:

| Frame | Mode | `b1f` | `s0_5a` | `s1_5a` | `ad4` | `w2` | `w3` | `b0c` |
|---|---|---|---|---|---|---|---|---|
| `0x894` | 6 | 01 | `71` | `19` | `FFFFFFFF` | 00 | 00 | 00 |
| `0x898` | 8 (round result) | 01 | **`78`** | `19` | `FFFFFFFF` | 00 | 01 | 00 |
| `0xF9F` | 9 (match result) | 01 | `78` | `5C` | `00000001` | 00 | **02** | 00 |

- `ko f=898`: the first record with `s0_5a = 0x78`, in the round result (mode 8), after
  the live `0x71` at `0x894` (the §F3.2 shape, measured).
- `b0c`: 0 in all 9,471 S rows, so `death-done` is `-`. A hand-dealt KO leaves the
  death animation to run, which is why `b0c` stays 0 here.
- No `W` record (0), no `X` record (0). `poll.log` is 9,471 S rows and no X.
- `b1f`: 01 in 2,913 rows (modes 5 to `0x1B`, the round and the match results
  included), 00 in the rest, never 03 (`grep -c ' b1f=03'` is 0).

### L8.3 The four session failures (`session.txt`, measured)

The artifact's own `session.txt` records these as `FAIL`:

1. `end frame reached`: the capture did not reach its scenario's end frame.
2. `port script v2`: the key `70` (`p1.b3`) at `f=69D` was not queued (the BIOS ring
   was full), so the port replay of the script does not match.
3. `unscripted input`: `0` key words, `2` pad presses, `6` pad releases, first at
   `f=71A`, so input the harness did not inject reached the game.
4. `stopped at the end`: the SIGTERM at `f=-`, `rc=0`, so the run did not stop on its
   own end record.

These are why it is not a capture. Its `S` rows are measured data; the claim below
rests only on them.

### L8.4 The claim

**Game evidence, narrow:** the game's own damage path (`0x27FA8`'s KO value `0x78`, no
poke) KO'd fighter 0 in a one-human arcade round, and the round and match results name
side 1 (`w3` 1, then 2; `ad4` 1). The claim is about the KO and the result it produced.

**Not claimed:** the fought win, a published capture, an oracle, a pin, a miss set,
`b0c` (0 throughout), the death animation, the rest of the match, or any port
behaviour beyond the KO's record. The win stays the declared gap (`GP_U14_*` empty,
`GATE_NAMED_GAPS` keeps `gp-u14-win-live`).

### L8.5 Option (b) is rejected

§2.7 of the design named two options for U14's rule. Option (a) is this section. Option
(b) moves the rule to the final, which needs `b0c = 1`. The only direct store of a
non-zero `b0c` is `0x37FF6` in `0x37EA0` (§E1.3, the death-animation-end opcode `0x11`).
The U10 final that does write `b0c=1` (`f=0x1602`) runs with all seven final KOs
poked, so `b0c=1` there does not show a fought KO. Taking option (b) would move the
claim onto a poked final, and the rule would not separate a fought KO from a poke, so
it is rejected.

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

## §L10 The 5120 DAC-injection instrument gap (Task 10)

**Decision: Task 10 was not taken; the 5120 band stays INCONCLUSIVE.** The fade
palette source was not funded, so no DAC-injection experiment ran and no new
capture or diagnostic was made. This is an instrument gap, not evidence for
either the capture-pipeline or content-divergence hypothesis. No cause is
fitted. The byte-pinned allowance `(5120, 1, 5, 56b900b1…)`, its bounds and
every pin are unchanged, as is the state-coverage §D4 finding that the DOSBox-X
memory file excludes the VGA aperture.

## §L11 The diff-allowed audio matched prefixes (Task 11)

**Decision: all 74 diff-allowed sections in the existing 27 reports have a
falsifiable matched prefix, so all 74 are pinned.** This is tool-only: no capture,
port code or Makefile change. The claim is the leading run of matched tick groups
already computed as `al.claimed`, not the whole differing section. The first
difference and everything after it stay a named gap.

### L11.1 The measured per-section minima

The inputs were the existing `/tmp/pr_audio_dump/<scenario>/audio.txt` dumps and
their `.dro` captures, with no replay. Re-running the comparator over those dumps
measured **74 prefixes / 29,656 ticks**. `TRACE_PREFIX_PINS` in
`tools/audio_compare.py` records each minimum by scenario and the port dump's
original section index. Each row below is `section/bank=measured ticks`; the shipped
pin equals that measurement in every row.

| scenario | section/bank=matched-prefix ticks |
|---|---|
| `gp-idle-loss` | 3/BF1C78E2=30; 9/8FE546D4=960; 10/19AB8B2B=301 |
| `gp-idle-loss-run2` | 3/BF1C78E2=30; 5/E3533961=5; 6/BF1C78E2=20; 9/8FE546D4=960; 10/19AB8B2B=303 |
| `gp-keys-fight` | 3/BF1C78E2=30 |
| `gp-u10-ending` | 5/E3533961=5; 8/42BECD5B=932; 12/B40FF4A6=120; 13/15B3F30E=1437; 14/15B3F30E=826; 15/19AB8B2B=118 |
| `gp-u12-endurance` | 3/3D536DDE=180 |
| `gp-u13-ending2` | 5/E3533961=5; 8/42BECD5B=932; 12/B40FF4A6=120; 13/15B3F30E=1435; 14/15B3F30E=827; 15/19AB8B2B=301 |
| `gp-u16-twop2` | 5/E3533961=5; 8/E3533961=5 |
| `gp-u18-talon-ending` | 5/E3533961=5; 6/87E7C2DE=28; 8/42BECD5B=933; 9/94C63685=581; 12/B40FF4A6=120; 13/15B3F30E=1435; 14/15B3F30E=827; 15/19AB8B2B=302 |
| `gp-u19-vertigo-ending` | 3/3D536DDE=180; 5/E3533961=5; 6/3D536DDE=180; 8/42BECD5B=930; 9/94C63685=14; 12/B40FF4A6=120; 13/15B3F30E=1436; 14/15B3F30E=828; 15/19AB8B2B=303 |
| `gp-u20-chaos-ending` | 5/E3533961=5; 6/86946EB7=180; 8/42BECD5B=932; 9/94C63685=581; 12/B40FF4A6=120; 13/15B3F30E=1437; 14/15B3F30E=57; 15/19AB8B2B=302 |
| `gp-u21-armadon-ending` | 5/E3533961=5; 6/BC44EF50=204; 9/94C63685=581; 12/B40FF4A6=120; 13/15B3F30E=1437; 14/15B3F30E=828; 15/19AB8B2B=302 |
| `gp-u22-diablo-ending` | 3/BF1C78E2=30; 5/E3533961=5; 6/BF1C78E2=30; 8/42BECD5B=931; 12/B40FF4A6=120; 13/15B3F30E=1435; 14/15B3F30E=828; 15/19AB8B2B=118 |
| `gp-u23-u16-finisher` | 1/286ED662=25; 2/F1774E70=21; 5/E3533961=5; 7/3EA76262=425; 8/1895A942=177 |
| `gp-u6-moves` | 3/BF1C78E2=20 |
| `gp-u8-attract-start` | 3/86946EB7=85 |
| `gp-u8-right-training` | 2/F1774E70=21 |
| `gp-u9-win` | 5/E3533961=5; 9/86946EB7=170 |

A diff-allowed result shorter than its section's minimum fails. A capture differing
at tick 0 has `claimed = 0`. A pinned prefix that becomes unanchored or otherwise
loses its comparison fails rather than silently becoming only a named gap; a later
full-section match is a stronger claim and is accepted. A new diff-allowed section
in a measured scenario needs its own measured pin. The existing `TRACE_PINS`
whole-section totals, minima, allowances and section inventories are unchanged;
`prefix_ticks` is reported separately and is not added to `total_ticks`.

### L11.2 RED/GREEN, nulls and the 27-report rerun

The new public-comparator test was RED before the option/guard existed:
`test_diff_allowed_matched_prefix_pin_fails_one_tick_short` returned usage error 2
for `--prefix-pin`, instead of accepting the measured five-tick prefix. After the
change, pin 5 passes and pin 6 fails with `claimed matched prefix 5 ticks, pinned
minimum 6`. `test_diff_allowed_prefix_difference_at_tick_zero_claims_zero` isolates
`align_groups`: different tick-0 groups return `first_tick = 0`, `claimed = 0`.
The existing rotation nulls remain in the same suite and pass:
`test_rotation_anchor_rejects_a_wrongly_rotated_copy` (the shifted copy) and
`test_rotation_anchor_rejects_a_shuffled_capture`; the full module is **43 tests OK**.

On the real `gp-u23-u16-finisher` dump, section 1 measures 25 ticks and passes pin
25. The otherwise identical command with `--prefix-pin 1:26` exits 1 and names
`section 1 bank 286ED662: claimed matched prefix 25 ticks, pinned minimum 26`.
The dump run root was **`/tmp/pr_audio_dump`** (the existing dump set; its gp-u23
`audio.txt` is timestamped 2026-10-29 15:34 -03:00). The report-only rerun at
2026-10-29 22:58 -03:00 used each existing dump without replay:

```bash
caps=""; for f in $(python3 tools/audio_compare.py expected-files --scenario "$name"); do
    caps="$caps --capture data/k11-captures/$name/$f"
done
python3 tools/audio_compare.py compare --name "$name" \
    --port "/tmp/pr_audio_dump/$name/audio.txt" $caps --game-dir data/game/C \
    --json-out ".superpowers/sdd/2026-10-29-live-truth/reports/$name/report.json"
python3 tools/audio_compare.py summary --captures data/k11-captures \
    --audio-captures data/audio-captures \
    --reports .superpowers/sdd/2026-10-29-live-truth/reports
```

The audio path is unchanged since those dumps: the later `9716403f` (coverage hit
dump deduplication) and `f82c73fa` (render free-list drain) commits touch no audio
code or comparator. The comparison of all 27 dumps passed; the summary reads
**29/29 files, 26 with music, summary ok** (plus the declared U17 wall-paced skip).
A JSON cross-check reproduced all **74/74** values exactly, total **29,656**, and
confirmed every pre-existing whole-section total/minimum against the old reports.
No Makefile pin or command was needed.

The review fix round adds four synthetic guards: an unpinned diff-allowed section,
a pinned prefix lost to the named rotation-null gap, an absent pinned index, and a
malformed `--prefix-pin`. Removing each corresponding guard makes its test fail
(the three guard-removal mutants return `synthetic: ok` where the test requires
exit 1); malformed input now exits 2 through argparse without a traceback. The
module is now **47 tests OK**.

### L11.3 The bank inventory re-measured from those reports

The 27 reports request **22 distinct banks** of the code-reachable 26. The categories
below are **whole-section** categories; a bounded prefix claim does not reclassify a
bank as a whole-section match.

- **12 claimed by at least one whole-section match:** title+0x36102, selmo+0x25C,
  selmo+0x08, beach+0x08, caves+0x08, jungl+0x08, stone+0x08, snd2+0x21BFA,
  sound+0x6EC8, sound+0x08, grave+0x1A10 and grave+0x05E4.
- **10 requested but unclaimed as whole sections:** citys+0x08, conti+0x08,
  hghsc+0x08, himal+0x08, sound+0x335E, sound+0x39F4, sound+0x3E64,
  sound+0x5B88, sound+0x6CA8 and sound+0x72D4. Eight of these have at least one
  of the bounded diff-allowed prefix claims above; citys+0x08 and sound+0x5B88
  remain unanchored.
- **4 never requested:** **grave+0x0A8C, grave+0x08, sound+0x7714 and
  konsd+0x1CCB0**. These need new captures that request them. No such capture was
  made, and no coverage is claimed for them.

### L11.4 Claim limits

The new claim is exactly the per-section matched register-tick prefix under the
existing reduction/alignment model. The exact minima intentionally inherit the
documented host-load frame/tick **label-shift flake** (fought-match record §F6.3):
a replay whose labels shift can shorten or lose a prefix and fail even when its
register/value data is unchanged. That strictness is deliberate, matching the
existing whole-section pins; it is not hidden by widening or lowering a pin. This
is not a whole-section match, not a claim about the unmatched remainder or capture
tail, and not rendered audibility (the OPL core/mixer remains an invented model;
the PCM pin remains determinism). The U14 gap, all gameplay pins and every
pre-existing audio pin stay unchanged.

## §L12 Documentation reconciliation (Task 12a)

**Decision: the narrative now matches the landed claims; Task 12b and the full
gate remain separate.** `gp-draw-oracle`, `gp-twop-ko-oracle`,
`gp-u28-continue-oracle` and `gp-u27-oracle` are the four live-truth additions
to `make verify`. The measured inventory is **42 explicit capture paths, 41/42
present**, with only `gp-u14-win-live` declared absent. The KO-branch `0x0B`
pose and `0x0E` continue screen/countdown/expiry are exhibited; U27 is gated.
U16 R2's gate operand is measured as §L4 records, without deciding the wider
finisher-gap question. U14's failed artifact remains game evidence only.
`0x21` and `0x25` stay declared, the latter with §L6's structural blocker and
two `.failed` artifacts. The audio claim is §L11's **74 prefixes / 29,656
ticks**, not whole-section coverage or audibility.

The unreproducible U27 258 scene total and mode split are withdrawn in §L7,
the design and `docs/PROGRESS.md`; the reproducible facts are the 178
mode-`0x1E` port frames and the 303 explained content-bearing capture frames.
The duplicate §L7 trailing clause is removed. The state-coverage design now
uses past tense for its historical U27 exhibit. The plan's `0x21` wording
quotes the state-coverage stop rule, and the historical PROGRESS paragraphs
remain with superseded pointers to §L2/§L7. Task 10's declaration is
§L10: not taken, instrument gap, INCONCLUSIVE, no fitted cause, unchanged
bounds and pins.

Task 12a ran only the scoped documentation checks:

```text
python3 tools/check_docs_claims.py
check_docs_claims: 10 stale-phrase claim(s) absent

make gate-check
gate: captures 41/42 present, 1 missing (strict=1, pins_exact=1)
gate: declared gap (absent): data/k11-captures/gp-u14-win-live
capture_manifest: 41 present checked, 1 declared skipped, 0 problems
capture_manifest: pin baseline ok (digest adffb4c97366215f2e4e3ed0e5a07b56c3eb275f156c7c492f7a3b94cc2a9bf7)
capture_manifest: verify preflight ok

python3 tools/capture_manifest.py --check-pins
capture_manifest: pin baseline ok (digest adffb4c97366215f2e4e3ed0e5a07b56c3eb275f156c7c492f7a3b94cc2a9bf7)

python3 tools/port_progress.py
798 1203 66
759 759 100
ported 798 (759 portable + 39 runtime)  host-owned 41  deferred 12  replaced 37  runtime 315
```

No capture, census, `diff-verify` or full `make verify` was run. Task 12a did
not modify `tools/coverage_baseline.txt`, `tools/capture_manifest.json` or
`tools/pin_baseline.sha256`; Task 12b owns those measured results and Task 13
owns the full gate.
