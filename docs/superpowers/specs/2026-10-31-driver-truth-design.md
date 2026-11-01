# Driver Truth — design

**Status:** implemented (Tasks 1–3, 5–6) and gated; Task 4 (`0x21` spec) not
taken. `make verify` exited 0 on 2026-10-31 (`gate: captures 43/44 present`,
audio summary ok, `diff-verify` 1033/1033 / 14993/14993 / 0 named gaps,
`symbols.h` 1304/1206 byte-identical, pin baseline `25202638…`), and
`make audio-render` twice produced the unchanged title `515b7c94…` and selmo
`530a9430…` (peak 2790).
**Predecessor:** Final Truth (`docs/superpowers/specs/2026-10-30-final-truth-design.md`);
the terminal claim register is `docs/superpowers/specs/2026-10-30-final-claims.md`.
**Plan:** `docs/superpowers/plans/2026-10-31-driver-truth.md`.

## 1. Where the project stands

The port is complete on its declared target: **759/759 portable functions**
(798/1203 raw, 66%) and the Final Truth phase closed every remaining gap into
the terminal claim register: **34 rows — 6 closed, 1 gated, 19 terminal, 8
declared**, no row open. The capture inventory is **44 paths, 43/44 present**
(only U14's fought live win declared absent). The differential census is
`1033/1033` functions, `14993/14993` mutants, 0 named gaps, `810/845` full,
8 unrowed, `WEAKER` 12. Coverage is HIT 675 / NOT-HIT 80 / UNINSTRUMENTED 4.

The register's terminal rows are, by definition, measured limits the Final
Truth phase did not attack. One of them (R10, the 10 requested-unclaimed
audio banks) names its terminal reason as "the driver's voice-rotation/tail
model". That is a statement about **our model of the original driver**, not
about the original's behaviour. A model gap is fixable; this phase measures
which of those rows are port defects and fixes them.

## 2. Goal and success criterion

**Goal:** replace the port's invented driver-state reset with the original
driver's measured persistence model, and re-measure the register-stream
oracle against the 29 existing gameplay traces. Close every R10 row the fix
proves; keep the rest terminal with their measured reason. Document the
unexhibited mode `0x21` sub-game from the ported code so R05's "future work"
is a spec plus a route assessment, not a sketch.

**Success criterion:** `make audio-oracle` green with the moved rows measured
whole-section; every pin that moves re-pinned with its evidence in the phase
record; the register updated so no row is open and every status change names
the fix and the pin; `make verify` green (the audio change cannot affect the
pixel oracles, and the gate proves it); `make audio-render` deterministic
across two runs with the new PCM pin recorded.

**Non-goals:** no new capture, no new portable function, no change to the
pixel/frame oracles, no change to the game's own byte-level behaviour. R11
(the 2 reset-only banks) and R12 (the 2 never-requested banks) are not
touched: R11's blocker is the claim model (a reset-only start has no section
content to compare) and R12 needs a capture. The `0x21` sub-game is
documented, not exhibited: its route is a future capture task (R05 stays
declared).

## 3. What the investigation measured

The investigation was read-only except one throwaway patch, reverted (no
capture, no committed change).

### 3.1 The load-bearing fact: the original's start does not reset per-channel driver state

`AIL_start_sequence` (`0x5DE79`) calls `0x6A780`, which calls `0x69250`. The
decompiled `0x69250` (size 165, `port/decomp/prage.c:49385-49420`) writes the
sequence record only: the 16-dword `+0x90` map to identity, four dwords at
`+0x7c`, 32 dwords at `+0x510` (then `+0x510 = 0`), and the counters
`+0x2c`, `+0x58`, `+0x5c`, `+0x68`, `+0x64`, `+0x60`, `+0x6c`, `+0x30`,
`+0x28`. It writes none of the per-MIDI-channel controller state
(`0x18f9` bend, `0x1909` volume, `0x1919` pan, `0x1929`/`0x1939` wheel,
`0x1949` expression, `0x1959` mod, `0x1969` sustain) and no program cache.
`0x61a70` inside it is the "silence all channels" path the spec already
names (`port/spec/audio.md:719`).

The port's `seq_start` (`port/src/platform/audio/sequencer.c:679`) does more:
it zeroes `S.program[]`, `S.bank[]`, `S.volume[]`, `S.expression[]`,
`S.mod[]`, `S.sustain[]`, `S.pan[]`, `S.wheel_*`, `S.bend_scale`, and all 18
voices. The voice reset is the raw's (`0x69250`'s map + the silence); the
per-channel reset is not.

### 3.2 The experiment that proves it

One throwaway patch made the per-channel reset run once (the install state)
instead of every start. Rebuilding and replaying `gp-u12-endurance` moved
section 3 (`s16sound.gra+0x3E64`, bank `3D536DDE`) from **FIRST DIFFERENCE at
tick 180** to a **608-tick whole-section match**; the comparator's only
complaint was that the shipped `TRACE_RESIDUE` table still expects the old
diff. The differing group was the patch: the port emitted program 0's bank
bytes (`0x20=0x01, 0x40=0x8f, 0x60=0xf2`, no `0xC0`), the capture emitted
`FAT.OPL` program `0x0C`'s (`0x20=0x18, 0x40=0x23, 0x60=0xf6, 0xC0=0x30`) —
the program `selmo+0x25C` set on MIDI channel 3 in section 1 and the original
still held at section 3's start.

### 3.3 The row-level diagnoses (measured by re-running the comparator)

The 10 `TRACE_RESIDUE` rows split into four causes, not one:

| cause | rows | evidence |
|---|---|---|
| per-channel state reset at start | `s16sound.gra+0x3E64` (tick 180), `s16sound.gra+0x335E` (tick 120), `s16himal.gra+0x08` (tick 30, the carrier-TL volume latch), `s16citys.gra+0x08` (tick 0, unanchored) | the capture's group is the previous section's program (`0x0C`, `0x36`/`0x2A`) or the previous CC7-latched level; the port's is program 0 / level 0. `335E`'s record example `0x120 1 vs 0x71` is program 0 vs program 42. |
| patchless note keyed | `s16sound.gra+0x6CA8` (tick 5) | the port keys `0x7F0B` (note 11, attr 9) but `FAT.OPL` has no `0x7F0B`, so `patches_lookup` returns NULL and `fam_apply` writes frequency without a patch; the capture keys only the other three drums. |
| release/retrigger ordering | `s16sound.gra+0x72D4` (tick 177) | the port writes key-off `0xB4=0x12` for the tick-241 note; the capture retriggers the same voice (`0x1B5=0x33`) and never writes the off. The port expires `release` before the tick's events (`sequencer.c:755-762`). |
| comparison model (not a port bug) | `s16conti.gra+0x08` (960 of 964), `s16hghsc.gra+0x08` (1437 of 1440), `s16sound.gra+0x39F4` (303 of 304) | the capture's music clock is ~0.8 % longer than the game frame's; the port's section ends 1-4 ticks before the capture's, and the boundary tick is unclaimed by construction. The per-row request frame is not isolable from a `.dro` (ms only). |
| boundary cursor (still measured) | `s16sound.gra+0x5B88` (tick 0, unanchored) | no candidate passes the 4-tick prefix; consistent with the measured section-boundary cursor gap. |

So **6 of the 10 rows are predicted fixable**, 3 are the documented
comparison-model limit, and 1 stays a measured boundary. The prediction is
not a claim: Task 3 measures each row's new status after the fix and keeps
whichever rows still differ, with their measured tick and cause.

## 4. Approaches considered

**A. Fix the driver model (chosen).** Derive the original's persistence from
the bytes, implement it, re-measure all 27 traces, move the rows the oracle
proves, keep the rest terminal. Cost: one port module (`sequencer.c`) and the
audio oracle's pins. Risk: a claimed bank could move the wrong way; the
re-measurement decides, and the capture is ground truth.

**B. Leave R10 terminal and document the cause.** Cheapest, but it keeps a
known port defect: the port's second song starts with different patches than
the original's, which is audible behaviour, not a measurement artifact. The
register's own rule is that a terminal row must be a measured *limit*; a
fixable model gap is not one.

**C. Re-capture the 10 banks.** Not possible to help: the divergence is in
the port's state, not the capture's; a new capture of the same songs would
reproduce the same original behaviour and the same port bug.

**D. Fit the observations.** Rejected by the project's evidence rule: the
program/TL values are read from `FAT.OPL` and the capture's register stream,
and the model is the raw's own start routine.

## 5. The model to implement (to be confirmed by Task 1)

At the install state (once, before the first sequence): the per-channel
program/bank 0, volume 0, expression `0x7f`, pan `0x40`, mod 0, sustain 0,
wheel LSB 0 / MSB `0x40`, bend scale 0, matching today's first-song output
(the title oracle's 9866 writes must not move). At every start: the raw's
`0x69250` reset only — voices silenced and the sequence record cleared — and
the per-channel state left as the previous sequence left it.

The patchless note and the release/retrigger ordering are separate rules
whose exact form Task 1 derives from the driver bytes; the fix is only the
part the bytes prove.

## 6. Tasks

| task | deliverable | acceptance |
|---|---|---|
| 1 | the driver model in record §D1: persistence, install defaults, patchless note, retrigger ordering, each with an address | every statement has an address; the model is a testable rule |
| 2 | the `sequencer.c` fix + unit tests | build green; each new test fails under its named mutation; the title's 9866-write oracle unchanged |
| 3 | the 27-trace re-measurement and the re-pin | `make audio-oracle` green; every moved row whole-section; the 12+10+2+2 partition still covers 26 |
| 4 | `port/spec/volleyball.md` (mode `0x21`) + the route assessment | every rule cites a ported function and its raw address; no claim change |
| 5 | the register and docs reconciliation | `test_final_claims` green; no row open; every status change names the fix and the pin |
| 6 | the gate | `make verify` green; `make audio-render` twice byte-identical; the counters re-measured |

## 7. Risks

- **A claimed bank regresses.** The fix changes what every later song starts
  with. The oracle decides; if a previously claimed bank stops matching, the
  fix is wrong for that bank and the model must be narrowed to what the bytes
  prove (the register's rule: on a plan-vs-raw conflict the raw wins).
- **The same-song path.** `0x1C93B` re-inits the sequence without a resolve;
  it still calls `AIL_start_sequence`, so the same persistence applies. The
  title's "start once" rule is unchanged.
- **The PCM pin moves.** `make audio-render` renders title then selmo; the
  selmo now starts with the title's carried state, as the capture does. The
  pin is determinism, not audibility, and is re-pinned with its measured
  value and reason.
- **The scope drifts into a capture phase.** R05, R11 and R12 stay declared.
  The volleyball spec is documentation; no route search and no capture is
  attempted in this phase.
