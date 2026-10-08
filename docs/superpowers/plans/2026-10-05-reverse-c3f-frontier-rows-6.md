# C3f: the frontier rows, part 6 (track P, batch C3f) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add the differential-verification row for each of the **thirteen addresses this batch
measures** — the leaf tier of the C3e record's §C3e.7 tail: `0x38BB0` (`fighter_38bb0`), `0x38BC8`
(`fighter_38bc8`), `0x3B038` (`fighter_3b038`), `0x46534` (`fighter_46534`), `0x3BDB0`
(`fight_attack_ready`), `0x4649C` (`fighter_input_scan`), `0x1AB10` (`fighter_state_ok`), `0x1E75C`
(`string_lock`), `0x1E808` (`string_unlock`), `0x474E4` (`string_decode`), `0x1C500`
(`game_string_get`), `0x1A6AC` (`fighter_block_anim`) and `0x3CF38` (`hit_chain_resolve`) — with
their bindings, mutants and exact-set pins; **the string pair's port restructuring** (`0x474E4`
owns its `0x1E75C`/`0x1E808` lock, so `0x1C500` is exactly its tail), **the `0x1E808` +0x10
correction** (the raw's `0x1E819` store of the `0x500BB` clock, which the port omitted), **the host
seams the text rows need** (`0x65546`/`0x61A70` wrappers; the `0x2F198`/`0x2F5A0`/`0x2EFD4`/
`0x2EF24` by-value seams), and the seams the stubbed callees need. **The terminal verdict**
(§C3f.7, required): after this batch the remaining frontier is the **nineteen deferred C3f
addresses plus the thirteen addresses rowing `0x3CF38` exposes** (`0x32BAC` is a named non-row),
each with its measured dependency order and evidence.

**Architecture:** Each row is a `Spec` in `tools/diff_verify.py` (`C3F_SPECS` with its `c3f_*`
fixtures), a `b_*` binding and `m_*` mutants in `port/tests/diff_runner.c` (`k_bindings`), and
exact-set assertions in `tools/tests/test_diff_verify.py` (`C3F_MASKS`, `C3F_KINDS`, the case-set
test, the clobber and counter lines). `port/src` changes: the `0x474E4`/`0x1C500` restructuring and
the `0x1E808` store in `flow.c`; the text seams + the two host wrappers in `actors.c`; the
`0x38C5C`/`0x38ED0`/`0x1A6AC`/`0x3CF38`/`0x3C6A8`/`0x3CD44`/`0x3CE58`/`0x32BAC` seams in
`fighter.c`; `fighter_input_scan`'s export.

**Tech Stack:** C11 (`port/src`, `port/tests`), Python 3.12 (`unittest`), `unicorn` 2.1.4 and
`capstone` 5.0.7 (`tools/requirements-diff.txt`), CMake, `make`.

**Spec:** `docs/superpowers/specs/2026-09-30-reverse-completion-design.md` §4 track P, §5.1-§5.3,
§6 ("each ported function has a verification row; unhit blocks and non-emulable instructions named;
counters and README percentage updated"), §7.

**Derivation record:** `docs/superpowers/plans/2026-10-05-reverse-c3f-derivations.md` (§C3f.1 the
members, §C3f.2 the rows, fixtures and the corrections, §C3f.3 the mutants and their measured
catching/kinds sets, §C3f.4 the counters, §C3f.5 the named gaps and limits, §C3f.6 what the planner
ran, §C3f.7 **the terminal verdict and the C3g list**). Recipe: `2026-10-01-reverse-e3-derivations.md`
§E3.10; checklist: `.superpowers/sdd/2026-10-01-plans/p-track-review-checklist.md`.

**What the planner ran (scratch, 2026-10-07, on `reverse-c3f` at `main` `0aa5eff` = C3e merged;
image sha1 `ff3b8cb14e00f1c282de7b7e15dcd7c230766947`):** a full prototype of all thirteen rows. The
scattered fighter leaves first, then the string pair and List C (`0x1A6AC`, `0x3CF38`). Every row
was measured with `--function NAME --self-check` until VERIFIED with every mutant detected, then
the full `python3 tools/diff_verify.py --self-check` counter (§C3f.4), `make entry-triage`
(byte-identical) and `python3 tools/port_progress.py` (`771 1203 64` / `731 731 100`). Then the
prototype was reverted (`git checkout -- port tools`); the patch below is the exact diff it applied
(see the file table for the list). The **32-address session measured only these thirteen**; the
other nineteen (§C3f.7's deferred list) and the thirteen the `0x3CF38` row exposes are C3g with
the measured evidence — a session shortfall, not a correctness judgement.

**Re-baseline note.** The counters below are `0aa5eff`'s (the C3e record §C3e.8's final: `278/278
functions VERIFIED; 1031/1031 mutants detected; 1 named gaps; 196/217 rows with callees closed (61
have none)`). If `main` moves before this plan executes, Task 1 records the measured base and every
later expected counter adds this plan's increments: functions +13, mutants +56, rows with callees
+8, no-callee rows +5, closed rows +9 (four of the new rows and the three existing rows the new
leaves close). The E2 table must not move (no E2 candidate; no `fn_register`): if a task regenerates
it, the line must be byte-identical.

## Decisions needed from the user

**None.** The `0x1E808` +0x10 store and the `0x474E4`/`0x1C500` restructuring are derived from the
raw with their addresses (§C3f.2); the nineteen unrowed tail addresses and the thirteen the
`0x3CF38` row exposes are deferred to C3g with the measured list and evidence (§C3f.7), exactly as
the C3e record's §C3e.7 anticipated.

## The C3f roadmap

| task | what | commit |
|---|---|---|
| 1 | baseline on `0aa5eff` (no commit) | - |
| 2 | the thirteen rows: the `0x1E808` correction, the string restructuring, the seams/exports, bindings, mutants and the test exact-set updates | `tools:` |
| 3 | the review sweep (stores, clobbers, mutant case sets) | `tests:` (only if a fix) |
| 4 | closure: PROGRESS, the record's §C3f.8, the final gates and the tail-verdict re-measure | `docs:` |

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
  and adds the C3f case-set test; no other assertion changes.
- AGENTS.md: "`data/` is git-ignored and read-only — never write to it." / "Commit style:
  `<area>: <what changed>`. **Never `git add -A`** — stage named files." No `Co-Authored-By`
  trailer. Never run `make gp-capture`.
- The C3f brief: the gp miss sets must stay as Task 1 measures them. The `0x1E808` correction and
  the `0x474E4`/`0x1C500` restructuring touch the string path every screen uses; `make gp-oracle`
  and Task 4's full `make verify` are the batch's evidence that no oracle moved.
- Spec §5.3 / E3 §E3.4: "VERIFIED" is equivalence on the exercised blocks and inputs only, each
  function with its callees stubbed or run as stated.

## Where to run

The worktree `.worktrees/reverse-c3f` (branch `reverse-c3f`). Every command runs from the worktree
root; `data/` and `.superpowers/` are symlinks; `build/` exists. Before Task 1:

```bash
git rev-parse HEAD   # 0aa5eff
python3 -c "import unicorn, capstone; print(unicorn.__version__, capstone.__version__)"  # 2.1.4 5.0.7
cmake -S port -B build && cmake --build build 2>&1 | tail -1
./build/diffrun --exe data/game/C/PRAGE.EXE --image-out /tmp/pr_c3f_img.bin && shasum /tmp/pr_c3f_img.bin
ls port/tests/ghidra_data.bin   # the PR_ORACLE_REQUIRED fixture (git-ignored); copy it from the main repo if absent
ls data/k11-captures/gp-idle-loss   # the gp-oracle capture (git-ignored)
```

The image line must print `ff3b8cb14e00f1c282de7b7e15dcd7c230766947`; another hash means the image
differs from the one the record measured: stop. Ledger:
`.superpowers/sdd/2026-10-05-reverse-c3f/progress.md`.

## The thirteen rows (the brief's grouping, executed as families)

The patch is one `git diff` (Task 2 applies it at once); the families below are the gate order:

| family | rows | shared seams/fixtures |
|---|---|---|
| the fighter leaves | `0x38BB0` `0x38BC8` `0x3B038` `0x46534` `0x3BDB0` `0x4649C` `0x1AB10` | the existing `0x33950`/`0x33A10`/`0x38BB0` allows; `c3f_38b_case`, `c3f_b038_case`, `c3f_46534_case`, `c3f_ready_case`, `c3f_scan_case`, `c3f_ok_case` |
| the string pair | `0x1E75C` `0x1E808` `0x474E4` `0x1C500` | the `0x1E75C`/`0x1E808`/`0x474E4` seams; `c3f_lock_case`, `c3f_unlock_case`, `c3f_str_case`; the `0x1E808` +0x10 correction; the `game_string_get`/`string_decode` split |
| List C | `0x1A6AC` `0x3CF38` | `HIT_A` (`0x3C480`), `0x18B04`, `0x3CD44`/`0x3CE58`/`0x3C6A8`/`0x32BAC` stubs; `c3f_anim_case`, `c3f_chain_case` |

The measured rows (cases/blocks/mutants measured; `EAX mask` from the Spec):

| row | entry | cases | blocks | mutants | EAX mask | named notes |
|---|---|---|---|---|---|---|
| `fighter_38bb0` | 0x38BB0 | 2 | 3/3 | 3/3 | 0 | the three 0x40-byte tables seeded with different fills |
| `fighter_38bc8` | 0x38BC8 | 2 | 3/3 | 3/3 | 0 | 0x38BB0 allow; the 0x107D24 word |
| `fighter_3b038` | 0x3B038 | 8 | 5/5 | 3/3 | 0xFF | negative k (the only real miss band) |
| `fighter_46534` | 0x46534 | 10 | 7/7 | 4/4 | 0 | signed clamp, floor order, cap table |
| `fight_attack_ready` | 0x3BDB0 | 7 | 4/4 | 3/3 | 0xFF | 0x33950 allow |
| `fighter_input_scan` | 0x4649C | 10 | 12/12 | 4/4 | 0xFF | ring wrap 0x13, skip/scan |
| `fighter_state_ok` | 0x1AB10 | 8 | 14/14 | 3/3 | 0xFF | unsigned `jbe` bounds |
| `string_lock` | 0x1E75C | 5 | 4/4 | 4/4 | 0xFFFFFFFF | handle at 0x10AA00 |
| `string_unlock` | 0x1E808 | 3 | 1/1 | 4/4 | 0 | 0x500BB allow; the +0x10 store |
| `string_decode` | 0x474E4 | 8 | 17/17 | 7/7 | 0xFFFFFFFF | 0x1E75C/0x1E808 stubs choose the base |
| `game_string_get` | 0x1C500 | 3 | 3/3 | 4/4 | 0xFFFFFFFF | 0x474E4 stub (the nil/truncated tails) |
| `fighter_block_anim` | 0x1A6AC | 11 | 8/8 | 6/6 | 0 | 0x33A68 allow; the 0xC8F40/0xC8F90 tables |
| `hit_chain_resolve` | 0x3CF38 | 11 | 12/12 | 8/8 | 0xFF | 0x32BAC named; 0x3C6A8/0x3CD44/0x3CE58 stubs (C3g) |

No row reads outside the image (the string tables and handles live in the data object; the ring is
at 0x108270).

## How the code steps are written

Task 2's patch is the prototype's exact `git diff` (the planner applied it, ran every gate, and
reverted). Apply it with `git apply`; it touches `port/src/game/actors.c`, `port/src/game/actors.h`,
`port/src/game/flow.c`, `port/src/game/flow.h`, `port/src/game/fighter.c`,
`port/src/game/fighter.h`, `port/tests/diff_runner.c`, `tools/diff_verify.py` and
`tools/tests/test_diff_verify.py`. If `main` moved, `git apply --3way` and resolve by keeping the
patch's additions at the same anchors; never merge the E2 table by hand.

## File Structure

| File | Responsibility |
|---|---|
| `port/src/game/flow.c`/`.h` | the `0x474E4`/`0x1C500` restructuring, the `0x1E75C`/`0x1E808` seams and the `0x1E819` store |
| `port/src/game/actors.c`/`.h` | the text seams, the two host-libc seam wrappers, `text_number_core` |
| `port/src/game/fighter.c`/`.h` | the `0x38C5C`/`0x38ED0`/`0x1A6AC`/`0x3CF38`/`0x3C6A8`/`0x3CD44`/`0x3CE58`/`0x32BAC` seams, `fighter_input_scan`'s export |
| `port/tests/diff_runner.c` | the thirteen bindings and 56 mutants, the `c3f_*` cores |
| `tools/diff_verify.py` | `C3F_SPECS` (the thirteen rows and their fixtures) |
| `tools/tests/test_diff_verify.py` | `C3F_MASKS`, `C3F_KINDS`, the case-set test, the clobber and counter lines |
| `docs/superpowers/plans/2026-10-05-reverse-c3f-derivations.md` | the record (this plan's source of values) |
| `docs/PROGRESS.md` | Task 4's paragraph |

---

### Task 1: Baseline (no commit)

**Files:** none. **Interfaces:** consumes the worktree at `0aa5eff`; produces the baseline counters
and table.

- [ ] **Step 1: the task gates on the untouched tree.**

```bash
make diff-verify DIFF_IMAGE=/tmp/pr_c3f_base.bin DIFF_TABLE=/tmp/pr_c3f_base_table.md 2>&1 | tail -1
make entry-triage E2_IMAGE=/tmp/pr_c3f_base_e2.bin 2>&1 | tail -2
python3 tools/port_progress.py
```

Expected (measured at `0aa5eff` by the planner):

```
diff-verify: 278/278 functions VERIFIED; 1031/1031 mutants detected; 1 named gaps; 196/217 rows with callees closed (61 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
entry-triage: targets 233 unported, 262 ported; supplement 131 (3 unported, 0 stale); untrusted entries 30
entry-triage: voice sites outside Ghidra 134: 0 in unported code, 115 in ported code, 19 nowhere
771 1203 64
731 731 100 (portable: excludes 81 host-owned/deferred and runtime >= 5D000)
```

- [ ] **Step 2: pin the gp miss sets and the oracle WAV.** The `PR_GP_DUMP` scenarios' pinned sets
  in `test_platform.c` must be the ones Task 2 leaves untouched; the string restructuring is the
  batch's behavioral change and `make gp-oracle` plus Task 4's full `make verify` are the evidence.

### Task 2: the thirteen rows (one commit)

**Files:** the patch below. **Interfaces:** produces `C3F_SPECS`, the thirteen bindings and 56
mutants, the restructuring, the correction, the seams/exports, and the test exact-set updates;
consumes the C1-C3e fixtures and the seam table.

- [ ] **Step 1: apply the patch.** `git apply - <<'PATCH' ... PATCH` with the diff embedded at the
  end of this task (the exact prototype diff).

- [ ] **Step 2: build and run the fighter leaves' self-checks.**

```bash
cmake --build build 2>&1 | tail -1
for f in fighter_38bb0 fighter_38bc8 fighter_3b038 fighter_46534 fight_attack_ready \
         fighter_input_scan fighter_state_ok; do
  python3 tools/diff_verify.py --function "$f" --self-check
done
```

Expected rows (measured): `38bb0` 2/3-3/3/3, `38bc8` 2/3-3/3/3, `3b038` 8/5-5/5/3, `46534`
10/7-7/7/4, `fight_attack_ready` 7/4-4/4/3, `fighter_input_scan` 10/12-12/12/4, `fighter_state_ok`
8/14-14/14/3 (cases/blocks/mutants).

- [ ] **Step 3: the string pair's self-checks.**

```bash
for f in string_lock string_unlock string_decode game_string_get; do
  python3 tools/diff_verify.py --function "$f" --self-check
done
```

Expected rows (measured): `string_lock` 5/4-4/4/4, `string_unlock` 3/1-1/1/4, `string_decode`
8/17-17/17/7, `game_string_get` 3/3-3/3/4.

- [ ] **Step 4: List C's self-checks.**

```bash
for f in fighter_block_anim hit_chain_resolve; do
  python3 tools/diff_verify.py --function "$f" --self-check
done
```

Expected rows (measured): `fighter_block_anim` 11/8-8/8/6, `hit_chain_resolve` 11/12-12/12/8.

- [ ] **Step 5: the task gates.**

```bash
make diff-verify DIFF_IMAGE=/tmp/pr_c3f_after.bin DIFF_TABLE=/tmp/pr_c3f_after_table.md 2>&1 | tail -1
python3 -m unittest tools.tests.test_diff_verify 2>&1 | tail -3
make entry-triage E2_IMAGE=/tmp/pr_c3f_after_e2.bin 2>&1 | tail -2
PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -1
python3 tools/gen_symbols.py port/decomp /tmp/pr_c3f_sym.h && diff /tmp/pr_c3f_sym.h port/src/symbols.h
python3 tools/port_progress.py
```

Expected: the record's §C3f.4 counter (291/291 functions, 1087/1087 mutants, 205/225 closed,
66 without); `OK` (the extended exact sets); E2 byte-identical;
`all checks passed`; `symbols.h` regeneration byte-identical; `771 1203 64` / `731 731 100`.

- [ ] **Step 6: commit.**

```bash
git add port/src/game/actors.c port/src/game/actors.h port/src/game/flow.c port/src/game/flow.h \
        port/src/game/fighter.c port/src/game/fighter.h port/tests/diff_runner.c tools/diff_verify.py \
        tools/tests/test_diff_verify.py
git commit -m "tools: C3f: the tail's leaves, the string pair and the C3g list"
```

### Task 3: the review sweep (one commit, only if a fix)

**Files:** the patch's rows. **Interfaces:** consumes Task 2's state; produces the store sweep's
findings.

- [ ] **Step 1: the store sweep.** For every C3f row, poke each field the row writes to a value
  that differs from what it writes and re-run `--function NAME --self-check`; a store with no
  sentinel fails some mutant. Re-check the fields the record lists as the rows' writes: the
  0x107A80 tables, the 0x107D24 word, the 0x1082C8/0x1082D0 accumulators, the 0x10AA00 handle's
  +0x10/+0x15, the 0x10A200 out buffer, the string table bytes, the slots' +0x43/+0x54/+0x55/+0x5F/
  +0x63/+0x7C and the 0x105F38-free rows.
- [ ] **Step 2: the clobber and case-set re-check.** `python3 -m unittest
  tools.tests.test_diff_verify.RealFunctionTests` (the clobber re-derivation with the new stub
  entries and the C3f case-set test). Expected: OK.
- [ ] **Step 3: commit any fix.**

```bash
git add <the fixed files>
git commit -m "tests: C3f review sweep: the store sentinels and the case-set pins"
```

### Task 4: Closure (one commit)

**Files:** `docs/PROGRESS.md`, the record. **Interfaces:** consumes Task 2's measured state.

- [ ] **Step 1: the final gates.**

```bash
make diff-verify DIFF_IMAGE=/tmp/pr_c3f_close.bin 2>&1 | tail -1
make entry-triage E2_IMAGE=/tmp/pr_c3f_close_e2.bin 2>&1 | tail -2
PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -1
python3 tools/port_progress.py
make verify
```

Expected: the Task 2 counter; E2 byte-identical; `all checks passed`; `771 1203 64` / `731 731
100`; and the full `make verify` exit 0 with the oracle lines equal to
`.superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt`, the `make audio-render` WAV
equal to the pre-change WAV, every gp ratchet line `ok` with measured == pin, and `symbols.h`
byte-identical. The `port/src` changes are the restructuring, the correction, the seams and the
wrappers; the full ladder is what proves no oracle-visible path moved.

- [ ] **Step 2: re-measure the tail verdict** (the record's §C3f.7 lists) against the final tree:
  the unrowed callees of the thirteen rows and, from every row, the other frontier addresses. If
  any listed address got rowed, update §C3f.7 and the C3g list with the new evidence; otherwise the
  record's lists stand.

- [ ] **Step 3: append the PROGRESS paragraph** (the thirteen rows, the counter, the correction and
  the restructuring, the C3f → C3g tail verdict of record §C3f.7).

- [ ] **Step 4: append §C3f.8 Results to the record** (the executed tree's counters, the commit
  shas, the gate log lines), mirroring C3e's §C3e.8.

- [ ] **Step 5: commit.**

```bash
git add docs/PROGRESS.md docs/superpowers/plans/2026-10-05-reverse-c3f-derivations.md
git commit -m "docs: C3f closure: the thirteen rows measured and the C3g tail verdict"
```

### The patch

> **Pointer (C3g final-review nit):** this patch is the prototype **as first measured**. The C3f
> fix wave (record §C3f.8) later changed `host_memset`'s pointer form, made the `string_decode`
> outlen compare signed, grew `0x2EFD4`'s format buffer to `0x14` and fixed the `c3f_unlock_case`
> seed — so the patch's text at those four places is pre-fix; apply with record §C3f.2/§C3f.8.

```diff
diff --git a/port/src/game/actors.c b/port/src/game/actors.c
index 152c4d3..92a52bf 100644
--- a/port/src/game/actors.c
+++ b/port/src/game/actors.c
@@ -3882,6 +3882,12 @@ u32 actor_spawn(const u32 *desc, u32 a2, u32 a3, u32 a4, u32 a5)
     return rec;
 }
 
+/* PORT: the C3f text rows pass a caller's string/buffer by value, four little-endian dwords
+ * (record §P2.7): 0x2F198's string and 0x2EFD4/0x2EF24's buffers are on the caller's stack, whose
+ * address has no comparable mem[] offset. */
+#define C3F_PTR_DW(f, k) ((u32)(f)[k] | (u32)(f)[(k) + 1u] << 8 | (u32)(f)[(k) + 2u] << 16 \
+                          | (u32)(f)[(k) + 3u] << 24)
+
 /* ---- text renderer and record grid -------------------------------------
  * (0x2F0F0, 0x2F198, 0x2F280, 0x2F4BC, 0x2F5A0, 0x2F830)
  *
@@ -3933,6 +3939,9 @@ int text_width(const u8 *s, u32 mode)
  * therefore produced by the actor renderer (0x1C390), not written here. */
 u8 text_glyph_emit(s32 ch, s32 *col, s32 *row, u32 mode, u32 vertical)
 {
+    /* PORT: the harness seam passes &col/&row by value (record §P2.7): the raw's EDX/EBX are
+     * pointers into its caller's stack (0x2F830's locals), which have no mem[] offset. */
+    PR_SEAM_RET(0x2F5A0u, ch, (u32)*col, (u32)*row, mode, vertical);
     u32 c = (u32)ch & 0xffu;
     u32 cls = mode & 3u;
     u32 mhi = mode & 0xf000u;
@@ -4067,6 +4076,7 @@ void text_blit_string(const u8 *s, s32 x, s32 y)
  * limit (0x2A for horizontal, 0x1E for vertical). */
 s32 text_render(const u8 *s, u32 mode, s32 row, s32 col, u32 vertical)
 {
+    PR_SEAM_RET(0x2F830u, (u32)(s - mem), mode, row, col, vertical);
     u8 *m = (u8 *)s;   /* 0x2F830 writes the terminator into param_1 */
 
     if (*s == 0) return 0;                                 /* 0x2F849 */
@@ -4106,6 +4116,10 @@ s32 text_render(const u8 *s, u32 mode, s32 row, s32 col, u32 vertical)
  * (0x1223F) passes EAX=-1, EDX=4, EBX=0x1C500's result, ECX=0x1000. */
 void text_cursor_set(s32 col, s32 row, const u8 *s, u32 mode)
 {
+    /* PORT: the string passes by value, four little-endian dwords (record §P2.7): 0x2F4D0's
+     * buffer is on its caller's stack, so the address itself has no comparable value. */
+    PR_SEAM(0x2F198u, col, row, C3F_PTR_DW(s, 0), C3F_PTR_DW(s, 4), C3F_PTR_DW(s, 8),
+            C3F_PTR_DW(s, 12), mode);
     if (row == -1) {
         /* 0x2F1B4/0x2F1BA: reload both cursor words and sign-extend them. */
         col = (s16)DSW(DS_00105F34 + 2);
@@ -4134,6 +4148,7 @@ void text_cursor_next_line(const u8 *s, u32 mode)
  * next row, and releases every non-empty record through 0x2AD40. */
 void text_cells_release(s32 col, s32 row, const u8 *s, u32 mode)
 {
+    PR_SEAM(0x2F280u, col, row, (u32)(s - mem), mode);
     s32 count = text_width(s, mode);
     if (col < 0) {
         col = (0x2b - count) >> 1;
@@ -4197,6 +4212,7 @@ void text_cells_release_count(s32 col, s32 row, s32 count)
  * this cycle, so the port takes the arguments explicitly. */
 void text_cursor_hold(s32 col, s32 row, const u8 *s, u32 mode)
 {
+    PR_SEAM(0x2F4BCu, col, row, (u32)(s - mem), mode);
     u32 save = DSD(DS_00105F34);
     text_cursor_set(col, row, s, mode);
     DSD(DS_00105F34) = save;
@@ -4222,6 +4238,7 @@ void text_cursor_hold_font2(s32 col, s32 row, const u8 *s, u32 mode)
  * which sits in no Ghidra function; only 0x38D90's is ported. */
 void text_vertical_set(s32 col, s32 row, const u8 *s, u32 mode)
 {
+    PR_SEAM(0x2F20Cu, col, row, (u32)(s - mem), mode);
     if (row == -1) {
         col = (s16)DSW(DS_00105F34 + 2);                /* 0x2F228/0x2F234 */
         row = (s16)DSW(DS_00105F34);                    /* 0x2F22E/0x2F237 */
@@ -4244,6 +4261,7 @@ void text_vertical_set(s32 col, s32 row, const u8 *s, u32 mode)
  * sits in no Ghidra function; only 0x38C5C's is ported. */
 void text_cells_release_vertical(s32 col, s32 row, const u8 *s)
 {
+    PR_SEAM(0x2F314u, col, row, (u32)(s - mem));
     s32 count = (s32)strlen((const char *)s);           /* 0x2F31F..0x2F328 */
     u32 idx = (u32)row * 0xacu + (u32)col * 4u;         /* 0x2F335..0x2F341 */
     for (s32 i = 0; i < count; i++) {                   /* 0x2F331/0x2F37A */
@@ -4258,9 +4276,37 @@ void text_cells_release_vertical(s32 col, s32 row, const u8 *s)
     }
 }
 
+/* PORT: the harness seam for the host-libc pair the C3f text rows stub (record §C3f.2): the WATCOM
+ * bodies at 0x65546/0x61A70 trap under unicorn (the sprintf chain reaches `hlt` at 0xFF3C), so the
+ * rows run them mode="stub" and the port's calls must be interceptable. Both are inert outside
+ * build/diffrun. 0x65546's dest is the caller's buffer; for a stack buffer the seam passes its
+ * four dwords by value (record §P2.7). */
+static s32 host_sprintf(u8 *dest, const u8 *fmt, s32 value)
+{
+    PR_SEAM_RET(0x65546u, (u32)(dest - mem), (u32)(fmt - mem), value);
+    return (s32)snprintf((char *)dest, 0x10u, (const char *)fmt, (int)value);
+}
+
+static void host_memset(u32 dest, u32 fill, u32 len)
+{
+    PR_SEAM(0x61A70u, dest, fill, len);
+    memset(mem + dest, (int)fill, (size_t)len);
+}
+
+/* 0x2EF24. EAX = value, EDX = dest. Formats the value with the libc
+ * sprintf 0x65546 and the format "%i" at 0x80B40 into dest and returns its
+ * length (the `repne scasb` strlen). */
+s32 text_number_core(s32 value, u8 *dest)
+{
+    PR_SEAM_RET(0x2EF24u, value, C3F_PTR_DW(dest, 0), C3F_PTR_DW(dest, 4),
+                C3F_PTR_DW(dest, 8), C3F_PTR_DW(dest, 12));
+    host_sprintf(dest, mem + 0x80B40u, value);              /* 0x2EF2D 0x65546 */
+    return (s32)strlen((const char *)dest);
+}
+
 /* 0x2EFD4 (with 0x2EF24). EAX = value, EDX = dest, EBX = width, ECX = pad.
  * 0x2EF24 formats the value with the libc sprintf 0x65546 and the format
- * "%i" at 0x80B40 into a 0x0C-byte stack buffer and returns its length L.
+ * "%i" at 0x80B40 into a 0x14-byte stack buffer and returns its length L.
  * When width <= L (0x2EFEF `jg`) the last `width` characters are copied
  * (0x2EFF3..0x2F00A). Otherwise pad selects the jump table at 0x2EFC4:
  * 0 right-justifies with '0' (0x2F026, 0x61A70 = memset), 1 right-justifies
@@ -4270,8 +4316,10 @@ void text_cells_release_vertical(s32 col, s32 row, const u8 *s)
  * (0x2F0D8). Returns L. */
 s32 text_number_format(s32 value, u8 *dest, s32 width, u32 pad)
 {
-    char buf[16];
-    s32 len = (s32)snprintf(buf, sizeof buf, "%i", (int)value); /* 0x2EF24 */
+    PR_SEAM_RET(0x2EFD4u, value, C3F_PTR_DW(dest, 0), C3F_PTR_DW(dest, 4),
+                C3F_PTR_DW(dest, 8), C3F_PTR_DW(dest, 12), width, pad);
+    char buf[0x10] = {0};
+    s32 len = text_number_core(value, (u8 *)buf);           /* 0x2EFE2 0x2EF24 */
     s32 gap = width - len;                                  /* 0x2EFE9 */
     s32 end = width;
     if (gap <= 0) {
@@ -4282,11 +4330,12 @@ s32 text_number_format(s32 value, u8 *dest, s32 width, u32 pad)
         case 0u:
         case 1u:
             memcpy(dest + gap, buf, (size_t)len);           /* 0x2F026/0x2F059 */
-            memset(dest, pad == 0u ? 0x30 : 0x20, (size_t)gap); /* 0x2F041/0x2F074 */
+            host_memset((u32)(dest - mem), pad == 0u ? 0x30u : 0x20u,
+                        (u32)gap);                          /* 0x2F041/0x2F074 0x61A70 */
             break;
         case 2u:
             memcpy(dest, buf, (size_t)len);                 /* 0x2F08C..0x2F0A1 */
-            memset(dest + len, 0x20, (size_t)gap);          /* 0x2F0AA */
+            host_memset((u32)(dest - mem) + (u32)len, 0x20u, (u32)gap); /* 0x2F0AA */
             break;
         default:
             memcpy(dest, buf, (size_t)len);                 /* 0x2F0C2..0x2F0D7 */
@@ -4304,6 +4353,7 @@ s32 text_number_format(s32 value, u8 *dest, s32 width, u32 pad)
  * with 0x2F198, the cursor saved (0x2F4E4) and restored (0x2F4FE). */
 void text_number_draw(s32 col, s32 row, s32 value, s32 width, u32 pad, u32 mode)
 {
+    PR_SEAM(0x2F4D0u, col, row, value, width, pad, mode);
     /* PORT: the original's buffer is uninitialised stack; the port zeroes it,
      * which only a pad above 3 (no caller) could observe. */
     u8 buf[0x14] = {0};
diff --git a/port/src/game/actors.h b/port/src/game/actors.h
index 382ee82..8997247 100644
--- a/port/src/game/actors.h
+++ b/port/src/game/actors.h
@@ -196,6 +196,7 @@ void text_cells_release_vertical(s32 col, s32 row, const u8 *s);
 /* 0x2EFD4 (with 0x2EF24). "%i" of `value` into `dest`, fitted to `width` by
  * `pad` (0 '0'-left, 1 ' '-left, 2 ' '-right, 3 none); returns the digit
  * count. */
+s32 text_number_core(s32 value, u8 *dest);                       /* 0x2EF24 */
 s32 text_number_format(s32 value, u8 *dest, s32 width, u32 pad);
 /* 0x2F4D0. EAX = col, EDX = row, EBX = value, ECX = width, stack pad and mode:
  * 0x2EFD4 then 0x2F198 with the cursor saved and restored. */
diff --git a/port/src/game/fighter.c b/port/src/game/fighter.c
index 4cfaecd..b7d45d0 100644
--- a/port/src/game/fighter.c
+++ b/port/src/game/fighter.c
@@ -606,7 +606,7 @@ u32 fighter_input_read(u32 side, s32 index)
 
 /* 0x4649C. Skip `n1` ring entries, then scan the next `n2` for a word whose low
  * 16 bits overlap `mask`; 1 on the first hit, 0 otherwise. */
-static int fighter_input_scan(u32 side, s32 n1, s32 n2, u32 mask)
+int fighter_input_scan(u32 side, s32 n1, s32 n2, u32 mask)
 {
     s32 pos = (s32)DSD(DS_001082D2) >> 16;      /* 0x464A5/0x464AC */
     if (n1 > 0) {
@@ -637,6 +637,7 @@ void fighter_ctx_rec_swap(u32 out[6], u32 rec);                  /* 0x33A68 */
 /* 0x1A6AC — record §38. */
 void fighter_block_anim(u32 slot, u32 rec)
 {
+    PR_SEAM(0x1A6ACu, slot, rec);
     u32 ctx[6];
     fighter_ctx_rec_swap(ctx, rec);                     /* 0x1A6B7 0x33A68 */
     hit_facing_flag(ctx[1]);                            /* 0x1A6C0 0x18B04 */
@@ -3201,6 +3202,7 @@ void fighter_37178(u32 slot)
  * strlen mode for 0x2F280; 0x2F314 ignores its ECX. */
 void fighter_38c5c(u32 side)
 {
+    PR_SEAM(0x38C5Cu, side);
     s32 c = (s32)side * 0x25;                                   /* 0x38C64..0x38C70 */
     text_cells_release(c + 2, 8, mem + FIGHT_TXT_2SP, 0x2000u); /* 0x38C8B 0x2F280 */
     text_cells_release_vertical(c + 2, 9, mem + FIGHT_TXT_6SP); /* 0x38CA1 0x2F314 */
@@ -3261,6 +3263,7 @@ void fighter_38d90(u32 side)
  * threshold met, or 0x14 ids with no terminator, returns 0. */
 u8 fighter_38ed0(u32 side, u32 rec)
 {
+    PR_SEAM_RET(0x38ED0u, side, rec);
     u32 ctx[6];
     fighter_ctx_same(ctx, side);                                /* 0x38EDF 0x33950 */
     for (u32 i = 0; i < 0x14u; i++) {                           /* 0x38FD9 */
@@ -3766,6 +3769,7 @@ u32 hit_frame_desc(u32 side, u32 i)
  * accumulator = 0, and the stun countdown = the frame's word at +0xC. */
 void hit_slot_seed(u32 side, u32 value, u32 i)
 {
+    PR_SEAM(0x3C6A8u, side, value, i);
     u32 desc = hit_frame_desc(side, i);
     u32 off = side * 0x40u + i * 2u;
     DSW(DS_00107D58 + off) = (u16)value;
@@ -3910,6 +3914,7 @@ int hit_stance_ok(u32 side, u32 i)
  * stance is valid, or -1. */
 s32 hit_scan(u32 side)
 {
+    PR_SEAM_RET(0x3CD44u, side);
     for (u32 i = 0; i < 0x20u; i++) {
         if ((s16)DSW(DS_00107D58 + side * 0x40u + i * 2u) == 8
                 && hit_stance_ok(side, i))
@@ -4232,6 +4237,7 @@ static void hit_stance_timer(u32 side)
  * entirely (slot+0x63 != 0). Correction to record §3.6/§7.11 (raw wins). */
 void hit_sound(u32 ch)
 {
+    PR_SEAM(0x32BACu, ch);
     (void)ch;
 }
 
@@ -5293,6 +5299,7 @@ void fighter_14c98(u32 slot, u32 rec, u32 side)
  * and 0x34E2C. */
 int hit_reaction_drive(u32 side, u32 i)
 {
+    PR_SEAM_RET(0x3CE58u, side, i);
     u32 slot = DS_001077B0 + side * 0x94u;
     u32 reaction;
     if (!hit_gate(side, i)) return 0;                   /* 0x3CE5E */
@@ -5313,6 +5320,7 @@ int hit_reaction_drive(u32 side, u32 i)
 /* 0x3CF38. The hit wrapper. */
 int hit_chain_resolve(u32 side)
 {
+    PR_SEAM_RET(0x3CF38u, side);
     u32 slot = DS_001077B0 + side * 0x94u;
     s32 i = hit_scan(side);                             /* 0x3CF54 */
     if (i == -1) return 0;                              /* 0x3CF5E */
diff --git a/port/src/game/fighter.h b/port/src/game/fighter.h
index 21f6309..84ad69f 100644
--- a/port/src/game/fighter.h
+++ b/port/src/game/fighter.h
@@ -424,6 +424,7 @@ void hit_anim_start_c(u32 rec, u32 stream, u32 frame_bits);
  * 0x107D24-adding sibling; exported for their differential rows' mutants. */
 void fighter_38bb0(u32 side);
 void fighter_38bc8(u32 side);
+int  fighter_input_scan(u32 side, s32 n1, s32 n2, u32 mask);     /* 0x4649C */
 
 /* C3e: the 0x392A0/0x3AAFC callees and 0x33A68's own row the differential
  * rows call (each already carries its seam). */
diff --git a/port/src/game/flow.c b/port/src/game/flow.c
index c3f7f41..f6186bf 100644
--- a/port/src/game/flow.c
+++ b/port/src/game/flow.c
@@ -231,8 +231,9 @@ static void title_origin_reset(u32 idx)
  * when it is locked or empty. The handle is {base @+8; len @+0xc; flags @+0x15}
  * (0x1E6D8/0x1E75C/0x1E808's block header); the port builds it in mem[] and the
  * "lock" is an inert single-threaded flag. */
-static u32 string_lock(u32 handle)
+u32 string_lock(u32 handle)
 {
+    PR_SEAM_RET(0x1E75Cu, handle);
     if ((DSB(handle + 0x15) & 1u) == 0 && DSD(handle + 0xc) != 0) {
         DSB(handle + 0x15) |= 2u;
         return DSD(handle + 8);
@@ -240,23 +241,30 @@ static u32 string_lock(u32 handle)
     return 0;
 }
 
-/* 0x1E808 — record §50-D. PORT: clear the lock bit. Its second argument feeds 0x500BB (a DPMI
- * page-map query) and a write to [arg+0x10] that the string reader never reads;
- * the port omits both, which is the arm 0x474E4 reaches. */
-static void string_unlock(u32 handle) { DSB(handle + 0x15) &= 0xFDu; }
+/* 0x1E808 — record §50-D. Clear the lock bit, then the 0x500BB DPMI clock read and its store:
+ * `call 0x500BB; mov [edx+0x10],eax` at 0x1E814/0x1E819, i.e. the clock shadow DS_00101500
+ * (0x500BB is `mov eax,[0x101500]; ret`). */
+void string_unlock(u32 handle)
+{
+    PR_SEAM(0x1E808u, handle);
+    DSB(handle + 0x15) &= 0xFDu;
+    DSD(handle + 0x10u) = DSD(DS_00101500);                 /* 0x1E814/0x1E819 0x500BB */
+}
 
-/* 0x474E4 — record §49-Z. Decode string `id` from the localisation table at
- * `base` into `out` (capacity `outlen`). The table's +4 holds a linked list of
- * group offsets relative to `base`; each group is a run of one-byte-length-
- * prefixed entries and an entry's bytes are XORed with its plaintext length
- * byte. Returns the decoded length + 1 (0 for an empty entry) or `outlen` when
- * truncated. PORT: the original locks the DS_001082DC handle itself (0x474F2
- * 0x1E75C) and unlocks it at 0x475A5 (0x1E808); the port's only caller,
- * game_string_get, does both around this body, so `base` arrives locked. The
- * `outlen` compare is signed (0x47556 JGE) and the ids are the callers'
- * constants. */
-static u32 string_decode(u32 base, u32 id, u8 *out, u32 outlen)
+/* 0x474E4 — record §49-Z. Decode string `id` from the localisation table into
+ * `out` (capacity `outlen`). The table's +4 holds a linked list of group
+ * offsets relative to the base the DS_001082DC handle names; each group is a
+ * run of one-byte-length-prefixed entries and an entry's bytes are XORed with
+ * its plaintext length byte. Returns the decoded length + 1 (0 for an empty
+ * entry) or `outlen` when truncated. The function owns the lock: 0x474F2
+ * loads the handle and calls 0x1E75C, 0x475A5 calls 0x1E808 (both always,
+ * even when the lock fails). The `outlen` compare is signed (0x47556 JGE) and
+ * the ids are the callers' constants. */
+u32 string_decode(u32 id, u8 *out, u32 outlen)
 {
+    PR_SEAM_RET(0x474E4u, id, (u32)(out - mem), outlen);
+    u32 handle = DSD(DS_001082DC);                          /* 0x474F2 */
+    u32 base = string_lock(handle);                         /* 0x474F7 0x1E75C */
     u32 off = 0;
     for (u32 g = id / 0x40u; g != 0; g--)
         off = DSD(base + off + 4u);             /* 0x4752F */
@@ -264,17 +272,21 @@ static u32 string_decode(u32 base, u32 id, u8 *out, u32 outlen)
     for (u32 i = id % 0x40u; i != 0; i--)
         p += (u32)DSB(p) + 1u;                  /* 0x47544 */
     u32 len = DSB(p);                           /* 0x4754D */
+    u32 ret;
     p += 1u;
     if (len < outlen) {
         for (u32 i = 0; i < len; i++)
             out[i] = (u8)(DSB(p + i) ^ (u8)len);   /* 0x47564 */
         out[len] = 0;                              /* 0x47572 */
-        return len != 0 ? len + 1u : 0u;
+        ret = len != 0 ? len + 1u : 0u;
+    } else {
+        for (u32 i = 0; i + 1u < outlen; i++)
+            out[i] = (u8)(DSB(p + i) ^ (u8)len);   /* 0x47591 */
+        out[outlen - 1u] = 0;                      /* 0x4759E */
+        ret = outlen;
     }
-    for (u32 i = 0; i + 1u < outlen; i++)
-        out[i] = (u8)(DSB(p + i) ^ (u8)len);       /* 0x47591 */
-    out[outlen - 1u] = 0;                          /* 0x4759E */
-    return outlen;
+    string_unlock(handle);                         /* 0x475A5 0x1E808 */
+    return ret;
 }
 
 /* PORT: 0x47370. The original loads the loaded-config language file (index 0 =
@@ -306,18 +318,13 @@ void game_string_table_load(const char *dir)
     DSD(DS_001082D8) = (u32)n;                  /* 0x473A3 */
 }
 
-/* PORT: 0x1C500 + 0x474E4. The original's EAX = string id, EDX = DS_00102760,
- * EBX = 0x100; 0x1C500 zeroes the first byte when 0x474E4 reports no string. */
+/* PORT: 0x1C500. EAX = string id, EDX = DS_00102760, EBX = 0x100; it calls
+ * 0x474E4 and zeroes the first byte when the return is 0. */
 const u8 *game_string_get(u32 id)
 {
-    u32 base = string_lock(DSD(DS_001082DC));
-    if (base != 0) {
-        string_decode(base, id, mem + DS_00102760, 0x100u);
-        string_unlock(DSD(DS_001082DC));
-    } else {
-        DSB(DS_00102760) = 0;                   /* 0x1C517 */
-    }
-    return (const u8 *)(mem + DS_00102760);
+    if (string_decode(id, mem + DS_00102760, 0x100u) == 0)  /* 0x1C50C/0x1C511 */
+        DSB(DS_00102760) = 0;                               /* 0x1C515/0x1C517 */
+    return (const u8 *)(mem + DS_00102760);                 /* 0x1C51D */
 }
 
 /* 0x1C500(0x15) -> 0x2F198: the title's caption. */
diff --git a/port/src/game/flow.h b/port/src/game/flow.h
index 04aebcc..6119b30 100644
--- a/port/src/game/flow.h
+++ b/port/src/game/flow.h
@@ -302,6 +302,9 @@ void game_attract_dump_frame(void);
  * a unit test; 0x121A0 uses it for string 0x15 ("THE FUTURE..."). */
 void game_string_table_load(const char *dir);
 const u8 *game_string_get(u32 id);
+u32  string_lock(u32 handle);                    /* 0x1E75C */
+void string_unlock(u32 handle);                  /* 0x1E808 */
+u32  string_decode(u32 id, u8 *out, u32 outlen); /* 0x474E4 */
 
 /* 0x4F1D0. Zeroes the two origin words DS_00107A3A/DS_00107A38. Distinct from
  * 0x4F1E4 (frontend_input_reset). Exposed so attract.c (0x11000 phase 0/1) and
diff --git a/port/tests/diff_runner.c b/port/tests/diff_runner.c
index 32f740d..ea56741 100644
--- a/port/tests/diff_runner.c
+++ b/port/tests/diff_runner.c
@@ -12058,6 +12058,297 @@ static void m_c3e_0fc_stream(const u32 *r, u32 *eax) { c3e_3a0fc_core(r, C3E_0FC
 static void m_c3e_0fc_key(const u32 *r, u32 *eax)    { c3e_3a0fc_core(r, C3E_0FC_KEY); *eax = 0u; }
 static void m_c3e_0fc_s59(const u32 *r, u32 *eax)    { c3e_3a0fc_core(r, C3E_0FC_S59); *eax = 0u; }
 
+/* ---- C3f (record 2026-10-05-reverse-c3f): the tail's tail ----------------------------------- */
+
+/* 0x474E4's mutants. */
+static u32 c3f_474e4_core(const u32 *r, u32 mut)
+{
+    u32 id = r[R_EAX];
+    u8 *out = mem + r[R_EDX];
+    u32 outlen = r[R_EBX];
+    u32 base = string_lock(DSD(0x001082DCu));
+    u32 off = 0;
+    for (u32 g = id / 0x40u; g != 0; g--)
+        off = DSD(base + off + ((mut & 8u) ? 0u : 4u));
+    u32 p = base + off + ((mut & 16u) ? 9u : 8u);
+    for (u32 i = id % 0x40u; i != 0; i--)
+        p += (u32)DSB(p) + ((mut & 32u) ? 0u : 1u);
+    u32 len = DSB(p);
+    u32 ret;
+    p += 1u;
+    if ((mut & 1u) ? (len <= outlen) : (len < outlen)) {
+        for (u32 i = 0; i < len; i++)
+            out[i] = (mut & 2u) ? DSB(p + i) : (u8)(DSB(p + i) ^ (u8)len);
+        if (!(mut & 64u)) out[len] = 0;
+        ret = (mut & 128u) ? len : (len != 0 ? len + 1u : 0u);
+    } else {
+        for (u32 i = 0; i + 1u < outlen; i++)
+            out[i] = (u8)(DSB(p + i) ^ (u8)len);
+        out[outlen - 1u] = 0;
+        ret = outlen;
+    }
+    string_unlock(DSD(0x001082DCu));
+    return ret;
+}
+static void b_c3f_474e4(const u32 *r, u32 *eax)
+{ *eax = string_decode(r[R_EAX], mem + r[R_EDX], r[R_EBX]); }
+static void m_c3f_474_xor(const u32 *r, u32 *eax)   { *eax = c3f_474e4_core(r, 2u); }
+static void m_c3f_474_trunc(const u32 *r, u32 *eax) { *eax = c3f_474e4_core(r, 1u); }
+static void m_c3f_474_term(const u32 *r, u32 *eax)  { *eax = c3f_474e4_core(r, 64u); }
+static void m_c3f_474_skip(const u32 *r, u32 *eax)  { *eax = c3f_474e4_core(r, 32u); }
+static void m_c3f_474_off(const u32 *r, u32 *eax)   { *eax = c3f_474e4_core(r, 16u); }
+static void m_c3f_474_ret(const u32 *r, u32 *eax)   { *eax = c3f_474e4_core(r, 128u); }
+static void m_c3f_474_link(const u32 *r, u32 *eax)  { *eax = c3f_474e4_core(r, 8u); }
+
+/* 0x1C500's mutants. */
+static u32 c3f_1c500_core(const u32 *r, u32 mut)
+{
+    if (mut & 8u) return 0x00102760u;
+    u32 ret = string_decode((mut & 2u) ? r[R_EAX] + 1u : r[R_EAX], mem + 0x00102760u, 0x100u);
+    if (ret == 0u && !(mut & 1u)) DSB(0x00102760u) = 0;
+    return (mut & 4u) ? 0x0010275Fu : 0x00102760u;
+}
+static void b_c3f_1c500(const u32 *r, u32 *eax)
+{ *eax = (u32)(game_string_get(r[R_EAX]) - mem); }
+static void m_c3f_500_zero(const u32 *r, u32 *eax) { *eax = c3f_1c500_core(r, 1u); }
+static void m_c3f_500_id(const u32 *r, u32 *eax)   { *eax = c3f_1c500_core(r, 2u); }
+static void m_c3f_500_ret(const u32 *r, u32 *eax)  { *eax = c3f_1c500_core(r, 4u); }
+static void m_c3f_500_call(const u32 *r, u32 *eax) { *eax = c3f_1c500_core(r, 8u); }
+
+/* 0x1A6AC's mutants. */
+static void c3f_1a6ac_core(const u32 *r, u32 mut)
+{
+    u32 slot = r[R_EAX], rec = r[R_EDX];
+    u32 ctx[6];
+    fighter_ctx_rec_swap(ctx, rec);
+    hit_facing_flag(ctx[1]);
+    u8 s54 = DSB(slot + ((mut & 1u) ? 0x55u : 0x54u));
+    if (s54 == 0u) {
+        if ((DSB(slot + 0x43u) & ((mut & 2u) ? 0x10u : 0x20u)) != 0u) return;
+        if (!(mut & 32u))
+            hit_anim_start_a((mut & 8u) ? ctx[5] : rec,
+                             DSD(((mut & 4u) ? 0x000C8F90u : 0x000C8F40u)
+                                 + (u32)DSB(slot + 0x7Au) * 4u), 0x40400000u);
+        DSB(slot + 0x43u) =
+            (u8)((DSB(slot + 0x43u) & ((mut & 16u) ? 0x3Fu : 0xCFu)) | 0x20u);
+    } else if (s54 == 1u) {
+        if ((DSB(slot + 0x43u) & 0x10u) != 0u) return;
+        if (!(mut & 32u))
+            hit_anim_start_a(rec, DSD(0x000C8F90u + (u32)DSB(slot + 0x7Au) * 4u),
+                             0x40400000u);
+        DSB(slot + 0x43u) = (u8)((DSB(slot + 0x43u) & 0xCFu) | 0x10u);
+    }
+}
+static void b_c3f_1a6ac(const u32 *r, u32 *eax)
+{ fighter_block_anim(r[R_EAX], r[R_EDX]); *eax = 0u; }
+static void m_c3f_6a_s54(const u32 *r, u32 *eax)  { c3f_1a6ac_core(r, 1u); *eax = 0u; }
+static void m_c3f_6a_bit(const u32 *r, u32 *eax)  { c3f_1a6ac_core(r, 2u); *eax = 0u; }
+static void m_c3f_6a_stream(const u32 *r, u32 *eax){ c3f_1a6ac_core(r, 4u); *eax = 0u; }
+static void m_c3f_6a_rec(const u32 *r, u32 *eax)  { c3f_1a6ac_core(r, 8u); *eax = 0u; }
+static void m_c3f_6a_mask(const u32 *r, u32 *eax) { c3f_1a6ac_core(r, 16u); *eax = 0u; }
+static void m_c3f_6a_call(const u32 *r, u32 *eax) { c3f_1a6ac_core(r, 32u); *eax = 0u; }
+
+/* 0x3CF38's mutants. */
+static u32 c3f_3cf38_core(const u32 *r, u32 mut)
+{
+    u32 side = r[R_EAX];
+    u32 slot = 0x001077B0u + side * 0x94u;
+    s32 i = hit_scan(side);
+    if (i == -1) return 0u;
+    if (!(mut & 1u)) {
+        u8 rr = DSB(slot + 0x5Fu);
+        if (rr >= 0x10u && rr <= 0x17u && (u32)i >= 0x1Cu) return 0u;
+    }
+    if ((mut & 2u) ? (hit_reaction_drive(side, (u32)i) == 0)
+                   : (hit_reaction_drive(side, (u32)i) != 0)) {
+        if (!(mut & 64u)) DSB(slot + 0x7Cu) = (u8)(DSB(slot + 0x7Cu) + 1u);
+        DSB(slot + 0x55u) = (u8)i;
+        if (!(mut & 4u)) hit_slot_seed(side, 0u, (u32)i);
+        if ((mut & 8u) ? (DSB(slot + 0x63u) != 0u) : (DSB(slot + 0x63u) == 0u))
+            hit_sound((u32)DSB(slot + 0x7Au));
+        return 1u;
+    }
+    if (!(mut & 16u) && DSB(slot + 0x53u) == 0u) DSB(slot + 0x5Fu) = 0xFFu;
+    if (!(mut & 32u)) DSB(slot + 0x55u) = 0xFFu;
+    return (mut & 128u) ? 1u : 0u;
+}
+static void b_c3f_3cf38(const u32 *r, u32 *eax) { *eax = (u32)hit_chain_resolve(r[R_EAX]); }
+static void m_c3f_cf_guard(const u32 *r, u32 *eax){ *eax = c3f_3cf38_core(r, 1u); }
+static void m_c3f_cf_drive(const u32 *r, u32 *eax){ *eax = c3f_3cf38_core(r, 2u); }
+static void m_c3f_cf_seed(const u32 *r, u32 *eax) { *eax = c3f_3cf38_core(r, 4u); }
+static void m_c3f_cf_sound(const u32 *r, u32 *eax){ *eax = c3f_3cf38_core(r, 8u); }
+static void m_c3f_cf_s5f(const u32 *r, u32 *eax)  { *eax = c3f_3cf38_core(r, 16u); }
+static void m_c3f_cf_s55(const u32 *r, u32 *eax)  { *eax = c3f_3cf38_core(r, 32u); }
+static void m_c3f_cf_inc(const u32 *r, u32 *eax)  { *eax = c3f_3cf38_core(r, 64u); }
+static void m_c3f_cf_ret(const u32 *r, u32 *eax)  { *eax = c3f_3cf38_core(r, 128u); }
+
+/* 0x38BB0's mutants. */
+static void c3f_38bb0_core(const u32 *r, u32 mut)
+{
+    u32 base = 0x00107A80u + r[R_EAX] * ((mut & 1u) ? 0x3Fu : 0x40u)
+             + ((mut & 4u) ? 1u : 0u);
+    u32 n = (mut & 2u) ? 0x3Fu : 0x40u;
+    for (u32 i = 0; i < n; i++) DSB(base + i) = 0;
+}
+static void b_c3f_38bb0(const u32 *r, u32 *eax)    { fighter_38bb0(r[R_EAX]); *eax = 0u; }
+static void m_c3f_bb_stride(const u32 *r, u32 *eax){ c3f_38bb0_core(r, 1u); *eax = 0u; }
+static void m_c3f_bb_len(const u32 *r, u32 *eax)   { c3f_38bb0_core(r, 2u); *eax = 0u; }
+static void m_c3f_bb_off(const u32 *r, u32 *eax)   { c3f_38bb0_core(r, 4u); *eax = 0u; }
+
+/* 0x38BC8's mutants. */
+static void c3f_38bc8_core(const u32 *r, u32 mut)
+{
+    u32 side = r[R_EAX];
+    if (!(mut & 1u)) DSW(0x00107D24u + side * ((mut & 2u) ? 1u : 2u)) = 0;
+    if (!(mut & 4u)) fighter_38bb0(side);
+}
+static void b_c3f_38bc8(const u32 *r, u32 *eax)    { fighter_38bc8(r[R_EAX]); *eax = 0u; }
+static void m_c3f_bc_word(const u32 *r, u32 *eax)  { c3f_38bc8_core(r, 1u); *eax = 0u; }
+static void m_c3f_bc_off(const u32 *r, u32 *eax)   { c3f_38bc8_core(r, 2u); *eax = 0u; }
+static void m_c3f_bc_clear(const u32 *r, u32 *eax) { c3f_38bc8_core(r, 4u); *eax = 0u; }
+
+/* 0x3B038's mutants. */
+static u32 c3f_3b038_core(const u32 *r, u32 mut)
+{
+    u32 side = r[R_EAX];
+    s32 base = (s32)DSD(0x000BE018u);
+    s32 k = (s32)DSD(0x000BEDEEu) >> 16;
+    s32 x = (s32)DSD(0x001077B0u + side * 0x94u + 0x2Cu);
+    s32 lhs = (mut & 4u) ? (base + k) : (base - k);
+    if ((mut & 1u) ? (lhs < x) : (lhs <= x)) return 1u;
+    if ((mut & 2u) ? (base - k >= x) : (k - base >= x)) return 1u;
+    return 0u;
+}
+static void b_c3f_3b038(const u32 *r, u32 *eax)    { *eax = (u32)fighter_3b038(r[R_EAX]); }
+static void m_c3f_b038_s1(const u32 *r, u32 *eax)  { *eax = c3f_3b038_core(r, 1u); }
+static void m_c3f_b038_s2(const u32 *r, u32 *eax)  { *eax = c3f_3b038_core(r, 2u); }
+static void m_c3f_b038_sum(const u32 *r, u32 *eax) { *eax = c3f_3b038_core(r, 4u); }
+
+/* 0x46534's mutants. */
+static void c3f_46534_core(const u32 *r, u32 mut)
+{
+    u32 side = r[R_EAX];
+    s32 v = (s32)DSD(0x001082C8u + side * 4u) + (s32)r[R_EDX];
+    u32 cap = (u32)DSB(0x000C9408u + (u32)DSB(0x0010452Cu) + ((mut & 1u) ? 1u : 0u));
+    DSD(0x001082C8u + side * 4u) = (u32)v;
+    if (mut & 8u) {
+        if ((s32)DSD(0x001082D0u) > (s32)DSD(0x001082C8u + side * 4u))
+            DSD(0x001082C8u + side * 4u) = DSD(0x001082D0u);
+    }
+    if (mut & 2u) {
+        if (cap < DSD(0x001082C8u + side * 4u)) DSD(0x001082C8u + side * 4u) = cap;
+        else if (DSD(0x001082C8u + side * 4u) >= 0x80000000u)
+            DSD(0x001082C8u + side * 4u) = 0u;
+    } else {
+        if ((s32)cap < (s32)DSD(0x001082C8u + side * 4u))
+            DSD(0x001082C8u + side * 4u) = cap;
+        else if ((s32)DSD(0x001082C8u + side * 4u) < 0)
+            DSD(0x001082C8u + side * 4u) = 0u;
+    }
+    if (!(mut & 4u) && !(mut & 8u)) {
+        if ((s32)DSD(0x001082D0u) > (s32)DSD(0x001082C8u + side * 4u))
+            DSD(0x001082C8u + side * 4u) = DSD(0x001082D0u);
+    }
+}
+static void b_c3f_46534(const u32 *r, u32 *eax)
+{ fighter_46534(r[R_EAX], (s32)r[R_EDX]); *eax = 0u; }
+static void m_c3f_46534_cap(const u32 *r, u32 *eax)  { c3f_46534_core(r, 1u); *eax = 0u; }
+static void m_c3f_46534_sign(const u32 *r, u32 *eax) { c3f_46534_core(r, 2u); *eax = 0u; }
+static void m_c3f_46534_floor(const u32 *r, u32 *eax){ c3f_46534_core(r, 4u); *eax = 0u; }
+static void m_c3f_46534_order(const u32 *r, u32 *eax){ c3f_46534_core(r, 8u); *eax = 0u; }
+
+/* 0x3BDB0's mutants. */
+static u32 c3f_3bdb0_core(const u32 *r, u32 mut)
+{
+    u32 ctx[6];
+    fighter_ctx_same(ctx, r[R_EAX]);
+    u32 self = (mut & 4u) ? (0x001077B0u + (1u - r[R_EAX]) * 0x94u) : ctx[2];
+    if (DSB(self + ((mut & 1u) ? 0x54u : 0x53u)) != 0u) return 0u;
+    if ((mut & 2u) ? DSB(self + 0x54u) == 2u : DSB(self + 0x54u) != 2u) return 1u;
+    return 0u;
+}
+static void b_c3f_3bdb0(const u32 *r, u32 *eax)    { *eax = (u32)fight_attack_ready(r[R_EAX]); }
+static void m_c3f_ready_s53(const u32 *r, u32 *eax){ *eax = c3f_3bdb0_core(r, 1u); }
+static void m_c3f_ready_s54(const u32 *r, u32 *eax){ *eax = c3f_3bdb0_core(r, 2u); }
+static void m_c3f_ready_side(const u32 *r, u32 *eax){ *eax = c3f_3bdb0_core(r, 4u); }
+
+/* 0x4649C's mutants. */
+static u32 c3f_4649c_core(const u32 *r, u32 mut)
+{
+    u32 side = r[R_EAX];
+    s32 n1 = (s32)r[R_EDX], n2 = (s32)r[R_EBX];
+    u32 mask = r[R_ECX];
+    s32 pos = (s32)DSD(0x001082D2u) >> 16;
+    if (n1 > 0) {
+        s32 n = n1;
+        do {
+            if (--pos < 0) pos = (mut & 1u) ? 0x14 : 0x13;
+        } while (--n > 0);
+    }
+    if (n2 > 0) {
+        s32 i = 0;
+        do {
+            u16 v = DSW(0x00108270u + side * 0x28u + (u32)pos * 2u);
+            if ((mut & 4u) ? ((u32)v & (mask >> 8)) : ((u32)v & (u16)mask)) return 1u;
+            if (--pos < 0) pos = (mut & 8u) ? 0x14 : 0x13;
+        } while (++i < ((mut & 2u) ? n2 - 1 : n2));
+    }
+    return 0u;
+}
+static void b_c3f_4649c(const u32 *r, u32 *eax)
+{ *eax = (u32)fighter_input_scan(r[R_EAX], (s32)r[R_EDX], (s32)r[R_EBX], r[R_ECX]); }
+static void m_c3f_scan_n1(const u32 *r, u32 *eax)  { *eax = c3f_4649c_core(r, 1u); }
+static void m_c3f_scan_n2(const u32 *r, u32 *eax)  { *eax = c3f_4649c_core(r, 2u); }
+static void m_c3f_scan_mask(const u32 *r, u32 *eax){ *eax = c3f_4649c_core(r, 4u); }
+static void m_c3f_scan_wrap(const u32 *r, u32 *eax){ *eax = c3f_4649c_core(r, 8u); }
+
+/* 0x1AB10's mutants. */
+static u32 c3f_1ab10_core(const u32 *r, u32 mut)
+{
+    u32 ctx[6];
+    fighter_ctx_swap(ctx, r[R_EAX]);
+    u32 self = ctx[3];
+    u8 a = DSB(self + ((mut & 1u) ? 0x53u : 0x54u));
+    u8 b = DSB(self + ((mut & 2u) ? 0x52u : 0x53u));
+    if ((mut & 4u) ? ((s8)a > 1) : ((u8)a > 1u)) return 0u;
+    if ((mut & 4u) ? ((s8)b > 1) : ((u8)b > 1u)) return 0u;
+    return 1u;
+}
+static void b_c3f_1ab10(const u32 *r, u32 *eax)    { *eax = (u32)fighter_state_ok(r[R_EAX]); }
+static void m_c3f_ok_s54(const u32 *r, u32 *eax)   { *eax = c3f_1ab10_core(r, 1u); }
+static void m_c3f_ok_s53(const u32 *r, u32 *eax)   { *eax = c3f_1ab10_core(r, 2u); }
+static void m_c3f_ok_sign(const u32 *r, u32 *eax)  { *eax = c3f_1ab10_core(r, 4u); }
+
+/* 0x1E75C's mutants. */
+static u32 c3f_1e75c_core(const u32 *r, u32 mut)
+{
+    u32 h = r[R_EAX];
+    if ((DSB(h + ((mut & 1u) ? 0x16u : 0x15u)) & ((mut & 2u) ? 2u : 1u)) == 0
+            && DSD(h + ((mut & 4u) ? 8u : 0xCu)) != 0) {
+        DSB(h + 0x15u) |= (mut & 8u) ? 4u : 2u;
+        return DSD(h + ((mut & 4u) ? 0xCu : 8u));
+    }
+    return 0u;
+}
+static void b_c3f_1e75c(const u32 *r, u32 *eax)    { *eax = string_lock(r[R_EAX]); }
+static void m_c3f_lock_bit(const u32 *r, u32 *eax) { *eax = c3f_1e75c_core(r, 1u); }
+static void m_c3f_lock_len(const u32 *r, u32 *eax) { *eax = c3f_1e75c_core(r, 2u); }
+static void m_c3f_lock_base(const u32 *r, u32 *eax){ *eax = c3f_1e75c_core(r, 4u); }
+static void m_c3f_lock_or(const u32 *r, u32 *eax)  { *eax = c3f_1e75c_core(r, 8u); }
+
+/* 0x1E808's mutants. */
+static void c3f_1e808_core(const u32 *r, u32 mut)
+{
+    u32 h = r[R_EAX];
+    DSB(h + ((mut & 1u) ? 0x14u : 0x15u)) &= (mut & 4u) ? 0xFEu : 0xFDu;
+    if (!(mut & 8u)) DSD(h + 0x10u) = (mut & 16u) ? 0u : DSD(0x00101500u);
+}
+static void b_c3f_1e808(const u32 *r, u32 *eax)    { string_unlock(r[R_EAX]); *eax = 0u; }
+static void m_c3f_unlock_off(const u32 *r, u32 *eax){ c3f_1e808_core(r, 1u); *eax = 0u; }
+static void m_c3f_unlock_and(const u32 *r, u32 *eax){ c3f_1e808_core(r, 4u); *eax = 0u; }
+static void m_c3f_unlock_store(const u32 *r, u32 *eax){ c3f_1e808_core(r, 8u); *eax = 0u; }
+static void m_c3f_unlock_clock(const u32 *r, u32 *eax){ c3f_1e808_core(r, 16u); *eax = 0u; }
+
 static const binding_t k_bindings[] = {
     { "rng_next",                 b_rng_next,     0xFFFFFFFFu },
     { "fighter_slot_flag",        b_slot_flag,    0x000000FFu },
@@ -13373,6 +13664,75 @@ static const binding_t k_bindings[] = {
     { "fighter_3a0fc@stream",              m_c3e_0fc_stream, 0x00000000u },
     { "fighter_3a0fc@key",                 m_c3e_0fc_key,  0x00000000u },
     { "fighter_3a0fc@s59",                 m_c3e_0fc_s59,  0x00000000u },
+    { "fighter_38bb0",                     b_c3f_38bb0,    0x00000000u },
+    { "fighter_38bb0@stride",              m_c3f_bb_stride, 0x00000000u },
+    { "fighter_38bb0@len",                 m_c3f_bb_len,   0x00000000u },
+    { "fighter_38bb0@off",                 m_c3f_bb_off,   0x00000000u },
+    { "fighter_38bc8",                     b_c3f_38bc8,    0x00000000u },
+    { "fighter_38bc8@word",                m_c3f_bc_word,  0x00000000u },
+    { "fighter_38bc8@off",                 m_c3f_bc_off,   0x00000000u },
+    { "fighter_38bc8@clear",               m_c3f_bc_clear, 0x00000000u },
+    { "fighter_3b038",                     b_c3f_3b038,    0x000000FFu },
+    { "fighter_3b038@s1",                  m_c3f_b038_s1,  0x000000FFu },
+    { "fighter_3b038@s2",                  m_c3f_b038_s2,  0x000000FFu },
+    { "fighter_3b038@sum",                 m_c3f_b038_sum, 0x000000FFu },
+    { "fighter_46534",                     b_c3f_46534,    0x00000000u },
+    { "fighter_46534@cap",                 m_c3f_46534_cap, 0x00000000u },
+    { "fighter_46534@sign",                m_c3f_46534_sign, 0x00000000u },
+    { "fighter_46534@floor",               m_c3f_46534_floor, 0x00000000u },
+    { "fighter_46534@order",               m_c3f_46534_order, 0x00000000u },
+    { "fight_attack_ready",                b_c3f_3bdb0,    0x000000FFu },
+    { "fight_attack_ready@s53",            m_c3f_ready_s53, 0x000000FFu },
+    { "fight_attack_ready@s54",            m_c3f_ready_s54, 0x000000FFu },
+    { "fight_attack_ready@side",           m_c3f_ready_side, 0x000000FFu },
+    { "fighter_input_scan",                b_c3f_4649c,    0x000000FFu },
+    { "fighter_input_scan@n1",             m_c3f_scan_n1,  0x000000FFu },
+    { "fighter_input_scan@n2",             m_c3f_scan_n2,  0x000000FFu },
+    { "fighter_input_scan@mask",           m_c3f_scan_mask, 0x000000FFu },
+    { "fighter_input_scan@wrap",           m_c3f_scan_wrap, 0x000000FFu },
+    { "fighter_state_ok",                  b_c3f_1ab10,    0x000000FFu },
+    { "fighter_state_ok@s54",              m_c3f_ok_s54,   0x000000FFu },
+    { "fighter_state_ok@s53",              m_c3f_ok_s53,   0x000000FFu },
+    { "fighter_state_ok@sign",             m_c3f_ok_sign,  0x000000FFu },
+    { "string_lock",                       b_c3f_1e75c,    0xFFFFFFFFu },
+    { "string_lock@bit",                   m_c3f_lock_bit, 0xFFFFFFFFu },
+    { "string_lock@len",                   m_c3f_lock_len, 0xFFFFFFFFu },
+    { "string_lock@base",                  m_c3f_lock_base, 0xFFFFFFFFu },
+    { "string_lock@or",                    m_c3f_lock_or,  0xFFFFFFFFu },
+    { "string_unlock",                     b_c3f_1e808,    0x00000000u },
+    { "string_unlock@off",                 m_c3f_unlock_off, 0x00000000u },
+    { "string_unlock@and",                 m_c3f_unlock_and, 0x00000000u },
+    { "string_unlock@store",               m_c3f_unlock_store, 0x00000000u },
+    { "string_unlock@clock",               m_c3f_unlock_clock, 0x00000000u },
+    { "string_decode",                     b_c3f_474e4,    0xFFFFFFFFu },
+    { "string_decode@xor",                 m_c3f_474_xor,  0xFFFFFFFFu },
+    { "string_decode@trunc",               m_c3f_474_trunc, 0xFFFFFFFFu },
+    { "string_decode@term",                m_c3f_474_term, 0xFFFFFFFFu },
+    { "string_decode@skip",                m_c3f_474_skip, 0xFFFFFFFFu },
+    { "string_decode@off",                 m_c3f_474_off,  0xFFFFFFFFu },
+    { "string_decode@ret",                 m_c3f_474_ret,  0xFFFFFFFFu },
+    { "string_decode@link",                m_c3f_474_link, 0xFFFFFFFFu },
+    { "game_string_get",                   b_c3f_1c500,    0xFFFFFFFFu },
+    { "game_string_get@zero",              m_c3f_500_zero, 0xFFFFFFFFu },
+    { "game_string_get@id",                m_c3f_500_id,   0xFFFFFFFFu },
+    { "game_string_get@ret",               m_c3f_500_ret,  0xFFFFFFFFu },
+    { "game_string_get@call",              m_c3f_500_call, 0xFFFFFFFFu },
+    { "fighter_block_anim",                b_c3f_1a6ac,    0x00000000u },
+    { "fighter_block_anim@s54",            m_c3f_6a_s54,   0x00000000u },
+    { "fighter_block_anim@bit",            m_c3f_6a_bit,   0x00000000u },
+    { "fighter_block_anim@stream",         m_c3f_6a_stream, 0x00000000u },
+    { "fighter_block_anim@rec",            m_c3f_6a_rec,    0x00000000u },
+    { "fighter_block_anim@mask",           m_c3f_6a_mask,   0x00000000u },
+    { "fighter_block_anim@call",           m_c3f_6a_call,   0x00000000u },
+    { "hit_chain_resolve",                 b_c3f_3cf38,    0x000000FFu },
+    { "hit_chain_resolve@guard",           m_c3f_cf_guard, 0x000000FFu },
+    { "hit_chain_resolve@drive",           m_c3f_cf_drive, 0x000000FFu },
+    { "hit_chain_resolve@seed",            m_c3f_cf_seed,  0x000000FFu },
+    { "hit_chain_resolve@sound",           m_c3f_cf_sound, 0x000000FFu },
+    { "hit_chain_resolve@s5f",             m_c3f_cf_s5f,   0x000000FFu },
+    { "hit_chain_resolve@s55",             m_c3f_cf_s55,   0x000000FFu },
+    { "hit_chain_resolve@inc",             m_c3f_cf_inc,   0x000000FFu },
+    { "hit_chain_resolve@ret",             m_c3f_cf_ret,   0x000000FFu },
 };
 
 static const binding_t *find_binding(const char *name)
diff --git a/tools/diff_verify.py b/tools/diff_verify.py
index 510618c..ce4db6b 100644
--- a/tools/diff_verify.py
+++ b/tools/diff_verify.py
@@ -6487,6 +6487,294 @@ C3E_SPECS = [
        mutants=("@frame", "@face", "@off", "@layer", "@stream", "@key", "@s59")),
 ]
 
+# ---- track P batch C3f (record 2026-10-05-reverse-c3f): the tail's tail --------------------------
+#
+# List B/C and the text-tree leaves first; the 0x38D90/0x38FEC tree follows (the batch's prototype
+# order is the record's §C3f dependency order).
+
+
+def c3f_38b_case():
+    """0x38BB0/0x38BC8: the three 0x40-byte 0x107A80 tables and the 0x107D24 word (each poke at
+    most 0x40 bytes: the driver's per-poke cap)."""
+    return {0x00107A40: bytes([0x5A]) * 0x40, 0x00107A80: bytes([0x5B]) * 0x40,
+            0x00107AC0: bytes([0x5C]) * 0x40, 0x00107D24: le16(0x1234)}
+
+
+def c3f_b038_case(side, base, k, x):
+    """0x3B038: the 0xBE018 base, the 0xBEDEE window word (high 16) and the side slot's +0x2C."""
+    return {0x000BE018: le32(base), 0x000BEDEE: le32(k << 16),
+            0x001077B0 + side * 0x94 + 0x2C: le32(x)}
+
+
+def c3f_46534_case(side, v, floor, capidx=0, cap=0x40):
+    """0x46534: the per-side accumulator 0x1082C8 (seed pre-delta), the 0x1082D0 floor, the
+    0x10452C index byte and the 0xC9408 cap table entry."""
+    return {0x001082C8 + side * 4: le32(v), 0x001082D0: le32(floor),
+            0x0010452C: bytes([capidx]), 0x000C9408 + capidx: bytes([cap])}
+
+
+def c3f_ready_case(side, s53, s54):
+    """0x3BDB0: the side slot's +0x53/+0x54."""
+    p = c3d_slot_pokes()
+    so = 0x001077B0 + side * 0x94
+    p[so + 0x53] = bytes([s53])
+    p[so + 0x54] = bytes([s54])
+    return p
+
+
+def c3f_scan_case(side, pos, values):
+    """0x4649C: the side's 0x14-word input ring at 0x108270 and the 0x1082D2 read position
+    (the raw reads the dword's high 16 as the word index). `values` maps ring index -> word."""
+    p = {0x00108270 + side * 0x28: le16(0x1111) * 0x14, 0x001082D2: le32(pos << 16)}
+    for i, v in values.items():
+        p[0x00108270 + side * 0x28 + i * 2] = le16(v)
+    return p
+
+
+def c3f_ok_case(side, s54, s53):
+    """0x1AB10: the side slot's +0x54/+0x53."""
+    p = c3d_slot_pokes()
+    so = 0x001077B0 + side * 0x94
+    p[so + 0x54] = bytes([s54])
+    p[so + 0x53] = bytes([s53])
+    return p
+
+
+def c3f_lock_case(flags, ln, base=0x0010A000):
+    """0x1E75C: the handle at 0x10AA00 (base +8, len +0xC, bits +0x15) with its neighbours."""
+    return {0x0010AA00: b"\x11" * 8 + le32(base) + le32(ln) + b"\x22\x33\x44\x55\x66"
+            + bytes([flags]) + b"\x99\x77"}
+
+
+def c3f_unlock_case(flags, clock=0x11223344):
+    """0x1E808: the handle at 0x10AA00 and the DPMI clock shadow 0x101500."""
+    return {0x00101500: le32(clock),
+            0x0010AA00: b"\x11" * 8 + le32(0x0010A000) + le32(0x40) + le32(0x77777777)
+            + b"\x88\x66" + bytes([flags]) + b"\x99\x77"}
+
+
+def c3f_anim_case(side, s54, s43, ch, rec=C3D_REC1):
+    """0x1A6AC: the slot's +0x54/+0x43/+0x7A, the stream tables 0xC8F40/0xC8F90 and the rec
+    argument's +0x51 (fighter_ctx_rec_swap's side)."""
+    p = c3d_slot_pokes()
+    so = 0x001077B0 + side * 0x94
+    p[so + 0x54] = bytes([s54])
+    p[so + 0x43] = bytes([s43])
+    p[so + 0x7A] = bytes([ch])
+    p[rec + 0x51] = bytes([side])
+    p[0x000C8F40 + ch * 4] = le32(0x11111111)
+    p[0x000C8F90 + ch * 4] = le32(0x22222222)
+    return p
+
+
+def c3f_chain_case(side, s5f, s53, s63, s7c=0, s55=0, ch=0):
+    """0x3CF38: the side slot's +0x5F/+0x53/+0x63/+0x7C/+0x55/+0x7A."""
+    p = c3d_slot_pokes()
+    so = 0x001077B0 + side * 0x94
+    p[so + 0x5F] = bytes([s5f])
+    p[so + 0x53] = bytes([s53])
+    p[so + 0x63] = bytes([s63])
+    p[so + 0x7C] = bytes([s7c])
+    p[so + 0x55] = bytes([s55])
+    p[so + 0x7A] = bytes([ch])
+    return p
+
+
+def c3f_str_case(groups, outlen=0x100, base=0x0010B000, handle=0x0010AA00):
+    """0x474E4: the localisation table at `base`. `groups` is a list of lists of plaintext
+    entries; group i>0 sits at an 8-byte block (its chain link at +4) and every entry is a length
+    byte followed by its bytes XORed with that length. The handle names base; the out buffer at
+    0x10A200 is seeded 0x5A so a dropped or short write is visible."""
+    buf = bytearray(8)                                  # base+0..+7 (group 0's entries at +8)
+    offs = []
+    for gi, g in enumerate(groups):
+        if gi > 0:
+            offs.append(len(buf))
+            buf.extend(b"\x00" * 8)
+        for e in g:
+            buf.append(len(e) & 0xFF)
+            buf.extend(bytes(b ^ (len(e) & 0xFF) for b in e))
+    for i in range(1, len(offs)):
+        buf[offs[i] + 4:offs[i] + 8] = le32(offs[i + 1] if i + 1 < len(offs) else 0)
+    if offs:
+        buf[4:8] = le32(offs[0])
+    return {0x001082DC: le32(handle),
+            handle: b"\x00" * 8 + le32(base) + le32(0x400) + b"\x00\x00\x00\x00\x00"
+            + b"\x00\x00\x00",
+            base: bytes(buf), 0x0010A200: bytes([0x5A]) * 0x40}
+
+
+C3F_SPECS = [
+    # 0x38BB0 fighter_38bb0: EAX = side; clear 0x107A80 + side*0x40 .. +0x3F. EAX is dead.
+    Spec("fighter_38bb0", 0x38BB0, [
+        Case("b0", {"eax": 0}, c3f_38b_case()),
+        Case("b1", {"eax": 1}, c3f_38b_case()),
+    ], eax_mask=0, mutants=("@stride", "@len", "@off")),
+    # 0x38BC8 fighter_38bc8: EAX = side; 0x38BB0 plus DSW(0x107D24 + side*2) = 0.
+    Spec("fighter_38bc8", 0x38BC8, [
+        Case("b0", {"eax": 0}, c3f_38b_case()),
+        Case("b1", {"eax": 1}, c3f_38b_case()),
+    ], allow_calls=(0x38BB0,), eax_mask=0, mutants=("@word", "@off", "@clear")),
+    # 0x3B038 fighter_3b038: EAX = side; 1 when slot+0x2C is in the window about 0xBE018.
+    Spec("fighter_3b038", 0x3B038, [
+        Case("w0", {"eax": 0}, c3f_b038_case(0, 0, 0xFF00, 0xFFFFFF00)),   # arm 2 boundary
+        Case("w1", {"eax": 0}, c3f_b038_case(0, 0, 0xFF00, 0xFFFFFF01)),   # arm 2 only
+        Case("w2", {"eax": 0}, c3f_b038_case(0, 0, 0xFF00, 0x00000000)),   # both miss
+        Case("w3", {"eax": 0}, c3f_b038_case(0, 0, 0xFF00, 0x00000100)),   # arm 1 boundary
+        Case("w4", {"eax": 0}, c3f_b038_case(0, 0, 0xFF00, 0x00000101)),   # arm 1
+        Case("w5", {"eax": 0}, c3f_b038_case(0, 0x1000, 0x40, 0x0FBF)),    # positive k: miss
+        Case("w6", {"eax": 1}, c3f_b038_case(1, 0, 0xFF00, 0x00000000)),   # side 1: both miss
+        Case("w7", {"eax": 1}, c3f_b038_case(1, 0, 0xFF00, 0x00000100)),   # side 1: arm 1
+    ], eax_mask=0xFF, mutants=("@s1", "@s2", "@sum")),
+    # 0x46534 fighter_46534: EAX = side, EDX = delta; add to 0x1082C8[side], clamp to the
+    # 0xC9408 cap down, to 0 up, then to the 0x1082D0 floor. EAX is dead.
+    Spec("fighter_46534", 0x46534, [
+        Case("v0", {"eax": 0, "edx": 5}, c3f_46534_case(0, 0x10, 0)),      # 0x15
+        Case("v1", {"eax": 0, "edx": 0x30}, c3f_46534_case(0, 0x10, 0)),   # cap 0x40: 0x40
+        Case("v2", {"eax": 0, "edx": 0xFFFFFFF6}, c3f_46534_case(0, 0x10, 0)),  # -10 -> 6
+        Case("v3", {"eax": 0, "edx": 0xFFFFFFEF}, c3f_46534_case(0, 0x10, 0)),  # -17 -> 0
+        Case("v4", {"eax": 0, "edx": 5}, c3f_46534_case(0, 0x10, 0x30)),   # floor 0x30
+        Case("v5", {"eax": 0, "edx": 0x20}, c3f_46534_case(0, 0x3E, 0)),   # cap 0x40: 0x40
+        Case("v6", {"eax": 0, "edx": 2}, c3f_46534_case(0, 0x10, 0, capidx=3, cap=0x20)),
+        Case("v7", {"eax": 1, "edx": 5}, c3f_46534_case(1, 0x10, 0)),      # the other side
+        Case("v8", {"eax": 0, "edx": 0}, c3f_46534_case(0, 0x10, 0x50)),   # floor > cap
+        Case("v9", {"eax": 0, "edx": 0xFFFFFFE0}, c3f_46534_case(0, 0x10, 0)),  # negative
+    ], eax_mask=0, mutants=("@cap", "@sign", "@floor", "@order")),
+    # 0x3BDB0 fight_attack_ready: EAX = side; AL = 1 when slot+0x53 == 0 and +0x54 != 2.
+    Spec("fight_attack_ready", 0x3BDB0, [
+        Case("r0", {"eax": 0}, c3f_ready_case(0, 0, 0)),
+        Case("r1", {"eax": 0}, c3f_ready_case(0, 0, 2)),
+        Case("r2", {"eax": 0}, c3f_ready_case(0, 1, 0)),
+        Case("r3", {"eax": 0}, c3f_ready_case(0, 0, 3)),
+        Case("r4", {"eax": 0}, c3f_ready_case(0, 0x80, 1)),
+        Case("r5", {"eax": 1}, c3f_ready_case(1, 0, 2)),
+        Case("r6", {"eax": 1}, c3f_ready_case(1, 0, 0)),
+    ], allow_calls=(0x33950,), eax_mask=0xFF, mutants=("@s53", "@s54", "@side")),
+    # 0x4649C fighter_input_scan: EAX = side, EDX = n1, EBX = n2, ECX = mask; skip n1 ring
+    # entries, scan n2 from there, 1 on the first word whose low 16 bits overlap the mask.
+    Spec("fighter_input_scan", 0x4649C, [
+        Case("s0", {"eax": 0, "edx": 0, "ebx": 0, "ecx": 0xFFFF}, c3f_scan_case(0, 3, {})),
+        Case("s1", {"eax": 0, "edx": 0, "ebx": 1, "ecx": 0x0001},
+             c3f_scan_case(0, 3, {3: 0x0001})),                            # the first word hits
+        Case("s2", {"eax": 0, "edx": 0, "ebx": 1, "ecx": 0x0002},
+             c3f_scan_case(0, 3, {3: 0x0001})),                            # the mask misses
+        Case("s3", {"eax": 0, "edx": 2, "ebx": 2, "ecx": 0x0004},
+             c3f_scan_case(0, 3, {3: 0x0008, 2: 0x0004})),                 # hit after the skip
+        Case("s4", {"eax": 0, "edx": 2, "ebx": 2, "ecx": 0x0010},
+             c3f_scan_case(0, 3, {3: 0x0008, 2: 0x0004})),                 # miss
+        Case("s5", {"eax": 0, "edx": 2, "ebx": 1, "ecx": 0x0004},
+             c3f_scan_case(0, 0, {0x12: 0x0004, 0x13: 0x0000})),           # skip wraps 0 -> 0x13
+        Case("s6", {"eax": 0, "edx": 0, "ebx": 2, "ecx": 0x0008},
+             c3f_scan_case(0, 0, {0x13: 0x0008})),                         # scan wraps 0 -> 0x13
+        Case("s7", {"eax": 0, "edx": 1, "ebx": 3, "ecx": 0x0100},
+             c3f_scan_case(0, 1, {0: 0x0100})),                            # second word hits
+        Case("s8", {"eax": 1, "edx": 0, "ebx": 2, "ecx": 0x0001},
+             c3f_scan_case(1, 2, {2: 0x0001})),
+        Case("s9", {"eax": 1, "edx": 0, "ebx": 1, "ecx": 0x8000},
+             c3f_scan_case(1, 0x13, {0x13: 0x8000})),
+    ], eax_mask=0xFF, mutants=("@n1", "@n2", "@mask", "@wrap")),
+    # 0x1AB10 fighter_state_ok: EAX = side; 1 when the side slot's +0x54 and +0x53 are both <= 1
+    # (unsigned, `test al,al`/`jbe`). 0x33A10 is an allow (leaf).
+    Spec("fighter_state_ok", 0x1AB10, [
+        Case("o0", {"eax": 0}, c3f_ok_case(0, 0, 0)),
+        Case("o1", {"eax": 0}, c3f_ok_case(0, 1, 1)),
+        Case("o2", {"eax": 0}, c3f_ok_case(0, 2, 0)),
+        Case("o3", {"eax": 0}, c3f_ok_case(0, 0, 2)),
+        Case("o4", {"eax": 0}, c3f_ok_case(0, 0x80, 0)),
+        Case("o5", {"eax": 0}, c3f_ok_case(0, 0, 0xFF)),
+        Case("o6", {"eax": 1}, c3f_ok_case(1, 0, 0)),
+        Case("o7", {"eax": 1}, c3f_ok_case(1, 2, 0)),
+    ], allow_calls=(0x33A10,), eax_mask=0xFF, mutants=("@s54", "@s53", "@sign")),
+    # 0x1E75C string_lock: EAX = the handle; returns its +8 base when +0x15 bit 0 is clear and
+    # +0xC != 0 (setting bit 1), else 0.
+    Spec("string_lock", 0x1E75C, [
+        Case("l0", {"eax": 0x10AA00}, c3f_lock_case(0x00, 0x40)),
+        Case("l1", {"eax": 0x10AA00}, c3f_lock_case(0x01, 0x40)),
+        Case("l2", {"eax": 0x10AA00}, c3f_lock_case(0x00, 0)),
+        Case("l3", {"eax": 0x10AA00}, c3f_lock_case(0xFE, 0x40)),
+        Case("l4", {"eax": 0x10AA00}, c3f_lock_case(0x80, 0x40)),
+    ], eax_mask=0xFFFFFFFF, mutants=("@bit", "@len", "@base", "@or")),
+    # 0x1E808 string_unlock: EAX = the handle; clears +0x15 bit 1, then the 0x500BB clock read
+    # stored at +0x10 (the raw `mov eax,[0x101500]; ret` body is an allow).
+    Spec("string_unlock", 0x1E808, [
+        Case("u0", {"eax": 0x10AA00}, c3f_unlock_case(0xFF)),
+        Case("u1", {"eax": 0x10AA00}, c3f_unlock_case(0x00)),
+        Case("u2", {"eax": 0x10AA00}, c3f_unlock_case(0x02, clock=0xDEADBEEF)),
+    ], allow_calls=(0x500BB,), eax_mask=0, mutants=("@and", "@off", "@store", "@clock")),
+    # 0x474E4 string_decode: EAX = id, EDX = out, EBX = outlen. The handle at 0x1082DC names the
+    # table base; 0x1E75C/0x1E808 are stubs, so each case chooses the base through the lock stub.
+    Spec("string_decode", 0x474E4, [
+        Case("n0", {"eax": 0, "edx": 0x10A200, "ebx": 0x100},
+             c3f_str_case([[b"HELLO"]]), {0x1E75C: 0x0010B000}),          # len 5, ret 6
+        Case("n1", {"eax": 3, "edx": 0x10A200, "ebx": 0x100},
+             c3f_str_case([[b"aa", b"bbb", b"c", b"ABCD"]]), {0x1E75C: 0x0010B000}),
+        Case("n2", {"eax": 0x40, "edx": 0x10A200, "ebx": 0x100},
+             c3f_str_case([[b"first"], [b"WORLD"]]), {0x1E75C: 0x0010B000}),
+        Case("n3", {"eax": 0x41, "edx": 0x10A200, "ebx": 0x100},
+             c3f_str_case([[b"first"], [b"aa", b"WORLD"]]), {0x1E75C: 0x0010B000}),
+        Case("n4", {"eax": 0, "edx": 0x10A200, "ebx": 4},
+             c3f_str_case([[b"HELLO"]]), {0x1E75C: 0x0010B000}),          # truncated: ret 4
+        Case("n4b", {"eax": 0, "edx": 0x10A200, "ebx": 5},
+             c3f_str_case([[b"HELLO"]]), {0x1E75C: 0x0010B000}),          # len == outlen
+        Case("n5", {"eax": 0, "edx": 0x10A200, "ebx": 0x100},
+             c3f_str_case([[b""]]), {0x1E75C: 0x0010B000}),               # len 0: ret 0
+        Case("n6", {"eax": 0x80, "edx": 0x10A200, "ebx": 0x100},
+             c3f_str_case([[b"g0"], [b"g1"], [b"THIRD"]]), {0x1E75C: 0x0010B000}),
+    ], calls=(E.Call(0x1E75C, ("eax",), mode="stub"), E.Call(0x1E808, ("eax",), mode="stub")),
+       eax_mask=0xFFFFFFFF, mutants=("@xor", "@trunc", "@term", "@skip", "@off", "@ret", "@link")),
+    # 0x1C500 game_string_get: EAX = id; calls 0x474E4(EDX = 0x102760, EBX = 0x100) and zeroes the
+    # buffer's first byte when the return is 0; returns the buffer address (EAX always 0x102760).
+    Spec("game_string_get", 0x1C500, [
+        Case("c0", {"eax": 0}, c3f_str_case([]), {0x474E4: 6}),
+        Case("c1", {"eax": 7}, c3f_str_case([]), {0x474E4: 0}),
+        Case("c2", {"eax": 0x40}, c3f_str_case([]), {0x474E4: 0x100}),
+    ], calls=(E.Call(0x474E4, ("eax", "edx", "ebx"), mode="stub",
+                     clobbers=("ebx", "edx"), writes=((1, 0, b"HELLO\x00"),)),),
+       eax_mask=0xFFFFFFFF, mutants=("@zero", "@id", "@ret", "@call")),
+    # 0x1A6AC fighter_block_anim: EAX = slot, EDX = rec. 0x33A68 runs on both sides (allow, the
+    # port's ctx builder); 0x18B04 (the facing flag) and 0x3C480 (the animation start) are stubs.
+    Spec("fighter_block_anim", 0x1A6AC, [
+        Case("a0", {"eax": 0x001077B0, "edx": C3D_REC1}, c3f_anim_case(0, 0, 0x00, 3)),
+        Case("a1", {"eax": 0x001077B0, "edx": C3D_REC1}, c3f_anim_case(0, 0, 0x20, 3)),
+        Case("a2", {"eax": 0x001077B0, "edx": C3D_REC1}, c3f_anim_case(0, 1, 0x00, 3)),
+        Case("a3", {"eax": 0x001077B0, "edx": C3D_REC1}, c3f_anim_case(0, 1, 0x10, 3)),
+        Case("a4", {"eax": 0x001077B0, "edx": C3D_REC1}, c3f_anim_case(0, 2, 0x00, 3)),
+        Case("a5", {"eax": 0x001077B0, "edx": C3D_REC1}, c3f_anim_case(0, 0xFF, 0x00, 3)),
+        Case("a6", {"eax": 0x001077B0, "edx": C3D_REC1}, c3f_anim_case(0, 0x80, 0x00, 3)),
+        Case("a7", {"eax": 0x001077B0 + 0x94, "edx": C3D_REC1}, c3f_anim_case(1, 0, 0x00, 5)),
+        Case("a8", {"eax": 0x001077B0 + 0x94, "edx": C3D_REC1}, c3f_anim_case(1, 1, 0x10, 5)),
+        Case("a9", {"eax": 0x001077B0, "edx": C3D_REC1}, c3f_anim_case(0, 0, 0x40, 3)),
+        Case("a10", {"eax": 0x001077B0, "edx": C3D_REC1}, c3f_anim_case(0, 0, 0x80, 3)),
+    ], allow_calls=(0x33A68,),
+       calls=(E.Call(0x18B04, ("eax",), mode="stub"),
+              E.Call(0x3C480, ("eax", "edx", "s0"), mode="stub", pop=4, clobbers=("edx",))),
+       eax_mask=0, mutants=("@s54", "@bit", "@mask", "@stream", "@call", "@rec")),
+    # 0x3CF38 hit_chain_resolve: EAX = side. 0x3CD44 (scan), 0x3CE58 (drive) and 0x3C6A8 (seed)
+    # are stubs; 0x32BAC is a one-byte RET the port's hit_sound no-ops (allow).
+    Spec("hit_chain_resolve", 0x3CF38, [
+        Case("h0", {"eax": 0}, c3f_chain_case(0, 0, 0, 0), {0x3CD44: 0xFFFFFFFF}),  # scan -1
+        Case("h1", {"eax": 0}, c3f_chain_case(0, 0, 1, 0), {0x3CD44: 5, 0x3CE58: 0},),
+        Case("h2", {"eax": 0}, c3f_chain_case(0, 0, 0, 0), {0x3CD44: 5, 0x3CE58: 0},),
+        Case("h3", {"eax": 0}, c3f_chain_case(0, 0, 0, 0), {0x3CD44: 5, 0x3CE58: 1},),
+        Case("h4", {"eax": 0}, c3f_chain_case(0, 0, 0, 1), {0x3CD44: 5, 0x3CE58: 1},),
+        Case("h5", {"eax": 0}, c3f_chain_case(0, 0x10, 0, 0), {0x3CD44: 0x1C, 0x3CE58: 1},),
+        Case("h6", {"eax": 0}, c3f_chain_case(0, 0x10, 0, 0), {0x3CD44: 0x1B, 0x3CE58: 1},),
+        Case("h7", {"eax": 0}, c3f_chain_case(0, 0x17, 0, 0), {0x3CD44: 0x1C, 0x3CE58: 1},),
+        Case("h8", {"eax": 0}, c3f_chain_case(0, 0x18, 0, 0), {0x3CD44: 0x1C, 0x3CE58: 1},),
+        Case("h9", {"eax": 1}, c3f_chain_case(1, 0, 0, 0, s7c=7, s55=9, ch=2),
+             {0x3CD44: 3, 0x3CE58: 1},),
+        Case("h10", {"eax": 1}, c3f_chain_case(1, 0, 0, 0),
+             {0x3CD44: 3, 0x3CE58: 0},),
+    ],
+       calls=(E.Call(0x3CD44, ("eax",), mode="stub"),
+              E.Call(0x3CE58, ("eax", "edx"), mode="stub", clobbers=("edx", "edi", "ebp")),
+              E.Call(0x3C6A8, ("eax", "edx", "ebx"), mode="stub", clobbers=("edx",)),
+              E.Call(0x32BAC, ("eax",), mode="stub")),
+       eax_mask=0xFF, mutants=("@guard", "@drive", "@seed", "@sound", "@s5f", "@s55", "@inc",
+                               "@ret")),
+]
+
+
 SPECS = [
     Spec("rng_next", 0x5D7DC, [
         Case("r1", {"eax": 0x1234}, {DS_RNG: le32(0x12345678)}),
@@ -6529,7 +6817,7 @@ SPECS = [
         Case("d0", {}, {DS_1078FC: b"\x00"}),
         Case("d1", {"eax": U6_REC}, {DS_1078FC: b"\x5a"}),
     ], eax_mask=0),
-] + E3_SPECS + P1_SPECS + P1_ANIM_SPECS + P2_SPECS + P3_SPECS + P6_SPECS + P45_SPECS + C1_SPECS + C2_SPECS + C2B_SPECS + C3_SPECS + C3B_SPECS + P7_SPECS + P8_SPECS + C3C_SPECS + C3D_SPECS + C3E_SPECS
+] + E3_SPECS + P1_SPECS + P1_ANIM_SPECS + P2_SPECS + P3_SPECS + P6_SPECS + P45_SPECS + C1_SPECS + C2_SPECS + C2B_SPECS + C3_SPECS + C3B_SPECS + P7_SPECS + P8_SPECS + C3C_SPECS + C3D_SPECS + C3E_SPECS + C3F_SPECS
 
 
 # ---- driver -------------------------------------------------------------------------------------
diff --git a/tools/tests/test_diff_verify.py b/tools/tests/test_diff_verify.py
index 507fddc..a0459af 100644
--- a/tools/tests/test_diff_verify.py
+++ b/tools/tests/test_diff_verify.py
@@ -1486,6 +1486,80 @@ C3E_KINDS = {
     "fighter_reaction_apply@u2": {'byte', 'call #3', 'call #4'},
     "fighter_reaction_apply@voice": {'call #2', 'call #3', 'call #3 memory', 'call #4'},
 }
+C3F_MASKS = {
+    "fighter_38bb0": 0x0,
+    "fighter_38bc8": 0x0,
+    "fighter_3b038": 0xff,
+    "fighter_46534": 0x0,
+    "fight_attack_ready": 0xff,
+    "fighter_input_scan": 0xff,
+    "fighter_state_ok": 0xff,
+    "string_lock": 0xffffffff,
+    "string_unlock": 0x0,
+    "string_decode": 0xffffffff,
+    "game_string_get": 0xffffffff,
+    "fighter_block_anim": 0x0,
+    "hit_chain_resolve": 0xff,
+}
+C3F_KINDS = {
+    "fight_attack_ready@s53": {'eax'},
+    "fight_attack_ready@s54": {'eax'},
+    "fight_attack_ready@side": {'eax'},
+    "fighter_38bb0@len": {'byte'},
+    "fighter_38bb0@off": {'byte'},
+    "fighter_38bb0@stride": {'byte'},
+    "fighter_38bc8@clear": {'byte'},
+    "fighter_38bc8@off": {'byte'},
+    "fighter_38bc8@word": {'byte'},
+    "fighter_3b038@s1": {'eax'},
+    "fighter_3b038@s2": {'eax'},
+    "fighter_3b038@sum": {'eax'},
+    "fighter_46534@cap": {'byte'},
+    "fighter_46534@floor": {'byte'},
+    "fighter_46534@order": {'byte'},
+    "fighter_46534@sign": {'byte'},
+    "fighter_block_anim@bit": {'call #1'},
+    "fighter_block_anim@call": {'call #1'},
+    "fighter_block_anim@mask": {'byte'},
+    "fighter_block_anim@rec": {'call #1'},
+    "fighter_block_anim@s54": {'byte', 'call #1'},
+    "fighter_block_anim@stream": {'call #1'},
+    "fighter_input_scan@mask": {'eax'},
+    "fighter_input_scan@n1": {'eax'},
+    "fighter_input_scan@n2": {'eax'},
+    "fighter_input_scan@wrap": {'eax'},
+    "fighter_state_ok@s53": {'eax'},
+    "fighter_state_ok@s54": {'eax'},
+    "fighter_state_ok@sign": {'eax'},
+    "game_string_get@call": {'byte', 'call #0'},
+    "game_string_get@id": {'call #0'},
+    "game_string_get@ret": {'eax'},
+    "game_string_get@zero": {'byte'},
+    "hit_chain_resolve@drive": {'byte', 'call #2', 'call #3', 'eax'},
+    "hit_chain_resolve@guard": {'byte', 'call #1', 'call #2', 'call #3', 'eax'},
+    "hit_chain_resolve@inc": {'byte', 'call #2 memory', 'call #3 memory'},
+    "hit_chain_resolve@ret": {'eax'},
+    "hit_chain_resolve@s55": {'byte'},
+    "hit_chain_resolve@s5f": {'byte'},
+    "hit_chain_resolve@seed": {'call #2', 'call #3'},
+    "hit_chain_resolve@sound": {'call #3'},
+    "string_decode@link": {'byte', 'call #1 memory', 'eax'},
+    "string_decode@off": {'byte', 'call #1 memory', 'eax'},
+    "string_decode@ret": {'eax'},
+    "string_decode@skip": {'byte', 'call #1 memory', 'eax'},
+    "string_decode@term": {'byte', 'call #1 memory'},
+    "string_decode@trunc": {'byte', 'call #1 memory', 'eax'},
+    "string_decode@xor": {'byte', 'call #1 memory'},
+    "string_lock@base": {'byte', 'eax'},
+    "string_lock@bit": {'byte', 'eax'},
+    "string_lock@len": {'byte', 'eax'},
+    "string_lock@or": {'byte'},
+    "string_unlock@and": {'byte'},
+    "string_unlock@clock": {'byte'},
+    "string_unlock@off": {'byte'},
+    "string_unlock@store": {'byte'},
+}
+
 
 @needs_unicorn
 @unittest.skipUnless((os.path.exists(DIFFRUN) and os.path.exists(EXE)) or REQUIRED,
@@ -1511,7 +1585,7 @@ class RealFunctionTests(unittest.TestCase):
                                              "fighter_slot_flag", "hit_anim_ctx", "hit_anim_start_b",
                                              "host_1b890", "rng_next"] + list(P1_MASKS) + list(P2_MASKS)
                                             + list(P3_MASKS) + list(P45_MASKS) + list(C1_MASKS)
-                                            + list(P6_MASKS) + list(C2_MASKS) + list(C2B_MASKS) + list(C3_MASKS) + list(C3B_MASKS) + list(C3C_MASKS) + list(C3D_MASKS) + list(C3E_MASKS) + list(P7_MASKS) + list(P8_MASKS)))
+                                            + list(P6_MASKS) + list(C2_MASKS) + list(C2B_MASKS) + list(C3_MASKS) + list(C3B_MASKS) + list(C3C_MASKS) + list(C3D_MASKS) + list(C3E_MASKS) + list(C3F_MASKS) + list(P7_MASKS) + list(P8_MASKS)))
         for name, r in self.real.items():
             if name == "host_1b890":       # the named gap (record E3 §E3.8), tested on its own below
                 continue
@@ -1527,7 +1601,7 @@ class RealFunctionTests(unittest.TestCase):
             "fighter_45878@mutant", "fighter_ctx_same@mutant", "fighter_slot_flag@mutant",
             "hit_anim_ctx@mutant", "hit_anim_start_b@mutant", "hit_anim_start_b@set", "rng_next@mutant"]
             + list(P1_KINDS) + list(P2_KINDS) + list(P3_KINDS) + list(P45_KINDS) + list(C1_KINDS)
-            + list(P6_KINDS) + list(C2_KINDS) + list(C2B_KINDS) + list(C3_KINDS) + list(C3B_KINDS) + list(C3C_KINDS) + list(C3D_KINDS) + list(C3E_KINDS) + list(P7_KINDS) + list(P8_KINDS)))
+            + list(P6_KINDS) + list(C2_KINDS) + list(C2B_KINDS) + list(C3_KINDS) + list(C3B_KINDS) + list(C3C_KINDS) + list(C3D_KINDS) + list(C3E_KINDS) + list(C3F_KINDS) + list(P7_KINDS) + list(P8_KINDS)))
         for name, r in self.mut.items():
             self.assertEqual(r.verdict, "MISMATCH", name)
 
@@ -1592,7 +1666,7 @@ class RealFunctionTests(unittest.TestCase):
             "fighter_3640c": 0, "fighter_37dcc": 0,
             "fighter_23130": 0xFF, "fighter_45878": 0, "anim_10fa8": 0, "anim_3e4e4": 0,
             "fighter_ctx_same": 0, "hit_anim_ctx": 0, "hit_anim_start_b": 0, "host_1b890": 0xFFFFFFFF,
-            **P1_MASKS, **P2_MASKS, **P3_MASKS, **P45_MASKS, **C1_MASKS, **P6_MASKS, **C2_MASKS, **C2B_MASKS, **C3_MASKS, **C3B_MASKS, **C3C_MASKS, **C3D_MASKS, **C3E_MASKS,
+            **P1_MASKS, **P2_MASKS, **P3_MASKS, **P45_MASKS, **C1_MASKS, **P6_MASKS, **C2_MASKS, **C2B_MASKS, **C3_MASKS, **C3B_MASKS, **C3C_MASKS, **C3D_MASKS, **C3E_MASKS, **C3F_MASKS,
             **P7_MASKS, **P8_MASKS})
         # with the full mask the slot-flag original's scratch bits (case f9: EAX = 0x201) differ
         spec = dataclasses.replace([s for s in V.SPECS if s.name == "fighter_slot_flag"][0],
@@ -2439,6 +2513,74 @@ class RealFunctionTests(unittest.TestCase):
                    else p.split(": ", 1)[1].split(" ")[0] for p in self.mut[name].problems}
             self.assertEqual(got, want, name)
 
+
+    def test_each_c3f_mutant_is_caught_by_what_it_breaks(self):
+        # track P batch C3f (record 2026-10-05-reverse-c3f): what alone catches each mutant
+        for name, want in C3F_KINDS.items():
+            got = {p.split(": ", 1)[1].split(":")[0] if p.split(": ", 1)[1].startswith("call #")
+                   else p.split(": ", 1)[1].split(" ")[0] for p in self.mut[name].problems}
+            self.assertEqual(got, want, name)
+        # the exact case set that alone catches each mutant (measured on the prototype)
+        for name, ids in (
+        ("fight_attack_ready@s53", ['r2', 'r3']),
+        ("fight_attack_ready@s54", ['r0', 'r1', 'r3', 'r5', 'r6']),
+        ("fight_attack_ready@side", ['r1', 'r2', 'r4', 'r5']),
+        ("fighter_38bb0@len", ['b0', 'b1']),
+        ("fighter_38bb0@off", ['b0', 'b1']),
+        ("fighter_38bb0@stride", ['b1']),
+        ("fighter_38bc8@clear", ['b0', 'b1']),
+        ("fighter_38bc8@off", ['b1']),
+        ("fighter_38bc8@word", ['b0']),
+        ("fighter_3b038@s1", ['w3', 'w7']),
+        ("fighter_3b038@s2", ['w1', 'w2', 'w5', 'w6']),
+        ("fighter_3b038@sum", ['w1', 'w2', 'w6']),
+        ("fighter_46534@cap", ['v0', 'v1', 'v2', 'v5', 'v6', 'v7']),
+        ("fighter_46534@floor", ['v4', 'v8']),
+        ("fighter_46534@order", ['v8']),
+        ("fighter_46534@sign", ['v3', 'v9']),
+        ("fighter_block_anim@bit", ['a1']),
+        ("fighter_block_anim@call", ['a0', 'a10', 'a2', 'a7', 'a9']),
+        ("fighter_block_anim@mask", ['a10', 'a9']),
+        ("fighter_block_anim@rec", ['a0', 'a10', 'a9']),
+        ("fighter_block_anim@s54", ['a2', 'a3', 'a4', 'a5', 'a6', 'a8']),
+        ("fighter_block_anim@stream", ['a0', 'a10', 'a7', 'a9']),
+        ("fighter_input_scan@mask", ['s1', 's4', 's5', 's6', 's7', 's8', 's9']),
+        ("fighter_input_scan@n1", ['s5']),
+        ("fighter_input_scan@n2", ['s6']),
+        ("fighter_input_scan@wrap", ['s6']),
+        ("fighter_state_ok@s53", ['o3', 'o5']),
+        ("fighter_state_ok@s54", ['o2', 'o4', 'o7']),
+        ("fighter_state_ok@sign", ['o4', 'o5']),
+        ("game_string_get@call", ['c0', 'c1', 'c2']),
+        ("game_string_get@id", ['c0', 'c1', 'c2']),
+        ("game_string_get@ret", ['c0', 'c1', 'c2']),
+        ("game_string_get@zero", ['c1']),
+        ("hit_chain_resolve@drive", ['h1', 'h10', 'h2', 'h3', 'h4', 'h6', 'h8', 'h9']),
+        ("hit_chain_resolve@guard", ['h5', 'h7']),
+        ("hit_chain_resolve@inc", ['h3', 'h4', 'h6', 'h8', 'h9']),
+        ("hit_chain_resolve@ret", ['h1', 'h10', 'h2']),
+        ("hit_chain_resolve@s55", ['h1', 'h10', 'h2']),
+        ("hit_chain_resolve@s5f", ['h10', 'h2']),
+        ("hit_chain_resolve@seed", ['h3', 'h4', 'h6', 'h8', 'h9']),
+        ("hit_chain_resolve@sound", ['h3', 'h4', 'h6', 'h8', 'h9']),
+        ("string_decode@link", ['n2', 'n3']),
+        ("string_decode@off", ['n0', 'n1', 'n2', 'n3', 'n4', 'n4b', 'n6']),
+        ("string_decode@ret", ['n0', 'n1', 'n2', 'n3', 'n6']),
+        ("string_decode@skip", ['n1', 'n3']),
+        ("string_decode@term", ['n0', 'n1', 'n2', 'n3', 'n5', 'n6']),
+        ("string_decode@trunc", ['n4b']),
+        ("string_decode@xor", ['n0', 'n1', 'n2', 'n3', 'n6']),
+        ("string_lock@base", ['l0', 'l2', 'l3', 'l4']),
+        ("string_lock@bit", ['l0', 'l3', 'l4']),
+        ("string_lock@len", ['l1', 'l3']),
+        ("string_lock@or", ['l0', 'l4']),
+        ("string_unlock@and", ['u0', 'u1', 'u2']),
+        ("string_unlock@clock", ['u0', 'u1', 'u2']),
+        ("string_unlock@off", ['u0', 'u1', 'u2']),
+        ("string_unlock@store", ['u0', 'u1', 'u2']),
+        ):
+            self.assertEqual(sorted({p.split(":")[0] for p in self.mut[name].problems}), ids, name)
+
     def test_each_stub_declares_the_registers_its_callee_clobbers(self):
         # Call.clobbers, re-derived from the bytes (record §E3.5's table, §E3.12)
         img = E.Image.load(os.path.join(self.tmp.name, "image.bin"))
@@ -2452,7 +2594,7 @@ class RealFunctionTests(unittest.TestCase):
         0x188AC: ("edx",), 0x188DC: ("edx",), 0x1890C: ("edx",), 0x189FC: (), 0x18A4C: (),
         0x18AF8: ("ebx", "ecx", "edx"), 0x18B04: (), 0x18B44: (), 0x18C14: ("ebx", "edx", "ebp"), 0x1A570: (),
         0x1A5AC: (), 0x1A734: (), 0x1A7CC: ("esi", "edi", "ebp"), 0x1AB5C: ("ebp",), 0x1CA14: ("edx",),
-        0x1CA40: (), 0x1CA6C: (), 0x1CC28: ("edx",), 0x1CD9C: (), 0x1CE04: (), 0x1CE70: (), 0x1D238: (),
+        0x1CA40: (), 0x1CA6C: (), 0x1CC28: ("edx",), 0x1CD9C: (), 0x1CE04: (), 0x1CE70: (), 0x1D238: (), 0x1E75C: (), 0x1E808: (),
         0x1D244: (), 0x1DDF4: ("ebx", "edx"), 0x22404: (), 0x23960: (), 0x249B0: (), 0x249D0: (),
         0x29BC8: ("ebx", "edx"), 0x29C08: ("edx",), 0x29DB8: ("ebx", "edx"), 0x29F34: ("edx",),
         0x2A148: ("edx",), 0x2A17C: ("edx",), 0x2A408: ("edx",), 0x2A620: ("edx",), 0x2A690: (),
@@ -2469,7 +2611,7 @@ class RealFunctionTests(unittest.TestCase):
         0x3B080: ("ebx", "ecx", "edx"), 0x3B134: ("ebx", "edx", "edi", "ebp"), 0x3B298: ("edx",), 0x3B6C4: (),
         0x3B714: ("edx",), 0x3B8D8: ("edx",), 0x3B90C: ("edx",), 0x3BDB0: (), 0x3BDDC: ("ebp",), 0x3C148: (),
         0x3C16C: (), 0x3C190: ("edx",), 0x3C208: ("edx",), 0x3C358: (), 0x3C480: ("edx",), 0x3C4CC: ("edx",),
-        0x3C520: ("edx",), 0x3C59C: ("edx",), 0x41310: (), 0x46190: (), 0x46460: ("edx",), 0x468D8: (), 0x48170: (),
+        0x3C520: ("edx",), 0x3C59C: ("edx",), 0x32BAC: (), 0x3C6A8: ("edx",), 0x3CD44: (), 0x3CE58: ("edx", "edi", "ebp"), 0x41310: (), 0x46190: (), 0x46460: ("edx",), 0x468D8: (), 0x474E4: ("ebx", "edx"), 0x48170: (),
         0x49444: ("edi", "ebp"), 0x4F434: (), 0x5D7DC: (), 0x5DC0F: ("ebx", "ecx", "edx"),
         0x5DC8B: ("ebx", "ecx", "edx"), 0x5DD03: (), 0x5DEAF: ("ebx", "ecx", "edx"), 0x5DEED: ()})
         for addr, declared in stubs.items():
@@ -2565,9 +2707,9 @@ class RealFunctionTests(unittest.TestCase):
             rc = V.main(["--diffrun", DIFFRUN, "--exe", EXE, "--image", os.path.join(self.tmp.name, "a.bin"),
                          "--self-check"])
         self.assertEqual(rc, 0)
-        # the closed-row count is over the rows that have callees (217), the 61 without are counted apart
-        self.assertIn("diff-verify: 278/278 functions VERIFIED; 1031/1031 mutants detected; 1 named gaps; "
-                      "196/217 rows with callees closed (61 have none).", out.getvalue())
+        # the closed-row count is over the rows that have callees (225), the 66 without are counted apart
+        self.assertIn("diff-verify: 291/291 functions VERIFIED; 1087/1087 mutants detected; 1 named gaps; "
+                      "205/225 rows with callees closed (66 have none).", out.getvalue())
 
 
     def test_each_c3c_mutant_is_caught_by_what_it_breaks(self):
```
