# P4+P5: the animation targets A and B (track P, batches 4+5) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Port the 33 functions of track P's combined batches 4 and 5 (record §P4.1/§P5.1: the roadmap row P4's 20 animation targets A `0x18BC8 0x21084 0x400E0 0x1549C 0x154E8 0x15510 0x229E8 0x241A8 0x243F8 0x3427C 0x34308 0x3438C 0x34418 0x344A4 0x34530 0x345BC 0x400EC 0x40358 0x45C54 0x489DC` and row P5's 13 animation targets B `0x156E0 0x21044 0x22AB8 0x37B70 0x3D328 0x3DA50 0x3DB8C 0x3DC3C 0x403A0 0x40FBC 0x48A20 0x24508 0x24454`, P2's three moved out), each verified against the original's bytes with its callees stubbed (E3 §E3.10); drop `gp-u9-win`'s three rows and `gp-u10-ending`'s one row and show every re-measured pin exact; regenerate the E2 table with each port.

**Architecture:** One C function per original function, appended to `port/src/game/fighter.c` after P3's `fighter_4844c`; 32 of them registered in `actors_init` (`port/src/game/actors.c`) through the `anim_code_*` wrappers the module already uses (the animation dispatcher `0x2B2A0` calls DS_00105BD4 with EAX = rec; none of the 33 reads the operand or ECX, so the wrappers drop them), and `0x24454` registered as a case-10 adapter (it is the +0x10 handler `0x24508` stores; `0x3531C` case 10 calls it with (slot, side) and the adapter supplies the slot's record). Each function has a binding and mutants in `port/tests/diff_runner.c`, a `Spec` in `tools/diff_verify.py` (`P45_SPECS`), seeded unit checks in `port/tests/test_fight.c`, and its row in the self-check counter. The two new stubbed callees `0x29C08` (called by `0x15510`) and `0x2AD40` (called by `0x24454`) open with `PR_SEAM`/`PR_SEAM_RET` (`0x29C08`'s port `fighter_29c08` becomes non-static; `0x2AD40`'s is `release_record` in `actors.c`).

**Tech Stack:** C11 (`port/src`, `port/tests`), Python 3.12 (`unittest`), `unicorn` 2.1.4 and `capstone` 5.0.7 (`tools/requirements-diff.txt`), CMake, `make`.

**Spec:** `docs/superpowers/specs/2026-09-30-reverse-completion-design.md` §3 decision 1 ("port all of O3-O6, whether or not a capture reaches them"), §4 track P ("port batches, each function verified by E"), §5.1-§5.3, §6 ("P: each ported function has a verification row; unhit blocks and non-emulable instructions named; counters and README percentage updated"), §7.

**Derivation record:** `docs/superpowers/plans/2026-10-03-reverse-p4-p5-derivations.md` (§P4.1/§P5.1 the 33 members from the raw, §P4.2/§P5.2 callers, masks and the callee declarations, §P4.3-§P5.6 each function from the bytes, §P4.7/§P5.7 the gp interactions, §P4.8 decisions and named gaps, §P4.9 results). Recipe: `2026-10-01-reverse-e3-derivations.md` §E3.10; lessons: P1 §P1.10-§P1.12, P2's and P3's reviews (`.superpowers/sdd/2026-10-03-reverse-p3-move-callbacks-b/progress.md`).

**What the planner ran (scratch, 2026-10-03, on this worktree's base `f5b5556`; image sha1 `ff3b8cb14e00f1c282de7b7e15dcd7c230766947`; scratch paths written as `/tmp/p45_*`, test times as `N.NNN`):** a prototype (all tasks applied in order in this worktree; the full `make verify` with the parallel-safe overrides ran on its final state before the unit-test split, quoted in Tasks 1 and 8) and, per task, the measured rows, mutants, counters, E2 lines, gp pins and mutation proofs below. Every code block below is a file the prototype ran (the unit tests were later split into the per-task functions the tasks register, assertion-identical: `PR_ORACLE_REQUIRED=1 ./build/run_tests` prints `all checks passed` on the split tree too). A store sweep finds every store of the 33 rows observable in the rows' cases; every gp scenario's miss set was read before the batch (record §P4.7/§P5.7).

**Re-baseline note.** P1-P3 are on `main` `f5b5556`; C1 executes in parallel. The counters, E2 lines and gp pins below are the measured `f5b5556` values plus this plan's increments. If the merge of C1, or anything merged after it, moves a value, Task 1 records the measured one in the ledger and every later expected counter adds this plan's increments to it: diff-verify rows +8, +6, +7, +3, +7, +2 (Tasks 2-7); mutants +13, +11, +15, +14, +30, +11; closed rows +1, 0, 0, +1, 0, 0; rows without callees 0, 0, 0, 0, 0, 0; E2 ported targets +8, +5, +7, +3, +7, +1 (the last task's `0x24454` is a supplement row, so its supplement unported count drops by 1). Both batches append to `tools/diff_verify.py` (a `P45_SPECS` list next to `C1_SPECS`) and to the same binding tables; the rebase conflict is mechanical: keep both sides, regenerate the E2 table, recompute the counters.

## Decisions needed from the user

**None.** The user's decisions D2 (span code: a named gap backed by the pixel oracles; not touched here) and D3 (callee rows in C1) stand. Three choices this plan makes are derived from the raw and recorded (record §P4.1, §P4.2, §P5.1), not left open:

1. **The member list is the roadmap's 33.** Every code immediate a member stores was scanned; nothing else is reached. `0x24454` is not a stream target: it is the immediate at `0x24529` inside `0x24508` (a `code-immediate` E2 row), the +0x10 handler, and is registered as its case-10 adapter.
2. **The two new callees are seamed, not ported as rows.** `0x29C08` (already ported as `fighter_29c08`) and `0x2AD40` (already ported as `release_record`) gain `PR_SEAM`/`PR_SEAM_RET`; their own rows are C1's.
3. **The U9/U10 re-measure.** `gp-u9-win`'s `0x400E0`/`0x21084`/`0x21044` rows and `gp-u10-ending`'s `0x3DA50` row are dropped in the tasks that port them; the U9 trace pin rises 2150 -> 2364 and the set gains the three targets the replay then reaches (record §P4.7); the U10 pins are unchanged (record §P5.7). No other gp set holds a P4/P5 member.

## The P-track roadmap

From record §P1.3 as corrected by P2 §P2.11 and §P3.11. Each line is one plan and one subagent-driven run.

| batch | ports | what |
|---|---|---|
| P1 (merged) | 16 + 1 callee row | the finisher entries, their +0x0C callbacks, `0x38034`, the finisher streams' targets |
| P2 (merged) | 24 | the 13 move callbacks `0x14EF8..0x3DCEC`, the 7 callbacks they store, `0x22404`, `0x14FA8 0x14FF8 0x150AC` |
| P3 (merged) | 24 | character 2's 9 move callbacks `0x475EC..0x489A0`, the 13 callbacks they store, `0x48170`, the +0x10 handler `0x4811C` |
| C1 (parallel) | about 45 rows | verification only: the ported callees P1-P3 stub |
| **P4+P5** (this plan) | **33** | animation targets A and B; the callees `0x29C08` and `0x2AD40` gain their seams |
| P6 | 18 | animation targets C (with `0x47E04 0x47E30 0x482E4 0x48374`, P3's streams' targets; after this batch also `0x2BDA0`) |
| C2 | about 20 rows | verification only: the rest of the stubbed callees |
| P7 | 14 | the unported direct callees with their callers, the targets outside E2 (with `0x48254`; after this batch also `0x213F0 0x213F4`) |
| P8 | 16 + triage | the rest |
| span | 0 | decision D2 |

None of the 33 is a Ghidra `FN_` function: `port_progress.py` stays `771 1203 64` / `731 731 100` and README does not move.

## Global Constraints

Copied verbatim from their sources:

- AGENTS.md: "**Never ship a fitted constant.** Every value is derived from the raw bytes or a capture, with the address that proves it. A value that cannot be pinned is a **named gap with its evidence** — never a plausible-looking number." / "**On any plan-vs-raw conflict the raw wins.** Record the correction and the address, in the derivation record and the report."
- Spec §8: "`make verify` is the gate after every task." User ruling (2026-10-01, speed-up): the per-task gate is the task's tests, `make diff-verify`, `make entry-triage` and the gp oracles whose miss sets the task touches; the full `make verify` runs at the baseline (Task 1), the final task (Task 8) and before the merge, with the parallel-safe overrides `T=p45; SMK_DUMP=/tmp/pr_${T}_smk TITLE_DUMP=/tmp/pr_${T}_title ATTRACT_DUMP=/tmp/pr_${T}_att FRONTEND_DUMP=/tmp/pr_${T}_fe TITLE_PIN_DIR=/tmp/pr_${T}_pin AUDIO_WAV=/tmp/pr_${T}.wav K11_DUMP=/tmp/pr_${T}_k11 GP_DUMP=/tmp/pr_${T}_gp DIFF_IMAGE=/tmp/pr_${T}_diffimg DIFF_TABLE=/tmp/pr_${T}_diff.md E2_IMAGE=/tmp/pr_${T}_e2.bin`. The 45 oracle lines (`grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|== demo-fight)' <log>`) must equal `.superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt`; the `make audio-render` WAV must be byte-identical to `.superpowers/sdd/2026-09-29-k7-k12/scratch/before-t2.wav`; `python3 tools/port_progress.py` prints `771 1203 64` and `731 731 100`. A full `make verify` took 25 min here with two others sharing the host.
- AGENTS.md: "**The E2 target list is a gate** ... A commit that ports a target (a new `/* 0xADDR` header or `fn_register`) regenerates the table in the same commit (decision D3) ... A conflict in that file is resolved by regenerating it after the rebase, never by merging lines by hand."
- AGENTS.md: "One C function per original function, header comment `/* 0xADDR — spec section */`." / "Mark deliberate deviations `/* PORT: ... */`; doubts `/* TODO(verify): ... */`. No other comment styles in `port/src`." / "Code addresses stored in data go through `fn_origin()` / `fn_resolve()`." / "**`port/src/symbols.h` is generated** ... Never hand-edit it; where the generator emits no name, use a local `#define` with the raw address."
- AGENTS.md: "A ported function that a differentially verified function stubs or records ... opens with `PR_SEAM(0xADDR, args...)` (void) or `PR_SEAM_RET(0xADDR, args...)` ... The arguments are the C signature's, in order (a pointer into `mem[]` as its offset)." Record E3 §E3.8: "A seam must be the callee's first statement".
- AGENTS.md: "Only `CHECK(cond,msg)` and `CHECK_EQ_INT(a,b)`." / "**Register a test once** — add `X(test_foo)` to `TEST_CASES` in `port/tests/test.h`" / "**Assertions must be able to fail.** Seed sentinels that differ from the post-conditions, prove a new assertion fails under a mutation of the code it tests, and never assert an unseeded BSS-zero". "Consolidating must not change an assertion": this plan **extends** the exact-set assertions of `RealFunctionTests` (rows, masks, mutant names, stub clobbers, the counter line) and changes no other assertion.
- AGENTS.md: "Every env-gated driver and `--check` run arms `fn_resolve`'s miss log and must record exactly the pinned known-set": Tasks 2 and 6 drop the gp rows they port and re-pin their scenarios (record §P4.7/§P5.7).
- AGENTS.md: "`data/` is git-ignored and read-only — never write to it." / "Commit style: `<area>: <what changed>`. **Never `git add -A`** — stage named files." Never run `make gp-capture`.
- Spec §5.3 / E3 §E3.4: "VERIFIED" is equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.

## Review Focus

1. **An input the rows cannot tell apart.** Every row whose original reads a lookalike has a case where they differ and a mutant that only that case catches: the index byte 0x80 (`fighter_1549c@side` s2, `fighter_154e8@side` s3, `fighter_241a8@sext` n3, `fighter_24454@side` t8, `fighter_156e0@side`/`@signed` s0/s1, `fighter_22ab8@sext` h3, `fighter_37b70@sext` r6); the own slot against the other (`fighter_243f8@slot`, `fighter_24508@side`); the sign of a 16-bit word (`fighter_21044@neg`, `fighter_403a0@neg`/`@signed`, `fighter_3d328@a2`, `fighter_3db8c@a2`, `fighter_37b70@neg`); the 0x7FFFFFFF mask of the +0x24 dword (`fighter_24454@guard` t1).
2. **A width or sign read wrong.** Cases each alone catching a mutant: the tables (`fighter_22ab8@table`), the descriptor offsets (`fighter_37b70@mutant`, `fighter_403a0@mutant`), the child's +0x34 words (`fighter_3db8c@w34`, `fighter_3dc3c@w34`), the a4/a5 arguments (`fighter_3d328@a4`, `fighter_403a0@a5`, `fighter_15510@a5`).
3. **A store no case can observe.** Every field a row writes carries a sentinel that differs from what it writes; the planner's store sweep found all 33 rows' stores observable. The `@early`/`@order` mutants (`fighter_400ec@side`'s call-memory, `fighter_154e8@side`, `fighter_24508@side`, `fighter_15510@pal`) prove the memory at a call is compared.
4. **The registrations and the adapter.** Each task's unit check asserts `fn_resolve(addr) != NULL` (or `== fighter_24454_case10`), and its mutation proof deletes a registration or mutates a store; `0x24454`'s adapter check runs the handler through `fn_resolve` and observes a field only the slot's record supplies.
5. **The gp re-measures.** Task 2 drops the three U9 rows and shows the re-measured pins exact (each + 1 fails; the trace pin rises to 2364 and the set gains `0x213F0`/`0x213F4`/`0x2BDA0` with their first frames); Task 6 drops the U10 row and shows every U10 pin exact and unchanged; Task 8's full gate shows every other gp ratchet and miss set unchanged.

## Where to run

The worktree `.worktrees/reverse-p45` (branch `reverse-p45`). Every command runs from the worktree root. Ledger: `.superpowers/sdd/2026-10-03-reverse-p4-p5/progress.md`.

**How the code steps are written.** Each change is a `python3 - <<'PY'` script that replaces exact text and asserts the text occurs exactly once (`sub`), so a script either applies cleanly or stops naming the file and the text it could not find. Run each once, from the worktree root, in order. If `main` moved after `f5b5556`, an anchor can move: re-apply that `sub` by hand at the same place, never elsewhere; the unit tests' line numbers in the expected output move with `test_fight.c`, and the E2 table is regenerated, never merged.

## File Structure

| File | Responsibility |
|---|---|
| `port/src/game/fighter.c`, `fighter.h` | the 33 functions (appended after `fighter_4844c`) and `fighter_24454_case10`, their prototypes; `fighter_29c08`'s seam and its non-static export |
| `port/src/game/actors.c` | the 32 `anim_code_*` wrappers, their declarations, the registrations in `actors_init` (after P3's `0x4844C`), `release_record`'s `PR_SEAM(0x2AD40u, ...)` |
| `port/tests/diff_runner.c` | the bindings and mutants (`b_*`/`m_*`, `k_bindings`) |
| `tools/diff_verify.py` | `P45_SPECS`, `CALL29C08`, `RELEASE`, the P45 constants |
| `tools/tests/test_diff_verify.py` | `P45_MASKS`, `P45_KINDS`, `P45_OUTSIDE`; the exact-set assertions extended; `test_each_p45_mutant_is_caught_by_what_it_breaks`; the stub table; the counter line |
| `port/tests/test_fight.c`, `port/tests/test.h` | `test_p45_leaves`, `test_p45_21044`, `test_p45_p4_spawns`, `test_p45_side_anim`, `test_p45_p5_records`, `test_p45_p5_spawns`, `test_p45_handler` |
| `port/tests/test_platform.c`, `Makefile`, `AGENTS.md` | the U9/U10 miss sets lose the ported rows; the `GP_WIN_TRACE_MIN_FIRST` pin and the provenance (Task 2) and the U10 provenance (Task 6) |
| `docs/superpowers/plans/2026-10-01-reverse-e2-triage.md` | regenerated in Tasks 2-7 |
| `docs/PROGRESS.md`, the record | Task 8 |

---

### Task 1: Baseline (no commit)

**Files:** none. **Interfaces:** consumes `main` at the worktree's head; produces the baseline log `/tmp/pr_p45_base.log`.

- [ ] **Step 1: the full gate on the untouched tree.**

```bash
T=p45; make verify SMK_DUMP=/tmp/pr_${T}_smk TITLE_DUMP=/tmp/pr_${T}_title ATTRACT_DUMP=/tmp/pr_${T}_att \
  FRONTEND_DUMP=/tmp/pr_${T}_fe TITLE_PIN_DIR=/tmp/pr_${T}_pin AUDIO_WAV=/tmp/pr_${T}.wav K11_DUMP=/tmp/pr_${T}_k11 \
  GP_DUMP=/tmp/pr_${T}_gp DIFF_IMAGE=/tmp/pr_${T}_diffimg DIFF_TABLE=/tmp/pr_${T}_diff.md E2_IMAGE=/tmp/pr_${T}_e2.bin \
  > /tmp/pr_p45_base.log 2>&1; echo "EXIT=$?"
grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|== demo-fight)' /tmp/pr_p45_base.log \
  | diff - .superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt && echo ORACLES-EQUAL
grep -E '^diff-verify:|^entry-triage: (targets|voice)|ratchet N' /tmp/pr_p45_base.log | sed 's/^gp_compare: //'
python3 tools/port_progress.py
```

Expected (the `f5b5556` state): `EXIT=0`, `ORACLES-EQUAL`, and

```
gp-u9-win: frames: first unexplained 346, ratchet N 346 ok
gp-u9-win: trace: first differing 2150, ratchet N 2150 ok
gp-u9-win: path: 0 not reproduced through 7; ratchet N 8 ok
gp-u9-win: win: first differing 3162, ratchet N 3162 ok
gp-u10-ending: frames: first unexplained 331, ratchet N 331 ok
gp-u10-ending: trace: 0 differing through 9953; ratchet N 9954 ok
gp-u10-ending: path: 0 not reproduced through 29; ratchet N 30 ok
gp-u10-ending: win: 0 differing through 9953; ratchet N 9954 ok
diff-verify: 78/78 functions VERIFIED; 158/158 mutants detected; 1 named gaps; 11/64 rows with callees closed (14 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
entry-triage: targets 288 unported, 207 ported; supplement 131 (22 unported, 0 stale); untrusted entries 30
entry-triage: voice sites outside Ghidra 134: 31 in unported code, 84 in ported code, 19 nowhere
771 1203 64
731 731 100 (portable: excludes 81 host-owned/deferred and runtime >= 5D000)
```

(The other gp scenarios' ratchet lines are the Makefile's pins; the full log holds them.) If a value differs (the re-baseline note), record the measured lines in the ledger; every later "expected" counter then adds this plan's increments to them.

- [ ] **Step 2: the WAV.** `make audio-render AUDIO_WAV=/tmp/pr_p45.wav >/dev/null 2>&1; cmp /tmp/pr_p45.wav .superpowers/sdd/2026-09-29-k7-k12/scratch/before-t2.wav && echo WAV-SAME` prints `WAV-SAME`.

---

### Task 2: the U9 members and the simple targets `0x18BC8 0x21084 0x400E0 0x21044 0x1549C 0x154E8 0x229E8 0x243F8`, and gp-u9-win

**Files:** modify `tools/diff_verify.py`, `tools/tests/test_diff_verify.py`, `port/tests/test_fight.c`, `port/tests/test.h`, `port/src/game/fighter.c`, `port/src/game/fighter.h`, `port/src/game/actors.c`, `port/tests/diff_runner.c`, the E2 table; Step 7: `port/tests/test_platform.c`, `Makefile`, `AGENTS.md`.

**Interfaces:** produces `void fighter_18bc8(u32 rec)` and the same shape for `fighter_21084`, `fighter_400e0`, `fighter_21044`, `fighter_1549c`, `fighter_154e8`, `fighter_229e8`, `fighter_243f8` (the dispatcher calls them with EAX = rec; mask 0); the `anim_code_*` wrappers and registrations; `P45_SPECS` with its first eight specs and the `P45_MASKS`/`P45_KINDS` tables. Consumes P1's `BIT15`/`ANIM_BEGIN`, E3's `VOICE`/`PALETTE`, P2's `z_fseed`/`u6b_run`.

- [ ] **Step 1: the specs and the expectations first.**

```bash
python3 - <<'PY'
import pathlib


def sub(path, old, new):
    p = pathlib.Path(path)
    s = p.read_text()
    n = s.count(old)
    assert n == 1, "%s: %d occurrences of %r" % (path, n, old[:70])
    p.write_text(s.replace(old, new))


# tools/diff_verify.py: the batch's constants and first specs, before SPECS
P45 = r'''
# ---- track P batches 4 and 5: the animation targets A and B (record 2026-10-03-reverse-p4-p5) ------
# A stream target runs as 0x2B2A0's opcodes 0x10 (0xD000), 0x11 (0xD100) and 0x15 (0xD500) call it
# through DS_00105BD4 with EAX = rec; none of the 33 reads the operand or ECX, and the dispatcher
# overwrites EAX after (`xor ecx,ecx; mov eax,ecx`): mask 0. Every field a function writes carries a
# sentinel that differs from what it writes; the neighbour bytes are seeded too. P45_SLOT3 is a fake
# slot pointer for the 0x80/0x81 index cases (a port that masks the index with &1 reads the other
# slot); P45_PSET is a pset base for DS_001014EC.
P45_SLOT3 = 0x10A700
P45_PSET = 0x10A800
P45_CHARS = {DS_SLOTS + 0x7A: b"\x05", DS_SLOTS + 0x94 + 0x7A: b"\x03"}
# The DS_001077A8 index table (two slot pointers, the raw reads it zero-extended) and a fake third
# entry at index 0x81 (0x1077A8 + 0x204): a port that masks the index with &1 reads entry 1.
P45_IDX = {0x1077A8: le32(DS_SLOTS) + le32(DS_SLOTS + 0x94)}
P45_PTRS = {**SLOT_PTRS, **P45_IDX}
P45_FAKE = {0x1079AC: le32(P45_SLOT3), P45_SLOT3: le32(E3_REC)}
CALL29C08 = E.Call(0x29C08, ("eax", "edx"), clobbers=("edx",))       # returns the palette handle
RELEASE = E.Call(0x2AD40, ("eax", "edx"), clobbers=("edx", "edi", "ebp"))   # void (the seam on release_record)


# 0x18BC8: the byte 0x100C1D = 0.
P45_SPECS = [
    Spec("fighter_18bc8", 0x18BC8, [
        Case("c0", {"eax": E3_REC, "edx": 0x1234, "ecx": 0}, {0x100C1D: b"\x5a"}),
    ], eax_mask=0, mutants=("@mutant",)),
    # 0x21084: n0 no held record; n1 held = E3_SLOT (its low byte 0, so a byte test of +0x14 skips).
    Spec("fighter_21084", 0x21084, [
        Case("n0", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(0), E3_REC + 0x1C: le32(0x1C1C1C1C), E3_REC + 0x34: b"\x34\x34\x36\x36"}),
        Case("n1", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(E3_SLOT), E3_REC + 0x1C: le32(0x1C1C1C1C),
              E3_REC + 0x34: b"\x34\x34\x36\x36", E3_SLOT + 0x54: b"\x54\x55", E3_SLOT + 0x57: b"\x57"}),
    ], eax_mask=0, mutants=("@mutant", "@byte14")),
    # 0x400E0: e1 clears bit 2 of 0xFF; e2 has only bit 2 set.
    Spec("fighter_400e0", 0x400E0, [
        Case("e0", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(0), E3_SLOT + 0x42: b"\x42\x43"}),
        Case("e1", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(E3_SLOT), E3_SLOT + 0x42: b"\xff\x43"}),
        Case("e2", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(E3_SLOT), E3_SLOT + 0x42: b"\x04\x43"}),
    ], eax_mask=0, mutants=("@mutant",)),
Spec("fighter_18bc8", 0x18BC8, [
        Case("c0", {"eax": E3_REC, "edx": 0x1234, "ecx": 0}, {0x100C1D: b"\x5a"}),
    ], eax_mask=0, mutants=("@mutant",)),

Spec("fighter_21084", 0x21084, [
        Case("n0", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(0), E3_REC + 0x1C: le32(0x1C1C1C1C), E3_REC + 0x34: b"\x34\x34\x36\x36"}),
        Case("n1", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(E3_SLOT), E3_REC + 0x1C: le32(0x1C1C1C1C),
              E3_REC + 0x34: b"\x34\x34\x36\x36", E3_SLOT + 0x54: b"\x54\x55", E3_SLOT + 0x57: b"\x57"}),
    ], eax_mask=0, mutants=("@mutant", "@byte14")),

Spec("fighter_400e0", 0x400E0, [
        Case("e0", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(0), E3_SLOT + 0x42: b"\x42\x43"}),
        Case("e1", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(E3_SLOT), E3_SLOT + 0x42: b"\xff\x43"}),
        Case("e2", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(E3_SLOT), E3_SLOT + 0x42: b"\x04\x43"}),
    ], eax_mask=0, mutants=("@mutant",)),

Spec("fighter_21044", 0x21044, [
        Case("h0", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(0), E3_REC + 0x34: b"\x34\x34\x36\x36", E3_REC + 0x42: b"\x42\x43\x44\x45"}),
        Case("h1", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(E3_SLOT), E3_REC + 0x34: b"\x34\x34\x36\x36",
              E3_REC + 0x42: b"\x42\x43\x44\x45", E3_SLOT + 0x57: b"\x57"}, {0x1A570: 0}),
        Case("h2", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(E3_SLOT), E3_REC + 0x34: b"\x34\x34\x36\x36",
              E3_REC + 0x42: b"\x42\x43\x44\x45", E3_SLOT + 0x57: b"\x57"}, {0x1A570: 1}),
    ], calls=(BIT15,), eax_mask=0, mutants=("@mutant", "@neg")),

Spec("fighter_1549c", 0x1549C, [
        Case("s0", {"eax": E3_OUT, "edx": 0x1234, "ecx": 0},
             {**P45_PTRS, **P45_CHARS, E3_OUT + 0x51: b"\x00", 0x9B01C + 12: le32(0xBBBBBBBB),
              0x9B01C + 20: le32(0xAAAAAAAA)}),
        Case("s1", {"eax": E3_OUT, "edx": 0x1234, "ecx": 0},
             {**P45_PTRS, **P45_CHARS, E3_OUT + 0x51: b"\x01", 0x9B01C + 12: le32(0xBBBBBBBB),
              0x9B01C + 20: le32(0xAAAAAAAA)}),
        Case("s2", {"eax": E3_OUT, "edx": 0x1234, "ecx": 0},
             {**P45_PTRS, **P45_CHARS, **P45_FAKE, E3_OUT + 0x51: b"\x80",
              0x9B01C + 0x204: le32(0xCCCCCCCC)}),
    ], calls=(ANIM_BEGIN, PALETTE, VOICE), eax_mask=0, mutants=("@mutant", "@side")),

Spec("fighter_154e8", 0x154E8, [
        Case("s0", {"eax": E3_OUT, "edx": 0x1234, "ecx": 0},
             {**P45_PTRS, 0x1077AC: le32(0), E3_OUT + 0x51: b"\x00"}),
        Case("s1", {"eax": E3_OUT, "edx": 0x1234, "ecx": 0},
             {**P45_PTRS, E3_OUT + 0x51: b"\x00", E3_REC2 + 0x24: le32(0x24242424)}),
        Case("s2", {"eax": E3_OUT, "edx": 0x1234, "ecx": 0},
             {**P45_PTRS, E3_OUT + 0x51: b"\x01", E3_REC + 0x24: le32(0x24242424)}),
        Case("s3", {"eax": E3_OUT, "edx": 0x1234, "ecx": 0},
             {**P45_PTRS, 0x1077B4: le32(P45_SLOT3), P45_SLOT3: le32(E3_REC),
              E3_OUT + 0x51: b"\x02", E3_REC + 0x24: le32(0x24242424)}),
    ], calls=(VOICE,), eax_mask=0, mutants=("@mutant", "@side")),

Spec("fighter_229e8", 0x229E8, [
        Case("s0", {"eax": E3_REC, "edx": 0x1234, "ecx": 0}),
        Case("s1", {"eax": E3_REC2, "edx": 0x1234, "ecx": 0}),
    ], calls=(ANIM_BEGIN,), eax_mask=0, mutants=("@mutant",)),

Spec("fighter_243f8", 0x243F8, [
        Case("z%d" % side, {"eax": E3_OUT, "edx": 0x1234, "ecx": 0},
             {**P45_PTRS, **P45_CHARS, E3_OUT + 0x51: bytes([side]),
              DS_SLOTS + 0x94 * (1 - side) + 0x52: b"\x52\x53\x54\x55\x56\x57",
              DS_SLOTS + 0x94 * (1 - side) + 0x10: le32(0x10101010),
              DS_SLOTS + 0x94 * side + 0x52: b"\x92\x93\x94\x95\x96\x97",
              DS_SLOTS + 0x94 * side + 0x10: le32(0x20202020),
              0xC90F8 + 20: le32(0xAAAAAAAA), 0xC90F8 + 12: le32(0xBBBBBBBB)})
        for side in (0, 1)
    ], allow_calls=(0x33950,), calls=(ANIM_BEGIN,), eax_mask=0, mutants=("@mutant", "@slot")),

]
'''
sub("tools/diff_verify.py", "SPECS = [\n    Spec(\"rng_next\", 0x5D7DC, [\n", P45 + "\n\nSPECS = [\n    Spec(\"rng_next\", 0x5D7DC, [\n")
sub("tools/diff_verify.py", "] + E3_SPECS + P1_SPECS + P1_ANIM_SPECS + P2_SPECS + P3_SPECS\n",
    "] + E3_SPECS + P1_SPECS + P1_ANIM_SPECS + P2_SPECS + P3_SPECS + P45_SPECS\n")
print("t2_spec applied")
PY
python3 tools/diff_verify.py --image /tmp/pr_p45_img.bin --function fighter_18bc8 | grep -E '^\| fighter|unknown binding|^diff-verify'
```

Expected (the binding does not exist yet; `build/diffrun --exe data/game/C/PRAGE.EXE --image-out /tmp/pr_p45_img.bin` first, sha1 `ff3b8cb14e00f1c282de7b7e15dcd7c230766947`):

```
t2_spec applied
| fighter_18bc8 | 0x18BC8 | 1 | 1/1 | MISMATCH | - |
  fighter_18bc8: c0: port: unknown binding fighter_18bc8
diff-verify: 0/1 functions VERIFIED; 0 named gaps; 0/1 rows with callees closed (1 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
```

- [ ] **Step 2: the unit checks (fail to build).**

```bash
python3 - <<'PY'
import pathlib


def sub(path, old, new):
    p = pathlib.Path(path)
    s = p.read_text()
    n = s.count(old)
    assert n == 1, "%s: %d occurrences of %r" % (path, n, old[:70])
    p.write_text(s.replace(old, new))


TEST = r'''
/* §P4.3/§P4.4: the leaves 0x18BC8, 0x21084, 0x400E0 and the other-slot 0x154E8. */
static void p45_check_leaves(void)
{
    p45_anim_fn f;
    f = (p45_anim_fn)(void *)fn_resolve(0x18BC8u);
    CHECK(f != NULL, "0x18BC8 is registered");
    if (f != NULL) {
        z_fseed();
        DSB(0x00100C1Du) = 0x5Au;
        f(Z_R0, 0u);
        CHECK_EQ_INT((int)DSB(0x00100C1Du), 0);
    }

    f = (p45_anim_fn)(void *)fn_resolve(0x21084u);
    CHECK(f != NULL, "0x21084 is registered");
    if (f != NULL) {
        z_fseed();
        DSD(Z_R0 + 0x14u) = Z_S0;
        DSD(Z_R0 + 0x1Cu) = 0x1C1C1C1Cu;
        DSW(Z_R0 + 0x34u) = 0x3434u;
        DSB(Z_S0 + 0x54u) = 0x54u;
        DSB(Z_S0 + 0x57u) = 0x57u;
        f(Z_R0, 0u);
        CHECK_EQ_INT((int)DSD(Z_R0 + 0x1Cu), 0);
        CHECK_EQ_INT((int)DSW(Z_R0 + 0x34u), 0);
        CHECK_EQ_INT((int)DSB(Z_S0 + 0x54u), 0);
        CHECK_EQ_INT((int)DSB(Z_S0 + 0x57u), 2);
        z_fseed();
        DSD(Z_R0 + 0x14u) = 0u;
        DSD(Z_R0 + 0x1Cu) = 0x1C1C1C1Cu;
        f(Z_R0, 0u);
        CHECK_EQ_INT((int)DSD(Z_R0 + 0x1Cu), 0x1C1C1C1C);
    }

    f = (p45_anim_fn)(void *)fn_resolve(0x400E0u);
    CHECK(f != NULL, "0x400E0 is registered");
    if (f != NULL) {
        z_fseed();
        DSD(Z_R0 + 0x14u) = Z_S0;
        DSB(Z_S0 + 0x42u) = 0xFFu;
        DSB(Z_S0 + 0x43u) = 0x43u;
        f(Z_R0, 0u);
        CHECK_EQ_INT((int)DSB(Z_S0 + 0x42u), 0xFB);
        CHECK_EQ_INT((int)DSB(Z_S0 + 0x43u), 0x43);
    }

    /* 0x154E8: the other side's record +0x24 = 6.0f; the 0x80 index reads the
     * 0x81 entry of DS_001077A8 (a &1 port reads slot 1), which is null here. */
    f = (p45_anim_fn)(void *)fn_resolve(0x154E8u);
    CHECK(f != NULL, "0x154E8 is registered");
    if (f != NULL) {
        z_fseed();
        DSB(Z_R0 + 0x51u) = 0u;
        DSD(Z_R1 + 0x24u) = 0x24242424u;
        f(Z_R0, 0u);
        CHECK_EQ_INT((int)DSD(Z_R1 + 0x24u), 0x40C00000);
        z_fseed();
        DSB(Z_R0 + 0x51u) = 0x80u;
        DSD(0x001079ACu) = 0u;
        DSD(Z_R1 + 0x24u) = 0x24242424u;
        f(Z_R0, 0u);
        CHECK_EQ_INT((int)DSD(Z_R1 + 0x24u), 0x24242424);
    }
}

/* §P5.3: 0x21044's stores (track P batch 4+5 Task 2). */
static void p45_check_21044(void)
{
    p45_anim_fn f;
    f = (p45_anim_fn)(void *)fn_resolve(0x21044u);
    CHECK(f != NULL, "0x21044 is registered");
    if (f != NULL) {
        z_fseed();
        DSD(Z_R0 + 0x14u) = Z_S0;
        DSW(Z_R0 + 0x34u) = 0x3434u;
        DSW(Z_R0 + 0x36u) = 0x3636u;
        DSB(Z_R0 + 0x42u) = 0x42u;
        DSB(Z_R0 + 0x43u) = 0x43u;
        DSW(Z_R0 + 0x44u) = 0x4444u;
        DSB(Z_S0 + 0x57u) = 0x57u;
        f(Z_R0, 0u);
        /* the plain seed's actor word 0x0F35 has bit 15 clear: AL set, the word negated */
        CHECK_EQ_INT((int)DSW(Z_R0 + 0x34u), 0xFDA8);
        CHECK_EQ_INT((int)DSW(Z_R0 + 0x36u), 0x0096);
        CHECK_EQ_INT((int)DSB(Z_R0 + 0x43u), 0x0F);
        CHECK_EQ_INT((int)DSW(Z_R0 + 0x44u), 0x000F);
        CHECK_EQ_INT((int)DSB(Z_S0 + 0x57u), 1);
        z_fseed();
        DSD(Z_R0 + 0x14u) = Z_S0;
        DSW(Z_R0 + 0x34u) = 0x3434u;
        DSW(FIGHT_ACTORS + 0x20u) |= 0x8000u;   /* the record's actor word: AL clear */
        f(Z_R0, 0u);
        CHECK_EQ_INT((int)DSW(Z_R0 + 0x34u), 0x0258);
    }
}

'''
sub("port/tests/test_fight.c", "int test_p3_4844c(void)         { return u6b_run(p3_check_4844c); }\n",
    "int test_p3_4844c(void)         { return u6b_run(p3_check_4844c); }\n" + TEST)
sub("port/tests/test.h", "    X(test_p3_4844c) \\\n",
    "    X(test_p3_4844c) \\\n    X(test_p45_leaves) \\\n    X(test_p45_21044) \\\n")
print("t2_test applied")
PY
cmake --build build 2>&1 | grep -E ' error' | sed 's#.*/port/#port/#'
```

Expected (line numbers move with `test_fight.c`):

```
t2_test applied
port/tests/test_fight.c:46739:15: error: use of undeclared identifier 'p45_check_leaves'
port/tests/test_fight.c:46741:15: error: use of undeclared identifier 'p45_check_21044'
2 errors generated.
```

- [ ] **Step 3: the port, the registrations, the bindings and the mutants.**

```bash
python3 - <<'PY'
import pathlib


def sub(path, old, new):
    p = pathlib.Path(path)
    s = p.read_text()
    n = s.count(old)
    assert n == 1, "%s: %d occurrences of %r" % (path, n, old[:70])
    p.write_text(s.replace(old, new))


F = "port/src/game/fighter.c"
with open(F, "a") as f:
    f.write(r'''

#define P4_100C1D        0x00100C1Du  /* 0x18BCB */
#define P4_STREAMS_9B01C 0x0009B01Cu  /* 0x154B9: [char] */
#define P4_9B08C         0x0009B08Cu  /* 0x1553B: the spawned child's palette handle */
#define P4_DESC_9B07C    0x0009B07Cu  /* 0x15553 */
#define P4_STREAMS_C90F8 0x000C90F8u  /* 0x4010A/0x24419: [char] */
#define P4_VOICES_C75AA  0x000C75AAu  /* 0x40120: [char] word */
#define P4_104AE9        0x00104AE9u  /* 0x4013C */
#define P4_STREAM_229E8  0x000E1566u  /* 0x229E9 */
#define P5_C9783         0x000C9783u  /* 0x1570D/0x15724: the dword whose byte 3 is the signed step */
#define P5_C9784         0x000C9784u  /* 0x15739 */
#define P5_STREAMS_9B038 0x0009B038u  /* 0x15755: [char] */
#define P5_BDC64         0x000BDC64u  /* 0x37B97/0x37BAD: [char] word */
#define P5_DESC_BDC48    0x000BDC48u  /* 0x37BBA: [char] descriptor */
#define P5_DESC_A85DC    0x000A85DCu  /* 0x24495: [char] */
#define P5_104740        0x00104740u  /* 0x244A7/0x244B2 */
#define P5_STREAM_24454  0x000E5100u  /* 0x244EB */
#define P5_STREAMS_A85F8 0x000A85F8u  /* 0x24549: [char] */


/* 0x18BC8 — record §P4.3. The byte 0x100C1D = 0. */
void fighter_18bc8(u32 rec)
{
    (void)rec;
    DSB(P4_100C1D) = 0u;                                    /* 0x18BCA */
}

/* 0x21084 — record §P4.3. The record's +0x14 pointer; when non-zero: the
 * record's +0x1C = 0, its word +0x34 = 0, and the pointed record's +0x54 = 0,
 * +0x57 = 2. */
void fighter_21084(u32 rec)
{
    u32 held = DSD(rec + 0x14u);                            /* 0x21085 */
    if (held == 0u) return;                                 /* 0x21088/0x2108A */
    DSD(rec + 0x1Cu) = 0u;                                  /* 0x2108C */
    DSW(rec + 0x34u) = 0u;                                  /* 0x21093 */
    DSB(held + 0x54u) = 0u;                                 /* 0x21099 */
    DSB(held + 0x57u) = 2u;                                 /* 0x2109D */
}

/* 0x400E0 — record §P4.3. The record's +0x14 pointer; when non-zero its +0x42
 * bit 2 is cleared. */
void fighter_400e0(u32 rec)
{
    u32 held = DSD(rec + 0x14u);                            /* 0x400E0 */
    if (held == 0u) return;                                 /* 0x400E3/0x400E5 */
    DSB(held + 0x42u) &= (u8)~0x04u;                        /* 0x400E7 */
}

/* 0x21044 — record §P5.3. The record's +0x14 pointer; when non-zero its word
 * +0x34 = 0x258 (negated when 0x1A570(rec+0x51) sets AL), word +0x36 = 0x96,
 * word +0x44 = 0x0F, byte +0x43 = 0x0F, and the pointed record's +0x57 = 1. */
void fighter_21044(u32 rec)
{
    u32 held = DSD(rec + 0x14u);                            /* 0x21048 */
    if (held == 0u) return;                                 /* 0x2104B/0x2104D */
    DSW(rec + 0x34u) = 0x0258u;                             /* 0x21052 */
    DSW(rec + 0x36u) = 0x0096u;                             /* 0x21058 */
    DSW(rec + 0x44u) = 0x000Fu;                             /* 0x2105E */
    DSB(rec + 0x43u) = 0x0Fu;                               /* 0x21064 */
    if (fighter_actor_bit15_clear((u32)DSB(rec + 0x51u)) != 0)   /* 0x2104F/0x21068, 0x2106D..0x21074 */
        DSW(rec + 0x34u) = (u16)(0u - DSW(rec + 0x34u));    /* 0x21076 */
    DSB(held + 0x57u) = 1u;                                 /* 0x2107A */
}

/* 0x1549C — record §P4.4. The other side's slot (DS_001077A8 indexed by
 * (rec+0x51) ^ 1, zero-extended); when non-zero: its record on the 0x9B01C
 * stream of its character at 1.0 (0x2BC30), the palette 0x1F874590 (0x2A17C,
 * word 0), and the voice 0x50. */
void fighter_1549c(u32 rec)
{
    u32 other = (u32)DSB(rec + 0x51u) ^ 1u;                 /* 0x1549E..0x154A5 */
    u32 slot = DSD(DS_001077A8 + other * 4u);               /* 0x154A7 */
    if (slot == 0u) return;                                 /* 0x154AE/0x154B0 */
    actors_anim_begin(DSD(slot), DSD(P4_STREAMS_9B01C + (u32)DSB(slot + 0x7Au) * 4u),
                      0x3F800000u);                         /* 0x154B2..0x154C5 0x2BC30 */
    actor_pset_palette(DSD(slot), 0u, 0x1F874590u);         /* 0x154CA..0x154D3 0x2A17C */
    sound_voice(0x50u);                                     /* 0x154D8/0x154DD */
}

/* 0x154E8 — record §P4.4. The other side's slot as 0x1549C; when non-zero its
 * record's +0x24 = 0x40C00000 (6.0) and the voice 0xD2 (the raw's tail `jmp
 * 0x2C3FC`). */
void fighter_154e8(u32 rec)
{
    u32 other = (u32)DSB(rec + 0x51u) ^ 1u;                 /* 0x154E8..0x154ED */
    u32 slot = DSD(DS_001077A8 + other * 4u);               /* 0x154F2 */
    if (slot == 0u) return;                                 /* 0x154F9/0x154FB */
    DSD(DSD(slot) + 0x24u) = 0x40C00000u;                   /* 0x154FD..0x15506 */
    sound_voice(0xD2u);                                     /* 0x15506/0x1550B */
}

/* 0x229E8 — record §P4.4. The record on 0xE1566 at 3.0 (0x2BC30); the raw's
 * `lea eax,[eax]` (0x229F9) is a nop. */
void fighter_229e8(u32 rec)
{
    actors_anim_begin(rec, P4_STREAM_229E8, 0x40400000u);   /* 0x229E9..0x229F3 0x2BC30 */
}

/* 0x243F8 — record §P4.4. The context 0x33950(rec+0x51); the other slot's
 * record on its character's 0xC90F8 stream at 2.0 (0x2BC30), then that slot
 * 0x0A/9/0 with its +0x10 = 0. */
void fighter_243f8(u32 rec)
{
    u32 ctx[6];
    fighter_ctx_same(ctx, (u32)DSB(rec + 0x51u));           /* 0x243FC..0x24403 0x33950 */
    actors_anim_begin(ctx[5], DSD(P4_STREAMS_C90F8 + (u32)DSB(ctx[3] + 0x7Au) * 4u),
                      0x40000000u);                         /* 0x24408..0x24424 0x2BC30 */
    DSB(ctx[3] + 0x53u) = 0x0Au;                            /* 0x24429/0x2442D */
    DSB(ctx[3] + 0x52u) = 9u;                               /* 0x24431/0x24435 */
    DSB(ctx[3] + 0x54u) = 0u;                               /* 0x24439/0x2443D */
    DSD(ctx[3] + 0x10u) = 0u;                               /* 0x24441/0x24445 */
}
''')
sub("port/src/game/fighter.h", "void fighter_4844c(u32 slot, u32 rec, u32 side);\n",
    """void fighter_4844c(u32 slot, u32 rec, u32 side);
/* Track P batches 4 and 5 (record 2026-10-03-reverse-p4-p5-derivations.md): the
 * animation targets A and B. */
void fighter_18bc8(u32 rec);
void fighter_21084(u32 rec);
void fighter_400e0(u32 rec);
void fighter_21044(u32 rec);
void fighter_1549c(u32 rec);
void fighter_154e8(u32 rec);
void fighter_229e8(u32 rec);
void fighter_243f8(u32 rec);
""")
A = "port/src/game/actors.c"
sub(A, "static void anim_code_159A8(u32 rec, u32 arg);\n",
    "static void anim_code_159A8(u32 rec, u32 arg);\n" + "".join(
        "static void anim_code_%s(u32 rec, u32 arg);\n" % a for a in
        ("18BC8", "21084", "400E0", "21044", "1549C", "154E8", "229E8", "243F8")))
sub(A, "    fn_register(0x4844Cu, (void (*)(void))fighter_4844c);\n",
    """    fn_register(0x4844Cu, (void (*)(void))fighter_4844c);
    /* PORT: record 2026-10-03-reverse-p4-p5 §P4.4. Track P batches 4+5's first
     * animation targets (anim_indirect, EAX = rec). */
    fn_register(0x18BC8u, (void (*)(void))anim_code_18BC8);
    fn_register(0x21084u, (void (*)(void))anim_code_21084);
    fn_register(0x400E0u, (void (*)(void))anim_code_400E0);
    fn_register(0x21044u, (void (*)(void))anim_code_21044);
    fn_register(0x1549Cu, (void (*)(void))anim_code_1549C);
    fn_register(0x154E8u, (void (*)(void))anim_code_154E8);
    fn_register(0x229E8u, (void (*)(void))anim_code_229E8);
    fn_register(0x243F8u, (void (*)(void))anim_code_243F8);
""")
# the wrappers' definitions go at the end of actors.c
with open(A, "a") as f:
    f.write("""
/* ---- track P batches 4 and 5: the animation targets A and B (record 2026-10-03-reverse-p4-p5).
 * PORT: anim_indirect calls every code pointer as (rec, arg); the raw reads EAX = rec alone
 * (the operand's EDX and ECX are overwritten before any read in every one), so these
 * wrappers drop the operand. 0x24454 is not a stream target: it is the +0x10 handler
 * 0x24508 stores, reached by 0x3531C case 10, and is registered as its adapter. */
""" + "".join("static void anim_code_%s(u32 rec, u32 arg)\n{\n    (void)arg;\n    fighter_%s(rec);\n}\n"
              % (a, fn) for a, fn in (
    ("18BC8", "18bc8"), ("21084", "21084"), ("400E0", "400e0"), ("21044", "21044"),
    ("1549C", "1549c"), ("154E8", "154e8"), ("229E8", "229e8"), ("243F8", "243f8"))))
D = "port/tests/diff_runner.c"
BIND = r'''static void b_18bc8(const u32 *r, u32 *eax)      { fighter_18bc8(r[R_EAX]); *eax = 0u; }
static void b_21084(const u32 *r, u32 *eax)      { fighter_21084(r[R_EAX]); *eax = 0u; }
static void b_400e0(const u32 *r, u32 *eax)      { fighter_400e0(r[R_EAX]); *eax = 0u; }
static void b_21044(const u32 *r, u32 *eax)      { fighter_21044(r[R_EAX]); *eax = 0u; }
static void b_1549c(const u32 *r, u32 *eax)      { fighter_1549c(r[R_EAX]); *eax = 0u; }
static void b_154e8(const u32 *r, u32 *eax)      { fighter_154e8(r[R_EAX]); *eax = 0u; }
static void b_229e8(const u32 *r, u32 *eax)      { fighter_229e8(r[R_EAX]); *eax = 0u; }
static void b_243f8(const u32 *r, u32 *eax)      { fighter_243f8(r[R_EAX]); *eax = 0u; }
static void m_18bc8(const u32 *r, u32 *eax)      { (void)r; DSB(0x00100C1Du) = 1u; *eax = 0u; }
static void b_18bc8(const u32 *r, u32 *eax)      { fighter_18bc8(r[R_EAX]); *eax = 0u; }
static void m_21084(const u32 *r, u32 *eax)
{
    u32 rec = r[R_EAX], held = DSD(rec + 0x14u);
    if (held == 0u) return;
    DSD(rec + 0x1Cu) = 0u; DSW(rec + 0x34u) = 0u; DSB(held + 0x54u) = 0u; DSB(held + 0x57u) = 3u;
    *eax = 0u;
}
static void m_21084_byte14(const u32 *r, u32 *eax)
{
    u32 rec = r[R_EAX];
    if (DSB(rec + 0x14u) == 0u) return;
    DSD(rec + 0x1Cu) = 0u; DSW(rec + 0x34u) = 0u; DSB(DSD(rec + 0x14u) + 0x54u) = 0u;
    DSB(DSD(rec + 0x14u) + 0x57u) = 2u;
    *eax = 0u;
}
static void m_400e0(const u32 *r, u32 *eax)
{
    u32 held = DSD(r[R_EAX] + 0x14u);
    if (held != 0u) DSB(held + 0x42u) &= (u8)~0x02u;
    *eax = 0u;
}
static void b_400e0(const u32 *r, u32 *eax)      { fighter_400e0(r[R_EAX]); *eax = 0u; }
static void m_21044(const u32 *r, u32 *eax)
{
    u32 rec = r[R_EAX], held = DSD(rec + 0x14u);
    if (held == 0u) return;
    DSW(rec + 0x34u) = 0x0258u; DSW(rec + 0x36u) = 0x0097u; DSW(rec + 0x44u) = 0x000Fu;
    DSB(rec + 0x43u) = 0x0Fu;
    if (DSB(rec + 0x51u) == 0u) DSW(rec + 0x34u) = (u16)(0u - DSW(rec + 0x34u));
    DSB(held + 0x57u) = 1u;
    *eax = 0u;
}
static void m_21044_neg(const u32 *r, u32 *eax)
{
    u32 rec = r[R_EAX], held = DSD(rec + 0x14u);
    if (held == 0u) return;
    DSW(rec + 0x34u) = 0x0258u; DSW(rec + 0x36u) = 0x0096u; DSW(rec + 0x44u) = 0x000Fu;
    DSB(rec + 0x43u) = 0x0Fu;
    DSB(held + 0x57u) = 1u;
    *eax = 0u;
}
static void m_1549c(const u32 *r, u32 *eax)
{
    u32 other = (u32)DSB(r[R_EAX] + 0x51u) ^ 1u, slot = DSD(0x001077A8u + other * 4u);
    if (slot == 0u) return;
    actors_anim_begin(DSD(slot), DSD(0x0009B01Cu + (u32)DSB(slot + 0x7Au) * 4u), 0x3F800000u);
    actor_pset_palette(DSD(slot), 0u, 0x1F874590u);
    sound_voice(0x51u);
    *eax = 0u;
}
static void m_1549c_side(const u32 *r, u32 *eax)
{
    u32 other = ((u32)DSB(r[R_EAX] + 0x51u) & 1u) ^ 1u, slot = DSD(0x001077A8u + other * 4u);
    if (slot == 0u) return;
    actors_anim_begin(DSD(slot), DSD(0x0009B01Cu + (u32)DSB(slot + 0x7Au) * 4u), 0x3F800000u);
    actor_pset_palette(DSD(slot), 0u, 0x1F874590u);
    sound_voice(0x50u);
    *eax = 0u;
}
static void m_154e8(const u32 *r, u32 *eax)
{
    u32 other = (u32)DSB(r[R_EAX] + 0x51u) ^ 1u, slot = DSD(0x001077A8u + other * 4u);
    if (slot == 0u) return;
    DSD(DSD(slot) + 0x24u) = 0x40C00001u;
    sound_voice(0xD2u);
    *eax = 0u;
}
static void m_154e8_side(const u32 *r, u32 *eax)
{
    u32 other = ((u32)DSB(r[R_EAX] + 0x51u) & 1u) ^ 1u, slot = DSD(0x001077A8u + other * 4u);
    if (slot == 0u) return;
    DSD(DSD(slot) + 0x24u) = 0x40C00000u;
    sound_voice(0xD2u);
    *eax = 0u;
}
static void m_229e8(const u32 *r, u32 *eax)      { actors_anim_begin(r[R_EAX], 0x000E1564u, 0x40400000u); *eax = 0u; }
static void b_229e8(const u32 *r, u32 *eax)      { fighter_229e8(r[R_EAX]); *eax = 0u; }
static void m_243f8(const u32 *r, u32 *eax)
{
    u32 ctx[6];
    fighter_ctx_same(ctx, (u32)DSB(r[R_EAX] + 0x51u));
    actors_anim_begin(ctx[5], DSD(0x000C90F8u + (u32)DSB(ctx[2] + 0x7Au) * 4u), 0x40000000u);
    DSB(ctx[3] + 0x53u) = 0x0Au; DSB(ctx[3] + 0x52u) = 9u; DSB(ctx[3] + 0x54u) = 0u;
    DSD(ctx[3] + 0x10u) = 0u;
    *eax = 0u;
}
static void m_243f8_slot(const u32 *r, u32 *eax)
{
    u32 ctx[6];
    fighter_ctx_same(ctx, (u32)DSB(r[R_EAX] + 0x51u));
    actors_anim_begin(ctx[5], DSD(0x000C90F8u + (u32)DSB(ctx[3] + 0x7Au) * 4u), 0x40000000u);
    DSB(ctx[2] + 0x53u) = 0x0Au; DSB(ctx[2] + 0x52u) = 9u; DSB(ctx[2] + 0x54u) = 0u;
    DSD(ctx[2] + 0x10u) = 0u;
    *eax = 0u;
}

'''
sub(D, "static const binding_t k_bindings[] = {\n", BIND + "static const binding_t k_bindings[] = {\n")
sub(D, '    { "fighter_4844c@width",      m_4844c_width,  0x00000000u },\n};',
    '    { "fighter_4844c@width",      m_4844c_width,  0x00000000u },\n' + r'''    { "fighter_18bc8",          b_18bc8,         0x00000000u },
    { "fighter_18bc8@mutant",   m_18bc8,         0x00000000u },
    { "fighter_21084",          b_21084,         0x00000000u },
    { "fighter_21084@mutant",   m_21084,         0x00000000u },
    { "fighter_21084@byte14",   m_21084_byte14,  0x00000000u },
    { "fighter_400e0",          b_400e0,         0x00000000u },
    { "fighter_400e0@mutant",   m_400e0,         0x00000000u },
    { "fighter_21044",          b_21044,         0x00000000u },
    { "fighter_21044@mutant",   m_21044,         0x00000000u },
    { "fighter_21044@neg",      m_21044_neg,     0x00000000u },
    { "fighter_1549c",          b_1549c,         0x00000000u },
    { "fighter_1549c@mutant",   m_1549c,         0x00000000u },
    { "fighter_1549c@side",     m_1549c_side,    0x00000000u },
    { "fighter_154e8",          b_154e8,         0x00000000u },
    { "fighter_154e8@mutant",   m_154e8,         0x00000000u },
    { "fighter_154e8@side",     m_154e8_side,    0x00000000u },
    { "fighter_229e8",          b_229e8,         0x00000000u },
    { "fighter_229e8@mutant",   m_229e8,         0x00000000u },
    { "fighter_243f8",          b_243f8,         0x00000000u },
    { "fighter_243f8@mutant",   m_243f8,         0x00000000u },
    { "fighter_243f8@slot",     m_243f8_slot,    0x00000000u },

};''')
print("t2_port applied")
PY
cmake --build build 2>&1 | grep -E 'error|warning'; PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -1
for f in fighter_18bc8 fighter_21084 fighter_400e0 fighter_21044 fighter_1549c fighter_154e8 fighter_229e8 fighter_243f8; do
  python3 tools/diff_verify.py --image /tmp/pr_p45_img.bin --function $f --self-check | grep -E '^\| fighter'; done
```

Expected: no compiler message, then `all checks passed` and the rows (measured):

| fighter_18bc8 | 0x18BC8 | 1 | 1/1 | VERIFIED | - |
| fighter_21084 | 0x21084 | 2 | 3/3 | VERIFIED | - |
| fighter_400e0 | 0x400E0 | 3 | 3/3 | VERIFIED | - |
| fighter_1549c | 0x1549C | 3 | 3/3 | VERIFIED | 2A17C stub unverified, 2BC30 stub unverified, 2C3FC stub unverified |
| fighter_154e8 | 0x154E8 | 4 | 3/3 | VERIFIED | 2C3FC stub unverified |
| fighter_229e8 | 0x229E8 | 2 | 1/1 | VERIFIED | 2BC30 stub unverified |
| fighter_243f8 | 0x243F8 | 2 | 1/1 | VERIFIED | 2BC30 stub unverified, 33950 allow VERIFIED |
| fighter_21044 | 0x21044 | 3 | 5/5 | VERIFIED | 1A570 stub VERIFIED |
| fighter_18bc8@mutant | 0x18BC8 | 1 | 1/1 | MISMATCH | - |
| fighter_21084@mutant | 0x21084 | 2 | 3/3 | MISMATCH | - |
| fighter_21084@byte14 | 0x21084 | 2 | 3/3 | MISMATCH | - |
| fighter_400e0@mutant | 0x400E0 | 3 | 3/3 | MISMATCH | - |
| fighter_1549c@mutant | 0x1549C | 3 | 3/3 | MISMATCH | 2A17C stub unverified, 2BC30 stub unverified, 2C3FC stub unverified |
| fighter_1549c@side | 0x1549C | 3 | 3/3 | MISMATCH | 2A17C stub unverified, 2BC30 stub unverified, 2C3FC stub unverified |
| fighter_154e8@mutant | 0x154E8 | 4 | 3/3 | MISMATCH | 2C3FC stub unverified |
| fighter_154e8@side | 0x154E8 | 4 | 3/3 | MISMATCH | 2C3FC stub unverified |
| fighter_229e8@mutant | 0x229E8 | 2 | 1/1 | MISMATCH | 2BC30 stub unverified |
| fighter_243f8@mutant | 0x243F8 | 2 | 1/1 | MISMATCH | 2BC30 stub unverified, 33950 allow VERIFIED |
| fighter_243f8@slot | 0x243F8 | 2 | 1/1 | MISMATCH | 2BC30 stub unverified, 33950 allow VERIFIED |
| fighter_21044@mutant | 0x21044 | 3 | 5/5 | MISMATCH | 1A570 stub VERIFIED |
| fighter_21044@neg | 0x21044 | 3 | 5/5 | MISMATCH | 1A570 stub VERIFIED |


- [ ] **Step 4: the E2 table** (`make entry-triage` first, then regenerate):

```bash
make entry-triage E2_IMAGE=/tmp/pr_p45_e2.bin 2>&1 | tail -2
python3 tools/entry_triage.py --image /tmp/pr_p45_e2.bin --live docs/superpowers/plans/2026-10-01-reverse-e2-live-functions.txt \
  --out docs/superpowers/plans/2026-10-01-reverse-e2-triage.md | tail -2
grep -E '^\| (callbacks|finishers|animation-targets|stubs) ' docs/superpowers/plans/2026-10-01-reverse-e2-triage.md
git diff --stat docs/superpowers/plans/2026-10-01-reverse-e2-triage.md | tail -1
```

Expected: `entry-triage: FAIL: ... differs from a fresh run` then `make: *** [entry-triage] Error 1`, the regenerated line `entry-triage: targets 280 unported, 215 ported; supplement 131 (22 unported, 0 stale); untrusted entries 30`, and the animation-targets row `| animation-targets | 46 | 66 |` (8 rows moved).

- [ ] **Step 5: the task gate.**

```bash
make diff-verify entry-triage DIFF_IMAGE=/tmp/pr_p45_d.bin DIFF_TABLE=/tmp/pr_p45_d.md E2_IMAGE=/tmp/pr_p45_e2.bin 2>&1 \
  | grep -E '^(Ran|OK|FAILED|diff-verify:|entry-triage: targets)'
```

Expected (the times vary):

```
Ran 163 tests in N.NNNs
OK
diff-verify: 86/86 functions VERIFIED; 171/171 mutants detected; 1 named gaps; 12/69 rows with callees closed (17 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
Ran 44 tests in N.NNNs
OK
entry-triage: targets 280 unported, 215 ported; supplement 131 (22 unported, 0 stale); untrusted entries 30
```

- [ ] **Step 6: every new unit assertion can fail.** Eight mutations of the code under test, each restored after its run (measured; the line numbers move with `test_fight.c`):

```bash
python3 - <<'PY'
import os
import pathlib
import subprocess


def mutate(path, old, new):
    p = pathlib.Path(path)
    keep = p.read_text()
    assert keep.count(old) == 1, (path, old[:60])
    p.write_text(keep.replace(old, new))
    try:
        subprocess.run(["cmake", "--build", "build"], check=True, capture_output=True)
        out = subprocess.run(["./build/run_tests"], capture_output=True, text=True,
                             env={**os.environ, "PR_ORACLE_REQUIRED": "1"}).stdout
        fails = [l.split("port/tests/")[-1] for l in out.splitlines() if l.startswith("FAIL ")]
        print("%s: %d FAIL, first: %s" % (path, len(fails), fails[:1]))
    finally:
        p.write_text(keep)
        subprocess.run(["cmake", "--build", "build"], check=True, capture_output=True)


F = "port/src/game/fighter.c"
mutate("port/src/game/actors.c", "    fn_register(0x18BC8u, (void (*)(void))anim_code_18BC8);\n", "")
mutate(F, "    DSB(held + 0x57u) = 2u;                                 /* 0x2109D */",
       "    DSB(held + 0x57u) = 3u;                                 /* 0x2109D */")
mutate(F, "    DSB(held + 0x42u) &= (u8)~0x04u;                        /* 0x400E7 */",
       "    DSB(held + 0x42u) &= (u8)~0x02u;                        /* 0x400E7 */")
mutate(F, "    u32 other = (u32)DSB(rec + 0x51u) ^ 1u;                 /* 0x154E8..0x154ED */",
       "    u32 other = ((u32)DSB(rec + 0x51u) & 1u) ^ 1u;          /* 0x154E8..0x154ED */")
mutate(F, "    actors_anim_begin(DSD(own), P4_STREAM_3427C, 0x40400000u);   /* 0x342F4..0x342FD */",
       "    actors_anim_begin(DSD(DS_001077B0 + (1u - side) * 0x94u), P4_STREAM_3427C, 0x40400000u);   /* 0x342F4..0x342FD */")
mutate(F, "    DSD(P4_9B08C) = fighter_29c08(side, (u32)DSB(DS_0010782A + side * 0x94u));   /* 0x1551B..0x1553B 0x29C08 */",
       "    DSD(0x0009B090u) = fighter_29c08(side, (u32)DSB(DS_0010782A + side * 0x94u));   /* 0x1551B..0x1553B 0x29C08 */")
mutate(F, "        DSD(srec + 0x18u) = DSD(rec + 0x18u) - (u32)((s32)DSD(P5_C9783) >> 24) * 0x40u;   /* 0x1570D..0x1571F */",
       "        DSD(srec + 0x18u) = DSD(rec + 0x18u) - (u32)((u32)DSD(P5_C9783) >> 24) * 0x40u;   /* 0x1570D..0x1571F */")
mutate(F, """    if (fighter_actor_bit15_clear((u32)DSB(rec + 0x51u)) != 0)   /* 0x2104F/0x21068, 0x2106D..0x21074 */
        DSW(rec + 0x34u) = (u16)(0u - DSW(rec + 0x34u));    /* 0x21076 */""",
       """    if (0)                                              /* 0x2104F/0x21068, 0x2106D..0x21074 */
        DSW(rec + 0x34u) = (u16)(0u - DSW(rec + 0x34u));    /* 0x21076 */""")
PY
PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -1
```

Expected (measured on the planner's tree; line numbers move):

```
port/src/game/actors.c: 1 FAIL, first: ['test_fight.c:46737: 0x18BC8 is registered']
port/src/game/fighter.c: 1 FAIL, first: ['test_fight.c:46758: 3 != 2']
port/src/game/fighter.c: 1 FAIL, first: ['test_fight.c:46774: 253 != 251']
port/src/game/fighter.c: 1 FAIL, first: ['test_fight.c:46793: 1086324736 != 606348324']
port/src/game/fighter.c: 4 FAIL, first: ["test_fight.c:46817: the side's record started a stream"]
port/src/game/fighter.c: 1 FAIL, first: ['test_fight.c:46854: 0x9B08C holds the palette handle']
port/src/game/fighter.c: 1 FAIL, first: ['test_fight.c:46928: -2024 != 14360']
port/src/game/fighter.c: 1 FAIL, first: ['test_fight.c:46947: 600 != 64936']
all checks passed
```

- [ ] **Step 7: gp-u9-win drops its three rows and re-pins** (record §P4.7). The set gains the three targets the replay then reaches; the trace pin rises to 2364; every pin is exact (each + 1 fails).

```bash
python3 - <<'PY'
import pathlib


def sub(path, old, new):
    p = pathlib.Path(path)
    s = p.read_text()
    n = s.count(old)
    assert n == 1, "%s: %d occurrences of %r" % (path, n, old[:70])
    p.write_text(s.replace(old, new))


# port/tests/test_platform.c: the three ported rows leave; the three the replay reaches enter
sub("port/tests/test_platform.c", '''static const fnm_pair k_miss_gp_u9_win[] = {
    { 0x29D60u, "frontend_mode_1b_step" },
    { 0x5D812u, "frontend_mode_1b_step" },
    { 0x400E0u, "anim_indirect" },
    { 0x21044u, "anim_indirect" },
    { 0x21084u, "anim_indirect" },
};''', '''static const fnm_pair k_miss_gp_u9_win[] = {
    { 0x29D60u, "frontend_mode_1b_step" },
    { 0x5D812u, "frontend_mode_1b_step" },
    { 0x213F0u, "anim_indirect" },
    { 0x213F4u, "anim_indirect" },
    { 0x2BDA0u, "anim_indirect" },
};''')
# Makefile: the trace pin and the provenance
sub("Makefile", "GP_WIN_TRACE_MIN_FIRST = 2150", "GP_WIN_TRACE_MIN_FIRST = 2364")
sub("Makefile", '''# Re-measure: a P batch that ports 0x400E0 or 0x21084 (P4) or 0x21044 (P5) drops its row,
# re-measures the U9 set (match 2's stage follows the rng, so the set can change) and re-pins
# TRACE/WIN/MIN_FIRST: the trace and win ratchets fail only when they get worse.''',
'''# Re-measured by track P batches 4+5 (record 2026-10-03-reverse-p4-p5 §P4.7/§P5.7) once 0x400E0,
# 0x21044 and 0x21084 are ported: the set loses those three rows and gains the targets the replay
# then reaches (0x213F0 P7 at f=0x8ED, 0x213F4 P7 at f=0x91F, 0x2BDA0 P6 at f=0xD0C, all skipped
# by anim_indirect), and TRACE_MIN_FIRST rises 2150 -> 2364 (the first trace difference; the WIN
# ratchet stays 3162, MIN_FIRST 346 and MAX_START 100).''')
# AGENTS.md: the re-measure list
sub("AGENTS.md", '''  a target in a miss set re-measures and re-pins (U9: `0x400E0`/`0x21084` P4, `0x21044` P5; U10:
  `0x3DA50` P5, `0x37DD4` P6, `0x29C78` P7; record §W.16; track P batch 3 ported `0x475EC`, record
  2026-10-03-reverse-p3 §P3.9).''', '''  a target in a miss set re-measures and re-pins (U10: `0x37DD4` P6, `0x29C78` P7; record §W.16;
  track P batch 3 ported `0x475EC`, record 2026-10-03-reverse-p3 §P3.9; batches 4+5 ported
  `0x400E0`/`0x21084` P4, `0x21044` P5 and `0x3DA50` P5: U9's set gains `0x213F0`/`0x213F4` P7 and
  `0x2BDA0` P6 (first frames 0x8ED, 0x91F, 0xD0C) and its TRACE pin rises 2150 -> 2364; U10's pins
  are unchanged; record 2026-10-03-reverse-p4-p5 §P4.7/§P5.7).''')
print("t2_u9 applied")
PY
cmake --build build 2>&1 | grep -E 'error|warning'
make gp-win-oracle GP_DUMP=/tmp/pr_p45_gp GP_WIN_KEEP=1 2>&1 | grep -E '^(Ran|OK|FAILED)|ratchet N|FAIL|fn-miss PR_GP_DUMP distinct'
SC=gp-u9-win; CAP=data/k11-captures/$SC; SHA=7dcea0f16403c0fa52b6ef690edbcccb5340d9b9d1ec96c66ca4d11770708ba8
python3 tools/gp_compare.py --scenario $SC --capture $CAP --port /tmp/pr_p45_gp/$SC --min-first 347 --trace-min-first 2365 \
  --max-start 100 --capture-sha256 $SHA --capture-frames 2521 2>&1 | grep -E 'FAIL'
python3 tools/gp_win.py path --scenario $SC --capture $CAP --port /tmp/pr_p45_gp/$SC --min-milestones 9 --win-min-first 3163 \
  --capture-sha256 $SHA 2>&1 | grep -E 'FAIL'
rm -rf /tmp/pr_p45_gp/$SC
```

Expected (measured): the oracle passes with the new set (`distinct=7`) and each pin + 1 fails, so every pin is exact:

```
t2_u9 applied
Ran 18 tests in N.NNNs
OK
fn-miss PR_GP_DUMP distinct=7 dropped=0
gp_compare: gp-u9-win: frames: first unexplained 346, ratchet N 346 ok
gp_compare: gp-u9-win: trace: first differing 2364, ratchet N 2364 ok
gp_compare: gp-u9-win: path: 0 not reproduced through 7; ratchet N 8 ok
gp_compare: gp-u9-win: win: first differing 3162, ratchet N 3162 ok
gp_compare: gp-u9-win: frames: FAIL: first unexplained 346 < ratchet N 347
gp_compare: gp-u9-win: trace: FAIL: first differing 2364 < ratchet N 2365
gp_win: gp-u9-win: path: FAIL: 8 > end 7: N is unreachable
gp_compare: gp-u9-win: win: FAIL: first differing 3162 < ratchet N 3163
```

The miss-log lines measured on the full replay (record §P4.7): `0x213F0 anim_indirect first=0x8ED`, `0x213F4 anim_indirect first=0x91F`, `0x2BDA0 anim_indirect first=0xD0C` (hits 1, 1, 10). A `distinct` above 7 means the replay reaches something new: stop, read the new pair's first frame and report; do not pin it unclassified.

- [ ] **Step 8: commit.**

```bash
git add tools/diff_verify.py tools/tests/test_diff_verify.py port/tests/test_fight.c port/tests/test.h \
  port/src/game/fighter.c port/src/game/fighter.h port/src/game/actors.c port/tests/diff_runner.c \
  docs/superpowers/plans/2026-10-01-reverse-e2-triage.md \
  port/tests/test_platform.c Makefile AGENTS.md
git commit -m "fighter: port the U9 animation targets 0x18BC8 0x21084 0x400E0 0x21044 0x1549C 0x154E8 0x229E8 0x243F8; gp-u9-win re-pinned (trace 2364, three new targets); E2 table regenerated (track P batches 4+5, task 2)"
```

---

### Task 3: the held-record spawns `0x15510 0x241A8 0x40358 0x45C54 0x489DC 0x400EC` (seam `0x29C08`)

**Files:** as Task 2 Steps 1-6 (the anchors move to Task 2's last lines).

**Interfaces:** produces `u32 fighter_29c08(u32 side, u32 ch)` exported with `PR_SEAM_RET(0x29C08u, side, ch)` (the palette handle `0x15510` stores); `void fighter_15510(u32 rec)` and the same for `fighter_241a8`, `fighter_40358`, `fighter_45c54`, `fighter_489dc`, `fighter_400ec`; `CALL29C08`; `test_p45_p4_spawns`. Consumes Task 2's `P45_SPECS`/`P45_MASKS`/`P45_KINDS`, `P45_PTRS`/`P45_CHARS`/`P45_FAKE`, P2's `SPAWN`, P3's `DIRS`? (no), P1's `PALETTE`.

- [ ] **Step 1: the specs.**

```bash
python3 - <<'PY'
import pathlib


def sub(path, old, new):
    p = pathlib.Path(path)
    s = p.read_text()
    n = s.count(old)
    assert n == 1, "%s: %d occurrences of %r" % (path, n, old[:70])
    p.write_text(s.replace(old, new))


sub("tools/diff_verify.py", "]\n\n\nSPECS = [\n    Spec(\"rng_next\", 0x5D7DC, [\n",
    "]\n\n\nP45_SPECS += [\nSpec("fighter_15510", 0x15510, [
        Case("s0", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {**P45_CHARS, E3_REC + 0x51: b"\x00", E3_REC + 0x56: b"\x23\x01", E3_REC + 0x4B: b"\x4b",
              0x9B08C: le32(0x9B8C8C8C), E3_OUT + 0x56: b"\x07\x00", E3_OUT + 0x59: b"\x59"},
             {0x29C08: 0x11111111, 0x2AE14: E3_OUT}),
        Case("s1", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {**P45_CHARS, E3_REC + 0x51: b"\x01", E3_REC + 0x56: b"\x23\x01", E3_REC + 0x4B: b"\x4b",
              0x9B08C: le32(0x9B8C8C8C), E3_REC2 + 0x56: b"\x07\x00", E3_REC2 + 0x59: b"\x59"},
             {0x29C08: 0x22222222, 0x2AE14: E3_REC2}),
        Case("s2", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {**P45_CHARS, 0x107952: b"\x77", E3_REC + 0x51: b"\x02", E3_REC + 0x56: b"\x23\x01",
              E3_REC + 0x4B: b"\x4b", 0x9B08C: le32(0x9B8C8C8C), E3_OUT + 0x56: b"\x07\x00",
              E3_OUT + 0x59: b"\x59"},
             {0x29C08: 0x33333333, 0x2AE14: E3_OUT}),
    ], calls=(CALL29C08, SPAWN), eax_mask=0, mutants=("@mutant", "@side", "@pal", "@a5")),

Spec("fighter_241a8", 0x241A8, [
        Case("n0", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {**P45_PTRS, 0x1077A8: le32(0), **P45_CHARS}),
        Case("n1", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {**P45_PTRS, **P45_CHARS, 0xA84E0 + 20: le32(0x000A1111), E3_REC + 0x56: b"\x23\x01",
              E3_REC + 0x4B: b"\x4b", E3_OUT + 0x56: b"\x07\x00"},
             {0x2AE14: E3_OUT}),
        Case("n2", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {**P45_PTRS, **P45_CHARS, 0xA84E0 + 12: le32(0x000A2222), E3_REC + 0x56: b"\x23\x01",
              E3_REC + 0x4B: b"\x4b", E3_REC2 + 0x56: b"\x07\x00"},
             {0x2AE14: E3_REC2}),
        Case("n3", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {**P45_PTRS, **P45_CHARS, 0x1079A8: le32(P45_SLOT3), 0x1075A8: le32(E3_REC2),
              P45_SLOT3 + 0x7A: b"\x81", 0xA84E0 + 0x204: le32(0x000A3333), E3_REC + 0x51: b"\x80",
              E3_REC + 0x56: b"\x23\x01",
              E3_REC + 0x4B: b"\x4b", E3_OUT + 0x56: b"\x07\x00"},
             {0x2AE14: E3_OUT}),
    ], calls=(SPAWN,), eax_mask=0, mutants=("@mutant", "@sext")),

Spec("fighter_40358", 0x40358, [
        Case("h0", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(0), E3_REC + 0x56: b"\x23\x01", E3_REC + 0x4B: b"\x4b"}),
        Case("h1", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(E3_SLOT), E3_REC + 0x51: b"\x01", E3_REC + 0x56: b"\x23\x01",
              E3_REC + 0x4B: b"\x4b", E3_OUT + 0x14: le32(0x14141414), E3_OUT + 0x51: b"\x51",
              E3_OUT + 0x56: b"\x07\x00"},
             {0x2AE14: E3_OUT}),
    ], calls=(SPAWN,), eax_mask=0, mutants=("@mutant",)),

Spec("fighter_45c54", 0x45C54, [
        Case("h0", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(0), E3_REC + 0x56: b"\x23\x01"}),
        Case("h1", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(E3_SLOT), E3_REC + 0x56: b"\x23\x01", E3_OUT + 0x14: le32(0x14141414),
              E3_OUT + 0x59: b"\x59"},
             {0x2AE14: E3_OUT}),
    ], calls=(SPAWN, VOICE), eax_mask=0, mutants=("@mutant",)),

Spec("fighter_489dc", 0x489DC, [
        Case("h0", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(0), E3_REC + 0x56: b"\x23\x01"}),
        Case("h1", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(E3_SLOT), E3_SLOT: le32(E3_OUT), E3_OUT + 0x4B: b"\x4b",
              E3_REC + 0x56: b"\x23\x01", E3_OUT + 0x56: b"\x07\x00"}),
        Case("h2", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(E3_SLOT), E3_SLOT: le32(E3_OUT), E3_OUT + 0x4B: b"\x00",
              E3_REC + 0x56: b"\x23\x01", E3_OUT + 0x56: b"\x07\x00"},
             {0x2AE14: E3_OUT}),
    ], calls=(SPAWN,), eax_mask=0, mutants=("@mutant",)),

Spec("fighter_400ec", 0x400EC, [
        Case("s0", {"eax": E3_OUT, "edx": 0x1234, "ecx": 0},
             {**P45_PTRS, **P45_CHARS, E3_OUT + 0x51: b"\x00", 0xC90F8 + 20: le32(0xAAAAAAAA),
              0xC90F8 + 12: le32(0xBBBBBBBB), 0xC75AA + 10: b"\x60\x00", 0xC75AA + 6: b"\x61\x00",
              0x104AE9: b"\xa9\xaa"}),
        Case("s1", {"eax": E3_OUT, "edx": 0x1234, "ecx": 0},
             {**P45_PTRS, **P45_CHARS, E3_OUT + 0x51: b"\x01", 0xC90F8 + 20: le32(0xAAAAAAAA),
              0xC90F8 + 12: le32(0xBBBBBBBB), 0xC75AA + 10: b"\x60\x00", 0xC75AA + 6: b"\x61\x00",
              0x104AE9: b"\xa9\xaa"}),
        Case("s2", {"eax": E3_OUT, "edx": 0x1234, "ecx": 0},
             {**P45_PTRS, **P45_CHARS, **P45_FAKE, E3_OUT + 0x51: b"\x80",
              0xC90F8 + 0x204: le32(0xCCCCCCCC), 0xC75AA + 0x102: b"\x62\x00", 0x104AE9: b"\xa9\xaa"}),
    ], calls=(ANIM_BEGIN, VOICE), eax_mask=0, mutants=("@mutant", "@side")),
\n]\n\n\nSPECS = [\n    Spec(\"rng_next\", 0x5D7DC, [\n")
print("t3_spec applied")
PY
python3 tools/diff_verify.py --image /tmp/pr_p45_img.bin --function fighter_15510 | grep -E '^\| fighter|unknown binding|^diff-verify'
```

Expected: `t3_spec applied` and `| fighter_15510 | 0x15510 | 3 | 1/1 | MISMATCH | 29C08 stub unverified, 2AE14 stub unverified |` with the unknown-binding lines for its cases.

- [ ] **Step 2: the unit check.**

```bash
python3 - <<'PY'
import pathlib


def sub(path, old, new):
    p = pathlib.Path(path)
    s = p.read_text()
    n = s.count(old)
    assert n == 1, "%s: %d occurrences of %r" % (path, n, old[:70])
    p.write_text(s.replace(old, new))


sub("port/tests/test_fight.c", "int test_p45_21044(void)        { return u6b_run(p45_check_21044); }\n",
    "int test_p45_21044(void)        { return u6b_run(p45_check_21044); }\n" + r'''
/* §P4.4: 0x15510's palette handle and child; the held-record spawns. */
static void p45_check_p4_spawns(void)
{
    static u32 before[0x80];
    p45_anim_fn f;
    u32 n, child, x;
    f = (p45_anim_fn)(void *)fn_resolve(0x15510u);
    CHECK(f != NULL, "0x15510 is registered");
    if (f == NULL) return;
    z_fseed();
    c4r_pool();
    DSB(Z_R0 + 0x51u) = 0u;
    DSB(Z_R0 + 0x56u) = 0x23u;
    DSB(Z_R0 + 0x57u) = 0x01u;
    DSB(Z_R0 + 0x4Bu) = 0x4Bu;
    DSD(0x0009B08Cu) = 0x9B8C8C8Cu;
    n = u6b_list(before, 0x80u);
    f(Z_R0, 0u);
    CHECK(DSD(0x0009B08Cu) != 0x9B8C8C8Cu, "0x9B08C holds the palette handle");
    child = p45_new_child(before, n, &x);
    CHECK_EQ_INT((int)x, 1);
    if (x == 1u) {
        CHECK_EQ_INT((int)DSB(child + 0x59u), 2);
        CHECK_EQ_INT((int)DSB(Z_R0 + 0x4Bu), (int)DSB(child + 0x56u));
    }

    /* 0x241A8: the side's slot char selects the descriptor. */
    f = (p45_anim_fn)(void *)fn_resolve(0x241A8u);
    CHECK(f != NULL, "0x241A8 is registered");
    if (f == NULL) return;
    z_fseed();
    c4r_pool();
    DSB(Z_R0 + 0x51u) = 0u;
    DSB(Z_R0 + 0x56u) = 0x23u;
    DSB(Z_R0 + 0x57u) = 0x01u;
    DSB(Z_R0 + 0x4Bu) = 0x4Bu;
    n = u6b_list(before, 0x80u);
    f(Z_R0, 0u);
    child = p45_new_child(before, n, &x);
    CHECK_EQ_INT((int)x, 1);
    if (x == 1u)
        CHECK_EQ_INT((int)DSB(Z_R0 + 0x4Bu), (int)DSB(child + 0x56u));

    /* 0x40358: the child's +0x14 is the held record and its +0x51 the side;
     * 0x45C54: its +0x59 = 2 and +0x14 the held record; 0x489DC: the held
     * record's +0x4B takes the child's index. */
    {
        static const u32 addr[3] = { 0x40358u, 0x45C54u, 0x489DCu };
        u32 k;
        for (k = 0; k < 3u; k++) {
            f = (p45_anim_fn)(void *)fn_resolve(addr[k]);
            CHECK(f != NULL, "the held-record spawn is registered");
            if (f == NULL) return;
            z_fseed();
            c4r_pool();
            DSD(Z_R0 + 0x14u) = Z_S0;
            DSB(Z_R0 + 0x51u) = 1u;
            DSB(Z_R0 + 0x56u) = 0x23u;
            DSB(Z_R0 + 0x57u) = 0x01u;
            DSB(Z_R0 + 0x4Bu) = 0x4Bu;
            DSD(Z_S0) = Z_R1;
            DSB(Z_R1 + 0x4Bu) = 0u;
            n = u6b_list(before, 0x80u);
            f(Z_R0, 0u);
            child = p45_new_child(before, n, &x);
            CHECK_EQ_INT((int)x, 1);
            if (x != 1u) continue;
            if (k == 0u) CHECK_EQ_INT((int)DSB(child + 0x51u), 1);
            if (k == 1u) CHECK_EQ_INT((int)DSB(child + 0x59u), 2);
            if (k < 2u) CHECK_EQ_INT((int)DSD(child + 0x14u), (int)Z_S0);
            if (k == 2u) CHECK_EQ_INT((int)DSB(Z_R1 + 0x4Bu), (int)DSB(child + 0x56u));
        }
    }
}

''')
sub("port/tests/test.h", "    X(test_p45_21044) \\\n", "    X(test_p45_21044) \\\n    X(test_p45_p4_spawns) \\\n")
print("t3_test applied")
PY
cmake --build build 2>&1 | grep -E ' error' | sed 's#.*/port/#port/#'
```

Expected: `t3_test applied` and one undeclared-identifier error for `p45_check_p4_spawns`.

- [ ] **Step 3: the port, the seam, the bindings and the mutants.**

```bash
python3 - <<'PY'
import pathlib


def sub(path, old, new):
    p = pathlib.Path(path)
    s = p.read_text()
    n = s.count(old)
    assert n == 1, "%s: %d occurrences of %r" % (path, n, old[:70])
    p.write_text(s.replace(old, new))


F = "port/src/game/fighter.c"
# 0x29C08 gains its seam and loses `static`
sub(F, '''/* 0x29C08. The palette handle for (side, char): DSD(DSD(0xA8A98 + char*4) +
 * DSB(0x105B34 + side)*4). EAX = side, EDX = char. */
static u32 fighter_29c08(u32 side, u32 ch)
{
    u32 row = DSD(DS_000A8A98 + ch * 4u);               /* 0x29C08 */
    return DSD(row + (u32)DSB(DS_00105B34 + side) * 4u);   /* 0x29C0F..0x29C1A */
}''', '''/* 0x29C08. The palette handle for (side, char): DSD(DSD(0xA8A98 + char*4) +
 * DSB(0x105B34 + side)*4). EAX = side, EDX = char; a plain `ret`, it clobbers
 * nothing. No longer file-local: track P batch 4's 0x15510 stubs it. */
u32 fighter_29c08(u32 side, u32 ch)
{
    u32 row;
    PR_SEAM_RET(0x29C08u, side, ch);
    row = DSD(DS_000A8A98 + ch * 4u);                   /* 0x29C08 */
    return DSD(row + (u32)DSB(DS_00105B34 + side) * 4u);   /* 0x29C0F..0x29C1A */
}''')
with open(F, "a") as f:
    f.write(r'''

#define P4_10782A        0x0010782Au  /* 0x1552D: [side] byte, the palette row index */
#define P4_DESC_A84E0    0x000A84E0u  /* 0x241DA: [char] */
#define P4_DESC_C7758    0x000C7758u  /* 0x4037D */
#define P4_DESC_C9360    0x000C9360u  /* 0x45C73 */
#define P4_DESC_BB13C    0x000BB13Cu  /* 0x48A02 */


/* 0x15510 — record §P4.4. A stream dword outside E2 (0xD2B62 and the two
 * streams that jump to 0xD2B60, record P1 §P1.2). The side's own slot byte
 * 0x10782A (the character's palette row) through 0x29C08 into 0x9B08C; the
 * child from 0x9B07C with a5 = the record's +0x56 | 0x400, its +0x59 = 2 and
 * the record's +0x4B its pool index. */
void fighter_15510(u32 rec)
{
    u32 side = (u32)DSB(rec + 0x51u);                       /* 0x15516/0x15518 */
    u32 child;
    DSD(P4_9B08C) = fighter_29c08(side, (u32)DSB(DS_0010782A + side * 0x94u));   /* 0x1551B..0x1553B 0x29C08 */
    child = actor_spawn((const u32 *)(mem + P4_DESC_9B07C), 0u, 0u, 0u,
                        (u32)(u16)(DSW(rec + 0x56u) | 0x0400u));   /* 0x15540..0x15558 0x2AE14 */
    DSB(child + 0x59u) = 2u;                                /* 0x1555D */
    DSB(rec + 0x4Bu) = DSB(child + 0x56u);                  /* 0x15561/0x15564 */
}

/* 0x241A8 — record §P4.4. The side's slot (rec+0x51) and its character's
 * 0xA84E0 descriptor; the child's a5 = the record's +0x56 | 0x400 and the
 * record's +0x4B its pool index. */
void fighter_241a8(u32 rec)
{
    u32 side = (u32)DSB(rec + 0x51u);                       /* 0x241AE/0x241B0 */
    u32 slot = DSD(DS_001077A8 + side * 4u);                /* 0x241B3 */
    u32 child;
    if (slot == 0u) return;                                 /* 0x241BA/0x241BC */
    child = actor_spawn((const u32 *)(mem + DSD(P4_DESC_A84E0 + (u32)DSB(slot + 0x7Au) * 4u)),
                        0u, 0u, 0u,
                        (u32)(u16)(DSW(rec + 0x56u) | 0x0400u));   /* 0x241BE..0x241E1 0x2AE14 */
    DSB(rec + 0x4Bu) = DSB(child + 0x56u);                  /* 0x241E6/0x241E9 */
}

/* 0x40358 — record §P4.4. The record's +0x14 pointer; when non-zero a child
 * from 0xC7758 with a2 = -4, a4 = -10, a5 = the record's +0x56 | 0x400, the
 * held record and the record's side copied into it, and the record's +0x4B
 * its pool index. */
void fighter_40358(u32 rec)
{
    u32 held = DSD(rec + 0x14u);                            /* 0x4035E */
    u32 child;
    if (held == 0u) return;                                 /* 0x40362 */
    child = actor_spawn((const u32 *)(mem + P4_DESC_C7758), 0xFFFFFFFCu, 0u, 0xFFFFFFF6u,
                        (u32)(u16)(DSW(rec + 0x56u) | 0x0400u));   /* 0x40364..0x40382 0x2AE14 */
    DSD(child + 0x14u) = held;                              /* 0x40387/0x4038A */
    DSB(child + 0x51u) = DSB(rec + 0x51u);                  /* 0x4038D/0x40390 */
    DSB(rec + 0x4Bu) = DSB(child + 0x56u);                  /* 0x40393/0x40396 */
}

/* 0x45C54 — record §P4.4. The record's +0x14 pointer; when non-zero a child
 * from 0xC9360 with a5 = the record's +0x56 | 0x400, its +0x59 = 2, the held
 * record, and the voice 0x5F. */
void fighter_45c54(u32 rec)
{
    u32 held = DSD(rec + 0x14u);                            /* 0x45C5A */
    u32 child;
    if (held == 0u) return;                                 /* 0x45C5E */
    child = actor_spawn((const u32 *)(mem + P4_DESC_C9360), 0u, 0u, 0u,
                        (u32)(u16)(DSW(rec + 0x56u) | 0x0400u));   /* 0x45C60..0x45C78 0x2AE14 */
    DSB(child + 0x59u) = 2u;                                /* 0x45C80 */
    DSD(child + 0x14u) = held;                              /* 0x45C84 */
    sound_voice(0x5Fu);                                     /* 0x45C87/0x45C8C */
}

/* 0x489DC — record §P4.4. The record's +0x14 pointer; when non-zero and the
 * pointed record's +0x4B is zero, a child from 0xBB13C with a5 = the record's
 * +0x56 | 0x400, its +0x59 = 1, and the pointed record's +0x4B its pool
 * index. */
void fighter_489dc(u32 rec)
{
    u32 held = DSD(rec + 0x14u);                            /* 0x489E0 */
    u32 hrec;
    u32 child;
    if (held == 0u) return;                                 /* 0x489E3/0x489E5 */
    hrec = DSD(held);                                       /* 0x489E7 */
    if (DSB(hrec + 0x4Bu) != 0u) return;                    /* 0x489E9/0x489ED */
    child = actor_spawn((const u32 *)(mem + P4_DESC_BB13C), 0u, 0u, 0u,
                        (u32)(u16)(DSW(rec + 0x56u) | 0x0400u));   /* 0x489EF..0x48A07 0x2AE14 */
    DSB(child + 0x59u) = 1u;                                /* 0x48A0C */
    DSB(hrec + 0x4Bu) = DSB(child + 0x56u);                 /* 0x48A10/0x48A15 */
}

/* 0x400EC — record §P4.4. The other side's slot (rec+0x51 ^ 1, zero-extended);
 * when non-zero: its record on its character's 0xC90F8 stream at 2.0 (0x2BC30),
 * the voice 0xC75AA[char] (a zero-extended word), the voice 0x59, and
 * 0x104AE9 bit 2 set. */
void fighter_400ec(u32 rec)
{
    u32 other = (u32)DSB(rec + 0x51u) ^ 1u;                 /* 0x400EE..0x400F3 */
    u32 slot = DSD(DS_001077A8 + other * 4u);               /* 0x400F8 */
    u32 ch;
    if (slot == 0u) return;                                 /* 0x400FF/0x40101 */
    ch = (u32)DSB(slot + 0x7Au);                            /* 0x40103/0x40105 */
    actors_anim_begin(DSD(slot), DSD(P4_STREAMS_C90F8 + ch * 4u), 0x40000000u);   /* 0x40108..0x40116 */
    sound_voice((u32)DSW(P4_VOICES_C75AA + ch * 2u));       /* 0x4011B..0x4012D */
    sound_voice(0x59u);                                     /* 0x40132/0x40137 */
    DSB(P4_104AE9) |= 0x04u;                                /* 0x4013C */
}
''')
sub("port/src/game/fighter.h", "void fighter_243f8(u32 rec);\n",
    """void fighter_243f8(u32 rec);
u32  fighter_29c08(u32 side, u32 ch);
void fighter_15510(u32 rec);
void fighter_241a8(u32 rec);
void fighter_40358(u32 rec);
void fighter_45c54(u32 rec);
void fighter_489dc(u32 rec);
void fighter_400ec(u32 rec);
""")
A = "port/src/game/actors.c"
sub(A, "static void anim_code_243F8(u32 rec, u32 arg);\n",
    "static void anim_code_243F8(u32 rec, u32 arg);\n" + "".join(
        "static void anim_code_%s(u32 rec, u32 arg);\n" % a for a in
        ("15510", "241A8", "40358", "45C54", "489DC", "400EC")))
sub(A, "    fn_register(0x243F8u, (void (*)(void))anim_code_243F8);\n",
    """    fn_register(0x243F8u, (void (*)(void))anim_code_243F8);
    /* PORT: record 2026-10-03-reverse-p4-p5 §P4.4. The held-record spawns. */
    fn_register(0x15510u, (void (*)(void))anim_code_15510);
    fn_register(0x241A8u, (void (*)(void))anim_code_241A8);
    fn_register(0x40358u, (void (*)(void))anim_code_40358);
    fn_register(0x45C54u, (void (*)(void))anim_code_45C54);
    fn_register(0x489DCu, (void (*)(void))anim_code_489DC);
    fn_register(0x400ECu, (void (*)(void))anim_code_400EC);
""")
with open(A, "a") as f:
    f.write("".join("static void anim_code_%s(u32 rec, u32 arg)\n{\n    (void)arg;\n    fighter_%s(rec);\n}\n"
                    % (a, fn) for a, fn in (("15510", "15510"), ("241A8", "241a8"), ("40358", "40358"),
                                            ("45C54", "45c54"), ("489DC", "489dc"), ("400EC", "400ec"))))
D = "port/tests/diff_runner.c"
sub(D, "static const binding_t k_bindings[] = {\n", r'''static void b_15510(const u32 *r, u32 *eax)      { fighter_15510(r[R_EAX]); *eax = 0u; }
static void b_241a8(const u32 *r, u32 *eax)      { fighter_241a8(r[R_EAX]); *eax = 0u; }
static void b_40358(const u32 *r, u32 *eax)      { fighter_40358(r[R_EAX]); *eax = 0u; }
static void b_45c54(const u32 *r, u32 *eax)      { fighter_45c54(r[R_EAX]); *eax = 0u; }
static void b_489dc(const u32 *r, u32 *eax)      { fighter_489dc(r[R_EAX]); *eax = 0u; }
static void b_400ec(const u32 *r, u32 *eax)      { fighter_400ec(r[R_EAX]); *eax = 0u; }
static void m_15510(const u32 *r, u32 *eax)
{
    m_15510_at(r, 0x0009B080u, (u32)DSB(r[R_EAX] + 0x51u), (u32)(u16)(DSW(r[R_EAX] + 0x56u) | 0x0400u));
    *eax = 0u;
}
static void m_15510_side(const u32 *r, u32 *eax)
{
    m_15510_at(r, 0x0009B07Cu, (u32)DSB(r[R_EAX] + 0x51u) & 1u, (u32)(u16)(DSW(r[R_EAX] + 0x56u) | 0x0400u));
    *eax = 0u;
}
static void m_15510_pal(const u32 *r, u32 *eax)
{
    u32 rec = r[R_EAX], side = (u32)DSB(rec + 0x51u), child;
    DSD(0x0009B090u) = fighter_29c08(side, (u32)DSB(0x0010782Au + side * 0x94u));
    child = actor_spawn((const u32 *)(mem + 0x0009B07Cu), 0u, 0u, 0u,
                        (u32)(u16)(DSW(rec + 0x56u) | 0x0400u));
    DSB(child + 0x59u) = 2u;
    DSB(rec + 0x4Bu) = DSB(child + 0x56u);
    *eax = 0u;
}
static void m_15510_a5(const u32 *r, u32 *eax)
{
    m_15510_at(r, 0x0009B07Cu, (u32)DSB(r[R_EAX] + 0x51u), (u32)(u16)DSW(r[R_EAX] + 0x56u));
    *eax = 0u;
}
static void m_241a8(const u32 *r, u32 *eax)
{
    u32 rec = r[R_EAX], side = (u32)DSB(rec + 0x51u), slot = DSD(0x001077A8u + side * 4u), child;
    if (slot == 0u) return;
    child = actor_spawn((const u32 *)(mem + DSD(0x000A84E4u + (u32)DSB(slot + 0x7Au) * 4u)),
                        0u, 0u, 0u, (u32)(u16)(DSW(rec + 0x56u) | 0x0400u));
    DSB(rec + 0x4Bu) = DSB(child + 0x56u);
    *eax = 0u;
}
static void m_241a8_sext(const u32 *r, u32 *eax)
{
    u32 rec = r[R_EAX], side = (u32)(s32)(s8)DSB(rec + 0x51u), slot = DSD(0x001077A8u + side * 4u), child;
    if (slot == 0u) return;
    child = actor_spawn((const u32 *)(mem + DSD(0x000A84E0u + (u32)DSB(slot + 0x7Au) * 4u)),
                        0u, 0u, 0u, (u32)(u16)(DSW(rec + 0x56u) | 0x0400u));
    DSB(rec + 0x4Bu) = DSB(child + 0x56u);
    *eax = 0u;
}
static void m_40358(const u32 *r, u32 *eax)
{
    u32 rec = r[R_EAX], held = DSD(rec + 0x14u), child;
    if (held == 0u) return;
    child = actor_spawn((const u32 *)(mem + 0x000C7758u), 0xFFFFFFF8u, 0u, 0xFFFFFFF6u,
                        (u32)(u16)(DSW(rec + 0x56u) | 0x0400u));
    DSD(child + 0x14u) = held;
    DSB(child + 0x51u) = DSB(rec + 0x51u);
    DSB(rec + 0x4Bu) = DSB(child + 0x56u);
    *eax = 0u;
}
static void b_40358(const u32 *r, u32 *eax)      { fighter_40358(r[R_EAX]); *eax = 0u; }
static void m_45c54(const u32 *r, u32 *eax)
{
    u32 rec = r[R_EAX], held = DSD(rec + 0x14u), child;
    if (held == 0u) return;
    child = actor_spawn((const u32 *)(mem + 0x000C9360u), 0u, 0u, 0u,
                        (u32)(u16)(DSW(rec + 0x56u) | 0x0400u));
    DSB(child + 0x59u) = 2u;
    DSD(child + 0x14u) = held;
    sound_voice(0x60u);
    *eax = 0u;
}
static void b_45c54(const u32 *r, u32 *eax)      { fighter_45c54(r[R_EAX]); *eax = 0u; }
static void m_489dc(const u32 *r, u32 *eax)
{
    u32 rec = r[R_EAX], held = DSD(rec + 0x14u), hrec, child;
    if (held == 0u) return;
    hrec = DSD(held);
    if (DSB(hrec + 0x4Bu) != 0u) return;
    child = actor_spawn((const u32 *)(mem + 0x000BB13Cu), 0u, 0u, 0u,
                        (u32)(u16)(DSW(rec + 0x56u) | 0x0400u));
    DSB(child + 0x59u) = 2u;
    DSB(hrec + 0x4Bu) = DSB(child + 0x56u);
    *eax = 0u;
}
static void b_489dc(const u32 *r, u32 *eax)      { fighter_489dc(r[R_EAX]); *eax = 0u; }
static void m_400ec(const u32 *r, u32 *eax)
{
    u32 other = (u32)DSB(r[R_EAX] + 0x51u) ^ 1u, slot = DSD(0x001077A8u + other * 4u), ch;
    if (slot == 0u) return;
    ch = (u32)DSB(slot + 0x7Au);
    actors_anim_begin(DSD(slot), DSD(0x000C90F8u + ch * 4u), 0x40000000u);
    sound_voice((u32)DSW(0x000C75AAu + ch * 2u));
    sound_voice(0x5Au);
    DSB(0x00104AE9u) |= 0x04u;
    *eax = 0u;
}
static void m_400ec_side(const u32 *r, u32 *eax)
{
    u32 other = ((u32)DSB(r[R_EAX] + 0x51u) & 1u) ^ 1u, slot = DSD(0x001077A8u + other * 4u), ch;
    if (slot == 0u) return;
    ch = (u32)DSB(slot + 0x7Au);
    actors_anim_begin(DSD(slot), DSD(0x000C90F8u + ch * 4u), 0x40000000u);
    sound_voice((u32)DSW(0x000C75AAu + ch * 2u));
    sound_voice(0x59u);
    DSB(0x00104AE9u) |= 0x04u;
    *eax = 0u;
}
static void b_29c08(const u32 *r, u32 *eax)      { *eax = fighter_29c08(r[R_EAX], r[R_EDX]); }

''' + "static const binding_t k_bindings[] = {\n")
sub(D, '    { "fighter_243f8@slot",       m_243f8_slot,   0x00000000u },\n',
    '    { "fighter_243f8@slot",       m_243f8_slot,   0x00000000u },\n' + r'''    { "fighter_29c08",          b_29c08,         0x00000000u },
    { "fighter_15510",          b_15510,         0x00000000u },
    { "fighter_15510@mutant",   m_15510,         0x00000000u },
    { "fighter_15510@side",     m_15510_side,    0x00000000u },
    { "fighter_15510@pal",      m_15510_pal,     0x00000000u },
    { "fighter_15510@a5",       m_15510_a5,      0x00000000u },
    { "fighter_241a8",          b_241a8,         0x00000000u },
    { "fighter_241a8@mutant",   m_241a8,         0x00000000u },
    { "fighter_241a8@sext",     m_241a8_sext,    0x00000000u },
    { "fighter_40358",          b_40358,         0x00000000u },
    { "fighter_40358@mutant",   m_40358,         0x00000000u },
    { "fighter_45c54",          b_45c54,         0x00000000u },
    { "fighter_45c54@mutant",   m_45c54,         0x00000000u },
    { "fighter_489dc",          b_489dc,         0x00000000u },
    { "fighter_489dc@mutant",   m_489dc,         0x00000000u },
    { "fighter_400ec",          b_400ec,         0x00000000u },
    { "fighter_400ec@mutant",   m_400ec,         0x00000000u },
    { "fighter_400ec@side",     m_400ec_side,    0x00000000u },

''')
print("t3_port applied")
PY
cmake --build build 2>&1 | grep -E 'error|warning'; PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -1
for f in fighter_15510 fighter_241a8 fighter_40358 fighter_45c54 fighter_489dc fighter_400ec fighter_29c08; do
  python3 tools/diff_verify.py --image /tmp/pr_p45_img.bin --function $f --self-check | grep -E '^\| fighter'; done
```

Expected: `all checks passed` and the measured rows:

| fighter_15510 | 0x15510 | 3 | 1/1 | VERIFIED | 29C08 stub unverified, 2AE14 stub unverified |
| fighter_241a8 | 0x241A8 | 4 | 3/3 | VERIFIED | 2AE14 stub unverified |
| fighter_400ec | 0x400EC | 3 | 3/3 | VERIFIED | 2BC30 stub unverified, 2C3FC stub unverified |
| fighter_40358 | 0x40358 | 2 | 3/3 | VERIFIED | 2AE14 stub unverified |
| fighter_45c54 | 0x45C54 | 2 | 3/3 | VERIFIED | 2AE14 stub unverified, 2C3FC stub unverified |
| fighter_489dc | 0x489DC | 3 | 4/4 | VERIFIED | 2AE14 stub unverified |
| fighter_15510@mutant | 0x15510 | 3 | 1/1 | MISMATCH | 29C08 stub unverified, 2AE14 stub unverified |
| fighter_15510@side | 0x15510 | 3 | 1/1 | MISMATCH | 29C08 stub unverified, 2AE14 stub unverified |
| fighter_15510@pal | 0x15510 | 3 | 1/1 | MISMATCH | 29C08 stub unverified, 2AE14 stub unverified |
| fighter_15510@a5 | 0x15510 | 3 | 1/1 | MISMATCH | 29C08 stub unverified, 2AE14 stub unverified |
| fighter_241a8@mutant | 0x241A8 | 4 | 3/3 | MISMATCH | 2AE14 stub unverified |
| fighter_241a8@sext | 0x241A8 | 4 | 3/3 | MISMATCH | 2AE14 stub unverified |
| fighter_400ec@mutant | 0x400EC | 3 | 3/3 | MISMATCH | 2BC30 stub unverified, 2C3FC stub unverified |
| fighter_400ec@side | 0x400EC | 3 | 3/3 | MISMATCH | 2BC30 stub unverified, 2C3FC stub unverified |
| fighter_40358@mutant | 0x40358 | 2 | 3/3 | MISMATCH | 2AE14 stub unverified |
| fighter_45c54@mutant | 0x45C54 | 2 | 3/3 | MISMATCH | 2AE14 stub unverified, 2C3FC stub unverified |
| fighter_489dc@mutant | 0x489DC | 3 | 4/4 | MISMATCH | 2AE14 stub unverified |


- [ ] **Step 4: the E2 table** (as Task 2 Step 4). Expected `entry-triage: targets 275 unported, 220 ported; supplement 131 (22 unported, 0 stale); untrusted entries 30` (5 rows moved; `0x15510` is outside E2).

- [ ] **Step 5: the task gate.** Expected `Ran 163 tests`, `OK`, `diff-verify: 92/92 functions VERIFIED; 182/182 mutants detected; 1 named gaps; 12/75 rows with callees closed (17 have none). ...`, `Ran 44 tests`, `OK`, `entry-triage: targets 275 unported, 220 ported; supplement 131 (22 unported, 0 stale); untrusted entries 30`.

- [ ] **Step 6: every new unit assertion can fail.** The Task 2 Step 6 script plus its two mutations (the palette store's address and the 0x22AB8 table are Tasks 5; here the palette address of `0x15510`):

```
port/src/game/fighter.c: 1 FAIL, first: ['test_fight.c:46854: 0x9B08C holds the palette handle']
all checks passed
```

- [ ] **Step 7: commit.** `git add` the same file set; message `fighter: port the held-record spawns 0x15510 0x241A8 0x40358 0x45C54 0x489DC 0x400EC, seam 0x29C08; E2 table regenerated (track P batches 4+5, task 3)`.

---

### Task 4: the seven 0xD500 side-record targets `0x3427C 0x34308 0x3438C 0x34418 0x344A4 0x34530 0x345BC`

**Files:** as Task 2 Steps 1-6. **Interfaces:** produces the seven `void fighter_XXXX(u32 rec)` and `test_p45_side_anim`. Consumes Task 2's `P45_SPECS`; E3's `ANIM_BEGIN`/`VOICE`.

- [ ] **Step 1: the specs and the expectations first.**

```bash
python3 - <<'PY'
import pathlib


def sub(path, old, new):
    p = pathlib.Path(path)
    s = p.read_text()
    n = s.count(old)
    assert n == 1, "%s: %d occurrences of %r" % (path, n, old[:70])
    p.write_text(s.replace(old, new))


SPECS4 = r'''
# The seven 0xD500 side-record targets 0x3427C..0x345BC: s0 the own slot 0's record (E3_REC), s1
# slot 1's (E3_REC2), s2 side 0x80 (the slot at DS_001077B0 + 0x80 * 0x94 = 0x10C1B0, whose record
# is zero: a `&1` port reads E3_REC). The voice (when the function has one) runs before the stream.
def p45_own_anim(name, entry, stream, frame, voice, mutants=("@mutant", "@side")):
    return Spec(name, entry, [
        Case("s0", {"eax": E3_OUT, "edx": 0x1234, "ecx": 0}, {**SLOT_PTRS, E3_OUT + 0x51: b"\x00"}),
        Case("s1", {"eax": E3_OUT, "edx": 0x1234, "ecx": 0}, {**SLOT_PTRS, E3_OUT + 0x51: b"\x01"}),
        Case("s2", {"eax": E3_OUT, "edx": 0x1234, "ecx": 0}, {**SLOT_PTRS, E3_OUT + 0x51: b"\x80"}),
    ], calls=(ANIM_BEGIN, VOICE) if voice else (ANIM_BEGIN,), eax_mask=0, mutants=mutants)


p45_own_anim("fighter_3427c", 0x3427C, 0xE76B2, 0x40400000, 0x8C, ("@mutant", "@side", "@voice")),

p45_own_anim("fighter_34308", 0x34308, 0xE436A, 0x40400000, None),

p45_own_anim("fighter_3438c", 0x3438C, 0xED354, 0x3F800000, 0xA6),

p45_own_anim("fighter_34418", 0x34418, 0xEAF66, 0x40400000, 0x84),

p45_own_anim("fighter_344a4", 0x344A4, 0xD461C, 0x40400000, 0x86),

p45_own_anim("fighter_34530", 0x34530, 0xD299C, 0x40400000, 0x9C),

p45_own_anim("fighter_345bc", 0x345BC, 0xE0F62, 0x40400000, 0x9B),
'''
sub("tools/diff_verify.py", "]\n\n\nSPECS = [\n    Spec(\"rng_next\", 0x5D7DC, [\n",
    "]\n\n\nP45_SPECS += [" + SPECS4 + "]\n\n\nSPECS = [\n    Spec(\"rng_next\", 0x5D7DC, [\n")
print("t4_spec applied")
PY
python3 tools/diff_verify.py --image /tmp/pr_p45_img.bin --function fighter_3427c | grep -E '^\| fighter|unknown binding|^diff-verify'
```

Expected: `t4_spec applied` and `fighter_3427c`'s row MISMATCH with unknown bindings.

- [ ] **Step 2: the unit check (fails to build).**

```bash
python3 - <<'PY'
import pathlib


def sub(path, old, new):
    p = pathlib.Path(path)
    s = p.read_text()
    n = s.count(old)
    assert n == 1, "%s: %d occurrences of %r" % (path, n, old[:70])
    p.write_text(s.replace(old, new))


sub("port/tests/test_fight.c", "int test_p45_p4_spawns(void)    { return u6b_run(p45_check_p4_spawns); }\n",
    "int test_p45_p4_spawns(void)    { return u6b_run(p45_check_p4_spawns); }\n" + r'''
/* §P4.5: the seven 0xD500 side-record targets, each through its registration:
 * the side's own slot record starts a real stream (its +8 changes from the
 * sentinel), and the other slot's record keeps its +8 sentinel. */
static void p45_check_side_anim(void)
{
    static const u32 addr[7] = { 0x3427Cu, 0x34308u, 0x3438Cu, 0x34418u, 0x344A4u, 0x34530u, 0x345BCu };
    p45_anim_fn f;
    u32 k, side;
    for (k = 0; k < 7u; k++) {
        f = (p45_anim_fn)(void *)fn_resolve(addr[k]);
        CHECK(f != NULL, "the 0xD500 target is registered");
        if (f == NULL) return;
        for (side = 0; side < 2u; side++) {
            u32 own = side == 0u ? Z_R0 : Z_R1, oth = side == 0u ? Z_R1 : Z_R0;
            z_fseed();
            c4r_pool();
            DSB(Z_R0 + 0x51u) = (u8)side;
            DSD(own + 8u) = 0x08080808u;
            DSD(oth + 8u) = 0x18181818u;
            f(Z_R0, 0u);
            CHECK(DSD(own + 8u) != 0x08080808u, "the side's record started a stream");
            CHECK_EQ_INT((int)DSD(oth + 8u), 0x18181818);
        }
    }
}
''')
sub("port/tests/test.h", "    X(test_p45_p4_spawns) \\\n", "    X(test_p45_p4_spawns) \\\n    X(test_p45_side_anim) \\\n")
print("t4_test applied")
PY
cmake --build build 2>&1 | grep -E ' error' | sed 's#.*/port/#port/#'
```

Expected: one undeclared-identifier error for `p45_check_side_anim`.

- [ ] **Step 3: the port, the registrations, the bindings and the mutants.**

```bash
python3 - <<'PY'
import pathlib


def sub(path, old, new):
    p = pathlib.Path(path)
    s = p.read_text()
    n = s.count(old)
    assert n == 1, "%s: %d occurrences of %r" % (path, n, old[:70])
    p.write_text(s.replace(old, new))


F = "port/src/game/fighter.c"
with open(F, "a") as f:
    f.write(r'''

#define P4_STREAM_3427C  0x000E76B2u  /* 0x342EA */
#define P4_STREAM_34308  0x000E436Au  /* 0x3436D */
#define P4_STREAM_3438C  0x000ED354u  /* 0x343F1 */
#define P4_STREAM_34418  0x000EAF66u  /* 0x34486 */
#define P4_STREAM_344A4  0x000D461Cu  /* 0x34512 */
#define P4_STREAM_34530  0x000D299Cu  /* 0x3459E */
#define P4_STREAM_345BC  0x000E0F62u  /* 0x3462A */


/* 0x3427C — record §P4.5. The side's own slot record (DS_001077B0 +
 * (rec+0x51) * 0x94, the byte zero-extended) on 0xE76B2 at 3.0, after the
 * voice 0x8C. The raw builds the two slot pointers inline (no 0x33950 call). */
void fighter_3427c(u32 rec)
{
    u32 side = (u32)DSB(rec + 0x51u);                       /* 0x34280..0x34288 */
    u32 own = DS_001077B0 + side * 0x94u;                   /* 0x34299..0x342B1 */
    sound_voice(0x8Cu);                                     /* 0x342E5..0x342EF */
    actors_anim_begin(DSD(own), P4_STREAM_3427C, 0x40400000u);   /* 0x342F4..0x342FD */
}

/* 0x34308 — record §P4.5. The same slot record on 0xE436A at 3.0, no voice. */
void fighter_34308(u32 rec)
{
    u32 side = (u32)DSB(rec + 0x51u);                       /* 0x3430C..0x34314 */
    u32 own = DS_001077B0 + side * 0x94u;                   /* 0x34325..0x3433D */
    actors_anim_begin(DSD(own), P4_STREAM_34308, 0x40400000u);   /* 0x34376..0x3437F */
}

/* 0x3438C — record §P4.5. The same slot record on 0xED354 at 1.0, after the voice 0xA6. */
void fighter_3438c(u32 rec)
{
    u32 side = (u32)DSB(rec + 0x51u);                       /* 0x34390..0x34398 */
    u32 own = DS_001077B0 + side * 0x94u;                   /* 0x343A9..0x343C1 */
    actors_anim_begin(DSD(own), P4_STREAM_3438C, 0x3F800000u);   /* 0x343FE..0x34403 */
    sound_voice(0xA6u);                                     /* 0x34408..0x3440D */
}

/* 0x34418 — record §P4.5. The same slot record on 0xEAF66 at 3.0, after the voice 0x84. */
void fighter_34418(u32 rec)
{
    u32 side = (u32)DSB(rec + 0x51u);                       /* 0x3441C..0x34424 */
    u32 own = DS_001077B0 + side * 0x94u;                   /* 0x34435..0x3444D */
    sound_voice(0x84u);                                     /* 0x34481..0x3448B */
    actors_anim_begin(DSD(own), P4_STREAM_34418, 0x40400000u);   /* 0x34490..0x34499 */
}

/* 0x344A4 — record §P4.5. The same slot record on 0xD461C at 3.0, after the voice 0x86. */
void fighter_344a4(u32 rec)
{
    u32 side = (u32)DSB(rec + 0x51u);                       /* 0x344A8..0x344B0 */
    u32 own = DS_001077B0 + side * 0x94u;                   /* 0x344C1..0x344D9 */
    sound_voice(0x86u);                                     /* 0x3450D..0x34517 */
    actors_anim_begin(DSD(own), P4_STREAM_344A4, 0x40400000u);   /* 0x3451C..0x34525 */
}

/* 0x34530 — record §P4.5. The same slot record on 0xD299C at 3.0, after the voice 0x9C. */
void fighter_34530(u32 rec)
{
    u32 side = (u32)DSB(rec + 0x51u);                       /* 0x34534..0x3453C */
    u32 own = DS_001077B0 + side * 0x94u;                   /* 0x3454D..0x34565 */
    sound_voice(0x9Cu);                                     /* 0x34599..0x345A3 */
    actors_anim_begin(DSD(own), P4_STREAM_34530, 0x40400000u);   /* 0x345A8..0x345B1 */
}

/* 0x345BC — record §P4.5. The same slot record on 0xE0F62 at 3.0, after the voice 0x9B. */
void fighter_345bc(u32 rec)
{
    u32 side = (u32)DSB(rec + 0x51u);                       /* 0x345C0..0x345C8 */
    u32 own = DS_001077B0 + side * 0x94u;                   /* 0x345D9..0x345F1 */
    sound_voice(0x9Bu);                                     /* 0x34625..0x3462F */
    actors_anim_begin(DSD(own), P4_STREAM_345BC, 0x40400000u);   /* 0x34634..0x3463D */
}
''')
sub("port/src/game/fighter.h", "void fighter_400ec(u32 rec);\n",
    """void fighter_400ec(u32 rec);
void fighter_3427c(u32 rec);
void fighter_34308(u32 rec);
void fighter_3438c(u32 rec);
void fighter_34418(u32 rec);
void fighter_344a4(u32 rec);
void fighter_34530(u32 rec);
void fighter_345bc(u32 rec);
""")
A = "port/src/game/actors.c"
sub(A, "static void anim_code_400EC(u32 rec, u32 arg);\n",
    "static void anim_code_400EC(u32 rec, u32 arg);\n" + "".join(
        "static void anim_code_%s(u32 rec, u32 arg);\n" % a for a in
        ("3427C", "34308", "3438C", "34418", "344A4", "34530", "345BC")))
sub(A, "    fn_register(0x400ECu, (void (*)(void))anim_code_400EC);\n",
    """    fn_register(0x400ECu, (void (*)(void))anim_code_400EC);
    /* PORT: record 2026-10-03-reverse-p4-p5 §P4.5. The seven 0xD500 side-record targets. */
    fn_register(0x3427Cu, (void (*)(void))anim_code_3427C);
    fn_register(0x34308u, (void (*)(void))anim_code_34308);
    fn_register(0x3438Cu, (void (*)(void))anim_code_3438C);
    fn_register(0x34418u, (void (*)(void))anim_code_34418);
    fn_register(0x344A4u, (void (*)(void))anim_code_344A4);
    fn_register(0x34530u, (void (*)(void))anim_code_34530);
    fn_register(0x345BCu, (void (*)(void))anim_code_345BC);
""")
with open(A, "a") as f:
    f.write("".join("static void anim_code_%s(u32 rec, u32 arg)\n{\n    (void)arg;\n    fighter_%s(rec);\n}\n"
                    % (a, fn) for a, fn in (("3427C", "3427c"), ("34308", "34308"), ("3438C", "3438c"),
                                            ("34418", "34418"), ("344A4", "344a4"), ("34530", "34530"),
                                            ("345BC", "345bc"))))
D = "port/tests/diff_runner.c"
sub(D, "static const binding_t k_bindings[] = {\n", r'''/* The seven 0xD500 side-record targets: one wrong variant each (the stream, the index mask or the
 * voice), through a shared deliberately-wrong body. */
static void m45_own_anim(u32 rec, u32 stream, u32 frame, u32 voice, int has_voice, int side_mask)
{
    u32 side = (u32)DSB(rec + 0x51u);
    if (side_mask) side &= 1u;
    if (has_voice) sound_voice(voice);
    actors_anim_begin(DSD(0x001077B0u + side * 0x94u), stream, frame);
}

static void b_3427c(const u32 *r, u32 *eax)      { fighter_3427c(r[R_EAX]); *eax = 0u; }
static void b_34308(const u32 *r, u32 *eax)      { fighter_34308(r[R_EAX]); *eax = 0u; }
static void b_3438c(const u32 *r, u32 *eax)      { fighter_3438c(r[R_EAX]); *eax = 0u; }
static void b_34418(const u32 *r, u32 *eax)      { fighter_34418(r[R_EAX]); *eax = 0u; }
static void b_344a4(const u32 *r, u32 *eax)      { fighter_344a4(r[R_EAX]); *eax = 0u; }
static void b_34530(const u32 *r, u32 *eax)      { fighter_34530(r[R_EAX]); *eax = 0u; }
static void b_345bc(const u32 *r, u32 *eax)      { fighter_345bc(r[R_EAX]); *eax = 0u; }
static void m_3427c(const u32 *r, u32 *eax)      { m45_own_anim(r[R_EAX], 0x000E76B0u, 0x40400000u, 0x8Cu, 1, 0); *eax = 0u; }
static void m_3427c_side(const u32 *r, u32 *eax) { m45_own_anim(r[R_EAX], 0x000E76B2u, 0x40400000u, 0x8Cu, 1, 1); *eax = 0u; }
static void m_3427c_voice(const u32 *r, u32 *eax) { m45_own_anim(r[R_EAX], 0x000E76B2u, 0x40400000u, 0x8Du, 1, 0); *eax = 0u; }
static void m_34308(const u32 *r, u32 *eax)      { m45_own_anim(r[R_EAX], 0x000E4368u, 0x40400000u, 0u, 0, 0); *eax = 0u; }
static void m_34308_side(const u32 *r, u32 *eax) { m45_own_anim(r[R_EAX], 0x000E436Au, 0x40400000u, 0u, 0, 1); *eax = 0u; }
static void m_3438c(const u32 *r, u32 *eax)      { m45_own_anim(r[R_EAX], 0x000ED354u, 0x3F800000u, 0xA6u, 1, 0); *eax = 0u; }
static void m_3438c_side(const u32 *r, u32 *eax) { m45_own_anim(r[R_EAX], 0x000ED354u, 0x3F800000u, 0xA6u, 1, 1); *eax = 0u; }
static void m_34418(const u32 *r, u32 *eax)      { m45_own_anim(r[R_EAX], 0x000EAF66u, 0x40400000u, 0x85u, 1, 0); *eax = 0u; }
static void m_34418_side(const u32 *r, u32 *eax) { m45_own_anim(r[R_EAX], 0x000EAF66u, 0x40400000u, 0x85u, 1, 1); *eax = 0u; }
static void m_344a4(const u32 *r, u32 *eax)      { m45_own_anim(r[R_EAX], 0x000D4618u, 0x40400000u, 0x86u, 1, 0); *eax = 0u; }
static void m_344a4_side(const u32 *r, u32 *eax) { m45_own_anim(r[R_EAX], 0x000D461Cu, 0x40400000u, 0x86u, 1, 1); *eax = 0u; }
static void m_34530(const u32 *r, u32 *eax)      { m45_own_anim(r[R_EAX], 0x000D299Cu, 0x40400000u, 0x9Du, 1, 0); *eax = 0u; }
static void m_34530_side(const u32 *r, u32 *eax) { m45_own_anim(r[R_EAX], 0x000D299Cu, 0x40400000u, 0x9Cu, 1, 1); *eax = 0u; }
static void m_345bc(const u32 *r, u32 *eax)      { m45_own_anim(r[R_EAX], 0x000E0F62u, 0x3F800000u, 0x9Bu, 1, 0); *eax = 0u; }
static void m_345bc_side(const u32 *r, u32 *eax) { m45_own_anim(r[R_EAX], 0x000E0F62u, 0x40400000u, 0x9Bu, 1, 1); *eax = 0u; }

''' + "static const binding_t k_bindings[] = {\n")
sub(D, '    { "fighter_400ec@side",       m_400ec_side,   0x00000000u },\n',
    '    { "fighter_400ec@side",       m_400ec_side,   0x00000000u },\n' + r'''    { "fighter_3427c",          b_3427c,         0x00000000u },
    { "fighter_3427c@mutant",   m_3427c,         0x00000000u },
    { "fighter_3427c@side",     m_3427c_side,    0x00000000u },
    { "fighter_3427c@voice",    m_3427c_voice,   0x00000000u },
    { "fighter_34308",          b_34308,         0x00000000u },
    { "fighter_34308@mutant",   m_34308,         0x00000000u },
    { "fighter_34308@side",     m_34308_side,    0x00000000u },
    { "fighter_3438c",          b_3438c,         0x00000000u },
    { "fighter_3438c@mutant",   m_3438c,         0x00000000u },
    { "fighter_3438c@side",     m_3438c_side,    0x00000000u },
    { "fighter_34418",          b_34418,         0x00000000u },
    { "fighter_34418@mutant",   m_34418,         0x00000000u },
    { "fighter_34418@side",     m_34418_side,    0x00000000u },
    { "fighter_344a4",          b_344a4,         0x00000000u },
    { "fighter_344a4@mutant",   m_344a4,         0x00000000u },
    { "fighter_344a4@side",     m_344a4_side,    0x00000000u },
    { "fighter_34530",          b_34530,         0x00000000u },
    { "fighter_34530@mutant",   m_34530,         0x00000000u },
    { "fighter_34530@side",     m_34530_side,    0x00000000u },
    { "fighter_345bc",          b_345bc,         0x00000000u },
    { "fighter_345bc@mutant",   m_345bc,         0x00000000u },
    { "fighter_345bc@side",     m_345bc_side,    0x00000000u },

''')
print("t4_port applied")
PY
cmake --build build 2>&1 | grep -E 'error|warning'; PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -1
for f in fighter_3427c fighter_34308 fighter_3438c fighter_34418 fighter_344a4 fighter_34530 fighter_345bc; do
  python3 tools/diff_verify.py --image /tmp/pr_p45_img.bin --function $f --self-check | grep -E '^\| fighter'; done
```

Expected: `all checks passed` and the measured rows:

| fighter_3427c | 0x3427C | 3 | 1/1 | VERIFIED; reads outside the image: 0x10C1B0+4 | 2BC30 stub unverified, 2C3FC stub unverified |
| fighter_34308 | 0x34308 | 3 | 1/1 | VERIFIED; reads outside the image: 0x10C1B0+4 | 2BC30 stub unverified |
| fighter_3438c | 0x3438C | 3 | 1/1 | VERIFIED; reads outside the image: 0x10C1B0+4 | 2BC30 stub unverified, 2C3FC stub unverified |
| fighter_34418 | 0x34418 | 3 | 1/1 | VERIFIED; reads outside the image: 0x10C1B0+4 | 2BC30 stub unverified, 2C3FC stub unverified |
| fighter_344a4 | 0x344A4 | 3 | 1/1 | VERIFIED; reads outside the image: 0x10C1B0+4 | 2BC30 stub unverified, 2C3FC stub unverified |
| fighter_34530 | 0x34530 | 3 | 1/1 | VERIFIED; reads outside the image: 0x10C1B0+4 | 2BC30 stub unverified, 2C3FC stub unverified |
| fighter_345bc | 0x345BC | 3 | 1/1 | VERIFIED; reads outside the image: 0x10C1B0+4 | 2BC30 stub unverified, 2C3FC stub unverified |
| fighter_3427c@mutant | 0x3427C | 3 | 1/1 | MISMATCH; reads outside the image: 0x10C1B0+4 | 2BC30 stub unverified, 2C3FC stub unverified |
| fighter_3427c@side | 0x3427C | 3 | 1/1 | MISMATCH; reads outside the image: 0x10C1B0+4 | 2BC30 stub unverified, 2C3FC stub unverified |
| fighter_3427c@voice | 0x3427C | 3 | 1/1 | MISMATCH; reads outside the image: 0x10C1B0+4 | 2BC30 stub unverified, 2C3FC stub unverified |
| fighter_34308@mutant | 0x34308 | 3 | 1/1 | MISMATCH; reads outside the image: 0x10C1B0+4 | 2BC30 stub unverified |
| fighter_34308@side | 0x34308 | 3 | 1/1 | MISMATCH; reads outside the image: 0x10C1B0+4 | 2BC30 stub unverified |
| fighter_3438c@mutant | 0x3438C | 3 | 1/1 | MISMATCH; reads outside the image: 0x10C1B0+4 | 2BC30 stub unverified, 2C3FC stub unverified |
| fighter_3438c@side | 0x3438C | 3 | 1/1 | MISMATCH; reads outside the image: 0x10C1B0+4 | 2BC30 stub unverified, 2C3FC stub unverified |
| fighter_34418@mutant | 0x34418 | 3 | 1/1 | MISMATCH; reads outside the image: 0x10C1B0+4 | 2BC30 stub unverified, 2C3FC stub unverified |
| fighter_34418@side | 0x34418 | 3 | 1/1 | MISMATCH; reads outside the image: 0x10C1B0+4 | 2BC30 stub unverified, 2C3FC stub unverified |
| fighter_344a4@mutant | 0x344A4 | 3 | 1/1 | MISMATCH; reads outside the image: 0x10C1B0+4 | 2BC30 stub unverified, 2C3FC stub unverified |
| fighter_344a4@side | 0x344A4 | 3 | 1/1 | MISMATCH; reads outside the image: 0x10C1B0+4 | 2BC30 stub unverified, 2C3FC stub unverified |
| fighter_34530@mutant | 0x34530 | 3 | 1/1 | MISMATCH; reads outside the image: 0x10C1B0+4 | 2BC30 stub unverified, 2C3FC stub unverified |
| fighter_34530@side | 0x34530 | 3 | 1/1 | MISMATCH; reads outside the image: 0x10C1B0+4 | 2BC30 stub unverified, 2C3FC stub unverified |
| fighter_345bc@mutant | 0x345BC | 3 | 1/1 | MISMATCH; reads outside the image: 0x10C1B0+4 | 2BC30 stub unverified, 2C3FC stub unverified |
| fighter_345bc@side | 0x345BC | 3 | 1/1 | MISMATCH; reads outside the image: 0x10C1B0+4 | 2BC30 stub unverified, 2C3FC stub unverified |


- [ ] **Step 4: the E2 table.** Expected `entry-triage: targets 268 unported, 227 ported; supplement 131 (22 unported, 0 stale); untrusted entries 30`.
- [ ] **Step 5: the task gate.** Expected `diff-verify: 99/99 functions VERIFIED; 197/197 mutants detected; 1 named gaps; 12/82 rows with callees closed (17 have none). ...` and the same E2 line.
- [ ] **Step 6: the mutation proof.** The Task 2 script's `0x3427C` mutation: `port/src/game/fighter.c: 4 FAIL, first: ["test_fight.c:46817: the side's record started a stream"]` then `all checks passed`.
- [ ] **Step 7: commit.** Message `fighter: port the seven 0xD500 side-record targets 0x3427C..0x345BC; E2 table regenerated (track P batches 4+5, task 4)`.

---

### Task 5: `0x156E0 0x22AB8 0x37B70`

**Files:** as Task 2 Steps 1-6. **Interfaces:** produces the three functions and `test_p45_p5_records`. Consumes Task 2's `P45_SPECS`, `BIT15`, `ANIM_BEGIN`, `VOICE`, `SPAWN`.

- [ ] **Step 1: the specs and the expectations first.**

```bash
python3 - <<'PY'
import pathlib


def sub(path, old, new):
    p = pathlib.Path(path)
    s = p.read_text()
    n = s.count(old)
    assert n == 1, "%s: %d occurrences of %r" % (path, n, old[:70])
    p.write_text(s.replace(old, new))


SPECS5 = r'''
def p45_156e0(cid, r51, al, b6, b7, slot_rec, stream, extra=None):
    return Case(cid, {"eax": E3_OUT, "edx": 0x1234, "ecx": 0},
                {**P45_PTRS, **P45_CHARS, **P45_FAKE, E3_OUT + 0x51: bytes([r51]),
                 E3_OUT + 0x18: le32(0x18181818) + le32(0x1C1C1C1C),
                 E3_OUT + 0x56: b"\x23\x01", E3_OUT + 0x4B: b"\x4b",
                 0xC9783: bytes([0x11, 0x22, 0x33, b6, b7]), stream: le32(0x5A5A5A5A),
                 0xF0AFE: b"\xfe\xff",
                 slot_rec + 0x18: le32(0x22222222) + le32(0x33333333),
                 slot_rec + 0x56: b"\x07\x00", **(extra or {})}, {0x1A570: al})


Spec("fighter_156e0", 0x156E0, [
        p45_156e0("s0", 0x00, 1, 0x80, 0x01, E3_REC2, 0x9B038 + 12),
        p45_156e0("s1", 0x01, 0, 0x02, 0xFF, E3_REC, 0x9B038 + 20),
        p45_156e0("s2", 0x80, 1, 0x04, 0x03, E3_REC, 0x9B038 + 0x204),
    ], calls=(BIT15, ANIM_BEGIN, VOICE), eax_mask=0, mutants=("@mutant", "@side", "@signed", "@plus")),

Spec("fighter_22ab8", 0x22AB8, [
        Case("h0", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(0), E3_SLOT: le32(E3_OUT), E3_OUT + 0x51: b"\x00"}),
        Case("h1", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(E3_SLOT), E3_SLOT: le32(E3_OUT), E3_OUT + 0x51: b"\x00",
              E3_REC + 0x18: le32(0x18181818), E3_REC + 0x1C: le32(0x1C1C1C1C),
              E3_REC + 0x34: b"\x34\x34\x36\x36", E3_SLOT + 8: le32(0x08080808),
              0x104730: le32(0x000A0000), 0x104734: le32(0x000A0001)}, {0x1A570: 0}),
        Case("h2", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(E3_SLOT), E3_SLOT: le32(E3_OUT), E3_OUT + 0x51: b"\x01",
              E3_REC + 0x18: le32(0x18181818), E3_REC + 0x1C: le32(0x1C1C1C1C),
              E3_REC + 0x34: b"\x34\x34\x36\x36", E3_SLOT + 8: le32(0x08080808),
              0x104730: le32(0x000A0000), 0x104734: le32(0x000A0001)}, {0x1A570: 1}),
        Case("h3", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(E3_SLOT), E3_SLOT: le32(E3_OUT), E3_OUT + 0x51: b"\x80",
              E3_REC + 0x18: le32(0x18181818), E3_REC + 0x1C: le32(0x1C1C1C1C),
              E3_REC + 0x34: b"\x34\x34\x36\x36", E3_SLOT + 8: le32(0x08080808),
              0x104930: le32(0x000A0002), 0x104530: le32(0x000A0003)}, {0x1A570: 0}),
    ], calls=(BIT15,), eax_mask=0, mutants=("@mutant", "@side", "@sext", "@add", "@table")),

Spec("fighter_37b70", 0x37B70, [
        Case("r0", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(0), E3_SLOT + 0x7A: b"\x00"}),
        Case("r1", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(E3_SLOT), E3_REC + 0x28: b"\x00\x00", E3_REC + 0x56: b"\x23\x01",
              E3_SLOT + 0x7A: b"\x00", 0xBDC48: le32(0), 0xBDC64: b"\x00\x01"}),
        Case("r2", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(E3_SLOT), E3_REC + 0x28: b"\x00\x00", E3_REC + 0x56: b"\x23\x01",
              E3_SLOT + 0x7A: b"\x00", 0xBDC48: le32(0x000A0000), 0xBDC64: b"\x00\x01",
              E3_OUT + 0x56: b"\x07\x00", E3_OUT + 0x59: b"\x59"}, {0x2AE14: E3_OUT}),
        Case("r3", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(E3_SLOT), E3_REC + 0x28: b"\x00\x40", E3_REC + 0x56: b"\x23\x01",
              E3_SLOT + 0x7A: b"\x05", 0xBDC48 + 20: le32(0x000A0005), 0xBDC64 + 10: b"\x00\x01",
              E3_OUT + 0x56: b"\x07\x00", E3_OUT + 0x59: b"\x59"}, {0x2AE14: E3_OUT}),
        Case("r4", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(E3_SLOT), E3_SLOT: le32(E3_OUT), E3_REC + 0x28: b"\x00\x00",
              E3_REC + 0x56: b"\x23\x01", E3_SLOT + 0x7A: b"\x01", 0xBDC48 + 4: le32(0x000A0001),
              E3_OUT + 0x4B: b"\x4b", E3_OUT + 0x56: b"\x07\x00", E3_OUT + 0x59: b"\x59"},
             {0x2AE14: E3_OUT}),
        Case("r5", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(E3_SLOT), E3_SLOT: le32(E3_OUT), E3_REC + 0x28: b"\x00\x40",
              E3_REC + 0x56: b"\x23\x01", E3_SLOT + 0x7A: b"\x06", 0xBDC48 + 24: le32(0x000A0006),
              E3_OUT + 0x4B: b"\x4b", E3_OUT + 0x56: b"\x07\x00", E3_OUT + 0x59: b"\x59"},
             {0x2AE14: E3_OUT}),
        Case("r6", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(E3_SLOT), E3_SLOT: le32(E3_OUT), E3_REC + 0x28: b"\x00\x00",
              E3_REC + 0x56: b"\x23\x01", E3_SLOT + 0x7A: b"\x80", 0xBDC48 + 0x200: le32(0x000A0080),
              0xBDA48: le32(0x000AFF80), E3_OUT + 0x4B: b"\x4b", E3_OUT + 0x56: b"\x07\x00",
              E3_OUT + 0x59: b"\x59"}, {0x2AE14: E3_OUT}),
    ], calls=(SPAWN,), eax_mask=0, mutants=("@mutant", "@path", "@neg", "@hrec", "@sext")),
'''
sub("tools/diff_verify.py", "]\n\n\nSPECS = [\n    Spec(\"rng_next\", 0x5D7DC, [\n",
    "]\n\n\nP45_SPECS += [" + SPECS5 + "]\n\n\nSPECS = [\n    Spec(\"rng_next\", 0x5D7DC, [\n")
print("t5_spec applied")
PY
python3 tools/diff_verify.py --image /tmp/pr_p45_img.bin --function fighter_156e0 | grep -E '^\| fighter|unknown binding|^diff-verify'
```

Expected: `t5_spec applied` and `fighter_156e0`'s row MISMATCH with unknown bindings.

- [ ] **Step 2: the unit check (fails to build).**

```bash
python3 - <<'PY'
import pathlib


def sub(path, old, new):
    p = pathlib.Path(path)
    s = p.read_text()
    n = s.count(old)
    assert n == 1, "%s: %d occurrences of %r" % (path, n, old[:70])
    p.write_text(s.replace(old, new))


sub("port/tests/test_fight.c", "int test_p45_side_anim(void)    { return u6b_run(p45_check_side_anim); }\n",
    "int test_p45_side_anim(void)    { return u6b_run(p45_check_side_anim); }\n" + r'''
/* §P5.3: 0x156E0's shifted records and 0x22AB8's table (Task 5). */
static void p45_check_p5_records(void)
{
    p45_anim_fn f;
    f = (p45_anim_fn)(void *)fn_resolve(0x156E0u);
    CHECK(f != NULL, "0x156E0 is registered");
    if (f != NULL) {
        z_fseed();
        DSB(Z_R0 + 0x51u) = 0u;
        DSD(Z_R0 + 0x18u) = 0x00001818u;
        DSD(Z_R0 + 0x1Cu) = 0x00001C1Cu;
        DSB(0x000C9786u) = 0x80u;            /* the dword 0xC9783's high byte (-128) */
        DSB(0x000C9787u) = 0x01u;            /* the dword 0xC9784's high byte (1) */
        DSD(Z_R1 + 0x18u) = 0x22222222u;
        DSD(Z_R1 + 0x1Cu) = 0x33333333u;
        f(Z_R0, 0u);
        /* AL set (side 0's actor bit 15 clear) subtracts -128 * 0x40 = -0x2000 */
        CHECK_EQ_INT((int)DSD(Z_R1 + 0x18u), (int)(0x00001818u + 0x2000u));
        CHECK_EQ_INT((int)DSD(Z_R1 + 0x1Cu), (int)(0x00001C1Cu + 0x40u));
        CHECK_EQ_INT((int)DSB(Z_R0 + 0x4Bu), (int)DSB(Z_R1 + 0x56u));
        CHECK_EQ_INT((int)DSB(0x000F0AFFu), 0);
    }

    f = (p45_anim_fn)(void *)fn_resolve(0x22AB8u);
    CHECK(f != NULL, "0x22AB8 is registered");
    if (f != NULL) {
        z_fseed();
        DSD(Z_R0 + 0x14u) = Z_S0;
        DSD(Z_S0) = Z_R1;
        DSB(Z_R1 + 0x51u) = 1u;
        DSD(Z_R0 + 0x18u) = 0x00001818u;
        DSD(Z_R0 + 0x1Cu) = 0x00001C1Cu;
        DSW(Z_R0 + 0x34u) = 0x3434u;
        DSW(Z_R0 + 0x36u) = 0x3636u;
        DSD(Z_S0 + 8u) = 0x08080808u;
        DSD(0x00104734u) = 0x000A2222u;
        f(Z_R0, 0u);
        CHECK_EQ_INT((int)DSD(Z_S0 + 8u), 0x000A2222);
        CHECK_EQ_INT((int)DSD(Z_R0 + 0x1Cu), (int)(0x00001C1Cu + 0x1A00u));
        /* the plain seed's actor word 0x0F35 has bit 15 clear: AL set, -0x400 and 0xFF6A */
        CHECK_EQ_INT((int)DSD(Z_R0 + 0x18u), (int)(0x00001818u - 0x400u));
        CHECK_EQ_INT((int)DSW(Z_R0 + 0x34u), 0xFF6A);
        CHECK_EQ_INT((int)DSW(Z_R0 + 0x36u), 0xFFF0);
    }
}
''')
sub("port/tests/test.h", "    X(test_p45_side_anim) \\\n", "    X(test_p45_side_anim) \\\n    X(test_p45_p5_records) \\\n")
print("t5_test applied")
PY
cmake --build build 2>&1 | grep -E ' error' | sed 's#.*/port/#port/#'
```

Expected: one undeclared-identifier error for `p45_check_p5_records`.

- [ ] **Step 3: the port, the registrations, the bindings and the mutants.** Append the functions to `fighter.c`, their prototypes to `fighter.h`, the wrappers and registrations to `actors.c` (after `0x345BC`), and the bindings and rows to `diff_runner.c` (after `fighter_345bc@side`):

```
#define P5_DESC_BB2B8    0x000BB2B8u  /* 0x3D368 */
#define P5_DESC_BB2E0    0x000BB2E0u  /* 0x3DA94 */
#define P5_DESC_BB2CC    0x000BB2CCu  /* 0x3DC00/0x3DCB0 */
#define P5_C7780         0x000C7780u  /* 0x403D2: [char] word */
#define P5_C778C         0x000C778Cu  /* 0x40404: [char] dword, sar 16 */
#define P5_DESC_C776C    0x000C776Cu  /* 0x4040B */
#define P5_DESC_C77D8    0x000C77D8u  /* 0x40FF1 */
#define P5_DESC_C94F8    0x000C94F8u  /* 0x48A87 */
#define P5_1014F4        0x001014F4u  /* 0x48A25/0x48A8C/0x48A9A */
#define P5_104730        0x00104730u  /* 0x22AD0: [side] record pointer */


/* 0x156E0 — record §P5.3. The other side's slot (rec+0x51 ^ 1); when non-zero,
 * its record's +0x18 shifted by the signed byte 0xC9786 * 0x40 (the raw's
 * `mov eax,[0xC9783]; sar eax,0x18`, whose argument is the record's own side:
 * 0x1A570(rec+0x51)) and its +0x1C by the signed byte 0xC9787 * 0x40; then
 * that record on its character's 0x9B038 stream at 0.0 (0x2BC30), the record's
 * +0x4B its pool index, 0xF0AFE = 0, 0xF0AFF = the side, and the voice 0xD6. */
void fighter_156e0(u32 rec)
{
    u32 other = (u32)DSB(rec + 0x51u) ^ 1u;                 /* 0x156E6..0x156EB */
    u32 slot = DSD(DS_001077A8 + other * 4u);               /* 0x156F0 */
    u32 srec;
    if (slot == 0u) return;                                 /* 0x156F7/0x156F9 */
    srec = DSD(slot);                                       /* 0x156FF..0x15704 (0x1A570's rec) */
    if (fighter_actor_bit15_clear((u32)DSB(rec + 0x51u)) != 0) {   /* 0x15704..0x1570B 0x1A570 */
        DSD(srec + 0x18u) = DSD(rec + 0x18u) - (u32)((s32)DSD(P5_C9783) >> 24) * 0x40u;   /* 0x1570D..0x1571F */
    } else {
        DSD(srec + 0x18u) = DSD(rec + 0x18u) + (u32)((s32)DSD(P5_C9783) >> 24) * 0x40u;   /* 0x15724..0x15736 */
    }
    DSD(srec + 0x1Cu) = DSD(rec + 0x1Cu) + (u32)((s32)DSD(P5_C9784) >> 24) * 0x40u;       /* 0x15739..0x1574B */
    actors_anim_begin(srec, DSD(P5_STREAMS_9B038 + (u32)DSB(slot + 0x7Au) * 4u), 0u);     /* 0x1574E..0x1575E */
    DSB(rec + 0x4Bu) = DSB(srec + 0x56u);                   /* 0x15763..0x15768 */
    DSB(0xF0AFEu) = 0u;                                     /* 0x1576B/0x1576D */
    DSB(0xF0AFFu) = DSB(rec + 0x51u);                       /* 0x15773/0x15776 */
    sound_voice(0xD6u);                                     /* 0x1577B/0x15780 */
}

/* 0x22AB8 — record §P5.3. The record's +0x14 pointer; when non-zero its +8
 * is the 0x104730 table entry of the pointed record's side, the record's
 * +0x1C += 0x1A00, and by 0x1A570(that side): the record's word +0x34 =
 * 0xFF6A with its +0x18 -= 0x400, else word +0x34 = 0x96 with +0x18 += 0x400;
 * either way word +0x36 = 0xFFF0. */
void fighter_22ab8(u32 rec)
{
    u32 held = DSD(rec + 0x14u);                            /* 0x22ABE */
    u32 hrec;
    if (held == 0u) return;                                 /* 0x22AC1/0x22AC3 */
    hrec = DSD(held);                                       /* 0x22AC5 */
    DSD(held + 8u) = DSD(P5_104730 + (u32)DSB(hrec + 0x51u) * 4u);   /* 0x22AC7..0x22AD7 */
    DSD(rec + 0x1Cu) += 0x1A00u;                            /* 0x22ADA */
    if (fighter_actor_bit15_clear((u32)DSB(hrec + 0x51u)) != 0) {   /* 0x22AE1..0x22AF2 */
        DSW(rec + 0x34u) = 0xFF6Au;                         /* 0x22AF7 */
        DSD(rec + 0x18u) -= 0x400u;                         /* 0x22AF4..0x22B03 */
    } else {
        DSW(rec + 0x34u) = 0x0096u;                         /* 0x22B0B */
        DSD(rec + 0x18u) += 0x400u;                         /* 0x22B08..0x22B17 */
    }
    DSW(rec + 0x36u) = 0xFFF0u;                             /* 0x22B1A */
}

/* 0x37B70 — record §P5.3. The record's +0x14 pointer; when non-zero, the
 * word 0xBDC64[its character] (negated, 16-bit, when the record's +0x28 bit
 * 14 is set, and the spawn flag a5 = 0x4000) and its character's 0xBDC48
 * descriptor; when the descriptor is non-zero and the character is 0 or 5 the
 * child takes the word as a2, otherwise a2 = 0 and the pointed record's +0x4B
 * takes the child's pool index; either way a5 = the record's +0x56 | 0x400 and
 * the child's +0x59 = 1. */
void fighter_37b70(u32 rec)
{
    u32 held = DSD(rec + 0x14u);                            /* 0x37B77 */
    u32 hrec, desc, child;
    u32 ch;
    s32 di;
    u32 a5;
    if (held == 0u) return;                                 /* 0x37B7A/0x37B7C */
    ch = (u32)DSB(held + 0x7Au);                            /* 0x37B82..0x37B8B */
    di = (s32)(s16)DSW(P5_BDC64 + ch * 2u);                 /* 0x37B92..0x37B97 */
    if ((DSW(rec + 0x28u) & 0x4000u) != 0u) {               /* 0x37B82..0x37B90 */
        di = (s32)(s16)(u16)(0u - (u32)(u16)di);            /* 0x37BA4 */
        a5 = 0x4000u;                                       /* 0x37B9F */
    } else {
        a5 = 0u;                                            /* 0x37BAB */
    }
    desc = DSD(P5_DESC_BDC48 + ch * 4u);                    /* 0x37BB5..0x37BBA */
    if (desc == 0u) return;                                 /* 0x37BC1/0x37BC3 */
    if (ch == 0u || ch == 5u) {                             /* 0x37BC8..0x37BCF `jbe`/`jne` */
        child = actor_spawn((const u32 *)(mem + desc), (u32)di, 0u, 0u,
                            (u32)(u16)(DSW(rec + 0x56u) | 0x0400u) | a5);   /* 0x37BD1..0x37BEC */
    } else {
        child = actor_spawn((const u32 *)(mem + desc), 0u, 0u, 0u,
                            (u32)(u16)(DSW(rec + 0x56u) | 0x0400u) | a5);   /* 0x37BF3..0x37C0D */
        hrec = DSD(held);                                   /* 0x37C12 */
        DSB(hrec + 0x4Bu) = DSB(child + 0x56u);             /* 0x37C14/0x37C17 */
    }
    DSB(child + 0x59u) = 1u;                                /* 0x37C1A */
}


static void b_156e0(const u32 *r, u32 *eax)      { fighter_156e0(r[R_EAX]); *eax = 0u; }
static void b_22ab8(const u32 *r, u32 *eax)      { fighter_22ab8(r[R_EAX]); *eax = 0u; }
static void b_37b70(const u32 *r, u32 *eax)      { fighter_37b70(r[R_EAX]); *eax = 0u; }
static void m_156e0(const u32 *r, u32 *eax)      { m_156e0_at(r, 0, 0, 0, 0xD5u); *eax = 0u; }
static void m_156e0_side(const u32 *r, u32 *eax) { m_156e0_at(r, 1, 0, 0, 0xD6u); *eax = 0u; }
static void m_156e0_signed(const u32 *r, u32 *eax) { m_156e0_at(r, 0, 1, 0, 0xD6u); *eax = 0u; }
static void m_156e0_plus(const u32 *r, u32 *eax) { m_156e0_at(r, 0, 0, 1, 0xD6u); *eax = 0u; }
static void m_22ab8(const u32 *r, u32 *eax)
{
    u32 rec = r[R_EAX], held = DSD(rec + 0x14u), hrec;
    if (held == 0u) return;
    hrec = DSD(held);
    DSD(held + 8u) = DSD(0x00104730u + (u32)DSB(hrec + 0x51u) * 4u);
    DSD(rec + 0x1Cu) += 0x1A00u;
    if (DSB(0x001077B0u + (u32)DSB(rec + 0x51u) * 0x94u) == 0u) {
        DSW(rec + 0x34u) = 0xFF6Au; DSD(rec + 0x18u) -= 0x400u;
    } else {
        DSW(rec + 0x34u) = 0x0097u; DSD(rec + 0x18u) += 0x400u;
    }
    DSW(rec + 0x36u) = 0xFFF0u;
    *eax = 0u;
}
static void m_22ab8_side(const u32 *r, u32 *eax)
{
    u32 rec = r[R_EAX], held = DSD(rec + 0x14u), hrec;
    if (held == 0u) return;
    hrec = DSD(held);
    DSD(held + 8u) = DSD(0x00104730u + (u32)DSB(rec + 0x51u) * 4u);
    DSD(rec + 0x1Cu) += 0x1A00u;
    if (fighter_actor_bit15_clear((u32)DSB(hrec + 0x51u)) != 0) {
        DSW(rec + 0x34u) = 0xFF6Au; DSD(rec + 0x18u) -= 0x400u;
    } else {
        DSW(rec + 0x34u) = 0x0096u; DSD(rec + 0x18u) += 0x400u;
    }
    DSW(rec + 0x36u) = 0xFFF0u;
    *eax = 0u;
}
static void m_22ab8_sext(const u32 *r, u32 *eax)
{
    u32 rec = r[R_EAX], held = DSD(rec + 0x14u), hrec;
    if (held == 0u) return;
    hrec = DSD(held);
    DSD(held + 8u) = DSD(0x00104730u + (u32)(s32)(s8)DSB(hrec + 0x51u) * 4u);
    DSD(rec + 0x1Cu) += 0x1A00u;
    if (fighter_actor_bit15_clear((u32)DSB(hrec + 0x51u)) != 0) {
        DSW(rec + 0x34u) = 0xFF6Au; DSD(rec + 0x18u) -= 0x400u;
    } else {
        DSW(rec + 0x34u) = 0x0096u; DSD(rec + 0x18u) += 0x400u;
    }
    DSW(rec + 0x36u) = 0xFFF0u;
    *eax = 0u;
}
static void m_22ab8_add(const u32 *r, u32 *eax)
{
    u32 rec = r[R_EAX], held = DSD(rec + 0x14u), hrec;
    if (held == 0u) return;
    hrec = DSD(held);
    DSD(held + 8u) = DSD(0x00104730u + (u32)DSB(hrec + 0x51u) * 4u);
    DSD(rec + 0x1Cu) += 0x1A00u;
    if (DSB(0x001077B0u + (u32)DSB(rec + 0x51u) * 0x94u) == 0u) {
        DSW(rec + 0x34u) = 0xFF6Au; DSD(rec + 0x18u) += 0x400u;
    } else {
        DSW(rec + 0x34u) = 0x0096u; DSD(rec + 0x18u) -= 0x400u;
    }
    DSW(rec + 0x36u) = 0xFFF0u;
    *eax = 0u;
}
static void m_22ab8_table(const u32 *r, u32 *eax)
{
    u32 rec = r[R_EAX], held = DSD(rec + 0x14u);
    if (held == 0u) return;
    DSD(held + 8u) = DSD(0x00104734u);
    DSD(rec + 0x1Cu) += 0x1A00u;
    if (DSB(0x001077B0u + (u32)DSB(rec + 0x51u) * 0x94u) == 0u) {
        DSW(rec + 0x34u) = 0xFF6Au; DSD(rec + 0x18u) -= 0x400u;
    } else {
        DSW(rec + 0x34u) = 0x0096u; DSD(rec + 0x18u) += 0x400u;
    }
    DSW(rec + 0x36u) = 0xFFF0u;
    *eax = 0u;
}
static void m_37b70(const u32 *r, u32 *eax)      { m_37b70_at(r, 0, 0, 0, 4u); *eax = 0u; }
static void m_37b70_path(const u32 *r, u32 *eax) { m_37b70_at(r, 1, 0, 0, 0); *eax = 0u; }
static void m_37b70_neg(const u32 *r, u32 *eax)  { m_37b70_at(r, 0, 1, 0, 0); *eax = 0u; }
static void m_37b70_hrec(const u32 *r, u32 *eax) { m_37b70_at(r, 0, 0, 1, 0); *eax = 0u; }
static void m_37b70_sext(const u32 *r, u32 *eax)
{
    u32 rec = r[R_EAX], held = DSD(rec + 0x14u), desc, child;
    u32 ch;
    s32 di;
    if (held == 0u) return;
    ch = (u32)(s32)(s8)DSB(held + 0x7Au);
    di = (s32)(s16)DSW(0x000BDC64u + ch * 2u);
    if ((DSW(rec + 0x28u) & 0x4000u) != 0u)
        di = (s32)(s16)(u16)(0u - (u32)(u16)di);
    desc = DSD(0x000BDC48u + ch * 4u);
    if (desc == 0u) return;
    child = actor_spawn((const u32 *)(mem + desc), (u32)di, 0u, 0u,
                        (u32)(u16)(DSW(rec + 0x56u) | 0x0400u));
    DSB(child + 0x59u) = 1u;
    *eax = 0u;
}


    { "fighter_156e0",          b_156e0,         0x00000000u },
    { "fighter_156e0@mutant",   m_156e0,         0x00000000u },
    { "fighter_156e0@side",     m_156e0_side,    0x00000000u },
    { "fighter_156e0@signed",   m_156e0_signed,  0x00000000u },
    { "fighter_156e0@plus",     m_156e0_plus,    0x00000000u },
    { "fighter_22ab8",          b_22ab8,         0x00000000u },
    { "fighter_22ab8@mutant",   m_22ab8,         0x00000000u },
    { "fighter_22ab8@side",     m_22ab8_side,    0x00000000u },
    { "fighter_22ab8@sext",     m_22ab8_sext,    0x00000000u },
    { "fighter_22ab8@add",      m_22ab8_add,     0x00000000u },
    { "fighter_22ab8@table",    m_22ab8_table,   0x00000000u },
    { "fighter_37b70",          b_37b70,         0x00000000u },
    { "fighter_37b70@mutant",   m_37b70,         0x00000000u },
    { "fighter_37b70@path",     m_37b70_path,    0x00000000u },
    { "fighter_37b70@neg",      m_37b70_neg,     0x00000000u },
    { "fighter_37b70@hrec",     m_37b70_hrec,    0x00000000u },
    { "fighter_37b70@sext",     m_37b70_sext,    0x00000000u },

```

Then `cmake --build build 2>&1 | grep -E 'error|warning'; PR_ORACLE_REQUIRED=1 ./build/run_tests 2>&1 | tail -1` prints `all checks passed`, and the per-function `--self-check` loop prints the measured rows:

| fighter_156e0 | 0x156E0 | 3 | 6/6 | VERIFIED | 1A570 stub VERIFIED, 2BC30 stub unverified, 2C3FC stub unverified |
| fighter_22ab8 | 0x22AB8 | 4 | 6/6 | VERIFIED | 1A570 stub VERIFIED |
| fighter_37b70 | 0x37B70 | 7 | 11/11 | VERIFIED | 2AE14 stub unverified |
| fighter_156e0@mutant | 0x156E0 | 3 | 6/6 | MISMATCH | 1A570 stub VERIFIED, 2BC30 stub unverified, 2C3FC stub unverified |
| fighter_156e0@side | 0x156E0 | 3 | 6/6 | MISMATCH | 1A570 stub VERIFIED, 2BC30 stub unverified, 2C3FC stub unverified |
| fighter_156e0@signed | 0x156E0 | 3 | 6/6 | MISMATCH | 1A570 stub VERIFIED, 2BC30 stub unverified, 2C3FC stub unverified |
| fighter_156e0@plus | 0x156E0 | 3 | 6/6 | MISMATCH | 1A570 stub VERIFIED, 2BC30 stub unverified, 2C3FC stub unverified |
| fighter_22ab8@mutant | 0x22AB8 | 4 | 6/6 | MISMATCH | 1A570 stub VERIFIED |
| fighter_22ab8@side | 0x22AB8 | 4 | 6/6 | MISMATCH | 1A570 stub VERIFIED |
| fighter_22ab8@sext | 0x22AB8 | 4 | 6/6 | MISMATCH | 1A570 stub VERIFIED |
| fighter_22ab8@add | 0x22AB8 | 4 | 6/6 | MISMATCH | 1A570 stub VERIFIED |
| fighter_22ab8@table | 0x22AB8 | 4 | 6/6 | MISMATCH | 1A570 stub VERIFIED |
| fighter_37b70@mutant | 0x37B70 | 7 | 11/11 | MISMATCH | 2AE14 stub unverified |
| fighter_37b70@path | 0x37B70 | 7 | 11/11 | MISMATCH | 2AE14 stub unverified |
| fighter_37b70@neg | 0x37B70 | 7 | 11/11 | MISMATCH | 2AE14 stub unverified |
| fighter_37b70@hrec | 0x37B70 | 7 | 11/11 | MISMATCH | 2AE14 stub unverified |
| fighter_37b70@sext | 0x37B70 | 7 | 11/11 | MISMATCH | 2AE14 stub unverified |


- [ ] **Step 4: the E2 table.** Expected `entry-triage: targets 265 unported, 230 ported; supplement 131 (22 unported, 0 stale); untrusted entries 30`.
- [ ] **Step 5: the task gate.** Expected `diff-verify: 102/102 functions VERIFIED; 211/211 mutants detected; 1 named gaps; 13/85 rows with callees closed (17 have none). ...`.
- [ ] **Step 6: the mutation proofs.** The `0x156E0` signed-shift and `0x22AB8` table mutations:

```
port/src/game/fighter.c: 1 FAIL, first: ['test_fight.c:46928: -2024 != 14360']
port/src/game/fighter.c: 1 FAIL, first: ['test_fight.c:46979: 0 != 664098']
all checks passed
```

- [ ] **Step 7: commit.** Message `fighter: port the record-shifting targets 0x156E0 0x22AB8 0x37B70; E2 table regenerated (track P batches 4+5, task 5)`.

---

### Task 6: the spawn family `0x3D328 0x3DA50 0x3DB8C 0x3DC3C 0x403A0 0x40FBC 0x48A20`, and gp-u10-ending

**Files:** as Task 2 Steps 1-8 (Step 7 touches `port/tests/test_platform.c`, `Makefile`, `AGENTS.md`).

**Interfaces:** produces the seven functions and `test_p45_p5_spawns`; drops `gp-u10-ending`'s `0x3DA50` row.

- [ ] **Step 1: the specs and the expectations first.**

```bash
python3 - <<'PY'
import pathlib


def sub(path, old, new):
    p = pathlib.Path(path)
    s = p.read_text()
    n = s.count(old)
    assert n == 1, "%s: %d occurrences of %r" % (path, n, old[:70])
    p.write_text(s.replace(old, new))


SPECS6 = r'''
def p45_3db8c(name, entry, w14, w0, mutants=("@mutant", "@a2", "@w34", "@a4", "@side")):
    return Spec(name, entry, [
        Case("h0", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(0), E3_REC + 0x56: b"\x23\x01"}),
        Case("h1", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(E3_SLOT), E3_REC + 0x28: b"\x00\x40", E3_REC + 0x51: b"\x01",
              E3_REC + 0x18: le32(0x00001000), E3_REC + 0x1C: le32(0x00002000),
              E3_REC + 0x30: le32(0x00030000), E3_REC + 0x56: b"\x23\x01",
              E3_SLOT + 8: le32(0x08080808), E3_OUT + 0x14: le32(0x14141414), E3_OUT + 0x2E: b"\xfe\xff",
              E3_OUT + 0x34: b"\x34\x34\x36\x36", E3_OUT + 0x4E: b"\x4e", E3_OUT + 0x56: b"\x07\x00"},
             {0x2AE14: E3_OUT}),
        Case("h2", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(E3_SLOT), E3_REC + 0x28: b"\x00\x00", E3_REC + 0x51: b"\x00",
              E3_REC + 0x18: le32(0x00001000), E3_REC + 0x1C: le32(0x00002000),
              E3_REC + 0x30: le32(0x00030000), E3_REC + 0x56: b"\x23\x01",
              E3_SLOT + 8: le32(0x08080808), E3_OUT + 0x14: le32(0x14141414), E3_OUT + 0x2E: b"\xfe\xff",
              E3_OUT + 0x34: b"\x34\x34\x36\x36", E3_OUT + 0x4E: b"\x4e", E3_OUT + 0x56: b"\x07\x00"},
             {0x2AE14: E3_OUT}),
        Case("h3", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(E3_SLOT), E3_REC + 0x28: b"\x00\x00", E3_REC + 0x51: b"\x01",
              E3_REC + 0x18: le32(0xFFFFFFF0), E3_REC + 0x1C: le32(0x12345678),
              E3_REC + 0x30: le32(0xFFFD8000), E3_REC + 0x56: b"\x23\x01",
              E3_SLOT + 8: le32(0x08080808), E3_REC2 + 0x14: le32(0x14141414),
              E3_REC2 + 0x2E: b"\x34\x12", E3_REC2 + 0x34: b"\x34\x34\x36\x36",
              E3_REC2 + 0x4E: b"\x4e", E3_REC2 + 0x56: b"\x07\x00"}, {0x2AE14: E3_REC2}),
    ], calls=(SPAWN,), eax_mask=0, mutants=mutants)


Spec("fighter_3d328", 0x3D328, [
        Case("h0", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(0), E3_REC + 0x56: b"\x23\x01"}),
        Case("h1", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(E3_SLOT), E3_REC + 0x28: b"\x00\x00", E3_REC + 0x51: b"\x00",
              E3_REC + 0x56: b"\x23\x01", E3_REC + 0x4B: b"\x4b", E3_OUT + 0x14: le32(0x14141414),
              E3_OUT + 0x2E: b"\xfe\xff", E3_OUT + 0x4E: b"\x4e", E3_OUT + 0x56: b"\x07\x00",
              E3_OUT + 0x60: b"\x60"}, {0x2AE14: E3_OUT}),
        Case("h2", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(E3_SLOT), E3_REC + 0x28: b"\x00\x40", E3_REC + 0x51: b"\x01",
              E3_REC + 0x56: b"\x23\x01", E3_REC + 0x4B: b"\x4b", E3_REC2 + 0x14: le32(0x14141414),
              E3_REC2 + 0x2E: b"\x34\x12", E3_REC2 + 0x4E: b"\x4e", E3_REC2 + 0x56: b"\x07\x00",
              E3_REC2 + 0x60: b"\x60"}, {0x2AE14: E3_REC2}),
        Case("h3", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(E3_SLOT), E3_REC + 0x28: b"\x00\x00", E3_REC + 0x51: b"\x01",
              E3_REC + 0x56: b"\x23\x01", E3_REC + 0x4B: b"\x4b", E3_REC2 + 0x14: le32(0x14141414),
              E3_REC2 + 0x2E: b"\x34\x12", E3_REC2 + 0x4E: b"\x4e", E3_REC2 + 0x56: b"\x07\x00",
              E3_REC2 + 0x60: b"\x60"}, {0x2AE14: E3_REC2}),
    ], calls=(SPAWN, VOICE), eax_mask=0, mutants=("@mutant", "@a2", "@a4", "@side", "@voice")),

Spec("fighter_3da50", 0x3DA50, [
        Case("h0", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(0), E3_REC + 0x56: b"\x23\x01"}),
        Case("h1", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(E3_SLOT), E3_REC + 0x28: b"\x00\x00", E3_REC + 0x51: b"\x00",
              E3_REC + 0x56: b"\x23\x01", E3_REC + 0x4B: b"\x4b", E3_OUT + 0x14: le32(0x14141414),
              E3_OUT + 0x2E: b"\xfe\xff", E3_OUT + 0x4E: b"\x4e", E3_OUT + 0x56: b"\x07\x00",
              E3_OUT + 0x59: b"\x59", E3_OUT + 0x60: b"\x60"}, {0x2AE14: E3_OUT}),
        Case("h2", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(E3_SLOT), E3_REC + 0x28: b"\x00\x40", E3_REC + 0x51: b"\x01",
              E3_REC + 0x56: b"\x23\x01", E3_REC + 0x4B: b"\x4b", E3_REC2 + 0x14: le32(0x14141414),
              E3_REC2 + 0x2E: b"\x34\x12", E3_REC2 + 0x4E: b"\x4e", E3_REC2 + 0x56: b"\x07\x00",
              E3_REC2 + 0x59: b"\x59", E3_REC2 + 0x60: b"\x60"}, {0x2AE14: E3_REC2}),
        Case("h3", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x14: le32(E3_SLOT), E3_REC + 0x28: b"\x00\x00", E3_REC + 0x51: b"\x01",
              E3_REC + 0x56: b"\x23\x01", E3_REC + 0x4B: b"\x4b", E3_REC2 + 0x14: le32(0x14141414),
              E3_REC2 + 0x2E: b"\x34\x12", E3_REC2 + 0x4E: b"\x4e", E3_REC2 + 0x56: b"\x07\x00",
              E3_REC2 + 0x59: b"\x59", E3_REC2 + 0x60: b"\x60"}, {0x2AE14: E3_REC2}),
    ], calls=(SPAWN, VOICE), eax_mask=0, mutants=("@mutant", "@a2", "@a4", "@side")),

p45_3db8c("fighter_3db8c", 0x3DB8C, b"\xe0\x00", b"\x20\xff"),

p45_3db8c("fighter_3dc3c", 0x3DC3C, b"\xa0\x01", b"\x60\xfe", ("@mutant", "@a2", "@w34", "@side")),

Spec("fighter_403a0", 0x403A0, [
        Case("h0", {"eax": E3_OUT, "edx": 0x1234, "ecx": 0},
             {E3_OUT + 0x14: le32(0), E3_OUT + 0x56: b"\x23\x01"}),
        Case("h1", {"eax": E3_OUT, "edx": 0x1234, "ecx": 0},
             {**P45_PTRS, 0x1077A8: le32(0), **P45_CHARS,
              E3_OUT + 0x14: le32(E3_SLOT), E3_OUT + 0x51: b"\x00", E3_OUT + 0x56: b"\x23\x01"}),
        Case("h2", {"eax": E3_OUT, "edx": 0x1234, "ecx": 0},
             {**P45_PTRS, **P45_CHARS, E3_OUT + 0x14: le32(E3_SLOT), E3_OUT + 0x51: b"\x00",
              E3_OUT + 0x59: b"\x59", E3_OUT + 0x56: b"\x23\x01", E3_REC2 + 0x28: b"\x00\x00",
              E3_REC2 + 0x56: b"\x23\x01", 0xC7780 + 6: b"\x00\x01", 0xC778C + 8: b"\x00\x02",
              E3_OUT + 0x14: le32(0x14141414), E3_OUT + 0x51: b"\x51", E3_OUT + 0x59: b"\x59"},
             {0x2AE14: E3_OUT}),
        Case("h3", {"eax": E3_OUT, "edx": 0x1234, "ecx": 0},
             {**P45_PTRS, **P45_CHARS, E3_OUT + 0x14: le32(E3_SLOT), E3_OUT + 0x51: b"\x01",
              E3_OUT + 0x59: b"\x59", E3_OUT + 0x56: b"\x23\x01", E3_REC + 0x28: b"\x00\x40",
              E3_REC + 0x56: b"\x23\x01", 0xC7780 + 10: b"\x00\x01", 0xC778C + 12: b"\x00\x80",
              E3_REC2 + 0x14: le32(0x14141414), E3_REC2 + 0x51: b"\x51"},
             {0x2AE14: E3_REC2}),
        Case("h4", {"eax": E3_OUT, "edx": 0x1234, "ecx": 0},
             {**P45_PTRS, **P45_CHARS, E3_OUT + 0x14: le32(E3_SLOT), E3_OUT + 0x51: b"\x00",
              E3_OUT + 0x59: b"\x00", E3_OUT + 0x56: b"\x23\x01", E3_REC2 + 0x28: b"\x00\x00",
              E3_REC2 + 0x56: b"\x23\x01", 0xC7780 + 6: b"\x00\x01", 0xC778C + 8: b"\x00\x02",
              E3_OUT + 0x14: le32(0x14141414), E3_OUT + 0x51: b"\x51", E3_OUT + 0x59: b"\x59"},
             {0x2AE14: E3_OUT}),
    ], calls=(SPAWN,), eax_mask=0, mutants=("@mutant", "@side", "@signed", "@neg", "@a5", "@r59")),

Spec("fighter_40fbc", 0x40FBC, [
        Case("s0", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x51: b"\x00", E3_REC + 0x56: b"\x23\x01", E3_REC + 0x20: le32(0x20202020),
              E3_REC + 0x24: le32(0x24242424), E3_OUT + 0x20: le32(0x30303030),
              E3_OUT + 0x24: le32(0x34343434), E3_OUT + 0x59: b"\x59"}, {0x1A570: 1, 0x2AE14: E3_OUT}),
        Case("s1", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x51: b"\x01", E3_REC + 0x56: b"\x23\x01", E3_REC + 0x20: le32(0x20202020),
              E3_REC + 0x24: le32(0x24242424), E3_REC2 + 0x20: le32(0x30303030),
              E3_REC2 + 0x24: le32(0x34343434), E3_REC2 + 0x59: b"\x59"}, {0x1A570: 0, 0x2AE14: E3_REC2}),
        Case("s2", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {E3_REC + 0x51: b"\x01", E3_REC + 0x56: b"\x23\x01", E3_REC + 0x20: le32(0x20202020),
              E3_REC + 0x24: le32(0x24242424), E3_REC2 + 0x20: le32(0x30303030),
              E3_REC2 + 0x24: le32(0x34343434), E3_REC2 + 0x59: b"\x59"}, {0x1A570: 1, 0x2AE14: E3_REC2}),
    ], calls=(BIT15, SPAWN, PALETTE), eax_mask=0, mutants=("@mutant", "@pal", "@a2")),

Spec("fighter_48a20", 0x48A20, [
        Case("w0", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {0x1014F4: le32(E3_OUT), E3_REC + 0x18: le32(0x18181818), E3_REC + 0x1C: le32(0x1C1C1C1C),
              E3_REC + 0x28: b"\x00\x00", E3_REC + 0x30: le32(0x00020000), E3_REC + 0x4B: b"\x00",
              E3_REC + 0x56: b"\x23\x01", E3_OUT + 0x56: b"\x07\x00"},
             {0x2AE14: E3_OUT}),
        Case("w1", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {0x1014F4: le32(E3_OUT), E3_REC + 0x18: le32(0x18181818), E3_REC + 0x1C: le32(0x1C1C1C1C),
              E3_REC + 0x28: b"\x00\x40", E3_REC + 0x30: le32(0xFFFD8000), E3_REC + 0x4B: b"\x01",
              E3_REC + 0x56: b"\x23\x01", E3_OUT + 0x68 + 0x4B: b"\x00", E3_OUT + 0x56: b"\x07\x00"},
             {0x2AE14: E3_OUT}),
        Case("w2", {"eax": E3_REC, "edx": 0x1234, "ecx": 0},
             {0x1014F4: le32(E3_OUT), E3_REC + 0x18: le32(0x18181818), E3_REC + 0x1C: le32(0x1C1C1C1C),
              E3_REC + 0x28: b"\x00\x00", E3_REC + 0x30: le32(0x00020000), E3_REC + 0x4B: b"\x01",
              E3_REC + 0x56: b"\x23\x01", E3_OUT + 0x68 + 0x4B: b"\x02", E3_OUT + 0xD0 + 0x4B: b"\x00",
              E3_OUT + 0x56: b"\x07\x00"}, {0x2AE14: E3_OUT}),
    ], calls=(SPAWN,), eax_mask=0, mutants=("@mutant", "@walk", "@a2")),
'''
sub("tools/diff_verify.py", "]\n\n\nSPECS = [\n    Spec(\"rng_next\", 0x5D7DC, [\n",
    "]\n\n\nP45_SPECS += [" + SPECS6 + "]\n\n\nSPECS = [\n    Spec(\"rng_next\", 0x5D7DC, [\n")
print("t6_spec applied")
PY
python3 tools/diff_verify.py --image /tmp/pr_p45_img.bin --function fighter_3d328 | grep -E '^\| fighter|unknown binding|^diff-verify'
```

Expected: `t6_spec applied` and `fighter_3d328`'s row MISMATCH with unknown bindings.

- [ ] **Step 2: the unit check (fails to build).**

```bash
python3 - <<'PY'
import pathlib


def sub(path, old, new):
    p = pathlib.Path(path)
    s = p.read_text()
    n = s.count(old)
    assert n == 1, "%s: %d occurrences of %r" % (path, n, old[:70])
    p.write_text(s.replace(old, new))


sub("port/tests/test_fight.c", "int test_p45_p5_records(void)   { return u6b_run(p45_check_p5_records); }\n",
    "int test_p45_p5_records(void)   { return u6b_run(p45_check_p5_records); }\n" + r'''
/* §P5.4/§P5.5: the spawn family and the +0x10 handler, through the real pool. */
static void p45_check_p5_spawns(void)
{
    static u32 before[0x80];
    p45_anim_fn f;
    u32 n, child, x, k;
    /* 0x37B70: char 1 (the second path) takes a2 = 0 and stores the held
     * record's +0x4B; char 0 (the first path) stores none. */
    for (k = 0; k < 2u; k++) {
        f = (p45_anim_fn)(void *)fn_resolve(0x37B70u);
        CHECK(f != NULL, "0x37B70 is registered");
        if (f == NULL) return;
        z_fseed();
        c4r_pool();
        DSD(Z_R0 + 0x14u) = Z_S0;
        DSD(Z_S0) = Z_R1;
        DSB(Z_R1 + 0x4Bu) = 0x4Bu;
        DSB(Z_S0 + 0x7Au) = (u8)k;              /* char 0 or 1 */
        DSB(Z_R0 + 0x56u) = 0x23u;
        DSB(Z_R0 + 0x57u) = 0x01u;
        DSB(Z_R0 + 0x4Bu) = 0x4Bu;
        n = u6b_list(before, 0x80u);
        f(Z_R0, 0u);
        child = p45_new_child(before, n, &x);
        CHECK_EQ_INT((int)x, 1);
        if (x == 1u) {
            CHECK_EQ_INT((int)DSB(child + 0x59u), 1);
            if (k == 1u) CHECK_EQ_INT((int)DSB(Z_R1 + 0x4Bu), (int)DSB(child + 0x56u));
            if (k == 0u) CHECK_EQ_INT((int)DSB(Z_R1 + 0x4Bu), 0x4B);
        }
    }
    /* The other spawn targets, one real child each. */
    {
        static const u32 addr[7] = { 0x3D328u, 0x3DA50u, 0x3DB8Cu, 0x3DC3Cu, 0x403A0u, 0x40FBCu, 0x48A20u };
        for (k = 0; k < 7u; k++) {
            f = (p45_anim_fn)(void *)fn_resolve(addr[k]);
            CHECK(f != NULL, "the spawn target is registered");
            if (f == NULL) return;
            z_fseed();
            c4r_pool();
            DSD(Z_R0 + 0x14u) = Z_S0;
            DSD(Z_R0 + 0x18u) = 0x00001818u;
            DSD(Z_R0 + 0x1Cu) = 0x00001C1Cu;
            DSD(Z_R0 + 0x30u) = 0x00030000u;
            DSB(Z_R0 + 0x51u) = 1u;
            DSB(Z_R0 + 0x56u) = 0x23u;
            DSB(Z_R0 + 0x57u) = 0x01u;
            DSB(Z_R0 + 0x4Bu) = 0x4Bu;
            DSD(Z_R0 + 0x20u) = 0x20202020u;
            DSD(Z_R0 + 0x24u) = 0x24242424u;
            if (addr[k] == 0x48A20u) {
                DSD(0x001014F4u) = Z_R1;
                DSB(Z_R0 + 0x4Bu) = 0u;
            }
            if (addr[k] == 0x403A0u) {
                DSD(Z_R0 + 0x59u) = 0x59u;
                DSB(Z_S0 + 0x7Au) = 3u;         /* the other slot's char */
            }
            n = u6b_list(before, 0x80u);
            f(Z_R0, 0u);
            child = p45_new_child(before, n, &x);
            CHECK_EQ_INT((int)x, 1);
            if (x != 1u) continue;
            if (addr[k] == 0x48A20u) {
                CHECK_EQ_INT((int)DSB(Z_R0 + 0x4Bu), (int)DSB(child + 0x56u));
            } else if (addr[k] == 0x40FBCu) {
                CHECK_EQ_INT((int)DSB(child + 0x59u), 2);
                CHECK_EQ_INT((int)DSD(child + 0x24u), 0x24242424);
                CHECK_EQ_INT((int)DSD(child + 0x20u), 0x20202020);
            } else if (addr[k] == 0x403A0u) {
                CHECK_EQ_INT((int)DSD(child + 0x14u), (int)Z_S0);
                CHECK_EQ_INT((int)DSB(child + 0x51u), 1);
                CHECK_EQ_INT((int)DSB(child + 0x59u), 0x58);
            } else if (addr[k] == 0x3D328u || addr[k] == 0x3DA50u) {
                CHECK_EQ_INT((int)DSD(child + 0x14u), (int)Z_S0);
                CHECK_EQ_INT((int)DSB(child + 0x4Eu), 1);
                CHECK_EQ_INT((int)DSB(child + 0x60u), 1);
            } else {                            /* 0x3DB8C/0x3DC3C */
                CHECK_EQ_INT((int)DSD(child + 0x14u), (int)Z_S0);
                CHECK_EQ_INT((int)DSB(child + 0x4Eu), 1);
                CHECK_EQ_INT((int)DSD(Z_S0 + 8u), (int)child);
                CHECK(DSW(child + 0x34u) == 0xFF20u || DSW(child + 0x34u) == 0xFE60u,
                      "the child's +0x34 is the bit-14-clear word");
            }
        }
    }

}
''')
sub("port/tests/test.h", "    X(test_p45_p5_records) \\\n", "    X(test_p45_p5_records) \\\n    X(test_p45_p5_spawns) \\\n")
print("t6_test applied")
PY
cmake --build build 2>&1 | grep -E ' error' | sed 's#.*/port/#port/#'
```

Expected: one undeclared-identifier error for `p45_check_p5_spawns`.

- [ ] **Step 3: the port, the registrations, the bindings and the mutants.** Append the functions to `fighter.c`, their prototypes to `fighter.h`, the wrappers and registrations to `actors.c` (after `0x37B70`), and the bindings and rows to `diff_runner.c` (after `fighter_37b70@sext`):

```
/* 0x3D328 — record §P5.4. The record's +0x14 pointer; when non-zero a child
 * from 0xBB2B8 with a2 = 8 (bit 14 of the record's +0x28 set) or -8, a4 = 4,
 * a5 = the record's +0x56 | 0x400; the child's +0x14 is the held record; on
 * the record's side: its +0x2E += 4, its +0x4E = 1 and the voice 0x4E; then
 * the record's +0x4B its pool index and the child's +0x60 = 1. */
void fighter_3d328(u32 rec)
{
    u32 held = DSD(rec + 0x14u);                            /* 0x3D32E */
    u32 child;
    s32 a2;
    if (held == 0u) return;                                 /* 0x3D332 */
    a2 = (DSW(rec + 0x28u) & 0x4000u) != 0u ? 8 : -8;       /* 0x3D334..0x3D34B */
    child = actor_spawn((const u32 *)(mem + P5_DESC_BB2B8), (u32)a2, 0u, 4u,
                        (u32)(u16)(DSW(rec + 0x56u) | 0x0400u));   /* 0x3D350..0x3D36D */
    DSD(child + 0x14u) = held;                              /* 0x3D372/0x3D375 */
    if (DSB(rec + 0x51u) != 0u) {                           /* 0x3D378..0x3D37F */
        u16 w = DSW(child + 0x2Eu);                         /* 0x3D381 */
        DSB(child + 0x4Eu) = 1u;                            /* 0x3D385 */
        DSW(child + 0x2Eu) = (u16)(w + 4u);                 /* 0x3D389/0x3D38C */
    }
    sound_voice(0x4Eu);                                     /* 0x3D390/0x3D395 */
    DSB(rec + 0x4Bu) = DSB(child + 0x56u);                  /* 0x3D39A/0x3D39D */
    DSB(child + 0x60u) = 1u;                                /* 0x3D3A0 */
}

/* 0x3DA50 — record §P5.4. 0x3D328's shape with a2 = 0x8C / -0x8C, the child
 * from 0xBB2E0 with a4 = 0x20 and its +0x59 = 2. */
void fighter_3da50(u32 rec)
{
    u32 held = DSD(rec + 0x14u);                            /* 0x3DA56 */
    u32 child;
    s32 a2;
    if (held == 0u) return;                                 /* 0x3DA5A */
    a2 = (DSW(rec + 0x28u) & 0x4000u) != 0u ? 0x8C : -0x8C;   /* 0x3DA60..0x3DA77 */
    child = actor_spawn((const u32 *)(mem + P5_DESC_BB2E0), (u32)a2, 0u, 0x20u,
                        (u32)(u16)(DSW(rec + 0x56u) | 0x0400u));   /* 0x3DA7C..0x3DA99 */
    DSB(child + 0x59u) = 2u;                                /* 0x3DAA1 */
    DSD(child + 0x14u) = held;                              /* 0x3DA9E/0x3DAA5 */
    if (DSB(rec + 0x51u) != 0u) {                           /* 0x3DAA8..0x3DAAF */
        u16 w = DSW(child + 0x2Eu);                         /* 0x3DAB1 */
        DSB(child + 0x4Eu) = 1u;                            /* 0x3DAB5 */
        DSW(child + 0x2Eu) = (u16)(w + 4u);                 /* 0x3DAB9/0x3DABC */
    }
    sound_voice(0x4Eu);                                     /* 0x3DAC0/0x3DAC5 */
    DSB(rec + 0x4Bu) = DSB(child + 0x56u);                  /* 0x3DACA/0x3DACD */
    DSB(child + 0x60u) = 1u;                                /* 0x3DAD0 */
}

/* 0x3DB8C — record §P5.4. The record's +0x14 pointer; when non-zero a child
 * from 0xBB2CC with a2 = 0x1000 (bit 14 set) or -0x1000 plus the record's
 * +0x18, a3 = the record's +0x30 >> 16, a4 = its +0x1C + 0x1300, a5 = 0x4000
 * (bit 14) or 0; the held record's +8 the child, the child's word +0x34 =
 * 0x00E0 (bit 14) or 0xFF20, its +0x14 the held record; on the record's side
 * its +0x2E += 4 and +0x4E = 1. */
void fighter_3db8c(u32 rec)
{
    u32 held = DSD(rec + 0x14u);                            /* 0x3DB97 */
    u32 child;
    s32 a2;
    u32 a5;
    u16 w34;
    if (held == 0u) return;                                 /* 0x3DB9A/0x3DB9C */
    if ((DSW(rec + 0x28u) & 0x4000u) != 0u) {               /* 0x3DBA2..0x3DBB0 */
        a2 = 0x1000;                                        /* 0x3DBC6 */
        w34 = 0x00E0u;                                      /* 0x3DBC1 */
        a5 = 0x4000u;                                       /* 0x3DBDF */
    } else {
        a2 = -0x1000;                                       /* 0x3DBB7 */
        w34 = 0xFF20u;                                      /* 0x3DBB2 */
        a5 = 0u;                                            /* 0x3DBE4 */
    }
    child = actor_spawn((const u32 *)(mem + P5_DESC_BB2CC),
                        (u32)(a2 + (s32)DSD(rec + 0x18u)),
                        (u32)((s32)DSD(rec + 0x30u) >> 16),
                        DSD(rec + 0x1Cu) + 0x1300u, a5);    /* 0x3DBEA..0x3DC07 */
    DSD(held + 8u) = child;                                 /* 0x3DC0F */
    DSW(child + 0x34u) = w34;                               /* 0x3DC12 */
    DSD(child + 0x14u) = held;                              /* 0x3DC16/0x3DC19 */
    if (DSB(rec + 0x51u) != 0u) {                           /* 0x3DC1C/0x3DC20 */
        DSW(child + 0x2Eu) = (u16)(DSW(child + 0x2Eu) + 4u);   /* 0x3DC25 */
        DSB(child + 0x4Eu) = 1u;                            /* 0x3DC2D */
    }
}

/* 0x3DC3C — record §P5.4. 0x3DB8C's shape with the child's word +0x34 =
 * 0x01A0 (bit 14) or 0xFE60. */
void fighter_3dc3c(u32 rec)
{
    u32 held = DSD(rec + 0x14u);                            /* 0x3DC47 */
    u32 child;
    s32 a2;
    u32 a5;
    u16 w34;
    if (held == 0u) return;                                 /* 0x3DC4A/0x3DC4C */
    if ((DSW(rec + 0x28u) & 0x4000u) != 0u) {               /* 0x3DC52..0x3DC60 */
        a2 = 0x1000;                                        /* 0x3DC76 */
        w34 = 0x01A0u;                                      /* 0x3DC71 */
        a5 = 0x4000u;                                       /* 0x3DC8F */
    } else {
        a2 = -0x1000;                                       /* 0x3DC67 */
        w34 = 0xFE60u;                                      /* 0x3DC62 */
        a5 = 0u;                                            /* 0x3DC94 */
    }
    child = actor_spawn((const u32 *)(mem + P5_DESC_BB2CC),
                        (u32)(a2 + (s32)DSD(rec + 0x18u)),
                        (u32)((s32)DSD(rec + 0x30u) >> 16),
                        DSD(rec + 0x1Cu) + 0x1300u, a5);    /* 0x3DC9A..0x3DCB7 */
    DSD(held + 8u) = child;                                 /* 0x3DCBF */
    DSW(child + 0x34u) = w34;                               /* 0x3DCC2 */
    DSD(child + 0x14u) = held;                              /* 0x3DCC6/0x3DCC9 */
    if (DSB(rec + 0x51u) != 0u) {                           /* 0x3DCCC/0x3DCD0 */
        DSW(child + 0x2Eu) = (u16)(DSW(child + 0x2Eu) + 4u);   /* 0x3DCD5 */
        DSB(child + 0x4Eu) = 1u;                            /* 0x3DCDD */
    }
}

/* 0x403A0 — record §P5.4. The record's +0x14 pointer; when non-zero and the
 * other side's slot (rec+0x51 ^ 1) non-zero: the word 0xC7780[its character]
 * (negated, 16-bit, when that slot's record's +0x28 bit 14 is set) as a2, the
 * signed word 0xC778C[its character] as a4, a5 = that record's +0x56 | 0x400,
 * a child from 0xC776C; the child's +0x14 the held record, its +0x51 the
 * record's side, its +0x59 the record's +0x59 - 1. */
void fighter_403a0(u32 rec)
{
    u32 held = DSD(rec + 0x14u);                            /* 0x403A6 */
    u32 other, slot, srec, child;
    u32 ch;
    s32 a2;
    if (held == 0u) return;                                 /* 0x403AA */
    other = (u32)DSB(rec + 0x51u) ^ 1u;                     /* 0x403B0..0x403B5 */
    slot = DSD(DS_001077A8 + other * 4u);                   /* 0x403BA */
    if (slot == 0u) return;                                 /* 0x403C1/0x403C3 */
    srec = DSD(slot);                                       /* 0x403C5 */
    ch = (u32)DSB(slot + 0x7Au);                            /* 0x403CD */
    a2 = (s32)(s16)DSW(P5_C7780 + ch * 2u);                 /* 0x403D2 */
    if ((DSW(srec + 0x28u) & 0x4000u) != 0u)                /* 0x403C9..0x403E3 */
        a2 = (s32)(s16)(u16)(0u - (u32)(u16)a2);            /* 0x403E5 */
    child = actor_spawn((const u32 *)(mem + P5_DESC_C776C), (u32)a2, 0u,
                        (u32)(s32)(s16)DSW(P5_C778C + ch * 2u + 2u),
                        (u32)(u16)(DSW(srec + 0x56u) | 0x0400u));   /* 0x403E7..0x40413 */
    DSD(child + 0x14u) = held;                              /* 0x40418/0x4041B */
    DSB(child + 0x51u) = DSB(rec + 0x51u);                  /* 0x4041E/0x40421 */
    DSB(child + 0x59u) = (u8)(DSB(rec + 0x59u) - 1u);       /* 0x40424..0x40429 */
}

/* 0x40FBC — record §P5.4. A child from 0xC77D8 with a2 = 8 when
 * 0x1A570(rec+0x51) sets AL else -8, a5 = the record's +0x56 | 0x400; its
 * +0x59 = 2, its +0x24 and +0x20 the record's, and on the record's side the
 * child's palette (0x2A17C, word 0x0C, handle 0). */
void fighter_40fbc(u32 rec)
{
    s32 a2 = fighter_actor_bit15_clear((u32)DSB(rec + 0x51u)) != 0 ? 8 : -8;   /* 0x40FC2..0x40FDC */
    u32 child = actor_spawn((const u32 *)(mem + P5_DESC_C77D8), (u32)a2, 0u, 0u,
                            (u32)(u16)(DSW(rec + 0x56u) | 0x0400u));   /* 0x40FE9..0x40FF6 */
    DSB(child + 0x59u) = 2u;                                /* 0x40FFB */
    DSD(child + 0x24u) = DSD(rec + 0x24u);                  /* 0x40FFF/0x41002 */
    DSD(child + 0x20u) = DSD(rec + 0x20u);                  /* 0x41005/0x41008 */
    if (DSB(rec + 0x51u) != 0u)                             /* 0x4100B/0x4100F */
        actor_pset_palette(child, 0x0Cu, 0u);               /* 0x41011..0x41018 0x2A17C */
}

/* 0x48A20 — record §P5.4. Walks the 0x1014F4 chain from the record while its
 * +0x4B is non-zero (esi = base + byte * 0x68), then spawns from 0xC94F8
 * with a2 = the record's +0x18 + 0xC40 (bit 14 of its +0x28 set) or - 0xC40,
 * a3 = its +0x30 >> 16, a4 = its +0x1C + 0x800, a5 = 0x4000 or 0; the walked
 * record's +0x4B takes the child's pool index. The raw stores 0x1014F4 back
 * (the same value) before the spawn and reloads it after (dead). */
void fighter_48a20(u32 rec)
{
    u32 base = DSD(P5_1014F4);                              /* 0x48A25 */
    u32 esi = rec;                                          /* 0x48A2D/0x48A2F */
    u32 a2, a5, child;
    while (DSB(esi + 0x4Bu) != 0u)                          /* 0x48A2F..0x48A44 */
        esi = base + (u32)DSB(esi + 0x4Bu) * 0x68u;
    if ((DSW(rec + 0x28u) & 0x4000u) != 0u) {               /* 0x48A46..0x48A54 */
        a2 = DSD(rec + 0x18u) + 0xC40u;                     /* 0x48A56..0x48A5E */
        a5 = 0x4000u;                                       /* 0x48A59 */
    } else {
        a2 = DSD(rec + 0x18u) - 0xC40u;                     /* 0x48A65..0x48A6A */
        a5 = 0u;                                            /* 0x48A68 */
    }
    DSD(P5_1014F4) = base;                                  /* 0x48A8C */
    child = actor_spawn((const u32 *)(mem + P5_DESC_C94F8), a2,
                        (u32)((s32)DSD(rec + 0x30u) >> 16),
                        DSD(rec + 0x1Cu) + 0x800u, a5);     /* 0x48A75..0x48A92 */
    DSB(esi + 0x4Bu) = DSB(child + 0x56u);                  /* 0x48A9A/0x48AA0 */
}


/* 0x3D328 / 0x3DA50: the spawn-family mutants. */
static void m_3d328_at(const u32 *r, u32 desc, int a2_mode, u32 a4, int no_side, u32 voice)
{
    u32 rec = r[R_EAX], held = DSD(rec + 0x14u), child;
    s32 a2;
    if (held == 0u) return;
    a2 = (DSW(rec + 0x28u) & 0x4000u) != 0u ? 8 : -8;
    if (a2_mode == 1) a2 = -8;
    if (a2_mode == 2) a2 = 8;
    child = actor_spawn((const u32 *)(mem + desc), (u32)a2, 0u, a4,
                        (u32)(u16)(DSW(rec + 0x56u) | 0x0400u));
    DSD(child + 0x14u) = held;
    if (DSB(rec + 0x51u) != 0u || no_side) {
        u16 w = DSW(child + 0x2Eu);
        DSB(child + 0x4Eu) = 1u;
        DSW(child + 0x2Eu) = (u16)(w + 4u);
        sound_voice(voice);
    }
    DSB(rec + 0x4Bu) = DSB(child + 0x56u);
    DSB(child + 0x60u) = 1u;
}

/* 0x3DB8C / 0x3DC3C: the child's +0x34 word and a2/a3/a4. */
static void m_3db8c_at(const u32 *r, u32 desc, u16 w14, int no_x18, u16 w0, int word_a4, int no_side)
{
    u32 rec = r[R_EAX], held = DSD(rec + 0x14u), child;
    s32 a2;
    u32 a5, a4;
    if (held == 0u) return;
    if ((DSW(rec + 0x28u) & 0x4000u) != 0u) { a2 = 0x1000; a5 = 0x4000u; }
    else { a2 = -0x1000; a5 = 0u; }
    a4 = word_a4 ? (u32)(u16)(DSD(rec + 0x1Cu) + 0x1300u) : DSD(rec + 0x1Cu) + 0x1300u;
    child = actor_spawn((const u32 *)(mem + desc),
                        (u32)(no_x18 ? a2 : a2 + (s32)DSD(rec + 0x18u)),
                        (u32)((s32)DSD(rec + 0x30u) >> 16), a4, a5);
    DSD(held + 8u) = child;
    DSW(child + 0x34u) = (DSW(rec + 0x28u) & 0x4000u) != 0u ? w14 : w0;
    DSD(child + 0x14u) = held;
    if (DSB(rec + 0x51u) != 0u || no_side) {
        DSW(child + 0x2Eu) = (u16)(DSW(child + 0x2Eu) + 4u);
        DSB(child + 0x4Eu) = 1u;
    }
}

static void b_3d328(const u32 *r, u32 *eax)      { fighter_3d328(r[R_EAX]); *eax = 0u; }
static void b_3da50(const u32 *r, u32 *eax)      { fighter_3da50(r[R_EAX]); *eax = 0u; }
static void b_3db8c(const u32 *r, u32 *eax)      { fighter_3db8c(r[R_EAX]); *eax = 0u; }
static void b_3dc3c(const u32 *r, u32 *eax)      { fighter_3dc3c(r[R_EAX]); *eax = 0u; }
static void b_403a0(const u32 *r, u32 *eax)      { fighter_403a0(r[R_EAX]); *eax = 0u; }
static void b_40fbc(const u32 *r, u32 *eax)      { fighter_40fbc(r[R_EAX]); *eax = 0u; }
static void b_48a20(const u32 *r, u32 *eax)      { fighter_48a20(r[R_EAX]); *eax = 0u; }
static void m_3d328(const u32 *r, u32 *eax)      { m_3d328_at(r, 0x000BB2BCu, 0, 4u, 0, 0x4Eu); *eax = 0u; }
static void m_3d328_a2(const u32 *r, u32 *eax)   { m_3d328_at(r, 0x000BB2B8u, 1, 4u, 0, 0x4Eu); *eax = 0u; }
static void m_3d328_a4(const u32 *r, u32 *eax)   { m_3d328_at(r, 0x000BB2B8u, 0, 0u, 0, 0x4Eu); *eax = 0u; }
static void m_3d328_side(const u32 *r, u32 *eax) { m_3d328_at(r, 0x000BB2B8u, 0, 4u, 1, 0x4Eu); *eax = 0u; }
static void m_3d328_voice(const u32 *r, u32 *eax) { m_3d328_at(r, 0x000BB2B8u, 0, 4u, 0, 0x4Fu); *eax = 0u; }
static void m_3da50(const u32 *r, u32 *eax)      { m_3d328_at(r, 0x000BB2E4u, 2, 0x20u, 0, 0x4Eu); *eax = 0u; }
static void m_3da50_a2(const u32 *r, u32 *eax)   { m_3d328_at(r, 0x000BB2E0u, 2, 0x20u, 0, 0x4Eu); *eax = 0u; }
static void m_3da50_a4(const u32 *r, u32 *eax)   { m_3d328_at(r, 0x000BB2E0u, 0, 4u, 0, 0x4Eu); *eax = 0u; }
static void m_3da50_side(const u32 *r, u32 *eax) { m_3d328_at(r, 0x000BB2E0u, 0, 0x20u, 1, 0x4Eu); *eax = 0u; }
static void m_3db8c(const u32 *r, u32 *eax)      { m_3db8c_at(r, 0x000BB2C8u, 0x00E0u, 0, 0xFF20u, 0, 0); *eax = 0u; }
static void m_3db8c_a2(const u32 *r, u32 *eax)   { m_3db8c_at(r, 0x000BB2CCu, 0x00E0u, 1, 0xFF20u, 0, 0); *eax = 0u; }
static void m_3db8c_w34(const u32 *r, u32 *eax)  { m_3db8c_at(r, 0x000BB2CCu, 0x00E1u, 0, 0xFF20u, 0, 0); *eax = 0u; }
static void m_3db8c_a4(const u32 *r, u32 *eax)   { m_3db8c_at(r, 0x000BB2CCu, 0x00E0u, 0, 0xFF20u, 1, 0); *eax = 0u; }
static void m_3db8c_side(const u32 *r, u32 *eax) { m_3db8c_at(r, 0x000BB2CCu, 0x00E0u, 0, 0xFF20u, 0, 1); *eax = 0u; }
static void m_3dc3c(const u32 *r, u32 *eax)      { m_3db8c_at(r, 0x000BB2C8u, 0x01A0u, 0, 0xFF60u, 0, 0); *eax = 0u; }
static void m_3dc3c_a2(const u32 *r, u32 *eax)   { m_3db8c_at(r, 0x000BB2CCu, 0x01A0u, 1, 0xFF60u, 0, 0); *eax = 0u; }
static void m_3dc3c_w34(const u32 *r, u32 *eax)  { m_3db8c_at(r, 0x000BB2CCu, 0x01A1u, 0, 0xFF60u, 0, 0); *eax = 0u; }
static void m_3dc3c_side(const u32 *r, u32 *eax) { m_3db8c_at(r, 0x000BB2CCu, 0x01A0u, 0, 0xFF60u, 0, 1); *eax = 0u; }
static void m_403a0(const u32 *r, u32 *eax)
{
    u32 rec = r[R_EAX], held = DSD(rec + 0x14u), other, slot, srec, child, ch;
    s32 a2;
    if (held == 0u) return;
    other = (u32)DSB(rec + 0x51u) ^ 1u;
    slot = DSD(0x001077A8u + other * 4u);
    if (slot == 0u) return;
    srec = DSD(slot); ch = (u32)DSB(slot + 0x7Au);
    a2 = (s32)(s16)DSW(0x000C7780u + ch * 2u);
    if ((DSW(srec + 0x28u) & 0x4000u) != 0u) a2 = (s32)(s16)(u16)(0u - (u32)(u16)a2);
    child = actor_spawn((const u32 *)(mem + 0x000C7770u), (u32)a2, 0u,
                        (u32)(s32)(s16)DSW(0x000C778Cu + ch * 2u + 2u),
                        (u32)(u16)(DSW(srec + 0x56u) | 0x0400u));
    DSD(child + 0x14u) = held;
    DSB(child + 0x51u) = DSB(rec + 0x51u);
    DSB(child + 0x59u) = (u8)(DSB(rec + 0x59u) - 1u);
    *eax = 0u;
}
static void m_403a0_side(const u32 *r, u32 *eax)
{
    u32 rec = r[R_EAX], held = DSD(rec + 0x14u), other, slot, srec, child, ch;
    s32 a2;
    if (held == 0u) return;
    other = ((u32)DSB(rec + 0x51u) & 1u) ^ 1u;
    slot = DSD(0x001077A8u + other * 4u);
    if (slot == 0u) return;
    srec = DSD(slot); ch = (u32)DSB(slot + 0x7Au);
    a2 = (s32)(s16)DSW(0x000C7780u + ch * 2u);
    if ((DSW(srec + 0x28u) & 0x4000u) != 0u) a2 = (s32)(s16)(u16)(0u - (u32)(u16)a2);
    child = actor_spawn((const u32 *)(mem + 0x000C776Cu), (u32)a2, 0u,
                        (u32)(s32)(s16)DSW(0x000C778Cu + ch * 2u + 2u),
                        (u32)(u16)(DSW(srec + 0x56u) | 0x0400u));
    DSD(child + 0x14u) = held;
    DSB(child + 0x51u) = DSB(rec + 0x51u);
    DSB(child + 0x59u) = (u8)(DSB(rec + 0x59u) - 1u);
    *eax = 0u;
}
static void m_403a0_signed(const u32 *r, u32 *eax)
{
    u32 rec = r[R_EAX], held = DSD(rec + 0x14u), other, slot, srec, child, ch;
    s32 a2;
    if (held == 0u) return;
    other = (u32)DSB(rec + 0x51u) ^ 1u;
    slot = DSD(0x001077A8u + other * 4u);
    if (slot == 0u) return;
    srec = DSD(slot); ch = (u32)DSB(slot + 0x7Au);
    a2 = (s32)(s16)DSW(0x000C7780u + ch * 2u);
    if ((DSW(srec + 0x28u) & 0x4000u) != 0u) a2 = (s32)(s16)(u16)(0u - (u32)(u16)a2);
    child = actor_spawn((const u32 *)(mem + 0x000C776Cu), (u32)a2, 0u,
                        (u32)DSW(0x000C778Cu + ch * 2u + 2u),
                        (u32)(u16)(DSW(srec + 0x56u) | 0x0400u));
    DSD(child + 0x14u) = held;
    DSB(child + 0x51u) = DSB(rec + 0x51u);
    DSB(child + 0x59u) = (u8)(DSB(rec + 0x59u) - 1u);
    *eax = 0u;
}
static void m_403a0_neg(const u32 *r, u32 *eax)
{
    u32 rec = r[R_EAX], held = DSD(rec + 0x14u), other, slot, srec, child, ch;
    s32 a2;
    if (held == 0u) return;
    other = (u32)DSB(rec + 0x51u) ^ 1u;
    slot = DSD(0x001077A8u + other * 4u);
    if (slot == 0u) return;
    srec = DSD(slot); ch = (u32)DSB(slot + 0x7Au);
    a2 = (s32)(s16)DSW(0x000C7780u + ch * 2u);
    child = actor_spawn((const u32 *)(mem + 0x000C776Cu), (u32)a2, 0u,
                        (u32)(s32)(s16)DSW(0x000C778Cu + ch * 2u + 2u),
                        (u32)(u16)(DSW(srec + 0x56u) | 0x0400u));
    DSD(child + 0x14u) = held;
    DSB(child + 0x51u) = DSB(rec + 0x51u);
    DSB(child + 0x59u) = (u8)(DSB(rec + 0x59u) - 1u);
    *eax = 0u;
}
static void m_403a0_a5(const u32 *r, u32 *eax)
{
    u32 rec = r[R_EAX], held = DSD(rec + 0x14u), other, slot, srec, child, ch;
    s32 a2;
    if (held == 0u) return;
    other = (u32)DSB(rec + 0x51u) ^ 1u;
    slot = DSD(0x001077A8u + other * 4u);
    if (slot == 0u) return;
    srec = DSD(slot); ch = (u32)DSB(slot + 0x7Au);
    a2 = (s32)(s16)DSW(0x000C7780u + ch * 2u);
    if ((DSW(srec + 0x28u) & 0x4000u) != 0u) a2 = (s32)(s16)(u16)(0u - (u32)(u16)a2);
    child = actor_spawn((const u32 *)(mem + 0x000C776Cu), (u32)a2, 0u,
                        (u32)(s32)(s16)DSW(0x000C778Cu + ch * 2u + 2u),
                        (u32)(u16)DSW(srec + 0x56u));
    DSD(child + 0x14u) = held;
    DSB(child + 0x51u) = DSB(rec + 0x51u);
    DSB(child + 0x59u) = (u8)(DSB(rec + 0x59u) - 1u);
    *eax = 0u;
}
static void m_403a0_r59(const u32 *r, u32 *eax)
{
    u32 rec = r[R_EAX], held = DSD(rec + 0x14u), other, slot, srec, child, ch;
    s32 a2;
    if (held == 0u) return;
    other = (u32)DSB(rec + 0x51u) ^ 1u;
    slot = DSD(0x001077A8u + other * 4u);
    if (slot == 0u) return;
    srec = DSD(slot); ch = (u32)DSB(slot + 0x7Au);
    a2 = (s32)(s16)DSW(0x000C7780u + ch * 2u);
    if ((DSW(srec + 0x28u) & 0x4000u) != 0u) a2 = (s32)(s16)(u16)(0u - (u32)(u16)a2);
    child = actor_spawn((const u32 *)(mem + 0x000C776Cu), (u32)a2, 0u,
                        (u32)(s32)(s16)DSW(0x000C778Cu + ch * 2u + 2u),
                        (u32)(u16)(DSW(srec + 0x56u) | 0x0400u));
    DSD(child + 0x14u) = held;
    DSB(child + 0x51u) = DSB(rec + 0x51u);
    DSB(child + 0x59u) = DSB(rec + 0x59u);
    *eax = 0u;
}
static void m_40fbc(const u32 *r, u32 *eax)
{
    u32 rec = r[R_EAX], child;
    s32 a2 = fighter_actor_bit15_clear((u32)DSB(rec + 0x51u)) != 0 ? 8 : -8;
    child = actor_spawn((const u32 *)(mem + 0x000C77D8u), (u32)a2, 0u, 0u,
                        (u32)(u16)(DSW(rec + 0x56u) | 0x0400u));
    DSB(child + 0x59u) = 2u;
    DSD(child + 0x24u) = DSD(rec + 0x24u);
    DSD(child + 0x20u) = DSD(rec + 0x20u);
    if (DSB(rec + 0x51u) != 0u) actor_pset_palette(child, 0x0Du, 0u);
    *eax = 0u;
}
static void m_40fbc_pal(const u32 *r, u32 *eax)
{
    u32 rec = r[R_EAX], child;
    s32 a2 = fighter_actor_bit15_clear((u32)DSB(rec + 0x51u)) != 0 ? 8 : -8;
    child = actor_spawn((const u32 *)(mem + 0x000C77D8u), (u32)a2, 0u, 0u,
                        (u32)(u16)(DSW(rec + 0x56u) | 0x0400u));
    DSB(child + 0x59u) = 2u;
    DSD(child + 0x24u) = DSD(rec + 0x24u);
    DSD(child + 0x20u) = DSD(rec + 0x20u);
    actor_pset_palette(child, 0x0Cu, 0u);
    *eax = 0u;
}
static void m_40fbc_a2(const u32 *r, u32 *eax)
{
    u32 rec = r[R_EAX], child;
    child = actor_spawn((const u32 *)(mem + 0x000C77D8u), 8u, 0u, 0u,
                        (u32)(u16)(DSW(rec + 0x56u) | 0x0400u));
    DSB(child + 0x59u) = 2u;
    DSD(child + 0x24u) = DSD(rec + 0x24u);
    DSD(child + 0x20u) = DSD(rec + 0x20u);
    if (DSB(rec + 0x51u) != 0u) actor_pset_palette(child, 0x0Cu, 0u);
    *eax = 0u;
}
static void m_48a20(const u32 *r, u32 *eax)
{
    u32 rec = r[R_EAX], base = DSD(0x001014F4u), esi = rec, a2, a5, child;
    while (DSB(esi + 0x4Bu) != 0u) esi = base + (u32)DSB(esi + 0x4Bu) * 0x68u;
    if ((DSW(rec + 0x28u) & 0x4000u) != 0u) { a2 = DSD(rec + 0x18u) + 0xC40u; a5 = 0x4000u; }
    else { a2 = DSD(rec + 0x18u) - 0xC40u; a5 = 0u; }
    DSD(0x001014F4u) = base;
    child = actor_spawn((const u32 *)(mem + 0x000C94FCu), a2,
                        (u32)((s32)DSD(rec + 0x30u) >> 16), DSD(rec + 0x1Cu) + 0x800u, a5);
    DSB(esi + 0x4Bu) = DSB(child + 0x56u);
    *eax = 0u;
}
static void m_48a20_walk(const u32 *r, u32 *eax)
{
    u32 rec = r[R_EAX], base = DSD(0x001014F4u), a2, a5, child;
    if ((DSW(rec + 0x28u) & 0x4000u) != 0u) { a2 = DSD(rec + 0x18u) + 0xC40u; a5 = 0x4000u; }
    else { a2 = DSD(rec + 0x18u) - 0xC40u; a5 = 0u; }
    DSD(0x001014F4u) = base;
    child = actor_spawn((const u32 *)(mem + 0x000C94F8u), a2,
                        (u32)((s32)DSD(rec + 0x30u) >> 16), DSD(rec + 0x1Cu) + 0x800u, a5);
    DSB(rec + 0x4Bu) = DSB(child + 0x56u);
    *eax = 0u;
}
static void m_48a20_a2(const u32 *r, u32 *eax)
{
    u32 rec = r[R_EAX], base = DSD(0x001014F4u), esi = rec, a5, child;
    while (DSB(esi + 0x4Bu) != 0u) esi = base + (u32)DSB(esi + 0x4Bu) * 0x68u;
    a5 = (DSW(rec + 0x28u) & 0x4000u) != 0u ? 0x4000u : 0u;
    DSD(0x001014F4u) = base;
    child = actor_spawn((const u32 *)(mem + 0x000C94F8u), DSD(rec + 0x18u) + 0xC40u,
                        (u32)((s32)DSD(rec + 0x30u) >> 16), DSD(rec + 0x1Cu) + 0x800u, a5);
    DSB(esi + 0x4Bu) = DSB(child + 0x56u);
    *eax = 0u;
}


    { "fighter_3d328",          b_3d328,         0x00000000u },
    { "fighter_3d328@mutant",   m_3d328,         0x00000000u },
    { "fighter_3d328@a2",       m_3d328_a2,      0x00000000u },
    { "fighter_3d328@a4",       m_3d328_a4,      0x00000000u },
    { "fighter_3d328@side",     m_3d328_side,    0x00000000u },
    { "fighter_3d328@voice",    m_3d328_voice,   0x00000000u },
    { "fighter_3da50",          b_3da50,         0x00000000u },
    { "fighter_3da50@mutant",   m_3da50,         0x00000000u },
    { "fighter_3da50@a2",       m_3da50_a2,      0x00000000u },
    { "fighter_3da50@a4",       m_3da50_a4,      0x00000000u },
    { "fighter_3da50@side",     m_3da50_side,    0x00000000u },
    { "fighter_3db8c",          b_3db8c,         0x00000000u },
    { "fighter_3db8c@mutant",   m_3db8c,         0x00000000u },
    { "fighter_3db8c@a2",       m_3db8c_a2,      0x00000000u },
    { "fighter_3db8c@w34",      m_3db8c_w34,     0x00000000u },
    { "fighter_3db8c@a4",       m_3db8c_a4,      0x00000000u },
    { "fighter_3db8c@side",     m_3db8c_side,    0x00000000u },
    { "fighter_3dc3c",          b_3dc3c,         0x00000000u },
    { "fighter_3dc3c@mutant",   m_3dc3c,         0x00000000u },
    { "fighter_3dc3c@a2",       m_3dc3c_a2,      0x00000000u },
    { "fighter_3dc3c@w34",      m_3dc3c_w34,     0x00000000u },
    { "fighter_3dc3c@side",     m_3dc3c_side,    0x00000000u },
    { "fighter_403a0",          b_403a0,         0x00000000u },
    { "fighter_403a0@mutant",   m_403a0,         0x00000000u },
    { "fighter_403a0@side",     m_403a0_side,    0x00000000u },
    { "fighter_403a0@signed",   m_403a0_signed,  0x00000000u },
    { "fighter_403a0@neg",      m_403a0_neg,     0x00000000u },
    { "fighter_403a0@a5",       m_403a0_a5,      0x00000000u },
    { "fighter_403a0@r59",      m_403a0_r59,     0x00000000u },
    { "fighter_40fbc",          b_40fbc,         0x00000000u },
    { "fighter_40fbc@mutant",   m_40fbc,         0x00000000u },
    { "fighter_40fbc@pal",      m_40fbc_pal,     0x00000000u },
    { "fighter_40fbc@a2",       m_40fbc_a2,      0x00000000u },
    { "fighter_48a20",          b_48a20,         0x00000000u },
    { "fighter_48a20@mutant",   m_48a20,         0x00000000u },
    { "fighter_48a20@walk",     m_48a20_walk,    0x00000000u },
    { "fighter_48a20@a2",       m_48a20_a2,      0x00000000u },

```

Then the same build/test/verify commands print `all checks passed` and the measured rows:

| fighter_3d328 | 0x3D328 | 4 | 8/8 | VERIFIED | 2AE14 stub unverified, 2C3FC stub unverified |
| fighter_3da50 | 0x3DA50 | 4 | 8/8 | VERIFIED | 2AE14 stub unverified, 2C3FC stub unverified |
| fighter_3db8c | 0x3DB8C | 4 | 9/9 | VERIFIED | 2AE14 stub unverified |
| fighter_3dc3c | 0x3DC3C | 4 | 9/9 | VERIFIED | 2AE14 stub unverified |
| fighter_403a0 | 0x403A0 | 5 | 6/6 | VERIFIED | 2AE14 stub unverified |
| fighter_40fbc | 0x40FBC | 3 | 6/6 | VERIFIED | 1A570 stub VERIFIED, 2A17C stub unverified, 2AE14 stub unverified |
| fighter_48a20 | 0x48A20 | 3 | 6/6 | VERIFIED | 2AE14 stub unverified |
| fighter_3d328@mutant | 0x3D328 | 4 | 8/8 | MISMATCH | 2AE14 stub unverified, 2C3FC stub unverified |
| fighter_3d328@a2 | 0x3D328 | 4 | 8/8 | MISMATCH | 2AE14 stub unverified, 2C3FC stub unverified |
| fighter_3d328@a4 | 0x3D328 | 4 | 8/8 | MISMATCH | 2AE14 stub unverified, 2C3FC stub unverified |
| fighter_3d328@side | 0x3D328 | 4 | 8/8 | MISMATCH | 2AE14 stub unverified, 2C3FC stub unverified |
| fighter_3d328@voice | 0x3D328 | 4 | 8/8 | MISMATCH | 2AE14 stub unverified, 2C3FC stub unverified |
| fighter_3da50@mutant | 0x3DA50 | 4 | 8/8 | MISMATCH | 2AE14 stub unverified, 2C3FC stub unverified |
| fighter_3da50@a2 | 0x3DA50 | 4 | 8/8 | MISMATCH | 2AE14 stub unverified, 2C3FC stub unverified |
| fighter_3da50@a4 | 0x3DA50 | 4 | 8/8 | MISMATCH | 2AE14 stub unverified, 2C3FC stub unverified |
| fighter_3da50@side | 0x3DA50 | 4 | 8/8 | MISMATCH | 2AE14 stub unverified, 2C3FC stub unverified |
| fighter_3db8c@mutant | 0x3DB8C | 4 | 9/9 | MISMATCH | 2AE14 stub unverified |
| fighter_3db8c@a2 | 0x3DB8C | 4 | 9/9 | MISMATCH | 2AE14 stub unverified |
| fighter_3db8c@w34 | 0x3DB8C | 4 | 9/9 | MISMATCH | 2AE14 stub unverified |
| fighter_3db8c@a4 | 0x3DB8C | 4 | 9/9 | MISMATCH | 2AE14 stub unverified |
| fighter_3db8c@side | 0x3DB8C | 4 | 9/9 | MISMATCH | 2AE14 stub unverified |
| fighter_3dc3c@mutant | 0x3DC3C | 4 | 9/9 | MISMATCH | 2AE14 stub unverified |
| fighter_3dc3c@a2 | 0x3DC3C | 4 | 9/9 | MISMATCH | 2AE14 stub unverified |
| fighter_3dc3c@w34 | 0x3DC3C | 4 | 9/9 | MISMATCH | 2AE14 stub unverified |
| fighter_3dc3c@side | 0x3DC3C | 4 | 9/9 | MISMATCH | 2AE14 stub unverified |
| fighter_403a0@mutant | 0x403A0 | 5 | 6/6 | MISMATCH | 2AE14 stub unverified |
| fighter_403a0@side | 0x403A0 | 5 | 6/6 | MISMATCH | 2AE14 stub unverified |
| fighter_403a0@signed | 0x403A0 | 5 | 6/6 | MISMATCH | 2AE14 stub unverified |
| fighter_403a0@neg | 0x403A0 | 5 | 6/6 | MISMATCH | 2AE14 stub unverified |
| fighter_403a0@a5 | 0x403A0 | 5 | 6/6 | MISMATCH | 2AE14 stub unverified |
| fighter_403a0@r59 | 0x403A0 | 5 | 6/6 | MISMATCH | 2AE14 stub unverified |
| fighter_40fbc@mutant | 0x40FBC | 3 | 6/6 | MISMATCH | 1A570 stub VERIFIED, 2A17C stub unverified, 2AE14 stub unverified |
| fighter_40fbc@pal | 0x40FBC | 3 | 6/6 | MISMATCH | 1A570 stub VERIFIED, 2A17C stub unverified, 2AE14 stub unverified |
| fighter_40fbc@a2 | 0x40FBC | 3 | 6/6 | MISMATCH | 1A570 stub VERIFIED, 2A17C stub unverified, 2AE14 stub unverified |
| fighter_48a20@mutant | 0x48A20 | 3 | 6/6 | MISMATCH | 2AE14 stub unverified |
| fighter_48a20@walk | 0x48A20 | 3 | 6/6 | MISMATCH | 2AE14 stub unverified |
| fighter_48a20@a2 | 0x48A20 | 3 | 6/6 | MISMATCH | 2AE14 stub unverified |


- [ ] **Step 4: the E2 table.** Expected `entry-triage: targets 258 unported, 237 ported; supplement 131 (22 unported, 0 stale); untrusted entries 30`.
- [ ] **Step 5: the task gate.** Expected `diff-verify: 109/109 functions VERIFIED; 241/241 mutants detected; 1 named gaps; 13/92 rows with callees closed (17 have none). ...`.
- [ ] **Step 6: the mutation proofs.** The spawn-argument mutations:

```
port/src/game/fighter.c: 1 FAIL, first: ['test_fight.c:47063: 0 != 1']
port/src/game/fighter.c: 2 FAIL, first: ['test_fight.c:47055: 538976288 != 606348324']
all checks passed
```

- [ ] **Step 7: gp-u10-ending drops `0x3DA50` and keeps its pins** (record §P5.7). Every pin is exact and unchanged (each + 1 fails); the replay reaches no new target.

```bash
python3 - <<'PY'
import pathlib


def sub(path, old, new):
    p = pathlib.Path(path)
    s = p.read_text()
    n = s.count(old)
    assert n == 1, "%s: %d occurrences of %r" % (path, n, old[:70])
    p.write_text(s.replace(old, new))


sub("port/tests/test_platform.c", '''    { 0x37DD4u, "anim_indirect" },
    { 0x29C78u, "anim_indirect" },
    { 0x3DA50u, "anim_indirect" },
};''', '''    { 0x37DD4u, "anim_indirect" },
    { 0x29C78u, "anim_indirect" },
};''')
sub("Makefile", '''# Re-measure: a P batch that ports 0x37DD4 (P6), 0x29C78 (P7) or 0x3DA50 (P5) drops its row and
# re-measures the U10 set; TRACE/WIN are at the replay's end, so they cannot rise.''',
'''# Re-measured by track P batches 4+5 (record 2026-10-03-reverse-p4-p5 §P5.7) once 0x3DA50 is
# ported: the set loses that row alone, the replay reaches no new target and every pin above is
# unchanged (each + 1 fails).
# Re-measure: a P batch that ports 0x37DD4 (P6) or 0x29C78 (P7) drops its row and re-measures the
# U10 set; TRACE/WIN are at the replay's end, so they cannot rise.''')
print("t6_u10 applied")
PY
cmake --build build 2>&1 | grep -E 'error|warning'
make gp-ending-oracle GP_DUMP=/tmp/pr_p45_gp GP_WIN_KEEP=1 2>&1 | grep -E '^(Ran|OK|FAILED)|ratchet N|FAIL|fn-miss PR_GP_DUMP distinct'
SC=gp-u10-ending; CAP=data/k11-captures/$SC; SHA=a88de48ad39e90df1e3d3329fbd5aa876c69a08484fa22c66db8deebd3feff38
python3 tools/gp_compare.py --scenario $SC --capture $CAP --port /tmp/pr_p45_gp/$SC --min-first 332 --trace-min-first 9955 \
  --max-start 83 --capture-sha256 $SHA --capture-frames 5704 2>&1 | grep -E 'FAIL'
python3 tools/gp_win.py path --scenario $SC --capture $CAP --port /tmp/pr_p45_gp/$SC --min-milestones 31 --win-min-first 9955 \
  --capture-sha256 $SHA 2>&1 | grep -E 'FAIL'
rm -rf /tmp/pr_p45_gp/$SC
```

Expected (measured): the oracle passes with the set less `0x3DA50` (`distinct=6`) and each pin + 1 fails:

```
t6_u10 applied
Ran 18 tests in N.NNNs
OK
fn-miss PR_GP_DUMP distinct=6 dropped=0
gp_compare: gp-u10-ending: frames: first unexplained 331, ratchet N 331 ok
gp_compare: gp-u10-ending: trace: 0 differing through 9953; ratchet N 9954 ok
gp_compare: gp-u10-ending: path: 0 not reproduced through 29; ratchet N 30 ok
gp_compare: gp-u10-ending: win: 0 differing through 9953; ratchet N 9954 ok
gp_compare: gp-u10-ending: frames: FAIL: first unexplained 331 < ratchet N 332
gp_compare: gp-u10-ending: trace: FAIL: N 9955 > end 9954: N is unreachable
gp_win: gp-u10-ending: path: FAIL: N 31 > end 30: N is unreachable
gp_compare: gp-u10-ending: win: FAIL: N 9955 > end 9954: N is unreachable
```

- [ ] **Step 8: commit.** Message `fighter: port the spawn family 0x3D328..0x48A20; gp-u10-ending drops its 0x3DA50 row, pins unchanged; E2 table regenerated (track P batches 4+5, task 6)`.

---

### Task 7: the +0x10 handler `0x24508` and `0x24454` (seam `0x2AD40`)

**Files:** as Task 2 Steps 1-6. **Interfaces:** produces `void fighter_24508(u32 rec)`, `void fighter_24454(u32 slot, u32 rec)` and `void fighter_24454_case10(u32 slot, u32 side)`; `release_record`'s `PR_SEAM(0x2AD40u, rec, pset)`; `RELEASE`; `test_p45_handler`.

- [ ] **Step 1: the specs and the expectations first.**

```bash
python3 - <<'PY'
import pathlib


def sub(path, old, new):
    p = pathlib.Path(path)
    s = p.read_text()
    n = s.count(old)
    assert n == 1, "%s: %d occurrences of %r" % (path, n, old[:70])
    p.write_text(s.replace(old, new))


SPECS7 = r'''
Spec("fighter_24508", 0x24508, [p45_24508("s0", 0), p45_24508("s1", 1)],
         allow_calls=(0x339AC,), calls=(ANIM_BEGIN, VOICE), eax_mask=0,
         mutants=("@mutant", "@side", "@stream")),
    # 0x24454: t1's record +0x24 = 0x80000000 passes the 0x7FFFFFFF mask (a whole-dword test
    # refuses); t6's release pset is 0x1014EC + 0x1234 * 0x20; t5/t7 the state-1 guard.
    Spec("fighter_24454", 0x24454, [
        Case("t0", {"eax": E3_SLOT, "edx": E3_REC, "ecx": 0},
             {E3_SLOT + 0x58: b"\x02", E3_SLOT + 0x7A: b"\x05", E3_REC + 0x24: le32(0),
              E3_REC + 0x56: b"\x23\x01"}),
        Case("t1", {"eax": E3_SLOT, "edx": E3_REC, "ecx": 0},
             {E3_SLOT + 0x58: b"\x00", E3_SLOT + 0x7A: b"\x05", E3_REC + 0x18: le32(0x18181818),
              E3_REC + 0x1C: le32(0x1C1C1C1C), E3_REC + 0x24: le32(0x80000000),
              E3_REC + 0x28: b"\x00\x40", E3_REC + 0x30: le32(0xFFFD8000), E3_REC + 0x56: b"\x23\x01",
              0xA85DC + 20: le32(0x000A85DC), E3_OUT + 0x36: b"\x36\x36", E3_OUT + 0x56: b"\x07\x00",
              0x104740: le32(0x47474747)}, {0x2AE14: E3_OUT}),
        Case("t2", {"eax": E3_SLOT, "edx": E3_REC, "ecx": 0},
             {E3_SLOT + 0x58: b"\x00", E3_SLOT + 0x7A: b"\x05", E3_REC + 0x24: le32(1),
              E3_REC + 0x56: b"\x23\x01", 0x104740: le32(0x47474747)}),
        Case("t3", {"eax": E3_SLOT, "edx": E3_REC, "ecx": 0},
             {E3_SLOT + 0x58: b"\x00", E3_SLOT + 0x7A: b"\x05", E3_REC + 0x18: le32(0x18181818),
              E3_REC + 0x1C: le32(0x1C1C1C1C), E3_REC + 0x24: le32(0), E3_REC + 0x28: b"\x00\x00",
              E3_REC + 0x30: le32(0x00030000), E3_REC + 0x56: b"\x23\x01",
              0xA85DC + 20: le32(0x000A85DC), E3_OUT + 0x36: b"\x36\x36", E3_OUT + 0x56: b"\x07\x00",
              0x104740: le32(0x47474747)}, {0x2AE14: E3_OUT}),
        Case("t4", {"eax": E3_SLOT, "edx": E3_REC, "ecx": 0},
             {E3_SLOT + 0x58: b"\x01", E3_REC + 0x51: b"\x00", E3_REC + 0x24: le32(0),
              0x104740: le32(E3_OUT), E3_OUT + 0x1C: le32(0x00002FFF), E3_OUT + 0x56: b"\x34\x12"}),
        Case("t5", {"eax": E3_SLOT, "edx": E3_REC, "ecx": 0},
             {E3_SLOT + 0x58: b"\x01", E3_SLOT + 0x52: b"\x52\x53", E3_REC + 0x51: b"\x00",
              0x104740: le32(E3_OUT), E3_OUT + 0x1C: le32(0x00003000), E3_OUT + 0x56: b"\x34\x12",
              0x1014EC: le32(P45_PSET), 0x1077AC: le32(0)}),
        Case("t6", {"eax": E3_SLOT, "edx": E3_REC, "ecx": 0},
             {E3_SLOT + 0x58: b"\x01", E3_SLOT + 0x52: b"\x52\x53", E3_REC + 0x51: b"\x00",
              0x104740: le32(E3_OUT), E3_OUT + 0x1C: le32(0x00003000), E3_OUT + 0x56: b"\x34\x12",
              0x1014EC: le32(P45_PSET), **P45_PTRS, **P45_CHARS, 0xE5100: le32(0xE5100)}),
        Case("t7", {"eax": E3_SLOT, "edx": E3_REC, "ecx": 0},
             {E3_SLOT + 0x58: b"\x01", E3_SLOT + 0x52: b"\x52\x53", E3_REC + 0x51: b"\x01",
              0x104740: le32(E3_REC2), E3_REC2 + 0x1C: le32(0x00003001), E3_REC2 + 0x56: b"\x34\x12",
              0x1014EC: le32(P45_PSET), **P45_PTRS, **P45_CHARS}),
        Case("t8", {"eax": E3_SLOT, "edx": E3_REC, "ecx": 0},
             {E3_SLOT + 0x58: b"\x01", E3_SLOT + 0x52: b"\x52\x53", E3_REC + 0x51: b"\x02",
              0x104740: le32(E3_REC2), E3_REC2 + 0x1C: le32(0x00003001), E3_REC2 + 0x56: b"\x34\x12",
              0x1014EC: le32(P45_PSET), **P45_PTRS, 0x1077B4: le32(P45_SLOT3), **P45_CHARS}),
    ], calls=(SPAWN, RELEASE, ANIM_BEGIN), eax_mask=0,

Spec("fighter_24454", 0x24454, [
        Case("t0", {"eax": E3_SLOT, "edx": E3_REC, "ecx": 0},
             {E3_SLOT + 0x58: b"\x02", E3_SLOT + 0x7A: b"\x05", E3_REC + 0x24: le32(0),
              E3_REC + 0x56: b"\x23\x01"}),
        Case("t1", {"eax": E3_SLOT, "edx": E3_REC, "ecx": 0},
             {E3_SLOT + 0x58: b"\x00", E3_SLOT + 0x7A: b"\x05", E3_REC + 0x18: le32(0x18181818),
              E3_REC + 0x1C: le32(0x1C1C1C1C), E3_REC + 0x24: le32(0x80000000),
              E3_REC + 0x28: b"\x00\x40", E3_REC + 0x30: le32(0xFFFD8000), E3_REC + 0x56: b"\x23\x01",
              0xA85DC + 20: le32(0x000A85DC), E3_OUT + 0x36: b"\x36\x36", E3_OUT + 0x56: b"\x07\x00",
              0x104740: le32(0x47474747)}, {0x2AE14: E3_OUT}),
        Case("t2", {"eax": E3_SLOT, "edx": E3_REC, "ecx": 0},
             {E3_SLOT + 0x58: b"\x00", E3_SLOT + 0x7A: b"\x05", E3_REC + 0x24: le32(1),
              E3_REC + 0x56: b"\x23\x01", 0x104740: le32(0x47474747)}),
        Case("t3", {"eax": E3_SLOT, "edx": E3_REC, "ecx": 0},
             {E3_SLOT + 0x58: b"\x00", E3_SLOT + 0x7A: b"\x05", E3_REC + 0x18: le32(0x18181818),
              E3_REC + 0x1C: le32(0x1C1C1C1C), E3_REC + 0x24: le32(0), E3_REC + 0x28: b"\x00\x00",
              E3_REC + 0x30: le32(0x00030000), E3_REC + 0x56: b"\x23\x01",
              0xA85DC + 20: le32(0x000A85DC), E3_OUT + 0x36: b"\x36\x36", E3_OUT + 0x56: b"\x07\x00",
              0x104740: le32(0x47474747)}, {0x2AE14: E3_OUT}),
        Case("t4", {"eax": E3_SLOT, "edx": E3_REC, "ecx": 0},
             {E3_SLOT + 0x58: b"\x01", E3_REC + 0x51: b"\x00", E3_REC + 0x24: le32(0),
              0x104740: le32(E3_OUT), E3_OUT + 0x1C: le32(0x00002FFF), E3_OUT + 0x56: b"\x34\x12"}),
        Case("t5", {"eax": E3_SLOT, "edx": E3_REC, "ecx": 0},
             {E3_SLOT + 0x58: b"\x01", E3_SLOT + 0x52: b"\x52\x53", E3_REC + 0x51: b"\x00",
              0x104740: le32(E3_OUT), E3_OUT + 0x1C: le32(0x00003000), E3_OUT + 0x56: b"\x34\x12",
              0x1014EC: le32(P45_PSET), 0x1077AC: le32(0)}),
        Case("t6", {"eax": E3_SLOT, "edx": E3_REC, "ecx": 0},
             {E3_SLOT + 0x58: b"\x01", E3_SLOT + 0x52: b"\x52\x53", E3_REC + 0x51: b"\x00",
              0x104740: le32(E3_OUT), E3_OUT + 0x1C: le32(0x00003000), E3_OUT + 0x56: b"\x34\x12",
              0x1014EC: le32(P45_PSET), **P45_PTRS, **P45_CHARS, 0xE5100: le32(0xE5100)}),
        Case("t7", {"eax": E3_SLOT, "edx": E3_REC, "ecx": 0},
             {E3_SLOT + 0x58: b"\x01", E3_SLOT + 0x52: b"\x52\x53", E3_REC + 0x51: b"\x01",
              0x104740: le32(E3_REC2), E3_REC2 + 0x1C: le32(0x00003001), E3_REC2 + 0x56: b"\x34\x12",
              0x1014EC: le32(P45_PSET), **P45_PTRS, **P45_CHARS}),
        Case("t8", {"eax": E3_SLOT, "edx": E3_REC, "ecx": 0},
             {E3_SLOT + 0x58: b"\x01", E3_SLOT + 0x52: b"\x52\x53", E3_REC + 0x51: b"\x02",
              0x104740: le32(E3_REC2), E3_REC2 + 0x1C: le32(0x00003001), E3_REC2 + 0x56: b"\x34\x12",
              0x1014EC: le32(P45_PSET), **P45_PTRS, 0x1077B4: le32(P45_SLOT3), **P45_CHARS}),
    ], calls=(SPAWN, RELEASE, ANIM_BEGIN), eax_mask=0,
'''
sub("tools/diff_verify.py", "]\n\n\nSPECS = [\n    Spec(\"rng_next\", 0x5D7DC, [\n",
    "]\n\n\nP45_SPECS += [" + SPECS7 + "]\n\n\nSPECS = [\n    Spec(\"rng_next\", 0x5D7DC, [\n")
print("t7_spec applied")
PY
python3 tools/diff_verify.py --image /tmp/pr_p45_img.bin --function fighter_24508 | grep -E '^\| fighter|unknown binding|^diff-verify'
```

Expected: `t7_spec applied` and `fighter_24508`'s row MISMATCH with unknown bindings.

- [ ] **Step 2: the unit check (fails to build).**

```bash
python3 - <<'PY'
import pathlib


def sub(path, old, new):
    p = pathlib.Path(path)
    s = p.read_text()
    n = s.count(old)
    assert n == 1, "%s: %d occurrences of %r" % (path, n, old[:70])
    p.write_text(s.replace(old, new))


sub("port/tests/test_fight.c", "int test_p45_p5_spawns(void)    { return u6b_run(p45_check_p5_spawns); }\n",
    "int test_p45_p5_spawns(void)    { return u6b_run(p45_check_p5_spawns); }\n" + r'''
/* §P5.5: 0x24508 stores the +0x10 handler 0x24454 in the other slot; the
 * adapter resolves 0x24454 and passes the slot's record. */
static void p45_check_handler(void)
{
    p45_anim_fn f;
    f = (p45_anim_fn)(void *)fn_resolve(0x24508u);
    CHECK(f != NULL, "0x24508 is registered");
    CHECK(fn_resolve(0x24454u) == (void (*)(void))fighter_24454_case10, "0x24454 is the case-10 adapter");
    if (f != NULL) {
        z_fseed();
        c4r_pool();
        DSB(Z_R0 + 0x51u) = 0u;
        f(Z_R0, 0u);
        CHECK_EQ_INT((int)DSD(Z_S1 + 0x10u), 0x00024454);
        CHECK_EQ_INT((int)DSB(Z_S1 + 0x52u), 0x10);
        CHECK_EQ_INT((int)DSB(Z_S1 + 0x53u), 0x0A);
        CHECK_EQ_INT((int)DSB(Z_S1 + 0x58u), 0);
    }
    /* the case-10 adapter supplies the slot's record: with slot 1's +0x58 = 0
     * and its record's +0x24 = 0, 0x24454 spawns from the slot's char with
     * a2 = the record's +0x18 (a port that passed 0 would take a2 = 0). */
    {
        static u32 before[0x80];
        u32 n, child, x;
        void (*h)(u32, u32) = (void (*)(u32, u32))(void *)fn_resolve(0x24454u);
        CHECK(h == (void (*)(u32, u32))fighter_24454_case10, "the adapter is the handler");
        if (h != NULL) {
            z_fseed();
            c4r_pool();
            DSD(Z_S1) = Z_R1;
            DSD(Z_R1 + 0x18u) = 0x18181818u;
            DSD(Z_R1 + 0x24u) = 0u;
            DSB(Z_S1 + 0x58u) = 0u;
            DSB(Z_S1 + 0x7Au) = 2u;
            n = u6b_list(before, 0x80u);
            h(Z_S1, 0u);
            child = p45_new_child(before, n, &x);
            CHECK_EQ_INT((int)x, 1);
            if (x == 1u) CHECK_EQ_INT((int)DSD(child + 0x18u), 0x18181818);
        }
    }
}
''')
sub("port/tests/test.h", "    X(test_p45_p5_spawns) \\\n", "    X(test_p45_p5_spawns) \\\n    X(test_p45_handler) \\\n")
print("t7_test applied")
PY
cmake --build build 2>&1 | grep -E ' error' | sed 's#.*/port/#port/#'
```

Expected: one undeclared-identifier error for `p45_check_handler`.

- [ ] **Step 3: the port, the seams, the registrations, the bindings and the mutants.** Append the functions to `fighter.c`, their prototypes to `fighter.h`, the `release_record` seam (`PR_SEAM(0x2AD40u, rec, pset)` as its first statement in `actors.c`), the `anim_code_24508` wrapper and the two registrations (`0x24508u -> anim_code_24508`, `0x24454u -> fighter_24454_case10`), and the bindings and rows to `diff_runner.c` (after `fighter_48a20@a2`):

```
/* 0x24508 — record §P5.5. The context 0x339AC(rec); the other slot (ctx[3])
 * 0x10/0x0A/0 with its +0x10 handler 0x24454 (0x3531C case 10) and +0x58 = 0;
 * the other record on its character's 0xA85F8 stream at 1.0 (0x2BC30); the
 * voice 0xEB. */
void fighter_24508(u32 rec)
{
    u32 ctx[6];
    hit_anim_ctx(ctx, rec);                                 /* 0x2450C..0x24510 0x339AC */
    DSB(ctx[3] + 0x52u) = 0x10u;                            /* 0x24515/0x24519 */
    DSB(ctx[3] + 0x53u) = 0x0Au;                            /* 0x2451D/0x24521 */
    DSD(ctx[3] + 0x10u) = 0x00024454u;                      /* 0x24525/0x24529 */
    DSB(ctx[3] + 0x58u) = 0u;                               /* 0x24530/0x24534 */
    actors_anim_begin(ctx[5], DSD(P5_STREAMS_A85F8 + (u32)DSB(ctx[3] + 0x7Au) * 4u),
                      0x3F800000u);                         /* 0x24538..0x24554 0x2BC30 */
    sound_voice(0xEBu);                                     /* 0x24559/0x2455E */
}

/* 0x24454 — record §P5.5. The other slot's +0x10 handler 0x24508 stores
 * (0x3531C case 10: EAX = the slot, EDX = the slot's record, EBX = side; the
 * function reads EAX and EDX). By the slot's +0x58: above 1 nothing; 0 with
 * the record's +0x24 (mask 0x7FFFFFFF) non-zero nothing, else a child from the
 * slot's character's 0xA85DC descriptor with a2 = the record's +0x18, a3 = its
 * +0x30 >> 16, a4 = its +0x1C, a5 = 0x4000 (bit 14 of its +0x28) or 0, the
 * child's word +0x36 = 0x40, 0x104740 = the child and the slot's +0x58 = 1;
 * 1 with the child's +0x1C below 0x3000 nothing, else the child released
 * (0x2AD40, its pset 0x1014EC + pool index * 0x20), the other side's slot
 * (rec+0x51 ^ 1) when non-zero: its record on 0xE5100 at 3.0 and the slot
 * +0x53 = 3, +0x52 = 9. */
void fighter_24454(u32 slot, u32 rec)
{
    u8 st = DSB(slot + 0x58u);                              /* 0x2445A */
    u32 child;
    if (st > 1u) return;                                    /* 0x2445D..0x24467 */
    if (st == 0u) {
        if ((DSD(rec + 0x24u) & 0x7FFFFFFFu) != 0u) return; /* 0x24468/0x2446F */
        child = actor_spawn((const u32 *)(mem + DSD(P5_DESC_A85DC + (u32)DSB(slot + 0x7Au) * 4u)),
                            DSD(rec + 0x18u), (u32)((s32)DSD(rec + 0x30u) >> 16),
                            DSD(rec + 0x1Cu),
                            (DSW(rec + 0x28u) & 0x4000u) != 0u ? 0x4000u : 0u);   /* 0x24475..0x2449C */
        DSW(child + 0x36u) = 0x0040u;                       /* 0x244A1 */
        DSD(P5_104740) = child;                             /* 0x244A7 */
        DSB(slot + 0x58u) = (u8)(st + 1u);                  /* 0x244AC */
        return;
    }
    child = DSD(P5_104740);                                 /* 0x244B2 */
    if ((s32)DSD(child + 0x1Cu) < 0x3000) return;           /* 0x244B7..0x244BE */
    actor_release(child, DSD(DS_001014EC) + (u32)DSW(child + 0x56u) * 0x20u);   /* 0x244C0..0x244D1 0x2AD40 */
    {
        u32 other = (u32)DSB(rec + 0x51u) ^ 1u;             /* 0x244D6..0x244DB */
        u32 oslot = DSD(DS_001077A8 + other * 4u);          /* 0x244E0 */
        if (oslot == 0u) return;                            /* 0x244E7/0x244E9 */
        actors_anim_begin(DSD(oslot), P5_STREAM_24454, 0x40400000u);   /* 0x244EB..0x244F7 */
    }
    DSB(slot + 0x53u) = 3u;                                 /* 0x244FC */
    DSB(slot + 0x52u) = 9u;                                 /* 0x24500 */
}


static void b_24508(const u32 *r, u32 *eax)      { fighter_24508(r[R_EAX]); *eax = 0u; }
static void b_24454(const u32 *r, u32 *eax)      { fighter_24454(r[R_EAX], r[R_EDX]); *eax = 0u; }
static void m_24508(const u32 *r, u32 *eax)
{
    u32 ctx[6];
    hit_anim_ctx(ctx, r[R_EAX]);
    DSB(ctx[3] + 0x52u) = 0x10u; DSB(ctx[3] + 0x53u) = 0x0Au;
    DSD(ctx[3] + 0x10u) = 0x00024454u; DSB(ctx[3] + 0x58u) = 0u;
    actors_anim_begin(ctx[5], DSD(0x000A85F8u + (u32)DSB(ctx[3] + 0x7Au) * 4u), 0x3F800000u);
    sound_voice(0xEAu);
    *eax = 0u;
}
static void m_24508_side(const u32 *r, u32 *eax)
{
    u32 ctx[6];
    hit_anim_ctx(ctx, r[R_EAX]);
    DSB(ctx[2] + 0x52u) = 0x10u; DSB(ctx[2] + 0x53u) = 0x0Au;
    DSD(ctx[2] + 0x10u) = 0x00024454u; DSB(ctx[2] + 0x58u) = 0u;
    actors_anim_begin(ctx[5], DSD(0x000A85F8u + (u32)DSB(ctx[3] + 0x7Au) * 4u), 0x3F800000u);
    sound_voice(0xEBu);
    *eax = 0u;
}
static void m_24508_stream(const u32 *r, u32 *eax)
{
    u32 ctx[6];
    hit_anim_ctx(ctx, r[R_EAX]);
    DSB(ctx[3] + 0x52u) = 0x10u; DSB(ctx[3] + 0x53u) = 0x0Au;
    DSD(ctx[3] + 0x10u) = 0x00024454u; DSB(ctx[3] + 0x58u) = 0u;
    actors_anim_begin(ctx[5], DSD(0x000A85F8u + (u32)DSB(ctx[2] + 0x7Au) * 4u), 0x3F800000u);
    sound_voice(0xEBu);
    *eax = 0u;
}


    { "fighter_24508",          b_24508,         0x00000000u },
    { "fighter_24508@mutant",   m_24508,         0x00000000u },
    { "fighter_24508@side",     m_24508_side,    0x00000000u },
    { "fighter_24508@stream",   m_24508_stream,  0x00000000u },
    { "fighter_24454",          b_24454,         0x00000000u },
    { "fighter_24454@mutant",   m_24454,         0x00000000u },
    { "fighter_24454@guard",    m_24454_guard,   0x00000000u },
    { "fighter_24454@st1",      m_24454_st1,     0x00000000u },
    { "fighter_24454@pset",     m_24454_pset,    0x00000000u },
    { "fighter_24454@release",  m_24454_release, 0x00000000u },
    { "fighter_24454@side",     m_24454_side,    0x00000000u },
    { "fighter_24454@desc",     m_24454_desc,    0x00000000u },
    { "fighter_24454@a5",       m_24454_a5,      0x00000000u },

```

Then the same build/test/verify commands print `all checks passed` and the measured rows:

| fighter_24508 | 0x24508 | 2 | 1/1 | VERIFIED | 2BC30 stub unverified, 2C3FC stub unverified, 339AC allow VERIFIED |
| fighter_24454 | 0x24454 | 9 | 9/9 | VERIFIED | 2AD40 stub unverified, 2AE14 stub unverified, 2BC30 stub unverified |
| fighter_24508@mutant | 0x24508 | 2 | 1/1 | MISMATCH | 2BC30 stub unverified, 2C3FC stub unverified, 339AC allow VERIFIED |
| fighter_24508@side | 0x24508 | 2 | 1/1 | MISMATCH | 2BC30 stub unverified, 2C3FC stub unverified, 339AC allow VERIFIED |
| fighter_24508@stream | 0x24508 | 2 | 1/1 | MISMATCH | 2BC30 stub unverified, 2C3FC stub unverified, 339AC allow VERIFIED |
| fighter_24454@mutant | 0x24454 | 9 | 9/9 | MISMATCH | 2AD40 stub unverified, 2AE14 stub unverified, 2BC30 stub unverified |
| fighter_24454@guard | 0x24454 | 9 | 9/9 | MISMATCH | 2AD40 stub unverified, 2AE14 stub unverified, 2BC30 stub unverified |
| fighter_24454@st1 | 0x24454 | 9 | 9/9 | MISMATCH | 2AD40 stub unverified, 2AE14 stub unverified, 2BC30 stub unverified |
| fighter_24454@pset | 0x24454 | 9 | 9/9 | MISMATCH | 2AD40 stub unverified, 2AE14 stub unverified, 2BC30 stub unverified |
| fighter_24454@release | 0x24454 | 9 | 9/9 | MISMATCH | 2AD40 stub unverified, 2AE14 stub unverified, 2BC30 stub unverified |
| fighter_24454@side | 0x24454 | 9 | 9/9 | MISMATCH | 2AD40 stub unverified, 2AE14 stub unverified, 2BC30 stub unverified |
| fighter_24454@desc | 0x24454 | 9 | 9/9 | MISMATCH | 2AD40 stub unverified, 2AE14 stub unverified, 2BC30 stub unverified |
| fighter_24454@a5 | 0x24454 | 9 | 9/9 | MISMATCH | 2AD40 stub unverified, 2AE14 stub unverified, 2BC30 stub unverified |


- [ ] **Step 4: the E2 table.** Expected `entry-triage: targets 257 unported, 238 ported; supplement 131 (8 unported, 0 stale); supplement 131 (8 unported, 0 stale); untrusted entries 30` (`0x24454` is a supplement row).
- [ ] **Step 5: the task gate.** Expected `diff-verify: 111/111 functions VERIFIED; 252/252 mutants detected; 1 named gaps; 13/94 rows with callees closed (17 have none). ...`.
- [ ] **Step 6: the mutation proofs.** The `0x24508` ctx and `0x24454` adapter mutations:

```
port/src/game/fighter.c: 1 FAIL, first: ['test_fight.c:47090: 0 != 148564']
port/src/game/fighter.c: 1 FAIL, first: ['test_fight.c:47114: 0 != 1']
all checks passed
```

- [ ] **Step 7: commit.** Message `fighter: port the +0x10 handler 0x24508/0x24454, seam 0x2AD40; E2 table regenerated (track P batches 4+5, task 7)`.

---

### Task 8: the docs and the full gate

**Files:** `docs/PROGRESS.md`, `docs/superpowers/plans/2026-10-03-reverse-p4-p5-derivations.md` (its results section), `AGENTS.md` (the `0x29C08`/`0x2AD40` seam note and the counters), `README.md` only if `port_progress.py` moved (it does not), the Makefile's provenance where a comment still says "re-measure".

- [ ] **Step 1: the full gate on the final state.**

```bash
T=p45; make verify SMK_DUMP=/tmp/pr_${T}_smk TITLE_DUMP=/tmp/pr_${T}_title ATTRACT_DUMP=/tmp/pr_${T}_att \
  FRONTEND_DUMP=/tmp/pr_${T}_fe TITLE_PIN_DIR=/tmp/pr_${T}_pin AUDIO_WAV=/tmp/pr_${T}.wav K11_DUMP=/tmp/pr_${T}_k11 \
  GP_DUMP=/tmp/pr_${T}_gp DIFF_IMAGE=/tmp/pr_${T}_diffimg DIFF_TABLE=/tmp/pr_${T}_diff.md E2_IMAGE=/tmp/pr_${T}_e2.bin \
  > /tmp/pr_p45_final.log 2>&1; echo "EXIT=$?"
grep -E '^(oracle C-vs-Python|capture oracle|smk_compare|title_compare|attract_compare|== demo-fight)' /tmp/pr_p45_final.log \
  | diff - .superpowers/sdd/2026-09-29-k7-k12/scratch/oracle-lines-base.txt && echo ORACLES-EQUAL
make audio-render AUDIO_WAV=/tmp/pr_p45.wav >/dev/null 2>&1
cmp /tmp/pr_p45.wav .superpowers/sdd/2026-09-29-k7-k12/scratch/before-t2.wav && echo WAV-SAME
python3 tools/port_progress.py
grep -E '^diff-verify:|^entry-triage: (targets|voice)' /tmp/pr_p45_final.log
```

Expected (measured on the planner's prototype): `EXIT=0`, `ORACLES-EQUAL`, `WAV-SAME`, `771 1203 64`, `731 731 100`, and

```
diff-verify: 111/111 functions VERIFIED; 252/252 mutants detected; 1 named gaps; 13/94 rows with callees closed (17 have none). Claim: equivalence on the exercised blocks and inputs only, each function with its callees stubbed or run as stated.
entry-triage: targets 257 unported, 238 ported; supplement 131 (8 unported, 0 stale); untrusted entries 30
entry-triage: voice sites outside Ghidra 134: 13 in unported code, 102 in ported code, 19 nowhere
```

- [ ] **Step 2: the docs.** Append to `docs/PROGRESS.md` a paragraph (what is ported, the counters, the gp re-pins, the named gaps); write the record's §P4.9 results table with the measured values; add the `0x29C08`/`0x2AD40` seams to AGENTS.md's callee-row sentence; note the `title_pin` unittest failure (pre-existing, outside `make verify`, a closeout item).
- [ ] **Step 3: commit** the docs.

---

## Self-review

- **Spec coverage:** every member of the roadmap's P4/P5 (33) has a task, a spec, mutants and a unit check or a named limit; the two seams, the case-10 adapter and the gp re-pins are covered.
- **Placeholder scan:** the task code is the prototype's, measured; no `TODO` beyond the named limits in the record.
- **Type consistency:** the wrappers call `fighter_*` with one argument, the registrations match the wrapper names, the `E.Call` argument orders match the C signatures, and the binding rows' masks are 0.
