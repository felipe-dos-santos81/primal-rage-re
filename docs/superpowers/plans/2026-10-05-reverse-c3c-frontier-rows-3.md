# C3c: the frontier rows, part 3 (track P, batch C3c) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add the differential-verification row for each of the seventeen frontier addresses this
batch measures — the render-list seam split (`0x1C390` `0x1C3A0` `0x1C458` `0x1C3D0`), the
type-0x01 callbacks (`0x127C0` `0x12800`), the new items the C3b rows created (`0x367DC`
`0x1CA40` `0x33714` `0x33734` `0x12800` `0x18428` `0x18460`) and this session's type-family
slice (`0x2BD44` `0x3B6C4` `0x3C520` `0x1A734` `0x39738`) — with their seams, bindings, mutants
and exact-set pins; one raw-over-port correction (`0x39738`). The remaining thirteen type-family
rows are C3d (record §C3c.7).

**Architecture:** Each row is a `Spec` in `tools/diff_verify.py` (`C3C_SPECS`), a `b_*` binding and
`m_*` mutants in `port/tests/diff_runner.c` (`k_bindings`), and exact-set assertions in
`tools/tests/test_diff_verify.py` (`C3C_MASKS`, `C3C_KINDS`, the case-set table, the clobber table,
the counter line). `port/src` changes: the `render_list_insert` split into `render_pop_free`
(`0x1C390`, new) + `render_splice` (`0x1C3A0`, exported) and the `render_list_remove` split into
`render_find` (`0x1C458`, new) + `render_unlink` (`0x1C3D0`, new); `actor_type_127C0`/
`actor_type_12800` and `list_insert_after`/`list_unlink` lose `static` (the last two only for the
mutant cores); `fighter_18428` loses `static` and gains its seam; `AIL_sequence_status` (`0x5DEED`)
gains a seam; `fighter_39738` gets the raw-over-port early return.

**Tech Stack:** C11 (`port/src`, `port/tests`), Python 3.12 (`unittest`), `unicorn` 2.1.4 and
`capstone` 5.0.7 (`tools/requirements-diff.txt`), CMake, `make`.

**Spec:** `docs/superpowers/specs/2026-09-30-reverse-completion-design.md` §4 track P, §5.1-§5.3,
§6 ("each ported function has a verification row; unhit blocks and non-emulable instructions named;
counters and README percentage updated"), §7.

**Derivation record:** `docs/superpowers/plans/2026-10-05-reverse-c3c-derivations.md` (§C3c.1 the
members, §C3c.2 the rows, seams, fixtures and the `0x39738` correction, §C3c.3 the mutants and
their measured catching/kinds sets, §C3c.4 the counters, §C3c.5 the named gaps and limits,
§C3c.6 what the planner ran, §C3c.7 the C3d deferral with its evidence). Recipe:
`2026-10-01-reverse-e3-derivations.md` §E3.10; checklist:
`.superpowers/sdd/2026-10-01-plans/p-track-review-checklist.md`.

**What the planner ran (scratch, 2026-10-06, on `reverse-c3c` at `main` `bb14036` = C3b merged;
image sha1 `ff3b8cb14e00f1c282de7b7e15dcd7c230766947`, the E2/E3/P1-P8/C1-C3b image):** the baseline
(Task 1) and a full prototype of the seventeen rows: the split and exports and the `0x5DEED` seam
first, then the rows family by family. Every row was measured with `--function NAME --self-check`
until VERIFIED with every mutant detected, then the full `python3 tools/diff_verify.py
--self-check` (`251/251 functions VERIFIED; 874/874 mutants detected; 1 named gaps; 178/195 rows
with callees closed (56 have none)`), `python3 -m unittest tools.tests.test_diff_verify` (107 tests
OK), `make entry-triage` (byte-identical: `targets 233 unported, 262 ported; supplement 131 (3
unported, 0 stale); untrusted 30`; voice `0 / 115 / 19`), `PR_ORACLE_REQUIRED=1 ./build/run_tests`
(all checks passed), `python3 tools/port_progress.py` (`771 1203 64` / `731 731 100`) and a
`symbols.h` regeneration (byte-identical). One raw-over-port correction was found and recorded
(`0x39738`, record §C3c.2 correction 1); the prototype was then reverted (`git checkout -- port
tools`); the patch below is the exact diff it applied.

**Re-baseline note.** The counters below are `bb14036`'s. If `main` moves before this plan
executes, Task 1 records the measured base and every later expected counter adds this plan's
increments: functions +17, mutants +95, rows with callees +10, closed rows +13, no-callee rows +7.
The E2 table must not move (no E2 candidate; no `fn_register`): if a task regenerates it, the line
must be byte-identical.

## Decisions needed from the user

**None.** The C3b §C3b.5 row-shape decision (`0x1C390`/`0x1C3A0`, two rows via the split) was
executed; the C3c brief's `0x1C458`/`0x1C3D0` pair got the same split. The one raw-over-port
correction (`0x39738`: the slot+0x8C path returns at the raw's `0x39789 jmp 0x3982C` before the
15% cut and the difficulty multiplier) is recorded with its disassembly and executed-trace
evidence in the record §C3c.2.

## The C3c roadmap

| task | what | commit |
|---|---|---|
| 1 | baseline on `bb14036` (no commit) | - |
| 2 | the seventeen rows, the split/exports, bindings, mutants and the test exact-set updates | `tools:` |
| 3 | the review sweep (stores, clobbers, mutant case sets) | `tests:` (only if a fix) |
| 4 | closure: PROGRESS, the record's §C3c.8, the final gates | `docs:` |

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
  no target: the table must be byte-identical. (The split adds `/* 0x1C390 — ...` to
  `render_pop_free`, but 0x1C390 was already counted by the counter's regex through
  `actors.c:3877`'s inline `/* 0x1C390 + 0x1C3A0 */` comment; `port_progress.py` must print
  `771 1203 64` before and after — record §C3c.2.)
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
  adds the C3c case-set test; no other assertion changes.
- AGENTS.md: "`data/` is git-ignored and read-only — never write to it." / "Commit style:
  `<area>: <what changed>`. **Never `git add -A`** — stage named files." No `Co-Authored-By`
  trailer. Never run `make gp-capture`.
- The C3c brief: "the gp miss sets must stay exactly as Task 1 measures them" (this batch touches
  no gameplay path; the only behavioral `port/src` change is the `0x39738` correction, which no
  driver path reaches — the executor's gp gates are the evidence).
- Spec §5.3 / E3 §E3.4: "VERIFIED" is equivalence on the exercised blocks and inputs only, each
  function with its callees stubbed or run as stated.

## Where to run

The worktree `.worktrees/reverse-c3c` (branch `reverse-c3c`). Every command runs from the worktree
root; `data/` and `.superpowers/` are symlinks; `build/` exists. Before Task 1:

```bash
git rev-parse HEAD   # bb14036
python3 -c "import unicorn, capstone; print(unicorn.__version__, capstone.__version__)"  # 2.1.4 5.0.7
cmake -S port -B build && cmake --build build 2>&1 | tail -1
./build/diffrun --exe data/game/C/PRAGE.EXE --image-out /tmp/pr_c3c_img.bin && shasum /tmp/pr_c3c_img.bin
ls port/tests/ghidra_data.bin   # the PR_ORACLE_REQUIRED fixture (git-ignored); copy it from the main repo if absent
```

The last-but-one line must print `ff3b8cb14e00f1c282de7b7e15dcd7c230766947`; another hash means the
image differs from the one the record measured: stop. Ledger:
`.superpowers/sdd/2026-10-05-reverse-c3c-frontier-rows-3/progress.md`.

### The seventeen rows (the brief's grouping, executed as families)

The patch is one `git diff` (Task 2 applies it at once); the families below are the gate order:

| family | rows | shared seams/fixtures |
|---|---|---|
| the render seam | `0x1C390` `0x1C3A0` `0x1C458` `0x1C3D0` | the split (`render_pop_free`/`render_find`/`render_unlink` new, `render_splice` exported); no callees; the shared node/pset scratch |
| the type-0x01 callbacks | `0x127C0` `0x12800` | the exported `actor_type_127C0`/`actor_type_12800` and `list_insert_after`/`list_unlink`; the `0x249B0`/`0x249D0` real calls |
| the new items | `0x367DC` `0x1CA40` `0x33714` `0x33734` `0x18428` `0x18460` | the `0x5DEED` seam; the exported `fighter_18428` (seamed); the `0x2BC30`/`0x18B04`/`0x3C480` stubs |
| the type-family slice | `0x2BD44` `0x3B6C4` `0x3C520` `0x1A734` `0x39738` | the `0x39738` correction; the `0x2A408`/`0x2B150`/`0x188DC`/`0x1890C` stubs and the `0x1A570` real / `0x33950`/`0x339AC`/`0x33A10` allows |

## How the code steps are written

Task 2's patch is the prototype's exact `git diff` (the planner applied it, ran every gate, and
reverted). Apply it with `git apply`; it touches `port/src/game/actors.c`, `port/src/game/actors.h`,
`port/src/game/fighter.c`, `port/src/game/fighter.h`, `port/src/platform/audio/ail.c`,
`port/src/platform/render.c`, `port/src/platform/render.h`, `port/tests/diff_runner.c`,
`tools/diff_verify.py` and `tools/tests/test_diff_verify.py`. If `main` moved, `git apply --3way`
and resolve by keeping the patch's additions at the same anchors; never merge the E2 table by hand.

## File Structure

| File | Responsibility |
|---|---|
| `port/src/platform/render.c`/`.h` | the `render_list_insert` and `render_list_remove` splits (`0x1C390`/`0x1C3A0`/`0x1C458`/`0x1C3D0`) |
| `port/src/game/actors.c`/`.h` | `actor_type_127C0`/`actor_type_12800` exported (declarations updated); `list_insert_after`/`list_unlink` exported for the mutant cores |
| `port/src/game/fighter.c`/`.h` | `fighter_18428` exported + seamed; the `fighter_39738` raw-over-port correction |
| `port/src/platform/audio/ail.c` | `AIL_sequence_status` (`0x5DEED`) seam |
| `port/tests/diff_runner.c` | the seventeen bindings and 95 mutants, the `C3C` cores |
| `tools/diff_verify.py` | `C3C_SPECS` (the seventeen rows and their fixtures) |
| `tools/tests/test_diff_verify.py` | `C3C_MASKS`, `C3C_KINDS`, the case-set test, the clobber and counter lines |
| `docs/superpowers/plans/2026-10-05-reverse-c3c-derivations.md` | the record (this plan's source of values) |
| `docs/PROGRESS.md` | Task 4's paragraph |

---

### Task 1: Baseline (no commit)

**Files:** none. **Interfaces:** consumes the worktree at `bb14036`; produces the baseline counters
and table.

- [ ] **Step 1: the task gates on the untouched tree.**

```bash
make diff-verify DIFF_IMAGE=/tmp/pr_c3c_base.bin DIFF_TABLE=/tmp/pr_c3c_base_table.md 2>&1 | tail -1
make entry-triage E2_IMAGE=/tmp/pr_c3c_base_e2.bin 2>&1 | tail -2
python3 tools/port_progress.py
```

Expected (measured at `bb14036`):

```
diff-verify: 234/234 functions VERIFIED; 779/779 mutants detected; 1 named gaps; 165/185 rows with callees closed (49 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
entry-triage: targets 233 unported, 262 ported; supplement 131 (3 unported, 0 stale); untrusted entries 30
entry-triage: voice sites outside Ghidra 134: 0 in unported code, 115 in ported code, 19 nowhere
771 1203 64
731 731 100 (portable: excludes 81 host-owned/deferred and runtime >= 5D000)
```

- [ ] **Step 2: pin the gp miss sets.** `PR_GP_DUMP` scenarios' pinned sets in `test_platform.c`
  must be the ones Task 2 leaves untouched. The `0x39738` correction is the batch's only
  behavior change; the function is reached only through `fighter_39834` (a stub in every current
  row), and Task 4's full `make verify` is the evidence no scenario reaches it.

### Task 2: the seventeen rows (one commit)

**Files:** the patch below. **Interfaces:** produces `C3C_SPECS`, the seventeen bindings and 95
mutants, the render splits and exports, the `actor_type_*`/`list_*`/`fighter_18428` exports, the
`AIL_sequence_status` seam, the `fighter_39738` correction, and the test exact-set updates;
consumes the C1/C2/C2b/C3/C3b fixtures and the C3b scratch records.

- [ ] **Step 1: apply the patch.** `git apply - <<'PATCH' ... PATCH` with the diff embedded at the
  end of this task (the exact prototype diff; 1314 insertions over 10 files).

- [ ] **Step 2: build and run each row's self-check.**

```bash
cmake --build build 2>&1 | tail -1
for f in render_pop_free render_splice render_find render_unlink actor_type_127C0 actor_type_12800 \
         fighter_18428 fighter_18460 fighter_state_367dc snd_music_playing palette_record_flagged \
         palette_record fighter_2bd44 fighter_3b6c4 hit_anim_start_c fighter_block_hit fighter_39738; do
  python3 tools/diff_verify.py --function "$f" --self-check
done
```

Expected rows (measured; the format is row, entry, cases, blocks, mutants, named unhit):

| row | entry | cases | blocks | mutants | named unhit |
|---|---|---|---|---|---|
| `render_pop_free` | 0x1C390 | 3 | 1/1 | 3/3 | — |
| `render_splice` | 0x1C3A0 | 6 | 5/5 | 5/5 | — |
| `render_find` | 0x1C458 | 4 | 5/5 | 4/4 | — |
| `render_unlink` | 0x1C3D0 | 5 | 5/5 | 5/5 | — |
| `actor_type_127C0` | 0x127C0 | 3 | 6/6 | 6/6 | — |
| `actor_type_12800` | 0x12800 | 3 | 3/3 | 5/5 | — |
| `fighter_18428` | 0x18428 | 4 | 3/3 | 0/0 | — (the effect-free reader; record §C3c.5) |
| `fighter_18460` | 0x18460 | 11 | 10/10 | 8/8 | — |
| `fighter_state_367dc` | 0x367DC | 5 | 5/5 | 8/8 | — |
| `snd_music_playing` | 0x1CA40 | 4 | 3/3 | 6/6 | — |
| `palette_record_flagged` | 0x33714 | 2 | 1/1 | 5/5 | — |
| `palette_record` | 0x33734 | 2 | 1/1 | 5/5 | — |
| `fighter_2bd44` | 0x2BD44 | 2 | 1/1 | 7/7 | — |
| `fighter_3b6c4` | 0x3B6C4 | 6 | 6/6 | 6/6 | — |
| `hit_anim_start_c` | 0x3C520 | 3 | 1/1 | 6/6 | — |
| `fighter_block_hit` | 0x1A734 | 6 | 9/9 | 8/8 | — |
| `fighter_39738` | 0x39738 | 8 | 17/17 | 8/8 | — |

(The per-row run reports the callee column as `unverified` for every callee whose own row is not
run; only the full run's table reads `VERIFIED`. Record E3 §E3.8.)

- [ ] **Step 3: the task gates.**

```bash
make diff-verify DIFF_IMAGE=/tmp/pr_c3c_after.bin DIFF_TABLE=/tmp/pr_c3c_after_table.md 2>&1 | tail -1
python3 -m unittest tools.tests.test_diff_verify 2>&1 | tail -3
make entry-triage E2_IMAGE=/tmp/pr_c3c_after_e2.bin 2>&1 | tail -2
PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -1
python3 tools/gen_symbols.py port/decomp /tmp/pr_c3c_sym.h && diff /tmp/pr_c3c_sym.h port/src/symbols.h
python3 tools/port_progress.py
```

Expected: `251/251 functions VERIFIED; 874/874 mutants detected; 1 named gaps; 178/195 rows with
callees closed (56 have none)`; `OK` (107 tests); E2 byte-identical; `all checks passed`;
`symbols.h` regeneration byte-identical; `771 1203 64` / `731 731 100`.

- [ ] **Step 4: commit.**

```bash
git add port/src/platform/render.c port/src/platform/render.h port/src/game/actors.c \
        port/src/game/actors.h port/src/game/fighter.c port/src/game/fighter.h \
        port/src/platform/audio/ail.c port/tests/diff_runner.c tools/diff_verify.py \
        tools/tests/test_diff_verify.py
git commit -m "tools: C3c: the seventeen frontier rows, their splits and mutants"
```

### Task 3: the review sweep (one commit, only if a fix)

**Files:** the patch's rows. **Interfaces:** consumes Task 2's state; produces the store sweep's
findings.

- [ ] **Step 1: the store sweep.** For every C3c row, poke each field the row writes to a value that
  differs from what it writes and re-run `--function NAME --self-check`; a store with no sentinel
  fails some mutant. The planner's sweep is baked into the fixtures (the scratch records' fields,
  the palette records' `+0x0D..0x0F` flag neighbours, the two table entries and fallbacks, the
  slot words); re-check the fields the record lists as the rows' only writes: the free/list heads
  `0x10275C`/`0x105B44`, the node `+0`/`+4`, the callback nodes' `+0`/`+4`/`+8`, `rec+0x14`,
  `rec+0x24/+0x28/+0x29/+0x2A/+8`, `rec+0x4C/+0x4D`, `slot+0x40/+0x52/+0x53/+0x54/+0x55/+0x5F`,
  `slot+0x43/+0x54/+0x60/+0x61/?0x62`, the palette head `0x107798` and record bytes, the
  `0x104B00` mode read, and `fighter_39738`'s return.
- [ ] **Step 2: the clobber and case-set re-check.** `python3 -m unittest
  tools.tests.test_diff_verify.RealFunctionTests` (the clobber re-derivation and the C3c case-set
  test). Expected: OK.
- [ ] **Step 3: commit any fix.**

```bash
git add <the fixed files>
git commit -m "tests: C3c review sweep: the store sentinels and the case-set pins"
```

### Task 4: Closure (one commit)

**Files:** `docs/PROGRESS.md`, the record. **Interfaces:** consumes Task 2's measured state.

- [ ] **Step 1: the final gates.**

```bash
make diff-verify DIFF_IMAGE=/tmp/pr_c3c_close.bin 2>&1 | tail -1
make entry-triage E2_IMAGE=/tmp/pr_c3c_close_e2.bin 2>&1 | tail -2
PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -1
python3 tools/port_progress.py
make verify
```

Expected: the Task 2 counter; E2 byte-identical; `all checks passed`; `771 1203 64` / `731 731
100`; and the full `make verify` exit 0 with the 45 oracle lines equal to
`.superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt`, the `make audio-render` WAV
equal to the pre-change WAV, every gp ratchet line `ok` with measured == pin (Task 1's list
verbatim), and `symbols.h` byte-identical. The `port/src` changes are the splits/exports, one
seam and the `0x39738` correction; the full ladder is what proves no oracle-visible path moved.

- [ ] **Step 2: append the PROGRESS paragraph** (the seventeen rows, the counter, the `0x39738`
  correction, the C3c → C3d deferral of record §C3c.7).

- [ ] **Step 3: append §C3c.8 Results to the record** (the executed tree's counters, the commit
  shas, the gate log lines), mirroring C3b's §C3b.8.

- [ ] **Step 4: commit.**

```bash
git add docs/PROGRESS.md docs/superpowers/plans/2026-10-05-reverse-c3c-derivations.md
git commit -m "docs: C3c closure: the seventeen frontier rows measured and the C3d deferral"
```

### The patch

```diff
diff --git a/port/src/game/actors.c b/port/src/game/actors.c
index 7c715e2..6f8bb8d 100644
--- a/port/src/game/actors.c
+++ b/port/src/game/actors.c
@@ -30,8 +30,9 @@
 
 /* ---- the two splice lists (0x249B0/0x249C0/0x249D0) -------------------- */
 
-/* 0x249B0: insert rec immediately after `at`. */
-static void list_insert_after(u32 at, u32 rec)
+/* 0x249B0: insert rec immediately after `at`. Exported for the C3c type-callback
+ * rows' mutant cores (the seams stay the only observers). */
+void list_insert_after(u32 at, u32 rec)
 {
     PR_SEAM(0x249B0u, at, rec);
     u32 next = DSD(at);
@@ -51,8 +52,8 @@ static void list_insert_before(u32 at, u32 rec)
     DSD(prev) = rec;
 }
 
-/* 0x249D0: unlink rec. */
-static void list_unlink(u32 rec)
+/* 0x249D0: unlink rec. Exported for the C3c type-callback rows' mutant cores. */
+void list_unlink(u32 rec)
 {
     PR_SEAM(0x249D0u, rec);
     u32 prev = DSD(rec + 4);
@@ -133,14 +134,14 @@ static void anim_code_150AC(u32 rec, u32 arg);
 typedef u8 (*actor_type_cb1)(u32 rec, u32 slot);
 typedef void (*actor_type_cb2)(u32 rec);
 
-static u8   actor_type_127C0(u32 rec, u32 slot);
+u8   actor_type_127C0(u32 rec, u32 slot);
 static u8   actor_type_198E8(u32 rec, u32 slot);
 static u8   actor_type_28F64(u32 rec, u32 slot);
 static u8   actor_type_2901C(u32 rec, u32 slot);
 static u8   actor_type_48CD8(u32 rec, u32 slot);
 static u8   actor_type_412F0(u32 rec, u32 slot);
 static u8   actor_type_412FC(u32 rec, u32 slot);
-static void actor_type_12800(u32 rec);
+void actor_type_12800(u32 rec);
 static void actor_type_19928(u32 rec);
 static void debris_update(void);
 static void actor_type_290D0(u32 rec);
@@ -3088,8 +3089,9 @@ void actor_pset_word_set(u32 rec, u32 word)
  * is empty, else they link the popped node at rec+0x14 and re-insert it at
  * the destination's head (0x249B0 insert-after; 0x249C0 is insert-before). */
 
-/* 0x127C0. Type 0x01: pop the 0xF0A78 head, insert it at the 0xF0AE0 head. */
-static u8 actor_type_127C0(u32 rec, u32 slot)
+/* 0x127C0. Type 0x01: pop the 0xF0A78 head, insert it at the 0xF0AE0 head.
+ * Exported for its own C3c row's binding. */
+u8 actor_type_127C0(u32 rec, u32 slot)
 {
     (void)slot;
     u32 rec2 = list_head(DS_000F0A78);
@@ -3101,8 +3103,9 @@ static u8 actor_type_127C0(u32 rec, u32 slot)
     return 0;
 }
 
-/* 0x12800. Type 0x01's teardown: return the rec+0x14 node to 0xF0A78. */
-static void actor_type_12800(u32 rec)
+/* 0x12800. Type 0x01's teardown: return the rec+0x14 node to 0xF0A78.
+ * Exported for its own C3c row's binding. */
+void actor_type_12800(u32 rec)
 {
     u32 rec2 = DSD(rec + 0x14);
     if (rec2 == 0) return;
diff --git a/port/src/game/actors.h b/port/src/game/actors.h
index 16ad577..382ee82 100644
--- a/port/src/game/actors.h
+++ b/port/src/game/actors.h
@@ -76,6 +76,17 @@ void actor_set_dead(u32 rec);
  * the node's +0x10 child, then return the node to 0x1083C4. Exported for
  * fight_4e67c's direct call (0x4E8EE, not a stored callback). */
 void actor_type_49444(u32 rec);
+/* 0x249B0 / 0x249D0 (C3c). The actors.c list splice helpers, exported for the
+ * type-callback rows' mutant cores; the seams record their calls. */
+void list_insert_after(u32 at, u32 rec);
+void list_unlink(u32 rec);
+/* 0x127C0 (C3c). The type-0x01 cb1: pop the 0xF0A78 head and insert it at the
+ * 0xF0AE0 head, returning 0; 0xFF when the list is empty. Exported for its own
+ * row's binding. */
+u8 actor_type_127C0(u32 rec, u32 slot);
+/* 0x12800 (C3c). The type-0x01 cb2: return the rec+0x14 node to 0xF0A78.
+ * Exported for its own row's binding. */
+void actor_type_12800(u32 rec);
 /* 0x10D70. Clear rec+0x28 bit 2 and write `word` to the record's pset +0, its
  * bit 15 taken from rec+0x28 bit 14. The wipe steps 0x4F9E4/0x4FA88 call it
  * (record §43-B), and so do 0x1D2F0/0x1D464 (fight.c, record §48-U) and
diff --git a/port/src/game/fighter.c b/port/src/game/fighter.c
index 9da06c0..0387168 100644
--- a/port/src/game/fighter.c
+++ b/port/src/game/fighter.c
@@ -132,8 +132,9 @@ int fighter_actor_bit15_clear(u32 side)
  * and otherwise jumps through the seven-entry table 0x1840C, whose every entry
  * is 0x18408, a bare RET (the fixup-applied dwords `08 84 01 00` x 7). The
  * dispatch therefore has no effect for any character. */
-static void fighter_18428(u32 side, u32 sprite, u32 a0, u32 a1)
+void fighter_18428(u32 side, u32 sprite, u32 a0, u32 a1)
 {
+    PR_SEAM(0x18428u, side, sprite, a0, a1);
     u32 slot = DSD(DS_001077A8 + side * 4u);            /* 0x18428 */
     u32 ch = (u32)DSB(slot + 0x7Au);                    /* 0x1842F */
     (void)sprite; (void)a0; (void)a1;
@@ -6057,8 +6058,12 @@ u32 fighter_3aa54(u32 slot)
 }
 
 /* 0x39738. The reaction damage/knockback scaler: pick the per-character base
- * from the 0xBECxx tables scaled by 100, apply the +0x8C guards and (when
- * slot+0x63 != 0) the DS_001082C8 difficulty multiplier. */
+ * from the 0xBECxx tables scaled by 100, and in the slot+0x8C == 0 path apply
+ * the other slot's 15% cut and (when slot+0x63 != 0) the DS_001082C8 difficulty
+ * multiplier.
+ * PORT: the raw's first path returns at 0x39789 (`jmp 0x3982C`), before both
+ * later adjustments; the port as first written fell through them (the C3c row
+ * corrected it, record §C3c). */
 s32 fighter_39738(u32 side, s32 b)
 {
     PR_SEAM_RET(0x39738u, side, (u32)b);
@@ -6069,8 +6074,9 @@ s32 fighter_39738(u32 side, s32 b)
         s32 k = (s32)DSD(DS_00107D2A + side * 2u) >> 16;    /* 0x3975D/0x39763 */
         v = (k > 0xB) ? (s32)DSD(DS_000BEC84)               /* 0x39774 */
                       : (s32)DSD(DS_000BEC58 + (u32)k * 4u); /* 0x3976B */
-        v = (s32)((u32)v * (u32)b) / 100;                   /* 0x3977A/0x39787 */
-    } else {
+        return (s32)((u32)v * (u32)b) / 100;                /* 0x3977A/0x39787/0x39789 */
+    }
+    {
         s32 k = (s32)DSD(DS_00107D2A + side * 2u) >> 16;    /* 0x3978E/0x39794 */
         if (k > 0xB) {                                      /* 0x39797 */
             s32 k2 = (s32)DSD(DS_00107D1E + side * 2u) >> 16;   /* 0x397A5/0x397AB */
diff --git a/port/src/game/fighter.h b/port/src/game/fighter.h
index 8517e8e..da756e1 100644
--- a/port/src/game/fighter.h
+++ b/port/src/game/fighter.h
@@ -49,6 +49,12 @@ int fighter_actor_bit15_clear(u32 side);
  * the side's sprite id lies in the character's range or is 0x1E1. */
 int fighter_18460(u32 side);
 
+/* 0x18428 (C3c). The effect-free per-character dispatch 0x18460 ends in: read
+ * the side's slot character and, at most, jump through the seven-entry table
+ * 0x1840C whose entries are all the RET at 0x18408. Seamed and exported for
+ * its own row; the raw writes nothing. */
+void fighter_18428(u32 side, u32 sprite, u32 a0, u32 a1);
+
 /* 0x3C570. Test-and-set bit `bit` of DS_00107EE0: 1 when it was already set,
  * else set it and return 0. The camera page tails test bits 0..3 and
  * fighter_pass_b bit 5; fight_slot_clear (0x3C5CC) clears the word per frame. */
diff --git a/port/src/platform/audio/ail.c b/port/src/platform/audio/ail.c
index b0181ef..18028dc 100644
--- a/port/src/platform/audio/ail.c
+++ b/port/src/platform/audio/ail.c
@@ -494,9 +494,12 @@ void AIL_set_sequence_volume(HSEQUENCE sequence, s32 volume, u32 fade_ms)
     seq_fade_sequence_volume(volume, (s32)fade_ms);
 }
 
-/* 0x5deed — spec audio.md "AIL surface" (row 31). */
+/* 0x5deed — spec audio.md "AIL surface" (row 31). The seam (C3c) records
+ * snd_music_playing's call; the handle is a host pointer, so the stub takes no
+ * mem[] argument (record C3b §C3b.5's AIL treatment). */
 s32 AIL_sequence_status(HSEQUENCE sequence)
 {
+    PR_SEAM_RET0(0x5DEEDu);
     if (sequence == NULL || !sequence->used)
         return 0;
     return seq_playing() ? 4 : 2;
diff --git a/port/src/platform/render.c b/port/src/platform/render.c
index 9d61e20..3e93ea3 100644
--- a/port/src/platform/render.c
+++ b/port/src/platform/render.c
@@ -52,7 +52,7 @@ static u16 node_layer(u32 node)
  * so equal layers keep their relative order (stable). This is the one place the
  * ordering rule lives: render_list_insert and render_list_sort both go through
  * it. `node` must be detached -- its next field is overwritten here. */
-static void render_splice(u32 *headp, u32 node)
+void render_splice(u32 *headp, u32 node)
 {
     u16 layer = node_layer(node);
     u32 prev = 0;
@@ -78,35 +78,58 @@ void render_list_init(void)
     render_count = 0;
 }
 
-int render_list_insert(u32 pset_off)
+/* 0x1C390 — record §50-D. Pops the free-list head: returns it and advances the
+ * head to its next. The raw dereferences the new head unconditionally; the
+ * port's render_list_insert keeps its pool-exhausted guard at the call site. */
+u32 render_pop_free(void)
 {
     u32 node = DSD(RENDER_FREE_HEAD);
-    if (node == 0) return 0;                    /* PORT: pool exhausted guard */
     DSD(RENDER_FREE_HEAD) = DSD(node);
+    return node;
+}
+
+int render_list_insert(u32 pset_off)
+{
+    u32 node = render_pop_free();               /* 0x1C390 */
+    if (node == 0) return 0;                    /* PORT: pool exhausted guard */
     DSD(node + 4) = pset_off;
     render_splice((u32 *)(mem + RENDER_LIST_HEAD), node);
     render_count++;
     return 1;
 }
 
-/* 0x1C458 — record §50-D. Searches the list for the node whose +4 is
- * `pset_off`; the port runs 0x1C3D0 in the same body. */
-/* 0x1C3D0 — record §50-D. The unlink half (EAX = the list head, EDX = the
- * node): splices the node out (0x1C3DD 0x1C3DF) and pushes it on the free list
- * DS_0010275C (0x1C3E1 0x1C3E7 0x1C3EC). */
-void render_list_remove(u32 pset_off)
+/* 0x1C458 — record §50-D. Finds the node whose +4 is `pset_off` in the list
+ * rooted at *headp; 0 when it is not there. */
+u32 render_find(u32 *headp, u32 pset_off)
+{
+    u32 cur = *headp;
+    while (cur != 0 && DSD(cur + 4) != pset_off) cur = DSD(cur);
+    return cur;
+}
+
+/* 0x1C3D0 — record §50-D. Unlinks `node` from the list rooted at *headp (a
+ * no-op when it is not there) and pushes it on the free-list head. */
+void render_unlink(u32 *headp, u32 node)
 {
     u32 prev = 0;
-    u32 cur  = DSD(RENDER_LIST_HEAD);
-    while (cur != 0 && DSD(cur + 4) != pset_off) {
+    u32 cur  = *headp;
+    while (cur != 0 && cur != node) {
         prev = cur;
         cur = DSD(cur);
     }
-    if (cur == 0) return;                       /* PORT: 0x1C458 returns null */
-    if (prev == 0) DSD(RENDER_LIST_HEAD) = DSD(cur);
+    if (cur == 0) return;
+    if (prev == 0) *headp = DSD(cur);
     else DSD(prev) = DSD(cur);
     DSD(cur) = DSD(RENDER_FREE_HEAD);
     DSD(RENDER_FREE_HEAD) = cur;
+}
+
+void render_list_remove(u32 pset_off)
+{
+    u32 *headp = (u32 *)(mem + RENDER_LIST_HEAD);
+    u32 node = render_find(headp, pset_off);    /* 0x1C458 */
+    if (node == 0) return;
+    render_unlink(headp, node);                 /* 0x1C3D0 */
     render_count--;
 }
 
diff --git a/port/src/platform/render.h b/port/src/platform/render.h
index 1ac32c2..b5bcd71 100644
--- a/port/src/platform/render.h
+++ b/port/src/platform/render.h
@@ -13,14 +13,26 @@
  * display list (head = 0). */
 void render_list_init(void);
 
-/* PORT: 0x1C390 (pop the free-list head) + 0x1C3A0 (splice before the first
- * node with a greater layer, stable). Returns 1 when pset_off was added and 0
- * when the pool is exhausted -- the original would dereference a null free
- * head, so the port guards it; nothing is corrupted either way. */
+/* 0x1C390. Pops and returns the free-list head (its next becomes the head). */
+u32 render_pop_free(void);
+
+/* 0x1C3A0. Splices `node` into the list rooted at *headp before the first node
+ * with a greater layer (stable for equal layers). */
+void render_splice(u32 *headp, u32 node);
+
+/* PORT: 0x1C390 + 0x1C3A0. Returns 1 when pset_off was added and 0 when the
+ * pool is exhausted -- the original would dereference a null free head, so the
+ * port guards it; nothing is corrupted either way. */
 int render_list_insert(u32 pset_off);
 
-/* PORT: 0x1C458 (find the node whose pset is pset_off) + 0x1C3D0 (unlink it and
- * return it to the free-list head). A pset_off not in the list is a no-op. */
+/* 0x1C458. The node whose +4 is pset_off in the list rooted at *headp, or 0. */
+u32 render_find(u32 *headp, u32 pset_off);
+
+/* 0x1C3D0. Unlinks `node` from the list rooted at *headp and pushes it on the
+ * free-list head (a no-op when `node` is not in the list). */
+void render_unlink(u32 *headp, u32 node);
+
+/* PORT: 0x1C458 + 0x1C3D0. A pset_off not in the list is a no-op. */
 void render_list_remove(u32 pset_off);
 
 /* PORT: 0x1C3FC. Restores ascending-by-layer order (stable) by detaching every
diff --git a/port/tests/diff_runner.c b/port/tests/diff_runner.c
index 4cbc329..0fd6b76 100644
--- a/port/tests/diff_runner.c
+++ b/port/tests/diff_runner.c
@@ -10298,6 +10298,514 @@ static void m_29db8_bep(const u32 *r, u32 *eax)        { c3b_w_core(r, eax, C3B_
 static void m_29db8_child(const u32 *r, u32 *eax)      { c3b_w_core(r, eax, C3B_W_CHILD); }
 static void m_29db8_high(const u32 *r, u32 *eax)       { c3b_w_core(r, eax, C3B_W_HIGH); }
 
+/* Track P batch C3c (record 2026-10-05-reverse-c3c): the frontier rows, part 3. */
+
+/* 0x1C390's mutants. */
+#define C3C_POP_HEAD 0x01u   /* the head is not advanced */
+#define C3C_POP_RET  0x02u   /* the new head is returned, not the popped node */
+#define C3C_POP_NEXT 0x04u   /* the next comes from node+4 */
+static u32 c3c_pop_core(u32 mut)
+{
+    u32 node = DSD(0x0010275Cu);
+    if (!(mut & C3C_POP_HEAD))
+        DSD(0x0010275Cu) = (mut & C3C_POP_NEXT) ? DSD(node + 4u) : DSD(node);
+    return (mut & C3C_POP_RET) ? DSD(0x0010275Cu) : node;
+}
+static void b_c3c_pop(const u32 *r, u32 *eax)      { (void)r; *eax = render_pop_free(); }
+static void m_c3c_pop_head(const u32 *r, u32 *eax) { (void)r; *eax = c3c_pop_core(C3C_POP_HEAD); }
+static void m_c3c_pop_ret(const u32 *r, u32 *eax)  { (void)r; *eax = c3c_pop_core(C3C_POP_RET); }
+static void m_c3c_pop_next(const u32 *r, u32 *eax) { (void)r; *eax = c3c_pop_core(C3C_POP_NEXT); }
+
+/* 0x1C3A0's mutants. */
+#define C3C_SP_LT    0x01u   /* the walk stops at the first equal layer */
+#define C3C_SP_NEXT  0x02u   /* node->next is not written */
+#define C3C_SP_HEAD  0x04u   /* *headp is not written on the head insert */
+#define C3C_SP_PREV  0x08u   /* prev->next is not written */
+#define C3C_SP_FIRST 0x10u   /* the node is always inserted at the head */
+static void c3c_splice_core(const u32 *r, u32 mut)
+{
+    u32 *headp = (u32 *)(mem + r[R_EAX]);
+    u32 node = r[R_EDX];
+    u32 layer = (u32)DSW(DSD(node + 4u) + 0x0Eu);
+    u32 prev = 0, cur;
+    if (mut & C3C_SP_FIRST) {
+        DSD(node) = *headp;
+        *headp = node;
+        return;
+    }
+    cur = *headp;
+    while (cur != 0) {
+        u32 cl = (u32)DSW(DSD(cur + 4u) + 0x0Eu);
+        if ((mut & C3C_SP_LT) ? (cl < layer) : (cl <= layer)) { prev = cur; cur = DSD(cur); }
+        else break;
+    }
+    if (!(mut & C3C_SP_NEXT)) DSD(node) = cur;
+    if (prev == 0) { if (!(mut & C3C_SP_HEAD)) *headp = node; }
+    else if (!(mut & C3C_SP_PREV)) DSD(prev) = node;
+}
+static void b_c3c_splice(const u32 *r, u32 *eax)
+{ render_splice((u32 *)(mem + r[R_EAX]), r[R_EDX]); *eax = 0u; }
+static void m_c3c_sp_lt(const u32 *r, u32 *eax)     { c3c_splice_core(r, C3C_SP_LT); *eax = 0u; }
+static void m_c3c_sp_next(const u32 *r, u32 *eax)   { c3c_splice_core(r, C3C_SP_NEXT); *eax = 0u; }
+static void m_c3c_sp_head(const u32 *r, u32 *eax)   { c3c_splice_core(r, C3C_SP_HEAD); *eax = 0u; }
+static void m_c3c_sp_prev(const u32 *r, u32 *eax)   { c3c_splice_core(r, C3C_SP_PREV); *eax = 0u; }
+static void m_c3c_sp_first(const u32 *r, u32 *eax)  { c3c_splice_core(r, C3C_SP_FIRST); *eax = 0u; }
+
+/* 0x1C458's mutants. */
+#define C3C_FIND_CMP  0x01u  /* the compare reads node+0 */
+#define C3C_FIND_SKIP 0x02u  /* the head is returned without scanning */
+#define C3C_FIND_LAST 0x04u  /* a match is not returned */
+#define C3C_FIND_NULL 0x08u  /* always returns 0 */
+static u32 c3c_find_core(const u32 *r, u32 mut)
+{
+    u32 *headp = (u32 *)(mem + r[R_EAX]);
+    u32 pset = r[R_EDX];
+    u32 cur;
+    if (mut & C3C_FIND_NULL) return 0u;
+    cur = *headp;
+    if (mut & C3C_FIND_SKIP) return cur;
+    while (cur != 0) {
+        u32 f = (mut & C3C_FIND_CMP) ? DSD(cur) : DSD(cur + 4u);
+        if (f == pset && !(mut & C3C_FIND_LAST)) return cur;
+        cur = DSD(cur);
+    }
+    return 0u;
+}
+static void b_c3c_find(const u32 *r, u32 *eax)
+{ *eax = render_find((u32 *)(mem + r[R_EAX]), r[R_EDX]); }
+static void m_c3c_find_cmp(const u32 *r, u32 *eax)  { *eax = c3c_find_core(r, C3C_FIND_CMP); }
+static void m_c3c_find_skip(const u32 *r, u32 *eax) { *eax = c3c_find_core(r, C3C_FIND_SKIP); }
+static void m_c3c_find_last(const u32 *r, u32 *eax) { *eax = c3c_find_core(r, C3C_FIND_LAST); }
+static void m_c3c_find_null(const u32 *r, u32 *eax) { *eax = c3c_find_core(r, C3C_FIND_NULL); }
+
+/* 0x1C3D0's mutants. */
+#define C3C_UNL_HEAD  0x01u  /* the head link is updated for every unlink */
+#define C3C_UNL_FREE  0x02u  /* the free-list push is skipped */
+#define C3C_UNL_CHAIN 0x04u  /* node->next is not chained to the old free head */
+#define C3C_UNL_PREV  0x08u  /* the previous node is not tracked */
+#define C3C_UNL_NOOP  0x10u  /* nothing is done */
+static void c3c_unlink_core(const u32 *r, u32 mut)
+{
+    u32 *headp = (u32 *)(mem + r[R_EAX]);
+    u32 node = r[R_EDX];
+    u32 prev = 0, cur;
+    if (mut & C3C_UNL_NOOP) return;
+    cur = *headp;
+    while (cur != 0 && cur != node) {
+        if (!(mut & C3C_UNL_PREV)) prev = cur;
+        cur = DSD(cur);
+    }
+    if (cur == 0) return;
+    if (prev == 0 || (mut & C3C_UNL_HEAD)) *headp = DSD(cur);
+    else DSD(prev) = DSD(cur);
+    if (!(mut & C3C_UNL_FREE)) {
+        if (!(mut & C3C_UNL_CHAIN)) DSD(cur) = DSD(0x0010275Cu);
+        DSD(0x0010275Cu) = cur;
+    }
+}
+static void b_c3c_unlink(const u32 *r, u32 *eax)
+{ render_unlink((u32 *)(mem + r[R_EAX]), r[R_EDX]); *eax = 0u; }
+static void m_c3c_unl_head(const u32 *r, u32 *eax)  { c3c_unlink_core(r, C3C_UNL_HEAD); *eax = 0u; }
+static void m_c3c_unl_free(const u32 *r, u32 *eax)  { c3c_unlink_core(r, C3C_UNL_FREE); *eax = 0u; }
+static void m_c3c_unl_chain(const u32 *r, u32 *eax) { c3c_unlink_core(r, C3C_UNL_CHAIN); *eax = 0u; }
+static void m_c3c_unl_prev(const u32 *r, u32 *eax)  { c3c_unlink_core(r, C3C_UNL_PREV); *eax = 0u; }
+static void m_c3c_unl_noop(const u32 *r, u32 *eax)  { c3c_unlink_core(r, C3C_UNL_NOOP); *eax = 0u; }
+
+/* 0x127C0's mutants. The list calls go through the exported actors.c helpers, so the seams record
+ * them exactly as the port function's own calls do. */
+#define C3C_T01_EMPTY  0x01u  /* the sentinel-self check is dropped */
+#define C3C_T01_RET    0x02u  /* the empty path returns 0 */
+#define C3C_T01_LINK   0x04u  /* node+8 is not set */
+#define C3C_T01_REC    0x08u  /* rec+0x14 is not set */
+#define C3C_T01_AT     0x10u  /* the node is re-inserted at 0xF0A78, not 0xF0AE0 */
+#define C3C_T01_UNLINK 0x20u  /* the pop's unlink is skipped */
+static u32 c3c_t01_core(const u32 *r, u32 mut)
+{
+    u32 rec = r[R_EAX];
+    u32 node = DSD(0x000F0A78u);
+    if (!(mut & C3C_T01_EMPTY) && node == 0x000F0A78u) node = 0u;
+    if (node == 0u) return (mut & C3C_T01_RET) ? 0u : 0xFFu;
+    if (!(mut & C3C_T01_UNLINK)) list_unlink(node);
+    if (!(mut & C3C_T01_LINK)) DSD(node + 8u) = rec;
+    if (!(mut & C3C_T01_REC)) DSD(rec + 0x14u) = node;
+    list_insert_after((mut & C3C_T01_AT) ? 0x000F0A78u : 0x000F0AE0u, node);
+    return 0u;
+}
+static void b_c3c_type127c0(const u32 *r, u32 *eax)
+{ *eax = actor_type_127C0(r[R_EAX], r[R_EDX]); }
+static void m_c3c_t01_empty(const u32 *r, u32 *eax)  { *eax = c3c_t01_core(r, C3C_T01_EMPTY); }
+static void m_c3c_t01_ret(const u32 *r, u32 *eax)    { *eax = c3c_t01_core(r, C3C_T01_RET); }
+static void m_c3c_t01_link(const u32 *r, u32 *eax)   { *eax = c3c_t01_core(r, C3C_T01_LINK); }
+static void m_c3c_t01_rec(const u32 *r, u32 *eax)    { *eax = c3c_t01_core(r, C3C_T01_REC); }
+static void m_c3c_t01_at(const u32 *r, u32 *eax)     { *eax = c3c_t01_core(r, C3C_T01_AT); }
+static void m_c3c_t01_unlink(const u32 *r, u32 *eax) { *eax = c3c_t01_core(r, C3C_T01_UNLINK); }
+
+/* 0x12800's mutants. */
+#define C3C_T02_ZERO   0x01u  /* the null-node check is dropped */
+#define C3C_T02_CLEAR  0x02u  /* rec+0x14 is not cleared */
+#define C3C_T02_AT     0x04u  /* the node goes to 0xF0AE0, not 0xF0A78 */
+#define C3C_T02_UNLINK 0x08u  /* the unlink is skipped */
+#define C3C_T02_NODE   0x10u  /* the node comes from rec+0x18 */
+static void c3c_t02_core(const u32 *r, u32 mut)
+{
+    u32 rec = r[R_EAX];
+    u32 node = DSD(rec + ((mut & C3C_T02_NODE) ? 0x18u : 0x14u));
+    if (node == 0u && !(mut & C3C_T02_ZERO)) return;
+    if (!(mut & C3C_T02_UNLINK)) list_unlink(node);
+    list_insert_after((mut & C3C_T02_AT) ? 0x000F0AE0u : 0x000F0A78u, DSD(rec + 0x14u));
+    if (!(mut & C3C_T02_CLEAR)) DSD(rec + 0x14u) = 0u;
+}
+static void b_c3c_type12800(const u32 *r, u32 *eax)
+{ actor_type_12800(r[R_EAX]); *eax = 0u; }
+static void m_c3c_t02_zero(const u32 *r, u32 *eax)   { c3c_t02_core(r, C3C_T02_ZERO); *eax = 0u; }
+static void m_c3c_t02_clear(const u32 *r, u32 *eax)  { c3c_t02_core(r, C3C_T02_CLEAR); *eax = 0u; }
+static void m_c3c_t02_at(const u32 *r, u32 *eax)     { c3c_t02_core(r, C3C_T02_AT); *eax = 0u; }
+static void m_c3c_t02_unlink(const u32 *r, u32 *eax) { c3c_t02_core(r, C3C_T02_UNLINK); *eax = 0u; }
+static void m_c3c_t02_node(const u32 *r, u32 *eax)   { c3c_t02_core(r, C3C_T02_NODE); *eax = 0u; }
+
+/* 0x18460's mutants. The row has no return-value observers (mask 0), so every mutant is caught by
+ * the recorded 0x18428 call (its presence or its arguments). */
+#define C3C_46_NULL   0x01u  /* the slot-0 null check is dropped */
+#define C3C_46_NULL2  0x02u  /* the slot-1 null check is dropped */
+#define C3C_46_LO     0x04u  /* lo reads the hi word */
+#define C3C_46_HI     0x08u  /* hi reads the lo word */
+#define C3C_46_RANGE  0x10u  /* every character is range-tested */
+#define C3C_46_E1     0x20u  /* the 0x1E1 special id is not excluded */
+#define C3C_46_CALL   0x40u  /* the 0x18428 call is skipped */
+#define C3C_46_ID     0x80u  /* the sprite id is not masked with 0x7FFF */
+static void c3c_46_core(const u32 *r, u32 mut)
+{
+    u32 side = r[R_EAX];
+    u32 slot, ch, idx, lo = 0u, hi = 0u, id;
+    int ranged = 0;
+    u8 a0, a1;
+    if (!(mut & C3C_46_NULL) && DSD(0x001077A8u) == 0u) return;
+    if (!(mut & C3C_46_NULL2) && DSD(0x001077ACu) == 0u) return;
+    slot = DSD(0x001077A8u + side * 4u);
+    ch = (u32)DSB(slot + 0x7Au);
+    if ((mut & C3C_46_RANGE) || ch <= 6u) {
+        lo = (u32)DSW(((mut & C3C_46_LO) ? 0x000A1776u : 0x000A1774u) + ch * 14u);
+        hi = (u32)DSW(((mut & C3C_46_HI) ? 0x000A1774u : 0x000A1776u) + ch * 14u);
+        ranged = 1;
+    }
+    a0 = DSB(DSD(DSD(0x001077A8u)) + 0x56u);
+    a1 = DSB(DSD(DSD(0x001077ACu)) + 0x56u);
+    idx = (u32)DSW(DSD(slot) + 0x56u);
+    id = (u32)DSW(DSD(0x001014ECu) + idx * 0x20u) & ((mut & C3C_46_ID) ? 0xFFFFu : 0x7FFFu);
+    if (ranged && id >= lo && id < hi) return;
+    if (id == 0x1E1u && !(mut & C3C_46_E1)) return;
+    if (mut & C3C_46_CALL) return;
+    fighter_18428(side, id, (u32)a0, (u32)a1);
+}
+static void b_c3c_18428(const u32 *r, u32 *eax)
+{ fighter_18428(r[R_EAX], r[R_EDX], r[R_EBX], r[R_ECX]); *eax = 0u; }
+static void b_c3c_18460(const u32 *r, u32 *eax) { *eax = (u32)fighter_18460(r[R_EAX]); }
+static void m_c3c_46_null(const u32 *r, u32 *eax)   { c3c_46_core(r, C3C_46_NULL); *eax = 0u; }
+static void m_c3c_46_null2(const u32 *r, u32 *eax)  { c3c_46_core(r, C3C_46_NULL2); *eax = 0u; }
+static void m_c3c_46_lo(const u32 *r, u32 *eax)     { c3c_46_core(r, C3C_46_LO); *eax = 0u; }
+static void m_c3c_46_hi(const u32 *r, u32 *eax)     { c3c_46_core(r, C3C_46_HI); *eax = 0u; }
+static void m_c3c_46_range(const u32 *r, u32 *eax)  { c3c_46_core(r, C3C_46_RANGE); *eax = 0u; }
+static void m_c3c_46_e1(const u32 *r, u32 *eax)     { c3c_46_core(r, C3C_46_E1); *eax = 0u; }
+static void m_c3c_46_call(const u32 *r, u32 *eax)   { c3c_46_core(r, C3C_46_CALL); *eax = 0u; }
+static void m_c3c_46_id(const u32 *r, u32 *eax)     { c3c_46_core(r, C3C_46_ID); *eax = 0u; }
+
+/* 0x367DC's mutants. */
+#define C3C_D_MODE   0x01u  /* the mode test is dropped (the second call always runs) */
+#define C3C_D_STREAM 0x02u  /* the first stream comes from 0xC8A18 */
+#define C3C_D_CH     0x04u  /* the first stream indexes by rec+0x51 */
+#define C3C_D_CLR4C 0x08u   /* rec+0x4C is not cleared */
+#define C3C_D_CLR53 0x10u   /* slot+0x53 is not cleared */
+#define C3C_D_FF     0x20u   /* the 0xFF stores become 0x0F */
+#define C3C_D_MASK   0x40u   /* the slot+0x40 mask is 0xFFFFFF00 */
+#define C3C_D_CALL   0x80u   /* the first call's stream is off by one */
+static void c3c_367dc_core(const u32 *r, u32 mut)
+{
+    u32 slot = r[R_EAX], rec = r[R_EDX];
+    u32 ch = (mut & C3C_D_CH) ? (u32)DSB(rec + 0x51u) : (u32)DSB(slot + 0x7Au);
+    u32 stream = DSD(((mut & C3C_D_STREAM) ? 0x000C8A18u : 0x000C8950u) + ch * 4u);
+    if (mut & C3C_D_CALL) stream += 1u;
+    actors_anim_begin(rec, stream, 0x40400000u);
+    if (!(mut & C3C_D_CLR4C)) DSB(rec + 0x4Cu) = 0;
+    DSB(rec + 0x4Du) = 0x1Eu;
+    DSB(slot + 0x52u) = 0;
+    if (!(mut & C3C_D_CLR53)) DSB(slot + 0x53u) = 0;
+    DSB(slot + 0x5Fu) = (mut & C3C_D_FF) ? 0x0Fu : 0xFFu;
+    DSB(slot + 0x55u) = (mut & C3C_D_FF) ? 0x0Fu : 0xFFu;
+    DSB(slot + 0x54u) = 0;
+    DSD(slot + 0x40u) &= (mut & C3C_D_MASK) ? 0xFFFFFF00u : 0xCCF7BFFFu;
+    if ((mut & C3C_D_MODE) || (DSW(0x00104B00u) != 3u && DSW(0x00104B00u) != 0x22u
+                              && DSW(0x00104B00u) != 0x24u)) {
+        actors_anim_begin(DSD(0x00102900u + (u32)DSB(rec + 0x51u) * 4u), 0x000E906Au, 0x3F800000u);
+    }
+}
+static void b_c3c_367dc(const u32 *r, u32 *eax)
+{ fighter_state_367dc(r[R_EAX], r[R_EDX]); *eax = 0u; }
+static void m_c3c_d_mode(const u32 *r, u32 *eax)   { c3c_367dc_core(r, C3C_D_MODE); *eax = 0u; }
+static void m_c3c_d_stream(const u32 *r, u32 *eax) { c3c_367dc_core(r, C3C_D_STREAM); *eax = 0u; }
+static void m_c3c_d_ch(const u32 *r, u32 *eax)     { c3c_367dc_core(r, C3C_D_CH); *eax = 0u; }
+static void m_c3c_d_clr4c(const u32 *r, u32 *eax)  { c3c_367dc_core(r, C3C_D_CLR4C); *eax = 0u; }
+static void m_c3c_d_clr53(const u32 *r, u32 *eax)  { c3c_367dc_core(r, C3C_D_CLR53); *eax = 0u; }
+static void m_c3c_d_ff(const u32 *r, u32 *eax)     { c3c_367dc_core(r, C3C_D_FF); *eax = 0u; }
+static void m_c3c_d_mask(const u32 *r, u32 *eax)   { c3c_367dc_core(r, C3C_D_MASK); *eax = 0u; }
+static void m_c3c_d_call(const u32 *r, u32 *eax)   { c3c_367dc_core(r, C3C_D_CALL); *eax = 0u; }
+
+/* 0x1CA40's mutants. */
+#define C3C_MP_ZERO 0x01u  /* always 0 */
+#define C3C_MP_ONE  0x02u  /* always 1 */
+#define C3C_MP_GATE 0x04u  /* the zero-handle gate is dropped */
+#define C3C_MP_EQ3  0x08u  /* the playing status is 3, not 4 */
+#define C3C_MP_CALL 0x10u  /* the status call is skipped */
+#define C3C_MP_EAX  0x20u  /* the compare is on AL, not EAX */
+static u32 c3c_mp_core(u32 mut)
+{
+    if (mut & C3C_MP_ZERO) return 0u;
+    if (mut & C3C_MP_ONE) return 1u;
+    if (DSD(0x001028C0u) == 0u && !(mut & C3C_MP_GATE)) return 0u;
+    if (mut & C3C_MP_CALL) return 0u;
+    {
+        s32 st = AIL_sequence_status(NULL);
+        if (mut & C3C_MP_EAX) return ((u8)st == 4u) ? 1u : 0u;
+        return (st == ((mut & C3C_MP_EQ3) ? 3 : 4)) ? 1u : 0u;
+    }
+}
+static void b_c3c_music_playing(const u32 *r, u32 *eax) { (void)r; *eax = snd_music_playing(); }
+static void m_c3c_mp_zero(const u32 *r, u32 *eax) { (void)r; *eax = c3c_mp_core(C3C_MP_ZERO); }
+static void m_c3c_mp_one(const u32 *r, u32 *eax)  { (void)r; *eax = c3c_mp_core(C3C_MP_ONE); }
+static void m_c3c_mp_gate(const u32 *r, u32 *eax) { (void)r; *eax = c3c_mp_core(C3C_MP_GATE); }
+static void m_c3c_mp_eq3(const u32 *r, u32 *eax)  { (void)r; *eax = c3c_mp_core(C3C_MP_EQ3); }
+static void m_c3c_mp_call(const u32 *r, u32 *eax) { (void)r; *eax = c3c_mp_core(C3C_MP_CALL); }
+static void m_c3c_mp_eax(const u32 *r, u32 *eax)  { (void)r; *eax = c3c_mp_core(C3C_MP_EAX); }
+
+/* 0x33714/0x33734's mutants (the append with the record's flag byte). */
+#define C3C_PR_FLAG  0x01u  /* the flag byte is wrong */
+#define C3C_PR_ORDER 0x02u  /* first and count are swapped */
+#define C3C_PR_ADV   0x04u  /* the head is not advanced */
+#define C3C_PR_WIDE  0x08u  /* the flag is a dword store */
+#define C3C_PR_SWAP  0x10u  /* EAX and EDX are swapped */
+static void c3c_pr_impl(const u32 *r, u32 *eax, u32 flag, u32 mut)
+{
+    u32 head = DSD(0x00107798u);
+    u32 first = (mut & C3C_PR_SWAP) ? r[R_EDX] : r[R_EAX];
+    u32 count = (mut & C3C_PR_SWAP) ? r[R_EAX] : r[R_EDX];
+    if (mut & C3C_PR_FLAG) flag ^= 1u;
+    DSD(head + 0u) = r[R_EBX];
+    DSD(head + 4u) = (mut & C3C_PR_ORDER) ? count : first;
+    DSD(head + 8u) = (mut & C3C_PR_ORDER) ? first : count;
+    if (mut & C3C_PR_WIDE) DSD(head + 12u) = flag & 0xFFFFFFFFu;
+    else DSB(head + 12u) = (u8)flag;
+    if (!(mut & C3C_PR_ADV)) DSD(0x00107798u) = head + 16u;
+    *eax = 0u;
+}
+static void b_c3c_prf(const u32 *r, u32 *eax)
+{ palette_record_flagged(r[R_EBX], r[R_EAX], r[R_EDX]); *eax = 0u; }
+static void b_c3c_pr(const u32 *r, u32 *eax)
+{ palette_record(r[R_EBX], r[R_EAX], r[R_EDX], 0u); *eax = 0u; }
+static void m_c3c_prf_flag(const u32 *r, u32 *eax)  { c3c_pr_impl(r, eax, 1u, C3C_PR_FLAG); }
+static void m_c3c_prf_order(const u32 *r, u32 *eax) { c3c_pr_impl(r, eax, 1u, C3C_PR_ORDER); }
+static void m_c3c_prf_adv(const u32 *r, u32 *eax)   { c3c_pr_impl(r, eax, 1u, C3C_PR_ADV); }
+static void m_c3c_prf_wide(const u32 *r, u32 *eax)  { c3c_pr_impl(r, eax, 1u, C3C_PR_WIDE); }
+static void m_c3c_prf_swap(const u32 *r, u32 *eax)  { c3c_pr_impl(r, eax, 1u, C3C_PR_SWAP); }
+static void m_c3c_pr_flag(const u32 *r, u32 *eax)   { c3c_pr_impl(r, eax, 0u, C3C_PR_FLAG); }
+static void m_c3c_pr_order(const u32 *r, u32 *eax)  { c3c_pr_impl(r, eax, 0u, C3C_PR_ORDER); }
+static void m_c3c_pr_adv(const u32 *r, u32 *eax)    { c3c_pr_impl(r, eax, 0u, C3C_PR_ADV); }
+static void m_c3c_pr_wide(const u32 *r, u32 *eax)   { c3c_pr_impl(r, eax, 0u, C3C_PR_WIDE); }
+static void m_c3c_pr_swap(const u32 *r, u32 *eax)   { c3c_pr_impl(r, eax, 0u, C3C_PR_SWAP); }
+
+/* 0x2BD44's mutants. */
+#define C3C_BD_SRC   0x01u  /* the copied byte comes from param_1 */
+#define C3C_BD_CLR24 0x02u  /* param_2+0x24 is not cleared */
+#define C3C_BD_A2A   0x04u  /* the +0x2A mask is 0xEF */
+#define C3C_BD_O29   0x08u  /* param_2+0x29 bit 3 is not set */
+#define C3C_BD_A28   0x10u  /* the +0x28 mask is 0xC7 */
+#define C3C_BD_ID    0x20u  /* the +8 id is a word store */
+#define C3C_BD_IDX   0x40u  /* the actor index scales by 0x10 */
+static void c3c_2bd44_core(const u32 *r, u32 mut)
+{
+    u32 p1 = r[R_EAX], p2 = r[R_EDX];
+    u32 actor;
+    DSB(p1 + 0x4Bu) = DSB(((mut & C3C_BD_SRC) ? p1 : p2) + 0x4Bu);
+    if (!(mut & C3C_BD_CLR24)) DSD(p2 + 0x24u) = 0u;
+    DSB(p2 + 0x2Au) &= (mut & C3C_BD_A2A) ? 0xEFu : 0xF7u;
+    if (!(mut & C3C_BD_O29)) DSB(p2 + 0x29u) |= 8u;
+    DSB(p2 + 0x28u) &= (mut & C3C_BD_A28) ? 0xC7u : 0xEBu;
+    if (mut & C3C_BD_ID) DSW(p2 + 0x08u) = 0x01E1u;
+    else DSD(p2 + 0x08u) = 0x1E1u;
+    actor = DSD(0x001014ECu) + (u32)DSW(p2 + 0x56u) * ((mut & C3C_BD_IDX) ? 0x10u : 0x20u);
+    DSW(actor) = (u16)anim_next_sprite_id(p2, actor);
+    actor_set_dead(p2);
+}
+static void b_c3c_2bd44(const u32 *r, u32 *eax)
+{ fighter_2bd44(r[R_EAX], r[R_EDX]); *eax = 0u; }
+static void m_c3c_bd_src(const u32 *r, u32 *eax)   { c3c_2bd44_core(r, C3C_BD_SRC); *eax = 0u; }
+static void m_c3c_bd_clr24(const u32 *r, u32 *eax) { c3c_2bd44_core(r, C3C_BD_CLR24); *eax = 0u; }
+static void m_c3c_bd_a2a(const u32 *r, u32 *eax)   { c3c_2bd44_core(r, C3C_BD_A2A); *eax = 0u; }
+static void m_c3c_bd_o29(const u32 *r, u32 *eax)   { c3c_2bd44_core(r, C3C_BD_O29); *eax = 0u; }
+static void m_c3c_bd_a28(const u32 *r, u32 *eax)   { c3c_2bd44_core(r, C3C_BD_A28); *eax = 0u; }
+static void m_c3c_bd_id(const u32 *r, u32 *eax)    { c3c_2bd44_core(r, C3C_BD_ID); *eax = 0u; }
+static void m_c3c_bd_idx(const u32 *r, u32 *eax)   { c3c_2bd44_core(r, C3C_BD_IDX); *eax = 0u; }
+
+/* 0x3B6C4's mutants. The 0x1A570 calls go through the port's exported predicate, so the seams
+ * record them exactly as the port function's own calls do. */
+#define C3C_B6_S53  0x01u  /* the +0x53 compare is 9 */
+#define C3C_B6_S54  0x02u  /* the +0x54 compare is 3 */
+#define C3C_B6_O54  0x04u  /* the other +0x54 == 2 gate is dropped */
+#define C3C_B6_CMP  0x08u  /* the two predicates must differ instead of agree */
+#define C3C_B6_RET  0x10u  /* the predicate always returns 0 */
+#define C3C_B6_SIDE 0x20u  /* both predicates read side 0 */
+static u32 c3c_3b6c4_core(const u32 *r, u32 mut)
+{
+    u32 side = r[R_EAX];
+    u32 s2 = 0x001077B0u + side * 0x94u;
+    u32 s3 = 0x001077B0u + (1u - side) * 0x94u;
+    if (DSB(s2 + 0x53u) != ((mut & C3C_B6_S53) ? 9u : 8u)) return 0u;
+    if (DSB(s2 + 0x54u) != ((mut & C3C_B6_S54) ? 3u : 2u)) return 0u;
+    if (!(mut & C3C_B6_O54) && DSB(s3 + 0x54u) == 2u) return 0u;
+    {
+        u32 a = (mut & C3C_B6_SIDE) ? 0u : side;
+        u32 b = (mut & C3C_B6_SIDE) ? 0u : 1u - side;
+        int ca = fighter_actor_bit15_clear(a);
+        int cb = fighter_actor_bit15_clear(b);
+        if ((mut & C3C_B6_CMP) ? (ca == cb) : (ca != cb)) return 0u;
+    }
+    return (mut & C3C_B6_RET) ? 0u : 1u;
+}
+static void b_c3c_3b6c4(const u32 *r, u32 *eax) { *eax = (u32)fighter_3b6c4(r[R_EAX]); }
+static void m_c3c_b6_s53(const u32 *r, u32 *eax)  { *eax = c3c_3b6c4_core(r, C3C_B6_S53); }
+static void m_c3c_b6_s54(const u32 *r, u32 *eax)  { *eax = c3c_3b6c4_core(r, C3C_B6_S54); }
+static void m_c3c_b6_o54(const u32 *r, u32 *eax)  { *eax = c3c_3b6c4_core(r, C3C_B6_O54); }
+static void m_c3c_b6_cmp(const u32 *r, u32 *eax)  { *eax = c3c_3b6c4_core(r, C3C_B6_CMP); }
+static void m_c3c_b6_ret(const u32 *r, u32 *eax)  { *eax = c3c_3b6c4_core(r, C3C_B6_RET); }
+static void m_c3c_b6_side(const u32 *r, u32 *eax) { *eax = c3c_3b6c4_core(r, C3C_B6_SIDE); }
+
+/* 0x3C520's mutants. */
+#define C3C_C5_SIDE   0x01u  /* the anchors use side 0 */
+#define C3C_C5_XY     0x02u  /* the slot's x and y are swapped */
+#define C3C_C5_BEGIN  0x04u  /* the animation start is skipped */
+#define C3C_C5_BITS   0x08u  /* the frame bits are 3.0 */
+#define C3C_C5_ANCH   0x10u  /* the anchor pair is skipped */
+#define C3C_C5_STREAM 0x20u  /* the stream is off by one */
+static void c3c_3c520_core(const u32 *r, u32 mut)
+{
+    u32 rec = r[R_EAX], stream = r[R_EDX], bits = r[R_S0];
+    u32 ctx[6];
+    u32 slot, x, y, side;
+    hit_anim_ctx(ctx, rec);
+    side = (mut & C3C_C5_SIDE) ? 0u : ctx[0];
+    slot = 0x001077B0u + side * 0x94u;
+    x = DSD(slot + ((mut & C3C_C5_XY) ? 0x30u : 0x2Cu));
+    y = DSD(slot + ((mut & C3C_C5_XY) ? 0x2Cu : 0x30u));
+    if (!(mut & C3C_C5_BEGIN))
+        actors_anim_begin(rec, (mut & C3C_C5_STREAM) ? stream + 1u : stream,
+                          (mut & C3C_C5_BITS) ? 0x40400000u : bits);
+    if (!(mut & C3C_C5_ANCH)) {
+        hit_anchor_x(side, x);
+        hit_anchor_y(side, y);
+    }
+}
+static void b_c3c_3c520(const u32 *r, u32 *eax)
+{ hit_anim_start_c(r[R_EAX], r[R_EDX], r[R_S0]); *eax = 0u; }
+static void m_c3c_c5_side(const u32 *r, u32 *eax)   { c3c_3c520_core(r, C3C_C5_SIDE); *eax = 0u; }
+static void m_c3c_c5_xy(const u32 *r, u32 *eax)     { c3c_3c520_core(r, C3C_C5_XY); *eax = 0u; }
+static void m_c3c_c5_begin(const u32 *r, u32 *eax)  { c3c_3c520_core(r, C3C_C5_BEGIN); *eax = 0u; }
+static void m_c3c_c5_bits(const u32 *r, u32 *eax)   { c3c_3c520_core(r, C3C_C5_BITS); *eax = 0u; }
+static void m_c3c_c5_anch(const u32 *r, u32 *eax)   { c3c_3c520_core(r, C3C_C5_ANCH); *eax = 0u; }
+static void m_c3c_c5_stream(const u32 *r, u32 *eax) { c3c_3c520_core(r, C3C_C5_STREAM); *eax = 0u; }
+
+/* 0x1A734's mutants. */
+#define C3C_K_C61   0x01u  /* the +0x61 seed is 0x0B */
+#define C3C_K_CLAMP 0x02u  /* the clamp is skipped */
+#define C3C_K_B62   0x04u  /* the +0x62 gate is dropped */
+#define C3C_K_U     0x08u  /* the +0x60 compare is unsigned */
+#define C3C_K_ARM   0x10u  /* the bit tests are swapped */
+#define C3C_K_S54   0x20u  /* the first arm stores 1 */
+#define C3C_K_CALL  0x40u  /* the 0x3C480 call is skipped */
+#define C3C_K_TAB   0x80u  /* the two stream tables are swapped */
+static void c3c_1a734_core(const u32 *r, u32 mut)
+{
+    u32 side = r[R_EAX];
+    u32 slot = 0x001077B0u + side * 0x94u;
+    u32 rec = DSD(slot);
+    DSB(slot + 0x61u) = (mut & C3C_K_C61) ? 0x0Bu : 0x0Cu;
+    if (!(mut & C3C_K_CLAMP)) {
+        int over = (mut & C3C_K_U)
+            ? (u32)DSB(slot + 0x61u) > (u32)DSB(slot + 0x60u)
+            : (s32)(s8)DSB(slot + 0x61u) > (s32)(s8)DSB(slot + 0x60u);
+        if (over && ((mut & C3C_K_B62) || DSB(slot + 0x62u) != 0u))
+            DSB(slot + 0x61u) = DSB(slot + 0x60u);
+    }
+    if ((DSB(slot + 0x43u) & ((mut & C3C_K_ARM) ? 0x10u : 0x20u)) != 0u) {
+        DSB(slot + 0x54u) = (mut & C3C_K_S54) ? 1u : 0u;
+        if (!(mut & C3C_K_CALL))
+            hit_anim_start_a(rec, DSD(((mut & C3C_K_TAB) ? 0x000C8F90u : 0x000C8F40u)
+                                      + (u32)DSB(slot + 0x7Au) * 4u), 0x40400000u);
+    } else if ((DSB(slot + 0x43u) & 0x10u) != 0u) {
+        DSB(slot + 0x54u) = 1u;
+        if (!(mut & C3C_K_CALL))
+            hit_anim_start_a(rec, DSD(((mut & C3C_K_TAB) ? 0x000C8F40u : 0x000C8F90u)
+                                      + (u32)DSB(slot + 0x7Au) * 4u), 0x40400000u);
+    }
+}
+static void b_c3c_1a734(const u32 *r, u32 *eax) { fighter_block_hit(r[R_EAX]); *eax = 0u; }
+static void m_c3c_k_c61(const u32 *r, u32 *eax)   { c3c_1a734_core(r, C3C_K_C61); *eax = 0u; }
+static void m_c3c_k_clamp(const u32 *r, u32 *eax) { c3c_1a734_core(r, C3C_K_CLAMP); *eax = 0u; }
+static void m_c3c_k_b62(const u32 *r, u32 *eax)   { c3c_1a734_core(r, C3C_K_B62); *eax = 0u; }
+static void m_c3c_k_u(const u32 *r, u32 *eax)     { c3c_1a734_core(r, C3C_K_U); *eax = 0u; }
+static void m_c3c_k_arm(const u32 *r, u32 *eax)   { c3c_1a734_core(r, C3C_K_ARM); *eax = 0u; }
+static void m_c3c_k_s54(const u32 *r, u32 *eax)   { c3c_1a734_core(r, C3C_K_S54); *eax = 0u; }
+static void m_c3c_k_call(const u32 *r, u32 *eax)  { c3c_1a734_core(r, C3C_K_CALL); *eax = 0u; }
+static void m_c3c_k_tab(const u32 *r, u32 *eax)   { c3c_1a734_core(r, C3C_K_TAB); *eax = 0u; }
+
+/* 0x39738's mutants. */
+#define C3C_97_CHAN 0x01u  /* the side slot+0x8C gate is dropped */
+#define C3C_97_K    0x02u  /* k comes from 0x107D1E */
+#define C3C_97_TAB  0x04u  /* the tables are cross-read */
+#define C3C_97_K2   0x08u  /* the k2 compare is > 0x45 */
+#define C3C_97_DIV  0x10u  /* b/4 is a shift */
+#define C3C_97_CUT  0x20u  /* the other slot's 15% cut is skipped */
+#define C3C_97_DIFF 0x40u  /* the difficulty multiplier is skipped */
+#define C3C_97_IDX  0x80u  /* the difficulty index uses 1-side */
+static s32 c3c_39738_core(const u32 *r, u32 mut)
+{
+    u32 side = r[R_EAX];
+    s32 b = (s32)r[R_EDX];
+    u32 s = 0x001077B0u + side * 0x94u;
+    u32 o = 0x001077B0u + (1u - side) * 0x94u;
+    s32 k = (mut & C3C_97_K) ? ((s32)DSD(0x00107D1Eu + side * 2u) >> 16)
+                             : ((s32)DSD(0x00107D2Au + side * 2u) >> 16);
+    s32 v;
+    if ((mut & C3C_97_CHAN) || DSW(s + 0x8Cu) != 0u) {
+        v = (k > 0xB) ? (s32)DSD((mut & C3C_97_TAB) ? (0x000BEC58u + (u32)k * 4u)
+                                                    : 0x000BEC84u)
+                      : (s32)DSD(0x000BEC58u + (u32)k * 4u);
+        return (s32)((u32)v * (u32)b) / 100;    /* 0x39789 jmp 0x3982C */
+    }
+    v = (s32)((u32)DSD(0x000BEC28u + (u32)k * 4u) * (u32)b) / 100;
+    if (k > 0xB) {
+        s32 k2 = (s32)DSD(0x00107D1Eu + side * 2u) >> 16;
+        if (k2 > ((mut & C3C_97_K2) ? 0x45 : 0x46))
+            v = (mut & C3C_97_DIV) ? (s32)((u32)b >> 2) : (b / 4);
+        else
+            v = (s32)((u32)DSD((mut & C3C_97_TAB) ? (0x000BEC28u + (u32)k * 4u)
+                                                  : 0x000BEC54u) * (u32)b) / 100;
+    }
+    if (!(mut & C3C_97_CUT) && DSW(o + 0x8Cu) != 0u)
+        v -= (s32)((u32)v * 15u) / 100;
+    if (!(mut & C3C_97_DIFF) && DSB(s + 0x63u) != 0u) {
+        u32 d = DSD(0x001082C8u + ((mut & C3C_97_IDX) ? (1u - side) : side) * 4u);
+        v += (s32)((u32)DSD(0x000BEBD8u + d * 4u) * (u32)v) / 100;
+    }
+    return v;
+}
+static void b_c3c_39738(const u32 *r, u32 *eax) { *eax = (u32)fighter_39738(r[R_EAX], (s32)r[R_EDX]); }
+static void m_c3c_97_chan(const u32 *r, u32 *eax) { *eax = (u32)c3c_39738_core(r, C3C_97_CHAN); }
+static void m_c3c_97_k(const u32 *r, u32 *eax)    { *eax = (u32)c3c_39738_core(r, C3C_97_K); }
+static void m_c3c_97_tab(const u32 *r, u32 *eax)  { *eax = (u32)c3c_39738_core(r, C3C_97_TAB); }
+static void m_c3c_97_k2(const u32 *r, u32 *eax)   { *eax = (u32)c3c_39738_core(r, C3C_97_K2); }
+static void m_c3c_97_div(const u32 *r, u32 *eax)  { *eax = (u32)c3c_39738_core(r, C3C_97_DIV); }
+static void m_c3c_97_cut(const u32 *r, u32 *eax)  { *eax = (u32)c3c_39738_core(r, C3C_97_CUT); }
+static void m_c3c_97_diff(const u32 *r, u32 *eax) { *eax = (u32)c3c_39738_core(r, C3C_97_DIFF); }
+static void m_c3c_97_idx(const u32 *r, u32 *eax)  { *eax = (u32)c3c_39738_core(r, C3C_97_IDX); }
+
 static const binding_t k_bindings[] = {
     { "rng_next",                 b_rng_next,     0xFFFFFFFFu },
     { "fighter_slot_flag",        b_slot_flag,    0x000000FFu },
@@ -11317,6 +11825,118 @@ static const binding_t k_bindings[] = {
     { "fighter_anim_triple@o0",            m_3afc4_o0,     0x00000000u },
     { "fighter_anim_triple@o1",            m_3afc4_o1,     0x00000000u },
     { "fighter_anim_triple@o2",            m_3afc4_o2,     0x00000000u },
+    { "render_pop_free",                   b_c3c_pop,      0xFFFFFFFFu },
+    { "render_pop_free@head",              m_c3c_pop_head, 0xFFFFFFFFu },
+    { "render_pop_free@ret",               m_c3c_pop_ret,  0xFFFFFFFFu },
+    { "render_pop_free@next",              m_c3c_pop_next, 0xFFFFFFFFu },
+    { "render_splice",                     b_c3c_splice,   0x00000000u },
+    { "render_splice@lt",                  m_c3c_sp_lt,    0x00000000u },
+    { "render_splice@next",                m_c3c_sp_next,  0x00000000u },
+    { "render_splice@head",                m_c3c_sp_head,  0x00000000u },
+    { "render_splice@prev",                m_c3c_sp_prev,  0x00000000u },
+    { "render_splice@first",               m_c3c_sp_first, 0x00000000u },
+    { "render_find",                       b_c3c_find,     0xFFFFFFFFu },
+    { "render_find@cmp",                   m_c3c_find_cmp, 0xFFFFFFFFu },
+    { "render_find@skip",                  m_c3c_find_skip, 0xFFFFFFFFu },
+    { "render_find@last",                  m_c3c_find_last, 0xFFFFFFFFu },
+    { "render_find@null",                  m_c3c_find_null, 0xFFFFFFFFu },
+    { "render_unlink",                     b_c3c_unlink,   0x00000000u },
+    { "render_unlink@head",                m_c3c_unl_head, 0x00000000u },
+    { "render_unlink@free",                m_c3c_unl_free, 0x00000000u },
+    { "render_unlink@chain",               m_c3c_unl_chain, 0x00000000u },
+    { "render_unlink@prev",                m_c3c_unl_prev, 0x00000000u },
+    { "render_unlink@noop",                m_c3c_unl_noop, 0x00000000u },
+    { "actor_type_127C0",                  b_c3c_type127c0, 0x000000FFu },
+    { "actor_type_127C0@empty",            m_c3c_t01_empty, 0x000000FFu },
+    { "actor_type_127C0@ret",              m_c3c_t01_ret,  0x000000FFu },
+    { "actor_type_127C0@link",             m_c3c_t01_link, 0x000000FFu },
+    { "actor_type_127C0@rec",              m_c3c_t01_rec,  0x000000FFu },
+    { "actor_type_127C0@at",               m_c3c_t01_at,   0x000000FFu },
+    { "actor_type_127C0@unlink",           m_c3c_t01_unlink, 0x000000FFu },
+    { "actor_type_12800",                  b_c3c_type12800, 0x00000000u },
+    { "actor_type_12800@zero",             m_c3c_t02_zero, 0x00000000u },
+    { "actor_type_12800@clear",            m_c3c_t02_clear, 0x00000000u },
+    { "actor_type_12800@at",               m_c3c_t02_at,   0x00000000u },
+    { "actor_type_12800@unlink",           m_c3c_t02_unlink, 0x00000000u },
+    { "actor_type_12800@node",             m_c3c_t02_node, 0x00000000u },
+    { "fighter_18428",                     b_c3c_18428,    0x00000000u },
+    { "fighter_18460",                     b_c3c_18460,    0x00000000u },
+    { "fighter_18460@null",                m_c3c_46_null,  0x00000000u },
+    { "fighter_18460@null2",               m_c3c_46_null2, 0x00000000u },
+    { "fighter_18460@lo",                  m_c3c_46_lo,    0x00000000u },
+    { "fighter_18460@hi",                  m_c3c_46_hi,    0x00000000u },
+    { "fighter_18460@range",               m_c3c_46_range, 0x00000000u },
+    { "fighter_18460@e1",                  m_c3c_46_e1,    0x00000000u },
+    { "fighter_18460@call",                m_c3c_46_call,  0x00000000u },
+    { "fighter_18460@id",                  m_c3c_46_id,    0x00000000u },
+    { "fighter_state_367dc",               b_c3c_367dc,    0x00000000u },
+    { "fighter_state_367dc@mode",          m_c3c_d_mode,   0x00000000u },
+    { "fighter_state_367dc@stream",        m_c3c_d_stream, 0x00000000u },
+    { "fighter_state_367dc@ch",            m_c3c_d_ch,     0x00000000u },
+    { "fighter_state_367dc@clr4c",         m_c3c_d_clr4c,  0x00000000u },
+    { "fighter_state_367dc@clr53",         m_c3c_d_clr53,  0x00000000u },
+    { "fighter_state_367dc@ff",            m_c3c_d_ff,     0x00000000u },
+    { "fighter_state_367dc@mask",          m_c3c_d_mask,   0x00000000u },
+    { "fighter_state_367dc@call",          m_c3c_d_call,   0x00000000u },
+    { "snd_music_playing",                 b_c3c_music_playing, 0x000000FFu },
+    { "snd_music_playing@zero",            m_c3c_mp_zero,  0x000000FFu },
+    { "snd_music_playing@one",             m_c3c_mp_one,   0x000000FFu },
+    { "snd_music_playing@gate",            m_c3c_mp_gate,  0x000000FFu },
+    { "snd_music_playing@eq3",             m_c3c_mp_eq3,   0x000000FFu },
+    { "snd_music_playing@call",            m_c3c_mp_call,  0x000000FFu },
+    { "snd_music_playing@eax",             m_c3c_mp_eax,   0x000000FFu },
+    { "palette_record_flagged",            b_c3c_prf,      0x00000000u },
+    { "palette_record_flagged@flag",       m_c3c_prf_flag, 0x00000000u },
+    { "palette_record_flagged@order",      m_c3c_prf_order, 0x00000000u },
+    { "palette_record_flagged@adv",        m_c3c_prf_adv,  0x00000000u },
+    { "palette_record_flagged@wide",       m_c3c_prf_wide, 0x00000000u },
+    { "palette_record_flagged@swap",       m_c3c_prf_swap, 0x00000000u },
+    { "palette_record",                    b_c3c_pr,       0x00000000u },
+    { "palette_record@flag",               m_c3c_pr_flag,  0x00000000u },
+    { "palette_record@order",              m_c3c_pr_order, 0x00000000u },
+    { "palette_record@adv",                m_c3c_pr_adv,   0x00000000u },
+    { "palette_record@wide",               m_c3c_pr_wide,  0x00000000u },
+    { "palette_record@swap",               m_c3c_pr_swap,  0x00000000u },
+    { "fighter_2bd44",                     b_c3c_2bd44,    0x00000000u },
+    { "fighter_2bd44@src",                 m_c3c_bd_src,   0x00000000u },
+    { "fighter_2bd44@clr24",               m_c3c_bd_clr24, 0x00000000u },
+    { "fighter_2bd44@a2a",                 m_c3c_bd_a2a,   0x00000000u },
+    { "fighter_2bd44@o29",                 m_c3c_bd_o29,   0x00000000u },
+    { "fighter_2bd44@a28",                 m_c3c_bd_a28,   0x00000000u },
+    { "fighter_2bd44@id",                  m_c3c_bd_id,    0x00000000u },
+    { "fighter_2bd44@idx",                 m_c3c_bd_idx,   0x00000000u },
+    { "fighter_3b6c4",                     b_c3c_3b6c4,    0x000000FFu },
+    { "fighter_3b6c4@s53",                 m_c3c_b6_s53,   0x000000FFu },
+    { "fighter_3b6c4@s54",                 m_c3c_b6_s54,   0x000000FFu },
+    { "fighter_3b6c4@o54",                 m_c3c_b6_o54,   0x000000FFu },
+    { "fighter_3b6c4@cmp",                 m_c3c_b6_cmp,   0x000000FFu },
+    { "fighter_3b6c4@ret",                 m_c3c_b6_ret,   0x000000FFu },
+    { "fighter_3b6c4@side",                m_c3c_b6_side,  0x000000FFu },
+    { "hit_anim_start_c",                  b_c3c_3c520,    0x00000000u },
+    { "hit_anim_start_c@side",             m_c3c_c5_side,  0x00000000u },
+    { "hit_anim_start_c@xy",               m_c3c_c5_xy,    0x00000000u },
+    { "hit_anim_start_c@begin",            m_c3c_c5_begin, 0x00000000u },
+    { "hit_anim_start_c@bits",             m_c3c_c5_bits,  0x00000000u },
+    { "hit_anim_start_c@anchor",           m_c3c_c5_anch,  0x00000000u },
+    { "hit_anim_start_c@stream",           m_c3c_c5_stream, 0x00000000u },
+    { "fighter_block_hit",                 b_c3c_1a734,    0x00000000u },
+    { "fighter_block_hit@c61",             m_c3c_k_c61,    0x00000000u },
+    { "fighter_block_hit@clamp",           m_c3c_k_clamp,  0x00000000u },
+    { "fighter_block_hit@b62",             m_c3c_k_b62,    0x00000000u },
+    { "fighter_block_hit@u",               m_c3c_k_u,      0x00000000u },
+    { "fighter_block_hit@arm",             m_c3c_k_arm,    0x00000000u },
+    { "fighter_block_hit@s54",             m_c3c_k_s54,    0x00000000u },
+    { "fighter_block_hit@call",            m_c3c_k_call,   0x00000000u },
+    { "fighter_block_hit@tab",             m_c3c_k_tab,    0x00000000u },
+    { "fighter_39738",                     b_c3c_39738,    0xFFFFFFFFu },
+    { "fighter_39738@chan",                m_c3c_97_chan,  0xFFFFFFFFu },
+    { "fighter_39738@k",                   m_c3c_97_k,     0xFFFFFFFFu },
+    { "fighter_39738@tab",                 m_c3c_97_tab,   0xFFFFFFFFu },
+    { "fighter_39738@k2",                  m_c3c_97_k2,    0xFFFFFFFFu },
+    { "fighter_39738@div",                 m_c3c_97_div,   0xFFFFFFFFu },
+    { "fighter_39738@cut",                 m_c3c_97_cut,   0xFFFFFFFFu },
+    { "fighter_39738@diff",                m_c3c_97_diff,  0xFFFFFFFFu },
+    { "fighter_39738@idx",                 m_c3c_97_idx,   0xFFFFFFFFu },
 };
 
 static const binding_t *find_binding(const char *name)
diff --git a/tools/diff_verify.py b/tools/diff_verify.py
index 743c04c..cc2c7e7 100644
--- a/tools/diff_verify.py
+++ b/tools/diff_verify.py
@@ -5224,6 +5224,365 @@ C3B_SPECS = [
        eax_mask=0, mutants=("@idx", "@shift", "@add", "@o0", "@o1", "@o2")),
 ]
 
+# ---- track P batch C3c (record 2026-10-05-reverse-c3c): the frontier rows, part 3 ----------------
+#
+# The render-list halves (0x1C390/0x1C3A0/0x1C458/0x1C3D0), the type-0x01 callbacks
+# (0x127C0/0x12800) and the new frontier items (0x18428/0x18460/0x367DC/0x1CA40/0x33714/0x33734).
+# The four render rows share this scratch: nodes are { next; pset } and the layer is pset+0x0E.
+C3C_R_FREE = 0x0010275C         # the render free-list head
+C3C_R_LIST = 0x00105B44         # the render list head
+C3C_R_HEAD = 0x10AF00           # a scratch list head for the splice/find/unlink rows
+C3C_R_N1, C3C_R_N2, C3C_R_N3, C3C_R_N4 = 0x10AF40, 0x10AF80, 0x10AFC0, 0x10B000
+C3C_R_P1, C3C_R_P2, C3C_R_P3, C3C_R_P4 = 0x10B040, 0x10B080, 0x10B0A0, 0x10B0B0
+C3C_MP_HUB = 0x10AF00           # the type-callback hub scratch
+
+
+def c3c_r_node(nxt, pset, layer):
+    return {nxt[0]: le32(nxt[1]), nxt[0] + 4: le32(pset), pset + 0x0E: le16(layer)}
+
+
+def c3c_r_list(head, items):
+    """Pokes linking `items` = [(node, pset, layer)] from [head]; the last next is 0."""
+    p = {head: le32(items[0][0] if items else 0)}
+    for i, (n, ps, ly) in enumerate(items):
+        p[n] = le32(items[i + 1][0] if i + 1 < len(items) else 0)
+        p[n + 4] = le32(ps)
+        p[ps + 0x0E] = le16(ly)
+    return p
+
+
+def c3c_b6_case(side, s53, s54, o54, bit15_0, bit15_1):
+    """0x3B6C4's slots: the side's +0x53/+0x54, the other's +0x54 and the two actor bit-15 words."""
+    rec0, rec1 = 0x10AF00, 0x10AF40
+    p = {0x001077B0: le32(rec0), 0x001077B0 + 0x94: le32(rec1),
+         rec0 + 0x56: le16(1), rec1 + 0x56: le16(2),
+         0x001014EC: le32(0x10B000),
+         0x10B020: le16(bit15_0 << 15), 0x10B040: le16(bit15_1 << 15),
+         0x001077B0 + side * 0x94 + 0x53: bytes([s53]),
+         0x001077B0 + side * 0x94 + 0x54: bytes([s54]),
+         0x001077B0 + (1 - side) * 0x94 + 0x54: bytes([o54])}
+    return p
+
+
+def c3c_k_case(side, b60, b62, b43, ch):
+    """0x1A734's side slot and its record (EAX = side; ch selects the 0xC8F40/0xC8F90 stream)."""
+    slot = 0x001077B0 + side * 0x94
+    rec = 0x10AF00 + side * 0x40
+    return {slot: le32(rec), slot + 0x60: bytes([b60]), slot + 0x62: bytes([b62]),
+            slot + 0x43: bytes([b43]), slot + 0x54: b"\xAA", slot + 0x7A: bytes([ch])}
+
+
+def c3c_97_case(side, self8c, other8c, k, b, k2=0, s63=0, diff=0x5678, d=1):
+    """0x39738's state: the per-side 0x8C words, the slot+0x63 gate, the two k words, the two
+    12-entry per-character tables (0xBEC28/0xBEC58; their entries 11 are the raw's >0xB fallbacks
+    0xBEC54/0xBEC84) and the difficulty table entry 0xBEBD8[d]. Only the entries a case (or a
+    mutant) reads are poked: the harness caps a case at 16 pokes."""
+    s = 0x001077B0 + side * 0x94
+    o = 0x001077B0 + (1 - side) * 0x94
+    kk, k2c = min(k, 11), min(k2, 11)
+    p = {s + 0x8C: le16(self8c), o + 0x8C: le16(other8c), s + 0x63: bytes([s63]),
+         0x00107D2A + side * 2: le32(k << 16), 0x00107D1E + side * 2: le32(k2 << 16),
+         0x001082C8 + side * 4: le32(d), 0x000BEBD8: le32(0x5555),
+         0x000BEBD8 + d * 4: le32(diff)}
+    if self8c:
+        p[0x000BEC58 + kk * 4] = le32(0x2000 + kk)
+        if k2c != kk:
+            p[0x000BEC58 + k2c * 4] = le32(0x2000 + k2c)
+        p[0x000BEC58 + 44] = le32(0x200B)
+        p[0x000BEC58 + 48] = le32(0x200C)
+    else:
+        p[0x000BEC28 + kk * 4] = le32(0x1000 + kk)
+        if k2c != kk:
+            p[0x000BEC28 + k2c * 4] = le32(0x1000 + k2c)
+        p[0x000BEC28 + 44] = le32(0x100B)
+    return p
+
+
+def c3c_46_case(ch, id0, id1=0x0015, lo=0x0010, hi=0x0020):
+    """0x18460's state: slot0/slot1 + their records + the ch range pair + the actor words.
+    idx0 = 1 and idx1 = 2, so a0 = 1 and a1 = 2 on every call."""
+    s0, s1, r0, r1 = 0x10AF00, 0x10AF80, 0x10AF40, 0x10AFC0
+    return {0x001077A8: le32(s0), 0x001077AC: le32(s1), s0: le32(r0), s1: le32(r1),
+            s0 + 0x7A: bytes([ch & 0xFF]), s1 + 0x7A: b"\x00",
+            r0 + 0x56: le16(1), r1 + 0x56: le16(2),
+            0x001014EC: le32(0x10B000),
+            0x10B000 + 1 * 0x20: le16(id0), 0x10B000 + 2 * 0x20: le16(id1),
+            0x000A1774 + ch * 14: le16(lo), 0x000A1776 + ch * 14: le16(hi)}
+
+
+C3C_SPECS = [
+    # 0x1C390 render_pop_free: pop the free-list head [0x10275C]; EAX = the popped node. The raw
+    # dereferences the new head unconditionally (no empty-list case; the port's caller guards it).
+    Spec("render_pop_free", 0x1C390, [
+        Case("p0", {}, {C3C_R_FREE: le32(C3C_R_N1), C3C_R_N1: le32(C3C_R_N2), C3C_R_N2: le32(0)}),
+        Case("p1", {}, {C3C_R_FREE: le32(C3C_R_N3), C3C_R_N3: le32(0)}),
+        Case("p2", {}, {C3C_R_FREE: le32(C3C_R_N1), C3C_R_N1: le32(C3C_R_N2),
+                        C3C_R_N2: le32(C3C_R_N3), C3C_R_N3: le32(0),
+                        C3C_R_N1 + 4: le32(0x5A5A5A5A)}),
+    ], eax_mask=0xFFFFFFFF, mutants=("@head", "@ret", "@next")),
+    # 0x1C3A0 render_splice: EAX = headp, EDX = node; insert node before the first node whose
+    # pset+0x0E layer is greater (stable <=). Void.
+    Spec("render_splice", 0x1C3A0, [
+        Case("s0", {"eax": C3C_R_HEAD, "edx": C3C_R_N1},
+             {C3C_R_HEAD: le32(0), C3C_R_N1: le32(0xDEADBEEF), C3C_R_N1 + 4: le32(C3C_R_P1),
+              C3C_R_P1 + 0x0E: le16(0x10)}),
+        Case("s1", {"eax": C3C_R_HEAD, "edx": C3C_R_N1},
+             {**c3c_r_list(C3C_R_HEAD, [(C3C_R_N2, C3C_R_P2, 0x30)]),
+              C3C_R_N1: le32(0xDEADBEEF), C3C_R_N1 + 4: le32(C3C_R_P1), C3C_R_P1 + 0x0E: le16(0x10)}),
+        Case("s2", {"eax": C3C_R_HEAD, "edx": C3C_R_N1},
+             {**c3c_r_list(C3C_R_HEAD, [(C3C_R_N2, C3C_R_P2, 0x10), (C3C_R_N3, C3C_R_P3, 0x30)]),
+              C3C_R_N1: le32(0xDEADBEEF), C3C_R_N1 + 4: le32(C3C_R_P1), C3C_R_P1 + 0x0E: le16(0x20)}),
+        Case("s3", {"eax": C3C_R_HEAD, "edx": C3C_R_N1},
+             {**c3c_r_list(C3C_R_HEAD, [(C3C_R_N2, C3C_R_P2, 0x10), (C3C_R_N3, C3C_R_P3, 0x20)]),
+              C3C_R_N1: le32(0xDEADBEEF), C3C_R_N1 + 4: le32(C3C_R_P1), C3C_R_P1 + 0x0E: le16(0x30)}),
+        Case("s4", {"eax": C3C_R_HEAD, "edx": C3C_R_N1},
+             {**c3c_r_list(C3C_R_HEAD, [(C3C_R_N2, C3C_R_P2, 0x20)]),
+              C3C_R_N1: le32(0xDEADBEEF), C3C_R_N1 + 4: le32(C3C_R_P1), C3C_R_P1 + 0x0E: le16(0x20)}),
+        Case("s5", {"eax": C3C_R_HEAD, "edx": C3C_R_N1},
+             {**c3c_r_list(C3C_R_HEAD, [(C3C_R_N2, C3C_R_P2, 0x20), (C3C_R_N3, C3C_R_P3, 0x20)]),
+              C3C_R_N1: le32(0xDEADBEEF), C3C_R_N1 + 4: le32(C3C_R_P1), C3C_R_P1 + 0x0E: le16(0x20)}),
+    ], eax_mask=0, mutants=("@lt", "@next", "@head", "@prev", "@first")),
+    # 0x1C458 render_find: EAX = headp, EDX = pset_off; EAX = the node whose +4 matches, or 0.
+    Spec("render_find", 0x1C458, [
+        Case("f0", {"eax": C3C_R_HEAD, "edx": 0x10B040},
+             {C3C_R_HEAD: le32(C3C_R_N1), C3C_R_N1: le32(0), C3C_R_N1 + 4: le32(0x10B040)}),
+        Case("f1", {"eax": C3C_R_HEAD, "edx": 0x10B080},
+             {C3C_R_HEAD: le32(C3C_R_N1), C3C_R_N1: le32(C3C_R_N2), C3C_R_N1 + 4: le32(0x10B040),
+              C3C_R_N2: le32(0), C3C_R_N2 + 4: le32(0x10B080)}),
+        Case("f2", {"eax": C3C_R_HEAD, "edx": 0x10B0A0},
+             {C3C_R_HEAD: le32(C3C_R_N1), C3C_R_N1: le32(C3C_R_N2), C3C_R_N1 + 4: le32(0x10B040),
+              C3C_R_N2: le32(0), C3C_R_N2 + 4: le32(0x10B080)}),
+        Case("f3", {"eax": C3C_R_HEAD, "edx": 0x10B040}, {C3C_R_HEAD: le32(0)}),
+    ], eax_mask=0xFFFFFFFF, mutants=("@cmp", "@skip", "@last", "@null")),
+    # 0x1C3D0 render_unlink: EAX = headp, EDX = node; unlink node (no-op when absent) and push it
+    # on the free-list head. Void.
+    Spec("render_unlink", 0x1C3D0, [
+        Case("u0", {"eax": C3C_R_HEAD, "edx": C3C_R_N2},
+             {**c3c_r_list(C3C_R_HEAD, [(C3C_R_N1, C3C_R_P1, 0x10), (C3C_R_N2, C3C_R_P2, 0x20),
+                                        (C3C_R_N3, C3C_R_P3, 0x30)]),
+              C3C_R_N2: le32(C3C_R_N3), C3C_R_FREE: le32(C3C_R_N4), C3C_R_N4: le32(0xF0F0F0F0)}),
+        Case("u1", {"eax": C3C_R_HEAD, "edx": C3C_R_N1},
+             {**c3c_r_list(C3C_R_HEAD, [(C3C_R_N1, C3C_R_P1, 0x10), (C3C_R_N2, C3C_R_P2, 0x20)]),
+              C3C_R_FREE: le32(0), C3C_R_N1: le32(C3C_R_N2)}),
+        Case("u2", {"eax": C3C_R_HEAD, "edx": C3C_R_N3},
+             {**c3c_r_list(C3C_R_HEAD, [(C3C_R_N1, C3C_R_P1, 0x10), (C3C_R_N2, C3C_R_P2, 0x20),
+                                        (C3C_R_N3, C3C_R_P3, 0x30)]),
+              C3C_R_FREE: le32(C3C_R_N1), C3C_R_N3: le32(0x01020304)}),
+        Case("u3", {"eax": C3C_R_HEAD, "edx": C3C_R_N4},
+             {**c3c_r_list(C3C_R_HEAD, [(C3C_R_N1, C3C_R_P1, 0x10), (C3C_R_N2, C3C_R_P2, 0x20)]),
+              C3C_R_FREE: le32(C3C_R_N4)}),
+        Case("u4", {"eax": C3C_R_HEAD, "edx": C3C_R_N1},
+             {C3C_R_HEAD: le32(0), C3C_R_FREE: le32(C3C_R_N1)}),
+    ], eax_mask=0, mutants=("@head", "@free", "@chain", "@prev", "@noop")),
+    # 0x127C0 actor_type_127C0 (the type-0x01 cb1): EAX = rec, EDX = slot (unread). Pop the 0xF0A78
+    # head; empty (the sentinel points to itself) -> EAX 0xFF, else link the node at rec+0x14 and
+    # insert it after 0xF0AE0, return 0. The 0x249D0/0x249B0 calls run as real on both sides.
+    Spec("actor_type_127C0", 0x127C0, [
+        Case("t0", {"eax": C3C_MP_HUB, "edx": 0x10AF40},
+             {0x000F0A78: le32(0x000F0A78), 0x000F0A78 + 4: le32(0x000F0A78),
+              C3C_MP_HUB + 0x14: le32(0x11111111)}),
+        Case("t1", {"eax": C3C_MP_HUB, "edx": 0x10AF40},
+             {0x000F0A78: le32(C3C_R_N1), C3C_R_N1: le32(0x000F0A78),
+              C3C_R_N1 + 4: le32(0x000F0A78), C3C_R_N1 + 8: le32(0xF0F0F0F0),
+              0x000F0AE0: le32(0x000F0AE0), 0x000F0AE0 + 4: le32(0x000F0AE0),
+              C3C_MP_HUB + 0x14: le32(0x11111111)}),
+        Case("t2", {"eax": C3C_R_N4, "edx": C3C_R_P4},
+             {0x000F0A78: le32(C3C_R_N2), C3C_R_N2: le32(0x000F0A78),
+              C3C_R_N2 + 4: le32(0x000F0A78), C3C_R_N2 + 8: le32(0xF0F0F0F0),
+              0x000F0AE0: le32(C3C_R_N3), C3C_R_N3: le32(0x000F0AE0),
+              C3C_R_N3 + 4: le32(0x000F0AE0), 0x000F0AE0 + 4: le32(C3C_R_N3),
+              C3C_R_N4 + 0x14: le32(0x11111111)}),
+    ], calls=(E.Call(0x249D0, ("eax",), mode="real"),
+              E.Call(0x249B0, ("eax", "edx"), mode="real")),
+       eax_mask=0xFF, mutants=("@empty", "@ret", "@link", "@rec", "@at", "@unlink")),
+    # 0x12800 actor_type_12800 (the type-0x01 cb2): EAX = rec. Return the rec+0x14 node to 0xF0A78
+    # and clear rec+0x14; a null node returns with nothing done.
+    Spec("actor_type_12800", 0x12800, [
+        Case("t0", {"eax": C3C_MP_HUB}, {C3C_MP_HUB + 0x14: le32(0)}),
+        Case("t1", {"eax": C3C_MP_HUB},
+             {C3C_MP_HUB + 0x14: le32(C3C_R_N1), 0x10AF80: le32(C3C_R_N1),
+              C3C_R_N1: le32(0x10AF80), C3C_R_N1 + 4: le32(0x10AF80),
+              0x000F0A78: le32(0x000F0A78), 0x000F0A78 + 4: le32(0x000F0A78)}),
+        Case("t2", {"eax": C3C_MP_HUB},
+             {C3C_MP_HUB + 0x14: le32(C3C_R_N2), 0x10AF40: le32(C3C_R_N2),
+              C3C_R_N2: le32(0x10AF40), C3C_R_N2 + 4: le32(0x10AF40),
+              0x000F0A78: le32(C3C_R_N3), C3C_R_N3: le32(0x000F0A78),
+              C3C_R_N3 + 4: le32(0x000F0A78), 0x000F0A78 + 4: le32(C3C_R_N3)}),
+    ], calls=(E.Call(0x249D0, ("eax",), mode="real"),
+              E.Call(0x249B0, ("eax", "edx"), mode="real")),
+       eax_mask=0, mutants=("@zero", "@clear", "@at", "@unlink", "@node")),
+    # 0x18428 fighter_18428: EAX = side. Reads slot[side]+0x7A and (for a character 0..6) jumps
+    # through the all-RET table 0x1840C. Effect-free: reads only, so no case can catch a read-only
+    # mutation and the row carries no mutants (a named limit, record §C3c).
+    Spec("fighter_18428", 0x18428, [
+        Case("h0", {"eax": 0}, {0x1077A8: le32(C3C_R_N1), C3C_R_N1 + 0x7A: b"\x03"}),
+        Case("h1", {"eax": 1}, {0x1077A8: le32(C3C_R_N1), 0x1077AC: le32(C3C_R_N2),
+                                C3C_R_N1 + 0x7A: b"\x00", C3C_R_N2 + 0x7A: b"\x06"}),
+        Case("h2", {"eax": 0}, {0x1077A8: le32(C3C_R_N2), C3C_R_N2 + 0x7A: b"\x07"}),
+        Case("h3", {"eax": 1}, {0x1077A8: le32(0), 0x1077AC: le32(0)}),
+    ], eax_mask=0, mutants=()),
+    # 0x18460 fighter_18460: EAX = side. Both slot pointers must be live; the side's character
+    # 0..6 reads {lo,hi} = 0xA1774/76 + ch*14, the sprite id is the actor word & 0x7FFF, and
+    # 0x18428 runs when the id is outside [lo,hi) and not 0x1E1. Returns 1 when the call is made
+    # (the return is port-only; mask 0). 0x18428 runs real: the row compares its call and args.
+    Spec("fighter_18460", 0x18460, [
+        Case("h0", {"eax": 0}, {0x1077A8: le32(0), 0x1077AC: le32(0x10AF80)}),
+        Case("h1", {"eax": 0}, {0x1077A8: le32(0x10AF00), 0x1077AC: le32(0)}),
+        Case("h2", {"eax": 0}, c3c_46_case(0, 0x0015)),
+        Case("h3", {"eax": 0}, c3c_46_case(0, 0x0030)),
+        Case("h4", {"eax": 0}, c3c_46_case(7, 0x0100, lo=0x0001, hi=0x7FFF)),
+        Case("h5", {"eax": 0}, c3c_46_case(0, 0x01E1)),
+        Case("h6", {"eax": 1}, c3c_46_case(0, 0x0000, id1=0x0060)),
+        Case("h7", {"eax": 0}, c3c_46_case(0, 0x0010)),
+        Case("h8", {"eax": 0}, c3c_46_case(0, 0x0020)),
+        Case("h9", {"eax": 0}, c3c_46_case(0, 0x0005)),
+        Case("ha", {"eax": 0}, c3c_46_case(0, 0x8123, lo=0x2000, hi=0x3000)),
+    ], calls=(E.Call(0x18428, ("eax", "edx", "ebx", "ecx"), mode="real"),),
+       eax_mask=0, mutants=("@null", "@null2", "@lo", "@hi", "@range", "@e1", "@call", "@id")),
+    # 0x367DC fighter_state_367dc: EAX = slot, EDX = rec. Restart the record's animation at
+    # 0xC8950[slot+0x7A] (float 3.0), clear the slot/record fields, mask slot+0x40, then in modes
+    # other than 3/0x22/0x24 a second animation (0x102900[rec+0x51], 0xE906A, 1.0). Void.
+    Spec("fighter_state_367dc", 0x367DC, [
+        Case("d0", {"eax": C3C_R_N1, "edx": C3C_R_N2},
+             {C3C_R_N1 + 0x7A: b"\x02", C3C_R_N2 + 0x4C: b"\xAA", C3C_R_N2 + 0x4D: b"\xBB",
+              C3C_R_N1 + 0x52: b"\x01", C3C_R_N1 + 0x53: b"\x02", C3C_R_N1 + 0x5F: b"\x01",
+              C3C_R_N1 + 0x55: b"\x02", C3C_R_N1 + 0x54: b"\x03",
+              C3C_R_N1 + 0x40: le32(0xFFFFFFFF), 0x104B00: le16(3)}),
+        Case("d1", {"eax": C3C_R_N1, "edx": C3C_R_N2},
+             {C3C_R_N1 + 0x7A: b"\x02", C3C_R_N2 + 0x4C: b"\xAA", C3C_R_N2 + 0x4D: b"\xBB",
+              C3C_R_N1 + 0x52: b"\x01", C3C_R_N1 + 0x53: b"\x02", C3C_R_N1 + 0x5F: b"\x01",
+              C3C_R_N1 + 0x55: b"\x02", C3C_R_N1 + 0x54: b"\x03",
+              C3C_R_N1 + 0x40: le32(0xFFFFFFFF), 0x104B00: le16(0x22)}),
+        Case("d2", {"eax": C3C_R_N1, "edx": C3C_R_N2},
+             {C3C_R_N1 + 0x7A: b"\x02", C3C_R_N2 + 0x4C: b"\xAA", C3C_R_N2 + 0x4D: b"\xBB",
+              C3C_R_N1 + 0x52: b"\x01", C3C_R_N1 + 0x53: b"\x02", C3C_R_N1 + 0x5F: b"\x01",
+              C3C_R_N1 + 0x55: b"\x02", C3C_R_N1 + 0x54: b"\x03",
+              C3C_R_N1 + 0x40: le32(0xFFFFFFFF), 0x104B00: le16(0x24)}),
+        Case("d3", {"eax": C3C_R_N1, "edx": C3C_R_N2},
+             {C3C_R_N1 + 0x7A: b"\x05", C3C_R_N2 + 0x4C: b"\xAA", C3C_R_N2 + 0x4D: b"\xBB",
+              C3C_R_N1 + 0x52: b"\x01", C3C_R_N1 + 0x53: b"\x02", C3C_R_N1 + 0x5F: b"\x01",
+              C3C_R_N1 + 0x55: b"\x02", C3C_R_N1 + 0x54: b"\x03",
+              C3C_R_N1 + 0x40: le32(0xFFFFFFFF), 0x104B00: le16(0),
+              C3C_R_N2 + 0x51: b"\x01", 0x102900: le32(0x10B0A0), 0x102904: le32(0x10B0B0)}),
+        Case("d4", {"eax": C3C_R_N1, "edx": C3C_R_N2},
+             {C3C_R_N1 + 0x7A: b"\x05", C3C_R_N2 + 0x4C: b"\xAA", C3C_R_N2 + 0x4D: b"\xBB",
+              C3C_R_N1 + 0x52: b"\x01", C3C_R_N1 + 0x53: b"\x02", C3C_R_N1 + 0x5F: b"\x01",
+              C3C_R_N1 + 0x55: b"\x02", C3C_R_N1 + 0x54: b"\x03",
+              C3C_R_N1 + 0x40: le32(0xFFFFFFFF), 0x104B00: le16(0x23),
+              C3C_R_N2 + 0x51: b"\x00", 0x102900: le32(0x10B0A0), 0x102904: le32(0x10B0B0)}),
+    ], calls=(E.Call(0x2BC30, ("eax", "edx", "s0"), pop=4, clobbers=("edx",)),),
+       eax_mask=0, mutants=("@mode", "@stream", "@ch", "@clr4c", "@clr53", "@ff", "@mask", "@call")),
+    # 0x1CA40 snd_music_playing: no args; AL = 1 when [0x1028C0] (the sequence handle) is non-zero
+    # and 0x5DEED (the AIL status, stubbed through the port's seam) returns 4; [0x1028C0] == 0
+    # returns 0 without the call. The raw masks the result to AL (`and eax,0xff`).
+    Spec("snd_music_playing", 0x1CA40, [
+        Case("m0", {}, {0x001028C0: le32(0)}, {0x5DEED: 4}),
+        Case("m1", {}, {0x001028C0: le32(0x10B0C0)}, {0x5DEED: 4}),
+        Case("m2", {}, {0x001028C0: le32(0x10B0C0)}, {0x5DEED: 3}),
+        Case("m3", {}, {0x001028C0: le32(0x10B0C0)}, {0x5DEED: 0x104}),
+    ], calls=(E.Call(0x5DEED, (), mode="stub"),),
+       eax_mask=0xFF, mutants=("@zero", "@one", "@gate", "@eq3", "@call", "@eax")),
+    # 0x33714 palette_record_flagged (EBX = ptr, EAX = first, EDX = count): append the record
+    # { ptr; first; count; flag = 1 } at the head [0x107798] and advance it by 0x10.
+    Spec("palette_record_flagged", 0x33714, [
+        Case("v0", {"eax": 0x00001234, "edx": 0x00005678, "ebx": 0x00009ABC},
+             {0x00107798: le32(0x00107498), 0x001074A5: b"\xA5", 0x001074A6: b"\xA6",
+              0x001074A7: b"\xA7", 0x001074A8: le32(0x11111111)}),
+        Case("v1", {"eax": 0xDEADBEEF, "edx": 0x00000002, "ebx": 0x00107798},
+             {0x00107798: le32(0x00107538), 0x00107545: b"\xB5", 0x00107546: b"\xB6",
+              0x00107547: b"\xB7", 0x00107548: le32(0x44444444)}),
+    ], eax_mask=0, mutants=("@flag", "@order", "@adv", "@wide", "@swap")),
+    # 0x33734 palette_record (EBX = ptr, EAX = first, EDX = count): the same append with flag 0.
+    Spec("palette_record", 0x33734, [
+        Case("w0", {"eax": 0x00001234, "edx": 0x00005678, "ebx": 0x00009ABC},
+             {0x00107798: le32(0x00107498), 0x001074A5: b"\xA5", 0x001074A6: b"\xA6",
+              0x001074A7: b"\xA7", 0x001074A8: le32(0x11111111)}),
+        Case("w1", {"eax": 0xDEADBEEF, "edx": 0x00000002, "ebx": 0x00107798},
+             {0x00107798: le32(0x00107538), 0x00107545: b"\xB5", 0x00107546: b"\xB6",
+              0x00107547: b"\xB7", 0x00107548: le32(0x44444444)}),
+    ], eax_mask=0, mutants=("@flag", "@order", "@adv", "@wide", "@swap")),
+    # 0x2BD44 fighter_2bd44: EAX = param_1 (the fighter record), EDX = param_2 (the 0x1014F4 row).
+    # Copy param_2's +0x4B into param_1, re-arm param_2 (clear +0x24, +0x2A bit 3, +0x28 bits 2/4;
+    # set +0x29 bit 3; +8 = 0x1E1), load its sprite id through 0x2A408 (stub, per-case EAX) into its
+    # actor word, then 0x2B150 (stub). Void.
+    Spec("fighter_2bd44", 0x2BD44, [
+        Case("m0", {"eax": 0x10AF00, "edx": 0x10B000},
+             {0x10AF00 + 0x4B: b"\x11", 0x10B000 + 0x4B: b"\xA7", 0x10B000 + 0x24: le32(0xDEADBEEF),
+              0x10B000 + 0x2A: b"\xFF", 0x10B000 + 0x29: b"\x00", 0x10B000 + 0x28: b"\xFF",
+              0x10B000 + 8: le32(0x99999999), 0x10B000 + 0x56: le16(3),
+              0x001014EC: le32(0x10AF80), 0x10AFF0: le16(0x5A5A)}, {0x2A408: 0xBEEF}),
+        Case("m1", {"eax": 0x10AF00, "edx": 0x10B000},
+             {0x10AF00 + 0x4B: b"\x00", 0x10B000 + 0x4B: b"\x3C", 0x10B000 + 0x24: le32(0x00000000),
+              0x10B000 + 0x2A: b"\x08", 0x10B000 + 0x29: b"\xFF", 0x10B000 + 0x28: b"\x00",
+              0x10B000 + 8: le32(0x11111111), 0x10B000 + 0x56: le16(0),
+              0x001014EC: le32(0x10AF80), 0x10AF80: le16(0x7FFF)}, {0x2A408: 0x1234}),
+    ], calls=(E.Call(0x2A408, ("eax", "edx"), mode="stub", clobbers=("edx",)),
+              E.Call(0x2B150, ("eax",), mode="stub")),
+       eax_mask=0, mutants=("@src", "@clr24", "@a2a", "@o29", "@a28", "@id", "@idx")),
+    # 0x3B6C4 fighter_3b6c4: EAX = side. 1 when the side's slot +0x53 == 8 and +0x54 == 2, the
+    # other slot's +0x54 != 2, and both sides' 0x1A570 actor-bit-15 predicates agree. The 0x33950
+    # ctx builder and the 0x1A570 calls are the row's; AL only (`mov al,1`/`xor al,al`).
+    Spec("fighter_3b6c4", 0x3B6C4, [
+        Case("b0", {"eax": 0}, c3c_b6_case(0, 8, 2, 0, 0, 0)),
+        Case("b1", {"eax": 0}, c3c_b6_case(0, 7, 2, 0, 0, 0)),
+        Case("b2", {"eax": 0}, c3c_b6_case(0, 8, 3, 0, 0, 0)),
+        Case("b3", {"eax": 0}, c3c_b6_case(0, 8, 2, 2, 0, 0)),
+        Case("b4", {"eax": 0}, c3c_b6_case(0, 8, 2, 0, 1, 0)),
+        Case("b5", {"eax": 1}, c3c_b6_case(1, 8, 2, 0, 1, 1)),
+    ], allow_calls=(0x33950,),
+       calls=(E.Call(0x1A570, ("eax",), mode="real"),),
+       eax_mask=0xFF, mutants=("@s53", "@s54", "@o54", "@cmp", "@ret", "@side")),
+    # 0x3C520 hit_anim_start_c: EAX = rec, EDX = stream, s0 = frame bits. The ctx (0x339AC) gives
+    # the side's slot; 0x2BC30 (stub) starts the animation; 0x188DC/0x1890C (stubs) take the slot's
+    # +0x2C/+0x30 as x/y. Void.
+    Spec("hit_anim_start_c", 0x3C520, [
+        Case("c0", {"eax": 0x10AF00, "edx": 0x00E12345, "s0": 0x3F800000},
+             {0x10AF00 + 0x51: b"\x00", 0x001077B0 + 0x2C: le32(0x11111111),
+              0x001077B0 + 0x30: le32(0x22222222)}),
+        Case("c1", {"eax": 0x10AF40, "edx": 0x00E56789, "s0": 0x40400000},
+             {0x10AF40 + 0x51: b"\x01", 0x001077B0 + 0x94 + 0x2C: le32(0x33333333),
+              0x001077B0 + 0x94 + 0x30: le32(0x44444444)}),
+        Case("c2", {"eax": 0x10AF80, "edx": 0x80000000, "s0": 0x00000000},
+             {0x10AF80 + 0x51: b"\x00", 0x001077B0 + 0x2C: le32(0xFFFF8000),
+              0x001077B0 + 0x30: le32(0x00008000)}),
+    ], allow_calls=(0x339AC,),
+       calls=(E.Call(0x2BC30, ("eax", "edx", "s0"), pop=4, clobbers=("edx",)),
+              E.Call(0x188DC, ("eax", "edx"), mode="stub", clobbers=("edx",)),
+              E.Call(0x1890C, ("eax", "edx"), mode="stub", clobbers=("edx",))),
+       eax_mask=0, mutants=("@side", "@xy", "@begin", "@bits", "@anchor", "@stream")),
+    # 0x1A734 fighter_block_hit: EAX = side. The ctx (0x33A10) gives the side's slot; +0x61 = 0x0C,
+    # lowered to +0x60 when greater and +0x62 set; +0x43 bit 0x20 picks the +0x54 = 0 arm with the
+    # 0xC8F40[ch] stream, bit 0x10 the +0x54 = 1 arm with 0xC8F90[ch]; both end in 0x3C480(rec,
+    # stream, 3.0). 0x18B04 and 0x3C480 are stubs (the latter pops its stack arg). Void.
+    Spec("fighter_block_hit", 0x1A734, [
+        Case("k0", {"eax": 0}, c3c_k_case(0, 0x05, 0x01, 0x20, 0x02)),
+        Case("k1", {"eax": 0}, c3c_k_case(0, 0x20, 0x01, 0x10, 0x02)),
+        Case("k2", {"eax": 0}, c3c_k_case(0, 0x20, 0x01, 0x00, 0x02)),
+        Case("k3", {"eax": 1}, c3c_k_case(1, 0x05, 0x01, 0x20, 0x05)),
+        Case("k4", {"eax": 0}, c3c_k_case(0, 0x00, 0x00, 0x20, 0x02)),
+        Case("k5", {"eax": 0}, c3c_k_case(0, 0x05, 0x01, 0x30, 0x02)),
+    ], allow_calls=(0x33A10,),
+       calls=(E.Call(0x18B04, ("eax",), mode="stub"),
+              E.Call(0x3C480, ("eax", "edx", "s0"), pop=4, clobbers=("edx",))),
+       eax_mask=0, mutants=("@c61", "@clamp", "@b62", "@u", "@arm", "@s54", "@call", "@tab")),
+    # 0x39738 fighter_39738: EAX = side, EDX = b. The 0x33950 ctx gives the side's and the other's
+    # slots. When the side's slot+0x8C is non-zero the per-character base is 0xBEC58[0x107D2A[side]]
+    # (0xBEC84 above 0xB) * b / 100, else 0xBEC28[k] * b / 100, or with k > 0xB: b/4 when
+    # 0x107D1E[side] > 0x46, else 0xBEC54 * b / 100. Then the other slot's +0x8C cuts 15% and a
+    # non-zero side slot+0x63 adds 0xBEBD8[0x1082C8[side]] * v / 100. Full EAX (the dword result).
+    Spec("fighter_39738", 0x39738, [
+        Case("r0", {"eax": 0, "edx": 100}, c3c_97_case(0, 1, 0, 5, 100)),
+        Case("r1", {"eax": 0, "edx": 100}, c3c_97_case(0, 1, 0, 11, 100)),
+        Case("r2", {"eax": 0, "edx": 100}, c3c_97_case(0, 1, 0, 12, 100)),
+        Case("r3", {"eax": 0, "edx": 200}, c3c_97_case(0, 0, 0, 5, 200)),
+        Case("r4", {"eax": 0, "edx": 200}, c3c_97_case(0, 0, 0, 12, 200, k2=0x46)),
+        Case("r5", {"eax": 0, "edx": 0xFFFFFF9B}, c3c_97_case(0, 0, 0, 12, 0xFFFFFF9B, k2=0x47)),
+        Case("r6", {"eax": 1, "edx": 0xFFFFFF9C}, c3c_97_case(1, 1, 1, 12, 0xFFFFFF9C, k2=0x47, s63=1, d=2)),
+        Case("r7", {"eax": 1, "edx": 101}, c3c_97_case(1, 0, 1, 3, 101, s63=1, d=2)),
+    ], allow_calls=(0x33950,), eax_mask=0xFFFFFFFF,
+       mutants=("@chan", "@k", "@tab", "@k2", "@div", "@cut", "@diff", "@idx")),
+]
+
 SPECS = [
     Spec("rng_next", 0x5D7DC, [
         Case("r1", {"eax": 0x1234}, {DS_RNG: le32(0x12345678)}),
@@ -5266,7 +5625,7 @@ SPECS = [
         Case("d0", {}, {DS_1078FC: b"\x00"}),
         Case("d1", {"eax": U6_REC}, {DS_1078FC: b"\x5a"}),
     ], eax_mask=0),
-] + E3_SPECS + P1_SPECS + P1_ANIM_SPECS + P2_SPECS + P3_SPECS + P6_SPECS + P45_SPECS + C1_SPECS + C2_SPECS + C2B_SPECS + C3_SPECS + C3B_SPECS + P7_SPECS + P8_SPECS
+] + E3_SPECS + P1_SPECS + P1_ANIM_SPECS + P2_SPECS + P3_SPECS + P6_SPECS + P45_SPECS + C1_SPECS + C2_SPECS + C2B_SPECS + C3_SPECS + C3B_SPECS + P7_SPECS + P8_SPECS + C3C_SPECS
 
 
 # ---- driver -------------------------------------------------------------------------------------
diff --git a/tools/tests/test_diff_verify.py b/tools/tests/test_diff_verify.py
index ed6a2f7..09b428c 100644
--- a/tools/tests/test_diff_verify.py
+++ b/tools/tests/test_diff_verify.py
@@ -516,6 +516,9 @@ P45_KINDS = {"fighter_18bc8@mutant": {"byte"},
 P45_OUTSIDE = {name: [(0x10C1B0, 4)] for name in (
     "fighter_3427c", "fighter_34308", "fighter_3438c", "fighter_34418",
     "fighter_344a4", "fighter_34530", "fighter_345bc")}
+# C3c: fighter_18428 h3's null slot makes the raw read 0x007A (outside the image: the emulator's
+# zero page); the port reads mem[0x7A] the same way.
+P45_OUTSIDE["fighter_18428"] = [(122, 1)]
 # Track P batch C1 (record 2026-10-03-reverse-c1): the callee rows with their EAX masks, and what
 # alone catches each of their mutants.
 C1_MASKS = {"fighter_3c148": 0, "fighter_3c16c": 0, "fighter_39a10": 0, "fighter_36d98": 0,
@@ -1171,6 +1174,125 @@ C3B_KINDS = {
     "snd_samples_stop_all@skip2": {"call #1", "call #1 memory", "call #10", "call #11", "call #2", "call #2 memory", "call #3 memory", "call #4", "call #5", "call #6", "call #7", "call #8", "call #9"},
 }
 
+# Track P batch C3c (record 2026-10-05-reverse-c3c): the frontier rows, part 3, with their
+# EAX masks and what alone catches each of their mutants.
+C3C_MASKS = {
+            "render_pop_free": 0xffffffff,
+            "render_splice": 0x0,
+            "render_find": 0xffffffff,
+            "render_unlink": 0x0,
+            "actor_type_127C0": 0xff,
+            "actor_type_12800": 0x0,
+            "fighter_18428": 0x0,
+            "fighter_18460": 0x0,
+            "fighter_state_367dc": 0x0,
+            "snd_music_playing": 0xff,
+            "palette_record_flagged": 0x0,
+            "palette_record": 0x0,
+            "fighter_2bd44": 0x0,
+            "fighter_3b6c4": 0xff,
+            "hit_anim_start_c": 0x0,
+            "fighter_block_hit": 0x0,
+            "fighter_39738": 0xffffffff,
+}
+C3C_KINDS = {
+    "actor_type_127C0@at": {'call #1', 'byte'},
+    "actor_type_127C0@empty": {'call #1', 'byte', 'call #0', 'eax'},
+    "actor_type_127C0@link": {'byte', 'call #1 memory'},
+    "actor_type_127C0@rec": {'byte', 'call #1 memory'},
+    "actor_type_127C0@ret": {'eax'},
+    "actor_type_127C0@unlink": {'call #1', 'call #0 memory', 'byte', 'call #0'},
+    "actor_type_12800@at": {'call #1', 'byte'},
+    "actor_type_12800@clear": {'byte'},
+    "actor_type_12800@node": {'call #1', 'byte', 'call #0'},
+    "actor_type_12800@unlink": {'call #1', 'byte', 'call #0'},
+    "actor_type_12800@zero": {'call #1', 'call #0'},
+    "fighter_18460@call": {'call #0'},
+    "fighter_18460@e1": {'call #0'},
+    "fighter_18460@hi": {'call #0'},
+    "fighter_18460@id": {'call #0'},
+    "fighter_18460@lo": {'call #0'},
+    "fighter_18460@null": {'call #0'},
+    "fighter_18460@null2": {'call #0'},
+    "fighter_18460@range": {'call #0'},
+    "fighter_2bd44@a28": {'call #0 memory', 'byte', 'call #1 memory'},
+    "fighter_2bd44@a2a": {'call #0 memory', 'byte', 'call #1 memory'},
+    "fighter_2bd44@clr24": {'call #0 memory', 'byte', 'call #1 memory'},
+    "fighter_2bd44@id": {'call #0 memory', 'byte', 'call #1 memory'},
+    "fighter_2bd44@idx": {'byte', 'call #1 memory', 'call #0'},
+    "fighter_2bd44@o29": {'call #0 memory', 'byte', 'call #1 memory'},
+    "fighter_2bd44@src": {'call #0 memory', 'byte', 'call #1 memory'},
+    "fighter_39738@chan": {'eax'},
+    "fighter_39738@cut": {'eax'},
+    "fighter_39738@diff": {'eax'},
+    "fighter_39738@div": {'eax'},
+    "fighter_39738@idx": {'eax'},
+    "fighter_39738@k": {'eax'},
+    "fighter_39738@k2": {'eax'},
+    "fighter_39738@tab": {'eax'},
+    "fighter_3b6c4@cmp": {'eax'},
+    "fighter_3b6c4@o54": {'call #1', 'call #0', 'eax'},
+    "fighter_3b6c4@ret": {'eax'},
+    "fighter_3b6c4@s53": {'call #1', 'call #0', 'eax'},
+    "fighter_3b6c4@s54": {'call #1', 'call #0', 'eax'},
+    "fighter_3b6c4@side": {'call #1', 'call #0', 'eax'},
+    "fighter_block_hit@arm": {'call #1', 'call #0 memory', 'byte', 'call #0'},
+    "fighter_block_hit@b62": {'call #1', 'call #0 memory', 'byte', 'call #0'},
+    "fighter_block_hit@c61": {'call #1', 'call #0 memory', 'byte', 'call #0'},
+    "fighter_block_hit@call": {'call #1', 'call #0'},
+    "fighter_block_hit@clamp": {'call #1', 'call #0 memory', 'byte', 'call #0'},
+    "fighter_block_hit@s54": {'call #1', 'call #0 memory', 'byte', 'call #0'},
+    "fighter_block_hit@tab": {'call #1', 'call #0 memory', 'call #0'},
+    "fighter_block_hit@u": {'call #1', 'call #0 memory', 'call #0'},
+    "fighter_state_367dc@call": {'call #0'},
+    "fighter_state_367dc@ch": {'call #0'},
+    "fighter_state_367dc@clr4c": {'byte', 'call #1 memory'},
+    "fighter_state_367dc@clr53": {'byte', 'call #1 memory'},
+    "fighter_state_367dc@ff": {'byte', 'call #1 memory'},
+    "fighter_state_367dc@mask": {'byte', 'call #1 memory'},
+    "fighter_state_367dc@mode": {'call #1'},
+    "fighter_state_367dc@stream": {'call #0'},
+    "hit_anim_start_c@anchor": {'call #1', 'call #2'},
+    "hit_anim_start_c@begin": {'call #1', 'call #2', 'call #0'},
+    "hit_anim_start_c@bits": {'call #0'},
+    "hit_anim_start_c@side": {'call #1', 'call #2'},
+    "hit_anim_start_c@stream": {'call #0'},
+    "hit_anim_start_c@xy": {'call #1', 'call #2'},
+    "palette_record@adv": {'byte'},
+    "palette_record@flag": {'byte'},
+    "palette_record@order": {'byte'},
+    "palette_record@swap": {'byte'},
+    "palette_record@wide": {'byte'},
+    "palette_record_flagged@adv": {'byte'},
+    "palette_record_flagged@flag": {'byte'},
+    "palette_record_flagged@order": {'byte'},
+    "palette_record_flagged@swap": {'byte'},
+    "palette_record_flagged@wide": {'byte'},
+    "render_find@cmp": {'eax'},
+    "render_find@last": {'eax'},
+    "render_find@null": {'eax'},
+    "render_find@skip": {'eax'},
+    "render_pop_free@head": {'byte'},
+    "render_pop_free@next": {'byte'},
+    "render_pop_free@ret": {'eax'},
+    "render_splice@first": {'byte'},
+    "render_splice@head": {'byte'},
+    "render_splice@lt": {'byte'},
+    "render_splice@next": {'byte'},
+    "render_splice@prev": {'byte'},
+    "render_unlink@chain": {'byte'},
+    "render_unlink@free": {'byte'},
+    "render_unlink@head": {'byte'},
+    "render_unlink@noop": {'byte'},
+    "render_unlink@prev": {'byte'},
+    "snd_music_playing@call": {'eax', 'call #0'},
+    "snd_music_playing@eax": {'eax'},
+    "snd_music_playing@eq3": {'eax'},
+    "snd_music_playing@gate": {'eax', 'call #0'},
+    "snd_music_playing@one": {'eax', 'call #0'},
+    "snd_music_playing@zero": {'eax', 'call #0'},
+}
+
 @needs_unicorn
 @unittest.skipUnless((os.path.exists(DIFFRUN) and os.path.exists(EXE)) or REQUIRED,
                      "build/diffrun or PRAGE.EXE absent")
@@ -1195,7 +1317,7 @@ class RealFunctionTests(unittest.TestCase):
                                              "fighter_slot_flag", "hit_anim_ctx", "hit_anim_start_b",
                                              "host_1b890", "rng_next"] + list(P1_MASKS) + list(P2_MASKS)
                                             + list(P3_MASKS) + list(P45_MASKS) + list(C1_MASKS)
-                                            + list(P6_MASKS) + list(C2_MASKS) + list(C2B_MASKS) + list(C3_MASKS) + list(C3B_MASKS) + list(P7_MASKS) + list(P8_MASKS)))
+                                            + list(P6_MASKS) + list(C2_MASKS) + list(C2B_MASKS) + list(C3_MASKS) + list(C3B_MASKS) + list(C3C_MASKS) + list(P7_MASKS) + list(P8_MASKS)))
         for name, r in self.real.items():
             if name == "host_1b890":       # the named gap (record E3 §E3.8), tested on its own below
                 continue
@@ -1211,7 +1333,7 @@ class RealFunctionTests(unittest.TestCase):
             "fighter_45878@mutant", "fighter_ctx_same@mutant", "fighter_slot_flag@mutant",
             "hit_anim_ctx@mutant", "hit_anim_start_b@mutant", "hit_anim_start_b@set", "rng_next@mutant"]
             + list(P1_KINDS) + list(P2_KINDS) + list(P3_KINDS) + list(P45_KINDS) + list(C1_KINDS)
-            + list(P6_KINDS) + list(C2_KINDS) + list(C2B_KINDS) + list(C3_KINDS) + list(C3B_KINDS) + list(P7_KINDS) + list(P8_KINDS)))
+            + list(P6_KINDS) + list(C2_KINDS) + list(C2B_KINDS) + list(C3_KINDS) + list(C3B_KINDS) + list(C3C_KINDS) + list(P7_KINDS) + list(P8_KINDS)))
         for name, r in self.mut.items():
             self.assertEqual(r.verdict, "MISMATCH", name)
 
@@ -1276,7 +1398,7 @@ class RealFunctionTests(unittest.TestCase):
             "fighter_3640c": 0, "fighter_37dcc": 0,
             "fighter_23130": 0xFF, "fighter_45878": 0, "anim_10fa8": 0, "anim_3e4e4": 0,
             "fighter_ctx_same": 0, "hit_anim_ctx": 0, "hit_anim_start_b": 0, "host_1b890": 0xFFFFFFFF,
-            **P1_MASKS, **P2_MASKS, **P3_MASKS, **P45_MASKS, **C1_MASKS, **P6_MASKS, **C2_MASKS, **C2B_MASKS, **C3_MASKS, **C3B_MASKS,
+            **P1_MASKS, **P2_MASKS, **P3_MASKS, **P45_MASKS, **C1_MASKS, **P6_MASKS, **C2_MASKS, **C2B_MASKS, **C3_MASKS, **C3B_MASKS, **C3C_MASKS,
             **P7_MASKS, **P8_MASKS})
         # with the full mask the slot-flag original's scratch bits (case f9: EAX = 0x201) differ
         spec = dataclasses.replace([s for s in V.SPECS if s.name == "fighter_slot_flag"][0],
@@ -1979,7 +2101,7 @@ class RealFunctionTests(unittest.TestCase):
                                  0x3C520: ("edx",), 0x3C59C: ("edx",), 0x41310: (), 0x46460: ("edx",),
                                  0x468D8: (), 0x48170: (), 0x49444: ("edi", "ebp"), 0x4F434: (), 0x5D7DC: (), 0x5DC0F: ("ebx", "ecx", "edx"),
                                  0x5DC8B: ("ebx", "ecx", "edx"), 0x5DD03: (),
-                                 0x5DEAF: ("ebx", "ecx", "edx")})
+                                 0x5DEAF: ("ebx", "ecx", "edx"), 0x5DEED: ()})
         for addr, declared in stubs.items():
             self.assertEqual(E.callee_clobbers(img, addr), declared, hex(addr))
 
@@ -2074,9 +2196,116 @@ class RealFunctionTests(unittest.TestCase):
                          "--self-check"])
         self.assertEqual(rc, 0)
         # the closed-row count is over the rows that have callees (185), the 49 without are counted apart
-        self.assertIn("diff-verify: 234/234 functions VERIFIED; 779/779 mutants detected; 1 named gaps; "
-                      "165/185 rows with callees closed (49 have none).", out.getvalue())
+        self.assertIn("diff-verify: 251/251 functions VERIFIED; 874/874 mutants detected; 1 named gaps; "
+                      "178/195 rows with callees closed (56 have none).", out.getvalue())
+
+
+    def test_each_c3c_mutant_is_caught_by_what_it_breaks(self):
+        # track P batch C3c (record 2026-10-05-reverse-c3c): what alone catches each mutant
+        for name, want in C3C_KINDS.items():
+            got = {p.split(": ", 1)[1].split(":")[0] if p.split(": ", 1)[1].startswith("call #")
+                   else p.split(": ", 1)[1].split(" ")[0] for p in self.mut[name].problems}
+            self.assertEqual(got, want, name)
+        # the exact case set that alone catches each mutant (measured on the prototype)
+        for name, ids in (
 
+        ("actor_type_127C0@at", ['t1', 't2']),
+        ("actor_type_127C0@empty", ['t0']),
+        ("actor_type_127C0@link", ['t1', 't2']),
+        ("actor_type_127C0@rec", ['t1', 't2']),
+        ("actor_type_127C0@ret", ['t0']),
+        ("actor_type_127C0@unlink", ['t1', 't2']),
+        ("actor_type_12800@at", ['t1', 't2']),
+        ("actor_type_12800@clear", ['t1', 't2']),
+        ("actor_type_12800@node", ['t1', 't2']),
+        ("actor_type_12800@unlink", ['t1', 't2']),
+        ("actor_type_12800@zero", ['t0']),
+        ("fighter_18460@call", ['h3', 'h4', 'h6', 'h8', 'h9', 'ha']),
+        ("fighter_18460@e1", ['h5']),
+        ("fighter_18460@hi", ['h2', 'h7']),
+        ("fighter_18460@id", ['ha']),
+        ("fighter_18460@lo", ['h2', 'h7']),
+        ("fighter_18460@null", ['h0']),
+        ("fighter_18460@null2", ['h1']),
+        ("fighter_18460@range", ['h4']),
+        ("fighter_2bd44@a28", ['m0']),
+        ("fighter_2bd44@a2a", ['m0', 'm1']),
+        ("fighter_2bd44@clr24", ['m0']),
+        ("fighter_2bd44@id", ['m0', 'm1']),
+        ("fighter_2bd44@idx", ['m0']),
+        ("fighter_2bd44@o29", ['m0']),
+        ("fighter_2bd44@src", ['m0', 'm1']),
+        ("fighter_39738@chan", ['r3', 'r4', 'r5', 'r7']),
+        ("fighter_39738@cut", ['r7']),
+        ("fighter_39738@diff", ['r7']),
+        ("fighter_39738@div", ['r5']),
+        ("fighter_39738@idx", ['r7']),
+        ("fighter_39738@k", ['r0', 'r1', 'r2', 'r3', 'r7']),
+        ("fighter_39738@k2", ['r4']),
+        ("fighter_39738@tab", ['r2', 'r4', 'r6']),
+        ("fighter_3b6c4@cmp", ['b0', 'b4', 'b5']),
+        ("fighter_3b6c4@o54", ['b3']),
+        ("fighter_3b6c4@ret", ['b0', 'b5']),
+        ("fighter_3b6c4@s53", ['b0', 'b4', 'b5']),
+        ("fighter_3b6c4@s54", ['b0', 'b2', 'b4', 'b5']),
+        ("fighter_3b6c4@side", ['b0', 'b4', 'b5']),
+        ("fighter_block_hit@arm", ['k0', 'k1', 'k2', 'k3', 'k4', 'k5']),
+        ("fighter_block_hit@b62", ['k0', 'k1', 'k2', 'k3', 'k4', 'k5']),
+        ("fighter_block_hit@c61", ['k0', 'k1', 'k2', 'k3', 'k4', 'k5']),
+        ("fighter_block_hit@call", ['k0', 'k1', 'k2', 'k3', 'k4', 'k5']),
+        ("fighter_block_hit@clamp", ['k0', 'k1', 'k2', 'k3', 'k4', 'k5']),
+        ("fighter_block_hit@s54", ['k0', 'k1', 'k2', 'k3', 'k4', 'k5']),
+        ("fighter_block_hit@tab", ['k0', 'k1', 'k2', 'k3', 'k4', 'k5']),
+        ("fighter_block_hit@u", ['k0', 'k1', 'k2', 'k3', 'k4', 'k5']),
+        ("fighter_state_367dc@call", ['d0', 'd1', 'd2', 'd3', 'd4']),
+        ("fighter_state_367dc@ch", ['d0', 'd1', 'd2', 'd3', 'd4']),
+        ("fighter_state_367dc@clr4c", ['d0', 'd1', 'd2', 'd3', 'd4']),
+        ("fighter_state_367dc@clr53", ['d0', 'd1', 'd2', 'd3', 'd4']),
+        ("fighter_state_367dc@ff", ['d0', 'd1', 'd2', 'd3', 'd4']),
+        ("fighter_state_367dc@mask", ['d0', 'd1', 'd2', 'd3', 'd4']),
+        ("fighter_state_367dc@mode", ['d0', 'd1', 'd2']),
+        ("fighter_state_367dc@stream", ['d0', 'd1', 'd2', 'd3', 'd4']),
+        ("hit_anim_start_c@anchor", ['c0', 'c1', 'c2']),
+        ("hit_anim_start_c@begin", ['c0', 'c1', 'c2']),
+        ("hit_anim_start_c@bits", ['c0', 'c2']),
+        ("hit_anim_start_c@side", ['c1']),
+        ("hit_anim_start_c@stream", ['c0', 'c1', 'c2']),
+        ("hit_anim_start_c@xy", ['c0', 'c1', 'c2']),
+        ("palette_record@adv", ['w0', 'w1']),
+        ("palette_record@flag", ['w0', 'w1']),
+        ("palette_record@order", ['w0', 'w1']),
+        ("palette_record@swap", ['w0', 'w1']),
+        ("palette_record@wide", ['w0', 'w1']),
+        ("palette_record_flagged@adv", ['v0', 'v1']),
+        ("palette_record_flagged@flag", ['v0', 'v1']),
+        ("palette_record_flagged@order", ['v0', 'v1']),
+        ("palette_record_flagged@swap", ['v0', 'v1']),
+        ("palette_record_flagged@wide", ['v0', 'v1']),
+        ("render_find@cmp", ['f0', 'f1']),
+        ("render_find@last", ['f0', 'f1']),
+        ("render_find@null", ['f0', 'f1']),
+        ("render_find@skip", ['f1', 'f2']),
+        ("render_pop_free@head", ['p0', 'p1', 'p2']),
+        ("render_pop_free@next", ['p0', 'p2']),
+        ("render_pop_free@ret", ['p0', 'p1', 'p2']),
+        ("render_splice@first", ['s2', 's3', 's4', 's5']),
+        ("render_splice@head", ['s0', 's1']),
+        ("render_splice@lt", ['s4', 's5']),
+        ("render_splice@next", ['s0', 's1', 's2', 's3', 's4', 's5']),
+        ("render_splice@prev", ['s2', 's3', 's4', 's5']),
+        ("render_unlink@chain", ['u0', 'u1', 'u2']),
+        ("render_unlink@free", ['u0', 'u1', 'u2']),
+        ("render_unlink@head", ['u0', 'u2']),
+        ("render_unlink@noop", ['u0', 'u1', 'u2']),
+        ("render_unlink@prev", ['u0', 'u2']),
+        ("snd_music_playing@call", ['m1', 'm2', 'm3']),
+        ("snd_music_playing@eax", ['m3']),
+        ("snd_music_playing@eq3", ['m1', 'm2']),
+        ("snd_music_playing@gate", ['m0']),
+        ("snd_music_playing@one", ['m0', 'm1', 'm2', 'm3']),
+        ("snd_music_playing@zero", ['m1', 'm2', 'm3']),
+        ):
+            self.assertEqual(sorted({p.split(":")[0] for p in self.mut[name].problems}), ids, name)
 
 # ---- E3: the call list, named gaps, the callee column (record 2026-10-01-reverse-e3 §E3.4, §E3.8) --
 
```
