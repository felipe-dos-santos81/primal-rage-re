# C3d: the frontier rows, part 4 (track P, batch C3d) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add the differential-verification row for each of the ten type-family addresses this batch
measures — `0x1AB5C` (`fighter_input_mask`), `0x2A820` (`pset_write`), `0x36BC8`
(`fighter_state_36bc8`), `0x379C4` (`fighter_379c4`), `0x385B0` (`fighter_385b0`), `0x3AD98`
(`fighter_3ad98`), `0x3AE9C` (`fighter_3ae9c`), `0x3B080` (`fighter_3b080`), `0x3B134`
(`fight_command_map`) and `0x4F434` (`fighter_4f434`) — with their seams, bindings, mutants and
exact-set pins; **one raw-over-port correction** (`0x3AD98`'s `0x392A0` argument). The three
remaining type rows (`0x39040`, `0x392A0`, `0x3AAFC`) and the frontier tail the ten rows' callees
name are **C3e** (record §C3d.7), with the measured evidence: the tail is 23 addresses that surface
a further 11 unrowed callees, so it cannot close in this batch.

**Architecture:** Each row is a `Spec` in `tools/diff_verify.py` (`C3D_SPECS`), a `b_*` binding and
`m_*` mutants in `port/tests/diff_runner.c` (`k_bindings`), and exact-set assertions in
`tools/tests/test_diff_verify.py` (`C3D_MASKS`, `C3D_KINDS`, the case-set table, the clobber table,
the counter line). `port/src` changes: sixteen `PR_SEAM`/`PR_SEAM_RET`/`PR_SEAM_RET0` seams on the
ten rows' unrowed callees (`0x1A7CC`, `0x2A690`, `0x38BC8`, `0x37178`, `0x38D90`, `0x38FEC`,
`0x46190`, `0x36E78`, `0x3A0FC`, `0x36D20`, `0x3A504`/`0x3A650`/`0x3A79C`/`0x3A8E8`, `0x3BDB0`,
`0x3BDDC`), the two exports `fighter_38bb0`/`fighter_38bc8` and `fight_attack_ready` (the mutant
cores call them), and the `0x3AD98` correction (the `0x392A0` argument is the `anim[0]+4` byte, not
the sum, unless the `0x78` clamp ran).

**Tech Stack:** C11 (`port/src`, `port/tests`), Python 3.12 (`unittest`), `unicorn` 2.1.4 and
`capstone` 5.0.7 (`tools/requirements-diff.txt`), CMake, `make`.

**Spec:** `docs/superpowers/specs/2026-09-30-reverse-completion-design.md` §4 track P, §5.1-§5.3,
§6 ("each ported function has a verification row; unhit blocks and non-emulable instructions named;
counters and README percentage updated"), §7.

**Derivation record:** `docs/superpowers/plans/2026-10-05-reverse-c3d-derivations.md` (§C3d.1 the
members, §C3d.2 the rows, seams, fixtures and the `0x3AD98` correction, §C3d.3 the mutants and
their measured catching/kinds sets, §C3d.4 the counters, §C3d.5 the named gaps and limits, §C3d.6
what the planner ran, §C3d.7 the C3e deferral with its evidence). Recipe:
`2026-10-01-reverse-e3-derivations.md` §E3.10; checklist:
`.superpowers/sdd/2026-10-01-plans/p-track-review-checklist.md`.

**What the planner ran (scratch, 2026-10-07, on `reverse-c3d` at `main` `0bda4fa` = C3c merged;
image sha1 `ff3b8cb14e00f1c282de7b7e15dcd7c230766947`, the E2/E3/P1-P8/C1-C3c image):** the baseline
(Task 1) and a full prototype of the ten rows: the sixteen seams and the three exports first (the
`--self-check` still `251/251; 874/874; 178/195 (56)`), then the rows family by family. Every row
was measured with `--function NAME --self-check` until VERIFIED with every mutant detected, then
the full `python3 tools/diff_verify.py --self-check` (`261/261 functions VERIFIED; 945/945 mutants
detected; 1 named gaps; 181/205 rows with callees closed (56 have none)`), `python3 -m unittest
tools.tests.test_diff_verify` (107 tests with the extended exact-set tables), `make entry-triage`
(byte-identical: `targets 233 unported, 262 ported; supplement 131 (3 unported, 0 stale); untrusted
30`; voice `0 / 115 / 19`), `PR_ORACLE_REQUIRED=1 ./build/run_tests` (all checks passed) and
`python3 tools/port_progress.py` (`771 1203 64` / `731 731 100`). One raw-over-port correction was
found and recorded (`0x3AD98`, record §C3d.2 correction 1); the prototype was then reverted
(`git checkout -- port tools`); the patch below is the exact diff it applied.

**Re-baseline note.** The counters below are `0bda4fa`'s. If `main` moves before this plan executes,
Task 1 records the measured base and every later expected counter adds this plan's increments:
functions +10, mutants +71, rows with callees +10, closed rows +3, no-callee rows unchanged. The E2
table must not move (no E2 candidate; no `fn_register`): if a task regenerates it, the line must be
byte-identical.

## Decisions needed from the user

**None.** The two row-shape questions the C3c record left (the `0x3AD98` argument and whether a
ninth/heavy row could be folded into C3d) are resolved inside this plan: the `0x3AD98` correction is
a raw-over-port fix (§C3d.2), and the three unrrowed-heavy rows are C3e with the measured evidence
(§C3d.7). The one raw-over-port correction (`0x3AD98`: `0x3AE83` is reached with EDX unchanged at the
`anim[0]+4` byte; only the `0x3AE75` clamp path replaces it) is recorded with its disassembly and
executed-call evidence in the record §C3d.2.

## The C3d roadmap

| task | what | commit |
|---|---|---|
| 1 | baseline on `0bda4fa` (no commit) | - |
| 2 | the ten rows, the seams/exports, bindings, mutants and the test exact-set updates | `tools:` |
| 3 | the review sweep (stores, clobbers, mutant case sets) | `tests:` (only if a fix) |
| 4 | closure: PROGRESS, the record's §C3d.8, the final gates | `docs:` |

## Global Constraints

Copied verbatim from their sources:

- AGENTS.md: "**Never ship a fitted constant.** Every value is derived from the raw bytes or a
  capture, with the address that proves it. A value that cannot be pinned is a **named gap with its
  evidence** — never a plausible-looking number." / "**On any plan-vs-raw conflict the raw wins.**
  Record the correction and the address, in the derivation record and the report."
- Spec §8: "`make verify` is the gate after every task." User ruling (2026-10-01, speed-up): the
  per-task gate is the task's tests, `make diff-verify`, `make entry-triage` and the gp oracles
  whose miss sets the task touches; the full `make verify` runs at the baseline, the final task and
  before the merge. Here the task-scoped gates only (the brief): `make diff-verify`,
  `make entry-triage`, `PR_ORACLE_REQUIRED=1 ./build/run_tests`, and
  `python3 -m unittest tools.tests.test_diff_verify`.
- AGENTS.md: "**The E2 target list is a gate** ... A commit that ports a target (a new `/* 0xADDR`
  header or `fn_register`) regenerates the table in the same commit (decision D3)". This plan ports
  no target: the table must be byte-identical.
- AGENTS.md: "One C function per original function, header comment `/* 0xADDR — spec section */`." /
  "Mark deliberate deviations `/* PORT: ... */`; doubts `/* TODO(verify): ... */`. No other comment
  styles in `port/src`." / "Code addresses stored in data go through `fn_origin()` /
  `fn_resolve()`." / "**`port/src/symbols.h` is generated** ... Never hand-edit it; where the
  generator emits no name, use a local `#define` with the raw address."
- AGENTS.md: "A ported function that a differentially verified function stubs or records ... opens
  with `PR_SEAM(0xADDR, args...)` (void) or `PR_SEAM_RET(0xADDR, args...)` ... The arguments are the
  C signature's, in order (a pointer into `mem[]` as its offset)." Record E3 §E3.8: "A seam must be
  the callee's first statement".
- AGENTS.md: "Only `CHECK(cond,msg)` and `CHECK_EQ_INT(a,b)`." / "**Assertions must be able to
  fail.**" / "Consolidating must not change an assertion": this plan extends the exact-set
  assertions of `RealFunctionTests` (rows, masks, mutant names, stub clobbers, the counter line) and
  adds the C3d case-set test; no other assertion changes.
- AGENTS.md: "`data/` is git-ignored and read-only — never write to it." / "Commit style:
  `<area>: <what changed>`. **Never `git add -A`** — stage named files." No `Co-Authored-By`
  trailer. Never run `make gp-capture`.
- The C3d brief: "the gp miss sets must stay as Task 1 measures them" (this batch touches no
  gameplay path; the only behavioral `port/src` change is the `0x3AD98` correction, which no driver
  path reaches — the executor's gp gates are the evidence).
- Spec §5.3 / E3 §E3.4: "VERIFIED" is equivalence on the exercised blocks and inputs only, each
  function with its callees stubbed or run as stated.

## Where to run

The worktree `.worktrees/reverse-c3d` (branch `reverse-c3d`). Every command runs from the worktree
root; `data/` and `.superpowers/` are symlinks; `build/` exists. Before Task 1:

```bash
git rev-parse HEAD   # 0bda4fa
python3 -c "import unicorn, capstone; print(unicorn.__version__, capstone.__version__)"  # 2.1.4 5.0.7
cmake -S port -B build && cmake --build build 2>&1 | tail -1
./build/diffrun --exe data/game/C/PRAGE.EXE --image-out /tmp/pr_c3d_img.bin && shasum /tmp/pr_c3d_img.bin
ls port/tests/ghidra_data.bin   # the PR_ORACLE_REQUIRED fixture (git-ignored); copy it from the main repo if absent
```

The last-but-one line must print `ff3b8cb14e00f1c282de7b7e15dcd7c230766947`; another hash means the
image differs from the one the record measured: stop. Ledger:
`.superpowers/sdd/2026-10-05-reverse-c3d-frontier-rows-4/progress.md`.

### The ten rows (the brief's grouping, executed as families)

The patch is one `git diff` (Task 2 applies it at once); the families below are the gate order:

| family | rows | shared seams/fixtures |
|---|---|---|
| the facing/block rows | `0x1AB5C` `0x2A820` | the `0x1A7CC`/`0x2A690` seams; `0x1AB10`/`0x33A10` allows |
| the state rows | `0x36BC8` `0x379C4` `0x385B0` | the `0x38BC8`/`0x37178` seams; `0x38BB0`/`0x3AFC4` allows |
| the reaction rows | `0x3AE9C` `0x3B080` `0x3AD98` `0x3B134` `0x4F434` | the `0x46190`/`0x3A0FC`/`0x36D20`/four pose/`0x3BDB0`/`0x3BDDC` seams; the `0x1A570` real, the leaf allows; the `0x3AD98` correction |

## How the code steps are written

Task 2's patch is the prototype's exact `git diff` (the planner applied it, ran every gate, and
reverted). Apply it with `git apply`; it touches `port/src/game/actors.c`, `port/src/game/fighter.c`,
`port/src/game/fighter.h`, `port/src/game/fight.c`, `port/src/game/fight.h`, `port/tests/diff_runner.c`,
`tools/diff_verify.py` and `tools/tests/test_diff_verify.py`. If `main` moved, `git apply --3way`
and resolve by keeping the patch's additions at the same anchors; never merge the E2 table by hand.

## File Structure

| File | Responsibility |
|---|---|
| `port/src/game/fighter.c`/`.h` | the eleven seams on the 0x36xx/0x37xx/0x38xx/0x3Axx/0x46xx callees, the `fighter_38bb0`/`fighter_38bc8` exports and the `0x3AD98` correction |
| `port/src/game/actors.c` | the `0x2A690` seam on `actor_pset_point` |
| `port/src/game/fight.c`/`.h` | the `0x3BDB0` `PR_SEAM_RET` and the `fight_attack_ready` export |
| `port/tests/diff_runner.c` | the ten bindings and 71 mutants, the `C3D` cores |
| `tools/diff_verify.py` | `C3D_SPECS` (the ten rows and their fixtures) |
| `tools/tests/test_diff_verify.py` | `C3D_MASKS`, `C3D_KINDS`, the case-set test, the clobber and counter lines |
| `docs/superpowers/plans/2026-10-05-reverse-c3d-derivations.md` | the record (this plan's source of values) |
| `docs/PROGRESS.md` | Task 4's paragraph |

---

### Task 1: Baseline (no commit)

**Files:** none. **Interfaces:** consumes the worktree at `0bda4fa`; produces the baseline counters
and table.

- [ ] **Step 1: the task gates on the untouched tree.**

```bash
make diff-verify DIFF_IMAGE=/tmp/pr_c3d_base.bin DIFF_TABLE=/tmp/pr_c3d_base_table.md 2>&1 | tail -1
make entry-triage E2_IMAGE=/tmp/pr_c3d_base_e2.bin 2>&1 | tail -2
python3 tools/port_progress.py
```

Expected (measured at `0bda4fa`):

```
diff-verify: 251/251 functions VERIFIED; 874/874 mutants detected; 1 named gaps; 178/195 rows with callees closed (56 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
entry-triage: targets 233 unported, 262 ported; supplement 131 (3 unported, 0 stale); untrusted entries 30
entry-triage: voice sites outside Ghidra 134: 0 in unported code, 115 in ported code, 19 nowhere
771 1203 64
731 731 100 (portable: excludes 81 host-owned/deferred and runtime >= 5D000)
```

- [ ] **Step 2: pin the gp miss sets.** `PR_GP_DUMP` scenarios' pinned sets in `test_platform.c`
  must be the ones Task 2 leaves untouched. The `0x3AD98` correction is the batch's only behavior
  change; the function is reached only through `fighter_reaction` (0x3B714)/`fighter_winner_body`
  paths that no scenario reaches at this base, and Task 4's full `make verify` is the evidence.

### Task 2: the ten rows (one commit)

**Files:** the patch below. **Interfaces:** produces `C3D_SPECS`, the ten bindings and 71 mutants,
the sixteen seams, the three exports, the `0x3AD98` correction, and the test exact-set updates;
consumes the C1/C2/C2b/C3/C3b/C3c fixtures and the C3b/C3c scratch records.

- [ ] **Step 1: apply the patch.** `git apply - <<'PATCH' ... PATCH` with the diff embedded at the
  end of this task (the exact prototype diff; 1270 insertions over 8 files).

- [ ] **Step 2: build and run each row's self-check.**

```bash
cmake --build build 2>&1 | tail -1
for f in fighter_4f434 fighter_385b0 fighter_3ae9c fighter_3b080 fighter_state_36bc8 \
         fighter_379c4 fighter_3ad98 fighter_input_mask fight_command_map pset_write; do
  python3 tools/diff_verify.py --function "$f" --self-check
done
```

Expected rows (measured; the format is row, entry, cases, blocks, mutants, named unhit):

| row | entry | cases | blocks | mutants | named unhit |
|---|---|---|---|---|---|
| `fighter_4f434` | 0x4F434 | 9 | 10/10 | 6/6 | — |
| `fighter_385b0` | 0x385B0 | 5 | 8/8 | 6/6 | — |
| `fighter_3ae9c` | 0x3AE9C | 9 | 23/23 | 7/7 | — |
| `fighter_3b080` | 0x3B080 | 5 | 7/7 | 6/6 | — |
| `fighter_state_36bc8` | 0x36BC8 | 6 | 9/9 | 6/6 | — |
| `fighter_379c4` | 0x379C4 | 6 | 10/10 | 7/7 | — |
| `fighter_3ad98` | 0x3AD98 | 8 | 15/15 | 8/8 | — |
| `fighter_input_mask` | 0x1AB5C | 9 | 24/24 | 9/9 | — |
| `fight_command_map` | 0x3B134 | 12 | 19/19 | 9/9 | — |
| `pset_write` | 0x2A820 | 11 | 27/28 | 7/7 | 0x2A9CE: the child layer is an 8-bit add (0x2A9B5/0x2A9B8), so its 0xFF clamp cannot fire |

(The per-row run reports the callee column as `unverified` for every callee whose own row is not
run; only the full run's table reads `VERIFIED`. Record E3 §E3.8.)

- [ ] **Step 3: the task gates.**

```bash
make diff-verify DIFF_IMAGE=/tmp/pr_c3d_after.bin DIFF_TABLE=/tmp/pr_c3d_after_table.md 2>&1 | tail -1
python3 -m unittest tools.tests.test_diff_verify 2>&1 | tail -3
make entry-triage E2_IMAGE=/tmp/pr_c3d_after_e2.bin 2>&1 | tail -2
PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -1
python3 tools/gen_symbols.py port/decomp /tmp/pr_c3d_sym.h && diff /tmp/pr_c3d_sym.h port/src/symbols.h
python3 tools/port_progress.py
```

Expected: `261/261 functions VERIFIED; 945/945 mutants detected; 1 named gaps; 181/205 rows with
callees closed (56 have none)`; `OK` (107 tests, the extended exact sets); E2 byte-identical;
`all checks passed`; `symbols.h` regeneration byte-identical; `771 1203 64` / `731 731 100`.

- [ ] **Step 4: commit.**

```bash
git add port/src/game/actors.c port/src/game/fighter.c port/src/game/fighter.h \
        port/src/game/fight.c port/src/game/fight.h port/tests/diff_runner.c \
        tools/diff_verify.py tools/tests/test_diff_verify.py
git commit -m "tools: C3d: the ten frontier rows, their seams and mutants"
```

### Task 3: the review sweep (one commit, only if a fix)

**Files:** the patch's rows. **Interfaces:** consumes Task 2's state; produces the store sweep's
findings.

- [ ] **Step 1: the store sweep.** For every C3d row, poke each field the row writes to a value that
  differs from what it writes and re-run `--function NAME --self-check`; a store with no sentinel
  fails some mutant. The planner's sweep is baked into the fixtures (the slot/record sentinel
  fields, the `0x4F434` counter and cap, the `0x3AD98` anim structs, the `0x3B134` anim byte,
  the `pset_write` pool/parent neighbours); re-check the fields the record lists as the rows' only
  writes: `slot+0x2B/+0x40/+0x41/+0x43/+0x49/+0x52..0x55/+0x5D/+0x5F/+0x63/+0x65/+0x67/+0x68/`
  `+0x74/+0x7A/+0x84/+0x8A`, `rec+0x18/+0x1C/+0x28/+0x2B/+0x34/+0x36/+0x3C/+0x42..0x4D`, the
  `0x1088E0` word, the `0x105BDC/BE0` pair, the pset bytes and `0x1078FC`.
- [ ] **Step 2: the clobber and case-set re-check.** `python3 -m unittest
  tools.tests.test_diff_verify.RealFunctionTests` (the clobber re-derivation and the C3d case-set
  test). Expected: OK.
- [ ] **Step 3: commit any fix.**

```bash
git add <the fixed files>
git commit -m "tests: C3d review sweep: the store sentinels and the case-set pins"
```

### Task 4: Closure (one commit)

**Files:** `docs/PROGRESS.md`, the record. **Interfaces:** consumes Task 2's measured state.

- [ ] **Step 1: the final gates.**

```bash
make diff-verify DIFF_IMAGE=/tmp/pr_c3d_close.bin 2>&1 | tail -1
make entry-triage E2_IMAGE=/tmp/pr_c3d_close_e2.bin 2>&1 | tail -2
PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -1
python3 tools/port_progress.py
make verify
```

Expected: the Task 2 counter; E2 byte-identical; `all checks passed`; `771 1203 64` / `731 731
100`; and the full `make verify` exit 0 with the 45 oracle lines equal to
`.superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt`, the `make audio-render` WAV
equal to the pre-change WAV, every gp ratchet line `ok` with measured == pin (Task 1's list
verbatim), and `symbols.h` byte-identical. The `port/src` changes are the seams/exports and the
`0x3AD98` correction; the full ladder is what proves no oracle-visible path moved.

- [ ] **Step 2: append the PROGRESS paragraph** (the ten rows, the counter, the `0x3AD98`
  correction, the C3d → C3e deferral of record §C3d.7).

- [ ] **Step 3: append §C3d.8 Results to the record** (the executed tree's counters, the commit
  shas, the gate log lines), mirroring C3c's §C3c.8.

- [ ] **Step 4: commit.**

```bash
git add docs/PROGRESS.md docs/superpowers/plans/2026-10-05-reverse-c3d-derivations.md
git commit -m "docs: C3d closure: the ten frontier rows measured and the C3e deferral"
```

### The patch

```diff
diff --git a/port/src/game/actors.c b/port/src/game/actors.c
index 6f8bb8d..152c4d3 100644
--- a/port/src/game/actors.c
+++ b/port/src/game/actors.c
@@ -2694,6 +2694,7 @@ void mode1_cursor(u32 rec, u32 pset)
  * into rec+0x3C. Exported for the game_frame tail's per-fighter sync (0x25443). */
 void actor_pset_point(u32 rec)
 {
+    PR_SEAM(0x2A690u, rec);
     u32 pset = actor_pset(rec);
     u32 x;
     if ((DSW(rec + 0x28) >> 8 & 0x10u) == 0) {
diff --git a/port/src/game/fight.c b/port/src/game/fight.c
index 409e1a1..a9a6066 100644
--- a/port/src/game/fight.c
+++ b/port/src/game/fight.c
@@ -2021,8 +2021,9 @@ void fight_stance_pass(u32 side)
  * same-side context (0x3BDB8 0x33950), then AL = 1 when the slot's +0x53 == 0
  * (0x3BDC1) and +0x54 != 2 (0x3BDC7), else AL = 0. Only AL is defined; the
  * caller 0x3B27F tests `test al,al`. */
-static int fight_attack_ready(u32 side)
+int fight_attack_ready(u32 side)
 {
+    PR_SEAM_RET(0x3BDB0u, side);
     u32 ctx[6];
     fighter_ctx_same(ctx, side);                /* 0x3BDB8 0x33950 */
     u32 self = ctx[2];
diff --git a/port/src/game/fight.h b/port/src/game/fight.h
index f6096c3..a24b508 100644
--- a/port/src/game/fight.h
+++ b/port/src/game/fight.h
@@ -50,6 +50,10 @@ void fight_stance_pass(u32 side);
  * 0x3B298 calls it. */
 void fight_command_map(u32 side, u32 edx_arg, u32 override);
 
+/* 0x3BDB0 (C3d). EAX = side: the attack-readiness gate (slot +0x53 == 0 and
+ * +0x54 != 2); exported for the mapper's row, whose mutant cores call it. */
+int fight_attack_ready(u32 side);
+
 /* 0x49C78. The scene/effects pass: the list walk with its per-entry prelude
  * (the side and entry counts, and 0x4B69C, the trample, demo-pose record
  * §29), every type of the 0x49C2C jump table — 0/>0xE (0x4AAD0), 1..7, 8
diff --git a/port/src/game/fighter.c b/port/src/game/fighter.c
index c379a00..ce1d943 100644
--- a/port/src/game/fighter.c
+++ b/port/src/game/fighter.c
@@ -658,6 +658,7 @@ void fighter_block_anim(u32 slot, u32 rec)
 /* 0x1A7CC — record §38. */
 void fighter_block_start(u32 side)
 {
+    PR_SEAM(0x1A7CCu, side);
     u32 ctx[6], tri[3];
     fighter_ctx_swap(ctx, side);                        /* 0x1A7D5 0x33A10 */
     DSB(ctx[3] + 0x43u) &= 0xFDu;                       /* 0x1A7DE */
@@ -2258,6 +2259,7 @@ void fighter_wall_clamp(u32 side)
  * the other slot's +0x5D/+0x43 and the DS_001078FF character's palette. */
 static void fighter_36e78(u32 slot)
 {
+    PR_SEAM(0x36E78u, slot);
     u8 b = DSB(slot + 0x5Bu);                           /* 0x36E7B */
     if (b != 0u) {
         DSB(slot + 0x5Bu) = 0u;                         /* 0x36E82 */
@@ -2533,16 +2535,17 @@ int fighter_34e20(u32 reaction)
     return reaction < 0x18u;
 }
 
-/* 0x38BB0. Clear the 0x40-byte per-side table at 0x107A80 + side*0x40. */
-static void fighter_38bb0(u32 side)
+/* PORT: exported for the C3d mutant cores only (the seams stay the only observers). */
+void fighter_38bb0(u32 side)
 {
     u32 base = FIGHT_STUN_BASE + side * 0x40u;
     for (u32 i = 0; i < 0x40u; i++) DSB(base + i) = 0;      /* 0x38BBB */
 }
 
 /* 0x38BC8. 0x38BB0 plus DSW(0x107D24 + side*2) = 0. */
-static void fighter_38bc8(u32 side)
+void fighter_38bc8(u32 side)
 {
+    PR_SEAM(0x38BC8u, side);
     DSW(FIGHT_D24_BASE + side * 2u) = 0;                    /* 0x38BCC */
     fighter_38bb0(side);                                    /* 0x38BDA */
 }
@@ -3104,6 +3107,7 @@ void fighter_36870(u32 rec)
  * the far arm mirrors with S+0x57 = 1. EAX = slot. */
 void fighter_37178(u32 slot)
 {
+    PR_SEAM(0x37178u, slot);
     u32 rec = DSD(slot);                                    /* 0x37180 */
     u32 side = (u32)DSB(rec + 0x51u);                       /* 0x37188 */
     u32 other = 1u - side;                                  /* 0x37195 */
@@ -3231,6 +3235,7 @@ void fighter_38d24(u32 side)
  * 0x38E5C/0x38E89/0x38E8E feed no branch: both arms pass col EDI - 2. */
 void fighter_38d90(u32 side)
 {
+    PR_SEAM(0x38D90u, side);
     s32 c = (s32)side * 0x25;
     fighter_38c5c(side);                                        /* 0x38D98 */
     text_cursor_hold(-1, 6, game_string_get(0xe5u), 0x3000u);   /* 0x38DAC/0x38DB8 */
@@ -3290,6 +3295,7 @@ u8 fighter_38ed0(u32 side, u32 rec)
  * word at 0xBEBB8 + char*2. */
 void fighter_38fec(u32 side)
 {
+    PR_SEAM(0x38FECu, side);
     u32 ctx[6];
     fighter_ctx_same(ctx, side);                                /* 0x38FF7 0x33950 */
     u32 ch = DSB(ctx[2] + 0x7au);                               /* 0x39000 */
@@ -3627,6 +3633,7 @@ void fighter_state_3531c(u32 side)
  * rejects. */
 int fighter_attack_consume(u32 side)
 {
+    PR_SEAM_RET(0x3BDDCu, side);
     u32 ctx[6];
     fighter_ctx_same(ctx, side);                        /* 0x3BDEA */
     u32 self = ctx[2];                                  /* &slot[side] */
@@ -5376,6 +5383,7 @@ void fighter_4660c(u32 v)
  * free play. */
 static int fighter_46190(void)
 {
+    PR_SEAM_RET0(0x46190u);
     u32 r = config_field_get(0x29u);                        /* 0x46190/0x46195 */
     if ((r & 0x800u) == 0u) return 0;                       /* 0x4619A/0x4619F */
     return config_not_free_play() == 0u;                    /* 0x461A2/0x461AE */
@@ -5493,24 +5501,28 @@ static void fighter_pose_commit(u32 side, u32 edx, u32 callback,
  * pair. */
 static void fighter_pose_3a504(u32 side, u32 edx)
 {
+    PR_SEAM(0x3A504u, side, edx);
     fighter_pose_commit(side, edx, 0x0003A43Cu, DS_00107D14, DS_00107D10);
 }
 
 /* 0x3A650. The pose setter with the 0x3A588 callback and 0x107D0C/0x107D00. */
 static void fighter_pose_3a650(u32 side, u32 edx)
 {
+    PR_SEAM(0x3A650u, side, edx);
     fighter_pose_commit(side, edx, 0x0003A588u, DS_00107D0C, DS_00107D00);
 }
 
 /* 0x3A79C. The pose setter with the 0x3A6D4 callback and 0x107D08/0x107D04. */
 static void fighter_pose_3a79c(u32 side, u32 edx)
 {
+    PR_SEAM(0x3A79Cu, side, edx);
     fighter_pose_commit(side, edx, 0x0003A6D4u, DS_00107D08, DS_00107D04);
 }
 
 /* 0x3A8E8. The pose setter with the 0x3A820 callback and 0x107CF8/0x107CFC. */
 static void fighter_pose_3a8e8(u32 side, u32 edx)
 {
+    PR_SEAM(0x3A8E8u, side, edx);
     fighter_pose_commit(side, edx, 0x0003A820u, DS_00107CF8, DS_00107CFC);
 }
 
@@ -6107,6 +6119,7 @@ s32 fighter_39738(u32 side, s32 b)
  * stance restart; return slot+0x52. */
 static u32 fighter_36d20(u32 slot)
 {
+    PR_SEAM_RET(0x36D20u, slot);
     u32 rec = DSD(slot);                                    /* 0x36D24 */
     u32 side = (u32)DSB(rec + 0x51u);                       /* 0x36D26 */
     if (ai_pred_468d8(side) != 0) return 0u;                /* 0x36D2E/0x36D37 */
@@ -6139,6 +6152,7 @@ static void fighter_spawn_reaction_effect(const u32 ctx[6], u32 off, u32 stream)
  * start the per-side 0xE8Dxx effect stream. EAX = 1-side from 0x3AAFC. */
 static void fighter_3a0fc(u32 side)
 {
+    PR_SEAM(0x3A0FCu, side);
     u32 ctx[6];
     u32 anim[3];
     u32 facing;
@@ -7811,13 +7825,18 @@ void fighter_3ad98(u32 side, const u32 anim[3])
     if (stream != 0u)                                       /* 0x3AE20/0x3AE22 */
         fighter_spawn_reaction_effect(ctx, off, stream);    /* 0x3AE3C */
     {
-        u32 d = (u32)DSB(ctx[3] + 0x5Au)
-              + (u32)DSB(anim[0] + 4u);                     /* 0x3AE55..0x3AE6D */
-        if (d >= 0x78u) {                                   /* 0x3AE70/0x3AE73 */
-            d = 0x77u - (u32)DSB(ctx[3] + 0x5Au);           /* 0x3AE75/0x3AE7A */
-            if (d < 1u) d = 0u;                             /* 0x3AE7C/0x3AE81 */
+        /* 0x3AE55: EDX = the anim[0]+4 byte; 0x3AE6D's LEA puts the sum in ECX and
+         * only the 0x3AE75 clamp path replaces EDX (0x3AE83 is reached with EDX
+         * unchanged when the sum is below 0x78). The port passed the sum before
+         * the C3d correction (record §C3d.2). */
+        u32 v = (u32)DSB(anim[0] + 4u);                     /* 0x3AE55 */
+        s32 sum = (s32)DSB(ctx[3] + 0x5Au) + (s32)v;        /* 0x3AE6D */
+        if (sum >= 0x78) {                                  /* 0x3AE70/0x3AE73 */
+            s32 d = 0x77 - (s32)DSB(ctx[3] + 0x5Au);        /* 0x3AE75/0x3AE7A */
+            if (d < 1) d = 0;                               /* 0x3AE7C/0x3AE81 */
+            v = (u32)d;
         }
-        fighter_392a0(ctx[3], (s32)d, (s32)DSB(anim[0] + 5u));  /* 0x3AE87 */
+        fighter_392a0(ctx[3], (s32)v, (s32)DSB(anim[0] + 5u));  /* 0x3AE87 */
     }
     DSB(ctx[3] + 0x41u) |= 0x80u;                           /* 0x3AE90 */
 }
diff --git a/port/src/game/fighter.h b/port/src/game/fighter.h
index da756e1..a912534 100644
--- a/port/src/game/fighter.h
+++ b/port/src/game/fighter.h
@@ -420,6 +420,11 @@ int fighter_state_365c8(u32 slot, u32 rec, u32 side);
 int fighter_state_36bc8(u32 slot, u32 rec);
 void hit_anim_start_c(u32 rec, u32 stream, u32 frame_bits);
 
+/* 0x38BB0 / 0x38BC8 (C3d): the side's 0x107A80 stun-table clear and its
+ * 0x107D24-adding sibling; exported for their differential rows' mutants. */
+void fighter_38bb0(u32 side);
+void fighter_38bc8(u32 side);
+
 /* 0x1DE64. The reaction picker: map the side's command word (or, with slot+0x63
  * clear, the 0x46460/0x4649C input scan, record §49-B) through 0x1DDF4 to a
  * reaction code; 0xFF when nothing maps. */
diff --git a/port/tests/diff_runner.c b/port/tests/diff_runner.c
index 0fd6b76..c79377d 100644
--- a/port/tests/diff_runner.c
+++ b/port/tests/diff_runner.c
@@ -10806,6 +10806,580 @@ static void m_c3c_97_cut(const u32 *r, u32 *eax)  { *eax = (u32)c3c_39738_core(r
 static void m_c3c_97_diff(const u32 *r, u32 *eax) { *eax = (u32)c3c_39738_core(r, C3C_97_DIFF); }
 static void m_c3c_97_idx(const u32 *r, u32 *eax)  { *eax = (u32)c3c_39738_core(r, C3C_97_IDX); }
 
+/* ==== track P batch C3d (record 2026-10-05-reverse-c3d): the type-family rows ==== */
+
+/* 0x4F434's mutants. */
+#define C3D_4F_SEL   0x01u  /* opp = sel */
+#define C3D_4F_SCALE 0x02u  /* the 100/120 scale divides by 0x79 */
+#define C3D_4F_THR   0x04u  /* the 0x32 threshold is 0x31 */
+#define C3D_4F_DX    0x08u  /* the dx tests are dropped */
+#define C3D_4F_DELTA 0x10u  /* the delta signs are swapped */
+#define C3D_4F_CALL  0x20u  /* the 0x46534 call is skipped */
+static void c3d_4f434_core(u32 mut)
+{
+    u32 sel = (u32)DSB(0x0010810Du);
+    u32 opp = (mut & C3D_4F_SEL) ? sel : (sel ^ 1u);
+    s32 diff = (s32)DSB(0x001077B0u + sel * 0x94u + 0x5Au)
+             - (s32)DSB(0x001077B0u + opp * 0x94u + 0x5Au);
+    s32 scaled = ((s32)(s16)diff * 100) / ((mut & C3D_4F_SCALE) ? 0x79 : 0x78);
+    s16 dx = (s16)(DSW(0x001082C8u + opp * 4u) - DSW(0x001082C0u + opp * 4u));
+    s32 delta;
+    if ((s16)scaled >= ((mut & C3D_4F_THR) ? 0x31 : 0x32)) {
+        if (!(mut & C3D_4F_DX) && dx != 0) return;
+        delta = (mut & C3D_4F_DELTA) ? 1 : -1;
+    } else if ((s16)scaled >= -0x20) {
+        return;
+    } else if ((s16)scaled >= -0x41) {
+        if (!(mut & C3D_4F_DX) && dx != 0) return;
+        delta = (mut & C3D_4F_DELTA) ? -1 : 1;
+    } else {
+        if (!(mut & C3D_4F_DX) && dx > 1) return;
+        delta = (mut & C3D_4F_DELTA) ? -1 : 1;
+    }
+    if (!(mut & C3D_4F_CALL)) fighter_46534(opp, delta);
+}
+static void b_c3d_4f434(const u32 *r, u32 *eax)   { (void)r; fighter_4f434(); *eax = 0u; }
+static void m_c3d_4f_sel(const u32 *r, u32 *eax)  { (void)r; c3d_4f434_core(C3D_4F_SEL); *eax = 0u; }
+static void m_c3d_4f_scale(const u32 *r, u32 *eax){ (void)r; c3d_4f434_core(C3D_4F_SCALE); *eax = 0u; }
+static void m_c3d_4f_thr(const u32 *r, u32 *eax)  { (void)r; c3d_4f434_core(C3D_4F_THR); *eax = 0u; }
+static void m_c3d_4f_dx(const u32 *r, u32 *eax)   { (void)r; c3d_4f434_core(C3D_4F_DX); *eax = 0u; }
+static void m_c3d_4f_delta(const u32 *r, u32 *eax){ (void)r; c3d_4f434_core(C3D_4F_DELTA); *eax = 0u; }
+static void m_c3d_4f_call(const u32 *r, u32 *eax) { (void)r; c3d_4f434_core(C3D_4F_CALL); *eax = 0u; }
+
+/* 0x385B0's mutants. */
+#define C3D_85_AF8  0x01u  /* the 0x100AF8 clear is skipped */
+#define C3D_85_ARM  0x02u  /* the +0x54 0/1 arm always runs */
+#define C3D_85_FIVE 0x04u  /* the +0x54 == 5 test is inverted */
+#define C3D_85_ANIM 0x08u  /* the 0xC8950 animation is skipped */
+#define C3D_85_TAIL 0x10u  /* the +0x4D/+0x52/+0x53 writes are skipped */
+#define C3D_85_CALL 0x20u  /* the 0x38154 call is skipped */
+static void c3d_385b0_core(const u32 *r, u32 mut)
+{
+    u32 rec = r[R_EAX];
+    u32 side = (u32)DSB(rec + 0x51u);
+    u32 s = 0x001077B0u + side * 0x94u;
+    if (!(mut & C3D_85_AF8)) DSD(0x00100AF8u + side * 4u) = 0;
+    DSB(rec + 0x28u) &= 0xDFu;
+    DSB(s + 0x62u) = 0;
+    DSB(s + 0x8Au) = 0;
+    DSW(s + 0x84u) = (u16)(DSW(s + 0x84u) + 1u);
+    fighter_164e8(side);
+    DSD(s + 0x40u) &= 0xCCF3BFFFu;
+    DSB(rec + 0x42u) = 0;
+    DSB(s + 0x5Fu) = 0xFFu;
+    DSB(s + 0x55u) = 0xFFu;
+    DSD(s + 0x0Cu) = 0;
+    DSD(s + 0x10u) = 0;
+    DSD(s + 0x18u) = 0;
+    DSD(s + 0x1Cu) = 0;
+    DSB(s + 0x67u) = 0;
+    DSB(s + 0x65u) = 0xFFu;
+    DSW(s + 0x74u) = 0;
+    if ((mut & C3D_85_ARM) || DSB(s + 0x54u) == 0u || DSB(s + 0x54u) == 1u) {
+        DSB(s + 0x68u) = 0;
+        DSW(rec + 0x44u) = 0;
+        DSB(rec + 0x43u) = 0;
+        DSB(rec + 0x42u) = 0;
+        DSW(rec + 0x34u) = 0;
+        DSW(rec + 0x36u) = 0;
+        DSD(rec + 0x1Cu) = 0;
+    }
+    if ((mut & C3D_85_FIVE) ? (DSB(s + 0x54u) == 5u) : (DSB(s + 0x54u) != 5u)) {
+        DSB(s + 0x54u) = 0;
+        DSB(s + 0x41u) &= 0x7Fu;
+        DSB(rec + 0x4Cu) = 0;
+        if (!(mut & C3D_85_ANIM))
+            actors_anim_begin(rec, DSD(0x000C8950u + (u32)DSB(s + 0x7Au) * 4u), 0x40400000u);
+        if (!(mut & C3D_85_TAIL)) {
+            DSB(rec + 0x4Du) = 0x1Eu;
+            DSB(s + 0x52u) = 0;
+            DSB(s + 0x53u) = 0;
+        }
+    } else if (!(mut & C3D_85_CALL)) {
+        fighter_38154((u32)DSB(rec + 0x51u));
+    }
+}
+static void b_c3d_385b0(const u32 *r, u32 *eax)    { fighter_385b0(r[R_EAX]); *eax = 0u; }
+static void m_c3d_85_af8(const u32 *r, u32 *eax)   { c3d_385b0_core(r, C3D_85_AF8); *eax = 0u; }
+static void m_c3d_85_arm(const u32 *r, u32 *eax)   { c3d_385b0_core(r, C3D_85_ARM); *eax = 0u; }
+static void m_c3d_85_five(const u32 *r, u32 *eax)  { c3d_385b0_core(r, C3D_85_FIVE); *eax = 0u; }
+static void m_c3d_85_anim(const u32 *r, u32 *eax)  { c3d_385b0_core(r, C3D_85_ANIM); *eax = 0u; }
+static void m_c3d_85_tail(const u32 *r, u32 *eax)  { c3d_385b0_core(r, C3D_85_TAIL); *eax = 0u; }
+static void m_c3d_85_call(const u32 *r, u32 *eax)  { c3d_385b0_core(r, C3D_85_CALL); *eax = 0u; }
+
+/* 0x3AE9C's mutants. */
+#define C3D_AE_S53  0x01u  /* the +0x53 == 7 gate is inverted */
+#define C3D_AE_G    0x02u  /* the rec+0x36 > 0 test is inverted */
+#define C3D_AE_SIGN 0x04u  /* the sign apply is skipped */
+#define C3D_AE_TAB  0x08u  /* the table reads the other character */
+#define C3D_AE_U44  0x10u  /* the 0x28/0x3C band is swapped */
+#define C3D_AE_K    0x20u  /* the k clamp bounds are swapped */
+#define C3D_AE_BIT  0x40u  /* the +0x40 bit 0x80 test is dropped */
+static void c3d_3ae9c_core(const u32 *r, u32 mut)
+{
+    u32 side = r[R_EAX];
+    u8 param_2 = (u8)r[R_EDX];
+    u32 ctx[6];
+    u32 rec;
+    s32 sign;
+    int g;
+    fighter_ctx_same(ctx, side);
+    rec = ctx[4];
+    g = ((s16)DSW(rec + 0x36u) > 0);
+    if (mut & C3D_AE_G) g = !g;
+    {
+        s16 w34 = (s16)DSW(rec + 0x34u);
+        sign = (w34 < 0) ? -1 : ((w34 > 0) ? 1 : 0);
+    }
+    if ((mut & C3D_AE_S53) ? (DSB(ctx[2] + 0x53u) == 7u) : (DSB(ctx[2] + 0x53u) != 7u)) {
+        if (g) {
+            DSW(rec + 0x34u) = 0x64u;
+        } else {
+            u32 ch = (u32)DSB(ctx[2] + ((mut & C3D_AE_TAB) ? 0x64u : 0x7Au));
+            DSW(rec + 0x36u) = DSW(0x000BEDDCu + ch * 2u);
+            DSW(rec + 0x34u) = 0x64u;
+            if (!(mut & C3D_AE_BIT) && (DSB(ctx[2] + 0x40u) & 0x80u) != 0u) {
+                DSW(rec + 0x44u) = (param_2 != 0u)
+                    ? ((mut & C3D_AE_U44) ? 0x3Cu : 0x28u)
+                    : ((mut & C3D_AE_U44) ? 0x28u : 0x3Cu);
+            } else if (!(DSB(ctx[2] + 0x40u) & 0x80u)) {
+                s32 k = (s32)DSD(rec + 0x42u) >> 16;
+                int hi = (mut & C3D_AE_K) ? 0x1Au : 0x1Eu;
+                int lo = (mut & C3D_AE_K) ? 0x1Eu : 0x1Au;
+                if (k > hi) DSW(rec + 0x44u) = (u16)hi;
+                else if (k < lo) DSW(rec + 0x44u) = (u16)lo;
+                if (param_2 != 0u)
+                    DSW(rec + 0x36u) = DSW(0x000BEDDCu + ch * 2u);
+            }
+        }
+        if (!(mut & C3D_AE_SIGN)) {
+            if (sign == 0) {
+                if (fighter_actor_bit15_clear(ctx[0]) != 0)
+                    DSW(rec + 0x34u) = (u16)(-(s16)DSW(rec + 0x34u));
+            } else {
+                DSW(rec + 0x34u) = (u16)((s16)DSW(rec + 0x34u) * (s16)sign);
+            }
+        }
+    }
+}
+static void b_c3d_3ae9c(const u32 *r, u32 *eax)   { fighter_3ae9c(r[R_EAX], (u8)r[R_EDX]); *eax = 0u; }
+static void m_c3d_ae_s53(const u32 *r, u32 *eax)  { c3d_3ae9c_core(r, C3D_AE_S53); *eax = 0u; }
+static void m_c3d_ae_g(const u32 *r, u32 *eax)    { c3d_3ae9c_core(r, C3D_AE_G); *eax = 0u; }
+static void m_c3d_ae_sign(const u32 *r, u32 *eax) { c3d_3ae9c_core(r, C3D_AE_SIGN); *eax = 0u; }
+static void m_c3d_ae_tab(const u32 *r, u32 *eax)  { c3d_3ae9c_core(r, C3D_AE_TAB); *eax = 0u; }
+static void m_c3d_ae_u44(const u32 *r, u32 *eax)  { c3d_3ae9c_core(r, C3D_AE_U44); *eax = 0u; }
+static void m_c3d_ae_k(const u32 *r, u32 *eax)    { c3d_3ae9c_core(r, C3D_AE_K); *eax = 0u; }
+static void m_c3d_ae_bit(const u32 *r, u32 *eax)  { c3d_3ae9c_core(r, C3D_AE_BIT); *eax = 0u; }
+
+/* 0x3B080's mutants. */
+#define C3D_B0_GATE   0x01u  /* the 0xBEDF2 gate is dropped */
+#define C3D_B0_NEG    0x02u  /* the sign flip is inverted */
+#define C3D_B0_MIRROR 0x04u  /* the mirror arm is skipped */
+#define C3D_B0_P4     0x08u  /* the param_4 gate is dropped */
+#define C3D_B0_C148   0x10u  /* the 0x3C148 calls are skipped */
+#define C3D_B0_O43    0x20u  /* the +0x43 store uses param_2 */
+static void c3d_3b080_core(const u32 *r, u32 mut)
+{
+    u32 side = r[R_EAX];
+    u32 param_2 = r[R_EDX];
+    u32 param_3 = r[R_EBX];
+    u32 param_4 = r[R_ECX];
+    s32 v;
+    if (!(mut & C3D_B0_GATE) && DSB(0x000BEDF2u) != 0u) return;
+    v = (s32)(param_2 * 2u);
+    if ((mut & C3D_B0_NEG) ? (fighter_actor_bit15_clear(1u - side) == 0)
+                           : (fighter_actor_bit15_clear(1u - side) != 0))
+        v = -v;
+    if (!(mut & C3D_B0_C148)) fighter_3c148(side);
+    DSW(DSD(0x001077B0u + side * 0x94u) + 0x34u) = (u16)v;
+    DSB(DSD(0x001077B0u + side * 0x94u) + 0x43u) = (u8)((mut & C3D_B0_O43) ? param_2 : param_3);
+    if (fighter_3b038(side) == 0) return;
+    if (!(mut & C3D_B0_P4) && param_4 == 0u) return;
+    if (mut & C3D_B0_MIRROR) return;
+    if (!(mut & C3D_B0_C148)) {
+        fighter_3c148(side);
+        fighter_3c148(1u - side);
+    }
+    DSW(DSD(0x001077B0u + (1u - side) * 0x94u) + 0x34u) = (u16)(-v);
+    DSB(DSD(0x001077B0u + (1u - side) * 0x94u) + 0x43u) = (u8)param_3;
+}
+static void b_c3d_3b080(const u32 *r, u32 *eax)
+{ fighter_3b080(r[R_EAX], r[R_EDX], r[R_EBX], r[R_ECX]); *eax = 0u; }
+static void m_c3d_b0_gate(const u32 *r, u32 *eax)   { c3d_3b080_core(r, C3D_B0_GATE); *eax = 0u; }
+static void m_c3d_b0_neg(const u32 *r, u32 *eax)    { c3d_3b080_core(r, C3D_B0_NEG); *eax = 0u; }
+static void m_c3d_b0_mirror(const u32 *r, u32 *eax) { c3d_3b080_core(r, C3D_B0_MIRROR); *eax = 0u; }
+static void m_c3d_b0_p4(const u32 *r, u32 *eax)     { c3d_3b080_core(r, C3D_B0_P4); *eax = 0u; }
+static void m_c3d_b0_c148(const u32 *r, u32 *eax)   { c3d_3b080_core(r, C3D_B0_C148); *eax = 0u; }
+static void m_c3d_b0_o43(const u32 *r, u32 *eax)    { c3d_3b080_core(r, C3D_B0_O43); *eax = 0u; }
+
+/* 0x36BC8's mutants. */
+#define C3D_6B_OTHER 0x01u  /* other = side */
+#define C3D_6B_COND  0x02u  /* the D2C gate is inverted */
+#define C3D_6B_ANIM  0x04u  /* the C8A18 animation is skipped */
+#define C3D_6B_S5D   0x08u  /* +0x5D is 0x45 */
+#define C3D_6B_BIT   0x10u  /* the +0x43 bit 2 clear is skipped */
+#define C3D_6B_TAIL  0x20u  /* the second-animation block is skipped */
+static u32 c3d_36bc8_core(const u32 *r, u32 mut)
+{
+    u32 slot = r[R_EAX];
+    u32 rec = r[R_EDX];
+    u32 side = (u32)DSB(rec + 0x51u);
+    u32 other = (mut & C3D_6B_OTHER) ? side : (1u - side);
+    DSW(slot + 0x74u) = 0;
+    if (((s16)DSW(0x00107D2Cu + other * 2u) < 1) != ((mut & C3D_6B_COND) != 0))
+        fighter_38bc8(other);
+    else
+        fighter_38bb0(other);
+    if (!(mut & C3D_6B_ANIM))
+        actors_anim_begin(rec, DSD(0x000C8A18u + (u32)DSB(slot + 0x7Au) * 4u), 0x40800000u);
+    hit_anchor_set(side, DSD(rec + 0x18u), 0u);
+    DSB(slot + 0x5Du) = (mut & C3D_6B_S5D) ? 0x45u : 0x44u;
+    DSB(slot + 0x54u) = 0;
+    DSB(slot + 0x52u) = 7u;
+    DSB(slot + 0x53u) = 2u;
+    if ((DSB(slot + 0x43u) & 4u) != 0u) {
+        if (!(mut & C3D_6B_BIT)) DSB(slot + 0x43u) &= 0xFBu;
+        return 7;
+    }
+    if (!(mut & C3D_6B_TAIL) && DSW(0x00104B00u) != 3u && DSW(0x00104B00u) != 0x22u) {
+        actors_anim_begin(DSD(0x00102900u + (u32)DSB(rec + 0x51u) * 4u),
+                          0x000E906Eu, 0x40400000u);
+    }
+    return 7;
+}
+static void b_c3d_36bc8(const u32 *r, u32 *eax)   { *eax = (u32)fighter_state_36bc8(r[R_EAX], r[R_EDX]); }
+static void m_c3d_6b_other(const u32 *r, u32 *eax) { *eax = c3d_36bc8_core(r, C3D_6B_OTHER); }
+static void m_c3d_6b_cond(const u32 *r, u32 *eax)  { *eax = c3d_36bc8_core(r, C3D_6B_COND); }
+static void m_c3d_6b_anim(const u32 *r, u32 *eax)  { *eax = c3d_36bc8_core(r, C3D_6B_ANIM); }
+static void m_c3d_6b_s5d(const u32 *r, u32 *eax)   { *eax = c3d_36bc8_core(r, C3D_6B_S5D); }
+static void m_c3d_6b_bit(const u32 *r, u32 *eax)   { *eax = c3d_36bc8_core(r, C3D_6B_BIT); }
+static void m_c3d_6b_tail(const u32 *r, u32 *eax)  { *eax = c3d_36bc8_core(r, C3D_6B_TAIL); }
+
+/* 0x379C4's mutants. */
+#define C3D_79_FE   0x01u  /* the 0x1078FE gate is inverted */
+#define C3D_79_S57  0x02u  /* the +0x57 == 2 test is inverted */
+#define C3D_79_B41  0x04u  /* the +0x41 bit 2 test is dropped */
+#define C3D_79_E8   0x08u  /* the 0x1078E8 null test is inverted */
+#define C3D_79_CB   0x10u  /* the callback's non-zero return is ignored */
+#define C3D_79_FC   0x20u  /* the 0x1078FC store is skipped */
+#define C3D_79_TAB  0x40u  /* the stream tables are swapped */
+static void c3d_379c4_core(const u32 *r, u32 mut)
+{
+    u32 slot = r[R_EAX];
+    u32 rec = DSD(slot);
+    if ((mut & C3D_79_FE) ? (DSB(0x001078FEu) != 0u) : (DSB(0x001078FEu) == 0u)) {
+        if ((mut & C3D_79_S57) ? (DSB(slot + 0x57u) != 2u) : (DSB(slot + 0x57u) == 2u))
+            fighter_37178(slot);
+        return;
+    }
+    if ((mut & C3D_79_B41) || (DSB(slot + 0x41u) & 2u) != 0u) {
+        if ((mut & C3D_79_E8) ? (DSD(0x001078E8u) == 0u) : (DSD(0x001078E8u) != 0u)) {
+            int (*cb)(u32, u32) =
+                (int (*)(u32, u32))(void *)fn_resolve(DSD(0x001078E8u));
+            if (cb != 0 && cb(slot, rec) != 0 && !(mut & C3D_79_CB)) return;
+            if (!(mut & C3D_79_FC)) DSB(0x001078FCu) = 1u;
+            actors_anim_begin(rec, DSD(((mut & C3D_79_TAB) ? 0x001078E4u : 0x000C9260u)
+                                       + (u32)DSB(slot + 0x7Au) * 4u), 0x40400000u);
+            return;
+        }
+        actors_anim_begin(rec, DSD(0x001078E4u), 0x40400000u);
+        return;
+    }
+    actors_anim_begin(rec, DSD(0x000C9260u + (u32)DSB(slot + 0x7Au) * 4u), 0x40400000u);
+}
+static void b_c3d_379c4(const u32 *r, u32 *eax)   { fighter_379c4(r[R_EAX]); *eax = 0u; }
+static void m_c3d_79_fe(const u32 *r, u32 *eax)   { c3d_379c4_core(r, C3D_79_FE); *eax = 0u; }
+static void m_c3d_79_s57(const u32 *r, u32 *eax)  { c3d_379c4_core(r, C3D_79_S57); *eax = 0u; }
+static void m_c3d_79_b41(const u32 *r, u32 *eax)  { c3d_379c4_core(r, C3D_79_B41); *eax = 0u; }
+static void m_c3d_79_e8(const u32 *r, u32 *eax)   { c3d_379c4_core(r, C3D_79_E8); *eax = 0u; }
+static void m_c3d_79_cb(const u32 *r, u32 *eax)   { c3d_379c4_core(r, C3D_79_CB); *eax = 0u; }
+static void m_c3d_79_fc(const u32 *r, u32 *eax)   { c3d_379c4_core(r, C3D_79_FC); *eax = 0u; }
+static void m_c3d_79_tab(const u32 *r, u32 *eax)  { c3d_379c4_core(r, C3D_79_TAB); *eax = 0u; }
+
+/* 0x3AD98's mutants. */
+#define C3D_AD_VOICE 0x01u  /* the voice index reads anim[0]+8 */
+#define C3D_AD_OFF   0x02u  /* the offset table indexes by side, not ctx[0] */
+#define C3D_AD_TAB   0x04u  /* the stream switch is off by one */
+#define C3D_AD_ZERO  0x08u  /* the default stream is 1 */
+#define C3D_AD_CALL  0x10u  /* the effect spawn is skipped */
+#define C3D_AD_CLAMP 0x20u  /* the >= 0x78 clamp is dropped */
+#define C3D_AD_D     0x40u  /* d reads anim[0]+5, not +4 */
+#define C3D_AD_BIT   0x80u  /* the +0x41 bit 0x80 store is skipped */
+static void c3d_3ad98_core(const u32 *r, u32 mut)
+{
+    u32 side = r[R_EAX];
+    u32 a0 = r[R_EDX], a1 = DSD(r[R_EDX] + 4u), a2 = DSD(r[R_EDX] + 8u);
+    u32 ctx[6];
+    u32 off, stream;
+    fighter_ctx_swap(ctx, side);
+    (void)sound_voice((u32)DSW(0x000E9358u + (u32)DSB(a0 + ((mut & C3D_AD_VOICE) ? 8u : 9u)) * 2u));
+    off = DSD(0x000F0AECu) + 0x3BC0u
+        - ((u32)DSD(0x00100AD8u + ((mut & C3D_AD_OFF) ? side : ctx[0]) * 4u) << 6);
+    off -= (u32)((s32)DSD(ctx[5] + 0x30u) >> 16);
+    {
+        u32 key = (u16)((a2 & 0xFFFFu) + ((mut & C3D_AD_TAB) ? 1u : 0u));
+        if (key == 1u) stream = 0x000E8E08u;
+        else if (key == 2u) stream = 0x000E8E22u;
+        else if (key == 3u) stream = 0x000E8E3Cu;
+        else stream = (mut & C3D_AD_ZERO) ? 1u : 0u;
+    }
+    if (stream != 0u && !(mut & C3D_AD_CALL)) {
+        u32 a = actor_spawn((const u32 *)(mem + 0x000BB0B0u), DSD(ctx[3] + 0x2Cu),
+                            (u32)((s32)DSD(ctx[5] + 0x30u) >> 16), off, 0u);
+        DSB(a + 0x59u) = 3u;
+        actors_anim_begin(a, stream, 0x40000000u);
+    }
+    {
+        u32 v = (u32)DSB(a0 + ((mut & C3D_AD_D) ? 5u : 4u));
+        s32 sum = (s32)DSB(ctx[3] + 0x5Au) + (s32)v;
+        if ((sum >= 0x78) || (mut & C3D_AD_CLAMP)) {
+            s32 d = 0x77 - (s32)DSB(ctx[3] + 0x5Au);
+            if (d < 1) d = 0;
+            v = (u32)d;
+        }
+        fighter_392a0(ctx[3], (s32)v, (s32)DSB(a0 + 5u));
+    }
+    if (!(mut & C3D_AD_BIT)) DSB(ctx[3] + 0x41u) |= 0x80u;
+}
+static void b_c3d_3ad98(const u32 *r, u32 *eax)
+{
+    u32 anim[3] = { DSD(r[R_EDX]), DSD(r[R_EDX] + 4u), DSD(r[R_EDX] + 8u) };
+    fighter_3ad98(r[R_EAX], anim);
+    *eax = 0u;
+}
+static void m_c3d_ad_voice(const u32 *r, u32 *eax) { c3d_3ad98_core(r, C3D_AD_VOICE); *eax = 0u; }
+static void m_c3d_ad_off(const u32 *r, u32 *eax)   { c3d_3ad98_core(r, C3D_AD_OFF); *eax = 0u; }
+static void m_c3d_ad_tab(const u32 *r, u32 *eax)   { c3d_3ad98_core(r, C3D_AD_TAB); *eax = 0u; }
+static void m_c3d_ad_zero(const u32 *r, u32 *eax)  { c3d_3ad98_core(r, C3D_AD_ZERO); *eax = 0u; }
+static void m_c3d_ad_call(const u32 *r, u32 *eax)  { c3d_3ad98_core(r, C3D_AD_CALL); *eax = 0u; }
+static void m_c3d_ad_clamp(const u32 *r, u32 *eax) { c3d_3ad98_core(r, C3D_AD_CLAMP); *eax = 0u; }
+static void m_c3d_ad_d(const u32 *r, u32 *eax)     { c3d_3ad98_core(r, C3D_AD_D); *eax = 0u; }
+static void m_c3d_ad_bit(const u32 *r, u32 *eax)   { c3d_3ad98_core(r, C3D_AD_BIT); *eax = 0u; }
+
+/* 0x1AB5C's mutants. */
+#define C3D_IM_LOOP 0x01u  /* six ring reads */
+#define C3D_IM_WORD 0x02u  /* the command word index drops the *2 */
+#define C3D_IM_OK   0x04u  /* the state gate is dropped */
+#define C3D_IM_FACE 0x08u  /* the facing base pair is swapped */
+#define C3D_IM_EQ   0x10u  /* the equal-fate base is 0x1000 */
+#define C3D_IM_BL   0x20u  /* the 0xFF path mask bits are swapped */
+#define C3D_IM_ARM  0x40u  /* the final bl==0 gate is dropped */
+#define C3D_IM_S54  0x80u  /* the +0x54 mask is 0x2000 */
+#define C3D_IM_CALL 0x100u /* the 0x1A7CC call is skipped */
+static u32 c3d_1ab5c_core(const u32 *r, u32 mut)
+{
+    u32 side = r[R_EAX];
+    u32 ctx[6];
+    u32 mask = 0;
+    u8 bl = 0;
+    s32 i;
+    fighter_ctx_swap(ctx, side);
+    for (i = 0; i < ((mut & C3D_IM_LOOP) ? 6 : 7); i++)
+        mask |= fighter_input_read(ctx[1], i);
+    mask |= DSW(0x001088E0u + ctx[1] * ((mut & C3D_IM_WORD) ? 1u : 2u));
+    if (!(mut & C3D_IM_OK) && !fighter_state_ok(ctx[1])) {
+        DSB(ctx[3] + 0x43u) &= 0xCFu;
+        return 0;
+    }
+    {
+        s32 self2c = (s32)DSD(ctx[3] + 0x2Cu);
+        s32 other2c = (s32)DSD(ctx[2] + 0x2Cu);
+        u32 base;
+        if (self2c == other2c) base = (mut & C3D_IM_EQ) ? 0x1000u : 0x3000u;
+        else if (self2c < other2c) base = (mut & C3D_IM_FACE) ? 0x1000u : 0x2000u;
+        else base = (mut & C3D_IM_FACE) ? 0x2000u : 0x1000u;
+        if (DSB(ctx[2] + 0x5Fu) != 0xFFu) {
+            if (((u16)mask & (u16)base) != 0u) {
+                bl = 1;
+                hit_facing_flag(ctx[1]);
+            }
+        } else if (DSB(ctx[2] + 0x64u) != 0xFFu) {
+            u32 rec2 = DSD(ctx[2] + 0x08u);
+            if (rec2 != 0) {
+                u32 want = (mut & C3D_IM_BL) ? 0x1000u : 0x2000u;
+                u32 want2 = (mut & C3D_IM_BL) ? 0x2000u : 0x1000u;
+                if ((s16)DSW(rec2 + 0x34u) < 0) {
+                    if ((mask & want) != 0) bl = 1;
+                } else if ((mask & want2) != 0) bl = 1;
+            }
+        }
+        if (bl == 0 && (mut & C3D_IM_ARM)) { /* the gate dropped */ }
+        else if (bl == 0 && DSB(ctx[3] + 0x53u) != 1u) return base;
+        if ((mut & C3D_IM_CALL)) return base;
+        DSB(ctx[3] + 0x54u) = (u8)((mask & ((mut & C3D_IM_S54) ? 0x2000u : 0x4000u)) != 0);
+        fighter_block_start(ctx[1]);
+        return base;
+    }
+}
+static void b_c3d_1ab5c(const u32 *r, u32 *eax)    { *eax = fighter_input_mask(r[R_EAX]); }
+static void m_c3d_im_loop(const u32 *r, u32 *eax)  { *eax = c3d_1ab5c_core(r, C3D_IM_LOOP); }
+static void m_c3d_im_word(const u32 *r, u32 *eax)  { *eax = c3d_1ab5c_core(r, C3D_IM_WORD); }
+static void m_c3d_im_ok(const u32 *r, u32 *eax)    { *eax = c3d_1ab5c_core(r, C3D_IM_OK); }
+static void m_c3d_im_face(const u32 *r, u32 *eax)  { *eax = c3d_1ab5c_core(r, C3D_IM_FACE); }
+static void m_c3d_im_eq(const u32 *r, u32 *eax)    { *eax = c3d_1ab5c_core(r, C3D_IM_EQ); }
+static void m_c3d_im_bl(const u32 *r, u32 *eax)    { *eax = c3d_1ab5c_core(r, C3D_IM_BL); }
+static void m_c3d_im_arm(const u32 *r, u32 *eax)   { *eax = c3d_1ab5c_core(r, C3D_IM_ARM); }
+static void m_c3d_im_s54(const u32 *r, u32 *eax)   { *eax = c3d_1ab5c_core(r, C3D_IM_S54); }
+static void m_c3d_im_call(const u32 *r, u32 *eax)  { *eax = c3d_1ab5c_core(r, C3D_IM_CALL); }
+
+/* 0x3B134's mutants. */
+#define C3D_CM_A  0x01u  /* the +0x63 gate is dropped */
+#define C3D_CM_B  0x02u  /* the 0x1AB10 gate is dropped */
+#define C3D_CM_R  0x04u  /* the roll gate is dropped */
+#define C3D_CM_OV 0x08u  /* the override is ignored */
+#define C3D_CM_BS 0x10u  /* the facing base pair is swapped */
+#define C3D_CM_OS 0x20u  /* the other-slot 0x5000/0x6000 are swapped */
+#define C3D_CM_B1 0x40u  /* the animation +2 bit tests are swapped */
+#define C3D_CM_RD 0x80u  /* the readiness gate is dropped */
+#define C3D_CM_C  0x100u /* the consumer call is skipped */
+static void c3d_3b134_core(const u32 *r, u32 mut)
+{
+    u32 side = r[R_EAX];
+    u32 edx_arg = r[R_EDX];
+    u32 override = r[R_EBX];
+    u32 ctx[6];
+    u32 anim[3];
+    u32 self, other, base;
+    u8 b;
+    fighter_ctx_swap(ctx, side);
+    fighter_anim_triple(anim, ctx[0], (s32)edx_arg);
+    if (!(mut & C3D_CM_A) && DSB(0x001077B0u + side * 0x94u + 0x63u) == 0) return;
+    if (!(mut & C3D_CM_B) && !fighter_state_ok(ctx[1])) return;
+    {
+        u32 draw = rng_next(0x64u);
+        u32 c8 = DSD(0x001082C8u + side * 4u);
+        u32 idx = (u32)DSB(0x0010452Cu) << 4;
+        s32 thr = (s32)DSD(0x000BEDF2u + idx + c8 * 2u) >> 16;
+        if (!(mut & C3D_CM_R)) {
+            int bypass = ((override & 0xFFu) != 0) && !(mut & C3D_CM_OV);
+            if ((s32)draw > thr && !bypass) return;
+        }
+    }
+    self = ctx[3]; other = ctx[2];
+    {
+        s32 a = (s32)DSD(self + 0x2Cu), o = (s32)DSD(other + 0x2Cu);
+        base = (a == o) ? 0x2000u : ((a > o) ? 0x1000u : 0x2000u);
+        if (mut & C3D_CM_BS)
+            base = (a == o) ? 0x1000u : ((a > o) ? 0x2000u : 0x1000u);
+    }
+    if (DSB(other + 0x64u) != 0xFFu && DSD(other + 0x08u) != 0) {
+        s16 cx = (s16)DSW(DSD(other + 0x08u) + 0x34u);
+        DSW(0x001088E0u + side * 2u) = (u16)((cx < 0) == !(mut & C3D_CM_OS) ? 0x6000u : 0x5000u);
+        return;
+    }
+    b = DSB(anim[2] + 2u);
+    if (mut & C3D_CM_B1) {
+        if ((b & 2u) == 0) { DSW(0x001088E0u + side * 2u) = (u16)base; return; }
+        if ((b & 1u) == 0) { DSW(0x001088E0u + side * 2u) = (u16)(base | 0x4000u); return; }
+    } else {
+        if ((b & 1u) == 0) { DSW(0x001088E0u + side * 2u) = (u16)base; return; }
+        if ((b & 2u) == 0) { DSW(0x001088E0u + side * 2u) = (u16)(base | 0x4000u); return; }
+    }
+    DSW(0x001088E0u + side * 2u) = 0x8000u;
+    if ((!(mut & C3D_CM_RD) && fight_attack_ready(side)) || (mut & C3D_CM_RD))
+        if (!(mut & C3D_CM_C)) (void)fighter_attack_consume(side);
+}
+static void b_c3d_3b134(const u32 *r, u32 *eax)
+{ fight_command_map(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
+static void m_c3d_cm_a(const u32 *r, u32 *eax)   { c3d_3b134_core(r, C3D_CM_A); *eax = 0u; }
+static void m_c3d_cm_b(const u32 *r, u32 *eax)   { c3d_3b134_core(r, C3D_CM_B); *eax = 0u; }
+static void m_c3d_cm_r(const u32 *r, u32 *eax)   { c3d_3b134_core(r, C3D_CM_R); *eax = 0u; }
+static void m_c3d_cm_ov(const u32 *r, u32 *eax)  { c3d_3b134_core(r, C3D_CM_OV); *eax = 0u; }
+static void m_c3d_cm_bs(const u32 *r, u32 *eax)  { c3d_3b134_core(r, C3D_CM_BS); *eax = 0u; }
+static void m_c3d_cm_os(const u32 *r, u32 *eax)  { c3d_3b134_core(r, C3D_CM_OS); *eax = 0u; }
+static void m_c3d_cm_b1(const u32 *r, u32 *eax)  { c3d_3b134_core(r, C3D_CM_B1); *eax = 0u; }
+static void m_c3d_cm_rd(const u32 *r, u32 *eax)  { c3d_3b134_core(r, C3D_CM_RD); *eax = 0u; }
+static void m_c3d_cm_c(const u32 *r, u32 *eax)   { c3d_3b134_core(r, C3D_CM_C); *eax = 0u; }
+
+/* 0x2A820's mutants. */
+#define C3D_PW_POOL 0x01u  /* the parent index reads rec+0x4b */
+#define C3D_PW_X    0x02u  /* the child x reads pp+8 */
+#define C3D_PW_LAY  0x04u  /* the layer clamp is dropped */
+#define C3D_PW_VIS  0x08u  /* the visibility bit is 0x02 */
+#define C3D_PW_EXT  0x10u  /* the x window uses 0x3bc0 */
+#define C3D_PW_DEAD 0x20u  /* the dead tests are dropped */
+#define C3D_PW_CALL 0x40u  /* the 0x2A690 call is skipped */
+static void c3d_2a820_core(const u32 *r, u32 mut)
+{
+    u32 rec = r[R_EAX], pset = r[R_EDX];
+    if ((DSW(rec + 0x28u) >> 8 & 0x20u) == 0) {
+        if ((DSW(rec + 0x28u) >> 8 & 0x04u) == 0) {
+            if (!(mut & C3D_PW_CALL)) actor_pset_point(rec);
+        } else {
+            u32 parent = DSD(0x001014F4u)
+                       + (u32)DSB(rec + ((mut & C3D_PW_POOL) ? 0x4Bu : 0x4Au)) * ACTOR_REC_SIZE;
+            if ((DSW(parent + 0x28u) & 8u) != 0) goto dead;
+            {
+                u32 pp = DSD(0x001014ECu) + (u32)DSW(parent + 0x56u) * PSET_SIZE;
+                u32 x = DSD(pp + ((mut & C3D_PW_X) ? 8u : 4u))
+                      + (u32)(((s32)DSD(rec + 0x32u) >> 16) * 64);
+                DSD(pset + 4u) = x;
+                DSD(0x00105BDCu) = x;
+                if ((DSW(rec + 0x28u) & 0x40u) == 0)
+                    DSD(pset + 8u) = DSD(pp + 8u)
+                        + (u32)(((s32)DSD(rec + 0x34u) >> 16) * 64);
+                else
+                    DSD(pset + 8u) = (u32)((s32)DSD(0x000F0AECu) + 0x3bc0
+                        - ((s32)DSD(parent + 0x30u) >> 16));
+                if ((DSW(parent + 0x28u) & 8u) != 0) goto dead;
+                {
+                    u8 lyr = (u8)(DSB(pp + 0x0Eu) + DSB(rec + 0x59u));
+                    DSB(rec + 0x49u) = lyr;
+                    DSW(pset + 0x0Eu) = lyr;
+                }
+                DSW(pset + 0x0Cu) = DSW(rec + 0x2Cu);
+                DSD(rec + 0x3Cu) = DSD(pset + 4u);
+            }
+        }
+    } else {
+        if ((DSW(rec + 0x28u) >> 8 & 0x04u) != 0) {
+            u32 parent = DSD(0x001014F4u)
+                       + (u32)DSB(rec + 0x4Au) * ACTOR_REC_SIZE;
+            if ((DSW(parent + 0x28u) & 8u) != 0) goto dead;
+            DSD(rec + 0x18u) = DSD(parent + 0x18u)
+                + (u32)(((s32)DSD(rec + 0x32u) >> 16) * 64);
+            DSD(rec + 0x1Cu) = DSD(parent + 0x1Cu)
+                + (u32)(((s32)DSD(rec + 0x34u) >> 16) * 64);
+            DSW(rec + 0x2Cu) = DSW(parent + 0x2Cu);
+        }
+        DSD(pset + 4u) = DSD(rec + 0x18u);
+        DSD(0x00105BDCu) = DSD(rec + 0x18u);
+        DSD(pset + 8u) = DSD(rec + 0x1Cu);
+        DSD(0x00105BE0u) = DSD(rec + 0x1Cu);
+        DSW(pset + 0x0Cu) = DSW(rec + 0x2Cu);
+        {
+            u16 layer = (u16)((s32)DSB(rec + 0x49u) + (s32)(s8)DSB(rec + 0x59u));
+            if (!(mut & C3D_PW_LAY) && (s32)layer > 0xff) layer = 0xff;
+            DSW(pset + 0x0Eu) = layer;
+        }
+    }
+    if ((DSW(rec + 0x28u) >> 8 & ((mut & C3D_PW_VIS) ? 2u : 1u)) != 0) return;
+    {
+        u32 extent = DSW(rec + 0x40u);
+        u32 hi = (mut & C3D_PW_EXT) ? 0x3bc0u : 0x5400u;
+        if ((s32)(0u - extent) < (s32)DSD(0x00105BDCu) &&
+            (s32)DSD(0x00105BDCu) < (s32)(extent + hi) &&
+            (s32)(0u - extent) < (s32)DSD(0x00105BE0u) &&
+            (s32)DSD(0x00105BE0u) < (s32)(extent + 0x3bc0u) &&
+            DSW(rec + 0x2Cu) != 0) {
+            DSB(rec + 0x2Bu) |= 0x18u;
+            return;
+        }
+    }
+    if ((DSW(rec + 0x28u) & 0x80u) == 0) return;
+    if ((DSW(rec + 0x2Au) >> 8 & 8u) == 0) return;
+dead:
+    if (!(mut & C3D_PW_DEAD)) set_dead(rec);
+}
+static void b_c3d_2a820(const u32 *r, u32 *eax)  { pset_write(r[R_EAX], r[R_EDX]); *eax = 0u; }
+static void m_c3d_pw_pool(const u32 *r, u32 *eax) { c3d_2a820_core(r, C3D_PW_POOL); *eax = 0u; }
+static void m_c3d_pw_x(const u32 *r, u32 *eax)    { c3d_2a820_core(r, C3D_PW_X); *eax = 0u; }
+static void m_c3d_pw_lay(const u32 *r, u32 *eax)  { c3d_2a820_core(r, C3D_PW_LAY); *eax = 0u; }
+static void m_c3d_pw_vis(const u32 *r, u32 *eax)  { c3d_2a820_core(r, C3D_PW_VIS); *eax = 0u; }
+static void m_c3d_pw_ext(const u32 *r, u32 *eax)  { c3d_2a820_core(r, C3D_PW_EXT); *eax = 0u; }
+static void m_c3d_pw_dead(const u32 *r, u32 *eax) { c3d_2a820_core(r, C3D_PW_DEAD); *eax = 0u; }
+static void m_c3d_pw_call(const u32 *r, u32 *eax) { c3d_2a820_core(r, C3D_PW_CALL); *eax = 0u; }
+
 static const binding_t k_bindings[] = {
     { "rng_next",                 b_rng_next,     0xFFFFFFFFu },
     { "fighter_slot_flag",        b_slot_flag,    0x000000FFu },
@@ -11937,6 +12511,87 @@ static const binding_t k_bindings[] = {
     { "fighter_39738@cut",                 m_c3c_97_cut,   0xFFFFFFFFu },
     { "fighter_39738@diff",                m_c3c_97_diff,  0xFFFFFFFFu },
     { "fighter_39738@idx",                 m_c3c_97_idx,   0xFFFFFFFFu },
+    { "fighter_4f434",                     b_c3d_4f434,    0x00000000u },
+    { "fighter_4f434@sel",                 m_c3d_4f_sel,   0x00000000u },
+    { "fighter_4f434@scale",               m_c3d_4f_scale, 0x00000000u },
+    { "fighter_4f434@thr",                 m_c3d_4f_thr,   0x00000000u },
+    { "fighter_4f434@dx",                  m_c3d_4f_dx,    0x00000000u },
+    { "fighter_4f434@delta",               m_c3d_4f_delta, 0x00000000u },
+    { "fighter_4f434@call",                m_c3d_4f_call,  0x00000000u },
+    { "fighter_3ad98",                     b_c3d_3ad98,    0x00000000u },
+    { "fighter_3ad98@voice",               m_c3d_ad_voice, 0x00000000u },
+    { "fighter_3ad98@off",                 m_c3d_ad_off,   0x00000000u },
+    { "fighter_3ad98@tab",                 m_c3d_ad_tab,   0x00000000u },
+    { "fighter_3ad98@zero",                m_c3d_ad_zero,  0x00000000u },
+    { "fighter_3ad98@call",                m_c3d_ad_call,  0x00000000u },
+    { "fighter_3ad98@clamp",               m_c3d_ad_clamp, 0x00000000u },
+    { "fighter_3ad98@d",                   m_c3d_ad_d,     0x00000000u },
+    { "fighter_3ad98@bit",                 m_c3d_ad_bit,   0x00000000u },
+    { "fighter_input_mask",                b_c3d_1ab5c,    0xFFFFFFFFu },
+    { "fighter_input_mask@loop",           m_c3d_im_loop,  0xFFFFFFFFu },
+    { "fighter_input_mask@word",           m_c3d_im_word,  0xFFFFFFFFu },
+    { "fighter_input_mask@ok",             m_c3d_im_ok,    0xFFFFFFFFu },
+    { "fighter_input_mask@face",           m_c3d_im_face,  0xFFFFFFFFu },
+    { "fighter_input_mask@eq",             m_c3d_im_eq,    0xFFFFFFFFu },
+    { "fighter_input_mask@bl",             m_c3d_im_bl,    0xFFFFFFFFu },
+    { "fighter_input_mask@arm",            m_c3d_im_arm,   0xFFFFFFFFu },
+    { "fighter_input_mask@s54",            m_c3d_im_s54,   0xFFFFFFFFu },
+    { "fighter_input_mask@call",           m_c3d_im_call,  0xFFFFFFFFu },
+    { "fight_command_map",                 b_c3d_3b134,    0x00000000u },
+    { "fight_command_map@a",               m_c3d_cm_a,     0x00000000u },
+    { "fight_command_map@b",               m_c3d_cm_b,     0x00000000u },
+    { "fight_command_map@r",               m_c3d_cm_r,     0x00000000u },
+    { "fight_command_map@ov",              m_c3d_cm_ov,    0x00000000u },
+    { "fight_command_map@bs",              m_c3d_cm_bs,    0x00000000u },
+    { "fight_command_map@os",              m_c3d_cm_os,    0x00000000u },
+    { "fight_command_map@b1",              m_c3d_cm_b1,    0x00000000u },
+    { "fight_command_map@rd",              m_c3d_cm_rd,    0x00000000u },
+    { "fight_command_map@c",               m_c3d_cm_c,     0x00000000u },
+    { "pset_write",                        b_c3d_2a820,    0x00000000u },
+    { "pset_write@pool",                   m_c3d_pw_pool,  0x00000000u },
+    { "pset_write@x",                      m_c3d_pw_x,     0x00000000u },
+    { "pset_write@lay",                    m_c3d_pw_lay,   0x00000000u },
+    { "pset_write@vis",                    m_c3d_pw_vis,   0x00000000u },
+    { "pset_write@ext",                    m_c3d_pw_ext,   0x00000000u },
+    { "pset_write@dead",                   m_c3d_pw_dead,  0x00000000u },
+    { "pset_write@call",                   m_c3d_pw_call,  0x00000000u },
+    { "fighter_385b0",                     b_c3d_385b0,    0x00000000u },
+    { "fighter_385b0@af8",                 m_c3d_85_af8,   0x00000000u },
+    { "fighter_385b0@arm",                 m_c3d_85_arm,   0x00000000u },
+    { "fighter_385b0@five",                m_c3d_85_five,  0x00000000u },
+    { "fighter_385b0@anim",                m_c3d_85_anim,  0x00000000u },
+    { "fighter_385b0@tail",                m_c3d_85_tail,  0x00000000u },
+    { "fighter_385b0@call",                m_c3d_85_call,  0x00000000u },
+    { "fighter_3ae9c",                     b_c3d_3ae9c,    0x00000000u },
+    { "fighter_3ae9c@s53",                 m_c3d_ae_s53,   0x00000000u },
+    { "fighter_3ae9c@g",                   m_c3d_ae_g,     0x00000000u },
+    { "fighter_3ae9c@sign",                m_c3d_ae_sign,  0x00000000u },
+    { "fighter_3ae9c@tab",                 m_c3d_ae_tab,   0x00000000u },
+    { "fighter_3ae9c@u44",                 m_c3d_ae_u44,   0x00000000u },
+    { "fighter_3ae9c@k",                   m_c3d_ae_k,     0x00000000u },
+    { "fighter_3ae9c@bit",                 m_c3d_ae_bit,   0x00000000u },
+    { "fighter_3b080",                     b_c3d_3b080,    0x00000000u },
+    { "fighter_3b080@gate",                m_c3d_b0_gate,  0x00000000u },
+    { "fighter_3b080@neg",                 m_c3d_b0_neg,   0x00000000u },
+    { "fighter_3b080@mirror",              m_c3d_b0_mirror, 0x00000000u },
+    { "fighter_3b080@p4",                  m_c3d_b0_p4,    0x00000000u },
+    { "fighter_3b080@c148",                m_c3d_b0_c148,  0x00000000u },
+    { "fighter_3b080@o43",                 m_c3d_b0_o43,   0x00000000u },
+    { "fighter_state_36bc8",               b_c3d_36bc8,    0x00000000u },
+    { "fighter_state_36bc8@other",         m_c3d_6b_other, 0x00000000u },
+    { "fighter_state_36bc8@cond",          m_c3d_6b_cond,  0x00000000u },
+    { "fighter_state_36bc8@anim",          m_c3d_6b_anim,  0x00000000u },
+    { "fighter_state_36bc8@s5d",           m_c3d_6b_s5d,   0x00000000u },
+    { "fighter_state_36bc8@bit",           m_c3d_6b_bit,   0x00000000u },
+    { "fighter_state_36bc8@tail",          m_c3d_6b_tail,  0x00000000u },
+    { "fighter_379c4",                     b_c3d_379c4,    0x00000000u },
+    { "fighter_379c4@fe",                  m_c3d_79_fe,    0x00000000u },
+    { "fighter_379c4@s57",                 m_c3d_79_s57,   0x00000000u },
+    { "fighter_379c4@b41",                 m_c3d_79_b41,   0x00000000u },
+    { "fighter_379c4@e8",                  m_c3d_79_e8,    0x00000000u },
+    { "fighter_379c4@cb",                  m_c3d_79_cb,    0x00000000u },
+    { "fighter_379c4@fc",                  m_c3d_79_fc,    0x00000000u },
+    { "fighter_379c4@tab",                 m_c3d_79_tab,   0x00000000u },
 };
 
 static const binding_t *find_binding(const char *name)
diff --git a/tools/diff_verify.py b/tools/diff_verify.py
index 81aa90e..36186cb 100644
--- a/tools/diff_verify.py
+++ b/tools/diff_verify.py
@@ -5593,6 +5593,381 @@ C3C_SPECS = [
        mutants=("@chan", "@k", "@tab", "@k2", "@div", "@cut", "@diff", "@idx")),
 ]
 
+# ---- track P batch C3d (record 2026-10-05-reverse-c3d): the type-family rows, part 4 ------
+
+C3D_REC0, C3D_REC1 = 0x10AF00, 0x10AF40   # the two scratch records (C3c's convention)
+C3D_ACTOR = 0x10B000                      # the actor pool DS_001014EC points at
+
+
+def c3d_slot_pokes():
+    """The 0x1077A8 pointer pair and both slots' record pointers, one poke each (capped at 64
+    bytes). The case helpers add per-slot field pokes, contiguous where the fields are."""
+    return {0x001077A8: le32(C3D_REC0) + le32(C3D_REC1) + le32(C3D_REC0),
+            0x001077B0 + 0x94: le32(C3D_REC1)}
+
+
+def c3d_4f_case(sel, v_sel, v_opp, dx):
+    """0x4F434: the selector byte, the two slot +0x5A bytes and the dx pair. The counter dword at
+    0x1082C8 + opp*4 is 3 and 0x1082C0 + opp*4 is 3 - dx, so dx is exact while the +-1 nudge stays
+    inside the 5-cap of the 0xC9408[3] clamp."""
+    opp = 1 - sel
+    return {0x0010810D: bytes([sel]),
+            0x001077B0 + sel * 0x94 + 0x5A: bytes([v_sel & 0xFF]),
+            0x001077B0 + opp * 0x94 + 0x5A: bytes([v_opp & 0xFF]),
+            0x001082C8 + sel * 4: le32(0x10),
+            0x001082C0 + sel * 4: le32(0),
+            0x001082C8 + opp * 4: le32(3),
+            0x001082C0 + opp * 4: le32(3 - dx),
+            0x0010452C: b"\x03", 0x001082D0: le32(0)}
+
+
+def c3d_85_case(side, s54):
+    """0x385B0: the side's slot +0x54 and every field the body writes, seeded with sentinels."""
+    p = c3d_slot_pokes()
+    so = 0x001077B0 + side * 0x94
+    rec = C3D_REC0 + side * 0x40
+    p[so + 0x0C] = le32(0x33333333) + le32(0x44444444) + le32(0x55555555) + le32(0x66666666)
+    p[so + 0x40] = le32(0xFFFFFFFF)
+    sb = bytearray(0x17)                      # slot +0x52 .. +0x68
+    sb[0x00] = 0x11; sb[0x01] = 0x22; sb[0x02] = s54; sb[0x03] = 0x55
+    sb[0x0D] = 0x11; sb[0x10] = 0x7F; sb[0x13] = 0x88; sb[0x15] = 0x77; sb[0x16] = 0xAA
+    p[so + 0x52] = bytes(sb)
+    p[so + 0x74] = le32(0x99999999) + b"\x00\x00\x03"
+    p[so + 0x84] = le32(0x11111111) + b"\x00\x00\x7F"
+    rb = bytearray(0x26)                      # rec+0x28 .. rec+0x4D
+    rb[0x00] = 0xFF                           # +0x28
+    rb[0x0C:0x0E] = le16(0xDDDD)              # +0x34
+    rb[0x0E:0x10] = le16(0xEEEE)              # +0x36
+    rb[0x1A] = 0x55; rb[0x1B] = 0xCC          # +0x42, +0x43
+    rb[0x1C:0x20] = le32(0xBBBBBBBB)          # +0x44
+    rb[0x24] = 0x99; rb[0x25] = 0x88          # +0x4C, +0x4D
+    p[rec + 0x28] = bytes(rb)
+    p[rec + 0x1C] = le32(0x0F0F0F0F)
+    p[rec + 0x51] = bytes([side, 0, 0, 0, 0]) + le16(1)
+    p[0x001014EC] = le32(C3D_ACTOR)
+    p[C3D_ACTOR] = le16(0x8000)
+    p[0x00100AF8 + side * 4] = le32(0xA5A5A5A5)
+    return p
+
+
+def c3d_6b_case(side, s43, mode, d2c_other):
+    """0x36BC8: the side slot +0x43, the mode word and the other side's 0x107D2C word."""
+    p = c3d_slot_pokes()
+    so = 0x001077B0 + side * 0x94
+    rec = C3D_REC0 + side * 0x40
+    p[so + 0x43] = bytes([s43])
+    p[so + 0x52] = bytes([0x33, 0x44, 0x66, 0x77])
+    p[so + 0x5D] = b"\x77"
+    p[so + 0x74] = le32(0xAAAA5555) + b"\x00\x00\x03"
+    p[rec + 0x18] = le32(0x11223344)
+    p[rec + 0x51] = bytes([side, 0, 0, 0, 0]) + le16(1)
+    p[0x001014EC] = le32(C3D_ACTOR)
+    p[C3D_ACTOR] = le16(0x8000)
+    p[0x00107D2C + (1 - side) * 2] = le16(d2c_other)
+    p[0x00104B00] = le16(mode)
+    return p
+
+
+def c3d_79_case(fe, s57, b41, e8):
+    """0x379C4: the 0x1078FE gate, the slot's +0x57/+0x41, and the 0x1078E8 callback pointer."""
+    p = c3d_slot_pokes()
+    p[0x001077B0 + 0x57] = bytes([s57])
+    p[0x001077B0 + 0x41] = bytes([b41])
+    p[0x001077B0 + 0x7A] = b"\x03"
+    p[C3D_REC0 + 0x51] = b"\x00\x00\x00\x00\x00" + le16(1)
+    p[0x001078FE] = bytes([fe])
+    p[0x001078E8] = le32(e8)
+    return p
+
+
+def c3d_ae_case(side, s53, w34, w36, s40, k44, bit15):
+    """0x3AE9C: the side's slot +0x53/+0x40/+0x7A, the record's +0x34/+0x36/+0x44 word (the k the
+    raw reads is the word at +0x44, the dword at +0x42 shifted) and the side's actor bit 15."""
+    p = c3d_slot_pokes()
+    so = 0x001077B0 + side * 0x94
+    rec = C3D_REC0 + side * 0x40
+    p[so + 0x53] = bytes([s53])
+    p[so + 0x40] = bytes([s40])
+    p[so + 0x7A] = b"\x03"
+    p[rec + 0x34] = le16(w34) + le16(w36)
+    p[rec + 0x44] = le16(k44)
+    p[rec + 0x51] = bytes([side, 0, 0, 0, 0]) + le16(1)
+    p[0x001014EC] = le32(C3D_ACTOR)
+    p[C3D_ACTOR + 1 * 0x20] = le16(bit15 << 15)
+    return p
+
+
+def c3d_b0_case(side, d2, bit15_other, x):
+    """0x3B080: the 0xBEDF2 gate, the other side's actor bit 15 (0x1A570), the side's +0x2C x and
+    the 0xBE018/0xBEDEE window 0x3B038 reads."""
+    p = c3d_slot_pokes()
+    so = 0x001077B0 + side * 0x94
+    oo = 0x001077B0 + (1 - side) * 0x94
+    rec = C3D_REC0 + side * 0x40
+    orec = C3D_REC0 + (1 - side) * 0x40
+    p[so + 0x34] = b"\x11" + b"\x00" * 0x0E + b"\x22"
+    p[oo + 0x34] = b"\x33" + b"\x00" * 0x0E + b"\x44"
+    p[so + 0x2C] = le32(x)
+    p[rec + 0x51] = bytes([side, 0, 0, 0, 0]) + le16(1)
+    p[orec + 0x56] = le16(2)
+    p[0x001014EC] = le32(C3D_ACTOR)
+    p[C3D_ACTOR + (1 + (1 - side)) * 0x20] = le16(bit15_other << 15)
+    p[0x000BEDF2] = bytes([d2])
+    p[0x000BE018] = le32(0x100)
+    p[0x000BEDEE] = le32(0x50 << 16)
+    return p
+
+
+def c3d_ad_case(side, key, d5a, dplus):
+    """0x3AD98: EAX = side, EDX = 0x10B100 (the triple); anim[0] -> 0x10B200 (its +4/+5/+9
+    bytes), anim[2] -> 0x10B280 (the first word selects the stream). The side record's +0x30 high
+    word is 5 and 0x100AD8[other] is 2."""
+    p = c3d_slot_pokes()
+    so = 0x001077B0 + side * 0x94
+    rec = C3D_REC0 + side * 0x40
+    p[0x0010A000] = le32(0x10A400) + le32(0x10A420) + le32(0x10A440)
+    p[0x0010A400 + 4] = bytes([dplus, 5, 0, 0, 0, 9])
+    p[0x0010A440] = le16(key)
+    p[so + 0x5A] = bytes([d5a])
+    p[so + 0x41] = b"\x11"
+    p[so + 0x7A] = b"\x03"
+    p[rec + 0x30] = le32(0x00050000)
+    p[0x00100AD8 + (1 - side) * 4] = le32(2)
+    p[0x000F0AEC] = le32(0x1000)
+    return p
+
+
+def c3d_im_case(side, s53, s54, o5f, o64, orec34, self2c, other2c, ring, word):
+    """0x1AB5C: the side slot's +0x53/+0x54/+0x43/+0x2C, the other's +0x5F/+0x64/+8 and its
+    record's +0x34, the command word 0x1088E0[side] and the ring stub's word (the spec's
+    stub_eax)."""
+    p = c3d_slot_pokes()
+    so = 0x001077B0 + side * 0x94
+    oo = 0x001077B0 + (1 - side) * 0x94
+    orec = C3D_REC0 + (1 - side) * 0x40
+    p[so + 0x53] = bytes([s53])
+    p[so + 0x54] = bytes([s54])
+    p[so + 0x43] = b"\xFF"
+    p[so + 0x2C] = le32(self2c)
+    p[oo + 0x2C] = le32(other2c)
+    p[oo + 0x5F] = bytes([o5f])
+    p[oo + 0x64] = bytes([o64])
+    p[oo + 0x08] = le32(orec if o5f == 0xFF else 0)
+    p[orec + 0x34] = le16(orec34)
+    wb = bytearray(4)
+    wb[side * 2:side * 2 + 2] = le16(word)
+    p[0x001088E0] = bytes(wb)
+    return p
+
+
+def c3d_cm_case(side, s63, s53, s54, self2c, other2c, o5f, o64, orec34, thr, animbits):
+    """0x3B134: the side slot's +0x63/+0x53/+0x54/+0x2C, the other's +0x5F/+0x64/+8 and its
+    record's +0x34, the rng threshold word 0xBEDF4 and the anim[2]+2 byte at 0xA672A (char 0,
+    edx_arg 0; the 0x3AFC4 allow computes the same triple on both sides)."""
+    p = c3d_slot_pokes()
+    so = 0x001077B0 + side * 0x94
+    oo = 0x001077B0 + (1 - side) * 0x94
+    orec = C3D_REC0 + (1 - side) * 0x40
+    p[so + 0x63] = bytes([s63])
+    p[so + 0x53] = bytes([s53])
+    p[so + 0x54] = bytes([s54])
+    p[so + 0x2C] = le32(self2c)
+    p[oo + 0x2C] = le32(other2c)
+    p[oo + 0x5F] = bytes([o5f])
+    p[oo + 0x64] = bytes([o64])
+    p[oo + 0x08] = le32(orec)
+    p[orec + 0x34] = le16(orec34)
+    p[0x0010452C] = b"\x00"
+    p[0x001082C8 + side * 4] = le32(0)
+    p[0x000BEDF4] = le16(thr)
+    p[0x000A672A] = bytes([animbits])
+    return p
+
+
+def c3d_28_case(rec28, rec2c=0, rec40=0x1000, rec2a=0, bdc=0, be0=0, s49=0x09, s59=5):
+    """0x2A820: EAX = rec (0x10AF00), EDX = pset (0x10AA00); the pools at 0x10A800/0x10A900 with
+    one 0x68-byte parent record and its 0x20-byte pset; DS_00105BDC/BE0 seeded for the window."""
+    rb = bytearray(0x5A)                      # rec 0x10AF00 .. +0x59
+    rb[0x18:0x1C] = le32(0x11111111); rb[0x1C:0x20] = le32(0x22222222)
+    rb[0x28:0x2A] = le16(rec28); rb[0x2A:0x2C] = le16(rec2a)
+    rb[0x2C:0x2E] = le16(rec2c)
+    rb[0x32:0x36] = le32(0x00030000); rb[0x34:0x38] = le32(0x00020000)
+    rb[0x40:0x42] = le16(rec40)
+    rb[0x49] = s49; rb[0x4A] = 0x01; rb[0x4B] = 0x02; rb[0x59] = s59
+    pb = bytearray(0x38)                      # parent record 0x10A800 .. +0x57
+    pb[0x18:0x1C] = le32(0x33333333); pb[0x1C:0x20] = le32(0x44444444)
+    pb[0x2C:0x2E] = le16(0x1234); pb[0x30:0x34] = le32(0x00010000)
+    pb[0x56:0x58] = le16(0)
+    pp = bytearray(0x10)                      # parent pset 0x10A900 .. +0x0F
+    pp[4:8] = le32(0x100); pp[8:12] = le32(0x200); pp[0x0E:0x10] = le16(7)
+    qb = bytearray(0x10)                      # the caller's pset 0x10AA00 .. +0x0F
+    qb[0:4] = le32(0xAAAA5555); qb[4:8] = le32(0xBBBBBBBB)
+    qb[8:12] = le32(0xCCCCCCCC); qb[0x0C:0x0E] = le16(0xDDDD)
+    return {0x001014EC: le32(0x10A900), 0x001014F4: le32(0x10A800),
+            0x00105BDC: le32(bdc), 0x00105BE0: le32(be0),
+            0x10AF00: bytes(rb[:0x30]), 0x10AF30: bytes(rb[0x30:0x50]),
+            0x10AF50: bytes(rb[0x50:]),
+            0x10A800: bytes(pb), 0x10A900: bytes(pp), 0x10AA00: bytes(qb),
+            0x10A8D0: b"\x00" * 0x30 + le32(0x00090000), 0x10A8D0 + 0x56: le16(3)}
+
+
+C3D_SPECS = [
+    # 0x4F434 fighter_4f434: the AI difficulty nudge. No args; the 0x46534 callee runs on both
+    # sides (allow, unrecorded), so the accumulator's byte diff is the observation.
+    Spec("fighter_4f434", 0x4F434, [
+        Case("f0", {}, c3d_4f_case(0, 0x3C, 0x00, 0)),    # scaled 50: -1 when dx == 0
+        Case("f1", {}, c3d_4f_case(0, 0x3C, 0x00, 1)),    # dx != 0 -> return
+        Case("f2", {}, c3d_4f_case(0, 0x10, 0x00, 0)),    # scaled 13 -> return
+        Case("f3", {}, c3d_4f_case(0, 0x00, 0x30, 0)),    # scaled -40: +1 when dx == 0
+        Case("f4", {}, c3d_4f_case(0, 0x00, 0x30, 2)),    # dx != 0 -> return
+        Case("f5", {}, c3d_4f_case(0, 0x00, 0x60, 0)),    # scaled -80: +1 when dx <= 1
+        Case("f6", {}, c3d_4f_case(0, 0x00, 0x60, 3)),    # dx > 1 -> return
+        Case("f7", {}, c3d_4f_case(1, 0x3C, 0x00, 0)),    # the other selector
+        Case("f8", {}, c3d_4f_case(0, 0x3B, 0x00, 0)),    # scaled 49 -> return (the threshold)
+    ], allow_calls=(0x46534,), eax_mask=0,
+       mutants=("@sel", "@scale", "@thr", "@dx", "@delta", "@call")),
+    # 0x385B0 fighter_385b0: the mode-0x25 slot reset. EAX = rec; 0x164E8, the 0xC8950 animation
+    # and 0x38154 are stubs; the +0x54 == 5 arm runs 0x38154, every other value the animation.
+    Spec("fighter_385b0", 0x385B0, [
+        Case("b0", {"eax": C3D_REC0}, c3d_85_case(0, 0)),
+        Case("b1", {"eax": C3D_REC0}, c3d_85_case(0, 1)),
+        Case("b2", {"eax": C3D_REC0}, c3d_85_case(0, 2)),
+        Case("b3", {"eax": C3D_REC0}, c3d_85_case(0, 5)),
+        Case("b4", {"eax": C3D_REC1}, c3d_85_case(1, 2)),
+    ], calls=(E.Call(0x164E8, ("eax",), mode="stub"), ANIM_BEGIN,
+              E.Call(0x38154, ("eax",), mode="stub")),
+       eax_mask=0, mutants=("@af8", "@arm", "@five", "@anim", "@tail", "@call")),
+    # 0x3AE9C fighter_3ae9c: EAX = side, DL = param_2. EAX mask 0 (void). The 0x1A570 predicate
+    # runs real (its row exists); 0x33950 is an allow.
+    Spec("fighter_3ae9c", 0x3AE9C, [
+        Case("e0", {"eax": 0, "edx": 0}, c3d_ae_case(0, 7, 0, 0, 0, 0, 0)),
+        Case("e1", {"eax": 0, "edx": 0}, c3d_ae_case(0, 2, 0, 1, 0, 0, 0)),      # g=1, sign 0 -> flip
+        Case("e2", {"eax": 0, "edx": 1}, c3d_ae_case(0, 2, 0, 0, 0x80, 0, 1)),   # bit set, +0x44=0x28
+        Case("e3", {"eax": 0, "edx": 0}, c3d_ae_case(0, 2, 0, 0, 0x80, 0, 1)),   # +0x44=0x3C
+        Case("e4", {"eax": 0, "edx": 1}, c3d_ae_case(0, 2, 5, 0, 0, 0x40, 1)),   # k>0x1E
+        Case("e5", {"eax": 0, "edx": 0}, c3d_ae_case(0, 2, 0xFFFF, 0, 0, 0x10, 1)),  # k<0x1A
+        Case("e6", {"eax": 0, "edx": 0}, c3d_ae_case(0, 2, 0, 0, 0, 0x1C, 1)),   # k in range
+        Case("e7", {"eax": 0, "edx": 0}, c3d_ae_case(0, 2, 0xFF9C, 0, 0, 0, 0)),  # sign -1 multiply
+        Case("e8", {"eax": 1, "edx": 1}, c3d_ae_case(1, 2, 0, 0, 0x80, 0, 0)),    # the other side
+    ], allow_calls=(0x33950,), calls=(E.Call(0x1A570, ("eax",), mode="real"),),
+       eax_mask=0, mutants=("@s53", "@g", "@sign", "@tab", "@u44", "@k", "@bit")),
+    # 0x3B080 fighter_3b080: EAX = side, EDX = param_2, EBX = param_3, ECX = param_4. The 0x1A570
+    # predicate runs real, 0x3C148 is a stub; 0x3B038 is an allow (leaf).
+    Spec("fighter_3b080", 0x3B080, [
+        Case("b0", {"eax": 0, "edx": 3, "ebx": 0x11, "ecx": 1}, c3d_b0_case(0, 1, 0, 0x64)),
+        Case("b1", {"eax": 0, "edx": 3, "ebx": 0x11, "ecx": 0}, c3d_b0_case(0, 0, 0, 0x64)),
+        Case("b2", {"eax": 0, "edx": 3, "ebx": 0x11, "ecx": 0}, c3d_b0_case(0, 0, 1, 0x200)),
+        Case("b3", {"eax": 0, "edx": 3, "ebx": 0x11, "ecx": 1}, c3d_b0_case(0, 0, 0, 0x200)),
+        Case("b4", {"eax": 1, "edx": 4, "ebx": 0x22, "ecx": 1}, c3d_b0_case(1, 0, 0, 0x200)),
+    ], allow_calls=(0x3B038,), calls=(E.Call(0x1A570, ("eax",), mode="real"),
+                                      E.Call(0x3C148, ("eax",), mode="stub")),
+       eax_mask=0, mutants=("@gate", "@neg", "@mirror", "@p4", "@c148", "@o43")),
+    # 0x36BC8 fighter_state_36bc8: EAX = slot, EDX = rec. 0x38BC8 is a stub, 0x38BB0 an allow
+    # (leaf), the 0x2BC30 animation and 0x188AC anchor stubs. Returns 7.
+    Spec("fighter_state_36bc8", 0x36BC8, [
+        Case("c0", {"eax": 0x001077B0, "edx": C3D_REC0}, c3d_6b_case(0, 4, 0, 0)),
+        Case("c1", {"eax": 0x001077B0, "edx": C3D_REC0}, c3d_6b_case(0, 4, 0, 2)),
+        Case("c2", {"eax": 0x001077B0, "edx": C3D_REC0}, c3d_6b_case(0, 0, 0, 2)),
+        Case("c3", {"eax": 0x001077B0, "edx": C3D_REC0}, c3d_6b_case(0, 0, 3, 2)),
+        Case("c4", {"eax": 0x001077B0, "edx": C3D_REC0}, c3d_6b_case(0, 0, 0x22, 2)),
+        Case("c5", {"eax": 0x001077B0 + 0x94, "edx": C3D_REC1}, c3d_6b_case(1, 4, 0, 2)),
+    ], allow_calls=(0x38BB0,), calls=(E.Call(0x38BC8, ("eax",), mode="stub"), ANIM_BEGIN, ANCHOR),
+       eax_mask=0, mutants=("@other", "@cond", "@anim", "@s5d", "@bit", "@tail")),
+    # 0x379C4 fighter_379c4: EAX = slot. 0x37178 and the 0xC8950/0x1078E4/0xC9260 animations are
+    # in the call set; the 0x1078E8 callback runs as an allow (0x45D14 returns 1, the named 0x5D812
+    # returns 0 through the port's NULL-resolve guard, so both sides take the same arm).
+    Spec("fighter_379c4", 0x379C4, [
+        Case("k0", {"eax": 0x001077B0}, c3d_79_case(0, 2, 0, 0)),
+        Case("k1", {"eax": 0x001077B0}, c3d_79_case(0, 3, 0, 0)),
+        Case("k2", {"eax": 0x001077B0}, c3d_79_case(1, 0, 0, 0)),
+        Case("k3", {"eax": 0x001077B0}, c3d_79_case(1, 0, 2, 0)),
+        Case("k4", {"eax": 0x001077B0}, c3d_79_case(1, 0, 2, 0x45D14)),
+        Case("k5", {"eax": 0x001077B0}, c3d_79_case(1, 0, 2, 0x5D812)),
+    ], allow_calls=(0x45D14, 0x5D812), calls=(E.Call(0x37178, ("eax",), mode="stub", clobbers=("esi", "edi", "ebp")), ANIM_BEGIN),
+       eax_mask=0, mutants=("@fe", "@s57", "@b41", "@e8", "@cb", "@fc", "@tab")),
+    # 0x3AD98 fighter_3ad98: EAX = side, EDX = the 3-dword animation triple. The voice, the
+    # 0x2AE14 effect spawn and the 0x2BC30 animation are stubs; 0x392A0 is a stub (its own row).
+    # The spawn's stub EAX is the new actor (0x10B400), so its +0x59/stream writes are compared.
+    Spec("fighter_3ad98", 0x3AD98, [
+        Case("a0", {"eax": 0, "edx": 0x10A000}, c3d_ad_case(0, 1, 0x10, 0x20),
+             {0x2AE14: 0x10A480}),
+        Case("a1", {"eax": 0, "edx": 0x10A000}, c3d_ad_case(0, 2, 0x10, 0x20),
+             {0x2AE14: 0x10A480}),
+        Case("a2", {"eax": 0, "edx": 0x10A000}, c3d_ad_case(0, 3, 0x10, 0x20),
+             {0x2AE14: 0x10A480}),
+        Case("a3", {"eax": 0, "edx": 0x10A000}, c3d_ad_case(0, 0, 0x10, 0x20), {}),
+        Case("a4", {"eax": 0, "edx": 0x10A000}, c3d_ad_case(0, 0, 0x70, 0x20), {}),
+        Case("a5", {"eax": 1, "edx": 0x10A000}, c3d_ad_case(1, 1, 0x10, 0x20),
+             {0x2AE14: 0x10A480}),
+        Case("a6", {"eax": 0, "edx": 0x10A000}, c3d_ad_case(0, 4, 0x10, 0x20), {}),
+        Case("a7", {"eax": 0, "edx": 0x10A000}, c3d_ad_case(0, 0, 0x77, 0x20), {}),
+    ], allow_calls=(0x33A10,),
+       calls=(VOICE, SPAWN, ANIM_BEGIN,
+              E.Call(0x392A0, ("eax", "edx", "ebx"), mode="stub", clobbers=("ebx", "edx"))),
+       eax_mask=0, mutants=("@voice", "@off", "@tab", "@zero", "@call", "@clamp", "@d", "@bit")),
+    # 0x1AB5C fighter_input_mask: EAX = side; the seven 0x46460 ring reads and the command word are
+    # ORed; a non-ok state clears +0x43 bit 4 and returns 0, else the facing base and the +0x54
+    # store/0x1A7CC arm. The ring stub returns the case's 16-bit word on every read; 0x1AB10 and
+    # 0x33A10 are allows.
+    Spec("fighter_input_mask", 0x1AB5C, [
+        Case("i0", {"eax": 0}, c3d_im_case(0, 0, 0, 0x00, 0x00, 0, 5, 3, 0x1000, 0x0000), {0x46460: 0x1000}),
+        Case("i1", {"eax": 0}, c3d_im_case(0, 2, 0, 0x00, 0x00, 0, 5, 3, 0x1000, 0x0000), {0x46460: 0x1000}),
+        Case("i2", {"eax": 0}, c3d_im_case(0, 0, 1, 0x00, 0x00, 0, 3, 5, 0x2000, 0x0000), {0x46460: 0x2000}),
+        Case("i3", {"eax": 0}, c3d_im_case(0, 1, 1, 0x00, 0x00, 0, 4, 4, 0x3000, 0x0000), {0x46460: 0x3000}),
+        Case("i4", {"eax": 0}, c3d_im_case(0, 0, 0, 0xFF, 0x00, 0xFFFF, 4, 4, 0x2000, 0x0000), {0x46460: 0x2000}),
+        Case("i5", {"eax": 0}, c3d_im_case(0, 0, 0, 0xFF, 0x00, 0x0000, 4, 4, 0x1000, 0x0000), {0x46460: 0x1000}),
+        Case("i6", {"eax": 0}, c3d_im_case(0, 1, 0, 0x00, 0x00, 0, 5, 3, 0x4000, 0x0000), {0x46460: 0x4000}),
+        Case("i7", {"eax": 0}, c3d_im_case(0, 0, 0, 0x00, 0x00, 0, 5, 3, 0x0000, 0x0000), {0x46460: 0x0000}),
+        Case("i8", {"eax": 1}, c3d_im_case(1, 0, 0, 0x00, 0x00, 0, 5, 3, 0x8000, 0x1000), {0x46460: 0x8000}),
+    ], allow_calls=(0x33A10, 0x1AB10),
+       calls=(E.Call(0x46460, ("eax", "edx"), mode="stub", clobbers=("edx",)),
+              E.Call(0x18B04, ("eax",), mode="stub"),
+              E.Call(0x1A7CC, ("eax",), mode="stub", clobbers=("esi", "edi", "ebp"))),
+       eax_mask=0xFFFFFFFF, mutants=("@loop", "@word", "@ok", "@face", "@eq", "@bl", "@arm",
+                                     "@s54", "@call")),
+    # 0x3B134 fight_command_map: EAX = side, EDX = edx_arg, ECX = override. 0x33A10/0x3AFC4/
+    # 0x1AB10 are allows; the rng, the readiness gate and the consumer are stubs.
+    Spec("fight_command_map", 0x3B134, [
+        Case("m0", {"eax": 0, "edx": 0, "ecx": 0}, c3d_cm_case(0, 0, 0, 0, 5, 3, 0x00, 0x00, 0, 50, 0), {0x5D7DC: 100}),
+        Case("m1", {"eax": 0, "edx": 0, "ecx": 0}, c3d_cm_case(0, 1, 2, 0, 5, 3, 0x00, 0x00, 0, 50, 0), {0x5D7DC: 100}),
+        Case("m2", {"eax": 0, "edx": 0, "ecx": 0}, c3d_cm_case(0, 1, 0, 0, 5, 3, 0x00, 0x00, 0, 50, 0), {0x5D7DC: 100}),
+        Case("m3", {"eax": 0, "edx": 0, "ecx": 0}, c3d_cm_case(0, 1, 0, 0, 5, 3, 0x00, 0x00, 0xFFFF, 50, 0), {0x5D7DC: 10}),
+        Case("m4", {"eax": 0, "edx": 0, "ecx": 0}, c3d_cm_case(0, 1, 0, 0, 5, 3, 0x00, 0x00, 0x0000, 50, 0), {0x5D7DC: 10}),
+        Case("m5", {"eax": 0, "edx": 0, "ecx": 0}, c3d_cm_case(0, 1, 0, 0, 5, 3, 0xFF, 0xFF, 0, 50, 0), {0x5D7DC: 10}),
+        Case("m6", {"eax": 0, "edx": 0, "ecx": 0}, c3d_cm_case(0, 1, 0, 0, 5, 3, 0xFF, 0xFF, 0, 50, 1), {0x5D7DC: 10}),
+        Case("m7", {"eax": 0, "edx": 0, "ecx": 0}, c3d_cm_case(0, 1, 0, 1, 5, 3, 0xFF, 0xFF, 0, 50, 3), {0x5D7DC: 10, 0x3BDB0: 1, 0x3BDDC: 1}),
+        Case("m8", {"eax": 0, "edx": 0, "ecx": 0}, c3d_cm_case(0, 1, 0, 2, 5, 3, 0xFF, 0xFF, 0, 50, 3), {0x5D7DC: 10}),
+        Case("m9", {"eax": 1, "edx": 0, "ecx": 0}, c3d_cm_case(1, 1, 0, 0, 5, 3, 0x00, 0x00, 0xFFFF, 50, 0), {0x5D7DC: 10}),
+        Case("m10", {"eax": 0, "edx": 0, "ecx": 0}, c3d_cm_case(0, 1, 0, 0, 3, 5, 0x00, 0x00, 0x0000, 50, 0), {0x5D7DC: 10}),
+        Case("m11", {"eax": 0, "edx": 0, "ebx": 1}, c3d_cm_case(0, 1, 0, 0, 5, 3, 0x00, 0x00, 0xFFFF, 50, 0), {0x5D7DC: 100}),
+    ], allow_calls=(0x33A10, 0x3AFC4, 0x1AB10),
+       calls=(E.Call(0x5D7DC, ("eax",), mode="stub"),
+              E.Call(0x3BDB0, ("eax",), mode="stub"),
+              E.Call(0x3BDDC, ("eax",), mode="stub", clobbers=("ebp",))),
+       eax_mask=0, mutants=("@a", "@b", "@r", "@ov", "@bs", "@os", "@b1", "@rd", "@c")),
+    # 0x2A820 pset_write: EAX = rec, EDX = pset. The 0x2A690 free-record writer and the 0x2B150
+    # (set_dead) dead path are stubs; the parent-relative arm and the visibility test run inline.
+    Spec("pset_write", 0x2A820, [
+        Case("p0", {"eax": 0x10AF00, "edx": 0x10AA00}, c3d_28_case(0x0000)),
+        Case("p1", {"eax": 0x10AF00, "edx": 0x10AA00}, c3d_28_case(0x0400)),
+        Case("p2", {"eax": 0x10AF00, "edx": 0x10AA00}, c3d_28_case(0x2000)),
+        Case("p3", {"eax": 0x10AF00, "edx": 0x10AA00}, c3d_28_case(0x2400)),
+        Case("p4", {"eax": 0x10AF00, "edx": 0x10AA00}, c3d_28_case(0x0100, rec2c=5, bdc=0x100, be0=0x200)),
+        Case("p5", {"eax": 0x10AF00, "edx": 0x10AA00},
+             c3d_28_case(0x0000, rec2c=5, rec40=0x1000, bdc=0x5000, be0=0x2000)),
+        Case("p6", {"eax": 0x10AF00, "edx": 0x10AA00},
+             c3d_28_case(0x0080, rec2c=0, rec2a=0x0800, bdc=0x10000, be0=0x10000)),
+        Case("p7", {"eax": 0x10AF00, "edx": 0x10AA00},
+             c3d_28_case(0x0400, bdc=0x10000, be0=0x10000)),
+        Case("p8", {"eax": 0x10AF00, "edx": 0x10AA00}, c3d_28_case(0x2000, s49=0xFF, s59=0x7F)),
+        Case("p9", {"eax": 0x10AF00, "edx": 0x10AA00}, c3d_28_case(0x0440)),
+        Case("p10", {"eax": 0x10AF00, "edx": 0x10AA00}, c3d_28_case(0x0400, s59=0xFF)),
+    ], calls=(E.Call(0x2A690, ("eax",), mode="stub"),
+              E.Call(0x2B150, ("eax",), mode="stub")),
+       unhit_named={0x2A9CE: "the child layer is an 8-bit add (0x2A9B5/0x2A9B8), so its 0xFF clamp cannot fire"},
+       eax_mask=0, mutants=("@pool", "@x", "@lay", "@vis", "@ext", "@dead", "@call")),
+]
+
 SPECS = [
     Spec("rng_next", 0x5D7DC, [
         Case("r1", {"eax": 0x1234}, {DS_RNG: le32(0x12345678)}),
@@ -5635,7 +6010,7 @@ SPECS = [
         Case("d0", {}, {DS_1078FC: b"\x00"}),
         Case("d1", {"eax": U6_REC}, {DS_1078FC: b"\x5a"}),
     ], eax_mask=0),
-] + E3_SPECS + P1_SPECS + P1_ANIM_SPECS + P2_SPECS + P3_SPECS + P6_SPECS + P45_SPECS + C1_SPECS + C2_SPECS + C2B_SPECS + C3_SPECS + C3B_SPECS + P7_SPECS + P8_SPECS + C3C_SPECS
+] + E3_SPECS + P1_SPECS + P1_ANIM_SPECS + P2_SPECS + P3_SPECS + P6_SPECS + P45_SPECS + C1_SPECS + C2_SPECS + C2B_SPECS + C3_SPECS + C3B_SPECS + P7_SPECS + P8_SPECS + C3C_SPECS + C3D_SPECS
 
 
 # ---- driver -------------------------------------------------------------------------------------
diff --git a/tools/tests/test_diff_verify.py b/tools/tests/test_diff_verify.py
index 09b428c..cb40d5f 100644
--- a/tools/tests/test_diff_verify.py
+++ b/tools/tests/test_diff_verify.py
@@ -1293,6 +1293,92 @@ C3C_KINDS = {
     "snd_music_playing@zero": {'eax', 'call #0'},
 }
 
+C3D_MASKS = {
+            "fighter_4f434": 0x0,
+            "fighter_385b0": 0x0,
+            "fighter_3ae9c": 0x0,
+            "fighter_3b080": 0x0,
+            "fighter_state_36bc8": 0x0,
+            "fighter_379c4": 0x0,
+            "fighter_3ad98": 0x0,
+            "fighter_input_mask": 0xffffffff,
+            "fight_command_map": 0x0,
+            "pset_write": 0x0,
+}
+C3D_KINDS = {
+    "fighter_4f434@sel": {'byte'},
+    "fighter_4f434@scale": {'byte'},
+    "fighter_4f434@thr": {'byte'},
+    "fighter_4f434@dx": {'byte'},
+    "fighter_4f434@delta": {'byte'},
+    "fighter_4f434@call": {'byte'},
+    "fighter_385b0@af8": {'byte', 'call #0 memory', 'call #1 memory'},
+    "fighter_385b0@arm": {'byte', 'call #1 memory'},
+    "fighter_385b0@five": {'byte', 'call #1', 'call #1 memory'},
+    "fighter_385b0@anim": {'call #1'},
+    "fighter_385b0@tail": {'byte'},
+    "fighter_385b0@call": {'call #1'},
+    "fighter_3ae9c@s53": {'byte', 'call #0'},
+    "fighter_3ae9c@g": {'byte', 'call #0 memory'},
+    "fighter_3ae9c@sign": {'byte', 'call #0'},
+    "fighter_3ae9c@tab": {'byte', 'call #0 memory'},
+    "fighter_3ae9c@u44": {'byte', 'call #0 memory'},
+    "fighter_3ae9c@k": {'byte', 'call #0 memory'},
+    "fighter_3ae9c@bit": {'byte', 'call #0 memory'},
+    "fighter_3b080@gate": {'byte', 'call #0', 'call #1'},
+    "fighter_3b080@neg": {'byte', 'call #2 memory', 'call #3 memory'},
+    "fighter_3b080@mirror": {'byte', 'call #2', 'call #3'},
+    "fighter_3b080@p4": {'byte', 'call #2', 'call #3'},
+    "fighter_3b080@c148": {'call #1', 'call #2', 'call #3'},
+    "fighter_3b080@o43": {'byte', 'call #2 memory', 'call #3 memory'},
+    "fighter_state_36bc8@other": {'call #0', 'call #1', 'call #2', 'call #2 memory', 'call #3'},
+    "fighter_state_36bc8@cond": {'call #0', 'call #1', 'call #2', 'call #2 memory', 'call #3'},
+    "fighter_state_36bc8@anim": {'call #0', 'call #1', 'call #1 memory', 'call #2'},
+    "fighter_state_36bc8@s5d": {'byte', 'call #2 memory'},
+    "fighter_state_36bc8@bit": {'byte'},
+    "fighter_state_36bc8@tail": {'call #2'},
+    "fighter_379c4@fe": {'byte', 'call #0'},
+    "fighter_379c4@s57": {'call #0'},
+    "fighter_379c4@b41": {'call #0'},
+    "fighter_379c4@e8": {'byte', 'call #0', 'call #0 memory'},
+    "fighter_379c4@cb": {'byte', 'call #1'},
+    "fighter_379c4@fc": {'byte', 'call #0 memory'},
+    "fighter_379c4@tab": {'call #0'},
+    "fighter_3ad98@voice": {'byte', 'call #1', 'call #2', 'call #3'},
+    "fighter_3ad98@off": {'byte', 'call #1', 'call #2', 'call #3'},
+    "fighter_3ad98@tab": {'byte', 'call #1', 'call #2', 'call #3'},
+    "fighter_3ad98@zero": {'byte', 'call #1', 'call #2', 'call #3'},
+    "fighter_3ad98@call": {'byte', 'call #1', 'call #2', 'call #3'},
+    "fighter_3ad98@clamp": {'byte', 'call #1', 'call #2', 'call #3'},
+    "fighter_3ad98@d": {'byte', 'call #1', 'call #2', 'call #3'},
+    "fighter_3ad98@bit": {'byte', 'call #1', 'call #2', 'call #3'},
+    "fighter_input_mask@loop": {'call #6', 'call #6 memory', 'call #7', 'call #7 memory', 'call #8'},
+    "fighter_input_mask@word": {'call #7', 'call #8'},
+    "fighter_input_mask@ok": {'byte', 'call #7', 'call #8', 'eax'},
+    "fighter_input_mask@face": {'byte', 'call #7', 'call #8', 'eax'},
+    "fighter_input_mask@eq": {'eax'},
+    "fighter_input_mask@bl": {'call #7'},
+    "fighter_input_mask@arm": {'call #7'},
+    "fighter_input_mask@s54": {'byte', 'call #7 memory', 'call #8 memory'},
+    "fighter_input_mask@call": {'byte', 'call #7', 'call #8'},
+    "fight_command_map@a": {'call #0'},
+    "fight_command_map@b": {'byte', 'call #0', 'call #1'},
+    "fight_command_map@r": {'byte'},
+    "fight_command_map@ov": {'byte'},
+    "fight_command_map@bs": {'byte'},
+    "fight_command_map@os": {'byte'},
+    "fight_command_map@b1": {'byte'},
+    "fight_command_map@rd": {'call #1', 'call #2'},
+    "fight_command_map@c": {'call #2'},
+    "pset_write@pool": {'byte'},
+    "pset_write@x": {'byte'},
+    "pset_write@lay": {'byte'},
+    "pset_write@vis": {'byte'},
+    "pset_write@ext": {'byte'},
+    "pset_write@dead": {'call #1'},
+    "pset_write@call": {'call #0', 'call #1'},
+}
+
 @needs_unicorn
 @unittest.skipUnless((os.path.exists(DIFFRUN) and os.path.exists(EXE)) or REQUIRED,
                      "build/diffrun or PRAGE.EXE absent")
@@ -1317,7 +1403,7 @@ class RealFunctionTests(unittest.TestCase):
                                              "fighter_slot_flag", "hit_anim_ctx", "hit_anim_start_b",
                                              "host_1b890", "rng_next"] + list(P1_MASKS) + list(P2_MASKS)
                                             + list(P3_MASKS) + list(P45_MASKS) + list(C1_MASKS)
-                                            + list(P6_MASKS) + list(C2_MASKS) + list(C2B_MASKS) + list(C3_MASKS) + list(C3B_MASKS) + list(C3C_MASKS) + list(P7_MASKS) + list(P8_MASKS)))
+                                            + list(P6_MASKS) + list(C2_MASKS) + list(C2B_MASKS) + list(C3_MASKS) + list(C3B_MASKS) + list(C3C_MASKS) + list(C3D_MASKS) + list(P7_MASKS) + list(P8_MASKS)))
         for name, r in self.real.items():
             if name == "host_1b890":       # the named gap (record E3 §E3.8), tested on its own below
                 continue
@@ -1333,7 +1419,7 @@ class RealFunctionTests(unittest.TestCase):
             "fighter_45878@mutant", "fighter_ctx_same@mutant", "fighter_slot_flag@mutant",
             "hit_anim_ctx@mutant", "hit_anim_start_b@mutant", "hit_anim_start_b@set", "rng_next@mutant"]
             + list(P1_KINDS) + list(P2_KINDS) + list(P3_KINDS) + list(P45_KINDS) + list(C1_KINDS)
-            + list(P6_KINDS) + list(C2_KINDS) + list(C2B_KINDS) + list(C3_KINDS) + list(C3B_KINDS) + list(C3C_KINDS) + list(P7_KINDS) + list(P8_KINDS)))
+            + list(P6_KINDS) + list(C2_KINDS) + list(C2B_KINDS) + list(C3_KINDS) + list(C3B_KINDS) + list(C3C_KINDS) + list(C3D_KINDS) + list(P7_KINDS) + list(P8_KINDS)))
         for name, r in self.mut.items():
             self.assertEqual(r.verdict, "MISMATCH", name)
 
@@ -1398,7 +1484,7 @@ class RealFunctionTests(unittest.TestCase):
             "fighter_3640c": 0, "fighter_37dcc": 0,
             "fighter_23130": 0xFF, "fighter_45878": 0, "anim_10fa8": 0, "anim_3e4e4": 0,
             "fighter_ctx_same": 0, "hit_anim_ctx": 0, "hit_anim_start_b": 0, "host_1b890": 0xFFFFFFFF,
-            **P1_MASKS, **P2_MASKS, **P3_MASKS, **P45_MASKS, **C1_MASKS, **P6_MASKS, **C2_MASKS, **C2B_MASKS, **C3_MASKS, **C3B_MASKS, **C3C_MASKS,
+            **P1_MASKS, **P2_MASKS, **P3_MASKS, **P45_MASKS, **C1_MASKS, **P6_MASKS, **C2_MASKS, **C2B_MASKS, **C3_MASKS, **C3B_MASKS, **C3C_MASKS, **C3D_MASKS,
             **P7_MASKS, **P8_MASKS})
         # with the full mask the slot-flag original's scratch bits (case f9: EAX = 0x201) differ
         spec = dataclasses.replace([s for s in V.SPECS if s.name == "fighter_slot_flag"][0],
@@ -2051,6 +2137,89 @@ class RealFunctionTests(unittest.TestCase):
         ):
             self.assertEqual(sorted({p.split(":")[0] for p in self.mut[name].problems}), ids, name)
 
+    def test_each_c3d_mutant_is_caught_by_what_it_breaks(self):
+        # track P batch C3d (record 2026-10-05-reverse-c3d): what alone catches each mutant
+        for name, want in C3D_KINDS.items():
+            got = {p.split(": ", 1)[1].split(":")[0] if p.split(": ", 1)[1].startswith("call #")
+                   else p.split(": ", 1)[1].split(" ")[0] for p in self.mut[name].problems}
+            self.assertEqual(got, want, name)
+        # the exact case set that alone catches each mutant (measured on the prototype)
+        for name, ids in (
+
+        ("fighter_4f434@sel", ['f0', 'f3', 'f5', 'f7']),
+        ("fighter_4f434@scale", ['f0', 'f7']),
+        ("fighter_4f434@thr", ['f8']),
+        ("fighter_4f434@dx", ['f1', 'f4', 'f6']),
+        ("fighter_4f434@delta", ['f0', 'f3', 'f5', 'f7']),
+        ("fighter_4f434@call", ['f0', 'f3', 'f5', 'f7']),
+        ("fighter_385b0@af8", ['b0', 'b1', 'b2', 'b3', 'b4']),
+        ("fighter_385b0@arm", ['b2', 'b3', 'b4']),
+        ("fighter_385b0@five", ['b0', 'b1', 'b2', 'b3', 'b4']),
+        ("fighter_385b0@anim", ['b0', 'b1', 'b2', 'b4']),
+        ("fighter_385b0@tail", ['b0', 'b1', 'b2', 'b4']),
+        ("fighter_385b0@call", ['b3']),
+        ("fighter_3ae9c@s53", ['e0', 'e1', 'e2', 'e3', 'e4', 'e5', 'e6', 'e7', 'e8']),
+        ("fighter_3ae9c@g", ['e1', 'e2', 'e3', 'e4', 'e5', 'e6', 'e7', 'e8']),
+        ("fighter_3ae9c@sign", ['e1', 'e2', 'e3', 'e5', 'e6', 'e7', 'e8']),
+        ("fighter_3ae9c@tab", ['e2', 'e3', 'e4', 'e5', 'e6', 'e7', 'e8']),
+        ("fighter_3ae9c@u44", ['e2', 'e3', 'e8']),
+        ("fighter_3ae9c@k", ['e4', 'e5', 'e6', 'e7']),
+        ("fighter_3ae9c@bit", ['e2', 'e3', 'e8']),
+        ("fighter_3b080@gate", ['b0']),
+        ("fighter_3b080@neg", ['b1', 'b2', 'b3', 'b4']),
+        ("fighter_3b080@mirror", ['b3', 'b4']),
+        ("fighter_3b080@p4", ['b2']),
+        ("fighter_3b080@c148", ['b1', 'b2', 'b3', 'b4']),
+        ("fighter_3b080@o43", ['b1', 'b2', 'b3', 'b4']),
+        ("fighter_state_36bc8@other", ['c0', 'c1', 'c2', 'c3', 'c4', 'c5']),
+        ("fighter_state_36bc8@cond", ['c0', 'c1', 'c2', 'c3', 'c4', 'c5']),
+        ("fighter_state_36bc8@anim", ['c0', 'c1', 'c2', 'c3', 'c4', 'c5']),
+        ("fighter_state_36bc8@s5d", ['c0', 'c1', 'c2', 'c3', 'c4', 'c5']),
+        ("fighter_state_36bc8@bit", ['c0', 'c1', 'c5']),
+        ("fighter_state_36bc8@tail", ['c2']),
+        ("fighter_379c4@fe", ['k0', 'k1', 'k2', 'k3', 'k4', 'k5']),
+        ("fighter_379c4@s57", ['k0', 'k1']),
+        ("fighter_379c4@b41", ['k2']),
+        ("fighter_379c4@e8", ['k3', 'k4', 'k5']),
+        ("fighter_379c4@cb", ['k4']),
+        ("fighter_379c4@fc", ['k5']),
+        ("fighter_379c4@tab", ['k5']),
+        ("fighter_3ad98@voice", ['a0', 'a1', 'a2', 'a3', 'a4', 'a5', 'a6', 'a7']),
+        ("fighter_3ad98@off", ['a0', 'a1', 'a2', 'a3', 'a4', 'a5', 'a6', 'a7']),
+        ("fighter_3ad98@tab", ['a0', 'a1', 'a2', 'a3', 'a4', 'a5', 'a6', 'a7']),
+        ("fighter_3ad98@zero", ['a0', 'a1', 'a2', 'a3', 'a4', 'a5', 'a6', 'a7']),
+        ("fighter_3ad98@call", ['a0', 'a1', 'a2', 'a3', 'a4', 'a5', 'a6', 'a7']),
+        ("fighter_3ad98@clamp", ['a0', 'a1', 'a2', 'a3', 'a4', 'a5', 'a6', 'a7']),
+        ("fighter_3ad98@d", ['a0', 'a1', 'a2', 'a3', 'a4', 'a5', 'a6', 'a7']),
+        ("fighter_3ad98@bit", ['a0', 'a1', 'a2', 'a3', 'a4', 'a5', 'a6', 'a7']),
+        ("fighter_input_mask@loop", ['i0', 'i1', 'i2', 'i3', 'i4', 'i5', 'i6', 'i7', 'i8']),
+        ("fighter_input_mask@word", ['i8']),
+        ("fighter_input_mask@ok", ['i1']),
+        ("fighter_input_mask@face", ['i0', 'i2', 'i6', 'i7', 'i8']),
+        ("fighter_input_mask@eq", ['i3', 'i4', 'i5']),
+        ("fighter_input_mask@bl", ['i4', 'i5']),
+        ("fighter_input_mask@arm", ['i7']),
+        ("fighter_input_mask@s54", ['i2', 'i3', 'i4', 'i6']),
+        ("fighter_input_mask@call", ['i0', 'i2', 'i3', 'i4', 'i5', 'i6', 'i8']),
+        ("fight_command_map@a", ['m0']),
+        ("fight_command_map@b", ['m1', 'm8']),
+        ("fight_command_map@r", ['m2']),
+        ("fight_command_map@ov", ['m11']),
+        ("fight_command_map@bs", ['m5', 'm6']),
+        ("fight_command_map@os", ['m10', 'm11', 'm3', 'm4', 'm9']),
+        ("fight_command_map@b1", ['m6']),
+        ("fight_command_map@rd", ['m7']),
+        ("fight_command_map@c", ['m7']),
+        ("pset_write@pool", ['p1', 'p10', 'p7', 'p9']),
+        ("pset_write@x", ['p1', 'p10', 'p7', 'p9']),
+        ("pset_write@lay", ['p8']),
+        ("pset_write@vis", ['p4']),
+        ("pset_write@ext", ['p5']),
+        ("pset_write@dead", ['p6']),
+        ("pset_write@call", ['p0', 'p4', 'p5', 'p6']),
+        ):
+            self.assertEqual(sorted({p.split(":")[0] for p in self.mut[name].problems}), ids, name)
+
     def test_each_p7_mutant_is_caught_by_what_it_breaks(self):
         # track P batch 7 (record 2026-10-04-reverse-p7): what alone catches each mutant
         for name, want in P7_KINDS.items():
@@ -2072,36 +2241,31 @@ class RealFunctionTests(unittest.TestCase):
         # 0x2B150/0x2BD44/0x3B298 carry the caller-observed sets (record §C2b.3): the byte-derived
         # transitive scan over-approximates the Watcom callee-saved registers their callers keep
         # live (E.callee_clobbers' _CALLEE_CLOBBER_FIXES).
-        self.assertEqual(stubs, {0x13244: (), 0x13420: (), 0x13C70: ("ebx", "edx"), 0x13DF0: (), 0x164E8: (),
-                                 0x18350: ("edx",), 0x18540: (), 0x186D0: (), 0x18714: (), 0x18788: (),
-                                 0x187FC: (), 0x1881C: (), 0x1883C: ("ebx", "edx"), 0x188AC: ("edx",),
-                                 0x188DC: ("edx",), 0x1890C: ("edx",), 0x189FC: (), 0x18A4C: (),
-                                 0x18AF8: ("ebx", "ecx", "edx"), 0x18B04: (), 0x18B44: (),
-                                 0x18C14: ("ebx", "edx", "ebp"), 0x1A570: (), 0x1A5AC: (), 0x1A734: (),
-                                 0x1AB5C: ("ebp",), 0x1CA14: ("edx",), 0x1CA40: (), 0x1CA6C: (), 0x1CC28: ("edx",),
-                                 0x1CD9C: (), 0x1CE04: (), 0x1CE70: (), 0x1D238: (), 0x1D244: (),
-                                 0x1DDF4: ("ebx", "edx"), 0x22404: (), 0x23960: (), 0x249B0: (), 0x249D0: (),
-                                 0x29BC8: ("ebx", "edx"), 0x29C08: ("edx",), 0x29DB8: ("ebx", "edx"),
-                                 0x29F34: ("edx",), 0x2A148: ("edx",), 0x2A17C: ("edx",), 0x2A408: ("edx",),
-                                 0x2A620: ("edx",), 0x2A820: ("edx",), 0x2AC80: (), 0x2AD40: ("edx", "edi", "ebp"),
-                                 0x2AE14: ("ebx", "ecx", "edx"), 0x2B150: (), 0x2B2A0: ("ebx", "edx"),
-                                 0x2BC30: ("edx",), 0x2BCF4: ("edx",), 0x2BD44: ("edx",), 0x2C3FC: (),
-                                 0x33754: (), 0x33864: (), 0x34D8C: (), 0x35838: ("ebx", "edx"),
-                                 0x365C8: ("ebx", "edx"), 0x36638: ("edx",), 0x36870: ("esi", "edi", "ebp"),
-                                 0x36BC8: ("edx",), 0x36CE4: (), 0x36D98: (), 0x379C4: ("ecx", "esi", "edi", "ebp"),
-                                 0x37D18: ("edx",), 0x38034: (), 0x38154: (), 0x385B0: (), 0x39040: (),
-                                 0x39280: (), 0x392A0: ("ebx", "edx"), 0x39738: ("edx",),
-                                 0x39834: ("edx", "ebp"), 0x39A10: ("edx",), 0x39EFC: (), 0x39F40: ("ebx", "edx"),
-                                 0x39FB0: (), 0x3A95C: ("edx",), 0x3A9D8: ("edx",), 0x3AA54: (),
-                                 0x3AAFC: ("ebx", "ecx", "edx", "edi", "ebp"), 0x3AD98: ("edx",), 0x3AE9C: ("edx",),
-                                 0x3B080: ("ebx", "ecx", "edx"), 0x3B134: ("ebx", "edx", "edi", "ebp"),
-                                 0x3B298: ("edx",), 0x3B6C4: (), 0x3B714: ("edx",), 0x3B8D8: ("edx",),
-                                 0x3B90C: ("edx",), 0x3C148: (), 0x3C16C: (), 0x3C190: ("edx",),
-                                 0x3C208: ("edx",), 0x3C358: (), 0x3C480: ("edx",), 0x3C4CC: ("edx",),
-                                 0x3C520: ("edx",), 0x3C59C: ("edx",), 0x41310: (), 0x46460: ("edx",),
-                                 0x468D8: (), 0x48170: (), 0x49444: ("edi", "ebp"), 0x4F434: (), 0x5D7DC: (), 0x5DC0F: ("ebx", "ecx", "edx"),
-                                 0x5DC8B: ("ebx", "ecx", "edx"), 0x5DD03: (),
-                                 0x5DEAF: ("ebx", "ecx", "edx"), 0x5DEED: ()})
+        self.assertEqual(stubs, {
+        0x13244: (), 0x13420: (), 0x13C70: ("ebx", "edx"), 0x13DF0: (), 0x164E8: (), 0x18350: ("edx",),
+        0x18540: (), 0x186D0: (), 0x18714: (), 0x18788: (), 0x187FC: (), 0x1881C: (), 0x1883C: ("ebx", "edx"),
+        0x188AC: ("edx",), 0x188DC: ("edx",), 0x1890C: ("edx",), 0x189FC: (), 0x18A4C: (),
+        0x18AF8: ("ebx", "ecx", "edx"), 0x18B04: (), 0x18B44: (), 0x18C14: ("ebx", "edx", "ebp"), 0x1A570: (),
+        0x1A5AC: (), 0x1A734: (), 0x1A7CC: ("esi", "edi", "ebp"), 0x1AB5C: ("ebp",), 0x1CA14: ("edx",),
+        0x1CA40: (), 0x1CA6C: (), 0x1CC28: ("edx",), 0x1CD9C: (), 0x1CE04: (), 0x1CE70: (), 0x1D238: (),
+        0x1D244: (), 0x1DDF4: ("ebx", "edx"), 0x22404: (), 0x23960: (), 0x249B0: (), 0x249D0: (),
+        0x29BC8: ("ebx", "edx"), 0x29C08: ("edx",), 0x29DB8: ("ebx", "edx"), 0x29F34: ("edx",),
+        0x2A148: ("edx",), 0x2A17C: ("edx",), 0x2A408: ("edx",), 0x2A620: ("edx",), 0x2A690: (),
+        0x2A820: ("edx",), 0x2AC80: (), 0x2AD40: ("edx", "edi", "ebp"), 0x2AE14: ("ebx", "ecx", "edx"),
+        0x2B150: (), 0x2B2A0: ("ebx", "edx"), 0x2BC30: ("edx",), 0x2BCF4: ("edx",), 0x2BD44: ("edx",),
+        0x2C3FC: (), 0x33754: (), 0x33864: (), 0x34D8C: (), 0x35838: ("ebx", "edx"), 0x365C8: ("ebx", "edx"),
+        0x36638: ("edx",), 0x36870: ("esi", "edi", "ebp"), 0x36BC8: ("edx",), 0x36CE4: (), 0x36D98: (),
+        0x37178: ("esi", "edi", "ebp"), 0x379C4: ("ecx", "esi", "edi", "ebp"), 0x37D18: ("edx",), 0x38034: (),
+        0x38154: (), 0x385B0: (), 0x38BC8: (), 0x39040: (), 0x39280: (), 0x392A0: ("ebx", "edx"),
+        0x39738: ("edx",), 0x39834: ("edx", "ebp"), 0x39A10: ("edx",), 0x39EFC: (), 0x39F40: ("ebx", "edx"),
+        0x39FB0: (), 0x3A95C: ("edx",), 0x3A9D8: ("edx",), 0x3AA54: (),
+        0x3AAFC: ("ebx", "ecx", "edx", "edi", "ebp"), 0x3AD98: ("edx",), 0x3AE9C: ("edx",),
+        0x3B080: ("ebx", "ecx", "edx"), 0x3B134: ("ebx", "edx", "edi", "ebp"), 0x3B298: ("edx",), 0x3B6C4: (),
+        0x3B714: ("edx",), 0x3B8D8: ("edx",), 0x3B90C: ("edx",), 0x3BDB0: (), 0x3BDDC: ("ebp",), 0x3C148: (),
+        0x3C16C: (), 0x3C190: ("edx",), 0x3C208: ("edx",), 0x3C358: (), 0x3C480: ("edx",), 0x3C4CC: ("edx",),
+        0x3C520: ("edx",), 0x3C59C: ("edx",), 0x41310: (), 0x46460: ("edx",), 0x468D8: (), 0x48170: (),
+        0x49444: ("edi", "ebp"), 0x4F434: (), 0x5D7DC: (), 0x5DC0F: ("ebx", "ecx", "edx"),
+        0x5DC8B: ("ebx", "ecx", "edx"), 0x5DD03: (), 0x5DEAF: ("ebx", "ecx", "edx"), 0x5DEED: ()})
         for addr, declared in stubs.items():
             self.assertEqual(E.callee_clobbers(img, addr), declared, hex(addr))
 
@@ -2196,8 +2360,8 @@ class RealFunctionTests(unittest.TestCase):
                          "--self-check"])
         self.assertEqual(rc, 0)
         # the closed-row count is over the rows that have callees (185), the 49 without are counted apart
-        self.assertIn("diff-verify: 251/251 functions VERIFIED; 874/874 mutants detected; 1 named gaps; "
-                      "178/195 rows with callees closed (56 have none).", out.getvalue())
+        self.assertIn("diff-verify: 261/261 functions VERIFIED; 945/945 mutants detected; 1 named gaps; "
+                      "181/205 rows with callees closed (56 have none).", out.getvalue())
 
 
     def test_each_c3c_mutant_is_caught_by_what_it_breaks(self):
```
