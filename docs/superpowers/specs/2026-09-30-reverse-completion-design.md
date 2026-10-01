# Reverse-engineering completion: design

**Status:** design approved in conversation (sections 1-3, 2026-09-30); this file is the written spec awaiting review. Extends `2026-09-30-gameplay-ground-truth-design.md` (U1-U4) to everything still open. Where this file and the earlier spec differ on U5-U11, this file wins.

## 1. Purpose

The function counters read 771/1203 (64%) raw and 731/731 (100%) portable. That
counts functions only. It does not say the original's behaviour is reproduced:
the port already diverges from the original in fights, and a wider scan finds
code the Ghidra function list never contained. This design defines "complete" and
the work to reach it.

**Definition of complete.**

1. Every gameplay path a scripted DOSBox-X capture can reach matches the capture
   frame by frame, under a ratchet in `make verify`.
2. Every other function (the unported move callbacks, finisher entries, voice
   sites and the real entry points among the 575 candidates) is ported and
   verified against the original's own bytes by differential emulation, on the
   inputs the harness can produce.
3. Everything else is a named gap with its evidence.

It does **not** mean "proven identical everywhere". Differential verification
covers the exercised blocks and inputs only, and every record says so.

## 2. Open items (as recorded)

| # | Item | Source |
|---|------|--------|
| O1 | Divergence 1: character-select idle animation reverses about 96 frames late (capture frame 203, raw 2359); cause not isolated | gameplay derivations §G.13-§G.16, §I |
| O2 | Divergence 2: unregistered `0x23208` (character 1's reaction-0x26 entry) is skipped; trace first differs at f=0x828 in `rng` | §G.17-§G.23, §J |
| O3 | 27 of 72 move-table callbacks unported | U0 record |
| O4 | 6 of 10 finisher entries unported (`1567C 15908 23BF8 23EC0 402FC 45D14`); six finisher paths fall back to the `0xC9260` start | U0 record |
| O5 | 66 voice sites outside the ported set | U0 record |
| O6 | 575 plausible entry points Ghidra never listed | U0 record, wider scan |
| O7 | Round 1: the original is a CPU KO after 1148 frames, the port goes to time-up after 3300 (time-up rounds last 3250/3249 in the original, possibly a separate divergence) | §G.17-§G.23 |
| O8 | The original's 1831-frame probe round is unexplained | §G.17-§G.23 |
| O9 | Q3: a queued key is consumed 1-4 frames late; cause open | gameplay derivations |
| O10 | Stale claims: `PROGRESS.md` :301-302 :652, `game_flow.md` :1867-1870 1964-1967 2012-2013 2094, `flow.c` :7136-7140 7166-7169, `host.c` :53-68 key labels, README/AGENTS "100% portable" wording | scoping report |

## 3. Decisions taken with the user

1. **Scope: port all of O3-O6**, whether or not a capture reaches them. Not
   "only if a capture reaches them".
2. **Verification: hybrid.** Captures are the oracle for reachable gameplay;
   differential emulation (original x86 bytes against the port, same snapshot,
   compare writes) is the oracle for everything else; the 575 are triaged first;
   code the emulator cannot exercise stays a named gap.
3. **Bar for the gameplay tracks: reached paths match.** Unreachable items stay
   named gaps (§7).

## 4. Tracks, in order

Each track is its own plan and its own subagent-driven run.

- **T0 — merge U3+U4.** Prerequisite. Brings the frame-compare ratchet and the
  idle-loss run. Gate: `make verify` EXIT=0, the 45 oracle lines equal, WAV
  identical, fresh checkout without the capture skips cleanly.
- **E — differential-emulation harness.**
  - **E1 (spike, then keep):** emulator over the port's `mem[]` layout (`unicorn`,
    else a small 386 interpreter). Self-check: agrees with the port on an
    already-ported function, **disagrees** with a deliberately mutated port.
    Settles the two open questions in §5.4.
  - **E2 (triage tool):** classifies the 575 candidates as code reached by a jump
    table, pointer or caller, dead code, or data; one evidence line per row;
    output is a counted target list.
- **G — capture-driven gameplay.** U5 character-select walk, U6 moves, U7 two
  players, U8 other modes, U11 in-match keys (parallel after T0); then U9 win
  path under pokes, U10 endings. Structure as in the earlier spec. O1, O2, O7,
  O8 and O9 are fixed in the owning unit, each with a ratchet that tightens to
  the new first-unexplained frame, or restated as a named gap with evidence of
  why it cannot be isolated. E can be used to isolate O1 (run the original
  character-select code from a snapshot and find where the port departs).
- **P — port batches, each function verified by E.** Callbacks (O3), finishers
  (O4), voice sites (O5), triaged entry points (O6). Where a capture also reaches
  a function, it is cross-checked against the G ratchet.
- **Closeout.** O10, counters, README percentage, restated named gaps.

**Order:** T0 and E1 first. E2 and G in parallel after that. P starts once E1
passes its self-check. U9→U10 and the closeout last.

## 5. Differential harness (track E)

### 5.1 Contract per function
Start state: a `mem[]` snapshot (dumped by a port driver as the gp captures are),
registers and stack arguments. Original side: the emulator runs from the entry to
a sentinel return address. Port side: the C function runs on a copy of `mem[]`
with the same arguments. Compared: (1) every byte of the image that changed,
stack excluded; (2) the return value; (3) the ordered list of outgoing calls with
arguments.

### 5.2 Calls out
A callee is stubbed identically on both sides and its call recorded: ported
functions, unported functions, the DOS4GW runtime, host I/O. Each function is
verified in isolation; each callee is proven by its own check. The port side uses
the existing `fn_resolve` miss log for the call record. A function whose
behaviour depends on port I/O, interrupts, or an instruction the stubs cannot
model faithfully is **not exercisable**: it becomes a named gap naming the
blocking instruction.

### 5.3 Inputs and the claim
Seeds are real snapshots (boot, attract, gp-capture frames) plus mutations of
the fields the function reads, found by tracing reads in the emulator, not random
bytes. Basic-block coverage of the original is measured. A function is
**verified** only when every reachable block is hit, or each unhit block is named
with the reason. Every record states the narrow claim: equivalence on the
exercised blocks and inputs only.

### 5.4 To settle in E1, not assume
- Addressing: whether data references are plain linear addresses under flat
  addressing, or need the `0x80000` data base (the address model says DS offset =
  address − `0x80000`; fixups write linear addresses).
- Whether `unicorn` installs cleanly on this host (it is not installed now;
  `capstone` 5.0.7 is). Fallback: a small 386 interpreter.

### 5.5 Output
One row per function in a verification table (address, blocks hit and total,
input count, result) and a `make diff-verify` target wired into `make verify`. It
skips when the emulator or the snapshots are absent, like the other oracles
(snapshots derive from `data/` and are git-ignored). Existing oracle lines do not
move.

## 6. Exit criteria

Every item also requires `make verify` EXIT=0 with the 45 oracle lines unchanged
and the WAV identical.

- **T0:** U3+U4 merged on `main`, gate evidence recorded.
- **E1:** self-check passes both directions; §5.4 settled and recorded.
- **E2:** the 575 split into a counted list with evidence per row.
- **G:** U5-U8, U11 captured and ratcheted; U9, U10 ratcheted under pokes; O1,
  O2, O7, O8, O9 each fixed with a tightened ratchet or restated as a named gap
  with evidence.
- **P:** each ported function has a verification row; unhit blocks and
  non-emulable instructions named; counters and README percentage updated.
- **Closeout:** O10 done; remaining named gaps restated with evidence.

## 7. Named gaps that stay, whatever we build

- Physical keyboard and joystick paths (claims start at the key bitmap).
- Wins and endings without pokes.
- Digital voice audio (call sites wired and verified; the sound is not compared).
- STATISTICS after play.
- Slow-frame presentation.
- Any function the emulator cannot faithfully exercise, listed individually.

## 8. Cross-cutting rules

- Raw wins; never a fitted constant; a value that cannot be pinned is a named gap
  with its evidence.
- `make verify` is the gate after every task. Tests use only
  `CHECK`/`CHECK_EQ_INT`; every assertion must be able to fail (seeded
  sentinels, mutation proofs).
- `data/` is git-ignored and read-only; only `data/k11-captures/` is written, by
  the tools' own paths.
- One C function per original function, header `/* 0xADDR — spec section */`;
  deviations `/* PORT: ... */`.
- Stage named files only; commit style `<area>: <what changed>`.
- Merge and push only reviewed, gated heads.

## 9. Risks

- The harness may not model the DOS4GW runtime faithfully, which shrinks P into
  named gaps rather than weakening the evidence.
- Differential verification covers only the inputs the seeds produce.
- Scale: about 575 candidates (before triage), 27 callbacks, 6 finishers, 66 voice
  sites, on top of U5-U11. Triage in E2 sets the real count before porting
  starts, and the plan may split P into several batches.
- U3 and U4 are built but not yet merged (T0); G depends on them.
