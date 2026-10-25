# Music Truth — design

Status: Proposed, 2026-10-25. Follows the proof-completion and endgame-truth
phases. It closes a functional audio hole the earlier phases left as a product
chore: the port plays the title/attract music but never plays the in-match or
menu music. The proof is a tolerance-free comparison against the original's
already-captured OPL register traces.

## 1. Intent and why

The port's audio path runs: `0x1CF40` AIL init completes, the title/attract
XMIDI bank decodes and sequences into OPL register writes, one located
announcer sample plays, and mixed stereo frames reach SDL audio. The title
bank's FM data is byte-exact against an independent Python decoder (9866 OPL
register writes, zero differences) under a symmetric oracle anchored at the
first key-on.

What is missing is **music selection**. `snd_music_request` returns 0 because
`DS_001028C0 == 0`; only `title_music_start` loads `S16TITLE.GRA`. A match or
menu that asks for its own song gets silence. This is a player-visible
functional gap, not a proof nuance: the title and attract are musical, the
matches are not. The existing gameplay captures already hold the original's
music: each `data/k11-captures/gp-*/` has a `.dro` OPL register trace (up to
38,499 writes / 176 s). The port's replay can be compared to it with no new
capture and no fitted threshold.

The audit also exposed two proof gaps around the same path:

- The title capture comparison is **report-only**; it prints the first
  difference (write 430) and stops. The enforced test is port-vs-Python, not
  port-vs-original.
- `make audio-render` writes a WAV and fails only on `fopen`; a silent WAV
  passes. There is no non-silence or determinism pin, and no rendered-audibility
  claim (the OPL core and mixer are an invented model).

The phase's thesis: **music is game logic, and the original's captured register
streams can prove it without listening to the port.** Rendered audibility stays
a model claim, explicitly out of scope.

## 2. Current state (measured 2026-10-25)

- `python3 tools/port_progress.py` → `799 1203 66` / `759 759 100`.
- Audio init/sequencer path: `0x1CF40` completes; the title/attract bank
  decodes; the title FM stream is byte-exact against `tools/opl_seq.py`
  (9866 writes); the capture comparison is report-only (`test_audio.c:942`
  and `:1178-1191`), with the remaining named differences (intra-tick order,
  carrier TL on MIDI ch1/4, `0xBD`, `0x34`) declared.
- `snd_music_request` returns 0 because `DS_001028C0 == 0`
  (`port/src/game/flow.c:6300-6307`); only `title_music_start`
  (`flow.c:6186`) loads `S16TITLE.GRA`. No in-match/menu song is requested.
- The 37 gameplay `.dro` traces are in the existing `data/k11-captures/gp-*`
  directories (the capture harness records them; the AVI PCM is discarded,
  `tools/gp_capture.py:367`/`:575`). They are the original's OPL register
  streams, with per-song first-key-on anchors.
- `make audio-render` renders the title bank to `/tmp/pr_title_fm.wav`; it
  fails only on `fopen` (`port/tests/test_audio.c:1204-1267`). A silent WAV
  passes.
- `0x1C930` (the host music-request/sequencer path) and `0x1CF40` (the audio
  init) are two of the 8 portable `UNROWED` functions: the port's host
  music-request path replaces them, and a differential row would mask the AIL
  handle boundary (`docs/superpowers/plans/2026-10-24-proof-completion-derivations.md`
  §P8.2).
- The OPL/mixer model is invented (Q8 gain, nearest-neighbour, four voices);
  a PCM comparison to the original would pin the model, not the game.

## 3. Goals

**G1 — The music request/resolve.** Port the game-side music selection that
`snd_music_request` needs: resolve the requested song from the game's tables,
load/select its bank, and hand it to the host sequencer. Keep the AIL runtime
and mixing host-owned. Add a differential row only if it can be faithful
without fabricating AIL handles; otherwise record the boundary and keep the
function's `UNROWED` status with the measured reason. The functional claim:
a menu/match request selects and starts the original's song.

**G2 — The register-stream oracle.** Add a tolerance-free comparison of the
port's OPL register writes against the original's `.dro` traces for the 37
gameplay captures, anchored at each song's first key-on. It claims song
choice, notes, patches and tempo in the captured windows; it does not claim
rendered audibility. Add it to `make verify` (skipping without the traces) and
prove it fails on a one-write mutation.

**G3 — PCM determinism.** Make `make audio-render` fail on a silent render and
pin the WAV's sha256 plus a non-zero peak, with the game's sequence volume.
This claims deterministic non-silence, not that the PCM matches the original.

**G4 — Docs and claims.** Correct the audio status in `README.md`,
`AGENTS.md`, `docs/PROGRESS.md`, `port/spec/audio.md` and `port/RE_GUIDE.md`:
title/attract music is proved; in-match/menu music is the phase's new claim;
rendered audibility stays a named gap. No fitted constant.

**G5 — Gate.** `PR_DIFF_FULLSCAN=1 GATE_STRICT=1 PR_PINS_EXACT=1 make verify`
exits 0; the title/attract claims and every existing pin are unchanged except
the phase's own; `port_progress.py` and the differential counter are unchanged
unless a row is added or a function leaves the classification.

## 4. Non-goals

- **Rendered audibility.** A PCM envelope/spectral comparison against a
  DOSBox-X `/A` capture would pin the port's OPL/mixer model, not the game.
  The register stream is the claim; PCM is a determinism check only.
- New captures. The 37 `.dro` traces are the evidence; a missing song is a
  named gap, not a new capture.
- The `0x1CF40` init, the 60 Hz AIL callback and the mixer internals: they
  stay host-owned. The phase ports the game-side song resolve, not the AIL
  runtime.
- The fought-match U14 R1 and the two-human KO: a separate proof phase
  (`docs/superpowers/specs/2026-10-25-fought-match-truth-design.md`).
- CI/packaging, SDL pad hot-plug, save-path docs and the review minors except
  the small housekeeping commit.

## 5. Approaches considered

**A — Music Truth (recommended).** Port the game-side song request/resolve,
make in-match/menu music play, prove it against the existing `.dro` traces
tolerance-free, and add a PCM determinism check. Functional, deterministic,
no new capture.

**B — Fought-Match Truth.** The U14 R1 fought win (one authorized attempt,
~50/50 CPU fight), a hand-dealt two-human KO, the U16 finisher replay fix and
the 5120 experiment. It closes the last fight-proof gap but changes little
player-visible behavior; it stays the next proof phase.

**C — Oracle/product completion.** K11 `de`/`diags`/`idle` promotion, a longer
attract3 capture, CI/packaging and the review minors. Useful but narrower; it
does not close a functional hole.

Choose A. The port already plays the game; the missing music is a fidelity
bug. B and C follow.

## 6. Constraints

- Work from the repo root. `data/game/` is read-only; the `.dro` traces are
  read-only.
- No new capture and no re-capture. A song absent from the 37 traces is a
  named gap.
- The register-stream comparison is **tolerance-free**: no fitted timing
  window, no "close enough" note count. Anchoring at the first key-on is the
  only alignment; a whole-tick/mixer-block tolerance must be declared, not
  fitted.
- The PCM hash is not an audibility claim. It pins the port's own render
  (sha256 + peak > 0) and is labelled as such.
- The AIL runtime stays host-owned. The game-side song resolve is a C function
  with the `/* 0xADDR — section */` header; a `PORT:` note names the host
  hand-off. A differential row is added only if it does not fabricate AIL
  handles; otherwise the `UNROWED` boundary is re-stated with the new evidence.
- Every new pin is measured from the tool's output and proved to fail one step
  in the tested direction. No fitted constant.
- Existing audio/title/attract pins must not move except the phase's own.
- Docs: `README.md`, `AGENTS.md`, `docs/PROGRESS.md`, `port/spec/audio.md`,
  `port/RE_GUIDE.md`, the Makefile comments, this spec and the derivation
  record. Commit style `<area>: <what>`; stage named files, never `git add
  -A`; commit only when authorized.
- The final gate is `PR_DIFF_FULLSCAN=1 GATE_STRICT=1 PR_PINS_EXACT=1 make
  verify`.

## 7. The measured starting point

### 7.1 The title/attract path is proved

The title bank decodes and sequences; the 9866 OPL writes are byte-exact
against the Python decoder; the symmetric oracle is anchored at the first
key-on. The one located announcer sample plays through the game's sample
request/play path. This is the proof the new phase builds on, not a claim it
re-opens.

### 7.2 The in-match/menu hole

`snd_music_request` returns 0 because `DS_001028C0 == 0`; only
`title_music_start` loads `S16TITLE.GRA`. The game's match/menu songs are never
requested, so the port's matches are silent. The raw `0x1C930` is the
host music-request/sequencer path; the port's host replacement is why the
function is one of the 8 `UNROWED`. The functional fix is the game-side
resolve, not the AIL runtime.

### 7.3 The captured evidence

Each gameplay capture has a `.dro` OPL register trace (up to 38,499 writes /
176 s). The traces are the original's actual register writes for the match,
including its song choice, notes, patches and tempo. They are the oracle's
reference; no new capture is needed. A song absent from the traces stays a
named gap.

### 7.4 The PCM state

`audio-render` writes a WAV and fails only on `fopen`; a silent render passes.
The OPL core/mixer is an invented model, so a PCM envelope comparison would be
a model claim. The phase pins non-silence/determinism only.

### 7.5 The unrowed boundary

`0x1C930` and `0x1CF40` are in `UNROWED` because a differential row would
replace the AIL handle gates/copies/stops/clear with fabricated handle state.
The phase ports the game-side song resolve and keeps the AIL calls host-owned;
if no faithful row exists, the boundary is re-stated with the functional
evidence rather than a synthetic row.

## 8. The approach

### 8.1 Derive the song request/resolve

From the raw `0x1C930` and the game's music tables, derive the requested song
id, the bank load/select, and the host hand-off. Read the title path
(`title_music_start`) as the working example; the in-match/menu path is the
missing arm. Record the addresses and the table entries in the derivation
record.

### 8.2 Make the music play

Implement the game-side resolve so `snd_music_request` returns a real song and
the sequencer starts it. Keep the AIL runtime host-owned. Add a unit test that
a match/menu request selects the expected song and hands it to the sequencer;
the test fails on the current zero return.

### 8.3 The register-stream oracle

Add `tools/audio_compare.py` (or extend the existing OPL trace tooling) that
parses the port's register trace and the capture's `.dro`, anchors each song at
its first key-on, and compares the writes. The first claim is the song choice
and the write stream in the captured window; per-tick write multisets are the
fallback if the intra-tick order is a known model difference. Add the oracle
to `make verify`, skipping without the traces; prove it fails on a one-write
mutation. A capture whose song is missing is skipped by name, not silently
passed.

### 8.4 PCM determinism

Extend `audio-render` to render the selected in-match/menu song, fail on a
silent output, and pin the WAV's sha256 + peak. The label is "deterministic
non-silence", not "matches the original".

### 8.5 Optional small gaps and housekeeping

The service-menu language reload (`§K11.3`) is a small functional gap; the
proof-completion review's two follow-ups (`waitpid` retry/assert,
allowance-before-window-start guard) and the five cheap minors are a single
housekeeping commit. The coin-countdown capture is deferred: it needs a new
capture, which this phase's binding no-new-capture constraint forbids. Take
the optional items only if the audio work leaves budget; otherwise declare
them.

## 9. Acceptance

1. A menu/match request selects the original's song and the port's OPL trace
   matches the capture's `.dro` in the captured window (tolerance-free; the
   declared model differences named), or the first difference is recorded with
   its cause.
2. The register-stream oracle is in `make verify`; it skips without the traces
   and fails on a one-write mutation; the 37 traces are inventoried and any
   missing song is a named gap.
3. `make audio-render` fails on a silent WAV and pins a non-zero peak + sha256;
   the claim is labelled determinism, not audibility.
4. The title/attract claims (9866 writes, the symmetric oracle, the announcer
   sample) are unchanged; every existing audio pin is unchanged except the
   phase's own.
5. `port_progress.py` and the differential counter are unchanged unless a row
   is added or a function leaves the classification; any movement is measured
   and recorded.
6. `make verify` exits 0; docs current; the derivation record has every
   measurement, table, allowance and named gap.
7. The final review is done.

## 10. Named gaps after the phase

- Rendered audibility vs the original: the OPL core/mixer model; a PCM
  envelope/spectral claim is a model claim.
- A song/bank absent from the 37 `.dro` traces.
- Any in-match/menu song path the raw does not reach through `0x1C930`.
- The `0x1CF40` init and AIL handle boundary; host-owned.
- The fought-match U14 R1 and the two-human KO (the next proof phase).
- CI/packaging, SDL hot-plug, save-path docs and any un-taken housekeeping.

## 11. Risks

- **`0x1C930` may be mostly AIL handle plumbing.** The game-side resolve may
  be small; the phase then fixes the request path and records the boundary,
  rather than forcing a synthetic differential row.
- **The in-match songs may need banks not loaded by the title path.** The
  bank load/select is part of the task; a missing bank is a named gap.
- **The register streams may differ in more than the declared model.** The
  oracle is tolerance-free, so a real port bug shows as a first difference;
  fix it with the raw instruction or record it. Do not add a timing tolerance.
- **The PCM render may be silent for a non-title song.** The sequence volume
  and bank selection are part of the task; the hash is a determinism check,
  not a fix.
- **The title/attract claims must not move.** The title path is the working
  reference; a regression there stops the task.

## 12. Decisions I made (your call to override)

- **Phase scope:** music truth, not another proof sweep. The in-match/menu
  silence is a functional hole; the fought-match U14 is the next proof phase.
- **Evidence:** the 37 existing `.dro` traces, no new capture.
- **Claim:** register stream/song choice, not rendered audibility; PCM is a
  determinism hash only.
- **Port boundary:** game-side song resolve in C; AIL runtime host-owned; no
  fabricated differential row.
- **Unrowed:** if `0x1C930` cannot be faithfully rowed, keep it declared with
  the new functional evidence; do not fake a row.
- **Small gaps:** the language reload and the review housekeeping are optional;
  the coin-countdown capture is deferred to a future phase because it needs a
  new capture. None justifies a phase.

## 13. What is left to the builder

The exact `0x1C930`/song-table addresses, the bank load/select, the
`audio_compare.py` format and anchor, the PCM hash/peak, the Makefile wiring,
the tests, the derivation record and the gate. The raw bytes win over this
document and over the agents' reports; the plan's first task re-derives the
addresses and the record is authoritative if anything here is wrong.
