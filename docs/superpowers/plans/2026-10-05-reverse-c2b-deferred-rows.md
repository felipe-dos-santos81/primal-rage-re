# C2b: the nine deferred callee rows (track P, batch C2b) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add the differential-verification row for each of the nine callees C2 deferred (C2 record
§C2.5), so the dependent rows close: each gets its own `Spec`, binding, mutants and, where needed,
new `PR_SEAM` first statements on its ported callees; verification only — no ported function, no
`fn_register`, no E2 move.

**Architecture:** Each row is a `Spec` in `tools/diff_verify.py` (`C2B_SPECS`), a `b_*` binding and
`m_*` mutants in `port/tests/diff_runner.c` (`k_bindings`), and exact-set assertions in
`tools/tests/test_diff_verify.py` (`C2B_MASKS`, `C2B_KINDS`, the case-set table, the clobber table,
the counter line). A callee a row stubs gets `PR_SEAM`/`PR_SEAM_RET`/`PR_SEAM_RET0` as its first
statement (E3 §E3.10); a callee proven by its own check runs `mode="real"`; a leaf whose arguments
cannot be compared runs `allow`. The prototype found one raw-over-port correction (the `0x2B2A0`
opcode 0x2D arm order) and three caller-observed clobber corrections (`E.callee_clobbers`'
`_CALLEE_CLOBBER_FIXES` for `0x2B150`, `0x2BD44`, `0x3B298`), baked into the patch (record §C2b.2).

**Tech Stack:** C11 (`port/src`, `port/tests`), Python 3.12 (`unittest`), `unicorn` 2.1.4 and
`capstone` 5.0.7 (`tools/requirements-diff.txt`), CMake, `make`.

**Spec:** `docs/superpowers/specs/2026-09-30-reverse-completion-design.md` §4 track P, §5.1-§5.3,
§6 ("each ported function has a verification row; unhit blocks and non-emulable instructions named;
counters and README percentage updated"), §7.

**Derivation record:** `docs/superpowers/plans/2026-10-05-reverse-c2b-derivations.md` (§C2b.1 the
nine rows and their seam lists checked against the raw, §C2b.2 the rows, their seams and the
corrections, §C2b.3 the mutants and their measured catching sets, §C2b.4 the counters, §C2b.5 the
named gaps and limits, §C2b.6 what the planner ran, §C2b.7 the new frontier). Recipe:
`2026-10-01-reverse-e3-derivations.md` §E3.10; checklist:
`.superpowers/sdd/2026-10-01-plans/p-track-review-checklist.md`.

**What the planner ran (scratch, 2026-10-05, on `reverse-c2b` at `main` `e43712f`; image sha1
`ff3b8cb14e00f1c282de7b7e15dcd7c230766947`):** the baseline (Task 1) and a full prototype of the
nine rows: the patch below applied to the clean worktree, each row measured with
`--function NAME --self-check` until VERIFIED with every mutant detected, then `make diff-verify`
(`201/201 functions VERIFIED; 598/598 mutants detected; 1 named gaps; 148/167 rows with callees
closed (34 have none)`), `make entry-triage` (byte-identical: `targets 233 unported, 262 ported;
supplement 131 (3 unported, 0 stale); untrusted 30`; voice `0 / 115 / 19`),
`PR_ORACLE_REQUIRED=1 ./build/run_tests` (all checks passed) and
`python3 tools/port_progress.py` (`771 1203 64` / `731 731 100`). Every expected output below is
the prototype's measured output. The prototype was then reverted (`git checkout -- port tools`); the
patch is the exact diff it applied.

**Re-baseline note.** The counters below are `e43712f`'s. If `main` moves before this plan executes,
Task 1 records the measured base and every later expected counter adds this plan's increments:
functions +9, mutants +46, rows with callees +9, closed rows +79 (base rows whose callees now have
rows). The E2 table must not move (no ported function, no `fn_register`): if a task regenerates it,
the line must be byte-identical.

## Decisions needed from the user

**None.** The scope is C2's deferral list (record §C2.5), verified against the raw. The one
raw-over-port correction (the opcode 0x2D arm order, record §C2b.2) is raw-over-plan and recorded
with its address; the three clobber corrections are pinned by the callers' live-register use.

## The C2b roadmap

| task | what | commit |
|---|---|---|
| 1 | baseline on `e43712f` (no commit) | - |
| 2 | the nine rows, their seams, bindings, mutants and the test exact-set updates | `tools:`/`tests:` |
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
  adds the C2b case-set test; no other assertion changes.
- AGENTS.md: "`data/` is git-ignored and read-only — never write to it." / "Commit style:
  `<area>: <what changed>`. **Never `git add -A`** — stage named files." No `Co-Authored-By`
  trailer. Never run `make gp-capture`.
- Spec §5.3 / E3 §E3.4: "VERIFIED" is equivalence on the exercised blocks and inputs only, each
  function with its callees stubbed or run as stated.

## Review Focus

1. **Observable stores and neighbours** (checklist 1-2). Every field a row writes carries a
   sentinel that differs from what it writes; every neighbour byte is seeded. The planner's store
   sweep (record §C2b.6) found and fixed the `c2b_res` payload-base arithmetic (the handle offset is
   added twice by both sides) during the prototype.
2. **Inputs that separate alternatives** (checklist 3). Side 0 and 1 (`0x3B298`, `0x36870`,
   `0x39834`, `0x3B714`); signed bounds (`0x2B2A0`'s op 0x18/0x19/0x2D arms); the per-case stub
   EAX (the `sound_voice` sweep, `0x36870`'s 0x365C8/0x36638, `0x39834`'s 0x39738); the 0x1F
   prefix form for the opcodes above 0x1F. The `C2B_KINDS` case-set table pins the case that alone
   catches each mutant.
3. **Every call observed** (checklist 5). Each row with callees has at least one mutant caught only
   by the call list or the memory at a call (record §C2b.3).
4. **Seams** (checklist 9). First statement; args = entry registers = C signature order = `E.Call`;
   clobbers re-derived (`test_each_stub_declares_the_registers_its_callee_clobbers` extended); no
   existing row changes (the full self-check at Task 2's gate: 201/201, 598/598).
5. **The two wide-blast corrections.** The `0x2B2A0` op 0x2D arm swap is a game-behaviour fix
   (raw-over-port, 0x2B8AB); the `callee_clobbers` fixes change three stub declarations only (the
   poison direction), and every previously passing row must stay VERIFIED (the full self-check).
6. **E2 and the counters.** `make entry-triage` byte-identical; `port_progress.py` `771 1203 64` /
   `731 731 100`; the counter line in the test file is the measured `201/201 ... 598/598 ... 148/167
   (34 have none)`.

## Where to run

The worktree `.worktrees/reverse-c2b` (branch `reverse-c2b`). Every command runs from the worktree
root; `data/` and `.superpowers/` are symlinks; `build/` exists. Before Task 1:

```bash
cd /Users/felipe.dos.santos/code/mine/primal-rage-reverse/.worktrees/reverse-c2b
git rev-parse HEAD   # e43712f
python3 -c "import unicorn, capstone; print(unicorn.__version__, capstone.__version__)"  # 2.1.4 5.0.7
cmake -S port -B build && cmake --build build 2>&1 | tail -1
./build/diffrun --exe data/game/C/PRAGE.EXE --image-out /tmp/pr_c2b_img.bin && shasum /tmp/pr_c2b_img.bin
ls port/tests/ghidra_data.bin   # the PR_ORACLE_REQUIRED fixture (git-ignored); copy it from the main repo if absent
```

The last-but-one line must print `ff3b8cb14e00f1c282de7b7e15dcd7c230766947`; another hash means the
image differs from the one the record measured: stop. Ledger:
`.superpowers/sdd/2026-10-05-reverse-c2b-deferred-rows/progress.md`.

### The nine families (the brief's grouping; the patch is one diff)

The prototype was one `git diff`; splitting it into family patches would be artificial. Task 2
applies it at once and then gates each family separately, in this order:

| family | rows | shared seams |
|---|---|---|
| the host pair | `0x33754` `0x13C70` | the allowed `0x1B544`/`0x249B0`/`0x249D0`, the `res_resolve` fixture |
| the opcode family | `0x2B2A0` `0x2AE14` | `0x2B8F8`/`0x29DB8` seams, the `anim_operand` allow |
| the type family | `0x3B298` `0x3B714` `0x39834` `0x36870` | the `fighter.c` seams (`fighter_input_*`, the `0x365C8`/`0x36BC8`/`0x3C520`/`0x379C4`/`0x164E8`/`0x385B0`/`0x39040`/`0x39738`/`0x392A0`/`0x36CE4`/`0x4F434`/`0x3B134`/`0x2BD44`/`0x3AAFC`/`0x3AD98`/`0x3AE9C`/`0x3B080`/`0x3B6C4`/`0x3C59C`/`0x2A820`/`0x2A620`/`0x2AC80` seams) |
| the voice dispatcher | `0x2C3FC` | the `flow.c` audio seams |

## How the code steps are written

Task 2's patch is the prototype's exact `git diff` (the planner applied it, ran every gate, and
reverted). Apply it with `git apply`; it touches `port/src/game/{actors.c,actors.h,fight.c,fighter.c,
fighter.h,flow.c,flow.h}`, `port/tests/diff_runner.c`, `tools/diff_verify.py`, `tools/diff_emu.py`
and `tools/tests/test_diff_verify.py`. If `main` moved, `git apply --3way` and resolve by keeping
the patch's additions at the same anchors; never merge the E2 table by hand.

## File Structure

| File | Responsibility |
|---|---|
| `port/src/game/actors.c/.h` | the `0x29DB8`/`0x2B8F8`/`0x2AC80`/`0x2A820`/`0x2A620` seams and exports, the `anim_operand` signature |
| `port/src/game/fighter.c/.h` | the twenty `fighter.c` seams and exports (the type-family callees) |
| `port/src/game/fight.c` | the `0x3B134` seam |
| `port/src/game/flow.c/.h` | the eight audio seams/exports and the `0x2B2A0` op-0x2D arm swap |
| `port/tests/diff_runner.c` | the nine bindings and 46 mutants, the shared mutant cores |
| `tools/diff_verify.py` | `C2B_SPECS`, the C2b fixtures (`c2b_res`/`c2b_pal_*`/`c2b_3b298_pokes`/`c2b_3b714`/`c2b_39834`/`c2b_36870`/`c2b_2ae14`/`c2b_voice`/`c2b_op`), the `0x2B2A0` op-0x2D arm swap |
| `tools/diff_emu.py` | `_CALLEE_CLOBBER_FIXES` (the caller-observed clobber sets) |
| `tools/tests/test_diff_verify.py` | `C2B_MASKS`, `C2B_KINDS`, the case-set test, the clobber table, the counter line |
| `docs/superpowers/plans/2026-10-05-reverse-c2b-derivations.md` | the record (this plan's source of values) |
| `docs/PROGRESS.md` | Task 4's paragraph |

---

### Task 1: Baseline (no commit)

**Files:** none. **Interfaces:** consumes the worktree at `e43712f`; produces the baseline counters
and table.

- [ ] **Step 1: the task gates on the untouched tree.**

```bash
make diff-verify DIFF_IMAGE=/tmp/pr_c2b_base.bin DIFF_TABLE=/tmp/pr_c2b_base_table.md 2>&1 | tail -3
make entry-triage E2_IMAGE=/tmp/pr_c2b_e2.bin 2>&1 | tail -3
python3 tools/port_progress.py
```

Expected (measured at `e43712f`):

```
diff-verify: 192/192 functions VERIFIED; 552/552 mutants detected; 1 named gaps; 69/158 rows with callees closed (34 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
entry-triage: targets 233 unported, 262 ported; supplement 131 (3 unported, 0 stale); untrusted entries 30
entry-triage: voice sites outside Ghidra 134: 0 in unported code, 115 in ported code, 19 nowhere
771 1203 64
731 731 100 (portable: excludes 81 host-owned/deferred and runtime >= 5D000)
```

- [ ] **Step 2: enumerate the unverified set from the table.** `python3` over
  `/tmp/pr_c2b_base_table.md`: the callees marked `unverified` that are the nine deferred rows'
  (record §C2b.1). Expected: the nine addresses.

### Task 2: the nine rows (one commit)

**Files:** the patch below. **Interfaces:** produces `C2B_SPECS`, the C2b call declarations, the
seams, the clobber fixes, the op-0x2D correction and the test exact-set updates; consumes the
E3/P1-P8 fixtures (`E3_REC`, `E3_REC2`, `SLOT_PTRS`, `DS_SLOTS`, `C2_POOL`, ...).

- [ ] **Step 1: apply the patch.** `git apply - <<'PATCH' ... PATCH` with the diff embedded at the
  end of this task (the exact prototype diff; 2158 insertions over 11 files).

- [ ] **Step 2: build and run each family's self-check.**

```bash
cmake --build build 2>&1 | tail -1
for f in palette_acquire effects_spawn fighter_command_dispatch fighter_reaction \
         fighter_39834 fighter_36870 actor_spawn sound_voice spawn_anim_opcode; do
  python3 tools/diff_verify.py --function "$f" --self-check
done
```

Expected rows (measured; the format is row, entry, cases, blocks, mutants, mask):

| row | entry | cases | blocks | mutants | mask |
|---|---|---|---|---|---|
| `palette_acquire` | 0x33754 | 7 | 14/15 (0x3384E named: the table-full fatal) | 6/6 | full |
| `effects_spawn` | 0x13C70 | 5 | 12/12 | 5/5 | 0 |
| `fighter_command_dispatch` | 0x3B298 | 15 | 28/28 | 7/7 | 0xFF |
| `fighter_reaction` | 0x3B714 | 9 | 22/23 (0x3B75D named: the 0xFF fatal) | 6/6 | 0 |
| `fighter_39834` | 0x39834 | 7 | 16/16 | 5/5 | 0 |
| `fighter_36870` | 0x36870 | 14 | 31/31 | 5/5 | 0 |
| `actor_spawn` | 0x2AE14 | 11 | 32/33 (0x2B071 named dead) | 4/4 | full |
| `sound_voice` | 0x2C3FC | 62 | 100/100 | 4/4 | 0xFF |
| `spawn_anim_opcode` | 0x2B2A0 | 59 | 84/85 (0x2B52F named dead) | 4/4 | 0xFF |

- [ ] **Step 3: the task gates.**

```bash
make diff-verify DIFF_IMAGE=/tmp/pr_c2b_after.bin DIFF_TABLE=/tmp/pr_c2b_after_table.md 2>&1 | tail -3
python3 -m unittest tools.tests.test_diff_verify 2>&1 | tail -3
make entry-triage E2_IMAGE=/tmp/pr_c2b_after_e2.bin 2>&1 | tail -2
PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -1
```

Expected: `201/201 functions VERIFIED; 598/598 mutants detected; 1 named gaps; 148/167 rows with
callees closed (34 have none)`; `OK` (104 tests); E2 byte-identical; `all checks passed`.

- [ ] **Step 4: commit.**

```bash
git add port/src/game/actors.c port/src/game/actors.h port/src/game/fight.c \
        port/src/game/fighter.c port/src/game/fighter.h port/src/game/flow.c port/src/game/flow.h \
        port/tests/diff_runner.c tools/diff_verify.py tools/diff_emu.py tools/tests/test_diff_verify.py
git commit -m "tools: C2b: the nine deferred callee rows, their seams and mutants"
```

### Task 3: the review sweep (one commit)

**Files:** the patch's rows. **Interfaces:** consumes Task 2's state; produces the store sweep's
findings.

- [ ] **Step 1: the store sweep.** For every C2b row, poke each field the row writes to a value that
  differs from what it writes and re-run `--function NAME --self-check`; a store with no sentinel
  fails some mutant. The prototype's sweep found and fixed the `c2b_res` payload-base arithmetic
  (the handle's low 23 bits are added twice — the entry stores the payload base), the `0x2AE14`
  descriptor's `dp8`/extent order and the op-0x2D arm order.
- [ ] **Step 2: the clobber and case-set re-check.** `python3 -m unittest
  tools.tests.test_diff_verify.RealFunctionTests` (the clobber re-derivation and the C2b case-set
  test). Expected: OK.
- [ ] **Step 3: commit any fix.**

```bash
git add <the fixed files>
git commit -m "tests: C2b review sweep: the store sentinels and the case-set pins"
```

### Task 4: Closure (one commit)

**Files:** `docs/PROGRESS.md`, the record. **Interfaces:** consumes Task 2's measured state.

- [ ] **Step 1: the final gates.**

```bash
make diff-verify DIFF_IMAGE=/tmp/pr_c2b_close.bin 2>&1 | tail -1
make entry-triage E2_IMAGE=/tmp/pr_c2b_close_e2.bin 2>&1 | tail -2
PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -1
python3 tools/port_progress.py
make verify
```

Expected: the Task 2 counter; E2 byte-identical; `all checks passed`; `771 1203 64` / `731 731 100`;
and the full `make verify` exit 0 with the 45 oracle lines equal to
`.superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt` and the `make audio-render` WAV
equal to `before-t2.wav` (the `port/src` changes are seams plus one game-logic correction; the
correction is outside every oracle-visible path, but the full gate is what proves it).

- [ ] **Step 2: append the PROGRESS paragraph** (the nine rows, the counter, the corrections, the new
  frontier of record §C2b.7).

- [ ] **Step 3: commit.**

```bash
git add docs/PROGRESS.md docs/superpowers/plans/2026-10-05-reverse-c2b-derivations.md
git commit -m "docs: C2b closure: the nine deferred rows measured and the new frontier"
```

### The patch

```diff
diff --git a/port/src/game/actors.c b/port/src/game/actors.c
index 9d6bf8c..dadd65c 100644
--- a/port/src/game/actors.c
+++ b/port/src/game/actors.c
@@ -1168,6 +1168,7 @@ void actors_reset(void)
  * arg 5 (`0x2AE37 mov ax, [esp+0x28]`), so actor_spawn threads a5 here. */
 u32 actor_alloc(u32 flag)
 {
+    PR_SEAM_RET(0x2AC80u, flag);
     if (list_head(DS_00105B3C) == 0) return 0;
     u32 rec = DSD(DS_00105B3C);
     list_unlink(rec);
@@ -1359,6 +1360,7 @@ u32 anim_read_var(u32 rec, u32 op)
  * (0x29E6A/0x29E75); 0x4A/0x4B/0x50/0x51 take the value. */
 void anim_write_var(u32 rec, u8 op, u32 value)
 {
+    PR_SEAM(0x29DB8u, rec, (u32)op, value);
     u32 o = (u32)(op & 0x7fu);
     if (o < 0x40u) {
         DSW(DS_00105B4C + ((o + (u32)DSB(rec + 0x51)) & 0x3fu) * 2u) = (u16)value;
@@ -1397,8 +1399,9 @@ void anim_write_var(u32 rec, u8 op, u32 value)
  * scaled by the next word's high nibble and dereferenced against rec+0x0C.
  * The decompiler drops the `*2`/`*4` scalings (0x2BA78/0x2BAA3/0x2BAC9); this
  * transcribes the raw arithmetic. */
-static u32 anim_operand(u32 rec)
+u32 anim_operand(u32 rec)
 {
+    PR_SEAM_RET(0x2B8F8u, rec);
     u32 p = DSD(rec + 8);
     u16 cw = DSW(p);
     u16 mode = (u16)(cw & 0x6000u);
@@ -2583,11 +2586,13 @@ u32 spawn_anim_opcode(u32 rec, u32 index, u32 flag)
          * compared value is the sign-extended word at pset+0x0C. */
         s32 pv = (s32)(s16)DSW(DSD(DS_001014EC)
                                + index * PSET_SIZE + 0x0c);
+        /* 0x2B8AB `cmp edx,eax; jl 0x2B8C2`: pv below ax takes the +4 arm; pv >= ax jumps the
+         * stream through the dword at rec+8 (record C2b §C2b.3, a raw-over-port correction). */
         if (pv < (s32)ax) {
+            DSD(rec + 8) += 4u;
+        } else {
             DSD(rec + 8) += 2u;
             DSD(rec + 8) = DSD(DSD(rec + 8)) - 2u;
-        } else {
-            DSD(rec + 8) += 4u;
         }
         return 0;
     }
@@ -2663,8 +2668,9 @@ static void set_dead(u32 rec);
 
 /* 0x2A620. The mode-1 shear cursor: derive rec+0x64 from the current y, or
  * from pset+0x14 (the previous frame's x) when rec+0x1c is zero. */
-static void mode1_cursor(u32 rec, u32 pset)
+void mode1_cursor(u32 rec, u32 pset)
 {
+    PR_SEAM(0x2A620u, rec, pset);
     s32 v;
     if (DSD(rec + 0x1c) == 0)
         v = (s32)DSD(pset + 0x14);
@@ -2768,8 +2774,9 @@ void actor_mode1_pset(u32 rec)
 
 /* 0x2A820. The pset position/layer writer: 0x2A690 for a free record, the
  * parent-relative form for a child, and the on-screen visibility test. */
-static void pset_write(u32 rec, u32 pset)
+void pset_write(u32 rec, u32 pset)
 {
+    PR_SEAM(0x2A820u, rec, pset);
     if ((DSW(rec + 0x28) >> 8 & 0x20u) == 0) {
         if ((DSW(rec + 0x28) >> 8 & 0x04u) == 0) {
             actor_pset_point(rec);
diff --git a/port/src/game/actors.h b/port/src/game/actors.h
index 408521e..24f8e7e 100644
--- a/port/src/game/actors.h
+++ b/port/src/game/actors.h
@@ -24,6 +24,15 @@ void actor_cursor_reset(void);
  * selects the tail insert (0x249C0) over the head insert (0x249B0). 0x2AE14
  * supplies the low 16 bits of its arg 5. */
 u32  actor_alloc(u32 flag);
+
+/* 0x2A820 / 0x2A620 (C2b): 0x2AE14's pset writer and mode-1 cursor, exported
+ * for its differential row's mutants. */
+void pset_write(u32 rec, u32 pset);
+
+/* 0x2B8F8 (C2b): the operand fetch 0x2B2A0 calls, exported for its row's
+ * mutants (0x29DB8 is already declared above the actors_init section). */
+u32 anim_operand(u32 rec);
+void mode1_cursor(u32 rec, u32 pset);
 /* 0x2AE14. `desc` points at a descriptor in mem[]; the four register arguments
  * are a2=EDX, a3=ECX, a4=EBX and a5=the stack word, pinned by disassembly in
  * docs/superpowers/plans/2026-09-17-actor-system-args.md. Returns the record's
diff --git a/port/src/game/fight.c b/port/src/game/fight.c
index 657d58c..409e1a1 100644
--- a/port/src/game/fight.c
+++ b/port/src/game/fight.c
@@ -2033,6 +2033,7 @@ static int fight_attack_ready(u32 side)
 
 void fight_command_map(u32 side, u32 edx_arg, u32 override)
 {
+    PR_SEAM(0x3B134u, side, edx_arg, override);
     u32 ctx[6];
     fighter_ctx_swap(ctx, side);                             /* 0x3B149 */
     u32 anim[3];
diff --git a/port/src/game/fighter.c b/port/src/game/fighter.c
index 87dbcd8..c5dfd9c 100644
--- a/port/src/game/fighter.c
+++ b/port/src/game/fighter.c
@@ -39,8 +39,8 @@ void fighter_37d18(u32 slot, u32 rec);                   /* 0x37D18 */
 void fighter_36870(u32 rec);                             /* 0x36870 */
 void fighter_37178(u32 slot);                            /* 0x37178 */
 void fighter_385b0(u32 rec);                             /* 0x385B0 */
-static void fighter_379c4(u32 slot);                     /* 0x379C4 */
-static void fighter_164e8(u32 side);                     /* 0x164E8 */
+void fighter_379c4(u32 slot);                           /* 0x379C4 */
+void fighter_164e8(u32 side);                           /* 0x164E8 */
 
 /* The winner-body helpers the think chain 0x1975C/0x3B464 shares; defined with
  * the 0x193B0 and 0x3B714 blocks below. */
@@ -588,8 +588,9 @@ void fighter_pass_b(u32 arg)
 
 /* 0x46460. One word of player `side`'s 0x28-stride input-history ring at
  * `index` steps behind the ring position (wrapping modulo 0x14). */
-static u32 fighter_input_read(u32 side, s32 index)
+u32 fighter_input_read(u32 side, s32 index)
 {
+    PR_SEAM_RET(0x46460u, side, (u32)index);
     s32 pos = (s32)DSD(DS_001082D2) >> 16;      /* 0x46466/0x4646E */
     if (index > 0) {
         s32 n = index;
@@ -712,6 +713,7 @@ void fighter_block_end(u32 rec)
 /* 0x1A734 — record §39. */
 void fighter_block_hit(u32 side)
 {
+    PR_SEAM(0x1A734u, side);
     u32 ctx[6];
     fighter_ctx_swap(ctx, side);                        /* 0x1A73C 0x33A10 */
     hit_facing_flag(ctx[1]);                            /* 0x1A745 0x18B04 */
@@ -737,8 +739,9 @@ void fighter_block_hit(u32 side)
  * facing base, or 0 when 0x1AB10 rejects the fighter. The raw returns EDX (the
  * base), not the accumulated OR. The block arm calls 0x18B04 and 0x1A7CC
  * (record §38). */
-static u32 fighter_input_mask(u32 side)
+u32 fighter_input_mask(u32 side)
 {
+    PR_SEAM_RET(0x1AB5Cu, side);
     u32 ctx[6];
     fighter_ctx_swap(ctx, side);                /* 0x1AB6C */
     u32 mask = 0;
@@ -1585,8 +1588,9 @@ int fighter_state_36638(u32 slot, u32 rec)
 
 /* 0x365C8. 1 when this slot is behind the other's +0x2C in the facing
  * direction and the other slot's +0x43 bit 0x80 is set. */
-static int fighter_state_365c8(u32 slot, u32 rec, u32 side)
+int fighter_state_365c8(u32 slot, u32 rec, u32 side)
 {
+    PR_SEAM_RET(0x365C8u, slot, rec, side);
     u32 other;
     if ((DSB(slot + 0x42u) & 0x10u) != 0u) return 0;
     other = DSD(DS_001077A8 + (side ^ 1u) * 4u);
@@ -1722,7 +1726,7 @@ void fighter_state_35d7c(u32 side)
 
 static void fighter_state_367dc(u32 slot, u32 rec);         /* 0x367DC */
 static void hit_stance_timer(u32 side);                     /* 0x1922C */
-static void hit_anim_start_c(u32 rec, u32 stream, u32 frame_bits); /* 0x3C520 */
+void hit_anim_start_c(u32 rec, u32 stream, u32 frame_bits); /* 0x3C520 */
 
 /* PORT: data-object addresses symbols.h does not name. */
 #define FIGHT_LAND_THR     0x000BD882u  /* 0x35F84: per-char landing dword */
@@ -2489,6 +2493,7 @@ void fighter_state_35e6c(u32 slot, u32 rec)
  * pair each arena frame, so 0x35658's preamble gate is 0 once per side. */
 int fighter_pass_flag(u32 bit, u32 side)
 {
+    PR_SEAM_RET(0x3C59Cu, bit, side);
     u32 m = 1u << (bit & 0x1Fu);
     if ((DSD(DS_00107D50 + side * 4u) & m) != 0u) return 1;   /* 0x3C5B3 */
     DSD(DS_00107D50 + side * 4u) |= m;                        /* 0x3C5C2 */
@@ -2603,8 +2608,9 @@ static void fighter_state_367dc(u32 slot, u32 rec)
  * Returns 7. The 0x36CA9 second animation (0xE906E on the side's 0x102900
  * record) is dead: both callers (0x350D0 0x35162, 0x36870 0x36A3F) require
  * +0x43 bit 2 set, which forces this function's 0x36CA7 early return. */
-static int fighter_state_36bc8(u32 slot, u32 rec)
+int fighter_state_36bc8(u32 slot, u32 rec)
 {
+    PR_SEAM_RET(0x36BC8u, slot, rec);
     u32 side = (u32)DSB(rec + 0x51u);
     u32 other = 1u - side;
     DSW(slot + 0x74u) = 0;                                  /* 0x36C32 */
@@ -2686,8 +2692,9 @@ void fighter_39a10(u32 rec, u32 value)
 #define FIGHT_379C4_STREAM  0x001078E4u  /* 0x379C4: the +0x41 bit 2 alt stream */
 
 /* 0x164E8. Zero the per-side dword at 0xFD148 + side*4. */
-static void fighter_164e8(u32 side)
+void fighter_164e8(u32 side)
 {
+    PR_SEAM(0x164E8u, side);
     DSD(DS_000FD148 + side * 4u) = 0;                       /* 0x164EB */
 }
 
@@ -2702,6 +2709,7 @@ static void fighter_164e8(u32 side)
  * The raw computes So/rec_o but never reads them. EAX = rec. */
 void fighter_385b0(u32 rec)
 {
+    PR_SEAM(0x385B0u, rec);
     u32 side = (u32)DSB(rec + 0x51u);                       /* 0x385BA */
     u32 s = DS_001077B0 + side * 0x94u;                     /* 0x385E6 */
     DSD(DS_00100AF8 + side * 4u) = 0;                       /* 0x38625 */
@@ -2751,8 +2759,9 @@ void fighter_385b0(u32 rec)
  * non-zero; otherwise set 0x1078FC = 1 and start the 0xC9260[char] animation at
  * 2.0 (or the 0x1078E4 stream when 0x1078E8 is null). Else start the
  * 0xC9260[char] animation at 2.0. EAX = slot. */
-static void fighter_379c4(u32 slot)
+void fighter_379c4(u32 slot)
 {
+    PR_SEAM(0x379C4u, slot);
     u32 rec = DSD(slot);
     if (DSB(DS_001078FE) == 0u) {                           /* 0x379C8 */
         if (DSB(slot + 0x57u) == 2u) fighter_37178(slot);   /* 0x37A4F */
@@ -3298,6 +3307,7 @@ void fighter_38fec(u32 side)
  * table at 0x107A80 + side*0x40. */
 void fighter_39040(u32 side)
 {
+    PR_SEAM(0x39040u, side);
     u32 slot = DS_001077B0 + side * 0x94u;
     if ((s16)DSW(FIGHT_D2C_BASE + side * 2u) > 1) {         /* 0x39056 */
         u32 q = DSD(slot + 0x3Cu) / DSD(DS_000C9520);       /* 0x3908C */
@@ -4150,8 +4160,9 @@ void hit_anim_start_b(u32 rec, u32 stream, u32 frame_bits)
 }
 
 /* 0x3C520. anim-begin plus the +0x2C/+0x30 anchor writes. */
-static void hit_anim_start_c(u32 rec, u32 stream, u32 frame_bits)
+void hit_anim_start_c(u32 rec, u32 stream, u32 frame_bits)
 {
+    PR_SEAM(0x3C520u, rec, stream, frame_bits);
     u32 ctx[6];
     hit_anim_ctx(ctx, rec);                             /* 0x3C52F */
     actors_anim_begin(rec, stream, frame_bits);         /* 0x3C54A */
@@ -5395,8 +5406,9 @@ int fighter_3a280(u32 code)
 
 /* 0x36CE4. Set slot+0x43 bit 2; in modes other than 3/0x22 restart the side's
  * DS_00102900 record on the 0xE906E stream at 3.0. */
-static void fighter_36ce4(u32 slot)
+void fighter_36ce4(u32 slot)
 {
+    PR_SEAM(0x36CE4u, slot);
     u32 rec = DSD(slot);                                    /* 0x36CFC */
     DSB(slot + 0x43u) |= 4u;                                /* 0x36CE5 */
     if (DSW(DS_00104B00) == 3u || DSW(DS_00104B00) == 0x22u)
@@ -5422,8 +5434,9 @@ void fighter_36d98(u32 slot)
 /* 0x4F434. The AI difficulty nudge: with byte[0x10810D] selecting a side and its
  * opposite, compare the two +0x5A bytes scaled by 100/120 and, when the gap
  * clears the thresholds, bump DS_001082C8[opposite] by -1/1 through 0x46534. */
-static void fighter_4f434(void)
+void fighter_4f434(void)
 {
+    PR_SEAM0(0x4F434u);
     u32 sel = (u32)DSB(FIGHTER_10810D);                     /* 0x4F437 */
     u32 opp = sel ^ 1u;                                     /* 0x4F440 */
     s32 diff = (s32)DSB(DS_001077B0 + sel * 0x94u + 0x5Au)
@@ -6046,8 +6059,9 @@ u32 fighter_3aa54(u32 slot)
 /* 0x39738. The reaction damage/knockback scaler: pick the per-character base
  * from the 0xBECxx tables scaled by 100, apply the +0x8C guards and (when
  * slot+0x63 != 0) the DS_001082C8 difficulty multiplier. */
-static s32 fighter_39738(u32 side, s32 b)
+s32 fighter_39738(u32 side, s32 b)
 {
+    PR_SEAM_RET(0x39738u, side, (u32)b);
     u32 ctx[6];
     s32 v;
     fighter_ctx_same(ctx, side);                            /* 0x39743 */
@@ -6165,6 +6179,7 @@ static void fighter_3a0fc(u32 side)
  * Exported for 0x28C38 (record §48-B). */
 void fighter_392a0(u32 slot, s32 v, s32 w)
 {
+    PR_SEAM(0x392A0u, slot, (u32)v, (u32)w);
     u32 rec = DSD(slot);                                    /* 0x392A7 */
     s32 A = (s32)((u32)v * 120u) / 100;                     /* 0x392AD..0x392C7 */
     s32 B = (s32)((u32)w * 68u) / 100;                      /* 0x392C9..0x392DC */
@@ -7516,6 +7531,7 @@ void fighter_24804(u32 side)
  * the two reaction animation words and slot+0x54. */
 void fighter_reaction_apply(u32 slot, u32 reaction)
 {
+    PR_SEAM(0x3AAFCu, slot, reaction);
     u32 ctx[6];
     u32 anim1[3];
     u32 anim2[3];
@@ -7648,6 +7664,7 @@ int fighter_39efc(u32 side)
  * copies self's rec+0x34 to the other record when this holds. */
 int fighter_3b6c4(u32 side)
 {
+    PR_SEAM_RET(0x3B6C4u, side);
     u32 ctx[6];
     fighter_ctx_same(ctx, side);                            /* 0x3B6CC */
     if (DSB(ctx[2] + 0x53u) != 8u) return 0;                /* 0x3B6D5/0x3B6D9 */
@@ -7665,6 +7682,7 @@ int fighter_3b6c4(u32 side)
  * param_2, EBX = param_3, ECX = param_4. */
 void fighter_3b080(u32 side, u32 param_2, u32 param_3, u32 param_4)
 {
+    PR_SEAM(0x3B080u, side, param_2, param_3, param_4);
     s32 v;
     if (DSB(DS_000BEDF2) != 0u) return;                     /* 0x3B08F/0x3B096 */
     v = (s32)(param_2 * 2u);                                /* 0x3B0A1 */
@@ -7688,6 +7706,7 @@ void fighter_3b080(u32 side, u32 param_2, u32 param_3, u32 param_4)
  * DL = param_2 (the 0x3B298 result). */
 void fighter_3ae9c(u32 side, u8 param_2)
 {
+    PR_SEAM(0x3AE9Cu, side, (u32)param_2);
     u32 ctx[6];
     u32 rec;
     s32 sign;
@@ -7731,8 +7750,9 @@ void fighter_3ae9c(u32 side, u8 param_2)
  * +8 = 0x1E1), load its first sprite id through 0x2A408 into its actor row,
  * then mark it dead. EAX = param_1 (the fighter record), EDX = param_2 (the
  * 0x1014F4 row). */
-static void fighter_2bd44(u32 param_1, u32 param_2)
+void fighter_2bd44(u32 param_1, u32 param_2)
 {
+    PR_SEAM(0x2BD44u, param_1, param_2);
     u32 actor;
     DSB(param_1 + 0x4Bu) = DSB(param_2 + 0x4Bu);            /* 0x2BD48/0x2BD4B */
     DSD(param_2 + 0x24u) = 0;                               /* 0x2BD51 */
@@ -7764,6 +7784,7 @@ static void fighter_2bd44_by_index(u32 rec)
  * EDX = &anim. */
 void fighter_3ad98(u32 side, const u32 anim[3])
 {
+    PR_SEAM(0x3AD98u, side, anim[0], anim[1], anim[2]);
     u32 ctx[6];
     u32 off;
     u32 stream;
diff --git a/port/src/game/fighter.h b/port/src/game/fighter.h
index 311fcd5..d4d6da2 100644
--- a/port/src/game/fighter.h
+++ b/port/src/game/fighter.h
@@ -32,6 +32,14 @@ void fighter_anim_triple(u32 out[3], u32 slot_char, s32 edx);
  * <= 1 (unsigned bytes). */
 int fighter_state_ok(u32 side);
 
+/* 0x46460. One word of player `side`'s 0x28-stride input-history ring. The
+ * seam (C2b) records it for 0x3B298's row; the harness reads it too. */
+u32 fighter_input_read(u32 side, s32 index);
+
+/* 0x1AB5C. The facing word: the seven ring reads ORed with the side's command
+ * word, then the 0x1000/0x2000/0x3000 base. The seam (C2b) records it. */
+u32 fighter_input_mask(u32 side);
+
 /* 0x1A570. The "actor bit 15 clear" predicate:
  * (word[actor] & 0x8000) == 0 for slot[side]'s actor record. */
 int fighter_actor_bit15_clear(u32 side);
@@ -208,6 +216,10 @@ int fighter_1a640(u32 side);
  * stack argument = the reaction byte. */
 void fighter_reaction_apply(u32 slot, u32 reaction);
 
+/* 0x2BD44. Copy param_2's +0x4B into param_1 and re-arm param_2; the C2b seam
+ * records 0x3B714's call. */
+void fighter_2bd44(u32 param_1, u32 param_2);
+
 /* 0x3A43C. The 0x3A504 pose family's per-frame handler: phase 0 arms +0x58;
  * phase 1 starts the self record's 0xC8FE0[char] stream at 3.0, re-anchors the
  * self record and snaps the self x to A[side] behind the B[side] and +0x90
@@ -392,6 +404,14 @@ void fighter_38fec(u32 side);
  * 0x107D20/0x107D24 words and the 0x107A80 table. */
 void fighter_39040(u32 side);
 
+/* 0x379C4 / 0x164E8 / 0x365C8 / 0x36BC8 / 0x3C520 (C2b): the 0x36870
+ * machine's callees, exported for its differential row's mutants. */
+void fighter_379c4(u32 slot);
+void fighter_164e8(u32 side);
+int fighter_state_365c8(u32 slot, u32 rec, u32 side);
+int fighter_state_36bc8(u32 slot, u32 rec);
+void hit_anim_start_c(u32 rec, u32 stream, u32 frame_bits);
+
 /* 0x1DE64. The reaction picker: map the side's command word (or, with slot+0x63
  * clear, the 0x46460/0x4649C input scan, record §49-B) through 0x1DDF4 to a
  * reaction code; 0xFF when nothing maps. */
@@ -1113,6 +1133,16 @@ void fighter_19820(void);
  * exported for 0x28C38 (record §48-B). EAX = slot, EDX = v, EBX = w. */
 void fighter_392a0(u32 slot, s32 v, s32 w);
 
+/* 0x39738. The reaction damage/knockback scaler 0x39834 runs; the C2b
+ * seams record its call and 0x36CE4/0x4F434's. */
+s32 fighter_39738(u32 side, s32 b);
+
+/* 0x36CE4. Set slot+0x43 bit 2 (and restart the side's record off modes 3/0x22). */
+void fighter_36ce4(u32 slot);
+
+/* 0x4F434. The AI difficulty nudge 0x39834's tail runs. */
+void fighter_4f434(void);
+
 /* Record §49-Z. 0x39FB0: the pose 0x39F40 (-0x50, 0x64, 0xF, 0x14) on the
  * slot record's side after 0x18B04. EAX = slot. */
 void fighter_39fb0(u32 slot);
diff --git a/port/src/game/flow.c b/port/src/game/flow.c
index bb1b568..64718dd 100644
--- a/port/src/game/flow.c
+++ b/port/src/game/flow.c
@@ -6139,14 +6139,16 @@ int game_music_notes_seen(void) { return s_music_notes; }
 #define SND_VOICE_REC   0x0Cu
 
 /* 0x1D238. Clears the music pause byte DS_001028DA (0x1D23A). */
-static void snd_music_unpause(void)
+void snd_music_unpause(void)
 {
+    PR_SEAM0(0x1D238u);
     DSB(DS_001028DA) = 0;                                  /* 0x1D23A */
 }
 
 /* 0x1D244. Clears the sample pause byte DS_001028DB (0x1D246). */
-static void snd_sample_unpause(void)
+void snd_sample_unpause(void)
 {
+    PR_SEAM0(0x1D244u);
     DSB(DS_001028DB) = 0;                                  /* 0x1D246 */
 }
 
@@ -6154,8 +6156,9 @@ static void snd_sample_unpause(void)
  * song (DS_001028D4/DS_001028D9); unless the music is paused (DS_001028DA ==
  * 1) or there is no sequence handle (DS_001028C0), it becomes the pending
  * song DS_001028CC and AL = 1. */
-static u32 snd_music_request(u32 song, u32 b)
+u32 snd_music_request(u32 song, u32 b)
 {
+    PR_SEAM_RET(0x1CA14u, song, b);
     DSB(DS_001028D9) = (u8)b;                              /* 0x1CA14 */
     DSD(DS_001028D4) = song;                               /* 0x1CA22 */
     if (DSB(DS_001028DA) == 1u) return 0;                  /* 0x1CA27 */
@@ -6176,8 +6179,9 @@ static u32 snd_music_playing(void)
 /* 0x1CA6C. Clears the current song (DS_001028D4 = 0, DS_001028D9 = 0); when
  * the sequence plays, the pending song DS_001028CC = 0 and 0x5DEAF stops it
  * (AL = 1). The caller's EAX/EDX are passed to 0x1CA40, which reads neither. */
-static u32 snd_music_stop(void)
+u32 snd_music_stop(void)
 {
+    PR_SEAM_RET0(0x1CA6Cu);
     DSD(DS_001028D4) = 0;                                  /* 0x1CA7A */
     DSB(DS_001028D9) = 0;                                  /* 0x1CA80 */
     if (DSD(DS_001028C0) == 0u) return 0;                  /* 0x1CA86 */
@@ -6201,8 +6205,9 @@ static s32 snd_slot_status(u32 off)
 /* 0x1CE70. AL = 1 when a slot plays the resource handle `h` (its +0x0C is
  * `h` and 0x5DD03 reports 4); a slot whose +0x0C is `h` but has stopped gets
  * +0x0C = 0 and the scan goes on. AL = 0 without a DIG driver. */
-static u32 snd_sample_playing(u32 h)
+u32 snd_sample_playing(u32 h)
 {
+    PR_SEAM_RET(0x1CE70u, h);
     if (DSD(DS_001028C8) == 0u) return 0;                  /* 0x1CE78 */
     for (u32 off = 0; off < SND_SLOT_END; off += SND_SLOT_STRIDE) {
         if (DSD(DS_0010286C + off) != h) continue;         /* 0x1CE83 */
@@ -6215,8 +6220,9 @@ static u32 snd_sample_playing(u32 h)
 /* 0x1CE04. Stops the first slot playing `h`: a slot whose +0x0C is `h` and
  * whose 0x5DD03 status is not 2 is ended (0x5DC8B) and re-inited (0x5DC0F),
  * its +0x0C = 0, AL = 1. AL = 0 when none (or no DIG driver). */
-static u32 snd_sample_stop(u32 h)
+u32 snd_sample_stop(u32 h)
 {
+    PR_SEAM_RET(0x1CE04u, h);
     if (DSD(DS_001028C8) == 0u) return 0;                  /* 0x1CE0C */
     for (u32 off = 0; off < SND_SLOT_END; off += SND_SLOT_STRIDE) {
         if (DSD(DS_0010286C + off) != h) continue;         /* 0x1CE17 */
@@ -6232,8 +6238,9 @@ static u32 snd_sample_stop(u32 h)
 /* 0x1CD9C. Without a DIG driver AL = 0. Otherwise every slot's +0x04 and
  * +0x0C are cleared and a slot whose status is not 2 is ended and re-inited;
  * AL = 1. */
-static u32 snd_samples_stop_all(void)
+u32 snd_samples_stop_all(void)
 {
+    PR_SEAM_RET0(0x1CD9Cu);
     if (DSD(DS_001028C8) == 0u) return 0;                  /* 0x1CDA2 */
     for (u32 off = 0; off < SND_SLOT_END; off += SND_SLOT_STRIDE) {
         DSD(DS_00102864 + off) = 0;                        /* 0x1CDB5 */
@@ -6375,8 +6382,9 @@ void sound_resume(void)
  * first free of slots 3..0, else the one whose queue time +0x14 is the
  * smallest below now (unsigned; slot 0 when none is). A forced slot is ended
  * and re-inited. Queueing stores +0x04, +0x08 and +0x14; AL = 1. */
-static u32 snd_sample_queue(u32 h, u32 loop)
+u32 snd_sample_queue(u32 h, u32 loop)
 {
+    PR_SEAM_RET(0x1CC28u, h, loop);
     if (DSD(DS_001028C8) == 0u) return 0;                  /* 0x1CC37 */
     if (DSB(DS_001028DB) != 0u) return 0;                  /* 0x1CC44 */
     u32 now = DSD(DS_00101500);                            /* 0x1CC51 0x500BB */
diff --git a/port/src/game/flow.h b/port/src/game/flow.h
index 14cd597..5cab89c 100644
--- a/port/src/game/flow.h
+++ b/port/src/game/flow.h
@@ -197,6 +197,17 @@ void game_isr_word_reset(void);
  * sample in cases 2/3 or an unlisted case-3 id. */
 u32 sound_voice(u32 id);
 
+/* 0x1CA14/0x1CA6C/0x1CC28/0x1CD9C/0x1CE04/0x1CE70/0x1D238/0x1D244 (C2b):
+ * the voice dispatcher's audio callees, exported for its differential row's mutants. */
+u32 snd_music_request(u32 song, u32 b);
+u32 snd_music_stop(void);
+u32 snd_sample_queue(u32 h, u32 loop);
+u32 snd_samples_stop_all(void);
+u32 snd_sample_playing(u32 h);
+u32 snd_sample_stop(u32 h);
+void snd_music_unpause(void);
+void snd_sample_unpause(void);
+
 /* PORT: test seam (record k7-k12 §4), not original state. sound_voice logs
  * the id of every entry, the first SOUND_VOICE_LOG_CAP of them, since the
  * last reset. sound_voice_log_count is the number of entries since the reset
diff --git a/port/tests/diff_runner.c b/port/tests/diff_runner.c
index 8526fd3..a0dcb5a 100644
--- a/port/tests/diff_runner.c
+++ b/port/tests/diff_runner.c
@@ -11,9 +11,13 @@
 #include "game/effects.h"
 #include "game/config.h"
 #include "game/fighter.h"
+#include "game/fight.h"
 #include "game/flow.h"
 #include "game/rng.h"
 #include "game/svcmenu.h"
+#include "platform/res.h"
+#include "platform/render.h"
+#include "platform/gfx.h"
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
@@ -7674,6 +7678,1044 @@ static void m_4b03c_voice(const u32 *r, u32 *eax)      /* voice 0xD5 */
     *eax = 0u;
 }
 
+/* Track P batch C2b (record 2026-10-05-reverse-c2b): the nine deferred callee rows. */
+static void b_33754(const u32 *r, u32 *eax)            { *eax = palette_acquire(r[R_EAX]); }
+static void m_33754(const u32 *r, u32 *eax)            /* the search never matches: always a new entry */
+{
+    u32 handle = r[R_EAX];
+    const u32 *res = (const u32 *)res_resolve(handle);
+    u32 count = res ? *res : 0u;
+    u32 e = 0x00107618u, start = 1u;
+    while (e < 0x00107798u) {
+        u32 next = DSD(e + 8u) + DSD(e + 12u);
+        if (DSD(e) != 0u) start = next;
+        e += 0x10u;
+    }
+    if (e >= 0x00107798u) { *eax = 0u; return; }
+    DSD(e) = handle; DSD(e + 4u) = 1u; DSD(e + 8u) = start; DSD(e + 12u) = count;
+    *eax = e;
+}
+static void m_33754_new(const u32 *r, u32 *eax)        /* a hit still creates a second entry */
+{
+    u32 handle = r[R_EAX];
+    const u32 *res = (const u32 *)res_resolve(handle);
+    u32 count = res ? *res : 0u;
+    u32 e = 0x00107618u, start = 1u;
+    while (e < 0x00107798u) {
+        start = DSD(e + 8u) + DSD(e + 12u);
+        if (DSD(e) == 0u) break;
+        e += 0x10u;
+    }
+    if (e >= 0x00107798u) { *eax = 0u; return; }
+    DSD(e) = handle; DSD(e + 4u) = 1u; DSD(e + 8u) = start; DSD(e + 12u) = count;
+    *eax = e;
+}
+static void m_33754_inc(const u32 *r, u32 *eax)        /* a hit leaves the refcount alone */
+{
+    u32 handle = r[R_EAX];
+    u32 e = 0x00107618u;
+    while (e < 0x00107798u) {
+        if (DSD(e) == handle) { *eax = e; return; }
+        e += 0x10u;
+    }
+    *eax = palette_acquire(handle);
+}
+static void m_33754_start(const u32 *r, u32 *eax)      /* the free-entry start is always 1 */
+{
+    u32 handle = r[R_EAX];
+    const u32 *res = (const u32 *)res_resolve(handle);
+    u32 count = res ? *res : 0u;
+    u32 e = 0x00107618u;
+    while (e < 0x00107798u) {
+        if (DSD(e) == handle) { DSD(e + 4u) = DSD(e + 4u) + 1u; *eax = e; return; }
+        e += 0x10u;
+    }
+    e = 0x00107618u;
+    while (e < 0x00107798u && DSD(e) != 0u) e += 0x10u;
+    if (e >= 0x00107798u) { *eax = 0u; return; }
+    DSD(e) = handle; DSD(e + 4u) = 1u; DSD(e + 8u) = 1u; DSD(e + 12u) = count;
+    *eax = e;
+}
+static void m_33754_reflow(const u32 *r, u32 *eax)     /* the reflow walk is skipped */
+{
+    u32 handle = r[R_EAX];
+    const u32 *res = (const u32 *)res_resolve(handle);
+    u32 count = res ? *res : 0u;
+    u32 e = 0x00107618u, start = 1u;
+    while (e < 0x00107798u) {
+        if (DSD(e) == handle) { DSD(e + 4u) = DSD(e + 4u) + 1u; *eax = e; return; }
+        e += 0x10u;
+    }
+    e = 0x00107618u;
+    while (e < 0x00107798u && DSD(e) != 0u) { start = DSD(e + 8u) + DSD(e + 12u); e += 0x10u; }
+    if (e >= 0x00107798u) { *eax = 0u; return; }
+    DSD(e) = handle; DSD(e + 4u) = 1u; DSD(e + 8u) = start; DSD(e + 12u) = count;
+    palette_record(handle, start, count, 1u);
+    *eax = e;
+}
+static void m_33754_count(const u32 *r, u32 *eax)      /* the entry length is always 1 */
+{
+    u32 handle = r[R_EAX];
+    u32 e = 0x00107618u, start = 1u;
+    while (e < 0x00107798u) {
+        if (DSD(e) == handle) { DSD(e + 4u) = DSD(e + 4u) + 1u; *eax = e; return; }
+        e += 0x10u;
+    }
+    e = 0x00107618u;
+    while (e < 0x00107798u && DSD(e) != 0u) { start = DSD(e + 8u) + DSD(e + 12u); e += 0x10u; }
+    if (e >= 0x00107798u) { *eax = 0u; return; }
+    DSD(e) = handle; DSD(e + 4u) = 1u; DSD(e + 8u) = start; DSD(e + 12u) = 1u;
+    palette_record(handle, start, 1u, 1u);
+    *eax = e;
+}
+
+static void b_13c70(const u32 *r, u32 *eax)
+{
+    *eax = effects_spawn(r[R_EAX], r[R_EDX], r[R_EBX]);
+}
+static void m_13c70(const u32 *r, u32 *eax)            /* forgets the +0x0E latch */
+{
+    u32 src = r[R_EAX], byte_arg = r[R_EDX], handle = r[R_EBX];
+    u32 rec = DSD(DS_000FCCE8), prev;
+    const u32 *res;
+    if (rec == 0u || rec == DS_000FCCE8) { *eax = 0u; return; }
+    prev = DSD(rec + 4u);
+    DSD(DSD(rec) + 4u) = prev;
+    DSD(prev) = DSD(rec);
+    DSD(rec + 4u) = 0u; DSD(rec) = 0u;
+    res = (const u32 *)res_resolve(handle);
+    DSB(rec + 0x0Fu) = 0x80u; DSB(rec + 0x0Cu) = 3u; DSD(rec + 8u) = src;
+    DSB(rec + 0x0Du) = (u8)byte_arg;
+    if ((s32)DSD(src + 0x0Cu) > 0) {
+        for (s32 i = 0; i < (s32)DSD(src + 0x0Cu); i++) DSD(rec + 0x10u + (u32)i * 4u) = 0u;
+        if (res) for (s32 i = 0; i < (s32)DSD(src + 0x0Cu); i++)
+            DSD(rec + 0x410u + (u32)i * 4u) = res[1 + i];
+    }
+    DSB(DS_0009AF3C) = 1u;
+    effects_list_insert_after(DS_000FCCE0, rec);
+    DSB(DS_0009AF3D) = (u8)(DSB(DS_0009AF3D) + 1u);
+    DSB(DS_0009AF3C) = 0u;
+    *eax = rec;
+}
+static void m_13c70_count(const u32 *r, u32 *eax)      /* count 0 still runs one copy (>= for >) */
+{
+    u32 src = r[R_EAX], byte_arg = r[R_EDX], handle = r[R_EBX];
+    u32 rec = DSD(DS_000FCCE8), prev;
+    const u32 *res;
+    if (rec == 0u || rec == DS_000FCCE8) { *eax = 0u; return; }
+    prev = DSD(rec + 4u);
+    DSD(DSD(rec) + 4u) = prev;
+    DSD(prev) = DSD(rec);
+    DSD(rec + 4u) = 0u; DSD(rec) = 0u;
+    res = (const u32 *)res_resolve(handle);
+    DSB(rec + 0x0Fu) = 0x80u; DSB(rec + 0x0Cu) = 3u; DSD(rec + 8u) = src;
+    DSB(rec + 0x0Du) = (u8)byte_arg;
+    if ((s32)DSD(src + 0x0Cu) >= 0) {
+        for (s32 i = 0; i < (s32)DSD(src + 0x0Cu); i++) DSD(rec + 0x10u + (u32)i * 4u) = 0u;
+        if (res) for (s32 i = 0; i < (s32)DSD(src + 0x0Cu) + 1; i++)
+            DSD(rec + 0x410u + (u32)i * 4u) = res[1 + i];
+    }
+    DSB(DS_0009AF3C) = 1u;
+    DSB(rec + 0x0Eu) = 1u;
+    effects_list_insert_after(DS_000FCCE0, rec);
+    DSB(DS_0009AF3D) = (u8)(DSB(DS_0009AF3D) + 1u);
+    DSB(DS_0009AF3C) = 0u;
+    *eax = rec;
+}
+static void m_13c70_copy(const u32 *r, u32 *eax)       /* the copy starts at resolved[0] */
+{
+    u32 src = r[R_EAX], byte_arg = r[R_EDX], handle = r[R_EBX];
+    u32 rec = DSD(DS_000FCCE8), prev;
+    const u32 *res;
+    if (rec == 0u || rec == DS_000FCCE8) { *eax = 0u; return; }
+    prev = DSD(rec + 4u);
+    DSD(DSD(rec) + 4u) = prev;
+    DSD(prev) = DSD(rec);
+    DSD(rec + 4u) = 0u; DSD(rec) = 0u;
+    res = (const u32 *)res_resolve(handle);
+    DSB(rec + 0x0Fu) = 0x80u; DSB(rec + 0x0Cu) = 3u; DSD(rec + 8u) = src;
+    DSB(rec + 0x0Du) = (u8)byte_arg;
+    if ((s32)DSD(src + 0x0Cu) > 0) {
+        for (s32 i = 0; i < (s32)DSD(src + 0x0Cu); i++) DSD(rec + 0x10u + (u32)i * 4u) = 0u;
+        if (res) for (s32 i = 0; i < (s32)DSD(src + 0x0Cu); i++)
+            DSD(rec + 0x410u + (u32)i * 4u) = res[i];
+    }
+    DSB(DS_0009AF3C) = 1u;
+    DSB(rec + 0x0Eu) = 1u;
+    effects_list_insert_after(DS_000FCCE0, rec);
+    DSB(DS_0009AF3D) = (u8)(DSB(DS_0009AF3D) + 1u);
+    DSB(DS_0009AF3C) = 0u;
+    *eax = rec;
+}
+static void m_13c70_lock(const u32 *r, u32 *eax)       /* the saved lock byte is not restored */
+{
+    u32 src = r[R_EAX], byte_arg = r[R_EDX], handle = r[R_EBX];
+    u32 rec = DSD(DS_000FCCE8), prev;
+    const u32 *res;
+    if (rec == 0u || rec == DS_000FCCE8) { *eax = 0u; return; }
+    prev = DSD(rec + 4u);
+    DSD(DSD(rec) + 4u) = prev;
+    DSD(prev) = DSD(rec);
+    DSD(rec + 4u) = 0u; DSD(rec) = 0u;
+    res = (const u32 *)res_resolve(handle);
+    DSB(rec + 0x0Fu) = 0x80u; DSB(rec + 0x0Cu) = 3u; DSD(rec + 8u) = src;
+    DSB(rec + 0x0Du) = (u8)byte_arg;
+    if ((s32)DSD(src + 0x0Cu) > 0) {
+        for (s32 i = 0; i < (s32)DSD(src + 0x0Cu); i++) DSD(rec + 0x10u + (u32)i * 4u) = 0u;
+        if (res) for (s32 i = 0; i < (s32)DSD(src + 0x0Cu); i++)
+            DSD(rec + 0x410u + (u32)i * 4u) = res[1 + i];
+    }
+    DSB(DS_0009AF3C) = 1u;
+    DSB(rec + 0x0Eu) = 1u;
+    effects_list_insert_after(DS_000FCCE0, rec);
+    DSB(DS_0009AF3D) = (u8)(DSB(DS_0009AF3D) + 1u);
+    DSB(DS_0009AF3C) = 1u;
+    *eax = rec;
+}
+static void m_13c70_free(const u32 *r, u32 *eax)       /* the free head is not unlinked */
+{
+    u32 src = r[R_EAX], byte_arg = r[R_EDX], handle = r[R_EBX];
+    u32 rec = DSD(DS_000FCCE8);
+    const u32 *res;
+    if (rec == 0u || rec == DS_000FCCE8) { *eax = 0u; return; }
+    res = (const u32 *)res_resolve(handle);
+    DSB(rec + 0x0Fu) = 0x80u; DSB(rec + 0x0Cu) = 3u; DSD(rec + 8u) = src;
+    DSB(rec + 0x0Du) = (u8)byte_arg;
+    if ((s32)DSD(src + 0x0Cu) > 0) {
+        for (s32 i = 0; i < (s32)DSD(src + 0x0Cu); i++) DSD(rec + 0x10u + (u32)i * 4u) = 0u;
+        if (res) for (s32 i = 0; i < (s32)DSD(src + 0x0Cu); i++)
+            DSD(rec + 0x410u + (u32)i * 4u) = res[1 + i];
+    }
+    DSB(DS_0009AF3C) = 1u;
+    DSB(rec + 0x0Eu) = 1u;
+    effects_list_insert_after(DS_000FCCE0, rec);
+    DSB(DS_0009AF3D) = (u8)(DSB(DS_0009AF3D) + 1u);
+    DSB(DS_0009AF3C) = 0u;
+    *eax = rec;
+}
+
+/* 0x3B298's mutants share one re-implementation of fighter_command_dispatch with a mutation
+ * selector, so every mutant differs from the port in exactly one plausible way and calls the same
+ * helpers (the seams still record). */
+#define C2B3_NOCOPY 0x01u   /* the +0x86 copy is skipped */
+#define C2B3_COPY   0x02u   /* the copy writes one byte at +0x85 */
+#define C2B3_EARLY  0x04u   /* the early return takes either anim bit */
+#define C2B3_SCAN   0x08u   /* the ring filter ignores the mask */
+#define C2B3_B2     0x10u   /* b2 tests bit 0x8000 */
+#define C2B3_FORCE  0x20u   /* the forced-block test is dropped */
+#define C2B3_ARM    0x40u   /* the two +0x43 arms are swapped */
+#define C2B3_CMD    0x80u   /* the command-word block is dropped */
+
+static u32 c2b_3b298_core(const u32 *r, u32 mut)
+{
+    u32 side = r[R_EAX], edx_arg = r[R_EDX];
+    u32 ctx[6], anim[3];
+    fighter_ctx_swap(ctx, side);
+    fighter_anim_triple(anim, ctx[0], (s32)edx_arg);
+    fight_command_map(ctx[1], edx_arg, 0u);
+    if (mut & C2B3_NOCOPY) {
+        /* nothing */
+    } else if (mut & C2B3_COPY) {
+        DSB(ctx[3] + 0x85u) = DSB(ctx[2] + 0x84u);
+    } else {
+        DSW(ctx[3] + 0x86u) = DSW(ctx[2] + 0x84u);
+    }
+    u16 bits = DSW(anim[2] + 2u);
+    if (mut & C2B3_EARLY) {
+        if ((bits & 2u) != 0 || (bits & 1u) != 0) return 0u;
+    } else {
+        if ((bits & 2u) != 0 && (bits & 1u) != 0) return 0u;
+    }
+    u8 b1 = 0, b2 = 0;
+    u32 mask = fighter_input_mask(ctx[1]);
+    s32 count = (s32)DSD(DS_000BEEF2) >> 16;
+    for (s32 i = 0; i < count; i++) {
+        u16 v = (u16)fighter_input_read(ctx[1], i);
+        if (mut & C2B3_SCAN) {
+            if (v == 0u) continue;
+        } else if ((v & (u16)mask) == 0u) continue;
+        if (mut & C2B3_B2) {
+            if ((v & 0x8000u) != 0) b2 = 1u;
+            else if ((v & 0x8000u) == 0u) b1 = 1u;
+        } else {
+            if ((v & 0x4000u) != 0) b2 = 1u;
+            else if ((v & 0x8000u) == 0u) b1 = 1u;
+        }
+    }
+    if (!(mut & C2B3_CMD)) {
+        u16 cmd = DSW(DS_001088E0 + ctx[1] * 2u);
+        if ((cmd & (u16)mask) != 0u) {
+            if ((cmd & 0x4000u) != 0) b2 = 1u;
+            else if ((cmd & 0x8000u) == 0u) b1 = 1u;
+        }
+    }
+    if (!(mut & C2B3_FORCE)
+            && ((s32)DSD(DS_00100CDE + ctx[0] * 2u) >> 16) > 1
+            && (DSW(anim[2] + 2u) & 0x80u) != 0
+            && (DSB(ctx[3] + 0x43u) & 0x30u) != 0)
+        b1 = 1u;
+    if (b1 && (DSW(anim[2] + 2u) & 1u) == 0) {
+        if (mut & C2B3_ARM)
+            DSB(ctx[3] + 0x43u) = (u8)((DSB(ctx[3] + 0x43u) & 0xCFu) | 0x10u);
+        else
+            DSB(ctx[3] + 0x43u) = (u8)((DSB(ctx[3] + 0x43u) & 0xCFu) | 0x20u);
+    } else if (b2 && (DSW(anim[2] + 2u) & 2u) == 0) {
+        if (mut & C2B3_ARM)
+            DSB(ctx[3] + 0x43u) = (u8)((DSB(ctx[3] + 0x43u) & 0xCFu) | 0x20u);
+        else
+            DSB(ctx[3] + 0x43u) = (u8)((DSB(ctx[3] + 0x43u) & 0xCFu) | 0x10u);
+    } else {
+        return 0u;
+    }
+    fighter_block_hit(ctx[1]);
+    return 1u;
+}
+
+static void b_3b298(const u32 *r, u32 *eax)
+{
+    *eax = (u32)fighter_command_dispatch(r[R_EAX], r[R_EDX]);
+}
+static void m_3b298(const u32 *r, u32 *eax)      { *eax = c2b_3b298_core(r, C2B3_NOCOPY); }
+static void m_3b298_copy(const u32 *r, u32 *eax) { *eax = c2b_3b298_core(r, C2B3_COPY); }
+static void m_3b298_early(const u32 *r, u32 *eax) { *eax = c2b_3b298_core(r, C2B3_EARLY); }
+static void m_3b298_scan(const u32 *r, u32 *eax) { *eax = c2b_3b298_core(r, C2B3_SCAN); }
+static void m_3b298_b2(const u32 *r, u32 *eax)   { *eax = c2b_3b298_core(r, C2B3_B2); }
+static void m_3b298_force(const u32 *r, u32 *eax) { *eax = c2b_3b298_core(r, C2B3_FORCE); }
+static void m_3b298_arm(const u32 *r, u32 *eax)  { *eax = c2b_3b298_core(r, C2B3_ARM); }
+static void m_3b298_cmd(const u32 *r, u32 *eax)  { *eax = c2b_3b298_core(r, C2B3_CMD); }
+
+/* 0x3B714's mutants: one re-implementation with a mutation selector. */
+#define C2B7_COPY   0x01u   /* the 0x3B6C4 hold copies the other way */
+#define C2B7_HOLD   0x02u   /* the 0x3B080 guard tests == 2 */
+#define C2B7_SWAP   0x04u   /* the two slot arguments are swapped */
+#define C2B7_EARLY  0x08u   /* the anim gate tests bit 0x4000 */
+#define C2B7_EFC    0x10u   /* the 0x39EFC result is inverted */
+#define C2B7_BRANCH 0x20u   /* the dispatch branch is swapped */
+
+static void c2b_3b714_core(const u32 *r, u32 *eax, u32 mut)
+{
+    u32 param_1 = (mut & C2B7_SWAP) ? r[R_EDX] : r[R_EAX];
+    u32 param_2 = (mut & C2B7_SWAP) ? r[R_EAX] : r[R_EDX];
+    u32 ctx[6], anim[3];
+    u32 side = (u32)DSB(DSD(param_2) + 0x51u);
+    fighter_ctx_same(ctx, side);
+    if (fighter_pass_flag(1u, ctx[1]) != 0) { *eax = 0u; return; }
+    u32 local_24 = (u32)DSB(param_2 + 0x5Fu);
+    if (local_24 == 0xFFu) { *eax = 0u; return; }
+    fighter_anim_triple(anim, side, (s32)local_24);
+    u32 bit = (mut & C2B7_EARLY) ? 0x4000u : 0x800u;
+    if ((DSW(anim[2] + 2u) & bit) != 0u) { *eax = 0u; return; }
+    u32 efc = (u32)fighter_39efc(ctx[1]);
+    if (mut & C2B7_EFC) efc = efc ? 0u : 1u;
+    if (efc != 0 && (DSW(anim[2] + 2u) & 0x4000u) == 0u) { *eax = 0u; return; }
+    if (DSB(param_1 + 0x52u) == 4u) {
+        DSW(DSD(param_1) + 0x34u) = 0;
+        DSW(param_1 + 0x4Eu) = 0;
+    }
+    DSB(param_1 + 0x65u) = DSB(param_2 + 0x5Fu);
+    u8 local_18 = (u8)fighter_command_dispatch(ctx[1], local_24);
+    if ((mut & C2B7_HOLD) ? (DSB(param_1 + 0x54u) == 2u) : (DSB(param_1 + 0x54u) != 2u))
+        fighter_3b080(ctx[1], (u32)DSB(anim[0] + 3u), (u32)DSB(anim[0] + 2u), 1u);
+    if (DSB(ctx[2] + 0x52u) == 4u) fighter_3ae9c(side, local_18);
+    if (mut & C2B7_BRANCH) local_18 = (u8)(local_18 == 0u);
+    if (local_18 == 0u) {
+        u32 idx = (u32)DSB(ctx[5] + 0x4Bu);
+        if (idx != 0u) {
+            u32 row = DSD(DS_001014F4) + idx * 0x68u;
+            if (DSB(row + 0x60u) != 0u) fighter_2bd44(ctx[5], row);
+        }
+        fighter_reaction_apply(param_1, local_24);
+    } else {
+        DSB(ctx[2] + 0x8Au) = 0;
+        fighter_3ad98(ctx[1], anim);
+    }
+    if (fighter_3b6c4(side) != 0) {
+        if (mut & C2B7_COPY) DSW(ctx[4] + 0x34u) = DSW(ctx[5] + 0x34u);
+        else DSW(ctx[5] + 0x34u) = DSW(ctx[4] + 0x34u);
+    }
+    DSB(param_1 + 0x41u) |= 0x80u;
+    *eax = 0u;
+}
+
+static void b_3b714(const u32 *r, u32 *eax)      { fighter_reaction(r[R_EAX], r[R_EDX]); *eax = 0u; }
+static void m_3b714(const u32 *r, u32 *eax)      { c2b_3b714_core(r, eax, C2B7_COPY); }
+static void m_3b714_hold(const u32 *r, u32 *eax) { c2b_3b714_core(r, eax, C2B7_HOLD); }
+static void m_3b714_swap(const u32 *r, u32 *eax) { c2b_3b714_core(r, eax, C2B7_SWAP); }
+static void m_3b714_early(const u32 *r, u32 *eax) { c2b_3b714_core(r, eax, C2B7_EARLY); }
+static void m_3b714_efc(const u32 *r, u32 *eax)  { c2b_3b714_core(r, eax, C2B7_EFC); }
+static void m_3b714_branch(const u32 *r, u32 *eax) { c2b_3b714_core(r, eax, C2B7_BRANCH); }
+
+/* 0x39834's mutants: one re-implementation with a mutation selector. */
+#define C2B8_DIV  0x01u   /* the k > 0xB arm divides by 8 */
+#define C2B8_AI   0x02u   /* the r > 0 test is r >= 0 */
+#define C2B8_ARM  0x04u   /* the 0x1078F2 arm is inverted */
+#define C2B8_THR  0x08u   /* the 0x39973 threshold is 0x15 */
+#define C2B8_TAIL 0x10u   /* the 0x104B14 test is dropped */
+
+static void c2b_39834_core(const u32 *r, u32 *eax, u32 mut)
+{
+    u32 side = r[R_EAX];
+    s32 b = (s32)r[R_EDX];
+    u32 ctx[6], anim[3];
+    fighter_ctx_swap(ctx, side);
+    fighter_anim_triple(anim, ctx[0], b);
+    u32 edi = (u32)DSB(anim[0]);
+    u32 ebx = (u32)DSB(anim[0] + 1u);
+    s32 k = (s32)DSD(DS_00107D2A + ctx[0] * 2u) >> 16;
+    if (k > 0xB)
+        ebx = (mut & C2B8_DIV) ? ebx / 8u : ebx / 16u;
+    else
+        ebx = (u32)((s32)((u32)DSD(0x000BEBF8u + (u32)k * 4u) * ebx) / 100);
+    s32 rr = (s32)fighter_39738(ctx[0], (s32)edi);
+    fighter_392a0(ctx[3], rr, (s32)ebx);
+    if (ai_pred_468d8(ctx[1]) != 0
+            && ((mut & C2B8_AI) ? rr >= 0 : rr > 0))
+        fighter_36d98(ctx[3]);
+    else if ((u32)DSB(ctx[3] + 0x5Du) >= 0x44u) {
+        u32 f2 = (u32)DSB(DS_001078F2 + ((mut & C2B8_ARM) ? ctx[0] : ctx[1]));
+        if ((f2 != 0u) != ((mut & C2B8_ARM) != 0u)) {
+            DSB(ctx[3] + 0x5Du) = 0u;
+            DSB(ctx[3] + 0x43u) &= 0xFBu;
+        } else {
+            fighter_36ce4(ctx[3]);
+        }
+    }
+    DSW(DS_00107D2C + ctx[0] * 2u) = (u16)(DSW(DS_00107D2C + ctx[0] * 2u) + 1u);
+    DSW(DS_00107D20 + ctx[0] * 2u) = (u16)(DSW(DS_00107D20 + ctx[0] * 2u) + (u16)rr);
+    DSD(DS_00107D28) = (u32)b;
+    (void)sound_voice((u32)DSW(0x000E933Cu + (u32)DSB(anim[0] + 8u) * 2u));
+    if ((s32)DSD(DS_00107D2A + ctx[0] * 2u) >> 16 >= ((mut & C2B8_THR) ? 0x15 : 0x14))
+        DSW(0x00107824u + (u32)DSB(ctx[5] + 0x51u) * 0x94u) = 0x29Au;
+    if (DSD(DS_00104ABC) == 1u
+            && ((mut & C2B8_TAIL) || DSB(DS_00104B14) == 0u))
+        fighter_4f434();
+    *eax = 0u;
+}
+
+static void b_39834(const u32 *r, u32 *eax) { fighter_39834(r[R_EAX], (s32)r[R_EDX]); *eax = 0u; }
+static void m_39834(const u32 *r, u32 *eax) { c2b_39834_core(r, eax, C2B8_DIV); }
+static void m_39834_ai(const u32 *r, u32 *eax) { c2b_39834_core(r, eax, C2B8_AI); }
+static void m_39834_arm(const u32 *r, u32 *eax) { c2b_39834_core(r, eax, C2B8_ARM); }
+static void m_39834_thr(const u32 *r, u32 *eax) { c2b_39834_core(r, eax, C2B8_THR); }
+static void m_39834_tail(const u32 *r, u32 *eax) { c2b_39834_core(r, eax, C2B8_TAIL); }
+
+/* 0x36870's mutants: one re-implementation with a mutation selector. */
+#define C2B68_MODE  0x01u   /* the +0x385B0 mode test is 0x26 */
+#define C2B68_ARM   0x02u   /* the +0x36BC8 gate tests +0x43 bit 1 */
+#define C2B68_SO    0x04u   /* the So clears act on S */
+#define C2B68_CASE1 0x08u   /* case 1 uses the 0xC8950 table */
+#define C2B68_MASK  0x10u   /* the +0x40 reset mask is 0xCCF3BF00 */
+
+static void c2b_36870_core(const u32 *r, u32 *eax, u32 mut)
+{
+    u32 rec = r[R_EAX];
+    if (DSW(DS_00104B00) == ((mut & C2B68_MODE) ? 0x26u : 0x25u)) {
+        fighter_385b0(rec);
+        *eax = 0u;
+        return;
+    }
+    u32 side = (u32)DSB(rec + 0x51u);
+    u32 other = 1u - side;
+    u32 s = DS_001077B0 + side * 0x94u;
+    u32 so = DS_001077B0 + other * 0x94u;
+    u32 rec_s = DSD(s);
+    DSW(DS_00100CE0 + other * 2u) = 0;
+    DSB(s + 0x90u) = 0;
+    if ((DSB(s + 0x41u) & 4u) != 0u) {
+        DSD(s + 0x40u) &= 0xFBFFFBFFu;
+        fighter_state_39280(side);
+    }
+    DSD(DS_00100AF8 + side * 4u) = 0;
+    DSB(rec_s + 0x28u) &= 0xDFu;
+    DSB(s + 0x62u) = 0;
+    DSW(s + 0x84u) = (u16)(DSW(s + 0x84u) + 1u);
+    DSB(s + 0x8Au) = 0;
+    fighter_164e8(side);
+    DSD(s + 0x40u) &= (mut & C2B68_MASK) ? 0xCCF3BF00u : 0xCCF3BFFFu;
+    DSB(rec_s + 0x42u) = 0;
+    fighter_39040(other);
+    DSB(s + 0x5Fu) = 0xFFu;
+    DSB(s + 0x55u) = 0xFFu;
+    DSB(s + 0x67u) = 0;
+    DSB(s + 0x65u) = 0xFFu;
+    DSW(s + 0x74u) = 0;
+    DSD(s + 0x0Cu) = 0;
+    DSD(s + 0x10u) = 0;
+    DSD(s + 0x18u) = 0;
+    DSD(s + 0x1Cu) = 0;
+    u32 sot = (mut & C2B68_SO) ? s : so;
+    DSB(sot + 0x65u) = 0xFFu;
+    DSB(sot + 0x66u) = 0;
+    if (DSB(s + 0x54u) == 0u || DSB(s + 0x54u) == 1u) {
+        DSB(s + 0x68u) = 0;
+        DSW(rec_s + 0x44u) = 0;
+        DSB(rec_s + 0x43u) = 0;
+        DSB(rec_s + 0x42u) = 0;
+        DSW(rec_s + 0x34u) = 0;
+        DSW(rec_s + 0x36u) = 0;
+        DSD(rec_s + 0x1Cu) = 0;
+    }
+    switch (DSB(s + 0x54u)) {
+    case 0u:
+        DSW(s + 0x40u) &= 0x7F7Fu;
+        DSB(rec_s + 0x4Cu) = 0;
+        if ((DSB(s + 0x42u) & 0x20u) != 0u) {
+            fighter_37d18(s, rec_s);
+            break;
+        }
+        if (fighter_state_365c8(s, rec_s, side) != 0)
+            DSB(s + 0x43u) |= 0x40u;
+        else
+            DSB(s + 0x43u) &= 0xBFu;
+        if ((DSB(s + 0x42u) & 0x10u) == 0u
+                && (DSB(s + 0x43u) & ((mut & C2B68_ARM) ? 2u : 4u)) != 0u) {
+            (void)fighter_state_36bc8(s, rec_s);
+            break;
+        }
+        if (fighter_state_36638(s, rec_s) == 0) {
+            actors_anim_begin(rec_s, DSD(0x000C8950u + (u32)DSB(s + 0x7Au) * 4u), 0x40400000u);
+            DSB(rec_s + 0x4Du) = 0x1Eu;
+            DSB(s + 0x52u) = 0;
+            DSB(s + 0x53u) = 0;
+        }
+        if (DSW(DS_00104B00) == 3u || DSW(DS_00104B00) == 0x22u
+                || DSW(DS_00104B00) == 0x24u)
+            break;
+        actors_anim_begin(DSD(DS_00102900 + (u32)DSB(rec_s + 0x51u) * 4u), 0x000E906Au,
+                          0x3F800000u);
+        break;
+    case 1u:
+        DSB(s + 0x40u) &= 0x7Fu;
+        if (fighter_state_365c8(s, rec_s, side) != 0)
+            DSB(s + 0x43u) |= 0x40u;
+        else
+            DSB(s + 0x43u) &= 0xBFu;
+        DSB(s + 0x41u) &= 0x7Fu;
+        DSB(s + 0x68u) = 0;
+        if (fighter_state_36638(s, rec_s) != 0) break;
+        DSB(s + 0x52u) = 5u;
+        DSB(s + 0x53u) = 0;
+        DSB(rec_s + 0x4Du) = 0x14u;
+        DSB(rec_s + 0x4Cu) = 0;
+        actors_anim_begin(rec_s, DSD(((mut & C2B68_CASE1) ? 0x000C8950u : 0x000C89A0u)
+                                     + (u32)DSB(s + 0x7Au) * 4u), 0x40400000u);
+        break;
+    case 2u:
+        DSB(s + 0x52u) = 4u;
+        DSB(s + 0x53u) = 0;
+        hit_anim_start_c(rec_s, DSD(0x000C89F0u + (u32)DSB(s + 0x7Au) * 4u), 0x40000000u);
+        break;
+    case 3u:
+        break;
+    case 4u:
+        fighter_379c4(s);
+        break;
+    default:
+        break;
+    }
+    *eax = 0u;
+}
+
+static void b_36870(const u32 *r, u32 *eax) { fighter_36870(r[R_EAX]); *eax = 0u; }
+static void m_36870(const u32 *r, u32 *eax) { c2b_36870_core(r, eax, C2B68_MODE); }
+static void m_36870_arm(const u32 *r, u32 *eax) { c2b_36870_core(r, eax, C2B68_ARM); }
+static void m_36870_so(const u32 *r, u32 *eax) { c2b_36870_core(r, eax, C2B68_SO); }
+static void m_36870_case(const u32 *r, u32 *eax) { c2b_36870_core(r, eax, C2B68_CASE1); }
+static void m_36870_mask(const u32 *r, u32 *eax) { c2b_36870_core(r, eax, C2B68_MASK); }
+
+/* 0x2AE14's mutants: one re-implementation with a mutation selector. */
+#define C2B2A_WALK  0x01u   /* the status-2 mapping is dropped (always 0x2A408) */
+#define C2B2A_MINUS 0x02u   /* the stream cursor is not rewound by 2 */
+#define C2B2A_PAL   0x04u   /* the palette handle test is inverted */
+#define C2B2A_TYPE  0x08u   /* the invisible arm keeps the type byte */
+
+static u32 c2b_2ae14_core(const u32 *r, u32 *eax, u32 mut)
+{
+    const u8 *dp = (const u8 *)(mem + r[R_EAX]);
+    u32 a2 = r[R_EDX], a3 = r[R_ECX], a4 = r[R_EBX], a5 = r[R_S0];
+    u32 rec = actor_alloc(a5);
+    if (rec == 0) { *eax = 0u; return 0u; }
+    u32 index = (rec - DSD(DS_001014F4)) / ACTOR_REC_SIZE;
+    DSW(rec + 0x56) = (u16)index;
+    u32 pset = DSD(DS_001014EC) + (index & 0xffffu) * 0x20u;
+    DSD(rec + 0x08) = *(const u32 *)dp;
+    u8 frame = dp[5];
+    union { float f; u32 u; } fu;
+    fu.f = (float)frame;
+    DSD(rec + 0x20) = fu.u;
+    DSD(rec + 0x24) = fu.u;
+    if ((fu.u & 0x7fffffffu) != 0) {
+        fu.f = fu.f - 1.0f;
+        DSD(rec + 0x20) = fu.u;
+    }
+    DSB(rec + 0x48) = dp[4];
+    DSW(rec + 0x2a) = 0;
+    DSB(rec + 0x4a) = (u8)a5 & 0x7fu;
+    DSW(rec + 0x2c) = *(const u16 *)(dp + 0x0c);
+    s16 extent = *(const s16 *)(dp + 0x0a);
+    DSB(rec + 0x51) = 0; DSB(rec + 0x55) = 0; DSB(rec + 0x4b) = 0;
+    DSW(rec + 0x38) = 0; DSB(rec + 0x4f) = 0; DSB(rec + 0x59) = 0; DSB(rec + 0x4e) = 0;
+    DSB(rec + 0x43) = 0; DSW(rec + 0x44) = 0; DSB(rec + 0x60) = 0; DSB(rec + 0x61) = 0;
+    DSW(rec + 0x40) = (u16)(extent * 64);
+    DSB(rec + 0x50) = DSB(rec + 0x51);
+    DSB(rec + 0x54) = DSB(rec + 0x55);
+    DSB(rec + 0x53) = DSB(rec + 0x55);
+    DSB(rec + 0x52) = DSB(rec + 0x55);
+    DSW(rec + 0x36) = DSW(rec + 0x38);
+    DSW(rec + 0x34) = DSW(rec + 0x38);
+    DSW(rec + 0x2e) = *(const u16 *)(dp + 0x06);
+    DSB(rec + 0x5a) = (u8)(a5 >> 0x10);
+    DSB(rec + 0x5f) = 1;
+    u8 layer = (u8)a3;
+    if ((a5 & 0x400u) == 0) {
+        DSD(rec + 0x18) = a2; DSD(rec + 0x1c) = a4;
+        DSW(rec + 0x28) = (u16)((*(const u16 *)(dp + 0x08) & 0xffc3u)
+                                | (((a5 >> 8) & 0x44u) << 8));
+        DSB(rec + 0x2b) |= 0x80u;
+        if ((DSW(rec + 0x28) >> 8 & 0x20) != 0)
+            DSB(rec + 0x49) = layer;
+        else
+            DSW(rec + 0x32) = (u16)a3;
+    } else {
+        u32 parent = (u32)DSB(rec + 0x4a) * ACTOR_REC_SIZE + DSD(DS_001014F4);
+        DSW(rec + 0x34) = (u16)a2; DSW(rec + 0x36) = (u16)a4;
+        DSB(rec + 0x5a) = DSB(parent + 0x5a);
+        DSB(parent + 0x4f) = (u8)(DSB(parent + 0x4f) + 1);
+        DSW(rec + 0x28) = (u16)((*(const u16 *)(dp + 0x08) & 0xffc3u)
+              | ((((a5 >> 8) & 0x40u) ^ ((DSW(parent + 0x28) >> 8) & 0x40u)) << 8)
+              | 0x400u);
+        layer = a3 != 0 ? (u8)a3 : DSB(parent + 0x49);
+        DSB(rec + 0x49) = layer;
+    }
+    u32 id = 0;
+    int have_id = 0;
+    if ((DSW(rec + 0x28) >> 8 & 8) == 0) {
+        if (!(mut & C2B2A_MINUS)) DSD(rec + 0x08) = DSD(rec + 0x08) - 2;
+        u32 status = 0;
+        do {
+            u32 p = DSD(rec + 0x08) + 2;
+            DSD(rec + 0x08) = p;
+            if ((DSW(p) >> 8 & 0x80) == 0) break;
+            status = spawn_anim_opcode(rec, index, 1);
+        } while (status == 0);
+        if (status == 2 && !(mut & C2B2A_WALK)) { id = 0x1e1u; have_id = 1; }
+    }
+    if (!have_id) id = anim_next_sprite_id(rec, pset);
+    DSW(pset + 0x00) = (u16)id;
+    DSW(pset + 0x02) = (u16)(DSW(rec + 0x2e) | (DSB(rec + 0x5f) != 0 ? 0x800u : 0u));
+    u32 hdl = *(const u32 *)(dp + 0x10);
+    if ((mut & C2B2A_PAL) ? (hdl == 0u) : (hdl != 0u))
+        DSD(pset + 0x18) = palette_acquire(hdl);
+    else
+        DSD(pset + 0x18) = 0u;
+    DSB(rec + 0x2b) |= 0x20u;
+    pset_write(rec, pset);
+    DSB(rec + 0x2b) &= 0xdfu;
+    if ((DSW(rec + 0x28) >> 8 & 0x10) != 0) {
+        DSD(pset + 0x14) = DSD(pset + 0x08);
+        mode1_cursor(rec, pset);
+    }
+    u32 cb = DSD(DS_000BB9DC + (u32)DSB(rec + 0x48) * 0xCu);
+    typedef u8 (*c2b_cb1)(u32, u32);
+    c2b_cb1 cb1 = (c2b_cb1)(void *)fn_resolve(cb);
+    u8 visible;
+    if (cb1 != NULL)
+        visible = cb1(rec, index) == 0;
+    else
+        visible = cb == FN_0005D812;
+    if (!visible) {
+        if (!(mut & C2B2A_TYPE)) DSB(rec + 0x48) = 0;
+        DSW(rec + 0x28) |= 8;
+        *eax = 0u;
+        return 0u;
+    }
+    DSB(rec + 0x2b) |= 0x40u;
+    if ((a5 & 0x400u) == 0) DSB(rec + 0x4a) = 0;
+    render_list_insert(pset);
+    *eax = rec;
+    return rec;
+}
+
+static void b_2ae14(const u32 *r, u32 *eax)
+{
+    *eax = actor_spawn((const u32 *)(mem + r[R_EAX]), r[R_EDX], r[R_ECX], r[R_EBX], r[R_S0]);
+}
+static void m_2ae14(const u32 *r, u32 *eax) { *eax = c2b_2ae14_core(r, eax, C2B2A_WALK); }
+static void m_2ae14_minus(const u32 *r, u32 *eax) { *eax = c2b_2ae14_core(r, eax, C2B2A_MINUS); }
+static void m_2ae14_pal(const u32 *r, u32 *eax) { *eax = c2b_2ae14_core(r, eax, C2B2A_PAL); }
+static void m_2ae14_type(const u32 *r, u32 *eax) { *eax = c2b_2ae14_core(r, eax, C2B2A_TYPE); }
+
+/* 0x2C3FC's mutants: one re-implementation with a mutation selector. */
+#define C2BVC_REMAP 0x01u   /* the 0x100 remap is dropped */
+#define C2BVC_QUEUE 0x02u   /* the type-2 queue loop flag is 0 */
+#define C2BVC_CASE5 0x04u   /* the 0x3F sub-id stops 0x1800EBC8 */
+#define C2BVC_PLAY  0x08u   /* the type-2 playing test is inverted */
+
+static u32 c2b_voice_core(const u32 *r, u32 mut)
+{
+    u32 id = r[R_EAX];
+    if (id == 0u) return 0u;
+    if (id == 0x100u && !(mut & C2BVC_REMAP)) id = 0u;
+    u32 rec = DS_000BBDC8 + id * 0x0Cu;
+    u32 h = DSD(rec + 4u);
+    u32 b = DSB(rec + 8u);
+    switch (DSB(rec)) {
+    case 0:
+        return 1u;
+    case 1:
+        DSD(DS_00105D5C) = h;
+        (void)snd_music_request(DSD(DS_00105D5C), b);
+        return 1u;
+    case 2:
+        if ((snd_sample_playing(h) != 0u) != ((mut & C2BVC_PLAY) != 0u)) return 0u;
+        snd_sample_queue(h, (mut & C2BVC_QUEUE) ? 0u : b);
+        return 1u;
+    case 3:
+        if (id == 0x46u) {
+            if (snd_sample_playing(0x2886158u) != 0u) return 0u;
+            snd_sample_queue(0x28847C9u, 0);
+            snd_sample_queue(0x2886158u, 0);
+            return 1u;
+        }
+        if (id == 0x4Du) {
+            if (snd_sample_playing(0x1201D606u) != 0u) return 0u;
+            snd_sample_queue(0x1201D606u, 0);
+            snd_sample_queue(0x2001513Cu, 0);
+            return 1u;
+        }
+        if (id == 0x5Du) {
+            if (snd_sample_playing(0x281A726u) != 0u) return 0u;
+            snd_sample_queue(0x281A726u, 0);
+            snd_sample_queue(0x2819183u, 0);
+            return 1u;
+        }
+        return 0u;
+    case 4:
+        snd_music_unpause();
+        snd_sample_unpause();
+        DSD(DS_00105D5C) = 0x21u;
+        (void)snd_music_request(0x2803E64u, 0);
+        if (snd_sample_playing(0x180122FDu) == 0u) {
+            (void)snd_samples_stop_all();
+            snd_sample_queue(0x180122FDu, 1);
+        }
+        return 1u;
+    case 5: {
+        u32 cur = DSD(DS_00105D5C);
+        switch (id) {
+        case 0x00: (void)snd_music_stop(); (void)snd_samples_stop_all(); break;
+        case 0x22:
+            if ((cur >= 0x1Bu && cur <= 0x21u) || cur == 0x25u || cur == 0x26u)
+                (void)snd_music_stop();
+            break;
+        case 0x2B: if (cur == 0x2Au) (void)snd_music_stop(); break;
+        case 0x2D: if (cur == 0x2Cu) (void)snd_music_stop(); break;
+        case 0x2F:
+            if (cur == 0x2Eu || cur == 0x30u) (void)snd_music_stop();
+            break;
+        case 0x33: if (cur == 0x32u) (void)snd_music_stop(); break;
+        case 0x3C: if (cur == 0x3Bu) (void)snd_music_stop(); break;
+        case 0x3F: (void)snd_sample_stop((mut & C2BVC_CASE5) ? 0x1800EBC8u : 0x1800EBC9u); break;
+        case 0x41: (void)snd_sample_stop(0x383B6F4u); break;
+        case 0x43: (void)snd_sample_stop(0x3837440u); break;
+        case 0x4C: (void)snd_sample_stop(0x22008696u); break;
+        case 0x4F: (void)snd_sample_stop(0x1501053Cu); break;
+        case 0x55: if (cur == 0x54u) (void)snd_music_stop(); break;
+        case 0x57: if (cur == 0x56u) (void)snd_music_stop(); break;
+        case 0x5B: (void)snd_sample_stop(0x1B01AF00u); break;
+        case 0xE0: if (cur == 0xDFu) (void)snd_music_stop(); break;
+        case 0xE2:
+            if (cur == 0xE1u || cur == 0xE3u) (void)snd_music_stop();
+            break;
+        case 0xF1: (void)snd_sample_stop(0x22018405u); break;
+        default: break;
+        }
+        return 1u;
+    }
+    default:
+        return 0u;
+    }
+}
+
+static void b_2c3fc(const u32 *r, u32 *eax) { *eax = sound_voice(r[R_EAX]); }
+static void m_2c3fc(const u32 *r, u32 *eax) { *eax = c2b_voice_core(r, C2BVC_REMAP); }
+static void m_2c3fc_queue(const u32 *r, u32 *eax) { *eax = c2b_voice_core(r, C2BVC_QUEUE); }
+static void m_2c3fc_case5(const u32 *r, u32 *eax) { *eax = c2b_voice_core(r, C2BVC_CASE5); }
+static void m_2c3fc_play(const u32 *r, u32 *eax) { *eax = c2b_voice_core(r, C2BVC_PLAY); }
+
+/* 0x2B2A0's mutants: one re-implementation with a mutation selector. */
+#define C2BOP_GE     0x01u   /* opcode 0x06 compares n1 > cx */
+#define C2BOP_RET2   0x02u   /* opcode 0x15 returns 0, not 2 */
+#define C2BOP_CHILD  0x04u   /* opcode 0x0C swaps the a5 arms */
+#define C2BOP_SKIP   0x08u   /* opcode 0x2D compares <= */
+
+#define C2BOP_INDEX  1u      /* the caller's EDX: the pset index the cases seed */
+
+static u32 c2b_op_core(const u32 *r, u32 mut)
+{
+    u32 rec = r[R_EAX];
+    u32 index = r[R_EDX];
+    u32 flag = r[R_EBX];
+    u32 p = DSD(rec + 8);
+    DSW(DS_00105BE4) = (u16)((DSW(p) >> 8) & 0x1fu);
+    if ((u8)DSW(DS_00105BE4) == 0x0du) return 1u;
+    u32 value = anim_operand(rec);
+    u8 op = (u8)DSW(DS_00105BE4);
+    u16 ax = (u16)value;
+    union { float f; u32 u; } fu;
+
+    switch (op) {
+    case 0x00:
+        (void)flag;
+        actor_set_dead(rec);
+        /* fallthrough */
+    case 0x01:
+        DSD(rec + 0x24) = 0;
+        DSD(rec + 0x20) = DSD(rec + 0x24);
+        return 2;
+    case 0x02:
+        DSB(rec + 0x61) = (u8)value;
+        return 0;
+    case 0x03:
+        DSD(rec + 8) = DSD(DS_00105BD4) - 2u;
+        return 0;
+    case 0x04: {
+        u8 c = (u8)(DSB(rec + 0x50) + 1u);
+        DSB(rec + 0x50) = c;
+        if ((u32)c <= (u32)DSW(DS_00105BE8))
+            DSD(rec + 8) = DSD(DS_00105BD4) - 2u;
+        else
+            DSB(rec + 0x50) = 0;
+        return 0;
+    }
+    case 0x05:
+        if (ax != 0) return 0;
+        DSD(rec + 0x20) = 0;
+        DSB(rec + 0x28) |= 0x10;
+        return 2;
+    case 0x06: {
+        u32 edx = DSD(rec + 8) + 2u;
+        DSD(rec + 8) = edx;
+        u16 n1 = (u16)((u32)DSW(edx) + 1u);
+        u16 cx = ax;
+        if (cx != 0 && ((mut & C2BOP_GE) ? n1 > cx : n1 >= cx)) {
+            u32 t = edx + ((u32)cx * 4u - 2u);
+            DSD(rec + 8) = t;
+            DSD(rec + 8) = DSD(t) - 2u;
+        } else {
+            DSD(rec + 8) = edx + ((u32)n1 * 4u);
+        }
+        return 0;
+    }
+    case 0x07:
+        fu.f = (float)(u32)ax;
+        DSD(rec + 0x20) = fu.u;
+        return 2;
+    case 0x08:
+        fu.f = (float)(u32)rng_next(ax);
+        DSD(rec + 0x20) = fu.u;
+        return 2;
+    case 0x09:
+        return 0;
+    case 0x0a:
+        fu.f = (float)(s32)(s16)ax;
+        { union { float f; u32 u; } b; b.u = DSD(rec + 0x24); fu.f += b.f; }
+        DSD(rec + 0x24) = fu.u;
+        return 0;
+    case 0x0b:
+        fu.f = (float)(u32)ax;
+        DSD(rec + 0x24) = fu.u;
+        return 0;
+    case 0x0c: {
+        u16 var = DSW(DS_00105BE8);
+        u32 a5 = 0;
+        if (var > 0) a5 = (var == 1) ? 0x400u : 0u;
+        u32 e = DSD(rec + 8);
+        u32 w0 = e + 2;
+        e += 4;
+        DSD(rec + 8) = e;
+        s16 di = (s16)DSW(w0);
+        s16 bx = (s16)DSW(e);
+        u32 a2, a3, a4;
+        int use400 = (mut & C2BOP_CHILD) ? (a5 != 0x400u) : (a5 == 0x400u);
+        if (use400) {
+            a2 = (u32)(s32)di;
+            a3 = 0;
+            a4 = (u32)(s32)bx;
+            a5 = (u32)(index + 0x400u);
+        } else {
+            a5 = (u32)(DSW(rec + 0x28) & 0x4000u);
+            a2 = (u32)((s32)di + (s32)DSD(rec + 0x18));
+            a3 = (u32)((s32)DSD(rec + 0x30) >> 16);
+            a4 = (u32)((s32)bx + (s32)DSD(rec + 0x1c));
+        }
+        DSD(DS_00105BD8) = rec;
+        u32 child = actor_spawn((const u32 *)(mem + DSD(DS_00105BD4)), a2, a3, a4, a5);
+        if (child != 0) {
+            DSW(child + 0x2a) |= (u16)((DSW(DS_000EF6DC) & 1u) | 4u);
+            DSB(child + 0x51) = DSB(rec + 0x51);
+        }
+        return 0;
+    }
+    case 0x0d:
+    case 0x0e:
+        anim_write_var(rec, (u8)ax, 0);
+        return 0;
+    case 0x0f: {
+        if (DSW(DS_00105BE6) == 0) DSD(rec + 8) += 2u;
+        u32 v = DSW(DSD(rec + 8));
+        anim_write_var(rec, (u8)ax, v);
+        return 0;
+    }
+    case 0x10:
+        {
+            typedef void (*anim_code_fn)(u32, u32);
+            anim_code_fn fn = (anim_code_fn)(void *)fn_resolve(DSD(DS_00105BD4));
+            if (fn) fn(rec, index);
+        }
+        return 0;
+    case 0x11: {
+        u32 e = DSD(rec + 8) + 2u;
+        DSD(rec + 8) = e;
+        typedef void (*anim_code_fn)(u32, u32);
+        anim_code_fn fn = (anim_code_fn)(void *)fn_resolve(DSD(DS_00105BD4));
+        if (fn) fn(rec, DSW(e));
+        return 0;
+    }
+    case 0x12:
+        DSB(rec + 0x4e) = 1;
+        DSW(rec + 0x2e) = (u16)(value << 4);
+        return 0;
+    case 0x13:
+        DSB(rec + 0x59) = (u8)value;
+        return 0;
+    case 0x14:
+        DSB(rec + 0x29) ^= 0x40;
+        return 0;
+    case 0x15: {
+        typedef void (*anim_code_fn)(u32, u32);
+        anim_code_fn fn = (anim_code_fn)(void *)fn_resolve(DSD(DS_00105BD4));
+        if (fn) fn(rec, index);
+        return (mut & C2BOP_RET2) ? 0u : 2u;
+    }
+    case 0x16:
+        DSB(rec + 0x29) &= (u8)~0x02u;
+        return 0;
+    case 0x17:
+        anim_write_var(rec, (u8)DSW(DS_00105BE8), (u16)(value + 1u));
+        return 0;
+    case 0x18: {
+        u16 cx = (u16)(value + 1u);
+        anim_write_var(rec, (u8)DSW(DS_00105BE8), cx);
+        u32 edi = DSD(rec + 8) + 2u;
+        DSD(rec + 8) = edi;
+        u16 d = DSW(edi);
+        if ((u32)d > (u32)cx) {
+            u32 a = edi + 2u;
+            DSD(rec + 8) = a;
+            DSD(rec + 8) = DSD(a) - 2u;
+        } else {
+            DSD(rec + 8) = edi + 4u;
+        }
+        return 0;
+    }
+    case 0x19: {
+        u16 cx = (u16)(value - 1u);
+        anim_write_var(rec, (u8)DSW(DS_00105BE8), cx);
+        u32 ebx = DSD(rec + 8) + 2u;
+        DSD(rec + 8) = ebx;
+        u16 d = DSW(ebx);
+        if ((s32)(s16)cx < (s32)d) {
+            DSD(rec + 8) = ebx + 4u;
+        } else {
+            u32 a = ebx + 2u;
+            DSD(rec + 8) = a;
+            DSD(rec + 8) = DSD(a) - 2u;
+        }
+        return 0;
+    }
+    case 0x1a:
+        DSD(rec + 0x0c) = DSD(DS_00105BD4);
+        return 0;
+    case 0x1b:
+        DSD(rec + 0x0c) = 0;
+        return 0;
+    case 0x1c: {
+        DSD(rec + 0x10) = DSD(DS_00105BD4);
+        u32 pb = DSD(rec + 0x10) + (u32)((s32)DSD(rec + 0x4f) >> 24);
+        fu.f = (float)(u32)DSB(pb);
+        DSD(rec + 0x20) = fu.u;
+        DSD(rec + 0x24) = fu.u;
+        DSB(rec + 0x2b) |= 0x04;
+        return 0;
+    }
+    case 0x1d:
+        DSD(rec + 0x10) = 0;
+        return 0;
+    case 0x1e:
+        DSB(rec + 0x2b) &= (u8)~0x04u;
+        return 0;
+    case 0x1f:
+    case 0x20:
+        if ((DSW(rec + 0x28) >> 8 & 0x40u) != 0)
+            DSD(rec + 0x18) = (u32)((s32)DSD(rec + 0x18) - (s32)(s16)ax * 64);
+        else
+            DSD(rec + 0x18) = (u32)((s32)DSD(rec + 0x18) + (s32)(s16)ax * 64);
+        return 0;
+    case 0x21:
+        DSD(rec + 0x1c) = (u32)((s32)DSD(rec + 0x1c) + (s32)(s16)ax * 64);
+        return 0;
+    case 0x22:
+        DSW(rec + 0x32) = (u16)(DSW(rec + 0x32) + (u16)((value << 6) & 0xffffu));
+        return 0;
+    case 0x25:
+        DSW(rec + 0x34) = (u16)(DSW(rec + 0x34) + ax);
+        return 0;
+    case 0x26:
+        DSW(rec + 0x36) = (u16)(DSW(rec + 0x36) + ax);
+        return 0;
+    case 0x27:
+        DSW(rec + 0x38) = (u16)(DSW(rec + 0x38) + ax);
+        return 0;
+    case 0x28:
+        if ((DSW(rec + 0x28) >> 8 & 0x40u) != 0)
+            DSW(rec + 0x34) = (u16)(-value);
+        else
+            DSW(rec + 0x34) = ax;
+        return 0;
+    case 0x29: DSW(rec + 0x36) = ax; return 0;
+    case 0x2a: DSW(rec + 0x38) = ax; return 0;
+    case 0x2b: DSW(rec + 0x2c) = ax; return 0;
+    case 0x2c:
+        DSW(rec + 0x2c) = (u16)(DSW(rec + 0x2c) + ax);
+        return 0;
+    case 0x2d: {
+        s32 pv = (s32)(s16)DSW(DSD(DS_001014EC) + index * 0x20u + 0x0c);
+        if ((mut & C2BOP_SKIP) ? pv <= (s32)ax : pv < (s32)ax) {
+            DSD(rec + 8) += 4u;
+        } else {
+            DSD(rec + 8) += 2u;
+            DSD(rec + 8) = DSD(DSD(rec + 8)) - 2u;
+        }
+        return 0;
+    }
+    case 0x2e:
+        (void)sound_voice((u32)ax & 0xFFFFu);
+        return 0;
+    default:
+        return 0;
+    }
+}
+
+static void b_2b2a0(const u32 *r, u32 *eax)
+{
+    *eax = spawn_anim_opcode(r[R_EAX], r[R_EDX], r[R_EBX]);
+}
+static void m_2b2a0(const u32 *r, u32 *eax) { *eax = c2b_op_core(r, C2BOP_GE); }
+static void m_2b2a0_indirect(const u32 *r, u32 *eax) { *eax = c2b_op_core(r, C2BOP_RET2); }
+static void m_2b2a0_child(const u32 *r, u32 *eax) { *eax = c2b_op_core(r, C2BOP_CHILD); }
+static void m_2b2a0_skip(const u32 *r, u32 *eax) { *eax = c2b_op_core(r, C2BOP_SKIP); }
+
 static const binding_t k_bindings[] = {
     { "rng_next",                 b_rng_next,     0xFFFFFFFFu },
     { "fighter_slot_flag",        b_slot_flag,    0x000000FFu },
@@ -8423,6 +9465,62 @@ static const binding_t k_bindings[] = {
     { "fighter_4b03c@tear",       m_4b03c_tear,   0x00000000u },
     { "fighter_4b03c@order",      m_4b03c_order,  0x00000000u },
     { "fighter_4b03c@voice",      m_4b03c_voice,  0x00000000u },
+    { "palette_acquire",          b_33754,        0xFFFFFFFFu },
+    { "palette_acquire@mutant",   m_33754,        0xFFFFFFFFu },
+    { "palette_acquire@new",      m_33754_new,    0xFFFFFFFFu },
+    { "palette_acquire@inc",      m_33754_inc,    0xFFFFFFFFu },
+    { "palette_acquire@start",    m_33754_start,  0xFFFFFFFFu },
+    { "palette_acquire@reflow",   m_33754_reflow, 0xFFFFFFFFu },
+    { "palette_acquire@count",    m_33754_count,  0xFFFFFFFFu },
+    { "effects_spawn",            b_13c70,        0x00000000u },
+    { "effects_spawn@mutant",     m_13c70,        0x00000000u },
+    { "effects_spawn@count",      m_13c70_count,  0x00000000u },
+    { "effects_spawn@copy",       m_13c70_copy,   0x00000000u },
+    { "effects_spawn@lock",       m_13c70_lock,   0x00000000u },
+    { "effects_spawn@free",       m_13c70_free,   0x00000000u },
+    { "fighter_command_dispatch",          b_3b298,        0x000000FFu },
+    { "fighter_command_dispatch@mutant",   m_3b298,        0x000000FFu },
+    { "fighter_command_dispatch@copy",     m_3b298_copy,   0x000000FFu },
+    { "fighter_command_dispatch@early",    m_3b298_early,  0x000000FFu },
+    { "fighter_command_dispatch@scan",     m_3b298_scan,   0x000000FFu },
+    { "fighter_command_dispatch@b2",       m_3b298_b2,     0x000000FFu },
+    { "fighter_command_dispatch@force",    m_3b298_force,  0x000000FFu },
+    { "fighter_command_dispatch@arm",      m_3b298_arm,    0x000000FFu },
+    { "fighter_command_dispatch@cmd",      m_3b298_cmd,    0x000000FFu },
+    { "fighter_reaction",                  b_3b714,        0x00000000u },
+    { "fighter_reaction@mutant",           m_3b714,        0x00000000u },
+    { "fighter_reaction@hold",             m_3b714_hold,   0x00000000u },
+    { "fighter_reaction@swap",             m_3b714_swap,   0x00000000u },
+    { "fighter_reaction@early",            m_3b714_early,  0x00000000u },
+    { "fighter_reaction@efc",              m_3b714_efc,    0x00000000u },
+    { "fighter_reaction@branch",           m_3b714_branch, 0x00000000u },
+    { "fighter_39834",                     b_39834,        0x00000000u },
+    { "fighter_39834@mutant",              m_39834,        0x00000000u },
+    { "fighter_39834@ai",                  m_39834_ai,     0x00000000u },
+    { "fighter_39834@arm",                 m_39834_arm,    0x00000000u },
+    { "fighter_39834@thr",                 m_39834_thr,    0x00000000u },
+    { "fighter_39834@tail",                m_39834_tail,   0x00000000u },
+    { "fighter_36870",                     b_36870,        0x00000000u },
+    { "fighter_36870@mutant",              m_36870,        0x00000000u },
+    { "fighter_36870@arm",                 m_36870_arm,    0x00000000u },
+    { "fighter_36870@so",                  m_36870_so,     0x00000000u },
+    { "fighter_36870@case",                m_36870_case,   0x00000000u },
+    { "fighter_36870@mask",                m_36870_mask,   0x00000000u },
+    { "actor_spawn",                       b_2ae14,        0xFFFFFFFFu },
+    { "actor_spawn@mutant",                m_2ae14,        0xFFFFFFFFu },
+    { "actor_spawn@minus",                 m_2ae14_minus,  0xFFFFFFFFu },
+    { "actor_spawn@pal",                   m_2ae14_pal,    0xFFFFFFFFu },
+    { "actor_spawn@type",                  m_2ae14_type,   0xFFFFFFFFu },
+    { "sound_voice",                       b_2c3fc,        0x000000FFu },
+    { "sound_voice@mutant",                m_2c3fc,        0x000000FFu },
+    { "sound_voice@queue",                 m_2c3fc_queue,  0x000000FFu },
+    { "sound_voice@case5",                 m_2c3fc_case5,  0x000000FFu },
+    { "sound_voice@stop",                  m_2c3fc_play,   0x000000FFu },
+    { "spawn_anim_opcode",                 b_2b2a0,        0x000000FFu },
+    { "spawn_anim_opcode@mutant",          m_2b2a0,        0x000000FFu },
+    { "spawn_anim_opcode@indirect",        m_2b2a0_indirect, 0x000000FFu },
+    { "spawn_anim_opcode@child",           m_2b2a0_child,  0x000000FFu },
+    { "spawn_anim_opcode@skip",            m_2b2a0_skip,   0x000000FFu },
 };
 
 static const binding_t *find_binding(const char *name)
diff --git a/tools/diff_emu.py b/tools/diff_emu.py
index c41d106..14fd7bf 100644
--- a/tools/diff_emu.py
+++ b/tools/diff_emu.py
@@ -627,7 +627,14 @@ def callee_clobbers(image, addr, resolved=None):
             c -= saved
             if c != clob[f]:
                 clob[f], moved = c, True
-    return tuple(r for r in REGS if r != "eax" and r in clob[addr])
+    result = tuple(r for r in REGS if r != "eax" and r in clob[addr])
+    # Hand corrections where the transitive scan over-approximates the Watcom callee-saved registers
+    # (record §C2b.3): the callers of 0x2B150, 0x2BD44 and 0x3B298 keep ESI/EDI/EBP live across the
+    # call (0x2B30D reads ESI after 0x2B150; 0x3B877/0x3B8C8 read ESI after 0x2BD44; 0x3B834 reads
+    # EDI after 0x3B298), so those callees preserve them.
+    return _CALLEE_CLOBBER_FIXES.get(addr, result)
+
+_CALLEE_CLOBBER_FIXES = {0x2B150: (), 0x2BD44: ("edx",), 0x3B298: ("edx",)}
 
 
 # ---- decoding helpers for the static tools (E2 tools/entry_triage.py); additive, used by nothing above --
diff --git a/tools/diff_verify.py b/tools/diff_verify.py
index b89e178..d9038b0 100644
--- a/tools/diff_verify.py
+++ b/tools/diff_verify.py
@@ -998,7 +998,7 @@ P3_SPECS = [
 # The callees 0x47688 stubs (record §P3.4), args from their bytes, clobbers from E.callee_clobbers: 0x3B298 EAX =
 # side, EDX = a byte (`mov ecx,edx; ...; mov edx,eax`; it returns AL); 0x39FB0 EAX = slot (pushes EBX/ECX/EDX);
 # 0x3A95C EAX = side, EDX = a byte. All plain `ret`.
-DISPATCH = E.Call(0x3B298, ("eax", "edx"), clobbers=("edx", "edi", "ebp"))
+DISPATCH = E.Call(0x3B298, ("eax", "edx"), clobbers=("edx",))
 PIVOT = E.Call(0x39FB0, ("eax",))
 STANCE = E.Call(0x3A95C, ("eax", "edx"), clobbers=("edx",))
 
@@ -2618,7 +2618,7 @@ P7_2BDB8_R = E.Call(0x2BDB8, ("eax", "edx"), mode="real")
 P7_2BDE8_R = E.Call(0x2BDE8, ("eax",), mode="real")
 P7_23960 = E.Call(0x23960, ("eax",))
 P7_3A9D8 = E.Call(0x3A9D8, ("eax", "edx"), clobbers=("edx",))
-P7_2B150 = E.Call(0x2B150, ("eax",), clobbers=("esi", "edi", "ebp"))
+P7_2B150 = E.Call(0x2B150, ("eax",))
 P7_41310 = E.Call(0x41310, ("eax", "edx"))
 P7_49444 = E.Call(0x49444, ("eax",), clobbers=("edi", "ebp"))
 P7_RNG   = E.Call(0x5D7DC, ("eax",), mode="real")
@@ -3293,7 +3293,7 @@ C2_SPECS += [
              C2_REC + 0x28: b"\x28\x04", C2_REC + 0x68 + 0x2A: b"\xff", C2_PSET + 4: le32(0x04040404),
              C2_PSET + 8: le32(0x08080808), C2_PSET + 0x0C: b"\x0c\x0c\x0e\x0e",
              0x105B3C: le32(0x105B3C), 0xBCD60: b"\x00"}),
-    ], allow_calls=(0x2EA30,), calls=(E.Call(0x2B150, ("eax",), clobbers=("esi", "edi", "ebp")), E.Call(0x249D0, ("eax",)),
+    ], allow_calls=(0x2EA30,), calls=(E.Call(0x2B150, ("eax",)), E.Call(0x249D0, ("eax",)),
                                      E.Call(0x249B0, ("eax", "edx"))),
        eax_mask=0, mutants=("@mutant", "@field", "@latch")),
     # 0x2A408 (record §P6.2): EAX = rec, EDX = pset; the sprite-id reader: +0x28 bit 8 keeps
@@ -3409,6 +3409,782 @@ P8_SPECS = [
        mutants=("@side", "@stream", "@globs", "@end", "@order")),
 ]
 
+# ---- track P batch C2b: the nine deferred callee rows (record 2026-10-05-reverse-c2b) ------------
+#
+# The resource fixture every 0x1B544 caller shares: a preloaded INDEX entry whose payload is in the
+# image, so the original's 0x1B544 fast path (0x1B569 `test [ecx+0xc],0x1000000`) returns
+# payload + (handle & 0x7FFFFF) exactly as the port's res_resolve does, with no EMS call. The entry
+# count DS_001014F0 covers the index, so the port's in-range guard passes too.
+C2B_RES_PTR, C2B_RES_N = 0x001014E0, 0x001014F0   # DS_001014E0 (table ptr) / DS_001014F0 (count)
+C2B_RES_TABLE = 0x10A600          # 0x14-byte entries in the image's zero BSS
+C2B_RES_DATA = 0x10A700           # the resolved payload (its first dword is the colour count)
+
+def c2b_res(handle, count):
+    """The pokes that make `handle` resolve to a preloaded entry whose payload's [0] = count.
+
+    0x1B544 (and res_resolve) add the handle's low 23 bits to the entry's payload base, so the entry
+    stores the base and the per-handle payload sits at base + (handle & 0x7FFFFF)."""
+    idx = (handle >> 23) & 0xFF
+    entry = C2B_RES_TABLE + idx * 0x14
+    data = C2B_RES_DATA + (handle & 0x7FFFFF)
+    return {C2B_RES_PTR: le32(C2B_RES_TABLE), C2B_RES_N: le32(0x100),
+            entry + 0x0C: le32(0x01000000), entry + 0x10: le32(C2B_RES_DATA), data: le32(count)}
+
+# 0x33754 (record §C2b): EAX = palette handle. Search the 24-entry palette table at 0x107618 for the
+# handle (refcount++ on a hit), else take the first free entry, seed it {handle; 1; start; count} and
+# append its DAC record at DS_00107798, then reflow the occupied entries after it. EDX is scratch
+# (the entry search re-reads ECX). Mask full (0x2A17C stores the entry offset in the pset's +0x18).
+# 0x1B544 is allowed against the preloaded entry (E.Call would need a port seam it does not have);
+# its fast path and res_resolve agree byte for byte. The full-table fatal 0x3384E is unhit and named.
+C2B_PAL_TABLE, C2B_PAL_END = 0x107618, 0x107798
+C2B_DIRTY = 0x107500              # a valid dirty-list head (0x107498..0x107618)
+C2B_PAL_TAIL = 0x00107798         # DS_00107798: the dirty-list head variable
+
+def c2b_pal_entry(i, handle, start, count):
+    """One 16-byte poke per entry: handle, 1, start, count (the case's poke limit is 16)."""
+    e = C2B_PAL_TABLE + i * 0x10
+    return {e: le32(handle) + le32(1) + le32(start) + le32(count)}
+
+def c2b_pal_run(n, base_handle, start0, length):
+    """Pokes for the contiguous occupied entries 0..n-1, as 64-byte pokes (the case limit)."""
+    blob = b"".join(le32(base_handle + i) + le32(1) + le32(start0 + i * length) + le32(length)
+                    for i in range(n))
+    return {C2B_PAL_TABLE + off: blob[off:off + 64] for off in range(0, len(blob), 64)}
+
+def c2b_3b298_pokes(side, edx_arg, char, mask, word, count, cmd, ead, bits, o43):
+    """The 0x3B298 fixture: the slot pair, the command word, the +0x100CDE dword, the character byte
+    and the anim[2]+2 word for the (char, arg) triple the allowed 0x3AFC4 computes. mask/word are
+    the case's 0x1AB5C/0x46460 stub EAX values, named here so the call site reads as one fixture."""
+    del mask, word
+    ctx0 = 1 - side
+    anim2 = 0x000A6728 + ((char << 6) + edx_arg) * 6
+    return {**SLOT_PTRS,
+            DS_SLOTS + side * 0x94 + 0x43: bytes([o43]),
+            DS_SLOTS + side * 0x94 + 0x86: b"\x86\x86",
+            DS_SLOTS + (1 - side) * 0x94 + 0x84: b"\x84\x84",
+            0x001088E0 + side * 2: cmd.to_bytes(2, "little"),
+            0x00100CDE + ctx0 * 2: (ead << 16).to_bytes(4, "little"),
+            0x0010782A + ctx0 * 0x94: bytes([char]),
+            0x000BEEF2: (count << 16).to_bytes(4, "little"),
+            anim2 + 2: bits.to_bytes(2, "little")}
+
+C2B_POOL = 0x10A900   # a 0x68-stride actor pool in the image's zero BSS (0x2BD44's row)
+
+def c2b_3b714(side, char, r24, bits=0, a02=0x22, a03=0x33, efc=0, p1_52=0, p1_54=0, p2_52=0,              rec4_34=0, rec5_34=0, rec5_4b=0, row60=0, p2_8a=0):
+    """The 0x3B714 fixture: the winner slot (param_2 = the side's slot), the other slot (param_1),
+    the reaction byte r24, the anim[2]+2 word and anim[0]'s +2/+3 bytes, the 0x39EFC state on the
+    inverted side, and the 2bd44 row for the other record's +0x4B index."""
+    p2 = DS_SLOTS + side * 0x94
+    p1 = DS_SLOTS + (1 - side) * 0x94
+    efc_slot = DS_SLOTS + (1 - side) * 0x94
+    rec4 = E3_REC if side == 0 else E3_REC2
+    rec5 = E3_REC2 if side == 0 else E3_REC
+    c = (char << 6) + r24
+    a0 = 0x000DE114 + c * 11
+    a2 = 0x000A6728 + c * 6
+    # one 64-byte block per record (its +0x34 word, +0x4B index and +0x51 side byte) and per slot
+    # (its +0x52/+0x5F bytes, or the +0x41..+0x65 run), so a case stays inside the 16-poke limit.
+    r4 = bytearray(0x40); r4[0:4] = le32(rec4_34); r4[0x51 - 0x34] = side
+    r5 = bytearray(0x40); r5[0:4] = le32(rec5_34); r5[0x51 - 0x34] = 1 - side
+    if rec5_4b:
+        r5[0x4B - 0x34] = rec5_4b
+    s2 = bytearray(64); s2[0x52 - 0x40] = p2_52; s2[0x5F - 0x40] = r24
+    s1 = bytearray(64)
+    s1[0x41 - 0x41] = 0x41; s1[0x52 - 0x41] = p1_52; s1[0x54 - 0x41] = p1_54
+    s1[0x4E - 0x41] = 0x4E; s1[0x65 - 0x41] = 0x65
+    p = {**SLOT_PTRS, rec4 + 0x34: bytes(r4), rec5 + 0x34: bytes(r5),
+         p2 + 0x40: bytes(s2), p1 + 0x41: bytes(s1),
+         0x0010782A + side * 0x94: bytes([char]),
+         a0 + 2: bytes([a02, a03]), a2 + 2: bits.to_bytes(2, "little")}
+    if p2_8a:
+        p[p2 + 0x8A] = bytes([p2_8a])
+    if rec5_4b:
+        p[0x001014F4] = le32(C2B_POOL)
+        p[C2B_POOL + rec5_4b * 0x68 + 0x60] = bytes([row60])
+    if efc:
+        p[efc_slot + 0x53] = b"\x0a"
+        p[efc_slot + 0x10] = le32(0x39CC8)
+        p[efc_slot + 0x58] = b"\x04"
+    return p
+
+def c2b_39834(side, b, char, k=0, a0=0x10, a1=0x40, a8=0x08, tbl=100, r=0, ai=0, s5d=0, f2=0,
+              s52=0, s54=1, p_104abc=0, p_104b14=0, m2c=0x0101, m28=0x28282828):
+    """The 0x39834 fixture: the side's slot and the inverted side's char byte select the anim triple
+    (char<<6 | b); the 0x107D2A+ctx0*2 dword's high word is k (and its low word the +0x107D2C/D2A
+    counter); a0/a0+1/a0+8 are the anim[0] bytes; tbl the per-level table entry for k <= 0xB;
+    r the 0x39738 stub value; ai the 0x468D8 inputs (slot+0x10 == 0x22BEC, rec+0x24 clear, +0x54
+    != 2, or +0x52 == 7); s5d/s52/s54/f2/p_104abc/p_104b14 the tail gates. Blocks: the record's
+    +0x24..+0x63, the slot's +0x41..+0x80 and the 0x107D20..+0x5F run (the counters and 0x107D28
+    plus the per-side k dword), so a case stays inside the 16-poke limit."""
+    ctx0 = 1 - side
+    slot = DS_SLOTS + side * 0x94
+    rec = E3_REC if side == 0 else E3_REC2
+    c = (char << 6) + b
+    A0 = 0x000DE114 + c * 11
+    kd = 0x00107D2A + ctx0 * 2
+    rb = bytearray(0x40); rb[0:4] = le32(0); rb[0x51 - 0x24] = side
+    if ai == 0:
+        rb[0:4] = le32(0x80000000)
+    sb = bytearray(64)
+    sb[0x43 - 0x41] = 0x43; sb[0x52 - 0x41] = s52; sb[0x54 - 0x41] = s54; sb[0x5D - 0x41] = s5d
+    sb[0x53 - 0x41] = 0x53; sb[0x5E - 0x41] = 0x5E
+    tb = bytearray(64)
+    tb[0x00107D28 - 0x00107D20:0x00107D28 - 0x00107D20 + 4] = le32(m28)
+    tb[kd - 0x00107D20:kd - 0x00107D20 + 4] = (m2c | (k << 16)).to_bytes(4, "little")
+    ctr = 0x00107D20 + ctx0 * 2
+    tb[ctr - 0x00107D20:ctr - 0x00107D20 + 2] = m2c.to_bytes(2, "little")
+    ab = bytearray(11); ab[0] = a0; ab[1] = a1; ab[8] = a8
+    p = {**SLOT_PTRS, rec + 0x24: bytes(rb), slot + 0x41: bytes(sb), 0x00107D20: bytes(tb),
+         slot + 0x10: le32(0x22BEC if ai else 0x11111111),
+         0x0010782A + ctx0 * 0x94: bytes([char]), A0: bytes(ab),
+         0x001078F2 + side: bytes([f2]),
+         0x00104ABC: le32(p_104abc), 0x00104B14: bytes([p_104b14])}
+    if k <= 0xB:
+        p[0x000BEBF8 + k * 4] = le32(tbl)
+    return p
+
+def c2b_36870(side, mode=0, s54=0, s41=0x41, s42=0x42, s43=0x43, s40=0x40404040, s5d=0x5d,
+              r365=0, r366=0, char=0, s28=0x2828):
+    """The 0x36870 fixture: rec+0x51 = side, the slot pair, the slot's +0x40 dword and +0x41..+0x7F
+    run (one block), its +0x84..+0x90 run, the record's +0x1C..+0x5B run and +0x7A char, the mode
+    word and the three dword globals the resets clear. r365/r366 are the 0x365C8/0x36638 stub
+    values; char selects the per-character anim pointers DSD(0xC8950/0xC89A0/0xC89F0 + char*4)."""
+    s = DS_SLOTS + side * 0x94
+    so = DS_SLOTS + (1 - side) * 0x94
+    rec = E3_REC if side == 0 else E3_REC2
+    b1 = bytearray(64)
+    b1[0:4] = le32(s40); b1[0x41 - 0x40] = s41; b1[0x42 - 0x40] = s42; b1[0x43 - 0x40] = s43
+    for off in (0x52, 0x53, 0x54, 0x55, 0x5F, 0x62, 0x65, 0x67, 0x68):
+        b1[off - 0x40] = off
+    b1[0x54 - 0x40] = s54
+    b1[0x5D - 0x40] = s5d
+    b1[0x74 - 0x40:0x74 - 0x40 + 2] = (0x7474).to_bytes(2, "little")
+    b1[0x7A - 0x40] = char
+    b2 = bytearray(17)
+    b2[0x84 - 0x80:0x84 - 0x80 + 2] = (0x8484).to_bytes(2, "little")
+    b2[0x8A - 0x80] = 0x8A; b2[0x90 - 0x80] = 0x90
+    rb = bytearray(0x40)
+    rb[0x28 - 0x1C:0x2A - 0x1C] = s28.to_bytes(2, "little")
+    for off in (0x34, 0x36, 0x42, 0x43, 0x44, 0x4C, 0x4D):
+        rb[off - 0x1C] = off
+    rb[0x51 - 0x1C] = side
+    p = {**SLOT_PTRS, rec + 0x51: bytes([side]), rec + 0x1C: bytes(rb), s + 0x40: bytes(b1),
+         s + 0x80: bytes(b2), rec + 0x7A: bytes([char]),
+         0x00104B00: mode.to_bytes(2, "little"),
+         0x00100CE0 + (1 - side) * 2: (0xCE00 + 1 - side).to_bytes(2, "little"),
+         0x00100AF8 + side * 4: le32(0xAF8AF8F8),
+         0x000FD148 + side * 4: le32(0xD148D148)}
+    return p
+
+C2B_2AE14_POOL = 0x10A900        # the actor pool base DS_001014F4 points at
+C2B_2AE14_REC = 0x10A968         # pool + 1*0x68: the record actor_alloc returns
+C2B_2AE14_PSET = 0x10AD00        # the pset base DS_001014EC points at
+C2B_2AE14_DESC = 0x10AE00        # the descriptor the spawn reads
+C2B_2AE14_STR = 0x10AF00         # the stream the animation walk follows
+C2B_2AE14_NODE = 0x10AF80        # the render list's free node
+
+def c2b_2ae14(a2=0, a3=0, a4=0, a5=0, frame=0, hdl=0, type21=1, dp8=0, dp0=C2B_2AE14_STR,
+              word1=0, word2=0, cb=0x5D812, free=1, word_a=0, word_b=0):
+    """The 0x2AE14 fixture: the descriptor (stream dword, type, frame, the word fields and the
+    palette handle), the actor pool and pset bases, the returned record (pool+0x68), the render
+    free node and list head, the 0xBB9DC type-table entry (type 1 -> 0xBB9E8) and the stream's first
+    two words. free=0 empties the render free list."""
+    rec = C2B_2AE14_REC
+    pset = C2B_2AE14_PSET + 0x20
+    dp = (le32(dp0) + bytes([type21, frame]) + word_a.to_bytes(2, "little")
+          + dp8.to_bytes(2, "little") + word_b.to_bytes(2, "little")
+          + (0x0C0C).to_bytes(2, "little") + le32(hdl))
+    psb = bytearray(32)
+    for i in range(32):
+        psb[i] = 0xA5
+    psb[0x0E] = 0x2E; psb[0x0F] = 0x2E
+    p = {0x001014EC: le32(C2B_2AE14_PSET), 0x001014F4: le32(C2B_2AE14_POOL),
+         C2B_2AE14_DESC: dp + b"\xa5" * (0x14 - len(dp)),
+         0x000BB9E8: le32(cb), C2B_2AE14_STR: word1.to_bytes(2, "little") + word2.to_bytes(2, "little"),
+         pset: bytes(psb), 0x00105B44: le32(0), 0x0010275C: le32(C2B_2AE14_NODE) if free else le32(0),
+         0x000F0A78: le32(0x000F0A78),
+         C2B_2AE14_NODE: le32(0) + le32(pset)}
+    # seed the record with sentinels: the spawn overwrites most fields.
+    p[rec + 0x08] = b"\xa5" * 48
+    p[rec + 0x40] = b"\xa5" * 40
+    return p
+
+C2B_OP_REC = 0x10A980            # the 0x2B2A0 row's record
+C2B_OP_STR = 0x10AA00            # its command word and the words that follow
+C2B_OP_PSET = 0x10AA80           # DS_001014EC: an 0x20-stride pset base (index 1 -> +0x20)
+C2B_OP_DESC = 0x10AB00           # the opcode-0x0C child descriptor
+
+C2B_OP_RNG = 0x1234
+C2B_OP_CHILD = 0x10A9E8   # C2B_OP_REC + 0x68
+
+def c2b_op(op, value=0x10, low=None, r8=None, word_extra=b"", r28=0x2828, r18=0x18181818,
+           r1c=0x1C1C1C1C, r20=0x20202020, r24=0x24242424, r50=0x50, r2a=0x2A2A,
+           r2c=0x2C2C, r2e=0x2E2E, r30=0x3030, r32=0x3232, r34=0x3434, r36=0x3636,
+           r38=0x3838, r4e=0x4E, r4f=0x4F4F4F4F, r59=0x59, r61=0x61, p5e6=0, p5e8=0,
+           p5d4=C2B_OP_DESC, pset0c=0x0C0C):
+    """One 0x2B2A0 case: the command word (op<<8 | low) at C2B_OP_STR, the record's fields, the
+    0x105BE4/6/8 words, the stream base 0x105BD4 and the pset's +0x0C word. `value` is the 0x2B8F8
+    stub EAX (the operand); `word_extra` are the bytes after the command word (bounded ops)."""
+    rec, str_ = C2B_OP_REC, C2B_OP_STR
+    if r8 is None:
+        r8 = str_
+    if op >= 0x20 or op == 0x1F:
+        # the 0x1F prefix: anim_operand stores the low byte as the opcode and returns the next word.
+        data = (((0x1F << 8) | op).to_bytes(2, "little")
+                + (value & 0xFFFF).to_bytes(2, "little") + word_extra)
+    else:
+        low = (value & 0xFF) if low is None else low
+        data = (((op << 8) | low).to_bytes(2, "little") + word_extra)
+    p = {rec + 8: le32(r8) + b"\xa5" * 60,
+         rec + 0x48: b"\xa5" * 28,
+         str_: data + b"\xa5" * (8 - len(data)),
+         rec + 0x18: le32(r18) + le32(r1c) + le32(r20) + le32(r24),
+         rec + 0x28: r28.to_bytes(2, "little") + r2a.to_bytes(2, "little") + r2c.to_bytes(2, "little")
+                     + r2e.to_bytes(2, "little") + r30.to_bytes(2, "little") + r32.to_bytes(2, "little")
+                     + r34.to_bytes(2, "little") + r36.to_bytes(2, "little") + r38.to_bytes(2, "little"),
+         rec + 0x4E: bytes([r4e]) + le32(r4f) + bytes([r50]) + bytes([0x51]),
+         rec + 0x59: bytes([r59]) + b"\xa5" * 7 + bytes([r61]),
+         0x00105BE4: p5e6.to_bytes(2, "little") + p5e8.to_bytes(2, "little"),
+         0x00105BD4: le32(p5d4) + le32(0x5D8),
+         0x001014EC: le32(C2B_OP_PSET),
+         C2B_OP_PSET + 0x20 + 0x0C: pset0c.to_bytes(2, "little"),
+         0x000EF6DC: le32(0xEF6DC) + le32(0)}
+    p[0x00105BE6] = p5e6.to_bytes(2, "little")
+    p[0x00105BE8] = p5e8.to_bytes(2, "little")
+    return p
+
+C2B_VOICE_BASE = 0x000BBDC8      # the 0x0C-stride voice table
+
+def c2b_voice(vid, vtype, h=0x1111, b=0x22, cur=0, playing=0):
+    """One 0x2C3FC case: the id's voice record {type, h, b} and the current-song dword; the playing
+    stub (0x1CE70) is set where the type-2/3/4 or the case-5 sub-ids test it."""
+    rec = C2B_VOICE_BASE + vid * 0xC
+    pokes = {rec: bytes([vtype]) + b"\x00\x00\x00" + le32(h) + bytes([b, 0, 0]),
+             0x00105D5C: le32(cur)}
+    stubs = {0x1CE70: playing} if vtype in (2, 3, 4) or vtype == 5 else {}
+    return {"eax": vid}, pokes, stubs
+
+def c2b_voice5(cid, vid, cur, playing=0):
+    """A type-5 case: the dispatcher's music_stop/sample_stop sub-id menu."""
+    r, p, s = c2b_voice(vid, 5, cur=cur, playing=playing)
+    return Case(cid, r, p, s)
+
+C2B_VOICE_CASES = [
+    Case("v0", {"eax": 0}, {0x00105D5C: le32(0x30)}),          # id 0: return 0
+    Case("v100", {"eax": 0x100}, {C2B_VOICE_BASE: bytes([1]) + b"\x00\x00\x00" + le32(0x9999)
+                                 + bytes([9, 0, 0]), 0x00105D5C: le32(0)}),  # id 0x100 -> record 0
+    Case("t0", *c2b_voice(1, 0)),                              # type 0: return 1
+    Case("t1", *c2b_voice(2, 1, h=0x1234, b=5, cur=0x30)),      # type 1: music request
+    Case("t2a", *c2b_voice(3, 2, h=0x77, playing=1)),           # type 2, playing: return 0
+    Case("t2b", *c2b_voice(4, 2, h=0x78, b=3, playing=0)),      # type 2, free: queue
+    Case("t3a", *c2b_voice(0x46, 3, h=1, playing=1)),           # id 0x46, playing
+    Case("t3b", *c2b_voice(0x46, 3, h=1, playing=0)),
+    Case("t3c", *c2b_voice(0x4D, 3, h=2, playing=0)),
+    Case("t3d", *c2b_voice(0x5D, 3, h=3, playing=0)),
+    Case("t3e", *c2b_voice(0x10, 3, h=4, playing=0)),           # another type-3 id: return 0
+    Case("t3f", *c2b_voice(0x50, 3, h=5, playing=0)),           # an id in (0x4D,0x5D): the 0x2C4D2 tail
+    Case("t4a", *c2b_voice(5, 4, playing=1)),                   # type 4, the sample playing
+    Case("t4b", *c2b_voice(6, 4, playing=0)),                   # type 4, the queue arm
+    *[c2b_voice5("t5_%02x_%d" % (vid, i), vid, cur)
+      for (vid, curs) in [(0x00, [0]), (0x22, [0x20, 0x10]), (0x2B, [0x2A, 0]),
+                          (0x2D, [0x2C, 0]), (0x2F, [0x2E, 0x30, 0]), (0x33, [0x32, 0]),
+                          (0x3C, [0x3B, 0]), (0x3F, [0]), (0x41, [0]), (0x43, [0]),
+                          (0x4C, [0]), (0x4F, [0]), (0x55, [0x54, 0]), (0x57, [0x56, 0]),
+                          (0x5B, [0]), (0xE0, [0xDF, 0]), (0xE2, [0xE1, 0xE3, 0]), (0xF1, [0]),
+                          (0x01, [0])]
+      for i, cur in enumerate(curs)],
+    Case("t6", *c2b_voice(7, 6)),                               # type 6: return 0
+    # the unmatched sub-ids in each case-5 binary-search range, so every range tail is hit.
+    *[c2b_voice5("t5_tail_%02x" % vid, vid, 0)
+      for vid in (0x02, 0x24, 0x2C, 0x2E, 0x34, 0x42, 0x44, 0x50, 0x51, 0x58, 0x5C, 0x80, 0xE4, 0xF2, 0xFF)],
+    # the 0x100 remap with record 0's type 5: the sub-id switch still sees the remapped 0.
+    Case("v100b", {"eax": 0x100}, {C2B_VOICE_BASE: bytes([5]) + b"\x00\x00\x00" + le32(1)
+                                   + bytes([0, 0, 0]), 0x00105D5C: le32(0)}),
+]
+
+C2B_OP_CASES = [
+    # One case per opcode: the command word is (op<<8 | value) for the 5-bit ops and the 0x1F prefix
+    # (0x1F<op>) plus the operand word for op >= 0x20, so the real anim_operand (allowed) selects the
+    # opcode. The stub EAX values: set_dead/sample are void; 0x5D7DC rng; 0x2AE14 the child record;
+    # 0x29DB8/0x2C3FC void.
+    *[Case(cid, {"eax": C2B_OP_REC, "edx": 1, "ebx": flag},
+           {**c2b_op(op, value=value, word_extra=we, **{k: v for k, v in kw.items()
+                                                       if k in ("r28", "p5e6", "p5e8", "p5d4", "pset0c")})},
+           ({0x2B150: 0} if op == 0 else {})
+           | ({0x5D7DC: C2B_OP_RNG} if op == 0x08 else {})
+           | ({0x2AE14: C2B_OP_CHILD} if op == 0x0C else {})
+           | ({0x29DB8: 0} if op in (0x0D, 0x0E, 0x0F, 0x17, 0x18, 0x19) else {})
+           | ({0x2C3FC: 0} if op == 0x2E else {}))
+      for (cid, op, value, flag, we, kw) in [
+        ("o00a", 0x00, 0x11, 1, b"", {}),
+        ("o00b", 0x00, 0x12, 0, b"", {}),
+        ("o01", 0x01, 0x13, 0, b"", {}),
+        ("o02", 0x02, 0x2B, 0, b"", {}),
+        ("o03", 0x03, 0x14, 0, b"", {}),
+        ("o04a", 0x04, 0x15, 0, b"", {"p5e8": 5}),
+        ("o04b", 0x04, 0x15, 0, b"", {"p5e8": 0}),
+        ("o05a", 0x05, 0x00, 0, b"", {}),
+        ("o05b", 0x05, 0x01, 0, b"", {}),
+        ("o06a", 0x06, 0x02, 0, b"\x01\x00", {}),
+        ("o06b", 0x06, 0x02, 0, b"\x00\x00", {}),
+        ("o07", 0x07, 0x16, 0, b"", {}),
+        ("o08", 0x08, 0x64, 0, b"", {}),
+        ("o09", 0x09, 0x17, 0, b"", {}),
+        ("o0a", 0x0A, 0x7E, 0, b"", {}),
+        ("o0b", 0x0B, 0x18, 0, b"", {}),
+        ("o0ca", 0x0C, 0x01, 0, b"\x02\x00\x03\x00", {"p5e8": 1}),
+        ("o0cb", 0x0C, 0x02, 0, b"\x02\x00\x03\x00", {"p5e8": 0}),
+        ("o0d", 0x0D, 0x1A, 0, b"", {}),
+        ("o0e", 0x0E, 0x1B, 0, b"", {}),
+        ("o0fa", 0x0F, 0x1C, 0, b"", {"p5e6": 0}),
+        ("o0fb", 0x0F, 0x1C, 0, b"", {"p5e6": 1}),
+        ("o10", 0x10, 0x29D60, 0, b"", {"p5d4": 0x29D60}),
+        ("o11", 0x11, 0x29D60, 0, b"", {"p5d4": 0x29D60}),
+        ("o12", 0x12, 0x1D, 0, b"", {}),
+        ("o13", 0x13, 0x1E, 0, b"", {}),
+        ("o14", 0x14, 0x1F, 0, b"", {}),
+        ("o15", 0x15, 0x29D60, 0, b"", {"p5d4": 0x29D60}),
+        ("o16", 0x16, 0x20, 0, b"", {}),
+        ("o17", 0x17, 0x21, 0, b"", {"p5e8": 3}),
+        ("o18a", 0x18, 0x01, 0, b"\x04\x00", {"p5e8": 2}),
+        ("o18b", 0x18, 0x01, 0, b"\x01\x00", {"p5e8": 2}),
+        ("o19a", 0x19, 0x03, 0, b"\x01\x00", {"p5e8": 2}),
+        ("o19b", 0x19, 0x03, 0, b"\x05\x00", {"p5e8": 2}),
+        ("o1a", 0x1A, 0x22, 0, b"", {}),
+        ("o1b", 0x1B, 0x23, 0, b"", {}),
+        ("o1c", 0x1C, 0x24, 0, b"", {}),
+        ("o1d", 0x1D, 0x25, 0, b"", {}),
+        ("o1e", 0x1E, 0x26, 0, b"", {}),
+        ("o1f", 0x1F, 0x02, 0, b"", {"r28": 0x2828}),
+        ("o20a", 0x20, 0x02, 0, b"", {"r28": 0x2828}),
+        ("o20b", 0x20, 0x02, 0, b"", {"r28": 0x6828}),
+        ("o21", 0x21, 0x03, 0, b"", {}),
+        ("o22", 0x22, 0x03, 0, b"", {}),
+        ("o23", 0x23, 0x00, 0, b"", {}),
+        ("o24", 0x24, 0x00, 0, b"", {}),
+        ("o25", 0x25, 0x04, 0, b"", {}),
+        ("o26", 0x26, 0x05, 0, b"", {}),
+        ("o27", 0x27, 0x06, 0, b"", {}),
+        ("o28a", 0x28, 0x07, 0, b"", {"r28": 0x2828}),
+        ("o28b", 0x28, 0x07, 0, b"", {"r28": 0x6828}),
+        ("o29", 0x29, 0x08, 0, b"", {}),
+        ("o2a", 0x2A, 0x09, 0, b"", {}),
+        ("o2b", 0x2B, 0x0A, 0, b"", {}),
+        ("o2c", 0x2C, 0x0B, 0, b"", {}),
+        ("o2da", 0x2D, 0x20, 0, b"", {"pset0c": 0x10}),
+        ("o2db", 0x2D, 0x20, 0, b"", {"pset0c": 0x20}),
+        ("o2e", 0x2E, 0x6F, 0, b"", {}),
+        ("o2f", 0x2F, 0x00, 0, b"", {}),
+      ]]]
+
+C2B_SPECS = [
+    Spec("palette_acquire", 0x33754, [
+        # p0: empty table; entry 0 is the first free; sentinels on its fields and a seeded dirty area.
+        Case("p0", {"eax": 0x800040}, {**c2b_res(0x800040, 3), C2B_PAL_TAIL: le32(C2B_DIRTY),
+             C2B_PAL_TABLE + 4: b"\xa5\xa5\xa5\xa5", C2B_PAL_TABLE + 8: b"\xa5\xa5\xa5\xa5",
+             C2B_PAL_TABLE + 0x0C: b"\xa5\xa5\xa5\xa5", C2B_DIRTY: b"\xa5" * 0x20}),
+        # p1: the handle already owns entry 0: refcount 7 -> 8, nothing else moves.
+        Case("p1", {"eax": 0x800040}, {**c2b_res(0x800040, 3), C2B_PAL_TAIL: le32(C2B_DIRTY),
+             **c2b_pal_entry(0, 0x800040, 0x1111, 0x2222), C2B_DIRTY: b"\xa5" * 0x20}),
+        # p2: entry 1 is free and entry 0 is occupied (start 5, len 4): the new entry starts at 9;
+        # the reflow skips free entry 1 and 3, moves entry 2 (0x20 -> 12), and entry 4 (14) stops it.
+        Case("p2", {"eax": 0x800040}, {**c2b_res(0x800040, 3), C2B_PAL_TAIL: le32(C2B_DIRTY),
+             **c2b_pal_entry(0, 0x800001, 5, 4), **c2b_pal_entry(2, 0x800002, 0x20, 2),
+             **c2b_pal_entry(4, 0x800004, 14, 4), C2B_PAL_TABLE + 0x10: b"\x00\x00\x00\x00",
+             C2B_DIRTY: b"\xa5" * 0x40}),
+        # p3: entries 0..22 occupied, slot 23 free: the new entry lands on the last slot, start is the
+        # accumulated end 0x170 and the reflow loop does not run (the next slot is the table end).
+        Case("p3", {"eax": 0x800040}, {**c2b_res(0x800040, 3), C2B_PAL_TAIL: le32(C2B_DIRTY),
+             **c2b_pal_run(23, 0x800100, 0, 0x10),
+             C2B_PAL_TABLE + 23 * 0x10: b"\x00\x00\x00\x00", C2B_PAL_TABLE + 23 * 0x10 + 4: b"\xa5" * 4,
+             C2B_DIRTY: b"\xa5" * 0x20}),
+        # p4: the handle owns the last slot. The search walks all 24 entries.
+        Case("p4", {"eax": 0x800040}, {**c2b_res(0x800040, 3), C2B_PAL_TAIL: le32(C2B_DIRTY),
+             **c2b_pal_entry(23, 0x800040, 0x1111, 0x2222), C2B_DIRTY: b"\xa5" * 0x20}),
+        # p5: the resolved count is 0.
+        Case("p5", {"eax": 0x800040}, {**c2b_res(0x800040, 0), C2B_PAL_TAIL: le32(C2B_DIRTY),
+             C2B_PAL_TABLE + 4: b"\xa5\xa5\xa5\xa5", C2B_PAL_TABLE + 8: b"\xa5\xa5\xa5\xa5",
+             C2B_PAL_TABLE + 0x0C: b"\xa5\xa5\xa5\xa5", C2B_DIRTY: b"\xa5" * 0x20}),
+        # p6: a nonzero low-23 offset: the count comes from payload+0x40 and the record stores the
+        # full handle.
+        Case("p6", {"eax": 0x800043}, {**c2b_res(0x800043, 5), C2B_PAL_TAIL: le32(C2B_DIRTY),
+             C2B_PAL_TABLE + 4: b"\xa5\xa5\xa5\xa5", C2B_PAL_TABLE + 8: b"\xa5\xa5\xa5\xa5",
+             C2B_PAL_TABLE + 0x0C: b"\xa5\xa5\xa5\xa5", C2B_DIRTY: b"\xa5" * 0x20}),
+    ], allow_calls=(0x1B544,), eax_mask=0xFFFFFFFF,
+       unhit_named={0x3384E: "the table-full fatal: the raw calls 0x62003, the port returns 0 "
+                           "(raw-over-port deviation, record §C2b.3)"},
+       mutants=("@mutant", "@new", "@inc", "@start", "@reflow", "@count")),
+    # 0x13C70 (record §C2b): EAX = source_rec, EDX = byte_arg, EBX = palette handle. Pop the free
+    # head (0x249D0), fill the record, zero +0x10 if the source's +0xC signed count is positive, copy
+    # resolved[1..] into +0x410, then insert at the active head (0x249B0) under the 0x9AF3C lock.
+    # 0x249D0/0x249B0 and the host 0x1B544 are allowed: the port's effects.c keeps its own copies of
+    # the two list primitives (no seam), and its writes match the raw's byte for byte. The port skips
+    # the +0x410 copy when res_resolve fails; the cases resolve. Mask 0: every caller ignores the
+    # return (the raw's EAX at return is 0xFC CE0, the port's is the record offset, record §C2b.3).
+    Spec("effects_spawn", 0x13C70, [
+        # f0: count 3: both loops run, the copies differ from their sentinels, the lock is restored
+        # from 0x5A and the active counter 0x9AF3D starts at 0x7E.
+        Case("f0", {"eax": 0x10A100, "edx": 3, "ebx": 0x800043},
+             {**c2b_res(0x800043, 7), 0x000FCCE8: le32(0x10A200),
+              0x10A200: le32(0x000FCCE0) + le32(0x000FCCE8),
+              0x10A208: b"\xa5" * 8, 0x10A210: b"\xa5" * 12, 0x10A610: b"\xa5" * 12,
+              0x000FCCE0: le32(0x000FCCE0) + le32(0x000FCCE0), 0x10A10C: le32(3),
+              0x0009AF3C: b"\x5a", 0x0009AF3D: b"\x7e",
+              C2B_RES_DATA + 0x43 + 4: le32(0x11) + le32(0x22) + le32(0x33)}),
+        # f1: count -1: the signed test skips both loops; +0x10 and +0x410 keep their sentinels.
+        Case("f1", {"eax": 0x10A100, "edx": 0, "ebx": 0x800043},
+             {**c2b_res(0x800043, 7), 0x000FCCE8: le32(0x10A200),
+              0x10A200: le32(0x000FCCE0) + le32(0x000FCCE8), 0x10A210: b"\xa5" * 12,
+              0x10A610: b"\xa5" * 12, 0x000FCCE0: le32(0x000FCCE0), 0x10A10C: le32(0xFFFFFFFF),
+              0x0009AF3C: b"\x00", 0x0009AF3D: b"\x00"}),
+        # f2: count 0: the same skip path.
+        Case("f2", {"eax": 0x10A100, "edx": 0, "ebx": 0x800043},
+             {**c2b_res(0x800043, 7), 0x000FCCE8: le32(0x10A200),
+              0x10A200: le32(0x000FCCE0) + le32(0x000FCCE8), 0x10A210: b"\xa5" * 12,
+              0x10A610: b"\xa5" * 12, 0x000FCCE0: le32(0x000FCCE0), 0x10A10C: le32(0),
+              0x0009AF3C: b"\x00", 0x0009AF3D: b"\x00"}),
+        # f3: the free list is empty (its sentinel points at itself): the raw returns with rec 0 and
+        # EAX = the still-untouched source_rec; the port returns 0. Mask 0 makes that agree.
+        Case("f3", {"eax": 0x10A100, "edx": 9, "ebx": 0x800043},
+             {0x000FCCE8: le32(0x000FCCE8), 0x000FCCE0: le32(0x000FCCE0), 0x10A10C: le32(3),
+              0x0009AF3C: b"\x11"}),
+        # f4: count 1: each loop runs exactly once.
+        Case("f4", {"eax": 0x10A100, "edx": 0xFF, "ebx": 0x800043},
+             {**c2b_res(0x800043, 7), 0x000FCCE8: le32(0x10A200),
+              0x10A200: le32(0x000FCCE0) + le32(0x000FCCE8), 0x10A210: b"\xa5" * 8,
+              0x10A610: b"\xa5" * 8, 0x000FCCE0: le32(0x000FCCE0), 0x10A10C: le32(1),
+              0x0009AF3C: b"\x00", 0x0009AF3D: b"\x00",
+              C2B_RES_DATA + 0x43 + 4: le32(0x7777)}),
+    ], allow_calls=(0x1B544, 0x249B0, 0x249D0), eax_mask=0,
+       mutants=("@mutant", "@count", "@copy", "@lock", "@free")),
+    # 0x3B298 (record §C2b): EAX = side, EDX = arg. The command dispatch: ctx swap (0x33A10) and the
+    # anim triple (0x3AFC4) are allowed; 0x3B134 (the mapper), 0x1AB5C (the input mask), 0x46460
+    # (one ring word) and 0x1A734 (the block hit) are stubbed with per-case values, so the cases
+    # select each arm. anim[2]+2 is the word at 0xA6728 + ((char<<6)+arg)*6 + 2. Mask 0xFF (`test
+    # al,al` at the caller). 0x43's bits 0x20/0x10 are the block flags; +0x86 copies the other
+    # slot's +0x84.
+    Spec("fighter_command_dispatch", 0x3B298, [
+        # c0: anim[2]+2 has both bits 0 and 1: return 0 before the scan.
+        Case("c0", {"eax": 0, "edx": 0}, {**SLOT_PTRS, **c2b_3b298_pokes(0, 0, 0, 0, 0, 0, 0, 0, 3, 0x40)}),
+        # c1: no ring hits, mask 0, cmd 0, the +0x100CDE word <= 1: b1 = b2 = 0, return 0.
+        Case("c1", {"eax": 0, "edx": 0}, {**SLOT_PTRS, **c2b_3b298_pokes(0, 0, 0, 0, 0, 0, 0, 0, 0, 0x40)},
+             {0x1AB5C: 0, 0x46460: 0}),
+        # c2: one ring word 0x4000 against mask 0x4000: b2, anim bits 0: set +0x43 bit 0x10, hit.
+        Case("c2", {"eax": 0, "edx": 0}, {**SLOT_PTRS, **c2b_3b298_pokes(0, 0, 0, 0x4000, 0x4000, 1, 0, 0, 0, 0x40)},
+             {0x1AB5C: 0x4000, 0x46460: 0x4000}),
+        # c3: side 1, ring word 1 against mask 1: bit 0x4000 clear and 0x8000 clear: b1, set +0x43
+        # bit 0x20, hit.
+        Case("c3", {"eax": 1, "edx": 0}, {**SLOT_PTRS, **c2b_3b298_pokes(1, 0, 0, 1, 1, 1, 0, 0, 0, 0x40)},
+             {0x1AB5C: 1, 0x46460: 1}),
+        # c4: b1 but anim bit 0 set: the b1 arm is skipped, b2 is 0: return 0.
+        Case("c4", {"eax": 0, "edx": 0}, {**SLOT_PTRS, **c2b_3b298_pokes(0, 0, 0, 1, 1, 1, 0, 0, 1, 0x40)},
+             {0x1AB5C: 1, 0x46460: 1}),
+        # c5: b2 but anim bit 1 set: the b2 arm is skipped: return 0.
+        Case("c5", {"eax": 0, "edx": 0}, {**SLOT_PTRS, **c2b_3b298_pokes(0, 0, 0, 0x4000, 0x4000, 1, 0, 0, 2, 0x40)},
+             {0x1AB5C: 0x4000, 0x46460: 0x4000}),
+        # c6: no ring hits; the command word 0x4000 against mask 0x4000: b2 and the b2 arm.
+        Case("c6", {"eax": 0, "edx": 0}, {**SLOT_PTRS, **c2b_3b298_pokes(0, 0, 0, 0x4000, 0, 0, 0x4000, 0, 0, 0x40)},
+             {0x1AB5C: 0x4000, 0x46460: 0}),
+        # c7: the command word 1 against mask 1 (bit 0x8000 clear): b1 and the b1 arm.
+        Case("c7", {"eax": 0, "edx": 0}, {**SLOT_PTRS, **c2b_3b298_pokes(0, 0, 0, 1, 0, 0, 1, 0, 0, 0x40)},
+             {0x1AB5C: 1, 0x46460: 0}),
+        # c8: the forced block: the +0x100CDE word > 1, anim bit 0x80 set and +0x43 bit 0x30 set.
+        Case("c8", {"eax": 0, "edx": 0}, {**SLOT_PTRS, **c2b_3b298_pokes(0, 0, 0, 0, 0, 0, 0, 2, 0x80, 0x30)},
+             {0x1AB5C: 0, 0x46460: 0}),
+        # c9: the forced block's > 1 test fails (word == 1): no force.
+        Case("c9", {"eax": 0, "edx": 0}, {**SLOT_PTRS, **c2b_3b298_pokes(0, 0, 0, 0, 0, 0, 0, 1, 0x80, 0x30)},
+             {0x1AB5C: 0, 0x46460: 0}),
+        # c10: word > 1 but anim bit 0x80 clear: no force.
+        Case("c10", {"eax": 0, "edx": 0}, {**SLOT_PTRS, **c2b_3b298_pokes(0, 0, 0, 0, 0, 0, 0, 2, 0, 0x30)},
+             {0x1AB5C: 0, 0x46460: 0}),
+        # c11: word > 1 and bit 0x80 set but +0x43's 0x30 bits clear: no force.
+        Case("c11", {"eax": 0, "edx": 0}, {**SLOT_PTRS, **c2b_3b298_pokes(0, 0, 0, 0, 0, 0, 0, 2, 0x80, 0)},
+             {0x1AB5C: 0, 0x46460: 0}),
+        # c12: one ring word 0 against mask 0x4000: the loop's filter skips it.
+        Case("c12", {"eax": 0, "edx": 0}, {**SLOT_PTRS, **c2b_3b298_pokes(0, 0, 0, 0x4000, 0, 1, 0, 0, 0, 0x40)},
+             {0x1AB5C: 0x4000, 0x46460: 0}),
+        # c13: one ring word 0x8001 against mask 1: the match has neither block bit, its 0x8000 bit
+        # is set: the loop continues.
+        Case("c13", {"eax": 0, "edx": 0}, {**SLOT_PTRS, **c2b_3b298_pokes(0, 0, 0, 1, 0x8001, 1, 0, 0, 0, 0x40)},
+             {0x1AB5C: 1, 0x46460: 0x8001}),
+        # c14: one ring word 1 against mask 0x4000: nonzero, but the mask filter skips it (a mutant
+        # that only tests the word for zero would arm b1).
+        Case("c14", {"eax": 0, "edx": 0}, {**SLOT_PTRS, **c2b_3b298_pokes(0, 0, 0, 0x4000, 1, 1, 0, 0, 0, 0x40)},
+             {0x1AB5C: 0x4000, 0x46460: 1}),
+    ], allow_calls=(0x33A10, 0x3AFC4),
+       calls=(E.Call(0x3B134, ("eax", "edx", "ebx"), clobbers=("ebx", "edx", "edi", "ebp")),
+              E.Call(0x1AB5C, ("eax",), clobbers=("ebp",)),
+              E.Call(0x46460, ("eax", "edx"), clobbers=("edx",)),
+              E.Call(0x1A734, ("eax",))),
+       eax_mask=0xFF, mutants=("@mutant", "@copy", "@early", "@scan", "@b2", "@force", "@arm")),
+    # 0x3B714 (record §C2b): EAX = param_1 (the other slot), EDX = param_2 (the winner's slot). The
+    # reaction applier. side = DSD(param_2)[0x51]; ctx = 0x33950(side) (allow); the frame flag
+    # 0x3C59C, the anim triple 0x3AFC4 (allow) and 0x39EFC (its own row, run real) gate; the clean
+    # path runs 0x3B298 (stub), 0x3B080/0x3AE9C/0x2BD44/0x3AAFC/0x3AD98/0x3B6C4 (stubs), the
+    # 0x3B6C4 hold copies the own record's +0x34 onto the other, then +0x41 |= 0x80. The
+    # local_24 == 0xFF fatal (0x3B75D, the raw's 0x62003) is unhit and named. Mask 0: the caller
+    # 0x193B0 ignores the return.
+    Spec("fighter_reaction", 0x3B714, [
+        # r0: the frame flag is already set: return at once.
+        Case("r0", {"eax": DS_SLOTS + 0x94, "edx": DS_SLOTS}, {**c2b_3b714(0, 0, 0x10)},
+             {0x3C59C: 1, 0x3B298: 0, 0x3B080: 0, 0x3AE9C: 0, 0x2BD44: 0, 0x3AAFC: 0,
+              0x3AD98: 0, 0x3B6C4: 0}),
+        # r1: anim[2]+2 bit 0x800: return.
+        Case("r1", {"eax": DS_SLOTS + 0x94, "edx": DS_SLOTS}, {**c2b_3b714(0, 0, 0x10, bits=0x800)},
+             {0x3C59C: 0, 0x3B298: 0, 0x3B080: 0, 0x3AE9C: 0, 0x2BD44: 0, 0x3AAFC: 0,
+              0x3AD98: 0, 0x3B6C4: 0}),
+        # r2: 0x39EFC holds and anim bit 0x4000 is clear: return.
+        Case("r2", {"eax": DS_SLOTS + 0x94, "edx": DS_SLOTS}, {**c2b_3b714(0, 0, 0x10, efc=1)},
+             {0x3C59C: 0, 0x3B298: 0, 0x3B080: 0, 0x3AE9C: 0, 0x2BD44: 0, 0x3AAFC: 0,
+              0x3AD98: 0, 0x3B6C4: 0}),
+        # r3: 0x39EFC holds but anim bit 0x4000 is set: past the second gate; command dispatch 1:
+        # the else arm (0x8A cleared, 0x3AD98), no 0x3B6C4 copy.
+        Case("r3", {"eax": DS_SLOTS + 0x94, "edx": DS_SLOTS}, {**c2b_3b714(0, 0, 0x10, bits=0x4000, efc=1,
+             p2_8a=0x8a)},
+             {0x3C59C: 0, 0x3B298: 1, 0x3B080: 0, 0x3AE9C: 0, 0x2BD44: 0, 0x3AAFC: 0,
+              0x3AD98: 0, 0x3B6C4: 0}),
+        # r4: the full path: param_1's +0x52 == 4 (the two stores), the 0x3B080 seed (param_1's
+        # +0x54 != 2), the 0x3AE9C landing (the winner's +0x52 == 4), dispatch 0, the 2bd44 index
+        # zero, 0x3AAFC, and the 0x3B6C4 hold copy.
+        Case("r4", {"eax": DS_SLOTS + 0x94, "edx": DS_SLOTS}, {**c2b_3b714(0, 0, 0x10, p1_52=4, p1_54=1,
+             p2_52=4, rec4_34=0x1111, rec5_34=0x2222)},
+             {0x3C59C: 0, 0x3B298: 0, 0x3B080: 0, 0x3AE9C: 0, 0x2BD44: 0, 0x3AAFC: 0,
+              0x3AD98: 0, 0x3B6C4: 1}),
+        # r5: the skip arms: param_1's +0x52 != 4, +0x54 == 2 (no 0x3B080), winner's +0x52 != 4
+        # (no 0x3AE9C) and dispatch 1 (0x3AD98); no hold copy.
+        Case("r5", {"eax": DS_SLOTS + 0x94, "edx": DS_SLOTS}, {**c2b_3b714(0, 0, 0x10, p1_52=0, p1_54=2)},
+             {0x3C59C: 0, 0x3B298: 1, 0x3B080: 0, 0x3AE9C: 0, 0x2BD44: 0, 0x3AAFC: 0,
+              0x3AD98: 0, 0x3B6C4: 0}),
+        # r6: dispatch 0 and the other record's +0x4B nonzero with its pool row's +0x60 set: the
+        # 0x2BD44 call, then 0x3AAFC.
+        Case("r6", {"eax": DS_SLOTS + 0x94, "edx": DS_SLOTS}, {**c2b_3b714(0, 0, 0x10, rec5_4b=3, row60=1)},
+             {0x3C59C: 0, 0x3B298: 0, 0x3B080: 0, 0x3AE9C: 0, 0x2BD44: 0, 0x3AAFC: 0,
+              0x3AD98: 0, 0x3B6C4: 0}),
+        # r7: the same index but the row's +0x60 clear: no 0x2BD44 call.
+        Case("r7", {"eax": DS_SLOTS + 0x94, "edx": DS_SLOTS}, {**c2b_3b714(0, 0, 0x10, rec5_4b=3, row60=0)},
+             {0x3C59C: 0, 0x3B298: 0, 0x3B080: 0, 0x3AE9C: 0, 0x2BD44: 0, 0x3AAFC: 0,
+              0x3AD98: 0, 0x3B6C4: 0}),
+        # r8: side 1 with its own character byte and a different anim record: the side/char/index
+        # choices do not fall back on side 0's.
+        Case("r8", {"eax": DS_SLOTS, "edx": DS_SLOTS + 0x94}, {**c2b_3b714(1, 1, 0x20, bits=0x4000)},
+             {0x3C59C: 0, 0x3B298: 0, 0x3B080: 0, 0x3AE9C: 0, 0x2BD44: 0, 0x3AAFC: 0,
+              0x3AD98: 0, 0x3B6C4: 0}),
+    ], allow_calls=(0x33950, 0x33A10, 0x3AFC4),
+       calls=(E.Call(0x3C59C, ("eax", "edx"), clobbers=("edx",)),
+              E.Call(0x3B298, ("eax", "edx"), clobbers=("edx",)),
+              E.Call(0x39EFC, ("eax",), mode="real"),
+              E.Call(0x3B080, ("eax", "edx", "ebx", "ecx"), clobbers=("ebx", "ecx", "edx")),
+              E.Call(0x3AE9C, ("eax", "edx"), clobbers=("edx",)),
+              E.Call(0x2BD44, ("eax", "edx"), clobbers=("edx",)),
+              E.Call(0x3AAFC, ("eax", "s0"), pop=4, clobbers=("ebx", "ecx", "edx", "edi", "ebp")),
+              E.Call(0x3AD98, ("eax", "[edx]", "[edx+4]", "[edx+8]"), clobbers=("edx",)),
+              E.Call(0x3B6C4, ("eax",))),
+       eax_mask=0, mutants=("@mutant", "@hold", "@swap", "@early", "@efc", "@branch"),
+       unhit_named={0x3B75D: "local_24 == 0xFF: the raw calls the 0x62003 fatal; the port returns "
+                           "0 (raw-over-port deviation, record §C2b.3)"}),
+    # 0x39834 (record §C2b): EAX = side, EDX = the reaction byte b. The winner's pose driver. The
+    # ctx swap (0x33A10) and the anim triple (0x3AFC4) are allowed; 0x39738/0x392A0/0x36CE4/0x4F434
+    # are stubbed with per-case values (0x39738's EAX is the scaler's r); 0x468D8 and 0x36D98 run
+    # real (their own rows) and 0x2C3FC is stubbed. The k source is DSD(0x107D2A + ctx0*2) >> 16 and
+    # the +0x107D2C/+0x107D20 word counters are DSW(0x107D2C + ctx0*2)/DSW(0x107D20 + ctx0*2).
+    Spec("fighter_39834", 0x39834, [
+        # f0: k = 1 (the table arm): ebx = 250*0x40/100 = 0xA0; r = 7 with the 0x468D8 predicate
+        # true: 0x36D98 runs; the tail gates are open (4f434).
+        Case("f0", {"eax": 0, "edx": 5}, {**c2b_39834(0, 5, 3, k=1, a1=0x40, tbl=250, r=7, ai=1,
+             s5d=0x10, p_104abc=1, p_104b14=0)},
+             {0x39738: 7, 0x392A0: 0, 0x36CE4: 0, 0x2C3FC: 0, 0x4F434: 0}),
+        # f1: k = 0xC (the divide arm): ebx = 0x40/16 = 4; the predicate false and +0x5D >= 0x44
+        # with 0x1078F2+side set: the zero arm (no 0x36CE4).
+        Case("f1", {"eax": 0, "edx": 1}, {**c2b_39834(0, 1, 3, k=0xC, a1=0x40, r=0, ai=0, s5d=0x50,
+             f2=1, p_104abc=0)},
+             {0x39738: 0, 0x392A0: 0, 0x36CE4: 0, 0x2C3FC: 0, 0x4F434: 0}),
+        # f2: the same but 0x1078F2 clear: 0x36CE4 runs.
+        Case("f2", {"eax": 0, "edx": 2}, {**c2b_39834(0, 2, 3, k=0xC, a1=0x40, r=0, ai=0, s5d=0x50,
+             f2=0, p_104abc=0)},
+             {0x39738: 0, 0x392A0: 0, 0x36CE4: 0, 0x2C3FC: 0, 0x4F434: 0}),
+        # f3: the predicate true but r <= 0: the else-if arm (the 0x398C9 jle).
+        Case("f3", {"eax": 0, "edx": 3}, {**c2b_39834(0, 3, 3, k=1, r=0, ai=1, s5d=0x50, f2=1)},
+             {0x39738: 0, 0x392A0: 0, 0x36CE4: 0, 0x2C3FC: 0, 0x4F434: 0}),
+        # f4: the predicate false and +0x5D below 0x44: both arms skipped.
+        Case("f4", {"eax": 0, "edx": 4}, {**c2b_39834(0, 4, 3, k=1, r=0, ai=0, s5d=0x43)},
+             {0x39738: 0, 0x392A0: 0, 0x36CE4: 0, 0x2C3FC: 0, 0x4F434: 0}),
+        # f5: k = 0x13 (the +0x107D2E counter the same dword's high word bumps to 0x14 before
+        # 0x39973), so the +0x29A store at 0x107824 + side*0x94 runs; the nested 0x104B14 test
+        # blocks 0x4F434.
+        Case("f5", {"eax": 0, "edx": 0x10}, {**c2b_39834(0, 0x10, 3, k=0x13, r=0, ai=0, s5d=0x10,
+             p_104abc=1, p_104b14=1)},
+             {0x39738: 0, 0x392A0: 0, 0x36CE4: 0, 0x2C3FC: 0, 0x4F434: 0}),
+        # f6: side 1 with its own character byte (the 0x1078BE byte) and the k dword at
+        # 0x107D2A; the predicate true via +0x52 == 7.
+        Case("f6", {"eax": 1, "edx": 6}, {**c2b_39834(1, 6, 4, k=1, a1=0x40, tbl=100, r=3, ai=1,
+             s52=7, s5d=0x10, p_104abc=1, p_104b14=0)},
+             {0x39738: 3, 0x392A0: 0, 0x36CE4: 0, 0x2C3FC: 0, 0x4F434: 0}),
+    ], allow_calls=(0x33A10, 0x3AFC4),
+       calls=(E.Call(0x39738, ("eax", "edx"), clobbers=("edx",)),
+              E.Call(0x392A0, ("eax", "edx", "ebx"), clobbers=("ebx", "edx")),
+              E.Call(0x468D8, ("eax",), mode="real"),
+              E.Call(0x36D98, ("eax",), mode="real"),
+              E.Call(0x36CE4, ("eax",)),
+              E.Call(0x2C3FC, ("eax",)),
+              E.Call(0x4F434, ())),
+       eax_mask=0, mutants=("@mutant", "@ai", "@arm", "@thr", "@tail")),
+    # 0x36870 (record §C2b): EAX = rec. The +0x54 machine. All eleven callees are stubbed through
+    # their seams: 0x385B0 (the 0x25 mode reset), 0x39280, 0x164E8, 0x39040, 0x37D18, 0x365C8,
+    # 0x36BC8, 0x36638, 0x2BC30 (anim-begin, three sites), 0x3C520 (case 2) and 0x379C4 (case 4);
+    # 0x365C8/0x36638 return the per-case AL. Mask 0 (the animation-opcode target's return is
+    # dropped by 0x2B2A0's 0x10/0x11/0x15 arms).
+    Spec("fighter_36870", 0x36870, [
+        # f0: mode 0x25: the 0x385B0 reset runs and returns.
+        Case("f0", {"eax": E3_REC}, {**c2b_36870(0, mode=0x25)}),
+        # f1: +0x54 = 0 with +0x42 bit 5: 0x37D18 and return.
+        Case("f1", {"eax": E3_REC}, {**c2b_36870(0, s54=0, s42=0x22, s43=0x43)}),
+        # f2: +0x43 bit 2 runs 0x36BC8 after the 0x365C8 hit sets bit 0x40 (the +0x41 bit-2 mask
+        # clears +0x43 bit 2 when it runs, so the two are separate cases).
+        Case("f2", {"eax": E3_REC}, {**c2b_36870(0, s54=0, s41=0x41, s42=0x42, s43=0x04,
+             r365=1, char=1)}, {0x365C8: 1}),
+        # f2b: the +0x41 bit-2 arm runs 0x39280 and its +0x40 mask; 0x36638 then returns nonzero.
+        Case("f2b", {"eax": E3_REC}, {**c2b_36870(0, s54=0, s41=0x44, s42=0x12, s43=0x43,
+             char=1)}, {0x365C8: 0, 0x36638: 1}),
+        # f3: +0x42 bit 4 skips the 0x36BC8 test; 0x36638 returns 0 and mode 3 restarts the record
+        # (anim-begin) then returns before the side stream.
+        Case("f3", {"eax": E3_REC}, {**c2b_36870(0, s54=0, s42=0x52, s43=0x43, mode=3, char=1)}),
+        # f4: the same with mode 0: after the restart the side's 0x102900 record starts the 0xE906A
+        # stream.
+        Case("f4", {"eax": E3_REC}, {**c2b_36870(0, s54=0, s42=0x12, s43=0x43, mode=0, char=1)},
+             {0x365C8: 0, 0x36638: 0}),
+        # f5: 0x36638 returns nonzero: no restart.
+        Case("f5", {"eax": E3_REC}, {**c2b_36870(0, s54=0, s42=0x12, s43=0x43)},
+             {0x36638: 1}),
+        # f6: +0x54 = 1: the case-1 arm (mask +0x40, 0x365C8, 0x36638 0, +0x52 = 5 and the
+        # 0xC89A0 stream).
+        Case("f6", {"eax": E3_REC}, {**c2b_36870(0, s54=1, s41=0x41, s42=0x42, s43=0x43, char=1)},
+             {0x365C8: 1, 0x36638: 0}),
+        # f6b: the case-1 miss: +0x43 bit 0x40 cleared instead.
+        Case("f6b", {"eax": E3_REC}, {**c2b_36870(0, s54=1, s41=0x41, s42=0x42, s43=0x43, char=1)},
+             {0x365C8: 0, 0x36638: 1}),
+        # f7: +0x54 = 2: 0x3C520 with the 0xC89F0 stream.
+        Case("f7", {"eax": E3_REC}, {**c2b_36870(0, s54=2, char=1)}),
+        # f8: +0x54 = 4: 0x379C4.
+        Case("f8", {"eax": E3_REC}, {**c2b_36870(0, s54=4)}),
+        # f9: +0x54 = 3: nothing after the shared resets.
+        Case("f9", {"eax": E3_REC}, {**c2b_36870(0, s54=3)}),
+        # f10: +0x54 = 5: the default arm.
+        Case("f10", {"eax": E3_REC}, {**c2b_36870(0, s54=5)}),
+        # f11: side 1 (the record's +0x51 byte selects its own slot): the +0x42 bit-5 arm again.
+        Case("f11", {"eax": E3_REC2}, {**c2b_36870(1, s54=0, s42=0x22, s43=0x43, char=2)}),
+    ], calls=(E.Call(0x385B0, ("eax",)),
+              E.Call(0x39280, ("eax",)),
+              E.Call(0x164E8, ("eax",)),
+              E.Call(0x39040, ("eax",)),
+              E.Call(0x37D18, ("eax", "edx"), clobbers=("edx",)),
+              E.Call(0x365C8, ("eax", "edx", "ebx"), clobbers=("ebx", "edx")),
+              E.Call(0x36BC8, ("eax", "edx"), clobbers=("edx",)),
+              E.Call(0x36638, ("eax", "edx"), clobbers=("edx",)),
+              E.Call(0x2BC30, ("eax", "edx", "s0"), pop=4, clobbers=("edx",)),
+              E.Call(0x3C520, ("eax", "edx", "s0"), pop=4, clobbers=("edx",)),
+              E.Call(0x379C4, ("eax",), clobbers=("ecx", "esi", "edi", "ebp"))),
+       eax_mask=0, mutants=("@mutant", "@arm", "@so", "@case", "@mask")),
+    # 0x2AE14 (record §C2b): EAX = desc, EDX = a2, ECX = a3, EBX = a4, stack = a5. The spawn: alloc
+    # (0x2AC80 stub), the descriptor field copy, the initial animation walk (0x2B2A0 stub, status
+    # 0 loops / 2 -> id 0x1E1, else 0x2A408 stub), the pset writes (0x2A820 stub), the mode-1
+    # cursor (0x2A620 stub, rec+0x28 bit 0x10), the type-callback indirect at 0x2B0E9 (0x5D812
+    # allow for the visible arm; 0x127C0 allow, whose empty-list path is self-contained, for the
+    # invisible one) and 0x1C390/0x1C3A0 (allow: the port's render_list_insert performs both). The
+    # raw's 0x2AE3C overwrites a5's high word with a2; the cases keep them 0, so both sides agree.
+    Spec("actor_spawn", 0x2AE14, [
+        # g0: the alloc fails: return 0.
+        Case("g0", {"eax": C2B_2AE14_DESC, "edx": 2, "ecx": 3, "ebx": 4, "s0": 0},
+             {**c2b_2ae14()}, {0x2AC80: 0}),
+        # g1: frame 5 (the float -1 arm), rec+0x28 bit 0x800 skips the walk (0x2A408), hdl 0 (no
+        # palette), a5 without 0x400 (the +0x4A clear at the end).
+        Case("g1", {"eax": C2B_2AE14_DESC, "edx": 0x2222, "ecx": 0x3333, "ebx": 0x4444, "s0": 0},
+             {**c2b_2ae14(frame=5, dp8=0x0800, a2=0x2222, a3=0x3333, a4=0x4444)},
+             {0x2AC80: C2B_2AE14_REC, 0x2A408: 0x1234, 0x2B2A0: 0}),
+        # g2: the walk's first word has bit 15 clear: no 0x2B2A0 call, the id comes from 0x2A408.
+        Case("g2", {"eax": C2B_2AE14_DESC, "edx": 2, "ecx": 3, "ebx": 4, "s0": 0},
+             {**c2b_2ae14(dp8=0, word1=0x0100)},
+             {0x2AC80: C2B_2AE14_REC, 0x2A408: 0x2222, 0x2B2A0: 0}),
+        # g3: the first word has bit 15 set and the second clears it: one 0x2B2A0 call (status 0)
+        # then the 0x2A408 id.
+        Case("g3", {"eax": C2B_2AE14_DESC, "edx": 2, "ecx": 3, "ebx": 4, "s0": 0},
+             {**c2b_2ae14(dp8=0, word1=0x8000, word2=0x0100)},
+             {0x2AC80: C2B_2AE14_REC, 0x2A408: 0x3333, 0x2B2A0: 0}),
+        # g4: the 0x2B2A0 stub returns 2: id 0x1E1, no 0x2A408 call.
+        Case("g4", {"eax": C2B_2AE14_DESC, "edx": 2, "ecx": 3, "ebx": 4, "s0": 0},
+             {**c2b_2ae14(dp8=0, word1=0x8000)},
+             {0x2AC80: C2B_2AE14_REC, 0x2A408: 0x4444, 0x2B2A0: 2}),
+        # g5: a palette handle: 0x33754 (stub) fills pset+0x18.
+        Case("g5", {"eax": C2B_2AE14_DESC, "edx": 2, "ecx": 3, "ebx": 4, "s0": 0},
+             {**c2b_2ae14(dp8=0x0800, hdl=0x800040, word1=0)},
+             {0x2AC80: C2B_2AE14_REC, 0x2A408: 0x5555, 0x2B2A0: 0, 0x33754: 0xB0B}),
+        # g6: rec+0x28 bit 0x1000 runs the mode-1 cursor after pset_write.
+        Case("g6", {"eax": C2B_2AE14_DESC, "edx": 2, "ecx": 3, "ebx": 4, "s0": 0},
+             {**c2b_2ae14(dp8=0x1800, word1=0)},
+             {0x2AC80: C2B_2AE14_REC, 0x2A408: 0x6666, 0x2B2A0: 0}),
+        # g7: the type callback is the 0x127C0 allow (empty list -> AL 0xFF): the record dies.
+        Case("g7", {"eax": C2B_2AE14_DESC, "edx": 2, "ecx": 3, "ebx": 4, "s0": 0},
+             {**c2b_2ae14(dp8=0x0800, word1=0, cb=0x127C0)},
+             {0x2AC80: C2B_2AE14_REC, 0x2A408: 0x7777, 0x2B2A0: 0}),
+        # g8: a5 bit 0x400: the parent-index branch (parent = pool + (a5&0x7f)*0x68 = pool), the
+        # layer comes from the parent when a3 is 0, and the +0x4A clear is skipped.
+        Case("g8", {"eax": C2B_2AE14_DESC, "edx": 0x2222, "ecx": 0, "ebx": 0x4444, "s0": 0x400},
+             {**c2b_2ae14(a2=0x2222, a4=0x4444, a5=0x400, dp8=0x0800, word1=0),
+              C2B_2AE14_POOL + 0x5A: b"\x5a", C2B_2AE14_POOL + 0x4F: b"\x4f",
+              C2B_2AE14_POOL + 0x49: b"\x49", C2B_2AE14_POOL + 0x28: b"\x28\x02"},
+             {0x2AC80: C2B_2AE14_REC, 0x2A408: 0x8888, 0x2B2A0: 0}),
+        # g10: rec+0x28 bit 0x2000 stores a3 to rec+0x49 instead of rec+0x32.
+        Case("g10", {"eax": C2B_2AE14_DESC, "edx": 2, "ecx": 0x33, "ebx": 4, "s0": 0},
+             {**c2b_2ae14(a3=0x33, dp8=0x2800, word1=0)},
+             {0x2AC80: C2B_2AE14_REC, 0x2A408: 0xAAAA, 0x2B2A0: 0}),
+        # g11: the parent branch with a nonzero layer: rec+0x49 takes a3 (0x2AFEE).
+        Case("g11", {"eax": C2B_2AE14_DESC, "edx": 0x2222, "ecx": 0x5A, "ebx": 0x4444, "s0": 0x400},
+             {**c2b_2ae14(a2=0x2222, a3=0x5A, a4=0x4444, a5=0x400, dp8=0x0800, word1=0),
+              C2B_2AE14_POOL + 0x5A: b"\x5a", C2B_2AE14_POOL + 0x4F: b"\x4f",
+              C2B_2AE14_POOL + 0x49: b"\x49", C2B_2AE14_POOL + 0x28: b"\x28\x02"},
+             {0x2AC80: C2B_2AE14_REC, 0x2A408: 0xBBBB, 0x2B2A0: 0}),
+    ], allow_calls=(0x1C390, 0x1C3A0, 0x127C0, 0x5D812),
+       calls=(E.Call(0x2AC80, ("eax",)),
+              E.Call(0x2B2A0, ("eax", "edx", "ebx"), clobbers=("ebx", "edx")),
+              E.Call(0x2A408, ("eax", "edx"), clobbers=("edx",)),
+              E.Call(0x33754, ("eax",)),
+              E.Call(0x2A820, ("eax", "edx"), clobbers=("edx",)),
+              E.Call(0x2A620, ("eax", "edx"), clobbers=("edx",))),
+       eax_mask=0xFFFFFFFF, mutants=("@mutant", "@minus", "@pal", "@type"),
+       unhit_named={0x2B071: "rec+0x5F is set to 1 at 0x2AF31, so the pset+2 word's zero arm "
+                           "cannot be reached (a dead block in the raw)"}),
+    # 0x2C3FC (record §C2b): EAX = voice id. The record at 0xBBDC8 + id*0xC ({type, h, b}) selects
+    # the arm: 0 no-op, 1 music request, 2 sample queue unless playing, 3 the 0x46/0x4D/0x5D pairs,
+    # 4 the unpause pair, 5 the music_stop/sample_stop sub-id menu on DSD(0x105D5C), >= 6 nothing.
+    # The audio callees are stubbed through their seams (music_stop and samples_stop_all return
+    # through PR_SEAM_RET0, the unpause pair through PR_SEAM0). Mask 0xFF (AL, `mov al,1`).
+    Spec("sound_voice", 0x2C3FC, C2B_VOICE_CASES,
+       calls=(E.Call(0x1CA14, ("eax", "edx"), clobbers=("edx",)),
+              E.Call(0x1CA6C, ()),
+              E.Call(0x1CC28, ("eax", "edx"), clobbers=("edx",)),
+              E.Call(0x1CD9C, ()),
+              E.Call(0x1CE04, ("eax",)),
+              E.Call(0x1CE70, ("eax",)),
+              E.Call(0x1D238, ()),
+              E.Call(0x1D244, ())),
+       eax_mask=0xFF, mutants=("@mutant", "@queue", "@case5", "@stop")),
+    # 0x2B2A0 (record §C2b): EAX = rec, EDX = index, EBX = flag. The animation-opcode dispatcher.
+    # 0x2B8F8 (the operand), 0x2B150 (set_dead), 0x5D7DC (rng), 0x2AE14 (the opcode-0x0C child),
+    # 0x29DB8 (the variable write) and 0x2C3FC (the opcode-0x2E voice) are stubbed through their
+    # seams; 0x2EA64 (a bare `ret`) and the 0x10/0x11/0x15 indirect target 0x29D60 are allowed
+    # (the port's fn_resolve has no entry for it, so both sides do nothing). Mask 0xFF.
+    Spec("spawn_anim_opcode", 0x2B2A0, C2B_OP_CASES,
+       allow_calls=(0x2EA64, 0x29D60, 0x2B8F8, 0x29F34),
+       calls=(E.Call(0x2B150, ("eax",)),
+              E.Call(0x5D7DC, ("eax",)),
+              E.Call(0x2AE14, ("eax", "edx", "ecx", "ebx", "s0"), pop=4,
+                     clobbers=("ebx", "ecx", "edx")),
+              E.Call(0x29DB8, ("eax", "edx", "ebx"), clobbers=("ebx", "edx")),
+              E.Call(0x2C3FC, ("eax",))),
+       eax_mask=0xFF, mutants=("@mutant", "@indirect", "@child", "@skip"),
+       unhit_named={0x2B52F: "the 0x0D jump-table entry: the early 0x0D test at 0x2B2CA returns "
+                           "before the table, so the raw's 0x0D target is dead"}),
+]
+
 SPECS = [
     Spec("rng_next", 0x5D7DC, [
         Case("r1", {"eax": 0x1234}, {DS_RNG: le32(0x12345678)}),
@@ -3451,7 +4227,7 @@ SPECS = [
         Case("d0", {}, {DS_1078FC: b"\x00"}),
         Case("d1", {"eax": U6_REC}, {DS_1078FC: b"\x5a"}),
     ], eax_mask=0),
-] + E3_SPECS + P1_SPECS + P1_ANIM_SPECS + P2_SPECS + P3_SPECS + P6_SPECS + P45_SPECS + C1_SPECS + C2_SPECS + P7_SPECS + P8_SPECS
+] + E3_SPECS + P1_SPECS + P1_ANIM_SPECS + P2_SPECS + P3_SPECS + P6_SPECS + P45_SPECS + C1_SPECS + C2_SPECS + C2B_SPECS + P7_SPECS + P8_SPECS
 
 
 # ---- driver -------------------------------------------------------------------------------------
diff --git a/tools/tests/test_diff_verify.py b/tools/tests/test_diff_verify.py
index d385e36..0b3e67e 100644
--- a/tools/tests/test_diff_verify.py
+++ b/tools/tests/test_diff_verify.py
@@ -879,6 +879,70 @@ P8_KINDS = {
 }
 
 
+# Track P batch C2b (record 2026-10-05-reverse-c2b): the nine deferred callee rows with their EAX
+# masks, and what alone catches each of their mutants.
+C2B_MASKS = {
+            "palette_acquire": 0xffffffff,
+            "effects_spawn": 0x0,
+            "fighter_command_dispatch": 0xff,
+            "fighter_reaction": 0x0,
+            "fighter_39834": 0x0,
+            "fighter_36870": 0x0,
+            "actor_spawn": 0xffffffff,
+            "sound_voice": 0xff,
+            "spawn_anim_opcode": 0xff,
+}
+C2B_KINDS = {
+    "palette_acquire@mutant": {"byte", "eax"},
+    "palette_acquire@new": {"byte", "eax"},
+    "palette_acquire@inc": {"byte"},
+    "palette_acquire@start": {"byte"},
+    "palette_acquire@reflow": {"byte"},
+    "palette_acquire@count": {"byte"},
+    "effects_spawn@mutant": {"byte"},
+    "effects_spawn@count": {"byte"},
+    "effects_spawn@copy": {"byte"},
+    "effects_spawn@lock": {"byte"},
+    "effects_spawn@free": {"byte"},
+    "fighter_command_dispatch@mutant": {"byte", "call #1 memory", "call #2 memory", "call #3 memory"},
+    "fighter_command_dispatch@copy": {"byte", "call #1 memory", "call #2 memory", "call #3 memory"},
+    "fighter_command_dispatch@early": {"call #1", "call #2"},
+    "fighter_command_dispatch@scan": {"byte", "eax", "call #3"},
+    "fighter_command_dispatch@b2": {"byte", "eax", "call #3", "call #3 memory"},
+    "fighter_command_dispatch@force": {"byte", "eax", "call #2"},
+    "fighter_command_dispatch@arm": {"byte", "call #2 memory", "call #3 memory"},
+    "fighter_reaction@mutant": {"byte"},
+    "fighter_reaction@hold": {"call #3", "call #3 memory", "call #4", "call #5", "call #6"},
+    "fighter_reaction@swap": {"byte", "call #0", "call #1", "call #2", "call #2 memory", "call #3",
+                              "call #3 memory", "call #4", "call #4 memory", "call #5", "call #5 memory",
+                              "call #6", "call #6 memory"},
+    "fighter_reaction@early": {"byte", "call #1", "call #2", "call #3", "call #4", "call #5"},
+    "fighter_reaction@efc": {"byte", "call #2", "call #3", "call #4", "call #5", "call #6"},
+    "fighter_reaction@branch": {"byte", "call #3", "call #4", "call #4 memory", "call #5", "call #5 memory", "call #6"},
+    "fighter_39834@mutant": {"call #1"},
+    "fighter_39834@ai": {"byte", "call #3", "call #3 memory", "call #4"},
+    "fighter_39834@arm": {"byte", "call #3", "call #3 memory", "call #4"},
+    "fighter_39834@thr": {"byte"},
+    "fighter_39834@tail": {"call #4"},
+    "fighter_36870@mutant": {"byte", "call #0", "call #0 memory", "call #1", "call #2", "call #3", "call #4", "call #5"},
+    "fighter_36870@arm": {"byte", "call #3", "call #4", "call #5"},
+    "fighter_36870@so": {"byte", "call #2 memory", "call #3 memory", "call #4 memory", "call #5 memory"},
+    "fighter_36870@case": {"call #4"},
+    "fighter_36870@mask": {"byte", "call #1 memory", "call #2 memory", "call #3 memory", "call #4 memory", "call #5 memory"},
+    "actor_spawn@mutant": {"byte", "call #2", "call #2 memory", "call #3", "call #3 memory", "call #4"},
+    "actor_spawn@minus": {"byte", "call #1", "call #1 memory", "call #2", "call #2 memory", "call #3", "call #3 memory", "call #4"},
+    "actor_spawn@pal": {"byte", "call #2", "call #2 memory", "call #3", "call #3 memory", "call #4"},
+    "actor_spawn@type": {"byte"},
+    "sound_voice@mutant": {"byte", "eax", "call #0", "call #1"},
+    "sound_voice@queue": {"call #1"},
+    "sound_voice@case5": {"call #0"},
+    "sound_voice@stop": {"eax", "call #1"},
+    "spawn_anim_opcode@mutant": {"byte"},
+    "spawn_anim_opcode@indirect": {"eax"},
+    "spawn_anim_opcode@child": {"call #0"},
+    "spawn_anim_opcode@skip": {"byte"},
+}
+
 @needs_unicorn
 @unittest.skipUnless((os.path.exists(DIFFRUN) and os.path.exists(EXE)) or REQUIRED,
                      "build/diffrun or PRAGE.EXE absent")
@@ -903,7 +967,7 @@ class RealFunctionTests(unittest.TestCase):
                                              "fighter_slot_flag", "hit_anim_ctx", "hit_anim_start_b",
                                              "host_1b890", "rng_next"] + list(P1_MASKS) + list(P2_MASKS)
                                             + list(P3_MASKS) + list(P45_MASKS) + list(C1_MASKS)
-                                            + list(P6_MASKS) + list(C2_MASKS) + list(P7_MASKS) + list(P8_MASKS)))
+                                            + list(P6_MASKS) + list(C2_MASKS) + list(C2B_MASKS) + list(P7_MASKS) + list(P8_MASKS)))
         for name, r in self.real.items():
             if name == "host_1b890":       # the named gap (record E3 §E3.8), tested on its own below
                 continue
@@ -919,7 +983,7 @@ class RealFunctionTests(unittest.TestCase):
             "fighter_45878@mutant", "fighter_ctx_same@mutant", "fighter_slot_flag@mutant",
             "hit_anim_ctx@mutant", "hit_anim_start_b@mutant", "hit_anim_start_b@set", "rng_next@mutant"]
             + list(P1_KINDS) + list(P2_KINDS) + list(P3_KINDS) + list(P45_KINDS) + list(C1_KINDS)
-            + list(P6_KINDS) + list(C2_KINDS) + list(P7_KINDS) + list(P8_KINDS)))
+            + list(P6_KINDS) + list(C2_KINDS) + list(C2B_KINDS) + list(P7_KINDS) + list(P8_KINDS)))
         for name, r in self.mut.items():
             self.assertEqual(r.verdict, "MISMATCH", name)
 
@@ -984,7 +1048,7 @@ class RealFunctionTests(unittest.TestCase):
             "fighter_3640c": 0, "fighter_37dcc": 0,
             "fighter_23130": 0xFF, "fighter_45878": 0, "anim_10fa8": 0, "anim_3e4e4": 0,
             "fighter_ctx_same": 0, "hit_anim_ctx": 0, "hit_anim_start_b": 0, "host_1b890": 0xFFFFFFFF,
-            **P1_MASKS, **P2_MASKS, **P3_MASKS, **P45_MASKS, **C1_MASKS, **P6_MASKS, **C2_MASKS,
+            **P1_MASKS, **P2_MASKS, **P3_MASKS, **P45_MASKS, **C1_MASKS, **P6_MASKS, **C2_MASKS, **C2B_MASKS,
             **P7_MASKS, **P8_MASKS})
         # with the full mask the slot-flag original's scratch bits (case f9: EAX = 0x201) differ
         spec = dataclasses.replace([s for s in V.SPECS if s.name == "fighter_slot_flag"][0],
@@ -1375,6 +1439,64 @@ class RealFunctionTests(unittest.TestCase):
         ):
             self.assertEqual(sorted({p.split(":")[0] for p in self.mut[name].problems}), ids, name)
 
+    def test_each_c2b_mutant_is_caught_by_what_it_breaks(self):
+        # track P batch C2b (record 2026-10-05-reverse-c2b): what alone catches each mutant; the
+        # rows with callees have mutants caught only by the call list or the memory at a call
+        for name, want in C2B_KINDS.items():
+            got = {p.split(": ", 1)[1].split(":")[0] if p.split(": ", 1)[1].startswith("call #")
+                   else p.split(": ", 1)[1].split(" ")[0] for p in self.mut[name].problems}
+            self.assertEqual(got, want, name)
+        # the exact case set that alone catches each mutant (measured on the prototype)
+        for name, ids in (
+        ("palette_acquire@mutant", ['p0', 'p1', 'p2', 'p3', 'p4', 'p5', 'p6']),
+        ("palette_acquire@new", ['p0', 'p1', 'p2', 'p3', 'p4', 'p5', 'p6']),
+        ("palette_acquire@inc", ['p1', 'p4']),
+        ("palette_acquire@start", ['p0', 'p2', 'p3', 'p5', 'p6']),
+        ("palette_acquire@reflow", ['p2']),
+        ("palette_acquire@count", ['p0', 'p2', 'p3', 'p5', 'p6']),
+        ("effects_spawn@mutant", ['f0', 'f1', 'f2', 'f4']),
+        ("effects_spawn@count", ['f2', 'f4']),
+        ("effects_spawn@copy", ['f0', 'f4']),
+        ("effects_spawn@lock", ['f0', 'f1', 'f2', 'f4']),
+        ("effects_spawn@free", ['f0', 'f1', 'f2', 'f4']),
+        ("fighter_command_dispatch@mutant", ['c0', 'c1', 'c10', 'c11', 'c12', 'c13', 'c14', 'c2', 'c3', 'c4', 'c5', 'c6', 'c7', 'c8', 'c9']),
+        ("fighter_command_dispatch@copy", ['c0', 'c1', 'c10', 'c11', 'c12', 'c13', 'c14', 'c2', 'c3', 'c4', 'c5', 'c6', 'c7', 'c8', 'c9']),
+        ("fighter_command_dispatch@early", ['c4', 'c5']),
+        ("fighter_command_dispatch@scan", ['c14']),
+        ("fighter_command_dispatch@b2", ['c13', 'c2', 'c5']),
+        ("fighter_command_dispatch@force", ['c8']),
+        ("fighter_command_dispatch@arm", ['c2', 'c3', 'c6', 'c7', 'c8']),
+        ("fighter_reaction@mutant", ['r4']),
+        ("fighter_reaction@hold", ['r3', 'r4', 'r5', 'r6', 'r7', 'r8']),
+        ("fighter_reaction@swap", ['r0', 'r1', 'r2', 'r3', 'r4', 'r5', 'r6', 'r7', 'r8']),
+        ("fighter_reaction@early", ['r1', 'r3', 'r8']),
+        ("fighter_reaction@efc", ['r2', 'r4', 'r5', 'r6', 'r7']),
+        ("fighter_reaction@branch", ['r3', 'r4', 'r5', 'r6', 'r7', 'r8']),
+        ("fighter_39834@mutant", ['f1', 'f2', 'f5']),
+        ("fighter_39834@ai", ['f3']),
+        ("fighter_39834@arm", ['f2']),
+        ("fighter_39834@thr", ['f5']),
+        ("fighter_39834@tail", ['f5']),
+        ("fighter_36870@mutant", ['f0']),
+        ("fighter_36870@arm", ['f2']),
+        ("fighter_36870@so", ['f1', 'f10', 'f11', 'f2', 'f2b', 'f3', 'f4', 'f5', 'f6', 'f6b', 'f7', 'f8', 'f9']),
+        ("fighter_36870@case", ['f6']),
+        ("fighter_36870@mask", ['f1', 'f10', 'f11', 'f2', 'f2b', 'f3', 'f4', 'f5', 'f6', 'f6b', 'f7', 'f8', 'f9']),
+        ("actor_spawn@mutant", ['g4']),
+        ("actor_spawn@minus", ['g2', 'g3', 'g4']),
+        ("actor_spawn@pal", ['g1', 'g10', 'g11', 'g2', 'g3', 'g4', 'g5', 'g6', 'g7', 'g8']),
+        ("actor_spawn@type", ['g7']),
+        ("sound_voice@mutant", ['v100', 'v100b']),
+        ("sound_voice@queue", ['t2b']),
+        ("sound_voice@case5", ['t5_3f_0']),
+        ("sound_voice@stop", ['t2a', 't2b']),
+        ("spawn_anim_opcode@mutant", ['o06a']),
+        ("spawn_anim_opcode@indirect", ['o15']),
+        ("spawn_anim_opcode@child", ['o0ca', 'o0cb']),
+        ("spawn_anim_opcode@skip", ['o2db']),
+        ):
+            self.assertEqual(sorted({p.split(":")[0] for p in self.mut[name].problems}), ids, name)
+
     def test_each_p7_mutant_is_caught_by_what_it_breaks(self):
         # track P batch 7 (record 2026-10-04-reverse-p7): what alone catches each mutant
         for name, want in P7_KINDS.items():
@@ -1393,31 +1515,37 @@ class RealFunctionTests(unittest.TestCase):
         # Call.clobbers, re-derived from the bytes (record §E3.5's table, §E3.12)
         img = E.Image.load(os.path.join(self.tmp.name, "image.bin"))
         stubs = {k.addr: k.clobbers for s in V.SPECS for k in s.calls if k.mode == "stub"}
-        self.assertEqual(stubs, {0x2C3FC: (), 0x2BC30: ("edx",), 0x3C4CC: ("edx",), 0x3C480: ("edx",),
-                                 0x2AE14: ("ebx", "ecx", "edx"), 0x29C08: ("edx",), 0x1A570: (), 0x2A17C: ("edx",),
-                                 0x188AC: ("edx",), 0x38034: (), 0x34D8C: (),
-                                 0x18C14: ("ebx", "edx", "ebp"), 0x18AF8: ("ebx", "ecx", "edx"),
-                                 0x39834: ("edx", "ebp"), 0x39A10: ("edx",), 0x3C208: ("edx",), 0x3C358: (),
-                                 0x22404: (), 0x36870: ("esi", "edi", "ebp"), 0x35838: ("ebx", "edx"),
-                                 0x3B298: ("edx", "edi", "ebp"), 0x39FB0: (), 0x3A95C: ("edx",),
-                                 0x3C190: ("edx",), 0x3B714: ("edx",), 0x48170: (), 0x3C148: (), 0x468D8: (),
-                                 0x36D98: (), 0x188DC: ("edx",), 0x3C16C: (),
-                                 0x186D0: (), 0x18714: (), 0x187FC: (), 0x1883C: ("ebx", "edx"),
-                                 0x189FC: (), 0x18A4C: (), 0x18B04: (), 0x18B44: (),
-                                 0x1DDF4: ("ebx", "edx"), 0x2A408: ("edx",), 0x2B2A0: ("ebx", "edx"),
-                                 0x33754: (), 0x33864: (), 0x36638: ("edx",), 0x39EFC: (),
-                                 0x39F40: ("ebx", "edx"), 0x3B8D8: ("edx",), 0x3B90C: ("edx",),
-                                 0x2AD40: ("edx", "edi", "ebp"),
-                                 0x23960: (),
-                                 0x3A9D8: ("edx",),
-                                 0x2B150: ("esi", "edi", "ebp"),
-                                 0x41310: (), 0x49444: ("edi", "ebp"),
-                                 0x39280: (), 0x13244: (), 0x2A148: ("edx",), 0x2BCF4: ("edx",),
-                                 0x1890C: ("edx",), 0x29BC8: ("ebx", "edx"), 0x37D18: ("edx",),
-                                 0x13C70: ("ebx", "edx"), 0x3AA54: (),
-                                 0x13DF0: (), 0x18540: (), 0x18350: ("edx",), 0x18788: (), 0x1881C: (),
-                                 0x1A5AC: (), 0x38154: (), 0x249B0: (), 0x249D0: (),
-                                 0x2B150: ("esi", "edi", "ebp"), 0x29F34: ("edx",)})
+        # 0x2B150/0x2BD44/0x3B298 carry the caller-observed sets (record §C2b.3): the byte-derived
+        # transitive scan over-approximates the Watcom callee-saved registers their callers keep
+        # live (E.callee_clobbers' _CALLEE_CLOBBER_FIXES).
+        self.assertEqual(stubs, {0x13244: (), 0x13C70: ("ebx", "edx"), 0x13DF0: (), 0x164E8: (),
+                                 0x18350: ("edx",), 0x18540: (), 0x186D0: (), 0x18714: (), 0x18788: (),
+                                 0x187FC: (), 0x1881C: (), 0x1883C: ("ebx", "edx"), 0x188AC: ("edx",),
+                                 0x188DC: ("edx",), 0x1890C: ("edx",), 0x189FC: (), 0x18A4C: (),
+                                 0x18AF8: ("ebx", "ecx", "edx"), 0x18B04: (), 0x18B44: (),
+                                 0x18C14: ("ebx", "edx", "ebp"), 0x1A570: (), 0x1A5AC: (), 0x1A734: (),
+                                 0x1AB5C: ("ebp",), 0x1CA14: ("edx",), 0x1CA6C: (), 0x1CC28: ("edx",),
+                                 0x1CD9C: (), 0x1CE04: (), 0x1CE70: (), 0x1D238: (), 0x1D244: (),
+                                 0x1DDF4: ("ebx", "edx"), 0x22404: (), 0x23960: (), 0x249B0: (), 0x249D0: (),
+                                 0x29BC8: ("ebx", "edx"), 0x29C08: ("edx",), 0x29DB8: ("ebx", "edx"),
+                                 0x29F34: ("edx",), 0x2A148: ("edx",), 0x2A17C: ("edx",), 0x2A408: ("edx",),
+                                 0x2A620: ("edx",), 0x2A820: ("edx",), 0x2AC80: (), 0x2AD40: ("edx", "edi", "ebp"),
+                                 0x2AE14: ("ebx", "ecx", "edx"), 0x2B150: (), 0x2B2A0: ("ebx", "edx"),
+                                 0x2BC30: ("edx",), 0x2BCF4: ("edx",), 0x2BD44: ("edx",), 0x2C3FC: (),
+                                 0x33754: (), 0x33864: (), 0x34D8C: (), 0x35838: ("ebx", "edx"),
+                                 0x365C8: ("ebx", "edx"), 0x36638: ("edx",), 0x36870: ("esi", "edi", "ebp"),
+                                 0x36BC8: ("edx",), 0x36CE4: (), 0x36D98: (), 0x379C4: ("ecx", "esi", "edi", "ebp"),
+                                 0x37D18: ("edx",), 0x38034: (), 0x38154: (), 0x385B0: (), 0x39040: (),
+                                 0x39280: (), 0x392A0: ("ebx", "edx"), 0x39738: ("edx",),
+                                 0x39834: ("edx", "ebp"), 0x39A10: ("edx",), 0x39EFC: (), 0x39F40: ("ebx", "edx"),
+                                 0x39FB0: (), 0x3A95C: ("edx",), 0x3A9D8: ("edx",), 0x3AA54: (),
+                                 0x3AAFC: ("ebx", "ecx", "edx", "edi", "ebp"), 0x3AD98: ("edx",), 0x3AE9C: ("edx",),
+                                 0x3B080: ("ebx", "ecx", "edx"), 0x3B134: ("ebx", "edx", "edi", "ebp"),
+                                 0x3B298: ("edx",), 0x3B6C4: (), 0x3B714: ("edx",), 0x3B8D8: ("edx",),
+                                 0x3B90C: ("edx",), 0x3C148: (), 0x3C16C: (), 0x3C190: ("edx",),
+                                 0x3C208: ("edx",), 0x3C358: (), 0x3C480: ("edx",), 0x3C4CC: ("edx",),
+                                 0x3C520: ("edx",), 0x3C59C: ("edx",), 0x41310: (), 0x46460: ("edx",),
+                                 0x468D8: (), 0x48170: (), 0x49444: ("edi", "ebp"), 0x4F434: (), 0x5D7DC: ()})
         for addr, declared in stubs.items():
             self.assertEqual(E.callee_clobbers(img, addr), declared, hex(addr))
 
@@ -1512,8 +1640,8 @@ class RealFunctionTests(unittest.TestCase):
                          "--self-check"])
         self.assertEqual(rc, 0)
         # the closed-row count is over the rows that have callees (158), the 34 without are counted apart
-        self.assertIn("diff-verify: 192/192 functions VERIFIED; 552/552 mutants detected; 1 named gaps; "
-                      "69/158 rows with callees closed (34 have none).", out.getvalue())
+        self.assertIn("diff-verify: 201/201 functions VERIFIED; 598/598 mutants detected; 1 named gaps; "
+                      "148/167 rows with callees closed (34 have none).", out.getvalue())
 
 
 # ---- E3: the call list, named gaps, the callee column (record 2026-10-01-reverse-e3 §E3.4, §E3.8) --
```

**Applied-tree note (closeout, 2026-10-06).** The embedded diff quotes the prototype's
`tools/diff_emu.py` hand-correction wording ("0x3B834 reads EDI after 0x3B298"). The review fix wave
`297000c` corrected the applied comment: the EDI read is *before* the `0x3B834` call into `0x3AE9C`
— `mov eax,edi` at `0x3B82E` (record §C2b.8; `tools/diff_emu.py` carries the final text). The diff
otherwise applies as written.
