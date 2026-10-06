# C3: the frontier callee rows (track P, batch C3) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add the differential-verification row for each of the sixteen frontier callees this batch
measures (C2b record §C2b.7's 53-row frontier): each gets its own `Spec`, binding, mutants and the
one new seam it needs; verification only — no ported function, no `fn_register`, no E2 move. The
remaining 36 candidate addresses plus the two new frontier items (`0x13420`, `0x249C0`) are C3b
(record §C3.7).

**Architecture:** Each row is a `Spec` in `tools/diff_verify.py` (`C3_SPECS`), a `b_*` binding and
`m_*` mutants in `port/tests/diff_runner.c` (`k_bindings`), and exact-set assertions in
`tools/tests/test_diff_verify.py` (`C3_MASKS`, `C3_KINDS`, the case-set table, the clobber table,
the counter line). One new seam: `effect_teardown` (`0x13420`) loses `static`, gains
`PR_SEAM(0x13420u, rec)` and an `effects.h` declaration, because the `effects_clear` row stubs it.
No other `port/src` change: every other callee already carries its seam or is allowed.

**Tech Stack:** C11 (`port/src`, `port/tests`), Python 3.12 (`unittest`), `unicorn` 2.1.4 and
`capstone` 5.0.7 (`tools/requirements-diff.txt`), CMake, `make`.

**Spec:** `docs/superpowers/specs/2026-09-30-reverse-completion-design.md` §4 track P, §5.1-§5.3,
§6 ("each ported function has a verification row; unhit blocks and non-emulable instructions named;
counters and README percentage updated"), §7.

**Derivation record:** `docs/superpowers/plans/2026-10-05-reverse-c3-derivations.md` (§C3.1 the
sixteen rows checked against the raw, §C3.2 the rows, their seams and fixtures, §C3.3 the mutants
and their measured catching sets and case sets, §C3.4 the counters, §C3.5 the named gaps and
limits, §C3.6 what the planner ran, §C3.7 the C3b deferral with its evidence). Recipe:
`2026-10-01-reverse-e3-derivations.md` §E3.10; checklist:
`.superpowers/sdd/2026-10-01-plans/p-track-review-checklist.md`.

**What the planner ran (scratch, 2026-10-06, on `reverse-c3` at `main` `6da0128`; image sha1
`ff3b8cb14e00f1c282de7b7e15dcd7c230766947`, the E2/E3/P1-P8 image):** the baseline (Task 1) and a
full prototype of the sixteen rows: the patch below applied to the clean worktree, each row
measured with `--function NAME --self-check` until VERIFIED with every mutant detected, then
`make diff-verify` (`217/217 functions VERIFIED; 664/664 mutants detected; 1 named gaps; 154/172
rows with callees closed (45 have none)`), `make entry-triage` (byte-identical: `targets 233
unported, 262 ported; supplement 131 (3 unported, 0 stale); untrusted 30`; voice `0 / 115 / 19`),
`python3 -m unittest tools.tests.test_diff_verify` (105 tests OK),
`PR_ORACLE_REQUIRED=1 ./build/run_tests` (all checks passed) and
`python3 tools/port_progress.py` (`771 1203 64` / `731 731 100`). Every expected output below is
the prototype's measured output. The prototype was then reverted (`git checkout -- port tools`);
the patch is the exact diff it applied.

**Re-baseline note.** The counters below are `6da0128`'s. If `main` moves before this plan
executes, Task 1 records the measured base and every later expected counter adds this plan's
increments: functions +16, mutants +66, rows with callees +5, closed rows +6, no-callee rows +11.
The E2 table must not move (no ported function, no `fn_register`): if a task regenerates it, the
line must be byte-identical.

## Decisions needed from the user

**None.** No raw-over-port correction was needed: every row matched on the first measurement
(after the prototype's own mutant-fixture fixes, all recorded). The one new seam is on a ported
callee (`0x13420`) with a stated reason; the two new frontier items its row and `actor_alloc`'s
allow create (`0x13420`, `0x249C0`) are listed for C3b in the record §C3.7.

## The C3 roadmap

| task | what | commit |
|---|---|---|
| 1 | baseline on `6da0128` (no commit) | - |
| 2 | the sixteen rows, the `0x13420` seam, bindings, mutants and the test exact-set updates | `tools:`/`tests:` |
| 3 | the review sweep (stores, clobbers, mutant case sets) | `tests:` |
| 4 | closure: PROGRESS, the record/PROGRESS counters, the final gates | `docs:` |

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
  nothing: the table must be byte-identical.
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
  adds the C3 case-set test; no other assertion changes.
- AGENTS.md: "`data/` is git-ignored and read-only — never write to it." / "Commit style:
  `<area>: <what changed>`. **Never `git add -A`** — stage named files." No `Co-Authored-By`
  trailer. Never run `make gp-capture`.
- Spec §5.3 / E3 §E3.4: "VERIFIED" is equivalence on the exercised blocks and inputs only, each
  function with its callees stubbed or run as stated.

## Where to run

The worktree `.worktrees/reverse-c3` (branch `reverse-c3`). Every command runs from the worktree
root; `data/` and `.superpowers/` are symlinks; `build/` exists. Before Task 1:

```bash
git rev-parse HEAD   # 6da0128
python3 -c "import unicorn, capstone; print(unicorn.__version__, capstone.__version__)"  # 2.1.4 5.0.7
cmake -S port -B build && cmake --build build 2>&1 | tail -1
./build/diffrun --exe data/game/C/PRAGE.EXE --image-out /tmp/pr_c3_img.bin && shasum /tmp/pr_c3_img.bin
ls port/tests/ghidra_data.bin   # the PR_ORACLE_REQUIRED fixture (git-ignored); copy it from the main repo if absent
```

The last-but-one line must print `ff3b8cb14e00f1c282de7b7e15dcd7c230766947`; another hash means the
image differs from the one the record measured: stop. Ledger:
`.superpowers/sdd/2026-10-05-reverse-c3-frontier-rows/progress.md`.

### The sixteen rows (the brief's grouping, executed as families)

The patch is one `git diff` (Task 2 applies it at once); the families below are the gate order:

| family | rows | shared seams/fixtures |
|---|---|---|
| the splice lists | `0x249B0` `0x249D0` | none (the effects.c copies carry the row) |
| the per-side leaves | `0x164E8` `0x1D238` `0x1D244` `0x1CA14` | none (already seamed) |
| the fighter leaves | `0x2A620` `0x3C59C` `0x46460` `0x41310` `0x365C8` `0x1A5AC` `0x1881C` `0x36CE4` | the `0x33950`/`0x186D0`/`0x2BC30` call-set entries |
| the walkers | `0x13DF0` `0x2AC80` | the new `0x13420` seam; the `0x249B0`/`0x249D0` real calls and the `0x249C0`/`0x2EA30` allows |

## How the code steps are written

Task 2's patch is the prototype's exact `git diff` (the planner applied it, ran every gate, and
reverted). Apply it with `git apply`; it touches `port/src/game/effects.c`,
`port/src/game/effects.h`, `port/tests/diff_runner.c`, `tools/diff_verify.py` and
`tools/tests/test_diff_verify.py`. If `main` moved, `git apply --3way` and resolve by keeping the
patch's additions at the same anchors; never merge the E2 table by hand.

## File Structure

| File | Responsibility |
|---|---|
| `port/src/game/effects.c` | the `0x13420` `effect_teardown` seam and its `static` removal |
| `port/src/game/effects.h` | the `effect_teardown` declaration |
| `port/tests/diff_runner.c` | the sixteen bindings and 66 mutants, the `C3` cores |
| `tools/diff_verify.py` | `C3_SPECS` (the sixteen rows and their fixtures) |
| `tools/tests/test_diff_verify.py` | `C3_MASKS`, `C3_KINDS`, the case-set test, the clobber and counter lines |
| `docs/superpowers/plans/2026-10-05-reverse-c3-derivations.md` | the record (this plan's source of values) |
| `docs/PROGRESS.md` | Task 4's paragraph |

---

### Task 1: Baseline (no commit)

**Files:** none. **Interfaces:** consumes the worktree at `6da0128`; produces the baseline counters
and table.

- [ ] **Step 1: the task gates on the untouched tree.**

```bash
make diff-verify DIFF_IMAGE=/tmp/pr_c3_base.bin DIFF_TABLE=/tmp/pr_c3_base_table.md 2>&1 | tail -1
make entry-triage E2_IMAGE=/tmp/pr_c3_e2.bin 2>&1 | tail -2
python3 tools/port_progress.py
```

Expected (measured at `6da0128`):

```
diff-verify: 201/201 functions VERIFIED; 598/598 mutants detected; 1 named gaps; 148/167 rows with callees closed (34 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
entry-triage: targets 233 unported, 262 ported; supplement 131 (3 unported, 0 stale); untrusted entries 30
entry-triage: voice sites outside Ghidra 134: 0 in unported code, 115 in ported code, 19 nowhere
771 1203 64
731 731 100 (portable: excludes 81 host-owned/deferred and runtime >= 5D000)
```

- [ ] **Step 2: enumerate the frontier from the table.** `python3` over `/tmp/pr_c3_base_table.md`:
  the callees marked `unverified` (record §C3.7's 57 distinct). Expected: the 53 candidates of the
  record plus the four non-rows `0x1B544`, `0x5D812`, `0x29D60`, `0x2EA64`.

### Task 2: the sixteen rows (one commit)

**Files:** the patch below. **Interfaces:** produces `C3_SPECS`, the sixteen bindings and 66
mutants, the `0x13420` seam and the test exact-set updates; consumes the C1/C2/C2b fixtures
(`E3_SLOT`, `E3_REC`, `ANIM_BEGIN`, `le32`).

- [ ] **Step 1: apply the patch.** `git apply - <<'PATCH' ... PATCH` with the diff embedded at the
  end of this task (the exact prototype diff; 1170 insertions over 5 files).

- [ ] **Step 2: build and run each family's self-check.**

```bash
cmake --build build 2>&1 | tail -1
for f in list_insert_after list_unlink fighter_164e8 snd_music_unpause snd_sample_unpause \
         snd_music_request mode1_cursor fighter_pass_flag fighter_input_read fighter_41310 \
         fighter_state_365c8 fighter_1a5ac effects_clear hit_vert_distance fighter_36ce4 actor_alloc; do
  python3 tools/diff_verify.py --function "$f" --self-check
done
```

Expected rows (measured; the format is row, entry, cases, blocks, mutants, named unhit):

| row | entry | cases | blocks | mutants | named unhit |
|---|---|---|---|---|---|
| `list_insert_after` | 0x249B0 | 3 | 1/1 | 4/4 | — |
| `list_unlink` | 0x249D0 | 3 | 1/1 | 4/4 | — |
| `fighter_164e8` | 0x164E8 | 3 | 1/1 | 3/3 | — |
| `snd_music_unpause` | 0x1D238 | 3 | 1/1 | 3/3 | — |
| `snd_sample_unpause` | 0x1D244 | 3 | 1/1 | 2/2 | — |
| `snd_music_request` | 0x1CA14 | 4 | 4/4 | 5/5 | — |
| `mode1_cursor` | 0x2A620 | 7 | 7/8 | 5/5 | 0x2A66D (dead) |
| `fighter_pass_flag` | 0x3C59C | 7 | 3/3 | 4/4 | — |
| `fighter_input_read` | 0x46460 | 6 | 5/5 | 4/4 | — |
| `fighter_41310` | 0x41310 | 7 | 6/6 | 5/5 | — |
| `fighter_state_365c8` | 0x365C8 | 13 | 11/11 | 4/4 | — |
| `fighter_1a5ac` | 0x1A5AC | 6 | 1/1 | 3/3 | — |
| `effects_clear` | 0x13DF0 | 3 | 3/3 | 5/5 | — |
| `hit_vert_distance` | 0x1881C | 4 | 1/1 | 4/4 | — |
| `fighter_36ce4` | 0x36CE4 | 4 | 4/4 | 6/6 | — |
| `actor_alloc` | 0x2AC80 | 5 | 9/9 | 5/5 | — |

- [ ] **Step 3: the task gates.**

```bash
make diff-verify DIFF_IMAGE=/tmp/pr_c3_after.bin DIFF_TABLE=/tmp/pr_c3_after_table.md 2>&1 | tail -1
python3 -m unittest tools.tests.test_diff_verify 2>&1 | tail -3
make entry-triage E2_IMAGE=/tmp/pr_c3_after_e2.bin 2>&1 | tail -2
PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -1
```

Expected: `217/217 functions VERIFIED; 664/664 mutants detected; 1 named gaps; 154/172 rows with
callees closed (45 have none)`; `OK` (105 tests); E2 byte-identical; `all checks passed`.

- [ ] **Step 4: commit.**

```bash
git add port/src/game/effects.c port/src/game/effects.h port/tests/diff_runner.c \
        tools/diff_verify.py tools/tests/test_diff_verify.py
git commit -m "tools: C3: the sixteen frontier callee rows, their seam and mutants"
```

### Task 3: the review sweep (one commit)

**Files:** the patch's rows. **Interfaces:** consumes Task 2's state; produces the store sweep's
findings.

- [ ] **Step 1: the store sweep.** For every C3 row, poke each field the row writes to a value that
  differs from what it writes and re-run `--function NAME --self-check`; a store with no sentinel
  fails some mutant. The prototype's sweep was done per row during measurement (record §C3.6);
  re-check the 11 fields the record lists as the rows' only writes (`DS_001077E0`/`0x107874` for
  `hit_vert_distance`, the pause bytes, the latched camera field, the `+0x3C` camera field, the
  slot `+0x43` bit, `rec+0x64`, the `0x107D50` dword, `rec+0x28`'s bit 0x4000, the free-list
  links, the lock/count bytes).
- [ ] **Step 2: the clobber and case-set re-check.** `python3 -m unittest
  tools.tests.test_diff_verify.RealFunctionTests` (the clobber re-derivation and the C3 case-set
  test). Expected: OK.
- [ ] **Step 3: commit any fix.**

```bash
git add <the fixed files>
git commit -m "tests: C3 review sweep: the store sentinels and the case-set pins"
```

### Task 4: Closure (one commit)

**Files:** `docs/PROGRESS.md`, the record. **Interfaces:** consumes Task 2's measured state.

- [ ] **Step 1: the final gates.**

```bash
make diff-verify DIFF_IMAGE=/tmp/pr_c3_close.bin 2>&1 | tail -1
make entry-triage E2_IMAGE=/tmp/pr_c3_close_e2.bin 2>&1 | tail -2
PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -1
python3 tools/port_progress.py
make verify
```

Expected: the Task 2 counter; E2 byte-identical; `all checks passed`; `771 1203 64` / `731 731
100`; and the full `make verify` exit 0 with the 45 oracle lines equal to
`.superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt` and the `make audio-render` WAV
equal to `before-t2.wav` (the only `port/src` changes are a seam and an export; the full gate is
what proves no oracle-visible path moved).

- [ ] **Step 2: append the PROGRESS paragraph** (the sixteen rows, the counter, the new frontier
  items `0x13420`/`0x249C0`, the C3b deferral of record §C3.7).

- [ ] **Step 3: commit.**

```bash
git add docs/PROGRESS.md docs/superpowers/plans/2026-10-05-reverse-c3-derivations.md
git commit -m "docs: C3 closure: the sixteen frontier rows measured and the C3b deferral"
```

### The patch

```diff
diff --git a/port/src/game/effects.c b/port/src/game/effects.c
index bf6438a..0d62204 100644
--- a/port/src/game/effects.c
+++ b/port/src/game/effects.c
@@ -63,8 +63,9 @@ void effects_list_unlink(u32 rec)
  * record's own fields through 0x33714 (flag 1, palette_record_flagged), type 1
  * the record's +0x10 block through 0x33734, type 4 the raw 0xFCCF0 buffer
  * (jump table 0x13408). */
-static void effect_teardown(u32 rec)
+void effect_teardown(u32 rec)
 {
+    PR_SEAM(0x13420u, rec);
     u8 saved = DSB(DS_0009AF3C);
     DSB(DS_0009AF3C) = 1;
     effects_list_unlink(rec);
diff --git a/port/src/game/effects.h b/port/src/game/effects.h
index b277c38..16d2ec8 100644
--- a/port/src/game/effects.h
+++ b/port/src/game/effects.h
@@ -79,4 +79,8 @@ int effects_active(void);
  * dispatcher) is ported in camera.c. */
 void camera_shake_decay(void);
 
+/* 0x13420 (record C3 §C3.4): unlinks `rec` and runs its type dispatch. Exposed
+ * for the 0x13DF0 row's mutants; its own row is C3b (the 13DF0 row stubs it). */
+void effect_teardown(u32 rec);
+
 #endif /* PRAGE_GAME_EFFECTS_H */
diff --git a/port/tests/diff_runner.c b/port/tests/diff_runner.c
index 776e7de..6664857 100644
--- a/port/tests/diff_runner.c
+++ b/port/tests/diff_runner.c
@@ -8716,6 +8716,596 @@ static void m_2b2a0_indirect(const u32 *r, u32 *eax) { *eax = c2b_op_core(r, C2B
 static void m_2b2a0_child(const u32 *r, u32 *eax) { *eax = c2b_op_core(r, C2BOP_CHILD); }
 static void m_2b2a0_skip(const u32 *r, u32 *eax) { *eax = c2b_op_core(r, C2BOP_SKIP); }
 
+/* Track P batch C3 (record 2026-10-05-reverse-c3): the frontier callee rows. */
+static void b_249b0(const u32 *r, u32 *eax)            { effects_list_insert_after(r[R_EAX], r[R_EDX]); *eax = 0u; }
+/* `next`'s back link points at `at`, not `rec`. */
+static void m_249b0_next(const u32 *r, u32 *eax)
+{
+    u32 at = r[R_EAX], rec = r[R_EDX];
+    u32 next = DSD(at);
+    DSD(at) = rec; DSD(rec) = next; DSD(rec + 4u) = at; DSD(next + 4u) = at;
+    *eax = 0u;
+}
+/* the back link of `next` is not written. */
+static void m_249b0_skip(const u32 *r, u32 *eax)
+{
+    u32 at = r[R_EAX], rec = r[R_EDX];
+    u32 next = DSD(at);
+    DSD(at) = rec; DSD(rec) = next; DSD(rec + 4u) = at;
+    *eax = 0u;
+}
+/* `rec`'s back link points at `next`, not `at`. */
+static void m_249b0_back(const u32 *r, u32 *eax)
+{
+    u32 at = r[R_EAX], rec = r[R_EDX];
+    u32 next = DSD(at);
+    DSD(at) = rec; DSD(rec) = next; DSD(rec + 4u) = next; DSD(next + 4u) = rec;
+    *eax = 0u;
+}
+/* the head link is left alone: nothing is inserted. */
+static void m_249b0_head(const u32 *r, u32 *eax)
+{
+    u32 at = r[R_EAX], rec = r[R_EDX];
+    u32 next = DSD(at);
+    DSD(rec) = next; DSD(rec + 4u) = at; DSD(next + 4u) = rec;
+    *eax = 0u;
+}
+static void b_249d0(const u32 *r, u32 *eax)            { effects_list_unlink(r[R_EAX]); *eax = 0u; }
+/* `prev` is read from rec's forward link, not its back link. */
+static void m_249d0_prev(const u32 *r, u32 *eax)
+{
+    u32 rec = r[R_EAX];
+    u32 next = DSD(rec);
+    u32 prev = DSD(rec);
+    DSD(next + 4u) = prev; DSD(prev) = next; DSD(rec + 4u) = 0u; DSD(rec) = 0u;
+    *eax = 0u;
+}
+/* the neighbour splice is skipped: only rec is zeroed. */
+static void m_249d0_link(const u32 *r, u32 *eax)
+{
+    u32 rec = r[R_EAX];
+    DSD(rec + 4u) = 0u; DSD(rec) = 0u;
+    *eax = 0u;
+}
+/* `rec`'s forward link is left set. */
+static void m_249d0_one(const u32 *r, u32 *eax)
+{
+    u32 rec = r[R_EAX];
+    u32 next = DSD(rec), prev = DSD(rec + 4u);
+    DSD(next + 4u) = prev; DSD(prev) = next; DSD(rec + 4u) = 0u;
+    *eax = 0u;
+}
+/* the neighbour splice swaps forward and back. */
+static void m_249d0_swap(const u32 *r, u32 *eax)
+{
+    u32 rec = r[R_EAX];
+    u32 next = DSD(rec), prev = DSD(rec + 4u);
+    DSD(next + 4u) = next; DSD(prev) = prev; DSD(rec + 4u) = 0u; DSD(rec) = 0u;
+    *eax = 0u;
+}
+static void b_164e8(const u32 *r, u32 *eax)            { fighter_164e8(r[R_EAX]); *eax = 0u; }
+/* the per-side offset is dropped. */
+static void m_164e8_noside(const u32 *r, u32 *eax)     { DSD(DS_000FD148) = 0u; *eax = 0u; }
+/* only the low byte is cleared. */
+static void m_164e8_byte(const u32 *r, u32 *eax)       { DSB(DS_000FD148 + r[R_EAX] * 4u) = 0u; *eax = 0u; }
+/* the dword is set to the side, not zero. */
+static void m_164e8_side(const u32 *r, u32 *eax)       { DSD(DS_000FD148 + r[R_EAX] * 4u) = r[R_EAX]; *eax = 0u; }
+static void b_1d238(const u32 *r, u32 *eax)            { (void)r; snd_music_unpause(); *eax = 0u; }
+/* the sample pause byte is cleared instead. */
+static void m_1d238_db(const u32 *r, u32 *eax)         { (void)r; DSB(0x001028DB) = 0; *eax = 0u; }
+/* the byte is set, not cleared. */
+static void m_1d238_one(const u32 *r, u32 *eax)        { (void)r; DSB(0x001028DA) = 1; *eax = 0u; }
+/* a dword store clears both pause bytes. */
+static void m_1d238_word(const u32 *r, u32 *eax)       { (void)r; DSD(0x001028D8) = 0u; *eax = 0u; }
+static void b_1d244(const u32 *r, u32 *eax)            { (void)r; snd_sample_unpause(); *eax = 0u; }
+/* the music pause byte is cleared instead. */
+static void m_1d244_da(const u32 *r, u32 *eax)         { (void)r; DSB(0x001028DA) = 0; *eax = 0u; }
+/* the byte is set, not cleared. */
+static void m_1d244_one(const u32 *r, u32 *eax)        { (void)r; DSB(0x001028DB) = 1; *eax = 0u; }
+static void b_1ca14(const u32 *r, u32 *eax)            { *eax = snd_music_request(r[R_EAX], r[R_EDX]); }
+/* b (DL) is not stored in DS_001028D9. */
+static void m_1ca14_d9(const u32 *r, u32 *eax)
+{
+    DSD(DS_001028D4) = r[R_EAX];
+    if (DSB(DS_001028DA) == 1u) { *eax = 0u; return; }
+    if (DSD(DS_001028C0) == 0u) { *eax = 0u; return; }
+    DSD(DS_001028CC) = r[R_EAX];
+    *eax = 1u;
+}
+/* the pause byte is not tested: a paused song becomes pending. */
+static void m_1ca14_pause(const u32 *r, u32 *eax)
+{
+    DSB(DS_001028D9) = (u8)r[R_EDX];
+    DSD(DS_001028D4) = r[R_EAX];
+    if (DSD(DS_001028C0) == 0u) { *eax = 0u; return; }
+    DSD(DS_001028CC) = r[R_EAX];
+    *eax = 1u;
+}
+/* the sequence handle is not tested: the pending song is stored anyway. */
+static void m_1ca14_seq(const u32 *r, u32 *eax)
+{
+    DSB(DS_001028D9) = (u8)r[R_EDX];
+    DSD(DS_001028D4) = r[R_EAX];
+    if (DSB(DS_001028DA) == 1u) { *eax = 0u; return; }
+    DSD(DS_001028CC) = r[R_EAX];
+    *eax = 1u;
+}
+/* the pending song is never stored. */
+static void m_1ca14_cc(const u32 *r, u32 *eax)
+{
+    DSB(DS_001028D9) = (u8)r[R_EDX];
+    DSD(DS_001028D4) = r[R_EAX];
+    if (DSB(DS_001028DA) == 1u) { *eax = 0u; return; }
+    if (DSD(DS_001028C0) == 0u) { *eax = 0u; return; }
+    *eax = 1u;
+}
+/* AL is 1 on every path. */
+static void m_1ca14_al(const u32 *r, u32 *eax)
+{
+    DSB(DS_001028D9) = (u8)r[R_EDX];
+    DSD(DS_001028D4) = r[R_EAX];
+    if (DSB(DS_001028DA) != 1u && DSD(DS_001028C0) != 0u) DSD(DS_001028CC) = r[R_EAX];
+    *eax = 1u;
+}
+static void b_2a620(const u32 *r, u32 *eax)            { mode1_cursor(r[R_EAX], r[R_EDX]); *eax = 0u; }
+/* rec+0x1C is not tested: always the previous-x path. */
+static void m_2a620_pset(const u32 *r, u32 *eax)
+{
+    u32 rec = r[R_EAX], pset = r[R_EDX];
+    s32 v = (s32)DSD(pset + 0x14u) >> 6;
+    if ((u16)v < DSW(DS_00107A4C)) { DSB(rec + 0x64u) = 0xff; return; }
+    DSB(rec + 0x64u) = (u8)((u8)v - (u8)DSW(DS_00107A4C));
+    if (((s32)DSD(rec + 0x61u) >> 24) >= 0x80) DSB(rec + 0x64u) = 0x7f;
+    *eax = 0u;
+}
+/* always the current-y path. */
+static void m_2a620_y(const u32 *r, u32 *eax)
+{
+    u32 rec = r[R_EAX], pset = r[R_EDX];
+    s32 v = (s32)DSD(DS_000F0AEC) + 0x3bc0 - ((s32)DSD(rec + 0x30u) >> 16);
+    v >>= 6;
+    if ((u16)v < DSW(DS_00107A4C)) { DSB(rec + 0x64u) = 0xff; return; }
+    DSB(rec + 0x64u) = (u8)((u8)v - (u8)DSW(DS_00107A4C));
+    if (((s32)DSD(rec + 0x61u) >> 24) >= 0x80) DSB(rec + 0x64u) = 0x7f;
+    *eax = 0u;
+}
+/* the threshold compare is <=, so v == the threshold clamps. */
+static void m_2a620_cmp(const u32 *r, u32 *eax)
+{
+    u32 rec = r[R_EAX], pset = r[R_EDX];
+    s32 v = DSD(rec + 0x1cu) == 0 ? (s32)DSD(pset + 0x14u)
+                                  : (s32)DSD(DS_000F0AEC) + 0x3bc0 - ((s32)DSD(rec + 0x30u) >> 16);
+    v >>= 6;
+    if ((u16)v <= DSW(DS_00107A4C)) { DSB(rec + 0x64u) = 0xff; return; }
+    DSB(rec + 0x64u) = (u8)((u8)v - (u8)DSW(DS_00107A4C));
+    if (((s32)DSD(rec + 0x61u) >> 24) >= 0x80) DSB(rec + 0x64u) = 0x7f;
+    *eax = 0u;
+}
+/* the threshold is not subtracted. */
+static void m_2a620_sub(const u32 *r, u32 *eax)
+{
+    u32 rec = r[R_EAX], pset = r[R_EDX];
+    s32 v = DSD(rec + 0x1cu) == 0 ? (s32)DSD(pset + 0x14u)
+                                  : (s32)DSD(DS_000F0AEC) + 0x3bc0 - ((s32)DSD(rec + 0x30u) >> 16);
+    v >>= 6;
+    if ((u16)v < DSW(DS_00107A4C)) { DSB(rec + 0x64u) = 0xff; return; }
+    DSB(rec + 0x64u) = (u8)v;
+    if (((s32)DSD(rec + 0x61u) >> 24) >= 0x80) DSB(rec + 0x64u) = 0x7f;
+    *eax = 0u;
+}
+/* the y path's shift is 5, not 6. */
+static void m_2a620_shl(const u32 *r, u32 *eax)
+{
+    u32 rec = r[R_EAX], pset = r[R_EDX];
+    s32 v = DSD(rec + 0x1cu) == 0 ? (s32)DSD(pset + 0x14u)
+                                  : (s32)DSD(DS_000F0AEC) + 0x3bc0 - ((s32)DSD(rec + 0x30u) >> 16);
+    v >>= (DSD(rec + 0x1cu) == 0 ? 6 : 5);
+    if ((u16)v < DSW(DS_00107A4C)) { DSB(rec + 0x64u) = 0xff; return; }
+    DSB(rec + 0x64u) = (u8)((u8)v - (u8)DSW(DS_00107A4C));
+    if (((s32)DSD(rec + 0x61u) >> 24) >= 0x80) DSB(rec + 0x64u) = 0x7f;
+    *eax = 0u;
+}
+static void b_3c59c(const u32 *r, u32 *eax)            { *eax = (u32)fighter_pass_flag(r[R_EAX], r[R_EDX]); }
+/* the membership test is ==, not a bit mask. */
+static void m_3c59c_eq(const u32 *r, u32 *eax)
+{
+    u32 bit = r[R_EAX] & 0x1Fu, side = r[R_EDX];
+    u32 m = 1u << bit;
+    if (DSD(DS_00107D50 + side * 4u) == m) { *eax = 1u; return; }
+    DSD(DS_00107D50 + side * 4u) |= m;
+    *eax = 0u;
+}
+/* the bit is never set. */
+static void m_3c59c_set(const u32 *r, u32 *eax)
+{
+    u32 bit = r[R_EAX] & 0x1Fu, side = r[R_EDX];
+    u32 m = 1u << bit;
+    *eax = (DSD(DS_00107D50 + side * 4u) & m) != 0u ? 1u : 0u;
+}
+/* the side's stride is 1, not 4. */
+static void m_3c59c_side(const u32 *r, u32 *eax)
+{
+    u32 bit = r[R_EAX] & 0x1Fu, side = r[R_EDX];
+    u32 m = 1u << bit;
+    if ((DSD(DS_00107D50 + side) & m) != 0u) { *eax = 1u; return; }
+    DSD(DS_00107D50 + side) |= m;
+    *eax = 0u;
+}
+/* the shift count is not masked to 5 bits. */
+static void m_3c59c_shift(const u32 *r, u32 *eax)
+{
+    u32 bit = r[R_EAX], side = r[R_EDX];
+    u32 m = bit < 32u ? 1u << bit : 0u;
+    if ((DSD(DS_00107D50 + side * 4u) & m) != 0u) { *eax = 1u; return; }
+    DSD(DS_00107D50 + side * 4u) |= m;
+    *eax = 0u;
+}
+static void b_46460(const u32 *r, u32 *eax)            { *eax = fighter_input_read(r[R_EAX], (s32)r[R_EDX]); }
+/* the ring side stride is 0x20, not 0x28. */
+static void m_46460_side(const u32 *r, u32 *eax)
+{
+    s32 pos = (s32)DSD(0x001082D2) >> 16;
+    s32 index = (s32)r[R_EDX];
+    for (s32 n = index; n > 0; n--) if (--pos < 0) pos = 0x13;
+    *eax = DSW(DS_00108270 + r[R_EAX] * 0x20u + (u32)pos * 2u);
+}
+/* the position does not wrap below zero. */
+static void m_46460_wrap(const u32 *r, u32 *eax)
+{
+    s32 pos = (s32)DSD(0x001082D2) >> 16;
+    s32 index = (s32)r[R_EDX];
+    for (s32 n = index; n > 0; n--) pos--;
+    *eax = DSW(DS_00108270 + r[R_EAX] * 0x28u + (u32)pos * 2u) & 0xFFFFu;
+}
+/* the loop runs for index 0 too (>= instead of >). */
+static void m_46460_sign(const u32 *r, u32 *eax)
+{
+    s32 pos = (s32)DSD(0x001082D2) >> 16;
+    s32 index = (s32)r[R_EDX];
+    for (s32 n = index; n >= 0; n--) if (--pos < 0) pos = 0x13;
+    *eax = DSW(DS_00108270 + r[R_EAX] * 0x28u + (u32)pos * 2u);
+}
+/* the ring position is the whole dword, not its high word. */
+static void m_46460_pos(const u32 *r, u32 *eax)
+{
+    s32 pos = (s32)DSD(0x001082D2);
+    s32 index = (s32)r[R_EDX];
+    for (s32 n = index; n > 0; n--) if (--pos < 0) pos = 0x13;
+    *eax = DSW(DS_00108270 + r[R_EAX] * 0x28u + (u32)pos * 2u) & 0xFFFFu;
+}
+static void b_41310(const u32 *r, u32 *eax)            { fighter_41310(r[R_EAX], (s32)r[R_EDX]); *eax = 0u; }
+/* mode 3 is not excluded. */
+static void m_41310_mode(const u32 *r, u32 *eax)
+{
+    u32 rec = DSD(DS_001077A8 + r[R_EAX] * 4u);
+    s32 v = (s32)DSD(rec + 0x3Cu) + (s32)r[R_EDX];
+    if ((s32)r[R_EDX] < 0 && v < 1) { DSD(rec + 0x3Cu) = 0u; *eax = 0u; return; }
+    DSD(rec + 0x3Cu) = (u32)v;
+    *eax = 0u;
+}
+/* the underflow clamp is dropped. */
+static void m_41310_clamp(const u32 *r, u32 *eax)
+{
+    if (DSW(DS_00104B00) == 3u) { *eax = 0u; return; }
+    u32 rec = DSD(DS_001077A8 + r[R_EAX] * 4u);
+    DSD(rec + 0x3Cu) = (u32)((s32)DSD(rec + 0x3Cu) + (s32)r[R_EDX]);
+    *eax = 0u;
+}
+/* the clamp also fires when the sum is exactly 1. */
+static void m_41310_eq(const u32 *r, u32 *eax)
+{
+    if (DSW(DS_00104B00) == 3u) { *eax = 0u; return; }
+    u32 rec = DSD(DS_001077A8 + r[R_EAX] * 4u);
+    s32 v = (s32)DSD(rec + 0x3Cu) + (s32)r[R_EDX];
+    if ((s32)r[R_EDX] < 0 && v < 2) { DSD(rec + 0x3Cu) = 0u; *eax = 0u; return; }
+    DSD(rec + 0x3Cu) = (u32)v;
+    *eax = 0u;
+}
+/* delta is stored, not the sum. */
+static void m_41310_add(const u32 *r, u32 *eax)
+{
+    if (DSW(DS_00104B00) == 3u) { *eax = 0u; return; }
+    u32 rec = DSD(DS_001077A8 + r[R_EAX] * 4u);
+    DSD(rec + 0x3Cu) = r[R_EDX];
+    *eax = 0u;
+}
+/* the side is ignored: always side 0's record. */
+static void m_41310_side(const u32 *r, u32 *eax)
+{
+    if (DSW(DS_00104B00) == 3u) { *eax = 0u; return; }
+    u32 rec = DSD(DS_001077A8);
+    s32 v = (s32)DSD(rec + 0x3Cu) + (s32)r[R_EDX];
+    if ((s32)r[R_EDX] < 0 && v < 1) { DSD(rec + 0x3Cu) = 0u; *eax = 0u; return; }
+    DSD(rec + 0x3Cu) = (u32)v;
+    *eax = 0u;
+}
+static void b_365c8(const u32 *r, u32 *eax)            { *eax = (u32)fighter_state_365c8(r[R_EAX], r[R_EDX], r[R_EBX]); }
+/* the rec bit is tested in the low byte. */
+static void m_365c8_bit(const u32 *r, u32 *eax)
+{
+    u32 slot = r[R_EAX], rec = r[R_EDX], side = r[R_EBX];
+    u32 other;
+    if ((DSB(slot + 0x42u) & 0x10u) != 0u) { *eax = 0u; return; }
+    other = DSD(DS_001077A8 + (side ^ 1u) * 4u);
+    if (other == 0u) { *eax = 0u; return; }
+    if ((DSB(slot + 0x42u) & 0x08u) != 0u) { *eax = 0u; return; }
+    if ((DSB(other + 0x43u) & 0x80u) == 0u) { *eax = 0u; return; }
+    if ((DSB(other + 0x42u) & 0x08u) != 0u) { *eax = 0u; return; }
+    if ((DSW(rec + 0x28u) & 0x40u) == 0u) {
+        if ((s32)DSD(slot + 0x2Cu) <= (s32)DSD(other + 0x2Cu)) { *eax = 1u; return; }
+    } else if ((s32)DSD(other + 0x2Cu) < (s32)DSD(slot + 0x2Cu)) { *eax = 1u; return; }
+    *eax = 0u;
+}
+/* the other slot is looked up by `side`, not `side ^ 1`. */
+static void m_365c8_other(const u32 *r, u32 *eax)
+{
+    u32 slot = r[R_EAX], rec = r[R_EDX], side = r[R_EBX];
+    u32 other;
+    if ((DSB(slot + 0x42u) & 0x10u) != 0u) { *eax = 0u; return; }
+    other = DSD(DS_001077A8 + side * 4u);
+    if (other == 0u) { *eax = 0u; return; }
+    if ((DSB(slot + 0x42u) & 0x08u) != 0u) { *eax = 0u; return; }
+    if ((DSB(other + 0x43u) & 0x80u) == 0u) { *eax = 0u; return; }
+    if ((DSB(other + 0x42u) & 0x08u) != 0u) { *eax = 0u; return; }
+    if ((DSW(rec + 0x28u) >> 8 & 0x40u) == 0u) {
+        if ((s32)DSD(slot + 0x2Cu) <= (s32)DSD(other + 0x2Cu)) { *eax = 1u; return; }
+    } else if ((s32)DSD(other + 0x2Cu) < (s32)DSD(slot + 0x2Cu)) { *eax = 1u; return; }
+    *eax = 0u;
+}
+/* the +0x2C compares are unsigned. */
+static void m_365c8_signed(const u32 *r, u32 *eax)
+{
+    u32 slot = r[R_EAX], rec = r[R_EDX], side = r[R_EBX];
+    u32 other;
+    if ((DSB(slot + 0x42u) & 0x10u) != 0u) { *eax = 0u; return; }
+    other = DSD(DS_001077A8 + (side ^ 1u) * 4u);
+    if (other == 0u) { *eax = 0u; return; }
+    if ((DSB(slot + 0x42u) & 0x08u) != 0u) { *eax = 0u; return; }
+    if ((DSB(other + 0x43u) & 0x80u) == 0u) { *eax = 0u; return; }
+    if ((DSB(other + 0x42u) & 0x08u) != 0u) { *eax = 0u; return; }
+    if ((DSW(rec + 0x28u) >> 8 & 0x40u) == 0u) {
+        if (DSD(slot + 0x2Cu) <= DSD(other + 0x2Cu)) { *eax = 1u; return; }
+    } else if (DSD(other + 0x2Cu) < DSD(slot + 0x2Cu)) { *eax = 1u; return; }
+    *eax = 0u;
+}
+/* the other's facing byte is tested at +0x42. */
+static void m_365c8_f43(const u32 *r, u32 *eax)
+{
+    u32 slot = r[R_EAX], rec = r[R_EDX], side = r[R_EBX];
+    u32 other;
+    if ((DSB(slot + 0x42u) & 0x10u) != 0u) { *eax = 0u; return; }
+    other = DSD(DS_001077A8 + (side ^ 1u) * 4u);
+    if (other == 0u) { *eax = 0u; return; }
+    if ((DSB(slot + 0x42u) & 0x08u) != 0u) { *eax = 0u; return; }
+    if ((DSB(other + 0x42u) & 0x80u) == 0u) { *eax = 0u; return; }
+    if ((DSB(other + 0x42u) & 0x08u) != 0u) { *eax = 0u; return; }
+    if ((DSW(rec + 0x28u) >> 8 & 0x40u) == 0u) {
+        if ((s32)DSD(slot + 0x2Cu) <= (s32)DSD(other + 0x2Cu)) { *eax = 1u; return; }
+    } else if ((s32)DSD(other + 0x2Cu) < (s32)DSD(slot + 0x2Cu)) { *eax = 1u; return; }
+    *eax = 0u;
+}
+static void b_1a5ac(const u32 *r, u32 *eax)            { *eax = (u32)fighter_1a5ac(r[R_EAX]); }
+/* ctx[5] is read instead of ctx[4]. */
+static void m_1a5ac_slot(const u32 *r, u32 *eax)
+{
+    u32 ctx[6];
+    fighter_ctx_same(ctx, r[R_EAX]);
+    *eax = (DSW(ctx[5] + 0x28u) & 0x4000u) == 0u ? 1u : 0u;
+}
+/* the record type is the same side's, not the other's. */
+static void m_1a5ac_side(const u32 *r, u32 *eax)
+{
+    u32 ctx[6];
+    fighter_ctx_same(ctx, r[R_EAX] ^ 1u);
+    *eax = (DSW(ctx[4] + 0x28u) & 0x4000u) == 0u ? 1u : 0u;
+}
+/* the bit tested is 0x0040 in the low word. */
+static void m_1a5ac_bit(const u32 *r, u32 *eax)
+{
+    u32 ctx[6];
+    fighter_ctx_same(ctx, r[R_EAX]);
+    *eax = (DSW(ctx[4] + 0x28u) & 0x0040u) == 0u ? 1u : 0u;
+}
+static void b_13df0(const u32 *r, u32 *eax)            { (void)r; effects_clear(); *eax = 0u; }
+/* the active-list walk is skipped. */
+static void m_13df0_walk(const u32 *r, u32 *eax)
+{
+    (void)r;
+    if (DSD(DS_000FCCE0) == 0) { *eax = 0u; return; }
+    DSB(DS_0009AF3C) = 1;
+    DSB(DS_0009AF3D) = 0;
+    DSB(DS_0009AF3C) = 0;
+    *eax = 0u;
+}
+/* the active counter is not cleared. */
+static void m_13df0_count(const u32 *r, u32 *eax)
+{
+    (void)r;
+    if (DSD(DS_000FCCE0) == 0) { *eax = 0u; return; }
+    DSB(DS_0009AF3C) = 1;
+    u32 node = DSD(DS_000FCCE0);
+    while (node != DS_000FCCE0) { u32 next = DSD(node); effect_teardown(node); node = next; }
+    DSB(DS_0009AF3C) = 0;
+    *eax = 0u;
+}
+/* the lock byte is not restored to zero. */
+static void m_13df0_lock(const u32 *r, u32 *eax)
+{
+    (void)r;
+    if (DSD(DS_000FCCE0) == 0) { *eax = 0u; return; }
+    DSB(DS_0009AF3C) = 1;
+    u32 node = DSD(DS_000FCCE0);
+    while (node != DS_000FCCE0) { u32 next = DSD(node); effect_teardown(node); node = next; }
+    DSB(DS_0009AF3D) = 0;
+    *eax = 0u;
+}
+/* the lock byte is not set for the walk. */
+static void m_13df0_set(const u32 *r, u32 *eax)
+{
+    (void)r;
+    if (DSD(DS_000FCCE0) == 0) { *eax = 0u; return; }
+    u32 node = DSD(DS_000FCCE0);
+    while (node != DS_000FCCE0) { u32 next = DSD(node); effect_teardown(node); node = next; }
+    DSB(DS_0009AF3D) = 0;
+    DSB(DS_0009AF3C) = 0;
+    *eax = 0u;
+}
+/* a single node ends the walk after the first unlink. */
+static void m_13df0_one(const u32 *r, u32 *eax)
+{
+    (void)r;
+    if (DSD(DS_000FCCE0) == 0) { *eax = 0u; return; }
+    DSB(DS_0009AF3C) = 1;
+    u32 node = DSD(DS_000FCCE0);
+    if (node != DS_000FCCE0) effect_teardown(node);
+    DSB(DS_0009AF3D) = 0;
+    DSB(DS_0009AF3C) = 0;
+    *eax = 0u;
+}
+static void b_1881c(const u32 *r, u32 *eax)            { (void)r; *eax = (u32)hit_vert_distance(); }
+/* only the first slot is latched. */
+static void m_1881c_one(const u32 *r, u32 *eax)
+{
+    (void)r;
+    fighter_slot_latch(0u);
+    *eax = (u32)((s32)DSD(DS_001077E0) - (s32)DSD(DS_001077E0 + 0x94u));
+}
+/* the latches run in the other order. */
+static void m_1881c_order(const u32 *r, u32 *eax)
+{
+    (void)r;
+    fighter_slot_latch(1u);
+    fighter_slot_latch(0u);
+    *eax = (u32)((s32)DSD(DS_001077E0) - (s32)DSD(DS_001077E0 + 0x94u));
+}
+/* both latches get side 1. */
+static void m_1881c_side(const u32 *r, u32 *eax)
+{
+    (void)r;
+    fighter_slot_latch(1u);
+    fighter_slot_latch(1u);
+    *eax = (u32)((s32)DSD(DS_001077E0) - (s32)DSD(DS_001077E0 + 0x94u));
+}
+/* the two latched heights are added. */
+static void m_1881c_diff(const u32 *r, u32 *eax)
+{
+    (void)r;
+    fighter_slot_latch(0u);
+    fighter_slot_latch(1u);
+    *eax = (u32)((s32)DSD(DS_001077E0) + (s32)DSD(DS_001077E0 + 0x94u));
+}
+static void b_36ce4(const u32 *r, u32 *eax)            { fighter_36ce4(r[R_EAX]); *eax = 0u; }
+/* slot+0x43 bit 2 is not set. */
+static void m_36ce4_bit(const u32 *r, u32 *eax)
+{
+    u32 slot = r[R_EAX];
+    u32 rec = DSD(slot);
+    if (DSW(DS_00104B00) == 3u || DSW(DS_00104B00) == 0x22u) { *eax = 0u; return; }
+    actors_anim_begin(DSD(DS_00102900 + (u32)DSB(rec + 0x51u) * 4u), 0x000E906Eu, 0x40400000u);
+    *eax = 0u;
+}
+/* mode 3 is not excluded. */
+static void m_36ce4_mode(const u32 *r, u32 *eax)
+{
+    u32 slot = r[R_EAX];
+    u32 rec = DSD(slot);
+    DSB(slot + 0x43u) |= 4u;
+    if (DSW(DS_00104B00) == 0x22u) { *eax = 0u; return; }
+    actors_anim_begin(DSD(DS_00102900 + (u32)DSB(rec + 0x51u) * 4u), 0x000E906Eu, 0x40400000u);
+    *eax = 0u;
+}
+/* mode 0x22 is not excluded. */
+static void m_36ce4_mode22(const u32 *r, u32 *eax)
+{
+    u32 slot = r[R_EAX];
+    u32 rec = DSD(slot);
+    DSB(slot + 0x43u) |= 4u;
+    if (DSW(DS_00104B00) == 3u) { *eax = 0u; return; }
+    actors_anim_begin(DSD(DS_00102900 + (u32)DSB(rec + 0x51u) * 4u), 0x000E906Eu, 0x40400000u);
+    *eax = 0u;
+}
+/* the per-side record index is dropped. */
+static void m_36ce4_rec(const u32 *r, u32 *eax)
+{
+    u32 slot = r[R_EAX];
+    u32 rec = DSD(slot);
+    DSB(slot + 0x43u) |= 4u;
+    if (DSW(DS_00104B00) == 3u || DSW(DS_00104B00) == 0x22u) { *eax = 0u; return; }
+    actors_anim_begin(DSD(DS_00102900), 0x000E906Eu, 0x40400000u);
+    *eax = 0u;
+}
+/* the stream constant is 0xE906A. */
+static void m_36ce4_stream(const u32 *r, u32 *eax)
+{
+    u32 slot = r[R_EAX];
+    u32 rec = DSD(slot);
+    DSB(slot + 0x43u) |= 4u;
+    if (DSW(DS_00104B00) == 3u || DSW(DS_00104B00) == 0x22u) { *eax = 0u; return; }
+    actors_anim_begin(DSD(DS_00102900 + (u32)DSB(rec + 0x51u) * 4u), 0x000E906Au, 0x40400000u);
+    *eax = 0u;
+}
+/* the side byte is read at rec+0x50. */
+static void m_36ce4_side(const u32 *r, u32 *eax)
+{
+    u32 slot = r[R_EAX];
+    u32 rec = DSD(slot);
+    DSB(slot + 0x43u) |= 4u;
+    if (DSW(DS_00104B00) == 3u || DSW(DS_00104B00) == 0x22u) { *eax = 0u; return; }
+    actors_anim_begin(DSD(DS_00102900 + (u32)DSB(rec + 0x50u) * 4u), 0x000E906Eu, 0x40400000u);
+    *eax = 0u;
+}
+
+static void b_2ac80(const u32 *r, u32 *eax)            { *eax = actor_alloc(r[R_EAX]); }
+/* the 0x400 flag is ignored: always the head insert. */
+static void m_2ac80_flag(const u32 *r, u32 *eax)
+{
+    (void)r;
+    if (DSD(0x00105B3C) == 0u || DSD(0x00105B3C) == 0x00105B3Cu) { *eax = 0u; return; }
+    u32 rec = DSD(0x00105B3C);
+    effects_list_unlink(rec);
+    effects_list_insert_after(0x00105BCC, rec);
+    *eax = rec;
+}
+/* the free list is not unlinked. */
+static void m_2ac80_unlink(const u32 *r, u32 *eax)
+{
+    if (DSD(0x00105B3C) == 0u || DSD(0x00105B3C) == 0x00105B3Cu) { *eax = 0u; return; }
+    u32 rec = DSD(0x00105B3C);
+    if (r[R_EAX] & 0x400u) effects_list_insert_before(0x00105BCC, rec);
+    else effects_list_insert_after(0x00105BCC, rec);
+    *eax = rec;
+}
+/* the 0x400 flag selects insert-after. */
+static void m_2ac80_tail(const u32 *r, u32 *eax)
+{
+    if (DSD(0x00105B3C) == 0u || DSD(0x00105B3C) == 0x00105B3Cu) { *eax = 0u; return; }
+    u32 rec = DSD(0x00105B3C);
+    effects_list_unlink(rec);
+    if (r[R_EAX] & 0x400u) effects_list_insert_after(0x00105BCC, rec);
+    else effects_list_insert_before(0x00105BCC, rec);
+    *eax = rec;
+}
+/* the empty list returns the sentinel, not 0. */
+static void m_2ac80_empty(const u32 *r, u32 *eax)
+{
+    (void)r;
+    u32 rec = DSD(0x00105B3C);
+    if (rec == 0x00105B3Cu) { *eax = rec; return; }
+    effects_list_unlink(rec);
+    effects_list_insert_after(0x00105BCC, rec);
+    *eax = rec;
+}
+/* the record is not returned. */
+static void m_2ac80_ret(const u32 *r, u32 *eax)
+{
+    if (DSD(0x00105B3C) == 0u || DSD(0x00105B3C) == 0x00105B3Cu) { *eax = 0u; return; }
+    u32 rec = DSD(0x00105B3C);
+    effects_list_unlink(rec);
+    if (r[R_EAX] & 0x400u) effects_list_insert_before(0x00105BCC, rec);
+    else effects_list_insert_after(0x00105BCC, rec);
+    *eax = 0u;
+}
+
 static const binding_t k_bindings[] = {
     { "rng_next",                 b_rng_next,     0xFFFFFFFFu },
     { "fighter_slot_flag",        b_slot_flag,    0x000000FFu },
@@ -9521,6 +10111,88 @@ static const binding_t k_bindings[] = {
     { "spawn_anim_opcode@indirect",        m_2b2a0_indirect, 0x000000FFu },
     { "spawn_anim_opcode@child",           m_2b2a0_child,  0x000000FFu },
     { "spawn_anim_opcode@skip",            m_2b2a0_skip,   0x000000FFu },
+    { "list_insert_after",                 b_249b0,        0x00000000u },
+    { "list_insert_after@next",            m_249b0_next,   0x00000000u },
+    { "list_insert_after@skip",            m_249b0_skip,   0x00000000u },
+    { "list_insert_after@back",            m_249b0_back,   0x00000000u },
+    { "list_insert_after@head",            m_249b0_head,   0x00000000u },
+    { "list_unlink",                       b_249d0,        0x00000000u },
+    { "list_unlink@prev",                  m_249d0_prev,   0x00000000u },
+    { "list_unlink@link",                  m_249d0_link,   0x00000000u },
+    { "list_unlink@one",                   m_249d0_one,    0x00000000u },
+    { "list_unlink@swap",                  m_249d0_swap,   0x00000000u },
+    { "fighter_164e8",                     b_164e8,        0x00000000u },
+    { "fighter_164e8@noside",              m_164e8_noside, 0x00000000u },
+    { "fighter_164e8@byte",                m_164e8_byte,   0x00000000u },
+    { "fighter_164e8@side",                m_164e8_side,   0x00000000u },
+    { "snd_music_unpause",                 b_1d238,        0x00000000u },
+    { "snd_music_unpause@db",              m_1d238_db,     0x00000000u },
+    { "snd_music_unpause@one",             m_1d238_one,    0x00000000u },
+    { "snd_music_unpause@word",            m_1d238_word,   0x00000000u },
+    { "snd_sample_unpause",                b_1d244,        0x00000000u },
+    { "snd_sample_unpause@da",             m_1d244_da,     0x00000000u },
+    { "snd_sample_unpause@one",            m_1d244_one,    0x00000000u },
+    { "snd_music_request",                 b_1ca14,        0x000000FFu },
+    { "snd_music_request@d9",              m_1ca14_d9,     0x000000FFu },
+    { "snd_music_request@pause",           m_1ca14_pause,  0x000000FFu },
+    { "snd_music_request@seq",             m_1ca14_seq,    0x000000FFu },
+    { "snd_music_request@cc",              m_1ca14_cc,     0x000000FFu },
+    { "snd_music_request@al",              m_1ca14_al,     0x000000FFu },
+    { "mode1_cursor",                      b_2a620,        0x00000000u },
+    { "mode1_cursor@pset",                 m_2a620_pset,   0x00000000u },
+    { "mode1_cursor@y",                    m_2a620_y,      0x00000000u },
+    { "mode1_cursor@cmp",                  m_2a620_cmp,    0x00000000u },
+    { "mode1_cursor@sub",                  m_2a620_sub,    0x00000000u },
+    { "mode1_cursor@shl",                  m_2a620_shl,    0x00000000u },
+    { "fighter_pass_flag",                 b_3c59c,        0x000000FFu },
+    { "fighter_pass_flag@eq",              m_3c59c_eq,     0x000000FFu },
+    { "fighter_pass_flag@set",             m_3c59c_set,    0x000000FFu },
+    { "fighter_pass_flag@side",            m_3c59c_side,   0x000000FFu },
+    { "fighter_pass_flag@shift",           m_3c59c_shift,  0x000000FFu },
+    { "fighter_input_read",                b_46460,        0x0000FFFFu },
+    { "fighter_input_read@side",           m_46460_side,   0x0000FFFFu },
+    { "fighter_input_read@wrap",           m_46460_wrap,   0x0000FFFFu },
+    { "fighter_input_read@sign",           m_46460_sign,   0x0000FFFFu },
+    { "fighter_input_read@pos",            m_46460_pos,    0x0000FFFFu },
+    { "fighter_41310",                     b_41310,        0x00000000u },
+    { "fighter_41310@mode",                m_41310_mode,   0x00000000u },
+    { "fighter_41310@clamp",               m_41310_clamp,  0x00000000u },
+    { "fighter_41310@eq",                  m_41310_eq,     0x00000000u },
+    { "fighter_41310@add",                 m_41310_add,    0x00000000u },
+    { "fighter_41310@side",                m_41310_side,   0x00000000u },
+    { "fighter_state_365c8",               b_365c8,        0x000000FFu },
+    { "fighter_state_365c8@bit",           m_365c8_bit,    0x000000FFu },
+    { "fighter_state_365c8@other",         m_365c8_other,  0x000000FFu },
+    { "fighter_state_365c8@signed",        m_365c8_signed, 0x000000FFu },
+    { "fighter_state_365c8@f43",           m_365c8_f43,    0x000000FFu },
+    { "fighter_1a5ac",                     b_1a5ac,        0x000000FFu },
+    { "fighter_1a5ac@slot",                m_1a5ac_slot,   0x000000FFu },
+    { "fighter_1a5ac@side",                m_1a5ac_side,   0x000000FFu },
+    { "fighter_1a5ac@bit",                 m_1a5ac_bit,    0x000000FFu },
+    { "effects_clear",                     b_13df0,        0x00000000u },
+    { "effects_clear@walk",                m_13df0_walk,   0x00000000u },
+    { "effects_clear@count",               m_13df0_count,  0x00000000u },
+    { "effects_clear@lock",                m_13df0_lock,   0x00000000u },
+    { "effects_clear@set",                 m_13df0_set,    0x00000000u },
+    { "effects_clear@one",                 m_13df0_one,    0x00000000u },
+    { "hit_vert_distance",                 b_1881c,        0xFFFFFFFFu },
+    { "hit_vert_distance@one",             m_1881c_one,    0xFFFFFFFFu },
+    { "hit_vert_distance@order",           m_1881c_order,  0xFFFFFFFFu },
+    { "hit_vert_distance@side",            m_1881c_side,   0xFFFFFFFFu },
+    { "hit_vert_distance@diff",            m_1881c_diff,   0xFFFFFFFFu },
+    { "fighter_36ce4",                     b_36ce4,        0x00000000u },
+    { "fighter_36ce4@bit",                 m_36ce4_bit,    0x00000000u },
+    { "fighter_36ce4@mode",                m_36ce4_mode,   0x00000000u },
+    { "fighter_36ce4@mode22",              m_36ce4_mode22, 0x00000000u },
+    { "fighter_36ce4@rec",                 m_36ce4_rec,    0x00000000u },
+    { "fighter_36ce4@stream",              m_36ce4_stream, 0x00000000u },
+    { "fighter_36ce4@side",                m_36ce4_side,   0x00000000u },
+    { "actor_alloc",                       b_2ac80,        0xFFFFFFFFu },
+    { "actor_alloc@flag",                  m_2ac80_flag,   0xFFFFFFFFu },
+    { "actor_alloc@unlink",                m_2ac80_unlink, 0xFFFFFFFFu },
+    { "actor_alloc@tail",                  m_2ac80_tail,   0xFFFFFFFFu },
+    { "actor_alloc@empty",                 m_2ac80_empty,  0xFFFFFFFFu },
+    { "actor_alloc@ret",                   m_2ac80_ret,    0xFFFFFFFFu },
 };
 
 static const binding_t *find_binding(const char *name)
diff --git a/tools/diff_verify.py b/tools/diff_verify.py
index 22bfa32..ca35544 100644
--- a/tools/diff_verify.py
+++ b/tools/diff_verify.py
@@ -4233,6 +4233,324 @@ C2B_SPECS = [
        eax_mask=0xFF, mutants=("@mutant", "@indirect", "@child", "@skip")),
 ]
 
+# ---- track P batch C3 (record 2026-10-05-reverse-c3): the frontier callee rows ------------------
+#
+# 0x249B0 (record C3 §C3.1): EAX = `at`, EDX = `rec`. The splice insert: next = [at]; [at] = rec;
+# [rec] = next; [rec+4] = at; [next+4] = rec. Straight-line (record §C3.1); every store observed by
+# the distinct sentinels. The port's effects.c copy carries the row (the actors.c copy is the same
+# body, named in §C3.5).
+C3_SPECS = [
+    Spec("list_insert_after", 0x249B0, [
+        # l0: distinct at/rec/next; rec's links and next's back link carry sentinels.
+        Case("l0", {"eax": 0x10A200, "edx": 0x10A240},
+             {0x10A200: le32(0x10A280), 0x10A250: le32(0x10A260),
+              0x10A240: le32(0xA5A5A5A5), 0x10A244: le32(0xA5A5A5A5),
+              0x10A284: le32(0xA5A5A5A5)}),
+        # l1: at's next is at itself (a one-element ring): the [at] and [at+4] stores land on the
+        # same word, so the last one wins; rec's two links land on it.
+        Case("l1", {"eax": 0x10A200, "edx": 0x10A240},
+             {0x10A200: le32(0x10A200), 0x10A204: le32(0xA5A5A5A5),
+              0x10A240: le32(0x11111111), 0x10A244: le32(0x22222222)}),
+        # l2: next == rec (re-inserting a detached node): [rec] is written twice.
+        Case("l2", {"eax": 0x10A200, "edx": 0x10A280},
+             {0x10A200: le32(0x10A280), 0x10A280: le32(0x33333333), 0x10A284: le32(0x44444444)}),
+    ], eax_mask=0, mutants=("@next", "@skip", "@back", "@head")),
+    # 0x249D0 (record C3 §C3.1): EAX = `rec`. next = [rec]; prev = [rec+4]; [next+4] = prev;
+    # [prev] = next; [rec+4] = 0; [rec] = 0.
+    Spec("list_unlink", 0x249D0, [
+        # u0: distinct neighbours; every neighbour field seeded differently from what is written.
+        Case("u0", {"eax": 0x10A240},
+             {0x10A240: le32(0x10A280), 0x10A244: le32(0x10A260),
+              0x10A284: le32(0xA5A5A5A5), 0x10A260: le32(0xA5A5A5A5)}),
+        # u1: a self-linked rec: next == prev == rec, so [next+4] and [prev] hit rec's own fields
+        # and are then zeroed.
+        Case("u1", {"eax": 0x10A240},
+             {0x10A240: le32(0x10A240), 0x10A244: le32(0x10A240), 0x10A248: le32(0xDEADBEEF)}),
+        # u2: next == prev (a two-element ring): [next+4] and [prev] are the same word.
+        Case("u2", {"eax": 0x10A240},
+             {0x10A240: le32(0x10A280), 0x10A244: le32(0x10A280),
+              0x10A284: le32(0x55555555)}),
+    ], eax_mask=0, mutants=("@prev", "@link", "@one", "@swap")),
+    # 0x164E8 (record C3 §C3.1): EAX = side. One dword store of 0 at 0xFD148 + side*4. The raw
+    # leaves EAX = side; every caller ignores it (mask 0).
+    Spec("fighter_164e8", 0x164E8, [
+        Case("s0", {"eax": 0}, {0x000FD148: le32(0xA5A5A5A5), 0x000FD144: le32(0x12345678)}),
+        Case("s1", {"eax": 1}, {0x000FD14C: le32(0xA5A5A5A5), 0x000FD150: le32(0x12345678)}),
+        Case("s2", {"eax": 2}, {0x000FD150: le32(0xA5A5A5A5), 0x000FD14C: le32(0x12345678)}),
+    ], eax_mask=0, mutants=("@noside", "@byte", "@side")),
+    # 0x1D238 (record C3 §C3.2): clear the music pause byte DS_001028DA. `xor ah,ah` clears AH and
+    # writes AL's high byte... no: it writes AH=0 while the store's source is AH alone (0x1028DA is
+    # one byte). Callers ignore EAX (mask 0).
+    Spec("snd_music_unpause", 0x1D238, [
+        Case("p1", {}, {0x001028DA: b"\x01", 0x001028DB: b"\xA5"}),
+        Case("p2", {}, {0x001028DA: b"\xA5", 0x001028DB: b"\x00"}),
+        Case("p3", {}, {0x001028DA: b"\x00", 0x001028D9: b"\x5A"}),
+    ], eax_mask=0, mutants=("@db", "@one", "@word")),
+    # 0x1D244 (record C3 §C3.2): clear the sample pause byte DS_001028DB.
+    Spec("snd_sample_unpause", 0x1D244, [
+        Case("p1", {}, {0x001028DB: b"\x01", 0x001028DA: b"\xA5"}),
+        Case("p2", {}, {0x001028DB: b"\xA5", 0x001028DA: b"\x00"}),
+        Case("p3", {}, {0x001028DB: b"\x00", 0x001028DC: b"\x5A"}),
+    ], eax_mask=0, mutants=("@da", "@one")),
+    # 0x1CA14 (record C3 §C3.2): EAX = song, DL = b. Store b at DS_001028D9 and song at
+    # DS_001028D4; when not paused (DS_001028DA != 1) and a sequence handle exists (DS_001028C0 !=
+    # 0), DS_001028CC = song and AL = 1, else AL = 0. Mask 0xFF (AL).
+    Spec("snd_music_request", 0x1CA14, [
+        Case("r0", {"eax": 0x2803E640, "edx": 0x12},
+             {0x001028D9: b"\xA5", 0x001028D4: le32(0), 0x001028DA: b"\x00",
+              0x001028C0: le32(0x11111111), 0x001028CC: le32(0xA5A5A5A5)}),
+        Case("r1", {"eax": 0x2803E640, "edx": 0x12},
+             {0x001028D9: b"\xA5", 0x001028D4: le32(0), 0x001028DA: b"\x01",
+              0x001028C0: le32(0x11111111), 0x001028CC: le32(0xA5A5A5A5)}),
+        Case("r2", {"eax": 0x2803E640, "edx": 0x12},
+             {0x001028D9: b"\xA5", 0x001028D4: le32(0), 0x001028DA: b"\x00",
+              0x001028C0: le32(0), 0x001028CC: le32(0xA5A5A5A5)}),
+        Case("r3", {"eax": 0, "edx": 0x1FF},
+             {0x001028D9: b"\x5A", 0x001028D4: le32(0xDEADBEEF), 0x001028DA: b"\x00",
+              0x001028C0: le32(1), 0x001028CC: le32(0xA5A5A5A5)}),
+    ], eax_mask=0xFF, mutants=("@d9", "@pause", "@seq", "@cc", "@al")),
+    # 0x2A620 (record C3 §C3.3): EAX = rec, EDX = pset. The mode-1 shear cursor. rec+0x1C == 0 ->
+    # v = pset+0x14; else v = [0xF0AEC] + 0x3BC0 - (rec+0x30 >> 16). v >>= 6 (arithmetic); the
+    # unsigned word v < [0x107A4C] -> 0xFF; else rec+0x64 = (u8)v - (u8)[0x107A4C]; rec+0x61's top
+    # byte >= 0x80 -> 0x7F.
+    Spec("mode1_cursor", 0x2A620, [
+        # y0: the pset path, v below the threshold.
+        Case("y0", {"eax": 0x10A600, "edx": 0x10A700},
+             {0x10A61C: le32(0), 0x10A700 + 0x14: le32(0x1000), 0x107A4C: b"\x50\x00",
+              0x10A600 + 0x64: b"\x5A", 0x10A661: le32(0)}),
+        # y1: v == the threshold: not below (stores 0, no clamp).
+        Case("y1", {"eax": 0x10A600, "edx": 0x10A700},
+             {0x10A61C: le32(0), 0x10A700 + 0x14: le32(0x1400), 0x107A4C: b"\x50\x00",
+              0x10A600 + 0x64: b"\x5A", 0x10A661: le32(0)}),
+        # y2: above the threshold, no clamp.
+        Case("y2", {"eax": 0x10A600, "edx": 0x10A700},
+             {0x10A61C: le32(0), 0x10A700 + 0x14: le32(0x2000), 0x107A4C: b"\x50\x00",
+              0x10A600 + 0x64: b"\x5A", 0x10A661: le32(0)}),
+        # y3: the y path and the 0x7F clamp (rec+0x61's top byte 0x80).
+        Case("y3", {"eax": 0x10A600, "edx": 0x10A700},
+             {0x10A61C: le32(1), 0x10A630: le32(0), 0x000F0AEC: le32(0), 0x107A4C: b"\x10\x00",
+              0x10A600 + 0x64: b"\x5A", 0x10A661: le32(0x80000000)}),
+        # y4: the y path, the (u8) subtraction with a threshold whose low byte is 0.
+        Case("y4", {"eax": 0x10A600, "edx": 0x10A700},
+             {0x10A61C: le32(1), 0x10A630: le32(0x10000), 0x000F0AEC: le32(0x1000),
+              0x107A4C: b"\x00\x01", 0x10A600 + 0x64: b"\x5A", 0x10A661: le32(0)}),
+        # y5: a negative v: the arithmetic shift keeps -1 and the unsigned word compare is high.
+        Case("y5", {"eax": 0x10A600, "edx": 0x10A700},
+             {0x10A61C: le32(1), 0x10A630: le32(0x3BC10000), 0x000F0AEC: le32(0),
+              0x107A4C: b"\x10\x00", 0x10A600 + 0x64: b"\x5A", 0x10A661: le32(0)}),
+        # y6: the pset path with the clamp.
+        Case("y6", {"eax": 0x10A600, "edx": 0x10A700},
+             {0x10A61C: le32(0), 0x10A700 + 0x14: le32(0x2000), 0x107A4C: b"\x50\x00",
+              0x10A600 + 0x64: b"\x5A", 0x10A661: le32(0xFF000000)}),
+    ], eax_mask=0, mutants=("@pset", "@y", "@cmp", "@sub", "@shl"),
+       unhit_named={0x2A66D: "the 0x7F clamp store is dead in both: sar edx,0x18 yields "
+                            "[-0x80,0x7F], so cmp edx,0x80 / jl at 0x2A665/0x2A66B always takes "
+                            "the jump (the port's >= 0x80 is never true; record C3 §C3.3)"}),
+    # 0x3C59C (record C3 §C3.3): EAX = bit (AL), EDX = side. Test-and-set bit (bit & 0x1F) of
+    # DSD(0x107D50 + side*4): AL = 1 when already set, else the bit is set and AL = 0. Mask 0xFF.
+    Spec("fighter_pass_flag", 0x3C59C, [
+        Case("f0", {"eax": 0, "edx": 0}, {0x00107D50: le32(0)}),
+        Case("f1", {"eax": 0, "edx": 1}, {0x00107D54: le32(1)}),
+        Case("f2", {"eax": 0x1F, "edx": 0}, {0x00107D50: le32(0)}),
+        Case("f3", {"eax": 0x20, "edx": 0}, {0x00107D50: le32(1)}),
+        Case("f4", {"eax": 3, "edx": 2}, {0x00107D58: le32(8)}),
+        Case("f5", {"eax": 5, "edx": 0}, {0x00107D50: le32(0x22)}),
+        Case("f6", {"eax": 0, "edx": 0}, {0x00107D50: le32(0x80000000)}),
+    ], eax_mask=0xFF, mutants=("@eq", "@set", "@side", "@shift")),
+    # 0x46460 (record C3 §C3.3): EAX = side, EDX = index (signed). Word of the 0x28-stride ring at
+    # 0x108270, `index` steps behind the position DSD(0x1082D2) >> 16 (wrapping modulo 0x14). Mask
+    # 0xFFFF: the raw sets AX alone, so the high half is scratch.
+    Spec("fighter_input_read", 0x46460, [
+        Case("i0", {"eax": 0, "edx": 0}, {0x001082D2: le32(5 << 16),
+             0x0010827A: b"\x05\x10", 0x00108278: b"\x04\x10"}),
+        Case("i1", {"eax": 0, "edx": 3}, {0x001082D2: le32(5 << 16),
+             0x00108274: b"\x02\x10", 0x00108272: b"\x01\x10"}),
+        Case("i2", {"eax": 0, "edx": 5}, {0x001082D2: le32(1 << 16),
+             0x00108290: b"\x10\x10", 0x00108268: b"\x77\x77"}),
+        Case("i3", {"eax": 0, "edx": 0xFFFFFFFF}, {0x001082D2: le32(4 << 16),
+             0x00108278: b"\x04\x10"}),
+        Case("i4", {"eax": 1, "edx": 1}, {0x001082D2: le32(2 << 16),
+             0x0010829A: b"\x01\x20", 0x00108292: b"\x77\x77"}),
+        Case("i5", {"eax": 0, "edx": 0x14}, {0x001082D2: le32(0),
+             0x00108270: b"\x00\x10", 0x00108296: b"\x77\x77"}),
+    ], eax_mask=0xFFFF, mutants=("@side", "@wrap", "@sign", "@pos")),
+    # 0x41310 (record C3 §C3.3): EAX = side, EDX = delta (signed). Add delta to the side's
+    # camera-target record +0x3C unless [0x104B00] == 3; a negative delta whose sum < 1 clamps the
+    # field to 0. Mask 0.
+    Spec("fighter_41310", 0x41310, [
+        Case("g0", {"eax": 0, "edx": 25},
+             {0x001077A8: le32(0x10A600), 0x001077AC: le32(0x10A680),
+              0x00104B00: b"\x03\x00", 0x10A600 + 0x3C: le32(100), 0x10A680 + 0x3C: le32(200)}),
+        Case("g1", {"eax": 0, "edx": 25},
+             {0x001077A8: le32(0x10A600), 0x001077AC: le32(0x10A680),
+              0x00104B00: b"\x00\x00", 0x10A600 + 0x3C: le32(100), 0x10A680 + 0x3C: le32(200)}),
+        Case("g2", {"eax": 0, "edx": 0},
+             {0x001077A8: le32(0x10A600), 0x001077AC: le32(0x10A680),
+              0x00104B00: b"\x00\x00", 0x10A600 + 0x3C: le32(100), 0x10A680 + 0x3C: le32(200)}),
+        Case("g3", {"eax": 0, "edx": 0xFFFFFFE7},
+             {0x001077A8: le32(0x10A600), 0x001077AC: le32(0x10A680),
+              0x00104B00: b"\x00\x00", 0x10A600 + 0x3C: le32(10), 0x10A680 + 0x3C: le32(200)}),
+        Case("g4", {"eax": 1, "edx": 0xFFFFFFF6},
+             {0x001077A8: le32(0x10A600), 0x001077AC: le32(0x10A680),
+              0x00104B00: b"\x00\x00", 0x10A600 + 0x3C: le32(100), 0x10A680 + 0x3C: le32(10)}),
+        Case("g5", {"eax": 0, "edx": 0xFFFFFFF7},
+             {0x001077A8: le32(0x10A600), 0x001077AC: le32(0x10A680),
+              0x00104B00: b"\x00\x00", 0x10A600 + 0x3C: le32(10), 0x10A680 + 0x3C: le32(200)}),
+        Case("g6", {"eax": 0, "edx": 1},
+             {0x001077A8: le32(0x10A600), 0x001077AC: le32(0x10A680),
+              0x00104B00: b"\x00\x00", 0x10A600 + 0x3C: le32(0x7FFFFFFF), 0x10A680 + 0x3C: le32(200)}),
+    ], eax_mask=0, mutants=("@mode", "@clamp", "@eq", "@add", "@side")),
+    # 0x365C8 (record C3 §C3.3): EAX = slot, EDX = rec, EBX = side. 1 when this slot is behind the
+    # other's +0x2C in the facing direction and the other slot's +0x43 bit 0x80 is set. Mask 0xFF.
+    Spec("fighter_state_365c8", 0x365C8, [
+        # s0: the slot's +0x42 bit 0x10.
+        Case("s0", {"eax": 0x10A600, "edx": 0x10A700, "ebx": 0},
+             {0x10A642: b"\x10", 0x001077A8: le32(0x10A600), 0x001077AC: le32(0x10A680)}),
+        # s1: the other slot's record pointer is null.
+        Case("s1", {"eax": 0x10A600, "edx": 0x10A700, "ebx": 0},
+             {0x10A642: b"\x00", 0x001077A8: le32(0x10A600), 0x001077AC: le32(0)}),
+        # s2: the slot's +0x42 bit 0x08.
+        Case("s2", {"eax": 0x10A600, "edx": 0x10A700, "ebx": 0},
+             {0x10A642: b"\x08", 0x001077A8: le32(0x10A600), 0x001077AC: le32(0x10A680)}),
+        # s3: the other's +0x43 bit 0x80 clear.
+        Case("s3", {"eax": 0x10A600, "edx": 0x10A700, "ebx": 0},
+             {0x10A642: b"\x00", 0x10A6C3: b"\x00", 0x001077A8: le32(0x10A600), 0x001077AC: le32(0x10A680)}),
+        # s4: the other's +0x42 bit 0x08.
+        Case("s4", {"eax": 0x10A600, "edx": 0x10A700, "ebx": 0},
+             {0x10A642: b"\x00", 0x10A6C3: b"\x80", 0x10A6C2: b"\x08",
+              0x001077A8: le32(0x10A600), 0x001077AC: le32(0x10A680)}),
+        # s5: bit 0x4000 clear, slot+0x2C <= other+0x2C: 1.
+        Case("s5", {"eax": 0x10A600, "edx": 0x10A700, "ebx": 0},
+             {0x10A642: b"\x00", 0x10A6C3: b"\x80", 0x10A6C2: b"\x00", 0x10A62C: le32(5),
+              0x10A6AC: le32(10), 0x10A728: b"\x00\x00", 0x10A600 + 0x43: b"\x00",
+              0x001077A8: le32(0x10A600), 0x001077AC: le32(0x10A680)}),
+        # s6: the same with slot+0x2C > other+0x2C: 0.
+        Case("s6", {"eax": 0x10A600, "edx": 0x10A700, "ebx": 0},
+             {0x10A642: b"\x00", 0x10A6C3: b"\x80", 0x10A6C2: b"\x00", 0x10A62C: le32(10),
+              0x10A6AC: le32(5), 0x10A728: b"\x00\x00", 0x001077A8: le32(0x10A600), 0x001077AC: le32(0x10A680)}),
+        # s7: bit 0x4000 set, other+0x2C < slot+0x2C: 1.
+        Case("s7", {"eax": 0x10A600, "edx": 0x10A700, "ebx": 0},
+             {0x10A642: b"\x00", 0x10A6C3: b"\x80", 0x10A6C2: b"\x00", 0x10A62C: le32(10),
+              0x10A6AC: le32(5), 0x10A728: b"\x00\x40", 0x001077A8: le32(0x10A600), 0x001077AC: le32(0x10A680)}),
+        # s8: bit 0x4000 set, other+0x2C >= slot+0x2C: 0.
+        Case("s8", {"eax": 0x10A600, "edx": 0x10A700, "ebx": 0},
+             {0x10A642: b"\x00", 0x10A6C3: b"\x80", 0x10A6C2: b"\x00", 0x10A62C: le32(5),
+              0x10A6AC: le32(10), 0x10A728: b"\x00\x40", 0x001077A8: le32(0x10A600), 0x001077AC: le32(0x10A680)}),
+        # s9: the signed <=: -1 <= 1.
+        Case("s9", {"eax": 0x10A600, "edx": 0x10A700, "ebx": 0},
+             {0x10A642: b"\x00", 0x10A6C3: b"\x80", 0x10A6C2: b"\x00", 0x10A62C: le32(0xFFFFFFFF),
+              0x10A6AC: le32(1), 0x10A728: b"\x00\x00", 0x001077A8: le32(0x10A600), 0x001077AC: le32(0x10A680)}),
+        # s10: the signed <: -1 < 1.
+        Case("s10", {"eax": 0x10A600, "edx": 0x10A700, "ebx": 0},
+             {0x10A642: b"\x00", 0x10A6C3: b"\x80", 0x10A6C2: b"\x00", 0x10A62C: le32(1),
+              0x10A6AC: le32(0xFFFFFFFF), 0x10A728: b"\x00\x40",
+              0x001077A8: le32(0x10A600), 0x001077AC: le32(0x10A680)}),
+        # s11: rec+0x28's low byte 0x40: the bit is 0x4000, so the first arm applies (1).
+        Case("s11", {"eax": 0x10A600, "edx": 0x10A700, "ebx": 0},
+             {0x10A642: b"\x00", 0x10A6C3: b"\x80", 0x10A6C2: b"\x00", 0x10A62C: le32(5),
+              0x10A6AC: le32(10), 0x10A728: b"\x40\x00", 0x001077A8: le32(0x10A600), 0x001077AC: le32(0x10A680)}),
+        # s12: the other's +0x42 bit 0x80 set but +0x43 clear: 0.
+        Case("s12", {"eax": 0x10A600, "edx": 0x10A700, "ebx": 0},
+             {0x10A642: b"\x00", 0x10A6C3: b"\x00", 0x10A6C2: b"\x80", 0x10A62C: le32(5),
+              0x10A6AC: le32(10), 0x10A728: b"\x00\x00", 0x001077A8: le32(0x10A600), 0x001077AC: le32(0x10A680)}),
+    ], eax_mask=0xFF, mutants=("@bit", "@other", "@signed", "@f43")),
+    # 0x1A5AC (record C3 §C3.3): EAX = side. 1 when the side's slot record (ctx[4]) +0x28 has bit
+    # 0x4000 clear. 0x33950 runs on both sides (allow). Mask 0xFF (the caller's `test al,al`).
+    Spec("fighter_1a5ac", 0x1A5AC, [
+        Case("a0", {"eax": 0},
+             {0x001077B0: le32(0x10A600), 0x001077B0 + 0x94: le32(0x10A680),
+              0x10A628: b"\x00\x00", 0x10A680 + 0x28: b"\x00\x40"}),
+        Case("a1", {"eax": 0},
+             {0x001077B0: le32(0x10A600), 0x001077B0 + 0x94: le32(0x10A680),
+              0x10A628: b"\x00\x40", 0x10A680 + 0x28: b"\x00\x00"}),
+        Case("a2", {"eax": 1},
+             {0x001077B0: le32(0x10A600), 0x001077B0 + 0x94: le32(0x10A680),
+              0x10A628: b"\x00\x40", 0x10A680 + 0x28: b"\x00\x00"}),
+        Case("a3", {"eax": 0},
+             {0x001077B0: le32(0x10A600), 0x001077B0 + 0x94: le32(0x10A680),
+              0x10A628: b"\x00\x80", 0x10A680 + 0x28: b"\x00\x40"}),
+        Case("a4", {"eax": 0},
+             {0x001077B0: le32(0x10A600), 0x001077B0 + 0x94: le32(0x10A680),
+              0x10A628: b"\x40\x00", 0x10A680 + 0x28: b"\x00\x40"}),
+        Case("a5", {"eax": 0},
+             {0x001077B0: le32(0x10A600), 0x001077B0 + 0x94: le32(0x10A680),
+              0x10A628: b"\x00\xC0", 0x10A680 + 0x28: b"\x00\x00"}),
+    ], allow_calls=(0x33950,), eax_mask=0xFF, mutants=("@slot", "@side", "@bit")),
+    # 0x13DF0 (record C3 §C3.4): walk the active list from the sentinel DS_000FCCE0, tear each node
+    # down (0x13420, stubbed: its own row is C3b), then zero the active counter and the lock. The
+    # port's zero-sentinel guard is a PORT deviation: the cases keep the head non-zero.
+    Spec("effects_clear", 0x13DF0, [
+        # e0: the empty list (the sentinel points at itself): no call, both bytes written.
+        Case("e0", {}, {0x000FCCE0: le32(0x000FCCE0), 0x0009AF3C: b"\x5A", 0x0009AF3D: b"\xA5"}),
+        # e1: one node.
+        Case("e1", {}, {0x000FCCE0: le32(0x10A600), 0x10A600: le32(0x000FCCE0),
+                        0x0009AF3C: b"\x5A", 0x0009AF3D: b"\xA5"}),
+        # e2: three nodes; the walk order is pinned by the recorded calls.
+        Case("e2", {}, {0x000FCCE0: le32(0x10A600), 0x10A600: le32(0x10A620),
+                        0x10A620: le32(0x10A640), 0x10A640: le32(0x000FCCE0),
+                        0x0009AF3C: b"\x5A", 0x0009AF3D: b"\xA5"}),
+    ], calls=(E.Call(0x13420, ("eax",)),), eax_mask=0,
+       mutants=("@walk", "@count", "@lock", "@set", "@one")),
+    # 0x1881C (record C3 §C3.5): latch slot 0 then slot 1 (0x186D0, a stubbed call: its own row
+    # pins it), then return slot0+0x30 - slot1+0x30 at 0x1077E0 and 0x1077E0+0x94. The stub writes
+    # nothing, so the difference is read from the seeded memory; the call list pins the order and
+    # the two side arguments. Mask full (the caller reads the signed difference).
+    Spec("hit_vert_distance", 0x1881C, [
+        Case("v0", {}, {0x001077E0: le32(100), 0x00107874: le32(40)}),
+        Case("v1", {}, {0x001077E0: le32(0), 0x00107874: le32(0xFFFFFFFF)}),
+        Case("v2", {}, {0x001077E0: le32(0x80000000), 0x00107874: le32(1)}),
+        Case("v3", {}, {0x001077E0: le32(0), 0x00107874: le32(0)}),
+    ], calls=(E.Call(0x186D0, ("eax",)),), eax_mask=0xFFFFFFFF,
+       mutants=("@one", "@order", "@side", "@diff")),
+    # 0x36CE4 (record C3 §C3.5): EAX = slot. Set slot+0x43 bit 2; in modes other than 3/0x22
+    # restart the side's DS_00102900 record on the 0xE906E stream at 3.0 (0x2BC30, a stubbed call
+    # with its own row). Mask 0.
+    Spec("fighter_36ce4", 0x36CE4, [
+        # c0: mode 3: only the bit is set.
+        Case("c0", {"eax": 0x10A600}, {0x10A600: le32(0x10A700), 0x10A643: b"\x00",
+             0x00104B00: b"\x03\x00", 0x10A751: b"\x00", 0x00102900: le32(0x10A700),
+             0x00102904: le32(0x10A780)}),
+        # c1: mode 0x22: only the bit.
+        Case("c1", {"eax": 0x10A600}, {0x10A600: le32(0x10A700), 0x10A643: b"\x00",
+             0x00104B00: b"\x22\x00", 0x10A751: b"\x00", 0x00102900: le32(0x10A700),
+             0x00102904: le32(0x10A780)}),
+        # c2: mode 0, side 0: the animation call.
+        Case("c2", {"eax": 0x10A600}, {0x10A600: le32(0x10A700), 0x10A643: b"\x00",
+             0x00104B00: b"\x00\x00", 0x10A750: b"\x01", 0x10A751: b"\x00",
+             0x00102900: le32(0x10A700), 0x00102904: le32(0x10A780)}),
+        # c3: mode 0, side 1: the other record; rec+0x50 differs from rec+0x51.
+        Case("c3", {"eax": 0x10A600}, {0x10A600: le32(0x10A700), 0x10A643: b"\x00",
+             0x00104B00: b"\x00\x00", 0x10A750: b"\x00", 0x10A751: b"\x01",
+             0x00102900: le32(0x10A700), 0x00102904: le32(0x10A780)}),
+    ], calls=(ANIM_BEGIN,), eax_mask=0,
+       mutants=("@bit", "@mode", "@mode22", "@rec", "@stream", "@side")),
+    # 0x2AC80 (record C3 §C3.6): EAX = flag. When the free list is not the empty sentinel, pop its
+    # head (0x249D0, a real call: its own row proves it) and insert it at the active list's head
+    # (0x249B0 real) or, when the flag's 0x400 bit (CH bit 2) is set, at the tail (0x249C0, not in
+    # the call set: it has no row, and both bounds read its final bytes). The two 0x2EA30
+    # interrupt-lock calls run on the original side only (allow); with the lock byte zero their net
+    # write is zero, so the memory at the recorded calls agrees.
+    Spec("actor_alloc", 0x2AC80, [
+        Case("a0", {"eax": 0}, {0x000BCD60: b"\x00", 0x00105B3C: le32(0x10A600), 0x10A600: le32(0x00105B3C),
+             0x10A604: le32(0x00105B3C), 0x00105B40: le32(0x5A5A5A5A),
+             0x00105BCC: le32(0x00105BCC), 0x00105BD0: le32(0x00105BCC)}),
+        Case("a1", {"eax": 0x400}, {0x000BCD60: b"\x00", 0x00105B3C: le32(0x10A600), 0x10A600: le32(0x00105B3C),
+             0x10A604: le32(0x00105B3C), 0x00105B40: le32(0x5A5A5A5A),
+             0x00105BCC: le32(0x00105BCC), 0x00105BD0: le32(0x00105BCC)}),
+        Case("a2", {"eax": 0}, {0x000BCD60: b"\x00", 0x00105B3C: le32(0x00105B3C), 0x00105BCC: le32(0x00105BCC),
+             0x00105BD0: le32(0x12345678)}),
+        Case("a3", {"eax": 0xFFFFFFFF}, {0x000BCD60: b"\x00", 0x00105B3C: le32(0x10A600), 0x10A600: le32(0x00105B3C),
+             0x10A604: le32(0x00105B3C), 0x00105B40: le32(0x5A5A5A5A),
+             0x00105BCC: le32(0x00105BCC), 0x00105BD0: le32(0x00105BCC)}),
+        Case("a4", {"eax": 1}, {0x000BCD60: b"\x00", 0x00105B3C: le32(0x10A600), 0x10A600: le32(0x00105B3C),
+             0x10A604: le32(0x00105B3C), 0x00105B40: le32(0x5A5A5A5A),
+             0x00105BCC: le32(0x00105BCC), 0x00105BD0: le32(0x00105BCC)}),
+    ], allow_calls=(0x2EA30, 0x249C0),
+       calls=(E.Call(0x249D0, ("eax",), mode="real"),
+              E.Call(0x249B0, ("eax", "edx"), mode="real")),
+       eax_mask=0xFFFFFFFF, mutants=("@flag", "@unlink", "@tail", "@empty", "@ret")),
+]
+
 SPECS = [
     Spec("rng_next", 0x5D7DC, [
         Case("r1", {"eax": 0x1234}, {DS_RNG: le32(0x12345678)}),
@@ -4275,7 +4593,7 @@ SPECS = [
         Case("d0", {}, {DS_1078FC: b"\x00"}),
         Case("d1", {"eax": U6_REC}, {DS_1078FC: b"\x5a"}),
     ], eax_mask=0),
-] + E3_SPECS + P1_SPECS + P1_ANIM_SPECS + P2_SPECS + P3_SPECS + P6_SPECS + P45_SPECS + C1_SPECS + C2_SPECS + C2B_SPECS + P7_SPECS + P8_SPECS
+] + E3_SPECS + P1_SPECS + P1_ANIM_SPECS + P2_SPECS + P3_SPECS + P6_SPECS + P45_SPECS + C1_SPECS + C2_SPECS + C2B_SPECS + C3_SPECS + P7_SPECS + P8_SPECS
 
 
 # ---- driver -------------------------------------------------------------------------------------
diff --git a/tools/tests/test_diff_verify.py b/tools/tests/test_diff_verify.py
index f5d4224..a6bc9e8 100644
--- a/tools/tests/test_diff_verify.py
+++ b/tools/tests/test_diff_verify.py
@@ -943,6 +943,95 @@ C2B_KINDS = {
     "spawn_anim_opcode@skip": {"byte"},
 }
 
+# Track P batch C3 (record 2026-10-05-reverse-c3): the frontier callee rows with their EAX
+# masks, and what alone catches each of their mutants.
+C3_MASKS = {
+            "list_insert_after":         0x0,
+            "list_unlink":               0x0,
+            "fighter_164e8":             0x0,
+            "snd_music_unpause":         0x0,
+            "snd_sample_unpause":        0x0,
+            "snd_music_request":         0xff,
+            "mode1_cursor":              0x0,
+            "fighter_pass_flag":         0xff,
+            "fighter_input_read":        0xffff,
+            "fighter_41310":             0x0,
+            "fighter_state_365c8":       0xff,
+            "fighter_1a5ac":             0xff,
+            "effects_clear":             0x0,
+            "hit_vert_distance":         0xffffffff,
+            "fighter_36ce4":             0x0,
+            "actor_alloc":               0xffffffff,
+}
+C3_KINDS = {
+    "actor_alloc@empty": {"call #0", "call #1", "eax"},
+    "actor_alloc@flag": {"call #0", "call #1"},
+    "actor_alloc@ret": {"call #0", "call #1", "eax"},
+    "actor_alloc@tail": {"call #0", "call #1"},
+    "actor_alloc@unlink": {"byte", "call #0", "call #1"},
+    "effects_clear@count": {"byte"},
+    "effects_clear@lock": {"byte"},
+    "effects_clear@one": {"call #1", "call #2"},
+    "effects_clear@set": {"call #0 memory", "call #1 memory", "call #2 memory"},
+    "effects_clear@walk": {"call #0", "call #1", "call #2"},
+    "fighter_164e8@byte": {"byte"},
+    "fighter_164e8@noside": {"byte"},
+    "fighter_164e8@side": {"byte"},
+    "fighter_1a5ac@bit": {"eax"},
+    "fighter_1a5ac@side": {"eax"},
+    "fighter_1a5ac@slot": {"eax"},
+    "fighter_36ce4@bit": {"byte", "call #0 memory"},
+    "fighter_36ce4@mode": {"call #0"},
+    "fighter_36ce4@mode22": {"call #0"},
+    "fighter_36ce4@rec": {"call #0"},
+    "fighter_36ce4@side": {"call #0"},
+    "fighter_36ce4@stream": {"call #0"},
+    "fighter_41310@add": {"byte"},
+    "fighter_41310@clamp": {"byte"},
+    "fighter_41310@eq": {"byte"},
+    "fighter_41310@mode": {"byte"},
+    "fighter_41310@side": {"byte"},
+    "fighter_input_read@pos": {"eax"},
+    "fighter_input_read@side": {"eax"},
+    "fighter_input_read@sign": {"eax"},
+    "fighter_input_read@wrap": {"eax"},
+    "fighter_pass_flag@eq": {"eax"},
+    "fighter_pass_flag@set": {"byte"},
+    "fighter_pass_flag@shift": {"eax"},
+    "fighter_pass_flag@side": {"byte", "eax"},
+    "fighter_state_365c8@bit": {"eax"},
+    "fighter_state_365c8@f43": {"eax"},
+    "fighter_state_365c8@other": {"eax"},
+    "fighter_state_365c8@signed": {"eax"},
+    "hit_vert_distance@diff": {"eax"},
+    "hit_vert_distance@one": {"call #1"},
+    "hit_vert_distance@order": {"call #0", "call #1"},
+    "hit_vert_distance@side": {"call #0"},
+    "list_insert_after@back": {"byte"},
+    "list_insert_after@head": {"byte"},
+    "list_insert_after@next": {"byte"},
+    "list_insert_after@skip": {"byte"},
+    "list_unlink@link": {"byte"},
+    "list_unlink@one": {"byte"},
+    "list_unlink@prev": {"byte"},
+    "list_unlink@swap": {"byte"},
+    "mode1_cursor@cmp": {"byte"},
+    "mode1_cursor@pset": {"byte"},
+    "mode1_cursor@shl": {"byte"},
+    "mode1_cursor@sub": {"byte"},
+    "mode1_cursor@y": {"byte"},
+    "snd_music_request@al": {"eax"},
+    "snd_music_request@cc": {"byte"},
+    "snd_music_request@d9": {"byte"},
+    "snd_music_request@pause": {"byte", "eax"},
+    "snd_music_request@seq": {"byte", "eax"},
+    "snd_music_unpause@db": {"byte"},
+    "snd_music_unpause@one": {"byte"},
+    "snd_music_unpause@word": {"byte"},
+    "snd_sample_unpause@da": {"byte"},
+    "snd_sample_unpause@one": {"byte"},
+}
+
 @needs_unicorn
 @unittest.skipUnless((os.path.exists(DIFFRUN) and os.path.exists(EXE)) or REQUIRED,
                      "build/diffrun or PRAGE.EXE absent")
@@ -967,7 +1056,7 @@ class RealFunctionTests(unittest.TestCase):
                                              "fighter_slot_flag", "hit_anim_ctx", "hit_anim_start_b",
                                              "host_1b890", "rng_next"] + list(P1_MASKS) + list(P2_MASKS)
                                             + list(P3_MASKS) + list(P45_MASKS) + list(C1_MASKS)
-                                            + list(P6_MASKS) + list(C2_MASKS) + list(C2B_MASKS) + list(P7_MASKS) + list(P8_MASKS)))
+                                            + list(P6_MASKS) + list(C2_MASKS) + list(C2B_MASKS) + list(C3_MASKS) + list(P7_MASKS) + list(P8_MASKS)))
         for name, r in self.real.items():
             if name == "host_1b890":       # the named gap (record E3 §E3.8), tested on its own below
                 continue
@@ -983,7 +1072,7 @@ class RealFunctionTests(unittest.TestCase):
             "fighter_45878@mutant", "fighter_ctx_same@mutant", "fighter_slot_flag@mutant",
             "hit_anim_ctx@mutant", "hit_anim_start_b@mutant", "hit_anim_start_b@set", "rng_next@mutant"]
             + list(P1_KINDS) + list(P2_KINDS) + list(P3_KINDS) + list(P45_KINDS) + list(C1_KINDS)
-            + list(P6_KINDS) + list(C2_KINDS) + list(C2B_KINDS) + list(P7_KINDS) + list(P8_KINDS)))
+            + list(P6_KINDS) + list(C2_KINDS) + list(C2B_KINDS) + list(C3_KINDS) + list(P7_KINDS) + list(P8_KINDS)))
         for name, r in self.mut.items():
             self.assertEqual(r.verdict, "MISMATCH", name)
 
@@ -1048,7 +1137,7 @@ class RealFunctionTests(unittest.TestCase):
             "fighter_3640c": 0, "fighter_37dcc": 0,
             "fighter_23130": 0xFF, "fighter_45878": 0, "anim_10fa8": 0, "anim_3e4e4": 0,
             "fighter_ctx_same": 0, "hit_anim_ctx": 0, "hit_anim_start_b": 0, "host_1b890": 0xFFFFFFFF,
-            **P1_MASKS, **P2_MASKS, **P3_MASKS, **P45_MASKS, **C1_MASKS, **P6_MASKS, **C2_MASKS, **C2B_MASKS,
+            **P1_MASKS, **P2_MASKS, **P3_MASKS, **P45_MASKS, **C1_MASKS, **P6_MASKS, **C2_MASKS, **C2B_MASKS, **C3_MASKS,
             **P7_MASKS, **P8_MASKS})
         # with the full mask the slot-flag original's scratch bits (case f9: EAX = 0x201) differ
         spec = dataclasses.replace([s for s in V.SPECS if s.name == "fighter_slot_flag"][0],
@@ -1497,6 +1586,84 @@ class RealFunctionTests(unittest.TestCase):
         ):
             self.assertEqual(sorted({p.split(":")[0] for p in self.mut[name].problems}), ids, name)
 
+    def test_each_c3_mutant_is_caught_by_what_it_breaks(self):
+        # track P batch C3 (record 2026-10-05-reverse-c3): what alone catches each mutant; the rows
+        # with callees have mutants caught only by the call list or the memory at a call
+        for name, want in C3_KINDS.items():
+            got = {p.split(": ", 1)[1].split(":")[0] if p.split(": ", 1)[1].startswith("call #")
+                   else p.split(": ", 1)[1].split(" ")[0] for p in self.mut[name].problems}
+            self.assertEqual(got, want, name)
+        # the exact case set that alone catches each mutant (measured on the prototype)
+        for name, ids in (
+        ("actor_alloc@empty", ['a0', 'a1', 'a2', 'a3', 'a4']),
+        ("actor_alloc@flag", ['a0', 'a1', 'a3', 'a4']),
+        ("actor_alloc@ret", ['a0', 'a1', 'a3', 'a4']),
+        ("actor_alloc@tail", ['a0', 'a1', 'a3', 'a4']),
+        ("actor_alloc@unlink", ['a0', 'a1', 'a3', 'a4']),
+        ("effects_clear@count", ['e0', 'e1', 'e2']),
+        ("effects_clear@lock", ['e0', 'e1', 'e2']),
+        ("effects_clear@one", ['e2']),
+        ("effects_clear@set", ['e1', 'e2']),
+        ("effects_clear@walk", ['e1', 'e2']),
+        ("fighter_164e8@byte", ['s0', 's1', 's2']),
+        ("fighter_164e8@noside", ['s1', 's2']),
+        ("fighter_164e8@side", ['s1', 's2']),
+        ("fighter_1a5ac@bit", ['a1', 'a4', 'a5']),
+        ("fighter_1a5ac@side", ['a0', 'a1', 'a2', 'a3', 'a4', 'a5']),
+        ("fighter_1a5ac@slot", ['a0', 'a1', 'a2', 'a3', 'a4', 'a5']),
+        ("fighter_36ce4@bit", ['c0', 'c1', 'c2', 'c3']),
+        ("fighter_36ce4@mode", ['c0']),
+        ("fighter_36ce4@mode22", ['c1']),
+        ("fighter_36ce4@rec", ['c3']),
+        ("fighter_36ce4@side", ['c2', 'c3']),
+        ("fighter_36ce4@stream", ['c2', 'c3']),
+        ("fighter_41310@add", ['g1', 'g2', 'g3', 'g4', 'g5', 'g6']),
+        ("fighter_41310@clamp", ['g3']),
+        ("fighter_41310@eq", ['g5']),
+        ("fighter_41310@mode", ['g0']),
+        ("fighter_41310@side", ['g4']),
+        ("fighter_input_read@pos", ['i0', 'i1', 'i2', 'i3', 'i4']),
+        ("fighter_input_read@side", ['i4']),
+        ("fighter_input_read@sign", ['i0', 'i1', 'i2', 'i4', 'i5']),
+        ("fighter_input_read@wrap", ['i2', 'i5']),
+        ("fighter_pass_flag@eq", ['f5']),
+        ("fighter_pass_flag@set", ['f0', 'f2', 'f6']),
+        ("fighter_pass_flag@shift", ['f3']),
+        ("fighter_pass_flag@side", ['f1', 'f4']),
+        ("fighter_state_365c8@bit", ['s10', 's11', 's7', 's8']),
+        ("fighter_state_365c8@f43", ['s10', 's11', 's12', 's5', 's7', 's9']),
+        ("fighter_state_365c8@other", ['s10', 's11', 's5', 's7', 's9']),
+        ("fighter_state_365c8@signed", ['s10', 's9']),
+        ("hit_vert_distance@diff", ['v0', 'v1', 'v2']),
+        ("hit_vert_distance@one", ['v0', 'v1', 'v2', 'v3']),
+        ("hit_vert_distance@order", ['v0', 'v1', 'v2', 'v3']),
+        ("hit_vert_distance@side", ['v0', 'v1', 'v2', 'v3']),
+        ("list_insert_after@back", ['l0']),
+        ("list_insert_after@head", ['l0', 'l1']),
+        ("list_insert_after@next", ['l0', 'l1', 'l2']),
+        ("list_insert_after@skip", ['l0', 'l1', 'l2']),
+        ("list_unlink@link", ['u0', 'u2']),
+        ("list_unlink@one", ['u0', 'u1', 'u2']),
+        ("list_unlink@prev", ['u0']),
+        ("list_unlink@swap", ['u0']),
+        ("mode1_cursor@cmp", ['y1']),
+        ("mode1_cursor@pset", ['y3', 'y4', 'y5']),
+        ("mode1_cursor@shl", ['y3', 'y4']),
+        ("mode1_cursor@sub", ['y1', 'y2', 'y3', 'y5', 'y6']),
+        ("mode1_cursor@y", ['y0', 'y1', 'y2', 'y6']),
+        ("snd_music_request@al", ['r1', 'r2']),
+        ("snd_music_request@cc", ['r0', 'r3']),
+        ("snd_music_request@d9", ['r0', 'r1', 'r2', 'r3']),
+        ("snd_music_request@pause", ['r1']),
+        ("snd_music_request@seq", ['r2']),
+        ("snd_music_unpause@db", ['p1', 'p2']),
+        ("snd_music_unpause@one", ['p1', 'p2', 'p3']),
+        ("snd_music_unpause@word", ['p1', 'p3']),
+        ("snd_sample_unpause@da", ['p1', 'p2']),
+        ("snd_sample_unpause@one", ['p1', 'p2', 'p3']),
+        ):
+            self.assertEqual(sorted({p.split(":")[0] for p in self.mut[name].problems}), ids, name)
+
     def test_each_p7_mutant_is_caught_by_what_it_breaks(self):
         # track P batch 7 (record 2026-10-04-reverse-p7): what alone catches each mutant
         for name, want in P7_KINDS.items():
@@ -1518,7 +1685,7 @@ class RealFunctionTests(unittest.TestCase):
         # 0x2B150/0x2BD44/0x3B298 carry the caller-observed sets (record §C2b.3): the byte-derived
         # transitive scan over-approximates the Watcom callee-saved registers their callers keep
         # live (E.callee_clobbers' _CALLEE_CLOBBER_FIXES).
-        self.assertEqual(stubs, {0x13244: (), 0x13C70: ("ebx", "edx"), 0x13DF0: (), 0x164E8: (),
+        self.assertEqual(stubs, {0x13244: (), 0x13420: (), 0x13C70: ("ebx", "edx"), 0x13DF0: (), 0x164E8: (),
                                  0x18350: ("edx",), 0x18540: (), 0x186D0: (), 0x18714: (), 0x18788: (),
                                  0x187FC: (), 0x1881C: (), 0x1883C: ("ebx", "edx"), 0x188AC: ("edx",),
                                  0x188DC: ("edx",), 0x1890C: ("edx",), 0x189FC: (), 0x18A4C: (),
@@ -1640,8 +1807,8 @@ class RealFunctionTests(unittest.TestCase):
                          "--self-check"])
         self.assertEqual(rc, 0)
         # the closed-row count is over the rows that have callees (167), the 34 without are counted apart
-        self.assertIn("diff-verify: 201/201 functions VERIFIED; 598/598 mutants detected; 1 named gaps; "
-                      "148/167 rows with callees closed (34 have none).", out.getvalue())
+        self.assertIn("diff-verify: 217/217 functions VERIFIED; 664/664 mutants detected; 1 named gaps; "
+                      "154/172 rows with callees closed (45 have none).", out.getvalue())
 
 
 # ---- E3: the call list, named gaps, the callee column (record 2026-10-01-reverse-e3 §E3.4, §E3.8) --
```
