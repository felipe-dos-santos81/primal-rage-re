# C3b: the frontier rows, part 2 (track P, batch C3b) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add the differential-verification row for each of the seventeen frontier addresses this batch
measures — the C3 record §C3.7 remainder's first three families (`0x18350`, `0x18540`, `0x18788`,
`0x29F34`, `0x38154`, `0x2B150`, `0x3AFC4`), the five voice rows (`0x1CA6C`, `0x1CE70`, `0x1CD9C`,
`0x1CE04`, `0x1CC28`), and this batch's items (`0x13420`, `0x249C0`, `0x29DB8`, `0x2B8F8`) — with
their seams, bindings, mutants and exact-set pins; verification only, no ported function, no
`fn_register`, no E2 move. The remaining 21 candidate addresses and the 9 new frontier items these
rows create are C3c (record §C3b.7).

**Architecture:** Each row is a `Spec` in `tools/diff_verify.py` (`C3B_SPECS`), a `b_*` binding and
`m_*` mutants in `port/tests/diff_runner.c` (`k_bindings`), and exact-set assertions in
`tools/tests/test_diff_verify.py` (`C3B_MASKS`, `C3B_KINDS`, the case-set table, the clobber table,
the counter line). `port/src` changes: the AIL host wrappers gain seams (`AIL_init_sample`
`0x5DC0F`, `AIL_stop_sample` `0x5DC8B`, `AIL_sample_status` `0x5DD03`, `AIL_stop_sequence`
`0x5DEAF`), `snd_music_playing` (`0x1CA40`) gains a seam and loses `static`, `fighter_state_367dc`
(`0x367DC`) and `set_dead` (`0x2B150`) lose `static` for their rows' bindings and mutants. No new
seam on the effects.c list copies: C3's `actor_alloc` mutant route depends on them staying
seam-less (record §C3b.2, found during prototyping).

**Tech Stack:** C11 (`port/src`, `port/tests`), Python 3.12 (`unittest`), `unicorn` 2.1.4 and
`capstone` 5.0.7 (`tools/requirements-diff.txt`), CMake, `make`.

**Spec:** `docs/superpowers/specs/2026-09-30-reverse-completion-design.md` §4 track P, §5.1-§5.3,
§6 ("each ported function has a verification row; unhit blocks and non-emulable instructions named;
counters and README percentage updated"), §7.

**Derivation record:** `docs/superpowers/plans/2026-10-05-reverse-c3b-derivations.md` (§C3b.1 the
membership re-derived, §C3b.2 the rows, seams and fixtures, §C3b.3 the mutants and their measured
catching sets and case sets, §C3b.4 the counters, §C3b.5 the named gaps, limits and the
`0x1C390`/`0x1C3A0` row-shape decision, §C3b.6 what the planner ran, §C3b.7 the C3c deferral with
its evidence). Recipe: `2026-10-01-reverse-e3-derivations.md` §E3.10; checklist:
`.superpowers/sdd/2026-10-01-plans/p-track-review-checklist.md`.

**What the planner ran (scratch, 2026-10-06, on `reverse-c3b` at `main` `2cf874b` = C3a merged;
image sha1 `ff3b8cb14e00f1c282de7b7e15dcd7c230766947`, the E2/E3/P1-P8/C1-C3 image):** the baseline
(Task 1) and a full prototype of the seventeen rows: the previous session's uncommitted prototype
(11 rows, saved at `/tmp/c3b-partial-prototype.patch`) was re-measured, its AIL stub clobber
declarations were corrected against `E.callee_clobbers` (record §C3b.2), the effects.c seam
attempt was reverted (it broke C3's `actor_alloc` mutant route), and six further rows
(`0x13420`, `0x49444`, `0x2B150`, `0x29DB8`, `0x2B8F8`, `0x3AFC4`) were added. Every row was
measured with `--function NAME --self-check` until VERIFIED with every mutant detected, then the
full `python3 tools/diff_verify.py --self-check` (`234/234 functions VERIFIED; 779/779 mutants
detected; 1 named gaps; 165/185 rows with callees closed (49 have none)`),
`make entry-triage` (byte-identical: `targets 233 unported, 262 ported; supplement 131 (3 unported,
0 stale); untrusted 30`; voice `0 / 115 / 19`), `python3 -m unittest tools.tests.test_diff_verify`
(106 tests OK), `PR_ORACLE_REQUIRED=1 ./build/run_tests` (all checks passed),
`python3 tools/port_progress.py` (`771 1203 64` / `731 731 100`) and `symbols.h` regeneration
(byte-identical). Every expected output below is the prototype's measured output. The prototype was
then reverted (`git checkout -- port tools`); the patch below is the exact diff it applied.

**Re-baseline note.** The counters below are `2cf874b`'s. If `main` moves before this plan executes,
Task 1 records the measured base and every later expected counter adds this plan's increments:
functions +17, mutants +115, rows with callees +13, closed rows +11, no-callee rows +4. The E2 table
must not move (no ported function, no `fn_register`): if a task regenerates it, the line must be
byte-identical.

## Decisions needed from the user

**None.** No raw-over-port correction was needed: every row matched on first measurement of the
tree. The `0x1C390`/`0x1C3A0` row-shape decision (a C3c item) is recorded with its disassembly
evidence in the record §C3b.5: two rows (the raw's pop at `0x1C390` and splice at `0x1C3A0` are
separate entries; the port's combined `render_list_insert` cannot bind either), reached by splitting
the port function along the raw's seam in C3c. The effects.c seam attempt and its revert are
recorded as a harness constraint, not a port deviation.

## The C3b roadmap

| task | what | commit |
|---|---|---|
| 1 | baseline on `2cf874b` (no commit) | - |
| 2 | the seventeen rows, the seams and exports, bindings, mutants and the test exact-set updates | `tools:` |
| 3 | the review sweep (stores, clobbers, mutant case sets) | `tests:` (only if a fix) |
| 4 | closure: PROGRESS, the record's §C3b.8, the final gates | `docs:` |

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
  adds the C3b case-set test; no other assertion changes.
- AGENTS.md: "`data/` is git-ignored and read-only — never write to it." / "Commit style:
  `<area>: <what changed>`. **Never `git add -A`** — stage named files." No `Co-Authored-By`
  trailer. Never run `make gp-capture`.
- The C3b brief: "the gp miss sets must stay exactly as Task 1 measures them (C3b touches no
  gameplay path)"; the only `port/src` changes are seams and exports, inert outside `build/diffrun`.
- Spec §5.3 / E3 §E3.4: "VERIFIED" is equivalence on the exercised blocks and inputs only, each
  function with its callees stubbed or run as stated.

## Where to run

The worktree `.worktrees/reverse-c3b` (branch `reverse-c3b`). Every command runs from the worktree
root; `data/` and `.superpowers/` are symlinks; `build/` exists. Before Task 1:

```bash
git rev-parse HEAD   # 2cf874b
python3 -c "import unicorn, capstone; print(unicorn.__version__, capstone.__version__)"  # 2.1.4 5.0.7
cmake -S port -B build && cmake --build build 2>&1 | tail -1
./build/diffrun --exe data/game/C/PRAGE.EXE --image-out /tmp/pr_c3b_img.bin && shasum /tmp/pr_c3b_img.bin
ls port/tests/ghidra_data.bin   # the PR_ORACLE_REQUIRED fixture (git-ignored); copy it from the main repo if absent
```

The last-but-one line must print `ff3b8cb14e00f1c282de7b7e15dcd7c230766947`; another hash means the
image differs from the one the record measured: stop. Ledger:
`.superpowers/sdd/2026-10-05-reverse-c3b-frontier-rows-2/progress.md`.

### The seventeen rows (the brief's grouping, executed as families)

The patch is one `git diff` (Task 2 applies it at once); the families below are the gate order:

| family | rows | shared seams/fixtures |
|---|---|---|
| the small leaves | `0x29F34` `0x18350` `0x18540` `0x18788` `0x38154` | no new seams; the `1A570`/`18350`/`18540`/`36638`/`35838` real calls and the `18428`/`18460`/`367DC` allows; the exported `fighter_state_367dc` |
| the hub | `0x2B150` | the exported `set_dead`; the `33864` stub call and the `249B0`/`249D0` real calls; the `1C458`/`1C3D0`/`5D812`/`12800` allows |
| the anim triple | `0x3AFC4` | none (the `0x62003` fatal named unhit) |
| the voice remainder | `0x1CA6C` `0x1CE70` `0x1CD9C` `0x1CE04` `0x1CC28` | the AIL wrapper seams (`5DC0F`/`5DC8B`/`5DD03`/`5DEAF`) and the `1CA40` seam; the `1B544`/`500BB` allows |
| the anim variables | `0x29DB8` `0x2B8F8` | the `29F34` stub call |
| the new items | `0x13420` `0x249C0` | the `249B0`/`249D0`/`33714`/`33734` allows for the teardown |

## How the code steps are written

Task 2's patch is the prototype's exact `git diff` (the planner applied it, ran every gate, and
reverted). Apply it with `git apply`; it touches `port/src/game/actors.c`, `port/src/game/actors.h`,
`port/src/game/fighter.c`, `port/src/game/fighter.h`, `port/src/game/flow.c`, `port/src/game/flow.h`,
`port/src/platform/audio/ail.c`, `port/tests/diff_runner.c`, `tools/diff_verify.py` and
`tools/tests/test_diff_verify.py`. If `main` moved, `git apply --3way` and resolve by keeping the
patch's additions at the same anchors; never merge the E2 table by hand.

## File Structure

| File | Responsibility |
|---|---|
| `port/src/game/actors.c`/`.h` | `set_dead` (`0x2B150`) loses `static` for its row's binding and mutants |
| `port/src/game/fighter.c`/`.h` | `fighter_state_367dc` (`0x367DC`) loses `static` for `0x38154`'s mutant cores |
| `port/src/game/flow.c`/`.h` | `snd_music_playing` (`0x1CA40`) loses `static`, gains `PR_SEAM_RET0` |
| `port/src/platform/audio/ail.c` | the four AIL wrapper seams (`0x5DC0F`, `0x5DC8B`, `0x5DD03`, `0x5DEAF`) |
| `port/tests/diff_runner.c` | the seventeen bindings and 115 mutants, the `C3B` cores |
| `tools/diff_verify.py` | `C3B_SPECS` (the seventeen rows and their fixtures) |
| `tools/tests/test_diff_verify.py` | `C3B_MASKS`, `C3B_KINDS`, the case-set test, the clobber and counter lines |
| `docs/superpowers/plans/2026-10-05-reverse-c3b-derivations.md` | the record (this plan's source of values) |
| `docs/PROGRESS.md` | Task 4's paragraph |

---

### Task 1: Baseline (no commit)

**Files:** none. **Interfaces:** consumes the worktree at `2cf874b`; produces the baseline counters
and table.

- [ ] **Step 1: the task gates on the untouched tree.**

```bash
make diff-verify DIFF_IMAGE=/tmp/pr_c3b_base.bin DIFF_TABLE=/tmp/pr_c3b_base_table.md 2>&1 | tail -1
make entry-triage E2_IMAGE=/tmp/pr_c3b_base_e2.bin 2>&1 | tail -2
python3 tools/port_progress.py
```

Expected (measured at `2cf874b`):

```
diff-verify: 217/217 functions VERIFIED; 664/664 mutants detected; 1 named gaps; 154/172 rows with callees closed (45 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
entry-triage: targets 233 unported, 262 ported; supplement 131 (3 unported, 0 stale); untrusted entries 30
entry-triage: voice sites outside Ghidra 134: 0 in unported code, 115 in ported code, 19 nowhere
771 1203 64
731 731 100 (portable: excludes 81 host-owned/deferred and runtime >= 5D000)
```

- [ ] **Step 2: enumerate the frontier from the table.** `python3` over `/tmp/pr_c3b_base_table.md`:
  the callees marked `unverified` (record §C3b.1's 43 distinct: 36 candidates + the named non-row
  `0x2EA30` + the two new items `0x13420`/`0x249C0` + the four non-rows `0x1B544`, `0x5D812`,
  `0x29D60`, `0x2EA64`). Expected: this plan's seventeen rows are in the set; the other 21
  candidates and the 4 non-rows are C3c (record §C3b.7).

- [ ] **Step 3: pin the gp miss sets.** `PR_GP_DUMP` scenarios' pinned sets in `test_platform.c`
  must be the ones Task 2 leaves untouched; no C3b change reaches a gameplay path.

### Task 2: the seventeen rows (one commit)

**Files:** the patch below. **Interfaces:** produces `C3B_SPECS`, the seventeen bindings and 115
mutants, the `set_dead`/`snd_music_playing`/`fighter_state_367dc` exports, the four AIL seams, and
the test exact-set updates; consumes the C1/C2/C2b/C3 fixtures (`E3_SLOT`, `E3_REC`, `ANIM_BEGIN`,
`le32`, the C3B scratch records).

- [ ] **Step 1: apply the patch.** `git apply - <<'PATCH' ... PATCH` with the diff embedded at the
  end of this task (the exact prototype diff; 2089 insertions over 10 files).

- [ ] **Step 2: build and run each row's self-check.**

```bash
cmake --build build 2>&1 | tail -1
for f in anim_read_var fighter_18350 fighter_18540 hit_record_y fighter_38154 set_dead \
         fighter_anim_triple snd_music_stop snd_sample_playing snd_samples_stop_all \
         snd_sample_stop snd_sample_queue anim_write_var anim_operand effect_teardown \
         actor_type_49444 list_insert_before; do
  python3 tools/diff_verify.py --function "$f" --self-check
done
```

Expected rows (measured; the format is row, entry, cases, blocks, mutants, named unhit):

| row | entry | cases | blocks | mutants | named unhit |
|---|---|---|---|---|---|
| `anim_read_var` | 0x29F34 | 20 | 24/24 | 8/8 | — |
| `fighter_18350` | 0x18350 | 10 | 11/11 | 5/5 | — |
| `fighter_18540` | 0x18540 | 13 | 14/14 | 6/6 | — |
| `hit_record_y` | 0x18788 | 4 | 6/6 | 5/5 | — |
| `fighter_38154` | 0x38154 | 17 | 36/36 | 8/8 | — |
| `set_dead` | 0x2B150 | 8 | 7/7 | 7/7 | — |
| `fighter_anim_triple` | 0x3AFC4 | 5 | 3/4 | 6/6 | 0x3AFCE (the `0x62003` fatal) |
| `snd_music_stop` | 0x1CA6C | 3 | 4/4 | 6/6 | — |
| `snd_sample_playing` | 0x1CE70 | 7 | 9/9 | 6/6 | — |
| `snd_samples_stop_all` | 0x1CD9C | 3 | 8/8 | 6/6 | — |
| `snd_sample_stop` | 0x1CE04 | 7 | 8/8 | 6/6 | — |
| `snd_sample_queue` | 0x1CC28 | 10 | 19/19 | 13/13 | — |
| `anim_write_var` | 0x29DB8 | 22 | 24/24 | 8/8 | — |
| `anim_operand` | 0x2B8F8 | 16 | 28/28 | 8/8 | — |
| `effect_teardown` | 0x13420 | 8 | 6/6 | 6/6 | — |
| `actor_type_49444` | 0x49444 | 8 | 7/7 | 6/6 | — |
| `list_insert_before` | 0x249C0 | 3 | 1/1 | 5/5 | — |

(The per-row run reports the callee column as `unverified` for every callee whose own row is not
run; only the full run's table reads `VERIFIED`. Record E3 §E3.8.)

- [ ] **Step 3: the task gates.**

```bash
make diff-verify DIFF_IMAGE=/tmp/pr_c3b_after.bin DIFF_TABLE=/tmp/pr_c3b_after_table.md 2>&1 | tail -1
python3 -m unittest tools.tests.test_diff_verify 2>&1 | tail -3
make entry-triage E2_IMAGE=/tmp/pr_c3b_after_e2.bin 2>&1 | tail -2
PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -1
python3 tools/gen_symbols.py port/decomp /tmp/pr_c3b_sym.h && diff /tmp/pr_c3b_sym.h port/src/symbols.h
```

Expected: `234/234 functions VERIFIED; 779/779 mutants detected; 1 named gaps; 165/185 rows with
callees closed (49 have none)`; `OK` (106 tests); E2 byte-identical; `all checks passed`; `symbols.h`
regeneration byte-identical.

- [ ] **Step 4: commit.**

```bash
git add port/src/game/actors.c port/src/game/actors.h port/src/game/fighter.c \
        port/src/game/fighter.h port/src/game/flow.c port/src/game/flow.h \
        port/src/platform/audio/ail.c port/tests/diff_runner.c tools/diff_verify.py \
        tools/tests/test_diff_verify.py
git commit -m "tools: C3b: the seventeen frontier rows, their seams and mutants"
```

### Task 3: the review sweep (one commit, only if a fix)

**Files:** the patch's rows. **Interfaces:** consumes Task 2's state; produces the store sweep's
findings.

- [ ] **Step 1: the store sweep.** For every C3b row, poke each field the row writes to a value that
  differs from what it writes and re-run `--function NAME --self-check`; a store with no sentinel
  fails some mutant. The prototype's sweep was done per row during measurement; re-check the fields
  the record lists as the rows' only writes (the `0x105B4C` ring words, the `+0x52..+0x58` record
  bytes, the pool records' fields, the `0x10839C` entry, `rec+0x14`, `rec+0x28/+0x2B`, the palette
  dirty-list head and record, the three `fighter_anim_triple` output dwords, the `0x100AB0/AB4`
  latches, the `0x1028CC` pending song and the pause bytes, the DIG slot fields, `0x105B44/0x10275C`
  render links, `0x9AF3C/0x9AF3D`, `0xFCCE8` links).
- [ ] **Step 2: the clobber and case-set re-check.** `python3 -m unittest
  tools.tests.test_diff_verify.RealFunctionTests` (the clobber re-derivation and the C3b case-set
  test). Expected: OK.
- [ ] **Step 3: commit any fix.**

```bash
git add <the fixed files>
git commit -m "tests: C3b review sweep: the store sentinels and the case-set pins"
```

### Task 4: Closure (one commit)

**Files:** `docs/PROGRESS.md`, the record. **Interfaces:** consumes Task 2's measured state.

- [ ] **Step 1: the final gates.**

```bash
make diff-verify DIFF_IMAGE=/tmp/pr_c3b_close.bin 2>&1 | tail -1
make entry-triage E2_IMAGE=/tmp/pr_c3b_close_e2.bin 2>&1 | tail -2
PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -1
python3 tools/port_progress.py
make verify
```

Expected: the Task 2 counter; E2 byte-identical; `all checks passed`; `771 1203 64` / `731 731
100`; and the full `make verify` exit 0 with the 45 oracle lines equal to
`.superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt`, the `make audio-render` WAV
equal to the pre-change WAV, every gp ratchet line `ok` with measured == pin (Task 1's list
verbatim), and `symbols.h` byte-identical. The only `port/src` changes are seams and exports (inert
outside `build/diffrun`); the full ladder is what proves no oracle-visible path moved.

- [ ] **Step 2: append the PROGRESS paragraph** (the seventeen rows, the counter, the new frontier
  items this batch creates, the C3c deferral of record §C3b.7).

- [ ] **Step 3: append §C3b.8 Results to the record** (the executed tree's counters, the commit
  shas, the gate log lines), mirroring C3's §C3.8.

- [ ] **Step 4: commit.**

```bash
git add docs/PROGRESS.md docs/superpowers/plans/2026-10-05-reverse-c3b-derivations.md
git commit -m "docs: C3b closure: the seventeen frontier rows measured and the C3c deferral"
```

### The patch

```diff
diff --git a/port/src/game/actors.c b/port/src/game/actors.c
index 9158cb7..7c715e2 100644
--- a/port/src/game/actors.c
+++ b/port/src/game/actors.c
@@ -1316,7 +1316,7 @@ void palette_reflow(u32 descriptor, u32 handle)
 
 /* ---- animation-stream interpreter (0x29F34/0x29DB8/0x2A408/0x2B8F8/0x2B2A0) */
 
-static void set_dead(u32 rec);
+void set_dead(u32 rec);
 
 /* 0x29F34. Read one animation variable. `op & 0x7F` selects: < 0x40 the
  * 0x40-word ring at DS_00105B4C indexed by rec+0x51; 0x40..0x45 the record's
@@ -2666,7 +2666,7 @@ u32 actors_link_held(u32 rec, u32 v)
 
 /* ---- pset sync (0x2A31C -> 0x2A1FC -> 0x2A820) -------------------------- */
 
-static void set_dead(u32 rec);
+void set_dead(u32 rec);
 
 /* 0x2A620. The mode-1 shear cursor: derive rec+0x64 from the current y, or
  * from pset+0x14 (the previous frame's x) when rec+0x1c is zero. */
@@ -3033,8 +3033,8 @@ void actor_pset_palette(u32 rec, u32 word, u32 handle)
 /* 0x2B150. Set the dead bit (0x28 0x08), release the pset palette and unlink
  * the pset from the render list. 63 callers in the original; the port reaches
  * it from the sync path (0x2A1FC's release_record) and from the type-0x20..0x25
- * teardown (0x49444). */
-static void set_dead(u32 rec)
+ * teardown (0x49444). Exported for its own C3b row's binding. */
+void set_dead(u32 rec)
 {
     PR_SEAM(0x2B150u, rec);
     DSB(rec + 0x28) |= 0x08;
diff --git a/port/src/game/actors.h b/port/src/game/actors.h
index 24f8e7e..16ad577 100644
--- a/port/src/game/actors.h
+++ b/port/src/game/actors.h
@@ -65,8 +65,11 @@ void actor_pset_palette(u32 rec, u32 word, u32 handle);
  * with the 0x800 sprite bit while that byte is non-zero. */
 void actor_pset_flag_5f(u32 rec, u8 flag);
 /* 0x2B150. Mark `rec` dead (rec+0x28 |= 8), release its pset palette and unlink
- * the pset from the render list. 0x121A0's phase 1 calls it on the logo and the
- * second object when DS_000F0A66 <= 0x10. */
+ * the pset from the render list. Exported for its own C3b row; the port's other
+ * callers go through actor_set_dead below. */
+void set_dead(u32 rec);
+/* 0x2B150. The wrapper the port's non-actor modules call: 0x121A0's phase 1
+ * uses it on the logo and the second object when DS_000F0A66 <= 0x10. */
 void actor_set_dead(u32 rec);
 /* 0x49444 (record §49-P). Types 0x20..0x25's teardown: clear the 0x10839C
  * entry named by the node's 16.16 +0x18 when its +0x1C bit 1 is set, retire
diff --git a/port/src/game/fighter.c b/port/src/game/fighter.c
index c294ebf..9da06c0 100644
--- a/port/src/game/fighter.c
+++ b/port/src/game/fighter.c
@@ -1724,7 +1724,7 @@ void fighter_state_35d7c(u32 side)
  * Ghidra decompilation + disassembly (register args resolved from each
  * prologue). The dispatch table is 0x34B14; the entries are in fight.c. */
 
-static void fighter_state_367dc(u32 slot, u32 rec);         /* 0x367DC */
+void fighter_state_367dc(u32 slot, u32 rec);                /* 0x367DC */
 static void hit_stance_timer(u32 side);                     /* 0x1922C */
 void hit_anim_start_c(u32 rec, u32 stream, u32 frame_bits); /* 0x3C520 */
 
@@ -2581,7 +2581,7 @@ void fighter_4f944(u32 v)
  * 3/0x22/0x24 a second animation on the side's 0x102900 record with the fixed
  * 0xE906A stream (raw 0x36843 EDX = 0xE906A, 0x36848 EAX = 0x102900[side];
  * 0x2BC30 stores EDX to rec+8 at 0x2BC52, so EDX is the stream). */
-static void fighter_state_367dc(u32 slot, u32 rec)
+void fighter_state_367dc(u32 slot, u32 rec)
 {
     actors_anim_begin(rec, DSD(FIGHT_ANIM_367DC
                                + (u32)DSB(slot + 0x7Au) * 4u),
diff --git a/port/src/game/fighter.h b/port/src/game/fighter.h
index d4d6da2..8517e8e 100644
--- a/port/src/game/fighter.h
+++ b/port/src/game/fighter.h
@@ -182,6 +182,8 @@ void fighter_385b0(u32 rec);                             /* 0x385B0 */
 /* 0x36638. Reset the slot's +0x43 bit 0x40 and restart the fighter's animation
  * per slot+0x54. Called by 0x349C8, 0x35838 and the 0x34B6C position branch. */
 int fighter_state_36638(u32 slot, u32 rec);
+/* 0x367DC (C3b): the +0x53 reset, exported for 0x38154's row mutants. */
+void fighter_state_367dc(u32 slot, u32 rec);
 
 /* 0x35D7C. The +0x52 == 3 handler: clear slot+0x53/+0x54, then, when the
  * 0x3CF38 hit chain reports no hit, arm slot+0x54 = 2, slot+0x53 = 4. */
diff --git a/port/src/game/flow.c b/port/src/game/flow.c
index 64718dd..c3f7f41 100644
--- a/port/src/game/flow.c
+++ b/port/src/game/flow.c
@@ -6170,8 +6170,9 @@ u32 snd_music_request(u32 song, u32 b)
 /* 0x1CA40. AL = 1 when the sequence DS_001028C0 plays (0x5DEED status 4).
  * PORT: the sequence handle is s_sequence; DS_001028C0 is 0 in the port, so
  * the status arm runs only when a caller has stored one. */
-static u32 snd_music_playing(void)
+u32 snd_music_playing(void)
 {
+    PR_SEAM_RET0(0x1CA40u);
     if (DSD(DS_001028C0) == 0u) return 0;                  /* 0x1CA40 */
     return AIL_sequence_status(s_sequence) == 4 ? 1u : 0u; /* 0x5DEED */
 }
diff --git a/port/src/game/flow.h b/port/src/game/flow.h
index 5cab89c..04aebcc 100644
--- a/port/src/game/flow.h
+++ b/port/src/game/flow.h
@@ -200,6 +200,8 @@ u32 sound_voice(u32 id);
 /* 0x1CA14/0x1CA6C/0x1CC28/0x1CD9C/0x1CE04/0x1CE70/0x1D238/0x1D244 (C2b):
  * the voice dispatcher's audio callees, exported for its differential row's mutants. */
 u32 snd_music_request(u32 song, u32 b);
+/* 0x1CA40 (C3b): the sequence-playing predicate, exported for the voice rows. */
+u32 snd_music_playing(void);
 u32 snd_music_stop(void);
 u32 snd_sample_queue(u32 h, u32 loop);
 u32 snd_samples_stop_all(void);
diff --git a/port/src/platform/audio/ail.c b/port/src/platform/audio/ail.c
index d1a0252..b0181ef 100644
--- a/port/src/platform/audio/ail.c
+++ b/port/src/platform/audio/ail.c
@@ -15,6 +15,8 @@
 #include <stddef.h>
 #include <stdlib.h>
 
+#include "mem.h"
+
 #include "ail.h"
 #include "mixer.h"
 #include "samples.h"
@@ -248,6 +250,7 @@ void AIL_release_sample_handle(HSAMPLE sample)
 /* 0x5dc0f — spec audio.md "AIL surface" (row 12). */
 void AIL_init_sample(HSAMPLE sample)
 {
+    PR_SEAM0(0x5DC0Fu);
     if (sample == NULL || !sample->used)
         return;
     sample->state = 2;
@@ -313,6 +316,7 @@ void AIL_start_sample(HSAMPLE sample)
 /* 0x5dc8b — spec audio.md "AIL surface" (row 16). */
 void AIL_stop_sample(HSAMPLE sample)
 {
+    PR_SEAM0(0x5DC8Bu);
     if (sample == NULL || !sample->used)
         return;
     sample->state = 2;
@@ -361,6 +365,7 @@ s32 AIL_sample_volume(HSAMPLE sample)
 /* 0x5dd03 — spec audio.md "AIL surface" (row 20). */
 s32 AIL_sample_status(HSAMPLE sample)
 {
+    PR_SEAM_RET0(0x5DD03u);
     if (sample == NULL || !sample->used)
         return 0;
     /* Record k7-k12 §0.7.6: the DIG service 0x6F120 marks a sample done at
@@ -471,6 +476,7 @@ void AIL_start_sequence(HSEQUENCE sequence)
 /* 0x5deaf — spec audio.md "AIL surface" (row 29). */
 void AIL_stop_sequence(HSEQUENCE sequence)
 {
+    PR_SEAM0(0x5DEAFu);
     if (sequence == NULL || !sequence->used)
         return;
     seq_stop();
diff --git a/port/tests/diff_runner.c b/port/tests/diff_runner.c
index 71cfb21..4cbc329 100644
--- a/port/tests/diff_runner.c
+++ b/port/tests/diff_runner.c
@@ -15,6 +15,7 @@
 #include "game/flow.h"
 #include "game/rng.h"
 #include "game/svcmenu.h"
+#include "platform/audio/ail.h"
 #include "platform/res.h"
 #include "platform/render.h"
 #include "platform/gfx.h"
@@ -9305,6 +9306,998 @@ static void m_2ac80_ret(const u32 *r, u32 *eax)
     *eax = 0u;
 }
 
+/* Track P batch C3b (record 2026-10-05-reverse-c3b): the frontier rows, part 2. */
+
+/* 0x29F34's mutants: one re-implementation with a mutation selector. */
+#define C3B_A_RING  0x01u   /* the ring index is not masked to 0x3F */
+#define C3B_A_SX    0x02u   /* case 0x40 returns the byte zero-extended */
+#define C3B_A_PSWAP 0x04u   /* 0x46..0x4B reads the child index */
+#define C3B_A_CSWAP 0x08u   /* 0x4C..0x51 reads the parent index */
+#define C3B_A_W58   0x10u   /* 0x44 reads the +0x58 word */
+#define C3B_A_A45   0x20u   /* 0x45 reads rec+0x57 */
+#define C3B_A_RET   0x40u   /* above 0x51 returns the ring word */
+#define C3B_A_OPFF  0x80u   /* the op mask is 0xFF, not 0x7F */
+static u32 c3b_a_core(const u32 *r, u32 mut)
+{
+    u32 rec = r[R_EAX];
+    u32 o = r[R_EDX] & ((mut & C3B_A_OPFF) ? 0xFFu : 0x7Fu);
+    if (o < 0x40u) {
+        u32 ix = o + (u32)DSB(rec + 0x51u);
+        if (!(mut & C3B_A_RING)) ix &= 0x3Fu;
+        return (u32)DSW(DS_00105B4C + ix * 2u);
+    }
+    switch (o) {
+    case 0x40: return (mut & C3B_A_SX) ? (u32)DSB(rec + 0x52u)
+                                       : (u32)(u16)(s8)DSB(rec + 0x52u);
+    case 0x41: return (u32)(u16)(s8)DSB(rec + 0x53u);
+    case 0x42: return (u32)(u16)(s8)DSB(rec + 0x54u);
+    case 0x43: return (u32)(u16)(s8)DSB(rec + 0x55u);
+    case 0x44: return (mut & C3B_A_W58) ? (u32)DSW(rec + 0x58u) : (u32)DSW(rec + 0x56u);
+    case 0x45: return (u32)(u16)(s8)DSB((mut & C3B_A_A45) ? rec + 0x57u : rec + 0x58u);
+    default: break;
+    }
+    u32 base;
+    if (o <= 0x4bu)
+        base = DSD(DS_001014F4)
+             + (u32)DSB(rec + ((mut & C3B_A_PSWAP) ? 0x4bu : 0x4au)) * ACTOR_REC_SIZE;
+    else if (o <= 0x51u)
+        base = DSD(DS_001014F4)
+             + (u32)DSB(rec + ((mut & C3B_A_CSWAP) ? 0x4au : 0x4bu)) * ACTOR_REC_SIZE;
+    else
+        return (mut & C3B_A_RET) ? (u32)DSW(DS_00105B4C) : 0u;
+    switch (o) {
+    case 0x46: case 0x4c: return (u32)(u16)(s8)DSB(base + 0x52u);
+    case 0x47: case 0x4d: return (u32)(u16)(s8)DSB(base + 0x53u);
+    case 0x48: case 0x4e: return (u32)(u16)(s8)DSB(base + 0x54u);
+    case 0x49: case 0x4f: return (u32)(u16)(s8)DSB(base + 0x55u);
+    case 0x4a: case 0x50: return (u32)DSW(base + 0x56u);
+    default:              return (u32)(u16)(s8)DSB(base + 0x58u);
+    }
+}
+static void b_c3b_anim_read_var(const u32 *r, u32 *eax)
+{ *eax = anim_read_var(r[R_EAX], r[R_EDX]); }
+static void m_c3b_a_ring(const u32 *r, u32 *eax)  { *eax = c3b_a_core(r, C3B_A_RING); }
+static void m_c3b_a_sx(const u32 *r, u32 *eax)    { *eax = c3b_a_core(r, C3B_A_SX); }
+static void m_c3b_a_pswap(const u32 *r, u32 *eax) { *eax = c3b_a_core(r, C3B_A_PSWAP); }
+static void m_c3b_a_cswap(const u32 *r, u32 *eax) { *eax = c3b_a_core(r, C3B_A_CSWAP); }
+static void m_c3b_a_w58(const u32 *r, u32 *eax)   { *eax = c3b_a_core(r, C3B_A_W58); }
+static void m_c3b_a_a45(const u32 *r, u32 *eax)   { *eax = c3b_a_core(r, C3B_A_A45); }
+static void m_c3b_a_ret(const u32 *r, u32 *eax)   { *eax = c3b_a_core(r, C3B_A_RET); }
+static void m_c3b_a_opff(const u32 *r, u32 *eax)  { *eax = c3b_a_core(r, C3B_A_OPFF); }
+
+/* 0x18350's mutants. */
+#define C3B_B_TAB    0x01u   /* the table is always 0xCEB00 */
+#define C3B_B_ANCHOR 0x02u   /* the anchor is not scaled by 2 */
+#define C3B_B_NEG    0x04u   /* the bit-15 negation is skipped */
+#define C3B_B_BIT    0x08u   /* the actor index is not scaled by 0x20 */
+#define C3B_B_SIDE   0x10u   /* side 0's char is always read */
+static void c3b_b_core(const u32 *r, u32 mut)
+{
+    u32 side = r[R_EAX], anchor = r[R_EDX];
+    u32 ch = (u32)DSB(DS_001077B0 + ((mut & C3B_B_SIDE) ? 0u : side) * 0x94u + 0x7Au);
+    u32 a;
+    switch ((mut & C3B_B_TAB) ? 0u : ch) {
+    case 0u: a = 0x000CEB00u; break;
+    case 1u: a = 0x000CF399u; break;
+    case 2u: a = 0x000CFC32u; break;
+    case 3u: a = 0x000D033Bu; break;
+    case 4u: a = 0x000D0A44u; break;
+    case 5u: a = 0x000CEB00u; break;
+    case 6u: a = 0x000CF399u; break;
+    default: a = 0x000CEB00u; break;
+    }
+    a += (mut & C3B_B_ANCHOR) ? anchor : anchor * 2u;
+    const u8 *p = mem + a;
+    DSD(0x00100AB0u + side * 8u) = (u32)(s32)(s8)p[0];
+    DSD(0x00100AB4u + side * 8u) = (u32)(s32)(s8)p[1];
+    if (!(mut & C3B_B_NEG)) {
+        int clear = fighter_actor_bit15_clear(side);
+        if (mut & C3B_B_BIT) {
+            u32 rec = DSD(DS_001077B0 + side * 0x94u);
+            u32 actor = DSD(DS_001014EC) + (u32)DSW(rec + 0x56u);
+            clear = (DSW(actor) & 0x8000u) == 0u;
+        }
+        if (!clear)
+            DSD(0x00100AB0u + side * 8u) = (u32)(-(s32)DSD(0x00100AB0u + side * 8u));
+    }
+    DSD(0x00100AB0u + side * 8u) <<= 6;
+    DSD(0x00100AB4u + side * 8u) <<= 6;
+}
+static void b_c3b_18350(const u32 *r, u32 *eax)
+{ fighter_18350(r[R_EAX], r[R_EDX]); *eax = 0u; }
+static void m_c3b_b_tab(const u32 *r, u32 *eax)    { c3b_b_core(r, C3B_B_TAB); *eax = 0u; }
+static void m_c3b_b_anchor(const u32 *r, u32 *eax) { c3b_b_core(r, C3B_B_ANCHOR); *eax = 0u; }
+static void m_c3b_b_neg(const u32 *r, u32 *eax)    { c3b_b_core(r, C3B_B_NEG); *eax = 0u; }
+static void m_c3b_b_bit(const u32 *r, u32 *eax)    { c3b_b_core(r, C3B_B_BIT); *eax = 0u; }
+static void m_c3b_b_side(const u32 *r, u32 *eax)   { c3b_b_core(r, C3B_B_SIDE); *eax = 0u; }
+
+/* 0x18540's mutants. */
+#define C3B_C_DEFAULT 0x01u  /* every character's camera is 0xE6DD0 */
+#define C3B_C_NOCLAMP 0x02u  /* the negative/limit clamp is skipped */
+#define C3B_C_GT      0x04u  /* the limit compare is >, not >= */
+#define C3B_C_MASK    0x08u  /* the sprite mask is 0xFFFF */
+#define C3B_C_IDX     0x10u  /* the actor index is not scaled by 0x20 */
+#define C3B_C_SIDE    0x20u  /* side 0 is always used */
+static void c3b_c_core(const u32 *r, u32 mut)
+{
+    u32 side = (mut & C3B_C_SIDE) ? 0u : r[R_EAX];
+    u32 slot = DSD(DS_001077A8 + side * 4u);
+    if (slot == 0u) return;
+    u32 ch = (u32)DSB(slot + 0x7Au);
+    u32 cam;
+    switch ((mut & C3B_C_DEFAULT) ? 0u : ch) {
+    case 1u: cam = (u32)DSW(0x000E39D0u); break;
+    case 2u: cam = (u32)DSW(0x000ECBD8u); break;
+    case 3u: cam = (u32)DSW(0x000D2134u); break;
+    case 4u: cam = (u32)DSW(0x000EA604u); break;
+    case 5u: cam = (u32)DSW(0x000D3E08u); break;
+    case 6u: cam = (u32)DSW(0x000E061Cu); break;
+    default: cam = (u32)DSW(0x000E6DD0u); break;
+    }
+    u32 idx = (u32)DSW(DSD(slot) + 0x56u);
+    u32 sprite = (u32)DSW(DSD(DS_001014EC) + ((mut & C3B_C_IDX) ? idx : idx * 0x20u));
+    sprite &= (mut & C3B_C_MASK) ? 0xFFFFu : 0x7FFFu;
+    s32 anchor = (s32)sprite - (s32)cam;
+    DSD(0x00100AF0u + side * 4u) = (u32)anchor;
+    if (!(mut & C3B_C_NOCLAMP)) {
+        u32 lim = DSD(0x000E6DB4u + ch * 4u);
+        if (anchor < 0 || ((mut & C3B_C_GT) ? ((u32)anchor > lim) : ((u32)anchor >= lim)))
+            DSD(0x00100AF0u + side * 4u) = 0u;
+    }
+}
+static void b_c3b_18540(const u32 *r, u32 *eax)
+{ fighter_18540(r[R_EAX]); *eax = 0u; }
+static void m_c3b_c_default(const u32 *r, u32 *eax) { c3b_c_core(r, C3B_C_DEFAULT); *eax = 0u; }
+static void m_c3b_c_noclamp(const u32 *r, u32 *eax) { c3b_c_core(r, C3B_C_NOCLAMP); *eax = 0u; }
+static void m_c3b_c_gt(const u32 *r, u32 *eax)      { c3b_c_core(r, C3B_C_GT); *eax = 0u; }
+static void m_c3b_c_mask(const u32 *r, u32 *eax)    { c3b_c_core(r, C3B_C_MASK); *eax = 0u; }
+static void m_c3b_c_idx(const u32 *r, u32 *eax)     { c3b_c_core(r, C3B_C_IDX); *eax = 0u; }
+static void m_c3b_c_side(const u32 *r, u32 *eax)    { c3b_c_core(r, C3B_C_SIDE); *eax = 0u; }
+
+/* 0x18788's mutants. */
+#define C3B_Y_BIT    0x01u   /* the +0x42 test is bit 2 */
+#define C3B_Y_REC    0x02u   /* the record offset is +0x18 */
+#define C3B_Y_CALL   0x04u   /* the 0x18540 re-latch is skipped */
+#define C3B_Y_ALWAYS 0x08u   /* the anchor test always re-runs 0x18350 */
+#define C3B_Y_ADD    0x10u   /* the two terms are added */
+static u32 c3b_y_core(const u32 *r, u32 mut)
+{
+    u32 side = r[R_EAX];
+    u32 slot = DS_001077B0 + side * 0x94u;
+    if ((DSB(slot + 0x42u) & ((mut & C3B_Y_BIT) ? 0x04u : 0x08u)) != 0u)
+        return DSD(DSD(slot) + ((mut & C3B_Y_REC) ? 0x18u : 0x1Cu));
+    if (!(mut & C3B_Y_CALL)) fighter_18540(side);
+    {
+        u32 anchor = DSD(DS_00100AF0 + side * 4u);
+        if ((mut & C3B_Y_ALWAYS) || anchor != DSD(slot + 0x20u))
+            fighter_18350(side, anchor);
+    }
+    if (mut & C3B_Y_ADD) return DSD(slot + 0x30u) + DSD(DS_00100AB4 + side * 8u);
+    return DSD(slot + 0x30u) - DSD(DS_00100AB4 + side * 8u);
+}
+static void b_c3b_hit_record_y(const u32 *r, u32 *eax) { *eax = hit_record_y(r[R_EAX]); }
+static void m_c3b_y_bit(const u32 *r, u32 *eax)    { *eax = c3b_y_core(r, C3B_Y_BIT); }
+static void m_c3b_y_rec(const u32 *r, u32 *eax)    { *eax = c3b_y_core(r, C3B_Y_REC); }
+static void m_c3b_y_call(const u32 *r, u32 *eax)   { *eax = c3b_y_core(r, C3B_Y_CALL); }
+static void m_c3b_y_always(const u32 *r, u32 *eax) { *eax = c3b_y_core(r, C3B_Y_ALWAYS); }
+static void m_c3b_y_add(const u32 *r, u32 *eax)    { *eax = c3b_y_core(r, C3B_Y_ADD); }
+
+/* 0x38154's mutants. */
+#define C3B_D_MODE8 0x01u   /* DS_001088BD == 8 alone selects the special v */
+#define C3B_D_RANGE 0x02u   /* the p range test is dropped (always in range) */
+#define C3B_D_ABS   0x04u   /* |v| is v */
+#define C3B_D_BIT   0x08u   /* the +0x28 test is bit 6, not bit 14 */
+#define C3B_D_BD    0x10u   /* the >= 8 test is == 8 */
+#define C3B_D_FLAG  0x20u   /* the flag is written to side 0 */
+#define C3B_D_CALL  0x40u   /* the 0x35838 call is skipped */
+#define C3B_D_DIR   0x80u   /* the 0x35838 dirbits are swapped */
+static void c3b_d_core(const u32 *r, u32 mut)
+{
+    u32 side = r[R_EAX];
+    u32 slot = DSD(DS_001077A8 + side * 4u);
+    if (slot == 0u) return;
+    u32 rec = DSD(slot);
+    if (rec == 0u) return;
+    s32 v;
+    s32 p = (s32)DSD(rec + 0x3Cu);
+    if (DSB(DS_001088BD) == 8u && ((mut & C3B_D_MODE8) || side == 1u)) {
+        if (p >= 0 && ((mut & C3B_D_RANGE) || p <= 0x5D00))
+            v = (s32)(0x1500u - (u32)p);
+        else
+            v = (s32)(DSD(DS_00108884) - 0x1500u - DSD(rec + 0x18u));
+    } else {
+        if (p >= 0 && ((mut & C3B_D_RANGE) || p <= 0x5D00))
+            v = (s32)(0x4900u - (side << 9) - DSD(rec + 0x3Cu));
+        else
+            v = (s32)(0x1F00u - (side << 9) + DSD(DS_00108884) - DSD(rec + 0x18u));
+    }
+    s32 av = (mut & C3B_D_ABS) ? v : (v < 0 ? (s32)(0u - (u32)v) : v);
+    u32 flag;
+    if ((DSW(rec + 0x28u) & ((mut & C3B_D_BIT) ? 0x0040u : 0x4000u)) != 0u) {
+        if (av > 0x200 && v >= 0) {
+            if (!(mut & C3B_D_CALL))
+                fighter_state_35838(slot, rec, (mut & C3B_D_DIR) ? 0x2000u : 0x1000u);
+            flag = 0u;
+        } else {
+            DSB(slot + 0x43u) |= 0x40u;
+            int big = (mut & C3B_D_BD) ? ((u32)DSB(DS_001088BD) == 8u)
+                                       : ((u32)DSB(DS_001088BD) >= 8u);
+            if (big) {
+                flag = 1u;
+            } else {
+                fighter_state_36638(slot, rec);
+                flag = 0u;
+            }
+        }
+    } else if (av <= 0x200) {
+        flag = 1u;
+    } else {
+        if (!(mut & C3B_D_CALL))
+            fighter_state_35838(slot, rec, (mut & C3B_D_DIR) ? 0x1000u : 0x2000u);
+        flag = 0u;
+    }
+    DSB(DS_001078F0 + ((mut & C3B_D_FLAG) ? 0u : side)) = (u8)flag;
+    if (DSB(DS_001078F0 + side) != 0u && DSB(slot + 0x53u) != 0u) {
+        fighter_state_367dc(slot, rec);
+        DSB(slot + 0x52u) = 9u;
+        return;
+    }
+    if (DSB(DS_001078F0 + side) == 0u)
+        DSB(slot + 0x53u) = 0x0Cu;
+}
+static void b_c3b_fighter_38154(const u32 *r, u32 *eax)
+{ fighter_38154(r[R_EAX]); *eax = 0u; }
+static void m_c3b_d_mode8(const u32 *r, u32 *eax) { c3b_d_core(r, C3B_D_MODE8); *eax = 0u; }
+static void m_c3b_d_range(const u32 *r, u32 *eax) { c3b_d_core(r, C3B_D_RANGE); *eax = 0u; }
+static void m_c3b_d_abs(const u32 *r, u32 *eax)   { c3b_d_core(r, C3B_D_ABS); *eax = 0u; }
+static void m_c3b_d_bit(const u32 *r, u32 *eax)   { c3b_d_core(r, C3B_D_BIT); *eax = 0u; }
+static void m_c3b_d_bd(const u32 *r, u32 *eax)    { c3b_d_core(r, C3B_D_BD); *eax = 0u; }
+static void m_c3b_d_flag(const u32 *r, u32 *eax)  { c3b_d_core(r, C3B_D_FLAG); *eax = 0u; }
+static void m_c3b_d_call(const u32 *r, u32 *eax)  { c3b_d_core(r, C3B_D_CALL); *eax = 0u; }
+static void m_c3b_d_dir(const u32 *r, u32 *eax)   { c3b_d_core(r, C3B_D_DIR); *eax = 0u; }
+
+/* The voice rows' mutants (record 2026-10-05-reverse-c3b): the AIL runtime calls go through the
+ * port's host wrappers (AIL_sample_status/AIL_stop_sample/AIL_init_sample/AIL_stop_sequence and
+ * flow.c's snd_music_playing), whose first-statement seams record them; the mutants call the same
+ * wrappers (via sound_slot_handle, the exported s_samples accessor), so only the mutation differs. */
+#define C3B_M_GATE 0x01u    /* the sequence-handle gate is dropped */
+#define C3B_M_PLAY 0x02u    /* the playing predicate is not consulted */
+#define C3B_M_STOP 0x04u    /* the 0x5DEAF stop is skipped */
+#define C3B_M_ZERO 0x08u    /* the current song/pause bytes are not cleared */
+#define C3B_M_AL   0x10u    /* AL = 0 on the stop path */
+#define C3B_M_CC   0x20u    /* the pending song is not cleared */
+static u32 c3b_music_stop_core(u32 mut)
+{
+    u32 seq = DSD(DS_001028C0);
+    if (!(mut & C3B_M_ZERO)) {
+        DSD(DS_001028D4) = 0;
+        DSB(DS_001028D9) = 0;
+    }
+    if (seq == 0u && !(mut & C3B_M_GATE)) return 0;
+    if (snd_music_playing() == 0u && !(mut & C3B_M_PLAY)) return 0;
+    if (!(mut & C3B_M_CC)) DSD(DS_001028CC) = 0;
+    if (!(mut & C3B_M_STOP)) AIL_stop_sequence(NULL);
+    return (mut & C3B_M_AL) ? 0u : 1u;
+}
+static void b_c3b_snd_music_stop(const u32 *r, u32 *eax)
+{ (void)r; *eax = snd_music_stop(); }
+static void m_c3b_m_gate(const u32 *r, u32 *eax) { (void)r; *eax = c3b_music_stop_core(C3B_M_GATE); }
+static void m_c3b_m_play(const u32 *r, u32 *eax) { (void)r; *eax = c3b_music_stop_core(C3B_M_PLAY); }
+static void m_c3b_m_stop(const u32 *r, u32 *eax) { (void)r; *eax = c3b_music_stop_core(C3B_M_STOP); }
+static void m_c3b_m_zero(const u32 *r, u32 *eax) { (void)r; *eax = c3b_music_stop_core(C3B_M_ZERO); }
+static void m_c3b_m_al(const u32 *r, u32 *eax)   { (void)r; *eax = c3b_music_stop_core(C3B_M_AL); }
+static void m_c3b_m_cc(const u32 *r, u32 *eax)   { (void)r; *eax = c3b_music_stop_core(C3B_M_CC); }
+
+#define C3B_P_DRIVER 0x01u  /* the DIG-driver gate is dropped */
+#define C3B_P_STATUS 0x02u  /* the playing status is 2, not 4 */
+#define C3B_P_CLEAR  0x04u  /* a stopped slot's +0x0C is not cleared */
+#define C3B_P_SCAN   0x08u  /* the scan stops at the first stopped match */
+#define C3B_P_WIDE   0x10u  /* the +0x0C compare is a word */
+#define C3B_P_AL     0x20u  /* any match returns 1 */
+static u32 c3b_sample_playing_core(const u32 *r, u32 mut)
+{
+    u32 h = r[R_EAX];
+    if (DSD(DS_001028C8) == 0u && !(mut & C3B_P_DRIVER)) return 0;
+    for (u32 k = 0; k < 4u; k++) {
+        u32 off = k * 0x18u;
+        u32 cur = (mut & C3B_P_WIDE) ? (u32)DSW(DS_0010286C + off)
+                                     : DSD(DS_0010286C + off);
+        if (cur != h) continue;
+        s32 st = AIL_sample_status(sound_slot_handle(k));
+        if ((mut & C3B_P_AL) || ((mut & C3B_P_STATUS) ? st == 2 : st == 4)) return 1;
+        if (!(mut & C3B_P_CLEAR)) DSD(DS_0010286C + off) = 0;
+        if (mut & C3B_P_SCAN) return 0;
+    }
+    return 0;
+}
+static void b_c3b_snd_sample_playing(const u32 *r, u32 *eax)
+{ *eax = snd_sample_playing(r[R_EAX]); }
+static void m_c3b_p_driver(const u32 *r, u32 *eax) { *eax = c3b_sample_playing_core(r, C3B_P_DRIVER); }
+static void m_c3b_p_status(const u32 *r, u32 *eax) { *eax = c3b_sample_playing_core(r, C3B_P_STATUS); }
+static void m_c3b_p_clear(const u32 *r, u32 *eax)  { *eax = c3b_sample_playing_core(r, C3B_P_CLEAR); }
+static void m_c3b_p_scan(const u32 *r, u32 *eax)   { *eax = c3b_sample_playing_core(r, C3B_P_SCAN); }
+static void m_c3b_p_wide(const u32 *r, u32 *eax)   { *eax = c3b_sample_playing_core(r, C3B_P_WIDE); }
+static void m_c3b_p_al(const u32 *r, u32 *eax)     { *eax = c3b_sample_playing_core(r, C3B_P_AL); }
+
+#define C3B_A_DRIVER 0x01u  /* the DIG-driver gate is dropped */
+#define C3B_A_SKIP2  0x02u  /* the status-2 skip is dropped */
+#define C3B_A_CLR4   0x04u  /* the +0x04 queued handle is not cleared */
+#define C3B_A_CLRC   0x08u  /* the +0x0C current handle is not cleared */
+#define C3B_A_AL     0x10u  /* AL = 0 */
+#define C3B_A_ORDER  0x20u  /* the stop/init pair is swapped */
+static u32 c3b_stop_all_core(u32 mut)
+{
+    if (DSD(DS_001028C8) == 0u && !(mut & C3B_A_DRIVER)) return 0;
+    for (u32 k = 0; k < 4u; k++) {
+        u32 off = k * 0x18u;
+        if (!(mut & C3B_A_CLR4)) DSD(DS_00102864 + off) = 0;
+        if (!(mut & C3B_A_CLRC)) DSD(DS_0010286C + off) = 0;
+        s32 st = AIL_sample_status(sound_slot_handle(k));
+        if (st == 2 && !(mut & C3B_A_SKIP2)) continue;
+        if (mut & C3B_A_ORDER) {
+            AIL_init_sample(sound_slot_handle(k));
+            AIL_stop_sample(sound_slot_handle(k));
+        } else {
+            AIL_stop_sample(sound_slot_handle(k));
+            AIL_init_sample(sound_slot_handle(k));
+        }
+    }
+    return (mut & C3B_A_AL) ? 0u : 1u;
+}
+static void b_c3b_snd_samples_stop_all(const u32 *r, u32 *eax)
+{ (void)r; *eax = snd_samples_stop_all(); }
+static void m_c3b_a_driver(const u32 *r, u32 *eax) { (void)r; *eax = c3b_stop_all_core(C3B_A_DRIVER); }
+static void m_c3b_a_skip2(const u32 *r, u32 *eax)  { (void)r; *eax = c3b_stop_all_core(C3B_A_SKIP2); }
+static void m_c3b_a_clr4(const u32 *r, u32 *eax)   { (void)r; *eax = c3b_stop_all_core(C3B_A_CLR4); }
+static void m_c3b_a_clrc(const u32 *r, u32 *eax)   { (void)r; *eax = c3b_stop_all_core(C3B_A_CLRC); }
+static void m_c3b_a_al(const u32 *r, u32 *eax)     { (void)r; *eax = c3b_stop_all_core(C3B_A_AL); }
+static void m_c3b_a_order(const u32 *r, u32 *eax)  { (void)r; *eax = c3b_stop_all_core(C3B_A_ORDER); }
+
+#define C3B_S_DRIVER  0x01u /* the DIG-driver gate is dropped */
+#define C3B_S_EQ2     0x02u /* the stop condition is status == 2 */
+#define C3B_S_CLEAR   0x04u /* the matched +0x0C is not cleared */
+#define C3B_S_NOLIMIT 0x08u /* the scan does not stop at the first match */
+#define C3B_S_AL      0x10u /* AL = 0 after a stop */
+#define C3B_S_CALLS   0x20u /* the stop/init calls are skipped */
+static u32 c3b_sample_stop_core(const u32 *r, u32 mut)
+{
+    u32 h = r[R_EAX];
+    if (DSD(DS_001028C8) == 0u && !(mut & C3B_S_DRIVER)) return 0;
+    for (u32 k = 0; k < 4u; k++) {
+        u32 off = k * 0x18u;
+        if (DSD(DS_0010286C + off) != h) continue;
+        s32 st = AIL_sample_status(sound_slot_handle(k));
+        int stop = (mut & C3B_S_EQ2) ? (st == 2) : (st != 2);
+        if (!stop) continue;
+        if (!(mut & C3B_S_CALLS)) {
+            AIL_stop_sample(sound_slot_handle(k));
+            AIL_init_sample(sound_slot_handle(k));
+        }
+        if (!(mut & C3B_S_CLEAR)) DSD(DS_0010286C + off) = 0;
+        if (!(mut & C3B_S_NOLIMIT)) return (mut & C3B_S_AL) ? 0u : 1u;
+    }
+    return 0;
+}
+static void b_c3b_snd_sample_stop(const u32 *r, u32 *eax)
+{ *eax = snd_sample_stop(r[R_EAX]); }
+static void m_c3b_s_driver(const u32 *r, u32 *eax)  { *eax = c3b_sample_stop_core(r, C3B_S_DRIVER); }
+static void m_c3b_s_eq2(const u32 *r, u32 *eax)     { *eax = c3b_sample_stop_core(r, C3B_S_EQ2); }
+static void m_c3b_s_clear(const u32 *r, u32 *eax)   { *eax = c3b_sample_stop_core(r, C3B_S_CLEAR); }
+static void m_c3b_s_nolimit(const u32 *r, u32 *eax) { *eax = c3b_sample_stop_core(r, C3B_S_NOLIMIT); }
+static void m_c3b_s_al(const u32 *r, u32 *eax)      { *eax = c3b_sample_stop_core(r, C3B_S_AL); }
+static void m_c3b_s_calls(const u32 *r, u32 *eax)   { *eax = c3b_sample_stop_core(r, C3B_S_CALLS); }
+
+/* 0x1CC28's mutants. */
+#define C3B_Q_DRIVER    0x0001u  /* the DIG-driver gate is dropped */
+#define C3B_Q_PAUSE     0x0002u  /* the sample-pause gate is dropped */
+#define C3B_Q_SIZE      0x0004u  /* the >0x6000 size selector is flipped */
+#define C3B_Q_ARMBUF    0x0008u  /* the arm's buffer test is forced true */
+#define C3B_Q_ARMQ      0x0010u  /* the arm's +0x04 test is forced true */
+#define C3B_Q_ARMST     0x0020u  /* the arm's status test is forced true */
+#define C3B_Q_ORDER     0x0040u  /* the scan runs 0..3, not 3..0 */
+#define C3B_Q_CANDCMP   0x0080u  /* the candidate compare is >=, not > */
+#define C3B_Q_CANDMIN   0x0100u  /* the candidate minimum is not updated */
+#define C3B_Q_TAILRET   0x0200u  /* the tail returns 0 */
+#define C3B_Q_TAILSTOP  0x0400u  /* the tail's stop/init is skipped */
+#define C3B_Q_TAILORDER 0x0800u  /* the tail's stop/init is swapped */
+#define C3B_Q_LOOPBYTE  0x1000u  /* the loop byte is the handle's low byte */
+static void c3b_q_store(u32 off, u32 h, u32 loop, u32 now)
+{
+    DSD(DS_00102864 + off) = h;
+    DSB(DS_00102868 + off) = (u8)loop;
+    DSD(DS_00102874 + off) = now;
+}
+static u32 c3b_queue_core(const u32 *r, u32 mut)
+{
+    u32 h = r[R_EAX], loop = r[R_EDX] & 0xFFu;
+    if (DSD(DS_001028C8) == 0u && !(mut & C3B_Q_DRIVER)) return 0u;
+    if (DSB(DS_001028DB) != 0u && !(mut & C3B_Q_PAUSE)) return 0u;
+    u32 now = DSD(DS_00101500);
+    const u8 *p = (const u8 *)res_resolve(h);
+    if (p == NULL) return 1u;
+    u32 size = DSD((u32)(p - mem));
+    u32 cand = 0u;
+    int fast = size > 0x6000u;
+    if (mut & C3B_Q_SIZE) fast = !fast;
+    if (fast) {
+        int buf = DSD(DS_00102870) != 0u;
+        int freeq = DSD(DS_00102864) == 0u;
+        if (mut & C3B_Q_ARMBUF) buf = 1;
+        if (mut & C3B_Q_ARMQ) freeq = 1;
+        if (buf && freeq) {
+            s32 st = AIL_sample_status(sound_slot_handle(0u));
+            if (st != 4 || (mut & C3B_Q_ARMST)) {
+                c3b_q_store(0u, h, (mut & C3B_Q_LOOPBYTE) ? h : loop, now);
+                return 1u;
+            }
+        }
+    } else {
+        u32 min = now;
+        for (u32 i = 0; i < 4u; i++) {
+            u32 k = (mut & C3B_Q_ORDER) ? i : 3u - i;
+            u32 off = k * 0x18u;
+            int buf = DSD(DS_00102870 + off) != 0u;
+            int freeq = DSD(DS_00102864 + off) == 0u;
+            if (mut & C3B_Q_ARMBUF) buf = 1;
+            if (mut & C3B_Q_ARMQ) freeq = 1;
+            if (buf && freeq) {
+                s32 st = AIL_sample_status(sound_slot_handle(k));
+                if (st != 4 || (mut & C3B_Q_ARMST)) {
+                    c3b_q_store(off, h, (mut & C3B_Q_LOOPBYTE) ? h : loop, now);
+                    return 1u;
+                }
+            }
+            u32 t = DSD(DS_00102874 + off);
+            int better = (mut & C3B_Q_CANDCMP) ? min >= t : min > t;
+            if (better) {
+                cand = k;
+                if (!(mut & C3B_Q_CANDMIN)) min = t;
+            }
+        }
+    }
+    {
+        u32 off = cand * 0x18u;
+        if (!(mut & C3B_Q_TAILSTOP)) {
+            if (mut & C3B_Q_TAILORDER) {
+                AIL_init_sample(sound_slot_handle(cand));
+                AIL_stop_sample(sound_slot_handle(cand));
+            } else {
+                AIL_stop_sample(sound_slot_handle(cand));
+                AIL_init_sample(sound_slot_handle(cand));
+            }
+        }
+        c3b_q_store(off, h, (mut & C3B_Q_LOOPBYTE) ? h : loop, now);
+    }
+    return (mut & C3B_Q_TAILRET) ? 0u : 1u;
+}
+static void b_c3b_snd_sample_queue(const u32 *r, u32 *eax)
+{ *eax = snd_sample_queue(r[R_EAX], r[R_EDX]); }
+static void m_c3b_q_driver(const u32 *r, u32 *eax)    { *eax = c3b_queue_core(r, C3B_Q_DRIVER); }
+static void m_c3b_q_pause(const u32 *r, u32 *eax)     { *eax = c3b_queue_core(r, C3B_Q_PAUSE); }
+static void m_c3b_q_size(const u32 *r, u32 *eax)      { *eax = c3b_queue_core(r, C3B_Q_SIZE); }
+static void m_c3b_q_armbuf(const u32 *r, u32 *eax)    { *eax = c3b_queue_core(r, C3B_Q_ARMBUF); }
+static void m_c3b_q_armq(const u32 *r, u32 *eax)      { *eax = c3b_queue_core(r, C3B_Q_ARMQ); }
+static void m_c3b_q_armst(const u32 *r, u32 *eax)     { *eax = c3b_queue_core(r, C3B_Q_ARMST); }
+static void m_c3b_q_order(const u32 *r, u32 *eax)     { *eax = c3b_queue_core(r, C3B_Q_ORDER); }
+static void m_c3b_q_candcmp(const u32 *r, u32 *eax)   { *eax = c3b_queue_core(r, C3B_Q_CANDCMP); }
+static void m_c3b_q_candmin(const u32 *r, u32 *eax)   { *eax = c3b_queue_core(r, C3B_Q_CANDMIN); }
+static void m_c3b_q_tailret(const u32 *r, u32 *eax)   { *eax = c3b_queue_core(r, C3B_Q_TAILRET); }
+static void m_c3b_q_tailstop(const u32 *r, u32 *eax)  { *eax = c3b_queue_core(r, C3B_Q_TAILSTOP); }
+static void m_c3b_q_tailorder(const u32 *r, u32 *eax) { *eax = c3b_queue_core(r, C3B_Q_TAILORDER); }
+static void m_c3b_q_loopbyte(const u32 *r, u32 *eax)  { *eax = c3b_queue_core(r, C3B_Q_LOOPBYTE); }
+
+/* 0x249C0's mutants. */
+static void b_c3b_list_insert_before(const u32 *r, u32 *eax)
+{ effects_list_insert_before(r[R_EAX], r[R_EDX]); *eax = 0u; }
+/* prev is read from [at], not [at+4]. */
+static void m_c3b_lb_prev(const u32 *r, u32 *eax)
+{
+    u32 at = r[R_EAX], rec = r[R_EDX];
+    u32 prev = DSD(at);
+    DSD(at + 4u) = rec; DSD(rec) = at; DSD(rec + 4u) = prev; DSD(prev) = rec;
+    *eax = 0u;
+}
+/* the [prev] back link is not written. */
+static void m_c3b_lb_link(const u32 *r, u32 *eax)
+{
+    u32 at = r[R_EAX], rec = r[R_EDX];
+    u32 prev = DSD(at + 4u);
+    DSD(at + 4u) = rec; DSD(rec) = at; DSD(rec + 4u) = prev;
+    *eax = 0u;
+}
+/* rec's back link points at `at`, not prev. */
+static void m_c3b_lb_back(const u32 *r, u32 *eax)
+{
+    u32 at = r[R_EAX], rec = r[R_EDX];
+    u32 prev = DSD(at + 4u);
+    DSD(at + 4u) = rec; DSD(rec) = at; DSD(rec + 4u) = at; DSD(prev) = rec;
+    *eax = 0u;
+}
+/* the at back link is left alone: nothing is inserted. */
+static void m_c3b_lb_head(const u32 *r, u32 *eax)
+{
+    u32 at = r[R_EAX], rec = r[R_EDX];
+    u32 prev = DSD(at + 4u);
+    DSD(rec) = at; DSD(rec + 4u) = prev; DSD(prev) = rec;
+    *eax = 0u;
+}
+/* rec's forward link points at prev, not at. */
+static void m_c3b_lb_forward(const u32 *r, u32 *eax)
+{
+    u32 at = r[R_EAX], rec = r[R_EDX];
+    u32 prev = DSD(at + 4u);
+    DSD(at + 4u) = rec; DSD(rec) = prev; DSD(rec + 4u) = prev; DSD(prev) = rec;
+    *eax = 0u;
+}
+
+/* 0x13420's mutants: the port body with one store or call moved. */
+static void b_13420(const u32 *r, u32 *eax)            { effect_teardown(r[R_EAX]); *eax = 0u; }
+/* the active-list unlink is skipped. */
+static void m_13420_unlink(const u32 *r, u32 *eax)
+{
+    u32 rec = r[R_EAX];
+    u8 saved = DSB(DS_0009AF3C);
+    DSB(DS_0009AF3C) = 1;
+    DSB(DS_0009AF3C) = saved;
+    u32 src = DSD(rec + 8);
+    switch (DSB(rec + 0x0cu)) {
+    case 0: case 2: case 3: case 5:
+        palette_record_flagged(DSD(src), DSD(src + 8), DSD(src + 0x0cu)); break;
+    case 1:
+        palette_record(rec + 0x10u, DSD(src + 8) + (u32)DSB(rec + 0x0fu), 1, 0); break;
+    case 4:
+        palette_record(0x000FCCF0u, DSD(src + 8), DSD(src + 0x0cu), 0); break;
+    default: break;
+    }
+    effects_list_insert_after(DS_000FCCE8, rec);
+    *eax = 0u;
+}
+/* the lock is not restored after the walk. */
+static void m_13420_restore(const u32 *r, u32 *eax)
+{
+    u32 rec = r[R_EAX];
+    DSB(DS_0009AF3C) = 1;
+    effects_list_unlink(rec);
+    u32 src = DSD(rec + 8);
+    switch (DSB(rec + 0x0cu)) {
+    case 0: case 2: case 3: case 5:
+        palette_record_flagged(DSD(src), DSD(src + 8), DSD(src + 0x0cu)); break;
+    case 1:
+        palette_record(rec + 0x10u, DSD(src + 8) + (u32)DSB(rec + 0x0fu), 1, 0); break;
+    case 4:
+        palette_record(0x000FCCF0u, DSD(src + 8), DSD(src + 0x0cu), 0); break;
+    default: break;
+    }
+    effects_list_insert_after(DS_000FCCE8, rec);
+    *eax = 0u;
+}
+/* type 1 takes the no-op default. */
+static void m_13420_type1(const u32 *r, u32 *eax)
+{
+    u32 rec = r[R_EAX];
+    u8 saved = DSB(DS_0009AF3C);
+    DSB(DS_0009AF3C) = 1;
+    effects_list_unlink(rec);
+    DSB(DS_0009AF3C) = saved;
+    u32 src = DSD(rec + 8);
+    switch (DSB(rec + 0x0cu)) {
+    case 0: case 2: case 3: case 5:
+        palette_record_flagged(DSD(src), DSD(src + 8), DSD(src + 0x0cu)); break;
+    case 4:
+        palette_record(0x000FCCF0u, DSD(src + 8), DSD(src + 0x0cu), 0); break;
+    default: break;
+    }
+    effects_list_insert_after(DS_000FCCE8, rec);
+    *eax = 0u;
+}
+/* the flagged path stores flag 0. */
+static void m_13420_flag(const u32 *r, u32 *eax)
+{
+    u32 rec = r[R_EAX];
+    u8 saved = DSB(DS_0009AF3C);
+    DSB(DS_0009AF3C) = 1;
+    effects_list_unlink(rec);
+    DSB(DS_0009AF3C) = saved;
+    u32 src = DSD(rec + 8);
+    switch (DSB(rec + 0x0cu)) {
+    case 0: case 2: case 3: case 5:
+        palette_record(DSD(src), DSD(src + 8), DSD(src + 0x0cu), 0); break;
+    case 1:
+        palette_record(rec + 0x10u, DSD(src + 8) + (u32)DSB(rec + 0x0fu), 1, 0); break;
+    case 4:
+        palette_record(0x000FCCF0u, DSD(src + 8), DSD(src + 0x0cu), 0); break;
+    default: break;
+    }
+    effects_list_insert_after(DS_000FCCE8, rec);
+    *eax = 0u;
+}
+/* type 4 enqueues from the record's +0x10 block. */
+static void m_13420_buf(const u32 *r, u32 *eax)
+{
+    u32 rec = r[R_EAX];
+    u8 saved = DSB(DS_0009AF3C);
+    DSB(DS_0009AF3C) = 1;
+    effects_list_unlink(rec);
+    DSB(DS_0009AF3C) = saved;
+    u32 src = DSD(rec + 8);
+    switch (DSB(rec + 0x0cu)) {
+    case 0: case 2: case 3: case 5:
+        palette_record_flagged(DSD(src), DSD(src + 8), DSD(src + 0x0cu)); break;
+    case 1:
+        palette_record(rec + 0x10u, DSD(src + 8) + (u32)DSB(rec + 0x0fu), 1, 0); break;
+    case 4:
+        palette_record(rec + 0x10u, DSD(src + 8), DSD(src + 0x0cu), 0); break;
+    default: break;
+    }
+    effects_list_insert_after(DS_000FCCE8, rec);
+    *eax = 0u;
+}
+/* the tail insert is an insert-before. */
+static void m_13420_tail(const u32 *r, u32 *eax)
+{
+    u32 rec = r[R_EAX];
+    u8 saved = DSB(DS_0009AF3C);
+    DSB(DS_0009AF3C) = 1;
+    effects_list_unlink(rec);
+    DSB(DS_0009AF3C) = saved;
+    u32 src = DSD(rec + 8);
+    switch (DSB(rec + 0x0cu)) {
+    case 0: case 2: case 3: case 5:
+        palette_record_flagged(DSD(src), DSD(src + 8), DSD(src + 0x0cu)); break;
+    case 1:
+        palette_record(rec + 0x10u, DSD(src + 8) + (u32)DSB(rec + 0x0fu), 1, 0); break;
+    case 4:
+        palette_record(0x000FCCF0u, DSD(src + 8), DSD(src + 0x0cu), 0); break;
+    default: break;
+    }
+    effects_list_insert_before(DS_000FCCE8, rec);
+    *eax = 0u;
+}
+
+/* 0x49444's mutants: the port body with one test, index or call moved. */
+static void b_49444(const u32 *r, u32 *eax)            { actor_type_49444(r[R_EAX]); *eax = 0u; }
+/* the +0x1C test reads bit 5, not bit 1. */
+static void m_49444_bit(const u32 *r, u32 *eax)
+{
+    u32 rec = r[R_EAX], node = DSD(rec + 0x14u);
+    if (node == 0u) { *eax = 0u; return; }
+    if ((DSB(node + 0x1cu) & 0x20u) != 0u)
+        DSD(0x0010839Cu + (u32)((s32)DSD(node + 0x18u) >> 16) * 4u) = 0u;
+    if (DSD(node + 0x10u) != 0u) { actor_set_dead(DSD(node + 0x10u)); DSD(node + 0x10u) = 0u; }
+    effects_list_unlink(node);
+    effects_list_insert_after(0x001083C4u, node);
+    DSD(rec + 0x14u) = 0u;
+    *eax = 0u;
+}
+/* the entry is cleared one byte wide, not 4. */
+static void m_49444_idx(const u32 *r, u32 *eax)
+{
+    u32 rec = r[R_EAX], node = DSD(rec + 0x14u);
+    if (node == 0u) { *eax = 0u; return; }
+    if ((DSB(node + 0x1cu) & 2u) != 0u)
+        DSB(0x0010839Cu + (u32)((s32)DSD(node + 0x18u) >> 16)) = 0u;
+    if (DSD(node + 0x10u) != 0u) { actor_set_dead(DSD(node + 0x10u)); DSD(node + 0x10u) = 0u; }
+    effects_list_unlink(node);
+    effects_list_insert_after(0x001083C4u, node);
+    DSD(rec + 0x14u) = 0u;
+    *eax = 0u;
+}
+/* the node's +0x18 index is zero-extended, not sign-extended. */
+static void m_49444_sign(const u32 *r, u32 *eax)
+{
+    u32 rec = r[R_EAX], node = DSD(rec + 0x14u);
+    if (node == 0u) { *eax = 0u; return; }
+    if ((DSB(node + 0x1cu) & 2u) != 0u)
+        DSD(0x0010839Cu + (u32)(DSD(node + 0x18u) >> 16) * 4u) = 0u;
+    if (DSD(node + 0x10u) != 0u) { actor_set_dead(DSD(node + 0x10u)); DSD(node + 0x10u) = 0u; }
+    effects_list_unlink(node);
+    effects_list_insert_after(0x001083C4u, node);
+    DSD(rec + 0x14u) = 0u;
+    *eax = 0u;
+}
+/* the +0x10 child is not retired. */
+static void m_49444_child(const u32 *r, u32 *eax)
+{
+    u32 rec = r[R_EAX], node = DSD(rec + 0x14u);
+    if (node == 0u) { *eax = 0u; return; }
+    if ((DSB(node + 0x1cu) & 2u) != 0u)
+        DSD(0x0010839Cu + (u32)((s32)DSD(node + 0x18u) >> 16) * 4u) = 0u;
+    effects_list_unlink(node);
+    effects_list_insert_after(0x001083C4u, node);
+    DSD(rec + 0x14u) = 0u;
+    *eax = 0u;
+}
+/* the node is re-inserted before the sentinel. */
+static void m_49444_link(const u32 *r, u32 *eax)
+{
+    u32 rec = r[R_EAX], node = DSD(rec + 0x14u);
+    if (node == 0u) { *eax = 0u; return; }
+    if ((DSB(node + 0x1cu) & 2u) != 0u)
+        DSD(0x0010839Cu + (u32)((s32)DSD(node + 0x18u) >> 16) * 4u) = 0u;
+    if (DSD(node + 0x10u) != 0u) { actor_set_dead(DSD(node + 0x10u)); DSD(node + 0x10u) = 0u; }
+    effects_list_unlink(node);
+    effects_list_insert_before(0x001083C4u, node);
+    DSD(rec + 0x14u) = 0u;
+    *eax = 0u;
+}
+/* rec's +0x14 is left set. */
+static void m_49444_reclr(const u32 *r, u32 *eax)
+{
+    u32 rec = r[R_EAX], node = DSD(rec + 0x14u);
+    if (node == 0u) { *eax = 0u; return; }
+    if ((DSB(node + 0x1cu) & 2u) != 0u)
+        DSD(0x0010839Cu + (u32)((s32)DSD(node + 0x18u) >> 16) * 4u) = 0u;
+    if (DSD(node + 0x10u) != 0u) { actor_set_dead(DSD(node + 0x10u)); DSD(node + 0x10u) = 0u; }
+    effects_list_unlink(node);
+    effects_list_insert_after(0x001083C4u, node);
+    *eax = 0u;
+}
+
+/* 0x2B150's mutants: one re-implementation with a mutation selector. */
+typedef void (*c3b_s_cb2)(u32 rec);
+#define C3B_S_BIT    0x01u   /* the dead bit is set at +0x29 */
+#define C3B_S_GATE   0x02u   /* the callback gate tests the low byte of +0x2A */
+#define C3B_S_CLEAR  0x04u   /* rec+0x2B bit 6 is not cleared */
+#define C3B_S_TABLE  0x08u   /* the callback reads the cb1 table entry */
+#define C3B_S_PSET   0x10u   /* the pset stride is 0x10 */
+#define C3B_S_PAL    0x20u   /* the palette release is not called */
+#define C3B_S_RENDER 0x40u   /* the render unlist is skipped */
+static void c3b_s_core(const u32 *r, u32 *eax, u32 mut)
+{
+    u32 rec = r[R_EAX];
+    if (mut & C3B_S_BIT) DSB(rec + 0x29u) |= 8u;
+    else DSB(rec + 0x28u) |= 8u;
+    if (((mut & C3B_S_GATE) ? (DSW(rec + 0x2au) & 0x40u)
+                            : (DSW(rec + 0x2au) >> 8 & 0x40u)) != 0u) {
+        c3b_s_cb2 cb = (c3b_s_cb2)(void *)fn_resolve(
+            DSD(((mut & C3B_S_TABLE) ? 0x000BB9DCu : 0x000BB9E0u)
+                + (u32)DSB(rec + 0x48u) * 0xCu));
+        if (cb) cb(rec);
+        if (!(mut & C3B_S_CLEAR)) DSB(rec + 0x2bu) &= (u8)~0x40u;
+    }
+    u32 pset = DSD(0x001014ECu) + (u32)DSW(rec + 0x56u) * ((mut & C3B_S_PSET) ? 0x10u : 0x20u);
+    if (DSD(pset + 0x18u) != 0u) {
+        if (!(mut & C3B_S_PAL)) palette_release(DSD(pset + 0x18u));
+        DSD(pset + 0x18u) = 0u;
+    }
+    if (!(mut & C3B_S_RENDER)) render_list_remove(pset);
+    *eax = 0u;
+}
+static void b_2b150(const u32 *r, u32 *eax)            { set_dead(r[R_EAX]); *eax = 0u; }
+static void m_2b150_bit(const u32 *r, u32 *eax)        { c3b_s_core(r, eax, C3B_S_BIT); }
+static void m_2b150_gate(const u32 *r, u32 *eax)       { c3b_s_core(r, eax, C3B_S_GATE); }
+static void m_2b150_clear(const u32 *r, u32 *eax)      { c3b_s_core(r, eax, C3B_S_CLEAR); }
+static void m_2b150_table(const u32 *r, u32 *eax)      { c3b_s_core(r, eax, C3B_S_TABLE); }
+static void m_2b150_pset(const u32 *r, u32 *eax)       { c3b_s_core(r, eax, C3B_S_PSET); }
+static void m_2b150_pal(const u32 *r, u32 *eax)        { c3b_s_core(r, eax, C3B_S_PAL); }
+static void m_2b150_render(const u32 *r, u32 *eax)     { c3b_s_core(r, eax, C3B_S_RENDER); }
+
+/* 0x29DB8's mutants: one re-implementation with a mutation selector. */
+#define C3B_W_MASK  0x01u   /* the op byte is not masked to 0x7F */
+#define C3B_W_RINGM 0x02u   /* the ring index is not masked to 0x3F */
+#define C3B_W_RADIO 0x04u   /* the ring base reads +0x52, not +0x51 */
+#define C3B_W_W44   0x08u   /* the word stores keep the high byte */
+#define C3B_W_SWAP  0x10u   /* the parent form reads the child index +0x4B */
+#define C3B_W_BEP   0x20u   /* the parent byte stores use value, not DS_00105BE8 */
+#define C3B_W_CHILD 0x40u   /* the child form reads the parent index +0x4A */
+#define C3B_W_HIGH  0x80u   /* above 0x51 the ring takes the value */
+static void c3b_w_core(const u32 *r, u32 *eax, u32 mut)
+{
+    u32 rec = r[R_EAX], value = r[R_EBX];
+    u32 op = (u32)(u8)r[R_EDX];
+    u32 o = (mut & C3B_W_MASK) ? op : (op & 0x7fu);
+    if (o < 0x40u) {
+        u32 bi = (mut & C3B_W_RADIO) ? (u32)DSB(rec + 0x52u) : (u32)DSB(rec + 0x51u);
+        u32 idx = o + bi;
+        if (!(mut & C3B_W_RINGM)) idx &= 0x3Fu;
+        DSW(0x00105B4Cu + idx * 2u) = (u16)value;
+        *eax = 0u;
+        return;
+    }
+    switch (o) {
+    case 0x40: DSB(rec + 0x52u) = (u8)value; *eax = 0u; return;
+    case 0x41: DSB(rec + 0x53u) = (u8)value; *eax = 0u; return;
+    case 0x42: DSB(rec + 0x54u) = (u8)value; *eax = 0u; return;
+    case 0x43: DSB(rec + 0x55u) = (u8)value; *eax = 0u; return;
+    case 0x44:
+        DSW(rec + 0x56u) = (mut & C3B_W_W44) ? (u16)value : (u16)(value & 0xffu);
+        *eax = 0u;
+        return;
+    case 0x45: DSB(rec + 0x58u) = (u8)value; *eax = 0u; return;
+    default: break;
+    }
+    u32 base;
+    if (o <= 0x4bu)
+        base = DSD(0x001014F4u) + (u32)DSB(rec + ((mut & C3B_W_SWAP) ? 0x4bu : 0x4au)) * 0x68u;
+    else if (o <= 0x51u)
+        base = DSD(0x001014F4u) + (u32)DSB(rec + ((mut & C3B_W_CHILD) ? 0x4au : 0x4bu)) * 0x68u;
+    else if (mut & C3B_W_HIGH) {
+        DSW(0x00105B4Cu + ((o + (u32)DSB(rec + 0x51u)) & 0x3fu) * 2u) = (u16)value;
+        *eax = 0u;
+        return;
+    } else {
+        *eax = 0u;
+        return;
+    }
+    u8 sv = (mut & C3B_W_BEP) ? (u8)value : DSB(0x00105BE8u);
+    switch (o) {
+    case 0x46: case 0x4c: DSB(base + 0x52u) = sv; *eax = 0u; return;
+    case 0x47: case 0x4d: DSB(base + 0x53u) = sv; *eax = 0u; return;
+    case 0x48: case 0x4e: DSB(base + 0x54u) = sv; *eax = 0u; return;
+    case 0x49: case 0x4f: DSB(base + 0x55u) = sv; *eax = 0u; return;
+    case 0x4a: case 0x50:
+        DSW(base + 0x56u) = (mut & C3B_W_W44) ? (u16)value : (u16)(value & 0xffu);
+        *eax = 0u;
+        return;
+    default: DSB(base + 0x58u) = (u8)value; *eax = 0u; return;
+    }
+}
+static void b_29db8(const u32 *r, u32 *eax)
+{
+    anim_write_var(r[R_EAX], (u8)r[R_EDX], r[R_EBX]);
+    *eax = 0u;
+}
+
+/* 0x2B8F8's mutants: one re-implementation with a mutation selector. */
+#define C3B_O_OP    0x01u   /* the previous-op compare is against 0x1E */
+#define C3B_O_MASK  0x02u   /* the mode-0 byte return is zero-extended */
+#define C3B_O_ADV   0x04u   /* the 0x1F arm does not advance rec+8 */
+#define C3B_O_BE8   0x08u   /* the 0x1F arm stores the command byte, not the next word */
+#define C3B_O_SEL   0x10u   /* the selector uses 0x0F00 */
+#define C3B_O_SCALE 0x20u   /* the 0x1000/0x2000 arms scale by 4 */
+#define C3B_O_DEREF 0x40u   /* the 0x4000 arm stores the post-advance pointer */
+#define C3B_O_BYTE  0x80u   /* the byte return is zero-extended */
+static u32 c3b_o_core(u32 rec, u32 mut)
+{
+    u32 p = DSD(rec + 8);
+    u16 cw = DSW(p);
+    u16 mode = (u16)(cw & 0x6000u);
+    DSW(0x00105BE6u) = mode;
+    u8 op = (u8)DSW(0x00105BE4u);
+    u8 ob = (u8)(cw & 0xffu);
+    u32 cx;
+    if (op == (u16)((mut & C3B_O_OP) ? 0x1Eu : 0x1Fu)) {
+        u32 np = p + 2;
+        if (!(mut & C3B_O_ADV)) DSD(rec + 8) = np;
+        DSW(0x00105BE4u) = ob;
+        cx = DSW(np);
+        if (mode == 0) {
+            DSW(0x00105BE8u) = (u16)((mut & C3B_O_BE8) ? cw : cx);
+            return cx;
+        }
+    } else {
+        DSW(0x00105BE8u) = ob;
+        if (mode == 0) {
+            if (mut & C3B_O_MASK) return ob;
+            return ob > 0x7fu ? (0xff00u | (u32)ob) : (u32)ob;
+        }
+        cx = ob;
+    }
+    DSW(0x00105BE8u) = (u16)cx;
+    u32 v = anim_read_var(rec, cx);
+    u16 mode2 = DSW(0x00105BE6u);
+    if (mode2 == 0x2000u) return v & 0xffffu;
+    u32 p2 = DSD(rec + 8) + 2;
+    u32 np = p2 + 2;
+    DSD(rec + 8) = np;
+    if (mode2 == 0x4000u) {
+        DSD(0x00105BD4u) = (mut & C3B_O_DEREF) ? DSD(np) : DSD(p2);
+        return v & 0xffffu;
+    }
+    u32 base = DSD(rec + 0x0cu);
+    DSD(0x00105BD4u) = base;
+    u16 sel = (u16)(DSW(np) & (u16)((mut & C3B_O_SEL) ? 0x0f00u : 0xf000u));
+    u32 sc = (mut & C3B_O_SCALE) ? 4u : 2u;
+    if (sel == 0x1000u) {
+        v = (v & 0xffffu) * sc;
+    } else if (sel == 0x2000u) {
+        v = (v & 0xffffu) * sc + 1u;
+    } else if (sel == 0x4000u) {
+        base += (v & 0xffffu) * 2u;
+        DSD(0x00105BD4u) = base;
+        return DSW(base);
+    } else if (sel == 0x5000u) {
+        base += (v & 0xffffu) * 4u;
+        DSD(0x00105BD4u) = base;
+        return DSW(base);
+    } else if (sel == 0x6000u) {
+        base += (v & 0xffffu) * 4u + 2u;
+        DSD(0x00105BD4u) = base;
+        return DSW(base);
+    } else {
+        v &= 0xffffu;
+    }
+    u8 b = DSB(base + v);
+    if (mut & C3B_O_BYTE) return (u32)b;
+    return b > 0x7fu ? ((0xff00u | (u32)b) & 0xffffu) : (u32)b;
+}
+static void b_2b8f8(const u32 *r, u32 *eax)            { *eax = anim_operand(r[R_EAX]); }
+/* 0x3AFC4's mutants: one triple builder with a mutation selector. */
+#define C3B_T_IDX   0x01u   /* the table stride is 0x20 */
+#define C3B_T_SHIFT 0x02u   /* the table byte shifts left 5 */
+#define C3B_T_ADD   0x04u   /* EDX is not added to the shifted byte */
+#define C3B_T_O0    0x08u   /* the first word's stride is 12 */
+#define C3B_T_O1    0x10u   /* the second word's stride is 16 */
+#define C3B_T_O2    0x20u   /* the third word's stride is 4 */
+static void c3b_t_core(u32 out[3], u32 slot_char, s32 edx, u32 mut)
+{
+    if (edx < 0 || edx >= 0x40) {
+        out[0] = out[1] = out[2] = 0u;
+        return;
+    }
+    u32 c = ((u32)DSB(0x0010782Au + slot_char * ((mut & C3B_T_IDX) ? 0x20u : 0x94u))
+             << ((mut & C3B_T_SHIFT) ? 5u : 6u));
+    if (!(mut & C3B_T_ADD)) c += (u32)edx;
+    out[0] = 0x000DE114u + c * ((mut & C3B_T_O0) ? 12u : 11u);
+    out[1] = 0x000A3528u + c * ((mut & C3B_T_O1) ? 16u : 20u);
+    out[2] = 0x000A6728u + c * ((mut & C3B_T_O2) ? 4u : 6u);
+}
+static void b_3afc4(const u32 *r, u32 *eax)
+{
+    u32 out[3];
+    fighter_anim_triple(out, r[R_EAX], (s32)r[R_EDX]);
+    memcpy(mem + r[R_EBX], out, sizeof out);
+    *eax = 0u;
+}
+static void m_3afc4_idx(const u32 *r, u32 *eax)
+{
+    u32 out[3];
+    c3b_t_core(out, r[R_EAX], (s32)r[R_EDX], C3B_T_IDX);
+    memcpy(mem + r[R_EBX], out, sizeof out);
+    *eax = 0u;
+}
+static void m_3afc4_shift(const u32 *r, u32 *eax)
+{
+    u32 out[3];
+    c3b_t_core(out, r[R_EAX], (s32)r[R_EDX], C3B_T_SHIFT);
+    memcpy(mem + r[R_EBX], out, sizeof out);
+    *eax = 0u;
+}
+static void m_3afc4_add(const u32 *r, u32 *eax)
+{
+    u32 out[3];
+    c3b_t_core(out, r[R_EAX], (s32)r[R_EDX], C3B_T_ADD);
+    memcpy(mem + r[R_EBX], out, sizeof out);
+    *eax = 0u;
+}
+static void m_3afc4_o0(const u32 *r, u32 *eax)
+{
+    u32 out[3];
+    c3b_t_core(out, r[R_EAX], (s32)r[R_EDX], C3B_T_O0);
+    memcpy(mem + r[R_EBX], out, sizeof out);
+    *eax = 0u;
+}
+static void m_3afc4_o1(const u32 *r, u32 *eax)
+{
+    u32 out[3];
+    c3b_t_core(out, r[R_EAX], (s32)r[R_EDX], C3B_T_O1);
+    memcpy(mem + r[R_EBX], out, sizeof out);
+    *eax = 0u;
+}
+static void m_3afc4_o2(const u32 *r, u32 *eax)
+{
+    u32 out[3];
+    c3b_t_core(out, r[R_EAX], (s32)r[R_EDX], C3B_T_O2);
+    memcpy(mem + r[R_EBX], out, sizeof out);
+    *eax = 0u;
+}
+static void m_2b8f8_op(const u32 *r, u32 *eax)         { *eax = c3b_o_core(r[R_EAX], C3B_O_OP); }
+static void m_2b8f8_mask(const u32 *r, u32 *eax)       { *eax = c3b_o_core(r[R_EAX], C3B_O_MASK); }
+static void m_2b8f8_adv(const u32 *r, u32 *eax)        { *eax = c3b_o_core(r[R_EAX], C3B_O_ADV); }
+static void m_2b8f8_be8(const u32 *r, u32 *eax)        { *eax = c3b_o_core(r[R_EAX], C3B_O_BE8); }
+static void m_2b8f8_sel(const u32 *r, u32 *eax)        { *eax = c3b_o_core(r[R_EAX], C3B_O_SEL); }
+static void m_2b8f8_scale(const u32 *r, u32 *eax)      { *eax = c3b_o_core(r[R_EAX], C3B_O_SCALE); }
+static void m_2b8f8_deref(const u32 *r, u32 *eax)      { *eax = c3b_o_core(r[R_EAX], C3B_O_DEREF); }
+static void m_2b8f8_byte(const u32 *r, u32 *eax)       { *eax = c3b_o_core(r[R_EAX], C3B_O_BYTE); }
+static void m_29db8_mask(const u32 *r, u32 *eax)       { c3b_w_core(r, eax, C3B_W_MASK); }
+static void m_29db8_ringm(const u32 *r, u32 *eax)      { c3b_w_core(r, eax, C3B_W_RINGM); }
+static void m_29db8_radio(const u32 *r, u32 *eax)      { c3b_w_core(r, eax, C3B_W_RADIO); }
+static void m_29db8_w44(const u32 *r, u32 *eax)        { c3b_w_core(r, eax, C3B_W_W44); }
+static void m_29db8_swap(const u32 *r, u32 *eax)       { c3b_w_core(r, eax, C3B_W_SWAP); }
+static void m_29db8_bep(const u32 *r, u32 *eax)        { c3b_w_core(r, eax, C3B_W_BEP); }
+static void m_29db8_child(const u32 *r, u32 *eax)      { c3b_w_core(r, eax, C3B_W_CHILD); }
+static void m_29db8_high(const u32 *r, u32 *eax)       { c3b_w_core(r, eax, C3B_W_HIGH); }
+
 static const binding_t k_bindings[] = {
     { "rng_next",                 b_rng_next,     0xFFFFFFFFu },
     { "fighter_slot_flag",        b_slot_flag,    0x000000FFu },
@@ -10192,6 +11185,138 @@ static const binding_t k_bindings[] = {
     { "actor_alloc@tail",                  m_2ac80_tail,   0xFFFFFFFFu },
     { "actor_alloc@empty",                 m_2ac80_empty,  0xFFFFFFFFu },
     { "actor_alloc@ret",                   m_2ac80_ret,    0xFFFFFFFFu },
+    { "anim_read_var",                     b_c3b_anim_read_var, 0x0000FFFFu },
+    { "anim_read_var@ring",                m_c3b_a_ring,   0x0000FFFFu },
+    { "anim_read_var@sx",                  m_c3b_a_sx,     0x0000FFFFu },
+    { "anim_read_var@pswap",               m_c3b_a_pswap,  0x0000FFFFu },
+    { "anim_read_var@cswap",               m_c3b_a_cswap,  0x0000FFFFu },
+    { "anim_read_var@w58",                 m_c3b_a_w58,    0x0000FFFFu },
+    { "anim_read_var@a45",                 m_c3b_a_a45,    0x0000FFFFu },
+    { "anim_read_var@ret",                 m_c3b_a_ret,    0x0000FFFFu },
+    { "anim_read_var@opff",                m_c3b_a_opff,   0x0000FFFFu },
+    { "fighter_18350",                     b_c3b_18350,    0x00000000u },
+    { "fighter_18350@tab",                 m_c3b_b_tab,    0x00000000u },
+    { "fighter_18350@anchor",              m_c3b_b_anchor, 0x00000000u },
+    { "fighter_18350@neg",                 m_c3b_b_neg,    0x00000000u },
+    { "fighter_18350@bit",                 m_c3b_b_bit,    0x00000000u },
+    { "fighter_18350@side",                m_c3b_b_side,   0x00000000u },
+    { "fighter_18540",                     b_c3b_18540,    0x00000000u },
+    { "fighter_18540@default",             m_c3b_c_default, 0x00000000u },
+    { "fighter_18540@noclamp",             m_c3b_c_noclamp, 0x00000000u },
+    { "fighter_18540@gt",                  m_c3b_c_gt,     0x00000000u },
+    { "fighter_18540@mask",                m_c3b_c_mask,   0x00000000u },
+    { "fighter_18540@idx",                 m_c3b_c_idx,    0x00000000u },
+    { "fighter_18540@side",                m_c3b_c_side,   0x00000000u },
+    { "hit_record_y",                      b_c3b_hit_record_y, 0xFFFFFFFFu },
+    { "hit_record_y@bit",                  m_c3b_y_bit,    0xFFFFFFFFu },
+    { "hit_record_y@rec",                  m_c3b_y_rec,    0xFFFFFFFFu },
+    { "hit_record_y@call",                 m_c3b_y_call,   0xFFFFFFFFu },
+    { "hit_record_y@always",               m_c3b_y_always, 0xFFFFFFFFu },
+    { "hit_record_y@add",                  m_c3b_y_add,    0xFFFFFFFFu },
+    { "fighter_38154",                     b_c3b_fighter_38154, 0x00000000u },
+    { "fighter_38154@mode8",               m_c3b_d_mode8,  0x00000000u },
+    { "fighter_38154@range",               m_c3b_d_range,  0x00000000u },
+    { "fighter_38154@abs",                 m_c3b_d_abs,    0x00000000u },
+    { "fighter_38154@bit",                 m_c3b_d_bit,    0x00000000u },
+    { "fighter_38154@bd",                  m_c3b_d_bd,     0x00000000u },
+    { "fighter_38154@flag",                m_c3b_d_flag,   0x00000000u },
+    { "fighter_38154@call",                m_c3b_d_call,   0x00000000u },
+    { "fighter_38154@dir",                 m_c3b_d_dir,    0x00000000u },
+    { "snd_music_stop",                    b_c3b_snd_music_stop, 0x000000FFu },
+    { "snd_music_stop@gate",               m_c3b_m_gate,   0x000000FFu },
+    { "snd_music_stop@play",               m_c3b_m_play,   0x000000FFu },
+    { "snd_music_stop@stop",               m_c3b_m_stop,   0x000000FFu },
+    { "snd_music_stop@zero",               m_c3b_m_zero,   0x000000FFu },
+    { "snd_music_stop@al",                 m_c3b_m_al,     0x000000FFu },
+    { "snd_music_stop@cc",                 m_c3b_m_cc,     0x000000FFu },
+    { "snd_sample_playing",                b_c3b_snd_sample_playing, 0x000000FFu },
+    { "snd_sample_playing@driver",         m_c3b_p_driver, 0x000000FFu },
+    { "snd_sample_playing@status",         m_c3b_p_status, 0x000000FFu },
+    { "snd_sample_playing@clear",          m_c3b_p_clear,  0x000000FFu },
+    { "snd_sample_playing@scan",           m_c3b_p_scan,   0x000000FFu },
+    { "snd_sample_playing@wide",           m_c3b_p_wide,   0x000000FFu },
+    { "snd_sample_playing@al",             m_c3b_p_al,     0x000000FFu },
+    { "snd_samples_stop_all",              b_c3b_snd_samples_stop_all, 0x000000FFu },
+    { "snd_samples_stop_all@driver",       m_c3b_a_driver, 0x000000FFu },
+    { "snd_samples_stop_all@skip2",        m_c3b_a_skip2,  0x000000FFu },
+    { "snd_samples_stop_all@clr4",         m_c3b_a_clr4,   0x000000FFu },
+    { "snd_samples_stop_all@clrc",         m_c3b_a_clrc,   0x000000FFu },
+    { "snd_samples_stop_all@al",           m_c3b_a_al,     0x000000FFu },
+    { "snd_samples_stop_all@order",        m_c3b_a_order,  0x000000FFu },
+    { "snd_sample_stop",                   b_c3b_snd_sample_stop, 0x000000FFu },
+    { "snd_sample_stop@driver",            m_c3b_s_driver, 0x000000FFu },
+    { "snd_sample_stop@eq2",               m_c3b_s_eq2,    0x000000FFu },
+    { "snd_sample_stop@clear",             m_c3b_s_clear,  0x000000FFu },
+    { "snd_sample_stop@nolimit",           m_c3b_s_nolimit, 0x000000FFu },
+    { "snd_sample_stop@al",                m_c3b_s_al,     0x000000FFu },
+    { "snd_sample_stop@calls",             m_c3b_s_calls,  0x000000FFu },
+    { "snd_sample_queue",                  b_c3b_snd_sample_queue, 0x000000FFu },
+    { "snd_sample_queue@driver",           m_c3b_q_driver, 0x000000FFu },
+    { "snd_sample_queue@pause",            m_c3b_q_pause,  0x000000FFu },
+    { "snd_sample_queue@size",             m_c3b_q_size,   0x000000FFu },
+    { "snd_sample_queue@armbuf",           m_c3b_q_armbuf, 0x000000FFu },
+    { "snd_sample_queue@armq",             m_c3b_q_armq,   0x000000FFu },
+    { "snd_sample_queue@armst",            m_c3b_q_armst,  0x000000FFu },
+    { "snd_sample_queue@order",            m_c3b_q_order,  0x000000FFu },
+    { "snd_sample_queue@candcmp",          m_c3b_q_candcmp, 0x000000FFu },
+    { "snd_sample_queue@candmin",          m_c3b_q_candmin, 0x000000FFu },
+    { "snd_sample_queue@tailret",          m_c3b_q_tailret, 0x000000FFu },
+    { "snd_sample_queue@tailstop",         m_c3b_q_tailstop, 0x000000FFu },
+    { "snd_sample_queue@tailorder",        m_c3b_q_tailorder, 0x000000FFu },
+    { "snd_sample_queue@loopbyte",         m_c3b_q_loopbyte, 0x000000FFu },
+    { "list_insert_before",                b_c3b_list_insert_before, 0x00000000u },
+    { "list_insert_before@prev",           m_c3b_lb_prev,   0x00000000u },
+    { "list_insert_before@link",           m_c3b_lb_link,   0x00000000u },
+    { "list_insert_before@back",           m_c3b_lb_back,   0x00000000u },
+    { "list_insert_before@head",           m_c3b_lb_head,   0x00000000u },
+    { "list_insert_before@forward",        m_c3b_lb_forward, 0x00000000u },
+    { "effect_teardown",                   b_13420,        0x00000000u },
+    { "effect_teardown@unlink",            m_13420_unlink, 0x00000000u },
+    { "effect_teardown@restore",           m_13420_restore, 0x00000000u },
+    { "effect_teardown@type1",             m_13420_type1,  0x00000000u },
+    { "effect_teardown@flag",              m_13420_flag,   0x00000000u },
+    { "effect_teardown@buf",               m_13420_buf,    0x00000000u },
+    { "effect_teardown@tail",              m_13420_tail,   0x00000000u },
+    { "actor_type_49444",                  b_49444,        0x00000000u },
+    { "actor_type_49444@bit",              m_49444_bit,    0x00000000u },
+    { "actor_type_49444@idx",              m_49444_idx,    0x00000000u },
+    { "actor_type_49444@sign",             m_49444_sign,   0x00000000u },
+    { "actor_type_49444@child",            m_49444_child,  0x00000000u },
+    { "actor_type_49444@link",             m_49444_link,   0x00000000u },
+    { "actor_type_49444@reclr",            m_49444_reclr,  0x00000000u },
+    { "set_dead",                          b_2b150,        0x00000000u },
+    { "set_dead@bit",                      m_2b150_bit,    0x00000000u },
+    { "set_dead@gate",                     m_2b150_gate,   0x00000000u },
+    { "set_dead@clear",                    m_2b150_clear,  0x00000000u },
+    { "set_dead@table",                    m_2b150_table,  0x00000000u },
+    { "set_dead@pset",                     m_2b150_pset,   0x00000000u },
+    { "set_dead@pal",                      m_2b150_pal,    0x00000000u },
+    { "set_dead@render",                   m_2b150_render, 0x00000000u },
+    { "anim_write_var",                    b_29db8,        0x00000000u },
+    { "anim_write_var@mask",               m_29db8_mask,   0x00000000u },
+    { "anim_write_var@ringm",              m_29db8_ringm,  0x00000000u },
+    { "anim_write_var@radio",              m_29db8_radio,  0x00000000u },
+    { "anim_write_var@w44",                m_29db8_w44,    0x00000000u },
+    { "anim_write_var@swap",               m_29db8_swap,   0x00000000u },
+    { "anim_write_var@bep",                m_29db8_bep,    0x00000000u },
+    { "anim_write_var@child",              m_29db8_child,  0x00000000u },
+    { "anim_write_var@high",               m_29db8_high,   0x00000000u },
+    { "anim_operand",                      b_2b8f8,        0x0000FFFFu },
+    { "anim_operand@op",                   m_2b8f8_op,     0x0000FFFFu },
+    { "anim_operand@mask",                 m_2b8f8_mask,   0x0000FFFFu },
+    { "anim_operand@adv",                  m_2b8f8_adv,    0x0000FFFFu },
+    { "anim_operand@be8",                  m_2b8f8_be8,    0x0000FFFFu },
+    { "anim_operand@sel",                  m_2b8f8_sel,    0x0000FFFFu },
+    { "anim_operand@scale",                m_2b8f8_scale,  0x0000FFFFu },
+    { "anim_operand@deref",                m_2b8f8_deref,  0x0000FFFFu },
+    { "anim_operand@byte",                 m_2b8f8_byte,   0x0000FFFFu },
+    { "fighter_anim_triple",               b_3afc4,        0x00000000u },
+    { "fighter_anim_triple@idx",           m_3afc4_idx,    0x00000000u },
+    { "fighter_anim_triple@shift",         m_3afc4_shift,  0x00000000u },
+    { "fighter_anim_triple@add",           m_3afc4_add,    0x00000000u },
+    { "fighter_anim_triple@o0",            m_3afc4_o0,     0x00000000u },
+    { "fighter_anim_triple@o1",            m_3afc4_o1,     0x00000000u },
+    { "fighter_anim_triple@o2",            m_3afc4_o2,     0x00000000u },
 };
 
 static const binding_t *find_binding(const char *name)
diff --git a/tools/diff_verify.py b/tools/diff_verify.py
index 8417b94..8127e08 100644
--- a/tools/diff_verify.py
+++ b/tools/diff_verify.py
@@ -24,6 +24,10 @@ def le32(v):
     return (v & 0xFFFFFFFF).to_bytes(4, "little")
 
 
+def le16(v):
+    return (v & 0xFFFF).to_bytes(2, "little")
+
+
 @dataclass
 class Case:
     id: str
@@ -4552,6 +4556,667 @@ C3_SPECS = [
        eax_mask=0xFFFFFFFF, mutants=("@flag", "@unlink", "@tail", "@empty", "@ret")),
 ]
 
+# ---- track P batch C3b (record 2026-10-05-reverse-c3b): the frontier rows, part 2 ----------------
+#
+# 0x29F34 anim_read_var (record §C3b.1): EAX = rec, EDX = op; the low word selects (`xor dh,dh` /
+# `and dl,0x7f` at 0x29F38/0x29F3C). o = op & 0x7F: < 0x40 the ring word at 0x105B4C indexed by
+# (o + rec+0x51) & 0x3F; 0x40..0x45 the record's own fields (0x40..0x43 and 0x45 sign-extended
+# bytes, 0x44 the +0x56 word); 0x46..0x4B / 0x4C..0x51 the same fields on the record at
+# [0x1014F4] + rec[0x4A]/rec[0x4B] * 0x68; above 0x51 zero. EAX is zero-extended on every path
+# (0x29F3A/0x29F63/0x29FB2 `xor eax,eax`) and the port returns u32, so the mask is full.
+C3B_A_REC = 0x10A800          # C3b zero BSS: 0x29F34's record
+C3B_A_ACT = 0x10A900          # its parent/child actor pair (index 0 / 1 at +0x68)
+C3B_A_RING = 0x105B4C         # DS_00105B4C: the 0x40-word ring
+
+def c3b_a_pokes(idx=0):
+    return {C3B_A_REC + 0x51: bytes([idx]),
+            C3B_A_REC + 0x52: b"\x81", C3B_A_REC + 0x53: b"\x7f",
+            C3B_A_REC + 0x54: b"\x80", C3B_A_REC + 0x55: b"\xff",
+            C3B_A_REC + 0x56: b"\xef\xbe", C3B_A_REC + 0x58: b"\x80",
+            C3B_A_REC + 0x4a: b"\x00", C3B_A_REC + 0x4b: b"\x01",
+            0x001014F4: le32(C3B_A_ACT),
+            C3B_A_RING: b"\x11\x11", C3B_A_RING + 2: b"\x22\x22",
+            C3B_A_RING + 0x80: b"\x77\x77"}
+
+# 0x18350 fighter_18350 (record §C3b.1): EAX = side, EDX = anchor. ch = 0x1077B0[side*0x94]+0x7A
+# selects the per-character table (0x18334; >6 0xCEB00), p = table + anchor*2 (the u32 wrap the
+# port keeps), x/y = sx(p[0])/sx(p[1]), the x negated when 0x1A570 reports the actor's bit 15 set,
+# both <<6 into 0x100AB0/AB4[side*8]. Callers ignore EAX (mask 0).
+C3B_B_REC = 0x10AB00          # the side's record for 0x1A570
+C3B_B_ACT = 0x10AB40          # its actor table ([0x1014EC])
+C3B_B_TABLES = {0: 0xCEB00, 1: 0xCF399, 2: 0xCFC32, 3: 0xD033B, 4: 0xD0A44, 5: 0xCEB00,
+                6: 0xCF399}
+
+def c3b_b_case(side, ch, anchor, tbl, tblbytes, idx=0, actor=0x0000, actor0=0x0000):
+    slot = 0x1077B0 + side * 0x94
+    out = 0x100AB0 + side * 8
+    p = {slot + 0x7A: bytes([ch]), slot: le32(C3B_B_REC),
+         C3B_B_REC + 0x56: (idx & 0xFFFF).to_bytes(2, "little"),
+         0x001014EC: le32(C3B_B_ACT), C3B_B_ACT: (actor0 & 0xFFFF).to_bytes(2, "little"),
+         C3B_B_ACT + idx * 0x20: (actor & 0xFFFF).to_bytes(2, "little"),
+         out: le32(0xA5A5A5A5), out + 4: le32(0x5A5A5A5A)}
+    p[tbl + anchor * 2] = tblbytes
+    return p
+
+# 0x18540 fighter_18540 (record §C3b.1): EAX = side. slot = 0x1077A8[side]; null returns. ch =
+# slot+0x7A selects the per-character camera word (0x18524; ch 0 and >6 the 0xE6DD0 default), then
+# 0x18460 runs (allow: effect-free, its 0x18428 dispatch is a bare RET per character). sprite =
+# (word at [0x1014EC] + word[DSD(slot)+0x56] * 0x20) & 0x7FFF; anchor = sprite - cam into
+# 0x100AF0[side], zeroed when negative or at/over 0xE6DB4[ch]. Callers ignore EAX (mask 0).
+C3B_C_SLOT = 0x10AC00
+C3B_C_REC = 0x10AC40
+C3B_C_ACT = 0x10AC80
+
+def c3b_c_case(side, ch, cam, limit, sprite, idx=0, live=True, extra=None):
+    slot = 0x1077B0 + side * 0x94
+    p = {0x100AF0 + side * 4: le32(0xA5A5A5A5),
+         0xE6DB4 + ch * 4: le32(limit)}
+    if live:
+        p.update({0x1077A8 + side * 4: le32(slot),
+                  slot + 0x7A: bytes([ch]),
+                  slot: le32(C3B_C_REC),
+                  C3B_C_REC + 0x56: (idx & 0xFFFF).to_bytes(2, "little"),
+                  0x001014EC: le32(C3B_C_ACT),
+                  C3B_C_ACT: (0x0000 if idx else sprite & 0xFFFF).to_bytes(2, "little"),
+                  C3B_C_ACT + idx * 0x20: (sprite & 0xFFFF).to_bytes(2, "little")})
+    else:
+        p[0x1077A8 + side * 4] = le32(0)
+    cams = {1: 0xE39D0, 2: 0xECBD8, 3: 0xD2134, 4: 0xEA604, 5: 0xD3E08, 6: 0xE061C}
+    p[cams.get(ch, 0xE6DD0)] = (cam & 0xFFFF).to_bytes(2, "little")
+    if extra:
+        p.update(extra)
+    return p
+
+# 0x18788 hit_record_y (record §C3b.1): EAX = side; slot+0x42 bit 3 takes the record's +0x1C;
+# otherwise 0x18540 re-latches, 0x18350 re-runs when the anchor differs from slot+0x20, and the
+# result is slot+0x30 - 0x100AB4[side]. Full mask (the only caller 0x1883C reads EAX).
+C3B_Y_REC = 0x10AD00
+
+def c3b_y_pokes(side, bit42, anchor=None, slot30=0, ab4=0, cam=0x0100, sprite=0x0140, extra=None):
+    slot = 0x1077B0 + side * 0x94
+    p = {slot: le32(C3B_Y_REC), slot + 0x42: bytes([bit42]),
+         C3B_Y_REC + 0x1C: le32(0x11223344), C3B_Y_REC + 0x18: le32(0x55667788),
+         0x100AB4 + side * 8: le32(ab4)}
+    if anchor is not None:
+        p.update({0x1077A8 + side * 4: le32(slot), slot + 0x7A: b"\x00",
+                  C3B_Y_REC + 0x56: b"\x00\x00",
+                  0x001014EC: le32(C3B_C_ACT), C3B_C_ACT: (sprite & 0xFFFF).to_bytes(2, "little"),
+                  slot + 0x20: le32(anchor), slot + 0x30: le32(slot30),
+                  0xE6DD0: (cam & 0xFFFF).to_bytes(2, "little")})
+    if extra:
+        p.update(extra)
+    return p
+
+C3B_D_REC = 0x10AE00
+
+# The voice rows' state (record C3b §C3b.1): the port's DS_ symbols, not in symbols.h's Python side.
+DS_001028C0, DS_001028C8 = 0x001028C0, 0x001028C8
+DS_001028CC, DS_001028D4, DS_001028D9 = 0x001028CC, 0x001028D4, 0x001028D9
+DS_001028DB = 0x001028DB
+
+
+def c3b_v_case(driver, cur, queued=None, extra=None):
+    p = {DS_001028C8: le32(driver)}
+    for k in range(4):
+        p[0x00102860 + k * 0x18] = le32(0x10C000 + k * 0x18)
+        p[0x0010286C + k * 0x18] = le32(cur[k])
+        if queued is not None:
+            p[0x00102864 + k * 0x18] = le32(queued)
+    if extra:
+        p.update(extra)
+    return p
+
+
+def c3b_d_case(side, bd, s884, p, x, bit14, slot53=1, slot54=0, mode4b=0, live=True, recnull=False):
+    slot = 0x1077B0 + side * 0x94
+    p2 = {0x1077A8 + side * 4: le32(slot), slot: le32(0 if recnull else C3B_D_REC),
+          slot + 0x7A: b"\x00",
+          C3B_D_REC + 0x3C: le32(p), C3B_D_REC + 0x18: le32(x),
+          C3B_D_REC + 0x28: (0x4000 if bit14 else 0).to_bytes(2, "little"),
+          0x1088BD: bytes([bd]), 0x108884: le32(s884), 0x104B00: le16(mode4b),
+          slot + 0x53: bytes([slot53]), slot + 0x54: bytes([slot54]),
+          slot + 0x40: le32(0xFFFFFFFF), 0x1078F0 + side: b"\xA5",
+          0x10AE4C: b"\xAA\xBB", slot + 0x55: b"\xCC", slot + 0x5F: b"\xDD"}
+    if not live:
+        p2[0x1077A8 + side * 4] = le32(0)
+    return p2
+
+def c3b_q_slots():
+    out = bytearray()
+    for k in range(4):
+        out += le32(0x10C000 + k * 0x18) + le32(0x11111111) + b"\x22" + le32(0x99999999) \
+             + le32(0x33333333) + le32(0x44444444)
+    return bytes(out)
+
+C3B_Q_T = 0x00102874
+C3B_Q_BUF = 0x00102870
+C3B_Q_QUEUED = 0x00102864
+
+# 0x13420 effect_teardown (record §C3b.1): EAX = rec. The interrupt lock DS_0009AF3C is set to 1,
+# 0x249D0 unlinks rec from the active list, the lock is restored, then the record's type
+# (rec+0xC, an unsigned byte: `mov al`/`cmp al,5`/`ja`) dispatches through the table 0x13408:
+# type 1 -> palette_record(rec+0x10, DSD(src+8)+DSB(rec+0xF), 1, 0); type 4 -> palette_record(
+# 0xFCCF0, DSD(src+8), DSD(src+0xC), 0); types 0/2/3/5 -> palette_record_flagged(DSD(src),
+# DSD(src+8), DSD(src+0xC)); anything else (6..0xFF) -> nothing. Then 0x249B0 inserts rec after
+# the free-list sentinel DS_000FCCE8. The two palette writers and the two list calls run as
+# allows (the effects.c copies carry no seam: C3's actor_alloc mutant route depends on their
+# calls staying unrecorded; the final bytes are the comparison). Mask 0.
+C3B_E_REC = 0x10A800
+C3B_E_SRC = 0x10A900
+C3B_E_SENTINEL = 0x000FCCE8
+
+
+def c3b_e_pokes(typ, extra=None):
+    # rec, the sentinel and the two neighbour nodes A/B are interlinked so the unlink and the
+    # insert-after both move bytes (a self-linked sentinel would make them idempotent and the
+    # @unlink/@tail mutants unobservable).
+    p = {C3B_E_REC: le32(0x10AC00) + le32(0x10AC40) + le32(C3B_E_SRC),
+         C3B_E_REC + 0xC: bytes([typ, 0x00, 0x00, 0x44]) + b"\x77\x77\x77\x77",
+         C3B_E_SRC: le32(0x11111111) + b"\x00" * 4 + le32(0x22222222) + le32(0x33333333),
+         C3B_E_SENTINEL: le32(0x10AC00) + le32(0x10AC40),
+         0x10AC00: le32(0x10AC40) + le32(C3B_E_SENTINEL),
+         0x10AC40: le32(C3B_E_SENTINEL) + le32(0x10AC00),
+         0x0009AF3C: b"\x5A", 0x00107798: le32(0x00107498)}
+    if extra:
+        p.update(extra)
+    return p
+
+# 0x49444 actor_type_49444 (record §C3b.1): EAX = rec. node = DSD(rec+0x14); null returns.
+# node+0x1C bit 1 (loaded as a word but only AL bit 1 read: `mov ax,[..]; xor ah,ah; and al,2`)
+# clears the entry DSD(0x10839C + (signed DSD(node+0x18)>>16)*4); a non-zero node+0x10 is
+# retired with 0x2B150 (stubbed: its own row proves it) and zeroed; then 0x249D0 unlinks the
+# node and 0x249B0 re-inserts it after the 0x1083C4 sentinel, and rec+0x14 is zeroed. Mask 0.
+C3B_F_REC = 0x10A800
+C3B_F_NODE = 0x10A900
+C3B_F_SENTINEL = 0x001083C4
+
+
+def c3b_f_pokes(q=0x00030000, flags=2, child=0, rec14=None, extra=None):
+    p = {C3B_F_REC + 0x14: le32(C3B_F_NODE if rec14 is None else rec14),
+         C3B_F_NODE: le32(C3B_F_SENTINEL), C3B_F_NODE + 4: le32(C3B_F_SENTINEL),
+         C3B_F_NODE + 0x10: le32(child), C3B_F_NODE + 0x18: le32(q),
+         C3B_F_NODE + 0x1C: le16(flags),
+         C3B_F_SENTINEL: le32(C3B_F_SENTINEL), C3B_F_SENTINEL + 4: le32(C3B_F_SENTINEL),
+         0x0010839C: le32(0xDEADBEEF), 0x001083A0: le32(0xDEADBEEF),
+         0x001083A4: le32(0xDEADBEEF), 0x001083A8: le32(0xDEADBEEF),
+         0x00108398: le32(0xDEADBEEF)}
+    if extra:
+        p.update(extra)
+    return p
+
+# 0x2B150 set_dead (record §C3b.1): EAX = rec. rec+0x28 |= 8 (a byte store); when rec+0x2b
+# bit 6 is set, the type callback DSD(0xBB9E0 + DSB(rec+0x48)*0xC) is called with EAX = rec
+# (an indirect call: the 0x5D812 entries are the `xor eax,eax; ret` stub, the others are the
+# registered port callbacks) and rec+0x2b bit 6 is cleared; then pset = DSD(0x1014EC) +
+# DSW(rec+0x56)*0x20, and when pset+0x18 is non-zero 0x33864 releases it and it is zeroed;
+# finally 0x1C458 finds the render-list node whose +4 is the pset and 0x1C3D0 unlinks it to
+# the free head (the port runs both as one render_list_remove). The palette release is
+# stubbed; the render pair and the callback targets are allowed. Mask 0.
+C3B_S_REC = 0x10A068           # in-pool at DS_001014F4 = 0x10A000, stride 0x68
+C3B_S_POOL = 0x10A000
+C3B_S_PSET = 0x10A800          # DS_001014EC: the pset table, stride 0x20
+C3B_S_NODE = 0x10A900          # the type callback's rec+0x14 node
+C3B_S_LAND = 0x000F0A78        # 0x12800's destination sentinel
+C3B_S_RNODE = 0x0010153C       # the first render-pool node
+C3B_S_RFREE = 0x0010275C       # the render free-list head
+C3B_S_RHEAD = 0x00105B44       # the render list head
+
+
+# 0x29DB8 anim_write_var (record §C3b.1): EAX = rec, EDX = op, EBX = value. The pool record the
+# parent/child forms write is DS_001014F4 + DSB(rec+0x4A|0x4B) * 0x68.
+C3B_W_REC = 0x10A800
+C3B_W_POOL = 0x10A000
+C3B_W_RING = 0x00105B4C
+C3B_W_BE8 = 0x00105BE8
+
+
+def c3b_w_pokes(i51=0, i4a=0, i4b=1, be8=0x5A, ring=None, extra=None):
+    p = {0x001014F4: le32(C3B_W_POOL),
+         C3B_W_REC + 0x4A: bytes([i4a, i4b]),
+         C3B_W_REC + 0x51: bytes([i51, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27]),
+         C3B_W_BE8: le16(be8),
+         C3B_W_POOL + 0x52: b"\xA5" * 8,
+         C3B_W_POOL + 0x68 + 0x52: b"\xB5" * 8}
+    for i, b in (ring or {}).items():
+        p[C3B_W_RING + i * 2] = b
+    if extra:
+        p.update(extra)
+    return p
+
+
+# 0x2B8F8 anim_operand (record §C3b.1): EAX = rec; the stream pointer DSD(rec+8) walks the
+# image words at C3B_O_P, the byte/deref forms index from DSD(rec+0x0C) = C3B_O_BASE.
+C3B_O_REC = 0x10A800
+C3B_O_P = 0x10B000
+C3B_O_BASE = 0x10A900
+
+
+def c3b_o_pokes(op=0, cw=0, w2=0, w4=0, extra=None):
+    p = {C3B_O_REC + 8: le32(C3B_O_P), C3B_O_REC + 0x0C: le32(C3B_O_BASE),
+         C3B_O_P: le16(cw), C3B_O_P + 2: le16(w2), C3B_O_P + 4: le16(w4),
+         0x00105BE4: le16(op), 0x00105BE6: le16(0), 0x00105BE8: le16(0xA5A5),
+         0x00105BD4: le32(0xA5A5A5A5),
+         C3B_O_BASE: bytes([0x00, 0xFF, 0xEE, 0x42, 0x7F, 0x5A, 0x00, 0x5A]),
+         C3B_O_BASE + 8: le16(0x5A5A), C3B_O_BASE + 0x0A: le16(0x6B6B)}
+    if extra:
+        p.update(extra)
+    return p
+
+
+# 0x3AFC4 fighter_anim_triple (record §C3b.1): EAX = slot_char, EDX = the frame byte, EBX = the
+# 3-dword output. The per-character byte at DS_0010782A + slot_char*0x94 is shifted left 6 and
+# added to EDX, giving c; the output is 0xDE114 + c*11, 0xA3528 + c*20 and 0xA6728 + c*6. EDX
+# outside [0,0x40) is the raw's 0x62003 fatal (the port zeroes the triple: the named gap).
+C3B_T_OUT = 0x10A800
+C3B_T_TBL = 0x0010782A
+
+
+def c3b_t_pokes(sc=0, edx=0, tbl=None, extra=None):
+    p = {C3B_T_OUT: b"\xA5" * 12}
+    if tbl is not None:
+        p[C3B_T_TBL + sc * 0x94] = bytes([tbl])
+    if extra:
+        p.update(extra)
+    return p
+
+
+def c3b_s_pokes(typ=0, cb=False, pset18=0, idx=0, node=0, rnode=False, extra=None):
+    p = {0x001014F4: le32(C3B_S_POOL), 0x001014EC: le32(C3B_S_PSET),
+         C3B_S_REC + 0x14: le32(node),
+         C3B_S_REC + 0x28: b"\xA5", C3B_S_REC + 0x2A: b"\x00",
+         C3B_S_REC + 0x2B: (b"\x40" if cb else b"\x00"),
+         C3B_S_REC + 0x48: bytes([typ]), C3B_S_REC + 0x56: le16(idx),
+         C3B_S_PSET + idx * 0x20 + 0x18: le32(pset18),
+         C3B_S_RHEAD: le32(0), C3B_S_RFREE: le32(0),
+         C3B_S_LAND: le32(C3B_S_LAND), C3B_S_LAND + 4: le32(C3B_S_LAND)}
+    if rnode:
+        p[C3B_S_RHEAD] = le32(C3B_S_RNODE)
+        p[C3B_S_RNODE] = le32(0)
+        p[C3B_S_RNODE + 4] = le32(C3B_S_PSET + idx * 0x20)
+    if node:
+        p[C3B_S_NODE] = le32(C3B_S_LAND)
+        p[C3B_S_NODE + 4] = le32(C3B_S_LAND)
+    if extra:
+        p.update(extra)
+    return p
+
+def c3b_q_case(driver=1, db=0, size=0x6001, overrides=None):
+    slots = c3b_q_slots()
+    p = {0x00102860: slots[:0x30], 0x00102890: slots[0x30:],
+         DS_001028C8: le32(driver), DS_001028DB: bytes([db]), 0x00101500: le32(0x1000)}
+    p.update(c2b_res(0x800001, size))
+    p.update(overrides or {})
+    return p
+
+
+C3B_SPECS = [
+    Spec("anim_read_var", 0x29F34, [
+        Case("a0", {"eax": C3B_A_REC, "edx": 0x00}, c3b_a_pokes(0)),          # ring[0]
+        Case("a1", {"eax": C3B_A_REC, "edx": 0x3F}, c3b_a_pokes(1)),          # (0x3F+1)&0x3F = 0
+        Case("a2", {"eax": C3B_A_REC, "edx": 0x40}, c3b_a_pokes()),           # sx(+0x52) = -0x7F
+        Case("a3", {"eax": C3B_A_REC, "edx": 0x41}, c3b_a_pokes()),           # sx(+0x53) = +0x7F
+        Case("a4", {"eax": C3B_A_REC, "edx": 0x42}, c3b_a_pokes()),           # sx(+0x54)
+        Case("a5", {"eax": C3B_A_REC, "edx": 0x43}, c3b_a_pokes()),           # sx(+0x55)
+        Case("a6", {"eax": C3B_A_REC, "edx": 0x44}, c3b_a_pokes()),           # word +0x56
+        Case("a7", {"eax": C3B_A_REC, "edx": 0x45}, c3b_a_pokes()),           # sx(+0x58)
+        Case("a8", {"eax": C3B_A_REC, "edx": 0x46},
+             {**c3b_a_pokes(), C3B_A_ACT + 0x52: b"\x33"}),                   # parent +0x52
+        Case("a9", {"eax": C3B_A_REC, "edx": 0x4B},
+             {**c3b_a_pokes(), C3B_A_ACT + 0x56: b"\x42\x41"}),               # parent word
+        Case("a10", {"eax": C3B_A_REC, "edx": 0x4C},
+             {**c3b_a_pokes(), C3B_A_ACT + 0x68 + 0x52: b"\x55"}),            # child +0x52
+        Case("a11", {"eax": C3B_A_REC, "edx": 0x51},
+             {**c3b_a_pokes(), C3B_A_ACT + 0x68 + 0x56: b"\x62\x61"}),        # child word
+        Case("a12", {"eax": C3B_A_REC, "edx": 0x52}, c3b_a_pokes()),          # above 0x51: 0
+        Case("a13", {"eax": C3B_A_REC, "edx": 0xFF40}, c3b_a_pokes()),        # dx = 0x40
+        Case("a14", {"eax": C3B_A_REC, "edx": 0x80}, c3b_a_pokes(0)),         # dl&0x7F = 0
+        Case("a15", {"eax": C3B_A_REC, "edx": 0x3F}, c3b_a_pokes(0x80)),      # ring index 0x3F
+        Case("a16", {"eax": C3B_A_REC, "edx": 0x49},
+             {**c3b_a_pokes(), C3B_A_ACT + 0x55: b"\x66"}),                   # parent +0x55
+        Case("a17", {"eax": C3B_A_REC, "edx": 0x4A},
+             {**c3b_a_pokes(), C3B_A_ACT + 0x56: b"\x77\x76"}),               # parent word
+        Case("a18", {"eax": C3B_A_REC, "edx": 0x48},
+             {**c3b_a_pokes(), C3B_A_ACT + 0x54: b"\x88"}),                   # parent +0x54
+        Case("a19", {"eax": C3B_A_REC, "edx": 0x47},
+             {**c3b_a_pokes(), C3B_A_ACT + 0x53: b"\x99"}),                   # parent +0x53
+    ], eax_mask=0xFFFF, mutants=("@ring", "@sx", "@pswap", "@cswap", "@w58", "@a45", "@ret", "@opff")),
+    Spec("fighter_18350", 0x18350, [
+        Case("b0", {"eax": 0, "edx": 0}, c3b_b_case(0, 0, 0, 0xCEB00, b"\x01\x02")),
+        Case("b1", {"eax": 1, "edx": 0}, c3b_b_case(1, 1, 0, 0xCF399, b"\x03\x04", 1, 0x8000)),
+        Case("b2", {"eax": 0, "edx": 0}, c3b_b_case(0, 2, 0, 0xCFC32, b"\x05\x06")),
+        Case("b3", {"eax": 0, "edx": 1}, c3b_b_case(0, 3, 1, 0xD033B, b"\x7f\x80")),
+        Case("b4", {"eax": 0, "edx": 0}, c3b_b_case(0, 4, 0, 0xD0A44, b"\x09\x0a")),
+        Case("b5", {"eax": 1, "edx": 0}, c3b_b_case(1, 5, 0, 0xCEB00, b"\x01\x02")),
+        Case("b6", {"eax": 0, "edx": 0}, c3b_b_case(0, 6, 0, 0xCF399, b"\x03\x04")),
+        Case("b7", {"eax": 0, "edx": 0}, c3b_b_case(0, 7, 0, 0xCEB00, b"\x01\x02")),
+        Case("b8", {"eax": 1, "edx": 0}, c3b_b_case(1, 0x20, 0, 0xCEB00, b"\x01\x02")),
+        Case("b9", {"eax": 0, "edx": 0}, c3b_b_case(0, 0, 0, 0xCEB00, b"\x01\x02", 0, 0x8000)),
+    ], calls=(E.Call(0x1A570, ("eax",), mode="real"),), eax_mask=0,
+       mutants=("@tab", "@anchor", "@neg", "@bit", "@side")),
+    Spec("fighter_18540", 0x18540, [
+        Case("c0", {"eax": 0}, c3b_c_case(0, 0, 0x0100, 0x0FFF, 0x1234)),
+        Case("c1", {"eax": 1}, c3b_c_case(1, 1, 0x0200, 0xFFFF, 0x1000)),
+        Case("c2", {"eax": 0}, c3b_c_case(0, 2, 0x0300, 0x0FFF, 0x1000)),
+        Case("c3", {"eax": 0}, c3b_c_case(0, 3, 0x0400, 0x0FFF, 0x1000)),
+        Case("c4", {"eax": 0}, c3b_c_case(0, 4, 0x0500, 0x0FFF, 0x1000)),
+        Case("c5", {"eax": 0}, c3b_c_case(0, 5, 0x0600, 0x0FFF, 0x1000)),
+        Case("c6", {"eax": 0}, c3b_c_case(0, 6, 0x0700, 0x0FFF, 0x1000)),
+        Case("c7", {"eax": 0}, c3b_c_case(0, 7, 0x0800, 0x0FFF, 0x1000)),
+        Case("c8", {"eax": 0}, c3b_c_case(0, 0, 0x0200, 0x0FFF, 0x0100)),    # negative: 0
+        Case("c9", {"eax": 0}, c3b_c_case(0, 0, 0x0100, 0x2000, 0x2100)),    # == limit: 0
+        Case("c10", {"eax": 0}, c3b_c_case(0, 0, 0x0100, 0x2000, 0x20FF)),   # limit-1 kept
+        Case("c11", {"eax": 0}, c3b_c_case(0, 0, 0, 0, 0, live=False)),      # slot null: no write
+        Case("c12", {"eax": 0}, c3b_c_case(0, 0, 0x0000, 0x0FFF, 0x8010, 1)),  # mask 0x7FFF, *0x20
+    ], allow_calls=(0x18460, 0x18428), eax_mask=0,
+       mutants=("@default", "@noclamp", "@gt", "@mask", "@idx", "@side")),
+    Spec("hit_record_y", 0x18788, [
+        Case("y0", {"eax": 0}, c3b_y_pokes(0, 0x08)),
+        Case("y1", {"eax": 0}, c3b_y_pokes(0, 0x00, anchor=0x40, slot30=0x00020010,
+                                           ab4=0x00010000)),
+        Case("y2", {"eax": 0}, c3b_y_pokes(0, 0x00, anchor=0x40, slot30=0x00020000,
+                                           ab4=0x00010000,
+                                           extra={0xCEB80: b"\x01\x02", 0x1077B0 + 0x20: le32(0x41)})),
+        Case("y3", {"eax": 1}, c3b_y_pokes(1, 0x00, anchor=0x40, slot30=0x00030020,
+                                           ab4=0x00020000)),
+    ], allow_calls=(0x18460, 0x18428),
+       calls=(E.Call(0x18540, ("eax",), mode="real"),
+              E.Call(0x18350, ("eax", "edx"), mode="real"),
+              E.Call(0x1A570, ("eax",), mode="real")),
+       eax_mask=0xFFFFFFFF, mutants=("@bit", "@rec", "@call", "@always", "@add")),
+    # 0x38154 fighter_38154 (record §C3b.1): EAX = side. slot = DSD(0x1077A8[side]), rec = DSD(slot);
+    # either null returns. v: DS_001088BD == 8 && side == 1 gives 0x1500 - rec+0x3C when rec+0x3C is
+    # in [0, 0x5D00], otherwise DS_00108884 - 0x1500 - rec+0x18; every other side gives
+    # (0x4900 - side*0x200) - rec+0x3C in range, otherwise (0x1F00 - side*0x200) + DS_00108884 -
+    # rec+0x18. With rec+0x28 bit 14: |v| > 0x200 and v >= 0 runs 0x35838(slot, rec, 0x1000), flag 0;
+    # else slot+0x43 |= 0x40 and, when the zero-extended DS_001088BD >= 8, flag 1, else
+    # 0x36638(slot, rec) and flag 0. Without the bit: |v| <= 0x200 gives flag 1, else
+    # 0x35838(slot, rec, v < 0 ? 0x2000 : 0x1000) and flag 0. The flag is DS_001078F0[side]; with it
+    # set and slot+0x53 != 0, 0x367DC(slot, rec) (allow) runs and slot+0x52 = 9; with it clear,
+    # slot+0x53 = 0xC. The 0x36638 and 0x35838 paths are declared real; their own anim calls are the
+    # ANIM_BEGIN stub; slot+0x54 = 5 is never used (its 0x36638 arm recurses into 0x38154). Mask 0.
+    Spec("fighter_38154", 0x38154, [
+        Case("d0", {"eax": 0}, c3b_d_case(0, 8, 0, 0x1000, 0, 0)),        # V3 far v>=0: 0x35838 0x1000
+        Case("d1", {"eax": 0}, c3b_d_case(0, 8, 0, 0x1000, 0, 1, 0, 4)),  # bit14 far v>=0: 0x35838
+        Case("d2", {"eax": 0}, c3b_d_case(0, 8, 0, 0x5000, 0, 1, 0, 4)),  # bit14 far v<0: 0x35838
+        Case("d3", {"eax": 0}, c3b_d_case(0, 8, 0, 0x4800, 0, 1)),        # bit14 near, bd>=8: flag 1
+        Case("d4", {"eax": 0}, c3b_d_case(0, 0, 0, 0x4800, 0, 1, 0, 1)),  # bit14 near, bd<8: 0x36638
+        Case("d5", {"eax": 1}, c3b_d_case(1, 8, 0, 0x4700, 0, 1)),        # side1 V3, near: flag 1
+        Case("d6", {"eax": 0}, c3b_d_case(0, 0, 0, 0x4800, 0, 0, 0)),     # near, slot53 0: no 0x367DC
+        Case("d7", {"eax": 0}, c3b_d_case(0, 8, 0x2000, 0x8000, 0x0B00, 0)),  # V2 near: flag 1
+        Case("d8", {"eax": 0}, c3b_d_case(0, 0, 0x1000, 0x8000, 0x2000, 1, 0, 4)),  # V4 far: 0x35838
+        Case("d9", {"eax": 0}, c3b_d_case(0, 0, 0, 0, 0, 0, live=False)),  # slot null: return
+        Case("d10", {"eax": 0}, c3b_d_case(0, 0, 0, 0, 0, 0, recnull=True)),  # rec null: return
+        Case("d11", {"eax": 1}, c3b_d_case(1, 8, 0, 0x1000, 0, 0)),       # side1 V1: v = 0x500
+        Case("d12", {"eax": 1}, c3b_d_case(1, 8, 0x2000, 0x8000, 0x0B00, 0)),  # side1 V2 near
+        Case("d13", {"eax": 1}, c3b_d_case(1, 8, 0, 0x1400, 0, 0)),       # side1 V1 near: flag 1
+        Case("d14", {"eax": 0}, c3b_d_case(0, 0, 0, 0x5000, 0, 0)),       # v<0, bit14 clear: abs
+        Case("d15", {"eax": 0}, c3b_d_case(0, 9, 0, 0x4800, 0, 1)),       # bd=9 still >= 8: flag 1
+        Case("d16", {"eax": 0}, c3b_d_case(0, 0x88, 0, 0x4800, 0, 1, 0, 1)),  # bd 0x88 signed < 8
+    ], allow_calls=(0x367DC,),
+       calls=(ANIM_BEGIN,
+              E.Call(0x36638, ("eax", "edx"), mode="real"),
+              E.Call(0x35838, ("eax", "edx", "ebx"), mode="real")),
+       eax_mask=0, mutants=("@mode8", "@range", "@abs", "@bit", "@bd", "@flag", "@call", "@dir")),
+    # The voice remainder (record §C3b.1): the four DIG functions whose only callees are the AIL
+    # runtime calls. The runtime addresses are stubbed, with the port's host wrappers carrying the
+    # seams (ail.c AIL_sample_status/AIL_stop_sample/AIL_init_sample/AIL_stop_sequence, flow.c
+    # snd_music_playing): every wrapper passes no arguments, so the E.Call entries record none (the
+    # original's mem[] handle and the port's host pointer cannot be compared). EAX is AL everywhere
+    # (the raw's `mov al,1`/`xor al,al`); the callee clobbers are E.callee_clobbers over the image.
+    Spec("snd_music_stop", 0x1CA6C, [
+        Case("m0", {}, {DS_001028C0: le32(0), DS_001028D4: le32(0xA5A5A5A5),
+                        DS_001028D9: b"\xAA", DS_001028CC: le32(0xBBBBBBBB)}),      # no sequence: 0
+        Case("m1", {}, {DS_001028C0: le32(0x10B000), DS_001028D4: le32(0xA5A5A5A5),
+                        DS_001028D9: b"\xAA", DS_001028CC: le32(0xBBBBBBBB)}, {0x1CA40: 0}),
+        Case("m2", {}, {DS_001028C0: le32(0x10B000), DS_001028D4: le32(0xA5A5A5A5),
+                        DS_001028D9: b"\xAA", DS_001028CC: le32(0xBBBBBBBB)}, {0x1CA40: 1}),
+    ], calls=(E.Call(0x1CA40, (), eax=0), E.Call(0x5DEAF, (), clobbers=("ebx", "ecx", "edx"))),
+       eax_mask=0xFF,
+       mutants=("@gate", "@play", "@stop", "@zero", "@al", "@cc")),
+    # 0x1CE70 (record §C3b.1): EAX = the resource handle. The +0x0C dword of each 0x18-stride DIG
+    # slot is compared; on a match 0x5DD03 (+0x00's AIL handle) status 4 returns 1, any other clears
+    # the slot's +0x0C and the scan goes on. AL = 0 without a DIG driver.
+    Spec("snd_sample_playing", 0x1CE70, [
+        Case("q0", {"eax": 0x1234}, c3b_v_case(0, (0x1234, 0x99999999, 0x99999999, 0x99999999)),
+             {0x5DD03: 4}),
+        Case("q1", {"eax": 0x1234}, c3b_v_case(1, (0x99999999, 0x99999999, 0x99999999, 0x99999999)),
+             {0x5DD03: 4}),
+        Case("q2", {"eax": 0x1234}, c3b_v_case(1, (0x1234, 0x99999999, 0x99999999, 0x99999999)),
+             {0x5DD03: 4}),
+        Case("q3", {"eax": 0x1234}, c3b_v_case(1, (0x99999999, 0x1234, 0x99999999, 0x99999999)),
+             {0x5DD03: 0}),
+        Case("q4", {"eax": 0x1234}, c3b_v_case(1, (0x99999999, 0x99999999, 0x99999999, 0x1234)),
+             {0x5DD03: 4}),
+        Case("q5", {"eax": 0x1234}, c3b_v_case(1, (0x1234, 0x99999999, 0x99999999, 0x1234)),
+             {0x5DD03: 0}),
+        Case("q6", {"eax": 0x1234}, c3b_v_case(1, (0xAB001234, 0x99999999, 0x99999999, 0x99999999)),
+             {0x5DD03: 4}),
+    ], calls=(E.Call(0x5DD03, (), eax=0),), eax_mask=0xFF,
+       mutants=("@driver", "@status", "@clear", "@scan", "@wide", "@al")),
+    # 0x1CD9C (record §C3b.1): without a DIG driver AL = 0. Otherwise every slot's +0x04 and +0x0C
+    # are cleared and a slot whose 0x5DD03 status is not 2 is stopped/re-inited; AL = 1.
+    Spec("snd_samples_stop_all", 0x1CD9C, [
+        Case("r0", {}, c3b_v_case(0, (0x99999999, 0x99999999, 0x99999999, 0x99999999),
+                                  queued=0x11111111), {0x5DD03: 2}),
+        Case("r1", {}, c3b_v_case(1, (0x99999999, 0x99999999, 0x99999999, 0x99999999),
+                                  queued=0x11111111), {0x5DD03: 2}),
+        Case("r2", {}, c3b_v_case(1, (0x99999999, 0x99999999, 0x99999999, 0x99999999),
+                                  queued=0x11111111), {0x5DD03: 0}),
+    ], calls=(E.Call(0x5DD03, (), eax=0), E.Call(0x5DC8B, (), clobbers=("ebx", "ecx", "edx")),
+              E.Call(0x5DC0F, (), clobbers=("ebx", "ecx", "edx"))),
+       eax_mask=0xFF, mutants=("@driver", "@skip2", "@clr4", "@clrc", "@al", "@order")),
+    # 0x1CE04 (record §C3b.1): EAX = the handle. The first 0x18-stride slot whose +0x0C is the
+    # handle and whose 0x5DD03 status is not 2 is stopped/re-inited, its +0x0C cleared, AL = 1.
+    Spec("snd_sample_stop", 0x1CE04, [
+        Case("t0", {"eax": 0x1234}, c3b_v_case(0, (0x1234, 0x99999999, 0x99999999, 0x99999999)),
+             {0x5DD03: 4}),
+        Case("t1", {"eax": 0x1234}, c3b_v_case(1, (0x99999999, 0x99999999, 0x99999999, 0x99999999)),
+             {0x5DD03: 4}),
+        Case("t2", {"eax": 0x1234}, c3b_v_case(1, (0x1234, 0x99999999, 0x99999999, 0x99999999)),
+             {0x5DD03: 2}),
+        Case("t3", {"eax": 0x1234}, c3b_v_case(1, (0x1234, 0x99999999, 0x99999999, 0x99999999)),
+             {0x5DD03: 0}),
+        Case("t4", {"eax": 0x1234}, c3b_v_case(1, (0x99999999, 0x99999999, 0x1234, 0x99999999)),
+             {0x5DD03: 4}),
+        Case("t5", {"eax": 0x1234}, c3b_v_case(1, (0x1234, 0x1234, 0x99999999, 0x99999999)),
+             {0x5DD03: 0}),
+        Case("t6", {"eax": 0x1234}, c3b_v_case(1, (0xAB001234, 0x99999999, 0x99999999, 0x99999999)),
+             {0x5DD03: 4}),
+    ], calls=(E.Call(0x5DD03, (), eax=0), E.Call(0x5DC8B, (), clobbers=("ebx", "ecx", "edx")),
+              E.Call(0x5DC0F, (), clobbers=("ebx", "ecx", "edx"))),
+       eax_mask=0xFF, mutants=("@driver", "@eq2", "@clear", "@nolimit", "@al", "@calls")),
+    # 0x1CC28 snd_sample_queue (record §C3b.1): EAX = handle h, DL = the loop byte. Without a DIG
+    # driver, while the sample pause byte DS_001028DB is set, or when the handle does not resolve
+    # (0x1B544 allow; the 0x500BB allow reads the same DS_00101500), the queue does nothing. A
+    # payload above 0x6000 bytes tries only slot 0: buffer +0x10 set, +0x04 free and 0x5DD03 status
+    # not 4 queue there; otherwise the tail. A payload at or below 0x6000 scans slots 3..0 for a
+    # free arm (+0x10 set, +0x04 clear, status != 4) and otherwise takes the tail's candidate, the
+    # slot with the smallest +0x14 time strictly below the 0x500BB now (slot 0 on a tie). The tail
+    # stops and re-inits the handle's AIL sample (0x5DC8B/0x5DC0F), stores h/+0x08/now and AL = 1.
+    # The slot fields are seeded by one 0x60-byte poke (queued 0x11111111, loop 0x22, current
+    # 0x99999999, buffer 0x33333333, time 0x44444444), overridden per case.
+    # 0x249C0 list_insert_before (record §C3b.1): EAX = `at`, EDX = `rec`. prev = [at+4];
+    # [at+4] = rec; [rec] = at; [rec+4] = prev; [prev] = rec. Straight-line, no callees; the row
+    # binds the effects.c copy (the actors.c copy is the same body, as in C3's list rows). Mask 0.
+    Spec("list_insert_before", 0x249C0, [
+        Case("v0", {"eax": 0x10A200, "edx": 0x10A240},
+             {0x10A204: le32(0x10A260), 0x10A260: le32(0xA5A5A5A5),
+              0x10A240: le32(0x11111111), 0x10A244: le32(0x22222222)}),
+        Case("v1", {"eax": 0x10A200, "edx": 0x10A240},
+             {0x10A204: le32(0x10A200), 0x10A240: le32(0x33333333), 0x10A244: le32(0x44444444)}),
+        Case("v2", {"eax": 0x10A200, "edx": 0x10A240},
+             {0x10A204: le32(0x10A240), 0x10A240: le32(0x55555555), 0x10A244: le32(0x66666666)}),
+    ], eax_mask=0, mutants=("@prev", "@link", "@back", "@head", "@forward")),
+    Spec("snd_sample_queue", 0x1CC28, [
+        Case("z0", {"eax": 0x800001, "edx": 0x37}, c3b_q_case(driver=0), {0x5DD03: 0}),
+        Case("z1", {"eax": 0x800001, "edx": 0x37}, c3b_q_case(db=1), {0x5DD03: 0}),
+        # size > 0x6000: slot 0's arm queues (buf set, +0x04 clear, status 0).
+        Case("z2", {"eax": 0x800001, "edx": 0x37}, c3b_q_case(overrides={C3B_Q_QUEUED: le32(0)}),
+             {0x5DD03: 0}),
+        # size > 0x6000: slot 0 has no buffer, the tail queues slot 0.
+        Case("z3", {"eax": 0x800001, "edx": 0x37},
+             c3b_q_case(overrides={C3B_Q_QUEUED: le32(0), C3B_Q_BUF: le32(0)}), {0x5DD03: 0}),
+        # size > 0x6000: slot 0 is occupied (+0x04 set), the tail queues it.
+        Case("z4", {"eax": 0x800001, "edx": 0x37}, c3b_q_case(), {0x5DD03: 0}),
+        # size > 0x6000: slot 0 plays (status 4), the tail queues it.
+        Case("z5", {"eax": 0x800001, "edx": 0x37}, c3b_q_case(overrides={C3B_Q_QUEUED: le32(0)}),
+             {0x5DD03: 4}),
+        # size <= 0x6000: every arm fails differently; the smallest time is slot 1's.
+        Case("z6", {"eax": 0x800001, "edx": 0x37},
+             c3b_q_case(size=0x100, overrides={
+                 C3B_Q_T: le32(0x400), C3B_Q_T + 0x18: le32(0x100), C3B_Q_T + 0x30: le32(0x200),
+                 C3B_Q_T + 0x48: le32(0x300), C3B_Q_QUEUED: le32(0), C3B_Q_BUF + 0x18: le32(0)}),
+             {0x5DD03: 4}),
+        # size <= 0x6000: slot 2's arm queues before any candidate scan.
+        Case("z7", {"eax": 0x800001, "edx": 0x37},
+             c3b_q_case(size=0x100, overrides={C3B_Q_BUF + 0x48: le32(0), C3B_Q_QUEUED + 0x30: le32(0)}),
+             {0x5DD03: 0}),
+        # size <= 0x6000: slots 1 and 0 tie on the smallest time; the strict compare keeps 2.
+        Case("z8", {"eax": 0x800001, "edx": 0x37},
+             c3b_q_case(size=0x100, overrides={
+                 C3B_Q_T: le32(0x100), C3B_Q_T + 0x18: le32(0x200), C3B_Q_T + 0x30: le32(0x100),
+                 C3B_Q_T + 0x48: le32(0x200), C3B_Q_QUEUED: le32(0)}), {0x5DD03: 4}),
+        # size <= 0x6000: slots 3 and 0 are both free; the 3..0 scan queues slot 3.
+        Case("z9", {"eax": 0x800001, "edx": 0x37},
+             c3b_q_case(size=0x100, overrides={C3B_Q_QUEUED + 0x48: le32(0), C3B_Q_QUEUED: le32(0)}),
+             {0x5DD03: 0}),
+    ], allow_calls=(0x500BB, 0x1B544),
+       calls=(E.Call(0x5DD03, (), eax=0), E.Call(0x5DC8B, (), clobbers=("ebx", "ecx", "edx")),
+              E.Call(0x5DC0F, (), clobbers=("ebx", "ecx", "edx"))),
+       eax_mask=0xFF, mutants=("@driver", "@pause", "@size", "@armbuf", "@armq", "@armst",
+                               "@order", "@candcmp", "@candmin", "@tailret", "@tailstop",
+                               "@tailorder", "@loopbyte")),
+    Spec("effect_teardown", 0x13420, [
+        Case("e0", {"eax": C3B_E_REC}, c3b_e_pokes(0)),
+        Case("e1", {"eax": C3B_E_REC}, c3b_e_pokes(1)),
+        Case("e2", {"eax": C3B_E_REC}, c3b_e_pokes(2)),
+        Case("e3", {"eax": C3B_E_REC}, c3b_e_pokes(3)),
+        Case("e4", {"eax": C3B_E_REC}, c3b_e_pokes(4)),
+        Case("e5", {"eax": C3B_E_REC}, c3b_e_pokes(5)),
+        Case("e6", {"eax": C3B_E_REC}, c3b_e_pokes(6)),
+        Case("e7", {"eax": C3B_E_REC}, c3b_e_pokes(0x80)),
+    ], allow_calls=(0x249D0, 0x249B0, 0x33734, 0x33714),
+       eax_mask=0, mutants=("@unlink", "@restore", "@type1", "@flag", "@buf", "@tail")),
+    Spec("actor_type_49444", 0x49444, [
+        Case("a0", {"eax": C3B_F_REC}, c3b_f_pokes(rec14=0)),                 # rec+0x14 null
+        Case("a1", {"eax": C3B_F_REC}, c3b_f_pokes()),                        # bit1, idx 3
+        Case("a2", {"eax": C3B_F_REC}, c3b_f_pokes(flags=0)),                 # bit1 clear
+        Case("a3", {"eax": C3B_F_REC}, c3b_f_pokes(child=0x10AB00)),          # child retired
+        Case("a4", {"eax": C3B_F_REC}, c3b_f_pokes(q=0xFFFF0000)),            # idx -1
+        Case("a5", {"eax": C3B_F_REC}, c3b_f_pokes(flags=0x22)),              # bit5 too
+        Case("a6", {"eax": C3B_F_REC}, c3b_f_pokes(flags=1)),                 # bit0 only
+        Case("a7", {"eax": C3B_F_REC}, c3b_f_pokes(q=0x00020000, child=0x10AB00)),  # idx 2, child
+    ], calls=(E.Call(0x2B150, ("eax",)),
+              E.Call(0x249D0, ("eax",), mode="real"),
+              E.Call(0x249B0, ("eax", "edx"), mode="real")),
+       eax_mask=0, mutants=("@bit", "@idx", "@sign", "@child", "@link", "@reclr")),
+    # 0x2B150 (record §C3b.1). The callback targets 0x5D812 (a stub) and 0x12800 (the port's
+    # actor_type_12800; the callback's own 0x249D0/0x249B0 calls are in the call set) are
+    # allowed; the render pair 0x1C458+0x1C3D0 is allowed as the raw's two calls (the port's
+    # render_list_remove is the same body).
+    Spec("set_dead", 0x2B150, [
+        Case("s0", {"eax": C3B_S_REC}, c3b_s_pokes()),
+        Case("s1", {"eax": C3B_S_REC}, c3b_s_pokes(pset18=0x10AB00)),
+        Case("s2", {"eax": C3B_S_REC}, c3b_s_pokes(rnode=True)),
+        Case("s3", {"eax": C3B_S_REC}, c3b_s_pokes(typ=0, cb=True)),
+        Case("s4", {"eax": C3B_S_REC}, c3b_s_pokes(typ=1, cb=True, node=C3B_S_NODE)),
+        Case("s5", {"eax": C3B_S_REC}, c3b_s_pokes(typ=1, cb=True)),
+        Case("s6", {"eax": C3B_S_REC}, c3b_s_pokes(pset18=0x10AB00, idx=1)),
+        Case("s7", {"eax": C3B_S_REC},
+             c3b_s_pokes(typ=1, cb=True, node=C3B_S_NODE, pset18=0x10AB00)),
+    ], allow_calls=(0x1C458, 0x1C3D0, 0x5D812, 0x12800),
+       calls=(E.Call(0x33864, ("eax",)),
+              E.Call(0x249D0, ("eax",), mode="real"),
+              E.Call(0x249B0, ("eax", "edx"), mode="real")),
+       eax_mask=0, mutants=("@bit", "@gate", "@clear", "@table", "@pset", "@pal", "@render")),
+    # 0x29DB8 anim_write_var (record §C3b.1): EAX = rec, EDX = op, EBX = value (0x29DBC
+    # `mov eax,ebx`). o = (EDX & 0xFF) & 0x7F: below 0x40 the ring word at 0x105B4C indexed by
+    # (o + rec+0x51) & 0x3F takes (u16)value; 0x40..0x45 the record's own +0x52..+0x55/+0x58
+    # bytes and the +0x56 word (low byte only), all (u8)value or (u16)value&0xFF; 0x46..0x4B and
+    # 0x4C..0x51 the same fields on the pool record named by rec+0x4A / rec+0x4B (the byte forms
+    # store DS_00105BE8, not value); above 0x51 nothing. Mask 0.
+    Spec("anim_write_var", 0x29DB8, [
+        Case("w0", {"eax": C3B_W_REC, "edx": 0, "ebx": 0x1234},
+             c3b_w_pokes(i51=0x3F, ring={0: b"\xBB\xBB", 0x3E: b"\xAA\xAA"})),
+        Case("w1", {"eax": C3B_W_REC, "edx": 1, "ebx": 0x5678},
+             c3b_w_pokes(i51=0x3F, ring={0: b"\xBB\xBB", 0x3F: b"\xAA\xAA"})),
+        Case("w2", {"eax": C3B_W_REC, "edx": 2, "ebx": 0x9ABC},
+             c3b_w_pokes(i51=0x3E, ring={0: b"\xBB\xBB", 0x3F: b"\xAA\xAA"})),
+        Case("w3", {"eax": C3B_W_REC, "edx": 0x40, "ebx": 0x11223344}, c3b_w_pokes()),
+        Case("w4", {"eax": C3B_W_REC, "edx": 0x41, "ebx": 0x5566}, c3b_w_pokes()),
+        Case("w5", {"eax": C3B_W_REC, "edx": 0x42, "ebx": 0x7788}, c3b_w_pokes()),
+        Case("w6", {"eax": C3B_W_REC, "edx": 0x43, "ebx": 0x99AA}, c3b_w_pokes()),
+        Case("w7", {"eax": C3B_W_REC, "edx": 0x44, "ebx": 0x1234}, c3b_w_pokes()),
+        Case("w8", {"eax": C3B_W_REC, "edx": 0x45, "ebx": 0x5566}, c3b_w_pokes()),
+        Case("w9", {"eax": C3B_W_REC, "edx": 0x46, "ebx": 0x99}, c3b_w_pokes(i4a=0, be8=0x5A)),
+        Case("w10", {"eax": C3B_W_REC, "edx": 0x4A, "ebx": 0x1234}, c3b_w_pokes(i4a=0)),
+        Case("w11", {"eax": C3B_W_REC, "edx": 0x4B, "ebx": 0x7788}, c3b_w_pokes(i4a=0)),
+        Case("w12", {"eax": C3B_W_REC, "edx": 0x4C, "ebx": 0x99}, c3b_w_pokes(i4a=0, i4b=1, be8=0x6B)),
+        Case("w13", {"eax": C3B_W_REC, "edx": 0x50, "ebx": 0x1234}, c3b_w_pokes(i4a=0, i4b=1)),
+        Case("w14", {"eax": C3B_W_REC, "edx": 0x51, "ebx": 0x99}, c3b_w_pokes(i4a=0, i4b=1)),
+        Case("w15", {"eax": C3B_W_REC, "edx": 0x52, "ebx": 0x99}, c3b_w_pokes()),
+        Case("w16", {"eax": C3B_W_REC, "edx": 0xFF, "ebx": 0x99}, c3b_w_pokes()),
+        Case("w17", {"eax": C3B_W_REC, "edx": 0xBF, "ebx": 0x4321},
+             c3b_w_pokes(i51=0, ring={0x3F: b"\xAA\xAA"})),
+        Case("w18", {"eax": C3B_W_REC, "edx": 0xC0, "ebx": 0xABCD}, c3b_w_pokes()),
+        Case("w19", {"eax": C3B_W_REC, "edx": 0x47, "ebx": 0x99}, c3b_w_pokes(i4a=0, be8=0x61)),
+        Case("w20", {"eax": C3B_W_REC, "edx": 0x48, "ebx": 0x99}, c3b_w_pokes(i4a=0, be8=0x62)),
+        Case("w21", {"eax": C3B_W_REC, "edx": 0x49, "ebx": 0x99}, c3b_w_pokes(i4a=0, be8=0x63)),
+    ], eax_mask=0,
+       mutants=("@mask", "@ringm", "@radio", "@w44", "@swap", "@bep", "@child", "@high")),
+    # 0x2B8F8 anim_operand (record §C3b.1): EAX = rec. cw = DSW(DSD(rec+8)); mode = cw & 0x6000
+    # is stored at DS_00105BE6. When the previous op DS_00105BE4 is 0x1F, p advances by 2 and the
+    # next word is the operand (cx); when not, cx = cw & 0xFF and the mode-0 arm returns cx
+    # zero/sign-extended and stores it at DS_00105BE8. Otherwise 0x29F34 reads the variable
+    # (stubbed here: its own row proves it) and the mode selects: 0x2000 the read word; 0x4000
+    # the read word with DS_00105BD4 = DSD(rec+8 + 2) and return; otherwise DS_00105BD4 =
+    # DSD(rec+0x0C) and the next word's 0xF000: 0x1000 v*2, 0x2000 v*2+1, 0x4000/0x5000/0x6000
+    # a word at base + v*2 / v*4 / v*4+2 (DS_00105BD4 = that address), anything else the byte at
+    # base + v (sign-extended to 0xFFxx when above 0x7F). EAX is masked to 16 bits on every return.
+    Spec("anim_operand", 0x2B8F8, [
+        Case("o0", {"eax": C3B_O_REC}, c3b_o_pokes(op=0x1F, cw=0x0001, w2=0x1234)),
+        Case("o1", {"eax": C3B_O_REC}, c3b_o_pokes(op=0x1F, cw=0x2002, w2=0x002A),
+             stub_eax={0x29F34: 0x7777}),
+        Case("o2", {"eax": C3B_O_REC}, c3b_o_pokes(cw=0x0005)),
+        Case("o3", {"eax": C3B_O_REC}, c3b_o_pokes(cw=0x0085)),
+        Case("o4", {"eax": C3B_O_REC}, c3b_o_pokes(cw=0x2007), stub_eax={0x29F34: 0x1234}),
+        Case("o5", {"eax": C3B_O_REC}, c3b_o_pokes(cw=0x4008, w2=0xB100, w4=0x0010),
+             stub_eax={0x29F34: 0x9ABC}),
+        Case("o6", {"eax": C3B_O_REC}, c3b_o_pokes(cw=0x6009, w4=0x1000),
+             stub_eax={0x29F34: 3}),
+        Case("o7", {"eax": C3B_O_REC}, c3b_o_pokes(cw=0x600A, w4=0x2000),
+             stub_eax={0x29F34: 3}),
+        Case("o8", {"eax": C3B_O_REC}, c3b_o_pokes(cw=0x600B, w4=0x4000),
+             stub_eax={0x29F34: 2}),
+        Case("o9", {"eax": C3B_O_REC}, c3b_o_pokes(cw=0x600C, w4=0x5000),
+             stub_eax={0x29F34: 2}),
+        Case("o10", {"eax": C3B_O_REC}, c3b_o_pokes(cw=0x600D, w4=0x6000),
+             stub_eax={0x29F34: 2}),
+        Case("o11", {"eax": C3B_O_REC}, c3b_o_pokes(cw=0x600E, w4=0),
+             stub_eax={0x29F34: 2}),
+        Case("o12", {"eax": C3B_O_REC}, c3b_o_pokes(cw=0x600F, w4=0),
+             stub_eax={0x29F34: 3}),
+        Case("o13", {"eax": C3B_O_REC}, c3b_o_pokes(cw=0x6010, w4=0x3000),
+             stub_eax={0x29F34: 4}),
+        Case("o14", {"eax": C3B_O_REC}, c3b_o_pokes(op=0x1F, cw=0x0003, w2=0xFFFF)),
+        Case("o15", {"eax": C3B_O_REC}, c3b_o_pokes(cw=0x6011, w4=0x7000),
+             stub_eax={0x29F34: 5}),
+    ], calls=(E.Call(0x29F34, ("eax", "edx"), clobbers=("edx",)),),
+       eax_mask=0xFFFF,
+       mutants=("@op", "@mask", "@adv", "@be8", "@sel", "@scale", "@deref", "@byte")),
+    # 0x3AFC4 fighter_anim_triple (record §C3b.1). The 0x3AFCE block is the raw's 0x62003 fatal
+    # path (EDX outside [0,0x40)); the port zeroes the triple there, and no case can stop the
+    # original on a fatal, so the block is named unhit. Mask 0 (every port caller ignores EAX).
+    Spec("fighter_anim_triple", 0x3AFC4, [
+        Case("t0", {"eax": 0, "edx": 0, "ebx": C3B_T_OUT}, c3b_t_pokes(sc=0, tbl=0x03)),
+        Case("t1", {"eax": 0, "edx": 0x3F, "ebx": C3B_T_OUT}, c3b_t_pokes(sc=0, tbl=0x03)),
+        Case("t2", {"eax": 1, "edx": 0x20, "ebx": C3B_T_OUT}, c3b_t_pokes(sc=1, tbl=0x01)),
+        Case("t3", {"eax": 1, "edx": 0, "ebx": C3B_T_OUT}, c3b_t_pokes(sc=1, tbl=0x01)),
+        Case("t4", {"eax": 2, "edx": 0x11, "ebx": C3B_T_OUT}, c3b_t_pokes(sc=2, tbl=0xFF)),
+    ], unhit_named={0x3AFCE: "the raw's 0x62003 fatal for edx outside [0,0x40)"},
+       eax_mask=0, mutants=("@idx", "@shift", "@add", "@o0", "@o1", "@o2")),
+]
+
 SPECS = [
     Spec("rng_next", 0x5D7DC, [
         Case("r1", {"eax": 0x1234}, {DS_RNG: le32(0x12345678)}),
@@ -4594,7 +5259,7 @@ SPECS = [
         Case("d0", {}, {DS_1078FC: b"\x00"}),
         Case("d1", {"eax": U6_REC}, {DS_1078FC: b"\x5a"}),
     ], eax_mask=0),
-] + E3_SPECS + P1_SPECS + P1_ANIM_SPECS + P2_SPECS + P3_SPECS + P6_SPECS + P45_SPECS + C1_SPECS + C2_SPECS + C2B_SPECS + C3_SPECS + P7_SPECS + P8_SPECS
+] + E3_SPECS + P1_SPECS + P1_ANIM_SPECS + P2_SPECS + P3_SPECS + P6_SPECS + P45_SPECS + C1_SPECS + C2_SPECS + C2B_SPECS + C3_SPECS + C3B_SPECS + P7_SPECS + P8_SPECS
 
 
 # ---- driver -------------------------------------------------------------------------------------
diff --git a/tools/tests/test_diff_verify.py b/tools/tests/test_diff_verify.py
index 9730c40..d839c3f 100644
--- a/tools/tests/test_diff_verify.py
+++ b/tools/tests/test_diff_verify.py
@@ -1032,6 +1032,145 @@ C3_KINDS = {
     "snd_sample_unpause@one": {"byte"},
 }
 
+# Track P batch C3b (record 2026-10-05-reverse-c3b): the frontier rows, part 2, with their EAX
+# masks, and what alone catches each of their mutants.
+C3B_MASKS = {
+            "anim_read_var": 0xffff,
+            "fighter_18350": 0x0,
+            "fighter_18540": 0x0,
+            "hit_record_y": 0xffffffff,
+            "fighter_38154": 0x0,
+            "snd_music_stop": 0xff,
+            "snd_sample_playing": 0xff,
+            "snd_samples_stop_all": 0xff,
+            "snd_sample_stop": 0xff,
+            "list_insert_before": 0x0,
+            "snd_sample_queue": 0xff,
+            "effect_teardown": 0x0,
+            "actor_type_49444": 0x0,
+            "set_dead": 0x0,
+            "anim_write_var": 0x0,
+            "anim_operand": 0xffff,
+            "fighter_anim_triple": 0x0,
+}
+C3B_KINDS = {
+    "actor_type_49444@bit": {"byte", "call #0", "call #0 memory", "call #1", "call #2"},
+    "actor_type_49444@child": {"byte", "call #0", "call #1", "call #2"},
+    "actor_type_49444@idx": {"byte", "call #0", "call #0 memory", "call #1", "call #2"},
+    "actor_type_49444@link": {"call #0", "call #1", "call #2"},
+    "actor_type_49444@reclr": {"byte", "call #0", "call #1", "call #2"},
+    "actor_type_49444@sign": {"byte", "call #0", "call #1", "call #2"},
+    "anim_operand@adv": {"byte", "call #0 memory"},
+    "anim_operand@be8": {"byte"},
+    "anim_operand@byte": {"eax"},
+    "anim_operand@deref": {"byte"},
+    "anim_operand@mask": {"eax"},
+    "anim_operand@op": {"byte", "call #0", "call #0 memory", "eax"},
+    "anim_operand@scale": {"eax"},
+    "anim_operand@sel": {"byte", "eax"},
+    "anim_read_var@a45": {"eax"},
+    "anim_read_var@cswap": {"eax"},
+    "anim_read_var@opff": {"eax"},
+    "anim_read_var@pswap": {"eax"},
+    "anim_read_var@ret": {"eax"},
+    "anim_read_var@ring": {"eax"},
+    "anim_read_var@sx": {"eax"},
+    "anim_read_var@w58": {"eax"},
+    "anim_write_var@bep": {"byte"},
+    "anim_write_var@child": {"byte"},
+    "anim_write_var@high": {"byte"},
+    "anim_write_var@mask": {"byte"},
+    "anim_write_var@radio": {"byte"},
+    "anim_write_var@ringm": {"byte"},
+    "anim_write_var@swap": {"byte"},
+    "anim_write_var@w44": {"byte"},
+    "effect_teardown@buf": {"byte"},
+    "effect_teardown@flag": {"byte"},
+    "effect_teardown@restore": {"byte"},
+    "effect_teardown@tail": {"byte"},
+    "effect_teardown@type1": {"byte"},
+    "effect_teardown@unlink": {"byte"},
+    "fighter_18350@anchor": {"byte", "call #0 memory"},
+    "fighter_18350@bit": {"byte"},
+    "fighter_18350@neg": {"byte", "call #0"},
+    "fighter_18350@side": {"byte", "call #0 memory"},
+    "fighter_18350@tab": {"byte", "call #0 memory"},
+    "fighter_18540@default": {"byte"},
+    "fighter_18540@gt": {"byte"},
+    "fighter_18540@idx": {"byte"},
+    "fighter_18540@mask": {"byte"},
+    "fighter_18540@noclamp": {"byte"},
+    "fighter_18540@side": {"byte"},
+    "fighter_38154@abs": {"byte", "call #0", "call #0 memory", "call #1", "call #1 memory"},
+    "fighter_38154@bd": {"byte", "call #0", "call #0 memory", "call #1", "call #1 memory"},
+    "fighter_38154@bit": {"byte", "call #0", "call #0 memory", "call #1", "call #1 memory"},
+    "fighter_38154@call": {"byte", "call #0", "call #1"},
+    "fighter_38154@dir": {"byte", "call #0", "call #1"},
+    "fighter_38154@flag": {"byte", "call #0", "call #0 memory", "call #1", "call #1 memory"},
+    "fighter_38154@mode8": {"byte", "call #0", "call #0 memory", "call #1", "call #1 memory"},
+    "fighter_38154@range": {"byte", "call #0", "call #0 memory", "call #1", "call #1 memory"},
+    "fighter_anim_triple@add": {"byte"},
+    "fighter_anim_triple@idx": {"byte"},
+    "fighter_anim_triple@o0": {"byte"},
+    "fighter_anim_triple@o1": {"byte"},
+    "fighter_anim_triple@o2": {"byte"},
+    "fighter_anim_triple@shift": {"byte"},
+    "hit_record_y@add": {"eax"},
+    "hit_record_y@always": {"byte", "call #1", "call #2", "eax"},
+    "hit_record_y@bit": {"call #0", "eax"},
+    "hit_record_y@call": {"byte", "call #0", "call #1", "call #1 memory", "call #2", "eax"},
+    "hit_record_y@rec": {"eax"},
+    "list_insert_before@back": {"byte"},
+    "list_insert_before@forward": {"byte"},
+    "list_insert_before@head": {"byte"},
+    "list_insert_before@link": {"byte"},
+    "list_insert_before@prev": {"byte"},
+    "set_dead@bit": {"byte", "call #0 memory", "call #1 memory", "call #2 memory"},
+    "set_dead@clear": {"byte", "call #2 memory"},
+    "set_dead@gate": {"byte", "call #0", "call #1", "call #2"},
+    "set_dead@pal": {"call #0", "call #2"},
+    "set_dead@pset": {"byte", "call #0"},
+    "set_dead@render": {"byte"},
+    "set_dead@table": {"byte", "call #0", "call #0 memory", "call #1", "call #2"},
+    "snd_music_stop@al": {"eax"},
+    "snd_music_stop@cc": {"byte", "call #1 memory"},
+    "snd_music_stop@gate": {"call #0"},
+    "snd_music_stop@play": {"byte", "call #1", "eax"},
+    "snd_music_stop@stop": {"call #1"},
+    "snd_music_stop@zero": {"byte", "call #0 memory", "call #1 memory"},
+    "snd_sample_playing@al": {"byte", "call #1", "eax"},
+    "snd_sample_playing@clear": {"byte", "call #1 memory"},
+    "snd_sample_playing@driver": {"call #0", "eax"},
+    "snd_sample_playing@scan": {"byte", "call #1"},
+    "snd_sample_playing@status": {"byte", "eax"},
+    "snd_sample_playing@wide": {"call #0", "eax"},
+    "snd_sample_queue@armbuf": {"byte", "call #0", "call #1"},
+    "snd_sample_queue@armq": {"byte", "call #0", "call #1", "call #2", "call #3", "call #4"},
+    "snd_sample_queue@armst": {"byte", "call #1", "call #2"},
+    "snd_sample_queue@candcmp": {"byte"},
+    "snd_sample_queue@candmin": {"byte"},
+    "snd_sample_queue@driver": {"byte", "call #0", "call #1", "eax"},
+    "snd_sample_queue@loopbyte": {"byte"},
+    "snd_sample_queue@order": {"byte"},
+    "snd_sample_queue@pause": {"byte", "call #0", "call #1", "eax"},
+    "snd_sample_queue@size": {"byte", "call #0", "call #1"},
+    "snd_sample_queue@tailorder": {"call #0", "call #1", "call #2"},
+    "snd_sample_queue@tailret": {"eax"},
+    "snd_sample_queue@tailstop": {"call #0", "call #1", "call #2"},
+    "snd_sample_stop@al": {"eax"},
+    "snd_sample_stop@calls": {"call #1", "call #2"},
+    "snd_sample_stop@clear": {"byte"},
+    "snd_sample_stop@driver": {"byte", "call #0", "call #1", "call #2", "eax"},
+    "snd_sample_stop@eq2": {"byte", "call #1", "call #2", "eax"},
+    "snd_sample_stop@nolimit": {"byte", "call #3", "call #4", "call #5", "eax"},
+    "snd_samples_stop_all@al": {"eax"},
+    "snd_samples_stop_all@clr4": {"byte", "call #0 memory", "call #1 memory", "call #10 memory", "call #11 memory", "call #2 memory", "call #3 memory", "call #4 memory", "call #5 memory", "call #6 memory", "call #7 memory", "call #8 memory", "call #9 memory"},
+    "snd_samples_stop_all@clrc": {"byte", "call #0 memory", "call #1 memory", "call #10 memory", "call #11 memory", "call #2 memory", "call #3 memory", "call #4 memory", "call #5 memory", "call #6 memory", "call #7 memory", "call #8 memory", "call #9 memory"},
+    "snd_samples_stop_all@driver": {"byte", "call #0", "call #1", "call #2", "call #3", "eax"},
+    "snd_samples_stop_all@order": {"call #1", "call #10", "call #11", "call #2", "call #4", "call #5", "call #7", "call #8"},
+    "snd_samples_stop_all@skip2": {"call #1", "call #1 memory", "call #10", "call #11", "call #2", "call #2 memory", "call #3 memory", "call #4", "call #5", "call #6", "call #7", "call #8", "call #9"},
+}
+
 @needs_unicorn
 @unittest.skipUnless((os.path.exists(DIFFRUN) and os.path.exists(EXE)) or REQUIRED,
                      "build/diffrun or PRAGE.EXE absent")
@@ -1056,7 +1195,7 @@ class RealFunctionTests(unittest.TestCase):
                                              "fighter_slot_flag", "hit_anim_ctx", "hit_anim_start_b",
                                              "host_1b890", "rng_next"] + list(P1_MASKS) + list(P2_MASKS)
                                             + list(P3_MASKS) + list(P45_MASKS) + list(C1_MASKS)
-                                            + list(P6_MASKS) + list(C2_MASKS) + list(C2B_MASKS) + list(C3_MASKS) + list(P7_MASKS) + list(P8_MASKS)))
+                                            + list(P6_MASKS) + list(C2_MASKS) + list(C2B_MASKS) + list(C3_MASKS) + list(C3B_MASKS) + list(P7_MASKS) + list(P8_MASKS)))
         for name, r in self.real.items():
             if name == "host_1b890":       # the named gap (record E3 §E3.8), tested on its own below
                 continue
@@ -1072,7 +1211,7 @@ class RealFunctionTests(unittest.TestCase):
             "fighter_45878@mutant", "fighter_ctx_same@mutant", "fighter_slot_flag@mutant",
             "hit_anim_ctx@mutant", "hit_anim_start_b@mutant", "hit_anim_start_b@set", "rng_next@mutant"]
             + list(P1_KINDS) + list(P2_KINDS) + list(P3_KINDS) + list(P45_KINDS) + list(C1_KINDS)
-            + list(P6_KINDS) + list(C2_KINDS) + list(C2B_KINDS) + list(C3_KINDS) + list(P7_KINDS) + list(P8_KINDS)))
+            + list(P6_KINDS) + list(C2_KINDS) + list(C2B_KINDS) + list(C3_KINDS) + list(C3B_KINDS) + list(P7_KINDS) + list(P8_KINDS)))
         for name, r in self.mut.items():
             self.assertEqual(r.verdict, "MISMATCH", name)
 
@@ -1137,7 +1276,7 @@ class RealFunctionTests(unittest.TestCase):
             "fighter_3640c": 0, "fighter_37dcc": 0,
             "fighter_23130": 0xFF, "fighter_45878": 0, "anim_10fa8": 0, "anim_3e4e4": 0,
             "fighter_ctx_same": 0, "hit_anim_ctx": 0, "hit_anim_start_b": 0, "host_1b890": 0xFFFFFFFF,
-            **P1_MASKS, **P2_MASKS, **P3_MASKS, **P45_MASKS, **C1_MASKS, **P6_MASKS, **C2_MASKS, **C2B_MASKS, **C3_MASKS,
+            **P1_MASKS, **P2_MASKS, **P3_MASKS, **P45_MASKS, **C1_MASKS, **P6_MASKS, **C2_MASKS, **C2B_MASKS, **C3_MASKS, **C3B_MASKS,
             **P7_MASKS, **P8_MASKS})
         # with the full mask the slot-flag original's scratch bits (case f9: EAX = 0x201) differ
         spec = dataclasses.replace([s for s in V.SPECS if s.name == "fighter_slot_flag"][0],
@@ -1664,6 +1803,132 @@ class RealFunctionTests(unittest.TestCase):
         ):
             self.assertEqual(sorted({p.split(":")[0] for p in self.mut[name].problems}), ids, name)
 
+    def test_each_c3b_mutant_is_caught_by_what_it_breaks(self):
+        # track P batch C3b (record 2026-10-05-reverse-c3b): what alone catches each mutant
+        for name, want in C3B_KINDS.items():
+            got = {p.split(": ", 1)[1].split(":")[0] if p.split(": ", 1)[1].startswith("call #")
+                   else p.split(": ", 1)[1].split(" ")[0] for p in self.mut[name].problems}
+            self.assertEqual(got, want, name)
+        # the exact case set that alone catches each mutant (measured on the prototype)
+        for name, ids in (
+        ("actor_type_49444@bit", ['a1', 'a2', 'a3', 'a4', 'a5', 'a6', 'a7']),
+        ("actor_type_49444@child", ['a1', 'a2', 'a3', 'a4', 'a5', 'a6', 'a7']),
+        ("actor_type_49444@idx", ['a1', 'a2', 'a3', 'a4', 'a5', 'a6', 'a7']),
+        ("actor_type_49444@link", ['a1', 'a2', 'a3', 'a4', 'a5', 'a6', 'a7']),
+        ("actor_type_49444@reclr", ['a1', 'a2', 'a3', 'a4', 'a5', 'a6', 'a7']),
+        ("actor_type_49444@sign", ['a1', 'a2', 'a3', 'a4', 'a5', 'a6', 'a7']),
+        ("anim_operand@adv", ['o0', 'o1', 'o14']),
+        ("anim_operand@be8", ['o0', 'o14']),
+        ("anim_operand@byte", ['o11']),
+        ("anim_operand@deref", ['o5']),
+        ("anim_operand@mask", ['o3']),
+        ("anim_operand@op", ['o0', 'o1', 'o14']),
+        ("anim_operand@scale", ['o7']),
+        ("anim_operand@sel", ['o10', 'o6', 'o7', 'o8', 'o9']),
+        ("anim_read_var@a45", ['a7']),
+        ("anim_read_var@cswap", ['a10']),
+        ("anim_read_var@opff", ['a14']),
+        ("anim_read_var@pswap", ['a16', 'a17', 'a18', 'a19', 'a8']),
+        ("anim_read_var@ret", ['a12']),
+        ("anim_read_var@ring", ['a1']),
+        ("anim_read_var@sx", ['a13', 'a2']),
+        ("anim_read_var@w58", ['a6']),
+        ("anim_write_var@bep", ['w12', 'w19', 'w20', 'w21', 'w9']),
+        ("anim_write_var@child", ['w12', 'w13', 'w14']),
+        ("anim_write_var@high", ['w15', 'w16']),
+        ("anim_write_var@mask", ['w17', 'w18']),
+        ("anim_write_var@radio", ['w0', 'w1', 'w17', 'w2']),
+        ("anim_write_var@ringm", ['w1', 'w2']),
+        ("anim_write_var@swap", ['w10', 'w11', 'w19', 'w20', 'w21', 'w9']),
+        ("anim_write_var@w44", ['w10', 'w13', 'w7']),
+        ("effect_teardown@buf", ['e4']),
+        ("effect_teardown@flag", ['e0', 'e2', 'e3', 'e5']),
+        ("effect_teardown@restore", ['e0', 'e1', 'e2', 'e3', 'e4', 'e5', 'e6', 'e7']),
+        ("effect_teardown@tail", ['e0', 'e1', 'e2', 'e3', 'e4', 'e5', 'e6', 'e7']),
+        ("effect_teardown@type1", ['e1']),
+        ("effect_teardown@unlink", ['e0', 'e1', 'e2', 'e3', 'e4', 'e5', 'e6', 'e7']),
+        ("fighter_18350@anchor", ['b3']),
+        ("fighter_18350@bit", ['b1']),
+        ("fighter_18350@neg", ['b0', 'b1', 'b2', 'b3', 'b4', 'b5', 'b6', 'b7', 'b8', 'b9']),
+        ("fighter_18350@side", ['b1']),
+        ("fighter_18350@tab", ['b1', 'b2', 'b3', 'b4', 'b6']),
+        ("fighter_18540@default", ['c1', 'c2', 'c3', 'c4', 'c5', 'c6']),
+        ("fighter_18540@gt", ['c9']),
+        ("fighter_18540@idx", ['c12']),
+        ("fighter_18540@mask", ['c12']),
+        ("fighter_18540@noclamp", ['c0', 'c8', 'c9']),
+        ("fighter_18540@side", ['c1']),
+        ("fighter_38154@abs", ['d0', 'd11', 'd14', 'd7']),
+        ("fighter_38154@bd", ['d0', 'd11', 'd15', 'd16', 'd7']),
+        ("fighter_38154@bit", ['d0', 'd1', 'd11', 'd2', 'd4', 'd5', 'd7', 'd8']),
+        ("fighter_38154@call", ['d0', 'd1', 'd11', 'd14', 'd7', 'd8']),
+        ("fighter_38154@dir", ['d1', 'd14', 'd8']),
+        ("fighter_38154@flag", ['d0', 'd11', 'd12', 'd13', 'd5', 'd7']),
+        ("fighter_38154@mode8", ['d0', 'd11', 'd7']),
+        ("fighter_38154@range", ['d0', 'd11', 'd12', 'd7', 'd8']),
+        ("fighter_anim_triple@add", ['t1', 't2', 't4']),
+        ("fighter_anim_triple@idx", ['t2', 't3', 't4']),
+        ("fighter_anim_triple@o0", ['t0', 't1', 't2', 't3', 't4']),
+        ("fighter_anim_triple@o1", ['t0', 't1', 't2', 't3', 't4']),
+        ("fighter_anim_triple@o2", ['t0', 't1', 't2', 't3', 't4']),
+        ("fighter_anim_triple@shift", ['t0', 't1', 't2', 't3', 't4']),
+        ("hit_record_y@add", ['y1', 'y2', 'y3']),
+        ("hit_record_y@always", ['y1', 'y3']),
+        ("hit_record_y@bit", ['y0']),
+        ("hit_record_y@call", ['y1', 'y2', 'y3']),
+        ("hit_record_y@rec", ['y0']),
+        ("list_insert_before@back", ['v0', 'v2']),
+        ("list_insert_before@forward", ['v0']),
+        ("list_insert_before@head", ['v0', 'v1']),
+        ("list_insert_before@link", ['v0', 'v1', 'v2']),
+        ("list_insert_before@prev", ['v0', 'v1', 'v2']),
+        ("set_dead@bit", ['s0', 's1', 's2', 's3', 's4', 's5', 's6', 's7']),
+        ("set_dead@clear", ['s3', 's4', 's5', 's7']),
+        ("set_dead@gate", ['s3', 's4', 's5', 's7']),
+        ("set_dead@pal", ['s1', 's6', 's7']),
+        ("set_dead@pset", ['s6']),
+        ("set_dead@render", ['s2']),
+        ("set_dead@table", ['s4', 's7']),
+        ("snd_music_stop@al", ['m2']),
+        ("snd_music_stop@cc", ['m2']),
+        ("snd_music_stop@gate", ['m0']),
+        ("snd_music_stop@play", ['m1']),
+        ("snd_music_stop@stop", ['m2']),
+        ("snd_music_stop@zero", ['m0', 'm1', 'm2']),
+        ("snd_sample_playing@al", ['q3', 'q5']),
+        ("snd_sample_playing@clear", ['q3', 'q5']),
+        ("snd_sample_playing@driver", ['q0']),
+        ("snd_sample_playing@scan", ['q5']),
+        ("snd_sample_playing@status", ['q2', 'q4']),
+        ("snd_sample_playing@wide", ['q6']),
+        ("snd_sample_queue@armbuf", ['z3', 'z9']),
+        ("snd_sample_queue@armq", ['z4', 'z6', 'z8', 'z9']),
+        ("snd_sample_queue@armst", ['z5', 'z6', 'z8']),
+        ("snd_sample_queue@candcmp", ['z8']),
+        ("snd_sample_queue@candmin", ['z6', 'z8']),
+        ("snd_sample_queue@driver", ['z0']),
+        ("snd_sample_queue@loopbyte", ['z2', 'z3', 'z4', 'z5', 'z6', 'z7', 'z8', 'z9']),
+        ("snd_sample_queue@order", ['z8']),
+        ("snd_sample_queue@pause", ['z1']),
+        ("snd_sample_queue@size", ['z3', 'z4', 'z5', 'z6', 'z7', 'z8']),
+        ("snd_sample_queue@tailorder", ['z3', 'z4', 'z5', 'z6', 'z8']),
+        ("snd_sample_queue@tailret", ['z3', 'z4', 'z5', 'z6', 'z8']),
+        ("snd_sample_queue@tailstop", ['z3', 'z4', 'z5', 'z6', 'z8']),
+        ("snd_sample_stop@al", ['t3', 't4', 't5']),
+        ("snd_sample_stop@calls", ['t3', 't4', 't5']),
+        ("snd_sample_stop@clear", ['t3', 't4', 't5']),
+        ("snd_sample_stop@driver", ['t0']),
+        ("snd_sample_stop@eq2", ['t2', 't3', 't4', 't5']),
+        ("snd_sample_stop@nolimit", ['t3', 't4', 't5']),
+        ("snd_samples_stop_all@al", ['r1', 'r2']),
+        ("snd_samples_stop_all@clr4", ['r1', 'r2']),
+        ("snd_samples_stop_all@clrc", ['r1', 'r2']),
+        ("snd_samples_stop_all@driver", ['r0']),
+        ("snd_samples_stop_all@order", ['r2']),
+        ("snd_samples_stop_all@skip2", ['r1']),
+        ):
+            self.assertEqual(sorted({p.split(":")[0] for p in self.mut[name].problems}), ids, name)
+
     def test_each_p7_mutant_is_caught_by_what_it_breaks(self):
         # track P batch 7 (record 2026-10-04-reverse-p7): what alone catches each mutant
         for name, want in P7_KINDS.items():
@@ -1691,7 +1956,7 @@ class RealFunctionTests(unittest.TestCase):
                                  0x188DC: ("edx",), 0x1890C: ("edx",), 0x189FC: (), 0x18A4C: (),
                                  0x18AF8: ("ebx", "ecx", "edx"), 0x18B04: (), 0x18B44: (),
                                  0x18C14: ("ebx", "edx", "ebp"), 0x1A570: (), 0x1A5AC: (), 0x1A734: (),
-                                 0x1AB5C: ("ebp",), 0x1CA14: ("edx",), 0x1CA6C: (), 0x1CC28: ("edx",),
+                                 0x1AB5C: ("ebp",), 0x1CA14: ("edx",), 0x1CA40: (), 0x1CA6C: (), 0x1CC28: ("edx",),
                                  0x1CD9C: (), 0x1CE04: (), 0x1CE70: (), 0x1D238: (), 0x1D244: (),
                                  0x1DDF4: ("ebx", "edx"), 0x22404: (), 0x23960: (), 0x249B0: (), 0x249D0: (),
                                  0x29BC8: ("ebx", "edx"), 0x29C08: ("edx",), 0x29DB8: ("ebx", "edx"),
@@ -1712,7 +1977,9 @@ class RealFunctionTests(unittest.TestCase):
                                  0x3B90C: ("edx",), 0x3C148: (), 0x3C16C: (), 0x3C190: ("edx",),
                                  0x3C208: ("edx",), 0x3C358: (), 0x3C480: ("edx",), 0x3C4CC: ("edx",),
                                  0x3C520: ("edx",), 0x3C59C: ("edx",), 0x41310: (), 0x46460: ("edx",),
-                                 0x468D8: (), 0x48170: (), 0x49444: ("edi", "ebp"), 0x4F434: (), 0x5D7DC: ()})
+                                 0x468D8: (), 0x48170: (), 0x49444: ("edi", "ebp"), 0x4F434: (), 0x5D7DC: (), 0x5DC0F: ("ebx", "ecx", "edx"),
+                                 0x5DC8B: ("ebx", "ecx", "edx"), 0x5DD03: (),
+                                 0x5DEAF: ("ebx", "ecx", "edx")})
         for addr, declared in stubs.items():
             self.assertEqual(E.callee_clobbers(img, addr), declared, hex(addr))
 
@@ -1806,9 +2073,9 @@ class RealFunctionTests(unittest.TestCase):
             rc = V.main(["--diffrun", DIFFRUN, "--exe", EXE, "--image", os.path.join(self.tmp.name, "a.bin"),
                          "--self-check"])
         self.assertEqual(rc, 0)
-        # the closed-row count is over the rows that have callees (172), the 45 without are counted apart
-        self.assertIn("diff-verify: 217/217 functions VERIFIED; 664/664 mutants detected; 1 named gaps; "
-                      "154/172 rows with callees closed (45 have none).", out.getvalue())
+        # the closed-row count is over the rows that have callees (185), the 49 without are counted apart
+        self.assertIn("diff-verify: 234/234 functions VERIFIED; 779/779 mutants detected; 1 named gaps; "
+                      "165/185 rows with callees closed (49 have none).", out.getvalue())
 
 
 # ---- E3: the call list, named gaps, the callee column (record 2026-10-01-reverse-e3 §E3.4, §E3.8) --
```
