# C1: the verification-only callee rows (track P, batch C1) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a differential-verification row for each ported callee the P1-P3 rows stub (the 26 of record §C1.1), so the dependent rows close: the ported function the row stubs gets its own `Spec`, binding, mutants and, where needed, a `PR_SEAM` first statement; every row's cases cover its blocks with seeded sentinels and neighbours; the self-check counter, the mutant case-set table and the E2 table are reconciled. Verification only: no ported function, no `fn_register`, no E2 move.

**Architecture:** Each row is a `Spec` in `tools/diff_verify.py` (`C1_SPECS`), a `b_*` binding and `m_*` mutants in `port/tests/diff_runner.c` (`k_bindings`), and exact-set assertions in `tools/tests/test_diff_verify.py` (`C1_MASKS`, `C1_KINDS`, the case-set table, the clobber table, the counter line). A callee a row stubs gets `PR_SEAM`/`PR_SEAM_RET`/`PR_SEAM_RET0` as its first statement (E3 §E3.10); a callee proven by its own check runs `mode="real"` (E3 §E3.4); a leaf whose arguments cannot be compared runs `allow`. `PR_SEAM_RET0` is new in `mem.h` (the `PR_SEAM0` shape with a value). Four raw-over-plan corrections were found and are baked into the patch (record §C1.2): `0x18AF8`'s second entry is a fall-through (the wrapper split + the real call from `0x3C208`), `0x18AF8` preserves EBX/ECX/EDX (no clobbers), `0x39A10`'s 0x80 case lands outside the image (dropped), `0x18C14`'s flag 7 reads ctx[3] (and the flags buffer is copied back to mem).

**Tech Stack:** C11 (`port/src`, `port/tests`), Python 3.12 (`unittest`), `unicorn` 2.1.4 and `capstone` 5.0.7 (`tools/requirements-diff.txt`), CMake, `make`.

**Spec:** `docs/superpowers/specs/2026-09-30-reverse-completion-design.md` §4 track P, §5.1-§5.3, §6 ("each ported function has a verification row; unhit blocks and non-emulable instructions named; counters and README percentage updated"), §7.

**Derivation record:** `docs/superpowers/plans/2026-10-03-reverse-c1-derivations.md` (§C1.1 the 26 unverified callees from the final table, §C1.2 the 20 rows, their cases, seams and the four corrections, §C1.3 the mutants and their measured catching sets, §C1.4 the counters, §C1.5 the six deferred C1b rows with their seam lists, §C1.6 decisions and limits, §C1.7 what the planner ran). Recipe: `2026-10-01-reverse-e3-derivations.md` §E3.10; checklist: `.superpowers/sdd/2026-10-01-plans/p-track-review-checklist.md`.

**What the planner ran (scratch, 2026-10-03, on `reverse-c1` at `main` `f5b5556`; image sha1 `ff3b8cb14e00f1c282de7b7e15dcd7c230766947`):** the baseline (Task 1) and a full prototype: the patch below applied to the clean worktree, the six stages each measured with `--function NAME --self-check`, the fix rounds of record §C1.2, then `python3 -m unittest tools.tests.test_diff_verify` (98 tests OK), `make diff-verify` (`98/98 functions VERIFIED; 219/219 mutants detected; 1 named gaps; 34/78 rows with callees closed (20 have none)`), `make entry-triage` (byte-identical: `288 unported, 207 ported; supplement 131 (9 unported, 0 stale); untrusted 30`; voice `28 / 87 / 19`) and `PR_ORACLE_REQUIRED=1 ./build/run_tests` (all checks passed). Every expected output below is the prototype's measured output. The prototype was then reverted (`git checkout -- port tools`); the patch is the exact diff it applied.

**Re-baseline note.** The counters below are `f5b5556`'s. If `main` moves before this plan executes, Task 1 records the measured base and every later expected counter adds this plan's increments: functions +20, mutants +61, closed rows +23, rows with callees +14, rows without callees +6. The E2 table must not move (no ported function, no `fn_register`): if a task regenerates it, the line must be byte-identical.

## Decisions needed from the user

**None.** The scope is the raw's: the 26 unverified callees the final table names (record §C1.1). The six C1b rows are deferred with their evidence (record §C1.5) — a scope call the planner made after the prototype reached 20 rows; the plan's Task 5 names them and Task 4 keeps their dependent rows open with the reason. The four corrections of record §C1.2 are raw-over-plan, recorded with their addresses.

## The C1 roadmap

| task | what | commit |
|---|---|---|
| 1 | baseline on `f5b5556` (no commit) | - |
| 2 | the 20 rows, their seams, bindings, mutants and the test exact-set updates | `tools:`/`tests:` |
| 3 | the review sweep (stores, clobbers, mutant case sets) — the prototype's fix rounds | `tests:` |
| 4 | closure: PROGRESS, the record/PROGRESS counters, the final gates | `docs:` |
| 5 | C1b: the six deferred rows (2C3FC, 2AE14, 36870, 39834, 3B298, 3B714) — a follow-on plan | - |

## Global Constraints

Copied verbatim from their sources:

- AGENTS.md: "**Never ship a fitted constant.** Every value is derived from the raw bytes or a capture, with the address that proves it. A value that cannot be pinned is a **named gap with its evidence** — never a plausible-looking number." / "**On any plan-vs-raw conflict the raw wins.** Record the correction and the address, in the derivation record and the report."
- Spec §8: "`make verify` is the gate after every task." User ruling (2026-10-01, speed-up): the per-task gate is the task's tests, `make diff-verify`, `make entry-triage` and the gp oracles whose miss sets the task touches; the full `make verify` runs at the baseline, the final task and before the merge. Here the task-scoped gates only (the brief): `make diff-verify`, `make entry-triage`, `PR_ORACLE_REQUIRED=1 ./build/run_tests`, and `python3 -m unittest tools.tests.test_diff_verify`.
- AGENTS.md: "**The E2 target list is a gate** ... A commit that ports a target (a new `/* 0xADDR` header or `fn_register`) regenerates the table in the same commit (decision D3)". This plan ports nothing: the table must be byte-identical.
- AGENTS.md: "One C function per original function, header comment `/* 0xADDR — spec section */`." / "Mark deliberate deviations `/* PORT: ... */`; doubts `/* TODO(verify): ... */`. No other comment styles in `port/src`." / "Code addresses stored in data go through `fn_origin()` / `fn_resolve()`." / "**`port/src/symbols.h` is generated** ... Never hand-edit it; where the generator emits no name, use a local `#define` with the raw address."
- AGENTS.md: "A ported function that a differentially verified function stubs or records ... opens with `PR_SEAM(0xADDR, args...)` (void) or `PR_SEAM_RET(0xADDR, args...)` ... The arguments are the C signature's, in order (a pointer into `mem[]` as its offset)." Record E3 §E3.8: "A seam must be the callee's first statement".
- AGENTS.md: "Only `CHECK(cond,msg)` and `CHECK_EQ_INT(a,b)`." / "**Assertions must be able to fail.**" / "Consolidating must not change an assertion": this plan extends the exact-set assertions of `RealFunctionTests` (rows, masks, mutant names, stub clobbers, the counter line) and relaxes one assertion for a row with a named-unhit block (`hit == total` -> `unhit == [] and hit <= total`), because `unhit_named` is spec §5.2's feature; no other assertion changes.
- AGENTS.md: "`data/` is git-ignored and read-only — never write to it." / "Commit style: `<area>: <what changed>`. **Never `git add -A`** — stage named files." No `Co-Authored-By` trailer (this is not a Claude session). Never run `make gp-capture`.
- Spec §5.3 / E3 §E3.4: "VERIFIED" is equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.

## Review Focus

1. **Observable stores and neighbours** (checklist 1-2). Every field a row writes carries a sentinel that differs from what it writes; every neighbour byte is seeded. The prototype's rows follow this; Task 3 re-checks with a store sweep.
2. **Inputs that separate alternatives** (checklist 3). `rec+0x51` takes 0, 1 and (where observable) 0x80; side 0 and 1; own vs other slot seeded differently; the stub EAX varies per case; signed bounds and exact boundaries. The `C1_KINDS` case-set table pins the case that alone catches each mutant.
3. **Every call observed** (checklist 5). Each row with a callee has at least one mutant caught only by the call list or the memory at a call (record §C1.3).
4. **Seams** (checklist 9). First statement; args = entry registers = C signature order = `E.Call`; clobbers re-derived (`test_each_stub_declares_the_registers_its_callee_clobbers` extended); no existing row changes (the full self-check at Task 2's gate: 98/98, 219/219).
5. **The two corrections with wide blast radius.** `0x18AF8`'s wrapper split must keep every P2 row VERIFIED (the seam is on the wrapper; the body is the same code) and `0x3C208` must run it real with `allow_calls=(0x33950,)` and the `18714`/`18B04` stubs. The clobber table's `0x18AF8` entry stays the image's (`("ebx", "ecx", "edx")`) because the P2 stub is the only `stub`-mode declaration; `0x3C208`'s real call declares none.
6. **E2 and the counters.** `make entry-triage` byte-identical; `port_progress.py` `771 1203 64` / `731 731 100`; the counter line in the test file is the measured `98/98 ... 219/219 ... 34/78 (20 have none)`.

## Where to run

The worktree `.worktrees/reverse-c1` (branch `reverse-c1`). Every command runs from the worktree root; `data/` and `.superpowers/` are symlinks; `build/` exists. Before Task 1:

```bash
cd /Users/felipe.dos.santos/code/mine/primal-rage-reverse/.worktrees/reverse-c1
git rev-parse HEAD   # f5b5556
python3 -c "import unicorn, capstone; print(unicorn.__version__, capstone.__version__)"  # 2.1.4 5.0.7
cmake -S port -B build && cmake --build build 2>&1 | tail -1
./build/diffrun --exe data/game/C/PRAGE.EXE --image-out /tmp/pr_c1_img.bin && shasum /tmp/pr_c1_img.bin
```

The last line must print `ff3b8cb14e00f1c282de7b7e15dcd7c230766947`; another hash means the image differs from the one the record measured: stop. Ledger: `.superpowers/sdd/2026-10-03-reverse-c1-callee-rows/progress.md`.

**How the code steps are written.** Task 2's patch is the prototype's exact `git diff` (the planner applied it, ran every gate, and reverted). Apply it with `git apply`; it touches only `port/src/mem.h`, `port/src/game/{actors.c,actors.h,fighter.c,fighter.h}`, `port/tests/diff_runner.c`, `tools/diff_verify.py`, `tools/tests/test_diff_verify.py`. If `main` moved, `git apply --3way` and resolve by keeping the patch's additions at the same anchors; never merge the E2 table by hand.

## File Structure

| File | Responsibility |
|---|---|
| `port/src/mem.h` | `PR_SEAM_RET0` (a no-argument returning callee's seam) |
| `port/src/game/fighter.c`, `fighter.h` | the 18 new seams, the `hit_facing_flag` wrapper split, the `static` removals (`fighter_18bd4`, `ai_distance`, `fighter_1883c`, `fighter_3b8d8`, `fighter_18b44`, `fighter_189fc`, `fighter_18a4c`) and their declarations |
| `port/src/game/actors.c`, `actors.h` | the seams of `palette_release`, `palette_acquire`, `spawn_anim_opcode`, `anim_next_sprite_id` and their `static` removals |
| `port/tests/diff_runner.c` | the 20 bindings and 62 mutants (`k_bindings`) |
| `tools/diff_verify.py` | `C1_SPECS`, the C1 callee declarations, the two shared case generators (`c1_18c14_cases`) |
| `tools/tests/test_diff_verify.py` | `C1_MASKS`, `C1_KINDS`, the case-set table, the clobber table, the counter line, the relaxed `hit` assertion |
| `docs/superpowers/plans/2026-10-03-reverse-c1-derivations.md` | the record (this plan's source of values) |
| `docs/PROGRESS.md` | Task 4's paragraph |

---

### Task 1: Baseline (no commit)

**Files:** none. **Interfaces:** consumes the worktree at `f5b5556`; produces the baseline counters and table.

- [ ] **Step 1: the task gates on the untouched tree.**

```bash
make diff-verify DIFF_IMAGE=/tmp/pr_c1_base.bin DIFF_TABLE=/tmp/pr_c1_base_table.md 2>&1 | tail -3
make entry-triage E2_IMAGE=/tmp/pr_c1_e2.bin 2>&1 | tail -3
python3 tools/port_progress.py
```

Expected (measured at `f5b5556`):

```
diff-verify: 78/78 functions VERIFIED; 158/158 mutants detected; 1 named gaps; 11/64 rows with callees closed (14 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
entry-triage: targets 288 unported, 207 ported; supplement 131 (9 unported, 0 stale); untrusted entries 30
entry-triage: voice sites outside Ghidra 134: 28 in unported code, 87 in ported code, 19 nowhere
771 1203 64
731 731 100 (portable: excludes 81 host-owned/deferred and runtime >= 5D000)
```

- [ ] **Step 2: enumerate the unverified set from the table.** `python3` over `/tmp/pr_c1_base_table.md`: every callee marked `unverified` (record §C1.1). Expected: the 26 addresses of record §C1.1; `0x3C4CC` is VERIFIED (its own E3 row).

### Task 2: the 20 rows (one commit)

**Files:** the patch below. **Interfaces:** produces `C1_SPECS`, the `C1_*` call declarations, `PR_SEAM_RET0`, the seams and the test exact-set updates; consumes the E3/P1/P2/P3 fixtures (`E3_REC`, `E3_REC2`, `E3_OUT`, `E3_SLOT`, `DS_SLOTS`, `SLOT_PTRS`, `BIT15`, `ANCHOR`, `ANCHORX`, `ANIM_BEGIN`, `DISPATCH`, ...).

- [ ] **Step 1: apply the patch.**

```bash
git apply - <<'PATCH'
diff --git a/port/src/game/actors.c b/port/src/game/actors.c
index 84d35e5..1a47fdc 100644
--- a/port/src/game/actors.c
+++ b/port/src/game/actors.c
@@ -1075,6 +1075,7 @@ u32 actor_pset(u32 rec)
  * full table is a fatal error; the port returns 0 rather than exiting. */
 u32 palette_acquire(u32 handle)
 {
+    PR_SEAM_RET(0x33754u, handle);
     const u32 *res = res_resolve(handle);           /* 0x1B544 */
     u32 count = res ? *res : 0;
     u32 e = DS_00107618;
@@ -1300,6 +1301,7 @@ static u32 anim_operand(u32 rec)
  * 0x2A4A7 (`mov cx,[edx]`), which is the keep-current-id arm. */
 u32 anim_next_sprite_id(u32 rec, u32 pset)
 {
+    PR_SEAM_RET(0x2A408u, rec, pset);
     u32 res;
     if ((DSW(rec + 0x28) >> 8 & 8u) != 0) {
         res = DSW(rec + 8);
@@ -2168,8 +2170,9 @@ void actors_pin_anim_tick_zero(int on) { anim_tick_zero = on; }
  * that anim_operand stores back into DS_00105BE4. `flag` is the original's EBX
  * on entry: 0x2AE14 passes 1, 0x2AA70 and 0x2BC30 pass 0; only opcode 0 reads
  * it. Returns 0 to keep walking, 1 to stop (opcode 0x0D) and 2 for death. */
-static u32 spawn_anim_opcode(u32 rec, u32 index, u32 flag)
+u32 spawn_anim_opcode(u32 rec, u32 index, u32 flag)
 {
+    PR_SEAM_RET(0x2B2A0u, rec, index, flag);
     u32 p = DSD(rec + 8);
     DSW(DS_00105BE4) = (u16)((DSW(p) >> 8) & 0x1fu);
     if ((u8)DSW(DS_00105BE4) == 0x0du) return 1;
@@ -2810,8 +2813,9 @@ stream_walk:;
 }
 
 /* 0x33864: drop a palette-table reference; clear the handle at 0 at zero. */
-static void palette_release(u32 entry)
+void palette_release(u32 entry)
 {
+    PR_SEAM(0x33864u, entry);
     u32 ref = DSD(entry + 4);
     DSD(entry + 4) = ref - 1;
     if (ref - 1 == 0) DSD(entry) = 0;
diff --git a/port/src/game/actors.h b/port/src/game/actors.h
index f80be67..43ab729 100644
--- a/port/src/game/actors.h
+++ b/port/src/game/actors.h
@@ -111,6 +111,8 @@ void palette_reflow(u32 descriptor, u32 handle);
  * following word or uses it to index a table. The returned bit 0x8000 is the
  * stream's bit XOR the record's `rec+0x28 >> 8 & 0x40` flip. */
 u32  anim_next_sprite_id(u32 rec, u32 pset);
+u32  spawn_anim_opcode(u32 rec, u32 index, u32 flag);
+void palette_release(u32 entry);
 /* 0x29F34. Read an animation variable: `op & 0x7F` selects the 0x40-word ring
  * at DS_00105B4C (< 0x40), the record's own bytes (0x40..0x45), the parent
  * rec+0x4A's bytes (0x46..0x4B) or the child rec+0x4B's bytes (0x4C..0x51). */
diff --git a/port/src/game/fighter.c b/port/src/game/fighter.c
index 356a78c..4f8eec7 100644
--- a/port/src/game/fighter.c
+++ b/port/src/game/fighter.c
@@ -47,11 +47,11 @@ static void fighter_164e8(u32 side);                     /* 0x164E8 */
 static void hit_stance_timer(u32 side);                  /* 0x1922C */
 static int fighter_3962c(u32 side, u32 param_2);         /* 0x3962C */
 static int fighter_396ac(u32 side, u32 param_2);         /* 0x396AC */
-static void fighter_18b44(u32 slot);                     /* 0x18B44 */
+void fighter_18b44(u32 slot);                           /* 0x18B44 */
 static void fighter_39278(u32 v);                        /* 0x39278 */
 static u32 fighter_36d20(u32 slot);                      /* 0x36D20 */
 static void fighter_2bd44_by_index(u32 rec);             /* 0x3B4D4 = 0x3B844 */
-static void fighter_18bd4(u8 flags[16]);                 /* 0x18BD4 */
+void fighter_18bd4(u8 flags[16]);                        /* 0x18BD4 */
 static u8 hit_3d004(u32 side);                            /* 0x3D004 */
 
 /* PORT: the register shape of the slot callbacks 0x34E2C (0x35045) and
@@ -249,6 +249,7 @@ static void fighter_18350(u32 side, u32 anchor)
 /* 0x186D0. The slot position latch (the game_frame tail's 0x25438 call). */
 void fighter_slot_latch(u32 side)
 {
+    PR_SEAM(0x186D0u, side);
     u32 slot = DS_001077B0 + side * 0x94u;
     u32 rec = DSD(slot);                            /* 0x186F3/0x18702 */
     if ((DSB(slot + 0x42u) & 0x08u) == 0) {         /* 0x186E6 */
@@ -777,8 +778,9 @@ static u32 fighter_input_mask(u32 side)
 
 /* 0x39F40. Arm a pose: seed the four per-side pose words, set the slot's state
  * bytes and its +0x10 handler, and zero the record's +0x24. */
-static void fighter_pose_start(u32 side, u32 edx, u32 ebx, u32 ecx, u32 frame)
+void fighter_pose_start(u32 side, u32 edx, u32 ebx, u32 ecx, u32 frame)
 {
+    PR_SEAM(0x39F40u, side, edx, ebx, ecx, frame);
     u32 ctx[6];
     fighter_ctx_swap(ctx, side);                /* 0x39F4A */
     u32 s = ctx[1];
@@ -1047,8 +1049,9 @@ static u32 ai_b(u32 side)
 
 /* 0x187FC. The two slots latched, then slot0+0x2C - slot1+0x2C. 0x46DD4 calls
  * it once for the sign and once for the value, so the port keeps both calls. */
-static s32 ai_distance(void)
+s32 ai_distance(void)
 {
+    PR_SEAM_RET0(0x187FCu);
     fighter_slot_latch(0u);                             /* 0x18800 */
     fighter_slot_latch(1u);                             /* 0x18808 */
     return (s32)DSD(0x001077DCu) - (s32)DSD(0x00107870u);   /* 0x1880D */
@@ -1545,6 +1548,7 @@ void fighter_command_block(void)
  * 0x38154 on the record's +0x51 and returns 0 (record §48-K). */
 int fighter_state_36638(u32 slot, u32 rec)
 {
+    PR_SEAM_RET(0x36638u, slot, rec);
     if (DSB(slot + 0x54u) == 3u) {                      /* 0x3663E */
         DSB(slot + 0x43u) &= 0xBFu;
         return 0;
@@ -2053,7 +2057,7 @@ void fighter_state_364fc(u32 slot, u32 rec, u32 side)
 
 static void fighter_state_35b7c(u32 slot, u32 rec);         /* 0x35B7C */
 void fighter_state_35d20(u32 slot, u32 rec);                /* 0x35D20 */
-static void fighter_1883c(u32 side, u32 a, u32 b);          /* 0x1883C */
+void fighter_1883c(u32 side, u32 a, u32 b);                  /* 0x1883C */
 static void fighter_36e78(u32 slot);                        /* 0x36E78 */
 static u32  hit_record_y(u32 side);                         /* 0x18788 */
 
@@ -2164,8 +2168,9 @@ static void fighter_state_35b7c(u32 slot, u32 rec)
 
 /* 0x1883C. Re-latch both slots, add (a, b) to this side's +0x2C/+0x30, then
  * re-derive the record's +0x18/+0x1C through 0x18714/0x18788. */
-static void fighter_1883c(u32 side, u32 a, u32 b)
+void fighter_1883c(u32 side, u32 a, u32 b)
 {
+    PR_SEAM(0x1883Cu, side, a, b);
     u32 slot = DS_001077B0 + side * 0x94u;
     fighter_slot_latch(0u);                             /* 0x18846 */
     fighter_slot_latch(1u);                             /* 0x18852 */
@@ -3964,6 +3969,7 @@ static s32 hit_vert_distance(void)
  * `table` is 0xA7B44[char(side)] and `idx` is 0xA7A70. §6.7. */
 int hit_geometry(u32 side, u32 table, u32 idx)
 {
+    PR_SEAM_RET(0x1DDF4u, side, table, idx);
     u32 ch = (u32)DSB(DS_001077B0 + side * 0x94u + 0x7Au);
     u32 thr1 = (u32)DSB(table + ch) << 6;
     u32 thr2 = (u32)DSB(idx + ch) << 6;
@@ -4045,6 +4051,7 @@ void hit_anchor_set(u32 side, u32 x, u32 y)
  * and returns slot+0x2C minus DS_00100AB0[side]. */
 u32 hit_record_x(u32 side)
 {
+    PR_SEAM_RET(0x18714u, side);
     u32 slot = DS_001077B0 + side * 0x94u;
     if ((DSB(slot + 0x42u) & 0x08u) != 0u)
         return DSD(DSD(slot) + 0x18u);                  /* 0x18738 */
@@ -4140,7 +4147,7 @@ static void hit_anim_start_c(u32 rec, u32 stream, u32 frame_bits)
 /* 0x18B04. The attacker/defender facing flag: when mode != 0x22 and
  * self+0x2C < other+0x2C set self_rec+0x29 bit 0x40 (else clear it), then
  * slot+0x2C = self+0x2C and rec+0x18 = 0x18714(side). */
-void hit_facing_flag(u32 side)
+static void hit_facing_flag_body(u32 side)
 {
     u32 ctx[6];
     if (DSW(DS_00104B00) == 0x22u) return;              /* 0x18B16 */
@@ -4153,6 +4160,14 @@ void hit_facing_flag(u32 side)
     DSD(ctx[4] + 0x18u) = hit_record_x(side);           /* 0x18AEC */
 }
 
+/* 0x18B04's entry as a call: the seam records it; 0x18AF8's second entry (0x18AFF's
+ * fall-through) runs the body without a call record. */
+void hit_facing_flag(u32 side)
+{
+    PR_SEAM(0x18B04u, side);
+    hit_facing_flag_body(side);
+}
+
 /* 0x1922C. The stance timer: when DS_00100B5A[side] > 0 and the record's +0x24
  * float is clear, seed it from the signed byte DS_00100B5C[side], zero +0x20,
  * and clamp a value outside [1.0, DS_0008058C] to 3.0. Then clear B5A (only on
@@ -4776,7 +4791,7 @@ void fighter_3e328(u32 slot, u32 rec, u32 side)
 #define FIGHT_9AFE4        0x0009AFE4u  /* 0x1484C: the 0x3C4CC hold (0x40200000) */
 #define FIGHT_FD108        0x000FD108u  /* 0x148E2: a dword per side, 0x2000/0x1000 */
 
-static int fighter_189fc(u32 side);                      /* 0x189FC */
+int fighter_189fc(u32 side);                            /* 0x189FC */
 
 /* 0x14590. The gate the 0x1490C family shares. EAX = side. With 0x33950's
  * ctx[3] (the other slot) in the knockback pose (+0x10 == 0x39CC8) or in
@@ -7551,6 +7566,7 @@ int fighter_3b038(u32 side)
  * +0x10 == 0x39CC8, +0x58 == 4). 0x3B714's early-out gate. */
 int fighter_39efc(u32 side)
 {
+    PR_SEAM_RET(0x39EFCu, side);
     u32 ctx[6];
     fighter_ctx_swap(ctx, side);                            /* 0x39F04 */
     u32 slot = ctx[3];
@@ -7828,8 +7844,9 @@ static int fighter_396ac(u32 side, u32 param_2)
  * not when the slot's +0x63 is 1, spawn the two dust actors (descriptor
  * 0xA1804/0xA17DC then 0xA17F0) at the 0xA17D4/0xA17D6-derived position, layers
  * 0xFE/0xFF. EAX is the slot; the raw's EDX (0x29A) is overwritten before use. */
-static void fighter_18b44(u32 slot)
+void fighter_18b44(u32 slot)
 {
+    PR_SEAM(0x18B44u, slot);
     u32 desc;
     if (DSB(slot + 0x63u) == 1u) return;                    /* 0x18B52 */
     if (DSB(DS_00100C1D) != 0u) return;                     /* 0x18B5B */
@@ -7852,8 +7869,9 @@ static void fighter_18b44(u32 slot)
 /* 0x189FC. The facing test on 0x33A10's context for EAX = side: with ctx[0]'s
  * actor bit 15 clear, 1 when ctx[3]'s x (+0x2C) is below ctx[2]'s, else 1
  * when it is above (signed); 0 otherwise. */
-static int fighter_189fc(u32 side)
+int fighter_189fc(u32 side)
 {
+    PR_SEAM_RET(0x189FCu, side);
     u32 ctx[6];
     fighter_ctx_swap(ctx, side);                            /* 0x18A04 0x33A10 */
     if (fighter_actor_bit15_clear(ctx[0]))                  /* 0x18A0C 0x1A570 */
@@ -7865,8 +7883,9 @@ static int fighter_189fc(u32 side)
 
 /* 0x18A4C. 1 when 0x189FC(ctx[1]) holds and ctx[1]'s actor bit 15 differs
  * from ctx[0]'s (0x33950 context for EAX = side), else 0. */
-static int fighter_18a4c(u32 side)
+int fighter_18a4c(u32 side)
 {
+    PR_SEAM_RET(0x18A4Cu, side);
     u32 ctx[6];
     fighter_ctx_same(ctx, side);                            /* 0x18A54 0x33950 */
     if (fighter_actor_bit15_clear(ctx[0])) {                /* 0x18A5C 0x1A570 */
@@ -7880,7 +7899,7 @@ static int fighter_18a4c(u32 side)
 }
 
 /* 0x18BD4. Fill the 16 check flags with 2 ("skip"). EAX = the flag bytes. */
-static void fighter_18bd4(u8 flags[16])
+void fighter_18bd4(u8 flags[16])
 {
     for (u32 i = 0; i < 16u; i++) flags[i] = 2u;            /* 0x18BD4..0x18C0F */
 }
@@ -8393,8 +8412,9 @@ void fighter_winner_body(u32 side)
 
 /* 0x3B8D8. 1 when the side's slot+0x2C moved by `delta` reaches the arena
  * wall: x >= DS_000BE018 or x <= -DS_000BE018. EAX = side, EDX = delta. */
-static int fighter_3b8d8(u32 side, s32 delta)
+int fighter_3b8d8(u32 side, s32 delta)
 {
+    PR_SEAM_RET(0x3B8D8u, side, delta);
     s32 x = (s32)DSD(DS_001077B0 + side * 0x94u + 0x2Cu) + delta;  /* 0x3B8E7/0x3B8F4 */
     s32 wall = (s32)DSD(DS_000BE018);                   /* 0x3B8EE */
     if (x >= wall) return 1;                            /* 0x3B8F6 */
@@ -8408,6 +8428,7 @@ static int fighter_3b8d8(u32 side, s32 delta)
  * 0x3C208's 0x3C2CC/0x3C2FC. */
 s32 fighter_3b90c(u32 side, s32 delta)
 {
+    PR_SEAM_RET(0x3B90Cu, side, delta);
     s32 x = (s32)(DSD(DS_001077B0 + side * 0x94u + 0x2Cu)
                   + (u32)delta);                        /* 0x3B91B/0x3B928 */
     s32 wall = (s32)DSD(DS_000BE018);                   /* 0x3B922 */
@@ -8423,7 +8444,7 @@ void fighter_18af8(void)
 {
     PR_SEAM0(0x18AF8u);
     hit_facing_flag(0u);                                /* 0x18AF8/0x18AFA */
-    hit_facing_flag(1u);                                /* 0x18AFF, fall-through */
+    hit_facing_flag_body(1u);                           /* 0x18AFF, fall-through into 0x18B04 */
 }
 
 /* 0x3C208. Put the other side `dist` (its magnitude) away from the side.
diff --git a/port/src/game/fighter.h b/port/src/game/fighter.h
index 4eab5f3..d13912c 100644
--- a/port/src/game/fighter.h
+++ b/port/src/game/fighter.h
@@ -16,6 +16,7 @@
 /* 0x33A10. Fill the six-dword side context: out[0]=1-side, out[1]=side,
  * out[2]=&slot[1-side], out[3]=&slot[side], out[4]=rec_other, out[5]=rec_self. */
 void fighter_ctx_swap(u32 out[6], u32 side);
+void fighter_pose_start(u32 side, u32 edx, u32 ebx, u32 ecx, u32 frame);
 
 /* 0x33950. The mirror of fighter_ctx_swap: out[0]=side, out[1]=1-side,
  * out[2]=&slot[side], out[3]=&slot[1-side], out[4]=rec_self, out[5]=rec_other. */
@@ -484,6 +485,7 @@ void fighter_3e328(u32 slot, u32 rec, u32 side);
  * check, 0 returns 1 when it holds, 1 when it fails; flag 0 is inverted and
  * rewritten to 4/3), EBX/ECX = two box tables (0 selects 0xA1818/0xA1822).
  * Returns 0 only when every check passes. */
+void fighter_18bd4(u8 flags[16]);                        /* 0x18BD4 */
 int fighter_18c14(u32 side, u8 flags[16], u32 box_a, u32 box_b);
 
 /* 0x19020. 0x1958C's per-slot hook call (0x195B6): with the side's slot +0x18
@@ -586,6 +588,12 @@ void fighter_3c208(u32 side, s32 dist);
 /* 0x3B90C. The side's slot+0x2C plus `delta`, clamped to +/-DS_000BE018 (the
  * arena wall). EAX = side, EDX = delta; 0x3C208's two calls only. */
 s32 fighter_3b90c(u32 side, s32 delta);
+s32 ai_distance(void);
+void fighter_18b44(u32 slot);
+int fighter_189fc(u32 side);
+int fighter_18a4c(u32 side);
+void fighter_1883c(u32 side, u32 a, u32 b);
+int fighter_3b8d8(u32 side, s32 delta);
 
 /* 0x14CC4. The +0x18 hook 0x14E44 stores: 1 while the record's +0x61 is
  * clear; else the 0x18C14 checks and the 0x187FC range 0x1900..0x3200 decide
diff --git a/port/src/mem.h b/port/src/mem.h
index 048bd3a..3d0730f 100644
--- a/port/src/mem.h
+++ b/port/src/mem.h
@@ -103,6 +103,14 @@ extern pr_seam_fn pr_seam;
             if (pr_seam((addr), 0u, (const u32 *)0, &seam_e_)) return;      \
         }                                                                   \
     } while (0)
+#define PR_SEAM_RET0(addr)                                                  \
+    do {                                                                    \
+        if (pr_seam) {                                                      \
+            u32 seam_e_;                                                    \
+            if (pr_seam((addr), 0u, (const u32 *)0, &seam_e_))              \
+                return seam_e_;                                             \
+        }                                                                   \
+    } while (0)
 #define PR_SEAM_RET(addr, ...)                                              \
     do {                                                                    \
         if (pr_seam) {                                                      \
diff --git a/port/tests/diff_runner.c b/port/tests/diff_runner.c
index ceb9df0..165750f 100644
--- a/port/tests/diff_runner.c
+++ b/port/tests/diff_runner.c
@@ -2139,6 +2139,884 @@ static void m_4844c_width(const u32 *r, u32 *eax)      /* the word +0x74 cleared
     *eax = 0u;
 }
 
+/* Track P batch C1 (record 2026-10-03-reverse-c1): the callee rows. Each binding adapts the
+ * original's registers to the port function the dependent rows stub; each mutant is a plausible
+ * porting bug of that function alone. */
+static void b_3c148(const u32 *r, u32 *eax)            { fighter_3c148(r[R_EAX]); *eax = 0u; }
+static void b_3c16c(const u32 *r, u32 *eax)            { fighter_3c16c(r[R_EAX]); *eax = 0u; }
+static void b_39a10(const u32 *r, u32 *eax)            { fighter_39a10(r[R_EAX], r[R_EDX]); *eax = 0u; }
+static void b_36d98(const u32 *r, u32 *eax)            { fighter_36d98(r[R_EAX]); *eax = 0u; }
+static void b_18bd4(const u32 *r, u32 *eax)            { fighter_18bd4(mem + r[R_EAX]); *eax = 0u; }
+static void b_34d8c(const u32 *r, u32 *eax)            { hit_flash_pair(r[R_EAX]); *eax = 0u; }
+static void b_3c358(const u32 *r, u32 *eax)            { fighter_3c358(r[R_EAX]); *eax = 0u; }
+static void b_3c190(const u32 *r, u32 *eax)            { fighter_3c190(r[R_EAX], r[R_EDX]); *eax = 0u; }
+static void b_3c480(const u32 *r, u32 *eax)            { hit_anim_start_a(r[R_EAX], r[R_EDX], r[R_S0]); *eax = 0u; }
+static void m_3c148(const u32 *r, u32 *eax)            /* the word cleared at +0x35 */
+{
+    u32 rec = DSD(DS_001077B0 + r[R_EAX] * 0x94u);
+    DSW(rec + 0x35u) = 0u;
+    DSB(rec + 0x43u) = 0u;
+    DSB(rec + 0x42u) = 0u;
+    *eax = 0u;
+}
+static void m_3c148_side(const u32 *r, u32 *eax)       /* always side 0 */
+{
+    (void)r;
+    u32 rec = DSD(DS_001077B0);
+    DSW(rec + 0x34u) = 0u;
+    DSB(rec + 0x43u) = 0u;
+    DSB(rec + 0x42u) = 0u;
+    *eax = 0u;
+}
+static void m_3c148_width(const u32 *r, u32 *eax)      /* the word cleared as a dword (+0x36/+0x37 too) */
+{
+    u32 rec = DSD(DS_001077B0 + r[R_EAX] * 0x94u);
+    DSD(rec + 0x34u) = 0u;
+    DSB(rec + 0x43u) = 0u;
+    DSB(rec + 0x42u) = 0u;
+    *eax = 0u;
+}
+static void m_3c16c(const u32 *r, u32 *eax)            /* the word cleared at +0x38 */
+{
+    u32 rec = DSD(DS_001077B0 + r[R_EAX] * 0x94u);
+    DSW(rec + 0x38u) = 0u;
+    DSW(rec + 0x44u) = 0u;
+    *eax = 0u;
+}
+static void m_3c16c_side(const u32 *r, u32 *eax)       /* always side 0 */
+{
+    (void)r;
+    u32 rec = DSD(DS_001077B0);
+    DSW(rec + 0x36u) = 0u;
+    DSW(rec + 0x44u) = 0u;
+    *eax = 0u;
+}
+static void m_3c16c_width(const u32 *r, u32 *eax)      /* the +0x36 word cleared as a dword (+0x38/+0x39) */
+{
+    u32 rec = DSD(DS_001077B0 + r[R_EAX] * 0x94u);
+    DSD(rec + 0x36u) = 0u;
+    DSW(rec + 0x44u) = 0u;
+    *eax = 0u;
+}
+static void m_39a10_at(const u32 *r, int mode)         /* 0: side 0; 1: rec+0x51 */
+{
+    u32 side = mode == 0 ? 0u : (u32)DSB(r[R_EAX] + 0x51u);
+    DSW(DS_001077B0 + side * 0x94u + 0x74u) = (u16)r[R_EDX];
+}
+static void m_39a10(const u32 *r, u32 *eax)            /* the timer at +0x76 */
+{
+    u32 side = (u32)DSB(r[R_EAX] + 0x51u);
+    DSW(DS_001077B0 + side * 0x94u + 0x76u) = (u16)r[R_EDX];
+    *eax = 0u;
+}
+static void m_39a10_side(const u32 *r, u32 *eax)       /* always side 0 */
+{
+    m_39a10_at(r, 0);
+    *eax = 0u;
+}
+static void m_36d98(const u32 *r, u32 *eax)            /* +0x52 = 8 */
+{
+    u32 slot = r[R_EAX], rec = DSD(slot), side = (u32)DSB(rec + 0x51u);
+    DSB(DS_001078F2 + side) = 1u;
+    DSB(slot + 0x5Du) = 0u;
+    DSB(slot + 0x52u) = 8u;
+    DSB(slot + 0x53u) = 4u;
+    DSB(slot + 0x43u) &= 0xFBu;
+    *eax = 0u;
+}
+static void m_36d98_side(const u32 *r, u32 *eax)       /* the flag byte always side 0's */
+{
+    u32 slot = r[R_EAX];
+    DSB(DS_001078F2) = 1u;
+    DSB(slot + 0x5Du) = 0u;
+    DSB(slot + 0x52u) = 9u;
+    DSB(slot + 0x53u) = 4u;
+    DSB(slot + 0x43u) &= 0xFBu;
+    *eax = 0u;
+}
+static void m_36d98_and(const u32 *r, u32 *eax)        /* `or 4` for the `and ~4` */
+{
+    u32 slot = r[R_EAX], rec = DSD(slot), side = (u32)DSB(rec + 0x51u);
+    DSB(DS_001078F2 + side) = 1u;
+    DSB(slot + 0x5Du) = 0u;
+    DSB(slot + 0x52u) = 9u;
+    DSB(slot + 0x53u) = 4u;
+    DSB(slot + 0x43u) |= 4u;
+    *eax = 0u;
+}
+static void m_18bd4(const u32 *r, u32 *eax)            /* 15 bytes, not 16 */
+{
+    u8 *f = mem + r[R_EAX];
+    for (u32 i = 0; i < 15u; i++) f[i] = 2u;
+    *eax = 0u;
+}
+static void m_18bd4_val(const u32 *r, u32 *eax)        /* fills 1 */
+{
+    u8 *f = mem + r[R_EAX];
+    for (u32 i = 0; i < 16u; i++) f[i] = 1u;
+    *eax = 0u;
+}
+static void m_18bd4_off(const u32 *r, u32 *eax)        /* starts one byte late */
+{
+    u8 *f = mem + r[R_EAX] + 1;
+    for (u32 i = 0; i < 16u; i++) f[i] = 2u;
+    *eax = 0u;
+}
+static void m_34d8c(const u32 *r, u32 *eax)            /* ignores 0x1078FA */
+{
+    DSB(DSD(DS_001077B0 + r[R_EAX] * 0x94u) + 0x59u) = 1u;
+    DSB(DSD(DS_001077B0 + (1u - r[R_EAX]) * 0x94u) + 0x59u) = 0xFFu;
+    *eax = 0u;
+}
+static void m_34d8c_side(const u32 *r, u32 *eax)       /* always side 0 */
+{
+    (void)r;
+    if (DSB(DS_001078FA) != 2u) { *eax = 0u; return; }
+    DSB(DSD(DS_001077B0) + 0x59u) = 1u;
+    DSB(DSD(DS_001077B0 + 0x94u) + 0x59u) = 0xFFu;
+    *eax = 0u;
+}
+static void m_3c358_at(const u32 *r, int mode)         /* 0: slots swapped; 1: side 0; 2: +0x42 = 4 (mov) */
+{
+    u32 side = mode == 1 ? 0u : r[R_EAX];
+    u32 self = DS_001077B0 + (mode == 0 ? 1u - side : side) * 0x94u;
+    u32 other = DS_001077B0 + (mode == 0 ? side : 1u - side) * 0x94u;
+    u32 ctx[6], rec;
+    fighter_ctx_same(ctx, side);
+    DSB(self + 0x52u) = 9u;
+    DSB(self + 0x53u) = 7u;
+    if (mode == 2) DSB(self + 0x42u) = 4u; else DSB(self + 0x42u) |= 4u;
+    DSB(other + 0x52u) = 0x10u;
+    DSB(other + 0x53u) = 0x0Au;
+    DSB(other + 0x54u) = 0u;
+    DSD(other + 0x0Cu) = 0u;
+    rec = DSD(DS_001077B0);
+    DSW(rec + 0x34u) = 0u;
+    DSB(rec + 0x43u) = 0u;
+    DSB(rec + 0x42u) = 0u;
+    rec = DSD(DS_00107844);
+    DSW(rec + 0x34u) = 0u;
+    DSB(rec + 0x43u) = 0u;
+    DSB(rec + 0x42u) = 0u;
+    DSD(ctx[4] + 0x1Cu) = 0u;
+    DSD(ctx[5] + 0x1Cu) = 0u;
+}
+static void m_3c358(const u32 *r, u32 *eax)            /* the self/other slots swapped */
+{
+    m_3c358_at(r, 0);
+    *eax = 0u;
+}
+static void m_3c358_side(const u32 *r, u32 *eax)       /* always side 0 */
+{
+    m_3c358_at(r, 1);
+    *eax = 0u;
+}
+static void m_3c358_42(const u32 *r, u32 *eax)         /* +0x42 = 4 instead of or 4 */
+{
+    m_3c358_at(r, 2);
+    *eax = 0u;
+}
+static void m_3c190_at(const u32 *r, int mode)         /* 0: always negate; 1: the other side's flip; 2: dword store */
+{
+    u32 side = r[R_EAX], v = r[R_EDX];
+    int unflipped = mode == 0 ? 1 : fighter_actor_bit15_clear(mode == 1 ? 1u - side : side);
+    u32 rec = DSD(DS_001077B0 + side * 0x94u);
+    if (unflipped != 0) v = 0u - v;
+    if (mode == 2) DSD(rec + 0x34u) = v; else DSW(rec + 0x34u) = (u16)v;
+}
+static void m_3c190(const u32 *r, u32 *eax)            /* always negates */
+{
+    m_3c190_at(r, 0);
+    *eax = 0u;
+}
+static void m_3c190_arg(const u32 *r, u32 *eax)        /* 0x1A570 on the other side */
+{
+    m_3c190_at(r, 1);
+    *eax = 0u;
+}
+static void m_3c190_width(const u32 *r, u32 *eax)      /* the word stored as a dword (+0x36/+0x37) */
+{
+    m_3c190_at(r, 2);
+    *eax = 0u;
+}
+static void m_3c480(const u32 *r, u32 *eax)            /* x from the record, not the slot */
+{
+    u32 ctx[6];
+    hit_anim_ctx(ctx, r[R_EAX]);
+    hit_anchor_set(ctx[0], DSD(ctx[4] + 0x18u), 0u);
+    actors_anim_begin(r[R_EAX], r[R_EDX], r[R_S0]);
+    hit_anchor_x(ctx[0], DSD(ctx[4] + 0x2Cu));
+    *eax = 0u;
+}
+static void m_3c480_order(const u32 *r, u32 *eax)      /* 0x2BC30 before the anchor set */
+{
+    u32 ctx[6];
+    hit_anim_ctx(ctx, r[R_EAX]);
+    actors_anim_begin(r[R_EAX], r[R_EDX], r[R_S0]);
+    hit_anchor_set(ctx[0], DSD(ctx[4] + 0x18u), 0u);
+    hit_anchor_x(ctx[0], DSD(ctx[2] + 0x2Cu));
+    *eax = 0u;
+}
+static void m_3c480_side(const u32 *r, u32 *eax)       /* the side forced to 0 */
+{
+    u32 ctx[6];
+    hit_anim_ctx(ctx, r[R_EAX]);
+    hit_anchor_set(0u, DSD(ctx[4] + 0x18u), 0u);
+    actors_anim_begin(r[R_EAX], r[R_EDX], r[R_S0]);
+    hit_anchor_x(0u, DSD(ctx[2] + 0x2Cu));
+    *eax = 0u;
+}
+
+/* §C1.3: the P1/P2/P3 callee rows with few second-level callees. */
+static void b_188ac(const u32 *r, u32 *eax)            { hit_anchor_set(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
+static void b_188dc(const u32 *r, u32 *eax)            { hit_anchor_x(r[R_EAX], r[R_EDX]); *eax = 0u; }
+static void b_18af8(const u32 *r, u32 *eax)            { (void)r; fighter_18af8(); *eax = 0u; }
+static void b_2a17c(const u32 *r, u32 *eax)            { actor_pset_palette(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
+static void b_2bc30(const u32 *r, u32 *eax)            { actors_anim_begin(r[R_EAX], r[R_EDX], r[R_S0]); *eax = 0u; }
+static void b_39fb0(const u32 *r, u32 *eax)            { fighter_39fb0(r[R_EAX]); *eax = 0u; }
+static void b_3a95c(const u32 *r, u32 *eax)            { fighter_3a95c(r[R_EAX], r[R_EDX]); *eax = 0u; }
+static void b_35838(const u32 *r, u32 *eax)            { fighter_state_35838(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
+static void b_468d8(const u32 *r, u32 *eax)            { *eax = (u32)ai_pred_468d8(r[R_EAX]); }
+static void m_188ac(const u32 *r, u32 *eax)            /* +0x18 and +0x1C swapped */
+{
+    u32 rec = DSD(DS_001077B0 + r[R_EAX] * 0x94u);
+    DSD(rec + 0x18u) = r[R_EBX];
+    DSD(rec + 0x1Cu) = r[R_EDX];
+    fighter_slot_latch(r[R_EAX]);
+    *eax = 0u;
+}
+static void m_188ac_side(const u32 *r, u32 *eax)       /* always side 0 */
+{
+    u32 rec = DSD(DS_001077B0);
+    DSD(rec + 0x18u) = r[R_EDX];
+    DSD(rec + 0x1Cu) = r[R_EBX];
+    fighter_slot_latch(0u);
+    *eax = 0u;
+}
+static void m_188ac_latch(const u32 *r, u32 *eax)      /* the latch on the other side */
+{
+    u32 rec = DSD(DS_001077B0 + r[R_EAX] * 0x94u);
+    DSD(rec + 0x18u) = r[R_EDX];
+    DSD(rec + 0x1Cu) = r[R_EBX];
+    fighter_slot_latch(1u - r[R_EAX]);
+    *eax = 0u;
+}
+static void m_188dc(const u32 *r, u32 *eax)            /* the result stored at +0x1C */
+{
+    DSD(DS_001077B0 + r[R_EAX] * 0x94u + 0x2Cu) = r[R_EDX];
+    DSD(DSD(DS_001077B0 + r[R_EAX] * 0x94u) + 0x1Cu) = hit_record_x(r[R_EAX]);
+    *eax = 0u;
+}
+static void m_188dc_side(const u32 *r, u32 *eax)       /* always side 0 */
+{
+    (void)r;
+    DSD(DS_001077B0 + 0x2Cu) = r[R_EDX];
+    DSD(DSD(DS_001077B0) + 0x18u) = hit_record_x(0u);
+    *eax = 0u;
+}
+static void m_188dc_arg(const u32 *r, u32 *eax)        /* 0x18714 on the other side */
+{
+    DSD(DS_001077B0 + r[R_EAX] * 0x94u + 0x2Cu) = r[R_EDX];
+    DSD(DSD(DS_001077B0 + r[R_EAX] * 0x94u) + 0x18u) = hit_record_x(1u - r[R_EAX]);
+    *eax = 0u;
+}
+static void m_188dc_eax(const u32 *r, u32 *eax)        /* a constant for 0x18714's result */
+{
+    DSD(DS_001077B0 + r[R_EAX] * 0x94u + 0x2Cu) = r[R_EDX];
+    DSD(DSD(DS_001077B0 + r[R_EAX] * 0x94u) + 0x18u) = 0x77777777u;
+    *eax = 0u;
+}
+static void m_18af8_body(u32 side, int le)             /* the 0x18B04 body, inline (it is static) */
+{
+    u32 ctx[6];
+    if (DSW(DS_00104B00) == 0x22u) return;
+    fighter_ctx_same(ctx, side);
+    if (le ? (s32)DSD(ctx[2] + 0x2Cu) <= (s32)DSD(ctx[3] + 0x2Cu)
+           : (s32)DSD(ctx[2] + 0x2Cu) < (s32)DSD(ctx[3] + 0x2Cu))
+        DSB(ctx[4] + 0x29u) |= 0x40u;
+    else
+        DSB(ctx[4] + 0x29u) &= 0xBFu;
+    DSD(ctx[2] + 0x2Cu) = DSD(ctx[2] + 0x2Cu);
+    DSD(ctx[4] + 0x18u) = hit_record_x(side);
+}
+static void m_18af8(const u32 *r, u32 *eax)            /* the fall-through body on side 0 */
+{
+    (void)r;
+    hit_facing_flag(0u);
+    m_18af8_body(0u, 0);
+    *eax = 0u;
+}
+static void m_18af8_once(const u32 *r, u32 *eax)       /* the fall-through body skipped */
+{
+    (void)r;
+    hit_facing_flag(0u);
+    *eax = 0u;
+}
+static void m_18af8_le(const u32 *r, u32 *eax)         /* `<=` for the `<` at 0x18B2E */
+{
+    (void)r;
+    hit_facing_flag(0u);
+    m_18af8_body(1u, 1);
+    *eax = 0u;
+}
+static void m_2a17c(const u32 *r, u32 *eax)            /* the old handle never released */
+{
+    u32 rec = r[R_EAX], pset = DSD(DS_001014EC) + (u32)DSW(rec + 0x56u) * 0x20u;
+    DSW(pset + 0x02u) = (u16)(r[R_EDX] | (DSB(rec + 0x5Fu) != 0 ? 0x800u : 0u));
+    if (r[R_EBX] == 0u) { *eax = 0u; return; }
+    DSD(pset + 0x18u) = palette_acquire(r[R_EBX]);
+    *eax = 0u;
+}
+static void m_2a17c_order(const u32 *r, u32 *eax)      /* the acquire before the release */
+{
+    u32 rec = r[R_EAX], pset = DSD(DS_001014EC) + (u32)DSW(rec + 0x56u) * 0x20u;
+    u32 old;
+    DSW(pset + 0x02u) = (u16)(r[R_EDX] | (DSB(rec + 0x5Fu) != 0 ? 0x800u : 0u));
+    if (r[R_EBX] == 0u) { *eax = 0u; return; }
+    old = DSD(pset + 0x18u);
+    DSD(pset + 0x18u) = palette_acquire(r[R_EBX]);
+    if (old != 0u) palette_release(old);
+    *eax = 0u;
+}
+static void m_2a17c_arg(const u32 *r, u32 *eax)        /* the release given the new handle */
+{
+    u32 rec = r[R_EAX], pset = DSD(DS_001014EC) + (u32)DSW(rec + 0x56u) * 0x20u;
+    DSW(pset + 0x02u) = (u16)(r[R_EDX] | (DSB(rec + 0x5Fu) != 0 ? 0x800u : 0u));
+    if (r[R_EBX] == 0u) { *eax = 0u; return; }
+    if (DSD(pset + 0x18u) != 0u) {
+        palette_release(r[R_EBX]);
+        DSD(pset + 0x18u) = 0u;
+    }
+    DSD(pset + 0x18u) = palette_acquire(r[R_EBX]);
+    *eax = 0u;
+}
+static void m_2a17c_early(const u32 *r, u32 *eax)      /* pset+2 stored after the calls */
+{
+    u32 rec = r[R_EAX], pset = DSD(DS_001014EC) + (u32)DSW(rec + 0x56u) * 0x20u;
+    if (r[R_EBX] != 0u) {
+        u32 old = DSD(pset + 0x18u);
+        if (old != 0u) {
+            palette_release(old);
+            DSD(pset + 0x18u) = 0u;
+        }
+        DSD(pset + 0x18u) = palette_acquire(r[R_EBX]);
+    }
+    DSW(pset + 0x02u) = (u16)(r[R_EDX] | (DSB(rec + 0x5Fu) != 0 ? 0x800u : 0u));
+    *eax = 0u;
+}
+static void m_2bc30(const u32 *r, u32 *eax)            /* the opcode flag 1 */
+{
+    u32 rec = r[R_EAX];
+    DSD(rec + 0x0Cu) = 0u;
+    DSD(rec + 0x10u) = 0u;
+    DSB(rec + 0x52u) = 0u;
+    DSB(rec + 0x50u) = 0u;
+    DSB(rec + 0x61u) = 0u;
+    DSD(rec + 8u) = r[R_EDX];
+    DSW(rec + 0x28u) &= 0xf7ebu;
+    DSB(rec + 0x2bu) &= (u8)~0x04u;
+    DSD(rec + 0x24u) = r[R_S0];
+    DSD(rec + 0x20u) = r[R_S0];
+    for (;;) {
+        if (((DSW(DSD(rec + 8u)) >> 8) & 0x80u) == 0) break;
+        u32 st = spawn_anim_opcode(rec, DSW(rec + 0x56u), 1u);
+        if (st != 0) {
+            if (st != 1) DSD(rec + 8u) += 2u;
+            break;
+        }
+        DSD(rec + 8u) += 2u;
+    }
+    u32 pset = DSD(DS_001014EC) + (u32)DSW(rec + 0x56u) * PSET_SIZE;
+    DSW(pset) = (u16)anim_next_sprite_id(rec, pset);
+    *eax = 0u;
+}
+static void m_2bc30_order(const u32 *r, u32 *eax)      /* the frame stores after the walk */
+{
+    u32 rec = r[R_EAX];
+    DSD(rec + 0x0Cu) = 0u;
+    DSD(rec + 0x10u) = 0u;
+    DSB(rec + 0x52u) = 0u;
+    DSB(rec + 0x50u) = 0u;
+    DSB(rec + 0x61u) = 0u;
+    DSD(rec + 8u) = r[R_EDX];
+    DSW(rec + 0x28u) &= 0xf7ebu;
+    DSB(rec + 0x2bu) &= (u8)~0x04u;
+    for (;;) {
+        if (((DSW(DSD(rec + 8u)) >> 8) & 0x80u) == 0) break;
+        u32 st = spawn_anim_opcode(rec, DSW(rec + 0x56u), 0u);
+        if (st != 0) {
+            if (st != 1) DSD(rec + 8u) += 2u;
+            break;
+        }
+        DSD(rec + 8u) += 2u;
+    }
+    DSD(rec + 0x24u) = r[R_S0];
+    DSD(rec + 0x20u) = r[R_S0];
+    u32 pset = DSD(DS_001014EC) + (u32)DSW(rec + 0x56u) * PSET_SIZE;
+    DSW(pset) = (u16)anim_next_sprite_id(rec, pset);
+    *eax = 0u;
+}
+static void m_2bc30_frame(const u32 *r, u32 *eax)      /* only +0x24 takes the frame */
+{
+    u32 rec = r[R_EAX];
+    DSD(rec + 0x0Cu) = 0u;
+    DSD(rec + 0x10u) = 0u;
+    DSB(rec + 0x52u) = 0u;
+    DSB(rec + 0x50u) = 0u;
+    DSB(rec + 0x61u) = 0u;
+    DSD(rec + 8u) = r[R_EDX];
+    DSW(rec + 0x28u) &= 0xf7ebu;
+    DSB(rec + 0x2bu) &= (u8)~0x04u;
+    DSD(rec + 0x24u) = r[R_S0];
+    for (;;) {
+        if (((DSW(DSD(rec + 8u)) >> 8) & 0x80u) == 0) break;
+        u32 st = spawn_anim_opcode(rec, DSW(rec + 0x56u), 0u);
+        if (st != 0) {
+            if (st != 1) DSD(rec + 8u) += 2u;
+            break;
+        }
+        DSD(rec + 8u) += 2u;
+    }
+    u32 pset = DSD(DS_001014EC) + (u32)DSW(rec + 0x56u) * PSET_SIZE;
+    DSW(pset) = (u16)anim_next_sprite_id(rec, pset);
+    *eax = 0u;
+}
+static void m_39fb0(const u32 *r, u32 *eax)            /* the pose's edx argument 0x63 */
+{
+    u32 ctx[6];
+    fighter_ctx_swap(ctx, (u32)DSB(DSD(r[R_EAX]) + 0x51u));
+    hit_facing_flag(ctx[1]);
+    fighter_pose_start(ctx[1], 0xFFFFFFB0u, 0x63u, 0x0Fu, 0x14u);
+    *eax = 0u;
+}
+static void m_39fb0_side(const u32 *r, u32 *eax)       /* the facing flag always side 0 */
+{
+    u32 ctx[6];
+    fighter_ctx_swap(ctx, (u32)DSB(DSD(r[R_EAX]) + 0x51u));
+    hit_facing_flag(0u);
+    fighter_pose_start(ctx[1], 0xFFFFFFB0u, 0x64u, 0x0Fu, 0x14u);
+    *eax = 0u;
+}
+static void m_39fb0_order(const u32 *r, u32 *eax)      /* the pose before the facing flag */
+{
+    u32 ctx[6];
+    fighter_ctx_swap(ctx, (u32)DSB(DSD(r[R_EAX]) + 0x51u));
+    fighter_pose_start(ctx[1], 0xFFFFFFB0u, 0x64u, 0x0Fu, 0x14u);
+    hit_facing_flag(ctx[1]);
+    *eax = 0u;
+}
+static void m_3a95c(const u32 *r, u32 *eax)            /* the anchor x from the other record */
+{
+    u32 side = r[R_EAX], ctx[6];
+    fighter_ctx_swap(ctx, side);
+    hit_anchor_set(ctx[1], DSD(ctx[4] + 0x18u), 0u);
+    DSB(ctx[3] + 0x52u) = 0x10u;
+    DSB(ctx[3] + 0x53u) = 0x0Au;
+    DSB(ctx[3] + 0x54u) = 0u;
+    DSD(ctx[3] + 0x10u) = 0u;
+    actors_anim_begin(ctx[5], DSD(0x000C8FE0u + (u32)DSB(ctx[3] + 0x7Au) * 4u), 0x40400000u);
+    DSB(ctx[3] + 0x7Eu) = (u8)(DSB(0x000BECF8u) + (u8)r[R_EDX]);
+    *eax = 0u;
+}
+static void m_3a95c_side(const u32 *r, u32 *eax)       /* the anchor always on side 1 */
+{
+    u32 side = r[R_EAX], ctx[6];
+    fighter_ctx_swap(ctx, side);
+    hit_anchor_set(1u, DSD(ctx[5] + 0x18u), 0u);
+    DSB(ctx[3] + 0x52u) = 0x10u;
+    DSB(ctx[3] + 0x53u) = 0x0Au;
+    DSB(ctx[3] + 0x54u) = 0u;
+    DSD(ctx[3] + 0x10u) = 0u;
+    actors_anim_begin(ctx[5], DSD(0x000C8FE0u + (u32)DSB(ctx[3] + 0x7Au) * 4u), 0x40400000u);
+    DSB(ctx[3] + 0x7Eu) = (u8)(DSB(0x000BECF8u) + (u8)r[R_EDX]);
+    *eax = 0u;
+}
+static void m_3a95c_arg(const u32 *r, u32 *eax)        /* the animation from the wrong slot */
+{
+    u32 side = r[R_EAX], ctx[6];
+    fighter_ctx_swap(ctx, side);
+    hit_anchor_set(ctx[1], DSD(ctx[5] + 0x18u), 0u);
+    DSB(ctx[3] + 0x52u) = 0x10u;
+    DSB(ctx[3] + 0x53u) = 0x0Au;
+    DSB(ctx[3] + 0x54u) = 0u;
+    DSD(ctx[3] + 0x10u) = 0u;
+    actors_anim_begin(ctx[4], DSD(0x000C8FE0u + (u32)DSB(ctx[3] + 0x7Au) * 4u), 0x40400000u);
+    DSB(ctx[3] + 0x7Eu) = (u8)(DSB(0x000BECF8u) + (u8)r[R_EDX]);
+    *eax = 0u;
+}
+static void m_35838_at(const u32 *r, int mode)         /* 0: streams swapped; 1: char 0; 2: stores before the call */
+{
+    u32 slot = r[R_EAX], rec = r[R_EDX], dirbits = r[R_EBX];
+    u32 charb = mode == 1 ? 0u : (u32)DSB(slot + 0x7Au);
+    DSD(slot + 0x40u) &= 0xFCFF7FFFu;
+    DSB(slot + 0x41u) |= 0x80u;
+    if (mode == 2) {
+        DSW(slot + 0x4Cu) = 0u;
+        DSB(slot + 0x52u) = 0x0Eu;
+        DSB(slot + 0x53u) = 0u;
+    }
+    if ((DSW(rec + 0x28u) >> 8 & 0x40u) == 0u) {
+        if ((dirbits & 0x2000u) != 0u) {
+            actors_anim_begin(rec, DSD((mode == 0 ? 0x000C8AB8u : 0x000C8A40u) + charb * 4u), 0x3F800000u);
+            DSB(slot + 0x43u) |= 2u;
+            goto tail;
+        }
+        if (DSW(DS_00104B00) == 0x22u) {
+            DSB(slot + 0x43u) |= 0x40u;
+            (void)fighter_state_36638(slot, rec);
+            goto tail;
+        }
+    } else {
+        if ((dirbits & 0x1000u) != 0u) {
+            actors_anim_begin(rec, DSD((mode == 0 ? 0x000C8AB8u : 0x000C8A40u) + charb * 4u), 0x3F800000u);
+            DSB(slot + 0x43u) |= 2u;
+            DSW(slot + 0x4Cu) = 0;
+            DSB(slot + 0x52u) = 0x0Eu;
+            DSB(slot + 0x53u) = 0u;
+            return;
+        }
+        if (DSW(DS_00104B00) == 0x22u) {
+            DSB(slot + 0x43u) |= 0x40u;
+            (void)fighter_state_36638(slot, rec);
+            DSW(slot + 0x4Cu) = 0;
+            DSB(slot + 0x52u) = 0x0Eu;
+            DSB(slot + 0x53u) = 0u;
+            return;
+        }
+    }
+    actors_anim_begin(rec, DSD((mode == 0 ? 0x000C8A40u : 0x000C8AB8u) + charb * 4u), 0x3F800000u);
+    DSB(slot + 0x43u) |= 1u;
+tail:
+    DSW(slot + 0x4Cu) = 0;
+    DSB(slot + 0x52u) = 0x0Eu;
+    DSB(slot + 0x53u) = 0u;
+}
+static void m_35838(const u32 *r, u32 *eax)            /* the two animation streams swapped */
+{
+    m_35838_at(r, 0);
+    *eax = 0u;
+}
+static void m_35838_side(const u32 *r, u32 *eax)       /* the character index forced to 0 */
+{
+    m_35838_at(r, 1);
+    *eax = 0u;
+}
+static void m_35838_order(const u32 *r, u32 *eax)      /* the tail stores before the call */
+{
+    m_35838_at(r, 2);
+    *eax = 0u;
+}
+static void m_468d8(const u32 *r, u32 *eax)            /* the +0x24 low-31 mask dropped */
+{
+    u32 ctx[6];
+    fighter_ctx_swap(ctx, r[R_EAX]);
+    if (DSD(ctx[3] + 0x10u) == 0x22BECu && DSD(ctx[5] + 0x24u) == 0u
+            && DSB(ctx[3] + 0x54u) != 2u) {
+        *eax = 1u;
+        return;
+    }
+    *eax = DSB(ctx[3] + 0x52u) == 7u ? 1u : 0u;
+}
+static void m_468d8_eq(const u32 *r, u32 *eax)         /* +0x54 == 2 instead of != 2 */
+{
+    u32 ctx[6];
+    fighter_ctx_swap(ctx, r[R_EAX]);
+    if (DSD(ctx[3] + 0x10u) == 0x22BECu && (DSD(ctx[5] + 0x24u) & 0x7FFFFFFFu) == 0u
+            && DSB(ctx[3] + 0x54u) == 2u) {
+        *eax = 1u;
+        return;
+    }
+    *eax = DSB(ctx[3] + 0x52u) == 7u ? 1u : 0u;
+}
+static void m_468d8_side(const u32 *r, u32 *eax)       /* the context by 1-side */
+{
+    u32 ctx[6];
+    fighter_ctx_swap(ctx, 1u - r[R_EAX]);
+    if (DSD(ctx[3] + 0x10u) == 0x22BECu && (DSD(ctx[5] + 0x24u) & 0x7FFFFFFFu) == 0u
+            && DSB(ctx[3] + 0x54u) != 2u) {
+        *eax = 1u;
+        return;
+    }
+    *eax = DSB(ctx[3] + 0x52u) == 7u ? 1u : 0u;
+}
+
+/* §C1.4: 0x3C208 and its four new callees. */
+static void b_3c208(const u32 *r, u32 *eax)            { fighter_3c208(r[R_EAX], (s32)r[R_EDX]); *eax = 0u; }
+static void m_3c208_at(const u32 *r, int mode)         /* 0: the d<want arms swapped; 1: no |d|; 2: 1883C on the side */
+{
+    u32 side = r[R_EAX], other = 1u - side;
+    u32 want = r[R_EDX], d, gap;
+    u32 rec;
+    fighter_slot_latch(0u);
+    fighter_slot_latch(1u);
+    fighter_18af8();
+    rec = DSD(DS_001077B0);
+    DSW(rec + 0x34u) = 0u;
+    DSB(rec + 0x43u) = 0u;
+    DSB(rec + 0x42u) = 0u;
+    rec = DSD(DS_00107844);
+    DSW(rec + 0x34u) = 0u;
+    DSB(rec + 0x43u) = 0u;
+    DSB(rec + 0x42u) = 0u;
+    if ((s32)r[R_EDX] < 0) want = 0u - want;
+    if (mode == 1) {
+        d = (u32)ai_distance();
+    } else if (ai_distance() < 0) {
+        d = 0u - (u32)ai_distance();
+    } else {
+        d = (u32)ai_distance();
+    }
+    gap = d - want;
+    if ((s32)gap < 0) gap = 0u - gap;
+    if ((s32)d >= (s32)want) {
+        if (fighter_actor_bit15_clear(side))
+            fighter_1883c(mode == 2 ? side : other, gap, 0u);
+        else
+            fighter_1883c(mode == 2 ? side : other, 0u - gap, 0u);
+        return;
+    }
+    if (fighter_actor_bit15_clear(side) != (mode == 0)) {
+        gap = 0u - gap;
+        if (fighter_3b8d8(other, (s32)gap) == 0) {
+            fighter_1883c(other, gap, 0u);
+            return;
+        }
+        hit_anchor_x(other, (u32)fighter_3b90c(other, (s32)gap));
+        hit_anchor_x(side, DSD(DS_001077B0 + other * 0x94u + 0x2Cu) + want);
+        return;
+    }
+    if (fighter_3b8d8(other, (s32)gap) == 0) {
+        fighter_1883c(other, gap, 0u);
+        return;
+    }
+    hit_anchor_x(other, (u32)fighter_3b90c(other, (s32)gap));
+    hit_anchor_x(side, DSD(DS_001077B0 + other * 0x94u + 0x2Cu) - want);
+}
+static void m_3c208(const u32 *r, u32 *eax)            /* the d<want arms swapped */
+{
+    m_3c208_at(r, 0);
+    *eax = 0u;
+}
+static void m_3c208_abs(const u32 *r, u32 *eax)        /* no |d| */
+{
+    m_3c208_at(r, 1);
+    *eax = 0u;
+}
+static void m_3c208_arg(const u32 *r, u32 *eax)        /* 0x1883C on the side */
+{
+    m_3c208_at(r, 2);
+    *eax = 0u;
+}
+static void m_3c208_early(const u32 *r, u32 *eax)      /* the record clears after the calls */
+{
+    u32 side = r[R_EAX], other = 1u - side, want = r[R_EDX], d, gap, rec;
+    fighter_slot_latch(0u);
+    fighter_slot_latch(1u);
+    fighter_18af8();
+    if ((s32)r[R_EDX] < 0) want = 0u - want;
+    if (ai_distance() < 0) d = 0u - (u32)ai_distance(); else d = (u32)ai_distance();
+    gap = d - want;
+    if ((s32)gap < 0) gap = 0u - gap;
+    if ((s32)d >= (s32)want) {
+        if (fighter_actor_bit15_clear(side)) fighter_1883c(other, gap, 0u);
+        else fighter_1883c(other, 0u - gap, 0u);
+    } else if (fighter_actor_bit15_clear(side)) {
+        gap = 0u - gap;
+        if (fighter_3b8d8(other, (s32)gap) == 0) fighter_1883c(other, gap, 0u);
+        else {
+            hit_anchor_x(other, (u32)fighter_3b90c(other, (s32)gap));
+            hit_anchor_x(side, DSD(DS_001077B0 + other * 0x94u + 0x2Cu) + want);
+        }
+    } else if (fighter_3b8d8(other, (s32)gap) == 0) {
+        fighter_1883c(other, gap, 0u);
+    } else {
+        hit_anchor_x(other, (u32)fighter_3b90c(other, (s32)gap));
+        hit_anchor_x(side, DSD(DS_001077B0 + other * 0x94u + 0x2Cu) - want);
+    }
+    rec = DSD(DS_001077B0);
+    DSW(rec + 0x34u) = 0u;
+    DSB(rec + 0x43u) = 0u;
+    DSB(rec + 0x42u) = 0u;
+    rec = DSD(DS_00107844);
+    DSW(rec + 0x34u) = 0u;
+    DSB(rec + 0x43u) = 0u;
+    DSB(rec + 0x42u) = 0u;
+    *eax = 0u;
+}
+
+/* §C1.5: 0x18C14 and its five new callees. */
+static void b_18c14(const u32 *r, u32 *eax)
+{
+    u8 flags[16];
+    memcpy(flags, mem + r[R_EDX], sizeof flags);
+    *eax = (u32)fighter_18c14(r[R_EAX], flags, r[R_EBX], r[R_ECX]);
+    memcpy(mem + r[R_EDX], flags, sizeof flags);
+}
+static int m_18c14_impl(u32 side, u8 flags[16], u32 box_a, u32 box_b)
+{
+    u32 ctx[6];
+    u8 f;
+    int le;
+    /* PORT: 0x18C26 sets the local [esp+0x18] to 1 only for side != 0x29A, and
+     * 0x19005 returns 0 only when it is non-zero. For side 0x29A the raw
+     * reads an uninitialised stack byte; the port treats it as 0 (return 1).
+     * Every caller the port reaches passes side 0 or 1. */
+    int live = (side != 0x29Au);                            /* 0x18C1F/0x18C26 */
+    fighter_ctx_same(ctx, side);                            /* 0x18C2F 0x33950 */
+    if (box_a == 0) box_a = 0x000A1818u;                  /* 0x18C34/0x18C38 */
+    if (box_b == 0) box_b = 0x000A1822u;                  /* 0x18C3D/0x18C41 */
+
+    le = (s32)DSD(DS_00100AF8 + ctx[0] * 4u) <= 0;          /* 0x18C49 setle */
+    f = flags[0];                                           /* 0x18C58 */
+    if (f == 0) {
+        if (!le) { flags[0] = 4u; return 1; }               /* 0x18C73..0x18C7C */
+    } else if (f == 1) {
+        if (le) { flags[0] = 3u; return 1; }                /* 0x18C62..0x18C6B */
+    }
+
+    f = flags[1];                                           /* 0x18C84 */
+    if (f == 0) {
+        if (DSW(ctx[3] + 0x74u) != 0) return 1;             /* 0x18CC0 ja */
+        if (DSW(ctx[3] + 0x76u) >= 1u) return 1;             /* 0x18CD4 jg */
+    } else if (f == 1) {
+        if (DSW(ctx[3] + 0x74u) < 1u) return 1;             /* 0x18C9C jl */
+        if (DSW(ctx[3] + 0x76u) < 2u) return 1;             /* 0x18CB2 jge */
+    }
+
+    f = flags[0xF];                                         /* 0x18CDD */
+    if (f == 0) {
+        if (DSB(ctx[2] + 0x43u) & 4u) return 1;             /* 0x18D01 */
+    } else if (f == 1) {
+        if ((DSB(ctx[2] + 0x43u) & 4u) == 0) return 1;      /* 0x18CEC */
+    }
+
+    f = flags[2];                                           /* 0x18D0B */
+    if (f == 0) {
+        if (DSB(ctx[3] + 0x54u) == 0) return 1;             /* 0x18D2F */
+    } else if (f == 1) {
+        if (DSB(ctx[3] + 0x54u) != 0) return 1;             /* 0x18D1A */
+    }
+
+    f = flags[3];                                           /* 0x18D39 */
+    if (f == 0) {
+        if (DSB(ctx[3] + 0x54u) == 1u) return 1;            /* 0x18D5D */
+    } else if (f == 1) {
+        if (DSB(ctx[3] + 0x54u) != 1u) return 1;            /* 0x18D48 */
+    }
+
+    f = flags[5];                                           /* 0x18D67 */
+    if (f == 0) {
+        if (hit_geometry(ctx[1], box_a, box_b)) return 1;   /* 0x18D92 0x1DDF4 */
+    } else if (f == 1) {
+        if (!hit_geometry(ctx[1], box_a, box_b)) return 1;  /* 0x18D78 0x1DDF4 */
+    }
+
+    f = flags[6];                                           /* 0x18D9F */
+    if (f == 0) {
+        if (DSB(ctx[3] + 0x54u) == 7u) return 1;            /* 0x18DC3 */
+    } else if (f == 1) {
+        if (DSB(ctx[3] + 0x54u) != 7u) return 1;            /* 0x18DAE */
+    }
+
+    f = flags[7];                                           /* 0x18DCD */
+    if ((f == 0 && DSB(ctx[3] + 0x62u) != 0)                /* 0x18E05 */
+            || (f == 1 && DSB(ctx[3] + 0x62u) == 0)) {      /* 0x18DDC */
+        DSB(ctx[2] + 0x8Au) = 0;                            /* 0x18DE7/0x18E0F */
+        fighter_18b44(ctx[2]);                              /* 0x18DF1/0x18E1A */
+        return 1;
+    }
+
+    f = flags[8];                                           /* 0x18E2A */
+    if (f == 0) {
+        if (DSB(ctx[3] + 0x42u) & 8u) return 1;             /* 0x18E4E */
+    } else if (f == 1) {
+        if ((DSB(ctx[3] + 0x42u) & 8u) == 0) return 1;      /* 0x18E39 */
+    }
+
+    f = flags[9];                                           /* 0x18E58 */
+    if (f == 0) {
+        if (fighter_189fc(ctx[1])) return 1;                /* 0x18E7F */
+    } else if (f == 1) {
+        if (!fighter_189fc(ctx[1])) return 1;               /* 0x18E67 */
+    }
+
+    f = flags[0xA];                                         /* 0x18E8C */
+    if (f == 0) {
+        if (fighter_18a4c(ctx[0])) return 1;                /* 0x18EB1 */
+    } else if (f == 1) {
+        if (!fighter_18a4c(ctx[0])) return 1;               /* 0x18E9A */
+    }
+
+    f = flags[0xB];                                         /* 0x18EBE */
+    if (f == 0) {
+        if (DSB(ctx[4] + 0x61u) != 0) return 1;             /* 0x18EE2 */
+    } else if (f == 1) {
+        if (DSB(ctx[4] + 0x61u) == 0) return 1;             /* 0x18ECD */
+    }
+
+    f = flags[0xD];                                         /* 0x18EEC */
+    if ((f == 0 && fighter_39efc(ctx[1]))                   /* 0x18F27 */
+            || (f == 1 && !fighter_39efc(ctx[1]))) {        /* 0x18EFB */
+        DSB(ctx[2] + 0x8Au) = 0;                            /* 0x18F08/0x18F34 */
+        fighter_18b44(ctx[2]);                              /* 0x18F13/0x18F3F */
+        return 1;
+    }
+
+    f = flags[0xC];                                         /* 0x18F4F */
+    if (f == 0) {
+        if (DSB(ctx[3] + 0x53u) == 0x0Au) return 1;         /* 0x18F73 */
+    } else if (f == 1) {
+        if (DSB(ctx[3] + 0x53u) != 0x0Au) return 1;         /* 0x18F5E */
+    }
+
+    f = flags[0xE];                                         /* 0x18F7D */
+    if (f != 2u) {
+        u8 r = (u8)fighter_command_dispatch(ctx[1],
+                                            DSB(ctx[2] + 0x5Fu));   /* 0x18F94 0x3B298 */
+        if (f == 1u && r == 0) {
+            DSB(ctx[2] + 0x8Au) = r;                        /* 0x18FB0 */
+            return 1;
+        }
+        if ((f == 0 || f == 1u) && r != 0) {
+            DSB(ctx[2] + 0x8Au) = 0;                        /* 0x18FC9 */
+            return 1;
+        }
+    }
+
+    f = flags[4];                                           /* 0x18FDB */
+    if (f == 0) {
+        if (DSB(ctx[3] + 0x54u) == 2u) return 1;            /* 0x18FFF */
+    } else if (f == 1) {
+        if (DSB(ctx[3] + 0x54u) != 2u) return 1;            /* 0x18FEA */
+    }
+
+    return live ? 0 : 1;                                    /* 0x19005..0x19014 */
+}
+static void m_18c14(const u32 *r, u32 *eax)            /* flag 1's second test `>=` for `>` */
+{
+    u8 flags[16];
+    memcpy(flags, mem + r[R_EDX], sizeof flags);
+    *eax = (u32)m_18c14_impl(r[R_EAX], flags, r[R_EBX], r[R_ECX]);
+    memcpy(mem + r[R_EDX], flags, sizeof flags);
+}
+static void m_18c14_store(const u32 *r, u32 *eax)      /* the 0x8A store writes 1 */
+{
+    u8 flags[16];
+    u32 ctx[6];
+    memcpy(flags, mem + r[R_EDX], sizeof flags);
+    *eax = (u32)fighter_18c14(r[R_EAX], flags, r[R_EBX], r[R_ECX]);
+    memcpy(mem + r[R_EDX], flags, sizeof flags);
+    if (*eax == 1u) {
+        fighter_ctx_same(ctx, r[R_EAX]);
+        DSB(ctx[2] + 0x8Au) = 1u;
+    }
+}
+static void m_18c14_live(const u32 *r, u32 *eax)       /* always 0 */
+{
+    (void)r;
+    *eax = 0u;
+}
+
 static const binding_t k_bindings[] = {
     { "rng_next",                 b_rng_next,     0xFFFFFFFFu },
     { "fighter_slot_flag",        b_slot_flag,    0x000000FFu },
@@ -2377,6 +3255,87 @@ static const binding_t k_bindings[] = {
     { "fighter_4844c@bound",      m_4844c_bound,  0x00000000u },
     { "fighter_4844c@zext",       m_4844c_zext,   0x00000000u },
     { "fighter_4844c@width",      m_4844c_width,  0x00000000u },
+    { "fighter_3c148",            b_3c148,        0x00000000u },
+    { "fighter_3c16c",            b_3c16c,        0x00000000u },
+    { "fighter_39a10",            b_39a10,        0x00000000u },
+    { "fighter_36d98",            b_36d98,        0x00000000u },
+    { "fighter_18bd4",            b_18bd4,        0x00000000u },
+    { "fighter_34d8c",            b_34d8c,        0x00000000u },
+    { "fighter_3c358",            b_3c358,        0x00000000u },
+    { "fighter_3c190",            b_3c190,        0x00000000u },
+    { "fighter_3c480",            b_3c480,        0x00000000u },
+    { "fighter_3c148@mutant",     m_3c148,        0x00000000u },
+    { "fighter_3c148@side",       m_3c148_side,   0x00000000u },
+    { "fighter_3c148@width",      m_3c148_width,  0x00000000u },
+    { "fighter_3c16c@mutant",     m_3c16c,        0x00000000u },
+    { "fighter_3c16c@side",       m_3c16c_side,   0x00000000u },
+    { "fighter_3c16c@width",      m_3c16c_width,  0x00000000u },
+    { "fighter_39a10@mutant",     m_39a10,        0x00000000u },
+    { "fighter_39a10@side",       m_39a10_side,   0x00000000u },
+    { "fighter_36d98@mutant",     m_36d98,        0x00000000u },
+    { "fighter_36d98@side",       m_36d98_side,   0x00000000u },
+    { "fighter_36d98@and",        m_36d98_and,    0x00000000u },
+    { "fighter_18bd4@mutant",     m_18bd4,        0x00000000u },
+    { "fighter_18bd4@val",        m_18bd4_val,    0x00000000u },
+    { "fighter_18bd4@off",        m_18bd4_off,    0x00000000u },
+    { "fighter_34d8c@mutant",     m_34d8c,        0x00000000u },
+    { "fighter_34d8c@side",       m_34d8c_side,   0x00000000u },
+    { "fighter_3c358@mutant",     m_3c358,        0x00000000u },
+    { "fighter_3c358@side",       m_3c358_side,   0x00000000u },
+    { "fighter_3c358@42",         m_3c358_42,     0x00000000u },
+    { "fighter_3c190@mutant",     m_3c190,        0x00000000u },
+    { "fighter_3c190@arg",        m_3c190_arg,    0x00000000u },
+    { "fighter_3c190@width",      m_3c190_width,  0x00000000u },
+    { "fighter_3c480@mutant",     m_3c480,        0x00000000u },
+    { "fighter_3c480@order",      m_3c480_order,  0x00000000u },
+    { "fighter_3c480@side",       m_3c480_side,   0x00000000u },
+    { "fighter_188ac",            b_188ac,        0x00000000u },
+    { "fighter_188dc",            b_188dc,        0x00000000u },
+    { "fighter_18af8",            b_18af8,        0x00000000u },
+    { "fighter_2a17c",            b_2a17c,        0x00000000u },
+    { "fighter_2bc30",            b_2bc30,        0x00000000u },
+    { "fighter_39fb0",            b_39fb0,        0x00000000u },
+    { "fighter_3a95c",            b_3a95c,        0x00000000u },
+    { "fighter_35838",            b_35838,        0x00000000u },
+    { "fighter_468d8",            b_468d8,        0x000000FFu },
+    { "fighter_188ac@mutant",     m_188ac,        0x00000000u },
+    { "fighter_188ac@side",       m_188ac_side,   0x00000000u },
+    { "fighter_188ac@latch",      m_188ac_latch,  0x00000000u },
+    { "fighter_188dc@mutant",     m_188dc,        0x00000000u },
+    { "fighter_188dc@side",       m_188dc_side,   0x00000000u },
+    { "fighter_188dc@arg",        m_188dc_arg,    0x00000000u },
+    { "fighter_188dc@eax",        m_188dc_eax,    0x00000000u },
+    { "fighter_18af8@mutant",     m_18af8,        0x00000000u },
+    { "fighter_18af8@once",       m_18af8_once,   0x00000000u },
+    { "fighter_18af8@le",         m_18af8_le,     0x00000000u },
+    { "fighter_2a17c@mutant",     m_2a17c,        0x00000000u },
+    { "fighter_2a17c@order",      m_2a17c_order,  0x00000000u },
+    { "fighter_2a17c@arg",        m_2a17c_arg,    0x00000000u },
+    { "fighter_2a17c@early",      m_2a17c_early,  0x00000000u },
+    { "fighter_2bc30@mutant",     m_2bc30,        0x00000000u },
+    { "fighter_2bc30@order",      m_2bc30_order,  0x00000000u },
+    { "fighter_2bc30@frame",      m_2bc30_frame,  0x00000000u },
+    { "fighter_39fb0@mutant",     m_39fb0,        0x00000000u },
+    { "fighter_39fb0@side",       m_39fb0_side,   0x00000000u },
+    { "fighter_39fb0@order",      m_39fb0_order,  0x00000000u },
+    { "fighter_3a95c@mutant",     m_3a95c,        0x00000000u },
+    { "fighter_3a95c@side",       m_3a95c_side,   0x00000000u },
+    { "fighter_3a95c@arg",        m_3a95c_arg,    0x00000000u },
+    { "fighter_35838@mutant",     m_35838,        0x00000000u },
+    { "fighter_35838@side",       m_35838_side,   0x00000000u },
+    { "fighter_35838@order",      m_35838_order,  0x00000000u },
+    { "fighter_468d8@mutant",     m_468d8,        0x000000FFu },
+    { "fighter_468d8@eq",         m_468d8_eq,     0x000000FFu },
+    { "fighter_468d8@side",       m_468d8_side,   0x000000FFu },
+    { "fighter_3c208",            b_3c208,        0x00000000u },
+    { "fighter_3c208@mutant",     m_3c208,        0x00000000u },
+    { "fighter_3c208@abs",        m_3c208_abs,    0x00000000u },
+    { "fighter_3c208@arg",        m_3c208_arg,    0x00000000u },
+    { "fighter_3c208@early",      m_3c208_early,  0x00000000u },
+    { "fighter_18c14",            b_18c14,        0x000000FFu },
+    { "fighter_18c14@mutant",     m_18c14,        0x000000FFu },
+    { "fighter_18c14@store",      m_18c14_store,  0x000000FFu },
+    { "fighter_18c14@live",       m_18c14_live,   0x000000FFu },
 };
 
 static const binding_t *find_binding(const char *name)
diff --git a/tools/diff_verify.py b/tools/diff_verify.py
index 3caa04f..679ba5a 100644
--- a/tools/diff_verify.py
+++ b/tools/diff_verify.py
@@ -1379,6 +1379,367 @@ P3_SPECS += [
        mutants=("@mutant", "@signed", "@abs", "@order", "@bound", "@zext", "@width")),
 ]
 
+# ---- track P batch C1: the callee rows (record 2026-10-03-reverse-c1) -----------------------------
+# The ported callees the P1-P3 rows stub, each with its own row so the dependent rows close (P3
+# record §P3.10). Every field a row writes carries a sentinel that differs from what it writes and
+# every neighbour byte is seeded (the P-track checklist); the two records E3_REC/E3_REC2 and the
+# two slots DS_SLOTS/DS_SLOTS+0x94 are the E3 fixtures.
+
+# Both records seeded slot-distinct: the low record A (0x32..) and B (0x62..); every byte of
+# +0x32..+0x39, +0x41..+0x47 and +0x0C/+0x1C/+0x2C/+0x34/+0x42/+0x43/+0x59/+0x74 sentinels.
+def c1_rec(rec, base):
+    return {rec + 0x32: bytes(range(base, base + 8)),           # +0x32..+0x39
+            rec + 0x41: bytes(range(base + 0x0F, base + 0x16))}  # +0x41..+0x47
+
+
+C1_A = c1_rec(E3_REC, 0x32)
+C1_B = c1_rec(E3_REC2, 0x62)
+C1_PTRS = dict(SLOT_PTRS)
+
+
+def c1_slots(seed0, seed1):
+    return {DS_SLOTS + 0x42: seed0, DS_SLOTS + 0x94 + 0x42: seed1}
+
+
+C1_SLOT_SEED0 = b"\x42\xff\x44\x52\x53\x54\x5c\x5d\x5e"       # +0x42..+0x44, +0x52..+0x54, +0x5c..+0x5e
+C1_SLOT_SEED1 = b"\x62\xef\x64\x72\x73\x74\x7c\x7d\x7e"
+C1_PSET = 0x10A800                # the pset base 0x1014EC points at (32-byte entries)
+C1_STR = 0x10A700                 # the animation-stream words the 0x2BC30 cases walk
+C1_18 = {E3_REC + 0x16: b"\x16\x17\x18\x19\x1a\x1b", E3_REC + 0x1C: b"\x1c\x1c\x1c\x1c",
+         E3_REC2 + 0x16: b"\x26\x27\x28\x29\x2a\x2b", E3_REC2 + 0x1C: b"\x2c\x2c\x2c\x2c"}
+
+# The callee declarations C1's rows stub (record §C1.2). Clobbers are E.callee_clobbers over the
+# image (re-derived by test_each_stub_declares_the_registers_its_callee_clobbers).
+C1_SLOT_LATCH = E.Call(0x186D0, ("eax",))
+C1_RECORD_X = E.Call(0x18714, ("eax",))
+C1_FACING = E.Call(0x18B04, ("eax",))
+C1_FACING0 = E.Call(0x18AF8, (), mode="real")
+C1_PAL_REL = E.Call(0x33864, ("eax",))
+C1_PAL_ACQ = E.Call(0x33754, ("eax",))
+C1_ANIM_OPCODE = E.Call(0x2B2A0, ("eax", "edx", "ebx"), clobbers=("ebx", "edx"))
+C1_SPRITE_ID = E.Call(0x2A408, ("eax", "edx"), clobbers=("edx",))
+C1_POSE = E.Call(0x39F40, ("eax", "edx", "ebx", "ecx", "s0"), pop=4, clobbers=("ebx", "edx"))
+C1_36638 = E.Call(0x36638, ("eax", "edx"), clobbers=("edx",))
+C1_AI_DIST = E.Call(0x187FC, ())
+C1_1883C = E.Call(0x1883C, ("eax", "edx", "ebx"), clobbers=("ebx", "edx"))
+C1_3B8D8 = E.Call(0x3B8D8, ("eax", "edx"), clobbers=("edx",))
+C1_3B90C = E.Call(0x3B90C, ("eax", "edx"), clobbers=("edx",))
+C1_GEOM = E.Call(0x1DDF4, ("eax", "edx", "ebx"), clobbers=("ebx", "edx"))
+C1_18B44 = E.Call(0x18B44, ("eax",))
+C1_189FC = E.Call(0x189FC, ("eax",))
+C1_18A4C = E.Call(0x18A4C, ("eax",))
+C1_39EFC = E.Call(0x39EFC, ("eax",))
+C1_FLAGS = 0x10A600                # the 16 flag bytes the 0x18C14 cases poke
+C1_18C14_SEED = {
+    DS_SLOTS + 0x42: b"\x40\x41\x42\x43", DS_SLOTS + 0x52: b"\x52\x53\x54\x55",
+    DS_SLOTS + 0x5F: b"\x5f\x60\x61\x62\x63", DS_SLOTS + 0x8A: b"\x8a",
+    DS_SLOTS + 0x94 + 0x42: b"\x50\x51\x52\x53", DS_SLOTS + 0x94 + 0x52: b"\x62\x63\x64\x65",
+    DS_SLOTS + 0x94 + 0x5F: b"\x6f\x70\x71\x72\x73", DS_SLOTS + 0x94 + 0x74: b"\x74\x75\x76\x77",
+    E3_REC + 0x61: b"\x61", 0x100AF8: b"\x00\x00\x00\x00",
+}
+
+
+def c1_18c14(cid, flag, val, hit, stubs=None, extra=None):
+    flags = [2] * 16
+    flags[flag] = val
+    pokes = {**C1_PTRS, **C1_18C14_SEED, C1_FLAGS: bytes(flags)}
+    pokes.update(extra or {})
+    return Case(cid, {"eax": 0, "edx": C1_FLAGS, "ebx": 0, "ecx": 0}, pokes, stubs or {})
+
+
+def c1_18c14_cases():
+    out = []
+    for flag in range(16):
+        for val in (0, 1):
+            for hit in (False, True):
+                cid = "g%X%s%s" % (flag, val, "h" if hit else "n")
+                stubs, extra = {}, {}
+                if flag == 0:
+                    le = (val == 1) if hit else (val == 0)
+                    extra[0x100AF8] = le32(0 if le else 1)
+                elif flag == 1:
+                    if val == 0:
+                        extra[DS_SLOTS + 0x94 + 0x74] = b"\x01\x00" if hit else b"\x00\x00"
+                        extra[DS_SLOTS + 0x94 + 0x76] = b"\x00\x00" if hit else b"\x01\x00"
+                    else:
+                        extra[DS_SLOTS + 0x94 + 0x74] = b"\x01\x00" if hit else b"\x01\x00"
+                        extra[DS_SLOTS + 0x94 + 0x76] = b"\x00\x00" if hit else b"\x02\x00"
+                elif flag == 0xF:
+                    extra[DS_SLOTS + 0x43] = b"\x04" if hit == (val == 0) else b"\x00"
+                elif flag in (2, 3, 6, 4):
+                    v = {2: (0, 1), 3: (1, 0), 6: (7, 0), 4: (2, 0)}[flag][0 if hit == (val == 0) else 1]
+                    extra[DS_SLOTS + 0x94 + 0x54] = bytes([v])
+                elif flag in (5, 9, 0xA, 0xD, 0xE):
+                    addr = {5: 0x1DDF4, 9: 0x189FC, 0xA: 0x18A4C, 0xD: 0x39EFC, 0xE: 0x3B298}[flag]
+                    stubs[addr] = 1 if hit == (val == 0) else 0
+                elif flag == 7:
+                    extra[DS_SLOTS + 0x94 + 0x62] = b"\x01" if hit == (val == 0) else b"\x00"
+                elif flag == 8:
+                    extra[DS_SLOTS + 0x94 + 0x42] = b"\x08" if hit == (val == 0) else b"\x00"
+                elif flag == 0xB:
+                    extra[E3_REC + 0x61] = b"\x01" if hit == (val == 0) else b"\x00"
+                elif flag == 0xC:
+                    extra[DS_SLOTS + 0x94 + 0x53] = b"\x0a" if hit == (val == 0) else b"\x0b"
+                out.append(c1_18c14(cid, flag, val, hit, stubs, extra))
+    return out
+
+
+
+C1_2BC30_SEED = {
+    E3_REC + 0x08: le32(0x08080808), E3_REC + 0x0C: b"\x0c" * 8, E3_REC + 0x20: b"\x20" * 8,
+    E3_REC + 0x28: b"\xff\xff\xff\xff", E3_REC + 0x50: b"\x50\x51\x52\x53",
+    E3_REC + 0x54: b"\x54\x55\x00\x00\x58\x59", E3_REC + 0x5F: b"\x5f\x60\x61\x62",
+    0x1014EC: le32(C1_PSET), C1_PSET: b"\x00\x00\x02\x02",
+}
+C1_35838_SEED = {
+    **C1_PTRS, DS_SLOTS + 0x40: b"\x40\x41\x42\x43\x44\x45\x46\x47", DS_SLOTS + 0x4C: b"\x4c\x4d\x4e\x4f\x50\x51\x52\x53",
+    DS_SLOTS + 0x7A: b"\x01", 0x104B00: b"\x11\x00",
+}
+C1_468D8_SEED = {
+    **C1_PTRS, DS_SLOTS + 0x10: le32(0), DS_SLOTS + 0x52: b"\x52", DS_SLOTS + 0x54: b"\x54",
+    E3_REC + 0x24: le32(0x24242424),
+    DS_SLOTS + 0x94 + 0x10: le32(0), DS_SLOTS + 0x94 + 0x52: b"\x62", DS_SLOTS + 0x94 + 0x54: b"\x64",
+    E3_REC2 + 0x24: le32(0x34343434),
+}
+
+
+C1_SPECS = [
+    # 0x3C148/0x3C16C (record §48-C): clear the side's record fields; EAX = side, mask 0.
+    Spec("fighter_3c148", 0x3C148, [
+        Case("c0", {"eax": 0}, {**C1_PTRS, **C1_A, **C1_B}),
+        Case("c1", {"eax": 1}, {**C1_PTRS, **C1_A, **C1_B}),
+    ], eax_mask=0, mutants=("@mutant", "@side", "@width")),
+    Spec("fighter_3c16c", 0x3C16C, [
+        Case("c0", {"eax": 0}, {**C1_PTRS, **C1_A, **C1_B}),
+        Case("c1", {"eax": 1}, {**C1_PTRS, **C1_A, **C1_B}),
+    ], eax_mask=0, mutants=("@mutant", "@side", "@width")),
+    # 0x39A10: EAX = rec, EDX = value; the side is rec+0x51, zero-extended (`and eax,0xff` at
+    # 0x39A14); the store is the word +0x74 of the side's slot (0x107824 = 0x1077B0 + 0x74).
+    Spec("fighter_39a10", 0x39A10, [
+        Case("t0", {"eax": E3_REC, "edx": 0x12345678},
+             {**C1_PTRS, E3_REC + 0x51: b"\x00", DS_SLOTS + 0x72: b"\x72\x73\x74\x75\x76\x77",
+              DS_SLOTS + 0x94 + 0x72: b"\x82\x83\x84\x85\x86\x87"}),
+        Case("t1", {"eax": E3_REC2, "edx": 0xFFFF},
+             {**C1_PTRS, E3_REC2 + 0x51: b"\x01", DS_SLOTS + 0x72: b"\x72\x73\x74\x75\x76\x77",
+              DS_SLOTS + 0x94 + 0x72: b"\x82\x83\x84\x85\x86\x87"}),
+    ], eax_mask=0, mutants=("@mutant", "@side")),
+    # 0x36D98: EAX = the slot (DS_001077B0 + side*0x94); the side is its record's +0x51.
+    Spec("fighter_36d98", 0x36D98, [
+        Case("r0", {"eax": DS_SLOTS},
+             {**C1_PTRS, E3_REC + 0x51: b"\x00", **c1_slots(C1_SLOT_SEED0, C1_SLOT_SEED1),
+              0x1078F2: b"\x11\x22"}),
+        Case("r1", {"eax": DS_SLOTS + 0x94},
+             {**C1_PTRS, E3_REC2 + 0x51: b"\x01", **c1_slots(C1_SLOT_SEED0, C1_SLOT_SEED1),
+              0x1078F2: b"\x11\x22"}),
+    ], eax_mask=0, mutants=("@mutant", "@side", "@and")),
+    # 0x18BD4: EAX = the 16 flag bytes; fill them with 2 (`mov byte [eax+7]` last, 0x18C0B).
+    Spec("fighter_18bd4", 0x18BD4, [
+        Case("b0", {"eax": E3_OUT}, {E3_OUT: b"\xaa" * 16 + b"\xbb"}),
+        Case("b1", {"eax": E3_SLOT + 0x60}, {E3_SLOT + 0x60: b"\xcc" * 16 + b"\xdd"}),
+    ], eax_mask=0, mutants=("@mutant", "@val", "@off")),
+    # 0x34D8C: the +0x59 palette-flash pair, only when byte 0x1078FA == 2 (`xor eax,eax; mov
+    # al,[0x1078fa]; cmp eax,2` 0x34D90..0x34D9A). EAX = side.
+    Spec("fighter_34d8c", 0x34D8C, [
+        Case("f0", {"eax": 0}, {**C1_PTRS, **C1_A, **C1_B, 0x1078FA: b"\x01"}),
+        Case("f1", {"eax": 0}, {**C1_PTRS, **C1_A, **C1_B, 0x1078FA: b"\x02"}),
+        Case("f2", {"eax": 1}, {**C1_PTRS, **C1_A, **C1_B, 0x1078FA: b"\x02"}),
+    ], eax_mask=0, mutants=("@mutant", "@side")),
+    # 0x3C358: EAX = side; 0x33950 runs on both sides (allow); every field seeded on both slots
+    # and both records (the +0x0C and +0x1C dwords included).
+    Spec("fighter_3c358", 0x3C358, [
+        Case("s0", {"eax": 0},
+             {**C1_PTRS, **c1_slots(C1_SLOT_SEED0, C1_SLOT_SEED1),
+              DS_SLOTS + 0x0C: le32(0x0C0C0C0C), DS_SLOTS + 0x94 + 0x0C: le32(0x1C1C1C1C),
+              **C1_A, **C1_B, E3_REC + 0x1C: le32(0x1C1C1C1C), E3_REC2 + 0x1C: le32(0x2C2C2C2C)}),
+        Case("s1", {"eax": 1},
+             {**C1_PTRS, **c1_slots(C1_SLOT_SEED0, C1_SLOT_SEED1),
+              DS_SLOTS + 0x0C: le32(0x0C0C0C0C), DS_SLOTS + 0x94 + 0x0C: le32(0x1C1C1C1C),
+              **C1_A, **C1_B, E3_REC + 0x1C: le32(0x1C1C1C1C), E3_REC2 + 0x1C: le32(0x2C2C2C2C)}),
+    ], allow_calls=(0x33950,), eax_mask=0, mutants=("@mutant", "@side", "@42")),
+    # 0x3C190: EAX = side, EDX = v; 0x1A570(side)'s AL negates v; the word +0x34 of the side's
+    # slot takes the low 16 bits.
+    Spec("fighter_3c190", 0x3C190, [
+        Case("v0", {"eax": 0, "edx": 0x1234}, {**C1_PTRS, **C1_A, **C1_B}, {0x1A570: 0}),
+        Case("v1", {"eax": 0, "edx": 0x1234}, {**C1_PTRS, **C1_A, **C1_B}, {0x1A570: 1}),
+        Case("v2", {"eax": 1, "edx": 0xFFFF8000}, {**C1_PTRS, **C1_A, **C1_B}, {0x1A570: 1}),
+    ], calls=(BIT15,), eax_mask=0, mutants=("@mutant", "@arg", "@width")),
+    # 0x3C480: EAX = rec, EDX = stream, s0 = frame bits; 0x339AC runs on both sides (allow), the
+    # anchor pair and 0x2BC30 are stubbed (the caller reads none of their effects).
+    Spec("fighter_3c480", 0x3C480, [
+        Case("a0", {"eax": E3_REC, "edx": 0xE1234, "s0": 0x40000000},
+             {**C1_PTRS, E3_REC + 0x51: b"\x00", E3_REC + 0x18: le32(0x18181818),
+              DS_SLOTS + 0x2C: le32(0x2C2C2C2C), DS_SLOTS + 0x94 + 0x2C: le32(0x3C3C3C3C),
+              E3_REC2 + 0x18: le32(0x28282828)}),
+        Case("a1", {"eax": E3_REC2, "edx": 0xE5678, "s0": 0x3F800000},
+             {**C1_PTRS, E3_REC2 + 0x51: b"\x01", E3_REC + 0x18: le32(0x18181818),
+              DS_SLOTS + 0x2C: le32(0x2C2C2C2C), DS_SLOTS + 0x94 + 0x2C: le32(0x3C3C3C3C),
+              E3_REC2 + 0x18: le32(0x28282828)}),
+    ], allow_calls=(0x339AC,), calls=(ANCHOR, ANIM_BEGIN, ANCHORX), eax_mask=0,
+       mutants=("@mutant", "@order", "@side")),
+    # 0x188AC (P1's ANCHOR): EAX = side, EDX = x, EBX = y; the record's +0x18/+0x1C
+    # take x/y, then 0x186D0(side) re-latches the slot.
+    Spec("fighter_188ac", 0x188AC, [
+        Case("a0", {"eax": 0, "edx": 0x11223344, "ebx": 0x55667788},
+             {**C1_PTRS, E3_REC + 0x16: b"\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f",
+              E3_REC2 + 0x16: b"\x66\x67\x68\x69\x6a\x6b\x6c\x6d\x6e\x6f"}),
+        Case("a1", {"eax": 1, "edx": 0x99AABBCC, "ebx": 0xDDEEFF00},
+             {**C1_PTRS, E3_REC + 0x16: b"\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f",
+              E3_REC2 + 0x16: b"\x66\x67\x68\x69\x6a\x6b\x6c\x6d\x6e\x6f"}),
+    ], calls=(C1_SLOT_LATCH,), eax_mask=0, mutants=("@mutant", "@side", "@latch")),
+    # 0x188DC (P3's ANCHORX): EAX = side, EDX = x; the slot's +0x2C takes x, then
+    # the record's +0x18 takes 0x18714(side)'s result.
+    Spec("fighter_188dc", 0x188DC, [
+        Case("d0", {"eax": 0, "edx": 0x11223344},
+             {**C1_PTRS, **C1_18, DS_SLOTS + 0x2A: b"\x2a\x2b\x2c\x2c\x2e\x2f",
+              DS_SLOTS + 0x94 + 0x2A: b"\x3a\x3b\x3c\x3c\x3e\x3f"}, {0x18714: 0x11111111}),
+        Case("d1", {"eax": 1, "edx": 0x99AABBCC},
+             {**C1_PTRS, **C1_18, DS_SLOTS + 0x2A: b"\x2a\x2b\x2c\x2c\x2e\x2f",
+              DS_SLOTS + 0x94 + 0x2A: b"\x3a\x3b\x3c\x3c\x3e\x3f"}, {0x18714: 0x22222222}),
+    ], calls=(C1_RECORD_X,), eax_mask=0, mutants=("@mutant", "@side", "@arg", "@eax")),
+    # 0x18AF8: `xor eax,eax; call 0x18B04` (recorded, stubbed) then `mov eax,1` falls
+    # through into 0x18B04's body with EAX = 1 (no second call). The body runs on both
+    # sides: 0x33950 allow, then 0x18714(side) into the record's +0x18 and the +0x29
+    # bit 0x40 by the two slots' +0x2C comparison (side 1 only: the fall-through's EAX).
+    Spec("fighter_18af8", 0x18AF8, [
+        Case("f0", {}, {**C1_PTRS, E3_REC2 + 0x29: b"\x29", E3_REC2 + 0x16: b"\x26\x27\x28\x29\x2a\x2b",
+                        DS_SLOTS + 0x2C: le32(0x22222222), DS_SLOTS + 0x94 + 0x2C: le32(0x11111111)},
+             {0x18B04: 0, 0x18714: 0x33333333}),
+        Case("f1", {}, {**C1_PTRS, E3_REC2 + 0x29: b"\x29", E3_REC2 + 0x16: b"\x26\x27\x28\x29\x2a\x2b",
+                        DS_SLOTS + 0x2C: le32(0x11111111), DS_SLOTS + 0x94 + 0x2C: le32(0x11111111)},
+             {0x18B04: 1, 0x18714: 0x44444444}),
+    ], allow_calls=(0x33950,), calls=(C1_FACING, C1_RECORD_X), eax_mask=0,
+       mutants=("@mutant", "@once", "@le")),
+    # 0x2A17C (P1's PALETTE): EAX = rec, EDX = word, EBX = handle; pset+2 takes the word
+    # with 0x800 when rec+0x5F is set; a zero handle returns; else the old pset+0x18 is
+    # released (0x33864) and 0x33754(handle) is stored.
+    Spec("fighter_2a17c", 0x2A17C, [
+        Case("p0", {"eax": E3_REC, "edx": 0x1234, "ebx": 0},
+             {**C1_PTRS, 0x1014EC: le32(C1_PSET), 0x1014F4: le32(E3_REC), E3_REC + 0x56: b"\x00\x00",
+              E3_REC + 0x5F: b"\x00", C1_PSET + 0x02: b"\x02\x02", C1_PSET + 0x18: le32(0)}),
+        Case("p1", {"eax": E3_REC, "edx": 0x5678, "ebx": 0x77},
+             {**C1_PTRS, 0x1014EC: le32(C1_PSET), 0x1014F4: le32(E3_REC), E3_REC + 0x56: b"\x00\x00",
+              E3_REC + 0x5F: b"\x01", C1_PSET + 0x02: b"\x02\x02", C1_PSET + 0x18: le32(0)},
+             {0x33754: 0xAABBCCDD}),
+        Case("p2", {"eax": E3_REC, "edx": 0x9ABC, "ebx": 0x88},
+             {**C1_PTRS, 0x1014EC: le32(C1_PSET), 0x1014F4: le32(E3_REC), E3_REC + 0x56: b"\x00\x00",
+              E3_REC + 0x5F: b"\x00", C1_PSET + 0x02: b"\x02\x02", C1_PSET + 0x18: le32(0x1234)},
+             {0x33754: 0x11223344}),
+    ], calls=(C1_PAL_REL, C1_PAL_ACQ), eax_mask=0, mutants=("@mutant", "@order", "@arg", "@early"),
+       # 0x2A1AC's EBX==0 return runs before 0x2A1E6's test, so the 0x2A1F5 store is dead
+       unhit_named={0x2A1F5: "dead: EBX == 0 already returned at 0x2A1AC"}),
+    # 0x2BC30 (E3's ANIM_BEGIN): EAX = rec, EDX = stream, s0 = frame bits; the stream
+    # opcode walk calls 0x2B2A0 (stub EAX 0 continues, 1 stops, 2 stops after +2) and
+    # the pset word takes 0x2A408's low word.
+    Spec("fighter_2bc30", 0x2BC30, [
+        Case("n0", {"eax": E3_REC, "edx": C1_STR, "s0": 0x40000000},
+             {**C1_2BC30_SEED, C1_STR: b"\x00\x00\x00\x00"}, {0x2A408: 0x1234}),
+        Case("n1", {"eax": E3_REC, "edx": C1_STR, "s0": 0x40000000},
+             {**C1_2BC30_SEED, C1_STR: b"\x00\x80\x00\x00"}, {0x2A408: 0x2345, 0x2B2A0: 1}),
+        Case("n2", {"eax": E3_REC, "edx": C1_STR, "s0": 0x40000000},
+             {**C1_2BC30_SEED, C1_STR: b"\x00\x80\x00\x00"}, {0x2A408: 0x3456, 0x2B2A0: 2}),
+        Case("n3", {"eax": E3_REC, "edx": C1_STR, "s0": 0x40000000},
+             {**C1_2BC30_SEED, C1_STR: b"\x00\x80\x00\x80\x00\x00\x00\x00"},
+             {0x2A408: 0x4567, 0x2B2A0: 0}),
+    ], calls=(C1_ANIM_OPCODE, C1_SPRITE_ID), eax_mask=0, mutants=("@mutant", "@order", "@frame"),
+       # 0x2BC61 `xor eax,eax` makes the 0x2BC66 test always take the jump: the fild block is dead
+       unhit_named={0x2BC6D: "dead: EAX is 0 at 0x2BC66 (`xor eax,eax` at 0x2BC61)"}),
+    # 0x39FB0: EAX = slot; the side is its record's +0x51; 0x33A10 runs on both sides
+    # (allow); 0x18B04(ctx[1]) then the 0x39F40 pose with (ctx[1], -0x50, 0x64, 0xF, 0x14).
+    Spec("fighter_39fb0", 0x39FB0, [
+        Case("g0", {"eax": DS_SLOTS}, {**C1_PTRS, E3_REC + 0x51: b"\x00"}),
+        Case("g1", {"eax": DS_SLOTS + 0x94}, {**C1_PTRS, E3_REC2 + 0x51: b"\x01"}),
+    ], allow_calls=(0x33A10,), calls=(C1_FACING, C1_POSE), eax_mask=0,
+       mutants=("@mutant", "@side", "@order")),
+    # 0x3A95C: EAX = side, EDX = b; 0x33A10 runs on both sides (allow); 0x188AC(ctx[1],
+    # ctx[5].+0x18, 0); the other slot 0x10/0x0A/0/0x10=0; 0x2BC30(ctx[5], the char
+    # stream, 3.0); ctx[3].+0x7E = byte[0xBECF8] + b.
+    Spec("fighter_3a95c", 0x3A95C, [
+        Case("c0", {"eax": 0, "edx": 0x21},
+             {**C1_PTRS, E3_REC2 + 0x18: le32(0x18181818), E3_REC + 0x18: le32(0x28282828),
+              DS_SLOTS + 0x52: b"\x52\x53\x54\x55", DS_SLOTS + 0x94 + 0x52: b"\x62\x63\x64\x65",
+              DS_SLOTS + 0x94 + 0x10: b"\x10\x10\x10\x10", DS_SLOTS + 0x94 + 0x7A: b"\x00",
+              DS_SLOTS + 0x94 + 0x7E: b"\x7e\x7f"}),
+        Case("c1", {"eax": 1, "edx": 0x42},
+             {**C1_PTRS, E3_REC2 + 0x18: le32(0x18181818), E3_REC + 0x18: le32(0x28282828),
+              DS_SLOTS + 0x52: b"\x52\x53\x54\x55", DS_SLOTS + 0x94 + 0x52: b"\x62\x63\x64\x65",
+              DS_SLOTS + 0x10: b"\x20\x20\x20\x20", DS_SLOTS + 0x7A: b"\x01",
+              DS_SLOTS + 0x7E: b"\x6e\x6f"}),
+    ], allow_calls=(0x33A10,), calls=(ANCHOR, ANIM_BEGIN), eax_mask=0,
+       mutants=("@mutant", "@side", "@arg")),
+    # 0x35838 (P3's DIRS): EAX = slot, EDX = rec, EBX = dirbits; the rec+0x28 bit
+    # 0x4000 gate and dirbits 0x2000/0x1000 select the two animation streams (or
+    # 0x36638 when mode 0x104B00 == 0x22).
+    Spec("fighter_35838", 0x35838, [
+        Case("s0", {"eax": DS_SLOTS, "edx": E3_REC, "ebx": 0},
+             {**C1_35838_SEED, E3_REC + 0x28: b"\x00\x00"}),
+        Case("s1", {"eax": DS_SLOTS, "edx": E3_REC, "ebx": 0x2000},
+             {**C1_35838_SEED, E3_REC + 0x28: b"\x00\x00"}),
+        Case("s2", {"eax": DS_SLOTS, "edx": E3_REC, "ebx": 0},
+             {**C1_35838_SEED, E3_REC + 0x28: b"\x00\x00", 0x104B00: b"\x22\x00"}),
+        Case("s3", {"eax": DS_SLOTS, "edx": E3_REC, "ebx": 0x1000},
+             {**C1_35838_SEED, E3_REC + 0x28: b"\x00\x40"}),
+        Case("s4", {"eax": DS_SLOTS, "edx": E3_REC, "ebx": 0},
+             {**C1_35838_SEED, E3_REC + 0x28: b"\x00\x40", 0x104B00: b"\x22\x00"}),
+        Case("s5", {"eax": DS_SLOTS, "edx": E3_REC, "ebx": 0},
+             {**C1_35838_SEED, E3_REC + 0x28: b"\x00\x40"}),
+    ], calls=(ANIM_BEGIN, C1_36638), eax_mask=0, mutants=("@mutant", "@side", "@order")),
+    # 0x468D8 (P3's PRED): EAX = side; 0x33A10 runs on both sides (allow); 1 when the
+    # side slot's +0x10 handler is 0x22BEC, its record's +0x24 low 31 bits are clear and
+    # +0x54 is not 2; else 1 when +0x52 is 7.
+    Spec("fighter_468d8", 0x468D8, [
+        Case("h0", {"eax": 0}, {**C1_468D8_SEED, DS_SLOTS + 0x10: le32(0x22BEC), E3_REC + 0x24: le32(0)}),
+        Case("h1", {"eax": 0}, {**C1_468D8_SEED, DS_SLOTS + 0x10: le32(0x1234), DS_SLOTS + 0x52: b"\x07"}),
+        Case("h2", {"eax": 0}, {**C1_468D8_SEED, DS_SLOTS + 0x10: le32(0x22BEC),
+                                E3_REC + 0x24: le32(0x80000000)}),
+        Case("h3", {"eax": 0}, {**C1_468D8_SEED, DS_SLOTS + 0x10: le32(0x22BEC), E3_REC + 0x24: le32(0),
+                                DS_SLOTS + 0x54: b"\x02"}),
+        Case("h4", {"eax": 0}, {**C1_468D8_SEED, DS_SLOTS + 0x10: le32(0x22BEC), E3_REC + 0x24: le32(0),
+                                DS_SLOTS + 0x52: b"\x07"}),
+        Case("h5", {"eax": 0}, {**C1_468D8_SEED, DS_SLOTS + 0x10: le32(0x1234)}),
+        Case("h6", {"eax": 1}, {**C1_468D8_SEED, DS_SLOTS + 0x94 + 0x10: le32(0x22BEC),
+                                E3_REC2 + 0x24: le32(0)}),
+    ], allow_calls=(0x33A10,), eax_mask=0xFF, mutants=("@mutant", "@eq", "@side")),
+    # 0x3C208 (P2's PLACE): EAX = side, EDX = dist; both slots latch, 0x18AF8 sets the
+    # facing flags, both records clear +0x34/+0x43/+0x42; then 0x187FC's |d| against
+    # |dist|: farther, 0x1883C(other, ±gap, 0) by 0x1A570; closer, 0x3B8D8(other, ±gap)
+    # then 0x1883C or 0x3B90C and the two anchors (slot[other]+0x2C ± want).
+    Spec("fighter_3c208", 0x3C208, [
+        Case("c0", {"eax": 0, "edx": 0x100},
+             {**C1_PTRS, **C1_A, **C1_B, DS_SLOTS + 0x2C: le32(0x1234), DS_SLOTS + 0x94 + 0x2C: le32(0x5678)},
+             {0x187FC: 0x200, 0x1A570: 0}),
+        Case("c1", {"eax": 0, "edx": 0x100},
+             {**C1_PTRS, **C1_A, **C1_B, DS_SLOTS + 0x2C: le32(0x1234), DS_SLOTS + 0x94 + 0x2C: le32(0x5678)},
+             {0x187FC: 0x200, 0x1A570: 1}),
+        Case("c2", {"eax": 0, "edx": 0xFFFFFF00},
+             {**C1_PTRS, **C1_A, **C1_B, DS_SLOTS + 0x2C: le32(0x1234), DS_SLOTS + 0x94 + 0x2C: le32(0x5678)},
+             {0x187FC: 0x200, 0x1A570: 0}),
+        Case("c3", {"eax": 0, "edx": 0x100},
+             {**C1_PTRS, **C1_A, **C1_B, DS_SLOTS + 0x2C: le32(0x1234), DS_SLOTS + 0x94 + 0x2C: le32(0x5678)},
+             {0x187FC: 0x50, 0x1A570: 0, 0x3B8D8: 0}),
+        Case("c4", {"eax": 0, "edx": 0x100},
+             {**C1_PTRS, **C1_A, **C1_B, DS_SLOTS + 0x2C: le32(0x1234), DS_SLOTS + 0x94 + 0x2C: le32(0x5678)},
+             {0x187FC: 0x50, 0x1A570: 0, 0x3B8D8: 1, 0x3B90C: 0x777}),
+        Case("c5", {"eax": 0, "edx": 0x100},
+             {**C1_PTRS, **C1_A, **C1_B, DS_SLOTS + 0x2C: le32(0x1234), DS_SLOTS + 0x94 + 0x2C: le32(0x5678)},
+             {0x187FC: 0x50, 0x1A570: 1, 0x3B8D8: 0}),
+        Case("c6", {"eax": 0, "edx": 0x100},
+             {**C1_PTRS, **C1_A, **C1_B, DS_SLOTS + 0x2C: le32(0x1234), DS_SLOTS + 0x94 + 0x2C: le32(0x5678)},
+             {0x187FC: 0x50, 0x1A570: 1, 0x3B8D8: 1, 0x3B90C: 0x888}),
+        Case("c7", {"eax": 0, "edx": 0x100},
+             {**C1_PTRS, **C1_A, **C1_B, DS_SLOTS + 0x2C: le32(0x1234), DS_SLOTS + 0x94 + 0x2C: le32(0x5678)},
+             {0x187FC: 0xFFFFFFF0, 0x1A570: 0, 0x3B8D8: 0}),
+    ], allow_calls=(0x33950,),
+       calls=(C1_SLOT_LATCH, C1_FACING0, C1_FACING, C1_RECORD_X, C1_AI_DIST, BIT15, C1_1883C, C1_3B8D8, C1_3B90C, ANCHORX),
+       eax_mask=0, mutants=("@mutant", "@abs", "@arg", "@early")),
+    # 0x18C14 (P2's CHECKS): EAX = side, EDX = the 16 flag bytes (poked; the binding reads
+    # mem[EDX]); 0x33950 allow; each flag 2 skips, 0/1 tests (flag 0 rewrites to 4/3); flags
+    # 7/0xD/0xE store ctx[2]+0x8A and 7/0xD call 0x18B44; returns 0 only when every check
+    # passes. The 64 cases: every flag, value 0/1, condition holding and not.
+    Spec("fighter_18c14", 0x18C14, c1_18c14_cases(),
+       allow_calls=(0x33950,), calls=(C1_GEOM, C1_18B44, C1_189FC, C1_18A4C, C1_39EFC, DISPATCH),
+       eax_mask=0xFF, mutants=("@mutant", "@store", "@live")),
+]
+
 SPECS = [
     Spec("rng_next", 0x5D7DC, [
         Case("r1", {"eax": 0x1234}, {DS_RNG: le32(0x12345678)}),
@@ -1421,7 +1782,7 @@ SPECS = [
         Case("d0", {}, {DS_1078FC: b"\x00"}),
         Case("d1", {"eax": U6_REC}, {DS_1078FC: b"\x5a"}),
     ], eax_mask=0),
-] + E3_SPECS + P1_SPECS + P1_ANIM_SPECS + P2_SPECS + P3_SPECS
+] + E3_SPECS + P1_SPECS + P1_ANIM_SPECS + P2_SPECS + P3_SPECS + C1_SPECS
 
 
 # ---- driver -------------------------------------------------------------------------------------
diff --git a/tools/tests/test_diff_verify.py b/tools/tests/test_diff_verify.py
index fa69539..9b6eec4 100644
--- a/tools/tests/test_diff_verify.py
+++ b/tools/tests/test_diff_verify.py
@@ -404,6 +404,77 @@ P3_KINDS = {"fighter_475ec@mutant": {"call #0 memory"}, "fighter_475ec@side": {"
             "fighter_4844c@zext": {"call #4"}, "fighter_4844c@width": {"byte", "call #0 memory", "call #1 memory", "call #2 memory",
                                     "call #3 memory", "call #4 memory"}}
 
+# Track P batch C1 (record 2026-10-03-reverse-c1): the callee rows with their EAX masks, and what
+# alone catches each of their mutants.
+C1_MASKS = {"fighter_3c148": 0, "fighter_3c16c": 0, "fighter_39a10": 0, "fighter_36d98": 0,
+            "fighter_18bd4": 0, "fighter_34d8c": 0, "fighter_3c358": 0, "fighter_3c190": 0,
+            "fighter_3c480": 0, "fighter_188ac": 0, "fighter_188dc": 0, "fighter_18af8": 0,
+            "fighter_2a17c": 0, "fighter_2bc30": 0, "fighter_39fb0": 0, "fighter_3a95c": 0,
+            "fighter_35838": 0, "fighter_468d8": 0xFF, "fighter_3c208": 0, "fighter_18c14": 0xFF}
+C1_KINDS = {
+    "fighter_3c148@mutant": {"byte"},
+    "fighter_3c148@side": {"byte"},
+    "fighter_3c148@width": {"byte"},
+    "fighter_3c16c@mutant": {"byte"},
+    "fighter_3c16c@side": {"byte"},
+    "fighter_3c16c@width": {"byte"},
+    "fighter_39a10@mutant": {"byte"},
+    "fighter_39a10@side": {"byte"},
+    "fighter_36d98@mutant": {"byte"},
+    "fighter_36d98@side": {"byte"},
+    "fighter_36d98@and": {"byte"},
+    "fighter_18bd4@mutant": {"byte"},
+    "fighter_18bd4@val": {"byte"},
+    "fighter_18bd4@off": {"byte"},
+    "fighter_34d8c@mutant": {"byte"},
+    "fighter_34d8c@side": {"byte"},
+    "fighter_3c358@mutant": {"byte"},
+    "fighter_3c358@side": {"byte"},
+    "fighter_3c358@42": {"byte"},
+    "fighter_3c190@mutant": {"byte", "call #0"},
+    "fighter_3c190@arg": {"call #0"},
+    "fighter_3c190@width": {"byte"},
+    "fighter_3c480@mutant": {"call #2"},
+    "fighter_3c480@order": {"call #0", "call #1"},
+    "fighter_3c480@side": {"call #0", "call #2"},
+    "fighter_188ac@mutant": {"byte", "call #0 memory"},
+    "fighter_188ac@side": {"byte", "call #0", "call #0 memory"},
+    "fighter_188ac@latch": {"call #0"},
+    "fighter_188dc@mutant": {"byte"},
+    "fighter_188dc@side": {"byte", "call #0", "call #0 memory"},
+    "fighter_188dc@arg": {"call #0"},
+    "fighter_188dc@eax": {"byte", "call #0"},
+    "fighter_18af8@mutant": {"byte", "call #1", "call #1 memory"},
+    "fighter_18af8@once": {"byte", "call #1"},
+    "fighter_18af8@le": {"byte", "call #1 memory"},
+    "fighter_2a17c@mutant": {"call #0", "call #1"},
+    "fighter_2a17c@order": {"call #0", "call #1", "call #1 memory"},
+    "fighter_2a17c@arg": {"call #0"},
+    "fighter_2a17c@early": {"call #0 memory", "call #1 memory"},
+    "fighter_2bc30@mutant": {"call #0", "call #1"},
+    "fighter_2bc30@order": {"call #0 memory", "call #1 memory"},
+    "fighter_2bc30@frame": {"byte", "call #0 memory", "call #1 memory", "call #2 memory"},
+    "fighter_39fb0@mutant": {"call #1"},
+    "fighter_39fb0@side": {"call #0"},
+    "fighter_39fb0@order": {"call #0", "call #1"},
+    "fighter_3a95c@mutant": {"call #0"},
+    "fighter_3a95c@side": {"call #0"},
+    "fighter_3a95c@arg": {"call #1"},
+    "fighter_35838@mutant": {"call #0"},
+    "fighter_35838@side": {"call #0"},
+    "fighter_35838@order": {"call #0 memory"},
+    "fighter_468d8@mutant": {"eax"},
+    "fighter_468d8@eq": {"eax"},
+    "fighter_468d8@side": {"eax"},
+    "fighter_3c208@mutant": {"call #11", "call #8", "call #9"},
+    "fighter_3c208@abs": {"call #10", "call #11", "call #6", "call #7", "call #8", "call #9"},
+    "fighter_3c208@arg": {"call #8"},
+    "fighter_3c208@early": {"call #10 memory", "call #11 memory", "call #5 memory", "call #6 memory", "call #7 memory", "call #8 memory", "call #9 memory"},
+    "fighter_18c14@mutant": {"eax"},
+    "fighter_18c14@store": {"byte"},
+    "fighter_18c14@live": {"byte", "call #0", "call #1", "eax"},
+}
+
 
 @needs_unicorn
 @unittest.skipUnless((os.path.exists(DIFFRUN) and os.path.exists(EXE)) or REQUIRED,
@@ -428,11 +499,12 @@ class RealFunctionTests(unittest.TestCase):
                                              "fighter_37dcc", "fighter_45878", "fighter_ctx_same",
                                              "fighter_slot_flag", "hit_anim_ctx", "hit_anim_start_b",
                                              "host_1b890", "rng_next"] + list(P1_MASKS) + list(P2_MASKS)
-                                            + list(P3_MASKS)))
+                                            + list(P3_MASKS) + list(C1_MASKS)))
         for name, r in self.real.items():
             if name == "host_1b890":       # the named gap (record E3 §E3.8), tested on its own below
                 continue
-            self.assertEqual((r.verdict, r.problems, r.unhit, r.hit), ("VERIFIED", [], [], r.total), name)
+            self.assertEqual((r.verdict, r.problems, r.unhit), ("VERIFIED", [], []), name)
+            self.assertLessEqual(r.hit, r.total, name)
             self.assertEqual(r.outside, [], name)
 
     def test_every_mutant_is_reported_as_a_mismatch(self):
@@ -442,7 +514,7 @@ class RealFunctionTests(unittest.TestCase):
             "fighter_23130@novoice", "fighter_23130@reorder", "fighter_23130@voice", "fighter_3640c@mutant", "fighter_37dcc@mutant",
             "fighter_45878@mutant", "fighter_ctx_same@mutant", "fighter_slot_flag@mutant",
             "hit_anim_ctx@mutant", "hit_anim_start_b@mutant", "hit_anim_start_b@set", "rng_next@mutant"]
-            + list(P1_KINDS) + list(P2_KINDS) + list(P3_KINDS)))
+            + list(P1_KINDS) + list(P2_KINDS) + list(P3_KINDS) + list(C1_KINDS)))
         for name, r in self.mut.items():
             self.assertEqual(r.verdict, "MISMATCH", name)
 
@@ -507,7 +579,7 @@ class RealFunctionTests(unittest.TestCase):
             "fighter_3640c": 0, "fighter_37dcc": 0,
             "fighter_23130": 0xFF, "fighter_45878": 0, "anim_10fa8": 0, "anim_3e4e4": 0,
             "fighter_ctx_same": 0, "hit_anim_ctx": 0, "hit_anim_start_b": 0, "host_1b890": 0xFFFFFFFF,
-            **P1_MASKS, **P2_MASKS, **P3_MASKS})
+            **P1_MASKS, **P2_MASKS, **P3_MASKS, **C1_MASKS})
         # with the full mask the slot-flag original's scratch bits (case f9: EAX = 0x201) differ
         spec = dataclasses.replace([s for s in V.SPECS if s.name == "fighter_slot_flag"][0],
                                    eax_mask=0xFFFFFFFF)
@@ -673,6 +745,79 @@ class RealFunctionTests(unittest.TestCase):
                           ("fighter_4844c@width", ["a9", "aA", "aB", "aO", "aP"])):
             self.assertEqual(sorted({p.split(":")[0] for p in self.mut[name].problems}), ids, name)
 
+    def test_each_c1_mutant_is_caught_by_what_it_breaks(self):
+        # track P batch C1 (record 2026-10-03-reverse-c1): what alone catches each mutant; every row
+        # with a callee has one that only the call list or the memory at a call catches
+        for name, want in C1_KINDS.items():
+            got = {p.split(": ", 1)[1].split(":")[0] if p.split(": ", 1)[1].startswith("call #")
+                   else p.split(": ", 1)[1].split(" ")[0] for p in self.mut[name].problems}
+            self.assertEqual(got, want, name)
+        # the exact case set that alone catches each mutant (measured on the prototype)
+        for name, ids in (
+        ("fighter_3c148@mutant", ['c0', 'c1']),
+        ("fighter_3c148@side", ['c1']),
+        ("fighter_3c148@width", ['c0', 'c1']),
+        ("fighter_3c16c@mutant", ['c0', 'c1']),
+        ("fighter_3c16c@side", ['c1']),
+        ("fighter_3c16c@width", ['c0', 'c1']),
+        ("fighter_39a10@mutant", ['t0', 't1']),
+        ("fighter_39a10@side", ['t1']),
+        ("fighter_36d98@mutant", ['r0', 'r1']),
+        ("fighter_36d98@side", ['r1']),
+        ("fighter_36d98@and", ['r0', 'r1']),
+        ("fighter_18bd4@mutant", ['b0', 'b1']),
+        ("fighter_18bd4@val", ['b0', 'b1']),
+        ("fighter_18bd4@off", ['b0', 'b1']),
+        ("fighter_34d8c@mutant", ['f0']),
+        ("fighter_34d8c@side", ['f2']),
+        ("fighter_3c358@mutant", ['s0', 's1']),
+        ("fighter_3c358@side", ['s1']),
+        ("fighter_3c358@42", ['s0', 's1']),
+        ("fighter_3c190@mutant", ['v0', 'v1', 'v2']),
+        ("fighter_3c190@arg", ['v0', 'v1', 'v2']),
+        ("fighter_3c190@width", ['v0', 'v1', 'v2']),
+        ("fighter_3c480@mutant", ['a0', 'a1']),
+        ("fighter_3c480@order", ['a0', 'a1']),
+        ("fighter_3c480@side", ['a1']),
+        ("fighter_188ac@mutant", ['a0', 'a1']),
+        ("fighter_188ac@side", ['a1']),
+        ("fighter_188ac@latch", ['a0', 'a1']),
+        ("fighter_188dc@mutant", ['d0', 'd1']),
+        ("fighter_188dc@side", ['d1']),
+        ("fighter_188dc@arg", ['d0', 'd1']),
+        ("fighter_188dc@eax", ['d0', 'd1']),
+        ("fighter_18af8@mutant", ['f0', 'f1']),
+        ("fighter_18af8@once", ['f0', 'f1']),
+        ("fighter_18af8@le", ['f1']),
+        ("fighter_2a17c@mutant", ['p2']),
+        ("fighter_2a17c@order", ['p2']),
+        ("fighter_2a17c@arg", ['p2']),
+        ("fighter_2a17c@early", ['p1', 'p2']),
+        ("fighter_2bc30@mutant", ['n1', 'n2', 'n3']),
+        ("fighter_2bc30@order", ['n1', 'n2', 'n3']),
+        ("fighter_2bc30@frame", ['n0', 'n1', 'n2', 'n3']),
+        ("fighter_39fb0@mutant", ['g0', 'g1']),
+        ("fighter_39fb0@side", ['g1']),
+        ("fighter_39fb0@order", ['g0', 'g1']),
+        ("fighter_3a95c@mutant", ['c0', 'c1']),
+        ("fighter_3a95c@side", ['c0']),
+        ("fighter_3a95c@arg", ['c0', 'c1']),
+        ("fighter_35838@mutant", ['s0', 's1', 's3', 's5']),
+        ("fighter_35838@side", ['s0', 's1', 's3', 's5']),
+        ("fighter_35838@order", ['s0', 's1', 's2', 's3', 's4', 's5']),
+        ("fighter_468d8@mutant", ['h2']),
+        ("fighter_468d8@eq", ['h0', 'h2', 'h3', 'h6']),
+        ("fighter_468d8@side", ['h0', 'h1', 'h2', 'h4', 'h6']),
+        ("fighter_3c208@mutant", ['c3', 'c4', 'c5', 'c6', 'c7']),
+        ("fighter_3c208@abs", ['c0', 'c1', 'c2', 'c3', 'c4', 'c5', 'c6', 'c7']),
+        ("fighter_3c208@arg", ['c0', 'c1', 'c2']),
+        ("fighter_3c208@early", ['c0', 'c1', 'c2', 'c3', 'c4', 'c5', 'c6', 'c7']),
+        ("fighter_18c14@mutant", ['g10n']),
+        ("fighter_18c14@store", ['g00h', 'g01h', 'g10h', 'g11h', 'g20h', 'g21h', 'g30h', 'g31h', 'g40h', 'g41h', 'g50h', 'g51h', 'g60h', 'g61h', 'g70h', 'g71h', 'g80h', 'g81h', 'g90h', 'g91h', 'gA0h', 'gA1h', 'gB0h', 'gB1h', 'gC0h', 'gC1h', 'gD0h', 'gD1h', 'gE0h', 'gE1h', 'gE1n', 'gF0h', 'gF1h']),
+        ("fighter_18c14@live", ['g00h', 'g01h', 'g10h', 'g11h', 'g20h', 'g21h', 'g30h', 'g31h', 'g40h', 'g41h', 'g50h', 'g50n', 'g51h', 'g51n', 'g60h', 'g61h', 'g70h', 'g71h', 'g80h', 'g81h', 'g90h', 'g90n', 'g91h', 'g91n', 'gA0h', 'gA0n', 'gA1h', 'gA1n', 'gB0h', 'gB1h', 'gC0h', 'gC1h', 'gD0h', 'gD0n', 'gD1h', 'gD1n', 'gE0h', 'gE0n', 'gE1h', 'gE1n', 'gF0h', 'gF1h']),
+        ):
+            self.assertEqual(sorted({p.split(":")[0] for p in self.mut[name].problems}), ids, name)
+
     def test_each_stub_declares_the_registers_its_callee_clobbers(self):
         # Call.clobbers, re-derived from the bytes (record §E3.5's table, §E3.12)
         img = E.Image.load(os.path.join(self.tmp.name, "image.bin"))
@@ -685,7 +830,12 @@ class RealFunctionTests(unittest.TestCase):
                                  0x22404: (), 0x36870: ("esi", "edi", "ebp"), 0x35838: ("ebx", "edx"),
                                  0x3B298: ("edx", "edi", "ebp"), 0x39FB0: (), 0x3A95C: ("edx",),
                                  0x3C190: ("edx",), 0x3B714: ("edx",), 0x48170: (), 0x3C148: (), 0x468D8: (),
-                                 0x36D98: (), 0x188DC: ("edx",), 0x3C16C: ()})
+                                 0x36D98: (), 0x188DC: ("edx",), 0x3C16C: (),
+                                 0x186D0: (), 0x18714: (), 0x187FC: (), 0x1883C: ("ebx", "edx"),
+                                 0x189FC: (), 0x18A4C: (), 0x18B04: (), 0x18B44: (),
+                                 0x1DDF4: ("ebx", "edx"), 0x2A408: ("edx",), 0x2B2A0: ("ebx", "edx"),
+                                 0x33754: (), 0x33864: (), 0x36638: ("edx",), 0x39EFC: (),
+                                 0x39F40: ("ebx", "edx"), 0x3B8D8: ("edx",), 0x3B90C: ("edx",)})
         for addr, declared in stubs.items():
             self.assertEqual(E.callee_clobbers(img, addr), declared, hex(addr))
 
@@ -780,8 +930,8 @@ class RealFunctionTests(unittest.TestCase):
                          "--self-check"])
         self.assertEqual(rc, 0)
         # the closed-row count is over the rows that have callees (64), the 14 without are counted apart
-        self.assertIn("diff-verify: 78/78 functions VERIFIED; 158/158 mutants detected; 1 named gaps; "
-                      "11/64 rows with callees closed (14 have none).", out.getvalue())
+        self.assertIn("diff-verify: 98/98 functions VERIFIED; 219/219 mutants detected; 1 named gaps; "
+                      "34/78 rows with callees closed (20 have none).", out.getvalue())
 
 
 # ---- E3: the call list, named gaps, the callee column (record 2026-10-01-reverse-e3 §E3.4, §E3.8) --
PATCH
git status --short
```

Expected: the eight files of the File Structure table modified, no conflict.

- [ ] **Step 2: build and run the row gates.**

```bash
cmake --build build 2>&1 | tail -1
python3 tools/diff_verify.py --diffrun build/diffrun --exe data/game/C/PRAGE.EXE --image /tmp/pr_c1_img.bin --table /tmp/pr_c1_table.md --self-check 2>&1 | tail -1
```

Expected (measured):

```
diff-verify: 98/98 functions VERIFIED; 219/219 mutants detected; 1 named gaps; 34/78 rows with callees closed (20 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
```

- [ ] **Step 3: the row table.** `grep -E '^\| fighter' /tmp/pr_c1_table.md` must show the 20 rows of record §C1.2 with the measured cases/blocks/verdicts (e.g. `fighter_18c14 | 0x18C14 | 64 | 97/97 | VERIFIED`).

- [ ] **Step 4: the unit and Python suites.**

```bash
PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -1      # all checks passed
python3 -m unittest tools.tests.test_diff_verify 2>&1 | tail -3   # Ran 98 tests ... OK
make entry-triage E2_IMAGE=/tmp/pr_c1_e2.bin 2>&1 | tail -3       # byte-identical lines of Task 1
```

- [ ] **Step 5: commit.**

```bash
git add port/src/mem.h port/src/game/actors.c port/src/game/actors.h port/src/game/fighter.c port/src/game/fighter.h port/tests/diff_runner.c tools/diff_verify.py tools/tests/test_diff_verify.py
git commit -m "tests: C1 adds the differential rows for the 20 callees P1-P3 stub (record 2026-10-03-reverse-c1)"
```

### Task 3: the review sweep (same commit or one follow-up)

**Files:** none expected; a hole closes in `port/tests/diff_runner.c` / `tools/diff_verify.py` and the test file in the same commit. **Interfaces:** consumes the Task 2 state.

- [ ] **Step 1: the store sweep.** For each of the 20 rows, every non-stack store instruction of the row's scan is the last writer of a byte that differs from the case's start at the end or at a recorded call, in some case. The prototype's rows pass by construction (the sentinels of record §C1.2); a survivor is a hole: seed the neighbour, add the case, re-measure.
- [ ] **Step 2: the clobber re-derivation.** `python3 -m unittest tools.tests.test_diff_verify.RealFunctionTests.test_each_stub_declares_the_registers_its_callee_clobbers` (OK). A new stub without an entry fails it: add the image's `E.callee_clobbers` to the test's dict.
- [ ] **Step 3: the mutant case sets.** `test_each_c1_mutant_is_caught_by_what_it_breaks` pins every mutant's kind set and its exact case set (the measured table of record §C1.3). A case set that moved means a case was added/removed without re-measuring: re-run the harness and update the table.
- [ ] **Step 4: commit (only if a hole was closed).** `tests: C1 review closes <the hole>`.

### Task 4: Closure

**Files:** `docs/PROGRESS.md`, the record, this plan. **Interfaces:** consumes the Task 2/3 state.

- [ ] **Step 1: the final gates.**

```bash
make diff-verify DIFF_IMAGE=/tmp/pr_c1_final.bin DIFF_TABLE=/tmp/pr_c1_final_table.md 2>&1 | tail -1
make entry-triage E2_IMAGE=/tmp/pr_c1_final_e2.bin 2>&1 | tail -3
PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -1
python3 tools/port_progress.py
```

Expected: the Task 2 counter; the Task 1 E2 lines; `all checks passed`; `771 1203 64` / `731 731 100`.

- [ ] **Step 2: reconcile the counters.** The record §C1.4 and the PROGRESS paragraph carry the measured `98/98 ... 219/219 ... 34/78 (20 have none)`; the test file's counter line matches; the E2 table is untouched (`git diff --stat` shows no `docs/superpowers/plans/2026-10-01-reverse-e2-triage.md`).
- [ ] **Step 3: the PROGRESS paragraph.** Append: C1 added 20 of the 26 callee rows (the six C1b rows deferred, record §C1.5), the counter before/after, the four corrections, the named limits.
- [ ] **Step 4: commit.** `docs: C1 record and PROGRESS reconcile the 20 rows and the six C1b deferrals`.

### Task 5: C1b (the six deferred rows) — follow-on

**Files:** none here. **Interfaces:** record §C1.5 lists each row's direct callees needing a seam. This task is not prototyped: it needs a planner pass (the 2C3FC voice sweep, the 2AE14 descriptor cases, the 36870 opcode cases, the 39834/3B298/3B714 state cases) and its own measured plan. The six keep 33 base rows open (record §C1.4-§C1.5); the closure above names them and does not close those rows.
