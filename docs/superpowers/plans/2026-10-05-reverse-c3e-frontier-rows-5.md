# C3e: the frontier rows, part 5 (track P, batch C3e) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add the differential-verification row for each of the **seventeen addresses** this batch
measures — the three remaining type rows (`0x39040` `fighter_39040`, `0x392A0` `fighter_392a0`,
`0x3AAFC` `fighter_reaction_apply`) and the fourteen tail leaves that close or advance the chain
(`0x3A280`, `0x4F944`, `0x33A68` `fighter_ctx_rec_swap`, `0x36E78`, `0x2CAA8` `config_not_free_play`,
`0x2D974` `config_field_get`, `0x468D8` `ai_pred_468d8`, `0x46190`, `0x36D20`, the four pose setters
`0x3A504`/`0x3A650`/`0x3A79C`/`0x3A8E8` and `0x3A0FC`) — with their bindings, mutants and
exact-set pins; **two raw-over-port corrections** (`0x3AAFC`'s second anim triple and `0x3A280`'s
u8 cast) and the exports/seams their mutant cores need. **The tail verdict** (§C3e.7, required):
after this batch the remaining frontier is exactly the twelve first-wave addresses the other
frontier rows wait on plus the 0x38D90/0x38FEC trees — the C3f list with evidence.

**Architecture:** Each row is a `Spec` in `tools/diff_verify.py` (`C3E_SPECS` with its `c3e_*`
fixtures), a `b_*` binding and `m_*` mutants in `port/tests/diff_runner.c` (`k_bindings`), and
exact-set assertions in `tools/tests/test_diff_verify.py` (`C3E_MASKS`, `C3E_KINDS`, the case-set
test, the clobber and counter lines). `port/src` changes: the `0x3AAFC` and `0x3A280` corrections,
nine `static` removals with `fighter.h` declarations (`fighter_ctx_rec_swap`, `fighter_46190`,
`fighter_36e78`, `fighter_36d20`, `fighter_3a0fc`, the four pose setters — the mutant cores call
them), and two seams on the config pair (`config_field_get`, `config_not_free_play`) so
`fighter_46190`'s mode-real callees are recorded on the port side.

**Tech Stack:** C11 (`port/src`, `port/tests`), Python 3.12 (`unittest`), `unicorn` 2.1.4 and
`capstone` 5.0.7 (`tools/requirements-diff.txt`), CMake, `make`.

**Spec:** `docs/superpowers/specs/2026-09-30-reverse-completion-design.md` §4 track P, §5.1-§5.3,
§6 ("each ported function has a verification row; unhit blocks and non-emulable instructions named;
counters and README percentage updated"), §7.

**Derivation record:** `docs/superpowers/plans/2026-10-05-reverse-c3e-derivations.md` (§C3e.1 the
members, §C3e.2 the rows, fixtures and the two corrections, §C3e.3 the mutants and their measured
catching/kinds sets, §C3e.4 the counters, §C3e.5 the named gaps and limits, §C3e.6 what the planner
ran, §C3e.7 **the tail verdict and the C3f list**). Recipe: `2026-10-01-reverse-e3-derivations.md`
§E3.10; checklist: `.superpowers/sdd/2026-10-01-plans/p-track-review-checklist.md`.

**What the planner ran (scratch, 2026-10-07, on `reverse-c3e` at `main` `c28e522` = C3d merged;
image sha1 `ff3b8cb14e00f1c282de7b7e15dcd7c230766947`):** a full prototype of all seventeen rows. The
three rows first (with the `0x3AAFC` correction, which case `a0` found), then the tail family by
family. Every row was measured with `--function NAME --self-check` until VERIFIED with every mutant
detected, then the full `python3 tools/diff_verify.py --self-check`
(`278/278 functions VERIFIED; 1031/1031 mutants detected; 1 named gaps; 196/217 rows with callees
closed (61 have none)`), `python3 -m unittest tools.tests.test_diff_verify` (`109 tests OK` with
the extended exact-set tables), `make entry-triage` (byte-identical: `targets 233 unported, 262
ported; supplement 131 (3 unported, 0 stale); untrusted 30`; voice `0 / 115 / 19`),
`PR_ORACLE_REQUIRED=1 ./build/run_tests` (`all checks passed`), `make gp-oracle` (every ratchet
`ok`, N 2064 / trace 8320 at their pins) and `python3 tools/port_progress.py`
(`771 1203 64` / `731 731 100`). Then the prototype was reverted (`git checkout -- port tools`);
the patch below is the exact diff it applied (1797 lines, 1542 insertions over six files).

**Re-baseline note.** The counters below are `c28e522`'s. If `main` moves before this plan executes,
Task 1 records the measured base and every later expected counter adds this plan's increments:
functions +17, mutants +86, rows with callees +12, closed rows +15, no-callee rows +5. The E2 table
must not move (no E2 candidate; no `fn_register`): if a task regenerates it, the line must be
byte-identical.

## Decisions needed from the user

**None.** The two raw-over-port corrections are derived from the raw with their addresses
(§C3e.2); the tail that does not fit is deferred to C3f with the measured list and evidence
(§C3e.7), exactly as the C3d record's §C3d.7 anticipated.

## The C3e roadmap

| task | what | commit |
|---|---|---|
| 1 | baseline on `c28e522` (no commit) | - |
| 2 | the seventeen rows: the corrections, the exports/seams, bindings, mutants and the test exact-set updates | `tools:` |
| 3 | the review sweep (stores, clobbers, mutant case sets) | `tests:` (only if a fix) |
| 4 | closure: PROGRESS, the record's §C3e.8, the final gates and the tail-verdict re-measure | `docs:` |

## Global Constraints

Copied verbatim from their sources:

- AGENTS.md: "**Never ship a fitted constant.** Every value is derived from the raw bytes or a
  capture, with the address that proves it. A value that cannot be pinned is a **named gap with its
  evidence** — never a plausible-looking number." / "**On any plan-vs-raw conflict the raw wins.**
  Record the correction and the address, in the derivation record and the report."
- Spec §8: "`make verify` is the gate after every task." User ruling (2026-10-01, speed-up): the
  per-task gate is the task's tests, `make diff-verify`, `make entry-triage` and the gp oracles
  whose miss sets the task touches; the full `make verify` runs at the baseline, the final task and
  before the merge. Here the task-scoped gates (the brief): `make diff-verify`,
  `make entry-triage`, `PR_ORACLE_REQUIRED=1 ./build/run_tests`, and
  `python3 -m unittest tools.tests.test_diff_verify`.
- AGENTS.md: "**The E2 target list is a gate** ... A commit that ports a target (a new `/* 0xADDR`
  header or `fn_register`) regenerates the table in the same commit (decision D3)". This plan ports
  no target: the table must be byte-identical.
- AGENTS.md: "One C function per original function, header comment `/* 0xADDR — spec section */`." /
  "Mark deliberate deviations `/* PORT: ... */`; doubts `/* TODO(verify): ... */`. No other comment
  styles in `port/src`." / "Code addresses stored in data go through `fn_origin()` /
  `fn_resolve()`." / "**`port/src/symbols.h` is generated** ... Never hand-edit it."
- AGENTS.md: "A ported function that a differentially verified function stubs or records ... opens
  with `PR_SEAM(0xADDR, args...)` (void) or `PR_SEAM_RET(0xADDR, args...)` ... The arguments are the
  C signature's, in order." Record E3 §E3.8: "A seam must be the callee's first statement".
- AGENTS.md: "Only `CHECK(cond,msg)` and `CHECK_EQ_INT(a,b)`." / "**Assertions must be able to
  fail.**" / "Consolidating must not change an assertion": this plan extends the exact-set
  assertions of `RealFunctionTests` (rows, masks, mutant names, stub clobbers, the counter line)
  and adds the C3e case-set test; no other assertion changes.
- AGENTS.md: "`data/` is git-ignored and read-only — never write to it." / "Commit style:
  `<area>: <what changed>`. **Never `git add -A`** — stage named files." No `Co-Authored-By`
  trailer. Never run `make gp-capture`.
- The C3e brief: the gp miss sets must stay as Task 1 measures them. The `0x3AAFC` correction
  changes the winner's reaction path; `make gp-oracle`'s ratchets are at their pins on the
  prototype, and Task 4's full `make verify` is the batch's evidence that no oracle moved.
- Spec §5.3 / E3 §E3.4: "VERIFIED" is equivalence on the exercised blocks and inputs only, each
  function with its callees stubbed or run as stated.

## Where to run

The worktree `.worktrees/reverse-c3e` (branch `reverse-c3e`). Every command runs from the worktree
root; `data/` and `.superpowers/` are symlinks; `build/` exists. Before Task 1:

```bash
git rev-parse HEAD   # c28e522
python3 -c "import unicorn, capstone; print(unicorn.__version__, capstone.__version__)"  # 2.1.4 5.0.7
cmake -S port -B build && cmake --build build 2>&1 | tail -1
./build/diffrun --exe data/game/C/PRAGE.EXE --image-out /tmp/pr_c3e_img.bin && shasum /tmp/pr_c3e_img.bin
ls port/tests/ghidra_data.bin   # the PR_ORACLE_REQUIRED fixture (git-ignored); copy it from the main repo if absent
ls data/k11-captures/gp-idle-loss   # the gp-oracle capture (git-ignored)
```

The image line must print `ff3b8cb14e00f1c282de7b7e15dcd7c230766947`; another hash means the image
differs from the one the record measured: stop. Ledger:
`.superpowers/sdd/2026-10-05-reverse-c3e/progress.md`.

## The seventeen rows (the brief's grouping, executed as families)

The patch is one `git diff` (Task 2 applies it at once); the families below are the gate order:

| family | rows | shared seams/fixtures |
|---|---|---|
| the three type rows | `0x39040` `0x392A0` `0x3AAFC` | the C3d seams (`0x38D90`, `0x38FEC`, `0x46190`, `0x36E78`, `0x3A0FC`, `0x36D20`, the four pose setters) and the `0x4F944`/`0x33A68`/`0x3A280` leaf allows |
| the small leaves | `0x3A280` `0x4F944` `0x33A68` `0x36E78` `0x2CAA8` `0x2D974` `0x468D8` `0x46190` `0x36D20` | the two new config seams; `c3e_6e_case`, `c3e_8d_case`, `c3e_cf_case`, `c3e_90b_case`, `c3e_36d_case` |
| the pose family and `0x3A0FC` | `0x3A504` `0x3A650` `0x3A79C` `0x3A8E8` `0x3A0FC` | the shared `fighter_pose_commit` body and the `c3e_pose_case`/`c3e_0fc_case` fixtures |

The measured rows (cases/blocks/mutants measured; `EAX mask` from the Spec):

| row | entry | cases | blocks | mutants | EAX mask | named unhit / notes |
|---|---|---|---|---|---|---|
| `fighter_39040` | 0x39040 | 17 | 33/33 | 11/11 | 0 | — |
| `fighter_392a0` | 0x392A0 | 18 | 42/42 | 11/11 | 0 | — |
| `fighter_reaction_apply` | 0x3AAFC | 13 | 28/28 | 12/12 | 0 | the `0x3AAFC` correction read |
| `fighter_3a280` | 0x3A280 | 10 | 6/6 | 2/2 | 0xFF | the raw's full-EAX compares (r8/r9) |
| `fighter_4f944` | 0x4F944 | 7 | 3/3 | 4/4 | 0 | — |
| `fighter_ctx_rec_swap` | 0x33A68 | 4 | 1/1 | 3/3 | 0 | — |
| `fighter_36e78` | 0x36E78 | 5 | 5/5 | 4/4 | 0 | — |
| `config_not_free_play` | 0x2CAA8 | 3 | 1/1 | 3/3 | 0xFF | — |
| `ai_pred_468d8` | 0x468D8 | 7 | 8/8 | 4/4 | 0xFF | — |
| `config_field_get` | 0x2D974 | 9 | 12/12 | 5/5 | 0xFFFFFFFF | — |
| `fighter_46190` | 0x46190 | 5 | 5/5 | 3/3 | 0xFF | the config pair runs real |
| `fighter_36d20` | 0x36D20 | 6 | 8/8 | 5/5 | 0xFF | the raw inlines `0x36CE4`; the port calls it (unrecorded) |
| `fighter_pose_3a504` | 0x3A504 | 2 | 1/1 | 3/3 | 0 | EBX = the slot's +0x52 (call convention) |
| `fighter_pose_3a650` | 0x3A650 | 2 | 1/1 | 3/3 | 0 | the same |
| `fighter_pose_3a79c` | 0x3A79C | 2 | 1/1 | 3/3 | 0 | the same |
| `fighter_pose_3a8e8` | 0x3A8E8 | 2 | 1/1 | 3/3 | 0 | the same |
| `fighter_3a0fc` | 0x3A0FC | 9 | 28/28 | 7/7 | 0 | the 0x105B3A table's 0..2 all spawn |

No row reads outside the image (the `0x3A0FC` actor-spawn stub EAX keeps the +0x59 stores in the
image; record §C3e.2 correction 3).

## How the code steps are written

Task 2's patch is the prototype's exact `git diff` (the planner applied it, ran every gate, and
reverted). Apply it with `git apply`; it touches `port/src/game/fighter.c`, `port/src/game/fighter.h`,
`port/src/game/config.c`, `port/tests/diff_runner.c`, `tools/diff_verify.py` and
`tools/tests/test_diff_verify.py`. If `main` moved, `git apply --3way` and resolve by keeping the
patch's additions at the same anchors; never merge the E2 table by hand.

## File Structure

| File | Responsibility |
|---|---|
| `port/src/game/fighter.c`/`.h` | the two corrections, the eight exports and their declarations |
| `port/src/game/config.c` | the `0x2D974`/`0x2CAA8` seams |
| `port/tests/diff_runner.c` | the seventeen bindings and 86 mutants, the `c3e_*` cores |
| `tools/diff_verify.py` | `C3E_SPECS` (the seventeen rows and their fixtures) |
| `tools/tests/test_diff_verify.py` | `C3E_MASKS`, `C3E_KINDS`, the case-set test, the clobber and counter lines |
| `docs/superpowers/plans/2026-10-05-reverse-c3e-derivations.md` | the record (this plan's source of values) |
| `docs/PROGRESS.md` | Task 4's paragraph |

---

### Task 1: Baseline (no commit)

**Files:** none. **Interfaces:** consumes the worktree at `c28e522`; produces the baseline counters
and table.

- [ ] **Step 1: the task gates on the untouched tree.**

```bash
make diff-verify DIFF_IMAGE=/tmp/pr_c3e_base.bin DIFF_TABLE=/tmp/pr_c3e_base_table.md 2>&1 | tail -1
make entry-triage E2_IMAGE=/tmp/pr_c3e_base_e2.bin 2>&1 | tail -2
python3 tools/port_progress.py
```

Expected (measured at `c28e522` by the planner):

```
diff-verify: 261/261 functions VERIFIED; 945/945 mutants detected; 1 named gaps; 181/205 rows with callees closed (56 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
entry-triage: targets 233 unported, 262 ported; supplement 131 (3 unported, 0 stale); untrusted entries 30
entry-triage: voice sites outside Ghidra 134: 0 in unported code, 115 in ported code, 19 nowhere
771 1203 64
731 731 100 (portable: excludes 81 host-owned/deferred and runtime >= 5D000)
```

- [ ] **Step 2: pin the gp miss sets and the oracle WAV.** The `PR_GP_DUMP` scenarios' pinned sets
  in `test_platform.c` must be the ones Task 2 leaves untouched; the `0x3AAFC` correction is the
  batch's behavioral change and `make gp-oracle` (planner: N 2064 / trace 8320, every ratchet `ok`)
  plus Task 4's full `make verify` are the evidence.

### Task 2: the seventeen rows (one commit)

**Files:** the patch below. **Interfaces:** produces `C3E_SPECS`, the seventeen bindings and 86
mutants, the two corrections, the exports and the two config seams, and the test exact-set updates;
consumes the C1-C3d fixtures and the seam table.

- [ ] **Step 1: apply the patch.** `git apply - <<'PATCH' ... PATCH` with the diff embedded at the
  end of this task (the exact prototype diff; 1797 lines, 1542 insertions over six files).

- [ ] **Step 2: build and run the three type rows' self-checks.**

```bash
cmake --build build 2>&1 | tail -1
for f in fighter_39040 fighter_392a0 fighter_reaction_apply; do
  python3 tools/diff_verify.py --function "$f" --self-check
done
```

Expected rows (measured): `fighter_39040` 17 cases, 33/33 blocks, 11/11 mutants; `fighter_392a0`
18, 42/42, 11/11; `fighter_reaction_apply` 13, 28/28, 12/12. (The per-row run reports the callee
column as `unverified` for every callee whose own row is not run; only the full run's table reads
`VERIFIED`. Record E3 §E3.8.)

- [ ] **Step 3: the small leaves' self-checks.**

```bash
for f in fighter_3a280 fighter_4f944 fighter_ctx_rec_swap fighter_36e78 config_not_free_play \
         ai_pred_468d8 config_field_get fighter_46190 fighter_36d20; do
  python3 tools/diff_verify.py --function "$f" --self-check
done
```

Expected rows (measured): `3a280` 10/6/2, `4f944` 7/3/4, `ctx_rec_swap` 4/1/3, `36e78` 5/5/4,
`config_not_free_play` 3/1/3, `ai_pred_468d8` 7/8/4, `config_field_get` 9/12/5, `fighter_46190`
5/5/3, `fighter_36d20` 6/8/5 (cases/blocks/mutants).

- [ ] **Step 4: the pose family and `0x3A0FC`'s self-checks.**

```bash
for f in fighter_pose_3a504 fighter_pose_3a650 fighter_pose_3a79c fighter_pose_3a8e8 fighter_3a0fc; do
  python3 tools/diff_verify.py --function "$f" --self-check
done
```

Expected rows (measured): each pose setter 2 cases, 1/1 blocks, 3/3 mutants; `fighter_3a0fc`
9 cases, 28/28 blocks, 7/7 mutants.

- [ ] **Step 5: the task gates.**

```bash
make diff-verify DIFF_IMAGE=/tmp/pr_c3e_after.bin DIFF_TABLE=/tmp/pr_c3e_after_table.md 2>&1 | tail -1
python3 -m unittest tools.tests.test_diff_verify 2>&1 | tail -3
make entry-triage E2_IMAGE=/tmp/pr_c3e_after_e2.bin 2>&1 | tail -2
PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -1
python3 tools/gen_symbols.py port/decomp /tmp/pr_c3e_sym.h && diff /tmp/pr_c3e_sym.h port/src/symbols.h
python3 tools/port_progress.py
```

Expected: `278/278 functions VERIFIED; 1031/1031 mutants detected; 1 named gaps; 196/217 rows with
callees closed (61 have none)`; `OK` (109 tests, the extended exact sets); E2 byte-identical;
`all checks passed`; `symbols.h` regeneration byte-identical; `771 1203 64` / `731 731 100`.

- [ ] **Step 6: commit.**

```bash
git add port/src/game/fighter.c port/src/game/fighter.h port/src/game/config.c \
        port/tests/diff_runner.c tools/diff_verify.py tools/tests/test_diff_verify.py
git commit -m "tools: C3e: the last type rows and the tail leaves, their corrections and mutants"
```

### Task 3: the review sweep (one commit, only if a fix)

**Files:** the patch's rows. **Interfaces:** consumes Task 2's state; produces the store sweep's
findings.

- [ ] **Step 1: the store sweep.** For every C3e row, poke each field the row writes to a value that
  differs from what it writes and re-run `--function NAME --self-check`; a store with no sentinel
  fails some mutant. Re-check the fields the record lists as the rows' writes: the `0x107D18/1C/
  20/24/2A/2C` words and the `0x40`-byte `0x107A80` table, the `0x1088B6/BF/CB/CC/A8/F0/C9520`
  bytes, the slot `+0x3C/+0x41/+0x52/+0x53/+0x54/+0x5A/+0x5B/+0x5D/+0x63/+0x7A/+0x7B/+0x81`, the
  record `+0x24/+0x28/+0x34/+0x43/+0x51`, the config descriptors and `0x105DE1/0x105DAF/0x105D60`,
  the pose setters' `+0x10/+0x2C/+0x58/+0x7E` and the eight glob words, and `0x3A0FC`'s
  `0xF0AEC/0x100AD8/0x105B3A/0x105B36/0x105B38` bytes.
- [ ] **Step 2: the clobber and case-set re-check.** `python3 -m unittest
  tools.tests.test_diff_verify.RealFunctionTests` (the clobber re-derivation with the ten new
  entries and the C3e case-set test). Expected: OK.
- [ ] **Step 3: commit any fix.**

```bash
git add <the fixed files>
git commit -m "tests: C3e review sweep: the store sentinels and the case-set pins"
```

### Task 4: Closure (one commit)

**Files:** `docs/PROGRESS.md`, the record. **Interfaces:** consumes Task 2's measured state.

- [ ] **Step 1: the final gates.**

```bash
make diff-verify DIFF_IMAGE=/tmp/pr_c3e_close.bin 2>&1 | tail -1
make entry-triage E2_IMAGE=/tmp/pr_c3e_close_e2.bin 2>&1 | tail -2
PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -1
python3 tools/port_progress.py
make verify
```

Expected: the Task 2 counter; E2 byte-identical; `all checks passed`; `771 1203 64` / `731 731
100`; and the full `make verify` exit 0 with the 45 oracle lines equal to
`.superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt`, the `make audio-render` WAV
equal to the pre-change WAV, every gp ratchet line `ok` with measured == pin (Task 1's list
verbatim; the planner measured gp-oracle N 2064 / trace 8320), and `symbols.h` byte-identical. The
`port/src` changes are the two corrections, the exports and the two seams; the full ladder is what
proves no oracle-visible path moved.

- [ ] **Step 2: re-measure the tail verdict** (the record's §C3e.7 list) against the final tree:
  the unrowed callees of the three rows (`0x39040`, `0x392A0`, `0x3AAFC`) and, from every row, the
  other frontier addresses. If any of the twelve first-wave/C3f addresses got rowed, update §C3e.7 and the C3f
  list with the new evidence; otherwise the record's list stands.

- [ ] **Step 3: append the PROGRESS paragraph** (the seventeen rows, the counter, the two
  corrections, the C3e → C3f tail verdict of record §C3e.7).

- [ ] **Step 4: append §C3e.8 Results to the record** (the executed tree's counters, the commit
  shas, the gate log lines), mirroring C3d's §C3d.8.

- [ ] **Step 5: commit.**

```bash
git add docs/PROGRESS.md docs/superpowers/plans/2026-10-05-reverse-c3e-derivations.md
git commit -m "docs: C3e closure: the seventeen rows measured and the C3f tail verdict"
```

### The patch

```diff
diff --git a/port/src/game/config.c b/port/src/game/config.c
index ca13f42..78e92a7 100644
--- a/port/src/game/config.c
+++ b/port/src/game/config.c
@@ -20,6 +20,7 @@
  * trailing byte from DS_00105DAF + (descriptor & 0x3f). */
 u32 config_field_get(u32 field)
 {
+    PR_SEAM_RET(0x2D974u, field);
     if (field > 0x3Eu) return 0xFFFFFFFFu;
 
     u32 d = DSD(DS_0002D300 + field * 4u);
@@ -371,6 +372,7 @@ u32 hiscore_rank_probe(u32 value, u32 table)
 /* 0x2CAA8. `cmp byte [0x85d60],0; sete al; and eax,0xff`. */
 u32 config_not_free_play(void)
 {
+    PR_SEAM_RET0(0x2CAA8u);
     return DSB(DS_00105D60) == 0u ? 1u : 0u;
 }
 
diff --git a/port/src/game/fighter.c b/port/src/game/fighter.c
index 9b51ba0..4cfaecd 100644
--- a/port/src/game/fighter.c
+++ b/port/src/game/fighter.c
@@ -49,7 +49,7 @@ static int fighter_3962c(u32 side, u32 param_2);         /* 0x3962C */
 static int fighter_396ac(u32 side, u32 param_2);         /* 0x396AC */
 void fighter_18b44(u32 slot);                           /* 0x18B44 */
 static void fighter_39278(u32 v);                        /* 0x39278 */
-static u32 fighter_36d20(u32 slot);                      /* 0x36D20 */
+u32 fighter_36d20(u32 slot);                             /* 0x36D20 */
 static void fighter_2bd44_by_index(u32 rec);             /* 0x3B4D4 = 0x3B844 */
 void fighter_18bd4(u8 flags[16]);                        /* 0x18BD4 */
 static u8 hit_3d004(u32 side);                            /* 0x3D004 */
@@ -632,7 +632,7 @@ static int fighter_input_scan(u32 side, s32 n1, s32 n2, u32 mask)
 #define FIGHT_BLOCK_END0   0x000C8F68u  /* 0x1A8F4: per-char stream */
 #define FIGHT_BLOCK_END1   0x000C8FB8u  /* 0x1A8F4: per-char stream, +0x54 1 */
 
-static void fighter_ctx_rec_swap(u32 out[6], u32 rec);           /* 0x33A68 */
+void fighter_ctx_rec_swap(u32 out[6], u32 rec);                  /* 0x33A68 */
 
 /* 0x1A6AC — record §38. */
 void fighter_block_anim(u32 slot, u32 rec)
@@ -2068,7 +2068,7 @@ void fighter_state_364fc(u32 slot, u32 rec, u32 side)
 static void fighter_state_35b7c(u32 slot, u32 rec);         /* 0x35B7C */
 void fighter_state_35d20(u32 slot, u32 rec);                /* 0x35D20 */
 void fighter_1883c(u32 side, u32 a, u32 b);                  /* 0x1883C */
-static void fighter_36e78(u32 slot);                        /* 0x36E78 */
+void fighter_36e78(u32 slot);                              /* 0x36E78 */
 u32  hit_record_y(u32 side);                                /* 0x18788 */
 
 /* 0x29BC8. Resolve the character's palette handle for `side` and point `rec`'s
@@ -2257,7 +2257,7 @@ void fighter_wall_clamp(u32 side)
 /* 0x36E78. The +0x5B/landing reset: when +0x5B is set, arm +0x5A = 0x78 - +0x5B
  * and clear +0x5B; otherwise clear +0x5D, set +0x40 bits 0x1000/0x100000, reset
  * the other slot's +0x5D/+0x43 and the DS_001078FF character's palette. */
-static void fighter_36e78(u32 slot)
+void fighter_36e78(u32 slot)
 {
     PR_SEAM(0x36E78u, slot);
     u8 b = DSB(slot + 0x5Bu);                           /* 0x36E7B */
@@ -5382,7 +5382,7 @@ void fighter_4660c(u32 v)
 
 /* 0x46190. 1 when the DIP field 0x29 has bit 0x800 set and the machine is in
  * free play. */
-static int fighter_46190(void)
+int fighter_46190(void)
 {
     PR_SEAM_RET0(0x46190u);
     u32 r = config_field_get(0x29u);                        /* 0x46190/0x46195 */
@@ -5391,7 +5391,7 @@ static int fighter_46190(void)
 }
 
 /* 0x33A68. fighter_ctx_swap from a record pointer: side = rec+0x51. */
-static void fighter_ctx_rec_swap(u32 out[6], u32 rec)
+void fighter_ctx_rec_swap(u32 out[6], u32 rec)
 {
     fighter_ctx_swap(out, (u32)DSB(rec + 0x51u));           /* 0x33A69 */
 }
@@ -5405,14 +5405,15 @@ int fighter_1a5ac(u32 side)
     return (DSW(ctx[4] + 0x28u) & 0x4000u) == 0u;           /* 0x1A5BD..0x1A5CB */
 }
 
-/* 0x3A280. The reaction predicate: 1 when the byte is in 0x10..0x17 or
+/* 0x3A280. The reaction predicate: 1 when `code` is in 0x10..0x17 or
  * 0x20..0x3F. The raw's 0x3A260 jump table maps all eight 0x10..0x17 entries
  * to the `return 1` at 0x3A28A. */
+/* PORT: C3e's row removed the u8 cast this function had: the raw compares the
+ * full EAX (its r9 case, 0x100020, separates the two). */
 int fighter_3a280(u32 code)
 {
-    u8 a = (u8)code;
-    if (a >= 0x20u && a <= 0x3Fu) return 1;                 /* 0x3A280/0x3A28A */
-    if (a >= 0x10u && a <= 0x17u) return 1;                 /* 0x3A295/0x3A28A */
+    if (code >= 0x20u && code <= 0x3Fu) return 1;           /* 0x3A280/0x3A28A */
+    if (code >= 0x10u && code <= 0x17u) return 1;           /* 0x3A295/0x3A28A */
     return 0;                                               /* 0x3A29D */
 }
 
@@ -5500,28 +5501,28 @@ static void fighter_pose_commit(u32 side, u32 edx, u32 callback,
 
 /* 0x3A504. The pose setter with the 0x3A43C callback and the 0x107D14/0x107D10
  * pair. */
-static void fighter_pose_3a504(u32 side, u32 edx)
+void fighter_pose_3a504(u32 side, u32 edx)
 {
     PR_SEAM(0x3A504u, side, edx);
     fighter_pose_commit(side, edx, 0x0003A43Cu, DS_00107D14, DS_00107D10);
 }
 
 /* 0x3A650. The pose setter with the 0x3A588 callback and 0x107D0C/0x107D00. */
-static void fighter_pose_3a650(u32 side, u32 edx)
+void fighter_pose_3a650(u32 side, u32 edx)
 {
     PR_SEAM(0x3A650u, side, edx);
     fighter_pose_commit(side, edx, 0x0003A588u, DS_00107D0C, DS_00107D00);
 }
 
 /* 0x3A79C. The pose setter with the 0x3A6D4 callback and 0x107D08/0x107D04. */
-static void fighter_pose_3a79c(u32 side, u32 edx)
+void fighter_pose_3a79c(u32 side, u32 edx)
 {
     PR_SEAM(0x3A79Cu, side, edx);
     fighter_pose_commit(side, edx, 0x0003A6D4u, DS_00107D08, DS_00107D04);
 }
 
 /* 0x3A8E8. The pose setter with the 0x3A820 callback and 0x107CF8/0x107CFC. */
-static void fighter_pose_3a8e8(u32 side, u32 edx)
+void fighter_pose_3a8e8(u32 side, u32 edx)
 {
     PR_SEAM(0x3A8E8u, side, edx);
     fighter_pose_commit(side, edx, 0x0003A820u, DS_00107CF8, DS_00107CFC);
@@ -6118,7 +6119,7 @@ s32 fighter_39738(u32 side, s32 b)
 /* 0x36D20. The pose dispatcher's ECX&0x2000 arm: when 0x468D8(side) is 0,
  * either zero rec+0x34 and run 0x36BC8 (slot+0x54 != 2) or run the 0x36CE4
  * stance restart; return slot+0x52. */
-static u32 fighter_36d20(u32 slot)
+u32 fighter_36d20(u32 slot)
 {
     PR_SEAM_RET(0x36D20u, slot);
     u32 rec = DSD(slot);                                    /* 0x36D24 */
@@ -6151,7 +6152,7 @@ static void fighter_spawn_reaction_effect(const u32 ctx[6], u32 off, u32 stream)
 /* 0x3A0FC. The winner's effect spawn: build the reaction animation triple,
  * spawn the 0xBB09C/0xBB0B0 effect actors at the 0xF0AEC-derived offset and
  * start the per-side 0xE8Dxx effect stream. EAX = 1-side from 0x3AAFC. */
-static void fighter_3a0fc(u32 side)
+void fighter_3a0fc(u32 side)
 {
     PR_SEAM(0x3A0FCu, side);
     u32 ctx[6];
@@ -7575,7 +7576,10 @@ void fighter_reaction_apply(u32 slot, u32 reaction)
     if ((u8)DSB(self + 0x5Fu) == 0xFFu) {                   /* 0x3AB3F/0x3AB48 */
         uvar2 = 0u;                                         /* 0x3AB70 */
     } else {
-        fighter_anim_triple(anim2, ctx[1], (s32)reaction);  /* 0x3AB58 */
+        /* PORT: the raw's second triple takes DSB(self+0x5F) as the frame (0x3AB3F
+         * `mov dl,[edx+0x5f]` survives into 0x3AB58), not the reaction byte; the
+         * gate above excludes 0xFF (C3e correction). */
+        fighter_anim_triple(anim2, ctx[1], (s32)DSB(self + 0x5Fu));  /* 0x3AB58 */
         uvar2 = (u32)DSW(anim2[2] + 2u);                    /* 0x3AB61 */
     }
     fighter_39834(side, (s32)reaction);                     /* 0x3AB7C */
diff --git a/port/src/game/fighter.h b/port/src/game/fighter.h
index a912534..21f6309 100644
--- a/port/src/game/fighter.h
+++ b/port/src/game/fighter.h
@@ -425,6 +425,18 @@ void hit_anim_start_c(u32 rec, u32 stream, u32 frame_bits);
 void fighter_38bb0(u32 side);
 void fighter_38bc8(u32 side);
 
+/* C3e: the 0x392A0/0x3AAFC callees and 0x33A68's own row the differential
+ * rows call (each already carries its seam). */
+void fighter_ctx_rec_swap(u32 out[6], u32 rec);
+int  fighter_46190(void);
+void fighter_36e78(u32 slot);
+u32  fighter_36d20(u32 slot);
+void fighter_3a0fc(u32 side);
+void fighter_pose_3a504(u32 side, u32 edx);
+void fighter_pose_3a650(u32 side, u32 edx);
+void fighter_pose_3a79c(u32 side, u32 edx);
+void fighter_pose_3a8e8(u32 side, u32 edx);
+
 /* 0x1DE64. The reaction picker: map the side's command word (or, with slot+0x63
  * clear, the 0x46460/0x4649C input scan, record §49-B) through 0x1DDF4 to a
  * reaction code; 0xFF when nothing maps. */
diff --git a/port/tests/diff_runner.c b/port/tests/diff_runner.c
index c79377d..32f740d 100644
--- a/port/tests/diff_runner.c
+++ b/port/tests/diff_runner.c
@@ -11380,6 +11380,684 @@ static void m_c3d_pw_ext(const u32 *r, u32 *eax)  { c3d_2a820_core(r, C3D_PW_EXT
 static void m_c3d_pw_dead(const u32 *r, u32 *eax) { c3d_2a820_core(r, C3D_PW_DEAD); *eax = 0u; }
 static void m_c3d_pw_call(const u32 *r, u32 *eax) { c3d_2a820_core(r, C3D_PW_CALL); *eax = 0u; }
 
+/* ---- C3e (record 2026-10-05-reverse-c3e): the three last type rows -------------------------- */
+
+/* 0x39040's mutants. */
+#define C3E_90_GATE  0x0001u  /* the gate is > 0 */
+#define C3E_90_LIM   0x0002u  /* the band limit adds cc */
+#define C3E_90_B4    0x0004u  /* the display timer is 0xB5 */
+#define C3E_90_DELTA 0x0008u  /* the 0x7D0 step is 0x7CF */
+#define C3E_90_A23   0x0010u  /* the a >= 0x23 test is a > 0x23 */
+#define C3E_90_A41   0x0020u  /* the a >= 0x41 test is a > 0x41 */
+#define C3E_90_ID    0x0040u  /* the rng(3) 0 maps to 0xCE */
+#define C3E_90_RNG2  0x0080u  /* the 0xDA/0xDB pair is swapped */
+#define C3E_90_VOICE 0x0100u  /* the sound_voice call is skipped */
+#define C3E_90_CLEAR 0x0200u  /* the 0x40-byte clear is skipped */
+#define C3E_90_INC   0x0400u  /* the +0x7B increment is skipped */
+static void c3e_39040_core(const u32 *r, u32 mut)
+{
+    u32 side = r[R_EAX];
+    u32 slot = 0x001077B0u + side * 0x94u;
+    if ((s16)DSW(0x00107D2Cu + side * 2u) > ((mut & C3E_90_GATE) ? 0 : 1)) {
+        u32 q = DSD(slot + 0x3Cu) / DSD(0x000C9520u);
+        DSB(0x001088B6u + side) = (u8)DSB(0x00107D2Cu + side * 2u);
+        {
+            s32 lim = (mut & C3E_90_LIM)
+                    ? (s32)(u32)DSB(slot + 0x81u) + (s32)(u32)DSB(0x001088CCu)
+                    : (s32)(u32)DSB(slot + 0x81u) - (s32)(u32)DSB(0x001088CCu);
+            if ((s32)q > lim) {
+                u32 cap = (u32)DSB(0x001088CBu);
+                if (q >= cap) q = cap;
+                DSB(slot + 0x81u) = (u8)((u32)DSB(0x001088CCu) + (u8)q);
+            }
+        }
+        DSW(0x00107D18u + side * 2u) = (u16)((mut & C3E_90_B4) ? 0xB5u : 0xB4u);
+        fighter_38d90(side);
+        fighter_38fec(side);
+        DSW(0x00107D1Cu + side * 2u) = DSW(0x00107D20u + side * 2u);
+        if (!(mut & C3E_90_INC)) DSB(slot + 0x7Bu) = (u8)(DSB(slot + 0x7Bu) + 1u);
+        if (DSB(slot + 0x63u) == 0u) {
+            s32 rnd = (s32)(s16)DSW(0x00107D2Cu + side * 2u);
+            if (rnd == 2) {
+                fighter_41310(side, 0x3E8);
+            } else if (rnd >= 3) {
+                fighter_41310(side, rnd <= 0xC
+                              ? (s32)(u32)((u32)(rnd - 2) * ((mut & C3E_90_DELTA) ? 0x7CFu : 0x7D0u))
+                              : 0x4E20);
+                {
+                    s32 a = (s32)(s16)DSW(0x00107D20u + side * 2u);
+                    s32 r2 = (s32)(s16)DSW(0x00107D2Cu + side * 2u);
+                    int ge23 = (mut & C3E_90_A23) ? (a > 0x23) : (a >= 0x23);
+                    int ge41 = (mut & C3E_90_A41) ? (a > 0x41) : (a >= 0x41);
+                    if (ge23 && r2 >= 4) {
+                        fighter_4f944((u32)r2);
+                        if (ge41 && r2 >= 5)
+                            DSB(0x001088BFu) = (u8)(rng_next(2u) + 1u);
+                    } else if (a <= 0x23) {
+                        u32 r3 = (u32)DSB(0x001088A8u + side);
+                        u32 id;
+                        if (r3 >= 0x20u && r3 <= 0x3Fu) {
+                            u32 d = rng_next(3u);
+                            id = (d == 0u) ? ((mut & C3E_90_ID) ? 0xCEu : 0xCDu)
+                               : (d == 1u) ? 0xCEu : 0xCFu;
+                        } else {
+                            u32 d = rng_next(2u);
+                            id = (mut & C3E_90_RNG2) ? (d != 0u ? 0xDBu : 0xDAu)
+                                                     : (d != 0u ? 0xDAu : 0xDBu);
+                        }
+                        if (!(mut & C3E_90_VOICE)) (void)sound_voice(id);
+                    }
+                }
+            }
+        }
+    }
+    DSW(0x00107D2Cu + side * 2u) = 0;
+    DSW(0x00107D20u + side * 2u) = 0;
+    DSW(0x00107D24u + side * 2u) = 0;
+    if (!(mut & C3E_90_CLEAR)) fighter_38bb0(side);
+}
+static void b_c3e_39040(const u32 *r, u32 *eax)    { fighter_39040(r[R_EAX]); *eax = 0u; }
+static void m_c3e_90_gate(const u32 *r, u32 *eax)  { c3e_39040_core(r, C3E_90_GATE); *eax = 0u; }
+static void m_c3e_90_lim(const u32 *r, u32 *eax)   { c3e_39040_core(r, C3E_90_LIM); *eax = 0u; }
+static void m_c3e_90_b4(const u32 *r, u32 *eax)    { c3e_39040_core(r, C3E_90_B4); *eax = 0u; }
+static void m_c3e_90_delta(const u32 *r, u32 *eax) { c3e_39040_core(r, C3E_90_DELTA); *eax = 0u; }
+static void m_c3e_90_a23(const u32 *r, u32 *eax)   { c3e_39040_core(r, C3E_90_A23); *eax = 0u; }
+static void m_c3e_90_a41(const u32 *r, u32 *eax)   { c3e_39040_core(r, C3E_90_A41); *eax = 0u; }
+static void m_c3e_90_id(const u32 *r, u32 *eax)    { c3e_39040_core(r, C3E_90_ID); *eax = 0u; }
+static void m_c3e_90_rng2(const u32 *r, u32 *eax)  { c3e_39040_core(r, C3E_90_RNG2); *eax = 0u; }
+static void m_c3e_90_voice(const u32 *r, u32 *eax) { c3e_39040_core(r, C3E_90_VOICE); *eax = 0u; }
+static void m_c3e_90_clear(const u32 *r, u32 *eax) { c3e_39040_core(r, C3E_90_CLEAR); *eax = 0u; }
+static void m_c3e_90_inc(const u32 *r, u32 *eax)   { c3e_39040_core(r, C3E_90_INC); *eax = 0u; }
+
+/* 0x392A0's mutants. */
+#define C3E_92_A       0x0001u  /* the A scale is 0x77 */
+#define C3E_92_B       0x0002u  /* the B scale is 0x43 */
+#define C3E_92_CLAMP   0x0004u  /* the +0x53 == 1 B clamp is dropped */
+#define C3E_92_HALF    0x0008u  /* the +0x41 bit-1 halving is dropped */
+#define C3E_92_K       0x0010u  /* the k < 6 bound is 5 */
+#define C3E_92_T       0x0020u  /* the t table reads the next word */
+#define C3E_92_ZERO    0x0040u  /* the 0x46190 zeroing is dropped */
+#define C3E_92_MODE2   0x0080u  /* the mode-2 test is mode 3 */
+#define C3E_92_SO      0x0100u  /* the so pointer index is the own side */
+#define C3E_92_CLAMP44 0x0200u  /* the 0x44 clamp is 0x43 */
+#define C3E_92_TAIL    0x0400u  /* the 0x36E78 call is skipped */
+static void c3e_392a0_core(const u32 *r, u32 mut)
+{
+    u32 slot = r[R_EAX];
+    s32 v = (s32)r[R_EDX];
+    s32 w = (s32)r[R_EBX];
+    u32 rec = DSD(slot);
+    s32 A = (s32)((u32)v * ((mut & C3E_92_A) ? 0x77u : 120u)) / 100;
+    s32 B = (s32)((u32)w * ((mut & C3E_92_B) ? 0x43u : 68u)) / 100;
+    if (!(mut & C3E_92_CLAMP) && DSB(slot + 0x53u) == 1u) {
+        s32 e = (s32)DSB(slot + 0x5Du);
+        if (e + B > 0x2F) B = 0x2F - e;
+    }
+    if ((mut & C3E_92_HALF) ? 0 : ((DSB(slot + 0x41u) & 1u) != 0u))
+        A += (A - (A >> 31)) >> 1;
+    {
+        u32 ctx[6];
+        fighter_ctx_swap(ctx, (u32)DSB(rec + 0x51u));
+        {
+            u32 e0 = ctx[0] * 2u;
+            s32 k = (s32)DSD(0x00100CDEu + e0) >> 16;
+            s32 prod;
+            if ((s16)DSW(0x00100CE0u + e0) >= 0 && k < (s32)((mut & C3E_92_K) ? 5u : 6u)) {
+                s32 t = (s32)DSD(0x000BEBCAu + (u32)k * 2u + ((mut & C3E_92_T) ? 2u : 0u)) >> 16;
+                prod = (s32)((u32)t * (u32)B);
+            } else {
+                prod = (s32)((u32)B * 40u);
+            }
+            B = prod / 100;
+        }
+        {
+            int z = fighter_46190() != 0
+                    && ((u32)DSB(0x00107813u) ^ (u32)DSB(0x001078A7u)) != 0u
+                    && DSB(ctx[2] + 0x63u) != 0u;
+            if (!(mut & C3E_92_ZERO) && z) {
+                B = 0;
+                A = 0;
+            }
+        }
+        {
+            u32 mode2 = (u32)DSB(0x00104B1Du);
+            s32 base100 = (s32)((u32)v * 100u);
+            if (mode2 == ((mut & C3E_92_MODE2) ? 3u : 2u)) {
+                u32 other, so;
+                s32 x;
+                if (DSB(0x00105B38u) != 0u) goto L5E7;
+                other = (u32)DSB(rec + 0x51u) ^ 1u;
+                if (mut & C3E_92_SO) other ^= 1u;
+                so = DSD(0x001077A8u + other * 4u);
+                if (so == 0u) return;
+                x = A + (s32)DSB(slot + 0x5Au);
+                if (x > 0x78) {
+                    s32 q = (s32)((u32)((u32)DSB(slot + 0x5Au) * 100u)) / 0x78;
+                    s32 d = 0x64 - q;
+                    fighter_41310(1u - (u32)DSB(rec + 0x51u), (s32)((u32)d * 100u));
+                    DSB(slot + 0x5Au) = 0x78u;
+                } else {
+                    s32 y;
+                    fighter_41310(1u - (u32)DSB(rec + 0x51u), base100);
+                    y = (s32)DSB(so + 0x5Au);
+                    if (y > A) {
+                        DSB(so + 0x5Au) = (u8)(DSB(so + 0x5Au) - (u8)A);
+                    } else {
+                        DSB(slot + 0x5Au) = (u8)(DSB(slot + 0x5Au) + (u8)(A - y));
+                        DSB(so + 0x5Au) = 0u;
+                    }
+                }
+            } else if (mode2 == 4u) {
+                s32 E;
+                if (DSB(0x00105B38u) != 0u) goto L5E7;
+                if ((u32)DSB(0x00104B1Fu) == 3u) {
+                    u32 side = (u32)DSB(rec + 0x51u);
+                    E = (s32)((u32)A * (u32)DSD(0x00107468u + side * 4u)) / 100;
+                } else {
+                    E = A;
+                }
+                if (E + (s32)DSB(slot + 0x5Au) > 0x78) {
+                    s32 q = (s32)((u32)((u32)DSB(slot + 0x5Au) * 100u)) / 0x78;
+                    s32 d = 0x64 - q;
+                    fighter_41310(1u - (u32)DSB(rec + 0x51u), (s32)((u32)d * 100u));
+                    DSB(slot + 0x5Au) = 0x78u;
+                } else {
+                    DSB(slot + 0x5Au) = (u8)(DSB(slot + 0x5Au) + (u8)E);
+                    fighter_41310(1u - (u32)DSB(rec + 0x51u), base100);
+                }
+            } else {
+                if (DSB(0x00105B38u) != 0u) goto L5E7;
+                if (A + (s32)DSB(slot + 0x5Au) > 0x78) {
+                    s32 q = (s32)((u32)((u32)DSB(slot + 0x5Au) * 100u)) / 0x78;
+                    s32 d = 0x64 - q;
+                    fighter_41310(1u - (u32)DSB(rec + 0x51u), (s32)((u32)d * 100u));
+                    DSB(slot + 0x5Au) = 0x78u;
+                } else {
+                    DSB(slot + 0x5Au) = (u8)(DSB(slot + 0x5Au) + (u8)A);
+                    fighter_41310(1u - (u32)DSB(rec + 0x51u), base100);
+                }
+            }
+        }
+    }
+L5E7:
+    if (DSB(0x00105B36u) == 0u) {
+        s32 t = (s32)DSB(slot + 0x5Du) + B;
+        if (t > ((mut & C3E_92_CLAMP44) ? 0x43 : 0x44))
+            DSB(slot + 0x5Du) = (u8)((mut & C3E_92_CLAMP44) ? 0x43u : 0x44u);
+        else
+            DSB(slot + 0x5Du) = (u8)(DSB(slot + 0x5Du) + (u8)B);
+    }
+    if ((u32)DSB(slot + 0x5Au) >= 0x78u && DSW(0x00104B00u) != 3u) {
+        if (!(mut & C3E_92_TAIL)) fighter_36e78(slot);
+    }
+}
+static void b_c3e_392a0(const u32 *r, u32 *eax)   { fighter_392a0(r[R_EAX], (s32)r[R_EDX], (s32)r[R_EBX]); *eax = 0u; }
+static void m_c3e_92_a(const u32 *r, u32 *eax)     { c3e_392a0_core(r, C3E_92_A); *eax = 0u; }
+static void m_c3e_92_b(const u32 *r, u32 *eax)     { c3e_392a0_core(r, C3E_92_B); *eax = 0u; }
+static void m_c3e_92_clamp(const u32 *r, u32 *eax) { c3e_392a0_core(r, C3E_92_CLAMP); *eax = 0u; }
+static void m_c3e_92_half(const u32 *r, u32 *eax)  { c3e_392a0_core(r, C3E_92_HALF); *eax = 0u; }
+static void m_c3e_92_k(const u32 *r, u32 *eax)     { c3e_392a0_core(r, C3E_92_K); *eax = 0u; }
+static void m_c3e_92_t(const u32 *r, u32 *eax)     { c3e_392a0_core(r, C3E_92_T); *eax = 0u; }
+static void m_c3e_92_zero(const u32 *r, u32 *eax)  { c3e_392a0_core(r, C3E_92_ZERO); *eax = 0u; }
+static void m_c3e_92_mode2(const u32 *r, u32 *eax) { c3e_392a0_core(r, C3E_92_MODE2); *eax = 0u; }
+static void m_c3e_92_so(const u32 *r, u32 *eax)    { c3e_392a0_core(r, C3E_92_SO); *eax = 0u; }
+static void m_c3e_92_cl44(const u32 *r, u32 *eax)  { c3e_392a0_core(r, C3E_92_CLAMP44); *eax = 0u; }
+static void m_c3e_92_tail(const u32 *r, u32 *eax)  { c3e_392a0_core(r, C3E_92_TAIL); *eax = 0u; }
+
+/* 0x3AAFC's mutants. */
+#define C3E_AA_U2     0x0001u  /* uvar2 reads the first triple's word */
+#define C3E_AA_S5F    0x0002u  /* the self +0x5F == 0xFF test is dropped */
+#define C3E_AA_S6     0x0004u  /* the +0x53 == 6 && char == 1 test is dropped */
+#define C3E_AA_S54    0x0008u  /* the +0x54 == 2 test is dropped */
+#define C3E_AA_E200   0x0010u  /* the ecx 0x200 test is 0x100 */
+#define C3E_AA_E100   0x0020u  /* the ecx 0x100 test is dropped */
+#define C3E_AA_E2000  0x0040u  /* the ecx 0x2000 test is dropped */
+#define C3E_AA_ST     0x0080u  /* the +0x54 == 1 and == 3 arms are swapped */
+#define C3E_AA_S42    0x0100u  /* the +0x42 OR masks are swapped */
+#define C3E_AA_INC    0x0200u  /* the +0x6C increment is skipped */
+#define C3E_AA_VOICE  0x0400u  /* the voice call is skipped */
+#define C3E_AA_CALL0FC 0x0800u /* the 0x3A0FC call is skipped */
+static void c3e_3aafc_core(const u32 *r, u32 mut)
+{
+    u32 slot = r[R_EAX];
+    u32 reaction = r[R_S0];
+    u32 ctx[6];
+    u32 anim1[3], anim2[3], anim3[3];
+    u32 side, self, other;
+    u32 ecx, uvar2, edx3;
+    fighter_ctx_swap(ctx, (u32)DSB(DSD(slot) + 0x51u));
+    side = ctx[1];
+    other = ctx[2];
+    self = ctx[3];
+    hit_facing_flag(side);
+    fighter_anim_triple(anim1, ctx[0], (s32)reaction);
+    ecx = (u32)DSW(anim1[2] + 2u);
+    if ((mut & C3E_AA_S5F)
+            ? ((u8)DSB(self + 0x5Fu) != 0xFFu)
+            : ((u8)DSB(self + 0x5Fu) == 0xFFu)) {
+        uvar2 = 0u;
+    } else {
+        fighter_anim_triple(anim2, ctx[1], (s32)DSB(self + 0x5Fu));
+        uvar2 = (u32)DSW(((mut & C3E_AA_U2) ? anim1[2] : anim2[2]) + 2u);
+    }
+    fighter_39834(side, (s32)reaction);
+    if (fighter_3a280((u32)DSB(other + 0x5Fu)) != 0)
+        if (!(mut & C3E_AA_VOICE))
+            (void)sound_voice((u32)DSW(0x000BE008u + (u32)DSB(self + 0x7Au) * 2u));
+    if (!(mut & C3E_AA_CALL0FC)) fighter_3a0fc(ctx[0]);
+    if (!(mut & C3E_AA_INC)) DSW(other + 0x6Cu) = (u16)(DSW(other + 0x6Cu) + 1u);
+    if (mut & C3E_AA_S42) {
+        DSB(other + 0x42u) |= 1u;
+        DSB(self + 0x42u) |= 2u;
+    } else {
+        DSB(other + 0x42u) |= 2u;
+        DSB(self + 0x42u) |= 1u;
+    }
+    if ((uvar2 & 0x40u) != 0u) {
+        hit_facing_flag(side);
+        fighter_pose_start(side, 0xFFFFFFB0u, 0x46u, 0x0Cu, 0x14u);
+        DSB(self + 0x90u) = 5u;
+        return;
+    }
+    if (!(mut & C3E_AA_S6)
+            && DSB(self + 0x53u) == 6u && (u8)DSB(self + 0x7Au) == 1u) {
+        hit_facing_flag(side);
+        fighter_pose_start(side, 0xFFFFFFB0u, 0x46u, 0x0Cu, 0x14u);
+        DSB(self + 0x90u) = 5u;
+        return;
+    }
+    if (!(mut & C3E_AA_S54) && DSB(self + 0x54u) == 2u) {
+        hit_facing_flag(side);
+        fighter_pose_start(side, 0xFFFFFFB0u, 0x46u, 0x0Cu, 0x14u);
+        DSB(self + 0x90u) = 5u;
+        return;
+    }
+    if ((ecx & ((mut & C3E_AA_E200) ? 0x100u : 0x200u)) != 0u) {
+        hit_facing_flag(side);
+        fighter_pose_start(side, 0xFFFFFFB0u, 0x64u, 0x0Fu, 0x14u);
+        DSB(self + 0x90u) = 5u;
+        return;
+    }
+    if ((ecx & 0x100u) != 0u) {
+        if (mut & C3E_AA_E100) { /* fall through to the 0x2000 arm */ }
+        else {
+            DSB(self + 0x52u) = (u8)fighter_3aa54(self);
+            DSB(self + 0x90u) = 6u;
+            return;
+        }
+    }
+    if ((ecx & 0x2000u) != 0u) {
+        if (!(mut & C3E_AA_E2000)) {
+            DSB(self + 0x52u) = (u8)fighter_36d20(self);
+            return;
+        }
+    }
+    fighter_anim_triple(anim3, ctx[0], (s32)reaction);
+    edx3 = (u32)(s32)(s8)DSB(anim3[0] + 6u);
+    {
+        u8 st = (u8)DSB(self + 0x54u);
+        if ((mut & C3E_AA_ST) ? (st == 3u) : (st == 1u)) {
+            fighter_pose_3a8e8(side, edx3);
+            return;
+        }
+        if ((mut & C3E_AA_ST) ? (st == 1u) : (st == 3u)) return;
+        if ((ecx & 4u) != 0u) {
+            fighter_pose_3a650(side, edx3);
+            return;
+        }
+        if ((ecx & 8u) != 0u) {
+            fighter_pose_3a79c(side, edx3);
+            return;
+        }
+        fighter_pose_3a504(side, edx3);
+    }
+}
+static void b_c3e_3aafc(const u32 *r, u32 *eax)    { fighter_reaction_apply(r[R_EAX], r[R_S0]); *eax = 0u; }
+static void m_c3e_aa_u2(const u32 *r, u32 *eax)    { c3e_3aafc_core(r, C3E_AA_U2); *eax = 0u; }
+static void m_c3e_aa_s5f(const u32 *r, u32 *eax)   { c3e_3aafc_core(r, C3E_AA_S5F); *eax = 0u; }
+static void m_c3e_aa_s6(const u32 *r, u32 *eax)    { c3e_3aafc_core(r, C3E_AA_S6); *eax = 0u; }
+static void m_c3e_aa_s54(const u32 *r, u32 *eax)   { c3e_3aafc_core(r, C3E_AA_S54); *eax = 0u; }
+static void m_c3e_aa_e200(const u32 *r, u32 *eax)  { c3e_3aafc_core(r, C3E_AA_E200); *eax = 0u; }
+static void m_c3e_aa_e100(const u32 *r, u32 *eax)  { c3e_3aafc_core(r, C3E_AA_E100); *eax = 0u; }
+static void m_c3e_aa_e2000(const u32 *r, u32 *eax) { c3e_3aafc_core(r, C3E_AA_E2000); *eax = 0u; }
+static void m_c3e_aa_st(const u32 *r, u32 *eax)    { c3e_3aafc_core(r, C3E_AA_ST); *eax = 0u; }
+static void m_c3e_aa_s42(const u32 *r, u32 *eax)   { c3e_3aafc_core(r, C3E_AA_S42); *eax = 0u; }
+static void m_c3e_aa_inc(const u32 *r, u32 *eax)   { c3e_3aafc_core(r, C3E_AA_INC); *eax = 0u; }
+static void m_c3e_aa_voice(const u32 *r, u32 *eax) { c3e_3aafc_core(r, C3E_AA_VOICE); *eax = 0u; }
+static void m_c3e_aa_call0fc(const u32 *r, u32 *eax){ c3e_3aafc_core(r, C3E_AA_CALL0FC); *eax = 0u; }
+
+/* ---- C3e tail: the small leaves that close the three rows' callees ------------------------- */
+
+/* 0x3A280's mutants. */
+#define C3E_A280_LO   0x1u  /* the 0x10 bound is 0x11 */
+#define C3E_A280_CAST 0x2u  /* the u8 cast the port dropped */
+static u32 c3e_3a280_core(u32 code, u32 mut)
+{
+    u32 a = (mut & C3E_A280_CAST) ? (u32)(u8)code : code;
+    if (a >= 0x20u && a <= 0x3Fu) return 1u;
+    a -= (mut & C3E_A280_LO) ? 0x11u : 0x10u;
+    if (a <= 7u) return 1u;
+    return 0u;
+}
+static void b_c3e_3a280(const u32 *r, u32 *eax)   { *eax = (u32)fighter_3a280(r[R_EAX]); }
+static void m_c3e_3a_lo(const u32 *r, u32 *eax)   { *eax = c3e_3a280_core(r[R_EAX], C3E_A280_LO); }
+static void m_c3e_3a_cast(const u32 *r, u32 *eax) { *eax = c3e_3a280_core(r[R_EAX], C3E_A280_CAST); }
+
+/* 0x4F944's mutants. */
+static void c3e_4f944_core(const u32 *r, u32 mut)
+{
+    u32 v = r[R_EAX];
+    if ((mut & 1u) ? (v > 0x14u) : ((s32)v > 0x14)) v = 0x14u;
+    if (!(mut & 2u)) DSB(0x001088F0u) = (u8)v;
+    if (mut & 4u) {
+        DSW(0x001088E8u) = 0x10u;
+        DSW(0x001088EAu) = 0u;
+    } else {
+        DSW(0x001088E8u) = 0u;
+        DSW(0x001088EAu) = 0x10u;
+    }
+    if (!(mut & 8u)) DSB(0x00104AE9u) |= 8u;
+}
+static void b_c3e_4f944(const u32 *r, u32 *eax)   { fighter_4f944(r[R_EAX]); *eax = 0u; }
+static void m_c3e_4f_cmp(const u32 *r, u32 *eax)  { c3e_4f944_core(r, 1); *eax = 0u; }
+static void m_c3e_4f_f0(const u32 *r, u32 *eax)   { c3e_4f944_core(r, 2); *eax = 0u; }
+static void m_c3e_4f_e8(const u32 *r, u32 *eax)   { c3e_4f944_core(r, 4); *eax = 0u; }
+static void m_c3e_4f_ae9(const u32 *r, u32 *eax)  { c3e_4f944_core(r, 8); *eax = 0u; }
+
+/* 0x33A68's mutants. */
+static void c3e_33a68_core(const u32 *r, u32 mut)
+{
+    u32 eax = r[R_EAX], edx = r[R_EDX];
+    u32 side = (u32)DSB(edx + 0x51u) & 0xFFu;
+    u32 other = 1u - side;
+    u32 stride = (mut & 2u) ? 0x90u : 0x94u;
+    DSD(eax + 4u) = (mut & 1u) ? other : side;
+    DSD(eax) = (mut & 1u) ? side : other;
+    DSD(eax + 8u) = 0x001077B0u + other * stride;
+    DSD(eax + 0x0Cu) = 0x001077B0u + side * stride;
+    if (mut & 4u) {
+        DSD(eax + 0x10u) = DSD(DSD(eax + 0x0Cu));
+        DSD(eax + 0x14u) = DSD(DSD(eax + 8u));
+    } else {
+        DSD(eax + 0x10u) = DSD(DSD(eax + 8u));
+        DSD(eax + 0x14u) = DSD(DSD(eax + 0x0Cu));
+    }
+}
+static void b_c3e_33a68(const u32 *r, u32 *eax)
+{
+    fighter_ctx_rec_swap((u32 *)(mem + r[R_EAX]), r[R_EDX]);
+    *eax = 0u;
+}
+static void m_c3e_33_side(const u32 *r, u32 *eax)   { c3e_33a68_core(r, 1); *eax = 0u; }
+static void m_c3e_33_stride(const u32 *r, u32 *eax) { c3e_33a68_core(r, 2); *eax = 0u; }
+static void m_c3e_33_rec(const u32 *r, u32 *eax)    { c3e_33a68_core(r, 4); *eax = 0u; }
+
+/* 0x36E78's mutants. */
+static void c3e_36e78_core(const u32 *r, u32 mut)
+{
+    u32 slot = r[R_EAX];
+    u8 b = DSB(slot + 0x5Bu);
+    if (!(mut & 1u) && b != 0u) {
+        DSB(slot + 0x5Bu) = 0u;
+        DSB(slot + 0x5Au) = (u8)(0x78u - b);
+        DSB(slot + 0x41u) |= 8u;
+        return;
+    }
+    DSB(slot + 0x5Du) = 0u;
+    if (mut & 2u)
+        DSD(slot + 0x40u) = (DSD(slot + 0x40u) | 0x00100000u) & 0xFBFFF7FFu;
+    else
+        DSD(slot + 0x40u) = (DSD(slot + 0x40u) | 0x00101000u) & 0xFBFFF7FFu;
+    {
+        u32 other = DSD(0x001077A8u + ((u32)DSB(DSD(slot) + 0x51u) ^ 1u) * 4u);
+        if (other != 0u && !(mut & 4u)) {
+            u32 side, slot2;
+            DSB(other + 0x5Du) = 0u;
+            DSB(other + 0x43u) &= 0xFBu;
+            side = (u32)DSB(0x001078FFu);
+            slot2 = 0x001077B0u + side * 0x94u;
+            if (!(mut & 8u)) fighter_29bc8(side, DSD(slot2), (u32)DSB(slot2 + 0x7Au));
+            DSB(0x00104AE9u) &= 0xFEu;
+        }
+    }
+}
+static void b_c3e_36e78(const u32 *r, u32 *eax)    { fighter_36e78(r[R_EAX]); *eax = 0u; }
+static void m_c3e_6e_f5b(const u32 *r, u32 *eax)   { c3e_36e78_core(r, 1); *eax = 0u; }
+static void m_c3e_6e_f40(const u32 *r, u32 *eax)   { c3e_36e78_core(r, 2); *eax = 0u; }
+static void m_c3e_6e_other(const u32 *r, u32 *eax) { c3e_36e78_core(r, 4); *eax = 0u; }
+static void m_c3e_6e_pal(const u32 *r, u32 *eax)   { c3e_36e78_core(r, 8); *eax = 0u; }
+
+/* 0x2CAA8's mutants. */
+static u32 c3e_2caa8_core(const u32 *r, u32 mut)
+{
+    (void)r;
+    u32 b = (mut & 2u) ? (u32)DSB(0x00105D61u) : (u32)DSB(0x00105D60u);
+    if (mut & 4u) return 0u;
+    return (mut & 1u) ? (b != 0u ? 1u : 0u) : (b == 0u ? 1u : 0u);
+}
+static void b_c3e_2caa8(const u32 *r, u32 *eax)   { *eax = config_not_free_play(); }
+static void m_c3e_cnf_eq(const u32 *r, u32 *eax)  { *eax = c3e_2caa8_core(r, 1); }
+static void m_c3e_cnf_byte(const u32 *r, u32 *eax){ *eax = c3e_2caa8_core(r, 2); }
+static void m_c3e_cnf_zero(const u32 *r, u32 *eax){ *eax = c3e_2caa8_core(r, 4); }
+
+/* 0x468D8's mutants. */
+static u32 c3e_468d8_core(const u32 *r, u32 mut)
+{
+    u32 side = r[R_EAX];
+    u32 ctx[6];
+    u32 hand = (mut & 1u) ? 0x22BEAu : 0x22BECu;
+    u32 low = (mut & 2u) ? 0x80000000u : 0x7FFFFFFFu;
+    u32 when = (mut & 4u) ? 2u : 7u;
+    if (mut & 8u) side ^= 1u;
+    fighter_ctx_swap(ctx, side);
+    if (DSD(ctx[3] + 0x10u) == hand
+            && (DSD(ctx[5] + 0x24u) & low) == 0u
+            && DSB(ctx[3] + 0x54u) != 2u)
+        return 1u;
+    return DSB(ctx[3] + 0x52u) == when ? 1u : 0u;
+}
+static void b_c3e_468d8(const u32 *r, u32 *eax)   { *eax = (u32)ai_pred_468d8(r[R_EAX]); }
+static void m_c3e_8d_hand(const u32 *r, u32 *eax) { *eax = c3e_468d8_core(r, 1); }
+static void m_c3e_8d_low(const u32 *r, u32 *eax)  { *eax = c3e_468d8_core(r, 2); }
+static void m_c3e_8d_byte(const u32 *r, u32 *eax) { *eax = c3e_468d8_core(r, 4); }
+static void m_c3e_8d_side(const u32 *r, u32 *eax) { *eax = c3e_468d8_core(r, 8); }
+
+/* 0x2D974's mutants. */
+#define C3E_CF_ODD   0x1u   /* the cursor parity is flipped */
+#define C3E_CF_WIDTH 0x2u   /* the width is one too wide */
+#define C3E_CF_TRAIL 0x4u   /* the trailer byte is skipped */
+#define C3E_CF_FIELD 0x8u   /* the descriptor index is field+1 */
+#define C3E_CF_HI    0x10u  /* the bound is 0x3D */
+static u32 c3e_2d974_core(const u32 *r, u32 mut)
+{
+    u32 field = r[R_EAX];
+    u32 width, cursor, idx, value, trail;
+    s32 ebx;
+    int odd;
+    if (mut & C3E_CF_FIELD) field += 1u;
+    if (field > ((mut & C3E_CF_HI) ? 0x3Du : 0x3Eu)) return 0xFFFFFFFFu;
+    {
+        u32 d = DSD(0x0002D300u + field * 4u);
+        width = ((d >> 14) & 7u) + 1u;
+        cursor = ((d >> 6) & 0xFFu) + width;
+        if (mut & C3E_CF_WIDTH) width += 1u;
+        idx = cursor >> 1;
+        odd = (int)(cursor & 1u) ^ ((mut & C3E_CF_ODD) ? 1 : 0);
+        if (odd) {
+            value = (u32)DSB(0x00105DE1u + idx) & 0x0Fu;
+            ebx = (s32)idx;
+            width -= 1u;
+        } else {
+            value = 0u;
+            ebx = (s32)idx;
+        }
+        while (width != 0u) {
+            ebx -= 1;
+            if (width == 1u) {
+                value = (value << 4)
+                      | (((u32)DSB(0x00105DE1u + (u32)ebx) >> 4) & 0x0Fu);
+                break;
+            }
+            value = (value << 8) | (u32)DSB(0x00105DE1u + (u32)ebx);
+            width -= 2u;
+        }
+        trail = d & 0x3Fu;
+        if (trail != 0u && !(mut & C3E_CF_TRAIL))
+            value = (value << 8) | (u32)DSB(0x00105DAFu + trail);
+    }
+    return value;
+}
+static void b_c3e_2d974(const u32 *r, u32 *eax)   { *eax = config_field_get(r[R_EAX]); }
+static void m_c3e_cf_odd(const u32 *r, u32 *eax)  { *eax = c3e_2d974_core(r, C3E_CF_ODD); }
+static void m_c3e_cf_width(const u32 *r, u32 *eax){ *eax = c3e_2d974_core(r, C3E_CF_WIDTH); }
+static void m_c3e_cf_trail(const u32 *r, u32 *eax){ *eax = c3e_2d974_core(r, C3E_CF_TRAIL); }
+static void m_c3e_cf_field(const u32 *r, u32 *eax){ *eax = c3e_2d974_core(r, C3E_CF_FIELD); }
+static void m_c3e_cf_hi(const u32 *r, u32 *eax)   { *eax = c3e_2d974_core(r, C3E_CF_HI); }
+
+/* 0x46190's mutants. */
+static u32 c3e_46190_core(u32 mut)
+{
+    u32 v = config_field_get((mut & 4u) ? 0x28u : 0x29u);
+    if ((v & ((mut & 1u) ? 0x400u : 0x800u)) == 0u) return 0u;
+    if (mut & 2u) return 1u;
+    return config_not_free_play() == 0u ? 1u : 0u;
+}
+static void b_c3e_46190(const u32 *r, u32 *eax)   { (void)r; *eax = (u32)fighter_46190(); }
+static void m_c3e_90b_bit(const u32 *r, u32 *eax) { (void)r; *eax = c3e_46190_core(1); }
+static void m_c3e_90b_free(const u32 *r, u32 *eax){ (void)r; *eax = c3e_46190_core(2); }
+static void m_c3e_90b_fld(const u32 *r, u32 *eax) { (void)r; *eax = c3e_46190_core(4); }
+
+/* 0x36D20's mutants. */
+#define C3E_36D_PRED 0x1u  /* the 0x468D8 result is inverted */
+#define C3E_36D_S54  0x2u  /* the +0x54 == 2 test is dropped */
+#define C3E_36D_ANIM 0x4u  /* the 0x2BC30 call is skipped */
+#define C3E_36D_REC  0x8u  /* the rec+0x34 clear writes 0x1234 */
+#define C3E_36D_RET  0x10u /* the return reads +0x53 */
+static u32 c3e_36d20_core(const u32 *r, u32 mut)
+{
+    u32 slot = r[R_EAX];
+    u32 rec = DSD(slot);
+    u32 side = (u32)DSB(rec + 0x51u);
+    int p = ai_pred_468d8(side);
+    if (mut & C3E_36D_PRED) p = !p;
+    if (p != 0) return 0u;
+    if (!(mut & C3E_36D_S54) && DSB(slot + 0x54u) != 2u) {
+        DSW(rec + 0x34u) = (u16)((mut & C3E_36D_REC) ? 0x1234u : 0u);
+        fighter_state_36bc8(slot, rec);
+    } else {
+        DSB(slot + 0x43u) |= 4u;
+        if (!(mut & C3E_36D_ANIM)
+                && DSW(0x00104B00u) != 3u && DSW(0x00104B00u) != 0x22u)
+            actors_anim_begin(DSD(0x00102900u + side * 4u), 0x000E906Eu, 0x40400000u);
+    }
+    return (u32)DSB(slot + ((mut & C3E_36D_RET) ? 0x53u : 0x52u));
+}
+static void b_c3e_36d20(const u32 *r, u32 *eax)   { *eax = fighter_36d20(r[R_EAX]); }
+static void m_c3e_36d_pred(const u32 *r, u32 *eax){ *eax = c3e_36d20_core(r, C3E_36D_PRED); }
+static void m_c3e_36d_s54(const u32 *r, u32 *eax) { *eax = c3e_36d20_core(r, C3E_36D_S54); }
+static void m_c3e_36d_anim(const u32 *r, u32 *eax){ *eax = c3e_36d20_core(r, C3E_36D_ANIM); }
+static void m_c3e_36d_rec(const u32 *r, u32 *eax) { *eax = c3e_36d20_core(r, C3E_36D_REC); }
+static void m_c3e_36d_ret(const u32 *r, u32 *eax) { *eax = c3e_36d20_core(r, C3E_36D_RET); }
+
+/* The four pose setters' shared core. */
+static void c3e_pose_core(const u32 *r, u32 mut, u32 cb, u32 cb2, u32 ga, u32 gb)
+{
+    u32 side = r[R_EAX], edx = r[R_EDX];
+    u32 ctx[6];
+    u32 bx;
+    fighter_ctx_swap(ctx, side);
+    bx = (u32)DSB(ctx[3] + 0x52u);
+    DSD(ctx[5] + 0x24u) = 0;
+    DSB(ctx[3] + 0x52u) = (mut & 1u) ? 0x11u : 0x10u;
+    DSB(ctx[3] + 0x53u) = 0x0Au;
+    DSB(ctx[3] + 0x54u) = 0u;
+    DSD(ctx[3] + 0x10u) = (mut & 2u) ? cb2 : cb;
+    DSB(ctx[3] + 0x58u) = 0u;
+    DSB(ctx[3] + 0x7Eu) = (u8)(DSB(0x000BECF8u) + (u8)edx);
+    if (mut & 4u) {
+        DSW(gb + side * 2u) = DSW(ctx[3] + 0x2Cu);
+        DSW(ga + side * 2u) = (u16)bx;
+    } else {
+        DSW(ga + side * 2u) = DSW(ctx[3] + 0x2Cu);
+        DSW(gb + side * 2u) = (u16)bx;
+    }
+}
+static void b_c3e_pose504(const u32 *r, u32 *eax) { fighter_pose_3a504(r[R_EAX], r[R_EDX]); *eax = 0u; }
+static void m_c3e_p504_s52(const u32 *r, u32 *eax)  { c3e_pose_core(r, 1, 0x3A43Cu, 0x3A588u, 0x107D14u, 0x107D10u); *eax = 0u; }
+static void m_c3e_p504_cb(const u32 *r, u32 *eax)   { c3e_pose_core(r, 2, 0x3A43Cu, 0x3A588u, 0x107D14u, 0x107D10u); *eax = 0u; }
+static void m_c3e_p504_glob(const u32 *r, u32 *eax) { c3e_pose_core(r, 4, 0x3A43Cu, 0x3A588u, 0x107D14u, 0x107D10u); *eax = 0u; }
+static void b_c3e_pose650(const u32 *r, u32 *eax) { fighter_pose_3a650(r[R_EAX], r[R_EDX]); *eax = 0u; }
+static void m_c3e_p650_s52(const u32 *r, u32 *eax)  { c3e_pose_core(r, 1, 0x3A588u, 0x3A43Cu, 0x107D0Cu, 0x107D00u); *eax = 0u; }
+static void m_c3e_p650_cb(const u32 *r, u32 *eax)   { c3e_pose_core(r, 2, 0x3A588u, 0x3A43Cu, 0x107D0Cu, 0x107D00u); *eax = 0u; }
+static void m_c3e_p650_glob(const u32 *r, u32 *eax) { c3e_pose_core(r, 4, 0x3A588u, 0x3A43Cu, 0x107D0Cu, 0x107D00u); *eax = 0u; }
+static void b_c3e_pose79c(const u32 *r, u32 *eax) { fighter_pose_3a79c(r[R_EAX], r[R_EDX]); *eax = 0u; }
+static void m_c3e_p79c_s52(const u32 *r, u32 *eax)  { c3e_pose_core(r, 1, 0x3A6D4u, 0x3A820u, 0x107D08u, 0x107D04u); *eax = 0u; }
+static void m_c3e_p79c_cb(const u32 *r, u32 *eax)   { c3e_pose_core(r, 2, 0x3A6D4u, 0x3A820u, 0x107D08u, 0x107D04u); *eax = 0u; }
+static void m_c3e_p79c_glob(const u32 *r, u32 *eax) { c3e_pose_core(r, 4, 0x3A6D4u, 0x3A820u, 0x107D08u, 0x107D04u); *eax = 0u; }
+static void b_c3e_pose8e8(const u32 *r, u32 *eax) { fighter_pose_3a8e8(r[R_EAX], r[R_EDX]); *eax = 0u; }
+static void m_c3e_p8e8_s52(const u32 *r, u32 *eax)  { c3e_pose_core(r, 1, 0x3A820u, 0x3A6D4u, 0x107CF8u, 0x107CFCu); *eax = 0u; }
+static void m_c3e_p8e8_cb(const u32 *r, u32 *eax)   { c3e_pose_core(r, 2, 0x3A820u, 0x3A6D4u, 0x107CF8u, 0x107CFCu); *eax = 0u; }
+static void m_c3e_p8e8_glob(const u32 *r, u32 *eax) { c3e_pose_core(r, 4, 0x3A820u, 0x3A6D4u, 0x107CF8u, 0x107CFCu); *eax = 0u; }
+
+/* 0x3A0FC's mutants. */
+#define C3E_0FC_FRAME  0x01u  /* the triple frame reads +0x52 */
+#define C3E_0FC_FACE   0x02u  /* the facing bit is 0x8000 */
+#define C3E_0FC_OFF    0x04u  /* the 0x3BC0 addend is 0x3BC1 */
+#define C3E_0FC_LAYER  0x08u  /* the layer is not subtracted */
+#define C3E_0FC_STREAM 0x10u  /* the first spawn is skipped */
+#define C3E_0FC_KEY    0x20u  /* the key-1 stream is the key-2 one */
+#define C3E_0FC_S59    0x40u  /* the +0x59 = 3 store is skipped */
+static void c3e_3a0fc_core(const u32 *r, u32 mut)
+{
+    u32 side = r[R_EAX];
+    u32 ctx[6];
+    u32 anim[3];
+    u32 facing, off, stream;
+    s32 layer;
+    fighter_ctx_same(ctx, side);
+    fighter_anim_triple(anim, side,
+                        (s32)DSB(ctx[2] + ((mut & C3E_0FC_FRAME) ? 0x52u : 0x5Fu)));
+    facing = ((DSW(ctx[4] + 0x28u) & ((mut & C3E_0FC_FACE) ? 0x8000u : 0x4000u)) != 0u)
+           ? 0u : 0x4000u;
+    off = DSD(0x000F0AECu) + ((mut & C3E_0FC_OFF) ? 0x3BC1u : 0x3BC0u)
+        - (DSD(0x00100AD8u + side * 4u) << 6);
+    layer = (s32)DSD(ctx[5] + 0x30u) >> 16;
+    if (!(mut & C3E_0FC_LAYER)) off -= (u32)layer;
+    stream = DSD(anim[1] + 8u);
+    if (stream != 0u && (u32)DSB(0x00105B3Au) < 3u && !(mut & C3E_0FC_STREAM)) {
+        u32 a = actor_spawn((const u32 *)(mem + 0x000BB09Cu), DSD(ctx[3] + 0x2Cu),
+                            (u32)layer, off, facing);
+        actors_anim_begin(a, stream, 0x40400000u);
+    }
+    {
+        u32 key = DSW(anim[2]);
+        u32 s = 0u;
+        if (ctx[1] == 0u) {
+            if (key == 1u)      s = (mut & C3E_0FC_KEY) ? 0x000E8D86u : 0x000E8D6Cu;
+            else if (key == 2u) s = 0x000E8D86u;
+            else if (key == 3u) s = 0x000E8DA0u;
+        } else {
+            if (key == 1u)      s = (mut & C3E_0FC_KEY) ? 0x000E8DD4u : 0x000E8DBAu;
+            else if (key == 2u) s = 0x000E8DD4u;
+            else if (key == 3u) s = 0x000E8DEEu;
+        }
+        if (s != 0u) {
+            u32 a = actor_spawn((const u32 *)(mem + 0x000BB0B0u), DSD(ctx[3] + 0x2Cu),
+                                (u32)layer, off, 0u);
+            if (!(mut & C3E_0FC_S59)) DSB(a + 0x59u) = 3u;
+            actors_anim_begin(a, s, 0x40000000u);
+        }
+    }
+}
+static void b_c3e_3a0fc(const u32 *r, u32 *eax)    { fighter_3a0fc(r[R_EAX]); *eax = 0u; }
+static void m_c3e_0fc_frame(const u32 *r, u32 *eax)  { c3e_3a0fc_core(r, C3E_0FC_FRAME); *eax = 0u; }
+static void m_c3e_0fc_face(const u32 *r, u32 *eax)   { c3e_3a0fc_core(r, C3E_0FC_FACE); *eax = 0u; }
+static void m_c3e_0fc_off(const u32 *r, u32 *eax)    { c3e_3a0fc_core(r, C3E_0FC_OFF); *eax = 0u; }
+static void m_c3e_0fc_layer(const u32 *r, u32 *eax)  { c3e_3a0fc_core(r, C3E_0FC_LAYER); *eax = 0u; }
+static void m_c3e_0fc_stream(const u32 *r, u32 *eax) { c3e_3a0fc_core(r, C3E_0FC_STREAM); *eax = 0u; }
+static void m_c3e_0fc_key(const u32 *r, u32 *eax)    { c3e_3a0fc_core(r, C3E_0FC_KEY); *eax = 0u; }
+static void m_c3e_0fc_s59(const u32 *r, u32 *eax)    { c3e_3a0fc_core(r, C3E_0FC_S59); *eax = 0u; }
+
 static const binding_t k_bindings[] = {
     { "rng_next",                 b_rng_next,     0xFFFFFFFFu },
     { "fighter_slot_flag",        b_slot_flag,    0x000000FFu },
@@ -12592,6 +13270,109 @@ static const binding_t k_bindings[] = {
     { "fighter_379c4@cb",                  m_c3d_79_cb,    0x00000000u },
     { "fighter_379c4@fc",                  m_c3d_79_fc,    0x00000000u },
     { "fighter_379c4@tab",                 m_c3d_79_tab,   0x00000000u },
+    { "fighter_39040",                     b_c3e_39040,    0x00000000u },
+    { "fighter_39040@gate",                m_c3e_90_gate,  0x00000000u },
+    { "fighter_39040@lim",                 m_c3e_90_lim,   0x00000000u },
+    { "fighter_39040@b4",                  m_c3e_90_b4,    0x00000000u },
+    { "fighter_39040@delta",               m_c3e_90_delta, 0x00000000u },
+    { "fighter_39040@a23",                 m_c3e_90_a23,   0x00000000u },
+    { "fighter_39040@a41",                 m_c3e_90_a41,   0x00000000u },
+    { "fighter_39040@id",                  m_c3e_90_id,    0x00000000u },
+    { "fighter_39040@rng2",                m_c3e_90_rng2,  0x00000000u },
+    { "fighter_39040@voice",               m_c3e_90_voice, 0x00000000u },
+    { "fighter_39040@clear",               m_c3e_90_clear, 0x00000000u },
+    { "fighter_39040@inc",                 m_c3e_90_inc,   0x00000000u },
+    { "fighter_392a0",                     b_c3e_392a0,    0x00000000u },
+    { "fighter_392a0@a",                   m_c3e_92_a,     0x00000000u },
+    { "fighter_392a0@b",                   m_c3e_92_b,     0x00000000u },
+    { "fighter_392a0@clamp",               m_c3e_92_clamp, 0x00000000u },
+    { "fighter_392a0@half",                m_c3e_92_half,  0x00000000u },
+    { "fighter_392a0@k",                   m_c3e_92_k,     0x00000000u },
+    { "fighter_392a0@t",                   m_c3e_92_t,     0x00000000u },
+    { "fighter_392a0@zero",                m_c3e_92_zero,  0x00000000u },
+    { "fighter_392a0@mode2",               m_c3e_92_mode2, 0x00000000u },
+    { "fighter_392a0@so",                  m_c3e_92_so,    0x00000000u },
+    { "fighter_392a0@clamp44",             m_c3e_92_cl44,  0x00000000u },
+    { "fighter_392a0@tail",                m_c3e_92_tail,  0x00000000u },
+    { "fighter_reaction_apply",            b_c3e_3aafc,    0x00000000u },
+    { "fighter_reaction_apply@u2",         m_c3e_aa_u2,    0x00000000u },
+    { "fighter_reaction_apply@s5f",        m_c3e_aa_s5f,   0x00000000u },
+    { "fighter_reaction_apply@s6",         m_c3e_aa_s6,    0x00000000u },
+    { "fighter_reaction_apply@s54",        m_c3e_aa_s54,   0x00000000u },
+    { "fighter_reaction_apply@e200",       m_c3e_aa_e200,  0x00000000u },
+    { "fighter_reaction_apply@e100",       m_c3e_aa_e100,  0x00000000u },
+    { "fighter_reaction_apply@e2000",      m_c3e_aa_e2000, 0x00000000u },
+    { "fighter_reaction_apply@st",         m_c3e_aa_st,    0x00000000u },
+    { "fighter_reaction_apply@s42",        m_c3e_aa_s42,   0x00000000u },
+    { "fighter_reaction_apply@inc",        m_c3e_aa_inc,   0x00000000u },
+    { "fighter_reaction_apply@voice",      m_c3e_aa_voice, 0x00000000u },
+    { "fighter_reaction_apply@call0fc",    m_c3e_aa_call0fc, 0x00000000u },
+    { "fighter_3a280",                     b_c3e_3a280,    0x000000FFu },
+    { "fighter_3a280@lo",                  m_c3e_3a_lo,    0x000000FFu },
+    { "fighter_3a280@cast",                m_c3e_3a_cast,  0x000000FFu },
+    { "fighter_4f944",                     b_c3e_4f944,    0x00000000u },
+    { "fighter_4f944@cmp",                 m_c3e_4f_cmp,   0x00000000u },
+    { "fighter_4f944@f0",                  m_c3e_4f_f0,    0x00000000u },
+    { "fighter_4f944@e8",                  m_c3e_4f_e8,    0x00000000u },
+    { "fighter_4f944@ae9",                 m_c3e_4f_ae9,   0x00000000u },
+    { "fighter_ctx_rec_swap",              b_c3e_33a68,    0x00000000u },
+    { "fighter_ctx_rec_swap@side",         m_c3e_33_side,  0x00000000u },
+    { "fighter_ctx_rec_swap@stride",       m_c3e_33_stride, 0x00000000u },
+    { "fighter_ctx_rec_swap@rec",          m_c3e_33_rec,   0x00000000u },
+    { "fighter_36e78",                     b_c3e_36e78,    0x00000000u },
+    { "fighter_36e78@f5b",                 m_c3e_6e_f5b,   0x00000000u },
+    { "fighter_36e78@f40",                 m_c3e_6e_f40,   0x00000000u },
+    { "fighter_36e78@other",               m_c3e_6e_other, 0x00000000u },
+    { "fighter_36e78@pal",                 m_c3e_6e_pal,   0x00000000u },
+    { "config_not_free_play",              b_c3e_2caa8,    0x000000FFu },
+    { "config_not_free_play@eq",           m_c3e_cnf_eq,   0x000000FFu },
+    { "config_not_free_play@byte",         m_c3e_cnf_byte, 0x000000FFu },
+    { "config_not_free_play@zero",         m_c3e_cnf_zero, 0x000000FFu },
+    { "ai_pred_468d8",                     b_c3e_468d8,    0x000000FFu },
+    { "ai_pred_468d8@hand",                m_c3e_8d_hand,  0x000000FFu },
+    { "ai_pred_468d8@low",                 m_c3e_8d_low,   0x000000FFu },
+    { "ai_pred_468d8@byte",                m_c3e_8d_byte,  0x000000FFu },
+    { "ai_pred_468d8@side",                m_c3e_8d_side,  0x000000FFu },
+    { "config_field_get",                  b_c3e_2d974,    0xFFFFFFFFu },
+    { "config_field_get@odd",              m_c3e_cf_odd,   0xFFFFFFFFu },
+    { "config_field_get@width",            m_c3e_cf_width, 0xFFFFFFFFu },
+    { "config_field_get@trail",            m_c3e_cf_trail, 0xFFFFFFFFu },
+    { "config_field_get@field",            m_c3e_cf_field, 0xFFFFFFFFu },
+    { "config_field_get@hi",               m_c3e_cf_hi,    0xFFFFFFFFu },
+    { "fighter_46190",                     b_c3e_46190,    0x000000FFu },
+    { "fighter_46190@bit",                 m_c3e_90b_bit,  0x000000FFu },
+    { "fighter_46190@notfree",             m_c3e_90b_free, 0x000000FFu },
+    { "fighter_46190@field",               m_c3e_90b_fld,  0x000000FFu },
+    { "fighter_36d20",                     b_c3e_36d20,    0x000000FFu },
+    { "fighter_36d20@pred",                m_c3e_36d_pred, 0x000000FFu },
+    { "fighter_36d20@s54",                 m_c3e_36d_s54,  0x000000FFu },
+    { "fighter_36d20@anim",                m_c3e_36d_anim, 0x000000FFu },
+    { "fighter_36d20@rec",                 m_c3e_36d_rec,  0x000000FFu },
+    { "fighter_36d20@ret",                 m_c3e_36d_ret,  0x000000FFu },
+    { "fighter_pose_3a504",                b_c3e_pose504,  0x00000000u },
+    { "fighter_pose_3a504@s52",            m_c3e_p504_s52, 0x00000000u },
+    { "fighter_pose_3a504@cb",             m_c3e_p504_cb,  0x00000000u },
+    { "fighter_pose_3a504@glob",           m_c3e_p504_glob, 0x00000000u },
+    { "fighter_pose_3a650",                b_c3e_pose650,  0x00000000u },
+    { "fighter_pose_3a650@s52",            m_c3e_p650_s52, 0x00000000u },
+    { "fighter_pose_3a650@cb",             m_c3e_p650_cb,  0x00000000u },
+    { "fighter_pose_3a650@glob",           m_c3e_p650_glob, 0x00000000u },
+    { "fighter_pose_3a79c",                b_c3e_pose79c,  0x00000000u },
+    { "fighter_pose_3a79c@s52",            m_c3e_p79c_s52, 0x00000000u },
+    { "fighter_pose_3a79c@cb",             m_c3e_p79c_cb,  0x00000000u },
+    { "fighter_pose_3a79c@glob",           m_c3e_p79c_glob, 0x00000000u },
+    { "fighter_pose_3a8e8",                b_c3e_pose8e8,  0x00000000u },
+    { "fighter_pose_3a8e8@s52",            m_c3e_p8e8_s52, 0x00000000u },
+    { "fighter_pose_3a8e8@cb",             m_c3e_p8e8_cb,  0x00000000u },
+    { "fighter_pose_3a8e8@glob",           m_c3e_p8e8_glob, 0x00000000u },
+    { "fighter_3a0fc",                     b_c3e_3a0fc,    0x00000000u },
+    { "fighter_3a0fc@frame",               m_c3e_0fc_frame, 0x00000000u },
+    { "fighter_3a0fc@face",                m_c3e_0fc_face, 0x00000000u },
+    { "fighter_3a0fc@off",                 m_c3e_0fc_off,  0x00000000u },
+    { "fighter_3a0fc@layer",               m_c3e_0fc_layer, 0x00000000u },
+    { "fighter_3a0fc@stream",              m_c3e_0fc_stream, 0x00000000u },
+    { "fighter_3a0fc@key",                 m_c3e_0fc_key,  0x00000000u },
+    { "fighter_3a0fc@s59",                 m_c3e_0fc_s59,  0x00000000u },
 };
 
 static const binding_t *find_binding(const char *name)
diff --git a/tools/diff_verify.py b/tools/diff_verify.py
index 9d21fc2..5df5b5e 100644
--- a/tools/diff_verify.py
+++ b/tools/diff_verify.py
@@ -5975,6 +5975,515 @@ C3D_SPECS = [
        eax_mask=0, mutants=("@pool", "@x", "@lay", "@vis", "@ext", "@dead", "@call")),
 ]
 
+# ---- track P batch C3e (record 2026-10-05-reverse-c3e): the last type rows, part 5 -------------
+
+
+def c3e_90_case(side, rnd, q=0x20, divisor=2, s81=0x30, cc=8, cb=0x20, s7b=0x11, s63=1,
+                a=0x23, d1c=0x1111, r3=0x30):
+    """0x39040: the per-side gate word 0x107D2C (the high half of the raw's 0x107D2A dword read),
+    the 0x107D20 counter, the slot's +0x3C/+0x81/+0x7B/+0x63, the 0xC9520 divisor, the
+    0x1088CB/CC/BF/A8 band bytes, the 0x4F944 target globals and the 0x40-byte table at
+    0x107A80 + side*0x40."""
+    so = 0x001077B0 + side * 0x94
+    d = bytearray([0x7E]) * 0x1C                       # 0x107D18 .. 0x107D33
+    d[0x107D18 - 0x107D18 + side * 2:0x107D1A - 0x107D18 + side * 2] = le16(0x3333)
+    d[0x107D1C - 0x107D18 + side * 2:0x107D1E - 0x107D18 + side * 2] = le16(d1c)
+    d[0x107D20 - 0x107D18 + side * 2:0x107D22 - 0x107D18 + side * 2] = le16(a)
+    d[0x107D24 - 0x107D18 + side * 2:0x107D26 - 0x107D18 + side * 2] = le16(0x2222)
+    d[0x107D2A - 0x107D18 + side * 2:0x107D2C - 0x107D18 + side * 2] = le16(0x5A5A)
+    d[0x107D2C - 0x107D18 + side * 2:0x107D2E - 0x107D18 + side * 2] = le16(rnd)
+    b = bytearray([0x7E]) * (0x1088CE - 0x1088A8)      # 0x1088A8 .. 0x1088CD
+    b[0x1088A8 - 0x1088A8 + side] = r3
+    b[0x1088B6 - 0x1088A8 + side] = 0x77
+    b[0x1088BF - 0x1088A8] = 0x66
+    b[0x1088CB - 0x1088A8] = cb
+    b[0x1088CC - 0x1088A8] = cc
+    e = bytearray(10)                                  # 0x1088E8 .. 0x1088F1
+    e[0:2] = le16(0xAAAA)
+    e[2:4] = le16(0xBBBB)
+    e[8] = 0x99
+    return {
+        0x00107D18: bytes(d),
+        0x00107A80 + side * 0x40: bytes([0xEE]) * 0x40,
+        0x001088A8: bytes(b),
+        0x001088E8: bytes(e),
+        0x00104AE9: b"\x01",
+        0x000C9520: le32(divisor),
+        so + 0x3C: le32(q),
+        so + 0x7B: bytes([s7b]),
+        so + 0x63: bytes([s63]),
+        so + 0x81: bytes([s81]),
+    }
+
+
+def c3e_92_base(side, s53=0, s54=0, s41=0x10, s5a=0x10, s5d=0x10, o63=0):
+    """0x392A0: the side's slot +0x53/+0x54/+0x41/+0x5A/+0x5D, its record's +0x51, the other
+    slot's +0x63 and the mode-2 `so` record's +0x5A (DSD(0x1077A8 + other*4) is REC1 for side 0
+    and REC0 for side 1, both seeded by c3d_slot_pokes)."""
+    p = c3d_slot_pokes()
+    so = 0x001077B0 + side * 0x94
+    rec = C3D_REC0 + side * 0x40
+    p[so + 0x53] = bytes([s53])
+    p[so + 0x54] = bytes([s54])
+    p[so + 0x41] = bytes([s41])
+    p[so + 0x5A] = bytes([s5a])
+    p[so + 0x5D] = bytes([s5d])
+    p[rec + 0x51] = bytes([side])
+    p[0x001077B0 + (1 - side) * 0x94 + 0x63] = bytes([o63])
+    p[C3D_REC0 + (1 - side) * 0x40 + 0x5A] = b"\x22"
+    return p
+
+
+def c3e_aa_base(side, s5f=0, s53=0, s54=0, s52=0, s42=0, s90=0, o5f=0, o42=0, o6c=0,
+                ch0=0, ch1=0):
+    """0x3AAFC: the side's slot +0x5F/+0x53/+0x54/+0x52/+0x42/+0x90/+0x7A, the other slot's
+    +0x5F/+0x42/+0x6C and both slots' +0x7A character bytes (the anim-triple index)."""
+    p = c3d_slot_pokes()
+    so = 0x001077B0 + side * 0x94
+    oo = 0x001077B0 + (1 - side) * 0x94
+    rec = C3D_REC0 + side * 0x40
+    a = bytearray([0x7E]) * 0x3C                       # so+0x40 .. so+0x7B
+    a[0x42 - 0x40] = s42
+    a[0x52 - 0x40] = s52
+    a[0x53 - 0x40] = s53
+    a[0x54 - 0x40] = s54
+    a[0x5F - 0x40] = s5f
+    a[0x7A - 0x40] = ch1 if side else ch0
+    c = bytearray([0x7D]) * 0x3C                       # oo+0x40 .. oo+0x7B
+    c[0x42 - 0x40] = o42
+    c[0x5F - 0x40] = o5f
+    c[0x6C - 0x40:0x6E - 0x40] = le16(o6c)
+    c[0x7A - 0x40] = ch0 if side else ch1
+    p[rec + 0x51] = bytes([side])
+    p[so + 0x40] = bytes(a)
+    p[so + 0x90] = bytes([s90])
+    p[oo + 0x40] = bytes(c)
+    return p
+
+
+def c3e_4f_case():
+    """0x4F944: the four globals its clamp/arming body writes, seeded off their written values."""
+    return {0x001088F0: b"\x55", 0x001088E8: le16(0xAAAA), 0x001088EA: le16(0xBBBB),
+            0x00104AE9: b"\x01"}
+
+
+def c3e_33_case(out, rec, rec51):
+    """0x33A68: the out buffer, rec+0x51 and the two slot record pointers through 0x1077B0."""
+    return {rec + 0x51: bytes([rec51]), out: b"\xAA" * 24,
+            0x001077B0: le32(0x10A200), 0x001077B0 + 0x94: le32(0x10A240),
+            0x10A200: le32(0x10A280), 0x10A240: le32(0x10A2C0)}
+
+
+def c3e_6e_case(side, s5b, live=True):
+    """0x36E78: the +0x5B/else arms' slot fields, the other record's +0x43/+0x5D through
+    0x1077A8 and the 0x1078FF character's slot record for the 0x29BC8 stub."""
+    p = c3d_slot_pokes()
+    so = 0x001077B0 + side * 0x94
+    orec = 0x10A300
+    p[so + 0x5B] = bytes([s5b])
+    p[so + 0x41] = b"\x11"
+    p[so + 0x5A] = b"\x77"
+    p[so + 0x5D] = b"\x44"
+    p[so + 0x40] = le32(0x404040C0)
+    p[0x001077A8 + (1 - side) * 4] = le32(orec if live else 0)
+    p[orec + 0x43] = b"\x44"
+    p[orec + 0x5D] = b"\x66"
+    p[0x001078FF] = b"\x02"
+    p[0x001077B0 + 2 * 0x94] = le32(0x10A400)
+    p[0x001077B0 + 2 * 0x94 + 0x7A] = b"\x03"
+    return p
+
+
+def c3e_8d_case(side, s10, r24, s54, s52):
+    """0x468D8: the side slot's +0x10/+0x54/+0x52 and its record's +0x24."""
+    p = c3d_slot_pokes()
+    so = 0x001077B0 + side * 0x94
+    rec = C3D_REC0 + side * 0x40
+    p[so + 0x10] = le32(s10)
+    p[so + 0x54] = bytes([s54])
+    p[so + 0x52] = bytes([s52])
+    p[rec + 0x24] = le32(r24)
+    p[rec + 0x51] = bytes([side])
+    return p
+
+
+def c3e_cf_case(field, desc, base=None, extra=None):
+    """0x2D974: the descriptor table 0x2D300[field] and (optionally) the packed config bytes
+    0x105DE1.. and the trailing byte 0x105DAF+trail."""
+    p = {0x0002D300 + field * 4: le32(desc)}
+    if base is not None:
+        p[0x00105DE1] = base
+    trail = desc & 0x3F
+    if trail:
+        p[0x00105DAF + trail] = b"\x3E"
+    if extra:
+        p.update(extra)
+    return p
+
+
+def c3e_90b_case(desc, free, base):
+    """0x46190: the 0x29 descriptor, the packed config bytes and the free-play byte."""
+    return {0x0002D300 + 0x29 * 4: le32(desc), 0x00105D60: bytes([free]),
+            0x00105DE1: base}
+
+
+def c3e_36d_case(side, s54, s52, s43=0x11, mode=0, rec34=0x11223344):
+    """0x36D20: the slot's +0x54/+0x52/+0x43, the mode word, the record's +0x34 and the side's
+    0x102900 animation record."""
+    p = c3d_slot_pokes()
+    so = 0x001077B0 + side * 0x94
+    rec = C3D_REC0 + side * 0x40
+    p[so + 0x54] = bytes([s54])
+    p[so + 0x52] = bytes([s52])
+    p[so + 0x43] = bytes([s43])
+    p[rec + 0x34] = le32(rec34)
+    p[rec + 0x51] = bytes([side])
+    p[0x00104B00] = le16(mode)
+    p[0x00102900 + side * 4] = le32(0x10A600 + side * 0x40)
+    return p
+
+
+def c3e_pose_case(side, edx, ga, gb, s52=0x33, s2c=0x1122, rec24=0x99999999):
+    """The four 0x3A5xx/0x3A6xx/0x3A8xx pose setters: the self slot's +0x52 band (the raw reads
+    the caller's BX from it), +0x2C (the latched word), +0x58/+0x7E, the record's +0x24 and the
+    setter's glob pair."""
+    p = c3d_slot_pokes()
+    so = 0x001077B0 + side * 0x94
+    rec = C3D_REC0 + side * 0x40
+    p[so + 0x10] = le32(0xA5A5A5A5)
+    p[so + 0x2C] = le16(s2c)
+    p[so + 0x52] = bytes([s52, 0x44, 0x55, 0x66])
+    p[so + 0x58] = b"\x88"
+    p[so + 0x7E] = b"\x77"
+    p[rec + 0x24] = le32(rec24)
+    p[rec + 0x51] = bytes([side])
+    p[0x000BECF8] = b"\x0A"
+    p[ga + side * 2] = le16(0xAAAA)
+    p[gb + side * 2] = le16(0xBBBB)
+    return p
+
+
+def c3e_0fc_case(side, frame, char=0, stream=0x1234, key=1, b5b3a=0,
+                 layer=0, f0aec=0, f100ad8=0, s28=0, other2c=0x11223344):
+    """0x3A0FC: the triple frame char/frame, the effect stream at anim[1]+8 and the key word at
+    anim[2], the 0x105B3A gate, the 0xF0AEC/0x100AD8 offset, the layer from the other record's
+    +0x30 and the other slot's +0x2C."""
+    p = c3d_slot_pokes()
+    so = 0x001077B0 + side * 0x94
+    oo = 0x001077B0 + (1 - side) * 0x94
+    recs = C3D_REC0 + side * 0x40
+    reco = C3D_REC0 + (1 - side) * 0x40
+    c = (char << 6) + frame
+    p[so + 0x7A] = bytes([char])
+    p[so + 0x5F] = bytes([frame])
+    p[so + 0x52] = b"\x20"
+    p[recs + 0x51] = bytes([side])
+    p[reco + 0x51] = bytes([1 - side])
+    p[recs + 0x28] = le16(s28)
+    p[reco + 0x30] = le32(layer << 16)
+    p[oo + 0x2C] = le32(other2c)
+    p[0x000A3528 + 20 * c + 8] = le32(stream)
+    p[0x000A6728 + 6 * c] = le16(key)
+    p[0x00105B3A] = bytes([b5b3a])
+    p[0x000F0AEC] = le32(f0aec)
+    p[0x00100AD8 + side * 4] = le32(f100ad8)
+    return p
+
+
+C3E_SPECS = [
+    # 0x39040 fighter_39040: the per-side combo pass. EAX = side. The gate is the 0x107D2C word
+    # (the high half of the raw's 0x107D2A dword read); 0x38D90/0x38FEC/0x41310/rng (0x5D7DC)/
+    # voice (0x2C3FC) are stubs, 0x4F944 is a leaf allow (its writes are the observation).
+    Spec("fighter_39040", 0x39040, [
+        Case("g0", {"eax": 0}, c3e_90_case(0, 1)),                    # the gate: (s16)word <= 1
+        Case("g1", {"eax": 0}, c3e_90_case(0, 2, q=0x10)),            # q <= lim, s63 != 0
+        Case("g2", {"eax": 0}, c3e_90_case(0, 2, q=0x30, s81=0x20, cc=0x10)),  # q > lim, no clamp
+        Case("g3", {"eax": 0}, c3e_90_case(0, 2, q=0x40, s81=0x20, cc=0x10)),  # q >= cap: clamp
+        Case("g4", {"eax": 0}, c3e_90_case(0, 2, s63=0, a=0x50)),     # round == 2: 0x3E8
+        Case("g5", {"eax": 0}, c3e_90_case(0, 3, s63=0, a=0x23),
+             {0x5D7DC: 0}),                                           # delta 0x7D0; voice 0xCD
+        Case("g6", {"eax": 0}, c3e_90_case(0, 4, s63=0, a=0x23), {0x5D7DC: 0}),   # 0x4F944 runs
+        Case("g7", {"eax": 0}, c3e_90_case(0, 5, s63=0, a=0x41), {0x5D7DC: 0}),   # +BF = 1
+        Case("g8", {"eax": 0}, c3e_90_case(0, 5, s63=0, a=0x41), {0x5D7DC: 1}),   # +BF = 2
+        Case("g9", {"eax": 0}, c3e_90_case(0, 0x10, s63=0, a=0x10, r3=0x50),
+             {0x5D7DC: 1}),                                           # 0x4E20; voice 0xDA
+        Case("g10", {"eax": 0}, c3e_90_case(0, 4, s63=0, a=0x10, r3=0x50),
+             {0x5D7DC: 0}),                                           # voice 0xDB
+        Case("g11", {"eax": 0}, c3e_90_case(0, 4, s63=0, a=0x10, r3=0x30),
+             {0x5D7DC: 1}),                                           # voice 0xCE
+        Case("g12", {"eax": 0}, c3e_90_case(0, 4, s63=0, a=0x10, r3=0x30),
+             {0x5D7DC: 2}),                                           # voice 0xCF
+        Case("g13", {"eax": 1}, c3e_90_case(1, 4, s63=0, a=0x10, r3=0x30, q=0x42, s81=0x21, cc=0x11),
+             {0x5D7DC: 2}),                                           # the other side
+        Case("g14", {"eax": 0}, c3e_90_case(0, 3, s63=0, a=0x30), {0x5D7DC: 0}),  # a > 0x23: tail
+        Case("g15", {"eax": 0}, c3e_90_case(0, 4, s63=0, a=0x41), {0x5D7DC: 0}),  # round < 5: tail
+        Case("g16", {"eax": 0}, c3e_90_case(0, 0xC, s63=0, a=0x23), {0x5D7DC: 0}),  # the 0xC edge
+    ], allow_calls=(0x4F944,),
+       calls=(E.Call(0x38D90, ("eax",), mode="stub"),
+              E.Call(0x38FEC, ("eax",), mode="stub"),
+              E.Call(0x41310, ("eax", "edx"), mode="stub"),
+              E.Call(0x5D7DC, ("eax",), mode="stub"),
+              VOICE),
+       eax_mask=0,
+       mutants=("@gate", "@lim", "@b4", "@delta", "@a23", "@a41", "@id", "@rng2", "@voice",
+                "@clear", "@inc")),
+    # 0x392A0 fighter_392a0: EAX = slot, EDX = v, EBX = w. 0x33A68 is a leaf allow (its ctx out
+    # buffer is the caller's); 0x46190 (the per-case predicate, stub EAX 0/1), 0x41310, 0x36E78
+    # and rng are stubs.
+    Spec("fighter_392a0", 0x392A0, [
+        Case("m0", {"eax": 0x001077B0, "edx": 0x55, "ebx": 0x40},
+             {**c3e_92_base(0, s5a=0x10, s5d=0x10), 0x00100CE2: le16(1),
+              0x000BEBCA: le16(0x64), 0x00104B1D: b"\x00"}),           # mode 0: 5A += A
+        Case("m1", {"eax": 0x001077B0, "edx": 0x40, "ebx": 0x40},
+             {**c3e_92_base(0, s5a=0x70, s5d=0x10), 0x00100CE2: le16(1),
+              0x00104B1D: b"\x00", 0x00105B36: b"\x00", 0x00104B00: le16(0)}),  # 5A clamp to 0x78
+        Case("m2", {"eax": 0x001077B0, "edx": 1, "ebx": 0x40},
+             {**c3e_92_base(0, s5a=0x10), 0x00100CE2: le16(1), 0x00104B1D: b"\x02",
+              0x00104B00: le16(3), C3D_REC1 + 0x5A: b"\x40"}),         # mode 2: so 5A -= A
+        Case("m3", {"eax": 0x001077B0, "edx": 0x40, "ebx": 0x40},
+             {**c3e_92_base(0, s5a=0x10), 0x00100CE2: le16(1), 0x00104B1D: b"\x02",
+              0x00104B00: le16(3), C3D_REC1 + 0x5A: b"\x05"}),         # mode 2: 5A += A - y
+        Case("m4", {"eax": 0x001077B0, "edx": 0x40, "ebx": 0x40},
+             {**c3e_92_base(0, s5a=0x70), 0x00100CE2: le16(1), 0x00104B1D: b"\x02",
+              0x00104B00: le16(3), C3D_REC1 + 0x5A: b"\x05"}),         # mode 2 x > 0x78
+        Case("m5", {"eax": 0x001077B0, "edx": 1, "ebx": 0x40},
+             {**c3e_92_base(0, s5a=0x10), 0x00100CE2: le16(1), 0x00104B1D: b"\x02",
+              0x001077A8 + 4: le32(0), 0x00104B00: le16(3)}),          # so == 0: early return
+        Case("m6", {"eax": 0x001077B0, "edx": 1, "ebx": 0x40},
+             {**c3e_92_base(0, s5a=0x10), 0x00100CE2: le16(1), 0x00104B1D: b"\x02",
+              0x00105B38: b"\x01", 0x00104B00: le16(3)}),              # 5B38 != 0: L5E7
+        Case("m7", {"eax": 0x001077B0, "edx": 0x20, "ebx": 0x40},
+             {**c3e_92_base(0, s5a=0x70, s5d=0x10), 0x00100CE2: le16(1), 0x00104B1D: b"\x04",
+              0x00104B1F: b"\x03", 0x00107468: le32(50), 0x00104B00: le16(3)}),  # mode 4 E scale
+        Case("m8", {"eax": 0x001077B0, "edx": 1, "ebx": 0x40},
+             {**c3e_92_base(0, s5a=0x10, s5d=0x10), 0x00100CE2: le16(1), 0x00104B1D: b"\x04",
+              0x00104B1F: b"\x00", 0x00104B00: le16(3)}),              # mode 4: E = A
+        Case("m9", {"eax": 0x001077B0, "edx": 1, "ebx": 0x40},
+             {**c3e_92_base(0, s5a=0x78, s5d=0x10, o63=1), 0x00100CE2: le16(1),
+              0x00107813: b"\x55", 0x001078A7: b"\xAA", 0x00104B00: le16(0)},
+             {0x46190: 1}),                                            # 46190 zeroing
+        Case("m10", {"eax": 0x001077B0, "edx": 1, "ebx": 0x60},
+             {**c3e_92_base(0, s53=1, s5a=0x10, s5d=0x10), 0x00100CE2: le16(1),
+              0x000BEBCA: le16(0x64), 0x00104B1D: b"\x00"}),           # +0x53 == 1: B clamped
+        Case("m11", {"eax": 0x001077B0, "edx": 0x20, "ebx": 0x40},
+             {**c3e_92_base(0, s41=0x11, s5a=0x10, s5d=0x10), 0x00100CE2: le16(1),
+              0x000BEBCA: le16(0x64), 0x00104B1D: b"\x00"}),           # +0x41 bit 1: A halved
+        Case("m12", {"eax": 0x001077B0, "edx": 1, "ebx": 0x40},
+             {**c3e_92_base(0, s5a=0x10, s5d=0x10), 0x00100CE2: le16(5),
+              0x000BEBCA + 10: le16(0x100), 0x000BEBCA + 12: le16(0x20),
+              0x00104B1D: b"\x00"}),                          # the k table arm
+        Case("m13", {"eax": 0x001077B0, "edx": 1, "ebx": 0x40},
+             {**c3e_92_base(0, s5a=0x10, s5d=0x10), 0x00100CE2: le16(7),
+              0x00104B1D: b"\x00"}),                                   # k >= 6: 40 arm
+        Case("m14", {"eax": 0x001077B0, "edx": 1, "ebx": 0x40},
+             {**c3e_92_base(0, s5a=0x10, s5d=0x10), 0x00100CE2: le16(0xFF9C),
+              0x00104B1D: b"\x00"}),                                   # the word < 0: 40 arm
+        Case("m15", {"eax": 0x001077B0, "edx": 1, "ebx": 0x40},
+             {**c3e_92_base(0, s5a=0x10, s5d=0x40), 0x00100CE2: le16(1),
+              0x00104B1D: b"\x00"}),                                   # 5D clamp to 0x44
+        Case("m16", {"eax": 0x001077B0, "edx": 1, "ebx": 0x40},
+             {**c3e_92_base(0, s5a=0x10, s5d=0x10), 0x00100CE2: le16(1),
+              0x00104B1D: b"\x00"}),                                   # 5D + B <= 0x44
+        Case("m17", {"eax": 0x001077B0 + 0x94, "edx": 1, "ebx": 0x40},
+             {**c3e_92_base(1, s5a=0x10, s5d=0x10), 0x00100CE0: le16(1),
+              0x00104B1D: b"\x00"}),                                   # the other side
+    ], allow_calls=(0x33A68,),
+       calls=(E.Call(0x46190, (), mode="stub"),
+              E.Call(0x41310, ("eax", "edx"), mode="stub"),
+              E.Call(0x36E78, ("eax",), mode="stub"),
+              E.Call(0x5D7DC, ("eax",), mode="stub")),
+       eax_mask=0,
+       mutants=("@a", "@b", "@clamp", "@half", "@k", "@t", "@zero", "@mode2", "@so", "@clamp44",
+                "@tail")),
+    # 0x3AAFC fighter_reaction_apply: EAX = slot, the reaction is the stack argument s0. 0x33A10,
+    # 0x3AFC4 and 0x3A280 are leaf allows; 0x18B04, 0x39F40, 0x39834, rng, voice, 0x3A0FC,
+    # 0x3AA54, 0x36D20 and the four pose setters are stubs (each has its own row or is in
+    # progress).
+    Spec("fighter_reaction_apply", 0x3AAFC, [
+        # The anim word addresses: the first/third triple (ctx[0] = 1-side) at 0xA672A+6*c1 with
+        # c1 = (char[1-side] << 6) + reaction; the second (ctx[1] = side, frame DSB(self+0x5F))
+        # at 0xA672A+6*c2 with c2 = (char[side] << 6) + DSB(self+0x5F).
+        Case("a0", {"eax": 0x001077B0, "s0": 1},
+             {**c3e_aa_base(0), 0x000A672A: le16(0x44), 0x000A6730: le16(0)}),  # uvar2 bit 6
+        Case("a1", {"eax": 0x001077B0, "s0": 1},
+             {**c3e_aa_base(0, s5f=0xFF), 0x000A672A: le16(0x44),
+              0x000A6730: le16(0)}),                                   # 5F == 0xFF: uvar2 0
+        Case("a2", {"eax": 0x001077B0, "s0": 1},
+             {**c3e_aa_base(0, s5f=0xFF, s53=6, ch0=1), 0x000A672A: le16(0x44),
+              0x000A6730: le16(0)}),                                   # +0x53 == 6, char == 1
+        Case("a3", {"eax": 0x001077B0, "s0": 1},
+             {**c3e_aa_base(0, s5f=0xFF, s54=2), 0x000A672A: le16(0x44),
+              0x000A6730: le16(0)}),                                   # +0x54 == 2
+        Case("a4", {"eax": 0x001077B0, "s0": 1},
+             {**c3e_aa_base(0, s5f=0xFF), 0x000A672A: le16(0x44),
+              0x000A6730: le16(0x100)},
+             {0x3AA54: 0x42}),                                        # ecx bit 8: 0x3AA54
+        Case("a5", {"eax": 0x001077B0, "s0": 1},
+             {**c3e_aa_base(0, s5f=0xFF), 0x000A672A: le16(0x44),
+              0x000A6730: le16(0x2000)},
+             {0x36D20: 0x55}),                                        # ecx bit 0x2000: 0x36D20
+        Case("a6", {"eax": 0x001077B0, "s0": 1},
+             {**c3e_aa_base(0, s5f=0xFF, s54=1), 0x000A672A: le16(0x44),
+              0x000A6730: le16(0), 0x000DE125: b"\x03"}),             # st == 1: 0x3A8E8
+        Case("a7", {"eax": 0x001077B0, "s0": 1},
+             {**c3e_aa_base(0, s5f=0xFF, s54=3), 0x000A672A: le16(0x44),
+              0x000A6730: le16(0), 0x000DE125: b"\x03"}),             # st == 3: return
+        Case("a8", {"eax": 0x001077B0, "s0": 1},
+             {**c3e_aa_base(0, s5f=0xFF), 0x000A672A: le16(0x44),
+              0x000A6730: le16(0x04), 0x000DE125: b"\x03"}),          # ecx bit 4: 0x3A650
+        Case("a9", {"eax": 0x001077B0, "s0": 1},
+             {**c3e_aa_base(0, s5f=0xFF), 0x000A672A: le16(0x44),
+              0x000A6730: le16(0x08), 0x000DE125: b"\x03"}),          # ecx bit 8: 0x3A79C
+        Case("a10", {"eax": 0x001077B0, "s0": 1},
+             {**c3e_aa_base(0, s5f=0xFF), 0x000A672A: le16(0x44),
+              0x000A6730: le16(0), 0x000DE125: b"\x03"}),             # default: 0x3A504
+        Case("a11", {"eax": 0x001077B0, "s0": 1},
+             {**c3e_aa_base(0, s5f=0xFF, o5f=0x10), 0x000A672A: le16(0x44),
+              0x000A6730: le16(0), 0x000DE125: b"\x03",
+              0x000BE008: le16(0x1234)}),                             # the 0x3A280 voice arm
+        Case("a12", {"eax": 0x001077B0 + 0x94, "s0": 2},
+             {**c3e_aa_base(1, s5f=0), 0x000A672A: le16(0),
+              0x000A6736: le16(0x200)}),                               # side 1, c1 = 2
+    ], allow_calls=(0x33A10, 0x3AFC4, 0x3A280),
+       calls=(E.Call(0x18B04, ("eax",), mode="stub"),
+              P6_39F40,
+              POSE,
+              E.Call(0x2C3FC, ("eax",), mode="stub", eax=1),
+              E.Call(0x3A0FC, ("eax",), mode="stub", clobbers=("ebp",)),
+              E.Call(0x3AA54, ("eax",), mode="stub"),
+              E.Call(0x36D20, ("eax",), mode="stub"),
+              E.Call(0x3A504, ("eax", "edx"), mode="stub", clobbers=("edx",)),
+              E.Call(0x3A650, ("eax", "edx"), mode="stub", clobbers=("edx",)),
+              E.Call(0x3A79C, ("eax", "edx"), mode="stub", clobbers=("edx",)),
+              E.Call(0x3A8E8, ("eax", "edx"), mode="stub", clobbers=("edx",))),
+       eax_mask=0,
+       mutants=("@u2", "@s5f", "@s6", "@s54", "@e200", "@e100", "@e2000", "@st", "@s42", "@inc",
+                "@voice", "@call0fc")),
+    # 0x3A280 fighter_3a280: EAX = code (the callers read AL): 1 for 0x20..0x3F and, on the raw's
+    # full EAX, 0x10..0x17. The 0x100010/0x100020 cases pin the raw's full-width compares.
+    Spec("fighter_3a280", 0x3A280, [
+        Case("r%d" % n, {"eax": v})
+        for n, v in enumerate((0x0F, 0x10, 0x17, 0x18, 0x1F, 0x20, 0x3F, 0x40, 0x100010, 0x100020))
+    ], eax_mask=0xFF, mutants=("@lo", "@cast")),
+    # 0x4F944 fighter_4f944: EAX = v. The clamp is signed (0x2000000A > 0x14 but negative values
+    # are kept); the byte, the two words and the 0x104AE9 bit are the observation. No calls.
+    Spec("fighter_4f944", 0x4F944, [
+        Case("v%d" % n, {"eax": v}, c3e_4f_case())
+        for n, v in enumerate((0, 5, 0x14, 0x15, 0xFFFFFFFF, 0x80000000, 0x12345678))
+    ], eax_mask=0, mutants=("@cmp", "@f0", "@e8", "@ae9")),
+    # 0x33A68 fighter_ctx_rec_swap: EAX = the six-dword out buffer, EDX = rec; the side is
+    # rec+0x51 (masked) and the slot records come from 0x1077B0.
+    Spec("fighter_ctx_rec_swap", 0x33A68, [
+        Case("s0", {"eax": 0x10A000, "edx": 0x10A100}, c3e_33_case(0x10A000, 0x10A100, 0)),
+        Case("s1", {"eax": 0x10A000, "edx": 0x10A100}, c3e_33_case(0x10A000, 0x10A100, 1)),
+        Case("s2", {"eax": 0x10A000, "edx": 0x10A100}, c3e_33_case(0x10A000, 0x10A100, 2)),
+        Case("s3", {"eax": 0x10A030, "edx": 0x10A100}, c3e_33_case(0x10A030, 0x10A100, 1)),
+    ], eax_mask=0, mutants=("@side", "@stride", "@rec")),
+    # 0x36E78 fighter_36e78: EAX = slot. The +0x5B != 0 arm (clear it, +0x5A = 0x78 - b, +0x41
+    # bit 3), else the +0x40 mask, +0x5D = 0, the other record's reset and the 0x29BC8 palette
+    # call (stub).
+    Spec("fighter_36e78", 0x36E78, [
+        Case("b0", {"eax": 0x001077B0}, c3e_6e_case(0, 0x30)),
+        Case("b1", {"eax": 0x001077B0}, c3e_6e_case(0, 0x00)),
+        Case("b2", {"eax": 0x001077B0}, c3e_6e_case(0, 0x00, live=False)),
+        Case("b3", {"eax": 0x001077B0 + 0x94}, c3e_6e_case(1, 0x30)),
+        Case("b4", {"eax": 0x001077B0 + 0x94}, c3e_6e_case(1, 0x00)),
+    ], calls=(P6_29BC8,), eax_mask=0, mutants=("@f5b", "@f40", "@other", "@pal")),
+    # 0x2CAA8 config_not_free_play: `sete al` on DSB(0x105D60) (AL is what the caller reads).
+    Spec("config_not_free_play", 0x2CAA8, [
+        Case("f0", {}, {0x00105D60: b"\x00"}),
+        Case("f1", {}, {0x00105D60: b"\x01"}),
+        Case("f2", {}, {0x00105D60: b"\xFF"}),
+    ], eax_mask=0xFF, mutants=("@eq", "@byte", "@zero")),
+    # 0x468D8 ai_pred_468d8: EAX = side; 1 when the slot's +0x10 == 0x22BEC, the record's +0x24
+    # low 31 bits clear and +0x54 != 2, else +0x52 == 7. 0x33A10 runs on both sides.
+    Spec("ai_pred_468d8", 0x468D8, [
+        Case("p0", {"eax": 0}, c3e_8d_case(0, 0x22BEC, 0, 2, 0)),
+        Case("p1", {"eax": 0}, c3e_8d_case(0, 0x22BEC, 0, 1, 0)),
+        Case("p2", {"eax": 0}, c3e_8d_case(0, 0x22BEC, 1, 0, 0)),
+        Case("p3", {"eax": 0}, c3e_8d_case(0, 0x22BEC, 0, 2, 7)),
+        Case("p4", {"eax": 0}, c3e_8d_case(0, 0x1234, 0, 0, 7)),
+        Case("p5", {"eax": 0}, c3e_8d_case(0, 0x1234, 0, 0, 0)),
+        Case("p6", {"eax": 1}, c3e_8d_case(1, 0x22BEC, 0, 1, 0)),
+    ], allow_calls=(0x33A10,), eax_mask=0xFF,
+       mutants=("@hand", "@low", "@byte", "@side")),
+    # 0x2D974 config_field_get: EAX = field; 0xFFFFFFFF above 0x3E, else the packed nibble/byte
+    # walk of 0x2D300[field]'s descriptor over 0x105DE1 and the 0x105DAF trailer.
+    Spec("config_field_get", 0x2D974, [
+        Case("c0", {"eax": 6}, c3e_cf_case(6, 0x0000, b"\xAB\x3C")),
+        Case("c1", {"eax": 7}, c3e_cf_case(7, 0x4000, b"\xAB\xCD")),
+        Case("c2", {"eax": 8}, c3e_cf_case(8, 0x8040, b"\xAB\xAB\xCD")),
+        Case("c3", {"eax": 9}, c3e_cf_case(9, 0xC000, b"\xAB\xAB\xCD")),
+        Case("c4", {"eax": 0x0A}, c3e_cf_case(0x0A, 0x20000, b"\x06\x05\x04\x03\x21")),
+        Case("c5", {"eax": 0x0B}, c3e_cf_case(0x0B, 0x0005, b"\x0C\x00")),
+        Case("c6", {"eax": 0x3F}, c3e_cf_case(0x3F, 0)),
+        Case("c7", {"eax": 0x3E}, c3e_cf_case(0x3E, 0x0040, b"\x5A")),
+        Case("c8", {"eax": 0x0C}, c3e_cf_case(0x0C, 0x14040, b"\x5A\x11\x22\x33")),
+    ], eax_mask=0xFFFFFFFF, mutants=("@odd", "@width", "@trail", "@field", "@hi")),
+    # 0x46190 fighter_46190: DIP field 0x29 bit 0x800 and free play; 0x2D974/0x2CAA8 run real
+    # (each has its own row). Mask 0xFF (AL).
+    Spec("fighter_46190", 0x46190, [
+        Case("q0", {}, c3e_90b_case(0x3C000, 0, b"\x00\x08")),
+        Case("q1", {}, c3e_90b_case(0x3C000, 0, b"\x00\x07")),
+        Case("q2", {}, c3e_90b_case(0x3C000, 1, b"\x00\x08")),
+        Case("q3", {}, c3e_90b_case(0x3C000, 0xFF, b"\x00\x08")),
+        Case("q4", {}, c3e_90b_case(0x0000, 0, b"\x80")),
+    ], calls=(E.Call(0x2D974, ("eax",), mode="real"),
+              E.Call(0x2CAA8, (), mode="real")),
+       eax_mask=0xFF, mutants=("@bit", "@notfree", "@field")),
+    # 0x36D20 fighter_36d20: EAX = slot. 0x468D8 (stub, per-case EAX) gates; +0x54 == 2 runs the
+    # inlined 0x36CE4 body (the port factors it into a call, unrecorded), else rec+0x34 = 0 and
+    # 0x36BC8 (stub); the anim runs through 0x2BC30.
+    Spec("fighter_36d20", 0x36D20, [
+        Case("d0", {"eax": 0x001077B0}, c3e_36d_case(0, 0, 0x33), {0x468D8: 1}),
+        Case("d1", {"eax": 0x001077B0}, c3e_36d_case(0, 2, 0x33, mode=3), {0x468D8: 0}),
+        Case("d2", {"eax": 0x001077B0}, c3e_36d_case(0, 2, 0x33, mode=0), {0x468D8: 0}),
+        Case("d3", {"eax": 0x001077B0}, c3e_36d_case(0, 2, 0x33, mode=0x22), {0x468D8: 0}),
+        Case("d4", {"eax": 0x001077B0}, c3e_36d_case(0, 0, 0x33), {0x468D8: 0}),
+        Case("d5", {"eax": 0x001077B0 + 0x94}, c3e_36d_case(1, 1, 0x44, mode=0), {0x468D8: 0}),
+    ], calls=(E.Call(0x468D8, ("eax",), mode="stub"),
+              E.Call(0x36BC8, ("eax", "edx"), mode="stub", clobbers=("edx",)),
+              ANIM_BEGIN),
+       eax_mask=0xFF, mutants=("@pred", "@s54", "@anim", "@rec", "@ret")),
+    # The four 0x3A5xx/0x3A6xx/0x3A8xx pose setters: EAX = side, EDX = the frame delta; each
+    # shares the 0x3A50E..0x3A56B body with its own callback and glob pair. 0x33A10 runs on both
+    # sides (allow).
+    Spec("fighter_pose_3a504", 0x3A504, [
+        Case("p0", {"eax": 0, "edx": 0x10, "ebx": 0x33}, c3e_pose_case(0, 0x10, 0x107D14, 0x107D10)),
+        Case("p1", {"eax": 1, "edx": 0x80, "ebx": 0x33}, c3e_pose_case(1, 0x80, 0x107D14, 0x107D10)),
+    ], allow_calls=(0x33A10,), eax_mask=0, mutants=("@s52", "@cb", "@glob")),
+    Spec("fighter_pose_3a650", 0x3A650, [
+        Case("p0", {"eax": 0, "edx": 0x10, "ebx": 0x33}, c3e_pose_case(0, 0x10, 0x107D0C, 0x107D00)),
+        Case("p1", {"eax": 1, "edx": 0x80, "ebx": 0x33}, c3e_pose_case(1, 0x80, 0x107D0C, 0x107D00)),
+    ], allow_calls=(0x33A10,), eax_mask=0, mutants=("@s52", "@cb", "@glob")),
+    Spec("fighter_pose_3a79c", 0x3A79C, [
+        Case("p0", {"eax": 0, "edx": 0x10, "ebx": 0x33}, c3e_pose_case(0, 0x10, 0x107D08, 0x107D04)),
+        Case("p1", {"eax": 1, "edx": 0x80, "ebx": 0x33}, c3e_pose_case(1, 0x80, 0x107D08, 0x107D04)),
+    ], allow_calls=(0x33A10,), eax_mask=0, mutants=("@s52", "@cb", "@glob")),
+    Spec("fighter_pose_3a8e8", 0x3A8E8, [
+        Case("p0", {"eax": 0, "edx": 0x10, "ebx": 0x33}, c3e_pose_case(0, 0x10, 0x107CF8, 0x107CFC)),
+        Case("p1", {"eax": 1, "edx": 0x80, "ebx": 0x33}, c3e_pose_case(1, 0x80, 0x107CF8, 0x107CFC)),
+    ], allow_calls=(0x33A10,), eax_mask=0, mutants=("@s52", "@cb", "@glob")),
+    # 0x3A0FC fighter_3a0fc: EAX = side. 0x33950/0x3AFC4 are allows; the two effect spawns
+    # (0x2AE14, per-case stub EAX) and their 0x2BC30 animates are stubs. The 0x105B3A jump table
+    # (entries 0..2 spawn, 3 skips) and the per-side stream tables are the branches.
+    Spec("fighter_3a0fc", 0x3A0FC, [
+        Case("f0", {"eax": 0}, c3e_0fc_case(0, 5, stream=0x1234, key=1, f0aec=0x1000,
+             f100ad8=2, layer=3, other2c=0x55667788), {0x2AE14: 0x10A480}),
+        Case("f1", {"eax": 1}, c3e_0fc_case(1, 0, stream=0x2222, key=1, f0aec=0x2000,
+             f100ad8=1, layer=5, other2c=0x99AABBCC), {0x2AE14: 0x10A480}),
+        Case("f2", {"eax": 0}, c3e_0fc_case(0, 5, stream=0, key=2, f0aec=0x1000), {0x2AE14: 0x10A480}),
+        Case("f3", {"eax": 0}, c3e_0fc_case(0, 5, stream=0x1234, key=3, b5b3a=4), {0x2AE14: 0x10A480}),
+        Case("f4", {"eax": 0}, c3e_0fc_case(0, 5, stream=0x1234, key=0, s28=0x4000), {}),
+        Case("f5", {"eax": 1}, c3e_0fc_case(1, 2, stream=0x3333, key=2, s28=0x4000,
+             f0aec=0x3000), {0x2AE14: 0x10A480}),
+        Case("f6", {"eax": 1}, c3e_0fc_case(1, 2, stream=0x3333, key=3), {0x2AE14: 0x10A480}),
+        Case("f7", {"eax": 0}, c3e_0fc_case(0, 3, stream=0x4444, key=9, b5b3a=3), {}),
+        Case("f8", {"eax": 1}, c3e_0fc_case(1, 2, stream=0, key=4, b5b3a=4), {}),
+    ], allow_calls=(0x33950, 0x3AFC4), calls=(SPAWN, ANIM_BEGIN), eax_mask=0,
+       mutants=("@frame", "@face", "@off", "@layer", "@stream", "@key", "@s59")),
+]
+
 SPECS = [
     Spec("rng_next", 0x5D7DC, [
         Case("r1", {"eax": 0x1234}, {DS_RNG: le32(0x12345678)}),
@@ -6017,7 +6526,7 @@ SPECS = [
         Case("d0", {}, {DS_1078FC: b"\x00"}),
         Case("d1", {"eax": U6_REC}, {DS_1078FC: b"\x5a"}),
     ], eax_mask=0),
-] + E3_SPECS + P1_SPECS + P1_ANIM_SPECS + P2_SPECS + P3_SPECS + P6_SPECS + P45_SPECS + C1_SPECS + C2_SPECS + C2B_SPECS + C3_SPECS + C3B_SPECS + P7_SPECS + P8_SPECS + C3C_SPECS + C3D_SPECS
+] + E3_SPECS + P1_SPECS + P1_ANIM_SPECS + P2_SPECS + P3_SPECS + P6_SPECS + P45_SPECS + C1_SPECS + C2_SPECS + C2B_SPECS + C3_SPECS + C3B_SPECS + P7_SPECS + P8_SPECS + C3C_SPECS + C3D_SPECS + C3E_SPECS
 
 
 # ---- driver -------------------------------------------------------------------------------------
diff --git a/tools/tests/test_diff_verify.py b/tools/tests/test_diff_verify.py
index 20737b2..507fddc 100644
--- a/tools/tests/test_diff_verify.py
+++ b/tools/tests/test_diff_verify.py
@@ -1379,6 +1379,114 @@ C3D_KINDS = {
     "pset_write@call": {'call #0', 'call #1'},
 }
 
+C3E_MASKS = {
+    "fighter_39040": 0x0,
+    "fighter_392a0": 0x0,
+    "fighter_reaction_apply": 0x0,
+    "fighter_3a280": 0xff,
+    "fighter_4f944": 0x0,
+    "fighter_ctx_rec_swap": 0x0,
+    "fighter_36e78": 0x0,
+    "config_not_free_play": 0xff,
+    "ai_pred_468d8": 0xff,
+    "config_field_get": 0xffffffff,
+    "fighter_46190": 0xff,
+    "fighter_36d20": 0xff,
+    "fighter_pose_3a504": 0x0,
+    "fighter_pose_3a650": 0x0,
+    "fighter_pose_3a79c": 0x0,
+    "fighter_pose_3a8e8": 0x0,
+    "fighter_3a0fc": 0x0,
+}
+C3E_KINDS = {
+    "ai_pred_468d8@byte": {'eax'},
+    "ai_pred_468d8@hand": {'eax'},
+    "ai_pred_468d8@low": {'eax'},
+    "ai_pred_468d8@side": {'eax'},
+    "config_field_get@field": {'eax'},
+    "config_field_get@hi": {'eax'},
+    "config_field_get@odd": {'eax'},
+    "config_field_get@trail": {'eax'},
+    "config_field_get@width": {'eax'},
+    "config_not_free_play@byte": {'eax'},
+    "config_not_free_play@eq": {'eax'},
+    "config_not_free_play@zero": {'eax'},
+    "fighter_36d20@anim": {'call #1'},
+    "fighter_36d20@pred": {'byte', 'call #1', 'eax'},
+    "fighter_36d20@rec": {'byte', 'call #1 memory'},
+    "fighter_36d20@ret": {'eax'},
+    "fighter_36d20@s54": {'byte', 'call #1', 'call #1 memory'},
+    "fighter_36e78@f40": {'byte', 'call #0 memory'},
+    "fighter_36e78@f5b": {'byte', 'call #0'},
+    "fighter_36e78@other": {'byte', 'call #0'},
+    "fighter_36e78@pal": {'call #0'},
+    "fighter_39040@a23": {'byte', 'call #3', 'call #4'},
+    "fighter_39040@a41": {'byte', 'call #3'},
+    "fighter_39040@b4": {'byte', 'call #0 memory', 'call #1 memory', 'call #2 memory', 'call #3 memory', 'call #4 memory'},
+    "fighter_39040@clear": {'byte'},
+    "fighter_39040@delta": {'call #2'},
+    "fighter_39040@gate": {'byte', 'call #0', 'call #1'},
+    "fighter_39040@id": {'call #4'},
+    "fighter_39040@inc": {'byte', 'call #2 memory', 'call #3 memory', 'call #4 memory'},
+    "fighter_39040@lim": {'byte', 'call #0 memory', 'call #1 memory', 'call #2 memory', 'call #3 memory', 'call #4 memory'},
+    "fighter_39040@rng2": {'call #4'},
+    "fighter_39040@voice": {'call #4'},
+    "fighter_392a0@a": {'byte', 'call #1 memory'},
+    "fighter_392a0@b": {'byte', 'call #2 memory'},
+    "fighter_392a0@clamp": {'byte'},
+    "fighter_392a0@clamp44": {'byte'},
+    "fighter_392a0@half": {'byte', 'call #1 memory'},
+    "fighter_392a0@k": {'byte'},
+    "fighter_392a0@mode2": {'byte', 'call #1', 'call #1 memory'},
+    "fighter_392a0@so": {'byte', 'call #1'},
+    "fighter_392a0@t": {'byte', 'call #2 memory'},
+    "fighter_392a0@tail": {'call #2'},
+    "fighter_392a0@zero": {'byte', 'call #1', 'call #2 memory'},
+    "fighter_3a0fc@face": {'call #0'},
+    "fighter_3a0fc@frame": {'byte', 'call #0', 'call #1', 'call #2', 'call #3'},
+    "fighter_3a0fc@key": {'call #3'},
+    "fighter_3a0fc@layer": {'call #0', 'call #2'},
+    "fighter_3a0fc@off": {'call #0', 'call #2'},
+    "fighter_3a0fc@s59": {'byte', 'call #1 memory', 'call #3 memory'},
+    "fighter_3a0fc@stream": {'call #0', 'call #1', 'call #1 memory', 'call #2', 'call #3'},
+    "fighter_3a280@cast": {'eax'},
+    "fighter_3a280@lo": {'eax'},
+    "fighter_46190@bit": {'call #1', 'eax'},
+    "fighter_46190@field": {'call #0', 'call #1', 'eax'},
+    "fighter_46190@notfree": {'call #1', 'eax'},
+    "fighter_4f944@ae9": {'byte'},
+    "fighter_4f944@cmp": {'byte'},
+    "fighter_4f944@e8": {'byte'},
+    "fighter_4f944@f0": {'byte'},
+    "fighter_ctx_rec_swap@rec": {'byte'},
+    "fighter_ctx_rec_swap@side": {'byte'},
+    "fighter_ctx_rec_swap@stride": {'byte'},
+    "fighter_pose_3a504@cb": {'byte'},
+    "fighter_pose_3a504@glob": {'byte'},
+    "fighter_pose_3a504@s52": {'byte'},
+    "fighter_pose_3a650@cb": {'byte'},
+    "fighter_pose_3a650@glob": {'byte'},
+    "fighter_pose_3a650@s52": {'byte'},
+    "fighter_pose_3a79c@cb": {'byte'},
+    "fighter_pose_3a79c@glob": {'byte'},
+    "fighter_pose_3a79c@s52": {'byte'},
+    "fighter_pose_3a8e8@cb": {'byte'},
+    "fighter_pose_3a8e8@glob": {'byte'},
+    "fighter_pose_3a8e8@s52": {'byte'},
+    "fighter_reaction_apply@call0fc": {'call #2', 'call #2 memory', 'call #3', 'call #3 memory', 'call #4'},
+    "fighter_reaction_apply@e100": {'byte', 'call #3'},
+    "fighter_reaction_apply@e200": {'byte', 'call #3', 'call #4'},
+    "fighter_reaction_apply@e2000": {'byte', 'call #3'},
+    "fighter_reaction_apply@inc": {'byte', 'call #3 memory', 'call #4 memory'},
+    "fighter_reaction_apply@s42": {'byte', 'call #3 memory', 'call #4 memory'},
+    "fighter_reaction_apply@s54": {'byte', 'call #3', 'call #4'},
+    "fighter_reaction_apply@s5f": {'byte', 'call #3', 'call #4'},
+    "fighter_reaction_apply@s6": {'byte', 'call #3', 'call #4'},
+    "fighter_reaction_apply@st": {'call #3'},
+    "fighter_reaction_apply@u2": {'byte', 'call #3', 'call #4'},
+    "fighter_reaction_apply@voice": {'call #2', 'call #3', 'call #3 memory', 'call #4'},
+}
+
 @needs_unicorn
 @unittest.skipUnless((os.path.exists(DIFFRUN) and os.path.exists(EXE)) or REQUIRED,
                      "build/diffrun or PRAGE.EXE absent")
@@ -1403,7 +1511,7 @@ class RealFunctionTests(unittest.TestCase):
                                              "fighter_slot_flag", "hit_anim_ctx", "hit_anim_start_b",
                                              "host_1b890", "rng_next"] + list(P1_MASKS) + list(P2_MASKS)
                                             + list(P3_MASKS) + list(P45_MASKS) + list(C1_MASKS)
-                                            + list(P6_MASKS) + list(C2_MASKS) + list(C2B_MASKS) + list(C3_MASKS) + list(C3B_MASKS) + list(C3C_MASKS) + list(C3D_MASKS) + list(P7_MASKS) + list(P8_MASKS)))
+                                            + list(P6_MASKS) + list(C2_MASKS) + list(C2B_MASKS) + list(C3_MASKS) + list(C3B_MASKS) + list(C3C_MASKS) + list(C3D_MASKS) + list(C3E_MASKS) + list(P7_MASKS) + list(P8_MASKS)))
         for name, r in self.real.items():
             if name == "host_1b890":       # the named gap (record E3 §E3.8), tested on its own below
                 continue
@@ -1419,7 +1527,7 @@ class RealFunctionTests(unittest.TestCase):
             "fighter_45878@mutant", "fighter_ctx_same@mutant", "fighter_slot_flag@mutant",
             "hit_anim_ctx@mutant", "hit_anim_start_b@mutant", "hit_anim_start_b@set", "rng_next@mutant"]
             + list(P1_KINDS) + list(P2_KINDS) + list(P3_KINDS) + list(P45_KINDS) + list(C1_KINDS)
-            + list(P6_KINDS) + list(C2_KINDS) + list(C2B_KINDS) + list(C3_KINDS) + list(C3B_KINDS) + list(C3C_KINDS) + list(C3D_KINDS) + list(P7_KINDS) + list(P8_KINDS)))
+            + list(P6_KINDS) + list(C2_KINDS) + list(C2B_KINDS) + list(C3_KINDS) + list(C3B_KINDS) + list(C3C_KINDS) + list(C3D_KINDS) + list(C3E_KINDS) + list(P7_KINDS) + list(P8_KINDS)))
         for name, r in self.mut.items():
             self.assertEqual(r.verdict, "MISMATCH", name)
 
@@ -1484,7 +1592,7 @@ class RealFunctionTests(unittest.TestCase):
             "fighter_3640c": 0, "fighter_37dcc": 0,
             "fighter_23130": 0xFF, "fighter_45878": 0, "anim_10fa8": 0, "anim_3e4e4": 0,
             "fighter_ctx_same": 0, "hit_anim_ctx": 0, "hit_anim_start_b": 0, "host_1b890": 0xFFFFFFFF,
-            **P1_MASKS, **P2_MASKS, **P3_MASKS, **P45_MASKS, **C1_MASKS, **P6_MASKS, **C2_MASKS, **C2B_MASKS, **C3_MASKS, **C3B_MASKS, **C3C_MASKS, **C3D_MASKS,
+            **P1_MASKS, **P2_MASKS, **P3_MASKS, **P45_MASKS, **C1_MASKS, **P6_MASKS, **C2_MASKS, **C2B_MASKS, **C3_MASKS, **C3B_MASKS, **C3C_MASKS, **C3D_MASKS, **C3E_MASKS,
             **P7_MASKS, **P8_MASKS})
         # with the full mask the slot-flag original's scratch bits (case f9: EAX = 0x201) differ
         spec = dataclasses.replace([s for s in V.SPECS if s.name == "fighter_slot_flag"][0],
@@ -2220,6 +2328,103 @@ class RealFunctionTests(unittest.TestCase):
         ):
             self.assertEqual(sorted({p.split(":")[0] for p in self.mut[name].problems}), ids, name)
 
+    def test_each_c3e_mutant_is_caught_by_what_it_breaks(self):
+        # track P batch C3e (record 2026-10-05-reverse-c3e): what alone catches each mutant
+        for name, want in C3E_KINDS.items():
+            got = {p.split(": ", 1)[1].split(":")[0] if p.split(": ", 1)[1].startswith("call #")
+                   else p.split(": ", 1)[1].split(" ")[0] for p in self.mut[name].problems}
+            self.assertEqual(got, want, name)
+        # the exact case set that alone catches each mutant (measured on the prototype)
+        for name, ids in (
+        ("ai_pred_468d8@byte", ['p3', 'p4']),
+        ("ai_pred_468d8@hand", ['p1', 'p6']),
+        ("ai_pred_468d8@low", ['p2']),
+        ("ai_pred_468d8@side", ['p1', 'p3', 'p4', 'p6']),
+        ("config_field_get@field", ['c0', 'c1', 'c2', 'c3', 'c4', 'c5', 'c7', 'c8']),
+        ("config_field_get@hi", ['c7']),
+        ("config_field_get@odd", ['c0', 'c1', 'c2', 'c3', 'c4', 'c5', 'c7', 'c8']),
+        ("config_field_get@trail", ['c5']),
+        ("config_field_get@width", ['c0', 'c1', 'c2', 'c3', 'c4', 'c5', 'c7', 'c8']),
+        ("config_not_free_play@byte", ['f1', 'f2']),
+        ("config_not_free_play@eq", ['f0', 'f1', 'f2']),
+        ("config_not_free_play@zero", ['f0']),
+        ("fighter_36d20@anim", ['d2']),
+        ("fighter_36d20@pred", ['d0', 'd1', 'd2', 'd3', 'd4', 'd5']),
+        ("fighter_36d20@rec", ['d4', 'd5']),
+        ("fighter_36d20@ret", ['d1', 'd2', 'd3', 'd4', 'd5']),
+        ("fighter_36d20@s54", ['d4', 'd5']),
+        ("fighter_36e78@f40", ['b1', 'b2', 'b4']),
+        ("fighter_36e78@f5b", ['b0', 'b3']),
+        ("fighter_36e78@other", ['b1']),
+        ("fighter_36e78@pal", ['b1']),
+        ("fighter_39040@a23", ['g16', 'g6']),
+        ("fighter_39040@a41", ['g7', 'g8']),
+        ("fighter_39040@b4", ['g1', 'g10', 'g11', 'g12', 'g13', 'g14', 'g15', 'g16', 'g2', 'g3', 'g4', 'g5', 'g6', 'g7', 'g8', 'g9']),
+        ("fighter_39040@clear", ['g0', 'g1', 'g10', 'g11', 'g12', 'g13', 'g14', 'g15', 'g16', 'g2', 'g3', 'g4', 'g5', 'g6', 'g7', 'g8', 'g9']),
+        ("fighter_39040@delta", ['g10', 'g11', 'g12', 'g13', 'g14', 'g15', 'g16', 'g5', 'g6', 'g7', 'g8']),
+        ("fighter_39040@gate", ['g0']),
+        ("fighter_39040@id", ['g5']),
+        ("fighter_39040@inc", ['g1', 'g10', 'g11', 'g12', 'g13', 'g14', 'g15', 'g16', 'g2', 'g3', 'g4', 'g5', 'g6', 'g7', 'g8', 'g9']),
+        ("fighter_39040@lim", ['g13', 'g2', 'g3']),
+        ("fighter_39040@rng2", ['g10', 'g9']),
+        ("fighter_39040@voice", ['g10', 'g11', 'g12', 'g13', 'g5', 'g9']),
+        ("fighter_392a0@a", ['m0']),
+        ("fighter_392a0@b", ['m0', 'm1', 'm11', 'm13', 'm14', 'm16', 'm17', 'm2', 'm3', 'm4', 'm6', 'm7', 'm8']),
+        ("fighter_392a0@clamp", ['m10']),
+        ("fighter_392a0@clamp44", ['m15']),
+        ("fighter_392a0@half", ['m11']),
+        ("fighter_392a0@k", ['m12']),
+        ("fighter_392a0@mode2", ['m2', 'm3', 'm5']),
+        ("fighter_392a0@so", ['m2', 'm3', 'm5']),
+        ("fighter_392a0@t", ['m0', 'm1', 'm10', 'm11', 'm12', 'm16', 'm17', 'm2', 'm3', 'm4', 'm6', 'm7', 'm8']),
+        ("fighter_392a0@tail", ['m1', 'm9']),
+        ("fighter_392a0@zero", ['m9']),
+        ("fighter_3a0fc@face", ['f4', 'f5']),
+        ("fighter_3a0fc@frame", ['f0', 'f1', 'f2', 'f3', 'f4', 'f5', 'f6']),
+        ("fighter_3a0fc@key", ['f0', 'f1']),
+        ("fighter_3a0fc@layer", ['f0', 'f1']),
+        ("fighter_3a0fc@off", ['f0', 'f1', 'f2', 'f3', 'f4', 'f5', 'f6']),
+        ("fighter_3a0fc@s59", ['f0', 'f1', 'f2', 'f3', 'f5', 'f6']),
+        ("fighter_3a0fc@stream", ['f0', 'f1', 'f4', 'f5', 'f6']),
+        ("fighter_3a280@cast", ['r8', 'r9']),
+        ("fighter_3a280@lo", ['r1', 'r3']),
+        ("fighter_46190@bit", ['q0', 'q1', 'q2', 'q3']),
+        ("fighter_46190@field", ['q0', 'q1', 'q2', 'q3', 'q4']),
+        ("fighter_46190@notfree", ['q0', 'q2', 'q3']),
+        ("fighter_4f944@ae9", ['v0', 'v1', 'v2', 'v3', 'v4', 'v5', 'v6']),
+        ("fighter_4f944@cmp", ['v4', 'v5']),
+        ("fighter_4f944@e8", ['v0', 'v1', 'v2', 'v3', 'v4', 'v5', 'v6']),
+        ("fighter_4f944@f0", ['v0', 'v1', 'v2', 'v3', 'v4', 'v5', 'v6']),
+        ("fighter_ctx_rec_swap@rec", ['s0', 's1', 's3']),
+        ("fighter_ctx_rec_swap@side", ['s0', 's1', 's2', 's3']),
+        ("fighter_ctx_rec_swap@stride", ['s0', 's1', 's2', 's3']),
+        ("fighter_pose_3a504@cb", ['p0', 'p1']),
+        ("fighter_pose_3a504@glob", ['p0', 'p1']),
+        ("fighter_pose_3a504@s52", ['p0', 'p1']),
+        ("fighter_pose_3a650@cb", ['p0', 'p1']),
+        ("fighter_pose_3a650@glob", ['p0', 'p1']),
+        ("fighter_pose_3a650@s52", ['p0', 'p1']),
+        ("fighter_pose_3a79c@cb", ['p0', 'p1']),
+        ("fighter_pose_3a79c@glob", ['p0', 'p1']),
+        ("fighter_pose_3a79c@s52", ['p0', 'p1']),
+        ("fighter_pose_3a8e8@cb", ['p0', 'p1']),
+        ("fighter_pose_3a8e8@glob", ['p0', 'p1']),
+        ("fighter_pose_3a8e8@s52", ['p0', 'p1']),
+        ("fighter_reaction_apply@call0fc", ['a0', 'a1', 'a10', 'a11', 'a12', 'a2', 'a3', 'a4', 'a5', 'a6', 'a7', 'a8', 'a9']),
+        ("fighter_reaction_apply@e100", ['a4']),
+        ("fighter_reaction_apply@e200", ['a12', 'a4']),
+        ("fighter_reaction_apply@e2000", ['a5']),
+        ("fighter_reaction_apply@inc", ['a0', 'a1', 'a10', 'a11', 'a12', 'a2', 'a3', 'a4', 'a5', 'a6', 'a7', 'a8', 'a9']),
+        ("fighter_reaction_apply@s42", ['a0', 'a1', 'a10', 'a11', 'a12', 'a2', 'a3', 'a4', 'a5', 'a6', 'a7', 'a8', 'a9']),
+        ("fighter_reaction_apply@s54", ['a3']),
+        ("fighter_reaction_apply@s5f", ['a0']),
+        ("fighter_reaction_apply@s6", ['a2']),
+        ("fighter_reaction_apply@st", ['a6', 'a7']),
+        ("fighter_reaction_apply@u2", ['a0']),
+        ("fighter_reaction_apply@voice", ['a11']),
+        ):
+            self.assertEqual(sorted({p.split(":")[0] for p in self.mut[name].problems}), ids, name)
+
     def test_each_p7_mutant_is_caught_by_what_it_breaks(self):
         # track P batch 7 (record 2026-10-04-reverse-p7): what alone catches each mutant
         for name, want in P7_KINDS.items():
@@ -2254,16 +2459,17 @@ class RealFunctionTests(unittest.TestCase):
         0x2A820: ("edx",), 0x2AC80: (), 0x2AD40: ("edx", "edi", "ebp"), 0x2AE14: ("ebx", "ecx", "edx"),
         0x2B150: (), 0x2B2A0: ("ebx", "edx"), 0x2BC30: ("edx",), 0x2BCF4: ("edx",), 0x2BD44: ("edx",),
         0x2C3FC: (), 0x33754: (), 0x33864: (), 0x34D8C: (), 0x35838: ("ebx", "edx"), 0x365C8: ("ebx", "edx"),
-        0x36638: ("edx",), 0x36870: ("esi", "edi", "ebp"), 0x36BC8: ("edx",), 0x36CE4: (), 0x36D98: (),
+        0x36638: ("edx",), 0x36870: ("esi", "edi", "ebp"), 0x36BC8: ("edx",), 0x36CE4: (), 0x36D20: (), 0x36D98: (), 0x36E78: (),
         0x37178: ("esi", "edi", "ebp"), 0x379C4: ("ecx", "esi", "edi", "ebp"), 0x37D18: ("edx",), 0x38034: (),
-        0x38154: (), 0x385B0: (), 0x38BC8: (), 0x39040: (), 0x39280: (), 0x392A0: ("ebx", "edx"),
+        0x38154: (), 0x385B0: (), 0x38BC8: (), 0x38D90: (), 0x38FEC: (), 0x39040: (), 0x39280: (), 0x392A0: ("ebx", "edx"),
         0x39738: ("edx",), 0x39834: ("edx", "ebp"), 0x39A10: ("edx",), 0x39EFC: (), 0x39F40: ("ebx", "edx"),
-        0x39FB0: (), 0x3A95C: ("edx",), 0x3A9D8: ("edx",), 0x3AA54: (),
+        0x39FB0: (), 0x3A0FC: ("ebp",), 0x3A504: ("edx",), 0x3A650: ("edx",), 0x3A79C: ("edx",),
+        0x3A8E8: ("edx",), 0x3A95C: ("edx",), 0x3A9D8: ("edx",), 0x3AA54: (),
         0x3AAFC: ("ebx", "ecx", "edx", "edi", "ebp"), 0x3AD98: ("edx",), 0x3AE9C: ("edx",),
         0x3B080: ("ebx", "ecx", "edx"), 0x3B134: ("ebx", "edx", "edi", "ebp"), 0x3B298: ("edx",), 0x3B6C4: (),
         0x3B714: ("edx",), 0x3B8D8: ("edx",), 0x3B90C: ("edx",), 0x3BDB0: (), 0x3BDDC: ("ebp",), 0x3C148: (),
         0x3C16C: (), 0x3C190: ("edx",), 0x3C208: ("edx",), 0x3C358: (), 0x3C480: ("edx",), 0x3C4CC: ("edx",),
-        0x3C520: ("edx",), 0x3C59C: ("edx",), 0x41310: (), 0x46460: ("edx",), 0x468D8: (), 0x48170: (),
+        0x3C520: ("edx",), 0x3C59C: ("edx",), 0x41310: (), 0x46190: (), 0x46460: ("edx",), 0x468D8: (), 0x48170: (),
         0x49444: ("edi", "ebp"), 0x4F434: (), 0x5D7DC: (), 0x5DC0F: ("ebx", "ecx", "edx"),
         0x5DC8B: ("ebx", "ecx", "edx"), 0x5DD03: (), 0x5DEAF: ("ebx", "ecx", "edx"), 0x5DEED: ()})
         for addr, declared in stubs.items():
@@ -2359,9 +2565,9 @@ class RealFunctionTests(unittest.TestCase):
             rc = V.main(["--diffrun", DIFFRUN, "--exe", EXE, "--image", os.path.join(self.tmp.name, "a.bin"),
                          "--self-check"])
         self.assertEqual(rc, 0)
-        # the closed-row count is over the rows that have callees (205), the 56 without are counted apart
-        self.assertIn("diff-verify: 261/261 functions VERIFIED; 945/945 mutants detected; 1 named gaps; "
-                      "181/205 rows with callees closed (56 have none).", out.getvalue())
+        # the closed-row count is over the rows that have callees (217), the 61 without are counted apart
+        self.assertIn("diff-verify: 278/278 functions VERIFIED; 1031/1031 mutants detected; 1 named gaps; "
+                      "196/217 rows with callees closed (61 have none).", out.getvalue())
 
 
     def test_each_c3c_mutant_is_caught_by_what_it_breaks(self):
```
