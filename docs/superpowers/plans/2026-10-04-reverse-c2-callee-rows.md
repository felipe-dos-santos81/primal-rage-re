# C2: the verification-only callee rows (track P, batch C2) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a differential-verification row for each of the 27 ported callees the P/C1 rows stub
(record §C2.1), so the dependent rows close: the ported function the row stubs gets its own `Spec`,
binding, mutants and, where needed, a `PR_SEAM` first statement; every row's cases cover its blocks
with seeded sentinels and neighbours; the self-check counter, the mutant case-set table and the E2
table are reconciled. Verification only: no ported function, no `fn_register`, no E2 move. Nine
further callees are deferred to C2b with their evidence (record §C2.5); the 27 rows also introduce
11 new row-less stubs, the next callee-row batch's scope (record §C2.8).

**Architecture:** Each row is a `Spec` in `tools/diff_verify.py` (`C2_SPECS`), a `b_*` binding and
`m_*` mutants in `port/tests/diff_runner.c` (`k_bindings`), and exact-set assertions in
`tools/tests/test_diff_verify.py` (`C2_MASKS`, `C2_KINDS`, the case-set table, the clobber table,
the counter line). A callee a row stubs gets `PR_SEAM`/`PR_SEAM_RET`/`PR_SEAM_RET0` as its first
statement (E3 §E3.10); a callee proven by its own check runs `mode="real"` (E3 §E3.4); a leaf whose
arguments cannot be compared runs `allow`. The prototype found two raw-over-plan corrections, baked
into the patch (record §C2.2): `anim_read_var` takes the full stream word (the raw passes the word
and `0x29F34` masks it itself), and `hit_geometry` makes the raw's two calls per distance.

**Tech Stack:** C11 (`port/src`, `port/tests`), Python 3.12 (`unittest`), `unicorn` 2.1.4 and
`capstone` 5.0.7 (`tools/requirements-diff.txt`), CMake, `make`.

**Spec:** `docs/superpowers/specs/2026-09-30-reverse-completion-design.md` §4 track P, §5.1-§5.3,
§6 ("each ported function has a verification row; unhit blocks and non-emulable instructions named;
counters and README percentage updated"), §7.

**Derivation record:** `docs/superpowers/plans/2026-10-04-reverse-c2-derivations.md` (§C2.1 the 36
unverified callees from the final table, §C2.2 the 27 rows, their cases, seams and the two
corrections, §C2.3 the mutants and their measured catching sets, §C2.4 the counters, §C2.5 the nine
deferred C2b rows with their seam lists, §C2.6 decisions and limits, §C2.7 what the planner ran,
§C2.8 the 11 new stubs). Recipe: `2026-10-01-reverse-e3-derivations.md` §E3.10; checklist:
`.superpowers/sdd/2026-10-01-plans/p-track-review-checklist.md`.

**What the planner ran (scratch, 2026-10-04, on `reverse-c2` at `main` `e88eb44`; image sha1
`ff3b8cb14e00f1c282de7b7e15dcd7c230766947`):** the baseline (Task 1) and a full prototype of the 27
rows: the patch below applied to the clean worktree, the four stages each measured with
`--function NAME --self-check`, the fix rounds of record §C2.2 (the `anim_read_var` signature, the
`hit_geometry` two-call correction, the `0x18A4C` real `1A570`, the `release_record` list-head
seed, the `0x2A408` pointer fixture), then `make diff-verify` (`176/176 functions VERIFIED;
478/478 mutants detected; 1 named gaps; 61/145 rows with callees closed (31 have none)`),
`make entry-triage` (byte-identical: `targets 240 unported, 255 ported; supplement 131 (7
unported, 0 stale); untrusted 30`; voice `0 / 115 / 19`) and `PR_ORACLE_REQUIRED=1 ./build/run_tests`
(all checks passed). Every expected output below is the prototype's measured output. The prototype
was then reverted (`git checkout -- port tools`); the patch is the exact diff it applied.

**Re-baseline note.** The counters below are `e88eb44`'s. If `main` moves before this plan executes,
Task 1 records the measured base and every later expected counter adds this plan's increments:
functions +27, mutants +87, rows with callees +19, rows without callees +8, closed rows +20. The E2
table must not move (no ported function, no `fn_register`): if a task regenerates it, the line must
be byte-identical.

## Decisions needed from the user

**None.** The scope is the raw's: the 36 unverified callees the final table names (record §C2.1).
The nine deferrals are a session-budget call recorded with their evidence (record §C2.5) — the same
pattern C1 used for its six. The two corrections of record §C2.2 are raw-over-plan, recorded with
their addresses.

## The C2 roadmap

| task | what | commit |
|---|---|---|
| 1 | baseline on `e88eb44` (no commit) | - |
| 2 | the 27 rows, their seams, bindings, mutants and the test exact-set updates | `tools:`/`tests:` |
| 3 | the review sweep (stores, clobbers, mutant case sets) — the prototype's fix rounds | `tests:` |
| 4 | C2b: the nine deferred rows (2B2A0, 33754, 13C70, 2C3FC, 2AE14, 36870, 39834, 3B298, 3B714) — a follow-on plan | - |
| 5 | closure: PROGRESS, the record/PROGRESS counters, the final gates | `docs:` |

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
  adds the C2 case-set test; no other assertion changes.
- AGENTS.md: "`data/` is git-ignored and read-only — never write to it." / "Commit style:
  `<area>: <what changed>`. **Never `git add -A`** — stage named files." No `Co-Authored-By`
  trailer. Never run `make gp-capture`.
- Spec §5.3 / E3 §E3.4: "VERIFIED" is equivalence on the exercised blocks and inputs only, each
  function with its callees stubbed or run as stated.

## Review Focus

1. **Observable stores and neighbours** (checklist 1-2). Every field a row writes carries a sentinel
   that differs from what it writes; every neighbour byte is seeded. Task 3 re-checks with a store
   sweep.
2. **Inputs that separate alternatives** (checklist 3). `rec+0x51` takes 0 and 1 (and `0x80` in
   `0x29C08`'s `c3`); side 0 and 1; own vs other slot seeded differently; per-case stub EAX; signed
   bounds and exact boundaries (`0x3B8D8`/`0x3B90C`'s wall, `0x1DDF4`'s thresholds). The `C2_KINDS`
   case-set table pins the case that alone catches each mutant.
3. **Every call observed** (checklist 5). Each row with a callee has at least one mutant caught only
   by the call list or the memory at a call (record §C2.3).
4. **Seams** (checklist 9). First statement; args = entry registers = C signature order = `E.Call`;
   clobbers re-derived (`test_each_stub_declares_the_registers_its_callee_clobbers` extended); no
   existing row changes (the full self-check at Task 2's gate: 176/176, 478/478).
5. **The two corrections with wide blast radius.** `anim_read_var`'s signature change touches five
   call sites (three functions) and must keep every existing row VERIFIED; `hit_geometry`'s two
   calls are idempotent for the game and must keep its row's blocks at 11/11.
6. **E2 and the counters.** `make entry-triage` byte-identical; `port_progress.py` `771 1203 64` /
   `731 731 100`; the counter line in the test file is the measured `176/176 ... 478/478 ... 61/145
   (31 have none)`.

## Where to run

The worktree `.worktrees/reverse-c2` (branch `reverse-c2`). Every command runs from the worktree
root; `data/` and `.superpowers/` are symlinks; `build/` exists. Before Task 1:

```bash
cd /Users/felipe.dos.santos/code/mine/primal-rage-reverse/.worktrees/reverse-c2
git rev-parse HEAD   # e88eb44
python3 -c "import unicorn, capstone; print(unicorn.__version__, capstone.__version__)"  # 2.1.4 5.0.7
cmake -S port -B build && cmake --build build 2>&1 | tail -1
./build/diffrun --exe data/game/C/PRAGE.EXE --image-out /tmp/pr_c2_img.bin && shasum /tmp/pr_c2_img.bin
ls port/tests/ghidra_data.bin   # the PR_ORACLE_REQUIRED fixture (git-ignored); copy it from the main repo if absent
```

The last-but-one line must print `ff3b8cb14e00f1c282de7b7e15dcd7c230766947`; another hash means the
image differs from the one the record measured: stop. Ledger:
`.superpowers/sdd/2026-10-04-reverse-c2-callee-rows/progress.md`.

**How the code steps are written.** Task 2's patch is the prototype's exact `git diff` (the planner
applied it, ran every gate, and reverted). Apply it with `git apply`; it touches only
`port/src/game/{actors.c,actors.h,fighter.c,fighter.h}`, `port/tests/diff_runner.c`,
`tools/diff_verify.py`, `tools/tests/test_diff_verify.py`. If `main` moved, `git apply --3way` and
resolve by keeping the patch's additions at the same anchors; never merge the E2 table by hand.

## File Structure

| File | Responsibility |
|---|---|
| `port/src/game/fighter.c`, `fighter.h` | the `18540`/`18350`/`18788`/`1881C`/`1A5AC`/`38154` seams, the `hit_geometry` two-call correction, the `static` removals and declarations |
| `port/src/game/actors.c`, `actors.h` | the `249B0`/`249D0`/`2B150`/`29F34` seams, the `anim_read_var` signature, the `release_record` export and declaration |
| `port/tests/diff_runner.c` | the 27 bindings and 87 mutants (`k_bindings`) |
| `tools/diff_verify.py` | `C2_SPECS`, the C2 callee declarations and fixtures |
| `tools/tests/test_diff_verify.py` | `C2_MASKS`, `C2_KINDS`, the case-set table, the clobber table, the counter line |
| `docs/superpowers/plans/2026-10-04-reverse-c2-derivations.md` | the record (this plan's source of values) |
| `docs/PROGRESS.md` | Task 5's paragraph |

---

### Task 1: Baseline (no commit)

**Files:** none. **Interfaces:** consumes the worktree at `e88eb44`; produces the baseline counters
and table.

- [ ] **Step 1: the task gates on the untouched tree.**

```bash
make diff-verify DIFF_IMAGE=/tmp/pr_c2_base.bin DIFF_TABLE=/tmp/pr_c2_base_table.md 2>&1 | tail -3
make entry-triage E2_IMAGE=/tmp/pr_c2_e2.bin 2>&1 | tail -3
python3 tools/port_progress.py
```

Expected (measured at `e88eb44`):

```
diff-verify: 149/149 functions VERIFIED; 391/391 mutants detected; 1 named gaps; 41/126 rows with callees closed (23 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
entry-triage: targets 240 unported, 255 ported; supplement 131 (7 unported, 0 stale); untrusted entries 30
entry-triage: voice sites outside Ghidra 134: 0 in unported code, 115 in ported code, 19 nowhere
771 1203 64
731 731 100 (portable: excludes 81 host-owned/deferred and runtime >= 5D000)
```

- [ ] **Step 2: enumerate the unverified set from the table.** `python3` over
  `/tmp/pr_c2_base_table.md`: every callee marked `unverified` (record §C2.1). Expected: the 36
  addresses of record §C2.1.

### Task 2: the 27 rows (one commit)

**Files:** the patch below. **Interfaces:** produces `C2_SPECS`, the C2 call declarations, the
seams, the `static` removals, the two raw-fidelity corrections and the test exact-set updates;
consumes the E3/P1-P6 fixtures (`E3_REC`, `E3_REC2`, `E3_OUT`, `E3_SLOT`, `DS_SLOTS`, `SLOT_PTRS`,
`P6_PTRS`, `BIT15`, `ANIM_BEGIN`, `VOICE`, `SPAWN`, `PALETTE`, `C1_SPRITE_ID`, ...).

- [ ] **Step 1: apply the patch.**

```bash
git apply - <<'PATCH'
diff --git a/port/src/game/actors.c b/port/src/game/actors.c
index 36375d2..88229d7 100644
--- a/port/src/game/actors.c
+++ b/port/src/game/actors.c
@@ -33,6 +33,7 @@
 /* 0x249B0: insert rec immediately after `at`. */
 static void list_insert_after(u32 at, u32 rec)
 {
+    PR_SEAM(0x249B0u, at, rec);
     u32 next = DSD(at);
     DSD(at) = rec;
     DSD(rec) = next;
@@ -53,6 +54,7 @@ static void list_insert_before(u32 at, u32 rec)
 /* 0x249D0: unlink rec. */
 static void list_unlink(u32 rec)
 {
+    PR_SEAM(0x249D0u, rec);
     u32 prev = DSD(rec + 4);
     DSD(DSD(rec) + 4) = prev;
     DSD(prev) = DSD(rec);
@@ -1279,9 +1281,10 @@ static void set_dead(u32 rec);
  * 0x46..0x4B the same fields on the parent at rec+0x4A; 0x4C..0x51 on the
  * child at rec+0x4B. The disassembly passes the selector in EDX's low byte
  * (0x29F38 `xor dh,dh` / `and dl,0x7f`), not the decompiler's ABI reading. */
-u32 anim_read_var(u32 rec, u8 op)
+u32 anim_read_var(u32 rec, u32 op)
 {
-    u32 o = (u32)(op & 0x7fu);
+    PR_SEAM_RET(0x29F34u, rec, op);
+    u32 o = op & 0x7fu;
     if (o < 0x40u)
         return DSW(DS_00105B4C + ((o + (u32)DSB(rec + 0x51)) & 0x3fu) * 2u);
     switch (o) {
@@ -1383,7 +1386,7 @@ static u32 anim_operand(u32 rec)
     }
     /* 0x2B96A */
     DSW(DS_00105BE8) = (u16)cx;
-    u32 v = anim_read_var(rec, (u8)cx);
+    u32 v = anim_read_var(rec, cx);
     u16 mode2 = DSW(DS_00105BE6);
     if (mode2 == 0x2000u) return v & 0xffffu;
     u32 p2 = DSD(rec + 8) + 2;
@@ -1438,11 +1441,11 @@ u32 anim_next_sprite_id(u32 rec, u32 pset)
                 u32 e = p + 2;
                 DSD(rec + 8) = e;
                 if (((word >> 8) & 0x60u) == 0x40u) {
-                    u32 rv = anim_read_var(rec, (u8)(word & 0x7fu));
+                    u32 rv = anim_read_var(rec, (u32)word);
                     res = (u32)DSW(e) + rv;
                 } else {
                     DSD(rec + 8) = p + 4;
-                    u32 rv = anim_read_var(rec, (u8)(word & 0x7fu));
+                    u32 rv = anim_read_var(rec, (u32)word);
                     u32 tab = DSD(p + 2);
                     res = DSW(tab + (rv & 0xffffu) * 2u);
                 }
@@ -2863,7 +2866,7 @@ static void frame_timer(u32 rec, u32 slot)
     if ((DSW(rec + 0x28) & 0x810u) != 0 || (DSW(rec + 0x2a) & 4u) != 0) {
         if ((DSW(rec + 0x28) >> 8 & 8u) != 0) return;
         if ((DSW(rec + 0x28) & 0x10u) != 0) {
-            if ((u16)anim_read_var(rec, (u8)DSW(DSD(rec + 8))) == 0) return;
+            if ((u16)anim_read_var(rec, (u32)DSW(DSD(rec + 8))) == 0) return;
             DSB(rec + 0x28) &= 0xef;
         }
         if ((DSW(rec + 0x2a) & 4u) != 0 &&
@@ -2984,6 +2987,7 @@ void actor_pset_palette(u32 rec, u32 word, u32 handle)
  * teardown (0x49444). */
 static void set_dead(u32 rec)
 {
+    PR_SEAM(0x2B150u, rec);
     DSB(rec + 0x28) |= 0x08;
     if ((DSW(rec + 0x2a) >> 8 & 0x40u) != 0) {
         /* 0x2B185: cb2 = DS_000BB9E0[type * 0xC], called with EAX = rec; its
@@ -3599,7 +3603,7 @@ void actor_type_49444(u32 rec)
 
 /* 0x2AD40. The release path: drop the child, decrement the parent refcount,
  * return the record to the free list and zero its pset. */
-static void release_record(u32 rec, u32 pset)
+void release_record(u32 rec, u32 pset)
 {
     PR_SEAM(0x2AD40u, rec, pset);
     if (!in_pool(rec)) return;                  /* PORT: spec §7 invariant */
diff --git a/port/src/game/actors.h b/port/src/game/actors.h
index 43ab729..408521e 100644
--- a/port/src/game/actors.h
+++ b/port/src/game/actors.h
@@ -116,7 +116,7 @@ void palette_release(u32 entry);
 /* 0x29F34. Read an animation variable: `op & 0x7F` selects the 0x40-word ring
  * at DS_00105B4C (< 0x40), the record's own bytes (0x40..0x45), the parent
  * rec+0x4A's bytes (0x46..0x4B) or the child rec+0x4B's bytes (0x4C..0x51). */
-u32  anim_read_var(u32 rec, u8 op);
+u32  anim_read_var(u32 rec, u32 op);
 /* 0x29DB8. Write an animation variable (the mirror of anim_read_var). */
 void anim_write_var(u32 rec, u8 op, u32 value);
 /* 0x2BC30. Point an existing record at `stream`, reset its animation cursor and
@@ -126,6 +126,7 @@ void anim_write_var(u32 rec, u8 op, u32 value);
 void actors_anim_begin(u32 rec, u32 stream, u32 frame_bits);
 /* 0x2BCF4. Point a record at `stream` and load its first sprite id. */
 void actors_anim_seek(u32 rec, u32 stream);
+void release_record(u32 rec, u32 pset);
 /* 0x2BD20. Store `v`'s low byte at +0x4B of the pool record `rec`'s +0x4A byte
  * names (the holder's link to the held record); returns 0. */
 u32 actors_link_held(u32 rec, u32 v);
diff --git a/port/src/game/fighter.c b/port/src/game/fighter.c
index 1c6a340..acff2da 100644
--- a/port/src/game/fighter.c
+++ b/port/src/game/fighter.c
@@ -189,8 +189,9 @@ int fighter_18460(u32 side)
  * raw's 0x18460 call is effect-free in this build: its 0x18428 dispatch's
  * 0x1840C table resolves to 0x18408 (0x18350's epilogue), so it writes
  * nothing (the port's fixup-applied mem[] holds the seven 0x18408 entries). */
-static void fighter_18540(u32 side)
+void fighter_18540(u32 side)
 {
+    PR_SEAM(0x18540u, side);
     u32 slot = DSD(DS_001077A8 + side * 4u);            /* 0x18546 */
     if (slot == 0) return;                              /* 0x1854F */
     u32 ch = (u32)DSB(slot + 0x7Au);                    /* 0x18555 */
@@ -217,8 +218,9 @@ static void fighter_18540(u32 side)
  * pair at the character's anchor-indexed table (0xCEB00/0xCF399/0xCFC32/0xD033B/
  * 0xD0A44, table 0x18334), the x negated when the actor's bit 15 is set, both
  * scaled by 64. */
-static void fighter_18350(u32 side, u32 anchor)
+void fighter_18350(u32 side, u32 anchor)
 {
+    PR_SEAM(0x18350u, side, anchor);
     u32 ch = (u32)DSB(DS_001077B0 + side * 0x94u + 0x7Au);   /* 0x1835F */
     u32 a;
     const u8 *p;
@@ -2059,7 +2061,7 @@ static void fighter_state_35b7c(u32 slot, u32 rec);         /* 0x35B7C */
 void fighter_state_35d20(u32 slot, u32 rec);                /* 0x35D20 */
 void fighter_1883c(u32 side, u32 a, u32 b);                  /* 0x1883C */
 static void fighter_36e78(u32 slot);                        /* 0x36E78 */
-static u32  hit_record_y(u32 side);                         /* 0x18788 */
+u32  hit_record_y(u32 side);                                /* 0x18788 */
 
 /* 0x29BC8. Resolve the character's palette handle for `side` and point `rec`'s
  * pset at it (0x2A17C with word 0). */
@@ -3959,8 +3961,9 @@ int hit_reaction_allow(u32 side, u32 reaction)
 }
 
 /* 0x1881C. The two slots latched, then slot0+0x30 - slot1+0x30. */
-static s32 hit_vert_distance(void)
+s32 hit_vert_distance(void)
 {
+    PR_SEAM_RET0(0x1881Cu);
     fighter_slot_latch(0u);                             /* 0x18820 */
     fighter_slot_latch(1u);                             /* 0x18828 */
     return (s32)DSD(DS_001077E0) - (s32)DSD(DS_001077E0 + 0x94u);
@@ -3976,12 +3979,18 @@ int hit_geometry(u32 side, u32 table, u32 idx)
     u32 ch = (u32)DSB(DS_001077B0 + side * 0x94u + 0x7Au);
     u32 thr1 = (u32)DSB(table + ch) << 6;
     u32 thr2 = (u32)DSB(idx + ch) << 6;
+    /* The raw calls each distance twice on every path: once for the sign test
+     * (0x1DE1D/0x1DE3F) and once more for the value (0x1DE29/0x1DE32,
+     * 0x1DE48/0x1DE51). PORT: kept as the raw's two calls (the differential
+     * row compares the call list). */
     s32 d1 = ai_distance();
-    if (d1 < 0) d1 = -d1;
+    if (d1 < 0) d1 = -ai_distance();
+    else d1 = ai_distance();
     if (d1 > (s32)thr1) return 0;
     {
         s32 d2 = hit_vert_distance();
-        if (d2 < 0) d2 = -d2;
+        if (d2 < 0) d2 = -hit_vert_distance();
+        else d2 = hit_vert_distance();
         if (d2 > (s32)thr2) return 0;
     }
     return 1;
@@ -4072,8 +4081,9 @@ u32 hit_record_x(u32 side)
  * DS_00100AB4[side]. Its only caller, 0x1883C, latches both slots first
  * (0x186D0 stores the same anchor in slot+0x20), so the anchor path cannot
  * change anything there; it is transcribed as the raw has it. */
-static u32 hit_record_y(u32 side)
+u32 hit_record_y(u32 side)
 {
+    PR_SEAM_RET(0x18788u, side);
     u32 slot = DS_001077B0 + side * 0x94u;
     if ((DSB(slot + 0x42u) & 0x08u) != 0u)
         return DSD(DSD(slot) + 0x1Cu);                  /* 0x187AC */
@@ -5363,8 +5373,9 @@ static void fighter_ctx_rec_swap(u32 out[6], u32 rec)
 }
 
 /* 0x1A5AC. 1 when the side's record +0x28 has bit 0x4000 clear. */
-static int fighter_1a5ac(u32 side)
+int fighter_1a5ac(u32 side)
 {
+    PR_SEAM_RET(0x1A5ACu, side);
     u32 ctx[6];
     fighter_ctx_same(ctx, side);                            /* 0x1A5B4 */
     return (DSW(ctx[4] + 0x28u) & 0x4000u) == 0u;           /* 0x1A5BD..0x1A5CB */
@@ -10017,6 +10028,7 @@ void fighter_39ff4(void)
  * 0x382FA (0x382C4). */
 void fighter_38154(u32 side)
 {
+    PR_SEAM(0x38154u, side);
     u32 slot = DSD(DS_001077A8 + side * 4u);            /* 0x3815C */
     u32 rec, flag;
     s32 v, p, av;
diff --git a/port/src/game/fighter.h b/port/src/game/fighter.h
index 1857a13..76ec41a 100644
--- a/port/src/game/fighter.h
+++ b/port/src/game/fighter.h
@@ -589,6 +589,11 @@ void fighter_3c208(u32 side, s32 dist);
  * arena wall). EAX = side, EDX = delta; 0x3C208's two calls only. */
 s32 fighter_3b90c(u32 side, s32 delta);
 s32 ai_distance(void);
+void fighter_18540(u32 side);
+void fighter_18350(u32 side, u32 anchor);
+u32 hit_record_y(u32 side);
+s32 hit_vert_distance(void);
+int fighter_1a5ac(u32 side);
 void fighter_18b44(u32 slot);
 int fighter_189fc(u32 side);
 int fighter_18a4c(u32 side);
diff --git a/port/tests/diff_runner.c b/port/tests/diff_runner.c
index b193a63..2eb9ab4 100644
--- a/port/tests/diff_runner.c
+++ b/port/tests/diff_runner.c
@@ -5262,6 +5262,1112 @@ static void m_24220_decr(const u32 *r, u32 *eax)       /* the decrement 0x100 */
     *eax = 0u;
 }
 
+/* Track P batch C2 (record 2026-10-04-reverse-c2): the verification-only callee rows. Each binding
+ * adapts the original's registers to the port function the dependent rows stub; each mutant is a
+ * plausible porting bug of that function alone. */
+static void b_39280(const u32 *r, u32 *eax)            { fighter_state_39280(r[R_EAX]); *eax = 0u; }
+static void m_39280(const u32 *r, u32 *eax)            /* +0x5D takes 1, not 0 */
+{
+    u32 slot = DS_001077B0 + r[R_EAX] * 0x94u;
+    DSB(slot + 0x5Du) = 1u;
+    DSB(slot + 0x43u) &= 0xFBu;
+    *eax = 0u;
+}
+static void m_39280_side(const u32 *r, u32 *eax)       /* always side 0 */
+{
+    (void)r;
+    DSB(DS_001077B0 + 0x5Du) = 0u;
+    DSB(DS_001077B0 + 0x43u) &= 0xFBu;
+    *eax = 0u;
+}
+static void m_39280_width(const u32 *r, u32 *eax)      /* the word +0x5C cleared */
+{
+    u32 slot = DS_001077B0 + r[R_EAX] * 0x94u;
+    DSW(slot + 0x5Cu) = 0u;
+    DSB(slot + 0x43u) &= 0xFBu;
+    *eax = 0u;
+}
+static void b_33864(const u32 *r, u32 *eax)            { palette_release(r[R_EAX]); *eax = 0u; }
+static void m_33864(const u32 *r, u32 *eax)            /* clears at ref-1 == 1 (off by one) */
+{
+    u32 entry = r[R_EAX];
+    u32 ref = DSD(entry + 4u);
+    DSD(entry + 4u) = ref - 1u;
+    if (ref - 1u == 1u) DSD(entry) = 0u;
+    *eax = 0u;
+}
+static void m_33864_width(const u32 *r, u32 *eax)      /* the refcount decremented as a byte */
+{
+    u32 entry = r[R_EAX];
+    u32 ref = DSD(entry + 4u);
+    DSB(entry + 4u) = (u8)(ref - 1u);
+    if ((ref - 1u) == 0u) DSD(entry) = 0u;
+    *eax = 0u;
+}
+static void m_33864_off(const u32 *r, u32 *eax)        /* clears when the pre-decrement ref is 0 */
+{
+    u32 entry = r[R_EAX];
+    u32 ref = DSD(entry + 4u);
+    DSD(entry + 4u) = ref - 1u;
+    if (ref == 0u) DSD(entry) = 0u;
+    *eax = 0u;
+}
+static void b_13244(const u32 *r, u32 *eax)            { (void)r; fighter_13244(); *eax = 0u; }
+static void m_13244(const u32 *r, u32 *eax)            /* writes 0 */
+{
+    (void)r;
+    DSB(DS_001088C2) = 0u;
+    *eax = 0u;
+}
+static void m_13244_addr(const u32 *r, u32 *eax)       /* the next byte */
+{
+    (void)r;
+    DSB(DS_001088C3) = 1u;
+    *eax = 0u;
+}
+static void m_13244_val(const u32 *r, u32 *eax)        /* writes 2 */
+{
+    (void)r;
+    DSB(DS_001088C2) = 2u;
+    *eax = 0u;
+}
+static void m_29c08_side(const u32 *r, u32 *eax)       /* the side-0 index always */
+{
+    *eax = fighter_29c08(0u, r[R_EDX]);
+}
+static void m_29c08_char(const u32 *r, u32 *eax)       /* character 0's row always */
+{
+    *eax = fighter_29c08(r[R_EAX], 0u);
+}
+static void m_29c08_sext(const u32 *r, u32 *eax)       /* the index sign-extended */
+{
+    u32 row = DSD(DS_000A8A98 + r[R_EDX] * 4u);
+    *eax = DSD(row + (u32)((s32)(s8)DSB(DS_00105B34 + r[R_EAX])) * 4u);
+}
+static void b_2a148(const u32 *r, u32 *eax)            { actor_pset_flag_5f(r[R_EAX], (u8)r[R_EDX]); *eax = 0u; }
+static void m_2a148(const u32 *r, u32 *eax)            /* the flag ignored (no 0x800) */
+{
+    u32 rec = r[R_EAX];
+    DSB(rec + 0x5Fu) = (u8)r[R_EDX];
+    DSW(actor_pset(rec) + 0x02u) = DSW(rec + 0x2Eu);
+    *eax = 0u;
+}
+static void m_2a148_word(const u32 *r, u32 *eax)       /* the pset word from +0x2C */
+{
+    u32 rec = r[R_EAX];
+    DSB(rec + 0x5Fu) = (u8)r[R_EDX];
+    DSW(actor_pset(rec) + 0x02u) = (u16)(DSW(rec + 0x2Cu)
+        | (DSB(rec + 0x5Fu) != 0 ? 0x800u : 0u));
+    *eax = 0u;
+}
+static void m_2a148_pset(const u32 *r, u32 *eax)       /* the pset index forced to 0 */
+{
+    u32 rec = r[R_EAX];
+    DSB(rec + 0x5Fu) = (u8)r[R_EDX];
+    DSW(DSD(DS_001014EC) + 0x02u) = (u16)(DSW(rec + 0x2Eu)
+        | (DSB(rec + 0x5Fu) != 0 ? 0x800u : 0u));
+    *eax = 0u;
+}
+static void b_29bc8(const u32 *r, u32 *eax)            { fighter_29bc8(r[R_EAX], r[R_EBX], r[R_EDX]); *eax = 0u; }
+static void m_29bc8_side(const u32 *r, u32 *eax)       /* the side-0 index always */
+{
+    u32 tbl = DSD(DS_000A8A98 + r[R_EDX] * 4u);
+    u32 handle = DSD(tbl + (u32)DSB(DS_00105B34) * 4u);
+    actor_pset_palette(r[R_EBX], 0u, handle);
+    *eax = 0u;
+}
+static void m_29bc8_rec(const u32 *r, u32 *eax)        /* the record from side 0's slot */
+{
+    u32 tbl = DSD(DS_000A8A98 + r[R_EDX] * 4u);
+    u32 handle = DSD(tbl + (u32)DSB(DS_00105B34 + r[R_EAX]) * 4u);
+    actor_pset_palette(DSD(DS_001077B0), 0u, handle);
+    *eax = 0u;
+}
+static void b_1890c(const u32 *r, u32 *eax)            { hit_anchor_y(r[R_EAX], r[R_EDX]); *eax = 0u; }
+static void m_1890c(const u32 *r, u32 *eax)            /* adds y alone, not (y - slot+0x30) */
+{
+    u32 slot = DS_001077B0 + r[R_EAX] * 0x94u;
+    u32 rec;
+    fighter_slot_latch(r[R_EAX]);
+    rec = DSD(slot);
+    DSD(rec + 0x1Cu) = DSD(rec + 0x1Cu) + r[R_EDX];
+    fighter_slot_latch(r[R_EAX]);
+    *eax = 0u;
+}
+static void m_1890c_side(const u32 *r, u32 *eax)       /* always side 0 */
+{
+    u32 slot = DS_001077B0;
+    u32 rec;
+    fighter_slot_latch(0u);
+    rec = DSD(slot);
+    DSD(rec + 0x1Cu) = (u32)((s32)DSD(rec + 0x1Cu) + ((s32)r[R_EDX] - (s32)DSD(slot + 0x30u)));
+    fighter_slot_latch(0u);
+    *eax = 0u;
+}
+static void m_1890c_field(const u32 *r, u32 *eax)      /* the record's +0x18 */
+{
+    u32 slot = DS_001077B0 + r[R_EAX] * 0x94u;
+    u32 rec;
+    fighter_slot_latch(r[R_EAX]);
+    rec = DSD(slot);
+    DSD(rec + 0x18u) = (u32)((s32)DSD(rec + 0x18u) + ((s32)r[R_EDX] - (s32)DSD(slot + 0x30u)));
+    fighter_slot_latch(r[R_EAX]);
+    *eax = 0u;
+}
+static void b_187fc(const u32 *r, u32 *eax)            { (void)r; *eax = (u32)ai_distance(); }
+static void m_187fc(const u32 *r, u32 *eax)            /* slot1 - slot0 */
+{
+    (void)r;
+    fighter_slot_latch(0u);
+    fighter_slot_latch(1u);
+    *eax = (u32)((s32)DSD(0x00107870u) - (s32)DSD(0x001077DCu));
+}
+static void m_187fc_order(const u32 *r, u32 *eax)      /* the latches swapped */
+{
+    (void)r;
+    fighter_slot_latch(1u);
+    fighter_slot_latch(0u);
+    *eax = (u32)((s32)DSD(0x001077DCu) - (s32)DSD(0x00107870u));
+}
+static void b_186d0(const u32 *r, u32 *eax)            { fighter_slot_latch(r[R_EAX]); *eax = 0u; }
+static void m_186d0(const u32 *r, u32 *eax)            /* the bit-3 arms swapped */
+{
+    u32 side = r[R_EAX], slot = DS_001077B0 + side * 0x94u, rec = DSD(slot);
+    if ((DSB(slot + 0x42u) & 0x08u) != 0u) {
+        fighter_18540(side);
+        u32 anchor = DSD(0x00100AF0u + side * 4u);
+        if (anchor != DSD(slot + 0x20u)) {
+            DSD(slot + 0x20u) = anchor;
+            fighter_18350(side, anchor);
+        }
+        DSD(slot + 0x2Cu) = DSD(rec + 0x18u) + DSD(0x00100AB0u + side * 8u);
+        DSD(slot + 0x30u) = DSD(rec + 0x1Cu) + DSD(0x00100AB4u + side * 8u);
+    } else {
+        DSD(slot + 0x2Cu) = DSD(rec + 0x18u);
+        DSD(slot + 0x30u) = DSD(rec + 0x1Cu);
+    }
+    if ((DSB(slot + 0x41u) & 0x80u) != 0) DSD(slot + 0x34u) = DSD(slot + 0x2Cu);
+    *eax = 0u;
+}
+static void m_186d0_side(const u32 *r, u32 *eax)       /* always side 0 */
+{
+    (void)r;
+    fighter_slot_latch(0u);
+    *eax = 0u;
+}
+static void m_186d0_anchor(const u32 *r, u32 *eax)     /* the anchor compared at +0x24 */
+{
+    u32 side = r[R_EAX], slot = DS_001077B0 + side * 0x94u, rec = DSD(slot);
+    if ((DSB(slot + 0x42u) & 0x08u) == 0u) {
+        fighter_18540(side);
+        u32 anchor = DSD(0x00100AF0u + side * 4u);
+        if (anchor != DSD(slot + 0x24u)) {
+            DSD(slot + 0x20u) = anchor;
+            fighter_18350(side, anchor);
+        }
+        DSD(slot + 0x2Cu) = DSD(rec + 0x18u) + DSD(0x00100AB0u + side * 8u);
+        DSD(slot + 0x30u) = DSD(rec + 0x1Cu) + DSD(0x00100AB4u + side * 8u);
+    } else {
+        DSD(slot + 0x2Cu) = DSD(rec + 0x18u);
+        DSD(slot + 0x30u) = DSD(rec + 0x1Cu);
+    }
+    if ((DSB(slot + 0x41u) & 0x80u) != 0) DSD(slot + 0x34u) = DSD(slot + 0x2Cu);
+    *eax = 0u;
+}
+static void b_18714(const u32 *r, u32 *eax)            { *eax = hit_record_x(r[R_EAX]); }
+static void m_18714(const u32 *r, u32 *eax)            /* the bit-3 arm returns +0x1C */
+{
+    u32 side = r[R_EAX], slot = DS_001077B0 + side * 0x94u, rec = DSD(slot);
+    if ((DSB(slot + 0x42u) & 0x08u) != 0u) { *eax = DSD(rec + 0x1Cu); return; }
+    fighter_18540(side);
+    u32 anchor = DSD(0x00100AF0u + side * 4u);
+    if (anchor != DSD(slot + 0x20u)) {
+        DSD(slot + 0x20u) = anchor;
+        fighter_18350(side, anchor);
+    }
+    *eax = DSD(slot + 0x2Cu) - DSD(0x00100AB0u + side * 8u);
+}
+static void m_18714_side(const u32 *r, u32 *eax)       /* always side 0 */
+{
+    (void)r;
+    *eax = hit_record_x(0u);
+}
+static void m_18714_anchor(const u32 *r, u32 *eax)     /* the anchor compared at +0x24 */
+{
+    u32 side = r[R_EAX], slot = DS_001077B0 + side * 0x94u, rec = DSD(slot);
+    if ((DSB(slot + 0x42u) & 0x08u) != 0u) { *eax = DSD(rec + 0x18u); return; }
+    fighter_18540(side);
+    u32 anchor = DSD(0x00100AF0u + side * 4u);
+    if (anchor != DSD(slot + 0x24u)) {
+        DSD(slot + 0x20u) = anchor;
+        fighter_18350(side, anchor);
+    }
+    *eax = DSD(slot + 0x2Cu) - DSD(0x00100AB0u + side * 8u);
+}
+static void b_189fc(const u32 *r, u32 *eax)            { *eax = (u32)fighter_189fc(r[R_EAX]); }
+static void m_189fc(const u32 *r, u32 *eax)            /* the first comparison's direction */
+{
+    u32 side = r[R_EAX], ctx[6];
+    fighter_ctx_swap(ctx, side);
+    if (fighter_actor_bit15_clear(ctx[0])) *eax = (u32)((s32)DSD(ctx[3] + 0x2Cu) > (s32)DSD(ctx[2] + 0x2Cu));
+    else *eax = (u32)((s32)DSD(ctx[3] + 0x2Cu) > (s32)DSD(ctx[2] + 0x2Cu));
+}
+static void m_189fc_side(const u32 *r, u32 *eax)       /* always side 0 */
+{
+    (void)r;
+    *eax = (u32)fighter_189fc(0u);
+}
+static void m_189fc_bit(const u32 *r, u32 *eax)        /* the bit-15 test ignored */
+{
+    u32 side = r[R_EAX], ctx[6];
+    fighter_ctx_swap(ctx, side);
+    *eax = (u32)((s32)DSD(ctx[3] + 0x2Cu) < (s32)DSD(ctx[2] + 0x2Cu));
+}
+static void m_189fc_al(const u32 *r, u32 *eax)         /* the predicate compared to 1, not tested for zero */
+{
+    u32 side = r[R_EAX], ctx[6];
+    fighter_ctx_swap(ctx, side);
+    if (fighter_actor_bit15_clear(ctx[0]) == 1)
+        *eax = (u32)((s32)DSD(ctx[3] + 0x2Cu) < (s32)DSD(ctx[2] + 0x2Cu));
+    else
+        *eax = (u32)((s32)DSD(ctx[3] + 0x2Cu) > (s32)DSD(ctx[2] + 0x2Cu));
+}
+static void b_18a4c(const u32 *r, u32 *eax)            { *eax = (u32)fighter_18a4c(r[R_EAX]); }
+static void m_18a4c(const u32 *r, u32 *eax)            /* the bit-15 arm inverted */
+{
+    u32 side = r[R_EAX], ctx[6];
+    fighter_ctx_same(ctx, side);
+    if (!fighter_actor_bit15_clear(ctx[0])) {
+        if (!fighter_189fc(ctx[1])) return (void)(*eax = 0u);
+        if (fighter_actor_bit15_clear(ctx[1])) return (void)(*eax = 0u);
+        *eax = 1u;
+        return;
+    }
+    if (!fighter_189fc(ctx[1])) return (void)(*eax = 0u);
+    if (!fighter_actor_bit15_clear(ctx[1])) return (void)(*eax = 0u);
+    *eax = 1u;
+}
+static void m_18a4c_side(const u32 *r, u32 *eax)       /* always side 0 */
+{
+    (void)r;
+    *eax = (u32)fighter_18a4c(0u);
+}
+static void m_18a4c_arg(const u32 *r, u32 *eax)        /* 0x189FC on ctx[0] */
+{
+    u32 side = r[R_EAX], ctx[6];
+    fighter_ctx_same(ctx, side);
+    if (fighter_actor_bit15_clear(ctx[0])) {
+        if (!fighter_189fc(ctx[0])) return (void)(*eax = 0u);
+        if (fighter_actor_bit15_clear(ctx[1])) return (void)(*eax = 0u);
+        *eax = 1u;
+        return;
+    }
+    if (!fighter_189fc(ctx[0])) return (void)(*eax = 0u);
+    if (!fighter_actor_bit15_clear(ctx[1])) return (void)(*eax = 0u);
+    *eax = 1u;
+}
+static void b_39efc(const u32 *r, u32 *eax)            { *eax = (u32)fighter_39efc(r[R_EAX]); }
+static void m_39efc(const u32 *r, u32 *eax)            /* +0x52 instead of +0x53 */
+{
+    u32 ctx[6];
+    fighter_ctx_swap(ctx, r[R_EAX]);
+    u32 slot = ctx[3];
+    *eax = (u32)(DSB(slot + 0x52u) == 0x0Au && DSD(slot + 0x10u) == 0x00039CC8u
+                 && DSB(slot + 0x58u) == 4u);
+}
+static void m_39efc_val(const u32 *r, u32 *eax)        /* the handler 0x39CC0 */
+{
+    u32 ctx[6];
+    fighter_ctx_swap(ctx, r[R_EAX]);
+    u32 slot = ctx[3];
+    *eax = (u32)(DSB(slot + 0x53u) == 0x0Au && DSD(slot + 0x10u) == 0x00039CC0u
+                 && DSB(slot + 0x58u) == 4u);
+}
+static void m_39efc_side(const u32 *r, u32 *eax)       /* always side 0 */
+{
+    (void)r;
+    *eax = (u32)fighter_39efc(0u);
+}
+static void b_3b8d8(const u32 *r, u32 *eax)            { *eax = (u32)fighter_3b8d8(r[R_EAX], (s32)r[R_EDX]); }
+static void m_3b8d8(const u32 *r, u32 *eax)            /* the wall test only (no -wall) */
+{
+    s32 x = (s32)DSD(DS_001077B0 + r[R_EAX] * 0x94u + 0x2Cu) + (s32)r[R_EDX];
+    s32 wall = (s32)DSD(DS_000BE018);
+    *eax = (u32)(x >= wall);
+}
+static void m_3b8d8_side(const u32 *r, u32 *eax)       /* always side 0 */
+{
+    *eax = (u32)fighter_3b8d8(0u, (s32)r[R_EDX]);
+}
+static void m_3b8d8_bound(const u32 *r, u32 *eax)      /* strict at the wall */
+{
+    s32 x = (s32)DSD(DS_001077B0 + r[R_EAX] * 0x94u + 0x2Cu) + (s32)r[R_EDX];
+    s32 wall = (s32)DSD(DS_000BE018);
+    *eax = (u32)(x > wall || x < -wall);
+}
+static void b_3b90c(const u32 *r, u32 *eax)            { *eax = (u32)fighter_3b90c(r[R_EAX], (s32)r[R_EDX]); }
+static void m_3b90c(const u32 *r, u32 *eax)            /* the wall sign flipped */
+{
+    s32 x = (s32)(DSD(DS_001077B0 + r[R_EAX] * 0x94u + 0x2Cu) + (u32)r[R_EDX]);
+    s32 wall = (s32)DSD(DS_000BE018);
+    if (x >= wall) *eax = (u32)(0 - wall);
+    else if (x > (s32)(0u - (u32)wall)) *eax = (u32)x;
+    else *eax = (u32)(0 - wall);
+}
+static void m_3b90c_side(const u32 *r, u32 *eax)       /* always side 0 */
+{
+    *eax = (u32)fighter_3b90c(0u, (s32)r[R_EDX]);
+}
+static void m_3b90c_bound(const u32 *r, u32 *eax)      /* strict at the wall */
+{
+    s32 x = (s32)(DSD(DS_001077B0 + r[R_EAX] * 0x94u + 0x2Cu) + (u32)r[R_EDX]);
+    s32 wall = (s32)DSD(DS_000BE018);
+    if (x > wall) *eax = (u32)wall;
+    else if (x > (s32)(0u - (u32)wall)) *eax = (u32)x;
+    else *eax = (u32)wall;
+}
+static void b_33a10(const u32 *r, u32 *eax)
+{
+    u32 out[6];
+    fighter_ctx_swap(out, r[R_EDX]);
+    memcpy(mem + r[R_EAX], out, sizeof out);
+    *eax = 0u;
+}
+static void m_33a10(const u32 *r, u32 *eax)            /* out[0] = side */
+{
+    u32 out[6];
+    fighter_ctx_swap(out, r[R_EDX]);
+    out[0] = r[R_EDX];
+    memcpy(mem + r[R_EAX], out, sizeof out);
+    *eax = 0u;
+}
+static void m_33a10_slot(const u32 *r, u32 *eax)       /* out[2]/out[3] swapped */
+{
+    u32 out[6];
+    fighter_ctx_swap(out, r[R_EDX]);
+    u32 t = out[2];
+    out[2] = out[3];
+    out[3] = t;
+    memcpy(mem + r[R_EAX], out, sizeof out);
+    *eax = 0u;
+}
+static void m_33a10_rec(const u32 *r, u32 *eax)        /* out[4]/out[5] swapped */
+{
+    u32 out[6];
+    fighter_ctx_swap(out, r[R_EDX]);
+    u32 t = out[4];
+    out[4] = out[5];
+    out[5] = t;
+    memcpy(mem + r[R_EAX], out, sizeof out);
+    *eax = 0u;
+}
+static void b_1883c(const u32 *r, u32 *eax)            { fighter_1883c(r[R_EAX], r[R_EDX], r[R_EBX]); *eax = 0u; }
+static void m_1883c(const u32 *r, u32 *eax)            /* a/b swapped into +0x2C/+0x30 */
+{
+    u32 side = r[R_EAX], a = r[R_EDX], b = r[R_EBX];
+    u32 slot = DS_001077B0 + side * 0x94u;
+    fighter_slot_latch(0u);
+    fighter_slot_latch(1u);
+    DSD(slot + 0x2Cu) += b;
+    DSD(slot + 0x30u) += a;
+    DSD(DSD(slot) + 0x18u) = hit_record_x(side);
+    DSD(DSD(slot) + 0x1Cu) = hit_record_y(side);
+    *eax = 0u;
+}
+static void m_1883c_side(const u32 *r, u32 *eax)       /* always side 0 */
+{
+    (void)r;
+    fighter_1883c(0u, r[R_EDX], r[R_EBX]);
+    *eax = 0u;
+}
+static void m_1883c_latch(const u32 *r, u32 *eax)      /* the second latch skipped */
+{
+    u32 side = r[R_EAX], a = r[R_EDX], b = r[R_EBX];
+    u32 slot = DS_001077B0 + side * 0x94u;
+    fighter_slot_latch(0u);
+    DSD(slot + 0x2Cu) += a;
+    DSD(slot + 0x30u) += b;
+    DSD(DSD(slot) + 0x18u) = hit_record_x(side);
+    DSD(DSD(slot) + 0x1Cu) = hit_record_y(side);
+    *eax = 0u;
+}
+static void m_1883c_arg(const u32 *r, u32 *eax)        /* 0x18714 on the other side */
+{
+    u32 side = r[R_EAX], a = r[R_EDX], b = r[R_EBX];
+    u32 slot = DS_001077B0 + side * 0x94u;
+    fighter_slot_latch(0u);
+    fighter_slot_latch(1u);
+    DSD(slot + 0x2Cu) += a;
+    DSD(slot + 0x30u) += b;
+    DSD(DSD(slot) + 0x18u) = hit_record_x(1u - side);
+    DSD(DSD(slot) + 0x1Cu) = hit_record_y(side);
+    *eax = 0u;
+}
+static void b_18b04(const u32 *r, u32 *eax)            { hit_facing_flag(r[R_EAX]); *eax = 0u; }
+static void m_18b04(const u32 *r, u32 *eax)            /* `<=` for the `<` */
+{
+    u32 side = r[R_EAX], ctx[6];
+    if (DSW(DS_00104B00) == 0x22u) { *eax = 0u; return; }
+    fighter_ctx_same(ctx, side);
+    if ((s32)DSD(ctx[2] + 0x2Cu) <= (s32)DSD(ctx[3] + 0x2Cu)) DSB(ctx[4] + 0x29u) |= 0x40u;
+    else DSB(ctx[4] + 0x29u) &= 0xBFu;
+    DSD(ctx[4] + 0x18u) = hit_record_x(side);
+    *eax = 0u;
+}
+static void m_18b04_mode(const u32 *r, u32 *eax)       /* the mode early-out dropped */
+{
+    u32 side = r[R_EAX], ctx[6];
+    fighter_ctx_same(ctx, side);
+    if ((s32)DSD(ctx[2] + 0x2Cu) < (s32)DSD(ctx[3] + 0x2Cu)) DSB(ctx[4] + 0x29u) |= 0x40u;
+    else DSB(ctx[4] + 0x29u) &= 0xBFu;
+    DSD(ctx[4] + 0x18u) = hit_record_x(side);
+    *eax = 0u;
+}
+static void m_18b04_side(const u32 *r, u32 *eax)       /* always side 0 */
+{
+    (void)r;
+    hit_facing_flag(0u);
+    *eax = 0u;
+}
+static void m_18b04_store(const u32 *r, u32 *eax)      /* the flag byte at +0x28 */
+{
+    u32 side = r[R_EAX], ctx[6];
+    if (DSW(DS_00104B00) == 0x22u) { *eax = 0u; return; }
+    fighter_ctx_same(ctx, side);
+    if ((s32)DSD(ctx[2] + 0x2Cu) < (s32)DSD(ctx[3] + 0x2Cu)) DSB(ctx[4] + 0x28u) |= 0x40u;
+    else DSB(ctx[4] + 0x28u) &= 0xBFu;
+    DSD(ctx[4] + 0x18u) = hit_record_x(side);
+    *eax = 0u;
+}
+static void b_18b44(const u32 *r, u32 *eax)            { fighter_18b44(r[R_EAX]); *eax = 0u; }
+static void m_18b44(const u32 *r, u32 *eax)            /* the descriptor bit ignored */
+{
+    u32 slot = r[R_EAX];
+    if (DSB(slot + 0x63u) == 1u) { *eax = 0u; return; }
+    if (DSB(DS_00100C1D) != 0u) { *eax = 0u; return; }
+    DSB(DS_00100C1D) = 1u;
+    (void)actor_spawn((const u32 *)(mem + 0xA17DCu),
+                      (u32)((s32)DSD(0x000A17D4u) >> 16), 0xFEu,
+                      (u32)((s32)DSD(0x000A17D6u) >> 16), 0u);
+    (void)actor_spawn((const u32 *)(mem + 0xA17F0u),
+                      (u32)((s32)DSD(0x000A17D4u) >> 16), 0xFFu,
+                      (u32)((s32)DSD(0x000A17D6u) >> 16), 0u);
+    *eax = 0u;
+}
+static void m_18b44_latch(const u32 *r, u32 *eax)      /* the once latch not set */
+{
+    u32 slot = r[R_EAX];
+    if (DSB(slot + 0x63u) == 1u) { *eax = 0u; return; }
+    if (DSB(DS_00100C1D) != 0u) { *eax = 0u; return; }
+    u32 desc = ((DSB(0x00104529u) & 2u) != 0u) ? 0xA1804u : 0xA17DCu;
+    (void)actor_spawn((const u32 *)(mem + desc),
+                      (u32)((s32)DSD(0x000A17D4u) >> 16), 0xFEu,
+                      (u32)((s32)DSD(0x000A17D6u) >> 16), 0u);
+    (void)actor_spawn((const u32 *)(mem + 0xA17F0u),
+                      (u32)((s32)DSD(0x000A17D4u) >> 16), 0xFFu,
+                      (u32)((s32)DSD(0x000A17D6u) >> 16), 0u);
+    *eax = 0u;
+}
+static void m_18b44_layer(const u32 *r, u32 *eax)      /* both layers 0xFE */
+{
+    u32 slot = r[R_EAX];
+    if (DSB(slot + 0x63u) == 1u) { *eax = 0u; return; }
+    if (DSB(DS_00100C1D) != 0u) { *eax = 0u; return; }
+    DSB(DS_00100C1D) = 1u;
+    u32 desc = ((DSB(0x00104529u) & 2u) != 0u) ? 0xA1804u : 0xA17DCu;
+    (void)actor_spawn((const u32 *)(mem + desc),
+                      (u32)((s32)DSD(0x000A17D4u) >> 16), 0xFEu,
+                      (u32)((s32)DSD(0x000A17D6u) >> 16), 0u);
+    (void)actor_spawn((const u32 *)(mem + 0xA17F0u),
+                      (u32)((s32)DSD(0x000A17D4u) >> 16), 0xFEu,
+                      (u32)((s32)DSD(0x000A17D6u) >> 16), 0u);
+    *eax = 0u;
+}
+static void b_1ddf4(const u32 *r, u32 *eax)            { *eax = (u32)hit_geometry(r[R_EAX], r[R_EDX], r[R_EBX]); }
+static void m_1ddf4(const u32 *r, u32 *eax)            /* the second test dropped */
+{
+    u32 side = r[R_EAX], table = r[R_EDX], idx = r[R_EBX];
+    u32 ch = (u32)DSB(DS_001077B0 + side * 0x94u + 0x7Au);
+    s32 d1 = ai_distance();
+    if (d1 < 0) d1 = -d1;
+    *eax = (u32)(d1 <= (s32)((u32)DSB(table + ch) << 6));
+}
+static void m_1ddf4_side(const u32 *r, u32 *eax)       /* always side 0 */
+{
+    *eax = (u32)hit_geometry(0u, r[R_EDX], r[R_EBX]);
+}
+static void m_1ddf4_abs(const u32 *r, u32 *eax)        /* |d1| dropped */
+{
+    u32 side = r[R_EAX], table = r[R_EDX], idx = r[R_EBX];
+    u32 ch = (u32)DSB(DS_001077B0 + side * 0x94u + 0x7Au);
+    s32 d1 = ai_distance();
+    if (d1 > (s32)((u32)DSB(table + ch) << 6)) { *eax = 0u; return; }
+    s32 d2 = hit_vert_distance();
+    if (d2 < 0) d2 = -d2;
+    *eax = (u32)(d2 <= (s32)((u32)DSB(idx + ch) << 6));
+}
+static void m_1ddf4_table(const u32 *r, u32 *eax)      /* the two thresholds swapped */
+{
+    u32 side = r[R_EAX], table = r[R_EDX], idx = r[R_EBX];
+    u32 ch = (u32)DSB(DS_001077B0 + side * 0x94u + 0x7Au);
+    s32 d1 = ai_distance();
+    if (d1 < 0) d1 = -d1;
+    if (d1 > (s32)((u32)DSB(idx + ch) << 6)) { *eax = 0u; return; }
+    s32 d2 = hit_vert_distance();
+    if (d2 < 0) d2 = -d2;
+    *eax = (u32)(d2 <= (s32)((u32)DSB(table + ch) << 6));
+}
+static void b_39f40(const u32 *r, u32 *eax)
+{
+    fighter_pose_start(r[R_EAX], r[R_EDX], r[R_EBX], r[R_ECX], r[R_S0]);
+    *eax = 0u;
+}
+static void m_39f40(const u32 *r, u32 *eax)            /* the pose words at one base */
+{
+    u32 side = r[R_EAX], ctx[6];
+    fighter_ctx_swap(ctx, side);
+    u32 s = ctx[1];
+    DSD(DS_00107A68 + s * 4u) = r[R_EDX];
+    DSD(DS_00107A68 + s * 4u) = r[R_EBX];
+    DSD(DS_00107A68 + s * 4u) = r[R_ECX];
+    DSD(DS_00107A68 + s * 4u) = r[R_S0];
+    DSB(ctx[3] + 0x52u) = 0x10u;
+    DSB(ctx[3] + 0x53u) = 0x0Au;
+    DSB(ctx[3] + 0x54u) = 2u;
+    DSD(ctx[3] + 0x10u) = 0x39CC8u;
+    DSB(ctx[3] + 0x58u) = 0;
+    DSD(ctx[5] + 0x24u) = 0;
+    *eax = 0u;
+}
+static void m_39f40_side(const u32 *r, u32 *eax)       /* always side 0 */
+{
+    (void)r;
+    fighter_pose_start(0u, r[R_EDX], r[R_EBX], r[R_ECX], r[R_S0]);
+    *eax = 0u;
+}
+static void m_39f40_arg(const u32 *r, u32 *eax)        /* the frame from ECX */
+{
+    fighter_pose_start(r[R_EAX], r[R_EDX], r[R_EBX], r[R_ECX], r[R_ECX]);
+    *eax = 0u;
+}
+static void m_39f40_field(const u32 *r, u32 *eax)      /* the handler 0x39CC0 */
+{
+    u32 side = r[R_EAX], ctx[6];
+    fighter_ctx_swap(ctx, side);
+    u32 s = ctx[1];
+    DSD(DS_00107A68 + s * 4u) = r[R_EDX];
+    DSD(DS_00107A78 + s * 4u) = r[R_EBX];
+    DSD(DS_00107A60 + s * 4u) = r[R_ECX];
+    DSD(DS_00107A70 + s * 4u) = r[R_S0];
+    DSB(ctx[3] + 0x52u) = 0x10u;
+    DSB(ctx[3] + 0x53u) = 0x0Au;
+    DSB(ctx[3] + 0x54u) = 2u;
+    DSD(ctx[3] + 0x10u) = 0x39CC0u;
+    DSB(ctx[3] + 0x58u) = 0;
+    DSD(ctx[5] + 0x24u) = 0;
+    *eax = 0u;
+}
+static void b_2bcf4(const u32 *r, u32 *eax)            { actors_anim_seek(r[R_EAX], r[R_EDX]); *eax = 0u; }
+static void m_2bcf4(const u32 *r, u32 *eax)            /* the +0x28 mask 0xEF */
+{
+    u32 rec = r[R_EAX];
+    DSD(rec + 8u) = r[R_EDX];
+    DSB(rec + 0x28u) &= 0xEFu;
+    u32 pset = DSD(DS_001014EC) + (u32)DSW(rec + 0x56u) * PSET_SIZE;
+    DSW(pset) = (u16)anim_next_sprite_id(rec, pset);
+    *eax = 0u;
+}
+static void m_2bcf4_char(const u32 *r, u32 *eax)       /* the pset index forced to 0 */
+{
+    u32 rec = r[R_EAX];
+    DSD(rec + 8u) = r[R_EDX];
+    DSB(rec + 0x28u) &= 0xEBu;
+    u32 pset = DSD(DS_001014EC);
+    DSW(pset) = (u16)anim_next_sprite_id(rec, pset);
+    *eax = 0u;
+}
+static void m_2bcf4_width(const u32 *r, u32 *eax)      /* the pset word stored as a dword */
+{
+    u32 rec = r[R_EAX];
+    DSD(rec + 8u) = r[R_EDX];
+    DSB(rec + 0x28u) &= 0xEBu;
+    u32 pset = DSD(DS_001014EC) + (u32)DSW(rec + 0x56u) * PSET_SIZE;
+    DSD(pset) = (u32)anim_next_sprite_id(rec, pset);
+    *eax = 0u;
+}
+static void b_37d18(const u32 *r, u32 *eax)            { fighter_37d18(r[R_EAX], r[R_EDX]); *eax = 0u; }
+static void m_37d18(const u32 *r, u32 *eax)            /* the other slot not inverted */
+{
+    u32 slot = r[R_EAX], rec = r[R_EDX];
+    DSB(slot + 0x52u) = 9u;
+    DSB(slot + 0x53u) = 3u;
+    DSB(slot + 0x54u) = 3u;
+    DSB(slot + 0x42u) = (u8)((DSB(slot + 0x42u) & 0xDBu) | 0x04u);
+    actors_anim_begin(rec, DSD(0x000C9238u + (u32)DSB(slot + 0x7Au) * 4u), 0x40800000u);
+    fighter_39a10(rec, 0x309u);
+    (void)sound_voice((u32)DSW(0x000BDAD4u + (u32)DSB(slot + 0x7Au) * 2u));
+    u32 other = DSD(0x001077A8u + (u32)DSB(rec + 0x51u) * 4u);
+    if (other != 0u) {
+        DSB(DSD(other) + 0x59u) = 0xFFu;
+        DSD(other + 0x40u) |= 0x801000u;
+    }
+    DSD(0x001078DCu) = 0xBD89Cu;
+    *eax = 0u;
+}
+static void m_37d18_state(const u32 *r, u32 *eax)      /* +0x52 = 8 */
+{
+    u32 slot = r[R_EAX], rec = r[R_EDX];
+    DSB(slot + 0x52u) = 8u;
+    DSB(slot + 0x53u) = 3u;
+    DSB(slot + 0x54u) = 3u;
+    DSB(slot + 0x42u) = (u8)((DSB(slot + 0x42u) & 0xDBu) | 0x04u);
+    actors_anim_begin(rec, DSD(0x000C9238u + (u32)DSB(slot + 0x7Au) * 4u), 0x40800000u);
+    fighter_39a10(rec, 0x309u);
+    (void)sound_voice((u32)DSW(0x000BDAD4u + (u32)DSB(slot + 0x7Au) * 2u));
+    u32 other = DSD(0x001077A8u + ((u32)DSB(rec + 0x51u) ^ 1u) * 4u);
+    if (other != 0u) {
+        DSB(DSD(other) + 0x59u) = 0xFFu;
+        DSD(other + 0x40u) |= 0x801000u;
+    }
+    DSD(0x001078DCu) = 0xBD89Cu;
+    *eax = 0u;
+}
+static void m_37d18_order(const u32 *r, u32 *eax)      /* the approach pointer before the voice */
+{
+    u32 slot = r[R_EAX], rec = r[R_EDX];
+    DSB(slot + 0x52u) = 9u;
+    DSB(slot + 0x53u) = 3u;
+    DSB(slot + 0x54u) = 3u;
+    DSB(slot + 0x42u) = (u8)((DSB(slot + 0x42u) & 0xDBu) | 0x04u);
+    actors_anim_begin(rec, DSD(0x000C9238u + (u32)DSB(slot + 0x7Au) * 4u), 0x40800000u);
+    fighter_39a10(rec, 0x309u);
+    DSD(0x001078DCu) = 0xBD89Cu;
+    (void)sound_voice((u32)DSW(0x000BDAD4u + (u32)DSB(slot + 0x7Au) * 2u));
+    u32 other = DSD(0x001077A8u + ((u32)DSB(rec + 0x51u) ^ 1u) * 4u);
+    if (other != 0u) {
+        DSB(DSD(other) + 0x59u) = 0xFFu;
+        DSD(other + 0x40u) |= 0x801000u;
+    }
+    *eax = 0u;
+}
+static void b_3aa54(const u32 *r, u32 *eax)            { *eax = fighter_3aa54(r[R_EAX]); }
+static void m_3aa54(const u32 *r, u32 *eax)            /* the negation condition inverted */
+{
+    u32 slot = r[R_EAX], rec = DSD(slot);
+    u32 ch = (u32)DSB(slot + 0x7Au);
+    DSD(rec + 0x24u) = 0;
+    DSW(rec + 0x44u) = DSW(0x000BEC88u + ch * 4u);
+    DSW(rec + 0x36u) = DSW(0x000BECA4u + ch * 4u);
+    DSW(rec + 0x34u) = DSW(0x000BECC0u + ch * 4u);
+    DSB(rec + 0x43u) = DSB(0x000BECDCu + ch * 4u);
+    DSB(slot + 0x52u) = 0x11u;
+    DSB(slot + 0x53u) = 0x0Au;
+    DSB(slot + 0x54u) = 2u;
+    DSB(slot + 0x41u) |= 0x80u;
+    DSD(slot + 0x10u) = 0;
+    DSB(slot + 0x58u) = 0;
+    DSB(rec + 0x28u) &= 0xDFu;
+    if (fighter_1a5ac((u32)DSB(rec + 0x51u)) != 0) DSW(rec + 0x34u) = (u16)(-(s32)(s16)DSW(rec + 0x34u));
+    *eax = DSB(slot + 0x52u);
+}
+static void m_3aa54_char(const u32 *r, u32 *eax)       /* the character index forced to 0 */
+{
+    u32 slot = r[R_EAX], rec = DSD(slot);
+    DSD(rec + 0x24u) = 0;
+    DSW(rec + 0x44u) = DSW(0x000BEC88u);
+    DSW(rec + 0x36u) = DSW(0x000BECA4u);
+    DSW(rec + 0x34u) = DSW(0x000BECC0u);
+    DSB(rec + 0x43u) = DSB(0x000BECDCu);
+    DSB(slot + 0x52u) = 0x11u;
+    DSB(slot + 0x53u) = 0x0Au;
+    DSB(slot + 0x54u) = 2u;
+    DSB(slot + 0x41u) |= 0x80u;
+    DSD(slot + 0x10u) = 0;
+    DSB(slot + 0x58u) = 0;
+    DSB(rec + 0x28u) &= 0xDFu;
+    if (fighter_1a5ac((u32)DSB(rec + 0x51u)) == 0) DSW(rec + 0x34u) = (u16)(-(s32)(s16)DSW(rec + 0x34u));
+    *eax = DSB(slot + 0x52u);
+}
+static void m_3aa54_field(const u32 *r, u32 *eax)      /* the +0x44 word from the +0x36 table */
+{
+    u32 slot = r[R_EAX], rec = DSD(slot);
+    u32 ch = (u32)DSB(slot + 0x7Au);
+    DSD(rec + 0x24u) = 0;
+    DSW(rec + 0x44u) = DSW(0x000BECA4u + ch * 4u);
+    DSW(rec + 0x36u) = DSW(0x000BEC88u + ch * 4u);
+    DSW(rec + 0x34u) = DSW(0x000BECC0u + ch * 4u);
+    DSB(rec + 0x43u) = DSB(0x000BECDCu + ch * 4u);
+    DSB(slot + 0x52u) = 0x11u;
+    DSB(slot + 0x53u) = 0x0Au;
+    DSB(slot + 0x54u) = 2u;
+    DSB(slot + 0x41u) |= 0x80u;
+    DSD(slot + 0x10u) = 0;
+    DSB(slot + 0x58u) = 0;
+    DSB(rec + 0x28u) &= 0xDFu;
+    if (fighter_1a5ac((u32)DSB(rec + 0x51u)) == 0) DSW(rec + 0x34u) = (u16)(-(s32)(s16)DSW(rec + 0x34u));
+    *eax = DSB(slot + 0x52u);
+}
+static void b_2ad40(const u32 *r, u32 *eax)            { release_record(r[R_EAX], r[R_EDX]); *eax = 0u; }
+static void m_2ad40(const u32 *r, u32 *eax)            /* the child's dead bit and set_dead skipped */
+{
+    u32 rec = r[R_EAX], pset = r[R_EDX];
+    if ((DSW(rec + 0x2au) & 8u) != 0) { *eax = 0u; return; }
+    if (DSB(rec + 0x4f) != 0) { *eax = 0u; return; }
+    if ((DSW(rec + 0x28u) >> 8 & 4u) != 0) {
+        u32 parent = DSD(DS_001014F4) + (u32)DSB(rec + 0x4a) * ACTOR_REC_SIZE;
+        DSB(parent + 0x4f) = (u8)(DSB(parent + 0x4f) - 1);
+    }
+    {
+        u32 prev = DSD(rec + 4);
+        DSD(DSD(rec) + 4) = prev;
+        DSD(prev) = DSD(rec);
+        DSD(rec + 4) = 0;
+        DSD(rec) = 0;
+    }
+    {
+        u32 at = DS_00105B3C, next = DSD(at);
+        DSD(at) = rec;
+        DSD(rec) = next;
+        DSD(rec + 4) = at;
+        DSD(next + 4) = rec;
+    }
+    DSD(pset + 8) = 0;
+    DSW(pset + 0x0c) = 0;
+    DSD(pset + 4) = DSD(pset + 8);
+    DSW(pset + 0x0e) = DSW(pset + 0x0c);
+    actor_set_dead(rec);
+    DSB(rec + 0x4b) = 0;
+    DSB(rec + 0x4a) = DSB(rec + 0x4b);
+    *eax = 0u;
+}
+static void m_2ad40_field(const u32 *r, u32 *eax)      /* pset+4 = 1 */
+{
+    u32 rec = r[R_EAX], pset = r[R_EDX];
+    if ((DSW(rec + 0x2au) & 8u) != 0) { *eax = 0u; return; }
+    if (DSB(rec + 0x4f) != 0) { *eax = 0u; return; }
+    if (DSB(rec + 0x4b) != 0) {
+        u32 child = DSD(DS_001014F4) + (u32)DSB(rec + 0x4b) * ACTOR_REC_SIZE;
+        DSB(child + 0x2au) &= 0xf7u;
+        actor_set_dead(child);
+    }
+    if ((DSW(rec + 0x28u) >> 8 & 4u) != 0) {
+        u32 parent = DSD(DS_001014F4) + (u32)DSB(rec + 0x4a) * ACTOR_REC_SIZE;
+        DSB(parent + 0x4f) = (u8)(DSB(parent + 0x4f) - 1);
+    }
+    {
+        u32 prev = DSD(rec + 4);
+        DSD(DSD(rec) + 4) = prev;
+        DSD(prev) = DSD(rec);
+        DSD(rec + 4) = 0;
+        DSD(rec) = 0;
+    }
+    {
+        u32 at = DS_00105B3C, next = DSD(at);
+        DSD(at) = rec;
+        DSD(rec) = next;
+        DSD(rec + 4) = at;
+        DSD(next + 4) = rec;
+    }
+    DSD(pset + 8) = 0;
+    DSW(pset + 0x0c) = 0;
+    DSD(pset + 4) = 1u;
+    DSW(pset + 0x0e) = DSW(pset + 0x0c);
+    actor_set_dead(rec);
+    DSB(rec + 0x4b) = 0;
+    DSB(rec + 0x4a) = DSB(rec + 0x4b);
+    *eax = 0u;
+}
+static void m_2ad40_latch(const u32 *r, u32 *eax)      /* the second set_dead skipped */
+{
+    u32 rec = r[R_EAX], pset = r[R_EDX];
+    if ((DSW(rec + 0x2au) & 8u) != 0) { *eax = 0u; return; }
+    if (DSB(rec + 0x4f) != 0) { *eax = 0u; return; }
+    if (DSB(rec + 0x4b) != 0) {
+        u32 child = DSD(DS_001014F4) + (u32)DSB(rec + 0x4b) * ACTOR_REC_SIZE;
+        DSB(child + 0x2au) &= 0xf7u;
+        actor_set_dead(child);
+    }
+    if ((DSW(rec + 0x28u) >> 8 & 4u) != 0) {
+        u32 parent = DSD(DS_001014F4) + (u32)DSB(rec + 0x4a) * ACTOR_REC_SIZE;
+        DSB(parent + 0x4f) = (u8)(DSB(parent + 0x4f) - 1);
+    }
+    {
+        u32 prev = DSD(rec + 4);
+        DSD(DSD(rec) + 4) = prev;
+        DSD(prev) = DSD(rec);
+        DSD(rec + 4) = 0;
+        DSD(rec) = 0;
+    }
+    {
+        u32 at = DS_00105B3C, next = DSD(at);
+        DSD(at) = rec;
+        DSD(rec) = next;
+        DSD(rec + 4) = at;
+        DSD(next + 4) = rec;
+    }
+    DSD(pset + 8) = 0;
+    DSW(pset + 0x0c) = 0;
+    DSD(pset + 4) = DSD(pset + 8);
+    DSW(pset + 0x0e) = DSW(pset + 0x0c);
+    DSB(rec + 0x4b) = 0;
+    DSB(rec + 0x4a) = DSB(rec + 0x4b);
+    *eax = 0u;
+}
+
+static void b_2a408(const u32 *r, u32 *eax)            { *eax = anim_next_sprite_id(r[R_EAX], r[R_EDX]); }
+static void m_2a408(const u32 *r, u32 *eax)            /* the 0xD00 arm always keeps the pset word */
+{
+    u32 rec = r[R_EAX], pset = r[R_EDX];
+    u32 res;
+    if ((DSW(rec + 0x28) >> 8 & 8u) != 0) res = DSW(rec + 8);
+    else {
+        u32 p = DSD(rec + 8);
+        u16 word = DSW(p);
+        res = word;
+        if ((word & 0x8000u) != 0) res = DSW(pset) & 0x7fffu;
+    }
+    u32 clearbit = (res & 0x8000u) == 0 ? 1u : 0u;
+    u32 hflip = ((DSW(rec + 0x28) >> 8) & 0x40u) == 0 ? 1u : 0u;
+    if (clearbit == hflip) *eax = res & 0x7fffu;
+    else *eax = (res & 0x7fffu) | 0x8000u;
+}
+static void m_2a408_bit8(const u32 *r, u32 *eax)       /* the +0x28 bit-8 test dropped */
+{
+    u32 rec = r[R_EAX], pset = r[R_EDX];
+    u32 p = DSD(rec + 8);
+    u16 word = DSW(p);
+    u32 res = word;
+    if ((word & 0x8000u) != 0) {
+        if ((word & 0x1f00u) == 0xd00u) {
+            u32 e = p + 2;
+            DSD(rec + 8) = e;
+            if (((word >> 8) & 0x60u) == 0x40u) res = (u32)DSW(e) + anim_read_var(rec, (u8)(word & 0x7fu));
+            else {
+                DSD(rec + 8) = p + 4;
+                u32 rv = anim_read_var(rec, (u8)(word & 0x7fu));
+                u32 tab = DSD(p + 2);
+                res = DSW(tab + (rv & 0xffffu) * 2u);
+            }
+        } else res = DSW(pset) & 0x7fffu;
+    }
+    u32 clearbit = (res & 0x8000u) == 0 ? 1u : 0u;
+    u32 hflip = ((DSW(rec + 0x28) >> 8) & 0x40u) == 0 ? 1u : 0u;
+    if (clearbit == hflip) *eax = res & 0x7fffu;
+    else *eax = (res & 0x7fffu) | 0x8000u;
+}
+static void m_2a408_op(const u32 *r, u32 *eax)         /* the 0xD00 opcode test on the wrong mask */
+{
+    u32 rec = r[R_EAX], pset = r[R_EDX];
+    u32 res;
+    if ((DSW(rec + 0x28) >> 8 & 8u) != 0) res = DSW(rec + 8);
+    else {
+        u32 p = DSD(rec + 8);
+        u16 word = DSW(p);
+        res = word;
+        if ((word & 0x8000u) != 0) {
+            if ((word & 0x1fu) == 0u) res = DSW(pset) & 0x7fffu;
+            else {
+                u32 e = p + 2;
+                DSD(rec + 8) = e;
+                if (((word >> 8) & 0x60u) == 0x40u) res = (u32)DSW(e) + anim_read_var(rec, (u8)(word & 0x7fu));
+                else {
+                    DSD(rec + 8) = p + 4;
+                    u32 rv = anim_read_var(rec, (u8)(word & 0x7fu));
+                    u32 tab = DSD(p + 2);
+                    res = DSW(tab + (rv & 0xffffu) * 2u);
+                }
+            }
+        }
+    }
+    u32 clearbit = (res & 0x8000u) == 0 ? 1u : 0u;
+    u32 hflip = ((DSW(rec + 0x28) >> 8) & 0x40u) == 0 ? 1u : 0u;
+    if (clearbit == hflip) *eax = res & 0x7fffu;
+    else *eax = (res & 0x7fffu) | 0x8000u;
+}
+static void m_2a408_var(const u32 *r, u32 *eax)        /* 0x29F34 given the pset instead of the op */
+{
+    u32 rec = r[R_EAX], pset = r[R_EDX];
+    u32 res;
+    if ((DSW(rec + 0x28) >> 8 & 8u) != 0) res = DSW(rec + 8);
+    else {
+        u32 p = DSD(rec + 8);
+        u16 word = DSW(p);
+        res = word;
+        if ((word & 0x8000u) != 0) {
+            if ((word & 0x1f00u) == 0xd00u) {
+                u32 e = p + 2;
+                DSD(rec + 8) = e;
+                if (((word >> 8) & 0x60u) == 0x40u) res = (u32)DSW(e) + anim_read_var(rec, (u8)pset);
+                else {
+                    DSD(rec + 8) = p + 4;
+                    u32 rv = anim_read_var(rec, (u8)pset);
+                    u32 tab = DSD(p + 2);
+                    res = DSW(tab + (rv & 0xffffu) * 2u);
+                }
+            } else res = DSW(pset) & 0x7fffu;
+        }
+    }
+    u32 clearbit = (res & 0x8000u) == 0 ? 1u : 0u;
+    u32 hflip = ((DSW(rec + 0x28) >> 8) & 0x40u) == 0 ? 1u : 0u;
+    if (clearbit == hflip) *eax = res & 0x7fffu;
+    else *eax = (res & 0x7fffu) | 0x8000u;
+}
+static void m_2a408_clear(const u32 *r, u32 *eax)      /* the tail XOR inverted */
+{
+    u32 rec = r[R_EAX], pset = r[R_EDX];
+    u32 res;
+    if ((DSW(rec + 0x28) >> 8 & 8u) != 0) res = DSW(rec + 8);
+    else {
+        u32 p = DSD(rec + 8);
+        u16 word = DSW(p);
+        res = word;
+        if ((word & 0x8000u) != 0) {
+            if ((word & 0x1f00u) == 0xd00u) {
+                u32 e = p + 2;
+                DSD(rec + 8) = e;
+                if (((word >> 8) & 0x60u) == 0x40u) res = (u32)DSW(e) + anim_read_var(rec, (u8)(word & 0x7fu));
+                else {
+                    DSD(rec + 8) = p + 4;
+                    u32 rv = anim_read_var(rec, (u8)(word & 0x7fu));
+                    u32 tab = DSD(p + 2);
+                    res = DSW(tab + (rv & 0xffffu) * 2u);
+                }
+            } else res = DSW(pset) & 0x7fffu;
+        }
+    }
+    u32 clearbit = (res & 0x8000u) == 0 ? 1u : 0u;
+    u32 hflip = ((DSW(rec + 0x28) >> 8) & 0x40u) == 0 ? 1u : 0u;
+    if (clearbit != hflip) *eax = res & 0x7fffu;
+    else *eax = (res & 0x7fffu) | 0x8000u;
+}
+
+static void b_36638(const u32 *r, u32 *eax)            { *eax = (u32)fighter_state_36638(r[R_EAX], r[R_EDX]); }
+static void m_36638(const u32 *r, u32 *eax)            /* the +0x54 == 3 early-out dropped */
+{
+    u32 slot = r[R_EAX], rec = r[R_EDX];
+    if ((DSB(slot + 0x43u) & 0x40u) == 0u) { *eax = 0u; return; }
+    DSD(slot + 0x40u) &= 0xBFFF7FFFu;
+    DSB(slot + 0x41u) |= 0x80u;
+    DSB(slot + 0x53u) = DSW(DS_00104B00) == 0x25u ? 0x0Cu : 0u;
+    switch (DSB(slot + 0x54u)) {
+    case 1u:
+        actors_anim_begin(rec, DSD(0x000C9210u + (u32)DSB(slot + 0x7Au) * 4u), 0x3F800000u);
+        DSB(slot + 0x52u) = 0x12u;
+        *eax = 1u;
+        return;
+    case 4u:
+        actors_anim_begin(rec, DSD(0x000C91E8u + (u32)DSB(slot + 0x7Au) * 4u), 0x3F800000u);
+        DSB(slot + 0x52u) = 8u;
+        DSB(slot + 0x57u) = 2u;
+        *eax = 0u;
+        return;
+    case 5u:
+        fighter_38154((u32)DSB(rec + 0x51u));
+        *eax = 0u;
+        return;
+    default:
+        actors_anim_begin(rec, DSD(0x000C91E8u + (u32)DSB(slot + 0x7Au) * 4u), 0x3F800000u);
+        DSB(slot + 0x52u) = 0x12u;
+        *eax = 1u;
+        return;
+    }
+}
+static void m_36638_anim(const u32 *r, u32 *eax)       /* the case-1 stream from the case-4 table */
+{
+    u32 slot = r[R_EAX], rec = r[R_EDX];
+    if (DSB(slot + 0x54u) == 3u) {
+        DSB(slot + 0x43u) &= 0xBFu;
+        *eax = 0u;
+        return;
+    }
+    if ((DSB(slot + 0x43u) & 0x40u) == 0u) { *eax = 0u; return; }
+    DSD(slot + 0x40u) &= 0xBFFF7FFFu;
+    DSB(slot + 0x41u) |= 0x80u;
+    DSB(slot + 0x53u) = DSW(DS_00104B00) == 0x25u ? 0x0Cu : 0u;
+    switch (DSB(slot + 0x54u)) {
+    case 1u:
+        actors_anim_begin(rec, DSD(0x000C91E8u + (u32)DSB(slot + 0x7Au) * 4u), 0x3F800000u);
+        DSB(slot + 0x52u) = 0x12u;
+        *eax = 1u;
+        return;
+    default:
+        actors_anim_begin(rec, DSD(0x000C91E8u + (u32)DSB(slot + 0x7Au) * 4u), 0x3F800000u);
+        DSB(slot + 0x52u) = 0x12u;
+        *eax = 1u;
+        return;
+    }
+}
+static void m_36638_mode(const u32 *r, u32 *eax)       /* the mode word ignored */
+{
+    u32 slot = r[R_EAX], rec = r[R_EDX];
+    if (DSB(slot + 0x54u) == 3u) {
+        DSB(slot + 0x43u) &= 0xBFu;
+        *eax = 0u;
+        return;
+    }
+    if ((DSB(slot + 0x43u) & 0x40u) == 0u) { *eax = 0u; return; }
+    DSD(slot + 0x40u) &= 0xBFFF7FFFu;
+    DSB(slot + 0x41u) |= 0x80u;
+    DSB(slot + 0x53u) = 0u;
+    switch (DSB(slot + 0x54u)) {
+    case 1u:
+        actors_anim_begin(rec, DSD(0x000C9210u + (u32)DSB(slot + 0x7Au) * 4u), 0x3F800000u);
+        DSB(slot + 0x52u) = 0x12u;
+        *eax = 1u;
+        return;
+    case 4u:
+        actors_anim_begin(rec, DSD(0x000C91E8u + (u32)DSB(slot + 0x7Au) * 4u), 0x3F800000u);
+        DSB(slot + 0x52u) = 8u;
+        DSB(slot + 0x57u) = 2u;
+        *eax = 0u;
+        return;
+    case 5u:
+        fighter_38154((u32)DSB(rec + 0x51u));
+        *eax = 0u;
+        return;
+    default:
+        actors_anim_begin(rec, DSD(0x000C91E8u + (u32)DSB(slot + 0x7Au) * 4u), 0x3F800000u);
+        DSB(slot + 0x52u) = 0x12u;
+        *eax = 1u;
+        return;
+    }
+}
+static void m_36638_side(const u32 *r, u32 *eax)       /* 0x38154 on side 0 */
+{
+    u32 slot = r[R_EAX], rec = r[R_EDX];
+    if (DSB(slot + 0x54u) == 3u) {
+        DSB(slot + 0x43u) &= 0xBFu;
+        *eax = 0u;
+        return;
+    }
+    if ((DSB(slot + 0x43u) & 0x40u) == 0u) { *eax = 0u; return; }
+    DSD(slot + 0x40u) &= 0xBFFF7FFFu;
+    DSB(slot + 0x41u) |= 0x80u;
+    DSB(slot + 0x53u) = DSW(DS_00104B00) == 0x25u ? 0x0Cu : 0u;
+    switch (DSB(slot + 0x54u)) {
+    case 1u:
+        actors_anim_begin(rec, DSD(0x000C9210u + (u32)DSB(slot + 0x7Au) * 4u), 0x3F800000u);
+        DSB(slot + 0x52u) = 0x12u;
+        *eax = 1u;
+        return;
+    case 4u:
+        actors_anim_begin(rec, DSD(0x000C91E8u + (u32)DSB(slot + 0x7Au) * 4u), 0x3F800000u);
+        DSB(slot + 0x52u) = 8u;
+        DSB(slot + 0x57u) = 2u;
+        *eax = 0u;
+        return;
+    case 5u:
+        fighter_38154(0u);
+        *eax = 0u;
+        return;
+    default:
+        actors_anim_begin(rec, DSD(0x000C91E8u + (u32)DSB(slot + 0x7Au) * 4u), 0x3F800000u);
+        DSB(slot + 0x52u) = 0x12u;
+        *eax = 1u;
+        return;
+    }
+}
+
 static const binding_t k_bindings[] = {
     { "rng_next",                 b_rng_next,     0xFFFFFFFFu },
     { "fighter_slot_flag",        b_slot_flag,    0x000000FFu },
@@ -5566,7 +6672,7 @@ static const binding_t k_bindings[] = {
     { "fighter_243f8",          b_243f8,         0x00000000u },
     { "fighter_243f8@mutant",   m_243f8,         0x00000000u },
     { "fighter_243f8@slot",     m_243f8_slot,    0x00000000u },
-    { "fighter_29c08",          b_29c08,         0x00000000u },
+    { "fighter_29c08",          b_29c08,         0xFFFFFFFFu },
     { "fighter_15510",          b_15510,         0x00000000u },
     { "fighter_15510@mutant",   m_15510,         0x00000000u },
     { "fighter_15510@side",     m_15510_side,    0x00000000u },
@@ -5757,6 +6863,120 @@ static const binding_t k_bindings[] = {
     { "fighter_18c14@mutant",     m_18c14,        0x000000FFu },
     { "fighter_18c14@store",      m_18c14_store,  0x000000FFu },
     { "fighter_18c14@live",       m_18c14_live,   0x000000FFu },
+    { "fighter_state_39280",      b_39280,        0x00000000u },
+    { "fighter_state_39280@mutant", m_39280,      0x00000000u },
+    { "fighter_state_39280@side", m_39280_side,   0x00000000u },
+    { "fighter_state_39280@width", m_39280_width, 0x00000000u },
+    { "palette_release",          b_33864,        0x00000000u },
+    { "palette_release@mutant",   m_33864,        0x00000000u },
+    { "palette_release@width",    m_33864_width,  0x00000000u },
+    { "palette_release@off",      m_33864_off,    0x00000000u },
+    { "fighter_13244",            b_13244,        0x00000000u },
+    { "fighter_13244@mutant",     m_13244,        0x00000000u },
+    { "fighter_13244@addr",       m_13244_addr,   0x00000000u },
+    { "fighter_13244@val",        m_13244_val,    0x00000000u },
+    { "fighter_29c08@side",       m_29c08_side,   0xFFFFFFFFu },
+    { "fighter_29c08@char",       m_29c08_char,   0xFFFFFFFFu },
+    { "fighter_29c08@sext",       m_29c08_sext,   0xFFFFFFFFu },
+    { "actor_pset_flag_5f",       b_2a148,        0x00000000u },
+    { "actor_pset_flag_5f@mutant", m_2a148,       0x00000000u },
+    { "actor_pset_flag_5f@word",  m_2a148_word,   0x00000000u },
+    { "actor_pset_flag_5f@pset",  m_2a148_pset,   0x00000000u },
+    { "fighter_29bc8",            b_29bc8,        0x00000000u },
+    { "fighter_29bc8@side",       m_29bc8_side,   0x00000000u },
+    { "fighter_29bc8@rec",        m_29bc8_rec,    0x00000000u },
+    { "hit_anchor_y",             b_1890c,        0x00000000u },
+    { "hit_anchor_y@mutant",      m_1890c,        0x00000000u },
+    { "hit_anchor_y@side",        m_1890c_side,   0x00000000u },
+    { "hit_anchor_y@field",       m_1890c_field,  0x00000000u },
+    { "ai_distance",              b_187fc,        0xFFFFFFFFu },
+    { "ai_distance@mutant",       m_187fc,        0xFFFFFFFFu },
+    { "ai_distance@order",        m_187fc_order,  0xFFFFFFFFu },
+    { "fighter_slot_latch",       b_186d0,        0x00000000u },
+    { "fighter_slot_latch@mutant", m_186d0,       0x00000000u },
+    { "fighter_slot_latch@side",  m_186d0_side,   0x00000000u },
+    { "fighter_slot_latch@anchor", m_186d0_anchor, 0x00000000u },
+    { "hit_record_x",             b_18714,        0xFFFFFFFFu },
+    { "hit_record_x@mutant",      m_18714,        0xFFFFFFFFu },
+    { "hit_record_x@side",        m_18714_side,   0xFFFFFFFFu },
+    { "hit_record_x@anchor",      m_18714_anchor, 0xFFFFFFFFu },
+    { "fighter_189fc",            b_189fc,        0x000000FFu },
+    { "fighter_189fc@mutant",     m_189fc,        0x000000FFu },
+    { "fighter_189fc@side",       m_189fc_side,   0x000000FFu },
+    { "fighter_189fc@bit",        m_189fc_bit,    0x000000FFu },
+    { "fighter_189fc@al",         m_189fc_al,     0x000000FFu },
+    { "fighter_18a4c",            b_18a4c,        0x000000FFu },
+    { "fighter_18a4c@mutant",     m_18a4c,        0x000000FFu },
+    { "fighter_18a4c@side",       m_18a4c_side,   0x000000FFu },
+    { "fighter_18a4c@arg",        m_18a4c_arg,    0x000000FFu },
+    { "fighter_39efc",            b_39efc,        0x000000FFu },
+    { "fighter_39efc@mutant",     m_39efc,        0x000000FFu },
+    { "fighter_39efc@val",        m_39efc_val,    0x000000FFu },
+    { "fighter_39efc@side",       m_39efc_side,   0x000000FFu },
+    { "fighter_3b8d8",            b_3b8d8,        0x000000FFu },
+    { "fighter_3b8d8@mutant",     m_3b8d8,        0x000000FFu },
+    { "fighter_3b8d8@side",       m_3b8d8_side,   0x000000FFu },
+    { "fighter_3b8d8@bound",      m_3b8d8_bound,  0x000000FFu },
+    { "fighter_3b90c",            b_3b90c,        0xFFFFFFFFu },
+    { "fighter_3b90c",            b_3b90c,        0xFFFFFFFFu },
+    { "fighter_3b90c@mutant",     m_3b90c,        0xFFFFFFFFu },
+    { "fighter_3b90c@side",       m_3b90c_side,   0xFFFFFFFFu },
+    { "fighter_3b90c@bound",      m_3b90c_bound,  0xFFFFFFFFu },
+    { "fighter_ctx_swap",         b_33a10,        0x00000000u },
+    { "fighter_ctx_swap@mutant",  m_33a10,        0x00000000u },
+    { "fighter_ctx_swap@slot",    m_33a10_slot,   0x00000000u },
+    { "fighter_ctx_swap@rec",     m_33a10_rec,    0x00000000u },
+    { "fighter_1883c",            b_1883c,        0x00000000u },
+    { "fighter_1883c@mutant",     m_1883c,        0x00000000u },
+    { "fighter_1883c@side",       m_1883c_side,   0x00000000u },
+    { "fighter_1883c@latch",      m_1883c_latch,  0x00000000u },
+    { "fighter_1883c@arg",        m_1883c_arg,    0x00000000u },
+    { "hit_facing_flag",          b_18b04,        0x00000000u },
+    { "hit_facing_flag@mutant",   m_18b04,        0x00000000u },
+    { "hit_facing_flag@mode",     m_18b04_mode,   0x00000000u },
+    { "hit_facing_flag@side",     m_18b04_side,   0x00000000u },
+    { "hit_facing_flag@store",    m_18b04_store,  0x00000000u },
+    { "fighter_18b44",            b_18b44,        0x00000000u },
+    { "fighter_18b44@mutant",     m_18b44,        0x00000000u },
+    { "fighter_18b44@latch",      m_18b44_latch,  0x00000000u },
+    { "fighter_18b44@layer",      m_18b44_layer,  0x00000000u },
+    { "hit_geometry",             b_1ddf4,        0x000000FFu },
+    { "hit_geometry@mutant",      m_1ddf4,        0x000000FFu },
+    { "hit_geometry@side",        m_1ddf4_side,   0x000000FFu },
+    { "hit_geometry@abs",         m_1ddf4_abs,    0x000000FFu },
+    { "hit_geometry@table",       m_1ddf4_table,  0x000000FFu },
+    { "fighter_pose_start",       b_39f40,        0x00000000u },
+    { "fighter_pose_start@mutant", m_39f40,       0x00000000u },
+    { "fighter_pose_start@side",  m_39f40_side,   0x00000000u },
+    { "fighter_pose_start@arg",   m_39f40_arg,    0x00000000u },
+    { "fighter_pose_start@field", m_39f40_field,  0x00000000u },
+    { "actors_anim_seek",         b_2bcf4,        0x00000000u },
+    { "actors_anim_seek@mutant",  m_2bcf4,        0x00000000u },
+    { "actors_anim_seek@char",    m_2bcf4_char,   0x00000000u },
+    { "actors_anim_seek@width",   m_2bcf4_width,  0x00000000u },
+    { "fighter_37d18",            b_37d18,        0x00000000u },
+    { "fighter_37d18@mutant",     m_37d18,        0x00000000u },
+    { "fighter_37d18@state",      m_37d18_state,  0x00000000u },
+    { "fighter_37d18@order",      m_37d18_order,  0x00000000u },
+    { "fighter_3aa54",            b_3aa54,        0x00000000u },
+    { "fighter_3aa54@mutant",     m_3aa54,        0x00000000u },
+    { "fighter_3aa54@char",       m_3aa54_char,   0x00000000u },
+    { "fighter_3aa54@field",      m_3aa54_field,  0x00000000u },
+    { "release_record",           b_2ad40,        0x00000000u },
+    { "release_record@mutant",    m_2ad40,        0x00000000u },
+    { "release_record@field",     m_2ad40_field,  0x00000000u },
+    { "release_record@latch",     m_2ad40_latch,  0x00000000u },
+    { "anim_next_sprite_id",      b_2a408,        0x0000FFFFu },
+    { "anim_next_sprite_id@mutant", m_2a408,      0x0000FFFFu },
+    { "anim_next_sprite_id@bit8", m_2a408_bit8,   0x0000FFFFu },
+    { "anim_next_sprite_id@op",   m_2a408_op,     0x0000FFFFu },
+    { "anim_next_sprite_id@var",  m_2a408_var,    0x0000FFFFu },
+    { "anim_next_sprite_id@clear", m_2a408_clear, 0x0000FFFFu },
+    { "fighter_state_36638",      b_36638,        0x000000FFu },
+    { "fighter_state_36638@mutant", m_36638,      0x000000FFu },
+    { "fighter_state_36638@anim", m_36638_anim,   0x000000FFu },
+    { "fighter_state_36638@mode", m_36638_mode,   0x000000FFu },
+    { "fighter_state_36638@side", m_36638_side,   0x000000FFu },
     { "fighter_24338",            b_24338,        0x00000000u },
     { "fighter_3e160",            b_3e160,        0x00000000u },
     { "fighter_24338@mutant",     m_24338,        0x00000000u },
diff --git a/tools/diff_verify.py b/tools/diff_verify.py
index c8627e2..f7c8b69 100644
--- a/tools/diff_verify.py
+++ b/tools/diff_verify.py
@@ -2611,6 +2611,485 @@ C1_SPECS = [
        eax_mask=0xFF, mutants=("@mutant", "@store", "@live")),
 ]
 
+# ---- track P batch C2: the verification-only callee rows (record 2026-10-04-reverse-c2) ----------
+# The ported callees the P/C1 rows stub that still have no row (record §C2.1). Every field a row
+# writes carries a sentinel that differs from what it writes and every neighbour byte is seeded (the
+# P-track checklist 1-2). A row's callee is stubbed through its seam on BOTH sides (the port's C
+# returns at its PR_SEAM), so the callee's own behaviour is its own row's claim while this row's
+# call list, its arguments and the memory at each call are compared (E3 §E3.10, §E3.12).
+DS_1088C2 = 0x1088C2           # DS_001088C2: the byte 0x13244 sets
+DS_1014EC = 0x1014EC           # DS_001014EC: the pset pool base
+DS_1014F4 = 0x1014F4           # DS_001014F4: the actor pool base (the port's in_pool check)
+DS_A8A98 = 0x000A8A98          # DS_000A8A98: the per-character palette-row pointer table
+DS_105B34 = 0x00105B34         # DS_00105B34: the per-side palette index byte table
+DS_BE018 = 0x000BE018          # DS_000BE018: the arena wall
+DS_104B00 = 0x00104B00         # DS_00104B00: the fight mode word
+DS_100C1D = 0x00100C1D         # DS_00100C1D: the once-per-fight dust latch
+C2_PSET = 0x10A600             # zero BSS: a pset base for DS_001014EC
+C2_POOL = 0x10A2B8             # 0x68-aligned zero BSS: an actor-pool record base
+C2_REC = 0x10A2B8
+C2_ROW0 = 0x10A6C0             # zero BSS: two palette rows for 0x29C08/0x29BC8
+C2_STR = 0x10A700              # zero BSS: animation-stream words for 0x2A408
+C2_ROW1 = 0x10A6E0
+C2_ENTRY = 0x10A800            # zero BSS: a palette-table entry {handle; refcount; ...}
+DS_104529 = 0x00104529         # DS_00104529: the dust descriptor selector bit
+FIGHTER_A17D4 = 0x000A17D4     # the dust spawn x word pair
+FIGHTER_A17D6 = 0x000A17D6     # the dust spawn y word pair
+C2_LATCH = E.Call(0x186D0, ("eax",))
+
+C2_SPECS = [
+    # 0x39280 (record §48-C): EAX = side; clear the side slot's +0x5D and +0x43 bit 2. Mask 0: both
+    # callers overwrite EAX at once (0x350D0's +0x41 bit-2 arm and its DS_001078F2 block).
+    Spec("fighter_state_39280", 0x39280, [
+        Case("s0", {"eax": 0}, {DS_SLOTS + 0x42: b"\x42", DS_SLOTS + 0x43: b"\x07", DS_SLOTS + 0x44: b"\x44",
+                                DS_SLOTS + 0x5C: b"\x5c", DS_SLOTS + 0x5D: b"\x5d", DS_SLOTS + 0x5E: b"\x5e",
+                                DS_SLOTS + 0x94 + 0x42: b"\x52", DS_SLOTS + 0x94 + 0x43: b"\x06",
+                                DS_SLOTS + 0x94 + 0x44: b"\x54", DS_SLOTS + 0x94 + 0x5C: b"\x6c",
+                                DS_SLOTS + 0x94 + 0x5D: b"\x6d", DS_SLOTS + 0x94 + 0x5E: b"\x6e"}),
+        Case("s1", {"eax": 1}, {DS_SLOTS + 0x42: b"\x42", DS_SLOTS + 0x43: b"\x07", DS_SLOTS + 0x44: b"\x44",
+                                DS_SLOTS + 0x5C: b"\x5c", DS_SLOTS + 0x5D: b"\x5d", DS_SLOTS + 0x5E: b"\x5e",
+                                DS_SLOTS + 0x94 + 0x42: b"\x52", DS_SLOTS + 0x94 + 0x43: b"\x06",
+                                DS_SLOTS + 0x94 + 0x44: b"\x54", DS_SLOTS + 0x94 + 0x5C: b"\x6c",
+                                DS_SLOTS + 0x94 + 0x5D: b"\x6d", DS_SLOTS + 0x94 + 0x5E: b"\x6e"}),
+    ], eax_mask=0, mutants=("@mutant", "@side", "@width")),
+    # 0x33864 (record §C1.2): EAX = the 0x10-byte palette-table entry; refcount-- and clear the
+    # handle at zero. Mask 0: its only caller 0x2A1C8 stores over EAX at once (0x2A1D1).
+    Spec("palette_release", 0x33864, [
+        Case("r0", {"eax": C2_ENTRY}, {C2_ENTRY: le32(0x11223344), C2_ENTRY + 4: le32(1), C2_ENTRY + 8: le32(0x88888888)}),
+        Case("r1", {"eax": C2_ENTRY}, {C2_ENTRY: le32(0x11223344), C2_ENTRY + 4: le32(2), C2_ENTRY + 8: le32(0x88888888)}),
+        Case("r2", {"eax": C2_ENTRY}, {C2_ENTRY: le32(0x11223344), C2_ENTRY + 4: le32(0x100),
+                                       C2_ENTRY + 8: le32(0x88888888)}),
+    ], eax_mask=0, mutants=("@mutant", "@width", "@off")),
+    # 0x13244 (record §48-P): DS_001088C2 = 1; no argument, no callee. Mask 0 (the caller 0x3F34C
+    # falls through into the spawn tail and overwrites EAX).
+    Spec("fighter_13244", 0x13244, [
+        Case("c0", {}, {DS_1088C2: b"\x00"}),
+        Case("c1", {}, {DS_1088C2: b"\xa5"}),
+    ], eax_mask=0, mutants=("@mutant", "@addr", "@val")),
+    # 0x29C08 (record §P4.5): EAX = side, EDX = char; the handle is DSD(DSD(0xA8A98+ch*4) +
+    # DSB(0x105B34+side)*4). Mask full: 0x3E160 stores the result in a descriptor's +0x10.
+    Spec("fighter_29c08", 0x29C08, [
+        Case("c0", {"eax": 0, "edx": 0}, {DS_A8A98: le32(C2_ROW0), DS_A8A98 + 4: le32(C2_ROW1),
+                                          DS_105B34: b"\x00\x01",
+                                          C2_ROW0: le32(0x11111111), C2_ROW0 + 4: le32(0x22222222),
+                                          C2_ROW1: le32(0x33333333), C2_ROW1 + 4: le32(0x44444444)}),
+        Case("c1", {"eax": 1, "edx": 0}, {DS_A8A98: le32(C2_ROW0), DS_A8A98 + 4: le32(C2_ROW1),
+                                          DS_105B34: b"\x00\x01",
+                                          C2_ROW0: le32(0x11111111), C2_ROW0 + 4: le32(0x22222222),
+                                          C2_ROW1: le32(0x33333333), C2_ROW1 + 4: le32(0x44444444)}),
+        Case("c2", {"eax": 0, "edx": 1}, {DS_A8A98: le32(C2_ROW0), DS_A8A98 + 4: le32(C2_ROW1),
+                                          DS_105B34: b"\x00\x01",
+                                          C2_ROW0: le32(0x11111111), C2_ROW0 + 4: le32(0x22222222),
+                                          C2_ROW1: le32(0x33333333), C2_ROW1 + 4: le32(0x44444444)}),
+        Case("c3", {"eax": 0x80, "edx": 0}, {DS_A8A98: le32(C2_ROW0), DS_105B34 + 0x80: b"\xff",
+                                             C2_ROW0 + 0x3FC: le32(0xAABBCCDD), C2_ROW0 - 4: le32(0xDEADBEEF)}),
+    ], eax_mask=0xFFFFFFFF, mutants=("@side", "@char", "@sext")),
+    # 0x2A148 (record §42-A): EAX = rec, DL = flag; rec+0x5F = flag and the pset's +2 word =
+    # rec+0x2E | 0x800 when the stored flag is non-zero. Mask 0 (the callers ignore EAX; the raw
+    # leaves its 0x800 scratch there). The port's actor_pset checks the pool: DS_001014F4 = C2_REC.
+    Spec("actor_pset_flag_5f", 0x2A148, [
+        Case("f0", {"eax": C2_REC, "edx": 0}, {DS_1014EC: le32(C2_PSET), DS_1014F4: le32(C2_POOL),
+                                               C2_REC + 0x56: b"\x01\x00", C2_REC + 0x2E: b"\x26\x26",
+                                               C2_REC + 0x5F: b"\x5f", C2_PSET + 0x22: b"\x02\x02",
+                                               C2_PSET + 0x2C: b"\x2c\x2c"}),
+        Case("f1", {"eax": C2_REC, "edx": 1}, {DS_1014EC: le32(C2_PSET), DS_1014F4: le32(C2_POOL),
+                                               C2_REC + 0x56: b"\x01\x00", C2_REC + 0x2E: b"\x26\x26",
+                                               C2_REC + 0x5F: b"\x00", C2_PSET + 0x22: b"\x02\x02",
+                                               C2_PSET + 0x2C: b"\x2c\x2c"}),
+        Case("f2", {"eax": C2_REC, "edx": 0x100}, {DS_1014EC: le32(C2_PSET), DS_1014F4: le32(C2_POOL),
+                                                   C2_REC + 0x56: b"\x01\x00", C2_REC + 0x2E: b"\x26\x26",
+                                                   C2_REC + 0x5F: b"\x5f", C2_PSET + 0x22: b"\x02\x02",
+                                                   C2_PSET + 0x2C: b"\x2c\x2c"}),
+    ], eax_mask=0, mutants=("@mutant", "@word", "@pset")),
+    # 0x29BC8 (record §P6.2): EAX = side, EBX = rec, EDX = char; resolve the palette handle and
+    # 0x2A17C(rec, 0, handle). The callee is stubbed through its seam on both sides.
+    Spec("fighter_29bc8", 0x29BC8, [
+        Case("c0", {"eax": 0, "ebx": C2_REC, "edx": 0}, {DS_A8A98: le32(C2_ROW0), DS_A8A98 + 4: le32(C2_ROW1),
+                                                         DS_105B34: b"\x00\x01",
+                                                         C2_ROW0: le32(0x11111111), C2_ROW1: le32(0x22222222)}),
+        Case("c1", {"eax": 1, "ebx": C2_REC, "edx": 1}, {DS_A8A98: le32(C2_ROW0), DS_A8A98 + 4: le32(C2_ROW1),
+                                                         DS_105B34: b"\x00\x01",
+                                                         C2_ROW0: le32(0x11111111), C2_ROW1: le32(0x22222222)}),
+    ], calls=(PALETTE,), eax_mask=0, mutants=("@side", "@rec")),
+    # 0x1890C (record §P6.2): EAX = side, EDX = y; rec+0x1C += y - slot+0x30, latched twice. The
+    # latch is stubbed on both sides, so the case's slot/record bytes survive.
+    Spec("hit_anchor_y", 0x1890C, [
+        Case("y0", {"eax": 0, "edx": 0x100}, {**SLOT_PTRS, DS_SLOTS + 0x30: le32(0x50),
+                                              E3_REC + 0x1C: le32(0x1000), DS_SLOTS + 0x94 + 0x30: le32(0x60),
+                                              E3_REC2 + 0x1C: le32(0x2000)}),
+        Case("y1", {"eax": 0, "edx": 0xFFFFFF00}, {**SLOT_PTRS, DS_SLOTS + 0x30: le32(0x50),
+                                                   E3_REC + 0x1C: le32(0x1000), DS_SLOTS + 0x94 + 0x30: le32(0x60),
+                                                   E3_REC2 + 0x1C: le32(0x2000)}),
+        Case("y2", {"eax": 1, "edx": 0x200}, {**SLOT_PTRS, DS_SLOTS + 0x30: le32(0x50),
+                                              E3_REC + 0x1C: le32(0x1000), DS_SLOTS + 0x94 + 0x30: le32(0x60),
+                                              E3_REC2 + 0x1C: le32(0x2000)}),
+    ], calls=(C2_LATCH,), eax_mask=0, mutants=("@mutant", "@side", "@field")),
+    # 0x187FC (record §C1.2): latch both slots, return slot0+0x2C - slot1+0x2C. Mask full.
+    Spec("ai_distance", 0x187FC, [
+        Case("d0", {}, {**SLOT_PTRS, DS_SLOTS + 0x2C: le32(0x1000), DS_SLOTS + 0x94 + 0x2C: le32(0x800)}),
+        Case("d1", {}, {**SLOT_PTRS, DS_SLOTS + 0x2C: le32(0x800), DS_SLOTS + 0x94 + 0x2C: le32(0x1000)}),
+    ], calls=(C2_LATCH,), eax_mask=0xFFFFFFFF, mutants=("@mutant", "@order")),
+]
+
+# 0x18540/0x18350: the latch's own callees (record §48-V.5); stubbed in 0x186D0/0x18714's rows and
+# seamed in port/src/game/fighter.c. 0x18540 saves and restores everything; 0x18350 clobbers EDX.
+C2_18540 = E.Call(0x18540, ("eax",))
+C2_18350 = E.Call(0x18350, ("eax", "edx"), clobbers=("edx",))
+C2_189FC = E.Call(0x189FC, ("eax",))
+C2_18A4C = E.Call(0x18A4C, ("eax",))
+C2_39EFC = E.Call(0x39EFC, ("eax",))
+C2_3B8D8 = E.Call(0x3B8D8, ("eax", "edx"), clobbers=("edx",))
+C2_3B90C = E.Call(0x3B90C, ("eax", "edx"), clobbers=("edx",))
+C2_ACTOR = 0x1014EC              # DS_001014EC: the pset pool base (1A570's actor word)
+C2_ANCHOR = 0x100AF0             # DS_00100AF0: the per-side screen anchor
+C2_OFFS = 0x100AB0               # DS_00100AB0/AB4: the per-side screen offset pair
+
+C2_SPECS += [
+    # 0x186D0 (record §C1.2): EAX = side; with slot+0x42 bit 3 the record's +0x18/+0x1C latch into
+    # +0x2C/+0x30, else 0x18540(side) runs, the +0x20 anchor is refreshed (0x18350) and the offset
+    # pair is added; +0x41 bit 7 copies +0x2C into +0x34. Mask 0 (every caller ignores EAX).
+    Spec("fighter_slot_latch", 0x186D0, [
+        Case("l0", {"eax": 0}, {**SLOT_PTRS, DS_SLOTS + 0x20: le32(0x24242424), DS_SLOTS + 0x41: b"\x80",
+                                DS_SLOTS + 0x42: b"\x08", DS_SLOTS + 0x2C: le32(0x2C2C2C2C),
+                                DS_SLOTS + 0x30: le32(0x30303030), DS_SLOTS + 0x34: le32(0x34343434),
+                                E3_REC + 0x18: le32(0x18181818), E3_REC + 0x1C: le32(0x1C1C1C1C),
+                                C2_ANCHOR: le32(0x1111) + le32(0x2222), C2_OFFS: le32(0x64) + le32(0x65)}),
+        Case("l1", {"eax": 0}, {**SLOT_PTRS, DS_SLOTS + 0x20: le32(0x1111), DS_SLOTS + 0x24: le32(0x24242424),
+                                DS_SLOTS + 0x41: b"\x80", DS_SLOTS + 0x42: b"\x00",
+                                DS_SLOTS + 0x2C: le32(0x2C2C2C2C), DS_SLOTS + 0x30: le32(0x30303030),
+                                DS_SLOTS + 0x34: le32(0x34343434), E3_REC + 0x18: le32(0x18181818),
+                                E3_REC + 0x1C: le32(0x1C1C1C1C), C2_ANCHOR: le32(0x1111) + le32(0x2222),
+                                C2_OFFS: le32(0x64) + le32(0x65)}),
+        Case("l2", {"eax": 0}, {**SLOT_PTRS, DS_SLOTS + 0x20: le32(0x9999), DS_SLOTS + 0x24: le32(0x24242424),
+                                DS_SLOTS + 0x41: b"\x00", DS_SLOTS + 0x42: b"\x00",
+                                DS_SLOTS + 0x2C: le32(0x2C2C2C2C), DS_SLOTS + 0x30: le32(0x30303030),
+                                DS_SLOTS + 0x34: le32(0x34343434), E3_REC + 0x18: le32(0x18181818),
+                                E3_REC + 0x1C: le32(0x1C1C1C1C), C2_ANCHOR: le32(0x1111) + le32(0x2222),
+                                C2_OFFS: le32(0x64) + le32(0x65)}),
+        Case("l3", {"eax": 1}, {**SLOT_PTRS, DS_SLOTS + 0x94 + 0x20: le32(0x24242424),
+                                DS_SLOTS + 0x94 + 0x41: b"\x80", DS_SLOTS + 0x94 + 0x42: b"\x08",
+                                DS_SLOTS + 0x94 + 0x2C: le32(0x3C3C3C3C), DS_SLOTS + 0x94 + 0x30: le32(0x40404040),
+                                DS_SLOTS + 0x94 + 0x34: le32(0x44444444), E3_REC2 + 0x18: le32(0x28282828),
+                                E3_REC2 + 0x1C: le32(0x2C2C2C2C), C2_ANCHOR: le32(0x1111) + le32(0x2222),
+                                C2_OFFS: le32(0x64) + le32(0x65)}),
+    ], calls=(C2_18540, C2_18350), eax_mask=0, mutants=("@mutant", "@side", "@anchor")),
+    # 0x18714 (record §C1.2): EAX = side; the record-x: bit 3 set takes rec+0x18, else the latch
+    # path returns slot+0x2C - DS_00100AB0[side*8]. Mask full (0x188DC stores the result).
+    Spec("hit_record_x", 0x18714, [
+        Case("x0", {"eax": 0}, {**SLOT_PTRS, DS_SLOTS + 0x20: le32(0x9999), DS_SLOTS + 0x41: b"\x00",
+                                DS_SLOTS + 0x42: b"\x08", DS_SLOTS + 0x2C: le32(0x2C2C2C2C),
+                                E3_REC + 0x18: le32(0x18181818), E3_REC + 0x1C: le32(0x1C1C1C1C),
+                                C2_ANCHOR: le32(0x1111) + le32(0x2222), C2_OFFS: le32(0x64) + le32(0x65)}),
+        Case("x1", {"eax": 0}, {**SLOT_PTRS, DS_SLOTS + 0x20: le32(0x1111), DS_SLOTS + 0x24: le32(0x24242424),
+                                DS_SLOTS + 0x41: b"\x00", DS_SLOTS + 0x42: b"\x00",
+                                DS_SLOTS + 0x2C: le32(0x2C2C2C2C), E3_REC + 0x18: le32(0x18181818),
+                                E3_REC + 0x1C: le32(0x1C1C1C1C), C2_ANCHOR: le32(0x1111) + le32(0x2222),
+                                C2_OFFS: le32(0x64) + le32(0x65)}),
+        Case("x2", {"eax": 0}, {**SLOT_PTRS, DS_SLOTS + 0x20: le32(0x9999), DS_SLOTS + 0x24: le32(0x24242424),
+                                DS_SLOTS + 0x41: b"\x00", DS_SLOTS + 0x42: b"\x00",
+                                DS_SLOTS + 0x2C: le32(0x2C2C2C2C), E3_REC + 0x18: le32(0x18181818),
+                                E3_REC + 0x1C: le32(0x1C1C1C1C), C2_ANCHOR: le32(0x1111) + le32(0x2222),
+                                C2_OFFS: le32(0x64) + le32(0x65)}),
+        Case("x3", {"eax": 1}, {**SLOT_PTRS, DS_SLOTS + 0x94 + 0x20: le32(0x9999), DS_SLOTS + 0x94 + 0x42: b"\x08",
+                                DS_SLOTS + 0x94 + 0x2C: le32(0x3C3C3C3C), E3_REC2 + 0x18: le32(0x28282828),
+                                E3_REC2 + 0x1C: le32(0x2C2C2C2C), C2_ANCHOR: le32(0x1111) + le32(0x2222),
+                                C2_OFFS: le32(0x64) + le32(0x65)}),
+    ], calls=(C2_18540, C2_18350), eax_mask=0xFFFFFFFF, mutants=("@mutant", "@side", "@anchor")),
+    # 0x189FC (record §C1.2): EAX = side; 0x33A10 builds the context (allow), 0x1A570's AL picks the
+    # comparison: clear -> slot[side]+0x2C < slot[other]+0x2C, set -> >. Mask 0xFF (`test al,al`).
+    Spec("fighter_189fc", 0x189FC, [
+        Case("z0", {"eax": 0}, {**SLOT_PTRS, DS_SLOTS + 0x2C: le32(0x100), DS_SLOTS + 0x94 + 0x2C: le32(0x200)},
+             {0x1A570: 1}),
+        Case("z1", {"eax": 0}, {**SLOT_PTRS, DS_SLOTS + 0x2C: le32(0x200), DS_SLOTS + 0x94 + 0x2C: le32(0x200)},
+             {0x1A570: 1}),
+        Case("z2", {"eax": 0}, {**SLOT_PTRS, DS_SLOTS + 0x2C: le32(0x300), DS_SLOTS + 0x94 + 0x2C: le32(0x200)},
+             {0x1A570: 1}),
+        Case("z3", {"eax": 0}, {**SLOT_PTRS, DS_SLOTS + 0x2C: le32(0x300), DS_SLOTS + 0x94 + 0x2C: le32(0x200)},
+             {0x1A570: 0}),
+        Case("z4", {"eax": 0}, {**SLOT_PTRS, DS_SLOTS + 0x2C: le32(0x100), DS_SLOTS + 0x94 + 0x2C: le32(0x200)},
+             {0x1A570: 0}),
+        Case("z5", {"eax": 0}, {**SLOT_PTRS, DS_SLOTS + 0x2C: le32(0x300), DS_SLOTS + 0x94 + 0x2C: le32(0x200)},
+             {0x1A570: 0x101}),
+        Case("z6", {"eax": 1}, {**SLOT_PTRS, DS_SLOTS + 0x2C: le32(0x300), DS_SLOTS + 0x94 + 0x2C: le32(0x100)},
+             {0x1A570: 1}),
+    ], allow_calls=(0x33A10,), calls=(BIT15,), eax_mask=0xFF, mutants=("@mutant", "@side", "@bit", "@al")),
+    # 0x18A4C (record §C1.2): EAX = side; 0x33950's context (allow); 0x1A570 runs real (its own
+    # row) so its two calls can disagree; 0x189FC is stubbed with a per-case AL. Mask 0xFF.
+    Spec("fighter_18a4c", 0x18A4C, [
+        Case("a0", {"eax": 0}, {**SLOT_PTRS, C2_ACTOR: le32(C2_PSET), E3_REC + 0x56: b"\x00\x00",
+                                E3_REC2 + 0x56: b"\x01\x00", C2_PSET: b"\x00\x80", C2_PSET + 0x20: b"\x00\x00"},
+             {0x189FC: 1}),
+        Case("a1", {"eax": 0}, {**SLOT_PTRS, C2_ACTOR: le32(C2_PSET), E3_REC + 0x56: b"\x00\x00",
+                                E3_REC2 + 0x56: b"\x01\x00", C2_PSET: b"\x00\x80", C2_PSET + 0x20: b"\x00\x80"},
+             {0x189FC: 1}),
+        Case("a2", {"eax": 0}, {**SLOT_PTRS, C2_ACTOR: le32(C2_PSET), E3_REC + 0x56: b"\x00\x00",
+                                E3_REC2 + 0x56: b"\x01\x00", C2_PSET: b"\x00\x80", C2_PSET + 0x20: b"\x00\x00"},
+             {0x189FC: 0}),
+        Case("a3", {"eax": 0}, {**SLOT_PTRS, C2_ACTOR: le32(C2_PSET), E3_REC + 0x56: b"\x00\x00",
+                                E3_REC2 + 0x56: b"\x01\x00", C2_PSET: b"\x00\x00", C2_PSET + 0x20: b"\x00\x80"},
+             {0x189FC: 1}),
+        Case("a4", {"eax": 0}, {**SLOT_PTRS, C2_ACTOR: le32(C2_PSET), E3_REC + 0x56: b"\x00\x00",
+                                E3_REC2 + 0x56: b"\x01\x00", C2_PSET: b"\x00\x00", C2_PSET + 0x20: b"\x00\x00"},
+             {0x189FC: 1}),
+        Case("a5", {"eax": 1}, {**SLOT_PTRS, C2_ACTOR: le32(C2_PSET), E3_REC + 0x56: b"\x00\x00",
+                                E3_REC2 + 0x56: b"\x01\x00", C2_PSET: b"\x00\x00", C2_PSET + 0x20: b"\x00\x80"},
+             {0x189FC: 1}),
+    ], allow_calls=(0x33950,), calls=(C2_189FC, E.Call(0x1A570, ("eax",), mode="real")),
+       eax_mask=0xFF, mutants=("@mutant", "@side", "@arg")),
+    # 0x39EFC (record §P6.2): EAX = side; 0x33A10 allow; 1 only when the side's slot +0x53 == 0x0A,
+    # +0x10 == 0x39CC8 and +0x58 == 4. Mask 0xFF (`test al,al` at 0x3B797).
+    Spec("fighter_39efc", 0x39EFC, [
+        Case("e0", {"eax": 0}, {**SLOT_PTRS, DS_SLOTS + 0x53: b"\x0a", DS_SLOTS + 0x10: le32(0x39CC8),
+                                DS_SLOTS + 0x58: b"\x04"}),
+        Case("e1", {"eax": 0}, {**SLOT_PTRS, DS_SLOTS + 0x53: b"\x0b", DS_SLOTS + 0x10: le32(0x39CC8),
+                                DS_SLOTS + 0x58: b"\x04"}),
+        Case("e2", {"eax": 0}, {**SLOT_PTRS, DS_SLOTS + 0x53: b"\x0a", DS_SLOTS + 0x10: le32(0x39CC0),
+                                DS_SLOTS + 0x58: b"\x04"}),
+        Case("e3", {"eax": 0}, {**SLOT_PTRS, DS_SLOTS + 0x53: b"\x0a", DS_SLOTS + 0x10: le32(0x39CC8),
+                                DS_SLOTS + 0x58: b"\x05"}),
+        Case("e4", {"eax": 1}, {**SLOT_PTRS, DS_SLOTS + 0x94 + 0x53: b"\x0a", DS_SLOTS + 0x94 + 0x10: le32(0x39CC8),
+                                DS_SLOTS + 0x94 + 0x58: b"\x04"}),
+    ], allow_calls=(0x33A10,), eax_mask=0xFF, mutants=("@mutant", "@val", "@side")),
+    # 0x3B8D8 (record §C1.2): EAX = side, EDX = delta; 1 when slot+0x2C + delta >= wall or <= -wall.
+    # Mask 0xFF (`test al,al` at 0x3C2BF/0x3C2EF).
+    Spec("fighter_3b8d8", 0x3B8D8, [
+        Case("b0", {"eax": 0, "edx": 0x100}, {**SLOT_PTRS, DS_SLOTS + 0x2C: le32(0x1000), DS_BE018: le32(0x2000)}),
+        Case("b1", {"eax": 0, "edx": 0x100}, {**SLOT_PTRS, DS_SLOTS + 0x2C: le32(0x1F00), DS_BE018: le32(0x2000)}),
+        Case("b2", {"eax": 0, "edx": 0xFFFFE000}, {**SLOT_PTRS, DS_SLOTS + 0x2C: le32(0x1000), DS_BE018: le32(0x2000)}),
+        Case("b3", {"eax": 0, "edx": 0xFFFFD000}, {**SLOT_PTRS, DS_SLOTS + 0x2C: le32(0x1000), DS_BE018: le32(0x2000)}),
+        Case("b4", {"eax": 1, "edx": 0x100}, {**SLOT_PTRS, DS_SLOTS + 0x94 + 0x2C: le32(0x1F00), DS_BE018: le32(0x2000)}),
+        Case("b5", {"eax": 0, "edx": 0}, {**SLOT_PTRS, DS_SLOTS + 0x2C: le32(0), DS_BE018: le32(0x80000000)}),
+    ], eax_mask=0xFF, mutants=("@mutant", "@side", "@bound")),
+    # 0x3B90C (record §C1.2): EAX = side, EDX = delta; the sum clamped to +/-wall. Mask full
+    # (0x188DC stores the result as x).
+    Spec("fighter_3b90c", 0x3B90C, [
+        Case("c0", {"eax": 0, "edx": 0x100}, {**SLOT_PTRS, DS_SLOTS + 0x2C: le32(0x1000), DS_BE018: le32(0x2000)}),
+        Case("c1", {"eax": 0, "edx": 0x100}, {**SLOT_PTRS, DS_SLOTS + 0x2C: le32(0x1F00), DS_BE018: le32(0x2000)}),
+        Case("c2", {"eax": 0, "edx": 0xFFFFD000}, {**SLOT_PTRS, DS_SLOTS + 0x2C: le32(0x1000), DS_BE018: le32(0x2000)}),
+        Case("c3", {"eax": 0, "edx": 0xFFFFE000}, {**SLOT_PTRS, DS_SLOTS + 0x2C: le32(0x1000), DS_BE018: le32(0x2000)}),
+        Case("c4", {"eax": 1, "edx": 0x100}, {**SLOT_PTRS, DS_SLOTS + 0x94 + 0x2C: le32(0x1000), DS_BE018: le32(0x2000)}),
+        Case("c5", {"eax": 0, "edx": 0}, {**SLOT_PTRS, DS_SLOTS + 0x2C: le32(0x80000000), DS_BE018: le32(0x80000000)}),
+    ], eax_mask=0xFFFFFFFF, mutants=("@mutant", "@side", "@bound")),
+    # 0x33A10 (record §E3.6): EAX = the six-dword out buffer, EDX = side; out[0]=1-side, out[1]=side,
+    # out[2]=&slot[1-side], out[3]=&slot[side], out[4]=rec_other, out[5]=rec_self. Mask 0.
+    Spec("fighter_ctx_swap", 0x33A10, [
+        Case("w0", {"eax": E3_OUT, "edx": 0}, {**SLOT_PTRS, E3_OUT: b"\xaa" * 24}),
+        Case("w1", {"eax": E3_OUT, "edx": 1}, {**SLOT_PTRS, E3_OUT: b"\xaa" * 24}),
+    ], eax_mask=0, mutants=("@mutant", "@slot", "@rec")),
+    # 0x1883C (record §C1.2): EAX = side, EDX = a, EBX = b; latch both slots, add (a, b) to the
+    # side's +0x2C/+0x30, then rec+0x18/+0x1C from 0x18714/0x18788 (stubbed with a per-case EAX).
+    Spec("fighter_1883c", 0x1883C, [
+        Case("c0", {"eax": 0, "edx": 0x10, "ebx": 0x20},
+             {**SLOT_PTRS, DS_SLOTS + 0x2C: le32(0x100), DS_SLOTS + 0x30: le32(0x200),
+              DS_SLOTS + 0x94 + 0x2C: le32(0x300), DS_SLOTS + 0x94 + 0x30: le32(0x400),
+              E3_REC + 0x18: le32(0x18181818), E3_REC + 0x1C: le32(0x1C1C1C1C)},
+             {0x18714: 0x1111, 0x18788: 0x2222}),
+        Case("c1", {"eax": 1, "edx": 0x10, "ebx": 0x20},
+             {**SLOT_PTRS, DS_SLOTS + 0x2C: le32(0x100), DS_SLOTS + 0x30: le32(0x200),
+              DS_SLOTS + 0x94 + 0x2C: le32(0x300), DS_SLOTS + 0x94 + 0x30: le32(0x400),
+              E3_REC2 + 0x18: le32(0x28282828), E3_REC2 + 0x1C: le32(0x2C2C2C2C)},
+             {0x18714: 0x3333, 0x18788: 0x4444}),
+        Case("c2", {"eax": 0, "edx": 0xFFFFFFF0, "ebx": 0xFFFFFFE0},
+             {**SLOT_PTRS, DS_SLOTS + 0x2C: le32(0x100), DS_SLOTS + 0x30: le32(0x200),
+              DS_SLOTS + 0x94 + 0x2C: le32(0x300), DS_SLOTS + 0x94 + 0x30: le32(0x400),
+              E3_REC + 0x18: le32(0x18181818), E3_REC + 0x1C: le32(0x1C1C1C1C)},
+             {0x18714: 0x5555, 0x18788: 0x6666}),
+    ], calls=(C2_LATCH, E.Call(0x18714, ("eax",)), E.Call(0x18788, ("eax",))),
+       eax_mask=0, mutants=("@mutant", "@side", "@latch", "@arg")),
+    # 0x18B04 (record §C1.2): EAX = side; mode 0x104B00 == 0x22 returns; else the +0x29 bit 0x40
+    # by the two slots' +0x2C, then rec+0x18 = 0x18714(side) (stubbed). Mask 0.
+    Spec("hit_facing_flag", 0x18B04, [
+        Case("m0", {"eax": 0}, {DS_104B00: b"\x22\x00", **SLOT_PTRS, DS_SLOTS + 0x2C: le32(0x100),
+                               DS_SLOTS + 0x94 + 0x2C: le32(0x200), E3_REC + 0x29: b"\x29",
+                               E3_REC + 0x18: le32(0x18181818)}, {0x18714: 0x1111}),
+        Case("m1", {"eax": 0}, {DS_104B00: b"\x00\x00", **SLOT_PTRS, DS_SLOTS + 0x2C: le32(0x100),
+                               DS_SLOTS + 0x94 + 0x2C: le32(0x200), E3_REC + 0x29: b"\x00",
+                               E3_REC + 0x18: le32(0x18181818)}, {0x18714: 0x2222}),
+        Case("m2", {"eax": 0}, {DS_104B00: b"\x00\x00", **SLOT_PTRS, DS_SLOTS + 0x2C: le32(0x200),
+                               DS_SLOTS + 0x94 + 0x2C: le32(0x200), E3_REC + 0x29: b"\xff",
+                               E3_REC + 0x18: le32(0x18181818)}, {0x18714: 0x3333}),
+        Case("m3", {"eax": 1}, {DS_104B00: b"\x00\x00", **SLOT_PTRS, DS_SLOTS + 0x2C: le32(0x100),
+                               DS_SLOTS + 0x94 + 0x2C: le32(0x200), E3_REC2 + 0x29: b"\x00",
+                               E3_REC2 + 0x18: le32(0x28282828)}, {0x18714: 0x4444}),
+    ], allow_calls=(0x33950,), calls=(E.Call(0x18714, ("eax",)),), eax_mask=0,
+       mutants=("@mutant", "@mode", "@side", "@store")),
+    # 0x18B44 (record §P6.2): EAX = slot; once per fight (DS_00100C1D) and not when +0x63 == 1,
+    # the two dust spawns (0x2AE14 stub) at the 0xA17D4/0xA17D6-derived x/y, layers 0xFE/0xFF.
+    Spec("fighter_18b44", 0x18B44, [
+        Case("d0", {"eax": E3_SLOT}, {E3_SLOT + 0x63: b"\x01", DS_100C1D: b"\x00", DS_104529: b"\x02",
+                                      FIGHTER_A17D4: le32(0x00110022), FIGHTER_A17D6: le32(0x00330044)}),
+        Case("d1", {"eax": E3_SLOT}, {E3_SLOT + 0x63: b"\x00", DS_100C1D: b"\xff", DS_104529: b"\x02",
+                                      FIGHTER_A17D4: le32(0x00110022), FIGHTER_A17D6: le32(0x00330044)}),
+        Case("d2", {"eax": E3_SLOT}, {E3_SLOT + 0x63: b"\x00", DS_100C1D: b"\x00", DS_104529: b"\x02",
+                                      FIGHTER_A17D4: le32(0x00110022), FIGHTER_A17D6: le32(0x00330044)}),
+        Case("d3", {"eax": E3_SLOT}, {E3_SLOT + 0x63: b"\x00", DS_100C1D: b"\x00", DS_104529: b"\x01",
+                                      FIGHTER_A17D4: le32(0x00110022), FIGHTER_A17D6: le32(0x00330044)}),
+    ], calls=(SPAWN,), eax_mask=0, mutants=("@mutant", "@latch", "@layer")),
+    # 0x1DDF4 (record §6.7): EAX = side, EDX = table, EBX = idx; |0x187FC| <= DSB(table+char)<<6
+    # and |0x1881C| <= DSB(idx+char)<<6 (both stubbed with per-case EAX). Mask 0xFF (`test al,al`).
+    Spec("hit_geometry", 0x1DDF4, [
+        Case("g0", {"eax": 0, "edx": C2_ROW0, "ebx": C2_ROW1},
+             {**SLOT_PTRS, DS_SLOTS + 0x7A: b"\x01", C2_ROW0 + 1: b"\x10", C2_ROW1 + 1: b"\x20"},
+             {0x187FC: 0x300, 0x1881C: 0x700}),
+        Case("g1", {"eax": 0, "edx": C2_ROW0, "ebx": C2_ROW1},
+             {**SLOT_PTRS, DS_SLOTS + 0x7A: b"\x01", C2_ROW0 + 1: b"\x10", C2_ROW1 + 1: b"\x20"},
+             {0x187FC: 0x500, 0x1881C: 0x700}),
+        Case("g2", {"eax": 0, "edx": C2_ROW0, "ebx": C2_ROW1},
+             {**SLOT_PTRS, DS_SLOTS + 0x7A: b"\x01", C2_ROW0 + 1: b"\x10", C2_ROW1 + 1: b"\x20"},
+             {0x187FC: 0x300, 0x1881C: 0x900}),
+        Case("g3", {"eax": 0, "edx": C2_ROW0, "ebx": C2_ROW1},
+             {**SLOT_PTRS, DS_SLOTS + 0x7A: b"\x01", C2_ROW0 + 1: b"\x10", C2_ROW1 + 1: b"\x20"},
+             {0x187FC: 0xFFFFFB00, 0x1881C: 0x700}),
+        Case("g4", {"eax": 0, "edx": C2_ROW0, "ebx": C2_ROW1},
+             {**SLOT_PTRS, DS_SLOTS + 0x7A: b"\x01", C2_ROW0 + 1: b"\x10", C2_ROW1 + 1: b"\x20"},
+             {0x187FC: 0x400, 0x1881C: 0x800}),
+        Case("g5", {"eax": 1, "edx": C2_ROW0, "ebx": C2_ROW1},
+             {**SLOT_PTRS, DS_SLOTS + 0x94 + 0x7A: b"\x03", C2_ROW0 + 3: b"\x40", C2_ROW1 + 3: b"\x10"},
+             {0x187FC: 0x1000, 0x1881C: 0x400}),
+        Case("g6", {"eax": 0, "edx": C2_ROW0, "ebx": C2_ROW1},
+             {**SLOT_PTRS, DS_SLOTS + 0x7A: b"\x01", C2_ROW0 + 1: b"\x10", C2_ROW1 + 1: b"\x20"},
+             {0x187FC: 0x300, 0x1881C: 0xFFFFF900}),
+    ], calls=(E.Call(0x187FC, ()), E.Call(0x1881C, ())), eax_mask=0xFF,
+       mutants=("@mutant", "@side", "@abs", "@table")),
+    # 0x39F40 (record §P6.2): EAX = side, EDX/EBX/ECX/s0 = the four pose words; 0x33A10 allow;
+    # seeds DS_00107A60/68/70/78[side], the slot's +0x52/53/54/10/58 and the record's +0x24.
+    Spec("fighter_pose_start", 0x39F40, [
+        Case("p0", {"eax": 0, "edx": 0x11111111, "ebx": 0x22222222, "ecx": 0x33333333, "s0": 0x44444444},
+             {**SLOT_PTRS, DS_SLOTS + 0x10: le32(0x10101010), DS_SLOTS + 0x52: b"\x52\x53\x54\x55",
+              DS_SLOTS + 0x58: b"\x58", E3_REC + 0x24: le32(0x24242424), E3_REC2 + 0x24: le32(0x34343434),
+              0x107A60: bytes(range(0x60, 0x80))}),
+        Case("p1", {"eax": 1, "edx": 0x55555555, "ebx": 0x66666666, "ecx": 0x77777777, "s0": 0x88888888},
+             {**SLOT_PTRS, DS_SLOTS + 0x94 + 0x10: le32(0x20202020),
+              DS_SLOTS + 0x94 + 0x52: b"\x62\x63\x64\x65", DS_SLOTS + 0x94 + 0x58: b"\x68",
+              E3_REC + 0x24: le32(0x24242424), E3_REC2 + 0x24: le32(0x34343434),
+              0x107A60: bytes(range(0x60, 0x80))}),
+    ], allow_calls=(0x33A10,), eax_mask=0, mutants=("@mutant", "@side", "@arg", "@field")),
+    # 0x2BCF4 (record §P6.2): EAX = rec, EDX = stream; rec+8 = stream, rec+0x28 &= ~0x14, the pset
+    # word = 0x2A408(rec, pset)'s low word (stubbed with a per-case EAX). Mask 0.
+    Spec("actors_anim_seek", 0x2BCF4, [
+        Case("n0", {"eax": E3_REC, "edx": 0x10A700}, {DS_1014EC: le32(C2_PSET), E3_REC + 8: le32(0x08080808),
+                                                      E3_REC + 0x28: b"\xff\xff", E3_REC + 0x56: b"\x01\x00",
+                                                      C2_PSET + 0x20: b"\x02\x02", C2_PSET + 0x22: b"\x03\x03"},
+             {0x2A408: 0x1234}),
+        Case("n1", {"eax": E3_REC, "edx": 0x10A700}, {DS_1014EC: le32(C2_PSET), E3_REC + 8: le32(0x08080808),
+                                                      E3_REC + 0x28: b"\x00\x00", E3_REC + 0x56: b"\x00\x00",
+                                                      C2_PSET: b"\x02\x02", C2_PSET + 2: b"\x03\x03"},
+             {0x2A408: 0x5678}),
+    ], calls=(C1_SPRITE_ID,), eax_mask=0, mutants=("@mutant", "@char", "@width")),
+    # 0x37D18 (record §P6.2): EAX = slot, EDX = rec; the dispatch state, the 0xC9238[char]
+    # animation, 0x39A10(rec, 0x309), the 0xBDAD4[char] voice, the 0x1078DC approach pointer and
+    # the other slot's actor +0x59/+0x40. Mask 0.
+    Spec("fighter_37d18", 0x37D18, [
+        Case("r0", {"eax": DS_SLOTS, "edx": E3_REC}, {**P6_PTRS, **SLOT_PTRS, DS_SLOTS + 0x42: b"\x42",
+             DS_SLOTS + 0x7A: b"\x01", E3_REC + 0x51: b"\x00", 0x000C9238 + 4: le32(0x000E1234),
+             0x000BDAD4 + 2: b"\x55\x66", DS_SLOTS + 0x94 + 0x40: b"\x40\x41\x42\x43",
+             E3_REC2 + 0x59: b"\x59", 0x1078DC: b"\xdc\xdc\xdc\xdc"}),
+        Case("r1", {"eax": DS_SLOTS, "edx": E3_REC2}, {DS_SLOTS - 8: le32(DS_SLOTS + 0x94) + le32(0),
+             **SLOT_PTRS, DS_SLOTS + 0x94 + 0x42: b"\x52", DS_SLOTS + 0x94 + 0x7A: b"\x03",
+             E3_REC2 + 0x51: b"\x01", 0x000C9238 + 12: le32(0x000E5678), 0x000BDAD4 + 6: b"\x77\x88",
+             DS_SLOTS + 0x40: b"\x60\x61\x62\x63", E3_REC + 0x59: b"\x69", 0x1078DC: b"\xdc\xdc\xdc\xdc"}),
+    ], calls=(ANIM_BEGIN, E.Call(0x39A10, ("eax", "edx"), clobbers=("edx",)), VOICE), eax_mask=0,
+       mutants=("@mutant", "@state", "@order")),
+    # 0x3AA54 (record §P6.2): EAX = slot; the reaction-0x11 pose seed from the 0xBEC88/0xBECA4/
+    # 0xBECC0/0xBECDC[char] tables, slot+0x52 = 0x11, then 0x1A5AC(rec+0x51) (stubbed) negates
+    # rec+0x34 when 0. Mask 0 (the callers ignore the returned 0x11).
+    Spec("fighter_3aa54", 0x3AA54, [
+        Case("a0", {"eax": DS_SLOTS}, {**SLOT_PTRS, DS_SLOTS + 0x7A: b"\x01", E3_REC + 0x24: le32(0x24242424),
+             E3_REC + 0x28: b"\x28\x28", E3_REC + 0x34: b"\x34\x34", E3_REC + 0x51: b"\x00",
+             E3_REC + 0x43: b"\x43", DS_SLOTS + 0x10: le32(0x10101010), DS_SLOTS + 0x41: b"\x41",
+             DS_SLOTS + 0x52: b"\x52\x53\x54\x55", DS_SLOTS + 0x58: b"\x58", 0xBEC88 + 4: b"\x88\x88",
+             0xBECA4 + 4: b"\xa4\xa4", 0xBECC0 + 4: b"\xc0\xc0", 0xBECDC + 4: b"\xdc"}, {0x1A5AC: 0}),
+        Case("a1", {"eax": DS_SLOTS}, {**SLOT_PTRS, DS_SLOTS + 0x7A: b"\x01", E3_REC + 0x24: le32(0x24242424),
+             E3_REC + 0x28: b"\x28\x28", E3_REC + 0x34: b"\x34\x34", E3_REC + 0x51: b"\x00",
+             E3_REC + 0x43: b"\x43", DS_SLOTS + 0x10: le32(0x10101010), DS_SLOTS + 0x41: b"\x41",
+             DS_SLOTS + 0x52: b"\x52\x53\x54\x55", DS_SLOTS + 0x58: b"\x58", 0xBEC88 + 4: b"\x88\x88",
+             0xBECA4 + 4: b"\xa4\xa4", 0xBECC0 + 4: b"\xc0\xc0", 0xBECDC + 4: b"\xdc"}, {0x1A5AC: 1}),
+        Case("a2", {"eax": DS_SLOTS}, {**SLOT_PTRS, DS_SLOTS + 0x7A: b"\x02", E3_REC + 0x24: le32(0x24242424),
+             E3_REC + 0x28: b"\x28\x28", E3_REC + 0x34: b"\x34\x34", E3_REC + 0x51: b"\x01",
+             E3_REC + 0x43: b"\x43", DS_SLOTS + 0x10: le32(0x10101010), DS_SLOTS + 0x41: b"\x41",
+             DS_SLOTS + 0x52: b"\x52\x53\x54\x55", DS_SLOTS + 0x58: b"\x58", 0xBEC88 + 8: b"\x99\x99",
+             0xBECA4 + 8: b"\xb5\xb5", 0xBECC0 + 8: b"\xd1\xd1", 0xBECDC + 8: b"\xdd"}, {0x1A5AC: 0}),
+    ], calls=(E.Call(0x1A5AC, ("eax",)),), eax_mask=0, mutants=("@mutant", "@char", "@field")),
+    # 0x2AD40 (record §P4.5): EAX = rec, EDX = pset; the child's dead bit + 0x2B150(child), the
+    # parent refcount, 0x249D0/0x249B0, the pset reset and 0x2B150(rec). 0x2EA30 is the interrupt
+    # lock the port drops as inert (record §47-C): allowed, and 0xBCD60 = 0 makes it a no-op.
+    Spec("release_record", 0x2AD40, [
+        Case("r0", {"eax": C2_REC, "edx": C2_PSET}, {DS_1014F4: le32(C2_POOL), C2_REC: le32(0x105B3C) + le32(0x105B3C), C2_REC + 0x2A: b"\x00\x00",
+             C2_REC + 0x4F: b"\x00", C2_REC + 0x4B: b"\x01", C2_REC + 0x4A: b"\x02",
+             C2_REC + 0x28: b"\x28\x04", C2_REC + 0x68 + 0x2A: b"\xff", C2_PSET + 4: le32(0x04040404),
+             C2_PSET + 8: le32(0x08080808), C2_PSET + 0x0C: b"\x0c\x0c\x0e\x0e",
+             0x105B3C: le32(0x105B3C), 0xBCD60: b"\x00"}),
+        Case("r1", {"eax": C2_REC, "edx": C2_PSET}, {DS_1014F4: le32(C2_POOL), C2_REC + 0x2A: b"\x00\x00",
+             C2_REC + 0x4F: b"\x00", C2_REC + 0x4B: b"\x00", C2_REC + 0x4A: b"\x02",
+             C2_REC + 0x28: b"\x28\x04", C2_PSET + 4: le32(0x04040404), C2_PSET + 8: le32(0x08080808),
+             C2_PSET + 0x0C: b"\x0c\x0c\x0e\x0e", 0x105B3C: le32(0x105B3C), 0xBCD60: b"\x00"}),
+        Case("r2", {"eax": C2_REC, "edx": C2_PSET}, {DS_1014F4: le32(C2_POOL), C2_REC + 0x2A: b"\x08\x00",
+             C2_REC + 0x4F: b"\x00", C2_REC + 0x4B: b"\x01", C2_REC + 0x4A: b"\x02",
+             C2_REC + 0x28: b"\x28\x04", C2_REC + 0x68 + 0x2A: b"\xff", C2_PSET + 4: le32(0x04040404),
+             C2_PSET + 8: le32(0x08080808), C2_PSET + 0x0C: b"\x0c\x0c\x0e\x0e",
+             0x105B3C: le32(0x105B3C), 0xBCD60: b"\x00"}),
+        Case("r3", {"eax": C2_REC, "edx": C2_PSET}, {DS_1014F4: le32(C2_POOL), C2_REC + 0x2A: b"\x00\x00",
+             C2_REC + 0x4F: b"\x01", C2_REC + 0x4B: b"\x01", C2_REC + 0x4A: b"\x02",
+             C2_REC + 0x28: b"\x28\x04", C2_REC + 0x68 + 0x2A: b"\xff", C2_PSET + 4: le32(0x04040404),
+             C2_PSET + 8: le32(0x08080808), C2_PSET + 0x0C: b"\x0c\x0c\x0e\x0e",
+             0x105B3C: le32(0x105B3C), 0xBCD60: b"\x00"}),
+    ], allow_calls=(0x2EA30,), calls=(E.Call(0x2B150, ("eax",), clobbers=("esi", "edi", "ebp")), E.Call(0x249D0, ("eax",)),
+                                     E.Call(0x249B0, ("eax", "edx"))),
+       eax_mask=0, mutants=("@mutant", "@field", "@latch")),
+    # 0x2A408 (record §P6.2): EAX = rec, EDX = pset; the sprite-id reader: +0x28 bit 8 keeps
+    # DSW(rec+8), else the stream word's bit 15/op 0xD00 arms, then the bit-0x8000 flip by +0x28
+    # bit 6. 0x29F34 (stub, per-case EAX) supplies the variable. Mask 0xFFFF (the callers store AX).
+    Spec("anim_next_sprite_id", 0x2A408, [
+        Case("s0", {"eax": E3_REC, "edx": C2_PSET}, {E3_REC + 8: le32(0x00109234), E3_REC + 0x28: b"\x00\x08",
+                                                     C2_PSET: b"\x34\x92"}, {0x29F34: 0}),
+        Case("s1", {"eax": E3_REC, "edx": C2_PSET}, {E3_REC + 8: le32(C2_STR), E3_REC + 0x28: b"\x00\x40",
+                                                     C2_STR: b"\x34\x12", C2_PSET: b"\x34\x92"}, {0x29F34: 0}),
+        Case("s2", {"eax": E3_REC, "edx": C2_PSET}, {E3_REC + 8: le32(C2_STR), E3_REC + 0x28: b"\x00\x00",
+                                                     C2_STR: b"\x05\x80", C2_PSET: b"\x34\x92"}, {0x29F34: 0}),
+        Case("s3", {"eax": E3_REC, "edx": C2_PSET}, {E3_REC + 8: le32(C2_STR), E3_REC + 0x28: b"\x00\x00",
+                                                     C2_STR: b"\x45\xcd", C2_STR + 2: b"\x00\x01",
+                                                     C2_PSET: b"\x34\x92"}, {0x29F34: 0x10}),
+        Case("s4", {"eax": E3_REC, "edx": C2_PSET}, {E3_REC + 8: le32(C2_STR), E3_REC + 0x28: b"\x00\x40",
+                                                     C2_STR: b"\x25\xad", C2_STR + 2: le32(C2_ROW0),
+                                                     C2_ROW0 + 6: b"\x22\x02", C2_PSET: b"\x34\x92"}, {0x29F34: 3}),
+        Case("s5", {"eax": E3_REC, "edx": C2_PSET}, {E3_REC + 8: le32(0x00109234), E3_REC + 0x28: b"\x40\x08",
+                                                     C2_PSET: b"\x34\x92"}, {0x29F34: 0}),
+    ], calls=(E.Call(0x29F34, ("eax", "edx"), clobbers=("edx",)),), eax_mask=0xFFFF,
+       mutants=("@mutant", "@bit8", "@op", "@var", "@clear")),
+    # 0x36638 (record §P6.2): EAX = slot, EDX = rec; the +0x54 dispatch: 1/default restart on
+    # 0xC9210/0xC91E8[char] with +0x52 = 0x12, 4 on 0xC91E8 with +0x52 = 8/+0x57 = 2, 5 runs
+    # 0x38154(rec+0x51); +0x54 == 3 clears +0x43 bit 0x40 and returns. Mask 0xFF.
+    Spec("fighter_state_36638", 0x36638, [
+        Case("s0", {"eax": DS_SLOTS, "edx": E3_REC}, {**SLOT_PTRS, DS_SLOTS + 0x43: b"\x40",
+             DS_SLOTS + 0x54: b"\x03", DS_SLOTS + 0x7A: b"\x01", DS_SLOTS + 0x52: b"\x52",
+             DS_SLOTS + 0x53: b"\x53", DS_SLOTS + 0x57: b"\x57", DS_104B00: b"\x25\x00"}),
+        Case("s1", {"eax": DS_SLOTS, "edx": E3_REC}, {**SLOT_PTRS, DS_SLOTS + 0x43: b"\x00",
+             DS_SLOTS + 0x54: b"\x01", DS_SLOTS + 0x7A: b"\x01", DS_SLOTS + 0x52: b"\x52",
+             DS_SLOTS + 0x53: b"\x53", DS_SLOTS + 0x57: b"\x57", DS_104B00: b"\x00\x00"}),
+        Case("s2", {"eax": DS_SLOTS, "edx": E3_REC}, {**SLOT_PTRS, DS_SLOTS + 0x40: b"\x40\x41\x42\x43",
+             DS_SLOTS + 0x43: b"\x40", DS_SLOTS + 0x54: b"\x01", DS_SLOTS + 0x7A: b"\x01",
+             DS_SLOTS + 0x52: b"\x52", DS_SLOTS + 0x53: b"\x53", DS_SLOTS + 0x57: b"\x57",
+             0x000C9210 + 4: le32(0x000E1111), DS_104B00: b"\x00\x00"}),
+        Case("s3", {"eax": DS_SLOTS, "edx": E3_REC}, {**SLOT_PTRS, DS_SLOTS + 0x40: b"\x40\x41\x42\x43",
+             DS_SLOTS + 0x43: b"\x40", DS_SLOTS + 0x54: b"\x04", DS_SLOTS + 0x7A: b"\x01",
+             DS_SLOTS + 0x52: b"\x52", DS_SLOTS + 0x53: b"\x53", DS_SLOTS + 0x57: b"\x57",
+             0x000C91E8 + 4: le32(0x000E2222), DS_104B00: b"\x00\x00"}),
+        Case("s4", {"eax": DS_SLOTS, "edx": E3_REC}, {**SLOT_PTRS, DS_SLOTS + 0x40: b"\x40\x41\x42\x43",
+             DS_SLOTS + 0x43: b"\x40", DS_SLOTS + 0x54: b"\x05", DS_SLOTS + 0x7A: b"\x01",
+             DS_SLOTS + 0x52: b"\x52", DS_SLOTS + 0x53: b"\x53", DS_SLOTS + 0x57: b"\x57",
+             E3_REC + 0x51: b"\x01", DS_104B00: b"\x00\x00"}),
+        Case("s5", {"eax": DS_SLOTS, "edx": E3_REC}, {**SLOT_PTRS, DS_SLOTS + 0x40: b"\x40\x41\x42\x43",
+             DS_SLOTS + 0x43: b"\x40", DS_SLOTS + 0x54: b"\x00", DS_SLOTS + 0x7A: b"\x01",
+             DS_SLOTS + 0x52: b"\x52", DS_SLOTS + 0x53: b"\x53", DS_SLOTS + 0x57: b"\x57",
+             0x000C91E8 + 4: le32(0x000E3333), DS_104B00: b"\x00\x00"}),
+        Case("s6", {"eax": DS_SLOTS, "edx": E3_REC}, {**SLOT_PTRS, DS_SLOTS + 0x40: b"\x40\x41\x42\x43",
+             DS_SLOTS + 0x43: b"\x40", DS_SLOTS + 0x54: b"\x01", DS_SLOTS + 0x7A: b"\x01",
+             DS_SLOTS + 0x52: b"\x52", DS_SLOTS + 0x53: b"\x53", DS_SLOTS + 0x57: b"\x57",
+             0x000C9210 + 4: le32(0x000E4444), DS_104B00: b"\x25\x00"}),
+    ], calls=(ANIM_BEGIN, E.Call(0x38154, ("eax",))), eax_mask=0xFF,
+       mutants=("@mutant", "@anim", "@mode", "@side")),
+]
+
 SPECS = [
     Spec("rng_next", 0x5D7DC, [
         Case("r1", {"eax": 0x1234}, {DS_RNG: le32(0x12345678)}),
@@ -2653,7 +3132,7 @@ SPECS = [
         Case("d0", {}, {DS_1078FC: b"\x00"}),
         Case("d1", {"eax": U6_REC}, {DS_1078FC: b"\x5a"}),
     ], eax_mask=0),
-] + E3_SPECS + P1_SPECS + P1_ANIM_SPECS + P2_SPECS + P3_SPECS + P6_SPECS + P45_SPECS + C1_SPECS
+] + E3_SPECS + P1_SPECS + P1_ANIM_SPECS + P2_SPECS + P3_SPECS + P6_SPECS + P45_SPECS + C1_SPECS + C2_SPECS
 
 
 # ---- driver -------------------------------------------------------------------------------------
diff --git a/tools/tests/test_diff_verify.py b/tools/tests/test_diff_verify.py
index b5f9c08..1d9facd 100644
--- a/tools/tests/test_diff_verify.py
+++ b/tools/tests/test_diff_verify.py
@@ -587,6 +587,127 @@ C1_KINDS = {
     "fighter_18c14@live": {"byte", "call #0", "call #1", "eax"},
 }
 
+# Track P batch C2 (record 2026-10-04-reverse-c2): the verification-only callee rows with their EAX
+# masks, and what alone catches each of their mutants.
+C2_MASKS = {
+            "fighter_state_39280": 0x0,
+            "palette_release": 0x0,
+            "fighter_13244": 0x0,
+            "fighter_29c08": 0xffffffff,
+            "actor_pset_flag_5f": 0x0,
+            "fighter_29bc8": 0x0,
+            "hit_anchor_y": 0x0,
+            "ai_distance": 0xffffffff,
+            "fighter_slot_latch": 0x0,
+            "hit_record_x": 0xffffffff,
+            "fighter_189fc": 0xff,
+            "fighter_18a4c": 0xff,
+            "fighter_39efc": 0xff,
+            "fighter_3b8d8": 0xff,
+            "fighter_3b90c": 0xffffffff,
+            "fighter_ctx_swap": 0x0,
+            "fighter_1883c": 0x0,
+            "hit_facing_flag": 0x0,
+            "fighter_18b44": 0x0,
+            "hit_geometry": 0xff,
+            "fighter_pose_start": 0x0,
+            "actors_anim_seek": 0x0,
+            "fighter_37d18": 0x0,
+            "fighter_3aa54": 0x0,
+            "release_record": 0x0,
+            "anim_next_sprite_id": 0xffff,
+            "fighter_state_36638": 0xff,
+}
+C2_KINDS = {
+    "fighter_state_39280@mutant": {"byte"},
+    "fighter_state_39280@side": {"byte"},
+    "fighter_state_39280@width": {"byte"},
+    "palette_release@mutant": {"byte"},
+    "palette_release@width": {"byte"},
+    "palette_release@off": {"byte"},
+    "fighter_13244@mutant": {"byte"},
+    "fighter_13244@addr": {"byte"},
+    "fighter_13244@val": {"byte"},
+    "fighter_29c08@side": {"eax"},
+    "fighter_29c08@char": {"eax"},
+    "fighter_29c08@sext": {"eax"},
+    "actor_pset_flag_5f@mutant": {"byte"},
+    "actor_pset_flag_5f@word": {"byte"},
+    "actor_pset_flag_5f@pset": {"byte"},
+    "fighter_29bc8@side": {"call #0"},
+    "fighter_29bc8@rec": {"call #0"},
+    "hit_anchor_y@mutant": {"byte", "call #1 memory"},
+    "hit_anchor_y@side": {"byte", "call #0", "call #1", "call #1 memory"},
+    "hit_anchor_y@field": {"byte", "call #1 memory"},
+    "ai_distance@mutant": {"eax"},
+    "ai_distance@order": {"call #0", "call #1"},
+    "fighter_slot_latch@mutant": {"byte", "call #0", "call #1"},
+    "fighter_slot_latch@side": {"byte", "call #0", "call #1"},
+    "fighter_slot_latch@anchor": {"call #1"},
+    "hit_record_x@mutant": {"byte", "call #1 memory", "eax"},
+    "hit_record_x@side": {"call #0", "call #1", "eax"},
+    "hit_record_x@anchor": {"byte", "call #1", "call #1 memory"},
+    "fighter_189fc@mutant": {"eax"},
+    "fighter_189fc@side": {"call #0", "eax"},
+    "fighter_189fc@bit": {"call #0", "eax"},
+    "fighter_189fc@al": {"eax"},
+    "fighter_18a4c@mutant": {"eax"},
+    "fighter_18a4c@side": {"call #0", "call #1", "call #2"},
+    "fighter_18a4c@arg": {"call #1"},
+    "fighter_39efc@mutant": {"eax"},
+    "fighter_39efc@val": {"eax"},
+    "fighter_39efc@side": {"eax"},
+    "fighter_3b8d8@mutant": {"eax"},
+    "fighter_3b8d8@side": {"eax"},
+    "fighter_3b8d8@bound": {"eax"},
+    "fighter_3b90c@mutant": {"eax"},
+    "fighter_3b90c@side": {"eax"},
+    "fighter_3b90c@bound": {"eax"},
+    "fighter_ctx_swap@mutant": {"byte"},
+    "fighter_ctx_swap@slot": {"byte"},
+    "fighter_ctx_swap@rec": {"byte"},
+    "fighter_1883c@mutant": {"byte", "call #2 memory", "call #3 memory"},
+    "fighter_1883c@side": {"byte", "call #2", "call #2 memory", "call #3", "call #3 memory"},
+    "fighter_1883c@latch": {"call #1", "call #1 memory", "call #2", "call #2 memory", "call #3"},
+    "fighter_1883c@arg": {"call #2"},
+    "hit_facing_flag@mutant": {"byte", "call #0 memory"},
+    "hit_facing_flag@mode": {"byte", "call #0"},
+    "hit_facing_flag@side": {"byte", "call #0", "call #0 memory"},
+    "hit_facing_flag@store": {"byte", "call #0 memory"},
+    "fighter_18b44@mutant": {"call #0"},
+    "fighter_18b44@latch": {"byte", "call #0 memory", "call #1 memory"},
+    "fighter_18b44@layer": {"call #1"},
+    "hit_geometry@mutant": {"call #1", "call #2", "call #3", "eax"},
+    "hit_geometry@side": {"call #2", "call #3", "eax"},
+    "hit_geometry@abs": {"call #1", "call #2", "call #3", "eax"},
+    "hit_geometry@table": {"call #1", "call #2", "call #3", "eax"},
+    "fighter_pose_start@mutant": {"byte"},
+    "fighter_pose_start@side": {"byte"},
+    "fighter_pose_start@arg": {"byte"},
+    "fighter_pose_start@field": {"byte"},
+    "actors_anim_seek@mutant": {"byte", "call #0 memory"},
+    "actors_anim_seek@char": {"byte", "call #0"},
+    "actors_anim_seek@width": {"byte"},
+    "fighter_37d18@mutant": {"byte"},
+    "fighter_37d18@state": {"byte", "call #0 memory", "call #1 memory", "call #2 memory"},
+    "fighter_37d18@order": {"call #2 memory"},
+    "fighter_3aa54@mutant": {"byte"},
+    "fighter_3aa54@char": {"byte", "call #0 memory"},
+    "fighter_3aa54@field": {"byte", "call #0 memory"},
+    "release_record@mutant": {"byte", "call #0", "call #0 memory", "call #1", "call #2", "call #3"},
+    "release_record@field": {"byte", "call #0", "call #0 memory", "call #1", "call #1 memory", "call #2", "call #3"},
+    "release_record@latch": {"byte", "call #0", "call #1", "call #2", "call #3"},
+    "anim_next_sprite_id@mutant": {"byte", "call #0", "eax"},
+    "anim_next_sprite_id@bit8": {"call #0", "eax"},
+    "anim_next_sprite_id@op": {"byte", "call #0", "eax"},
+    "anim_next_sprite_id@var": {"call #0"},
+    "anim_next_sprite_id@clear": {"call #0", "eax"},
+    "fighter_state_36638@mutant": {"byte", "call #0", "eax"},
+    "fighter_state_36638@anim": {"byte", "call #0", "eax"},
+    "fighter_state_36638@mode": {"byte", "call #0 memory"},
+    "fighter_state_36638@side": {"call #0"},
+}
+
 # Track P batch 6 (record 2026-10-03-reverse-p6): its rows with their EAX masks, and what alone catches
 # each of its mutants.
 P6_MASKS = {"anim_2bda0": 0, "fighter_22338": 0, "fighter_22494": 0, "fighter_22a40": 0, "fighter_23f10": 0, "fighter_2400c": 0, "fighter_241f4": 0, "fighter_24220": 0, "fighter_24338": 0, "fighter_37dd4": 0, "fighter_3e160": 0, "fighter_40148": 0, "fighter_40170": 0, "fighter_45c98": 0, "fighter_47e04": 0, "fighter_47e30": 0, "fighter_482e4": 0, "fighter_48374": 0}
@@ -696,7 +817,7 @@ class RealFunctionTests(unittest.TestCase):
                                              "fighter_slot_flag", "hit_anim_ctx", "hit_anim_start_b",
                                              "host_1b890", "rng_next"] + list(P1_MASKS) + list(P2_MASKS)
                                             + list(P3_MASKS) + list(P45_MASKS) + list(C1_MASKS)
-                                            + list(P6_MASKS)))
+                                            + list(P6_MASKS) + list(C2_MASKS)))
         for name, r in self.real.items():
             if name == "host_1b890":       # the named gap (record E3 §E3.8), tested on its own below
                 continue
@@ -712,7 +833,7 @@ class RealFunctionTests(unittest.TestCase):
             "fighter_45878@mutant", "fighter_ctx_same@mutant", "fighter_slot_flag@mutant",
             "hit_anim_ctx@mutant", "hit_anim_start_b@mutant", "hit_anim_start_b@set", "rng_next@mutant"]
             + list(P1_KINDS) + list(P2_KINDS) + list(P3_KINDS) + list(P45_KINDS) + list(C1_KINDS)
-            + list(P6_KINDS)))
+            + list(P6_KINDS) + list(C2_KINDS)))
         for name, r in self.mut.items():
             self.assertEqual(r.verdict, "MISMATCH", name)
 
@@ -777,7 +898,7 @@ class RealFunctionTests(unittest.TestCase):
             "fighter_3640c": 0, "fighter_37dcc": 0,
             "fighter_23130": 0xFF, "fighter_45878": 0, "anim_10fa8": 0, "anim_3e4e4": 0,
             "fighter_ctx_same": 0, "hit_anim_ctx": 0, "hit_anim_start_b": 0, "host_1b890": 0xFFFFFFFF,
-            **P1_MASKS, **P2_MASKS, **P3_MASKS, **P45_MASKS, **C1_MASKS, **P6_MASKS})
+            **P1_MASKS, **P2_MASKS, **P3_MASKS, **P45_MASKS, **C1_MASKS, **P6_MASKS, **C2_MASKS})
         # with the full mask the slot-flag original's scratch bits (case f9: EAX = 0x201) differ
         spec = dataclasses.replace([s for s in V.SPECS if s.name == "fighter_slot_flag"][0],
                                    eax_mask=0xFFFFFFFF)
@@ -1068,6 +1189,105 @@ class RealFunctionTests(unittest.TestCase):
                    else p.split(": ", 1)[1].split(" ")[0] for p in self.mut[name].problems}
             self.assertEqual(got, want, name)
 
+    def test_each_c2_mutant_is_caught_by_what_it_breaks(self):
+        # track P batch C2 (record 2026-10-04-reverse-c2): what alone catches each mutant; every row
+        # with a callee has one caught only by the call list or the memory at a call
+        for name, want in C2_KINDS.items():
+            got = {p.split(": ", 1)[1].split(":")[0] if p.split(": ", 1)[1].startswith("call #")
+                   else p.split(": ", 1)[1].split(" ")[0] for p in self.mut[name].problems}
+            self.assertEqual(got, want, name)
+        # the exact case set that alone catches each mutant (measured on the prototype)
+        for name, ids in (
+        ("fighter_state_39280@mutant", ['s0', 's1']),
+        ("fighter_state_39280@side", ['s1']),
+        ("fighter_state_39280@width", ['s0', 's1']),
+        ("palette_release@mutant", ['r0', 'r1']),
+        ("palette_release@width", ['r2']),
+        ("palette_release@off", ['r0']),
+        ("fighter_13244@mutant", ['c0', 'c1']),
+        ("fighter_13244@addr", ['c0', 'c1']),
+        ("fighter_13244@val", ['c0', 'c1']),
+        ("fighter_29c08@side", ['c1', 'c3']),
+        ("fighter_29c08@char", ['c2']),
+        ("fighter_29c08@sext", ['c3']),
+        ("actor_pset_flag_5f@mutant", ['f1']),
+        ("actor_pset_flag_5f@word", ['f0', 'f1', 'f2']),
+        ("actor_pset_flag_5f@pset", ['f0', 'f1', 'f2']),
+        ("fighter_29bc8@side", ['c1']),
+        ("fighter_29bc8@rec", ['c0', 'c1']),
+        ("hit_anchor_y@mutant", ['y0', 'y1', 'y2']),
+        ("hit_anchor_y@side", ['y2']),
+        ("hit_anchor_y@field", ['y0', 'y1', 'y2']),
+        ("ai_distance@mutant", ['d0', 'd1']),
+        ("ai_distance@order", ['d0', 'd1']),
+        ("fighter_slot_latch@mutant", ['l0', 'l1', 'l2', 'l3']),
+        ("fighter_slot_latch@side", ['l3']),
+        ("fighter_slot_latch@anchor", ['l1']),
+        ("hit_record_x@mutant", ['x0', 'x2', 'x3']),
+        ("hit_record_x@side", ['x3']),
+        ("hit_record_x@anchor", ['x1', 'x2']),
+        ("fighter_189fc@mutant", ['z0', 'z2', 'z5', 'z6']),
+        ("fighter_189fc@side", ['z6']),
+        ("fighter_189fc@bit", ['z0', 'z1', 'z2', 'z3', 'z4', 'z5', 'z6']),
+        ("fighter_189fc@al", ['z5']),
+        ("fighter_18a4c@mutant", ['a0', 'a1', 'a3', 'a4', 'a5']),
+        ("fighter_18a4c@side", ['a5']),
+        ("fighter_18a4c@arg", ['a0', 'a1', 'a2', 'a3', 'a4', 'a5']),
+        ("fighter_39efc@mutant", ['e0', 'e4']),
+        ("fighter_39efc@val", ['e0', 'e2', 'e4']),
+        ("fighter_39efc@side", ['e4']),
+        ("fighter_3b8d8@mutant", ['b3']),
+        ("fighter_3b8d8@side", ['b4']),
+        ("fighter_3b8d8@bound", ['b1', 'b3', 'b4']),
+        ("fighter_3b90c@mutant", ['c1']),
+        ("fighter_3b90c@side", ['c4']),
+        ("fighter_3b90c@bound", ['c2']),
+        ("fighter_ctx_swap@mutant", ['w0', 'w1']),
+        ("fighter_ctx_swap@slot", ['w0', 'w1']),
+        ("fighter_ctx_swap@rec", ['w0', 'w1']),
+        ("fighter_1883c@mutant", ['c0', 'c1', 'c2']),
+        ("fighter_1883c@side", ['c1']),
+        ("fighter_1883c@latch", ['c0', 'c1', 'c2']),
+        ("fighter_1883c@arg", ['c0', 'c1', 'c2']),
+        ("hit_facing_flag@mutant", ['m2']),
+        ("hit_facing_flag@mode", ['m0']),
+        ("hit_facing_flag@side", ['m3']),
+        ("hit_facing_flag@store", ['m1', 'm2']),
+        ("fighter_18b44@mutant", ['d2']),
+        ("fighter_18b44@latch", ['d2', 'd3']),
+        ("fighter_18b44@layer", ['d2', 'd3']),
+        ("hit_geometry@mutant", ['g0', 'g1', 'g2', 'g3', 'g4', 'g5', 'g6']),
+        ("hit_geometry@side", ['g5']),
+        ("hit_geometry@abs", ['g0', 'g1', 'g2', 'g3', 'g4', 'g5', 'g6']),
+        ("hit_geometry@table", ['g0', 'g1', 'g2', 'g3', 'g4', 'g5', 'g6']),
+        ("fighter_pose_start@mutant", ['p0', 'p1']),
+        ("fighter_pose_start@side", ['p1']),
+        ("fighter_pose_start@arg", ['p0', 'p1']),
+        ("fighter_pose_start@field", ['p0', 'p1']),
+        ("actors_anim_seek@mutant", ['n0']),
+        ("actors_anim_seek@char", ['n0']),
+        ("actors_anim_seek@width", ['n0', 'n1']),
+        ("fighter_37d18@mutant", ['r0', 'r1']),
+        ("fighter_37d18@state", ['r0', 'r1']),
+        ("fighter_37d18@order", ['r0', 'r1']),
+        ("fighter_3aa54@mutant", ['a0', 'a1', 'a2']),
+        ("fighter_3aa54@char", ['a0', 'a1', 'a2']),
+        ("fighter_3aa54@field", ['a0', 'a1', 'a2']),
+        ("release_record@mutant", ['r0', 'r1']),
+        ("release_record@field", ['r0', 'r1']),
+        ("release_record@latch", ['r0', 'r1']),
+        ("anim_next_sprite_id@mutant", ['s3', 's4']),
+        ("anim_next_sprite_id@bit8", ['s0', 's3', 's4', 's5']),
+        ("anim_next_sprite_id@op", ['s2', 's3', 's4']),
+        ("anim_next_sprite_id@var", ['s3', 's4']),
+        ("anim_next_sprite_id@clear", ['s0', 's1', 's2', 's3', 's4', 's5']),
+        ("fighter_state_36638@mutant", ['s0']),
+        ("fighter_state_36638@anim", ['s2', 's3', 's4', 's6']),
+        ("fighter_state_36638@mode", ['s6']),
+        ("fighter_state_36638@side", ['s4']),
+        ):
+            self.assertEqual(sorted({p.split(":")[0] for p in self.mut[name].problems}), ids, name)
+
     def test_each_stub_declares_the_registers_its_callee_clobbers(self):
         # Call.clobbers, re-derived from the bytes (record §E3.5's table, §E3.12)
         img = E.Image.load(os.path.join(self.tmp.name, "image.bin"))
@@ -1089,7 +1309,10 @@ class RealFunctionTests(unittest.TestCase):
                                  0x2AD40: ("edx", "edi", "ebp"),
                                  0x39280: (), 0x13244: (), 0x2A148: ("edx",), 0x2BCF4: ("edx",),
                                  0x1890C: ("edx",), 0x29BC8: ("ebx", "edx"), 0x37D18: ("edx",),
-                                 0x13C70: ("ebx", "edx"), 0x3AA54: ()})
+                                 0x13C70: ("ebx", "edx"), 0x3AA54: (),
+                                 0x18540: (), 0x18350: ("edx",), 0x18788: (), 0x1881C: (),
+                                 0x1A5AC: (), 0x38154: (), 0x249B0: (), 0x249D0: (),
+                                 0x2B150: ("esi", "edi", "ebp"), 0x29F34: ("edx",)})
         for addr, declared in stubs.items():
             self.assertEqual(E.callee_clobbers(img, addr), declared, hex(addr))
 
@@ -1184,8 +1407,8 @@ class RealFunctionTests(unittest.TestCase):
                          "--self-check"])
         self.assertEqual(rc, 0)
         # the closed-row count is over the rows that have callees (126), the 23 without are counted apart
-        self.assertIn("diff-verify: 149/149 functions VERIFIED; 391/391 mutants detected; 1 named gaps; "
-                      "41/126 rows with callees closed (23 have none).", out.getvalue())
+        self.assertIn("diff-verify: 176/176 functions VERIFIED; 478/478 mutants detected; 1 named gaps; "
+                      "61/145 rows with callees closed (31 have none).", out.getvalue())
 
 
 # ---- E3: the call list, named gaps, the callee column (record 2026-10-01-reverse-e3 §E3.4, §E3.8) --
PATCH
cmake --build build 2>&1 | tail -1
```

- [ ] **Step 2: the task gates.**

```bash
make diff-verify DIFF_IMAGE=/tmp/pr_c2_final.bin DIFF_TABLE=/tmp/pr_c2_final_table.md 2>&1 | tail -1
make entry-triage E2_IMAGE=/tmp/pr_c2_final_e2.bin 2>&1 | tail -3
PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -1
```

Expected:

```
diff-verify: 176/176 functions VERIFIED; 478/478 mutants detected; 1 named gaps; 61/145 rows with callees closed (31 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
entry-triage: targets 240 unported, 255 ported; supplement 131 (7 unported, 0 stale); untrusted entries 30
entry-triage: voice sites outside Ghidra 134: 0 in unported code, 115 in ported code, 19 nowhere
all checks passed
```

- [ ] **Step 3: commit.**

```bash
git add port/src/game/actors.c port/src/game/actors.h port/src/game/fighter.c port/src/game/fighter.h port/tests/diff_runner.c tools/diff_verify.py tools/tests/test_diff_verify.py
git commit -m "tools: C2 callee rows: 27 verification-only rows, 87 mutants, the anim_read_var and hit_geometry raw-fidelity fixes"
```

### Task 3: the review sweep (one commit)

**Files:** the patch's rows. **Interfaces:** consumes Task 2's state; produces the store sweep's
findings.

- [ ] **Step 1: the store sweep.** For every C2 row, poke each field the row writes to a value that
  differs from what it writes and re-run `--function NAME --self-check`; a store with no sentinel
  fails some mutant. The prototype's sweep found and fixed: the `0x2A408` pointer fixture (a
  non-pointer bit-15 word made `@bit8` fault), the `release_record` list-head seed (a non-address
  `0x105B3C` made the mutants fault), and the `0x18A4C` real `1A570` (a constant stub EAX made the
  two `mov al,1` blocks unreachable).
- [ ] **Step 2: the clobber and case-set re-check.** `python3 -m unittest
  tools.tests.test_diff_verify.RealFunctionTests` (the clobber re-derivation and the C2 case-set
  test). Expected: OK.
- [ ] **Step 3: commit any fix.**

```bash
git add <the fixed files>
git commit -m "tests: C2 review sweep: the store sentinels and the case-set pins"
```

### Task 4: C2b — the nine deferred rows (a follow-on plan, no commit here)

**Files:** none. **Interfaces:** the record §C2.5 table is the input.

- [ ] **Step 1: write the C2b plan.** The nine: `0x2B2A0`, `0x33754`, `0x13C70`, `0x2C3FC`,
  `0x2AE14`, `0x36870`, `0x39834`, `0x3B298`, `0x3B714`, with the seam lists of record §C2.5 (C1's
  §C1.5 for the six, the planner's measured lists for the three). The host/runtime callees
  (`0x1B544`, `0x62003`) need their handling decided in that plan: `0x1B544` allowed against a
  preloaded resource entry, or a stub with the resolved pointer; `0x62003`'s fatal block named.
- [ ] **Step 2: note the new frontier.** The 11 row-less stubs of record §C2.8 are C3.

### Task 5: Closure (one commit)

**Files:** `docs/PROGRESS.md`, the record. **Interfaces:** consumes Task 2's measured state.

- [ ] **Step 1: the final gates.**

```bash
make diff-verify DIFF_IMAGE=/tmp/pr_c2_close.bin 2>&1 | tail -1
make entry-triage E2_IMAGE=/tmp/pr_c2_close_e2.bin 2>&1 | tail -2
PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -1
python3 tools/port_progress.py
make verify
```

Expected: the Task 2 counter; E2 byte-identical; `all checks passed`; `771 1203 64` / `731 731 100`;
and the full `make verify` exit 0 with the 45 oracle lines equal to
`.superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt` and the `make audio-render` WAV
equal to `before-t2.wav` (the port changes are seams and two idempotent corrections; no rendering,
timing or RNG path changes).

- [ ] **Step 2: append the PROGRESS paragraph** (the 27 rows, the counter, the deferred nine, the
  new frontier).

- [ ] **Step 3: commit.**

```bash
git add docs/PROGRESS.md docs/superpowers/plans/2026-10-04-reverse-c2-derivations.md
git commit -m "docs: C2 closure: the 27 callee rows measured, the nine C2b deferrals and the new frontier"
```
