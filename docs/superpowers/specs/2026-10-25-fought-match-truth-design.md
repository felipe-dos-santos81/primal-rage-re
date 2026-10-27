# Fought-Match Truth — design

Status: **Partially implemented**, revised 2026-10-26 after the phase's runs.
Follows the proof-completion, endgame-truth and music-truth phases. The harness
and two game-side measurements landed; U14's fought win (R1), a publishable
two-human replay and the U16 set-frame state remain named gaps (§1.1). The
port is complete against its declared population; the remaining honesty gap is
the fight itself, which the captured wins reach only under harness pokes or
with a measured replay divergence.

## 1. Intent and why

`python3 tools/port_progress.py` reads `799 1203 66` raw and `759 759 100`
portable. The differential audit reads `1033/1033 functions VERIFIED;
14993/14993 mutants detected; 0 named gaps; 825/845 rows with callees closed;
807/845 closed on full rows; 8 ported functions with no row`; the 20 open rows
are declared terminals. The capture oracles cover the boot, title, attract,
front end, demo fight, service menu, character select, one- and two-player
matches, seven endings and the U16 finisher reaction.

The claims that remain narrower than they read are all about the fight:

- **The full fought match.** U9/U10 and the five ending captures reach the win
  and the ending through pokes (`WIN_MENU`, the seven land marks, the `0x78`
  KO and `DEATH_DONE`). Their claim is the ending content, not the KO by hits,
  the death animation or the six earlier won matches. `gp-twop` and
  `gp-u16-twop2` are real two-human matches, but neither ends a round by a
  hand-dealt KO: `gp-twop` stops mid-round, and both U16 rounds end on the
  timer.
- **U14 R1.** The one fought (no-poke) win is a named gap. The two permitted
  attempts were spent in gameplay truth II; endgame truth's Task 8 declined a
  third. The failure is the CPU fight (P1 loses), not a port defect. The
  revised 160-attempt/260 s script is measured in the drafts but has never run.
- **The U16 finisher replay.** `gp-u23-u16-finisher` is the labelled
  poked-damage fallback; its reaction `r0=0x33` is the game's, but the port
  replay leaves `r0=0x27`. The divergence is now localised to a 41-frame
  over-dwell of the `0x27` reaction's case 3 (`+0x54` clears at capture
  f=0x1533 and port f=0x155C). The proposed y/anchor cause is **unproven**:
  the port's y chain is raw-faithful at every checked step, and no capture
  records the original's `slot+0x30`/`slot+0x42` at the deciding frame.
- **The 5120 band.** The U13 ending's capture 5120 differs from port 4317 in
  exactly 13 pixels (8/1/4, rows 1..4, x 21..298). The pixels are the first
  occurrences of three overlay palette indices; the neighbours 5119/5122 are
  clean/splice. A port-side composition dump can only improve the named gap
  (the capture is a 70.09 Hz sample); proving a port defect needs the
  original's back-buffer state, which no current capture holds.

The phase's thesis: **the port's remaining gap is not code coverage, it is
claim coverage.** One fought win, one hand-dealt two-human KO, and the
original's deciding U16 state turn the last "poked/partial/unclaimed"
sentences into measured claims or into clearly scoped named gaps. The 5120
band, the campaign's matches 2-7, the time-out winner, ENDURANCE past entry,
U17's wall-paced walk, the service-menu language reload, the coin countdown
and rendered-audio audibility are not this phase.

### 1.1 Phase outcome (measured 2026-10-26)

- **Harness:** `SNAP_FIELDS` and the port trace gained the four slot state
  fields (`s0_30`/`s0_42`/`s1_30`/`s1_42`); `Schedule`/`Poller` gained
  `stop_mode`/`stop_delay`/`skip_on_mode`. No game code changed.
- **U14 (R1 open):** the one authorized capture lost — P1 KO'd in both
  rounds, P2 VERTIGO `c1=3`/`afc=3`, `b0c` never 1, mode 0x12 never opened —
  and also failed its harness checks, so it is preserved as
  `gp-u14-win-live.failed`; the published target is absent and the nine
  `GP_U14_*` pins (the eight plus `MAX_MISSING`) are empty. `GATE_NAMED_GAPS`
  keeps it declared; the gate reads `34/35 present, 1 missing`.
- **Two-human KO (game-evidence-only, no claim):** two captures produced the
  game's hand-dealt KO (`check --ko`: `ko f=B9A`, then `f=B91`; P1's round
  result `ad4=0xFFFFFFFF`, `w2=1`, `w3=0`, no pokes), but both failed the
  port-script pin under host starvation. There is no published capture, no
  `gp-twop-ko-oracle`, no `GP_TWOP_KO_*` pin, no miss set and no gate entry;
  the first run's artifact was replaced by the recovery's publish path.
- **U16 (R2 open):** the recovery measured the `0x4844C` case-3 exit at
  `f=0x1533` (`s0_30=0x1180`, `s0_42=0x8000`, below the `0x1400` gate), but
  the poller missed the `s0_54=2` set record, so the gate input `y(0x1532)`
  is unmeasured. No fix, oracle or miss set; no third capture.
- **5120:** unchanged named gap with its measured facts; no pixel fix.
- **Gate:** no new oracle and no new capture path (35); counters
  `799 1203 66` / `759 759 100`, `diff-verify` `1033/1033`, `14993/14993`,
  0 named gaps, `entry-triage` unchanged and `symbols.h` byte-identical. The
  full gate **stopped at the register-stream audio oracle under host load**
  (`gp-u18-talon-ending`'s beach section and, on a standalone retry,
  `gp-u13-ending2`'s jungl section: section-anchor frame shifts; a standalone
  U18 run passed at 968 ticks and failed at 716 ticks with the audio dumps
  identical apart from the `W` rows' frame labels; no pin was weakened). The
  remaining steps were run separately and are green; the gate is not a single
  green `make verify`, and the audio model's host-load nondeterminism is a
  pre-existing limit (record §F6.3).

## 2. Current state (measured 2026-10-26)

- `port_progress.py`: `799 1203 66` / `759 759 100`.
- `diff-verify`: `1033/1033`, `14993/14993`, 0 named gaps, `825/845` with
  callees, `807/845` full, 8 unrowed; 20 open rows declared terminal (19
  distinct addresses), 15 `WEAKER`. No differential task is planned.
- Captures: 35 expected paths; 34 present; `gp-u14-win-live` absent after the
  phase's failed attempt (R1), with its artifact preserved as
  `gp-u14-win-live.failed`. The U23 finisher, the U13 ending,
  `gp-u17-device2` and the seven ending captures are present.
- U14: the revised 160-attempt script (two `_u16_round(160, 10,
  anchor=(0x06, 1/2))` blocks, `time_limit=260`, `stop_mode=0x12`,
  `skip_on_mode=(0x08,0x09)`) ran once on 2026-10-26 and lost: P1 was KO'd in
  both rounds, P2 (VERTIGO, `c1=3`, `afc=3`) won 2-0, `b0c` never became 1 and
  mode 0x12 never opened; the capture also failed its harness checks and was
  preserved as `.failed` (record §F2). The earlier two attempts (gameplay
  truth II) had also failed. All nine `GP_U14_*` pins are empty and
  `gp-u14-oracle` skips.
- `gp-twop`: 680 frames; two-human join (b1f 1→3, cred 4→4); ends mid-round 1
  at X f=0x5E1 in mode 6; P2 `+0x5A` peaks 0x19; no KO, no round result.
  Pins: N 612, F 1506, reach 463, start 83, missing 5, moves 1506.
- `gp-u16-twop2`: 8,091 frames; both rounds end on the timer (mode 7 at
  f=0x11D9 and f=0x2076), no `0x78` in any S record; three finisher entries,
  no 0x32..0x34 reaction. Pins: N 8069, F 8637, reach 6818, start 100,
  missing 23, moves 8637.
- `gp-u23-u16-finisher`: 5,464 frames; round 1 timer-out, round 2 P2
  `0x10789E` poked 0x77 and one real hit takes it to `0x78` (f=0x1486);
  TALON's second entry yields `r0=0x33` for 640 S records. Pins: N 4095,
  F 5449, reach 3439, start 106, missing 9, moves 5467. The port replay's
  first differences: `s0_54` at f=0x1533 (capture 00, port 02), `s0_52` at
  f=0x1545 (00 vs 09), `rng` at f=0x1549 (75B49A54 vs BA99085D), `r0` at
  f=0x155B (33 vs 27). The offsets are one uniform +0x29 frames. Slot 1
  matches. The port's `slot+0x30` is 0x202E at the reaction set `f=0x1532`
  and 0x22F4 at `f=0x1533`; the gate is
  `0xBD884[2]=0x1400` (`0x48599 cmp edx,[eax+0x30]`, `0x4859C jle`). The
  phase's recovery measured the original's exit state at `f=0x1533`
  (`s0_30=0x1180`, `s0_42=0x8000`, below the gate) but the poller missed the
  `s0_54=2` set record, so the gate input `y(0x1532)` is unmeasured
  (record §F5.6).
- `gp-idle-loss`: 8,173 frames; the CPU KOs P1 by hits in round 1
  (`s0_5a=0x78`, f=0xC74), round 2 times out; no mode 0x12. Pins: N 7559,
  F 8320, reach 6300, start 90, missing 29. This is a CPU win, not a
  player-driven fought win.
- Game-code frontier: the four no-op `game_frame` cases
  (`0x00/0x1C/0x1D/0x26`) are faithful; the nine `named gap` comments are
  seven host-owned/unreachable/terminal, one genuine functional gap (the
  service-menu language reload, `§K11.3`) and one oracle gap (the coin
  countdown). No porting task is planned.
- Docs: the phase's Task 6 fixed the `AGENTS.md` U13 `MIN_REACH` line to 4951,
  the U14 attribution and the nine-pin count, the 5120 wording and the gate
  count; the music-truth audio paragraph is current (music truth has landed:
  `make verify` runs `audio-oracle`, and `make audio-render` pins the selected
  selmo at peak 2790 / sha256 `530a9430…`). Rendered audibility remains a model
  gap.

## 3. Goals

**G1 — U14 R1 (one authorized run; the exact README gap).** Update the U14
scenario to the measured 160-attempt script (16-frame 1A/1B gap, a 12-frame
`p1.right` walk every 8 attempts, `time_limit=260`, stop on the conquered-lands
screen mode 0x12), run it **once** under the new go-ahead, and either pin all
nine `GP_U14_*` values (the eight existing plus `MAX_MISSING`) with a passing
`live_death` or record the measured failure and keep the pins empty. No second
script tweak under the same go-ahead. The measured hit rate is ~3 landed hits per 32 attempts against an
idle P2; the round timer is ~3,280 frames, so 160 attempts fit. The CPU P2 can
kill P1 first (the failed run's P1 died at ~83 attempts); it is a coin flip,
not a port defect.

**G2 — A hand-dealt two-human KO (the controllable arm).** Capture a real
two-human match from the character select through one **hand-dealt KO**, the
loser's death stream and the round result (mode 8). P1 lands 1A/1B; P2 is
passive except the walk-in's own `p2.left`, which satisfies the two-human
evidence rule. No pokes. Derive a sibling of `live_death` from the raw —
`gp_twop.py check --ko` — and claim the KO (`s*_5a=0x78` with a prior nonzero
value), the round result (`ad4=NO_RESULT`, `w2`/`w3`), the death animation the
capture exhibits, and `b0c` **only if** an S record shows it 1 with no `W`
writing `DS_00104B0C`. The result window is a 240-frame harness window sized
from the longest observed death stream (U10's ~190-200 frames after its KO),
not from a two-human measurement; the capture reports `b0c` if it appears and
claims it only then. The next round is not claimed. A time-out (mode 7) is a
failed task, not a claim.

**G3 — The U16 deciding state (measured, then a fix only if the raw proves
one).** Add `slots +0x30` and `+0x42` to the S-record fields and the port
trace, capture one diagnostic replay of the U23 scenario (`gp-u24-u16-state`,
same steps, no new claim), and compare the original's `slot+0x30`/`+0x42` at
f=0x1533 with the port's. If the original's y is below the `0x1400` gate and
differs, trace the first divergence and fix it only with a raw-derived change
and a boundary test (`0x13FF` ends case 3, `0x1400`/`0x1401` do not). If the
original's y matches the port's, the y/anchor hypothesis is refuted; record
the measured state and leave R2 a named gap with the new evidence. No fitted
constant.

**G4 — Claim hygiene, docs and the gate.** Correct the stale U13 `MIN_REACH`
line (4952 → 4951), the U14 attribution (the attempts were spent in gameplay
truth II; endgame Task 8 declined), the 5120 wording (a byte-pinned allowance,
not "the last unexplained pixel"), the capture-gate count (3 required + 2
compare-if-present + 30 skip = 35), and the music-truth state (register stream
verified; audibility a model gap). Update `README.md`, `AGENTS.md`,
`docs/PROGRESS.md`, `port/RE_GUIDE.md`, the Makefile comments and the
derivation record. Run the full gate and report the measured line.

## 4. Non-goals

- Campaign matches 2-7, the sequential land tally and the seventh-land
  selection. The ending captures' poked wins already claim the ending content;
  a seven-match campaign capture is a future phase.
- The time-out winner (`0x27DC8`'s mode 7), ENDURANCE past its 300-frame
  entry, the four `gp-u8` mode-6 rows past entry, and the finisher gate's
  capture-only 50000 award/0x384 hold.
- U17's wall-paced walk (its evidence check stands; no invented pins), the
  service-menu language reload, and the coin-countdown capture.
- The 5120 band's composition dump. The measured facts stay; the port-side
  dump can only improve the named gap, and a capture-side buffer poll is a
  separate experiment. No pixel fix is promised.
- Rendered-audio audibility, CI/packaging, SDL pad hot-plug and save-path
  docs.
- Closing the 20 terminal differential rows, the 15 `WEAKER` rows or the 8
  `UNROWED` functions; the census stays the authority.
- Pokes in the new fight captures. U23's existing labelled damage fallback is
  not re-run as a fight claim. No new portable function unless a raw-derived
  fix requires one; no fitted constants; no claim past the captured window or
  the port's exhibited frames.
- Re-capturing the seven endings or U9/U10; their claims stand.

## 5. Approaches considered

**A — Fought-Match Truth (chosen).** One U14 run, one controlled two-human KO
capture, and the U16 deciding state. Highest honesty value: U14 is the exact
README gap, the two-human KO is the controllable arm if U14's coin flip fails,
and the U16 measurement either closes a measured port divergence or turns its
cause into a scoped named gap. The harness work (state fields, stop-on-mode)
is small and reusable.

**B — Campaign matches 2-7.** Rejected. It needs seven sequential fought
matches or a much longer poked run, and the ending content is already claimed.
It would not add the fought-KO/death-stream claim that G2 adds, and its cost
is an order of magnitude larger.

**C — Fix the U16 reaction first.** Rejected. The y/anchor diagnosis is
unproven; the port's y chain matches the raw at every checked instruction. A
fix written now would be a fitted constant. The deciding state must be
captured first.

**D — 5120 composition dump.** Rejected. The port-side dump cannot prove the
original's back-buffer state; it can only retire the corner-ornament class or
improve the named gap. It is not a fight claim. The measured facts stay in
the record.

## 6. Constraints

- Work from the repo root. `make`, `run_tests` and the tools resolve
  `data/game/C` and `data/k11-captures/` relative to it.
- `data/game/` is read-only. Captures write only
  `data/k11-captures/<scenario>/`; never type in the DOSBox-X window while a
  capture runs. Every capture's `check-input` must be clean.
- New captures are made with `make gp-capture scenario=…`; the shell timeout
  is disabled or at least 600 s. `make title-pin` must be run first (the
  pinned original is not durable).
- One U14 attempt under this phase's explicit go-ahead. A failed run is
  recorded and the pins stay empty; no second script tweak. The two-human
  capture gets one run at its measured attempt count; a time-out is a failed
  task, re-run only by a new decision.
- No pokes in the fought captures. The U23 capture's damage poke is existing,
  labelled evidence and is not a model for the new fights.
- Every pin is measured from the tool's own output and proved to fail one step
  in the tested direction. No fitted constant. A value that cannot be pinned
  is a named gap with its evidence.
- Any port change is raw-derived, one C function per original, with the
  `/* 0xADDR — section */` header and a `PORT:` note for a deliberate
  deviation. A function a differential row covers opens with the right
  `PR_SEAM` form; a new row or an updated row must be measured.
- Existing pixel/state pins must not move except the phase's own; any
  unexpected movement stops the task and is recorded.
- `SNAP_FIELDS` additions are appended, so older `poll.log` lines simply lack
  them. A new scenario is added to `STOP_AT_END`; a gate-inventoried capture
  is added to `CAPTURE_DIRS` and the gate's literal count is bumped in all
  three places. A diagnostic capture is not gate-inventoried.
- Docs: `docs/PROGRESS.md`, `README.md`, `AGENTS.md`, `port/RE_GUIDE.md`, the
  affected Makefile comments, this spec and the derivation record. Commit
  style `<area>: <what>`; stage named files, never `git add -A`; commit only
  when the user authorizes the phase's commits.
- The final gate is `PR_DIFF_FULLSCAN=1 GATE_STRICT=1 PR_PINS_EXACT=1 make
  verify`, with every capture present except a declared failed U14.

## 7. The measured starting point

### 7.1 U14

The scenario is `gp-u14-win-live` (`tools/gp_session.py`): `WIN_MENU`'s
immediate confirm of character 0 (SAURON), then two rounds of 32 1A/1B
attempts, 16 frames apart, each held 4, `time_limit=200`, `extra=WIN_EXTRA`,
zero pokes, stopping on mode 0x12. 1A = new `b0+b1` (character 0's keyboard
entry 0x1A, reaction 0x10); 1B = new `b2+b3` (entry 0x1B, reaction 0x11).
Both are the table's 1-frame normals. The measured hit rate is ~3 landed hits
per 32 attempts; 7-12 landed hits reach `0x78`; a round timer is ~3,280
frames. The failed attempt 2's P1 was KO'd in both rounds, `b0c` never 1.

The revised script is the U16 builder's: `_u16_round(160, 10,
anchor=(0x06, 1))` and `_u16_round(160, 10, anchor=(0x06, 2))`, with a
12-frame `p1.right` walk every 8 attempts, `time_limit=260`, a **per-round
skip** on the round or match result (`skip_on_mode=(0x08, 0x09)`) and a
whole-capture stop on mode 0x12.
The skip is required: a KO at the measured ~88th attempt leaves ~72 queued
presses, and the U23 brief's 160-attempt run overran the result screen and
never reached its end frame when they all fired. With the skip, the round's
remaining attempts are dropped at its result and the scheduler resumes at the
next `after_entry` (round 2); the capture's "steps fired" check counts the
skipped steps as deliberately dropped. 160 attempts per round = about 2,934
frames at the measured walk/gap pattern (the builder's per-round interior sum
is 2,924; the measured two-round span is 5,868), under the round timer; two
rounds plus the menu fit inside 260 s. **The phase's one run (2026-10-26)
lost** — P1 was KO'd in both rounds, `b0c` never 1, mode 0x12 never opened —
and also failed its harness checks, so the script is as written but R1 remains
a named gap (record §F2, §1.1).

### 7.2 The two-human fight

`gp-twop` is the two-human start: LEFT PLAYER ARCADE (b1f=1, credits 5→4),
P2's F2 join in the character select (b1f |= 2, no debit), each side moves
its cursor once and confirms, both walk in, and both press two attack chords.
The capture ends mid-round 1; P2's health peaks 0x19; no KO, no result.

`gp-u16-twop2` extends the same start with two 32-attempt rounds and three
finisher entries. Both rounds end on the timer (mode 7 at f=0x11D9 and
f=0x2076; `s0_5a` stays 0; `s1_5a` peaks 0x41 then 0x1E; no `0x78`). The
match-result screen follows the second time-out; the finisher gate opens, but
no reaction is sampled (`r0`/`r1` never 0x32..0x34). The claim is the two-human
match and the frame evidence, not a KO.

The raw's KO/result fields (measured from the bytes): the damage clamp writes
`0x78` to `slot+0x5A` (`0x394D5`/`0x39525`, `0x3956D`/`0x395C2`); `0x27FA8`
tests `>= 0x78` (`0x27FB3`/`0x27FBF`) and stores mode 9 if `ad4` is 0..2, mode
8 otherwise (`0x28024..0x2803B`); `0x27C48` increments the round winner's
`w2`/`w3` (`0x27C83..0x27C96`, `0x27D33..0x27D3D`); `ad4` is written from
`b1e` vs `DS_00104ADC` (`0x27C3D`). `b0c` (`0x104B0C`) has one non-zero
writer, `0x37FF6` (the death stream's `0x37EA0` opcode `0x11`); no S record
can observe a poked 1 because the mode-0xD consumer clears it in the same
frame, which is why `live_death` rejects a `W` at `0x104B0C`. In a two-human
match `b1f=3` makes the `0x430E8` hook skip `0x41350`, so the CPU writers and
`+0x63` are absent; nothing on the KO/death chain reads `+0x63`, so the bytes
say a two-human KO should set `b0c`, but no capture has exhibited it. The new
capture will report it; the claim must not assume it.

`DS_00104ADC = 3` in these captures (`0x25A14` from config field `0x29`), so a
1-0 round does not decide the match; a mode-9 match result needs `w=2`. One
hand-dealt KO is therefore claimed on the round result (mode 8), not mode 9.

### 7.3 The U16 finisher

The capture's reaction `r0=0x33` is the game's; the port replay leaves
`r0=0x27`. The divergence is localised: both sides run `0x48608` at f=0x14F7
and the D500 target `0x48374` at f=0x1532; the capture's `slot+0x54` clears
at f=0x1533, the port's at f=0x155C (one uniform +0x29 frames); the capture's
`slot+0x52` clears at f=0x1545, the port's at f=0x156E; the capture's rng
call is at f=0x1549, the port's at f=0x1572; `r0` advances at f=0x155B in the
capture and never in the port. Input (`raw`/`pad`) matches throughout; slot 1
matches.

The port's y chain is raw-faithful at every checked instruction:

- `0x4844C` case 3 ends when `BD884[own char] > slot+0x30` (`0x48599
  cmp edx,[eax+0x30]`, `0x4859C jle`); the port matches.
- `0xBD884[2] = 0x1400` (TALON); the port's `slot+0x30` at the reaction set
  `f=0x1532` is 0x202E (0x22F4 at the capture's exit frame `f=0x1533`).
- The y is `ab4 0x1D40` + `rec+0x1C 0x2EE`: `ab4` is anchor 0x1E7 → sprite
  0x0DBB (the `0xED8FE` stream's first id) − TALON's cam word 0x0BD4 at
  `0xECBD8`; the table `0xCFC32[0x1E7] = (-99,+117)` gives 117*64 = 0x1D40.
  `rec+0x1C` is the `0x48396`/`0x483A0` store plus `motion_step`'s
  `(+0x34>>16)` and `+0x44` subtraction. The latch adds
  `0x100AB4[side]` (`0x186D0`).
- `fighter_48374` transcribes `0x48374` exactly, including the pre-fixup
  stream constant `0x6D8FE` → `0xED8FE`.

The phase's recovery measured the original's exit state at `f=0x1533`
(`slot+0x30 = 0x1180`, `slot+0x42 = 0x8000`, below the gate), but it missed
the `s0_54 = 2` set record: the deciding gate input `y(0x1532)` is
unmeasured, and the exit value is a different RNG draw's (record §F5.6). The
exit is consistent with the y hypothesis but does not decide it. If a future
capture's y is below 0x1400 at the set frame, the difference is real but its
source is still upstream (the stream, the anchor table, the latch arm, or a
`+0x42` bit-3 state). If it matches the port's, the y hypothesis is refuted
and the `+0x54` clear has another writer (`0x36870`, `0x4811C`, `0x36F10`, or
an anim target). The phase measures the state; it does not fit the gate.

### 7.4 The 5120 band

The capture 5120's 13 pixels are 8 `00001c` where port 4317 has index 0x32, 1
black where 0x2E, and 4 black where 0x4D — all in rows 1..4, x 21..298. They
are the first occurrences of the three overlay indices; the 8 `00001c` are
`DAC[0x15]` at 4317. Neighbour 5119 = 4315 (0 diff), 5122 = 4318 (0 diff),
5121 = a 4317/4318 splice. No port frame holds the band. The class is a
sub-frame composition state or a presentation transient; the port-side dump
can retire the corner-ornament class but cannot prove the original's buffer.
The phase leaves it a named gap with these facts; no pixel fix and no fitted
allowance.

### 7.5 Docs

`AGENTS.md`'s U13 `MIN_REACH` line read 4952 where the Makefile pins 4951;
Task 6 corrected it to 4951. The predecessor design's §7.6 said music had not
landed; music truth has landed (`audio-oracle` is in `make verify`;
`make audio-render` pins peak 2790 / sha256 `530a9430…`). The U14 attribution
("the endgame phase's two permitted attempts") is wrong: the attempts were
spent in gameplay truth II, endgame Task 8 declined a third, and this phase's
one authorized attempt also lost (record §F2). The 5120 "last unexplained
pixel" wording overstates: it is a byte-pinned allowance. The gate is 3
required + 2 compare-if-present + 30 skip = 35; because the phase added no
gate-inventoried capture and did not wire the two-human arm, it stays **35**
(the plan's conditional 36 applies only if `gp-twop-ko` were wired, which it
was not).

## 8. The approach

### 8.1 U14

Move the U16 builder's constants and `_u16_round` above the U14 block (or
redefine the U14 block after it), replace the 32-attempt tuples with the two
anchored `_u16_round` calls, and set `time_limit=260`, `stop_mode=0x12,
stop_delay=0` and **`skip_on_mode=(0x08, 0x09)`**. The skip drops a round's
queued attempts at its round or match result and resumes at the next
`after_entry`; without it the 160-attempt form repeats the U23 run's ring
overflow after an early KO. Update `TestLiveWin` to assert 160 attempts, the
walk every 8, the two anchors, `time_limit=260`, `stop_mode=0x12` and
`skip_on_mode=(0x08, 0x09)`.

Before the U14 run, validate the harness with a throwaway
`gp-harness-smoke` scenario (boot Enter → mode 0x27, stop 10 frames later;
not gate-inventoried, not in the audio traces; it exercises the stop path,
not the full feature), and run `make title-pin`. Then
one `DISPLAY=:1 timeout 600 make gp-capture scenario=gp-u14-win-live`. A
harness/OS failure before the game starts (no poll.log, DOSBox-X fails to
launch) is a preflight failure, not the U14 attempt; a run that starts the
game and produces a capture is the attempt.
If `gp_win.py check` passes (the six milestones and `live_death`), replay,
measure the nine pins, prove each one step, fill them, remove
`gp-u14-win-live` from `GATE_NAMED_GAPS` (a declared absence that is present
fails gate-check), and run `make gp-u14-oracle`. If it fails, record the
first failure and the measured rows, keep the pins empty and the
`GATE_NAMED_GAPS` entry in place, and leave R1 a named gap. Either way, the
phase continues to the two-human arm.

### 8.2 The two-human KO

The scenario clones `gp-twop`'s start through the walk-in (whose own
`p2.left` is the second-side fight press the two-human rule needs), then
`_u16_round(140, 30)` for round 1, and `stop_on_mode=(0x08, 0x09),
stop_delay=240` (a harness window sized from U10's observed death stream, not a
two-human measurement). `extra=WIN_EXTRA` logs `ad4`,
`w2`, `w3`, `b0c`; `time_limit=240`. 140 attempts is about 2,600 nominal
frames, 1.6× the measured ~88-attempt KO; the round timer is the backstop. No
pokes. The next round is not captured or claimed.

`gp_twop.py check --ko` is the evidence rule: a two-human log, no `W` at the
KO/death addresses, the loser's `s*_5a=0x78` with a prior nonzero value in
the same mode-6 entry, no mode-7 record in that entry, the winner's
`s*_5a<0x78`, `w` naming the winner, `ad4=NO_RESULT` (0xFFFFFFFF) with mode 8,
and `b0c` reported but not required. It fails the existing captures: `gp-twop`,
`gp-u16-twop2` and `gp-u23` carry no `WIN_EXTRA` fields, so the honest first
failure is "no WIN fields"; `gp-u9` has them and fails on its poked KO. The
time-out and poke arms are exercised by synthetic unit cases. The capture is
pinned with the evidence check and the frame/trace/moves ratchets; the `b0c`
claim is made only if the capture exhibits it.

### 8.3 The U16 state

Append `s0_30`/`s0_42`/`s1_30`/`s1_42` to `SNAP_FIELDS` and the port's
`gp_trace_line`. Define `gp-u24-u16-state` as a clone of
`gp-u23-u16-finisher` (same steps, same labelled poke) and add it to
`STOP_AT_END`; it is a diagnostic capture, not gate-inventoried. Capture it,
replay it, and locate the deciding frame by marker, not by the old capture's
literal f: the first `s0_54 == 2` (the `0x48374` store) and the following
`s0_54` 2→0 (the case-3 exit; f=0x1533 in the current capture). Compare the
original's and port's `s0_30`/`s0_42` at that transition and over the mode-6
window. If the original's y is below 0x1400 and the first divergence is
found, fix the raw-derived source and add the boundary test; if the y
matches, record the refutation and leave R2 a named gap. A fresh run whose KO
attempt shifts the frame is aligned by the marker, not by f. A fix that
cannot be derived from the bytes is not taken.

### 8.4 Claim hygiene

Fix the U13 4952 → 4951 line, the U14 attribution, the 5120 wording, the gate
count and the music-state paragraph. Add the new claims only after their
measurements: U14's either-or, the two-human KO's measured fields, and the
U16 state's decision. The docs commit is last.

### 8.5 Gate

`PR_DIFF_FULLSCAN=1 GATE_STRICT=1 PR_PINS_EXACT=1 make verify` must exit 0
with every new oracle run (not skipped), every old pin unchanged, the diff
counter unchanged, `entry-triage` matching a fresh run and `symbols.h`
regenerating byte-identically. `make audio-render` is not required (no audio
change) but is run as a regression check if the gate has budget.

## 9. Acceptance

1. U14, if run, either passes `live_death` with all nine pins measured and
   the oracle green, or fails once with the measured first failure and
   remains a named gap; no second attempt under the same go-ahead.
2. The two-human capture reaches a hand-dealt KO by hits and the round result
   (a 240-frame result window sized from the observed death stream), with no
   poke writing `DS_00104B0C`; `gp_twop.py check --ko` passes; the replay's
   frame/trace/moves pins are measured and fail one step in the tested
   direction; `b0c` is claimed only if the capture shows it. A time-out (the
   U16 shape) is a failed task, not a claim.
3. The U16 diagnostic capture records the original's `slot+0x30`/`+0x42` at
   the case-3 exit, located by the `s0_54` 2→0 marker (f=0x1533 in the current
   capture; a fresh run may shift). If a raw-derived cause is proved, the fix
   lands with a boundary test and the U23 pins move (or stay, if the replay
   does not improve); if not, R2 remains a named gap with the measured state
   and the refuted/retained hypotheses.
4. The 5120 band remains a named gap with its measured facts; no pixel fix and
   no fitted allowance.
5. `make verify` exits 0; every capture present except a failed U14; existing
   pins unmoved except the phase's own; `port_progress.py` and the differential
   counter unchanged unless a raw-derived fix adds a row; docs current.
6. The derivation record has every measurement, poke, allowance and named gap;
   the final review is done.

**Measured outcome (2026-10-26):** item 1 holds through its failure branch (one
attempt, recorded, nine pins empty, R1 named); item 4 holds (5120 unchanged);
item 5 is **not a single green `make verify`**: the full gate stopped at the
register-stream audio oracle under host load (`gp-u18-talon-ending`'s beach
section, then `gp-u13-ending2`'s jungl section on a standalone retry:
section-anchor frame shifts; a standalone U18 run passed at 968 ticks and
failed at 716, with the audio dumps identical apart from the `W` rows' frame
labels; no pin was weakened), while its remaining steps are green separately;
no pin moved and no function was added. Item 2's
*game* half holds — two captures exhibit the hand-dealt two-human KO — but its
pins do not: neither capture produced a port script, so there is no
`gp-twop-ko` target, oracle, miss set or gate entry (record §F4, §1.1). Item
3's exit state holds (`s0_30=0x1180`, `s0_42=0x8000` at `f=0x1533`) but the
set-frame gate input `y(0x1532)` was missed, so R2 remains a named gap and no
fix was taken. Item 6 is the record and §F6. The design is therefore
**Partially implemented**.
