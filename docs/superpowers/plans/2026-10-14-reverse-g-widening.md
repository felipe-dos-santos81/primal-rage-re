# Track G — widen the proven picture (wave 1) — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development
> (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps
> use checkbox (`- [ ]`) syntax for tracking.

**Goal:** close the harness-limited differential rows (exit model, six non-rows, game_loop's
RNG pokes) and add pixel claims that don't exist yet (pause/prompt screens, mode-8 catch-up),
without moving a single byte-exact oracle line.

**Architecture:** two lanes over one repo. Lane A (serial — shares `tools/diff_emu.py` /
`tools/diff_verify.py` / `port/tests/diff_runner.c`) adds an exit model to `E.Machine`,
converts the two save rows' exit blocks and the `0x2EA68` gap, then rows the six named
non-rows, then replaces `game_loop`'s RNG pokes. Lane B (serial — shares
`tools/title_compare.py` / `tools/gp_compare.py` / the `Makefile` oracle block) adds the
gp-keys-fight frame ratchet, isolates and models the mode-8 three-frame scan-out, then
diagnoses gp-moves N=2139. Tasks in different lanes run in parallel in their own worktrees;
within a lane, strictly sequential (same files). One read-only audit closes the track.

**Tech Stack:** C11 (port), Python 3 stdlib harness (`unicorn`, `capstone` for diff-verify),
GNU make, DOSBox-X (wave 2 only), Ghidra MCP (project `rage`) for read-only byte checks.

**Spec:** `docs/superpowers/specs/2026-10-14-track-g-design.md` — read it first; it carries
the rulings (§C3g.7 reversed, standing MISMATCH rule with the tick-model carve-out, gamepad
deferred, ENDURANCE = new scenario name) and the evidence citations to the four recon
reports in `.superpowers/planning/2026-10-14-next-phase/`.

**Spec (repo):** `AGENTS.md` (binding: address model, evidence discipline, oracle
paragraphs, the counter sentence, `port/src` rules) and `port/RE_GUIDE.md` /
`port/PORTING.md` before touching engine or harness code.

## Global Constraints

- **The byte-exact oracle lines never move.** Ratchet N/F values move **upward only**, each
  with its measured provenance recorded in the Makefile comment and the track record
  (`docs/superpowers/plans/2026-10-14-reverse-g-derivations.md`, one section **§G1..§G7 per
  batch** — the §G.n prefix is reserved by the gameplay-ground-truth record; track G never
  uses bare §G.n).
- Every harness mechanism task ends with the full
  `python3 tools/diff_verify.py --diffrun build/diffrun --exe data/game/C/PRAGE.EXE
  --image <tmp img> --self-check` **byte-identical to base before converting** (the §F.1/§H.1
  discipline), then re-measured and re-pinned after; the counter sentence
  `1000/1000 functions VERIFIED; 14885/14885 mutants detected; 1 named gaps; 638/831 rows
  with callees closed (169 have none)` is the base reading — every task that can move it
  re-measures and records why it moved, in the same commit, naming track G.
- A MISMATCH is a finding fixed from the bytes, one correction per commit, with a mutant
  that is the old code — unless it is a documented `PORT:` deviation, which is reported and
  left alone (spec ruling 2; the exception: if mode-8 isolation shows the **port's
  tick/catch-up model itself** is wrong, stop and escalate as a named finding).
- Every new case/mutant/binding stays inside the 64 MB `mem[]`; a deliberate read above the
  image must be named in `P45_OUTLINE`-style (`tools/tests/test_diff_verify.py`).
- `port/src` changes only: removing `static`, a header declaration, a `PR_SEAM`/`PR_SEAM_RET`.
  Never hand-edit `port/src/symbols.h`. Never write under `data/`; the game-dir `CMOS` stays
  2040 zero bytes or absent (`make verify` enforces).
- `PR_SEAM` rule: a ported function that a verified function stubs/records opens with
  `PR_SEAM(0xADDR, args…)` / `PR_SEAM_RET` (checklist: record
  `2026-10-01-reverse-e3-derivations.md` §E3.10).
- Commits: `<area>: <what changed>`, stage named files, **never `git add -A`** (the `data`
  and `.superpowers` worktree symlinks show untracked — never add them), never push, never
  touch `main` or another lane's worktree.
- Workers run: `make build`, `PR_ORACLE_REQUIRED=1 ./build/run_tests`, per-function and full
  `--self-check`, `python3 -m unittest tools.tests.test_diff_emu tools.tests.test_diff_verify`,
  the drivers behind a lane-B task's comparisons (`PR_GP_DUMP` runs, direct
  `tools/gp_compare.py` / `tools/title_compare.py` invocations). Workers do NOT run:
  `make verify`, any `*-capture` target, DOSBox-X.
- The §H.4 key-count hang limit (port spins to the 60 s timeout when a case under-feeds
  keys, failing closed without naming the row) and the §H.4 screen/data overlap in
  `[0xA0000, 0xAFA00)` bound lane B: name them in any new allowance's record section.

## Review Focus

Failures none of the per-task tests exercise directly, most likely first:

- **A named allowance that silences more than its frame.** Every new allowance
  (835 band, mode-8 splices) must fail if the same capture frame is asked to explain a
  DIFFERENT byte layout — expected: a mutation that changes the allowed frame's port dump
  still reports UNEXPLAINED, proving the allowance is per-frame per-bytes, not per-scene.
- **An exit-model row that passes without the terminal event.** Expected: a case whose
  original never reaches the exit but whose port returns normally FAILS the row; the
  terminal verdict must be *matched*, not merely permitted.
- **A vacuous row inflating the metric.** Each of the six new non-row rows states in its
  claim string what it compares (a dword read, a ret-0); the audit seat checks the claim
  wording against the case set, expected: no row reads as stronger than its cases prove.
- **A counter re-pin hiding a regression.** A callee closure that moved for a reason other
  than the task's own rows is a finding; expected: every delta line names its cause.
- **Lane interference through the `Makefile`.** Both lanes append pins; the integration
  step (T8) re-runs the full gate on the merge, not just per-lane self-checks.

---

### Task 1: Exit model in the emulator (`0x62003` chain)

**Files:**
- Modify: `tools/diff_emu.py` (extend `E.Machine` — track H's class at diff_emu.py:195-217 —
  with an opt-in exit field; the int hook lives in `_Devices.step` (:241-243, currently only
  `int 0x16` is answered and every other int stops `unmodeled`)
- Modify: `tools/diff_verify.py` (the case runner consumes the terminal verdict; `Case`
  gains the expected-terminal field)
- Test: `tools/tests/test_diff_emu.py` (new `G1ExitTests` block, contiguous, prefix `g1_`)

**Interfaces:**
- Consumes: nothing from other tasks.
- Produces: `E.Machine(exits=True)`; `run_original(...)` returns a result carrying
  `terminal = ("exit", al)` when the emulated run reaches `int 0x21 AH=0x4C` (the chain
  `0x62003..0x62723` ends `mov ah,0x4C; int 0x21` — records §N.1, §F.1:171-183); `Spec`/
  `Case` compare it as a declared terminal difference only when the row declares one
  (mechanism precedent: the `H2Case` declared-difference class, §H.3.2). Task 2 uses this.

- [ ] **Step 1: Write the failing tests** in `tools/tests/test_diff_emu.py` — three cases
  on a tiny fixture function of your own row (or an existing cheap one): (a) a case with
  `exits=True` whose run reaches the exit gets `terminal == ("exit", 1)` and the run stops
  cleanly instead of erroring; (b) the identical run **without** `exits=True` still stops
  `unmodeled int 0x21` exactly as today; (c) a run that never exits returns
  `terminal == None`.
- [ ] **Step 2: Run them, verify FAIL** (`python3 -m unittest
  tools.tests.test_diff_emu -k G1Exit`).
- [ ] **Step 3: Implement** the hook in `_Devices.step` (answer `int 0x21` when `AH==0x4C`
  under `machine.exits`, record `AL`, halt the run as terminal) and thread the verdict
  through `run_original`'s result and the Python-side case runner. Convention note in the
  docstring cites `0x62003..0x62723` and the `mov ah,0x4C` terminator from the bytes.
- [ ] **Step 4: Run the tests — PASS. Run the FULL `--self-check`: must be byte-identical
  to base** (no row declares exits yet, so nothing changes: 1000/1000, 14885/14885, 1,
  638/831). Prove the mechanism's absence is *felt*: stash the diff, G1ExitTests fails;
  unstash.
- [ ] **Step 5: `PR_ORACLE_REQUIRED=1 ./build/run_tests`** — all checks pass (untouched
  side).
- [ ] **Step 6: Record §G1** (the terminal-event semantics with the addresses; why `AL` is
  the whole observable; the §H.4-style limits this creates for future rows) and commit
  `tools: track G g1 mechanism: E.Machine(exits=...) models the runtime exit chain`.

### Task 2: Convert the save rows' exit blocks + row the `0x2EA68` gap

**Files:**
- Modify: `tools/diff_verify.py` (`SC_SPECS` load/save entries: add the exit-reaching case;
  the two helpers' comments; new Spec for `0x2EA68` in the menu section)
- Modify: `port/tests/diff_runner.c` (bindings for the new `0x2EA68` row calling the ported
  `menu_fatal_error` (port/src/game/menu.c, `/* 0x2EA68 */`); extend the SC bindings to
  feed the new cases)
- Test: `tools/tests/test_diff_verify.py` (SC_KINDS/SC_CASES pins re-measured; the new row's
  catch-sets; the named-gap line for `0x2EA68` removed — it becomes a row)
- Modify: `AGENTS.md` (weaker-row reasons for `0x1AFE8`/`0x1B084`: from "missing the exit
  block" to "one declared terminal difference"; the 15-unrowed count drops by 1)

**Interfaces:**
- Consumes: Task 1's `Machine(exits=True)` and `terminal` verdict.
- Produces: the terminal-difference precedent used by no other task.

- [ ] **Step 1: short-read case for `0x1AFE8`** — declare `exits` + a file whose length is
  short (the model answers the short count; the raw walks `0x1B04D` → `0x62003` → terminal
  exit AL=1; the port takes its documented `PORT:` failed-open path and returns). The row's
  expected result is the DECLARED terminal difference (original exits (1), port returns),
  stated in the claim string. Write the case first, watch the row fail without the
  declaration (Review Focus #2: a row that would pass without the terminal event is a bug).
- [ ] **Step 2: mirror case for `0x1B084`** where the fwrite path reaches its exit arm
  (`0x1B0F7`: count != 0x7F8 after fclose — force it with an `F1_ABSENT` file under the
  write-open-declared case if reachable; if the bytes show it is not reachable with the
  file model as built, name it and stop — evidence, not a shrug).
- [ ] **Step 3: row `0x2EA68` `menu_fatal_error`** — cases: (a) reaches the exit →
  declared terminal difference against the port's documented return/nothing (`PORT:`
  §S.8 F1); mutant = the C body skipping its observable side. Derive its arg convention
  from the callee's bytes (Ghidra `0x2EA68`) and follow the §E3.10 checklist; add the
  `PR_SEAM` per the porting rule if it is stubbed elsewhere.
- [ ] **Step 4: re-measure the counter and pins** (expect `1001/1001 functions VERIFIED`
  — `0x2EA68` gains its row — mutant/named-gap deltas explained line by line in the record;
  named gaps 9→8 → the AGENTS.md "9 named gaps and 6 named non-rows" sentence updates).
  Full `--self-check` green with the new numbers; re-pin the sentence in
  `tools/tests/test_diff_verify.py` naming track G.
- [ ] **Step 5: run_tests green; commit** `tools: track G g2 converts the save rows' exit
  blocks and rows menu_fatal_error (0x2EA68)`; **record §G2** with the reachability
  evidence for Step 2 either way.

### Task 3: Rows for the six named non-rows (the §C3g.7 reversal)

**Files:**
- Modify: `tools/diff_verify.py` (new Specs: `0x500BB` (47 rows depend on it), `0x1B544`
  resident arm only (slow arm named unhit, 34 rows), `0x29D60`, `0x2EA64`, `0x32BAC`,
  `0x2EA30` — each a minimal case set + mutants per the §E3.10 checklist)
- Modify: `port/tests/diff_runner.c` (bindings: the trivial ones call the port C function;
  `0x500BB`'s binding reads `[0x101500]` on the port side the same way the C does;
  `0x1B544`'s resident arm case forces residency in `mem[]` per its bytes)
- Modify: `tools/tests/test_diff_verify.py` (the unrowed list drops the six; counter
  sentence re-pinned — closed rows rise from 638; recompute with the `closed_rows` replay)
- Modify: `AGENTS.md` (the §C3g.7-era sentences: "6 named non-rows" closes with a reversal
  note; the open-rows list loses those names)

**Interfaces:**
- Consumes: nothing new mechanically.
- Produces: the closure-rule status Task 4 and the audit read; the weak-claim wording style
  every new row follows: one sentence saying exactly what it compares.

- [ ] **Step 1: order = biggest first: `0x500BB`, `0x1B544`, `0x29D60`, `0x2EA30`,
  `0x32BAC`, `0x2EA64`.** For each: derive the arg/return convention from the callee bytes
  (Ghidra), write the case + at least one mutant that the case catches, state the claim
  honestly in the Spec comment (e.g. `0x29D60`: "compares: ret 0 reads nothing, returns
  nothing — the row closes on it because its 16 callers' recorded stubs are now verified
  callees").
- [ ] **Step 2: per-function `--self-check` green for each new row as you land it.**
- [ ] **Step 3: recompute `closed_rows`** (import + replay per recon §3's method) and re-pin
  the counter line in `tools/tests/test_diff_verify.py` + AGENTS.md with a comment naming
  track G and the expected shape `638 -> 715+/831` (measure, don't guess).
- [ ] **Step 4: full `--self-check` byte-identical except the intended deltas**; every
  delta line explained (Review Focus #4).
- [ ] **Step 5: run_tests green; commit** `tools: track G g3 rows the six named non-rows
  (C3g.7 reversed); re-pins the closed count`; **record §G3** with each row's bytes-cited
  claim.

### Task 4: `game_loop`'s RNG pokes → recorded stubs

**Files:**
- Modify: `tools/diff_verify.py` (the `game_loop` `0x255CC` row: replace the two
  `mov eax,0` pokes at the call sites `0x256B1`/`0x256D6` with recorded stubs on
  `0x5D7DC` (`rng_next`, its own VERIFIED row, SPECS #1) — per-case stub EAX, §E3.12
  machinery; keep `0x25680` named unhit with its §H.3.4 reason)
- Modify: `tools/tests/test_diff_verify.py` (game_loop catch-set pins re-measured)

**Interfaces:**
- Consumes: Task 3's closure status (a row closed on a VERIFIED callee now counts).
- Produces: the counter's final lane-A shape.

- [ ] **Step 1: per-function `--self-check` on `game_loop` at base first** (record the
  current reading), then convert one call site to a recorded stub, run it, then the second;
  each stub's per-case EAX comes from the original's consumed values (bytes; the pokes'
  constants were `0` — verify from the callers' use, don't assume).
- [ ] **Step 2: mutants** — the stub EAX matters: add/keep a mutant that returns the wrong
  rng value and must be caught by an existing case (if none catches it, add one).
- [ ] **Step 3: re-pin catch-sets + counter (closed rises where game_loop's row list
  depends on it); full `--self-check` green;** `PR_SEAM` on the port side if
  `rng_next` is a ported callee the harness records (it is — check the seam rule).
- [ ] **Step 4: run_tests green; commit** `tools: track G g4 game_loop's rng pokes become
  recorded stubs on the verified 0x5D7DC row`; **record §G4.**

### Task 5: gp-keys-fight frame ratchet (first pause/prompt pixel claim)

**Files:**
- Modify: `Makefile` (gp-keys-oracle block ~:565-571: add the frame ratchet via
  `tools/gp_compare.py --scenario gp-keys-fight …` with new `GP_KEYS_MIN_FIRST`,
  `GP_KEYS_MAX_START`, sha/frames pins — the replay already runs there)
- Modify: `tools/title_compare.py` **or** `tools/gp_compare.py` (the 835 QUIT-band
  named-allowance mechanism — reuse the `ATTRACT2_SPLICE3_ALLOWED` pattern at
  title_compare.py:529-533 style: per-frame, byte-checked, never a class)
- Test: `tools/tests/` if title_compare unit tests exist there (`make verify` runs a
  "title_compare unit tests" step at Makefile:969 — extend that file)
- Modify: `AGENTS.md` (the gp-keys paragraph: from "no frame is compared (the pause and
  prompt screens are a named gap)" to the new narrow claim + its named limits)

**Interfaces:**
- Consumes: existing dump driver `PR_GP_DUMP` (scenario gp-keys-fight).
- Produces: the per-frame named-allowance pattern Task 6 reuses.

- [ ] **Step 1: run the replay + comparison in report mode** (`gp_compare.py --report` if
  the flag exists — check its argparse; else a report invocation the tool documents) over
  the current `gp-keys-fight` dump and capture; record the first-unexplained frame, the
  window start, and the 835 band's current classification into **record §G5** — before any
  change.
- [ ] **Step 2: decide 835: explain it (a named allowance with bytes) or leave the ratchet
  below it.** Whichever: the allowance names the capture frame AND the port-frame byte
  recipe, and the unit test proves Review Focus #1 (mutating the allowed frame's port bytes
  makes it unexplained again).
- [ ] **Step 3: pin intentionally** (there is no failing-Makefile-step-first for a pin):
  add the `gp_compare` invocation to the oracle target with new `GP_KEYS_MIN_FIRST` = the
  measured value from Step 1/2 and `GP_KEYS_MAX_START` (window-slide guard). The capture
  itself is UNCHANGED — reuse the existing `GP_KEYS_CAPTURE_SHA256`/frames pins as-is. Run
  `make gp-keys-oracle` — green.
- [ ] **Step 4: prove the ratchet fails on a regressed port** (temporarily corrupt one
  explained port frame's dump bytes in a copy of the dump, run the comparison, see it fail,
  restore) — evidence line in §G5.
- [ ] **Step 5: commit** `tools: track G g5 gives gp-keys-fight its frame ratchet (first
  pause/prompt pixel claim)`; update the AGENTS keys-paragraph claim sentence.

### Task 6: Isolate and model the mode-8 three-frame scan-out

**Files:**
- Modify: `tools/title_compare.py` (a named three-frame allowance list **per oracle**,
  patterned on `ATTRACT2_SPLICE3_ALLOWED` — the frames: idle-loss 2064 (ports 1666/1669/
  1670), U9 346 (218/221), U10 331 (216/218/219) plus every new first-unexplained frame
  the re-measure reveals, each byte-derived)
- Modify: `Makefile` (the three oracle blocks' MIN_FIRST + MAX_START pins re-measured
  upward with provenance comments)
- Modify: `tools/gp_compare.py` (plumb the named-allowance argument)
- Record: **§G6** — the isolation write-up (capture ticks vs port frame indices; the
  loader re-sync hypothesis at `0x1B45F/0x1B464` tested, not assumed, per the 3545
  precedent Makefile:288-295)

**Interfaces:**
- Consumes: Task 5's named-allowance pattern.
- Produces: the explanation class Task 7 diagnoses against.

- [ ] **Step 1: isolate first, in a spike** — replay one of the three (idle-loss is
  unpoked: best evidence) with per-tick ISR logging (the port side), align to the capture's
  `frame.txt` tick marks, and write down exactly what the three-frame rows are made of and
  which port frames fall inside one capture scan. **This step's deliverable is the §G6
  isolation record, including a NOT-ISOLATED outcome with its evidence** — if the cause
  cannot be isolated from existing captures, STOP: name it in the record, do not model an
  unexplained allowance (Review Focus: an allowance that silences more than its frame).
- [ ] **Step 2: only on an isolated cause: add the named per-frame allowance** (byte
  recipe per allowed frame; unit test extends the title_compare tests at Makefile:969).
- [ ] **Step 3: re-measure and re-pin the three oracles' MIN_FIRST/MAX_START upward** (each
  with the new first-unexplained provenance; they must not move down).
- [ ] **Step 4: prove Review Focus #1 on each new allowance** (mutate the allowed frames'
  bytes → UNEXPLAINED again).
- [ ] **Step 5: commit** (may be two commits: isolation record; model+pins)
  `tools: track G g6 <isolate|model> the mode-8 three-frame scan-out`.

### Task 7: Diagnose gp-moves N=2139 (the sub-replay-end pixel gap)

**Files:**
- Read-only: `data/k11-captures/gp-u6-moves-b`, dumps under `/tmp/pr_gp_dump/…`
- Modify: `Makefile` + pins ONLY if the diagnosis raises N (upward; never lower).
- Modify: `tools/gp_moves.py`/`gp_compare.py` if the gap is a model artifact Task 6's
  allowance class covers.
- Record: **§G7** — the classification of the 2139 frames' pixels (rows 64..197,
  4279 px per recon §U6.22 / Makefile:531-537): model artifact, named allowance, or a real
  port divergence.

- [ ] **Step 1: re-run the comparison in report mode** with Task 6's allowance active on the
  integrated head (after T6's commits); is 2139 still the first unexplained? Record reading.
- [ ] **Step 2: classify the pixels** — splice of which port frames / transition / true
  mismatch. If a true port mismatch: apply the standing rule (fix from the bytes, one
  correction per commit, mutant = old code) — or, if it is the port tick/catch-up model
  itself, STOP and escalate per the spec ruling.
- [ ] **Step 3: raise N with provenance if the model explains it; leave N pinned and record
  the named reason if not.** No silent movement.
- [ ] **Step 4: commit** `tools: track G g7 classifies the gp-moves 2139 gap` + §G7.

### Task 8: Audit, integration, gate, merge (controller — for the record)

Not a dispatched implementer task: the SDD controller lane. Steps for the ledger:
read-only audit of `reverse-g*` vs base (evidence discipline, claim wording, Review Focus
triage of deferred minors) → integrate lane-A + lane-B branches on `reverse-g` → **full
`PR_DIFF_FULLSCAN=1 make verify`** (the pre-push form) → merge to `main`, push → AGENTS.md
track-G paragraph + counter + the open-rows/weaker-list sentences final read → README title
re-check (`tools/port_progress.py` — unchanged unless a task added a ported target; it
should not) → PROGRESS.md paragraph → ledger close with the rulings list.
