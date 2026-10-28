# Claim Coverage — design

Status: **Approved and executing**, 2026-10-28 (the operator invoked
subagent-driven development on the plan). U14 is **not** authorized by that
invocation; Task 12 is skipped and the U14 R1 named gap stands. Follows
proof-completion, endgame-truth, music-truth and fought-match truth. The port is complete against its declared population; what
remains is claims that are narrower than they read, and a verification gate that
cannot be relied on to run twice green.

## 1. Intent and why

`python3 tools/port_progress.py` reads `799 1203 66` raw and `759 759 100`
portable. `make diff-verify` reads `1033/1033 functions VERIFIED; 14993/14993
mutants detected; 0 named gaps; 825/845 rows with callees closed (188 have
none); 807/845 closed on full rows; 8 ported functions with no row`. The
capture oracles cover boot, title, attract, front end, demo fight, service menu,
character select, one- and two-player matches, seven endings and the U16
finisher reaction. `make gate-check` reads `34/35 present, 1 missing`, the
missing capture declared (U14).

Recon for this design measured four things the existing records state less
strongly than the evidence supports, and one that is simply missing:

1. **The gate is not deterministic.** The fought-match phase's full gate exited
   2 in the register-stream audio oracle under host load: `gp-u18-talon-ending`
   compared 718 ticks against a pinned minimum of 964, then 968 on a standalone
   retry and 716 on another, with the dumps' register/value data identical and
   only their **labels** differing (180 `W` frame labels one lower, two `W` tick
   labels 308→310, one `S` `ticks` 2454→2456). The cause is structural:
   `game_audio_service` drives `seq_tick` from `host_tick_count()`
   (`port/src/game/flow.c:6311`, wall time), while its own mixer clock uses the
   modelled ISR tick `DS_00101500` (`flow.c:6321-6345`). The audio oracle
   anchors sections on those labels, so a descheduled process changes the
   comparison without changing the music. This is the only step observed to
   have killed a full gate; it must be fixed before any more captures.
2. **The completion figure is defensible as address coverage, not as written.**
   The 351 unported functions above `0x5D000` are **not** all WATCOM libc/DOS
   glue. The baseline reproduces **37 functions / 19,261 bytes** — the
   boot/movie/attract spine, which includes the movie player `0x1C740` plus six
   other entry points, not "movie-only" and not all middleware. The recon's
   **56 AIL-only / 10,824 bytes does not reproduce** under four mechanical
   closure rules (the nearest readings are 55/21,385 and 59/21,585); a richer
   call graph (indirect/table calls) is the likely cause. Neither set is in
   `tools/port_classification.txt` (which declares 53 effective entries, 41
   host + 12 deferred); the earlier recon reading of 43/76 does not reproduce.
   **The exact sets and closure rule are Task 4's output; do not ship a split
   without the rule, and name the AIL set unreproducible if the graph cannot
   settle it.** `0x62003` is credited as
   ported by a comment that says it is out of scope
   (`port/src/game/fighter.c:1175`); removing it makes the raw count 798, not
   799 (the portable figure is unaffected — it is a runtime address). Ten
   portable credits are inline comments rather than function headers, besides
   the three `UNROWED_HEADER_SHAPE` exceptions (`0x1B120`, `0x20C10`,
   `0x4FBA2`); `0x2EA64` is a bare `ret` and `0x500BB` is inlined
   (`actors.c:2369`, `flow.c:251-257`).
3. **The E2 writer triage is gated on a filler bug.** `tools/entry_triage.py:40`
   `FILLERS` lacks `87 db` (`xchg ebx,ebx`), the 2-byte filler that follows
   `ret` before 157 of the 158 untriaged span writers. The E2 table reads
   227 triaged / 158 untriaged; the census covers 158/158 by shape, but no E2
   row exists for them. Measured: adding the filler admits exactly **157**
   candidates, all `span-writer` — universe 579 → 736, U0 575 → 732. There is
   no non-writer set. The 158th (`0x5D004`) is above `RUNTIME_BASE`, so it is
   not an E2 candidate and needs a declared exception or a separate row.
4. **Three differential rows are one bounded change from full.** `0x51ED8`'s
   four drawing cases declare the renderers' node scratch (`+0x14`,
   `+0x38`) as a byte difference; `0x1BE30`'s row uses one code poke; `0x255CC`
   has two named unhit blocks. Each is a `WEAKER` entry. `0x51ED8` and
   `0x1BE30` can be closed offline; `0x255CC` can close `0x25680` and may keep
   `0x256D4` named (the measured 809 rather than 810), `WEAKER` 15 → 12 or 13.
5. **There are undeclared coverage blind spots.** The most important is the
   KO-branch drawn match: `flow_round_end_check` (`0x27FA8`) reaches
   `flow_round_over` (`0x27ED8`) on `B1E==ADC && AD4==2`; mode `0x0A`'s frame
   handler is `game_mode_0a_step` (`0x28BD4`), and mode `0x0B`'s winner-pose
   tick is `flow_winner_pose_step` (`0x28C38`). All three have **zero capture
   frames**; they are exercised only by unit and differential rows. The
   timeout/health drawn *round* (`AF3==AF2`, `|A-B|<=3` at `0x280BA`) is a
   second, distinct path and is not reached by a 1-1 KO scenario; it stays a
   named gap. Two more blind spots are the side-1 tour/ending (`ad4=1`; every
   ending capture is `ad4=0`) and ENDURANCE's team advance/end `0x32`/`0x33`
   (U12 stops at `0x30`/`0x31`).

The thesis: **the port's remaining gap is claim coverage.** A claim is only as
good as the gate that runs it; a percentage is only as good as its class map;
and a ported handler with no capture is a claim the port has not made. This
phase makes the gate deterministic, makes every headline count honest, closes
the bounded proof debt, and takes the three captures that turn the last
undeclared behaviours into measured claims. U14's fought win remains a named
gap unless its next attempt is explicitly authorized.

## 2. Current state (measured 2026-10-27)

- **Progress:** `799 1203 66` / `759 759 100`. The portable denominator excludes
  53 classified functions (41 host-owned + 12 deferred) and runtime `>= 0x5D000`.
  The classification file has 55 entries (65 physical lines): `0x10604` and
  `0x1BDF4` are not in `symbols.h`, so they affect nothing.
- **Differential:** `1033/1033`, `14993/14993`, 0 named gaps, 825/845 rows with
  callees closed, 807/845 full, 8 `UNROWED`. 20 rows are `TERMINAL_CENSUS`
  terminals (15 AIL thunks, `0x1D0BC`, `input_sampler`, and the three
  `0x6C4CF`-blocked runtime-exit rows). 15 entries are `WEAKER` (24 rows;
  `0x24C5C` has ten). The three counter-moving weaknesses are `0x51ED8`,
  `0x1BE30` and `0x255CC`.
- **Span writers:** 417 slots, 392 distinct targets, 385 writer slots / 385
  distinct, 384 reachable + dead `0x57E3C`, plus the out-of-table
  `0x81110[128] == 0x81310[0] == 0x5D110` for **386 distinct targets, 385
  reachable**. E2 triages 227, leaves 158; all 158 are shape-covered, and every
  untriaged index occurs as a control byte somewhere in the shipped art (an
  upper bound: literal lengths 1..127 and fill lengths 1..63 exist; direction
  and pset use are not traced; no untriaged index is 0).
- **Audio:** 26 code-reachable banks. Five are capture-matched (title, selmo
  `+0x25C`, selmo `+0x08`, beach `+0x08`, jungl `+0x08`); five are requested-only
  (citys, himal, caves, sound `+0x5B88`, sound `+0x3E64`); sixteen are not
  requested or attributed in the measured cut windows. `AUDIO_END=1400`.
  `documented_excluded` (`tools/audio_compare.py:219-230`) decides by the
  register's low byte and `attr in (1, 4)`; the second OPL bank's channels
  (`0x143..0x145`, `0x14B..0x14D`, `0x153..0x155`) carry attrs outside `(1, 4)`,
  so their carrier-TL writes are not excluded. The baseline measured four
  `0x145` writes in `gp-u23-u16-finisher`'s `s16selmo+0x25C`: rel 25 `0x20`
  attr `0x01` and rel 98 `0x98` attr `0x04` (both already excluded), and rel
  326/387 `0x16` attr `0x09`/`0x03` (not excluded). The earlier `0x145=0x20`
  citation was the already-excluded row; Task 7 derives the rule from the
  unexcluded attrs.
- **Captures:** 34/35 present. `gp-u14-win-live` absent and declared. The
  two-human KO (`gp-twop-ko`) and U16 state (`gp-u24-u16-state`) are diagnostic
  scenarios, not in `CAPTURE_DIRS`; both failed the port-script pin under host
  starvation. The two-human KO reached the game's hand-dealt KO twice; the U16
  recovery reached the KO but missed the `s0_54=2` set record (the first U16 run
  never KO'd P2). U14 has not won in three attempts: the first was killed by the
  tool timeout, the two completed runs lost 0-2; `b0c` never set and mode `0x12`
  never opened.
- **Ghidra/index:** the Ghidra MCP instance list exposes only a `kindle`
  project; no `primal-rage` instance is available. `port/decomp/prage.functions.csv` holds 1,206
  real code-object functions covering 260,172 / 408,341 bytes (63.7%); 468
  code-object addresses implemented in `port/src` are absent from it. The
  unindexed code is reachable and already ported by raw address (span writers,
  reaction callbacks, anim targets). The index is not a frontier; the E2 and
  span censuses are the completeness instruments.

## 3. Goals

**G1 — A deterministic gate.** Drive the sequencer's tick count from the same
modelled ISR clock as the mixer whenever there is no audio device
(`host_audio_rate() == 0`), keep the device path wall-clock, re-measure
`TRACE_PINS` (the `audio-render` peak/sha is expected to stay put and is
confirmed, not re-fitted), and prove two consecutive full gates green under
load. Add `make audio-render` to `make verify` (it holds the PCM determinism pin
and is currently a separate command).

**G2 — A trustworthy capture gate.** Give `gate-check` a per-capture manifest:
the 30 gameplay/menu captures pinned by `poll.log` sha256, frame count and
`.dro` inventory where present; the five frame-dump captures (`smk/twi5`, `smk/twg`, `title`,
`title2`, `frontend`) by frame count and content hash. A split preflight: the
verify preflight checks `data/game/C`, `ghidra_data.bin` and the 2040-zero CMOS
(capstone/unicorn stay skip-if-absent, as documented); the capture preflight
(`gp-capture` only) checks `dosbox-x`, `ffmpeg`, the writable root, `memsize`
and the load average. A truncated or substituted capture fails before an oracle
sees it; a declared absence (the captures are git-ignored) skips. Add a
pin-baseline tripwire (a sha256 over the marked pin regions) so a silent
`MAX_MISSING`/`MIN_FIRST` change fails and a deliberate one has to be recorded.

**G3 — An honest completion figure.** Give every `FN_` address exactly one
declared class: ported (header or `fn_register`), host-owned, deferred,
replaced-middleware, or runtime-libc. Derive the movie-path and AIL closures
with a recorded caller-closure rule and declare them `replaced` (37/19,261
reproduces as the boot/movie/attract spine; the 56/10,824 AIL reading does not
reproduce; the exact sets are the task's output, and an unreproducible AIL set
is named rather than fitted), remove the `0x62003` false positive, and either give the
inline-credited functions real headers or list them as declared exceptions
(besides the three `UNROWED_HEADER_SHAPE` addresses). Update
`port_progress.py` to print the class breakdown, the README title/line,
`AGENTS.md` and the records to say "759/759 addresses implemented or declared",
not "351 runtime = libc".

**G4 — Close the bounded differential debt.** Close `0x51ED8` (the renderers'
node scratch write-backs in `sprite_blit_aperture`) and `0x1BE30` (the three
guards, the `0x4FB98` call and `0x1ADE4` as a host-owned `allow_calls` callee,
with a `Machine` `int 0x10`/`int 0x31` answer if needed) to full rows, and
close what `0x255CC` can close: 810/845 full, `WEAKER` 15 → 12, `0` named gaps
unchanged. `0x255CC`'s `0x25680` is reachable with `Machine(aperture=True)` and
a seeded back buffer; `0x256D4` waits on the absent ISR tick and stays named
unless an ISR/tick model is added, so the row may stay weaker and the phase
reports 809 with the exact reason. It does not delete a case to claim a
closure.

**G5 — Triage every span writer, or name the residual.** Add `87 db` to E2's
filler set, measure the admission (the measured result is **157, all
`span-writer`**; universe 579 → 736, U0 575 → 732), regenerate the table with
the measured `--expect` counts, and name `0x5D004` as the one writer above
`RUNTIME_BASE` (a declared exception or a separate row). If the table cannot be
regenerated cleanly, keep it unchanged and record the residual with its
evidence — never fit the universe.

**G6 — Measure the audio coverage that already exists.** Fix the second-bank
carrier-TL exclusion, add a constant-channel-rotation anchor for the
`0x31`/match sections whose `k` is **derived from the sequencer's voice-cursor
state**, not chosen by a search, with a null test that includes a rotated copy
of the correct trace and a shuffled capture, plus mutation tests. Raise the
`AUDIO_END` cap above every capture's `X` so `capture-end` returns each own
`X`. Claim the sections that pass; name the rest.

**G7 — Measure the last undeclared behaviours.** Take the two-human KO with the
hardened harness and give it an oracle whose window covers the KO; measure the
U16 `0x4844C` gate input (measurement only, no port-code fix); and capture the
KO-branch drawn match (`B1E==ADC && AD4==2` → `0x0A` → the `0x0B` pose) with a
poked 1-1 scenario cloned from `gp-u9-win`. The timeout/health drawn round is
not this claim. U14's fought win is **conditional** on an explicit go-ahead in
this design's approval; without it, the task is skipped and the gap stands.

## 4. Non-goals

- **U14 R1 without a go-ahead.** Three attempts produced no win (one was killed
  by the tool timeout; the two completed runs lost 0-2); the next run needs an
  explicit authorization, and the plan records that the two-human substitute is
  a different claim (`b1f=3`, human P2) that a reviewer may reject.
- **The 5120 band's cause.** The 13-pixel, 8/1/4 anomaly at U13 capture 5120
  stays a named gap. The measured facts are in §E9 and this design's recon; the
  decisive experiment (a diagnostic capture reading the 13 offsets through
  `DS_000E87A4` at port f=1825/1826, or a port instrumentation of `0x501A3`) is
  recorded for a later phase, not taken here.
- **Rendered audibility.** The OPL core is Opal 2.0.3 (a YMF262 reference
  emulator); the sequencer voice/patch reconstruction and the mixer are invented.
  The PCM pin stays determinism, not audibility. A reference comparison through
  a second OPL emulator is a later, high-cost experiment.
- **Language reload, coin countdown, storage slot 13..35, the 39
  `config_field_get` consumers, U17's tick-keyed replay, the remaining
  front-end/ENDURANCE/high-score/finisher/service-menu blind spots** (B2, B3,
  B4, B6, B7, B8). Each is recorded with its evidence and left for a later
  phase; none is a regression.
- **The 20 terminal rows, the 8 `UNROWED` functions, the 15 `WEAKER` rows not
  named in G4.** They stay declared with their per-row reasons.
- **Porting new game code.** The phase fixes a host model (the audio clock), a
  counting rule, and the missing state writes/guards in **already ported**
  functions (`0x51ED8`'s node scratch, `0x1BE30`'s teardown guards). It does not
  port a new function. If a task's measurement proves a new game-code defect
  outside those bounded corrections, the task stops and records it rather than
  fitting a patch.

## 5. Constraints

- Work from the repo root. `data/` is read-only; never write to `data/game/`.
  Captures write only their own `data/k11-captures/<scenario>` directories.
- **No fitted constants.** Every pin is measured from a tool's output and
  proved to fail one step in the tested direction. A value that cannot be
  pinned is a named gap with its evidence.
- **The existing title/attract/front-end/endgame pins must not move** except
  the phase's own re-measured audio pins. A title or attract regression stops
  the task.
- **The byte-exact oracles remain authoritative.** `PR_ORACLE_REQUIRED=1`
  stays on for `run_tests`; the gate's order (`--check` before the unit suite)
  stays.
- **A failed capture is preserved, not overwritten.** `.failed.N`; no pins, no
  `CAPTURE_DIRS` entry, no oracle.
- **Doc and commit style:** `<area>: <what>`; stage named files, never
  `git add -A`; commit only when authorized.
- **The final gate is both** `PR_DIFF_FULLSCAN=1 GATE_STRICT=1 PR_PINS_EXACT=1
  make verify` **and** `make audio-render`, each twice (G1).

## 6. Approaches considered

**A — Capture-first.** Take U14, the two-human KO, U16 and the drawn match
before touching the gate. Highest immediate claim value; but the last phase
already showed the failure mode: a capture that reaches the game event still
fails publication on a poller gap, and the full gate cannot be relied on to
judge it. The offline debt (classification, E2, differential) stays. Rejected.

**B — Offline proof debt only.** Fix the gate, the class map, E2, the three
differential rows and the audio coverage; no new captures. Fully deterministic,
low risk; but the undeclared blind spots stay undeclared, and the two-human KO
stays game-evidence-only. Rejected as too narrow for the current state.

**C — Offline truth first, then captures (recommended).** Stage 1 (tasks 1–8)
makes the gate deterministic and the counts honest, entirely offline. A
mid-phase gate must pass twice before Stage 2 starts. Stage 2 (tasks 9–13) uses
the hardened harness for the two-human KO, the U16 state and the drawn match;
U14 is conditional. If a capture fails, its `.failed` is preserved and Stage 1's
claims still stand. This is the design below.

## 7. The design

### 7.1 Stage 1 — gate and proof debt (offline)

1. **Baseline and derivation record.** Run the current gate components, record
   the exact counters, the audio-flake reproduction, the classification audit,
   the E2 filler measurement, the audio-rotation measurement and the capture
   artifacts. Write `docs/superpowers/plans/2026-10-27-claim-coverage-derivations.md`
   §C1. Later tasks cite it.
2. **Deterministic sequencer clock.** In `game_audio_service`, when
   `host_audio_rate() == 0`, take the elapsed tick from `DSD(DS_00101500)` (the
   modelled ISR tick) rather than `host_tick_count()`; keep the device path on
   wall time. Use two deltas and a new `s_last_seq_tick` (the mixer's
   `s_last_isr_tick` is separate and already consumed by the mixer clock).
   Re-run the 29-trace oracle twice under load and re-pin `TRACE_PINS` only,
   each proved at ±1. **The `audio-render` peak/sha must not move** (the WAV
   blocks pace the sequencer from rendered audio, not `game_audio_service`); if
   it moves, stop. Add `make audio-render` to `verify`. Claim: the
   `PR_AUDIO_DUMP` stream and its `W`/`S` labels are byte-identical across runs.
3. **Capture gate hardening.** A `tools/capture_manifest.json` (or the Makefile
   equivalent) pins each capture's identity. The 30 gameplay captures have
   `poll.log` sha256, frame count and `.dro` inventory; the five frame-dump
   captures (`data/smk-captures/{twi5,twg}`, `data/title-captures/{title,title2,
   frontend}`) have a frame count and a content hash, not a `poll.log`. A
   truncated/substituted capture fails before an oracle; a declared absence
   (the captures are git-ignored) skips. `gate-check` validates the manifest and
   the verify preconditions (`data/game/C`, `ghidra_data.bin`, 2040-zero CMOS;
   capstone/unicorn stay skip-if-absent, as documented). The **capture**
   preflight (`gp-capture` only) checks `dosbox-x`, `ffmpeg`, the writable
   capture root, `memsize` and the load average, not the diff tooling. Add a
   pin-baseline tripwire (a sha256 over the marked pin regions, including
   `K11_*`, `DEMO_FIGHT_MIN_FIRST`, `TITLE_*`, `SMK`, `TRACE_PINS`,
   `SCAN3_ALLOWED`/`OVERLAY_ALLOWED`/`BANDS_ALLOWED`) and a derived capture
   count, replacing the literal `35`.
4. **Classification truth.** Derive the movie-path and AIL closures with a
   recorded caller-closure rule; add them as `replaced` with one evidence line
   per address (37/19,261 reproduces; the 56/10,824 AIL reading does not, so
   measure it or name it unreproducible; the 43/76 reading does not reproduce
   either). Remove the `0x62003` false positive; give `0x2EA64`
   and `0x500BB` real headers (or a declared exception); enumerate the ten
   inline credits besides the three header-shape exceptions. Change
   `port_progress.py` to require a header or a declared exception, to print the
   class breakdown, and to stop printing "excludes 53" after the `replaced`
   class lands. Update README/AGENTS/PROGRESS. Claim: every `FN_` has exactly
   one declared class and the completed figure's narrative is true.
5. **E2 writer triage.** Add `87 db` to `FILLERS`, measure the admission (157,
   all `span-writer`; universe 579 → 736, U0 575 → 732), regenerate the E2
   table and re-pin its `--expect` counts. The 158th writer (`0x5D004`) is above
   `RUNTIME_BASE` and needs a declared exception or a separate row. If the
   table cannot be regenerated cleanly, revert the filler and record the
   residual with the measured evidence. Claim: 384 of 385 writer slots have an
   E2 row, and the 385th is named with its rule.
6. **Differential micro-closures.** `0x51ED8`: in `sprite_blit_aperture` (not
   the shared `sprite_blit_at`), after a **live** renderer entry only (types
   1/2/9, not the `ret` types `0x0A`/5), write the renderers' scratch back:
   `+0x14 = 0` (the `dec` loop runs to zero) and `+0x38 = 0x140 - width` or
   `0x140 + width` (mirrored type 9); the current port stores `rows - clip_b`
   and leaves `+0x38` at the seed. `0x1BE30`: implement the three raw guards and
   the `0x4FB98` call, and put `0x1ADE4` in the row's `allow_calls` as a
   host-owned callee (a `PR_SEAM` is wrong: a stubbed seam returns from
   `game_shutdown` and a recorded stub is not excused). A `Machine` `int 0x10`
   and `int 0x31` answer may be needed. `0x255CC`: `0x25680` is reachable with
   `Machine(aperture=True)` and a seeded back buffer; `0x256D4` waits on the
   absent ISR tick and loops forever unless an ISR/tick model is added, so it
   stays named and the row stays weaker if it is not modelled. Prove the rows
   that close; report 810 or the measured 809 with the `0x255CC` reason,
   `WEAKER` 15 → 12 or 13.
7. **Audio coverage.** Fix `documented_excluded` to cover the second bank's
   carrier-TL writes (the measured unexcluded rows are `0x145=0x16` attr `0x09`
   at rel 326 and attr `0x03` at rel 387; `0x145=0x20` attr `0x01` and `0x98`
   attr `0x04` are already excluded; the rule is derived from the measured
   attrs, not guessed). Add the
   constant-rotation anchor for the `0x31` sections with `k` derived from the
   sequencer's voice-cursor state, not chosen by a search; the null test
   includes a rotated copy of the correct trace and a shuffled capture, and the
   mutation test drops one write. Raise `AUDIO_END` above the maximum `X` of the
   29 traces (the `capture-end` command still returns each capture's own `X`).
   Re-pin the traces and budget the extra gate time. Claim: the sections that
   pass; name the rest.
8. **Spec/claim reconciliation.** Fix the stale claims the docs sweep found:
   `audio.md`'s "never heard" and the 29/27 `.dro` wording, `game_flow.md`'s
   ported-but-called-gap lines (`0x17580`, `0x4AAD0`, `0x36870`, `0x4F83C`),
   `RE_GUIDE.md`'s "4b gap", `PROGRESS.md`'s "port is complete", and the
   `gp-u6-moves` audio-inventory contradiction. Claim: no doc states a ported
   function as a gap.

**Mid-phase gate:** `PR_DIFF_FULLSCAN=1 GATE_STRICT=1 PR_PINS_EXACT=1 make
verify` and `make audio-render`, each twice, green under load. Stage 2 does not
start until both runs pass. If the gate cannot be made deterministic, the phase
stops at Stage 1 and records the residual; it does not take captures on a flaky
gate.

### 7.2 Stage 2 — capture truth

9. **Two-human KO.** Harden `tools/gp_capture.py` with a pin guard (queue a
   BIOS word only when `S(f)` and `S(f-1)` are accepted; SIGTERM on a gap), a
   counted skip and a fast-fail preflight. Run the existing `gp-twop-ko`
   scenario once; accept only on `gp_twop.py check --ko` plus every session
   check, **and only if the port's replay reaches the KO** so the oracle's
   `N`/`MIN_REACH` covers it. If the replay stops before the KO, the capture is
   game evidence only with the replay gap named, or not published. When the
   window covers the KO, publish `data/k11-captures/gp-twop-ko` with its
   `GP_TWOP_KO_*` pins, `gp-twop-ko-oracle`, miss set and audio trace; a
   game-evidence-only capture gets no oracle pins. A failure is preserved as
   `.failed.N`; no pins.
10. **U16 R2 (measurement only).** First try a guarded retry of
    `gp-u24-u16-state` until the `s0_54=2` set record is sampled. If the poller
    cannot catch it, take the debugger path: `BPX 0x48599` (the
    `cmp edx,[eax+0x30]`) with a frame/record condition and a hit cap, a
    scripted dump and auto-continue, so the gate operand is read exactly when it
    is live (the breakpoint is in the reaction arm, so it fires for every slot
    and frame without the condition). Claim: the measured `y(0x1532)` and its
    relation to `0x1400`. **No port-code fix in this phase**: a proven reaction
    amplitude/anchor defect is recorded with its raw evidence for a follow-up
    task. No pin if the measurement cannot be taken.
11. **Drawn match `0x0A`/`0x0B` (KO branch).** A poked capture cloned from
    `gp-u9-win` with one KO each way reaches `B1E==ADC && AD4==2`, mode `0x0A`
    and the `0x0B` winner pose. `adc`/`b1e`/`ad4` are not all in `SNAP_FIELDS`;
    add a `DRAW_EXTRA` field set and a `gp_draw.py check` (the existing
    `gp_twop.py` rejects `ad4==2`). New scenario, its own `gp-draw-*` oracle,
    pins and miss set, **and only if the port's replay reaches `0x0A`/`0x0B`**
    so the oracle window covers them; otherwise game evidence only with the gap
    named. The timeout/health drawn round (`0x280BA`) is **not** this claim and
    stays a named gap. A failed run is preserved; no pins.
12. **U14 R1 (conditional).** Only if the design's approval explicitly
    authorizes the next attempt. The task records the two possible forms
    (the 1P 160-attempt script, which has lost three times, or a two-human
    substitute with an idle P2, which is a different `b1f=3` claim) and runs
    the one the approval names. Without authorization the task is skipped and
    U14 stays a declared gap.
13. **Final gate and docs.** Two full gates and `audio-render`; every new pin
    proved at ±1; `port_progress.py`, the diff counter, the gate inventory and
    `symbols.h` checked; the design status, the record, `docs/PROGRESS.md`,
    README, AGENTS and the affected specs updated.

## 8. What the phase will claim, and what it will not

**Will claim:** the full gate is deterministic (two green runs under load); the
completion figure has a class for every `FN_` and a true narrative; 384 of the
385 span-writer slots have an E2 row, with `0x5D004` named by its
`RUNTIME_BASE` rule; `807 → 810` full differential rows and `WEAKER 15 → 12`
(or the measured 809/13 with the `0x255CC` `0x256D4` reason); the `0x31`
sections and banks that pass the derived rotation anchor, with the rest named;
the two-human KO is a published, oracled capture **whose window covers the
KO**; the U16 gate operand is measured (or its bound is recorded); the
KO-branch drawn match's `0x0A`/`0x0B` handlers are captured and oracled.

**Will not claim:** U14's fought win without a go-ahead; the 5120 band's cause;
rendered audibility; the timeout/health drawn round; the 20 terminal rows, the
8 `UNROWED` functions or the remaining `WEAKER` rows; the language reload, coin
countdown, storage slot 13..35, the 39 consumers, U17's tick replay, or blind
spots B2/B3/B4/B6/B7/B8.

## 9. Risks and stop rules

- **The audio-clock change moves many pins.** Stop if a title/attract/front-end
  pin or the `audio-render` peak/sha moves; investigate the tick count before
  re-pinning. The re-pins are allowed only for `TRACE_PINS`, each proved at ±1.
- **The E2 filler admits 157 writers, not a non-writer set.** The residual is
  `0x5D004` above `RUNTIME_BASE`. If the table cannot be regenerated cleanly,
  revert the filler and keep 158 as a named residual. Do not widen the universe
  to make the table look complete.
- **The rotation anchor can overfit.** The null test must include a **rotated
  copy of the correct trace** (not just a wrong trace) and a shuffled capture;
  without it, a searched `k` is not a claim. Derive `k` from the voice-cursor
  state or leave the section unclaimed.
- **Captures fail under host load.** The pin guard and preflight are the
  mitigation; a `.failed` is an acceptable outcome and does not move a pin.
- **U14 may lose again.** It stays a named gap; the phase does not substitute a
  poked win or a two-human win without the approval naming that claim.
- **The classification audit may find more false positives.** Each is a finding
  with its comment/address; the count moves to the measured value, not to a
  target.

## 10. What is left to the builder

- The exact `TRACE_PINS` deltas after the clock fix (measured, not predicted);
  the `audio-render` peak/sha is expected to stay put and must be confirmed.
- Which of the ten inline credits get real headers vs a declared exception
  (besides the three header-shape exceptions).
- Whether `0x5D004` can be given an E2 row outside `RUNTIME_BASE` or must be a
  declared exception; the measured `--expect` counts (the measured reading is
  736/732).
- The exact `+0x14`/`+0x38` write-back values and the `0x1BE30` seam arguments
  (the raw bytes are the source; the record cites the addresses).
- The drawn-match scenario's poke sequence and the U16 debugger automation.
- Whether the two-human KO and U16 runs can be taken on the available host; if
  not, the phase records the harness state and leaves the captures named.
