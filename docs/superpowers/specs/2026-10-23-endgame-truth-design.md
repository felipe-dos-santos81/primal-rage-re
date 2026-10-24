# Endgame Truth — Design

Status: Implemented and gated (2026-10-24; draft approved 2026-10-23, revised after review)
Phase branch: `endgame-truth`, base `main` at `fcdd11d2`
Plan: `docs/superpowers/plans/2026-10-23-endgame-truth.md`
Record: `docs/superpowers/plans/2026-10-23-endgame-truth-derivations.md`

## 1. Intent

The port is 759/759 portable functions (799/1203 raw, 66%). Every portable
function is ported; the remaining player-visible gap is the endgame. Only two
of the seven character endings are captured: SAURON's (character 0) under the
U10 pokes, and BLIZZARD's (character 1) in `gp-u13-ending2`. TALON, VERTIGO,
CHAOS, ARMADON and DIABLO have no capture. The U16 finisher reaction (R2) was
attempted but never appeared. The game-set death-done byte `b0c` is already
pinned by U10: its first `b0c=1` is at f=0x1602 in mode 0x0F, 190 frames after
the seventh poked final KO, and `GP_ENDING_WIN_MIN_FIRST=9954` compares it
(`Makefile:1139/1212`). What U10 does not claim is a fought (no-poke) win; the
U14 fought live win (R1) failed its two permitted attempts.

This phase captures and pins the remaining endgame: the five missing endings,
the U16 finisher, and (conditional on a new go-ahead) the U14 fought win. It
also extends the U13 replay to claim its post-script tail (an
existing-capture, offline task), corrects the 5120 band's comment to what the
bytes show, and documents the INT 8 tick vector (the last `RE_GUIDE` item). It
does not touch the differential frontier, which is proof accounting on
runtime/host stubs, or the 5120 cause, which needs sub-frame evidence.

## 2. Why now

- The shared code paths exist and are captured: U13 exercises the mode-0x1F
  ending path for character 1; U16 exercises the two-human match and the three
  finisher entries; U10 exercises the final and the game's `b0c` write. The
  remaining work is the character/cell mapping and the KO timing, not new
  engine understanding.
- The ending selection is one table and one code path. The five missing
  endings differ by character id, cell, land mark and the ending title; a
  U13 clone reaches them without new portable code.
- The differential frontier is measured and honest but is not game content:
  31 open rows, all on recorded runtime/host stubs (23 blockers, 66
  incidences), and the AIL/card rows the last census classified. That work is
  a separate phase; doing it here would dilute the endgame claim.
- The U14 two-attempt limit is spent. A retake needs a new, explicit
  go-ahead; the phase therefore makes U14 conditional and does not silently
  spend a third attempt.
- The U13 tail is an existing-capture replay extension, not a new capture:
  the capture's `S` records run to f=2754 (10068) while the generated port
  script stops at the `X` record f=10013, so the last 13 frames have no port
  dump. The `port-script` guard is the only blocker.
- The 5120 band is not closable offline: no dumped present, DAC, pair or
  static index reproduces its 13 pixels; the current `BANDS_ALLOWED` comment
  says the neighbours corroborate "the port's content at the 13 pixels", which
  is true only of the port's rendered colours at its own indices, not of the
  capture's 13 pixels. The honest endgame includes correcting that sentence,
  not fitting a cause.
- U10 already holds the game-set `b0c`; the final's `DEATH_DONE` poke replaces
  the mode-0xD wait, not the `0x37EA0` write. A new b0c capture is not needed;
  the remaining b0c work is the no-poke fought win (U14).

## 3. Goals

**G1 — U16 finisher (R2).** Capture a two-human match with a real KO in the
deciding round, so mode 9 holds its `0x258` frames and the three finisher
entries (`0x15/0x16/0x17`) produce at least one of the `0x32/0x33/0x34`
reactions. The existing `gp_twop.finisher` check (which requires a reaction
unless `--allow-missing-reaction`) runs without the allowance; the capture
records which reactions appeared. The existing `gp-u16-twop2` capture keeps
its allowance and pins.

**G2 — The five missing endings.** Capture TALON (character 2, cell 1),
VERTIGO (3, cell 2), CHAOS (6, cell 4), ARMADON (4, cell 5) and DIABLO (5,
cell 6), each by cloning the U13 scenario with its cell, land mark and
character name. Each capture gets a `gp_win.py` check with the mode-0x1F row's
`ad4=0,c0=char` and the character's `_ENDING_LABEL`; the record's ending title
(the `0xA80E4` state-4 string) is decomp evidence and the mode-0x1F frames lie
inside the compared window. Each gets a `gp_compare.py` first-unexplained /
trace / reach pin, the capture identity (`poll.log` sha256 and frame count),
and a per-capture oracle in `make verify`.

**G3 — U14 R1 (conditional).** If the user gives a new explicit go-ahead,
retake `gp-u14-win-live` with the revised fight script (more attempts and a
re-approach, `time_limit` ~260 s, no pokes), requiring the game-set `b0c=1`
and the fought KO. If the go-ahead is declined or the retake fails, R1 stays
a named gap and the `GP_U14_*` pins stay empty. The existing `live_death` rule
anchors on `(0x09,1)`, rejects any `DEATH_DONE` write, and requires `b0c=1`
after the KO.

**G4 — U13 tail and 5120 wording.** Add an opt-in replay extension to the
last `S` record (f=2754), replay the existing `gp-u13-ending2` capture, and
claim the 5891..5903 tail if the port matches: `GP_U13_MIN_FIRST` 5891 →
5904 and `GP_U13_MIN_REACH` 4939 → the new last exhibited port frame (about
5000), with trace/WIN measured. If the tail diverges, record the first
unexplained frame and a named port cause; do not move the pins. Correct the
`BANDS_ALLOWED` sentence to "the neighbours corroborate the port's rendered
colours at those indices"; keep the 8/1/4 split (8 at port index `0x32`, 1 at
`0x2E`, 4 at `0x4D`). Add a synthetic band-shape/mutation test; a real-capture
test may skip without the capture. The 4248/4320 recipes and pins stay
unchanged.

**G5 — The tick vector.** Document the INT 8 installation from the raw bytes:
`0x66519 b8 08 00 00 00`, `0x6651E ba 7e 62 05 00`, `0x66523 66 8c cb`,
`0x66526 b4 25`, `0x66528 1e`, `0x66529 66 8e db`, `0x6652C cd 21`; the
immediate `0x0005627E` is the code-object offset of linear `0x6627E` (CS base
`0x10000`), so vector 8 points at the handler `0x6627E`. The PIT routine entry
is `0x663A4`; the mode command `b0 36 e6 43` is at `0x663AC..0x663AF`; the
divisor formula at `0x663D4` (`param*10000/0x20BC`) gives 19,887 for the AIL
60 Hz period 16,666 µs (`0x1CFFA` pushes `0x3C`; `0x1CFFF` is the
`call 0x5DA87`), 59.998 Hz. `0x2D62C` is the
9-byte counter called by the 60 Hz callback `0x1BDF4` at `0x1BE28`, not the
ISR entry. Mark `RE_GUIDE` item 4 done. No port code.

**G6 — The gate.** `PR_DIFF_FULLSCAN=1 make verify` exits 0 with the new
oracles and every existing pin intact except the phase's own measured pins.
`tools/port_progress.py` stays `799 1203 66` / `759 759 100`; the differential
counter stays `1031/1031` functions, `14983/14983` mutants, 0 named gaps,
`813/844` closed, `806/844` full, 8 unrowed, 31 open.

## 4. Non-goals

- **NG1 — The differential frontier.** The 31 open rows, the AIL wrappers,
  `0x65431`, the card entries, `0x65546`, the 8 portable unrowed functions,
  the 12 weaker rows and `flow_4f5c8`'s zero-divisor deviation are a separate
  "Proof Completion" phase. The agents' census (31 → 4 closable; 8 portable
  rowable) is seed evidence, not this phase's scope.
- **NG2 — The 5120 cause.** Keep it a named gap; the existing capture cannot
  distinguish a sub-frame transient from a present the replay never dumped.
- **NG3 — The U12/U15/U16 post-script tails.** They are real content but
  beyond their scripts' `X`; only U13's tail is in scope (it has an explicit
  opt-in replay).
- **NG4 — A new `b0c` capture.** U10 already pins the game's `0x37EA0` write
  at f=0x1602; the final's `DEATH_DONE` poke replaces the mode-0xD wait, not
  that write. The phase does not repeat it.
- **NG5 — Audio audibility, CI/packaging, SDL pad hot-plug and save-path
  docs.** Product chores; the register-stream oracle remains the audio
  evidence.
- **NG6 — New portable code and the tick installer.** The runtime AIL timer
  install stays host-owned; the tick task is documentation only.
- **NG7 — Changing existing captures or pins.** The new captures are
  additive; the only existing pin movement allowed is the U13 tail (G4) and
  the existing U16 capture stays exactly as pinned.

## 5. Anti-goals

- **AG1 — A fitted allowance or pin.** Every new pin is measured from the
  capture/report and fails when moved one step in the tested direction; every
  allowance binds bytes and is mutation-tested. A pin derived from the port's
  own dump is not evidence.
- **AG2 — Claiming a poked path as live.** The U13-style ending captures use
  the WIN_MENU/land/health pokes; the claim is the ending content and the
  game's mode-0x1F path, not the KO by hits or the six earlier won matches.
  Every poke is named in the record and the oracle comment.
- **AG3 — Claiming U14 R1 without a no-poke fought KO.** The two-attempt
  limit is spent; a poked win is not R1.
- **AG4 — Claiming the 5120 cause or writer without sub-frame evidence.**
  The correction is the comment and test coverage, not a fitted cause.
- **AG5 — A green gate with skipped oracles or absent captures.** The phase's
  captures are present; `PR_ORACLE_REQUIRED=1` and the new `make verify`
  entries run them.

## 6. Constraints

- `data/` is read-only. Captures are written only by
  `make gp-capture scenario=<gp-…>` to `data/k11-captures/<scenario>`, and
  only after the session's own `check=ok` lines and `check-input` pass.
- Every new scenario is added to `gp_session.STOP_AT_END`. `gp_capture`
  stops `STOP_TAIL` frames after the `X` and runs the "stopped at the end"
  check only for that set; without it a capture runs to `time_limit` and the
  post-script tail moves `MIN_FIRST`/`MIN_REACH`/`MAX_START`.
- The harness tools are Python 3 and the existing `gp_session` /
  `gp_capture` / `gp_win` / `gp_twop` / `gp_compare` / `gp_moves` modules.
  The port is C/SDL3; the raw source is the Ghidra decomp in `port/decomp/`
  (the live Ghidra MCP has no `primal-rage` instance) and the fixup-applied
  bytes. The EOF marker is the capture's `X` record; `STOP_TAIL` is 60 frames.
- Each new capture pins: `MIN_FIRST` (first unexplained capture frame),
  `TRACE_MIN_FIRST` (first differing traced frame; `MOVES_MIN_FIRST` for the
  U16 path), `MAX_START` (the window's first capture frame), `MIN_REACH`
  (the last exhibited port frame), `CAPTURE_SHA256` (`poll.log`) and
  `CAPTURE_FRAMES` (`frame_*.raw.gz`); the ending captures also pin
  `MILESTONES` and `WIN_MIN_FIRST`. `gp_win.py check` pins the capture sha
  and the milestone count.
- The poked captures: the WIN_MENU path, the seven land marks and the
  `0x10789E=0x78` KO are harness pokes. The game-produced content — the
  mode-0x1F ending, the finisher reaction and the ending path — is what the
  oracle claims. The U14 retake has no pokes.
- No C change is expected. If a capture exposes a port divergence, the fix
  needs the raw instruction, a `PORT:` note, a differential row or an
  existing row's case, and the gate; a divergence that cannot be fixed with
  evidence stays a named gap and the first unexplained frame records it.
- The full gate is `PR_DIFF_FULLSCAN=1 make verify` (the oracles, the
  differential, the E2 triage, `symbols.h`). The new targets are in `verify`;
  each skips only when its capture is absent.
- Docs: `docs/PROGRESS.md`, `README.md`, `AGENTS.md`, `port/RE_GUIDE.md` and
  the record. The README title stays at the current percentage unless a port
  fix adds an `FN_` function (it should not).
- Commit style `<area>: <what>`; stage named files, never `git add -A`;
  commit only when the user has authorized the phase's commits.

## 7. The measured starting point

These are the investigation's measurements; Task 1 of the plan re-derives each
from the bytes and the record is authoritative if they disagree.

### 7.1 The seven characters and endings

Mode 0x1F at `0x208F8` reads the winner's character id
(`DS_0010782A[DS_00104AD4*0x94]`, `0x20900..0x20921`) and indexes the ending
tables `0xA8090`/`0xA80AC` (state-1 actors), `0xA80C8` (state-3 actor),
`0xA7EBC` (state-3 stream), `0xA818C` (state-4 actor) and `0xA80E4` (state-4
string `0xDE..0xE4`). The character select's cell→class table is
`0xC8882 = 00 02 03 01 06 04 05`; the U13 capture confirms cell 3 = class 1
BLIZZARD. The record's title table
(`docs/superpowers/plans/2026-10-02-gameplay-u9-u10-derivations.md:234-242`)
is exact:

| char | character | ending title (`0xA80E4`) | class | cell | verified by |
|---:|---|---|---:|---:|---|
| 0 | SAURON | THE FEAST OF SAURON (`0xDE`) | 0 | 0 | U10 (`gp-u10-ending`, poked) |
| 1 | BLIZZARD | BLIZZARD'S SOLITUDE (`0xDF`) | 1 | 3 | U13 (`gp-u13-ending2`) |
| 2 | TALON | TALON'S DREAM (`0xE0`) | 2 | 1 | this phase |
| 3 | VERTIGO | THE PALACE OF VERTIGO (`0xE1`) | 3 | 2 | this phase |
| 4 | ARMADON | THE MEDITATION OF ARMADON (`0xE2`) | 4 | 5 | this phase |
| 5 | DIABLO | DIABLO'S HELL (`0xE3`) | 5 | 6 | this phase |
| 6 | CHAOS | CHAOS' REDEMPTION (`0xE4`) | 6 | 4 | this phase |

The content is distinct per character on a shared code path; 0/1 do not
generalize to the other five. The `gp_win.py` mode-0x1F label takes the
character name (`_ending_tail(2, 'TALON')` → "TALON's ending"); the ending
title is the `0xA80E4` state-4 string, not a WIN field, so it is claimed by
the record and the compared frames, not by a `gp_win.py` predicate.

### 7.2 The U16 finisher

The three finisher entries are `0x15/0x16/0x17`; their reactions are
`0x32/0x33/0x34`; the callbacks are `0x37640/0x37774/0x37898`. The raw gate is
`0x3766E test byte [ebx+0x42],0x20` and `0x37678 test byte [ebx+0x43],8`;
the only writer of both is `0x36F10` arm A (`0x36FC8 or edx,0x820`;
`0x36FD4 mov word [esi+0x42],dx`). Bit `0x10` of `+0x42` comes from
`0x36E78` (`0x36E9D/0x36EAF`), reached from `0x392A0`'s KO arm at `0x3961E`
when `+0x5A >= 0x78`. The KO path's mode 9 holds `0x258` frames
(`0x28012`). The existing `gp-u16-twop2` capture never KO'd P2 (round 1
`0x41`, round 2 `0x1E`), so mode 9 lasted one frame and every finisher press
fell in mode 0x17. The retry needs a real KO in the deciding round: 160–200
`1A/1B` attempts with a `p1.right` re-approach, or the `0x77` + real-hit
fallback at `after_entry 0x06,2` (the damage poke alone at `0x78` is not the
claim; the fallback is labeled). The existing `gp_twop.finisher` checks the
mode 9/0x12 entries and at least one reaction; the S records carry `s0_43`
and the reaction fields, not the `+0x42`/`+0x43` gate bits, so the gate is
decomp evidence and the reaction is the capture's claim.

### 7.3 The b0c evidence is already U10's

U10's `poll.log` has its first `b0c=1` at f=0x1602 in mode 0x0F, 190 frames
after the seventh poked final KO; `GP_ENDING_WIN_MIN_FIRST=9954` pins it
(`Makefile:1139/1212`). The final's `DEATH_DONE` poke replaces the mode-0xD
wait for the opponent's replacement, not the `0x37EA0`/`0x37FF6` write: the
poked `0x78` KO's death animation still ran and set `b0c`. The final sets
`b14=1`, and `0x36F10`'s arm A is gated on `b14==0`, so the `0x36F10`
finisher write is not the final's `b0c` path. A new b0c capture is therefore
not needed; the spec's earlier "`0x78` skips `0x36E78`" is withdrawn (raw and
record win). What is not claimed is a fought (no-poke) win: U14's
`live_death` checks the `(0x09,1)` anchor, rejects any `DEATH_DONE` write and
requires `b0c=1` after the KO.

### 7.4 U14's failed attempts and the revised script

`gp-u14-win-live` has no pokes: boot, START MENU/LEFT PLAYER ARCADE, the
character-select confirm of SAURON, then 32 alternating `1A/1B` presses per
round. The retry artifact is 7,287 frames and 9,761 S records; round 1 P1 was
KO'd (`s0_5a=0x78`, P2's `s1_5a=0x00`); round 2 P1 was KO'd
(`s0_5a=0x78`) while P2 remained at `s1_5a=0x38`; the match result `ad4=1`
(P2 won), then mode 9/0x17/0x1E, mode 3 at `0x1D68`; the 200 s limit cut the
attract at `0x265D`. The 200 s was the scenario's own limit (the first attempt
lost the shell tool's timeout). The script, not the clock, is the problem: P2
needs ~7–12 landed hits. The planned retry uses 160–200 attempts in round 2, a
`p1.right` re-approach every 8, and `time_limit=260`; it remains conditional
(G3).

### 7.5 The U13 tail and 5120

The capture's `X` is f=10013 (0x271D); its `S` records continue to f=2754
(10068), and the capture's last 13 frames (5891..5903) are the attract's
`CREDITS: 5` screen. `gp_session.port_script` refuses an `--end` past the `X`
(`end > last`), so the port dump ends at 4939 and those states have no port
frame. An opt-in extension to the last `S` is the only blocker.

Capture 5120 (raw 8839) differs from port 4317 (`f=1826`, mode 0x15) by
exactly 13 pixels in rows 1..4, x 21..298: 8 are `00001c` where 4317's index
is `0x32` (black in the frame's DAC), 1 is black where it is `0x2E`
(`100000`), and 4 are black where it is `0x4D` (`00000c`). The 8/1/4 split is
the record's (`docs/superpowers/plans/2026-10-22-pixel-and-proof-closure-derivations.md`
§P3.3); the frame-wide constant-offset test is k=0 with 13 mismatches (the
`{-29,+14,+29}` intersection is an alias); the U10 `DAC[index+1]` carry class
is absent; no dumped present, pair or static index reproduces the band. The
candidate writer (inference, not proof) is the top-corner ornament sprite
`0x01047345` at (9,0)/(285,0); `(3,74)` is outside it. The band is a class-C
sub-frame/comparison-model artifact, not a proven port defect. The
`BANDS_ALLOWED` comment currently says the port's content at the 13 pixels is
"corroborated on both sides"; the neighbours 5119/5122 corroborate the port's
rendered colours at its own indices, not the capture's 13 pixels. Correct
that sentence; keep the 8/1/4 split and the capture-pinned bounds.

The 4248/4320 recipes are sound: 4248 is the unique `scan3_bind(46,97)=3550`
with `top [3549]`; 4320 is `scan3_bind(18,108)=3614` with `top [3613]`; every
row and differing pixel has a holder; the tuple pins and mutation tests are
in `tools/tests/test_gp_compare.py`.

### 7.6 The tick vector

The raw bytes are
`0x66519 b8 08 00 00 00` (mov eax,8),
`0x6651E ba 7e 62 05 00` (mov edx,0x0005627E),
`0x66523 66 8c cb` (mov bx,cs),
`0x66526 b4 25` (mov ah,0x25),
`0x66528 1e` (push ds),
`0x66529 66 8e db` (mov ds,ebx),
`0x6652C cd 21` (int 0x21).
The immediate `0x0005627E` is the code-object offset of linear `0x6627E`
(CS base `0x10000`), so vector 8 points at the handler `0x6627E`. The PIT
routine entry is `0x663A4`; the mode command `b0 36 e6 43` is at
`0x663AC..0x663AF`; the divisor formula at `0x663D4` (`param*10000/0x20BC`)
gives 19,887 for the AIL 60 Hz period 16,666 µs (`0x1CFFA` pushes `0x3C`;
`0x1CFFF` is the `call 0x5DA87`), 59.998 Hz. `0x2D62C` is the 9-byte counter
called by the 60 Hz callback `0x1BDF4` at `0x1BE28`, not the ISR entry. The port
models the ticks in `game_isr_ticks` and the clock in `config.c`; the installer
is runtime (≥`0x5D000`) and stays host-owned.

## 8. The approach

**Endings (G2).** One new scenario per character, cloned from
`gp-u13-ending2`: the same WIN_MENU prefix, the same U10 steps from its
second poke on, the character's cell walk, `lands_marked(0, char)` at
`0x108106`, `0x10789E=0x78`, `time_limit=240`, `memsize=64`, `WIN_EXTRA`.
A shared `ending_menu(cell)` helper builds the cell walk; a shared
`start_char(char, cell)` builds the first two milestones; `_ending_tail(char,
name)` (already present; the second argument is the character name) supplies
the seventh-land-on rows with the mode-0x1F predicate and the character label.
Each new scenario is added to `STOP_AT_END`. The capture is
`make gp-capture scenario=gp-uXX-…`; the oracle is `gp_win.py check`,
`gp-replay`, `gp_compare.py`, `gp_win.py path`.

**U16 finisher (G1).** `gp-u23-u16-finisher` reuses `gp-u16-twop2`'s start
and revised `_u16_round` (160–200 attempts, a `p1.right` every 8), stops at
the real KO, then the `U16_FINISHER` entries from 10 frames into mode 9. The
existing `gp_twop.py finisher` check runs without `--allow-missing-reaction`;
no tool change is expected. The scenario is added to `STOP_AT_END`.

**U14 (G3).** The conditional retake reuses `gp-u14-win-live` with the
revised script; the no-poke rule and `live_death` are the claim.

**U13 (G4).** `gp_session.port_script` gains an opt-in `--end-to-last-s` that
lets `--end` reach the last `S`; the default is unchanged. The `port-script`
CLI exposes the flag and the `gp-u13-oracle` passes it through the existing
`GP_SCRIPT_ARGS`. The measured tail pins are updated only if the port matches.
The 5120 comment/test are corrected.

**Tick (G5).** Documentation only: `port/RE_GUIDE.md` item 4 and
`port/spec/game_flow.md`'s tick section cite the bytes above; an optional
static raw-byte test (no port counterpart) may pin the instructions and the
divisor.

## 9. Acceptance

1. **G1**: `gp-u23-u16-finisher` captured; `gp_twop.py check --finisher`
   passes without `--allow-missing-reaction`; at least one of the
   `0x32/0x33/0x34` reactions is in the S records (the record lists the
   observed set); the new oracle is in `make verify`; the old
   `gp-u16-twop2` pins are unchanged.
2. **G2**: five new captures (`gp-u18-talon-ending`, `gp-u19-vertigo-ending`,
   `gp-u20-chaos-ending`, `gp-u21-armadon-ending`, `gp-u22-diablo-ending`);
   each `gp_win.py check` reaches the mode-0x1F row with `ad4=0,c0=char` and
   the character label; the record's exact title is evidence and the
   mode-0x1F frames lie inside the compared window; each `gp_compare.py` and
   `gp_win.py path` pin is measured and fails one step above; each capture
   identity is pinned; each scenario is in `STOP_AT_END`; each oracle is in
   `make verify`; the `test_gp_win` scenario list includes the five names.
3. **G3**: either the no-poke fought KO retake passes (the game-set `b0c`,
   the mode-8/9 entries, the land, the pins) or U14 stays a named gap with
   the two-attempt limit and the new-go-ahead requirement recorded.
4. **G4**: the U13 tail is either claimed (`MIN_FIRST` 5904 and the measured
   reach/trace/WIN pins) or a first-unexplained divergence is recorded with
   its evidence; the 5120 comment/test match the measured 8/1/4 finding; the
   4248/4320 pins and recipes are unchanged.
5. **G5**: `RE_GUIDE` item 4 done; the tick section cites the bytes at
   `0x66519..0x6652D`, the handler `0x6627E` via the `0x0005627E` immediate,
   the PIT routine `0x663A4`/`0x663AC`/`0x663D4`, 19,887 and 59.998 Hz; no
   port code.
6. **G6**: `PR_DIFF_FULLSCAN=1 make verify` exits 0; every new oracle runs;
   the existing pins are unchanged except the phase's own; `port_progress.py`
   is `799 1203 66` / `759 759 100`; the differential counter is
   `1031/1031`, `14983/14983`, 0 named gaps, `813/844`, `806/844`, 8 unrowed,
   31 open.
7. The record has every measured pin, every poke, every allowance and every
   named gap; `docs/PROGRESS.md`, `README.md`, `AGENTS.md`, `RE_GUIDE.md` and
   the Makefile comments are current; the final review is done.

## 10. Named gaps after the phase

- U14 R1 if the retake is declined or fails.
- The 5120 band's cause (unless a future sub-frame trace isolates it).
- The differential frontier: 31 open rows, the AIL/card/`0x65431`/`0x65546`
  rows, the 8 portable unrowed functions, the 12 weaker rows, `flow_4f5c8`.
- The U12/U15/U16 post-script tails.
- Audio audibility, CI/packaging, SDL pad hot-plug, save-path docs.
- The ending captures' poked wins: the KO by hits and the six earlier won
  matches are poked; the mode-0x1F ending content is claimed, the fight path
  is not.
- The U16 finisher's poked-damage fallback, if the live KO cannot be landed;
  the record names it.
- The tick installer: host-owned; the documentation pin only.
- The game-set `b0c` is not a gap: U10's `GP_ENDING_WIN_MIN_FIRST=9954`
  pins the game's write at f=0x1602; only the fought (no-poke) win is open.

## 11. Risks

- **The captures are the schedule.** Each ending is ~207 s of DOSBox-X (the
  capture's wall is 206.7 s and 14,422 AVI frames at 70.0866; 5,904 are the
  collapsed distinct `frame_*.raw.gz`) plus the replay; the U16/U14 scripts
  may whiff or time out. The plan orders the captures so a failed one stops
  that task, not the phase; the fallback is a named gap or the labeled
  poked-damage path.
- **The U14 retake needs a new go-ahead.** The two-attempt limit is spent; the
  phase does not spend a third without it.
- **A new ending may expose a port divergence.** The contract is the raw
  evidence: fix with a `PORT:` note and a row/test, or record the named gap
  and the first unexplained frame. No fitted allowance.
- **The U13 tail's opt-in change touches a shared script guard.** The default
  must be identical; a test proves the other scenarios' scripts are
  byte-identical.
- **The 5120 cause may stay unknown.** The phase corrects the sentence, not
  the cause; a future sub-frame trace is the route.

## 12. Decisions I made (your call to override)

- **Phase scope:** endgame truth, not differential completion. The
  differential is proof accounting; the endings are player-visible content.
- **Endings:** one capture per character (not a chained session); the content
  tables differ and each needs its own dump/allowances.
- **Poked paths:** the five endings use the U13-style poked win path; the
  claim is the ending content, not the fight.
- **U16:** live KO first, `0x77` + real hit fallback; the reaction is the
  game's either way; the claim is at least one reaction (the existing
  `gp_twop.finisher` rule).
- **b0c:** no new capture; U10 already pins the game-set write. The U14
  `live_death` is the fought-win rule.
- **U14:** conditional on a new explicit go-ahead.
- **U13:** tail opt-in; 5120 cause stays a named gap; comment/test corrected.
- **Tick:** documentation only; no port code.

## 13. What is left to the builder

The measured pins (not pre-filled), the per-capture allowance recipes, the
exact scenario step lists, the `gp_win`/`gp_twop` milestone predicates, the
Makefile oracle wiring, the tests, the record and the gate. The raw bytes win
over this document and over the agents' reports; the plan's Task 1 re-derives
the addresses and the record is authoritative if anything here is wrong.
