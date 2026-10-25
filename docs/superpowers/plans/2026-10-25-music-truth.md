# Music Truth Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development
> (recommended) or superpowers:executing-plans to implement this plan task-by-task.
> Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the port play the game’s in-match and menu music, and prove the
song choice and register stream against the original’s already-captured OPL
traces — tolerance-free, with no new capture and no rendered-audibility claim.

**Architecture:** The game-side song request/resolve is ported from `0x1C930`
and the music tables; the AIL runtime and mixer stay host-owned. The
register-stream oracle compares the port’s OPL writes to the 37 gameplay
`.dro` traces, anchored per song at the first key-on. `audio-render` gains a
non-silence/determinism pin. The title/attract path is the working reference
and must not move.

**Tech Stack:** Python 3 (the `.dro`/OPL trace tools), the C/SDL3 port, GNU
Make, the raw `PRAGE.EXE`, Ghidra/capstone, the existing gameplay captures.

**Spec:** `docs/superpowers/specs/2026-10-25-music-truth-design.md`

## Global Constraints

- Work from the repo root. `data/game/` is read-only; the `.dro` traces in
  `data/k11-captures/gp-*/` are read-only.
- **No new capture and no re-capture.** The 37 existing `.dro` traces are the
  evidence; a song absent from them is a named gap.
- The register-stream comparison is **tolerance-free**. The only alignment is
  the first key-on; a whole-tick or mixer-block tolerance must be declared in
  advance, not fitted to make a test pass.
- The PCM hash is **not** an audibility claim. It pins the port’s own render
  (sha256 + peak > 0) and is labelled determinism.
- The AIL runtime, the 60 Hz callback and the mixer stay host-owned. The
  game-side song resolve is a C function with the `/* 0xADDR — section */`
  header; a `PORT:` note names the host hand-off. Add a differential row only
  if it is faithful without fabricating AIL handles; otherwise keep the
  `UNROWED` boundary and record the new evidence.
- Every new pin is measured from the tool’s output and proved to fail one step
  in the tested direction. No fitted constant.
- Existing title/attract/audio pins must not move except the phase’s own. A
  title regression stops the task.
- Docs: `README.md`, `AGENTS.md`, `docs/PROGRESS.md`, `port/spec/audio.md`,
  `port/RE_GUIDE.md`, the Makefile comments, the spec and the derivation
  record. Commit style `<area>: <what>`; stage named files, never `git add -A`;
  commit only when authorized.
- The final gate is `PR_DIFF_FULLSCAN=1 GATE_STRICT=1 PR_PINS_EXACT=1 make
  verify`.

## Review Focus

1. **A zero return claimed as music.** The unit test must assert the resolved
   song id, not merely that `snd_music_request` returns non-zero; a test that
   accepts any non-zero id can pass on the wrong song.
2. **A fitted tolerance.** The oracle’s anchor is the first key-on; a case
   that skips a write, reorders a tick or drops a note must fail. A test adds
   a one-write mutation and expects the first difference, not “close enough.”
3. **A synthetic differential row.** `0x1C930`’s AIL handles have no port
   counterparts; a row that pokes fabricated handles and calls it verified is
   a failure. The boundary stays declared unless a faithful row exists.
4. **A silent WAV passing.** `audio-render` must fail on zero samples; the
   peak/sha pin is proved by rendering silence.
5. **A title regression.** The 9866-write title oracle, the symmetric
   comparison and the announcer sample are unchanged; the phase’s new path is
   additive.
6. **A missing song silently skipped.** The oracle skips a capture with no
   `.dro` by name; a capture whose song is absent from the port is a first
   difference, not a skip.

---

### Task 1: Baseline and the music-path derivation

**Files:**
- Create: `docs/superpowers/plans/2026-10-25-music-truth-derivations.md`
- Read: `port/src/game/flow.c` (`snd_music_request`, `title_music_start`,
  `DS_001028C0`), `port/src/platform/audio/` (sequencer, OPL core, mixer),
  `port/tests/test_audio.c`, `tools/opl_trace.py`, `Makefile`
  (`audio-render`), `data/k11-captures/gp-*/*.dro`.
- Re-derive: the `0x1C930` addresses, the song/bank tables, the title path as
  the working reference, and the 37 `.dro` inventory.

**Interfaces:**
- Produces: §M1 (the measured baseline: the current audio path, the zero
  return, the title 9866 writes, the 37 `.dro` counts/anchors, the `0x1C930`
  and `0x1CF40` unrowed reasons, the PCM state). Later tasks cite §M1.

- [ ] **Step 1: Record the baseline.** Run and paste:
  ```bash
  python3 tools/port_progress.py
  make diff-verify
  make audio-render
  ```
  Record the current audio tests’ lines (the 9866-write exact check, the
  report-only capture comparison), the `snd_music_request` return path and
  `DS_001028C0`, and the `title_music_start` bank load.
- [ ] **Step 2: Inventory the `.dro` traces.** List every gameplay capture’s
  `.dro`, its write count, its first key-on timestamp and its duration; group
  them by song where the traces repeat. Record the 37 (or the measured count)
  and the songs covered.
- [ ] **Step 3: Re-derive `0x1C930` and the tables.** Use Ghidra/capstone and
  the decomp index to record the raw function, the song-id source, the bank
  load/select and the host hand-off. Record the `0x1CF40` init boundary and
  why a faithful row is or is not possible.
- [ ] **Step 4: Record the PCM state.** Confirm `audio-render` fails only on
  `fopen`, and that a silent WAV passes; record the output rate, format and
  the sequence-volume omission.
- [ ] **Step 5: Write §M1 and commit (when authorized).** `docs: baseline the
  music truth phase`.

---

### Task 2: Port the game-side song request/resolve

**Files:**
- Modify: `port/src/game/flow.c` (the `snd_music_request` path), the music
  table access; `port/src/game/flow.h` if a new helper is exported.
- Modify: `port/src/platform/audio/` only for the host hand-off, not the AIL
  runtime.
- Modify: `port/tests/test_audio.c` / `test_game.c` (the area owner).
- Modify: `tools/diff_verify.py`/`tools/tests/test_diff_verify.py` only if a
  faithful row is added; otherwise the record §M2.
- Modify: `docs/superpowers/plans/2026-10-25-music-truth-derivations.md` (§M2).

**Interfaces:**
- Consumes: §M1’s `0x1C930` addresses/tables and the title path.
- Produces: a game-side resolve that returns the requested song and hands it
  to the host sequencer; a unit test that asserts the exact song; the
  `UNROWED` boundary re-stated or a faithful row.

- [ ] **Step 1: Write the failing test.** Add a unit test that calls the music
  request for a known match/menu song (from §M1’s table) and asserts the exact
  song id and the host-sequencer hand-off. Expected: FAIL (the current return
  is 0).
- [ ] **Step 2: Implement the resolve.** Port the raw `0x1C930` game-side
  logic: the song id, the bank load/select, the host hand-off. Keep the AIL
  handles/mixing host-owned; add a `PORT:` note naming the seam. Do not
  change `title_music_start`.
- [ ] **Step 3: Test and row.** Run the focused tests. If a faithful
  differential row is possible, add it and measure the counter; if not, keep
  `0x1C930` in `UNROWED` and record the measured boundary in §M2. Do not
  fabricate AIL handles.
- [ ] **Step 4: Confirm the title path.** The 9866-write title test, the
  symmetric comparison and the announcer sample are unchanged.
- [ ] **Step 5: Commit (when authorized).** `audio: resolve the match music
  request`.

---

### Task 3: Make the in-match/menu music play

**Files:**
- Modify: the game-side music request call sites and the host sequencer
  hand-off; the audio tests.
- Read: the 37 `.dro` traces; `tools/opl_trace.py`.
- Modify: `docs/superpowers/plans/2026-10-25-music-truth-derivations.md` (§M3).

**Interfaces:**
- Consumes: Task 2’s resolve; the `.dro` reference.
- Produces: a played song whose OPL trace matches the capture’s `.dro` in the
  captured window, or the first difference with its cause.

- [ ] **Step 1: Drive a known match/menu state.** Use a headless/replay path
  that requests the song; capture the port’s OPL register trace (a new
  `PR_AUDIO_DUMP` driver or the existing sequencer log, whichever already
  exists; do not add a capture).
- [ ] **Step 2: Align and compare.** Anchor both streams at the first key-on;
  compare the writes tolerance-free. Record the first difference.
- [ ] **Step 3: Fix or declare.** Fix a real port bug with the raw instruction;
  a known model difference (intra-tick order, carrier TL, `0xBD`, `0x34`) is
  declared, not fitted. If the song/bank is absent from the traces, name the
  gap.
- [ ] **Step 4: Re-run.** The title/attract pins are unchanged; the new song’s
  first difference moves or is declared.
- [ ] **Step 5: Commit (when authorized).** `audio: play the match music`.

---

### Task 4: The register-stream oracle

**Files:**
- Create: `tools/audio_compare.py` (or extend `tools/opl_trace.py` with a
  comparison mode).
- Create/modify: `tools/tests/test_audio_compare.py`.
- Modify: `Makefile` (`audio-oracle`, `verify`), `docs/PROGRESS.md`.
- Read: the 37 `.dro` traces, the port’s trace driver.

**Interfaces:**
- Consumes: the `.dro` format, the port’s trace, the first-key-on anchor.
- Produces: a tolerance-free comparator, a Makefile target, a mutation test,
  and a per-song inventory of covered/missing traces.

- [ ] **Step 1: Write the failing test.** A synthetic `.dro`/port pair with one
  dropped write must fail with the first difference; an identical pair passes;
  a missing `.dro` skips by name. Expected: FAIL (the tool does not exist).
- [ ] **Step 2: Implement the parser/comparator.** Reuse `tools/opl_trace.py`’s
  `.dro` parser; anchor at the first key-on; compare the write stream
  tolerance-free (the per-tick multiset fallback only if §M3 declares the
  intra-tick order a model difference). Emit the first difference and the
  covered/missing songs.
- [ ] **Step 3: Wire the Makefile.** Add `audio-oracle` with the capture
  inventory; skip absent traces; add it to `verify`. Prove it fails on a
  one-write mutation and on an unanchored stream.
- [ ] **Step 4: Measure the 37.** Run the oracle; record the songs that match,
  the first differences and the declared model differences. A song that
  cannot match is a named gap, not a threshold.
- [ ] **Step 5: Commit (when authorized).** `audio: compare the captured
  register streams`.

---

### Task 5: PCM determinism

**Files:**
- Modify: `port/tests/test_audio.c` (`audio-render`), `Makefile`
  (`AUDIO_WAV`/`audio-render`), the audio tests.
- Modify: `docs/superpowers/plans/2026-10-25-music-truth-derivations.md` (§M5).

**Interfaces:**
- Consumes: the selected song from Task 2/3, the render path.
- Produces: a non-silent render check, a pinned sha256 + peak, and the
  determinism label.

- [ ] **Step 1: Write the failing test.** Render silence (or an empty song) and
  expect the check to fail; render the selected song and expect a non-zero
  peak. Expected: FAIL (the current check only tests `fopen`).
- [ ] **Step 2: Implement the pin.** Add the peak/sha256 check to the render
  test/Makefile; render the selected in-match/menu song at the game’s sequence
  volume. Do not claim audibility.
- [ ] **Step 3: Run and record.** The WAV’s sha256/peak are measured; the title
  render is unchanged; the test fails on silence.
- [ ] **Step 4: Commit (when authorized).** `audio: pin the rendered
  non-silence`.

---

### Task 6: Optional small gaps and housekeeping

Take only if Tasks 1–5 leave budget; otherwise declare them.

- [ ] **Language reload (`§K11.3`):** pick `<LANG>.TXT` from field `0x29` bits
  24–26, drop the idempotence, add the host reseat hook, unit-test English and
  one shipped non-English file.
- [ ] **Coin countdown (deferred, not this phase):** it needs a new capture,
  which the no-new-capture constraint forbids; the gap is recorded for a future
  phase.
- [ ] **Housekeeping:** `waitpid` retry/assert; allowance-before-window-start
  guard + duplicate-key rejection; `PC_DEAD` alias; one `closed_rows`
  assertion; 32 `ResourceWarning` contexts; `SpanCensusTests` docstring; the
  proof-completion design §7 sentence. No pin/assertion meaning changes.

---

### Task 7: Docs, claims and the full gate

**Files:**
- Modify: `README.md`, `AGENTS.md`, `docs/PROGRESS.md`, `port/spec/audio.md`,
  `port/RE_GUIDE.md`, the Makefile comments, the spec status, the record §M6.
- Run: the full gate.

- [ ] **Step 1: Correct the claims.** Title/attract music is proved; in-match/
  menu music is the new claim (the covered songs); rendered audibility stays a
  named gap; the `0x1C930` boundary/row is recorded.
- [ ] **Step 2: Keep the counters current.** `port_progress.py`, the
  differential counter and the README title percentage are the measured
  values; no prediction.
- [ ] **Step 3: Full gate.** `PR_DIFF_FULLSCAN=1 GATE_STRICT=1 PR_PINS_EXACT=1
  make verify`; every audio oracle runs; the title/attract pins unchanged.
- [ ] **Step 4: Record and commit (when authorized).** `docs: close the music
  truth phase`.

## Phase acceptance summary

- A menu/match request selects the exact original song; the port’s OPL trace
  matches the capture’s `.dro` tolerance-free, or the first difference is
  recorded with its cause.
- The register-stream oracle is in `verify`, skips absent traces, and fails on
  a one-write mutation; missing songs are named.
- `audio-render` fails on silence and pins a non-zero peak + sha256; labelled
  determinism.
- The title/attract claims (9866 writes, symmetric oracle, announcer sample)
  are unchanged.
- Existing pins unchanged except the phase’s own; `port_progress`/diff counter
  measured; docs current; full gate exit 0.
