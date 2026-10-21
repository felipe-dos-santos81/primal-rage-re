# Gameplay truth II — the unexplained tail and the held captures (2026-10-20)

Status: DRAFT — written from the 2026-10-14 next-phase recon, the track G/J/A
records, and a 2026-10-20 re-read of the current pins and tools. Builder-checked
2026-10-20: its three findings are folded in (W1 is a tail pass, not a single
frame; W8 is a raw verdict, not a capture; W5's death-animation test is
shared-code, not same-stream). The open calls are the controller's, overturnable.

Evidence base: `AGENTS.md` (oracle paragraphs, differential counter),
`docs/PROGRESS.md` tail (tracks G, J and sub-project A),
`.superpowers/planning/2026-10-14-next-phase/03-gameplay-frontier.md` (ranked
items 2-9), `docs/superpowers/plans/2026-10-15-reverse-j-closeout.md` Part II
Tasks 8-13 (the held capture contracts; Task 13 is re-scoped to a raw verdict in
W8), the track-G record
`2026-10-14-reverse-g-derivations.md` §G6/§G7, and the U9/U10 record
`2026-10-02-gameplay-u9-u10-derivations.md` §W.12-§W.16. Current pins are the
`Makefile`'s: U9 2452/3503, U10 4113/9954, keys 1136/2164, idle-loss 7559/8320,
charsel 516/1513, moves 2139/3248, twop 612/1506.

## Intent and the why

The port is playable end to end, the portable set is fully ported (747 of 747),
and the differential ledger is `1007/1007 functions VERIFIED; 14900/14900
mutants detected; 1 named gaps; 774/832 rows with callees closed (175 have
none)`. The remaining weakness is not a missing function. It is that the last
player-visible claims are still trace-only or uncaptured.

Track G moved the pixel claims to U9 2452 and U10 4113: the mode-8 three-frame
scan-out is now a named allowance (`SCAN3_ALLOWED`) and the keys frame ratchet
is at 1136. The first genuinely unexplained run begins at **U10 capture 4113**
(port `f = 0x1502`, mode `0x0D`: the poked final's fourth death animation). The
track-G report caps its unexplained list at five (`REPORT_MAX`), and the record
names only 4113 and 4114, so the run's full extent and everything after it — the
rest of the final, mode `0xF`, the ending and the high-score entry — was never
classified at all. That tail, and several entire modes, have no pixel evidence.
The five replay captures
designed in the track-J plan — ENDURANCE's fight, a second character's ending, a
real won match, a mid-match join, and an extended two-player match with a
finisher — are the evidence that covers those modes. The joystick question is
settled from the raw, not a session (W8). The win-live capture tests the shared
death-animation machinery that frame 4113 reaches.

So this phase is **evidence, not porting**. It either explains the first
unexplained run beginning at 4113 or names it with its evidence, and it turns the
five held replay capture designs into pinned oracles and the joystick question
into a recorded verdict. A green `make verify` at the
end should mean more than it does today, and no pin may move toward the port to
get there.

Success looks like: the first unexplained run is resolved to a fix, a named
poke artifact, or a named per-frame allowance, and the rest of the tail is
classified; the five replay captures exist (session-gated) with clean
`check-input` and pinned replay oracles; the joystick
verdict is recorded; every existing pin is unchanged or has a recorded
re-measure; and the docs say exactly what the new evidence does and does not
prove.

## Goals

### W1 — U10 4113 and the unclassified tail

The track-G measurement stops at the first five unexplained frames because
`gp_compare.py`'s report caps at `REPORT_MAX = 5`; the record names 4113 and
4114 but not the run's end or the tail after it. W1 therefore has two steps.

**Step 1 — classify the whole tail.** Run a full classification from 4113 to the
capture's last frame with the cap lifted (a report/scratch pass, not a pin), and
list every unexplained run with its nearest port frame, rows and pixel count.
This is the evidence the phase needs; a per-frame allowance for 4113 alone
cannot move the ratchet past 4114.

**Step 2 — resolve the first run, then each later run in order.** For the first
run beginning at 4113, decide which class it is:

- **A — a real port divergence.** The original's death-animation content
  (stream, sprite layout, palette, or the `rng(0x10)` draw) differs from the
  port's. Fix it from the bytes, with a failing-first unit test and a
  differential row where the changed function is rowed. The raw anchors are the
  death-animation end `0x37EA0`, the `0x37DD4`/`0x29C78` streams, and the
  mode-`0xD`/`0xF` difference already recorded in §W.16.
- **B — a poke artifact.** The poked final reaches a state the un-poked game
  does not, and the run is not evidence against the port. Prove it from the
  capture's `W` records and state, record the named gap, and let the win-live
  capture (W5) test the shared death-animation machinery. The B verdict is
  limited to that shared claim; it cannot prove SAURON's stream content.
- **C — a capture/scan-out artifact.** The frames are a splice the model does
  not express. Add named, per-frame, byte-checked allowances in the
  `SCAN3_ALLOWED`/`OVERLAY_ALLOWED` discipline. Never add a general N-frame
  model.

The pin moves only when the first run is resolved; a later run becomes the new
pin and its own task. No outcome re-pins toward the port.

### W2 — the capture harness prep (no capture)

Add the five replay scenario definitions and their plumbing before any session
is opened, so a capture session is only the capture:

- `tools/gp_session.py`: `gp-u12-endurance`, `gp-u13-ending2`,
  `gp-u14-win-live`, `gp-u15-join`, `gp-u16-twop2`. Each carries its steps,
  time limit, extra snapshot fields, pokes and `memsize` as its design
  requires. There is no `joystick-probe` gp-* scenario: W8 is a raw verdict,
  and a report-only Alt-J/boot probe, if wanted, is a K11-style session outside
  this harness.
- `tools/gp_capture.py` / `tools/gp_compare.py` / `tools/gp_win.py` /
  `tools/gp_twop.py`: the judge rules the new scenarios need (the join rule
  already exists; the win-live evidence check is new).
- `Makefile`: empty/skipped `GP_U12_*` through `GP_U16_*` blocks and targets,
  with the provenance-comment shape the existing blocks use. **No pin value is
  guessed**; a block stays empty until its capture is measured.
- Unit tests for the scenario definitions and judge rules, and a skip test for
  each target when its capture is absent.

This work is testable with no DOSBox-X session: the definitions import, the
drivers skip cleanly, and the existing pins do not move.

### W3 — `gp-u12-endurance`: the fight past the D3 cut

Extend `gp-u8-endurance`'s session through the `0x44798` team-pass picks and
modes `0x30`/`0x31`. This is the only ownerless named gap (§U8.9). The port
already replays to the 300-frame cut; the new scenario name is required because
the old capture and its pins stay untouched.

Claim: frame, trace and, where the snapshot carries them, moves, as far as the
comparison model carries them; `check-input` clean; pins `GP_U12_*` measured
from the capture. The frame claim may stop at the fight's own scan-out; that is
a named limit, not a failure.

### W4 — `gp-u13-ending2`: a non-character-0 ending

Clone `gp-u10-ending` with the WIN_MENU cursor moved off character 0; reuse its
pokes and `WIN_EXTRA` fields. This proves an endings-table entry §W.6 leaves
open and gives an independent second ending-screen content proof.

Claim: trace, milestones and frame as far as the model carries; the modal
content past the final's death still depends on W1's outcome. A different
character may also exercise a different death stream, which feeds W1.

### W5 — `gp-u14-win-live`: a won match without pokes

Script enough landed hits, from `gp-u6-moves-b`'s move knowledge, to win a
match without the WIN_MENU pokes. Goal: kill §W.9's "what a poke replaced is
not claimed" for one match — the fought KO, the death animation and `0x37FF2`.

The builder check narrowed what this proves: U10 4113 is SAURON's stream
(character 0, the fourth final opponent), while a win-live first match is
CHAOS's (character 6, stage 5's P2), a different one of the seven stream copies.
They share the death-animation machinery (`0x37EA0`, the remains actor, the
`rng(0x10)` draw, `0x37FF2`) but not the stream data. So W5 is a **shared-code**
test, not a same-stream test: it can confirm the machinery and the state
transition, not that SAURON's stream content is right. W1's B verdict is limited
the same way and must say so.

The death-animation frames come at or after the round-end mode-8 entry, where a
new capture's frame claim stops at the three-frame scan-out unless a measured
`SCAN3_ALLOWED` entry is added. After the replay is measured: add the allowance
if the scan-out recurs, then compare the frames; otherwise the evidence is the
`0x37FF2` state transition and the trace, and the claim says so.

Risk: CPU determinism against a frame-keyed script. The capture may need the
§G.19 pick re-alignment. If a deterministic script cannot be made, record the
attempt and the evidence; do not weaken the claim or fabricate a pin. Use
`memsize=64` only if the original's OOM is observed (the U10 precedent); it is a
harness value, not a claim.

### W6 — `gp-u15-join`: mid-match join

Press F2 during a round (`0x28CC8`) and capture the join. Reuse the join
judge-rule already in `gp_keys`; the scenario is short. This covers the
§U8.9/§T uncovered 2P entry (the character-select join is already captured by
`gp-twop`).

Claim: state + frames as far as the model carries; the free-credit join
(`0x2CA93`) is the raw evidence.

### W7 — `gp-u16-twop2`: extended two-player + finisher

A new scenario (never re-capture `gp-twop`): play past the first match's script
end and land a finisher (all finisher entries are ported, but no capture reaches
them). Goal: raise the two-human claim past its script end and give the finisher
frames their first capture evidence.

Claim: frame, trace and moves as far as the model carries; the finisher's score
and reaction path are the new content.

### W8 — the joystick verdict (raw evidence, no capture)

The game logic has no joystick path. `0x2D2F0` is literally `xor eax,eax; ret`
(it has its own differential row), so `0x4F644` stores 0 in
`0x1088D4`/`0x1088DC`; its gate at `0x4F68B` tests bit 1 of `0x1088D4` and
never opens, and the dead `0x4F694` merge would merge `0x1088D8` (the held
mask), not a joystick word. The original's joystick is the host-owned ISR
sampler `0x1BBAC`'s device-2/4/6 arms: the key-config device words
`+0x2D4/+0x2D6` select keyboard 0 or joystick 2/4/6, and the sampler writes the
same key bitmap `+0x2D8/+0x2D9` that `0x500C4` reads. The game-port readers are
the sampler `0x1BBAC`, the calibration `0x5004A` (Alt-J and boot), and the boot
checks `0x20DAC`/`0x20DD1` (which call the host-owned `0x4FBBB`); the readers
are host-owned, and the port's host models only the sampler's device-0
(keyboard) arms. So an SDL-gamepad map is **additive**
host input, not a reimplementation of a ported path; if faithful joystick
behaviour is wanted, model the sampler's device-2/4/6 arms (the
`|diff| >= 0x1E` axis thresholds — a ±0x1D dead band — and the `in 0x201`
button bits), not `0x4F644`.

W8 is therefore a documented verdict, not a capture: record the addresses and
the `0x4F68B` gate in `AGENTS.md`/the classification notes, and make the host
phase's gamepad decision from it. If the user wants the original's Alt-J or
boot controller probe behavior anyway, that is a separate report-only K11-style
session, not a `gp-*` replay fixture and not a pin.

### W9 — closeout

Re-measure every pin the phase touches; run `make verify`; ensure no existing
pin or oracle line moves without a recorded reason. Update `docs/PROGRESS.md`,
the `AGENTS.md` Commands/oracle paragraphs and the derivation records; update
the README title only if a ported function is added (expected: none). Each
capture task writes its own record section (the track-J `§J4`..`§J9` shape).

## Non-goals

- **The differential last mile.** The 58 open rows, 8 unrowed functions and 12
  weaker rows are a separate audit phase; the 2026-10-14 recon measured them as
  mostly bookkeeping and a closure-rule judgment, not novel RE.
- **Audio fidelity.** Rendered-audio fidelity, a PCM/DIG capture oracle, the
  movie-audio lines, and the sample/mixer path are out of scope. The FM
  register-stream oracle stands as it is.
- **Host/product work.** SDL gamepad implementation, CI, packaging,
  cross-platform builds, pacing/display policy and the pause-clock/language
  reload are out of scope. W8 produces evidence only; the gamepad decision and
  any implementation belong to the host phase.
- **New engine features and runtime porting.** No portable functions remain;
  the 231 span writers stay under decision D2.
- **Re-capturing an existing scenario under its own name.** The sha256 pins
  make that a new scenario, not a re-pin.

## Anti-goals

- A green gate achieved by re-pinning a ratchet toward the port.
- A general N-frame splice model; only named, per-frame, byte-checked
  allowances.
- Captures with unscripted input, or any write to `data/game/C`.
- Claims the evidence does not carry: no under-render claim inside a window,
  no frame-order or full-coverage claim, no claim past a capture's end. The
  existing narrow-claim language stays.

## Constraints (binding)

- The byte-exact oracle lines never move. Every new pin is measured from its
  capture, with its provenance in the `Makefile` comment and the record, and
  fails at pin + 1 (the existing ratchet discipline).
- Every new assertion is mutation-proven able to fail; every new test binding
  stays inside `mem[]`.
- One capture at a time, each preceded by its short design and the user's
  capture go-ahead (the track-J Part II rule). Nobody types into the DOSBox-X
  window; `check-input` must be clean.
- `data/` is git-ignored and read-only except the harness's own capture
  directory; `data/game/C` must be proven untouched by the capture's staging
  check.
- A fresh worktree needs the git-ignored fixtures (`port/tests/ghidra_data.bin`,
  `port/tests/title_screen_ref.ppm`) before `make verify`.
- Commit style `<area>: <what changed>`; stage named files; never `git add -A`.
- The full gate is `make verify` (~26 minutes); a phase task runs its owning
  target and the suite, and the controller runs the full gate at the close.

## Acceptance

1. The U10 tail from 4113 to the capture end is classified; the first
   unexplained run beginning at 4113 is resolved to W1's A, B or C, with the
   evidence, the pin consequence and the record; any later run is named as the
   new pin and its own task. The win-live capture tests the shared
   death-animation machinery (not SAURON's stream), and its claim says so.
2. Each of the five replay captures either exists with a clean `check-input`
   and its pins/target in `make verify`, or is named with the user's decline and
   the reason.
3. The joystick verdict is recorded from the raw evidence (the game logic never
   reads the joystick; the original's joystick path is the host-owned ISR
   sampler `0x1BBAC`'s device-2/4/6 arms; `0x4F644`'s gate never opens; an SDL
   map is additive host input); any Alt-J/boot probe the user requests is
   labeled report-only.
4. `make verify` exits 0; every existing pin and oracle line is unchanged or
   has a recorded re-measure; no fitted constants.
5. `docs/PROGRESS.md`, `AGENTS.md` and the README are truthful about what the
   new evidence proves; the ported-function counter is unchanged (unless W1's
   fix adds one, in which case `port_progress.py` and the README title move in
   the same commit).

## What is left to the builder

- The exact key scripts for the five replay scenarios (frames, holds, steps),
  derived from the raw and the existing `gp_session.py` helpers.
- The full-tail classification pass's shape (a raised-cap scratch run) and the
  later-run task split.
- The measured pin values for each capture.
- W1's root cause and the fix/allowance, from the bytes and the capture.
- The win-live determinism plan (hit landings, pick re-alignment if needed) and
  its `SCAN3_ALLOWED` decision.
- Whether a capture session can be made at all, and when.
- The task decomposition, worktree strategy and branch names.

## My calls (overturnable)

- **The phase is evidence-first.** The differential leftover is a separate
  audit phase; it does not compete for this phase's capture sessions.
- **W1 runs before the capture wave and classifies the whole tail.** The
  existing U10 capture is enough; the `REPORT_MAX` cap is a report limit, not
  evidence that only five frames differ. The answer shapes the win-live script
  and the ending2 claim.
- **The five replay captures keep the track-J names and contracts.** The old
  `gp-u8-endurance` capture and pins stay; new evidence gets new names.
- **The joystick probe is replaced by a raw verdict.** The game logic never
  reads the joystick; the original's joystick path is the host-owned ISR
  sampler `0x1BBAC`'s device-2/4/6 arms, and `0x4F644`'s gate never opens
  (`0x2D2F0` is a stub), so an SDL map is additive host input. A report-only
  Alt-J/boot session is optional and is not a `gp-*` fixture; no SDL gamepad
  code lands here.
- **A capture that cannot be made is a named gap, not a weakened claim.** The
  phase can succeed with fewer than five replay captures if each missing one is
  named with its evidence and the user's decision.
