# Closing the six named gaps: design

**Status:** design approved in conversation (decomposition A-D, order below). Plans A-D are written (`docs/superpowers/plans/2026-09-30-named-gaps-{a,b,c,d}-*.md`). §8 records what planning found; where §8 and §2-§7 differ, §8 wins (raw wins).

## 1. Purpose

After K7+K12 (main `475eafa`) the port has no unported portable function and no
unwired voice call. Six named gaps remain. Each is a place where the port
substitutes something for what the original does, or where the original's
behaviour is not covered by evidence. This design closes each with evidence
from the raw bytes or a DOSBox-X capture and a test that can fail, or restates
it as a smaller named gap with the evidence. Nothing here changes behaviour the
oracles observe today.

## 2. The gaps (as recorded)

| # | Gap | Where recorded |
|---|-----|----------------|
| G1 | The idle-timeout `longjmp` is not modelled | ledger mode-switch table row `0x27`: `0x251C6`, else `jmp 0x65431` (a `PORT:` deviation, spec §7); K7+K12 record §2.6 (idle-timeout store) |
| G2 | The `0xFFE80003` read is outside `mem[]` | `svcmenu.c:992` (`0x32573..0x32578`, TEST CONTROLS RAW DATA row), ledger §E row 32, K11 record §K11.5 |
| G3 | The `0x33458` `idiv` fault is drawn as 0 | `svcmenu.c:1192` (`0x334CD..0x334E2`, STATISTICS page 2), ledger §E row 33, K11 record §K11.7 |
| G4 | Both allocation-failure arms of `0x1D0BC` are untested | K7+K12 record §2 (line ~390-405): the MIDI-buffer arm (zeroes `C4`, `C0`, `CC`) and the sample-buffer arm (loop stops at the first failure; slot 0 empty sets `DS_001028C8 = 0`) |
| G5 | No oracle reaches the K11 service-menu code | K11 record §0.7, §K11.9 |
| G6 | Headless sample slots never end, so they saturate | K7+K12 record §0.7.6 and §7.6; `AIL_sample_status` (`ail.c`) only ends a one-shot when a mixer position advances, and with no audio device nothing advances |

## 3. Decisions taken with the user

1. **G2 and G3: faithful abort.** Model what the original really does, from the
   raw and DOS/4GW, not a safe stand-in. If the original aborts to DOS, the port
   stops with the same behaviour; if a handler swallows it, reproduce that.
   Evidence decides which.
2. **G5: capture with DOSBox-X** (`/opt/homebrew/bin/dosbox-x` is installed).
   Drive the real game into the service menu and capture frames and register
   writes, then add a byte-exact oracle. If scripted input cannot reach a path
   on this host, the spec of that sub-project says so and the path becomes a
   smaller named gap.
3. **G6: virtual mixer clock.** With no audio device the host mixer advances a
   virtual playback position from the game tick, so one-shots end at their real
   length and slots free. Same code path as a real device.
4. **G1: real `setjmp`/`longjmp`** at the raw's matching site if it is single;
   otherwise explicit unwinding. The raw decides, and the choice is recorded.
5. **G4: a test-only failure-injection seam** (`PORT:`) in the allocator.

## 4. Sub-projects (in order)

Each is its own plan and its own subagent-driven run. A comes first because its
capture is ground truth for B.

### A. K11 ground-truth harness (G5, and evidence for G1/G2/G3)
- A scripted DOSBox-X session (keyboard/joystick injection, memory-file dump or
  frame capture as the existing title/attract captures do) that enters the
  service menu, visits START MENU, CONFIGURE, STATISTICS, TEST CONTROLS RAW
  DATA and the idle timeout, and records frames and register writes.
- A byte-exact K11 oracle, gated on its capture like the others, added to
  `make verify` (skips silently without the capture, per AGENTS.md).
- Deliverable also answers: what does the original do at `0xFFE80003`, on the
  `idiv` `#DE`, and at the idle-timeout `longjmp`? Each answer is recorded with
  the capture evidence.
- Acceptance: the oracle passes on the port for the reached paths; every path
  the capture cannot reach is listed with the reason; existing oracle lines do
  not move.

### B. Fault and control-flow gaps (G1, G2, G3)
- G2/G3: implement the original's behaviour as A establishes it, with a
  `PORT:` note only if a host-level stand-in is unavoidable (for example a DOS
  abort message). A test that can fail per gap, plus a mutation proof.
- G1: model `0x251C6` case `0x27`'s `jmp 0x65431` per §3.4. The return target,
  the state it leaves in `mem[]` and the timing come from the raw and A's
  capture. A test drives the idle timeout and asserts the post-state.
- Acceptance: the three ledger rows (§E rows 32/33 and the mode-switch `0x27`
  row) are closed or restated with evidence; `make verify` green.

### C. Allocation-failure seam (G4)
- A `PORT:` seam in the port's allocator (`res_alloc` or the bump allocator
  behind `sound_buffers_alloc`) that a test can arm to fail the N-th request,
  off by default and inert in production.
- Tests for both arms: MIDI-buffer failure (`DS_001028C4`/`C0`/`CC` zeroed) and
  sample-buffer failure (loop stops at the first failure; slot 0 empty sets
  `DS_001028C8 = 0`; the slot-0-already-set entry case). Seeded sentinels and a
  mutation proof per arm.
- Acceptance: both arms tested; the seam adds no behaviour when unarmed.

### D. Virtual mixer clock (G6)
- When `host_audio` has no device, the mixer's sample position advances from
  the game tick at the profile's sample rate, so `AIL_sample_status` reports
  the end of a one-shot at its real length and slots free.
- Constraints: the FM `make audio-render` WAV stays byte-identical to
  `before-t2.wav`; the `--check` probes for the attract's looping samples
  (`0x40`, `0x42`) still pass; no game code reads sample status (only
  `main.c`'s probe does; verified in the Task 7 review); oracle lines do not move.
- Acceptance: a headless run frees a slot after a one-shot's length; the
  saturation observed in `--check` (BD/BE/BF fire 41/14/18 times, four slots
  full) no longer occurs; tests assert the end time from the sample's length.

## 5. Cross-cutting rules

- Raw wins; never a fitted constant; a value that cannot be pinned is a named
  gap with its evidence.
- `make verify` is the gate after every task; oracle lines equal the baseline
  (`§A` of the all-gaps ledger); the enforced front-end oracle, the demo-fight
  ratchet and the attract cycle-2 ratchet stay green.
- Tests only `CHECK`/`CHECK_EQ_INT`; every assertion must be able to fail; every
  untested arm goes in the record's "Not tested".
- Subagent-driven development with a ledger and a derivation record per
  sub-project; the Ghidra MCP is unavailable, so raw analysis uses the LE
  mirror (`le.py`, capstone) and DOSBox-X.
- Merge and push only when asked.

## 6. Risks and unknowns

- **A may not reach every path.** DOSBox-X scripted input on macOS may not
  reproduce the exact game state (for example the idle timeout needs a real
  time wait; `0xFFE80003` needs the diagnostic flag set). Mitigation: fall back
  to a raw-only model and record the path as a named gap.
- **G2/G3 originals may abort.** A DOS/4GW abort has no clean host analogue; the
  port may need a `PORT:` termination path in `main.c`/`host.c` only, per the
  I/O rule.
- **G1's `setjmp` site may not be single.** Then explicit unwinding, which
  touches more of the frame loop; the oracles would catch any drift.
- **D changes the observable state of headless runs** (slots free). The `--check`
  probes and the WAV are the guards.

## 7. Out of scope

- Any new gameplay behaviour, new ports of functions, or changes to the
  oracle-visible rendering.
- The 81 host-owned/deferred and runtime functions the classification file
  excludes.
- Real audio-device output on this macOS host (SDL audio cannot open here).

## 8. Corrections found while planning (raw wins)

Found by the four read-only planners against the raw mirror. Each is recorded
with addresses in the owning plan.

- **G1 has three `longjmp` sites, one `setjmp`.** `0x65431` is WATCOM `longjmp`
  and `0x653FC` is `setjmp`; the single `setjmp` is at `0x20C1F` inside
  `0x20C10` (its return value is discarded). The jumps come from `0x2EBB3` (the
  real idle timeout in `0x2EB80`: `[0x101500] - [0x105F2C] > 0x4B0`, unsigned),
  `0x2520B` (case `0x27`, menu result not 0/-5/-10) and `0x24AB0` ("ABANDON
  CONQUEST? Y/N" answered yes). All three are a soft restart (RNG re-seed
  `0xABCD`, `game_state_init`, re-entry at `0x20DE8`). The port's master-loop
  spin never advances `DS_00101500`, so the idle timeout cannot fire today, and
  the port currently quits to DOS at `0x24AB0` where the original restarts
  (fixing it changes two existing test expectations). Plan B closes all three;
  the restart lands in `game_loop()` as a `PORT:` because the test drivers step
  one frame per call.
- **G2 is unreachable in the stock game.** The diagnostic arm needs
  `DS_00107410 & 0x10` (config field `0x2A` bit 4, stored at `0x2FA10..0x2FA1C`);
  the field is 4 bits wide (descriptor `0x1B80`) and its only stores clear the
  low two bits, so only a saved config or a memory poke reaches it. Plan A's
  DIAGS capture is a "what if" run; Plan B has an ABORT-RAW branch for the case
  the capture cannot say.
- **G3:** the `#DE` fires when `v != 0 && (v & 0xFFFF) == 0`; STATISTICS row 2
  (fields 8 + 6) reaches it with field 8 = `0xFFFF` and field 6 = `1`. The game
  installs no fault handler (no DPMI `0203h`, vector 0/0Eh unhooked), so DOS/4GW's
  default handler runs; what it prints is left to Plan A's capture.
- **G6: game code does read sample status.** §4.D said only `main.c`'s probe
  does; that is false. Five sound routines call it (`0x1CE70`, `0x1CE04`,
  `0x1CD9C`, `0x1CED4`, `0x1CC28` twice) and the voice dispatcher `0x2C3FC` skips
  queueing when `0x1CE70` says a sample plays, so freed slots change which slot a
  sample takes and whether a voice is queued. The exposure is argued bounded (the
  loading screen draws on an entry's first resolve only, no RNG on the path) but
  the gate (oracle lines, dumps, WAV) is the proof. Plan D's clock is not a
  host-seam "frames consumed" count (the port pushes audio; nothing pulls): it
  lives in `game_audio_service` (`0x1CF20`), driven by the 60 Hz ISR tick
  (`0x1CFFA push 0x3c`; the 60.05 Hz figure is a DOSBox measurement), and calls
  the device path's own `mixer_render`. One behaviour it restores: voice `0xBD`
  shares handle with `0x42` as a one-shot, so a stuck `0xBD` slot makes the
  dispatcher refuse `0x42` in the second attract cycle today.
- **G4:** the arms are described in K7+K12 record §0.7.1 (lines ~388-406) and
  the "Not tested" entry is §2.4; the two stores at `0x1D0F7` and `0x1D163` write
  places that must already be 0 and stay untestable residue.
- **Dependencies between branches.** Plans C and D close ledger rows (§H.3) that
  exist only on `all-gaps-final`; run them on a branch that contains it.
- **Captures live under `data/`.** `AGENTS.md` says `data/` is read-only, but the
  `make title-capture` target is the precedent for a git-ignored capture folder;
  Plan A's `data/k11-captures/` follows it and its tool refuses to write
  anywhere else. `le.py` is not in the repo, so plans use capstone on the raw
  file.
