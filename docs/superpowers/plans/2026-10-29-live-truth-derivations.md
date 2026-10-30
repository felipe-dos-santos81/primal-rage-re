# Live Truth — derivation record

**Phase:** Live Truth. **Design:** `docs/superpowers/specs/2026-10-29-live-truth-design.md`
(status Proposed). **Plan:** `docs/superpowers/plans/2026-10-29-live-truth.md`.
**Baseline:** `main` at `0b458148` (`Merge branch 'state-coverage'`), full SHA
`0b45814842650de0b8f70fceeeab55a8d761fda4`. Branch `live-truth`.

**Phase rules.** No port code changes: the phase is capture and record, and no
new portable function is in scope. A capture that proves a port divergence is
recorded at its first unexplained frame and stops; it is not patched. The design
is proposed; §L1 is the baseline at which nothing of it had been built, captured
or pinned. The later sections record the phase's measured changes.

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
is the post-script tail: the original continues to its X record at `f=0x0B9E`,
whereas the port script ends at `f=0x0B62`; those 68 frames are not claimed.
Captures 2058..2063 exhibit ports 1681..1685, so the window below 2064 includes
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
and complete interior coverage are not claimed, and nothing in the post-script
tail is claimed.

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
39 present checked and the same declared U14 absence. The measured pin-baseline
reason is `live truth Task 2: measured gp-draw KO-draw oracle and capture-346
scan-out`, digest `b4b2d3b19322a9d24646820825c6e9cea8bfab6db21eac502b1de8a2fc602013`.

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

The `06` and `07` rows are mode-`0x1E` re-entry samples, not the first writes of those
`b25` values (those writes occur during mode `0x15`). The checker therefore pins the exact
records rather than inventing a global first-transition rule. `path` compares only the six
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

The frame window includes mode `0x1E` and the finaliser. The recon's **258** byte-identical
frames reproduce with a more precise split: in the name-entry/finaliser scene, **155
mode-`0x1E` port occurrences plus 103 finaliser mode-`0x15` occurrences = 258 distinct port
frames** each have a byte-identical capture frame (and there are 258 such capture frames).
The whole capture 5311..5616 scene has 303 content-bearing frames, all explained (255
classified clean and 48 splice), plus three all-black frames. The remaining byte-identical
port occurrences are not claimed: the oracle keeps the usual narrow existential/splice and
bounded-missing claims, not full port-frame order or coverage.

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
