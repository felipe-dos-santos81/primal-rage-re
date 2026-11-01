# Driver Truth — plan

**Status (2026-10-31):** Tasks 1–3 and 5–6 are implemented: the driver-state
model is derived (record §D1), the fix and its unit cases are in, the 29-trace
oracle is re-pinned and green (86 prefixes / 44,489 ticks after §D5), and the register
gets R35. **Task 4 (`0x21` spec) was not taken in the phase**: the subagent
fleet was unavailable (the provider reported insufficient account funds) and the
audio derivation took the session; R05 stays declared with no capture. **Both
leftovers were taken on 2026-11-01 without subagents:** the spec is
`port/spec/volleyball.md` (R05's derivation half, no capture and no claim
change), and the §D3 "Not fixed" `1895A942` row's cause is fixed by the driver's
sustain model and the row is re-pinned (record §D5, register R36; the row's
first difference 177 → 205
and the prefix total 44,461 → 44,489).

**Gate (Task 6):** `make verify` exited 0; the audio summary is `ok`, the
pin baseline is `25202638…`, and `make audio-render` twice is byte-identical
with the unchanged PCM pins.

**Design:** `docs/superpowers/specs/2026-10-31-driver-truth-design.md`.
**Record:** `docs/superpowers/plans/2026-10-31-driver-truth-derivations.md`.
**Ledger:** `.superpowers/sdd/2026-10-31-driver-truth/progress.md`.
**Register:** `docs/superpowers/specs/2026-10-30-final-claims.md`.

The phase fixes the port's invented AIL driver-state reset (register R10),
re-measures the 29 gameplay traces, re-pins the audio oracle, and documents
the unexhibited mode `0x21` sub-game (R05). No capture.

## Task 1 — Derive the driver-state model from the raw bytes

**Goal:** replace the design's prediction with a measured model, one address
per statement.

1. Read `0x5DE79` (`AIL_start_sequence`), `0x6A780`, `0x69250`, the install
   (`AIL_startup` / `0x306e` family), `AIL_init_sequence` (`0x5DE48`), the
   program-change handler, the controller handlers, and the note-on/off
   paths in `port/decomp/prage.c` and the decomp index.
2. Resolve `0x69250`'s `+0x90 + i*4 = i` explicitly: state whether it is a
   voice map, a program cache or something else, and reconcile it with the
   measured capture (the capture keeps program 12 across starts).
3. State the persistence model: every per-MIDI-channel field, whether the
   start preserves it, and where it is initialised.
4. State the patchless-note rule: what the driver does when the program has
   no `FAT.OPL` entry (`patches_lookup` NULL today keys frequency only).
5. State the release/retrigger ordering: what the driver does when a tick
   carries an event for a voice whose release expires that tick.
6. State the install defaults that must reproduce today's first-song output.

**Acceptance:** record §D1 has one address (or `file:line`) per statement; the
`+0x90` question is answered; the four rules are stated as testable
conditions. No code change in this task.

## Task 2 — Implement the model and its unit tests

1. `seq_start` stops resetting per-channel driver state; the reset becomes the
   install state (static initialiser + `seq_driver_reset`, whose contract
   widens from the cursor to the whole install state, still test-only).
2. Keep the raw's `0x69250` reset in `seq_start`: voices silenced, sequence
   record cleared (`S.pos`, `S.wait`, `S.svc`, `S.cc7_logged`, voice array).
3. Implement the Task 1 patchless-note rule and release/retrigger ordering.
4. Unit tests in `test_audio.c`: (a) a second start preserves program/bank/
   volume/expression/pan; (b) a note whose patch is absent keys nothing;
   (c) the retrigger ordering case. Each assertion must fail under a named
   mutation.
5. Build and run `test_sequencer` and the audio cases.

**Acceptance:** build green; the three new behaviours tested; each test
fails under its mutation (recorded); the first song's output unchanged.

## Task 3 — Re-measure the 27 traces and re-pin

1. `make audio-oracle`, capturing each scenario's section table.
2. For each of the 10 `TRACE_RESIDUE` rows: record the new status and first
   difference. A row that is now a whole-section match moves to
   `TRACE_CLAIMED_BANKS`; a row that still differs keeps its measured tick and
   cause (a new tick must be re-pinned).
3. Update `tools/audio_compare.py` (the residue/claimed tables, the counts,
   the partition test's expected sizes) and `tools/tests/test_audio_compare.py`.
4. Re-run `make audio-oracle`; confirm the partition still covers 26 banks and
   the prefix pins.
5. `make audio-render` twice; record the new selmo PCM pin and its reason.

**Acceptance:** `make audio-oracle` green; every moved bank is a whole-section
match; every remaining residue row carries a measured tick and cause; the
12+10+2+2 partition still holds; the two `audio-render` runs are
byte-identical.

## Task 4 — Document the mode `0x21` sub-game (R05)

1. Write `port/spec/volleyball.md`: entry gate chain, court actors, ball
   states, scoring, serve/point pause, timeout, end screen, strings and
   voices, each with the ported function and raw address.
2. Add the record's §D4 route assessment: the measured preconditions, the
   search dimensions, the harness shape, and the poke-labelled fallback.
3. Link the spec from the register's R05 row. R05 stays **declared**: no
   capture, no oracle, no claim.

**Acceptance:** every rule has a citation; the spec agrees with the ported
code; the register's R05 evidence names the spec; no claim is widened.

## Task 5 — Reconcile the register and the docs

1. Update `docs/superpowers/specs/2026-10-30-final-claims.md`: R10 becomes the
   measured split (fixed rows closed, remaining rows terminal with the
   comparison-model reason), §3's audio counter and PCM pin re-measured, the
   superseded row wording recorded.
2. Update `AGENTS.md`'s music-truth bullet, `docs/PROGRESS.md` (a task
   paragraph per task), `port/RE_GUIDE.md` if it carries the old wording.
3. Run `python3 -m unittest tools.tests.test_final_claims
   tools.tests.test_audio_compare tools.tests.test_check_docs_claims`.

**Acceptance:** the docs guard green; no row open; every status change names
its fix and pin; the superseded wording table updated.

## Task 6 — The gate

1. `PR_ORACLE_REQUIRED=1 ./build/run_tests`.
2. `make verify` (no `PR_DIFF_FULLSCAN`: no portable function changed).
3. `make audio-render` twice (already measured in Task 3; re-run for the gate).
4. Record the counters: `port_progress`, `gate-check`, the audio bank split,
   the PCM pins.

**Acceptance:** every step exits 0; the counters are recorded in the phase
report; no pin moves without its reason.
