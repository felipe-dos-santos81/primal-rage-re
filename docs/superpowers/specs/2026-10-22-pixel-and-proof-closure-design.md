# Pixel and Proof Closure — Design

**Date:** 2026-10-22
**Status:** Implemented and gated (2026-10-23)
**Predecessors:** `2026-10-20-gameplay-truth-ii-design.md` (GT2) and
`2026-10-22-evidence-closeout-design.md` (evidence closeout)

## Intent

The portable port is complete: `tools/port_progress.py` reports 759/759
portable functions (799/1203 raw, 66%), and no unported non-runtime function
remains that is game logic. The remaining weaknesses are evidence, not missing
code. Two of them are offline and can be closed now:

1. **U13's in-replay pixels.** `gp-u13-ending2` is the only enforced gameplay
   oracle whose first unexplained capture frame (4248) lies *inside* the
   replay's range. The trace and WIN fields say the state is right through the
   replay's end (f=10013), so this is the one place where the port's pixels may
   still be wrong on a path the state checks say is right. The U10 precedent
   (capture 4113) was a real RLE span-writer bug, not an artefact.
2. **The differential's rowable frontier.** 59 of 833 differential rows remain
   open. Twenty-eight of them are blocked only by game code whose callees are
   already VERIFIED, and one row (`fighter_state_3531c`) is blocked by a spec
   address that is not a function at all. Row these and the
   `rows with callees closed` figure moves 774 → 802 while every existing pin
   stays put.

The phase also fixes the two counters that currently overstate what they
measure: the `8 ported functions with no row` figure is a hand-maintained
frozenset, not a scan (the true `FN_` ported-unrowed population is larger), and
the 59 open rows' blocker table still names functions that later phases ported.
Neither is a player-visible defect, but both are evidence claims, and this
project treats evidence as the product.

## Current state (2026-10-22, main `1accd1b7`)

- `make verify` is green. Differential: `1019/1019 functions VERIFIED;
  14956/14956 mutants detected; 0 named gaps; 774/833 rows with callees closed
  (186 have none); 768/833 closed on full rows; 8 ported functions with no
  row`.
- `port_progress.py`: `799 1203 66` / `759 759 100`.
- U13: capture `data/k11-captures/gp-u13-ending2` exists, sha
  `e581916f…`, 5904 frames. The enforced compare starts at capture 96, reaches
  port 3549, and stops at capture 4248 (raw 7887), nearest port 3551, rows
  0..96, x 0..319, 22,923 px. `tools/gp_compare.py --report` also lists 4320,
  5120 and 5891..5903. `GP_U13_MIN_FIRST = 4248`; trace and WIN fields agree
  through f=10013.
- The 59 open differential rows block on 36 `(address, mode)` pairs over 35
  addresses. The rowable game-code set is: `0x5D812` (real, 8 rows),
  `0x28D68` (allow, 6), `0x3E484` (allow, 3), `0x29D04` (stub/allow, 4),
  `0x3E4C4` (allow, 1), `0x24568` (stub, 7), `0x24804` (stub, 6),
  `0x40CB0` (stub, 8), `0x2CB74` (stub, 2), `0x4F5C8` (stub, 1) and
  `0x61A70` (stub, 1). `0x3D0BC` is not a function (its bytes are
  `00 00 00 00`); the real slot+0x10 hooks are `0x3A6D4` and `0x3A588`, and
  the decomp index's real entry at that address range is `0x3D0C0`, which
  already has a row.
- U14 (`gp-u14-win-live`) has no capture; U16's finisher and U13's post-4248
  pixels are the other gameplay named gaps.

## Goals

1. **U13 pixel truth.** Classify capture frames 4248, 4320 and 5120 (the
   in-replay unexplained set) as a real port divergence or a measured
   comparison-model artefact. Fix a real divergence from the raw bytes, or add
   a byte-pinned, mutation-tested allowance. Move `GP_U13_MIN_FIRST` to the
   next genuine unexplained frame (the post-script 5891 or later) and keep the
   trace/WIN pins at 10014. Record the cause and the failing-direction proof.
2. **Differential last mile.** Add rows for `0x5D812`, `0x28D68`, `0x3E484`,
   `0x29D04`, `0x3E4C4`, `0x24568`, `0x24804`, `0x40CB0`, `0x2CB74`,
   `0x4F5C8`, `0x61A70` and `0x3A6D4`; correct the `fighter_state_3531c`
   spec's synthetic `0x3D0BC` case to the real `0x3A6D4` hook. Measured target:
   `802/833` rows with callees closed, `795/833` full, 31 open (from
   `774/768/59`), with 12 new functions and their mutants; re-measure and pin
   the exact output rather than fitting it.
3. **Counter truth.** Replace the printed `UNROWED` constant with a measured
   figure that keeps the portable 8 but reports the true `FN_` ported-unrowed
   population (42 at the time of writing), and add a test that a newly ported
   `FN_` function cannot silently escape both the row set and `UNROWED`.
   Refresh the stale counter/status text in `README.md`, `docs/PROGRESS.md`,
   `AGENTS.md`, the evidence-closeout spec and the Makefile's U14 comment.

## Non-goals

- No DOSBox-X capture sessions. U14, U16's finisher, the other five
  characters' endings, the U12/U15/U16 tails and U17's tick-keyed replay are
  not in this phase.
- No new portable code and no new `fn_register`/column-0 headers. The two
  runtime rows (`0x5D812`, `0x61A70`) are differential bindings, not ported
  functions; `port_progress.py` must stay `799 1203 66` / `759 759 100`.
- No closure-rule change, no closing a row on a stub/real callee, no weakening
  a row to make the counter move. The runtime/host-owned blockers
  (`0x65431`, the AIL wrappers, the DPMI allocator, `0x65546`, `0x6C4CF`) stay
  named as remaining.
- No `0x4FBBB`/`0x4FC05`/`0x4FCFD`/`0x4FDF5` host-hardware rows; their
  `E.Machine.in` answers would be host policy, not game code.
- No audio audibility, packaging/CI, SDL pad hot-plug, language reload,
  `0x2EA68` exit-path decision or other host/product work.
- No porting the 231 E2 span writers or the 53 host-owned/deferred functions.

## Constraints

- **Evidence discipline:** every value is derived from raw bytes or a capture
  and cites its address. No fitted constants. A value that cannot be pinned is
  a named gap.
- **Raw wins:** on any plan/prose-vs-raw conflict, the raw bytes win and the
  correction is recorded with its address.
- **No data writes:** `data/` is read-only; the U13 capture is consumed
  read-only. `--check`, drivers and `run_tests` keep the all-zero `CMOS`
  condition.
- **The byte-exact oracle lines and every existing pin must not move**, except
  U13's `MIN_FIRST` (and `MIN_REACH` if the port frame alignment changes) when
  the first genuine unexplained frame moves. Trace/WIN pins stay 10014.
- **Differential rows:** every row opens with the existing seam; every new
  binding and mutant stays inside `mem[]`; the harness self-check and the
  `tools.tests.test_diff_verify` clobber/seam pins pass.
- **Commit only if the operator authorizes execution.** Stage named files;
  never `git add -A`. The plan's commit steps are conditional on that
  authorization.
- **The live Ghidra `primal-rage` project was not running during planning.**
  The addresses in this design were checked against the raw EXE bytes and
  `port/decomp/`; the implementer re-checks each with the available RE tooling
  before writing a row or a fix.

## The two workstreams

### A. U13 pixel truth (no capture)

The report is the entry point. `GP_WIN_KEEP=1 make gp-u13-oracle` keeps
`/tmp/pr_gp_dump/gp-u13-ending2`; `tools/gp_compare.py --report
--max-unexplained 0` prints the full unexplained list. Capture 4248 maps to
raw 7887 via `window.txt`; the nearest S record gives the game frame and mode.
The classifier tries clean, a byte splice of adjacent port frames and one
transition row; 4248 is none of them.

The investigation has two honest outcomes:

- **A real divergence.** The U10 4113 case was the original's dword bank add
  in the RLE span writers versus the port's byte-wise add. If U13's 4248 is a
  renderer defect, the failing-first test goes in the renderer's unit area
  (`port/tests/test_video.c` or the `render_list_blit` cases in
  `port/tests/test_game.c`), the fix goes in the writer that owns the stream,
  and the differential row for that writer (if it changes) is updated.
- **A measured artefact.** If the frame is a scan-out/loader/splice composition
  the two-frame model cannot express, the allowance is a named per-frame
  recipe in `tools/gp_compare.py` (`SCAN3_ALLOWED`/`OVERLAY_ALLOWED`/
  `BANDS_ALLOWED` discipline): a pinned top-row hash plus a unique adjacent
  port pair, or a glyph-overlay recipe. A `tools/tests/test_gp_compare.py`
  test pins the exact tuple and shows that changing a hash/index un-explains
  the fixture. No class extension, no fitted bytes.

After 4248, the same treatment applies to 4320 and 5120. 5891..5903 are
post-script and stay outside the frame claim; they are recorded, not forced.

### B. Differential last mile (no capture)

The rows are grouped by the port code they exercise, so each task's reviewer
can accept or reject one coherent unit:

| Group | Rows | Expected closed |
|---|---|---:|
| Frontend stub pair | `0x5D812` (binding only), `0x28D68` | +8 |
| Fighter callbacks | `0x3E484`, `0x29D04`, `0x3E4C4` | +7 |
| Actor entrances | `0x24568`, `0x24804`, `0x40CB0` | +8 |
| Menu/loop/memset | `0x2CB74`, `0x4F5C8`, `0x61A70` | +4 |
| Spec correction | `0x3D0BC` case → real `0x3A6D4` hook + row | +1 |
| **Total** | **12 rows** | **+28** |

The expected counter is `802/833` closed, `795/833` full, 31 open. The
`0x5D812` and `0x61A70` rows are bindings: `0x5D812` is a no-op (`xor
eax,eax; ret`) the port models inline, and `0x61A70` is the runtime `memset`
the port serves with `mem_fill`; neither gets a `port/src` header or
`fn_register`, so `port_progress.py` and the `fn_resolve` miss sets do not
move.

`0x3D0BC` is the one raw-wins correction. Its bytes are `00 00 00 00`; the
real entry at that neighbourhood is `0x3D0C0`, which already has a row. The
slot+0x10 hooks the `0x3A79C`/`0x3A650` setters store are `0x3A6D4`
(`0x3A7D2`) and `0x3A588` (`0x3A686`); the spec's `d15` case must point at
the real hook, and a row for `0x3A6D4` closes `fighter_state_3531c`. The
correction is recorded in the derivation record and the spec's comment.

The counter is re-measured after all rows; `tools/tests/test_diff_verify.py`'s
pinned string, `README.md`, `AGENTS.md` and `docs/PROGRESS.md` are updated to
the exact measured output. No existing oracle pin moves.

## Acceptance

1. **U13:** the `gp-u13-oracle` report explains 4248, 4320 and 5120 (fix or
   byte-pinned allowance); `GP_U13_MIN_FIRST` is the next genuine unexplained
   frame (or the post-script tail), `GP_U13_MIN_REACH` is re-measured,
   `GP_U13_TRACE_MIN_FIRST`/`GP_U13_WIN_MIN_FIRST` stay 10014, and the
   capture sha/frames stay unchanged. The record has the A/B/C classification,
   the failing-first test or the allowance recipe, and the failing-direction
   proof for the new pin.
2. **Differential:** the measured counter is at least `802/833` rows with
   callees closed and `795/833` full, 31 or fewer open; the 12 rows are
   VERIFIED with their mutants; `0x3D0BC` is corrected; every existing pin and
   oracle line is intact; the remaining 31 open rows are named by blocker.
3. **Counter truth:** the `UNROWED` figure is computed, not a hand-maintained
   constant, and the true `FN_` ported-unrowed population is reported; a new
   ported function cannot escape both the row set and `UNROWED` without a test
   failing.
4. **Docs:** the counters in the pinned test, `README.md`, `AGENTS.md` and
   `docs/PROGRESS.md` match the tool output; the evidence-closeout spec's
   status is no longer "approved"; the Makefile's U14 comment cites
   `0x37FF6`, not `0x37FF2`.
5. **Gate:** `make verify` and a full-scan `PR_DIFF_FULLSCAN=1 make verify`
   pass, including `symbols.h` idempotence and `make entry-triage`. No capture
   is created, no `data/` file changes, and `port_progress.py` is unchanged.

## Risks and mitigations

| Risk | Mitigation |
|---|---|
| U13 4248 is a real renderer defect that also affects other oracles | Failing-first test from the exact capture bytes; fix the owning writer; run the full gate; re-measure all pixel oracles, not just U13 |
| U13 4248 is a new comparison class and an allowance becomes a fitted constant | Require a byte-pinned recipe and a mutation test; if no recipe binds, keep it a named gap and stop at 4248 |
| A differential row closes on a stub/real callee | The closure rule is unchanged; the per-row self-check and the full counter must show the callee VERIFIED or excused |
| The `0x5D812`/`0x61A70` bindings write outside `mem[]` | Bound every binding and mutant; the harness's outside-image checks and `PR_DIFF_FULLSCAN=1` |
| The `0x3A6D4` row changes an existing row's verdict | Run `make diff-verify` and the harness unittest after the correction; the full counter is re-measured |
| The counter moves without a real row | The pinned string is updated only from the measured `diffrun` output; the row count, mutant count and open-row census are recorded |

## Open questions (decided at execution, not now)

- Is 4248 a real divergence or a comparison artefact? The bytes decide; both
  outcomes satisfy the phase, but the claim differs.
- Do 4320/5120 share 4248's cause? Re-use the fix/allowance only if the bytes
  match; otherwise separate.
- Is the true `FN_` ported-unrowed count exactly 42 after the 12 rows? Measure;
  the plan's 42 is the pre-phase figure.

## Deliberately left alone

- U14's live win, U16's finisher and the other endings (capture-gated).
- The runtime/host-owned differential blockers and the closure rule.
- The 231 span writers and the 53 host-owned/deferred functions.
- Audio audibility, CI/packaging, pad hot-plug, language reload, the
  `0x2EA68` exit path and the other host/product work.
