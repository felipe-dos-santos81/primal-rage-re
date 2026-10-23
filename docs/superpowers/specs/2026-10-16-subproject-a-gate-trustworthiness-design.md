# Sub-project A — make the gate mean what it says (2026-10-16)

Status: DRAFT — written from an eight-lane read-only recon of the current tree
(2026-10-16, HEAD `bffb779e`) plus three focused fix-surface analyses of the same
rev; builder-checked. The open calls below are the controller's, overturnable.

Evidence base: `AGENTS.md` (address model, oracle paragraphs, differential
harness), `docs/PROGRESS.md` tail, and the live tools
`tools/gp_compare.py`, `tools/title_compare.py`, `tools/attract_compare.py`,
`tools/k11_compare.py`, `tools/smk_compare.py`, `tools/diff_verify.py`,
`tools/tests/test_diff_verify.py`, `port/tests/test_fight.c`, `Makefile`. Prior
specs: `2026-10-15-track-j-design.md`, `2026-10-14-track-g-design.md`.

## Intent and the why

Track J closed the differential closure rule and ran the first of its capture
wave (`save-reload`). The ledger reads
`1007/1007 functions VERIFIED; 14900/14900 mutants detected; 1 named gaps;
774/832 rows with callees closed (175 have none)` and `python3
tools/port_progress.py` reads `787 1203 65` / `747 747 100`. The reverse
engineering is essentially finished: the 416 unported functions are a paper
category (351 runtime ≥`0x5D000` served by the host libc, 65 classified
host-owned/deferred); there is no true code-region gap; and the only missing
user-visible feature is the deliberately-deferred Smacker movie audio, which is
unverifiable without an audio oracle.

So the value now is in what the green lines *prove*. And there is one
structural weakness, found independently by the recon's differential,
gameplay and tooling lanes: **the comparison is one-directional.** Every oracle
asks "is every content-bearing capture frame explained by the port's frames?"
It never asks the converse, "does the port exhibit every frame the capture
shows?" — because each window's start and end is derived from where the port's
own matches begin and stop. A port that renders *fewer* screens shrinks its own
window and passes. This bounds the meaning of every frame ratchet and every
pixel oracle, and **no capture session can close it** — a new capture re-pins
the same blind window.

Sub-project A closes that blind spot (A1) and pays down the adjacent trust debt
that the same recon measured: the ASLR-dependent out-of-bounds write whose
`setarch -R` workaround is not wired into the gate (A2); two harness tests that
run in no gate, one of them dead (A3); a differential counter that cannot tell a
full row from a weaker one (A4); and stale docs (A5). Each is small alone; the
theme is coherence — after A, a green `make verify` means what a reader already
assumes it means.

## Goals

### A1 — Window-bound ratchet (the comparison model)

The blind spot is window **truncation**: each oracle's window bounds are
capture-derived but the *comparison universe* (which port frames exist) and the
window END follow the port's matches. Concretely, from the recon:

- `tools/gp_compare.py` computes a coverage list `missing` (port non-black
  frames up to the last exhibited one, not exhibited by any classified capture
  frame) at `:558-564` and explicitly does **not** feed it to the verdict —
  "reported, not ratcheted" (a §I item-9 named gap, enshrined by
  `tools/tests/test_gp_compare.py:168-175`). The window START is the first
  capture frame exhibiting port frame 0 (`:460-472`), guarded by `start_check`
  (`:425-446`) which fails **only** on `start > max_start` (`:435`) and treats
  `start < max_start` as an improvement (`:439-441`).
- `tools/title_compare.py` derives its window `[a,b] = [idx[0], idx[-1]]` from
  where capture frames start/stop exhibiting port frames (`:264-275`); coverage
  **is** ratcheted inside that window (`missing`/`bad_missing`/`endpoint_ok` →
  `bad`, `:280-290`, `:315`), but the **front-end branch discards its `rc`**
  (`:653`), keeping only the unexplained set (documented `:624-638`) — so the
  enforced front-end oracle abandons coverage by design, and the window end `b`
  can shrink.
- `tools/attract_compare.py` prints the port `exhibited Y` count (`:104-106`)
  and does not ratchet it; its gate is the capture-divergence exact-match
  (`:119-132`).
- `tools/k11_compare.py` ratchets coverage **only over the settled screens**
  (`wanted`, `:158-160`, `:190`) plus an END ratchet (`:133-134`, `:144`,
  `:176-180`).
- `tools/smk_compare.py` is the strongest: a fixed `--frames` count is
  required on both sides and a missing port frame fails (`:22`, `:27-33`).

**Change (Option B, chosen):** make every frame oracle fail if the port
exhibits fewer port frames than the measured baseline, in addition to today's
"every content-bearing capture frame is explained":

1. `gp_compare.py` — pin the window **reach** (`p`, the highest port frame any
   classified capture frame exhibits) as a two-sided ratchet: a port that stops
   early (smaller `p`) fails, a genuine extension is allowed by name (the same
   "improved: raise the pin" pattern `start_check` already uses at `:439-441`).
   `start_check` itself keeps its one-directional semantics — an earlier START
   is genuinely better — because the truncation leak is the END, not the START.
   Optionally ratchet the coverage `missing` list for interior skips (a
   `GP_*_MAX_MISSING` pin with a named allowance); both-bounds, since the
   one-sided "≤" leaks the shrink direction.
2. `title_compare.py` — ratchet the window end `b` and stop the front-end
   branch discarding its `rc` (`:653`); pin the front-end window end. The
   front-end dump legitimately runs into state 9, so its coverage false-fails on
   raw bounds — the pin is the measured end, not a general coverage count.
3. `attract_compare.py` — ratchet the port `exhibited Y` count.
4. `k11_compare.py` — extend coverage beyond the settled screens where the
   capture shows content.

Each genuinely-unexhibited frame gets a named, byte-checked allowance in the
§G5/§G6 style (first instance: the character-select capture's 8 wipe/load
screens, `§C5.19`). The comparison pixels do not change; the verdicts and the
new `GP_*` values are re-measured from the existing captures, each with its
provenance in the Makefile comment.

### A2 — the ASLR out-of-bounds write → retire `setarch -R`

Cause (recon, verified): `port/tests/test_fight.c`'s `p45_check_p5_spawns`
builds an actor free list under the process's real pool base via `c4r_pool()`
(`:47054`) → `actors_reset()` (`port/src/game/actors.c:1143-1146`), then
rewrites the base `DSD(0x001014F4u) = Z_R1;` (`:47066`). `actor_spawn` then
computes `index = (rec - pool_base()) / ACTOR_REC_SIZE` (`actors.c:3782`)
against the new base and `pset = DSD(DS_001014EC) + (index & 0xffff) *
PSET_SIZE` (`actors.c:3784`) with **no `in_pool` guard** (unlike `actor_pset`,
`actors.c:1241-1245`), writing at `0x41107E0` — past `MEM_SIZE`
(`docs/PROGRESS.md:1044`, `AGENTS.md:64-68`). Silent where ASLR-off maps that
address, a SIGSEGV where it does not; values deterministic; base-identical at
`2ccd403e`.

**Change:** re-seed the test so the free list and `pool_base()` agree — move the
base assignment above `c4r_pool()` (test-only edit; `port/src` untouched, so no
`/* PORT: */`) — re-derive the assertions that observed the wild write
(`test_fight.c:47078-47079`; `:47079` becomes a real 3==3 check), and add a
mutation-proofed in-pool assertion (`actor_index(child) != 0xFFFFFFFF`) so an
inconsistent seed fails loudly. **No port guard** — the original trusts its
pool, and a guard could mask a genuine pool bug.

**Retire `setarch -R`:** no Makefile wrapper exists or is needed (it is a
documented manual prefix, `AGENTS.md:68`). Delete/reword the `AGENTS.md:64-68`
bullet and the `PROGRESS.md:1044` paragraph, and confirm `make verify` green
**without** `setarch -R`.

### A3 — orphan harness tests

The tools unittest suite is not a single target; modules are named in each
target's `-m unittest` invocation (e.g. the k11/gp list at `Makefile:1148`, the
title_compare list at `:1150`, the diff list at `:1077`).

- `tools/tests/test_title_pin.py` / `tools/title_pin.py` — the tool is live
  (`title-pin` target; `title-capture`/`frontend-capture`/`gp-capture` depend on
  it). The test is a `unittest.TestCase` and **passes today** (10/10, verified
  this session; the recon's "fails" is stale — it was fixed at closeout, commit
  `a3ff97ab`, recorded `PROGRESS.md:996`). Only the gate is missing.
  **Change:** append the module to the existing tools-unittest list
  (`Makefile:1148`).
- `tools/tests/test_mdi_disasm.py` / `tools/mdi_disasm.py` — the tool is
  referenced by no gate or capture target and has no consumer (the OPL path is
  C); the test uses module-level `def test_*` and collects **0 tests** under
  `unittest` (pytest-only). **Change:** delete both files, and remove the stale
  "ungated test" notes from `PROGRESS.md` (`:981`, `:985`, `:987`, `:990`,
  `:994`).

### A4 — metric honesty

The counter sentence is produced at `tools/diff_verify.py:34405-34414` and
pinned at `tools/tests/test_diff_verify.py`'s
`RealFunctionTests.test_the_self_check_counts_functions_mutants_gaps_and_closed_rows`
(the line cited in this dated design, `:32024`, has since moved). It cannot distinguish a full
row from a "weaker" one: `SpecResult` (`diff_verify.py:388-402`) drops the spec's
`Case.terminal`, `Case.pokes` and `Spec.unhit_named`; and "1 named gaps" is the
spec `NAMED_GAP` count (one spec, `host_1b890`), while **8 ported functions have
no row at all** and appear only in prose (`AGENTS.md:385-386`).

**Change:** add `weaker: bool` to `SpecResult`; add explicit `WEAKER` (the 12
named rows, `AGENTS.md:398-412`) and `UNROWED` (the 8 functions,
`§J1:26`) frozensets; set `weaker` in `verify_spec`/`verify_gap`; and extend the
sentence with `; N/M closed on full rows; L ported functions with no row`. An
explicit set, not a heuristic — a `terminal | pokes | unhit_named` heuristic
flags ~120 rows (~120 `unhit_named=` sites, most declared dead). Re-pin
`RealFunctionTests.test_the_self_check_counts_functions_mutants_gaps_and_closed_rows`;
add a unit test that distinguishes full from weaker
and is mutation-proven to flip; update `AGENTS.md:384-385`.

### A5 — doc truth

- Add the missing save-reload PROGRESS paragraph (record
  `2026-10-15-reverse-j-derivations.md` §J3.10; pins
  `Makefile:427-430`).
- Fix `PROGRESS.md:1028` (the Track-S paragraph still says "no recording of the
  original writing or loading a real save exists yet" — contradicted by
  `AGENTS.md:351-359` and `data/k11-captures/save-reload`).
- Fix `PROGRESS.md:1042` (the capture wave "is held"; the save-reload part
  landed at `bffb779e`).
- `README.md:53` — **172 is current and deliberate** (track-D-scoped); 174 is
  the program-wide total (track N's two, `PROGRESS.md:1034`). Disambiguate the
  wording rather than change the number.
- Fold A4's new figures into `AGENTS.md:384-385`; clarify the `AGENTS.md:415-424`
  blocker-count wording (the per-blocker counts sum above 58 because a row can
  cite more than one; state the unit).

## Non-goals

- **No byte-exact oracle line and no existing pin moves** except where A1's new
  measured bounds require it; each moved `GP_*` value carries its provenance.
- **No new capture sessions** — that is sub-project C. A uses only the captures
  already in `data/k11-captures/`.
- **No porting** — the unported set is a paper category; movie audio stays
  deferred; sub-project D owns host audio/gamepad/CI.
- **No grunt differential rows** — the handler-table/setjmp/AIL rows are
  sub-project B.
- **No general "N-frame" comparison model** — only named, byte-checked per-frame
  allowances (the §G5/§G6 precedent).

## Anti-goals (failure even if green)

- An A1 bound achieved by loosening a comparison rather than measuring the
  capture; or a gate that still passes an under-rendering port (the entire
  point is that it must fail one).
- A new assertion that cannot fail (unseeded, one-sided where the leak is the
  other way, or asserting an unseeded BSS-zero).
- A doc figure transcribed from a neighbouring sentence rather than re-measured
  from its source record.
- A `setarch -R` "retirement" that leaves the OOB path reachable under another
  seed.

## Constraints

- The byte-exact oracle **verdict lines** never move: the pre-A `make verify`
  oracle output (each oracle's pass/fail and its N/F/ratchet numbers) equals the
  post-A output; any new coverage line A1 prints is additive and named. The
  `CHECK`/assertion total grows only by the documented additions (A1's allowance
  tests, A2's in-pool check, A4's full-vs-weaker test), and no existing
  assertion changes.
- Every new pin is measured from an existing capture, with its provenance in the
  Makefile comment and the derivation record.
- Every new assertion is mutation-proven able to fail (AGENTS "Assertions must
  be able to fail").
- `port/src` is untouched by A (A2 is a test-fixture edit; A1/A4 are `tools/`;
  A3 is `Makefile` + `tools/`; A5 is docs). `port/src/symbols.h` is never
  hand-edited.
- Commits `<area>: <what changed>`, named files only, never `git add -A`;
  records in `docs/superpowers/plans/2026-10-16-reverse-a-derivations.md`.
- A fresh worktree needs the git-ignored fixtures copied in before any
  `PR_ORACLE_REQUIRED=1` run.

## Verification

- **A1 (the key proof):** mutate a port dump by dropping a port frame the capture
  exhibits → the owning oracle must now FAIL (today it passes). Then the
  reverse: the unmutated oracles stay green with the new measured bounds; the
  named allowances are mutation-tested so an allowance cannot silence a
  different byte layout (§G5 discipline).
- **A2:** the in-pool assertion FAILS when the old (pre-fix) seed is restored;
  `make verify` is green **without** `setarch -R` on this host.
- **A3:** `test_title_pin` runs in the gate; `grep -r mdi_disasm` finds no
  live reference after deletion.
- **A4:** the new "closed on full rows" figure MOVES under a `WEAKER` mutation
  (add/remove a row) — the pin in
  `RealFunctionTests.test_the_self_check_counts_functions_mutants_gaps_and_closed_rows`
  fails, then the
  mutation is reverted.
- **Whole-A:** `PR_DIFF_FULLSCAN=1 make verify` green on the integrated tree,
  with the oracle verdict lines equal to the pre-A baseline's (new coverage
  lines additive and named) and the differential counter's new clauses at their
  measured values; `symbols.h` idempotent; E2 table byte-identical.

## Calls (the controller's; overturn any)

- **A1 = Option B** (two-sided window-bound pins + frontend `rc`), not Option A
  (gp coverage alone — leaves the truncation direction open) and not Option C
  (capture-anchored bounds — reserve for a later pass if B proves insufficient).
- **A2 = re-seed the test**, not a `/* PORT: */` `in_pool` guard in
  `actor_spawn`.
- **A3 = gate `test_title_pin`; delete `mdi_disasm`.**
- **A4 = explicit `WEAKER`/`UNROWED` sets**, not a heuristic.
- **A5 = keep 172 (track-D-scoped) and state 174 (program-wide)** rather than
  change the number.

## Risks

- **A1 re-measures every `GP_*` pin.** A wrong bound fails an oracle
  spuriously; a too-loose bound re-opens the leak. Each pin is measured from the
  existing capture and recorded, and the "drop a port frame" mutation proves the
  bound bites.
- **A1 touches the enforced front-end oracle.** Its `rc` is discarded by design
  because raw coverage false-fails (the dump runs into state 9); the fix must
  pin the measured end, not reintroduce the false failure.
- **A4's `WEAKER` list is hand-maintained.** It must be kept in step with
  `AGENTS.md:398-412`; the unit test pins the distinction, and the record names
  the list's source.

## What's left to the builder

- Lane splits and ordering: A1/A2/A3/A4 are near-independent (A1 and A3 share
  the `Makefile` and so serialize on it); A5 runs last and quotes A4's final
  figures and A2's doc edits.
- The exact `GP_*_COVERAGE` / window-end pin names, values and provenance
  comments; the exact two-sided `start_check` semantics; the per-oracle named
  allowance table (charsel's 8 screens first).
- The exact new counter wording and the `WEAKER`/`UNROWED` literals, and the
  mutation-proven unit test that distinguishes them.
- The exact `test_fight.c` re-seed edit and the in-pool regression assertion,
  and the precise `AGENTS.md`/`PROGRESS.md` rewordings for A2 and A5.
