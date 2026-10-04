# Reverse completion C1: the callee rows (record)

**Scope.** Track P's verification-only batch C1 (roadmap row C1 of record `2026-10-02-reverse-p1-derivations.md`
§P1.3, as P3's §P3.10 left it): add a differential-verification row for each ported callee the P1-P3 rows stub,
so the dependent rows close. No ported function, no `fn_register`, no E2 move. Plan:
`2026-10-03-reverse-c1-callee-rows.md`. Recipe: E3 record §E3.10; lessons: the P-track review checklist
(`.superpowers/sdd/2026-10-01-plans/p-track-review-checklist.md`).

**Status of the numbers.** Measured by the planner on 2026-10-03 on `reverse-c1` at the base `main` `f5b5556`
(P3 merged), in this worktree: the baseline (`make diff-verify` and `PR_ORACLE_REQUIRED=1 ./build/run_tests`) and
a full prototype (all code applied, every gate run, then reverted). The image is
`build/diffrun --exe data/game/C/PRAGE.EXE --image-out FILE`, sha1 `ff3b8cb14e00f1c282de7b7e15dcd7c230766947`
(the E2/E3/P1/P2/P3 image). Every address below is capstone 5.0.7 over that image (fixups applied); `unicorn`
2.1.4 runs the original side. Ghidra was not consulted.

---

## §C1.1 The unverified set, enumerated from the final table

The brief's evidence list (P3 §P3.10, P2 §P2.10, P1 §P1.10-§P1.12, E3 §E3.10) was checked against the
final table of the base (`make diff-verify`, the callee column): the distinct callees marked `unverified`
are exactly **26** addresses, matching the brief's count. One correction to the brief's wording, raw over
text: `0x3C4CC` is **not** in the set (it has E3's own row `hit_anim_start_b`, VERIFIED), and E3's four are
`0x2BC30 0x2C3FC 0x2AE14 0x3C480` (the four unverified seams; E3 §E3.10's "five seamed" includes the
verified `0x3C4CC`).

| # | addr | family | the base rows that stub it | outcome |
|---|---|---|---|---|
| 1 | `0x188AC` | P1 | `4844c` (11 rows) | **row added** |
| 2 | `0x188DC` | P3 | `4844c`, `480b4`, `48170` (13) | **row added** |
| 3 | `0x18AF8` | P2 | `211f0`, `22588`, `47d24`, `480b4` (13) | **row added** |
| 4 | `0x18BD4` | P2/E3 | `47648`, `477a8`, `47cb0`, `48054` (allow, 18) | **row added** |
| 5 | `0x18C14` | P2 | the four +0x18 hooks (18) | **row added** |
| 6 | `0x2A17C` | P1 | `1579c`, `4844c` (6) | **row added** |
| 7 | `0x2AE14` | E3 | `anim_10fa8`, `38034`, `23b68`, `401d4`, `14ff8`, `150ac`, `4844c` (17) | deferred (C1b) |
| 8 | `0x2BC30` | E3 | nearly every P1-P3 row (78) | **row added** |
| 9 | `0x2C3FC` | E3 | nearly every P1-P3 row (82) | deferred (C1b) |
| 10 | `0x34D8C` | P2 | `211f0`, `22588`, `47d24` (16) | **row added** |
| 11 | `0x35838` | P3 | `48964`, `489a0` (10) | **row added** |
| 12 | `0x36870` | P2 | `47d24`, `4844c`, `22638` (17) | deferred (C1b) |
| 13 | `0x36D98` | P3 | `48170`, `39834`, `480b4` (5) | **row added** |
| 14 | `0x39834` | P2 | `47d24`, `480b4`, `211f0` (15) | deferred (C1b) |
| 15 | `0x39A10` | P2 | `47d24`, `480b4`, `48170`, `4844c` (22) | **row added** |
| 16 | `0x39FB0` | P3 | `47688` (5) | **row added** |
| 17 | `0x3A95C` | P3 | `47688` (5) | **row added** |
| 18 | `0x3B298` | P3 | `47688`, `480b4`, `18c14` (9) | deferred (C1b) |
| 19 | `0x3B714` | P3 | `477e8` (3) | deferred (C1b) |
| 20 | `0x3C148` | P3 | `48170`, `4844c` (13) | **row added** |
| 21 | `0x3C16C` | P3 | `4844c` (8) | **row added** |
| 22 | `0x3C190` | P3 | `47874`, `48608` (9) | **row added** |
| 23 | `0x3C208` | P2 | `47d24`, `480b4`, `48170` (20) | **row added** |
| 24 | `0x3C358` | P2 | `47d24` (13) | **row added** |
| 25 | `0x3C480` | E3 | `47d24`, `4844c` (21) | **row added** |
| 26 | `0x468D8` | P3 | `48170` (5) | **row added** |

**20 rows were prototyped, measured and reverted; 6 are deferred to C1b** (§C1.5). The deferred six keep
**33 base rows** open (the base rows whose callee set still holds one of them), so the base's 53 open rows do
not all close in C1; the count is measured, not predicted (§C1.4).

## §C1.2 The rows added (20): cases, mutants, seams

Each row is a `Spec` in `tools/diff_verify.py` (`C1_SPECS`), a binding `b_*` and mutants `m_*` in
`port/tests/diff_runner.c` (`k_bindings`), and the exact-set extensions in
`tools/tests/test_diff_verify.py` (`C1_MASKS`, `C1_KINDS`, the case-set table, the clobber table, the counter
line). Cases carry sentinels on every field a row writes and on every neighbour byte (the checklist items 1-2);
each row has at least one mutant caught only by the call list or the memory at a call where it has a callee
(item 7). The measured rows (the prototype's final table, `--self-check`):

| row | cases | blocks | mutants (caught) | callees (each by its own check) |
|---|---|---|---|---|
| `fighter_3c148` | 2 | 1/1 | 3/3 | - |
| `fighter_3c16c` | 2 | 1/1 | 3/3 | - |
| `fighter_39a10` | 2 | 1/1 | 2/2 | - |
| `fighter_36d98` | 2 | 1/1 | 3/3 | - |
| `fighter_18bd4` | 2 | 1/1 | 3/3 | - |
| `fighter_34d8c` | 3 | 3/3 | 2/2 | - |
| `fighter_3c358` | 2 | 1/1 | 3/3 | `33950` allow |
| `fighter_3c190` | 3 | 4/4 | 3/3 | `1A570` stub |
| `fighter_3c480` | 2 | 1/1 | 3/3 | `188AC`, `188DC`, `2BC30` stub; `339AC` allow |
| `fighter_188ac` | 2 | 1/1 | 3/3 | `186D0` stub |
| `fighter_188dc` | 2 | 1/1 | 4/4 | `18714` stub |
| `fighter_18af8` | 2 | 6/6 | 3/3 | `18714`, `18B04` stub; `33950` allow |
| `fighter_2a17c` | 3 | 9/10 (0x2A1F5 named dead) | 4/4 | `33754`, `33864` stub |
| `fighter_2bc30` | 4 | 9/10 (0x2BC6D named dead) | 3/3 | `2A408`, `2B2A0` stub |
| `fighter_39fb0` | 2 | 1/1 | 3/3 | `18B04`, `39F40` stub; `33A10` allow |
| `fighter_3a95c` | 2 | 1/1 | 3/3 | `188AC`, `2BC30` stub; `33A10` allow |
| `fighter_35838` | 6 | 11/11 | 3/3 | `2BC30` stub; `36638` stub |
| `fighter_468d8` | 7 | 8/8 | 3/3 | `33A10` allow |
| `fighter_3C208` | 8 | 18/18 | 4/4 | `186D0`, `18AF8` real, `187FC`, `1883C`, `3B8D8`, `3B90C` stub; `188DC`, `1A570` stub; `33950` allow |
| `fighter_18C14` | 64 | 97/97 | 3/3 | `1DDF4`, `18B44`, `189FC`, `18A4C`, `39EFC`, `3B298` stub; `33950` allow |

**New seams added to `port/src`** (the E3 recipe: a ported function a verified row stubs opens with
`PR_SEAM`/`PR_SEAM_RET`; `PR_SEAM_RET0` was added to `mem.h` for a no-argument returning callee, the
`PR_SEAM0` shape with a value). Each is a first statement; `E.callee_clobbers` re-derives every declared
clobber (`test_each_stub_declares_the_registers_its_callee_clobbers` extended):

| callee | port function | seam | clobbers (image) |
|---|---|---|---|
| `0x186D0` | `fighter_slot_latch` | `PR_SEAM(0x186D0u, side)` | () |
| `0x18714` | `hit_record_x` | `PR_SEAM_RET(0x18714u, side)` | () |
| `0x187FC` | `ai_distance` | `PR_SEAM_RET0(0x187FCu)` | () |
| `0x1883C` | `fighter_1883c` | `PR_SEAM(0x1883Cu, side, a, b)` | ebx, edx |
| `0x189FC` | `fighter_189fc` | `PR_SEAM_RET(0x189FCu, side)` | () |
| `0x18A4C` | `fighter_18a4c` | `PR_SEAM_RET(0x18A4Cu, side)` | () |
| `0x18B04` | `hit_facing_flag` | `PR_SEAM(0x18B04u, side)` | () |
| `0x18B44` | `fighter_18b44` | `PR_SEAM(0x18B44u, slot)` | () |
| `0x1DDF4` | `hit_geometry` | `PR_SEAM_RET(0x1DDF4u, side, table, idx)` | ebx, edx |
| `0x2A408` | `anim_next_sprite_id` | `PR_SEAM_RET(0x2A408u, rec, pset)` | edx |
| `0x2B2A0` | `spawn_anim_opcode` | `PR_SEAM_RET(0x2B2A0u, rec, index, flag)` | ebx, edx |
| `0x33754` | `palette_acquire` | `PR_SEAM_RET(0x33754u, handle)` | () |
| `0x33864` | `palette_release` | `PR_SEAM(0x33864u, entry)` | () |
| `0x36638` | `fighter_state_36638` | `PR_SEAM_RET(0x36638u, slot, rec)` | edx |
| `0x39EFC` | `fighter_39efc` | `PR_SEAM_RET(0x39EFCu, side)` | () |
| `0x39F40` | `fighter_pose_start` | `PR_SEAM(0x39F40u, side, edx, ebx, ecx, frame)` | ebx, edx |
| `0x3B8D8` | `fighter_3b8d8` | `PR_SEAM_RET(0x3B8D8u, side, delta)` | edx |
| `0x3B90C` | `fighter_3b90c` | `PR_SEAM_RET(0x3B90Cu, side, delta)` | edx |

`palette_release`, `spawn_anim_opcode`, `fighter_pose_start`, `ai_distance`, `fighter_1883c`,
`fighter_3b8d8`, `fighter_18b44`, `fighter_189fc`, `fighter_18a4c` lost `static` (declared in `actors.h` /
`fighter.h`) so the mutants can call them; `fighter_18bd4` lost `static` for its binding. All are
source-only changes: no new `FN_` address, no `fn_register`, so the E2 table is byte-identical (§C1.4) and
`port_progress.py` stays `771 1203 64` / `731 731 100`.

**Raw over plan, four corrections recorded (each found by the prototype):**

1. **`0x18AF8`'s second 0x18B04 entry is a fall-through, not a call.** The raw is `xor eax,eax; call 0x18B04`
   (0x18AFA) then `mov eax,1` (0x18AFF) falling into 0x18B04's body. The port's `fighter_18af8` called the
   seamed `hit_facing_flag(1u)`, so the port recorded a second call the raw never makes (the row would
   MISMATCH at `call #1`). Corrected: `hit_facing_flag` is split into the seamed wrapper and a static
   `hit_facing_flag_body`; `fighter_18af8` calls `hit_facing_flag(0u)` then `hit_facing_flag_body(1u)`.
   `0x3C208` runs `0x18AF8` **real** (`E.Call(0x18AF8, (), mode="real")`), the mode E3 defines for a callee
   proven by its own check.
2. **`0x18AF8` preserves EBX/ECX/EDX** (0x18B04 pushes and pops them; P2 §P2.8 predicted this). Its stub in
   `0x3C208`'s row must declare **no clobbers**, or the original's `test edx,edx` at 0x3C24D reads the
   poison: the row went 12/18 with the P2 over-declaration and 18/18 with the image-derived `()`. The
   clobber table's `0x18AF8` entry is the image's.
3. **`0x39A10`'s `rec+0x51` zero-extension is unobservable in-image.** Side 0x80 reads the slot table at
   0x10C1B0 (outside the image, zero) and writes at 0x10C224; `run_original` reports the outside write and
   the suite forbids outside accesses, so the case and the `@sext` mutant were dropped. Named limit (§C1.5).
4. **`0x18C14`'s flag 7 reads ctx[3] (the other slot's) `+0x62`**, not ctx[2]'s; the first case set poked
   the own slot and left block 0x18DE3 unhit. Corrected in the generator. The raw also rewrites the flag
   byte (flags[0] = 4/3) in the EDX buffer, which is mem[]: the binding copies the local flags back to
   `mem[EDX]` after the call.

Two dead blocks are named, not covered: `0x2A17C`'s 0x2A1F5 (`EBX == 0` already returned at 0x2A1AC, so
0x2A1E6's test cannot be reached with EBX zero) and `0x2BC30`'s 0x2BC6D (`xor eax,eax` at 0x2BC61 makes
0x2BC66's `je` always taken; the `fild` block is dead in the image).

## §C1.3 The mutants

62 mutants, all detected (`--self-check`); what alone catches each is pinned by
`test_each_c1_mutant_is_caught_by_what_it_breaks` (the `C1_KINDS` loop) and its measured case-set table.
The kinds (measured):

```
fighter_3c148@mutant byte            fighter_188ac@mutant byte call#0 memory
fighter_3c148@side   byte            fighter_188ac@side   byte call#0 call#0 memory
fighter_3c148@width  byte            fighter_188ac@latch  call#0
fighter_3c16c@mutant byte            fighter_188dc@mutant byte
fighter_3c16c@side   byte            fighter_188dc@side   byte call#0 call#0 memory
fighter_3c16c@width  byte            fighter_188dc@arg    call#0
fighter_39a10@mutant byte            fighter_188dc@eax    byte call#0
fighter_39a10@side   byte            fighter_18af8@mutant byte call#1 call#1 memory
fighter_36d98@mutant byte            fighter_18af8@once   byte call#1
fighter_36d98@side   byte            fighter_18af8@le     byte call#1 memory
fighter_36d98@and    byte            fighter_2a17c@mutant call#0 call#1
fighter_18bd4@mutant byte            fighter_2a17c@order  call#0 call#1 call#1 memory
fighter_18bd4@val    byte            fighter_2a17c@arg    call#0
fighter_18bd4@off    byte            fighter_2a17c@early  call#0 memory call#1 memory
fighter_34d8c@mutant byte            fighter_2bc30@mutant call#0 call#1
fighter_34d8c@side   byte            fighter_2bc30@order  call#0 memory call#1 memory
fighter_3c358@mutant byte            fighter_2bc30@frame  byte call#0..#2 memory
fighter_3c358@side   byte            fighter_39fb0@mutant call#1
fighter_3c358@42     byte            fighter_39fb0@side   call#0
fighter_3c190@mutant byte call#0     fighter_39fb0@order  call#0 call#1
fighter_3c190@arg    call#0          fighter_3a95c@mutant call#0
fighter_3c190@width  byte            fighter_3a95c@side   call#0
fighter_3c480@mutant call#2          fighter_3a95c@arg    call#1
fighter_3c480@order  call#0 call#1   fighter_35838@mutant call#0
fighter_3c480@side   call#0 call#2   fighter_35838@side   call#0
fighter_468d8@mutant eax             fighter_35838@order  call#0 memory
fighter_468d8@eq     eax             fighter_3c208@mutant call#6 call#7 call#9
fighter_468d8@side   eax             fighter_3c208@abs    call#4..#9
fighter_18c14@mutant eax             fighter_3c208@arg    call#6
fighter_18c14@store  byte            fighter_3c208@early  call#3..#9 memory
fighter_18c14@live   byte call#0 call#1 eax
```

**Named limits (mutations no case can observe).** `0x18C14`'s comparisons are covered by the 64 cases
(every flag, value 0/1, condition holding and not); its three mutants pin the return, the 0x8A store and the
flag-1 boundary, not each comparison — a per-comparison mutant set is a C1b/closeout item. `0x3C148`/
`0x3C16C`'s field order is not distinguishable (independent stores). `0x39A10`'s zero-extension (§C1.2 item 3).
`0x468D8`'s `& 0x7FFFFFFF` is pinned by h2 (0x80000000) and `@mutant` (mask dropped); the second return's
polarity by `@eq`.

## §C1.4 Counters, measured

| state | diff-verify counter | E2 |
|---|---|---|
| base `f5b5556` | `78/78 functions VERIFIED; 158/158 mutants detected; 1 named gaps; 11/64 rows with callees closed (14 have none)` | `288 unported, 207 ported; supplement 131 (9 unported, 0 stale); untrusted 30`; voice `28 / 87 / 19` |
| C1 prototype (20 rows) | `98/98 functions VERIFIED; 219/219 mutants detected; 1 named gaps; 34/78 rows with callees closed (20 have none)` | byte-identical |

The arithmetic: +20 functions, +61 mutants (the first full run showed 220; the `0x39A10@sext` mutant was
dropped in fix round 1, §C1.2 item 3), `with_callees` 64 -> 78 (the 20 new rows minus the 6 with no
callees), no-callee 14 -> 20 (+6), closed 11 -> 34. Of the 34 closed, 3 are new rows whose callees are all
verified (`3c358`, `3c190`, `3c480`); the base rows that close are those whose callee set is a subset of
{the 26 with rows, `33950`, `339AC`, `3C4CC`, `48170`}. The six deferred rows keep 33 base rows open. If
C1b adds them, the base's 64 rows all close and the counter reads `64 + 3` closed over `78 + 6` with callees.

`make entry-triage` is byte-identical (no ported function, no `fn_register`); `PR_ORACLE_REQUIRED=1
./build/run_tests` prints `all checks passed`; `python3 -m unittest tools.tests.test_diff_verify` runs 98
tests OK; `python3 tools/port_progress.py` stays `771 1203 64` / `731 731 100` (none of the 20 is a Ghidra
`FN_` function). The full `make verify` was not run (the brief's task-scoped gates only); the oracle lines
cannot move (no rendering, timing or RNG path changed; `port/src` changes are seams, one wrapper split and
`static` removals).

## §C1.5 The six deferred rows (C1b) and their evidence

The six remaining callees are ported and verifiable in principle; the prototype stopped after 20 rows and
did not build their cases. Each needs seams on its direct callees (all ported; the count from the raw's
`call`/`jmp` scan), then cases to cover its blocks:

| callee | blocks | direct callees needing a seam | notes |
|---|---|---|---|
| `0x39834` | 16 | `3AFC4` allow (4-function tree, no seamed member), `39738`, `392A0`, `36CE4`, `4F434` | `33A10` allow; `468D8`, `36D98`, `2C3FC` already seamed |
| `0x3B298` | 28 | `3B134`, `1AB5C`, `46460`, `1A734` | `33A10`, `3AFC4` allow |
| `0x2C3FC` | 100 | `1CA14`, `1CA6C`, `1CC28`, `1CD9C`, `1CE04`, `1CE70`, `1D238`, `1D244` (flow.c; two are no-arg returning: `PR_SEAM_RET0`) | the 0x2C3E0 jump table is the bounded form `switch_cases` follows; a voice-id sweep 0..0x100 is the case menu |
| `0x2AE14` | 33 | `2AC80`, `2A820`, `2A620`, `1C390`, `1C3A0` | `2B2A0`, `33754`, `2A408` already seamed; one indirect call at 0x2B0E9 (the animation-opcode target, resolved per case) |
| `0x36870` | 31 | `385B0`, `39280`, `164E8`, `39040`, `379C4`, `3C520`, `365C8`, `37D18`, `36BC8` | `2BC30`, `36638` already seamed |
| `0x3B714` | 23 | `3C59C`, `62003`, `39EFC`, `3B080`, `3AE9C`, `2BD44`, `3AAFC`, `3B6C4`, `3AD98` | `33A10`, `3AFC4` allow; `3B298` is C1b |

Their dependent rows stay open with this reason: "the callee has no row yet; C1b owns it" (33 base rows:
`fighter_23130`, the finishers `1567c/15908/23ec0/15584/1579c/23d38`, `237d0`, `2381c`, `3dadc`, `3db34`,
`3d10c`, `14ef8`, `14f50`, `211f0`, `22588`, `22638`, `47688`, `47874`, `47798`, `477e8`, `47d24`, `480b4`,
`4811c`, `4844c`, `anim_10fa8`, `38034`, `23b68`, `401d4`, `23868`, `14fa8`, `14ff8`, `150ac`, and their
mutant rows).

## §C1.6 Decisions, named gaps and limits

**No decision is left to the user.** Two choices were taken from the raw and recorded as corrections:
`0x18AF8`'s fall-through split (§C1.2 item 1) and `0x3C208`'s real call to it; both are source-fidelity
changes with no game-behaviour change (the seam is inert outside `build/diffrun`).

Named gaps and limits:
- **The six C1b rows** (§C1.5): deferred, with their seam lists; 33 base rows stay open.
- **`0x39A10`'s rec+0x51 zero-extension** (§C1.2 item 3): unobservable in-image; not claimed.
- **`0x18C14`'s per-comparison mutants**: the 64 cases cover the arms; three mutants pin the return, the
  store and the flag-1 boundary; the rest is a named limit.
- **`0x2A17C` and `0x2BC30` dead blocks** (§C1.2): named in `unhit_named`, not covered.
- **The clobber derivation over-approximates `0x18AF8`** (`E.callee_clobbers` reads (ebx, ecx, edx) where
  the real function preserves them): the P2 rows keep the over-declaration (safe); `0x3C208` runs it real.
  The exact-set test's entry is the image's.
- E3's, P1's and P2's limits stand: seeds are hand pokes; the memory at a call is mem[] only; the callee
  column is one level deep.

## §C1.7 What the planner ran

- The baseline on the clean `f5b5556` worktree: `make diff-verify` (2 min 36 s) -> the counter and table of
  §C1.4; `PR_ORACLE_REQUIRED=1 ./build/run_tests` -> all checks passed.
- The prototype, in this worktree, in six stages (the six leaf rows; `3c358/3c190/3c480`; the P1/P2 rows
  with few seams; `2bc30/39fb0/3a95c/35838/468d8`; `3c208`; `18c14`), each measured with
  `python3 tools/diff_verify.py --function NAME --self-check` and the full run at the end; the fix rounds
  of §C1.2; `python3 -m unittest tools.tests.test_diff_verify` (98 tests OK); `make diff-verify` (final
  counter of §C1.4); `make entry-triage` (byte-identical); `PR_ORACLE_REQUIRED=1 ./build/run_tests`
  (all checks passed). Then `git checkout -- port tools`, leaving only this record and the plan.
- The prototype's diff (1888 lines, the exact files the plan's Task 2 applies) is embedded in the plan.
