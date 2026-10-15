# Track J — close the ledger, run the captures — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development
> (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps
> use checkbox (`- [ ]`) syntax for tracking.

**Goal:** finish track G's wave 1 (§G4 RNG stubs, §G7 gp-moves + strips), land the
closure-rule amendment and the doc-truth batch, add the rebind, then run the session-gated
capture wave — without moving a single byte-exact oracle line.

**Architecture:** Part I (repo-side, executable now) has two file-serial lanes and two
small parallel tasks. Lane A (`reverse-j1`) owns `tools/diff_verify.py`'s pins: §G4 then
the closure amendment. Lane B (`reverse-j2`) owns `tools/gp_compare.py` + the Makefile's
`GP_*` pin block: §G7. Lane C (`reverse-j3`) owns `port/src/host.c`: the rebind. A short
lane D (`reverse-j4`) lands doc truth after lane A's counter is final. A controller close
audits, integrates on `reverse-j`, runs the pre-push `PR_DIFF_FULLSCAN=1 make verify`,
merges and pushes. Part II (capture wave, session-gated) is one task per capture, run
**one at a time** as the user opens a quiet DOSBox-X window, each on a short branch off
the current `reverse-j` tip; a second controller close gates the drained queue.

**Tech Stack:** C11 (port), Python 3 stdlib harness (`unicorn` 2.1.4, `capstone` 5.0.7
via `tools/requirements-diff.txt`), GNU make, DOSBox-X (Part II only), Ghidra MCP
(project `primal-rage`) for read-only byte checks.

**Spec:** `docs/superpowers/specs/2026-10-15-track-j-design.md` — read it first; it
records the rulings (closure amendment adopted with declared non-goals; captures = new
scenario names only, starting on the user's word; grunt batch off by default; audio
queued; gamepad deferred; live-play notes are notes-only) and cites the four recon
reports in `.superpowers/planning/2026-10-14-next-phase/`.

**Spec (repo):** `AGENTS.md` (binding: address model, evidence discipline, oracle and
counter paragraphs, `port/src` rules) and `port/RE_GUIDE.md` / `port/PORTING.md` before
touching engine or harness code.

**SDD ledger:** `.superpowers/sdd/2026-10-15-reverse-j/progress.md` (git-ignored) carries
the dispatch contracts, worker/reviewer sessions and the held-state of Part II.

## Global Constraints

- **The byte-exact oracle lines never move.** Ratchet N/F values move **upward only**,
  each with its measured provenance recorded in the Makefile comment and the track
  record. Part II pins come from the new captures' own measurements, never fitted.
- Base counter reading at plan time:
  `1007/1007 functions VERIFIED; 14899/14899 mutants detected; 1 named gaps; 716/832 rows
  with callees closed (175 have none)` (`tools/tests/test_diff_verify.py`'s pin sentence,
  `AGENTS.md` §"differential harness"). Every task that can move it re-measures and
  records why it moved, in the same commit.
- Records: **§G4/§G7 stay in `docs/superpowers/plans/2026-10-14-reverse-g-derivations.md`**
  (their contracted sections; their commit messages keep the `track G g4/g7` wording).
  All new batches record in **`docs/superpowers/plans/2026-10-15-reverse-j-derivations.md`**
  as §J1, §J2, … (new work never uses a bare §G.n).
- A MISMATCH is a finding fixed from the bytes, one correction per commit, with a mutant
  that is the old code — unless it is a documented `PORT:` deviation, which is reported
  and left alone. Exception (spec ruling 5): if §G7 or a Part II comparison shows the
  **port's tick/catch-up model itself** is wrong, STOP and escalate as a named finding.
- Every harness change ends with the full
  `python3 tools/diff_verify.py --diffrun build/diffrun --exe data/game/C/PRAGE.EXE
  --image <tmp img> --self-check` **byte-identical to base before converting**, then
  re-measured and re-pinned after.
- Every new case/mutant/binding stays inside the 64 MB `mem[]`; a deliberate read above
  the image is named in `P45_OUTSIDE`-style (`tools/tests/test_diff_verify.py`).
- `port/src` changes only: removing `static`, a header declaration, a
  `PR_SEAM`/`PR_SEAM_RET`, or the real body change the task names. Never hand-edit
  `port/src/symbols.h`. Never write under `data/`; the game-dir `CMOS` stays 2040 zero
  bytes or absent (`make verify` enforces). `host_set_key_bits_override` (host.c:101-108)
  must never be bypassed.
- Captures: **nobody may type while a capture runs**; each pins its `poll.log` sha256 and
  frame count; **new scenario names only** — never re-capture an existing scenario (it
  invalidates its pins). `data/` is read-only; captures are git-ignored; one user of
  `/tmp/pr_gp_dump` at a time.
- Commits `<area>: <what changed>`, stage named files, **never `git add -A`** (the `data`
  and `.superpowers` worktree symlinks show untracked — never add them), never push from a
  worker, never touch `main` or another lane's worktree.
- Workers run: `make build`, `PR_ORACLE_REQUIRED=1 ./build/run_tests`, per-function and
  full `--self-check`, `python3 -m unittest tools.tests.test_diff_emu
  tools.tests.test_diff_verify`, the drivers behind a lane's comparisons (`PR_GP_DUMP`
  runs, direct `tools/gp_compare.py` / `tools/k11_compare.py` invocations). Workers do
  NOT run `make verify`, any `*-capture` target, or DOSBox-X.
- The §H.4 limits bind new cases: the key-count mismatch hang (fails closed without naming
  the row) and the screen/data overlap in `[0xA0000, 0xAFA00)`; name them in any new
  allowance's record section.
- A fresh worktree needs the git-ignored fixtures copied in
  (`port/tests/ghidra_data.bin`, `port/tests/title_screen_ref.ppm`) before any
  `PR_ORACLE_REQUIRED=1` run.

## Review Focus

Failures none of the per-task tests exercise directly, most likely first:

- **A closure rule that inflates the metric.** Expected: a row blocked on any of the 8
  unrowed *ported* functions (the named gaps) stays open, and the counter's wording names
  exactly which classes were excluded. Task 2 Step 3 pins both directions with a mutation.
- **A recorded RNG stub that hides a real divergence.** The stub's per-case EAX must be
  the original's consumed value; expected: a mutant returning a wrong rng value is caught
  by an existing case, and a case whose consumed value differs is not silently absorbed.
  Task 1 Step 2 pins it.
- **An allowance that silences more than its frames.** Every new §G7 allowance must fail
  if the same capture frame is asked to explain a DIFFERENT byte layout. Task 3 Step 3.
- **A doc number transcribed stale.** Every figure in Task 5 is re-measured from its
  source record (raw wins on conflict), never copied from the neighbouring sentence.
- **A contaminated capture.** Stray input during a Part II session poisons the pinned
  `poll.log`; expected: `check-input` clean before any sha256 pin, and no fix-up of a
  pinned capture — re-capture under a NEW scenario name instead.

---

## Part I — repo-side (executable now)

### Task 1: §G4 — `game_loop`'s RNG pokes → recorded stubs

**Files:**
- Modify: `tools/diff_verify.py` (the `game_loop` `0x255CC` row: replace the two
  `mov eax,0` pokes at the call sites `0x256B1`/`0x256D6` with recorded stubs on
  `0x5D7DC` (`rng_next`, its own VERIFIED row) — per-case stub EAX, §E3.12 machinery;
  keep `0x25680` named unhit with its §H.3.4 reason)
- Modify: `tools/tests/test_diff_verify.py` (the `game_loop` catch-set pins re-measured)
- Possibly `port/tests/diff_runner.c` (seam binding) and `port/src` (a `PR_SEAM` only)

**Interfaces:**
- Consumes: nothing new; the §E3.12 stub machinery and `0x5D7DC`'s own row exist.
- Produces: the post-§G4 counter reading Task 2 re-measures from.

- [ ] **Step 1: per-function `--self-check` on `game_loop` at base first** (record the
  reading), then convert one call site to a recorded stub, run it, then the second. Each
  stub's per-case EAX comes from the original's consumed values (verify from the callers'
  use; the poke constants were `0` — do not assume).
- [ ] **Step 2: mutants** — the stub EAX matters: add/keep a mutant that returns the wrong
  rng value and must be caught by an existing case (if none catches it, add one).
- [ ] **Step 3: re-pin catch-sets + counter** (closed rises where `game_loop`'s row list
  depends on it); **full `--self-check` green with only the intended deltas**; `PR_SEAM`
  on the port side if `rng_next` is a recorded ported callee (it is — check the seam rule).
- [ ] **Step 4: `run_tests` green; commit**
  `tools: track G g4 game_loop's rng pokes become recorded stubs on the verified 0x5D7DC row`;
  **record §G4** in the track-G record.

### Task 2: The closure amendment (declared non-goals stop blocking a closed row)

**Files:**
- Modify: `tools/diff_verify.py` (the closed-row counting: the closure test's treatment of
  allow/stub callees vs the declared non-goal classes)
- Modify: `tools/tests/test_diff_verify.py` (the pin sentence + the new rule's regression
  case)
- Modify: `AGENTS.md` (the counter sentence; the open-rows census paragraph; the
  weaker-list sentence — all re-pinned from measurement)
- Record: **§J1** in the new track-J record

**Interfaces:**
- Consumes: Task 1's measured counter (runs after it — same files, serial lane).
- Produces: the final differential numbers Tasks 5-6 quote.

- [ ] **Step 1: baseline + classification first.** Reproduce `832/716/116` with the
  closed-rows replay; list each open row's blocker classes. The rule to implement (design
  goal 3): an allow-mode callee **outside the portable set** does not keep a row open —
  runtime ≥0x5D000, classification host-owned/deferred (`tools/port_classification.txt`),
  and E2-class non-functions (the E2 triage table; §G3's census names the current sets) —
  and each excluded class is **named** in the record and in the counter's wording. A row
  blocked on any of the 8 unrowed ported functions (the named gaps) **stays open**.
- [ ] **Step 2: implement the exclusion** in the closure test, with the classes derived
  from the repo's own tables (not a hardcoded address list).
- [ ] **Step 3: the regression case:** a row blocked only on excluded classes counts
  closed; a row blocked on a portable named-gap function stays open; mutation-prove both
  (flip a class entry → the count moves; add a named-gap blocker → the row re-opens).
- [ ] **Step 4: re-measure** with the replay + full `--self-check`; re-pin the sentence in
  `tools/tests/test_diff_verify.py` and `AGENTS.md` with the measured numbers, stating the
  rule, and report the secondary number "closed on full rows" (today: 12 weaker of 739
  rowed). Every delta line names its cause.
- [ ] **Step 5: `run_tests` green; commit**
  `tools: track J j1 closure rule: rows open only on declared non-goals count closed`;
  **record §J1** with the per-row classification table.

### Task 3: §G7 — classify gp-moves N=2139 + the two 166-px strips

**Files:**
- Read-only: `data/k11-captures/gp-u6-moves-b`, `gp-u9-win`, `gp-u10-ending`; dumps under
  `/tmp/pr_gp_dump/…`
- Modify: `Makefile` pins only as the evidence lands (`GP_MOVES_MIN_FIRST` :550, and
  `GP_WIN_MIN_FIRST` :865 / `GP_ENDING_MIN_FIRST` :934 / `GP_IDLE_LOSS_MIN_FIRST` :475 if
  the strips explain out — upward only, provenance comments)
- Modify: `tools/gp_compare.py` only if the gap needs a new named allowance (the §G5/§G6
  per-frame, byte-checked pattern)
- Test: `tools/tests/test_gp_compare.py` (where §G5/§G6's allowance tests live; extend
  if a new allowance is added)
- Record: **§G7** in the track-G record

**Interfaces:**
- Consumes: §G6's active allowances (`SCAN3_ALLOWED`).
- Produces: the re-pinned or named readings the Part I close gates.

- [ ] **Step 1: replay + report first** (solo `/tmp/pr_gp_dump` user):
  `make gp-replay scenario=gp-u6-moves-b`, then `make gp-report scenario=gp-u6-moves-b`;
  is 2139 still the first unexplained with §G6's allowances active? Record.
- [ ] **Step 2: classify the pixels** (rows 64..197, 4279 px): splice of which port frames
  / transition / true mismatch. Same for the two six-row strips (rows 192..197, x 0..85,
  sha `a886c0f3..`, identical at U9 2209 and U10 2062) — it is **not** the §G6 class; name
  the class it is.
- [ ] **Step 3: decide per the standing rule** — true mismatch → fix from the bytes (one
  correction per commit, mutant = old code); model artifact → a named per-frame allowance
  with the mutation proof in `tools/tests/test_gp_compare.py` (Review Focus #3; the §G5
  allowance test lives in that file, ~:399-492); tick-model wrong → STOP and escalate.
- [ ] **Step 4: raise N only with provenance; else leave pinned with the named reason.**
  If the strips explain, re-pin U9/U10/idle-loss upward as far as honesty carries and note
  the win/ending pixel claim's new reach past f=0x48A.
- [ ] **Step 5: `make gp-moves-oracle` green** (and `gp-win-oracle`/`gp-ending-oracle` if
  their pins move); commit
  `tools: track G g7 classifies the gp-moves 2139 gap and the two strips`; **record §G7**.

### Task 4: Host rebind + live-play notes

**Files:**
- Modify: `port/src/host.c` (:57-61 the hardcoded defaults; :110-122 `host_key_bits`;
  keep :101-108 `host_set_key_bits_override` untouched)
- Modify: `port/tests/test_platform.c` (new seeded cases)
- Record: **§J2** in the new track-J record

**Interfaces:**
- Consumes: track S's config image at `0x100CE4` (binding scans `+0x2DE..+0x2ED`).
- Produces: the windowed path reads the configured bindings; the replay seam path is
  byte-identical.

- [ ] **Step 1: tests first** — plant non-default binding scans in the config image →
  `host_key_bits` returns the configured pad words; leave the scans zero (or the image
  absent) → today's defaults. Both cases seeded, both able to fail.
- [ ] **Step 2: run them → FAIL (the configured case fails today); implement** the read
  with the defaults fallback; **run → PASS**.
- [ ] **Step 3: prove no pinned path moved** — `PR_ORACLE_REQUIRED=1 ./build/run_tests`,
  and a gp replay comparison unchanged through `host_set_key_bits_override`.
- [ ] **Step 4: SDL failure-branch reachability** — exercise/report deterministically
  which branches this host can reach (audio-open failure is already the observed
  behaviour; the no-display branch takes a dummy video driver); record what is and is not
  reachable here; no forced tests for unreachable branches.
- [ ] **Step 5: live-play notes** — one plain paragraph (key map + behaviours:
  idle-reboot, credits, quit) for the record; notes only, no new features. Commit
  `port: track J j2 host_key_bits reads the configured binding scans`; **record §J2**.

### Task 5: Doc truth

**Files:** `README.md` (:44-48), `Makefile` (:901), `docs/PROGRESS.md` (:11 header + the
missing track-G paragraph)

**Interfaces:**
- Consumes: Tasks 1-2's measured counter (dispatch after them; the README sentence must
  quote the current numbers). Task 3's pins only if a sentence names them.

- [ ] **Step 1: re-measure every number from its source** (the counter from the harness
  run; rows/full/weaker/unrowed from `AGENTS.md` post-Tasks 1-2; the corrections figure
  from track D's record §D.12 — README says 174, §D.12's summary is checked for 172;
  resolve from the records, raw wins).
- [ ] **Step 2: README fixes** — the verification sentence (`1000/1000 … 14885/14885` →
  measured), the rows sentence (`732 of 747 … 723 full … 9 … other 15` → measured
  `739/727/12/8`-shape), the corrections figure; re-read the "last 16" storage sentence
  for staleness.
- [ ] **Step 3: `Makefile:901`** — mark the "the replay still misses `0x29C78`" line
  superseded (the P7 commits ported it; the re-measure below it already says so).
- [ ] **Step 4: `docs/PROGRESS.md`** — add the track-G paragraph (G1-G3, G5-G6,
  the counter at their close, pins U9 2209 / U10 2062 / idle-loss 7559 / keys 1136) and
  refresh the header from "(2026-10-12)" to the current state.
- [ ] **Step 5: commit** `docs: track J doc truth: the counter, the rows list, the track-G paragraph`.

### Task 6: Part I close (controller — for the record)

Not a dispatched implementer task: the SDD controller lane.

- [ ] Read-only audit of `reverse-j1`/`reverse-j2`/`reverse-j3`/`reverse-j4` vs base
  (evidence discipline, claim wording, Review Focus triage; the track-G ledger's deferred
  minors m-a..m-c, m-e, m-h, g6-m2..m6, n-a..n-c get fulfilled or formally closed).
- [ ] Integrate the lanes on `reverse-j`; **full `PR_DIFF_FULLSCAN=1 make verify`** (the
  pre-push form); merge to `main`, push (SSH workaround:
  `GIT_SSH_COMMAND='ssh -o IdentityAgent=none -o IdentitiesOnly=yes -i ~/.ssh/id_ed25519'`).
- [ ] `AGENTS.md` final read; README title re-check (`python3 tools/port_progress.py` —
  unchanged unless a task added a ported target; none should); PROGRESS.md track-J
  wave-1 paragraph; ledger close with the rulings list.

---

## Part II — the capture wave (session-gated; one task per capture)

Order (design goal 4, evidence-per-hour): save-reload → ENDURANCE → ending2 → win-live →
join → twop2 → joystick. Each task's first step is a short design shown to the user with
the capture go-ahead; **nothing runs before the user opens a session window**. Tasks run
**one at a time** on a short branch off the current `reverse-j` tip
(`reverse-jc-<scenario>`), controller-merged after its oracle is green. Pins are measured
from the capture; a difference is fixed from the bytes (or named), never re-pinned
towards the port.

### Task 7: `save-reload` capture and oracle

The storage plan's Task 8 contract stands **verbatim**
(`docs/superpowers/plans/2026-10-13-reverse-s-storage-layer.md`:495-511): modify
`tools/k11_session.py`, `tools/k11_capture.py`, `Makefile`, `port/tests/test_game.c`
(the `PR_K11_DUMP` driver); create `tools/save_reload.py`,
`tools/tests/test_save_reload.py`; capture at `data/k11-captures/save-reload`.

- [ ] Follow its Steps 1-7: read the tooling; write the design in §S.9 and **show it to
  the user with the go-ahead request**; implement; capture (nobody typing; `check-input`
  clean); the oracle (`make save-reload-oracle`: write byte-identical to the captured
  `CMOS`; load holds the three polled ranges at the pinned frame; skips without the
  capture); a difference is a finding; commit. **Record §J3** (its provenance cross-links
  §S.9). If the user declines or the capture cannot be made: record the named gap in the
  record/AGENTS/PROGRESS and skip its Steps 3-5.

### Task 8: ENDURANCE past the D3 cut (`gp-u12-endurance`)

Goal: the only ownerless named gap (§U8.9) — the fight past the 300-frame team-select
cut, through the `0x44798` picks and modes 0x30/0x31.

- [ ] Design the scenario script (extend `gp-u8-endurance`'s session; the port already
  replays to the cut); user go-ahead; capture under the no-stray-input rule;
  `python3 tools/gp_capture.py check-input data/k11-captures/gp-u12-endurance`.
- [ ] Pins measured and pinned as a new `GP_U12_*` block (sha256, frames, MIN_FIRST,
  MAX_START; provenance comments); added to `make verify` and the AGENTS Commands list;
  comparison vs the port replay in the existing gp_compare form; commit
  `tools: track J j4 the ENDURANCE fight past the D3 cut`; **record §J4**.
- [ ] The old `gp-u8-endurance` capture and pins stay untouched (new scenario name only).

### Task 9: char≠0 ending (`gp-u13-ending2`)

Goal: prove an endings-table entry §W.6 leaves open; clone `gp-u10-ending` with a cursor
offset to a non-0 character.

- [ ] Clone the scenario script; user go-ahead; capture + `check-input`; pins as a
  `GP_U13_*` block; comparison — trace/milestone claim honest, frame claim as far as the
  mode-8/§G7 model carries it; commit `tools: track J j5 a second character's ending`;
  **record §J5**.

### Task 10: won match without pokes (`gp-u14-win-live`)

Goal: kill §W.9's "what a poke replaced is not claimed" for one match; first
fought-KO evidence including the death animation and `0x37FF2`.

- [ ] Hit-landing script from `gp-u6-moves-b`'s move knowledge; user go-ahead; longer
  capture, `memsize=64` if the fought final needs it; `check-input`; pins as a
  `GP_U14_*` block; comparison vs the port; commit
  `tools: track J j6 a real won match`; **record §J6**.

### Task 11: mid-match join (`gp-u15-join`)

Goal: the §U8.9/§T uncovered 2P entry (`0x28CC8` in a round); short scenario; the join
judge-rule already exists in gp_keys.

- [ ] Script the F2 join mid-round; user go-ahead; capture + `check-input`; pins as a
  `GP_U15_*` block; state + frames comparison; commit `tools: track J j7 the mid-match
  join`; **record §J7**.

### Task 12: extended two-player + finisher (`gp-u16-twop2`)

Goal: raise the two-human claim past the match's first script end; first finisher frames
(all entries are ported).

- [ ] New scenario (never re-capture `gp-twop`); user go-ahead; capture + `check-input`;
  pins as a `GP_U16_*` block; comparison; commit
  `tools: track J j8 the extended two-player match`; **record §J8**.

### Task 13: joystick probe (`joystick-probe`)

Goal: settle faithful-vs-host gamepad with one DOSBox-X session
(`joysticktype=2axis` + mapper-bound axes).

- [ ] Human-assisted session (report-only evidence — not a replay fixture); capture what
  the original does with game-port input; if interpretable, record it and feed the
  gamepad decision's evidence file; if not, formalize the named gap (recon-01 item 8's
  decline) in the classification/AGENTS. Commit `tools: track J j9 the joystick probe`;
  **record §J9**.

### Task 14: Part II close (controller — for the record)

- [ ] As the queue drains: re-measure pins, merge each `reverse-jc-*` branch, run the
  full gate, push; confirm no oracle line moved and every new pin has provenance;
  PROGRESS.md track-J paragraph; ledger close with the rulings and the held/declined
  captures named.

## Deferred (not planned here)

- **The grunt batch (design goal 5, off by default):** handler-table rows (`0x24568`
  family, ~50–80 agent-h), the setjmp/longjmp model (8–14), the renderer rows (12–16) —
  a separate plan if the user opts in.
- **Frontier residue:** capture 835's cause note, U6b's 4-of-12 unshown moves, U8 rows'
  fights past their cuts, the under-render blind spots, the `0x2EA68` exit-path decision
  (after Task 7's evidence), the pause-clock / language-reload items.
- **Host queue:** audio bring-up (device-capable host), the SDL-gamepad map (after
  Task 13), CI/alternative-OS builds, pacing/display policy.
- **Paper categories:** runtime-region porting, the deferred Smacker movie-audio lines.
