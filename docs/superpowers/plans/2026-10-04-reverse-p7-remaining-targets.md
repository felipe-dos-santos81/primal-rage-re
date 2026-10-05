# P7: the remaining unported direct callees and the targets outside E2 (track P, batch 7) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Port the 14 functions of track P's batch 7 (record §P7.1: the direct callees `0x2BDB8 0x23960 0x2BDE8 0x3A9D8` and the animation targets `0x213F0 0x213F4 0x3E424 0x224EC 0x36114 0x23A7C 0x48254 0x23AE0 0x29C78 0x4B03C`, four of them outside E2's universe), each verified against the original's bytes with its callees stubbed (E3 §E3.10), every block hit and every store observable; add the seams of `0x2B150 0x41310 0x49444` (the three ported callees `0x4B03C`'s row stubs); drop `gp-u9-win`'s `0x213F0`/`0x213F4` rows and `gp-u10-ending`'s `0x29C78` row and re-measure their pins; regenerate the E2 table with each port.

**Architecture:** One C function per original function, appended to `port/src/game/fighter.c` after P6's `fighter_24220_case10`, registered in `actors_init` (`port/src/game/actors.c`) through `anim_code_*` wrappers for the ten animation targets. Each new stubbed callee opens with `PR_SEAM`/`PR_SEAM_RET` (`mem.h`): the members `0x2BDB8 0x23960 0x2BDE8 0x3A9D8`, and the already-ported `0x2B150 0x41310 0x49444`; `0x1883C`, `0x22404`, `0x2BC30`, `0x2AE14`, `0x2BCF4`, `0x2A17C`, `0x2C3FC`, `0x39834` already had seams, and `0x33A10`/`0x339AC` run on both sides (allow). Each function has a binding and mutants in `port/tests/diff_runner.c`, a `Spec` in `tools/diff_verify.py` (`P7_SPECS`), seeded unit checks in `port/tests/test_fight.c`, and its row in the self-check counter.

**Tech Stack:** C11 (`port/src`, `port/tests`), Python 3.12 (`unittest`), `unicorn` 2.1.4 and `capstone` 5.0.7 (`tools/requirements-diff.txt`), CMake, `make`.

**Spec:** `docs/superpowers/specs/2026-09-30-reverse-completion-design.md` §3 decision 1, §4 track P, §5.1-§5.3, §6, §7.

**Derivation record:** `docs/superpowers/plans/2026-10-04-reverse-p7-derivations.md` (§P7.1 the 14 members from the raw and the callee decisions, §P7.2-§P7.7 each function from the bytes, §P7.8 the gp drops and re-measures, §P7.9 decisions and named gaps, §P7.10 results, §P7.11 the roadmap after P7). Recipe: `2026-10-01-reverse-e3-derivations.md` §E3.10; lessons: `2026-10-02-reverse-p1-derivations.md` §P1.10-§P1.12, the P2/P3 reviews and P6's record.

**What the planner ran (scratch, 2026-10-04, on `main` `e88eb44`; image sha1 `ff3b8cb14e00f1c282de7b7e15dcd7c230766947`):** the prototype is Tasks 2-7 applied in order; every expected output below is from that tree. The task scripts were then re-derived from the prototype by splitting it at the family boundaries and re-applied from a clean tree, so the scripts below are the ones the planner ran. The planner's store sweep and mutation runs are the `--self-check` counters and `P7_KINDS`; the gp oracles' report lines are quoted verbatim in §P7.8 of the record.

**Re-baseline note.** The counters, E2 lines and gp pins below are the measured `e88eb44` values plus this plan's increments. C2 (a parallel verification-only batch) may merge first; both batches append to `tools/diff_verify.py` (a `P7_SPECS` list next to `C2_SPECS`) and the same binding tables; the rebase conflict is mechanical: keep both sides, regenerate the E2 table, recompute the counters, re-measure the gp pins on the rebased tree. If a merge moves a value, Task 1 records the measured one in the ledger and every later expected counter adds this plan's increments to it: functions +4, +3, +2, +2, +2, +1 (Tasks 2-7); mutants +13, +12, +10, +12, +10, +11; closed rows +2, +1, 0, 0, +1, 0; rows without callees +2, +1, 0, 0, 0, 0; E2 ported targets +2, +2, +1, +1, 0, 0; E2 supplement unported -1, -1, -1, -1, 0, 0.

## Decisions needed from the user

**None.** The member list is the roadmap's 14 (§P7.1); the callee decisions follow the raw (the four members get seams, the three already-ported callees get seams, `0x33A10` is an allow callee as in C1); the gp drops follow the miss-set tables.

## The P-track roadmap

| batch | ports | what |
|---|---|---|
| P1-P6 (merged) | 143 | the finishers, callbacks, animation targets A-C |
| **P7** (this plan) | **14** | the direct callees `0x2BDB8 0x23960 0x2BDE8 0x3A9D8`, the animation targets `0x213F0 0x213F4 0x3E424 0x224EC 0x36114 0x23A7C 0x48254 0x23AE0 0x29C78 0x4B03C` (the last four outside E2) |
| C2 | about 20 rows | verification only: the rest of the stubbed callees (P7 adds `0x2B150 0x41310 0x49444`) |
| P8 | 16 + triage | the rest |

None of P7's 14 is a Ghidra `FN_` function: `port_progress.py` stays `771 1203 64` / `731 731 100` and README does not move.

## Global Constraints

Copied verbatim from their sources:

- AGENTS.md: "**Never ship a fitted constant.** Every value is derived from the raw bytes or a capture, with the address that proves it. A value that cannot be pinned is a **named gap with its evidence** — never a plausible-looking number." / "**On any plan-vs-raw conflict the raw wins.** Record the correction and the address, in the derivation record and the report."
- Spec §8: "`make verify` is the gate after every task." User ruling (2026-10-01, speed-up): the per-task gate is the task's tests, `make diff-verify`, `make entry-triage` and the gp oracles whose miss sets the task touches; the full `make verify` runs at the baseline (Task 1), the final task (Task 8) and before the merge, with the parallel-safe overrides `T=p7; SMK_DUMP=/tmp/pr_${T}_smk TITLE_DUMP=/tmp/pr_${T}_title ATTRACT_DUMP=/tmp/pr_${T}_att FRONTEND_DUMP=/tmp/pr_${T}_fe TITLE_PIN_DIR=/tmp/pr_${T}_pin AUDIO_WAV=/tmp/pr_${T}.wav K11_DUMP=/tmp/pr_${T}_k11 GP_DUMP=/tmp/pr_${T}_gp DIFF_IMAGE=/tmp/pr_${T}_diffimg DIFF_TABLE=/tmp/pr_${T}_diff.md E2_IMAGE=/tmp/pr_${T}_e2.bin`. The 45 oracle lines (`grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|== demo-fight)' <log>`) must equal `.superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt`; the `make audio-render` WAV must be byte-identical to `.superpowers/sdd/2026-09-29-k7-k12/scratch/before-t2.wav`; `python3 tools/port_progress.py` prints `771 1203 64` and `731 731 100`.
- AGENTS.md: "**The E2 target list is a gate** ... A commit that ports a target (a new `/* 0xADDR` header or `fn_register`) regenerates the table in the same commit (decision D3) ... A conflict in that file is resolved by regenerating it after the rebase, never by merging lines by hand."
- AGENTS.md: "One C function per original function, header comment `/* 0xADDR — spec section */`." / "Mark deliberate deviations `/* PORT: ... */`; doubts `/* TODO(verify): ... */`. No other comment styles in `port/src`." / "Code addresses stored in data go through `fn_origin()` / `fn_resolve()`." / "**`port/src/symbols.h` is generated** ... Never hand-edit it."
- AGENTS.md: "A ported function that a differentially verified function stubs or records ... opens with `PR_SEAM(0xADDR, args...)` (void) or `PR_SEAM_RET(0xADDR, args...)` ... The arguments are the C signature's, in order (a pointer into `mem[]` as its offset)." Record E3 §E3.8: "A seam must be the callee's first statement".
- AGENTS.md: "Only `CHECK(cond,msg)` and `CHECK_EQ_INT(a,b)`." / "**Register a test once**" / "**Assertions must be able to fail.** Seed sentinels that differ from the post-conditions, prove a new assertion fails under a mutation of the code it tests". "Consolidating must not change an assertion": this plan **extends** the exact-set assertions of `RealFunctionTests` and changes no other assertion.
- AGENTS.md: "Every env-gated driver and `--check` run arms `fn_resolve`'s miss log and must record exactly the pinned known-set": Task 2 drops `0x213F0`/`0x213F4` from `k_miss_gp_u9_win` and Task 6 drops `0x29C78` from `k_miss_gp_u10_ending`.
- AGENTS.md: "`data/` is git-ignored and read-only — never write to it." / "Commit style: `<area>: <what changed>`. **Never `git add -A`** — stage named files." Never run `make gp-capture`.
- Spec §5.3 / E3 §E3.4: "VERIFIED" is equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.

## Review Focus

1. **An input the rows cannot tell apart.** Every row that reads one of two lookalikes has a case where they differ and a mutant that only that case catches: the other **slot** against its record (`0x213F4`/`0x3E424`'s `@side`, `0x23960`'s `@x`, `0x23AE0`'s `@side`), the own record's char against the slot's (`@char` in `0x36114`/`0x23AE0`), the EAX slot against ctx[2]/ctx[3] (`0x224EC@side`, `0x3A9D8@slot`), the own side's flag against the other's (`0x48254@byte`).
2. **A width or sign read wrong.** Cases each alone catching a mutant: `0x2BDB8@width` (a word store of 0x105BEE), `0x2BDE8@byte`, `0x36114@table` (0xBDA5A for +0x34), `0x36114@neg` (the bit-14 negation), `0x4B03C@sub` (the +0x5A add), `0x4B03C@type` (the type from the slot's +0x20).
3. **A store no case can observe.** Every field a row writes carries a sentinel that differs from what it writes; the `@order` mutants (`0x213F4`, `0x3E424`, `0x224EC`, `0x36114`, `0x23A7C`, `0x48254`, `0x23AE0`, `0x4B03C`) prove the memory at a call is compared.
4. **The registrations and the seams.** Each unit check asserts `fn_resolve(addr) != NULL` (and the differential rows' `b_anim` resolves the wrapper); the new seams are the callees' first statements, and `test_each_stub_declares_the_registers_its_callee_clobbers` re-derives their clobber sets.
5. **The gp oracles.** Task 2 drops the U9 pair and raises TRACE/WIN to 3503; Task 6 drops `0x29C78` with every U10 pin unchanged; Task 8's full gate shows every other gp ratchet and miss set unchanged.

## Where to run

The worktree `.worktrees/reverse-p7` (branch `reverse-p7`, cut from `main` `e88eb44`). Before Task 1:

```bash
cd /Users/felipe.dos.santos/code/mine/primal-rage-reverse/.worktrees/reverse-p7
git status && git log --oneline -1                      # e88eb44
python3 -c "import unicorn, capstone; print(unicorn.__version__, capstone.__version__)"  # 2.1.4 5.0.7
ls port/tests/ghidra_data.bin port/tests/title_screen_ref.ppm   # both must exist (copy from the main checkout if absent)
cmake -S port -B build && cmake --build build 2>&1 | tail -1
./build/diffrun --exe data/game/C/PRAGE.EXE --image-out /tmp/pr_p7_img.bin && shasum /tmp/pr_p7_img.bin
```

The last line must print `ff3b8cb14e00f1c282de7b7e15dcd7c230766947`; another hash means the image differs from the one the record measured: stop. Every command runs from the worktree root. Ledger: `.superpowers/sdd/2026-10-04-reverse-p7-remaining-targets/progress.md`.

**How the code steps are written.** Each task's Step 1 is one `python3 - <<'PY'` script that replaces exact text and asserts the text occurs exactly once (`sub`), so a script either applies cleanly or stops naming the file and the text it could not find. Run each once, from the worktree root, in order. If `main` moved after `e88eb44`, an anchor can move: re-apply that `sub` by hand at the same place, never elsewhere; the E2 table is regenerated, never merged.

## File Structure

| File | Responsibility |
|---|---|
| `port/src/game/fighter.c`, `fighter.h` | the 14 functions (appended after `fighter_24220_case10`) and their prototypes |
| `port/src/game/actors.c` | the ten `anim_code_*` wrappers, the registrations in `actors_init`, and the `0x2B150`/`0x49444` seams |
| `port/tests/diff_runner.c` | the bindings and mutants (`b_*`/`m_*`, `k_bindings`) |
| `tools/diff_verify.py` | `P7_SPECS` and the batch's `E.Call` declarations |
| `tools/tests/test_diff_verify.py` | `P7_MASKS`, `P7_KINDS`; the exact-set assertions extended; `test_each_p7_mutant_is_caught_by_what_it_breaks`; the stub table; the counter line |
| `port/tests/test_fight.c`, `port/tests/test.h` | `test_p7_simple`, `test_p7_36114`, `test_p7_23960`, `test_p7_3a9d8`, `test_p7_streams`, `test_p7_4b03c` |
| `port/tests/test_platform.c`, `Makefile`, `AGENTS.md` | the two gp miss sets lose their P7 rows; the `GP_WIN_*`/`GP_ENDING_*` provenance and the AGENTS.md re-measure clauses (Tasks 2 and 6) |
| `docs/superpowers/plans/2026-10-01-reverse-e2-triage.md` | regenerated in Tasks 2, 3, 4 and 5 |
| `docs/PROGRESS.md`, the record | Task 8 |

---

### Task 1: Baseline (no commit)

**Files:** none. **Interfaces:** consumes `main` `e88eb44`; produces the baseline log `/tmp/pr_p7_base.log`.

- [ ] **Step 1: the full gate on the untouched tree.**

```bash
T=p7; make verify SMK_DUMP=/tmp/pr_${T}_smk TITLE_DUMP=/tmp/pr_${T}_title ATTRACT_DUMP=/tmp/pr_${T}_att \
  FRONTEND_DUMP=/tmp/pr_${T}_fe TITLE_PIN_DIR=/tmp/pr_${T}_pin AUDIO_WAV=/tmp/pr_${T}.wav K11_DUMP=/tmp/pr_${T}_k11 \
  GP_DUMP=/tmp/pr_${T}_gp DIFF_IMAGE=/tmp/pr_${T}_diffimg DIFF_TABLE=/tmp/pr_${T}_diff.md E2_IMAGE=/tmp/pr_${T}_e2.bin \
  > /tmp/pr_p7_base.log 2>&1; echo "EXIT=$?"
grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|== demo-fight)' /tmp/pr_p7_base.log \
  | diff - .superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt && echo ORACLES-EQUAL
grep -E '^diff-verify:|^entry-triage: (targets|voice)|ratchet N' /tmp/pr_p7_base.log | sed 's/^gp_compare: //'
python3 tools/port_progress.py
```

Expected (measured at `e88eb44`): `EXIT=0`, `ORACLES-EQUAL`, the diff-verify line `149/149 functions VERIFIED; 391/391 mutants detected; 1 named gaps; 41/126 rows with callees closed (23 have none)`, the E2 lines `targets 240 unported, 255 ported; supplement 131 (7 unported, 0 stale); untrusted entries 30` / `voice sites outside Ghidra 134: 0 in unported code, 115 in ported code, 19 nowhere`, `771 1203 64` / `731 731 100`, and every gp ratchet line at its pin (the U9 pair 346/2364/8/3162, the U10 331/9954/30/9954 among them). If a value differs, record the measured lines in the ledger.

- [ ] **Step 2: the WAV.** `make audio-render AUDIO_WAV=/tmp/pr_p7.wav >/dev/null 2>&1; cmp /tmp/pr_p7.wav .superpowers/sdd/2026-09-29-k7-k12/scratch/before-t2.wav && echo WAV-SAME` prints `WAV-SAME`.

---

### Task 2: `0x2BDB8`, `0x213F0`, `0x213F4`, `0x3E424` (the D000 pair and their leaf), and gp-u9-win's drop

**Files:** `tools/diff_verify.py` (`P7_SPECS`), `tools/tests/test_diff_verify.py`, `port/tests/test_fight.c`, `port/tests/test.h`, `port/src/game/fighter.c`/`.h`, `port/src/game/actors.c`, `port/tests/diff_runner.c`, the E2 table; the gp drop: `port/tests/test_platform.c`, `Makefile`, `AGENTS.md`.

**Interfaces:** produces `void fighter_2bdb8(u32 rec, u32 arg)`, `void fighter_213f0(u32 rec)`, `void fighter_213f4(u32 rec)`, `void fighter_3e424(u32 rec)`; `P7_2BDB8_R`, `P7_2BDB8_SEED`, `P7_2BDB8_SEED2`, `p7_213f4_case`, `P7_SPECS`; `b_2bdb8`, `m_2bdb8*`, `b_213f0`, `m_213f0`, `b_213f4`, `m_213f4*`, `b_3e424`, `m_3e424*`; `p7_check_simple`, `test_p7_simple`; `P7_MASKS`, `P7_KINDS` (the batch's first four rows). Consumes P6's `P6_PTRS`, `ANIM_BEGIN`, `E3_REC/E3_REC2`, `SLOT_PTRS`.

- [ ] **Step 1: the specs, the unit check, the port, the seams, the bindings and the mutants, in one script.** It applies in order: the specs and the expectations (`tools/diff_verify.py`), the port and the seams (`port/src/game/fighter.c`, `actors.c`), the unit check (`port/tests/test_fight.c`, `test.h`), the bindings and mutants (`port/tests/diff_runner.c`), the test expectations (`tools/tests/test_diff_verify.py`), and, for Tasks 2 and 6, the gp drop, the Makefile pins/provenance and the AGENTS.md clause. It asserts every anchor occurs once, so it either applies cleanly or stops naming the file and the text.

```bash
python3 - <<'PY'
import pathlib


def sub(path, old, new):
    p = pathlib.Path(path)
    s = p.read_text()
    n = s.count(old)
    assert n == 1, "%s: %d occurrences of %r" % (path, n, old[:70])
    p.write_text(s.replace(old, new))

sub('port/src/game/fighter.c', r'''void fighter_24220_case10(u32 slot, u32 side)
{
    fighter_24220(slot, DSD(slot), side);                   /* 0x35396, 0x354E2 */
}
''', r'''void fighter_24220_case10(u32 slot, u32 side)
{
    fighter_24220(slot, DSD(slot), side);                   /* 0x35396, 0x354E2 */
}


/* Track P batch 7 (record 2026-10-04-reverse-p7-derivations.md): the remaining
 * unported direct callees and the targets outside E2. */

/* 0x2BDB8 — record §P7.2. EAX = rec, EDX = the byte argument: the record's
 * +0x2B bit 1, the byte 0x105BEE = the argument, 0x105BEC = the argument - 1
 * and the record's +0x24 = (float)(u8)the argument (the raw's `fild word` of
 * the zero-extended DL). */
void fighter_2bdb8(u32 rec, u32 arg)
{
    PR_SEAM(0x2BDB8u, rec, arg);
    DSB(rec + 0x2Bu) |= 2u;                                 /* 0x2BDBE..0x2BDC4 */
    DSB(0x00105BEEu) = (u8)arg;                             /* 0x2BDCB */
    DSB(0x00105BECu) = (u8)(arg - 1u);                      /* 0x2BDD4/0x2BDD9 */
    p2_set_f32(rec + 0x24u, (float)(arg & 0xFFu));          /* 0x2BDD1/0x2BDD6/0x2BDDF */
}


/* 0x213F0 — record §P7.2. A bare `ret` (the byte 0x213F0, the end of the
 * previous function): the animation stream's target does nothing. */
void fighter_213f0(u32 rec)
{
    (void)rec;
}


/* 0x213F4 — record §P7.2. The D000 target at the dword 0xE16BA: with the other
 * side's slot set, its record on 0xC9148[its char] at 3.0, its +0x58 = 0 and
 * +0x52 = 0xC, then 0x2BDB8(3) on the own and on the other record. */
void fighter_213f4(u32 rec)
{
    u32 other = DSD(DS_001077A8 + ((((u32)DSB(rec + 0x51u)) ^ 1u) & 0xFFu) * 4u);  /* 0x213F9..0x21407 */
    if (other == 0u) return;                                /* 0x21409 */
    actors_anim_begin(DSD(other), DSD(0x000C9148u + (u32)DSB(other + 0x7Au) * 4u),
                      0x40400000u);                         /* 0x2140B..0x2141E */
    DSB(other + 0x58u) = 0u;                                /* 0x21428 */
    DSB(other + 0x52u) = 0x0Cu;                             /* 0x2142E */
    fighter_2bdb8(rec, 3u);                                 /* 0x2142C/0x21432 */
    fighter_2bdb8(DSD(other), 3u);                          /* 0x2143C/0x2143E */
}


/* 0x3E424 — record §P7.2. The D000 target at the dword 0xE8454: 0x213F4's
 * twin with the stream table 0xC9120 and the other slot's +0x41 bit 7. */
void fighter_3e424(u32 rec)
{
    u32 other = DSD(DS_001077A8 + ((((u32)DSB(rec + 0x51u)) ^ 1u) & 0xFFu) * 4u);  /* 0x3E429..0x3E437 */
    if (other == 0u) return;                                /* 0x3E439 */
    actors_anim_begin(DSD(other), DSD(0x000C9120u + (u32)DSB(other + 0x7Au) * 4u),
                      0x40400000u);                         /* 0x3E43B..0x3E44E */
    DSB(other + 0x58u) = 0u;                                /* 0x3E456 */
    DSB(other + 0x41u) |= 0x80u;                            /* 0x3E453/0x3E45A/0x3E462 */
    DSB(other + 0x52u) = 0x0Cu;                             /* 0x3E467 */
    fighter_2bdb8(rec, 3u);                                 /* 0x3E465/0x3E46B */
    fighter_2bdb8(DSD(other), 3u);                          /* 0x3E475/0x3E477 */
}


''')

sub('port/src/game/fighter.h', r'''void fighter_24220(u32 slot, u32 rec, u32 side);
void fighter_24220_case10(u32 slot, u32 side);

#endif /* PRAGE_GAME_FIGHTER_H */
''', r'''void fighter_24220(u32 slot, u32 rec, u32 side);
void fighter_24220_case10(u32 slot, u32 side);
/* Track P batch 7 (record 2026-10-04-reverse-p7-derivations.md): the remaining
 * unported direct callees and the targets outside E2. */
void fighter_2bdb8(u32 rec, u32 arg);
void fighter_213f0(u32 rec);
void fighter_213f4(u32 rec);
void fighter_3e424(u32 rec);
''')

sub('port/src/game/actors.c', r'''/* 0x48374 — the D500 target at the dword 0xED8EA. */
static void anim_code_48374(u32 rec, u32 arg)
{
    (void)arg;
    fighter_48374(rec);
}
''', r'''/* 0x48374 — the D500 target at the dword 0xED8EA. */
static void anim_code_48374(u32 rec, u32 arg)
{
    (void)arg;
    fighter_48374(rec);
}


/* PORT: record 2026-10-04-reverse-p7 §P7.2. Track P batch 7's first animation
 * targets (anim_indirect, EAX = rec, EDX = the operand). */
static void anim_code_213F0(u32 rec, u32 arg)
{
    (void)rec;
    (void)arg;
}
static void anim_code_213F4(u32 rec, u32 arg)
{
    (void)arg;
    fighter_213f4(rec);
}
static void anim_code_3E424(u32 rec, u32 arg)
{
    (void)arg;
    fighter_3e424(rec);
}


''')

sub('port/src/game/actors.c', r'''    fn_register(0x24220u, (void (*)(void))fighter_24220_case10);
    return 1;''', r'''    fn_register(0x24220u, (void (*)(void))fighter_24220_case10);
    /* PORT: record 2026-10-04-reverse-p7 §P7.2. Track P batch 7's animation
     * targets (anim_indirect, EAX = rec, EDX = the operand). */
    fn_register(0x213F0u, (void (*)(void))anim_code_213F0);
    fn_register(0x213F4u, (void (*)(void))anim_code_213F4);
    fn_register(0x3E424u, (void (*)(void))anim_code_3E424);
    ''')

sub('tools/diff_verify.py', r'''

SPECS = [
''', r'''
# ---- track P batch 7: the remaining unported direct callees and the targets outside E2 (record
# 2026-10-04-reverse-p7) --------------------------------------------------------------------------------
# The callees the batch stubs, args in the port's C order and clobbers from E.callee_clobbers.
P7_2BDB8 = E.Call(0x2BDB8, ("eax", "edx"), clobbers=("edx",))
P7_2BDB8_R = E.Call(0x2BDB8, ("eax", "edx"), mode="real")
P7_2BDE8_R = E.Call(0x2BDE8, ("eax",), mode="real")
P7_23960 = E.Call(0x23960, ("eax",))
P7_3A9D8 = E.Call(0x3A9D8, ("eax", "edx"), clobbers=("edx",))
P7_2B150 = E.Call(0x2B150, ("eax",), clobbers=("esi", "edi", "ebp"))
P7_41310 = E.Call(0x41310, ("eax", "edx"))
P7_49444 = E.Call(0x49444, ("eax",), clobbers=("edi", "ebp"))
P7_RNG   = E.Call(0x5D7DC, ("eax",), mode="real")
P7_22404 = E.Call(0x22404, ("eax",))
P7_2BCF4 = E.Call(0x2BCF4, ("eax", "edx"), clobbers=("edx",))
P7_2A17C = E.Call(0x2A17C, ("eax", "edx", "ebx"), clobbers=("edx",))

# 0x2BDB8 (record §P7.2): the record's +0x2B bit 1, the byte 0x105BEE = the argument, 0x105BEC =
# the argument - 1 and +0x24 = (float)(u8)the argument. b1 (0x100) separates the low byte from the
# whole argument for all three stores; b2 (0x1234FF) pins 255.0f.
P7_2BDB8_SEED = {E3_REC + 0x24: le32(0x24242424), E3_REC + 0x2B: b"\x2b\x2c",
                 0x105BEE: b"\xee\xef", 0x105BEC: b"\xec\xed"}
P7_2BDB8_SEED2 = {E3_REC2 + 0x24: le32(0x34343434), E3_REC2 + 0x2B: b"\x3b\x3c",
                  0x105BEE: b"\xee\xef", 0x105BEC: b"\xec\xed"}


# 0x213F4/0x3E424 (record §P7.2): the other side's slot by rec+0x51 ^ 1; its record on
# 0xC9148/0xC9120[its char] at 3.0; its +0x58 = 0; 0x3E424's +0x41 |= 0x80; its +0x52 = 0xC; then
# 0x2BDB8(3) on the own and the other record. The records and the +0x2B/0x24 stores carry sentinels.
def p7_213f4_case(cid, rec, orec, oside, char):
    oslot = DS_SLOTS + oside * 0x94
    return Case(cid, {"eax": rec, "edx": 0x1234},
                {**SLOT_PTRS, **P6_PTRS,
                 rec + 0x51: bytes([oside ^ 1]), orec + 0x51: bytes([oside]),
                 oslot + 0x7A: bytes([char]), oslot + 0x58: b"\x58\x59",
                 oslot + 0x52: b"\x52\x53", oslot + 0x41: b"\x41\x42",
                 rec + 0x24: le32(0x24242424), rec + 0x2B: b"\x2b\x2c",
                 orec + 0x24: le32(0x34343434), orec + 0x2B: b"\x3b\x3c",
                 0x105BEE: b"\xee\xef", 0x105BEC: b"\xec\xed"})


# 0x23960 (record §P7.4): 0x105B4C = 0; with the other side's slot set, four 0xA839C spawns at its
# +0x2C + 0x600/-0x600/+0x1000/-0x1000, y = the other record's +0x30 >> 16; each child's +0x24 +=
# (float)rng_next(6) and +0x20 the same. The spawn stub returns P6_CHILD.
P7_23960_SEED = {**SLOT_PTRS, **P6_PTRS,
                 E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
                 DS_SLOTS + 0x94 + 0x2C: le32(0x00001000),
                 E3_REC2 + 0x30: le32(0x12345678),
                 P6_CHILD + 0x20: le32(0x20202020), P6_CHILD + 0x24: le32(0x40400000),
                 0x105B4C: b"\x4c\x4d"}

# 0x23AE0 (record §P7.6): the other slot's record's +0x29 bit 3, +0x2E = 0x64, +0x4E = 1; the
# char's word from 0x23AC4 (0x46B6..0x46BA, default 0x46B9) sought on it; the palette handle
# 0x105FEBC; the word 0x105B4C = 1.
def p7_23ae0_case(cid, rec, char):
    return Case(cid, {"eax": rec, "edx": 0x1234},
                {**SLOT_PTRS, **P6_PTRS,
                 rec + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
                 DS_SLOTS + 0x94 + 0x7A: bytes([char]), E3_REC2 + 0x29: b"\x29\x2a",
                 E3_REC2 + 0x2E: b"\x2e\x2f", E3_REC2 + 0x4E: b"\x4e\x4f",
                 0x105B4C: b"\x4c\x4d"})


# 0x4B03C (record §P7.7): the byte (slot+8's record +0x48) - 0x20 selects 0x10/0xE/0x15/0xC/0xA
# (0xD default) and the slot's +0x5A loses it to 0; voice 0xD6, voice 0xCE, the record dead,
# 0x41310 on the slot's +0x21/+0x20 (mode 0x22/0x24 excluded), the 0x1088A4/0x1088A2 counters,
# then 0x49444.
P7_4B03C_SEED = {E3_REC + 0x14: le32(E3_SLOT), E3_SLOT + 8: le32(E3_OUT),
                 E3_SLOT + 0x20: b"\x00", E3_SLOT + 0x21: b"\x01",
                 0x10780A: b"\x5a", 0x10780A + 0x94: b"\x5b",
                 0x10889E + 1: b"\x9e", 0x1088A4: b"\xa4", 0x1088A2 + 1: b"\xa2",
                 0x104B00: b"\x11\x00", E3_OUT + 0x48: b"\x20"}

P7_SPECS = [
    Spec("fighter_2bdb8", 0x2BDB8, [
        Case("b0", {"eax": E3_REC, "edx": 0x42}, P7_2BDB8_SEED),
        Case("b1", {"eax": E3_REC, "edx": 0x100}, P7_2BDB8_SEED),
        Case("b2", {"eax": E3_REC2, "edx": 0x1234FF}, P7_2BDB8_SEED2),
    ], eax_mask=0, mutants=("@mutant", "@and", "@dec", "@width")),
    # 0x213F0 (record §P7.2): a bare `ret`; the row asserts no byte changes.
    Spec("fighter_213f0", 0x213F0, [
        Case("r0", {"eax": E3_REC, "edx": 0x1234}, {E3_REC + 0x2B: b"\x28"}),
    ], eax_mask=0, mutants=("@mutant",)),
    Spec("fighter_213f4", 0x213F4, [
        p7_213f4_case("s0", E3_REC, E3_REC2, 0, 3),
        p7_213f4_case("s1", E3_REC2, E3_REC, 1, 2),
        Case("o0", {"eax": E3_REC, "edx": 0x1234},
             {DS_SLOTS - 8: le32(DS_SLOTS) + le32(0), E3_REC + 0x51: b"\x00",
              E3_REC + 0x24: le32(0x24242424), E3_REC + 0x2B: b"\x2b\x2c",
              0x105BEE: b"\xee\xef", 0x105BEC: b"\xec\xed"}),
    ], calls=(ANIM_BEGIN, P7_2BDB8_R), eax_mask=0,
       mutants=("@mutant", "@stream", "@side", "@order")),
    Spec("fighter_3e424", 0x3E424, [
        p7_213f4_case("s0", E3_REC, E3_REC2, 0, 3),
        p7_213f4_case("s1", E3_REC2, E3_REC, 1, 2),
        Case("o0", {"eax": E3_REC, "edx": 0x1234},
             {DS_SLOTS - 8: le32(DS_SLOTS) + le32(0), E3_REC + 0x51: b"\x00",
              E3_REC + 0x24: le32(0x24242424), E3_REC + 0x2B: b"\x2b\x2c",
              0x105BEE: b"\xee\xef", 0x105BEC: b"\xec\xed"}),
    ], calls=(ANIM_BEGIN, P7_2BDB8_R), eax_mask=0,
       mutants=("@mutant", "@bit", "@side", "@order")),
    ''')

sub('tools/diff_verify.py', r'''] + E3_SPECS + P1_SPECS + P1_ANIM_SPECS + P2_SPECS + P3_SPECS + P6_SPECS + P45_SPECS + C1_SPECS
''', r'''] + E3_SPECS + P1_SPECS + P1_ANIM_SPECS + P2_SPECS + P3_SPECS + P6_SPECS + P45_SPECS + C1_SPECS + P7_SPECS
''')

sub('port/tests/diff_runner.c', r'''static const binding_t k_bindings[] = {
''', r'''/* Track P batch 7 (record 2026-10-04-reverse-p7): the remaining unported direct
 * callees and the targets outside E2. */
static void b_2bdb8(const u32 *r, u32 *eax)            { fighter_2bdb8(r[R_EAX], r[R_EDX]); *eax = 0u; }
static void m_2bdb8(const u32 *r, u32 *eax)            /* the float from the whole argument */
{
    u32 rec = r[R_EAX];
    DSB(rec + 0x2Bu) |= 2u;
    DSB(0x00105BEEu) = (u8)r[R_EDX];
    DSB(0x00105BECu) = (u8)(r[R_EDX] - 1u);
    DSD(rec + 0x24u) = (u32)(float)r[R_EDX];
    *eax = 0u;
}
static void m_2bdb8_and(const u32 *r, u32 *eax)        /* the +0x2B store without the OR */
{
    u32 rec = r[R_EAX];
    DSB(rec + 0x2Bu) = 2u;
    DSB(0x00105BEEu) = (u8)r[R_EDX];
    DSB(0x00105BECu) = (u8)(r[R_EDX] - 1u);
    DSD(rec + 0x24u) = (u32)(float)(r[R_EDX] & 0xFFu);
    *eax = 0u;
}
static void m_2bdb8_dec(const u32 *r, u32 *eax)        /* 0x105BEC = the argument, not -1 */
{
    u32 rec = r[R_EAX];
    DSB(rec + 0x2Bu) |= 2u;
    DSB(0x00105BEEu) = (u8)r[R_EDX];
    DSB(0x00105BECu) = (u8)r[R_EDX];
    DSD(rec + 0x24u) = (u32)(float)(r[R_EDX] & 0xFFu);
    *eax = 0u;
}
static void m_2bdb8_width(const u32 *r, u32 *eax)      /* a word store of 0x105BEE */
{
    u32 rec = r[R_EAX];
    DSB(rec + 0x2Bu) |= 2u;
    DSW(0x00105BEEu) = (u16)r[R_EDX];
    DSB(0x00105BECu) = (u8)(r[R_EDX] - 1u);
    DSD(rec + 0x24u) = (u32)(float)(r[R_EDX] & 0xFFu);
    *eax = 0u;
}
static void b_213f0(const u32 *r, u32 *eax)            { fighter_213f0(r[R_EAX]); *eax = 0u; }
static void m_213f0(const u32 *r, u32 *eax)            /* writes where the bare ret does not */
{
    DSB(r[R_EAX] + 0x2Bu) |= 2u;
    *eax = 0u;
}
static void b_213f4(const u32 *r, u32 *eax)            { b_anim(0x213F4u, r, eax); }
static void m_213f4(const u32 *r, u32 *eax)            /* the own record's char for the stream */
{
    u32 rec = r[R_EAX];
    u32 other = DSD(DS_001077A8 + ((((u32)DSB(rec + 0x51u)) ^ 1u) & 0xFFu) * 4u);
    if (other != 0u) {
        actors_anim_begin(DSD(other), DSD(0x000C9148u + (u32)DSB(rec + 0x7Au) * 4u), 0x40400000u);
        DSB(other + 0x58u) = 0u;
        DSB(other + 0x52u) = 0x0Cu;
        fighter_2bdb8(rec, 3u);
        fighter_2bdb8(DSD(other), 3u);
    }
    *eax = 0u;
}
static void m_213f4_stream(const u32 *r, u32 *eax)     /* the 0xC9120 stream table */
{
    u32 rec = r[R_EAX];
    u32 other = DSD(DS_001077A8 + ((((u32)DSB(rec + 0x51u)) ^ 1u) & 0xFFu) * 4u);
    if (other != 0u) {
        actors_anim_begin(DSD(other), DSD(0x000C9120u + (u32)DSB(other + 0x7Au) * 4u), 0x40400000u);
        DSB(other + 0x58u) = 0u;
        DSB(other + 0x52u) = 0x0Cu;
        fighter_2bdb8(rec, 3u);
        fighter_2bdb8(DSD(other), 3u);
    }
    *eax = 0u;
}
static void m_213f4_side(const u32 *r, u32 *eax)       /* the own side's slot (index & 1) */
{
    u32 rec = r[R_EAX];
    u32 other = DSD(DS_001077A8 + ((((u32)DSB(rec + 0x51u)) & 1u)) * 4u);
    if (other != 0u) {
        actors_anim_begin(DSD(other), DSD(0x000C9148u + (u32)DSB(other + 0x7Au) * 4u), 0x40400000u);
        DSB(other + 0x58u) = 0u;
        DSB(other + 0x52u) = 0x0Cu;
        fighter_2bdb8(rec, 3u);
        fighter_2bdb8(DSD(other), 3u);
    }
    *eax = 0u;
}
static void m_213f4_order(const u32 *r, u32 *eax)      /* the stores before the 0x2BC30 call */
{
    u32 rec = r[R_EAX];
    u32 other = DSD(DS_001077A8 + ((((u32)DSB(rec + 0x51u)) ^ 1u) & 0xFFu) * 4u);
    if (other != 0u) {
        DSB(other + 0x58u) = 0u;
        DSB(other + 0x52u) = 0x0Cu;
        actors_anim_begin(DSD(other), DSD(0x000C9148u + (u32)DSB(other + 0x7Au) * 4u), 0x40400000u);
        fighter_2bdb8(rec, 3u);
        fighter_2bdb8(DSD(other), 3u);
    }
    *eax = 0u;
}
static void b_3e424(const u32 *r, u32 *eax)            { b_anim(0x3E424u, r, eax); }
static void m_3e424(const u32 *r, u32 *eax)            /* the 0xC9148 stream table */
{
    u32 rec = r[R_EAX];
    u32 other = DSD(DS_001077A8 + ((((u32)DSB(rec + 0x51u)) ^ 1u) & 0xFFu) * 4u);
    if (other != 0u) {
        actors_anim_begin(DSD(other), DSD(0x000C9148u + (u32)DSB(other + 0x7Au) * 4u), 0x40400000u);
        DSB(other + 0x58u) = 0u;
        DSB(other + 0x41u) |= 0x80u;
        DSB(other + 0x52u) = 0x0Cu;
        fighter_2bdb8(rec, 3u);
        fighter_2bdb8(DSD(other), 3u);
    }
    *eax = 0u;
}
static void m_3e424_bit(const u32 *r, u32 *eax)        /* +0x41 set, not OR-ed */
{
    u32 rec = r[R_EAX];
    u32 other = DSD(DS_001077A8 + ((((u32)DSB(rec + 0x51u)) ^ 1u) & 0xFFu) * 4u);
    if (other != 0u) {
        actors_anim_begin(DSD(other), DSD(0x000C9120u + (u32)DSB(other + 0x7Au) * 4u), 0x40400000u);
        DSB(other + 0x58u) = 0u;
        DSB(other + 0x41u) = 0x80u;
        DSB(other + 0x52u) = 0x0Cu;
        fighter_2bdb8(rec, 3u);
        fighter_2bdb8(DSD(other), 3u);
    }
    *eax = 0u;
}
static void m_3e424_side(const u32 *r, u32 *eax)       /* the own side's slot */
{
    u32 rec = r[R_EAX];
    u32 other = DSD(DS_001077A8 + ((((u32)DSB(rec + 0x51u)) & 1u)) * 4u);
    if (other != 0u) {
        actors_anim_begin(DSD(other), DSD(0x000C9120u + (u32)DSB(other + 0x7Au) * 4u), 0x40400000u);
        DSB(other + 0x58u) = 0u;
        DSB(other + 0x41u) |= 0x80u;
        DSB(other + 0x52u) = 0x0Cu;
        fighter_2bdb8(rec, 3u);
        fighter_2bdb8(DSD(other), 3u);
    }
    *eax = 0u;
}
static void m_3e424_order(const u32 *r, u32 *eax)      /* the stores before the 0x2BC30 call */
{
    u32 rec = r[R_EAX];
    u32 other = DSD(DS_001077A8 + ((((u32)DSB(rec + 0x51u)) ^ 1u) & 0xFFu) * 4u);
    if (other != 0u) {
        DSB(other + 0x58u) = 0u;
        DSB(other + 0x41u) |= 0x80u;
        DSB(other + 0x52u) = 0x0Cu;
        actors_anim_begin(DSD(other), DSD(0x000C9120u + (u32)DSB(other + 0x7Au) * 4u), 0x40400000u);
        fighter_2bdb8(rec, 3u);
        fighter_2bdb8(DSD(other), 3u);
    }
    *eax = 0u;
}
''')

sub('port/tests/diff_runner.c', r'''    { "fighter_24220@decr",       m_24220_decr,   0x00000000u },
};
''', r'''    { "fighter_24220@decr",       m_24220_decr,   0x00000000u },
    { "fighter_2bdb8",            b_2bdb8,        0x00000000u },
    { "fighter_2bdb8@mutant",     m_2bdb8,        0x00000000u },
    { "fighter_2bdb8@and",        m_2bdb8_and,    0x00000000u },
    { "fighter_2bdb8@dec",        m_2bdb8_dec,    0x00000000u },
    { "fighter_2bdb8@width",      m_2bdb8_width,  0x00000000u },
    { "fighter_213f0",            b_213f0,        0x00000000u },
    { "fighter_213f0@mutant",     m_213f0,        0x00000000u },
    { "fighter_213f4",            b_213f4,        0x00000000u },
    { "fighter_213f4@mutant",     m_213f4,        0x00000000u },
    { "fighter_213f4@stream",     m_213f4_stream, 0x00000000u },
    { "fighter_213f4@side",       m_213f4_side,   0x00000000u },
    { "fighter_213f4@order",      m_213f4_order,  0x00000000u },
    { "fighter_3e424",            b_3e424,        0x00000000u },
    { "fighter_3e424@mutant",     m_3e424,        0x00000000u },
    { "fighter_3e424@bit",        m_3e424_bit,    0x00000000u },
    { "fighter_3e424@side",       m_3e424_side,   0x00000000u },
    { "fighter_3e424@order",      m_3e424_order,  0x00000000u },
    ''')

sub('port/tests/test_fight.c', r'''int test_p6_24220(void)         { return u6b_run(p6_check_24220); }
''', r'''int test_p6_24220(void)         { return u6b_run(p6_check_24220); }

typedef void (*p7_anim_fn)(u32 rec, u32 arg);

/* Track P batch 7 (record 2026-10-04-reverse-p7): the remaining unported
 * direct callees and the targets outside E2. Each check seeds sentinels that
 * differ from the post-conditions; the stores a mutation changes are asserted
 * (the row's mutants carry the same cases). */
static void p7_check_simple(void)
{
    p7_anim_fn f;
    /* 0x2BDB8: +0x2B bit 1, 0x105BEE = the low byte, 0x105BEC = it - 1 and
     * +0x24 = (float)(u8)arg; 0x100 pins the low byte. */
    z_fseed();
    DSB(Z_R0 + 0x2Bu) = 0x01u;
    DSD(Z_R0 + 0x24u) = 0x24242424u;
    DSB(0x00105BEEu) = 0xEEu;
    DSB(0x00105BECu) = 0xECu;
    fighter_2bdb8(Z_R0, 0x42u);
    CHECK_EQ_INT((int)DSB(Z_R0 + 0x2Bu), 0x03);
    CHECK_EQ_INT((int)DSD(Z_R0 + 0x24u), 0x42840000);
    CHECK_EQ_INT((int)DSB(0x00105BEEu), 0x42);
    CHECK_EQ_INT((int)DSB(0x00105BECu), 0x41);
    fighter_2bdb8(Z_R0, 0x100u);
    CHECK_EQ_INT((int)DSD(Z_R0 + 0x24u), 0x00000000);
    CHECK_EQ_INT((int)DSB(0x00105BEEu), 0x00);
    CHECK_EQ_INT((int)DSB(0x00105BECu), 0xFF);

    /* 0x213F0: registered, and a no-op. */
    f = (p7_anim_fn)(void *)fn_resolve(0x213F0u);
    CHECK(f != NULL, "0x213F0 is registered");
    if (f != NULL) {
        z_fseed();
        DSB(Z_R0 + 0x2Bu) = 0x28u;
        f(Z_R0, 0u);
        CHECK_EQ_INT((int)DSB(Z_R0 + 0x2Bu), 0x28);
    }

    /* 0x213F4: the other slot's +0x58 = 0 and +0x52 = 0xC, both records on
     * 3.0 through 0x2BDB8 and the other record on 0xC9148[its char] at 3.0. */
    f = (p7_anim_fn)(void *)fn_resolve(0x213F4u);
    CHECK(f != NULL, "0x213F4 is registered");
    if (f != NULL) {
        z_fseed();
        DSB(Z_R0 + 0x51u) = 0u;
        DSB(Z_S1 + 0x7Au) = 3u;
        DSB(Z_S1 + 0x58u) = 0x58u;
        DSB(Z_S1 + 0x52u) = 0x52u;
        DSD(Z_R0 + 0x24u) = 0x24242424u;
        DSD(Z_R1 + 0x24u) = 0x34343434u;
        DSB(Z_R0 + 0x2Bu) = 0x28u;
        DSB(Z_R1 + 0x2Bu) = 0x38u;
        DSD(Z_R1 + 8u) = 0x9999u;
        f(Z_R0, 0u);
        CHECK_EQ_INT((int)DSB(Z_S1 + 0x58u), 0);
        CHECK_EQ_INT((int)DSB(Z_S1 + 0x52u), 0x0C);
        CHECK_EQ_INT((int)DSD(Z_R0 + 0x24u), 0x40400000);
        CHECK_EQ_INT((int)DSD(Z_R1 + 0x24u), 0x40400000);
        CHECK_EQ_INT((int)DSB(Z_R0 + 0x2Bu), 0x2A);
        CHECK_EQ_INT((int)DSB(Z_R1 + 0x2Bu), 0x3A);
        CHECK(DSD(Z_R1 + 8u) != 0x9999u, "the other record's stream started");
    }

    /* 0x3E424: the other slot's +0x41 bit 7 and the 0xC9120 stream. */
    f = (p7_anim_fn)(void *)fn_resolve(0x3E424u);
    CHECK(f != NULL, "0x3E424 is registered");
    if (f != NULL) {
        z_fseed();
        DSB(Z_R0 + 0x51u) = 0u;
        DSB(Z_S1 + 0x7Au) = 3u;
        DSB(Z_S1 + 0x41u) = 0x41u;
        DSB(Z_S1 + 0x58u) = 0x58u;
        DSD(Z_R1 + 8u) = 0x9999u;
        f(Z_R0, 0u);
        CHECK_EQ_INT((int)DSB(Z_S1 + 0x41u), 0xC1);
        CHECK_EQ_INT((int)DSB(Z_S1 + 0x58u), 0);
        CHECK(DSD(Z_R1 + 8u) != 0x9999u, "the other record's stream started");
    }

    /* 0x224EC: the own slot's +0x57 = 2 (0x22404 runs real on the same side);
     * the other slot's sentinel survives. */
    f = (p7_anim_fn)(void *)fn_resolve(0x224ECu);
    CHECK(f != NULL, "0x224EC is registered");
    if (f != NULL) {
        z_fseed();
        DSB(Z_R0 + 0x51u) = 0u;
        DSB(Z_S0 + 0x57u) = 0x57u;
        DSB(Z_S1 + 0x57u) = 0x67u;
        f(Z_R0, 0u);
        CHECK_EQ_INT((int)DSB(Z_S0 + 0x57u), 2);
        CHECK_EQ_INT((int)DSB(Z_S1 + 0x57u), 0x67);
    }
}
int test_p7_simple(void)        { return u6b_run(p7_check_simple); }

/* §P7.3: 0x2BDE8 and 0x36114 through their registrations. */
''')

sub('port/tests/test.h', r'''    X(test_p6_24220) \
''', r'''    X(test_p6_24220) \
    X(test_p7_simple) \
    X(test_p7_36114) \
    X(test_p7_23960) \
    X(test_p7_3a9d8) \
    X(test_p7_streams) \
    X(test_p7_4b03c) \
''')

sub('tools/tests/test_diff_verify.py', r'''    "fighter_48374@stream": {"call #1"},
}
''', r'''    "fighter_48374@stream": {"call #1"},
}

# Track P batch 7 (record 2026-10-04-reverse-p7): its rows with their EAX masks, and what alone
# catches each of its mutants.
P7_MASKS = {"fighter_2bdb8": 0, "fighter_213f0": 0, "fighter_213f4": 0, "fighter_3e424": 0}
P7_KINDS = {
    "fighter_2bdb8@mutant": {"byte"},
    "fighter_2bdb8@and": {"byte"},
    "fighter_2bdb8@dec": {"byte"},
    "fighter_2bdb8@width": {"byte"},
    "fighter_213f0@mutant": {"byte"},
    "fighter_213f4@mutant": {"call #0"},
    "fighter_213f4@stream": {"call #0"},
    "fighter_213f4@side": {"byte", "call #0", "call #1", "call #1 memory", "call #2", "call #2 memory"},
    "fighter_213f4@order": {"call #0 memory"},
    "fighter_3e424@mutant": {"call #0"},
    "fighter_3e424@bit": {"byte", "call #1 memory", "call #2 memory"},
    "fighter_3e424@side": {"byte", "call #0", "call #1", "call #1 memory", "call #2", "call #2 memory"},
    "fighter_3e424@order": {"call #0 memory"},
    ''')

sub('tools/tests/test_diff_verify.py', '+ list(P6_MASKS)))', '+ list(P6_MASKS) + list(P7_MASKS)))')

sub('tools/tests/test_diff_verify.py', '+ list(P6_KINDS)))', '+ list(P6_KINDS) + list(P7_KINDS)))')

sub('tools/tests/test_diff_verify.py', '**C1_MASKS, **P6_MASKS})', r'''**C1_MASKS, **P6_MASKS,
            **P7_MASKS})''')

sub('tools/tests/test_diff_verify.py', '    def test_each_stub_declares_the_registers_its_callee_clobbers(self):', r'''    def test_each_p7_mutant_is_caught_by_what_it_breaks(self):
        # track P batch 7 (record 2026-10-04-reverse-p7): what alone catches each mutant
        for name, want in P7_KINDS.items():
            got = {p.split(": ", 1)[1].split(":")[0] if p.split(": ", 1)[1].startswith("call #")
                   else p.split(": ", 1)[1].split(" ")[0] for p in self.mut[name].problems}
            self.assertEqual(got, want, name)

    def test_each_stub_declares_the_registers_its_callee_clobbers(self):''')

print('T2 applied')
sub('tools/tests/test_diff_verify.py', '        self.assertIn("diff-verify: 149/149 functions VERIFIED; 391/391 mutants detected; 1 named gaps; "\n                      "41/126 rows with callees closed (23 have none).", out.getvalue())', '        self.assertIn("diff-verify: 153/153 functions VERIFIED; 404/404 mutants detected; 1 named gaps; "\n                      "43/128 rows with callees closed (25 have none).", out.getvalue())')



sub('port/src/game/actors.c', '''static void anim_code_22338(u32 rec, u32 arg);
static void anim_code_48374(u32 rec, u32 arg);
''', '''static void anim_code_22338(u32 rec, u32 arg);
static void anim_code_48374(u32 rec, u32 arg);
static void anim_code_213F0(u32 rec, u32 arg);
static void anim_code_213F4(u32 rec, u32 arg);
static void anim_code_3E424(u32 rec, u32 arg);
static void anim_code_224EC(u32 rec, u32 arg);
static void anim_code_36114(u32 rec, u32 arg);
static void anim_code_23A7C(u32 rec, u32 arg);
static void anim_code_48254(u32 rec, u32 arg);
static void anim_code_23AE0(u32 rec, u32 arg);
static void anim_code_29C78(u32 rec, u32 arg);
static void anim_code_4B03C(u32 rec, u32 arg);
''')
sub('port/tests/test_platform.c', r''' * (0x29D60, a bare `ret`, from f = 0x28D; 0x5D812, the runtime stub, from f = 0x405)
 * and the anim_indirect targets the replay then reaches, 0x213F0 (first = 0x8ED,
 * 1 hit) and 0x213F4 (first = 0x91F, 1 hit). The replay no longer misses 0x2BDA0
 * (first = 0xD0C, 10 hits before P6 ported it). */
static const fnm_pair k_miss_gp_u9_win[] = {
    { 0x29D60u, "frontend_mode_1b_step" },
    { 0x5D812u, "frontend_mode_1b_step" },
    { 0x213F0u, "anim_indirect" },
    { 0x213F4u, "anim_indirect" },
};''', r''' * (0x29D60, a bare `ret`, from f = 0x28D; 0x5D812, the runtime stub, from f = 0x405).
 * Track P batch 7 (record 2026-10-04-reverse-p7 §P7.2) ports 0x213F0 and 0x213F4,
 * so the replay no longer misses them; the pins were re-measured (record §P7.8). */
static const fnm_pair k_miss_gp_u9_win[] = {
    { 0x29D60u, "frontend_mode_1b_step" },
    { 0x5D812u, "frontend_mode_1b_step" },
};''')
sub('Makefile', '''GP_WIN_MIN_FIRST = 346
GP_WIN_TRACE_MIN_FIRST = 2364
GP_WIN_MAX_START = 100
GP_WIN_MILESTONES = 8
GP_WIN_WIN_MIN_FIRST = 3162''', '''GP_WIN_MIN_FIRST = 346
GP_WIN_TRACE_MIN_FIRST = 3503
GP_WIN_MAX_START = 100
GP_WIN_MILESTONES = 8
GP_WIN_WIN_MIN_FIRST = 3503''')
sub('Makefile', '''# trace's first difference at f=0x93C precedes 0x2BDA0's first frame 0xD0C: every pin is exact
# (frames +1 347, trace +1 2365, win +1 3163, path +1 9 unreachable, max-start -1 99 fail).''', '''# trace's first difference at f=0x93C precedes 0x2BDA0's first frame 0xD0C: every pin is exact
# (frames +1 347, trace +1 2365, win +1 3163, path +1 9 unreachable, max-start -1 99 fail).
# Re-measured by track P batch 7 (record 2026-10-04-reverse-p7 §P7.8) once 0x213F0 and 0x213F4 are
# ported: the set loses both rows (fn-miss PR_GP_DUMP distinct=4 dropped=0: 0x5D812 actor_spawn and
# set_dead, 0x29D60 and 0x5D812 from frontend_mode_1b_step) and TRACE_MIN_FIRST and WIN_MIN_FIRST
# rise 2364/3162 -> 3503 (0 differing through 3502; N 3504 is unreachable, the replay's end).
# MIN_FIRST stays 346 (the mode-8 long frame; 347 fails) and MAX_START stays 100 (99 fails).''')
sub('AGENTS.md', ''' (distinct=5; `0x29C78` P7 remains) with every U10 pin unchanged — record 2026-10-03-reverse-p6
  §P6.12).''', ''' (distinct=5; `0x29C78` P7 remains) with every U10 pin unchanged — record 2026-10-03-reverse-p6
  §P6.12; batch 7 ported `0x213F0`/`0x213F4` P7: U9's set loses both rows (distinct=4) and its
  TRACE and WIN pins rise 2364/3162 -> 3503 (MIN_FIRST 346 and MAX_START 100 unchanged and exact)
  — record 2026-10-04-reverse-p7 §P7.8).''')

PY
```

Then build and run the task's tests:

```bash
cmake --build build 2>&1 | tail -1
python3 tools/diff_verify.py --self-check --image /tmp/pr_p7_dfv.bin --table /tmp/pr_p7_diff.md | tail -1
PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | grep -E 'FAIL|FAILURES|all checks passed'
```

Expected: the counter line `153/153 functions VERIFIED; 404/404 mutants detected; 1 named gaps; 43/128 rows with callees closed (25 have none).`; every new row VERIFIED with its blocks hit; the new unit check passes. The rows: `fighter_2bdb8` 3 cases 1/1 blocks (no callee), `fighter_213f0` 1 1/1 (no callee), `fighter_213f4` 3 3/3 (`2BC30` stub VERIFIED, `2BDB8` real VERIFIED), `fighter_3e424` 3 3/3 (the same callees); 13 mutants detected.

- [ ] **Step 2: the E2 table.** `make entry-triage` must pass against the regenerated table; then

```bash
./build/diffrun --exe data/game/C/PRAGE.EXE --image-out /tmp/pr_p7_e2img.bin && \
python3 tools/entry_triage.py --image /tmp/pr_p7_e2img.bin \
  --live docs/superpowers/plans/2026-10-01-reverse-e2-live-functions.txt \
  --out docs/superpowers/plans/2026-10-01-reverse-e2-triage.md
```

Expected: `entry-triage: targets 238 unported, 257 ported; supplement 131 (6 unported, 0 stale); untrusted entries 30` and `voice sites outside Ghidra 134: 0 in unported code, 115 in ported code, 19 nowhere`.

-

 

- [ ] **Step 4: every new unit assertion can fail.** The `P7_KINDS` entry of every new mutant names the case set that alone catches it (record §P7.10); `test_each_p7_mutant_is_caught_by_what_it_breaks` asserts the sets, and the self-check reports each mutant MISMATCH. For each unit check, mutate the port function it tests (the same one-line change as the matching `m_*` mutant), run `PR_ORACLE_REQUIRED=1 ./build/run_tests`, record the first FAIL line, and restore the code.

- [ ] **Step 5: commit.** Stage the named files (`git add` each; never `git add -A`) and commit:

```bash
git commit -m "fighter: port 0x2BDB8 0x213F0 0x213F4 0x3E424; gp-u9-win drops the pair (track P batch 7, task 2)"
```

---

### Task 3: `0x224EC`, `0x2BDE8`, `0x36114` (the D500 ctx target, the leaf, the anchor target)

**Files:** as Task 2 (the anchors move to Task 2's last lines).

**Interfaces:** produces `void fighter_224ec(u32 rec)`, `void fighter_2bde8(u32 rec)`, `void fighter_36114(u32 rec)`; `P7_22404`, `P7_2BDE8_R`; the specs, bindings and mutants for the three; `p7_check_36114`, `test_p7_36114`. Consumes Task 2's `P7_SPECS`/`P7_KINDS`/`b_anim`; C1's `C1_1883C`.

- [ ] **Step 1: the specs, the unit check, the port, the seams, the bindings and the mutants, in one script.** It applies in order: the specs and the expectations (`tools/diff_verify.py`), the port and the seams (`port/src/game/fighter.c`, `actors.c`), the unit check (`port/tests/test_fight.c`, `test.h`), the bindings and mutants (`port/tests/diff_runner.c`), the test expectations (`tools/tests/test_diff_verify.py`), and, for Tasks 2 and 6, the gp drop, the Makefile pins/provenance and the AGENTS.md clause. It asserts every anchor occurs once, so it either applies cleanly or stops naming the file and the text.

```bash
python3 - <<'PY'
import pathlib


def sub(path, old, new):
    p = pathlib.Path(path)
    s = p.read_text()
    n = s.count(old)
    assert n == 1, "%s: %d occurrences of %r" % (path, n, old[:70])
    p.write_text(s.replace(old, new))

sub('port/src/game/fighter.c', r'''    DSB(other + 0x41u) |= 0x80u;                            /* 0x3E453/0x3E45A/0x3E462 */
    DSB(other + 0x52u) = 0x0Cu;                             /* 0x3E467 */
    fighter_2bdb8(rec, 3u);                                 /* 0x3E465/0x3E46B */
    fighter_2bdb8(DSD(other), 3u);                          /* 0x3E475/0x3E477 */
}
''', r'''    DSB(other + 0x41u) |= 0x80u;                            /* 0x3E453/0x3E45A/0x3E462 */
    DSB(other + 0x52u) = 0x0Cu;                             /* 0x3E467 */
    fighter_2bdb8(rec, 3u);                                 /* 0x3E465/0x3E46B */
    fighter_2bdb8(DSD(other), 3u);                          /* 0x3E475/0x3E477 */
}
/* 0x224EC — record §P7.3. The D500 target at the dword 0xE4E30: ctx;
 * 0x22404(ctx[0]); the own slot's +0x57 = 2. */
void fighter_224ec(u32 rec)
{
    u32 ctx[6];
    hit_anim_ctx(ctx, rec);                                 /* 0x224F4 */
    fighter_22404(ctx[0]);                                  /* 0x224F9/0x224FC */
    DSB(ctx[2] + 0x57u) = 2u;                               /* 0x22501/0x22505 */
}


/* 0x2BDE8 — record §P7.3. EAX = rec: the record's +0x20 = its +0x24 + (-1.0f,
 * the dword 0x809B8) and +0x2B bit 1 clear. */
void fighter_2bde8(u32 rec)
{
    PR_SEAM(0x2BDE8u, rec);
    p2_set_f32(rec + 0x20u, p2_f32(rec + 0x24u) + p2_f32(0x000809B8u));   /* 0x2BDE9..0x2BDF8 */
    DSB(rec + 0x2Bu) &= (u8)~2u;                            /* 0x2BDEC/0x2BDF5/0x2BDFB */
}


/* 0x36114 — record §P7.3. The D000 target at the dword 0xD27E4. The record's
 * +0x24 = 2.0; with its +0x14 slot set: the slot's +0x58 + 1; the signed word
 * 0xBDA4C[char] negated unless the record's +0x28 bit 14; the word
 * 0xBDA5A[char] into +0x36; 0x1883C(side, +0x34's word, +0x36's word); then
 * 0x2BDE8 on the own record and, with the other side's slot set, its record. */
void fighter_36114(u32 rec)
{
    u32 slot, side, other;
    s32 dx;
    p2_set_f32(rec + 0x24u, 2.0f);                          /* 0x3611A */
    slot = DSD(rec + 0x14u);                                /* 0x36121 */
    if (slot == 0u) return;                                 /* 0x36124/0x36126 */
    DSB(slot + 0x58u) = (u8)(DSB(slot + 0x58u) + 1u);       /* 0x3612C/0x3612F/0x36135 */
    side = (u32)DSB(rec + 0x51u);                           /* 0x36131 */
    if ((DSW(rec + 0x28u) & 0x4000u) != 0u)                 /* 0x36138..0x36147 */
        dx = (s32)(s16)DSW(0x000BDA4Cu + (u32)DSB(slot + 0x7Au) * 2u);   /* 0x36158..0x3615D */
    else
        dx = -(s32)(s16)DSW(0x000BDA4Cu + (u32)DSB(slot + 0x7Au) * 2u);  /* 0x36149..0x36156 */
    DSW(rec + 0x34u) = (u16)dx;                             /* 0x36165 */
    DSW(rec + 0x36u) = DSW(0x000BDA5Au + (u32)DSB(slot + 0x7Au) * 2u);   /* 0x36169..0x3617C */
    fighter_1883c(side, (u32)((s32)DSD(rec + 0x32u) >> 16),
                  (u32)((s32)DSD(rec + 0x34u) >> 16));      /* 0x36179..0x3618B */
    other = DSD(DS_001077A8 + (side ^ 1u) * 4u);            /* 0x36190..0x3619A */
    if (other == 0u) return;                                /* 0x361A1/0x361A3 */
    fighter_2bde8(rec);                                     /* 0x361A5/0x361A7 */
    fighter_2bde8(DSD(other));                              /* 0x361AC/0x361AE */
}


''')

sub('port/src/game/fighter.h', r''' * unported direct callees and the targets outside E2. */
void fighter_2bdb8(u32 rec, u32 arg);
void fighter_213f0(u32 rec);
void fighter_213f4(u32 rec);
void fighter_3e424(u32 rec);
''', r''' * unported direct callees and the targets outside E2. */
void fighter_2bdb8(u32 rec, u32 arg);
void fighter_213f0(u32 rec);
void fighter_213f4(u32 rec);
void fighter_3e424(u32 rec);
void fighter_224ec(u32 rec);
void fighter_2bde8(u32 rec);
void fighter_36114(u32 rec);
''')

sub('port/src/game/actors.c', r'''static void anim_code_3E424(u32 rec, u32 arg)
{
    (void)arg;
    fighter_3e424(rec);
}
''', r'''static void anim_code_3E424(u32 rec, u32 arg)
{
    (void)arg;
    fighter_3e424(rec);
}
/* PORT: record 2026-10-04-reverse-p7 §P7.3. Track P batch 7's Task 3
 * animation targets. */
static void anim_code_224EC(u32 rec, u32 arg)
{
    (void)arg;
    fighter_224ec(rec);
}
static void anim_code_36114(u32 rec, u32 arg)
{
    (void)arg;
    fighter_36114(rec);
}


/* PORT: record 2026-10-04-reverse-p7 ''')

sub('port/src/game/actors.c', r'''    fn_register(0x24220u, (void (*)(void))fighter_24220_case10);
    /* PORT: record 2026-10-04-reverse-p7 §P7.2. Track P batch 7's animation
     * targets (anim_indirect, EAX = rec, EDX = the operand). */
    fn_register(0x213F0u, (void (*)(void))anim_code_213F0);
    fn_register(0x213F4u, (void (*)(void))anim_code_213F4);
    fn_register(0x3E424u, (void (*)(void))anim_code_3E424);
    ''', r'''    fn_register(0x24220u, (void (*)(void))fighter_24220_case10);
    /* PORT: record 2026-10-04-reverse-p7 §P7.2. Track P batch 7's animation
     * targets (anim_indirect, EAX = rec, EDX = the operand). */
    fn_register(0x213F0u, (void (*)(void))anim_code_213F0);
    fn_register(0x213F4u, (void (*)(void))anim_code_213F4);
    fn_register(0x3E424u, (void (*)(void))anim_code_3E424);
    /* PORT: record 2026-10-04-reverse-p7 §P7.3. Track P batch 7's Task 3
     * animation targets. */
    fn_register(0x224ECu, (void (*)(void))anim_code_224EC);
    fn_register(0x36114u, (void (*)(void))anim_code_36114);
    /* PORT: record 2026-10-04-reverse-p7 ''')

sub('tools/diff_verify.py', r'''
# ---- track P batch 7: the remaining unported direct callees and the targets outside E2 (record
# 2026-10-04-reverse-p7) --------------------------------------------------------------------------------
# The callees the batch stubs, args in the port's C order and clobbers from E.callee_clobbers.
P7_2BDB8 = E.Call(0x2BDB8, ("eax", "edx"), clobbers=("edx",))
P7_2BDB8_R = E.Call(0x2BDB8, ("eax", "edx"), mode="real")
P7_2BDE8_R = E.Call(0x2BDE8, ("eax",), mode="real")
P7_23960 = E.Call(0x23960, ("eax",))
P7_3A9D8 = E.Call(0x3A9D8, ("eax", "edx"), clobbers=("edx",))
P7_2B150 = E.Call(0x2B150, ("eax",), clobbers=("esi", "edi", "ebp"))
P7_41310 = E.Call(0x41310, ("eax", "edx"))
P7_49444 = E.Call(0x49444, ("eax",), clobbers=("edi", "ebp"))
P7_RNG   = E.Call(0x5D7DC, ("eax",), mode="real")
P7_22404 = E.Call(0x22404, ("eax",))
P7_2BCF4 = E.Call(0x2BCF4, ("eax", "edx"), clobbers=("edx",))
P7_2A17C = E.Call(0x2A17C, ("eax", "edx", "ebx"), clobbers=("edx",))

# 0x2BDB8 (record §P7.2): the record's +0x2B bit 1, the byte 0x105BEE = the argument, 0x105BEC =
# the argument - 1 and +0x24 = (float)(u8)the argument. b1 (0x100) separates the low byte from the
# whole argument for all three stores; b2 (0x1234FF) pins 255.0f.
P7_2BDB8_SEED = {E3_REC + 0x24: le32(0x24242424), E3_REC + 0x2B: b"\x2b\x2c",
                 0x105BEE: b"\xee\xef", 0x105BEC: b"\xec\xed"}
P7_2BDB8_SEED2 = {E3_REC2 + 0x24: le32(0x34343434), E3_REC2 + 0x2B: b"\x3b\x3c",
                  0x105BEE: b"\xee\xef", 0x105BEC: b"\xec\xed"}


# 0x213F4/0x3E424 (record §P7.2): the other side's slot by rec+0x51 ^ 1; its record on
# 0xC9148/0xC9120[its char] at 3.0; its +0x58 = 0; 0x3E424's +0x41 |= 0x80; its +0x52 = 0xC; then
# 0x2BDB8(3) on the own and the other record. The records and the +0x2B/0x24 stores carry sentinels.
def p7_213f4_case(cid, rec, orec, oside, char):
    oslot = DS_SLOTS + oside * 0x94
    return Case(cid, {"eax": rec, "edx": 0x1234},
                {**SLOT_PTRS, **P6_PTRS,
                 rec + 0x51: bytes([oside ^ 1]), orec + 0x51: bytes([oside]),
                 oslot + 0x7A: bytes([char]), oslot + 0x58: b"\x58\x59",
                 oslot + 0x52: b"\x52\x53", oslot + 0x41: b"\x41\x42",
                 rec + 0x24: le32(0x24242424), rec + 0x2B: b"\x2b\x2c",
                 orec + 0x24: le32(0x34343434), orec + 0x2B: b"\x3b\x3c",
                 0x105BEE: b"\xee\xef", 0x105BEC: b"\xec\xed"})


# 0x23960 (record §P7.4): 0x105B4C = 0; with the other side's slot set, four 0xA839C spawns at its
# +0x2C + 0x600/-0x600/+0x1000/-0x1000, y = the other record's +0x30 >> 16; each child's +0x24 +=
# (float)rng_next(6) and +0x20 the same. The spawn stub returns P6_CHILD.
P7_23960_SEED = {**SLOT_PTRS, **P6_PTRS,
                 E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
                 DS_SLOTS + 0x94 + 0x2C: le32(0x00001000),
                 E3_REC2 + 0x30: le32(0x12345678),
                 P6_CHILD + 0x20: le32(0x20202020), P6_CHILD + 0x24: le32(0x40400000),
                 0x105B4C: b"\x4c\x4d"}

# 0x23AE0 (record §P7.6): the other slot's record's +0x29 bit 3, +0x2E = 0x64, +0x4E = 1; the
# char's word from 0x23AC4 (0x46B6..0x46BA, default 0x46B9) sought on it; the palette handle
# 0x105FEBC; the word 0x105B4C = 1.
def p7_23ae0_case(cid, rec, char):
    return Case(cid, {"eax": rec, "edx": 0x1234},
                {**SLOT_PTRS, **P6_PTRS,
                 rec + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
                 DS_SLOTS + 0x94 + 0x7A: bytes([char]), E3_REC2 + 0x29: b"\x29\x2a",
                 E3_REC2 + 0x2E: b"\x2e\x2f", E3_REC2 + 0x4E: b"\x4e\x4f",
                 0x105B4C: b"\x4c\x4d"})


# 0x4B03C (record §P7.7): the byte (slot+8's record +0x48) - 0x20 selects 0x10/0xE/0x15/0xC/0xA
# (0xD default) and the slot's +0x5A loses it to 0; voice 0xD6, voice 0xCE, the record dead,
# 0x41310 on the slot's +0x21/+0x20 (mode 0x22/0x24 excluded), the 0x1088A4/0x1088A2 counters,
# then 0x49444.
P7_4B03C_SEED = {E3_REC + 0x14: le32(E3_SLOT), E3_SLOT + 8: le32(E3_OUT),
                 E3_SLOT + 0x20: b"\x00", E3_SLOT + 0x21: b"\x01",
                 0x10780A: b"\x5a", 0x10780A + 0x94: b"\x5b",
                 0x10889E + 1: b"\x9e", 0x1088A4: b"\xa4", 0x1088A2 + 1: b"\xa2",
                 0x104B00: b"\x11\x00", E3_OUT + 0x48: b"\x20"}

P7_SPECS = [
    Spec("fighter_2bdb8", 0x2BDB8, [
        Case("b0", {"eax": E3_REC, "edx": 0x42}, P7_2BDB8_SEED),
        Case("b1", {"eax": E3_REC, "edx": 0x100}, P7_2BDB8_SEED),
        Case("b2", {"eax": E3_REC2, "edx": 0x1234FF}, P7_2BDB8_SEED2),
    ], eax_mask=0, mutants=("@mutant", "@and", "@dec", "@width")),
    # 0x213F0 (record §P7.2): a bare `ret`; the row asserts no byte changes.
    Spec("fighter_213f0", 0x213F0, [
        Case("r0", {"eax": E3_REC, "edx": 0x1234}, {E3_REC + 0x2B: b"\x28"}),
    ], eax_mask=0, mutants=("@mutant",)),
    Spec("fighter_213f4", 0x213F4, [
        p7_213f4_case("s0", E3_REC, E3_REC2, 0, 3),
        p7_213f4_case("s1", E3_REC2, E3_REC, 1, 2),
        Case("o0", {"eax": E3_REC, "edx": 0x1234},
             {DS_SLOTS - 8: le32(DS_SLOTS) + le32(0), E3_REC + 0x51: b"\x00",
              E3_REC + 0x24: le32(0x24242424), E3_REC + 0x2B: b"\x2b\x2c",
              0x105BEE: b"\xee\xef", 0x105BEC: b"\xec\xed"}),
    ], calls=(ANIM_BEGIN, P7_2BDB8_R), eax_mask=0,
       mutants=("@mutant", "@stream", "@side", "@order")),
    Spec("fighter_3e424", 0x3E424, [
        p7_213f4_case("s0", E3_REC, E3_REC2, 0, 3),
        p7_213f4_case("s1", E3_REC2, E3_REC, 1, 2),
        Case("o0", {"eax": E3_REC, "edx": 0x1234},
             {DS_SLOTS - 8: le32(DS_SLOTS) + le32(0), E3_REC + 0x51: b"\x00",
              E3_REC + 0x24: le32(0x24242424), E3_REC + 0x2B: b"\x2b\x2c",
              0x105BEE: b"\xee\xef", 0x105BEC: b"\xec\xed"}),
    ], calls=(ANIM_BEGIN, P7_2BDB8_R), eax_mask=0,
       mutants=("@mutant", "@bit", "@side", "@order")),
    ''', r'''
# ---- track P batch 7: the remaining unported direct callees and the targets outside E2 (record
# 2026-10-04-reverse-p7) --------------------------------------------------------------------------------
# The callees the batch stubs, args in the port's C order and clobbers from E.callee_clobbers.
P7_2BDB8 = E.Call(0x2BDB8, ("eax", "edx"), clobbers=("edx",))
P7_2BDB8_R = E.Call(0x2BDB8, ("eax", "edx"), mode="real")
P7_2BDE8_R = E.Call(0x2BDE8, ("eax",), mode="real")
P7_23960 = E.Call(0x23960, ("eax",))
P7_3A9D8 = E.Call(0x3A9D8, ("eax", "edx"), clobbers=("edx",))
P7_2B150 = E.Call(0x2B150, ("eax",), clobbers=("esi", "edi", "ebp"))
P7_41310 = E.Call(0x41310, ("eax", "edx"))
P7_49444 = E.Call(0x49444, ("eax",), clobbers=("edi", "ebp"))
P7_RNG   = E.Call(0x5D7DC, ("eax",), mode="real")
P7_22404 = E.Call(0x22404, ("eax",))
P7_2BCF4 = E.Call(0x2BCF4, ("eax", "edx"), clobbers=("edx",))
P7_2A17C = E.Call(0x2A17C, ("eax", "edx", "ebx"), clobbers=("edx",))

# 0x2BDB8 (record §P7.2): the record's +0x2B bit 1, the byte 0x105BEE = the argument, 0x105BEC =
# the argument - 1 and +0x24 = (float)(u8)the argument. b1 (0x100) separates the low byte from the
# whole argument for all three stores; b2 (0x1234FF) pins 255.0f.
P7_2BDB8_SEED = {E3_REC + 0x24: le32(0x24242424), E3_REC + 0x2B: b"\x2b\x2c",
                 0x105BEE: b"\xee\xef", 0x105BEC: b"\xec\xed"}
P7_2BDB8_SEED2 = {E3_REC2 + 0x24: le32(0x34343434), E3_REC2 + 0x2B: b"\x3b\x3c",
                  0x105BEE: b"\xee\xef", 0x105BEC: b"\xec\xed"}


# 0x213F4/0x3E424 (record §P7.2): the other side's slot by rec+0x51 ^ 1; its record on
# 0xC9148/0xC9120[its char] at 3.0; its +0x58 = 0; 0x3E424's +0x41 |= 0x80; its +0x52 = 0xC; then
# 0x2BDB8(3) on the own and the other record. The records and the +0x2B/0x24 stores carry sentinels.
def p7_213f4_case(cid, rec, orec, oside, char):
    oslot = DS_SLOTS + oside * 0x94
    return Case(cid, {"eax": rec, "edx": 0x1234},
                {**SLOT_PTRS, **P6_PTRS,
                 rec + 0x51: bytes([oside ^ 1]), orec + 0x51: bytes([oside]),
                 oslot + 0x7A: bytes([char]), oslot + 0x58: b"\x58\x59",
                 oslot + 0x52: b"\x52\x53", oslot + 0x41: b"\x41\x42",
                 rec + 0x24: le32(0x24242424), rec + 0x2B: b"\x2b\x2c",
                 orec + 0x24: le32(0x34343434), orec + 0x2B: b"\x3b\x3c",
                 0x105BEE: b"\xee\xef", 0x105BEC: b"\xec\xed"})


# 0x23960 (record §P7.4): 0x105B4C = 0; with the other side's slot set, four 0xA839C spawns at its
# +0x2C + 0x600/-0x600/+0x1000/-0x1000, y = the other record's +0x30 >> 16; each child's +0x24 +=
# (float)rng_next(6) and +0x20 the same. The spawn stub returns P6_CHILD.
P7_23960_SEED = {**SLOT_PTRS, **P6_PTRS,
                 E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
                 DS_SLOTS + 0x94 + 0x2C: le32(0x00001000),
                 E3_REC2 + 0x30: le32(0x12345678),
                 P6_CHILD + 0x20: le32(0x20202020), P6_CHILD + 0x24: le32(0x40400000),
                 0x105B4C: b"\x4c\x4d"}

# 0x23AE0 (record §P7.6): the other slot's record's +0x29 bit 3, +0x2E = 0x64, +0x4E = 1; the
# char's word from 0x23AC4 (0x46B6..0x46BA, default 0x46B9) sought on it; the palette handle
# 0x105FEBC; the word 0x105B4C = 1.
def p7_23ae0_case(cid, rec, char):
    return Case(cid, {"eax": rec, "edx": 0x1234},
                {**SLOT_PTRS, **P6_PTRS,
                 rec + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
                 DS_SLOTS + 0x94 + 0x7A: bytes([char]), E3_REC2 + 0x29: b"\x29\x2a",
                 E3_REC2 + 0x2E: b"\x2e\x2f", E3_REC2 + 0x4E: b"\x4e\x4f",
                 0x105B4C: b"\x4c\x4d"})


# 0x4B03C (record §P7.7): the byte (slot+8's record +0x48) - 0x20 selects 0x10/0xE/0x15/0xC/0xA
# (0xD default) and the slot's +0x5A loses it to 0; voice 0xD6, voice 0xCE, the record dead,
# 0x41310 on the slot's +0x21/+0x20 (mode 0x22/0x24 excluded), the 0x1088A4/0x1088A2 counters,
# then 0x49444.
P7_4B03C_SEED = {E3_REC + 0x14: le32(E3_SLOT), E3_SLOT + 8: le32(E3_OUT),
                 E3_SLOT + 0x20: b"\x00", E3_SLOT + 0x21: b"\x01",
                 0x10780A: b"\x5a", 0x10780A + 0x94: b"\x5b",
                 0x10889E + 1: b"\x9e", 0x1088A4: b"\xa4", 0x1088A2 + 1: b"\xa2",
                 0x104B00: b"\x11\x00", E3_OUT + 0x48: b"\x20"}

P7_SPECS = [
    Spec("fighter_2bdb8", 0x2BDB8, [
        Case("b0", {"eax": E3_REC, "edx": 0x42}, P7_2BDB8_SEED),
        Case("b1", {"eax": E3_REC, "edx": 0x100}, P7_2BDB8_SEED),
        Case("b2", {"eax": E3_REC2, "edx": 0x1234FF}, P7_2BDB8_SEED2),
    ], eax_mask=0, mutants=("@mutant", "@and", "@dec", "@width")),
    # 0x213F0 (record §P7.2): a bare `ret`; the row asserts no byte changes.
    Spec("fighter_213f0", 0x213F0, [
        Case("r0", {"eax": E3_REC, "edx": 0x1234}, {E3_REC + 0x2B: b"\x28"}),
    ], eax_mask=0, mutants=("@mutant",)),
    Spec("fighter_213f4", 0x213F4, [
        p7_213f4_case("s0", E3_REC, E3_REC2, 0, 3),
        p7_213f4_case("s1", E3_REC2, E3_REC, 1, 2),
        Case("o0", {"eax": E3_REC, "edx": 0x1234},
             {DS_SLOTS - 8: le32(DS_SLOTS) + le32(0), E3_REC + 0x51: b"\x00",
              E3_REC + 0x24: le32(0x24242424), E3_REC + 0x2B: b"\x2b\x2c",
              0x105BEE: b"\xee\xef", 0x105BEC: b"\xec\xed"}),
    ], calls=(ANIM_BEGIN, P7_2BDB8_R), eax_mask=0,
       mutants=("@mutant", "@stream", "@side", "@order")),
    Spec("fighter_3e424", 0x3E424, [
        p7_213f4_case("s0", E3_REC, E3_REC2, 0, 3),
        p7_213f4_case("s1", E3_REC2, E3_REC, 1, 2),
        Case("o0", {"eax": E3_REC, "edx": 0x1234},
             {DS_SLOTS - 8: le32(DS_SLOTS) + le32(0), E3_REC + 0x51: b"\x00",
              E3_REC + 0x24: le32(0x24242424), E3_REC + 0x2B: b"\x2b\x2c",
              0x105BEE: b"\xee\xef", 0x105BEC: b"\xec\xed"}),
    ], calls=(ANIM_BEGIN, P7_2BDB8_R), eax_mask=0,
       mutants=("@mutant", "@bit", "@side", "@order")),
    # 0x224EC (record §P7.3): ctx; 0x22404(ctx[0]); the own slot's +0x57 = 2. Both slots' +0x57
    # are seeded differently, so @side (ctx[3]) shows.
    Spec("fighter_224ec", 0x224EC, [
        Case("s0", {"eax": E3_REC, "edx": 0x1234},
             {**SLOT_PTRS, E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
              DS_SLOTS + 0x57: b"\x57", DS_SLOTS + 0x94 + 0x57: b"\x67"}),
        Case("s1", {"eax": E3_REC2, "edx": 0x5678},
             {**SLOT_PTRS, E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
              DS_SLOTS + 0x57: b"\x57", DS_SLOTS + 0x94 + 0x57: b"\x67"}),
    ], allow_calls=(0x339AC,), calls=(P7_22404,), eax_mask=0,
       mutants=("@mutant", "@side", "@order")),
    # 0x2BDE8 (record §P7.3): +0x20 = +0x24 + (-1.0f, 0x809B8); +0x2B bit 1 clear. s1's +0x2B
    # has only bit 1 set (the clear observable), s0's 0xFF and s2's 0x03.
    Spec("fighter_2bde8", 0x2BDE8, [
        Case("s0", {"eax": E3_REC}, {E3_REC + 0x20: le32(0x20202020), E3_REC + 0x24: le32(0x40400000),
                                     E3_REC + 0x2B: b"\xff\x2c", E3_REC + 0x2A: b"\x2a"}),
        Case("s1", {"eax": E3_REC2}, {E3_REC2 + 0x20: le32(0x20202020), E3_REC2 + 0x24: le32(0xC0000000),
                                      E3_REC2 + 0x2B: b"\x02\x3c", E3_REC2 + 0x2A: b"\x3a"}),
        Case("s2", {"eax": E3_REC}, {E3_REC + 0x20: le32(0x20202020), E3_REC + 0x24: le32(0x3F800000),
                                     E3_REC + 0x2B: b"\x03\x2c", E3_REC + 0x2A: b"\x2a"}),
    ], eax_mask=0, mutants=("@mutant", "@byte")),
    # 0x36114 (record §P7.3): +0x24 = 2.0; the slot's +0x58 + 1; the signed word 0xBDA4C[char]
    # negated unless the record's +0x28 bit 14; 0xBDA5A[char] into +0x36; 0x1883C(side, +0x34's
    # word, +0x36's word); then 0x2BDE8 on the own and the other record (when set).
    Spec("fighter_36114", 0x36114, [
        Case("s0", {"eax": E3_REC, "edx": 0x1234},
             {**SLOT_PTRS, **P6_PTRS, E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
              E3_REC + 0x14: le32(DS_SLOTS), E3_REC + 0x28: b"\x00\x40",
              E3_REC + 0x24: le32(0x24242424), E3_REC + 0x32: b"\x32\x33\x34\x35\x36\x37",
              E3_REC + 0x20: le32(0x20202020), E3_REC + 0x2B: b"\x2b\x2c",
              E3_REC2 + 0x20: le32(0x40404040), E3_REC2 + 0x24: le32(0x44444444),
              E3_REC2 + 0x2B: b"\x4b\x4c", DS_SLOTS + 0x7A: b"\x03", DS_SLOTS + 0x58: b"\x58\x59"}),
        Case("s1", {"eax": E3_REC, "edx": 0x1234},
             {**SLOT_PTRS, **P6_PTRS, E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
              E3_REC + 0x14: le32(DS_SLOTS), E3_REC + 0x28: b"\x00\x00",
              E3_REC + 0x24: le32(0x24242424), E3_REC + 0x32: b"\x32\x33\x34\x35\x36\x37",
              E3_REC + 0x20: le32(0x20202020), E3_REC + 0x2B: b"\x2b\x2c",
              E3_REC2 + 0x20: le32(0x40404040), E3_REC2 + 0x24: le32(0x44444444),
              E3_REC2 + 0x2B: b"\x4b\x4c", DS_SLOTS + 0x7A: b"\x03", DS_SLOTS + 0x58: b"\x58\x59"}),
        Case("s2", {"eax": E3_REC2, "edx": 0x5678},
             {**SLOT_PTRS, **P6_PTRS, E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
              E3_REC2 + 0x14: le32(DS_SLOTS + 0x94), E3_REC2 + 0x28: b"\x00\x40",
              E3_REC2 + 0x24: le32(0x44444444), E3_REC2 + 0x32: b"\x42\x43\x44\x45\x46\x47",
              E3_REC2 + 0x20: le32(0x40404040), E3_REC2 + 0x2B: b"\x4b\x4c",
              E3_REC + 0x20: le32(0x20202020), E3_REC + 0x24: le32(0x24242424),
              E3_REC + 0x2B: b"\x2b\x2c", DS_SLOTS + 0x94 + 0x7A: b"\x02",
              DS_SLOTS + 0x94 + 0x58: b"\x68\x69"}),
        Case("n0", {"eax": E3_REC, "edx": 0x1234},
             {E3_REC + 0x14: le32(0), E3_REC + 0x24: le32(0x24242424), E3_REC + 0x2B: b"\x2b\x2c"}),
        Case("o0", {"eax": E3_REC, "edx": 0x1234},
             {DS_SLOTS - 8: le32(DS_SLOTS) + le32(0), E3_REC + 0x51: b"\x00",
              E3_REC + 0x14: le32(DS_SLOTS), E3_REC + 0x28: b"\x00\x40",
              E3_REC + 0x24: le32(0x24242424), E3_REC + 0x32: b"\x32\x33\x34\x35\x36\x37",
              E3_REC + 0x20: le32(0x20202020), E3_REC + 0x2B: b"\x2b\x2c",
              DS_SLOTS + 0x7A: b"\x03", DS_SLOTS + 0x58: b"\x58\x59"}),
    ], calls=(C1_1883C, P7_2BDE8_R), eax_mask=0,
       mutants=("@neg", "@side", "@anchor", "@char", "@table", "@order", "@second")),
    ''')

sub('port/tests/diff_runner.c', r'''        actors_anim_begin(DSD(other), DSD(0x000C9120u + (u32)DSB(other + 0x7Au) * 4u), 0x40400000u);
        fighter_2bdb8(rec, 3u);
        fighter_2bdb8(DSD(other), 3u);
    }
    *eax = 0u;
}
''', r'''        actors_anim_begin(DSD(other), DSD(0x000C9120u + (u32)DSB(other + 0x7Au) * 4u), 0x40400000u);
        fighter_2bdb8(rec, 3u);
        fighter_2bdb8(DSD(other), 3u);
    }
    *eax = 0u;
}
static void b_224ec(const u32 *r, u32 *eax)            { fighter_224ec(r[R_EAX]); *eax = 0u; }
static void m_224ec(const u32 *r, u32 *eax)            /* the +0x57 value 3 */
{
    u32 ctx[6];
    hit_anim_ctx(ctx, r[R_EAX]);
    fighter_22404(ctx[0]);
    DSB(ctx[2] + 0x57u) = 3u;
    *eax = 0u;
}
static void m_224ec_side(const u32 *r, u32 *eax)       /* the other slot (ctx[3]) */
{
    u32 ctx[6];
    hit_anim_ctx(ctx, r[R_EAX]);
    fighter_22404(ctx[0]);
    DSB(ctx[3] + 0x57u) = 2u;
    *eax = 0u;
}
static void m_224ec_order(const u32 *r, u32 *eax)      /* the store before the 0x22404 call */
{
    u32 ctx[6];
    hit_anim_ctx(ctx, r[R_EAX]);
    DSB(ctx[2] + 0x57u) = 2u;
    fighter_22404(ctx[0]);
    *eax = 0u;
}
static void b_2bde8(const u32 *r, u32 *eax)            { fighter_2bde8(r[R_EAX]); *eax = 0u; }
static void m_2bde8(const u32 *r, u32 *eax)            /* +1.0f, not -1.0f */
{
    union { float f; u32 u; } v;
    u32 rec = r[R_EAX];
    v.u = DSD(rec + 0x24u);
    v.f = v.f + 1.0f;
    DSD(rec + 0x20u) = v.u;
    DSB(rec + 0x2Bu) &= (u8)~2u;
    *eax = 0u;
}
static void m_2bde8_byte(const u32 *r, u32 *eax)       /* +0x2B = 0xFD, not the AND */
{
    union { float f; u32 u; } v;
    u32 rec = r[R_EAX];
    v.u = DSD(rec + 0x24u);
    v.f = v.f + -1.0f;
    DSD(rec + 0x20u) = v.u;
    DSB(rec + 0x2Bu) = 0xFDu;
    *eax = 0u;
}
static void b_36114(const u32 *r, u32 *eax)            { fighter_36114(r[R_EAX]); *eax = 0u; }
static void m_36114_neg(const u32 *r, u32 *eax)        /* the bit-14 branch inverted */
{
    u32 rec = r[R_EAX];
    u32 slot = DSD(rec + 0x14u);
    u32 side;
    s32 dx;
    if (slot == 0u) { *eax = 0u; return; }
    DSD(rec + 0x24u) = 0x40000000u;
    DSB(slot + 0x58u) = (u8)(DSB(slot + 0x58u) + 1u);
    side = (u32)DSB(rec + 0x51u);
    if ((DSW(rec + 0x28u) & 0x4000u) == 0u)
        dx = (s32)(s16)DSW(0x000BDA4Cu + (u32)DSB(slot + 0x7Au) * 2u);
    else
        dx = -(s32)(s16)DSW(0x000BDA4Cu + (u32)DSB(slot + 0x7Au) * 2u);
    DSW(rec + 0x34u) = (u16)dx;
    DSW(rec + 0x36u) = DSW(0x000BDA5Au + (u32)DSB(slot + 0x7Au) * 2u);
    fighter_1883c(side, (u32)((s32)DSD(rec + 0x32u) >> 16), (u32)((s32)DSD(rec + 0x34u) >> 16));
    {
        u32 other = DSD(DS_001077A8 + (side ^ 1u) * 4u);
        if (other != 0u) {
            fighter_2bde8(rec);
            fighter_2bde8(DSD(other));
        }
    }
    *eax = 0u;
}
static void m_36114_side(const u32 *r, u32 *eax)       /* the 0x1883C side from +0x51 ^ 1 */
{
    u32 rec = r[R_EAX];
    u32 slot = DSD(rec + 0x14u);
    u32 side;
    s32 dx;
    if (slot == 0u) { *eax = 0u; return; }
    DSD(rec + 0x24u) = 0x40000000u;
    DSB(slot + 0x58u) = (u8)(DSB(slot + 0x58u) + 1u);
    side = (u32)DSB(rec + 0x51u);
    if ((DSW(rec + 0x28u) & 0x4000u) != 0u)
        dx = (s32)(s16)DSW(0x000BDA4Cu + (u32)DSB(slot + 0x7Au) * 2u);
    else
        dx = -(s32)(s16)DSW(0x000BDA4Cu + (u32)DSB(slot + 0x7Au) * 2u);
    DSW(rec + 0x34u) = (u16)dx;
    DSW(rec + 0x36u) = DSW(0x000BDA5Au + (u32)DSB(slot + 0x7Au) * 2u);
    fighter_1883c(side ^ 1u, (u32)((s32)DSD(rec + 0x32u) >> 16), (u32)((s32)DSD(rec + 0x34u) >> 16));
    {
        u32 other = DSD(DS_001077A8 + (side ^ 1u) * 4u);
        if (other != 0u) {
            fighter_2bde8(rec);
            fighter_2bde8(DSD(other));
        }
    }
    *eax = 0u;
}
static void m_36114_anchor(const u32 *r, u32 *eax)     /* the anchor y from +0x36 */
{
    u32 rec = r[R_EAX];
    u32 slot = DSD(rec + 0x14u);
    u32 side;
    s32 dx;
    if (slot == 0u) { *eax = 0u; return; }
    DSD(rec + 0x24u) = 0x40000000u;
    DSB(slot + 0x58u) = (u8)(DSB(slot + 0x58u) + 1u);
    side = (u32)DSB(rec + 0x51u);
    if ((DSW(rec + 0x28u) & 0x4000u) != 0u)
        dx = (s32)(s16)DSW(0x000BDA4Cu + (u32)DSB(slot + 0x7Au) * 2u);
    else
        dx = -(s32)(s16)DSW(0x000BDA4Cu + (u32)DSB(slot + 0x7Au) * 2u);
    DSW(rec + 0x34u) = (u16)dx;
    DSW(rec + 0x36u) = DSW(0x000BDA5Au + (u32)DSB(slot + 0x7Au) * 2u);
    fighter_1883c(side, (u32)((s32)DSD(rec + 0x34u) >> 16), (u32)((s32)DSD(rec + 0x36u) >> 16));
    {
        u32 other = DSD(DS_001077A8 + (side ^ 1u) * 4u);
        if (other != 0u) {
            fighter_2bde8(rec);
            fighter_2bde8(DSD(other));
        }
    }
    *eax = 0u;
}
static void m_36114_char(const u32 *r, u32 *eax)       /* the own record's char */
{
    u32 rec = r[R_EAX];
    u32 slot = DSD(rec + 0x14u);
    u32 side;
    s32 dx;
    if (slot == 0u) { *eax = 0u; return; }
    DSD(rec + 0x24u) = 0x40000000u;
    DSB(slot + 0x58u) = (u8)(DSB(slot + 0x58u) + 1u);
    side = (u32)DSB(rec + 0x51u);
    if ((DSW(rec + 0x28u) & 0x4000u) != 0u)
        dx = (s32)(s16)DSW(0x000BDA4Cu + (u32)DSB(rec + 0x7Au) * 2u);
    else
        dx = -(s32)(s16)DSW(0x000BDA4Cu + (u32)DSB(rec + 0x7Au) * 2u);
    DSW(rec + 0x34u) = (u16)dx;
    DSW(rec + 0x36u) = DSW(0x000BDA5Au + (u32)DSB(rec + 0x7Au) * 2u);
    fighter_1883c(side, (u32)((s32)DSD(rec + 0x32u) >> 16), (u32)((s32)DSD(rec + 0x34u) >> 16));
    {
        u32 other = DSD(DS_001077A8 + (side ^ 1u) * 4u);
        if (other != 0u) {
            fighter_2bde8(rec);
            fighter_2bde8(DSD(other));
        }
    }
    *eax = 0u;
}
static void m_36114_table(const u32 *r, u32 *eax)      /* 0xBDA5A for +0x34 */
{
    u32 rec = r[R_EAX];
    u32 slot = DSD(rec + 0x14u);
    u32 side;
    s32 dx;
    if (slot == 0u) { *eax = 0u; return; }
    DSD(rec + 0x24u) = 0x40000000u;
    DSB(slot + 0x58u) = (u8)(DSB(slot + 0x58u) + 1u);
    side = (u32)DSB(rec + 0x51u);
    if ((DSW(rec + 0x28u) & 0x4000u) != 0u)
        dx = (s32)(s16)DSW(0x000BDA5Au + (u32)DSB(slot + 0x7Au) * 2u);
    else
        dx = -(s32)(s16)DSW(0x000BDA5Au + (u32)DSB(slot + 0x7Au) * 2u);
    DSW(rec + 0x34u) = (u16)dx;
    DSW(rec + 0x36u) = DSW(0x000BDA5Au + (u32)DSB(slot + 0x7Au) * 2u);
    fighter_1883c(side, (u32)((s32)DSD(rec + 0x32u) >> 16), (u32)((s32)DSD(rec + 0x34u) >> 16));
    {
        u32 other = DSD(DS_001077A8 + (side ^ 1u) * 4u);
        if (other != 0u) {
            fighter_2bde8(rec);
            fighter_2bde8(DSD(other));
        }
    }
    *eax = 0u;
}
static void m_36114_order(const u32 *r, u32 *eax)      /* the +0x34/+0x36 stores after the call */
{
    u32 rec = r[R_EAX];
    u32 slot = DSD(rec + 0x14u);
    u32 side;
    s32 dx;
    if (slot == 0u) { *eax = 0u; return; }
    DSD(rec + 0x24u) = 0x40000000u;
    DSB(slot + 0x58u) = (u8)(DSB(slot + 0x58u) + 1u);
    side = (u32)DSB(rec + 0x51u);
    if ((DSW(rec + 0x28u) & 0x4000u) != 0u)
        dx = (s32)(s16)DSW(0x000BDA4Cu + (u32)DSB(slot + 0x7Au) * 2u);
    else
        dx = -(s32)(s16)DSW(0x000BDA4Cu + (u32)DSB(slot + 0x7Au) * 2u);
    fighter_1883c(side, (u32)((s32)DSD(rec + 0x32u) >> 16), (u32)((s32)DSD(rec + 0x34u) >> 16));
    DSW(rec + 0x34u) = (u16)dx;
    DSW(rec + 0x36u) = DSW(0x000BDA5Au + (u32)DSB(slot + 0x7Au) * 2u);
    {
        u32 other = DSD(DS_001077A8 + (side ^ 1u) * 4u);
        if (other != 0u) {
            fighter_2bde8(rec);
            fighter_2bde8(DSD(other));
        }
    }
    *eax = 0u;
}
static void m_36114_second(const u32 *r, u32 *eax)     /* only the own 0x2BDE8 */
{
    u32 rec = r[R_EAX];
    u32 slot = DSD(rec + 0x14u);
    u32 side;
    s32 dx;
    if (slot == 0u) { *eax = 0u; return; }
    DSD(rec + 0x24u) = 0x40000000u;
    DSB(slot + 0x58u) = (u8)(DSB(slot + 0x58u) + 1u);
    side = (u32)DSB(rec + 0x51u);
    if ((DSW(rec + 0x28u) & 0x4000u) != 0u)
        dx = (s32)(s16)DSW(0x000BDA4Cu + (u32)DSB(slot + 0x7Au) * 2u);
    else
        dx = -(s32)(s16)DSW(0x000BDA4Cu + (u32)DSB(slot + 0x7Au) * 2u);
    DSW(rec + 0x34u) = (u16)dx;
    DSW(rec + 0x36u) = DSW(0x000BDA5Au + (u32)DSB(slot + 0x7Au) * 2u);
    fighter_1883c(side, (u32)((s32)DSD(rec + 0x32u) >> 16), (u32)((s32)DSD(rec + 0x34u) >> 16));
    fighter_2bde8(rec);
    *eax = 0u;
}
''')

sub('port/tests/diff_runner.c', r'''    { "fighter_24220@decr",       m_24220_decr,   0x00000000u },
    { "fighter_2bdb8",            b_2bdb8,        0x00000000u },
    { "fighter_2bdb8@mutant",     m_2bdb8,        0x00000000u },
    { "fighter_2bdb8@and",        m_2bdb8_and,    0x00000000u },
    { "fighter_2bdb8@dec",        m_2bdb8_dec,    0x00000000u },
    { "fighter_2bdb8@width",      m_2bdb8_width,  0x00000000u },
    { "fighter_213f0",            b_213f0,        0x00000000u },
    { "fighter_213f0@mutant",     m_213f0,        0x00000000u },
    { "fighter_213f4",            b_213f4,        0x00000000u },
    { "fighter_213f4@mutant",     m_213f4,        0x00000000u },
    { "fighter_213f4@stream",     m_213f4_stream, 0x00000000u },
    { "fighter_213f4@side",       m_213f4_side,   0x00000000u },
    { "fighter_213f4@order",      m_213f4_order,  0x00000000u },
    { "fighter_3e424",            b_3e424,        0x00000000u },
    { "fighter_3e424@mutant",     m_3e424,        0x00000000u },
    { "fighter_3e424@bit",        m_3e424_bit,    0x00000000u },
    { "fighter_3e424@side",       m_3e424_side,   0x00000000u },
    { "fighter_3e424@order",      m_3e424_order,  0x00000000u },
    ''', r'''    { "fighter_24220@decr",       m_24220_decr,   0x00000000u },
    { "fighter_2bdb8",            b_2bdb8,        0x00000000u },
    { "fighter_2bdb8@mutant",     m_2bdb8,        0x00000000u },
    { "fighter_2bdb8@and",        m_2bdb8_and,    0x00000000u },
    { "fighter_2bdb8@dec",        m_2bdb8_dec,    0x00000000u },
    { "fighter_2bdb8@width",      m_2bdb8_width,  0x00000000u },
    { "fighter_213f0",            b_213f0,        0x00000000u },
    { "fighter_213f0@mutant",     m_213f0,        0x00000000u },
    { "fighter_213f4",            b_213f4,        0x00000000u },
    { "fighter_213f4@mutant",     m_213f4,        0x00000000u },
    { "fighter_213f4@stream",     m_213f4_stream, 0x00000000u },
    { "fighter_213f4@side",       m_213f4_side,   0x00000000u },
    { "fighter_213f4@order",      m_213f4_order,  0x00000000u },
    { "fighter_3e424",            b_3e424,        0x00000000u },
    { "fighter_3e424@mutant",     m_3e424,        0x00000000u },
    { "fighter_3e424@bit",        m_3e424_bit,    0x00000000u },
    { "fighter_3e424@side",       m_3e424_side,   0x00000000u },
    { "fighter_3e424@order",      m_3e424_order,  0x00000000u },
    { "fighter_224ec",            b_224ec,        0x00000000u },
    { "fighter_224ec@mutant",     m_224ec,        0x00000000u },
    { "fighter_224ec@side",       m_224ec_side,   0x00000000u },
    { "fighter_224ec@order",      m_224ec_order,  0x00000000u },
    { "fighter_2bde8",            b_2bde8,        0x00000000u },
    { "fighter_2bde8@mutant",     m_2bde8,        0x00000000u },
    { "fighter_2bde8@byte",       m_2bde8_byte,   0x00000000u },
    { "fighter_36114",            b_36114,        0x00000000u },
    { "fighter_36114@neg",        m_36114_neg,    0x00000000u },
    { "fighter_36114@side",       m_36114_side,   0x00000000u },
    { "fighter_36114@anchor",     m_36114_anchor, 0x00000000u },
    { "fighter_36114@char",       m_36114_char,   0x00000000u },
    { "fighter_36114@table",      m_36114_table,  0x00000000u },
    { "fighter_36114@order",      m_36114_order,  0x00000000u },
    { "fighter_36114@second",     m_36114_second, 0x00000000u },
    ''')

sub('port/tests/test_fight.c', r'''    }
}
int test_p7_simple(void)        { return u6b_run(p7_check_simple); }

/* §P7.3: 0x2BDE8 and 0x36114 through their registrations. */
''', r'''    }
}
int test_p7_simple(void)        { return u6b_run(p7_check_simple); }

/* §P7.3: 0x2BDE8 and 0x36114 through their registrations. */
static void p7_check_36114(void)
{
    p7_anim_fn f;
    /* 0x2BDE8: +0x20 = +0x24 + (-1.0f) and +0x2B bit 1 clear. */
    z_fseed();
    DSD(Z_R0 + 0x24u) = 0x40400000u;
    DSD(Z_R0 + 0x20u) = 0x20202020u;
    DSB(Z_R0 + 0x2Bu) = 0xFFu;
    fighter_2bde8(Z_R0);
    CHECK_EQ_INT((int)DSD(Z_R0 + 0x20u), 0x40000000);
    CHECK_EQ_INT((int)DSB(Z_R0 + 0x2Bu), 0xFD);
    DSD(Z_R0 + 0x24u) = 0xC0000000u;
    DSB(Z_R0 + 0x2Bu) = 0x02u;
    fighter_2bde8(Z_R0);
    CHECK_EQ_INT((int)DSD(Z_R0 + 0x20u), (int)0xC0400000u);
    CHECK_EQ_INT((int)DSB(Z_R0 + 0x2Bu), 0x00);

    /* 0x36114: bit 14 set: +0x34 = 0xBDA4C[3] = 0x180, +0x36 = 0x300, the
     * slot's +0x58 + 1, the anchor (0x180, 0x300) on the slot's +0x2C/+0x30,
     * 0x2BDE8 on both records; bit 14 clear: +0x34 = -0x180. */
    f = (p7_anim_fn)(void *)fn_resolve(0x36114u);
    CHECK(f != NULL, "0x36114 is registered");
    if (f != NULL) {
        z_fseed();
        DSB(Z_R0 + 0x51u) = 0u;
        DSD(Z_R0 + 0x14u) = Z_S0;
        DSW(Z_R0 + 0x28u) = 0x4000u;
        DSB(Z_S0 + 0x7Au) = 3u;
        DSB(Z_S0 + 0x58u) = 0x10u;
        DSD(Z_R0 + 0x24u) = 0x40400000u;
        DSD(Z_R0 + 0x20u) = 0x20202020u;
        DSB(Z_R0 + 0x2Bu) = 0x28u;
        DSD(Z_R1 + 0x24u) = 0x40A00000u;
        DSD(Z_R1 + 0x20u) = 0x40404040u;
        DSB(Z_R1 + 0x2Bu) = 0x48u;
        fighter_1883c(0u, 0u, 0u);      /* apply the latch the anchor's call will redo */
        {
            u32 b2c = DSD(Z_S0 + 0x2Cu), b30 = DSD(Z_S0 + 0x30u);
            f(Z_R0, 0u);
            CHECK_EQ_INT((int)DSD(Z_S0 + 0x2Cu), (int)(b2c + 0x180u));
            CHECK_EQ_INT((int)DSD(Z_S0 + 0x30u), (int)(b30 + 0x300u));
        }
        CHECK_EQ_INT((int)DSD(Z_R0 + 0x24u), 0x40000000);
        CHECK_EQ_INT((int)DSW(Z_R0 + 0x34u), 0x0180);
        CHECK_EQ_INT((int)DSW(Z_R0 + 0x36u), 0x0300);
        CHECK_EQ_INT((int)DSB(Z_S0 + 0x58u), 0x11);
        CHECK_EQ_INT((int)DSD(Z_R0 + 0x20u), 0x3F800000);
        CHECK_EQ_INT((int)DSD(Z_R1 + 0x20u), 0x40800000);
        CHECK_EQ_INT((int)DSB(Z_R0 + 0x2Bu), 0x28);
        CHECK_EQ_INT((int)DSB(Z_R1 + 0x2Bu), 0x48);
        z_fseed();
        DSB(Z_R0 + 0x51u) = 0u;
        DSD(Z_R0 + 0x14u) = Z_S0;
        DSW(Z_R0 + 0x28u) = 0x0000u;
        DSB(Z_S0 + 0x7Au) = 3u;
        f(Z_R0, 0u);
        CHECK_EQ_INT((int)DSW(Z_R0 + 0x34u), 0xFE80);
    }
}
int test_p7_36114(void)         { return u6b_run(p7_check_36114); }

/* §P7.4: the spawn pair 0x23960 and 0x23A7C. */
''')

sub('tools/tests/test_diff_verify.py', r'''    "fighter_48374@stream": {"call #1"},
}

# Track P batch 7 (record 2026-10-04-reverse-p7): its rows with their EAX masks, and what alone
# catches each of its mutants.
P7_MASKS = {"fighter_2bdb8": 0, "fighter_213f0": 0, "fighter_213f4": 0, "fighter_3e424": 0}
P7_KINDS = {
    "fighter_2bdb8@mutant": {"byte"},
    "fighter_2bdb8@and": {"byte"},
    "fighter_2bdb8@dec": {"byte"},
    "fighter_2bdb8@width": {"byte"},
    "fighter_213f0@mutant": {"byte"},
    "fighter_213f4@mutant": {"call #0"},
    "fighter_213f4@stream": {"call #0"},
    "fighter_213f4@side": {"byte", "call #0", "call #1", "call #1 memory", "call #2", "call #2 memory"},
    "fighter_213f4@order": {"call #0 memory"},
    "fighter_3e424@mutant": {"call #0"},
    "fighter_3e424@bit": {"byte", "call #1 memory", "call #2 memory"},
    "fighter_3e424@side": {"byte", "call #0", "call #1", "call #1 memory", "call #2", "call #2 memory"},
    "fighter_3e424@order": {"call #0 memory"},
    ''', r'''    "fighter_48374@stream": {"call #1"},
}

# Track P batch 7 (record 2026-10-04-reverse-p7): its rows with their EAX masks, and what alone
# catches each of its mutants.
P7_MASKS = {"fighter_2bdb8": 0, "fighter_213f0": 0, "fighter_213f4": 0, "fighter_3e424": 0}
P7_KINDS = {
    "fighter_2bdb8@mutant": {"byte"},
    "fighter_2bdb8@and": {"byte"},
    "fighter_2bdb8@dec": {"byte"},
    "fighter_2bdb8@width": {"byte"},
    "fighter_213f0@mutant": {"byte"},
    "fighter_213f4@mutant": {"call #0"},
    "fighter_213f4@stream": {"call #0"},
    "fighter_213f4@side": {"byte", "call #0", "call #1", "call #1 memory", "call #2", "call #2 memory"},
    "fighter_213f4@order": {"call #0 memory"},
    "fighter_3e424@mutant": {"call #0"},
    "fighter_3e424@bit": {"byte", "call #1 memory", "call #2 memory"},
    "fighter_3e424@side": {"byte", "call #0", "call #1", "call #1 memory", "call #2", "call #2 memory"},
    "fighter_3e424@order": {"call #0 memory"},
    "fighter_224ec@mutant": {"byte"},
    "fighter_224ec@side": {"byte"},
    "fighter_224ec@order": {"call #0 memory"},
    "fighter_2bde8@mutant": {"byte"},
    "fighter_2bde8@byte": {"byte"},
    "fighter_36114@neg": {"byte", "call #0", "call #0 memory", "call #1 memory", "call #2 memory"},
    "fighter_36114@side": {"byte", "call #0"},
    "fighter_36114@anchor": {"byte", "call #0"},
    "fighter_36114@char": {"byte", "call #0", "call #0 memory", "call #1 memory", "call #2 memory"},
    "fighter_36114@table": {"byte", "call #0", "call #0 memory", "call #1 memory", "call #2 memory"},
    "fighter_36114@order": {"byte", "call #0", "call #0 memory"},
    "fighter_36114@second": {"byte", "call #1", "call #2"},
    ''')

print('T3 applied')
sub('tools/tests/test_diff_verify.py', 'P7_MASKS = {"fighter_2bdb8": 0, "fighter_213f0": 0, "fighter_213f4": 0, "fighter_3e424": 0}', 'P7_MASKS = {"fighter_2bdb8": 0, "fighter_213f0": 0, "fighter_213f4": 0, "fighter_3e424": 0, "fighter_224ec": 0, "fighter_2bde8": 0, "fighter_36114": 0}')

sub('tools/tests/test_diff_verify.py', '        self.assertIn("diff-verify: 153/153 functions VERIFIED; 404/404 mutants detected; 1 named gaps; "\n                      "43/128 rows with callees closed (25 have none).", out.getvalue())', '        self.assertIn("diff-verify: 156/156 functions VERIFIED; 416/416 mutants detected; 1 named gaps; "\n                      "44/130 rows with callees closed (26 have none).", out.getvalue())')

PY
```

Then build and run the task's tests:

```bash
cmake --build build 2>&1 | tail -1
python3 tools/diff_verify.py --self-check --image /tmp/pr_p7_dfv.bin --table /tmp/pr_p7_diff.md | tail -1
PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | grep -E 'FAIL|FAILURES|all checks passed'
```

Expected: the counter line `156/156 functions VERIFIED; 416/416 mutants detected; 1 named gaps; 44/130 rows with callees closed (26 have none).`; every new row VERIFIED with its blocks hit; the new unit check passes. The rows: `fighter_224ec` 2 cases 1/1 (`22404` stub VERIFIED, `339AC` allow VERIFIED), `fighter_2bde8` 3 1/1 (no callee), `fighter_36114` 5 7/7 (`1883C` stub unverified, `2BDE8` real VERIFIED); 12 mutants detected.

- [ ] **Step 2: the E2 table.** `make entry-triage` must pass against the regenerated table; then

```bash
./build/diffrun --exe data/game/C/PRAGE.EXE --image-out /tmp/pr_p7_e2img.bin && \
python3 tools/entry_triage.py --image /tmp/pr_p7_e2img.bin \
  --live docs/superpowers/plans/2026-10-01-reverse-e2-live-functions.txt \
  --out docs/superpowers/plans/2026-10-01-reverse-e2-triage.md
```

Expected: `entry-triage: targets 236 unported, 259 ported; supplement 131 (5 unported, 0 stale); untrusted entries 30` and `voice sites outside Ghidra 134: 0 in unported code, 115 in ported code, 19 nowhere`.

- [ ] **Step 3: every new unit assertion can fail.** The `P7_KINDS` entry of every new mutant names the case set that alone catches it (record §P7.10); `test_each_p7_mutant_is_caught_by_what_it_breaks` asserts the sets, and the self-check reports each mutant MISMATCH. For each unit check, mutate the port function it tests (the same one-line change as the matching `m_*` mutant), run `PR_ORACLE_REQUIRED=1 ./build/run_tests`, record the first FAIL line, and restore the code.

- [ ] **Step 4: commit.** Stage the named files (`git add` each; never `git add -A`) and commit:

```bash
git commit -m "fighter: port 0x224EC 0x2BDE8 0x36114; E2 table regenerated (track P batch 7, task 3)"
```

---

### Task 4: `0x23960`, `0x23A7C` (the spawn pair)

**Files:** as Task 2.

**Interfaces:** produces `void fighter_23960(u32 rec)`, `void fighter_23a7c(u32 rec)`; `P7_23960`, `P7_RNG`, `P7_23960_SEED`; the specs, bindings and mutants; `p7_check_23960`, `test_p7_23960`. Consumes Task 3's `P7_SPECS`; E3's `SPAWN`; `rng_next`.

- [ ] **Step 1: the specs, the unit check, the port, the seams, the bindings and the mutants, in one script.** It applies in order: the specs and the expectations (`tools/diff_verify.py`), the port and the seams (`port/src/game/fighter.c`, `actors.c`), the unit check (`port/tests/test_fight.c`, `test.h`), the bindings and mutants (`port/tests/diff_runner.c`), the test expectations (`tools/tests/test_diff_verify.py`), and, for Tasks 2 and 6, the gp drop, the Makefile pins/provenance and the AGENTS.md clause. It asserts every anchor occurs once, so it either applies cleanly or stops naming the file and the text.

```bash
python3 - <<'PY'
import pathlib


def sub(path, old, new):
    p = pathlib.Path(path)
    s = p.read_text()
    n = s.count(old)
    assert n == 1, "%s: %d occurrences of %r" % (path, n, old[:70])
    p.write_text(s.replace(old, new))

sub('port/src/game/fighter.c', r'''    other = DSD(DS_001077A8 + (side ^ 1u) * 4u);            /* 0x36190..0x3619A */
    if (other == 0u) return;                                /* 0x361A1/0x361A3 */
    fighter_2bde8(rec);                                     /* 0x361A5/0x361A7 */
    fighter_2bde8(DSD(other));                              /* 0x361AC/0x361AE */
}
''', r'''    other = DSD(DS_001077A8 + (side ^ 1u) * 4u);            /* 0x36190..0x3619A */
    if (other == 0u) return;                                /* 0x361A1/0x361A3 */
    fighter_2bde8(rec);                                     /* 0x361A5/0x361A7 */
    fighter_2bde8(DSD(other));                              /* 0x361AC/0x361AE */
}
/* 0x23960 — record §P7.4. EAX = rec. The word 0x105B4C = 0; with the other
 * side's slot set: four 0xA839C spawns, x = the other slot's +0x2C + 0x600,
 * -0x600, +0x1000, -0x1000, y = the other record's +0x30 >> 16, z 0 and a5 0;
 * each child's +0x24 += (float)rng_next(6) and +0x20 takes the same value. */
void fighter_23960(u32 rec)
{
    PR_SEAM(0x23960u, rec);
    static const s32 p7_off[4] = { 0x600, -0x600, 0x1000, -0x1000 };
    u32 other;
    DSW(0x00105B4Cu) = 0u;                                  /* 0x2396C */
    other = DSD(DS_001077A8 + ((((u32)DSB(rec + 0x51u)) ^ 1u) & 0xFFu) * 4u);   /* 0x23967..0x2397A */
    if (other == 0u) return;                                /* 0x23981/0x23983 */
    for (int i = 0; i < 4; i++) {
        u32 child = actor_spawn((const u32 *)(mem + 0x000A839Cu),          /* 0x2398B */
                                DSD(other + 0x2Cu) + (u32)p7_off[i],       /* 0x23992/0x2399A */
                                (u32)((s32)DSD(DSD(other) + 0x30u) >> 16), /* 0x23990/0x23997/0x239A0 */
                                0u, 0u);                            /* 0x23995/0x23989 */
        float v = p2_f32(child + 0x24u) + (float)rng_next(6u); /* 0x239AA..0x239BF */
        p2_set_f32(child + 0x24u, v);                       /* 0x239C1 */
        p2_set_f32(child + 0x20u, v);                       /* 0x239C4 */
    }
}


/* 0x23A7C — record §P7.4. The D100 target at the dword 0xE18C4. With the
 * record's +0x14 slot set: a 0xA8388 spawn with a5 = word +0x56 | 0x400; the
 * child's +0x14 = the slot, its +0x51 = the side; the record's +0x4B = the
 * child's +0x56; then 0x23960(child). */
void fighter_23a7c(u32 rec)
{
    u32 child;
    if (DSD(rec + 0x14u) == 0u) return;                     /* 0x23A82/0x23A86 */
    child = actor_spawn((const u32 *)(mem + 0x000A8388u), 0u, 0u, 0u,
                        (u32)(DSW(rec + 0x56u) | 0x400u));  /* 0x23A88..0x23AA0 */
    DSD(child + 0x14u) = DSD(rec + 0x14u);                  /* 0x23AA5/0x23AA8 */
    DSB(child + 0x51u) = DSB(rec + 0x51u);                  /* 0x23AAB/0x23AAE */
    DSB(rec + 0x4Bu) = DSB(child + 0x56u);                  /* 0x23AB1/0x23AB4 */
    fighter_23960(child);                                   /* 0x23AB7 */
}


''')

sub('port/src/game/fighter.h', r'''void fighter_224ec(u32 rec);
void fighter_2bde8(u32 rec);
void fighter_36114(u32 rec);
''', r'''void fighter_224ec(u32 rec);
void fighter_2bde8(u32 rec);
void fighter_36114(u32 rec);
void fighter_23960(u32 rec);
void fighter_23a7c(u32 rec);
''')

sub('port/src/game/actors.c', r'''/* PORT: record 2026-10-04-reverse-p7 §P7.3. Track P batch 7's Task 3
 * animation targets. */
static void anim_code_224EC(u32 rec, u32 arg)
{
    (void)arg;
    fighter_224ec(rec);
}
static void anim_code_36114(u32 rec, u32 arg)
{
    (void)arg;
    fighter_36114(rec);
}


/* PORT: record 2026-10-04-reverse-p7 ''', r'''/* PORT: record 2026-10-04-reverse-p7 §P7.3. Track P batch 7's Task 3
 * animation targets. */
static void anim_code_224EC(u32 rec, u32 arg)
{
    (void)arg;
    fighter_224ec(rec);
}
static void anim_code_36114(u32 rec, u32 arg)
{
    (void)arg;
    fighter_36114(rec);
}


/* PORT: record 2026-10-04-reverse-p7 §P7.4. Track P batch 7's Task 4
 * animation target. */
static void anim_code_23A7C(u32 rec, u32 arg)
{
    (void)arg;
    fighter_23a7c(rec);
}


/* PORT: record 2026-10-04-reverse-p7 ''')

sub('port/src/game/actors.c', r'''/* PORT: record 2026-10-04-reverse-p7 §P7.3. Track P batch 7's Task 3
     * animation targets. */
    fn_register(0x224ECu, (void (*)(void))anim_code_224EC);
    fn_register(0x36114u, (void (*)(void))anim_code_36114);
    /* PORT: record 2026-10-04-reverse-p7 ''', r'''/* PORT: record 2026-10-04-reverse-p7 §P7.3. Track P batch 7's Task 3
     * animation targets. */
    fn_register(0x224ECu, (void (*)(void))anim_code_224EC);
    fn_register(0x36114u, (void (*)(void))anim_code_36114);
    /* PORT: record 2026-10-04-reverse-p7 §P7.4. Track P batch 7's Task 4
     * animation target. */
    fn_register(0x23A7Cu, (void (*)(void))anim_code_23A7C);
    /* PORT: record 2026-10-04-reverse-p7 ''')

sub('tools/diff_verify.py', r'''# 0x224EC (record §P7.3): ctx; 0x22404(ctx[0]); the own slot's +0x57 = 2. Both slots' +0x57
    # are seeded differently, so @side (ctx[3]) shows.
    Spec("fighter_224ec", 0x224EC, [
        Case("s0", {"eax": E3_REC, "edx": 0x1234},
             {**SLOT_PTRS, E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
              DS_SLOTS + 0x57: b"\x57", DS_SLOTS + 0x94 + 0x57: b"\x67"}),
        Case("s1", {"eax": E3_REC2, "edx": 0x5678},
             {**SLOT_PTRS, E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
              DS_SLOTS + 0x57: b"\x57", DS_SLOTS + 0x94 + 0x57: b"\x67"}),
    ], allow_calls=(0x339AC,), calls=(P7_22404,), eax_mask=0,
       mutants=("@mutant", "@side", "@order")),
    # 0x2BDE8 (record §P7.3): +0x20 = +0x24 + (-1.0f, 0x809B8); +0x2B bit 1 clear. s1's +0x2B
    # has only bit 1 set (the clear observable), s0's 0xFF and s2's 0x03.
    Spec("fighter_2bde8", 0x2BDE8, [
        Case("s0", {"eax": E3_REC}, {E3_REC + 0x20: le32(0x20202020), E3_REC + 0x24: le32(0x40400000),
                                     E3_REC + 0x2B: b"\xff\x2c", E3_REC + 0x2A: b"\x2a"}),
        Case("s1", {"eax": E3_REC2}, {E3_REC2 + 0x20: le32(0x20202020), E3_REC2 + 0x24: le32(0xC0000000),
                                      E3_REC2 + 0x2B: b"\x02\x3c", E3_REC2 + 0x2A: b"\x3a"}),
        Case("s2", {"eax": E3_REC}, {E3_REC + 0x20: le32(0x20202020), E3_REC + 0x24: le32(0x3F800000),
                                     E3_REC + 0x2B: b"\x03\x2c", E3_REC + 0x2A: b"\x2a"}),
    ], eax_mask=0, mutants=("@mutant", "@byte")),
    # 0x36114 (record §P7.3): +0x24 = 2.0; the slot's +0x58 + 1; the signed word 0xBDA4C[char]
    # negated unless the record's +0x28 bit 14; 0xBDA5A[char] into +0x36; 0x1883C(side, +0x34's
    # word, +0x36's word); then 0x2BDE8 on the own and the other record (when set).
    Spec("fighter_36114", 0x36114, [
        Case("s0", {"eax": E3_REC, "edx": 0x1234},
             {**SLOT_PTRS, **P6_PTRS, E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
              E3_REC + 0x14: le32(DS_SLOTS), E3_REC + 0x28: b"\x00\x40",
              E3_REC + 0x24: le32(0x24242424), E3_REC + 0x32: b"\x32\x33\x34\x35\x36\x37",
              E3_REC + 0x20: le32(0x20202020), E3_REC + 0x2B: b"\x2b\x2c",
              E3_REC2 + 0x20: le32(0x40404040), E3_REC2 + 0x24: le32(0x44444444),
              E3_REC2 + 0x2B: b"\x4b\x4c", DS_SLOTS + 0x7A: b"\x03", DS_SLOTS + 0x58: b"\x58\x59"}),
        Case("s1", {"eax": E3_REC, "edx": 0x1234},
             {**SLOT_PTRS, **P6_PTRS, E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
              E3_REC + 0x14: le32(DS_SLOTS), E3_REC + 0x28: b"\x00\x00",
              E3_REC + 0x24: le32(0x24242424), E3_REC + 0x32: b"\x32\x33\x34\x35\x36\x37",
              E3_REC + 0x20: le32(0x20202020), E3_REC + 0x2B: b"\x2b\x2c",
              E3_REC2 + 0x20: le32(0x40404040), E3_REC2 + 0x24: le32(0x44444444),
              E3_REC2 + 0x2B: b"\x4b\x4c", DS_SLOTS + 0x7A: b"\x03", DS_SLOTS + 0x58: b"\x58\x59"}),
        Case("s2", {"eax": E3_REC2, "edx": 0x5678},
             {**SLOT_PTRS, **P6_PTRS, E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
              E3_REC2 + 0x14: le32(DS_SLOTS + 0x94), E3_REC2 + 0x28: b"\x00\x40",
              E3_REC2 + 0x24: le32(0x44444444), E3_REC2 + 0x32: b"\x42\x43\x44\x45\x46\x47",
              E3_REC2 + 0x20: le32(0x40404040), E3_REC2 + 0x2B: b"\x4b\x4c",
              E3_REC + 0x20: le32(0x20202020), E3_REC + 0x24: le32(0x24242424),
              E3_REC + 0x2B: b"\x2b\x2c", DS_SLOTS + 0x94 + 0x7A: b"\x02",
              DS_SLOTS + 0x94 + 0x58: b"\x68\x69"}),
        Case("n0", {"eax": E3_REC, "edx": 0x1234},
             {E3_REC + 0x14: le32(0), E3_REC + 0x24: le32(0x24242424), E3_REC + 0x2B: b"\x2b\x2c"}),
        Case("o0", {"eax": E3_REC, "edx": 0x1234},
             {DS_SLOTS - 8: le32(DS_SLOTS) + le32(0), E3_REC + 0x51: b"\x00",
              E3_REC + 0x14: le32(DS_SLOTS), E3_REC + 0x28: b"\x00\x40",
              E3_REC + 0x24: le32(0x24242424), E3_REC + 0x32: b"\x32\x33\x34\x35\x36\x37",
              E3_REC + 0x20: le32(0x20202020), E3_REC + 0x2B: b"\x2b\x2c",
              DS_SLOTS + 0x7A: b"\x03", DS_SLOTS + 0x58: b"\x58\x59"}),
    ], calls=(C1_1883C, P7_2BDE8_R), eax_mask=0,
       mutants=("@neg", "@side", "@anchor", "@char", "@table", "@order", "@second")),
    ''', r'''# 0x224EC (record §P7.3): ctx; 0x22404(ctx[0]); the own slot's +0x57 = 2. Both slots' +0x57
    # are seeded differently, so @side (ctx[3]) shows.
    Spec("fighter_224ec", 0x224EC, [
        Case("s0", {"eax": E3_REC, "edx": 0x1234},
             {**SLOT_PTRS, E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
              DS_SLOTS + 0x57: b"\x57", DS_SLOTS + 0x94 + 0x57: b"\x67"}),
        Case("s1", {"eax": E3_REC2, "edx": 0x5678},
             {**SLOT_PTRS, E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
              DS_SLOTS + 0x57: b"\x57", DS_SLOTS + 0x94 + 0x57: b"\x67"}),
    ], allow_calls=(0x339AC,), calls=(P7_22404,), eax_mask=0,
       mutants=("@mutant", "@side", "@order")),
    # 0x2BDE8 (record §P7.3): +0x20 = +0x24 + (-1.0f, 0x809B8); +0x2B bit 1 clear. s1's +0x2B
    # has only bit 1 set (the clear observable), s0's 0xFF and s2's 0x03.
    Spec("fighter_2bde8", 0x2BDE8, [
        Case("s0", {"eax": E3_REC}, {E3_REC + 0x20: le32(0x20202020), E3_REC + 0x24: le32(0x40400000),
                                     E3_REC + 0x2B: b"\xff\x2c", E3_REC + 0x2A: b"\x2a"}),
        Case("s1", {"eax": E3_REC2}, {E3_REC2 + 0x20: le32(0x20202020), E3_REC2 + 0x24: le32(0xC0000000),
                                      E3_REC2 + 0x2B: b"\x02\x3c", E3_REC2 + 0x2A: b"\x3a"}),
        Case("s2", {"eax": E3_REC}, {E3_REC + 0x20: le32(0x20202020), E3_REC + 0x24: le32(0x3F800000),
                                     E3_REC + 0x2B: b"\x03\x2c", E3_REC + 0x2A: b"\x2a"}),
    ], eax_mask=0, mutants=("@mutant", "@byte")),
    # 0x36114 (record §P7.3): +0x24 = 2.0; the slot's +0x58 + 1; the signed word 0xBDA4C[char]
    # negated unless the record's +0x28 bit 14; 0xBDA5A[char] into +0x36; 0x1883C(side, +0x34's
    # word, +0x36's word); then 0x2BDE8 on the own and the other record (when set).
    Spec("fighter_36114", 0x36114, [
        Case("s0", {"eax": E3_REC, "edx": 0x1234},
             {**SLOT_PTRS, **P6_PTRS, E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
              E3_REC + 0x14: le32(DS_SLOTS), E3_REC + 0x28: b"\x00\x40",
              E3_REC + 0x24: le32(0x24242424), E3_REC + 0x32: b"\x32\x33\x34\x35\x36\x37",
              E3_REC + 0x20: le32(0x20202020), E3_REC + 0x2B: b"\x2b\x2c",
              E3_REC2 + 0x20: le32(0x40404040), E3_REC2 + 0x24: le32(0x44444444),
              E3_REC2 + 0x2B: b"\x4b\x4c", DS_SLOTS + 0x7A: b"\x03", DS_SLOTS + 0x58: b"\x58\x59"}),
        Case("s1", {"eax": E3_REC, "edx": 0x1234},
             {**SLOT_PTRS, **P6_PTRS, E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
              E3_REC + 0x14: le32(DS_SLOTS), E3_REC + 0x28: b"\x00\x00",
              E3_REC + 0x24: le32(0x24242424), E3_REC + 0x32: b"\x32\x33\x34\x35\x36\x37",
              E3_REC + 0x20: le32(0x20202020), E3_REC + 0x2B: b"\x2b\x2c",
              E3_REC2 + 0x20: le32(0x40404040), E3_REC2 + 0x24: le32(0x44444444),
              E3_REC2 + 0x2B: b"\x4b\x4c", DS_SLOTS + 0x7A: b"\x03", DS_SLOTS + 0x58: b"\x58\x59"}),
        Case("s2", {"eax": E3_REC2, "edx": 0x5678},
             {**SLOT_PTRS, **P6_PTRS, E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
              E3_REC2 + 0x14: le32(DS_SLOTS + 0x94), E3_REC2 + 0x28: b"\x00\x40",
              E3_REC2 + 0x24: le32(0x44444444), E3_REC2 + 0x32: b"\x42\x43\x44\x45\x46\x47",
              E3_REC2 + 0x20: le32(0x40404040), E3_REC2 + 0x2B: b"\x4b\x4c",
              E3_REC + 0x20: le32(0x20202020), E3_REC + 0x24: le32(0x24242424),
              E3_REC + 0x2B: b"\x2b\x2c", DS_SLOTS + 0x94 + 0x7A: b"\x02",
              DS_SLOTS + 0x94 + 0x58: b"\x68\x69"}),
        Case("n0", {"eax": E3_REC, "edx": 0x1234},
             {E3_REC + 0x14: le32(0), E3_REC + 0x24: le32(0x24242424), E3_REC + 0x2B: b"\x2b\x2c"}),
        Case("o0", {"eax": E3_REC, "edx": 0x1234},
             {DS_SLOTS - 8: le32(DS_SLOTS) + le32(0), E3_REC + 0x51: b"\x00",
              E3_REC + 0x14: le32(DS_SLOTS), E3_REC + 0x28: b"\x00\x40",
              E3_REC + 0x24: le32(0x24242424), E3_REC + 0x32: b"\x32\x33\x34\x35\x36\x37",
              E3_REC + 0x20: le32(0x20202020), E3_REC + 0x2B: b"\x2b\x2c",
              DS_SLOTS + 0x7A: b"\x03", DS_SLOTS + 0x58: b"\x58\x59"}),
    ], calls=(C1_1883C, P7_2BDE8_R), eax_mask=0,
       mutants=("@neg", "@side", "@anchor", "@char", "@table", "@order", "@second")),
    Spec("fighter_23960", 0x23960, [
        Case("s0", {"eax": E3_REC}, P7_23960_SEED, {0x2AE14: P6_CHILD}),
        Case("s1", {"eax": E3_REC2},
             {**SLOT_PTRS, **P6_PTRS,
              E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
              DS_SLOTS + 0x2C: le32(0xFFFFF000),
              E3_REC + 0x30: le32(0x00008000),
              P6_CHILD + 0x20: le32(0x20202020), P6_CHILD + 0x24: le32(0x40400000),
              0x105B4C: b"\x4c\x4d"}, {0x2AE14: P6_CHILD}),
        Case("o0", {"eax": E3_REC}, {DS_SLOTS - 8: le32(DS_SLOTS) + le32(0), E3_REC + 0x51: b"\x00",
                                     0x105B4C: b"\x4c\x4d"}),
    ], calls=(SPAWN, P7_RNG), eax_mask=0,
       mutants=("@off", "@y", "@desc", "@float", "@x")),
    # 0x23A7C (record §P7.4): the D100 target at 0xE18C4. With the record's +0x14 slot set: a
    # 0xA8388 spawn with a5 = word +0x56 | 0x400; the child's +0x14 = the slot, +0x51 = the side;
    # the record's +0x4B = the child's +0x56; then 0x23960(child).
    Spec("fighter_23a7c", 0x23A7C, [
        Case("s0", {"eax": E3_REC}, {E3_REC + 0x14: le32(E3_SLOT), E3_REC + 0x51: b"\x00",
                                     E3_REC + 0x56: b"\x34\x12", E3_REC + 0x4B: b"\x4b",
                                     E3_SLOT + 0x14: le32(0x14141414), E3_SLOT + 0x51: b"\x51",
                                     P6_CHILD + 0x56: b"\x5a", P6_CHILD + 0x14: le32(0x14141414)},
             {0x2AE14: P6_CHILD}),
        Case("s1", {"eax": E3_REC2}, {E3_REC2 + 0x14: le32(E3_SLOT + 0x94), E3_REC2 + 0x51: b"\x01",
                                      E3_REC2 + 0x56: b"\x78\x56", E3_REC2 + 0x4B: b"\x4b",
                                      E3_SLOT + 0x94 + 0x14: le32(0x24242424),
                                      E3_SLOT + 0x94 + 0x51: b"\x61",
                                      P6_CHILD + 0x56: b"\x5a", P6_CHILD + 0x14: le32(0x24242424)},
             {0x2AE14: P6_CHILD}),
        Case("n0", {"eax": E3_REC}, {E3_REC + 0x14: le32(0), E3_REC + 0x4B: b"\x4b"}),
    ], calls=(SPAWN, P7_23960), eax_mask=0,
       mutants=("@a5", "@slot", "@side", "@mutant", "@order")),
    ''')

sub('port/tests/diff_runner.c', r'''    DSW(rec + 0x36u) = DSW(0x000BDA5Au + (u32)DSB(slot + 0x7Au) * 2u);
    fighter_1883c(side, (u32)((s32)DSD(rec + 0x32u) >> 16), (u32)((s32)DSD(rec + 0x34u) >> 16));
    fighter_2bde8(rec);
    *eax = 0u;
}
''', r'''    DSW(rec + 0x36u) = DSW(0x000BDA5Au + (u32)DSB(slot + 0x7Au) * 2u);
    fighter_1883c(side, (u32)((s32)DSD(rec + 0x32u) >> 16), (u32)((s32)DSD(rec + 0x34u) >> 16));
    fighter_2bde8(rec);
    *eax = 0u;
}
static void b_23960(const u32 *r, u32 *eax)            { fighter_23960(r[R_EAX]); *eax = 0u; }
static void m_23960_off(const u32 *r, u32 *eax)        /* +0x600 for every spawn */
{
    static const s32 off[4] = { 0x600, 0x600, 0x600, 0x600 };
    u32 rec = r[R_EAX];
    u32 other;
    DSW(0x00105B4Cu) = 0u;
    other = DSD(DS_001077A8 + ((((u32)DSB(rec + 0x51u)) ^ 1u) & 0xFFu) * 4u);
    if (other == 0u) { *eax = 0u; return; }
    for (int i = 0; i < 4; i++) {
        u32 child = actor_spawn((const u32 *)(mem + 0x000A839Cu),
                                DSD(other + 0x2Cu) + (u32)off[i],
                                (u32)((s32)DSD(DSD(other) + 0x30u) >> 16), 0u, 0u);
        union { float f; u32 u; } v;
        v.u = DSD(child + 0x24u);
        v.f = v.f + (float)rng_next(6u);
        DSD(child + 0x24u) = v.u;
        DSD(child + 0x20u) = v.u;
    }
    *eax = 0u;
}
static void m_23960_y(const u32 *r, u32 *eax)          /* y from the slot's +0x2C */
{
    static const s32 off[4] = { 0x600, -0x600, 0x1000, -0x1000 };
    u32 rec = r[R_EAX];
    u32 other;
    DSW(0x00105B4Cu) = 0u;
    other = DSD(DS_001077A8 + ((((u32)DSB(rec + 0x51u)) ^ 1u) & 0xFFu) * 4u);
    if (other == 0u) { *eax = 0u; return; }
    for (int i = 0; i < 4; i++) {
        u32 child = actor_spawn((const u32 *)(mem + 0x000A839Cu),
                                DSD(other + 0x2Cu) + (u32)off[i],
                                DSD(other + 0x2Cu), 0u, 0u);
        union { float f; u32 u; } v;
        v.u = DSD(child + 0x24u);
        v.f = v.f + (float)rng_next(6u);
        DSD(child + 0x24u) = v.u;
        DSD(child + 0x20u) = v.u;
    }
    *eax = 0u;
}
static void m_23960_desc(const u32 *r, u32 *eax)       /* the 0xA8388 descriptor */
{
    static const s32 off[4] = { 0x600, -0x600, 0x1000, -0x1000 };
    u32 rec = r[R_EAX];
    u32 other;
    DSW(0x00105B4Cu) = 0u;
    other = DSD(DS_001077A8 + ((((u32)DSB(rec + 0x51u)) ^ 1u) & 0xFFu) * 4u);
    if (other == 0u) { *eax = 0u; return; }
    for (int i = 0; i < 4; i++) {
        u32 child = actor_spawn((const u32 *)(mem + 0x000A8388u),
                                DSD(other + 0x2Cu) + (u32)off[i],
                                (u32)((s32)DSD(DSD(other) + 0x30u) >> 16), 0u, 0u);
        union { float f; u32 u; } v;
        v.u = DSD(child + 0x24u);
        v.f = v.f + (float)rng_next(6u);
        DSD(child + 0x24u) = v.u;
        DSD(child + 0x20u) = v.u;
    }
    *eax = 0u;
}
static void m_23960_float(const u32 *r, u32 *eax)      /* the rng value without the add */
{
    static const s32 off[4] = { 0x600, -0x600, 0x1000, -0x1000 };
    u32 rec = r[R_EAX];
    u32 other;
    DSW(0x00105B4Cu) = 0u;
    other = DSD(DS_001077A8 + ((((u32)DSB(rec + 0x51u)) ^ 1u) & 0xFFu) * 4u);
    if (other == 0u) { *eax = 0u; return; }
    for (int i = 0; i < 4; i++) {
        u32 child = actor_spawn((const u32 *)(mem + 0x000A839Cu),
                                DSD(other + 0x2Cu) + (u32)off[i],
                                (u32)((s32)DSD(DSD(other) + 0x30u) >> 16), 0u, 0u);
        float f = (float)rng_next(6u);
        memcpy(mem + child + 0x24u, &f, 4);
        memcpy(mem + child + 0x20u, &f, 4);
    }
    *eax = 0u;
}
static void m_23960_x(const u32 *r, u32 *eax)          /* x from the own slot's +0x2C */
{
    static const s32 off[4] = { 0x600, -0x600, 0x1000, -0x1000 };
    u32 rec = r[R_EAX];
    u32 other;
    DSW(0x00105B4Cu) = 0u;
    other = DSD(DS_001077A8 + ((((u32)DSB(rec + 0x51u)) ^ 1u) & 0xFFu) * 4u);
    if (other == 0u) { *eax = 0u; return; }
    for (int i = 0; i < 4; i++) {
        u32 child = actor_spawn((const u32 *)(mem + 0x000A839Cu),
                                DSD(rec + 0x2Cu) + (u32)off[i],
                                (u32)((s32)DSD(DSD(other) + 0x30u) >> 16), 0u, 0u);
        union { float f; u32 u; } v;
        v.u = DSD(child + 0x24u);
        v.f = v.f + (float)rng_next(6u);
        DSD(child + 0x24u) = v.u;
        DSD(child + 0x20u) = v.u;
    }
    *eax = 0u;
}
static void b_23a7c(const u32 *r, u32 *eax)            { b_anim(0x23A7Cu, r, eax); }
static void m_23a7c_a5(const u32 *r, u32 *eax)         /* a5 without the 0x400 */
{
    u32 rec = r[R_EAX];
    u32 child;
    if (DSD(rec + 0x14u) == 0u) { *eax = 0u; return; }
    child = actor_spawn((const u32 *)(mem + 0x000A8388u), 0u, 0u, 0u, (u32)DSW(rec + 0x56u));
    DSD(child + 0x14u) = DSD(rec + 0x14u);
    DSB(child + 0x51u) = DSB(rec + 0x51u);
    DSB(rec + 0x4Bu) = DSB(child + 0x56u);
    fighter_23960(child);
    *eax = 0u;
}
static void m_23a7c_slot(const u32 *r, u32 *eax)       /* the child's +0x14 = the record */
{
    u32 rec = r[R_EAX];
    u32 child;
    if (DSD(rec + 0x14u) == 0u) { *eax = 0u; return; }
    child = actor_spawn((const u32 *)(mem + 0x000A8388u), 0u, 0u, 0u,
                        (u32)(DSW(rec + 0x56u) | 0x400u));
    DSD(child + 0x14u) = rec;
    DSB(child + 0x51u) = DSB(rec + 0x51u);
    DSB(rec + 0x4Bu) = DSB(child + 0x56u);
    fighter_23960(child);
    *eax = 0u;
}
static void m_23a7c_side(const u32 *r, u32 *eax)       /* the child's +0x51 = 1 - side */
{
    u32 rec = r[R_EAX];
    u32 child;
    if (DSD(rec + 0x14u) == 0u) { *eax = 0u; return; }
    child = actor_spawn((const u32 *)(mem + 0x000A8388u), 0u, 0u, 0u,
                        (u32)(DSW(rec + 0x56u) | 0x400u));
    DSD(child + 0x14u) = DSD(rec + 0x14u);
    DSB(child + 0x51u) = (u8)(DSB(rec + 0x51u) ^ 1u);
    DSB(rec + 0x4Bu) = DSB(child + 0x56u);
    fighter_23960(child);
    *eax = 0u;
}
static void m_23a7c_mutant(const u32 *r, u32 *eax)     /* +0x4B from the record's own +0x56 */
{
    u32 rec = r[R_EAX];
    u32 child;
    if (DSD(rec + 0x14u) == 0u) { *eax = 0u; return; }
    child = actor_spawn((const u32 *)(mem + 0x000A8388u), 0u, 0u, 0u,
                        (u32)(DSW(rec + 0x56u) | 0x400u));
    DSD(child + 0x14u) = DSD(rec + 0x14u);
    DSB(child + 0x51u) = DSB(rec + 0x51u);
    DSB(rec + 0x4Bu) = DSB(rec + 0x56u);
    fighter_23960(child);
    *eax = 0u;
}
static void m_23a7c_order(const u32 *r, u32 *eax)      /* the stores before the 0x23960 call */
{
    u32 rec = r[R_EAX];
    u32 child;
    if (DSD(rec + 0x14u) == 0u) { *eax = 0u; return; }
    child = actor_spawn((const u32 *)(mem + 0x000A8388u), 0u, 0u, 0u,
                        (u32)(DSW(rec + 0x56u) | 0x400u));
    fighter_23960(child);
    DSD(child + 0x14u) = DSD(rec + 0x14u);
    DSB(child + 0x51u) = DSB(rec + 0x51u);
    DSB(rec + 0x4Bu) = DSB(child + 0x56u);
    *eax = 0u;
}
''')

sub('port/tests/diff_runner.c', r'''{ "fighter_224ec",            b_224ec,        0x00000000u },
    { "fighter_224ec@mutant",     m_224ec,        0x00000000u },
    { "fighter_224ec@side",       m_224ec_side,   0x00000000u },
    { "fighter_224ec@order",      m_224ec_order,  0x00000000u },
    { "fighter_2bde8",            b_2bde8,        0x00000000u },
    { "fighter_2bde8@mutant",     m_2bde8,        0x00000000u },
    { "fighter_2bde8@byte",       m_2bde8_byte,   0x00000000u },
    { "fighter_36114",            b_36114,        0x00000000u },
    { "fighter_36114@neg",        m_36114_neg,    0x00000000u },
    { "fighter_36114@side",       m_36114_side,   0x00000000u },
    { "fighter_36114@anchor",     m_36114_anchor, 0x00000000u },
    { "fighter_36114@char",       m_36114_char,   0x00000000u },
    { "fighter_36114@table",      m_36114_table,  0x00000000u },
    { "fighter_36114@order",      m_36114_order,  0x00000000u },
    { "fighter_36114@second",     m_36114_second, 0x00000000u },
    ''', r'''{ "fighter_224ec",            b_224ec,        0x00000000u },
    { "fighter_224ec@mutant",     m_224ec,        0x00000000u },
    { "fighter_224ec@side",       m_224ec_side,   0x00000000u },
    { "fighter_224ec@order",      m_224ec_order,  0x00000000u },
    { "fighter_2bde8",            b_2bde8,        0x00000000u },
    { "fighter_2bde8@mutant",     m_2bde8,        0x00000000u },
    { "fighter_2bde8@byte",       m_2bde8_byte,   0x00000000u },
    { "fighter_36114",            b_36114,        0x00000000u },
    { "fighter_36114@neg",        m_36114_neg,    0x00000000u },
    { "fighter_36114@side",       m_36114_side,   0x00000000u },
    { "fighter_36114@anchor",     m_36114_anchor, 0x00000000u },
    { "fighter_36114@char",       m_36114_char,   0x00000000u },
    { "fighter_36114@table",      m_36114_table,  0x00000000u },
    { "fighter_36114@order",      m_36114_order,  0x00000000u },
    { "fighter_36114@second",     m_36114_second, 0x00000000u },
    { "fighter_23960",            b_23960,        0x00000000u },
    { "fighter_23960@off",        m_23960_off,    0x00000000u },
    { "fighter_23960@y",          m_23960_y,      0x00000000u },
    { "fighter_23960@desc",       m_23960_desc,   0x00000000u },
    { "fighter_23960@float",      m_23960_float,  0x00000000u },
    { "fighter_23960@x",          m_23960_x,      0x00000000u },
    { "fighter_23a7c",            b_23a7c,        0x00000000u },
    { "fighter_23a7c@a5",         m_23a7c_a5,     0x00000000u },
    { "fighter_23a7c@slot",       m_23a7c_slot,   0x00000000u },
    { "fighter_23a7c@side",       m_23a7c_side,   0x00000000u },
    { "fighter_23a7c@mutant",     m_23a7c_mutant, 0x00000000u },
    { "fighter_23a7c@order",      m_23a7c_order,  0x00000000u },
    ''')

sub('port/tests/test_fight.c', r'''    }
}
int test_p7_36114(void)         { return u6b_run(p7_check_36114); }

/* §P7.4: the spawn pair 0x23960 and 0x23A7C. */
''', r'''    }
}
int test_p7_36114(void)         { return u6b_run(p7_check_36114); }

/* §P7.4: the spawn pair 0x23960 and 0x23A7C. */
static void p7_check_23960(void)
{
    static u32 before[0x80];
    u32 seed = 0x2468ACE0u;
    u32 n, k, r, found, kids[8], nk;
    float base;
    /* 0x23960: 0x105B4C = 0; four 0xA839C children at the other slot's
     * +0x2C ± 0x600/0x1000; each child's +0x24 and +0x20 take the spawn's
     * +0x24 + (float)rng_next(6). */
    z_fseed();
    DSB(Z_R0 + 0x51u) = 0u;
    DSD(Z_S1 + 0x2Cu) = 0x1000u;
    DSD(Z_R1 + 0x30u) = 0x12345678u;
    DSW(0x00105B4Cu) = 0x4C4Cu;
    DSD(0x000EF6D8u) = seed;
    base = (float)DSB(0x000A839Cu + 5u);
    n = u6b_list(before, 0x80u);
    fighter_23960(Z_R0);
    CHECK_EQ_INT((int)DSW(0x00105B4Cu), 0);
    CHECK_EQ_INT((int)u6b_list(before, 0x80u), (int)(n + 4u));
    nk = 0u;
    for (r = actor_list_head(); r != 0 && nk < 8u; r = actor_next(r)) {
        for (k = 0; k < n && before[k] != r; k++) {}
        if (k == n) kids[nk++] = r;
    }
    CHECK_EQ_INT((int)nk, 4);
    found = 0u;
    for (k = 0; k < nk; k++) {
        union { float f; u32 u; } c;
        c.u = DSD(kids[k] + 0x24u);
        CHECK_EQ_INT((int)DSD(kids[k] + 0x24u), (int)DSD(kids[k] + 0x20u));
        CHECK(c.f >= base && c.f <= base + 5.0f,
              "the child's +0x24 is the spawn value + rng_next(6)");
        if (c.f != base) found = 1u;
    }
    CHECK(found != 0u, "at least one child's rng draw was non-zero");

    /* 0x23A7C: one 0xA8388 child, its +0x14 = the record's slot, +0x51 = the
     * record's side and the record's +0x4B = the child's +0x56; then
     * 0x23960(child) spawns four more (the child's other slot is set). */
    n = u6b_list(before, 0x80u);
    DSB(Z_R0 + 0x14u) = 0u;     /* reset the +0x14 field the previous run's 49444-style paths may have touched */
    z_fseed();
    DSB(Z_R0 + 0x51u) = 0u;
    DSD(Z_R0 + 0x14u) = Z_S0;
    DSB(Z_R0 + 0x56u) = 0x34u;
    DSB(Z_R0 + 0x4Bu) = 0x4Bu;
    n = u6b_list(before, 0x80u);
    {
        p7_anim_fn f = (p7_anim_fn)(void *)fn_resolve(0x23A7Cu);
        CHECK(f != NULL, "0x23A7C is registered");
        if (f == NULL) return;
        f(Z_R0, 0u);
    }
    CHECK_EQ_INT((int)u6b_list(before, 0x80u), (int)(n + 5u));
    found = 0u;
    for (r = actor_list_head(); r != 0; r = actor_next(r)) {
        for (k = 0; k < n && before[k] != r; k++) {}
        if (k == n && DSD(r + 0x14u) == Z_S0) {
            found = r;
            CHECK_EQ_INT((int)DSB(r + 0x51u), 0);
            CHECK_EQ_INT((int)DSB(Z_R0 + 0x4Bu), (int)DSB(r + 0x56u));
        }
    }
    CHECK(found != 0u, "the 0x23A7C child holds the record's slot");
}
int test_p7_23960(void)         { return u6b_run(p7_check_23960); }

/* §P7.5: 0x3A9D8 and 0x48254 through their registrations. */
''')

sub('tools/tests/test_diff_verify.py', r'''"fighter_224ec@mutant": {"byte"},
    "fighter_224ec@side": {"byte"},
    "fighter_224ec@order": {"call #0 memory"},
    "fighter_2bde8@mutant": {"byte"},
    "fighter_2bde8@byte": {"byte"},
    "fighter_36114@neg": {"byte", "call #0", "call #0 memory", "call #1 memory", "call #2 memory"},
    "fighter_36114@side": {"byte", "call #0"},
    "fighter_36114@anchor": {"byte", "call #0"},
    "fighter_36114@char": {"byte", "call #0", "call #0 memory", "call #1 memory", "call #2 memory"},
    "fighter_36114@table": {"byte", "call #0", "call #0 memory", "call #1 memory", "call #2 memory"},
    "fighter_36114@order": {"byte", "call #0", "call #0 memory"},
    "fighter_36114@second": {"byte", "call #1", "call #2"},
    ''', r'''"fighter_224ec@mutant": {"byte"},
    "fighter_224ec@side": {"byte"},
    "fighter_224ec@order": {"call #0 memory"},
    "fighter_2bde8@mutant": {"byte"},
    "fighter_2bde8@byte": {"byte"},
    "fighter_36114@neg": {"byte", "call #0", "call #0 memory", "call #1 memory", "call #2 memory"},
    "fighter_36114@side": {"byte", "call #0"},
    "fighter_36114@anchor": {"byte", "call #0"},
    "fighter_36114@char": {"byte", "call #0", "call #0 memory", "call #1 memory", "call #2 memory"},
    "fighter_36114@table": {"byte", "call #0", "call #0 memory", "call #1 memory", "call #2 memory"},
    "fighter_36114@order": {"byte", "call #0", "call #0 memory"},
    "fighter_36114@second": {"byte", "call #1", "call #2"},
    "fighter_23960@off": {"call #2", "call #4", "call #6"},
    "fighter_23960@y": {"call #0", "call #2", "call #4", "call #6"},
    "fighter_23960@desc": {"call #0", "call #2", "call #4", "call #6"},
    "fighter_23960@float": {"byte", "call #2 memory", "call #3 memory", "call #4 memory", "call #5 memory", "call #6 memory", "call #7 memory"},
    "fighter_23960@x": {"call #0", "call #2", "call #4", "call #6"},
    "fighter_23a7c@a5": {"call #0"},
    "fighter_23a7c@slot": {"byte", "call #1 memory"},
    "fighter_23a7c@side": {"byte", "call #1 memory"},
    "fighter_23a7c@mutant": {"byte", "call #1 memory"},
    "fighter_23a7c@order": {"call #1 memory"},
    ''')

print('T4 applied')
sub('tools/tests/test_diff_verify.py', 'P7_MASKS = {"fighter_2bdb8": 0, "fighter_213f0": 0, "fighter_213f4": 0, "fighter_3e424": 0, "fighter_224ec": 0, "fighter_2bde8": 0, "fighter_36114": 0}', 'P7_MASKS = {"fighter_2bdb8": 0, "fighter_213f0": 0, "fighter_213f4": 0, "fighter_3e424": 0, "fighter_224ec": 0, "fighter_2bde8": 0, "fighter_36114": 0, "fighter_23960": 0, "fighter_23a7c": 0}')

sub('tools/tests/test_diff_verify.py', '        self.assertIn("diff-verify: 156/156 functions VERIFIED; 416/416 mutants detected; 1 named gaps; "\n                      "44/130 rows with callees closed (26 have none).", out.getvalue())', '        self.assertIn("diff-verify: 158/158 functions VERIFIED; 426/426 mutants detected; 1 named gaps; "\n                      "44/132 rows with callees closed (26 have none).", out.getvalue())')

sub('tools/tests/test_diff_verify.py', '                                 0x39280: (), 0x13244: (), 0x2A148: ("edx",), 0x2BCF4: ("edx",),', '                                 0x23960: (),\n                                 0x39280: (), 0x13244: (), 0x2A148: ("edx",), 0x2BCF4: ("edx",),')

PY
```

Then build and run the task's tests:

```bash
cmake --build build 2>&1 | tail -1
python3 tools/diff_verify.py --self-check --image /tmp/pr_p7_dfv.bin --table /tmp/pr_p7_diff.md | tail -1
PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | grep -E 'FAIL|FAILURES|all checks passed'
```

Expected: the counter line `158/158 functions VERIFIED; 426/426 mutants detected; 1 named gaps; 44/132 rows with callees closed (26 have none).`; every new row VERIFIED with its blocks hit; the new unit check passes. The rows: `fighter_23960` 3 cases 3/3 (`2AE14` stub unverified, `5D7DC` real VERIFIED), `fighter_23a7c` 3 3/3 (`23960` stub VERIFIED, `2AE14` stub unverified); 10 mutants detected.

- [ ] **Step 2: the E2 table.** `make entry-triage` must pass against the regenerated table; then

```bash
./build/diffrun --exe data/game/C/PRAGE.EXE --image-out /tmp/pr_p7_e2img.bin && \
python3 tools/entry_triage.py --image /tmp/pr_p7_e2img.bin \
  --live docs/superpowers/plans/2026-10-01-reverse-e2-live-functions.txt \
  --out docs/superpowers/plans/2026-10-01-reverse-e2-triage.md
```

Expected: `entry-triage: targets 235 unported, 260 ported; supplement 131 (4 unported, 0 stale); untrusted entries 30` and `voice sites outside Ghidra 134: 0 in unported code, 115 in ported code, 19 nowhere`.

- [ ] **Step 3: every new unit assertion can fail.** The `P7_KINDS` entry of every new mutant names the case set that alone catches it (record §P7.10); `test_each_p7_mutant_is_caught_by_what_it_breaks` asserts the sets, and the self-check reports each mutant MISMATCH. For each unit check, mutate the port function it tests (the same one-line change as the matching `m_*` mutant), run `PR_ORACLE_REQUIRED=1 ./build/run_tests`, record the first FAIL line, and restore the code.

- [ ] **Step 4: commit.** Stage the named files (`git add` each; never `git add -A`) and commit:

```bash
git commit -m "fighter: port 0x23960 0x23A7C; E2 table regenerated (track P batch 7, task 4)"
```

---

### Task 5: `0x3A9D8`, `0x48254` (the two twins)

**Files:** as Task 2.

**Interfaces:** produces `void fighter_3a9d8(u32 side, u32 b)`, `void fighter_48254(u32 rec)`; `P7_3A9D8`; the specs, bindings and mutants; `p7_check_3a9d8`, `test_p7_3a9d8`. Consumes Task 4's `P7_SPECS`; C1's `ANCHOR`, P2's `POSE`.

- [ ] **Step 1: the specs, the unit check, the port, the seams, the bindings and the mutants, in one script.** It applies in order: the specs and the expectations (`tools/diff_verify.py`), the port and the seams (`port/src/game/fighter.c`, `actors.c`), the unit check (`port/tests/test_fight.c`, `test.h`), the bindings and mutants (`port/tests/diff_runner.c`), the test expectations (`tools/tests/test_diff_verify.py`), and, for Tasks 2 and 6, the gp drop, the Makefile pins/provenance and the AGENTS.md clause. It asserts every anchor occurs once, so it either applies cleanly or stops naming the file and the text.

```bash
python3 - <<'PY'
import pathlib


def sub(path, old, new):
    p = pathlib.Path(path)
    s = p.read_text()
    n = s.count(old)
    assert n == 1, "%s: %d occurrences of %r" % (path, n, old[:70])
    p.write_text(s.replace(old, new))

sub('port/src/game/fighter.c', r'''    DSD(child + 0x14u) = DSD(rec + 0x14u);                  /* 0x23AA5/0x23AA8 */
    DSB(child + 0x51u) = DSB(rec + 0x51u);                  /* 0x23AAB/0x23AAE */
    DSB(rec + 0x4Bu) = DSB(child + 0x56u);                  /* 0x23AB1/0x23AB4 */
    fighter_23960(child);                                   /* 0x23AB7 */
}
''', r'''    DSD(child + 0x14u) = DSD(rec + 0x14u);                  /* 0x23AA5/0x23AA8 */
    DSB(child + 0x51u) = DSB(rec + 0x51u);                  /* 0x23AAB/0x23AAE */
    DSB(rec + 0x4Bu) = DSB(child + 0x56u);                  /* 0x23AB1/0x23AB4 */
    fighter_23960(child);                                   /* 0x23AB7 */
}
/* 0x3A9D8 — record §P7.5. EAX = side, EDX = b. 0x3A95C's twin (the stream
 * table 0xC9030): the swapped ctx; 0x188AC(ctx[1], the other record's +0x18,
 * 0); the own slot's +0x52/0x53/0x54 = 0x10/0xA/0 and +0x10 = 0; the other
 * record on 0xC9030[the own slot's char] at 3.0; the own slot's +0x7E = the
 * byte 0xBECF8 + b. */
void fighter_3a9d8(u32 side, u32 b)
{
    PR_SEAM(0x3A9D8u, side, b);
    u32 ctx[6];
    fighter_ctx_swap(ctx, side);                            /* 0x33A10 */
    hit_anchor_set(ctx[1], DSD(ctx[5] + 0x18u), 0u);        /* 0x3A9E8..0x3A9F5 */
    DSB(ctx[3] + 0x52u) = 0x10u;                            /* 0x3A9FA/0x3A9FE */
    DSB(ctx[3] + 0x53u) = 0x0Au;                            /* 0x3AA02/0x3AA06 */
    DSB(ctx[3] + 0x54u) = 0u;                               /* 0x3AA0A/0x3AA0E */
    DSD(ctx[3] + 0x10u) = 0u;                               /* 0x3AA12/0x3AA16 */
    actors_anim_begin(ctx[5], DSD(0x000C9030u + (u32)DSB(ctx[3] + 0x7Au) * 4u),
                      0x40400000u);                         /* 0x3AA1D..0x3AA39 */
    DSB(ctx[3] + 0x7Eu) = (u8)(DSB(0x000BECF8u) + (u8)b);   /* 0x3AA3E..0x3AA49 */
}


/* 0x48254 — record §P7.5. The D500 target at the dword 0xED872: 0x482E4's
 * twin (the stream 0xED87A and 0x3A9D8 instead of 0x3A95C). */
void fighter_48254(u32 rec)
{
    u32 own = (u32)DSB(rec + 0x51u);                        /* 0x48260 */
    u32 other_side = 1u - own;                              /* 0x4825B/0x48264 */
    u32 oslot = DS_001077B0 + other_side * 0x94u;           /* 0x48266..0x48281 */
    actors_anim_begin(rec, 0x000ED87Au, 0x40400000u);       /* 0x48283..0x4828A */
    if (DSB(0x00108392u + own) != 0u) {                     /* 0x4828F/0x48296 */
        u32 ch = (u32)DSB(oslot + 0x7Au);                   /* 0x4829A */
        actors_anim_begin(DSD(oslot), DSD(0x000C8F40u + ch * 4u),
                          0x40A00000u);                     /* 0x4829D..0x482AB */
    } else {
        fighter_3a9d8(other_side, 0x0Fu);                   /* 0x482B2..0x482B9 */
        fighter_39834(other_side, (s32)DSB(0x0010780Fu + own * 0x94u));   /* 0x482BE..0x482D7 */
    }
}


''')

sub('port/src/game/fighter.h', r'''void fighter_23960(u32 rec);
void fighter_23a7c(u32 rec);
''', r'''void fighter_23960(u32 rec);
void fighter_23a7c(u32 rec);
void fighter_3a9d8(u32 side, u32 b);
void fighter_48254(u32 rec);
''')

sub('port/src/game/actors.c', r'''§P7.4. Track P batch 7's Task 4
 * animation target. */
static void anim_code_23A7C(u32 rec, u32 arg)
{
    (void)arg;
    fighter_23a7c(rec);
}


/* PORT: record 2026-10-04-reverse-p7 ''', r'''§P7.4. Track P batch 7's Task 4
 * animation target. */
static void anim_code_23A7C(u32 rec, u32 arg)
{
    (void)arg;
    fighter_23a7c(rec);
}


/* PORT: record 2026-10-04-reverse-p7 §P7.5. Track P batch 7's Task 5
 * animation target. */
static void anim_code_48254(u32 rec, u32 arg)
{
    (void)arg;
    fighter_48254(rec);
}


/* PORT: record 2026-10-04-reverse-p7 ''')

sub('port/src/game/actors.c', r'''§P7.4. Track P batch 7's Task 4
     * animation target. */
    fn_register(0x23A7Cu, (void (*)(void))anim_code_23A7C);
    /* PORT: record 2026-10-04-reverse-p7 ''', r'''§P7.4. Track P batch 7's Task 4
     * animation target. */
    fn_register(0x23A7Cu, (void (*)(void))anim_code_23A7C);
    /* PORT: record 2026-10-04-reverse-p7 §P7.5. Track P batch 7's Task 5
     * animation target. */
    fn_register(0x48254u, (void (*)(void))anim_code_48254);
    /* PORT: record 2026-10-04-reverse-p7 ''')

sub('tools/diff_verify.py', r'''Spec("fighter_23960", 0x23960, [
        Case("s0", {"eax": E3_REC}, P7_23960_SEED, {0x2AE14: P6_CHILD}),
        Case("s1", {"eax": E3_REC2},
             {**SLOT_PTRS, **P6_PTRS,
              E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
              DS_SLOTS + 0x2C: le32(0xFFFFF000),
              E3_REC + 0x30: le32(0x00008000),
              P6_CHILD + 0x20: le32(0x20202020), P6_CHILD + 0x24: le32(0x40400000),
              0x105B4C: b"\x4c\x4d"}, {0x2AE14: P6_CHILD}),
        Case("o0", {"eax": E3_REC}, {DS_SLOTS - 8: le32(DS_SLOTS) + le32(0), E3_REC + 0x51: b"\x00",
                                     0x105B4C: b"\x4c\x4d"}),
    ], calls=(SPAWN, P7_RNG), eax_mask=0,
       mutants=("@off", "@y", "@desc", "@float", "@x")),
    # 0x23A7C (record §P7.4): the D100 target at 0xE18C4. With the record's +0x14 slot set: a
    # 0xA8388 spawn with a5 = word +0x56 | 0x400; the child's +0x14 = the slot, +0x51 = the side;
    # the record's +0x4B = the child's +0x56; then 0x23960(child).
    Spec("fighter_23a7c", 0x23A7C, [
        Case("s0", {"eax": E3_REC}, {E3_REC + 0x14: le32(E3_SLOT), E3_REC + 0x51: b"\x00",
                                     E3_REC + 0x56: b"\x34\x12", E3_REC + 0x4B: b"\x4b",
                                     E3_SLOT + 0x14: le32(0x14141414), E3_SLOT + 0x51: b"\x51",
                                     P6_CHILD + 0x56: b"\x5a", P6_CHILD + 0x14: le32(0x14141414)},
             {0x2AE14: P6_CHILD}),
        Case("s1", {"eax": E3_REC2}, {E3_REC2 + 0x14: le32(E3_SLOT + 0x94), E3_REC2 + 0x51: b"\x01",
                                      E3_REC2 + 0x56: b"\x78\x56", E3_REC2 + 0x4B: b"\x4b",
                                      E3_SLOT + 0x94 + 0x14: le32(0x24242424),
                                      E3_SLOT + 0x94 + 0x51: b"\x61",
                                      P6_CHILD + 0x56: b"\x5a", P6_CHILD + 0x14: le32(0x24242424)},
             {0x2AE14: P6_CHILD}),
        Case("n0", {"eax": E3_REC}, {E3_REC + 0x14: le32(0), E3_REC + 0x4B: b"\x4b"}),
    ], calls=(SPAWN, P7_23960), eax_mask=0,
       mutants=("@a5", "@slot", "@side", "@mutant", "@order")),
    ''', r'''Spec("fighter_23960", 0x23960, [
        Case("s0", {"eax": E3_REC}, P7_23960_SEED, {0x2AE14: P6_CHILD}),
        Case("s1", {"eax": E3_REC2},
             {**SLOT_PTRS, **P6_PTRS,
              E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
              DS_SLOTS + 0x2C: le32(0xFFFFF000),
              E3_REC + 0x30: le32(0x00008000),
              P6_CHILD + 0x20: le32(0x20202020), P6_CHILD + 0x24: le32(0x40400000),
              0x105B4C: b"\x4c\x4d"}, {0x2AE14: P6_CHILD}),
        Case("o0", {"eax": E3_REC}, {DS_SLOTS - 8: le32(DS_SLOTS) + le32(0), E3_REC + 0x51: b"\x00",
                                     0x105B4C: b"\x4c\x4d"}),
    ], calls=(SPAWN, P7_RNG), eax_mask=0,
       mutants=("@off", "@y", "@desc", "@float", "@x")),
    # 0x23A7C (record §P7.4): the D100 target at 0xE18C4. With the record's +0x14 slot set: a
    # 0xA8388 spawn with a5 = word +0x56 | 0x400; the child's +0x14 = the slot, +0x51 = the side;
    # the record's +0x4B = the child's +0x56; then 0x23960(child).
    Spec("fighter_23a7c", 0x23A7C, [
        Case("s0", {"eax": E3_REC}, {E3_REC + 0x14: le32(E3_SLOT), E3_REC + 0x51: b"\x00",
                                     E3_REC + 0x56: b"\x34\x12", E3_REC + 0x4B: b"\x4b",
                                     E3_SLOT + 0x14: le32(0x14141414), E3_SLOT + 0x51: b"\x51",
                                     P6_CHILD + 0x56: b"\x5a", P6_CHILD + 0x14: le32(0x14141414)},
             {0x2AE14: P6_CHILD}),
        Case("s1", {"eax": E3_REC2}, {E3_REC2 + 0x14: le32(E3_SLOT + 0x94), E3_REC2 + 0x51: b"\x01",
                                      E3_REC2 + 0x56: b"\x78\x56", E3_REC2 + 0x4B: b"\x4b",
                                      E3_SLOT + 0x94 + 0x14: le32(0x24242424),
                                      E3_SLOT + 0x94 + 0x51: b"\x61",
                                      P6_CHILD + 0x56: b"\x5a", P6_CHILD + 0x14: le32(0x24242424)},
             {0x2AE14: P6_CHILD}),
        Case("n0", {"eax": E3_REC}, {E3_REC + 0x14: le32(0), E3_REC + 0x4B: b"\x4b"}),
    ], calls=(SPAWN, P7_23960), eax_mask=0,
       mutants=("@a5", "@slot", "@side", "@mutant", "@order")),
    # 0x3A9D8 (record §P7.5): 0x3A95C's twin (the stream table 0xC9030). EAX = side, EDX = b.
    # 0x33A10 runs on both sides (allow); 0x188AC(ctx[1], the other record's +0x18, 0); the own
    # slot (ctx[3]) 0x10/0x0A/0/0x10 = 0; 0x2BC30(ctx[5], the char stream, 3.0); ctx[3].+0x7E =
    # byte 0xBECF8 + (u8)b.
    Spec("fighter_3a9d8", 0x3A9D8, [
        Case("s0", {"eax": 0, "edx": 0x1234},
             {**SLOT_PTRS, E3_REC2 + 0x18: le32(0x18181818), E3_REC + 0x18: le32(0x28282828),
              DS_SLOTS + 0x10: b"\x20\x20\x20\x20",
              DS_SLOTS + 0x52: b"\x52\x53\x54\x55", DS_SLOTS + 0x94 + 0x52: b"\x62\x63\x64\x65",
              DS_SLOTS + 0x94 + 0x10: b"\x10\x10\x10\x10", DS_SLOTS + 0x7A: b"\x03",
              DS_SLOTS + 0x94 + 0x7A: b"\x02", DS_SLOTS + 0x7E: b"\x7e\x7f",
              DS_SLOTS + 0x94 + 0x7E: b"\x8e\x8f"}),
        Case("s1", {"eax": 1, "edx": 0x0080},
             {**SLOT_PTRS, E3_REC2 + 0x18: le32(0x18181818), E3_REC + 0x18: le32(0x28282828),
              DS_SLOTS + 0x10: b"\x20\x20\x20\x20",
              DS_SLOTS + 0x52: b"\x52\x53\x54\x55", DS_SLOTS + 0x94 + 0x52: b"\x62\x63\x64\x65",
              DS_SLOTS + 0x94 + 0x10: b"\x10\x10\x10\x10", DS_SLOTS + 0x7A: b"\x03",
              DS_SLOTS + 0x94 + 0x7A: b"\x02", DS_SLOTS + 0x7E: b"\x7e\x7f",
              DS_SLOTS + 0x94 + 0x7E: b"\x8e\x8f"}),
    ], allow_calls=(0x33A10,), calls=(ANCHOR, ANIM_BEGIN), eax_mask=0,
       mutants=("@side", "@stream", "@anchor", "@slot", "@frame", "@byte")),
    # 0x48254 (record §P7.5): 0x482E4's twin (the stream 0xED87A and 0x3A9D8). The flag
    # 0x108392[own side] selects the other record on 0xC8F40[other char] at 5.0, else
    # 0x3A9D8(other side, 0xF) and 0x39834(other side, the own slot's +0x5F).
    Spec("fighter_48254", 0x48254, [
        Case("s0", {"eax": E3_REC, "edx": 0x1234},
             {**SLOT_PTRS, E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
              DS_SLOTS + 0x94 + 0x7A: b"\x02", DS_SLOTS + 0x7A: b"\x03",
              DS_SLOTS + 0x5F: b"\x5f", DS_SLOTS + 0x94 + 0x5F: b"\x9f",
              0x108392: b"\x01", 0x108392 + 1: b"\x00"}),
        Case("s1", {"eax": E3_REC2, "edx": 0x5678},
             {**SLOT_PTRS, E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
              DS_SLOTS + 0x94 + 0x7A: b"\x02", DS_SLOTS + 0x7A: b"\x03",
              DS_SLOTS + 0x5F: b"\x5f", DS_SLOTS + 0x94 + 0x5F: b"\x9f",
              0x108392: b"\x00", 0x108392 + 1: b"\x01"}),
        Case("z0", {"eax": E3_REC, "edx": 0x1234},
             {**SLOT_PTRS, E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
              DS_SLOTS + 0x94 + 0x7A: b"\x04", DS_SLOTS + 0x7A: b"\x03",
              DS_SLOTS + 0x5F: b"\x5f", DS_SLOTS + 0x94 + 0x5F: b"\x9f",
              0x108392: b"\x00", 0x108392 + 1: b"\x00"}),
    ], calls=(ANIM_BEGIN, P7_3A9D8, POSE), eax_mask=0,
       mutants=("@byte", "@side", "@stream", "@pose", "@char", "@order")),
    ''')

sub('port/tests/diff_runner.c', r'''    DSD(child + 0x14u) = DSD(rec + 0x14u);
    DSB(child + 0x51u) = DSB(rec + 0x51u);
    DSB(rec + 0x4Bu) = DSB(child + 0x56u);
    *eax = 0u;
}
''', r'''    DSD(child + 0x14u) = DSD(rec + 0x14u);
    DSB(child + 0x51u) = DSB(rec + 0x51u);
    DSB(rec + 0x4Bu) = DSB(child + 0x56u);
    *eax = 0u;
}
static void b_3a9d8(const u32 *r, u32 *eax)            { fighter_3a9d8(r[R_EAX], r[R_EDX]); *eax = 0u; }
static void m_3a9d8_side(const u32 *r, u32 *eax)       /* 0x188AC on the other side */
{
    u32 ctx[6];
    fighter_ctx_swap(ctx, r[R_EAX]);
    hit_anchor_set(ctx[0], DSD(ctx[5] + 0x18u), 0u);
    DSB(ctx[3] + 0x52u) = 0x10u;
    DSB(ctx[3] + 0x53u) = 0x0Au;
    DSB(ctx[3] + 0x54u) = 0u;
    DSD(ctx[3] + 0x10u) = 0u;
    actors_anim_begin(ctx[5], DSD(0x000C9030u + (u32)DSB(ctx[3] + 0x7Au) * 4u), 0x40400000u);
    DSB(ctx[3] + 0x7Eu) = (u8)(DSB(0x000BECF8u) + (u8)r[R_EDX]);
    *eax = 0u;
}
static void m_3a9d8_stream(const u32 *r, u32 *eax)     /* the 0xC8FE0 stream table */
{
    u32 ctx[6];
    fighter_ctx_swap(ctx, r[R_EAX]);
    hit_anchor_set(ctx[1], DSD(ctx[5] + 0x18u), 0u);
    DSB(ctx[3] + 0x52u) = 0x10u;
    DSB(ctx[3] + 0x53u) = 0x0Au;
    DSB(ctx[3] + 0x54u) = 0u;
    DSD(ctx[3] + 0x10u) = 0u;
    actors_anim_begin(ctx[5], DSD(0x000C8FE0u + (u32)DSB(ctx[3] + 0x7Au) * 4u), 0x40400000u);
    DSB(ctx[3] + 0x7Eu) = (u8)(DSB(0x000BECF8u) + (u8)r[R_EDX]);
    *eax = 0u;
}
static void m_3a9d8_anchor(const u32 *r, u32 *eax)     /* the other record's +0x1C */
{
    u32 ctx[6];
    fighter_ctx_swap(ctx, r[R_EAX]);
    hit_anchor_set(ctx[1], DSD(ctx[5] + 0x1Cu), 0u);
    DSB(ctx[3] + 0x52u) = 0x10u;
    DSB(ctx[3] + 0x53u) = 0x0Au;
    DSB(ctx[3] + 0x54u) = 0u;
    DSD(ctx[3] + 0x10u) = 0u;
    actors_anim_begin(ctx[5], DSD(0x000C9030u + (u32)DSB(ctx[3] + 0x7Au) * 4u), 0x40400000u);
    DSB(ctx[3] + 0x7Eu) = (u8)(DSB(0x000BECF8u) + (u8)r[R_EDX]);
    *eax = 0u;
}
static void m_3a9d8_slot(const u32 *r, u32 *eax)       /* the stores on ctx[2] */
{
    u32 ctx[6];
    fighter_ctx_swap(ctx, r[R_EAX]);
    hit_anchor_set(ctx[1], DSD(ctx[5] + 0x18u), 0u);
    DSB(ctx[2] + 0x52u) = 0x10u;
    DSB(ctx[2] + 0x53u) = 0x0Au;
    DSB(ctx[2] + 0x54u) = 0u;
    DSD(ctx[2] + 0x10u) = 0u;
    actors_anim_begin(ctx[5], DSD(0x000C9030u + (u32)DSB(ctx[2] + 0x7Au) * 4u), 0x40400000u);
    DSB(ctx[2] + 0x7Eu) = (u8)(DSB(0x000BECF8u) + (u8)r[R_EDX]);
    *eax = 0u;
}
static void m_3a9d8_frame(const u32 *r, u32 *eax)      /* 4.0 for the frame */
{
    u32 ctx[6];
    fighter_ctx_swap(ctx, r[R_EAX]);
    hit_anchor_set(ctx[1], DSD(ctx[5] + 0x18u), 0u);
    DSB(ctx[3] + 0x52u) = 0x10u;
    DSB(ctx[3] + 0x53u) = 0x0Au;
    DSB(ctx[3] + 0x54u) = 0u;
    DSD(ctx[3] + 0x10u) = 0u;
    actors_anim_begin(ctx[5], DSD(0x000C9030u + (u32)DSB(ctx[3] + 0x7Au) * 4u), 0x40800000u);
    DSB(ctx[3] + 0x7Eu) = (u8)(DSB(0x000BECF8u) + (u8)r[R_EDX]);
    *eax = 0u;
}
static void m_3a9d8_byte(const u32 *r, u32 *eax)       /* b's high byte added */
{
    u32 ctx[6];
    fighter_ctx_swap(ctx, r[R_EAX]);
    hit_anchor_set(ctx[1], DSD(ctx[5] + 0x18u), 0u);
    DSB(ctx[3] + 0x52u) = 0x10u;
    DSB(ctx[3] + 0x53u) = 0x0Au;
    DSB(ctx[3] + 0x54u) = 0u;
    DSD(ctx[3] + 0x10u) = 0u;
    actors_anim_begin(ctx[5], DSD(0x000C9030u + (u32)DSB(ctx[3] + 0x7Au) * 4u), 0x40400000u);
    DSB(ctx[3] + 0x7Eu) = (u8)(DSB(0x000BECF8u) + (u8)(r[R_EDX] >> 8));
    *eax = 0u;
}
static void b_48254(const u32 *r, u32 *eax)            { b_anim(0x48254u, r, eax); }
static void m_48254_byte(const u32 *r, u32 *eax)       /* the flag from the other side */
{
    u32 rec = r[R_EAX];
    u32 own = (u32)DSB(rec + 0x51u);
    u32 other_side = 1u - own;
    u32 oslot = DS_001077B0 + other_side * 0x94u;
    actors_anim_begin(rec, 0x000ED87Au, 0x40400000u);
    if (DSB(0x00108392u + other_side) != 0u) {
        u32 ch = (u32)DSB(oslot + 0x7Au);
        actors_anim_begin(DSD(oslot), DSD(0x000C8F40u + ch * 4u), 0x40A00000u);
    } else {
        fighter_3a9d8(other_side, 0x0Fu);
        fighter_39834(other_side, (s32)DSB(0x0010780Fu + own * 0x94u));
    }
    *eax = 0u;
}
static void m_48254_side(const u32 *r, u32 *eax)       /* the other side = own */
{
    u32 rec = r[R_EAX];
    u32 own = (u32)DSB(rec + 0x51u);
    u32 other_side = own;
    u32 oslot = DS_001077B0 + other_side * 0x94u;
    actors_anim_begin(rec, 0x000ED87Au, 0x40400000u);
    if (DSB(0x00108392u + own) != 0u) {
        u32 ch = (u32)DSB(oslot + 0x7Au);
        actors_anim_begin(DSD(oslot), DSD(0x000C8F40u + ch * 4u), 0x40A00000u);
    } else {
        fighter_3a9d8(other_side, 0x0Fu);
        fighter_39834(other_side, (s32)DSB(0x0010780Fu + own * 0x94u));
    }
    *eax = 0u;
}
static void m_48254_stream(const u32 *r, u32 *eax)     /* the 0xED8BC stream */
{
    u32 rec = r[R_EAX];
    u32 own = (u32)DSB(rec + 0x51u);
    u32 other_side = 1u - own;
    u32 oslot = DS_001077B0 + other_side * 0x94u;
    actors_anim_begin(rec, 0x000ED8BCu, 0x40400000u);
    if (DSB(0x00108392u + own) != 0u) {
        u32 ch = (u32)DSB(oslot + 0x7Au);
        actors_anim_begin(DSD(oslot), DSD(0x000C8F40u + ch * 4u), 0x40A00000u);
    } else {
        fighter_3a9d8(other_side, 0x0Fu);
        fighter_39834(other_side, (s32)DSB(0x0010780Fu + own * 0x94u));
    }
    *eax = 0u;
}
static void m_48254_pose(const u32 *r, u32 *eax)       /* the other slot's +0x5F */
{
    u32 rec = r[R_EAX];
    u32 own = (u32)DSB(rec + 0x51u);
    u32 other_side = 1u - own;
    u32 oslot = DS_001077B0 + other_side * 0x94u;
    actors_anim_begin(rec, 0x000ED87Au, 0x40400000u);
    if (DSB(0x00108392u + own) != 0u) {
        u32 ch = (u32)DSB(oslot + 0x7Au);
        actors_anim_begin(DSD(oslot), DSD(0x000C8F40u + ch * 4u), 0x40A00000u);
    } else {
        fighter_3a9d8(other_side, 0x0Fu);
        fighter_39834(other_side, (s32)DSB(0x0010780Fu + other_side * 0x94u));
    }
    *eax = 0u;
}
static void m_48254_char(const u32 *r, u32 *eax)       /* the own char for the stream */
{
    u32 rec = r[R_EAX];
    u32 own = (u32)DSB(rec + 0x51u);
    u32 other_side = 1u - own;
    u32 oslot = DS_001077B0 + other_side * 0x94u;
    actors_anim_begin(rec, 0x000ED87Au, 0x40400000u);
    if (DSB(0x00108392u + own) != 0u) {
        u32 ch = (u32)DSB(DS_001077B0 + own * 0x94u + 0x7Au);
        actors_anim_begin(DSD(oslot), DSD(0x000C8F40u + ch * 4u), 0x40A00000u);
    } else {
        fighter_3a9d8(other_side, 0x0Fu);
        fighter_39834(other_side, (s32)DSB(0x0010780Fu + own * 0x94u));
    }
    *eax = 0u;
}
static void m_48254_order(const u32 *r, u32 *eax)      /* the branch before the first call */
{
    u32 rec = r[R_EAX];
    u32 own = (u32)DSB(rec + 0x51u);
    u32 other_side = 1u - own;
    u32 oslot = DS_001077B0 + other_side * 0x94u;
    if (DSB(0x00108392u + own) != 0u) {
        actors_anim_begin(rec, 0x000ED87Au, 0x40400000u);
        u32 ch = (u32)DSB(oslot + 0x7Au);
        actors_anim_begin(DSD(oslot), DSD(0x000C8F40u + ch * 4u), 0x40A00000u);
    } else {
        actors_anim_begin(rec, 0x000ED87Au, 0x40400000u);
        fighter_39834(other_side, (s32)DSB(0x0010780Fu + own * 0x94u));
        fighter_3a9d8(other_side, 0x0Fu);
    }
    *eax = 0u;
}
''')

sub('port/tests/diff_runner.c', r'''{ "fighter_23960",            b_23960,        0x00000000u },
    { "fighter_23960@off",        m_23960_off,    0x00000000u },
    { "fighter_23960@y",          m_23960_y,      0x00000000u },
    { "fighter_23960@desc",       m_23960_desc,   0x00000000u },
    { "fighter_23960@float",      m_23960_float,  0x00000000u },
    { "fighter_23960@x",          m_23960_x,      0x00000000u },
    { "fighter_23a7c",            b_23a7c,        0x00000000u },
    { "fighter_23a7c@a5",         m_23a7c_a5,     0x00000000u },
    { "fighter_23a7c@slot",       m_23a7c_slot,   0x00000000u },
    { "fighter_23a7c@side",       m_23a7c_side,   0x00000000u },
    { "fighter_23a7c@mutant",     m_23a7c_mutant, 0x00000000u },
    { "fighter_23a7c@order",      m_23a7c_order,  0x00000000u },
    ''', r'''{ "fighter_23960",            b_23960,        0x00000000u },
    { "fighter_23960@off",        m_23960_off,    0x00000000u },
    { "fighter_23960@y",          m_23960_y,      0x00000000u },
    { "fighter_23960@desc",       m_23960_desc,   0x00000000u },
    { "fighter_23960@float",      m_23960_float,  0x00000000u },
    { "fighter_23960@x",          m_23960_x,      0x00000000u },
    { "fighter_23a7c",            b_23a7c,        0x00000000u },
    { "fighter_23a7c@a5",         m_23a7c_a5,     0x00000000u },
    { "fighter_23a7c@slot",       m_23a7c_slot,   0x00000000u },
    { "fighter_23a7c@side",       m_23a7c_side,   0x00000000u },
    { "fighter_23a7c@mutant",     m_23a7c_mutant, 0x00000000u },
    { "fighter_23a7c@order",      m_23a7c_order,  0x00000000u },
    { "fighter_3a9d8",            b_3a9d8,        0x00000000u },
    { "fighter_3a9d8@side",       m_3a9d8_side,   0x00000000u },
    { "fighter_3a9d8@stream",     m_3a9d8_stream, 0x00000000u },
    { "fighter_3a9d8@anchor",     m_3a9d8_anchor, 0x00000000u },
    { "fighter_3a9d8@slot",       m_3a9d8_slot,   0x00000000u },
    { "fighter_3a9d8@frame",      m_3a9d8_frame,  0x00000000u },
    { "fighter_3a9d8@byte",       m_3a9d8_byte,   0x00000000u },
    { "fighter_48254",            b_48254,        0x00000000u },
    { "fighter_48254@byte",       m_48254_byte,   0x00000000u },
    { "fighter_48254@side",       m_48254_side,   0x00000000u },
    { "fighter_48254@stream",     m_48254_stream, 0x00000000u },
    { "fighter_48254@pose",       m_48254_pose,   0x00000000u },
    { "fighter_48254@char",       m_48254_char,   0x00000000u },
    { "fighter_48254@order",      m_48254_order,  0x00000000u },
    ''')

sub('port/tests/test_fight.c', r'''    CHECK(found != 0u, "the 0x23A7C child holds the record's slot");
}
int test_p7_23960(void)         { return u6b_run(p7_check_23960); }

/* §P7.5: 0x3A9D8 and 0x48254 through their registrations. */
''', r'''    CHECK(found != 0u, "the 0x23A7C child holds the record's slot");
}
int test_p7_23960(void)         { return u6b_run(p7_check_23960); }

/* §P7.5: 0x3A9D8 and 0x48254 through their registrations. */
static void p7_check_3a9d8(void)
{
    p7_anim_fn f;
    /* 0x3A9D8 side 0: the own slot (ctx[3] = Z_S0) 0x10/0x0A/0/0x10 = 0 and
     * its +0x7E = 0xBECF8 + 0x34; the side-0 record (ctx[5] = Z_R0) on
     * 0xC9030[Z_S0's char] at 3.0; the anchor's y = 0. */
    z_fseed();
    DSB(Z_S0 + 0x7Au) = 3u;
    DSB(Z_S0 + 0x52u) = 0x52u;
    DSB(Z_S0 + 0x53u) = 0x53u;
    DSB(Z_S0 + 0x54u) = 0x54u;
    DSD(Z_S0 + 0x10u) = 0x10101010u;
    DSB(Z_S0 + 0x7Eu) = 0x7Eu;
    DSD(Z_R0 + 0x18u) = 0x18181818u;
    DSD(Z_R0 + 0x1Cu) = 0x1C1C1C1Cu;
    DSD(Z_R0 + 8u) = 0x9999u;
    fighter_3a9d8(0u, 0x1234u);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x52u), 0x10);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x53u), 0x0A);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x54u), 0x00);
    CHECK_EQ_INT((int)DSD(Z_S0 + 0x10u), 0);
    CHECK_EQ_INT((int)DSB(Z_S0 + 0x7Eu), (int)(u8)(DSB(0x000BECF8u) + 0x34u));
    CHECK_EQ_INT((int)DSD(Z_R0 + 8u), (int)DSD(0x000C9030u + 12u));
    CHECK_EQ_INT((int)DSD(Z_R0 + 0x18u), 0x18181818);
    CHECK_EQ_INT((int)DSD(Z_R0 + 0x1Cu), 0);

    /* 0x48254: the flag 1 starts the other record on 0xC8F40[its char] at
     * 5.0; 0 takes 0x3A9D8(other side, 0xF), whose slot stores are visible. */
    f = (p7_anim_fn)(void *)fn_resolve(0x48254u);
    CHECK(f != NULL, "0x48254 is registered");
    if (f != NULL) {
        z_fseed();
        DSB(Z_R0 + 0x51u) = 0u;
        DSB(Z_S1 + 0x7Au) = 2u;
        DSB(0x00108392u) = 1u;
        DSD(Z_R0 + 8u) = 0x9999u;
        DSD(Z_R1 + 8u) = 0x9999u;
        f(Z_R0, 0u);
        CHECK(DSD(Z_R0 + 8u) != 0x9999u, "the own stream started");
        CHECK(DSD(Z_R1 + 8u) != 0x9999u, "the other record's stream started");
        DSB(0x00108392u) = 0u;
        DSB(Z_S1 + 0x52u) = 0x52u;
        DSB(Z_S0 + 0x5Fu) = 0x57u;
        DSD(DS_00107D28) = 0x5A5A5A5Au;
        f(Z_R0, 0u);
        CHECK_EQ_INT((int)DSB(Z_S1 + 0x52u), 0x10);
        CHECK_EQ_INT((int)DSD(DS_00107D28), 0x57);
    }
}
int test_p7_3a9d8(void)         { return u6b_run(p7_check_3a9d8); }

/* §P7.6: the after-table streams 0x23AE0 and 0x29C78. */
''')

sub('tools/tests/test_diff_verify.py', r'''"fighter_23960@off": {"call #2", "call #4", "call #6"},
    "fighter_23960@y": {"call #0", "call #2", "call #4", "call #6"},
    "fighter_23960@desc": {"call #0", "call #2", "call #4", "call #6"},
    "fighter_23960@float": {"byte", "call #2 memory", "call #3 memory", "call #4 memory", "call #5 memory", "call #6 memory", "call #7 memory"},
    "fighter_23960@x": {"call #0", "call #2", "call #4", "call #6"},
    "fighter_23a7c@a5": {"call #0"},
    "fighter_23a7c@slot": {"byte", "call #1 memory"},
    "fighter_23a7c@side": {"byte", "call #1 memory"},
    "fighter_23a7c@mutant": {"byte", "call #1 memory"},
    "fighter_23a7c@order": {"call #1 memory"},
    ''', r'''"fighter_23960@off": {"call #2", "call #4", "call #6"},
    "fighter_23960@y": {"call #0", "call #2", "call #4", "call #6"},
    "fighter_23960@desc": {"call #0", "call #2", "call #4", "call #6"},
    "fighter_23960@float": {"byte", "call #2 memory", "call #3 memory", "call #4 memory", "call #5 memory", "call #6 memory", "call #7 memory"},
    "fighter_23960@x": {"call #0", "call #2", "call #4", "call #6"},
    "fighter_23a7c@a5": {"call #0"},
    "fighter_23a7c@slot": {"byte", "call #1 memory"},
    "fighter_23a7c@side": {"byte", "call #1 memory"},
    "fighter_23a7c@mutant": {"byte", "call #1 memory"},
    "fighter_23a7c@order": {"call #1 memory"},
    "fighter_3a9d8@side": {"call #0"},
    "fighter_3a9d8@stream": {"call #1"},
    "fighter_3a9d8@anchor": {"call #0"},
    "fighter_3a9d8@slot": {"byte", "call #1", "call #1 memory"},
    "fighter_3a9d8@frame": {"call #1"},
    "fighter_3a9d8@byte": {"byte"},
    "fighter_48254@byte": {"call #1", "call #2"},
    "fighter_48254@side": {"call #1", "call #2"},
    "fighter_48254@stream": {"call #0"},
    "fighter_48254@pose": {"call #2"},
    "fighter_48254@char": {"call #1"},
    "fighter_48254@order": {"call #1", "call #2"},
    ''')

print('T5 applied')
sub('tools/tests/test_diff_verify.py', 'P7_MASKS = {"fighter_2bdb8": 0, "fighter_213f0": 0, "fighter_213f4": 0, "fighter_3e424": 0, "fighter_224ec": 0, "fighter_2bde8": 0, "fighter_36114": 0, "fighter_23960": 0, "fighter_23a7c": 0}', 'P7_MASKS = {"fighter_2bdb8": 0, "fighter_213f0": 0, "fighter_213f4": 0, "fighter_3e424": 0, "fighter_224ec": 0, "fighter_2bde8": 0, "fighter_36114": 0, "fighter_23960": 0, "fighter_23a7c": 0, "fighter_3a9d8": 0, "fighter_48254": 0}')

sub('tools/tests/test_diff_verify.py', '        self.assertIn("diff-verify: 158/158 functions VERIFIED; 426/426 mutants detected; 1 named gaps; "\n                      "44/132 rows with callees closed (26 have none).", out.getvalue())', '        self.assertIn("diff-verify: 160/160 functions VERIFIED; 438/438 mutants detected; 1 named gaps; "\n                      "44/134 rows with callees closed (26 have none).", out.getvalue())')

sub('tools/tests/test_diff_verify.py', '                                 0x39280: (), 0x13244: (), 0x2A148: ("edx",), 0x2BCF4: ("edx",),', '                                 0x3A9D8: ("edx",),\n                                 0x39280: (), 0x13244: (), 0x2A148: ("edx",), 0x2BCF4: ("edx",),')

PY
```

Then build and run the task's tests:

```bash
cmake --build build 2>&1 | tail -1
python3 tools/diff_verify.py --self-check --image /tmp/pr_p7_dfv.bin --table /tmp/pr_p7_diff.md | tail -1
PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | grep -E 'FAIL|FAILURES|all checks passed'
```

Expected: the counter line `160/160 functions VERIFIED; 438/438 mutants detected; 1 named gaps; 44/134 rows with callees closed (26 have none).`; every new row VERIFIED with its blocks hit; the new unit check passes. The rows: `fighter_3a9d8` 2 cases 1/1 (`188AC` stub VERIFIED, `2BC30` stub VERIFIED, `33A10` allow unverified), `fighter_48254` 3 4/4 (`2BC30` stub VERIFIED, `39834` stub unverified, `3A9D8` stub VERIFIED); 12 mutants detected.

- [ ] **Step 2: the E2 table.** `make entry-triage` must pass against the regenerated table; then

```bash
./build/diffrun --exe data/game/C/PRAGE.EXE --image-out /tmp/pr_p7_e2img.bin && \
python3 tools/entry_triage.py --image /tmp/pr_p7_e2img.bin \
  --live docs/superpowers/plans/2026-10-01-reverse-e2-live-functions.txt \
  --out docs/superpowers/plans/2026-10-01-reverse-e2-triage.md
```

Expected: `entry-triage: targets 234 unported, 261 ported; supplement 131 (4 unported, 0 stale); untrusted entries 30` and `voice sites outside Ghidra 134: 0 in unported code, 115 in ported code, 19 nowhere`.

- [ ] **Step 3: every new unit assertion can fail.** The `P7_KINDS` entry of every new mutant names the case set that alone catches it (record §P7.10); `test_each_p7_mutant_is_caught_by_what_it_breaks` asserts the sets, and the self-check reports each mutant MISMATCH. For each unit check, mutate the port function it tests (the same one-line change as the matching `m_*` mutant), run `PR_ORACLE_REQUIRED=1 ./build/run_tests`, record the first FAIL line, and restore the code.

- [ ] **Step 4: commit.** Stage the named files (`git add` each; never `git add -A`) and commit:

```bash
git commit -m "fighter: port 0x3A9D8 0x48254; E2 table regenerated (track P batch 7, task 5)"
```

---

### Task 6: `0x23AE0`, `0x29C78` (the after-table streams), and gp-u10-ending's drop

**Files:** as Task 2; the gp drop: `port/tests/test_platform.c`, `Makefile`, `AGENTS.md`.

**Interfaces:** produces `void fighter_23ae0(u32 rec)`, `void fighter_29c78(u32 rec)`; `P7_2BCF4`, `P7_2A17C`, `p7_23ae0_case`; the specs, bindings and mutants; `p7_check_streams`, `test_p7_streams`. Consumes Task 5's `P7_SPECS`; P6's `P7_2BCF4`-style seam, P1's `P7_2A17C`.

- [ ] **Step 1: the specs, the unit check, the port, the seams, the bindings and the mutants, in one script.** It applies in order: the specs and the expectations (`tools/diff_verify.py`), the port and the seams (`port/src/game/fighter.c`, `actors.c`), the unit check (`port/tests/test_fight.c`, `test.h`), the bindings and mutants (`port/tests/diff_runner.c`), the test expectations (`tools/tests/test_diff_verify.py`), and, for Tasks 2 and 6, the gp drop, the Makefile pins/provenance and the AGENTS.md clause. It asserts every anchor occurs once, so it either applies cleanly or stops naming the file and the text.

```bash
python3 - <<'PY'
import pathlib


def sub(path, old, new):
    p = pathlib.Path(path)
    s = p.read_text()
    n = s.count(old)
    assert n == 1, "%s: %d occurrences of %r" % (path, n, old[:70])
    p.write_text(s.replace(old, new))

sub('port/src/game/fighter.c', r'''    } else {
        fighter_3a9d8(other_side, 0x0Fu);                   /* 0x482B2..0x482B9 */
        fighter_39834(other_side, (s32)DSB(0x0010780Fu + own * 0x94u));   /* 0x482BE..0x482D7 */
    }
}
''', r'''    } else {
        fighter_3a9d8(other_side, 0x0Fu);                   /* 0x482B2..0x482B9 */
        fighter_39834(other_side, (s32)DSB(0x0010780Fu + own * 0x94u));   /* 0x482BE..0x482D7 */
    }
}
/* 0x23AE0 — record §P7.6. The animation target after the 7-dword table 0x23AC4
 * (the stream dwords 0xD4FD4, 0xE1948): with the other side's slot set, its
 * record's +0x29 bit 3, +0x2E = 0x64 and +0x4E = 1; the char's word from the
 * table (0x46B6..0x46BA, the default 0x46B9) sought on that record; the
 * palette handle 0x105FEBC; the word 0x105B4C = 1. */
void fighter_23ae0(u32 rec)
{
    static const u16 p7_23ae0_val[7] = { 0x46B9u, 0x46BAu, 0x46B8u, 0x46B6u, 0x46B7u, 0x46B9u, 0x46BAu };
    u32 other = DSD(DS_001077A8 + ((((u32)DSB(rec + 0x51u)) ^ 1u) & 0xFFu) * 4u);  /* 0x23AE3..0x23AED */
    u32 orec, ch, val;
    if (other == 0u) return;                                /* 0x23AF4/0x23AF6 */
    orec = DSD(other);                                      /* 0x23AF8 */
    DSB(orec + 0x29u) |= 8u;                                /* 0x23AFA */
    DSW(orec + 0x2Eu) = 0x0064u;                            /* 0x23B00 */
    DSB(orec + 0x4Eu) = 1u;                                 /* 0x23B08 */
    ch = (u32)DSB(other + 0x7Au);                           /* 0x23B0C */
    val = ch <= 6u ? (u32)p7_23ae0_val[ch] : 0x46B9u;       /* 0x23B0F..0x23B3C */
    actors_anim_seek(orec, val);                            /* 0x23B41..0x23B4D */
    actor_pset_palette(orec, 0u, 0x00105FEBCu);             /* 0x23B52..0x23B56 */
    DSW(0x00105B4Cu) = 1u;                                  /* 0x23B5B */
}


/* 0x29C78 — record §P7.6. The animation target after the 7-dword table 0x29C5C
 * (the 14 stream dwords 0xD2BDA...). Every table entry is 0x29CA8, so the own
 * char's switch is degenerate: the body is 0x2A17C(rec, 0, 0x105FEBC) alone. */
void fighter_29c78(u32 rec)
{
    actor_pset_palette(rec, 0u, 0x00105FEBCu);              /* 0x29CA8..0x29CB1 */
}


''')

sub('port/src/game/fighter.h', r'''void fighter_3a9d8(u32 side, u32 b);
void fighter_48254(u32 rec);
''', r'''void fighter_3a9d8(u32 side, u32 b);
void fighter_48254(u32 rec);
void fighter_23ae0(u32 rec);
void fighter_29c78(u32 rec);
''')

sub('port/src/game/actors.c', r'''§P7.5. Track P batch 7's Task 5
 * animation target. */
static void anim_code_48254(u32 rec, u32 arg)
{
    (void)arg;
    fighter_48254(rec);
}


/* PORT: record 2026-10-04-reverse-p7 ''', r'''§P7.5. Track P batch 7's Task 5
 * animation target. */
static void anim_code_48254(u32 rec, u32 arg)
{
    (void)arg;
    fighter_48254(rec);
}


/* PORT: record 2026-10-04-reverse-p7 §P7.6. Track P batch 7's Task 6
 * animation targets (outside E2). */
static void anim_code_23AE0(u32 rec, u32 arg)
{
    (void)arg;
    fighter_23ae0(rec);
}
static void anim_code_29C78(u32 rec, u32 arg)
{
    (void)arg;
    fighter_29c78(rec);
}


/* PORT: record 2026-10-04-reverse-p7 ''')

sub('port/src/game/actors.c', r'''§P7.5. Track P batch 7's Task 5
     * animation target. */
    fn_register(0x48254u, (void (*)(void))anim_code_48254);
    /* PORT: record 2026-10-04-reverse-p7 ''', r'''§P7.5. Track P batch 7's Task 5
     * animation target. */
    fn_register(0x48254u, (void (*)(void))anim_code_48254);
    /* PORT: record 2026-10-04-reverse-p7 §P7.6. Track P batch 7's Task 6
     * animation targets (outside E2). */
    fn_register(0x23AE0u, (void (*)(void))anim_code_23AE0);
    fn_register(0x29C78u, (void (*)(void))anim_code_29C78);
    /* PORT: record 2026-10-04-reverse-p7 ''')

sub('tools/diff_verify.py', r'''# 0x3A9D8 (record §P7.5): 0x3A95C's twin (the stream table 0xC9030). EAX = side, EDX = b.
    # 0x33A10 runs on both sides (allow); 0x188AC(ctx[1], the other record's +0x18, 0); the own
    # slot (ctx[3]) 0x10/0x0A/0/0x10 = 0; 0x2BC30(ctx[5], the char stream, 3.0); ctx[3].+0x7E =
    # byte 0xBECF8 + (u8)b.
    Spec("fighter_3a9d8", 0x3A9D8, [
        Case("s0", {"eax": 0, "edx": 0x1234},
             {**SLOT_PTRS, E3_REC2 + 0x18: le32(0x18181818), E3_REC + 0x18: le32(0x28282828),
              DS_SLOTS + 0x10: b"\x20\x20\x20\x20",
              DS_SLOTS + 0x52: b"\x52\x53\x54\x55", DS_SLOTS + 0x94 + 0x52: b"\x62\x63\x64\x65",
              DS_SLOTS + 0x94 + 0x10: b"\x10\x10\x10\x10", DS_SLOTS + 0x7A: b"\x03",
              DS_SLOTS + 0x94 + 0x7A: b"\x02", DS_SLOTS + 0x7E: b"\x7e\x7f",
              DS_SLOTS + 0x94 + 0x7E: b"\x8e\x8f"}),
        Case("s1", {"eax": 1, "edx": 0x0080},
             {**SLOT_PTRS, E3_REC2 + 0x18: le32(0x18181818), E3_REC + 0x18: le32(0x28282828),
              DS_SLOTS + 0x10: b"\x20\x20\x20\x20",
              DS_SLOTS + 0x52: b"\x52\x53\x54\x55", DS_SLOTS + 0x94 + 0x52: b"\x62\x63\x64\x65",
              DS_SLOTS + 0x94 + 0x10: b"\x10\x10\x10\x10", DS_SLOTS + 0x7A: b"\x03",
              DS_SLOTS + 0x94 + 0x7A: b"\x02", DS_SLOTS + 0x7E: b"\x7e\x7f",
              DS_SLOTS + 0x94 + 0x7E: b"\x8e\x8f"}),
    ], allow_calls=(0x33A10,), calls=(ANCHOR, ANIM_BEGIN), eax_mask=0,
       mutants=("@side", "@stream", "@anchor", "@slot", "@frame", "@byte")),
    # 0x48254 (record §P7.5): 0x482E4's twin (the stream 0xED87A and 0x3A9D8). The flag
    # 0x108392[own side] selects the other record on 0xC8F40[other char] at 5.0, else
    # 0x3A9D8(other side, 0xF) and 0x39834(other side, the own slot's +0x5F).
    Spec("fighter_48254", 0x48254, [
        Case("s0", {"eax": E3_REC, "edx": 0x1234},
             {**SLOT_PTRS, E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
              DS_SLOTS + 0x94 + 0x7A: b"\x02", DS_SLOTS + 0x7A: b"\x03",
              DS_SLOTS + 0x5F: b"\x5f", DS_SLOTS + 0x94 + 0x5F: b"\x9f",
              0x108392: b"\x01", 0x108392 + 1: b"\x00"}),
        Case("s1", {"eax": E3_REC2, "edx": 0x5678},
             {**SLOT_PTRS, E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
              DS_SLOTS + 0x94 + 0x7A: b"\x02", DS_SLOTS + 0x7A: b"\x03",
              DS_SLOTS + 0x5F: b"\x5f", DS_SLOTS + 0x94 + 0x5F: b"\x9f",
              0x108392: b"\x00", 0x108392 + 1: b"\x01"}),
        Case("z0", {"eax": E3_REC, "edx": 0x1234},
             {**SLOT_PTRS, E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
              DS_SLOTS + 0x94 + 0x7A: b"\x04", DS_SLOTS + 0x7A: b"\x03",
              DS_SLOTS + 0x5F: b"\x5f", DS_SLOTS + 0x94 + 0x5F: b"\x9f",
              0x108392: b"\x00", 0x108392 + 1: b"\x00"}),
    ], calls=(ANIM_BEGIN, P7_3A9D8, POSE), eax_mask=0,
       mutants=("@byte", "@side", "@stream", "@pose", "@char", "@order")),
    ''', r'''# 0x3A9D8 (record §P7.5): 0x3A95C's twin (the stream table 0xC9030). EAX = side, EDX = b.
    # 0x33A10 runs on both sides (allow); 0x188AC(ctx[1], the other record's +0x18, 0); the own
    # slot (ctx[3]) 0x10/0x0A/0/0x10 = 0; 0x2BC30(ctx[5], the char stream, 3.0); ctx[3].+0x7E =
    # byte 0xBECF8 + (u8)b.
    Spec("fighter_3a9d8", 0x3A9D8, [
        Case("s0", {"eax": 0, "edx": 0x1234},
             {**SLOT_PTRS, E3_REC2 + 0x18: le32(0x18181818), E3_REC + 0x18: le32(0x28282828),
              DS_SLOTS + 0x10: b"\x20\x20\x20\x20",
              DS_SLOTS + 0x52: b"\x52\x53\x54\x55", DS_SLOTS + 0x94 + 0x52: b"\x62\x63\x64\x65",
              DS_SLOTS + 0x94 + 0x10: b"\x10\x10\x10\x10", DS_SLOTS + 0x7A: b"\x03",
              DS_SLOTS + 0x94 + 0x7A: b"\x02", DS_SLOTS + 0x7E: b"\x7e\x7f",
              DS_SLOTS + 0x94 + 0x7E: b"\x8e\x8f"}),
        Case("s1", {"eax": 1, "edx": 0x0080},
             {**SLOT_PTRS, E3_REC2 + 0x18: le32(0x18181818), E3_REC + 0x18: le32(0x28282828),
              DS_SLOTS + 0x10: b"\x20\x20\x20\x20",
              DS_SLOTS + 0x52: b"\x52\x53\x54\x55", DS_SLOTS + 0x94 + 0x52: b"\x62\x63\x64\x65",
              DS_SLOTS + 0x94 + 0x10: b"\x10\x10\x10\x10", DS_SLOTS + 0x7A: b"\x03",
              DS_SLOTS + 0x94 + 0x7A: b"\x02", DS_SLOTS + 0x7E: b"\x7e\x7f",
              DS_SLOTS + 0x94 + 0x7E: b"\x8e\x8f"}),
    ], allow_calls=(0x33A10,), calls=(ANCHOR, ANIM_BEGIN), eax_mask=0,
       mutants=("@side", "@stream", "@anchor", "@slot", "@frame", "@byte")),
    # 0x48254 (record §P7.5): 0x482E4's twin (the stream 0xED87A and 0x3A9D8). The flag
    # 0x108392[own side] selects the other record on 0xC8F40[other char] at 5.0, else
    # 0x3A9D8(other side, 0xF) and 0x39834(other side, the own slot's +0x5F).
    Spec("fighter_48254", 0x48254, [
        Case("s0", {"eax": E3_REC, "edx": 0x1234},
             {**SLOT_PTRS, E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
              DS_SLOTS + 0x94 + 0x7A: b"\x02", DS_SLOTS + 0x7A: b"\x03",
              DS_SLOTS + 0x5F: b"\x5f", DS_SLOTS + 0x94 + 0x5F: b"\x9f",
              0x108392: b"\x01", 0x108392 + 1: b"\x00"}),
        Case("s1", {"eax": E3_REC2, "edx": 0x5678},
             {**SLOT_PTRS, E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
              DS_SLOTS + 0x94 + 0x7A: b"\x02", DS_SLOTS + 0x7A: b"\x03",
              DS_SLOTS + 0x5F: b"\x5f", DS_SLOTS + 0x94 + 0x5F: b"\x9f",
              0x108392: b"\x00", 0x108392 + 1: b"\x01"}),
        Case("z0", {"eax": E3_REC, "edx": 0x1234},
             {**SLOT_PTRS, E3_REC + 0x51: b"\x00", E3_REC2 + 0x51: b"\x01",
              DS_SLOTS + 0x94 + 0x7A: b"\x04", DS_SLOTS + 0x7A: b"\x03",
              DS_SLOTS + 0x5F: b"\x5f", DS_SLOTS + 0x94 + 0x5F: b"\x9f",
              0x108392: b"\x00", 0x108392 + 1: b"\x00"}),
    ], calls=(ANIM_BEGIN, P7_3A9D8, POSE), eax_mask=0,
       mutants=("@byte", "@side", "@stream", "@pose", "@char", "@order")),
    Spec("fighter_23ae0", 0x23AE0,
         [p7_23ae0_case("c%d" % i, E3_REC, i) for i in range(7)]
         + [p7_23ae0_case("c7", E3_REC, 7),
            Case("o0", {"eax": E3_REC}, {DS_SLOTS - 8: le32(DS_SLOTS) + le32(0), E3_REC + 0x51: b"\x00",
                                         0x105B4C: b"\x4c\x4d"})],
         calls=(P7_2BCF4, P7_2A17C), eax_mask=0,
         mutants=("@char", "@val", "@side", "@pal", "@word", "@bit", "@seek", "@order")),
    # 0x29C78 (record §P7.6): every entry of the table 0x29C5C is 0x29CA8, so the body is
    # 0x2A17C(rec, 0, 0x105FEBC) alone. d0's own char 7 exercises the degenerate switch.
    Spec("fighter_29c78", 0x29C78, [
        Case("s0", {"eax": E3_REC, "edx": 0x1234}, {**SLOT_PTRS, E3_REC + 0x51: b"\x00"}),
        Case("s1", {"eax": E3_REC2, "edx": 0x5678}, {**SLOT_PTRS, E3_REC2 + 0x51: b"\x01"}),
        Case("d0", {"eax": E3_REC, "edx": 0x1234}, {**SLOT_PTRS, E3_REC + 0x51: b"\x00",
                                                    DS_SLOTS + 0x7A: b"\x07"}),
    ], calls=(P7_2A17C,), eax_mask=0, mutants=("@pal", "@rec")),
    ''')

sub('port/tests/diff_runner.c', r'''        fighter_39834(other_side, (s32)DSB(0x0010780Fu + own * 0x94u));
        fighter_3a9d8(other_side, 0x0Fu);
    }
    *eax = 0u;
}
''', r'''        fighter_39834(other_side, (s32)DSB(0x0010780Fu + own * 0x94u));
        fighter_3a9d8(other_side, 0x0Fu);
    }
    *eax = 0u;
}
static void b_23ae0(const u32 *r, u32 *eax)            { b_anim(0x23AE0u, r, eax); }
static void m_23ae0_char(const u32 *r, u32 *eax)       /* the own record's char */
{
    static const u16 val[7] = { 0x46B9u, 0x46BAu, 0x46B8u, 0x46B6u, 0x46B7u, 0x46B9u, 0x46BAu };
    u32 rec = r[R_EAX];
    u32 other = DSD(DS_001077A8 + ((((u32)DSB(rec + 0x51u)) ^ 1u) & 0xFFu) * 4u);
    if (other == 0u) { *eax = 0u; return; }
    {
        u32 orec = DSD(other);
        u32 ch;
        DSB(orec + 0x29u) |= 8u;
        DSW(orec + 0x2Eu) = 0x0064u;
        DSB(orec + 0x4Eu) = 1u;
        ch = (u32)DSB(rec + 0x7Au);
        actors_anim_seek(orec, ch <= 6u ? (u32)val[ch] : 0x46B9u);
        actor_pset_palette(orec, 0u, 0x00105FECBu);
        DSW(0x00105B4Cu) = 1u;
    }
    *eax = 0u;
}
static void m_23ae0_val(const u32 *r, u32 *eax)        /* the default 0x46B9 for every char */
{
    u32 rec = r[R_EAX];
    u32 other = DSD(DS_001077A8 + ((((u32)DSB(rec + 0x51u)) ^ 1u) & 0xFFu) * 4u);
    if (other == 0u) { *eax = 0u; return; }
    {
        u32 orec = DSD(other);
        DSB(orec + 0x29u) |= 8u;
        DSW(orec + 0x2Eu) = 0x0064u;
        DSB(orec + 0x4Eu) = 1u;
        actors_anim_seek(orec, 0x46B9u);
        actor_pset_palette(orec, 0u, 0x00105FECBu);
        DSW(0x00105B4Cu) = 1u;
    }
    *eax = 0u;
}
static void m_23ae0_side(const u32 *r, u32 *eax)       /* the own side's slot */
{
    static const u16 val[7] = { 0x46B9u, 0x46BAu, 0x46B8u, 0x46B6u, 0x46B7u, 0x46B9u, 0x46BAu };
    u32 rec = r[R_EAX];
    u32 other = DSD(DS_001077A8 + (((u32)DSB(rec + 0x51u)) & 1u) * 4u);
    if (other == 0u) { *eax = 0u; return; }
    {
        u32 orec = DSD(other);
        u32 ch;
        DSB(orec + 0x29u) |= 8u;
        DSW(orec + 0x2Eu) = 0x0064u;
        DSB(orec + 0x4Eu) = 1u;
        ch = (u32)DSB(other + 0x7Au);
        actors_anim_seek(orec, ch <= 6u ? (u32)val[ch] : 0x46B9u);
        actor_pset_palette(orec, 0u, 0x00105FECBu);
        DSW(0x00105B4Cu) = 1u;
    }
    *eax = 0u;
}
static void m_23ae0_pal(const u32 *r, u32 *eax)        /* the palette handle 0 */
{
    static const u16 val[7] = { 0x46B9u, 0x46BAu, 0x46B8u, 0x46B6u, 0x46B7u, 0x46B9u, 0x46BAu };
    u32 rec = r[R_EAX];
    u32 other = DSD(DS_001077A8 + ((((u32)DSB(rec + 0x51u)) ^ 1u) & 0xFFu) * 4u);
    if (other == 0u) { *eax = 0u; return; }
    {
        u32 orec = DSD(other);
        u32 ch;
        DSB(orec + 0x29u) |= 8u;
        DSW(orec + 0x2Eu) = 0x0064u;
        DSB(orec + 0x4Eu) = 1u;
        ch = (u32)DSB(other + 0x7Au);
        actors_anim_seek(orec, ch <= 6u ? (u32)val[ch] : 0x46B9u);
        actor_pset_palette(orec, 0u, 0u);
        DSW(0x00105B4Cu) = 1u;
    }
    *eax = 0u;
}
static void m_23ae0_word(const u32 *r, u32 *eax)       /* +0x2E = 0x63 */
{
    static const u16 val[7] = { 0x46B9u, 0x46BAu, 0x46B8u, 0x46B6u, 0x46B7u, 0x46B9u, 0x46BAu };
    u32 rec = r[R_EAX];
    u32 other = DSD(DS_001077A8 + ((((u32)DSB(rec + 0x51u)) ^ 1u) & 0xFFu) * 4u);
    if (other == 0u) { *eax = 0u; return; }
    {
        u32 orec = DSD(other);
        u32 ch;
        DSB(orec + 0x29u) |= 8u;
        DSW(orec + 0x2Eu) = 0x0063u;
        DSB(orec + 0x4Eu) = 1u;
        ch = (u32)DSB(other + 0x7Au);
        actors_anim_seek(orec, ch <= 6u ? (u32)val[ch] : 0x46B9u);
        actor_pset_palette(orec, 0u, 0x00105FECBu);
        DSW(0x00105B4Cu) = 1u;
    }
    *eax = 0u;
}
static void m_23ae0_bit(const u32 *r, u32 *eax)        /* +0x29 set, not OR-ed */
{
    static const u16 val[7] = { 0x46B9u, 0x46BAu, 0x46B8u, 0x46B6u, 0x46B7u, 0x46B9u, 0x46BAu };
    u32 rec = r[R_EAX];
    u32 other = DSD(DS_001077A8 + ((((u32)DSB(rec + 0x51u)) ^ 1u) & 0xFFu) * 4u);
    if (other == 0u) { *eax = 0u; return; }
    {
        u32 orec = DSD(other);
        u32 ch;
        DSB(orec + 0x29u) = 8u;
        DSW(orec + 0x2Eu) = 0x0064u;
        DSB(orec + 0x4Eu) = 1u;
        ch = (u32)DSB(other + 0x7Au);
        actors_anim_seek(orec, ch <= 6u ? (u32)val[ch] : 0x46B9u);
        actor_pset_palette(orec, 0u, 0x00105FECBu);
        DSW(0x00105B4Cu) = 1u;
    }
    *eax = 0u;
}
static void m_23ae0_seek(const u32 *r, u32 *eax)       /* the seek on the record, not the slot's record */
{
    static const u16 val[7] = { 0x46B9u, 0x46BAu, 0x46B8u, 0x46B6u, 0x46B7u, 0x46B9u, 0x46BAu };
    u32 rec = r[R_EAX];
    u32 other = DSD(DS_001077A8 + ((((u32)DSB(rec + 0x51u)) ^ 1u) & 0xFFu) * 4u);
    if (other == 0u) { *eax = 0u; return; }
    {
        u32 orec = DSD(other);
        u32 ch;
        DSB(orec + 0x29u) |= 8u;
        DSW(orec + 0x2Eu) = 0x0064u;
        DSB(orec + 0x4Eu) = 1u;
        ch = (u32)DSB(other + 0x7Au);
        actors_anim_seek(rec, ch <= 6u ? (u32)val[ch] : 0x46B9u);
        actor_pset_palette(orec, 0u, 0x00105FECBu);
        DSW(0x00105B4Cu) = 1u;
    }
    *eax = 0u;
}
static void m_23ae0_order(const u32 *r, u32 *eax)      /* the stores after the seek */
{
    static const u16 val[7] = { 0x46B9u, 0x46BAu, 0x46B8u, 0x46B6u, 0x46B7u, 0x46B9u, 0x46BAu };
    u32 rec = r[R_EAX];
    u32 other = DSD(DS_001077A8 + ((((u32)DSB(rec + 0x51u)) ^ 1u) & 0xFFu) * 4u);
    if (other == 0u) { *eax = 0u; return; }
    {
        u32 orec = DSD(other);
        u32 ch = (u32)DSB(other + 0x7Au);
        actors_anim_seek(orec, ch <= 6u ? (u32)val[ch] : 0x46B9u);
        DSB(orec + 0x29u) |= 8u;
        DSW(orec + 0x2Eu) = 0x0064u;
        DSB(orec + 0x4Eu) = 1u;
        actor_pset_palette(orec, 0u, 0x00105FECBu);
        DSW(0x00105B4Cu) = 1u;
    }
    *eax = 0u;
}
static void b_29c78(const u32 *r, u32 *eax)            { b_anim(0x29C78u, r, eax); }
static void m_29c78_pal(const u32 *r, u32 *eax)        /* the palette handle 0 */
{
    actor_pset_palette(r[R_EAX], 0u, 0u);
    *eax = 0u;
}
static void m_29c78_rec(const u32 *r, u32 *eax)        /* the word 0xFFFF */
{
    actor_pset_palette(r[R_EAX], 0xFFFFu, 0x00105FECBu);
    *eax = 0u;
}
''')

sub('port/tests/diff_runner.c', r'''{ "fighter_3a9d8",            b_3a9d8,        0x00000000u },
    { "fighter_3a9d8@side",       m_3a9d8_side,   0x00000000u },
    { "fighter_3a9d8@stream",     m_3a9d8_stream, 0x00000000u },
    { "fighter_3a9d8@anchor",     m_3a9d8_anchor, 0x00000000u },
    { "fighter_3a9d8@slot",       m_3a9d8_slot,   0x00000000u },
    { "fighter_3a9d8@frame",      m_3a9d8_frame,  0x00000000u },
    { "fighter_3a9d8@byte",       m_3a9d8_byte,   0x00000000u },
    { "fighter_48254",            b_48254,        0x00000000u },
    { "fighter_48254@byte",       m_48254_byte,   0x00000000u },
    { "fighter_48254@side",       m_48254_side,   0x00000000u },
    { "fighter_48254@stream",     m_48254_stream, 0x00000000u },
    { "fighter_48254@pose",       m_48254_pose,   0x00000000u },
    { "fighter_48254@char",       m_48254_char,   0x00000000u },
    { "fighter_48254@order",      m_48254_order,  0x00000000u },
    ''', r'''{ "fighter_3a9d8",            b_3a9d8,        0x00000000u },
    { "fighter_3a9d8@side",       m_3a9d8_side,   0x00000000u },
    { "fighter_3a9d8@stream",     m_3a9d8_stream, 0x00000000u },
    { "fighter_3a9d8@anchor",     m_3a9d8_anchor, 0x00000000u },
    { "fighter_3a9d8@slot",       m_3a9d8_slot,   0x00000000u },
    { "fighter_3a9d8@frame",      m_3a9d8_frame,  0x00000000u },
    { "fighter_3a9d8@byte",       m_3a9d8_byte,   0x00000000u },
    { "fighter_48254",            b_48254,        0x00000000u },
    { "fighter_48254@byte",       m_48254_byte,   0x00000000u },
    { "fighter_48254@side",       m_48254_side,   0x00000000u },
    { "fighter_48254@stream",     m_48254_stream, 0x00000000u },
    { "fighter_48254@pose",       m_48254_pose,   0x00000000u },
    { "fighter_48254@char",       m_48254_char,   0x00000000u },
    { "fighter_48254@order",      m_48254_order,  0x00000000u },
    { "fighter_23ae0",            b_23ae0,        0x00000000u },
    { "fighter_23ae0@char",       m_23ae0_char,   0x00000000u },
    { "fighter_23ae0@val",        m_23ae0_val,    0x00000000u },
    { "fighter_23ae0@side",       m_23ae0_side,   0x00000000u },
    { "fighter_23ae0@pal",        m_23ae0_pal,    0x00000000u },
    { "fighter_23ae0@word",       m_23ae0_word,   0x00000000u },
    { "fighter_23ae0@bit",        m_23ae0_bit,    0x00000000u },
    { "fighter_23ae0@seek",       m_23ae0_seek,   0x00000000u },
    { "fighter_23ae0@order",      m_23ae0_order,  0x00000000u },
    { "fighter_29c78",            b_29c78,        0x00000000u },
    { "fighter_29c78@pal",        m_29c78_pal,    0x00000000u },
    { "fighter_29c78@rec",        m_29c78_rec,    0x00000000u },
    ''')

sub('port/tests/test_fight.c', r'''    }
}
int test_p7_3a9d8(void)         { return u6b_run(p7_check_3a9d8); }

/* §P7.6: the after-table streams 0x23AE0 and 0x29C78. */
''', r'''    }
}
int test_p7_3a9d8(void)         { return u6b_run(p7_check_3a9d8); }

/* §P7.6: the after-table streams 0x23AE0 and 0x29C78. */
static void p7_check_streams(void)
{
    p7_anim_fn f;
    /* 0x23AE0 char 3: the other record's +0x29 bit 3, +0x2E = 0x64, +0x4E = 1,
     * the table word 0x46B6 at its +8, 0x105B4C = 1; char 7 takes the default
     * 0x46B9. */
    f = (p7_anim_fn)(void *)fn_resolve(0x23AE0u);
    CHECK(f != NULL, "0x23AE0 is registered");
    if (f != NULL) {
        z_fseed();
        DSB(Z_R0 + 0x51u) = 0u;
        DSB(Z_S1 + 0x7Au) = 3u;
        DSB(Z_R1 + 0x29u) = 0x29u;
        DSW(Z_R1 + 0x2Eu) = 0x2E2Eu;
        DSB(Z_R1 + 0x4Eu) = 0x4Eu;
        DSW(0x00105B4Cu) = 0x4C4Cu;
        DSD(Z_R1 + 8u) = 0x9999u;
        f(Z_R0, 0u);
        CHECK_EQ_INT((int)DSB(Z_R1 + 0x29u), 0x29);
        CHECK_EQ_INT((int)DSW(Z_R1 + 0x2Eu), 0x64);
        CHECK_EQ_INT((int)DSB(Z_R1 + 0x4Eu), 1);
        CHECK_EQ_INT((int)DSD(Z_R1 + 8u), 0x46B6);
        CHECK_EQ_INT((int)DSW(0x00105B4Cu), 1);
        DSB(Z_S1 + 0x7Au) = 7u;
        DSD(Z_R1 + 8u) = 0x9999u;
        f(Z_R0, 0u);
        CHECK_EQ_INT((int)DSD(Z_R1 + 8u), 0x46B9);
    }

    /* 0x29C78: the degenerate switch; 0x2A17C(rec, 0, 0x105FEBC): the record's
     * pset word takes 0, or 0x800 with its +0x5F set. */
    f = (p7_anim_fn)(void *)fn_resolve(0x29C78u);
    CHECK(f != NULL, "0x29C78 is registered");
    if (f != NULL) {
        u32 pset;
        z_fseed();
        DSB(Z_R0 + 0x51u) = 0u;
        DSB(Z_R0 + 0x5Fu) = 0u;
        pset = actor_pset(Z_R0);
        DSW(pset + 2u) = 0x1234u;
        f(Z_R0, 0u);
        CHECK_EQ_INT((int)DSW(pset + 2u), 0);
        DSB(Z_R0 + 0x5Fu) = 1u;
        DSW(pset + 2u) = 0x1234u;
        f(Z_R0, 0u);
        CHECK_EQ_INT((int)(DSW(pset + 2u) & 0x800u), 0x800);
    }
}
int test_p7_streams(void)       { return u6b_run(p7_check_streams); }

/* §P7.7: 0x4B03C through its registration. */
''')

sub('tools/tests/test_diff_verify.py', r'''"fighter_3a9d8@side": {"call #0"},
    "fighter_3a9d8@stream": {"call #1"},
    "fighter_3a9d8@anchor": {"call #0"},
    "fighter_3a9d8@slot": {"byte", "call #1", "call #1 memory"},
    "fighter_3a9d8@frame": {"call #1"},
    "fighter_3a9d8@byte": {"byte"},
    "fighter_48254@byte": {"call #1", "call #2"},
    "fighter_48254@side": {"call #1", "call #2"},
    "fighter_48254@stream": {"call #0"},
    "fighter_48254@pose": {"call #2"},
    "fighter_48254@char": {"call #1"},
    "fighter_48254@order": {"call #1", "call #2"},
    ''', r'''"fighter_3a9d8@side": {"call #0"},
    "fighter_3a9d8@stream": {"call #1"},
    "fighter_3a9d8@anchor": {"call #0"},
    "fighter_3a9d8@slot": {"byte", "call #1", "call #1 memory"},
    "fighter_3a9d8@frame": {"call #1"},
    "fighter_3a9d8@byte": {"byte"},
    "fighter_48254@byte": {"call #1", "call #2"},
    "fighter_48254@side": {"call #1", "call #2"},
    "fighter_48254@stream": {"call #0"},
    "fighter_48254@pose": {"call #2"},
    "fighter_48254@char": {"call #1"},
    "fighter_48254@order": {"call #1", "call #2"},
    "fighter_23ae0@char": {"call #0", "call #1"},
    "fighter_23ae0@val": {"call #0", "call #1"},
    "fighter_23ae0@side": {"byte", "call #0", "call #0 memory", "call #1", "call #1 memory"},
    "fighter_23ae0@pal": {"call #1"},
    "fighter_23ae0@word": {"byte", "call #0 memory", "call #1", "call #1 memory"},
    "fighter_23ae0@bit": {"byte", "call #0 memory", "call #1", "call #1 memory"},
    "fighter_23ae0@seek": {"call #0", "call #1"},
    "fighter_23ae0@order": {"call #0 memory", "call #1"},
    "fighter_29c78@pal": {"call #0"},
    "fighter_29c78@rec": {"call #0"},
    ''')

print('T6 applied')
sub('tools/tests/test_diff_verify.py', 'P7_MASKS = {"fighter_2bdb8": 0, "fighter_213f0": 0, "fighter_213f4": 0, "fighter_3e424": 0, "fighter_224ec": 0, "fighter_2bde8": 0, "fighter_36114": 0, "fighter_23960": 0, "fighter_23a7c": 0, "fighter_3a9d8": 0, "fighter_48254": 0}', 'P7_MASKS = {"fighter_2bdb8": 0, "fighter_213f0": 0, "fighter_213f4": 0, "fighter_3e424": 0, "fighter_224ec": 0, "fighter_2bde8": 0, "fighter_36114": 0, "fighter_23960": 0, "fighter_23a7c": 0, "fighter_3a9d8": 0, "fighter_48254": 0, "fighter_23ae0": 0, "fighter_29c78": 0}')

sub('tools/tests/test_diff_verify.py', '        self.assertIn("diff-verify: 160/160 functions VERIFIED; 438/438 mutants detected; 1 named gaps; "\n                      "44/134 rows with callees closed (26 have none).", out.getvalue())', '        self.assertIn("diff-verify: 162/162 functions VERIFIED; 448/448 mutants detected; 1 named gaps; "\n                      "45/136 rows with callees closed (26 have none).", out.getvalue())')



sub('port/tests/test_platform.c', r''' * §W.14), measured on its full replay to its X record (f = 0x26E1): the two wipe hooks of
 * §G.24 (0x29D60, a bare `ret`, from f = 0x286; 0x5D812, the runtime stub, from f = 0x3FE),
 * and the death-animation stream's target 0x29C78 (outside E2, record reverse-p1 §P1.2, dword
 * 0xD2BDA) at f = 0x14FE in mode 0xD. The stream target 0x37DD4 (E2 anim-target row, dword''', r''' * §W.14), measured on its full replay to its X record (f = 0x26E1): the two wipe hooks of
 * §G.24 (0x29D60, a bare `ret`, from f = 0x286; 0x5D812, the runtime stub, from f = 0x3FE).
 * The death-animation stream's target 0x29C78 (outside E2, record reverse-p1 §P1.2, dword
 * 0xD2BDA, f = 0x14FE in mode 0xD) is ported by track P batch 7 (record 2026-10-04-reverse-p7
 * §P7.6), so the replay no longer misses it; the pins were re-measured (record §P7.8). The
 * stream target 0x37DD4 (E2 anim-target row, dword''')
sub('port/tests/test_platform.c', '''static const fnm_pair k_miss_gp_u10_ending[] = {
    { 0x29D60u, "frontend_mode_1b_step" },
    { 0x5D812u, "frontend_mode_1b_step" },
    { 0x29C78u, "anim_indirect" },
};''', '''static const fnm_pair k_miss_gp_u10_ending[] = {
    { 0x29D60u, "frontend_mode_1b_step" },
    { 0x5D812u, "frontend_mode_1b_step" },
};''')
sub('Makefile', '''# ported: the set loses that row alone and every pin above is unchanged (each + 1 fails).
# Re-measure: a P batch that ports 0x29C78 (P7) drops its row and re-measures the U10 set;
# TRACE/WIN are at the replay's end, so they cannot rise.''', '''# ported: the set loses that row alone and every pin above is unchanged (each + 1 fails).
# Re-measured by track P batch 7 (record 2026-10-04-reverse-p7 §P7.8) once 0x29C78 is ported: the set
# loses that row (fn-miss PR_GP_DUMP distinct=4 dropped=0: 0x5D812 actor_spawn and set_dead, 0x29D60
# and 0x5D812 from frontend_mode_1b_step) and every pin above is exact and unchanged: frames 331
# (332 fails), trace/win 9954 (9955 is unreachable, the replay's end), path 30 (31 unreachable),
# MAX_START 83. TRACE/WIN are at the replay's end, so they cannot rise.''')
sub('AGENTS.md', '''  TRACE and WIN pins rise 2364/3162 -> 3503 (MIN_FIRST 346 and MAX_START 100 unchanged and exact)
  — record 2026-10-04-reverse-p7 §P7.8).''', '''  TRACE and WIN pins rise 2364/3162 -> 3503 (MIN_FIRST 346 and MAX_START 100 unchanged and exact),
  and `0x29C78` P7: U10's set loses it (distinct=4) with every pin exact and unchanged — record
  2026-10-04-reverse-p7 §P7.8).''')

PY
```

Then build and run the task's tests:

```bash
cmake --build build 2>&1 | tail -1
python3 tools/diff_verify.py --self-check --image /tmp/pr_p7_dfv.bin --table /tmp/pr_p7_diff.md | tail -1
PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | grep -E 'FAIL|FAILURES|all checks passed'
```

Expected: the counter line `162/162 functions VERIFIED; 448/448 mutants detected; 1 named gaps; 45/136 rows with callees closed (26 have none).`; every new row VERIFIED with its blocks hit; the new unit check passes. The rows: `fighter_23ae0` 9 cases 10/10 (`2A17C` stub VERIFIED, `2BCF4` stub unverified), `fighter_29c78` 3 3/3 (`2A17C` stub VERIFIED); 10 mutants detected.

- [ ] **Step 2: the E2 table.** `make entry-triage` must pass against the regenerated table; then

```bash
./build/diffrun --exe data/game/C/PRAGE.EXE --image-out /tmp/pr_p7_e2img.bin && \
python3 tools/entry_triage.py --image /tmp/pr_p7_e2img.bin \
  --live docs/superpowers/plans/2026-10-01-reverse-e2-live-functions.txt \
  --out docs/superpowers/plans/2026-10-01-reverse-e2-triage.md
```

Expected: `entry-triage: unchanged (both are outside E2's universe)` and `voice sites outside Ghidra 134: 0 in unported code, 115 in ported code, 19 nowhere`.

-

 

- [ ] **Step 4: every new unit assertion can fail.** The `P7_KINDS` entry of every new mutant names the case set that alone catches it (record §P7.10); `test_each_p7_mutant_is_caught_by_what_it_breaks` asserts the sets, and the self-check reports each mutant MISMATCH. For each unit check, mutate the port function it tests (the same one-line change as the matching `m_*` mutant), run `PR_ORACLE_REQUIRED=1 ./build/run_tests`, record the first FAIL line, and restore the code.

- [ ] **Step 5: commit.** Stage the named files (`git add` each; never `git add -A`) and commit:

```bash
git commit -m "fighter: port 0x23AE0 0x29C78; gp-u10-ending drops 0x29C78 (track P batch 7, task 6)"
```

---

### Task 7: `0x4B03C` (the 30-stream target) and the three seams

**Files:** as Task 2 (the E2 table does not change: `0x4B03C` is outside E2's universe).

**Interfaces:** produces `void fighter_4b03c(u32 rec)`; the `PR_SEAM` lines of `set_dead` (`0x2B150`), `fighter_41310` (`0x41310`) and `actor_type_49444` (`0x49444`); `P7_2B150`, `P7_41310`, `P7_49444`, `P7_4B03C_SEED`; the spec, bindings and mutants; `p7_check_4b03c`, `test_p7_4b03c`. Consumes Task 6's `P7_SPECS`; E3's `VOICE`.

- [ ] **Step 1: the specs, the unit check, the port, the seams, the bindings and the mutants, in one script.** It applies in order: the specs and the expectations (`tools/diff_verify.py`), the port and the seams (`port/src/game/fighter.c`, `actors.c`), the unit check (`port/tests/test_fight.c`, `test.h`), the bindings and mutants (`port/tests/diff_runner.c`), the test expectations (`tools/tests/test_diff_verify.py`), and, for Tasks 2 and 6, the gp drop, the Makefile pins/provenance and the AGENTS.md clause. It asserts every anchor occurs once, so it either applies cleanly or stops naming the file and the text.

```bash
python3 - <<'PY'
import pathlib


def sub(path, old, new):
    p = pathlib.Path(path)
    s = p.read_text()
    n = s.count(old)
    assert n == 1, "%s: %d occurrences of %r" % (path, n, old[:70])
    p.write_text(s.replace(old, new))

sub('port/src/game/fighter.c', r''' * char's switch is degenerate: the body is 0x2A17C(rec, 0, 0x105FEBC) alone. */
void fighter_29c78(u32 rec)
{
    actor_pset_palette(rec, 0u, 0x00105FEBCu);              /* 0x29CA8..0x29CB1 */
}
''', r''' * char's switch is degenerate: the body is 0x2A17C(rec, 0, 0x105FEBC) alone. */
void fighter_29c78(u32 rec)
{
    actor_pset_palette(rec, 0u, 0x00105FEBCu);              /* 0x29CA8..0x29CB1 */
}
/* 0x4B03C — record §P7.7. The animation target after the 6-dword table 0x4B024
 * (the 30 stream dwords 0xEE26E...): with the record's +0x14 slot set, the
 * byte (slot+8's record +0x48) - 0x20 selects 0x10/0xE/0x15/0xC/0xA (0xD
 * default) and the slot's +0x5A loses it down to 0; voice 0xD6, voice 0xCE,
 * the record dead, 0x41310 on the slot's +0x21/-0x20 counters (mode 0x22/0x24
 * excluded), the 0x1088A4/0x1088A2 counters, then 0x49444. */
void fighter_4b03c(u32 rec)
{
    static const u8 p7_4b03c_dl[6] = { 0x10u, 0x0Eu, 0x15u, 0x0Cu, 0x0Au, 0x0Du };
    u32 slot = DSD(rec + 0x14u);                            /* 0x4B042 */
    u8 dl, v;
    u32 esi;
    if (slot == 0u) return;                                 /* 0x4B045/0x4B047 */
    {
        u8 t = (u8)(DSB(DSD(slot + 8u) + 0x48u) - 0x20u);   /* 0x4B04D..0x4B053 */
        dl = t <= 5u ? p7_4b03c_dl[t] : 0x0Du;              /* 0x4B057..0x4B07C */
    }
    esi = (u32)DSB(slot + 0x20u);                           /* 0x4B07E */
    v = DSB(0x0010780Au + esi * 0x94u);                     /* 0x4B082..0x4B093 */
    if (dl < v)
        DSB(0x0010780Au + esi * 0x94u) = (u8)(v - dl);      /* 0x4B09D/0x4B09F */
    else
        DSB(0x0010780Au + esi * 0x94u) = 0u;                /* 0x4B0A7/0x4B0A9 */
    (void)sound_voice(0xD6u);                               /* 0x4B0AF/0x4B0B4 */
    (void)sound_voice(0xCEu);                               /* 0x4B0B9/0x4B0BE */
    actor_set_dead(rec);                                    /* 0x4B0C3/0x4B0C5 */
    if (DSW(DS_00104B00) != 0x22u && DSW(DS_00104B00) != 0x24u) {   /* 0x4B0CA..0x4B0DA */
        DSB(0x0010889Eu + (u32)DSB(slot + 0x21u)) = 1u;     /* 0x4B0DC..0x4B0E1 */
        fighter_41310((u32)DSB(slot + 0x21u), -10000);      /* 0x4B0E8..0x4B0F2 */
        if (DSB(slot + 0x21u) == DSB(slot + 0x20u))
            fighter_41310((u32)DSB(slot + 0x20u), -30000);  /* 0x4B101..0x4B10A */
        else
            fighter_41310((u32)DSB(slot + 0x20u), 10000);   /* 0x4B10C..0x4B110 */
    }
    DSB(0x001088A4u + (u32)DSB(slot + 0x20u)) += 1u;        /* 0x4B11A..0x4B11F */
    if (DSB(slot + 0x21u) == DSB(slot + 0x20u))
        DSB(0x001088A2u + (u32)DSB(slot + 0x21u)) += 1u;    /* 0x4B12D..0x4B132 */
    actor_type_49444(rec);                                  /* 0x4B138/0x4B13A */
}
''')

sub('port/src/game/fighter.h', r'''void fighter_23ae0(u32 rec);
void fighter_29c78(u32 rec);
''', r'''void fighter_23ae0(u32 rec);
void fighter_29c78(u32 rec);
void fighter_4b03c(u32 rec);

#endif /* PRAGE_GAME_FIGHTER_H */
''')

sub('port/src/game/actors.c', r'''static void set_dead(u32 rec)
{
    DSB(rec + 0x28) |= 0x08;''', r'''static void set_dead(u32 rec)
{
    PR_SEAM(0x2B150u, rec);
    DSB(rec + 0x28) |= 0x08;''')

sub('port/src/game/fighter.c', r'''void fighter_41310(u32 side, s32 delta)
{
    u32 rec;''', r'''void fighter_41310(u32 side, s32 delta)
{
    PR_SEAM(0x41310u, side, delta);
    u32 rec;''')

sub('port/src/game/actors.c', r'''void actor_type_49444(u32 rec)
{
    u32 rec2 = DSD(rec + 0x14);''', r'''void actor_type_49444(u32 rec)
{
    PR_SEAM(0x49444u, rec);
    u32 rec2 = DSD(rec + 0x14);''')

sub('port/src/game/actors.c', r'''§P7.6. Track P batch 7's Task 6
 * animation targets (outside E2). */
static void anim_code_23AE0(u32 rec, u32 arg)
{
    (void)arg;
    fighter_23ae0(rec);
}
static void anim_code_29C78(u32 rec, u32 arg)
{
    (void)arg;
    fighter_29c78(rec);
}


/* PORT: record 2026-10-04-reverse-p7 ''', r'''§P7.6. Track P batch 7's Task 6
 * animation targets (outside E2). */
static void anim_code_23AE0(u32 rec, u32 arg)
{
    (void)arg;
    fighter_23ae0(rec);
}
static void anim_code_29C78(u32 rec, u32 arg)
{
    (void)arg;
    fighter_29c78(rec);
}


/* PORT: record 2026-10-04-reverse-p7 §P7.7. Track P batch 7's Task 7
 * animation target (outside E2). */
static void anim_code_4B03C(u32 rec, u32 arg)
{
    (void)arg;
    fighter_4b03c(rec);
}
''')

sub('port/src/game/actors.c', r'''§P7.6. Track P batch 7's Task 6
     * animation targets (outside E2). */
    fn_register(0x23AE0u, (void (*)(void))anim_code_23AE0);
    fn_register(0x29C78u, (void (*)(void))anim_code_29C78);
    /* PORT: record 2026-10-04-reverse-p7 ''', r'''§P7.6. Track P batch 7's Task 6
     * animation targets (outside E2). */
    fn_register(0x23AE0u, (void (*)(void))anim_code_23AE0);
    fn_register(0x29C78u, (void (*)(void))anim_code_29C78);
    /* PORT: record 2026-10-04-reverse-p7 §P7.7. Track P batch 7's Task 7
     * animation target (outside E2). */
    fn_register(0x4B03Cu, (void (*)(void))anim_code_4B03C);
    return 1;''')

sub('tools/diff_verify.py', r'''Spec("fighter_23ae0", 0x23AE0,
         [p7_23ae0_case("c%d" % i, E3_REC, i) for i in range(7)]
         + [p7_23ae0_case("c7", E3_REC, 7),
            Case("o0", {"eax": E3_REC}, {DS_SLOTS - 8: le32(DS_SLOTS) + le32(0), E3_REC + 0x51: b"\x00",
                                         0x105B4C: b"\x4c\x4d"})],
         calls=(P7_2BCF4, P7_2A17C), eax_mask=0,
         mutants=("@char", "@val", "@side", "@pal", "@word", "@bit", "@seek", "@order")),
    # 0x29C78 (record §P7.6): every entry of the table 0x29C5C is 0x29CA8, so the body is
    # 0x2A17C(rec, 0, 0x105FEBC) alone. d0's own char 7 exercises the degenerate switch.
    Spec("fighter_29c78", 0x29C78, [
        Case("s0", {"eax": E3_REC, "edx": 0x1234}, {**SLOT_PTRS, E3_REC + 0x51: b"\x00"}),
        Case("s1", {"eax": E3_REC2, "edx": 0x5678}, {**SLOT_PTRS, E3_REC2 + 0x51: b"\x01"}),
        Case("d0", {"eax": E3_REC, "edx": 0x1234}, {**SLOT_PTRS, E3_REC + 0x51: b"\x00",
                                                    DS_SLOTS + 0x7A: b"\x07"}),
    ], calls=(P7_2A17C,), eax_mask=0, mutants=("@pal", "@rec")),
    ''', r'''Spec("fighter_23ae0", 0x23AE0,
         [p7_23ae0_case("c%d" % i, E3_REC, i) for i in range(7)]
         + [p7_23ae0_case("c7", E3_REC, 7),
            Case("o0", {"eax": E3_REC}, {DS_SLOTS - 8: le32(DS_SLOTS) + le32(0), E3_REC + 0x51: b"\x00",
                                         0x105B4C: b"\x4c\x4d"})],
         calls=(P7_2BCF4, P7_2A17C), eax_mask=0,
         mutants=("@char", "@val", "@side", "@pal", "@word", "@bit", "@seek", "@order")),
    # 0x29C78 (record §P7.6): every entry of the table 0x29C5C is 0x29CA8, so the body is
    # 0x2A17C(rec, 0, 0x105FEBC) alone. d0's own char 7 exercises the degenerate switch.
    Spec("fighter_29c78", 0x29C78, [
        Case("s0", {"eax": E3_REC, "edx": 0x1234}, {**SLOT_PTRS, E3_REC + 0x51: b"\x00"}),
        Case("s1", {"eax": E3_REC2, "edx": 0x5678}, {**SLOT_PTRS, E3_REC2 + 0x51: b"\x01"}),
        Case("d0", {"eax": E3_REC, "edx": 0x1234}, {**SLOT_PTRS, E3_REC + 0x51: b"\x00",
                                                    DS_SLOTS + 0x7A: b"\x07"}),
    ], calls=(P7_2A17C,), eax_mask=0, mutants=("@pal", "@rec")),
    Spec("fighter_4b03c", 0x4B03C,
         [Case("t%d" % i, {"eax": E3_REC}, {**P7_4B03C_SEED, E3_OUT + 0x48: bytes([0x20 + i])})
          for i in range(6)]
         + [
             Case("t6", {"eax": E3_REC}, {**P7_4B03C_SEED, E3_OUT + 0x48: b"\x26"}),
             Case("t7", {"eax": E3_REC}, {**P7_4B03C_SEED, E3_OUT + 0x48: b"\x1f"}),
             Case("z0", {"eax": E3_REC}, {**P7_4B03C_SEED, E3_OUT + 0x48: b"\x20", 0x10780A: b"\x08"}),
             Case("m0", {"eax": E3_REC}, {**P7_4B03C_SEED, E3_OUT + 0x48: b"\x20", 0x104B00: b"\x22\x00"}),
             Case("m1", {"eax": E3_REC}, {**P7_4B03C_SEED, E3_OUT + 0x48: b"\x20", 0x104B00: b"\x24\x00"}),
             Case("eq0", {"eax": E3_REC}, {**P7_4B03C_SEED, E3_OUT + 0x48: b"\x20", E3_SLOT + 0x21: b"\x00"}),
             Case("o0", {"eax": E3_REC}, {E3_REC + 0x14: le32(0)}),
         ],
         calls=(VOICE, VOICE, P7_2B150, P7_41310, P7_49444), eax_mask=0,
         mutants=("@type", "@val", "@sub", "@mode", "@plus", "@eq", "@dead", "@cam", "@tear", "@order", "@voice")),
]




SPECS = [
''')

sub('port/tests/diff_runner.c', r'''static void m_29c78_rec(const u32 *r, u32 *eax)        /* the word 0xFFFF */
{
    actor_pset_palette(r[R_EAX], 0xFFFFu, 0x00105FECBu);
    *eax = 0u;
}
''', r'''static void m_29c78_rec(const u32 *r, u32 *eax)        /* the word 0xFFFF */
{
    actor_pset_palette(r[R_EAX], 0xFFFFu, 0x00105FECBu);
    *eax = 0u;
}
static void b_4b03c(const u32 *r, u32 *eax)            { b_anim(0x4B03Cu, r, eax); }
static u8 p7_4b03c_dl(u32 rec)
{
    static const u8 val[6] = { 0x10u, 0x0Eu, 0x15u, 0x0Cu, 0x0Au, 0x0Du };
    u8 t = (u8)(DSB(DSD(rec + 0x14u) + 8u + 0x48u) - 0x20u);
    return t <= 5u ? val[t] : 0x0Du;
}
static void m_4b03c_type(const u32 *r, u32 *eax)       /* the type from the slot's own +0x20 */
{
    u32 rec = r[R_EAX];
    u32 slot = DSD(rec + 0x14u);
    u8 t, dl, v;
    u32 esi;
    if (slot == 0u) { *eax = 0u; return; }
    t = (u8)(DSB(slot + 0x20u) - 0x20u);
    dl = t <= 5u ? (t == 0u ? 0x10u : t == 1u ? 0x0Eu : t == 2u ? 0x15u : t == 3u ? 0x0Cu :
                    t == 4u ? 0x0Au : 0x0Du) : 0x0Du;
    esi = (u32)DSB(slot + 0x20u);
    v = DSB(0x0010780Au + esi * 0x94u);
    if (dl < v) DSB(0x0010780Au + esi * 0x94u) = (u8)(v - dl);
    else DSB(0x0010780Au + esi * 0x94u) = 0u;
    (void)sound_voice(0xD6u);
    (void)sound_voice(0xCEu);
    actor_set_dead(rec);
    if (DSW(DS_00104B00) != 0x22u && DSW(DS_00104B00) != 0x24u) {
        DSB(0x0010889Eu + (u32)DSB(slot + 0x21u)) = 1u;
        fighter_41310((u32)DSB(slot + 0x21u), -10000);
        if (DSB(slot + 0x21u) == DSB(slot + 0x20u))
            fighter_41310((u32)DSB(slot + 0x20u), -30000);
        else
            fighter_41310((u32)DSB(slot + 0x20u), 10000);
    }
    DSB(0x001088A4u + (u32)DSB(slot + 0x20u)) += 1u;
    if (DSB(slot + 0x21u) == DSB(slot + 0x20u))
        DSB(0x001088A2u + (u32)DSB(slot + 0x21u)) += 1u;
    actor_type_49444(rec);
    *eax = 0u;
}
static void m_4b03c_val(const u32 *r, u32 *eax)        /* 0x0D for every type */
{
    u32 rec = r[R_EAX];
    u32 slot = DSD(rec + 0x14u);
    u8 dl, v;
    u32 esi;
    if (slot == 0u) { *eax = 0u; return; }
    dl = 0x0Du;
    esi = (u32)DSB(slot + 0x20u);
    v = DSB(0x0010780Au + esi * 0x94u);
    if (dl < v) DSB(0x0010780Au + esi * 0x94u) = (u8)(v - dl);
    else DSB(0x0010780Au + esi * 0x94u) = 0u;
    (void)sound_voice(0xD6u);
    (void)sound_voice(0xCEu);
    actor_set_dead(rec);
    if (DSW(DS_00104B00) != 0x22u && DSW(DS_00104B00) != 0x24u) {
        DSB(0x0010889Eu + (u32)DSB(slot + 0x21u)) = 1u;
        fighter_41310((u32)DSB(slot + 0x21u), -10000);
        if (DSB(slot + 0x21u) == DSB(slot + 0x20u))
            fighter_41310((u32)DSB(slot + 0x20u), -30000);
        else
            fighter_41310((u32)DSB(slot + 0x20u), 10000);
    }
    DSB(0x001088A4u + (u32)DSB(slot + 0x20u)) += 1u;
    if (DSB(slot + 0x21u) == DSB(slot + 0x20u))
        DSB(0x001088A2u + (u32)DSB(slot + 0x21u)) += 1u;
    actor_type_49444(rec);
    *eax = 0u;
}
static void m_4b03c_sub(const u32 *r, u32 *eax)        /* the +0x5A store as an add */
{
    u32 rec = r[R_EAX];
    u32 slot = DSD(rec + 0x14u);
    u8 dl, v;
    u32 esi;
    if (slot == 0u) { *eax = 0u; return; }
    dl = p7_4b03c_dl(rec);
    esi = (u32)DSB(slot + 0x20u);
    v = DSB(0x0010780Au + esi * 0x94u);
    DSB(0x0010780Au + esi * 0x94u) = (u8)(v + dl);
    (void)sound_voice(0xD6u);
    (void)sound_voice(0xCEu);
    actor_set_dead(rec);
    if (DSW(DS_00104B00) != 0x22u && DSW(DS_00104B00) != 0x24u) {
        DSB(0x0010889Eu + (u32)DSB(slot + 0x21u)) = 1u;
        fighter_41310((u32)DSB(slot + 0x21u), -10000);
        if (DSB(slot + 0x21u) == DSB(slot + 0x20u))
            fighter_41310((u32)DSB(slot + 0x20u), -30000);
        else
            fighter_41310((u32)DSB(slot + 0x20u), 10000);
    }
    DSB(0x001088A4u + (u32)DSB(slot + 0x20u)) += 1u;
    if (DSB(slot + 0x21u) == DSB(slot + 0x20u))
        DSB(0x001088A2u + (u32)DSB(slot + 0x21u)) += 1u;
    actor_type_49444(rec);
    *eax = 0u;
}
static void m_4b03c_mode(const u32 *r, u32 *eax)       /* the 0x24 check dropped */
{
    u32 rec = r[R_EAX];
    u32 slot = DSD(rec + 0x14u);
    u8 dl, v;
    u32 esi;
    if (slot == 0u) { *eax = 0u; return; }
    dl = p7_4b03c_dl(rec);
    esi = (u32)DSB(slot + 0x20u);
    v = DSB(0x0010780Au + esi * 0x94u);
    if (dl < v) DSB(0x0010780Au + esi * 0x94u) = (u8)(v - dl);
    else DSB(0x0010780Au + esi * 0x94u) = 0u;
    (void)sound_voice(0xD6u);
    (void)sound_voice(0xCEu);
    actor_set_dead(rec);
    if (DSW(DS_00104B00) != 0x22u) {
        DSB(0x0010889Eu + (u32)DSB(slot + 0x21u)) = 1u;
        fighter_41310((u32)DSB(slot + 0x21u), -10000);
        if (DSB(slot + 0x21u) == DSB(slot + 0x20u))
            fighter_41310((u32)DSB(slot + 0x20u), -30000);
        else
            fighter_41310((u32)DSB(slot + 0x20u), 10000);
    }
    DSB(0x001088A4u + (u32)DSB(slot + 0x20u)) += 1u;
    if (DSB(slot + 0x21u) == DSB(slot + 0x20u))
        DSB(0x001088A2u + (u32)DSB(slot + 0x21u)) += 1u;
    actor_type_49444(rec);
    *eax = 0u;
}
static void m_4b03c_plus(const u32 *r, u32 *eax)       /* the +0x1088A4 index from +0x21 */
{
    u32 rec = r[R_EAX];
    u32 slot = DSD(rec + 0x14u);
    u8 dl, v;
    u32 esi;
    if (slot == 0u) { *eax = 0u; return; }
    dl = p7_4b03c_dl(rec);
    esi = (u32)DSB(slot + 0x20u);
    v = DSB(0x0010780Au + esi * 0x94u);
    if (dl < v) DSB(0x0010780Au + esi * 0x94u) = (u8)(v - dl);
    else DSB(0x0010780Au + esi * 0x94u) = 0u;
    (void)sound_voice(0xD6u);
    (void)sound_voice(0xCEu);
    actor_set_dead(rec);
    if (DSW(DS_00104B00) != 0x22u && DSW(DS_00104B00) != 0x24u) {
        DSB(0x0010889Eu + (u32)DSB(slot + 0x21u)) = 1u;
        fighter_41310((u32)DSB(slot + 0x21u), -10000);
        if (DSB(slot + 0x21u) == DSB(slot + 0x20u))
            fighter_41310((u32)DSB(slot + 0x20u), -30000);
        else
            fighter_41310((u32)DSB(slot + 0x20u), 10000);
    }
    DSB(0x001088A4u + (u32)DSB(slot + 0x21u)) += 1u;
    if (DSB(slot + 0x21u) == DSB(slot + 0x20u))
        DSB(0x001088A2u + (u32)DSB(slot + 0x21u)) += 1u;
    actor_type_49444(rec);
    *eax = 0u;
}
static void m_4b03c_eq(const u32 *r, u32 *eax)         /* the equality branch inverted */
{
    u32 rec = r[R_EAX];
    u32 slot = DSD(rec + 0x14u);
    u8 dl, v;
    u32 esi;
    if (slot == 0u) { *eax = 0u; return; }
    dl = p7_4b03c_dl(rec);
    esi = (u32)DSB(slot + 0x20u);
    v = DSB(0x0010780Au + esi * 0x94u);
    if (dl < v) DSB(0x0010780Au + esi * 0x94u) = (u8)(v - dl);
    else DSB(0x0010780Au + esi * 0x94u) = 0u;
    (void)sound_voice(0xD6u);
    (void)sound_voice(0xCEu);
    actor_set_dead(rec);
    if (DSW(DS_00104B00) != 0x22u && DSW(DS_00104B00) != 0x24u) {
        DSB(0x0010889Eu + (u32)DSB(slot + 0x21u)) = 1u;
        fighter_41310((u32)DSB(slot + 0x21u), -10000);
        if (DSB(slot + 0x21u) != DSB(slot + 0x20u))
            fighter_41310((u32)DSB(slot + 0x20u), -30000);
        else
            fighter_41310((u32)DSB(slot + 0x20u), 10000);
    }
    DSB(0x001088A4u + (u32)DSB(slot + 0x20u)) += 1u;
    if (DSB(slot + 0x21u) == DSB(slot + 0x20u))
        DSB(0x001088A2u + (u32)DSB(slot + 0x21u)) += 1u;
    actor_type_49444(rec);
    *eax = 0u;
}
static void m_4b03c_dead(const u32 *r, u32 *eax)       /* 0x2B150 on the slot's +8 record */
{
    u32 rec = r[R_EAX];
    u32 slot = DSD(rec + 0x14u);
    u8 dl, v;
    u32 esi;
    if (slot == 0u) { *eax = 0u; return; }
    dl = p7_4b03c_dl(rec);
    esi = (u32)DSB(slot + 0x20u);
    v = DSB(0x0010780Au + esi * 0x94u);
    if (dl < v) DSB(0x0010780Au + esi * 0x94u) = (u8)(v - dl);
    else DSB(0x0010780Au + esi * 0x94u) = 0u;
    (void)sound_voice(0xD6u);
    (void)sound_voice(0xCEu);
    actor_set_dead(DSD(slot + 8u));
    if (DSW(DS_00104B00) != 0x22u && DSW(DS_00104B00) != 0x24u) {
        DSB(0x0010889Eu + (u32)DSB(slot + 0x21u)) = 1u;
        fighter_41310((u32)DSB(slot + 0x21u), -10000);
        if (DSB(slot + 0x21u) == DSB(slot + 0x20u))
            fighter_41310((u32)DSB(slot + 0x20u), -30000);
        else
            fighter_41310((u32)DSB(slot + 0x20u), 10000);
    }
    DSB(0x001088A4u + (u32)DSB(slot + 0x20u)) += 1u;
    if (DSB(slot + 0x21u) == DSB(slot + 0x20u))
        DSB(0x001088A2u + (u32)DSB(slot + 0x21u)) += 1u;
    actor_type_49444(rec);
    *eax = 0u;
}
static void m_4b03c_cam(const u32 *r, u32 *eax)        /* the 0x41310 side from +0x20 */
{
    u32 rec = r[R_EAX];
    u32 slot = DSD(rec + 0x14u);
    u8 dl, v;
    u32 esi;
    if (slot == 0u) { *eax = 0u; return; }
    dl = p7_4b03c_dl(rec);
    esi = (u32)DSB(slot + 0x20u);
    v = DSB(0x0010780Au + esi * 0x94u);
    if (dl < v) DSB(0x0010780Au + esi * 0x94u) = (u8)(v - dl);
    else DSB(0x0010780Au + esi * 0x94u) = 0u;
    (void)sound_voice(0xD6u);
    (void)sound_voice(0xCEu);
    actor_set_dead(rec);
    if (DSW(DS_00104B00) != 0x22u && DSW(DS_00104B00) != 0x24u) {
        DSB(0x0010889Eu + (u32)DSB(slot + 0x21u)) = 1u;
        fighter_41310((u32)DSB(slot + 0x20u), -10000);
        if (DSB(slot + 0x21u) == DSB(slot + 0x20u))
            fighter_41310((u32)DSB(slot + 0x20u), -30000);
        else
            fighter_41310((u32)DSB(slot + 0x20u), 10000);
    }
    DSB(0x001088A4u + (u32)DSB(slot + 0x20u)) += 1u;
    if (DSB(slot + 0x21u) == DSB(slot + 0x20u))
        DSB(0x001088A2u + (u32)DSB(slot + 0x21u)) += 1u;
    actor_type_49444(rec);
    *eax = 0u;
}
static void m_4b03c_tear(const u32 *r, u32 *eax)       /* 0x49444 skipped */
{
    u32 rec = r[R_EAX];
    u32 slot = DSD(rec + 0x14u);
    u8 dl, v;
    u32 esi;
    if (slot == 0u) { *eax = 0u; return; }
    dl = p7_4b03c_dl(rec);
    esi = (u32)DSB(slot + 0x20u);
    v = DSB(0x0010780Au + esi * 0x94u);
    if (dl < v) DSB(0x0010780Au + esi * 0x94u) = (u8)(v - dl);
    else DSB(0x0010780Au + esi * 0x94u) = 0u;
    (void)sound_voice(0xD6u);
    (void)sound_voice(0xCEu);
    actor_set_dead(rec);
    if (DSW(DS_00104B00) != 0x22u && DSW(DS_00104B00) != 0x24u) {
        DSB(0x0010889Eu + (u32)DSB(slot + 0x21u)) = 1u;
        fighter_41310((u32)DSB(slot + 0x21u), -10000);
        if (DSB(slot + 0x21u) == DSB(slot + 0x20u))
            fighter_41310((u32)DSB(slot + 0x20u), -30000);
        else
            fighter_41310((u32)DSB(slot + 0x20u), 10000);
    }
    DSB(0x001088A4u + (u32)DSB(slot + 0x20u)) += 1u;
    if (DSB(slot + 0x21u) == DSB(slot + 0x20u))
        DSB(0x001088A2u + (u32)DSB(slot + 0x21u)) += 1u;
    *eax = 0u;
}
static void m_4b03c_order(const u32 *r, u32 *eax)      /* the +0x1088A4 increment before the voices */
{
    u32 rec = r[R_EAX];
    u32 slot = DSD(rec + 0x14u);
    u8 dl, v;
    u32 esi;
    if (slot == 0u) { *eax = 0u; return; }
    dl = p7_4b03c_dl(rec);
    esi = (u32)DSB(slot + 0x20u);
    v = DSB(0x0010780Au + esi * 0x94u);
    if (dl < v) DSB(0x0010780Au + esi * 0x94u) = (u8)(v - dl);
    else DSB(0x0010780Au + esi * 0x94u) = 0u;
    DSB(0x001088A4u + (u32)DSB(slot + 0x20u)) += 1u;
    (void)sound_voice(0xD6u);
    (void)sound_voice(0xCEu);
    actor_set_dead(rec);
    if (DSW(DS_00104B00) != 0x22u && DSW(DS_00104B00) != 0x24u) {
        DSB(0x0010889Eu + (u32)DSB(slot + 0x21u)) = 1u;
        fighter_41310((u32)DSB(slot + 0x21u), -10000);
        if (DSB(slot + 0x21u) == DSB(slot + 0x20u))
            fighter_41310((u32)DSB(slot + 0x20u), -30000);
        else
            fighter_41310((u32)DSB(slot + 0x20u), 10000);
    }
    if (DSB(slot + 0x21u) == DSB(slot + 0x20u))
        DSB(0x001088A2u + (u32)DSB(slot + 0x21u)) += 1u;
    actor_type_49444(rec);
    *eax = 0u;
}
static void m_4b03c_voice(const u32 *r, u32 *eax)      /* voice 0xD5 */
{
    u32 rec = r[R_EAX];
    u32 slot = DSD(rec + 0x14u);
    u8 dl, v;
    u32 esi;
    if (slot == 0u) { *eax = 0u; return; }
    dl = p7_4b03c_dl(rec);
    esi = (u32)DSB(slot + 0x20u);
    v = DSB(0x0010780Au + esi * 0x94u);
    if (dl < v) DSB(0x0010780Au + esi * 0x94u) = (u8)(v - dl);
    else DSB(0x0010780Au + esi * 0x94u) = 0u;
    (void)sound_voice(0xD5u);
    (void)sound_voice(0xCEu);
    actor_set_dead(rec);
    if (DSW(DS_00104B00) != 0x22u && DSW(DS_00104B00) != 0x24u) {
        DSB(0x0010889Eu + (u32)DSB(slot + 0x21u)) = 1u;
        fighter_41310((u32)DSB(slot + 0x21u), -10000);
        if (DSB(slot + 0x21u) == DSB(slot + 0x20u))
            fighter_41310((u32)DSB(slot + 0x20u), -30000);
        else
            fighter_41310((u32)DSB(slot + 0x20u), 10000);
    }
    DSB(0x001088A4u + (u32)DSB(slot + 0x20u)) += 1u;
    if (DSB(slot + 0x21u) == DSB(slot + 0x20u))
        DSB(0x001088A2u + (u32)DSB(slot + 0x21u)) += 1u;
    actor_type_49444(rec);
    *eax = 0u;
}

static const binding_t k_bindings[] = {
''')

sub('port/tests/diff_runner.c', r'''{ "fighter_23ae0",            b_23ae0,        0x00000000u },
    { "fighter_23ae0@char",       m_23ae0_char,   0x00000000u },
    { "fighter_23ae0@val",        m_23ae0_val,    0x00000000u },
    { "fighter_23ae0@side",       m_23ae0_side,   0x00000000u },
    { "fighter_23ae0@pal",        m_23ae0_pal,    0x00000000u },
    { "fighter_23ae0@word",       m_23ae0_word,   0x00000000u },
    { "fighter_23ae0@bit",        m_23ae0_bit,    0x00000000u },
    { "fighter_23ae0@seek",       m_23ae0_seek,   0x00000000u },
    { "fighter_23ae0@order",      m_23ae0_order,  0x00000000u },
    { "fighter_29c78",            b_29c78,        0x00000000u },
    { "fighter_29c78@pal",        m_29c78_pal,    0x00000000u },
    { "fighter_29c78@rec",        m_29c78_rec,    0x00000000u },
    ''', r'''{ "fighter_23ae0",            b_23ae0,        0x00000000u },
    { "fighter_23ae0@char",       m_23ae0_char,   0x00000000u },
    { "fighter_23ae0@val",        m_23ae0_val,    0x00000000u },
    { "fighter_23ae0@side",       m_23ae0_side,   0x00000000u },
    { "fighter_23ae0@pal",        m_23ae0_pal,    0x00000000u },
    { "fighter_23ae0@word",       m_23ae0_word,   0x00000000u },
    { "fighter_23ae0@bit",        m_23ae0_bit,    0x00000000u },
    { "fighter_23ae0@seek",       m_23ae0_seek,   0x00000000u },
    { "fighter_23ae0@order",      m_23ae0_order,  0x00000000u },
    { "fighter_29c78",            b_29c78,        0x00000000u },
    { "fighter_29c78@pal",        m_29c78_pal,    0x00000000u },
    { "fighter_29c78@rec",        m_29c78_rec,    0x00000000u },
    { "fighter_4b03c",            b_4b03c,        0x00000000u },
    { "fighter_4b03c@type",       m_4b03c_type,   0x00000000u },
    { "fighter_4b03c@val",        m_4b03c_val,    0x00000000u },
    { "fighter_4b03c@sub",        m_4b03c_sub,    0x00000000u },
    { "fighter_4b03c@mode",       m_4b03c_mode,   0x00000000u },
    { "fighter_4b03c@plus",       m_4b03c_plus,   0x00000000u },
    { "fighter_4b03c@eq",         m_4b03c_eq,     0x00000000u },
    { "fighter_4b03c@dead",       m_4b03c_dead,   0x00000000u },
    { "fighter_4b03c@cam",        m_4b03c_cam,    0x00000000u },
    { "fighter_4b03c@tear",       m_4b03c_tear,   0x00000000u },
    { "fighter_4b03c@order",      m_4b03c_order,  0x00000000u },
    { "fighter_4b03c@voice",      m_4b03c_voice,  0x00000000u },
};
''')

sub('port/tests/test_fight.c', r'''    }
}
int test_p7_streams(void)       { return u6b_run(p7_check_streams); }

/* §P7.7: 0x4B03C through its registration. */
''', r'''    }
}
int test_p7_streams(void)       { return u6b_run(p7_check_streams); }

/* §P7.7: 0x4B03C through its registration. */
static void p7_check_4b03c(void)
{
    p7_anim_fn f;
    CHECK(fn_resolve(0x4B03Cu) != NULL, "0x4B03C is registered");
    f = (p7_anim_fn)(void *)fn_resolve(0x4B03Cu);
    if (f == NULL) return;
    z_fseed();
    DSD(Z_R0 + 0x14u) = Z_S0;
    DSD(Z_S0 + 8u) = Z_R1;
    DSB(Z_R1 + 0x48u) = 0x20u;      /* type 0: the delta 0x10 */
    DSB(Z_S0 + 0x20u) = 0u;
    DSB(Z_S0 + 0x21u) = 1u;
    DSB(0x0010780Au) = 0x5Au;       /* slot 0's +0x5A */
    DSB(0x0010780Au + 0x94u) = 0x5Bu;
    DSB(0x0010889Eu + 1u) = 0x9Eu;
    DSB(0x001088A4u) = 0xA4u;
    DSB(0x001088A2u + 1u) = 0xA2u;
    DSW(DS_00104B00) = 0x11u;
    DSD(Z_S1 + 0x3Cu) = 0x10000u;
    DSD(Z_S0 + 0x3Cu) = 0x2000u;
    f(Z_R0, 0u);
    CHECK_EQ_INT((int)DSB(0x0010780Au), 0x4A);              /* 0x5A - 0x10 */
    CHECK_EQ_INT((int)DSB(0x0010780Au + 0x94u), 0x5B);
    CHECK_EQ_INT((int)DSB(0x0010889Eu + 1u), 1);
    CHECK_EQ_INT((int)DSB(0x001088A4u), 0xA5);
    CHECK_EQ_INT((int)DSB(0x001088A2u + 1u), 0xA2);
    CHECK((DSW(Z_R0 + 0x28u) & 8u) != 0u, "the record is dead");
    CHECK_EQ_INT((int)DSD(Z_S1 + 0x3Cu), 0xD8F0);           /* 0x10000 - 10000 */
    CHECK_EQ_INT((int)DSD(Z_S0 + 0x3Cu), 0x4710);           /* 0x2000 + 10000 */
    CHECK_EQ_INT((int)DSD(Z_R0 + 0x14u), 0);                /* 0x49444 unlinks */

    /* type 1 (0x21): the delta 0x0E; the slot's +0x5A 0x08 < 0x0E -> 0. */
    z_fseed();
    DSD(Z_R0 + 0x14u) = Z_S0;
    DSD(Z_S0 + 8u) = Z_R1;
    DSB(Z_R1 + 0x48u) = 0x21u;
    DSB(Z_S0 + 0x20u) = 0u;
    DSB(Z_S0 + 0x21u) = 1u;
    DSB(0x0010780Au) = 0x08u;
    DSW(DS_00104B00) = 0x11u;
    f(Z_R0, 0u);
    CHECK_EQ_INT((int)DSB(0x0010780Au), 0);

    /* type 7 (0x27, above the table): the default 0x0D. */
    z_fseed();
    DSD(Z_R0 + 0x14u) = Z_S0;
    DSD(Z_S0 + 8u) = Z_R1;
    DSB(Z_R1 + 0x48u) = 0x27u;
    DSB(Z_S0 + 0x20u) = 0u;
    DSB(Z_S0 + 0x21u) = 1u;
    DSB(0x0010780Au) = 0x5Au;
    DSW(DS_00104B00) = 0x11u;
    f(Z_R0, 0u);
    CHECK_EQ_INT((int)DSB(0x0010780Au), 0x4D);              /* 0x5A - 0x0D */

    /* mode 0x22: the 0x41310 block is skipped, the counters still run. */
    z_fseed();
    DSD(Z_R0 + 0x14u) = Z_S0;
    DSD(Z_S0 + 8u) = Z_R1;
    DSB(Z_R1 + 0x48u) = 0x20u;
    DSB(Z_S0 + 0x20u) = 0u;
    DSB(Z_S0 + 0x21u) = 1u;
    DSB(0x0010780Au) = 0x5Au;
    DSB(0x001088A4u) = 0xA4u;
    DSB(0x0010889Eu + 1u) = 0x9Eu;
    DSW(DS_00104B00) = 0x22u;
    DSD(Z_S1 + 0x3Cu) = 0x10000u;
    DSD(Z_S0 + 0x3Cu) = 0x2000u;
    f(Z_R0, 0u);
    CHECK_EQ_INT((int)DSB(0x0010780Au), 0x4A);
    CHECK_EQ_INT((int)DSB(0x001088A4u), 0xA5);
    CHECK_EQ_INT((int)DSB(0x0010889Eu + 1u), 0x9E);
    CHECK_EQ_INT((int)DSD(Z_S1 + 0x3Cu), 0x10000);
    CHECK_EQ_INT((int)DSD(Z_S0 + 0x3Cu), 0x2000);

    /* equal counters: the -30000 branch and the 0x1088A2 increment. */
    z_fseed();
    DSD(Z_R0 + 0x14u) = Z_S0;
    DSD(Z_S0 + 8u) = Z_R1;
    DSB(Z_R1 + 0x48u) = 0x20u;
    DSB(Z_S0 + 0x20u) = 2u;
    DSB(Z_S0 + 0x21u) = 2u;
    DSB(0x0010780Au + 0x94 * 2u) = 0x5Au;
    DSB(0x001088A2u + 2u) = 0xA2u;
    DSW(DS_00104B00) = 0x11u;
    DSD(Z_R0 + 0x3Cu) = 0x10000u;   /* side 2 reads the slot array's first dword (Z_R0) */
    f(Z_R0, 0u);
    CHECK_EQ_INT((int)DSB(0x001088A2u + 2u), 0xA3);
    CHECK_EQ_INT((int)DSD(Z_R0 + 0x3Cu), 0x63C0);   /* 0x10000 - 10000 - 30000 */
}
int test_p7_4b03c(void)         { return u6b_run(p7_check_4b03c); }
''')

sub('tools/tests/test_diff_verify.py', r'''"fighter_23ae0@char": {"call #0", "call #1"},
    "fighter_23ae0@val": {"call #0", "call #1"},
    "fighter_23ae0@side": {"byte", "call #0", "call #0 memory", "call #1", "call #1 memory"},
    "fighter_23ae0@pal": {"call #1"},
    "fighter_23ae0@word": {"byte", "call #0 memory", "call #1", "call #1 memory"},
    "fighter_23ae0@bit": {"byte", "call #0 memory", "call #1", "call #1 memory"},
    "fighter_23ae0@seek": {"call #0", "call #1"},
    "fighter_23ae0@order": {"call #0 memory", "call #1"},
    "fighter_29c78@pal": {"call #0"},
    "fighter_29c78@rec": {"call #0"},
    ''', r'''"fighter_23ae0@char": {"call #0", "call #1"},
    "fighter_23ae0@val": {"call #0", "call #1"},
    "fighter_23ae0@side": {"byte", "call #0", "call #0 memory", "call #1", "call #1 memory"},
    "fighter_23ae0@pal": {"call #1"},
    "fighter_23ae0@word": {"byte", "call #0 memory", "call #1", "call #1 memory"},
    "fighter_23ae0@bit": {"byte", "call #0 memory", "call #1", "call #1 memory"},
    "fighter_23ae0@seek": {"call #0", "call #1"},
    "fighter_23ae0@order": {"call #0 memory", "call #1"},
    "fighter_29c78@pal": {"call #0"},
    "fighter_29c78@rec": {"call #0"},
    "fighter_4b03c@type": {"byte", "call #0 memory", "call #1 memory", "call #2 memory", "call #3 memory", "call #4 memory", "call #5 memory"},
    "fighter_4b03c@val": {"byte", "call #0 memory", "call #1 memory", "call #2 memory", "call #3 memory", "call #4 memory", "call #5 memory"},
    "fighter_4b03c@sub": {"byte", "call #0 memory", "call #1 memory", "call #2 memory", "call #3 memory", "call #4 memory", "call #5 memory"},
    "fighter_4b03c@mode": {"byte", "call #0 memory", "call #1 memory", "call #2 memory", "call #3", "call #3 memory", "call #4", "call #4 memory", "call #5", "call #5 memory"},
    "fighter_4b03c@plus": {"byte", "call #0 memory", "call #1 memory", "call #2 memory", "call #3 memory", "call #4 memory", "call #5 memory"},
    "fighter_4b03c@eq": {"byte", "call #0 memory", "call #1 memory", "call #2 memory", "call #3 memory", "call #4", "call #4 memory", "call #5 memory"},
    "fighter_4b03c@dead": {"byte", "call #0 memory", "call #1 memory", "call #2", "call #2 memory", "call #3 memory", "call #4 memory", "call #5 memory"},
    "fighter_4b03c@cam": {"byte", "call #0 memory", "call #1 memory", "call #2 memory", "call #3", "call #3 memory", "call #4 memory", "call #5 memory"},
    "fighter_4b03c@tear": {"byte", "call #0 memory", "call #1 memory", "call #2 memory", "call #3", "call #3 memory", "call #4 memory", "call #5"},
    "fighter_4b03c@order": {"byte", "call #0 memory", "call #1 memory", "call #2 memory", "call #3 memory", "call #4 memory", "call #5 memory"},
    "fighter_4b03c@voice": {"byte", "call #0", "call #0 memory", "call #1 memory", "call #2 memory", "call #3 memory", "call #4 memory", "call #5 memory"},
}


''')

print('T7 applied')
sub('tools/tests/test_diff_verify.py', 'P7_MASKS = {"fighter_2bdb8": 0, "fighter_213f0": 0, "fighter_213f4": 0, "fighter_3e424": 0, "fighter_224ec": 0, "fighter_2bde8": 0, "fighter_36114": 0, "fighter_23960": 0, "fighter_23a7c": 0, "fighter_3a9d8": 0, "fighter_48254": 0, "fighter_23ae0": 0, "fighter_29c78": 0}', 'P7_MASKS = {"fighter_2bdb8": 0, "fighter_213f0": 0, "fighter_213f4": 0, "fighter_3e424": 0, "fighter_224ec": 0, "fighter_2bde8": 0, "fighter_36114": 0, "fighter_23960": 0, "fighter_23a7c": 0, "fighter_3a9d8": 0, "fighter_48254": 0, "fighter_23ae0": 0, "fighter_29c78": 0, "fighter_4b03c": 0}')

sub('tools/tests/test_diff_verify.py', '        self.assertIn("diff-verify: 162/162 functions VERIFIED; 448/448 mutants detected; 1 named gaps; "\n                      "45/136 rows with callees closed (26 have none).", out.getvalue())', '        self.assertIn("diff-verify: 163/163 functions VERIFIED; 459/459 mutants detected; 1 named gaps; "\n                      "45/137 rows with callees closed (26 have none).", out.getvalue())')

sub('tools/tests/test_diff_verify.py', '                                 0x39280: (), 0x13244: (), 0x2A148: ("edx",), 0x2BCF4: ("edx",),', '                                 0x2B150: ("esi", "edi", "ebp"),\n                                 0x41310: (), 0x49444: ("edi", "ebp"),\n                                 0x39280: (), 0x13244: (), 0x2A148: ("edx",), 0x2BCF4: ("edx",),')

PY
```

Then build and run the task's tests:

```bash
cmake --build build 2>&1 | tail -1
python3 tools/diff_verify.py --self-check --image /tmp/pr_p7_dfv.bin --table /tmp/pr_p7_diff.md | tail -1
PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | grep -E 'FAIL|FAILURES|all checks passed'
```

Expected: the counter line `163/163 functions VERIFIED; 459/459 mutants detected; 1 named gaps; 45/137 rows with callees closed (26 have none).`; every new row VERIFIED with its blocks hit; the new unit check passes. The rows: `fighter_4b03c` 13 cases 22/22 (`2B150` stub unverified, `2C3FC` stub unverified, `41310` stub unverified, `49444` stub unverified); 11 mutants detected.

- [ ] **Step 2: the E2 table.** `make entry-triage` must pass against the regenerated table; then

```bash
./build/diffrun --exe data/game/C/PRAGE.EXE --image-out /tmp/pr_p7_e2img.bin && \
python3 tools/entry_triage.py --image /tmp/pr_p7_e2img.bin \
  --live docs/superpowers/plans/2026-10-01-reverse-e2-live-functions.txt \
  --out docs/superpowers/plans/2026-10-01-reverse-e2-triage.md
```

Expected: `entry-triage: unchanged` and `voice sites outside Ghidra 134: 0 in unported code, 115 in ported code, 19 nowhere`.

- [ ] **Step 3: every new unit assertion can fail.** The `P7_KINDS` entry of every new mutant names the case set that alone catches it (record §P7.10); `test_each_p7_mutant_is_caught_by_what_it_breaks` asserts the sets, and the self-check reports each mutant MISMATCH. For each unit check, mutate the port function it tests (the same one-line change as the matching `m_*` mutant), run `PR_ORACLE_REQUIRED=1 ./build/run_tests`, record the first FAIL line, and restore the code.

- [ ] **Step 4: commit.** Stage the named files (`git add` each; never `git add -A`) and commit:

```bash
git commit -m "fighter: port 0x4B03C; seams 0x2B150 0x41310 0x49444 (track P batch 7, task 7)"
```

---

### Task 8: closure: the full gate, the docs, the record

**Files:** `docs/PROGRESS.md`, the record's §P7.10, `AGENTS.md`/`Makefile` (only if a measured value moved). **Interfaces:** consumes Tasks 2-7.

- [ ] **Step 1: the full gate** (Task 1 Step 1's commands with the log `/tmp/pr_p7_final.log`) and the WAV (Task 1 Step 2). Expected: `EXIT=0`; the 45 oracle lines equal to `.superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt`; the WAV cmp-equal to `before-t2.wav`; `symbols.h` regenerated byte-identical; `PR_ORACLE_REQUIRED=1 ./build/run_tests` `all checks passed`; `163/163 functions VERIFIED; 459/459 mutants detected; 1 named gaps; 45/137 rows with callees closed (26 have none)`; E2 `targets 234 unported, 261 ported; supplement 131 (3 unported, 0 stale); untrusted entries 30`; `771 1203 64` / `731 731 100`; every gp ratchet at its pin (U9 346/3503/8/3503, U10 331/9954/30/9954, the other twelve as at `e88eb44`). The planner measured the full gate on the prototype (record §P7.10); quote the actual output and the time it took.
- [ ] **Step 2: the docs.** Append the P7 paragraph to `docs/PROGRESS.md`: the 14 functions, the seams, the rows and mutants, the E2 and gp drops, the named gaps (record §P7.9), the counters, and the `771 1203 64` / `731 731 100` lines.
- [ ] **Step 3: the record's §P7.10.** Append the implementation's closure paragraph: the commit range, each task's counter and E2 lines as measured, the final gate's lines from Step 1, and any deviation from this plan's expected output (a deviation is a finding: record the raw fact and the address).
- [ ] **Step 4: commit.** `git add` the named files and `git commit -m "docs: P7 closure (track P batch 7): 14 functions, seams, gp re-measures, counters"`.
